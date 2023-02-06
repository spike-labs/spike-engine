/**
 * spike_format_loader.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "spike_define.h"
#include "core/io/config_file.h"
#include "core/io/resource_loader.h"
#include "editor/editor_file_system.h"

#define FILE_UID ".uids"
#define SECTION_UID "UIDS"

class SpikeFormatLoader : public ResourceFormatLoader {
public:
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override;
	static bool get_uid(const String &p_path, ResourceUID::ID &r_uid);

	/// @brief Load uids config for the specific file.
	/// @param[in] p_path
	/// @param[out] uids_path
	/// @return ConfigFile
	static Ref<ConfigFile> load_uids_for_path(const String &p_path, String &r_uids_path);

	static void update_uids_for_filesystem(const EditorFileSystemDirectory *p_dir);
};
