/**************************************************************************/
/*  block_allocator.cpp                                                   */
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

#include "block_allocator.h"

#include "core/os/memory.h"

uint32_t BlockAllocator::get_total_blocks() {
	return blocks_count;
}

void BlockAllocator::init(uint32_t p_structure_size, uint32_t p_start_size) {
	ERR_FAIL_COND_MSG(is_initialized(), "Allocator is in use!!!");
	if (p_structure_size < sizeof(FreeListElement)) {
		structure_size = sizeof(FreeListElement);
	} else {
		structure_size = p_structure_size;
	}
	cur_block_size = p_start_size;
	allocate_new_block(p_start_size);
}

void *BlockAllocator::alloc() {
	void *pointer = 0;
	uint8_t *block = blocks[blocks_count - 1];
	size_t left = current_pointer - block;
	total_elements++;
	if (free_list != nullptr) {
		pointer = free_list;
		free_list = free_list->next;
		return pointer;
	}

	if (unlikely(left >= cur_block_size * (size_t)structure_size)) {
		cur_block_size = cur_block_size > 1 ? cur_block_size * 1.5f : 2;
		allocate_new_block(cur_block_size);
	}
	pointer = current_pointer;
	current_pointer += structure_size;

	return pointer;
}

uint32_t BlockAllocator::get_blocks_capacity(uint32_t p_blocks_size) {
	return next_power_of_2(p_blocks_size);
}

void BlockAllocator::allocate_new_block(size_t p_block_size) {
	uint32_t blocks_capacity = get_blocks_capacity(blocks_count);
	blocks_count++;
	if (blocks_count > blocks_capacity) {
		blocks = reinterpret_cast<uint8_t **>(memrealloc(blocks, get_blocks_capacity(blocks_count) * sizeof(uint8_t *)));
	}
	blocks[blocks_count - 1] = reinterpret_cast<uint8_t *>(memalloc(cur_block_size * (size_t)structure_size));
	current_pointer = blocks[blocks_count - 1];
}

size_t BlockAllocator::get_total_elements() {
	return total_elements;
}

uint32_t BlockAllocator::get_structure_size() {
	return structure_size;
}

void BlockAllocator::free(void *p_ptr) {
	FreeListElement *new_head = (FreeListElement *)p_ptr;
	new_head->next = free_list;
	free_list = new_head;
	total_elements--;
}

void BlockAllocator::reset() {
	if (blocks == nullptr) {
		return;
	}
	for (uint32_t i = 0; i < blocks_count; i++) {
		memfree(blocks[i]);
	}
	memfree(blocks);
	blocks_count = 0;
	blocks = nullptr;
	free_list = nullptr;
}

BlockAllocator::~BlockAllocator() {
	reset();
}
