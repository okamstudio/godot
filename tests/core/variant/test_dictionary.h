/**************************************************************************/
/*  test_dictionary.h                                                     */
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

#include "core/variant/typed_dictionary.h"
#include "tests/test_macros.h"

namespace TestDictionary {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}
static inline Dictionary build_dictionary() {
	return Dictionary();
}
template <typename... Targs>
static inline Dictionary build_dictionary(Variant key, Variant item, Targs... Fargs) {
	Dictionary d = build_dictionary(Fargs...);
	d[key] = item;
	return d;
}

TEST_CASE("[Dictionary] Assignment using bracket notation ([])") {
	Dictionary map;
	map["Hello"] = 0;
	CHECK(int(map["Hello"]) == 0);
	map["Hello"] = 3;
	CHECK(int(map["Hello"]) == 3);
	map["World!"] = 4;
	CHECK(int(map["World!"]) == 4);

	map[StringName("HelloName")] = 6;
	CHECK(int(map[StringName("HelloName")]) == 6);
	CHECK(int(map.find_key(6).get_type()) == Variant::STRING_NAME);
	map[StringName("HelloName")] = 7;
	CHECK(int(map[StringName("HelloName")]) == 7);

	// Test String and StringName are equivalent.
	map[StringName("Hello")] = 8;
	CHECK(int(map["Hello"]) == 8);
	map["Hello"] = 9;
	CHECK(int(map[StringName("Hello")]) == 9);

	// Test non-string keys, since keys can be of any Variant type.
	map[12345] = -5;
	CHECK(int(map[12345]) == -5);
	map[false] = 128;
	CHECK(int(map[false]) == 128);
	map[Vector2(10, 20)] = 30;
	CHECK(int(map[Vector2(10, 20)]) == 30);
	map[0] = 400;
	CHECK(int(map[0]) == 400);
	// Check that assigning 0 doesn't overwrite the value for `false`.
	CHECK(int(map[false]) == 128);

	// Ensure read-only maps aren't modified by non-existing keys.
	const int length = map.size();
	map.make_read_only();
	CHECK(int(map["This key does not exist"].get_type()) == Variant::NIL);
	CHECK(map.size() == length);
}

TEST_CASE("[Dictionary] List init") {
	Dictionary dict{
		{ 0, "int" },
		{ "packed_string_array", PackedStringArray({ "array", "of", "values" }) },
		{ "key", Dictionary({ { "nested", 200 } }) },
		{ Vector2(), "v2" },
	};
	CHECK(dict.size() == 4);
	CHECK(dict[0] == "int");
	CHECK(PackedStringArray(dict["packed_string_array"])[2] == "values");
	CHECK(Dictionary(dict["key"])["nested"] == Variant(200));
	CHECK(dict[Vector2()] == "v2");

	TypedDictionary<double, double> tdict{
		{ 0.0, 1.0 },
		{ 5.0, 2.0 },
	};
	CHECK_EQ(tdict[0.0], Variant(1.0));
	CHECK_EQ(tdict[5.0], Variant(2.0));
}

TEST_CASE("[Dictionary] get_key_lists()") {
	Dictionary map;
	List<Variant> keys;
	List<Variant> *ptr = &keys;
	map.get_key_list(ptr);
	CHECK(keys.is_empty());
	map[1] = 3;
	map.get_key_list(ptr);
	CHECK(keys.size() == 1);
	CHECK(int(keys.front()->get()) == 1);
	map[2] = 4;
	map.get_key_list(ptr);
	CHECK(keys.size() == 3);
}

TEST_CASE("[Dictionary] get_key_at_index()") {
	Dictionary map;
	map[4] = 3;
	Variant val = map.get_key_at_index(0);
	CHECK(int(val) == 4);
	map[3] = 1;
	val = map.get_key_at_index(0);
	CHECK(int(val) == 4);
	val = map.get_key_at_index(1);
	CHECK(int(val) == 3);
}

TEST_CASE("[Dictionary] getptr()") {
	Dictionary map;
	map[1] = 3;
	Variant *key = map.getptr(1);
	CHECK(int(*key) == 3);
	key = map.getptr(2);
	CHECK(key == nullptr);
}

TEST_CASE("[Dictionary] get_valid()") {
	Dictionary map;
	map[1] = 3;
	Variant val = map.get_valid(1);
	CHECK(int(val) == 3);
}
TEST_CASE("[Dictionary] get()") {
	Dictionary map;
	map[1] = 3;
	Variant val = map.get(1, -1);
	CHECK(int(val) == 3);
}

TEST_CASE("[Dictionary] size(), empty() and clear()") {
	Dictionary map;
	CHECK(map.size() == 0);
	CHECK(map.is_empty());
	map[1] = 3;
	CHECK(map.size() == 1);
	CHECK(!map.is_empty());
	map.clear();
	CHECK(map.size() == 0);
	CHECK(map.is_empty());
}

TEST_CASE("[Dictionary] has() and has_all()") {
	Dictionary map;
	CHECK(map.has(1) == false);
	map[1] = 3;
	CHECK(map.has(1));
	Array keys;
	keys.push_back(1);
	CHECK(map.has_all(keys));
	keys.push_back(2);
	CHECK(map.has_all(keys) == false);
}

TEST_CASE("[Dictionary] keys() and values()") {
	Dictionary map;
	Array keys = map.keys();
	Array values = map.values();
	CHECK(keys.is_empty());
	CHECK(values.is_empty());
	map[1] = 3;
	keys = map.keys();
	values = map.values();
	CHECK(int(keys[0]) == 1);
	CHECK(int(values[0]) == 3);
}

TEST_CASE("[Dictionary] Duplicate dictionary") {
	// d = {1: {1: 1}, {2: 2}: [2], [3]: 3}
	Dictionary k2 = build_dictionary(2, 2);
	Array k3 = build_array(3);
	Dictionary d = build_dictionary(1, build_dictionary(1, 1), k2, build_array(2), k3, 3);

	// Deep copy
	Dictionary deep_d = d.duplicate(true);
	CHECK_MESSAGE(deep_d.id() != d.id(), "Should create a new dictionary");
	CHECK_MESSAGE(Dictionary(deep_d[1]).id() != Dictionary(d[1]).id(), "Should clone nested dictionary");
	CHECK_MESSAGE(Array(deep_d[k2]).id() != Array(d[k2]).id(), "Should clone nested array");
	CHECK_EQ(deep_d, d);
	deep_d[0] = 0;
	CHECK_NE(deep_d, d);
	deep_d.erase(0);
	Dictionary(deep_d[1]).operator[](0) = 0;
	CHECK_NE(deep_d, d);
	Dictionary(deep_d[1]).erase(0);
	CHECK_EQ(deep_d, d);
	// Keys should also be copied
	k2[0] = 0;
	CHECK_NE(deep_d, d);
	k2.erase(0);
	CHECK_EQ(deep_d, d);
	k3.push_back(0);
	CHECK_NE(deep_d, d);
	k3.pop_back();
	CHECK_EQ(deep_d, d);

	// Shallow copy
	Dictionary shallow_d = d.duplicate(false);
	CHECK_MESSAGE(shallow_d.id() != d.id(), "Should create a new array");
	CHECK_MESSAGE(Dictionary(shallow_d[1]).id() == Dictionary(d[1]).id(), "Should keep nested dictionary");
	CHECK_MESSAGE(Array(shallow_d[k2]).id() == Array(d[k2]).id(), "Should keep nested array");
	CHECK_EQ(shallow_d, d);
	shallow_d[0] = 0;
	CHECK_NE(shallow_d, d);
	shallow_d.erase(0);
#if 0 // TODO: recursion in dict key currently is buggy
	// Keys should also be shallowed
	k2[0] = 0;
	CHECK_EQ(shallow_d, d);
	k2.erase(0);
	k3.push_back(0);
	CHECK_EQ(shallow_d, d);
#endif
}

TEST_CASE("[Dictionary] Duplicate recursive dictionary") {
	// Self recursive
	Dictionary d;
	d[1] = d;

	Dictionary d_shallow = d.duplicate(false);
	CHECK_EQ(d, d_shallow);

	// Deep copy of recursive dictionary endup with recursion limit and return
	// an invalid result (multiple nested dictionaries), the point is we should
	// not end up with a segfault and an error log should be printed
	ERR_PRINT_OFF;
	d.duplicate(true);
	ERR_PRINT_ON;

	// Nested recursive
	Dictionary d1;
	Dictionary d2;
	d1[2] = d2;
	d2[1] = d1;

	Dictionary d1_shallow = d1.duplicate(false);
	CHECK_EQ(d1, d1_shallow);

	// Same deep copy issue as above
	ERR_PRINT_OFF;
	d1.duplicate(true);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d.clear();
	d1.clear();
	d2.clear();
}

#if 0 // TODO: duplicate recursion in dict key is currently buggy
TEST_CASE("[Dictionary] Duplicate recursive dictionary on keys") {
	// Self recursive
	Dictionary d;
	d[d] = d;

	Dictionary d_shallow = d.duplicate(false);
	CHECK_EQ(d, d_shallow);

	// Deep copy of recursive dictionary endup with recursion limit and return
	// an invalid result (multiple nested dictionaries), the point is we should
	// not end up with a segfault and an error log should be printed
	ERR_PRINT_OFF;
	d.duplicate(true);
	ERR_PRINT_ON;

	// Nested recursive
	Dictionary d1;
	Dictionary d2;
	d1[d2] = d2;
	d2[d1] = d1;

	Dictionary d1_shallow = d1.duplicate(false);
	CHECK_EQ(d1, d1_shallow);

	// Same deep copy issue as above
	ERR_PRINT_OFF;
	d1.duplicate(true);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d.clear();
	d1.clear();
	d2.clear();
}
#endif

TEST_CASE("[Dictionary] Hash dictionary") {
	// d = {1: {1: 1}, {2: 2}: [2], [3]: 3}
	Dictionary k2 = build_dictionary(2, 2);
	Array k3 = build_array(3);
	Dictionary d = build_dictionary(1, build_dictionary(1, 1), k2, build_array(2), k3, 3);
	uint32_t original_hash = d.hash();

	// Modify dict change the hash
	d[0] = 0;
	CHECK_NE(d.hash(), original_hash);
	d.erase(0);
	CHECK_EQ(d.hash(), original_hash);

	// Modify nested item change the hash
	Dictionary(d[1]).operator[](0) = 0;
	CHECK_NE(d.hash(), original_hash);
	Dictionary(d[1]).erase(0);
	Array(d[k2]).push_back(0);
	CHECK_NE(d.hash(), original_hash);
	Array(d[k2]).pop_back();

	// Modify a key change the hash
	k2[0] = 0;
	CHECK_NE(d.hash(), original_hash);
	k2.erase(0);
	CHECK_EQ(d.hash(), original_hash);
	k3.push_back(0);
	CHECK_NE(d.hash(), original_hash);
	k3.pop_back();
	CHECK_EQ(d.hash(), original_hash);

	// Duplication doesn't change the hash
	Dictionary d2 = d.duplicate(true);
	CHECK_EQ(d2.hash(), original_hash);
}

TEST_CASE("[Dictionary] Hash recursive dictionary") {
	Dictionary d;
	d[1] = d;

	// Hash should reach recursion limit, we just make sure this doesn't blow up
	ERR_PRINT_OFF;
	d.hash();
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d.clear();
}

#if 0 // TODO: recursion in dict key is currently buggy
TEST_CASE("[Dictionary] Hash recursive dictionary on keys") {
	Dictionary d;
	d[d] = 1;

	// Hash should reach recursion limit, we just make sure this doesn't blow up
	ERR_PRINT_OFF;
	d.hash();
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d.clear();
}
#endif

TEST_CASE("[Dictionary] Empty comparison") {
	Dictionary d1;
	Dictionary d2;

	// test both operator== and operator!=
	CHECK_EQ(d1, d2);
	CHECK_FALSE(d1 != d2);
}

TEST_CASE("[Dictionary] Flat comparison") {
	Dictionary d1 = build_dictionary(1, 1);
	Dictionary d2 = build_dictionary(1, 1);
	Dictionary other_d = build_dictionary(2, 1);

	// test both operator== and operator!=
	CHECK_EQ(d1, d1); // compare self
	CHECK_FALSE(d1 != d1);
	CHECK_EQ(d1, d2); // different equivalent arrays
	CHECK_FALSE(d1 != d2);
	CHECK_NE(d1, other_d); // different arrays with different content
	CHECK_FALSE(d1 == other_d);
}

TEST_CASE("[Dictionary] Nested dictionary comparison") {
	// d1 = {1: {2: {3: 4}}}
	Dictionary d1 = build_dictionary(1, build_dictionary(2, build_dictionary(3, 4)));

	Dictionary d2 = d1.duplicate(true);

	// other_d = {1: {2: {3: 0}}}
	Dictionary other_d = build_dictionary(1, build_dictionary(2, build_dictionary(3, 0)));

	// test both operator== and operator!=
	CHECK_EQ(d1, d1); // compare self
	CHECK_FALSE(d1 != d1);
	CHECK_EQ(d1, d2); // different equivalent arrays
	CHECK_FALSE(d1 != d2);
	CHECK_NE(d1, other_d); // different arrays with different content
	CHECK_FALSE(d1 == other_d);
}

TEST_CASE("[Dictionary] Nested array comparison") {
	// d1 = {1: [2, 3]}
	Dictionary d1 = build_dictionary(1, build_array(2, 3));

	Dictionary d2 = d1.duplicate(true);

	// other_d = {1: [2, 0]}
	Dictionary other_d = build_dictionary(1, build_array(2, 0));

	// test both operator== and operator!=
	CHECK_EQ(d1, d1); // compare self
	CHECK_FALSE(d1 != d1);
	CHECK_EQ(d1, d2); // different equivalent arrays
	CHECK_FALSE(d1 != d2);
	CHECK_NE(d1, other_d); // different arrays with different content
	CHECK_FALSE(d1 == other_d);
}

TEST_CASE("[Dictionary] Recursive comparison") {
	Dictionary d1;
	d1[1] = d1;

	Dictionary d2;
	d2[1] = d2;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_EQ(d1, d2);
	CHECK_FALSE(d1 != d2);
	ERR_PRINT_ON;

	d1[2] = 2;
	d2[2] = 2;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_EQ(d1, d2);
	CHECK_FALSE(d1 != d2);
	ERR_PRINT_ON;

	d1[3] = 3;
	d2[3] = 0;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_NE(d1, d2);
	CHECK_FALSE(d1 == d2);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d1.clear();
	d2.clear();
}

#if 0 // TODO: recursion in dict key is currently buggy
TEST_CASE("[Dictionary] Recursive comparison on keys") {
	Dictionary d1;
	// Hash computation should reach recursion limit
	ERR_PRINT_OFF;
	d1[d1] = 1;
	ERR_PRINT_ON;

	Dictionary d2;
	// Hash computation should reach recursion limit
	ERR_PRINT_OFF;
	d2[d2] = 1;
	ERR_PRINT_ON;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_EQ(d1, d2);
	CHECK_FALSE(d1 != d2);
	ERR_PRINT_ON;

	d1[2] = 2;
	d2[2] = 2;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_EQ(d1, d2);
	CHECK_FALSE(d1 != d2);
	ERR_PRINT_ON;

	d1[3] = 3;
	d2[3] = 0;

	// Comparison should reach recursion limit
	ERR_PRINT_OFF;
	CHECK_NE(d1, d2);
	CHECK_FALSE(d1 == d2);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d1.clear();
	d2.clear();
}
#endif

TEST_CASE("[Dictionary] Recursive self comparison") {
	Dictionary d1;
	Dictionary d2;
	d1[1] = d2;
	d2[1] = d1;

	CHECK_EQ(d1, d1);
	CHECK_FALSE(d1 != d1);

	// Break the recursivity otherwise Dictionary teardown will leak memory
	d1.clear();
	d2.clear();
}

TEST_CASE("[Dictionary] Order and find") {
	Dictionary d;
	d[4] = "four";
	d[8] = "eight";
	d[12] = "twelve";
	d["4"] = "four";

	Array keys;
	keys.append(4);
	keys.append(8);
	keys.append(12);
	keys.append("4");

	CHECK_EQ(d.keys(), keys);
	CHECK_EQ(d.find_key("four"), Variant(4));
	CHECK_EQ(d.find_key("does not exist"), Variant());
}

TEST_CASE("[Dictionary] Typed copying") {
	TypedDictionary<int, int> d1;
	d1[0] = 1;

	TypedDictionary<double, double> d2;
	d2[0] = 1.0;

	Dictionary d3 = d1;
	TypedDictionary<int, int> d4 = d3;

	Dictionary d5 = d2;
	TypedDictionary<int, int> d6 = d5;

	d3[0] = 2;
	d4[0] = 3;

	// Same typed TypedDictionary should be shared.
	CHECK_EQ(d1[0], Variant(3));
	CHECK_EQ(d3[0], Variant(3));
	CHECK_EQ(d4[0], Variant(3));

	d5[0] = 2.0;
	d6[0] = 3.0;

	// Different typed TypedDictionary should not be shared.
	CHECK_EQ(d2[0], Variant(2.0));
	CHECK_EQ(d5[0], Variant(2.0));
	CHECK_EQ(d6[0], Variant(3.0));

	d1.clear();
	d2.clear();
	d3.clear();
	d4.clear();
	d5.clear();
	d6.clear();
}

} // namespace TestDictionary
