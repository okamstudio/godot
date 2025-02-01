/**************************************************************************/
/*  sky.h                                                                 */
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

#pragma once

#include "core/templates/rid_owner.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/shaders/environment/sky.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/shader_compiler.h"

// Forward declare RendererSceneRenderRD so we can pass it into some of our methods, these classes are pretty tightly bound
class RendererSceneRenderRD;
class RenderSceneBuffersRD;

namespace RendererRD {

class SkyRD {
public:
	enum SkySet {
		SKY_SET_UNIFORMS,
		SKY_SET_MATERIAL,
		SKY_SET_TEXTURES,
		SKY_SET_FOG,
	};

	const int SAMPLERS_BINDING_FIRST_INDEX = 4;

	// Skys need less info from Directional Lights than the normal shaders
	struct SkyDirectionalLightData {
		float direction[3];
		float energy;
		float color[3];
		float size;
		uint32_t enabled;
		uint32_t pad[3];
	};

private:
	RD::DataFormat texture_format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;

	enum SkyTextureSetVersion {
		SKY_TEXTURE_SET_BACKGROUND,
		SKY_TEXTURE_SET_HALF_RES,
		SKY_TEXTURE_SET_QUARTER_RES,
		SKY_TEXTURE_SET_CUBEMAP,
		SKY_TEXTURE_SET_CUBEMAP_HALF_RES,
		SKY_TEXTURE_SET_CUBEMAP_QUARTER_RES,
		SKY_TEXTURE_SET_MAX
	};

	enum SkyVersion {
		SKY_VERSION_BACKGROUND,
		SKY_VERSION_HALF_RES,
		SKY_VERSION_QUARTER_RES,
		SKY_VERSION_CUBEMAP,
		SKY_VERSION_CUBEMAP_HALF_RES,
		SKY_VERSION_CUBEMAP_QUARTER_RES,

		SKY_VERSION_BACKGROUND_MULTIVIEW,
		SKY_VERSION_HALF_RES_MULTIVIEW,
		SKY_VERSION_QUARTER_RES_MULTIVIEW,

		SKY_VERSION_MAX
	};

	struct SkyPushConstant {
		float orientation[12]; // 48 - 48
		float projection[4]; // 16 - 64
		float position[3]; // 12 - 76
		float time; // 4 - 80
		float pad[2]; // 8 - 88
		float luminance_multiplier; // 4 - 92
		float brightness_multiplier; // 4 - 96
		// 128 is the max size of a push constant. We can replace "pad" but we can't add any more.
	};

	struct SkyShaderData : public RendererRD::MaterialStorage::ShaderData {
		bool valid = false;
		RID version;

		PipelineCacheRD pipelines[SKY_VERSION_MAX];
		Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

		Vector<uint32_t> ubo_offsets;
		uint32_t ubo_size = 0;

		String code;

		bool uses_time = false;
		bool uses_position = false;
		bool uses_half_res = false;
		bool uses_quarter_res = false;
		bool uses_light = false;

		virtual void set_code(const String &p_Code);
		virtual bool is_animated() const;
		virtual bool casts_shadows() const;
		virtual RS::ShaderNativeSourceCode get_native_source_code() const;

		SkyShaderData() {}
		virtual ~SkyShaderData();
	};

	void _render_sky(RD::DrawListID p_list, float p_time, RID p_fb, PipelineCacheRD *p_pipeline, RID p_uniform_set, RID p_texture_set, const Projection &p_projection, const Basis &p_orientation, const Vector3 &p_position, float p_luminance_multiplier, float p_brightness_modifier);

public:
	struct SkySceneState {
		struct UBO {
			float combined_reprojection[RendererSceneRender::MAX_RENDER_VIEWS][16]; // 2 x 64 - 128
			float view_inv_projections[RendererSceneRender::MAX_RENDER_VIEWS][16]; // 2 x 64 - 256
			float view_eye_offsets[RendererSceneRender::MAX_RENDER_VIEWS][4]; // 2 x 16 - 288

			uint32_t volumetric_fog_enabled; // 4 - 292
			float volumetric_fog_inv_length; // 4 - 296
			float volumetric_fog_detail_spread; // 4 - 300
			float volumetric_fog_sky_affect; // 4 - 304

			uint32_t fog_enabled; // 4 - 308
			float fog_sky_affect; // 4 - 312
			float fog_density; // 4 - 316
			float fog_sun_scatter; // 4 - 320

			float fog_light_color[3]; // 12 - 332
			float fog_aerial_perspective; // 4 - 336

			float z_far; // 4 - 340
			uint32_t directional_light_count; // 4 - 344
			uint32_t pad1; // 4 - 348
			uint32_t pad2; // 4 - 352
		};

		UBO ubo;

		uint32_t view_count = 1;
		Transform3D cam_transform;
		Projection cam_projection;

		SkyDirectionalLightData *directional_lights = nullptr;
		SkyDirectionalLightData *last_frame_directional_lights = nullptr;
		uint32_t max_directional_lights;
		uint32_t last_frame_directional_light_count;
		RID directional_light_buffer;
		RID uniform_set;
		RID uniform_buffer;
		RID fog_uniform_set;
		RID default_fog_uniform_set;

		RID fog_shader;
		RID fog_material;
		RID fog_only_texture_uniform_set;
	} sky_scene_state;

	struct ReflectionData {
		struct Layer {
			struct Mipmap {
				RID framebuffers[6];
				RID views[6];
				Size2i size;
			};
			Vector<Mipmap> mipmaps; //per-face view
			Vector<RID> views; // per-cubemap view
		};

		struct DownsampleLayer {
			struct Mipmap {
				RID view;
				Size2i size;

				// for mobile only
				RID views[6];
				RID framebuffers[6];
			};
			Vector<Mipmap> mipmaps;
		};

		RID radiance_base_cubemap; //cubemap for first layer, first cubemap
		RID downsampled_radiance_cubemap;
		DownsampleLayer downsampled_layer;
		RID coefficient_buffer;

		bool dirty = true;

		Vector<Layer> layers;

		void clear_reflection_data();
		void update_reflection_data(int p_size, int p_mipmaps, bool p_use_array, RID p_base_cube, int p_base_layer, bool p_low_quality, int p_roughness_layers, RD::DataFormat p_texture_format);
		void create_reflection_fast_filter(bool p_use_arrays);
		void create_reflection_importance_sample(bool p_use_arrays, int p_cube_side, int p_base_layer, uint32_t p_sky_ggx_samples_quality);
		void update_reflection_mipmaps(int p_start, int p_end);
	};

	/* Sky shader */

	struct SkyShader {
		SkyShaderRD shader;
		ShaderCompiler compiler;

		RID default_shader;
		RID default_material;
		RID default_shader_rd;
	} sky_shader;

	struct SkyMaterialData : public RendererRD::MaterialStorage::MaterialData {
		SkyShaderData *shader_data = nullptr;
		RID uniform_set;
		bool uniform_set_updated;

		virtual void set_render_priority(int p_priority) {}
		virtual void set_next_pass(RID p_pass) {}
		virtual bool update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
		virtual ~SkyMaterialData();
	};

	struct Sky {
		RID radiance;
		RID quarter_res_pass;
		RID quarter_res_framebuffer;
		Size2i screen_size;

		RID uniform_set;

		RID material;
		RID uniform_buffer;

		int radiance_size = 256;

		RS::SkyMode mode = RS::SKY_MODE_AUTOMATIC;

		ReflectionData reflection;
		bool dirty = false;
		int processing_layer = 0;
		Sky *dirty_list = nullptr;
		float baked_exposure = 1.0;

		//State to track when radiance cubemap needs updating
		SkyMaterialData *prev_material = nullptr;
		Vector3 prev_position;
		float prev_time;

		void free();

		RID get_textures(SkyTextureSetVersion p_version, RID p_default_shader_rd, Ref<RenderSceneBuffersRD> p_render_buffers);
		bool set_radiance_size(int p_radiance_size);
		bool set_mode(RS::SkyMode p_mode);
		bool set_material(RID p_material);
		Ref<Image> bake_panorama(float p_energy, int p_roughness_layers, const Size2i &p_size);
	};

	uint32_t sky_ggx_samples_quality;
	bool sky_use_cubemap_array;
	Sky *dirty_sky_list = nullptr;
	mutable RID_Owner<Sky, true> sky_owner;
	int roughness_layers;

	RendererRD::MaterialStorage::ShaderData *_create_sky_shader_func();
	static RendererRD::MaterialStorage::ShaderData *_create_sky_shader_funcs();

	RendererRD::MaterialStorage::MaterialData *_create_sky_material_func(SkyShaderData *p_shader);
	static RendererRD::MaterialStorage::MaterialData *_create_sky_material_funcs(RendererRD::MaterialStorage::ShaderData *p_shader);

	SkyRD();
	void init();
	void set_texture_format(RD::DataFormat p_texture_format);
	~SkyRD();

	void setup_sky(const RenderDataRD *p_render_data, const Size2i p_screen_size);
	void update_radiance_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_env, const Vector3 &p_global_pos, double p_time, float p_luminance_multiplier = 1.0, float p_brightness_multiplier = 1.0);
	void update_res_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_env, double p_time, float p_luminance_multiplier = 1.0, float p_brightness_multiplier = 1.0);
	void draw_sky(RD::DrawListID p_draw_list, Ref<RenderSceneBuffersRD> p_render_buffers, RID p_env, RID p_fb, double p_time, float p_luminance_multiplier = 1.0, float p_brightness_multiplier = 1.0);

	void invalidate_sky(Sky *p_sky);
	void update_dirty_skys();

	RID sky_get_material(RID p_sky) const;
	RID sky_get_radiance_texture_rd(RID p_sky) const;
	float sky_get_baked_exposure(RID p_sky) const;

	RID allocate_sky_rid();
	void initialize_sky_rid(RID p_rid);
	Sky *get_sky(RID p_sky) const;
	void free_sky(RID p_sky);
	void sky_set_radiance_size(RID p_sky, int p_radiance_size);
	void sky_set_mode(RID p_sky, RS::SkyMode p_mode);
	void sky_set_material(RID p_sky, RID p_material);
	Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size);
};

} // namespace RendererRD
