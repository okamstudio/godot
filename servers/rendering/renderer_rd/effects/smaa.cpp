/**************************************************************************/
/*  smaa.cpp                                                              */
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

#include "smaa.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "thirdparty/smaa/AreaTex.h"
#include "thirdparty/smaa/SearchTex.h"

using namespace RendererRD;

SMAA::SMAA() {
	{
		// Initialize edge detection
		Vector<String> smaa_modes;
		smaa_modes.push_back("\n");
		smaa.edge_shader.initialize(smaa_modes);

		smaa.edge_shader_version = smaa.edge_shader.version_create();

		RD::PipelineDepthStencilState stencil_state = RD::PipelineDepthStencilState();
		stencil_state.enable_stencil = true;
		stencil_state.back_op.reference = 0xff;
		stencil_state.back_op.write_mask = 0xff;
		stencil_state.back_op.compare_mask = 0xff;
		stencil_state.back_op.pass = RD::STENCIL_OP_REPLACE;
		stencil_state.front_op.reference = 0xff;
		stencil_state.front_op.write_mask = 0xff;
		stencil_state.front_op.compare_mask = 0xff;
		stencil_state.front_op.pass = RD::STENCIL_OP_REPLACE;

		for (int i = SMAA_EDGE_DETECTION_COLOR; i <= SMAA_EDGE_DETECTION_COLOR; i++) {
			smaa.pipelines[i].setup(smaa.edge_shader.version_get_shader(smaa.edge_shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), stencil_state, RD::PipelineColorBlendState::create_disabled(), 0);
		}
	}

	{
		// Initialize weight calculation
		Vector<String> smaa_modes;
		smaa_modes.push_back("\n");
		smaa.weight_shader.initialize(smaa_modes);

		smaa.weight_shader_version = smaa.weight_shader.version_create();

		RD::PipelineDepthStencilState stencil_state;
		stencil_state.enable_stencil = true;
		stencil_state.back_op.reference = 0xff;
		stencil_state.back_op.compare_mask = 0xff;
		stencil_state.back_op.compare = RD::COMPARE_OP_EQUAL;
		stencil_state.front_op.reference = 0xff;
		stencil_state.front_op.compare_mask = 0xff;
		stencil_state.front_op.compare = RD::COMPARE_OP_EQUAL;

		for (int i = SMAA_WEIGHT_FULL; i <= SMAA_WEIGHT_FULL; i++) {
			smaa.pipelines[i].setup(smaa.weight_shader.version_get_shader(smaa.weight_shader_version, i - SMAA_WEIGHT_FULL), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), stencil_state, RD::PipelineColorBlendState::create_disabled(), 0);
		}
	}

	{
		// Initialize color blending
		Vector<String> smaa_modes;
		smaa_modes.push_back("\n");
		smaa.blend_shader.initialize(smaa_modes);

		smaa.blend_shader_version = smaa.blend_shader.version_create();

		for (int i = SMAA_BLENDING; i <= SMAA_BLENDING; i++) {
			smaa.pipelines[i].setup(smaa.blend_shader.version_get_shader(smaa.blend_shader_version, i - SMAA_BLENDING), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		}
	}

	{
		// Initialize SearchTex
		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R8_UNORM;
		tf.width = SEARCHTEX_WIDTH;
		tf.height = SEARCHTEX_HEIGHT;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		smaa.search_tex = RD::get_singleton()->texture_create(tf, RD::TextureView(), Vector<Vector<unsigned char>>{ searchTexBytes });
	}

	{
		// Initialize AreaTex
		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R8G8_UNORM;
		tf.width = AREATEX_WIDTH;
		tf.height = AREATEX_HEIGHT;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		smaa.area_tex = RD::get_singleton()->texture_create(tf, RD::TextureView(), Vector<Vector<unsigned char>>{ areaTexBytes });
	}

	{
		// Find smallest stencil texture format
		if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D16_UNORM_S8_UINT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
			smaa.stencil_format = RD::DATA_FORMAT_D16_UNORM_S8_UINT;
		} else if (RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D24_UNORM_S8_UINT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
			smaa.stencil_format = RD::DATA_FORMAT_D24_UNORM_S8_UINT;
		} else {
			smaa.stencil_format = RD::DATA_FORMAT_D32_SFLOAT_S8_UINT;
		}
	}
}

SMAA::~SMAA() {
	RD::get_singleton()->free(smaa.search_tex);
	RD::get_singleton()->free(smaa.area_tex);

	smaa.edge_shader.version_free(smaa.edge_shader_version);
	smaa.weight_shader.version_free(smaa.weight_shader_version);
	smaa.blend_shader.version_free(smaa.blend_shader_version);
}

void SMAA::allocate_render_targets(Ref<RenderSceneBuffersRD> p_render_buffers) {
	Size2i full_size = p_render_buffers->get_internal_size();

	// As we're not clearing these, and render buffers will return the cached texture if it already exists,
	// we don't first check has_texture here

	p_render_buffers->create_texture(RB_SCOPE_SMAA, RB_EDGES, RD::DATA_FORMAT_R8G8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT, RD::TEXTURE_SAMPLES_1, full_size, 1, 1, true, true);
	p_render_buffers->create_texture(RB_SCOPE_SMAA, RB_BLEND, RD::DATA_FORMAT_R8G8B8A8_UNORM, RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT, RD::TEXTURE_SAMPLES_1, full_size, 1, 1, true, true);
	p_render_buffers->create_texture(RB_SCOPE_SMAA, RB_STENCIL, smaa.stencil_format, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, RD::TEXTURE_SAMPLES_1, full_size, 1, 1, true, true);
}

void SMAA::process(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_source_color, RID p_dst_framebuffer) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&smaa.edge_push_constant, 0, sizeof(SMAAEdgePushConstant));
	memset(&smaa.weight_push_constant, 0, sizeof(SMAAWeightPushConstant));
	memset(&smaa.blend_push_constant, 0, sizeof(SMAABlendPushConstant));

	Size2i size = p_render_buffers->get_internal_size();
	Size2 inv_size = Size2(1.0f / (float)size.x, 1.0f / (float)size.y);

	smaa.edge_push_constant.inv_size[0] = inv_size.x;
	smaa.edge_push_constant.inv_size[1] = inv_size.y;

	smaa.weight_push_constant.inv_size[0] = inv_size.x;
	smaa.weight_push_constant.inv_size[1] = inv_size.y;
	smaa.weight_push_constant.size[0] = size.x;
	smaa.weight_push_constant.size[1] = size.y;

	smaa.blend_push_constant.inv_size[0] = inv_size.x;
	smaa.blend_push_constant.inv_size[1] = inv_size.y;

	RID linear_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	allocate_render_targets(p_render_buffers);
	RID edges_tex = p_render_buffers->get_texture(RB_SCOPE_SMAA, RB_EDGES);
	RID blend_tex = p_render_buffers->get_texture(RB_SCOPE_SMAA, RB_BLEND);
	RID stencil_buffer = p_render_buffers->get_texture(RB_SCOPE_SMAA, RB_STENCIL);

	RID edges_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(edges_tex, stencil_buffer);
	RID blend_framebuffer = FramebufferCacheRD::get_singleton()->get_cache(blend_tex, stencil_buffer);

	RD::Uniform u_source_color;
	u_source_color.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_source_color.binding = 0;
	u_source_color.append_id(linear_sampler);
	u_source_color.append_id(p_source_color);

	RD::Uniform u_edges_texture;
	u_edges_texture.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_edges_texture.binding = 0;
	u_edges_texture.append_id(linear_sampler);
	u_edges_texture.append_id(edges_tex);

	RD::Uniform u_area_texture;
	u_area_texture.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_area_texture.binding = 0;
	u_area_texture.append_id(linear_sampler);
	u_area_texture.append_id(smaa.area_tex);

	RD::Uniform u_search_texture;
	u_search_texture.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_search_texture.binding = 1;
	u_search_texture.append_id(linear_sampler);
	u_search_texture.append_id(smaa.search_tex);

	RD::Uniform u_blend_texture;
	u_blend_texture.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	u_blend_texture.binding = 0;
	u_blend_texture.append_id(linear_sampler);
	u_blend_texture.append_id(blend_tex);

	{
		int mode = SMAA_EDGE_DETECTION_COLOR;
		RID shader = smaa.edge_shader.version_get_shader(smaa.edge_shader_version, mode);
		ERR_FAIL_COND(shader.is_null());

		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(edges_framebuffer, RD::DRAW_CLEAR_COLOR_0 | RD::DRAW_CLEAR_STENCIL, Vector<Color>({ Color(0, 0, 0, 0) }), 1.0f, 0);
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, smaa.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(edges_framebuffer), false, RD::get_singleton()->draw_list_get_current_pass()));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_color), 0);

		RD::get_singleton()->draw_list_set_push_constant(draw_list, &smaa.edge_push_constant, sizeof(SMAAEdgePushConstant));
		RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
		RD::get_singleton()->draw_list_end();
	}

	{
		int mode = SMAA_WEIGHT_FULL;
		RID shader = smaa.weight_shader.version_get_shader(smaa.weight_shader_version, mode - SMAA_WEIGHT_FULL);
		ERR_FAIL_COND(shader.is_null());

		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(blend_framebuffer, RD::DRAW_CLEAR_COLOR_0, Vector<Color>({ Color(0, 0, 0, 0) }));
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, smaa.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(blend_framebuffer), false, RD::get_singleton()->draw_list_get_current_pass()));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_edges_texture), 0);
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_area_texture, u_search_texture), 1);

		RD::get_singleton()->draw_list_set_push_constant(draw_list, &smaa.weight_push_constant, sizeof(SMAAWeightPushConstant));
		RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
		RD::get_singleton()->draw_list_end();
	}

	{
		int mode = SMAA_BLENDING;
		RID shader = smaa.blend_shader.version_get_shader(smaa.blend_shader_version, mode - SMAA_BLENDING);
		ERR_FAIL_COND(shader.is_null());

		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dst_framebuffer, RD::DRAW_IGNORE_COLOR_0);
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, smaa.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dst_framebuffer), false, RD::get_singleton()->draw_list_get_current_pass()));
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_color), 0);
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 1, u_blend_texture), 1);

		RD::get_singleton()->draw_list_set_push_constant(draw_list, &smaa.blend_push_constant, sizeof(SMAABlendPushConstant));
		RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
		RD::get_singleton()->draw_list_end();
	}
}
