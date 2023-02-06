/**
 * pck_loader.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_file_access.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/config/project_settings.h"
#include "pck_loader.h"

bool PckLoader::load_pack_with_key(const String &p_pack, const String &p_key, bool p_replace_files, int p_offset) {
	bool ok = SpikePackSource::get_singleton()->try_open_pack(p_pack, p_key, p_replace_files, p_offset);
	if (!ok) {
		return false;
	}
	SpikePackSource::get_singleton()->add_uids_from_cache(p_key);
	//if data.pck is found, all directory access will be from here
	DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);

	return true;
}

bool PckLoader::load_pack(const String &p_pack, bool p_replace_files, int p_offset) {
	bool ok = SpikePackSource::get_singleton()->try_open_pack(p_pack, p_replace_files, p_offset);
	if (!ok) {
		return false;
	}
	//if data.pck is found, all directory access will be from here
	DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);

	return true;
}

void PckLoader::_bind_methods() {
	ClassDB::bind_static_method("PckLoader", D_METHOD("load_pack_with_key", "pack", "key", "replace_files", "offset"), &PckLoader::load_pack_with_key, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_static_method("PckLoader", D_METHOD("load_pack", "pack", "replace_files", "offset"), &PckLoader::load_pack, DEFVAL(true), DEFVAL(0));
}