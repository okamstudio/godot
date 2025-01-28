/**************************************************************************/
/*  metal_fx.mm                                                           */
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

#import "metal_fx.h"

#import "../storage_rd/render_scene_buffers_rd.h"
#import "drivers/metal/pixel_formats.h"
#import "drivers/metal/rendering_device_driver_metal.h"

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

using namespace RendererRD;

#pragma mark - Spatial Scaler

MFXSpatialContext::~MFXSpatialContext() {
}

MFXSpatialEffect::MFXSpatialEffect() {
}

MFXSpatialEffect::~MFXSpatialEffect() {
}

void MFXSpatialEffect::callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	MDCommandBuffer *obj = (MDCommandBuffer *)(p_command_buffer.id);
	obj->end();

	id<MTLTexture> src_texture = rid::get(p_userdata->src);
	id<MTLTexture> dst_texture = rid::get(p_userdata->dst);

	__block id<MTLFXSpatialScaler> scaler = p_userdata->ctx.scaler;
	scaler.colorTexture = src_texture;
	scaler.outputTexture = dst_texture;
	[scaler encodeToCommandBuffer:obj->get_command_buffer()];
	// TODO(sgc): add API to retain objects until the command buffer completes
	[obj->get_command_buffer() addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
		// This block retains a reference to the scaler until the command buffer.
		// completes.
		scaler = nil;
	}];

	CallbackArgs::free(&p_userdata);

#pragma clang diagnostic pop
}

void MFXSpatialEffect::ensure_context(Ref<RenderSceneBuffersRD> p_render_buffers) {
	p_render_buffers->ensure_mfx(this);
}

void MFXSpatialEffect::process(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_src, RID p_dst) {
	MFXSpatialContext *ctx = p_render_buffers->get_mfx_spatial_context();
	DEV_ASSERT(ctx); // this should have been done by the caller via ensure_context

	CallbackArgs *userdata = args_allocator.alloc(
			this,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_src)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_dst)),
			*ctx);
	RD::CallbackResource res[2] = {
		{ .rid = p_src, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_dst, .usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE }
	};
	RD::get_singleton()->driver_callback_add((RDD::DriverCallback)MFXSpatialEffect::callback, userdata, VectorView<RD::CallbackResource>(res, 2));
}

MFXSpatialContext *MFXSpatialEffect::create_context(CreateParams p_params) const {
	DEV_ASSERT(RD::get_singleton()->has_feature(RD::SUPPORTS_METALFX_SPATIAL));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	RenderingDeviceDriverMetal *rdd = (RenderingDeviceDriverMetal *)RD::get_singleton()->get_device_driver();
	PixelFormats &pf = rdd->get_pixel_formats();
	id<MTLDevice> dev = rdd->get_device();

	MTLFXSpatialScalerDescriptor *desc = [MTLFXSpatialScalerDescriptor new];
	desc.inputWidth = (NSUInteger)p_params.input_size.width;
	desc.inputHeight = (NSUInteger)p_params.input_size.height;

	desc.outputWidth = (NSUInteger)p_params.output_size.width;
	desc.outputHeight = (NSUInteger)p_params.output_size.height;

	desc.colorTextureFormat = pf.getMTLPixelFormat(p_params.input_format);
	desc.outputTextureFormat = pf.getMTLPixelFormat(p_params.output_format);
	desc.colorProcessingMode = MTLFXSpatialScalerColorProcessingModeLinear;
	id<MTLFXSpatialScaler> scaler = [desc newSpatialScalerWithDevice:dev];
	MFXSpatialContext *context = memnew(MFXSpatialContext);
	context->scaler = scaler;

#pragma clang diagnostic pop

	return context;
}

#pragma mark - Temporal Scaler

MFXTemporalContext::~MFXTemporalContext() {}

MFXTemporalEffect::MFXTemporalEffect() {}
MFXTemporalEffect::~MFXTemporalEffect() {}

MFXTemporalContext *MFXTemporalEffect::create_context(CreateParams p_params) const {
	DEV_ASSERT(RD::get_singleton()->has_feature(RD::SUPPORTS_METALFX_TEMPORAL));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	RenderingDeviceDriverMetal *rdd = (RenderingDeviceDriverMetal *)RD::get_singleton()->get_device_driver();
	PixelFormats &pf = rdd->get_pixel_formats();
	id<MTLDevice> dev = rdd->get_device();

	MTLFXTemporalScalerDescriptor *desc = [MTLFXTemporalScalerDescriptor new];
	desc.inputWidth = (NSUInteger)p_params.input_size.width;
	desc.inputHeight = (NSUInteger)p_params.input_size.height;

	desc.outputWidth = (NSUInteger)p_params.output_size.width;
	desc.outputHeight = (NSUInteger)p_params.output_size.height;

	desc.colorTextureFormat = pf.getMTLPixelFormat(p_params.input_format);
	desc.depthTextureFormat = pf.getMTLPixelFormat(p_params.depth_format);
	desc.motionTextureFormat = pf.getMTLPixelFormat(p_params.motion_format);
	desc.autoExposureEnabled = NO;

	desc.outputTextureFormat = pf.getMTLPixelFormat(p_params.output_format);

	id<MTLFXTemporalScaler> scaler = [desc newTemporalScalerWithDevice:dev];
	MFXTemporalContext *context = memnew(MFXTemporalContext);
	context->scaler = scaler;

	scaler.motionVectorScaleX = p_params.motion_vector_scale.x;
	scaler.motionVectorScaleY = p_params.motion_vector_scale.y;
	scaler.depthReversed = true; // Godot uses reverse Z per https://github.com/godotengine/godot/pull/88328

#pragma clang diagnostic pop

	return context;
}

void MFXTemporalEffect::process(RendererRD::MFXTemporalContext *p_ctx, RendererRD::MFXTemporalEffect::Params p_params) {
	CallbackArgs *userdata = args_allocator.alloc(
			this,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.src)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.depth)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.motion)),
			p_params.exposure.is_valid() ? RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.exposure)) : RDD::TextureID(),
			p_params.jitter_offset,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.dst)),
			*p_ctx,
			p_params.reset);
	RD::CallbackResource res[3] = {
		{ .rid = p_params.src, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_params.depth, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_params.dst, .usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE },
	};
	RD::get_singleton()->driver_callback_add((RDD::DriverCallback)MFXTemporalEffect::callback, userdata, VectorView<RD::CallbackResource>(res, 3));
}

void MFXTemporalEffect::callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	MDCommandBuffer *obj = (MDCommandBuffer *)(p_command_buffer.id);
	obj->end();

	id<MTLTexture> src_texture = rid::get(p_userdata->src);
	id<MTLTexture> depth = rid::get(p_userdata->depth);
	id<MTLTexture> motion = rid::get(p_userdata->motion);
	id<MTLTexture> exposure = rid::get(p_userdata->exposure);

	id<MTLTexture> dst_texture = rid::get(p_userdata->dst);

	__block id<MTLFXTemporalScaler> scaler = p_userdata->ctx.scaler;
	scaler.reset = p_userdata->reset;
	scaler.colorTexture = src_texture;
	scaler.depthTexture = depth;
	scaler.motionTexture = motion;
	scaler.exposureTexture = exposure;
	scaler.jitterOffsetX = p_userdata->jitter_offset.x;
	scaler.jitterOffsetY = p_userdata->jitter_offset.y;
	scaler.outputTexture = dst_texture;
	[scaler encodeToCommandBuffer:obj->get_command_buffer()];
	// TODO(sgc): add API to retain objects until the command buffer completes
	[obj->get_command_buffer() addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
		// This block retains a reference to the scaler until the command buffer.
		// completes.
		scaler = nil;
	}];

	CallbackArgs::free(&p_userdata);

#pragma clang diagnostic pop
}
