/**************************************************************************/
/*  bokeh_dof.cpp                                                         */
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

#include "bokeh_dof.h"
#include "copy_effects.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/storage/camera_attributes_storage.h"

using namespace RendererRD;

BokehDOF::BokehDOF(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;

	// Initialize bokeh
	Vector<String> bokeh_modes;
	bokeh_modes.push_back("\n#define MODE_GEN_BLUR_SIZE\n");
	bokeh_modes.push_back("\n#define MODE_BOKEH_BOX\n#define OUTPUT_WEIGHT\n");
	bokeh_modes.push_back("\n#define MODE_BOKEH_BOX\n");
	bokeh_modes.push_back("\n#define MODE_BOKEH_HEXAGONAL\n#define OUTPUT_WEIGHT\n");
	bokeh_modes.push_back("\n#define MODE_BOKEH_HEXAGONAL\n");
	bokeh_modes.push_back("\n#define MODE_BOKEH_CIRCULAR\n#define OUTPUT_WEIGHT\n");
	bokeh_modes.push_back("\n#define MODE_COMPOSITE_BOKEH\n");
	if (prefer_raster_effects) {
		bokeh.raster_shader.initialize(bokeh_modes);

		bokeh.shader_version = bokeh.raster_shader.version_create();

		const int att_count[BOKEH_MAX] = { 1, 2, 1, 2, 1, 2, 1 };
		for (int i = 0; i < BOKEH_MAX; i++) {
			RD::PipelineColorBlendState blend_state = (i == BOKEH_COMPOSITE) ? RD::PipelineColorBlendState::create_blend(att_count[i]) : RD::PipelineColorBlendState::create_disabled(att_count[i]);
			bokeh.raster_pipelines[i].setup(bokeh.raster_shader.version_get_shader(bokeh.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), blend_state, 0);
		}
	} else {
		bokeh.compute_shader.initialize(bokeh_modes);
		bokeh.compute_shader.set_variant_enabled(BOKEH_GEN_BOKEH_BOX_NOWEIGHT, false);
		bokeh.compute_shader.set_variant_enabled(BOKEH_GEN_BOKEH_HEXAGONAL_NOWEIGHT, false);
		bokeh.shader_version = bokeh.compute_shader.version_create();

		for (int i = 0; i < BOKEH_MAX; i++) {
			if (bokeh.compute_shader.is_variant_enabled(i)) {
				bokeh.compute_pipelines[i] = RD::get_singleton()->compute_pipeline_create(bokeh.compute_shader.version_get_shader(bokeh.shader_version, i));
			}
		}

		for (int i = 0; i < BOKEH_MAX; i++) {
			bokeh.raster_pipelines[i].clear();
		}
	}
}

BokehDOF::~BokehDOF() {
	if (prefer_raster_effects) {
		bokeh.raster_shader.version_free(bokeh.shader_version);
	} else {
		bokeh.compute_shader.version_free(bokeh.shader_version);
	}
}

void BokehDOF::bokeh_dof_compute(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal) {
	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	bool dof_far = RSG::camera_attributes->camera_attributes_get_dof_far_enabled(p_camera_attributes);
	float dof_far_begin = RSG::camera_attributes->camera_attributes_get_dof_far_distance(p_camera_attributes);
	float dof_far_size = RSG::camera_attributes->camera_attributes_get_dof_far_transition(p_camera_attributes);
	bool dof_near = RSG::camera_attributes->camera_attributes_get_dof_near_enabled(p_camera_attributes);
	float dof_near_begin = RSG::camera_attributes->camera_attributes_get_dof_near_distance(p_camera_attributes);
	float dof_near_size = RSG::camera_attributes->camera_attributes_get_dof_near_transition(p_camera_attributes);
	float bokeh_size = RSG::camera_attributes->camera_attributes_get_dof_blur_amount(p_camera_attributes) * 64; // Base 64 pixel radius.

	bool use_jitter = RSG::camera_attributes->camera_attributes_get_dof_blur_use_jitter();
	RS::DOFBokehShape bokeh_shape = RSG::camera_attributes->camera_attributes_get_dof_blur_bokeh_shape();
	RS::DOFBlurQuality blur_quality = RSG::camera_attributes->camera_attributes_get_dof_blur_quality();

	// setup our push constant
	memset(&bokeh.push_constant, 0, sizeof(BokehPushConstant));
	bokeh.push_constant.blur_far_active = dof_far;
	bokeh.push_constant.blur_far_begin = dof_far_begin;
	bokeh.push_constant.blur_far_end = dof_far_begin + dof_far_size; // Only used with non-physically-based.
	bokeh.push_constant.use_physical_far = dof_far_size < 0.0;
	bokeh.push_constant.blur_size_far = bokeh_size; // Only used with physically-based.

	bokeh.push_constant.blur_near_active = dof_near;
	bokeh.push_constant.blur_near_begin = dof_near_begin;
	bokeh.push_constant.blur_near_end = dof_near_begin - dof_near_size; // Only used with non-physically-based.
	bokeh.push_constant.use_physical_near = dof_near_size < 0.0;
	bokeh.push_constant.blur_size_near = bokeh_size; // Only used with physically-based.

	bokeh.push_constant.use_jitter = use_jitter;
	bokeh.push_constant.jitter_seed = Math::randf() * 1000.0;

	bokeh.push_constant.z_near = p_cam_znear;
	bokeh.push_constant.z_far = p_cam_zfar;
	bokeh.push_constant.orthogonal = p_cam_orthogonal;
	bokeh.push_constant.blur_size = (dof_near_size < 0.0 && dof_far_size < 0.0) ? 32 : bokeh_size; // Cap with physically-based to keep performance reasonable.

	bokeh.push_constant.second_pass = false;
	bokeh.push_constant.half_size = false;

	bokeh.push_constant.blur_scale = 0.5;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.base_texture }));
	RD::Uniform u_depth_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.depth_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.secondary_texture }));
	RD::Uniform u_half_texture0(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[0] }));
	RD::Uniform u_half_texture1(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[1] }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.base_texture);
	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.secondary_texture);
	RD::Uniform u_half_image0(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.half_texture[0]);
	RD::Uniform u_half_image1(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.half_texture[1]);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	/* FIRST PASS */
	// The alpha channel of the source color texture is filled with the expected circle size
	// If used for DOF far, the size is positive, if used for near, its negative.

	RID shader = bokeh.compute_shader.version_get_shader(bokeh.shader_version, BOKEH_GEN_BLUR_SIZE);
	ERR_FAIL_COND(shader.is_null());

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, bokeh.compute_pipelines[BOKEH_GEN_BLUR_SIZE]);

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_depth_texture), 1);

	bokeh.push_constant.size[0] = p_buffers.base_texture_size.x;
	bokeh.push_constant.size[1] = p_buffers.base_texture_size.y;

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, p_buffers.base_texture_size.y, 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	if (bokeh_shape == RS::DOF_BOKEH_BOX || bokeh_shape == RS::DOF_BOKEH_HEXAGON) {
		//second pass
		BokehMode mode = bokeh_shape == RS::DOF_BOKEH_BOX ? BOKEH_GEN_BOKEH_BOX : BOKEH_GEN_BOKEH_HEXAGONAL;
		shader = bokeh.compute_shader.version_get_shader(bokeh.shader_version, mode);
		ERR_FAIL_COND(shader.is_null());

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, bokeh.compute_pipelines[mode]);

		static const int quality_samples[4] = { 6, 12, 12, 24 };

		bokeh.push_constant.steps = quality_samples[blur_quality];

		if (blur_quality == RS::DOF_BLUR_QUALITY_VERY_LOW || blur_quality == RS::DOF_BLUR_QUALITY_LOW) {
			//box and hexagon are more or less the same, and they can work in either half (very low and low quality) or full (medium and high quality_ sizes)

			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_half_image0), 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_base_texture), 1);

			bokeh.push_constant.size[0] = p_buffers.base_texture_size.x >> 1;
			bokeh.push_constant.size[1] = p_buffers.base_texture_size.y >> 1;
			bokeh.push_constant.half_size = true;
			bokeh.push_constant.blur_size *= 0.5;

		} else {
			//medium and high quality use full size
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_secondary_image), 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_base_texture), 1);
		}

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, bokeh.push_constant.size[0], bokeh.push_constant.size[1], 1);
		RD::get_singleton()->compute_list_add_barrier(compute_list);

		//third pass
		bokeh.push_constant.second_pass = true;

		if (blur_quality == RS::DOF_BLUR_QUALITY_VERY_LOW || blur_quality == RS::DOF_BLUR_QUALITY_LOW) {
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_half_image1), 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_half_texture0), 1);
		} else {
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
		}

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, bokeh.push_constant.size[0], bokeh.push_constant.size[1], 1);
		RD::get_singleton()->compute_list_add_barrier(compute_list);

		if (blur_quality == RS::DOF_BLUR_QUALITY_VERY_LOW || blur_quality == RS::DOF_BLUR_QUALITY_LOW) {
			//forth pass, upscale for low quality

			shader = bokeh.compute_shader.version_get_shader(bokeh.shader_version, BOKEH_COMPOSITE);
			ERR_FAIL_COND(shader.is_null());

			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, bokeh.compute_pipelines[BOKEH_COMPOSITE]);

			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_half_texture1), 1);

			bokeh.push_constant.size[0] = p_buffers.base_texture_size.x;
			bokeh.push_constant.size[1] = p_buffers.base_texture_size.y;
			bokeh.push_constant.half_size = false;
			bokeh.push_constant.second_pass = false;

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

			RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, p_buffers.base_texture_size.y, 1);
		}
	} else {
		//circle

		shader = bokeh.compute_shader.version_get_shader(bokeh.shader_version, BOKEH_GEN_BOKEH_CIRCULAR);
		ERR_FAIL_COND(shader.is_null());

		//second pass
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, bokeh.compute_pipelines[BOKEH_GEN_BOKEH_CIRCULAR]);

		static const float quality_scale[4] = { 8.0, 4.0, 1.0, 0.5 };

		bokeh.push_constant.steps = 0;
		bokeh.push_constant.blur_scale = quality_scale[blur_quality];

		//circle always runs in half size, otherwise too expensive

		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_half_image0), 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_base_texture), 1);

		bokeh.push_constant.size[0] = p_buffers.base_texture_size.x >> 1;
		bokeh.push_constant.size[1] = p_buffers.base_texture_size.y >> 1;
		bokeh.push_constant.half_size = true;

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, bokeh.push_constant.size[0], bokeh.push_constant.size[1], 1);
		RD::get_singleton()->compute_list_add_barrier(compute_list);

		//circle is just one pass, then upscale

		// upscale

		shader = bokeh.compute_shader.version_get_shader(bokeh.shader_version, BOKEH_COMPOSITE);
		ERR_FAIL_COND(shader.is_null());

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, bokeh.compute_pipelines[BOKEH_COMPOSITE]);

		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_half_texture0), 1);

		bokeh.push_constant.size[0] = p_buffers.base_texture_size.x;
		bokeh.push_constant.size[1] = p_buffers.base_texture_size.y;
		bokeh.push_constant.half_size = false;
		bokeh.push_constant.second_pass = false;

		RD::get_singleton()->compute_list_set_push_constant(compute_list, &bokeh.push_constant, sizeof(BokehPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, p_buffers.base_texture_size.y, 1);
	}

	RD::get_singleton()->compute_list_end();
}

void BokehDOF::bokeh_dof_raster(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal) {
	ERR_FAIL_COND_MSG(!prefer_raster_effects, "Can't blur-based depth of field with the clustered renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	bool dof_far = RSG::camera_attributes->camera_attributes_get_dof_far_enabled(p_camera_attributes);
	float dof_far_begin = RSG::camera_attributes->camera_attributes_get_dof_far_distance(p_camera_attributes);
	float dof_far_size = RSG::camera_attributes->camera_attributes_get_dof_far_transition(p_camera_attributes);
	bool dof_near = RSG::camera_attributes->camera_attributes_get_dof_near_enabled(p_camera_attributes);
	float dof_near_begin = RSG::camera_attributes->camera_attributes_get_dof_near_distance(p_camera_attributes);
	float dof_near_size = RSG::camera_attributes->camera_attributes_get_dof_near_transition(p_camera_attributes);
	float bokeh_size = RSG::camera_attributes->camera_attributes_get_dof_blur_amount(p_camera_attributes) * 64; // Base 64 pixel radius.

	RS::DOFBokehShape bokeh_shape = RSG::camera_attributes->camera_attributes_get_dof_blur_bokeh_shape();
	RS::DOFBlurQuality blur_quality = RSG::camera_attributes->camera_attributes_get_dof_blur_quality();

	// setup our base push constant
	memset(&bokeh.push_constant, 0, sizeof(BokehPushConstant));

	bokeh.push_constant.orthogonal = p_cam_orthogonal;
	bokeh.push_constant.size[0] = p_buffers.base_texture_size.width;
	bokeh.push_constant.size[1] = p_buffers.base_texture_size.height;
	bokeh.push_constant.z_far = p_cam_zfar;
	bokeh.push_constant.z_near = p_cam_znear;

	bokeh.push_constant.second_pass = false;
	bokeh.push_constant.half_size = false;
	bokeh.push_constant.blur_size = bokeh_size;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.base_texture }));
	RD::Uniform u_depth_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.depth_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.secondary_texture }));
	RD::Uniform u_half_texture0(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[0] }));
	RD::Uniform u_half_texture1(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[1] }));
	RD::Uniform u_weight_texture0(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.weight_texture[0] }));
	RD::Uniform u_weight_texture1(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.weight_texture[1] }));
	RD::Uniform u_weight_texture2(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.weight_texture[2] }));
	RD::Uniform u_weight_texture3(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.weight_texture[3] }));

	if (dof_far || dof_near) {
		if (dof_far) {
			bokeh.push_constant.blur_far_active = true;
			bokeh.push_constant.blur_far_begin = dof_far_begin;
			bokeh.push_constant.blur_far_end = dof_far_begin + dof_far_size;
		}

		if (dof_near) {
			bokeh.push_constant.blur_near_active = true;
			bokeh.push_constant.blur_near_begin = dof_near_begin;
			bokeh.push_constant.blur_near_end = dof_near_begin - dof_near_size;
		}

		{
			// generate our depth data
			RID shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, BOKEH_GEN_BLUR_SIZE);
			ERR_FAIL_COND(shader.is_null());

			RID framebuffer = p_buffers.base_weight_fb;
			RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[BOKEH_GEN_BLUR_SIZE].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_depth_texture), 0);

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

			RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
			RD::get_singleton()->draw_list_end();
		}

		if (bokeh_shape == RS::DOF_BOKEH_BOX || bokeh_shape == RS::DOF_BOKEH_HEXAGON) {
			// double pass approach
			BokehMode mode = bokeh_shape == RS::DOF_BOKEH_BOX ? BOKEH_GEN_BOKEH_BOX : BOKEH_GEN_BOKEH_HEXAGONAL;

			RID shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, mode);
			ERR_FAIL_COND(shader.is_null());

			if (blur_quality == RS::DOF_BLUR_QUALITY_VERY_LOW || blur_quality == RS::DOF_BLUR_QUALITY_LOW) {
				//box and hexagon are more or less the same, and they can work in either half (very low and low quality) or full (medium and high quality_ sizes)
				bokeh.push_constant.size[0] = p_buffers.base_texture_size.x >> 1;
				bokeh.push_constant.size[1] = p_buffers.base_texture_size.y >> 1;
				bokeh.push_constant.half_size = true;
				bokeh.push_constant.blur_size *= 0.5;
			}

			static const int quality_samples[4] = { 6, 12, 12, 24 };
			bokeh.push_constant.blur_scale = 0.5;
			bokeh.push_constant.steps = quality_samples[blur_quality];

			RID framebuffer = bokeh.push_constant.half_size ? p_buffers.half_fb[0] : p_buffers.secondary_fb;

			// Pass 1
			RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_base_texture), 0);
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_weight_texture0), 1);

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

			RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
			RD::get_singleton()->draw_list_end();

			// Pass 2
			if (!bokeh.push_constant.half_size) {
				// do not output weight, we're writing back into our base buffer
				mode = bokeh_shape == RS::DOF_BOKEH_BOX ? BOKEH_GEN_BOKEH_BOX_NOWEIGHT : BOKEH_GEN_BOKEH_HEXAGONAL_NOWEIGHT;

				shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, mode);
				ERR_FAIL_COND(shader.is_null());
			}
			bokeh.push_constant.second_pass = true;

			framebuffer = bokeh.push_constant.half_size ? p_buffers.half_fb[1] : p_buffers.base_fb;
			RD::Uniform texture = bokeh.push_constant.half_size ? u_half_texture0 : u_secondary_texture;
			RD::Uniform weight = bokeh.push_constant.half_size ? u_weight_texture2 : u_weight_texture1;

			draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, texture), 0);
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, weight), 1);

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

			RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
			RD::get_singleton()->draw_list_end();

			if (bokeh.push_constant.half_size) {
				// Compose pass
				mode = BOKEH_COMPOSITE;
				shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, mode);
				ERR_FAIL_COND(shader.is_null());

				framebuffer = p_buffers.base_fb;

				draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
				RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_half_texture1), 0);
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_weight_texture3), 1);
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 2, u_weight_texture0), 2);

				RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

				RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
				RD::get_singleton()->draw_list_end();
			}

		} else {
			// circular is a single pass approach
			BokehMode mode = BOKEH_GEN_BOKEH_CIRCULAR;

			RID shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, mode);
			ERR_FAIL_COND(shader.is_null());

			{
				// circle always runs in half size, otherwise too expensive (though the code below does support making this optional)
				bokeh.push_constant.size[0] = p_buffers.base_texture_size.x >> 1;
				bokeh.push_constant.size[1] = p_buffers.base_texture_size.y >> 1;
				bokeh.push_constant.half_size = true;
				// bokeh.push_constant.blur_size *= 0.5;
			}

			static const float quality_scale[4] = { 8.0, 4.0, 1.0, 0.5 };
			bokeh.push_constant.blur_scale = quality_scale[blur_quality];
			bokeh.push_constant.steps = 0.0;

			RID framebuffer = bokeh.push_constant.half_size ? p_buffers.half_fb[0] : p_buffers.secondary_fb;

			RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_base_texture), 0);
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_weight_texture0), 1);

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

			RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
			RD::get_singleton()->draw_list_end();

			if (bokeh.push_constant.half_size) {
				// Compose
				mode = BOKEH_COMPOSITE;
				shader = bokeh.raster_shader.version_get_shader(bokeh.shader_version, mode);
				ERR_FAIL_COND(shader.is_null());

				framebuffer = p_buffers.base_fb;

				draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
				RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, bokeh.raster_pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_half_texture0), 0);
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_weight_texture2), 1);
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 2, u_weight_texture0), 2);

				RD::get_singleton()->draw_list_set_push_constant(draw_list, &bokeh.push_constant, sizeof(BokehPushConstant));

				RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
				RD::get_singleton()->draw_list_end();
			} else {
				CopyEffects::get_singleton()->copy_raster(p_buffers.secondary_texture, p_buffers.base_fb);
			}
		}
	}
}
