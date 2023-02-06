/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "spike_define.h"
#include "core/io/resource_loader.h"

class LuaScriptResourceFormatLoader : public ResourceFormatLoader {
public:
#ifdef GODOT_3_X
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr) override;
#else
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false) override;
#endif
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

};
