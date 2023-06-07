/**
 * configuration_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_server.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"

#define FORMAT_KEY(p_name, p_group) p_group.ends_with("/") ? vformat("%s%s", p_group, p_name) : vformat("%s/%s", p_group, p_name)

ConfigurationServer *ConfigurationServer::singleton = nullptr;

ConfigurationServer *ConfigurationServer::get_singleton() {
	if (nullptr == singleton) {
		singleton = memnew(ConfigurationServer);
	}
	return singleton;
}

void ConfigurationServer::_bind_methods() {
	ClassDB::bind_static_method("ConfigurationServer", D_METHOD("load", "source"), &ConfigurationServer::load);
	ClassDB::bind_static_method("ConfigurationServer", D_METHOD("unload", "source"), &ConfigurationServer::unload);
	ClassDB::bind_static_method("ConfigurationServer", D_METHOD("add_patch", "patch"), &ConfigurationServer::add_patch);
	// ClassDB::bind_static_method("ConfigurationServer", D_METHOD("remove_patch", "patch"), &ConfigurationServer::remove_patch);
}

Ref<ConfigurationResource> ConfigurationServer::load(const String &p_source) {
	Ref<ConfigurationResource> resource;
	auto patches = &get_singleton()->patches;
	auto configurations = &get_singleton()->configurations;
	if (!configurations->has(p_source)) {
		resource = ResourceLoader::load(p_source);
		if (resource.is_valid()) {
			configurations->operator[](p_source) = resource;
		} else {
			if (auto patches = get_patches(p_source)) {
				if (patches->size() > 0) {
					resource = patches->get(patches->size() - 1);
				}
			}
		}
	} else {
		resource = configurations->get(p_source);
	}
	return resource;
}

Error ConfigurationServer::unload(const String &p_source) {
	auto configurations = &get_singleton()->configurations;
	if (configurations->has(p_source)) {
		configurations->erase(p_source);
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}

Vector<Ref<ConfigurationResource>> *ConfigurationServer::get_patches(const String &p_source) {
	auto patches = &get_singleton()->patches;
	if (patches->has(p_source)) {
		return &patches->operator[](p_source);
	}
	return nullptr;
}

Error ConfigurationServer::add_patch(const Ref<ConfigurationResource> &p_patch) {
	ERR_FAIL_COND_V(!p_patch.is_valid(), ERR_FILE_UNRECOGNIZED);
	ERR_FAIL_COND_V(p_patch->get_source_path().is_empty(), FAILED);
	auto patches = &get_singleton()->patches;
	patches->operator[](p_patch->get_source_path()).append(p_patch);
	return OK;
}

Error ConfigurationServer::remove_patch(const Ref<ConfigurationResource> &p_patch) {
	ERR_FAIL_COND_V(!p_patch.is_valid(), ERR_FILE_UNRECOGNIZED);
	if (auto patch = get_patches(p_patch->get_source_path())) {
		Vector<Ref<ConfigurationResource>> delete_array;
		for (int i = 0; i < patch->size(); i++) {
			Ref<ConfigurationResource> elem = patch->get(i);
			if (elem == p_patch) {
				delete_array.append(elem);
				break;
			}
		}
		for (int i = 0; i < delete_array.size(); i++) {
			patch->erase(delete_array[i]);
		}
		return OK;
	}
	return FAILED;
}
