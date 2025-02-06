/**************************************************************************/
/*  hash_set.h                                                            */
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

#ifndef HASH_SET_H
#define HASH_SET_H

#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "core/templates/hash_map.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/paged_allocator.h"

struct HashSetData {
	union {
		struct
		{
			uint32_t hash;
			uint32_t hash_to_key;
		};
		uint64_t data;
	};
};

static_assert(sizeof(HashSetData) == 8);

/**
 * Implementation of Set using a bidi indexed hash map.
 * Use RBSet instead of this only if the following conditions are met:
 *
 * - You need to keep an iterator or const pointer to Key and you intend to add/remove elements in the meantime.
 * - Iteration order does matter (via operator<).
 *
 */
template <typename TKey,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>>
class HashSet {
public:
	// Must be 2^n.
	static constexpr uint32_t INITIAL_CAPACITY = 32;
	static constexpr uint32_t EMPTY_HASH = 0;

private:
	TKey *keys = nullptr;
	HashSetData *set_data = nullptr;

	// Due to optimization, this is `capacity - 1`. Use + 1 to get normal capacity.
	uint32_t capacity = 0;
	uint32_t num_elements = 0;

	_FORCE_INLINE_ uint32_t _hash(const TKey &p_key) const {
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

	_FORCE_INLINE_ bool _lookup_pos(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos) const {
		if (unlikely(keys == nullptr)) {
			return false; // Failed lookups, no elements.
		}
		return _lookup_pos_with_hash(p_key, r_pos, r_hash_pos, _hash(p_key));
	}

	_FORCE_INLINE_ bool _lookup_pos_with_hash(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos, uint32_t p_hash) const {
		if (unlikely(keys == nullptr)) {
			return false; // Failed lookups, no elements.
		}

		uint32_t pos = p_hash & capacity;
		HashSetData data = set_data[pos];
		if (data.hash == p_hash && Comparator::compare(keys[data.hash_to_key], p_key)) {
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
			data = set_data[pos];
			if (data.hash == p_hash && Comparator::compare(keys[data.hash_to_key], p_key)) {
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

	_FORCE_INLINE_ uint32_t _insert_with_hash(uint32_t p_hash, uint32_t p_index) {
		uint32_t pos = p_hash & capacity;

		if (set_data[pos].data == EMPTY_HASH) {
			uint64_t data = ((uint64_t)p_index << 32) | p_hash;
			set_data[pos].data = data;
			return pos;
		}

		uint32_t distance = 1;
		pos = (pos + 1) & capacity;
		HashSetData c_data;
		c_data.hash = p_hash;
		c_data.hash_to_key = p_index;

		while (true) {
			if (set_data[pos].data == EMPTY_HASH) {
#ifdef DEV_ENABLED
				if (unlikely(distance > 12)) {
					WARN_PRINT("Excessive collision count (" +
							itos(distance) + "), is the right hash function being used?\nClass Name:" + __PRETTY_FUNCTION__);
				}
#endif
				set_data[pos] = c_data;
				return pos;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, set_data[pos].hash, capacity);
			if (existing_probe_len < distance) {
				SWAP(c_data, set_data[pos]);
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

		HashSetData *old_set_data = set_data;

		set_data = reinterpret_cast<HashSetData *>(Memory::alloc_static(sizeof(HashSetData) * real_capacity));
		keys = reinterpret_cast<TKey *>(Memory::realloc_static(keys, sizeof(TKey) * (_get_resize_count(capacity) + 1)));

		memset(set_data, EMPTY_HASH, real_capacity * sizeof(HashSetData));

		if (num_elements != 0) {
			for (uint32_t i = 0; i < real_old_capacity; i++) {
				HashSetData data = old_set_data[i];
				if (data.data != EMPTY_HASH) {
					_insert_with_hash(data.hash, data.hash_to_key);
				}
			}
		}

		Memory::free_static(old_set_data);
	}

	_FORCE_INLINE_ int32_t _insert(const TKey &p_key) {
		if (unlikely(keys == nullptr)) {
			// Allocate on demand to save memory.

			uint32_t real_capacity = capacity + 1;
			set_data = reinterpret_cast<HashSetData *>(Memory::alloc_static(sizeof(HashSetData) * real_capacity));
			keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * (_get_resize_count(capacity) + 1)));

			memset(set_data, EMPTY_HASH, real_capacity * sizeof(HashSetData));
		}

		uint32_t pos = 0;
		uint32_t h_pos = 0;
		uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, h_pos, hash);

		if (exists) {
			return pos;
		} else {
			if (unlikely(num_elements > _get_resize_count(capacity))) {
				_resize_and_rehash(capacity * 2);
			}

			if constexpr (!std::is_trivially_constructible_v<TKey>) {
				memnew_placement(&keys[num_elements], TKey(p_key));
			} else {
				TKey key = p_key;
				keys[num_elements] = key;
			}

			_insert_with_hash(hash, num_elements);
			num_elements++;
			return num_elements - 1;
		}
	}

	void _init_from(const HashSet &p_other) {
		capacity = p_other.capacity;
		uint32_t real_capacity = capacity + 1;
		num_elements = p_other.num_elements;

		if (p_other.num_elements == 0) {
			return;
		}

		set_data = reinterpret_cast<HashSetData *>(Memory::alloc_static(sizeof(HashSetData) * real_capacity));
		keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * (_get_resize_count(capacity) + 1)));

		if constexpr (std::is_trivially_copyable_v<TKey>) {
			memcpy(keys, p_other.keys, sizeof(TKey) * num_elements);
		} else {
			for (uint32_t i = 0; i < num_elements; i++) {
				memnew_placement(&keys[i], TKey(p_other.keys[i]));
			}
		}

		memcpy(set_data, p_other.set_data, sizeof(HashSetData) * real_capacity);
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity + 1; }
	_FORCE_INLINE_ uint32_t size() const { return num_elements; }

	/* Standard Godot Container API */

	_FORCE_INLINE_ bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (keys == nullptr || num_elements == 0) {
			return;
		}

		memset(set_data, EMPTY_HASH, (capacity + 1) * sizeof(HashSetData));
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			for (uint32_t i = 0; i < num_elements; i++) {
				keys[i].~TKey();
			}
		}

		num_elements = 0;
	}

	bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		uint32_t h_pos = 0;
		return _lookup_pos(p_key, _pos, h_pos);
	}

	bool erase(const TKey &p_key) {
		uint32_t pos = 0;
		uint32_t key_pos = 0;
		bool exists = _lookup_pos(p_key, key_pos, pos);

		if (!exists) {
			return false;
		}

		uint32_t next_pos = (pos + 1) & capacity;
		while (set_data[next_pos].hash != EMPTY_HASH && _get_probe_length(next_pos, set_data[next_pos].hash, capacity) != 0) {
			SWAP(set_data[next_pos], set_data[pos]);

			pos = next_pos;
			next_pos = (next_pos + 1) & capacity;
		}

		set_data[pos].data = EMPTY_HASH;
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			keys[key_pos].~TKey();
		}
		num_elements--;
		if (key_pos < num_elements) {
			// Not the last key, move the last one here to keep keys lineal.
			if constexpr (!std::is_trivially_constructible_v<TKey>) {
				memnew_placement(&keys[key_pos], TKey(keys[num_elements]));
			} else {
				TKey key = keys[num_elements];
				keys[key_pos] = key;
			}
			uint32_t h_pos = 0;
			_lookup_pos(keys[num_elements], pos, h_pos);
			set_data[h_pos].hash_to_key = key_pos;

			if constexpr (!std::is_trivially_destructible_v<TKey>) {
				keys[num_elements].~TKey();
			}
		}

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once, must be larger than old capacity.
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND_MSG(p_new_capacity < get_capacity(), "It is impossible to reserve less capacity than is currently available.");
		if (keys == nullptr) {
			capacity = MAX(4u, p_new_capacity);
			capacity = next_power_of_2(capacity) - 1;
			return; // Unallocated yet.
		}
		_resize_and_rehash(p_new_capacity);
	}

	/** Iterator API **/

	struct Iterator {
		_FORCE_INLINE_ const TKey &operator*() const {
			return *key;
		}
		_FORCE_INLINE_ const TKey *operator->() const {
			return key;
		}
		_FORCE_INLINE_ Iterator &operator++() {
			key++;
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			key--;
			if (key < begin) {
				key = end;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return key == b.key; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return key != b.key; }

		_FORCE_INLINE_ explicit operator bool() const {
			return key != end;
		}

		_FORCE_INLINE_ Iterator(TKey *p_key, TKey *p_begin, TKey *p_end) {
			key = p_key;
			begin = p_begin;
			end = p_end;
		}
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) {
			key = p_it.key;
			begin = p_it.begin;
			end = p_it.end;
		}
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			key = p_it.key;
			begin = p_it.begin;
			end = p_it.end;
		}

	private:
		TKey *key = nullptr;
		TKey *begin = nullptr;
		TKey *end = nullptr;
	};

	_FORCE_INLINE_ Iterator begin() const {
		return Iterator(keys, keys, keys + num_elements);
	}
	_FORCE_INLINE_ Iterator end() const {
		return Iterator(keys + num_elements, keys, keys + num_elements);
	}
	_FORCE_INLINE_ Iterator last() const {
		if (unlikely(num_elements == 0)) {
			return Iterator(nullptr, nullptr, nullptr);
		}
		return Iterator(keys + num_elements - 1, keys, keys + num_elements);
	}

	Iterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		uint32_t h_pos = 0;
		bool exists = _lookup_pos(p_key, pos, h_pos);
		if (!exists) {
			return end();
		}
		return Iterator(keys + pos, keys, keys + num_elements);
	}

	void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(*p_iter);
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key) {
		uint32_t pos = _insert(p_key);
		return Iterator(keys + pos, keys, keys + num_elements);
	}

	/* Constructors */

	HashSet(const HashSet &p_other) {
		_init_from(p_other);
	}

	void operator=(const HashSet &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}

		reset();

		_init_from(p_other);
	}

	HashSet(uint32_t p_initial_capacity) {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_initial_capacity);
		capacity = next_power_of_2(capacity) - 1;
	}
	HashSet() :
			capacity(INITIAL_CAPACITY - 1) {
	}

	HashSet(std::initializer_list<TKey> p_init) {
		reserve(p_init.size());
		for (const TKey &E : p_init) {
			insert(E);
		}
	}

	void reset() {
		if (keys != nullptr) {
			if constexpr (!std::is_trivially_destructible_v<TKey>) {
				for (uint32_t i = 0; i < num_elements; i++) {
					keys[i].~TKey();
				}
			}
			Memory::free_static(keys);
			Memory::free_static(set_data);
			keys = nullptr;
		}
		capacity = INITIAL_CAPACITY - 1;
	}

	~HashSet() {
		reset();
	}
};

#endif // HASH_SET_H
