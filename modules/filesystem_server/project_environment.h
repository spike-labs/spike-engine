/**
 * project_environment.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "core/object/ref_counted.h"
#include "core/io/resource_uid.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"
#include "filesystem_server/file_provider.h"


class ProjectEnvironment : public RefCounted {
	GDCLASS(ProjectEnvironment, RefCounted);

private:
	HashMap<ResourceUID::ID, CharString> unique_ids; //unique IDs and utf8 paths (less memory used)

	TypedArray<Dictionary> global_classes;

	PackedStringArray resource_paths;

	HashMap<String, Variant> project_settings;

protected:
	static void _bind_methods();
public:

	Dictionary get_unique_ids() const;
	TypedArray<Dictionary> get_global_classes() const;
	PackedStringArray get_resource_paths() const;
	Dictionary get_project_settings() const;
	Variant get_project_setting(const String &p_setting, const Variant &p_default_value = Variant()) const;

	void parse_provider(Ref<FileProvider> p_provider);

	ProjectEnvironment();
	~ProjectEnvironment();
};