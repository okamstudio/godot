/**************************************************************************/
/*  inflection_map.h                                                      */
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

#ifndef INFLECTION_MAP_H
#define INFLECTION_MAP_H

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

/// An unordered map that splits elements between a fast-access vector of LinearCount consecutively
/// indexed elements, and a slower-access map holding sparse indexes larger than LinearCount.
///
/// \tparam KeyType is used to lookup values, and must be a type that is convertible to an unsigned integer.
/// \tparam ValueType must have an empty constructor (default or otherwise).
/// \tparam LinearCount
/// \tparam IndexType must be a type that is convertible to an unsigned integer (eg. uint8_t...uint64_t), and which is large enough to represent the number of values in this map.
template <typename KeyType, typename ValueType, size_t LinearCount, typename IndexType = uint16_t>
class InflectionMap {
public:
	using value_type = ValueType;
	class Iterator {
		InflectionMap *map;
		IndexType index;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = ValueType;
		using pointer = value_type *;
		using reference = value_type &;

		Iterator() :
				map(nullptr), index(0) {}
		Iterator(InflectionMap &p_m, const IndexType p_i) :
				map(&p_m), index(p_i) {}

		Iterator &operator=(const Iterator &p_it) {
			map = p_it.map;
			index = p_it.index;
			return *this;
		}

		ValueType *operator->() { return &map->_values[index]; }
		ValueType &operator*() { return map->_values[index]; }
		operator ValueType *() { return &map->_values[index]; }

		bool operator==(const Iterator &p_it) const { return map == p_it.map && index == p_it.index; }
		bool operator!=(const Iterator &p_it) const { return map != p_it.map || index != p_it.index; }

		Iterator &operator++() {
			index++;
			return *this;
		}
		Iterator operator++(int) {
			Iterator t = *this;
			index++;
			return t;
		}

		bool is_valid() const { return index < map->_values.size(); }
	};

	const ValueType &operator[](const KeyType p_idx) const { return get_value(p_idx); }
	ValueType &operator[](const KeyType p_idx) { return get_value(p_idx); }

	Iterator begin() { return Iterator(*this, 0); }
	Iterator end() { return Iterator(*this, _values.size()); }

	bool is_empty() { return _values.is_empty(); }
	size_t size() { return _values.size(); }
	void reserve(size_t p_new_cap) { _values.reserve(p_new_cap); }

protected:
	static constexpr IndexType INVALID = std::numeric_limits<IndexType>::max();
	typedef struct IndexValue {
		IndexType value = INVALID;
	} IndexValue;

	// Returns a reference to the value at the index.
	// If the index has not been initialized, add an empty element at
	// the end of the values array, and set the index to its position.
	ValueType &get_value(KeyType p_idx) {
		IndexValue *val_idx = p_idx < LinearCount ? &_linear_indexes[p_idx] : _inflection_indexes.getptr(p_idx);
		if (val_idx == nullptr || val_idx->value == INVALID) {
			_values.push_back({});
			if (val_idx == nullptr) {
				val_idx = &_inflection_indexes.insert(p_idx, {})->value;
			}
			val_idx->value = _values.size() - 1;
		}
		return _values[val_idx->value];
	}

	TightLocalVector<ValueType> _values;
	HashMap<KeyType, IndexValue> _inflection_indexes;
	IndexValue _linear_indexes[LinearCount];
};

#endif // INFLECTION_MAP_H
