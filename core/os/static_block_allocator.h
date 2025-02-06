/**************************************************************************/
/*  static_block_allocator.h                                              */
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

#ifndef STATIC_BLOCK_ALLOCATOR_H
#define STATIC_BLOCK_ALLOCATOR_H

#include "core/typedefs.h"

class StaticBlockAllocator {
	static int64_t _get_allocator_id_for_size(size_t p_size);
	static int32_t _get_thread_id();
	static int64_t _create_id(int32_t p_size_pos, int32_t p_thread_id);
	static void _get_from_id(int64_t p_id, int32_t &r_size_pos, int32_t &r_thread_id);
	static void _init();

public:
	static int64_t get_allocator_id_for_size(size_t p_size);
	static void *allocate_by_id(int64_t p_id);
	static void free_by_id(int64_t p_id, void *p_ptr);
};

#endif // STATIC_BLOCK_ALLOCATOR_H
