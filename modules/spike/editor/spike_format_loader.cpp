/**
 * spike_format_loader.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_format_loader.h"
#include "editor/editor_file_system.h"

ResourceUID::ID SpikeFormatLoader::get_resource_uid(const String &p_path) const {
	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	if (get_uid(p_path, uid)) {
		ResourceUID::get_singleton()->add_id(uid, p_path);
	}
	return uid;
}

bool SpikeFormatLoader::get_uid(const String &p_path, ResourceUID::ID &r_uid) {
	bool is_created = false;
	r_uid = ResourceUID::INVALID_ID;

	if (!FileAccess::exists(p_path)) {
		return false;
	}

	ResourceUID::ID cache_uid = ResourceUID::INVALID_ID, save_uid = ResourceUID::INVALID_ID;

	// Seek uid from cache at first
	int pos;
	auto fs = EditorFileSystem::get_singleton()->find_file(p_path, &pos);
	if (fs && pos >= 0) {
		cache_uid = fs->files[pos]->uid;
	}

	// Seek uid from file or create a new one
	String uids_path;
	Ref<ConfigFile> cfg = load_uids_for_path(p_path, uids_path);
	String file_name = p_path.get_file();
	if (IS_EMPTY(file_name))
		return false;

	if (cfg->has_section_key(SECTION_UID, file_name)) {
		save_uid = ResourceUID::get_singleton()->text_to_id(cfg->get_value(SECTION_UID, file_name));
	}

	if (cache_uid != ResourceUID::INVALID_ID) {
		r_uid = cache_uid;
	} else if (save_uid != ResourceUID::INVALID_ID) {
		r_uid = save_uid;
	} else {
		r_uid = ResourceUID::get_singleton()->create_id();
		is_created = true;
	}

	if (r_uid != save_uid) {
		cfg->set_value(SECTION_UID, file_name, ResourceUID::get_singleton()->id_to_text(r_uid));
		cfg->save(uids_path);
	}

	return is_created;
}

Ref<ConfigFile> SpikeFormatLoader::load_uids_for_path(const String &p_path, String &r_uids_path) {
	Ref<ConfigFile> cfg;
	REF_INSTANTIATE(cfg);
	r_uids_path = p_path.get_base_dir().path_join(FILE_UID);
	auto fa = FileAccess::open(r_uids_path, FileAccess::READ);
	if (fa.is_valid()) {
		auto content = fa->get_as_text();
		if (!content.is_empty()) {
			cfg->parse(content);
		}
	}
	return cfg;
}

void SpikeFormatLoader::update_uids_for_filesystem(const EditorFileSystemDirectory *p_dir) {
	{
		Ref<ConfigFile> cfg;
		REF_INSTANTIATE(cfg);
		auto path = PATH_JOIN(p_dir->get_path(), FILE_UID);
		if (cfg->load(path) == OK && cfg->has_section(SECTION_UID)) {
			bool changed = false;
			List<String> keys;
			cfg->get_section_keys(SECTION_UID, &keys);
			for (auto k : keys) {
				if (p_dir->find_file_index(k) < 0) {
					cfg->erase_section_key(SECTION_UID, k);
					changed = true;
				}
			}
			if (changed) {
				cfg->save(path);
			}
		}
		if (!cfg->has_section(SECTION_UID) && FileAccess::exists(path)) {
			DirAccess::create(DirAccess::ACCESS_RESOURCES)->remove(path);
		}
	}

	for (size_t i = 0; i < p_dir->get_subdir_count(); i++) {
		update_uids_for_filesystem(const_cast<EditorFileSystemDirectory *>(p_dir)->get_subdir(i));
	}
}