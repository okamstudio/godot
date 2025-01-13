/**************************************************************************/
/*  hash_map.h                                                            */
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

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "core/os/memory.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/pair.h"

#include <initializer_list>

/**
 * A HashMap implementation that uses open addressing with Robin Hood hashing.
 * Robin Hood hashing swaps out entries that have a smaller probing distance
 * than the to-be-inserted entry, that evens out the average probing distance
 * and enables faster lookups. Backward shift deletion is employed to further
 * improve the performance and to avoid infinite loops in rare cases.
 *
 * Keys and values are stored in a double linked list by insertion order. This
 * has a slight performance overhead on lookup, which can be mostly compensated
 * using a paged allocator if required.
 *
 * The assignment operator copy the pairs from one map to the other.
 */

template <typename TKey, typename TValue>
struct HashMapElement {
	HashMapElement *next = nullptr;
	HashMapElement *prev = nullptr;
	KeyValue<TKey, TValue> data;
	HashMapElement() {}
	HashMapElement(const TKey &p_key, const TValue &p_value) :
			data(p_key, p_value) {}
};

bool _hashmap_variant_less_than(const Variant &p_left, const Variant &p_right);

template <typename TKey, typename TValue,
		typename Hasher = HashMapHasherDefault,
		typename Comparator = HashMapComparatorDefault<TKey>,
		typename Allocator = DefaultTypedAllocator<HashMapElement<TKey, TValue>>>
class HashMap {
public:
	static constexpr uint32_t MIN_CAPACITY_INDEX = 2; // Use a prime.
	static constexpr float MAX_OCCUPANCY = 0.75;
	static constexpr uint32_t EMPTY_HASH = 0;

private:
	Allocator element_alloc;
	HashMapElement<TKey, TValue> **elements = nullptr;
	uint32_t *hashes = nullptr;
	HashMapElement<TKey, TValue> *head_element = nullptr;
	HashMapElement<TKey, TValue> *tail_element = nullptr;

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
		if (elements == nullptr || num_elements == 0) {
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

			if (hashes[pos] == hash && Comparator::compare(elements[pos]->data.key, p_key)) {
				r_pos = pos;
				return true;
			}

			pos = fastmod((pos + 1), capacity_inv, capacity);
			distance++;
		}
	}

	void _insert_with_hash(uint32_t p_hash, HashMapElement<TKey, TValue> *p_value) {
		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t hash = p_hash;
		HashMapElement<TKey, TValue> *value = p_value;
		uint32_t distance = 0;
		uint32_t pos = fastmod(hash, capacity_inv, capacity);

		while (true) {
			if (hashes[pos] == EMPTY_HASH) {
				elements[pos] = value;
				hashes[pos] = hash;

				num_elements++;

				return;
			}

			// Not an empty slot, let's check the probing length of the existing one.
			uint32_t existing_probe_len = _get_probe_length(pos, hashes[pos], capacity, capacity_inv);
			if (existing_probe_len < distance) {
				SWAP(hash, hashes[pos]);
				SWAP(value, elements[pos]);
				distance = existing_probe_len;
			}

			pos = fastmod((pos + 1), capacity_inv, capacity);
			distance++;
		}
	}

	void _resize_and_rehash(uint32_t p_new_capacity_index) {
		uint32_t old_capacity = hash_table_size_primes[capacity_index];

		// Capacity can't be 0.
		capacity_index = MAX((uint32_t)MIN_CAPACITY_INDEX, p_new_capacity_index);

		uint32_t capacity = hash_table_size_primes[capacity_index];

		HashMapElement<TKey, TValue> **old_elements = elements;
		uint32_t *old_hashes = hashes;

		num_elements = 0;
		hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
		elements = reinterpret_cast<HashMapElement<TKey, TValue> **>(Memory::alloc_static(sizeof(HashMapElement<TKey, TValue> *) * capacity));

		for (uint32_t i = 0; i < capacity; i++) {
			hashes[i] = 0;
			elements[i] = nullptr;
		}

		if (old_capacity == 0) {
			// Nothing to do.
			return;
		}

		for (uint32_t i = 0; i < old_capacity; i++) {
			if (old_hashes[i] == EMPTY_HASH) {
				continue;
			}

			_insert_with_hash(old_hashes[i], old_elements[i]);
		}

		Memory::free_static(old_elements);
		Memory::free_static(old_hashes);
	}

	_FORCE_INLINE_ HashMapElement<TKey, TValue> *_insert(const TKey &p_key, const TValue &p_value, bool p_front_insert = false) {
		uint32_t capacity = hash_table_size_primes[capacity_index];
		if (unlikely(elements == nullptr)) {
			// Allocate on demand to save memory.

			hashes = reinterpret_cast<uint32_t *>(Memory::alloc_static(sizeof(uint32_t) * capacity));
			elements = reinterpret_cast<HashMapElement<TKey, TValue> **>(Memory::alloc_static(sizeof(HashMapElement<TKey, TValue> *) * capacity));

			for (uint32_t i = 0; i < capacity; i++) {
				hashes[i] = EMPTY_HASH;
				elements[i] = nullptr;
			}
		}

		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			elements[pos]->data.value = p_value;
			return elements[pos];
		} else {
			if (num_elements + 1 > MAX_OCCUPANCY * capacity) {
				ERR_FAIL_COND_V_MSG(capacity_index + 1 == HASH_TABLE_SIZE_MAX, nullptr, "Hash table maximum capacity reached, aborting insertion.");
				_resize_and_rehash(capacity_index + 1);
			}

			HashMapElement<TKey, TValue> *elem = element_alloc.new_allocation(HashMapElement<TKey, TValue>(p_key, p_value));

			if (tail_element == nullptr) {
				head_element = elem;
				tail_element = elem;
			} else if (p_front_insert) {
				head_element->prev = elem;
				elem->next = head_element;
				head_element = elem;
			} else {
				tail_element->next = elem;
				elem->prev = tail_element;
				tail_element = elem;
			}

			uint32_t hash = _hash(p_key);
			_insert_with_hash(hash, elem);
			return elem;
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
		if (elements == nullptr || num_elements == 0) {
			return;
		}
		uint32_t capacity = hash_table_size_primes[capacity_index];
		for (uint32_t i = 0; i < capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}

			hashes[i] = EMPTY_HASH;
			element_alloc.delete_allocation(elements[i]);
			elements[i] = nullptr;
		}

		tail_element = nullptr;
		head_element = nullptr;
		num_elements = 0;
	}

	void sort() {
		if (elements == nullptr || num_elements < 2) {
			return; // An empty or single element HashMap is already sorted.
		}
		// Use insertion sort because we want this operation to be fast for the
		// common case where the input is already sorted or nearly sorted.
		HashMapElement<TKey, TValue> *inserting = head_element->next;
		while (inserting != nullptr) {
			HashMapElement<TKey, TValue> *after = nullptr;
			for (HashMapElement<TKey, TValue> *current = inserting->prev; current != nullptr; current = current->prev) {
				if (_hashmap_variant_less_than(inserting->data.key, current->data.key)) {
					after = current;
				} else {
					break;
				}
			}
			HashMapElement<TKey, TValue> *next = inserting->next;
			if (after != nullptr) {
				// Modify the elements around `inserting` to remove it from its current position.
				inserting->prev->next = next;
				if (next == nullptr) {
					tail_element = inserting->prev;
				} else {
					next->prev = inserting->prev;
				}
				// Modify `before` and `after` to insert `inserting` between them.
				HashMapElement<TKey, TValue> *before = after->prev;
				if (before == nullptr) {
					head_element = inserting;
				} else {
					before->next = inserting;
				}
				after->prev = inserting;
				// Point `inserting` to its new surroundings.
				inserting->prev = before;
				inserting->next = after;
			}
			inserting = next;
		}
	}

	TValue &get(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND_MSG(!exists, "HashMap key not found.");
		return elements[pos]->data.value;
	}

	const TValue &get(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND_MSG(!exists, "HashMap key not found.");
		return elements[pos]->data.value;
	}

	const TValue *getptr(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos]->data.value;
		}
		return nullptr;
	}

	TValue *getptr(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);

		if (exists) {
			return &elements[pos]->data.value;
		}
		return nullptr;
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

		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t next_pos = fastmod((pos + 1), capacity_inv, capacity);
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos], capacity, capacity_inv) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(elements[next_pos], elements[pos]);
			pos = next_pos;
			next_pos = fastmod((pos + 1), capacity_inv, capacity);
		}

		hashes[pos] = EMPTY_HASH;

		if (head_element == elements[pos]) {
			head_element = elements[pos]->next;
		}

		if (tail_element == elements[pos]) {
			tail_element = elements[pos]->prev;
		}

		if (elements[pos]->prev) {
			elements[pos]->prev->next = elements[pos]->next;
		}

		if (elements[pos]->next) {
			elements[pos]->next->prev = elements[pos]->prev;
		}

		element_alloc.delete_allocation(elements[pos]);
		elements[pos] = nullptr;

		num_elements--;
		return true;
	}

	// Replace the key of an entry in-place, without invalidating iterators or changing the entries position during iteration.
	// p_old_key must exist in the map and p_new_key must not, unless it is equal to p_old_key.
	bool replace_key(const TKey &p_old_key, const TKey &p_new_key) {
		if (p_old_key == p_new_key) {
			return true;
		}
		uint32_t pos = 0;
		ERR_FAIL_COND_V(_lookup_pos(p_new_key, pos), false);
		ERR_FAIL_COND_V(!_lookup_pos(p_old_key, pos), false);
		HashMapElement<TKey, TValue> *element = elements[pos];

		// Delete the old entries in hashes and elements.
		const uint32_t capacity = hash_table_size_primes[capacity_index];
		const uint64_t capacity_inv = hash_table_size_primes_inv[capacity_index];
		uint32_t next_pos = fastmod((pos + 1), capacity_inv, capacity);
		while (hashes[next_pos] != EMPTY_HASH && _get_probe_length(next_pos, hashes[next_pos], capacity, capacity_inv) != 0) {
			SWAP(hashes[next_pos], hashes[pos]);
			SWAP(elements[next_pos], elements[pos]);
			pos = next_pos;
			next_pos = fastmod((pos + 1), capacity_inv, capacity);
		}
		hashes[pos] = EMPTY_HASH;
		elements[pos] = nullptr;
		// _insert_with_hash will increment this again.
		num_elements--;

		// Update the HashMapElement with the new key and reinsert it.
		const_cast<TKey &>(element->data.key) = p_new_key;
		uint32_t hash = _hash(p_new_key);
		_insert_with_hash(hash, element);

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

		if (elements == nullptr) {
			capacity_index = new_index;
			return; // Unallocated yet.
		}
		_resize_and_rehash(new_index);
	}

	/** Iterator API **/

	struct ConstIterator {
		_FORCE_INLINE_ const KeyValue<TKey, TValue> &operator*() const {
			return E->data;
		}
		_FORCE_INLINE_ const KeyValue<TKey, TValue> *operator->() const { return &E->data; }
		_FORCE_INLINE_ ConstIterator &operator++() {
			if (E) {
				E = E->next;
			}
			return *this;
		}
		_FORCE_INLINE_ ConstIterator &operator--() {
			if (E) {
				E = E->prev;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const ConstIterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const ConstIterator &b) const { return E != b.E; }

		_FORCE_INLINE_ explicit operator bool() const {
			return E != nullptr;
		}

		_FORCE_INLINE_ ConstIterator(const HashMapElement<TKey, TValue> *p_E) { E = p_E; }
		_FORCE_INLINE_ ConstIterator() {}
		_FORCE_INLINE_ ConstIterator(const ConstIterator &p_it) { E = p_it.E; }
		_FORCE_INLINE_ void operator=(const ConstIterator &p_it) {
			E = p_it.E;
		}

	private:
		const HashMapElement<TKey, TValue> *E = nullptr;
	};

	struct Iterator {
		_FORCE_INLINE_ KeyValue<TKey, TValue> &operator*() const {
			return E->data;
		}
		_FORCE_INLINE_ KeyValue<TKey, TValue> *operator->() const { return &E->data; }
		_FORCE_INLINE_ Iterator &operator++() {
			if (E) {
				E = E->next;
			}
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			if (E) {
				E = E->prev;
			}
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return E != b.E; }

		_FORCE_INLINE_ explicit operator bool() const {
			return E != nullptr;
		}

		_FORCE_INLINE_ Iterator(HashMapElement<TKey, TValue> *p_E) { E = p_E; }
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_it) { E = p_it.E; }
		_FORCE_INLINE_ void operator=(const Iterator &p_it) {
			E = p_it.E;
		}

		operator ConstIterator() const {
			return ConstIterator(E);
		}

	private:
		HashMapElement<TKey, TValue> *E = nullptr;
	};

	_FORCE_INLINE_ Iterator begin() {
		return Iterator(head_element);
	}
	_FORCE_INLINE_ Iterator end() {
		return Iterator(nullptr);
	}
	_FORCE_INLINE_ Iterator last() {
		return Iterator(tail_element);
	}

	_FORCE_INLINE_ Iterator find(const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return Iterator(elements[pos]);
	}

	_FORCE_INLINE_ void remove(const Iterator &p_iter) {
		if (p_iter) {
			erase(p_iter->key);
		}
	}

	_FORCE_INLINE_ ConstIterator begin() const {
		return ConstIterator(head_element);
	}
	_FORCE_INLINE_ ConstIterator end() const {
		return ConstIterator(nullptr);
	}
	_FORCE_INLINE_ ConstIterator last() const {
		return ConstIterator(tail_element);
	}

	_FORCE_INLINE_ ConstIterator find(const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return end();
		}
		return ConstIterator(elements[pos]);
	}

	/* Indexing */

	const TValue &operator[](const TKey &p_key) const {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		CRASH_COND(!exists);
		return elements[pos]->data.value;
	}

	TValue &operator[](const TKey &p_key) {
		uint32_t pos = 0;
		bool exists = _lookup_pos(p_key, pos);
		if (!exists) {
			return _insert(p_key, TValue())->data.value;
		} else {
			return elements[pos]->data.value;
		}
	}

	/* Insert */

	Iterator insert(const TKey &p_key, const TValue &p_value, bool p_front_insert = false) {
		return Iterator(_insert(p_key, p_value, p_front_insert));
	}

	/* Constructors */

	HashMap(const HashMap &p_other) {
		reserve(hash_table_size_primes[p_other.capacity_index]);

		if (p_other.num_elements == 0) {
			return;
		}

		for (const KeyValue<TKey, TValue> &E : p_other) {
			insert(E.key, E.value);
		}
	}

	void operator=(const HashMap &p_other) {
		if (this == &p_other) {
			return; // Ignore self assignment.
		}
		if (num_elements != 0) {
			clear();
		}

		reserve(hash_table_size_primes[p_other.capacity_index]);

		if (p_other.elements == nullptr) {
			return; // Nothing to copy.
		}

		for (const KeyValue<TKey, TValue> &E : p_other) {
			insert(E.key, E.value);
		}
	}

	HashMap(uint32_t p_initial_capacity) {
		// Capacity can't be 0.
		capacity_index = 0;
		reserve(p_initial_capacity);
	}
	HashMap() {
		capacity_index = MIN_CAPACITY_INDEX;
	}

	HashMap(std::initializer_list<KeyValue<TKey, TValue>> p_init) {
		reserve(p_init.size());
		for (const KeyValue<TKey, TValue> &E : p_init) {
			insert(E.key, E.value);
		}
	}

	uint32_t debug_get_hash(uint32_t p_index) {
		if (num_elements == 0) {
			return 0;
		}
		ERR_FAIL_INDEX_V(p_index, get_capacity(), 0);
		return hashes[p_index];
	}
	Iterator debug_get_element(uint32_t p_index) {
		if (num_elements == 0) {
			return Iterator();
		}
		ERR_FAIL_INDEX_V(p_index, get_capacity(), Iterator());
		return Iterator(elements[p_index]);
	}

	~HashMap() {
		clear();

		if (elements != nullptr) {
			Memory::free_static(elements);
			Memory::free_static(hashes);
		}
	}
};

#endif // HASH_MAP_H
