/**************************************************************************/
/*  test_heap.h                                                           */
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

#ifndef TEST_HEAP_H
#define TEST_HEAP_H

#include "core/templates/heap.h"

#include "tests/test_macros.h"

namespace TestHeap {
struct GreaterThan {
	bool operator()(int p_a, int p_b) const { return p_a > p_b; }
};

struct CompareArrayValues {
	const int *array;

	CompareArrayValues(const int *p_array) :
			array(p_array) {}

	bool operator()(uint32_t p_index_a, uint32_t p_index_b) const {
		return array[p_index_a] < array[p_index_b];
	}
};

struct RegisterHeapIndexes {
	uint32_t *indexes;

	RegisterHeapIndexes(uint32_t *p_indexes) :
			indexes(p_indexes) {}

	void operator()(uint32_t p_vector_index, uint32_t p_heap_index) {
		indexes[p_vector_index] = p_heap_index;
	}
};

TEST_CASE("[Heap] size") {
	Heap<int> heap;

	CHECK(heap.size() == 0);

	heap.push(0);
	CHECK(heap.size() == 1);

	heap.push(1);
	CHECK(heap.size() == 2);

	heap.pop();
	CHECK(heap.size() == 1);

	heap.pop();
	CHECK(heap.size() == 0);
}

TEST_CASE("[Heap] is_empty") {
	Heap<int> heap;

	CHECK(heap.is_empty() == true);

	heap.push(0);
	CHECK(heap.is_empty() == false);

	heap.pop();
	CHECK(heap.is_empty() == true);
}

TEST_CASE("[Heap] push/pop") {
	SUBCASE("Default comparator") {
		Heap<int> heap;

		heap.push(2);
		heap.push(7);
		heap.push(5);
		heap.push(3);
		heap.push(4);

		CHECK(heap.pop() == 7);
		CHECK(heap.pop() == 5);
		CHECK(heap.pop() == 4);
		CHECK(heap.pop() == 3);
		CHECK(heap.pop() == 2);
	}

	SUBCASE("Custom comparator") {
		GreaterThan greaterThan;
		Heap<int, GreaterThan> heap(greaterThan);

		heap.push(2);
		heap.push(7);
		heap.push(5);
		heap.push(3);
		heap.push(4);

		CHECK(heap.pop() == 2);
		CHECK(heap.pop() == 3);
		CHECK(heap.pop() == 4);
		CHECK(heap.pop() == 5);
		CHECK(heap.pop() == 7);
	}

	SUBCASE("Intermediate pops") {
		Heap<int> heap;

		heap.push(0);
		heap.push(3);
		heap.pop();
		heap.push(1);
		heap.push(2);

		CHECK(heap.pop() == 2);
		CHECK(heap.pop() == 1);
		CHECK(heap.pop() == 0);
	}
}

TEST_CASE("[Heap] shift") {
	int values[] = { 5, 3, 6, 7, 1 };
	uint32_t heap_indexes[] = { 0, 0, 0, 0, 0 };
	CompareArrayValues comparator(values);
	RegisterHeapIndexes indexer(heap_indexes);
	Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes> heap(comparator, indexer);

	heap.push(0);
	heap.push(1);
	heap.push(2);
	heap.push(3);
	heap.push(4);

	// Shift down: 6 -> 2
	values[2] = 2;
	heap.shift(heap_indexes[2]);

	// Shift up: 5 -> 8
	values[0] = 8;
	heap.shift(heap_indexes[0]);

	CHECK(heap.pop() == 0);
	CHECK(heap.pop() == 3);
	CHECK(heap.pop() == 1);
	CHECK(heap.pop() == 2);
	CHECK(heap.pop() == 4);

	CHECK(heap_indexes[0] == Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[1] == Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[2] == Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[3] == Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[4] == Heap<uint32_t, CompareArrayValues, RegisterHeapIndexes>::INVALID_INDEX);
}

TEST_CASE("[Heap] clear") {
	uint32_t heap_indexes[] = { 0, 0, 0, 0 };
	RegisterHeapIndexes indexer(heap_indexes);
	Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes> heap(indexer);

	heap.push(0);
	heap.push(2);
	heap.push(1);
	heap.push(3);

	heap.clear();

	CHECK(heap.size() == 0);

	CHECK(heap_indexes[0] == Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[1] == Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[2] == Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes>::INVALID_INDEX);
	CHECK(heap_indexes[3] == Heap<uint32_t, Comparator<uint32_t>, RegisterHeapIndexes>::INVALID_INDEX);
}
} //namespace TestHeap

#endif // TEST_HEAP_H
