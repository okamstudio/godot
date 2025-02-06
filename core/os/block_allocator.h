/**************************************************************************/
/*  block_allocator.h                                                     */
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

#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include "core/typedefs.h"

/**
 * Expands exponentially. Deleted elements are written into `free_list` in place of freed memory.
 *
 * [ 1     2        3         4  ]
 *   |     |        |         |
 *   1    2 3    4 5 6 7
 * | * || * * || * * * * | | ... |
 *
 * See more here: https://github.com/godotengine/godot/pull/97016.
 */
class BlockAllocator {
	struct FreeListElement {
		FreeListElement *next = nullptr;
	};

	size_t cur_block_size = 1;
	uint8_t *current_pointer = nullptr;
	uint8_t **blocks = nullptr;

	FreeListElement *free_list = nullptr;
	size_t total_elements = 0;
	uint32_t structure_size = 0;
	uint32_t blocks_count = 0;
	void allocate_new_block(size_t p_block_size);
	static uint32_t get_blocks_capacity(uint32_t p_blocks_size);

public:
	_FORCE_INLINE_ bool is_initialized() { return blocks != nullptr; }
	size_t get_total_elements();
	uint32_t get_structure_size();
	uint32_t get_total_blocks();

	void init(uint32_t p_structure_size, uint32_t p_start_size);
	void *alloc();
	void free(void *p_ptr);
	void reset();
	~BlockAllocator();
};

#endif // BLOCK_ALLOCATOR_H
