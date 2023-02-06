/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once
#include "spike_define.h"
#include "core/io/resource_saver.h"

class LuaScriptResourceFormatSaver : public ResourceFormatSaver {
public:
#ifdef GODOT_3_X
	virtual Error save(const String &p_path, const RES &p_resource, uint32_t p_flags = 0) override;
#else
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
#endif
	virtual void get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const RES &p_resource) const override;
};
