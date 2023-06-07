/**
 * editor_package_system.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "core/io/config_file.h"
#include "editor/editor_file_system.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor_package_source.h"
#include "spike_define.h"

#define NAME_PLUGINS "spike.plugins"
#define DIR_PLUGINS "res://" NAME_PLUGINS "/"
#define FILE_MANIFEST "manifest.cfg"
#define PATH_MANIFEST DIR_PLUGINS FILE_MANIFEST
#define SECTION_PLUGINS "plugins"
#define DISACTIVE_SECTION "disable"

#define PLUGIN_WILL_LOAD "package_will_load"
#define PLUGIN_HAS_LOADED "package_has_loaded"
#define PLUGIN_PROC_FINISHED "packages_reloaded"
//#define PLUGIN_LOAD_ERR "plugin_load_err"
#define PLUGIN_UNLOAD_ERR "package_remove_fail"

#define GET_PLUGIN_PATH(path) PATH_JOIN(path, "plugin.cfg")
#define NAME_TO_PLUGIN_PATH(name) GET_PLUGIN_PATH(DIR_PLUGINS + (name))

#define DEFAULT_REGISTRY_URL "https://spike-engine-global.oss-us-west-1.aliyuncs.com/packages/"

class EditorPackageSystem : public Node {
	GDCLASS(EditorPackageSystem, Node);

public:
	enum PackageStatus {
		/// @brief The package is not configured.
		PACKAGE_NONE,
		/// @brief The package is configured, but has not yet been loaded.
		PACKAGE_EXISTS,
		/// @brief The configured package has been successfully loaded.
		PACKAGE_LOADED,
		/// @brief The configured package has been successfully loaded and the internal plug-in (if any) has been enabled.
		/// @deprecated
		PACKAGE_ENABLED,
		/// @brief Error occurred during package loading.
		PACKAGE_ERROR,
		/// @brief The package is mark removed.
		PACKAGE_REMOVED,
	};

private:
	class PackageInfo {
	public:
		uint64_t prev_modified_time;
		uint64_t modified_time;
		PackageStatus status;
		PackageInfo() :
				prev_modified_time(0), modified_time(0), status(PACKAGE_EXISTS) {
		}
	};

	static EditorPackageSystem *singleton;
	SafeFlag _thread_working;

	Mutex _eps_mutex;
	bool _cfg_dirty;
	uint64_t _cfg_time;
	Ref<ConfigFile> _config;
	Map<String, String> _packages_map;
	int _fetch_step;

	int _active_step;

	EditorPackageSource *_current_task;

	HashMap<String, EditorPlugin *> _enabled_plugins_map;

	void _set_current_task(EditorPackageSource *task) {
		MutexLock task_lock(_eps_mutex);
		_current_task = task;
	}

	bool _require_packages_cache() {
		auto cache_dir = EditorPackageSource::get_cache_dir();
		auto da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (!da->dir_exists(cache_dir)) {
			da->make_dir(cache_dir);
			return true;
		}
		return false;
	}

	bool _require_packages_dir() {
		bool is_new = false;
		String plugins_dir = DIR_PLUGINS;
		auto proj_da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (!proj_da->dir_exists(plugins_dir)) {
			proj_da->make_dir(plugins_dir);
			is_new = true;
		}

		auto manifest_path = PATH_JOIN(plugins_dir, FILE_MANIFEST);
		if (!proj_da->file_exists(manifest_path)) {
			String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();
			String def_manifest = PATH_JOIN(PATH_JOIN(exe_path, "Packages"), FILE_MANIFEST);
			if (FileAccess::exists(def_manifest)) {
				DirAccess::copy_absolute(def_manifest, manifest_path);
			} else {
				auto fa = FileAccess::open(manifest_path, FileAccess::WRITE);
				fa->store_line("[" SECTION_PLUGINS "]");
			}
			is_new = true;
		}

		return is_new;
	}

	bool _get_current_task_step(String &state, int &step);

	void _fetch_source_progress();

protected:
	Map<String, PackageInfo> package_info_map;

	String _get_plugin_path(const String &p_name_path) const {
		String plugin_path = p_name_path;

		if (!plugin_path.begins_with("res://")) {
			plugin_path = NAME_TO_PLUGIN_PATH(plugin_path);
		}
		return plugin_path;
	}

	String _get_package_name(const String &p_name_path) const {
		String package_name = p_name_path;
		if (package_name.begins_with("res://")) {
			package_name = package_name.get_base_dir().get_file();
		}
		return package_name;
	}

	void _scan_packages() {
		_require_packages_dir();
		if (_update_config() && !_thread_working.is_set()) {
			update_packages();
		}
	}

	void _set_load_err_info(const String plugin_name, Error err, const String &err_text);
	void _set_unload_error(const String &p_error);

	void _notification(int p_notification);

	bool _update_config();
	void _deactive_expire_plugins();
	void _active_plugins(int n);
	void _preload_fs(const EditorFileSystemDirectory *p_fs);
	//void _filesystem_changed();
	void _sources_changed(bool exist);
	void _resources_changed(const Vector<String> &p_resources);
	Error _handle_packages_source();
	Error _delete_package_folder(const String &folder_path);
	void _remove_packages_from_disk(const PackedStringArray &path);
	void _update_autoload_from_disk(const String &path_info);

	void _source_handler();
	static uint64_t get_modified_time(const String &dir_path);
	static void _bind_methods();

	void _set_package_plugin_enabled(const String &p_name_path, bool p_enabled);

public:
	EditorPackageSystem();
	~EditorPackageSystem();

	void _enable_plugin(const String &p_plugin_path, String pkg_name = String()) {
		if (FileAccess::exists(p_plugin_path)) {
			if (IS_EMPTY(pkg_name)) {
				pkg_name = p_plugin_path.get_base_dir().get_file();
			}

			_set_package_plugin_enabled(p_plugin_path, true);

			bool success = _enabled_plugins_map.has(p_plugin_path);
			ED_MSG(STTR("Active plugin: ") + "'%s' %s", pkg_name, success ? STTR("success") : STTR("fail"));
		}
	}

	void _disable_plugin(const String &p_plugin_path, String pkg_name = String()) {
		if (FileAccess::exists(p_plugin_path)) {
			_set_package_plugin_enabled(p_plugin_path, false);
			if (IS_EMPTY(pkg_name)) {
				pkg_name = p_plugin_path.get_base_dir().get_file();
			}
			ED_MSG(STTR("Deactive plugin: ") + "%s", pkg_name);
		}
	}

	void update_packages(const String &p_reset = String());
	PackageStatus get_package_status(const String &p_name_path);
	bool is_package_plugin_enabled(const String &p_name_path);
	void set_package_plugin_enabled(const String &p_name_path, const bool p_enabled);
	void install_package(const String &p_package, const Variant &p_value);
	void uninstall_package(const String &p_package);
	String get_package_source(const String &p_package);
	String get_package_version(const String &p_package);

	static EditorPackageSystem *get_singleton() { return singleton; }
};

VARIANT_ENUM_CAST(EditorPackageSystem::PackageStatus);
