/**************************************************************************/
/*  instance_uniforms.cpp                                                 */
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

#include "instance_uniforms.h"

#include "rendering_server_globals.h"

void InstanceUniforms::free(RID p_self) {
	ERR_FAIL_COND(p_self.is_null());

	if (is_allocated()) {
		RSG::material_storage->global_shader_parameters_instance_free(p_self);
		_location = -1;
	}

	_invalidate_items();
}

void InstanceUniforms::materials_start() {
	_invalidate_items();
}

void InstanceUniforms::materials_append(RID p_material) {
	ERR_FAIL_COND(p_material.is_null());

	List<RendererMaterialStorage::InstanceShaderParam> params;
	RSG::material_storage->material_get_instance_shader_parameters(p_material, &params);

	for (const RendererMaterialStorage::InstanceShaderParam &srcp : params) {
		StringName name = srcp.info.name;
		if (Item *ptr = _parameters.getptr(name); ptr) {
			if (!ptr->is_valid()) {
				_init_param(*ptr, srcp);
			} else if (ptr->index != srcp.index) {
				WARN_PRINT("More than one material in instance export the same instance shader uniform '" + srcp.info.name +
						"', but they do it with different indices. Only the first one (in order) will display correctly.");
			} else if (ptr->info.type != srcp.info.type) {
				WARN_PRINT("More than one material in instance export the same instance shader uniform '" + srcp.info.name +
						"', but they do it with different data types. Only the first one (in order) will display correctly.");
			}
		} else {
			Item i;
			_init_param(i, srcp);
			_parameters[name] = i;
		}
	}
}

bool InstanceUniforms::materials_finish(RID p_self) {
	ERR_FAIL_COND_V(p_self.is_null(), false);

	if (_parameters.is_empty()) {
		if (is_allocated()) {
			free(p_self);
			return true;
		}
		return false;
	}

	const bool should_alloc = !is_allocated();

	if (should_alloc) {
		_location = RSG::material_storage->global_shader_parameters_instance_allocate(p_self);
	}

	for (KeyValue<StringName, Item> &kv : _parameters) {
		Item &i = kv.value;
		if (i.is_valid()) {
			RSG::material_storage->global_shader_parameters_instance_update(p_self, i.index, i.value, i.flags);
		}
	}

	return should_alloc;
}

Variant InstanceUniforms::get(const StringName &p_name) const {
	if (const Item *ptr = _parameters.getptr(p_name); ptr) {
		return ptr->value;
	}
	return Variant();
}

void InstanceUniforms::set(RID p_self, const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND(p_self.is_null());
	ERR_FAIL_COND(p_value.get_type() == Variant::OBJECT);

	if (Item *ptr = _parameters.getptr(p_name); ptr) {
		ptr->value = p_value;
		if (ptr->is_valid()) {
			RSG::material_storage->global_shader_parameters_instance_update(p_self, ptr->index, ptr->value, ptr->flags);
		}
	} else {
		Item i; // Initialize in materials_finish.
		i.value = p_value;
		_parameters[p_name] = i;
	}
}

Variant InstanceUniforms::get_default(const StringName &p_name) const {
	if (const Item *ptr = _parameters.getptr(p_name); ptr) {
		return ptr->default_value;
	}
	return Variant();
}

void InstanceUniforms::get_property_list(List<PropertyInfo> &r_parameters) const {
	Vector<StringName> names;

	// Invalid items won't be saved, but will remain in memory in case of shader compilation failure.
	for (const KeyValue<StringName, Item> &kv : _parameters) {
		if (kv.value.is_valid()) {
			names.push_back(kv.key);
		}
	}

	names.sort_custom<StringName::AlphCompare>();

	for (const StringName &n : names) {
		PropertyInfo pinfo = _parameters[n].info;
		r_parameters.push_back(pinfo);
	}
}

void InstanceUniforms::_init_param(Item &r_item, const RendererMaterialStorage::InstanceShaderParam &p_param) const {
	r_item.index = p_param.index;
	r_item.flags = 0;
	r_item.info = p_param.info;
	r_item.default_value = p_param.default_value;

	if (r_item.default_value.get_type() == Variant::NIL) {
		Callable::CallError cerr;
		Variant::construct(r_item.info.type, r_item.default_value, nullptr, 0, cerr);
	}

	if (r_item.value.get_type() == Variant::NIL) {
		r_item.value = r_item.default_value;
	}

	if (r_item.info.hint == PROPERTY_HINT_FLAGS) {
		// HACK: Detect boolean flags count and prevent overhead.
		switch (r_item.info.hint_string.length()) {
			case 3: // "x,y"
				r_item.flags = 1;
				break;
			case 5: // "x,y,z"
				r_item.flags = 2;
				break;
			case 7: // "x,y,z,w"
				r_item.flags = 3;
				break;
		}
	}
}

void InstanceUniforms::_invalidate_items() {
	for (KeyValue<StringName, Item> &kv : _parameters) {
		kv.value.index = -1;
	}
}
