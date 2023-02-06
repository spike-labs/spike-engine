/**
 * material_editor_inspector.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "material_editor_inspector.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/filesystem_dock.h"
#include "scene/gui/separator.h"
#include "spike/material_utils.h"

void SpikeInspectorPluginMaterial::_about_to_popup() {
	PopupMenu *popup = prop->get_popup();
	material_types.clear();
	ClassDB::get_inheriters_from_class("Material", &material_types);
	material_types.erase(PlaceholderMaterial::get_class_static());
	const StringName shader_material_name = ShaderMaterial::get_class_static();
	Shader::Mode shader_mode = material->get_shader_mode();
	for (size_t i = 0; i < material_types.size(); i++) {
		auto &cls_name = material_types[i];
		if (cls_name == material->get_class_name())
			continue;
		if (cls_name != shader_material_name) {
			if (!ClassDB::can_instantiate(cls_name))
				continue;

			Ref<Material> res = Ref<Material>(ClassDB::instantiate(cls_name));
			if (res.is_null() || res->get_shader_mode() != shader_mode)
				continue;
		}

		popup->add_icon_item(prop->get_theme_icon(cls_name, SNAME("EditorIcons")), cls_name, i);
	}
	popup->connect("id_pressed", callable_mp(this, &SpikeInspectorPluginMaterial::_material_type_pressed));
}

void SpikeInspectorPluginMaterial::_material_type_pressed(int id) {
	shader = nullptr;
	if (id >= 0 && id < material_types.size()) {
		current_type = material_types[id];
		if (current_type == ShaderMaterial::get_class_static()) {
			if (quick_open == nullptr) {
				quick_open = memnew(EditorQuickOpen);
				quick_open->connect("quick_open", callable_mp(this, &SpikeInspectorPluginMaterial::_shader_quick_selected).bind(quick_open));
				prop->add_child(quick_open);
			}
			quick_open->popup_dialog("Shader");
			quick_open->set_title(TTR("Shader"));
		} else {
			_open_save_dialog();
		}
	}
}

void SpikeInspectorPluginMaterial::_shader_quick_selected(EditorQuickOpen *quick_open) {
	auto path = quick_open->get_selected();
	Ref<Shader> res = ResourceLoader::load(path);
	ERR_FAIL_COND_MSG(res.is_null(), "Cannot load resource from path '" + path + "'.");
	if (!res->is_class_ptr(Shader::get_class_ptr_static())) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("The selected resource (%s) does not match any type expected for this property (%s)."), res->get_class(), "Shader"));
		return;
	}

	shader = res.ptr();
	if (shader->get_mode() != material->get_shader_mode()) {
		if (warn_dialog == nullptr) {
			warn_dialog = memnew(AcceptDialog);
			warn_dialog->add_cancel_button(TTR("Cancel"));
			warn_dialog->set_ok_button_text(TTR("Continue"));
			warn_dialog->connect("confirmed", callable_mp(this, &SpikeInspectorPluginMaterial::_warning_confirmed));
			prop->add_child(warn_dialog);
		}
		const char *mode_names[] = {
			"MODE_SPATIAL",
			"MODE_CANVAS_ITEM",
			"MODE_PARTICLES",
			"MODE_SKY",
			"MODE_FOG",
		};
		warn_dialog->set_title(TTR("Warnning"));
		warn_dialog->set_text(vformat(TTR("The selected shader mode (%s) does not match for this material (%s)"),
				mode_names[shader->get_mode()], mode_names[material->get_shader_mode()]));
		warn_dialog->set_meta("callback", (int)WarnCB::OPEN_SAVE);
		warn_dialog->popup_centered();
	} else {
		_open_save_dialog();
	}
}

void SpikeInspectorPluginMaterial::_warning_confirmed() {
	WarnCB cb = WarnCB(warn_dialog->get_meta("callback").operator int());
	warn_dialog->set_meta("callback", Variant());
	switch (cb) {
		case WarnCB::OPEN_SAVE:
			_open_save_dialog();
			break;

		default:
			break;
	}
}

void SpikeInspectorPluginMaterial::_open_save_dialog() {
	if (save_dialog == nullptr) {
		save_dialog = memnew(EditorFileDialog);
		prop->add_child(save_dialog);
		save_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		save_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		save_dialog->add_filter("*.tres", "TRES");
		save_dialog->add_filter("*.res", "RES");
		save_dialog->connect("file_selected", callable_mp(this, &SpikeInspectorPluginMaterial::_save_path_selected));
		save_dialog->set_current_path(material->get_path());
		save_dialog->set_title(TTR("Save Resource As..."));
	}
	save_dialog->popup_file_dialog();
}

void SpikeInspectorPluginMaterial::_save_path_selected(const String &p_path) {
	if (shader) {
		Ref<ShaderMaterial> dst_material;
		REF_INSTANTIATE(dst_material);
		dst_material->set_shader(shader);
		MaterialUtils::convert_shader_parameters(material, dst_material);
		ResourceSaver::save(dst_material, p_path);
	} else {
		Ref<Material> dst_material = Ref<Material>(ClassDB::instantiate(current_type));
		MaterialUtils::convert_shader_parameters(material, dst_material);
		ResourceSaver::save(dst_material, p_path);
	}
	EditorFileSystem::get_singleton()->update_file(p_path);
	FileSystemDock::get_singleton()->call_deferred("_select_file", p_path, false);
}

bool SpikeInspectorPluginMaterial::can_handle(Object *p_object) {
	return Object::cast_to<Material>(p_object) != nullptr;
}

void SpikeInspectorPluginMaterial::parse_begin(Object *p_object) {
	material = Object::cast_to<Material>(p_object);
	shader = nullptr;
	quick_open = nullptr;
	save_dialog = nullptr;
	warn_dialog = nullptr;

	MenuButton *menu = memnew(MenuButton);
	this->prop = menu;
	menu->set_text(TTR("Convert Material"));
	menu->connect("about_to_popup", callable_mp(this, &SpikeInspectorPluginMaterial::_about_to_popup), CONNECT_ONE_SHOT);

	add_custom_control(menu);
	add_custom_control(memnew(HSeparator));
}

EditorPropertyPopup::EditorPropertyPopup() {
	menu = memnew(MenuButton);
	add_child(menu);
}
