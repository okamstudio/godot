/**************************************************************************/
/*  array.h                                                               */
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

#ifndef ARRAY_H
#define ARRAY_H

#include "core/typedefs.h"

#include <climits>

class Callable;
class Object;
class StringName;
class Variant;

struct ArrayPrivate;
struct ContainerType;

class Array {
	mutable ArrayPrivate *_p;
	void _unref() const;

public:
	struct ConstIterator {
		_FORCE_INLINE_ const Variant &operator*() const;
		_FORCE_INLINE_ const Variant *operator->() const;

		_FORCE_INLINE_ ConstIterator &operator++();
		_FORCE_INLINE_ ConstIterator &operator--();

		_FORCE_INLINE_ bool operator==(const ConstIterator &p_other) const { return element_ptr == p_other.element_ptr; }
		_FORCE_INLINE_ bool operator!=(const ConstIterator &p_other) const { return element_ptr != p_other.element_ptr; }

		_FORCE_INLINE_ ConstIterator(const Variant *p_element_ptr, Variant *p_read_only = nullptr) :
				element_ptr(p_element_ptr), read_only(p_read_only) {}
		_FORCE_INLINE_ ConstIterator() {}
		_FORCE_INLINE_ ConstIterator(const ConstIterator &p_other) :
				element_ptr(p_other.element_ptr), read_only(p_other.read_only) {}

		_FORCE_INLINE_ ConstIterator &operator=(const ConstIterator &p_other) {
			element_ptr = p_other.element_ptr;
			read_only = p_other.read_only;
			return *this;
		}

	private:
		const Variant *element_ptr = nullptr;
		Variant *read_only = nullptr;
	};

	struct Iterator {
		_FORCE_INLINE_ Variant &operator*() const;
		_FORCE_INLINE_ Variant *operator->() const;

		_FORCE_INLINE_ Iterator &operator++();
		_FORCE_INLINE_ Iterator &operator--();

		_FORCE_INLINE_ bool operator==(const Iterator &p_other) const { return element_ptr == p_other.element_ptr; }
		_FORCE_INLINE_ bool operator!=(const Iterator &p_other) const { return element_ptr != p_other.element_ptr; }

		_FORCE_INLINE_ Iterator(Variant *p_element_ptr, Variant *p_read_only = nullptr) :
				element_ptr(p_element_ptr), read_only(p_read_only) {}
		_FORCE_INLINE_ Iterator() {}
		_FORCE_INLINE_ Iterator(const Iterator &p_other) :
				element_ptr(p_other.element_ptr), read_only(p_other.read_only) {}

		_FORCE_INLINE_ Iterator &operator=(const Iterator &p_other) {
			element_ptr = p_other.element_ptr;
			read_only = p_other.read_only;
			return *this;
		}

		operator ConstIterator() const {
			return ConstIterator(element_ptr, read_only);
		}

	private:
		Variant *element_ptr = nullptr;
		Variant *read_only = nullptr;
	};

	Iterator begin();
	Iterator end();

	ConstIterator begin() const;
	ConstIterator end() const;

	void _ref(const Array &p_from) const;

	Variant &operator[](int p_idx);
	const Variant &operator[](int p_idx) const;

	void set(int p_idx, const Variant &p_value);
	const Variant &get(int p_idx) const;

	int size() const;
	bool is_empty() const;
	void clear();

	bool operator==(const Array &p_array) const;
	bool operator!=(const Array &p_array) const;
	bool recursive_equal(const Array &p_array, int recursion_count) const;

	uint32_t hash() const;
	uint32_t recursive_hash(int recursion_count) const;
	void operator=(const Array &p_array);

	void assign(const Array &p_array);
	void push_back(const Variant &p_value);
	_FORCE_INLINE_ void append(const Variant &p_value) { push_back(p_value); } //for python compatibility
	void append_array(const Array &p_array);
	Error resize(int p_new_size);

	Error insert(int p_pos, const Variant &p_value);
	void remove_at(int p_pos);
	void fill(const Variant &p_value);

	Variant front() const;
	Variant back() const;
	Variant pick_random() const;

	void sort();
	void sort_custom(const Callable &p_callable);
	void shuffle();
	int bsearch(const Variant &p_value, bool p_before = true) const;
	int bsearch_custom(const Variant &p_value, const Callable &p_callable, bool p_before = true) const;
	void reverse();

	int find(const Variant &p_value, int p_from = 0) const;
	int find_custom(const Callable &p_callable, int p_from = 0) const;
	int rfind(const Variant &p_value, int p_from = -1) const;
	int rfind_custom(const Callable &p_callable, int p_from = -1) const;
	int count(const Variant &p_value) const;
	bool has(const Variant &p_value) const;

	void erase(const Variant &p_value);

	void push_front(const Variant &p_value);
	Variant pop_back();
	Variant pop_front();
	Variant pop_at(int p_pos);

	Array duplicate(bool p_deep = false) const;
	Array recursive_duplicate(bool p_deep, int recursion_count) const;

	Array slice(int p_begin, int p_end = INT_MAX, int p_step = 1, bool p_deep = false) const;
	Array filter(const Callable &p_callable) const;
	Array map(const Callable &p_callable) const;
	Variant reduce(const Callable &p_callable, const Variant &p_accum) const;
	bool any(const Callable &p_callable) const;
	bool all(const Callable &p_callable) const;

	bool operator<(const Array &p_array) const;
	bool operator<=(const Array &p_array) const;
	bool operator>(const Array &p_array) const;
	bool operator>=(const Array &p_array) const;

	Variant min() const;
	Variant max() const;

	const void *id() const;

	void set_typed(const ContainerType &p_element_type);
	void set_typed(uint32_t p_type, const StringName &p_class_name, const Variant &p_script);

	bool is_typed() const;
	bool is_same_typed(const Array &p_other) const;
	bool is_same_instance(const Array &p_other) const;

	ContainerType get_element_type() const;
	uint32_t get_typed_builtin() const;
	StringName get_typed_class_name() const;
	Variant get_typed_script() const;

	void make_read_only();
	bool is_read_only() const;
	static Array create_read_only();

	Array(const Array &p_base, uint32_t p_type, const StringName &p_class_name, const Variant &p_script);
	Array(const Array &p_from);
	Array();
	~Array();
};

#endif // ARRAY_H
