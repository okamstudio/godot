/**************************************************************************/
/*  renderer_compositor_rd.h                                              */
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

#ifndef RENDERER_COMPOSITOR_RD_H
#define RENDERER_COMPOSITOR_RD_H

#include "core/io/image.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/environment/fog.h"
#include "servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.h"
#include "servers/rendering/renderer_rd/framebuffer_cache_rd.h"
#include "servers/rendering/renderer_rd/renderer_canvas_render_rd.h"
#include "servers/rendering/renderer_rd/shaders/blit.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/utilities.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"

class RendererCompositorRD : public RendererCompositor {
protected:
	UniformSetCacheRD *uniform_set_cache = nullptr;
	FramebufferCacheRD *framebuffer_cache = nullptr;
	RendererCanvasRenderRD *canvas = nullptr;
	RendererRD::Utilities *utilities = nullptr;
	RendererRD::LightStorage *light_storage = nullptr;
	RendererRD::MaterialStorage *material_storage = nullptr;
	RendererRD::MeshStorage *mesh_storage = nullptr;
	RendererRD::ParticlesStorage *particles_storage = nullptr;
	RendererRD::TextureStorage *texture_storage = nullptr;
	RendererRD::Fog *fog = nullptr;
	RendererSceneRenderRD *scene = nullptr;

	enum BlitMode {
		BLIT_MODE_NORMAL,
		BLIT_MODE_USE_LAYER,
		BLIT_MODE_LENS,
		BLIT_MODE_NORMAL_ALPHA,
		BLIT_MODE_MAX
	};

	struct BlitPushConstant {
		float src_rect[4];
		float dst_rect[4];

		float rotation_sin;
		float rotation_cos;
		float pad[2];

		float eye_center[2];
		float k1;
		float k2;

		float upscale;
		float aspect_ratio;
		uint32_t layer;
		uint32_t convert_to_srgb;
	};

	struct Blit {
		BlitPushConstant push_constant;
		BlitShaderRD shader;
		RID shader_version;
		RID pipelines[BLIT_MODE_MAX];
		RID index_buffer;
		RID array;
		RID sampler;
	} blit;

	HashMap<RID, RID> render_target_descriptors;

	double time = 0.0;
	double delta = 0.0;

	static uint64_t frame;
	static RendererCompositorRD *singleton;

public:
	RendererUtilities *get_utilities() { return utilities; }
	RendererLightStorage *get_light_storage() { return light_storage; }
	RendererMaterialStorage *get_material_storage() { return material_storage; }
	RendererMeshStorage *get_mesh_storage() { return mesh_storage; }
	RendererParticlesStorage *get_particles_storage() { return particles_storage; }
	RendererTextureStorage *get_texture_storage() { return texture_storage; }
	RendererGI *get_gi() {
		ERR_FAIL_NULL_V(scene, nullptr);
		return scene->get_gi();
	}
	RendererFog *get_fog() { return fog; }
	RendererCanvasRender *get_canvas() { return canvas; }
	RendererSceneRender *get_scene() { return scene; }

	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter);

	void initialize();
	void begin_frame(double frame_step);
	void blit_render_targets_to_screen(DisplayServer::WindowID p_screen, const BlitToScreen *p_render_targets, int p_amount);

	void gl_end_frame(bool p_swap_buffers) {}
	void end_frame(bool p_present);
	void finalize();

	_ALWAYS_INLINE_ uint64_t get_frame_number() const { return frame; }
	_ALWAYS_INLINE_ double get_frame_delta_time() const { return delta; }
	_ALWAYS_INLINE_ double get_total_time() const { return time; }
	_ALWAYS_INLINE_ bool can_create_resources_async() const { return true; }

	static Error is_viable() {
		return OK;
	}

	static RendererCompositor *_create_current() {
		return memnew(RendererCompositorRD);
	}

	static void make_current() {
		_create_func = _create_current;
		low_end = false;
	}

	static RendererCompositorRD *get_singleton() { return singleton; }
	RendererCompositorRD();
	~RendererCompositorRD();
};

#endif // RENDERER_COMPOSITOR_RD_H
