/**************************************************************************/
/*  vrs.cpp                                                               */
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

#include "vrs.h"
#include "../renderer_compositor_rd.h"
#include "../storage_rd/texture_storage.h"
#include "../uniform_set_cache_rd.h"

#ifndef _3D_DISABLED
#include "servers/xr_server.h"
#endif // _3D_DISABLED

using namespace RendererRD;

VRS::VRS() {
	{
		Vector<String> vrs_modes;
		vrs_modes.push_back("\n"); // VRS_DEFAULT
		vrs_modes.push_back("\n#define USE_MULTIVIEW\n"); // VRS_MULTIVIEW

		vrs_shader.shader.initialize(vrs_modes);

		if (!RendererCompositorRD::get_singleton()->is_xr_enabled()) {
			vrs_shader.shader.set_variant_enabled(VRS_MULTIVIEW, false);
		}

		vrs_shader.shader_version = vrs_shader.shader.version_create();

		//use additive

		for (int i = 0; i < VRS_MAX; i++) {
			if (vrs_shader.shader.is_variant_enabled(i)) {
				vrs_shader.pipelines[i].setup(vrs_shader.shader.version_get_shader(vrs_shader.shader_version, i), RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
			} else {
				vrs_shader.pipelines[i].clear();
			}
		}
	}
}

VRS::~VRS() {
	vrs_shader.shader.version_free(vrs_shader.shader_version);
}

void VRS::copy_vrs(RID p_source_rd_texture, RID p_dest_framebuffer, bool p_multiview) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_source_rd_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_rd_texture }));

	VRSPushConstant push_constant = {};

	int mode = p_multiview ? VRS_MULTIVIEW : VRS_DEFAULT;

	// Set maximum texel factor based on maximum fragment size, some GPUs do not support 8x8 (fragment shading rate approach).
	if (MIN(RD::get_singleton()->limit_get(RD::LIMIT_VRS_MAX_FRAGMENT_WIDTH), RD::get_singleton()->limit_get(RD::LIMIT_VRS_MAX_FRAGMENT_HEIGHT)) > 4) {
		push_constant.max_texel_factor = 3.0;
	} else {
		push_constant.max_texel_factor = 2.0;
	}

	RID shader = vrs_shader.shader.version_get_shader(vrs_shader.shader_version, mode);
	ERR_FAIL_COND(shader.is_null());

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_dest_framebuffer);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, vrs_shader.pipelines[mode].get_render_pipeline(RD::INVALID_ID, RD::get_singleton()->framebuffer_get_format(p_dest_framebuffer)));
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set_cache->get_cache(shader, 0, u_source_rd_texture), 0);
	RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(VRSPushConstant));
	RD::get_singleton()->draw_list_draw(draw_list, false, 1u, 3u);
	RD::get_singleton()->draw_list_end();
}

Size2i VRS::get_vrs_texture_size(const Size2i p_base_size) const {
	int32_t texel_width = RD::get_singleton()->limit_get(RD::LIMIT_VRS_TEXEL_WIDTH);
	int32_t texel_height = RD::get_singleton()->limit_get(RD::LIMIT_VRS_TEXEL_HEIGHT);

	int width = p_base_size.x / texel_width;
	if (p_base_size.x % texel_width != 0) {
		width++;
	}
	int height = p_base_size.y / texel_height;
	if (p_base_size.y % texel_height != 0) {
		height++;
	}
	return Size2i(width, height);
}

void VRS::update_vrs_texture(RID p_vrs_fb, RID p_render_target) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();
	RS::ViewportVRSMode vrs_mode = texture_storage->render_target_get_vrs_mode(p_render_target);
	RS::ViewportVRSUpdateMode vrs_update_mode = texture_storage->render_target_get_vrs_update_mode(p_render_target);

	if (vrs_mode != RS::VIEWPORT_VRS_DISABLED && vrs_update_mode != RS::VIEWPORT_VRS_UPDATE_DISABLED) {
		RD::get_singleton()->draw_command_begin_label("VRS Setup");

		if (vrs_mode == RS::VIEWPORT_VRS_TEXTURE) {
			RID vrs_texture = texture_storage->render_target_get_vrs_texture(p_render_target);
			if (vrs_texture.is_valid()) {
				RID rd_texture = texture_storage->texture_get_rd_texture(vrs_texture);
				int layers = texture_storage->texture_get_layers(vrs_texture);
				if (rd_texture.is_valid()) {
					// Copy into our density buffer
					copy_vrs(rd_texture, p_vrs_fb, layers > 1);
				}
			}
#ifndef _3D_DISABLED
		} else if (vrs_mode == RS::VIEWPORT_VRS_XR) {
			Ref<XRInterface> interface = XRServer::get_singleton()->get_primary_interface();
			if (interface.is_valid()) {
				RID vrs_texture = interface->get_vrs_texture();
				if (vrs_texture.is_valid()) {
					RID rd_texture = texture_storage->texture_get_rd_texture(vrs_texture);
					int layers = texture_storage->texture_get_layers(vrs_texture);

					if (rd_texture.is_valid()) {
						// Copy into our density buffer
						copy_vrs(rd_texture, p_vrs_fb, layers > 1);
					}
				}
			}
#endif // _3D_DISABLED
		}

		if (vrs_update_mode == RS::VIEWPORT_VRS_UPDATE_ONCE) {
			texture_storage->render_target_set_vrs_update_mode(p_render_target, RS::VIEWPORT_VRS_UPDATE_DISABLED);
		}

		RD::get_singleton()->draw_command_end_label();
	}
}
