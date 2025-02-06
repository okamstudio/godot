/**************************************************************************/
/*  typed_static_block_allocator.h                                        */
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

#ifndef TYPED_STATIC_BLOCK_ALLOCATOR_H
#define TYPED_STATIC_BLOCK_ALLOCATOR_H

#include "core/os/memory.h"
#include "core/os/static_block_allocator.h"

template <typename T>
class TypedStaticBlockAllocator {
	int64_t allocator_id = -1;

public:
	template <typename... Args>
	T *new_allocation(const Args &&...p_args) {
		if (unlikely(allocator_id == -1)) {
			allocator_id = StaticBlockAllocator::get_allocator_id_for_size(sizeof(T));
		}
		T *ret = static_cast<T *>(StaticBlockAllocator::allocate_by_id(allocator_id));
		memnew_placement(ret, T(p_args...));
		return ret;
	}
	void delete_allocation(T *p_allocation) {
		if (!predelete_handler(p_allocation)) {
			return; // doesn't want to be deleted
		}
		if constexpr (!std::is_trivially_destructible_v<T>) {
			p_allocation->~T();
		}
		StaticBlockAllocator::free_by_id(allocator_id, p_allocation);
	}
};

#endif // TYPED_STATIC_BLOCK_ALLOCATOR_H
