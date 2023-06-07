/**
 * configuration_set_inspector.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_set_inspector.h"
#include "core/variant/callable.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "spike/editor/spike_undo_redo.h"
#include "spike_define.h"

#define MENU_REMOVE 5

const Vector<int> CONFIGURATION_TYPES = {
	Variant::BOOL,
	Variant::INT,
	Variant::FLOAT,
	Variant::STRING,
	Variant::VECTOR2,
	Variant::VECTOR3,
	Variant::COLOR,
	Variant::PACKED_INT32_ARRAY,
	Variant::PACKED_INT64_ARRAY,
	Variant::PACKED_FLOAT32_ARRAY,
	Variant::PACKED_FLOAT64_ARRAY,
	Variant::PACKED_STRING_ARRAY,
	Variant::PACKED_VECTOR2_ARRAY,
	Variant::PACKED_VECTOR3_ARRAY,
	Variant::PACKED_COLOR_ARRAY,
};

class ConfigurationSetEditorProperty : public MarginContainer {
	GDCLASS(ConfigurationSetEditorProperty, MarginContainer);

protected:
	struct ThemeCache {
		Ref<Texture2D> patched;
	} theme_cache;

	bool is_patched = false;

	EditorProperty *editor = nullptr;

	void _update_theme_item_cache() override {
		theme_cache.patched = get_theme_icon(SNAME("Error"), SNAME("EditorIcons"));
	}

	void _editor_property_draw() {
		if (is_patched) {
			Ref<Texture2D> patched_icon = theme_cache.patched;
			Vector2 ofs;
			Vector2 size = editor->get_size();
			ofs.x = int(size.x / 2 - patched_icon->get_width());
			ofs.y = int(size.y / 2 - patched_icon->get_height() / 2);
			editor->draw_texture(patched_icon, ofs);
		}
	}

public:
	EditorProperty *get_editor() { return editor; }

	void update_property() {
		ConfigurationSet *target = (ConfigurationSet *)editor->get_edited_object();
		is_patched = target->is_patch && target->has(editor->get_edited_property());
		editor->update_property();
		editor->queue_redraw();
	}

	ConfigurationSetEditorProperty(Object *p_object, const int &p_index, const String &p_path, const Variant &p_value) {
		editor = EditorInspectorDefaultPlugin::get_editor_for_property(p_object, p_value.get_type(), p_path,
				PropertyHint::PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);
		editor->connect("draw", callable_mp(this, &ConfigurationSetEditorProperty::_editor_property_draw));
		editor->set_object_and_property(p_object, p_path);
		editor->set_label(p_path);
		add_child(editor);
	}
	~ConfigurationSetEditorProperty() {}
};

void ConfigurationSetInspector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("new_modification"));
}

void ConfigurationSetInspector::_name_edit_text_changed(const String &p_text) {
	prompt_label->hide();
	if (resource->has(p_text)) {
		prompt_label->show();
		prompt_label->set_text(vformat(TTR("Configuration already exists: %s"), p_text));
	}
}

void ConfigurationSetInspector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process(is_visible_in_tree());
		} break;

		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			prompt_label->add_theme_color_override("font_color", get_theme_color(SNAME("error_color"), SNAME("Editor")));
			scroll_container->add_theme_style_override("panel", get_theme_stylebox(SNAME("panel"), SNAME("Tree")));
		} break;
	}
}

void ConfigurationSetInspector::_create_new_config() {
	COND_MET_RETURN(nullptr == undo_redo_mgr);
	String config_name = name_edit->get_text();
	if (config_name.is_empty()) {
		prompt_label->show();
		prompt_label->set_text(STTR("Please enter a configuration name"));
		return;
	}

	if (resource->has(config_name)) {
		prompt_label->show();
		prompt_label->set_text(vformat(STTR("Configuration already exists: %s"), config_name));
		return;
	}
	int config_type = type_option->get_selected_id();
	Variant defval;
	Callable::CallError ce;
	Variant::construct(Variant::Type(config_type), defval, nullptr, 0, ce);

	undo_redo_mgr->create_action(vformat(TTR("Add %s"), config_name));
	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
	undo_redo->add_do_method(callable_mp(this, &ConfigurationSetInspector::_do_add_config).bind(config_name, defval));
	undo_redo->add_undo_method(callable_mp(this, &ConfigurationSetInspector::_do_remove_config).bind(config_name));
	undo_redo_mgr->commit_action();
}

void ConfigurationSetInspector::_do_add_config(const String &p_path, const Variant &p_value) {
	resource->add(p_path, p_value);
	_create_property_editor(resource->size(), p_path, p_value);
	_emit_new_modification();
}

void ConfigurationSetInspector::_do_insert_config(const int &p_index, const String &p_path, const Variant &p_value) {
	resource->insert(p_index, p_path, p_value);
	_create_property_editor(p_index, p_path, p_value);
	_emit_new_modification();
}

void ConfigurationSetInspector::_do_remove_config(const String &p_path) {
	resource->remove(p_path);
	if (editor_property_map.has(p_path)) {
		if (property_selected == editor_property_map[p_path]) {
			property_selected = nullptr;
		}
		editor_property_map[p_path]->queue_free();
		editor_property_map.erase(p_path);
	}
	_emit_new_modification();
}

void ConfigurationSetInspector::_property_selected(const String &p_path, int p_focusable) {
	if (nullptr != property_selected) {
		property_selected->get_editor()->deselect();
	}
	property_selected = editor_property_map[p_path];
}

void ConfigurationSetInspector::_property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing) {
	COND_MET_RETURN(nullptr == undo_redo_mgr);
	ConfigurationSetEditorProperty *editor = editor_property_map[p_path];
	Variant old_value = resource->get(p_path);
	undo_redo_mgr->create_action(vformat(TTR("Change %s"), p_path));
	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
	undo_redo->add_do_property(resource.ptr(), p_path, p_value);
	undo_redo->add_do_method(callable_mp(editor, &ConfigurationSetEditorProperty::update_property));
	undo_redo->add_do_method(callable_mp(this, &ConfigurationSetInspector::_emit_new_modification));
	undo_redo->add_undo_property(resource.ptr(), p_path, old_value);
	undo_redo->add_undo_method(callable_mp(editor, &ConfigurationSetEditorProperty::update_property));
	undo_redo->add_undo_method(callable_mp(this, &ConfigurationSetInspector::_emit_new_modification));
	undo_redo_mgr->commit_action();
}

void ConfigurationSetInspector::_property_deleted(const String &p_path) {
	COND_MET_RETURN(nullptr == undo_redo_mgr);
	int old_index = resource->keys_ptr()->find(p_path);
	undo_redo_mgr->create_action(vformat(TTR("Remove %s"), p_path));
	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
	undo_redo->add_do_method(callable_mp(this, &ConfigurationSetInspector::_do_remove_config).bind(p_path));
	undo_redo->add_undo_method(callable_mp(this, &ConfigurationSetInspector::_do_insert_config).bind(old_index, p_path, resource->get(p_path)));
	undo_redo_mgr->commit_action();
}

void ConfigurationSetInspector::_create_property_editor(const int &p_index, const String &p_path, const Variant &p_value) {
	ConfigurationSetEditorProperty *property_editor = memnew(ConfigurationSetEditorProperty(resource.ptr(), p_index, p_path, p_value));
	EditorProperty *editor = property_editor->get_editor();
	editor->connect("selected", callable_mp(this, &ConfigurationSetInspector::_property_selected));
	editor->connect("property_deleted", callable_mp(this, &ConfigurationSetInspector::_property_deleted));
	editor->connect("property_changed", callable_mp(this, &ConfigurationSetInspector::_property_changed));
	editor->set_object_and_property((Object *)resource.ptr(), p_path);
	editor->set_label(p_path);
	if (resource->is_patch && p_index < resource->get_editable_size()) {
		editor->set_deletable(false);
	} else {
		editor->set_deletable(true);
	}
	property_editor->update_property();
	editor_property_map[p_path] = property_editor;
	scroll_content->add_child(property_editor);
	scroll_content->move_child(property_editor, p_index);
}

void ConfigurationSetInspector::edit(const Ref<ConfigurationSet> &p_resource) {
	resource = p_resource;
	property_selected = nullptr;
	while (scroll_content->get_child_count()) {
		memdelete(scroll_content->get_child(0));
	}
	Vector<StringName> keys = resource->get_editable_keys();
	Vector<Variant> values = resource->get_editable_values();
	for (int i = 0; i < keys.size(); i++) {
		_create_property_editor(i, keys.get(i), values.get(i));
	}
	add_container->set_visible(!resource->is_patch);
}

ConfigurationSetInspector::ConfigurationSetInspector() {
	undo_redo_mgr = SpikeUndoRedo::get_undo_redo_manager("Configuration");

	add_container = CREATE_NODE(this, HBoxContainer);
	add_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_container->set_custom_minimum_size(Size2(0, 30) * EDSCALE);
	add_container->add_theme_constant_override("separation", 10);

	HBoxContainer *name_box = CREATE_NODE(add_container, HBoxContainer);
	name_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	CREATE_NODE(name_box, Label(TTR("Name:")));
	name_edit = CREATE_NODE(name_box, LineEdit());
	name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	name_edit->connect("text_changed", callable_mp(this, &ConfigurationSetInspector::_name_edit_text_changed));

	HBoxContainer *type_box = CREATE_NODE(add_container, HBoxContainer);
	type_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	CREATE_NODE(type_box, Label(TTR("Type:")));
	type_option = CREATE_NODE(type_box, OptionButton);
	type_option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	for (int i = 0; i < CONFIGURATION_TYPES.size(); i++) {
		int type_id = CONFIGURATION_TYPES[i];
		String type = i == Variant::OBJECT ? String("Resource") : Variant::get_type_name(Variant::Type(type_id));
		type_option->add_icon_item(EditorNode::get_singleton()->get_gui_base()->get_theme_icon(type, "EditorIcons"), type, type_id);
	}

	add_button = CREATE_NODE(add_container, Button);
	add_button->set_text(TTR("Add"));
	add_button->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
	add_button->connect("pressed", callable_mp(this, &ConfigurationSetInspector::_create_new_config));

	prompt_label = memnew(Label);
	prompt_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	prompt_label->set_theme_type_variation("HeaderSmall");
	prompt_label->add_theme_font_size_override("font_size", 10);
	prompt_label->hide();
	add_child(prompt_label);

	scroll_container = CREATE_NODE(this, ScrollContainer);
	scroll_container->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	scroll_content = CREATE_NODE(scroll_container, VBoxContainer);
	scroll_content->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
}

ConfigurationSetInspector::~ConfigurationSetInspector() {
}