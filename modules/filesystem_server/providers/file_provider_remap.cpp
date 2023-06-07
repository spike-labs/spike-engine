/**
 * file_provider_remap.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "file_provider_remap.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "filesystem_server/filesystem_server.h"

void FileProviderRemap::add_remap(const String &p_path, const String &p_to) {
	add_provider_remap(p_path, Ref<FileProvider>(), p_to);
}

void FileProviderRemap::add_provider_remap(const String &p_path, const Ref<FileProvider> &p_provider, const String &p_to) {
	remaps[p_path] = Remap{ p_path, p_to, p_provider };
}

bool FileProviderRemap::has_remap(const String &p_path) const {
	return remaps.has(p_path);
}

bool FileProviderRemap::remove_remap(const String &p_from) {
	return remaps.erase(p_from);
}

Ref<FileAccess> FileProviderRemap::open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	Ref<FileAccess> ret;
	if (remaps.has(p_path)) {
		auto remap = remaps[p_path];
		if (remap.provider.is_valid()) {
			ret = remap.provider->open(remap.to, p_mode_flags, r_error);
		} else {
			ret = FileSystemServer::get_singleton()->open(remap.to, p_mode_flags, r_error);
		}
	}
	return ret;
}

PackedStringArray FileProviderRemap::get_files() const {
	PackedStringArray ret;
	for (auto remap : remaps) {
		ret.push_back(remap.key);
	}
	return ret;
}

bool FileProviderRemap::has_file(const String &p_path) const {
	return remaps.has(p_path);
}

void FileProviderRemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_remap", "path", "to"), &FileProviderRemap::add_remap);
	ClassDB::bind_method(D_METHOD("add_provider_remap", "path", "provider", "to"), &FileProviderRemap::add_provider_remap);
	ClassDB::bind_method(D_METHOD("has_remap", "path"), &FileProviderRemap::has_remap);
	ClassDB::bind_method(D_METHOD("remove_remap", "path"), &FileProviderRemap::remove_remap);
}

FileProviderRemap::FileProviderRemap() {
}

FileProviderRemap::~FileProviderRemap() {
}
