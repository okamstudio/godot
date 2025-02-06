/**************************************************************************/
/*  property_utils.h                                                      */
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

#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

class PropertyUtils {
public:
	static bool is_property_value_different(const Object *p_object, const Variant &p_a, const Variant &p_b);
	// Gets the most pure default value, the one that would be set when the node has just been instantiated
	static Variant get_property_default_value(const Object *p_object, const StringName &p_property, bool *r_is_valid = nullptr, const Vector<SceneState::PackState> *p_states_stack_cache = nullptr, bool p_update_exports = false, const Node *p_owner = nullptr, bool *r_is_class_default = nullptr);

	// Gets the instance/inheritance states of this node, in order of precedence,
	// that is, from the topmost (the most able to override values) to the lowermost
	// (Note that in nested instantiation, the one with the greatest precedence is the furthest
	// in the tree, since every owner found while traversing towards the root gets a chance
	// to override property values.)
	static Vector<SceneState::PackState> get_node_states_stack(const Node *p_node, const Node *p_owner = nullptr, bool *r_instantiated_by_owner = nullptr);
};
