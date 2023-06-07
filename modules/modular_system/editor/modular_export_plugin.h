/**
 * modular_export_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

/*****************************************************************************
 * MUST include this file before "editor/export/editor_export_plugin.h",
 * cause forward declaration of 'EditorExportPlatform' inside that file.
 */
#include "editor/export/editor_export_platform.h"
/*****************************************************************************/

#include "editor/export/editor_export_plugin.h"
#include "filesystem_server/file_provider.h"

class ModularExportPlugin : public EditorExportPlugin {
	GDCLASS(ModularExportPlugin, EditorExportPlugin);

protected:
	static ModularExportPlugin *singleton;
	virtual String _get_name() const override;
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;

	HashSet<String> export_files;

public:
	void clear_files() { export_files.clear(); }
	void add_export_file(const String &p_file);
	static ModularExportPlugin *get_singleton() { return singleton; }

	ModularExportPlugin() {
		if (nullptr == singleton) {
			singleton = this;
		}
	};
};

class ModularFileProvider : public FileProvider {
	GDCLASS(ModularFileProvider, FileProvider);

	HashMap<String, String> path_remap_file_system;
	HashMap<String, Pair<String, String>> path_remap_packed_file;

public:
	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const override;
	void insert_path_file_system(const String &p_path, const String &p_real_path) { path_remap_file_system.insert(p_path, p_real_path); }
	void insert_path_packed_file(const String &p_path, const String &p_packed_file_path, const String &p_path_in_packed_file) { path_remap_packed_file.insert(p_path, { p_packed_file_path, p_path_in_packed_file }); }
	void erase_path(const String &p_path) {
		path_remap_file_system.erase(p_path);
		path_remap_packed_file.erase(p_path);
	}
	virtual PackedStringArray get_files() const override;
	virtual bool has_file(const String &p_path) const override { return path_remap_file_system.has(p_path) || path_remap_packed_file.has(p_path); }
};
