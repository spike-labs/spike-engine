/**
 * modular_override_utils.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/config_file.h"

#define MODULAR_REMAP_FILES ".remaps"
#define MODULAR_REMAP_PATH_KEY "path"
#define MODULAR_REMAP_PACKED_FILE_KEY "packed_file"
#define MODULAR_REMAP_ORIGIN_FILE_KEY "origin_file"
#define MODULAR_REMAP_EXTENDS_KEY "extends"

#define MODULAR_COMPOSITE_FILE_REMAPPER_META_NAME SNAME("_ModularCompositeFileRemapper_")
#define MODIFY_TIME_META_NAME SNAME("_modify_time_")
#define SAVE_PATH_META_NAME SNAME("_save_path_")

class ModularOverrideUtils {
public:
	static Ref<class FileProviderRemap> get_or_create_provider_remapper_with_caching();
	static Ref<class FileProviderPack> get_or_create_packed_file_provider_with_caching(const String &p_packed_file, bool *r_is_new = nullptr);

	static inline Ref<ConfigFile> get_loaded_remap_config(const String &p_folder_path, Ref<ConfigFile> r_conf = Variant());
	static inline Error save_remap_config(Ref<ConfigFile> &p_remap_config, const String &p_save_path = {});

	static bool try_override_script_from_packed_file(const String &p_override_file, const String &p_packed_file, const String &p_file_path, const String &p_modular_override_folder);

	static void scan_and_remap_override_scripts_path(const Array &p_resources);

	static void trace_file_path_change(const String &p_old_file, const String &p_new_file = "");

private:
	ModularOverrideUtils() = default;
};

Ref<ConfigFile> ModularOverrideUtils::get_loaded_remap_config(const String &p_folder_path, Ref<ConfigFile> r_conf) {
	if (r_conf.is_null()) {
		r_conf.instantiate();
	}
	auto save_path = p_folder_path.path_join(MODULAR_REMAP_FILES);
	if (FileAccess::exists(save_path)) {
		r_conf->load(save_path);
	}
	r_conf->set_meta(SAVE_PATH_META_NAME, save_path);
	return r_conf;
}

Error ModularOverrideUtils::save_remap_config(Ref<ConfigFile> &p_remap_config, const String &p_save_path) {
	if (p_remap_config.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_save_path.is_empty()) {
		String save_path = p_remap_config->get_meta(SAVE_PATH_META_NAME, "");
		return p_remap_config->save(save_path);
	} else {
		return p_remap_config->save(p_save_path);
	}
}
