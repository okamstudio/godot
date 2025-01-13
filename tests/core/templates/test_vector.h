/**************************************************************************/
/*  test_vector.h                                                         */
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

#ifndef TEST_VECTOR_H
#define TEST_VECTOR_H

#include "core/templates/vector.h"

#include "tests/test_macros.h"

namespace TestVector {

TEST_CASE("[Vector] List initialization") {
	Vector<int> vector{ 0, 1, 2, 3, 4 };

	CHECK(vector.size() == 5);
	CHECK(vector[0] == 0);
	CHECK(vector[1] == 1);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);
	CHECK(vector[4] == 4);
}

TEST_CASE("[Vector] Push back and append") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	// Alias for `push_back`.
	vector.append(4);

	CHECK(vector[0] == 0);
	CHECK(vector[1] == 1);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);
	CHECK(vector[4] == 4);
}

TEST_CASE("[Vector] Append array") {
	Vector<int> vector;
	vector.push_back(1);
	vector.push_back(2);

	Vector<int> vector_other;
	vector_other.push_back(128);
	vector_other.push_back(129);
	vector.append_array(vector_other);

	CHECK(vector.size() == 4);
	CHECK(vector[0] == 1);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 128);
	CHECK(vector[3] == 129);
}

TEST_CASE("[Vector] Insert") {
	Vector<int> vector;
	vector.insert(0, 2);
	vector.insert(0, 8);
	vector.insert(2, 5);
	vector.insert(1, 5);
	vector.insert(0, -2);

	CHECK(vector.size() == 5);
	CHECK(vector[0] == -2);
	CHECK(vector[1] == 8);
	CHECK(vector[2] == 5);
	CHECK(vector[3] == 2);
	CHECK(vector[4] == 5);
}

TEST_CASE("[Vector] Ordered insert") {
	Vector<int> vector;
	vector.ordered_insert(2);
	vector.ordered_insert(8);
	vector.ordered_insert(5);
	vector.ordered_insert(5);
	vector.ordered_insert(-2);

	CHECK(vector.size() == 5);
	CHECK(vector[0] == -2);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 5);
	CHECK(vector[3] == 5);
	CHECK(vector[4] == 8);
}

TEST_CASE("[Vector] Insert + Ordered insert") {
	Vector<int> vector;
	vector.ordered_insert(2);
	vector.ordered_insert(8);
	vector.insert(0, 5);
	vector.ordered_insert(5);
	vector.insert(1, -2);

	CHECK(vector.size() == 5);
	CHECK(vector[0] == 5);
	CHECK(vector[1] == -2);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 5);
	CHECK(vector[4] == 8);
}

TEST_CASE("[Vector] Fill large array and modify it") {
	Vector<int> vector;
	vector.resize(1'000'000);
	vector.fill(0x60d07);

	vector.write[200] = 0;
	CHECK(vector.size() == 1'000'000);
	CHECK(vector[0] == 0x60d07);
	CHECK(vector[200] == 0);
	CHECK(vector[499'999] == 0x60d07);
	CHECK(vector[999'999] == 0x60d07);
	vector.remove_at(200);
	CHECK(vector[200] == 0x60d07);

	vector.clear();
	CHECK(vector.size() == 0);
}

TEST_CASE("[Vector] Copy creation") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	Vector<int> vector_other = Vector<int>(vector);
	vector_other.remove_at(0);
	CHECK(vector_other[0] == 1);
	CHECK(vector_other[1] == 2);
	CHECK(vector_other[2] == 3);
	CHECK(vector_other[3] == 4);

	// Make sure the original vector isn't modified.
	CHECK(vector[0] == 0);
	CHECK(vector[1] == 1);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);
	CHECK(vector[4] == 4);
}

TEST_CASE("[Vector] Duplicate") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	Vector<int> vector_other = vector.duplicate();
	vector_other.remove_at(0);
	CHECK(vector_other[0] == 1);
	CHECK(vector_other[1] == 2);
	CHECK(vector_other[2] == 3);
	CHECK(vector_other[3] == 4);

	// Make sure the original vector isn't modified.
	CHECK(vector[0] == 0);
	CHECK(vector[1] == 1);
	CHECK(vector[2] == 2);
	CHECK(vector[3] == 3);
	CHECK(vector[4] == 4);
}

TEST_CASE("[Vector] Get, set") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	CHECK(vector.get(0) == 0);
	CHECK(vector.get(1) == 1);
	vector.set(2, 256);
	CHECK(vector.get(2) == 256);
	CHECK(vector.get(3) == 3);

	ERR_PRINT_OFF;
	// Invalid (but should not crash): setting out of bounds.
	vector.set(6, 500);
	ERR_PRINT_ON;

	CHECK(vector.get(4) == 4);
}

TEST_CASE("[Vector] To byte array") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(-1);
	vector.push_back(2008);
	vector.push_back(999999999);

	Vector<uint8_t> byte_array = vector.to_byte_array();
	CHECK(byte_array.size() == 16);
	// vector[0]
	CHECK(byte_array[0] == 0);
	CHECK(byte_array[1] == 0);
	CHECK(byte_array[2] == 0);
	CHECK(byte_array[3] == 0);

	// vector[1]
	CHECK(byte_array[4] == 255);
	CHECK(byte_array[5] == 255);
	CHECK(byte_array[6] == 255);
	CHECK(byte_array[7] == 255);

	// vector[2]
	CHECK(byte_array[8] == 216);
	CHECK(byte_array[9] == 7);
	CHECK(byte_array[10] == 0);
	CHECK(byte_array[11] == 0);

	// vector[3]
	CHECK(byte_array[12] == 255);
	CHECK(byte_array[13] == 201);
	CHECK(byte_array[14] == 154);
	CHECK(byte_array[15] == 59);
}

TEST_CASE("[Vector] Slice") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	Vector<int> slice0 = vector.slice(0, 0);
	CHECK(slice0.size() == 0);

	Vector<int> slice1 = vector.slice(1, 3);
	CHECK(slice1.size() == 2);
	CHECK(slice1[0] == 1);
	CHECK(slice1[1] == 2);

	Vector<int> slice2 = vector.slice(1, -1);
	CHECK(slice2.size() == 3);
	CHECK(slice2[0] == 1);
	CHECK(slice2[1] == 2);
	CHECK(slice2[2] == 3);

	Vector<int> slice3 = vector.slice(3);
	CHECK(slice3.size() == 2);
	CHECK(slice3[0] == 3);
	CHECK(slice3[1] == 4);

	Vector<int> slice4 = vector.slice(2, -2);
	CHECK(slice4.size() == 1);
	CHECK(slice4[0] == 2);

	Vector<int> slice5 = vector.slice(-2);
	CHECK(slice5.size() == 2);
	CHECK(slice5[0] == 3);
	CHECK(slice5[1] == 4);

	Vector<int> slice6 = vector.slice(2, 42);
	CHECK(slice6.size() == 3);
	CHECK(slice6[0] == 2);
	CHECK(slice6[1] == 3);
	CHECK(slice6[2] == 4);

	ERR_PRINT_OFF;
	Vector<int> slice7 = vector.slice(5, 1);
	CHECK(slice7.size() == 0); // Expected to fail.
	ERR_PRINT_ON;
}

TEST_CASE("[Vector] Find, has") {
	Vector<int> vector;
	vector.push_back(3);
	vector.push_back(1);
	vector.push_back(4);
	vector.push_back(0);
	vector.push_back(2);

	CHECK(vector[0] == 3);
	CHECK(vector[1] == 1);
	CHECK(vector[2] == 4);
	CHECK(vector[3] == 0);
	CHECK(vector[4] == 2);

	CHECK(vector.find(0) == 3);
	CHECK(vector.find(1) == 1);
	CHECK(vector.find(2) == 4);
	CHECK(vector.find(3) == 0);
	CHECK(vector.find(4) == 2);

	CHECK(vector.find(-1) == -1);
	CHECK(vector.find(5) == -1);

	CHECK(vector.has(0));
	CHECK(vector.has(1));
	CHECK(vector.has(2));
	CHECK(vector.has(3));
	CHECK(vector.has(4));

	CHECK(!vector.has(-1));
	CHECK(!vector.has(5));
}

TEST_CASE("[Vector] Remove at") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	vector.remove_at(0);

	CHECK(vector[0] == 1);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 3);
	CHECK(vector[3] == 4);

	vector.remove_at(2);

	CHECK(vector[0] == 1);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 4);

	vector.remove_at(1);

	CHECK(vector[0] == 1);
	CHECK(vector[1] == 4);

	vector.remove_at(0);

	CHECK(vector[0] == 4);
}

TEST_CASE("[Vector] Remove at and find") {
	Vector<int> vector;
	vector.push_back(0);
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(4);

	CHECK(vector.size() == 5);

	vector.remove_at(0);

	CHECK(vector.size() == 4);

	CHECK(vector.find(0) == -1);
	CHECK(vector.find(1) != -1);
	CHECK(vector.find(2) != -1);
	CHECK(vector.find(3) != -1);
	CHECK(vector.find(4) != -1);

	vector.remove_at(vector.find(3));

	CHECK(vector.size() == 3);

	CHECK(vector.find(3) == -1);
	CHECK(vector.find(1) != -1);
	CHECK(vector.find(2) != -1);
	CHECK(vector.find(4) != -1);

	vector.remove_at(vector.find(2));

	CHECK(vector.size() == 2);

	CHECK(vector.find(2) == -1);
	CHECK(vector.find(1) != -1);
	CHECK(vector.find(4) != -1);

	vector.remove_at(vector.find(4));

	CHECK(vector.size() == 1);

	CHECK(vector.find(4) == -1);
	CHECK(vector.find(1) != -1);

	vector.remove_at(0);

	CHECK(vector.is_empty());
	CHECK(vector.size() == 0);
}

TEST_CASE("[Vector] Erase") {
	Vector<int> vector;
	vector.push_back(1);
	vector.push_back(3);
	vector.push_back(0);
	vector.push_back(2);
	vector.push_back(4);

	CHECK(vector.find(2) == 3);

	vector.erase(2);

	CHECK(vector.find(2) == -1);
	CHECK(vector.size() == 4);
}

TEST_CASE("[Vector] Size, resize, reserve") {
	Vector<int> vector;
	CHECK(vector.is_empty());
	CHECK(vector.size() == 0);

	vector.resize(10);

	CHECK(vector.size() == 10);

	vector.resize(5);

	CHECK(vector.size() == 5);

	vector.remove_at(0);
	vector.remove_at(0);
	vector.remove_at(0);

	CHECK(vector.size() == 2);

	vector.clear();

	CHECK(vector.size() == 0);
	CHECK(vector.is_empty());

	vector.push_back(0);
	vector.push_back(0);
	vector.push_back(0);

	CHECK(vector.size() == 3);

	vector.push_back(0);

	CHECK(vector.size() == 4);
}

TEST_CASE("[Vector] Sort") {
	Vector<int> vector;
	vector.push_back(2);
	vector.push_back(8);
	vector.push_back(-4);
	vector.push_back(5);
	vector.sort();

	CHECK(vector.size() == 4);
	CHECK(vector[0] == -4);
	CHECK(vector[1] == 2);
	CHECK(vector[2] == 5);
	CHECK(vector[3] == 8);
}

TEST_CASE("[Vector] Sort custom") {
	Vector<String> vector;
	vector.push_back("world");
	vector.push_back("World");
	vector.push_back("Hello");
	vector.push_back("10Hello");
	vector.push_back("12Hello");
	vector.push_back("01Hello");
	vector.push_back("1Hello");
	vector.push_back(".Hello");
	vector.sort_custom<NaturalNoCaseComparator>();

	CHECK(vector.size() == 8);
	CHECK(vector[0] == ".Hello");
	CHECK(vector[1] == "01Hello");
	CHECK(vector[2] == "1Hello");
	CHECK(vector[3] == "10Hello");
	CHECK(vector[4] == "12Hello");
	CHECK(vector[5] == "Hello");
	CHECK(vector[6] == "world");
	CHECK(vector[7] == "World");
}

TEST_CASE("[Vector] Search") {
	Vector<int> vector;
	vector.push_back(1);
	vector.push_back(2);
	vector.push_back(3);
	vector.push_back(5);
	vector.push_back(8);
	CHECK(vector.bsearch(2, true) == 1);
	CHECK(vector.bsearch(2, false) == 2);
	CHECK(vector.bsearch(5, true) == 3);
	CHECK(vector.bsearch(5, false) == 4);
}

TEST_CASE("[Vector] Operators") {
	Vector<int> vector;
	vector.push_back(2);
	vector.push_back(8);
	vector.push_back(-4);
	vector.push_back(5);

	Vector<int> vector_other;
	vector_other.push_back(2);
	vector_other.push_back(8);
	vector_other.push_back(-4);
	vector_other.push_back(5);

	CHECK(vector == vector_other);

	vector_other.push_back(10);
	CHECK(vector != vector_other);
}

struct CyclicVectorHolder {
	Vector<CyclicVectorHolder> *vector = nullptr;
	bool is_destructing = false;

	~CyclicVectorHolder() {
		if (is_destructing) {
			// The vector must exist and not expose its backing array at this point.
			CHECK_NE(vector, nullptr);
			CHECK_EQ(vector->ptr(), nullptr);
		}
	}
};

TEST_CASE("[Vector] Cyclic Reference") {
	// Create a stack-space vector.
	Vector<CyclicVectorHolder> vector;
	// Add a new (empty) element.
	vector.resize(1);
	// Expose the vector to its element through CyclicVectorHolder.
	// This is questionable behavior, but should still behave graciously.
	vector.ptrw()[0] = CyclicVectorHolder{ &vector };
	vector.ptrw()[0].is_destructing = true;
	// The vector goes out of scope and destructs, calling CyclicVectorHolder's destructor.
}

} // namespace TestVector

#endif // TEST_VECTOR_H
