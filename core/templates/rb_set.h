/**************************************************************************/
/*  rb_set.h                                                              */
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

#ifndef RB_SET_H
#define RB_SET_H

#include "core/templates/rb_map.h"

// TODO Ideally, we want this to be truly empty, but this appears to be difficult to achieve.
struct RBEmptyValue {};

template <typename T, typename C = Comparator<T>, typename A = DefaultAllocator>
class RBSet : RBMap<T, RBEmptyValue, C, A, true> {
public:
	using Super = RBMap<T, RBEmptyValue, C, A, true>;
	typedef T ValueType;
	using Element = typename Super::Element;
	using ConstIterator = typename Super::ConstIterator;

	// Cannot iterate non-const because mutable access to the key would invalidate the tree.
	_FORCE_INLINE_ ConstIterator begin() const { return Super::begin(); }
	_FORCE_INLINE_ ConstIterator end() const { return Super::end(); }

	const Element *find(const T &p_value) const { return Super::find(p_value); }
	Element *find(const T &p_value) { return Super::find(p_value); }

	const Element *lower_bound(const T &p_value) const { return Super::find_closest(p_value); }
	Element *lower_bound(const T &p_value) { return Super::find_closest(p_value); }
	bool has(const T &p_value) const { return Super::has(p_value); }

	Element *insert(const T &p_value) { return Super::insert(p_value, {}); }
	void erase(Element *p_element) { Super::erase(p_element); }
	bool erase(const T &p_value) { return Super::erase(p_value); }

	Element *front() const { return Super::front(); }
	Element *back() const { return Super::back(); }

	inline bool is_empty() const { return Super::is_empty(); }
	inline int size() const { return Super::size(); }

	int calculate_depth() const { return Super::calculate_depth(); }

	void clear() { Super::clear(); }

	void operator=(const RBSet &p_set) { Super::operator=(p_set); }

	RBSet(const RBSet &p_set) :
			Super(p_set) {}

	RBSet(std::initializer_list<T> p_init) :
			Super(p_init) {}

	_FORCE_INLINE_ RBSet() :
			Super() {}
	~RBSet() {}
};

#endif // RB_SET_H
