/**************************************************************************/
/*  smaa.h                                                                */
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

#ifndef SMAA_RD_H
#define SMAA_RD_H

#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/shaders/effects/smaa_blending.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/smaa_edge_detection.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/smaa_weight_calculation.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_scene_render.h"

#include "servers/rendering_server.h"

#define RB_SCOPE_SMAA SNAME("rb_smaa")

#define RB_EDGES SNAME("edges")
#define RB_BLEND SNAME("blend")
#define RB_STENCIL SNAME("stencil")

namespace RendererRD {

class SMAA {
private:
	enum SMAAMode {
		SMAA_EDGE_DETECTION_COLOR,
		SMAA_WEIGHT_FULL,
		SMAA_BLENDING,
		SMAA_MAX,
	};

	struct SMAAEdgePushConstant {
		float inv_size[2];
		float reserved[2];
	};

	struct SMAAWeightPushConstant {
		float inv_size[2];
		uint32_t size[2];

		float subsample_indices[4];
	};

	struct SMAABlendPushConstant {
		float inv_size[2];
		float reserved[2];
	};

	struct SMAAEffect {
		SMAAEdgePushConstant edge_push_constant;
		SmaaEdgeDetectionShaderRD edge_shader;
		RID edge_shader_version;

		SMAAWeightPushConstant weight_push_constant;
		SmaaWeightCalculationShaderRD weight_shader;
		RID weight_shader_version;

		SMAABlendPushConstant blend_push_constant;
		SmaaBlendingShaderRD blend_shader;
		RID blend_shader_version;

		RID search_tex;
		RID area_tex;

		RD::DataFormat stencil_format;

		PipelineCacheRD pipelines[SMAA_MAX];
	} smaa;

public:
	SMAA();
	~SMAA();

	void allocate_render_targets(Ref<RenderSceneBuffersRD> p_render_buffers);
	void process(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_source_color, RID p_dst_framebuffer);
};

} // namespace RendererRD

#endif // SMAA_RD_H
