/**************************************************************************/
/*  editor_folding.h                                                      */
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

class EditorFolding {
	Vector<String> _get_unfolds(const Object *p_object);
	void _set_unfolds(Object *p_object, const Vector<String> &p_unfolds);

	void _fill_folds(const Node *p_root, const Node *p_node, Array &p_folds, Array &resource_folds, Array &nodes_folded, HashSet<Ref<Resource>> &resources);

	void _do_object_unfolds(Object *p_object, HashSet<Ref<Resource>> &resources);
	void _do_node_unfolds(Node *p_root, Node *p_node, HashSet<Ref<Resource>> &resources);

public:
	void save_resource_folding(const Ref<Resource> &p_resource, const String &p_path);
	void load_resource_folding(Ref<Resource> p_resource, const String &p_path);

	void save_scene_folding(const Node *p_scene, const String &p_path);
	void load_scene_folding(Node *p_scene, const String &p_path);

	void unfold_scene(Node *p_scene);

	bool has_folding_data(const String &p_path);

	EditorFolding();
};
