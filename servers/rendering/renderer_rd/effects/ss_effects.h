/**************************************************************************/
/*  ss_effects.h                                                          */
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

#include "servers/rendering/renderer_rd/shaders/effects/screen_space_reflection.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/screen_space_reflection_filter.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/screen_space_reflection_scale.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ss_effects_downsample.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssao.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssao_blur.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssao_importance_map.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssao_interleave.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssil.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssil_blur.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssil_importance_map.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/ssil_interleave.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/subsurface_scattering.glsl.gen.h"
#include "servers/rendering_server.h"

#define RB_SCOPE_SSDS SNAME("rb_ssds")
#define RB_SCOPE_SSIL SNAME("rb_ssil")
#define RB_SCOPE_SSAO SNAME("rb_ssao")
#define RB_SCOPE_SSR SNAME("rb_ssr")

#define RB_LINEAR_DEPTH SNAME("linear_depth")
#define RB_FINAL SNAME("final")
#define RB_LAST_FRAME SNAME("last_frame")
#define RB_DEINTERLEAVED SNAME("deinterleaved")
#define RB_DEINTERLEAVED_PONG SNAME("deinterleaved_pong")
#define RB_EDGES SNAME("edges")
#define RB_IMPORTANCE_MAP SNAME("importance_map")
#define RB_IMPORTANCE_PONG SNAME("importance_pong")

#define RB_DEPTH_SCALED SNAME("depth_scaled")
#define RB_NORMAL_SCALED SNAME("normal_scaled")
#define RB_BLUR_RADIUS SNAME("blur_radius")
#define RB_INTERMEDIATE SNAME("intermediate")
#define RB_OUTPUT SNAME("output")

class RenderSceneBuffersRD;

namespace RendererRD {

class SSEffects {
private:
	static SSEffects *singleton;

public:
	static SSEffects *get_singleton() { return singleton; }

	SSEffects();
	~SSEffects();

	/* SS Downsampler */

	void downsample_depth(Ref<RenderSceneBuffersRD> p_render_buffers, uint32_t p_view, const Projection &p_projection);

	/* SSIL */
	void ssil_set_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to);

	struct SSILRenderBuffers {
		bool half_size = false;
		int buffer_width;
		int buffer_height;
		int half_buffer_width;
		int half_buffer_height;
	};

	struct SSILSettings {
		float radius = 1.0;
		float intensity = 2.0;
		float sharpness = 0.98;
		float normal_rejection = 1.0;

		Size2i full_screen_size;
	};

	void ssil_allocate_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, SSILRenderBuffers &p_ssil_buffers, const SSILSettings &p_settings);
	void screen_space_indirect_lighting(Ref<RenderSceneBuffersRD> p_render_buffers, SSILRenderBuffers &p_ssil_buffers, uint32_t p_view, RID p_normal_buffer, const Projection &p_projection, const Projection &p_last_projection, const SSILSettings &p_settings);

	/* SSAO */
	void ssao_set_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to);

	struct SSAORenderBuffers {
		bool half_size = false;
		int buffer_width;
		int buffer_height;
		int half_buffer_width;
		int half_buffer_height;
	};

	struct SSAOSettings {
		float radius = 1.0;
		float intensity = 2.0;
		float power = 1.5;
		float detail = 0.5;
		float horizon = 0.06;
		float sharpness = 0.98;

		Size2i full_screen_size;
	};

	void ssao_allocate_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, SSAORenderBuffers &p_ssao_buffers, const SSAOSettings &p_settings);
	void generate_ssao(Ref<RenderSceneBuffersRD> p_render_buffers, SSAORenderBuffers &p_ssao_buffers, uint32_t p_view, RID p_normal_buffer, const Projection &p_projection, const SSAOSettings &p_settings);

	/* Screen Space Reflection */
	void ssr_set_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality);

	struct SSRRenderBuffers {
		Size2i size;
		RenderingServer::EnvironmentSSRRoughnessQuality roughness_quality = RenderingServer::ENV_SSR_ROUGHNESS_QUALITY_DISABLED;
	};

	void ssr_allocate_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, SSRRenderBuffers &p_ssr_buffers, const RenderingDevice::DataFormat p_color_format);
	void screen_space_reflection(Ref<RenderSceneBuffersRD> p_render_buffers, SSRRenderBuffers &p_ssr_buffers, const RID *p_normal_roughness_slices, const RID *p_metallic_slices, int p_max_steps, float p_fade_in, float p_fade_out, float p_tolerance, const Projection *p_projections, const Vector3 *p_eye_offsets);

	/* subsurface scattering */
	void sss_set_quality(RS::SubSurfaceScatteringQuality p_quality);
	RS::SubSurfaceScatteringQuality sss_get_quality() const;
	void sss_set_scale(float p_scale, float p_depth_scale);

	void sub_surface_scattering(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_diffuse, RID p_depth, const Projection &p_camera, const Size2i &p_screen_size);

private:
	/* Settings */

	RS::EnvironmentSSAOQuality ssao_quality = RS::ENV_SSAO_QUALITY_MEDIUM;
	bool ssao_half_size = false;
	float ssao_adaptive_target = 0.5;
	int ssao_blur_passes = 2;
	float ssao_fadeout_from = 50.0;
	float ssao_fadeout_to = 300.0;

	RS::EnvironmentSSILQuality ssil_quality = RS::ENV_SSIL_QUALITY_MEDIUM;
	bool ssil_half_size = false;
	float ssil_adaptive_target = 0.5;
	int ssil_blur_passes = 4;
	float ssil_fadeout_from = 50.0;
	float ssil_fadeout_to = 300.0;

	RS::EnvironmentSSRRoughnessQuality ssr_roughness_quality = RS::ENV_SSR_ROUGHNESS_QUALITY_LOW;

	RS::SubSurfaceScatteringQuality sss_quality = RS::SUB_SURFACE_SCATTERING_QUALITY_MEDIUM;
	float sss_scale = 0.05;
	float sss_depth_scale = 0.01;

	/* SS Downsampler */

	struct SSEffectsDownsamplePushConstant {
		float pixel_size[2];
		float z_far;
		float z_near;
		uint32_t orthogonal;
		float radius_sq;
		uint32_t pad[2];
	};

	enum SSEffectsMode {
		SS_EFFECTS_DOWNSAMPLE,
		SS_EFFECTS_DOWNSAMPLE_HALF_RES,
		SS_EFFECTS_DOWNSAMPLE_MIPMAP,
		SS_EFFECTS_DOWNSAMPLE_MIPMAP_HALF_RES,
		SS_EFFECTS_DOWNSAMPLE_HALF,
		SS_EFFECTS_DOWNSAMPLE_HALF_RES_HALF,
		SS_EFFECTS_DOWNSAMPLE_FULL_MIPS,
		SS_EFFECTS_MAX
	};

	struct SSEffectsGatherConstants {
		float rotation_matrices[80]; //5 vec4s * 4
	};

	struct SSEffectsShader {
		SSEffectsDownsamplePushConstant downsample_push_constant;
		SsEffectsDownsampleShaderRD downsample_shader;
		RID downsample_shader_version;
		bool used_half_size_last_frame = false;
		bool used_mips_last_frame = false;
		bool used_full_mips_last_frame = false;

		RID gather_constants_buffer;

		RID mirror_sampler;

		RID pipelines[SS_EFFECTS_MAX];
	} ss_effects;

	/* SSIL */

	enum SSILMode {
		SSIL_GATHER,
		SSIL_GATHER_BASE,
		SSIL_GATHER_ADAPTIVE,
		SSIL_GENERATE_IMPORTANCE_MAP,
		SSIL_PROCESS_IMPORTANCE_MAPA,
		SSIL_PROCESS_IMPORTANCE_MAPB,
		SSIL_BLUR_PASS,
		SSIL_BLUR_PASS_SMART,
		SSIL_BLUR_PASS_WIDE,
		SSIL_INTERLEAVE,
		SSIL_INTERLEAVE_SMART,
		SSIL_INTERLEAVE_HALF,
		SSIL_MAX
	};

	struct SSILGatherPushConstant {
		int32_t screen_size[2];
		int pass;
		int quality;

		float half_screen_pixel_size[2];
		float half_screen_pixel_size_x025[2];

		float NDC_to_view_mul[2];
		float NDC_to_view_add[2];

		float pad2[2];
		float z_near;
		float z_far;

		float radius;
		float intensity;
		int size_multiplier;
		int pad;

		float fade_out_mul;
		float fade_out_add;
		float normal_rejection_amount;
		float inv_radius_near_limit;

		uint32_t is_orthogonal;
		float neg_inv_radius;
		float load_counter_avg_div;
		float adaptive_sample_limit;

		int32_t pass_coord_offset[2];
		float pass_uv_offset[2];
	};

	struct SSILImportanceMapPushConstant {
		float half_screen_pixel_size[2];
		float intensity;
		float pad;
	};

	struct SSILBlurPushConstant {
		float edge_sharpness;
		float pad;
		float half_screen_pixel_size[2];
	};

	struct SSILInterleavePushConstant {
		float inv_sharpness;
		uint32_t size_modifier;
		float pixel_size[2];
	};

	struct SSILProjectionUniforms {
		float inv_last_frame_projection_matrix[16];
	};

	struct SSIL {
		SSILGatherPushConstant gather_push_constant;
		SsilShaderRD gather_shader;
		RID gather_shader_version;
		RID projection_uniform_buffer;

		SSILImportanceMapPushConstant importance_map_push_constant;
		SsilImportanceMapShaderRD importance_map_shader;
		RID importance_map_shader_version;
		RID importance_map_load_counter;
		RID counter_uniform_set;

		SSILBlurPushConstant blur_push_constant;
		SsilBlurShaderRD blur_shader;
		RID blur_shader_version;

		SSILInterleavePushConstant interleave_push_constant;
		SsilInterleaveShaderRD interleave_shader;
		RID interleave_shader_version;

		RID pipelines[SSIL_MAX];
	} ssil;

	void gather_ssil(RD::ComputeListID p_compute_list, const RID *p_ssil_slices, const RID *p_edges_slices, const SSILSettings &p_settings, bool p_adaptive_base_pass, RID p_gather_uniform_set, RID p_importance_map_uniform_set, RID p_projection_uniform_set);

	/* SSAO */

	enum SSAOMode {
		SSAO_GATHER,
		SSAO_GATHER_BASE,
		SSAO_GATHER_ADAPTIVE,
		SSAO_GENERATE_IMPORTANCE_MAP,
		SSAO_PROCESS_IMPORTANCE_MAPA,
		SSAO_PROCESS_IMPORTANCE_MAPB,
		SSAO_BLUR_PASS,
		SSAO_BLUR_PASS_SMART,
		SSAO_BLUR_PASS_WIDE,
		SSAO_INTERLEAVE,
		SSAO_INTERLEAVE_SMART,
		SSAO_INTERLEAVE_HALF,
		SSAO_MAX
	};

	struct SSAOGatherPushConstant {
		int32_t screen_size[2];
		int pass;
		int quality;

		float half_screen_pixel_size[2];
		int size_multiplier;
		float detail_intensity;

		float NDC_to_view_mul[2];
		float NDC_to_view_add[2];

		float pad[2];
		float half_screen_pixel_size_x025[2];

		float radius;
		float intensity;
		float shadow_power;
		float shadow_clamp;

		float fade_out_mul;
		float fade_out_add;
		float horizon_angle_threshold;
		float inv_radius_near_limit;

		uint32_t is_orthogonal;
		float neg_inv_radius;
		float load_counter_avg_div;
		float adaptive_sample_limit;

		int32_t pass_coord_offset[2];
		float pass_uv_offset[2];
	};

	struct SSAOImportanceMapPushConstant {
		float half_screen_pixel_size[2];
		float intensity;
		float power;
	};

	struct SSAOBlurPushConstant {
		float edge_sharpness;
		float pad;
		float half_screen_pixel_size[2];
	};

	struct SSAOInterleavePushConstant {
		float inv_sharpness;
		uint32_t size_modifier;
		float pixel_size[2];
	};

	struct SSAO {
		SSAOGatherPushConstant gather_push_constant;
		SsaoShaderRD gather_shader;
		RID gather_shader_version;

		SSAOImportanceMapPushConstant importance_map_push_constant;
		SsaoImportanceMapShaderRD importance_map_shader;
		RID importance_map_shader_version;
		RID importance_map_load_counter;
		RID counter_uniform_set;

		SSAOBlurPushConstant blur_push_constant;
		SsaoBlurShaderRD blur_shader;
		RID blur_shader_version;

		SSAOInterleavePushConstant interleave_push_constant;
		SsaoInterleaveShaderRD interleave_shader;
		RID interleave_shader_version;

		RID pipelines[SSAO_MAX];
	} ssao;

	void gather_ssao(RD::ComputeListID p_compute_list, const RID *p_ao_slices, const SSAOSettings &p_settings, bool p_adaptive_base_pass, RID p_gather_uniform_set, RID p_importance_map_uniform_set);

	/* Screen Space Reflection */

	enum SSRShaderSpecializations {
		SSR_MULTIVIEW = 1 << 0,
		SSR_VARIATIONS = 2,
	};

	struct ScreenSpaceReflectionSceneData {
		float projection[2][16];
		float inv_projection[2][16];
		float eye_offset[2][4];
	};

	// SSR Scale

	struct ScreenSpaceReflectionScalePushConstant {
		int32_t screen_size[2];
		float camera_z_near;
		float camera_z_far;

		uint32_t orthogonal;
		uint32_t filter;
		uint32_t view_index;
		uint32_t pad1;
	};

	struct ScreenSpaceReflectionScale {
		ScreenSpaceReflectionScaleShaderRD shader;
		RID shader_version;
		RID pipelines[SSR_VARIATIONS];
	} ssr_scale;

	// SSR main

	enum ScreenSpaceReflectionMode {
		SCREEN_SPACE_REFLECTION_NORMAL,
		SCREEN_SPACE_REFLECTION_ROUGH,
		SCREEN_SPACE_REFLECTION_MAX,
	};

	struct ScreenSpaceReflectionPushConstant {
		float proj_info[4]; // 16 - 16

		int32_t screen_size[2]; //  8 - 24
		float camera_z_near; //  4 - 28
		float camera_z_far; //  4 - 32

		int32_t num_steps; //  4 - 36
		float depth_tolerance; //  4 - 40
		float distance_fade; //  4 - 44
		float curve_fade_in; //  4 - 48

		uint32_t orthogonal; //  4 - 52
		float filter_mipmap_levels; //  4 - 56
		uint32_t use_half_res; //  4 - 60
		uint32_t view_index; //  4 - 64

		// float projection[16];			// this is in our ScreenSpaceReflectionSceneData now
	};

	struct ScreenSpaceReflection {
		ScreenSpaceReflectionShaderRD shader;
		RID shader_version;
		RID pipelines[SSR_VARIATIONS][SCREEN_SPACE_REFLECTION_MAX];

		RID ubo;
	} ssr;

	// SSR Filter

	struct ScreenSpaceReflectionFilterPushConstant {
		float proj_info[4]; // 16 - 16

		uint32_t orthogonal; //  4 - 20
		float edge_tolerance; //  4 - 24
		int32_t increment; //  4 - 28
		uint32_t view_index; //  4 - 32

		int32_t screen_size[2]; //  8 - 40
		uint32_t vertical; //  4 - 44
		uint32_t steps; //  4 - 48
	};

	enum SSRReflectionMode {
		SCREEN_SPACE_REFLECTION_FILTER_HORIZONTAL,
		SCREEN_SPACE_REFLECTION_FILTER_VERTICAL,
		SCREEN_SPACE_REFLECTION_FILTER_MAX,
	};

	struct ScreenSpaceReflectionFilter {
		ScreenSpaceReflectionFilterShaderRD shader;
		RID shader_version;
		RID pipelines[SSR_VARIATIONS][SCREEN_SPACE_REFLECTION_FILTER_MAX];
	} ssr_filter;

	/* Subsurface scattering */

	struct SubSurfaceScatteringPushConstant {
		int32_t screen_size[2];
		float camera_z_far;
		float camera_z_near;

		uint32_t vertical;
		uint32_t orthogonal;
		float unit_size;
		float scale;

		float depth_scale;
		uint32_t pad[3];
	};

	struct SubSurfaceScattering {
		SubSurfaceScatteringPushConstant push_constant;
		SubsurfaceScatteringShaderRD shader;
		RID shader_version;
		RID pipelines[3]; //3 quality levels
	} sss;
};

} // namespace RendererRD
