/**************************************************************************/
/*  property_list_helper.h                                                */
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

#include "core/object/method_bind.h"
#include "core/object/object.h"

class PropertyListHelper {
	struct Property {
		PropertyInfo info;
		Variant default_value;
		MethodBind *setter = nullptr;
		MethodBind *getter = nullptr;
	};

	static Vector<PropertyListHelper *> base_helpers;

	String prefix;
	MethodBind *array_length_getter = nullptr;
	HashMap<String, Property> property_list;
	Object *object = nullptr;

	const Property *_get_property(const String &p_property, int *r_index) const;
	void _call_setter(const MethodBind *p_setter, int p_index, const Variant &p_value) const;
	Variant _call_getter(const Property *p_property, int p_index) const;
	int _call_array_length_getter() const;

public:
	static void clear_base_helpers();
	static void register_base_helper(PropertyListHelper *p_helper);

	void set_prefix(const String &p_prefix);
	template <typename G>
	void set_array_length_getter(G p_array_length_getter) {
		array_length_getter = create_method_bind(p_array_length_getter);
	}

	// Register property without setter/getter. Only use when you don't need PropertyListHelper for _set/_get logic.
	void register_property(const PropertyInfo &p_info, const Variant &p_default);

	template <typename S, typename G>
	void register_property(const PropertyInfo &p_info, const Variant &p_default, S p_setter, G p_getter) {
		Property property;
		property.info = p_info;
		property.default_value = p_default;
		property.setter = create_method_bind(p_setter);
		property.getter = create_method_bind(p_getter);

		property_list[p_info.name] = property;
	}

	bool is_initialized() const;
	void setup_for_instance(const PropertyListHelper &p_base, Object *p_object);
	bool is_property_valid(const String &p_property, int *r_index = nullptr) const;

	void get_property_list(List<PropertyInfo> *p_list) const;
	bool property_get_value(const String &p_property, Variant &r_ret) const;
	bool property_set_value(const String &p_property, const Variant &p_value) const;
	bool property_can_revert(const String &p_property) const;
	bool property_get_revert(const String &p_property, Variant &r_value) const;

	void clear();
};
