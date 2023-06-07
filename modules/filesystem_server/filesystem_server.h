/**
 * filesystem_server.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/file_access.h"
#include "file_provider.h"

class FileSystemServer : public Object {
	GDCLASS(FileSystemServer, Object);

protected:
	Vector<Ref<FileProvider>> provider_list;
	FileAccess::CreateFunc os_create_func;
	static FileSystemServer *singleton;

	thread_local static Error last_file_open_error;

	List<Ref<FileProvider>> current_provider_stack;
	Ref<FileProvider> current_provider;

public:
	Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const;
	Ref<FileAccess> open_internal(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const;
	Ref<FileAccess> _open(const String &p_path, FileAccess::ModeFlags p_mode_flags) const;

	String get_resource_type(const String &p_path, const Ref<FileProvider> &p_provider = Ref<FileProvider>());
	Ref<Resource> load(const String &p_path, const String &p_type_hint = "", ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE, Error *r_error = nullptr);
	Ref<Resource> load_from_provider(const Ref<FileProvider> &p_provider, const String &p_path, const String &p_type_hint = "", ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE, Error *r_error = nullptr);

	bool file_exists(const String &p_name);
	uint64_t get_modified_time(const String &p_file);

	Ref<FileAccess> create_for_path(const String &p_path);

	int add_provider(const Ref<FileProvider> &p_provider);
	void remove_provider(const Ref<FileProvider> &p_provider);
	int get_provider_count() const;
	Ref<FileProvider> get_provider(int p_index) const;
	Ref<FileProvider> get_provider_by_source(const String &p_source) const;
	bool has_provider(const Ref<FileProvider> &p_provider) const;
	void move_provider(const Ref<FileProvider> &p_provider, int p_to_index);

	Ref<FileAccess> get_os_file_access() const;

	void push_current_provider(const Ref<FileProvider> &p_provider);
	void pop_current_provider();

public:
	static void _bind_methods();
	_FORCE_INLINE_ static FileSystemServer *get_singleton() { return singleton; }

	FileSystemServer();
	~FileSystemServer();
};

class FileProviderDefault : public FileProvider {
	GDCLASS(FileProviderDefault, FileProvider);

public:
	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const override {
		return FileSystemServer::get_singleton()->open_internal(p_path, p_mode_flags, r_error);
	}
	virtual bool has_file(const String &p_path) const override { return false; }
};
