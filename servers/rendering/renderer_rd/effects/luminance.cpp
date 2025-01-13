/**************************************************************************/
/*  luminance.cpp                                                         */
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

#include "luminance.h"
#include "../framebuffer_cache_rd.h"
#include "../uniform_set_cache_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"

using namespace RendererRD;

Luminance::Luminance(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;

	if (prefer_raster_effects) {
		Vector<String> luminance_reduce_modes;
		luminance_reduce_modes.push_back("\n#define FIRST_PASS\n"); // LUMINANCE_REDUCE_FRAGMENT_FIRST
		luminance_reduce_modes.push_back("\n"); // LUMINANCE_REDUCE_FRAGMENT
		luminance_reduce_modes.push_back("\n#define FINAL_PASS\n"); // LUMINANCE_REDUCE_FRAGMENT_FINAL

		luminance_reduce_raster.shader.initialize(luminance_reduce_modes);
		luminance_reduce_raster.shader_version = luminance_reduce_raster.shader.version_create();

		for (int i = 0; i < LUMINANCE_REDUCE_FRAGMENT_MAX; i++) {
			luminance_reduce_raster.pipelines[i].setup(luminance_reduce_raster.shader.version_get_shader(luminance_reduce_raster.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		}
	} else {
		// Initialize luminance_reduce
		Vector<String> luminance_reduce_modes;
		luminance_reduce_modes.push_back("\n#define READ_TEXTURE\n");
		luminance_reduce_modes.push_back("\n");
		luminance_reduce_modes.push_back("\n#define WRITE_LUMINANCE\n");

		luminance_reduce.shader.initialize(luminance_reduce_modes);
		luminance_reduce.shader_version = luminance_reduce.shader.version_create();

		for (int i = 0; i < LUMINANCE_REDUCE_MAX; i++) {
			luminance_reduce.pipelines[i] = RD::get_singleton()->compute_pipeline_create(luminance_reduce.shader.version_get_shader(luminance_reduce.shader_version, i));
		}

		for (int i = 0; i < LUMINANCE_REDUCE_FRAGMENT_MAX; i++) {
			luminance_reduce_raster.pipelines[i].clear();
		}
	}
}

Luminance::~Luminance() {
	if (prefer_raster_effects) {
		luminance_reduce_raster.shader.version_free(luminance_reduce_raster.shader_version);
	} else {
		luminance_reduce.shader.version_free(luminance_reduce.shader_version);
	}
}

void Luminance::LuminanceBuffers::set_prefer_raster_effects(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;
}

void Luminance::LuminanceBuffers::configure(RenderSceneBuffersRD *p_render_buffers) {
	Size2i internal_size = p_render_buffers->get_internal_size();
	int w = internal_size.x;
	int h = internal_size.y;

	while (true) {
		w = MAX(w / 8, 1);
		h = MAX(h / 8, 1);

		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;
		tf.width = w;
		tf.height = h;

		bool final = w == 1 && h == 1;

		if (prefer_raster_effects) {
			tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		} else {
			tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT;
			if (final) {
				tf.usage_bits |= RD::TEXTURE_USAGE_SAMPLING_BIT;
			}
		}

		RID texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
		reduce.push_back(texture);

		if (final) {
			current = RD::get_singleton()->texture_create(tf, RD::TextureView());
			break;
		}
	}
}

void Luminance::LuminanceBuffers::free_data() {
	for (int i = 0; i < reduce.size(); i++) {
		RD::get_singleton()->free(reduce[i]);
	}
	reduce.clear();

	if (current.is_valid()) {
		RD::get_singleton()->free(current);
		current = RID();
	}
}

Ref<Luminance::LuminanceBuffers> Luminance::get_luminance_buffers(Ref<RenderSceneBuffersRD> p_render_buffers) {
	if (p_render_buffers->has_custom_data(RB_LUMINANCE_BUFFERS)) {
		return p_render_buffers->get_custom_data(RB_LUMINANCE_BUFFERS);
	}

	Ref<LuminanceBuffers> buffers;
	buffers.instantiate();
	buffers->set_prefer_raster_effects(prefer_raster_effects);
	buffers->configure(p_render_buffers.ptr());

	p_render_buffers->set_custom_data(RB_LUMINANCE_BUFFERS, buffers);

	return buffers;
}

RID Luminance::get_current_luminance_buffer(Ref<RenderSceneBuffersRD> p_render_buffers) {
	if (p_render_buffers->has_custom_data(RB_LUMINANCE_BUFFERS)) {
		Ref<LuminanceBuffers> buffers = p_render_buffers->get_custom_data(RB_LUMINANCE_BUFFERS);
		return buffers->current;
	}

	return RID();
}

void Luminance::luminance_reduction(RID p_source_texture, const Size2i p_source_size, Ref<LuminanceBuffers> p_luminance_buffers, float p_min_luminance, float p_max_luminance, float p_adjust, bool p_set) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	if (prefer_raster_effects) {
		LuminanceReduceRasterPushConstant push_constant;
		memset(&push_constant, 0, sizeof(LuminanceReduceRasterPushConstant));

		push_constant.max_luminance = p_max_luminance;
		push_constant.min_luminance = p_min_luminance;
		push_constant.exposure_adjust = p_adjust;

		for (int i = 0; i < p_luminance_buffers->reduce.size(); i++) {
			push_constant.source_size[0] = i == 0 ? p_source_size.x : push_constant.dest_size[0];
			push_constant.source_size[1] = i == 0 ? p_source_size.y : push_constant.dest_size[1];
			push_constant.dest_size[0] = MAX(push_constant.source_size[0] / 8, 1);
			push_constant.dest_size[1] = MAX(push_constant.source_size[1] / 8, 1);

			bool final = !p_set && (push_constant.dest_size[0] == 1) && (push_constant.dest_size[1] == 1);
			LuminanceReduceRasterMode mode = final ? LUMINANCE_REDUCE_FRAGMENT_FINAL : (i == 0 ? LUMINANCE_REDUCE_FRAGMENT_FIRST : LUMINANCE_REDUCE_FRAGMENT);
			RID shader = luminance_reduce_raster.shader.version_get_shader(luminance_reduce_raster.shader_version, mode);

			RID framebuffer = FramebufferCacheRD::get_singleton()->get_cache(p_luminance_buffers->reduce[i]);

			RD::Uniform u_source_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, i == 0 ? p_source_texture : p_luminance_buffers->reduce[i - 1] }));

			RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, luminance_reduce_raster.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(framebuffer)));
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_texture), 0);
			if (final) {
				RD::Uniform u_current_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_luminance_buffers->current }));
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_current_texture), 1);
			}

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(LuminanceReduceRasterPushConstant));

			RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
			RD::get_singleton()->draw_list_end();
		}
	} else {
		LuminanceReducePushConstant push_constant;
		memset(&push_constant, 0, sizeof(LuminanceReducePushConstant));

		push_constant.source_size[0] = p_source_size.x;
		push_constant.source_size[1] = p_source_size.y;
		push_constant.max_luminance = p_max_luminance;
		push_constant.min_luminance = p_min_luminance;
		push_constant.exposure_adjust = p_adjust;

		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

		for (int i = 0; i < p_luminance_buffers->reduce.size(); i++) {
			RID shader;

			if (i == 0) {
				shader = luminance_reduce.shader.version_get_shader(luminance_reduce.shader_version, LUMINANCE_REDUCE_READ);
				RD::Uniform u_source_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_texture }));

				RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, luminance_reduce.pipelines[LUMINANCE_REDUCE_READ]);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_texture), 0);
			} else {
				RD::get_singleton()->compute_list_add_barrier(compute_list); //needs barrier, wait until previous is done

				if (i == p_luminance_buffers->reduce.size() - 1 && !p_set) {
					shader = luminance_reduce.shader.version_get_shader(luminance_reduce.shader_version, LUMINANCE_REDUCE_WRITE);
					RD::Uniform u_current_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_luminance_buffers->current }));

					RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, luminance_reduce.pipelines[LUMINANCE_REDUCE_WRITE]);
					RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 2, u_current_texture), 2);
				} else {
					shader = luminance_reduce.shader.version_get_shader(luminance_reduce.shader_version, LUMINANCE_REDUCE);
					RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, luminance_reduce.pipelines[LUMINANCE_REDUCE]);
				}

				RD::Uniform u_source_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_luminance_buffers->reduce[i - 1]);
				RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_source_texture), 0);
			}

			RD::Uniform u_reduce_texture(RD::UNIFORM_TYPE_IMAGE, 0, p_luminance_buffers->reduce[i]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_reduce_texture), 1);

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(LuminanceReducePushConstant));

			RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.source_size[0], push_constant.source_size[1], 1);

			push_constant.source_size[0] = MAX(push_constant.source_size[0] / 8, 1);
			push_constant.source_size[1] = MAX(push_constant.source_size[1] / 8, 1);
		}

		RD::get_singleton()->compute_list_end();
	}

	SWAP(p_luminance_buffers->current, p_luminance_buffers->reduce.write[p_luminance_buffers->reduce.size() - 1]);
}
