/**************************************************************************/
/*  test_oa_hash_map.h                                                    */
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

#ifndef TEST_OA_HASH_MAP_H
#define TEST_OA_HASH_MAP_H

#include "core/templates/oa_hash_map.h"
#include "scene/resources/texture.h"

#include "tests/test_macros.h"

namespace TestOAHashMap {

TEST_CASE("[OAHashMap] List initialization") {
	OAHashMap<int, String> map{ { 0, "A" }, { 1, "B" }, { 2, "C" }, { 3, "D" }, { 4, "E" } };

	CHECK(map.get_num_elements() == 5);
	String value;
	CHECK(map.lookup(0, value));
	CHECK(value == "A");
	CHECK(map.lookup(1, value));
	CHECK(value == "B");
	CHECK(map.lookup(2, value));
	CHECK(value == "C");
	CHECK(map.lookup(3, value));
	CHECK(value == "D");
	CHECK(map.lookup(4, value));
	CHECK(value == "E");
}

TEST_CASE("[OAHashMap] List initialization with existing elements") {
	OAHashMap<int, String> map{ { 0, "A" }, { 0, "B" }, { 0, "C" }, { 0, "D" }, { 0, "E" } };

	CHECK(map.get_num_elements() == 1);
	String value;
	CHECK(map.lookup(0, value));
	CHECK(value == "E");
}

TEST_CASE("[OAHashMap] Insert element") {
	OAHashMap<int, int> map;
	map.insert(42, 84);
	int data = 0;
	bool lookup_res = map.lookup(42, data);
	int value = *map.lookup_ptr(42);
	CHECK(lookup_res);
	CHECK(value == 84);
	CHECK(data == 84);
}

TEST_CASE("[OAHashMap] Set element") {
	OAHashMap<int, int> map;
	map.set(42, 84);
	int data = 0;
	bool lookup_res = map.lookup(42, data);
	int value = *map.lookup_ptr(42);
	CHECK(lookup_res);
	CHECK(value == 84);
	CHECK(data == 84);
}

TEST_CASE("[OAHashMap] Overwrite element") {
	OAHashMap<int, int> map;
	map.set(42, 84);
	map.set(42, 1234);
	int result = *map.lookup_ptr(42);
	CHECK(result == 1234);
}

TEST_CASE("[OAHashMap] Remove element") {
	OAHashMap<int, int> map;
	map.insert(42, 84);
	map.remove(42);
	CHECK(!map.has(42));
}

TEST_CASE("[OAHashMap] Get Num_Elements") {
	OAHashMap<int, int> map;
	map.set(42, 84);
	map.set(123, 84);
	map.set(123, 84);
	map.set(0, 84);
	map.set(123485, 84);

	CHECK(map.get_num_elements() == 4);
}

TEST_CASE("[OAHashMap] Iteration") {
	OAHashMap<int, int> map;
	map.insert(42, 84);
	map.insert(123, 12385);
	map.insert(0, 12934);
	map.insert(123485, 1238888);
	map.set(123, 111111);

	Vector<Pair<int, int>> expected;
	expected.push_back(Pair<int, int>(42, 84));
	expected.push_back(Pair<int, int>(123, 111111));
	expected.push_back(Pair<int, int>(0, 12934));
	expected.push_back(Pair<int, int>(123485, 1238888));

	for (OAHashMap<int, int>::Iterator it = map.iter(); it.valid; it = map.next_iter(it)) {
		int64_t result = expected.find(Pair<int, int>(*it.key, *it.value));
		CHECK(result >= 0);
	}
}

TEST_CASE("[OAHashMap] Insert, iterate, remove many strings") {
	uint64_t pre_mem = Memory::get_mem_usage();
	{
		const int elem_max = 40;
		OAHashMap<String, int> map;
		for (int i = 0; i < elem_max; i++) {
			map.insert(itos(i), i);
		}

		Vector<String> elems_still_valid;

		for (int i = 0; i < elem_max; i++) {
			if ((i % 5) == 0) {
				map.remove(itos(i));
			} else {
				elems_still_valid.push_back(itos(i));
			}
		}

		CHECK(elems_still_valid.size() == map.get_num_elements());

		for (int i = 0; i < elems_still_valid.size(); i++) {
			CHECK(map.has(elems_still_valid[i]));
		}
	}

	CHECK(Memory::get_mem_usage() == pre_mem);
}

TEST_CASE("[OAHashMap] Clear") {
	OAHashMap<int, int> map;
	map.insert(42, 84);
	map.insert(0, 1234);
	map.clear();
	CHECK(!map.has(42));
	CHECK(!map.has(0));
	CHECK(map.is_empty());
}

TEST_CASE("[OAHashMap] Copy constructor") {
	uint64_t pre_mem = Memory::get_mem_usage();
	{
		OAHashMap<int, int> map0;
		const uint32_t count = 5;
		for (uint32_t i = 0; i < count; i++) {
			map0.insert(i, i);
		}
		OAHashMap<int, int> map1(map0);
		CHECK(map0.get_num_elements() == map1.get_num_elements());
		CHECK(map0.get_capacity() == map1.get_capacity());
		CHECK(*map0.lookup_ptr(0) == *map1.lookup_ptr(0));
	}
	CHECK(Memory::get_mem_usage() == pre_mem);
}

TEST_CASE("[OAHashMap] Operator =") {
	uint64_t pre_mem = Memory::get_mem_usage();
	{
		OAHashMap<int, int> map0;
		OAHashMap<int, int> map1;
		const uint32_t count = 5;
		map1.insert(1234, 1234);
		for (uint32_t i = 0; i < count; i++) {
			map0.insert(i, i);
		}
		map1 = map0;
		CHECK(map0.get_num_elements() == map1.get_num_elements());
		CHECK(map0.get_capacity() == map1.get_capacity());
		CHECK(*map0.lookup_ptr(0) == *map1.lookup_ptr(0));
	}
	CHECK(Memory::get_mem_usage() == pre_mem);
}

TEST_CASE("[OAHashMap] Non-trivial types") {
	uint64_t pre_mem = Memory::get_mem_usage();
	{
		OAHashMap<String, Ref<Texture2D>> map1;
		const uint32_t count = 10;
		for (uint32_t i = 0; i < count; i++) {
			String string = "qwerty";
			string += itos(i);
			Ref<Texture2D> ref_texture_2d;

			map1.set(string, ref_texture_2d);
			Ref<Texture2D> map_vec = *map1.lookup_ptr(string);
			CHECK(map_vec == ref_texture_2d);
		}
		OAHashMap<String, Ref<Texture2D>> map1copy(map1);
		CHECK(map1copy.has(String("qwerty0")));
		map1copy = map1;
		CHECK(map1copy.has(String("qwerty2")));

		OAHashMap<int64_t, Vector4 *> map2;

		for (uint32_t i = 0; i < count; i++) {
			Vector4 *vec = memnew(Vector4);
			vec->x = 10;
			vec->y = 12;
			vec->z = 151;
			vec->w = -13;
			map2.set(i, vec);
			Vector4 *p = nullptr;
			map2.lookup(i, p);
			CHECK(*p == *vec);
		}

		OAHashMap<int64_t, Vector4 *> map3(map2);
		for (OAHashMap<int64_t, Vector4 *>::Iterator it = map2.iter(); it.valid; it = map2.next_iter(it)) {
			memdelete(*(it.value));
		}
	}
	CHECK(Memory::get_mem_usage() == pre_mem);
}

} // namespace TestOAHashMap

#endif // TEST_OA_HASH_MAP_H
