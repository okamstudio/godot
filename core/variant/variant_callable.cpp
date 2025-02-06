/**************************************************************************/
/*  variant_callable.cpp                                                  */
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

#include "variant_callable.h"

#include "core/templates/hashfuncs.h"

bool VariantCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	return p_a->hash() == p_b->hash();
}

bool VariantCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	return p_a->hash() < p_b->hash();
}

uint32_t VariantCallable::hash() const {
	return h;
}

String VariantCallable::get_as_text() const {
	return vformat("%s::%s", Variant::get_type_name(variant.get_type()), method);
}

CallableCustom::CompareEqualFunc VariantCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc VariantCallable::get_compare_less_func() const {
	return compare_less;
}

bool VariantCallable::is_valid() const {
	return Variant::has_builtin_method(variant.get_type(), method);
}

StringName VariantCallable::get_method() const {
	return method;
}

ObjectID VariantCallable::get_object() const {
	return ObjectID();
}

int VariantCallable::get_argument_count(bool &r_is_valid) const {
	if (!Variant::has_builtin_method(variant.get_type(), method)) {
		r_is_valid = false;
		return 0;
	}
	r_is_valid = true;
	return Variant::get_builtin_method_argument_count(variant.get_type(), method);
}

void VariantCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, Callable::CallError &r_call_error) const {
	Variant v = variant;
	v.callp(method, p_arguments, p_argcount, r_return_value, r_call_error);
}

VariantCallable::VariantCallable(const Variant &p_variant, const StringName &p_method) {
	variant = p_variant;
	method = p_method;
	h = variant.hash();
	h = hash_murmur3_one_64(Variant::get_builtin_method_hash(variant.get_type(), method), h);
}
