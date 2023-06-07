/**
 * configuration_server.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration_set.h"
#include "configuration_table.h"
#include "core/object/object.h"
#include "core/os/os.h"

#define DEFAULT_GROUP "res://"

class ConfigurationServer : public Object {
	GDCLASS(ConfigurationServer, Object);

	static ConfigurationServer *singleton;

	HashMap<String, Ref<ConfigurationResource>> configurations;
	HashMap<String, Vector<Ref<ConfigurationResource>>> patches;

protected:
	static void _bind_methods();

public:
	static ConfigurationServer *get_singleton();

	static Ref<ConfigurationResource> load(const String &p_source);
	static Ref<ConfigurationSet> load_set(const String &p_source) { return load(p_source); }
	static Ref<ConfigurationTable> load_table(const String &p_source) { return load(p_source); }
	static Error unload(const String &p_source);

	static Vector<Ref<ConfigurationResource>> *get_patches(const String &p_source);
	static Error add_patch(const Ref<ConfigurationResource> &p_patch);
	static Error remove_patch(const Ref<ConfigurationResource> &p_patch);
};