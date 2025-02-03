/**************************************************************************/
/*  animation_blend_tree.h                                                */
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

#ifndef ANIMATION_BLEND_TREE_H
#define ANIMATION_BLEND_TREE_H

#include "scene/animation/animation_tree.h"

class AnimationNodeAnimation : public AnimationRootNode {
	GDCLASS(AnimationNodeAnimation, AnimationRootNode);

	StringName animation;
	StringName time;

	uint64_t last_version;
	bool skip;

protected:
	void _validate_property(PropertyInfo &property) const;

	static void _bind_methods();

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const;

	static Vector<String> (*get_editable_animation_list)();

	virtual String get_caption() const;
	virtual float process(float p_time, bool p_seek);

	void set_animation(const StringName &p_name);
	StringName get_animation() const;

	AnimationNodeAnimation();
};

class AnimationNodeOneShot : public AnimationNode {
	GDCLASS(AnimationNodeOneShot, AnimationNode);

public:
	enum MixMode {
		MIX_MODE_BLEND,
		MIX_MODE_ADD
	};

private:
	float fade_in;
	float fade_out;

	bool autorestart;
	float autorestart_delay;
	float autorestart_random_delay;
	MixMode mix;

	bool sync;

	/*	bool active;
	bool do_start;
	float time;
	float remaining;*/

	StringName active;
	StringName prev_active;
	StringName time;
	StringName remaining;
	StringName time_to_restart;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	void set_fadein_time(float p_time);
	void set_fadeout_time(float p_time);

	float get_fadein_time() const;
	float get_fadeout_time() const;

	void set_autorestart(bool p_active);
	void set_autorestart_delay(float p_time);
	void set_autorestart_random_delay(float p_time);

	bool has_autorestart() const;
	float get_autorestart_delay() const;
	float get_autorestart_random_delay() const;

	void set_mix_mode(MixMode p_mix);
	MixMode get_mix_mode() const;

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	virtual bool has_filter() const;
	virtual float process(float p_time, bool p_seek);

	AnimationNodeOneShot();
};

VARIANT_ENUM_CAST(AnimationNodeOneShot::MixMode)

class AnimationNodeAdd2 : public AnimationNode {
	GDCLASS(AnimationNodeAdd2, AnimationNode);

	StringName add_amount;
	bool sync;
	bool add_directly;

protected:
	static void _bind_methods();

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	void set_add_directly(bool p_add_directly);
	bool get_add_directly() const;

	virtual bool has_filter() const;
	virtual float process(float p_time, bool p_seek);

	AnimationNodeAdd2();
};

class AnimationNodeAdd3 : public AnimationNode {
	GDCLASS(AnimationNodeAdd3, AnimationNode);

	StringName add_amount;
	bool sync;
	bool add_directly;

protected:
	static void _bind_methods();

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	void set_add_directly(bool p_add_directly);
	bool get_add_directly() const;

	virtual bool has_filter() const;
	virtual float process(float p_time, bool p_seek);

	AnimationNodeAdd3();
};

class AnimationNodeBlend2 : public AnimationNode {
	GDCLASS(AnimationNodeBlend2, AnimationNode);

	StringName blend_amount;
	bool sync;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;
	virtual float process(float p_time, bool p_seek);

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	virtual bool has_filter() const;
	AnimationNodeBlend2();
};

class AnimationNodeBlend3 : public AnimationNode {
	GDCLASS(AnimationNodeBlend3, AnimationNode);

	StringName blend_amount;
	bool sync;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	float process(float p_time, bool p_seek);
	AnimationNodeBlend3();
};

class AnimationNodeTimeScale : public AnimationNode {
	GDCLASS(AnimationNodeTimeScale, AnimationNode);

	StringName scale;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	float process(float p_time, bool p_seek);

	AnimationNodeTimeScale();
};

class AnimationNodeTimeSeek : public AnimationNode {
	GDCLASS(AnimationNodeTimeSeek, AnimationNode);

	StringName seek_pos;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	float process(float p_time, bool p_seek);

	AnimationNodeTimeSeek();
};

class AnimationNodeTransition : public AnimationNode {
	GDCLASS(AnimationNodeTransition, AnimationNode);

	enum {
		MAX_INPUTS = 32
	};
	struct InputData {
		String name;
		bool auto_advance;
		InputData() { auto_advance = false; }
	};

	InputData inputs[MAX_INPUTS];
	int enabled_inputs;

	/*
	float prev_xfading;
	int prev;
	float time;
	int current;
	int prev_current; */

	StringName prev_xfading;
	StringName prev;
	StringName time;
	StringName current;
	StringName prev_current;

	float xfade;

	void _update_inputs();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &property) const;

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	virtual String get_caption() const;

	void set_enabled_inputs(int p_inputs);
	int get_enabled_inputs();

	void set_input_as_auto_advance(int p_input, bool p_enable);
	bool is_input_set_as_auto_advance(int p_input) const;

	void set_input_caption(int p_input, const String &p_name);
	String get_input_caption(int p_input) const;

	void set_cross_fade_time(float p_fade);
	float get_cross_fade_time() const;

	float process(float p_time, bool p_seek);

	AnimationNodeTransition();
};

class AnimationNodeOutput : public AnimationNode {
	GDCLASS(AnimationNodeOutput, AnimationNode);

public:
	virtual String get_caption() const;
	virtual float process(float p_time, bool p_seek);
	AnimationNodeOutput();
};

/////

class AnimationNodeBlendTree : public AnimationRootNode {
	GDCLASS(AnimationNodeBlendTree, AnimationRootNode);

	struct Node {
		Ref<AnimationNode> node;
		Vector2 position;
		Vector<StringName> connections;
	};

	typedef Map<StringName, Node, StringName::AlphCompare> NodeMap;
	NodeMap nodes;

	Vector2 graph_offset;

	void _tree_changed();
	void _node_changed(const StringName &p_node);

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	enum ConnectionError {
		CONNECTION_OK,
		CONNECTION_ERROR_NO_INPUT,
		CONNECTION_ERROR_NO_INPUT_INDEX,
		CONNECTION_ERROR_NO_OUTPUT,
		CONNECTION_ERROR_SAME_NODE,
		CONNECTION_ERROR_CONNECTION_EXISTS,
		//no need to check for cycles due to tree topology
	};

	void add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position = Vector2());
	Ref<AnimationNode> get_node(const StringName &p_name) const;
	void remove_node(const StringName &p_name);
	void rename_node(const StringName &p_name, const StringName &p_new_name);
	bool has_node(const StringName &p_name) const;
	StringName get_node_name(const Ref<AnimationNode> &p_node) const;
	Vector<StringName> get_node_connection_array(const StringName &p_name) const;

	void set_node_position(const StringName &p_node, const Vector2 &p_position);
	Vector2 get_node_position(const StringName &p_node) const;

	virtual void get_child_nodes(List<ChildNode> *r_child_nodes);

	void connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node);
	void disconnect_node(const StringName &p_node, int p_input_index);

	struct NodeConnection {
		StringName input_node;
		int input_index;
		StringName output_node;
	};

	ConnectionError can_connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node) const;
	void get_node_connections(List<NodeConnection> *r_connections) const;

	virtual String get_caption() const;
	virtual float process(float p_time, bool p_seek);

	void get_node_list(List<StringName> *r_list);

	void set_graph_offset(const Vector2 &p_graph_offset);
	Vector2 get_graph_offset() const;

	virtual Ref<AnimationNode> get_child_by_name(const StringName &p_name);

	AnimationNodeBlendTree();
	~AnimationNodeBlendTree();
};

VARIANT_ENUM_CAST(AnimationNodeBlendTree::ConnectionError)

#endif // ANIMATION_BLEND_TREE_H
