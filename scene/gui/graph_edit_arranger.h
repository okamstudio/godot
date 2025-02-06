/**************************************************************************/
/*  graph_edit_arranger.h                                                 */
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

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

class GraphEdit;

class GraphEditArranger : public RefCounted {
	enum SET_OPERATIONS {
		IS_EQUAL,
		IS_SUBSET,
		DIFFERENCE,
		UNION,
	};

	GraphEdit *graph_edit = nullptr;
	bool arranging_graph = false;

	int _set_operations(SET_OPERATIONS p_operation, HashSet<StringName> &r_u, const HashSet<StringName> &r_v);
	HashMap<int, Vector<StringName>> _layering(const HashSet<StringName> &r_selected_nodes, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours);
	Vector<StringName> _split(const Vector<StringName> &r_layer, const HashMap<StringName, Dictionary> &r_crossings);
	void _horizontal_alignment(Dictionary &r_root, Dictionary &r_align, const HashMap<int, Vector<StringName>> &r_layers, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours, const HashSet<StringName> &r_selected_nodes);
	void _crossing_minimisation(HashMap<int, Vector<StringName>> &r_layers, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours);
	void _calculate_inner_shifts(Dictionary &r_inner_shifts, const Dictionary &r_root, const Dictionary &r_node_names, const Dictionary &r_align, const HashSet<StringName> &r_block_heads, const HashMap<StringName, Pair<int, int>> &r_port_info);
	float _calculate_threshold(const StringName &p_v, const StringName &p_w, const Dictionary &r_node_names, const HashMap<int, Vector<StringName>> &r_layers, const Dictionary &r_root, const Dictionary &r_align, const Dictionary &r_inner_shift, real_t p_current_threshold, const HashMap<StringName, Vector2> &r_node_positions);
	void _place_block(const StringName &p_v, float p_delta, const HashMap<int, Vector<StringName>> &r_layers, const Dictionary &r_root, const Dictionary &r_align, const Dictionary &r_node_name, const Dictionary &r_inner_shift, Dictionary &r_sink, Dictionary &r_shift, HashMap<StringName, Vector2> &r_node_positions);

public:
	void arrange_nodes();

	GraphEditArranger(GraphEdit *p_graph_edit) :
			graph_edit(p_graph_edit) {}
};
