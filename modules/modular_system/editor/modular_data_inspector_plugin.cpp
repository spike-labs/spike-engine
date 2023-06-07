/**
 * modular_data_inspector_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_data_inspector_plugin.h"
#include "annotation/annotation_plugin.h"
#include "editor/editor_file_system.h"
#include "filesystem_server/filesystem_server.h"
#include "modular_system/editor/modular_data.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

#include "spike/string/sstring.h"

#define META_KEY_SHOW_HIDDEN "_show_hidden_"
#define META_KEY_SHOW_RESOURCES_ONLY "_show_resources_only_"
#define META_KEY_FILTER "_filter_"
#define META_KEY_EXCLUDE "_exclude_"

// ModularResourcesEditorProperty
ModularResourcesSelectDialog *ModularResourcesSelectDialog::singleton = nullptr;

ModularResourcesSelectDialog::ModularResourcesSelectDialog() :
		folders_to_ignore({
				".git", // Git
				// Spike folders
				ANNO_RESOURCE_DIR.trim_suffix("/").get_file(),
				// Godot internal folders
				".godot",
		}),
		files_to_ignore({
				".gitattributes", ".gitignore", // Git
				".uids", MODULAR_REMAP_FILES, // Spike files
				"*.import", // Godot internal files
		}) {
	ERR_FAIL_COND(singleton);
	singleton = this;

	auto hsplit = memnew(HSplitContainer);
	hsplit->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	hsplit->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	add_child(hsplit);

	connect("confirmed", callable_mp(this, &ModularResourcesSelectDialog::_confirm_pressed));
	connect("canceled", callable_mp(this, &ModularResourcesSelectDialog::_cancel_pressed));

	set_title(STTR("Select Resources"));
	set_name("_ModularResourceSelector");
	hide();

	auto lvbox = memnew(VBoxContainer);
	lvbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	lvbox->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	hsplit->add_child(lvbox);

	auto lhbox = memnew(HBoxContainer);
	lhbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	lvbox->add_child(lhbox);

	auto collapse_all = memnew(Button);
	collapse_all->set_text(STTR("Collapse"));
	collapse_all->connect("pressed", callable_mp(this, &ModularResourcesSelectDialog::_collapse_tree));
	lhbox->add_child(collapse_all);

	auto expand_all = memnew(Button);
	expand_all->set_text(STTR("Expand"));
	expand_all->connect("pressed", callable_mp(this, &ModularResourcesSelectDialog::_expand_tree));
	lhbox->add_child(expand_all);

	select_tree = memnew(Tree);
	select_tree->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	select_tree->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	select_tree->connect("item_edited", callable_mp(this, &ModularResourcesSelectDialog::_select_tree_item_edited));
	lvbox->add_child(select_tree);

	auto rvbox = memnew(VBoxContainer);
	rvbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	rvbox->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	hsplit->add_child(rvbox);

	// Hidden
	show_hidden_btn = memnew(CheckButton);
	show_hidden_btn->set_pressed(false);
	show_hidden_btn->set_text(STTR("Show Hidden Folders:"));
	show_hidden_btn->connect("pressed", callable_mp(this, &ModularResourcesSelectDialog::_show_hidden_pressed));
	rvbox->add_child(show_hidden_btn);
	// All Files
	show_recognized_resources_only_btn = memnew(CheckButton);
	show_recognized_resources_only_btn->set_pressed(true);
	show_recognized_resources_only_btn->set_text(STTR("Show Recgnized Resources Only:"));
	show_recognized_resources_only_btn->connect("pressed", callable_mp(this, &ModularResourcesSelectDialog::_show_hidden_pressed));
	rvbox->add_child(show_recognized_resources_only_btn);

	// Filter
	auto filter_label = memnew(Label);
	filter_label->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	filter_label->set_text(STTR("Filter Files:\n"
								"1. You can use \";\" to separate filtering keywords;\n"
								"2. You can put \"|\" at the front of keyword to indecate that this keyword should use expression match;\n"
								"3. Space will be recognized as a character of filtering keyword, you should put it in keyword carefully;\n"
								"For example, \"|pic_*;text_\" will filter these files: \"test_text_a.gd, text_b.gd, pic_a.png\", but can't contain \"invalid_pic.png\"."));
	filter_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD_SMART);
	rvbox->add_child(filter_label);
	filter = memnew(LineEdit);
	filter->set_clear_button_enabled(true);
	filter->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	filter->connect("text_changed", callable_mp(this, &ModularResourcesSelectDialog::_filter_changed));
	rvbox->add_child(filter);

	// Exclude
	auto exclude_label = memnew(Label);
	exclude_label->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	exclude_label->set_text(STTR("Exclude Files:\n(The exclude rule the same as Filter Files, but this will exclude files which is filtered by Filter Files.)"));
	exclude_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD_SMART);
	rvbox->add_child(exclude_label);
	exclude = memnew(LineEdit);
	exclude->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	exclude->set_clear_button_enabled(true);
	exclude->connect("text_changed", callable_mp(this, &ModularResourcesSelectDialog::_exclude_changed));
	rvbox->add_child(exclude);

	// Hint
	Control *place_holder = memnew(Control);
	place_holder->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	place_holder->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	rvbox->add_child(place_holder);
	hidden_files_hint = memnew(RichTextLabel);
	hidden_files_hint->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	hidden_files_hint->set_v_size_flags(Control::SizeFlags::SIZE_SHRINK_END);
	hidden_files_hint->set_fit_content(true);
	hidden_files_hint->set_use_bbcode(true);
	rvbox->add_child(hidden_files_hint);
}

ModularResourcesSelectDialog::~ModularResourcesSelectDialog() {
	singleton = nullptr;
}

// recursively
void ModularResourcesSelectDialog::setup_tree_recursively(TreeItem *p_folder_item, const String &p_dir) {
	static const StringName EditorIcons = "EditorIcons";
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return;
	}
	p_folder_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	p_folder_item->set_icon(0, get_theme_icon(SNAME("Folder"), EditorIcons));
	p_folder_item->set_text(0, p_dir.get_file());
	p_folder_item->set_tooltip_text(0, p_dir);
	p_folder_item->set_metadata(0, p_dir);
	p_folder_item->set_editable(0, true);

	const String anno_folder = ANNO_RESOURCE_DIR.trim_suffix("/").get_file();
	for (auto folder : da->get_directories()) {
		bool need_ignore = false;
		for (const String &E : folders_to_ignore) {
			if (folder.matchn(E)) {
				need_ignore = true;
				break;
			}
		}
		if (need_ignore) {
			continue;
		}

		auto sub_folder_item = p_folder_item->create_child();
		setup_tree_recursively(sub_folder_item, p_dir.path_join(folder));
	}

	auto efsd = EditorFileSystem::get_singleton()->get_filesystem_path(p_dir);
	for (const auto &file : da->get_files()) {
		bool need_ignore = false;
		for (const String &E : files_to_ignore) {
			if (file.matchn(E)) {
				need_ignore = true;
				break;
			}
		}
		if (need_ignore) {
			continue;
		}

		auto file_item = p_folder_item->create_child();

		file_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);

		int file_idx = -1;
		if (efsd) {
			file_idx = efsd->find_file_index(file);
		}

		if (efsd && file_idx >= 0 && efsd->get_file_import_is_valid(file_idx)) {
			auto file_type = efsd->get_file_type(file_idx);
			auto icon = has_theme_icon(file_type, EditorIcons) ? get_theme_icon(file_type, EditorIcons) : get_theme_icon(SNAME("Object"), EditorIcons);
			file_item->set_icon(0, icon);
		} else {
			file_item->set_icon(0, get_theme_icon(SNAME("ImportFail"), SNAME("EditorIcons")));
		}

		auto fp = p_dir.path_join(file);
		file_item->set_text(0, file);
		file_item->set_tooltip_text(0, fp);
		file_item->set_metadata(0, fp);
		file_item->set_editable(0, true);
	}
}

void ModularResourcesSelectDialog::set_item_checked_recursively(TreeItem *p_item, bool p_checked) {
	auto fa = FileSystemServer::get_singleton()->get_os_file_access();
	auto child_count = p_item->get_child_count();
	if (child_count == 0) {
		const String path = p_item->get_metadata(0);
		if (fa->exists(path)) {
			if (p_checked) {
				if (!selected_files.has(path)) {
					selected_files.push_back(path);
				}
			} else {
				selected_files.erase(path);
			}
		}
	} else {
		for (auto i = 0; i < child_count; ++i) {
			auto child = p_item->get_child(i);
			if (child->is_visible()) {
				const String path = child->get_metadata(0);
				if (fa->exists(path)) {
					if (p_checked) {
						if (!selected_files.has(path)) {
							selected_files.push_back(path);
						}
					} else {
						selected_files.erase(path);
					}
				}

				child->set_checked(0, p_checked);
				set_item_checked_recursively(child, p_checked);
			}
		}
	}
}

void ModularResourcesSelectDialog::_show_hidden_pressed() {
	refresh();
}
void ModularResourcesSelectDialog::_show_recognized_resources_only_pressed() {
	refresh();
}
void ModularResourcesSelectDialog::_filter_changed(const String &p_new_text) {
	refresh();
	select_tree->get_root()->set_collapsed_recursive(false);
}
void ModularResourcesSelectDialog::_exclude_changed(const String &p_new_text) {
	refresh();
	select_tree->get_root()->set_collapsed_recursive(false);
}

void ModularResourcesSelectDialog::_collapse_tree() {
	auto root = select_tree->get_root();
	root->set_collapsed_recursive(true);
	root->set_collapsed(false);
}

void ModularResourcesSelectDialog::_expand_tree() {
	auto root = select_tree->get_root();
	root->set_collapsed_recursive(false);
	root->set_collapsed(false);
}

bool ModularResourcesSelectDialog::refresh_recursively(TreeItem *p_item,
		bool p_show_hidden, bool p_show_resources_only, const Vector<String> &p_filters, const Vector<String> &p_excludes) {
	auto fa = FileSystemServer::get_singleton()->get_os_file_access();

	bool has_showing_files = false;
	for (auto i = 0; i < p_item->get_child_count(); ++i) {
		auto item = p_item->get_child(i);
		item->set_checked(0, false);
		const String path = item->get_metadata(0);
		const String file = path.get_file();
		item->set_visible(false);
		if (!p_show_hidden && DirAccess::dir_exists_absolute(file) && file.matchn(".*")) {
			continue;
		}

		if (p_filters.size() > 0) {
			bool matched = false;
			for (const String &filter_text : p_filters) {
				if ((filter_text.begins_with("|") && file.matchn(filter_text.substr(1))) ||
						file.countn(filter_text) > 0) {
					matched = true;
				}
			}
			if (!matched) {
				continue;
			}
		}

		if (p_excludes.size() > 0) {
			bool matched = false;
			for (const String &exclude_text : p_excludes) {
				if ((exclude_text.begins_with("|") && file.matchn(exclude_text.substr(1))) ||
						file.countn(exclude_text) > 0) {
					matched = true;
					break;
				}
			}
			if (matched) {
				continue;
			}
		}

		if (fa->file_exists(path) && p_show_resources_only && !ResourceLoader::exists(path)) {
			continue;
		}

		if (item->get_child_count() == 0 || refresh_recursively(item, p_show_hidden, p_show_resources_only, p_filters, p_excludes)) {
			item->set_visible(true);
			has_showing_files = true;
		}
	}
	return has_showing_files;
}

bool check_items_recursively(TreeItem *p_item, const Vector<String> &p_selected_files, bool p_can_be_checked = true) {
	bool all_checked = true;

	auto fa = FileSystemServer::get_singleton()->get_os_file_access();
	for (auto i = 0; i < p_item->get_child_count(); ++i) {
		TreeItem *item = p_item->get_child(i);
		if (!item->is_visible()) {
			continue;
		}

		const String path = item->get_metadata(0);
		if (p_selected_files.has(path)) {
			item->set_checked(0, true);
		} else if (fa->file_exists(path) || !check_items_recursively(item, p_selected_files)) {
			all_checked = false;
		}
	}

	if (all_checked && p_can_be_checked) {
		p_item->set_checked(0, true);
	}

	return all_checked;
}

int count_showed_selected_files(TreeItem *p_item, const Vector<String> &p_selected_files) {
	int ret = 0;
	for (auto i = 0; i < p_item->get_child_count(); ++i) {
		auto child = p_item->get_child(i);
		if (!child->is_visible()) {
			continue;
		}
		if (p_selected_files.has(child->get_metadata(0))) {
			ret++;
		}
		ret += count_showed_selected_files(child, p_selected_files);
	}
	return ret;
}

void ModularResourcesSelectDialog::refresh() {
	bool show_hidden = show_hidden_btn->is_pressed();
	Vector<String> filters = filter->get_text().split(";", false);
	Vector<String> exlcudes = exclude->get_text().split(";", false);
	bool resources_only = show_recognized_resources_only_btn->is_pressed();

	auto root = select_tree->get_root();
	ERR_FAIL_COND(root == nullptr);
	refresh_recursively(root, show_hidden, resources_only, filters, exlcudes);

	check_items_recursively(root, selected_files, false);

	// Count hidden selected files.
	int hidden_file_count = selected_files.size() - count_showed_selected_files(root, selected_files);

	hidden_files_hint->clear();
	hidden_files_hint->push_color(hidden_file_count == 0 ? Color::named("WHITE") : Color::named("YELLOW"));
	hidden_files_hint->add_text(vformat(STTR("Selected Hidden files count: %d"), hidden_file_count));
	hidden_files_hint->pop();
}

void ModularResourcesSelectDialog::setup_and_popup(Ref<ModularResource> &p_data) {
	ERR_FAIL_COND(p_data.is_null());
	data = p_data;

	Array resources = data->get_resources();
	selected_files.resize(resources.size());
	for (auto i = 0; i < resources.size(); ++i) {
		String path = resources[i];
		if (ResourceLoader::exists(path)) {
			auto uid = ResourceLoader::get_resource_uid(path);
			path = ResourceUID::get_singleton()->get_id_path(uid);
		}
		selected_files.set(i, path);
	}

	select_tree->clear();
	auto root = select_tree->create_item();
	setup_tree_recursively(root, "res://");

	show_hidden_btn->set_pressed(data->get_meta(META_KEY_SHOW_HIDDEN, false));
	show_recognized_resources_only_btn->set_pressed(data->get_meta(META_KEY_SHOW_RESOURCES_ONLY, true));
	filter->set_text(data->get_meta(META_KEY_FILTER, ""));
	exclude->set_text(data->get_meta(META_KEY_EXCLUDE, ""));

	refresh();
	popup_centered_ratio();
}

void ModularResourcesSelectDialog::_confirm_pressed() {
	auto root = select_tree->get_root();
	if (data.is_valid()) {
		Array resources;
		for (auto i = 0; i < selected_files.size(); ++i) {
			resources.push_back(selected_files[i]);
		}

		data->set_resources(resources);

		data->set_meta(META_KEY_SHOW_HIDDEN, show_hidden_btn->is_pressed());
		data->set_meta(META_KEY_SHOW_RESOURCES_ONLY, show_recognized_resources_only_btn->is_pressed());
		data->set_meta(META_KEY_FILTER, filter->get_text());
		data->set_meta(META_KEY_EXCLUDE, exclude->get_text());
	}
	hide();
}

void ModularResourcesSelectDialog::_cancel_pressed() {
	if (data.is_valid()) {
		data->set_meta(META_KEY_SHOW_HIDDEN, show_hidden_btn->is_pressed());
		data->set_meta(META_KEY_SHOW_RESOURCES_ONLY, show_recognized_resources_only_btn->is_pressed());
		data->set_meta(META_KEY_FILTER, filter->get_text());
		data->set_meta(META_KEY_EXCLUDE, exclude->get_text());
	}
	hide();
}

void ModularResourcesSelectDialog::_select_tree_item_edited() {
	auto item = select_tree->get_edited();
	auto checked = item->is_checked(0);
	set_item_checked_recursively(item, checked);

	auto parent = item->get_parent();
	if (checked) {
		while (parent) {
			bool all = true;
			for (auto i = 0; i < parent->get_child_count(); ++i) {
				if (!parent->get_child(i)->is_checked(0)) {
					all = false;
					break;
				}
			}

			if (all) {
				parent->set_checked(0, true);
				parent = parent->get_parent();
			} else {
				break;
			}
		}
	} else {
		while (parent) {
			parent->set_checked(0, false);
			parent = parent->get_parent();
		}
	}
}

ModularResourcesEditorProperty::ModularResourcesEditorProperty() {
	select_btn = memnew(Button);
	add_child(select_btn);
	add_focusable(select_btn);
	select_btn->connect("pressed", callable_mp(this, &ModularResourcesEditorProperty::_select_btn_pressed));
}

void ModularResourcesEditorProperty::update_property() {
	Array resources = get_edited_object()->get(get_edited_property());
	select_btn->set_text(STTR("Count") + ": " + itos(resources.size()));
}

void ModularResourcesEditorProperty::_select_btn_pressed() {
	Ref<ModularResource> data = cast_to<ModularResource>(get_edited_object());
	ERR_FAIL_COND(data.is_null());
	ModularResourcesSelectDialog::get_singleton()->setup_and_popup(data);
}

// ModularDataInspectorPlugin
bool ModularDataInspectorPlugin::can_handle(Object *p_object) {
	return p_object->is_class_ptr(ModularResource::get_class_ptr_static());
}

bool ModularDataInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_type == Variant::ARRAY && p_path == "resources") {
		add_property_editor(p_path, memnew(ModularResourcesEditorProperty));
		return true;
	}

	return false;
}