/**************************************************************************/
/*  pipeline_cache_rd.cpp                                                 */
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

#include "pipeline_cache_rd.h"

#include "core/os/memory.h"

RID PipelineCacheRD::_generate_version(RD::VertexFormatID p_vertex_format_id, RD::FramebufferFormatID p_framebuffer_format_id, bool p_wireframe, uint32_t p_render_pass, uint32_t p_bool_specializations) {
	RD::PipelineMultisampleState multisample_state_version = multisample_state;
	multisample_state_version.sample_count = RD::get_singleton()->framebuffer_format_get_texture_samples(p_framebuffer_format_id, p_render_pass);

	bool wireframe = p_wireframe;

	RD::PipelineRasterizationState raster_state_version = rasterization_state;
	raster_state_version.wireframe = wireframe;

	Vector<RD::PipelineSpecializationConstant> specialization_constants = base_specialization_constants;

	uint32_t bool_index = 0;
	uint32_t bool_specializations = p_bool_specializations;
	while (bool_specializations) {
		if (bool_specializations & (1 << bool_index)) {
			RD::PipelineSpecializationConstant sc;
			sc.bool_value = true;
			sc.constant_id = bool_index;
			sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
			specialization_constants.push_back(sc);
			bool_specializations &= ~(1 << bool_index);
		}
		bool_index++;
	}

	RID pipeline = RD::get_singleton()->render_pipeline_create(shader, p_framebuffer_format_id, p_vertex_format_id, render_primitive, raster_state_version, multisample_state_version, depth_stencil_state, blend_state, dynamic_state_flags, p_render_pass, specialization_constants);
	ERR_FAIL_COND_V(pipeline.is_null(), RID());
	versions = static_cast<Version *>(memrealloc(versions, sizeof(Version) * (version_count + 1)));
	versions[version_count].framebuffer_id = p_framebuffer_format_id;
	versions[version_count].vertex_id = p_vertex_format_id;
	versions[version_count].wireframe = wireframe;
	versions[version_count].pipeline = pipeline;
	versions[version_count].render_pass = p_render_pass;
	versions[version_count].bool_specializations = p_bool_specializations;
	version_count++;
	return pipeline;
}

void PipelineCacheRD::_clear() {
	// TODO: Clear should probably recompile all the variants already compiled instead to avoid stalls? Needs discussion.
	if (versions) {
		for (uint32_t i = 0; i < version_count; i++) {
			//shader may be gone, so this may not be valid
			if (RD::get_singleton()->render_pipeline_is_valid(versions[i].pipeline)) {
				RD::get_singleton()->free(versions[i].pipeline);
			}
		}
		version_count = 0;
		memfree(versions);
		versions = nullptr;
	}
}

void PipelineCacheRD::setup(RID p_shader, RD::RenderPrimitive p_primitive, const RD::PipelineRasterizationState &p_rasterization_state, RD::PipelineMultisampleState p_multisample, const RD::PipelineDepthStencilState &p_depth_stencil_state, const RD::PipelineColorBlendState &p_blend_state, int p_dynamic_state_flags, const Vector<RD::PipelineSpecializationConstant> &p_base_specialization_constants) {
	ERR_FAIL_COND(p_shader.is_null());
	_clear();
	shader = p_shader;
	render_primitive = p_primitive;
	rasterization_state = p_rasterization_state;
	multisample_state = p_multisample;
	depth_stencil_state = p_depth_stencil_state;
	blend_state = p_blend_state;
	dynamic_state_flags = p_dynamic_state_flags;
	base_specialization_constants = p_base_specialization_constants;
}
void PipelineCacheRD::update_specialization_constants(const Vector<RD::PipelineSpecializationConstant> &p_base_specialization_constants) {
	base_specialization_constants = p_base_specialization_constants;
	_clear();
}

void PipelineCacheRD::update_shader(RID p_shader) {
	ERR_FAIL_COND(p_shader.is_null());
	_clear();
	setup(p_shader, render_primitive, rasterization_state, multisample_state, depth_stencil_state, blend_state, dynamic_state_flags);
}

void PipelineCacheRD::clear() {
	_clear();
	shader = RID(); //clear shader
}

PipelineCacheRD::PipelineCacheRD() {
	version_count = 0;
	versions = nullptr;
}

PipelineCacheRD::~PipelineCacheRD() {
	_clear();
}
