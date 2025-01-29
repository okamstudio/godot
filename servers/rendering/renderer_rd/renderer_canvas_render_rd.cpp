/**************************************************************************/
/*  renderer_canvas_render_rd.cpp                                         */
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

#include "renderer_canvas_render_rd.h"

#include "core/config/project_settings.h"
#include "core/math/geometry_2d.h"
#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/transform_interpolator.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"

void RendererCanvasRenderRD::_update_transform_2d_to_mat4(const Transform2D &p_transform, float *p_mat4) {
	p_mat4[0] = p_transform.columns[0][0];
	p_mat4[1] = p_transform.columns[0][1];
	p_mat4[2] = 0;
	p_mat4[3] = 0;
	p_mat4[4] = p_transform.columns[1][0];
	p_mat4[5] = p_transform.columns[1][1];
	p_mat4[6] = 0;
	p_mat4[7] = 0;
	p_mat4[8] = 0;
	p_mat4[9] = 0;
	p_mat4[10] = 1;
	p_mat4[11] = 0;
	p_mat4[12] = p_transform.columns[2][0];
	p_mat4[13] = p_transform.columns[2][1];
	p_mat4[14] = 0;
	p_mat4[15] = 1;
}

void RendererCanvasRenderRD::_update_transform_2d_to_mat2x4(const Transform2D &p_transform, float *p_mat2x4) {
	p_mat2x4[0] = p_transform.columns[0][0];
	p_mat2x4[1] = p_transform.columns[1][0];
	p_mat2x4[2] = 0;
	p_mat2x4[3] = p_transform.columns[2][0];

	p_mat2x4[4] = p_transform.columns[0][1];
	p_mat2x4[5] = p_transform.columns[1][1];
	p_mat2x4[6] = 0;
	p_mat2x4[7] = p_transform.columns[2][1];
}

void RendererCanvasRenderRD::_update_transform_2d_to_mat2x3(const Transform2D &p_transform, float *p_mat2x3) {
	p_mat2x3[0] = p_transform.columns[0][0];
	p_mat2x3[1] = p_transform.columns[0][1];
	p_mat2x3[2] = p_transform.columns[1][0];
	p_mat2x3[3] = p_transform.columns[1][1];
	p_mat2x3[4] = p_transform.columns[2][0];
	p_mat2x3[5] = p_transform.columns[2][1];
}

void RendererCanvasRenderRD::_update_transform_to_mat4(const Transform3D &p_transform, float *p_mat4) {
	p_mat4[0] = p_transform.basis.rows[0][0];
	p_mat4[1] = p_transform.basis.rows[1][0];
	p_mat4[2] = p_transform.basis.rows[2][0];
	p_mat4[3] = 0;
	p_mat4[4] = p_transform.basis.rows[0][1];
	p_mat4[5] = p_transform.basis.rows[1][1];
	p_mat4[6] = p_transform.basis.rows[2][1];
	p_mat4[7] = 0;
	p_mat4[8] = p_transform.basis.rows[0][2];
	p_mat4[9] = p_transform.basis.rows[1][2];
	p_mat4[10] = p_transform.basis.rows[2][2];
	p_mat4[11] = 0;
	p_mat4[12] = p_transform.origin.x;
	p_mat4[13] = p_transform.origin.y;
	p_mat4[14] = p_transform.origin.z;
	p_mat4[15] = 1;
}

RendererCanvasRender::PolygonID RendererCanvasRenderRD::request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, const Vector<int> &p_bones, const Vector<float> &p_weights) {
	// Care must be taken to generate array formats
	// in ways where they could be reused, so we will
	// put single-occurring elements first, and repeated
	// elements later. This way the generated formats are
	// the same no matter the length of the arrays.
	// This dramatically reduces the amount of pipeline objects
	// that need to be created for these formats.

	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	uint32_t vertex_count = p_points.size();
	uint32_t stride = 2; //vertices always repeat
	if ((uint32_t)p_colors.size() == vertex_count || p_colors.size() == 1) {
		stride += 4;
	}
	if ((uint32_t)p_uvs.size() == vertex_count) {
		stride += 2;
	}
	if ((uint32_t)p_bones.size() == vertex_count * 4 && (uint32_t)p_weights.size() == vertex_count * 4) {
		stride += 4;
	}

	uint32_t buffer_size = stride * p_points.size();

	Vector<uint8_t> polygon_buffer;
	polygon_buffer.resize(buffer_size * sizeof(float));
	Vector<RD::VertexAttribute> descriptions;
	descriptions.resize(5);
	Vector<RID> buffers;
	buffers.resize(5);

	{
		uint8_t *r = polygon_buffer.ptrw();
		float *fptr = reinterpret_cast<float *>(r);
		uint32_t *uptr = reinterpret_cast<uint32_t *>(r);
		uint32_t base_offset = 0;
		{ //vertices
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RS::ARRAY_VERTEX;
			vd.stride = stride * sizeof(float);

			descriptions.write[0] = vd;

			const Vector2 *points_ptr = p_points.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				fptr[base_offset + i * stride + 0] = points_ptr[i].x;
				fptr[base_offset + i * stride + 1] = points_ptr[i].y;
			}

			base_offset += 2;
		}

		//colors
		if ((uint32_t)p_colors.size() == vertex_count || p_colors.size() == 1) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RS::ARRAY_COLOR;
			vd.stride = stride * sizeof(float);

			descriptions.write[1] = vd;

			if (p_colors.size() == 1) {
				Color color = p_colors[0];
				for (uint32_t i = 0; i < vertex_count; i++) {
					fptr[base_offset + i * stride + 0] = color.r;
					fptr[base_offset + i * stride + 1] = color.g;
					fptr[base_offset + i * stride + 2] = color.b;
					fptr[base_offset + i * stride + 3] = color.a;
				}
			} else {
				const Color *color_ptr = p_colors.ptr();

				for (uint32_t i = 0; i < vertex_count; i++) {
					fptr[base_offset + i * stride + 0] = color_ptr[i].r;
					fptr[base_offset + i * stride + 1] = color_ptr[i].g;
					fptr[base_offset + i * stride + 2] = color_ptr[i].b;
					fptr[base_offset + i * stride + 3] = color_ptr[i].a;
				}
			}
			base_offset += 4;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = 0;
			vd.location = RS::ARRAY_COLOR;
			vd.stride = 0;

			descriptions.write[1] = vd;
			buffers.write[1] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_COLOR);
		}

		//uvs
		if ((uint32_t)p_uvs.size() == vertex_count) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RS::ARRAY_TEX_UV;
			vd.stride = stride * sizeof(float);

			descriptions.write[2] = vd;

			const Vector2 *uv_ptr = p_uvs.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				fptr[base_offset + i * stride + 0] = uv_ptr[i].x;
				fptr[base_offset + i * stride + 1] = uv_ptr[i].y;
			}
			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			vd.offset = 0;
			vd.location = RS::ARRAY_TEX_UV;
			vd.stride = 0;

			descriptions.write[2] = vd;
			buffers.write[2] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_TEX_UV);
		}

		//bones
		if ((uint32_t)p_indices.size() == vertex_count * 4 && (uint32_t)p_weights.size() == vertex_count * 4) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R16G16B16A16_UINT;
			vd.offset = base_offset * sizeof(float);
			vd.location = RS::ARRAY_BONES;
			vd.stride = stride * sizeof(float);

			descriptions.write[3] = vd;

			const int *bone_ptr = p_bones.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				uint16_t *bone16w = (uint16_t *)&uptr[base_offset + i * stride];

				bone16w[0] = bone_ptr[i * 4 + 0];
				bone16w[1] = bone_ptr[i * 4 + 1];
				bone16w[2] = bone_ptr[i * 4 + 2];
				bone16w[3] = bone_ptr[i * 4 + 3];
			}

			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_UINT;
			vd.offset = 0;
			vd.location = RS::ARRAY_BONES;
			vd.stride = 0;

			descriptions.write[3] = vd;
			buffers.write[3] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_BONES);
		}

		//weights
		if ((uint32_t)p_weights.size() == vertex_count * 4) {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R16G16B16A16_UNORM;
			vd.offset = base_offset * sizeof(float);
			vd.location = RS::ARRAY_WEIGHTS;
			vd.stride = stride * sizeof(float);

			descriptions.write[4] = vd;

			const float *weight_ptr = p_weights.ptr();

			for (uint32_t i = 0; i < vertex_count; i++) {
				uint16_t *weight16w = (uint16_t *)&uptr[base_offset + i * stride];

				weight16w[0] = CLAMP(weight_ptr[i * 4 + 0] * 65535, 0, 65535);
				weight16w[1] = CLAMP(weight_ptr[i * 4 + 1] * 65535, 0, 65535);
				weight16w[2] = CLAMP(weight_ptr[i * 4 + 2] * 65535, 0, 65535);
				weight16w[3] = CLAMP(weight_ptr[i * 4 + 3] * 65535, 0, 65535);
			}

			base_offset += 2;
		} else {
			RD::VertexAttribute vd;
			vd.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			vd.offset = 0;
			vd.location = RS::ARRAY_WEIGHTS;
			vd.stride = 0;

			descriptions.write[4] = vd;
			buffers.write[4] = mesh_storage->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DEFAULT_RD_BUFFER_WEIGHTS);
		}

		//check that everything is as it should be
		ERR_FAIL_COND_V(base_offset != stride, 0); //bug
	}

	RD::VertexFormatID vertex_id = RD::get_singleton()->vertex_format_create(descriptions);
	ERR_FAIL_COND_V(vertex_id == RD::INVALID_ID, 0);

	PolygonBuffers pb;
	pb.vertex_buffer = RD::get_singleton()->vertex_buffer_create(polygon_buffer.size(), polygon_buffer);
	for (int i = 0; i < descriptions.size(); i++) {
		if (buffers[i] == RID()) { //if put in vertex, use as vertex
			buffers.write[i] = pb.vertex_buffer;
		}
	}

	pb.vertex_array = RD::get_singleton()->vertex_array_create(p_points.size(), vertex_id, buffers);
	pb.primitive_count = vertex_count;

	if (p_indices.size()) {
		//create indices, as indices were requested
		Vector<uint8_t> index_buffer;
		index_buffer.resize(p_indices.size() * sizeof(int32_t));
		{
			uint8_t *w = index_buffer.ptrw();
			memcpy(w, p_indices.ptr(), sizeof(int32_t) * p_indices.size());
		}
		pb.index_buffer = RD::get_singleton()->index_buffer_create(p_indices.size(), RD::INDEX_BUFFER_FORMAT_UINT32, index_buffer);
		pb.indices = RD::get_singleton()->index_array_create(pb.index_buffer, 0, p_indices.size());
		pb.primitive_count = p_indices.size();
	}

	pb.vertex_format_id = vertex_id;

	PolygonID id = polygon_buffers.last_id++;

	polygon_buffers.polygons[id] = pb;

	return id;
}

void RendererCanvasRenderRD::free_polygon(PolygonID p_polygon) {
	PolygonBuffers *pb_ptr = polygon_buffers.polygons.getptr(p_polygon);
	ERR_FAIL_NULL(pb_ptr);

	PolygonBuffers &pb = *pb_ptr;

	if (pb.indices.is_valid()) {
		RD::get_singleton()->free(pb.indices);
	}
	if (pb.index_buffer.is_valid()) {
		RD::get_singleton()->free(pb.index_buffer);
	}

	RD::get_singleton()->free(pb.vertex_array);
	RD::get_singleton()->free(pb.vertex_buffer);

	polygon_buffers.polygons.erase(p_polygon);
}

////////////////////

static RD::RenderPrimitive _primitive_type_to_render_primitive(RS::PrimitiveType p_primitive) {
	switch (p_primitive) {
		case RS::PRIMITIVE_POINTS:
			return RD::RENDER_PRIMITIVE_POINTS;
		case RS::PRIMITIVE_LINES:
			return RD::RENDER_PRIMITIVE_LINES;
		case RS::PRIMITIVE_LINE_STRIP:
			return RD::RENDER_PRIMITIVE_LINESTRIPS;
		case RS::PRIMITIVE_TRIANGLES:
			return RD::RENDER_PRIMITIVE_TRIANGLES;
		case RS::PRIMITIVE_TRIANGLE_STRIP:
			return RD::RENDER_PRIMITIVE_TRIANGLE_STRIPS;
		default:
			return RD::RENDER_PRIMITIVE_MAX;
	}
}

_FORCE_INLINE_ static uint32_t _indices_to_primitives(RS::PrimitiveType p_primitive, uint32_t p_indices) {
	static const uint32_t divisor[RS::PRIMITIVE_MAX] = { 1, 2, 1, 3, 1 };
	static const uint32_t subtractor[RS::PRIMITIVE_MAX] = { 0, 0, 1, 0, 1 };
	return (p_indices - subtractor[p_primitive]) / divisor[p_primitive];
}

RID RendererCanvasRenderRD::_create_base_uniform_set(RID p_to_render_target, bool p_backbuffer) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	//re create canvas state
	Vector<RD::Uniform> uniforms;

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 1;
		u.append_id(state.canvas_state_buffer);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 2;
		u.append_id(state.lights_uniform_buffer);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 3;
		u.append_id(RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture());
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 4;
		u.append_id(state.shadow_texture);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
		u.binding = 5;
		u.append_id(state.shadow_sampler);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 6;
		RID screen;
		if (p_backbuffer) {
			screen = texture_storage->render_target_get_rd_texture(p_to_render_target);
		} else {
			screen = texture_storage->render_target_get_rd_backbuffer(p_to_render_target);
			if (screen.is_null()) { //unallocated backbuffer
				screen = RendererRD::TextureStorage::get_singleton()->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
			}
		}
		u.append_id(screen);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
		u.binding = 7;
		RID sdf = texture_storage->render_target_get_sdf_texture(p_to_render_target);
		u.append_id(sdf);
		uniforms.push_back(u);
	}

	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = 9;
		u.append_id(RendererRD::MaterialStorage::get_singleton()->global_shader_uniforms_get_storage_buffer());
		uniforms.push_back(u);
	}

	material_storage->samplers_rd_get_default().append_uniforms(uniforms, SAMPLERS_BINDING_FIRST_INDEX);

	RID uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shader.default_version_rd_shader, BASE_UNIFORM_SET);
	if (p_backbuffer) {
		texture_storage->render_target_set_backbuffer_uniform_set(p_to_render_target, uniform_set);
	} else {
		texture_storage->render_target_set_framebuffer_uniform_set(p_to_render_target, uniform_set);
	}

	return uniform_set;
}

RID RendererCanvasRenderRD::_get_pipeline_specialization_or_ubershader(CanvasShaderData *p_shader_data, PipelineKey &r_pipeline_key, PushConstant &r_push_constant, RID p_mesh_instance, void *p_surface, uint32_t p_surface_index, RID *r_vertex_array) {
	r_pipeline_key.ubershader = 0;

	const uint32_t ubershader_iterations = 1;
	while (r_pipeline_key.ubershader < ubershader_iterations) {
		if (r_vertex_array != nullptr) {
			RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
			uint64_t input_mask = p_shader_data->get_vertex_input_mask(r_pipeline_key.variant, r_pipeline_key.ubershader);
			if (p_mesh_instance.is_valid()) {
				mesh_storage->mesh_instance_surface_get_vertex_arrays_and_format(p_mesh_instance, p_surface_index, input_mask, false, *r_vertex_array, r_pipeline_key.vertex_format_id);
			} else {
				mesh_storage->mesh_surface_get_vertex_arrays_and_format(p_surface, input_mask, false, *r_vertex_array, r_pipeline_key.vertex_format_id);
			}
		}

		if (r_pipeline_key.ubershader) {
			r_push_constant.shader_specialization = r_pipeline_key.shader_specialization;
			r_pipeline_key.shader_specialization = {};
		} else {
			r_push_constant.shader_specialization = {};
		}

		bool wait_for_compilation = r_pipeline_key.ubershader || ubershader_iterations == 1;
		RS::PipelineSource source = RS::PIPELINE_SOURCE_CANVAS;
		RID pipeline = p_shader_data->pipeline_hash_map.get_pipeline(r_pipeline_key, r_pipeline_key.hash(), wait_for_compilation, source);
		if (pipeline.is_valid()) {
			return pipeline;
		}

		r_pipeline_key.ubershader++;
	}

	// This case should never be reached unless the shader wasn't available.
	return RID();
}

void RendererCanvasRenderRD::canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_light_list, const Transform2D &p_canvas_transform, RenderingServer::CanvasItemTextureFilter p_default_filter, RenderingServer::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

	r_sdf_used = false;
	int item_count = 0;

	//setup canvas state uniforms if needed

	Transform2D canvas_transform_inverse = p_canvas_transform.affine_inverse();

	//setup directional lights if exist

	uint32_t light_count = 0;
	uint32_t directional_light_count = 0;
	{
		Light *l = p_directional_light_list;
		uint32_t index = 0;

		while (l) {
			if (index == state.max_lights_per_render) {
				l->render_index_cache = -1;
				l = l->next_ptr;
				continue;
			}

			CanvasLight *clight = canvas_light_owner.get_or_null(l->light_internal);
			if (!clight) { //unused or invalid texture
				l->render_index_cache = -1;
				l = l->next_ptr;
				ERR_CONTINUE(!clight);
			}

			Vector2 canvas_light_dir = l->xform_cache.columns[1].normalized();

			state.light_uniforms[index].position[0] = -canvas_light_dir.x;
			state.light_uniforms[index].position[1] = -canvas_light_dir.y;

			_update_transform_2d_to_mat2x4(clight->shadow.directional_xform, state.light_uniforms[index].shadow_matrix);

			state.light_uniforms[index].height = l->height; //0..1 here

			for (int i = 0; i < 4; i++) {
				state.light_uniforms[index].shadow_color[i] = uint8_t(CLAMP(int32_t(l->shadow_color[i] * 255.0), 0, 255));
				state.light_uniforms[index].color[i] = l->color[i];
			}

			state.light_uniforms[index].color[3] *= l->energy; //use alpha for energy, so base color can go separate

			if (state.shadow_fb.is_valid()) {
				state.light_uniforms[index].shadow_pixel_size = (1.0 / state.shadow_texture_size) * (1.0 + l->shadow_smooth);
				state.light_uniforms[index].shadow_z_far_inv = 1.0 / clight->shadow.z_far;
				state.light_uniforms[index].shadow_y_ofs = clight->shadow.y_offset;
			} else {
				state.light_uniforms[index].shadow_pixel_size = 1.0;
				state.light_uniforms[index].shadow_z_far_inv = 1.0;
				state.light_uniforms[index].shadow_y_ofs = 0;
			}

			state.light_uniforms[index].flags = l->blend_mode << LIGHT_FLAGS_BLEND_SHIFT;
			state.light_uniforms[index].flags |= l->shadow_filter << LIGHT_FLAGS_FILTER_SHIFT;
			if (clight->shadow.enabled) {
				state.light_uniforms[index].flags |= LIGHT_FLAGS_HAS_SHADOW;
			}

			l->render_index_cache = index;

			index++;
			l = l->next_ptr;
		}

		light_count = index;
		directional_light_count = light_count;
		using_directional_lights = directional_light_count > 0;
	}

	//setup lights if exist

	{
		Light *l = p_light_list;
		uint32_t index = light_count;

		while (l) {
			if (index == state.max_lights_per_render) {
				l->render_index_cache = -1;
				l = l->next_ptr;
				continue;
			}

			CanvasLight *clight = canvas_light_owner.get_or_null(l->light_internal);
			if (!clight) { //unused or invalid texture
				l->render_index_cache = -1;
				l = l->next_ptr;
				ERR_CONTINUE(!clight);
			}

			Transform2D final_xform;
			if (!RSG::canvas->_interpolation_data.interpolation_enabled || !l->interpolated) {
				final_xform = l->xform_curr;
			} else {
				real_t f = Engine::get_singleton()->get_physics_interpolation_fraction();
				TransformInterpolator::interpolate_transform_2d(l->xform_prev, l->xform_curr, final_xform, f);
			}
			// Convert light position to canvas coordinates, as all computation is done in canvas coordinates to avoid precision loss.
			Vector2 canvas_light_pos = p_canvas_transform.xform(final_xform.get_origin());
			state.light_uniforms[index].position[0] = canvas_light_pos.x;
			state.light_uniforms[index].position[1] = canvas_light_pos.y;

			_update_transform_2d_to_mat2x4(l->light_shader_xform.affine_inverse(), state.light_uniforms[index].matrix);
			_update_transform_2d_to_mat2x4(l->xform_cache.affine_inverse(), state.light_uniforms[index].shadow_matrix);

			state.light_uniforms[index].height = l->height * (p_canvas_transform.columns[0].length() + p_canvas_transform.columns[1].length()) * 0.5; //approximate height conversion to the canvas size, since all calculations are done in canvas coords to avoid precision loss
			for (int i = 0; i < 4; i++) {
				state.light_uniforms[index].shadow_color[i] = uint8_t(CLAMP(int32_t(l->shadow_color[i] * 255.0), 0, 255));
				state.light_uniforms[index].color[i] = l->color[i];
			}

			state.light_uniforms[index].color[3] *= l->energy; //use alpha for energy, so base color can go separate

			if (state.shadow_fb.is_valid()) {
				state.light_uniforms[index].shadow_pixel_size = (1.0 / state.shadow_texture_size) * (1.0 + l->shadow_smooth);
				state.light_uniforms[index].shadow_z_far_inv = 1.0 / clight->shadow.z_far;
				state.light_uniforms[index].shadow_y_ofs = clight->shadow.y_offset;
			} else {
				state.light_uniforms[index].shadow_pixel_size = 1.0;
				state.light_uniforms[index].shadow_z_far_inv = 1.0;
				state.light_uniforms[index].shadow_y_ofs = 0;
			}

			state.light_uniforms[index].flags = l->blend_mode << LIGHT_FLAGS_BLEND_SHIFT;
			state.light_uniforms[index].flags |= l->shadow_filter << LIGHT_FLAGS_FILTER_SHIFT;
			if (clight->shadow.enabled) {
				state.light_uniforms[index].flags |= LIGHT_FLAGS_HAS_SHADOW;
			}

			if (clight->texture.is_valid()) {
				Rect2 atlas_rect = RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture_rect(clight->texture);
				state.light_uniforms[index].atlas_rect[0] = atlas_rect.position.x;
				state.light_uniforms[index].atlas_rect[1] = atlas_rect.position.y;
				state.light_uniforms[index].atlas_rect[2] = atlas_rect.size.width;
				state.light_uniforms[index].atlas_rect[3] = atlas_rect.size.height;

			} else {
				state.light_uniforms[index].atlas_rect[0] = 0;
				state.light_uniforms[index].atlas_rect[1] = 0;
				state.light_uniforms[index].atlas_rect[2] = 0;
				state.light_uniforms[index].atlas_rect[3] = 0;
			}

			l->render_index_cache = index;

			index++;
			l = l->next_ptr;
		}

		light_count = index;
	}

	if (light_count > 0) {
		RD::get_singleton()->buffer_update(state.lights_uniform_buffer, 0, sizeof(LightUniform) * light_count, &state.light_uniforms[0]);
	}

	bool use_linear_colors = texture_storage->render_target_is_using_hdr(p_to_render_target);

	{
		//update canvas state uniform buffer
		State::Buffer state_buffer;

		Size2i ssize = texture_storage->render_target_get_size(p_to_render_target);

		Transform3D screen_transform;
		screen_transform.translate_local(-(ssize.width / 2.0f), -(ssize.height / 2.0f), 0.0f);
		screen_transform.scale(Vector3(2.0f / ssize.width, 2.0f / ssize.height, 1.0f));
		_update_transform_to_mat4(screen_transform, state_buffer.screen_transform);
		_update_transform_2d_to_mat4(p_canvas_transform, state_buffer.canvas_transform);

		Transform2D normal_transform = p_canvas_transform;
		normal_transform.columns[0].normalize();
		normal_transform.columns[1].normalize();
		normal_transform.columns[2] = Vector2();
		_update_transform_2d_to_mat4(normal_transform, state_buffer.canvas_normal_transform);

		Color modulate = p_modulate;
		if (use_linear_colors) {
			modulate = p_modulate.srgb_to_linear();
		}
		state_buffer.canvas_modulate[0] = modulate.r;
		state_buffer.canvas_modulate[1] = modulate.g;
		state_buffer.canvas_modulate[2] = modulate.b;
		state_buffer.canvas_modulate[3] = modulate.a;

		Size2 render_target_size = texture_storage->render_target_get_size(p_to_render_target);
		state_buffer.screen_pixel_size[0] = 1.0 / render_target_size.x;
		state_buffer.screen_pixel_size[1] = 1.0 / render_target_size.y;

		state_buffer.time = state.time;
		state_buffer.use_pixel_snap = p_snap_2d_vertices_to_pixel;

		state_buffer.directional_light_count = directional_light_count;

		Vector2 canvas_scale = p_canvas_transform.get_scale();

		state_buffer.sdf_to_screen[0] = render_target_size.width / canvas_scale.x;
		state_buffer.sdf_to_screen[1] = render_target_size.height / canvas_scale.y;

		state_buffer.screen_to_sdf[0] = 1.0 / state_buffer.sdf_to_screen[0];
		state_buffer.screen_to_sdf[1] = 1.0 / state_buffer.sdf_to_screen[1];

		Rect2 sdf_rect = texture_storage->render_target_get_sdf_rect(p_to_render_target);
		Rect2 sdf_tex_rect(sdf_rect.position / canvas_scale, sdf_rect.size / canvas_scale);

		state_buffer.sdf_to_tex[0] = 1.0 / sdf_tex_rect.size.width;
		state_buffer.sdf_to_tex[1] = 1.0 / sdf_tex_rect.size.height;
		state_buffer.sdf_to_tex[2] = -sdf_tex_rect.position.x / sdf_tex_rect.size.width;
		state_buffer.sdf_to_tex[3] = -sdf_tex_rect.position.y / sdf_tex_rect.size.height;

		//print_line("w: " + itos(ssize.width) + " s: " + rtos(canvas_scale));
		state_buffer.tex_to_sdf = 1.0 / ((canvas_scale.x + canvas_scale.y) * 0.5);

		state_buffer.flags = use_linear_colors ? CANVAS_FLAGS_CONVERT_ATTRIBUTES_TO_LINEAR : 0;

		RD::get_singleton()->buffer_update(state.canvas_state_buffer, 0, sizeof(State::Buffer), &state_buffer);
	}

	{ //default filter/repeat
		default_filter = p_default_filter;
		default_repeat = p_default_repeat;
	}

	Item *ci = p_item_list;

	//fill the list until rendering is possible.
	bool material_screen_texture_cached = false;
	bool material_screen_texture_mipmaps_cached = false;

	Rect2 back_buffer_rect;
	bool backbuffer_copy = false;
	bool backbuffer_gen_mipmaps = false;

	Item *canvas_group_owner = nullptr;
	bool skip_item = false;

	state.last_instance_index = 0;

	bool update_skeletons = false;
	bool time_used = false;

	bool backbuffer_cleared = false;

	RenderTarget to_render_target;
	to_render_target.render_target = p_to_render_target;
	to_render_target.use_linear_colors = use_linear_colors;

	while (ci) {
		if (ci->copy_back_buffer && canvas_group_owner == nullptr) {
			backbuffer_copy = true;

			if (ci->copy_back_buffer->full) {
				back_buffer_rect = Rect2();
			} else {
				back_buffer_rect = ci->copy_back_buffer->rect;
			}
		}

		RID material = ci->material_owner == nullptr ? ci->material : ci->material_owner->material;

		if (material.is_valid()) {
			CanvasMaterialData *md = static_cast<CanvasMaterialData *>(material_storage->material_get_data(material, RendererRD::MaterialStorage::SHADER_TYPE_2D));
			if (md && md->shader_data->is_valid()) {
				if (md->shader_data->uses_screen_texture && canvas_group_owner == nullptr) {
					if (!material_screen_texture_cached) {
						backbuffer_copy = true;
						back_buffer_rect = Rect2();
						backbuffer_gen_mipmaps = md->shader_data->uses_screen_texture_mipmaps;
					} else if (!material_screen_texture_mipmaps_cached) {
						backbuffer_gen_mipmaps = md->shader_data->uses_screen_texture_mipmaps;
					}
				}

				if (md->shader_data->uses_sdf) {
					r_sdf_used = true;
				}
				if (md->shader_data->uses_time) {
					time_used = true;
				}
			}
		}

		if (ci->skeleton.is_valid()) {
			const Item::Command *c = ci->commands;

			while (c) {
				if (c->type == Item::Command::TYPE_MESH) {
					const Item::CommandMesh *cm = static_cast<const Item::CommandMesh *>(c);
					if (cm->mesh_instance.is_valid()) {
						mesh_storage->mesh_instance_check_for_update(cm->mesh_instance);
						mesh_storage->mesh_instance_set_canvas_item_transform(cm->mesh_instance, canvas_transform_inverse * ci->final_transform);
						update_skeletons = true;
					}
				}
				c = c->next;
			}
		}

		if (ci->canvas_group_owner != nullptr) {
			if (canvas_group_owner == nullptr) {
				// Canvas group begins here, render until before this item
				if (update_skeletons) {
					mesh_storage->update_mesh_instances();
					update_skeletons = false;
				}
				_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, false, r_render_info);
				item_count = 0;

				if (ci->canvas_group_owner->canvas_group->mode != RS::CANVAS_GROUP_MODE_TRANSPARENT) {
					Rect2i group_rect = ci->canvas_group_owner->global_rect_cache;
					texture_storage->render_target_copy_to_back_buffer(p_to_render_target, group_rect, false);
					if (ci->canvas_group_owner->canvas_group->mode == RS::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
						ci->canvas_group_owner->use_canvas_group = false;
						items[item_count++] = ci->canvas_group_owner;
					}
				} else if (!backbuffer_cleared) {
					texture_storage->render_target_clear_back_buffer(p_to_render_target, Rect2i(), Color(0, 0, 0, 0));
					backbuffer_cleared = true;
				}

				backbuffer_copy = false;
				canvas_group_owner = ci->canvas_group_owner; //continue until owner found
			}

			ci->canvas_group_owner = nullptr; //must be cleared
		}

		if (canvas_group_owner == nullptr && ci->canvas_group != nullptr && ci->canvas_group->mode != RS::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
			skip_item = true;
		}

		if (ci == canvas_group_owner) {
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, true, r_render_info);
			item_count = 0;

			if (ci->canvas_group->blur_mipmaps) {
				texture_storage->render_target_gen_back_buffer_mipmaps(p_to_render_target, ci->global_rect_cache);
			}

			canvas_group_owner = nullptr;
			// Backbuffer is dirty now and needs to be re-cleared if another CanvasGroup needs it.
			backbuffer_cleared = false;

			// Tell the renderer to paint this as a canvas group
			ci->use_canvas_group = true;
		} else {
			ci->use_canvas_group = false;
		}

		if (backbuffer_copy) {
			//render anything pending, including clearing if no items
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, false, r_render_info);
			item_count = 0;

			texture_storage->render_target_copy_to_back_buffer(p_to_render_target, back_buffer_rect, backbuffer_gen_mipmaps);

			backbuffer_copy = false;
			material_screen_texture_cached = true; // After a backbuffer copy, screen texture makes no further copies.
			material_screen_texture_mipmaps_cached = backbuffer_gen_mipmaps;
			backbuffer_gen_mipmaps = false;
		}

		if (backbuffer_gen_mipmaps) {
			texture_storage->render_target_gen_back_buffer_mipmaps(p_to_render_target, back_buffer_rect);

			backbuffer_gen_mipmaps = false;
			material_screen_texture_mipmaps_cached = true;
		}

		if (skip_item) {
			skip_item = false;
		} else {
			items[item_count++] = ci;
		}

		if (!ci->next || item_count == MAX_RENDER_ITEMS - 1) {
			if (update_skeletons) {
				mesh_storage->update_mesh_instances();
				update_skeletons = false;
			}

			_render_batch_items(to_render_target, item_count, canvas_transform_inverse, p_light_list, r_sdf_used, canvas_group_owner != nullptr, r_render_info);
			//then reset
			item_count = 0;
		}

		ci = ci->next;
	}

	if (time_used) {
		RenderingServerDefault::redraw_request();
	}

	texture_info_map.clear();
	state.current_batch_index = 0;
	state.canvas_instance_batches.clear();
	state.current_data_buffer_index = (state.current_data_buffer_index + 1) % BATCH_DATA_BUFFER_COUNT;
	state.current_instance_buffer_index = 0;
}

RID RendererCanvasRenderRD::light_create() {
	CanvasLight canvas_light;
	return canvas_light_owner.make_rid(canvas_light);
}

void RendererCanvasRenderRD::light_set_texture(RID p_rid, RID p_texture) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(cl);
	if (cl->texture == p_texture) {
		return;
	}

	ERR_FAIL_COND(p_texture.is_valid() && !texture_storage->owns_texture(p_texture));

	if (cl->texture.is_valid()) {
		texture_storage->texture_remove_from_decal_atlas(cl->texture);
	}
	cl->texture = p_texture;

	if (cl->texture.is_valid()) {
		texture_storage->texture_add_to_decal_atlas(cl->texture);
	}
}

void RendererCanvasRenderRD::light_set_use_shadow(RID p_rid, bool p_enable) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(cl);

	cl->shadow.enabled = p_enable;
}

void RendererCanvasRenderRD::_update_shadow_atlas() {
	if (state.shadow_fb == RID()) {
		//ah, we lack the shadow texture..
		RD::get_singleton()->free(state.shadow_texture); //erase placeholder

		Vector<RID> fb_textures;

		{ //texture
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = state.shadow_texture_size;
			tf.height = state.max_lights_per_render * 2;
			tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
			tf.format = RD::DATA_FORMAT_R32_SFLOAT;

			state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
			fb_textures.push_back(state.shadow_texture);
		}
		{
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = state.shadow_texture_size;
			tf.height = state.max_lights_per_render * 2;
			tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			tf.format = RD::DATA_FORMAT_D32_SFLOAT;
			tf.is_discardable = true;
			//chunks to write
			state.shadow_depth_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
			fb_textures.push_back(state.shadow_depth_texture);
		}

		state.shadow_fb = RD::get_singleton()->framebuffer_create(fb_textures);
	}
}

void RendererCanvasRenderRD::_update_occluder_buffer(uint32_t p_size) {
	bool needs_update = state.shadow_occluder_buffer.is_null();

	if (p_size > state.shadow_occluder_buffer_size) {
		needs_update = true;
		state.shadow_occluder_buffer_size = next_power_of_2(p_size);
		if (state.shadow_occluder_buffer.is_valid()) {
			RD::get_singleton()->free(state.shadow_occluder_buffer);
		}
	}

	if (needs_update) {
		state.shadow_occluder_buffer = RD::get_singleton()->storage_buffer_create(state.shadow_occluder_buffer_size);

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(state.shadow_occluder_buffer);
			uniforms.push_back(u);
		}
		state.shadow_ocluder_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_POSITIONAL_SHADOW), 0);
	}
}

void RendererCanvasRenderRD::light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, const Rect2 &p_light_rect) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_COND(!cl->shadow.enabled);

	_update_shadow_atlas();

	cl->shadow.z_far = p_far;
	cl->shadow.y_offset = float(p_shadow_index * 2 + 1) / float(state.max_lights_per_render * 2);
	Vector<Color> cc;
	cc.push_back(Color(p_far, p_far, p_far, 1.0));

	// First, do a culling pass and record what occluders need to be drawn for this light.
	static thread_local LocalVector<OccluderPolygon *> occluders;
	static thread_local LocalVector<uint32_t> occluder_indices;
	occluders.clear();
	occluder_indices.clear();

	uint32_t occluder_count = 0;

	LightOccluderInstance *instance = p_occluders;
	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		occluder_count++;

		if (!co || co->index_array.is_null()) {
			instance = instance->next;
			continue;
		}

		if (!(p_light_mask & instance->light_mask) || !p_light_rect.intersects_transformed(instance->xform_cache, instance->aabb_cache)) {
			instance = instance->next;
			continue;
		}

		occluders.push_back(co);
		occluder_indices.push_back(occluder_count - 1);

		instance = instance->next;
	}

	// Then, upload all the occluder transforms to a shared buffer.
	// We only do this for the first light so we can avoid uploading the same
	// Transforms over and over again.
	if (p_shadow_index == 0 && occluder_count > 0) {
		static thread_local LocalVector<float> transforms;
		transforms.clear();
		transforms.resize(occluder_count * 8);

		instance = p_occluders;
		uint32_t index = 0;
		while (instance) {
			_update_transform_2d_to_mat2x4(instance->xform_cache, &transforms[index * 8]);
			index++;
			instance = instance->next;
		}

		_update_occluder_buffer(occluder_count * 8 * sizeof(float));
		RD::get_singleton()->buffer_update(state.shadow_occluder_buffer, 0, transforms.size() * sizeof(float), transforms.ptr());
	}

	Rect2i rect(0, p_shadow_index * 2, state.shadow_texture_size, 2);
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(state.shadow_fb, RD::DRAW_CLEAR_ALL, cc, 1.0f, 0, rect);

	if (state.shadow_occluder_buffer.is_valid()) {
		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.render_pipelines[SHADOW_RENDER_MODE_POSITIONAL_SHADOW]);
		RD::get_singleton()->draw_list_bind_uniform_set(draw_list, state.shadow_ocluder_uniform_set, 0);

		for (int i = 0; i < 4; i++) {
			Rect2i sub_rect((state.shadow_texture_size / 4) * i, p_shadow_index * 2, (state.shadow_texture_size / 4), 2);
			RD::get_singleton()->draw_list_set_viewport(draw_list, sub_rect);

			static const Vector2 directions[4] = { Vector2(1, 0), Vector2(0, 1), Vector2(-1, 0), Vector2(0, -1) };
			static const Vector4 rotations[4] = { Vector4(0, -1, 1, 0), Vector4(-1, 0, 0, -1), Vector4(0, 1, -1, 0), Vector4(1, 0, 0, 1) };

			PositionalShadowRenderPushConstant push_constant;
			_update_transform_2d_to_mat2x4(p_light_xform, push_constant.modelview);
			push_constant.direction[0] = directions[i].x;
			push_constant.direction[1] = directions[i].y;
			push_constant.rotation[0] = rotations[i].x;
			push_constant.rotation[1] = rotations[i].y;
			push_constant.rotation[2] = rotations[i].z;
			push_constant.rotation[3] = rotations[i].w;
			push_constant.z_far = p_far;
			push_constant.z_near = p_near;

			for (uint32_t j = 0; j < occluders.size(); j++) {
				OccluderPolygon *co = occluders[j];

				push_constant.pad = occluder_indices[j];
				push_constant.cull_mode = uint32_t(co->cull_mode);

				// The slowest part about this whole function is that we have to draw the occluders one by one, 4 times.
				// We can optimize this so that all occluders draw at once if we store vertices and indices in a giant
				// SSBO and just save an index into that SSBO for each occluder.
				RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->vertex_array);
				RD::get_singleton()->draw_list_bind_index_array(draw_list, co->index_array);
				RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(PositionalShadowRenderPushConstant));

				RD::get_singleton()->draw_list_draw(draw_list, true);
			}
		}
	}
	RD::get_singleton()->draw_list_end();
}

void RendererCanvasRenderRD::light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) {
	CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
	ERR_FAIL_COND(!cl->shadow.enabled);

	_update_shadow_atlas();

	Vector2 light_dir = p_light_xform.columns[1].normalized();

	Vector2 center = p_clip_rect.get_center();

	float to_edge_distance = ABS(light_dir.dot(p_clip_rect.get_support(-light_dir)) - light_dir.dot(center));

	Vector2 from_pos = center - light_dir * (to_edge_distance + p_cull_distance);
	float distance = to_edge_distance * 2.0 + p_cull_distance;
	float half_size = p_clip_rect.size.length() * 0.5; //shadow length, must keep this no matter the angle

	cl->shadow.z_far = distance;
	cl->shadow.y_offset = float(p_shadow_index * 2 + 1) / float(state.max_lights_per_render * 2);

	Transform2D to_light_xform;

	to_light_xform[2] = from_pos;
	to_light_xform[1] = light_dir;
	to_light_xform[0] = -light_dir.orthogonal();

	to_light_xform.invert();

	Vector<Color> cc;
	cc.push_back(Color(1, 1, 1, 1));

	Rect2i rect(0, p_shadow_index * 2, state.shadow_texture_size, 2);
	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(state.shadow_fb, RD::DRAW_CLEAR_ALL, cc, 1.0f, 0, rect);
	RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.render_pipelines[SHADOW_RENDER_MODE_DIRECTIONAL_SHADOW]);

	Projection projection;
	projection.set_orthogonal(-half_size, half_size, -0.5, 0.5, 0.0, distance);
	projection = projection * Projection(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, -1)).affine_inverse());

	ShadowRenderPushConstant push_constant;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			push_constant.projection[y * 4 + x] = projection.columns[y][x];
		}
	}

	push_constant.direction[0] = 0.0;
	push_constant.direction[1] = 1.0;
	push_constant.z_far = distance;

	LightOccluderInstance *instance = p_occluders;

	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		if (!co || co->index_array.is_null() || !(p_light_mask & instance->light_mask)) {
			instance = instance->next;
			continue;
		}

		_update_transform_2d_to_mat2x4(to_light_xform * instance->xform_cache, push_constant.modelview);
		push_constant.cull_mode = uint32_t(co->cull_mode);

		RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->vertex_array);
		RD::get_singleton()->draw_list_bind_index_array(draw_list, co->index_array);
		RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(ShadowRenderPushConstant));

		RD::get_singleton()->draw_list_draw(draw_list, true);

		instance = instance->next;
	}

	RD::get_singleton()->draw_list_end();

	Transform2D to_shadow;
	to_shadow.columns[0].x = 1.0 / -(half_size * 2.0);
	to_shadow.columns[2].x = 0.5;

	cl->shadow.directional_xform = to_shadow * to_light_xform;
}

void RendererCanvasRenderRD::render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	RID fb = texture_storage->render_target_get_sdf_framebuffer(p_render_target);
	Rect2i rect = texture_storage->render_target_get_sdf_rect(p_render_target);

	Transform2D to_sdf;
	to_sdf.columns[0] *= rect.size.width;
	to_sdf.columns[1] *= rect.size.height;
	to_sdf.columns[2] = rect.position;

	Transform2D to_clip;
	to_clip.columns[0] *= 2.0;
	to_clip.columns[1] *= 2.0;
	to_clip.columns[2] = -Vector2(1.0, 1.0);

	to_clip = to_clip * to_sdf.affine_inverse();

	Vector<Color> cc;
	cc.push_back(Color(0, 0, 0, 0));

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(fb, RD::DRAW_CLEAR_ALL, cc);

	Projection projection;

	ShadowRenderPushConstant push_constant;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			push_constant.projection[y * 4 + x] = projection.columns[y][x];
		}
	}

	push_constant.direction[0] = 0.0;
	push_constant.direction[1] = 0.0;
	push_constant.z_far = 0;
	push_constant.cull_mode = 0;

	LightOccluderInstance *instance = p_occluders;

	while (instance) {
		OccluderPolygon *co = occluder_polygon_owner.get_or_null(instance->occluder);

		if (!co || co->sdf_index_array.is_null() || !instance->sdf_collision) {
			instance = instance->next;
			continue;
		}

		_update_transform_2d_to_mat2x4(to_clip * instance->xform_cache, push_constant.modelview);

		RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shadow_render.sdf_render_pipelines[co->sdf_is_lines ? SHADOW_RENDER_SDF_LINES : SHADOW_RENDER_SDF_TRIANGLES]);
		RD::get_singleton()->draw_list_bind_vertex_array(draw_list, co->sdf_vertex_array);
		RD::get_singleton()->draw_list_bind_index_array(draw_list, co->sdf_index_array);
		RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(ShadowRenderPushConstant));

		RD::get_singleton()->draw_list_draw(draw_list, true);

		instance = instance->next;
	}

	RD::get_singleton()->draw_list_end();

	texture_storage->render_target_sdf_process(p_render_target); //done rendering, process it
}

RID RendererCanvasRenderRD::occluder_polygon_create() {
	OccluderPolygon occluder;
	occluder.line_point_count = 0;
	occluder.sdf_point_count = 0;
	occluder.sdf_index_count = 0;
	occluder.cull_mode = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
	return occluder_polygon_owner.make_rid(occluder);
}

void RendererCanvasRenderRD::occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) {
	OccluderPolygon *oc = occluder_polygon_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(oc);

	Vector<Vector2> lines;

	if (p_points.size()) {
		int lc = p_points.size() * 2;

		lines.resize(lc - (p_closed ? 0 : 2));
		{
			Vector2 *w = lines.ptrw();
			const Vector2 *r = p_points.ptr();

			int max = lc / 2;
			if (!p_closed) {
				max--;
			}
			for (int i = 0; i < max; i++) {
				Vector2 a = r[i];
				Vector2 b = r[(i + 1) % (lc / 2)];
				w[i * 2 + 0] = a;
				w[i * 2 + 1] = b;
			}
		}
	}

	if ((oc->line_point_count != lines.size() || lines.size() == 0) && oc->vertex_array.is_valid()) {
		RD::get_singleton()->free(oc->vertex_array);
		RD::get_singleton()->free(oc->vertex_buffer);
		RD::get_singleton()->free(oc->index_array);
		RD::get_singleton()->free(oc->index_buffer);

		oc->vertex_array = RID();
		oc->vertex_buffer = RID();
		oc->index_array = RID();
		oc->index_buffer = RID();

		oc->line_point_count = lines.size();
	}

	if (lines.size()) {
		oc->line_point_count = lines.size();
		Vector<uint8_t> geometry;
		Vector<uint8_t> indices;
		int lc = lines.size();

		geometry.resize(lc * 6 * sizeof(float));
		indices.resize(lc * 3 * sizeof(uint16_t));

		{
			uint8_t *vw = geometry.ptrw();
			float *vwptr = reinterpret_cast<float *>(vw);
			uint8_t *iw = indices.ptrw();
			uint16_t *iwptr = (uint16_t *)iw;

			const Vector2 *lr = lines.ptr();

			const int POLY_HEIGHT = 16384;

			for (int i = 0; i < lc / 2; i++) {
				vwptr[i * 12 + 0] = lr[i * 2 + 0].x;
				vwptr[i * 12 + 1] = lr[i * 2 + 0].y;
				vwptr[i * 12 + 2] = POLY_HEIGHT;

				vwptr[i * 12 + 3] = lr[i * 2 + 1].x;
				vwptr[i * 12 + 4] = lr[i * 2 + 1].y;
				vwptr[i * 12 + 5] = POLY_HEIGHT;

				vwptr[i * 12 + 6] = lr[i * 2 + 1].x;
				vwptr[i * 12 + 7] = lr[i * 2 + 1].y;
				vwptr[i * 12 + 8] = -POLY_HEIGHT;

				vwptr[i * 12 + 9] = lr[i * 2 + 0].x;
				vwptr[i * 12 + 10] = lr[i * 2 + 0].y;
				vwptr[i * 12 + 11] = -POLY_HEIGHT;

				iwptr[i * 6 + 0] = i * 4 + 0;
				iwptr[i * 6 + 1] = i * 4 + 1;
				iwptr[i * 6 + 2] = i * 4 + 2;

				iwptr[i * 6 + 3] = i * 4 + 2;
				iwptr[i * 6 + 4] = i * 4 + 3;
				iwptr[i * 6 + 5] = i * 4 + 0;
			}
		}

		//if same buffer len is being set, just use buffer_update to avoid a pipeline flush

		if (oc->vertex_array.is_null()) {
			//create from scratch
			//vertices
			oc->vertex_buffer = RD::get_singleton()->vertex_buffer_create(lc * 6 * sizeof(float), geometry);

			Vector<RID> buffer;
			buffer.push_back(oc->vertex_buffer);
			oc->vertex_array = RD::get_singleton()->vertex_array_create(4 * lc / 2, shadow_render.vertex_format, buffer);
			//indices

			oc->index_buffer = RD::get_singleton()->index_buffer_create(3 * lc, RD::INDEX_BUFFER_FORMAT_UINT16, indices);
			oc->index_array = RD::get_singleton()->index_array_create(oc->index_buffer, 0, 3 * lc);

		} else {
			//update existing
			const uint8_t *vr = geometry.ptr();
			RD::get_singleton()->buffer_update(oc->vertex_buffer, 0, geometry.size(), vr);
			const uint8_t *ir = indices.ptr();
			RD::get_singleton()->buffer_update(oc->index_buffer, 0, indices.size(), ir);
		}
	}

	// sdf

	Vector<int> sdf_indices;

	if (p_points.size()) {
		if (p_closed) {
			sdf_indices = Geometry2D::triangulate_polygon(p_points);
			oc->sdf_is_lines = false;
		} else {
			int max = p_points.size();
			sdf_indices.resize(max * 2);

			int *iw = sdf_indices.ptrw();
			for (int i = 0; i < max; i++) {
				iw[i * 2 + 0] = i;
				iw[i * 2 + 1] = (i + 1) % max;
			}
			oc->sdf_is_lines = true;
		}
	}

	if (((oc->sdf_index_count != sdf_indices.size() && oc->sdf_point_count != p_points.size()) || p_points.size() == 0) && oc->sdf_vertex_array.is_valid()) {
		RD::get_singleton()->free(oc->sdf_vertex_array);
		RD::get_singleton()->free(oc->sdf_vertex_buffer);
		RD::get_singleton()->free(oc->sdf_index_array);
		RD::get_singleton()->free(oc->sdf_index_buffer);

		oc->sdf_vertex_array = RID();
		oc->sdf_vertex_buffer = RID();
		oc->sdf_index_array = RID();
		oc->sdf_index_buffer = RID();

		oc->sdf_index_count = sdf_indices.size();
		oc->sdf_point_count = p_points.size();

		oc->sdf_is_lines = false;
	}

	if (sdf_indices.size()) {
		if (oc->sdf_vertex_array.is_null()) {
			//create from scratch
			//vertices
#ifdef REAL_T_IS_DOUBLE
			PackedFloat32Array float_points;
			float_points.resize(p_points.size() * 2);
			float *float_points_ptr = (float *)float_points.ptrw();
			for (int i = 0; i < p_points.size(); i++) {
				float_points_ptr[i * 2] = p_points[i].x;
				float_points_ptr[i * 2 + 1] = p_points[i].y;
			}
			oc->sdf_vertex_buffer = RD::get_singleton()->vertex_buffer_create(p_points.size() * 2 * sizeof(float), float_points.to_byte_array());
#else
			oc->sdf_vertex_buffer = RD::get_singleton()->vertex_buffer_create(p_points.size() * 2 * sizeof(float), p_points.to_byte_array());
#endif
			oc->sdf_index_buffer = RD::get_singleton()->index_buffer_create(sdf_indices.size(), RD::INDEX_BUFFER_FORMAT_UINT32, sdf_indices.to_byte_array());
			oc->sdf_index_array = RD::get_singleton()->index_array_create(oc->sdf_index_buffer, 0, sdf_indices.size());

			Vector<RID> buffer;
			buffer.push_back(oc->sdf_vertex_buffer);
			oc->sdf_vertex_array = RD::get_singleton()->vertex_array_create(p_points.size(), shadow_render.sdf_vertex_format, buffer);
			//indices

		} else {
			//update existing
#ifdef REAL_T_IS_DOUBLE
			PackedFloat32Array float_points;
			float_points.resize(p_points.size() * 2);
			float *float_points_ptr = (float *)float_points.ptrw();
			for (int i = 0; i < p_points.size(); i++) {
				float_points_ptr[i * 2] = p_points[i].x;
				float_points_ptr[i * 2 + 1] = p_points[i].y;
			}
			RD::get_singleton()->buffer_update(oc->sdf_vertex_buffer, 0, sizeof(float) * 2 * p_points.size(), float_points.ptr());
#else
			RD::get_singleton()->buffer_update(oc->sdf_vertex_buffer, 0, sizeof(float) * 2 * p_points.size(), p_points.ptr());
#endif
			RD::get_singleton()->buffer_update(oc->sdf_index_buffer, 0, sdf_indices.size() * sizeof(int32_t), sdf_indices.ptr());
		}
	}
}

void RendererCanvasRenderRD::occluder_polygon_set_cull_mode(RID p_occluder, RS::CanvasOccluderPolygonCullMode p_mode) {
	OccluderPolygon *oc = occluder_polygon_owner.get_or_null(p_occluder);
	ERR_FAIL_NULL(oc);
	oc->cull_mode = p_mode;
}

void RendererCanvasRenderRD::CanvasShaderData::_clear_vertex_input_mask_cache() {
	for (uint32_t i = 0; i < VERTEX_INPUT_MASKS_SIZE; i++) {
		vertex_input_masks[i].store(0);
	}
}

void RendererCanvasRenderRD::CanvasShaderData::_create_pipeline(PipelineKey p_pipeline_key) {
#if PRINT_PIPELINE_COMPILATION_KEYS
	print_line(
			"HASH:", p_pipeline_key.hash(),
			"VERSION:", version,
			"VARIANT:", p_pipeline_key.variant,
			"FRAMEBUFFER:", p_pipeline_key.framebuffer_format_id,
			"VERTEX:", p_pipeline_key.vertex_format_id,
			"PRIMITIVE:", p_pipeline_key.render_primitive,
			"SPEC PACKED #0:", p_pipeline_key.shader_specialization.packed_0,
			"LCD:", p_pipeline_key.lcd_blend);
#endif

	RendererRD::MaterialStorage::ShaderData::BlendMode blend_mode_rd = RendererRD::MaterialStorage::ShaderData::BlendMode(blend_mode);
	RD::PipelineColorBlendState blend_state;
	RD::PipelineColorBlendState::Attachment attachment;
	uint32_t dynamic_state_flags = 0;
	if (p_pipeline_key.lcd_blend) {
		attachment.enable_blend = true;
		attachment.alpha_blend_op = RD::BLEND_OP_ADD;
		attachment.color_blend_op = RD::BLEND_OP_ADD;
		attachment.src_color_blend_factor = RD::BLEND_FACTOR_CONSTANT_COLOR;
		attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		dynamic_state_flags = RD::DYNAMIC_STATE_BLEND_CONSTANTS;
	} else {
		attachment = RendererRD::MaterialStorage::ShaderData::blend_mode_to_blend_attachment(blend_mode_rd);
	}

	blend_state.attachments.push_back(attachment);

	RD::PipelineMultisampleState multisample_state;
	multisample_state.sample_count = RD::get_singleton()->framebuffer_format_get_texture_samples(p_pipeline_key.framebuffer_format_id, 0);

	// Convert the specialization from the key to pipeline specialization constants.
	Vector<RD::PipelineSpecializationConstant> specialization_constants;
	RD::PipelineSpecializationConstant sc;
	sc.constant_id = 0;
	sc.int_value = p_pipeline_key.shader_specialization.packed_0;
	sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
	specialization_constants.push_back(sc);

	RID shader_rid = get_shader(p_pipeline_key.variant, p_pipeline_key.ubershader);
	ERR_FAIL_COND(shader_rid.is_null());

	RID pipeline = RD::get_singleton()->render_pipeline_create(shader_rid, p_pipeline_key.framebuffer_format_id, p_pipeline_key.vertex_format_id, p_pipeline_key.render_primitive, RD::PipelineRasterizationState(), multisample_state, RD::PipelineDepthStencilState(), blend_state, dynamic_state_flags, 0, specialization_constants);
	ERR_FAIL_COND(pipeline.is_null());

	pipeline_hash_map.add_compiled_pipeline(p_pipeline_key.hash(), pipeline);
}

void RendererCanvasRenderRD::CanvasShaderData::set_code(const String &p_code) {
	//compile

	code = p_code;
	ubo_size = 0;
	uniforms.clear();
	uses_screen_texture = false;
	uses_screen_texture_mipmaps = false;
	uses_sdf = false;
	uses_time = false;
	_clear_vertex_input_mask_cache();

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;

	blend_mode = BLEND_MODE_MIX;

	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["vertex"] = ShaderCompiler::STAGE_VERTEX;
	actions.entry_point_stages["fragment"] = ShaderCompiler::STAGE_FRAGMENT;
	actions.entry_point_stages["light"] = ShaderCompiler::STAGE_FRAGMENT;

	actions.render_mode_values["blend_add"] = Pair<int *, int>(&blend_mode, BLEND_MODE_ADD);
	actions.render_mode_values["blend_mix"] = Pair<int *, int>(&blend_mode, BLEND_MODE_MIX);
	actions.render_mode_values["blend_sub"] = Pair<int *, int>(&blend_mode, BLEND_MODE_SUB);
	actions.render_mode_values["blend_mul"] = Pair<int *, int>(&blend_mode, BLEND_MODE_MUL);
	actions.render_mode_values["blend_premul_alpha"] = Pair<int *, int>(&blend_mode, BLEND_MODE_PREMULTIPLIED_ALPHA);
	actions.render_mode_values["blend_disabled"] = Pair<int *, int>(&blend_mode, BLEND_MODE_DISABLED);

	actions.usage_flag_pointers["texture_sdf"] = &uses_sdf;
	actions.usage_flag_pointers["TIME"] = &uses_time;

	actions.uniforms = &uniforms;

	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);

	Error err = canvas_singleton->shader.compiler.compile(RS::SHADER_CANVAS_ITEM, code, &actions, path, gen_code);
	if (err != OK) {
		if (version.is_valid()) {
			canvas_singleton->shader.canvas_shader.version_free(version);
			version = RID();
		}
		ERR_FAIL_MSG("Shader compilation failed.");
	}

	uses_screen_texture_mipmaps = gen_code.uses_screen_texture_mipmaps;
	uses_screen_texture = gen_code.uses_screen_texture;

	pipeline_hash_map.clear_pipelines();

	if (version.is_null()) {
		version = canvas_singleton->shader.canvas_shader.version_create();
	}

#if 0
	print_line("**compiling shader:");
	print_line("**defines:\n");
	for (int i = 0; i < gen_code.defines.size(); i++) {
		print_line(gen_code.defines[i]);
	}

	HashMap<String, String>::Iterator el = gen_code.code.begin();
	while (el) {
		print_line("\n**code " + el->key + ":\n" + el->value);
		++el;
	}

	print_line("\n**uniforms:\n" + gen_code.uniforms);
	print_line("\n**vertex_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX]);
	print_line("\n**fragment_globals:\n" + gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT]);
#endif
	canvas_singleton->shader.canvas_shader.version_set_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_VERTEX], gen_code.stage_globals[ShaderCompiler::STAGE_FRAGMENT], gen_code.defines);

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;
}

bool RendererCanvasRenderRD::CanvasShaderData::is_animated() const {
	return false;
}

bool RendererCanvasRenderRD::CanvasShaderData::casts_shadows() const {
	return false;
}

RS::ShaderNativeSourceCode RendererCanvasRenderRD::CanvasShaderData::get_native_source_code() const {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	return canvas_singleton->shader.canvas_shader.version_get_native_source_code(version);
}

RID RendererCanvasRenderRD::CanvasShaderData::get_shader(ShaderVariant p_shader_variant, bool p_ubershader) const {
	if (version.is_valid()) {
		uint32_t variant_index = p_shader_variant + (p_ubershader ? SHADER_VARIANT_MAX : 0);
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		return canvas_singleton->shader.canvas_shader.version_get_shader(version, variant_index);
	} else {
		return RID();
	}
}

uint64_t RendererCanvasRenderRD::CanvasShaderData::get_vertex_input_mask(ShaderVariant p_shader_variant, bool p_ubershader) {
	// Vertex input masks require knowledge of the shader. Since querying the shader can be expensive due to high contention and the necessary mutex, we cache the result instead.
	uint32_t input_mask_index = p_shader_variant + (p_ubershader ? SHADER_VARIANT_MAX : 0);
	uint64_t input_mask = vertex_input_masks[input_mask_index].load(std::memory_order_relaxed);
	if (input_mask == 0) {
		RID shader_rid = get_shader(p_shader_variant, p_ubershader);
		ERR_FAIL_COND_V(shader_rid.is_null(), 0);

		input_mask = RD::get_singleton()->shader_get_vertex_input_attribute_mask(shader_rid);
		vertex_input_masks[input_mask_index].store(input_mask, std::memory_order_relaxed);
	}

	return input_mask;
}

bool RendererCanvasRenderRD::CanvasShaderData::is_valid() const {
	if (version.is_valid()) {
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		return canvas_singleton->shader.canvas_shader.version_is_valid(version);
	} else {
		return false;
	}
}

RendererCanvasRenderRD::CanvasShaderData::CanvasShaderData() {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	pipeline_hash_map.set_creation_object_and_function(this, &CanvasShaderData::_create_pipeline);
	pipeline_hash_map.set_compilations(&canvas_singleton->shader.pipeline_compilations[0], &canvas_singleton->shader.mutex);
}

RendererCanvasRenderRD::CanvasShaderData::~CanvasShaderData() {
	pipeline_hash_map.clear_pipelines();

	if (version.is_valid()) {
		RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
		MutexLock lock(canvas_singleton->shader.mutex);
		canvas_singleton->shader.canvas_shader.version_free(version);
	}
}

RendererRD::MaterialStorage::ShaderData *RendererCanvasRenderRD::_create_shader_func() {
	CanvasShaderData *shader_data = memnew(CanvasShaderData);
	return shader_data;
}

bool RendererCanvasRenderRD::CanvasMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	RID shader_to_update = canvas_singleton->shader.canvas_shader.version_get_shader(shader_data->version, 0);
	bool uniform_set_changed = update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, shader_to_update, MATERIAL_UNIFORM_SET, true, false);
	bool uniform_set_srgb_changed = update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set_srgb, shader_to_update, MATERIAL_UNIFORM_SET, false, false);
	return uniform_set_changed || uniform_set_srgb_changed;
}

RendererCanvasRenderRD::CanvasMaterialData::~CanvasMaterialData() {
	free_parameters_uniform_set(uniform_set);
	free_parameters_uniform_set(uniform_set_srgb);
}

RendererRD::MaterialStorage::MaterialData *RendererCanvasRenderRD::_create_material_func(CanvasShaderData *p_shader) {
	CanvasMaterialData *material_data = memnew(CanvasMaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}

void RendererCanvasRenderRD::set_time(double p_time) {
	state.time = p_time;
}

void RendererCanvasRenderRD::update() {
}

RendererCanvasRenderRD::RendererCanvasRenderRD() {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();

	{ //create default samplers

		default_samplers.default_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
		default_samplers.default_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
	}

	// preallocate 5 slots for uniform set 3
	state.batch_texture_uniforms.resize(5);

	{ //shader variants

		String global_defines;

		uint64_t uniform_max_size = RD::get_singleton()->limit_get(RD::LIMIT_MAX_UNIFORM_BUFFER_SIZE);
		if (uniform_max_size < 65536) {
			//Yes, you guessed right, ARM again
			state.max_lights_per_render = 64;
			global_defines += "#define MAX_LIGHTS 64\n";
		} else {
			state.max_lights_per_render = DEFAULT_MAX_LIGHTS_PER_RENDER;
			global_defines += "#define MAX_LIGHTS " + itos(DEFAULT_MAX_LIGHTS_PER_RENDER) + "\n";
		}

		global_defines += "\n#define SAMPLERS_BINDING_FIRST_INDEX " + itos(SAMPLERS_BINDING_FIRST_INDEX) + "\n";

		state.light_uniforms = memnew_arr(LightUniform, state.max_lights_per_render);
		Vector<String> variants;
		const uint32_t ubershader_iterations = 1;
		for (uint32_t ubershader = 0; ubershader < ubershader_iterations; ubershader++) {
			const String base_define = ubershader ? "\n#define UBERSHADER\n" : "";
			variants.push_back(base_define + ""); // SHADER_VARIANT_QUAD
			variants.push_back(base_define + "#define USE_NINEPATCH\n"); // SHADER_VARIANT_NINEPATCH
			variants.push_back(base_define + "#define USE_PRIMITIVE\n"); // SHADER_VARIANT_PRIMITIVE
			variants.push_back(base_define + "#define USE_PRIMITIVE\n#define USE_POINT_SIZE\n"); // SHADER_VARIANT_PRIMITIVE_POINTS
			variants.push_back(base_define + "#define USE_ATTRIBUTES\n"); // SHADER_VARIANT_ATTRIBUTES
			variants.push_back(base_define + "#define USE_ATTRIBUTES\n#define USE_POINT_SIZE\n"); // SHADER_VARIANT_ATTRIBUTES_POINTS
		}

		shader.canvas_shader.initialize(variants, global_defines);

		shader.default_version_data = memnew(CanvasShaderData);
		shader.default_version_data->version = shader.canvas_shader.version_create();
		shader.default_version_data->blend_mode = RendererRD::MaterialStorage::ShaderData::BLEND_MODE_MIX;
		shader.default_version_rd_shader = shader.default_version_data->get_shader(SHADER_VARIANT_QUAD, false);
	}

	{
		//shader compiler
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["VERTEX"] = "vertex";
		actions.renames["LIGHT_VERTEX"] = "light_vertex";
		actions.renames["SHADOW_VERTEX"] = "shadow_vertex";
		actions.renames["UV"] = "uv";
		actions.renames["POINT_SIZE"] = "point_size";

		actions.renames["MODEL_MATRIX"] = "model_matrix";
		actions.renames["CANVAS_MATRIX"] = "canvas_data.canvas_transform";
		actions.renames["SCREEN_MATRIX"] = "canvas_data.screen_transform";
		actions.renames["TIME"] = "canvas_data.time";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["AT_LIGHT_PASS"] = "false";
		actions.renames["INSTANCE_CUSTOM"] = "instance_custom";

		actions.renames["COLOR"] = "color";
		actions.renames["NORMAL"] = "normal";
		actions.renames["NORMAL_MAP"] = "normal_map";
		actions.renames["NORMAL_MAP_DEPTH"] = "normal_map_depth";
		actions.renames["TEXTURE"] = "color_texture";
		actions.renames["TEXTURE_PIXEL_SIZE"] = "draw_data.color_texture_pixel_size";
		actions.renames["NORMAL_TEXTURE"] = "normal_texture";
		actions.renames["SPECULAR_SHININESS_TEXTURE"] = "specular_texture";
		actions.renames["SPECULAR_SHININESS"] = "specular_shininess";
		actions.renames["SCREEN_UV"] = "screen_uv";
		actions.renames["SCREEN_PIXEL_SIZE"] = "canvas_data.screen_pixel_size";
		actions.renames["FRAGCOORD"] = "gl_FragCoord";
		actions.renames["POINT_COORD"] = "gl_PointCoord";
		actions.renames["INSTANCE_ID"] = "gl_InstanceIndex";
		actions.renames["VERTEX_ID"] = "gl_VertexIndex";

		actions.renames["CUSTOM0"] = "custom0";
		actions.renames["CUSTOM1"] = "custom1";

		actions.renames["LIGHT_POSITION"] = "light_position";
		actions.renames["LIGHT_DIRECTION"] = "light_direction";
		actions.renames["LIGHT_IS_DIRECTIONAL"] = "is_directional";
		actions.renames["LIGHT_COLOR"] = "light_color";
		actions.renames["LIGHT_ENERGY"] = "light_energy";
		actions.renames["LIGHT"] = "light";
		actions.renames["SHADOW_MODULATE"] = "shadow_modulate";

		actions.renames["texture_sdf"] = "texture_sdf";
		actions.renames["texture_sdf_normal"] = "texture_sdf_normal";
		actions.renames["sdf_to_screen_uv"] = "sdf_to_screen_uv";
		actions.renames["screen_uv_to_sdf"] = "screen_uv_to_sdf";

		actions.usage_defines["COLOR"] = "#define COLOR_USED\n";
		actions.usage_defines["SCREEN_UV"] = "#define SCREEN_UV_USED\n";
		actions.usage_defines["SCREEN_PIXEL_SIZE"] = "@SCREEN_UV";
		actions.usage_defines["NORMAL"] = "#define NORMAL_USED\n";
		actions.usage_defines["NORMAL_MAP"] = "#define NORMAL_MAP_USED\n";
		actions.usage_defines["SPECULAR_SHININESS"] = "#define SPECULAR_SHININESS_USED\n";
		actions.usage_defines["POINT_SIZE"] = "#define USE_POINT_SIZE\n";
		actions.usage_defines["CUSTOM0"] = "#define CUSTOM0_USED\n";
		actions.usage_defines["CUSTOM1"] = "#define CUSTOM1_USED\n";

		actions.render_mode_defines["skip_vertex_transform"] = "#define SKIP_TRANSFORM_USED\n";
		actions.render_mode_defines["unshaded"] = "#define MODE_UNSHADED\n";
		actions.render_mode_defines["light_only"] = "#define MODE_LIGHT_ONLY\n";
		actions.render_mode_defines["world_vertex_coords"] = "#define USE_WORLD_VERTEX_COORDS\n";

		actions.custom_samplers["TEXTURE"] = "texture_sampler";
		actions.custom_samplers["NORMAL_TEXTURE"] = "texture_sampler";
		actions.custom_samplers["SPECULAR_SHININESS_TEXTURE"] = "texture_sampler";
		actions.base_texture_binding_index = 1;
		actions.texture_layout_set = MATERIAL_UNIFORM_SET;
		actions.base_uniform_string = "material.";
		actions.default_filter = ShaderLanguage::FILTER_LINEAR;
		actions.default_repeat = ShaderLanguage::REPEAT_DISABLE;
		actions.base_varying_index = 5;

		actions.global_buffer_array_variable = "global_shader_uniforms.data";
		actions.instance_uniform_index_variable = "draw_data.instance_uniforms_ofs";

		shader.compiler.initialize(actions);
	}

	{ //shadow rendering
		Vector<String> versions;
		versions.push_back("\n#define MODE_SHADOW\n"); // Shadow.
		versions.push_back("\n#define MODE_SHADOW\n#define POSITIONAL_SHADOW\n"); // Positional shadow.
		versions.push_back("\n#define MODE_SDF\n"); // SDF.
		shadow_render.shader.initialize(versions);

		{
			Vector<RD::AttachmentFormat> attachments;

			RD::AttachmentFormat af_color;
			af_color.format = RD::DATA_FORMAT_R32_SFLOAT;
			af_color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

			attachments.push_back(af_color);

			RD::AttachmentFormat af_depth;
			af_depth.format = RD::DATA_FORMAT_D32_SFLOAT;
			af_depth.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			attachments.push_back(af_depth);

			shadow_render.framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);
		}

		{
			Vector<RD::AttachmentFormat> attachments;

			RD::AttachmentFormat af_color;
			af_color.format = RD::DATA_FORMAT_R8_UNORM;
			af_color.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;

			attachments.push_back(af_color);

			shadow_render.sdf_framebuffer_format = RD::get_singleton()->framebuffer_format_create(attachments);
		}

		//pipelines
		Vector<RD::VertexAttribute> vf;
		RD::VertexAttribute vd;
		vd.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
		vd.stride = sizeof(float) * 3;
		vd.location = 0;
		vd.offset = 0;
		vf.push_back(vd);
		shadow_render.vertex_format = RD::get_singleton()->vertex_format_create(vf);

		vd.format = RD::DATA_FORMAT_R32G32_SFLOAT;
		vd.stride = sizeof(float) * 2;

		vf.write[0] = vd;
		shadow_render.sdf_vertex_format = RD::get_singleton()->vertex_format_create(vf);

		shadow_render.shader_version = shadow_render.shader.version_create();

		for (int i = 0; i < 2; i++) {
			RD::PipelineRasterizationState rs;
			RD::PipelineDepthStencilState ds;
			ds.enable_depth_write = true;
			ds.enable_depth_test = true;
			ds.depth_compare_operator = RD::COMPARE_OP_LESS;
			shadow_render.render_pipelines[i] = RD::get_singleton()->render_pipeline_create(shadow_render.shader.version_get_shader(shadow_render.shader_version, ShadowRenderMode(i)), shadow_render.framebuffer_format, shadow_render.vertex_format, RD::RENDER_PRIMITIVE_TRIANGLES, rs, RD::PipelineMultisampleState(), ds, RD::PipelineColorBlendState::create_disabled(), 0);
		}

		for (int i = 0; i < 2; i++) {
			shadow_render.sdf_render_pipelines[i] = RD::get_singleton()->render_pipeline_create(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_SDF), shadow_render.sdf_framebuffer_format, shadow_render.sdf_vertex_format, i == 0 ? RD::RENDER_PRIMITIVE_TRIANGLES : RD::RENDER_PRIMITIVE_LINES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), RD::PipelineColorBlendState::create_disabled(), 0);
		}

		// Unload shader modules to save memory.
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_DIRECTIONAL_SHADOW));
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_POSITIONAL_SHADOW));
		RD::get_singleton()->shader_destroy_modules(shadow_render.shader.version_get_shader(shadow_render.shader_version, SHADOW_RENDER_MODE_SDF));
	}

	{ //bindings

		state.canvas_state_buffer = RD::get_singleton()->uniform_buffer_create(sizeof(State::Buffer));
		state.lights_uniform_buffer = RD::get_singleton()->uniform_buffer_create(sizeof(LightUniform) * state.max_lights_per_render);

		RD::SamplerState shadow_sampler_state;
		shadow_sampler_state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
		shadow_sampler_state.min_filter = RD::SAMPLER_FILTER_LINEAR;
		shadow_sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_REPEAT; //shadow wrap around
		shadow_sampler_state.compare_op = RD::COMPARE_OP_GREATER;
		shadow_sampler_state.enable_compare = true;
		state.shadow_sampler = RD::get_singleton()->sampler_create(shadow_sampler_state);
	}

	{
		//polygon buffers
		polygon_buffers.last_id = 1;
	}

	{ // default index buffer

		Vector<uint8_t> pv;
		pv.resize(6 * 2);
		{
			uint8_t *w = pv.ptrw();
			uint16_t *p16 = (uint16_t *)w;
			p16[0] = 0;
			p16[1] = 1;
			p16[2] = 2;
			p16[3] = 0;
			p16[4] = 2;
			p16[5] = 3;
		}
		shader.quad_index_buffer = RD::get_singleton()->index_buffer_create(6, RenderingDevice::INDEX_BUFFER_FORMAT_UINT16, pv);
		shader.quad_index_array = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 6);
	}

	{ //primitive
		primitive_arrays.index_array[0] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 1);
		primitive_arrays.index_array[1] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 2);
		primitive_arrays.index_array[2] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 3);
		primitive_arrays.index_array[3] = RD::get_singleton()->index_array_create(shader.quad_index_buffer, 0, 6);
	}

	{
		//default shadow texture to keep uniform set happy
		RD::TextureFormat tf;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.width = 4;
		tf.height = 4;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;

		state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
	}

	{
		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(RendererRD::MeshStorage::get_singleton()->get_default_rd_storage_buffer());
			uniforms.push_back(u);
		}

		state.default_transforms_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
	}

	default_canvas_texture = texture_storage->canvas_texture_allocate();
	texture_storage->canvas_texture_initialize(default_canvas_texture);

	RendererRD::TextureStorage::CanvasTextureInfo info = RendererRD::TextureStorage::get_singleton()->canvas_texture_get_info(default_canvas_texture, default_filter, default_repeat, false, false);
	default_texture_info.diffuse = info.diffuse;
	default_texture_info.normal = info.normal;
	default_texture_info.specular = info.specular;
	default_texture_info.sampler = info.sampler;

	state.shadow_texture_size = GLOBAL_GET("rendering/2d/shadow_atlas/size");

	//create functions for shader and material
	material_storage->shader_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_2D, _create_shader_funcs);
	material_storage->material_set_data_request_function(RendererRD::MaterialStorage::SHADER_TYPE_2D, _create_material_funcs);

	state.time = 0;

	{
		default_canvas_group_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(default_canvas_group_shader);

		material_storage->shader_set_code(default_canvas_group_shader, R"(
// Default CanvasGroup shader.

shader_type canvas_item;
render_mode unshaded;

uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_nearest;

void fragment() {
	vec4 c = textureLod(screen_texture, SCREEN_UV, 0.0);

	if (c.a > 0.0001) {
		c.rgb /= c.a;
	}

	COLOR *= c;
}
)");
		default_canvas_group_material = material_storage->material_allocate();
		material_storage->material_initialize(default_canvas_group_material);

		material_storage->material_set_shader(default_canvas_group_material, default_canvas_group_shader);
	}

	{
		default_clip_children_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(default_clip_children_shader);

		material_storage->shader_set_code(default_clip_children_shader, R"(
// Default clip children shader.

shader_type canvas_item;
render_mode unshaded;

uniform sampler2D screen_texture : hint_screen_texture, repeat_disable, filter_nearest;

void fragment() {
	vec4 c = textureLod(screen_texture, SCREEN_UV, 0.0);
	COLOR.rgb = c.rgb;
}
)");
		default_clip_children_material = material_storage->material_allocate();
		material_storage->material_initialize(default_clip_children_material);

		material_storage->material_set_shader(default_clip_children_material, default_clip_children_shader);
	}

	{
		uint32_t cache_size = uint32_t(GLOBAL_GET("rendering/2d/batching/uniform_set_cache_size"));
		rid_set_to_uniform_set.set_capacity(cache_size);
	}

	{
		state.max_instances_per_buffer = uint32_t(GLOBAL_GET("rendering/2d/batching/item_buffer_size"));
		state.max_instance_buffer_size = state.max_instances_per_buffer * sizeof(InstanceData);
		state.canvas_instance_batches.reserve(200);

		for (uint32_t i = 0; i < BATCH_DATA_BUFFER_COUNT; i++) {
			DataBuffer &db = state.canvas_instance_data_buffers[i];
			db.instance_buffers.push_back(RD::get_singleton()->storage_buffer_create(state.max_instance_buffer_size));
		}
		state.instance_data_array = memnew_arr(InstanceData, state.max_instances_per_buffer);
	}
}

bool RendererCanvasRenderRD::free(RID p_rid) {
	if (canvas_light_owner.owns(p_rid)) {
		CanvasLight *cl = canvas_light_owner.get_or_null(p_rid);
		ERR_FAIL_NULL_V(cl, false);
		light_set_use_shadow(p_rid, false);
		canvas_light_owner.free(p_rid);
	} else if (occluder_polygon_owner.owns(p_rid)) {
		occluder_polygon_set_shape(p_rid, Vector<Vector2>(), false);
		occluder_polygon_owner.free(p_rid);
	} else {
		return false;
	}

	return true;
}

void RendererCanvasRenderRD::set_shadow_texture_size(int p_size) {
	p_size = MAX(1, nearest_power_of_2_templated(p_size));
	if (p_size == state.shadow_texture_size) {
		return;
	}
	state.shadow_texture_size = p_size;
	if (state.shadow_fb.is_valid()) {
		RD::get_singleton()->free(state.shadow_texture);
		RD::get_singleton()->free(state.shadow_depth_texture);
		state.shadow_fb = RID();

		{
			//create a default shadow texture to keep uniform set happy (and that it gets erased when a new one is created)
			RD::TextureFormat tf;
			tf.texture_type = RD::TEXTURE_TYPE_2D;
			tf.width = 4;
			tf.height = 4;
			tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
			tf.format = RD::DATA_FORMAT_R32_SFLOAT;

			state.shadow_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());
		}
	}
}

void RendererCanvasRenderRD::set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) {
	debug_redraw = p_enabled;
	debug_redraw_time = p_time;
	debug_redraw_color = p_color;
}

uint32_t RendererCanvasRenderRD::get_pipeline_compilations(RS::PipelineSource p_source) {
	RendererCanvasRenderRD *canvas_singleton = static_cast<RendererCanvasRenderRD *>(RendererCanvasRender::singleton);
	MutexLock lock(canvas_singleton->shader.mutex);
	return shader.pipeline_compilations[p_source];
}

void RendererCanvasRenderRD::_render_batch_items(RenderTarget p_to_render_target, int p_item_count, const Transform2D &p_canvas_transform_inverse, Light *p_lights, bool &r_sdf_used, bool p_to_backbuffer, RenderingMethod::RenderInfo *r_render_info) {
	// Record batches
	uint32_t instance_index = 0;
	{
		RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
		Item *current_clip = nullptr;

		// Record Batches.
		// First item always forms its own batch.
		bool batch_broken = false;
		Batch *current_batch = _new_batch(batch_broken);
		// Override the start position and index as we want to start from where we finished off last time.
		current_batch->start = state.last_instance_index;

		for (int i = 0; i < p_item_count; i++) {
			Item *ci = items[i];

			if (ci->final_clip_owner != current_batch->clip) {
				current_batch = _new_batch(batch_broken);
				current_batch->clip = ci->final_clip_owner;
				current_clip = ci->final_clip_owner;
			}

			RID material = ci->material_owner == nullptr ? ci->material : ci->material_owner->material;

			if (ci->use_canvas_group) {
				if (ci->canvas_group->mode == RS::CANVAS_GROUP_MODE_CLIP_AND_DRAW) {
					material = default_clip_children_material;
				} else {
					if (material.is_null()) {
						if (ci->canvas_group->mode == RS::CANVAS_GROUP_MODE_CLIP_ONLY) {
							material = default_clip_children_material;
						} else {
							material = default_canvas_group_material;
						}
					}
				}
			}

			if (material != current_batch->material) {
				current_batch = _new_batch(batch_broken);

				CanvasMaterialData *material_data = nullptr;
				if (material.is_valid()) {
					material_data = static_cast<CanvasMaterialData *>(material_storage->material_get_data(material, RendererRD::MaterialStorage::SHADER_TYPE_2D));
				}

				current_batch->material = material;
				current_batch->material_data = material_data;
			}

			if (ci->repeat_source_item == nullptr || ci->repeat_size == Vector2()) {
				Transform2D base_transform = p_canvas_transform_inverse * ci->final_transform;
				_record_item_commands(ci, p_to_render_target, base_transform, current_clip, p_lights, instance_index, batch_broken, r_sdf_used, current_batch);
			} else {
				Point2 start_pos = ci->repeat_size * -(ci->repeat_times / 2);
				Point2 offset;
				int repeat_times_x = ci->repeat_size.x ? ci->repeat_times : 0;
				int repeat_times_y = ci->repeat_size.y ? ci->repeat_times : 0;
				for (int ry = 0; ry <= repeat_times_y; ry++) {
					offset.y = start_pos.y + ry * ci->repeat_size.y;
					for (int rx = 0; rx <= repeat_times_x; rx++) {
						offset.x = start_pos.x + rx * ci->repeat_size.x;
						Transform2D base_transform = ci->final_transform;
						base_transform.columns[2] += ci->repeat_source_item->final_transform.basis_xform(offset);
						base_transform = p_canvas_transform_inverse * base_transform;
						_record_item_commands(ci, p_to_render_target, base_transform, current_clip, p_lights, instance_index, batch_broken, r_sdf_used, current_batch);
					}
				}
			}
		}

		// Copy over remaining data needed for rendering.
		if (instance_index > 0) {
			RD::get_singleton()->buffer_update(
					state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers[state.current_instance_buffer_index],
					state.last_instance_index * sizeof(InstanceData),
					instance_index * sizeof(InstanceData),
					state.instance_data_array);
		}
	}

	if (state.canvas_instance_batches.is_empty()) {
		// Nothing to render, just return.
		return;
	}

	// Render batches

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	RID framebuffer;
	RID fb_uniform_set;
	bool clear = false;
	Vector<Color> clear_colors;

	if (p_to_backbuffer) {
		framebuffer = texture_storage->render_target_get_rd_backbuffer_framebuffer(p_to_render_target.render_target);
		fb_uniform_set = texture_storage->render_target_get_backbuffer_uniform_set(p_to_render_target.render_target);
	} else {
		framebuffer = texture_storage->render_target_get_rd_framebuffer(p_to_render_target.render_target);
		texture_storage->render_target_set_msaa_needs_resolve(p_to_render_target.render_target, false); // If MSAA is enabled, our framebuffer will be resolved!

		if (texture_storage->render_target_is_clear_requested(p_to_render_target.render_target)) {
			clear = true;
			clear_colors.push_back(texture_storage->render_target_get_clear_request_color(p_to_render_target.render_target));
			texture_storage->render_target_disable_clear_request(p_to_render_target.render_target);
		}
		// TODO: Obtain from framebuffer format eventually when this is implemented.
		fb_uniform_set = texture_storage->render_target_get_framebuffer_uniform_set(p_to_render_target.render_target);
	}

	if (fb_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(fb_uniform_set)) {
		fb_uniform_set = _create_base_uniform_set(p_to_render_target.render_target, p_to_backbuffer);
	}

	RD::FramebufferFormatID fb_format = RD::get_singleton()->framebuffer_get_format(framebuffer);

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer, clear ? RD::DRAW_CLEAR_COLOR_0 : RD::DRAW_DEFAULT_ALL, clear_colors, 1.0f, 0, Rect2(), RDD::BreadcrumbMarker::UI_PASS);

	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, fb_uniform_set, BASE_UNIFORM_SET);
	RD::get_singleton()->draw_list_bind_uniform_set(draw_list, state.default_transforms_uniform_set, TRANSFORMS_UNIFORM_SET);

	Item *current_clip = nullptr;
	state.current_batch_uniform_set = RID();

	for (uint32_t i = 0; i <= state.current_batch_index; i++) {
		Batch *current_batch = &state.canvas_instance_batches[i];
		// Skipping when there is no instances.
		if (current_batch->instance_count == 0) {
			continue;
		}

		//setup clip
		if (current_clip != current_batch->clip) {
			current_clip = current_batch->clip;
			if (current_clip) {
				RD::get_singleton()->draw_list_enable_scissor(draw_list, current_clip->final_clip_rect);
			} else {
				RD::get_singleton()->draw_list_disable_scissor(draw_list);
			}
		}

		CanvasShaderData *shader_data = shader.default_version_data;
		CanvasMaterialData *material_data = current_batch->material_data;
		if (material_data) {
			if (material_data->shader_data->version.is_valid() && material_data->shader_data->is_valid()) {
				shader_data = material_data->shader_data;
				// Update uniform set.
				RID uniform_set = texture_storage->render_target_is_using_hdr(p_to_render_target.render_target) ? material_data->uniform_set : material_data->uniform_set_srgb;
				if (uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(uniform_set)) { // Material may not have a uniform set.
					RD::get_singleton()->draw_list_bind_uniform_set(draw_list, uniform_set, MATERIAL_UNIFORM_SET);
					material_data->set_as_used();
				}
			}
		}

		_render_batch(draw_list, shader_data, fb_format, p_lights, current_batch, r_render_info);
	}

	RD::get_singleton()->draw_list_end();

	state.last_instance_index += instance_index;
}

RendererCanvasRenderRD::InstanceData *RendererCanvasRenderRD::new_instance_data(float *p_world, uint32_t *p_lights, uint32_t p_base_flags, uint32_t p_index, uint32_t p_uniforms_ofs, TextureInfo *p_info) {
	InstanceData *instance_data = &state.instance_data_array[p_index];
	// Zero out most fields.
	for (int i = 0; i < 4; i++) {
		instance_data->modulation[i] = 0.0;
		instance_data->ninepatch_margins[i] = 0.0;
		instance_data->src_rect[i] = 0.0;
		instance_data->dst_rect[i] = 0.0;
	}

	instance_data->pad[0] = 0.0;
	instance_data->pad[1] = 0.0;

	instance_data->lights[0] = p_lights[0];
	instance_data->lights[1] = p_lights[1];
	instance_data->lights[2] = p_lights[2];
	instance_data->lights[3] = p_lights[3];

	for (int i = 0; i < 6; i++) {
		instance_data->world[i] = p_world[i];
	}

	instance_data->flags = p_base_flags; // Reset on each command for safety.

	instance_data->color_texture_pixel_size[0] = p_info->texpixel_size.width;
	instance_data->color_texture_pixel_size[1] = p_info->texpixel_size.height;

	instance_data->instance_uniforms_ofs = p_uniforms_ofs;

	return instance_data;
}

void RendererCanvasRenderRD::_record_item_commands(const Item *p_item, RenderTarget p_render_target, const Transform2D &p_base_transform, Item *&r_current_clip, Light *p_lights, uint32_t &r_index, bool &r_batch_broken, bool &r_sdf_used, Batch *&r_current_batch) {
	const RenderingServer::CanvasItemTextureFilter texture_filter = p_item->texture_filter == RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT ? default_filter : p_item->texture_filter;
	const RenderingServer::CanvasItemTextureRepeat texture_repeat = p_item->texture_repeat == RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT ? default_repeat : p_item->texture_repeat;

	Transform2D base_transform = p_base_transform;

	float world[6];
	Transform2D draw_transform; // Used by transform command
	_update_transform_2d_to_mat2x3(base_transform, world);

	Color base_color = p_item->final_modulate;
	bool use_linear_colors = p_render_target.use_linear_colors;
	uint32_t base_flags = 0;
	uint32_t uniforms_ofs = static_cast<uint32_t>(p_item->instance_allocated_shader_uniforms_offset);

	bool reclip = false;

	bool skipping = false;

	// TODO: consider making lights a per-batch property and then baking light operations in the shader for better performance.
	uint32_t lights[4] = { 0, 0, 0, 0 };

	uint16_t light_count = 0;
	uint16_t shadow_mask = 0;

	{
		Light *light = p_lights;

		while (light) {
			if (light->render_index_cache >= 0 && p_item->light_mask & light->item_mask && p_item->z_final >= light->z_min && p_item->z_final <= light->z_max && p_item->global_rect_cache.intersects(light->rect_cache)) {
				uint32_t light_index = light->render_index_cache;
				lights[light_count >> 2] |= light_index << ((light_count & 3) * 8);

				if (p_item->light_mask & light->item_shadow_mask) {
					shadow_mask |= 1 << light_count;
				}

				light_count++;

				if (light_count == MAX_LIGHTS_PER_ITEM - 1) {
					break;
				}
			}
			light = light->next_ptr;
		}

		base_flags |= light_count << INSTANCE_FLAGS_LIGHT_COUNT_SHIFT;
		base_flags |= shadow_mask << INSTANCE_FLAGS_SHADOW_MASKED_SHIFT;
	}

	bool use_lighting = (light_count > 0 || using_directional_lights);

	if (use_lighting != r_current_batch->use_lighting) {
		r_current_batch = _new_batch(r_batch_broken);
		r_current_batch->use_lighting = use_lighting;
	}

	const Item::Command *c = p_item->commands;
	while (c) {
		if (skipping && c->type != Item::Command::TYPE_ANIMATION_SLICE) {
			c = c->next;
			continue;
		}

		switch (c->type) {
			case Item::Command::TYPE_RECT: {
				const Item::CommandRect *rect = static_cast<const Item::CommandRect *>(c);

				// 1: If commands are different, start a new batch.
				if (r_current_batch->command_type != Item::Command::TYPE_RECT) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_RECT;
					r_current_batch->command = c;
					// default variant
					r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
					r_current_batch->flags = 0;
				}

				RenderingServer::CanvasItemTextureRepeat rect_repeat = texture_repeat;
				if (bool(rect->flags & CANVAS_RECT_TILE)) {
					rect_repeat = RenderingServer::CanvasItemTextureRepeat::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
				}

				Color modulated = rect->modulate * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				bool has_blend = bool(rect->flags & CANVAS_RECT_LCD);
				// Start a new batch if the blend mode has changed,
				// or blend mode is enabled and the modulation has changed.
				if (has_blend != r_current_batch->has_blend || (has_blend && modulated != r_current_batch->modulate)) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->has_blend = has_blend;
					r_current_batch->modulate = modulated;
					r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
				}

				bool has_msdf = bool(rect->flags & CANVAS_RECT_MSDF);
				TextureState tex_state(rect->texture, texture_filter, rect_repeat, has_msdf, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(rect->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);
				Rect2 src_rect;
				Rect2 dst_rect;

				if (rect->texture.is_valid()) {
					src_rect = (rect->flags & CANVAS_RECT_REGION) ? Rect2(rect->source.position * tex_info->texpixel_size, rect->source.size * tex_info->texpixel_size) : Rect2(0, 0, 1, 1);
					dst_rect = Rect2(rect->rect.position, rect->rect.size);

					if (dst_rect.size.width < 0) {
						dst_rect.position.x += dst_rect.size.width;
						dst_rect.size.width *= -1;
					}
					if (dst_rect.size.height < 0) {
						dst_rect.position.y += dst_rect.size.height;
						dst_rect.size.height *= -1;
					}

					if (rect->flags & CANVAS_RECT_FLIP_H) {
						src_rect.size.x *= -1;
					}

					if (rect->flags & CANVAS_RECT_FLIP_V) {
						src_rect.size.y *= -1;
					}

					if (rect->flags & CANVAS_RECT_TRANSPOSE) {
						instance_data->flags |= INSTANCE_FLAGS_TRANSPOSE_RECT;
					}

					if (rect->flags & CANVAS_RECT_CLIP_UV) {
						instance_data->flags |= INSTANCE_FLAGS_CLIP_RECT_UV;
					}

				} else {
					dst_rect = Rect2(rect->rect.position, rect->rect.size);

					if (dst_rect.size.width < 0) {
						dst_rect.position.x += dst_rect.size.width;
						dst_rect.size.width *= -1;
					}
					if (dst_rect.size.height < 0) {
						dst_rect.position.y += dst_rect.size.height;
						dst_rect.size.height *= -1;
					}

					src_rect = Rect2(0, 0, 1, 1);
				}

				if (has_msdf) {
					instance_data->flags |= INSTANCE_FLAGS_USE_MSDF;
					instance_data->msdf[0] = rect->px_range; // Pixel range.
					instance_data->msdf[1] = rect->outline; // Outline size.
					instance_data->msdf[2] = 0.f; // Reserved.
					instance_data->msdf[3] = 0.f; // Reserved.
				} else if (rect->flags & CANVAS_RECT_LCD) {
					instance_data->flags |= INSTANCE_FLAGS_USE_LCD;
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;

				instance_data->src_rect[0] = src_rect.position.x;
				instance_data->src_rect[1] = src_rect.position.y;
				instance_data->src_rect[2] = src_rect.size.width;
				instance_data->src_rect[3] = src_rect.size.height;

				instance_data->dst_rect[0] = dst_rect.position.x;
				instance_data->dst_rect[1] = dst_rect.position.y;
				instance_data->dst_rect[2] = dst_rect.size.width;
				instance_data->dst_rect[3] = dst_rect.size.height;

				_add_to_batch(r_index, r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_NINEPATCH: {
				const Item::CommandNinePatch *np = static_cast<const Item::CommandNinePatch *>(c);

				if (r_current_batch->command_type != Item::Command::TYPE_NINEPATCH) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_NINEPATCH;
					r_current_batch->command = c;
					r_current_batch->has_blend = false;
					r_current_batch->shader_variant = SHADER_VARIANT_NINEPATCH;
					r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
					r_current_batch->flags = 0;
				}

				TextureState tex_state(np->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(np->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

				Rect2 src_rect;
				Rect2 dst_rect(np->rect.position.x, np->rect.position.y, np->rect.size.x, np->rect.size.y);

				if (np->texture.is_null()) {
					src_rect = Rect2(0, 0, 1, 1);
				} else {
					if (np->source != Rect2()) {
						src_rect = Rect2(np->source.position.x * tex_info->texpixel_size.width, np->source.position.y * tex_info->texpixel_size.height, np->source.size.x * tex_info->texpixel_size.width, np->source.size.y * tex_info->texpixel_size.height);
						instance_data->color_texture_pixel_size[0] = 1.0 / np->source.size.width;
						instance_data->color_texture_pixel_size[1] = 1.0 / np->source.size.height;
					} else {
						src_rect = Rect2(0, 0, 1, 1);
					}
				}

				Color modulated = np->color * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;

				instance_data->src_rect[0] = src_rect.position.x;
				instance_data->src_rect[1] = src_rect.position.y;
				instance_data->src_rect[2] = src_rect.size.width;
				instance_data->src_rect[3] = src_rect.size.height;

				instance_data->dst_rect[0] = dst_rect.position.x;
				instance_data->dst_rect[1] = dst_rect.position.y;
				instance_data->dst_rect[2] = dst_rect.size.width;
				instance_data->dst_rect[3] = dst_rect.size.height;

				instance_data->flags |= int(np->axis_x) << INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT;
				instance_data->flags |= int(np->axis_y) << INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT;

				if (np->draw_center) {
					instance_data->flags |= INSTANCE_FLAGS_NINEPACH_DRAW_CENTER;
				}

				instance_data->ninepatch_margins[0] = np->margin[SIDE_LEFT];
				instance_data->ninepatch_margins[1] = np->margin[SIDE_TOP];
				instance_data->ninepatch_margins[2] = np->margin[SIDE_RIGHT];
				instance_data->ninepatch_margins[3] = np->margin[SIDE_BOTTOM];

				_add_to_batch(r_index, r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_POLYGON: {
				const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(c);

				// Polygon's can't be batched, so always create a new batch
				r_current_batch = _new_batch(r_batch_broken);

				r_current_batch->command_type = Item::Command::TYPE_POLYGON;
				r_current_batch->has_blend = false;
				r_current_batch->command = c;
				r_current_batch->flags = 0;

				TextureState tex_state(polygon->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(polygon->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				// pipeline variant
				{
					ERR_CONTINUE(polygon->primitive < 0 || polygon->primitive >= RS::PRIMITIVE_MAX);
					r_current_batch->shader_variant = polygon->primitive == RS::PRIMITIVE_POINTS ? SHADER_VARIANT_ATTRIBUTES_POINTS : SHADER_VARIANT_ATTRIBUTES;
					r_current_batch->render_primitive = _primitive_type_to_render_primitive(polygon->primitive);
				}

				InstanceData *instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

				Color color = base_color;
				if (use_linear_colors) {
					color = color.srgb_to_linear();
				}

				instance_data->modulation[0] = color.r;
				instance_data->modulation[1] = color.g;
				instance_data->modulation[2] = color.b;
				instance_data->modulation[3] = color.a;

				_add_to_batch(r_index, r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_PRIMITIVE: {
				const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(c);

				if (primitive->point_count != r_current_batch->primitive_points || r_current_batch->command_type != Item::Command::TYPE_PRIMITIVE) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->command_type = Item::Command::TYPE_PRIMITIVE;
					r_current_batch->has_blend = false;
					r_current_batch->command = c;
					r_current_batch->primitive_points = primitive->point_count;
					r_current_batch->flags = 0;

					ERR_CONTINUE(primitive->point_count == 0 || primitive->point_count > 4);

					switch (primitive->point_count) {
						case 1:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE_POINTS;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_POINTS;
							break;
						case 2:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_LINES;
							break;
						case 3:
						case 4:
							r_current_batch->shader_variant = SHADER_VARIANT_PRIMITIVE;
							r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
							break;
						default:
							// Unknown point count.
							break;
					}
				}

				TextureState tex_state(primitive->texture, texture_filter, texture_repeat, false, use_linear_colors);
				TextureInfo *tex_info = texture_info_map.getptr(tex_state);
				if (!tex_info) {
					tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
					_prepare_batch_texture_info(primitive->texture, tex_state, tex_info);
				}

				if (r_current_batch->tex_info != tex_info) {
					r_current_batch = _new_batch(r_batch_broken);
					r_current_batch->tex_info = tex_info;
				}

				InstanceData *instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

				for (uint32_t j = 0; j < MIN(3u, primitive->point_count); j++) {
					instance_data->points[j * 2 + 0] = primitive->points[j].x;
					instance_data->points[j * 2 + 1] = primitive->points[j].y;
					instance_data->uvs[j * 2 + 0] = primitive->uvs[j].x;
					instance_data->uvs[j * 2 + 1] = primitive->uvs[j].y;
					Color col = primitive->colors[j] * base_color;
					if (use_linear_colors) {
						col = col.srgb_to_linear();
					}
					instance_data->colors[j * 2 + 0] = (uint32_t(Math::make_half_float(col.g)) << 16) | Math::make_half_float(col.r);
					instance_data->colors[j * 2 + 1] = (uint32_t(Math::make_half_float(col.a)) << 16) | Math::make_half_float(col.b);
				}

				_add_to_batch(r_index, r_batch_broken, r_current_batch);

				if (primitive->point_count == 4) {
					instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

					for (uint32_t j = 0; j < 3; j++) {
						int offset = j == 0 ? 0 : 1;
						// Second triangle in the quad. Uses vertices 0, 2, 3.
						instance_data->points[j * 2 + 0] = primitive->points[j + offset].x;
						instance_data->points[j * 2 + 1] = primitive->points[j + offset].y;
						instance_data->uvs[j * 2 + 0] = primitive->uvs[j + offset].x;
						instance_data->uvs[j * 2 + 1] = primitive->uvs[j + offset].y;
						Color col = primitive->colors[j + offset] * base_color;
						if (use_linear_colors) {
							col = col.srgb_to_linear();
						}
						instance_data->colors[j * 2 + 0] = (uint32_t(Math::make_half_float(col.g)) << 16) | Math::make_half_float(col.r);
						instance_data->colors[j * 2 + 1] = (uint32_t(Math::make_half_float(col.a)) << 16) | Math::make_half_float(col.b);
					}

					_add_to_batch(r_index, r_batch_broken, r_current_batch);
				}
			} break;

			case Item::Command::TYPE_MESH:
			case Item::Command::TYPE_MULTIMESH:
			case Item::Command::TYPE_PARTICLES: {
				// Mesh's can't be batched, so always create a new batch
				r_current_batch = _new_batch(r_batch_broken);
				r_current_batch->command = c;
				r_current_batch->command_type = c->type;
				r_current_batch->has_blend = false;
				r_current_batch->flags = 0;

				InstanceData *instance_data = nullptr;

				Color modulate(1, 1, 1, 1);
				if (c->type == Item::Command::TYPE_MESH) {
					const Item::CommandMesh *m = static_cast<const Item::CommandMesh *>(c);
					TextureState tex_state(m->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(m->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

					r_current_batch->mesh_instance_count = 1;
					_update_transform_2d_to_mat2x3(base_transform * draw_transform * m->transform, instance_data->world);
					modulate = m->modulate;
				} else if (c->type == Item::Command::TYPE_MULTIMESH) {
					RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();

					const Item::CommandMultiMesh *mm = static_cast<const Item::CommandMultiMesh *>(c);
					RID multimesh = mm->multimesh;

					if (mesh_storage->multimesh_get_transform_format(multimesh) != RS::MULTIMESH_TRANSFORM_2D) {
						break;
					}

					r_current_batch->mesh_instance_count = mesh_storage->multimesh_get_instances_to_draw(multimesh);
					if (r_current_batch->mesh_instance_count == 0) {
						break;
					}

					TextureState tex_state(mm->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(mm->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

					r_current_batch->flags |= 1; // multimesh, trails disabled

					if (mesh_storage->multimesh_uses_colors(mm->multimesh)) {
						r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_COLORS;
					}
					if (mesh_storage->multimesh_uses_custom_data(mm->multimesh)) {
						r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA;
					}
				} else if (c->type == Item::Command::TYPE_PARTICLES) {
					RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
					RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();

					const Item::CommandParticles *pt = static_cast<const Item::CommandParticles *>(c);
					TextureState tex_state(pt->texture, texture_filter, texture_repeat, false, use_linear_colors);
					TextureInfo *tex_info = texture_info_map.getptr(tex_state);
					if (!tex_info) {
						tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
						_prepare_batch_texture_info(pt->texture, tex_state, tex_info);
					}
					r_current_batch->tex_info = tex_info;
					instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

					uint32_t divisor = 1;
					r_current_batch->mesh_instance_count = particles_storage->particles_get_amount(pt->particles, divisor);
					r_current_batch->flags |= (divisor & BATCH_FLAGS_INSTANCING_MASK);
					r_current_batch->mesh_instance_count /= divisor;

					RID particles = pt->particles;

					r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_COLORS;
					r_current_batch->flags |= BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA;

					if (particles_storage->particles_has_collision(particles) && texture_storage->render_target_is_sdf_enabled(p_render_target.render_target)) {
						// Pass collision information.
						Transform2D xform = p_item->final_transform;

						RID sdf_texture = texture_storage->render_target_get_sdf_texture(p_render_target.render_target);

						Rect2 to_screen;
						{
							Rect2 sdf_rect = texture_storage->render_target_get_sdf_rect(p_render_target.render_target);

							to_screen.size = Vector2(1.0 / sdf_rect.size.width, 1.0 / sdf_rect.size.height);
							to_screen.position = -sdf_rect.position * to_screen.size;
						}

						particles_storage->particles_set_canvas_sdf_collision(pt->particles, true, xform, to_screen, sdf_texture);
					} else {
						particles_storage->particles_set_canvas_sdf_collision(pt->particles, false, Transform2D(), Rect2(), RID());
					}
					r_sdf_used |= particles_storage->particles_has_collision(particles);
				}

				Color modulated = modulate * base_color;
				if (use_linear_colors) {
					modulated = modulated.srgb_to_linear();
				}

				instance_data->modulation[0] = modulated.r;
				instance_data->modulation[1] = modulated.g;
				instance_data->modulation[2] = modulated.b;
				instance_data->modulation[3] = modulated.a;

				_add_to_batch(r_index, r_batch_broken, r_current_batch);
			} break;

			case Item::Command::TYPE_TRANSFORM: {
				const Item::CommandTransform *transform = static_cast<const Item::CommandTransform *>(c);
				draw_transform = transform->xform;
				_update_transform_2d_to_mat2x3(base_transform * transform->xform, world);
			} break;

			case Item::Command::TYPE_CLIP_IGNORE: {
				const Item::CommandClipIgnore *ci = static_cast<const Item::CommandClipIgnore *>(c);
				if (r_current_clip) {
					if (ci->ignore != reclip) {
						r_current_batch = _new_batch(r_batch_broken);
						if (ci->ignore) {
							r_current_batch->clip = nullptr;
							reclip = true;
						} else {
							r_current_batch->clip = r_current_clip;
							reclip = false;
						}
					}
				}
			} break;

			case Item::Command::TYPE_ANIMATION_SLICE: {
				const Item::CommandAnimationSlice *as = static_cast<const Item::CommandAnimationSlice *>(c);
				double current_time = RSG::rasterizer->get_total_time();
				double local_time = Math::fposmod(current_time - as->offset, as->animation_length);
				skipping = !(local_time >= as->slice_begin && local_time < as->slice_end);

				RenderingServerDefault::redraw_request(); // animation visible means redraw request
			} break;
		}

		c = c->next;
		r_batch_broken = false;
	}

#ifdef DEBUG_ENABLED
	if (debug_redraw && p_item->debug_redraw_time > 0.0) {
		Color dc = debug_redraw_color;
		dc.a *= p_item->debug_redraw_time / debug_redraw_time;

		// 1: If commands are different, start a new batch.
		if (r_current_batch->command_type != Item::Command::TYPE_RECT) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->command_type = Item::Command::TYPE_RECT;
			// it is ok to be null for a TYPE_RECT
			r_current_batch->command = nullptr;
			// default variant
			r_current_batch->shader_variant = SHADER_VARIANT_QUAD;
			r_current_batch->render_primitive = RD::RENDER_PRIMITIVE_TRIANGLES;
			r_current_batch->flags = 0;
		}

		// 2: If the current batch has lighting, start a new batch.
		if (r_current_batch->use_lighting) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->use_lighting = false;
		}

		// 3: If the current batch has blend, start a new batch.
		if (r_current_batch->has_blend) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->has_blend = false;
		}

		TextureState tex_state(default_canvas_texture, texture_filter, texture_repeat, false, use_linear_colors);
		TextureInfo *tex_info = texture_info_map.getptr(tex_state);
		if (!tex_info) {
			tex_info = &texture_info_map.insert(tex_state, TextureInfo())->value;
			_prepare_batch_texture_info(default_canvas_texture, tex_state, tex_info);
		}

		if (r_current_batch->tex_info != tex_info) {
			r_current_batch = _new_batch(r_batch_broken);
			r_current_batch->tex_info = tex_info;
		}

		InstanceData *instance_data = new_instance_data(world, lights, base_flags, r_index, uniforms_ofs, tex_info);

		Rect2 src_rect;
		Rect2 dst_rect;

		dst_rect = Rect2(Vector2(), p_item->rect.size);
		if (dst_rect.size.width < 0) {
			dst_rect.position.x += dst_rect.size.width;
			dst_rect.size.width *= -1;
		}
		if (dst_rect.size.height < 0) {
			dst_rect.position.y += dst_rect.size.height;
			dst_rect.size.height *= -1;
		}

		src_rect = Rect2(0, 0, 1, 1);

		instance_data->modulation[0] = dc.r;
		instance_data->modulation[1] = dc.g;
		instance_data->modulation[2] = dc.b;
		instance_data->modulation[3] = dc.a;

		instance_data->src_rect[0] = src_rect.position.x;
		instance_data->src_rect[1] = src_rect.position.y;
		instance_data->src_rect[2] = src_rect.size.width;
		instance_data->src_rect[3] = src_rect.size.height;

		instance_data->dst_rect[0] = dst_rect.position.x;
		instance_data->dst_rect[1] = dst_rect.position.y;
		instance_data->dst_rect[2] = dst_rect.size.width;
		instance_data->dst_rect[3] = dst_rect.size.height;

		_add_to_batch(r_index, r_batch_broken, r_current_batch);

		p_item->debug_redraw_time -= RSG::rasterizer->get_frame_delta_time();

		RenderingServerDefault::redraw_request();

		r_batch_broken = false;
	}
#endif

	if (r_current_clip && reclip) {
		// will make it re-enable clipping if needed afterwards
		r_current_clip = nullptr;
	}
}

void RendererCanvasRenderRD::_before_evict(RendererCanvasRenderRD::RIDSetKey &p_key, RID &p_rid) {
	RD::get_singleton()->uniform_set_set_invalidation_callback(p_rid, nullptr, nullptr);
	RD::get_singleton()->free(p_rid);
}

void RendererCanvasRenderRD::_uniform_set_invalidation_callback(void *p_userdata) {
	const RIDSetKey *key = static_cast<RIDSetKey *>(p_userdata);
	static_cast<RendererCanvasRenderRD *>(singleton)->rid_set_to_uniform_set.erase(*key);
}

void RendererCanvasRenderRD::_canvas_texture_invalidation_callback(bool p_deleted, void *p_userdata) {
	KeyValue<RID, TightLocalVector<RID>> *kv = static_cast<KeyValue<RID, TightLocalVector<RID>> *>(p_userdata);
	RD *rd = RD::get_singleton();
	for (RID rid : kv->value) {
		// The invalidation callback will also take care of clearing rid_set_to_uniform_set cache.
		rd->free(rid);
	}
	kv->value.clear();
	if (p_deleted) {
		static_cast<RendererCanvasRenderRD *>(singleton)->canvas_texture_to_uniform_set.erase(kv->key);
	}
}

void RendererCanvasRenderRD::_render_batch(RD::DrawListID p_draw_list, CanvasShaderData *p_shader_data, RenderingDevice::FramebufferFormatID p_framebuffer_format, Light *p_lights, Batch const *p_batch, RenderingMethod::RenderInfo *r_render_info) {
	{
		RendererRD::TextureStorage *ts = RendererRD::TextureStorage::get_singleton();

		RIDSetKey key(
				p_batch->tex_info->state,
				state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers[p_batch->instance_buffer_index]);

		const RID *uniform_set = rid_set_to_uniform_set.getptr(key);
		if (uniform_set == nullptr) {
			RD::Uniform *uniform_ptrw = state.batch_texture_uniforms.ptrw();
			uniform_ptrw[0] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 0, p_batch->tex_info->diffuse);
			uniform_ptrw[1] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 1, p_batch->tex_info->normal);
			uniform_ptrw[2] = RD::Uniform(RD::UNIFORM_TYPE_TEXTURE, 2, p_batch->tex_info->specular);
			uniform_ptrw[3] = RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, 3, p_batch->tex_info->sampler);
			uniform_ptrw[4] = RD::Uniform(RD::UNIFORM_TYPE_STORAGE_BUFFER, 4, state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers[p_batch->instance_buffer_index]);

			RID rid = RD::get_singleton()->uniform_set_create(state.batch_texture_uniforms, shader.default_version_rd_shader, BATCH_UNIFORM_SET);
			ERR_FAIL_COND_MSG(rid.is_null(), "Failed to create uniform set for batch.");

			const RIDCache::Pair *iter = rid_set_to_uniform_set.insert(key, rid);
			uniform_set = &iter->data;
			RD::get_singleton()->uniform_set_set_invalidation_callback(rid, RendererCanvasRenderRD::_uniform_set_invalidation_callback, (void *)&iter->key);

			// If this is a CanvasTexture, it must be tracked so that any changes to the diffuse, normal,
			// or specular channels invalidate all associated uniform sets.
			if (ts->owns_canvas_texture(p_batch->tex_info->state.texture)) {
				KeyValue<RID, TightLocalVector<RID>> *kv = nullptr;
				if (HashMap<RID, TightLocalVector<RID>>::Iterator i = canvas_texture_to_uniform_set.find(p_batch->tex_info->state.texture); i == canvas_texture_to_uniform_set.end()) {
					kv = &*canvas_texture_to_uniform_set.insert(p_batch->tex_info->state.texture, { *uniform_set });
				} else {
					i->value.push_back(rid);
					kv = &*i;
				}
				ts->canvas_texture_set_invalidation_callback(p_batch->tex_info->state.texture, RendererCanvasRenderRD::_canvas_texture_invalidation_callback, kv);
			}
		}

		if (state.current_batch_uniform_set != *uniform_set) {
			state.current_batch_uniform_set = *uniform_set;
			RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, *uniform_set, BATCH_UNIFORM_SET);
		}
	}
	PushConstant push_constant;
	push_constant.base_instance_index = p_batch->start;
	push_constant.specular_shininess = p_batch->tex_info->specular_shininess;
	push_constant.batch_flags = p_batch->tex_info->flags | p_batch->flags;

	RID pipeline;
	PipelineKey pipeline_key;
	pipeline_key.framebuffer_format_id = p_framebuffer_format;
	pipeline_key.variant = p_batch->shader_variant;
	pipeline_key.render_primitive = p_batch->render_primitive;
	pipeline_key.shader_specialization.use_lighting = p_batch->use_lighting;
	pipeline_key.lcd_blend = p_batch->has_blend;

	switch (p_batch->command_type) {
		case Item::Command::TYPE_RECT:
		case Item::Command::TYPE_NINEPATCH: {
			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);
			if (p_batch->has_blend) {
				RD::get_singleton()->draw_list_set_blend_constants(p_draw_list, p_batch->modulate);
			}

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(PushConstant));
			RD::get_singleton()->draw_list_bind_index_array(p_draw_list, shader.quad_index_array);
			RD::get_singleton()->draw_list_draw(p_draw_list, true, p_batch->instance_count);

			if (r_render_info) {
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += p_batch->instance_count;
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += 2 * p_batch->instance_count;
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_POLYGON: {
			ERR_FAIL_NULL(p_batch->command);

			const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(p_batch->command);

			PolygonBuffers *pb = polygon_buffers.polygons.getptr(polygon->polygon.polygon_id);
			ERR_FAIL_NULL(pb);

			pipeline_key.vertex_format_id = pb->vertex_format_id;
			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(PushConstant));
			RD::get_singleton()->draw_list_bind_vertex_array(p_draw_list, pb->vertex_array);
			if (pb->indices.is_valid()) {
				RD::get_singleton()->draw_list_bind_index_array(p_draw_list, pb->indices);
			}

			RD::get_singleton()->draw_list_draw(p_draw_list, pb->indices.is_valid());
			if (r_render_info) {
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME]++;
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(polygon->primitive, pb->primitive_count);
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_PRIMITIVE: {
			ERR_FAIL_NULL(p_batch->command);

			const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(p_batch->command);

			pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant);
			RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

			RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(PushConstant));
			RD::get_singleton()->draw_list_bind_index_array(p_draw_list, primitive_arrays.index_array[MIN(3u, primitive->point_count) - 1]);
			uint32_t instance_count = p_batch->instance_count;
			RD::get_singleton()->draw_list_draw(p_draw_list, true, instance_count);

			if (r_render_info) {
				const RenderingServer::PrimitiveType rs_primitive[5] = { RS::PRIMITIVE_POINTS, RS::PRIMITIVE_POINTS, RS::PRIMITIVE_LINES, RS::PRIMITIVE_TRIANGLES, RS::PRIMITIVE_TRIANGLES };
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += instance_count;
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(rs_primitive[p_batch->primitive_points], p_batch->primitive_points) * instance_count;
				r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
			}
		} break;

		case Item::Command::TYPE_MESH:
		case Item::Command::TYPE_MULTIMESH:
		case Item::Command::TYPE_PARTICLES: {
			ERR_FAIL_NULL(p_batch->command);

			RendererRD::MeshStorage *mesh_storage = RendererRD::MeshStorage::get_singleton();
			RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();

			RID mesh;
			RID mesh_instance;

			if (p_batch->command_type == Item::Command::TYPE_MESH) {
				const Item::CommandMesh *m = static_cast<const Item::CommandMesh *>(p_batch->command);
				mesh = m->mesh;
				mesh_instance = m->mesh_instance;
			} else if (p_batch->command_type == Item::Command::TYPE_MULTIMESH) {
				const Item::CommandMultiMesh *mm = static_cast<const Item::CommandMultiMesh *>(p_batch->command);
				RID multimesh = mm->multimesh;
				mesh = mesh_storage->multimesh_get_mesh(multimesh);

				RID uniform_set = mesh_storage->multimesh_get_2d_uniform_set(multimesh, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
				RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set, TRANSFORMS_UNIFORM_SET);
			} else if (p_batch->command_type == Item::Command::TYPE_PARTICLES) {
				const Item::CommandParticles *pt = static_cast<const Item::CommandParticles *>(p_batch->command);
				RID particles = pt->particles;
				mesh = particles_storage->particles_get_draw_pass_mesh(particles, 0);

				ERR_BREAK(particles_storage->particles_get_mode(particles) != RS::PARTICLES_MODE_2D);
				particles_storage->particles_request_process(particles);

				if (particles_storage->particles_is_inactive(particles)) {
					break;
				}

				RenderingServerDefault::redraw_request(); // Active particles means redraw request.

				int dpc = particles_storage->particles_get_draw_passes(particles);
				if (dpc == 0) {
					break; // Nothing to draw.
				}

				RID uniform_set = particles_storage->particles_get_instance_buffer_uniform_set(pt->particles, shader.default_version_rd_shader, TRANSFORMS_UNIFORM_SET);
				RD::get_singleton()->draw_list_bind_uniform_set(p_draw_list, uniform_set, TRANSFORMS_UNIFORM_SET);
			}

			if (mesh.is_null()) {
				break;
			}

			uint32_t surf_count = mesh_storage->mesh_get_surface_count(mesh);

			for (uint32_t j = 0; j < surf_count; j++) {
				void *surface = mesh_storage->mesh_get_surface(mesh, j);

				RS::PrimitiveType primitive = mesh_storage->mesh_surface_get_primitive(surface);
				ERR_CONTINUE(primitive < 0 || primitive >= RS::PRIMITIVE_MAX);

				RID vertex_array;
				pipeline_key.variant = primitive == RS::PRIMITIVE_POINTS ? SHADER_VARIANT_ATTRIBUTES_POINTS : SHADER_VARIANT_ATTRIBUTES;
				pipeline_key.render_primitive = _primitive_type_to_render_primitive(primitive);
				pipeline_key.vertex_format_id = RD::INVALID_FORMAT_ID;

				pipeline = _get_pipeline_specialization_or_ubershader(p_shader_data, pipeline_key, push_constant, mesh_instance, surface, j, &vertex_array);
				RD::get_singleton()->draw_list_bind_render_pipeline(p_draw_list, pipeline);

				RD::get_singleton()->draw_list_set_push_constant(p_draw_list, &push_constant, sizeof(PushConstant));

				RID index_array = mesh_storage->mesh_surface_get_index_array(surface, 0);

				if (index_array.is_valid()) {
					RD::get_singleton()->draw_list_bind_index_array(p_draw_list, index_array);
				}

				RD::get_singleton()->draw_list_bind_vertex_array(p_draw_list, vertex_array);
				RD::get_singleton()->draw_list_draw(p_draw_list, index_array.is_valid(), p_batch->mesh_instance_count);

				if (r_render_info) {
					r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME]++;
					r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += _indices_to_primitives(primitive, mesh_storage->mesh_surface_get_vertices_drawn_count(surface)) * p_batch->mesh_instance_count;
					r_render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
				}
			}
		} break;
		case Item::Command::TYPE_TRANSFORM:
		case Item::Command::TYPE_CLIP_IGNORE:
		case Item::Command::TYPE_ANIMATION_SLICE: {
			// Can ignore these as they only impact batch creation.
		} break;
	}
}

RendererCanvasRenderRD::Batch *RendererCanvasRenderRD::_new_batch(bool &r_batch_broken) {
	if (state.canvas_instance_batches.size() == 0) {
		state.canvas_instance_batches.push_back(Batch());
		return state.canvas_instance_batches.ptr();
	}

	if (r_batch_broken || state.canvas_instance_batches[state.current_batch_index].instance_count == 0) {
		return &state.canvas_instance_batches[state.current_batch_index];
	}

	r_batch_broken = true;

	// Copy the properties of the current batch, we will manually update the things that changed.
	Batch new_batch = state.canvas_instance_batches[state.current_batch_index];
	new_batch.instance_count = 0;
	new_batch.start = state.canvas_instance_batches[state.current_batch_index].start + state.canvas_instance_batches[state.current_batch_index].instance_count;
	new_batch.instance_buffer_index = state.current_instance_buffer_index;
	state.current_batch_index++;
	state.canvas_instance_batches.push_back(new_batch);
	return &state.canvas_instance_batches[state.current_batch_index];
}

void RendererCanvasRenderRD::_add_to_batch(uint32_t &r_index, bool &r_batch_broken, Batch *&r_current_batch) {
	r_current_batch->instance_count++;
	r_index++;
	if (r_index + state.last_instance_index >= state.max_instances_per_buffer) {
		// Copy over all data needed for rendering right away
		// then go back to recording item commands.
		RD::get_singleton()->buffer_update(
				state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers[state.current_instance_buffer_index],
				state.last_instance_index * sizeof(InstanceData),
				r_index * sizeof(InstanceData),
				state.instance_data_array);
		_allocate_instance_buffer();
		r_index = 0;
		state.last_instance_index = 0;
		r_batch_broken = false; // Force a new batch to be created
		r_current_batch = _new_batch(r_batch_broken);
		r_current_batch->start = 0;
	}
}

void RendererCanvasRenderRD::_allocate_instance_buffer() {
	state.current_instance_buffer_index++;

	if (state.current_instance_buffer_index < state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers.size()) {
		// We already allocated another buffer in a previous frame, so we can just use it.
		return;
	}

	// Allocate a new buffer.
	RID buf = RD::get_singleton()->storage_buffer_create(state.max_instance_buffer_size);
	state.canvas_instance_data_buffers[state.current_data_buffer_index].instance_buffers.push_back(buf);
}

void RendererCanvasRenderRD::_prepare_batch_texture_info(RID p_texture, TextureState &p_state, TextureInfo *p_info) {
	if (p_texture.is_null()) {
		p_texture = default_canvas_texture;
	}

	RendererRD::TextureStorage::CanvasTextureInfo info =
			RendererRD::TextureStorage::get_singleton()->canvas_texture_get_info(
					p_texture,
					p_state.texture_filter(),
					p_state.texture_repeat(),
					p_state.linear_colors(),
					p_state.texture_is_data());
	// something odd happened
	if (info.is_null()) {
		_prepare_batch_texture_info(default_canvas_texture, p_state, p_info);
		return;
	}

	p_info->state = p_state;
	p_info->diffuse = info.diffuse;
	p_info->normal = info.normal;
	p_info->specular = info.specular;
	p_info->sampler = info.sampler;

	// cache values to be copied to instance data
	if (info.specular_color.a < 0.999) {
		p_info->flags |= BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED;
	}

	if (info.use_normal) {
		p_info->flags |= BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED;
	}

	uint8_t a = uint8_t(CLAMP(info.specular_color.a * 255.0, 0.0, 255.0));
	uint8_t b = uint8_t(CLAMP(info.specular_color.b * 255.0, 0.0, 255.0));
	uint8_t g = uint8_t(CLAMP(info.specular_color.g * 255.0, 0.0, 255.0));
	uint8_t r = uint8_t(CLAMP(info.specular_color.r * 255.0, 0.0, 255.0));
	p_info->specular_shininess = uint32_t(a) << 24 | uint32_t(b) << 16 | uint32_t(g) << 8 | uint32_t(r);

	p_info->texpixel_size = Vector2(1.0 / float(info.size.width), 1.0 / float(info.size.height));
}

RendererCanvasRenderRD::~RendererCanvasRenderRD() {
	RendererRD::MaterialStorage *material_storage = RendererRD::MaterialStorage::get_singleton();
	//canvas state

	material_storage->material_free(default_canvas_group_material);
	material_storage->shader_free(default_canvas_group_shader);

	material_storage->material_free(default_clip_children_material);
	material_storage->shader_free(default_clip_children_shader);

	{
		if (state.canvas_state_buffer.is_valid()) {
			RD::get_singleton()->free(state.canvas_state_buffer);
		}

		memdelete_arr(state.light_uniforms);
		RD::get_singleton()->free(state.lights_uniform_buffer);
	}

	//shadow rendering
	{
		shadow_render.shader.version_free(shadow_render.shader_version);
		//this will also automatically clear all pipelines
		RD::get_singleton()->free(state.shadow_sampler);
	}

	//buffers
	{
		RD::get_singleton()->free(shader.quad_index_array);
		RD::get_singleton()->free(shader.quad_index_buffer);
		//primitives are erase by dependency
	}

	if (state.shadow_fb.is_valid()) {
		RD::get_singleton()->free(state.shadow_depth_texture);
	}
	RD::get_singleton()->free(state.shadow_texture);

	if (state.shadow_occluder_buffer.is_valid()) {
		RD::get_singleton()->free(state.shadow_occluder_buffer);
	}

	memdelete_arr(state.instance_data_array);
	for (uint32_t i = 0; i < BATCH_DATA_BUFFER_COUNT; i++) {
		for (uint32_t j = 0; j < state.canvas_instance_data_buffers[i].instance_buffers.size(); j++) {
			RD::get_singleton()->free(state.canvas_instance_data_buffers[i].instance_buffers[j]);
		}
	}

	RendererRD::TextureStorage::get_singleton()->canvas_texture_free(default_canvas_texture);
	//pipelines don't need freeing, they are all gone after shaders are gone

	memdelete(shader.default_version_data);
}
