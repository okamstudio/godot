/**************************************************************************/
/*  list.h                                                                */
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

#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/templates/sort_array.h"

#include <initializer_list>

/**
 * Generic Templatized Linked List Implementation.
 * The implementation differs from the STL one because
 * a compatible preallocated linked list can be written
 * using the same API, or features such as erasing an element
 * from the iterator.
 */

template <typename T, typename A = DefaultAllocator>
class List {
	struct _Data;

public:
	class Element {
	private:
		friend class List<T, A>;

		T value;
		Element *next_ptr = nullptr;
		Element *prev_ptr = nullptr;
		_Data *data = nullptr;

	public:
		/**
		 * Get NEXT Element iterator, for constant lists.
		 */
		_FORCE_INLINE_ const Element *next() const {
			return next_ptr;
		}
		/**
		 * Get NEXT Element iterator,
		 */
		_FORCE_INLINE_ Element *next() {
			return next_ptr;
		}

		/**
		 * Get PREV Element iterator, for constant lists.
		 */
		_FORCE_INLINE_ const Element *prev() const {
			return prev_ptr;
		}
		/**
		 * Get PREV Element iterator,
		 */
		_FORCE_INLINE_ Element *prev() {
			return prev_ptr;
		}

		/**
		 * * operator, for using as *iterator, when iterators are defined on stack.
		 */
		_FORCE_INLINE_ const T &operator*() const {
			return value;
		}
		/**
		 * operator->, for using as iterator->, when iterators are defined on stack, for constant lists.
		 */
		_FORCE_INLINE_ const T *operator->() const {
			return &value;
		}
		/**
		 * * operator, for using as *iterator, when iterators are defined on stack,
		 */
		_FORCE_INLINE_ T &operator*() {
			return value;
		}
		/**
		 * operator->, for using as iterator->, when iterators are defined on stack, for constant lists.
		 */
		_FORCE_INLINE_ T *operator->() {
			return &value;
		}

		/**
		 * get the value stored in this element.
		 */
		_FORCE_INLINE_ T &get() {
			return value;
		}
		/**
		 * get the value stored in this element, for constant lists
		 */
		_FORCE_INLINE_ const T &get() const {
			return value;
		}
		/**
		 * set the value stored in this element.
		 */
		_FORCE_INLINE_ void set(const T &p_value) {
			value = (T &)p_value;
		}

		void erase() {
			data->erase(this);
		}

		void transfer_to_back(List<T, A> *p_dst_list);

		_FORCE_INLINE_ Element() {}
	};

	typedef T ValueType;

	struct ConstIterator {
		_FORCE_INLINE_ const T &operator*() const {
			return E->get();
		}
		_FORCE_INLINE_ const T *operator->() const { return &E->get(); }
		_FORCE_INLINE_ ConstIterator &operator++() {
			E = E->next();
			return *this;
		}
		_FORCE_INLINE_ ConstIterator &operator--() {
			E = E->prev();
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const ConstIterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const ConstIterator &b) const { return E != b.E; }

		_FORCE_INLINE_ ConstIterator(const Element *p_E) { E = p_E; }
		_FORCE_INLINE_ ConstIterator() {}
		_FORCE_INLINE_ ConstIterator(const ConstIterator &p_it) { E = p_it.E; }

	private:
		const Element *E = nullptr;
	};

	struct Iterator {
		_FORCE_INLINE_ T &operator*() const {
			return E->get();
		}
		_FORCE_INLINE_ T *operator->() const { return &E->get(); }
		_FORCE_INLINE_ Iterator &operator++() {
			E = E->next();
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			E = E->prev();
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &b) const { return E == b.E; }
		_FORCE_INLINE_ bool operator!=(const Iterator &b) const { return E != b.E; }

		Iterator(Element *p_E) { E = p_E; }
		Iterator() {}
		Iterator(const Iterator &p_it) { E = p_it.E; }

		operator ConstIterator() const {
			return ConstIterator(E);
		}

	private:
		Element *E = nullptr;
	};

	_FORCE_INLINE_ Iterator begin() {
		return Iterator(front());
	}
	_FORCE_INLINE_ Iterator end() {
		return Iterator(nullptr);
	}

#if 0
	//to use when replacing find()
	_FORCE_INLINE_ Iterator find(const K &p_key) {
		return Iterator(find(p_key));
	}
#endif
	_FORCE_INLINE_ ConstIterator begin() const {
		return ConstIterator(front());
	}
	_FORCE_INLINE_ ConstIterator end() const {
		return ConstIterator(nullptr);
	}
#if 0
	//to use when replacing find()
	_FORCE_INLINE_ ConstIterator find(const K &p_key) const {
		return ConstIterator(find(p_key));
	}
#endif
private:
	struct _Data {
		Element *first = nullptr;
		Element *last = nullptr;
		int size_cache = 0;

		bool erase(Element *p_I) {
			ERR_FAIL_NULL_V(p_I, false);
			ERR_FAIL_COND_V(p_I->data != this, false);

			if (first == p_I) {
				first = p_I->next_ptr;
			}

			if (last == p_I) {
				last = p_I->prev_ptr;
			}

			if (p_I->prev_ptr) {
				p_I->prev_ptr->next_ptr = p_I->next_ptr;
			}

			if (p_I->next_ptr) {
				p_I->next_ptr->prev_ptr = p_I->prev_ptr;
			}

			memdelete_allocator<Element, A>(p_I);
			size_cache--;

			return true;
		}
	};

	_Data *_data = nullptr;

public:
	/**
	 * return a const iterator to the beginning of the list.
	 */
	_FORCE_INLINE_ const Element *front() const {
		return _data ? _data->first : nullptr;
	}

	/**
	 * return an iterator to the beginning of the list.
	 */
	_FORCE_INLINE_ Element *front() {
		return _data ? _data->first : nullptr;
	}

	/**
	 * return a const iterator to the last member of the list.
	 */
	_FORCE_INLINE_ const Element *back() const {
		return _data ? _data->last : nullptr;
	}

	/**
	 * return an iterator to the last member of the list.
	 */
	_FORCE_INLINE_ Element *back() {
		return _data ? _data->last : nullptr;
	}

	/**
	 * store a new element at the end of the list
	 */
	Element *push_back(const T &value) {
		if (!_data) {
			_data = memnew_allocator(_Data, A);
			_data->first = nullptr;
			_data->last = nullptr;
			_data->size_cache = 0;
		}

		Element *n = memnew_allocator(Element, A);
		n->value = (T &)value;

		n->prev_ptr = _data->last;
		n->next_ptr = nullptr;
		n->data = _data;

		if (_data->last) {
			_data->last->next_ptr = n;
		}

		_data->last = n;

		if (!_data->first) {
			_data->first = n;
		}

		_data->size_cache++;

		return n;
	}

	void pop_back() {
		if (_data && _data->last) {
			erase(_data->last);
		}
	}

	/**
	 * store a new element at the beginning of the list
	 */
	Element *push_front(const T &value) {
		if (!_data) {
			_data = memnew_allocator(_Data, A);
			_data->first = nullptr;
			_data->last = nullptr;
			_data->size_cache = 0;
		}

		Element *n = memnew_allocator(Element, A);
		n->value = (T &)value;
		n->prev_ptr = nullptr;
		n->next_ptr = _data->first;
		n->data = _data;

		if (_data->first) {
			_data->first->prev_ptr = n;
		}

		_data->first = n;

		if (!_data->last) {
			_data->last = n;
		}

		_data->size_cache++;

		return n;
	}

	void pop_front() {
		if (_data && _data->first) {
			erase(_data->first);
		}
	}

	Element *insert_after(Element *p_element, const T &p_value) {
		CRASH_COND(p_element && (!_data || p_element->data != _data));

		if (!p_element) {
			return push_back(p_value);
		}

		Element *n = memnew_allocator(Element, A);
		n->value = (T &)p_value;
		n->prev_ptr = p_element;
		n->next_ptr = p_element->next_ptr;
		n->data = _data;

		if (!p_element->next_ptr) {
			_data->last = n;
		} else {
			p_element->next_ptr->prev_ptr = n;
		}

		p_element->next_ptr = n;

		_data->size_cache++;

		return n;
	}

	Element *insert_before(Element *p_element, const T &p_value) {
		CRASH_COND(p_element && (!_data || p_element->data != _data));

		if (!p_element) {
			return push_back(p_value);
		}

		Element *n = memnew_allocator(Element, A);
		n->value = (T &)p_value;
		n->prev_ptr = p_element->prev_ptr;
		n->next_ptr = p_element;
		n->data = _data;

		if (!p_element->prev_ptr) {
			_data->first = n;
		} else {
			p_element->prev_ptr->next_ptr = n;
		}

		p_element->prev_ptr = n;

		_data->size_cache++;

		return n;
	}

	/**
	 * find an element in the list,
	 */
	template <typename T_v>
	Element *find(const T_v &p_val) {
		Element *it = front();
		while (it) {
			if (it->value == p_val) {
				return it;
			}
			it = it->next();
		}

		return nullptr;
	}

	/**
	 * erase an element in the list, by iterator pointing to it. Return true if it was found/erased.
	 */
	bool erase(Element *p_I) {
		if (_data && p_I) {
			bool ret = _data->erase(p_I);

			if (_data->size_cache == 0) {
				memdelete_allocator<_Data, A>(_data);
				_data = nullptr;
			}

			return ret;
		}

		return false;
	}

	/**
	 * erase the first element in the list, that contains value
	 */
	bool erase(const T &value) {
		Element *I = find(value);
		return erase(I);
	}

	/**
	 * return whether the list is empty
	 */
	_FORCE_INLINE_ bool is_empty() const {
		return (!_data || !_data->size_cache);
	}

	/**
	 * clear the list
	 */
	void clear() {
		while (front()) {
			erase(front());
		}
	}

	_FORCE_INLINE_ int size() const {
		return _data ? _data->size_cache : 0;
	}

	void swap(Element *p_A, Element *p_B) {
		ERR_FAIL_COND(!p_A || !p_B);
		ERR_FAIL_COND(p_A->data != _data);
		ERR_FAIL_COND(p_B->data != _data);

		if (p_A == p_B) {
			return;
		}
		Element *A_prev = p_A->prev_ptr;
		Element *A_next = p_A->next_ptr;
		Element *B_prev = p_B->prev_ptr;
		Element *B_next = p_B->next_ptr;

		if (A_prev) {
			A_prev->next_ptr = p_B;
		} else {
			_data->first = p_B;
		}
		if (B_prev) {
			B_prev->next_ptr = p_A;
		} else {
			_data->first = p_A;
		}
		if (A_next) {
			A_next->prev_ptr = p_B;
		} else {
			_data->last = p_B;
		}
		if (B_next) {
			B_next->prev_ptr = p_A;
		} else {
			_data->last = p_A;
		}
		p_A->prev_ptr = A_next == p_B ? p_B : B_prev;
		p_A->next_ptr = B_next == p_A ? p_B : B_next;
		p_B->prev_ptr = B_next == p_A ? p_A : A_prev;
		p_B->next_ptr = A_next == p_B ? p_A : A_next;
	}
	/**
	 * copy the list
	 */
	void operator=(const List &p_list) {
		clear();
		const Element *it = p_list.front();
		while (it) {
			push_back(it->get());
			it = it->next();
		}
	}
	void operator=(List &&p_list) {
		if (unlikely(this == &p_list)) {
			return;
		}

		clear();
		_data = p_list._data;
		p_list._data = nullptr;
	}

	// Random access to elements, use with care,
	// do not use for iteration.
	T &get(int p_index) {
		CRASH_BAD_INDEX(p_index, size());

		Element *I = front();
		int c = 0;
		while (c < p_index) {
			I = I->next();
			c++;
		}

		return I->get();
	}

	// Random access to elements, use with care,
	// do not use for iteration.
	const T &get(int p_index) const {
		CRASH_BAD_INDEX(p_index, size());

		const Element *I = front();
		int c = 0;
		while (c < p_index) {
			I = I->next();
			c++;
		}

		return I->get();
	}

	void move_to_back(Element *p_I) {
		ERR_FAIL_COND(p_I->data != _data);
		if (!p_I->next_ptr) {
			return;
		}

		if (_data->first == p_I) {
			_data->first = p_I->next_ptr;
		}

		if (_data->last == p_I) {
			_data->last = p_I->prev_ptr;
		}

		if (p_I->prev_ptr) {
			p_I->prev_ptr->next_ptr = p_I->next_ptr;
		}

		p_I->next_ptr->prev_ptr = p_I->prev_ptr;

		_data->last->next_ptr = p_I;
		p_I->prev_ptr = _data->last;
		p_I->next_ptr = nullptr;
		_data->last = p_I;
	}

	void reverse() {
		int s = size() / 2;
		Element *F = front();
		Element *B = back();
		for (int i = 0; i < s; i++) {
			SWAP(F->value, B->value);
			F = F->next();
			B = B->prev();
		}
	}

	void move_to_front(Element *p_I) {
		ERR_FAIL_COND(p_I->data != _data);
		if (!p_I->prev_ptr) {
			return;
		}

		if (_data->first == p_I) {
			_data->first = p_I->next_ptr;
		}

		if (_data->last == p_I) {
			_data->last = p_I->prev_ptr;
		}

		p_I->prev_ptr->next_ptr = p_I->next_ptr;

		if (p_I->next_ptr) {
			p_I->next_ptr->prev_ptr = p_I->prev_ptr;
		}

		_data->first->prev_ptr = p_I;
		p_I->next_ptr = _data->first;
		p_I->prev_ptr = nullptr;
		_data->first = p_I;
	}

	void move_before(Element *value, Element *where) {
		if (value->prev_ptr) {
			value->prev_ptr->next_ptr = value->next_ptr;
		} else {
			_data->first = value->next_ptr;
		}
		if (value->next_ptr) {
			value->next_ptr->prev_ptr = value->prev_ptr;
		} else {
			_data->last = value->prev_ptr;
		}

		value->next_ptr = where;
		if (!where) {
			value->prev_ptr = _data->last;
			_data->last = value;
			return;
		}

		value->prev_ptr = where->prev_ptr;

		if (where->prev_ptr) {
			where->prev_ptr->next_ptr = value;
		} else {
			_data->first = value;
		}

		where->prev_ptr = value;
	}

	/**
	 * simple insertion sort
	 */

	void sort() {
		sort_custom<Comparator<T>>();
	}

	template <typename C>
	void sort_custom_inplace() {
		if (size() < 2) {
			return;
		}

		Element *from = front();
		Element *current = from;
		Element *to = from;

		while (current) {
			Element *next = current->next_ptr;

			if (from != current) {
				current->prev_ptr = nullptr;
				current->next_ptr = from;

				Element *find = from;
				C less;
				while (find && less(find->value, current->value)) {
					current->prev_ptr = find;
					current->next_ptr = find->next_ptr;
					find = find->next_ptr;
				}

				if (current->prev_ptr) {
					current->prev_ptr->next_ptr = current;
				} else {
					from = current;
				}

				if (current->next_ptr) {
					current->next_ptr->prev_ptr = current;
				} else {
					to = current;
				}
			} else {
				current->prev_ptr = nullptr;
				current->next_ptr = nullptr;
			}

			current = next;
		}
		_data->first = from;
		_data->last = to;
	}

	template <typename C>
	struct AuxiliaryComparator {
		C compare;
		_FORCE_INLINE_ bool operator()(const Element *a, const Element *b) const {
			return compare(a->value, b->value);
		}
	};

	template <typename C>
	void sort_custom() {
		//this version uses auxiliary memory for speed.
		//if you don't want to use auxiliary memory, use the in_place version

		int s = size();
		if (s < 2) {
			return;
		}

		Element **aux_buffer = memnew_arr(Element *, s);

		int idx = 0;
		for (Element *E = front(); E; E = E->next_ptr) {
			aux_buffer[idx] = E;
			idx++;
		}

		SortArray<Element *, AuxiliaryComparator<C>> sort;
		sort.sort(aux_buffer, s);

		_data->first = aux_buffer[0];
		aux_buffer[0]->prev_ptr = nullptr;
		aux_buffer[0]->next_ptr = aux_buffer[1];

		_data->last = aux_buffer[s - 1];
		aux_buffer[s - 1]->prev_ptr = aux_buffer[s - 2];
		aux_buffer[s - 1]->next_ptr = nullptr;

		for (int i = 1; i < s - 1; i++) {
			aux_buffer[i]->prev_ptr = aux_buffer[i - 1];
			aux_buffer[i]->next_ptr = aux_buffer[i + 1];
		}

		memdelete_arr(aux_buffer);
	}

	const void *id() const {
		return (void *)_data;
	}

	/**
	 * copy constructor for the list
	 */
	List(const List &p_list) {
		const Element *it = p_list.front();
		while (it) {
			push_back(it->get());
			it = it->next();
		}
	}
	List(List &&p_list) {
		_data = p_list._data;
		p_list._data = nullptr;
	}

	List() {}

	List(std::initializer_list<T> p_init) {
		for (const T &E : p_init) {
			push_back(E);
		}
	}

	~List() {
		clear();
		if (_data) {
			ERR_FAIL_COND(_data->size_cache);
			memdelete_allocator<_Data, A>(_data);
		}
	}
};

template <typename T, typename A>
void List<T, A>::Element::transfer_to_back(List<T, A> *p_dst_list) {
	// Detach from current.

	if (data->first == this) {
		data->first = data->first->next_ptr;
	}
	if (data->last == this) {
		data->last = data->last->prev_ptr;
	}
	if (prev_ptr) {
		prev_ptr->next_ptr = next_ptr;
	}
	if (next_ptr) {
		next_ptr->prev_ptr = prev_ptr;
	}
	data->size_cache--;

	// Attach to the back of the new one.

	if (!p_dst_list->_data) {
		p_dst_list->_data = memnew_allocator(_Data, A);
		p_dst_list->_data->first = this;
		p_dst_list->_data->last = nullptr;
		p_dst_list->_data->size_cache = 0;
		prev_ptr = nullptr;
	} else {
		p_dst_list->_data->last->next_ptr = this;
		prev_ptr = p_dst_list->_data->last;
	}
	p_dst_list->_data->last = this;
	next_ptr = nullptr;

	data = p_dst_list->_data;
	p_dst_list->_data->size_cache++;
}
