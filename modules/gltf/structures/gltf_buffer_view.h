/**************************************************************************/
/*  gltf_buffer_view.h                                                    */
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

class GLTFBufferView : public Resource {
	GDCLASS(GLTFBufferView, Resource);
	friend class GLTFDocument;

private:
	GLTFBufferIndex buffer = -1;
	int byte_offset = 0;
	int byte_length = 0;
	int byte_stride = -1;
	bool indices = false;
	bool vertex_attributes = false;

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	GLTFBufferIndex _get_buffer_bind_compat_86907();
	int _get_byte_offset_bind_compat_86907();
	int _get_byte_length_bind_compat_86907();
	int _get_byte_stride_bind_compat_86907();
	bool _get_indices_bind_compat_86907();
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	GLTFBufferIndex get_buffer() const;
	void set_buffer(GLTFBufferIndex p_buffer);

	int get_byte_offset() const;
	void set_byte_offset(int p_byte_offset);

	int get_byte_length() const;
	void set_byte_length(int p_byte_length);

	int get_byte_stride() const;
	void set_byte_stride(int p_byte_stride);

	bool get_indices() const;
	void set_indices(bool p_indices);

	bool get_vertex_attributes() const;
	void set_vertex_attributes(bool p_attributes);

	Vector<uint8_t> load_buffer_view_data(const Ref<GLTFState> p_state) const;
};
