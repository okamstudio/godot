/**************************************************************************/
/*  audio_stream_graph.cpp                                               */
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

#include "audio_stream_graph.h"
#include "audio_stream_graph_nodes.h"

Ref<AudioStreamPlayback> AudioStreamGraph::instantiate_playback() {
	Ref<AudioStreamPlaybackGraph> playback_graph;
	playback_graph.instantiate();
	playback_graph->stream_graph = Ref<AudioStreamGraph>(this);
	return playback_graph;
}

void AudioStreamGraph::add_node(Ref<AudioStreamGraphNode> p_node, const Vector2 &p_position, const int p_id) {
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(p_id < 2);
	ERR_FAIL_COND(nodes.has(p_id));
	Node n;
	n.node = p_node;
	n.position = p_position;

	Ref<AudioStreamGraphNodeParameter> parameter = n.node;
	if (parameter.is_valid()) {
		String valid_name = validate_parameter_name(parameter->get_parameter_name(), parameter);
		parameter->set_parameter_name(valid_name);
	}

	emit_changed();
	//emit_signal(SNAME("tree_changed"));

	// p_node->connect(SNAME("tree_changed"), callable_mp(this, &AnimationNodeBlendTree::_tree_changed), CONNECT_REFERENCE_COUNTED);
	// p_node->connect(SNAME("animation_node_renamed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_renamed), CONNECT_REFERENCE_COUNTED);
	// p_node->connect(SNAME("animation_node_removed"), callable_mp(this, &AnimationNodeBlendTree::_animation_node_removed), CONNECT_REFERENCE_COUNTED);
	// p_node->connect_changed(callable_mp(this, &AnimationNodeBlendTree::_node_changed).bind(p_name), CONNECT_REFERENCE_COUNTED);

	nodes[p_id] = n;
}

Error AudioStreamGraph::connect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	ERR_FAIL_COND_V(!nodes.has(p_from_node), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!nodes.has(p_to_node), ERR_INVALID_PARAMETER);
	ERR_FAIL_INDEX_V(p_to_port, nodes[p_to_node].node->get_input_port_count(), ERR_INVALID_PARAMETER);

	for (const Connection &E : connections) {
		if (E.from_node == p_from_node && E.from_port == p_from_port && E.to_node == p_to_node && E.to_port == p_to_port) {
			ERR_FAIL_V(ERR_ALREADY_EXISTS);
		}
	}

	Connection c;
	c.from_node = p_from_node;
	c.from_port = p_from_port;
	c.to_node = p_to_node;
	c.to_port = p_to_port;
	connections.push_back(c);
	nodes[p_from_node].next_connected_nodes.push_back(p_to_node);
	nodes[p_to_node].prev_connected_nodes.push_back(p_from_node);
	nodes[p_from_node].node->set_output_port_connected(p_from_port, true);
	nodes[p_to_node].node->set_input_port_connected(p_to_port, true);
	nodes[p_to_node].node->connect_input_node(nodes[p_from_node].node, p_to_port);

	Ref<AudioStreamGraphNodeParameter> parameter = Object::cast_to<AudioStreamGraphNodeParameter>(nodes[p_to_node].node.ptr());
	if (parameter.is_valid()) {
		if (!parameter_nodes_cache.has(parameter->get_parameter_name())) {
			parameter_nodes_cache[parameter->get_parameter_name()] = parameter;
		}
	}

	parameter = Object::cast_to<AudioStreamGraphNodeParameter>(nodes[p_from_node].node.ptr());
	if (parameter.is_valid()) {
		if (!parameter_nodes_cache.has(parameter->get_parameter_name())) {
			parameter_nodes_cache[parameter->get_parameter_name()] = parameter;
		}
	}

	_queue_update();
	return OK;
}

void AudioStreamGraph::disconnect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {
		if (E->get().from_node == p_from_node && E->get().from_port == p_from_port && E->get().to_node == p_to_node && E->get().to_port == p_to_port) {
			connections.erase(E);
			nodes[p_from_node].next_connected_nodes.erase(p_to_node);
			nodes[p_to_node].prev_connected_nodes.erase(p_from_node);
			nodes[p_from_node].node->set_output_port_connected(p_from_port, false);
			nodes[p_to_node].node->set_input_port_connected(p_to_port, false);
			nodes[p_to_node].node->disconnect_input_node(p_to_port);

			Ref<AudioStreamGraphNodeParameter> parameter = Object::cast_to<AudioStreamGraphNodeParameter>(nodes[p_to_node].node.ptr());
			if (parameter.is_valid()) {
				if (parameter_nodes_cache.has(parameter->get_parameter_name())) {
					parameter_nodes_cache.erase(parameter->get_parameter_name());
				}
			}

			parameter = Object::cast_to<AudioStreamGraphNodeParameter>(nodes[p_from_node].node.ptr());
			if (parameter.is_valid()) {
				if (parameter_nodes_cache.has(parameter->get_parameter_name())) {
					parameter_nodes_cache.erase(parameter->get_parameter_name());
				}
			}

			_queue_update();
			return;
		}
	}
}

String AudioStreamGraph::validate_parameter_name(const String &p_name, const Ref<AudioStreamGraphNodeParameter> &p_parameter) const {
	String param_name = p_name; //validate name first
	while (param_name.length() && !is_ascii_alphabet_char(param_name[0])) {
		param_name = param_name.substr(1, param_name.length() - 1);
	}
	if (!param_name.is_empty()) {
		String valid_name;

		for (int i = 0; i < param_name.length(); i++) {
			if (is_ascii_identifier_char(param_name[i])) {
				valid_name += String::chr(param_name[i]);
			} else if (param_name[i] == ' ') {
				valid_name += "_";
			}
		}

		param_name = valid_name;
	}

	if (param_name.is_empty()) {
		param_name = p_parameter->get_caption();
	}

	int attempt = 1;

	while (true) {
		bool exists = false;

		for (const KeyValue<int, Node> &E : nodes) {
			Ref<AudioStreamGraphNodeParameter> node = E.value.node;
			if (node == p_parameter) { //do not test on self
				continue;
			}
			if (node.is_valid() && node->get_parameter_name() == param_name) {
				exists = true;
				break;
			}
		}

		if (exists) {
			//remove numbers, put new and try again
			attempt++;
			while (param_name.length() && is_digit(param_name[param_name.length() - 1])) {
				param_name = param_name.substr(0, param_name.length() - 1);
			}
			ERR_FAIL_COND_V(param_name.is_empty(), String());
			param_name += itos(attempt);
		} else {
			break;
		}
	}

	return param_name;
}

void AudioStreamGraph::set_node_position(int p_id, const Vector2 &p_position) {
	ERR_FAIL_COND(!nodes.has(p_id));
	nodes[p_id].position = p_position;
}

Ref<AudioStreamGraphNode> AudioStreamGraph::get_node(const int p_id) const {
	if (!nodes.has(p_id)) {
		return Ref<AudioStreamGraphNode>();
	}
	ERR_FAIL_COND_V(!nodes.has(p_id), Ref<AudioStreamGraphNode>());
	return nodes[p_id].node;
}

void AudioStreamGraph::remove_node(const int p_id) {
	ERR_FAIL_COND(p_id < 2);
	ERR_FAIL_COND(!nodes.has(p_id));

	nodes.erase(p_id);

	for (List<Connection>::Element *E = connections.front(); E;) {
		List<Connection>::Element *N = E->next();
		const AudioStreamGraph::Connection &connection = E->get();
		if (connection.from_node == p_id || connection.to_node == p_id) {
			if (connection.from_node == p_id) {
				nodes[connection.to_node].prev_connected_nodes.erase(p_id);
				nodes[connection.to_node].node->set_input_port_connected(connection.to_port, false);
			} else if (connection.to_node == p_id) {
				nodes[connection.from_node].next_connected_nodes.erase(p_id);
				nodes[connection.from_node].node->set_output_port_connected(connection.from_port, false);
			}
			connections.erase(E);
		}
		E = N;
	}
}

bool AudioStreamGraph::has_node(const int p_id) const {
	return nodes.has(p_id);
}

Vector<int> AudioStreamGraph::get_node_list() const {
	Vector<int> ret;
	for (const KeyValue<int, Node> &E : nodes) {
		ret.push_back(E.key);
	}

	return ret;
}

Vector2 AudioStreamGraph::get_node_position(const int p_id) const {
	ERR_FAIL_COND_V(!nodes.has(p_id), Vector2());
	return nodes[p_id].position;
}

int AudioStreamGraph::get_valid_node_id() {
	return nodes.size() ? MAX(2, nodes.back()->key() + 1) : 2;
}

void AudioStreamGraph::get_node_connections(List<Connection> *r_connections) const {
	for (const Connection &E : connections) {
		r_connections->push_back(E);
	}
}

void AudioStreamGraph::start_nodes(double p_from_pos) {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());

	ERR_FAIL_COND_EDMSG(!playback_node, "Node connected dirrectly to an output node is a parameter node");

	playback_node->start(p_from_pos);
}

int AudioStreamGraph::mix_nodes(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());
	return playback_node->mix(p_buffer, p_rate_scale, p_frames);
}

void AudioStreamGraph::stop_nodes() {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());
	playback_node->stop();
}

bool AudioStreamGraph::any_nodes_playing() const {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());
	return playback_node->is_playing();
}

double AudioStreamGraph::get_nodes_playback_positions() const {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());
	return playback_node->get_playback_position();
}

int AudioStreamGraph::get_nodes_loop_count() const {
	const Node *current_node = &nodes[0];
	AudioStreamGraphNodePlayback *playback_node = Object::cast_to<AudioStreamGraphNodePlayback>(current_node->node.ptr());
	return playback_node->get_loop_count();
}

void AudioStreamGraph::set_audio_parameter(const StringName &p_param, const Variant &p_value) {
	for (const KeyValue<int, Node> &E : nodes) {
		const Node *current_node = &E.value;
		Ref<AudioStreamGraphNodeParameter> parameter = Object::cast_to<AudioStreamGraphNodeParameter>(current_node->node.ptr());
		if (!parameter.is_valid()) {
			continue;
		}

		if (parameter->get_parameter_name() == p_param) {
			parameter->set_value(p_value);
			break;
		}
	}
}

Variant AudioStreamGraph::get_audio_parameter(const StringName &p_param) const {
	for (const KeyValue<int, Node> &E : nodes) {
		const Node *current_node = &E.value;
		Ref<AudioStreamGraphNodeParameter> parameter = Object::cast_to<AudioStreamGraphNodeParameter>(current_node->node.ptr());
		if (!parameter.is_valid()) {
			continue;
		}

		if (parameter->get_parameter_name() == p_param) {
			return parameter->get_value();
		}
	}

	return Variant();
}

void AudioStreamGraph::_queue_update() {
	if (dirty.is_set()) {
		return;
	}

	dirty.set();
	call_deferred(SNAME("_update_graph"));
}

void AudioStreamGraph::_update_graph() {
	if (!dirty.is_set()) {
		return;
	}
	dirty.clear();
	notify_property_list_changed();
	emit_changed();
}

void AudioStreamGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_node", "id", "node", "position"), &AudioStreamGraph::add_node);
	ClassDB::bind_method(D_METHOD("get_node", "id"), &AudioStreamGraph::get_node);
	ClassDB::bind_method(D_METHOD("remove_node", "id"), &AudioStreamGraph::remove_node);
	ClassDB::bind_method(D_METHOD("set_node_position", "id", "position"), &AudioStreamGraph::set_node_position);

	ClassDB::bind_method(D_METHOD("set_audio_parameter", "param", "value"), &AudioStreamGraph::set_audio_parameter);
	ClassDB::bind_method(D_METHOD("get_audio_parameter", "param"), &AudioStreamGraph::get_audio_parameter);

	ClassDB::bind_method(D_METHOD("connect_nodes", "from_node", "from_port", "to_node", "to_port"), &AudioStreamGraph::connect_nodes);
	ClassDB::bind_method(D_METHOD("disconnect_nodes", "from_node", "from_port", "to_node", "to_port"), &AudioStreamGraph::disconnect_nodes);

	ClassDB::bind_method(D_METHOD("_update_graph"), &AudioStreamGraph::_update_graph);
}

bool AudioStreamGraph::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String index = prop_name.get_slicec('/', 1);
		if (index == "connections") {
			Vector<int> conns = p_value;
			if (conns.size() % 4 == 0) {
				for (int i = 0; i < conns.size(); i += 4) {
					connect_nodes(conns[i + 0], conns[i + 1], conns[i + 2], conns[i + 3]);
				}
			}
			return true;
		}

		int id = index.to_int();
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			add_node(p_value, Vector2(), id);
			return true;
		} else if (what == "position") {
			set_node_position(id, p_value);
			return true;
		}
	}

	if (parameter_nodes_cache.has(prop_name)) {
		parameter_nodes_cache.get(prop_name)->set_value(p_value);
		return true;
	}

	return false;
}

bool AudioStreamGraph::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String index = prop_name.get_slicec('/', 1);
		if (index == "connections") {
			Vector<int> conns;
			for (const Connection &E : connections) {
				conns.push_back(E.from_node);
				conns.push_back(E.from_port);
				conns.push_back(E.to_node);
				conns.push_back(E.to_port);
			}

			r_ret = conns;
			return true;
		}

		int id = index.to_int();
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			r_ret = get_node(id);
			return true;
		} else if (what == "position") {
			r_ret = get_node_position(id);
			return true;
		}
	}

	if (parameter_nodes_cache.has(prop_name)) {
		r_ret = parameter_nodes_cache.get(prop_name)->get_value();
		return true;
	}

	return false;
}

void AudioStreamGraph::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<int, Node> &E : nodes) {
		String prop_name = "nodes/" + itos(E.key);

		if (E.key != 0) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, prop_name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "AudioStreamGraphNode", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_ALWAYS_DUPLICATE));
		}
		p_list->push_back(PropertyInfo(Variant::VECTOR2, prop_name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "nodes/connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));

	p_list->push_back(PropertyInfo(Variant::NIL, "Audio Parameters", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP));

	for (const KeyValue<StringName, Ref<AudioStreamGraphNodeParameter>> &E : parameter_nodes_cache) {
		p_list->push_back(PropertyInfo(E.value->get_value().get_type(), E.key, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR));
	}
}

bool AudioStreamGraph::_property_can_revert(const StringName &p_name) const {
	if (parameter_nodes_cache.has(p_name)) {
		Ref<AudioStreamGraphNodeParameter> param = parameter_nodes_cache.get(p_name);
		Ref<AudioStreamGraphNodeFloatParameter> float_param = Object::cast_to<AudioStreamGraphNodeFloatParameter>(param.ptr());
		if (float_param.is_valid()) {
			return float_param->is_default_value_enabled();
		}
	}
	return false;
}

bool AudioStreamGraph::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (parameter_nodes_cache.has(p_name)) {
		Ref<AudioStreamGraphNodeParameter> param = parameter_nodes_cache.get(p_name);
		Ref<AudioStreamGraphNodeFloatParameter> float_param = Object::cast_to<AudioStreamGraphNodeFloatParameter>(param.ptr());
		if (float_param.is_valid()) {
			r_property = float_param->get_default_value();
			return true;
		}
	}
	return false;
}

void AudioStreamGraph::reset_state() {
	// TODO: Everything needs to be cleared here.
	emit_changed();
}

AudioStreamGraph::AudioStreamGraph() {
	Ref<AudioStreamGraphOutputNode> output;
	output.instantiate();
	nodes[0].node = output;

	nodes[0].position = Vector2(400, 150);
}

/////

void AudioStreamGraphNode::add_input_port() {
	int new_group_value = get_port_group_count() + 1;
	set_port_group_count(new_group_value);
	update_default_values();
}

void AudioStreamGraphNode::remove_input_port() {
	remove_input_port_default_value(get_input_port_count() - 1);
	int new_group_value = get_port_group_count() - 1;
	set_port_group_count(new_group_value);
	update_default_values();
}

void AudioStreamGraphNode::set_input_port_connected(int p_port, bool p_connected) {
	connected_input_ports[p_port] = p_connected;
}

void AudioStreamGraphNode::set_output_port_connected(int p_port, bool p_connected) {
	if (p_connected) {
		connected_output_ports[p_port]++;
	} else {
		connected_output_ports[p_port]--;
	}
}

void AudioStreamGraphNode::connect_input_node(Ref<AudioStreamGraphNode> p_node, int p_port) {
	if (!p_node.is_valid()) {
		return;
	}

	connected_nodes[p_port] = p_node;
}

void AudioStreamGraphNode::disconnect_input_node(int p_port) {
	if (!connected_nodes.has(p_port)) {
		return;
	}

	connected_nodes.erase(p_port);
}

bool AudioStreamGraphNode::is_output_port_connected(int p_port) const {
	if (connected_output_ports.has(p_port)) {
		return connected_output_ports[p_port] > 0;
	}
	return false;
}

bool AudioStreamGraphNode::is_input_port_connected(int p_port) const {
	if (connected_input_ports.has(p_port)) {
		return connected_input_ports[p_port];
	}
	return false;
}

List<Ref<AudioStreamGraphNode>> AudioStreamGraphNode::get_connected_input_nodes() const {
	List<Ref<AudioStreamGraphNode>> out_list = {};
	for (int i = 0; i < get_input_port_count(); i++) {
		if (connected_nodes.has(i)) {
			out_list.push_back(connected_nodes.get(i));
		}
	}

	return out_list;
}

HashMap<int, Ref<AudioStreamGraphNodePlayback>> AudioStreamGraphNode::get_connected_input_playback_nodes() const {
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> out_nodes = {};
	for (int i = 0; i < get_input_port_count(); i++) {
		if (connected_nodes.has(i)) {
			Ref<AudioStreamGraphNodePlayback> playback_node = connected_nodes.get(i);
			if (playback_node.is_valid()) {
				out_nodes[i] = playback_node;
			}
		}
	}

	return out_nodes;
}

HashMap<int, Ref<AudioStreamGraphNodeParameter>> AudioStreamGraphNode::get_connected_input_parameter_nodes() const {
	HashMap<int, Ref<AudioStreamGraphNodeParameter>> out_nodes = {};
	for (int i = 0; i < get_input_port_count(); i++) {
		if (connected_nodes.has(i)) {
			Ref<AudioStreamGraphNodeParameter> parameter_node = connected_nodes.get(i);
			if (parameter_node.is_valid()) {
				out_nodes[i] = parameter_node;
			}
		}
	}

	return out_nodes;
}

Variant AudioStreamGraphNode::get_input_port_default_value(int p_port) const {
	if (default_input_values.has(p_port)) {
		return default_input_values.get(p_port);
	}

	return Variant();
}

void AudioStreamGraphNode::set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value) {
	Variant value = p_value;

	if (p_prev_value.get_type() != Variant::NIL) {
		switch (p_value.get_type()) {
			case Variant::FLOAT: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						value = (float)p_prev_value;
					} break;
					case Variant::FLOAT: {
						value = p_prev_value;
					} break;
					default:
						break;
				}
			} break;
			case Variant::INT: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						value = p_prev_value;
					} break;
					case Variant::FLOAT: {
						value = (int)p_prev_value;
					} break;
					default:
						break;
				}
			} break;
			default:
				break;
		}
	}
	default_input_values[p_port] = value;
	emit_changed();
}

Array AudioStreamGraphNode::get_default_input_values() const {
	Array ret;
	for (const KeyValue<int, Variant> &E : default_input_values) {
		ret.push_back(E.key);
		ret.push_back(E.value);
	}
	return ret;
}

void AudioStreamGraphNode::set_default_input_values(const Array &p_values) {
	if (p_values.size() % 2 == 0) {
		for (int i = 0; i < p_values.size(); i += 2) {
			default_input_values[p_values[i + 0]] = p_values[i + 1];
		}
	}

	emit_changed();
}

void AudioStreamGraphNode::remove_input_port_default_value(int p_port) {
	if (default_input_values.has(p_port)) {
		default_input_values.erase(p_port);
		emit_changed();
	}
}

bool AudioStreamGraphNode::is_show_prop_names() const {
	return false;
}

Vector<StringName> AudioStreamGraphNode::get_editable_properties() const {
	return Vector<StringName>();
}

HashMap<StringName, String> AudioStreamGraphNode::get_editable_properties_names() const {
	return HashMap<StringName, String>();
}

bool AudioStreamGraphNode::is_deletable() const {
	return closable;
}

void AudioStreamGraphNode::set_deletable(bool p_closable) {
	closable = p_closable;
}

int AudioStreamGraphNode::get_port_group_count() const {
	return port_group_count;
}

void AudioStreamGraphNode::set_port_group_count(int p_port_group_count) {
	port_group_count = p_port_group_count;
}

void AudioStreamGraphNode::_bind_methods() {
	ClassDB::bind_method("add_input_port", &AudioStreamGraphNode::add_input_port);
	ClassDB::bind_method("remove_input_port", &AudioStreamGraphNode::remove_input_port);

	ClassDB::bind_method(D_METHOD("set_input_port_default_value", "port", "value", "prev_value"), &AudioStreamGraphNode::set_input_port_default_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_input_port_default_value", "port"), &AudioStreamGraphNode::get_input_port_default_value);

	ClassDB::bind_method(D_METHOD("set_default_input_values", "values"), &AudioStreamGraphNode::set_default_input_values);
	ClassDB::bind_method(D_METHOD("get_default_input_values"), &AudioStreamGraphNode::get_default_input_values);

	ClassDB::bind_method(D_METHOD("set_port_group_count", "port_group_count"), &AudioStreamGraphNode::set_port_group_count);
	ClassDB::bind_method(D_METHOD("get_port_group_count"), &AudioStreamGraphNode::get_port_group_count);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "default_input_values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_default_input_values", "get_default_input_values");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port_group_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_port_group_count", "get_port_group_count");

	BIND_ENUM_CONSTANT(PORT_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(PORT_TYPE_STREAM);
}

AudioStreamGraphNode::AudioStreamGraphNode() {
}

AudioStreamGraphNodePlayback::AudioStreamGraphNodePlayback() {
}

void AudioStreamGraphNodeParameter::set_parameter_name(const String &p_name) {
	parameter_name = p_name;
	emit_changed();
}

String AudioStreamGraphNodeParameter::get_parameter_name() const {
	return parameter_name;
}

void AudioStreamGraphNodeParameter::set_value(const Variant &p_value) {
	value = p_value;
}

Variant AudioStreamGraphNodeParameter::get_value() const {
	return value;
}

bool AudioStreamGraphNodeParameter::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "value") {
		value = p_value;
		return true;
	}
	return false;
}

bool AudioStreamGraphNodeParameter::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "value") {
		r_ret = value;
		return true;
	}
	return false;
}

void AudioStreamGraphNodeParameter::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(value.get_type(), "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
}

void AudioStreamGraphNodeParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_parameter_name", "name"), &AudioStreamGraphNodeParameter::set_parameter_name);
	ClassDB::bind_method(D_METHOD("get_parameter_name"), &AudioStreamGraphNodeParameter::get_parameter_name);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &AudioStreamGraphNodeParameter::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &AudioStreamGraphNodeParameter::get_value);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "parameter_name"), "set_parameter_name", "get_parameter_name");
}

AudioStreamGraphNodeParameter::AudioStreamGraphNodeParameter() {
}

void AudioStreamPlaybackGraph::start(double p_from_pos) {
	if (is_playing()) {
		stop();
	}

	stream_graph->start_nodes(p_from_pos);
}

void AudioStreamPlaybackGraph::stop() {
	stream_graph->stop_nodes();
}

bool AudioStreamPlaybackGraph::is_playing() const {
	return stream_graph->any_nodes_playing();
}

int AudioStreamPlaybackGraph::get_loop_count() const {
	return stream_graph->get_nodes_loop_count();
}

double AudioStreamPlaybackGraph::get_playback_position() const {
	return stream_graph->get_nodes_playback_positions();
}

void AudioStreamPlaybackGraph::seek(double p_time) {
}

int AudioStreamPlaybackGraph::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	return stream_graph->mix_nodes(p_buffer, p_rate_scale, p_frames);
}

void AudioStreamPlaybackGraph::tag_used_streams() {
	if (is_playing()) {
		stream_graph->tag_used(stream_graph->get_nodes_playback_positions());
	}

	stream_graph->tag_used(0);
}

void AudioStreamPlaybackGraph::_bind_methods() {
}