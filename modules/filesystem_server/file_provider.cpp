/**
 * file_provider.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "file_provider.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/ref_counted.h"
#include "filesystem_server/file_provider.h"
#include "filesystem_server/filesystem_server.h"

Ref<FileAccess> FileProvider::_open(const String &p_path, FileAccess::ModeFlags p_mode_flags) const {
	Error err = OK;
	Ref<FileAccess> f = open(p_path, p_mode_flags, &err);
	if (err) {
		return Ref<FileAccess>();
	}
	return f;
}

Ref<Resource> FileProvider::load(const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) const {
	return FileSystemServer::get_singleton()->load_from_provider(Ref<FileProvider>(this), p_path, p_type_hint, p_cache_mode, r_error);
}

Ref<Resource> FileProvider::_load(const String &p_path, const String &p_type_hint, core_bind::ResourceLoader::CacheMode p_cache_mode) const {
	Error err = OK;
	Ref<Resource> res = load(p_path, p_type_hint, ResourceFormatLoader::CacheMode(p_cache_mode), &err);
	if (err) {
		return Ref<Resource>();
	}
	return res;
}

String FileProvider::get_resource_type(const String &p_path) const {
	return FileSystemServer::get_singleton()->get_resource_type(p_path, Ref<FileProvider>(this));
}

void FileProvider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "flags"), &FileProvider::_open);
	ClassDB::bind_method(D_METHOD("load", "path", "type_hint", "cache_mode"), &FileProvider::_load, DEFVAL(""), DEFVAL(core_bind::ResourceLoader::CACHE_MODE_IGNORE));
	ClassDB::bind_method(D_METHOD("get_files"), &FileProvider::get_files);
	ClassDB::bind_method(D_METHOD("has_file", "path"), &FileProvider::has_file);
	ClassDB::bind_method(D_METHOD("get_source"), &FileProvider::get_source);
}