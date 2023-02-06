/**
 * engine_utils.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/config/engine.h"
#include "spike_define.h"

class EngineUtils : public Object {
	GDCLASS(EngineUtils, Object);

protected:
	static void _bind_methods();

public:
	static Dictionary get_version_info();
};
