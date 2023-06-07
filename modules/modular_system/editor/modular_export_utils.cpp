/**
 * modular_export_utils.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "modular_export_utils.h"

#include "annotation/annotation_export_plugin.h"
#include "core/config/project_settings.h"
#include "editor/editor_file_system.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "modular_export_plugin.h"
#include "modular_override_utils.h"
#include "package_system/editor/editor_package_system.h"

#ifdef WINDOWS_ENABLED
#include "platform/windows/export/export_plugin.h"
#elif defined(UWP_ENABLED)
#include "platform/uwp/export/export_plugin.h"
#elif defined(MACOS_ENABLED)
#include "platform/macos/export/export_plugin.h"
#elif defined(UNIX_ENABLED)
#include "platform/linuxbsd/export/export_plugin.h"
#endif

#define MODULAR_GLOBAL_CLASS_LIST_PATH PATH_JOIN(EDITOR_CACHE, "_modular_global_class_list.cfg")

#define PROVIDER_REMAP_SPLITTER "||"
#define PROVIDER_REMAP_TEMPLATE "%s" PROVIDER_REMAP_SPLITTER "%s"

static void load_remap_resources(EditorFileSystemDirectory *p_fs,
		Vector<Ref<Resource>> &r_resources,
		PackedStringArray &r_files,
		Map<String, String> &r_remap) {
	String fs_path = p_fs->get_path();
	auto remap_conf = ModularOverrideUtils::get_loaded_remap_config(fs_path);

	for (int i = 0; i < p_fs->get_file_count(); i++) {
		auto file_path = PATH_JOIN(fs_path, p_fs->get_file(i));
		auto file_name = file_path.get_file();
		String remap_path = file_path;
		if (remap_conf->has_section_key(file_name, MODULAR_REMAP_PATH_KEY)) {
			remap_path = remap_conf->get_value(file_name, MODULAR_REMAP_PATH_KEY);
			r_remap.insert(remap_path, file_path);
		}

		if (remap_conf->has_section_key(file_name, MODULAR_REMAP_PACKED_FILE_KEY) &&
				remap_conf->has_section_key(file_name, MODULAR_REMAP_ORIGIN_FILE_KEY) &&
				remap_conf->has_section_key(file_name, MODULAR_REMAP_EXTENDS_KEY)) {
			String packed_file_path = remap_conf->get_value(file_name, MODULAR_REMAP_PACKED_FILE_KEY);
			String origin_file_path = remap_conf->get_value(file_name, MODULAR_REMAP_ORIGIN_FILE_KEY);
			String target_remaped_extends_file_path = remap_conf->get_value(file_name, MODULAR_REMAP_EXTENDS_KEY);

			String provider_remap = vformat(PROVIDER_REMAP_TEMPLATE, packed_file_path, origin_file_path);
			r_remap.insert(target_remaped_extends_file_path, provider_remap);
			r_files.push_back(provider_remap);
		}
	}

	for (int i = 0; i < p_fs->get_subdir_count(); i++) {
		load_remap_resources(p_fs->get_subdir(i), r_resources, r_files, r_remap);
	}
}

static void _find_modular_dependencies(const String &p_path, HashSet<String> &p_paths) {
	if (p_paths.has(p_path)) {
		return;
	}

	p_paths.insert(p_path);

	EditorFileSystemDirectory *dir;
	int file_idx;
	dir = EditorFileSystem::get_singleton()->find_file(p_path, &file_idx);
	if (!dir) {
		return;
	}

	Vector<String> deps = dir->get_file_deps(file_idx);

	for (int i = 0; i < deps.size(); i++) {
		_find_modular_dependencies(deps[i], p_paths);
	}
}

static void export_modular_data(
		const Ref<PackedScene> &p_main_scene,
		const Vector<Ref<Resource>> &p_res_list,
		const PackedStringArray &p_file_list,
		const Map<String, String> &p_remap,
		bool p_exclude_package_files,
		const String &p_save_path) {
#define EXPORT_MODULAR_ENABLED
#ifdef WINDOWS_ENABLED
	Ref<EditorExportPlatformWindows> export_platform;
#elif defined(UWP_ENABLED)
	Ref<EditorExportPlatformUWP> export_platform;
#elif defined(MACOS_ENABLED)
	Ref<EditorExportPlatformMacOS> export_platform;
#elif defined(UNIX_ENABLED)
	Ref<EditorExportPlatformLinuxBSD> export_platform;
#else
#undef EXPORT_MODULAR_ENABLED
#endif

#ifdef EXPORT_MODULAR_ENABLED
	REF_INSTANTIATE(export_platform);
	// Caches project settings that will not be exported during modular export.
	Map<String, Variant> cache_settigns;
	cache_settigns.insert("application/run/main_scene", p_main_scene.is_valid() ? p_main_scene->get_path() : String());
	cache_settigns.insert("application/config/icon", String());
	cache_settigns.insert("application/boot_splash/image", String());

	// Get autoload mapping
	Map<String, String> autoload_map;
	for (auto &kv : ProjectSettings::get_singleton()->get_autoload_list()) {
		autoload_map.insert(kv.value.path, kv.value.name);
	}

	// Get all global classes in project
	auto list = ProjectSettings::get_singleton()->get_global_class_list();
	HashMap<String, Dictionary> global_class_map;
	for (int i = 0; i < list.size(); ++i) {
		Dictionary item = list[i];
		global_class_map.insert(item["path"], item);
	}

	auto export_plugin = ModularExportPlugin::get_singleton();
	Ref<EditorExportPreset> export_preset = export_platform->create_preset();

// FileProviderPack still not support encrypted packed file.
#define MODULAR_SCRIPT_AND_DIR_ENCRYPT_DISABLED
#ifndef MODULAR_SCRIPT_AND_DIR_ENCRYPT_DISABLED
	export_preset->set_enc_pck(true);
	export_preset->set_script_encryption_key(String::utf8(reinterpret_cast<const char *>(&script_encryption_key[0]), 32));
	export_preset->set_enc_directory(true);
#endif

	Map<ResourceUID::ID, String> uid_map;
	TypedArray<Dictionary> global_class_list;
	Set<Ref<Script>> export_plugin_scripts;
	export_preset->set_export_filter(EditorExportPreset::EXPORT_SELECTED_RESOURCES);
	for (int i = 0; i < p_res_list.size(); i++) {
		auto res = p_res_list[i];
		auto res_path = res->get_path();
		Set<String> export_files;
		_find_modular_dependencies(res_path, export_files);
		for (auto &file : export_files) {
			const String *remap_ptr = p_remap.getptr(file);
			String remap_file = remap_ptr ? *remap_ptr : file;
			if (!p_exclude_package_files || !remap_file.begins_with(DIR_PLUGINS)) {
				export_plugin->add_export_file(remap_file);
				export_preset->add_export_file(remap_file);
				auto uid = ResourceLoader::get_resource_uid(file);
				if (uid == ResourceUID::INVALID_ID) {
					// "remap_file" is cache file and can't get it id( in hidden path);
					uid = ResourceUID::get_singleton()->create_id();
				}
				uid_map.insert(uid, file);

				if (auto class_ptr = global_class_map.getptr(file)) {
					global_class_list.push_back(*class_ptr);
				}

				autoload_map.erase(file);
			}
		}

		Ref<Script> script = res;
		if (script.is_valid() && script->get_instance_base_type() == EditorExportPlugin::get_class_static()) {
			export_plugin_scripts.insert(script);
		}
	}

	for (auto &kv : autoload_map) {
		cache_settigns.insert("autoload/" + kv.value, Variant());
	}

	for (auto &kv : cache_settigns) {
		auto value = kv.value;
		kv.value = GLOBAL_GET(kv.key);
		ProjectSettings::get_singleton()->set(kv.key, value);
	}

	{
		// Generate global classes apear in selected files.
		Ref<ConfigFile> global_classes_conf;
		REF_INSTANTIATE(global_classes_conf);
		global_classes_conf->set_value("", "list", global_class_list);
		global_classes_conf->save(MODULAR_GLOBAL_CLASS_LIST_PATH);
	}

	for (auto &file : p_file_list) {
		const String *remap_ptr = p_remap.getptr(file);
		String remap_file = remap_ptr ? *remap_ptr : file;
		if (!p_exclude_package_files || !remap_file.begins_with(DIR_PLUGINS)) {
			export_plugin->add_export_file(remap_file);
			export_preset->add_export_file(remap_file);
		}
	}

	// Save `uid_cache.bin` to temporary
	String resource_cache_file = ResourceUID::get_cache_file();
	String tmp_cache_file;
	if (FileAccess::exists(resource_cache_file)) {
		tmp_cache_file = resource_cache_file + ".bak";
		DirAccess::rename_absolute(resource_cache_file, tmp_cache_file);
	}

	ResourceUID::get_singleton()->clear();
	for (auto it = uid_map.begin(); it; ++it) {
		ResourceUID::get_singleton()->add_id(it->key, it->value);
	}
	ResourceUID::get_singleton()->save_to_cache();

	// Get all `EditorExportPlugin` and exclude the built-ins(No script attached).
	auto custom_export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = custom_export_plugins.size() - 1; i >= 0; i--) {
		auto plugin = custom_export_plugins[i];
		if (plugin->get_script().is_null()) {
			custom_export_plugins.remove_at(i);
		}
	}

	// Get all `EditorExportPlugin` inside [param p_res_list].
	Vector<Ref<EditorExportPlugin>> temp_export_plugins;
	temp_export_plugins.push_back(AnnotationExportPlugin::create_annotaion_export_plugin());
	temp_export_plugins.push_back(export_plugin);
	for (auto &item : export_plugin_scripts) {
		Ref<Script> script = item;
		Ref<EditorExportPlugin> plugin = script->call("new");
		if (plugin.is_valid()) {
			temp_export_plugins.push_back(plugin);
		}
	}

	for (const auto &plugin : custom_export_plugins) {
		EditorExport::get_singleton()->remove_export_plugin(plugin);
	}
	for (auto &plugin : temp_export_plugins) {
		EditorExport::get_singleton()->add_export_plugin(plugin);
	}

	if (p_save_path.get_extension() == "pck") {
		export_platform->export_pack(export_preset, false, p_save_path);
	} else {
		export_platform->export_zip(export_preset, false, p_save_path);
	}

	export_plugin->clear_files();

	for (auto &plugin : temp_export_plugins) {
		EditorExport::get_singleton()->remove_export_plugin(plugin);
	}
	for (const auto &plugin : custom_export_plugins) {
		EditorExport::get_singleton()->add_export_plugin(plugin);
	}

	// Restore Project
	if (!IS_EMPTY(tmp_cache_file)) {
		DirAccess::rename_absolute(tmp_cache_file, resource_cache_file);
		ResourceUID::get_singleton()->clear();
		ResourceUID::get_singleton()->load_from_cache();
	}
	for (auto &kv : cache_settigns) {
		ProjectSettings::get_singleton()->set_setting(kv.key, kv.value);
	}
#else
	ERR_PRINT("Export Modular is not support on this platform.");
	return String();
#endif
}

void ModularExportUtils::collect_resources(EditorFileSystemDirectory *p_fs, Vector<Ref<Resource>> &r_res_list, Vector<String> &r_file_list) {
	if (p_fs == nullptr)
		return;

	String fs_path = p_fs->get_path();
	for (int i = 0; i < p_fs->get_file_count(); ++i) {
		auto file_path = PATH_JOIN(fs_path, p_fs->get_file(i));
		if (!IS_EMPTY(ResourceLoader::get_resource_type(file_path))) {
			auto res = ResourceLoader::load(file_path);
			if (res.is_valid()) {
				r_res_list.push_back(res);
			}
		} else {
			r_file_list.push_back(file_path);
		}
	}

	for (int i = 0; i < p_fs->get_subdir_count(); ++i) {
		collect_resources(p_fs->get_subdir(i), r_res_list, r_file_list);
	}
}

String ModularExportUtils::export_specific_resources(
		const Ref<PackedScene> &p_main_scene,
		const Vector<Ref<Resource>> &p_res_list,
		const Vector<String> &p_file_list,
		bool p_exclude_package_files,
		int p_ext) {
	Vector<Ref<Resource>> res_list;
	PackedStringArray file_list = p_file_list;
	Map<String, String> remap;

	Set<String> folders;
	for (auto &res : p_res_list) {
		if (res.is_valid()) {
			folders.insert(res->get_path().get_base_dir());
		}
	}

	for (const auto &path : folders) {
		if (auto fs = EditorFileSystem::get_singleton()->get_filesystem_path(path)) {
			load_remap_resources(fs, res_list, file_list, remap);
		}
	}

	// Remap global class list file
	String global_class_list_path = ProjectSettings::get_singleton()->get_global_class_list_path();
	file_list.push_back(global_class_list_path);
	remap.insert(global_class_list_path, MODULAR_GLOBAL_CLASS_LIST_PATH);

	Map<String, String> swap_remap;
	Ref<ModularFileProvider> provider;
	REF_INSTANTIATE(provider);
	for (const auto &kv : remap) {
		auto splited = kv.value.split(PROVIDER_REMAP_SPLITTER, false, 1);
		if (splited.size() == 2) {
			provider->insert_path_packed_file(kv.key, splited[0], splited[1]);
		} else {
			provider->insert_path_file_system(kv.key, kv.value);
		}
		swap_remap.insert(kv.value, kv.key);
	}

	// Merge "res_list" and "p_res_list".
	HashSet<String> res_paths;
	for (const auto res : res_list) {
		res_paths.insert(res->get_path());
	}

	for (const auto res : p_res_list) {
		if (res_paths.has(res->get_path())) {
			continue;
		}

		res_list.push_back(res);
	}

	auto fss = FileSystemServer::get_singleton();
	fss->push_current_provider(provider);
	String save_path = p_ext == 0 ? PATH_JOIN(EDITOR_CACHE, "_modular_resources.pck") : PATH_JOIN(EDITOR_CACHE, "_modular_resources.zip");
	export_modular_data(p_main_scene, res_list, file_list, swap_remap, p_exclude_package_files, save_path);
	fss->pop_current_provider();

	return save_path;
}
