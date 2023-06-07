/**
 * filesystem_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "filesystem_server.h"
#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/resource.h"
#include "file_access_router.h"
#include "filesystem_server/filesystem_server.h"

thread_local Error FileSystemServer::last_file_open_error = OK;

FileSystemServer *FileSystemServer::singleton = nullptr;

Ref<FileAccess> FileSystemServer::open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	Ref<FileAccess> f;

	if (current_provider.is_valid()) {
		return current_provider->open(p_path, p_mode_flags, r_error);
	}

	if (PackedData::get_singleton()->is_disabled()) {
		f = open_internal(p_path, p_mode_flags, r_error);
		if (f.is_valid()) {
			return f;
		}
	}

	for (int i = 0; i < provider_list.size(); i++) {
		f = provider_list[i]->open(p_path, p_mode_flags, r_error);
		if (f.is_valid()) {
			return f;
		}
	}

	if (!PackedData::get_singleton()->is_disabled()) {
		f = open_internal(p_path, p_mode_flags, r_error);
		if (f.is_valid()) {
			return f;
		}
	}

	return Ref<FileAccess>();
}

Ref<FileAccess> FileSystemServer::open_internal(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	auto f = get_os_file_access();

	Error err = f->reopen(p_path, p_mode_flags);
	if (r_error) {
		*r_error = err;
	}
	if (err) {
		return Ref<FileAccess>();
	}
	return f;
}

Ref<FileAccess> FileSystemServer::_open(const String &p_path, FileAccess::ModeFlags p_mode_flags) const {
	Error err = OK;
	Ref<FileAccess> f = open(p_path, p_mode_flags, &err);
	last_file_open_error = err;
	if (err) {
		return Ref<FileAccess>();
	}
	return f;
}

static String __validate_local_path(const String &p_path) {
	ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(p_path);
	if (uid != ResourceUID::INVALID_ID) {
		return ResourceUID::get_singleton()->get_id_path(uid);
	} else if (p_path.is_relative_path()) {
		return "res://" + p_path;
	} else {
		return ProjectSettings::get_singleton()->localize_path(p_path);
	}
}

String FileSystemServer::get_resource_type(const String &p_path, const Ref<FileProvider> &p_provider) {
	push_current_provider(p_provider);
	String local_path = __validate_local_path(p_path);
	bool xl_remapped = false;
	String path = ResourceLoader::_path_remap(local_path, &xl_remapped);
	String res_type = ResourceLoader::get_resource_type(path);
	pop_current_provider();
	return res_type;
}

Ref<Resource> FileSystemServer::load(const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) {
	if (r_error) {
		*r_error = OK;
	}

	String local_path = __validate_local_path(p_path);
	bool xl_remapped = false;
	String path = ResourceLoader::_path_remap(local_path, &xl_remapped);

	if (path.is_empty()) {
		ERR_FAIL_V_MSG(Ref<Resource>(), "Remapping '" + local_path + "' failed.");
	}

	print_verbose("Loading resource: " + path);
	float p;
	Ref<Resource> res = ResourceLoader::_load(path, local_path, p_type_hint, p_cache_mode, r_error, false, &p);

	if (res.is_null()) {
		print_verbose("Failed loading resource: " + path);
		return Ref<Resource>();
	}

	if (xl_remapped) {
		res->set_as_translation_remapped(true);
	}
	return res;
}

Ref<Resource> FileSystemServer::load_from_provider(const Ref<FileProvider> &p_provider, const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) {
	Ref<Resource> ret;

	if (current_provider == p_provider) {
		ret = load(p_path, p_type_hint, p_cache_mode, r_error);
	} else {
		push_current_provider(p_provider);
		ret = load(p_path, p_type_hint, p_cache_mode, r_error);
		pop_current_provider();
	}
	return ret;
}

bool FileSystemServer::file_exists(const String &p_name) {
	for (int i = 0; i < provider_list.size(); i++) {
		// if (provider_list[i]->file_exists(p_name)) {
		// 	return true;
		// }
	}

	Ref<FileAccess> file_access = get_os_file_access();
	return file_access->file_exists(p_name);
}

uint64_t FileSystemServer::get_modified_time(const String &p_file) {
	for (int i = 0; i < provider_list.size(); i++) {
		// uint64_t time = provider_list[i]->get_modified_time(p_file);
		// if (time != 0) {
		// 	return time;
		// }
	}

	Ref<FileAccess> file_access = get_os_file_access();
	return file_access->_get_modified_time(p_file);
}

Ref<FileAccess> create_for_path(const String &p_path) {
	Ref<FileAccess> ret;
	if (p_path.begins_with("res://")) {
		ret = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	} else if (p_path.begins_with("user://")) {
		ret = FileAccess::create(FileAccess::ACCESS_USERDATA);
	} else {
		ret = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
	}

	return ret;
}

int FileSystemServer::add_provider(const Ref<FileProvider> &p_provider) {
	for (auto &&added_provider : provider_list) {
		ERR_FAIL_COND_V(added_provider == p_provider, -1);
		ERR_FAIL_COND_V(!p_provider->get_source().is_empty() && added_provider->get_source() == p_provider->get_source(), -1);
	}
	provider_list.push_back(p_provider);

	const_cast<FileProvider *>(p_provider.ptr())->on_added_to_filesystem_server();
	return provider_list.size() - 1;
}

void FileSystemServer::remove_provider(const Ref<FileProvider> &p_provider) {
	provider_list.erase(p_provider);
	if (p_provider.is_valid()) {
		const_cast<FileProvider *>(p_provider.ptr())->on_removed_to_filesystem_server();
	}
}

int FileSystemServer::get_provider_count() const {
	return provider_list.size();
}

Ref<FileProvider> FileSystemServer::get_provider(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, provider_list.size(), Ref<FileProvider>());
	return provider_list[p_index];
}

Ref<FileProvider> FileSystemServer::get_provider_by_source(const String &p_source) const {
	for (auto &&provider : provider_list) {
		if (provider->get_source() == p_source) {
			return provider;
		}
	}
	return {};
}

bool FileSystemServer::has_provider(const Ref<FileProvider> &p_provider) const {
	return provider_list.has(p_provider);
}

void FileSystemServer::move_provider(const Ref<FileProvider> &p_provider, int p_to_index) {
	int from = provider_list.find(p_provider);
	ERR_FAIL_COND_MSG(from == -1, "Provider not found.");
	ERR_FAIL_INDEX_MSG(p_to_index, provider_list.size(), "Invalid index.");
	provider_list.remove_at(from);
	provider_list.insert(p_to_index, p_provider);
}

Ref<FileAccess> FileSystemServer::get_os_file_access() const {
	Ref<FileAccess> f = os_create_func();
	f->_set_access_type(FileAccess::ACCESS_RESOURCES);
	return f;
}

void FileSystemServer::push_current_provider(const Ref<FileProvider> &p_provider) {
	if (current_provider.is_valid()) {
		current_provider_stack.push_back(current_provider);
	}
	current_provider = p_provider;
}

void FileSystemServer::pop_current_provider() {
	if (current_provider_stack.is_empty()) {
		current_provider = Ref<FileProvider>();
	} else {
		current_provider = current_provider_stack.back()->get();
		current_provider_stack.pop_back();
	}
}

void FileSystemServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "mode_flags"), &FileSystemServer::_open);
	ClassDB::bind_method(D_METHOD("file_exists", "path"), &FileSystemServer::file_exists);
	ClassDB::bind_method(D_METHOD("add_provider", "provider"), &FileSystemServer::add_provider);
	ClassDB::bind_method(D_METHOD("remove_provider", "provider"), &FileSystemServer::remove_provider);
	ClassDB::bind_method(D_METHOD("get_provider_count"), &FileSystemServer::get_provider_count);
	ClassDB::bind_method(D_METHOD("get_provider", "index"), &FileSystemServer::get_provider);
	ClassDB::bind_method(D_METHOD("has_provider", "provider"), &FileSystemServer::has_provider);
	ClassDB::bind_method(D_METHOD("move_provider", "provider", "to_index"), &FileSystemServer::move_provider);
	ClassDB::bind_method(D_METHOD("push_current_provider", "provider"), &FileSystemServer::push_current_provider);
	ClassDB::bind_method(D_METHOD("pop_current_provider"), &FileSystemServer::pop_current_provider);
}

FileSystemServer::FileSystemServer() {
	singleton = this;

	os_create_func = FileAccess::get_create_func(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessRouter>(FileAccess::ACCESS_RESOURCES);
}

FileSystemServer::~FileSystemServer() {
	singleton = nullptr;
}