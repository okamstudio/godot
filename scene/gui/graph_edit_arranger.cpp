/**************************************************************************/
/*  graph_edit_arranger.cpp                                               */
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

#include "graph_edit_arranger.h"

#include "scene/gui/graph_edit.h"

void GraphEditArranger::arrange_nodes() {
	ERR_FAIL_NULL(graph_edit);

	if (!arranging_graph) {
		arranging_graph = true;
	} else {
		return;
	}

	Dictionary node_names;
	HashSet<StringName> selected_nodes;

	bool arrange_entire_graph = true;
	for (int i = graph_edit->get_child_count() - 1; i >= 0; i--) {
		GraphNode *graph_element = Object::cast_to<GraphNode>(graph_edit->get_child(i));
		if (!graph_element) {
			continue;
		}

		node_names[graph_element->get_name()] = graph_element;

		if (graph_element->is_selected()) {
			arrange_entire_graph = false;
		}
	}

	HashMap<StringName, HashSet<StringName>> upper_neighbours;
	HashMap<StringName, Pair<int, int>> port_info;
	Vector2 origin(FLT_MAX, FLT_MAX);

	float gap_v = 100.0f;
	float gap_h = 100.0f;

	const Vector<Ref<GraphEdit::Connection>> connection_list = graph_edit->get_connections();

	for (int i = graph_edit->get_child_count() - 1; i >= 0; i--) {
		GraphNode *graph_element = Object::cast_to<GraphNode>(graph_edit->get_child(i));
		if (!graph_element) {
			continue;
		}

		if (graph_element->is_selected() || arrange_entire_graph) {
			selected_nodes.insert(graph_element->get_name());
			HashSet<StringName> s;

			for (const Ref<GraphEdit::Connection> &connection : connection_list) {
				GraphNode *p_from = Object::cast_to<GraphNode>(node_names[connection->from_node]);
				if (!p_from) {
					continue;
				}
				if (connection->to_node == graph_element->get_name() && (p_from->is_selected() || arrange_entire_graph) && connection->to_node != connection->from_node) {
					if (!s.has(p_from->get_name())) {
						s.insert(p_from->get_name());
					}
					String s_connection = String(p_from->get_name()) + " " + String(connection->to_node);
					StringName _connection(s_connection);
					Pair<int, int> ports(connection->from_port, connection->to_port);
					port_info.insert(_connection, ports);
				}
			}
			upper_neighbours.insert(graph_element->get_name(), s);
		}
	}

	if (!selected_nodes.size()) {
		arranging_graph = false;
		return;
	}

	HashMap<int, Vector<StringName>> layers = _layering(selected_nodes, upper_neighbours);
	_crossing_minimisation(layers, upper_neighbours);

	Dictionary root, align, sink, shift;
	_horizontal_alignment(root, align, layers, upper_neighbours, selected_nodes);

	HashMap<StringName, Vector2> new_positions;
	Vector2 default_position(FLT_MAX, FLT_MAX);
	Dictionary inner_shift;
	HashSet<StringName> block_heads;

	for (const StringName &E : selected_nodes) {
		inner_shift[E] = 0.0f;
		sink[E] = E;
		shift[E] = FLT_MAX;
		new_positions.insert(E, default_position);
		if ((StringName)root[E] == E) {
			block_heads.insert(E);
		}
	}

	_calculate_inner_shifts(inner_shift, root, node_names, align, block_heads, port_info);

	for (const StringName &E : block_heads) {
		_place_block(E, gap_v, layers, root, align, node_names, inner_shift, sink, shift, new_positions);
	}
	origin.y = Object::cast_to<GraphNode>(node_names[layers[0][0]])->get_position_offset().y - (new_positions[layers[0][0]].y + (float)inner_shift[layers[0][0]]);
	origin.x = Object::cast_to<GraphNode>(node_names[layers[0][0]])->get_position_offset().x;

	for (const StringName &E : block_heads) {
		StringName u = E;
		float start_from = origin.y + new_positions[E].y;
		do {
			Vector2 cal_pos;
			cal_pos.y = start_from + (real_t)inner_shift[u];
			new_positions.insert(u, cal_pos);
			u = align[u];
		} while (u != E);
	}

	// Compute horizontal coordinates individually for layers to get uniform gap.
	float start_from = origin.x;
	float largest_node_size = 0.0f;

	for (unsigned int i = 0; i < layers.size(); i++) {
		Vector<StringName> layer = layers[i];
		for (int j = 0; j < layer.size(); j++) {
			float current_node_size = Object::cast_to<GraphNode>(node_names[layer[j]])->get_size().x;
			largest_node_size = MAX(largest_node_size, current_node_size);
		}

		for (int j = 0; j < layer.size(); j++) {
			float current_node_size = Object::cast_to<GraphNode>(node_names[layer[j]])->get_size().x;
			Vector2 cal_pos = new_positions[layer[j]];

			if (current_node_size == largest_node_size) {
				cal_pos.x = start_from;
			} else {
				float current_node_start_pos = start_from;
				if (current_node_size < largest_node_size / 2) {
					if (!(i || j)) {
						start_from -= (largest_node_size - current_node_size);
					}
					current_node_start_pos = start_from + largest_node_size - current_node_size;
				}
				cal_pos.x = current_node_start_pos;
			}
			new_positions.insert(layer[j], cal_pos);
		}

		start_from += largest_node_size + gap_h;
		largest_node_size = 0.0f;
	}

	graph_edit->emit_signal(SNAME("begin_node_move"));
	for (const StringName &E : selected_nodes) {
		GraphNode *graph_node = Object::cast_to<GraphNode>(node_names[E]);
		graph_node->set_drag(true);
		Vector2 pos = (new_positions[E]);

		if (graph_edit->is_snapping_enabled()) {
			float snapping_distance = graph_edit->get_snapping_distance();
			pos = pos.snappedf(snapping_distance);
		}
		graph_node->set_position_offset(pos);
		graph_node->set_drag(false);
	}
	graph_edit->emit_signal(SNAME("end_node_move"));
	arranging_graph = false;
}

int GraphEditArranger::_set_operations(SET_OPERATIONS p_operation, HashSet<StringName> &r_u, const HashSet<StringName> &r_v) {
	switch (p_operation) {
		case GraphEditArranger::IS_EQUAL: {
			for (const StringName &E : r_u) {
				if (!r_v.has(E)) {
					return 0;
				}
			}
			return r_u.size() == r_v.size();
		} break;
		case GraphEditArranger::IS_SUBSET: {
			if (r_u.size() == r_v.size() && !r_u.size()) {
				return 1;
			}
			for (const StringName &E : r_u) {
				if (!r_v.has(E)) {
					return 0;
				}
			}
			return 1;
		} break;
		case GraphEditArranger::DIFFERENCE: {
			Vector<StringName> common;
			for (const StringName &E : r_u) {
				if (r_v.has(E)) {
					common.append(E);
				}
			}
			for (const StringName &E : common) {
				r_u.erase(E);
			}
			return r_u.size();
		} break;
		case GraphEditArranger::UNION: {
			for (const StringName &E : r_v) {
				if (!r_u.has(E)) {
					r_u.insert(E);
				}
			}
			return r_u.size();
		} break;
		default:
			break;
	}
	return -1;
}

HashMap<int, Vector<StringName>> GraphEditArranger::_layering(const HashSet<StringName> &r_selected_nodes, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours) {
	HashMap<int, Vector<StringName>> l;

	HashSet<StringName> p = r_selected_nodes, q = r_selected_nodes, u, z;
	int current_layer = 0;
	bool selected = false;

	while (!_set_operations(GraphEditArranger::IS_EQUAL, q, u)) {
		_set_operations(GraphEditArranger::DIFFERENCE, p, u);
		for (const StringName &E : p) {
			HashSet<StringName> n = r_upper_neighbours[E];
			if (_set_operations(GraphEditArranger::IS_SUBSET, n, z)) {
				Vector<StringName> t;
				t.push_back(E);
				if (!l.has(current_layer)) {
					l.insert(current_layer, Vector<StringName>{});
				}
				selected = true;
				t.append_array(l[current_layer]);
				l.insert(current_layer, t);
				u.insert(E);
			}
		}
		if (!selected) {
			current_layer++;
			uint32_t previous_size_z = z.size();
			_set_operations(GraphEditArranger::UNION, z, u);
			if (z.size() == previous_size_z) {
				WARN_PRINT("Graph contains cycle(s). The cycle(s) will not be rearranged accurately.");
				Vector<StringName> t;
				if (l.has(0)) {
					t.append_array(l[0]);
				}
				for (const StringName &E : p) {
					t.push_back(E);
				}
				l.insert(0, t);
				break;
			}
		}
		selected = false;
	}

	return l;
}

Vector<StringName> GraphEditArranger::_split(const Vector<StringName> &r_layer, const HashMap<StringName, Dictionary> &r_crossings) {
	if (!r_layer.size()) {
		return Vector<StringName>();
	}

	const StringName &p = r_layer[Math::random(0, r_layer.size() - 1)];
	Vector<StringName> left;
	Vector<StringName> right;

	for (int i = 0; i < r_layer.size(); i++) {
		if (p != r_layer[i]) {
			const StringName &q = r_layer[i];
			int cross_pq = r_crossings[p][q];
			int cross_qp = r_crossings[q][p];
			if (cross_pq > cross_qp) {
				left.push_back(q);
			} else {
				right.push_back(q);
			}
		}
	}

	left.push_back(p);
	left.append_array(right);
	return left;
}

void GraphEditArranger::_horizontal_alignment(Dictionary &r_root, Dictionary &r_align, const HashMap<int, Vector<StringName>> &r_layers, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours, const HashSet<StringName> &r_selected_nodes) {
	for (const StringName &E : r_selected_nodes) {
		r_root[E] = E;
		r_align[E] = E;
	}

	if (r_layers.size() == 1) {
		return;
	}

	for (unsigned int i = 1; i < r_layers.size(); i++) {
		Vector<StringName> lower_layer = r_layers[i];
		Vector<StringName> upper_layer = r_layers[i - 1];
		int r = -1;

		for (int j = 0; j < lower_layer.size(); j++) {
			Vector<Pair<int, StringName>> up;
			const StringName &current_node = lower_layer[j];
			for (int k = 0; k < upper_layer.size(); k++) {
				const StringName &adjacent_neighbour = upper_layer[k];
				if (r_upper_neighbours[current_node].has(adjacent_neighbour)) {
					up.push_back(Pair<int, StringName>(k, adjacent_neighbour));
				}
			}

			int start = (up.size() - 1) / 2;
			int end = (up.size() - 1) % 2 ? start + 1 : start;
			for (int p = start; p <= end; p++) {
				StringName Align = r_align[current_node];
				if (Align == current_node && r < up[p].first) {
					r_align[up[p].second] = lower_layer[j];
					r_root[current_node] = r_root[up[p].second];
					r_align[current_node] = r_root[up[p].second];
					r = up[p].first;
				}
			}
		}
	}
}

void GraphEditArranger::_crossing_minimisation(HashMap<int, Vector<StringName>> &r_layers, const HashMap<StringName, HashSet<StringName>> &r_upper_neighbours) {
	if (r_layers.size() == 1) {
		return;
	}

	for (unsigned int i = 1; i < r_layers.size(); i++) {
		Vector<StringName> upper_layer = r_layers[i - 1];
		Vector<StringName> lower_layer = r_layers[i];
		HashMap<StringName, Dictionary> c;

		for (int j = 0; j < lower_layer.size(); j++) {
			const StringName &p = lower_layer[j];
			Dictionary d;

			for (int k = 0; k < lower_layer.size(); k++) {
				unsigned int crossings = 0;
				const StringName &q = lower_layer[k];

				if (j != k) {
					for (int h = 1; h < upper_layer.size(); h++) {
						if (r_upper_neighbours[p].has(upper_layer[h])) {
							for (int g = 0; g < h; g++) {
								if (r_upper_neighbours[q].has(upper_layer[g])) {
									crossings++;
								}
							}
						}
					}
				}
				d[q] = crossings;
			}
			c.insert(p, d);
		}

		r_layers.insert(i, _split(lower_layer, c));
	}
}

void GraphEditArranger::_calculate_inner_shifts(Dictionary &r_inner_shifts, const Dictionary &r_root, const Dictionary &r_node_names, const Dictionary &r_align, const HashSet<StringName> &r_block_heads, const HashMap<StringName, Pair<int, int>> &r_port_info) {
	for (const StringName &E : r_block_heads) {
		real_t left = 0;
		StringName u = E;
		StringName v = r_align[u];
		while (u != v && (StringName)r_root[u] != v) {
			String _connection = String(u) + " " + String(v);

			GraphNode *gnode_from = Object::cast_to<GraphNode>(r_node_names[u]);
			GraphNode *gnode_to = Object::cast_to<GraphNode>(r_node_names[v]);

			Pair<int, int> ports = r_port_info[_connection];
			int port_from = ports.first;
			int port_to = ports.second;

			Vector2 pos_from = gnode_from->get_output_port_position(port_from) * graph_edit->get_zoom();
			Vector2 pos_to = gnode_to->get_input_port_position(port_to) * graph_edit->get_zoom();

			real_t s = (real_t)r_inner_shifts[u] + (pos_from.y - pos_to.y) / graph_edit->get_zoom();
			r_inner_shifts[v] = s;
			left = MIN(left, s);

			u = v;
			v = (StringName)r_align[v];
		}

		u = E;
		do {
			r_inner_shifts[u] = (real_t)r_inner_shifts[u] - left;
			u = (StringName)r_align[u];
		} while (u != E);
	}
}

float GraphEditArranger::_calculate_threshold(const StringName &p_v, const StringName &p_w, const Dictionary &r_node_names, const HashMap<int, Vector<StringName>> &r_layers, const Dictionary &r_root, const Dictionary &r_align, const Dictionary &r_inner_shift, real_t p_current_threshold, const HashMap<StringName, Vector2> &r_node_positions) {
#define MAX_ORDER 2147483647
#define ORDER(node, layers)                            \
	for (unsigned int i = 0; i < layers.size(); i++) { \
		int index = layers[i].find(node);              \
		if (index > 0) {                               \
			order = index;                             \
			break;                                     \
		}                                              \
		order = MAX_ORDER;                             \
	}

	int order = MAX_ORDER;
	float threshold = p_current_threshold;
	if (p_v == p_w) {
		int min_order = MAX_ORDER;
		Ref<GraphEdit::Connection> incoming;
		const Vector<Ref<GraphEdit::Connection>> connection_list = graph_edit->get_connections();
		for (const Ref<GraphEdit::Connection> &connection : connection_list) {
			if (connection->to_node == p_w) {
				ORDER(connection->from_node, r_layers);
				if (min_order > order) {
					min_order = order;
					incoming = connection;
				}
			}
		}

		if (incoming.is_valid()) {
			GraphNode *gnode_from = Object::cast_to<GraphNode>(r_node_names[incoming->from_node]);
			GraphNode *gnode_to = Object::cast_to<GraphNode>(r_node_names[p_w]);
			Vector2 pos_from = gnode_from->get_output_port_position(incoming->from_port) * graph_edit->get_zoom();
			Vector2 pos_to = gnode_to->get_input_port_position(incoming->to_port) * graph_edit->get_zoom();

			// If connected block node is selected, calculate threshold or add current block to list.
			if (gnode_from->is_selected()) {
				Vector2 connected_block_pos = r_node_positions[r_root[incoming->from_node]];
				if (connected_block_pos.y != FLT_MAX) {
					//Connected block is placed, calculate threshold.
					threshold = connected_block_pos.y + (real_t)r_inner_shift[incoming->from_node] - (real_t)r_inner_shift[p_w] + pos_from.y - pos_to.y;
				}
			}
		}
	}
	if (threshold == FLT_MIN && (StringName)r_align[p_w] == p_v) {
		// This time, pick an outgoing edge and repeat as above!
		int min_order = MAX_ORDER;
		Ref<GraphEdit::Connection> outgoing;
		const Vector<Ref<GraphEdit::Connection>> connection_list = graph_edit->get_connections();
		for (const Ref<GraphEdit::Connection> &connection : connection_list) {
			if (connection->from_node == p_w) {
				ORDER(connection->to_node, r_layers);
				if (min_order > order) {
					min_order = order;
					outgoing = connection;
				}
			}
		}

		if (outgoing.is_valid()) {
			GraphNode *gnode_from = Object::cast_to<GraphNode>(r_node_names[p_w]);
			GraphNode *gnode_to = Object::cast_to<GraphNode>(r_node_names[outgoing->to_node]);
			Vector2 pos_from = gnode_from->get_output_port_position(outgoing->from_port) * graph_edit->get_zoom();
			Vector2 pos_to = gnode_to->get_input_port_position(outgoing->to_port) * graph_edit->get_zoom();

			// If connected block node is selected, calculate threshold or add current block to list.
			if (gnode_to->is_selected()) {
				Vector2 connected_block_pos = r_node_positions[r_root[outgoing->to_node]];
				if (connected_block_pos.y != FLT_MAX) {
					//Connected block is placed. Calculate threshold
					threshold = connected_block_pos.y + (real_t)r_inner_shift[outgoing->to_node] - (real_t)r_inner_shift[p_w] + pos_from.y - pos_to.y;
				}
			}
		}
	}
#undef MAX_ORDER
#undef ORDER
	return threshold;
}

void GraphEditArranger::_place_block(const StringName &p_v, float p_delta, const HashMap<int, Vector<StringName>> &r_layers, const Dictionary &r_root, const Dictionary &r_align, const Dictionary &r_node_name, const Dictionary &r_inner_shift, Dictionary &r_sink, Dictionary &r_shift, HashMap<StringName, Vector2> &r_node_positions) {
#define PRED(node, layers)                             \
	for (unsigned int i = 0; i < layers.size(); i++) { \
		int index = layers[i].find(node);              \
		if (index > 0) {                               \
			predecessor = layers[i][index - 1];        \
			break;                                     \
		}                                              \
		predecessor = StringName();                    \
	}

	StringName predecessor;
	StringName successor;
	Vector2 pos = r_node_positions[p_v];

	if (pos.y == FLT_MAX) {
		pos.y = 0;
		bool initial = false;
		StringName w = p_v;
		real_t threshold = FLT_MIN;
		do {
			PRED(w, r_layers);
			if (predecessor != StringName()) {
				StringName u = r_root[predecessor];
				_place_block(u, p_delta, r_layers, r_root, r_align, r_node_name, r_inner_shift, r_sink, r_shift, r_node_positions);
				threshold = _calculate_threshold(p_v, w, r_node_name, r_layers, r_root, r_align, r_inner_shift, threshold, r_node_positions);
				if ((StringName)r_sink[p_v] == p_v) {
					r_sink[p_v] = r_sink[u];
				}

				Vector2 predecessor_root_pos = r_node_positions[u];
				Vector2 predecessor_node_size = Object::cast_to<GraphNode>(r_node_name[predecessor])->get_size();
				if (r_sink[p_v] != r_sink[u]) {
					real_t sc = pos.y + (real_t)r_inner_shift[w] - predecessor_root_pos.y - (real_t)r_inner_shift[predecessor] - predecessor_node_size.y - p_delta;
					r_shift[r_sink[u]] = MIN(sc, (real_t)r_shift[r_sink[u]]);
				} else {
					real_t sb = predecessor_root_pos.y + (real_t)r_inner_shift[predecessor] + predecessor_node_size.y - (real_t)r_inner_shift[w] + p_delta;
					sb = MAX(sb, threshold);
					if (initial) {
						pos.y = sb;
					} else {
						pos.y = MAX(pos.y, sb);
					}
					initial = false;
				}
			}
			threshold = _calculate_threshold(p_v, w, r_node_name, r_layers, r_root, r_align, r_inner_shift, threshold, r_node_positions);
			w = r_align[w];
		} while (w != p_v);
		r_node_positions.insert(p_v, pos);
	}

#undef PRED
}
