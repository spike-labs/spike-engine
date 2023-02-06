/**
 * display_server_universal.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "display_server_universal.h"

#include "core/config/project_settings.h"
#include "gles3/rasterizer_gles3.h"

DisplayServer::CreateFunction DisplayServerUniversal::_create_func = nullptr;

DisplayServer *DisplayServerUniversal::_create_universal(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Error &r_error) {
	auto display = _create_func(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, r_error);

#if defined(VULKAN_ENABLED)
	if (p_rendering_driver == "vulkan") {
	}
#endif

#if defined(GLES3_ENABLED)
	if (p_rendering_driver == "opengl3") {
		int skinning = GLOBAL_GET("rendering/gl_compatibility/skinning");
		if (skinning == 1) {
			RendererCompositorGLES3::make_current();
		}
	}
#endif

	return display;
}
