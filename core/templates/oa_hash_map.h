/**************************************************************************/
/*  oa_hash_map.h                                                         */
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

#ifndef OA_HASH_MAP_H
#define OA_HASH_MAP_H

#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/pair.h"

/**
 * A HashMap implementation that uses open addressing with Robin Hood hashing.
 * Robin Hood hashing swaps out entries that have a smaller probing distance
 * than the to-be-inserted entry, that evens out the average probing distance
 * and enables faster lookups. Backward shift deletion is employed to further
 * improve the performance and to avoid infinite loops in rare cases.
 *
 * The entries are stored inplace, so huge keys or values might fill cache lines
 * a lot faster.
 *
 * Only used keys and values are constructed. For free positions there's space
 * in the arrays for each, but that memory is kept uninitialized.
 *
 * The assignment operator copy the pairs from one map to the other.
 */
template <typename TKey, typename TValue,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>>
class OAHashMap {
private:
	TValue *values = nullptr;
	TKey *keys = nullptr;
	uint32_t *hashes = nullptr;

	uint32_t capacity = 0;

	uint32_t num_elements = 0;

	static const uint32_t EMPTY_HASH = 0;

	_FORCE_INLINE_ uint32_t _hash(const TKey &p_key) const {
		uint32_t hash = Hasher::hash(p_key);

		if (hash == EMPTY_HASH) {
			hash = EMPTY_HASH + 1;
		}

		return hash;
	}

	_FORCE_INLINE_ uint32_t _get_probe_length(uint32_t p_pos, uint32_t p_hash) const {
		uint32_t original_pos = p_hash % capacity;
		return (p_pos - original_pos + capacity) % capacity;
	}

	_FORCE_INLINE_ void _construct(uint32_t p_pos, uint32_t p_hash, const TKey &p_key, const TValue &p_value) {
		memnew_placement(&keys[p_pos], TKey(p_key));
		memnew_placement(&values[p_pos], TValue(p_value));
		hashes[p_pos] = p_hash;

		num_elements++;
	}

	bool _lookup_pos(const TKey &p_key, uint32_t &r_pos) const {
		uint32_t hash = _hash(p_key);
		uint32_t pos = hash % capacity;
		uint32_t distance = 0;

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_length(pos, hashes[pos])) {
				return false;
			}

			if (hashes[pos] == hash && Comparator::compare(keys[pos], p_key)) {
				r_pos = pos;
				return true;
			}

			pos = (pos + 1) % capacity;
			distance++;
		}
	}

	void _insert_with_hash(uint32_t p_hash, const TKey &p_key, const TValue &p_value) {
		uint32_t hash = p_hash;
		uint32_t distance = 0;
		uint32_t pos = hash % capacity;

		TKey key = p_key;
		TValue value = p_value;

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				_construct(pos, hash, key, value);

				return;
			}

			// not an empty slot, let's check the probing length of the existing one
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos]);
			if (existing_probe_len < distance) {
				SWAP(hash, hashes[pos]);
				SWAP(key, keys[pos]);
				SWAP(value, values[pos]);
				distance = existing_probe_len;
			}

			pos = (pos + 1) % capacity;
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		uint32_t old_capacity = capacity;

		// Capacity can't be 0.
		capacity = MAX(1u, p_new_capacity);

		TKey *old_keys = keys;
		TValue *old_values = values;
		uint32_t *old_hashes = hashes;

		num_elements = 0;
		keys = static_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * capacity));
		values = static_cast<TValue *>(Memory::alloc_static(sizeof(TValue) * capacity));
		hashes = static_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));

		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = 0;
		}

		if (old_capacity == 0) {
			// Nothing to do.
			return;
		}

		for (uint32_t i = 0; i < old_capacity; i++) {
			if (old_hashes[i] == EMPTY_HASH) {
				continue;
			}

			_insert_with_hash(old_hashes[i], old_keys[i], old_values[i]);

			old_keys[i].~TKey();
			old_values[i].~TValue();
		}

		Memory::free_static(old_keys);
		Memory::free_static(old_values);
		Memory::free_static(old_hashes);
	}

	void _resize_and_rehash() {
		_resize_and_rehash(capacity * 2);
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity; }
	_FORCE_INLINE_ uint32_t get_num_elements() const { return num_elements; }

	bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		for (uint32_t i = 0; i < capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			hashes[i] = EMPTY_HASH;
			values[i].~TValue();
			keys[i].~TKey();
		}

		num_elements = 0;
	}

	void insert(const TKey &p_key, const TValue &p_value) {
		if (num_elements + 1 > 0.9 * capacity) {
			_resize_and_rehash();
		}

		uint32_t hash = _hash(p_key);

		_insert_with_hash(hash, p_key, p_value);
	}

	void set(const TKey &p_key, const TValue &p_data) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			values[pos] = p_data;
		} else {
			insert(p_key, p_data);
		}
	}

	/**
	 * returns true if the value was found, false otherwise.
	 *
	 * if r_data is not nullptr then the value will be written to the object
	 * it points to.
	 */
	bool lookup(const TKey &p_key, TValue &r_data) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			r_data = values[pos];
			return true;
		}

		return false;
	}

	const TValue *lookup_ptr(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &values[pos];
		}
		return nullptr;
	}

	TValue *lookup_ptr(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &values[pos];
		}
		return nullptr;
	}

	_FORCE_INLINE_ bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		return _lookup_pos(p_key, _pos);
	}

	void remove(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (!exists) {
			return;
		}

		uint32_t next_pos = (pos + 1) % capacity;
		while (hashes[next_pos] != EMPTY_HASH &&
				_get_probe_length(next_pos, hashes[next_pos]) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(keys[next_pos], keys[pos]);
			SWAP(values[next_pos], values[pos]);
			pos = next_pos;
			next_pos = (pos + 1) % capacity;
		}

		hashes[pos] = EMPTY_HASH;
		values[pos].~TValue();
		keys[pos].~TKey();

		num_elements--;
	}

	/**
	 * reserves space for a number of elements, useful to avoid many resizes and rehashes
	 *  if adding a known (possibly large) number of elements at once, must be larger than old
	 *  capacity.
	 **/
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND(p_new_capacity < capacity);
		_resize_and_rehash(p_new_capacity);
	}

	struct Iterator {
		bool valid;

		const TKey *key;
		TValue *value = nullptr;

	private:
		uint32_t pos;
		friend class OAHashMap;
	};

	Iterator iter() const {
		Iterator it;

		it.valid = true;
		it.pos = 0;

		return next_iter(it);
	}

	Iterator next_iter(const Iterator &p_iter) const {
		if (!p_iter.valid) {
			return p_iter;
		}

		Iterator it;
		it.valid = false;
		it.pos = p_iter.pos;
		it.key = nullptr;
		it.value = nullptr;

		for (uint32_t i = it.pos; i < capacity; i++) {
			it.pos = i + 1;

			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			it.valid = true;
			it.key = &keys[i];
			it.value = &values[i];
			return it;
		}

		return it;
	}

	OAHashMap(std::initializer_list<KeyValue<TKey, TValue>> p_init) {
		reserve(p_init.size());
		for (const KeyValue<TKey, TValue> &E : p_init) {
			set(E.key, E.value);
		}
	}

	OAHashMap(const OAHashMap &p_other) {
		(*this) = p_other;
	}

	void operator=(const OAHashMap &p_other) {
		if (capacity != 0) {
			clear();
		}

		_resize_and_rehash(p_other.capacity);

		for (Iterator it = p_other.iter(); it.valid; it = p_other.next_iter(it)) {
			set(*it.key, *it.value);
		}
	}

	OAHashMap(uint32_t p_initial_capacity = 64) {
		// Capacity can't be 0.
		capacity = MAX(1u, p_initial_capacity);

		keys = static_cast<TKey *>(Memory::alloc_static(sizeof(TKey) * capacity));
		values = static_cast<TValue *>(Memory::alloc_static(sizeof(TValue) * capacity));
		hashes = static_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));

		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = EMPTY_HASH;
		}
	}

	~OAHashMap() {
		for (uint32_t i = 0; i < capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			values[i].~TValue();
			keys[i].~TKey();
		}

		Memory::free_static(keys);
		Memory::free_static(values);
		Memory::free_static(hashes);
	}
};

#endif // OA_HASH_MAP_H
