/**
 * modular_package.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "modular_package.h"
#include "editor_package_system.h"

#ifdef MODULE_MODULAR_SYSTEM_ENABLED
#include "editor/editor_scale.h"
#include "modular_system/editor/modular_editor_plugin.h"
#include "modular_system/editor/modular_export_utils.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/menu_button.h"

// Modular Package

void ModularPackage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_id", "id"), &ModularPackage::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &ModularPackage::get_id);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "package"), "set_id", "get_id");

	ClassDB::bind_method(D_METHOD("set_ver", "ver"), &ModularPackage::set_ver);
	ClassDB::bind_method(D_METHOD("get_ver"), &ModularPackage::get_ver);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "version"), "set_ver", "get_ver");

	ClassDB::bind_method(D_METHOD("set_ext", "ext"), &ModularPackage::set_ext);
	ClassDB::bind_method(D_METHOD("get_ext"), &ModularPackage::get_ext);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ext", PROPERTY_HINT_ENUM, "PCK,ZIP"), "set_ext", "get_ext");
}

void ModularPackage::_select_package_id() {
	auto interface = ModularEditorPlugin::find_interface();
	if (interface == nullptr)
		return;

	auto dlg_node = interface->get_node_or_null(NodePath("ModularPackageSelection"));
	auto selection_dlg = Object::cast_to<ModularPackageSelection>(dlg_node);
	if (selection_dlg) {
		PackedStringArray selected = selection_dlg->get_selected();
		auto his = interface->create_undo_redo(STTR("Select Package For Modular"));
		his.undo_redo->add_do_method(callable_mp(this, &ModularPackage::_select_package_undo_redo).bind(selected[0], selected[1]));
		his.undo_redo->add_undo_method(callable_mp(this, &ModularPackage::_select_package_undo_redo).bind(id, ver));
		interface->commit_undo_redo(his);
	}
}

void ModularPackage::_version_button_pressed(int p_id, Button *p_button) {
	switch (p_id) {
		case 1:
			ver = EditorPackageSystem::get_singleton()->get_package_version(id);
			break;
		case 2:
			EditorPackageSystem::get_singleton()->install_package(id, ver);
			break;
		default:
			break;
	}

	p_button->set_visible(false);
}

void ModularPackage::_select_package_undo_redo(const String &p_id, const String &p_ver) {
	set_id(p_id);
	set_ver(p_ver);

	// Button *version_btn = Object::cast_to<Button>(p_node->get_child(1));
	// if (version_btn) {
	// 	auto install_ver = EditorPackageSystem::get_singleton()->get_package_version(id);
	// 	version_btn->set_visible(!IS_EMPTY(install_ver) && install_ver != ver);
	// 	if (version_btn->is_visible()) {
	// 		version_btn->set_text(vformat(STTR("Use installed version: %s"), install_ver));
	// 	}
	// }
}

void ModularPackage::validation() {
	if (EditorPackageSystem::get_singleton()->get_package_status(id) == EditorPackageSystem::PACKAGE_NONE) {
		EditorPackageSystem::get_singleton()->install_package(id, ver);
	}
}

String ModularPackage::get_module_name() const {
	String package_dir = DIR_PLUGINS + id;
	Ref<ConfigFile> conf = memnew(ConfigFile);
	if (conf->load(NAME_TO_PLUGIN_PATH(id)) == OK) {
		return conf->get_value("plugin", "name", id);
	}
	return id;
}

void ModularPackage::edit_data(Object *p_context) {
	auto interface = ModularEditorPlugin::find_interface();
	if (interface == nullptr)
		return;

	auto selection_dlg = interface->get_node_or_null(NodePath("ModularPackageSelection"));
	if (selection_dlg == nullptr) {
		selection_dlg = memnew(ModularPackageSelection);
		interface->add_child(selection_dlg);
		selection_dlg->set_name("ModularPackageSelection");
		selection_dlg->connect("confirmed", callable_mp(this, &ModularPackage::_select_package_id));
	}
	Object::cast_to<ModularPackageSelection>(selection_dlg)->popup_selection(id);
}

String ModularPackage::export_data() {
	String package_dir = DIR_PLUGINS + id;
	if (FileAccess::exists(PATH_JOIN(package_dir, "plugin.cfg"))) {
		Vector<Ref<Resource>> res_list;
		Vector<String> file_list;
		ModularExportUtils::collect_resources(EditorFileSystem::get_singleton()->get_filesystem_path(package_dir), res_list, file_list);
		return ModularExportUtils::export_specific_resources(nullptr, res_list, file_list, false, ext);
	}
	return String();
}

Dictionary ModularPackage::get_module_info() {
	return Dictionary();
}

void ModularPackage::inspect_modular(Node *p_parent) {
	if (!IS_EMPTY(id)) {
		MenuButton *version_btn = Object::cast_to<MenuButton>(p_parent->get_node_or_null(NodePath("Version")));
		auto install_ver = EditorPackageSystem::get_singleton()->get_package_version(id);
		if (ver != install_ver) {
			if (version_btn == nullptr) {
				MenuButton *version_btn = memnew(MenuButton);
				p_parent->add_child(version_btn);
				version_btn->set_name("Version");
				version_btn->set_text(STTR("Version Conflict"));
				version_btn->get_popup()->add_item(vformat(STTR("Resolve: Use installed %s"), install_ver), 1);
				version_btn->get_popup()->add_item(vformat(STTR("Resolve: Update to %s"), ver), 2);
				version_btn->get_popup()->connect("id_pressed", callable_mp(this, &ModularPackage::_version_button_pressed).bind(version_btn));
				connect("name_changed", callable_mp(this, &ModularPackage::inspect_modular).bind(p_parent));
			}
		} else if (version_btn) {
			version_btn->queue_free();
		}
	}
}

void ModularPackage::set_id(String p_id) {
	id = p_id;

	String module_name = get_module_name();
	emit_signal("name_changed", module_name.is_empty() ? STTR("[Unnamed]") : module_name);
}

#endif

// ModularPackageSelection

void ModularPackageSelection::_package_item_selected(int p_index) {
	selected_package = itemlist->get_item_metadata(p_index);
}

ModularPackageSelection::ModularPackageSelection() {
	ScrollContainer *scroll = memnew(ScrollContainer);
	add_child(scroll);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	itemlist = memnew(ItemList);
	scroll->add_child(itemlist);
	itemlist->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	itemlist->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	itemlist->connect("item_selected", callable_mp(this, &ModularPackageSelection::_package_item_selected));

	set_title(STTR("Select a installed package..."));
}

Variant ModularPackageSelection::get_selected() {
	return selected_package;
}

void ModularPackageSelection::popup_selection(const String &p_selection) {
	itemlist->clear();

	List<String> packages;
	Ref<ConfigFile> manifest;
	REF_INSTANTIATE(manifest);
	if (manifest->load(PATH_MANIFEST) == OK) {
		auto checked_icon = EDITOR_THEME(icon, "GuiRadioChecked", "EditorIcons");
		auto unchecked_icon = EDITOR_THEME(icon, "GuiRadioUnchecked", "EditorIcons");
		manifest->get_section_keys(SECTION_PLUGINS, &packages);
		for (const auto &key : packages) {
			Ref<ConfigFile> plugin_conf;
			REF_INSTANTIATE(plugin_conf);
			if (plugin_conf->load(NAME_TO_PLUGIN_PATH(key)) == OK) {
				String package_name = plugin_conf->get_value("plugin", "name");
				String package_ver = plugin_conf->get_value("plugin", "version");

				int item_index = itemlist->get_item_count();
				itemlist->add_item(vformat("%s (%s)", IS_EMPTY(package_name) ? STTR("<No name>") : package_name, key));
				itemlist->set_item_icon(item_index, p_selection == key ? checked_icon : unchecked_icon);

				PackedStringArray meta;
				meta.push_back(key);
				meta.push_back(package_ver);
				itemlist->set_item_metadata(item_index, meta);
			}
		}
	}

	popup_centered_clamped(Vector2i(500, 700) * EDSCALE);
}
