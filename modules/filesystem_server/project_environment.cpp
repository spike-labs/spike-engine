/**
 * project_environment.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "project_environment.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/resource_uid.h"
#include "core/variant/array.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/project_environment.h"
#include "filesystem_server/providers/file_provider_pack.h"

void ProjectEnvironment::parse_provider(Ref<FileProvider> p_provider) {
	Ref<FileProviderPack> provider = p_provider;
	if (provider.is_null()) {
		return;
	}
	FileSystemServer::get_singleton()->push_current_provider(p_provider);

	// load unique_ids
	Error err = ERR_FILE_NOT_FOUND;
	auto f = p_provider->open("res://.godot/uid_cache.bin", FileAccess::READ, &err);
	if (err == OK) {
		int count = f->get_32();
		for (int i = 0; i < count; i++) {
			ResourceUID::ID id = f->get_64();
			String path = f->get_pascal_string();
			unique_ids[id] = path.utf8();
		}
		f.unref();
	}

	// load project settings and autoloads
	err = ERR_FILE_NOT_FOUND;
	f = p_provider->open("res://project.binary", FileAccess::READ, &err);
	if (err == OK) {
		auto magic = f->get_32();
		if (magic == 0x47464345) // ECFG
		{
			auto count = f->get_32();
			for (int i = 0; i < count; i++) {
				String key = f->get_pascal_string();
				Variant value = f->get_var(true);
				project_settings[key] = value;
			}
		}
		f.unref();
	}

	// load global_classes
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load("res://.godot/global_script_class_cache.cfg") == OK) {
		Array list = cf->get_value("", "list", Array());
		global_classes = list;
		cf.unref();
	}

	// parse resource_paths
	auto files = provider->get_files();
	for (int i = 0; i < files.size(); i++) {
		String path = files[i];
		if (path.begins_with("res://.godot/")) {
			continue;
		}
		if (path.ends_with(".import")) {
			path = path.substr(0, path.length() - 7);
		} else if (path.ends_with(".remap")) {
			path = path.substr(0, path.length() - 6);
		}
		resource_paths.push_back(path);
	}

	FileSystemServer::get_singleton()->pop_current_provider();
}

Dictionary ProjectEnvironment::get_unique_ids() const {
	Dictionary d;
	auto ruid = ResourceUID::get_singleton();
	for (auto E : unique_ids) {
		String key = ruid->id_to_text(E.key);
		String value = String::utf8(E.value.ptr());
		d[key] = value;
	}
	return d;
}

TypedArray<Dictionary> ProjectEnvironment::get_global_classes() const {
	return global_classes;
}

PackedStringArray ProjectEnvironment::get_resource_paths() const {
	return resource_paths;
}

Dictionary ProjectEnvironment::get_project_settings() const {
	Dictionary d;
	for (auto E : project_settings) {
		d[E.key] = E.value;
	}
	return d;
}

Variant ProjectEnvironment::get_project_setting(const String &p_setting, const Variant &p_default_value) const {
	const Variant *value = project_settings.getptr(p_setting);
	return value ? *value : p_default_value;
}

void ProjectEnvironment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_unique_ids"), &ProjectEnvironment::get_unique_ids);
	ClassDB::bind_method(D_METHOD("get_global_classes"), &ProjectEnvironment::get_global_classes);
	ClassDB::bind_method(D_METHOD("get_resource_paths"), &ProjectEnvironment::get_resource_paths);
	ClassDB::bind_method(D_METHOD("get_project_settings"), &ProjectEnvironment::get_project_settings);
	ClassDB::bind_method(D_METHOD("get_project_setting", "setting", "default_value"), &ProjectEnvironment::get_project_setting, DEFVAL(Variant()));
}

ProjectEnvironment::ProjectEnvironment() {
}

ProjectEnvironment::~ProjectEnvironment() {
}