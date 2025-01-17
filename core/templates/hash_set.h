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

#include "core/templates/hashes.h"
#include "core/templates/index_array.h"

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
	// Must be a power of two.
	static constexpr uint32_t INITIAL_CAPACITY = 16;

	_FORCE_INLINE_ bool _compare_function(uint32_t p_pos, const TKey &p_key) const {
		return Comparator::compare(keys[hashes_to_key.get_index(p_pos)], p_key);
	}

private:
	TKey *keys = nullptr;
	Hashes hashes;
	IndexArray hashes_to_key;

	// Due to optimization, this is `capacity - 1`. Use + 1 to get normal capacity.
	uint32_t capacity = 0;
	uint32_t keys_capacity = 0;
	uint32_t num_elements = 0;

	uint32_t _hash(const TKey &p_key) const {
		uint32_t hash = Hasher::hash(p_key);
		return hash;
	}

	static uint32_t _get_resize_count(uint32_t p_capacity) {
		return 15 * (p_capacity >> 4);
	}

	bool _lookup_pos(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos) const {
		if (unlikely(hashes.ptr == nullptr)) {
			return false; // Failed lookups, no elements.
		}
		return _lookup_pos_with_hash(p_key, r_pos, r_hash_pos, _hash(p_key));
	}

	bool _lookup_pos_with_hash(const TKey &p_key, uint32_t &r_pos, uint32_t &r_hash_pos, uint32_t p_hash) const {
		if (unlikely(hashes.ptr == nullptr)) {
			return false; // Failed lookups, no elements.
		}
		bool found = hashes.lookup_pos_with_hash(this, p_key, p_hash, capacity, r_hash_pos);
		if (found) {
			r_pos = hashes_to_key.get_index(r_hash_pos);
		}
		return found;
	}

	uint32_t _insert_with_hash(uint32_t p_hash, uint32_t p_index) {
		uint32_t inserted_position = hashes.insert_hash(p_hash, capacity);
		hashes_to_key.set_index(inserted_position, p_index);
		return inserted_position;
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_new_capacity);
		uint32_t real_capacity = next_power_of_2(capacity);
		capacity = real_capacity - 1;

		uint8_t *old_hashes = hashes.ptr;
		hashes_to_key.initialize(real_capacity, _get_resize_count(capacity) + 1);
		hashes.ptr = reinterpret_cast<uint8_t *>(Memory::alloc_static(sizeof(uint8_t) * real_capacity + HashGroup::GROUP_SIZE));
		if (old_hashes != nullptr) {
			Memory::free_static(old_hashes);
		}
		memset(hashes.ptr, Hashes::EMPTY_HASH, real_capacity * sizeof(uint8_t));
		memset(hashes.ptr + real_capacity, Hashes::END_HASH, HashGroup::GROUP_SIZE * sizeof(uint8_t));

		for (uint32_t i = 0; i < num_elements; i++) {
			uint32_t hash = _hash(keys[i]);
			_insert_with_hash(hash, i);
		}
	}

	void _push_back_key(const TKey &p_key) {
		if (unlikely(num_elements == keys_capacity)) {
			keys_capacity = keys_capacity < 2 ? 2 : Math::ceil(keys_capacity * 1.5f);
			keys = reinterpret_cast<TKey *>(Memory::realloc_static(keys, sizeof(TKey) * (keys_capacity)));
		}
		if constexpr (!std::is_trivially_constructible_v<TKey>) {
			memnew_placement(&keys[num_elements++], TKey(p_key));
		} else {
			keys[num_elements++] = p_key;
		}
	}

	int32_t _insert(const TKey &p_key) {
		if (unlikely(hashes.ptr == nullptr)) {
			// Allocate on demand to save memory.
			_resize_and_rehash(capacity);
		}

		uint32_t pos = 0;
		uint32_t h_pos = 0;
		uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, h_pos, hash);

		if (exists) {
			return pos;
		} else {
			if (unlikely(num_elements > _get_resize_count(capacity))) {
				_resize_and_rehash(capacity * 4);
			}

			_push_back_key(p_key);
			_insert_with_hash(hash, num_elements - 1);
			return num_elements - 1;
		}
	}

	void _init_from(const HashSet &p_other) {
		capacity = p_other.capacity;
		keys_capacity = p_other.keys_capacity;
		uint32_t real_capacity = capacity + 1;
		num_elements = p_other.num_elements;

		if (p_other.num_elements == 0) {
			return;
		}

		hashes_to_key = p_other.hashes_to_key;
		hashes.ptr = reinterpret_cast<uint8_t *>(Memory::alloc_static(sizeof(uint8_t) * real_capacity + HashGroup::GROUP_SIZE));
		keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * keys_capacity));

		if constexpr (std::is_trivially_copyable_v<TKey>) {
			memcpy(keys, p_other.keys, sizeof(TKey) * num_elements);
		} else {
			for (uint32_t i = 0; i < num_elements; i++) {
				memnew_placement(&keys[i], TKey(p_other.keys[i]));
			}
		}

		memcpy(hashes.ptr, p_other.hashes.ptr, sizeof(uint8_t) * real_capacity + HashGroup::GROUP_SIZE);
	}

public:
	uint32_t get_capacity() const { return capacity + 1; }
	uint32_t size() const { return num_elements; }

	/* Standard Godot Container API */

	bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (hashes.ptr == nullptr || num_elements == 0) {
			return;
		}

		memset(hashes.ptr, Hashes::EMPTY_HASH, (capacity + 1) * sizeof(uint8_t));
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

		hashes.delete_hash(pos);
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			keys[key_pos].~TKey();
		}
		num_elements--;
		if (key_pos < num_elements) {
			void *destination = &keys[key_pos];
			const void *source = &keys[num_elements];
			memcpy(destination, source, sizeof(TKey));
			uint32_t h_pos = 0;
			_lookup_pos(keys[num_elements], pos, h_pos);
			hashes_to_key.set_index(h_pos, key_pos);
		}

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once.
	void reserve(uint32_t p_new_capacity) {
		if (keys_capacity < p_new_capacity) {
			keys_capacity = p_new_capacity;
			keys = reinterpret_cast<TKey *>(Memory::realloc_static(keys, sizeof(TKey) * keys_capacity));
		}

		if (get_capacity() < p_new_capacity) {
			if (hashes.ptr == nullptr) {
				capacity = MAX(4u, p_new_capacity);
				capacity = next_power_of_2(capacity) - 1;
				return; // Unallocated yet.
			}
			_resize_and_rehash(p_new_capacity);
		}
	}

	/** Iterator API **/

	struct Iterator {
		const TKey &operator*() const {
			return *key;
		}
		const TKey *operator->() const {
			return key;
		}
		Iterator &operator++() {
			key++;
			return *this;
		}
		Iterator &operator--() {
			key--;
			if (key < begin) {
				key = end;
			}
			return *this;
		}

		bool operator==(const Iterator &b) const { return key == b.key; }
		bool operator!=(const Iterator &b) const { return key != b.key; }

		explicit operator bool() const {
			return key != end;
		}

		Iterator(TKey *p_key, TKey *p_begin, TKey *p_end) {
			key = p_key;
			begin = p_begin;
			end = p_end;
		}
		Iterator() {}
		Iterator(const Iterator &p_it) {
			key = p_it.key;
			begin = p_it.begin;
			end = p_it.end;
		}
		void operator=(const Iterator &p_it) {
			key = p_it.key;
			begin = p_it.begin;
			end = p_it.end;
		}

	private:
		TKey *key = nullptr;
		TKey *begin = nullptr;
		TKey *end = nullptr;
	};

	Iterator begin() const {
		return Iterator(keys, keys, keys + num_elements);
	}
	Iterator end() const {
		return Iterator(keys + num_elements, keys, keys + num_elements);
	}
	Iterator last() const {
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
			Memory::free_static(hashes.ptr);
			hashes_to_key.reset();
			keys = nullptr;
		}
		capacity = INITIAL_CAPACITY - 1;
		keys_capacity = 0;
		num_elements = 0;
	}

	~HashSet() {
		reset();
	}
};

#endif // HASH_SET_H
