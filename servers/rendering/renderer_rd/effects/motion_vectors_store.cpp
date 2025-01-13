/**************************************************************************/
/*  motion_vectors_store.cpp                                              */
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

#include "motion_vectors_store.h"

#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"

namespace RendererRD {

MotionVectorsStore::MotionVectorsStore() {
	Vector<String> modes;
	modes.push_back("");

	motion_shader.initialize(modes);
	shader_version = motion_shader.version_create();

	pipeline = RD::get_singleton()->compute_pipeline_create(motion_shader.version_get_shader(shader_version, 0));
}

MotionVectorsStore::~MotionVectorsStore() {
	motion_shader.version_free(shader_version);
}

void MotionVectorsStore::process(Ref<RenderSceneBuffersRD> p_render_buffers,
		const Projection &p_current_projection, const Transform3D &p_current_transform,
		const Projection &p_previous_projection, const Transform3D &p_previous_transform) {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);

	uint32_t view_count = p_render_buffers->get_view_count();
	Size2i internal_size = p_render_buffers->get_internal_size();

	PushConstant push_constant;
	{
		push_constant.resolution[0] = internal_size.width;
		push_constant.resolution[1] = internal_size.height;

		Projection correction;
		correction.set_depth_correction(true, true, false);
		Projection reprojection = (correction * p_previous_projection) * p_previous_transform.affine_inverse() * p_current_transform * (correction * p_current_projection).inverse();
		RendererRD::MaterialStorage::store_camera(reprojection, push_constant.reprojection_matrix);
	}

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::get_singleton()->draw_command_begin_label("Motion Vector Store");

	RID shader = motion_shader.version_get_shader(shader_version, 0);
	ERR_FAIL_COND(shader.is_null());

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipeline);

	for (uint32_t v = 0; v < view_count; v++) {
		RID velocity = p_render_buffers->get_velocity_buffer(false, v);
		RID depth = p_render_buffers->get_depth_texture(v);
		RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, depth }));
		RD::Uniform u_velocity(RD::UNIFORM_TYPE_IMAGE, 1, velocity);

		RID uniform_set = uniform_set_cache->get_cache(shader, 0, u_depth, u_velocity);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(PushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, internal_size.width, internal_size.height, 1);
	}

	RD::get_singleton()->compute_list_end();

	RD::get_singleton()->draw_command_end_label();
}

} //namespace RendererRD
