/**************************************************************************/
/*  hashes.cpp                                                            */
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

#include "hashes.h"

uint32_t Hashes::insert_hash(uint32_t p_hash, uint32_t p_capacity) {
	uint32_t pos = p_hash & p_capacity;
	uint8_t stored_hash = _get_stored_hash(p_hash);

	if (ptr[pos] <= DELETED_HASH) {
		ptr[pos] = stored_hash;
		return pos;
	}
	pos = (pos + 1) & p_capacity;

#ifdef SIMD_AVAILABLE
	while (true) {
		HashGroup group(&ptr[pos]);
		int mask = group.get_compare_mask(EMPTY_HASH) | group.get_compare_mask(DELETED_HASH);
		if (likely(mask)) {
			int bit_pos = count_trailing_zeros(mask);
			uint32_t actual_pos = pos + bit_pos;
			ptr[actual_pos] = stored_hash;
			return actual_pos;
		}
		pos += HashGroup::GROUP_SIZE;
		if (pos > p_capacity) {
			pos = 0;
		}
	}
#else
	while (true) {
		if (ptr[pos] <= DELETED_HASH) {
			ptr[pos] = stored_hash;

			return pos;
		}
		pos = (pos + 1) & p_capacity;
	}
#endif
}
