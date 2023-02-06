/**
 * display_server_universal.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "servers/display_server.h"
#include "spike_define.h"

class DisplayServerUniversal : public DisplayServer {
protected:
	static CreateFunction _create_func;
	static DisplayServer *_create_universal(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Error &r_error);

public:
	static void override_create_func() {
		_create_func = server_create_functions[0].create_function;
		server_create_functions[0].create_function = &_create_universal;
	}
};
