/**************************************************************************/
/*  editor_debugger_inspector.cpp                                         */
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

#include "editor_debugger_inspector.h"

#include "core/debugger/debugger_marshalls.h"
#include "core/io/marshalls.h"
#include "editor/editor_node.h"
#include "scene/debugger/scene_debugger.h"

bool EditorDebuggerRemoteObject::_set(const StringName &p_name, const Variant &p_value) {
	if (!prop_values.has(p_name) || String(p_name).begins_with("Constants/")) {
		return false;
	}

	prop_values[p_name] = p_value;
	emit_signal(SNAME("value_edited"), remote_object_id, p_name, p_value);
	return true;
}

bool EditorDebuggerRemoteObject::_get(const StringName &p_name, Variant &r_ret) const {
	if (!prop_values.has(p_name)) {
		return false;
	}

	r_ret = prop_values[p_name];
	return true;
}

void EditorDebuggerRemoteObject::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->clear(); // Sorry, no want category.
	for (const PropertyInfo &prop : prop_list) {
		if (prop.name == "script") {
			// Skip the script property, it's always added by the non-virtual method.
			continue;
		}

		p_list->push_back(prop);
	}
}

String EditorDebuggerRemoteObject::get_title() {
	if (remote_object_id.is_valid()) {
		return vformat(TTR("Remote %s:"), String(type_name)) + " " + itos(remote_object_id);
	} else {
		return "<null>";
	}
}

Variant EditorDebuggerRemoteObject::get_variant(const StringName &p_name) {
	Variant var;
	_get(p_name, var);
	return var;
}

void EditorDebuggerRemoteObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_title"), &EditorDebuggerRemoteObject::get_title);
	ClassDB::bind_method(D_METHOD("get_variant"), &EditorDebuggerRemoteObject::get_variant);
	ClassDB::bind_method(D_METHOD("clear"), &EditorDebuggerRemoteObject::clear);
	ClassDB::bind_method(D_METHOD("get_remote_object_id"), &EditorDebuggerRemoteObject::get_remote_object_id);

	ADD_SIGNAL(MethodInfo("value_edited", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "property"), PropertyInfo("value")));
}

EditorDebuggerInspector::EditorDebuggerInspector() {
	variables = memnew(EditorDebuggerRemoteObject);
}

EditorDebuggerInspector::~EditorDebuggerInspector() {
	clear_cache();
	memdelete(variables);
}

void EditorDebuggerInspector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("object_selected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("object_edited", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "property"), PropertyInfo("value")));
	ADD_SIGNAL(MethodInfo("object_property_updated", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "property")));
}

void EditorDebuggerInspector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			connect("object_id_selected", callable_mp(this, &EditorDebuggerInspector::_object_selected));
		} break;

		case NOTIFICATION_ENTER_TREE: {
			edit(variables);
		} break;
	}
}

void EditorDebuggerInspector::_object_edited(ObjectID p_id, const String &p_prop, const Variant &p_value) {
	emit_signal(SNAME("object_edited"), p_id, p_prop, p_value);
}

void EditorDebuggerInspector::_object_selected(ObjectID p_object) {
	emit_signal(SNAME("object_selected"), p_object);
}

ObjectID EditorDebuggerInspector::add_object(const Array &p_arr) {
	EditorDebuggerRemoteObject *debug_obj = nullptr;

	SceneDebuggerObject obj;
	obj.deserialize(p_arr);
	ERR_FAIL_COND_V(obj.id.is_null(), ObjectID());

	if (remote_objects.has(obj.id)) {
		debug_obj = remote_objects[obj.id];
	} else {
		debug_obj = memnew(EditorDebuggerRemoteObject);
		debug_obj->remote_object_id = obj.id;
		debug_obj->type_name = obj.class_name;
		remote_objects[obj.id] = debug_obj;
		debug_obj->connect("value_edited", callable_mp(this, &EditorDebuggerInspector::_object_edited));
	}

	int old_prop_size = debug_obj->prop_list.size();

	debug_obj->prop_list.clear();
	int new_props_added = 0;
	HashSet<String> changed;
	for (SceneDebuggerObject::SceneDebuggerProperty &property : obj.properties) {
		PropertyInfo &pinfo = property.first;
		Variant &var = property.second;

		if (pinfo.type == Variant::OBJECT) {
			if (var.is_string()) {
				String path = var;
				if (path.contains("::")) {
					// built-in resource
					String base_path = path.get_slice("::", 0);
					Ref<Resource> dependency = ResourceLoader::load(base_path);
					if (dependency.is_valid()) {
						remote_dependencies.insert(dependency);
					}
				}
				var = ResourceLoader::load(path);

				if (pinfo.hint_string == "Script") {
					if (debug_obj->get_script() != var) {
						debug_obj->set_script(Ref<RefCounted>());
						Ref<Script> scr(var);
						if (scr.is_valid()) {
							ScriptInstance *scr_instance = scr->placeholder_instance_create(debug_obj);
							if (scr_instance) {
								debug_obj->set_script_and_instance(var, scr_instance);
							}
						}
					}
				}
			}
		}

		//always add the property, since props may have been added or removed
		debug_obj->prop_list.push_back(pinfo);

		if (!debug_obj->prop_values.has(pinfo.name)) {
			new_props_added++;
			debug_obj->prop_values[pinfo.name] = var;
		} else {
			if (bool(Variant::evaluate(Variant::OP_NOT_EQUAL, debug_obj->prop_values[pinfo.name], var))) {
				debug_obj->prop_values[pinfo.name] = var;
				changed.insert(pinfo.name);
			}
		}
	}

	if (old_prop_size == debug_obj->prop_list.size() && new_props_added == 0) {
		//only some may have changed, if so, then update those, if exist
		for (const String &E : changed) {
			emit_signal(SNAME("object_property_updated"), debug_obj->remote_object_id, E);
		}
	} else {
		//full update, because props were added or removed
		debug_obj->update();
	}
	return obj.id;
}

void EditorDebuggerInspector::clear_cache() {
	for (const KeyValue<ObjectID, EditorDebuggerRemoteObject *> &E : remote_objects) {
		EditorNode *editor = EditorNode::get_singleton();
		if (editor->get_editor_selection_history()->get_current() == E.value->get_instance_id()) {
			editor->push_item(nullptr);
		}
		memdelete(E.value);
	}
	remote_objects.clear();
	remote_dependencies.clear();
}

Object *EditorDebuggerInspector::get_object(ObjectID p_id) {
	if (remote_objects.has(p_id)) {
		return remote_objects[p_id];
	}
	return nullptr;
}

void EditorDebuggerInspector::add_stack_variable(const Array &p_array, int p_offset) {
	DebuggerMarshalls::ScriptStackVariable var;
	var.deserialize(p_array);
	String n = var.name;
	Variant v = var.value;

	PropertyHint h = PROPERTY_HINT_NONE;
	String hs;

	if (var.var_type == Variant::OBJECT && v) {
		v = Object::cast_to<EncodedObjectAsID>(v)->get_object_id();
		h = PROPERTY_HINT_OBJECT_ID;
		hs = "Object";
	}
	String type;
	switch (var.type) {
		case 0:
			type = "Locals/";
			break;
		case 1:
			type = "Members/";
			break;
		case 2:
			type = "Globals/";
			break;
		case 3:
			type = "Evaluated/";
			break;
		default:
			type = "Unknown/";
	}

	PropertyInfo pinfo;
	pinfo.name = type + n;
	pinfo.type = v.get_type();
	pinfo.hint = h;
	pinfo.hint_string = hs;

	if ((p_offset == -1) || variables->prop_list.is_empty()) {
		variables->prop_list.push_back(pinfo);
	} else {
		List<PropertyInfo>::Element *current = variables->prop_list.front();
		for (int i = 0; i < p_offset; i++) {
			current = current->next();
		}
		variables->prop_list.insert_before(current, pinfo);
	}
	variables->prop_values[type + n] = v;
	variables->update();
	edit(variables);

	// To prevent constantly resizing when using filtering.
	int size_x = get_size().x;
	if (size_x > get_custom_minimum_size().x) {
		set_custom_minimum_size(Size2(size_x, 0));
	}
}

void EditorDebuggerInspector::clear_stack_variables() {
	variables->clear();
	variables->update();
	set_custom_minimum_size(Size2(0, 0));
}

String EditorDebuggerInspector::get_stack_variable(const String &p_var) {
	for (KeyValue<StringName, Variant> &E : variables->prop_values) {
		String v = E.key.operator String();
		if (v.get_slice("/", 1) == p_var) {
			return variables->get_variant(v);
		}
	}
	return String();
}
