/**************************************************************************/
/*  rendering_device_graph.cpp                                            */
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

#include "rendering_device_graph.h"

#define PRINT_RENDER_GRAPH 0
#define FORCE_FULL_ACCESS_BITS 0
#define PRINT_RESOURCE_TRACKER_TOTAL 0
#define PRINT_COMMAND_RECORDING 0

RenderingDeviceGraph::RenderingDeviceGraph() {
	driver_honors_barriers = false;
	driver_clears_with_copy_engine = false;
}

RenderingDeviceGraph::~RenderingDeviceGraph() {
}

String RenderingDeviceGraph::_usage_to_string(ResourceUsage p_usage) {
	switch (p_usage) {
		case RESOURCE_USAGE_NONE:
			return "None";
		case RESOURCE_USAGE_COPY_FROM:
			return "Copy From";
		case RESOURCE_USAGE_COPY_TO:
			return "Copy To";
		case RESOURCE_USAGE_RESOLVE_FROM:
			return "Resolve From";
		case RESOURCE_USAGE_RESOLVE_TO:
			return "Resolve To";
		case RESOURCE_USAGE_UNIFORM_BUFFER_READ:
			return "Uniform Buffer Read";
		case RESOURCE_USAGE_INDIRECT_BUFFER_READ:
			return "Indirect Buffer Read";
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ:
			return "Texture Buffer Read";
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE:
			return "Texture Buffer Read Write";
		case RESOURCE_USAGE_STORAGE_BUFFER_READ:
			return "Storage Buffer Read";
		case RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE:
			return "Storage Buffer Read Write";
		case RESOURCE_USAGE_VERTEX_BUFFER_READ:
			return "Vertex Buffer Read";
		case RESOURCE_USAGE_INDEX_BUFFER_READ:
			return "Index Buffer Read";
		case RESOURCE_USAGE_TEXTURE_SAMPLE:
			return "Texture Sample";
		case RESOURCE_USAGE_STORAGE_IMAGE_READ:
			return "Storage Image Read";
		case RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE:
			return "Storage Image Read Write";
		case RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE:
			return "Attachment Color Read Write";
		case RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE:
			return "Attachment Depth Stencil Read Write";
		default:
			ERR_FAIL_V_MSG("Invalid", vformat("Invalid resource usage %d.", p_usage));
	}
}

bool RenderingDeviceGraph::_is_write_usage(ResourceUsage p_usage) {
	switch (p_usage) {
		case RESOURCE_USAGE_COPY_FROM:
		case RESOURCE_USAGE_RESOLVE_FROM:
		case RESOURCE_USAGE_UNIFORM_BUFFER_READ:
		case RESOURCE_USAGE_INDIRECT_BUFFER_READ:
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ:
		case RESOURCE_USAGE_STORAGE_BUFFER_READ:
		case RESOURCE_USAGE_VERTEX_BUFFER_READ:
		case RESOURCE_USAGE_INDEX_BUFFER_READ:
		case RESOURCE_USAGE_TEXTURE_SAMPLE:
		case RESOURCE_USAGE_STORAGE_IMAGE_READ:
			return false;
		case RESOURCE_USAGE_COPY_TO:
		case RESOURCE_USAGE_RESOLVE_TO:
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE:
		case RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE:
		case RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE:
		case RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE:
		case RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE:
			return true;
		default:
			DEV_ASSERT(false && "Invalid resource tracker usage.");
			return false;
	}
}

RDD::TextureLayout RenderingDeviceGraph::_usage_to_image_layout(ResourceUsage p_usage) {
	switch (p_usage) {
		case RESOURCE_USAGE_COPY_FROM:
			return RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL;
		case RESOURCE_USAGE_COPY_TO:
			return RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL;
		case RESOURCE_USAGE_RESOLVE_FROM:
			return RDD::TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL;
		case RESOURCE_USAGE_RESOLVE_TO:
			return RDD::TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL;
		case RESOURCE_USAGE_TEXTURE_SAMPLE:
			return RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case RESOURCE_USAGE_STORAGE_IMAGE_READ:
		case RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE:
			return RDD::TEXTURE_LAYOUT_STORAGE_OPTIMAL;
		case RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE:
			return RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE:
			return RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case RESOURCE_USAGE_NONE:
			return RDD::TEXTURE_LAYOUT_UNDEFINED;
		default:
			DEV_ASSERT(false && "Invalid resource tracker usage or not an image usage.");
			return RDD::TEXTURE_LAYOUT_UNDEFINED;
	}
}

RDD::BarrierAccessBits RenderingDeviceGraph::_usage_to_access_bits(ResourceUsage p_usage) {
#if FORCE_FULL_ACCESS_BITS
	return RDD::BarrierAccessBits(RDD::BARRIER_ACCESS_MEMORY_READ_BIT | RDD::BARRIER_ACCESS_MEMORY_WRITE_BIT);
#else
	switch (p_usage) {
		case RESOURCE_USAGE_NONE:
			return RDD::BarrierAccessBits(0);
		case RESOURCE_USAGE_COPY_FROM:
			return RDD::BARRIER_ACCESS_COPY_READ_BIT;
		case RESOURCE_USAGE_COPY_TO:
			return RDD::BARRIER_ACCESS_COPY_WRITE_BIT;
		case RESOURCE_USAGE_RESOLVE_FROM:
			return RDD::BARRIER_ACCESS_RESOLVE_READ_BIT;
		case RESOURCE_USAGE_RESOLVE_TO:
			return RDD::BARRIER_ACCESS_RESOLVE_WRITE_BIT;
		case RESOURCE_USAGE_UNIFORM_BUFFER_READ:
			return RDD::BARRIER_ACCESS_UNIFORM_READ_BIT;
		case RESOURCE_USAGE_INDIRECT_BUFFER_READ:
			return RDD::BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT;
		case RESOURCE_USAGE_STORAGE_BUFFER_READ:
		case RESOURCE_USAGE_STORAGE_IMAGE_READ:
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ:
		case RESOURCE_USAGE_TEXTURE_SAMPLE:
			return RDD::BARRIER_ACCESS_SHADER_READ_BIT;
		case RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE:
		case RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE:
		case RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE:
			return RDD::BarrierAccessBits(RDD::BARRIER_ACCESS_SHADER_READ_BIT | RDD::BARRIER_ACCESS_SHADER_WRITE_BIT);
		case RESOURCE_USAGE_VERTEX_BUFFER_READ:
			return RDD::BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		case RESOURCE_USAGE_INDEX_BUFFER_READ:
			return RDD::BARRIER_ACCESS_INDEX_READ_BIT;
		case RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE:
			return RDD::BarrierAccessBits(RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		case RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE:
			return RDD::BarrierAccessBits(RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		default:
			DEV_ASSERT(false && "Invalid usage.");
			return RDD::BarrierAccessBits(0);
	}
#endif
}

bool RenderingDeviceGraph::_check_command_intersection(ResourceTracker *p_resource_tracker, int32_t p_previous_command_index, int32_t p_command_index) const {
	if (p_resource_tracker->usage != RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE && p_resource_tracker->usage != RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE) {
		// We don't check possible intersections for usages that aren't consecutive color or depth writes.
		return true;
	}

	const uint32_t previous_command_data_offset = command_data_offsets[p_previous_command_index];
	const uint32_t current_command_data_offset = command_data_offsets[p_command_index];
	const RecordedDrawListCommand &previous_draw_list_command = *reinterpret_cast<const RecordedDrawListCommand *>(&command_data[previous_command_data_offset]);
	const RecordedDrawListCommand &current_draw_list_command = *reinterpret_cast<const RecordedDrawListCommand *>(&command_data[current_command_data_offset]);
	if (previous_draw_list_command.type != RecordedCommand::TYPE_DRAW_LIST || current_draw_list_command.type != RecordedCommand::TYPE_DRAW_LIST) {
		// We don't check possible intersections if both commands aren't draw lists.
		return true;
	}

	// We check if the region used by both draw lists have an intersection.
	return previous_draw_list_command.region.intersects(current_draw_list_command.region);
}

bool RenderingDeviceGraph::_check_command_partial_coverage(ResourceTracker *p_resource_tracker, int32_t p_command_index) const {
	if (p_resource_tracker->usage != RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE && p_resource_tracker->usage != RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE) {
		// We don't check for partial coverage in usages that aren't attachment writes.
		return false;
	}

	const uint32_t command_data_offset = command_data_offsets[p_command_index];
	const RecordedDrawListCommand &draw_list_command = *reinterpret_cast<const RecordedDrawListCommand *>(&command_data[command_data_offset]);
	if (draw_list_command.type != RecordedCommand::TYPE_DRAW_LIST) {
		// We don't check for partial coverage on commands that aren't draw lists.
		return false;
	}

	Rect2i texture_region(Point2i(0, 0), p_resource_tracker->texture_size);
	return !draw_list_command.region.encloses(texture_region);
}

int32_t RenderingDeviceGraph::_add_to_command_list(int32_t p_command_index, int32_t p_list_index) {
	DEV_ASSERT(p_command_index < int32_t(command_count));
	DEV_ASSERT(p_list_index < int32_t(command_list_nodes.size()));

	int32_t next_index = int32_t(command_list_nodes.size());
	command_list_nodes.resize(next_index + 1);

	RecordedCommandListNode &new_node = command_list_nodes[next_index];
	new_node.command_index = p_command_index;
	new_node.next_list_index = p_list_index;
	return next_index;
}

void RenderingDeviceGraph::_add_adjacent_command(int32_t p_previous_command_index, int32_t p_command_index, RecordedCommand *r_command) {
	const uint32_t previous_command_data_offset = command_data_offsets[p_previous_command_index];
	RecordedCommand &previous_command = *reinterpret_cast<RecordedCommand *>(&command_data[previous_command_data_offset]);
	previous_command.adjacent_command_list_index = _add_to_command_list(p_command_index, previous_command.adjacent_command_list_index);
	previous_command.next_stages = previous_command.next_stages | r_command->self_stages;
	r_command->previous_stages = r_command->previous_stages | previous_command.self_stages;
}

int32_t RenderingDeviceGraph::_add_to_slice_read_list(int32_t p_command_index, Rect2i p_subresources, int32_t p_list_index) {
	DEV_ASSERT(p_command_index < int32_t(command_count));
	DEV_ASSERT(p_list_index < int32_t(read_slice_list_nodes.size()));

	int32_t next_index = int32_t(read_slice_list_nodes.size());
	read_slice_list_nodes.resize(next_index + 1);

	RecordedSliceListNode &new_node = read_slice_list_nodes[next_index];
	new_node.command_index = p_command_index;
	new_node.next_list_index = p_list_index;
	new_node.subresources = p_subresources;
	return next_index;
}

int32_t RenderingDeviceGraph::_add_to_write_list(int32_t p_command_index, Rect2i p_subresources, int32_t p_list_index, bool p_partial_coverage) {
	DEV_ASSERT(p_command_index < int32_t(command_count));
	DEV_ASSERT(p_list_index < int32_t(write_slice_list_nodes.size()));

	int32_t next_index = int32_t(write_slice_list_nodes.size());
	write_slice_list_nodes.resize(next_index + 1);

	RecordedSliceListNode &new_node = write_slice_list_nodes[next_index];
	new_node.command_index = p_command_index;
	new_node.next_list_index = p_list_index;
	new_node.subresources = p_subresources;
	new_node.partial_coverage = p_partial_coverage;
	return next_index;
}

RenderingDeviceGraph::RecordedCommand *RenderingDeviceGraph::_allocate_command(uint32_t p_command_size, int32_t &r_command_index) {
	uint32_t command_data_offset = command_data.size();
	command_data_offset = STEPIFY(command_data_offset, 8);
	command_data_offsets.push_back(command_data_offset);
	command_data.resize(command_data_offset + p_command_size);
	r_command_index = command_count++;
	RecordedCommand *new_command = reinterpret_cast<RecordedCommand *>(&command_data[command_data_offset]);
	*new_command = RecordedCommand();
	return new_command;
}

RenderingDeviceGraph::DrawListInstruction *RenderingDeviceGraph::_allocate_draw_list_instruction(uint32_t p_instruction_size) {
	uint32_t draw_list_data_offset = draw_instruction_list.data.size();
	draw_instruction_list.data.resize(draw_list_data_offset + p_instruction_size);
	return reinterpret_cast<DrawListInstruction *>(&draw_instruction_list.data[draw_list_data_offset]);
}

RenderingDeviceGraph::ComputeListInstruction *RenderingDeviceGraph::_allocate_compute_list_instruction(uint32_t p_instruction_size) {
	uint32_t compute_list_data_offset = compute_instruction_list.data.size();
	compute_instruction_list.data.resize(compute_list_data_offset + p_instruction_size);
	return reinterpret_cast<ComputeListInstruction *>(&compute_instruction_list.data[compute_list_data_offset]);
}

void RenderingDeviceGraph::_check_discardable_attachment_dependency(ResourceTracker *p_resource_tracker, int32_t p_previous_command_index, int32_t p_command_index) {
	if (!p_resource_tracker->is_discardable) {
		return;
	}

	// Check if the command is a a draw list that clears the attachment completely. If it is, we don't need to modify the previous draw list.
	uint32_t command_offset = command_data_offsets[p_command_index];
	RecordedDrawListCommand *draw_list_command = reinterpret_cast<RecordedDrawListCommand *>(&command_data[command_offset]);
	if (draw_list_command->type == RecordedCommand::TYPE_DRAW_LIST) {
		ResourceTracker **trackers = draw_list_command->trackers();
		for (uint32_t i = 0; i < draw_list_command->trackers_count; i++) {
			if (trackers[i] == p_resource_tracker && draw_list_command->load_ops()[i] == RDD::ATTACHMENT_LOAD_OP_CLEAR) {
				return;
			}
		}
	}

	// Check if the previous command is a draw list.
	uint32_t previous_command_offset = command_data_offsets[p_previous_command_index];
	RecordedDrawListCommand *previous_draw_list_command = reinterpret_cast<RecordedDrawListCommand *>(&command_data[previous_command_offset]);
	if (previous_draw_list_command->type != RecordedCommand::TYPE_DRAW_LIST) {
		return;
	}

	// Search for the tracker inside the draw list command and modify the store operation accordingly.
	ResourceTracker **trackers = previous_draw_list_command->trackers();
	for (uint32_t i = 0; i < previous_draw_list_command->trackers_count; i++) {
		if (trackers[i] == p_resource_tracker) {
			previous_draw_list_command->store_ops()[i] = RDD::ATTACHMENT_STORE_OP_STORE;
			return;
		}
	}
}

void RenderingDeviceGraph::_add_command_to_graph(ResourceTracker **p_resource_trackers, ResourceUsage *p_resource_usages, uint32_t p_resource_count, int32_t p_command_index, RecordedCommand *r_command) {
	// Assign the next stages derived from the stages the command requires first.
	r_command->next_stages = r_command->self_stages;

	if (command_label_index >= 0) {
		// If a label is active, tag the command with the label.
		r_command->label_index = command_label_index;
	}

	if (r_command->type == RecordedCommand::TYPE_CAPTURE_TIMESTAMP) {
		// All previous commands starting from the previous timestamp should be adjacent to this command.
		int32_t start_command_index = uint32_t(MAX(command_timestamp_index, 0));
		for (int32_t i = start_command_index; i < p_command_index; i++) {
			_add_adjacent_command(i, p_command_index, r_command);
		}

		// Make this command the new active timestamp command.
		command_timestamp_index = p_command_index;
	} else if (command_timestamp_index >= 0) {
		// Timestamp command should be adjacent to this command.
		_add_adjacent_command(command_timestamp_index, p_command_index, r_command);
	}

	if (command_synchronization_pending) {
		// All previous commands should be adjacent to this command.
		int32_t start_command_index = uint32_t(MAX(command_synchronization_index, 0));
		for (int32_t i = start_command_index; i < p_command_index; i++) {
			_add_adjacent_command(i, p_command_index, r_command);
		}

		command_synchronization_index = p_command_index;
		command_synchronization_pending = false;
	} else if (command_synchronization_index >= 0) {
		// Synchronization command should be adjacent to this command.
		_add_adjacent_command(command_synchronization_index, p_command_index, r_command);
	}

	for (uint32_t i = 0; i < p_resource_count; i++) {
		ResourceTracker *resource_tracker = p_resource_trackers[i];
		DEV_ASSERT(resource_tracker != nullptr);

		resource_tracker->reset_if_outdated(tracking_frame);

		const RDD::TextureSubresourceRange &subresources = resource_tracker->texture_subresources;
		const Rect2i resource_tracker_rect(subresources.base_mipmap, subresources.base_layer, subresources.mipmap_count, subresources.layer_count);
		Rect2i search_tracker_rect = resource_tracker_rect;

		ResourceUsage new_resource_usage = p_resource_usages[i];
		bool write_usage = _is_write_usage(new_resource_usage);
		BitField<RDD::BarrierAccessBits> new_usage_access = _usage_to_access_bits(new_resource_usage);
		bool is_resource_a_slice = resource_tracker->parent != nullptr;
		if (is_resource_a_slice) {
			// This resource depends on a parent resource.
			resource_tracker->parent->reset_if_outdated(tracking_frame);

			if (resource_tracker->texture_slice_command_index != p_command_index) {
				// Indicate this slice has been used by this command.
				resource_tracker->texture_slice_command_index = p_command_index;
			}

			if (resource_tracker->parent->usage == RESOURCE_USAGE_NONE) {
				if (resource_tracker->parent->texture_driver_id.id != 0) {
					// If the resource is a texture, we transition it entirely to the layout determined by the first slice that uses it.
					_add_texture_barrier_to_command(resource_tracker->parent->texture_driver_id, RDD::BarrierAccessBits(0), new_usage_access, RDG::RESOURCE_USAGE_NONE, new_resource_usage, resource_tracker->parent->texture_subresources, command_normalization_barriers, r_command->normalization_barrier_index, r_command->normalization_barrier_count);
				}

				// If the parent hasn't been used yet, we assign the usage of the slice to the entire resource.
				resource_tracker->parent->usage = new_resource_usage;

				// Also assign the usage to the slice and consider it a write operation. Consider the parent's current usage access as its own.
				resource_tracker->usage = new_resource_usage;
				resource_tracker->usage_access = resource_tracker->parent->usage_access;
				write_usage = true;

				// Indicate the area that should be tracked is the entire resource.
				const RDD::TextureSubresourceRange &parent_subresources = resource_tracker->parent->texture_subresources;
				search_tracker_rect = Rect2i(parent_subresources.base_mipmap, parent_subresources.base_layer, parent_subresources.mipmap_count, parent_subresources.layer_count);
			} else if (resource_tracker->in_parent_dirty_list) {
				if (resource_tracker->parent->usage == new_resource_usage) {
					// The slice will be transitioned to the resource of the parent and can be deleted from the dirty list.
					ResourceTracker *previous_tracker = nullptr;
					ResourceTracker *current_tracker = resource_tracker->parent->dirty_shared_list;
					bool initialized_dirty_rect = false;
					while (current_tracker != nullptr) {
						current_tracker->reset_if_outdated(tracking_frame);

						if (current_tracker == resource_tracker) {
							current_tracker->in_parent_dirty_list = false;

							if (previous_tracker != nullptr) {
								previous_tracker->next_shared = current_tracker->next_shared;
							} else {
								resource_tracker->parent->dirty_shared_list = current_tracker->next_shared;
							}

							current_tracker = current_tracker->next_shared;
						} else {
							if (initialized_dirty_rect) {
								resource_tracker->parent->texture_slice_or_dirty_rect = resource_tracker->parent->texture_slice_or_dirty_rect.merge(current_tracker->texture_slice_or_dirty_rect);
							} else {
								resource_tracker->parent->texture_slice_or_dirty_rect = current_tracker->texture_slice_or_dirty_rect;
								initialized_dirty_rect = true;
							}

							previous_tracker = current_tracker;
							current_tracker = current_tracker->next_shared;
						}
					}
				}
			} else {
				if (resource_tracker->parent->dirty_shared_list != nullptr && resource_tracker->parent->texture_slice_or_dirty_rect.intersects(resource_tracker->texture_slice_or_dirty_rect)) {
					// There's an intersection with the current dirty area of the parent and the slice. We must verify if the intersection is against a slice
					// that was used in this command or not. Any slice we can find that wasn't used by this command must be reverted to the layout of the parent.
					ResourceTracker *previous_tracker = nullptr;
					ResourceTracker *current_tracker = resource_tracker->parent->dirty_shared_list;
					bool initialized_dirty_rect = false;
					while (current_tracker != nullptr) {
						current_tracker->reset_if_outdated(tracking_frame);

						if (current_tracker->texture_slice_or_dirty_rect.intersects(resource_tracker->texture_slice_or_dirty_rect)) {
							if (current_tracker->command_frame == tracking_frame && current_tracker->texture_slice_command_index == p_command_index) {
								ERR_FAIL_MSG("Texture slices that overlap can't be used in the same command.");
							} else {
								// Delete the slice from the dirty list and revert it to the usage of the parent.
								if (current_tracker->texture_driver_id.id != 0) {
									_add_texture_barrier_to_command(current_tracker->texture_driver_id, current_tracker->usage_access, new_usage_access, current_tracker->usage, resource_tracker->parent->usage, current_tracker->texture_subresources, command_normalization_barriers, r_command->normalization_barrier_index, r_command->normalization_barrier_count);

									// Merge the area of the slice with the current tracking area of the command and indicate it's a write usage as well.
									search_tracker_rect = search_tracker_rect.merge(current_tracker->texture_slice_or_dirty_rect);
									write_usage = true;
								}

								current_tracker->in_parent_dirty_list = false;

								if (previous_tracker != nullptr) {
									previous_tracker->next_shared = current_tracker->next_shared;
								} else {
									resource_tracker->parent->dirty_shared_list = current_tracker->next_shared;
								}

								current_tracker = current_tracker->next_shared;
							}
						} else {
							// Recalculate the dirty rect of the parent so the deleted slices are excluded.
							if (initialized_dirty_rect) {
								resource_tracker->parent->texture_slice_or_dirty_rect = resource_tracker->parent->texture_slice_or_dirty_rect.merge(current_tracker->texture_slice_or_dirty_rect);
							} else {
								resource_tracker->parent->texture_slice_or_dirty_rect = current_tracker->texture_slice_or_dirty_rect;
								initialized_dirty_rect = true;
							}

							previous_tracker = current_tracker;
							current_tracker = current_tracker->next_shared;
						}
					}
				}

				// If it wasn't in the list, assume the usage is the same as the parent. Consider the parent's current usage access as its own.
				resource_tracker->usage = resource_tracker->parent->usage;
				resource_tracker->usage_access = resource_tracker->parent->usage_access;

				if (resource_tracker->usage != new_resource_usage) {
					// Insert to the dirty list if the requested usage is different.
					resource_tracker->next_shared = resource_tracker->parent->dirty_shared_list;
					resource_tracker->parent->dirty_shared_list = resource_tracker;
					resource_tracker->in_parent_dirty_list = true;
					if (resource_tracker->parent->dirty_shared_list != nullptr) {
						resource_tracker->parent->texture_slice_or_dirty_rect = resource_tracker->parent->texture_slice_or_dirty_rect.merge(resource_tracker->texture_slice_or_dirty_rect);
					} else {
						resource_tracker->parent->texture_slice_or_dirty_rect = resource_tracker->texture_slice_or_dirty_rect;
					}
				}
			}
		} else {
			ResourceTracker *current_tracker = resource_tracker->dirty_shared_list;
			if (current_tracker != nullptr) {
				// Consider the usage as write if we must transition any of the slices.
				write_usage = true;
			}

			while (current_tracker != nullptr) {
				current_tracker->reset_if_outdated(tracking_frame);

				if (current_tracker->texture_driver_id.id != 0) {
					// Transition all slices to the layout of the parent resource.
					_add_texture_barrier_to_command(current_tracker->texture_driver_id, current_tracker->usage_access, new_usage_access, current_tracker->usage, resource_tracker->usage, current_tracker->texture_subresources, command_normalization_barriers, r_command->normalization_barrier_index, r_command->normalization_barrier_count);
				}

				current_tracker->in_parent_dirty_list = false;
				current_tracker = current_tracker->next_shared;
			}

			resource_tracker->dirty_shared_list = nullptr;
		}

		// Use the resource's parent tracker directly for all search operations.
		bool resource_has_parent = resource_tracker->parent != nullptr;
		ResourceTracker *search_tracker = resource_has_parent ? resource_tracker->parent : resource_tracker;
		bool different_usage = resource_tracker->usage != new_resource_usage;
		bool write_usage_after_write = (write_usage && search_tracker->write_command_or_list_index >= 0);
		if (different_usage || write_usage_after_write) {
			// A barrier must be pushed if the usage is different of it's a write usage and there was already a command that wrote to this resource previously.
			if (resource_tracker->texture_driver_id.id != 0) {
				if (resource_tracker->usage_access.is_empty()) {
					// FIXME: If the tracker does not know the previous type of usage, assume the generic memory write one.
					// Tracking access bits across texture slices can be tricky, so this failsafe can be removed once that's improved.
					resource_tracker->usage_access = RDD::BARRIER_ACCESS_MEMORY_WRITE_BIT;
				}

				_add_texture_barrier_to_command(resource_tracker->texture_driver_id, resource_tracker->usage_access, new_usage_access, resource_tracker->usage, new_resource_usage, resource_tracker->texture_subresources, command_transition_barriers, r_command->transition_barrier_index, r_command->transition_barrier_count);
			} else if (resource_tracker->buffer_driver_id.id != 0) {
#if USE_BUFFER_BARRIERS
				_add_buffer_barrier_to_command(resource_tracker->buffer_driver_id, resource_tracker->usage_access, new_usage_access, r_command->buffer_barrier_index, r_command->buffer_barrier_count);
#endif
				// Memory barriers are pushed regardless of buffer barriers being used or not.
				r_command->memory_barrier.src_access = r_command->memory_barrier.src_access | resource_tracker->usage_access;
				r_command->memory_barrier.dst_access = r_command->memory_barrier.dst_access | new_usage_access;
			} else {
				DEV_ASSERT(false && "Resource tracker does not contain a valid buffer or texture ID.");
			}
		}

		// Always update the access of the tracker according to the latest usage.
		resource_tracker->usage_access = new_usage_access;

		// Always accumulate the stages of the tracker with the commands that use it.
		search_tracker->current_frame_stages = search_tracker->current_frame_stages | r_command->self_stages;

		if (!search_tracker->previous_frame_stages.is_empty()) {
			// Add to the command the stages the tracker was used on in the previous frame.
			r_command->previous_stages = r_command->previous_stages | search_tracker->previous_frame_stages;
			search_tracker->previous_frame_stages.clear();
		}

		if (different_usage) {
			// Even if the usage of the resource isn't a write usage explicitly, a different usage implies a transition and it should therefore be considered a write.
			// In the case of buffers however, this is not exactly necessary if the driver does not consider different buffer usages as different states.
			write_usage = write_usage || bool(resource_tracker->texture_driver_id) || driver_buffers_require_transitions;
			resource_tracker->usage = new_resource_usage;
		}

		bool write_usage_has_partial_coverage = !different_usage && _check_command_partial_coverage(resource_tracker, p_command_index);
		if (search_tracker->write_command_or_list_index >= 0) {
			if (search_tracker->write_command_list_enabled) {
				// Make this command adjacent to any commands that wrote to this resource and intersect with the slice if it applies.
				// For buffers or textures that never use slices, this list will only be one element long at most.
				int32_t previous_write_list_index = -1;
				int32_t write_list_index = search_tracker->write_command_or_list_index;
				while (write_list_index >= 0) {
					const RecordedSliceListNode &write_list_node = write_slice_list_nodes[write_list_index];
					if (!resource_has_parent || search_tracker_rect.intersects(write_list_node.subresources)) {
						if (write_list_node.command_index == p_command_index) {
							ERR_FAIL_COND_MSG(!resource_has_parent, "Command can't have itself as a dependency.");
						} else if (!write_list_node.partial_coverage || _check_command_intersection(resource_tracker, write_list_node.command_index, p_command_index)) {
							_check_discardable_attachment_dependency(search_tracker, write_list_node.command_index, p_command_index);

							// Command is dependent on this command. Add this command to the adjacency list of the write command.
							_add_adjacent_command(write_list_node.command_index, p_command_index, r_command);

							if (resource_has_parent && write_usage && search_tracker_rect.encloses(write_list_node.subresources) && !write_usage_has_partial_coverage) {
								// Eliminate redundant writes from the list.
								if (previous_write_list_index >= 0) {
									RecordedSliceListNode &previous_list_node = write_slice_list_nodes[previous_write_list_index];
									previous_list_node.next_list_index = write_list_node.next_list_index;
								} else {
									search_tracker->write_command_or_list_index = write_list_node.next_list_index;
								}

								write_list_index = write_list_node.next_list_index;
								continue;
							}
						}
					}

					previous_write_list_index = write_list_index;
					write_list_index = write_list_node.next_list_index;
				}
			} else {
				// The index is just the latest command index that wrote to the resource.
				if (search_tracker->write_command_or_list_index == p_command_index) {
					ERR_FAIL_MSG("Command can't have itself as a dependency.");
				} else {
					_check_discardable_attachment_dependency(search_tracker, search_tracker->write_command_or_list_index, p_command_index);
					_add_adjacent_command(search_tracker->write_command_or_list_index, p_command_index, r_command);
				}
			}
		}

		if (write_usage) {
			bool use_write_list = resource_has_parent || write_usage_has_partial_coverage;
			if (use_write_list) {
				if (!search_tracker->write_command_list_enabled && search_tracker->write_command_or_list_index >= 0) {
					// Write command list was not being used but there was a write command recorded. Add a new node with the entire parent resource's subresources and the recorded command index to the list.
					const RDD::TextureSubresourceRange &tracker_subresources = search_tracker->texture_subresources;
					Rect2i tracker_rect(tracker_subresources.base_mipmap, tracker_subresources.base_layer, tracker_subresources.mipmap_count, tracker_subresources.layer_count);
					search_tracker->write_command_or_list_index = _add_to_write_list(search_tracker->write_command_or_list_index, tracker_rect, -1, false);
				}

				search_tracker->write_command_or_list_index = _add_to_write_list(p_command_index, search_tracker_rect, search_tracker->write_command_or_list_index, write_usage_has_partial_coverage);
				search_tracker->write_command_list_enabled = true;
			} else {
				search_tracker->write_command_or_list_index = p_command_index;
				search_tracker->write_command_list_enabled = false;
			}

			// We add this command to the adjacency list of all commands that were reading from the entire resource.
			int32_t read_full_command_list_index = search_tracker->read_full_command_list_index;
			while (read_full_command_list_index >= 0) {
				int32_t read_full_command_index = command_list_nodes[read_full_command_list_index].command_index;
				int32_t read_full_next_index = command_list_nodes[read_full_command_list_index].next_list_index;
				if (read_full_command_index == p_command_index) {
					if (!resource_has_parent) {
						// Only slices are allowed to be in different usages in the same command as they are guaranteed to have no overlap in the same command.
						ERR_FAIL_MSG("Command can't have itself as a dependency.");
					}
				} else {
					// Add this command to the adjacency list of each command that was reading this resource.
					_add_adjacent_command(read_full_command_index, p_command_index, r_command);
				}

				read_full_command_list_index = read_full_next_index;
			}

			if (!use_write_list) {
				// Clear the full list if this resource is not a slice.
				search_tracker->read_full_command_list_index = -1;
			}

			// We add this command to the adjacency list of all commands that were reading from resource slices.
			int32_t previous_slice_command_list_index = -1;
			int32_t read_slice_command_list_index = search_tracker->read_slice_command_list_index;
			while (read_slice_command_list_index >= 0) {
				const RecordedSliceListNode &read_list_node = read_slice_list_nodes[read_slice_command_list_index];
				if (!use_write_list || search_tracker_rect.encloses(read_list_node.subresources)) {
					if (previous_slice_command_list_index >= 0) {
						// Erase this element and connect the previous one to the next element.
						read_slice_list_nodes[previous_slice_command_list_index].next_list_index = read_list_node.next_list_index;
					} else {
						// Erase this element from the head of the list.
						DEV_ASSERT(search_tracker->read_slice_command_list_index == read_slice_command_list_index);
						search_tracker->read_slice_command_list_index = read_list_node.next_list_index;
					}

					// Advance to the next element.
					read_slice_command_list_index = read_list_node.next_list_index;
				} else {
					previous_slice_command_list_index = read_slice_command_list_index;
					read_slice_command_list_index = read_list_node.next_list_index;
				}

				if (!resource_has_parent || search_tracker_rect.intersects(read_list_node.subresources)) {
					// Add this command to the adjacency list of each command that was reading this resource.
					// We only add the dependency if there's an intersection between slices or this resource isn't a slice.
					_add_adjacent_command(read_list_node.command_index, p_command_index, r_command);
				}
			}
		} else if (resource_has_parent) {
			// We add a read dependency to the tracker to indicate this command reads from the resource slice.
			search_tracker->read_slice_command_list_index = _add_to_slice_read_list(p_command_index, resource_tracker_rect, search_tracker->read_slice_command_list_index);
		} else {
			// We add a read dependency to the tracker to indicate this command reads from the entire resource.
			search_tracker->read_full_command_list_index = _add_to_command_list(p_command_index, search_tracker->read_full_command_list_index);
		}
	}
}

void RenderingDeviceGraph::_add_texture_barrier_to_command(RDD::TextureID p_texture_id, BitField<RDD::BarrierAccessBits> p_src_access, BitField<RDD::BarrierAccessBits> p_dst_access, ResourceUsage p_prev_usage, ResourceUsage p_next_usage, RDD::TextureSubresourceRange p_subresources, LocalVector<RDD::TextureBarrier> &r_barrier_vector, int32_t &r_barrier_index, int32_t &r_barrier_count) {
	if (!driver_honors_barriers) {
		return;
	}

	if (r_barrier_index < 0) {
		r_barrier_index = r_barrier_vector.size();
	}

	RDD::TextureBarrier texture_barrier;
	texture_barrier.texture = p_texture_id;
	texture_barrier.src_access = p_src_access;
	texture_barrier.dst_access = p_dst_access;
	texture_barrier.prev_layout = _usage_to_image_layout(p_prev_usage);
	texture_barrier.next_layout = _usage_to_image_layout(p_next_usage);
	texture_barrier.subresources = p_subresources;
	r_barrier_vector.push_back(texture_barrier);
	r_barrier_count++;
}

#if USE_BUFFER_BARRIERS
void RenderingDeviceGraph::_add_buffer_barrier_to_command(RDD::BufferID p_buffer_id, BitField<RDD::BarrierAccessBits> p_src_access, BitField<RDD::BarrierAccessBits> p_dst_access, int32_t &r_barrier_index, int32_t &r_barrier_count) {
	if (!driver_honors_barriers) {
		return;
	}

	if (r_barrier_index < 0) {
		r_barrier_index = command_buffer_barriers.size();
	}

	RDD::BufferBarrier buffer_barrier;
	buffer_barrier.buffer = p_buffer_id;
	buffer_barrier.src_access = p_src_access;
	buffer_barrier.dst_access = p_dst_access;
	buffer_barrier.offset = 0;
	buffer_barrier.size = RDD::BUFFER_WHOLE_SIZE;
	command_buffer_barriers.push_back(buffer_barrier);
	r_barrier_count++;
}
#endif

void RenderingDeviceGraph::_run_compute_list_command(RDD::CommandBufferID p_command_buffer, const uint8_t *p_instruction_data, uint32_t p_instruction_data_size) {
	uint32_t instruction_data_cursor = 0;
	while (instruction_data_cursor < p_instruction_data_size) {
		DEV_ASSERT((instruction_data_cursor + sizeof(ComputeListInstruction)) <= p_instruction_data_size);

		const ComputeListInstruction *instruction = reinterpret_cast<const ComputeListInstruction *>(&p_instruction_data[instruction_data_cursor]);
		switch (instruction->type) {
			case ComputeListInstruction::TYPE_BIND_PIPELINE: {
				const ComputeListBindPipelineInstruction *bind_pipeline_instruction = reinterpret_cast<const ComputeListBindPipelineInstruction *>(instruction);
				driver->command_bind_compute_pipeline(p_command_buffer, bind_pipeline_instruction->pipeline);
				instruction_data_cursor += sizeof(ComputeListBindPipelineInstruction);
			} break;
			case ComputeListInstruction::TYPE_BIND_UNIFORM_SETS: {
				const ComputeListBindUniformSetsInstruction *bind_uniform_sets_instruction = reinterpret_cast<const ComputeListBindUniformSetsInstruction *>(instruction);
				driver->command_bind_compute_uniform_sets(p_command_buffer, VectorView<RDD::UniformSetID>(bind_uniform_sets_instruction->uniform_set_ids(), bind_uniform_sets_instruction->set_count), bind_uniform_sets_instruction->shader, bind_uniform_sets_instruction->first_set_index, bind_uniform_sets_instruction->set_count);
				instruction_data_cursor += sizeof(ComputeListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * bind_uniform_sets_instruction->set_count;
			} break;
			case ComputeListInstruction::TYPE_DISPATCH: {
				const ComputeListDispatchInstruction *dispatch_instruction = reinterpret_cast<const ComputeListDispatchInstruction *>(instruction);
				driver->command_compute_dispatch(p_command_buffer, dispatch_instruction->x_groups, dispatch_instruction->y_groups, dispatch_instruction->z_groups);
				instruction_data_cursor += sizeof(ComputeListDispatchInstruction);
			} break;
			case ComputeListInstruction::TYPE_DISPATCH_INDIRECT: {
				const ComputeListDispatchIndirectInstruction *dispatch_indirect_instruction = reinterpret_cast<const ComputeListDispatchIndirectInstruction *>(instruction);
				driver->command_compute_dispatch_indirect(p_command_buffer, dispatch_indirect_instruction->buffer, dispatch_indirect_instruction->offset);
				instruction_data_cursor += sizeof(ComputeListDispatchIndirectInstruction);
			} break;
			case ComputeListInstruction::TYPE_SET_PUSH_CONSTANT: {
				const ComputeListSetPushConstantInstruction *set_push_constant_instruction = reinterpret_cast<const ComputeListSetPushConstantInstruction *>(instruction);
				const VectorView push_constant_data_view(reinterpret_cast<const uint32_t *>(set_push_constant_instruction->data()), set_push_constant_instruction->size / sizeof(uint32_t));
				driver->command_bind_push_constants(p_command_buffer, set_push_constant_instruction->shader, 0, push_constant_data_view);
				instruction_data_cursor += sizeof(ComputeListSetPushConstantInstruction);
				instruction_data_cursor += set_push_constant_instruction->size;
			} break;
			case ComputeListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE: {
				const ComputeListUniformSetPrepareForUseInstruction *uniform_set_prepare_for_use_instruction = reinterpret_cast<const ComputeListUniformSetPrepareForUseInstruction *>(instruction);
				driver->command_uniform_set_prepare_for_use(p_command_buffer, uniform_set_prepare_for_use_instruction->uniform_set, uniform_set_prepare_for_use_instruction->shader, uniform_set_prepare_for_use_instruction->set_index);
				instruction_data_cursor += sizeof(ComputeListUniformSetPrepareForUseInstruction);
			} break;
			default:
				DEV_ASSERT(false && "Unknown compute list instruction type.");
				return;
		}
	}
}

void RenderingDeviceGraph::_get_draw_list_render_pass_and_framebuffer(const RecordedDrawListCommand *p_draw_list_command, RDD::RenderPassID &r_render_pass, RDD::FramebufferID &r_framebuffer) {
	DEV_ASSERT(p_draw_list_command->trackers_count <= 21 && "Max number of attachments that can be encoded into the key.");

	// Build a unique key from the load and store ops for each attachment.
	const RDD::AttachmentLoadOp *load_ops = p_draw_list_command->load_ops();
	const RDD::AttachmentStoreOp *store_ops = p_draw_list_command->store_ops();
	uint64_t key = 0;
	for (uint32_t i = 0; i < p_draw_list_command->trackers_count; i++) {
		key |= uint64_t(load_ops[i]) << (i * 3);
		key |= uint64_t(store_ops[i]) << (i * 3 + 2);
	}

	// Check the storage map if the render pass and the framebuffer needs to be created.
	FramebufferCache *framebuffer_cache = p_draw_list_command->framebuffer_cache;
	HashMap<uint64_t, FramebufferStorage>::Iterator it = framebuffer_cache->storage_map.find(key);
	if (it == framebuffer_cache->storage_map.end()) {
		FramebufferStorage storage;
		VectorView<RDD::AttachmentLoadOp> load_ops_view(load_ops, p_draw_list_command->trackers_count);
		VectorView<RDD::AttachmentStoreOp> store_ops_view(store_ops, p_draw_list_command->trackers_count);
		storage.render_pass = render_pass_creation_function(driver, load_ops_view, store_ops_view, framebuffer_cache->render_pass_creation_user_data);
		ERR_FAIL_COND(!storage.render_pass);

		storage.framebuffer = driver->framebuffer_create(storage.render_pass, framebuffer_cache->textures, framebuffer_cache->width, framebuffer_cache->height);
		ERR_FAIL_COND(!storage.framebuffer);

		it = framebuffer_cache->storage_map.insert(key, storage);
	}

	r_render_pass = it->value.render_pass;
	r_framebuffer = it->value.framebuffer;
}

void RenderingDeviceGraph::_run_draw_list_command(RDD::CommandBufferID p_command_buffer, const uint8_t *p_instruction_data, uint32_t p_instruction_data_size) {
	uint32_t instruction_data_cursor = 0;
	while (instruction_data_cursor < p_instruction_data_size) {
		DEV_ASSERT((instruction_data_cursor + sizeof(DrawListInstruction)) <= p_instruction_data_size);

		const DrawListInstruction *instruction = reinterpret_cast<const DrawListInstruction *>(&p_instruction_data[instruction_data_cursor]);
		switch (instruction->type) {
			case DrawListInstruction::TYPE_BIND_INDEX_BUFFER: {
				const DrawListBindIndexBufferInstruction *bind_index_buffer_instruction = reinterpret_cast<const DrawListBindIndexBufferInstruction *>(instruction);
				driver->command_render_bind_index_buffer(p_command_buffer, bind_index_buffer_instruction->buffer, bind_index_buffer_instruction->format, bind_index_buffer_instruction->offset);
				instruction_data_cursor += sizeof(DrawListBindIndexBufferInstruction);
			} break;
			case DrawListInstruction::TYPE_BIND_PIPELINE: {
				const DrawListBindPipelineInstruction *bind_pipeline_instruction = reinterpret_cast<const DrawListBindPipelineInstruction *>(instruction);
				driver->command_bind_render_pipeline(p_command_buffer, bind_pipeline_instruction->pipeline);
				instruction_data_cursor += sizeof(DrawListBindPipelineInstruction);
			} break;
			case DrawListInstruction::TYPE_BIND_UNIFORM_SETS: {
				const DrawListBindUniformSetsInstruction *bind_uniform_sets_instruction = reinterpret_cast<const DrawListBindUniformSetsInstruction *>(instruction);
				driver->command_bind_render_uniform_sets(p_command_buffer, VectorView<RDD::UniformSetID>(bind_uniform_sets_instruction->uniform_set_ids(), bind_uniform_sets_instruction->set_count), bind_uniform_sets_instruction->shader, bind_uniform_sets_instruction->first_set_index, bind_uniform_sets_instruction->set_count);
				instruction_data_cursor += sizeof(DrawListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * bind_uniform_sets_instruction->set_count;
			} break;
			case DrawListInstruction::TYPE_BIND_VERTEX_BUFFERS: {
				const DrawListBindVertexBuffersInstruction *bind_vertex_buffers_instruction = reinterpret_cast<const DrawListBindVertexBuffersInstruction *>(instruction);
				driver->command_render_bind_vertex_buffers(p_command_buffer, bind_vertex_buffers_instruction->vertex_buffers_count, bind_vertex_buffers_instruction->vertex_buffers(), bind_vertex_buffers_instruction->vertex_buffer_offsets());
				instruction_data_cursor += sizeof(DrawListBindVertexBuffersInstruction);
				instruction_data_cursor += sizeof(RDD::BufferID) * bind_vertex_buffers_instruction->vertex_buffers_count;
				instruction_data_cursor += sizeof(uint64_t) * bind_vertex_buffers_instruction->vertex_buffers_count;
			} break;
			case DrawListInstruction::TYPE_CLEAR_ATTACHMENTS: {
				const DrawListClearAttachmentsInstruction *clear_attachments_instruction = reinterpret_cast<const DrawListClearAttachmentsInstruction *>(instruction);
				const VectorView attachments_clear_view(clear_attachments_instruction->attachments_clear(), clear_attachments_instruction->attachments_clear_count);
				const VectorView attachments_clear_rect_view(clear_attachments_instruction->attachments_clear_rect(), clear_attachments_instruction->attachments_clear_rect_count);
				driver->command_render_clear_attachments(p_command_buffer, attachments_clear_view, attachments_clear_rect_view);
				instruction_data_cursor += sizeof(DrawListClearAttachmentsInstruction);
				instruction_data_cursor += sizeof(RDD::AttachmentClear) * clear_attachments_instruction->attachments_clear_count;
				instruction_data_cursor += sizeof(Rect2i) * clear_attachments_instruction->attachments_clear_rect_count;
			} break;
			case DrawListInstruction::TYPE_DRAW: {
				const DrawListDrawInstruction *draw_instruction = reinterpret_cast<const DrawListDrawInstruction *>(instruction);
				driver->command_render_draw(p_command_buffer, draw_instruction->vertex_count, draw_instruction->instance_count, 0, 0);
				instruction_data_cursor += sizeof(DrawListDrawInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDEXED: {
				const DrawListDrawIndexedInstruction *draw_indexed_instruction = reinterpret_cast<const DrawListDrawIndexedInstruction *>(instruction);
				driver->command_render_draw_indexed(p_command_buffer, draw_indexed_instruction->index_count, draw_indexed_instruction->instance_count, draw_indexed_instruction->first_index, 0, 0);
				instruction_data_cursor += sizeof(DrawListDrawIndexedInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDIRECT: {
				const DrawListDrawIndirectInstruction *draw_indirect_instruction = reinterpret_cast<const DrawListDrawIndirectInstruction *>(instruction);
				driver->command_render_draw_indirect(p_command_buffer, draw_indirect_instruction->buffer, draw_indirect_instruction->offset, draw_indirect_instruction->draw_count, draw_indirect_instruction->stride);
				instruction_data_cursor += sizeof(DrawListDrawIndirectInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDEXED_INDIRECT: {
				const DrawListDrawIndexedIndirectInstruction *draw_indexed_indirect_instruction = reinterpret_cast<const DrawListDrawIndexedIndirectInstruction *>(instruction);
				driver->command_render_draw_indexed_indirect(p_command_buffer, draw_indexed_indirect_instruction->buffer, draw_indexed_indirect_instruction->offset, draw_indexed_indirect_instruction->draw_count, draw_indexed_indirect_instruction->stride);
				instruction_data_cursor += sizeof(DrawListDrawIndexedIndirectInstruction);
			} break;
			case DrawListInstruction::TYPE_EXECUTE_COMMANDS: {
				const DrawListExecuteCommandsInstruction *execute_commands_instruction = reinterpret_cast<const DrawListExecuteCommandsInstruction *>(instruction);
				driver->command_buffer_execute_secondary(p_command_buffer, execute_commands_instruction->command_buffer);
				instruction_data_cursor += sizeof(DrawListExecuteCommandsInstruction);
			} break;
			case DrawListInstruction::TYPE_NEXT_SUBPASS: {
				const DrawListNextSubpassInstruction *next_subpass_instruction = reinterpret_cast<const DrawListNextSubpassInstruction *>(instruction);
				driver->command_next_render_subpass(p_command_buffer, next_subpass_instruction->command_buffer_type);
				instruction_data_cursor += sizeof(DrawListNextSubpassInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_BLEND_CONSTANTS: {
				const DrawListSetBlendConstantsInstruction *set_blend_constants_instruction = reinterpret_cast<const DrawListSetBlendConstantsInstruction *>(instruction);
				driver->command_render_set_blend_constants(p_command_buffer, set_blend_constants_instruction->color);
				instruction_data_cursor += sizeof(DrawListSetBlendConstantsInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_LINE_WIDTH: {
				const DrawListSetLineWidthInstruction *set_line_width_instruction = reinterpret_cast<const DrawListSetLineWidthInstruction *>(instruction);
				driver->command_render_set_line_width(p_command_buffer, set_line_width_instruction->width);
				instruction_data_cursor += sizeof(DrawListSetLineWidthInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_PUSH_CONSTANT: {
				const DrawListSetPushConstantInstruction *set_push_constant_instruction = reinterpret_cast<const DrawListSetPushConstantInstruction *>(instruction);
				const VectorView push_constant_data_view(reinterpret_cast<const uint32_t *>(set_push_constant_instruction->data()), set_push_constant_instruction->size / sizeof(uint32_t));
				driver->command_bind_push_constants(p_command_buffer, set_push_constant_instruction->shader, 0, push_constant_data_view);
				instruction_data_cursor += sizeof(DrawListSetPushConstantInstruction);
				instruction_data_cursor += set_push_constant_instruction->size;
			} break;
			case DrawListInstruction::TYPE_SET_SCISSOR: {
				const DrawListSetScissorInstruction *set_scissor_instruction = reinterpret_cast<const DrawListSetScissorInstruction *>(instruction);
				driver->command_render_set_scissor(p_command_buffer, set_scissor_instruction->rect);
				instruction_data_cursor += sizeof(DrawListSetScissorInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_VIEWPORT: {
				const DrawListSetViewportInstruction *set_viewport_instruction = reinterpret_cast<const DrawListSetViewportInstruction *>(instruction);
				driver->command_render_set_viewport(p_command_buffer, set_viewport_instruction->rect);
				instruction_data_cursor += sizeof(DrawListSetViewportInstruction);
			} break;
			case DrawListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE: {
				const DrawListUniformSetPrepareForUseInstruction *uniform_set_prepare_for_use_instruction = reinterpret_cast<const DrawListUniformSetPrepareForUseInstruction *>(instruction);
				driver->command_uniform_set_prepare_for_use(p_command_buffer, uniform_set_prepare_for_use_instruction->uniform_set, uniform_set_prepare_for_use_instruction->shader, uniform_set_prepare_for_use_instruction->set_index);
				instruction_data_cursor += sizeof(DrawListUniformSetPrepareForUseInstruction);
			} break;
			default:
				DEV_ASSERT(false && "Unknown draw list instruction type.");
				return;
		}
	}
}

void RenderingDeviceGraph::_add_draw_list_begin(FramebufferCache *p_framebuffer_cache, RDD::RenderPassID p_render_pass, RDD::FramebufferID p_framebuffer, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb, bool p_split_cmd_buffer) {
	DEV_ASSERT(p_attachment_operations.size() == p_attachment_clear_values.size());

	draw_instruction_list.clear();
	draw_instruction_list.index++;
	draw_instruction_list.framebuffer_cache = p_framebuffer_cache;
	draw_instruction_list.render_pass = p_render_pass;
	draw_instruction_list.framebuffer = p_framebuffer;
	draw_instruction_list.region = p_region;
	draw_instruction_list.attachment_operations.resize(p_attachment_operations.size());
	draw_instruction_list.attachment_clear_values.resize(p_attachment_clear_values.size());

	for (uint32_t i = 0; i < p_attachment_operations.size(); i++) {
		draw_instruction_list.attachment_operations[i] = p_attachment_operations[i];
		draw_instruction_list.attachment_clear_values[i] = p_attachment_clear_values[i];
	}

	if (p_uses_color) {
		draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	if (p_uses_depth) {
		draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
		draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	}

	draw_instruction_list.split_cmd_buffer = p_split_cmd_buffer;

#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	draw_instruction_list.breadcrumb = p_breadcrumb;
#endif
}

void RenderingDeviceGraph::_run_secondary_command_buffer_task(const SecondaryCommandBuffer *p_secondary) {
	driver->command_buffer_begin_secondary(p_secondary->command_buffer, p_secondary->render_pass, 0, p_secondary->framebuffer);
	_run_draw_list_command(p_secondary->command_buffer, p_secondary->instruction_data.ptr(), p_secondary->instruction_data.size());
	driver->command_buffer_end(p_secondary->command_buffer);
}

void RenderingDeviceGraph::_wait_for_secondary_command_buffer_tasks() {
	for (uint32_t i = 0; i < frames[frame].secondary_command_buffers_used; i++) {
		WorkerThreadPool::TaskID &task = frames[frame].secondary_command_buffers[i].task;
		if (task != WorkerThreadPool::INVALID_TASK_ID) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(task);
			task = WorkerThreadPool::INVALID_TASK_ID;
		}
	}
}

void RenderingDeviceGraph::_run_render_commands(int32_t p_level, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, RDD::CommandBufferID &r_command_buffer, CommandBufferPool &r_command_buffer_pool, int32_t &r_current_label_index, int32_t &r_current_label_level) {
	for (uint32_t i = 0; i < p_sorted_commands_count; i++) {
		const uint32_t command_index = p_sorted_commands[i].index;
		const uint32_t command_data_offset = command_data_offsets[command_index];
		const RecordedCommand *command = reinterpret_cast<const RecordedCommand *>(&command_data[command_data_offset]);
		_run_label_command_change(r_command_buffer, command->label_index, p_level, false, true, &p_sorted_commands[i], p_sorted_commands_count - i, r_current_label_index, r_current_label_level);

		switch (command->type) {
			case RecordedCommand::TYPE_BUFFER_CLEAR: {
				const RecordedBufferClearCommand *buffer_clear_command = reinterpret_cast<const RecordedBufferClearCommand *>(command);
				driver->command_clear_buffer(r_command_buffer, buffer_clear_command->buffer, buffer_clear_command->offset, buffer_clear_command->size);
			} break;
			case RecordedCommand::TYPE_BUFFER_COPY: {
				const RecordedBufferCopyCommand *buffer_copy_command = reinterpret_cast<const RecordedBufferCopyCommand *>(command);
				driver->command_copy_buffer(r_command_buffer, buffer_copy_command->source, buffer_copy_command->destination, buffer_copy_command->region);
			} break;
			case RecordedCommand::TYPE_BUFFER_GET_DATA: {
				const RecordedBufferGetDataCommand *buffer_get_data_command = reinterpret_cast<const RecordedBufferGetDataCommand *>(command);
				driver->command_copy_buffer(r_command_buffer, buffer_get_data_command->source, buffer_get_data_command->destination, buffer_get_data_command->region);
			} break;
			case RecordedCommand::TYPE_BUFFER_UPDATE: {
				const RecordedBufferUpdateCommand *buffer_update_command = reinterpret_cast<const RecordedBufferUpdateCommand *>(command);
				const RecordedBufferCopy *command_buffer_copies = buffer_update_command->buffer_copies();
				for (uint32_t j = 0; j < buffer_update_command->buffer_copies_count; j++) {
					driver->command_copy_buffer(r_command_buffer, command_buffer_copies[j].source, buffer_update_command->destination, command_buffer_copies[j].region);
				}
			} break;
			case RecordedCommand::TYPE_COMPUTE_LIST: {
				if (device.workarounds.avoid_compute_after_draw && workarounds_state.draw_list_found) {
					// Avoid compute after draw workaround. Refer to the comment that enables this in the Vulkan driver for more information.
					workarounds_state.draw_list_found = false;

					// Create or reuse a command buffer and finish recording the current one.
					driver->command_buffer_end(r_command_buffer);

					while (r_command_buffer_pool.buffers_used >= r_command_buffer_pool.buffers.size()) {
						RDD::CommandBufferID command_buffer = driver->command_buffer_create(r_command_buffer_pool.pool);
						RDD::SemaphoreID command_semaphore = driver->semaphore_create();
						r_command_buffer_pool.buffers.push_back(command_buffer);
						r_command_buffer_pool.semaphores.push_back(command_semaphore);
					}

					// Start recording on the next usable command buffer from the pool.
					uint32_t command_buffer_index = r_command_buffer_pool.buffers_used++;
					r_command_buffer = r_command_buffer_pool.buffers[command_buffer_index];
					driver->command_buffer_begin(r_command_buffer);
				}

				const RecordedComputeListCommand *compute_list_command = reinterpret_cast<const RecordedComputeListCommand *>(command);
				_run_compute_list_command(r_command_buffer, compute_list_command->instruction_data(), compute_list_command->instruction_data_size);
			} break;
			case RecordedCommand::TYPE_DRAW_LIST: {
				if (device.workarounds.avoid_compute_after_draw) {
					// Indicate that a draw list was encountered for the workaround.
					workarounds_state.draw_list_found = true;
				}

				const RecordedDrawListCommand *draw_list_command = reinterpret_cast<const RecordedDrawListCommand *>(command);

				if (draw_list_command->split_cmd_buffer) {
					// Create or reuse a command buffer and finish recording the current one.
					driver->command_buffer_end(r_command_buffer);

					while (r_command_buffer_pool.buffers_used >= r_command_buffer_pool.buffers.size()) {
						RDD::CommandBufferID command_buffer = driver->command_buffer_create(r_command_buffer_pool.pool);
						RDD::SemaphoreID command_semaphore = driver->semaphore_create();
						r_command_buffer_pool.buffers.push_back(command_buffer);
						r_command_buffer_pool.semaphores.push_back(command_semaphore);
					}

					// Start recording on the next usable command buffer from the pool.
					uint32_t command_buffer_index = r_command_buffer_pool.buffers_used++;
					r_command_buffer = r_command_buffer_pool.buffers[command_buffer_index];
					driver->command_buffer_begin(r_command_buffer);
				}

				const VectorView clear_values(draw_list_command->clear_values(), draw_list_command->clear_values_count);
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
				driver->command_insert_breadcrumb(r_command_buffer, draw_list_command->breadcrumb);
#endif
				RDD::RenderPassID render_pass;
				RDD::FramebufferID framebuffer;
				if (draw_list_command->framebuffer_cache != nullptr) {
					_get_draw_list_render_pass_and_framebuffer(draw_list_command, render_pass, framebuffer);
				} else {
					render_pass = draw_list_command->render_pass;
					framebuffer = draw_list_command->framebuffer;
				}

				if (framebuffer && render_pass) {
					driver->command_begin_render_pass(r_command_buffer, render_pass, framebuffer, draw_list_command->command_buffer_type, draw_list_command->region, clear_values);
					_run_draw_list_command(r_command_buffer, draw_list_command->instruction_data(), draw_list_command->instruction_data_size);
					driver->command_end_render_pass(r_command_buffer);
				}
			} break;
			case RecordedCommand::TYPE_TEXTURE_CLEAR: {
				const RecordedTextureClearCommand *texture_clear_command = reinterpret_cast<const RecordedTextureClearCommand *>(command);
				driver->command_clear_color_texture(r_command_buffer, texture_clear_command->texture, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, texture_clear_command->color, texture_clear_command->range);
			} break;
			case RecordedCommand::TYPE_TEXTURE_COPY: {
				const RecordedTextureCopyCommand *texture_copy_command = reinterpret_cast<const RecordedTextureCopyCommand *>(command);
				const VectorView<RDD::TextureCopyRegion> command_texture_copy_regions_view(texture_copy_command->texture_copy_regions(), texture_copy_command->texture_copy_regions_count);
				driver->command_copy_texture(r_command_buffer, texture_copy_command->from_texture, RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL, texture_copy_command->to_texture, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, command_texture_copy_regions_view);
			} break;
			case RecordedCommand::TYPE_TEXTURE_GET_DATA: {
				const RecordedTextureGetDataCommand *texture_get_data_command = reinterpret_cast<const RecordedTextureGetDataCommand *>(command);
				const VectorView<RDD::BufferTextureCopyRegion> command_buffer_texture_copy_regions_view(texture_get_data_command->buffer_texture_copy_regions(), texture_get_data_command->buffer_texture_copy_regions_count);
				driver->command_copy_texture_to_buffer(r_command_buffer, texture_get_data_command->from_texture, RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL, texture_get_data_command->to_buffer, command_buffer_texture_copy_regions_view);
			} break;
			case RecordedCommand::TYPE_TEXTURE_RESOLVE: {
				const RecordedTextureResolveCommand *texture_resolve_command = reinterpret_cast<const RecordedTextureResolveCommand *>(command);
				driver->command_resolve_texture(r_command_buffer, texture_resolve_command->from_texture, RDD::TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL, texture_resolve_command->src_layer, texture_resolve_command->src_mipmap, texture_resolve_command->to_texture, RDD::TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL, texture_resolve_command->dst_layer, texture_resolve_command->dst_mipmap);
			} break;
			case RecordedCommand::TYPE_TEXTURE_UPDATE: {
				const RecordedTextureUpdateCommand *texture_update_command = reinterpret_cast<const RecordedTextureUpdateCommand *>(command);
				const RecordedBufferToTextureCopy *command_buffer_to_texture_copies = texture_update_command->buffer_to_texture_copies();
				for (uint32_t j = 0; j < texture_update_command->buffer_to_texture_copies_count; j++) {
					driver->command_copy_buffer_to_texture(r_command_buffer, command_buffer_to_texture_copies[j].from_buffer, texture_update_command->to_texture, RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, command_buffer_to_texture_copies[j].region);
				}
			} break;
			case RecordedCommand::TYPE_CAPTURE_TIMESTAMP: {
				const RecordedCaptureTimestampCommand *texture_capture_timestamp_command = reinterpret_cast<const RecordedCaptureTimestampCommand *>(command);
				driver->command_timestamp_write(r_command_buffer, texture_capture_timestamp_command->pool, texture_capture_timestamp_command->index);
			} break;
			default: {
				DEV_ASSERT(false && "Unknown recorded command type.");
				return;
			}
		}
	}
}

void RenderingDeviceGraph::_run_label_command_change(RDD::CommandBufferID p_command_buffer, int32_t p_new_label_index, int32_t p_new_level, bool p_ignore_previous_value, bool p_use_label_for_empty, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, int32_t &r_current_label_index, int32_t &r_current_label_level) {
	if (command_label_count == 0) {
		// Ignore any label operations if no labels were pushed.
		return;
	}

	if (p_ignore_previous_value || p_new_label_index != r_current_label_index || p_new_level != r_current_label_level) {
		if (!p_ignore_previous_value && (p_use_label_for_empty || r_current_label_index >= 0 || r_current_label_level >= 0)) {
			// End the current label.
			driver->command_end_label(p_command_buffer);
		}

		String label_name;
		Color label_color;
		if (p_new_label_index >= 0) {
			const char *label_chars = &command_label_chars[command_label_offsets[p_new_label_index]];
			label_name.parse_utf8(label_chars);
			label_color = command_label_colors[p_new_label_index];
		} else if (p_use_label_for_empty) {
			label_name = "Command graph";
			label_color = Color(1, 1, 1, 1);
		} else {
			return;
		}

		// Add the level to the name.
		label_name += " (L" + itos(p_new_level) + ")";

		if (p_sorted_commands != nullptr && p_sorted_commands_count > 0) {
			// Analyze the commands in the level that have the same label to detect what type of operations are performed.
			bool copy_commands = false;
			bool compute_commands = false;
			bool draw_commands = false;
			for (uint32_t i = 0; i < p_sorted_commands_count; i++) {
				const uint32_t command_index = p_sorted_commands[i].index;
				const uint32_t command_data_offset = command_data_offsets[command_index];
				const RecordedCommand *command = reinterpret_cast<RecordedCommand *>(&command_data[command_data_offset]);
				if (command->label_index != p_new_label_index) {
					break;
				}

				switch (command->type) {
					case RecordedCommand::TYPE_BUFFER_CLEAR:
					case RecordedCommand::TYPE_BUFFER_COPY:
					case RecordedCommand::TYPE_BUFFER_GET_DATA:
					case RecordedCommand::TYPE_BUFFER_UPDATE:
					case RecordedCommand::TYPE_TEXTURE_CLEAR:
					case RecordedCommand::TYPE_TEXTURE_COPY:
					case RecordedCommand::TYPE_TEXTURE_GET_DATA:
					case RecordedCommand::TYPE_TEXTURE_RESOLVE:
					case RecordedCommand::TYPE_TEXTURE_UPDATE: {
						copy_commands = true;
					} break;
					case RecordedCommand::TYPE_COMPUTE_LIST: {
						compute_commands = true;
					} break;
					case RecordedCommand::TYPE_DRAW_LIST: {
						draw_commands = true;
					} break;
					default: {
						// Ignore command.
					} break;
				}

				if (copy_commands && compute_commands && draw_commands) {
					// There's no more command types to find.
					break;
				}
			}

			if (copy_commands || compute_commands || draw_commands) {
				// Add the operations to the name.
				bool plus_after_copy = copy_commands && (compute_commands || draw_commands);
				bool plus_after_compute = compute_commands && draw_commands;
				label_name += " (";
				label_name += copy_commands ? "Copy" : "";
				label_name += plus_after_copy ? "+" : "";
				label_name += compute_commands ? "Compute" : "";
				label_name += plus_after_compute ? "+" : "";
				label_name += draw_commands ? "Draw" : "";
				label_name += ")";
			}
		}

		// Start the new label.
		CharString label_name_utf8 = label_name.utf8();
		driver->command_begin_label(p_command_buffer, label_name_utf8.get_data(), label_color);

		r_current_label_index = p_new_label_index;
		r_current_label_level = p_new_level;
	}
}

void RenderingDeviceGraph::_boost_priority_for_render_commands(RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, uint32_t &r_boosted_priority) {
	if (p_sorted_commands_count == 0) {
		return;
	}

	const uint32_t boosted_priority_value = 0;
	if (r_boosted_priority > 0) {
		bool perform_sort = false;
		for (uint32_t j = 0; j < p_sorted_commands_count; j++) {
			if (p_sorted_commands[j].priority == r_boosted_priority) {
				p_sorted_commands[j].priority = boosted_priority_value;
				perform_sort = true;
			}
		}

		if (perform_sort) {
			SortArray<RecordedCommandSort> command_sorter;
			command_sorter.sort(p_sorted_commands, p_sorted_commands_count);
		}
	}

	if (p_sorted_commands[p_sorted_commands_count - 1].priority != boosted_priority_value) {
		r_boosted_priority = p_sorted_commands[p_sorted_commands_count - 1].priority;
	}
}

void RenderingDeviceGraph::_group_barriers_for_render_commands(RDD::CommandBufferID p_command_buffer, const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count, bool p_full_memory_barrier) {
	if (!driver_honors_barriers) {
		return;
	}

	barrier_group.clear();
	barrier_group.src_stages = RDD::PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	barrier_group.dst_stages = RDD::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	for (uint32_t i = 0; i < p_sorted_commands_count; i++) {
		const uint32_t command_index = p_sorted_commands[i].index;
		const uint32_t command_data_offset = command_data_offsets[command_index];
		const RecordedCommand *command = reinterpret_cast<RecordedCommand *>(&command_data[command_data_offset]);

#if PRINT_COMMAND_RECORDING
		print_line(vformat("Grouping barriers for #%d", command_index));
#endif

		// Merge command's stage bits with the barrier group.
		barrier_group.src_stages = barrier_group.src_stages | command->previous_stages;
		barrier_group.dst_stages = barrier_group.dst_stages | command->next_stages;

		// Merge command's memory barrier bits with the barrier group.
		barrier_group.memory_barrier.src_access = barrier_group.memory_barrier.src_access | command->memory_barrier.src_access;
		barrier_group.memory_barrier.dst_access = barrier_group.memory_barrier.dst_access | command->memory_barrier.dst_access;

		// Gather texture barriers.
		for (int32_t j = 0; j < command->normalization_barrier_count; j++) {
			const RDD::TextureBarrier &recorded_barrier = command_normalization_barriers[command->normalization_barrier_index + j];
			barrier_group.normalization_barriers.push_back(recorded_barrier);
#if PRINT_COMMAND_RECORDING
			print_line(vformat("Normalization Barrier #%d", barrier_group.normalization_barriers.size() - 1));
#endif
		}

		for (int32_t j = 0; j < command->transition_barrier_count; j++) {
			const RDD::TextureBarrier &recorded_barrier = command_transition_barriers[command->transition_barrier_index + j];
			barrier_group.transition_barriers.push_back(recorded_barrier);
#if PRINT_COMMAND_RECORDING
			print_line(vformat("Transition Barrier #%d", barrier_group.transition_barriers.size() - 1));
#endif
		}

#if USE_BUFFER_BARRIERS
		// Gather buffer barriers.
		for (int32_t j = 0; j < command->buffer_barrier_count; j++) {
			const RDD::BufferBarrier &recorded_barrier = command_buffer_barriers[command->buffer_barrier_index + j];
			barrier_group.buffer_barriers.push_back(recorded_barrier);
		}
#endif
	}

	if (p_full_memory_barrier) {
		barrier_group.src_stages = RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT;
		barrier_group.dst_stages = RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT;
		barrier_group.memory_barrier.src_access = RDD::BARRIER_ACCESS_MEMORY_READ_BIT | RDD::BARRIER_ACCESS_MEMORY_WRITE_BIT;
		barrier_group.memory_barrier.dst_access = RDD::BARRIER_ACCESS_MEMORY_READ_BIT | RDD::BARRIER_ACCESS_MEMORY_WRITE_BIT;
	}

	const bool is_memory_barrier_empty = barrier_group.memory_barrier.src_access.is_empty() && barrier_group.memory_barrier.dst_access.is_empty();
	const bool are_texture_barriers_empty = barrier_group.normalization_barriers.is_empty() && barrier_group.transition_barriers.is_empty();
#if USE_BUFFER_BARRIERS
	const bool are_buffer_barriers_empty = barrier_group.buffer_barriers.is_empty();
#else
	const bool are_buffer_barriers_empty = true;
#endif
	if (is_memory_barrier_empty && are_texture_barriers_empty && are_buffer_barriers_empty) {
		// Commands don't require synchronization.
		return;
	}

	const VectorView<RDD::MemoryBarrier> memory_barriers = !is_memory_barrier_empty ? barrier_group.memory_barrier : VectorView<RDD::MemoryBarrier>();
	const VectorView<RDD::TextureBarrier> texture_barriers = barrier_group.normalization_barriers.is_empty() ? barrier_group.transition_barriers : barrier_group.normalization_barriers;
#if USE_BUFFER_BARRIERS
	const VectorView<RDD::BufferBarrier> buffer_barriers = !are_buffer_barriers_empty ? barrier_group.buffer_barriers : VectorView<RDD::BufferBarrier>();
#else
	const VectorView<RDD::BufferBarrier> buffer_barriers = VectorView<RDD::BufferBarrier>();
#endif

	driver->command_pipeline_barrier(p_command_buffer, barrier_group.src_stages, barrier_group.dst_stages, memory_barriers, buffer_barriers, texture_barriers);

	bool separate_texture_barriers = !barrier_group.normalization_barriers.is_empty() && !barrier_group.transition_barriers.is_empty();
	if (separate_texture_barriers) {
		driver->command_pipeline_barrier(p_command_buffer, barrier_group.src_stages, barrier_group.dst_stages, VectorView<RDD::MemoryBarrier>(), VectorView<RDD::BufferBarrier>(), barrier_group.transition_barriers);
	}
}

void RenderingDeviceGraph::_print_render_commands(const RecordedCommandSort *p_sorted_commands, uint32_t p_sorted_commands_count) {
	for (uint32_t i = 0; i < p_sorted_commands_count; i++) {
		const uint32_t command_index = p_sorted_commands[i].index;
		const uint32_t command_level = p_sorted_commands[i].level;
		const uint32_t command_data_offset = command_data_offsets[command_index];
		const RecordedCommand *command = reinterpret_cast<RecordedCommand *>(&command_data[command_data_offset]);
		switch (command->type) {
			case RecordedCommand::TYPE_BUFFER_CLEAR: {
				const RecordedBufferClearCommand *buffer_clear_command = reinterpret_cast<const RecordedBufferClearCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "BUFFER CLEAR DESTINATION", itos(buffer_clear_command->buffer.id));
			} break;
			case RecordedCommand::TYPE_BUFFER_COPY: {
				const RecordedBufferCopyCommand *buffer_copy_command = reinterpret_cast<const RecordedBufferCopyCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "BUFFER COPY SOURCE", itos(buffer_copy_command->source.id), "DESTINATION", itos(buffer_copy_command->destination.id));
			} break;
			case RecordedCommand::TYPE_BUFFER_GET_DATA: {
				const RecordedBufferGetDataCommand *buffer_get_data_command = reinterpret_cast<const RecordedBufferGetDataCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "BUFFER GET DATA DESTINATION", itos(buffer_get_data_command->destination.id));
			} break;
			case RecordedCommand::TYPE_BUFFER_UPDATE: {
				const RecordedBufferUpdateCommand *buffer_update_command = reinterpret_cast<const RecordedBufferUpdateCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "BUFFER UPDATE DESTINATION", itos(buffer_update_command->destination.id), "COPIES", buffer_update_command->buffer_copies_count);
			} break;
			case RecordedCommand::TYPE_COMPUTE_LIST: {
				const RecordedComputeListCommand *compute_list_command = reinterpret_cast<const RecordedComputeListCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "COMPUTE LIST SIZE", compute_list_command->instruction_data_size);
			} break;
			case RecordedCommand::TYPE_DRAW_LIST: {
				const RecordedDrawListCommand *draw_list_command = reinterpret_cast<const RecordedDrawListCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "DRAW LIST SIZE", draw_list_command->instruction_data_size);
			} break;
			case RecordedCommand::TYPE_TEXTURE_CLEAR: {
				const RecordedTextureClearCommand *texture_clear_command = reinterpret_cast<const RecordedTextureClearCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "TEXTURE CLEAR", itos(texture_clear_command->texture.id), "COLOR", texture_clear_command->color);
			} break;
			case RecordedCommand::TYPE_TEXTURE_COPY: {
				const RecordedTextureCopyCommand *texture_copy_command = reinterpret_cast<const RecordedTextureCopyCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "TEXTURE COPY FROM", itos(texture_copy_command->from_texture.id), "TO", itos(texture_copy_command->to_texture.id));
			} break;
			case RecordedCommand::TYPE_TEXTURE_GET_DATA: {
				print_line(command_index, "LEVEL", command_level, "TEXTURE GET DATA");
			} break;
			case RecordedCommand::TYPE_TEXTURE_RESOLVE: {
				const RecordedTextureResolveCommand *texture_resolve_command = reinterpret_cast<const RecordedTextureResolveCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "TEXTURE RESOLVE FROM", itos(texture_resolve_command->from_texture.id), "TO", itos(texture_resolve_command->to_texture.id));
			} break;
			case RecordedCommand::TYPE_TEXTURE_UPDATE: {
				const RecordedTextureUpdateCommand *texture_update_command = reinterpret_cast<const RecordedTextureUpdateCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "TEXTURE UPDATE TO", itos(texture_update_command->to_texture.id));
			} break;
			case RecordedCommand::TYPE_CAPTURE_TIMESTAMP: {
				const RecordedCaptureTimestampCommand *texture_capture_timestamp_command = reinterpret_cast<const RecordedCaptureTimestampCommand *>(command);
				print_line(command_index, "LEVEL", command_level, "CAPTURE TIMESTAMP POOL", itos(texture_capture_timestamp_command->pool.id), "INDEX", texture_capture_timestamp_command->index);
			} break;
			default:
				DEV_ASSERT(false && "Unknown recorded command type.");
				return;
		}
	}
}

void RenderingDeviceGraph::_print_draw_list(const uint8_t *p_instruction_data, uint32_t p_instruction_data_size) {
	uint32_t instruction_data_cursor = 0;
	while (instruction_data_cursor < p_instruction_data_size) {
		DEV_ASSERT((instruction_data_cursor + sizeof(DrawListInstruction)) <= p_instruction_data_size);

		const DrawListInstruction *instruction = reinterpret_cast<const DrawListInstruction *>(&p_instruction_data[instruction_data_cursor]);
		switch (instruction->type) {
			case DrawListInstruction::TYPE_BIND_INDEX_BUFFER: {
				const DrawListBindIndexBufferInstruction *bind_index_buffer_instruction = reinterpret_cast<const DrawListBindIndexBufferInstruction *>(instruction);
				print_line("\tBIND INDEX BUFFER ID", itos(bind_index_buffer_instruction->buffer.id), "FORMAT", bind_index_buffer_instruction->format, "OFFSET", bind_index_buffer_instruction->offset);
				instruction_data_cursor += sizeof(DrawListBindIndexBufferInstruction);
			} break;
			case DrawListInstruction::TYPE_BIND_PIPELINE: {
				const DrawListBindPipelineInstruction *bind_pipeline_instruction = reinterpret_cast<const DrawListBindPipelineInstruction *>(instruction);
				print_line("\tBIND PIPELINE ID", itos(bind_pipeline_instruction->pipeline.id));
				instruction_data_cursor += sizeof(DrawListBindPipelineInstruction);
			} break;
			case DrawListInstruction::TYPE_BIND_UNIFORM_SETS: {
				const DrawListBindUniformSetsInstruction *bind_uniform_sets_instruction = reinterpret_cast<const DrawListBindUniformSetsInstruction *>(instruction);
				print_line("\tBIND UNIFORM SETS COUNT", bind_uniform_sets_instruction->set_count);
				for (uint32_t i = 0; i < bind_uniform_sets_instruction->set_count; i++) {
					print_line("\tBIND UNIFORM SET ID", itos(bind_uniform_sets_instruction->uniform_set_ids()[i].id), "START INDEX", bind_uniform_sets_instruction->first_set_index);
				}
				instruction_data_cursor += sizeof(DrawListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * bind_uniform_sets_instruction->set_count;
			} break;
			case DrawListInstruction::TYPE_BIND_VERTEX_BUFFERS: {
				const DrawListBindVertexBuffersInstruction *bind_vertex_buffers_instruction = reinterpret_cast<const DrawListBindVertexBuffersInstruction *>(instruction);
				print_line("\tBIND VERTEX BUFFERS COUNT", bind_vertex_buffers_instruction->vertex_buffers_count);
				instruction_data_cursor += sizeof(DrawListBindVertexBuffersInstruction);
				instruction_data_cursor += sizeof(RDD::BufferID) * bind_vertex_buffers_instruction->vertex_buffers_count;
				instruction_data_cursor += sizeof(uint64_t) * bind_vertex_buffers_instruction->vertex_buffers_count;
			} break;
			case DrawListInstruction::TYPE_CLEAR_ATTACHMENTS: {
				const DrawListClearAttachmentsInstruction *clear_attachments_instruction = reinterpret_cast<const DrawListClearAttachmentsInstruction *>(instruction);
				print_line("\tATTACHMENTS CLEAR COUNT", clear_attachments_instruction->attachments_clear_count, "RECT COUNT", clear_attachments_instruction->attachments_clear_rect_count);
				instruction_data_cursor += sizeof(DrawListClearAttachmentsInstruction);
				instruction_data_cursor += sizeof(RDD::AttachmentClear) * clear_attachments_instruction->attachments_clear_count;
				instruction_data_cursor += sizeof(Rect2i) * clear_attachments_instruction->attachments_clear_rect_count;
			} break;
			case DrawListInstruction::TYPE_DRAW: {
				const DrawListDrawInstruction *draw_instruction = reinterpret_cast<const DrawListDrawInstruction *>(instruction);
				print_line("\tDRAW VERTICES", draw_instruction->vertex_count, "INSTANCES", draw_instruction->instance_count);
				instruction_data_cursor += sizeof(DrawListDrawInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDEXED: {
				const DrawListDrawIndexedInstruction *draw_indexed_instruction = reinterpret_cast<const DrawListDrawIndexedInstruction *>(instruction);
				print_line("\tDRAW INDICES", draw_indexed_instruction->index_count, "INSTANCES", draw_indexed_instruction->instance_count, "FIRST INDEX", draw_indexed_instruction->first_index);
				instruction_data_cursor += sizeof(DrawListDrawIndexedInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDIRECT: {
				const DrawListDrawIndirectInstruction *draw_indirect_instruction = reinterpret_cast<const DrawListDrawIndirectInstruction *>(instruction);
				print_line("\tDRAW INDIRECT BUFFER ID", itos(draw_indirect_instruction->buffer.id), "OFFSET", draw_indirect_instruction->offset, "DRAW COUNT", draw_indirect_instruction->draw_count, "STRIDE", draw_indirect_instruction->stride);
				instruction_data_cursor += sizeof(DrawListDrawIndirectInstruction);
			} break;
			case DrawListInstruction::TYPE_DRAW_INDEXED_INDIRECT: {
				const DrawListDrawIndexedIndirectInstruction *draw_indexed_indirect_instruction = reinterpret_cast<const DrawListDrawIndexedIndirectInstruction *>(instruction);
				print_line("\tDRAW INDEXED INDIRECT BUFFER ID", itos(draw_indexed_indirect_instruction->buffer.id), "OFFSET", draw_indexed_indirect_instruction->offset, "DRAW COUNT", draw_indexed_indirect_instruction->draw_count, "STRIDE", draw_indexed_indirect_instruction->stride);
				instruction_data_cursor += sizeof(DrawListDrawIndexedIndirectInstruction);
			} break;
			case DrawListInstruction::TYPE_EXECUTE_COMMANDS: {
				print_line("\tEXECUTE COMMANDS");
				instruction_data_cursor += sizeof(DrawListExecuteCommandsInstruction);
			} break;
			case DrawListInstruction::TYPE_NEXT_SUBPASS: {
				print_line("\tNEXT SUBPASS");
				instruction_data_cursor += sizeof(DrawListNextSubpassInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_BLEND_CONSTANTS: {
				const DrawListSetBlendConstantsInstruction *set_blend_constants_instruction = reinterpret_cast<const DrawListSetBlendConstantsInstruction *>(instruction);
				print_line("\tSET BLEND CONSTANTS COLOR", set_blend_constants_instruction->color);
				instruction_data_cursor += sizeof(DrawListSetBlendConstantsInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_LINE_WIDTH: {
				const DrawListSetLineWidthInstruction *set_line_width_instruction = reinterpret_cast<const DrawListSetLineWidthInstruction *>(instruction);
				print_line("\tSET LINE WIDTH", set_line_width_instruction->width);
				instruction_data_cursor += sizeof(DrawListSetLineWidthInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_PUSH_CONSTANT: {
				const DrawListSetPushConstantInstruction *set_push_constant_instruction = reinterpret_cast<const DrawListSetPushConstantInstruction *>(instruction);
				print_line("\tSET PUSH CONSTANT SIZE", set_push_constant_instruction->size);
				instruction_data_cursor += sizeof(DrawListSetPushConstantInstruction);
				instruction_data_cursor += set_push_constant_instruction->size;
			} break;
			case DrawListInstruction::TYPE_SET_SCISSOR: {
				const DrawListSetScissorInstruction *set_scissor_instruction = reinterpret_cast<const DrawListSetScissorInstruction *>(instruction);
				print_line("\tSET SCISSOR", set_scissor_instruction->rect);
				instruction_data_cursor += sizeof(DrawListSetScissorInstruction);
			} break;
			case DrawListInstruction::TYPE_SET_VIEWPORT: {
				const DrawListSetViewportInstruction *set_viewport_instruction = reinterpret_cast<const DrawListSetViewportInstruction *>(instruction);
				print_line("\tSET VIEWPORT", set_viewport_instruction->rect);
				instruction_data_cursor += sizeof(DrawListSetViewportInstruction);
			} break;
			case DrawListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE: {
				const DrawListUniformSetPrepareForUseInstruction *uniform_set_prepare_for_use_instruction = reinterpret_cast<const DrawListUniformSetPrepareForUseInstruction *>(instruction);
				print_line("\tUNIFORM SET PREPARE FOR USE ID", itos(uniform_set_prepare_for_use_instruction->uniform_set.id), "SHADER ID", itos(uniform_set_prepare_for_use_instruction->shader.id), "INDEX", uniform_set_prepare_for_use_instruction->set_index);
				instruction_data_cursor += sizeof(DrawListUniformSetPrepareForUseInstruction);
			} break;
			default:
				DEV_ASSERT(false && "Unknown draw list instruction type.");
				return;
		}
	}
}

void RenderingDeviceGraph::_print_compute_list(const uint8_t *p_instruction_data, uint32_t p_instruction_data_size) {
	uint32_t instruction_data_cursor = 0;
	while (instruction_data_cursor < p_instruction_data_size) {
		DEV_ASSERT((instruction_data_cursor + sizeof(ComputeListInstruction)) <= p_instruction_data_size);

		const ComputeListInstruction *instruction = reinterpret_cast<const ComputeListInstruction *>(&p_instruction_data[instruction_data_cursor]);
		switch (instruction->type) {
			case ComputeListInstruction::TYPE_BIND_PIPELINE: {
				const ComputeListBindPipelineInstruction *bind_pipeline_instruction = reinterpret_cast<const ComputeListBindPipelineInstruction *>(instruction);
				print_line("\tBIND PIPELINE ID", itos(bind_pipeline_instruction->pipeline.id));
				instruction_data_cursor += sizeof(ComputeListBindPipelineInstruction);
			} break;
			case ComputeListInstruction::TYPE_BIND_UNIFORM_SETS: {
				const ComputeListBindUniformSetsInstruction *bind_uniform_sets_instruction = reinterpret_cast<const ComputeListBindUniformSetsInstruction *>(instruction);
				print_line("\tBIND UNIFORM SETS COUNT", bind_uniform_sets_instruction->set_count);
				for (uint32_t i = 0; i < bind_uniform_sets_instruction->set_count; i++) {
					print_line("\tBIND UNIFORM SET ID", itos(bind_uniform_sets_instruction->uniform_set_ids()[i].id), "START INDEX", bind_uniform_sets_instruction->first_set_index);
				}
				instruction_data_cursor += sizeof(ComputeListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * bind_uniform_sets_instruction->set_count;
			} break;
			case ComputeListInstruction::TYPE_DISPATCH: {
				const ComputeListDispatchInstruction *dispatch_instruction = reinterpret_cast<const ComputeListDispatchInstruction *>(instruction);
				print_line("\tDISPATCH", dispatch_instruction->x_groups, dispatch_instruction->y_groups, dispatch_instruction->z_groups);
				instruction_data_cursor += sizeof(ComputeListDispatchInstruction);
			} break;
			case ComputeListInstruction::TYPE_DISPATCH_INDIRECT: {
				const ComputeListDispatchIndirectInstruction *dispatch_indirect_instruction = reinterpret_cast<const ComputeListDispatchIndirectInstruction *>(instruction);
				print_line("\tDISPATCH INDIRECT BUFFER ID", itos(dispatch_indirect_instruction->buffer.id), "OFFSET", dispatch_indirect_instruction->offset);
				instruction_data_cursor += sizeof(ComputeListDispatchIndirectInstruction);
			} break;
			case ComputeListInstruction::TYPE_SET_PUSH_CONSTANT: {
				const ComputeListSetPushConstantInstruction *set_push_constant_instruction = reinterpret_cast<const ComputeListSetPushConstantInstruction *>(instruction);
				print_line("\tSET PUSH CONSTANT SIZE", set_push_constant_instruction->size);
				instruction_data_cursor += sizeof(ComputeListSetPushConstantInstruction);
				instruction_data_cursor += set_push_constant_instruction->size;
			} break;
			case ComputeListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE: {
				const ComputeListUniformSetPrepareForUseInstruction *uniform_set_prepare_for_use_instruction = reinterpret_cast<const ComputeListUniformSetPrepareForUseInstruction *>(instruction);
				print_line("\tUNIFORM SET PREPARE FOR USE ID", itos(uniform_set_prepare_for_use_instruction->uniform_set.id), "SHADER ID", itos(uniform_set_prepare_for_use_instruction->shader.id), "INDEX", itos(uniform_set_prepare_for_use_instruction->set_index));
				instruction_data_cursor += sizeof(ComputeListUniformSetPrepareForUseInstruction);
			} break;
			default:
				DEV_ASSERT(false && "Unknown compute list instruction type.");
				return;
		}
	}
}

void RenderingDeviceGraph::initialize(RDD *p_driver, RenderingContextDriver::Device p_device, RenderPassCreationFunction p_render_pass_creation_function, uint32_t p_frame_count, RDD::CommandQueueFamilyID p_secondary_command_queue_family, uint32_t p_secondary_command_buffers_per_frame) {
	DEV_ASSERT(p_driver != nullptr);
	DEV_ASSERT(p_render_pass_creation_function != nullptr);
	DEV_ASSERT(p_frame_count > 0);

	driver = p_driver;
	device = p_device;
	render_pass_creation_function = p_render_pass_creation_function;
	frames.resize(p_frame_count);

	for (uint32_t i = 0; i < p_frame_count; i++) {
		frames[i].secondary_command_buffers.resize(p_secondary_command_buffers_per_frame);

		for (uint32_t j = 0; j < p_secondary_command_buffers_per_frame; j++) {
			SecondaryCommandBuffer &secondary = frames[i].secondary_command_buffers[j];
			secondary.command_pool = driver->command_pool_create(p_secondary_command_queue_family, RDD::COMMAND_BUFFER_TYPE_SECONDARY);
			secondary.command_buffer = driver->command_buffer_create(secondary.command_pool);
			secondary.task = WorkerThreadPool::INVALID_TASK_ID;
		}
	}

	driver_honors_barriers = driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS);
	driver_clears_with_copy_engine = driver->api_trait_get(RDD::API_TRAIT_CLEARS_WITH_COPY_ENGINE);
	driver_buffers_require_transitions = driver->api_trait_get(RDD::API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS);
}

void RenderingDeviceGraph::finalize() {
	_wait_for_secondary_command_buffer_tasks();

	for (Frame &f : frames) {
		for (SecondaryCommandBuffer &secondary : f.secondary_command_buffers) {
			if (secondary.command_pool.id != 0) {
				driver->command_pool_free(secondary.command_pool);
			}
		}
	}

	frames.clear();
}

void RenderingDeviceGraph::begin() {
	command_data.clear();
	command_data_offsets.clear();
	command_normalization_barriers.clear();
	command_transition_barriers.clear();
	command_buffer_barriers.clear();
	command_label_chars.clear();
	command_label_colors.clear();
	command_label_offsets.clear();
	command_list_nodes.clear();
	read_slice_list_nodes.clear();
	write_slice_list_nodes.clear();
	command_count = 0;
	command_label_count = 0;
	command_timestamp_index = -1;
	command_synchronization_index = -1;
	command_synchronization_pending = false;
	command_label_index = -1;
	frames[frame].secondary_command_buffers_used = 0;
	draw_instruction_list.index = 0;
	compute_instruction_list.index = 0;
	tracking_frame++;

#ifdef DEV_ENABLED
	write_dependency_counters.clear();
#endif
}

void RenderingDeviceGraph::add_buffer_clear(RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, uint32_t p_offset, uint32_t p_size) {
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	RecordedBufferClearCommand *command = static_cast<RecordedBufferClearCommand *>(_allocate_command(sizeof(RecordedBufferClearCommand), command_index));
	command->type = RecordedCommand::TYPE_BUFFER_CLEAR;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->buffer = p_dst;
	command->offset = p_offset;
	command->size = p_size;

	ResourceUsage usage = RESOURCE_USAGE_COPY_TO;
	_add_command_to_graph(&p_dst_tracker, &usage, 1, command_index, command);
}

void RenderingDeviceGraph::add_buffer_copy(RDD::BufferID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, RDD::BufferCopyRegion p_region) {
	// Source tracker is allowed to be null as it could be a read-only buffer.
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	RecordedBufferCopyCommand *command = static_cast<RecordedBufferCopyCommand *>(_allocate_command(sizeof(RecordedBufferCopyCommand), command_index));
	command->type = RecordedCommand::TYPE_BUFFER_COPY;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->source = p_src;
	command->destination = p_dst;
	command->region = p_region;

	ResourceTracker *trackers[2] = { p_dst_tracker, p_src_tracker };
	ResourceUsage usages[2] = { RESOURCE_USAGE_COPY_TO, RESOURCE_USAGE_COPY_FROM };
	_add_command_to_graph(trackers, usages, p_src_tracker != nullptr ? 2 : 1, command_index, command);
}

void RenderingDeviceGraph::add_buffer_get_data(RDD::BufferID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, RDD::BufferCopyRegion p_region) {
	// Source tracker is allowed to be null as it could be a read-only buffer.
	int32_t command_index;
	RecordedBufferGetDataCommand *command = static_cast<RecordedBufferGetDataCommand *>(_allocate_command(sizeof(RecordedBufferGetDataCommand), command_index));
	command->type = RecordedCommand::TYPE_BUFFER_GET_DATA;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->source = p_src;
	command->destination = p_dst;
	command->region = p_region;

	if (p_src_tracker != nullptr) {
		ResourceUsage usage = RESOURCE_USAGE_COPY_FROM;
		_add_command_to_graph(&p_src_tracker, &usage, 1, command_index, command);
	} else {
		_add_command_to_graph(nullptr, nullptr, 0, command_index, command);
	}
}

void RenderingDeviceGraph::add_buffer_update(RDD::BufferID p_dst, ResourceTracker *p_dst_tracker, VectorView<RecordedBufferCopy> p_buffer_copies) {
	DEV_ASSERT(p_dst_tracker != nullptr);

	size_t buffer_copies_size = p_buffer_copies.size() * sizeof(RecordedBufferCopy);
	uint64_t command_size = sizeof(RecordedBufferUpdateCommand) + buffer_copies_size;
	int32_t command_index;
	RecordedBufferUpdateCommand *command = static_cast<RecordedBufferUpdateCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_BUFFER_UPDATE;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->destination = p_dst;
	command->buffer_copies_count = p_buffer_copies.size();

	RecordedBufferCopy *buffer_copies = command->buffer_copies();
	for (uint32_t i = 0; i < command->buffer_copies_count; i++) {
		buffer_copies[i] = p_buffer_copies[i];
	}

	ResourceUsage buffer_usage = RESOURCE_USAGE_COPY_TO;
	_add_command_to_graph(&p_dst_tracker, &buffer_usage, 1, command_index, command);
}

void RenderingDeviceGraph::add_compute_list_begin(RDD::BreadcrumbMarker p_phase, uint32_t p_breadcrumb_data) {
	compute_instruction_list.clear();
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	compute_instruction_list.breadcrumb = p_breadcrumb_data | (p_phase & ((1 << 16) - 1));
#endif
	compute_instruction_list.index++;
}

void RenderingDeviceGraph::add_compute_list_bind_pipeline(RDD::PipelineID p_pipeline) {
	ComputeListBindPipelineInstruction *instruction = reinterpret_cast<ComputeListBindPipelineInstruction *>(_allocate_compute_list_instruction(sizeof(ComputeListBindPipelineInstruction)));
	instruction->type = ComputeListInstruction::TYPE_BIND_PIPELINE;
	instruction->pipeline = p_pipeline;
	compute_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void RenderingDeviceGraph::add_compute_list_bind_uniform_set(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index) {
	add_compute_list_bind_uniform_sets(p_shader, VectorView(&p_uniform_set, 1), set_index, 1);
}

void RenderingDeviceGraph::add_compute_list_bind_uniform_sets(RDD::ShaderID p_shader, VectorView<RDD::UniformSetID> p_uniform_sets, uint32_t p_first_set_index, uint32_t p_set_count) {
	DEV_ASSERT(p_uniform_sets.size() >= p_set_count);

	uint32_t instruction_size = sizeof(ComputeListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * p_set_count;
	ComputeListBindUniformSetsInstruction *instruction = reinterpret_cast<ComputeListBindUniformSetsInstruction *>(_allocate_compute_list_instruction(instruction_size));
	instruction->type = ComputeListInstruction::TYPE_BIND_UNIFORM_SETS;
	instruction->shader = p_shader;
	instruction->first_set_index = p_first_set_index;
	instruction->set_count = p_set_count;

	RDD::UniformSetID *ids = instruction->uniform_set_ids();
	for (uint32_t i = 0; i < p_set_count; i++) {
		ids[i] = p_uniform_sets[i];
	}
}

void RenderingDeviceGraph::add_compute_list_dispatch(uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	ComputeListDispatchInstruction *instruction = reinterpret_cast<ComputeListDispatchInstruction *>(_allocate_compute_list_instruction(sizeof(ComputeListDispatchInstruction)));
	instruction->type = ComputeListInstruction::TYPE_DISPATCH;
	instruction->x_groups = p_x_groups;
	instruction->y_groups = p_y_groups;
	instruction->z_groups = p_z_groups;
}

void RenderingDeviceGraph::add_compute_list_dispatch_indirect(RDD::BufferID p_buffer, uint32_t p_offset) {
	ComputeListDispatchIndirectInstruction *instruction = reinterpret_cast<ComputeListDispatchIndirectInstruction *>(_allocate_compute_list_instruction(sizeof(ComputeListDispatchIndirectInstruction)));
	instruction->type = ComputeListInstruction::TYPE_DISPATCH_INDIRECT;
	instruction->buffer = p_buffer;
	instruction->offset = p_offset;
	compute_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_DRAW_INDIRECT_BIT);
}

void RenderingDeviceGraph::add_compute_list_set_push_constant(RDD::ShaderID p_shader, const void *p_data, uint32_t p_data_size) {
	uint32_t instruction_size = sizeof(ComputeListSetPushConstantInstruction) + p_data_size;
	ComputeListSetPushConstantInstruction *instruction = reinterpret_cast<ComputeListSetPushConstantInstruction *>(_allocate_compute_list_instruction(instruction_size));
	instruction->type = ComputeListInstruction::TYPE_SET_PUSH_CONSTANT;
	instruction->size = p_data_size;
	instruction->shader = p_shader;
	memcpy(instruction->data(), p_data, p_data_size);
}

void RenderingDeviceGraph::add_compute_list_uniform_set_prepare_for_use(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index) {
	ComputeListUniformSetPrepareForUseInstruction *instruction = reinterpret_cast<ComputeListUniformSetPrepareForUseInstruction *>(_allocate_compute_list_instruction(sizeof(ComputeListUniformSetPrepareForUseInstruction)));
	instruction->type = ComputeListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE;
	instruction->shader = p_shader;
	instruction->uniform_set = p_uniform_set;
	instruction->set_index = set_index;
}

void RenderingDeviceGraph::add_compute_list_usage(ResourceTracker *p_tracker, ResourceUsage p_usage) {
	DEV_ASSERT(p_tracker != nullptr);

	p_tracker->reset_if_outdated(tracking_frame);

	if (p_tracker->compute_list_index != compute_instruction_list.index) {
		compute_instruction_list.command_trackers.push_back(p_tracker);
		compute_instruction_list.command_tracker_usages.push_back(p_usage);
		p_tracker->compute_list_index = compute_instruction_list.index;
		p_tracker->compute_list_usage = p_usage;
	}
#ifdef DEV_ENABLED
	else if (p_tracker->compute_list_usage != p_usage) {
		ERR_FAIL_MSG(vformat("Tracker can't have more than one type of usage in the same compute list. Compute list usage is %s and the requested usage is %s.", _usage_to_string(p_tracker->compute_list_usage), _usage_to_string(p_usage)));
	}
#endif
}

void RenderingDeviceGraph::add_compute_list_usages(VectorView<ResourceTracker *> p_trackers, VectorView<ResourceUsage> p_usages) {
	DEV_ASSERT(p_trackers.size() == p_usages.size());

	for (uint32_t i = 0; i < p_trackers.size(); i++) {
		add_compute_list_usage(p_trackers[i], p_usages[i]);
	}
}

void RenderingDeviceGraph::add_compute_list_end() {
	int32_t command_index;
	uint32_t instruction_data_size = compute_instruction_list.data.size();
	uint32_t command_size = sizeof(RecordedComputeListCommand) + instruction_data_size;
	RecordedComputeListCommand *command = static_cast<RecordedComputeListCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_COMPUTE_LIST;
	command->self_stages = compute_instruction_list.stages;
	command->instruction_data_size = instruction_data_size;
	memcpy(command->instruction_data(), compute_instruction_list.data.ptr(), instruction_data_size);
	_add_command_to_graph(compute_instruction_list.command_trackers.ptr(), compute_instruction_list.command_tracker_usages.ptr(), compute_instruction_list.command_trackers.size(), command_index, command);
}

void RenderingDeviceGraph::add_draw_list_begin(FramebufferCache *p_framebuffer_cache, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb, bool p_split_cmd_buffer) {
	_add_draw_list_begin(p_framebuffer_cache, RDD::RenderPassID(), RDD::FramebufferID(), p_region, p_attachment_operations, p_attachment_clear_values, p_uses_color, p_uses_depth, p_breadcrumb, p_split_cmd_buffer);
}

void RenderingDeviceGraph::add_draw_list_begin(RDD::RenderPassID p_render_pass, RDD::FramebufferID p_framebuffer, Rect2i p_region, VectorView<AttachmentOperation> p_attachment_operations, VectorView<RDD::RenderPassClearValue> p_attachment_clear_values, bool p_uses_color, bool p_uses_depth, uint32_t p_breadcrumb, bool p_split_cmd_buffer) {
	_add_draw_list_begin(nullptr, p_render_pass, p_framebuffer, p_region, p_attachment_operations, p_attachment_clear_values, p_uses_color, p_uses_depth, p_breadcrumb, p_split_cmd_buffer);
}

void RenderingDeviceGraph::add_draw_list_bind_index_buffer(RDD::BufferID p_buffer, RDD::IndexBufferFormat p_format, uint32_t p_offset) {
	DrawListBindIndexBufferInstruction *instruction = reinterpret_cast<DrawListBindIndexBufferInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListBindIndexBufferInstruction)));
	instruction->type = DrawListInstruction::TYPE_BIND_INDEX_BUFFER;
	instruction->buffer = p_buffer;
	instruction->format = p_format;
	instruction->offset = p_offset;

	if (instruction->buffer.id != 0) {
		draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_VERTEX_INPUT_BIT);
	}
}

void RenderingDeviceGraph::add_draw_list_bind_pipeline(RDD::PipelineID p_pipeline, BitField<RDD::PipelineStageBits> p_pipeline_stage_bits) {
	DrawListBindPipelineInstruction *instruction = reinterpret_cast<DrawListBindPipelineInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListBindPipelineInstruction)));
	instruction->type = DrawListInstruction::TYPE_BIND_PIPELINE;
	instruction->pipeline = p_pipeline;
	draw_instruction_list.stages = draw_instruction_list.stages | p_pipeline_stage_bits;
}

void RenderingDeviceGraph::add_draw_list_bind_uniform_set(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index) {
	add_draw_list_bind_uniform_sets(p_shader, VectorView(&p_uniform_set, 1), set_index, 1);
}

void RenderingDeviceGraph::add_draw_list_bind_uniform_sets(RDD::ShaderID p_shader, VectorView<RDD::UniformSetID> p_uniform_sets, uint32_t p_first_index, uint32_t p_set_count) {
	DEV_ASSERT(p_uniform_sets.size() >= p_set_count);

	uint32_t instruction_size = sizeof(DrawListBindUniformSetsInstruction) + sizeof(RDD::UniformSetID) * p_set_count;
	DrawListBindUniformSetsInstruction *instruction = reinterpret_cast<DrawListBindUniformSetsInstruction *>(_allocate_draw_list_instruction(instruction_size));
	instruction->type = DrawListInstruction::TYPE_BIND_UNIFORM_SETS;
	instruction->shader = p_shader;
	instruction->first_set_index = p_first_index;
	instruction->set_count = p_set_count;

	for (uint32_t i = 0; i < p_set_count; i++) {
		instruction->uniform_set_ids()[i] = p_uniform_sets[i];
	}
}

void RenderingDeviceGraph::add_draw_list_bind_vertex_buffers(VectorView<RDD::BufferID> p_vertex_buffers, VectorView<uint64_t> p_vertex_buffer_offsets) {
	DEV_ASSERT(p_vertex_buffers.size() == p_vertex_buffer_offsets.size());

	uint32_t instruction_size = sizeof(DrawListBindVertexBuffersInstruction) + sizeof(RDD::BufferID) * p_vertex_buffers.size() + sizeof(uint64_t) * p_vertex_buffer_offsets.size();
	DrawListBindVertexBuffersInstruction *instruction = reinterpret_cast<DrawListBindVertexBuffersInstruction *>(_allocate_draw_list_instruction(instruction_size));
	instruction->type = DrawListInstruction::TYPE_BIND_VERTEX_BUFFERS;
	instruction->vertex_buffers_count = p_vertex_buffers.size();

	RDD::BufferID *vertex_buffers = instruction->vertex_buffers();
	uint64_t *vertex_buffer_offsets = instruction->vertex_buffer_offsets();
	for (uint32_t i = 0; i < instruction->vertex_buffers_count; i++) {
		vertex_buffers[i] = p_vertex_buffers[i];
		vertex_buffer_offsets[i] = p_vertex_buffer_offsets[i];
	}

	if (instruction->vertex_buffers_count > 0) {
		draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_VERTEX_INPUT_BIT);
	}
}

void RenderingDeviceGraph::add_draw_list_clear_attachments(VectorView<RDD::AttachmentClear> p_attachments_clear, VectorView<Rect2i> p_attachments_clear_rect) {
	uint32_t instruction_size = sizeof(DrawListClearAttachmentsInstruction) + sizeof(RDD::AttachmentClear) * p_attachments_clear.size() + sizeof(Rect2i) * p_attachments_clear_rect.size();
	DrawListClearAttachmentsInstruction *instruction = reinterpret_cast<DrawListClearAttachmentsInstruction *>(_allocate_draw_list_instruction(instruction_size));
	instruction->type = DrawListInstruction::TYPE_CLEAR_ATTACHMENTS;
	instruction->attachments_clear_count = p_attachments_clear.size();
	instruction->attachments_clear_rect_count = p_attachments_clear_rect.size();

	RDD::AttachmentClear *attachments_clear = instruction->attachments_clear();
	Rect2i *attachments_clear_rect = instruction->attachments_clear_rect();
	for (uint32_t i = 0; i < instruction->attachments_clear_count; i++) {
		attachments_clear[i] = p_attachments_clear[i];
	}

	for (uint32_t i = 0; i < instruction->attachments_clear_rect_count; i++) {
		attachments_clear_rect[i] = p_attachments_clear_rect[i];
	}
}

void RenderingDeviceGraph::add_draw_list_draw(uint32_t p_vertex_count, uint32_t p_instance_count) {
	DrawListDrawInstruction *instruction = reinterpret_cast<DrawListDrawInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListDrawInstruction)));
	instruction->type = DrawListInstruction::TYPE_DRAW;
	instruction->vertex_count = p_vertex_count;
	instruction->instance_count = p_instance_count;
}

void RenderingDeviceGraph::add_draw_list_draw_indexed(uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index) {
	DrawListDrawIndexedInstruction *instruction = reinterpret_cast<DrawListDrawIndexedInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListDrawIndexedInstruction)));
	instruction->type = DrawListInstruction::TYPE_DRAW_INDEXED;
	instruction->index_count = p_index_count;
	instruction->instance_count = p_instance_count;
	instruction->first_index = p_first_index;
}

void RenderingDeviceGraph::add_draw_list_draw_indirect(RDD::BufferID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	DrawListDrawIndirectInstruction *instruction = reinterpret_cast<DrawListDrawIndirectInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListDrawIndirectInstruction)));
	instruction->type = DrawListInstruction::TYPE_DRAW_INDIRECT;
	instruction->buffer = p_buffer;
	instruction->offset = p_offset;
	instruction->draw_count = p_draw_count;
	instruction->stride = p_stride;
	draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_DRAW_INDIRECT_BIT);
}

void RenderingDeviceGraph::add_draw_list_draw_indexed_indirect(RDD::BufferID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	DrawListDrawIndexedIndirectInstruction *instruction = reinterpret_cast<DrawListDrawIndexedIndirectInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListDrawIndexedIndirectInstruction)));
	instruction->type = DrawListInstruction::TYPE_DRAW_INDEXED_INDIRECT;
	instruction->buffer = p_buffer;
	instruction->offset = p_offset;
	instruction->draw_count = p_draw_count;
	instruction->stride = p_stride;
	draw_instruction_list.stages.set_flag(RDD::PIPELINE_STAGE_DRAW_INDIRECT_BIT);
}

void RenderingDeviceGraph::add_draw_list_execute_commands(RDD::CommandBufferID p_command_buffer) {
	DrawListExecuteCommandsInstruction *instruction = reinterpret_cast<DrawListExecuteCommandsInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListExecuteCommandsInstruction)));
	instruction->type = DrawListInstruction::TYPE_EXECUTE_COMMANDS;
	instruction->command_buffer = p_command_buffer;
}

void RenderingDeviceGraph::add_draw_list_next_subpass(RDD::CommandBufferType p_command_buffer_type) {
	DrawListNextSubpassInstruction *instruction = reinterpret_cast<DrawListNextSubpassInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListNextSubpassInstruction)));
	instruction->type = DrawListInstruction::TYPE_NEXT_SUBPASS;
	instruction->command_buffer_type = p_command_buffer_type;
}

void RenderingDeviceGraph::add_draw_list_set_blend_constants(const Color &p_color) {
	DrawListSetBlendConstantsInstruction *instruction = reinterpret_cast<DrawListSetBlendConstantsInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListSetBlendConstantsInstruction)));
	instruction->type = DrawListInstruction::TYPE_SET_BLEND_CONSTANTS;
	instruction->color = p_color;
}

void RenderingDeviceGraph::add_draw_list_set_line_width(float p_width) {
	DrawListSetLineWidthInstruction *instruction = reinterpret_cast<DrawListSetLineWidthInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListSetLineWidthInstruction)));
	instruction->type = DrawListInstruction::TYPE_SET_LINE_WIDTH;
	instruction->width = p_width;
}

void RenderingDeviceGraph::add_draw_list_set_push_constant(RDD::ShaderID p_shader, const void *p_data, uint32_t p_data_size) {
	uint32_t instruction_size = sizeof(DrawListSetPushConstantInstruction) + p_data_size;
	DrawListSetPushConstantInstruction *instruction = reinterpret_cast<DrawListSetPushConstantInstruction *>(_allocate_draw_list_instruction(instruction_size));
	instruction->type = DrawListInstruction::TYPE_SET_PUSH_CONSTANT;
	instruction->size = p_data_size;
	instruction->shader = p_shader;
	memcpy(instruction->data(), p_data, p_data_size);
}

void RenderingDeviceGraph::add_draw_list_set_scissor(Rect2i p_rect) {
	DrawListSetScissorInstruction *instruction = reinterpret_cast<DrawListSetScissorInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListSetScissorInstruction)));
	instruction->type = DrawListInstruction::TYPE_SET_SCISSOR;
	instruction->rect = p_rect;
}

void RenderingDeviceGraph::add_draw_list_set_viewport(Rect2i p_rect) {
	DrawListSetViewportInstruction *instruction = reinterpret_cast<DrawListSetViewportInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListSetViewportInstruction)));
	instruction->type = DrawListInstruction::TYPE_SET_VIEWPORT;
	instruction->rect = p_rect;
}

void RenderingDeviceGraph::add_draw_list_uniform_set_prepare_for_use(RDD::ShaderID p_shader, RDD::UniformSetID p_uniform_set, uint32_t set_index) {
	DrawListUniformSetPrepareForUseInstruction *instruction = reinterpret_cast<DrawListUniformSetPrepareForUseInstruction *>(_allocate_draw_list_instruction(sizeof(DrawListUniformSetPrepareForUseInstruction)));
	instruction->type = DrawListInstruction::TYPE_UNIFORM_SET_PREPARE_FOR_USE;
	instruction->shader = p_shader;
	instruction->uniform_set = p_uniform_set;
	instruction->set_index = set_index;
}

void RenderingDeviceGraph::add_draw_list_usage(ResourceTracker *p_tracker, ResourceUsage p_usage) {
	p_tracker->reset_if_outdated(tracking_frame);

	if (p_tracker->draw_list_index != draw_instruction_list.index) {
		draw_instruction_list.command_trackers.push_back(p_tracker);
		draw_instruction_list.command_tracker_usages.push_back(p_usage);
		p_tracker->draw_list_index = draw_instruction_list.index;
		p_tracker->draw_list_usage = p_usage;
	}
#ifdef DEV_ENABLED
	else if (p_tracker->draw_list_usage != p_usage) {
		ERR_FAIL_MSG(vformat("Tracker can't have more than one type of usage in the same draw list. Draw list usage is %s and the requested usage is %s.", _usage_to_string(p_tracker->draw_list_usage), _usage_to_string(p_usage)));
	}
#endif
}

void RenderingDeviceGraph::add_draw_list_usages(VectorView<ResourceTracker *> p_trackers, VectorView<ResourceUsage> p_usages) {
	DEV_ASSERT(p_trackers.size() == p_usages.size());

	for (uint32_t i = 0; i < p_trackers.size(); i++) {
		add_draw_list_usage(p_trackers[i], p_usages[i]);
	}
}

void RenderingDeviceGraph::add_draw_list_end() {
	FramebufferCache *framebuffer_cache = draw_instruction_list.framebuffer_cache;
	int32_t command_index;
	uint32_t clear_values_size = sizeof(RDD::RenderPassClearValue) * draw_instruction_list.attachment_clear_values.size();
	uint32_t trackers_count = framebuffer_cache != nullptr ? framebuffer_cache->trackers.size() : 0;
	uint32_t trackers_and_ops_size = (sizeof(ResourceTracker *) + sizeof(RDD::AttachmentLoadOp) + sizeof(RDD::AttachmentStoreOp)) * trackers_count;
	uint32_t instruction_data_size = draw_instruction_list.data.size();
	uint32_t command_size = sizeof(RecordedDrawListCommand) + clear_values_size + trackers_and_ops_size + instruction_data_size;
	RecordedDrawListCommand *command = static_cast<RecordedDrawListCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_DRAW_LIST;
	command->self_stages = draw_instruction_list.stages;
	command->framebuffer_cache = framebuffer_cache;
	command->render_pass = draw_instruction_list.render_pass;
	command->framebuffer = draw_instruction_list.framebuffer;
	command->instruction_data_size = instruction_data_size;
	command->command_buffer_type = RDD::COMMAND_BUFFER_TYPE_PRIMARY;
	command->region = draw_instruction_list.region;
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	command->breadcrumb = draw_instruction_list.breadcrumb;
#endif
	command->split_cmd_buffer = draw_instruction_list.split_cmd_buffer;
	command->clear_values_count = draw_instruction_list.attachment_clear_values.size();
	command->trackers_count = trackers_count;

	// Initialize the load and store operations to their default behaviors. The store behavior will be modified if a command depends on the result of this render pass.
	uint32_t attachment_op_count = draw_instruction_list.attachment_operations.size();
	ResourceTracker **trackers = command->trackers();
	RDD::AttachmentLoadOp *load_ops = command->load_ops();
	RDD::AttachmentStoreOp *store_ops = command->store_ops();
	for (uint32_t i = 0; i < command->trackers_count; i++) {
		ResourceTracker *resource_tracker = framebuffer_cache->trackers[i];
		if (resource_tracker != nullptr) {
			if (i < command->clear_values_count && i < attachment_op_count && draw_instruction_list.attachment_operations[i] == ATTACHMENT_OPERATION_CLEAR) {
				load_ops[i] = RDD::ATTACHMENT_LOAD_OP_CLEAR;
			} else if (i < attachment_op_count && draw_instruction_list.attachment_operations[i] == ATTACHMENT_OPERATION_IGNORE) {
				load_ops[i] = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
			} else if (resource_tracker->is_discardable) {
				bool resource_has_parent = resource_tracker->parent != nullptr;
				ResourceTracker *search_tracker = resource_has_parent ? resource_tracker->parent : resource_tracker;
				search_tracker->reset_if_outdated(tracking_frame);
				bool resource_was_modified_this_frame = search_tracker->write_command_or_list_index >= 0;
				load_ops[i] = resource_was_modified_this_frame ? RDD::ATTACHMENT_LOAD_OP_LOAD : RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
			} else {
				load_ops[i] = RDD::ATTACHMENT_LOAD_OP_LOAD;
			}

			store_ops[i] = resource_tracker->is_discardable ? RDD::ATTACHMENT_STORE_OP_DONT_CARE : RDD::ATTACHMENT_STORE_OP_STORE;
		} else {
			load_ops[i] = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
			store_ops[i] = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
		}

		trackers[i] = resource_tracker;
	}

	RDD::RenderPassClearValue *clear_values = command->clear_values();
	for (uint32_t i = 0; i < command->clear_values_count; i++) {
		clear_values[i] = draw_instruction_list.attachment_clear_values[i];
	}

	memcpy(command->instruction_data(), draw_instruction_list.data.ptr(), instruction_data_size);
	_add_command_to_graph(draw_instruction_list.command_trackers.ptr(), draw_instruction_list.command_tracker_usages.ptr(), draw_instruction_list.command_trackers.size(), command_index, command);
}

void RenderingDeviceGraph::add_texture_clear(RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, const Color &p_color, const RDD::TextureSubresourceRange &p_range) {
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	RecordedTextureClearCommand *command = static_cast<RecordedTextureClearCommand *>(_allocate_command(sizeof(RecordedTextureClearCommand), command_index));
	command->type = RecordedCommand::TYPE_TEXTURE_CLEAR;
	command->texture = p_dst;
	command->color = p_color;
	command->range = p_range;

	ResourceUsage usage;
	if (driver_clears_with_copy_engine) {
		command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
		usage = RESOURCE_USAGE_COPY_TO;
	} else {
		// If the driver is uncapable of using the copy engine for clearing the image (e.g. D3D12), we must either transition the
		// resource to a render target or a storage image as that's the only two ways it can perform the operation.
		if (p_dst_tracker->texture_usage & RDD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) {
			command->self_stages = RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			usage = RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE;
		} else {
			command->self_stages = RDD::PIPELINE_STAGE_CLEAR_STORAGE_BIT;
			usage = RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
		}
	}

	_add_command_to_graph(&p_dst_tracker, &usage, 1, command_index, command);
}

void RenderingDeviceGraph::add_texture_copy(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, VectorView<RDD::TextureCopyRegion> p_texture_copy_regions) {
	DEV_ASSERT(p_src_tracker != nullptr);
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	uint64_t command_size = sizeof(RecordedTextureCopyCommand) + p_texture_copy_regions.size() * sizeof(RDD::TextureCopyRegion);
	RecordedTextureCopyCommand *command = static_cast<RecordedTextureCopyCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_TEXTURE_COPY;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->from_texture = p_src;
	command->to_texture = p_dst;
	command->texture_copy_regions_count = p_texture_copy_regions.size();

	RDD::TextureCopyRegion *texture_copy_regions = command->texture_copy_regions();
	for (uint32_t i = 0; i < command->texture_copy_regions_count; i++) {
		texture_copy_regions[i] = p_texture_copy_regions[i];
	}

	ResourceTracker *trackers[2] = { p_dst_tracker, p_src_tracker };
	ResourceUsage usages[2] = { RESOURCE_USAGE_COPY_TO, RESOURCE_USAGE_COPY_FROM };
	_add_command_to_graph(trackers, usages, 2, command_index, command);
}

void RenderingDeviceGraph::add_texture_get_data(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::BufferID p_dst, VectorView<RDD::BufferTextureCopyRegion> p_buffer_texture_copy_regions, ResourceTracker *p_dst_tracker) {
	DEV_ASSERT(p_src_tracker != nullptr);

	int32_t command_index;
	uint64_t command_size = sizeof(RecordedTextureGetDataCommand) + p_buffer_texture_copy_regions.size() * sizeof(RDD::BufferTextureCopyRegion);
	RecordedTextureGetDataCommand *command = static_cast<RecordedTextureGetDataCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_TEXTURE_GET_DATA;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->from_texture = p_src;
	command->to_buffer = p_dst;
	command->buffer_texture_copy_regions_count = p_buffer_texture_copy_regions.size();

	RDD::BufferTextureCopyRegion *buffer_texture_copy_regions = command->buffer_texture_copy_regions();
	for (uint32_t i = 0; i < command->buffer_texture_copy_regions_count; i++) {
		buffer_texture_copy_regions[i] = p_buffer_texture_copy_regions[i];
	}

	if (p_dst_tracker != nullptr) {
		// Add the optional destination tracker if it was provided.
		ResourceTracker *trackers[2] = { p_dst_tracker, p_src_tracker };
		ResourceUsage usages[2] = { RESOURCE_USAGE_COPY_TO, RESOURCE_USAGE_COPY_FROM };
		_add_command_to_graph(trackers, usages, 2, command_index, command);
	} else {
		ResourceUsage usage = RESOURCE_USAGE_COPY_FROM;
		_add_command_to_graph(&p_src_tracker, &usage, 1, command_index, command);
	}
}

void RenderingDeviceGraph::add_texture_resolve(RDD::TextureID p_src, ResourceTracker *p_src_tracker, RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, uint32_t p_src_layer, uint32_t p_src_mipmap, uint32_t p_dst_layer, uint32_t p_dst_mipmap) {
	DEV_ASSERT(p_src_tracker != nullptr);
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	RecordedTextureResolveCommand *command = static_cast<RecordedTextureResolveCommand *>(_allocate_command(sizeof(RecordedTextureResolveCommand), command_index));
	command->type = RecordedCommand::TYPE_TEXTURE_RESOLVE;
	command->self_stages = RDD::PIPELINE_STAGE_RESOLVE_BIT;
	command->from_texture = p_src;
	command->to_texture = p_dst;
	command->src_layer = p_src_layer;
	command->src_mipmap = p_src_mipmap;
	command->dst_layer = p_dst_layer;
	command->dst_mipmap = p_dst_mipmap;

	ResourceTracker *trackers[2] = { p_dst_tracker, p_src_tracker };
	ResourceUsage usages[2] = { RESOURCE_USAGE_RESOLVE_TO, RESOURCE_USAGE_RESOLVE_FROM };
	_add_command_to_graph(trackers, usages, 2, command_index, command);
}

void RenderingDeviceGraph::add_texture_update(RDD::TextureID p_dst, ResourceTracker *p_dst_tracker, VectorView<RecordedBufferToTextureCopy> p_buffer_copies, VectorView<ResourceTracker *> p_buffer_trackers) {
	DEV_ASSERT(p_dst_tracker != nullptr);

	int32_t command_index;
	uint64_t command_size = sizeof(RecordedTextureUpdateCommand) + p_buffer_copies.size() * sizeof(RecordedBufferToTextureCopy);
	RecordedTextureUpdateCommand *command = static_cast<RecordedTextureUpdateCommand *>(_allocate_command(command_size, command_index));
	command->type = RecordedCommand::TYPE_TEXTURE_UPDATE;
	command->self_stages = RDD::PIPELINE_STAGE_COPY_BIT;
	command->to_texture = p_dst;
	command->buffer_to_texture_copies_count = p_buffer_copies.size();

	RecordedBufferToTextureCopy *buffer_to_texture_copies = command->buffer_to_texture_copies();
	for (uint32_t i = 0; i < command->buffer_to_texture_copies_count; i++) {
		buffer_to_texture_copies[i] = p_buffer_copies[i];
	}

	if (p_buffer_trackers.size() > 0) {
		// Add the optional buffer trackers if they were provided.
		thread_local LocalVector<ResourceTracker *> trackers;
		thread_local LocalVector<ResourceUsage> usages;
		trackers.clear();
		usages.clear();
		for (uint32_t i = 0; i < p_buffer_trackers.size(); i++) {
			trackers.push_back(p_buffer_trackers[i]);
			usages.push_back(RESOURCE_USAGE_COPY_FROM);
		}

		trackers.push_back(p_dst_tracker);
		usages.push_back(RESOURCE_USAGE_COPY_TO);

		_add_command_to_graph(trackers.ptr(), usages.ptr(), trackers.size(), command_index, command);
	} else {
		ResourceUsage usage = RESOURCE_USAGE_COPY_TO;
		_add_command_to_graph(&p_dst_tracker, &usage, 1, command_index, command);
	}
}

void RenderingDeviceGraph::add_capture_timestamp(RDD::QueryPoolID p_query_pool, uint32_t p_index) {
	int32_t command_index;
	RecordedCaptureTimestampCommand *command = static_cast<RecordedCaptureTimestampCommand *>(_allocate_command(sizeof(RecordedCaptureTimestampCommand), command_index));
	command->type = RecordedCommand::TYPE_CAPTURE_TIMESTAMP;
	command->self_stages = 0;
	command->pool = p_query_pool;
	command->index = p_index;
	_add_command_to_graph(nullptr, nullptr, 0, command_index, command);
}

void RenderingDeviceGraph::add_synchronization() {
	// Synchronization is only acknowledged if commands have been recorded on the graph already.
	if (command_count > 0) {
		command_synchronization_pending = true;
	}
}

void RenderingDeviceGraph::begin_label(const String &p_label_name, const Color &p_color) {
	uint32_t command_label_offset = command_label_chars.size();
	PackedByteArray command_label_utf8 = p_label_name.to_utf8_buffer();
	int command_label_utf8_size = command_label_utf8.size();
	command_label_chars.resize(command_label_offset + command_label_utf8_size + 1);
	memcpy(&command_label_chars[command_label_offset], command_label_utf8.ptr(), command_label_utf8.size());
	command_label_chars[command_label_offset + command_label_utf8_size] = '\0';
	command_label_colors.push_back(p_color);
	command_label_offsets.push_back(command_label_offset);
	command_label_index = command_label_count;
	command_label_count++;
}

void RenderingDeviceGraph::end_label() {
	command_label_index = -1;
}

void RenderingDeviceGraph::end(bool p_reorder_commands, bool p_full_barriers, RDD::CommandBufferID &r_command_buffer, CommandBufferPool &r_command_buffer_pool) {
	if (command_count == 0) {
		// No commands have been logged, do nothing.
		return;
	}

	thread_local LocalVector<RecordedCommandSort> commands_sorted;
	if (p_reorder_commands) {
		thread_local LocalVector<int64_t> command_stack;
		thread_local LocalVector<int32_t> sorted_command_indices;
		thread_local LocalVector<uint32_t> command_degrees;
		int32_t adjacency_list_index = 0;
		int32_t command_index;

		// Count all the incoming connections to every node by traversing their adjacency list.
		command_degrees.resize(command_count);
		memset(command_degrees.ptr(), 0, sizeof(uint32_t) * command_degrees.size());
		for (uint32_t i = 0; i < command_count; i++) {
			const RecordedCommand &recorded_command = *reinterpret_cast<const RecordedCommand *>(&command_data[command_data_offsets[i]]);
			adjacency_list_index = recorded_command.adjacent_command_list_index;
			while (adjacency_list_index >= 0) {
				const RecordedCommandListNode &command_list_node = command_list_nodes[adjacency_list_index];
				DEV_ASSERT((command_list_node.command_index != int32_t(i)) && "Command can't have itself as a dependency.");
				command_degrees[command_list_node.command_index] += 1;
				adjacency_list_index = command_list_node.next_list_index;
			}
		}

		// Push to the stack all nodes that have no incoming connections.
		command_stack.clear();
		for (uint32_t i = 0; i < command_count; i++) {
			if (command_degrees[i] == 0) {
				command_stack.push_back(i);
			}
		}

		sorted_command_indices.clear();
		while (!command_stack.is_empty()) {
			// Pop command from the stack.
			command_index = command_stack[command_stack.size() - 1];
			command_stack.resize(command_stack.size() - 1);

			// Add it to the sorted commands.
			sorted_command_indices.push_back(command_index);

			// Search for its adjacents and lower their degree for every visit. If the degree reaches zero, we push the command to the stack.
			const uint32_t command_data_offset = command_data_offsets[command_index];
			const RecordedCommand &recorded_command = *reinterpret_cast<const RecordedCommand *>(&command_data[command_data_offset]);
			adjacency_list_index = recorded_command.adjacent_command_list_index;
			while (adjacency_list_index >= 0) {
				const RecordedCommandListNode &command_list_node = command_list_nodes[adjacency_list_index];
				uint32_t &command_degree = command_degrees[command_list_node.command_index];
				DEV_ASSERT(command_degree > 0);
				command_degree--;
				if (command_degree == 0) {
					command_stack.push_back(command_list_node.command_index);
				}

				adjacency_list_index = command_list_node.next_list_index;
			}
		}

		// Batch buffer, texture, draw lists and compute operations together.
		const uint32_t PriorityTable[RecordedCommand::TYPE_MAX] = {
			0, // TYPE_NONE
			1, // TYPE_BUFFER_CLEAR
			1, // TYPE_BUFFER_COPY
			1, // TYPE_BUFFER_GET_DATA
			1, // TYPE_BUFFER_UPDATE
			4, // TYPE_COMPUTE_LIST
			3, // TYPE_DRAW_LIST
			2, // TYPE_TEXTURE_CLEAR
			2, // TYPE_TEXTURE_COPY
			2, // TYPE_TEXTURE_GET_DATA
			2, // TYPE_TEXTURE_RESOLVE
			2, // TYPE_TEXTURE_UPDATE
			2, // TYPE_INSERT_BREADCRUMB
		};

		commands_sorted.clear();
		commands_sorted.resize(command_count);

		for (uint32_t i = 0; i < command_count; i++) {
			const int32_t sorted_command_index = sorted_command_indices[i];
			const uint32_t command_data_offset = command_data_offsets[sorted_command_index];
			const RecordedCommand recorded_command = *reinterpret_cast<const RecordedCommand *>(&command_data[command_data_offset]);
			const uint32_t next_command_level = commands_sorted[sorted_command_index].level + 1;
			adjacency_list_index = recorded_command.adjacent_command_list_index;
			while (adjacency_list_index >= 0) {
				const RecordedCommandListNode &command_list_node = command_list_nodes[adjacency_list_index];
				uint32_t &adjacent_command_level = commands_sorted[command_list_node.command_index].level;
				if (adjacent_command_level < next_command_level) {
					adjacent_command_level = next_command_level;
				}

				adjacency_list_index = command_list_node.next_list_index;
			}

			commands_sorted[sorted_command_index].index = sorted_command_index;
			commands_sorted[sorted_command_index].priority = PriorityTable[recorded_command.type];
		}
	} else {
		commands_sorted.clear();
		commands_sorted.resize(command_count);

		for (uint32_t i = 0; i < command_count; i++) {
			commands_sorted[i].index = i;
		}
	}

	_wait_for_secondary_command_buffer_tasks();

	if (command_count > 0) {
		int32_t current_label_index = -1;
		int32_t current_label_level = -1;
		_run_label_command_change(r_command_buffer, -1, -1, true, true, nullptr, 0, current_label_index, current_label_level);

		if (device.workarounds.avoid_compute_after_draw) {
			// Reset the state of the workaround.
			workarounds_state.draw_list_found = false;
		}

		if (p_reorder_commands) {
#if PRINT_RENDER_GRAPH
			print_line("BEFORE SORT");
			_print_render_commands(commands_sorted.ptr(), command_count);
#endif

			commands_sorted.sort();

#if PRINT_RENDER_GRAPH
			print_line("AFTER SORT");
			_print_render_commands(commands_sorted.ptr(), command_count);
#endif

#if PRINT_COMMAND_RECORDING
			print_line(vformat("Recording %d commands", command_count));
#endif

			uint32_t boosted_priority = 0;
			uint32_t current_level = commands_sorted[0].level;
			uint32_t current_level_start = 0;
			for (uint32_t i = 0; i < command_count; i++) {
				if (current_level != commands_sorted[i].level) {
					RecordedCommandSort *level_command_ptr = &commands_sorted[current_level_start];
					uint32_t level_command_count = i - current_level_start;
					_boost_priority_for_render_commands(level_command_ptr, level_command_count, boosted_priority);
					_group_barriers_for_render_commands(r_command_buffer, level_command_ptr, level_command_count, p_full_barriers);
					_run_render_commands(current_level, level_command_ptr, level_command_count, r_command_buffer, r_command_buffer_pool, current_label_index, current_label_level);
					current_level = commands_sorted[i].level;
					current_level_start = i;
				}
			}

			RecordedCommandSort *level_command_ptr = &commands_sorted[current_level_start];
			uint32_t level_command_count = command_count - current_level_start;
			_boost_priority_for_render_commands(level_command_ptr, level_command_count, boosted_priority);
			_group_barriers_for_render_commands(r_command_buffer, level_command_ptr, level_command_count, p_full_barriers);
			_run_render_commands(current_level, level_command_ptr, level_command_count, r_command_buffer, r_command_buffer_pool, current_label_index, current_label_level);

#if PRINT_RENDER_GRAPH
			print_line("COMMANDS", command_count, "LEVELS", current_level + 1);
#endif
		} else {
			for (uint32_t i = 0; i < command_count; i++) {
				_group_barriers_for_render_commands(r_command_buffer, &commands_sorted[i], 1, p_full_barriers);
				_run_render_commands(i, &commands_sorted[i], 1, r_command_buffer, r_command_buffer_pool, current_label_index, current_label_level);
			}
		}

		_run_label_command_change(r_command_buffer, -1, -1, false, false, nullptr, 0, current_label_index, current_label_level);

#if PRINT_COMMAND_RECORDING
		print_line(vformat("Recorded %d commands", command_count));
#endif
	}

	// Advance the frame counter. It's not necessary to do this if no commands are recorded because that means no secondary command buffers were used.
	frame = (frame + 1) % frames.size();
}

#if PRINT_RESOURCE_TRACKER_TOTAL
static uint32_t resource_tracker_total = 0;
#endif

RenderingDeviceGraph::ResourceTracker *RenderingDeviceGraph::resource_tracker_create() {
#if PRINT_RESOURCE_TRACKER_TOTAL
	print_line("Resource trackers:", ++resource_tracker_total);
#endif
	return memnew(ResourceTracker);
}

void RenderingDeviceGraph::resource_tracker_free(ResourceTracker *p_tracker) {
	if (p_tracker == nullptr) {
		return;
	}

	if (p_tracker->in_parent_dirty_list) {
		// Delete the tracker from the parent's dirty linked list.
		if (p_tracker->parent->dirty_shared_list == p_tracker) {
			p_tracker->parent->dirty_shared_list = p_tracker->next_shared;
		} else {
			ResourceTracker *node = p_tracker->parent->dirty_shared_list;
			while (node != nullptr) {
				if (node->next_shared == p_tracker) {
					node->next_shared = p_tracker->next_shared;
					node = nullptr;
				} else {
					node = node->next_shared;
				}
			}
		}
	}

	memdelete(p_tracker);

#if PRINT_RESOURCE_TRACKER_TOTAL
	print_line("Resource trackers:", --resource_tracker_total);
#endif
}

RenderingDeviceGraph::FramebufferCache *RenderingDeviceGraph::framebuffer_cache_create() {
	return memnew(FramebufferCache);
}

void RenderingDeviceGraph::framebuffer_cache_free(RDD *p_driver, FramebufferCache *p_cache) {
	DEV_ASSERT(p_driver != nullptr);

	if (p_cache == nullptr) {
		return;
	}

	for (KeyValue<uint64_t, FramebufferStorage> &E : p_cache->storage_map) {
		p_driver->framebuffer_free(E.value.framebuffer);
		p_driver->render_pass_free(E.value.render_pass);
	}

	memdelete(p_cache);
}
