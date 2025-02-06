/**************************************************************************/
/*  audio_stream_graph.h                                                  */
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

#ifndef AUDIO_STREAM_GRAPH_H
#define AUDIO_STREAM_GRAPH_H

#include "core/templates/local_vector.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio_server.h"

class AudioStreamGraphNode;
class AudioStreamGraphNodeParameter;
class AudioStreamPlaybackGraph;
class AudioStreamGraphNodePlayback;

class AudioStreamGraph : public AudioStream {
	GDCLASS(AudioStreamGraph, AudioStream)

public:
	struct Connection {
		int from_node = 0;
		int from_port = 0;
		int to_node = 0;
		int to_port = 0;
	};

private:
	struct Node {
		Ref<AudioStreamGraphNode> node;
		Vector2 position;
		LocalVector<int> prev_connected_nodes;
		LocalVector<int> next_connected_nodes;
	};

	RBMap<int, Node> nodes;
	List<Connection> connections;

	mutable SafeFlag dirty;
	HashMap<StringName, Ref<AudioStreamGraphNodeParameter>> parameter_nodes_cache;
	void _queue_update();

protected:
	static void _bind_methods();
	void _update_graph();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;

public:
	virtual Ref<AudioStreamPlayback> instantiate_playback() override;

	void add_node(Ref<AudioStreamGraphNode> p_node, const Vector2 &p_position, const int p_id);
	Ref<AudioStreamGraphNode> get_node(const int p_id) const;
	void remove_node(const int p_id);
	bool has_node(const int p_id) const;
	Vector<int> get_node_list() const;
	Vector2 get_node_position(const int p_id) const;
	int get_valid_node_id();
	void get_node_connections(List<Connection> *r_connections) const;
	Error connect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void disconnect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void set_node_position(int p_id, const Vector2 &p_position);
	void start_nodes(double p_from_pos);
	int mix_nodes(AudioFrame *p_buffer, float p_rate_scale, int p_frames);
	void stop_nodes();
	bool any_nodes_playing() const;
	double get_nodes_playback_positions() const;
	int get_nodes_loop_count() const;
	String validate_parameter_name(const String &p_name, const Ref<AudioStreamGraphNodeParameter> &p_parameter) const;
	void set_audio_parameter(const StringName &p_param, const Variant &p_value);
	Variant get_audio_parameter(const StringName &p_param) const;

	AudioStreamGraph();
};

class AudioStreamGraphNode : public Resource {
	GDCLASS(AudioStreamGraphNode, Resource);

public:
	enum PortType {
		PORT_TYPE_STREAM,
		PORT_TYPE_SCALAR,
		PORT_TYPE_MAX,
	};

	enum Category {
		CATEGORY_INPUT,
		CATEGORY_OUTPUT,
	};

private:
	int linked_parent_graph_frame = -1;

	HashMap<int, bool> connected_input_ports;
	HashMap<int, int> connected_output_ports;
	HashMap<int, Ref<AudioStreamGraphNode>> connected_nodes;

protected:
	HashMap<int, Variant> default_input_values;
	bool simple_decl = true;
	bool disabled = false;
	bool closable = false;
	int port_group_count = 1;

	static void _bind_methods();

public:
	virtual String get_caption() const = 0;
	virtual String get_description() const = 0;
	virtual int get_input_port_count() const = 0;
	virtual PortType get_input_port_type(int p_port) const = 0;
	virtual String get_input_port_name(int p_port) const = 0;

	virtual int get_output_port_count() const = 0;
	virtual PortType get_output_port_type(int p_port) const = 0;
	virtual String get_output_port_name(int p_port) const = 0;

	virtual void update_default_values() = 0;

	virtual bool is_show_prop_names() const;
	virtual Vector<StringName> get_editable_properties() const;
	virtual HashMap<StringName, String> get_editable_properties_names() const;

	void add_input_port();
	void remove_input_port();
	void set_input_port_connected(int p_port, bool p_connected);
	void connect_input_node(Ref<AudioStreamGraphNode> p_node, int p_port);
	void disconnect_input_node(int p_port);
	void set_output_port_connected(int p_port, bool p_connected);
	Variant get_input_port_default_value(int p_port) const;
	void set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value);
	Array get_default_input_values() const;
	void set_default_input_values(const Array &p_values);
	void remove_input_port_default_value(int p_port);
	int get_port_group_count() const;
	void set_port_group_count(int p_port_group_count);

	List<Ref<AudioStreamGraphNode>> get_connected_input_nodes() const;
	HashMap<int, Ref<AudioStreamGraphNodePlayback>> get_connected_input_playback_nodes() const;
	HashMap<int, Ref<AudioStreamGraphNodeParameter>> get_connected_input_parameter_nodes() const;

	bool is_output_port_connected(int p_port) const;
	bool is_input_port_connected(int p_port) const;

	bool is_deletable() const;
	void set_deletable(bool p_closable = true);

	AudioStreamGraphNode();
};

VARIANT_ENUM_CAST(AudioStreamGraphNode::PortType)

class AudioStreamGraphNodePlayback : public AudioStreamGraphNode {
	GDCLASS(AudioStreamGraphNodePlayback, AudioStreamGraphNode);

public:
	virtual void start(double p_from_pos) = 0;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) = 0;
	virtual void stop() = 0;
	virtual int get_loop_count() const = 0; // times it looped
	virtual double get_playback_position() const = 0;
	virtual bool is_playing() const = 0;

	AudioStreamGraphNodePlayback();
};

class AudioStreamGraphNodeParameter : public AudioStreamGraphNode {
	GDCLASS(AudioStreamGraphNodeParameter, AudioStreamGraphNode);

	String parameter_name = "";
	Variant value = Variant::NIL;

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void set_parameter_name(const String &p_name);
	String get_parameter_name() const;

	void set_value(const Variant &p_value);
	Variant get_value() const;

	AudioStreamGraphNodeParameter();
};

class AudioStreamPlaybackGraph : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackGraph, AudioStreamPlayback)
	friend class AudioStreamGraph;

	Ref<AudioStreamGraph> stream_graph;

protected:
	static void _bind_methods();

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;
};

#endif // AUDIO_STREAM_GRAPH_H