/**************************************************************************/
/*  copy_effects.h                                                        */
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

#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/shaders/effects/blur_raster.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/copy.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/copy_to_fb.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cube_to_dp.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_downsampler.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_downsampler_raster.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_filter.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_filter_raster.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_roughness.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/cubemap_roughness_raster.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/specular_merge.glsl.gen.h"
#include "servers/rendering/renderer_scene_render.h"

#include "servers/rendering_server.h"

namespace RendererRD {

class CopyEffects {
private:
	bool prefer_raster_effects;

	// Blur raster shader

	enum BlurRasterMode {
		BLUR_MIPMAP,

		BLUR_MODE_GAUSSIAN_BLUR,
		BLUR_MODE_GAUSSIAN_GLOW,
		BLUR_MODE_GAUSSIAN_GLOW_AUTO_EXPOSURE,
		BLUR_MODE_COPY,

		BLUR_MODE_SET_COLOR,

		BLUR_MODE_MAX
	};

	enum {
		BLUR_FLAG_HORIZONTAL = (1 << 0),
		BLUR_FLAG_USE_ORTHOGONAL_PROJECTION = (1 << 1),
		BLUR_FLAG_GLOW_FIRST_PASS = (1 << 2),
	};

	struct BlurRasterPushConstant {
		float pixel_size[2];
		uint32_t flags;
		uint32_t pad;

		//glow
		float glow_strength;
		float glow_bloom;
		float glow_hdr_threshold;
		float glow_hdr_scale;

		float glow_exposure;
		float glow_white;
		float glow_luminance_cap;
		float glow_auto_exposure_scale;

		float luminance_multiplier;
		float res1;
		float res2;
		float res3;
	};

	struct BlurRaster {
		BlurRasterPushConstant push_constant;
		BlurRasterShaderRD shader;
		RID shader_version;
		PipelineCacheRD pipelines[BLUR_MODE_MAX];
	} blur_raster;

	// Copy shader

	enum CopyMode {
		COPY_MODE_GAUSSIAN_COPY,
		COPY_MODE_GAUSSIAN_COPY_8BIT,
		COPY_MODE_GAUSSIAN_GLOW,
		COPY_MODE_GAUSSIAN_GLOW_AUTO_EXPOSURE,
		COPY_MODE_SIMPLY_COPY,
		COPY_MODE_SIMPLY_COPY_8BIT,
		COPY_MODE_SIMPLY_COPY_DEPTH,
		COPY_MODE_SET_COLOR,
		COPY_MODE_SET_COLOR_8BIT,
		COPY_MODE_MIPMAP,
		COPY_MODE_LINEARIZE_DEPTH,
		COPY_MODE_CUBE_TO_PANORAMA,
		COPY_MODE_CUBE_ARRAY_TO_PANORAMA,
		COPY_MODE_MAX,

	};

	enum {
		COPY_FLAG_HORIZONTAL = (1 << 0),
		COPY_FLAG_USE_COPY_SECTION = (1 << 1),
		COPY_FLAG_USE_ORTHOGONAL_PROJECTION = (1 << 2),
		COPY_FLAG_DOF_NEAR_FIRST_TAP = (1 << 3),
		COPY_FLAG_GLOW_FIRST_PASS = (1 << 4),
		COPY_FLAG_FLIP_Y = (1 << 5),
		COPY_FLAG_FORCE_LUMINANCE = (1 << 6),
		COPY_FLAG_ALL_SOURCE = (1 << 7),
		COPY_FLAG_ALPHA_TO_ONE = (1 << 8),
	};

	struct CopyPushConstant {
		int32_t section[4];
		int32_t target[2];
		uint32_t flags;
		uint32_t pad;
		// Glow.
		float glow_strength;
		float glow_bloom;
		float glow_hdr_threshold;
		float glow_hdr_scale;

		float glow_exposure;
		float glow_white;
		float glow_luminance_cap;
		float glow_auto_exposure_scale;
		// DOF.
		float camera_z_far;
		float camera_z_near;
		uint32_t pad2[2];
		//SET color
		float set_color[4];
	};

	struct Copy {
		CopyPushConstant push_constant;
		CopyShaderRD shader;
		RID shader_version;
		RID pipelines[COPY_MODE_MAX];

	} copy;

	// Copy to FB shader

	enum CopyToFBMode {
		COPY_TO_FB_COPY,
		COPY_TO_FB_COPY_PANORAMA_TO_DP,
		COPY_TO_FB_COPY2,
		COPY_TO_FB_SET_COLOR,

		// These variants are disabled unless XR shaders are enabled.
		// They should be listed last.
		COPY_TO_FB_MULTIVIEW,
		COPY_TO_FB_MULTIVIEW_WITH_DEPTH,

		COPY_TO_FB_MAX,
	};

	enum CopyToFBFlags {
		COPY_TO_FB_FLAG_FLIP_Y = (1 << 0),
		COPY_TO_FB_FLAG_USE_SECTION = (1 << 1),
		COPY_TO_FB_FLAG_FORCE_LUMINANCE = (1 << 2),
		COPY_TO_FB_FLAG_ALPHA_TO_ZERO = (1 << 3),
		COPY_TO_FB_FLAG_SRGB = (1 << 4),
		COPY_TO_FB_FLAG_ALPHA_TO_ONE = (1 << 5),
		COPY_TO_FB_FLAG_LINEAR = (1 << 6),
		COPY_TO_FB_FLAG_NORMAL = (1 << 7),
		COPY_TO_FB_FLAG_USE_SRC_SECTION = (1 << 8),
	};

	struct CopyToFbPushConstant {
		float section[4];
		float pixel_size[2];
		float luminance_multiplier;
		uint32_t flags;

		float set_color[4];
	};

	struct CopyToFb {
		CopyToFbPushConstant push_constant;
		CopyToFbShaderRD shader;
		RID shader_version;
		PipelineCacheRD pipelines[COPY_TO_FB_MAX];

	} copy_to_fb;

	// Copy to DP

	struct CopyToDPPushConstant {
		float z_far;
		float z_near;
		float texel_size[2];
	};

	struct CopyToDP {
		CubeToDpShaderRD shader;
		RID shader_version;
		PipelineCacheRD pipeline;
	} cube_to_dp;

	// Cubemap effects

	struct CubemapDownsamplerPushConstant {
		uint32_t face_size;
		uint32_t face_id;
		float pad[2];
	};

	struct CubemapDownsampler {
		CubemapDownsamplerPushConstant push_constant;
		CubemapDownsamplerShaderRD compute_shader;
		CubemapDownsamplerRasterShaderRD raster_shader;
		RID shader_version;
		RID compute_pipeline;
		PipelineCacheRD raster_pipeline;
	} cubemap_downsampler;

	enum CubemapFilterMode {
		FILTER_MODE_HIGH_QUALITY,
		FILTER_MODE_LOW_QUALITY,
		FILTER_MODE_HIGH_QUALITY_ARRAY,
		FILTER_MODE_LOW_QUALITY_ARRAY,
		FILTER_MODE_MAX,
	};

	struct CubemapFilterRasterPushConstant {
		uint32_t mip_level;
		uint32_t face_id;
		float pad[2];
	};

	struct CubemapFilter {
		CubemapFilterShaderRD compute_shader;
		CubemapFilterRasterShaderRD raster_shader;
		RID shader_version;
		RID compute_pipelines[FILTER_MODE_MAX];
		PipelineCacheRD raster_pipelines[FILTER_MODE_MAX];

		RID uniform_set;
		RID image_uniform_set;
		RID coefficient_buffer;
		bool use_high_quality;

	} filter;

	struct CubemapRoughnessPushConstant {
		uint32_t face_id;
		uint32_t sample_count;
		float roughness;
		uint32_t use_direct_write;
		float face_size;
		float pad[3];
	};

	struct CubemapRoughness {
		CubemapRoughnessPushConstant push_constant;
		CubemapRoughnessShaderRD compute_shader;
		CubemapRoughnessRasterShaderRD raster_shader;
		RID shader_version;
		RID compute_pipeline;
		PipelineCacheRD raster_pipeline;
	} roughness;

	// Merge specular

	enum SpecularMergeMode {
		SPECULAR_MERGE_ADD,
		SPECULAR_MERGE_SSR,
		SPECULAR_MERGE_ADDITIVE_ADD,
		SPECULAR_MERGE_ADDITIVE_SSR,

		SPECULAR_MERGE_ADD_MULTIVIEW,
		SPECULAR_MERGE_SSR_MULTIVIEW,
		SPECULAR_MERGE_ADDITIVE_ADD_MULTIVIEW,
		SPECULAR_MERGE_ADDITIVE_SSR_MULTIVIEW,

		SPECULAR_MERGE_MAX
	};

	/* Specular merge must be done using raster, rather than compute
	 * because it must continue the existing color buffer
	 */

	struct SpecularMerge {
		SpecularMergeShaderRD shader;
		RID shader_version;
		PipelineCacheRD pipelines[SPECULAR_MERGE_MAX];

	} specular_merge;

	static CopyEffects *singleton;

public:
	static CopyEffects *get_singleton();

	CopyEffects(bool p_prefer_raster_effects);
	~CopyEffects();

	bool get_prefer_raster_effects() { return prefer_raster_effects; }

	void copy_to_rect(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_rect, bool p_flip_y = false, bool p_force_luminance = false, bool p_all_source = false, bool p_8_bit_dst = false, bool p_alpha_to_one = false);
	void copy_cubemap_to_panorama(RID p_source_cube, RID p_dest_panorama, const Size2i &p_panorama_size, float p_lod, bool p_is_array);
	void copy_depth_to_rect(RID p_source_rd_texture, RID p_dest_framebuffer, const Rect2i &p_rect, bool p_flip_y = false);
	void copy_depth_to_rect_and_linearize(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_rect, bool p_flip_y, float p_z_near, float p_z_far);
	void copy_to_fb_rect(RID p_source_rd_texture, RID p_dest_framebuffer, const Rect2i &p_rect, bool p_flip_y = false, bool p_force_luminance = false, bool p_alpha_to_zero = false, bool p_srgb = false, RID p_secondary = RID(), bool p_multiview = false, bool alpha_to_one = false, bool p_linear = false, bool p_normal = false, const Rect2 &p_src_rect = Rect2());
	void copy_to_atlas_fb(RID p_source_rd_texture, RID p_dest_framebuffer, const Rect2 &p_uv_rect, RD::DrawListID p_draw_list, bool p_flip_y = false, bool p_panorama = false);
	void copy_to_drawlist(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, RID p_source_rd_texture, bool p_linear = false);
	void copy_raster(RID p_source_texture, RID p_dest_framebuffer);

	void gaussian_blur(RID p_source_rd_texture, RID p_texture, const Rect2i &p_region, const Size2i &p_size, bool p_8bit_dst = false);
	void gaussian_blur_raster(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_region, const Size2i &p_size);
	void gaussian_glow(RID p_source_rd_texture, RID p_back_texture, const Size2i &p_size, float p_strength = 1.0, bool p_first_pass = false, float p_luminance_cap = 16.0, float p_exposure = 1.0, float p_bloom = 0.0, float p_hdr_bleed_threshold = 1.0, float p_hdr_bleed_scale = 1.0, RID p_auto_exposure = RID(), float p_auto_exposure_scale = 1.0);
	void gaussian_glow_raster(RID p_source_rd_texture, RID p_half_texture, RID p_dest_texture, float p_luminance_multiplier, const Size2i &p_size, float p_strength = 1.0, bool p_first_pass = false, float p_luminance_cap = 16.0, float p_exposure = 1.0, float p_bloom = 0.0, float p_hdr_bleed_threshold = 1.0, float p_hdr_bleed_scale = 1.0, RID p_auto_exposure = RID(), float p_auto_exposure_scale = 1.0);

	void make_mipmap(RID p_source_rd_texture, RID p_dest_texture, const Size2i &p_size);
	void make_mipmap_raster(RID p_source_rd_texture, RID p_dest_texture, const Size2i &p_size);

	void set_color(RID p_dest_texture, const Color &p_color, const Rect2i &p_region, bool p_8bit_dst = false);
	void set_color_raster(RID p_dest_texture, const Color &p_color, const Rect2i &p_region);

	void copy_cubemap_to_dp(RID p_source_rd_texture, RID p_dst_framebuffer, const Rect2 &p_rect, const Vector2 &p_dst_size, float p_z_near, float p_z_far, bool p_dp_flip);
	void cubemap_downsample(RID p_source_cubemap, RID p_dest_cubemap, const Size2i &p_size);
	void cubemap_downsample_raster(RID p_source_cubemap, RID p_dest_framebuffer, uint32_t p_face_id, const Size2i &p_size);
	void cubemap_filter(RID p_source_cubemap, Vector<RID> p_dest_cubemap, bool p_use_array);
	void cubemap_filter_raster(RID p_source_cubemap, RID p_dest_framebuffer, uint32_t p_face_id, uint32_t p_mip_level);

	void cubemap_roughness(RID p_source_rd_texture, RID p_dest_texture, uint32_t p_face_id, uint32_t p_sample_count, float p_roughness, float p_size);
	void cubemap_roughness_raster(RID p_source_rd_texture, RID p_dest_framebuffer, uint32_t p_face_id, uint32_t p_sample_count, float p_roughness, float p_size);

	void merge_specular(RID p_dest_framebuffer, RID p_specular, RID p_base, RID p_reflection, uint32_t p_view_count);
};

} // namespace RendererRD
