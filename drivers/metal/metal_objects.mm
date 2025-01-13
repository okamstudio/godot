/**************************************************************************/
/*  metal_objects.mm                                                      */
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

/**************************************************************************/
/*                                                                        */
/* Portions of this code were derived from MoltenVK.                      */
/*                                                                        */
/* Copyright (c) 2015-2023 The Brenwill Workshop Ltd.                     */
/* (http://www.brenwill.com)                                              */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#import "metal_objects.h"

#import "metal_utils.h"
#import "pixel_formats.h"
#import "rendering_device_driver_metal.h"

#import <os/signpost.h>

void MDCommandBuffer::begin() {
	DEV_ASSERT(commandBuffer == nil);
	commandBuffer = queue.commandBufferWithUnretainedReferences;
}

void MDCommandBuffer::end() {
	switch (type) {
		case MDCommandBufferStateType::None:
			return;
		case MDCommandBufferStateType::Render:
			return render_end_pass();
		case MDCommandBufferStateType::Compute:
			return _end_compute_dispatch();
		case MDCommandBufferStateType::Blit:
			return _end_blit();
	}
}

void MDCommandBuffer::commit() {
	end();
	[commandBuffer commit];
	commandBuffer = nil;
}

void MDCommandBuffer::bind_pipeline(RDD::PipelineID p_pipeline) {
	MDPipeline *p = (MDPipeline *)(p_pipeline.id);

	// End current encoder if it is a compute encoder or blit encoder,
	// as they do not have a defined end boundary in the RDD like render.
	if (type == MDCommandBufferStateType::Compute) {
		_end_compute_dispatch();
	} else if (type == MDCommandBufferStateType::Blit) {
		_end_blit();
	}

	if (p->type == MDPipelineType::Render) {
		DEV_ASSERT(type == MDCommandBufferStateType::Render);
		MDRenderPipeline *rp = (MDRenderPipeline *)p;

		if (render.encoder == nil) {
			// This error would happen if the render pass failed.
			ERR_FAIL_NULL_MSG(render.desc, "Render pass descriptor is null.");

			// This condition occurs when there are no attachments when calling render_next_subpass()
			// and is due to the SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS flag.
			render.desc.defaultRasterSampleCount = static_cast<NSUInteger>(rp->sample_count);

// NOTE(sgc): This is to test rdar://FB13605547 and will be deleted once fix is confirmed.
#if 0
			if (render.pipeline->sample_count == 4) {
				static id<MTLTexture> tex = nil;
				static id<MTLTexture> res_tex = nil;
				static dispatch_once_t onceToken;
				dispatch_once(&onceToken, ^{
					Size2i sz = render.frameBuffer->size;
					MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:sz.width height:sz.height mipmapped:NO];
					td.textureType = MTLTextureType2DMultisample;
					td.storageMode = MTLStorageModeMemoryless;
					td.usage = MTLTextureUsageRenderTarget;
					td.sampleCount = render.pipeline->sample_count;
					tex = [device_driver->get_device() newTextureWithDescriptor:td];

					td.textureType = MTLTextureType2D;
					td.storageMode = MTLStorageModePrivate;
					td.usage = MTLTextureUsageShaderWrite;
					td.sampleCount = 1;
					res_tex = [device_driver->get_device() newTextureWithDescriptor:td];
				});
				render.desc.colorAttachments[0].texture = tex;
				render.desc.colorAttachments[0].loadAction = MTLLoadActionClear;
				render.desc.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;

				render.desc.colorAttachments[0].resolveTexture = res_tex;
			}
#endif
			render.encoder = [commandBuffer renderCommandEncoderWithDescriptor:render.desc];
		}

		if (render.pipeline != rp) {
			render.dirty.set_flag((RenderState::DirtyFlag)(RenderState::DIRTY_PIPELINE | RenderState::DIRTY_RASTER));
			// Mark all uniforms as dirty, as variants of a shader pipeline may have a different entry point ABI,
			// due to setting force_active_argument_buffer_resources = true for spirv_cross::CompilerMSL::Options.
			// As a result, uniform sets with the same layout will generate redundant binding warnings when
			// capturing a Metal frame in Xcode.
			//
			// If we don't mark as dirty, then some bindings will generate a validation error.
			render.mark_uniforms_dirty();
			if (render.pipeline != nullptr && render.pipeline->depth_stencil != rp->depth_stencil) {
				render.dirty.set_flag(RenderState::DIRTY_DEPTH);
			}
			if (rp->raster_state.blend.enabled) {
				render.dirty.set_flag(RenderState::DIRTY_BLEND);
			}
			render.pipeline = rp;
		}
	} else if (p->type == MDPipelineType::Compute) {
		DEV_ASSERT(type == MDCommandBufferStateType::None);
		type = MDCommandBufferStateType::Compute;

		compute.pipeline = (MDComputePipeline *)p;
		compute.encoder = commandBuffer.computeCommandEncoder;
		[compute.encoder setComputePipelineState:compute.pipeline->state];
	}
}

id<MTLBlitCommandEncoder> MDCommandBuffer::blit_command_encoder() {
	switch (type) {
		case MDCommandBufferStateType::None:
			break;
		case MDCommandBufferStateType::Render:
			render_end_pass();
			break;
		case MDCommandBufferStateType::Compute:
			_end_compute_dispatch();
			break;
		case MDCommandBufferStateType::Blit:
			return blit.encoder;
	}

	type = MDCommandBufferStateType::Blit;
	blit.encoder = commandBuffer.blitCommandEncoder;
	return blit.encoder;
}

void MDCommandBuffer::encodeRenderCommandEncoderWithDescriptor(MTLRenderPassDescriptor *p_desc, NSString *p_label) {
	switch (type) {
		case MDCommandBufferStateType::None:
			break;
		case MDCommandBufferStateType::Render:
			render_end_pass();
			break;
		case MDCommandBufferStateType::Compute:
			_end_compute_dispatch();
			break;
		case MDCommandBufferStateType::Blit:
			_end_blit();
			break;
	}

	id<MTLRenderCommandEncoder> enc = [commandBuffer renderCommandEncoderWithDescriptor:p_desc];
	if (p_label != nil) {
		[enc pushDebugGroup:p_label];
		[enc popDebugGroup];
	}
	[enc endEncoding];
}

#pragma mark - Render Commands

void MDCommandBuffer::render_bind_uniform_set(RDD::UniformSetID p_uniform_set, RDD::ShaderID p_shader, uint32_t p_set_index) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	MDUniformSet *set = (MDUniformSet *)(p_uniform_set.id);
	if (render.uniform_sets.size() <= set->index) {
		uint32_t s = render.uniform_sets.size();
		render.uniform_sets.resize(set->index + 1);
		// Set intermediate values to null.
		std::fill(&render.uniform_sets[s], &render.uniform_sets[set->index] + 1, nullptr);
	}

	if (render.uniform_sets[set->index] != set) {
		render.dirty.set_flag(RenderState::DIRTY_UNIFORMS);
		render.uniform_set_mask |= 1ULL << set->index;
		render.uniform_sets[set->index] = set;
	}
}

void MDCommandBuffer::render_bind_uniform_sets(VectorView<RDD::UniformSetID> p_uniform_sets, RDD::ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	for (size_t i = 0u; i < p_set_count; ++i) {
		MDUniformSet *set = (MDUniformSet *)(p_uniform_sets[i].id);
		if (render.uniform_sets.size() <= set->index) {
			uint32_t s = render.uniform_sets.size();
			render.uniform_sets.resize(set->index + 1);
			// Set intermediate values to null.
			std::fill(&render.uniform_sets[s], &render.uniform_sets[set->index] + 1, nullptr);
		}

		if (render.uniform_sets[set->index] != set) {
			render.dirty.set_flag(RenderState::DIRTY_UNIFORMS);
			render.uniform_set_mask |= 1ULL << set->index;
			render.uniform_sets[set->index] = set;
		}
	}
}

void MDCommandBuffer::render_clear_attachments(VectorView<RDD::AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	const MDSubpass &subpass = render.get_subpass();

	uint32_t vertex_count = p_rects.size() * 6 * subpass.view_count;
	simd::float4 vertices[vertex_count];
	simd::float4 clear_colors[ClearAttKey::ATTACHMENT_COUNT];

	Size2i size = render.frameBuffer->size;
	Rect2i render_area = render.clip_to_render_area({ { 0, 0 }, size });
	size = Size2i(render_area.position.x + render_area.size.width, render_area.position.y + render_area.size.height);
	_populate_vertices(vertices, size, p_rects);

	ClearAttKey key;
	key.sample_count = render.pass->get_sample_count();
	if (subpass.view_count > 1) {
		key.enable_layered_rendering();
	}

	float depth_value = 0;
	uint32_t stencil_value = 0;

	for (uint32_t i = 0; i < p_attachment_clears.size(); i++) {
		RDD::AttachmentClear const &attClear = p_attachment_clears[i];
		uint32_t attachment_index;
		if (attClear.aspect.has_flag(RDD::TEXTURE_ASPECT_COLOR_BIT)) {
			attachment_index = attClear.color_attachment;
		} else {
			attachment_index = subpass.depth_stencil_reference.attachment;
		}

		MDAttachment const &mda = render.pass->attachments[attachment_index];
		if (attClear.aspect.has_flag(RDD::TEXTURE_ASPECT_COLOR_BIT)) {
			key.set_color_format(attachment_index, mda.format);
			clear_colors[attachment_index] = {
				attClear.value.color.r,
				attClear.value.color.g,
				attClear.value.color.b,
				attClear.value.color.a
			};
		}

		if (attClear.aspect.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT)) {
			key.set_depth_format(mda.format);
			depth_value = attClear.value.depth;
		}

		if (attClear.aspect.has_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT)) {
			key.set_stencil_format(mda.format);
			stencil_value = attClear.value.stencil;
		}
	}
	clear_colors[ClearAttKey::DEPTH_INDEX] = {
		depth_value,
		depth_value,
		depth_value,
		depth_value
	};

	id<MTLRenderCommandEncoder> enc = render.encoder;

	MDResourceCache &cache = device_driver->get_resource_cache();

	[enc pushDebugGroup:@"ClearAttachments"];
	[enc setRenderPipelineState:cache.get_clear_render_pipeline_state(key, nil)];
	[enc setDepthStencilState:cache.get_depth_stencil_state(
									  key.is_depth_enabled(),
									  key.is_stencil_enabled())];
	[enc setStencilReferenceValue:stencil_value];
	[enc setCullMode:MTLCullModeNone];
	[enc setTriangleFillMode:MTLTriangleFillModeFill];
	[enc setDepthBias:0 slopeScale:0 clamp:0];
	[enc setViewport:{ 0, 0, (double)size.width, (double)size.height, 0.0, 1.0 }];
	[enc setScissorRect:{ 0, 0, (NSUInteger)size.width, (NSUInteger)size.height }];

	[enc setVertexBytes:clear_colors length:sizeof(clear_colors) atIndex:0];
	[enc setFragmentBytes:clear_colors length:sizeof(clear_colors) atIndex:0];
	[enc setVertexBytes:vertices length:vertex_count * sizeof(vertices[0]) atIndex:device_driver->get_metal_buffer_index_for_vertex_attribute_binding(VERT_CONTENT_BUFFER_INDEX)];

	[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertex_count];
	[enc popDebugGroup];

	render.dirty.set_flag((RenderState::DirtyFlag)(RenderState::DIRTY_PIPELINE | RenderState::DIRTY_DEPTH | RenderState::DIRTY_RASTER));
	render.mark_uniforms_dirty({ 0 }); // Mark index 0 dirty, if there is already a binding for index 0.
	render.mark_viewport_dirty();
	render.mark_scissors_dirty();
	render.mark_vertex_dirty();
	render.mark_blend_dirty();
}

void MDCommandBuffer::_render_set_dirty_state() {
	_render_bind_uniform_sets();

	MDSubpass const &subpass = render.get_subpass();
	if (subpass.view_count > 1) {
		uint32_t view_range[2] = { 0, subpass.view_count };
		[render.encoder setVertexBytes:view_range length:sizeof(view_range) atIndex:VIEW_MASK_BUFFER_INDEX];
		[render.encoder setFragmentBytes:view_range length:sizeof(view_range) atIndex:VIEW_MASK_BUFFER_INDEX];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_PIPELINE)) {
		[render.encoder setRenderPipelineState:render.pipeline->state];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_VIEWPORT)) {
		[render.encoder setViewports:render.viewports.ptr() count:render.viewports.size()];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_DEPTH)) {
		[render.encoder setDepthStencilState:render.pipeline->depth_stencil];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_RASTER)) {
		render.pipeline->raster_state.apply(render.encoder);
	}

	if (render.dirty.has_flag(RenderState::DIRTY_SCISSOR) && !render.scissors.is_empty()) {
		size_t len = render.scissors.size();
		MTLScissorRect rects[len];
		for (size_t i = 0; i < len; i++) {
			rects[i] = render.clip_to_render_area(render.scissors[i]);
		}
		[render.encoder setScissorRects:rects count:len];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_BLEND) && render.blend_constants.has_value()) {
		[render.encoder setBlendColorRed:render.blend_constants->r green:render.blend_constants->g blue:render.blend_constants->b alpha:render.blend_constants->a];
	}

	if (render.dirty.has_flag(RenderState::DIRTY_VERTEX)) {
		uint32_t p_binding_count = render.vertex_buffers.size();
		uint32_t first = device_driver->get_metal_buffer_index_for_vertex_attribute_binding(p_binding_count - 1);
		[render.encoder setVertexBuffers:render.vertex_buffers.ptr()
								 offsets:render.vertex_offsets.ptr()
							   withRange:NSMakeRange(first, p_binding_count)];
	}

	render.dirty.clear();
}

void MDCommandBuffer::render_set_viewport(VectorView<Rect2i> p_viewports) {
	render.viewports.resize(p_viewports.size());
	for (uint32_t i = 0; i < p_viewports.size(); i += 1) {
		Rect2i const &vp = p_viewports[i];
		render.viewports[i] = {
			.originX = static_cast<double>(vp.position.x),
			.originY = static_cast<double>(vp.position.y),
			.width = static_cast<double>(vp.size.width),
			.height = static_cast<double>(vp.size.height),
			.znear = 0.0,
			.zfar = 1.0,
		};
	}

	render.dirty.set_flag(RenderState::DIRTY_VIEWPORT);
}

void MDCommandBuffer::render_set_scissor(VectorView<Rect2i> p_scissors) {
	render.scissors.resize(p_scissors.size());
	for (uint32_t i = 0; i < p_scissors.size(); i += 1) {
		Rect2i const &vp = p_scissors[i];
		render.scissors[i] = {
			.x = static_cast<NSUInteger>(vp.position.x),
			.y = static_cast<NSUInteger>(vp.position.y),
			.width = static_cast<NSUInteger>(vp.size.width),
			.height = static_cast<NSUInteger>(vp.size.height),
		};
	}

	render.dirty.set_flag(RenderState::DIRTY_SCISSOR);
}

void MDCommandBuffer::render_set_blend_constants(const Color &p_constants) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	if (render.blend_constants != p_constants) {
		render.blend_constants = p_constants;
		render.dirty.set_flag(RenderState::DIRTY_BLEND);
	}
}

void BoundUniformSet::merge_into(ResourceUsageMap &p_dst) const {
	for (KeyValue<StageResourceUsage, ResourceVector> const &keyval : usage_to_resources) {
		ResourceVector *resources = p_dst.getptr(keyval.key);
		if (resources == nullptr) {
			resources = &p_dst.insert(keyval.key, ResourceVector())->value;
		}
		// Reserve space for the new resources, assuming they are all added.
		resources->reserve(resources->size() + keyval.value.size());

		uint32_t i = 0, j = 0;
		__unsafe_unretained id<MTLResource> *resources_ptr = resources->ptr();
		const __unsafe_unretained id<MTLResource> *keyval_ptr = keyval.value.ptr();
		// 2-way merge.
		while (i < resources->size() && j < keyval.value.size()) {
			if (resources_ptr[i] < keyval_ptr[j]) {
				i++;
			} else if (resources_ptr[i] > keyval_ptr[j]) {
				resources->insert(i, keyval_ptr[j]);
				i++;
				j++;
			} else {
				i++;
				j++;
			}
		}
		// Append the remaining resources.
		for (; j < keyval.value.size(); j++) {
			resources->push_back(keyval_ptr[j]);
		}
	}
}

void MDCommandBuffer::_render_bind_uniform_sets() {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	if (!render.dirty.has_flag(RenderState::DIRTY_UNIFORMS)) {
		return;
	}

	render.dirty.clear_flag(RenderState::DIRTY_UNIFORMS);
	uint64_t set_uniforms = render.uniform_set_mask;
	render.uniform_set_mask = 0;

	id<MTLRenderCommandEncoder> enc = render.encoder;
	MDRenderShader *shader = render.pipeline->shader;
	id<MTLDevice> device = enc.device;

	while (set_uniforms != 0) {
		// Find the index of the next set bit.
		int index = __builtin_ctzll(set_uniforms);
		// Clear the set bit.
		set_uniforms &= (set_uniforms - 1);
		MDUniformSet *set = render.uniform_sets[index];
		if (set == nullptr || set->index >= (uint32_t)shader->sets.size()) {
			continue;
		}
		UniformSet const &set_info = shader->sets[set->index];

		BoundUniformSet &bus = set->boundUniformSetForShader(shader, device);
		bus.merge_into(render.resource_usage);

		// Set the buffer for the vertex stage.
		{
			uint32_t const *offset = set_info.offsets.getptr(RDD::SHADER_STAGE_VERTEX);
			if (offset) {
				[enc setVertexBuffer:bus.buffer offset:*offset atIndex:set->index];
			}
		}
		// Set the buffer for the fragment stage.
		{
			uint32_t const *offset = set_info.offsets.getptr(RDD::SHADER_STAGE_FRAGMENT);
			if (offset) {
				[enc setFragmentBuffer:bus.buffer offset:*offset atIndex:set->index];
			}
		}
	}
}

void MDCommandBuffer::_populate_vertices(simd::float4 *p_vertices, Size2i p_fb_size, VectorView<Rect2i> p_rects) {
	uint32_t idx = 0;
	for (uint32_t i = 0; i < p_rects.size(); i++) {
		Rect2i const &rect = p_rects[i];
		idx = _populate_vertices(p_vertices, idx, rect, p_fb_size);
	}
}

uint32_t MDCommandBuffer::_populate_vertices(simd::float4 *p_vertices, uint32_t p_index, Rect2i const &p_rect, Size2i p_fb_size) {
	// Determine the positions of the four edges of the
	// clear rectangle as a fraction of the attachment size.
	float leftPos = (float)(p_rect.position.x) / (float)p_fb_size.width;
	float rightPos = (float)(p_rect.size.width) / (float)p_fb_size.width + leftPos;
	float bottomPos = (float)(p_rect.position.y) / (float)p_fb_size.height;
	float topPos = (float)(p_rect.size.height) / (float)p_fb_size.height + bottomPos;

	// Transform to clip-space coordinates, which are bounded by (-1.0 < p < 1.0) in clip-space.
	leftPos = (leftPos * 2.0f) - 1.0f;
	rightPos = (rightPos * 2.0f) - 1.0f;
	bottomPos = (bottomPos * 2.0f) - 1.0f;
	topPos = (topPos * 2.0f) - 1.0f;

	simd::float4 vtx;

	uint32_t idx = p_index;
	uint32_t endLayer = render.get_subpass().view_count;

	for (uint32_t layer = 0; layer < endLayer; layer++) {
		vtx.z = 0.0;
		vtx.w = (float)layer;

		// Top left vertex - First triangle.
		vtx.y = topPos;
		vtx.x = leftPos;
		p_vertices[idx++] = vtx;

		// Bottom left vertex.
		vtx.y = bottomPos;
		vtx.x = leftPos;
		p_vertices[idx++] = vtx;

		// Bottom right vertex.
		vtx.y = bottomPos;
		vtx.x = rightPos;
		p_vertices[idx++] = vtx;

		// Bottom right vertex - Second triangle.
		p_vertices[idx++] = vtx;

		// Top right vertex.
		vtx.y = topPos;
		vtx.x = rightPos;
		p_vertices[idx++] = vtx;

		// Top left vertex.
		vtx.y = topPos;
		vtx.x = leftPos;
		p_vertices[idx++] = vtx;
	}

	return idx;
}

void MDCommandBuffer::render_begin_pass(RDD::RenderPassID p_render_pass, RDD::FramebufferID p_frameBuffer, RDD::CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RDD::RenderPassClearValue> p_clear_values) {
	DEV_ASSERT(commandBuffer != nil);
	end();

	MDRenderPass *pass = (MDRenderPass *)(p_render_pass.id);
	MDFrameBuffer *fb = (MDFrameBuffer *)(p_frameBuffer.id);

	type = MDCommandBufferStateType::Render;
	render.pass = pass;
	render.current_subpass = UINT32_MAX;
	render.render_area = p_rect;
	render.clear_values.resize(p_clear_values.size());
	for (uint32_t i = 0; i < p_clear_values.size(); i++) {
		render.clear_values[i] = p_clear_values[i];
	}
	render.is_rendering_entire_area = (p_rect.position == Point2i(0, 0)) && p_rect.size == fb->size;
	render.frameBuffer = fb;
	render_next_subpass();
}

void MDCommandBuffer::_end_render_pass() {
	MDFrameBuffer const &fb_info = *render.frameBuffer;
	MDSubpass const &subpass = render.get_subpass();

	PixelFormats &pf = device_driver->get_pixel_formats();

	for (uint32_t i = 0; i < subpass.resolve_references.size(); i++) {
		uint32_t color_index = subpass.color_references[i].attachment;
		uint32_t resolve_index = subpass.resolve_references[i].attachment;
		DEV_ASSERT((color_index == RDD::AttachmentReference::UNUSED) == (resolve_index == RDD::AttachmentReference::UNUSED));
		if (color_index == RDD::AttachmentReference::UNUSED || !fb_info.has_texture(color_index)) {
			continue;
		}

		id<MTLTexture> resolve_tex = fb_info.get_texture(resolve_index);

		CRASH_COND_MSG(!flags::all(pf.getCapabilities(resolve_tex.pixelFormat), kMTLFmtCapsResolve), "not implemented: unresolvable texture types");
		// see: https://github.com/KhronosGroup/MoltenVK/blob/d20d13fe2735adb845636a81522df1b9d89c0fba/MoltenVK/MoltenVK/GPUObjects/MVKRenderPass.mm#L407
	}

	render.end_encoding();
}

void MDCommandBuffer::_render_clear_render_area() {
	MDRenderPass const &pass = *render.pass;
	MDSubpass const &subpass = render.get_subpass();

	// First determine attachments that should be cleared.
	LocalVector<RDD::AttachmentClear> clears;
	clears.reserve(subpass.color_references.size() + /* possible depth stencil clear */ 1);

	for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
		uint32_t idx = subpass.color_references[i].attachment;
		if (idx != RDD::AttachmentReference::UNUSED && pass.attachments[idx].shouldClear(subpass, false)) {
			clears.push_back({ .aspect = RDD::TEXTURE_ASPECT_COLOR_BIT, .color_attachment = idx, .value = render.clear_values[idx] });
		}
	}
	uint32_t ds_index = subpass.depth_stencil_reference.attachment;
	bool shouldClearDepth = (ds_index != RDD::AttachmentReference::UNUSED && pass.attachments[ds_index].shouldClear(subpass, false));
	bool shouldClearStencil = (ds_index != RDD::AttachmentReference::UNUSED && pass.attachments[ds_index].shouldClear(subpass, true));
	if (shouldClearDepth || shouldClearStencil) {
		MDAttachment const &attachment = pass.attachments[ds_index];
		BitField<RDD::TextureAspectBits> bits;
		if (shouldClearDepth && attachment.type & MDAttachmentType::Depth) {
			bits.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		}
		if (shouldClearStencil && attachment.type & MDAttachmentType::Stencil) {
			bits.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
		}

		clears.push_back({ .aspect = bits, .color_attachment = ds_index, .value = render.clear_values[ds_index] });
	}

	if (clears.is_empty()) {
		return;
	}

	render_clear_attachments(clears, { render.render_area });
}

void MDCommandBuffer::render_next_subpass() {
	DEV_ASSERT(commandBuffer != nil);

	if (render.current_subpass == UINT32_MAX) {
		render.current_subpass = 0;
	} else {
		_end_render_pass();
		render.current_subpass++;
	}

	MDFrameBuffer const &fb = *render.frameBuffer;
	MDRenderPass const &pass = *render.pass;
	MDSubpass const &subpass = render.get_subpass();

	MTLRenderPassDescriptor *desc = MTLRenderPassDescriptor.renderPassDescriptor;

	if (subpass.view_count > 1) {
		desc.renderTargetArrayLength = subpass.view_count;
	}

	PixelFormats &pf = device_driver->get_pixel_formats();

	uint32_t attachmentCount = 0;
	for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
		uint32_t idx = subpass.color_references[i].attachment;
		if (idx == RDD::AttachmentReference::UNUSED) {
			continue;
		}

		attachmentCount += 1;
		MTLRenderPassColorAttachmentDescriptor *ca = desc.colorAttachments[i];

		uint32_t resolveIdx = subpass.resolve_references.is_empty() ? RDD::AttachmentReference::UNUSED : subpass.resolve_references[i].attachment;
		bool has_resolve = resolveIdx != RDD::AttachmentReference::UNUSED;
		bool can_resolve = true;
		if (resolveIdx != RDD::AttachmentReference::UNUSED) {
			id<MTLTexture> resolve_tex = fb.get_texture(resolveIdx);
			can_resolve = flags::all(pf.getCapabilities(resolve_tex.pixelFormat), kMTLFmtCapsResolve);
			if (can_resolve) {
				ca.resolveTexture = resolve_tex;
			} else {
				CRASH_NOW_MSG("unimplemented: using a texture format that is not supported for resolve");
			}
		}

		MDAttachment const &attachment = pass.attachments[idx];

		id<MTLTexture> tex = fb.get_texture(idx);
		ERR_FAIL_NULL_MSG(tex, "Frame buffer color texture is null.");

		if ((attachment.type & MDAttachmentType::Color)) {
			if (attachment.configureDescriptor(ca, pf, subpass, tex, render.is_rendering_entire_area, has_resolve, can_resolve, false)) {
				Color clearColor = render.clear_values[idx].color;
				ca.clearColor = MTLClearColorMake(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
			}
		}
	}

	if (subpass.depth_stencil_reference.attachment != RDD::AttachmentReference::UNUSED) {
		attachmentCount += 1;
		uint32_t idx = subpass.depth_stencil_reference.attachment;
		MDAttachment const &attachment = pass.attachments[idx];
		id<MTLTexture> tex = fb.get_texture(idx);
		ERR_FAIL_NULL_MSG(tex, "Frame buffer depth / stencil texture is null.");
		if (attachment.type & MDAttachmentType::Depth) {
			MTLRenderPassDepthAttachmentDescriptor *da = desc.depthAttachment;
			if (attachment.configureDescriptor(da, pf, subpass, tex, render.is_rendering_entire_area, false, false, false)) {
				da.clearDepth = render.clear_values[idx].depth;
			}
		}

		if (attachment.type & MDAttachmentType::Stencil) {
			MTLRenderPassStencilAttachmentDescriptor *sa = desc.stencilAttachment;
			if (attachment.configureDescriptor(sa, pf, subpass, tex, render.is_rendering_entire_area, false, false, true)) {
				sa.clearStencil = render.clear_values[idx].stencil;
			}
		}
	}

	desc.renderTargetWidth = MAX((NSUInteger)MIN(render.render_area.position.x + render.render_area.size.width, fb.size.width), 1u);
	desc.renderTargetHeight = MAX((NSUInteger)MIN(render.render_area.position.y + render.render_area.size.height, fb.size.height), 1u);

	if (attachmentCount == 0) {
		// If there are no attachments, delay the creation of the encoder,
		// so we can use a matching sample count for the pipeline, by setting
		// the defaultRasterSampleCount from the pipeline's sample count.
		render.desc = desc;
	} else {
		render.encoder = [commandBuffer renderCommandEncoderWithDescriptor:desc];

		if (!render.is_rendering_entire_area) {
			_render_clear_render_area();
		}
		// With a new encoder, all state is dirty.
		render.dirty.set_flag(RenderState::DIRTY_ALL);
	}
}

void MDCommandBuffer::render_draw(uint32_t p_vertex_count,
		uint32_t p_instance_count,
		uint32_t p_base_vertex,
		uint32_t p_first_instance) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	ERR_FAIL_NULL_MSG(render.pipeline, "No pipeline set for render command buffer.");

	_render_set_dirty_state();

	MDSubpass const &subpass = render.get_subpass();
	if (subpass.view_count > 1) {
		p_instance_count *= subpass.view_count;
	}

	DEV_ASSERT(render.dirty == 0);

	id<MTLRenderCommandEncoder> enc = render.encoder;

	[enc drawPrimitives:render.pipeline->raster_state.render_primitive
			  vertexStart:p_base_vertex
			  vertexCount:p_vertex_count
			instanceCount:p_instance_count
			 baseInstance:p_first_instance];
}

void MDCommandBuffer::render_bind_vertex_buffers(uint32_t p_binding_count, const RDD::BufferID *p_buffers, const uint64_t *p_offsets) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	render.vertex_buffers.resize(p_binding_count);
	render.vertex_offsets.resize(p_binding_count);

	// Reverse the buffers, as their bindings are assigned in descending order.
	for (uint32_t i = 0; i < p_binding_count; i += 1) {
		render.vertex_buffers[i] = rid::get(p_buffers[p_binding_count - i - 1]);
		render.vertex_offsets[i] = p_offsets[p_binding_count - i - 1];
	}

	if (render.encoder) {
		uint32_t first = device_driver->get_metal_buffer_index_for_vertex_attribute_binding(p_binding_count - 1);
		[render.encoder setVertexBuffers:render.vertex_buffers.ptr()
								 offsets:render.vertex_offsets.ptr()
							   withRange:NSMakeRange(first, p_binding_count)];
	} else {
		render.dirty.set_flag(RenderState::DIRTY_VERTEX);
	}
}

void MDCommandBuffer::render_bind_index_buffer(RDD::BufferID p_buffer, RDD::IndexBufferFormat p_format, uint64_t p_offset) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	render.index_buffer = rid::get(p_buffer);
	render.index_type = p_format == RDD::IndexBufferFormat::INDEX_BUFFER_FORMAT_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
	render.index_offset = p_offset;
}

void MDCommandBuffer::render_draw_indexed(uint32_t p_index_count,
		uint32_t p_instance_count,
		uint32_t p_first_index,
		int32_t p_vertex_offset,
		uint32_t p_first_instance) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	ERR_FAIL_NULL_MSG(render.pipeline, "No pipeline set for render command buffer.");

	_render_set_dirty_state();

	MDSubpass const &subpass = render.get_subpass();
	if (subpass.view_count > 1) {
		p_instance_count *= subpass.view_count;
	}

	id<MTLRenderCommandEncoder> enc = render.encoder;

	uint32_t index_offset = render.index_offset;
	index_offset += p_first_index * (render.index_type == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));

	[enc drawIndexedPrimitives:render.pipeline->raster_state.render_primitive
					indexCount:p_index_count
					 indexType:render.index_type
				   indexBuffer:render.index_buffer
			 indexBufferOffset:index_offset
				 instanceCount:p_instance_count
					baseVertex:p_vertex_offset
				  baseInstance:p_first_instance];
}

void MDCommandBuffer::render_draw_indexed_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	ERR_FAIL_NULL_MSG(render.pipeline, "No pipeline set for render command buffer.");

	_render_set_dirty_state();

	id<MTLRenderCommandEncoder> enc = render.encoder;

	id<MTLBuffer> indirect_buffer = rid::get(p_indirect_buffer);
	NSUInteger indirect_offset = p_offset;

	for (uint32_t i = 0; i < p_draw_count; i++) {
		[enc drawIndexedPrimitives:render.pipeline->raster_state.render_primitive
						   indexType:render.index_type
						 indexBuffer:render.index_buffer
				   indexBufferOffset:0
					  indirectBuffer:indirect_buffer
				indirectBufferOffset:indirect_offset];
		indirect_offset += p_stride;
	}
}

void MDCommandBuffer::render_draw_indexed_indirect_count(RDD::BufferID p_indirect_buffer, uint64_t p_offset, RDD::BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	ERR_FAIL_MSG("not implemented");
}

void MDCommandBuffer::render_draw_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);
	ERR_FAIL_NULL_MSG(render.pipeline, "No pipeline set for render command buffer.");

	_render_set_dirty_state();

	id<MTLRenderCommandEncoder> enc = render.encoder;

	id<MTLBuffer> indirect_buffer = rid::get(p_indirect_buffer);
	NSUInteger indirect_offset = p_offset;

	for (uint32_t i = 0; i < p_draw_count; i++) {
		[enc drawPrimitives:render.pipeline->raster_state.render_primitive
					  indirectBuffer:indirect_buffer
				indirectBufferOffset:indirect_offset];
		indirect_offset += p_stride;
	}
}

void MDCommandBuffer::render_draw_indirect_count(RDD::BufferID p_indirect_buffer, uint64_t p_offset, RDD::BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	ERR_FAIL_MSG("not implemented");
}

void MDCommandBuffer::render_end_pass() {
	DEV_ASSERT(type == MDCommandBufferStateType::Render);

	render.end_encoding();
	render.reset();
	type = MDCommandBufferStateType::None;
}

#pragma mark - RenderState

void MDCommandBuffer::RenderState::reset() {
	pass = nil;
	frameBuffer = nil;
	pipeline = nil;
	current_subpass = UINT32_MAX;
	render_area = {};
	is_rendering_entire_area = false;
	desc = nil;
	encoder = nil;
	index_buffer = nil;
	index_type = MTLIndexTypeUInt16;
	dirty = DIRTY_NONE;
	uniform_sets.clear();
	uniform_set_mask = 0;
	clear_values.clear();
	viewports.clear();
	scissors.clear();
	blend_constants.reset();
	vertex_buffers.clear();
	vertex_offsets.clear();
	// Keep the keys, as they are likely to be used again.
	for (KeyValue<StageResourceUsage, LocalVector<__unsafe_unretained id<MTLResource>>> &kv : resource_usage) {
		kv.value.clear();
	}
}

void MDCommandBuffer::RenderState::end_encoding() {
	if (encoder == nil) {
		return;
	}

	// Bind all resources.
	for (KeyValue<StageResourceUsage, ResourceVector> const &keyval : resource_usage) {
		if (keyval.value.is_empty()) {
			continue;
		}

		MTLResourceUsage vert_usage = resource_usage_for_stage(keyval.key, RDD::ShaderStage::SHADER_STAGE_VERTEX);
		MTLResourceUsage frag_usage = resource_usage_for_stage(keyval.key, RDD::ShaderStage::SHADER_STAGE_FRAGMENT);
		if (vert_usage == frag_usage) {
			[encoder useResources:keyval.value.ptr() count:keyval.value.size() usage:vert_usage stages:MTLRenderStageVertex | MTLRenderStageFragment];
		} else {
			if (vert_usage != 0) {
				[encoder useResources:keyval.value.ptr() count:keyval.value.size() usage:vert_usage stages:MTLRenderStageVertex];
			}
			if (frag_usage != 0) {
				[encoder useResources:keyval.value.ptr() count:keyval.value.size() usage:frag_usage stages:MTLRenderStageFragment];
			}
		}
	}

	[encoder endEncoding];
	encoder = nil;
}

#pragma mark - ComputeState

void MDCommandBuffer::ComputeState::end_encoding() {
	if (encoder == nil) {
		return;
	}

	// Bind all resources.
	for (KeyValue<StageResourceUsage, ResourceVector> const &keyval : resource_usage) {
		if (keyval.value.is_empty()) {
			continue;
		}
		MTLResourceUsage usage = resource_usage_for_stage(keyval.key, RDD::ShaderStage::SHADER_STAGE_COMPUTE);
		if (usage != 0) {
			[encoder useResources:keyval.value.ptr() count:keyval.value.size() usage:usage];
		}
	}

	[encoder endEncoding];
	encoder = nil;
}

#pragma mark - Compute

void MDCommandBuffer::compute_bind_uniform_set(RDD::UniformSetID p_uniform_set, RDD::ShaderID p_shader, uint32_t p_set_index) {
	DEV_ASSERT(type == MDCommandBufferStateType::Compute);

	id<MTLComputeCommandEncoder> enc = compute.encoder;
	id<MTLDevice> device = enc.device;

	MDShader *shader = (MDShader *)(p_shader.id);
	UniformSet const &set_info = shader->sets[p_set_index];

	MDUniformSet *set = (MDUniformSet *)(p_uniform_set.id);
	BoundUniformSet &bus = set->boundUniformSetForShader(shader, device);
	bus.merge_into(compute.resource_usage);

	uint32_t const *offset = set_info.offsets.getptr(RDD::SHADER_STAGE_COMPUTE);
	if (offset) {
		[enc setBuffer:bus.buffer offset:*offset atIndex:p_set_index];
	}
}

void MDCommandBuffer::compute_bind_uniform_sets(VectorView<RDD::UniformSetID> p_uniform_sets, RDD::ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) {
	DEV_ASSERT(type == MDCommandBufferStateType::Compute);

	id<MTLComputeCommandEncoder> enc = compute.encoder;
	id<MTLDevice> device = enc.device;

	MDShader *shader = (MDShader *)(p_shader.id);

	thread_local LocalVector<__unsafe_unretained id<MTLBuffer>> buffers;
	thread_local LocalVector<NSUInteger> offsets;

	buffers.resize(p_set_count);
	offsets.resize(p_set_count);

	for (size_t i = 0u; i < p_set_count; ++i) {
		UniformSet const &set_info = shader->sets[p_first_set_index + i];

		MDUniformSet *set = (MDUniformSet *)(p_uniform_sets[i].id);
		BoundUniformSet &bus = set->boundUniformSetForShader(shader, device);
		bus.merge_into(compute.resource_usage);

		uint32_t const *offset = set_info.offsets.getptr(RDD::SHADER_STAGE_COMPUTE);
		if (offset) {
			buffers[i] = bus.buffer;
			offsets[i] = *offset;
		} else {
			buffers[i] = nullptr;
			offsets[i] = 0u;
		}
	}

	[enc setBuffers:buffers.ptr() offsets:offsets.ptr() withRange:NSMakeRange(p_first_set_index, p_set_count)];
}

void MDCommandBuffer::compute_dispatch(uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	DEV_ASSERT(type == MDCommandBufferStateType::Compute);

	MTLRegion region = MTLRegionMake3D(0, 0, 0, p_x_groups, p_y_groups, p_z_groups);

	id<MTLComputeCommandEncoder> enc = compute.encoder;
	[enc dispatchThreadgroups:region.size threadsPerThreadgroup:compute.pipeline->compute_state.local];
}

void MDCommandBuffer::compute_dispatch_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset) {
	DEV_ASSERT(type == MDCommandBufferStateType::Compute);

	id<MTLBuffer> indirectBuffer = rid::get(p_indirect_buffer);

	id<MTLComputeCommandEncoder> enc = compute.encoder;
	[enc dispatchThreadgroupsWithIndirectBuffer:indirectBuffer indirectBufferOffset:p_offset threadsPerThreadgroup:compute.pipeline->compute_state.local];
}

void MDCommandBuffer::_end_compute_dispatch() {
	DEV_ASSERT(type == MDCommandBufferStateType::Compute);

	compute.end_encoding();
	compute.reset();
	type = MDCommandBufferStateType::None;
}

void MDCommandBuffer::_end_blit() {
	DEV_ASSERT(type == MDCommandBufferStateType::Blit);

	[blit.encoder endEncoding];
	blit.reset();
	type = MDCommandBufferStateType::None;
}

MDComputeShader::MDComputeShader(CharString p_name, Vector<UniformSet> p_sets, MDLibrary *p_kernel) :
		MDShader(p_name, p_sets), kernel(p_kernel) {
}

void MDComputeShader::encode_push_constant_data(VectorView<uint32_t> p_data, MDCommandBuffer *p_cb) {
	DEV_ASSERT(p_cb->type == MDCommandBufferStateType::Compute);
	if (push_constants.binding == (uint32_t)-1) {
		return;
	}

	id<MTLComputeCommandEncoder> enc = p_cb->compute.encoder;

	void const *ptr = p_data.ptr();
	size_t length = p_data.size() * sizeof(uint32_t);

	[enc setBytes:ptr length:length atIndex:push_constants.binding];
}

MDRenderShader::MDRenderShader(CharString p_name,
		bool p_needs_view_mask_buffer,
		Vector<UniformSet> p_sets,
		MDLibrary *_Nonnull p_vert, MDLibrary *_Nonnull p_frag) :
		MDShader(p_name, p_sets), needs_view_mask_buffer(p_needs_view_mask_buffer), vert(p_vert), frag(p_frag) {
}

void MDRenderShader::encode_push_constant_data(VectorView<uint32_t> p_data, MDCommandBuffer *p_cb) {
	DEV_ASSERT(p_cb->type == MDCommandBufferStateType::Render);
	id<MTLRenderCommandEncoder> enc = p_cb->render.encoder;

	void const *ptr = p_data.ptr();
	size_t length = p_data.size() * sizeof(uint32_t);

	if (push_constants.vert.binding > -1) {
		[enc setVertexBytes:ptr length:length atIndex:push_constants.vert.binding];
	}

	if (push_constants.frag.binding > -1) {
		[enc setFragmentBytes:ptr length:length atIndex:push_constants.frag.binding];
	}
}

BoundUniformSet &MDUniformSet::boundUniformSetForShader(MDShader *p_shader, id<MTLDevice> p_device) {
	BoundUniformSet *sus = bound_uniforms.getptr(p_shader);
	if (sus != nullptr) {
		return *sus;
	}

	UniformSet const &set = p_shader->sets[index];

	HashMap<id<MTLResource>, StageResourceUsage> bound_resources;
	auto add_usage = [&bound_resources](id<MTLResource> __unsafe_unretained res, RDD::ShaderStage stage, MTLResourceUsage usage) {
		StageResourceUsage *sru = bound_resources.getptr(res);
		if (sru == nullptr) {
			bound_resources.insert(res, stage_resource_usage(stage, usage));
		} else {
			*sru |= stage_resource_usage(stage, usage);
		}
	};
	id<MTLBuffer> enc_buffer = nil;
	if (set.buffer_size > 0) {
		MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeTracked;
		enc_buffer = [p_device newBufferWithLength:set.buffer_size options:options];
		for (KeyValue<RDC::ShaderStage, id<MTLArgumentEncoder>> const &kv : set.encoders) {
			RDD::ShaderStage const stage = kv.key;
			ShaderStageUsage const stage_usage = ShaderStageUsage(1 << stage);
			id<MTLArgumentEncoder> const enc = kv.value;

			[enc setArgumentBuffer:enc_buffer offset:set.offsets[stage]];

			for (uint32_t i = 0; i < uniforms.size(); i++) {
				RDD::BoundUniform const &uniform = uniforms[i];
				UniformInfo ui = set.uniforms[i];

				BindingInfo *bi = ui.bindings.getptr(stage);
				if (bi == nullptr) {
					// No binding for this stage.
					continue;
				}

				if ((ui.active_stages & stage_usage) == 0) {
					// Not active for this state, so don't bind anything.
					continue;
				}

				switch (uniform.type) {
					case RDD::UNIFORM_TYPE_SAMPLER: {
						size_t count = uniform.ids.size();
						id<MTLSamplerState> __unsafe_unretained *objects = ALLOCA_ARRAY(id<MTLSamplerState> __unsafe_unretained, count);
						for (size_t j = 0; j < count; j += 1) {
							objects[j] = rid::get(uniform.ids[j].id);
						}
						[enc setSamplerStates:objects withRange:NSMakeRange(bi->index, count)];
					} break;
					case RDD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
						size_t count = uniform.ids.size() / 2;
						id<MTLTexture> __unsafe_unretained *textures = ALLOCA_ARRAY(id<MTLTexture> __unsafe_unretained, count);
						id<MTLSamplerState> __unsafe_unretained *samplers = ALLOCA_ARRAY(id<MTLSamplerState> __unsafe_unretained, count);
						for (uint32_t j = 0; j < count; j += 1) {
							id<MTLSamplerState> sampler = rid::get(uniform.ids[j * 2 + 0]);
							id<MTLTexture> texture = rid::get(uniform.ids[j * 2 + 1]);
							samplers[j] = sampler;
							textures[j] = texture;
							add_usage(texture, stage, bi->usage);
						}
						BindingInfo *sbi = ui.bindings_secondary.getptr(stage);
						if (sbi) {
							[enc setSamplerStates:samplers withRange:NSMakeRange(sbi->index, count)];
						}
						[enc setTextures:textures
								withRange:NSMakeRange(bi->index, count)];
					} break;
					case RDD::UNIFORM_TYPE_TEXTURE: {
						size_t count = uniform.ids.size();
						if (count == 1) {
							id<MTLTexture> obj = rid::get(uniform.ids[0]);
							[enc setTexture:obj atIndex:bi->index];
							add_usage(obj, stage, bi->usage);
						} else {
							id<MTLTexture> __unsafe_unretained *objects = ALLOCA_ARRAY(id<MTLTexture> __unsafe_unretained, count);
							for (size_t j = 0; j < count; j += 1) {
								id<MTLTexture> obj = rid::get(uniform.ids[j]);
								objects[j] = obj;
								add_usage(obj, stage, bi->usage);
							}
							[enc setTextures:objects withRange:NSMakeRange(bi->index, count)];
						}
					} break;
					case RDD::UNIFORM_TYPE_IMAGE: {
						size_t count = uniform.ids.size();
						if (count == 1) {
							id<MTLTexture> obj = rid::get(uniform.ids[0]);
							[enc setTexture:obj atIndex:bi->index];
							add_usage(obj, stage, bi->usage);
							BindingInfo *sbi = ui.bindings_secondary.getptr(stage);
							if (sbi) {
								id<MTLTexture> tex = obj.parentTexture ? obj.parentTexture : obj;
								id<MTLBuffer> buf = tex.buffer;
								if (buf) {
									[enc setBuffer:buf offset:tex.bufferOffset atIndex:sbi->index];
								}
							}
						} else {
							id<MTLTexture> __unsafe_unretained *objects = ALLOCA_ARRAY(id<MTLTexture> __unsafe_unretained, count);
							for (size_t j = 0; j < count; j += 1) {
								id<MTLTexture> obj = rid::get(uniform.ids[j]);
								objects[j] = obj;
								add_usage(obj, stage, bi->usage);
							}
							[enc setTextures:objects withRange:NSMakeRange(bi->index, count)];
						}
					} break;
					case RDD::UNIFORM_TYPE_TEXTURE_BUFFER: {
						ERR_PRINT("not implemented: UNIFORM_TYPE_TEXTURE_BUFFER");
					} break;
					case RDD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
						ERR_PRINT("not implemented: UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER");
					} break;
					case RDD::UNIFORM_TYPE_IMAGE_BUFFER: {
						CRASH_NOW_MSG("not implemented: UNIFORM_TYPE_IMAGE_BUFFER");
					} break;
					case RDD::UNIFORM_TYPE_UNIFORM_BUFFER: {
						id<MTLBuffer> buffer = rid::get(uniform.ids[0]);
						[enc setBuffer:buffer offset:0 atIndex:bi->index];
						add_usage(buffer, stage, bi->usage);
					} break;
					case RDD::UNIFORM_TYPE_STORAGE_BUFFER: {
						id<MTLBuffer> buffer = rid::get(uniform.ids[0]);
						[enc setBuffer:buffer offset:0 atIndex:bi->index];
						add_usage(buffer, stage, bi->usage);
					} break;
					case RDD::UNIFORM_TYPE_INPUT_ATTACHMENT: {
						size_t count = uniform.ids.size();
						if (count == 1) {
							id<MTLTexture> obj = rid::get(uniform.ids[0]);
							[enc setTexture:obj atIndex:bi->index];
							add_usage(obj, stage, bi->usage);
						} else {
							id<MTLTexture> __unsafe_unretained *objects = ALLOCA_ARRAY(id<MTLTexture> __unsafe_unretained, count);
							for (size_t j = 0; j < count; j += 1) {
								id<MTLTexture> obj = rid::get(uniform.ids[j]);
								objects[j] = obj;
								add_usage(obj, stage, bi->usage);
							}
							[enc setTextures:objects withRange:NSMakeRange(bi->index, count)];
						}
					} break;
					default: {
						DEV_ASSERT(false);
					}
				}
			}
		}
	}

	SearchArray<__unsafe_unretained id<MTLResource>> search;
	ResourceUsageMap usage_to_resources;
	for (KeyValue<id<MTLResource>, StageResourceUsage> const &keyval : bound_resources) {
		ResourceVector *resources = usage_to_resources.getptr(keyval.value);
		if (resources == nullptr) {
			resources = &usage_to_resources.insert(keyval.value, ResourceVector())->value;
		}
		int64_t pos = search.bisect(resources->ptr(), resources->size(), keyval.key, true);
		if (pos == resources->size() || (*resources)[pos] != keyval.key) {
			resources->insert(pos, keyval.key);
		}
	}

	BoundUniformSet bs = { .buffer = enc_buffer, .usage_to_resources = usage_to_resources };
	bound_uniforms.insert(p_shader, bs);
	return bound_uniforms.get(p_shader);
}

MTLFmtCaps MDSubpass::getRequiredFmtCapsForAttachmentAt(uint32_t p_index) const {
	MTLFmtCaps caps = kMTLFmtCapsNone;

	for (RDD::AttachmentReference const &ar : input_references) {
		if (ar.attachment == p_index) {
			flags::set(caps, kMTLFmtCapsRead);
			break;
		}
	}

	for (RDD::AttachmentReference const &ar : color_references) {
		if (ar.attachment == p_index) {
			flags::set(caps, kMTLFmtCapsColorAtt);
			break;
		}
	}

	for (RDD::AttachmentReference const &ar : resolve_references) {
		if (ar.attachment == p_index) {
			flags::set(caps, kMTLFmtCapsResolve);
			break;
		}
	}

	if (depth_stencil_reference.attachment == p_index) {
		flags::set(caps, kMTLFmtCapsDSAtt);
	}

	return caps;
}

void MDAttachment::linkToSubpass(const MDRenderPass &p_pass) {
	firstUseSubpassIndex = UINT32_MAX;
	lastUseSubpassIndex = 0;

	for (MDSubpass const &subpass : p_pass.subpasses) {
		MTLFmtCaps reqCaps = subpass.getRequiredFmtCapsForAttachmentAt(index);
		if (reqCaps) {
			firstUseSubpassIndex = MIN(subpass.subpass_index, firstUseSubpassIndex);
			lastUseSubpassIndex = MAX(subpass.subpass_index, lastUseSubpassIndex);
		}
	}
}

MTLStoreAction MDAttachment::getMTLStoreAction(MDSubpass const &p_subpass,
		bool p_is_rendering_entire_area,
		bool p_has_resolve,
		bool p_can_resolve,
		bool p_is_stencil) const {
	if (!p_is_rendering_entire_area || !isLastUseOf(p_subpass)) {
		return p_has_resolve && p_can_resolve ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionStore;
	}

	switch (p_is_stencil ? stencilStoreAction : storeAction) {
		case MTLStoreActionStore:
			return p_has_resolve && p_can_resolve ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionStore;
		case MTLStoreActionDontCare:
			return p_has_resolve ? (p_can_resolve ? MTLStoreActionMultisampleResolve : MTLStoreActionStore) : MTLStoreActionDontCare;

		default:
			return MTLStoreActionStore;
	}
}

bool MDAttachment::configureDescriptor(MTLRenderPassAttachmentDescriptor *p_desc,
		PixelFormats &p_pf,
		MDSubpass const &p_subpass,
		id<MTLTexture> p_attachment,
		bool p_is_rendering_entire_area,
		bool p_has_resolve,
		bool p_can_resolve,
		bool p_is_stencil) const {
	p_desc.texture = p_attachment;

	MTLLoadAction load;
	if (!p_is_rendering_entire_area || !isFirstUseOf(p_subpass)) {
		load = MTLLoadActionLoad;
	} else {
		load = p_is_stencil ? stencilLoadAction : loadAction;
	}

	p_desc.loadAction = load;

	MTLPixelFormat mtlFmt = p_attachment.pixelFormat;
	bool isDepthFormat = p_pf.isDepthFormat(mtlFmt);
	bool isStencilFormat = p_pf.isStencilFormat(mtlFmt);
	if (isStencilFormat && !p_is_stencil && !isDepthFormat) {
		p_desc.storeAction = MTLStoreActionDontCare;
	} else {
		p_desc.storeAction = getMTLStoreAction(p_subpass, p_is_rendering_entire_area, p_has_resolve, p_can_resolve, p_is_stencil);
	}

	return load == MTLLoadActionClear;
}

bool MDAttachment::shouldClear(const MDSubpass &p_subpass, bool p_is_stencil) const {
	// If the subpass is not the first subpass to use this attachment, don't clear this attachment.
	if (p_subpass.subpass_index != firstUseSubpassIndex) {
		return false;
	}
	return (p_is_stencil ? stencilLoadAction : loadAction) == MTLLoadActionClear;
}

MDRenderPass::MDRenderPass(Vector<MDAttachment> &p_attachments, Vector<MDSubpass> &p_subpasses) :
		attachments(p_attachments), subpasses(p_subpasses) {
	for (MDAttachment &att : attachments) {
		att.linkToSubpass(*this);
	}
}

#pragma mark - Resource Factory

id<MTLFunction> MDResourceFactory::new_func(NSString *p_source, NSString *p_name, NSError **p_error) {
	@autoreleasepool {
		NSError *err = nil;
		MTLCompileOptions *options = [MTLCompileOptions new];
		id<MTLDevice> device = device_driver->get_device();
		id<MTLLibrary> mtlLib = [device newLibraryWithSource:p_source
													 options:options
													   error:&err];
		if (err) {
			if (p_error != nil) {
				*p_error = err;
			}
		}
		return [mtlLib newFunctionWithName:p_name];
	}
}

id<MTLFunction> MDResourceFactory::new_clear_vert_func(ClearAttKey &p_key) {
	@autoreleasepool {
		NSString *msl = [NSString stringWithFormat:@R"(
#include <metal_stdlib>
using namespace metal;

typedef struct {
    float4 a_position [[attribute(0)]];
} AttributesPos;

typedef struct {
    float4 colors[9];
} ClearColorsIn;

typedef struct {
    float4 v_position [[position]];
    uint layer%s;
} VaryingsPos;

vertex VaryingsPos vertClear(AttributesPos attributes [[stage_in]], constant ClearColorsIn& ccIn [[buffer(0)]]) {
    VaryingsPos varyings;
    varyings.v_position = float4(attributes.a_position.x, -attributes.a_position.y, ccIn.colors[%d].r, 1.0);
    varyings.layer = uint(attributes.a_position.w);
    return varyings;
}
)", p_key.is_layered_rendering_enabled() ? " [[render_target_array_index]]" : "", ClearAttKey::DEPTH_INDEX];

		return new_func(msl, @"vertClear", nil);
	}
}

id<MTLFunction> MDResourceFactory::new_clear_frag_func(ClearAttKey &p_key) {
	@autoreleasepool {
		NSMutableString *msl = [NSMutableString stringWithCapacity:2048];

		[msl appendFormat:@R"(
#include <metal_stdlib>
using namespace metal;

typedef struct {
    float4 v_position [[position]];
} VaryingsPos;

typedef struct {
    float4 colors[9];
} ClearColorsIn;

typedef struct {
)"];

		for (uint32_t caIdx = 0; caIdx < ClearAttKey::COLOR_COUNT; caIdx++) {
			if (p_key.is_enabled(caIdx)) {
				NSString *typeStr = get_format_type_string((MTLPixelFormat)p_key.pixel_formats[caIdx]);
				[msl appendFormat:@"    %@4 color%u [[color(%u)]];\n", typeStr, caIdx, caIdx];
			}
		}
		[msl appendFormat:@R"(} ClearColorsOut;

fragment ClearColorsOut fragClear(VaryingsPos varyings [[stage_in]], constant ClearColorsIn& ccIn [[buffer(0)]]) {

    ClearColorsOut ccOut;
)"];
		for (uint32_t caIdx = 0; caIdx < ClearAttKey::COLOR_COUNT; caIdx++) {
			if (p_key.is_enabled(caIdx)) {
				NSString *typeStr = get_format_type_string((MTLPixelFormat)p_key.pixel_formats[caIdx]);
				[msl appendFormat:@"    ccOut.color%u = %@4(ccIn.colors[%u]);\n", caIdx, typeStr, caIdx];
			}
		}
		[msl appendString:@R"(    return ccOut;
})"];

		return new_func(msl, @"fragClear", nil);
	}
}

NSString *MDResourceFactory::get_format_type_string(MTLPixelFormat p_fmt) {
	switch (device_driver->get_pixel_formats().getFormatType(p_fmt)) {
		case MTLFormatType::ColorInt8:
		case MTLFormatType::ColorInt16:
			return @"short";
		case MTLFormatType::ColorUInt8:
		case MTLFormatType::ColorUInt16:
			return @"ushort";
		case MTLFormatType::ColorInt32:
			return @"int";
		case MTLFormatType::ColorUInt32:
			return @"uint";
		case MTLFormatType::ColorHalf:
			return @"half";
		case MTLFormatType::ColorFloat:
		case MTLFormatType::DepthStencil:
		case MTLFormatType::Compressed:
			return @"float";
		case MTLFormatType::None:
			return @"unexpected_MTLPixelFormatInvalid";
	}
}

id<MTLDepthStencilState> MDResourceFactory::new_depth_stencil_state(bool p_use_depth, bool p_use_stencil) {
	MTLDepthStencilDescriptor *dsDesc = [MTLDepthStencilDescriptor new];
	dsDesc.depthCompareFunction = MTLCompareFunctionAlways;
	dsDesc.depthWriteEnabled = p_use_depth;

	if (p_use_stencil) {
		MTLStencilDescriptor *sDesc = [MTLStencilDescriptor new];
		sDesc.stencilCompareFunction = MTLCompareFunctionAlways;
		sDesc.stencilFailureOperation = MTLStencilOperationReplace;
		sDesc.depthFailureOperation = MTLStencilOperationReplace;
		sDesc.depthStencilPassOperation = MTLStencilOperationReplace;

		dsDesc.frontFaceStencil = sDesc;
		dsDesc.backFaceStencil = sDesc;
	} else {
		dsDesc.frontFaceStencil = nil;
		dsDesc.backFaceStencil = nil;
	}

	return [device_driver->get_device() newDepthStencilStateWithDescriptor:dsDesc];
}

id<MTLRenderPipelineState> MDResourceFactory::new_clear_pipeline_state(ClearAttKey &p_key, NSError **p_error) {
	PixelFormats &pixFmts = device_driver->get_pixel_formats();

	id<MTLFunction> vtxFunc = new_clear_vert_func(p_key);
	id<MTLFunction> fragFunc = new_clear_frag_func(p_key);
	MTLRenderPipelineDescriptor *plDesc = [MTLRenderPipelineDescriptor new];
	plDesc.label = @"ClearRenderAttachments";
	plDesc.vertexFunction = vtxFunc;
	plDesc.fragmentFunction = fragFunc;
	plDesc.rasterSampleCount = p_key.sample_count;
	plDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;

	for (uint32_t caIdx = 0; caIdx < ClearAttKey::COLOR_COUNT; caIdx++) {
		MTLRenderPipelineColorAttachmentDescriptor *colorDesc = plDesc.colorAttachments[caIdx];
		colorDesc.pixelFormat = (MTLPixelFormat)p_key.pixel_formats[caIdx];
		colorDesc.writeMask = p_key.is_enabled(caIdx) ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
	}

	MTLPixelFormat mtlDepthFormat = p_key.depth_format();
	if (pixFmts.isDepthFormat(mtlDepthFormat)) {
		plDesc.depthAttachmentPixelFormat = mtlDepthFormat;
	}

	MTLPixelFormat mtlStencilFormat = p_key.stencil_format();
	if (pixFmts.isStencilFormat(mtlStencilFormat)) {
		plDesc.stencilAttachmentPixelFormat = mtlStencilFormat;
	}

	MTLVertexDescriptor *vtxDesc = plDesc.vertexDescriptor;

	// Vertex attribute descriptors.
	MTLVertexAttributeDescriptorArray *vaDescArray = vtxDesc.attributes;
	MTLVertexAttributeDescriptor *vaDesc;
	NSUInteger vtxBuffIdx = device_driver->get_metal_buffer_index_for_vertex_attribute_binding(VERT_CONTENT_BUFFER_INDEX);
	NSUInteger vtxStride = 0;

	// Vertex location.
	vaDesc = vaDescArray[0];
	vaDesc.format = MTLVertexFormatFloat4;
	vaDesc.bufferIndex = vtxBuffIdx;
	vaDesc.offset = vtxStride;
	vtxStride += sizeof(simd::float4);

	// Vertex attribute buffer.
	MTLVertexBufferLayoutDescriptorArray *vbDescArray = vtxDesc.layouts;
	MTLVertexBufferLayoutDescriptor *vbDesc = vbDescArray[vtxBuffIdx];
	vbDesc.stepFunction = MTLVertexStepFunctionPerVertex;
	vbDesc.stepRate = 1;
	vbDesc.stride = vtxStride;

	return [device_driver->get_device() newRenderPipelineStateWithDescriptor:plDesc error:p_error];
}

id<MTLRenderPipelineState> MDResourceCache::get_clear_render_pipeline_state(ClearAttKey &p_key, NSError **p_error) {
	HashMap::ConstIterator it = clear_states.find(p_key);
	if (it != clear_states.end()) {
		return it->value;
	}

	id<MTLRenderPipelineState> state = resource_factory->new_clear_pipeline_state(p_key, p_error);
	clear_states[p_key] = state;
	return state;
}

id<MTLDepthStencilState> MDResourceCache::get_depth_stencil_state(bool p_use_depth, bool p_use_stencil) {
	id<MTLDepthStencilState> __strong *val;
	if (p_use_depth && p_use_stencil) {
		val = &clear_depth_stencil_state.all;
	} else if (p_use_depth) {
		val = &clear_depth_stencil_state.depth_only;
	} else if (p_use_stencil) {
		val = &clear_depth_stencil_state.stencil_only;
	} else {
		val = &clear_depth_stencil_state.none;
	}
	DEV_ASSERT(val != nullptr);

	if (*val == nil) {
		*val = resource_factory->new_depth_stencil_state(p_use_depth, p_use_stencil);
	}
	return *val;
}

static const char *SHADER_STAGE_NAMES[] = {
	[RD::SHADER_STAGE_VERTEX] = "vert",
	[RD::SHADER_STAGE_FRAGMENT] = "frag",
	[RD::SHADER_STAGE_TESSELATION_CONTROL] = "tess_ctrl",
	[RD::SHADER_STAGE_TESSELATION_EVALUATION] = "tess_eval",
	[RD::SHADER_STAGE_COMPUTE] = "comp",
};

void ShaderCacheEntry::notify_free() const {
	owner.shader_cache_free_entry(key);
}

@interface MDLibrary ()
- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry;
@end

/// Loads the MTLLibrary when the library is first accessed.
@interface MDLazyLibrary : MDLibrary {
	id<MTLLibrary> _library;
	NSError *_error;
	std::shared_mutex _mu;
	bool _loaded;
	id<MTLDevice> _device;
	NSString *_source;
	MTLCompileOptions *_options;
}
- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry
							device:(id<MTLDevice>)device
							source:(NSString *)source
						   options:(MTLCompileOptions *)options;
@end

/// Loads the MTLLibrary immediately on initialization, using an asynchronous API.
@interface MDImmediateLibrary : MDLibrary {
	id<MTLLibrary> _library;
	NSError *_error;
	std::mutex _cv_mutex;
	std::condition_variable _cv;
	std::atomic<bool> _complete;
	bool _ready;
}
- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry
							device:(id<MTLDevice>)device
							source:(NSString *)source
						   options:(MTLCompileOptions *)options;
@end

@implementation MDLibrary

+ (instancetype)newLibraryWithCacheEntry:(ShaderCacheEntry *)entry
								  device:(id<MTLDevice>)device
								  source:(NSString *)source
								 options:(MTLCompileOptions *)options
								strategy:(ShaderLoadStrategy)strategy {
	switch (strategy) {
		case ShaderLoadStrategy::DEFAULT:
			[[fallthrough]];
		default:
			return [[MDImmediateLibrary alloc] initWithCacheEntry:entry device:device source:source options:options];
		case ShaderLoadStrategy::LAZY:
			return [[MDLazyLibrary alloc] initWithCacheEntry:entry device:device source:source options:options];
	}
}

- (id<MTLLibrary>)library {
	CRASH_NOW_MSG("Not implemented");
	return nil;
}

- (NSError *)error {
	CRASH_NOW_MSG("Not implemented");
	return nil;
}

- (void)setLabel:(NSString *)label {
}

- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry {
	self = [super init];
	_entry = entry;
	_entry->library = self;
	return self;
}

- (void)dealloc {
	_entry->notify_free();
}

@end

@implementation MDImmediateLibrary

- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry
							device:(id<MTLDevice>)device
							source:(NSString *)source
						   options:(MTLCompileOptions *)options {
	self = [super initWithCacheEntry:entry];
	_complete = false;
	_ready = false;

	__block os_signpost_id_t compile_id = (os_signpost_id_t)(uintptr_t)self;
	os_signpost_interval_begin(LOG_INTERVALS, compile_id, "shader_compile",
			"shader_name=%{public}s stage=%{public}s hash=%X",
			entry->name.get_data(), SHADER_STAGE_NAMES[entry->stage], entry->key.short_sha());

	[device newLibraryWithSource:source
						 options:options
			   completionHandler:^(id<MTLLibrary> library, NSError *error) {
				   os_signpost_interval_end(LOG_INTERVALS, compile_id, "shader_compile");
				   self->_library = library;
				   self->_error = error;
				   if (error) {
					   ERR_PRINT(vformat(U"Error compiling shader %s: %s", entry->name.get_data(), error.localizedDescription.UTF8String));
				   }

				   {
					   std::lock_guard<std::mutex> lock(self->_cv_mutex);
					   _ready = true;
				   }
				   _cv.notify_all();
				   _complete = true;
			   }];
	return self;
}

- (id<MTLLibrary>)library {
	if (!_complete) {
		std::unique_lock<std::mutex> lock(_cv_mutex);
		_cv.wait(lock, [&] { return _ready; });
	}
	return _library;
}

- (NSError *)error {
	if (!_complete) {
		std::unique_lock<std::mutex> lock(_cv_mutex);
		_cv.wait(lock, [&] { return _ready; });
	}
	return _error;
}

@end

@implementation MDLazyLibrary
- (instancetype)initWithCacheEntry:(ShaderCacheEntry *)entry
							device:(id<MTLDevice>)device
							source:(NSString *)source
						   options:(MTLCompileOptions *)options {
	self = [super initWithCacheEntry:entry];
	_device = device;
	_source = source;
	_options = options;

	return self;
}

- (void)load {
	{
		std::shared_lock<std::shared_mutex> lock(_mu);
		if (_loaded) {
			return;
		}
	}

	std::unique_lock<std::shared_mutex> lock(_mu);
	if (_loaded) {
		return;
	}

	__block os_signpost_id_t compile_id = (os_signpost_id_t)(uintptr_t)self;
	os_signpost_interval_begin(LOG_INTERVALS, compile_id, "shader_compile",
			"shader_name=%{public}s stage=%{public}s hash=%X",
			_entry->name.get_data(), SHADER_STAGE_NAMES[_entry->stage], _entry->key.short_sha());
	NSError *error;
	_library = [_device newLibraryWithSource:_source options:_options error:&error];
	os_signpost_interval_end(LOG_INTERVALS, compile_id, "shader_compile");
	_device = nil;
	_source = nil;
	_options = nil;
	_loaded = true;
}

- (id<MTLLibrary>)library {
	[self load];
	return _library;
}

- (NSError *)error {
	[self load];
	return _error;
}

@end
