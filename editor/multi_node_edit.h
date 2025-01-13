/**************************************************************************/
/*  multi_node_edit.h                                                     */
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

#ifndef MULTI_NODE_EDIT_H
#define MULTI_NODE_EDIT_H

#include "core/object/ref_counted.h"

class MultiNodeEdit : public RefCounted {
	GDCLASS(MultiNodeEdit, RefCounted);

	LocalVector<NodePath> nodes;
	struct PLData {
		int uses = 0;
		PropertyInfo info;
	};

	bool _set_impl(const StringName &p_name, const Variant &p_value, const String &p_field);

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	bool _hide_script_from_inspector() { return true; }
	bool _hide_metadata_from_inspector() { return true; }

	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;
	String _get_editor_name() const;

	void add_node(const NodePath &p_node);

	int get_node_count() const;
	NodePath get_node(int p_index) const;
	StringName get_edited_class_name() const;

	void set_property_field(const StringName &p_property, const Variant &p_value, const String &p_field);

	// If the nodes selected are the same independently of order then return true.
	bool is_same_selection(const MultiNodeEdit *p_other) const {
		if (get_node_count() != p_other->get_node_count()) {
			return false;
		}
		for (int i = 0; i < get_node_count(); i++) {
			if (!nodes.has(p_other->get_node(i))) {
				return false;
			}
		}

		return true;
	}
	MultiNodeEdit();
};

#endif // MULTI_NODE_EDIT_H
