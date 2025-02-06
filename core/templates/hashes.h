/**************************************************************************/
/*  hashes.h                                                              */
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

#ifndef HASHES_H
#define HASHES_H

#include "core/simd/simd.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/index_array.h"
#include "core/typedefs.h"

struct HashGroup {
#ifdef SIMD_AVAILABLE
	static constexpr uint32_t GROUP_SIZE = 16;

private:
#ifdef SIMD_SSE2
	__m128i vector;
#elif defined(SIMD_NEON)
	uint8x16_t vector;
#endif

public:
	_FORCE_INLINE_ HashGroup(const uint8_t *p_begin) {
#ifdef SIMD_SSE2
		vector = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p_begin));
#elif defined(SIMD_NEON)
		vector = vld1q_s8(reinterpret_cast<const int8_t *>(p_begin));
#endif
	}

	_FORCE_INLINE_ int get_compare_mask(uint8_t p_value) {
#ifdef SIMD_SSE2
		__m128i target_vector = _mm_set1_epi8(p_value);
		__m128i cmp = _mm_cmpeq_epi8(target_vector, vector);
		return _mm_movemask_epi8(cmp);
#elif defined(SIMD_NEON)
		uint8x16_t target_vector = vdupq_n_u8(p_value);
		uint8x16_t cmp = vceqq_u8(target_vector, vector);
		alignas(16) const uint8_t _powers[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };

		uint8x16_t powers = vld1q_u8(_powers);
		uint64x2_t mask = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vandq_u8(cmp, powers))));
		uint16_t output;
		vst1q_lane_u8((uint8_t *)&output + 0, (uint8x16_t)mask, 0);
		vst1q_lane_u8((uint8_t *)&output + 1, (uint8x16_t)mask, 8);
		return static_cast<int>(output);
#endif
	}
#else
	static constexpr uint32_t GROUP_SIZE = 1;
#endif // SIMD_AVAILABLE
};

struct Hashes {
private:
	static _FORCE_INLINE_ uint8_t _get_stored_hash(uint32_t p_hash) {
		uint8_t stored_hash = (p_hash >> 24) & HASH_MASK;
		if (unlikely(stored_hash <= END_HASH)) {
			stored_hash += END_HASH + 1;
		}
		return stored_hash;
	}

public:
	static constexpr uint32_t EMPTY_HASH = 0;
	static constexpr uint32_t DELETED_HASH = 1;
	static constexpr uint32_t END_HASH = 2;

	static_assert(EMPTY_HASH == 0 && DELETED_HASH == 1 && END_HASH == 2);

	static constexpr uint32_t HASH_MASK = (1 << (8 * sizeof(uint8_t))) - 1;
	uint8_t *ptr = nullptr;

	template <typename Container, typename TKey>
	inline bool lookup_pos_with_hash(const Container *p_container, const TKey &p_key, uint32_t p_hash, uint32_t p_capacity, uint32_t &r_hash_pos) const {
		uint32_t pos = p_hash & p_capacity;

		if (ptr[pos] == EMPTY_HASH) {
			return false;
		}

		uint8_t stored_hash = _get_stored_hash(p_hash);

		if (ptr[pos] == stored_hash && likely(p_container->_compare_function(pos, p_key))) {
			r_hash_pos = pos;
			return true;
		}
		pos = (pos + 1) & p_capacity;
		uint32_t distance = 1;
#ifdef SIMD_AVAILABLE
		while (true) { // This will compare 16 hahses at once.
			HashGroup group(&ptr[pos]);
			int mask = group.get_compare_mask(stored_hash);
			while (mask) {
				int bit_pos = count_trailing_zeros(mask);
				uint32_t actual_pos = pos + bit_pos;
				if (likely(p_container->_compare_function(actual_pos, p_key))) {
					r_hash_pos = actual_pos;
					return true;
				}

				mask &= mask - 1;
			}
			mask = group.get_compare_mask(EMPTY_HASH);

			if (likely(mask)) {
				return false;
			}

			pos += HashGroup::GROUP_SIZE;
			if (pos > p_capacity) {
				distance += p_capacity + HashGroup::GROUP_SIZE + 1 - pos;
				if (unlikely(distance > p_capacity)) {
					return false;
				}
				pos = 0;
			} else {
				distance += HashGroup::GROUP_SIZE;
			}
		}
#else
		while (true) {
			if (ptr[pos] == stored_hash && likely(p_container->_compare_function(pos, p_key))) {
				r_hash_pos = pos;
				return true;
			}

			if (ptr[pos] == EMPTY_HASH) {
				return false;
			}

			pos = (pos + 1) & p_capacity;
			distance++;
			if (unlikely(distance > p_capacity)) {
				return false;
			}
		}
#endif
	}

	uint32_t insert_hash(uint32_t p_hash, uint32_t p_capacity);

	_FORCE_INLINE_ const uint8_t &operator[](uint32_t p_index) const {
		return ptr[p_index];
	}

	_FORCE_INLINE_ void delete_hash(uint32_t p_index) {
		ptr[p_index] = DELETED_HASH;
	}
};

#endif // HASHES_H
