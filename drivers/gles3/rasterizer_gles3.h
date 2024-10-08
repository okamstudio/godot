/**************************************************************************/
/*  rasterizer_gles3.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef RASTERIZER_GLES3_H
#define RASTERIZER_GLES3_H

#ifdef GLES3_ENABLED

#include "effects/copy_effects.h"
#include "effects/cubemap_filter.h"
#include "effects/glow.h"
#include "effects/post_effects.h"
#include "environment/fog.h"
#include "environment/gi.h"
#include "rasterizer_canvas_gles3.h"
#include "rasterizer_scene_gles3.h"
#include "servers/rendering/renderer_compositor.h"
#include "storage/config.h"
#include "storage/light_storage.h"
#include "storage/material_storage.h"
#include "storage/mesh_storage.h"
#include "storage/particles_storage.h"
#include "storage/texture_storage.h"
#include "storage/utilities.h"

class RasterizerGLES3 : public RendererCompositor {
private:
	uint64_t frame = 1;
	float delta = 0;

	double time_total = 0.0;
	bool flip_xy_workaround = false;

	static bool gles_over_gl;

protected:
	GLES3::Config *config = nullptr;
	GLES3::Utilities *utilities = nullptr;
	GLES3::TextureStorage *texture_storage = nullptr;
	GLES3::MaterialStorage *material_storage = nullptr;
	GLES3::MeshStorage *mesh_storage = nullptr;
	GLES3::ParticlesStorage *particles_storage = nullptr;
	GLES3::LightStorage *light_storage = nullptr;
	GLES3::GI *gi = nullptr;
	GLES3::Fog *fog = nullptr;
	GLES3::CopyEffects *copy_effects = nullptr;
	GLES3::CubemapFilter *cubemap_filter = nullptr;
	GLES3::Glow *glow = nullptr;
	GLES3::PostEffects *post_effects = nullptr;
	RasterizerCanvasGLES3 *canvas = nullptr;
	RasterizerSceneGLES3 *scene = nullptr;
	static RasterizerGLES3 *singleton;

	void _blit_render_target_to_screen(RID p_render_target, DisplayServer::WindowID p_screen, const Rect2 &p_screen_rect, uint32_t p_layer, bool p_first = true);

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

	void blit_render_targets_to_screen(DisplayServer::WindowID p_screen, const BlitToScreen *p_render_targets, int p_amount);

	void gl_end_frame(bool p_swap_buffers);
	void end_frame(bool p_swap_buffers);

	void finalize();

	static RendererCompositor *_create_current() {
		return memnew(RasterizerGLES3);
	}

	static bool is_gles_over_gl() { return gles_over_gl; }
	static void clear_depth(float p_depth);

	static void make_current(bool p_gles_over_gl) {
		gles_over_gl = p_gles_over_gl;
		OS::get_singleton()->set_gles_over_gl(gles_over_gl);
		_create_func = _create_current;
		low_end = true;
	}

	_ALWAYS_INLINE_ uint64_t get_frame_number() const { return frame; }
	_ALWAYS_INLINE_ double get_frame_delta_time() const { return delta; }
	_ALWAYS_INLINE_ double get_total_time() const { return time_total; }
	_ALWAYS_INLINE_ bool can_create_resources_async() const { return false; }

	static RasterizerGLES3 *get_singleton() { return singleton; }
	RasterizerGLES3();
	~RasterizerGLES3();
};

#endif // GLES3_ENABLED

#endif // RASTERIZER_GLES3_H
