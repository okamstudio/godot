/**************************************************************************/
/*  a_hash_map.h                                                          */
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

#include "core/templates/hash_map.h"

struct HashMapData {
	union {
		uint64_t data;
		struct
		{
			uint32_t hash;
			uint32_t hash_to_key;
		};
	};
};

static_assert(sizeof(HashMapData) == 8);

/**
 * An array-based implementation of a hash map. It is very efficient in terms of performance and
 * memory usage. Works like a dynamic array, adding elements to the end of the array, and
 * allows you to access array elements by their index by using `get_by_index` method.
 * Example:
 * ```
 *  AHashMap<int, Object *> map;
 *
 *  int get_object_id_by_number(int p_number) {
 *		int id = map.get_index(p_number);
 *		return id;
 *  }
 *
 *  Object *get_object_by_id(int p_id) {
 *		map.get_by_index(p_id).value;
 *  }
 * ```
 * Still, don`t erase the elements because ID can break.
 *
 * When an element erase, its place is taken by the element from the end.
 *
 *        <-------------
 *      |               |
 *  6 8 X 9 32 -1 5 -10 7 X X X
 *  6 8 7 9 32 -1 5 -10 X X X X
 *
 *
 * Use RBMap if you need to iterate over sorted elements.
 *
 * Use HashMap if:
 *   - You need to keep an iterator or const pointer to Key and you intend to add/remove elements in the meantime.
 *   - You need to preserve the insertion order when using erase.
 *
 * It is recommended to use `HashMap` if `KeyValue` size is very large.
 */
template <typename TKey, typename TValue,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>>
class AHashMap {
public:
	// Must be a power of two.
	static constexpr uint32_t INITIAL_CAPACITY = 16;
	static constexpr uint32_t EMPTY_HASH = 0;
	static_assert(EMPTY_HASH == 0, "EMPTY_HASH must always be 0 for the memcpy() optimization.");

private:
	typedef KeyValue<TKey, TValue> MapKeyValue;
	MapKeyValue *elements = nullptr;
	HashMapData *map_data = nullptr;

	// Due to optimization, this is `capacity - 1`. Use + 1 to get normal capacity.
	uint32_t capacity = 0;
	uint32_t num_elements = 0;

	uint32_t _hash(const TKey &p_key) const {
		uint32_t hash = Hasher::hash(p_key);

		if (unlikely(hash == EMPTY_HASH)) {
			hash = EMPTY_HASH + 1;
		}

		return hash;
	}

	static _FORCE_INLINE_ uint32_t _get_resize_count(uint32_t p_capacity) {
		return p_capacity ^ (p_capacity + 1) >> 2; // = get_capacity() * 0.75 - 1; Works only if p_capacity = 2^n - 1.
	}

	static _FORCE_INLINE_ uint32_t _get_probe_length(uint32_t p_pos, uint32_t p_hash, uint32_t p_local_capacity) {
		const uint32_t original_pos = p_hash & p_local_capacity;
		return (p_pos - original_pos + p_local_capacity + 1) & p_local_capacity;
	}

	bool _lookup_pos(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos) const {
		if (unlikely(elements == nullptr)) {
			return false; // Failed lookups, no elements.
		}
		return _lookup_pos_with_hash(p_key, r_pos, r_hash_pos, _hash(p_key));
	}

	bool _lookup_pos_with_hash(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos, uint32_t p_hash) const {
		if (unlikely(elements == nullptr)) {
			return false; // Failed lookups, no elements.
		}

		uint32_t pos = p_hash & capacity;
		HashMapData data = map_data[pos];
		if (data.hash == p_hash && Comparator::compare(elements[data.hash_to_key].key, p_key)) {
			r_pos = data.hash_to_key;
			r_hash_pos = pos;
			return true;
		}

		if (data.data == EMPTY_HASH) {
			return false;
		}

		// A collision occurred.
		pos = (pos + 1) & capacity;
		uint32_t distance = 1;
		while (true) {
			data = map_data[pos];
			if (data.hash == p_hash && Comparator::compare(elements[data.hash_to_key].key, p_key)) {
				r_pos = data.hash_to_key;
				r_hash_pos = pos;
				return true;
			}

			if (data.data == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_length(pos, data.hash, capacity)) {
				return false;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	uint32_t _insert_with_hash(uint32_t p_hash, uint32_t p_index) {
		uint32_t pos = p_hash & capacity;

		if (map_data[pos].data == EMPTY_HASH) {
			uint64_t data = ((uint64_t)p_index << 32) | p_hash;
			map_data[pos].data = data;
			return pos;
		}

		uint32_t distance = 1;
		pos = (pos + 1) & capacity;
		HashMapData c_data;
		c_data.hash = p_hash;
		c_data.hash_to_key = p_index;

		while (true) {
			if (map_data[pos].data == EMPTY_HASH) {
#ifdef DEV_ENABLED
				if (unlikely(distance > 12)) {
					WARN_PRINT("Excessive collision count (" +
							itos(distance) + "), is the right hash function being used?");
				}
#endif
				map_data[pos] = c_data;
				return pos;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, map_data[pos].hash, capacity);
			if (existing_probe_len < distance) {
				SWAP(c_data, map_data[pos]);
				distance = existing_probe_len;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		uint32_t real_old_capacity = capacity + 1;
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_new_capacity);
		uint32_t real_capacity = next_power_of_2(capacity);
		capacity = real_capacity - 1;

		HashMapData *old_map_data = map_data;

		map_data = reinterpret_cast<HashMapData *>(Memory::alloc_static(sizeof(HashMapData) * real_capacity));
		elements = reinterpret_cast<MapKeyValue *>(Memory::realloc_static(elements, sizeof(MapKeyValue) * (_get_resize_count(capacity) + 1)));

		memset(map_data, EMPTY_HASH, real_capacity * sizeof(HashMapData));

		if (num_elements != 0) {
			for (uint32_t i = 0; i < real_old_capacity; i++) {
				HashMapData data = old_map_data[i];
				if (data.data != EMPTY_HASH) {
					_insert_with_hash(data.hash, data.hash_to_key);
				}
			}
		}

		Memory::free_static(old_map_data);
	}

	int32_t _insert_element(const TKey &p_key, const TValue &p_value, uint32_t p_hash) {
		if (unlikely(elements == nullptr)) {
			// Allocate on demand to save memory.

			uint32_t real_capacity = capacity + 1;
			map_data = reinterpret_cast<HashMapData *>(Memory::alloc_static(sizeof(HashMapData) * real_capacity));
			elements = reinterpret_cast<MapKeyValue *>(Memory::alloc_static(sizeof(MapKeyValue) * (_get_resize_count(capacity) + 1)));

			memset(map_data, EMPTY_HASH, real_capacity * sizeof(HashMapData));
		}

		if (unlikely(num_elements > _get_resize_count(capacity))) {
			_resize_and_rehash(capacity * 2);
		}

		memnew_placement(&elements[num_elements], MapKeyValue(p_key, p_value));

		_insert_with_hash(p_hash, num_elements);
		num_elements++;
		return num_elements - 1;
	}

	void _init_from(const AHashMap &p_other) {
		capacity = p_other.capacity;
		uint32_t real_capacity = capacity + 1;
		num_elements = p_other.num_elements;

		if (p_other.num_elements == 0) {
			return;
		}

		map_data = reinterpret_cast<HashMapData *>(Memory::alloc_static(sizeof(HashMapData) * real_capacity));
		elements = reinterpret_cast<MapKeyValue *>(Memory::alloc_static(sizeof(MapKeyValue) * (_get_resize_count(capacity) + 1)));

		if constexpr (std::is_trivially_copyable_v<TKey> && std::is_trivially_copyable_v<TValue>) {
			void *destination = elements;
			const void *source = p_other.elements;
			memcpy(destination, source, sizeof(MapKeyValue) * num_elements);
		} else {
			for (uint32_t i = 0; i < num_elements; i++) {
				memnew_placement(&elements[i], MapKeyValue(p_other.elements[i]));
			}
		}

		memcpy(map_data, p_other.map_data, sizeof(HashMapData) * real_capacity);
	}

public:
	/* Standard Godot Container API */

	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity + 1; }
	_FORCE_INLINE_ uint32_t size() const { return num_elements; }

	_FORCE_INLINE_ bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (elements == nullptr || num_elements == 0) {
			return;
		}

		memset(map_data, EMPTY_HASH, (capacity + 1) * sizeof(HashMapData));
		if constexpr (!(std::is_trivially_destructible_v<TKey> && std::is_trivially_destructible_v<TValue>)) {
			for (uint32_t i = 0; i < num_elements; i++) {
				elements[i].key.~TKey();
				elements[i].value.~TValue();
			}
		}

		num_elements = 0;
	}

	TValue &get(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t hash_pos = 0;
		bool exists = _lookup_pos(p_key, pos, hash_pos);
		CRASH_COND_MSG(!exists, "AHashMap key not found.");
		return elements[pos].value;
	}

	const TValue &get(const TKey &p_key) const {
		uint32_t pos = 0;
		uint32_t hash_pos = 0;
		bool exists = _lookup_pos(p_key, pos, hash_pos);
		CRASH_COND_MSG(!exists, "AHashMap key not found.");
		return elements[pos].value;
	}

	const TValue *getptr(const TKey &p_key) const {
		uint32_t pos = 0;
		uint32_t hash_pos = 0;
		bool exists = _lookup_pos(p_key, pos, hash_pos);

		if (exists) {
			return &elements[pos].value;
		}
		return nullptr;
	}

	TValue *getptr(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t hash_pos = 0;
		bool exists = _lookup_pos(p_key, pos, hash_pos);

		if (exists) {
			return &elements[pos].value;
		}
		return nullptr;
	}

	bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		uint32_t h_pos = 0;
		return _lookup_pos(p_key, _pos, h_pos);
	}

	bool erase(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t element_pos = 0;
		bool exists = _lookup_pos(p_key, element_pos, pos);

		if (!exists) {
			return false;
		}

		uint32_t next_pos = (pos + 1) & capacity;
		while (map_data[next_pos].hash != EMPTY_HASH && _get_probe_length(next_pos, map_data[next_pos].hash, capacity) != 0) {
			SWAP(map_data[next_pos], map_data[pos]);

			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}

		map_data[pos].data = EMPTY_HASH;
		elements[element_pos].key.~TKey();
		elements[element_pos].value.~TValue();
		num_elements--;

		if (element_pos < num_elements) {
			void *destination = &elements[element_pos];
			const void *source = &elements[num_elements];
			memcpy(destination, source, sizeof(MapKeyValue));
			uint32_t h_pos = 0;
			_lookup_pos(elements[num_elements].key, pos, h_pos);
			map_data[h_pos].hash_to_key = element_pos;
		}

		return true;
	}

	// Replace the key of an entry in-place, without invalidating iterators or changing the entries position during iteration.
	// p_old_key must exist in the map and p_new_key must not, unless it is equal to p_old_key.
	bool replace_key(const TKey &p_old_key, const TKey &p_new_key) {
		if (p_old_key == p_new_key) {
			return true;
		}
		uint32_t pos = 0;
		uint32_t element_pos = 0;
		ERR_FAIL_COND_V(_lookup_pos(p_new_key, element_pos, pos), false);
		ERR_FAIL_COND_V(!_lookup_pos(p_old_key, element_pos, pos), false);
		MapKeyValue &element = elements[element_pos];
		const_cast<TKey &>(element.key) = p_new_key;

		uint32_t next_pos = (pos + 1) & capacity;
		while (map_data[next_pos].hash != EMPTY_HASH && _get_probe_length(next_pos, map_data[next_pos].hash, capacity) != 0) {
			SWAP(map_data[next_pos], map_data[pos]);

			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}

		map_data[pos].data = EMPTY_HASH;

		uint32_t hash = _hash(p_new_key);
		_insert_with_hash(hash, element_pos);

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once, must be larger than old capacity.
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND_MSG(p_new_capacity < get_capacity(), "It is impossible to reserve less capacity than is currently available.");
		if (elements == nullptr) {
			capacity = MAX(4u, p_new_capacity);
			capacity = next_power_of_2(capacity) - 1;
			return; // Unallocated yet.
		}
		_resize_and_rehash(p_new_capacity);
	}

	/** Iterator API **/

	struct ConstIterator {
		_FORCE_INLINE_ const MapKeyValue &operator*() const {
			return *pair;
		}
		_FORCE_INLINE_ const MapKeyValue *operator->() const {
			return pair;
		}
		_FORCE_INLINE_ ConstIterator &operator++() {
			pair++;
			return *this;
		}

		_FORCE_INLINE_ ConstIterator &operator--() {
			pair--;
			if (pair < begin) {
				pair = end;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const ConstIterator &b) const { return pair == b.pair; }
		_FORCE_INLINE_ bool operator!=(const ConstIterator &b) const { return pair != b.pair; }

		_FORCE_INLINE_ explicit operator bool() const {
			return pair != end;
		}

		_FORCE_INLINE_ ConstIterator(MapKeyValue *p_key, MapKeyValue *p_begin, MapKeyValue *p_end) {
			pair = p_key;
			begin = p_begin;
			end = p_end;
		}
		_FORCE_INLINE_ ConstIterator() {}
		_FORCE_INLINE_ ConstIterator(const ConstIterator &p_it) {
			pair = p_it.pair;
			begin = p_it.begin;
			end = p_it.end;
		}
		_FORCE_INLINE_ void operator=(const ConstIterator &p_it) {
			pair = p_it.pair;
			begin = p_it.begin;
			end = p_it.end;
		}

	private:
		MapKeyValue *pair = nullptr;
		MapKeyValue *begin = nullptr;
		MapKeyValue *end = nullptr;
	};

	struct Iterator {
		_FORCE_INLINE_ MapKeyValue &operator*() const {
			return *pair;
		}
		_FORCE_INLINE_ MapKeyValue *operator->() const {
			return pair;
		}
		_FORCE_INLINE_ Iterator &operator++() {
			pair++;
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			pair--;
			if (pair < begin) {
				pair = end;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return pair == b.pair; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return pair != b.pair; }

		_FORCE_INLINE_ explicit operator bool() const {
			return pair != end;
		}

		_FORCE_INLINE_ Iterator(MapKeyValue *p_key, MapKeyValue *p_begin, MapKeyValue *p_end) {
			pair = p_key;
			begin = p_begin;
			end = p_end;
		}
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) {
			pair = p_it.pair;
			begin = p_it.begin;
			end = p_it.end;
		}
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			pair = p_it.pair;
			begin = p_it.begin;
			end = p_it.end;
		}

		operator ConstIterator() const {
			return ConstIterator(pair, begin, end);
		}

	private:
		MapKeyValue *pair = nullptr;
		MapKeyValue *begin = nullptr;
		MapKeyValue *end = nullptr;
	};

	_FORCE_INLINE_ Iterator begin() {
		return Iterator(elements, elements, elements + num_elements);
	}
	_FORCE_INLINE_ Iterator end() {
		return Iterator(elements + num_elements, elements, elements + num_elements);
	}
	_FORCE_INLINE_ Iterator last() {
		if (unlikely(num_elements == 0)) {
			return Iterator(nullptr, nullptr, nullptr);
		}
		return Iterator(elements + num_elements - 1, elements, elements + num_elements);
	}

	Iterator find(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		bool exists = _lookup_pos(p_key, pos, h_pos);
		if (!exists) {
			return end();
		}
		return Iterator(elements + pos, elements, elements + num_elements);
	}

	void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(p_iter->key);
		}
	}

	_FORCE_INLINE_ ConstIterator begin() const {
		return ConstIterator(elements, elements, elements + num_elements);
	}
	_FORCE_INLINE_ ConstIterator end() const {
		return ConstIterator(elements + num_elements, elements, elements + num_elements);
	}
	_FORCE_INLINE_ ConstIterator last() const {
		if (unlikely(num_elements == 0)) {
			return ConstIterator(nullptr, nullptr, nullptr);
		}
		return ConstIterator(elements + num_elements - 1, elements, elements + num_elements);
	}

	ConstIterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		bool exists = _lookup_pos(p_key, pos, h_pos);
		if (!exists) {
			return end();
		}
		return ConstIterator(elements + pos, elements, elements + num_elements);
	}

	/* Indexing */

	const TValue &operator[](const TKey &p_key) const {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		bool exists = _lookup_pos(p_key, pos, h_pos);
		CRASH_COND(!exists);
		return elements[pos].value;
	}

	TValue &operator[](const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, h_pos, hash);

		if (exists) {
			return elements[pos].value;
		} else {
			pos = _insert_element(p_key, TValue(), hash);
			return elements[pos].value;
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key, const TValue &p_value) {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, h_pos, hash);

		if (!exists) {
			pos = _insert_element(p_key, p_value, hash);
		} else {
			elements[pos].value = p_value;
		}
		return Iterator(elements + pos, elements, elements + num_elements);
	}

	// Inserts an element without checking if it already exists.
	Iterator insert_new(const TKey &p_key, const TValue &p_value) {
		DEV_ASSERT(!has(p_key));
		uint32_t hash = _hash(p_key);
		uint32_t pos = _insert_element(p_key, p_value, hash);
		return Iterator(elements + pos, elements, elements + num_elements);
	}

	/* Array methods. */

	// Unsafe. Changing keys and going outside the bounds of an array can lead to undefined behavior.
	KeyValue<TKey, TValue> *get_elements_ptr() {
		return elements;
	}

	// Returns the element index. If not found, returns -1.
	int get_index(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		bool exists = _lookup_pos(p_key, pos, h_pos);
		if (!exists) {
			return -1;
		}
		return pos;
	}

	KeyValue<TKey, TValue> &get_by_index(uint32_t p_index) {
		CRASH_BAD_UNSIGNED_INDEX(p_index, num_elements);
		return elements[p_index];
	}

	bool erase_by_index(uint32_t p_index) {
		if (p_index >= size()) {
			return false;
		}
		return erase(elements[p_index].key);
	}

	/* Constructors */

	AHashMap(const AHashMap &p_other) {
		_init_from(p_other);
	}

	AHashMap(const HashMap<TKey, TValue> &p_other) {
		reserve(p_other.size());
		for (const KeyValue<TKey, TValue> &E : p_other) {
			uint32_t hash = _hash(E.key);
			_insert_element(E.key, E.value, hash);
		}
	}

	void operator=(const AHashMap &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}

		reset();

		_init_from(p_other);
	}

	void operator=(const HashMap<TKey, TValue> &p_other) {
		reset();
		if (p_other.size() > get_capacity()) {
			reserve(p_other.size());
		}
		for (const KeyValue<TKey, TValue> &E : p_other) {
			uint32_t hash = _hash(E.key);
			_insert_element(E.key, E.value, hash);
		}
	}

	AHashMap(uint32_t p_initial_capacity) {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_initial_capacity);
		capacity = next_power_of_2(capacity) - 1;
	}
	AHashMap() :
			capacity(INITIAL_CAPACITY - 1) {
	}

	AHashMap(std::initializer_list<KeyValue<TKey, TValue>> p_init) {
		reserve(p_init.size());
		for (const KeyValue<TKey, TValue> &E : p_init) {
			insert(E.key, E.value);
		}
	}

	void reset() {
		if (elements != nullptr) {
			if constexpr (!(std::is_trivially_destructible_v<TKey> && std::is_trivially_destructible_v<TValue>)) {
				for (uint32_t i = 0; i < num_elements; i++) {
					elements[i].key.~TKey();
					elements[i].value.~TValue();
				}
			}
			Memory::free_static(elements);
			Memory::free_static(map_data);
			elements = nullptr;
		}
		capacity = INITIAL_CAPACITY - 1;
		num_elements = 0;
	}

	~AHashMap() {
		reset();
	}
};

extern template class AHashMap<int, int>;
extern template class AHashMap<String, int>;
extern template class AHashMap<StringName, StringName>;
extern template class AHashMap<StringName, Variant>;
extern template class AHashMap<StringName, int>;
