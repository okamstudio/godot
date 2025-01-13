/**************************************************************************/
/*  array.cpp                                                             */
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

#include "array.h"

#include "container_type_validate.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/search_array.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

struct ArrayPrivate {
	SafeRefCount refcount;
	Vector<Variant> array;
	Variant *read_only = nullptr; // If enabled, a pointer is used to a temporary value that is used to return read-only values.
	ContainerTypeValidate typed;
};

void Array::_ref(const Array &p_from) const {
	ArrayPrivate *_fp = p_from._p;

	ERR_FAIL_NULL(_fp); // Should NOT happen.

	if (_fp == _p) {
		return; // whatever it is, nothing to do here move along
	}

	bool success = _fp->refcount.ref();

	ERR_FAIL_COND(!success); // should really not happen either

	_unref();

	_p = _fp;
}

void Array::_unref() const {
	if (!_p) {
		return;
	}

	if (_p->refcount.unref()) {
		if (_p->read_only) {
			memdelete(_p->read_only);
		}
		memdelete(_p);
	}
	_p = nullptr;
}

Array::Iterator Array::begin() {
	return Iterator(_p->array.ptrw(), _p->read_only);
}

Array::Iterator Array::end() {
	return Iterator(_p->array.ptrw() + _p->array.size(), _p->read_only);
}

Array::ConstIterator Array::begin() const {
	return ConstIterator(_p->array.ptr(), _p->read_only);
}

Array::ConstIterator Array::end() const {
	return ConstIterator(_p->array.ptr() + _p->array.size(), _p->read_only);
}

Variant &Array::operator[](int p_idx) {
	if (unlikely(_p->read_only)) {
		*_p->read_only = _p->array[p_idx];
		return *_p->read_only;
	}
	return _p->array.write[p_idx];
}

const Variant &Array::operator[](int p_idx) const {
	if (unlikely(_p->read_only)) {
		*_p->read_only = _p->array[p_idx];
		return *_p->read_only;
	}
	return _p->array[p_idx];
}

int Array::size() const {
	return _p->array.size();
}

bool Array::is_empty() const {
	return _p->array.is_empty();
}

void Array::clear() {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	_p->array.clear();
}

bool Array::operator==(const Array &p_array) const {
	return recursive_equal(p_array, 0);
}

bool Array::operator!=(const Array &p_array) const {
	return !recursive_equal(p_array, 0);
}

bool Array::recursive_equal(const Array &p_array, int recursion_count) const {
	// Cheap checks
	if (_p == p_array._p) {
		return true;
	}
	const Vector<Variant> &a1 = _p->array;
	const Vector<Variant> &a2 = p_array._p->array;
	const int size = a1.size();
	if (size != a2.size()) {
		return false;
	}

	// Heavy O(n) check
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return true;
	}
	recursion_count++;
	for (int i = 0; i < size; i++) {
		if (!a1[i].hash_compare(a2[i], recursion_count, false)) {
			return false;
		}
	}

	return true;
}

bool Array::operator<(const Array &p_array) const {
	int a_len = size();
	int b_len = p_array.size();

	int min_cmp = MIN(a_len, b_len);

	for (int i = 0; i < min_cmp; i++) {
		if (operator[](i) < p_array[i]) {
			return true;
		} else if (p_array[i] < operator[](i)) {
			return false;
		}
	}

	return a_len < b_len;
}

bool Array::operator<=(const Array &p_array) const {
	return !operator>(p_array);
}
bool Array::operator>(const Array &p_array) const {
	return p_array < *this;
}
bool Array::operator>=(const Array &p_array) const {
	return !operator<(p_array);
}

uint32_t Array::hash() const {
	return recursive_hash(0);
}

uint32_t Array::recursive_hash(int recursion_count) const {
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return 0;
	}

	uint32_t h = hash_murmur3_one_32(Variant::ARRAY);

	recursion_count++;
	for (int i = 0; i < _p->array.size(); i++) {
		h = hash_murmur3_one_32(_p->array[i].recursive_hash(recursion_count), h);
	}
	return hash_fmix32(h);
}

void Array::operator=(const Array &p_array) {
	if (this == &p_array) {
		return;
	}
	_ref(p_array);
}

void Array::assign(const Array &p_array) {
	const ContainerTypeValidate &typed = _p->typed;
	const ContainerTypeValidate &source_typed = p_array._p->typed;

	if (typed == source_typed || typed.type == Variant::NIL || (source_typed.type == Variant::OBJECT && typed.can_reference(source_typed))) {
		// from same to same or
		// from anything to variants or
		// from subclasses to base classes
		_p->array = p_array._p->array;
		return;
	}

	const Variant *source = p_array._p->array.ptr();
	int size = p_array._p->array.size();

	if ((source_typed.type == Variant::NIL && typed.type == Variant::OBJECT) || (source_typed.type == Variant::OBJECT && source_typed.can_reference(typed))) {
		// from variants to objects or
		// from base classes to subclasses
		for (int i = 0; i < size; i++) {
			const Variant &element = source[i];
			if (element.get_type() != Variant::NIL && (element.get_type() != Variant::OBJECT || !typed.validate_object(element, "assign"))) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert array index %d from "%s" to "%s".)", i, Variant::get_type_name(element.get_type()), Variant::get_type_name(typed.type)));
			}
		}
		_p->array = p_array._p->array;
		return;
	}
	if (typed.type == Variant::OBJECT || source_typed.type == Variant::OBJECT) {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Array[%s]" to "Array[%s]".)", Variant::get_type_name(source_typed.type), Variant::get_type_name(typed.type)));
	}

	Vector<Variant> array;
	array.resize(size);
	Variant *data = array.ptrw();

	if (source_typed.type == Variant::NIL && typed.type != Variant::OBJECT) {
		// from variants to primitives
		for (int i = 0; i < size; i++) {
			const Variant *value = source + i;
			if (value->get_type() == typed.type) {
				data[i] = *value;
				continue;
			}
			if (!Variant::can_convert_strict(value->get_type(), typed.type)) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert array index %d from "%s" to "%s".)", i, Variant::get_type_name(value->get_type()), Variant::get_type_name(typed.type)));
			}
			Callable::CallError ce;
			Variant::construct(typed.type, data[i], &value, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert array index %d from "%s" to "%s".)", i, Variant::get_type_name(value->get_type()), Variant::get_type_name(typed.type)));
		}
	} else if (Variant::can_convert_strict(source_typed.type, typed.type)) {
		// from primitives to different convertible primitives
		for (int i = 0; i < size; i++) {
			const Variant *value = source + i;
			Callable::CallError ce;
			Variant::construct(typed.type, data[i], &value, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert array index %d from "%s" to "%s".)", i, Variant::get_type_name(value->get_type()), Variant::get_type_name(typed.type)));
		}
	} else {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Array[%s]" to "Array[%s]".)", Variant::get_type_name(source_typed.type), Variant::get_type_name(typed.type)));
	}

	_p->array = array;
}

void Array::push_back(const Variant &p_value) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND(!_p->typed.validate(value, "push_back"));
	_p->array.push_back(value);
}

void Array::append_array(const Array &p_array) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");

	Vector<Variant> validated_array = p_array._p->array;
	for (int i = 0; i < validated_array.size(); ++i) {
		ERR_FAIL_COND(!_p->typed.validate(validated_array.write[i], "append_array"));
	}

	_p->array.append_array(validated_array);
}

Error Array::resize(int p_new_size) {
	ERR_FAIL_COND_V_MSG(_p->read_only, ERR_LOCKED, "Array is in read-only state.");
	Variant::Type &variant_type = _p->typed.type;
	int old_size = _p->array.size();
	Error err = _p->array.resize_zeroed(p_new_size);
	if (!err && variant_type != Variant::NIL && variant_type != Variant::OBJECT) {
		for (int i = old_size; i < p_new_size; i++) {
			VariantInternal::initialize(&_p->array.write[i], variant_type);
		}
	}
	return err;
}

Error Array::insert(int p_pos, const Variant &p_value) {
	ERR_FAIL_COND_V_MSG(_p->read_only, ERR_LOCKED, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "insert"), ERR_INVALID_PARAMETER);
	return _p->array.insert(p_pos, value);
}

void Array::fill(const Variant &p_value) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND(!_p->typed.validate(value, "fill"));
	_p->array.fill(value);
}

void Array::erase(const Variant &p_value) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND(!_p->typed.validate(value, "erase"));
	_p->array.erase(value);
}

Variant Array::front() const {
	ERR_FAIL_COND_V_MSG(_p->array.is_empty(), Variant(), "Can't take value from empty array.");
	return operator[](0);
}

Variant Array::back() const {
	ERR_FAIL_COND_V_MSG(_p->array.is_empty(), Variant(), "Can't take value from empty array.");
	return operator[](_p->array.size() - 1);
}

Variant Array::pick_random() const {
	ERR_FAIL_COND_V_MSG(_p->array.is_empty(), Variant(), "Can't take value from empty array.");
	return operator[](Math::rand() % _p->array.size());
}

int Array::find(const Variant &p_value, int p_from) const {
	if (_p->array.size() == 0) {
		return -1;
	}
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "find"), -1);

	int ret = -1;

	if (p_from < 0 || size() == 0) {
		return ret;
	}

	for (int i = p_from; i < size(); i++) {
		if (StringLikeVariantComparator::compare(_p->array[i], value)) {
			ret = i;
			break;
		}
	}

	return ret;
}

int Array::find_custom(const Callable &p_callable, int p_from) const {
	int ret = -1;

	if (p_from < 0 || size() == 0) {
		return ret;
	}

	const Variant *argptrs[1];

	for (int i = p_from; i < size(); i++) {
		const Variant &val = _p->array[i];
		argptrs[0] = &val;
		Variant res;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, res, ce);
		if (unlikely(ce.error != Callable::CallError::CALL_OK)) {
			ERR_FAIL_V_MSG(ret, vformat("Error calling method from 'find_custom': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		ERR_FAIL_COND_V_MSG(res.get_type() != Variant::Type::BOOL, ret, "Error on method from 'find_custom': Return type of callable must be boolean.");
		if (res.operator bool()) {
			return i;
		}
	}

	return ret;
}

int Array::rfind(const Variant &p_value, int p_from) const {
	if (_p->array.size() == 0) {
		return -1;
	}
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "rfind"), -1);

	if (p_from < 0) {
		// Relative offset from the end
		p_from = _p->array.size() + p_from;
	}
	if (p_from < 0 || p_from >= _p->array.size()) {
		// Limit to array boundaries
		p_from = _p->array.size() - 1;
	}

	for (int i = p_from; i >= 0; i--) {
		if (StringLikeVariantComparator::compare(_p->array[i], value)) {
			return i;
		}
	}

	return -1;
}

int Array::rfind_custom(const Callable &p_callable, int p_from) const {
	if (_p->array.size() == 0) {
		return -1;
	}

	if (p_from < 0) {
		// Relative offset from the end.
		p_from = _p->array.size() + p_from;
	}
	if (p_from < 0 || p_from >= _p->array.size()) {
		// Limit to array boundaries.
		p_from = _p->array.size() - 1;
	}

	const Variant *argptrs[1];

	for (int i = p_from; i >= 0; i--) {
		const Variant &val = _p->array[i];
		argptrs[0] = &val;
		Variant res;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, res, ce);
		if (unlikely(ce.error != Callable::CallError::CALL_OK)) {
			ERR_FAIL_V_MSG(-1, vformat("Error calling method from 'rfind_custom': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		ERR_FAIL_COND_V_MSG(res.get_type() != Variant::Type::BOOL, -1, "Error on method from 'rfind_custom': Return type of callable must be boolean.");
		if (res.operator bool()) {
			return i;
		}
	}

	return -1;
}

int Array::count(const Variant &p_value) const {
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "count"), 0);
	if (_p->array.size() == 0) {
		return 0;
	}

	int amount = 0;
	for (int i = 0; i < _p->array.size(); i++) {
		if (StringLikeVariantComparator::compare(_p->array[i], value)) {
			amount++;
		}
	}

	return amount;
}

bool Array::has(const Variant &p_value) const {
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "use 'has'"), false);

	return find(value) != -1;
}

void Array::remove_at(int p_pos) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	_p->array.remove_at(p_pos);
}

void Array::set(int p_idx, const Variant &p_value) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND(!_p->typed.validate(value, "set"));

	operator[](p_idx) = value;
}

const Variant &Array::get(int p_idx) const {
	return operator[](p_idx);
}

Array Array::duplicate(bool p_deep) const {
	return recursive_duplicate(p_deep, 0);
}

Array Array::recursive_duplicate(bool p_deep, int recursion_count) const {
	Array new_arr;
	new_arr._p->typed = _p->typed;

	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return new_arr;
	}

	if (p_deep) {
		recursion_count++;
		int element_count = size();
		new_arr.resize(element_count);
		for (int i = 0; i < element_count; i++) {
			new_arr[i] = get(i).recursive_duplicate(true, recursion_count);
		}
	} else {
		new_arr._p->array = _p->array;
	}

	return new_arr;
}

Array Array::slice(int p_begin, int p_end, int p_step, bool p_deep) const {
	Array result;
	result._p->typed = _p->typed;

	ERR_FAIL_COND_V_MSG(p_step == 0, result, "Slice step cannot be zero.");

	const int s = size();

	if (s == 0 || (p_begin < -s && p_step < 0) || (p_begin >= s && p_step > 0)) {
		return result;
	}

	int begin = CLAMP(p_begin, -s, s - 1);
	if (begin < 0) {
		begin += s;
	}
	int end = CLAMP(p_end, -s - 1, s);
	if (end < 0) {
		end += s;
	}

	ERR_FAIL_COND_V_MSG(p_step > 0 && begin > end, result, "Slice step is positive, but bounds are decreasing.");
	ERR_FAIL_COND_V_MSG(p_step < 0 && begin < end, result, "Slice step is negative, but bounds are increasing.");

	int result_size = (end - begin) / p_step + (((end - begin) % p_step != 0) ? 1 : 0);
	result.resize(result_size);

	for (int src_idx = begin, dest_idx = 0; dest_idx < result_size; ++dest_idx) {
		result[dest_idx] = p_deep ? get(src_idx).duplicate(true) : get(src_idx);
		src_idx += p_step;
	}

	return result;
}

Array Array::filter(const Callable &p_callable) const {
	Array new_arr;
	new_arr.resize(size());
	new_arr._p->typed = _p->typed;
	int accepted_count = 0;

	const Variant *argptrs[1];
	for (int i = 0; i < size(); i++) {
		argptrs[0] = &get(i);

		Variant result;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(Array(), vformat("Error calling method from 'filter': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		if (result.operator bool()) {
			new_arr[accepted_count] = get(i);
			accepted_count++;
		}
	}

	new_arr.resize(accepted_count);

	return new_arr;
}

Array Array::map(const Callable &p_callable) const {
	Array new_arr;
	new_arr.resize(size());

	const Variant *argptrs[1];
	for (int i = 0; i < size(); i++) {
		argptrs[0] = &get(i);

		Variant result;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(Array(), vformat("Error calling method from 'map': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		new_arr[i] = result;
	}

	return new_arr;
}

Variant Array::reduce(const Callable &p_callable, const Variant &p_accum) const {
	int start = 0;
	Variant ret = p_accum;
	if (ret == Variant() && size() > 0) {
		ret = front();
		start = 1;
	}

	const Variant *argptrs[2];
	for (int i = start; i < size(); i++) {
		argptrs[0] = &ret;
		argptrs[1] = &get(i);

		Variant result;
		Callable::CallError ce;
		p_callable.callp(argptrs, 2, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(Variant(), vformat("Error calling method from 'reduce': %s.", Variant::get_callable_error_text(p_callable, argptrs, 2, ce)));
		}
		ret = result;
	}

	return ret;
}

bool Array::any(const Callable &p_callable) const {
	const Variant *argptrs[1];
	for (int i = 0; i < size(); i++) {
		argptrs[0] = &get(i);

		Variant result;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, vformat("Error calling method from 'any': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		if (result.operator bool()) {
			// Return as early as possible when one of the conditions is `true`.
			// This improves performance compared to relying on `filter(...).size() >= 1`.
			return true;
		}
	}

	return false;
}

bool Array::all(const Callable &p_callable) const {
	const Variant *argptrs[1];
	for (int i = 0; i < size(); i++) {
		argptrs[0] = &get(i);

		Variant result;
		Callable::CallError ce;
		p_callable.callp(argptrs, 1, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, vformat("Error calling method from 'all': %s.", Variant::get_callable_error_text(p_callable, argptrs, 1, ce)));
		}

		if (!(result.operator bool())) {
			// Return as early as possible when one of the inverted conditions is `false`.
			// This improves performance compared to relying on `filter(...).size() >= array_size().`.
			return false;
		}
	}

	return true;
}

struct _ArrayVariantSort {
	_FORCE_INLINE_ bool operator()(const Variant &p_l, const Variant &p_r) const {
		bool valid = false;
		Variant res;
		Variant::evaluate(Variant::OP_LESS, p_l, p_r, res, valid);
		if (!valid) {
			res = false;
		}
		return res;
	}
};

void Array::sort() {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	_p->array.sort_custom<_ArrayVariantSort>();
}

void Array::sort_custom(const Callable &p_callable) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	_p->array.sort_custom<CallableComparator, true>(p_callable);
}

void Array::shuffle() {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	const int n = _p->array.size();
	if (n < 2) {
		return;
	}
	Variant *data = _p->array.ptrw();
	for (int i = n - 1; i >= 1; i--) {
		const int j = Math::rand() % (i + 1);
		const Variant tmp = data[j];
		data[j] = data[i];
		data[i] = tmp;
	}
}

int Array::bsearch(const Variant &p_value, bool p_before) const {
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "binary search"), -1);
	SearchArray<Variant, _ArrayVariantSort> avs;
	return avs.bisect(_p->array.ptrw(), _p->array.size(), value, p_before);
}

int Array::bsearch_custom(const Variant &p_value, const Callable &p_callable, bool p_before) const {
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed.validate(value, "custom binary search"), -1);

	return _p->array.bsearch_custom<CallableComparator>(value, p_before, p_callable);
}

void Array::reverse() {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	_p->array.reverse();
}

void Array::push_front(const Variant &p_value) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	Variant value = p_value;
	ERR_FAIL_COND(!_p->typed.validate(value, "push_front"));
	_p->array.insert(0, value);
}

Variant Array::pop_back() {
	ERR_FAIL_COND_V_MSG(_p->read_only, Variant(), "Array is in read-only state.");
	if (!_p->array.is_empty()) {
		const int n = _p->array.size() - 1;
		const Variant ret = _p->array.get(n);
		_p->array.resize(n);
		return ret;
	}
	return Variant();
}

Variant Array::pop_front() {
	ERR_FAIL_COND_V_MSG(_p->read_only, Variant(), "Array is in read-only state.");
	if (!_p->array.is_empty()) {
		const Variant ret = _p->array.get(0);
		_p->array.remove_at(0);
		return ret;
	}
	return Variant();
}

Variant Array::pop_at(int p_pos) {
	ERR_FAIL_COND_V_MSG(_p->read_only, Variant(), "Array is in read-only state.");
	if (_p->array.is_empty()) {
		// Return `null` without printing an error to mimic `pop_back()` and `pop_front()` behavior.
		return Variant();
	}

	if (p_pos < 0) {
		// Relative offset from the end
		p_pos = _p->array.size() + p_pos;
	}

	ERR_FAIL_INDEX_V_MSG(
			p_pos,
			_p->array.size(),
			Variant(),
			vformat(
					"The calculated index %s is out of bounds (the array has %s elements). Leaving the array untouched and returning `null`.",
					p_pos,
					_p->array.size()));

	const Variant ret = _p->array.get(p_pos);
	_p->array.remove_at(p_pos);
	return ret;
}

Variant Array::min() const {
	Variant minval;
	for (int i = 0; i < size(); i++) {
		if (i == 0) {
			minval = get(i);
		} else {
			bool valid;
			Variant ret;
			Variant test = get(i);
			Variant::evaluate(Variant::OP_LESS, test, minval, ret, valid);
			if (!valid) {
				return Variant(); //not a valid comparison
			}
			if (bool(ret)) {
				//is less
				minval = test;
			}
		}
	}
	return minval;
}

Variant Array::max() const {
	Variant maxval;
	for (int i = 0; i < size(); i++) {
		if (i == 0) {
			maxval = get(i);
		} else {
			bool valid;
			Variant ret;
			Variant test = get(i);
			Variant::evaluate(Variant::OP_GREATER, test, maxval, ret, valid);
			if (!valid) {
				return Variant(); //not a valid comparison
			}
			if (bool(ret)) {
				//is greater
				maxval = test;
			}
		}
	}
	return maxval;
}

const void *Array::id() const {
	return _p;
}

Array::Array(const Array &p_from, uint32_t p_type, const StringName &p_class_name, const Variant &p_script) {
	_p = memnew(ArrayPrivate);
	_p->refcount.init();
	set_typed(p_type, p_class_name, p_script);
	assign(p_from);
}

void Array::set_typed(const ContainerType &p_element_type) {
	set_typed(p_element_type.builtin_type, p_element_type.class_name, p_element_type.script);
}

void Array::set_typed(uint32_t p_type, const StringName &p_class_name, const Variant &p_script) {
	ERR_FAIL_COND_MSG(_p->read_only, "Array is in read-only state.");
	ERR_FAIL_COND_MSG(_p->array.size() > 0, "Type can only be set when array is empty.");
	ERR_FAIL_COND_MSG(_p->refcount.get() > 1, "Type can only be set when array has no more than one user.");
	ERR_FAIL_COND_MSG(_p->typed.type != Variant::NIL, "Type can only be set once.");
	ERR_FAIL_COND_MSG(p_class_name != StringName() && p_type != Variant::OBJECT, "Class names can only be set for type OBJECT");
	Ref<Script> script = p_script;
	ERR_FAIL_COND_MSG(script.is_valid() && p_class_name == StringName(), "Script class can only be set together with base class name");

	_p->typed.type = Variant::Type(p_type);
	_p->typed.class_name = p_class_name;
	_p->typed.script = script;
	_p->typed.where = "TypedArray";
}

bool Array::is_typed() const {
	return _p->typed.type != Variant::NIL;
}

bool Array::is_same_typed(const Array &p_other) const {
	return _p->typed == p_other._p->typed;
}

bool Array::is_same_instance(const Array &p_other) const {
	return _p == p_other._p;
}

ContainerType Array::get_element_type() const {
	ContainerType type;
	type.builtin_type = _p->typed.type;
	type.class_name = _p->typed.class_name;
	type.script = _p->typed.script;
	return type;
}

uint32_t Array::get_typed_builtin() const {
	return _p->typed.type;
}

StringName Array::get_typed_class_name() const {
	return _p->typed.class_name;
}

Variant Array::get_typed_script() const {
	return _p->typed.script;
}

Array Array::create_read_only() {
	Array array;
	array.make_read_only();
	return array;
}

void Array::make_read_only() {
	if (_p->read_only == nullptr) {
		_p->read_only = memnew(Variant);
	}
}

bool Array::is_read_only() const {
	return _p->read_only != nullptr;
}

Array::Array(const Array &p_from) {
	_p = nullptr;
	_ref(p_from);
}

Array::Array() {
	_p = memnew(ArrayPrivate);
	_p->refcount.init();
}

Array::~Array() {
	_unref();
}
