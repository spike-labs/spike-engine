/**
 * configuration_resource.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/resource.h"
#include "spike_define.h"
class ConfigurationResource : public Resource {
	GDCLASS(ConfigurationResource, Resource);

protected:
	String source_path;
	Ref<ConfigurationResource> source_res = nullptr;

public:
	bool is_patch = false;
	String get_source_path() const { return source_path; }
	Ref<ConfigurationResource> get_source_res() const { return source_res; }
	void set_source(const Ref<ConfigurationResource> &p_source_res, const String &p_source_path) {
		source_res = p_source_res;
		source_path = p_source_path;
	}
	ConfigurationResource() {}
};
