/**************************************************************************/
/*  fsr2.h                                                                */
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

#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_accumulate_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_autogen_reactive_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_compute_luminance_pyramid_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_depth_clip_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_lock_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_rcas_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_reconstruct_previous_depth_pass.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/effects/fsr2/fsr2_tcr_autogen_pass.glsl.gen.h"

// This flag doesn't actually control anything GCC specific in FSR2. It determines
// if symbols should be exported, which is not required for Godot.
#ifndef FFX_GCC
#define FFX_GCC
#endif

#include "thirdparty/amd-fsr2/ffx_fsr2.h"

#define FSR2_MAX_QUEUED_FRAMES (4)
#define FSR2_MAX_UNIFORM_BUFFERS (4)
#define FSR2_MAX_BUFFERED_DESCRIPTORS (FFX_FSR2_PASS_COUNT * FSR2_MAX_QUEUED_FRAMES)
#define FSR2_UBO_RING_BUFFER_SIZE (FSR2_MAX_BUFFERED_DESCRIPTORS * FSR2_MAX_UNIFORM_BUFFERS)

namespace RendererRD {
class FSR2Context {
public:
	enum ResourceID : uint32_t {
		RESOURCE_ID_DYNAMIC = 0xFFFFFFFF
	};

	struct Resources {
		LocalVector<RID> rids;
		LocalVector<LocalVector<RID>> mip_slice_rids;
		LocalVector<uint32_t> ids;
		LocalVector<FfxResourceDescription> descriptions;
		LocalVector<uint32_t> dynamic_list;
		LocalVector<uint32_t> free_list;

		uint32_t add(RID p_rid, bool p_dynamic, uint32_t p_id, FfxResourceDescription p_description) {
			uint32_t ret_index;
			if (free_list.is_empty()) {
				ret_index = rids.size();
				uint32_t new_size = ret_index + 1;
				rids.resize(new_size);
				mip_slice_rids.resize(new_size);
				ids.resize(new_size);
				descriptions.resize(new_size);
			} else {
				uint32_t end_index = free_list.size() - 1;
				ret_index = free_list[end_index];
				free_list.resize(end_index);
			}

			rids[ret_index] = p_rid;
			mip_slice_rids[ret_index].clear();
			ids[ret_index] = p_id;
			descriptions[ret_index] = p_description;

			if (p_dynamic) {
				dynamic_list.push_back(ret_index);
			}

			return ret_index;
		}

		void remove(uint32_t p_index) {
			DEV_ASSERT(p_index < rids.size());
			free_list.push_back(p_index);
			rids[p_index] = RID();
			mip_slice_rids[p_index].clear();
			ids[p_index] = 0;
			descriptions[p_index] = {};
			dynamic_list.erase(p_index);
		}

		uint32_t size() const {
			return rids.size();
		}
	};

	struct Scratch {
		Resources resources;
		LocalVector<FfxGpuJobDescription> gpu_jobs;
		RID ubo_ring_buffer[FSR2_UBO_RING_BUFFER_SIZE];
		uint32_t ubo_ring_buffer_index = 0;
		FfxDevice device = nullptr;
	};

	Scratch scratch;
	FfxFsr2Context fsr_context;
	FfxFsr2ContextDescription fsr_desc;

	~FSR2Context();
};

class FSR2Effect {
public:
	struct RootSignature {
		// Proxy structure to store the shader required by RD that uses the terminology used by the FSR2 API.
		RID shader_rid;
	};

	struct Pipeline {
		RID pipeline_rid;
	};

	struct Pass {
		ShaderRD *shader;
		RID shader_version;
		RootSignature root_signature;
		uint32_t shader_variant = 0;
		Pipeline pipeline;
		Vector<FfxResourceBinding> sampled_bindings;
		Vector<FfxResourceBinding> storage_bindings;
		Vector<FfxResourceBinding> uniform_bindings;
	};

	struct Device {
		Pass passes[FFX_FSR2_PASS_COUNT];
		FfxDeviceCapabilities capabilities;
		RID point_clamp_sampler;
		RID linear_clamp_sampler;
	};

	struct Parameters {
		FSR2Context *context;
		Size2i internal_size;
		RID color;
		RID depth;
		RID velocity;
		RID reactive;
		RID exposure;
		RID output;
		float z_near = 0.0f;
		float z_far = 0.0f;
		float fovy = 0.0f;
		Vector2 jitter;
		float delta_time = 0.0f;
		float sharpness = 0.0f;
		bool reset_accumulation = false;
		Projection reprojection;
	};

	FSR2Effect();
	~FSR2Effect();
	FSR2Context *create_context(Size2i p_internal_size, Size2i p_target_size);
	void upscale(const Parameters &p_params);

private:
	struct {
		Fsr2DepthClipPassShaderRD depth_clip;
		Fsr2ReconstructPreviousDepthPassShaderRD reconstruct_previous_depth;
		Fsr2LockPassShaderRD lock;
		Fsr2AccumulatePassShaderRD accumulate;
		Fsr2AccumulatePassShaderRD accumulate_sharpen;
		Fsr2RcasPassShaderRD rcas;
		Fsr2ComputeLuminancePyramidPassShaderRD compute_luminance_pyramid;
		Fsr2AutogenReactivePassShaderRD autogen_reactive;
		Fsr2TcrAutogenPassShaderRD tcr_autogen;
	} shaders;

	Device device;
};

} // namespace RendererRD
