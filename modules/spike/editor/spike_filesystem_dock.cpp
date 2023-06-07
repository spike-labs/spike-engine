/**
 * spike_filesystem_dock.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_filesystem_dock.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "package_system/editor/editor_package_system.h"
#include "spike_define.h"
#include "spike_dependency_remove_dialog.h"
#include "spike_editor_utils.h"

#define HIDDEN_PREFIX "spike."
#define PLUGINS_PATH "res://" NAME_PLUGINS

static void _unregister_menu(PackedStringArray &p_menus) {
	Object::cast_to<SpikeFileSystemDock>(SpikeFileSystemDock::get_singleton())->unregister_menu_item(p_menus);
}

static void _remove_submenu(PopupMenu *p_menu) {
	for (size_t i = 0; i < p_menu->get_item_count(); i++) {
		auto submenu = p_menu->get_item_submenu(i);
		if (!IS_EMPTY(submenu)) {
			if (auto child = p_menu->find_child(submenu, false, false)) {
				p_menu->remove_child(child);
				memdelete(child);
			}
		}
	}
}

void SpikeFileSystemDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_generate_file_thumbnail_done", "p_path", "p_preview", "p_small_preview", "p_udata"), &SpikeFileSystemDock::_generate_file_thumbnail_done);

	ADD_SIGNAL(MethodInfo("dirs_and_files_before_remove", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files_to_delete"),
			PropertyInfo(Variant::PACKED_STRING_ARRAY, "dirs_to_delete")));

	ADD_SIGNAL(MethodInfo("tree_updated", PropertyInfo(Variant::OBJECT, "tree")));
}

void SpikeFileSystemDock::_file_removed(String p_file) {
	FileSystemDock::_file_removed(p_file);
}

void SpikeFileSystemDock::_folder_removed(String p_folder) {
	FileSystemDock::_folder_removed(p_folder);
}

SpikeFileSystemDock::SpikeFileSystemDock() {
	custom_menus = memnew(NodeContextRef<PackedStringArray>);
	custom_menus->unregister = &_unregister_menu;

	remove_dialog->queue_free();
	auto sdrd = memnew(SpikeDependencyRemoveDialog);

	sdrd->connect("file_removed", callable_mp(this, &SpikeFileSystemDock::_file_removed));
	sdrd->connect("folder_removed", callable_mp(this, &SpikeFileSystemDock::_folder_removed));
	sdrd->connect("dirs_and_files_before_remove", callable_mp(this, &SpikeFileSystemDock::_dirs_and_files_before_remove));
	add_child(sdrd);
	remove_dialog = sdrd;
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	efs->connect("resources_reimported", callable_mp(this, &SpikeFileSystemDock::_try_update_file_thumbnail));

	tree->disconnect("item_mouse_selected", callable_mp((FileSystemDock *)this, &SpikeFileSystemDock::_tree_rmb_select));
	tree->disconnect("empty_clicked", callable_mp((FileSystemDock *)this, &SpikeFileSystemDock::_tree_empty_click));
	tree->connect("item_collapsed", callable_mp(this, &SpikeFileSystemDock::_item_collapsed));
	tree->connect("item_mouse_selected", callable_mp(this, &SpikeFileSystemDock::_item_mouse_selected));
	tree->connect("empty_clicked", callable_mp(this, &SpikeFileSystemDock::_spike_tree_empty_click));
	tree_popup->connect("index_pressed", callable_mp(this, &SpikeFileSystemDock::_popup_index_pressed).bind(tree_popup));

	files->disconnect("item_clicked", callable_mp((FileSystemDock *)this, &SpikeFileSystemDock::_file_list_item_clicked));
	files->disconnect("empty_clicked", callable_mp((FileSystemDock *)this, &SpikeFileSystemDock::_file_list_empty_clicked));
	files->connect("item_clicked", callable_mp(this, &SpikeFileSystemDock::_spike_file_list_item_clicked));
	files->connect("empty_clicked", callable_mp(this, &SpikeFileSystemDock::_spike_file_list_empty_clicked));
	file_list_popup->connect("index_pressed", callable_mp(this, &SpikeFileSystemDock::_popup_index_pressed).bind(file_list_popup));
}

SpikeFileSystemDock::~SpikeFileSystemDock() {
	memdelete(custom_menus);
}

bool SpikeFileSystemDock::_find_file_tree_item(const String &fpath, TreeItem *&r_it, TreeItem *it) {
	if (!it) {
		return false;
	}
	auto meta = it->get_metadata(0);
	if (meta.get_type() == Variant::STRING && it->get_metadata(0) == fpath) {
		r_it = it;
		return true;
	} else {
		auto children = it->get_children();
		for (size_t i = 0; i < children.size(); i++) {
			auto child = Object::cast_to<TreeItem>(children[i]);
			if (child && _find_file_tree_item(fpath, r_it, child)) {
				return true;
			}
		}
		return false;
	}
}

void SpikeFileSystemDock::_try_update_file_thumbnail(PackedStringArray resources) {
	for (size_t i = 0; i < resources.size(); i++) {
		update_file_thumbnail(resources[i]);
	}
}

void SpikeFileSystemDock::_file_menu_check_uid_missing(PopupMenu *p_menu, const String &p_path) {
	auto index = p_menu->get_item_index(FILE_COPY_UID);
	if (index >= 0 && ResourceSaver::get_resource_id_for_path(p_path) == ResourceUID::INVALID_ID) {
		p_menu->set_item_icon(index, get_theme_icon(SNAME("Reload"), SNAME("EditorIcons")));
		p_menu->set_item_text(index, STTR("Update File"));
		p_menu->set_item_id(index, UPDATE_FILE);
	}
}

void SpikeFileSystemDock::update_file_thumbnail(const String &fpath) {
	EditorResourcePreview::get_singleton()->call_deferred("queue_resource_preview", fpath, this, "_generate_file_thumbnail_done", Variant());
}

void SpikeFileSystemDock::_generate_file_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
	if ((p_preview.is_null() || p_small_preview.is_null())) {
		return;
	}

	ResourceUID::get_singleton()->update_cache();

	TreeItem *it = nullptr;
	if (_find_file_tree_item(p_path, it, tree->get_root())) {
		it->set_icon(0, p_small_preview);
	}

	for (size_t i = 0; i < files->get_item_count(); i++) {
		if (files->get_item_metadata(i) == p_path) {
			if (file_list_display_mode == FILE_LIST_DISPLAY_LIST) {
				if (p_small_preview.is_valid()) {
					files->set_item_icon(i, p_small_preview);
				}
			} else {
				files->set_item_icon(i, p_preview);
			}
			return;
		}
	}
}

void SpikeFileSystemDock::_item_collapsed(TreeItem *item) {
	String path = item->get_metadata(0);
	if (path.begins_with(PLUGINS_PATH) && IS_EMPTY(searched_string)) {
		if (item->is_collapsed()) {
			plugin_uncollapsed_paths.erase(path);
		} else {
			plugin_uncollapsed_paths.push_back(path);
		}
	}
}

void SpikeFileSystemDock::_item_mouse_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	// Right click is pressed in the tree.
	Vector<String> paths = _tree_get_selected(false);
	selected_paths = paths;

	// Remove submenus
	_remove_submenu(tree_popup);
	tree_popup->clear();
	if (paths.size() == 1) {
		if (paths[0].ends_with("/")) {
			tree_popup->add_icon_item(get_theme_icon(SNAME("GuiTreeArrowDown"), SNAME("EditorIcons")), TTR("Expand All"), FOLDER_EXPAND_ALL);
			tree_popup->add_icon_item(get_theme_icon(SNAME("GuiTreeArrowRight"), SNAME("EditorIcons")), TTR("Collapse All"), FOLDER_COLLAPSE_ALL);
			tree_popup->add_separator();
		}
	}

	// Popup.
	if (!paths.is_empty()) {
		tree_popup->reset_size();
		if (paths.size() == 1 && paths[0] == DIR_PLUGINS) {
			if (EditorSettings::get_singleton()->get_favorites().find(paths[0]) < 0) {
				tree_popup->add_icon_item(get_theme_icon(SNAME("Favorites"), SNAME("EditorIcons")), TTR("Add to Favorites"), FILE_ADD_FAVORITE);
			} else {
				tree_popup->add_icon_item(get_theme_icon(SNAME("NonFavorite"), SNAME("EditorIcons")), TTR("Remove from Favorites"), FILE_REMOVE_FAVORITE);
			}
			tree_popup->add_separator();
			tree_popup->add_icon_item(get_theme_icon(SNAME("Load"), SNAME("EditorIcons")), TTR("Open"), FILE_OPEN);
			tree_popup->add_separator();
			tree_popup->add_icon_shortcut(get_theme_icon(SNAME("ActionCopy"), SNAME("EditorIcons")), ED_GET_SHORTCUT("filesystem_dock/copy_path"), FILE_COPY_PATH);
			tree_popup->add_separator();
			tree_popup->add_icon_item(get_theme_icon(SNAME("Folder"), SNAME("EditorIcons")), TTR("New Folder..."), FILE_NEW_FOLDER);
			tree_popup->add_icon_item(get_theme_icon(SNAME("PackedScene"), SNAME("EditorIcons")), TTR("New Scene..."), FILE_NEW_SCENE);
			tree_popup->add_icon_item(get_theme_icon(SNAME("Script"), SNAME("EditorIcons")), TTR("New Script..."), FILE_NEW_SCRIPT);
			tree_popup->add_icon_item(get_theme_icon(SNAME("Object"), SNAME("EditorIcons")), TTR("New Resource..."), FILE_NEW_RESOURCE);
			tree_popup->add_icon_item(get_theme_icon(SNAME("TextFile"), SNAME("EditorIcons")), TTR("New TextFile..."), FILE_NEW_TEXTFILE);
			tree_popup->add_separator();
			tree_popup->add_icon_item(get_theme_icon(SNAME("Filesystem"), SNAME("EditorIcons")), TTR("Open in File Manager"), FILE_SHOW_IN_EXPLORER);
		} else {
			if (paths[0] != "res://" && paths.find(DIR_PLUGINS) >= 0) {
				paths.insert(0, "res://");
			}
			_file_and_folders_fill_popup(tree_popup, paths);
			if (paths.find("res://") >= 0) {
				auto move_idx = tree_popup->get_item_index(FILE_MOVE);
				if (move_idx >= 0)
					tree_popup->remove_item(move_idx);
				auto remove_ids = tree_popup->get_item_index(FILE_REMOVE);
				if (remove_ids >= 0)
					tree_popup->remove_item(remove_ids);
			} else if (paths.size() == 1) {
				_file_menu_check_uid_missing(tree_popup, paths[0]);
			}
		}

		_create_custom_menus(tree_popup);

		tree_popup->set_position(tree->get_screen_position() + p_pos);
		tree_popup->reset_size();
		tree_popup->popup();
	}
}

void SpikeFileSystemDock::_spike_tree_empty_click(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}

	selected_paths = PackedStringArray();

	_remove_submenu(tree_popup);
	_tree_empty_click(p_pos, p_button);
	if (_create_custom_menus(tree_popup)) {
		tree_popup->reset_size();
		tree_popup->popup();
	}
}

void SpikeFileSystemDock::_spike_file_list_item_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	_remove_submenu(file_list_popup);
	file_list_popup->clear();

	Vector<String> paths;
	for (int i = 0; i < files->get_item_count(); i++) {
		if (!files->is_selected(i)) {
			continue;
		}
		if (files->get_item_text(p_item) == "..") {
			files->deselect(i);
			continue;
		}
		paths.push_back(files->get_item_metadata(i));
	}
	selected_paths = paths;

	if (!paths.is_empty()) {
		_file_and_folders_fill_popup(file_list_popup, paths, searched_string.length() == 0);
		if (paths.size() == 0) {
			_file_menu_check_uid_missing(file_list_popup, paths[0]);
		}
	}

	_create_custom_menus(file_list_popup);

	file_list_popup->set_position(files->get_screen_position() + p_pos);
	file_list_popup->reset_size();
	file_list_popup->popup();
}

void SpikeFileSystemDock::_spike_file_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	files->deselect_all();

	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	selected_paths = PackedStringArray();
	_remove_submenu(file_list_popup);
	_file_list_empty_clicked(p_pos, p_mouse_button_index);
	if (_create_custom_menus(file_list_popup)) {
		file_list_popup->reset_size();
		file_list_popup->popup();
	}
}

void SpikeFileSystemDock::_popup_index_pressed(int p_index, PopupMenu *popup) {
	int id = popup->get_item_id(p_index);
	switch (id) {
		case CUSTOM_CALLBACK: {
			if (IS_EMPTY(popup->get_item_submenu(p_index))) {
				Variant validate = Variant(false);
				Variant paths = Variant(selected_paths);
				Callable callback = popup->get_item_metadata(p_index);
				Callable::CallError ce;
				Variant result;
				if (popup->is_item_checkable(p_index)) {
					Variant checked = popup->is_item_checked(p_index);
					const Variant *args[] = { &validate, &checked, &paths };
					callback.callp(args, 3, result, ce);
					if (ce.error == Callable::CallError::CALL_OK) {
						popup->set_item_checked(p_index, result);
					}
				} else {
					const Variant *args[] = { &validate, &paths };
					callback.callp(args, 2, result, ce);
				}

				if (ce.error != Callable::CallError::CALL_OK) {
					String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
					ERR_PRINT("Error calling execute: " + err);
				}
			}
		} break;
		case UPDATE_FILE: {
			String path = selected_paths[0];
			EditorFileSystem::get_singleton()->update_file(path);
			EditorFileSystem::get_singleton()->scan();
		} break;
		default:
			break;
	}
}

int SpikeFileSystemDock::_create_custom_menus(Node *p_root) {
	int ret = context_menu_list.size();
	if (ret > 0) {
		if (auto p_menu = Object::cast_to<PopupMenu>(p_root)) {
			p_menu->add_separator();
		}
		Variant validate = Variant(true);
		Variant paths = Variant(selected_paths);
		const Variant *args[] = { &validate, &paths };
		Callable::CallError ce;
		Variant result;

		Callable callable = callable_mp(this, &SpikeFileSystemDock::_popup_index_pressed);
		for (auto &item : context_menu_list) {
			item.action.callp(args, 2, result, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				String err = Variant::get_callable_error_text(item.action, nullptr, 0, ce);
				ERR_PRINT("Error calling validate: " + err);
				continue;
			}
			if (result.operator bool() == false)
				continue;

			PackedStringArray menus = PackedStringArray(item.menus);
			int index;
			auto submenu = EditorUtils::make_menu(p_root, menus, callable, index);
			if (submenu) {
				if (index < 0) {
					index = submenu->get_item_count();
					submenu->add_item(TTR(item.menus[item.menus.size() - 1]), CUSTOM_CALLBACK);
				}
				submenu->set_item_metadata(index, item.action);
				submenu->set_item_icon(index, item.icon);
			}
		}
	}
	return ret;
}

bool SpikeFileSystemDock::_create_plugins_tree(EditorFileSystemDirectory *p_dir, bool p_select_in_favorites, bool p_unfold_path) {
	bool parent_should_expand = false;
	auto cache_uncollapsed_paths = PackedStringArray(plugin_uncollapsed_paths);

	int item_index = tree->get_root()->get_child_count();
	parent_should_expand = FileSystemDock::_create_tree(tree->get_root(), p_dir, cache_uncollapsed_paths, p_select_in_favorites, p_unfold_path);
	if (item_index < tree->get_root()->get_child_count()) {
		auto spike_plugins = tree->get_root()->get_child(item_index);
		spike_plugins->set_text(0, STTR("PACKAGES"));
		String lpath = spike_plugins->get_metadata(0);
		if (parent_should_expand) {
			spike_plugins->set_collapsed(false);
		} else {
			spike_plugins->set_collapsed(cache_uncollapsed_paths.find(lpath) < 0);
		}

		if (IS_EMPTY(searched_string)) {
			plugin_uncollapsed_paths.clear();
			Vector<TreeItem *> needs_check;
			needs_check.push_back(spike_plugins);
			while (needs_check.size()) {
				if (!needs_check[0]->is_collapsed()) {
					plugin_uncollapsed_paths.push_back(needs_check[0]->get_metadata(0));
					TreeItem *child = needs_check[0]->get_first_child();
					while (child) {
						needs_check.push_back(child);
						child = child->get_next();
					}
				}
				needs_check.remove_at(0);
			}
		}
	}
	return parent_should_expand;
}

bool SpikeFileSystemDock::_create_tree(TreeItem *p_parent, EditorFileSystemDirectory *p_dir, Vector<String> &uncollapsed_paths, bool p_select_in_favorites, bool p_unfold_path) {
	if (p_dir == EditorFileSystem::get_singleton()->get_filesystem()) {
		call_deferred("emit_signal", "tree_updated", tree);
	}

	if (p_parent->get_metadata(0) == String("res://")) {
		String dname = p_dir->get_name();
		if (dname == NAME_PLUGINS) {
			String plugins_root = PLUGINS_PATH;
			bool unfold_path = uncollapsed_paths.find(plugins_root) >= 0;
			uncollapsed_paths.erase(plugins_root);
			return _create_plugins_tree(p_dir, p_select_in_favorites, unfold_path);
		} else if (dname.begins_with(HIDDEN_PREFIX)) {
			return false;
		}
	}

	if (IS_EMPTY(p_dir->get_name()) && p_unfold_path) {
		if (path.begins_with(PLUGINS_PATH)) {
			uncollapsed_paths.push_back(PLUGINS_PATH);
			p_unfold_path = false;
		}
	}
	return FileSystemDock::_create_tree(p_parent, p_dir, uncollapsed_paths, p_select_in_favorites, p_unfold_path);
}

void SpikeFileSystemDock::_update_file_list(bool p_keep_selection) {
	FileSystemDock::_update_file_list(p_keep_selection);
	if (!IS_EMPTY(searched_string)) {
		PackedInt32Array pkg_files;
		for (int i = 0; i < files->get_item_count(); i++) {
			String meta = files->get_item_metadata(i);
			if (meta.begins_with(PLUGINS_PATH)) {
				pkg_files.push_back(i);
			}
		}

		int pkg_count = pkg_files.size();
		int item_count = files->get_item_count();
		if (pkg_count > 0 && pkg_count < item_count) {
			int last_idx = item_count - 1;
			for (int i = 0; i < pkg_count; i++) {
				int n = pkg_files[i] - i;
				files->move_item(n, last_idx);
			}

			int index = files->add_item(" -", Ref<Texture2D>(), false);
			files->move_item(index, item_count - pkg_count);
		}
	} else if (path == "res://") {
		int index = files->find_metadata(PLUGINS_PATH "/");
		if (index >= 0) {
			files->remove_item(index);
		}
	}
}