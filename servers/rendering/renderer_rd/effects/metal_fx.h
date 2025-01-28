/**************************************************************************/
/*  metal_fx.h                                                            */
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

#ifndef METAL_FX_RD_H
#define METAL_FX_RD_H

#ifdef METAL_ENABLED

#include "spatial_upscaler.h"

#include "core/templates/paged_allocator.h"
#include "servers/rendering/renderer_scene_render.h"

#ifdef __OBJC__
@protocol MTLFXSpatialScaler;
@protocol MTLFXTemporalScaler;
#endif

namespace RendererRD {

struct MFXSpatialContext {
#ifdef __OBJC__
	id<MTLFXSpatialScaler> scaler = nullptr;
#else
	void *scaler = nullptr;
#endif
	MFXSpatialContext() = default;
	~MFXSpatialContext();
};

class MFXSpatialEffect : public SpatialUpscaler {
	struct CallbackArgs {
		MFXSpatialEffect *owner;
		RDD::TextureID src;
		RDD::TextureID dst;
		MFXSpatialContext ctx;

		CallbackArgs(MFXSpatialEffect *p_owner, RDD::TextureID p_src, RDD::TextureID p_dst, MFXSpatialContext p_ctx) :
				owner(p_owner), src(p_src), dst(p_dst), ctx(p_ctx) {}

		static void free(CallbackArgs **p_args) {
			(*p_args)->owner->args_allocator.free(*p_args);
			*p_args = nullptr;
		}
	};

	PagedAllocator<CallbackArgs, true, 16> args_allocator;
	static void callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata);
	String name = "MetalFX Spatial Upscale";

public:
	virtual String get_label() const final { return name; }
	virtual void ensure_context(Ref<RenderSceneBuffersRD> p_render_buffers) final;
	virtual void process(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_src, RID p_dst) final;

	struct CreateParams {
		Vector2i input_size;
		Vector2i output_size;
		RD::DataFormat input_format;
		RD::DataFormat output_format;
	};

	MFXSpatialContext *create_context(CreateParams p_params) const;

	MFXSpatialEffect();
	~MFXSpatialEffect();
};

struct MFXTemporalContext {
#ifdef __OBJC__
	id<MTLFXTemporalScaler> scaler = nullptr;
#else
	void *scaler = nullptr;
#endif
	MFXTemporalContext() = default;
	~MFXTemporalContext();
};

class MFXTemporalEffect {
	struct CallbackArgs {
		MFXTemporalEffect *owner;
		RDD::TextureID src;
		RDD::TextureID depth;
		RDD::TextureID motion;
		RDD::TextureID exposure;
		Vector2 jitter_offset;
		RDD::TextureID dst;
		MFXTemporalContext ctx;
		bool reset = false;

		CallbackArgs(
				MFXTemporalEffect *p_owner,
				RDD::TextureID p_src,
				RDD::TextureID p_depth,
				RDD::TextureID p_motion,
				RDD::TextureID p_exposure,
				Vector2 p_jitter_offset,
				RDD::TextureID p_dst,
				MFXTemporalContext p_ctx,
				bool p_reset) :
				owner(p_owner),
				src(p_src),
				depth(p_depth),
				motion(p_motion),
				exposure(p_exposure),
				jitter_offset(p_jitter_offset),
				dst(p_dst),
				ctx(p_ctx),
				reset(p_reset) {}

		static void free(CallbackArgs **p_args) {
			(*p_args)->owner->args_allocator.free(*p_args);
			*p_args = nullptr;
		}
	};

	PagedAllocator<CallbackArgs, true, 16> args_allocator;

	static void callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata);

public:
	MFXTemporalEffect();
	~MFXTemporalEffect();

	struct CreateParams {
		Vector2i input_size;
		Vector2i output_size;
		RD::DataFormat input_format;
		RD::DataFormat depth_format;
		RD::DataFormat motion_format;
		RD::DataFormat reactive_format;
		RD::DataFormat output_format;
		Vector2 motion_vector_scale;
	};

	MFXTemporalContext *create_context(CreateParams p_params) const;

	struct Params {
		RID src;
		RID depth;
		RID motion;
		RID exposure;
		RID dst;
		Vector2 jitter_offset;
		bool reset = false;
	};

	void process(MFXTemporalContext *p_ctx, Params p_params);
};

} //namespace RendererRD

#endif // METAL_ENABLED

#endif // METAL_FX_RD_H
