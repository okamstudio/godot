/**************************************************************************/
/*  copy_effects.cpp                                                      */
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

#include "copy_effects.h"
#include "core/config/project_settings.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "thirdparty/misc/cubemap_coeffs.h"

using namespace RendererRD;

CopyEffects *CopyEffects::singleton = nullptr;

CopyEffects *CopyEffects::get_singleton() {
	return singleton;
}

CopyEffects::CopyEffects(bool p_prefer_raster_effects) {
	singleton = this;
	prefer_raster_effects = p_prefer_raster_effects;

	if (prefer_raster_effects) {
		// init blur shader (on compute use copy shader)

		Vector<String> blur_modes;
		blur_modes.push_back("\n#define MODE_MIPMAP\n"); // BLUR_MIPMAP
		blur_modes.push_back("\n#define MODE_GAUSSIAN_BLUR\n"); // BLUR_MODE_GAUSSIAN_BLUR
		blur_modes.push_back("\n#define MODE_GAUSSIAN_GLOW\n"); // BLUR_MODE_GAUSSIAN_GLOW
		blur_modes.push_back("\n#define MODE_GAUSSIAN_GLOW\n#define GLOW_USE_AUTO_EXPOSURE\n"); // BLUR_MODE_GAUSSIAN_GLOW_AUTO_EXPOSURE
		blur_modes.push_back("\n#define MODE_COPY\n"); // BLUR_MODE_COPY
		blur_modes.push_back("\n#define MODE_SET_COLOR\n"); // BLUR_MODE_SET_COLOR

		blur_raster.shader.initialize(blur_modes);
		memset(&blur_raster.push_constant, 0, sizeof(BlurRasterPushConstant));
		blur_raster.shader_version = blur_raster.shader.version_create();

		for (int i = 0; i < BLUR_MODE_MAX; i++) {
			blur_raster.pipelines[i].setup(blur_raster.shader.version_get_shader(blur_raster.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		}

	} else {
		// not used in clustered
		for (int i = 0; i < BLUR_MODE_MAX; i++) {
			blur_raster.pipelines[i].clear();
		}
	}

	{
		Vector<String> copy_modes;
		copy_modes.push_back("\n#define MODE_GAUSSIAN_BLUR\n");
		copy_modes.push_back("\n#define MODE_GAUSSIAN_BLUR\n#define DST_IMAGE_8BIT\n");
		copy_modes.push_back("\n#define MODE_GAUSSIAN_BLUR\n#define MODE_GLOW\n");
		copy_modes.push_back("\n#define MODE_GAUSSIAN_BLUR\n#define MODE_GLOW\n#define GLOW_USE_AUTO_EXPOSURE\n");
		copy_modes.push_back("\n#define MODE_SIMPLE_COPY\n");
		copy_modes.push_back("\n#define MODE_SIMPLE_COPY\n#define DST_IMAGE_8BIT\n");
		copy_modes.push_back("\n#define MODE_SIMPLE_COPY_DEPTH\n");
		copy_modes.push_back("\n#define MODE_SET_COLOR\n");
		copy_modes.push_back("\n#define MODE_SET_COLOR\n#define DST_IMAGE_8BIT\n");
		copy_modes.push_back("\n#define MODE_MIPMAP\n");
		copy_modes.push_back("\n#define MODE_LINEARIZE_DEPTH_COPY\n");
		copy_modes.push_back("\n#define MODE_CUBEMAP_TO_PANORAMA\n");
		copy_modes.push_back("\n#define MODE_CUBEMAP_ARRAY_TO_PANORAMA\n");

		copy.shader.initialize(copy_modes);
		memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

		copy.shader_version = copy.shader.version_create();

		for (int i = 0; i < COPY_MODE_MAX; i++) {
			if (copy.shader.is_variant_enabled(i)) {
				copy.pipelines[i] = RD::get_singleton()->compute_pipeline_create(copy.shader.version_get_shader(copy.shader_version, i));
			}
		}
	}

	{
		Vector<String> copy_modes;
		copy_modes.push_back("\n"); // COPY_TO_FB_COPY
		copy_modes.push_back("\n#define MODE_PANORAMA_TO_DP\n"); // COPY_TO_FB_COPY_PANORAMA_TO_DP
		copy_modes.push_back("\n#define MODE_TWO_SOURCES\n"); // COPY_TO_FB_COPY2
		copy_modes.push_back("\n#define MODE_SET_COLOR\n"); // COPY_TO_FB_SET_COLOR
		copy_modes.push_back("\n#define USE_MULTIVIEW\n"); // COPY_TO_FB_MULTIVIEW
		copy_modes.push_back("\n#define USE_MULTIVIEW\n#define MODE_TWO_SOURCES\n"); // COPY_TO_FB_MULTIVIEW_WITH_DEPTH

		copy_to_fb.shader.initialize(copy_modes);

		if (!RendererCompositorRD::get_singleton()->is_xr_enabled()) {
			copy_to_fb.shader.set_variant_enabled(COPY_TO_FB_MULTIVIEW, false);
			copy_to_fb.shader.set_variant_enabled(COPY_TO_FB_MULTIVIEW_WITH_DEPTH, false);
		}

		copy_to_fb.shader_version = copy_to_fb.shader.version_create();

		//use additive

		for (int i = 0; i < COPY_TO_FB_MAX; i++) {
			if (copy_to_fb.shader.is_variant_enabled(i)) {
				copy_to_fb.pipelines[i].setup(copy_to_fb.shader.version_get_shader(copy_to_fb.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
			} else {
				copy_to_fb.pipelines[i].clear();
			}
		}
	}

	{
		// Initialize copier
		Vector<String> copy_modes;
		copy_modes.push_back("\n");

		cube_to_dp.shader.initialize(copy_modes);

		cube_to_dp.shader_version = cube_to_dp.shader.version_create();
		RID shader = cube_to_dp.shader.version_get_shader(cube_to_dp.shader_version, 0);
		RD::PipelineDepthStencilState dss;
		dss.enable_depth_test = true;
		dss.depth_compare_operator = RD::COMPARE_OP_ALWAYS;
		dss.enable_depth_write = true;
		cube_to_dp.pipeline.setup(shader, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), dss, RD::PipelineColorBlendState(), 0);
	}

	{
		//Initialize cubemap downsampler
		Vector<String> cubemap_downsampler_modes;
		cubemap_downsampler_modes.push_back("");

		if (prefer_raster_effects) {
			cubemap_downsampler.raster_shader.initialize(cubemap_downsampler_modes);

			cubemap_downsampler.shader_version = cubemap_downsampler.raster_shader.version_create();

			cubemap_downsampler.raster_pipeline.setup(cubemap_downsampler.raster_shader.version_get_shader(cubemap_downsampler.shader_version, 0), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		} else {
			cubemap_downsampler.compute_shader.initialize(cubemap_downsampler_modes);

			cubemap_downsampler.shader_version = cubemap_downsampler.compute_shader.version_create();

			cubemap_downsampler.compute_pipeline = RD::get_singleton()->compute_pipeline_create(cubemap_downsampler.compute_shader.version_get_shader(cubemap_downsampler.shader_version, 0));
			cubemap_downsampler.raster_pipeline.clear();
		}
	}

	{
		// Initialize cubemap filter
		filter.use_high_quality = GLOBAL_GET("rendering/reflections/sky_reflections/fast_filter_high_quality");

		Vector<String> cubemap_filter_modes;
		cubemap_filter_modes.push_back("\n#define USE_HIGH_QUALITY\n");
		cubemap_filter_modes.push_back("\n#define USE_LOW_QUALITY\n");
		cubemap_filter_modes.push_back("\n#define USE_HIGH_QUALITY\n#define USE_TEXTURE_ARRAY\n");
		cubemap_filter_modes.push_back("\n#define USE_LOW_QUALITY\n#define USE_TEXTURE_ARRAY\n");

		if (filter.use_high_quality) {
			filter.coefficient_buffer = RD::get_singleton()->storage_buffer_create(sizeof(high_quality_coeffs));
			RD::get_singleton()->buffer_update(filter.coefficient_buffer, 0, sizeof(high_quality_coeffs), &high_quality_coeffs[0]);
		} else {
			filter.coefficient_buffer = RD::get_singleton()->storage_buffer_create(sizeof(low_quality_coeffs));
			RD::get_singleton()->buffer_update(filter.coefficient_buffer, 0, sizeof(low_quality_coeffs), &low_quality_coeffs[0]);
		}

		if (prefer_raster_effects) {
			filter.raster_shader.initialize(cubemap_filter_modes);

			// array variants are not supported in raster
			filter.raster_shader.set_variant_enabled(FILTER_MODE_HIGH_QUALITY_ARRAY, false);
			filter.raster_shader.set_variant_enabled(FILTER_MODE_LOW_QUALITY_ARRAY, false);

			filter.shader_version = filter.raster_shader.version_create();

			for (int i = 0; i < FILTER_MODE_MAX; i++) {
				if (filter.raster_shader.is_variant_enabled(i)) {
					filter.raster_pipelines[i].setup(filter.raster_shader.version_get_shader(filter.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
				} else {
					filter.raster_pipelines[i].clear();
				}
			}

			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(filter.coefficient_buffer);
				uniforms.push_back(u);
			}
			filter.uniform_set = RD::get_singleton()->uniform_set_create(uniforms, filter.raster_shader.version_get_shader(filter.shader_version, filter.use_high_quality ? 0 : 1), 1);
		} else {
			filter.compute_shader.initialize(cubemap_filter_modes);
			filter.shader_version = filter.compute_shader.version_create();

			for (int i = 0; i < FILTER_MODE_MAX; i++) {
				filter.compute_pipelines[i] = RD::get_singleton()->compute_pipeline_create(filter.compute_shader.version_get_shader(filter.shader_version, i));
				filter.raster_pipelines[i].clear();
			}

			Vector<RD::Uniform> uniforms;
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(filter.coefficient_buffer);
				uniforms.push_back(u);
			}
			filter.uniform_set = RD::get_singleton()->uniform_set_create(uniforms, filter.compute_shader.version_get_shader(filter.shader_version, filter.use_high_quality ? 0 : 1), 1);
		}
	}

	{
		// Initialize roughness
		Vector<String> cubemap_roughness_modes;
		cubemap_roughness_modes.push_back("");

		if (prefer_raster_effects) {
			roughness.raster_shader.initialize(cubemap_roughness_modes);

			roughness.shader_version = roughness.raster_shader.version_create();

			roughness.raster_pipeline.setup(roughness.raster_shader.version_get_shader(roughness.shader_version, 0), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);

		} else {
			roughness.compute_shader.initialize(cubemap_roughness_modes);

			roughness.shader_version = roughness.compute_shader.version_create();

			roughness.compute_pipeline = RD::get_singleton()->compute_pipeline_create(roughness.compute_shader.version_get_shader(roughness.shader_version, 0));
			roughness.raster_pipeline.clear();
		}
	}

	{
		Vector<String> specular_modes;
		specular_modes.push_back("\n#define MODE_MERGE\n"); // SPECULAR_MERGE_ADD
		specular_modes.push_back("\n#define MODE_MERGE\n#define MODE_SSR\n"); // SPECULAR_MERGE_SSR
		specular_modes.push_back("\n"); // SPECULAR_MERGE_ADDITIVE_ADD
		specular_modes.push_back("\n#define MODE_SSR\n"); // SPECULAR_MERGE_ADDITIVE_SSR

		specular_modes.push_back("\n#define USE_MULTIVIEW\n#define MODE_MERGE\n"); // SPECULAR_MERGE_ADD_MULTIVIEW
		specular_modes.push_back("\n#define USE_MULTIVIEW\n#define MODE_MERGE\n#define MODE_SSR\n"); // SPECULAR_MERGE_SSR_MULTIVIEW
		specular_modes.push_back("\n#define USE_MULTIVIEW\n"); // SPECULAR_MERGE_ADDITIVE_ADD_MULTIVIEW
		specular_modes.push_back("\n#define USE_MULTIVIEW\n#define MODE_SSR\n"); // SPECULAR_MERGE_ADDITIVE_SSR_MULTIVIEW

		specular_merge.shader.initialize(specular_modes);

		if (!RendererCompositorRD::get_singleton()->is_xr_enabled()) {
			specular_merge.shader.set_variant_enabled(SPECULAR_MERGE_ADD_MULTIVIEW, false);
			specular_merge.shader.set_variant_enabled(SPECULAR_MERGE_SSR_MULTIVIEW, false);
			specular_merge.shader.set_variant_enabled(SPECULAR_MERGE_ADDITIVE_ADD_MULTIVIEW, false);
			specular_merge.shader.set_variant_enabled(SPECULAR_MERGE_ADDITIVE_SSR_MULTIVIEW, false);
		}

		specular_merge.shader_version = specular_merge.shader.version_create();

		//use additive

		RD::PipelineColorBlendState::Attachment ba;
		ba.enable_blend = true;
		ba.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
		ba.dst_color_blend_factor = RD::BLEND_FACTOR_ONE;
		ba.src_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
		ba.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
		ba.color_blend_op = RD::BLEND_OP_ADD;
		ba.alpha_blend_op = RD::BLEND_OP_ADD;

		RD::PipelineColorBlendState blend_additive;
		blend_additive.attachments.push_back(ba);

		for (int i = 0; i < SPECULAR_MERGE_MAX; i++) {
			if (specular_merge.shader.is_variant_enabled(i)) {
				RD::PipelineColorBlendState blend_state;
				if (i == SPECULAR_MERGE_ADDITIVE_ADD || i == SPECULAR_MERGE_ADDITIVE_SSR || i == SPECULAR_MERGE_ADDITIVE_ADD_MULTIVIEW || i == SPECULAR_MERGE_ADDITIVE_SSR_MULTIVIEW) {
					blend_state = blend_additive;
				} else {
					blend_state = RD::PipelineColorBlendState::create_disabled();
				}
				specular_merge.pipelines[i].setup(specular_merge.shader.version_get_shader(specular_merge.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), blend_state, 0);
			}
		}
	}
}

CopyEffects::~CopyEffects() {
	if (prefer_raster_effects) {
		blur_raster.shader.version_free(blur_raster.shader_version);
		cubemap_downsampler.raster_shader.version_free(cubemap_downsampler.shader_version);
		filter.raster_shader.version_free(filter.shader_version);
		roughness.raster_shader.version_free(roughness.shader_version);
	} else {
		cubemap_downsampler.compute_shader.version_free(cubemap_downsampler.shader_version);
		filter.compute_shader.version_free(filter.shader_version);
		roughness.compute_shader.version_free(roughness.shader_version);
	}

	copy.shader.version_free(copy.shader_version);
	specular_merge.shader.version_free(specular_merge.shader_version);

	RD::get_singleton()->free(filter.coefficient_buffer);

	if (RD::get_singleton()->uniform_set_is_valid(filter.image_uniform_set)) {
		RD::get_singleton()->free(filter.image_uniform_set);
	}

	if (RD::get_singleton()->uniform_set_is_valid(filter.uniform_set)) {
		RD::get_singleton()->free(filter.uniform_set);
	}

	copy_to_fb.shader.version_free(copy_to_fb.shader_version);
	cube_to_dp.shader.version_free(cube_to_dp.shader_version);

	singleton = nullptr;
}

void CopyEffects::copy_to_rect(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_rect, bool p_flip_y, bool p_force_luminance, bool p_all_source, bool p_8_bit_dst, bool p_alpha_to_one) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));
	if (p_flip_y) {
		copy.push_constant.flags |= COPY_FLAG_FLIP_Y;
	}

	if (p_force_luminance) {
		copy.push_constant.flags |= COPY_FLAG_FORCE_LUMINANCE;
	}

	if (p_all_source) {
		copy.push_constant.flags |= COPY_FLAG_ALL_SOURCE;
	}

	if (p_alpha_to_one) {
		copy.push_constant.flags |= COPY_FLAG_ALPHA_TO_ONE;
	}

	copy.push_constant.section[0] = p_rect.position.x;
	copy.push_constant.section[1] = p_rect.position.y;
	copy.push_constant.section[2] = p_rect.size.width;
	copy.push_constant.section[3] = p_rect.size.height;
	copy.push_constant.target[0] = p_rect.position.x;
	copy.push_constant.target[1] = p_rect.position.y;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_texture);

	CopyMode mode = p_8_bit_dst ? COPY_MODE_SIMPLY_COPY_8BIT : COPY_MODE_SIMPLY_COPY;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_texture), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_rect.size.width, p_rect.size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::copy_cubemap_to_panorama(RID p_source_cube, RID p_dest_panorama, const Size2i &p_panorama_size, float p_lod, bool p_is_array) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

	copy.push_constant.section[0] = 0;
	copy.push_constant.section[1] = 0;
	copy.push_constant.section[2] = p_panorama_size.width;
	copy.push_constant.section[3] = p_panorama_size.height;
	copy.push_constant.target[0] = 0;
	copy.push_constant.target[1] = 0;
	copy.push_constant.camera_z_far = p_lod;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_cube(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_cube }));
	RD::Uniform u_dest_panorama(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_panorama);

	CopyMode mode = p_is_array ? COPY_MODE_CUBE_ARRAY_TO_PANORAMA : COPY_MODE_CUBE_TO_PANORAMA;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_cube), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_panorama), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_panorama_size.width, p_panorama_size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::copy_depth_to_rect(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_rect, bool p_flip_y) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));
	if (p_flip_y) {
		copy.push_constant.flags |= COPY_FLAG_FLIP_Y;
	}

	copy.push_constant.section[0] = 0;
	copy.push_constant.section[1] = 0;
	copy.push_constant.section[2] = p_rect.size.width;
	copy.push_constant.section[3] = p_rect.size.height;
	copy.push_constant.target[0] = p_rect.position.x;
	copy.push_constant.target[1] = p_rect.position.y;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_texture);

	CopyMode mode = COPY_MODE_SIMPLY_COPY_DEPTH;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_texture), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_rect.size.width, p_rect.size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::copy_depth_to_rect_and_linearize(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_rect, bool p_flip_y, float p_z_near, float p_z_far) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));
	if (p_flip_y) {
		copy.push_constant.flags |= COPY_FLAG_FLIP_Y;
	}

	copy.push_constant.section[0] = 0;
	copy.push_constant.section[1] = 0;
	copy.push_constant.section[2] = p_rect.size.width;
	copy.push_constant.section[3] = p_rect.size.height;
	copy.push_constant.target[0] = p_rect.position.x;
	copy.push_constant.target[1] = p_rect.position.y;
	copy.push_constant.camera_z_far = p_z_far;
	copy.push_constant.camera_z_near = p_z_near;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_texture);

	CopyMode mode = COPY_MODE_LINEARIZE_DEPTH;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_texture), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_rect.size.width, p_rect.size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::copy_to_atlas_fb(RID p_source_rd_texture, RID p_dest_framebuffer, const Rect2 &p_uv_rect, RD::DrawListID p_draw_list, bool p_flip_y, bool p_panorama) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy_to_fb.push_constant, 0, sizeof(CopyToFbPushConstant));

	copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_USE_SECTION;
	copy_to_fb.push_constant.section[0] = p_uv_rect.position.x;
	copy_to_fb.push_constant.section[1] = p_uv_rect.position.y;
	copy_to_fb.push_constant.section[2] = p_uv_rect.size.x;
	copy_to_fb.push_constant.section[3] = p_uv_rect.size.y;

	if (p_flip_y) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_FLIP_Y;
	}

	copy_to_fb.push_constant.luminance_multiplier = 1.0;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	CopyToFBMode mode = p_panorama ? COPY_TO_FB_COPY_PANORAMA_TO_DP : COPY_TO_FB_COPY;
	RID shader = copy_to_fb.shader.version_get_shader(copy_to_fb.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = p_draw_list;
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, copy_to_fb.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->draw_list_bind_index_array(draw_list, material_storage->get_quad_index_array());
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &copy_to_fb.push_constant, sizeof(CopyToFbPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, true);
}

void CopyEffects::copy_to_fb_rect(RID p_source_rd_texture, RID p_dest_framebuffer, const Rect2i &p_rect, bool p_flip_y, bool p_force_luminance, bool p_alpha_to_zero, bool p_srgb, RID p_secondary, bool p_multiview, bool p_alpha_to_one, bool p_linear, bool p_normal, const Rect2 &p_src_rect) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy_to_fb.push_constant, 0, sizeof(CopyToFbPushConstant));
	copy_to_fb.push_constant.luminance_multiplier = 1.0;

	if (p_flip_y) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_FLIP_Y;
	}
	if (p_force_luminance) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_FORCE_LUMINANCE;
	}
	if (p_alpha_to_zero) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_ALPHA_TO_ZERO;
	}
	if (p_srgb) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_SRGB;
	}
	if (p_alpha_to_one) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_ALPHA_TO_ONE;
	}
	if (p_linear) {
		// Used for copying to a linear buffer. In the mobile renderer we divide the contents of the linear buffer
		// to allow for a wider effective range.
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_LINEAR;
		copy_to_fb.push_constant.luminance_multiplier = prefer_raster_effects ? 2.0 : 1.0;
	}

	if (p_normal) {
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_NORMAL;
	}

	if (p_src_rect != Rect2()) {
		copy_to_fb.push_constant.section[0] = p_src_rect.position.x;
		copy_to_fb.push_constant.section[1] = p_src_rect.position.y;
		copy_to_fb.push_constant.section[2] = p_src_rect.size.x;
		copy_to_fb.push_constant.section[3] = p_src_rect.size.y;
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_USE_SRC_SECTION;
	}

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	CopyToFBMode mode;
	if (p_multiview) {
		mode = p_secondary.is_valid() ? COPY_TO_FB_MULTIVIEW_WITH_DEPTH : COPY_TO_FB_MULTIVIEW;
	} else {
		mode = p_secondary.is_valid() ? COPY_TO_FB_COPY2 : COPY_TO_FB_COPY;
	}

	RID shader = copy_to_fb.shader.version_get_shader(copy_to_fb.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer, RD::DRAW_DEFAULT_ALL, Vector<Color>(), 1.0f, 0, p_rect);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, copy_to_fb.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	if (p_secondary.is_valid()) {
		// TODO may need to do this differently when reading from depth buffer for multiview
		RD::Uniform u_secondary(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_secondary }));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_secondary), 1);
	}
	RD::get_singleton()->draw_list_bind_index_array(draw_list, material_storage->get_quad_index_array());
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &copy_to_fb.push_constant, sizeof(CopyToFbPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, true);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::copy_to_drawlist(RD::DrawListID p_draw_list, RD::FramebufferFormatID p_fb_format, RID p_source_rd_texture, bool p_linear) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy_to_fb.push_constant, 0, sizeof(CopyToFbPushConstant));
	copy_to_fb.push_constant.luminance_multiplier = 1.0;

	if (p_linear) {
		// Used for copying to a linear buffer. In the mobile renderer we divide the contents of the linear buffer
		// to allow for a wider effective range.
		copy_to_fb.push_constant.flags |= COPY_TO_FB_FLAG_LINEAR;
		copy_to_fb.push_constant.luminance_multiplier = prefer_raster_effects ? 2.0 : 1.0;
	}

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	// Multiview not supported here!
	CopyToFBMode mode = COPY_TO_FB_COPY;

	RID shader = copy_to_fb.shader.version_get_shader(copy_to_fb.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, copy_to_fb.pipelines[mode].get_render_pipeline(RD::INVALID_ID, p_fb_format));
	RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->draw_list_bind_index_array(p_draw_list, material_storage->get_quad_index_array());
	RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &copy_to_fb.push_constant, sizeof(CopyToFbPushConstant));
	RD::get_singleton()->draw_list_draw(p_draw_list, true);
}

void CopyEffects::copy_raster(RID p_source_texture, RID p_dest_framebuffer) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use the raster version of the copy with the clustered renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&blur_raster.push_constant, 0, sizeof(BlurRasterPushConstant));

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_texture }));

	RID shader = blur_raster.shader.version_get_shader(blur_raster.shader_version, BLUR_MODE_COPY);
	ERR_FAIL_COND(shader.is_null());

	// Just copy it back (we use our blur raster shader here)..
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blur_raster.pipelines[BLUR_MODE_COPY].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_texture), 0);
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blur_raster.push_constant, sizeof(BlurRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::gaussian_blur(RID p_source_rd_texture, RID p_texture, const Rect2i &p_region, const Size2i &p_size, bool p_8bit_dst) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use the compute version of the gaussian blur with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

	copy.push_constant.section[0] = p_region.position.x;
	copy.push_constant.section[1] = p_region.position.y;
	copy.push_constant.target[0] = p_region.position.x;
	copy.push_constant.target[1] = p_region.position.y;
	copy.push_constant.section[2] = p_size.width;
	copy.push_constant.section[3] = p_size.height;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_texture);

	CopyMode mode = p_8bit_dst ? COPY_MODE_GAUSSIAN_COPY_8BIT : COPY_MODE_GAUSSIAN_COPY;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_texture), 3);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_region.size.width, p_region.size.height, 1);

	RD::get_singleton()->compute_list_end();
}

void CopyEffects::gaussian_blur_raster(RID p_source_rd_texture, RID p_dest_texture, const Rect2i &p_region, const Size2i &p_size) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use the raster version of the gaussian blur with the clustered renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	RID dest_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_dest_texture);

	memset(&blur_raster.push_constant, 0, sizeof(BlurRasterPushConstant));

	BlurRasterMode blur_mode = BLUR_MODE_GAUSSIAN_BLUR;

	blur_raster.push_constant.pixel_size[0] = 1.0 / float(p_size.x);
	blur_raster.push_constant.pixel_size[1] = 1.0 / float(p_size.y);

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	RID shader = blur_raster.shader.version_get_shader(blur_raster.shader_version, blur_mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blur_raster.pipelines[blur_mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blur_raster.push_constant, sizeof(BlurRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::gaussian_glow(RID p_source_rd_texture, RID p_back_texture, const Size2i &p_size, float p_strength, bool p_first_pass, float p_luminance_cap, float p_exposure, float p_bloom, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, RID p_auto_exposure, float p_auto_exposure_scale) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use the compute version of the gaussian glow with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

	CopyMode copy_mode = p_first_pass && p_auto_exposure.is_valid() ? COPY_MODE_GAUSSIAN_GLOW_AUTO_EXPOSURE : COPY_MODE_GAUSSIAN_GLOW;
	uint32_t base_flags = 0;

	copy.push_constant.section[2] = p_size.x;
	copy.push_constant.section[3] = p_size.y;

	copy.push_constant.glow_strength = p_strength;
	copy.push_constant.glow_bloom = p_bloom;
	copy.push_constant.glow_hdr_threshold = p_hdr_bleed_threshold;
	copy.push_constant.glow_hdr_scale = p_hdr_bleed_scale;
	copy.push_constant.glow_exposure = p_exposure;
	copy.push_constant.glow_white = 0; //actually unused
	copy.push_constant.glow_luminance_cap = p_luminance_cap;

	copy.push_constant.glow_auto_exposure_scale = p_auto_exposure_scale; //unused also

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_back_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_back_texture);

	RID shader = copy.shader.version_get_shader(copy.shader_version, copy_mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[copy_mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_back_texture), 3);
	if (p_auto_exposure.is_valid() && p_first_pass) {
		RD::Uniform u_auto_exposure(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_auto_exposure }));
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_auto_exposure), 1);
	}

	copy.push_constant.flags = base_flags | (p_first_pass ? COPY_FLAG_GLOW_FIRST_PASS : 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_size.width, p_size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::gaussian_glow_raster(RID p_source_rd_texture, RID p_half_texture, RID p_dest_texture, float p_luminance_multiplier, const Size2i &p_size, float p_strength, bool p_first_pass, float p_luminance_cap, float p_exposure, float p_bloom, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, RID p_auto_exposure, float p_auto_exposure_scale) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use the raster version of the gaussian glow with the clustered renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	RID half_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_half_texture);
	RID dest_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_dest_texture);

	memset(&blur_raster.push_constant, 0, sizeof(BlurRasterPushConstant));

	BlurRasterMode blur_mode = p_first_pass && p_auto_exposure.is_valid() ? BLUR_MODE_GAUSSIAN_GLOW_AUTO_EXPOSURE : BLUR_MODE_GAUSSIAN_GLOW;
	uint32_t base_flags = 0;

	blur_raster.push_constant.pixel_size[0] = 1.0 / float(p_size.x);
	blur_raster.push_constant.pixel_size[1] = 1.0 / float(p_size.y);

	blur_raster.push_constant.glow_strength = p_strength;
	blur_raster.push_constant.glow_bloom = p_bloom;
	blur_raster.push_constant.glow_hdr_threshold = p_hdr_bleed_threshold;
	blur_raster.push_constant.glow_hdr_scale = p_hdr_bleed_scale;
	blur_raster.push_constant.glow_exposure = p_exposure;
	blur_raster.push_constant.glow_white = 0; //actually unused
	blur_raster.push_constant.glow_luminance_cap = p_luminance_cap;

	blur_raster.push_constant.glow_auto_exposure_scale = p_auto_exposure_scale; //unused also

	blur_raster.push_constant.luminance_multiplier = p_luminance_multiplier;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_half_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_half_texture }));

	RID shader = blur_raster.shader.version_get_shader(blur_raster.shader_version, blur_mode);
	ERR_FAIL_COND(shader.is_null());

	//HORIZONTAL
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(half_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blur_raster.pipelines[blur_mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(half_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	if (p_auto_exposure.is_valid() && p_first_pass) {
		RD::Uniform u_auto_exposure(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_auto_exposure }));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_auto_exposure), 1);
	}

	blur_raster.push_constant.flags = base_flags | BLUR_FLAG_HORIZONTAL | (p_first_pass ? BLUR_FLAG_GLOW_FIRST_PASS : 0);
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blur_raster.push_constant, sizeof(BlurRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();

	blur_mode = BLUR_MODE_GAUSSIAN_GLOW;

	shader = blur_raster.shader.version_get_shader(blur_raster.shader_version, blur_mode);
	ERR_FAIL_COND(shader.is_null());

	//VERTICAL
	draw_list = RD::get_singleton()->draw_list_begin(dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blur_raster.pipelines[blur_mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_half_texture), 0);

	blur_raster.push_constant.flags = base_flags;
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blur_raster.push_constant, sizeof(BlurRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::make_mipmap(RID p_source_rd_texture, RID p_dest_texture, const Size2i &p_size) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use the compute version of the make_mipmap shader with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

	copy.push_constant.section[0] = 0;
	copy.push_constant.section[1] = 0;
	copy.push_constant.section[2] = p_size.width;
	copy.push_constant.section[3] = p_size.height;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_texture);

	CopyMode mode = COPY_MODE_MIPMAP;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_texture), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_size.width, p_size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::make_mipmap_raster(RID p_source_rd_texture, RID p_dest_texture, const Size2i &p_size) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use the raster version of mipmap with the clustered renderer.");

	RID dest_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_dest_texture);

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&blur_raster.push_constant, 0, sizeof(BlurRasterPushConstant));

	BlurRasterMode mode = BLUR_MIPMAP;

	blur_raster.push_constant.pixel_size[0] = 1.0 / float(p_size.x);
	blur_raster.push_constant.pixel_size[1] = 1.0 / float(p_size.y);

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	RID shader = blur_raster.shader.version_get_shader(blur_raster.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, blur_raster.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &blur_raster.push_constant, sizeof(BlurRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::set_color(RID p_dest_texture, const Color &p_color, const Rect2i &p_region, bool p_8bit_dst) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use the compute version of the set_color shader with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);

	memset(&copy.push_constant, 0, sizeof(CopyPushConstant));

	copy.push_constant.section[0] = 0;
	copy.push_constant.section[1] = 0;
	copy.push_constant.section[2] = p_region.size.width;
	copy.push_constant.section[3] = p_region.size.height;
	copy.push_constant.target[0] = p_region.position.x;
	copy.push_constant.target[1] = p_region.position.y;
	copy.push_constant.set_color[0] = p_color.r;
	copy.push_constant.set_color[1] = p_color.g;
	copy.push_constant.set_color[2] = p_color.b;
	copy.push_constant.set_color[3] = p_color.a;

	// setup our uniforms
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_dest_texture);

	CopyMode mode = p_8bit_dst ? COPY_MODE_SET_COLOR_8BIT : COPY_MODE_SET_COLOR;
	RID shader = copy.shader.version_get_shader(copy.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, copy.pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_dest_texture), 3);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy.push_constant, sizeof(CopyPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_region.size.width, p_region.size.height, 1);
	RD::get_singleton()->compute_list_end();
}

void CopyEffects::set_color_raster(RID p_dest_texture, const Color &p_color, const Rect2i &p_region) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use the raster version of the set_color shader with the clustered renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&copy_to_fb.push_constant, 0, sizeof(CopyToFbPushConstant));

	copy_to_fb.push_constant.set_color[0] = p_color.r;
	copy_to_fb.push_constant.set_color[1] = p_color.g;
	copy_to_fb.push_constant.set_color[2] = p_color.b;
	copy_to_fb.push_constant.set_color[3] = p_color.a;

	RID dest_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_dest_texture);

	CopyToFBMode mode = COPY_TO_FB_SET_COLOR;

	RID shader = copy_to_fb.shader.version_get_shader(copy_to_fb.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(dest_framebuffer, RD::DRAW_DEFAULT_ALL, Vector<Color>(), 1.0f, 0, p_region);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, copy_to_fb.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_index_array(draw_list, material_storage->get_quad_index_array());
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &copy_to_fb.push_constant, sizeof(CopyToFbPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, true);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::copy_cubemap_to_dp(RID p_source_rd_texture, RID p_dst_framebuffer, const Rect2 &p_rect, const Vector2 &p_dst_size, float p_z_near, float p_z_far, bool p_dp_flip) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	Rect2i screen_rect;
	float atlas_width = p_dst_size.width / p_rect.size.width;
	float atlas_height = p_dst_size.height / p_rect.size.height;
	screen_rect.position.x = (int32_t)(Math::round(p_rect.position.x * atlas_width));
	screen_rect.position.y = (int32_t)(Math::round(p_rect.position.y * atlas_height));
	screen_rect.size.width = (int32_t)(Math::round(p_dst_size.width));
	screen_rect.size.height = (int32_t)(Math::round(p_dst_size.height));

	CopyToDPPushConstant push_constant;
	push_constant.z_far = p_z_far;
	push_constant.z_near = p_z_near;
	push_constant.texel_size[0] = 1.0f / p_dst_size.width;
	push_constant.texel_size[1] = 1.0f / p_dst_size.height;
	push_constant.texel_size[0] *= p_dp_flip ? -1.0f : 1.0f; // Encode dp flip as x size sign

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	RID shader = cube_to_dp.shader.version_get_shader(cube_to_dp.shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dst_framebuffer, RD::DRAW_DEFAULT_ALL, Vector<Color>(), 1.0f, 0, screen_rect);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, cube_to_dp.pipeline.get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dst_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->draw_list_bind_index_array(draw_list, material_storage->get_quad_index_array());

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(CopyToDPPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, true);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::cubemap_downsample(RID p_source_cubemap, RID p_dest_cubemap, const Size2i &p_size) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute based cubemap downsample with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	cubemap_downsampler.push_constant.face_size = p_size.x;
	cubemap_downsampler.push_constant.face_id = 0; // we render all 6 sides to each layer in one call

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_cubemap(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_cubemap }));
	RD::Uniform u_dest_cubemap(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ p_dest_cubemap }));

	RID shader = cubemap_downsampler.compute_shader.version_get_shader(cubemap_downsampler.shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, cubemap_downsampler.compute_pipeline);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_cubemap), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest_cubemap), 1);

	int x_groups = Math::division_round_up(p_size.x, 8);
	int y_groups = Math::division_round_up(p_size.y, 8);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &cubemap_downsampler.push_constant, sizeof(CubemapDownsamplerPushConstant));

	RD::get_singleton()->compute_list_dispatch(compute_list, x_groups, y_groups, 6); // one z_group for each face

	RD::get_singleton()->compute_list_end();
}

void CopyEffects::cubemap_downsample_raster(RID p_source_cubemap, RID p_dest_framebuffer, uint32_t p_face_id, const Size2i &p_size) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use raster based cubemap downsample with the clustered renderer.");
	ERR_FAIL_COND_MSG(p_face_id >= 6, "Raster implementation of cubemap downsample must process one side at a time.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	cubemap_downsampler.push_constant.face_size = p_size.x;
	cubemap_downsampler.push_constant.face_id = p_face_id;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_cubemap(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_cubemap }));

	RID shader = cubemap_downsampler.raster_shader.version_get_shader(cubemap_downsampler.shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, cubemap_downsampler.raster_pipeline.get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_cubemap), 0);

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &cubemap_downsampler.push_constant, sizeof(CubemapDownsamplerPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::cubemap_filter(RID p_source_cubemap, Vector<RID> p_dest_cubemap, bool p_use_array) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute based cubemap filter with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	Vector<RD::Uniform> uniforms;
	for (int i = 0; i < p_dest_cubemap.size(); i++) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
		u.binding = i;
		u.append_id(p_dest_cubemap[i]);
		uniforms.push_back(u);
	}
	if (RD::get_singleton()->uniform_set_is_valid(filter.image_uniform_set)) {
		RD::get_singleton()->free(filter.image_uniform_set);
	}
	filter.image_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, filter.compute_shader.version_get_shader(filter.shader_version, 0), 2);

	// setup our uniforms
	RID default_mipmap_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_cubemap(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_mipmap_sampler, p_source_cubemap }));

	int mode = p_use_array ? FILTER_MODE_HIGH_QUALITY_ARRAY : FILTER_MODE_HIGH_QUALITY;
	mode = filter.use_high_quality ? mode : mode + 1;

	RID shader = filter.compute_shader.version_get_shader(filter.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, filter.compute_pipelines[mode]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_cubemap), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, filter.uniform_set, 1);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, filter.image_uniform_set, 2);

	int x_groups = p_use_array ? 1792 : 342; // (128 * 128 * 7) / 64 : (128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2) / 64

	RD::get_singleton()->compute_list_dispatch(compute_list, x_groups, 6, 1); // one y_group for each face

	RD::get_singleton()->compute_list_end();
}

void CopyEffects::cubemap_filter_raster(RID p_source_cubemap, RID p_dest_framebuffer, uint32_t p_face_id, uint32_t p_mip_level) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use raster based cubemap filter with the clustered renderer.");
	ERR_FAIL_COND_MSG(p_face_id >= 6, "Raster implementation of cubemap filter must process one side at a time.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	// TODO implement!
	CubemapFilterRasterPushConstant push_constant;
	push_constant.mip_level = p_mip_level;
	push_constant.face_id = p_face_id;

	// setup our uniforms
	RID default_mipmap_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_cubemap(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_mipmap_sampler, p_source_cubemap }));

	CubemapFilterMode mode = filter.use_high_quality ? FILTER_MODE_HIGH_QUALITY : FILTER_MODE_LOW_QUALITY;

	RID shader = filter.raster_shader.version_get_shader(filter.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, filter.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_cubemap), 0);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, filter.uniform_set, 1);

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(CubemapFilterRasterPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::cubemap_roughness(RID p_source_rd_texture, RID p_dest_texture, uint32_t p_face_id, uint32_t p_sample_count, float p_roughness, float p_size) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute based cubemap roughness with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&roughness.push_constant, 0, sizeof(CubemapRoughnessPushConstant));

	roughness.push_constant.face_id = p_face_id > 9 ? 0 : p_face_id;
	// Remap to perceptual-roughness^2 to create more detail in lower mips and match the mapping of cubemap_filter.
	roughness.push_constant.roughness = p_roughness * p_roughness;
	roughness.push_constant.sample_count = p_sample_count;
	roughness.push_constant.use_direct_write = p_roughness == 0.0;
	roughness.push_constant.face_size = p_size;

	// setup our uniforms
	RID default_mipmap_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_mipmap_sampler, p_source_rd_texture }));
	RD::Uniform u_dest_texture(RD::UNIFORM_TYPE_IMAGE, 0, Vector<RID>({ p_dest_texture }));

	RID shader = roughness.compute_shader.version_get_shader(roughness.shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, roughness.compute_pipeline);

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_dest_texture), 1);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &roughness.push_constant, sizeof(CubemapRoughnessPushConstant));

	int x_groups = Math::division_round_up(p_size, 8);
	int y_groups = x_groups;

	RD::get_singleton()->compute_list_dispatch(compute_list, x_groups, y_groups, p_face_id > 9 ? 6 : 1);

	RD::get_singleton()->compute_list_end();
}

void CopyEffects::cubemap_roughness_raster(RID p_source_rd_texture, RID p_dest_framebuffer, uint32_t p_face_id, uint32_t p_sample_count, float p_roughness, float p_size) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't use raster based cubemap roughness with the clustered renderer.");
	ERR_FAIL_COND_MSG(p_face_id >= 6, "Raster implementation of cubemap roughness must process one side at a time.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&roughness.push_constant, 0, sizeof(CubemapRoughnessPushConstant));

	roughness.push_constant.face_id = p_face_id;
	roughness.push_constant.roughness = p_roughness * p_roughness; // Shader expects roughness, not perceptual roughness, so multiply before passing in.
	roughness.push_constant.sample_count = p_sample_count;
	roughness.push_constant.use_direct_write = p_roughness == 0.0;
	roughness.push_constant.face_size = p_size;

	// Setup our uniforms.
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	RID shader = roughness.raster_shader.version_get_shader(roughness.shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, roughness.raster_pipeline.get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);

	RD::get_singleton()->draw_list_set_push_constant(draw_list, &roughness.push_constant, sizeof(CubemapRoughnessPushConstant));

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

void CopyEffects::merge_specular(RID p_dest_framebuffer, RID p_specular, RID p_base, RID p_reflection, uint32_t p_view_count) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::get_singleton()->draw_command_begin_label("Merge specular");

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);

	int mode;
	if (p_reflection.is_valid()) {
		if (p_base.is_valid()) {
			mode = SPECULAR_MERGE_SSR;
		} else {
			mode = SPECULAR_MERGE_ADDITIVE_SSR;
		}
	} else {
		if (p_base.is_valid()) {
			mode = SPECULAR_MERGE_ADD;
		} else {
			mode = SPECULAR_MERGE_ADDITIVE_ADD;
		}
	}

	if (p_view_count > 1) {
		mode += SPECULAR_MERGE_ADD_MULTIVIEW;
	}

	RID shader = specular_merge.shader.version_get_shader(specular_merge.shader_version, mode);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, specular_merge.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));

	if (p_base.is_valid()) {
		RD::Uniform u_base(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_base }));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 2, u_base), 2);
	}

	RD::Uniform u_specular(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_specular }));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_specular), 0);

	if (p_reflection.is_valid()) {
		RD::Uniform u_reflection(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_reflection }));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_reflection), 1);
	}

	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();

	RD::get_singleton()->draw_command_end_label();
}
