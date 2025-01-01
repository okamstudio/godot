/**************************************************************************/
/*  rendering_device_graph.h                                              */
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

#ifndef RENDERING_DEVICE_GRAPH_H
#define RENDERING_DEVICE_GRAPH_H

#include "core/object/worker_thread_pool.h"
#include "rendering_device_commons.h"
#include "rendering_device_driver.h"

// Buffer barriers have not shown any significant improvement or shown to be
// even detrimental to performance. However, there are currently some known
// cases where using them can solve problems that using singular memory
// barriers does not, probably due to driver issues (see comment on PR #84976
// https://github.com/godotengine/godot/pull/84976#issuecomment-1878566830).

#define USE_BUFFER_BARRIERS 1

class RenderingDeviceGraph {
public:
	struct ComputeListInstruction {
		enum Type {
			TYPE_NONE,
			TYPE_BIND_PIPELINE,
			TYPE_BIND_UNIFORM_SETS,
			TYPE_DISPATCH,
			TYPE_DISPATCH_INDIRECT,
			TYPE_SET_PUSH_CONSTANT,
			TYPE_UNIFORM_SET_PREPARE_FOR_USE
		};

		Type type = TYPE_NONE;
	};

	struct DrawListInstruction {
		enum Type {
			TYPE_NONE,
			TYPE_BIND_INDEX_BUFFER,
			TYPE_BIND_PIPELINE,
			TYPE_BIND_UNIFORM_SETS,
			TYPE_BIND_VERTEX_BUFFERS,
			TYPE_CLEAR_ATTACHMENTS,
			TYPE_DRAW,
			TYPE_DRAW_INDEXED,
			TYPE_DRAW_INDIRECT,
			TYPE_DRAW_INDEXED_INDIRECT,
			TYPE_EXECUTE_COMMANDS,
			TYPE_NEXT_SUBPASS,
			TYPE_SET_BLEND_CONSTANTS,
			TYPE_SET_LINE_WIDTH,
			TYPE_SET_PUSH_CONSTANT,
			TYPE_SET_SCISSOR,
			TYPE_SET_VIEWPORT,
			TYPE_UNIFORM_SET_PREPARE_FOR_USE
		};

		Type type = TYPE_NONE;
	};

	struct RecordedCommand {
		enum Type {
			TYPE_NONE,
			TYPE_BUFFER_CLEAR,
			TYPE_BUFFER_COPY,
			TYPE_BUFFER_GET_DATA,
			TYPE_BUFFER_UPDATE,
			TYPE_COMPUTE_LIST,
			TYPE_DRAW_LIST,
			TYPE_TEXTURE_CLEAR,
			TYPE_TEXTURE_COPY,
			TYPE_TEXTURE_GET_DATA,
			TYPE_TEXTURE_RESOLVE,
			TYPE_TEXTURE_UPDATE,
			TYPE_CAPTURE_TIMESTAMP,
			TYPE_MAX
		};

		Type type = TYPE_NONE;
		int32_t adjacent_command_list_index = -1;
		RDD::MemoryBarrier memory_barrier;
		int32_t normalization_barrier_index = -1;
		int normalization_barrier_count = 0;
		int32_t transition_barrier_index = -1;
		int32_t transition_barrier_count = 0;
#if USE_BUFFER_BARRIERS
		int32_t buffer_barrier_index = -1;
		int32_t buffer_barrier_count = 0;
#endif
		int32_t label_index = -1;
		BitField<RDD::PipelineStageBits> previous_stages;
		BitField<RDD::PipelineStageBits> next_stages;
		BitField<RDD::PipelineStageBits> self_stages;
	};

	struct RecordedBufferCopy {
		RDD::BufferID source;
		RDD::BufferCopyRegion region;
	};

	struct RecordedBufferToTextureCopy {
		RDD::BufferID from_buffer;
		RDD::BufferTextureCopyRegion region;
	};

	enum ResourceUsage {
		RESOURCE_USAGE_NONE,
		RESOURCE_USAGE_COPY_FROM,
		RESOURCE_USAGE_COPY_TO,
		RESOURCE_USAGE_RESOLVE_FROM,
		RESOURCE_USAGE_RESOLVE_TO,
		RESOURCE_USAGE_UNIFORM_BUFFER_READ,
		RESOURCE_USAGE_INDIRECT_BUFFER_READ,
		RESOURCE_USAGE_TEXTURE_BUFFER_READ,
		RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE,
		RESOURCE_USAGE_STORAGE_BUFFER_READ,
		RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE,
		RESOURCE_USAGE_VERTEX_BUFFER_READ,
		RESOURCE_USAGE_INDEX_BUFFER_READ,
		RESOURCE_USAGE_TEXTURE_SAMPLE,
		RESOURCE_USAGE_STORAGE_IMAGE_READ,
		RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE,
		RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE,
		RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE
	};

	struct ResourceTracker {
		uint32_t reference_count = 0;
		int64_t command_frame = -1;
		BitField<RDD::PipelineStageBits> previous_frame_stages;
		BitField<RDD::PipelineStageBits> current_frame_stages;
		int32_t read_full_command_list_index = -1;
		int32_t read_slice_command_list_index = -1;
		int32_t write_command_or_list_index = -1;
		int32_t draw_list_index = -1;
		ResourceUsage draw_list_usage = RESOURCE_USAGE_NONE;
		int32_t compute_list_index = -1;
		ResourceUsage compute_list_usage = RESOURCE_USAGE_NONE;
		ResourceUsage usage = RESOURCE_USAGE_NONE;
		BitField<RDD::BarrierAccessBits> usage_access;
		RDD::BufferID buffer_driver_id;
		RDD::TextureID texture_driver_id;
		RDD::TextureSubresourceRange texture_subresources;
		Size2i texture_size;
		uint32_t texture_usage = 0;
		int32_t texture_slice_command_index = -1;
		ResourceTracker *parent = nullptr;
		ResourceTracker *dirty_shared_list = nullptr;
		ResourceTracker *next_shared = nullptr;
		Rect2i texture_slice_or_dirty_rect;
		bool in_parent_dirty_list = false;
		bool write_command_list_enabled = false;
		bool is_discardable = false;

		_FORCE_INLINE_ void reset_if_outdated(int64_t new_command_frame) {
			if (new_command_frame != command_frame) {
				command_frame = new_command_frame;
				previous_frame_stages = current_frame_stages;
				current_frame_stages.clear();
				read_full_command_list_index = -1;
				read_slice_command_list_index = -1;
				write_command_or_list_index = -1;
				draw_list_index = -1;
				compute_list_index = -1;
				texture_slice_command_index = -1;
				write_command_list_enabled = false;
			}
		}
	};

	typedef RDD::RenderPassID (*RenderPassCreationFunction)(RenderingDeviceDriver *p_driver, VectorView<RDD::AttachmentLoadOp> p_load_ops, VectorView<RDD::AttachmentStoreOp> p_store_ops, void *p_user_data);

	struct FramebufferStorage {
		RDD::FramebufferID framebuffer;
		RDD::RenderPassID render_pass;
	};

	struct FramebufferCache {
		uint32_t width = 0;
		uint32_t height = 0;
		LocalVector<RDD::TextureID> textures;
		LocalVector<ResourceTracker *> trackers;
		HashMap<uint64_t, FramebufferStorage> storage_map;
		void *render_pass_creation_user_data = nullptr;
	};

	struct CommandBufferPool {
		// Provided by RenderingDevice.
		RDD::CommandPoolID pool;

		// Created internally by RenderingDeviceGraph.
		LocalVector<RDD::CommandBufferID> buffers;
		LocalVector<RDD::SemaphoreID> semaphores;
		uint32_t buffers_used = 0;
	};

	struct WorkaroundsState {
		bool draw_list_found = false;
	};

	enum AttachmentOperation {
		// Loads or ignores if the attachment is discardable.
		ATTACHMENT_OPERATION_DEFAULT,
		// Clear the attachment to a value.
		ATTACHMENT_OPERATION_CLEAR,
		// Ignore any contents from the attachment.
		ATTACHMENT_OPERATION_IGNORE,
	};

private:
	struct InstructionList {
		LocalVector<uint8_t> data;
		LocalVector<ResourceTracker *> command_trackers;
		LocalVector<ResourceUsage> command_tracker_usages;
		BitField<RDD::PipelineStageBits> stages;
		int32_t index = 0;

		void clear() {
			data.clear();
			command_trackers.clear();
			command_tracker_usages.clear();
			stages.clear();
		}
	};

	struct ComputeInstructionList : InstructionList {
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		uint32_t breadcrumb;
#endif
	};

	struct DrawInstructionList : InstructionList {
		FramebufferCache *framebuffer_cache = nullptr;
		RDD::RenderPassID render_pass;
		RDD::FramebufferID framebuffer;
		Rect2i region;
		LocalVector<AttachmentOperation> attachment_operations;
		LocalVector<RDD::RenderPassClearValue> attachment_clear_values;

#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		uint32_t breadcrumb;
#endif
		bool split_cmd_buffer = false;
	};

	struct RecordedCommandSort {
		uint32_t level = 0;
		uint32_t priority = 0;
		int32_t index = -1;

		RecordedCommandSort() = default;

		bool operator<(const RecordedCommandSort &p_other) const {
			if (level < p_other.level) {
				return true;
			} else if (level > p_other.level) {
				return false;
			}

			if (priority < p_other.priority) {
				return true;
			} else if (priority > p_other.priority) {
				return false;
			}

			return index < p_other.index;
		}
	};

	struct RecordedCommandListNode {
		int32_t command_index = -1;
		int32_t next_list_index = -1;
	};

	struct RecordedSliceListNode {
		int32_t command_index = -1;
		int32_t next_list_index = -1;
		Rect2i subresources;
		bool partial_coverage = false;
	};

	struct RecordedBufferClearCommand : RecordedCommand {
		RDD::BufferID buffer;
		uint32_t offset = 0;
		uint32_t size = 0;
	};

	struct RecordedBufferCopyCommand : RecordedCommand {
		RDD::BufferID source;
		RDD::BufferID destination;
		RDD::BufferCopyRegion region;
	};

	struct RecordedBufferGetDataCommand : RecordedCommand {
		RDD::BufferID source;
		RDD::BufferID destination;
		RDD::BufferCopyRegion region;
	};

	struct RecordedBufferUpdateCommand : RecordedCommand {
		RDD::BufferID destination;
		uint32_t buffer_copies_count = 0;

		_FORCE_INLINE_ RecordedBufferCopy *buffer_copies() {
			return reinterpret_cast<RecordedBufferCopy *>(&this[1]);
		}

		_FORCE_INLINE_ const RecordedBufferCopy *buffer_copies() const {
			return reinterpret_cast<const RecordedBufferCopy *>(&this[1]);
		}
	};

	struct RecordedComputeListCommand : RecordedCommand {
		uint32_t instruction_data_size = 0;
		uint32_t breadcrumb = 0;

		_FORCE_INLINE_ uint8_t *instruction_data() {
			return reinterpret_cast<uint8_t *>(&this[1]);
		}

		_FORCE_INLINE_ const uint8_t *instruction_data() const {
			return reinterpret_cast<const uint8_t *>(&this[1]);
		}
	};

	struct RecordedDrawListCommand : RecordedCommand {
		FramebufferCache *framebuffer_cache = nullptr;
		RDD::FramebufferID framebuffer;
		RDD::RenderPassID render_pass;
		uint32_t instruction_data_size = 0;
		RDD::CommandBufferType command_buffer_type;
		Rect2i region;
		uint32_t clear_values_count = 0;
		uint32_t trackers_count = 0;

#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		uint32_t breadcrumb = 0;
#endif
		bool split_cmd_buffer = false;

		_FORCE_INLINE_ RDD::RenderPassClearValue *clear_values() {
			return reinterpret_cast<RDD::RenderPassClearValue *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::RenderPassClearValue *clear_values() const {
			return reinterpret_cast<const RDD::RenderPassClearValue *>(&this[1]);
		}

		_FORCE_INLINE_ ResourceTracker **trackers() {
			return reinterpret_cast<ResourceTracker **>(&clear_values()[clear_values_count]);
		}

		_FORCE_INLINE_ ResourceTracker *const *trackers() const {
			return reinterpret_cast<ResourceTracker *const *>(&clear_values()[clear_values_count]);
		}

		_FORCE_INLINE_ RDD::AttachmentLoadOp *load_ops() {
			return reinterpret_cast<RDD::AttachmentLoadOp *>(&trackers()[trackers_count]);
		}

		_FORCE_INLINE_ const RDD::AttachmentLoadOp *load_ops() const {
			return reinterpret_cast<const RDD::AttachmentLoadOp *>(&trackers()[trackers_count]);
		}

		_FORCE_INLINE_ RDD::AttachmentStoreOp *store_ops() {
			return reinterpret_cast<RDD::AttachmentStoreOp *>(&load_ops()[trackers_count]);
		}

		_FORCE_INLINE_ const RDD::AttachmentStoreOp *store_ops() const {
			return reinterpret_cast<const RDD::AttachmentStoreOp *>(&load_ops()[trackers_count]);
		}

		_FORCE_INLINE_ uint8_t *instruction_data() {
			return reinterpret_cast<uint8_t *>(&store_ops()[trackers_count]);
		}

		_FORCE_INLINE_ const uint8_t *instruction_data() const {
			return reinterpret_cast<const uint8_t *>(&store_ops()[trackers_count]);
		}
	};

	struct RecordedTextureClearCommand : RecordedCommand {
		RDD::TextureID texture;
		RDD::TextureSubresourceRange range;
		Color color;
	};

	struct RecordedTextureCopyCommand : RecordedCommand {
		RDD::TextureID from_texture;
		RDD::TextureID to_texture;
		uint32_t texture_copy_regions_count = 0;

		_FORCE_INLINE_ RDD::TextureCopyRegion *texture_copy_regions() {
			return reinterpret_cast<RDD::TextureCopyRegion *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::TextureCopyRegion *texture_copy_regions() const {
			return reinterpret_cast<const RDD::TextureCopyRegion *>(&this[1]);
		}
	};

	struct RecordedTextureGetDataCommand : RecordedCommand {
		RDD::TextureID from_texture;
		RDD::BufferID to_buffer;
		uint32_t buffer_texture_copy_regions_count = 0;

		_FORCE_INLINE_ RDD::BufferTextureCopyRegion *buffer_texture_copy_regions() {
			return reinterpret_cast<RDD::BufferTextureCopyRegion *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::BufferTextureCopyRegion *buffer_texture_copy_regions() const {
			return reinterpret_cast<const RDD::BufferTextureCopyRegion *>(&this[1]);
		}
	};

	struct RecordedTextureResolveCommand : RecordedCommand {
		RDD::TextureID from_texture;
		RDD::TextureID to_texture;
		uint32_t src_layer = 0;
		uint32_t src_mipmap = 0;
		uint32_t dst_layer = 0;
		uint32_t dst_mipmap = 0;
	};

	struct RecordedTextureUpdateCommand : RecordedCommand {
		RDD::TextureID to_texture;
		uint32_t buffer_to_texture_copies_count = 0;

		_FORCE_INLINE_ RecordedBufferToTextureCopy *buffer_to_texture_copies() {
			return reinterpret_cast<RecordedBufferToTextureCopy *>(&this[1]);
		}

		_FORCE_INLINE_ const RecordedBufferToTextureCopy *buffer_to_texture_copies() const {
			return reinterpret_cast<const RecordedBufferToTextureCopy *>(&this[1]);
		}
	};

	struct RecordedCaptureTimestampCommand : RecordedCommand {
		RDD::QueryPoolID pool;
		uint32_t index = 0;
	};

	struct DrawListBindIndexBufferInstruction : DrawListInstruction {
		RDD::BufferID buffer;
		RenderingDeviceCommons::IndexBufferFormat format;
		uint32_t offset = 0;
	};

	struct DrawListBindPipelineInstruction : DrawListInstruction {
		RDD::PipelineID pipeline;
	};

	struct DrawListBindUniformSetsInstruction : DrawListInstruction {
		RDD::ShaderID shader;
		uint32_t first_set_index = 0;
		uint32_t set_count = 0;

		_FORCE_INLINE_ RDD::UniformSetID *uniform_set_ids() {
			return reinterpret_cast<RDD::UniformSetID *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::UniformSetID *uniform_set_ids() const {
			return reinterpret_cast<const RDD::UniformSetID *>(&this[1]);
		}
	};

	struct DrawListBindVertexBuffersInstruction : DrawListInstruction {
		uint32_t vertex_buffers_count = 0;

		_FORCE_INLINE_ RDD::BufferID *vertex_buffers() {
			return reinterpret_cast<RDD::BufferID *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::BufferID *vertex_buffers() const {
			return reinterpret_cast<const RDD::BufferID *>(&this[1]);
		}

		_FORCE_INLINE_ uint64_t *vertex_buffer_offsets() {
			return reinterpret_cast<uint64_t *>(&vertex_buffers()[vertex_buffers_count]);
		}

		_FORCE_INLINE_ const uint64_t *vertex_buffer_offsets() const {
			return reinterpret_cast<const uint64_t *>(&vertex_buffers()[vertex_buffers_count]);
		}
	};

	struct DrawListClearAttachmentsInstruction : DrawListInstruction {
		uint32_t attachments_clear_count = 0;
		uint32_t attachments_clear_rect_count = 0;

		_FORCE_INLINE_ RDD::AttachmentClear *attachments_clear() {
			return reinterpret_cast<RDD::AttachmentClear *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::AttachmentClear *attachments_clear() const {
			return reinterpret_cast<const RDD::AttachmentClear *>(&this[1]);
		}

		_FORCE_INLINE_ Rect2i *attachments_clear_rect() {
			return reinterpret_cast<Rect2i *>(&attachments_clear()[attachments_clear_count]);
		}

		_FORCE_INLINE_ const Rect2i *attachments_clear_rect() const {
			return reinterpret_cast<const Rect2i *>(&attachments_clear()[attachments_clear_count]);
		}
	};

	struct DrawListDrawInstruction : DrawListInstruction {
		uint32_t vertex_count = 0;
		uint32_t instance_count = 0;
	};

	struct DrawListDrawIndexedInstruction : DrawListInstruction {
		uint32_t index_count = 0;
		uint32_t instance_count = 0;
		uint32_t first_index = 0;
	};

	struct DrawListDrawIndirectInstruction : DrawListInstruction {
		RDD::BufferID buffer;
		uint32_t offset = 0;
		uint32_t draw_count = 0;
		uint32_t stride = 0;
	};

	struct DrawListDrawIndexedIndirectInstruction : DrawListInstruction {
		RDD::BufferID buffer;
		uint32_t offset = 0;
		uint32_t draw_count = 0;
		uint32_t stride = 0;
	};

	struct DrawListEndRenderPassInstruction : DrawListInstruction {
		// No contents.
	};

	struct DrawListExecuteCommandsInstruction : DrawListInstruction {
		RDD::CommandBufferID command_buffer;
	};

	struct DrawListSetPushConstantInstruction : DrawListInstruction {
		uint32_t size = 0;
		RDD::ShaderID shader;

		_FORCE_INLINE_ uint8_t *data() {
			return reinterpret_cast<uint8_t *>(&this[1]);
		}

		_FORCE_INLINE_ const uint8_t *data() const {
			return reinterpret_cast<const uint8_t *>(&this[1]);
		}
	};

	struct DrawListNextSubpassInstruction : DrawListInstruction {
		RDD::CommandBufferType command_buffer_type;
	};

	struct DrawListSetBlendConstantsInstruction : DrawListInstruction {
		Color color;
	};

	struct DrawListSetLineWidthInstruction : DrawListInstruction {
		float width;
	};

	struct DrawListSetScissorInstruction : DrawListInstruction {
		Rect2i rect;
	};

	struct DrawListSetViewportInstruction : DrawListInstruction {
		Rect2i rect;
	};

	struct DrawListUniformSetPrepareForUseInstruction : DrawListInstruction {
		RDD::UniformSetID uniform_set;
		RDD::ShaderID shader;
		uint32_t set_index = 0;
	};

	struct ComputeListBindPipelineInstruction : ComputeListInstruction {
		RDD::PipelineID pipeline;
	};

	struct ComputeListBindUniformSetsInstruction : ComputeListInstruction {
		RDD::ShaderID shader;
		uint32_t first_set_index = 0;
		uint32_t set_count = 0;

		_FORCE_INLINE_ RDD::UniformSetID *uniform_set_ids() {
			return reinterpret_cast<RDD::UniformSetID *>(&this[1]);
		}

		_FORCE_INLINE_ const RDD::UniformSetID *uniform_set_ids() const {
			return reinterpret_cast<const RDD::UniformSetID *>(&this[1]);
		}
	};

	struct ComputeListDispatchInstruction : ComputeListInstruction {
		uint32_t x_groups = 0;
		uint32_t y_groups = 0;
		uint32_t z_groups = 0;
	};

	struct ComputeListDispatchIndirectInstruction : ComputeListInstruction {
		RDD::BufferID buffer;
		uint32_t offset = 0;
	};

	struct ComputeListSetPushConstantInstruction : ComputeListInstruction {
		uint32_t size = 0;
		RDD::ShaderID shader;

		_FORCE_INLINE_ uint8_t *data() {
			return reinterpret_cast<uint8_t *>(&this[1]);
		}

		_FORCE_INLINE_ const uint8_t *data() const {
			return reinterpret_cast<const uint8_t *>(&this[1]);
		}
	};

	struct ComputeListUniformSetPrepareForUseInstruction : ComputeListInstruction {
		RDD::UniformSetID uniform_set;
		RDD::ShaderID shader;
		uint32_t set_index = 0;
	};

	struct BarrierGroup {
		BitField<RDD::PipelineStageBits> src_stages;
		BitField<RDD::PipelineStageBits> dst_stages;
		RDD::MemoryBarrier memory_barrier;
		LocalVector<RDD::TextureBarrier> normalization_barriers;
		LocalVector<RDD::TextureBarrier> transition_barriers;
#if USE_BUFFER_BARRIERS
		LocalVector<RDD::BufferBarrier> buffer_barriers;
#endif

		void clear() {
			src_stages.clear();
			dst_stages.clear();
			memory_barrier.src_access.clear();
			memory_barrier.dst_access.clear();
			normalization_barriers.clear();
			transition_barriers.clear();
#if USE_BUFFER_BARRIERS
			buffer_barriers.clear();
#endif
		}
	};

	struct SecondaryCommandBuffer {
		LocalVector<uint8_t> instruction_data;
		RDD::CommandBufferID command_buffer;
		RDD::CommandPoolID command_pool;
		RDD::RenderPassID render_pass;
		RDD::FramebufferID framebuffer;
		WorkerThreadPool::TaskID task;
	};

	struct Frame {
		TightLocalVector<SecondaryCommandBuffer> secondary_command_buffers;
		uint32_t secondary_command_buffers_used = 0;
	};

	RDD *driver = nullptr;
	RenderingContextDriver::Device device;
	RenderPassCreationFunction render_pass_creation_function = nullptr;
	int64_t tracking_frame = 0;
	LocalVector<uint8_t> command_data;
	LocalVector<uint32_t> command_data_offsets;
	LocalVector<RDD::TextureBarrier> command_normalization_barriers;
	LocalVector<RDD::TextureBarrier> command_transition_barriers;
	LocalVector<RDD::BufferBarrier> command_buffer_barriers;
	LocalVector<char> command_label_chars;
	LocalVector<Color> command_label_colors;
	LocalVector<uint32_t> command_label_offsets;
	int32_t command_label_index = -1;
	DrawInstructionList draw_instruction_list;
	ComputeInstructionList compute_instruction_list;
	uint32_t command_count = 0;
	uint32_t command_label_count = 0;
	LocalVector<RecordedCommandListNode> command_list_nodes;
	LocalVector<RecordedSliceListNode> read_slice_list_nodes;
	LocalVector<RecordedSliceListNode> write_slice_list_nodes;
	int32_t command_timestamp_index = -1;
	int32_t command_synchronization_index = -1;
	bool command_synchronization_pending = false;
	BarrierGroup barrier_group;
	bool driver_honors_barriers : 1;
	bool driver_clears_with_copy_engine : 1;
	bool driver_buffers_require_transitions : 1;
	WorkaroundsState workarounds_state;
	TightLocalVector<Frame> frames;
	uint32_t frame = 0;

#ifdef DEV_ENABLED
	RBMap<ResourceTracker *, uint32_t> write_dependency_counters;
#endif

	static String _usage_to_string(ResourceUsage p_usage);
	static bool _is_write_usage(ResourceUsage p_usage);
	static RDD::TextureLayout _usage_to_image_layout(ResourceUsage p_usage);
	static RDD::BarrierAccessBits _usage_to_access_bits(ResourceUsage p_usage);
	bool _check_command_intersection(ResourceTracker *p_resource_tracker, int32_t p_previous_command_index, int32_t p_command_index) const;
	bool _check_command_partial_coverage(ResourceTracker *p_resource_tracker, int32_t p_command_index) const;
	int32_t _add_to_command_list(int32_t p_command_index, int32_t p_list_index);
	void _add_adjacent_command(int32_t p_previous_command_index, int32_t p_command_index, RecordedCommand *r_command);
	int32_t _add_to_slice_read_list(int32_t p_command_index, Rect2i p_subresources, int32_t p_list_index);
	int32_t _add_to_write_list(int32_t p_command_index, Rect2i p_subresources, int32_t p_list_index, bool p_partial_coverage);
	RecordedCommand *_allocate_command(uint32_t p_command_size, int32_t &r_command_index);
	DrawListInstruction *_allocate_draw_list_instruction(uint32_t p_instruction_size);
	ComputeListInstruction *_allocate_compute_list_instruction(uint32_t p_instruction_size);
	void _check_discardable_attachment_dependency(ResourceTracker *p_resource_tracker, int32_t p_previous_command_index, int32_t p_command_index);
	void _add_command_to_graph(ResourceTracker **p_resource_trackers, ResourceUsage *p_resource_usages, uint32_t p_resource_count, int32_t p_command_index, RecordedCommand *r_command);
	void _add_texture_barrier_to_command(RDD::TextureID p_texture_id, BitField<RDD::BarrierAccessBits> p_src_access, BitField<RDD::BarrierAccessBits> p_dst_access, ResourceUsage p_prev_usage, ResourceUsage p_next_usage, RDD::TextureSubresourceRange p_subresources, LocalVector<RDD::TextureBarrier> &r_barrier_vector, int32_t &r_barrier_index, int32_t &r_barrier_count);
#if USE_BUFFER_BARRIERS
	void _add_buffer_barrier_to_command(RDD::BufferID p_buffer_id, BitField<RDD::BarrierAccessBits> p_src_access, BitField<RDD::BarrierAccessBits> p_dst_access, int32_t &r_barrier_index, int32_t &r_barrier_count);
#endif
	void _run_compute_list_command(RDD::CommandBufferID p_command_buffer, const uint8_t *p_instruction_data, uint32_t p_instruction_data_size);
	void _get_draw_list_render_pass_and_framebuffer(const RecordedDrawListCommand *p_draw_list_command, RDD::RenderPassID &r_render_pass, RDD::FramebufferID &r_framebuffer);
	void _run_draw_list_command(RDD::CommandBufferID p_command_buffer, const uint8_t *p_instruction_data, uint32_t p_instruction_data_size);
	void _add_draw_list_begin(FramebufferCache *p_framebuffer_cache, RDD::RenderPassID p_render_pass, RDD::FramebufferID p_framebuffer, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb, bool p_split_cmd_buffer);
	void _run_secondary_command_buffer_task(const SecondaryCommandBuffer *p_secondary);
	void _wait_for_secondary_command_buffer_tasks();
	void _run_render_commands(int32_t p_level, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, RDD::CommandBufferID &r_command_buffer, CommandBufferPool &r_command_buffer_pool, int32_t &r_current_label_index, int32_t &r_current_label_level);
	void _run_label_command_change(RDD::CommandBufferID p_command_buffer, int32_t p_new_label_index, int32_t p_new_level, bool p_ignore_previous_value, bool p_use_label_for_empty, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, int32_t &r_current_label_index, int32_t &r_current_label_level);
	void _boost_priority_for_render_commands(RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, uint32_t &r_boosted_priority);
	void _group_barriers_for_render_commands(RDD::CommandBufferID p_command_buffer, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, bool p_full_memory_barrier);
	void _print_render_commands(const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count);
	void _print_draw_list(const uint8_t *p_instruction_data, uint32_t p_instruction_data_size);
	void _print_compute_list(const uint8_t *p_instruction_data, uint32_t p_instruction_data_size);

public:
	RenderingDeviceGraph();
	~RenderingDeviceGraph();
	void initialize(RDD *p_driver, RenderingContextDriver::Device p_device, RenderPassCreationFunction p_render_pass_creation_function, uint32_t p_frame_count, RDD::CommandQueueFamilyID p_secondary_command_queue_family, uint32_t p_secondary_command_buffers_per_frame);
	void finalize();
	void begin();
	void add_buffer_clear(RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, uint32_t p_offset, uint32_t p_size);
	void add_buffer_copy(RDD::BufferID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, RDD::BufferCopyRegion p_region);
	void add_buffer_get_data(RDD::BufferID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, RDD::BufferCopyRegion p_region);
	void add_buffer_update(RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, VectorView<RecordedBufferCopy> p_buffer_copies);
	void add_compute_list_begin(RDD::BreadcrumbMarker p_phase = RDD::BreadcrumbMarker::NONE, uint32_t p_breadcrumb_data = 0);
	void add_compute_list_bind_pipeline(RDD::PipelineID p_pipeline);
	void add_compute_list_bind_uniform_set(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index);
	void add_compute_list_bind_uniform_sets(RDD::ShaderID p_shader, VectorView<RDD::UniformSetID> p_uniform_set, uint32_t p_first_set_index, uint32_t p_set_count);
	void add_compute_list_dispatch(uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups);
	void add_compute_list_dispatch_indirect(RDD::BufferID p_buffer, uint32_t p_offset);
	void add_compute_list_set_push_constant(RDD::ShaderID p_shader, const void *p_data, uint32_t p_data_size);
	void add_compute_list_uniform_set_prepare_for_use(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index);
	void add_compute_list_usage(ResourceTracker *p_tracker, ResourceUsage p_usage);
	void add_compute_list_usages(VectorView<ResourceTracker *> p_trackers, VectorView<ResourceUsage> p_usages);
	void add_compute_list_end();
	void add_draw_list_begin(FramebufferCache *p_framebuffer_cache, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb = 0, bool p_split_cmd_buffer = false);
	void add_draw_list_begin(RDD::RenderPassID p_render_pass, RDD::FramebufferID p_framebuffer, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb = 0, bool p_split_cmd_buffer = false);
	void add_draw_list_bind_index_buffer(RDD::BufferID p_buffer, RDD::IndexBufferFormat p_format, uint32_t p_offset);
	void add_draw_list_bind_pipeline(RDD::PipelineID p_pipeline, BitField<RDD::PipelineStageBits> p_pipeline_stage_bits);
	void add_draw_list_bind_uniform_set(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index);
	void add_draw_list_bind_uniform_sets(RDD::ShaderID p_shader, VectorView<RDD::UniformSetID> p_uniform_set, uint32_t p_first_index, uint32_t p_set_count);
	void add_draw_list_bind_vertex_buffers(VectorView<RDD::BufferID> p_vertex_buffers, VectorView<uint64_t> p_vertex_buffer_offsets);
	void add_draw_list_clear_attachments(VectorView<RDD::AttachmentClear> p_attachments_clear, VectorView<Rect2i> p_attachments_clear_rect);
	void add_draw_list_draw(uint32_t p_vertex_count, uint32_t p_instance_count);
	void add_draw_list_draw_indexed(uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index);
	void add_draw_list_draw_indirect(RDD::BufferID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride);
	void add_draw_list_draw_indexed_indirect(RDD::BufferID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride);
	void add_draw_list_execute_commands(RDD::CommandBufferID p_command_buffer);
	void add_draw_list_next_subpass(RDD::CommandBufferType p_command_buffer_type);
	void add_draw_list_set_blend_constants(const Color &p_color);
	void add_draw_list_set_line_width(float p_width);
	void add_draw_list_set_push_constant(RDD::ShaderID p_shader, const void *p_data, uint32_t p_data_size);
	void add_draw_list_set_scissor(Rect2i p_rect);
	void add_draw_list_set_viewport(Rect2i p_rect);
	void add_draw_list_uniform_set_prepare_for_use(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index);
	void add_draw_list_usage(ResourceTracker *p_tracker, ResourceUsage p_usage);
	void add_draw_list_usages(VectorView<ResourceTracker *> p_trackers, VectorView<ResourceUsage> p_usages);
	void add_draw_list_end();
	void add_texture_clear(RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, const Color &p_color, const RDD::TextureSubresourceRange &p_range);
	void add_texture_copy(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, VectorView<RDD::TextureCopyRegion> p_texture_copy_regions);
	void add_texture_get_data(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, VectorView<RDD::BufferTextureCopyRegion> p_buffer_texture_copy_regions, ResourceTracker *p_dst_tracker = nullptr);
	void add_texture_resolve(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, uint32_t p_src_layer, uint32_t p_src_mipmap, uint32_t p_dst_layer, uint32_t p_dst_mipmap);
	void add_texture_update(RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, VectorView<RecordedBufferToTextureCopy> p_buffer_copies, VectorView<ResourceTracker *> p_buffer_trackers = VectorView<ResourceTracker *>());
	void add_capture_timestamp(RDD::QueryPoolID p_query_pool, uint32_t p_index);
	void add_synchronization();
	void begin_label(const String &p_label_name, const Color &p_color);
	void end_label();
	void end(bool p_reorder_commands, bool p_full_barriers, RDD::CommandBufferID &r_command_buffer, CommandBufferPool &r_command_buffer_pool);
	static ResourceTracker *resource_tracker_create();
	static void resource_tracker_free(ResourceTracker *p_tracker);
	static FramebufferCache *framebuffer_cache_create();
	static void framebuffer_cache_free(RDD *p_driver, FramebufferCache *p_cache);
};

using RDG = RenderingDeviceGraph;

#endif // RENDERING_DEVICE_GRAPH_H
