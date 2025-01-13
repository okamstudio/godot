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

/**
 * Implementation of Set using a bidi indexed hash map.
 * Use RBSet instead of this only if the following conditions are met:
 *
 * - You need to keep an iterator or const pointer to Key and you intend to add/remove elements in the meantime.
 * - Iteration order does matter (via operator<)
 *
 */

template <typename TKey,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>>
class HashSet {
public:
	static constexpr uint32_t MIN_CAPACITY_INDEX = 2; // Use a prime.
	static constexpr float MAX_OCCUPANCY = 0.75;
	static constexpr uint32_t EMPTY_HASH = 0;

private:
	TKey *keys = nullptr;
	uint32_t *hash_to_key = nullptr;
	uint32_t *key_to_hash = nullptr;
	uint32_t *hashes = nullptr;

	uint32_t capacity_index = 0;
	uint32_t num_elements = 0;

	_FORCE_INLINE_ uint32_t _hash(const TKey &p_key) const {
		uint32_t hash = Hasher::hash(p_key);

		if (unlikely(hash == EMPTY_HASH)) {
			hash = EMPTY_HASH + 1;
		}

		return hash;
	}

	static _FORCE_INLINE_ uint32_t _get_probe_length(const uint32_t p_pos, const uint32_t p_hash, const uint32_t p_capacity, const uint64_t p_capacity_inv) {
		const uint32_t original_pos = fastmod(p_hash, p_capacity_inv, p_capacity);
		return fastmod(p_pos - original_pos + p_capacity, p_capacity_inv, p_capacity);
	}

	bool _lookup_pos(const TKey &p_key, uint32_t &r_pos) const {
		if (keys == nullptr || num_elements == 0) {
			return false; // Failed lookups, no elements
		}

		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t hash = _hash(p_key);
		uint32_t pos = fastmod(hash, capacity_inv, capacity);
		uint32_t distance = 0;

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_length(pos, hashes[pos], capacity, capacity_inv)) {
				return false;
			}

			if (hashes[pos] == hash && Comparator::compare(keys[hash_to_key[pos]], p_key)) {
				r_pos = hash_to_key[pos];
				return true;
			}

			pos = fastmod(pos + 1, capacity_inv, capacity);
			distance++;
		}
	}

	uint32_t _insert_with_hash(uint32_t p_hash, uint32_t p_index) {
		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t hash = p_hash;
		uint32_t index = p_index;
		uint32_t distance = 0;
		uint32_t pos = fastmod(hash, capacity_inv, capacity);

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				hashes[pos] = hash;
				key_to_hash[index] = pos;
				hash_to_key[pos] = index;
				return pos;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos], capacity, capacity_inv);
			if (existing_probe_len < distance) {
				key_to_hash[index] = pos;
				SWAP(hash, hashes[pos]);
				SWAP(index, hash_to_key[pos]);
				distance = existing_probe_len;
			}

			pos = fastmod(pos + 1, capacity_inv, capacity);
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity_index) {
		// Capacity can't be 0.
		capacity_index = MAX((uint32_t)MIN_CAPACITY_INDEX, p_new_capacity_index);

		uint32_t capacity = hash_table_size_primes[capacity_index];

		uint32_t *old_hashes = hashes;
		uint32_t *old_key_to_hash = key_to_hash;

		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
		keys = reinterpret_cast<TKey *>(Memory::realloc_static(keys, sizeof(TKey) * capacity));
		key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
		hash_to_key = reinterpret_cast<uint32_t *>(Memory::realloc_static(hash_to_key, sizeof(uint32_t) * capacity));

		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = EMPTY_HASH;
		}

		for (uint32_t i = 0; i < num_elements; i++) {
			uint32_t h = old_hashes[old_key_to_hash[i]];
			_insert_with_hash(h, i);
		}

		Memory::free_static(old_hashes);
		Memory::free_static(old_key_to_hash);
	}

	_FORCE_INLINE_ int32_t _insert(const TKey &p_key) {
		uint32_t capacity = hash_table_size_primes[capacity_index];
		if (unlikely(keys == nullptr)) {
			// Allocate on demand to save memory.

			hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
			keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * capacity));
			key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
			hash_to_key = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));

			for (uint32_t i = 0; i < capacity; i++) {
				hashes[i] = EMPTY_HASH;
			}
		}

		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return pos;
		} else {
			if (num_elements + 1 > MAX_OCCUPANCY * capacity) {
				ERR_FAIL_COND_V_MSG(capacity_index + 1 == HASH_TABLE_SIZE_MAX, -1, "Hash table maximum capacity reached, aborting insertion.");
				_resize_and_rehash(capacity_index + 1);
			}

			uint32_t hash = _hash(p_key);
			memnew_placement(&keys[num_elements], TKey(p_key));
			_insert_with_hash(hash, num_elements);
			num_elements++;
			return num_elements - 1;
		}
	}

	void _init_from(const HashSet &p_other) {
		capacity_index = p_other.capacity_index;
		num_elements = p_other.num_elements;

		if (p_other.num_elements == 0) {
			return;
		}

		uint32_t capacity = hash_table_size_primes[capacity_index];

		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
		keys = reinterpret_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * capacity));
		key_to_hash = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
		hash_to_key = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));

		for (uint32_t i = 0; i < num_elements; i++) {
			memnew_placement(&keys[i], TKey(p_other.keys[i]));
			key_to_hash[i] = p_other.key_to_hash[i];
		}

		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = p_other.hashes[i];
			hash_to_key[i] = p_other.hash_to_key[i];
		}
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return hash_table_size_primes[capacity_index]; }
	_FORCE_INLINE_ uint32_t size() const { return num_elements; }

	/* Standard Godot Container API */

	bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		if (keys == nullptr || num_elements == 0) {
			return;
		}
		uint32_t capacity = hash_table_size_primes[capacity_index];
		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = EMPTY_HASH;
		}
		for (uint32_t i = 0; i < num_elements; i++) {
			keys[i].~TKey();
		}

		num_elements = 0;
	}

	_FORCE_INLINE_ bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		return _lookup_pos(p_key, _pos);
	}

	bool erase(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (!exists) {
			return false;
		}

		uint32_t key_pos = pos;
		pos = key_to_hash[pos]; //make hash pos

		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t next_pos = fastmod(pos + 1, capacity_inv, capacity);
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos], capacity, capacity_inv) != 0) {
			uint32_t kpos = hash_to_key[pos];
			uint32_t kpos_next = hash_to_key[next_pos];
			SWAP(key_to_hash[kpos], key_to_hash[kpos_next]);
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(hash_to_key[next_pos], hash_to_key[pos]);

			pos = next_pos;
			next_pos = fastmod(pos + 1, capacity_inv, capacity);
		}

		hashes[pos] = EMPTY_HASH;
		keys[key_pos].~TKey();
		num_elements--;
		if (key_pos < num_elements) {
			// Not the last key, move the last one here to keep keys lineal
			memnew_placement(&keys[key_pos], TKey(keys[num_elements]));
			keys[num_elements].~TKey();
			key_to_hash[key_pos] = key_to_hash[num_elements];
			hash_to_key[key_to_hash[num_elements]] = key_pos;
		}

		return true;
	}

	// Reserves space for a number of elements, useful to avoid many resizes and rehashes.
	// If adding a known (possibly large) number of elements at once, must be larger than old capacity.
	void reserve(uint32_t p_new_capacity) {
		uint32_t new_index = capacity_index;

		while (hash_table_size_primes[new_index] < p_new_capacity) {
			ERR_FAIL_COND_MSG(new_index + 1 == (uint32_t)HASH_TABLE_SIZE_MAX, nullptr);
			new_index++;
		}

		if (new_index == capacity_index) {
			return;
		}

		if (keys == nullptr) {
			capacity_index = new_index;
			return; // Unallocated yet.
		}
		_resize_and_rehash(new_index);
	}

	/** Iterator API **/

	struct Iterator {
		_FORCE_INLINE_ const TKey &operator*() const {
			return keys[index];
		}
		_FORCE_INLINE_ const TKey *operator->() const {
			return &keys[index];
		}
		_FORCE_INLINE_ Iterator &operator++() {
			index++;
			if (index >= (int32_t)num_keys) {
				index = -1;
				keys = nullptr;
				num_keys = 0;
			}
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			index--;
			if (index < 0) {
				index = -1;
				keys = nullptr;
				num_keys = 0;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return keys == b.keys && index == b.index; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return keys != b.keys || index != b.index; }

		_FORCE_INLINE_ explicit operator bool() const {
			return keys != nullptr;
		}

		_FORCE_INLINE_ Iterator(const TKey *p_keys, uint32_t p_num_keys, int32_t p_index = -1) {
			keys = p_keys;
			num_keys = p_num_keys;
			index = p_index;
		}
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) {
			keys = p_it.keys;
			num_keys = p_it.num_keys;
			index = p_it.index;
		}
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			keys = p_it.keys;
			num_keys = p_it.num_keys;
			index = p_it.index;
		}

	private:
		const TKey *keys = nullptr;
		uint32_t num_keys = 0;
		int32_t index = -1;
	};

	_FORCE_INLINE_ Iterator begin() const {
		return num_elements ? Iterator(keys, num_elements, 0) : Iterator();
	}
	_FORCE_INLINE_ Iterator end() const {
		return Iterator();
	}
	_FORCE_INLINE_ Iterator last() const {
		if (num_elements == 0) {
			return Iterator();
		}
		return Iterator(keys, num_elements, num_elements - 1);
	}

	_FORCE_INLINE_ Iterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return Iterator(keys, num_elements, pos);
	}

	_FORCE_INLINE_ void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(*p_iter);
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key) {
		uint32_t pos = _insert(p_key);
		return Iterator(keys, num_elements, pos);
	}

	/* Constructors */

	HashSet(const HashSet &p_other) {
		_init_from(p_other);
	}

	void operator=(const HashSet &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}

		clear();

		if (keys != nullptr) {
			Memory::free_static(keys);
			Memory::free_static(key_to_hash);
			Memory::free_static(hash_to_key);
			Memory::free_static(hashes);
			keys = nullptr;
			hashes = nullptr;
			hash_to_key = nullptr;
			key_to_hash = nullptr;
		}

		_init_from(p_other);
	}

	HashSet(uint32_t p_initial_capacity) {
		// Capacity can't be 0.
		capacity_index = 0;
		reserve(p_initial_capacity);
	}
	HashSet() {
		capacity_index = MIN_CAPACITY_INDEX;
	}

	HashSet(std::initializer_list<TKey> p_init) {
		reserve(p_init.size());
		for (const TKey &E : p_init) {
			insert(E);
		}
	}

	void reset() {
		clear();

		if (keys != nullptr) {
			Memory::free_static(keys);
			Memory::free_static(key_to_hash);
			Memory::free_static(hash_to_key);
			Memory::free_static(hashes);
			keys = nullptr;
			hashes = nullptr;
			hash_to_key = nullptr;
			key_to_hash = nullptr;
		}
		capacity_index = MIN_CAPACITY_INDEX;
	}

	~HashSet() {
		clear();

		if (keys != nullptr) {
			Memory::free_static(keys);
			Memory::free_static(key_to_hash);
			Memory::free_static(hash_to_key);
			Memory::free_static(hashes);
		}
	}
};

#endif // HASH_SET_H
