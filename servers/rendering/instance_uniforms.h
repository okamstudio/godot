/**************************************************************************/
/*  instance_uniforms.h                                                   */
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

#include "core/variant/variant.h"
#include "servers/rendering/storage/material_storage.h"

class InstanceUniforms {
public:
	void free(RID p_self);

	void materials_start();
	void materials_append(RID p_material);

	// Assign location() to instance offset if materials_finish returns true.
	bool materials_finish(RID p_self);

	Variant get(const StringName &p_name) const;
	void set(RID p_self, const StringName &p_name, const Variant &p_value);

	Variant get_default(const StringName &p_name) const;
	void get_property_list(List<PropertyInfo> &r_parameters) const;

	inline int32_t location() const { return _location; }
	inline bool is_allocated() const { return _location != -1; }

private:
	struct Item {
		int32_t index = -1;
		int32_t flags = 0;
		Variant value;
		Variant default_value;
		PropertyInfo info;

		inline bool is_valid() const { return index != -1; }
	};
	int32_t _location = -1;
	HashMap<StringName, Item> _parameters;

	void _init_param(Item &r_item, const RendererMaterialStorage::InstanceShaderParam &p_param) const;
	void _invalidate_items();
};
