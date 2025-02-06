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

template <typename K, typename V>
struct OAHashMapElement {
	K key;
	V value;
};

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
	OAHashMapElement<TKey, TValue> *elements = nullptr;
	uint32_t *hashes = nullptr;

	// Due to optimization, this is `capacity - 1`. Use + 1 to get normal capacity.
	uint32_t capacity = 0;

	uint32_t num_elements = 0;

	static const uint32_t EMPTY_HASH = 0;

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

	_FORCE_INLINE_ void _construct(uint32_t p_pos, uint32_t p_hash, const TKey &p_key, const TValue &p_value) {
		if constexpr (!std::is_trivially_constructible_v<TKey>) {
			memnew_placement(&elements[p_pos].key, TKey(p_key));
		} else {
			TKey key = p_key;
			elements[p_pos].key = key;
		}
		if constexpr (!std::is_trivially_constructible_v<TValue>) {
			memnew_placement(&elements[p_pos].value, TValue(p_value));
		} else {
			TValue value = p_value;
			elements[p_pos].value = value;
		}

		hashes[p_pos] = p_hash;

		num_elements++;
	}

	_FORCE_INLINE_ bool _lookup_pos(const TKey &p_key, uint32_t &r_pos) const {
		return _lookup_pos_with_hash(p_key, r_pos, _hash(p_key));
	}

	_FORCE_INLINE_ bool _lookup_pos_with_hash(const TKey &p_key, uint32_t &r_pos, uint32_t p_hash) const {
		uint32_t pos = p_hash & capacity;

		if (hashes[pos] == p_hash && Comparator::compare(elements[pos].key, p_key)) {
			r_pos = pos;
			return true;
		}

		if (hashes[pos] == EMPTY_HASH) {
			return false;
		}

		// A collision occurred.
		pos = (pos + 1) & capacity;
		uint32_t distance = 1;
		while (true) {
			if (hashes[pos] == p_hash && Comparator::compare(elements[pos].key, p_key)) {
				r_pos = pos;
				return true;
			}

			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_length(pos, hashes[pos], capacity)) {
				return false;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	_FORCE_INLINE_ void _insert_with_swaps(uint32_t p_hash, uint32_t p_pos, uint32_t p_distance, const TKey &p_key, const TValue &p_value) {
		uint32_t hash = p_hash;
		uint32_t distance = p_distance;
		uint32_t pos = p_pos;
		TKey key = p_key;
		TValue value = p_value;
		while (true) {
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos], capacity);
			if (existing_probe_len < distance) {
				SWAP(hash, hashes[pos]);
				SWAP(key, elements[pos].key);
				SWAP(value, elements[pos].value);
				distance = existing_probe_len;
			}

			pos = (pos + 1) & capacity;
			distance++;

			if (hashes[pos] == EMPTY_HASH) {
				_construct(pos, hash, key, value);
				return;
			}
		}
	}

	_FORCE_INLINE_ void _insert_with_hash(uint32_t p_hash, const TKey &p_key, const TValue &p_value) {
		uint32_t pos = p_hash & capacity;

		if (hashes[pos] == EMPTY_HASH) {
			_construct(pos, p_hash, p_key, p_value);
			return;
		}

		// A collision occurred.
		pos = (pos + 1) & capacity;
		uint32_t distance = 1;
		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				_construct(pos, p_hash, p_key, p_value);
				return;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos], capacity);
			if (existing_probe_len < distance) {
				// Now we force to make key-value copy.
				_insert_with_swaps(p_hash, pos, distance, p_key, p_value);
				return;
			}

			pos = (pos + 1) & capacity;
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity) {
		uint32_t old_real_capacity = capacity + 1;

		// Capacity can't be 0 and must be 2^n - 1.
		capacity = MAX(4u, p_new_capacity);
		uint32_t real_capacity = next_power_of_2(capacity);
		capacity = real_capacity - 1;

		OAHashMapElement<TKey, TValue> *old_elements = elements;
		uint32_t *old_hashes = hashes;

		hashes = static_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * real_capacity));
		elements = static_cast<OAHashMapElement<TKey, TValue> *>(Memory::alloc_static(sizeof(OAHashMapElement<TKey, TValue>) * real_capacity));

		memset(hashes, EMPTY_HASH, sizeof(uint32_t) * real_capacity);

		if (old_elements == nullptr) {
			// Nothing to do.
			return;
		}

		if (num_elements != 0) {
			num_elements = 0;

			for (uint32_t i = 0; i < old_real_capacity; i++) {
				if (old_hashes[i] == EMPTY_HASH) {
					continue;
				}

				_insert_with_hash(old_hashes[i], old_elements[i].key, old_elements[i].value);

				if constexpr (!std::is_trivially_destructible_v<TKey>) {
					old_elements[i].key.~TKey();
				}
				if constexpr (!std::is_trivially_destructible_v<TValue>) {
					old_elements[i].value.~TValue();
				}
			}
		}
		Memory::free_static(old_elements);
		Memory::free_static(old_hashes);
	}

	void _resize_and_rehash() {
		_resize_and_rehash(capacity * 2);
	}

	void _clear_elements() {
		uint32_t real_capacity = capacity + 1;
		for (uint32_t i = 0; i < real_capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}
			if constexpr (!std::is_trivially_destructible_v<TValue>) {
				elements[i].value.~TValue();
			}
			if constexpr (!std::is_trivially_destructible_v<TKey>) {
				elements[i].key.~TKey();
			}
		}
	}

	void _reset() {
		if (num_elements != 0) {
			_clear_elements();
		}

		Memory::free_static(hashes);
		Memory::free_static(elements);
	}

public:
	_FORCE_INLINE_ uint32_t get_capacity() const { return capacity + 1; }
	_FORCE_INLINE_ uint32_t get_num_elements() const { return num_elements; }

	_FORCE_INLINE_ bool is_empty() const {
		return num_elements == 0;
	}

	void clear() {
		_clear_elements();
		memset(hashes, EMPTY_HASH, (capacity + 1) * sizeof(uint32_t));
		num_elements = 0;
	}

	void insert(const TKey &p_key, const TValue &p_value) {
		if (unlikely(num_elements > _get_resize_count(capacity))) {
			_resize_and_rehash();
		}

		uint32_t hash = _hash(p_key);

		_insert_with_hash(hash, p_key, p_value);
	}

	void set(const TKey &p_key, const TValue &p_data) {
		uint32_t pos = 0;
		const uint32_t hash = _hash(p_key);
		bool exists = _lookup_pos_with_hash(p_key, pos, hash);

		if (exists) {
			elements[pos].value = p_data;
		} else {
			if (unlikely(num_elements > _get_resize_count(capacity))) {
				_resize_and_rehash();
			}
			_insert_with_hash(hash, p_key, p_data);
		}
	}

	/**
	 * Returns true if the value was found, false otherwise.
	 *
	 * If r_data is not nullptr then the value will be written to the object
	 * it points to.
	 */
	bool lookup(const TKey &p_key, TValue &r_data) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			r_data = elements[pos].value;
			return true;
		}

		return false;
	}

	const TValue *lookup_ptr(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos].value;
		}
		return nullptr;
	}

	TValue *lookup_ptr(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos].value;
		}
		return nullptr;
	}

	bool has(const TKey &p_key) const {
		uint32_t _pos = 0;
		return _lookup_pos(p_key, _pos);
	}

	void remove(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (!exists) {
			return;
		}

		uint32_t next_pos = (pos + 1) & capacity;
		while (hashes[next_pos] != EMPTY_HASH &&
				_get_probe_length(next_pos, hashes[next_pos], capacity) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(elements[next_pos].key, elements[pos].key);
			SWAP(elements[next_pos].value, elements[pos].value);
			pos = next_pos;
			next_pos = (pos + 1) & capacity;
		}

		hashes[pos] = EMPTY_HASH;
		if constexpr (!std::is_trivially_destructible_v<TValue>) {
			elements[pos].value.~TValue();
		}
		if constexpr (!std::is_trivially_destructible_v<TKey>) {
			elements[pos].key.~TKey();
		}

		num_elements--;
	}

	/**
	 * Reserves space for a number of elements, useful to avoid many resizes and rehashes
	 *  if adding a known (possibly large) number of elements at once, must be larger than old
	 *  capacity.
	 **/
	void reserve(uint32_t p_new_capacity) {
		ERR_FAIL_COND_MSG(p_new_capacity < get_capacity(), "It is impossible to reserve less capacity than is currently available.");
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
		uint32_t real_capacity = capacity + 1;
		for (uint32_t i = it.pos; i < real_capacity; i++) {
			it.pos = i + 1;

			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			it.valid = true;
			it.key = &elements[i].key;
			it.value = &elements[i].value;
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
		if (&p_other == this) {
			return;
		}
		if (elements != nullptr) {
			_reset();
		}

		capacity = p_other.capacity;
		num_elements = p_other.num_elements;
		uint32_t real_capacity = capacity + 1;
		hashes = static_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * real_capacity));
		elements = static_cast<OAHashMapElement<TKey, TValue> *>(Memory::alloc_static(sizeof(OAHashMapElement<TKey, TValue>) * real_capacity));

		memcpy(hashes, p_other.hashes, sizeof(uint32_t) * real_capacity);

		if constexpr (std::is_trivially_copyable_v<TKey> && std::is_trivially_copyable_v<TValue>) {
			memcpy(elements, p_other.elements, sizeof(OAHashMapElement<TKey, TValue>) * num_elements);
		} else {
			for (uint32_t i = 0; i < real_capacity; i++) {
				if (hashes[i] == EMPTY_HASH) {
					continue;
				}
				memnew_placement(&elements[i].key, TKey(p_other.elements[i].key));
				memnew_placement(&elements[i].value, TValue(p_other.elements[i].value));
			}
		}
	}

	OAHashMap(uint32_t p_initial_capacity = 32) {
		_resize_and_rehash(p_initial_capacity);
	}

	~OAHashMap() {
		_reset();
	}
};

#endif // OA_HASH_MAP_H
