/**************************************************************************/
/*  pair.h                                                                */
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

#ifndef PAIR_H
#define PAIR_H

#include "core/typedefs.h"

template <typename F, typename S>
struct Pair {
	F first;
	S second;

	constexpr Pair() :
			first(), second() {}
	constexpr Pair(const F &p_first, const S &p_second) :
			first(p_first), second(p_second) {}

	constexpr bool operator==(const Pair &p_other) const { return first == p_other.first && second == p_other.second; }
	constexpr bool operator!=(const Pair &p_other) const { return first != p_other.first || second != p_other.second; }
	constexpr bool operator<(const Pair &p_other) const { return first == p_other.first ? (second < p_other.second) : (first < p_other.first); }
	constexpr bool operator>(const Pair &p_other) const { return first == p_other.first ? (second > p_other.second) : (first > p_other.first); }
	constexpr bool operator<=(const Pair &p_other) const { return first == p_other.first ? (second <= p_other.second) : (first < p_other.first); }
	constexpr bool operator>=(const Pair &p_other) const { return first == p_other.first ? (second >= p_other.second) : (first > p_other.first); }
};

template <typename F, typename S>
struct PairSort {
	constexpr bool operator()(const Pair<F, S> &p_lhs, const Pair<F, S> &p_rhs) const {
		return p_lhs < p_rhs;
	}
};

template <typename K, typename V>
struct KeyValue {
	const K key;
	V value;

	void operator=(const KeyValue &p_kv) = delete;

	constexpr KeyValue(const KeyValue &p_kv) :
			key(p_kv.key), value(p_kv.value) {}
	constexpr KeyValue(const K &p_key, const V &p_value) :
			key(p_key), value(p_value) {}
	constexpr KeyValue(const Pair<K, V> &p_pair) :
			key(p_pair.first), value(p_pair.second) {}

	constexpr bool operator==(const KeyValue &p_other) const { return key == p_other.key && value == p_other.value; }
	constexpr bool operator!=(const KeyValue &p_other) const { return key != p_other.key || value != p_other.value; }
	constexpr bool operator<(const KeyValue &p_other) const { return key == p_other.key ? (value < p_other.value) : (key < p_other.key); }
	constexpr bool operator>(const KeyValue &p_other) const { return key == p_other.key ? (value > p_other.value) : (key > p_other.key); }
	constexpr bool operator<=(const KeyValue &p_other) const { return key == p_other.key ? (value <= p_other.value) : (key < p_other.key); }
	constexpr bool operator>=(const KeyValue &p_other) const { return key == p_other.key ? (value >= p_other.value) : (key > p_other.key); }
};

template <typename K, typename V>
struct KeyValueSort {
	constexpr bool operator()(const KeyValue<K, V> &p_lhs, const KeyValue<K, V> &p_rhs) const {
		return p_lhs.key < p_rhs.key;
	}
};

#endif // PAIR_H
