/**
 * modular_export_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_export_plugin.h"
#include "editor/editor_file_system.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"

ModularExportPlugin *ModularExportPlugin::singleton = nullptr;

String ModularExportPlugin::_get_name() const {
	return "!!!ModularExport";
}

void ModularExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
}

void ModularExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	if (!export_files.has(p_path)) {
		skip();
	}
}

void ModularExportPlugin::add_export_file(const String &p_file) {
	export_files.insert(p_file);
}

Ref<FileAccess> ModularFileProvider::open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	auto f = FileSystemServer::get_singleton()->get_os_file_access();
	// Find remap from packed file first
	auto packed_file_to_real_path = path_remap_packed_file.getptr(p_path);
	if (packed_file_to_real_path) {
		const String packed_file = packed_file_to_real_path->first;
		const String path_in_packed_file = packed_file_to_real_path->second;
		Ref<FileProviderPack> provider = FileSystemServer::get_singleton()->get_provider_by_source(packed_file);
		if (provider.is_null()) {
			if (!f->file_exists(packed_file)) {
				*r_error = ERR_FILE_NOT_FOUND;
				return Ref<FileAccess>();
			}
			provider.instantiate();
			if (!provider->load_pack(packed_file)) {
				*r_error = ERR_FILE_UNRECOGNIZED;
				return Ref<FileAccess>();
			}
		}
		return provider->open(path_in_packed_file, p_mode_flags, r_error);
	}

	// Find from file system.
	auto path = p_path;
	auto real_path = path_remap_file_system.getptr(p_path);
	if (real_path) {
		path = *real_path;
	}

	Error err = f->reopen(path, p_mode_flags);
	if (r_error) {
		*r_error = err;
	}

	if (err == OK) {
		return f;
	}
	return Ref<FileAccess>();
}

PackedStringArray ModularFileProvider::get_files() const {
	PackedStringArray files;
	for (auto E : path_remap_file_system) {
		files.push_back(E.key);
	}
	for (auto E : path_remap_packed_file) {
		files.push_back(E.key);
	}
	return files;
}