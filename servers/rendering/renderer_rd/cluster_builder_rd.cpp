/**************************************************************************/
/*  cluster_builder_rd.cpp                                                */
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

#include "cluster_builder_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server_globals.h"

ClusterBuilderSharedDataRD::ClusterBuilderSharedDataRD() {
	RD::VertexFormatID vertex_format;

	{
		Vector<RD::VertexAttribute> attributes;
		{
			RD::VertexAttribute va;
			va.format = RD::DATA_FORMAT_R32G32B32_SFLOAT;
			va.stride = sizeof(float) * 3;
			attributes.push_back(va);
		}
		vertex_format = RD::get_singleton()->vertex_format_create(attributes);
	}

	{
		RD::FramebufferFormatID fb_format;
		RD::PipelineColorBlendState blend_state;
		String defines;
		if (RD::get_singleton()->has_feature(RD::SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS)) {
			fb_format = RD::get_singleton()->framebuffer_format_create_empty();
			blend_state = RD::PipelineColorBlendState::create_disabled();
		} else {
			Vector<RD::AttachmentFormat> afs;
			afs.push_back(RD::AttachmentFormat());
			afs.write[0].usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			fb_format = RD::get_singleton()->framebuffer_format_create(afs);
			blend_state = RD::PipelineColorBlendState::create_blend();
			defines = "\n#define USE_ATTACHMENT\n";
		}

		RD::PipelineRasterizationState rasterization_state;
		rasterization_state.enable_depth_clamp = true;
		Vector<String> versions;
		versions.push_back("");
		cluster_render.cluster_render_shader.initialize(versions, defines);
		cluster_render.shader_version = cluster_render.cluster_render_shader.version_create();
		cluster_render.shader = cluster_render.cluster_render_shader.version_get_shader(cluster_render.shader_version, 0);
		cluster_render.shader_pipelines[ClusterRender::PIPELINE_NORMAL] = RD::get_singleton()->render_pipeline_create(cluster_render.shader, fb_format, vertex_format, RD::RENDER_PRIMITIVE_TRIANGLES, rasterization_state, RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), blend_state, 0);
		RD::PipelineMultisampleState ms;
		ms.sample_count = RD::TEXTURE_SAMPLES_4;
		cluster_render.shader_pipelines[ClusterRender::PIPELINE_MSAA] = RD::get_singleton()->render_pipeline_create(cluster_render.shader, fb_format, vertex_format, RD::RENDER_PRIMITIVE_TRIANGLES, rasterization_state, ms, RD::PipelineDepthStencilState(), blend_state, 0);
	}
	{
		Vector<String> versions;
		versions.push_back("");
		cluster_store.cluster_store_shader.initialize(versions);
		cluster_store.shader_version = cluster_store.cluster_store_shader.version_create();
		cluster_store.shader = cluster_store.cluster_store_shader.version_get_shader(cluster_store.shader_version, 0);
		cluster_store.shader_pipeline = RD::get_singleton()->compute_pipeline_create(cluster_store.shader);
	}
	{
		Vector<String> versions;
		versions.push_back("");
		cluster_debug.cluster_debug_shader.initialize(versions);
		cluster_debug.shader_version = cluster_debug.cluster_debug_shader.version_create();
		cluster_debug.shader = cluster_debug.cluster_debug_shader.version_get_shader(cluster_debug.shader_version, 0);
		cluster_debug.shader_pipeline = RD::get_singleton()->compute_pipeline_create(cluster_debug.shader);
	}

	{ // Sphere mesh data.
		static const uint32_t icosphere_vertex_count = 42;
		static const float icosphere_vertices[icosphere_vertex_count * 3] = {
			0, 0, -1, 0.7236073, -0.5257253, -0.4472195, -0.276388, -0.8506492, -0.4472199, -0.8944262, 0, -0.4472156, -0.276388, 0.8506492, -0.4472199, 0.7236073, 0.5257253, -0.4472195, 0.276388, -0.8506492, 0.4472199, -0.7236073, -0.5257253, 0.4472195, -0.7236073, 0.5257253, 0.4472195, 0.276388, 0.8506492, 0.4472199, 0.8944262, 0, 0.4472156, 0, 0, 1, -0.1624555, -0.4999952, -0.8506544, 0.4253227, -0.3090114, -0.8506542, 0.2628688, -0.8090116, -0.5257377, 0.8506479, 0, -0.5257359, 0.4253227, 0.3090114, -0.8506542, -0.5257298, 0, -0.8506517, -0.6881894, -0.4999969, -0.5257362, -0.1624555, 0.4999952, -0.8506544, -0.6881894, 0.4999969, -0.5257362, 0.2628688, 0.8090116, -0.5257377, 0.9510579, -0.3090126, 0, 0.9510579, 0.3090126, 0, 0, -1, 0, 0.5877856, -0.8090167, 0, -0.9510579, -0.3090126, 0, -0.5877856, -0.8090167, 0, -0.5877856, 0.8090167, 0, -0.9510579, 0.3090126, 0, 0.5877856, 0.8090167, 0, 0, 1, 0, 0.6881894, -0.4999969, 0.5257362, -0.2628688, -0.8090116, 0.5257377, -0.8506479, 0, 0.5257359, -0.2628688, 0.8090116, 0.5257377, 0.6881894, 0.4999969, 0.5257362, 0.1624555, -0.4999952, 0.8506544, 0.5257298, 0, 0.8506517, -0.4253227, -0.3090114, 0.8506542, -0.4253227, 0.3090114, 0.8506542, 0.1624555, 0.4999952, 0.8506544
		};
		static const uint32_t icosphere_triangle_count = 80;
		static const uint16_t icosphere_triangle_indices[icosphere_triangle_count * 3] = {
			0, 13, 12, 1, 13, 15, 0, 12, 17, 0, 17, 19, 0, 19, 16, 1, 15, 22, 2, 14, 24, 3, 18, 26, 4, 20, 28, 5, 21, 30, 1, 22, 25, 2, 24, 27, 3, 26, 29, 4, 28, 31, 5, 30, 23, 6, 32, 37, 7, 33, 39, 8, 34, 40, 9, 35, 41, 10, 36, 38, 38, 41, 11, 38, 36, 41, 36, 9, 41, 41, 40, 11, 41, 35, 40, 35, 8, 40, 40, 39, 11, 40, 34, 39, 34, 7, 39, 39, 37, 11, 39, 33, 37, 33, 6, 37, 37, 38, 11, 37, 32, 38, 32, 10, 38, 23, 36, 10, 23, 30, 36, 30, 9, 36, 31, 35, 9, 31, 28, 35, 28, 8, 35, 29, 34, 8, 29, 26, 34, 26, 7, 34, 27, 33, 7, 27, 24, 33, 24, 6, 33, 25, 32, 6, 25, 22, 32, 22, 10, 32, 30, 31, 9, 30, 21, 31, 21, 4, 31, 28, 29, 8, 28, 20, 29, 20, 3, 29, 26, 27, 7, 26, 18, 27, 18, 2, 27, 24, 25, 6, 24, 14, 25, 14, 1, 25, 22, 23, 10, 22, 15, 23, 15, 5, 23, 16, 21, 5, 16, 19, 21, 19, 4, 21, 19, 20, 4, 19, 17, 20, 17, 3, 20, 17, 18, 3, 17, 12, 18, 12, 2, 18, 15, 16, 5, 15, 13, 16, 13, 0, 16, 12, 14, 2, 12, 13, 14, 13, 1, 14
		};

		Vector<uint8_t> vertex_data;
		vertex_data.resize(sizeof(float) * icosphere_vertex_count * 3);
		memcpy(vertex_data.ptrw(), icosphere_vertices, vertex_data.size());

		sphere_vertex_buffer = RD::get_singleton()->vertex_buffer_create(vertex_data.size(), vertex_data);

		Vector<uint8_t> index_data;
		index_data.resize(sizeof(uint16_t) * icosphere_triangle_count * 3);
		memcpy(index_data.ptrw(), icosphere_triangle_indices, index_data.size());

		sphere_index_buffer = RD::get_singleton()->index_buffer_create(icosphere_triangle_count * 3, RD::INDEX_BUFFER_FORMAT_UINT16, index_data);

		Vector<RID> buffers;
		buffers.push_back(sphere_vertex_buffer);

		sphere_vertex_array = RD::get_singleton()->vertex_array_create(icosphere_vertex_count, vertex_format, buffers);

		sphere_index_array = RD::get_singleton()->index_array_create(sphere_index_buffer, 0, icosphere_triangle_count * 3);

		float min_d = 1e20;
		for (uint32_t i = 0; i < icosphere_triangle_count; i++) {
			Vector3 vertices[3];
			for (uint32_t j = 0; j < 3; j++) {
				uint32_t index = icosphere_triangle_indices[i * 3 + j];
				for (uint32_t k = 0; k < 3; k++) {
					vertices[j][k] = icosphere_vertices[index * 3 + k];
				}
			}
			Plane p(vertices[0], vertices[1], vertices[2]);
			min_d = MIN(Math::abs(p.d), min_d);
		}
		sphere_overfit = 1.0 / min_d;
	}

	{ // Cone mesh data.
		static const uint32_t cone_vertex_count = 99;
		static const float cone_vertices[cone_vertex_count * 3] = {
			0, 1, -1, 0.1950903, 0.9807853, -1, 0.3826835, 0.9238795, -1, 0.5555703, 0.8314696, -1, 0.7071068, 0.7071068, -1, 0.8314697, 0.5555702, -1, 0.9238795, 0.3826834, -1, 0.9807853, 0.1950903, -1, 1, 0, -1, 0.9807853, -0.1950902, -1, 0.9238796, -0.3826833, -1, 0.8314697, -0.5555702, -1, 0.7071068, -0.7071068, -1, 0.5555702, -0.8314697, -1, 0.3826833, -0.9238796, -1, 0.1950901, -0.9807853, -1, -3.25841e-7, -1, -1, -0.1950907, -0.9807852, -1, -0.3826839, -0.9238793, -1, -0.5555707, -0.8314693, -1, -0.7071073, -0.7071063, -1, -0.83147, -0.5555697, -1, -0.9238799, -0.3826827, -1, 0, 0, 0, -0.9807854, -0.1950894, -1, -1, 9.65599e-7, -1, -0.9807851, 0.1950913, -1, -0.9238791, 0.3826845, -1, -0.8314689, 0.5555713, -1, -0.7071059, 0.7071077, -1, -0.5555691, 0.8314704, -1, -0.3826821, 0.9238801, -1, -0.1950888, 0.9807856, -1
		};
		static const uint32_t cone_triangle_count = 62;
		static const uint16_t cone_triangle_indices[cone_triangle_count * 3] = {
			0, 23, 1, 1, 23, 2, 2, 23, 3, 3, 23, 4, 4, 23, 5, 5, 23, 6, 6, 23, 7, 7, 23, 8, 8, 23, 9, 9, 23, 10, 10, 23, 11, 11, 23, 12, 12, 23, 13, 13, 23, 14, 14, 23, 15, 15, 23, 16, 16, 23, 17, 17, 23, 18, 18, 23, 19, 19, 23, 20, 20, 23, 21, 21, 23, 22, 22, 23, 24, 24, 23, 25, 25, 23, 26, 26, 23, 27, 27, 23, 28, 28, 23, 29, 29, 23, 30, 30, 23, 31, 31, 23, 32, 32, 23, 0, 7, 15, 24, 32, 0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 3, 6, 7, 3, 7, 8, 9, 9, 10, 7, 10, 11, 7, 11, 12, 15, 12, 13, 15, 13, 14, 15, 15, 16, 17, 17, 18, 19, 19, 20, 24, 20, 21, 24, 21, 22, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 1, 3, 15, 17, 24, 17, 19, 24, 24, 26, 32, 26, 28, 32, 28, 30, 32, 32, 3, 7, 7, 11, 15, 32, 7, 24
		};

		Vector<uint8_t> vertex_data;
		vertex_data.resize(sizeof(float) * cone_vertex_count * 3);
		memcpy(vertex_data.ptrw(), cone_vertices, vertex_data.size());

		cone_vertex_buffer = RD::get_singleton()->vertex_buffer_create(vertex_data.size(), vertex_data);

		Vector<uint8_t> index_data;
		index_data.resize(sizeof(uint16_t) * cone_triangle_count * 3);
		memcpy(index_data.ptrw(), cone_triangle_indices, index_data.size());

		cone_index_buffer = RD::get_singleton()->index_buffer_create(cone_triangle_count * 3, RD::INDEX_BUFFER_FORMAT_UINT16, index_data);

		Vector<RID> buffers;
		buffers.push_back(cone_vertex_buffer);

		cone_vertex_array = RD::get_singleton()->vertex_array_create(cone_vertex_count, vertex_format, buffers);

		cone_index_array = RD::get_singleton()->index_array_create(cone_index_buffer, 0, cone_triangle_count * 3);

		float min_d = 1e20;
		for (uint32_t i = 0; i < cone_triangle_count; i++) {
			Vector3 vertices[3];
			int32_t zero_index = -1;
			for (uint32_t j = 0; j < 3; j++) {
				uint32_t index = cone_triangle_indices[i * 3 + j];
				for (uint32_t k = 0; k < 3; k++) {
					vertices[j][k] = cone_vertices[index * 3 + k];
				}
				if (vertices[j] == Vector3()) {
					zero_index = j;
				}
			}

			if (zero_index != -1) {
				Vector3 a = vertices[(zero_index + 1) % 3];
				Vector3 b = vertices[(zero_index + 2) % 3];
				Vector3 c = a + Vector3(0, 0, 1);
				Plane p(a, b, c);
				min_d = MIN(Math::abs(p.d), min_d);
			}
		}
		cone_overfit = 1.0 / min_d;
	}

	{ // Box mesh data.
		static const uint32_t box_vertex_count = 8;
		static const float box_vertices[box_vertex_count * 3] = {
			-1, -1, -1, -1, -1, 1, -1, 1, -1, -1, 1, 1, 1, -1, -1, 1, -1, 1, 1, 1, -1, 1, 1, 1
		};
		static const uint32_t box_triangle_count = 12;
		static const uint16_t box_triangle_indices[box_triangle_count * 3] = {
			1, 2, 0, 3, 6, 2, 7, 4, 6, 5, 0, 4, 6, 0, 2, 3, 5, 7, 1, 3, 2, 3, 7, 6, 7, 5, 4, 5, 1, 0, 6, 4, 0, 3, 1, 5
		};

		Vector<uint8_t> vertex_data;
		vertex_data.resize(sizeof(float) * box_vertex_count * 3);
		memcpy(vertex_data.ptrw(), box_vertices, vertex_data.size());

		box_vertex_buffer = RD::get_singleton()->vertex_buffer_create(vertex_data.size(), vertex_data);

		Vector<uint8_t> index_data;
		index_data.resize(sizeof(uint16_t) * box_triangle_count * 3);
		memcpy(index_data.ptrw(), box_triangle_indices, index_data.size());

		box_index_buffer = RD::get_singleton()->index_buffer_create(box_triangle_count * 3, RD::INDEX_BUFFER_FORMAT_UINT16, index_data);

		Vector<RID> buffers;
		buffers.push_back(box_vertex_buffer);

		box_vertex_array = RD::get_singleton()->vertex_array_create(box_vertex_count, vertex_format, buffers);

		box_index_array = RD::get_singleton()->index_array_create(box_index_buffer, 0, box_triangle_count * 3);
	}
}
ClusterBuilderSharedDataRD::~ClusterBuilderSharedDataRD() {
	RD::get_singleton()->free(sphere_vertex_buffer);
	RD::get_singleton()->free(sphere_index_buffer);
	RD::get_singleton()->free(cone_vertex_buffer);
	RD::get_singleton()->free(cone_index_buffer);
	RD::get_singleton()->free(box_vertex_buffer);
	RD::get_singleton()->free(box_index_buffer);

	cluster_render.cluster_render_shader.version_free(cluster_render.shader_version);
	cluster_store.cluster_store_shader.version_free(cluster_store.shader_version);
	cluster_debug.cluster_debug_shader.version_free(cluster_debug.shader_version);
}

/////////////////////////////

void ClusterBuilderRD::_clear() {
	if (cluster_buffer.is_null()) {
		return;
	}

	RD::get_singleton()->free(cluster_buffer);
	RD::get_singleton()->free(cluster_render_buffer);
	RD::get_singleton()->free(element_buffer);
	cluster_buffer = RID();
	cluster_render_buffer = RID();
	element_buffer = RID();

	memfree(render_elements);

	render_elements = nullptr;
	render_element_max = 0;
	render_element_count = 0;

	RD::get_singleton()->free(framebuffer);
	framebuffer = RID();

	cluster_render_uniform_set = RID();
	cluster_store_uniform_set = RID();
}

void ClusterBuilderRD::setup(Size2i p_screen_size, uint32_t p_max_elements, RID p_depth_buffer, RID p_depth_buffer_sampler, RID p_color_buffer) {
	ERR_FAIL_COND(p_max_elements == 0);
	ERR_FAIL_COND(p_screen_size.x < 1);
	ERR_FAIL_COND(p_screen_size.y < 1);

	_clear();

	screen_size = p_screen_size;

	cluster_screen_size.width = Math::division_round_up((uint32_t)p_screen_size.width, cluster_size);
	cluster_screen_size.height = Math::division_round_up((uint32_t)p_screen_size.height, cluster_size);

	max_elements_by_type = p_max_elements;
	if (max_elements_by_type % 32) { // Needs to be aligned to 32.
		max_elements_by_type += 32 - (max_elements_by_type % 32);
	}

	cluster_buffer_size = cluster_screen_size.x * cluster_screen_size.y * (max_elements_by_type / 32 + 32) * ELEMENT_TYPE_MAX * 4;

	render_element_max = max_elements_by_type * ELEMENT_TYPE_MAX;

	uint32_t element_tag_bits_size = render_element_max / 32;
	uint32_t element_tag_depth_bits_size = render_element_max;

	cluster_render_buffer_size = cluster_screen_size.x * cluster_screen_size.y * (element_tag_bits_size + element_tag_depth_bits_size) * 4; // Tag bits (element was used) and tag depth (depth range in which it was used).

	cluster_render_buffer = RD::get_singleton()->storage_buffer_create(cluster_render_buffer_size);
	cluster_buffer = RD::get_singleton()->storage_buffer_create(cluster_buffer_size);

	render_elements = static_cast<RenderElementData *>(memalloc(sizeof(RenderElementData) * render_element_max));
	render_element_count = 0;

	element_buffer = RD::get_singleton()->storage_buffer_create(sizeof(RenderElementData) * render_element_max);

	uint32_t div_value = 1 << divisor;
	if (use_msaa) {
		framebuffer = RD::get_singleton()->framebuffer_create_empty(p_screen_size / div_value, RD::TEXTURE_SAMPLES_4);
	} else {
		framebuffer = RD::get_singleton()->framebuffer_create_empty(p_screen_size / div_value);
	}

	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 1;
			u.append_id(state_uniform);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(element_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			u.append_id(cluster_render_buffer);
			uniforms.push_back(u);
		}

		cluster_render_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shared->cluster_render.shader, 0);
	}

	{
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(cluster_render_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(cluster_buffer);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			u.append_id(element_buffer);
			uniforms.push_back(u);
		}

		cluster_store_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shared->cluster_store.shader, 0);
	}

	if (p_color_buffer.is_valid()) {
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(cluster_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 2;
			u.append_id(p_color_buffer);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 3;
			u.append_id(p_depth_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER;
			u.binding = 4;
			u.append_id(p_depth_buffer_sampler);
			uniforms.push_back(u);
		}

		debug_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, shared->cluster_debug.shader, 0);
	} else {
		debug_uniform_set = RID();
	}
}

void ClusterBuilderRD::begin(const Transform3D &p_view_transform, const Projection &p_cam_projection, bool p_flip_y) {
	view_xform = p_view_transform.affine_inverse();
	projection = p_cam_projection;
	z_near = projection.get_z_near();
	z_far = projection.get_z_far();
	camera_orthogonal = p_cam_projection.is_orthogonal();
	adjusted_projection = projection;
	if (!camera_orthogonal) {
		adjusted_projection.adjust_perspective_znear(0.0001);
	}

	Projection correction;
	correction.set_depth_correction(p_flip_y);
	projection = correction * projection;
	adjusted_projection = correction * adjusted_projection;

	// Reset counts.
	render_element_count = 0;
	for (uint32_t i = 0; i < ELEMENT_TYPE_MAX; i++) {
		cluster_count_by_type[i] = 0;
	}
}

void ClusterBuilderRD::bake_cluster() {
	RENDER_TIMESTAMP("> Bake 3D Cluster");

	RD::get_singleton()->draw_command_begin_label("Bake Light Cluster");

	// Clear cluster buffer.
	RD::get_singleton()->buffer_clear(cluster_buffer, 0, cluster_buffer_size);

	if (render_element_count > 0) {
		// Clear render buffer.
		RD::get_singleton()->buffer_clear(cluster_render_buffer, 0, cluster_render_buffer_size);

		{ // Fill state uniform.

			StateUniform state;

			RendererRD::MaterialStorage::store_camera(adjusted_projection, state.projection);
			state.inv_z_far = 1.0 / z_far;
			state.screen_to_clusters_shift = get_shift_from_power_of_2(cluster_size);
			state.screen_to_clusters_shift -= divisor; //screen is smaller, shift one less

			state.cluster_screen_width = cluster_screen_size.x;
			state.cluster_depth_offset = (render_element_max / 32);
			state.cluster_data_size = state.cluster_depth_offset + render_element_max;

			RD::get_singleton()->buffer_update(state_uniform, 0, sizeof(StateUniform), &state);
		}

		// Update instances.

		RD::get_singleton()->buffer_update(element_buffer, 0, sizeof(RenderElementData) * render_element_count, render_elements);

		RENDER_TIMESTAMP("Render 3D Cluster Elements");

		// Render elements.
		{
			RD::DrawListID draw_list = RD::get_singleton()->draw_list_begin(framebuffer);
			ClusterBuilderSharedDataRD::ClusterRender::PushConstant push_constant = {};

			RD::get_singleton()->draw_list_bind_render_pipeline(draw_list, shared->cluster_render.shader_pipelines[use_msaa ? ClusterBuilderSharedDataRD::ClusterRender::PIPELINE_MSAA : ClusterBuilderSharedDataRD::ClusterRender::PIPELINE_NORMAL]);
			RD::get_singleton()->draw_list_bind_uniform_set(draw_list, cluster_render_uniform_set, 0);

			for (uint32_t i = 0; i < render_element_count;) {
				push_constant.base_index = i;
				switch (render_elements[i].type) {
					case ELEMENT_TYPE_OMNI_LIGHT: {
						RD::get_singleton()->draw_list_bind_vertex_array(draw_list, shared->sphere_vertex_array);
						RD::get_singleton()->draw_list_bind_index_array(draw_list, shared->sphere_index_array);
					} break;
					case ELEMENT_TYPE_SPOT_LIGHT: {
						// If the spot angle is above a certain threshold, use a sphere instead of a cone for building the clusters
						// since the cone gets too flat/large (spot angle close to 90 degrees) or
						// can't even cover the affected area of the light (spot angle above 90 degrees).
						if (render_elements[i].has_wide_spot_angle) {
							RD::get_singleton()->draw_list_bind_vertex_array(draw_list, shared->sphere_vertex_array);
							RD::get_singleton()->draw_list_bind_index_array(draw_list, shared->sphere_index_array);
						} else {
							RD::get_singleton()->draw_list_bind_vertex_array(draw_list, shared->cone_vertex_array);
							RD::get_singleton()->draw_list_bind_index_array(draw_list, shared->cone_index_array);
						}
					} break;
					case ELEMENT_TYPE_DECAL:
					case ELEMENT_TYPE_REFLECTION_PROBE: {
						RD::get_singleton()->draw_list_bind_vertex_array(draw_list, shared->box_vertex_array);
						RD::get_singleton()->draw_list_bind_index_array(draw_list, shared->box_index_array);
					} break;
				}

				RD::get_singleton()->draw_list_set_push_constant(draw_list, &push_constant, sizeof(ClusterBuilderSharedDataRD::ClusterRender::PushConstant));

				uint32_t instances = 1;
				RD::get_singleton()->draw_list_draw(draw_list, true, instances);
				i += instances;
			}
			RD::get_singleton()->draw_list_end();
		}
		// Store elements.
		RENDER_TIMESTAMP("Pack 3D Cluster Elements");

		{
			RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, shared->cluster_store.shader_pipeline);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, cluster_store_uniform_set, 0);

			ClusterBuilderSharedDataRD::ClusterStore::PushConstant push_constant;
			push_constant.cluster_render_data_size = render_element_max / 32 + render_element_max;
			push_constant.max_render_element_count_div_32 = render_element_max / 32;
			push_constant.cluster_screen_size[0] = cluster_screen_size.x;
			push_constant.cluster_screen_size[1] = cluster_screen_size.y;

			push_constant.render_element_count_div_32 = Math::division_round_up(render_element_count, 32U);
			push_constant.max_cluster_element_count_div_32 = max_elements_by_type / 32;
			push_constant.pad1 = 0;
			push_constant.pad2 = 0;

			RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ClusterBuilderSharedDataRD::ClusterStore::PushConstant));

			RD::get_singleton()->compute_list_dispatch_threads(compute_list, cluster_screen_size.x, cluster_screen_size.y, 1);

			RD::get_singleton()->compute_list_end();
		}
	}
	RENDER_TIMESTAMP("< Bake 3D Cluster");
	RD::get_singleton()->draw_command_end_label();
}

void ClusterBuilderRD::debug(ElementType p_element) {
	ERR_FAIL_COND(debug_uniform_set.is_null());
	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, shared->cluster_debug.shader_pipeline);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, debug_uniform_set, 0);

	ClusterBuilderSharedDataRD::ClusterDebug::PushConstant push_constant;
	push_constant.screen_size[0] = screen_size.x;
	push_constant.screen_size[1] = screen_size.y;
	push_constant.cluster_screen_size[0] = cluster_screen_size.x;
	push_constant.cluster_screen_size[1] = cluster_screen_size.y;
	push_constant.cluster_shift = get_shift_from_power_of_2(cluster_size);
	push_constant.cluster_type = p_element;
	push_constant.orthogonal = camera_orthogonal;
	push_constant.z_far = z_far;
	push_constant.z_near = z_near;
	push_constant.max_cluster_element_count_div_32 = max_elements_by_type / 32;

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ClusterBuilderSharedDataRD::ClusterDebug::PushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, screen_size.x, screen_size.y, 1);

	RD::get_singleton()->compute_list_end();
}

RID ClusterBuilderRD::get_cluster_buffer() const {
	return cluster_buffer;
}

uint32_t ClusterBuilderRD::get_cluster_size() const {
	return cluster_size;
}

uint32_t ClusterBuilderRD::get_max_cluster_elements() const {
	return max_elements_by_type;
}

void ClusterBuilderRD::set_shared(ClusterBuilderSharedDataRD *p_shared) {
	shared = p_shared;
}

ClusterBuilderRD::ClusterBuilderRD() {
	state_uniform = RD::get_singleton()->uniform_buffer_create(sizeof(StateUniform));
}

ClusterBuilderRD::~ClusterBuilderRD() {
	_clear();
	RD::get_singleton()->free(state_uniform);
}
