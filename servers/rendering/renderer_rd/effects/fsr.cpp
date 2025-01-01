/**************************************************************************/
/*  fsr.cpp                                                               */
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

#include "fsr.h"
#include "../storage_rd/material_storage.h"
#include "../uniform_set_cache_rd.h"

using namespace RendererRD;

FSR::FSR() {
	Vector<String> FSR_upscale_modes;
	if (RD::get_singleton()->has_feature(RD::SUPPORTS_FSR_HALF_FLOAT)) {
		FSR_upscale_modes.push_back("\n#define MODE_FSR_UPSCALE_NORMAL\n");
	} else {
		FSR_upscale_modes.push_back("\n#define MODE_FSR_UPSCALE_FALLBACK\n");
	}

	fsr_shader.initialize(FSR_upscale_modes);

	shader_version = fsr_shader.version_create();
	pipeline = RD::get_singleton()->compute_pipeline_create(fsr_shader.version_get_shader(shader_version, 0));
}

FSR::~FSR() {
	fsr_shader.version_free(shader_version);
}

void FSR::fsr_upscale(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_source_rd_texture, RID p_destination_texture) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	Size2i internal_size = p_render_buffers->get_internal_size();
	Size2i target_size = p_render_buffers->get_target_size();
	float fsr_upscale_sharpness = p_render_buffers->get_fsr_sharpness();

	if (!p_render_buffers->has_texture(SNAME("FSR"), SNAME("upscale_texture"))) {
		RD::DataFormat format = p_render_buffers->get_base_data_format();
		uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		uint32_t layers = 1; // we only need one layer, in multiview we're processing one layer at a time.

		p_render_buffers->create_texture(SNAME("FSR"), SNAME("upscale_texture"), format, usage_bits, RD::TEXTURE_SAMPLES_1, target_size, layers);
	}

	RID upscale_texture = p_render_buffers->get_texture(SNAME("FSR"), SNAME("upscale_texture"));

	FSRUpscalePushConstant push_constant;
	memset(&push_constant, 0, sizeof(FSRUpscalePushConstant));

	int dispatch_x = (target_size.x + 15) / 16;
	int dispatch_y = (target_size.y + 15) / 16;

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipeline);

	push_constant.resolution_width = internal_size.width;
	push_constant.resolution_height = internal_size.height;
	push_constant.upscaled_width = target_size.width;
	push_constant.upscaled_height = target_size.height;
	push_constant.sharpness = fsr_upscale_sharpness;

	RID shader = fsr_shader.version_get_shader(shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	//FSR Easc
	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, { default_sampler, p_source_rd_texture });
	RD::Uniform u_upscale_texture(RD::UNIFORM_TYPE_IMAGE, 0, { upscale_texture });

	push_constant.pass = FSR_UPSCALE_PASS_EASU;
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_upscale_texture), 1);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(FSRUpscalePushConstant));

	RD::get_singleton()->compute_list_dispatch(compute_list, dispatch_x, dispatch_y, 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	//FSR Rcas
	RD::Uniform u_upscale_texture_with_sampler(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, { default_sampler, upscale_texture });
	RD::Uniform u_destination_texture(RD::UNIFORM_TYPE_IMAGE, 0, { p_destination_texture });

	push_constant.pass = FSR_UPSCALE_PASS_RCAS;
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_upscale_texture_with_sampler), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_destination_texture), 1);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(FSRUpscalePushConstant));

	RD::get_singleton()->compute_list_dispatch(compute_list, dispatch_x, dispatch_y, 1);

	RD::get_singleton()->compute_list_end();
}
