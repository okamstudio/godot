/**************************************************************************/
/*  method_bind.cpp                                                       */
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

// object.h needs to be the first include *before* method_bind.h
// FIXME: Find out why and fix potential cyclical dependencies.
#include "core/object/object.h"

#include "method_bind.h"

uint32_t MethodBind::get_hash() const {
	MethodInfo mi;
	mi.return_val = get_return_info();
	mi.flags = get_hint_flags();
	for (int i = 0; i < get_argument_count(); i++) {
		mi.arguments.push_back(get_argument_info(i));
	}
	mi.default_arguments = default_arguments;

	return mi.get_compatibility_hash();
}

PropertyInfo MethodBind::get_argument_info(int p_argument) const {
	ERR_FAIL_INDEX_V(p_argument, get_argument_count(), PropertyInfo());

	PropertyInfo info = _gen_argument_type_info(p_argument);
#ifdef DEBUG_METHODS_ENABLED
	if (info.name.is_empty()) {
		info.name = p_argument < arg_names.size() ? String(arg_names[p_argument]) : String("_unnamed_arg" + itos(p_argument));
	}
#endif
	return info;
}

PropertyInfo MethodBind::get_return_info() const {
	return _gen_argument_type_info(-1);
}

void MethodBind::_set_const(bool p_const) {
	_const = p_const;
}

void MethodBind::_set_static(bool p_static) {
	_static = p_static;
}

void MethodBind::_set_returns(bool p_returns) {
	_returns = p_returns;
}

StringName MethodBind::get_name() const {
	return name;
}

void MethodBind::set_name(const StringName &p_name) {
	name = p_name;
}

#ifdef DEBUG_METHODS_ENABLED
void MethodBind::set_argument_names(const Vector<StringName> &p_names) {
	arg_names = p_names;
}

Vector<StringName> MethodBind::get_argument_names() const {
	return arg_names;
}

#endif

void MethodBind::set_default_arguments(const Vector<Variant> &p_defargs) {
	default_arguments = p_defargs;
	default_argument_count = default_arguments.size();
}

void MethodBind::_generate_argument_types(int p_count) {
	set_argument_count(p_count);

	Variant::Type *argt = memnew_arr(Variant::Type, p_count + 1);
	argt[0] = _gen_argument_type(-1); // return type

	for (int i = 0; i < p_count; i++) {
		argt[i + 1] = _gen_argument_type(i);
	}

	argument_types = argt;
}

MethodBind::MethodBind() {
	static int last_id = 0;
	method_id = last_id++;
}

MethodBind::~MethodBind() {
	if (argument_types) {
		memdelete_arr(argument_types);
	}
}
