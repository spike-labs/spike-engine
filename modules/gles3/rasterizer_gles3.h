/**
 * rasterizer_gles3.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#ifdef GLES3_ENABLED

#include "drivers/gles3/effects/copy_effects.h"
#include "drivers/gles3/environment/fog.h"
#include "drivers/gles3/environment/gi.h"
#include "rasterizer_canvas_gles3.h"
#include "rasterizer_scene_gles3.h"
#include "servers/rendering/renderer_compositor.h"
#include "drivers/gles3/storage/config.h"
#include "drivers/gles3/storage/light_storage.h"
#include "drivers/gles3/storage/material_storage.h"
#include "storage/mesh_storage.h"
#include "storage/particles_storage.h"
#include "drivers/gles3/storage/texture_storage.h"
#include "storage/utilities.h"

class RendererCompositorGLES3 : public RendererCompositor {
private:
	uint64_t frame = 1;
	float delta = 0;

	double time_total = 0.0;

protected:
	GLES3::Config *config = nullptr;
	OPENGL3::Utilities *utilities = nullptr;
	GLES3::TextureStorage *texture_storage = nullptr;
	GLES3::MaterialStorage *material_storage = nullptr;
	OPENGL3::MeshStorage *mesh_storage = nullptr;
	OPENGL3::ParticlesStorage *particles_storage = nullptr;
	GLES3::LightStorage *light_storage = nullptr;
	GLES3::GI *gi = nullptr;
	GLES3::Fog *fog = nullptr;
	GLES3::CopyEffects *copy_effects = nullptr;
	RendererCanvasRenderGLES3 *canvas = nullptr;
	RendererSceneRenderGLES3 *scene = nullptr;

	void _blit_render_target_to_screen(RID p_render_target, DisplayServer::WindowID p_screen, const Rect2 &p_screen_rect, uint32_t p_layer);

public:
	RendererUtilities *get_utilities() { return utilities; }
	RendererLightStorage *get_light_storage() { return light_storage; }
	RendererMaterialStorage *get_material_storage() { return material_storage; }
	RendererMeshStorage *get_mesh_storage() { return mesh_storage; }
	RendererParticlesStorage *get_particles_storage() { return particles_storage; }
	RendererTextureStorage *get_texture_storage() { return texture_storage; }
	RendererGI *get_gi() { return gi; }
	RendererFog *get_fog() { return fog; }
	RendererCanvasRender *get_canvas() { return canvas; }
	RendererSceneRender *get_scene() { return scene; }

	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true);

	void initialize();
	void begin_frame(double frame_step);

	void prepare_for_blitting_render_targets();
	void blit_render_targets_to_screen(DisplayServer::WindowID p_screen, const BlitToScreen *p_render_targets, int p_amount);

	void end_frame(bool p_swap_buffers);

	void finalize();

	static RendererCompositor *_create_current() {
		return memnew(RendererCompositorGLES3);
	}

	static void make_current() {
		_create_func = _create_current;
		low_end = true;
	}

	_ALWAYS_INLINE_ uint64_t get_frame_number() const { return frame; }
	_ALWAYS_INLINE_ double get_frame_delta_time() const { return delta; }
	_ALWAYS_INLINE_ double get_total_time() const { return time_total; }

	RendererCompositorGLES3();
	~RendererCompositorGLES3();
};

#endif // GLES3_ENABLED
