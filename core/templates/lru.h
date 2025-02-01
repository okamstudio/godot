/**************************************************************************/
/*  lru.h                                                                 */
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

#include "core/math/math_funcs.h"
#include "hash_map.h"
#include "list.h"

#if defined(__GNUC__) && !defined(__clang__)
#define ADDRESS_DIAGNOSTIC_WARNING_DISABLE \
	_Pragma("GCC diagnostic push");        \
	_Pragma("GCC diagnostic ignored \"-Waddress\"");

#define ADDRESS_DIAGNOSTIC_POP \
	_Pragma("GCC diagnostic pop");
#else
#define ADDRESS_DIAGNOSTIC_WARNING_DISABLE
#define ADDRESS_DIAGNOSTIC_POP
#endif

template <typename TKey, typename TData, typename Hasher = HashMapHasherDefault, typename Comparator = HashMapComparatorDefault<TKey>, void (*BeforeEvict)(TKey &, TData &) = nullptr>
class LRUCache {
public:
	struct Pair {
		TKey key;
		TData data;

		Pair() {}
		Pair(const TKey &p_key, const TData &p_data) :
				key(p_key),
				data(p_data) {
		}
	};

	typedef typename List<Pair>::Element *Element;

private:
	List<Pair> _list;
	HashMap<TKey, Element, Hasher, Comparator> _map;
	size_t capacity;

public:
	const Pair *insert(const TKey &p_key, const TData &p_value) {
		Element *e = _map.getptr(p_key);
		Element n = _list.push_front(Pair(p_key, p_value));

		if (e) {
			ADDRESS_DIAGNOSTIC_WARNING_DISABLE;
			if constexpr (BeforeEvict != nullptr) {
				BeforeEvict((*e)->get().key, (*e)->get().data);
			}
			ADDRESS_DIAGNOSTIC_POP;
			_list.erase(*e);
			_map.erase(p_key);
		}
		_map[p_key] = _list.front();

		while (_map.size() > capacity) {
			Element d = _list.back();
			ADDRESS_DIAGNOSTIC_WARNING_DISABLE
			if constexpr (BeforeEvict != nullptr) {
				BeforeEvict(d->get().key, d->get().data);
			}
			ADDRESS_DIAGNOSTIC_POP
			_map.erase(d->get().key);
			_list.pop_back();
		}

		return &n->get();
	}

	void clear() {
		_map.clear();
		_list.clear();
	}

	bool has(const TKey &p_key) const {
		return _map.getptr(p_key);
	}

	bool erase(const TKey &p_key) {
		Element *e = _map.getptr(p_key);
		if (!e) {
			return false;
		}
		_list.move_to_front(*e);
		_map.erase(p_key);
		_list.pop_front();
		return true;
	}

	const TData &get(const TKey &p_key) {
		Element *e = _map.getptr(p_key);
		CRASH_COND(!e);
		_list.move_to_front(*e);
		return (*e)->get().data;
	}

	const TData *getptr(const TKey &p_key) {
		Element *e = _map.getptr(p_key);
		if (!e) {
			return nullptr;
		} else {
			_list.move_to_front(*e);
			return &(*e)->get().data;
		}
	}

	_FORCE_INLINE_ size_t get_capacity() const { return capacity; }
	_FORCE_INLINE_ size_t get_size() const { return _map.size(); }

	void set_capacity(size_t p_capacity) {
		if (capacity > 0) {
			capacity = p_capacity;
			while (_map.size() > capacity) {
				Element d = _list.back();
				ADDRESS_DIAGNOSTIC_WARNING_DISABLE;
				if constexpr (BeforeEvict != nullptr) {
					BeforeEvict(d->get().key, d->get().data);
				}
				ADDRESS_DIAGNOSTIC_POP;
				_map.erase(d->get().key);
				_list.pop_back();
			}
		}
	}

	LRUCache() {
		capacity = 64;
	}

	LRUCache(int p_capacity) {
		capacity = p_capacity;
	}
};

#undef ADDRESS_DIAGNOSTIC_WARNING_DISABLE
#undef ADDRESS_DIAGNOSTIC_POP
