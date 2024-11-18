/**************************************************************************/
/*  editor_debugger_inspector.h                                           */
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

#ifndef EDITOR_DEBUGGER_INSPECTOR_H
#define EDITOR_DEBUGGER_INSPECTOR_H

#include "core/variant/typed_dictionary.h"
#include "editor/editor_inspector.h"

class SceneDebuggerObject;

class EditorDebuggerRemoteObjects : public Object {
	GDCLASS(EditorDebuggerRemoteObjects, Object);

private:
	bool _set_impl(const StringName &p_name, const Variant &p_value, const String &p_field);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	TypedArray<uint64_t> remote_object_ids;
	String type_name;
	List<PropertyInfo> prop_list;
	HashMap<StringName, TypedDictionary<uint64_t, Variant>> prop_values;

	void set_property_field(const StringName &p_property, const Variant &p_value, const String &p_field);
	String get_title();
	Variant get_variant(const StringName &p_name);

	void clear() {
		prop_list.clear();
		prop_values.clear();
	}

	void update() { notify_property_list_changed(); }

	EditorDebuggerRemoteObjects() {}
};

class EditorDebuggerInspector : public EditorInspector {
	GDCLASS(EditorDebuggerInspector, EditorInspector);

private:
	LocalVector<EditorDebuggerRemoteObjects *> remote_objects_list;
	HashSet<Ref<Resource>> remote_dependencies;
	EditorDebuggerRemoteObjects *variables = nullptr;

	void _object_selected(ObjectID p_object);
	void _objects_edited(const String &p_prop, const TypedDictionary<uint64_t, Variant> &p_values, const String &p_field);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	EditorDebuggerInspector();
	~EditorDebuggerInspector();

	// Remote Object cache
	EditorDebuggerRemoteObjects *set_objects(const Array &p_array);
	void clear_remote_inspector();
	void clear_cache();
	void invalidate_selection_from_cache(const TypedArray<uint64_t> &p_ids);

	// Stack Dump variables
	String get_stack_variable(const String &p_var);
	void add_stack_variable(const Array &p_arr, int p_offset = -1);
	void clear_stack_variables();
};

#endif // EDITOR_DEBUGGER_INSPECTOR_H
