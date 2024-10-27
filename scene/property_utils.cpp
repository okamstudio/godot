/**************************************************************************/
/*  property_utils.cpp                                                    */
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

#include "property_utils.h"

#include "core/config/engine.h"
#include "core/object/script_language.h"
#include "core/templates/local_vector.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif // TOOLS_ENABLED

bool PropertyUtils::is_property_value_different(const Object *p_object, const Variant &p_a, const Variant &p_b) {
	if (p_a.get_type() == Variant::FLOAT && p_b.get_type() == Variant::FLOAT) {
		// This must be done because, as some scenes save as text, there might be a tiny difference in floats due to numerical error.
		return !Math::is_equal_approx((float)p_a, (float)p_b);
	} else if (p_a.get_type() == Variant::NODE_PATH && p_b.get_type() == Variant::OBJECT) {
		// With properties of type Node, left side is NodePath, while right side is Node.
		const Node *base_node = Object::cast_to<Node>(p_object);
		const Node *target_node = Object::cast_to<Node>(p_b);
		if (base_node && target_node) {
			return p_a != base_node->get_path_to(target_node);
		}
	}

	if (p_a.get_type() == Variant::ARRAY && p_b.get_type() == Variant::ARRAY) {
		const Node *base_node = Object::cast_to<Node>(p_object);
		Array array1 = p_a;
		Array array2 = p_b;

		if (base_node && !array1.is_empty() && array2.size() == array1.size() && array1[0].get_type() == Variant::NODE_PATH && array2[0].get_type() == Variant::OBJECT) {
			// Like above, but NodePaths/Nodes are inside arrays.
			for (int i = 0; i < array1.size(); i++) {
				const Node *target_node = Object::cast_to<Node>(array2[i]);
				if (!target_node || array1[i] != base_node->get_path_to(target_node)) {
					return true;
				}
			}
			return false;
		}
	}

	// For our purposes, treating null object as NIL is the right thing to do
	const Variant &a = p_a.get_type() == Variant::OBJECT && (Object *)p_a == nullptr ? Variant() : p_a;
	const Variant &b = p_b.get_type() == Variant::OBJECT && (Object *)p_b == nullptr ? Variant() : p_b;
	return a != b;
}

Variant PropertyUtils::get_property_default_value(const Object *p_object, const StringName &p_property, bool *r_is_valid, const Vector<SceneState::PackState> *p_states_stack_cache, bool p_update_exports, const Node *p_owner, bool *r_is_class_default) {
	// This function obeys the way property values are set when an object is instantiated,
	// which is the following (the latter wins):
	// 1. Default value from builtin class
	// 2. Default value from script exported variable (from the topmost script)
	// 3. Value overrides from the instantiation/inheritance stack

	if (r_is_class_default) {
		*r_is_class_default = false;
	}
	if (r_is_valid) {
		*r_is_valid = false;
	}

	// Handle special case "script" property, where the default value is either null or the custom type script.
	// Do this only if there's no states stack cache to trace for default values.
	if (!p_states_stack_cache && p_property == CoreStringName(script) && p_object->has_meta(SceneStringName(_custom_type_script))) {
		Ref<Script> ct_scr = p_object->get_meta(SceneStringName(_custom_type_script));
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return ct_scr;
	}

	Ref<Script> topmost_script;

	if (const Node *node = Object::cast_to<Node>(p_object)) {
		// Check inheritance/instantiation ancestors
		const Vector<SceneState::PackState> &states_stack = p_states_stack_cache ? *p_states_stack_cache : PropertyUtils::get_node_states_stack(node, p_owner);
		for (int i = 0; i < states_stack.size(); ++i) {
			const SceneState::PackState &ia = states_stack[i];
			bool found = false;
			bool node_deferred = false;
			Variant value_in_ancestor = ia.state->get_property_value(ia.node, p_property, found, node_deferred);
			if (found) {
				if (r_is_valid) {
					*r_is_valid = true;
				}
				// Replace properties stored as NodePaths with actual Nodes.
				// Otherwise, the property value would be considered as overridden.
				if (node_deferred) {
					if (value_in_ancestor.get_type() == Variant::ARRAY) {
						Array paths = value_in_ancestor;

						bool valid = false;
						Array array = node->get(p_property, &valid);
						ERR_CONTINUE(!valid);
						array = array.duplicate();

						array.resize(paths.size());
						for (int j = 0; j < array.size(); j++) {
							array.set(j, node->get_node_or_null(paths[j]));
						}
						value_in_ancestor = array;
					} else {
						value_in_ancestor = node->get_node_or_null(value_in_ancestor);
					}
				}
				return value_in_ancestor;
			}
			// Save script for later
			bool has_script = false;
			Variant script = ia.state->get_property_value(ia.node, SNAME("script"), has_script, node_deferred);
			if (has_script) {
				Ref<Script> scr = script;
				if (scr.is_valid()) {
					topmost_script = scr;
				}
			}
		}
	}

	// Let's see what default is set by the topmost script having a default, if any
	if (topmost_script.is_null()) {
		topmost_script = p_object->get_script();
	}
	if (topmost_script.is_valid()) {
		// Should be called in the editor only and not at runtime,
		// otherwise it can cause problems because of missing instance state support
		if (p_update_exports && Engine::get_singleton()->is_editor_hint()) {
			topmost_script->update_exports();
		}
		Variant default_value;
		if (topmost_script->get_property_default_value(p_property, default_value)) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return default_value;
		}
	}

	// Fall back to the default from the native class
	{
		if (r_is_class_default) {
			*r_is_class_default = true;
		}
		bool valid = false;
		Variant value = ClassDB::class_get_default_property_value(p_object->get_class_name(), p_property, &valid);
		if (valid) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return value;
		} else {
			// Heuristically check if this is a synthetic property (whatever/0, whatever/1, etc.)
			// because they are not in the class DB yet must have a default (null).
			String prop_str = String(p_property);
			int p = prop_str.rfind("/");
			if (p != -1 && p < prop_str.length() - 1) {
				bool all_digits = true;
				for (int i = p + 1; i < prop_str.length(); i++) {
					if (!is_digit(prop_str[i])) {
						all_digits = false;
						break;
					}
				}
				if (r_is_valid) {
					*r_is_valid = all_digits;
				}
			}
			return Variant();
		}
	}
}

// Like SceneState::PackState, but using a raw pointer to avoid the cost of
// updating the reference count during the internal work of the functions below
namespace {
struct _FastPackState {
	SceneState *state = nullptr;
	int node = -1;
};
} // namespace

static bool _collect_inheritance_chain(const Ref<SceneState> &p_state, const NodePath &p_path, LocalVector<_FastPackState> &r_states_stack) {
	bool found = false;

	LocalVector<_FastPackState> inheritance_states;

	Ref<SceneState> state = p_state;
	while (state.is_valid()) {
		int node = state->find_node_by_path(p_path);
		if (node >= 0) {
			// This one has state for this node
			inheritance_states.push_back({ state.ptr(), node });
			found = true;
		}
		state = state->get_base_scene_state();
	}

	if (inheritance_states.size() > 0) {
		for (int i = inheritance_states.size() - 1; i >= 0; --i) {
			r_states_stack.push_back(inheritance_states[i]);
		}
	}

	return found;
}

Vector<SceneState::PackState> PropertyUtils::get_node_states_stack(const Node *p_node, const Node *p_owner, bool *r_instantiated_by_owner) {
	if (r_instantiated_by_owner) {
		*r_instantiated_by_owner = true;
	}

	LocalVector<_FastPackState> states_stack;
	{
		const Node *owner = p_owner;
#ifdef TOOLS_ENABLED
		if (!p_owner && Engine::get_singleton()->is_editor_hint()) {
			owner = EditorNode::get_singleton()->get_edited_scene();
		}
#endif

		const Node *n = p_node;
		while (n) {
			if (n == owner) {
				const Ref<SceneState> &state = n->get_scene_inherited_state();
				if (_collect_inheritance_chain(state, n->get_path_to(p_node), states_stack)) {
					if (r_instantiated_by_owner) {
						*r_instantiated_by_owner = false;
					}
				}
				break;
			} else if (!n->get_scene_file_path().is_empty()) {
				const Ref<SceneState> &state = n->get_scene_instance_state();
				_collect_inheritance_chain(state, n->get_path_to(p_node), states_stack);
			}
			n = n->get_owner();
		}
	}

	// Convert to the proper type for returning, inverting the vector on the go
	// (it was more convenient to fill the vector in reverse order)
	Vector<SceneState::PackState> states_stack_ret;
	{
		states_stack_ret.resize(states_stack.size());
		_FastPackState *ps = states_stack.ptr();
		if (states_stack.size() > 0) {
			for (int i = states_stack.size() - 1; i >= 0; --i) {
				states_stack_ret.write[i].state.reference_ptr(ps->state);
				states_stack_ret.write[i].node = ps->node;
				++ps;
			}
		}
	}
	return states_stack_ret;
}
