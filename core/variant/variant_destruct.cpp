/**************************************************************************/
/*  variant_destruct.cpp                                                  */
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

#include "variant_destruct.h"

static Variant::PTRDestructor destruct_pointers[Variant::VARIANT_MAX] = { nullptr };

template <typename T>
static void add_destructor() {
	destruct_pointers[T::get_base_type()] = T::ptr_destruct;
}

void Variant::_register_variant_destructors() {
	add_destructor<VariantDestruct<String>>();
	add_destructor<VariantDestruct<StringName>>();
	add_destructor<VariantDestruct<NodePath>>();
	add_destructor<VariantDestruct<Callable>>();
	add_destructor<VariantDestruct<Signal>>();
	add_destructor<VariantDestruct<Dictionary>>();
	add_destructor<VariantDestruct<Array>>();
	add_destructor<VariantDestruct<PackedByteArray>>();
	add_destructor<VariantDestruct<PackedInt32Array>>();
	add_destructor<VariantDestruct<PackedInt64Array>>();
	add_destructor<VariantDestruct<PackedFloat32Array>>();
	add_destructor<VariantDestruct<PackedFloat64Array>>();
	add_destructor<VariantDestruct<PackedStringArray>>();
	add_destructor<VariantDestruct<PackedVector2Array>>();
	add_destructor<VariantDestruct<PackedVector3Array>>();
	add_destructor<VariantDestruct<PackedColorArray>>();
	add_destructor<VariantDestruct<PackedVector4Array>>();
}

void Variant::_unregister_variant_destructors() {
	// Nothing to be done.
}

Variant::PTRDestructor Variant::get_ptr_destructor(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return destruct_pointers[p_type];
}

bool Variant::has_destructor(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	return destruct_pointers[p_type] != nullptr;
}
