/**************************************************************************/
/*  bokeh_dof.h                                                           */
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

#pragma once

#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/shaders/effects/bokeh_dof.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/bokeh_dof_raster.glsl.gen.h"

namespace RendererRD {

class BokehDOF {
private:
	bool prefer_raster_effects;

	struct BokehPushConstant {
		uint32_t size[2];
		float z_far;
		float z_near;

		uint32_t orthogonal;
		float blur_size;
		float blur_scale;
		uint32_t steps;

		uint32_t blur_near_active;
		float blur_near_begin;
		float blur_near_end;
		uint32_t blur_far_active;

		float blur_far_begin;
		float blur_far_end;
		uint32_t second_pass;
		uint32_t half_size;

		uint32_t use_jitter;
		float jitter_seed;
		uint32_t use_physical_near;
		uint32_t use_physical_far;

		float blur_size_near;
		float blur_size_far;
		uint32_t pad[2];
	};

	enum BokehMode {
		BOKEH_GEN_BLUR_SIZE,
		BOKEH_GEN_BOKEH_BOX,
		BOKEH_GEN_BOKEH_BOX_NOWEIGHT,
		BOKEH_GEN_BOKEH_HEXAGONAL,
		BOKEH_GEN_BOKEH_HEXAGONAL_NOWEIGHT,
		BOKEH_GEN_BOKEH_CIRCULAR,
		BOKEH_COMPOSITE,
		BOKEH_MAX
	};

	struct Bokeh {
		BokehPushConstant push_constant;
		BokehDofShaderRD compute_shader;
		BokehDofRasterShaderRD raster_shader;
		RID shader_version;
		RID compute_pipelines[BOKEH_MAX];
		PipelineCacheRD raster_pipelines[BOKEH_MAX];
	} bokeh;

public:
	struct BokehBuffers {
		// bokeh buffers

		// textures
		Size2i base_texture_size;
		RID base_texture;
		RID depth_texture;
		RID secondary_texture;
		RID half_texture[2];

		// raster only
		RID base_fb;
		RID secondary_fb; // with weights
		RID half_fb[2]; // with weights
		RID base_weight_fb;
		RID weight_texture[4];
	};

	BokehDOF(bool p_prefer_raster_effects);
	~BokehDOF();

	void bokeh_dof_compute(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal);
	void bokeh_dof_raster(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal);
};

} // namespace RendererRD
