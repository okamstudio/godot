/**************************************************************************/
/*  render_forward_mobile.cpp                                             */
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

#include "render_forward_mobile.h"
#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_default.h"

#define PRELOAD_PIPELINES_ON_SURFACE_CACHE_CONSTRUCTION 1

using namespace RendererSceneRenderImplementation;

RendererRD::ForwardID RenderForwardMobile::ForwardIDStorageMobile::allocate_forward_id(RendererRD::ForwardIDType p_type) {
	int32_t index = -1;
	for (uint32_t i = 0; i < forward_id_allocators[p_type].allocations.size(); i++) {
		if (forward_id_allocators[p_type].allocations[i] == false) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		index = forward_id_allocators[p_type].allocations.size();
		forward_id_allocators[p_type].allocations.push_back(true);
		forward_id_allocators[p_type].map.push_back(0xFF);
		forward_id_allocators[p_type].last_pass.push_back(0);
	} else {
		forward_id_allocators[p_type].allocations[index] = true;
	}

	return index;
}
void RenderForwardMobile::ForwardIDStorageMobile::free_forward_id(RendererRD::ForwardIDType p_type, RendererRD::ForwardID p_id) {
	ERR_FAIL_INDEX(p_id, (RendererRD::ForwardID)forward_id_allocators[p_type].allocations.size());
	forward_id_allocators[p_type].allocations[p_id] = false;
}

void RenderForwardMobile::ForwardIDStorageMobile::map_forward_id(RendererRD::ForwardIDType p_type, RendererRD::ForwardID p_id, uint32_t p_index, uint64_t p_last_pass) {
	forward_id_allocators[p_type].map[p_id] = p_index;
	forward_id_allocators[p_type].last_pass[p_id] = p_last_pass;
}

void RenderForwardMobile::fill_push_constant_instance_indices(SceneState::InstanceData *p_instance_data, const GeometryInstanceForwardMobile *p_instance) {
	uint64_t current_frame = RSG::rasterizer->get_frame_number();

	p_instance_data->omni_lights[0] = 0xFFFFFFFF;
	p_instance_data->omni_lights[1] = 0xFFFFFFFF;

	uint32_t idx = 0;
	for (uint32_t i = 0; i < p_instance->omni_light_count; i++) {
		uint32_t ofs = idx < 4 ? 0 : 1;
		uint32_t shift = (idx & 0x3) << 3;
		uint32_t mask = ~(0xFF << shift);

		if (forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_OMNI_LIGHT].last_pass[p_instance->omni_lights[i]] == current_frame) {
			p_instance_data->omni_lights[ofs] &= mask;
			p_instance_data->omni_lights[ofs] |= uint32_t(forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_OMNI_LIGHT].map[p_instance->omni_lights[i]]) << shift;
			idx++;
		}
	}

	p_instance_data->spot_lights[0] = 0xFFFFFFFF;
	p_instance_data->spot_lights[1] = 0xFFFFFFFF;

	idx = 0;
	for (uint32_t i = 0; i < p_instance->spot_light_count; i++) {
		uint32_t ofs = idx < 4 ? 0 : 1;
		uint32_t shift = (idx & 0x3) << 3;
		uint32_t mask = ~(0xFF << shift);
		if (forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_SPOT_LIGHT].last_pass[p_instance->spot_lights[i]] == current_frame) {
			p_instance_data->spot_lights[ofs] &= mask;
			p_instance_data->spot_lights[ofs] |= uint32_t(forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_SPOT_LIGHT].map[p_instance->spot_lights[i]]) << shift;
			idx++;
		}
	}

	p_instance_data->decals[0] = 0xFFFFFFFF;
	p_instance_data->decals[1] = 0xFFFFFFFF;

	idx = 0;
	for (uint32_t i = 0; i < p_instance->decals_count; i++) {
		uint32_t ofs = idx < 4 ? 0 : 1;
		uint32_t shift = (idx & 0x3) << 3;
		uint32_t mask = ~(0xFF << shift);
		if (forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_DECAL].last_pass[p_instance->decals[i]] == current_frame) {
			p_instance_data->decals[ofs] &= mask;
			p_instance_data->decals[ofs] |= uint32_t(forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_DECAL].map[p_instance->decals[i]]) << shift;
			idx++;
		}
	}

	p_instance_data->reflection_probes[0] = 0xFFFFFFFF;
	p_instance_data->reflection_probes[1] = 0xFFFFFFFF;

	idx = 0;
	for (uint32_t i = 0; i < p_instance->reflection_probe_count; i++) {
		uint32_t ofs = idx < 4 ? 0 : 1;
		uint32_t shift = (idx & 0x3) << 3;
		uint32_t mask = ~(0xFF << shift);
		if (forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_REFLECTION_PROBE].last_pass[p_instance->reflection_probes[i]] == current_frame) {
			p_instance_data->reflection_probes[ofs] &= mask;
			p_instance_data->reflection_probes[ofs] |= uint32_t(forward_id_storage_mobile->forward_id_allocators[RendererRD::FORWARD_ID_TYPE_REFLECTION_PROBE].map[p_instance->reflection_probes[i]]) << shift;
			idx++;
		}
	}
}

/* Render buffer */

void RenderForwardMobile::RenderBufferDataForwardMobile::free_data() {
	// this should already be done but JIC..
	if (render_buffers) {
		render_buffers->clear_context(RB_SCOPE_MOBILE);
	}
}

void RenderForwardMobile::RenderBufferDataForwardMobile::configure(RenderSceneBuffersRD *p_render_buffers) {
	if (render_buffers) {
		// JIC
		free_data();
	}

	render_buffers = p_render_buffers;
	ERR_FAIL_NULL(render_buffers); // Huh? really?
}

RID RenderForwardMobile::RenderBufferDataForwardMobile::get_color_fbs(FramebufferConfigType p_config_type) {
	ERR_FAIL_NULL_V(render_buffers, RID());

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	ERR_FAIL_NULL_V(texture_storage, RID());

	// We use our framebuffer cache here instead of building these in RenderBufferDataForwardMobile::configure
	// This approach ensures we only build the framebuffers we actually need for this viewport.
	// In the (near) future this means that if we cycle through a texture chain for our render target, we'll also support
	// this.

	RS::ViewportMSAA msaa_3d = render_buffers->get_msaa_3d();
	bool use_msaa = msaa_3d != RS::VIEWPORT_MSAA_DISABLED;

	uint32_t view_count = render_buffers->get_view_count();

	RID vrs_texture;
	if (render_buffers->has_texture(RB_SCOPE_VRS, RB_TEXTURE)) {
		vrs_texture = render_buffers->get_texture(RB_SCOPE_VRS, RB_TEXTURE);
	}

	Vector<RID> textures;
	int color_buffer_id = 0;
	textures.push_back(use_msaa ? render_buffers->get_color_msaa() : render_buffers->get_internal_texture()); // 0 - color buffer
	textures.push_back(use_msaa ? render_buffers->get_depth_msaa() : render_buffers->get_depth_texture()); // 1 - depth buffer
	if (vrs_texture.is_valid()) {
		textures.push_back(vrs_texture); // 2 - vrs texture
	}
	if (use_msaa) {
		color_buffer_id = textures.size();
		textures.push_back(render_buffers->get_internal_texture()); // color buffer for resolve

		// TODO add support for resolving depth buffer!!!
	}

	// Now define our subpasses
	Vector<RD::FramebufferPass> passes;

	switch (p_config_type) {
		case FB_CONFIG_RENDER_PASS: {
			RD::FramebufferPass pass;
			pass.color_attachments.push_back(0);
			pass.depth_attachment = 1;
			if (vrs_texture.is_valid()) {
				pass.vrs_attachment = 2;
			}

			if (use_msaa) {
				// Add resolve
				pass.resolve_attachments.push_back(color_buffer_id);
			}
			passes.push_back(pass);

			return FramebufferCacheRD::get_singleton()->get_cache_multipass(textures, passes, view_count);
		} break;

		case FB_CONFIG_RENDER_AND_POST_PASS: {
			Size2i target_size = render_buffers->get_target_size();
			Size2i internal_size = render_buffers->get_internal_size();

			// can't do our blit pass if resolutions don't match, this should already have been checked.
			ERR_FAIL_COND_V(target_size != internal_size, RID());

			RD::FramebufferPass pass;
			pass.color_attachments.push_back(0);
			pass.depth_attachment = 1;
			if (vrs_texture.is_valid()) {
				pass.vrs_attachment = 2;
			}

			if (use_msaa) {
				// add resolve
				pass.resolve_attachments.push_back(color_buffer_id);
			}

			passes.push_back(pass);

			// - add blit to 2D pass
			RID render_target = render_buffers->get_render_target();
			ERR_FAIL_COND_V(render_target.is_null(), RID());
			RID target_buffer;
			if (view_count > 1 || texture_storage->render_target_get_msaa(render_target) == RS::VIEWPORT_MSAA_DISABLED) {
				target_buffer = texture_storage->render_target_get_rd_texture(render_target);
			} else {
				target_buffer = texture_storage->render_target_get_rd_texture_msaa(render_target);
				texture_storage->render_target_set_msaa_needs_resolve(render_target, true); // Make sure this gets resolved.
			}
			ERR_FAIL_COND_V(target_buffer.is_null(), RID());

			int target_buffer_id = textures.size();
			textures.push_back(target_buffer); // target buffer

			RD::FramebufferPass blit_pass;
			blit_pass.input_attachments.push_back(color_buffer_id); // Read from our (resolved) color buffer
			blit_pass.color_attachments.push_back(target_buffer_id); // Write into our target buffer
			// this doesn't need VRS
			passes.push_back(blit_pass);

			return FramebufferCacheRD::get_singleton()->get_cache_multipass(textures, passes, view_count);
		} break;
		default:
			break;
	};

	return RID();
}

RID RenderForwardMobile::reflection_probe_create_framebuffer(RID p_color, RID p_depth) {
	// Our attachments
	Vector<RID> attachments;
	attachments.push_back(p_color); // 0
	attachments.push_back(p_depth); // 1

	return RD::get_singleton()->framebuffer_create(attachments);
}

void RenderForwardMobile::setup_render_buffer_data(Ref<RenderSceneBuffersRD> p_render_buffers) {
	Ref<RenderBufferDataForwardMobile> data;
	data.instantiate();

	p_render_buffers->set_custom_data(RB_SCOPE_MOBILE, data);
}

void RenderForwardMobile::mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	RID shadow_mesh = mesh_storage->mesh_get_shadow_mesh(p_mesh);
	uint32_t surface_count = 0;
	const RID *materials = mesh_storage->mesh_get_surface_count_and_materials(p_mesh, surface_count);
	Vector<ShaderPipelinePair> pipeline_pairs;
	for (uint32_t i = 0; i < surface_count; i++) {
		if (materials[i].is_null()) {
			continue;
		}

		void *mesh_surface = mesh_storage->mesh_get_surface(p_mesh, i);
		void *mesh_surface_shadow = mesh_surface;
		SceneShaderForwardMobile::MaterialData *material = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(materials[i], RendererRD::MaterialStorage::SHADER_TYPE_3D));
		if (material == nullptr || !material->shader_data->is_valid()) {
			continue;
		}

		SceneShaderForwardMobile::ShaderData *shader = material->shader_data;
		SceneShaderForwardMobile::ShaderData *shader_shadow = shader;
		if (material->shader_data->uses_shared_shadow_material()) {
			SceneShaderForwardMobile::MaterialData *material_shadow = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(scene_shader.default_material, RendererRD::MaterialStorage::SHADER_TYPE_3D));
			if (material_shadow != nullptr) {
				shader_shadow = material_shadow->shader_data;
				if (shadow_mesh.is_valid()) {
					mesh_surface_shadow = mesh_storage->mesh_get_surface(shadow_mesh, i);
				}
			}
		}

		if (!shader->is_valid()) {
			continue;
		}

		SurfacePipelineData surface;
		surface.mesh_surface = mesh_surface;
		surface.mesh_surface_shadow = mesh_surface_shadow;
		surface.shader = shader;
		surface.shader_shadow = shader_shadow;
		surface.instanced = mesh_storage->mesh_needs_instance(p_mesh, true);
		surface.uses_opaque = !material->shader_data->uses_alpha_pass();
		surface.uses_transparent = material->shader_data->uses_alpha_pass();
		surface.uses_depth = surface.uses_opaque || (surface.uses_transparent && material->shader_data->uses_depth_in_alpha_pass());
		surface.can_use_lightmap = mesh_storage->mesh_surface_get_format(mesh_surface) & RS::ARRAY_FORMAT_TEX_UV2;
		_mesh_compile_pipelines_for_surface(surface, global_pipeline_data_required, RS::PIPELINE_SOURCE_MESH, &pipeline_pairs);
	}

	// Try to wait for all the pipelines that were compiled. This will force the loader to wait on all ubershader pipelines to be ready.
	if (!p_background_compilation && !pipeline_pairs.is_empty()) {
		for (ShaderPipelinePair pair : pipeline_pairs) {
			pair.first->pipeline_hash_map.wait_for_pipeline(pair.second.hash());
		}
	}
}

uint32_t RenderForwardMobile::get_pipeline_compilations(RS::PipelineSource p_source) {
	return scene_shader.get_pipeline_compilations(p_source);
}

bool RenderForwardMobile::free(RID p_rid) {
	if (RendererSceneRenderRD::free(p_rid)) {
		return true;
	}
	return false;
}

void RenderForwardMobile::update() {
	RendererSceneRenderRD::update();
	_update_global_pipeline_data_requirements_from_project();
	_update_global_pipeline_data_requirements_from_light_storage();
}

/* Render functions */

float RenderForwardMobile::_render_buffers_get_luminance_multiplier() {
	// On mobile renderer we need to multiply source colors by 2 due to using a UNORM buffer
	// and multiplying by the output color during 3D rendering by 0.5
	return 2.0;
}

RD::DataFormat RenderForwardMobile::_render_buffers_get_color_format() {
	// Using 32bit buffers enables AFBC on mobile devices which should have a definite performance improvement (MALI G710 and newer support this on 64bit RTs)
	return RD::DATA_FORMAT_A2B10G10R10_UNORM_PACK32;
}

bool RenderForwardMobile::_render_buffers_can_be_storage() {
	// Using 32bit buffers enables AFBC on mobile devices which should have a definite performance improvement (MALI G710 and newer support this on 64bit RTs)
	// Doesn't support storage
	return false;
}

RID RenderForwardMobile::_setup_render_pass_uniform_set(RenderListType p_render_list, const RenderDataRD *p_render_data, RID p_radiance_texture, const RendererRD::MaterialStorage::Samplers &p_samplers, bool p_use_directional_shadow_atlas, int p_index) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	//there should always be enough uniform buffers for render passes, otherwise bugs
	ERR_FAIL_INDEX_V(p_index, (int)scene_state.uniform_buffers.size(), RID());

	bool is_multiview = false;

	Ref<RenderBufferDataForwardMobile> rb_data;
	Ref<RenderSceneBuffersRD> rb;
	if (p_render_data && p_render_data->render_buffers.is_valid()) {
		rb = p_render_data->render_buffers;
		is_multiview = rb->get_view_count() > 1;
		if (rb->has_custom_data(RB_SCOPE_MOBILE)) {
			// Our forward mobile custom data buffer will only be available when we're rendering our normal view.
			// This will not be available when rendering reflection probes.
			rb_data = rb->get_custom_data(RB_SCOPE_MOBILE);
		}
	}

	// default render buffer and scene state uniform set
	// loaded into set 1

	Vector<RD::Uniform> uniforms;

	{
		RD::Uniform u;
		u.binding = 0;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.append_id(scene_state.uniform_buffers[p_index]);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.binding = 1;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		RID instance_buffer = scene_state.instance_buffer[p_render_list];
		if (instance_buffer == RID()) {
			instance_buffer = scene_shader.default_vec4_xform_buffer; // Any buffer will do since its not used.
		}
		u.append_id(instance_buffer);
		uniforms.push_back(u);
	}

	{
		RID radiance_texture;
		if (p_radiance_texture.is_valid()) {
			radiance_texture = p_radiance_texture;
		} else {
			radiance_texture = texture_storage->texture_rd_get_default(is_using_radiance_cubemap_array() ? RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK : RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_BLACK);
		}
		RD::Uniform u;
		u.binding = 2;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.append_id(radiance_texture);
		uniforms.push_back(u);
	}

	{
		RID ref_texture = (p_render_data && p_render_data->reflection_atlas.is_valid()) ? light_storage->reflection_atlas_get_texture(p_render_data->reflection_atlas) : RID();
		RD::Uniform u;
		u.binding = 3;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		if (ref_texture.is_valid()) {
			u.append_id(ref_texture);
		} else {
			u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK));
		}
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.binding = 4;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		RID texture;
		if (p_render_data && p_render_data->shadow_atlas.is_valid()) {
			texture = light_storage->shadow_atlas_get_texture(p_render_data->shadow_atlas);
		}
		if (!texture.is_valid()) {
			texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_DEPTH);
		}
		u.append_id(texture);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.binding = 5;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		if (p_use_directional_shadow_atlas && light_storage->directional_shadow_get_texture().is_valid()) {
			u.append_id(light_storage->directional_shadow_get_texture());
		} else {
			u.append_id(texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_DEPTH));
		}
		uniforms.push_back(u);
	}

	/* we have limited ability to keep textures like this so we're moving this to a set we change before drawing geometry and just pushing the needed texture in */
	{
		RD::Uniform u;
		u.binding = 6;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;

		RID default_tex = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
		for (uint32_t i = 0; i < scene_state.max_lightmaps * 2; i++) {
			uint32_t current_lightmap_index = i < scene_state.max_lightmaps ? i : i - scene_state.max_lightmaps;

			if (p_render_data && current_lightmap_index < p_render_data->lightmaps->size()) {
				RID base = light_storage->lightmap_instance_get_lightmap((*p_render_data->lightmaps)[current_lightmap_index]);
				RID texture;

				if (i < scene_state.max_lightmaps) {
					// Lightmap
					texture = light_storage->lightmap_get_texture(base);
				} else {
					// Shadowmask
					texture = light_storage->shadowmask_get_texture(base);
				}

				if (texture.is_valid()) {
					RID rd_texture = texture_storage->texture_get_rd_texture(texture);
					u.append_id(rd_texture);
					continue;
				}
			}

			u.append_id(default_tex);
		}

		uniforms.push_back(u);
	}

	/*
	{
		RD::Uniform u;
		u.binding = 7;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.ids.resize(MAX_VOXEL_GI_INSTANCESS);
		RID default_tex = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);
		for (int i = 0; i < MAX_VOXEL_GI_INSTANCESS; i++) {
			if (i < (int)p_voxel_gi_instances.size()) {
				RID tex = gi.voxel_gi_instance_get_texture(p_voxel_gi_instances[i]);
				if (!tex.is_valid()) {
					tex = default_tex;
				}
				u.ids.write[i] = tex;
			} else {
				u.ids.write[i] = default_tex;
			}
		}

		uniforms.push_back(u);
	}
	*/

	{
		RD::Uniform u;
		u.binding = 9;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		RID texture;
		if (rb.is_valid() && rb->has_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_DEPTH)) {
			texture = rb->get_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_DEPTH);
		} else if (is_multiview) {
			texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_DEPTH);
		} else {
			texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_DEPTH);
		}
		u.append_id(texture);
		uniforms.push_back(u);
	}
	{
		RD::Uniform u;
		u.binding = 10;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		RID texture = rb_data.is_valid() ? rb->get_back_buffer_texture() : RID();
		if (texture.is_null()) {
			if (is_multiview) {
				texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_DEPTH);
			} else {
				texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
			}
		}
		u.append_id(texture);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.binding = 11;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		RID sampler;
		switch (decals_get_filter()) {
			case RS::DECAL_FILTER_NEAREST: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::DECAL_FILTER_LINEAR: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::DECAL_FILTER_NEAREST_MIPMAPS: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::DECAL_FILTER_LINEAR_MIPMAPS: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::DECAL_FILTER_NEAREST_MIPMAPS_ANISOTROPIC: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::DECAL_FILTER_LINEAR_MIPMAPS_ANISOTROPIC: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
		}

		u.append_id(sampler);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.binding = 12;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		RID sampler;
		switch (light_projectors_get_filter()) {
			case RS::LIGHT_PROJECTOR_FILTER_NEAREST: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::LIGHT_PROJECTOR_FILTER_LINEAR: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS_ANISOTROPIC: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
			case RS::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC: {
				sampler = p_samplers.get_sampler(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			} break;
		}

		u.append_id(sampler);
		uniforms.push_back(u);
	}

	p_samplers.append_uniforms(uniforms, 13);

	if (p_index >= (int)render_pass_uniform_sets.size()) {
		render_pass_uniform_sets.resize(p_index + 1);
	}

	if (render_pass_uniform_sets[p_index].is_valid() && RD::get_singleton()->uniform_set_is_valid(render_pass_uniform_sets[p_index])) {
		RD::get_singleton()->free(render_pass_uniform_sets[p_index]);
	}
	render_pass_uniform_sets[p_index] = RD::get_singleton()->uniform_set_create(uniforms, scene_shader.default_shader_rd, RENDER_PASS_UNIFORM_SET, true);
	return render_pass_uniform_sets[p_index];
}

void RenderForwardMobile::_setup_lightmaps(const RenderDataRD *p_render_data, const PagedArray<RID> &p_lightmaps, const Transform3D &p_cam_transform) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();

	// This probably needs to change...
	scene_state.lightmaps_used = 0;
	for (int i = 0; i < (int)p_lightmaps.size(); i++) {
		if (i >= (int)scene_state.max_lightmaps) {
			break;
		}

		RID lightmap = light_storage->lightmap_instance_get_lightmap(p_lightmaps[i]);

		// Transform (for directional lightmaps).
		Basis to_lm = light_storage->lightmap_instance_get_transform(p_lightmaps[i]).basis.inverse() * p_cam_transform.basis;
		to_lm = to_lm.inverse().transposed(); //will transform normals
		RendererRD::MaterialStorage::store_transform_3x3(to_lm, scene_state.lightmaps[i].normal_xform);

		// Light texture size.
		Vector2i lightmap_size = light_storage->lightmap_get_light_texture_size(lightmap);
		scene_state.lightmaps[i].texture_size[0] = lightmap_size[0];
		scene_state.lightmaps[i].texture_size[1] = lightmap_size[1];

		// Exposure.
		scene_state.lightmaps[i].exposure_normalization = 1.0;
		scene_state.lightmaps[i].flags = light_storage->lightmap_get_shadowmask_mode(lightmap);
		if (p_render_data->camera_attributes.is_valid()) {
			float baked_exposure = light_storage->lightmap_get_baked_exposure_normalization(lightmap);
			float enf = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
			scene_state.lightmaps[i].exposure_normalization = enf / baked_exposure;
		}

		scene_state.lightmap_ids[i] = p_lightmaps[i];
		scene_state.lightmap_has_sh[i] = light_storage->lightmap_uses_spherical_harmonics(lightmap);

		scene_state.lightmaps_used++;
	}
	if (scene_state.lightmaps_used > 0) {
		RD::get_singleton()->buffer_update(scene_state.lightmap_buffer, 0, sizeof(LightmapData) * scene_state.lightmaps_used, scene_state.lightmaps);
	}
}

void RenderForwardMobile::_pre_opaque_render(RenderDataRD *p_render_data) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();

	p_render_data->cube_shadows.clear();
	p_render_data->shadows.clear();
	p_render_data->directional_shadows.clear();

	float lod_distance_multiplier = p_render_data->scene_data->cam_projection.get_lod_multiplier();
	{
		for (int i = 0; i < p_render_data->render_shadow_count; i++) {
			RID li = p_render_data->render_shadows[i].light;
			RID base = light_storage->light_instance_get_base_light(li);

			if (light_storage->light_get_type(base) == RS::LIGHT_DIRECTIONAL) {
				p_render_data->directional_shadows.push_back(i);
			} else if (light_storage->light_get_type(base) == RS::LIGHT_OMNI && light_storage->light_omni_get_shadow_mode(base) == RS::LIGHT_OMNI_SHADOW_CUBE) {
				p_render_data->cube_shadows.push_back(i);
			} else {
				p_render_data->shadows.push_back(i);
			}
		}

		//cube shadows are rendered in their own way
		for (const int &index : p_render_data->cube_shadows) {
			_render_shadow_pass(p_render_data->render_shadows[index].light, p_render_data->shadow_atlas, p_render_data->render_shadows[index].pass, p_render_data->render_shadows[index].instances, lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, true, true, true, p_render_data->render_info, p_render_data->scene_data->cam_transform);
		}

		if (p_render_data->directional_shadows.size()) {
			//open the pass for directional shadows
			light_storage->update_directional_shadow_atlas();
			RD::get_singleton()->draw_list_begin(light_storage->direction_shadow_get_fb(), RD::DRAW_CLEAR_DEPTH, Vector<Color>(), 0.0f);
			RD::get_singleton()->draw_list_end();
		}
	}

	bool render_shadows = p_render_data->directional_shadows.size() || p_render_data->shadows.size();

	//prepare shadow rendering
	if (render_shadows) {
		RENDER_TIMESTAMP("Render Shadows");

		_render_shadow_begin();

		//render directional shadows
		for (uint32_t i = 0; i < p_render_data->directional_shadows.size(); i++) {
			_render_shadow_pass(p_render_data->render_shadows[p_render_data->directional_shadows[i]].light, p_render_data->shadow_atlas, p_render_data->render_shadows[p_render_data->directional_shadows[i]].pass, p_render_data->render_shadows[p_render_data->directional_shadows[i]].instances, lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, false, i == p_render_data->directional_shadows.size() - 1, false, p_render_data->render_info, p_render_data->scene_data->cam_transform);
		}
		//render positional shadows
		for (uint32_t i = 0; i < p_render_data->shadows.size(); i++) {
			_render_shadow_pass(p_render_data->render_shadows[p_render_data->shadows[i]].light, p_render_data->shadow_atlas, p_render_data->render_shadows[p_render_data->shadows[i]].pass, p_render_data->render_shadows[p_render_data->shadows[i]].instances, lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, i == 0, i == p_render_data->shadows.size() - 1, true, p_render_data->render_info, p_render_data->scene_data->cam_transform);
		}

		_render_shadow_process();

		_render_shadow_end();
	}
}

void RenderForwardMobile::_render_scene(RenderDataRD *p_render_data, const Color &p_default_bg_color) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	ERR_FAIL_NULL(p_render_data);

	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	Ref<RenderBufferDataForwardMobile> rb_data;
	if (rb->has_custom_data(RB_SCOPE_MOBILE)) {
		// Our forward mobile custom data buffer will only be available when we're rendering our normal view.
		// This will not be available when rendering reflection probes.
		rb_data = rb->get_custom_data(RB_SCOPE_MOBILE);
	}
	bool is_reflection_probe = p_render_data->reflection_probe.is_valid();

	RENDER_TIMESTAMP("Prepare 3D Scene");

	_update_vrs(rb);

	RENDER_TIMESTAMP("Setup 3D Scene");

	/* TODO
	// check if we need motion vectors
	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_MOTION_VECTORS) {
		p_render_data->scene_data->calculate_motion_vectors = true;
	} else if (_compositor_effects_has_flag(p_render_data, RS::COMPOSITOR_EFFECT_FLAG_NEEDS_MOTION_VECTORS)) {
		p_render_data->scene_data->calculate_motion_vectors = true;
	} else if (render target has velocity override) { // TODO
		p_render_data->scene_data->calculate_motion_vectors = true;
	} else {
		p_render_data->scene_data->calculate_motion_vectors = false;
	}
	*/
	p_render_data->scene_data->calculate_motion_vectors = false; // for now, not yet supported...

	p_render_data->scene_data->directional_light_count = 0;
	p_render_data->scene_data->opaque_prepass_threshold = 0.0;

	// We can only use our full subpass approach if we're:
	// - not reading from SCREEN_TEXTURE/DEPTH_TEXTURE
	// - not using ssr/sss (currently not supported)
	// - not using glow or other post effects (can't do 4th subpass)
	// - rendering to a half sized render buffer (can't do 4th subpass)
	// We'll need to restrict how far we're going with subpasses based on this.

	Size2i screen_size;
	RID framebuffer;
	bool reverse_cull = p_render_data->scene_data->cam_transform.basis.determinant() < 0;
	bool merge_transparent_pass = true; // If true: we can do our transparent pass in the same pass as our opaque pass.
	bool using_subpass_post_process = true; // If true: we can do our post processing in a subpass
	RendererRD::MaterialStorage::Samplers samplers;

	RS::ViewportMSAA msaa = rb->get_msaa_3d();
	bool use_msaa = msaa != RS::VIEWPORT_MSAA_DISABLED;

	bool ce_has_post_opaque = _has_compositor_effect(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_OPAQUE, p_render_data);
	bool ce_has_pre_transparent = _has_compositor_effect(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT, p_render_data);
	bool ce_has_post_transparent = _has_compositor_effect(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT, p_render_data);

	if (ce_has_post_opaque) {
		// As we're doing opaque and sky in subpasses we don't support this *yet*
		WARN_PRINT_ONCE("Post opaque rendering effect callback is not supported in the mobile renderer");
	}

	// Using RenderingEffects limits our ability to do subpasses..
	if (ce_has_pre_transparent) {
		merge_transparent_pass = false;
		using_subpass_post_process = false;
	}
	if (ce_has_post_transparent) {
		using_subpass_post_process = false;
	}

	if (use_msaa && _compositor_effects_has_flag(p_render_data, RS::COMPOSITOR_EFFECT_FLAG_ACCESS_RESOLVED_DEPTH, RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_ANY)) {
		// We'll be able to do this when we finish PR #78598
		WARN_PRINT_ONCE("Depth buffer resolve for rendering effect callback is not supported in the mobile renderer");
	}

	// We don't need to check resolve color flag, it will be resolved for transparent passes regardless.

	bool using_shadows = true;

	if (p_render_data->reflection_probe.is_valid()) {
		if (!RSG::light_storage->reflection_probe_renders_shadows(light_storage->reflection_probe_instance_get_probe(p_render_data->reflection_probe))) {
			using_shadows = false;
		}
	} else {
		//do not render reflections when rendering a reflection probe
		light_storage->update_reflection_probe_buffer(p_render_data, *p_render_data->reflection_probes, p_render_data->scene_data->cam_transform.affine_inverse(), p_render_data->environment);
	}

	// Update light and decal buffer first so we know what lights and decals are safe to pair with.
	uint32_t directional_light_count = 0;
	uint32_t positional_light_count = 0;
	light_storage->update_light_buffers(p_render_data, *p_render_data->lights, p_render_data->scene_data->cam_transform, p_render_data->shadow_atlas, using_shadows, directional_light_count, positional_light_count, p_render_data->directional_light_soft_shadows);
	texture_storage->update_decal_buffer(*p_render_data->decals, p_render_data->scene_data->cam_transform);

	p_render_data->directional_light_count = directional_light_count;

	// fill our render lists early so we can find out if we use various features
	_fill_render_list(RENDER_LIST_OPAQUE, p_render_data, PASS_MODE_COLOR);
	render_list[RENDER_LIST_OPAQUE].sort_by_key();
	render_list[RENDER_LIST_ALPHA].sort_by_reverse_depth_and_priority();
	_fill_instance_data(RENDER_LIST_OPAQUE);
	_fill_instance_data(RENDER_LIST_ALPHA);

	if (p_render_data->render_info) {
		p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] = p_render_data->instances->size();
		p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] = p_render_data->instances->size();
	}

	if (is_reflection_probe) {
		uint32_t resolution = light_storage->reflection_probe_instance_get_resolution(p_render_data->reflection_probe);
		screen_size.x = resolution;
		screen_size.y = resolution;

		framebuffer = light_storage->reflection_probe_instance_get_framebuffer(p_render_data->reflection_probe, p_render_data->reflection_probe_pass);

		if (light_storage->reflection_probe_is_interior(light_storage->reflection_probe_instance_get_probe(p_render_data->reflection_probe))) {
			p_render_data->environment = RID(); //no environment on interiors
		}

		reverse_cull = true;
		merge_transparent_pass = true; // we ignore our screen/depth texture here
		using_subpass_post_process = false; // not applicable at all for reflection probes.
		samplers = RendererRD::MaterialStorage::get_singleton()->samplers_rd_get_default();

		// Indicate pipelines for reflection probes are required.
		global_pipeline_data_required.use_reflection_probes = true;
	} else if (rb_data.is_valid()) {
		// setup rendering to render buffer
		screen_size = p_render_data->render_buffers->get_internal_size();

		if (rb->get_scaling_3d_mode() != RS::VIEWPORT_SCALING_3D_MODE_OFF) {
			// can't do blit subpass because we're scaling
			using_subpass_post_process = false;
		} else if (p_render_data->environment.is_valid() && (environment_get_glow_enabled(p_render_data->environment) || RSG::camera_attributes->camera_attributes_uses_auto_exposure(p_render_data->camera_attributes) || RSG::camera_attributes->camera_attributes_uses_dof(p_render_data->camera_attributes) || environment_get_background(p_render_data->environment) == RS::ENV_BG_CANVAS)) {
			// can't do blit subpass because we're using post processes
			using_subpass_post_process = false;
		}

		if (scene_state.used_screen_texture || scene_state.used_depth_texture) {
			// can't use our last two subpasses because we're reading from screen texture or depth texture
			merge_transparent_pass = false;
			using_subpass_post_process = false;
		}

		if (using_subpass_post_process) {
			// We can do all in one go.
			framebuffer = rb_data->get_color_fbs(RenderBufferDataForwardMobile::FB_CONFIG_RENDER_AND_POST_PASS);
		} else {
			// We separate things out.
			framebuffer = rb_data->get_color_fbs(RenderBufferDataForwardMobile::FB_CONFIG_RENDER_PASS);
		}
		samplers = rb->get_samplers();
	} else {
		ERR_FAIL(); //bug?
	}

	if (p_render_data->scene_data->view_count > 1) {
		global_pipeline_data_required.use_multiview = true;
	}

	if (scene_state.used_lightmap) {
		global_pipeline_data_required.use_lightmaps = true;
	}

	_update_dirty_geometry_pipelines();

	p_render_data->scene_data->emissive_exposure_normalization = -1.0;

	RD::get_singleton()->draw_command_begin_label("Render Setup");

	_setup_lightmaps(p_render_data, *p_render_data->lightmaps, p_render_data->scene_data->cam_transform);
	_setup_environment(p_render_data, is_reflection_probe, screen_size, p_default_bg_color, false);

	// May have changed due to the above (light buffer enlarged, as an example).
	_update_render_base_uniform_set();

	RD::get_singleton()->draw_command_end_label(); // Render Setup

	// setup environment
	RID radiance_texture;
	bool draw_sky = false;
	bool draw_sky_fog_only = false;
	// We invert luminance_multiplier for sky so that we can combine it with exposure value.
	float inverse_luminance_multiplier = 1.0 / _render_buffers_get_luminance_multiplier();
	float sky_energy_multiplier = inverse_luminance_multiplier;

	Color clear_color = p_default_bg_color;
	bool load_color = false;
	bool copy_canvas = false;
	bool use_ambient_cubemap = false;
	bool use_reflection_cubemap = false;

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW) {
		clear_color = Color(0, 0, 0, 1); //in overdraw mode, BG should always be black
	} else if (is_environment(p_render_data->environment)) {
		RS::EnvironmentAmbientSource ambient_source = environment_get_ambient_source(p_render_data->environment);
		RS::EnvironmentBG bg_mode = environment_get_background(p_render_data->environment);
		float bg_energy_multiplier = environment_get_bg_energy_multiplier(p_render_data->environment);
		bg_energy_multiplier *= environment_get_bg_intensity(p_render_data->environment);
		RS::EnvironmentReflectionSource reflection_source = environment_get_reflection_source(p_render_data->environment);
		use_ambient_cubemap = (ambient_source == RS::ENV_AMBIENT_SOURCE_BG && bg_mode == RS::ENV_BG_SKY) || ambient_source == RS::ENV_AMBIENT_SOURCE_SKY;
		use_reflection_cubemap = (reflection_source == RS::ENV_REFLECTION_SOURCE_BG && bg_mode == RS::ENV_BG_SKY) || reflection_source == RS::ENV_REFLECTION_SOURCE_SKY;

		if (p_render_data->camera_attributes.is_valid()) {
			bg_energy_multiplier *= RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_render_data->camera_attributes);
		}

		switch (bg_mode) {
			case RS::ENV_BG_CLEAR_COLOR: {
				clear_color = p_default_bg_color;
				clear_color.r *= bg_energy_multiplier;
				clear_color.g *= bg_energy_multiplier;
				clear_color.b *= bg_energy_multiplier;
				if (!p_render_data->transparent_bg && environment_get_fog_enabled(p_render_data->environment)) {
					draw_sky_fog_only = true;
					RendererRD::MaterialStorage::get_singleton()->material_set_param(sky.sky_scene_state.fog_material, "clear_color", Variant(clear_color.srgb_to_linear()));
				}
			} break;
			case RS::ENV_BG_COLOR: {
				clear_color = environment_get_bg_color(p_render_data->environment);
				clear_color.r *= bg_energy_multiplier;
				clear_color.g *= bg_energy_multiplier;
				clear_color.b *= bg_energy_multiplier;
				if (!p_render_data->transparent_bg && environment_get_fog_enabled(p_render_data->environment)) {
					draw_sky_fog_only = true;
					RendererRD::MaterialStorage::get_singleton()->material_set_param(sky.sky_scene_state.fog_material, "clear_color", Variant(clear_color.srgb_to_linear()));
				}
			} break;
			case RS::ENV_BG_SKY: {
				draw_sky = !p_render_data->transparent_bg;
			} break;
			case RS::ENV_BG_CANVAS: {
				if (rb_data.is_valid()) {
					copy_canvas = true;
				}
			} break;
			case RS::ENV_BG_KEEP: {
				load_color = true;
			} break;
			case RS::ENV_BG_CAMERA_FEED: {
			} break;
			default: {
			}
		}

		// setup sky if used for ambient, reflections, or background
		if (draw_sky || draw_sky_fog_only || (reflection_source == RS::ENV_REFLECTION_SOURCE_BG && bg_mode == RS::ENV_BG_SKY) || reflection_source == RS::ENV_REFLECTION_SOURCE_SKY || ambient_source == RS::ENV_AMBIENT_SOURCE_SKY) {
			RENDER_TIMESTAMP("Setup Sky");
			RD::get_singleton()->draw_command_begin_label("Setup Sky");

			sky.setup_sky(p_render_data, screen_size);

			sky_energy_multiplier *= bg_energy_multiplier;

			RID sky_rid = environment_get_sky(p_render_data->environment);
			if (sky_rid.is_valid()) {
				sky.update_radiance_buffers(rb, p_render_data->environment, p_render_data->scene_data->cam_transform.origin, time, sky_energy_multiplier);
				radiance_texture = sky.sky_get_radiance_texture_rd(sky_rid);
			} else {
				// do not try to draw sky if invalid
				draw_sky = false;
			}

			if (draw_sky || draw_sky_fog_only) {
				// update sky half/quarter res buffers (if required)
				sky.update_res_buffers(rb, p_render_data->environment, time, sky_energy_multiplier);
			}
			RD::get_singleton()->draw_command_end_label(); // Setup Sky
		}
	} else {
		clear_color = p_default_bg_color;
	}

	// We don't have access to any rendered buffers but we may be able to effect mesh data...
	_process_compositor_effects(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE, p_render_data);

	_pre_opaque_render(p_render_data);

	SceneShaderForwardMobile::ShaderSpecialization base_specialization = scene_shader.default_specialization;

	{
		base_specialization.use_directional_soft_shadows = p_render_data->directional_light_count > 0 ? p_render_data->directional_light_soft_shadows : false;
		base_specialization.directional_lights = p_render_data->directional_light_count;
		base_specialization.directional_light_blend_splits = light_storage->get_directional_light_blend_splits(p_render_data->directional_light_count);

		if (!is_environment(p_render_data->environment) || !environment_get_fog_enabled(p_render_data->environment)) {
			base_specialization.disable_fog = true;
			base_specialization.use_fog_aerial_perspective = false;
			base_specialization.use_fog_sun_scatter = false;
			base_specialization.use_fog_height_density = false;
			base_specialization.use_depth_fog = false;
		} else {
			base_specialization.disable_fog = false;
			base_specialization.use_fog_aerial_perspective = environment_get_fog_aerial_perspective(p_render_data->environment) > 0.0;
			base_specialization.use_fog_sun_scatter = environment_get_fog_sun_scatter(p_render_data->environment) > 0.001;
			base_specialization.use_fog_height_density = abs(environment_get_fog_height_density(p_render_data->environment)) >= 0.0001;
			base_specialization.use_depth_fog = p_render_data->environment.is_valid() && environment_get_fog_mode(p_render_data->environment) == RS::EnvironmentFogMode::ENV_FOG_MODE_DEPTH;
		}

		base_specialization.scene_use_ambient_cubemap = use_ambient_cubemap;
		base_specialization.scene_use_reflection_cubemap = use_reflection_cubemap;
		base_specialization.scene_roughness_limiter_enabled = p_render_data->render_buffers.is_valid() && screen_space_roughness_limiter_is_active();
	}

	{
		RDD::BreadcrumbMarker breadcrumb;
		if (rb_data.is_valid()) {
			RD::get_singleton()->draw_command_begin_label("Render 3D Pass");
			breadcrumb = RDD::BreadcrumbMarker::OPAQUE_PASS;
		} else {
			RD::get_singleton()->draw_command_begin_label("Render Reflection Probe Pass");
			breadcrumb = RDD::BreadcrumbMarker::REFLECTION_PROBES;
		}

		// opaque pass
		RD::get_singleton()->draw_command_begin_label("Render Opaque");

		p_render_data->scene_data->directional_light_count = p_render_data->directional_light_count;

		// Shadow pass can change the base uniform set samplers.
		_update_render_base_uniform_set();

		_setup_environment(p_render_data, is_reflection_probe, screen_size, p_default_bg_color, p_render_data->render_buffers.is_valid());

		if (merge_transparent_pass && using_subpass_post_process) {
			RENDER_TIMESTAMP("Render Opaque + Transparent + Tonemap");
		} else if (merge_transparent_pass) {
			RENDER_TIMESTAMP("Render Opaque + Transparent");
		} else {
			RENDER_TIMESTAMP("Render Opaque");
		}

		RID rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_OPAQUE, p_render_data, radiance_texture, samplers, true);

		// Set clear colors.
		Vector<Color> c;
		if (!load_color) {
			Color cc = clear_color.srgb_to_linear() * inverse_luminance_multiplier;
			if (rb_data.is_valid()) {
				cc.a = 0; // For transparent viewport backgrounds.
			}

			c.push_back(cc); // Our render buffer.
			if (rb_data.is_valid()) {
				if (use_msaa) {
					c.push_back(clear_color.srgb_to_linear() * inverse_luminance_multiplier); // Our resolve buffer.
				}
				if (using_subpass_post_process) {
					c.push_back(Color()); // Our 2D buffer we're copying into.
				}
			}
		}

		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer, load_color ? RD::DRAW_CLEAR_DEPTH : (RD::DRAW_CLEAR_COLOR_0 | RD::DRAW_CLEAR_DEPTH), c, 0.0f, 0, Rect2(), breadcrumb);
		RD::FramebufferFormatID fb_format = RD::get_singleton()->framebuffer_get_format(framebuffer);

		if (copy_canvas) {
			if (p_render_data->scene_data->view_count > 1) {
				WARN_PRINT_ONCE("Canvas background is not supported in multiview!");
			} else {
				RID texture = RendererRD::TextureStorage::get_singleton()->render_target_get_rd_texture(rb->get_render_target());
				bool convert_to_linear = !RendererRD::TextureStorage::get_singleton()->render_target_is_using_hdr(rb->get_render_target());

				copy_effects->copy_to_drawlist(draw_list, fb_format, texture, convert_to_linear);
			}
		}

		if (render_list[RENDER_LIST_OPAQUE].elements.size() > 0) {
			RenderListParameters render_list_params(render_list[RENDER_LIST_OPAQUE].elements.ptr(), render_list[RENDER_LIST_OPAQUE].element_info.ptr(), render_list[RENDER_LIST_OPAQUE].elements.size(), reverse_cull, PASS_MODE_COLOR, rp_uniform_set, base_specialization, get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME, Vector2(), p_render_data->scene_data->lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, p_render_data->scene_data->view_count);
			render_list_params.framebuffer_format = fb_format;
			render_list_params.subpass = RD::get_singleton()->draw_list_get_current_pass(); // Should now always be 0.

			_render_list(draw_list, fb_format, &render_list_params, 0, render_list_params.element_count);
		}

		RD::get_singleton()->draw_command_end_label(); //Render Opaque

		if (draw_sky || draw_sky_fog_only) {
			RD::get_singleton()->draw_command_begin_label("Draw Sky");

			// Note, sky.setup should have been called up above and setup stuff we need.

			sky.draw_sky(draw_list, rb, p_render_data->environment, framebuffer, time, sky_energy_multiplier);

			RD::get_singleton()->draw_command_end_label(); // Draw Sky
		}

		if (merge_transparent_pass) {
			if (render_list[RENDER_LIST_ALPHA].element_info.size() > 0) {
				// transparent pass

				RD::get_singleton()->draw_command_begin_label("Render Transparent");

				rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_ALPHA, p_render_data, radiance_texture, samplers, true);

				RenderListParameters render_list_params(render_list[RENDER_LIST_ALPHA].elements.ptr(), render_list[RENDER_LIST_ALPHA].element_info.ptr(), render_list[RENDER_LIST_ALPHA].elements.size(), reverse_cull, PASS_MODE_COLOR_TRANSPARENT, rp_uniform_set, base_specialization, get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME, Vector2(), p_render_data->scene_data->lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, p_render_data->scene_data->view_count);
				render_list_params.framebuffer_format = fb_format;
				render_list_params.subpass = RD::get_singleton()->draw_list_get_current_pass(); // Should now always be 0.

				_render_list(draw_list, fb_format, &render_list_params, 0, render_list_params.element_count);

				RD::get_singleton()->draw_command_end_label(); // Render Transparent Subpass
			}

			// note if we are using MSAA we should get an automatic resolve through our subpass configuration.

			// blit to tonemap
			if (rb_data.is_valid() && using_subpass_post_process) {
				_post_process_subpass(p_render_data->render_buffers->get_internal_texture(), framebuffer, p_render_data);
			}

			RD::get_singleton()->draw_command_end_label(); // Render 3D Pass / Render Reflection Probe Pass

			RD::get_singleton()->draw_list_end();
		} else {
			// We're done with our subpasses so end our container pass
			// note, if MSAA is used we should get an automatic resolve here

			RD::get_singleton()->draw_list_end();

			RD::get_singleton()->draw_command_end_label(); // Render 3D Pass / Render Reflection Probe Pass

			// rendering effects
			if (ce_has_pre_transparent) {
				_process_compositor_effects(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT, p_render_data);
			}

			if (scene_state.used_screen_texture) {
				// Copy screen texture to backbuffer so we can read from it
				_render_buffers_copy_screen_texture(p_render_data);
			}

			if (scene_state.used_depth_texture) {
				// Copy depth texture to backbuffer so we can read from it
				_render_buffers_copy_depth_texture(p_render_data);
			}

			if (render_list[RENDER_LIST_ALPHA].element_info.size() > 0) {
				RD::get_singleton()->draw_command_begin_label("Render Transparent Pass");
				RENDER_TIMESTAMP("Render Transparent");

				rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_ALPHA, p_render_data, radiance_texture, samplers, true);

				// this may be needed if we re-introduced steps that change info, not sure which do so in the previous implementation
				//_setup_environment(p_render_data, is_reflection_probe, screen_size, p_default_bg_color, false);

				RenderListParameters render_list_params(render_list[RENDER_LIST_ALPHA].elements.ptr(), render_list[RENDER_LIST_ALPHA].element_info.ptr(), render_list[RENDER_LIST_ALPHA].elements.size(), reverse_cull, PASS_MODE_COLOR, rp_uniform_set, base_specialization, get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_WIREFRAME, Vector2(), p_render_data->scene_data->lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, p_render_data->scene_data->view_count);
				render_list_params.framebuffer_format = fb_format;
				render_list_params.subpass = RD::get_singleton()->draw_list_get_current_pass(); // Should now always be 0.

				draw_list = RD::get_singleton()->draw_list_begin(framebuffer, RD::DRAW_DEFAULT_ALL, Vector<Color>(), 1.0f, 0, Rect2(), breadcrumb);
				_render_list(draw_list, fb_format, &render_list_params, 0, render_list_params.element_count);
				RD::get_singleton()->draw_list_end();

				RD::get_singleton()->draw_command_end_label(); // Render Transparent Pass
			}
		}
	}

	if (rb_data.is_valid() && !using_subpass_post_process) {
		RD::get_singleton()->draw_command_begin_label("Post process pass");

		if (ce_has_post_transparent) {
			_process_compositor_effects(RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT, p_render_data);
		}

		// If we need extra effects we do this in its own pass
		RENDER_TIMESTAMP("Tonemap");

		_render_buffers_post_process_and_tonemap(p_render_data);

		RD::get_singleton()->draw_command_end_label(); // Post process pass
	}

	if (rb_data.is_valid()) {
		_disable_clear_request(p_render_data);
	}

	_render_buffers_debug_draw(p_render_data);
}

/* these are being called from RendererSceneRenderRD::_pre_opaque_render */

void RenderForwardMobile::_render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, float p_lod_distance_multiplier, float p_screen_mesh_lod_threshold, bool p_open_pass, bool p_close_pass, bool p_clear_region, RenderingMethod::RenderInfo *p_render_info, const Transform3D &p_main_cam_transform) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();

	ERR_FAIL_COND(!light_storage->owns_light_instance(p_light));

	RID base = light_storage->light_instance_get_base_light(p_light);

	Rect2i atlas_rect;
	uint32_t atlas_size = 1;
	RID atlas_fb;

	bool using_dual_paraboloid = false;
	bool using_dual_paraboloid_flip = false;
	Vector2i dual_paraboloid_offset;
	RID render_fb;
	RID render_texture;
	float zfar;

	bool use_pancake = false;
	bool render_cubemap = false;
	bool finalize_cubemap = false;

	bool flip_y = false;

	Projection light_projection;
	Transform3D light_transform;

	if (light_storage->light_get_type(base) == RS::LIGHT_DIRECTIONAL) {
		//set pssm stuff
		uint64_t last_scene_shadow_pass = light_storage->light_instance_get_shadow_pass(p_light);
		if (last_scene_shadow_pass != get_scene_pass()) {
			light_storage->light_instance_set_directional_rect(p_light, light_storage->get_directional_shadow_rect());
			light_storage->directional_shadow_increase_current_light();
			light_storage->light_instance_set_shadow_pass(p_light, get_scene_pass());
		}

		use_pancake = light_storage->light_get_param(base, RS::LIGHT_PARAM_SHADOW_PANCAKE_SIZE) > 0;
		light_projection = light_storage->light_instance_get_shadow_camera(p_light, p_pass);
		light_transform = light_storage->light_instance_get_shadow_transform(p_light, p_pass);

		atlas_rect = light_storage->light_instance_get_directional_rect(p_light);

		if (light_storage->light_directional_get_shadow_mode(base) == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS) {
			atlas_rect.size.width /= 2;
			atlas_rect.size.height /= 2;

			if (p_pass == 1) {
				atlas_rect.position.x += atlas_rect.size.width;
			} else if (p_pass == 2) {
				atlas_rect.position.y += atlas_rect.size.height;
			} else if (p_pass == 3) {
				atlas_rect.position += atlas_rect.size;
			}
		} else if (light_storage->light_directional_get_shadow_mode(base) == RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS) {
			atlas_rect.size.height /= 2;

			if (p_pass == 0) {
			} else {
				atlas_rect.position.y += atlas_rect.size.height;
			}
		}

		float directional_shadow_size = light_storage->directional_shadow_get_size();
		Rect2 atlas_rect_norm = atlas_rect;
		atlas_rect_norm.position /= directional_shadow_size;
		atlas_rect_norm.size /= directional_shadow_size;
		light_storage->light_instance_set_directional_shadow_atlas_rect(p_light, p_pass, atlas_rect_norm);

		zfar = RSG::light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);

		render_fb = light_storage->direction_shadow_get_fb();
		render_texture = RID();
		flip_y = true;

	} else {
		//set from shadow atlas

		ERR_FAIL_COND(!light_storage->owns_shadow_atlas(p_shadow_atlas));
		ERR_FAIL_COND(!light_storage->shadow_atlas_owns_light_instance(p_shadow_atlas, p_light));

		RSG::light_storage->shadow_atlas_update(p_shadow_atlas);

		uint32_t key = light_storage->shadow_atlas_get_light_instance_key(p_shadow_atlas, p_light);

		uint32_t quadrant = (key >> RendererRD::LightStorage::QUADRANT_SHIFT) & 0x3;
		uint32_t shadow = key & RendererRD::LightStorage::SHADOW_INDEX_MASK;
		uint32_t subdivision = light_storage->shadow_atlas_get_quadrant_subdivision(p_shadow_atlas, quadrant);

		ERR_FAIL_INDEX((int)shadow, light_storage->shadow_atlas_get_quadrant_shadow_size(p_shadow_atlas, quadrant));

		uint32_t shadow_atlas_size = light_storage->shadow_atlas_get_size(p_shadow_atlas);
		uint32_t quadrant_size = shadow_atlas_size >> 1;

		atlas_rect.position.x = (quadrant & 1) * quadrant_size;
		atlas_rect.position.y = (quadrant >> 1) * quadrant_size;

		uint32_t shadow_size = (quadrant_size / subdivision);
		atlas_rect.position.x += (shadow % subdivision) * shadow_size;
		atlas_rect.position.y += (shadow / subdivision) * shadow_size;

		atlas_rect.size.width = shadow_size;
		atlas_rect.size.height = shadow_size;

		zfar = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);

		if (light_storage->light_get_type(base) == RS::LIGHT_OMNI) {
			bool wrap = (shadow + 1) % subdivision == 0;
			dual_paraboloid_offset = wrap ? Vector2i(1 - subdivision, 1) : Vector2i(1, 0);

			if (light_storage->light_omni_get_shadow_mode(base) == RS::LIGHT_OMNI_SHADOW_CUBE) {
				render_texture = light_storage->get_cubemap(shadow_size / 2);
				render_fb = light_storage->get_cubemap_fb(shadow_size / 2, p_pass);

				light_projection = light_storage->light_instance_get_shadow_camera(p_light, p_pass);
				light_transform = light_storage->light_instance_get_shadow_transform(p_light, p_pass);
				render_cubemap = true;
				finalize_cubemap = p_pass == 5;
				atlas_fb = light_storage->shadow_atlas_get_fb(p_shadow_atlas);

				atlas_size = shadow_atlas_size;

				if (p_pass == 0) {
					_render_shadow_begin();
				}

			} else {
				atlas_rect.position.x += 1;
				atlas_rect.position.y += 1;
				atlas_rect.size.x -= 2;
				atlas_rect.size.y -= 2;

				atlas_rect.position += p_pass * atlas_rect.size * dual_paraboloid_offset;

				light_projection = light_storage->light_instance_get_shadow_camera(p_light, 0);
				light_transform = light_storage->light_instance_get_shadow_transform(p_light, 0);

				using_dual_paraboloid = true;
				using_dual_paraboloid_flip = p_pass == 1;
				render_fb = light_storage->shadow_atlas_get_fb(p_shadow_atlas);
				flip_y = true;
			}

		} else if (light_storage->light_get_type(base) == RS::LIGHT_SPOT) {
			light_projection = light_storage->light_instance_get_shadow_camera(p_light, 0);
			light_transform = light_storage->light_instance_get_shadow_transform(p_light, 0);

			render_fb = light_storage->shadow_atlas_get_fb(p_shadow_atlas);

			flip_y = true;
		}
	}

	if (render_cubemap) {
		//rendering to cubemap
		_render_shadow_append(render_fb, p_instances, light_projection, light_transform, zfar, 0, 0, false, false, use_pancake, p_lod_distance_multiplier, p_screen_mesh_lod_threshold, Rect2(), false, true, true, true, p_render_info, p_main_cam_transform);
		if (finalize_cubemap) {
			_render_shadow_process();
			_render_shadow_end();

			// reblit
			Rect2 atlas_rect_norm = atlas_rect;
			atlas_rect_norm.position /= float(atlas_size);
			atlas_rect_norm.size /= float(atlas_size);
			copy_effects->copy_cubemap_to_dp(render_texture, atlas_fb, atlas_rect_norm, atlas_rect.size, light_projection.get_z_near(), zfar, false);
			atlas_rect_norm.position += Vector2(dual_paraboloid_offset) * atlas_rect_norm.size;
			copy_effects->copy_cubemap_to_dp(render_texture, atlas_fb, atlas_rect_norm, atlas_rect.size, light_projection.get_z_near(), zfar, true);

			//restore transform so it can be properly used
			light_storage->light_instance_set_shadow_transform(p_light, Projection(), light_storage->light_instance_get_base_transform(p_light), zfar, 0, 0, 0);
		}

	} else {
		//render shadow
		_render_shadow_append(render_fb, p_instances, light_projection, light_transform, zfar, 0, 0, using_dual_paraboloid, using_dual_paraboloid_flip, use_pancake, p_lod_distance_multiplier, p_screen_mesh_lod_threshold, atlas_rect, flip_y, p_clear_region, p_open_pass, p_close_pass, p_render_info, p_main_cam_transform);
	}
}

void RenderForwardMobile::_render_shadow_begin() {
	scene_state.shadow_passes.clear();
	RD::get_singleton()->draw_command_begin_label("Shadow Setup");
	_update_render_base_uniform_set();

	render_list[RENDER_LIST_SECONDARY].clear();
}

void RenderForwardMobile::_render_shadow_append(RID p_framebuffer, const PagedArray<RenderGeometryInstance *> &p_instances, const Projection &p_projection, const Transform3D &p_transform, float p_zfar, float p_bias, float p_normal_bias, bool p_use_dp, bool p_use_dp_flip, bool p_use_pancake, float p_lod_distance_multiplier, float p_screen_mesh_lod_threshold, const Rect2i &p_rect, bool p_flip_y, bool p_clear_region, bool p_begin, bool p_end, RenderingMethod::RenderInfo *p_render_info, const Transform3D &p_main_cam_transform) {
	uint32_t shadow_pass_index = scene_state.shadow_passes.size();

	SceneState::ShadowPass shadow_pass;

	if (p_render_info) {
		p_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] = p_instances.size();
		p_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] = p_instances.size();
	}

	RenderSceneDataRD scene_data;
	scene_data.flip_y = !p_flip_y; // Q: Why is this inverted? Do we assume flip in shadow logic?
	scene_data.cam_projection = p_projection;
	scene_data.cam_transform = p_transform;
	scene_data.view_projection[0] = p_projection;
	scene_data.z_near = 0.0;
	scene_data.z_far = p_zfar;
	scene_data.lod_distance_multiplier = p_lod_distance_multiplier;
	scene_data.dual_paraboloid_side = p_use_dp_flip ? -1 : 1;
	scene_data.opaque_prepass_threshold = 0.1;
	scene_data.time = time;
	scene_data.time_step = time_step;
	scene_data.main_cam_transform = p_main_cam_transform;

	RenderDataRD render_data;
	render_data.scene_data = &scene_data;
	render_data.instances = &p_instances;
	render_data.render_info = p_render_info;

	_setup_environment(&render_data, true, Vector2(1, 1), Color(), false, p_use_pancake, shadow_pass_index);

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_DISABLE_LOD) {
		scene_data.screen_mesh_lod_threshold = 0.0;
	} else {
		scene_data.screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
	}

	PassMode pass_mode = p_use_dp ? PASS_MODE_SHADOW_DP : PASS_MODE_SHADOW;

	uint32_t render_list_from = render_list[RENDER_LIST_SECONDARY].elements.size();
	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode, true);
	uint32_t render_list_size = render_list[RENDER_LIST_SECONDARY].elements.size() - render_list_from;
	render_list[RENDER_LIST_SECONDARY].sort_by_key_range(render_list_from, render_list_size);
	_fill_instance_data(RENDER_LIST_SECONDARY, render_list_from, render_list_size);

	{
		//regular forward for now
		bool flip_cull = p_use_dp_flip;
		if (p_flip_y) {
			flip_cull = !flip_cull;
		}

		shadow_pass.element_from = render_list_from;
		shadow_pass.element_count = render_list_size;
		shadow_pass.flip_cull = flip_cull;
		shadow_pass.pass_mode = pass_mode;

		shadow_pass.rp_uniform_set = RID(); //will be filled later when instance buffer is complete
		shadow_pass.screen_mesh_lod_threshold = scene_data.screen_mesh_lod_threshold;
		shadow_pass.lod_distance_multiplier = scene_data.lod_distance_multiplier;

		shadow_pass.framebuffer = p_framebuffer;
		shadow_pass.clear_depth = p_begin || p_clear_region;
		shadow_pass.rect = p_rect;

		scene_state.shadow_passes.push_back(shadow_pass);
	}
}

void RenderForwardMobile::_render_shadow_process() {
	//render shadows one after the other, so this can be done un-barriered and the driver can optimize (as well as allow us to run compute at the same time)

	for (uint32_t i = 0; i < scene_state.shadow_passes.size(); i++) {
		//render passes need to be configured after instance buffer is done, since they need the latest version
		SceneState::ShadowPass &shadow_pass = scene_state.shadow_passes[i];
		shadow_pass.rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_SECONDARY, nullptr, RID(), RendererRD::MaterialStorage::get_singleton()->samplers_rd_get_default(), false, i);
	}

	RD::get_singleton()->draw_command_end_label();
}

void RenderForwardMobile::_render_shadow_end() {
	RD::get_singleton()->draw_command_begin_label("Shadow Render");

	for (SceneState::ShadowPass &shadow_pass : scene_state.shadow_passes) {
		RenderListParameters render_list_parameters(render_list[RENDER_LIST_SECONDARY].elements.ptr() + shadow_pass.element_from, render_list[RENDER_LIST_SECONDARY].element_info.ptr() + shadow_pass.element_from, shadow_pass.element_count, shadow_pass.flip_cull, shadow_pass.pass_mode, shadow_pass.rp_uniform_set, scene_shader.default_specialization, false, Vector2(), shadow_pass.lod_distance_multiplier, shadow_pass.screen_mesh_lod_threshold, 1, shadow_pass.element_from);
		_render_list_with_draw_list(&render_list_parameters, shadow_pass.framebuffer, shadow_pass.clear_depth ? RD::DRAW_CLEAR_DEPTH : RD::DRAW_DEFAULT_ALL, Vector<Color>(), 0.0f, 0, shadow_pass.rect);
	}

	RD::get_singleton()->draw_command_end_label();
}

/* */

void RenderForwardMobile::_render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region, float p_exposure_normalization) {
	RENDER_TIMESTAMP("Setup Rendering 3D Material");

	RD::get_singleton()->draw_command_begin_label("Render 3D Material");

	_update_render_base_uniform_set();

	RenderSceneDataRD scene_data;
	scene_data.cam_projection = p_cam_projection;
	scene_data.cam_transform = p_cam_transform;
	scene_data.view_projection[0] = p_cam_projection;
	scene_data.dual_paraboloid_side = 0;
	scene_data.material_uv2_mode = false;
	scene_data.opaque_prepass_threshold = 0.0f;
	scene_data.emissive_exposure_normalization = p_exposure_normalization;
	scene_data.time = time;
	scene_data.time_step = time_step;
	scene_data.main_cam_transform = p_cam_transform;

	RenderDataRD render_data;
	render_data.scene_data = &scene_data;
	render_data.instances = &p_instances;

	_setup_environment(&render_data, true, Vector2(1, 1), Color());

	PassMode pass_mode = PASS_MODE_DEPTH_MATERIAL;
	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();
	_fill_instance_data(RENDER_LIST_SECONDARY);

	RID rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_SECONDARY, nullptr, RID(), RendererRD::MaterialStorage::get_singleton()->samplers_rd_get_default());

	RENDER_TIMESTAMP("Render 3D Material");

	{
		RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].element_info.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), true, pass_mode, rp_uniform_set, scene_shader.default_specialization);
		//regular forward for now
		Vector<Color> clear = {
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0)
		};
		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_framebuffer, RD::DRAW_CLEAR_ALL, clear, 0.0f, 0, p_region);
		_render_list(draw_list, RD::get_singleton()->framebuffer_get_format(p_framebuffer), &render_list_params, 0, render_list_params.element_count);
		RD::get_singleton()->draw_list_end();
	}

	RD::get_singleton()->draw_command_end_label();
}

void RenderForwardMobile::_render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) {
	RENDER_TIMESTAMP("Setup Rendering UV2");

	RD::get_singleton()->draw_command_begin_label("Render UV2");

	_update_render_base_uniform_set();

	RenderSceneDataRD scene_data;
	scene_data.dual_paraboloid_side = 0;
	scene_data.material_uv2_mode = true;
	scene_data.emissive_exposure_normalization = -1.0;

	RenderDataRD render_data;
	render_data.scene_data = &scene_data;
	render_data.instances = &p_instances;

	_setup_environment(&render_data, true, Vector2(1, 1), Color());

	PassMode pass_mode = PASS_MODE_DEPTH_MATERIAL;
	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();
	_fill_instance_data(RENDER_LIST_SECONDARY);

	RID rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_SECONDARY, nullptr, RID(), RendererRD::MaterialStorage::get_singleton()->samplers_rd_get_default());

	RENDER_TIMESTAMP("Render 3D Material");

	{
		RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].element_info.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), true, pass_mode, rp_uniform_set, scene_shader.default_specialization, false);
		//regular forward for now
		Vector<Color> clear = {
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0),
			Color(0, 0, 0, 0)
		};

		RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_framebuffer, RD::DRAW_CLEAR_ALL, clear, 0.0f, 0, p_region);

		const int uv_offset_count = 9;
		static const Vector2 uv_offsets[uv_offset_count] = {
			Vector2(-1, 1),
			Vector2(1, 1),
			Vector2(1, -1),
			Vector2(-1, -1),
			Vector2(-1, 0),
			Vector2(1, 0),
			Vector2(0, -1),
			Vector2(0, 1),
			Vector2(0, 0),

		};

		for (int i = 0; i < uv_offset_count; i++) {
			Vector2 ofs = uv_offsets[i];
			ofs.x /= p_region.size.width;
			ofs.y /= p_region.size.height;
			render_list_params.uv_offset = ofs;
			_render_list(draw_list, RD::get_singleton()->framebuffer_get_format(p_framebuffer), &render_list_params, 0, render_list_params.element_count); //first wireframe, for pseudo conservative
		}
		render_list_params.uv_offset = Vector2();
		_render_list(draw_list, RD::get_singleton()->framebuffer_get_format(p_framebuffer), &render_list_params, 0, render_list_params.element_count); //second regular triangles

		RD::get_singleton()->draw_list_end();
	}

	RD::get_singleton()->draw_command_end_label();
}

void RenderForwardMobile::_render_sdfgi(Ref<RenderSceneBuffersRD> p_render_buffers, const Vector3i &p_from, const Vector3i &p_size, const AABB &p_bounds, const PagedArray<RenderGeometryInstance *> &p_instances, const RID &p_albedo_texture, const RID &p_emission_texture, const RID &p_emission_aniso_texture, const RID &p_geom_facing_texture, float p_exposure_normalization) {
	// we don't do SDFGI in low end..
}

void RenderForwardMobile::_render_particle_collider_heightfield(RID p_fb, const Transform3D &p_cam_transform, const Projection &p_cam_projection, const PagedArray<RenderGeometryInstance *> &p_instances) {
	RENDER_TIMESTAMP("Setup GPUParticlesCollisionHeightField3D");

	RD::get_singleton()->draw_command_begin_label("Render Collider Heightfield");

	_update_render_base_uniform_set();

	RenderSceneDataRD scene_data;
	scene_data.flip_y = true;
	scene_data.cam_projection = p_cam_projection;
	scene_data.cam_transform = p_cam_transform;
	scene_data.view_projection[0] = p_cam_projection;
	scene_data.z_near = 0.0;
	scene_data.z_far = p_cam_projection.get_z_far();
	scene_data.dual_paraboloid_side = 0;
	scene_data.opaque_prepass_threshold = 0.0;
	scene_data.time = time;
	scene_data.time_step = time_step;
	scene_data.main_cam_transform = p_cam_transform;

	RenderDataRD render_data;
	render_data.scene_data = &scene_data;
	render_data.instances = &p_instances;

	_setup_environment(&render_data, true, Vector2(1, 1), Color(), false, false);

	PassMode pass_mode = PASS_MODE_SHADOW;

	_fill_render_list(RENDER_LIST_SECONDARY, &render_data, pass_mode);
	render_list[RENDER_LIST_SECONDARY].sort_by_key();
	_fill_instance_data(RENDER_LIST_SECONDARY);

	RID rp_uniform_set = _setup_render_pass_uniform_set(RENDER_LIST_SECONDARY, nullptr, RID(), RendererRD::MaterialStorage::get_singleton()->samplers_rd_get_default());

	RENDER_TIMESTAMP("Render Collider Heightfield");

	{
		//regular forward for now
		RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].element_info.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), false, pass_mode, rp_uniform_set, scene_shader.default_specialization);
		_render_list_with_draw_list(&render_list_params, p_fb);
	}
	RD::get_singleton()->draw_command_end_label();
}

void RenderForwardMobile::base_uniforms_changed() {
	if (!render_base_uniform_set.is_null() && RD::get_singleton()->uniform_set_is_valid(render_base_uniform_set)) {
		RD::get_singleton()->free(render_base_uniform_set);
	}
	render_base_uniform_set = RID();
}

void RenderForwardMobile::_update_render_base_uniform_set() {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();

	// We must always recreate the uniform set every frame if we're using linear pools (since we requested it on creation).
	// This pays off as long as we often get inside the if() block (i.e. the settings end up changing often).
	if (RD::get_singleton()->uniform_sets_have_linear_pools() || render_base_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(render_base_uniform_set) || (lightmap_texture_array_version != light_storage->lightmap_array_get_version())) {
		if (render_base_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(render_base_uniform_set)) {
			RD::get_singleton()->free(render_base_uniform_set);
		}

		lightmap_texture_array_version = light_storage->lightmap_array_get_version();

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.binding = 2;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.append_id(scene_shader.shadow_sampler);
			u.immutable_sampler = true;
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.binding = 3;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(RendererRD::LightStorage::get_singleton()->get_omni_light_buffer());
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 4;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(RendererRD::LightStorage::get_singleton()->get_spot_light_buffer());
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.binding = 5;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(RendererRD::LightStorage::get_singleton()->get_reflection_probe_buffer());
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 6;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.append_id(RendererRD::LightStorage::get_singleton()->get_directional_light_buffer());
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 7;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(scene_state.lightmap_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 8;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(scene_state.lightmap_capture_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 9;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			RID decal_atlas = RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture();
			u.append_id(decal_atlas);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 10;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			RID decal_atlas = RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture_srgb();
			u.append_id(decal_atlas);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.binding = 11;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.append_id(RendererRD::TextureStorage::get_singleton()->get_decal_buffer());
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 12;
			u.append_id(RendererRD::MaterialStorage::get_singleton()->global_shader_uniforms_get_storage_buffer());
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.binding = 13;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.append_id(RendererRD::MaterialStorage::get_singleton()->sampler_rd_get_default(RS::CanvasItemTextureFilter::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS, RS::CanvasItemTextureRepeat::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED));
			uniforms.push_back(u);
		}

		render_base_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, scene_shader.default_shader_rd, SCENE_UNIFORM_SET, true);
	}
}

RID RenderForwardMobile::_render_buffers_get_normal_texture(Ref<RenderSceneBuffersRD> p_render_buffers) {
	return RID();
}

RID RenderForwardMobile::_render_buffers_get_velocity_texture(Ref<RenderSceneBuffersRD> p_render_buffers) {
	return RID();
}

void RenderForwardMobile::_update_instance_data_buffer(RenderListType p_render_list) {
	if (scene_state.instance_data[p_render_list].size() > 0) {
		if (scene_state.instance_buffer[p_render_list] == RID() || scene_state.instance_buffer_size[p_render_list] < scene_state.instance_data[p_render_list].size()) {
			if (scene_state.instance_buffer[p_render_list] != RID()) {
				RD::get_singleton()->free(scene_state.instance_buffer[p_render_list]);
			}
			uint32_t new_size = nearest_power_of_2_templated(MAX(uint64_t(INSTANCE_DATA_BUFFER_MIN_SIZE), scene_state.instance_data[p_render_list].size()));
			scene_state.instance_buffer[p_render_list] = RD::get_singleton()->storage_buffer_create(new_size * sizeof(SceneState::InstanceData));
			scene_state.instance_buffer_size[p_render_list] = new_size;
		}
		RD::get_singleton()->buffer_update(scene_state.instance_buffer[p_render_list], 0, sizeof(SceneState::InstanceData) * scene_state.instance_data[p_render_list].size(), scene_state.instance_data[p_render_list].ptr());
	}
}

void RenderForwardMobile::_fill_instance_data(RenderListType p_render_list, uint32_t p_offset, int32_t p_max_elements, bool p_update_buffer) {
	RenderList *rl = &render_list[p_render_list];
	uint32_t element_total = p_max_elements >= 0 ? uint32_t(p_max_elements) : rl->elements.size();

	scene_state.instance_data[p_render_list].resize(p_offset + element_total);
	rl->element_info.resize(p_offset + element_total);

	for (uint32_t i = 0; i < element_total; i++) {
		GeometryInstanceSurfaceDataCache *surface = rl->elements[i + p_offset];
		GeometryInstanceForwardMobile *inst = surface->owner;

		SceneState::InstanceData &instance_data = scene_state.instance_data[p_render_list][i + p_offset];

		if (inst->store_transform_cache) {
			RendererRD::MaterialStorage::store_transform(inst->transform, instance_data.transform);

#ifdef REAL_T_IS_DOUBLE
			// Split the origin into two components, the float approximation and the missing precision.
			// In the shader we will combine these back together to restore the lost precision.
			RendererRD::MaterialStorage::split_double(inst->transform.origin.x, &instance_data.transform[12], &instance_data.transform[3]);
			RendererRD::MaterialStorage::split_double(inst->transform.origin.y, &instance_data.transform[13], &instance_data.transform[7]);
			RendererRD::MaterialStorage::split_double(inst->transform.origin.z, &instance_data.transform[14], &instance_data.transform[11]);
#endif
		} else {
			RendererRD::MaterialStorage::store_transform(Transform3D(), instance_data.transform);
		}

		instance_data.flags = inst->flags_cache;
		instance_data.gi_offset = inst->gi_offset_cache;
		instance_data.layer_mask = inst->layer_mask;
		instance_data.instance_uniforms_ofs = uint32_t(inst->shader_uniforms_offset);
		instance_data.lightmap_uv_scale[0] = inst->lightmap_uv_scale.position.x;
		instance_data.lightmap_uv_scale[1] = inst->lightmap_uv_scale.position.y;
		instance_data.lightmap_uv_scale[2] = inst->lightmap_uv_scale.size.x;
		instance_data.lightmap_uv_scale[3] = inst->lightmap_uv_scale.size.y;

		AABB surface_aabb = AABB(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0, 1.0));
		uint64_t format = RendererRD::MeshStorage::get_singleton()->mesh_surface_get_format(surface->surface);
		Vector4 uv_scale = Vector4(0.0, 0.0, 0.0, 0.0);

		if (format & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
			surface_aabb = RendererRD::MeshStorage::get_singleton()->mesh_surface_get_aabb(surface->surface);
			uv_scale = RendererRD::MeshStorage::get_singleton()->mesh_surface_get_uv_scale(surface->surface);
		}

		fill_push_constant_instance_indices(&instance_data, inst);

		instance_data.compressed_aabb_position[0] = surface_aabb.position.x;
		instance_data.compressed_aabb_position[1] = surface_aabb.position.y;
		instance_data.compressed_aabb_position[2] = surface_aabb.position.z;

		instance_data.compressed_aabb_size[0] = surface_aabb.size.x;
		instance_data.compressed_aabb_size[1] = surface_aabb.size.y;
		instance_data.compressed_aabb_size[2] = surface_aabb.size.z;

		instance_data.uv_scale[0] = uv_scale.x;
		instance_data.uv_scale[1] = uv_scale.y;
		instance_data.uv_scale[2] = uv_scale.z;
		instance_data.uv_scale[3] = uv_scale.w;

		RenderElementInfo &element_info = rl->element_info[p_offset + i];

		element_info.lod_index = surface->lod_index;
		element_info.uses_lightmap = surface->sort.uses_lightmap;
	}

	if (p_update_buffer) {
		_update_instance_data_buffer(p_render_list);
	}
}

_FORCE_INLINE_ static uint32_t _indices_to_primitives(RS::PrimitiveType p_primitive, uint32_t p_indices) {
	static const uint32_t divisor[RS::PRIMITIVE_MAX] = { 1, 2, 1, 3, 1 };
	static const uint32_t subtractor[RS::PRIMITIVE_MAX] = { 0, 0, 1, 0, 1 };
	return (p_indices - subtractor[p_primitive]) / divisor[p_primitive];
}

void RenderForwardMobile::_fill_render_list(RenderListType p_render_list, const RenderDataRD *p_render_data, PassMode p_pass_mode, bool p_append) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	if (p_render_list == RENDER_LIST_OPAQUE) {
		scene_state.used_sss = false;
		scene_state.used_screen_texture = false;
		scene_state.used_normal_texture = false;
		scene_state.used_depth_texture = false;
		scene_state.used_lightmap = false;
	}
	uint32_t lightmap_captures_used = 0;

	Plane near_plane(-p_render_data->scene_data->cam_transform.basis.get_column(Vector3::AXIS_Z), p_render_data->scene_data->cam_transform.origin);
	near_plane.d += p_render_data->scene_data->cam_projection.get_z_near();
	float z_max = p_render_data->scene_data->cam_projection.get_z_far() - p_render_data->scene_data->cam_projection.get_z_near();

	RenderList *rl = &render_list[p_render_list];

	// Parse any updates on our geometry, updates surface caches and such
	_update_dirty_geometry_instances();

	if (!p_append) {
		rl->clear();
		if (p_render_list == RENDER_LIST_OPAQUE) {
			render_list[RENDER_LIST_ALPHA].clear(); //opaque fills alpha too
		}
	}

	//fill list

	for (int i = 0; i < (int)p_render_data->instances->size(); i++) {
		GeometryInstanceForwardMobile *inst = static_cast<GeometryInstanceForwardMobile *>((*p_render_data->instances)[i]);

		Vector3 center = inst->transform.origin;
		if (p_render_data->scene_data->cam_orthogonal) {
			if (inst->use_aabb_center) {
				center = inst->transformed_aabb.get_support(-near_plane.normal);
			}
			inst->depth = near_plane.distance_to(center) - inst->sorting_offset;
		} else {
			if (inst->use_aabb_center) {
				center = inst->transformed_aabb.position + (inst->transformed_aabb.size * 0.5);
			}
			inst->depth = p_render_data->scene_data->cam_transform.origin.distance_to(center) - inst->sorting_offset;
		}
		uint32_t depth_layer = CLAMP(int(inst->depth * 16 / z_max), 0, 15);

		uint32_t flags = inst->base_flags; //fill flags if appropriate

		if (inst->non_uniform_scale) {
			flags |= INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE;
		}

		bool uses_lightmap = false;
		// bool uses_gi = false;

		if (p_render_list == RENDER_LIST_OPAQUE) {
			if (inst->lightmap_instance.is_valid()) {
				int32_t lightmap_cull_index = -1;
				for (uint32_t j = 0; j < scene_state.lightmaps_used; j++) {
					if (scene_state.lightmap_ids[j] == inst->lightmap_instance) {
						lightmap_cull_index = j;
						break;
					}
				}
				if (lightmap_cull_index >= 0) {
					inst->gi_offset_cache = inst->lightmap_slice_index << 16;
					inst->gi_offset_cache |= lightmap_cull_index;
					flags |= INSTANCE_DATA_FLAG_USE_LIGHTMAP;
					if (scene_state.lightmap_has_sh[lightmap_cull_index]) {
						flags |= INSTANCE_DATA_FLAG_USE_SH_LIGHTMAP;
					}
					uses_lightmap = true;
				} else {
					inst->gi_offset_cache = 0xFFFFFFFF;
				}

			} else if (inst->lightmap_sh) {
				if (lightmap_captures_used < scene_state.max_lightmap_captures) {
					const Color *src_capture = inst->lightmap_sh->sh;
					LightmapCaptureData &lcd = scene_state.lightmap_captures[lightmap_captures_used];
					for (int j = 0; j < 9; j++) {
						lcd.sh[j * 4 + 0] = src_capture[j].r;
						lcd.sh[j * 4 + 1] = src_capture[j].g;
						lcd.sh[j * 4 + 2] = src_capture[j].b;
						lcd.sh[j * 4 + 3] = src_capture[j].a;
					}
					flags |= INSTANCE_DATA_FLAG_USE_LIGHTMAP_CAPTURE;
					inst->gi_offset_cache = lightmap_captures_used;
					lightmap_captures_used++;
					uses_lightmap = true;
				}
			}
		}
		inst->flags_cache = flags;

		GeometryInstanceSurfaceDataCache *surf = inst->surface_caches;

		float lod_distance = 0.0;

		if (p_render_data->scene_data->cam_orthogonal) {
			lod_distance = 1.0;
		} else {
			Vector3 aabb_min = inst->transformed_aabb.position;
			Vector3 aabb_max = inst->transformed_aabb.position + inst->transformed_aabb.size;
			Vector3 camera_position = p_render_data->scene_data->main_cam_transform.origin;
			Vector3 surface_distance = Vector3(0.0, 0.0, 0.0).max(aabb_min - camera_position).max(camera_position - aabb_max);

			lod_distance = surface_distance.length();
		}

		while (surf) {
			surf->sort.uses_lightmap = 0;

			// LOD

			if (p_render_data->scene_data->screen_mesh_lod_threshold > 0.0 && mesh_storage->mesh_surface_has_lod(surf->surface)) {
				uint32_t indices = 0;
				surf->lod_index = mesh_storage->mesh_surface_get_lod(surf->surface, inst->lod_model_scale * inst->lod_bias, lod_distance * p_render_data->scene_data->lod_distance_multiplier, p_render_data->scene_data->screen_mesh_lod_threshold, indices);
				if (p_render_data->render_info) {
					indices = _indices_to_primitives(surf->primitive, indices);
					if (p_render_list == RENDER_LIST_OPAQUE) { //opaque
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += indices;
					} else if (p_render_list == RENDER_LIST_SECONDARY) { //shadow
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += indices;
					}
				}
			} else {
				surf->lod_index = 0;
				if (p_render_data->render_info) {
					uint32_t to_draw = mesh_storage->mesh_surface_get_vertices_drawn_count(surf->surface);
					to_draw = _indices_to_primitives(surf->primitive, to_draw);
					to_draw *= inst->instance_count;
					if (p_render_list == RENDER_LIST_OPAQUE) { //opaque
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += to_draw;
					} else if (p_render_list == RENDER_LIST_SECONDARY) { //shadow
						p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_SHADOW][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += to_draw;
					}
				}
			}

			// ADD Element
			if (p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) {
#ifdef DEBUG_ENABLED
				bool force_alpha = unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW);
#else
				bool force_alpha = false;
#endif
				if (!force_alpha && (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_PASS_OPAQUE)) {
					rl->add_element(surf);
				}
				if (force_alpha || (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_PASS_ALPHA)) {
					render_list[RENDER_LIST_ALPHA].add_element(surf);
				}

				if (uses_lightmap) {
					surf->sort.uses_lightmap = 1; // This needs to become our lightmap index but we'll do that in a separate PR.
					scene_state.used_lightmap = true;
				}

				if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_SUBSURFACE_SCATTERING) {
					scene_state.used_sss = true;
				}
				if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_SCREEN_TEXTURE) {
					scene_state.used_screen_texture = true;
				}
				if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_NORMAL_TEXTURE) {
					scene_state.used_normal_texture = true;
				}
				if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_DEPTH_TEXTURE) {
					scene_state.used_depth_texture = true;
				}

			} else if (p_pass_mode == PASS_MODE_SHADOW || p_pass_mode == PASS_MODE_SHADOW_DP) {
				if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_PASS_SHADOW) {
					rl->add_element(surf);
				}
			} else {
				if (surf->flags & (GeometryInstanceSurfaceDataCache::FLAG_PASS_DEPTH | GeometryInstanceSurfaceDataCache::FLAG_PASS_OPAQUE)) {
					rl->add_element(surf);
				}
			}

			surf->sort.depth_layer = depth_layer;

			surf = surf->next;
		}
	}

	if (p_render_list == RENDER_LIST_OPAQUE && lightmap_captures_used) {
		RD::get_singleton()->buffer_update(scene_state.lightmap_capture_buffer, 0, sizeof(LightmapCaptureData) * lightmap_captures_used, scene_state.lightmap_captures);
	}
}

void RenderForwardMobile::_setup_environment(const RenderDataRD *p_render_data, bool p_no_fog, const Size2i &p_screen_size, const Color &p_default_bg_color, bool p_opaque_render_buffers, bool p_pancake_shadows, int p_index) {
	RID env = is_environment(p_render_data->environment) ? p_render_data->environment : RID();
	RID reflection_probe_instance = p_render_data->reflection_probe.is_valid() ? RendererRD::LightStorage::get_singleton()->reflection_probe_instance_get_probe(p_render_data->reflection_probe) : RID();

	// May do this earlier in RenderSceneRenderRD::render_scene
	if (p_index >= (int)scene_state.uniform_buffers.size()) {
		uint32_t from = scene_state.uniform_buffers.size();
		scene_state.uniform_buffers.resize(p_index + 1);
		for (uint32_t i = from; i < scene_state.uniform_buffers.size(); i++) {
			scene_state.uniform_buffers[i] = p_render_data->scene_data->create_uniform_buffer();
		}
	}

	p_render_data->scene_data->update_ubo(scene_state.uniform_buffers[p_index], get_debug_draw_mode(), env, reflection_probe_instance, p_render_data->camera_attributes, p_pancake_shadows, p_screen_size, p_default_bg_color, _render_buffers_get_luminance_multiplier(), p_opaque_render_buffers, false);
}

/// RENDERING ///

void RenderForwardMobile::_render_list(RenderingDevice::DrawListID p_draw_list, RenderingDevice::FramebufferFormatID p_framebuffer_Format, RenderListParameters *p_params, uint32_t p_from_element, uint32_t p_to_element) {
	//use template for faster performance (pass mode comparisons are inlined)

	switch (p_params->pass_mode) {
		case PASS_MODE_COLOR: {
			_render_list_template<PASS_MODE_COLOR>(p_draw_list, p_framebuffer_Format, p_params, p_from_element, p_to_element);
		} break;
		case PASS_MODE_COLOR_TRANSPARENT: {
			_render_list_template<PASS_MODE_COLOR_TRANSPARENT>(p_draw_list, p_framebuffer_Format, p_params, p_from_element, p_to_element);
		} break;
		case PASS_MODE_SHADOW: {
			_render_list_template<PASS_MODE_SHADOW>(p_draw_list, p_framebuffer_Format, p_params, p_from_element, p_to_element);
		} break;
		case PASS_MODE_SHADOW_DP: {
			_render_list_template<PASS_MODE_SHADOW_DP>(p_draw_list, p_framebuffer_Format, p_params, p_from_element, p_to_element);
		} break;
		case PASS_MODE_DEPTH_MATERIAL: {
			_render_list_template<PASS_MODE_DEPTH_MATERIAL>(p_draw_list, p_framebuffer_Format, p_params, p_from_element, p_to_element);
		} break;
	}
}

void RenderForwardMobile::_render_list_with_draw_list(RenderListParameters *p_params, RID p_framebuffer, BitField<RD::DrawFlags> p_draw_flags, const Vector<Color> &p_clear_color_values, float p_clear_depth_value, uint32_t p_clear_stencil_value, const Rect2 &p_region) {
	RD::FramebufferFormatID fb_format = RD::get_singleton()->framebuffer_get_format(p_framebuffer);
	p_params->framebuffer_format = fb_format;

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(p_framebuffer, p_draw_flags, p_clear_color_values, p_clear_depth_value, p_clear_stencil_value, p_region);
	_render_list(draw_list, fb_format, p_params, 0, p_params->element_count);
	RD::get_singleton()->draw_list_end();
}

template <RenderForwardMobile::PassMode p_pass_mode>
void RenderForwardMobile::_render_list_template(RenderingDevice::DrawListID p_draw_list, RenderingDevice::FramebufferFormatID p_framebuffer_Format, RenderListParameters *p_params, uint32_t p_from_element, uint32_t p_to_element) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	RD::DrawListID draw_list = p_draw_list;
	RD::FramebufferFormatID framebuffer_format = p_framebuffer_Format;

	//global scope bindings
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, render_base_uniform_set, SCENE_UNIFORM_SET);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, p_params->render_pass_uniform_set, RENDER_PASS_UNIFORM_SET);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, scene_shader.default_vec4_xform_uniform_set, TRANSFORMS_UNIFORM_SET);

	RID material_uniform_set;
	RID prev_material_uniform_set;

	RID prev_vertex_array_rd;
	RID prev_index_array_rd;
	RID prev_xforms_uniform_set;
	bool should_request_redraw = false;

	void *mesh_surface;
	SceneShaderForwardMobile::ShaderData *shader = nullptr;
	SceneShaderForwardMobile::ShaderData *prev_shader = nullptr;
	SceneShaderForwardMobile::ShaderData::PipelineKey pipeline_key;
	uint32_t pipeline_hash = 0;
	uint32_t prev_pipeline_hash = 0;

	bool shadow_pass = (p_params->pass_mode == PASS_MODE_SHADOW) || (p_params->pass_mode == PASS_MODE_SHADOW_DP);

	for (uint32_t i = p_from_element; i < p_to_element; i++) {
		const GeometryInstanceSurfaceDataCache *surf = p_params->elements[i];
		const RenderElementInfo &element_info = p_params->element_info[i];
		const GeometryInstanceForwardMobile *inst = surf->owner;

		if (inst->instance_count == 0) {
			continue;
		}

		SceneShaderForwardMobile::ShaderSpecialization pipeline_specialization = p_params->base_specialization;
		pipeline_specialization.multimesh = bool(inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH);
		pipeline_specialization.multimesh_format_2d = bool(inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D);
		pipeline_specialization.multimesh_has_color = bool(inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR);
		pipeline_specialization.multimesh_has_custom_data = bool(inst->flags_cache & INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA);

		SceneState::PushConstant push_constant;
		push_constant.base_index = i + p_params->element_offset;

		if constexpr (p_pass_mode == PASS_MODE_DEPTH_MATERIAL) {
			push_constant.uv_offset[0] = p_params->uv_offset.x;
			push_constant.uv_offset[1] = p_params->uv_offset.y;
		} else {
			push_constant.uv_offset[0] = 0.0;
			push_constant.uv_offset[1] = 0.0;
		}

		if (shadow_pass) {
			material_uniform_set = surf->material_uniform_set_shadow;
			shader = surf->shader_shadow;
			mesh_surface = surf->surface_shadow;

		} else {
			pipeline_specialization.use_light_projector = inst->use_projector;
			pipeline_specialization.use_light_soft_shadows = inst->use_soft_shadow;
			pipeline_specialization.omni_lights = inst->omni_light_count;
			pipeline_specialization.spot_lights = inst->spot_light_count;
			pipeline_specialization.reflection_probes = inst->reflection_probe_count;
			pipeline_specialization.decals = inst->decals_count;

#ifdef DEBUG_ENABLED
			if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_LIGHTING)) {
				material_uniform_set = scene_shader.default_material_uniform_set;
				shader = scene_shader.default_material_shader_ptr;
			} else if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW)) {
				material_uniform_set = scene_shader.overdraw_material_uniform_set;
				shader = scene_shader.overdraw_material_shader_ptr;
			} else if (unlikely(get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_PSSM_SPLITS)) {
				material_uniform_set = scene_shader.debug_shadow_splits_material_uniform_set;
				shader = scene_shader.debug_shadow_splits_material_shader_ptr;
			} else {
#endif
				material_uniform_set = surf->material_uniform_set;
				shader = surf->shader;
				surf->material->set_as_used();
#ifdef DEBUG_ENABLED
			}
#endif
			mesh_surface = surf->surface;
		}

		if (!mesh_surface) {
			continue;
		}

		//request a redraw if one of the shaders uses TIME
		if (shader->uses_time) {
			should_request_redraw = true;
		}

		//find cull variant
		SceneShaderForwardMobile::ShaderData::CullVariant cull_variant;

		if (p_params->pass_mode == PASS_MODE_DEPTH_MATERIAL || ((p_params->pass_mode == PASS_MODE_SHADOW || p_params->pass_mode == PASS_MODE_SHADOW_DP) && surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_DOUBLE_SIDED_SHADOWS)) {
			cull_variant = SceneShaderForwardMobile::ShaderData::CULL_VARIANT_DOUBLE_SIDED;
		} else {
			bool mirror = surf->owner->mirror;
			if (p_params->reverse_cull) {
				mirror = !mirror;
			}
			cull_variant = mirror ? SceneShaderForwardMobile::ShaderData::CULL_VARIANT_REVERSED : SceneShaderForwardMobile::ShaderData::CULL_VARIANT_NORMAL;
		}

		pipeline_key.primitive_type = surf->primitive;
		RID xforms_uniform_set = surf->owner->transforms_uniform_set;

		switch (p_params->pass_mode) {
			case PASS_MODE_COLOR:
			case PASS_MODE_COLOR_TRANSPARENT: {
				if (element_info.uses_lightmap) {
					pipeline_key.version = p_params->view_count > 1 ? SceneShaderForwardMobile::SHADER_VERSION_LIGHTMAP_COLOR_PASS_MULTIVIEW : SceneShaderForwardMobile::SHADER_VERSION_LIGHTMAP_COLOR_PASS;
				} else {
					pipeline_key.version = p_params->view_count > 1 ? SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS_MULTIVIEW : SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS;
				}
			} break;
			case PASS_MODE_SHADOW: {
				pipeline_key.version = p_params->view_count > 1 ? SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS_MULTIVIEW : SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS;
			} break;
			case PASS_MODE_SHADOW_DP: {
				ERR_FAIL_COND_MSG(p_params->view_count > 1, "Multiview not supported for shadow DP pass");
				pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS_DP;
			} break;
			case PASS_MODE_DEPTH_MATERIAL: {
				ERR_FAIL_COND_MSG(p_params->view_count > 1, "Multiview not supported for material pass");
				pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_DEPTH_PASS_WITH_MATERIAL;
			} break;
		}

		pipeline_key.framebuffer_format_id = framebuffer_format;
		pipeline_key.wireframe = p_params->force_wireframe;
		pipeline_key.render_pass = p_params->subpass;
		pipeline_key.ubershader = 0;

		const RD::PolygonCullMode cull_mode = shader->get_cull_mode_from_cull_variant(cull_variant);
		RD::VertexFormatID vertex_format = -1;
		RID pipeline_rd;
		RID vertex_array_rd;
		RID index_array_rd;
		const uint32_t ubershader_iterations = 2;
		bool pipeline_valid = false;
		while (pipeline_key.ubershader < ubershader_iterations) {
			// Skeleton and blend shape.
			uint64_t input_mask = shader->get_vertex_input_mask(pipeline_key.version, pipeline_key.ubershader);
			if (surf->owner->mesh_instance.is_valid()) {
				mesh_storage->mesh_instance_surface_get_vertex_arrays_and_format(surf->owner->mesh_instance, surf->surface_index, input_mask, false, vertex_array_rd, vertex_format);
			} else {
				mesh_storage->mesh_surface_get_vertex_arrays_and_format(mesh_surface, input_mask, false, vertex_array_rd, vertex_format);
			}

			index_array_rd = mesh_storage->mesh_surface_get_index_array(mesh_surface, element_info.lod_index);
			pipeline_key.vertex_format_id = vertex_format;

			if (pipeline_key.ubershader) {
				pipeline_key.shader_specialization = {};
				pipeline_key.cull_mode = RD::POLYGON_CULL_DISABLED;
			} else {
				pipeline_key.shader_specialization = pipeline_specialization;
				pipeline_key.cull_mode = cull_mode;
			}

			pipeline_hash = pipeline_key.hash();

			if (shader != prev_shader || pipeline_hash != prev_pipeline_hash) {
				RS::PipelineSource pipeline_source = pipeline_key.ubershader ? RS::PIPELINE_SOURCE_DRAW : RS::PIPELINE_SOURCE_SPECIALIZATION;
				pipeline_rd = shader->pipeline_hash_map.get_pipeline(pipeline_key, pipeline_hash, pipeline_key.ubershader, pipeline_source);

				if (pipeline_rd.is_valid()) {
					pipeline_valid = true;
					prev_shader = shader;
					prev_pipeline_hash = pipeline_hash;
					break;
				} else {
					pipeline_key.ubershader++;
				}
			} else {
				// The same pipeline is bound already.
				pipeline_valid = true;
				break;
			}
		}

		if (pipeline_valid) {
			index_array_rd = mesh_storage->mesh_surface_get_index_array(mesh_surface, element_info.lod_index);

			if (prev_vertex_array_rd != vertex_array_rd) {
				RD::get_singleton()->draw_list_bind_vertex_array(draw_list, vertex_array_rd);
				prev_vertex_array_rd = vertex_array_rd;
			}

			if (prev_index_array_rd != index_array_rd) {
				if (index_array_rd.is_valid()) {
					RD::get_singleton()->draw_list_bind_index_array(draw_list, index_array_rd);
				}
				prev_index_array_rd = index_array_rd;
			}

			if (!pipeline_rd.is_null()) {
				RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, pipeline_rd);
			}

			if (xforms_uniform_set.is_valid() && prev_xforms_uniform_set != xforms_uniform_set) {
				RD::get_singleton()->draw_list_bind_uniform_set(draw_list, xforms_uniform_set, TRANSFORMS_UNIFORM_SET);
				prev_xforms_uniform_set = xforms_uniform_set;
			}

			if (material_uniform_set != prev_material_uniform_set) {
				// Update uniform set.
				if (material_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(material_uniform_set)) { // Material may not have a uniform set.
					RD::get_singleton()->draw_list_bind_uniform_set(draw_list, material_uniform_set, MATERIAL_UNIFORM_SET);
				}

				prev_material_uniform_set = material_uniform_set;
			}

			size_t push_constant_size = 0;
			if (pipeline_key.ubershader) {
				push_constant_size = sizeof(SceneState::PushConstant);
				push_constant.ubershader.specialization = pipeline_specialization;
				push_constant.ubershader.constants = {};
				push_constant.ubershader.constants.cull_mode = cull_mode;
			} else {
				push_constant_size = sizeof(SceneState::PushConstant) - sizeof(SceneState::PushConstantUbershader);
			}

			RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, push_constant_size);

			uint32_t instance_count = surf->owner->instance_count > 1 ? surf->owner->instance_count : 1;
			if (surf->flags & GeometryInstanceSurfaceDataCache::FLAG_USES_PARTICLE_TRAILS) {
				instance_count /= surf->owner->trail_steps;
			}

			RD::get_singleton()->draw_list_draw(draw_list, index_array_rd.is_valid(), instance_count);
		}
	}

	// Make the actual redraw request
	if (should_request_redraw) {
		RenderingServerDefault::redraw_request();
	}
}

/* Geometry instance */

RenderGeometryInstance *RenderForwardMobile::geometry_instance_create(RID p_base) {
	RS::InstanceType type = RSG::utilities->get_base_type(p_base);
	ERR_FAIL_COND_V(!((1 << type) & RS::INSTANCE_GEOMETRY_MASK), nullptr);

	GeometryInstanceForwardMobile *ginstance = geometry_instance_alloc.alloc();
	ginstance->data = memnew(GeometryInstanceForwardMobile::Data);

	ginstance->data->base = p_base;
	ginstance->data->base_type = type;
	ginstance->data->dependency_tracker.userdata = ginstance;
	ginstance->data->dependency_tracker.changed_callback = _geometry_instance_dependency_changed;
	ginstance->data->dependency_tracker.deleted_callback = _geometry_instance_dependency_deleted;

	ginstance->_mark_dirty();

	return ginstance;
}

void RenderForwardMobile::GeometryInstanceForwardMobile::set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) {
	lightmap_instance = p_lightmap_instance;
	lightmap_uv_scale = p_lightmap_uv_scale;
	lightmap_slice_index = p_lightmap_slice_index;

	_mark_dirty();
}

void RenderForwardMobile::GeometryInstanceForwardMobile::set_lightmap_capture(const Color *p_sh9) {
	if (p_sh9) {
		if (lightmap_sh == nullptr) {
			lightmap_sh = RenderForwardMobile::get_singleton()->geometry_instance_lightmap_sh.alloc();
		}

		memcpy(lightmap_sh->sh, p_sh9, sizeof(Color) * 9);
	} else {
		if (lightmap_sh != nullptr) {
			RenderForwardMobile::get_singleton()->geometry_instance_lightmap_sh.free(lightmap_sh);
			lightmap_sh = nullptr;
		}
	}
	_mark_dirty();
}

void RenderForwardMobile::geometry_instance_free(RenderGeometryInstance *p_geometry_instance) {
	GeometryInstanceForwardMobile *ginstance = static_cast<GeometryInstanceForwardMobile *>(p_geometry_instance);
	ERR_FAIL_NULL(ginstance);
	if (ginstance->lightmap_sh != nullptr) {
		geometry_instance_lightmap_sh.free(ginstance->lightmap_sh);
	}
	GeometryInstanceSurfaceDataCache *surf = ginstance->surface_caches;
	while (surf) {
		GeometryInstanceSurfaceDataCache *next = surf->next;
		geometry_instance_surface_alloc.free(surf);
		surf = next;
	}
	memdelete(ginstance->data);
	geometry_instance_alloc.free(ginstance);
}

uint32_t RenderForwardMobile::geometry_instance_get_pair_mask() {
	return ((1 << RS::INSTANCE_LIGHT) + (1 << RS::INSTANCE_REFLECTION_PROBE) + (1 << RS::INSTANCE_DECAL));
}

void RenderForwardMobile::GeometryInstanceForwardMobile::pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) {
	omni_light_count = 0;
	spot_light_count = 0;

	for (uint32_t i = 0; i < p_light_instance_count; i++) {
		RS::LightType type = RendererRD::LightStorage::get_singleton()->light_instance_get_type(p_light_instances[i]);
		switch (type) {
			case RS::LIGHT_OMNI: {
				if (omni_light_count < (uint32_t)MAX_RDL_CULL) {
					omni_lights[omni_light_count] = RendererRD::LightStorage::get_singleton()->light_instance_get_forward_id(p_light_instances[i]);
					omni_light_count++;
				}
			} break;
			case RS::LIGHT_SPOT: {
				if (spot_light_count < (uint32_t)MAX_RDL_CULL) {
					spot_lights[spot_light_count] = RendererRD::LightStorage::get_singleton()->light_instance_get_forward_id(p_light_instances[i]);
					spot_light_count++;
				}
			} break;
			default:
				break;
		}
	}
}

void RenderForwardMobile::GeometryInstanceForwardMobile::pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) {
	reflection_probe_count = p_reflection_probe_instance_count < (uint32_t)MAX_RDL_CULL ? p_reflection_probe_instance_count : (uint32_t)MAX_RDL_CULL;
	for (uint32_t i = 0; i < reflection_probe_count; i++) {
		reflection_probes[i] = RendererRD::LightStorage::get_singleton()->reflection_probe_instance_get_forward_id(p_reflection_probe_instances[i]);
	}
}

void RenderForwardMobile::GeometryInstanceForwardMobile::pair_decal_instances(const RID *p_decal_instances, uint32_t p_decal_instance_count) {
	decals_count = p_decal_instance_count < (uint32_t)MAX_RDL_CULL ? p_decal_instance_count : (uint32_t)MAX_RDL_CULL;
	for (uint32_t i = 0; i < decals_count; i++) {
		decals[i] = RendererRD::TextureStorage::get_singleton()->decal_instance_get_forward_id(p_decal_instances[i]);
	}
}

void RenderForwardMobile::GeometryInstanceForwardMobile::set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) {
	use_projector = p_projector;
	use_soft_shadow = p_softshadow;
}

void RenderForwardMobile::GeometryInstanceForwardMobile::_mark_dirty() {
	if (dirty_list_element.in_list()) {
		return;
	}

	//clear surface caches
	GeometryInstanceSurfaceDataCache *surf = surface_caches;

	while (surf) {
		GeometryInstanceSurfaceDataCache *next = surf->next;
		RenderForwardMobile::get_singleton()->geometry_instance_surface_alloc.free(surf);
		surf = next;
	}

	surface_caches = nullptr;

	RenderForwardMobile::get_singleton()->geometry_instance_dirty_list.add(&dirty_list_element);
}

void RenderForwardMobile::_update_global_pipeline_data_requirements_from_project() {
	const int msaa_2d_mode = GLOBAL_GET("rendering/anti_aliasing/quality/msaa_2d");
	const int msaa_3d_mode = GLOBAL_GET("rendering/anti_aliasing/quality/msaa_3d");
	const bool directional_shadow_16_bits = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/16_bits");
	const bool positional_shadow_16_bits = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_16_bits");
	global_pipeline_data_required.use_16_bit_shadows = directional_shadow_16_bits || positional_shadow_16_bits;
	global_pipeline_data_required.use_32_bit_shadows = !directional_shadow_16_bits || !positional_shadow_16_bits;
	global_pipeline_data_required.target_samples = RenderSceneBuffersRD::msaa_to_samples(RS::ViewportMSAA(msaa_2d_mode));
	global_pipeline_data_required.texture_samples = RenderSceneBuffersRD::msaa_to_samples(RS::ViewportMSAA(msaa_3d_mode));
}

void RenderForwardMobile::_update_global_pipeline_data_requirements_from_light_storage() {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	global_pipeline_data_required.use_shadow_cubemaps = light_storage->get_shadow_cubemaps_used();
	global_pipeline_data_required.use_shadow_dual_paraboloid = light_storage->get_shadow_dual_paraboloid_used();
}

void RenderForwardMobile::_geometry_instance_add_surface_with_material(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, SceneShaderForwardMobile::MaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	uint32_t flags = 0;

	if (p_material->shader_data->uses_sss) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_SUBSURFACE_SCATTERING;
	}

	if (p_material->shader_data->uses_screen_texture) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_SCREEN_TEXTURE;
	}

	if (p_material->shader_data->uses_depth_texture) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_DEPTH_TEXTURE;
	}

	if (p_material->shader_data->uses_normal_texture) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_NORMAL_TEXTURE;
	}

	if (ginstance->data->cast_double_sided_shadows) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_DOUBLE_SIDED_SHADOWS;
	}

	if (p_material->shader_data->uses_alpha_pass()) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_ALPHA;
		if (p_material->shader_data->uses_depth_in_alpha_pass()) {
			flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_DEPTH;
			flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_SHADOW;
		}
	} else {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_OPAQUE;
		flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_DEPTH;
		flags |= GeometryInstanceSurfaceDataCache::FLAG_PASS_SHADOW;
	}

	if (p_material->shader_data->uses_particle_trails) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_PARTICLE_TRAILS;
	}

	SceneShaderForwardMobile::MaterialData *material_shadow = nullptr;
	void *surface_shadow = nullptr;
	if (p_material->shader_data->uses_shared_shadow_material()) {
		flags |= GeometryInstanceSurfaceDataCache::FLAG_USES_SHARED_SHADOW_MATERIAL;
		material_shadow = static_cast<SceneShaderForwardMobile::MaterialData *>(RendererRD::MaterialStorage::get_singleton()->material_get_data(scene_shader.default_material, RendererRD::MaterialStorage::SHADER_TYPE_3D));

		RID shadow_mesh = mesh_storage->mesh_get_shadow_mesh(p_mesh);

		if (shadow_mesh.is_valid()) {
			surface_shadow = mesh_storage->mesh_get_surface(shadow_mesh, p_surface);
		}

	} else {
		material_shadow = p_material;
	}

	GeometryInstanceSurfaceDataCache *sdcache = geometry_instance_surface_alloc.alloc();

	sdcache->flags = flags;

	sdcache->shader = p_material->shader_data;
	sdcache->material = p_material;
	sdcache->material_uniform_set = p_material->uniform_set;
	sdcache->surface = mesh_storage->mesh_get_surface(p_mesh, p_surface);
	sdcache->primitive = mesh_storage->mesh_surface_get_primitive(sdcache->surface);
	sdcache->surface_index = p_surface;

	if (ginstance->data->dirty_dependencies) {
		RSG::utilities->base_update_dependency(p_mesh, &ginstance->data->dependency_tracker);
	}

	//shadow
	sdcache->shader_shadow = material_shadow->shader_data;
	sdcache->material_uniform_set_shadow = material_shadow->uniform_set;

	sdcache->surface_shadow = surface_shadow ? surface_shadow : sdcache->surface;

	sdcache->owner = ginstance;

	sdcache->next = ginstance->surface_caches;
	ginstance->surface_caches = sdcache;

	//sortkey

	sdcache->sort.sort_key1 = 0;
	sdcache->sort.sort_key2 = 0;

	sdcache->sort.surface_index = p_surface;
	sdcache->sort.material_id_low = p_material_id & 0x0000FFFF;
	sdcache->sort.material_id_hi = p_material_id >> 16;
	sdcache->sort.shader_id = p_shader_id;
	sdcache->sort.geometry_id = p_mesh.get_local_index();
	// sdcache->sort.uses_forward_gi = ginstance->can_sdfgi;
	sdcache->sort.priority = p_material->priority;

	uint64_t format = RendererRD::MeshStorage::get_singleton()->mesh_surface_get_format(sdcache->surface);
	if (p_material->shader_data->uses_tangent && !(format & RS::ARRAY_FORMAT_TANGENT)) {
		String shader_path = p_material->shader_data->path.is_empty() ? "" : "(" + p_material->shader_data->path + ")";
		String mesh_path = mesh_storage->mesh_get_path(p_mesh).is_empty() ? "" : "(" + mesh_storage->mesh_get_path(p_mesh) + ")";
		WARN_PRINT_ED(vformat("Attempting to use a shader %s that requires tangents with a mesh %s that doesn't contain tangents. Ensure that meshes are imported with the 'ensure_tangents' option. If creating your own meshes, add an `ARRAY_TANGENT` array (when using ArrayMesh) or call `generate_tangents()` (when using SurfaceTool).", shader_path, mesh_path));
	}

#if PRELOAD_PIPELINES_ON_SURFACE_CACHE_CONSTRUCTION
	if (!sdcache->compilation_dirty_element.in_list()) {
		geometry_surface_compilation_dirty_list.add(&sdcache->compilation_dirty_element);
	}

	if (!sdcache->compilation_all_element.in_list()) {
		geometry_surface_compilation_all_list.add(&sdcache->compilation_all_element);
	}
#endif
}

void RenderForwardMobile::_geometry_instance_add_surface_with_material_chain(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, SceneShaderForwardMobile::MaterialData *p_material, RID p_mat_src, RID p_mesh) {
	SceneShaderForwardMobile::MaterialData *material = p_material;
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	_geometry_instance_add_surface_with_material(ginstance, p_surface, material, p_mat_src.get_local_index(), material_storage->material_get_shader_id(p_mat_src), p_mesh);

	while (material->next_pass.is_valid()) {
		RID next_pass = material->next_pass;
		material = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(next_pass, RendererRD::MaterialStorage::SHADER_TYPE_3D));
		if (!material || !material->shader_data->is_valid()) {
			break;
		}
		if (ginstance->data->dirty_dependencies) {
			material_storage->material_update_dependency(next_pass, &ginstance->data->dependency_tracker);
		}
		_geometry_instance_add_surface_with_material(ginstance, p_surface, material, next_pass.get_local_index(), material_storage->material_get_shader_id(next_pass), p_mesh);
	}
}

void RenderForwardMobile::_geometry_instance_add_surface(GeometryInstanceForwardMobile *ginstance, uint32_t p_surface, RID p_material, RID p_mesh) {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RID m_src;

	m_src = ginstance->data->material_override.is_valid() ? ginstance->data->material_override : p_material;

	SceneShaderForwardMobile::MaterialData *material = nullptr;

	if (m_src.is_valid()) {
		material = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(m_src, RendererRD::MaterialStorage::SHADER_TYPE_3D));
		if (!material || !material->shader_data->is_valid()) {
			material = nullptr;
		}
	}

	if (material) {
		if (ginstance->data->dirty_dependencies) {
			material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
		}
	} else {
		material = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(scene_shader.default_material, RendererRD::MaterialStorage::SHADER_TYPE_3D));
		m_src = scene_shader.default_material;
	}

	ERR_FAIL_NULL(material);

	_geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material, m_src, p_mesh);

	if (ginstance->data->material_overlay.is_valid()) {
		m_src = ginstance->data->material_overlay;

		material = static_cast<SceneShaderForwardMobile::MaterialData *>(material_storage->material_get_data(m_src, RendererRD::MaterialStorage::SHADER_TYPE_3D));
		if (material && material->shader_data->is_valid()) {
			if (ginstance->data->dirty_dependencies) {
				material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
			}

			_geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material, m_src, p_mesh);
		}
	}
}

void RenderForwardMobile::_geometry_instance_update(RenderGeometryInstance *p_geometry_instance) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();
	GeometryInstanceForwardMobile *ginstance = static_cast<GeometryInstanceForwardMobile *>(p_geometry_instance);

	if (ginstance->data->dirty_dependencies) {
		ginstance->data->dependency_tracker.update_begin();
	}

	//add geometry for drawing
	switch (ginstance->data->base_type) {
		case RS::INSTANCE_MESH: {
			const RID *materials = nullptr;
			uint32_t surface_count;
			RID mesh = ginstance->data->base;

			materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
			if (materials) {
				//if no materials, no surfaces.
				const RID *inst_materials = ginstance->data->surface_materials.ptr();
				uint32_t surf_mat_count = ginstance->data->surface_materials.size();

				for (uint32_t j = 0; j < surface_count; j++) {
					RID material = (j < surf_mat_count && inst_materials[j].is_valid()) ? inst_materials[j] : materials[j];
					_geometry_instance_add_surface(ginstance, j, material, mesh);
				}
			}

			ginstance->instance_count = 1;

		} break;

		case RS::INSTANCE_MULTIMESH: {
			RID mesh = mesh_storage->multimesh_get_mesh(ginstance->data->base);
			if (mesh.is_valid()) {
				const RID *materials = nullptr;
				uint32_t surface_count;

				materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
				if (materials) {
					for (uint32_t j = 0; j < surface_count; j++) {
						_geometry_instance_add_surface(ginstance, j, materials[j], mesh);
					}
				}

				ginstance->instance_count = mesh_storage->multimesh_get_instances_to_draw(ginstance->data->base);
			}

		} break;
#if 0
		case RS::INSTANCE_IMMEDIATE: {
			RasterizerStorageGLES3::Immediate *immediate = storage->immediate_owner.get_or_null(inst->base);
			ERR_CONTINUE(!immediate);

			_add_geometry(immediate, inst, nullptr, -1, p_depth_pass, p_shadow_pass);

		} break;
#endif
		case RS::INSTANCE_PARTICLES: {
			int draw_passes = particles_storage->particles_get_draw_passes(ginstance->data->base);

			for (int j = 0; j < draw_passes; j++) {
				RID mesh = particles_storage->particles_get_draw_pass_mesh(ginstance->data->base, j);
				if (!mesh.is_valid()) {
					continue;
				}

				const RID *materials = nullptr;
				uint32_t surface_count;

				materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
				if (materials) {
					for (uint32_t k = 0; k < surface_count; k++) {
						_geometry_instance_add_surface(ginstance, k, materials[k], mesh);
					}
				}
			}

			ginstance->instance_count = particles_storage->particles_get_amount(ginstance->data->base, ginstance->trail_steps);

		} break;

		default: {
		}
	}

	//Fill push constant

	bool store_transform = true;
	ginstance->base_flags = 0;

	if (ginstance->data->base_type == RS::INSTANCE_MULTIMESH) {
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH;
		if (mesh_storage->multimesh_get_transform_format(ginstance->data->base) == RS::MULTIMESH_TRANSFORM_2D) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D;
		}
		if (mesh_storage->multimesh_uses_colors(ginstance->data->base)) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR;
		}
		if (mesh_storage->multimesh_uses_custom_data(ginstance->data->base)) {
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA;
		}

		ginstance->transforms_uniform_set = mesh_storage->multimesh_get_3d_uniform_set(ginstance->data->base, scene_shader.default_shader_rd, TRANSFORMS_UNIFORM_SET);

	} else if (ginstance->data->base_type == RS::INSTANCE_PARTICLES) {
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH;
		if (false) { // 2D particles
			ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D;
		}

		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR;
		ginstance->base_flags |= INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA;

		//for particles, stride is the trail size
		ginstance->base_flags |= (ginstance->trail_steps << INSTANCE_DATA_FLAGS_PARTICLE_TRAIL_SHIFT);

		if (!particles_storage->particles_is_using_local_coords(ginstance->data->base)) {
			store_transform = false;
		}
		ginstance->transforms_uniform_set = particles_storage->particles_get_instance_buffer_uniform_set(ginstance->data->base, scene_shader.default_shader_rd, TRANSFORMS_UNIFORM_SET);

		if (particles_storage->particles_get_frame_counter(ginstance->data->base) == 0) {
			// Particles haven't been cleared or updated, update once now to ensure they are ready to render.
			particles_storage->update_particles();
		}

		if (ginstance->data->dirty_dependencies) {
			particles_storage->particles_update_dependency(ginstance->data->base, &ginstance->data->dependency_tracker);
		}
	} else if (ginstance->data->base_type == RS::INSTANCE_MESH) {
		if (mesh_storage->skeleton_is_valid(ginstance->data->skeleton)) {
			ginstance->transforms_uniform_set = mesh_storage->skeleton_get_3d_uniform_set(ginstance->data->skeleton, scene_shader.default_shader_rd, TRANSFORMS_UNIFORM_SET);
			if (ginstance->data->dirty_dependencies) {
				mesh_storage->skeleton_update_dependency(ginstance->data->skeleton, &ginstance->data->dependency_tracker);
			}
		} else {
			ginstance->transforms_uniform_set = RID();
		}
	}

	ginstance->store_transform_cache = store_transform;

	if (ginstance->data->dirty_dependencies) {
		ginstance->data->dependency_tracker.update_end();
		ginstance->data->dirty_dependencies = false;
	}

	ginstance->dirty_list_element.remove_from_list();
}

static RD::FramebufferFormatID _get_color_framebuffer_format_for_pipeline(RD::DataFormat p_color_format, bool p_can_be_storage, RD::TextureSamples p_samples, RD::TextureSamples p_target_samples, bool p_vrs, bool p_post_pass, bool p_hdr, uint32_t p_view_count) {
	const bool multisampling = p_samples > RD::TEXTURE_SAMPLES_1;
	RD::AttachmentFormat attachment;

	RD::AttachmentFormat unused_attachment;
	unused_attachment.usage_flags = RD::AttachmentFormat::UNUSED_ATTACHMENT;

	thread_local Vector<RD::AttachmentFormat> attachments;
	attachments.clear();

	// Color attachment.
	attachment.samples = p_samples;
	attachment.format = p_color_format;
	attachment.usage_flags = RenderSceneBuffersRD::get_color_usage_bits(false, multisampling, p_can_be_storage);
	attachments.push_back(attachment);

	// Depth attachment.
	attachment.samples = p_samples;
	attachment.format = RenderSceneBuffersRD::get_depth_format(false, multisampling, p_can_be_storage);
	attachment.usage_flags = RenderSceneBuffersRD::get_depth_usage_bits(false, multisampling, p_can_be_storage);
	attachments.push_back(attachment);

	if (p_vrs) {
		attachment.samples = RD::TEXTURE_SAMPLES_1;
		attachment.format = RenderSceneBuffersRD::get_vrs_format();
		attachment.usage_flags = RenderSceneBuffersRD::get_vrs_usage_bits();
	}

	if (multisampling) {
		// Resolve attachment.
		attachment.samples = RD::TEXTURE_SAMPLES_1;
		attachment.format = p_color_format;
		attachment.usage_flags = RenderSceneBuffersRD::get_color_usage_bits(true, false, p_can_be_storage);
		attachments.push_back(attachment);
	}

	RD::FramebufferPass pass;
	thread_local Vector<RD::FramebufferPass> passes;
	passes.clear();
	pass.color_attachments.clear();
	pass.color_attachments.push_back(0);
	pass.depth_attachment = 1;

	if (p_vrs) {
		pass.vrs_attachment = 2;
	}

	if (multisampling) {
		pass.resolve_attachments.push_back(attachments.size() - 1);
	}

	passes.push_back(pass);

	if (p_post_pass) {
		attachment.format = RendererRD::TextureStorage::render_target_get_color_format(p_hdr, false);

		if (p_view_count > 1 || p_target_samples == RD::TEXTURE_SAMPLES_1) {
			attachment.samples = RD::TEXTURE_SAMPLES_1;
			attachment.usage_flags = RendererRD::TextureStorage::render_target_get_color_usage_bits(false);
		} else {
			attachment.samples = p_target_samples;
			attachment.usage_flags = RendererRD::TextureStorage::render_target_get_color_usage_bits(true);
		}

		attachments.push_back(attachment);

		RD::FramebufferPass blit_pass;
		blit_pass.input_attachments.push_back(multisampling ? (attachments.size() - 2) : 0);
		blit_pass.color_attachments.push_back(attachments.size() - 1);
		passes.push_back(blit_pass);
	}

	return RD::get_singleton()->framebuffer_format_create_multipass(attachments, passes, p_view_count);
}

static RD::FramebufferFormatID _get_reflection_probe_color_framebuffer_format_for_pipeline() {
	RD::AttachmentFormat attachment;
	thread_local Vector<RD::AttachmentFormat> attachments;
	attachments.clear();

	attachment.format = RendererRD::LightStorage::get_reflection_probe_color_format();
	attachment.usage_flags = RendererRD::LightStorage::get_reflection_probe_color_usage_bits();
	attachments.push_back(attachment);

	attachment.format = RendererRD::LightStorage::get_reflection_probe_depth_format();
	attachment.usage_flags = RendererRD::LightStorage::get_reflection_probe_depth_usage_bits();
	attachments.push_back(attachment);

	return RD::get_singleton()->framebuffer_format_create(attachments);
}

static RD::FramebufferFormatID _get_shadow_cubemap_framebuffer_format_for_pipeline() {
	thread_local LocalVector<RD::AttachmentFormat> attachments;
	attachments.clear();

	RD::AttachmentFormat attachment;
	attachment.format = RendererRD::LightStorage::get_cubemap_depth_format();
	attachment.usage_flags = RendererRD::LightStorage::get_cubemap_depth_usage_bits();
	attachments.push_back(attachment);

	return RD::get_singleton()->framebuffer_format_create(attachments);
}

static RD::FramebufferFormatID _get_shadow_atlas_framebuffer_format_for_pipeline(bool p_use_16_bits) {
	thread_local LocalVector<RD::AttachmentFormat> attachments;
	attachments.clear();

	RD::AttachmentFormat attachment;
	attachment.format = RendererRD::LightStorage::get_shadow_atlas_depth_format(p_use_16_bits);
	attachment.usage_flags = RendererRD::LightStorage::get_shadow_atlas_depth_usage_bits();
	attachments.push_back(attachment);

	return RD::get_singleton()->framebuffer_format_create(attachments);
}

void RenderForwardMobile::_mesh_compile_pipeline_for_surface(SceneShaderForwardMobile::ShaderData *p_shader, void *p_mesh_surface, bool p_instanced_surface, RS::PipelineSource p_source, SceneShaderForwardMobile::ShaderData::PipelineKey &r_pipeline_key, Vector<ShaderPipelinePair> *r_pipeline_pairs) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
	uint64_t input_mask = p_shader->get_vertex_input_mask(r_pipeline_key.version, true);
	r_pipeline_key.vertex_format_id = mesh_storage->mesh_surface_get_vertex_format(p_mesh_surface, input_mask, p_instanced_surface, false);
	r_pipeline_key.ubershader = true;
	p_shader->pipeline_hash_map.compile_pipeline(r_pipeline_key, r_pipeline_key.hash(), p_source);

	if (r_pipeline_pairs != nullptr) {
		r_pipeline_pairs->push_back({ p_shader, r_pipeline_key });
	}
}

void RenderForwardMobile::_mesh_compile_pipelines_for_surface(const SurfacePipelineData &p_surface, const GlobalPipelineData &p_global, RS::PipelineSource p_source, Vector<ShaderPipelinePair> *r_pipeline_pairs) {
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	// Set the attributes common to all pipelines.
	SceneShaderForwardMobile::ShaderData::PipelineKey pipeline_key;
	pipeline_key.cull_mode = RD::POLYGON_CULL_DISABLED;
	pipeline_key.primitive_type = mesh_storage->mesh_surface_get_primitive(p_surface.mesh_surface);
	pipeline_key.wireframe = false;

	const bool multiview_enabled = p_global.use_multiview && scene_shader.is_multiview_enabled();
	const RD::DataFormat buffers_color_format = _render_buffers_get_color_format();
	const bool buffers_can_be_storage = _render_buffers_can_be_storage();
	const uint32_t vrs_iterations = is_vrs_supported() ? 2 : 1;
	for (uint32_t use_vrs = 0; use_vrs < vrs_iterations; use_vrs++) {
		for (uint32_t use_post_pass = 0; use_post_pass < 2; use_post_pass++) {
			const uint32_t hdr_iterations = use_post_pass ? 2 : 1;
			for (uint32_t use_hdr = 0; use_hdr < hdr_iterations; use_hdr++) {
				pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS;
				pipeline_key.framebuffer_format_id = _get_color_framebuffer_format_for_pipeline(buffers_color_format, buffers_can_be_storage, RD::TextureSamples(p_global.texture_samples), RD::TextureSamples(p_global.target_samples), use_vrs, use_post_pass, use_hdr, 1);
				_mesh_compile_pipeline_for_surface(p_surface.shader, p_surface.mesh_surface, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);

				if (p_global.use_lightmaps && p_surface.can_use_lightmap) {
					pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_LIGHTMAP_COLOR_PASS;
					_mesh_compile_pipeline_for_surface(p_surface.shader, p_surface.mesh_surface, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
				}

				if (multiview_enabled) {
					// View count is assumed to be 2 as the configuration is dependent on the viewport. It's likely a safe assumption for stereo rendering.
					const uint32_t view_count = 2;
					pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS_MULTIVIEW;
					pipeline_key.framebuffer_format_id = _get_color_framebuffer_format_for_pipeline(buffers_color_format, buffers_can_be_storage, RD::TextureSamples(p_global.texture_samples), RD::TextureSamples(p_global.target_samples), use_vrs, use_post_pass, use_hdr, view_count);
					_mesh_compile_pipeline_for_surface(p_surface.shader, p_surface.mesh_surface, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);

					if (p_global.use_lightmaps && p_surface.can_use_lightmap) {
						pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_LIGHTMAP_COLOR_PASS_MULTIVIEW;
						_mesh_compile_pipeline_for_surface(p_surface.shader, p_surface.mesh_surface, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
					}
				}
			}
		}
	}

	if (p_global.use_reflection_probes) {
		pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS;
		pipeline_key.framebuffer_format_id = _get_reflection_probe_color_framebuffer_format_for_pipeline();
		_mesh_compile_pipeline_for_surface(p_surface.shader, p_surface.mesh_surface, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
	}

	if (!p_surface.uses_depth) {
		return;
	}

	if (p_global.use_shadow_cubemaps) {
		pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS;
		pipeline_key.framebuffer_format_id = _get_shadow_cubemap_framebuffer_format_for_pipeline();
		_mesh_compile_pipeline_for_surface(p_surface.shader_shadow, p_surface.mesh_surface_shadow, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
	}

	const uint32_t use_16_bits_start = p_global.use_32_bit_shadows ? 0 : 1;
	const uint32_t use_16_bits_iterations = p_global.use_16_bit_shadows ? 2 : 1;
	for (uint32_t use_16_bits = use_16_bits_start; use_16_bits < use_16_bits_iterations; use_16_bits++) {
		pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS;
		pipeline_key.framebuffer_format_id = _get_shadow_atlas_framebuffer_format_for_pipeline(use_16_bits);
		_mesh_compile_pipeline_for_surface(p_surface.shader_shadow, p_surface.mesh_surface_shadow, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);

		if (p_global.use_shadow_dual_paraboloid) {
			pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS_DP;
			_mesh_compile_pipeline_for_surface(p_surface.shader_shadow, p_surface.mesh_surface_shadow, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
		}

		if (multiview_enabled) {
			pipeline_key.version = SceneShaderForwardMobile::SHADER_VERSION_SHADOW_PASS_MULTIVIEW;
			_mesh_compile_pipeline_for_surface(p_surface.shader_shadow, p_surface.mesh_surface_shadow, p_surface.instanced, p_source, pipeline_key, r_pipeline_pairs);
		}
	}
}

void RenderForwardMobile::_mesh_generate_all_pipelines_for_surface_cache(GeometryInstanceSurfaceDataCache *p_surface_cache, const GlobalPipelineData &p_global) {
	bool uses_alpha_pass = (p_surface_cache->flags & GeometryInstanceSurfaceDataCache::FLAG_PASS_ALPHA) != 0;
	SurfacePipelineData surface;
	surface.mesh_surface = p_surface_cache->surface;
	surface.mesh_surface_shadow = p_surface_cache->surface_shadow;
	surface.shader = p_surface_cache->shader;
	surface.shader_shadow = p_surface_cache->shader_shadow;
	surface.instanced = p_surface_cache->owner->mesh_instance.is_valid();
	surface.uses_opaque = !uses_alpha_pass;
	surface.uses_transparent = uses_alpha_pass;
	surface.uses_depth = (p_surface_cache->flags & (GeometryInstanceSurfaceDataCache::FLAG_PASS_DEPTH | GeometryInstanceSurfaceDataCache::FLAG_PASS_OPAQUE | GeometryInstanceSurfaceDataCache::FLAG_PASS_SHADOW)) != 0;
	surface.can_use_lightmap = p_surface_cache->owner->lightmap_instance.is_valid() || p_surface_cache->owner->lightmap_sh;
	_mesh_compile_pipelines_for_surface(surface, p_global, RS::PIPELINE_SOURCE_SURFACE);
}

void RenderForwardMobile::_update_dirty_geometry_instances() {
	while (geometry_instance_dirty_list.first()) {
		_geometry_instance_update(geometry_instance_dirty_list.first()->self());
	}

	_update_dirty_geometry_pipelines();
}

void RenderForwardMobile::_update_dirty_geometry_pipelines() {
	if (global_pipeline_data_required.key != global_pipeline_data_compiled.key) {
		// Go through the entire list of surfaces and compile pipelines for everything again.
		SelfList<GeometryInstanceSurfaceDataCache> *list = geometry_surface_compilation_all_list.first();
		while (list != nullptr) {
			GeometryInstanceSurfaceDataCache *surface_cache = list->self();
			_mesh_generate_all_pipelines_for_surface_cache(surface_cache, global_pipeline_data_required);

			if (surface_cache->compilation_dirty_element.in_list()) {
				// Remove any elements from the dirty list as they don't need to be processed again.
				geometry_surface_compilation_dirty_list.remove(&surface_cache->compilation_dirty_element);
			}

			list = list->next();
		}

		global_pipeline_data_compiled.key = global_pipeline_data_required.key;
	} else {
		// Compile pipelines only for the dirty list.
		if (!geometry_surface_compilation_dirty_list.first()) {
			return;
		}

		while (geometry_surface_compilation_dirty_list.first() != nullptr) {
			GeometryInstanceSurfaceDataCache *surface_cache = geometry_surface_compilation_dirty_list.first()->self();
			_mesh_generate_all_pipelines_for_surface_cache(surface_cache, global_pipeline_data_compiled);
			surface_cache->compilation_dirty_element.remove_from_list();
		}
	}
}

void RenderForwardMobile::_geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	switch (p_notification) {
		case Dependency::DEPENDENCY_CHANGED_MATERIAL:
		case Dependency::DEPENDENCY_CHANGED_MESH:
		case Dependency::DEPENDENCY_CHANGED_PARTICLES:
		case Dependency::DEPENDENCY_CHANGED_MULTIMESH:
		case Dependency::DEPENDENCY_CHANGED_SKELETON_DATA: {
			static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
			static_cast<GeometryInstanceForwardMobile *>(p_tracker->userdata)->data->dirty_dependencies = true;
		} break;
		case Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES: {
			GeometryInstanceForwardMobile *ginstance = static_cast<GeometryInstanceForwardMobile *>(p_tracker->userdata);
			if (ginstance->data->base_type == RS::INSTANCE_MULTIMESH) {
				ginstance->instance_count = RendererRD::MeshStorage::get_singleton()->multimesh_get_instances_to_draw(ginstance->data->base);
			}
		} break;
		default: {
			//rest of notifications of no interest
		} break;
	}
}
void RenderForwardMobile::_geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker) {
	static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
	static_cast<GeometryInstanceForwardMobile *>(p_tracker->userdata)->data->dirty_dependencies = true;
}

/* misc */

bool RenderForwardMobile::is_dynamic_gi_supported() const {
	return false;
}

bool RenderForwardMobile::is_volumetric_supported() const {
	return false;
}

uint32_t RenderForwardMobile::get_max_elements() const {
	return 256;
}

RenderForwardMobile *RenderForwardMobile::singleton = nullptr;

void RenderForwardMobile::_update_shader_quality_settings() {
	SceneShaderForwardMobile::ShaderSpecialization specialization = {};
	specialization.soft_shadow_samples = soft_shadow_samples_get();
	specialization.penumbra_shadow_samples = penumbra_shadow_samples_get();
	specialization.directional_soft_shadow_samples = directional_soft_shadow_samples_get();
	specialization.directional_penumbra_shadow_samples = directional_penumbra_shadow_samples_get();
	specialization.decal_use_mipmaps =
			decals_get_filter() == RS::DECAL_FILTER_NEAREST_MIPMAPS ||
			decals_get_filter() == RS::DECAL_FILTER_LINEAR_MIPMAPS ||
			decals_get_filter() == RS::DECAL_FILTER_NEAREST_MIPMAPS_ANISOTROPIC ||
			decals_get_filter() == RS::DECAL_FILTER_LINEAR_MIPMAPS_ANISOTROPIC;

	specialization.projector_use_mipmaps =
			light_projectors_get_filter() == RS::LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS ||
			light_projectors_get_filter() == RS::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS ||
			light_projectors_get_filter() == RS::LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS_ANISOTROPIC ||
			light_projectors_get_filter() == RS::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC;

	specialization.use_lightmap_bicubic_filter = lightmap_filter_bicubic_get();
	specialization.luminance_multiplier = 2.0f;
	scene_shader.set_default_specialization(specialization);

	base_uniforms_changed(); //also need this
}

RenderForwardMobile::RenderForwardMobile() {
	singleton = this;

	sky.set_texture_format(_render_buffers_get_color_format());

	String defines;

	defines += "\n#define MAX_ROUGHNESS_LOD " + itos(get_roughness_layers() - 1) + ".0\n";
	if (is_using_radiance_cubemap_array()) {
		defines += "\n#define USE_RADIANCE_CUBEMAP_ARRAY \n";
	}
	// defines += "\n#define SDFGI_OCT_SIZE " + itos(gi.sdfgi_get_lightprobe_octahedron_size()) + "\n";
	defines += "\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS " + itos(MAX_DIRECTIONAL_LIGHTS) + "\n";

	bool force_vertex_shading = GLOBAL_GET("rendering/shading/overrides/force_vertex_shading");
	if (force_vertex_shading) {
		defines += "\n#define USE_VERTEX_LIGHTING\n";
	}

	{
		//lightmaps
		scene_state.max_lightmaps = 2;
		defines += "\n#define MAX_LIGHTMAP_TEXTURES " + itos(scene_state.max_lightmaps) + "\n";
		defines += "\n#define MAX_LIGHTMAPS " + itos(scene_state.max_lightmaps) + "\n";

		scene_state.lightmap_buffer = RD::get_singleton()->storage_buffer_create(sizeof(LightmapData) * scene_state.max_lightmaps);
	}
	{
		//captures
		scene_state.max_lightmap_captures = 2048;
		scene_state.lightmap_captures = memnew_arr(LightmapCaptureData, scene_state.max_lightmap_captures);
		scene_state.lightmap_capture_buffer = RD::get_singleton()->storage_buffer_create(sizeof(LightmapCaptureData) * scene_state.max_lightmap_captures);
	}
	{
		defines += "\n#define MATERIAL_UNIFORM_SET " + itos(MATERIAL_UNIFORM_SET) + "\n";
	}
#ifdef REAL_T_IS_DOUBLE
	{
		defines += "\n#define USE_DOUBLE_PRECISION \n";
	}
#endif

	scene_shader.init(defines);

	_update_shader_quality_settings();
}

RenderForwardMobile::~RenderForwardMobile() {
	RSG::light_storage->directional_shadow_atlas_set_size(0);

	//clear base uniform set if still valid
	for (uint32_t i = 0; i < render_pass_uniform_sets.size(); i++) {
		if (render_pass_uniform_sets[i].is_valid() && RD::get_singleton()->uniform_set_is_valid(render_pass_uniform_sets[i])) {
			RD::get_singleton()->free(render_pass_uniform_sets[i]);
		}
	}

	{
		for (const RID &rid : scene_state.uniform_buffers) {
			RD::get_singleton()->free(rid);
		}
		for (uint32_t i = 0; i < RENDER_LIST_MAX; i++) {
			if (scene_state.instance_buffer[i].is_valid()) {
				RD::get_singleton()->free(scene_state.instance_buffer[i]);
			}
		}
		RD::get_singleton()->free(scene_state.lightmap_buffer);
		RD::get_singleton()->free(scene_state.lightmap_capture_buffer);
		memdelete_arr(scene_state.lightmap_captures);
	}
}
