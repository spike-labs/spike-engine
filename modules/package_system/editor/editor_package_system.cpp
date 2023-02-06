/**
 * editor_package_system.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_package_system.h"
#include "core/config/project_settings.h"
#include "core/io/resource_importer.h"
#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/progress_dialog.h"
#include "editor_package_dialog.h"
#include "main/main.h"

#define TASK_NAME "spike:update_plugins"

EditorPackageSystem *EditorPackageSystem::singleton = nullptr;

EditorPackageSystem::EditorPackageSystem() {
	singleton = this;
	_cfg_time = 0;
	_active_step = -1;
	_current_task = nullptr;

	REF_INSTANTIATE(_config);
}

EditorPackageSystem::~EditorPackageSystem() {
}

void EditorPackageSystem::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_READY: {
			_require_packages_cache();
			_require_packages_dir();
			EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp(this, &EditorPackageSystem::_sources_changed));
			EditorFileSystem::get_singleton()->connect("resources_reload", callable_mp(this, &EditorPackageSystem::_resources_changed));

			auto dialog = memnew(EditorPackageDialog);
			EditorNode::get_singleton()->get_gui_base()->add_child(dialog);
			connect(PLUGIN_HAS_LOADED, callable_mp(dialog, &EditorPackageDialog::_package_has_loaded));
			connect(PLUGIN_UNLOAD_ERR, callable_mp(dialog, &EditorPackageDialog::_package_unload_error));
		} break;
		case NOTIFICATION_PROCESS:
			if (_fetch_step == 1) {
				_fetch_step = 0;
				set_process(false);
				_fetch_source_progress();
			}
			break;

		default:
			break;
	}
}

bool EditorPackageSystem::_get_current_task_step(String &state, int &step) {
	MutexLock task_lock(_eps_mutex);
	if (_current_task) {
		_current_task->get_task_step(state, step);
		return true;
	}
	return false;
}

void EditorPackageSystem::_fetch_source_progress() {
	String task_name = TASK_NAME;
	ProgressDialog::get_singleton()->add_task(task_name, STTR("Update Plugins"), 100);
	while (!_thread_done.is_set()) {
		String state;
		int step;
		if (_get_current_task_step(state, step)) {
			ProgressDialog::get_singleton()->task_step(task_name, state, step / 100.0 * 100);
		}
	}
	_fetch_thread.wait_to_finish();
	ProgressDialog::get_singleton()->end_task(task_name);

	if (_cfg_dirty) {
		update_packages();
	} else {
		String path_manifest = PATH_MANIFEST;
		int fpos = -1;
		auto fs = EditorFileSystem::get_singleton()->find_file(path_manifest, &fpos);
		if (fs) {
			// Mansualy change modified time of package folder, so the file system will rescan that folder.
			fs->modified_time -= 1;
			_active_step = EditorFileSystem::get_singleton()->is_scanning() ? 2 : 1;
			_deactive_expire_plugins();

			EditorFileSystem::get_singleton()->scan_changes();
		}
	}
}

bool EditorPackageSystem::_update_config() {
	String cfg_path = PATH_MANIFEST;
	if (FileAccess::exists(cfg_path)) {
		auto mtime = FileAccess::get_modified_time(cfg_path);
		if (_cfg_time >= mtime)
			return false;

		_cfg_time = mtime;
	} else {
	}

	Ref<ConfigFile> cfg;
	REF_INSTANTIATE(cfg);
	if (cfg->load(cfg_path) != OK) {
		return false;
	}

	if (_config.is_valid()) {
		for (auto &info : package_info_map) {
			if (info.value.status == PACKAGE_ERROR && cfg->has_section_key(SECTION_PLUGINS, info.key) && cfg->get_value(SECTION_PLUGINS, info.key) != _config->get_value(SECTION_PLUGINS, info.key)) {
				// Package source has changed, reset it's status.
				package_info_map[info.key].status = PACKAGE_EXISTS;
			}
		}
	}
	_config = cfg;
	_cfg_dirty = true;

	return true;
}

void EditorPackageSystem::update_packages(const String &p_reset) {
	if (_fetch_thread.is_started()) {
		return;
	}

	if (!IS_EMPTY(p_reset)) {
		auto info = package_info_map.getptr(_get_package_name(p_reset));
		if (info && info->status == PACKAGE_ERROR) {
			info->status = PACKAGE_EXISTS;
		}
	}

	_packages_map.clear();
	String section = SECTION_PLUGINS;
	if (_config->has_section(section)) {
		List<String> keys;
		_config->get_section_keys(section, &keys);
		for (auto E = keys.front(); E; E = E->next()) {
			String key = E->get();
			String value = _config->get_value(section, key);
			_packages_map.insert(key, value);
		}
	}

	set_process(true);
	_fetch_step = 1;

	_thread_done.clear();
	_fetch_thread.start(&EditorPackageSystem::_source_handler, this);
}

void EditorPackageSystem::_deactive_expire_plugins() {
	for (auto kv : package_info_map) {
		PackageInfo &info = package_info_map[kv.key];
		uint64_t prev_mtime = info.prev_modified_time;
		if (prev_mtime != info.modified_time) {
			String plugin_path = NAME_TO_PLUGIN_PATH(kv.key);
			if (is_package_plugin_enabled(plugin_path)) {
				if (prev_mtime == 0) {
					info.prev_modified_time = info.modified_time;
					continue;
				}

				_disable_plugin(plugin_path, kv.key);
			}
		}
	}
}

void EditorPackageSystem::_active_plugins(int n) {
	if (n > 0) {
		MessageQueue::get_singleton()->push_callable(callable_mp(this, &EditorPackageSystem::_active_plugins), n - 1);
		return;
	}

	auto da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	for (auto kv : package_info_map) {
		PackageInfo &info = package_info_map[kv.key];
		uint64_t prev_mtime = info.prev_modified_time;
		if (prev_mtime != info.modified_time) {
			info.prev_modified_time = info.modified_time;
		}

		if (info.status == PACKAGE_ERROR || info.status == PACKAGE_REMOVED) {
			continue;
		}

		if (_config.is_valid() && _config->has_section_key(DISACTIVE_SECTION, kv.key) && _config->get_value(DISACTIVE_SECTION, kv.key)) {
			continue;
		}

		String plugin_path = NAME_TO_PLUGIN_PATH(kv.key);
		if (!is_package_plugin_enabled(plugin_path)) {
			_enable_plugin(plugin_path, kv.key);
		}
	}
}

Error EditorPackageSystem::_handle_packages_source(const Map<String, String> &p_packages) {
	Set<String> remove_list;
	for (auto kv : package_info_map) {
		remove_list.insert(kv.key);
	}
	{
		auto da = DirAccess::open(DIR_PLUGINS);
		da->list_dir_begin();
		for (auto n = da->get_next(); !IS_EMPTY(n); n = da->get_next()) {
			if (n != "." && n != ".." && da->current_is_dir()) {
				remove_list.insert(n);
			}
		}
		da->list_dir_end();
	}

	Error ret = OK;

	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto packages_dir = ProjectSettings::get_singleton()->globalize_path(ProjectSettings::get_singleton()->get_resource_path()).path_join(NAME_PLUGINS);
	for (auto kv : p_packages) {
		String package_name = kv.key;
		String package_source = kv.value;
		if (!package_info_map.has(package_name)) {
			package_info_map.insert(package_name, PackageInfo());
		}
		PackageInfo &info = package_info_map.get(package_name);
		if (info.status == PACKAGE_ERROR) {
			continue;
		}

		EditorPackageSource *src = nullptr;
		if (package_source.begins_with("file://")) {
			src = memnew(EditorPackageSourceLocal(package_name, package_source));
		} else if (package_source.begins_with("http://") || package_source.begins_with("https://") || package_source.begins_with("git@")) {
			src = memnew(EditorPackageSourceGit(package_name, package_source));
		} else if (!IS_EMPTY(package_source)) {
			String full_pkg_name = package_name + "@" + package_source + ".zip";
			String registry_url = EditorPackageSourceRegistry::find_registry(full_pkg_name, _config);
			if (!IS_EMPTY(registry_url)) {
				src = memnew(EditorPackageSourceRegistry(package_name, registry_url, package_source));
			}
		}

		auto loc_path = packages_dir.path_join(package_name);
		bool pkg_exists = da->dir_exists(loc_path);
		String ntag = src ? src->get_tag() : String();
		String otag;
		if (pkg_exists) {
			auto f = FileAccess::open(PATH_JOIN(loc_path, ".tag"), FileAccess::READ);
			if (f.is_valid()) {
				otag = f->get_line();
			}
		}

		auto err = OK;
		String err_context = "";
		if (!pkg_exists || ntag != otag) {
			_set_current_task(src);
			call_deferred("emit_signal", PLUGIN_WILL_LOAD, package_name);

			if (pkg_exists) {
				if (_delete_package_folder(loc_path) != OK) {
					err = ERR_FILE_ALREADY_IN_USE;
				}
			}

			if (err == OK) {
				err = src ? src->fetch_source(loc_path, err_context) : FAILED;
				_set_current_task(nullptr);
				if (err == OK) {
					auto f = FileAccess::open(PATH_JOIN(loc_path, ".tag"), FileAccess::WRITE);
					f->store_line(ntag);

					Ref<DirAccess> dir = DirAccess::open(loc_path);
					auto ignore_file = PATH_JOIN(loc_path, ".gitignore");
					if (!dir->file_exists(ignore_file)) {
						FileAccess::open(ignore_file, FileAccess::WRITE)->store_string("*");
					}
				}
			}
		} else {
			ret = OK;
		}

		if (src) {
			memdelete(src);
			src = nullptr;
		}
		info.modified_time = get_modified_time(loc_path + "/");

		remove_list.erase(package_name);
		if (err != OK) {
			info.status = PACKAGE_ERROR;
			_set_load_err_info(package_name, err, err_context);
			break;
		} else {
			info.status = PACKAGE_LOADED;
			call_deferred("emit_signal", PLUGIN_HAS_LOADED, package_name, String());
		}
	}

	PackedStringArray remove_arr;
	da->change_dir(packages_dir);
	for (auto key : remove_list) {
		auto info = package_info_map.getptr(key);
		if (info && info->status == PACKAGE_REMOVED) {
			continue;
		}
		if (da->dir_exists(key)) {
			remove_arr.push_back(DIR_PLUGINS + key);
		}
	}
	if (remove_arr.size()) {
		MessageQueue::get_singleton()->push_callable(callable_mp(this, &EditorPackageSystem::_remove_packages_from_disk), remove_arr);
	}
	call_deferred("emit_signal", PLUGIN_PROC_FINISHED);
	return ret;
}

Error EditorPackageSystem::_delete_package_folder(const String &folder_path) {
	return OS::get_singleton()->move_to_trash(folder_path);
}

void EditorPackageSystem::_remove_packages_from_disk(const PackedStringArray &paths) {
	for (size_t i = 0; i < paths.size(); i++) {
		String plugin_path = GET_PLUGIN_PATH(paths[i]);
		if (is_package_plugin_enabled(plugin_path)) {
			_disable_plugin(plugin_path);
		}
		if (FileAccess::exists(PATH_JOIN(paths[i], ".tag"))) {
			auto package_name = _get_package_name(plugin_path);
			if (_config.is_valid() && _config->has_section_key(DISACTIVE_SECTION, package_name)) {
				_config->set_value(DISACTIVE_SECTION, package_name, Variant());
				_config->save(PATH_MANIFEST);
			}
			Error ret = _delete_package_folder(ProjectSettings::get_singleton()->globalize_path(paths[i]));
			String key = paths[i].get_file();
			if (ret != OK) {
				auto info = package_info_map.getptr(key);
				if (info) {
					info->status = PACKAGE_REMOVED;
				}
				String s = vformat(STTR("The plugin '%s' is in use, please restart the editor for complete unloading."), key);
				_set_unload_error(s);
			} else {
				package_info_map.erase(key);
			}
		}
		_update_autoload_from_disk(paths[i]);
	}
}

void EditorPackageSystem::_update_autoload_from_disk(const String &path_info) {
	Vector<String> autoloads;

	const auto auto_list = ProjectSettings::get_singleton()->get_autoload_list();
	for (const auto &e : auto_list) {
		if (e.value.path.begins_with(path_info)) {
			autoloads.push_back(e.key);
		}
	}

	for (const auto &e : autoloads) {
		String name = "autoload/" + e;
		ProjectSettings::get_singleton()->clear(name);
		ProjectSettings::get_singleton()->save();
		EditorNode::get_singleton()->get_log()->add_message(TTR("Remove AutoLoad") + " " + e, EditorLog::MSG_TYPE_EDITOR);
	}
}

void EditorPackageSystem::_preload_fs(const EditorFileSystemDirectory *p_fs) {
	if (nullptr == p_fs)
		return;

	for (size_t i = 0; i < p_fs->get_file_count(); i++) {
		if (p_fs->get_file_modified_time(i) > _cfg_time) {
			auto fpath = p_fs->get_file_path(i);
			if (IS_EMPTY(ResourceLoader::get_resource_type(fpath)))
				continue;

			if (ResourceFormatImporter::get_singleton()->are_import_settings_valid(fpath))
				continue;

			if (!ResourceCache::has(fpath)) {
				ResourceLoader::load(fpath);
			}
		}
	}

	for (size_t i = 0; i < p_fs->get_subdir_count(); i++) {
		_preload_fs(const_cast<EditorFileSystemDirectory *>(p_fs)->get_subdir(i));
	}
}

void EditorPackageSystem::_sources_changed(bool exist) {
	if (_active_step > 0) {
		// Preload add files & Reload modified files
		for (const auto &info : package_info_map) {
			if (info.value.modified_time > _cfg_time) {
				_preload_fs(EditorFileSystem::get_singleton()->get_filesystem_path(DIR_PLUGINS + info.key));
			}
		}
		_active_step -= 1;
		if (_active_step == 0) {
			// Should active packages after EditorFileSystem::update_script_classes,
			// But there is no signals for this behaviour,
			// So we delay the active action in 2 frames.
			MessageQueue::get_singleton()->push_callable(callable_mp(this, &EditorPackageSystem::_active_plugins), 1);
		}
	} else {
		_scan_packages();
	}
}

void EditorPackageSystem::_resources_changed(const Vector<String> &p_resources) {
	if (_active_step > 0) {
		int rc = p_resources.size();
		for (int i = 0; i < rc; i++) {
			Ref<Resource> res = ResourceCache::get_ref(p_resources.get(i));
			if (res.is_null()) {
				continue;
			}
			if (!res->get_path().is_resource_file() && !res->get_path().is_absolute_path()) {
				continue;
			}
			if (!FileAccess::exists(res->get_path())) {
				continue;
			}
			if (!res->get_import_path().is_empty()) {
				continue;
			}

			if (!res->editor_can_reload_from_file()) {
				// force reload from file
				res->reload_from_file();
			}
		}
	}
}

void EditorPackageSystem::_bind_methods() {
	ClassDB::bind_static_method("EditorPackageSystem", D_METHOD("get_singleton"), &EditorPackageSystem::get_singleton);
	ClassDB::bind_method(D_METHOD("get_package_status", "package"), &EditorPackageSystem::get_package_status);
	ClassDB::bind_method(D_METHOD("is_package_plugin_enabled", "package"), &EditorPackageSystem::is_package_plugin_enabled);
	ClassDB::bind_method(D_METHOD("set_package_plugin_enabled", "package", "enabled"), &EditorPackageSystem::set_package_plugin_enabled);

	ADD_SIGNAL(MethodInfo(PLUGIN_WILL_LOAD, PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo(PLUGIN_HAS_LOADED, PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo(PLUGIN_PROC_FINISHED));
	//ADD_SIGNAL(MethodInfo(PLUGIN_LOAD_ERR));
	ADD_SIGNAL(MethodInfo(PLUGIN_UNLOAD_ERR));

	BIND_ENUM_CONSTANT(PACKAGE_NONE);
	BIND_ENUM_CONSTANT(PACKAGE_EXISTS);
	BIND_ENUM_CONSTANT(PACKAGE_LOADED);
	BIND_ENUM_CONSTANT(PACKAGE_ERROR);
	BIND_ENUM_CONSTANT(PACKAGE_REMOVED);
}

uint64_t EditorPackageSystem::get_modified_time(const String &dir_path) {
	auto da = DirAccess::open(dir_path);
	if (da.is_valid()) {
		uint64_t mtime = 0;
		da->list_dir_begin();
		String n = da->get_next();
		while (!n.is_empty()) {
			if (n != "." && n != ".." && !n.ends_with(".import")) {
				auto time = da->current_is_dir() ? get_modified_time(dir_path + n + "/") : FileAccess::get_modified_time(dir_path + n);
				if (time > mtime)
					mtime = time;
			}
			n = da->get_next();
		}
		da->list_dir_end();
		return mtime;
	} else {
		return FileAccess::get_modified_time(dir_path);
	}
}

void EditorPackageSystem::_source_handler(void *p_user) {
	auto eps = static_cast<EditorPackageSystem *>(p_user);

	auto packages = eps->_packages_map;
	eps->_cfg_dirty = false;
	eps->_handle_packages_source(packages);
	eps->_thread_done.set();
}

void EditorPackageSystem::_set_package_plugin_enabled(const String &p_name_path, bool p_enabled) {
	String plugin_path = _get_plugin_path(p_name_path);

	ERR_FAIL_COND(p_enabled && _enabled_plugins_map.has(plugin_path));
	ERR_FAIL_COND(!p_enabled && !_enabled_plugins_map.has(plugin_path));

	auto plugin_ptr = _enabled_plugins_map.getptr(plugin_path);
	if (!p_enabled) {
		if (plugin_ptr) {
			EditorPlugin *plugin = *plugin_ptr;
			EditorNode::remove_editor_plugin(plugin, true);
			memdelete(plugin);
			_enabled_plugins_map.erase(plugin_path);
		} else {
			WARN_PRINT("Addon '" + plugin_path + "' has been disabled.");
		}
		return;
	}

	if (plugin_ptr) {
		WARN_PRINT("Addon '" + plugin_path + "' has been enabled.");
		return;
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	if (!DirAccess::exists(plugin_path.get_base_dir())) {
		WARN_PRINT("Addon '" + plugin_path + "' failed to load. No directory found. Removing from enabled plugins.");
		return;
	}
	Error err = cf->load(plugin_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to enable addon plugin at: '%s' parsing of config failed."), plugin_path));
		return;
	}

	if (!cf->has_section_key("plugin", "script")) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to find script field for addon plugin at: '%s'."), plugin_path));
		return;
	}

	String script_path = cf->get_value("plugin", "script");
	Ref<Script> scr; // We need to save it for creating "ep" below.

	// Only try to load the script if it has a name. Else, the plugin has no init script.
	if (script_path.length() > 0) {
		script_path = plugin_path.get_base_dir().path_join(script_path);
		scr = ResourceLoader::load(script_path);

		if (scr.is_null()) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to load addon script from path: '%s'."), script_path));
			return;
		}

		// Errors in the script cause the base_type to be an empty StringName.
		if (scr->get_instance_base_type() == StringName()) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to load addon script from path: '%s'. This might be due to a code error in that script.\nDisabling the addon at '%s' to prevent further errors."), script_path, plugin_path));
			return;
		}

		// Plugin init scripts must inherit from EditorPlugin and be tools.
		if (String(scr->get_instance_base_type()) != "EditorPlugin") {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to load addon script from path: '%s' Base type is not EditorPlugin."), script_path));
			return;
		}

		if (!scr->is_tool()) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Unable to load addon script from path: '%s' Script is not in tool mode."), script_path));
			return;
		}
	}

	EditorPlugin *ep = memnew(EditorPlugin);
	ep->set_script(scr);
	_enabled_plugins_map[plugin_path] = ep;
	EditorNode::add_editor_plugin(ep, true);
}

void EditorPackageSystem::_set_load_err_info(const String plugin_name, Error err, const String &err_text) {
	switch (err) {
		case OK:
			return;
		case ERR_FILE_ALREADY_IN_USE: {
			String s = vformat(STTR("The plugin '%s' is in use, please restart the editor for loading another version."), plugin_name);
			_set_unload_error(s);
			return;
		}
		default:
			break;
	}
	call_deferred("emit_signal", PLUGIN_HAS_LOADED, plugin_name, err_text);
}

void EditorPackageSystem::_set_unload_error(const String &p_error) {
	call_deferred("emit_signal", PLUGIN_UNLOAD_ERR, p_error);
}

EditorPackageSystem::PackageStatus EditorPackageSystem::get_package_status(const String &p_name_path) {
	auto info = package_info_map.getptr(_get_package_name(p_name_path));
	return info ? info->status : PackageStatus::PACKAGE_NONE;
}

bool EditorPackageSystem::is_package_plugin_enabled(const String &p_name_path) {
	return _enabled_plugins_map.has(_get_plugin_path(p_name_path));
}

void EditorPackageSystem::set_package_plugin_enabled(const String &p_name_path, const bool p_enabled) {
	_set_package_plugin_enabled(p_name_path, p_enabled);
	String package_name = _get_package_name(p_name_path);
	if (_config.is_valid()) {
		_config->set_value(DISACTIVE_SECTION, package_name, p_enabled ? Variant() : Variant(true));
		_config->save(PATH_MANIFEST);
	}
}