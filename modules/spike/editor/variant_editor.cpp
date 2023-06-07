/**
 * variant_editor.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "variant_editor.h"
#include "core/variant/variant_internal.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "spike_define.h"

HashMap<String, VariantEditor *> VariantEditor::editors;

void VariantEditor::register_editor(const String &p_name, VariantEditor *p_editor) {
	if (!editors.has(p_name)) {
		editors[p_name] = p_editor;
	}
}
void VariantEditor::unregister_editor(const String &p_name) {
	if (editors.has(p_name)) {
		editors.erase(p_name);
	}
}
VariantEditor *VariantEditor::get_editor(const String &p_name) {
	if (editors.has(p_name)) {
		return editors[p_name];
	}
	return nullptr;
}

void VariantEditor::_bind_methods() {
}

EditorProperty *VariantEditor::_get_editor_for_property(const Variant::Type p_type) {
	const String h_vector2_setting = "interface/inspector/horizontal_vector2_editing";
	Variant horizontal_vector2_editing;
	if (p_type == Variant::VECTOR2) {
		if (EditorSettings::get_singleton()->has_setting(h_vector2_setting)) {
			horizontal_vector2_editing = EditorSettings::get_singleton()->get(h_vector2_setting);
			EditorSettings::get_singleton()->set(h_vector2_setting, true);
		}
	}
	EditorProperty *editor = EditorInspectorDefaultPlugin::get_editor_for_property(dummy_object, p_type, EDIT_PROPERTY, PropertyHint::PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);
	if (horizontal_vector2_editing.get_type() != Variant::NIL) {
		EditorSettings::get_singleton()->set(h_vector2_setting, horizontal_vector2_editing);
	}
	return editor;
}

void VariantEditor::_pop_editor(const Control *p_control) {
	reset_size();
	Size2 control_size = p_control->get_size() * get_viewport()->get_canvas_transform().get_scale();
	Point2 control_screen_pointion = p_control->get_screen_position();
	Rect2i usable_rect = get_usable_parent_rect();
	Rect2i cp_rect(Point2i(), get_size());
	for (int i = 0; i < 4; i++) {
		if (i > 1) {
			cp_rect.position.y = control_screen_pointion.y - cp_rect.size.y;
		} else {
			cp_rect.position.y = control_screen_pointion.y + control_size.height;
		}

		if (i & 1) {
			cp_rect.position.x = control_screen_pointion.x;
		} else {
			cp_rect.position.x = control_screen_pointion.x - MAX(0, (cp_rect.size.x - control_size.x));
		}

		if (usable_rect.encloses(cp_rect)) {
			break;
		}
	}
	set_position(cp_rect.position);
	popup();
}

void VariantEditor::pop_editor(const Control *p_control, const Variant::Type p_type, const Variant &p_variant, const String &p_variant_name) {
	control = p_control;
	dummy_object->editor_set_section_unfold(EDIT_PROPERTY, true);
	if (p_variant.get_type() == Variant::NIL) {
		Variant new_variant;
		VariantInternal::initialize(&new_variant, p_type);
		dummy_object->edit_property = new_variant;
	} else {
		dummy_object->edit_property = p_variant;
	}
	EditorProperty *use_property = nullptr;
	if (property_cache.has(p_type)) {
		use_property = property_cache[p_type];
		use_property->set_object_and_property(dummy_object, EDIT_PROPERTY);
		use_property->set_label(p_variant_name.is_empty() ? Variant::get_type_name(p_type) : p_variant_name);
		use_property->update_property();
	} else {
		use_property = NODE_ADD_CHILD(container, _get_editor_for_property(p_type));
		use_property->connect("item_rect_changed", callable_mp(this, &VariantEditor::_property_editor_rect_changed));
		use_property->connect("property_changed", callable_mp(this, &VariantEditor::_property_editor_property_changed));
		use_property->set_custom_minimum_size(Size2(300, 0));
		use_property->set_object_and_property(dummy_object, EDIT_PROPERTY);
		use_property->set_label(p_variant_name.is_empty() ? Variant::get_type_name(p_type) : p_variant_name);
		use_property->update_property();
		if (use_property->is_class("EditorPropertyArray") || use_property->is_class("EditorPropertyDictionary")) {
			use_property->set_draw_top_bg(false);
			Button *edit_button = (Button *)use_property->get_child(0);
			edit_button->set_visible(false);
			Node *margin_container = use_property->get_child(2);
			VBoxContainer *vb = (VBoxContainer *)margin_container->get_child(0);
			vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			margin_container->remove_child(vb);
			ScrollContainer *scroll_container = CREATE_NODE(margin_container, ScrollContainer);
			scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			scroll_container->set_custom_minimum_size(Size2(0, 500));
			scroll_container->add_child(vb);
		}
		property_cache[p_type] = use_property;
	}

	if (nullptr != property_editor && property_editor != use_property) {
		property_editor->set_visible(false);
	}
	property_editor = use_property;
	property_editor->set_visible(true);

	_pop_editor(p_control);
}

void VariantEditor::_property_editor_rect_changed() {
}

void VariantEditor::_property_editor_property_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field, const bool &p_changing) {
	dummy_object->set(p_property, p_value);
	if (variant_value_changed.is_valid()) {
		Array args;
		args.append(dummy_object->get(p_property));
		variant_value_changed.callv(args);
	}
}

VariantEditor::VariantEditor() :
		dummy_object(nullptr),
		property_editor(nullptr) {
	set_wrap_controls(true);
	set_process_shortcut_input(true);
	dummy_object = memnew(DummyObject);
	container = CREATE_NODE(this, VBoxContainer);
}