/**
 * file_provider.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "spike_define.h"

#include "core/core_bind.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class FileProvider : public RefCounted {
	GDCLASS(FileProvider, RefCounted);

protected:
	static void _bind_methods();

	String source;

public:
	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const = 0;
	Ref<FileAccess> _open(const String &p_path, FileAccess::ModeFlags p_mode_flags) const;

	Ref<Resource> load(const String &p_path, const String &p_type_hint = "", ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE, Error *r_error = nullptr) const;
	Ref<Resource> _load(const String &p_path, const String &p_type_hint = "", core_bind::ResourceLoader::CacheMode p_cache_mode = core_bind::ResourceLoader::CACHE_MODE_IGNORE) const;

	virtual PackedStringArray get_files() const { return {}; }
	virtual bool has_file(const String &p_path) const = 0;

	virtual String get_source() const { return source; }

	String get_resource_type(const String &p_path) const;

	virtual void on_added_to_filesystem_server() {}
	virtual void on_removed_to_filesystem_server() {}
};
