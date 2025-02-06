/**************************************************************************/
/*  gltf_accessor.h                                                       */
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

#include "../gltf_defines.h"

#include "core/io/resource.h"

struct GLTFAccessor : public Resource {
	GDCLASS(GLTFAccessor, Resource);
	friend class GLTFDocument;

public:
	enum GLTFAccessorType {
		TYPE_SCALAR,
		TYPE_VEC2,
		TYPE_VEC3,
		TYPE_VEC4,
		TYPE_MAT2,
		TYPE_MAT3,
		TYPE_MAT4,
	};

	enum GLTFComponentType {
		COMPONENT_TYPE_NONE = 0,
		COMPONENT_TYPE_SIGNED_BYTE = 5120,
		COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
		COMPONENT_TYPE_SIGNED_SHORT = 5122,
		COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
		COMPONENT_TYPE_SIGNED_INT = 5124,
		COMPONENT_TYPE_UNSIGNED_INT = 5125,
		COMPONENT_TYPE_SINGLE_FLOAT = 5126,
		COMPONENT_TYPE_DOUBLE_FLOAT = 5130,
		COMPONENT_TYPE_HALF_FLOAT = 5131,
		COMPONENT_TYPE_SIGNED_LONG = 5134,
		COMPONENT_TYPE_UNSIGNED_LONG = 5135,
	};

private:
	GLTFBufferViewIndex buffer_view = -1;
	int byte_offset = 0;
	GLTFComponentType component_type = COMPONENT_TYPE_NONE;
	bool normalized = false;
	int count = 0;
	GLTFAccessorType accessor_type = GLTFAccessorType::TYPE_SCALAR;
	Vector<double> min;
	Vector<double> max;
	int sparse_count = 0;
	int sparse_indices_buffer_view = 0;
	int sparse_indices_byte_offset = 0;
	GLTFComponentType sparse_indices_component_type = COMPONENT_TYPE_NONE;
	int sparse_values_buffer_view = 0;
	int sparse_values_byte_offset = 0;

protected:
	static void _bind_methods();

public:
	GLTFBufferViewIndex get_buffer_view();
	void set_buffer_view(GLTFBufferViewIndex p_buffer_view);

	int get_byte_offset();
	void set_byte_offset(int p_byte_offset);

	int get_component_type();
	void set_component_type(int p_component_type);

	bool get_normalized();
	void set_normalized(bool p_normalized);

	int get_count();
	void set_count(int p_count);

	GLTFAccessorType get_accessor_type();
	void set_accessor_type(GLTFAccessorType p_accessor_type);

	int get_type();
	void set_type(int p_accessor_type);

	Vector<double> get_min();
	void set_min(Vector<double> p_min);

	Vector<double> get_max();
	void set_max(Vector<double> p_max);

	int get_sparse_count();
	void set_sparse_count(int p_sparse_count);

	int get_sparse_indices_buffer_view();
	void set_sparse_indices_buffer_view(int p_sparse_indices_buffer_view);

	int get_sparse_indices_byte_offset();
	void set_sparse_indices_byte_offset(int p_sparse_indices_byte_offset);

	int get_sparse_indices_component_type();
	void set_sparse_indices_component_type(int p_sparse_indices_component_type);

	int get_sparse_values_buffer_view();
	void set_sparse_values_buffer_view(int p_sparse_values_buffer_view);

	int get_sparse_values_byte_offset();
	void set_sparse_values_byte_offset(int p_sparse_values_byte_offset);
};

VARIANT_ENUM_CAST(GLTFAccessor::GLTFAccessorType);
VARIANT_ENUM_CAST(GLTFAccessor::GLTFComponentType);
