/**************************************************************************/
/*  test_paged_array.h                                                    */
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

#pragma once

#include "core/templates/paged_array.h"

#include "thirdparty/doctest/doctest.h"

namespace TestPagedArray {

// PagedArray

TEST_CASE("[PagedArray] Simple fill and refill") {
	PagedArrayPool<uint32_t> pool;
	PagedArray<uint32_t> array;
	array.set_page_pool(&pool);

	for (uint32_t i = 0; i < 123456; i++) {
		array.push_back(i);
	}
	CHECK_MESSAGE(
			array.size() == 123456,
			"PagedArray should have 123456 elements.");

	bool all_match = true;
	for (uint32_t i = 0; i < 123456; i++) {
		if (array[i] != i) {
			all_match = false;
			break;
		}
	}

	CHECK_MESSAGE(
			all_match,
			"PagedArray elements should match from 0 to 123455.");

	array.clear();

	CHECK_MESSAGE(
			array.size() == 0,
			"PagedArray elements should be 0 after clear.");

	for (uint32_t i = 0; i < 999; i++) {
		array.push_back(i);
	}
	CHECK_MESSAGE(
			array.size() == 999,
			"PagedArray should have 999 elements.");

	all_match = true;
	for (uint32_t i = 0; i < 999; i++) {
		if (array[i] != i) {
			all_match = false;
		}
	}

	CHECK_MESSAGE(
			all_match,
			"PagedArray elements should match from 0 to 998.");

	array.reset(); //reset so pagepool can be reset
	pool.reset();
}

TEST_CASE("[PagedArray] Shared pool fill, including merging") {
	PagedArrayPool<uint32_t> pool;
	PagedArray<uint32_t> array1;
	PagedArray<uint32_t> array2;
	array1.set_page_pool(&pool);
	array2.set_page_pool(&pool);

	for (uint32_t i = 0; i < 123456; i++) {
		array1.push_back(i);
	}
	CHECK_MESSAGE(
			array1.size() == 123456,
			"PagedArray #1 should have 123456 elements.");

	bool all_match = true;
	for (uint32_t i = 0; i < 123456; i++) {
		if (array1[i] != i) {
			all_match = false;
		}
	}

	CHECK_MESSAGE(
			all_match,
			"PagedArray #1 elements should match from 0 to 123455.");

	for (uint32_t i = 0; i < 999; i++) {
		array2.push_back(i);
	}
	CHECK_MESSAGE(
			array2.size() == 999,
			"PagedArray #2 should have 999 elements.");

	all_match = true;
	for (uint32_t i = 0; i < 999; i++) {
		if (array2[i] != i) {
			all_match = false;
		}
	}

	CHECK_MESSAGE(
			all_match,
			"PagedArray #2 elements should match from 0 to 998.");

	array1.merge_unordered(array2);

	CHECK_MESSAGE(
			array1.size() == 123456 + 999,
			"PagedArray #1 should now be 123456 + 999 elements.");

	CHECK_MESSAGE(
			array2.size() == 0,
			"PagedArray #2 should now be 0 elements.");

	array1.reset(); //reset so pagepool can be reset
	array2.reset(); //reset so pagepool can be reset
	pool.reset();
}

TEST_CASE("[PagedArray] Extensive merge_unordered() test") {
	for (int page_size = 1; page_size <= 128; page_size *= 2) {
		PagedArrayPool<uint32_t> pool(page_size);
		PagedArray<uint32_t> array1;
		PagedArray<uint32_t> array2;
		array1.set_page_pool(&pool);
		array2.set_page_pool(&pool);

		const int max_count = 123;
		// Test merging arrays of lengths 0+123, 1+122, 2+121, ..., 123+0
		for (uint32_t j = 0; j < max_count; j++) {
			CHECK(array1.size() == 0);
			CHECK(array2.size() == 0);

			uint32_t sum = 12345;
			for (uint32_t i = 0; i < j; i++) {
				// Hashing the addend makes it extremely unlikely for any values
				// other than the original inputs to produce a matching sum
				uint32_t addend = hash_murmur3_one_32(i) + i;
				array1.push_back(addend);
				sum += addend;
			}
			for (uint32_t i = j; i < max_count; i++) {
				// See above
				uint32_t addend = hash_murmur3_one_32(i) + i;
				array2.push_back(addend);
				sum += addend;
			}

			CHECK(array1.size() == j);
			CHECK(array2.size() == max_count - j);

			array1.merge_unordered(array2);
			CHECK_MESSAGE(array1.size() == max_count, "merge_unordered() added/dropped elements while merging");

			// If any elements were altered during merging, the sum will not match up.
			for (uint32_t i = 0; i < array1.size(); i++) {
				sum -= array1[i];
			}
			CHECK_MESSAGE(sum == 12345, "merge_unordered() altered elements while merging");

			array1.clear();
		}

		array1.reset();
		array2.reset();
		pool.reset();
	}
}

} // namespace TestPagedArray
