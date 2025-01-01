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

	StringName backward = "backward"; // Only used by pingpong animation.

	StringName animation;

	bool advance_on_start = false;

	bool use_custom_timeline = false;
	double timeline_length = 1.0;
	Animation::LoopMode loop_mode = Animation::LOOP_NONE;
	bool stretch_time_scale = true;
	double start_offset = 0.0;

	uint64_t last_version = 0;
	bool skip = false;

public:
	enum PlayMode {
		PLAY_MODE_FORWARD,
		PLAY_MODE_BACKWARD
	};

	void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual NodeTimeInfo get_node_time_info() const override; // Wrapper of get_parameter().

	static Vector<String> (*get_editable_animation_list)();

	virtual String get_caption() const override;
	virtual NodeTimeInfo process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	void set_animation(const StringName &p_name);
	StringName get_animation() const;

	void set_play_mode(PlayMode p_play_mode);
	PlayMode get_play_mode() const;

	void set_backward(bool p_backward);
	bool is_backward() const;

	void set_advance_on_start(bool p_advance_on_start);
	bool is_advance_on_start() const;

	void set_use_custom_timeline(bool p_use_custom_timeline);
	bool is_using_custom_timeline() const;

	void set_timeline_length(double p_length);
	double get_timeline_length() const;

	void set_stretch_time_scale(bool p_strech_time_scale);
	bool is_stretching_time_scale() const;

	void set_start_offset(double p_offset);
	double get_start_offset() const;

	void set_loop_mode(Animation::LoopMode p_loop_mode);
	Animation::LoopMode get_loop_mode() const;

	AnimationNodeAnimation();

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

private:
	PlayMode play_mode = PLAY_MODE_FORWARD;
};

VARIANT_ENUM_CAST(AnimationNodeAnimation::PlayMode)

class AnimationNodeSync : public AnimationNode {
	GDCLASS(AnimationNodeSync, AnimationNode);

protected:
	bool sync = false;

	static void _bind_methods();

public:
	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

	AnimationNodeSync();
};

class AnimationNodeOneShot : public AnimationNodeSync {
	GDCLASS(AnimationNodeOneShot, AnimationNodeSync);

public:
	enum OneShotRequest {
		ONE_SHOT_REQUEST_NONE,
		ONE_SHOT_REQUEST_FIRE,
		ONE_SHOT_REQUEST_ABORT,
		ONE_SHOT_REQUEST_FADE_OUT,
	};

	enum MixMode {
		MIX_MODE_BLEND,
		MIX_MODE_ADD
	};

private:
	double fade_in = 0.0;
	Ref<Curve> fade_in_curve;
	double fade_out = 0.0;
	Ref<Curve> fade_out_curve;

	bool auto_restart = false;
	double auto_restart_delay = 1.0;
	double auto_restart_random_delay = 0.0;
	MixMode mix = MIX_MODE_BLEND;
	bool break_loop_at_end = false;

	StringName request = PNAME("request");
	StringName active = PNAME("active");
	StringName internal_active = PNAME("internal_active");
	StringName fade_in_remaining = "fade_in_remaining";
	StringName fade_out_remaining = "fade_out_remaining";
	StringName time_to_restart = "time_to_restart";

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;
	virtual bool is_parameter_read_only(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	void set_fade_in_time(double p_time);
	double get_fade_in_time() const;

	void set_fade_in_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_fade_in_curve() const;

	void set_fade_out_time(double p_time);
	double get_fade_out_time() const;

	void set_fade_out_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_fade_out_curve() const;

	void set_auto_restart_enabled(bool p_enabled);
	void set_auto_restart_delay(double p_time);
	void set_auto_restart_random_delay(double p_time);

	bool is_auto_restart_enabled() const;
	double get_auto_restart_delay() const;
	double get_auto_restart_random_delay() const;

	void set_mix_mode(MixMode p_mix);
	MixMode get_mix_mode() const;

	void set_break_loop_at_end(bool p_enable);
	bool is_loop_broken_at_end() const;

	virtual bool has_filter() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeOneShot();
};

VARIANT_ENUM_CAST(AnimationNodeOneShot::OneShotRequest)
VARIANT_ENUM_CAST(AnimationNodeOneShot::MixMode)

class AnimationNodeAdd2 : public AnimationNodeSync {
	GDCLASS(AnimationNodeAdd2, AnimationNodeSync);

	StringName add_amount = PNAME("add_amount");

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual bool has_filter() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeAdd2();
};

class AnimationNodeAdd3 : public AnimationNodeSync {
	GDCLASS(AnimationNodeAdd3, AnimationNodeSync);

	StringName add_amount = PNAME("add_amount");

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual bool has_filter() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeAdd3();
};

class AnimationNodeBlend2 : public AnimationNodeSync {
	GDCLASS(AnimationNodeBlend2, AnimationNodeSync);

	StringName blend_amount = PNAME("blend_amount");

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	virtual bool has_filter() const override;
	AnimationNodeBlend2();
};

class AnimationNodeBlend3 : public AnimationNodeSync {
	GDCLASS(AnimationNodeBlend3, AnimationNodeSync);

	StringName blend_amount = PNAME("blend_amount");

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;
	AnimationNodeBlend3();
};

class AnimationNodeSub2 : public AnimationNodeSync {
	GDCLASS(AnimationNodeSub2, AnimationNodeSync);

	StringName sub_amount = PNAME("sub_amount");

public:
	void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual bool has_filter() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeSub2();
};

class AnimationNodeTimeScale : public AnimationNode {
	GDCLASS(AnimationNodeTimeScale, AnimationNode);

	StringName scale = PNAME("scale");

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeTimeScale();
};

class AnimationNodeTimeSeek : public AnimationNode {
	GDCLASS(AnimationNodeTimeSeek, AnimationNode);

	StringName seek_pos_request = PNAME("seek_request");
	bool explicit_elapse = true;

protected:
	static void _bind_methods();

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	void set_explicit_elapse(bool p_enable);
	bool is_explicit_elapse() const;

	AnimationNodeTimeSeek();
};

class AnimationNodeTransition : public AnimationNodeSync {
	GDCLASS(AnimationNodeTransition, AnimationNodeSync);

	struct InputData {
		bool auto_advance = false;
		bool break_loop_at_end = false;
		bool reset = true;
	};
	LocalVector<InputData> input_data;

	StringName prev_xfading = "prev_xfading";
	StringName prev_index = "prev_index";
	StringName current_index = PNAME("current_index");
	StringName current_state = PNAME("current_state");
	StringName transition_request = PNAME("transition_request");

	StringName prev_frame_current = "pf_current";
	StringName prev_frame_current_idx = "pf_current_idx";

	double xfade_time = 0.0;
	Ref<Curve> xfade_curve;
	bool allow_transition_to_self = false;

	bool pending_update = false;

protected:
	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;
	virtual bool is_parameter_read_only(const StringName &p_parameter) const override;

	virtual String get_caption() const override;

	void set_input_count(int p_inputs);

	virtual bool add_input(const String &p_name) override;
	virtual void remove_input(int p_index) override;
	virtual bool set_input_name(int p_input, const String &p_name) override;

	void set_input_as_auto_advance(int p_input, bool p_enable);
	bool is_input_set_as_auto_advance(int p_input) const;

	void set_input_break_loop_at_end(int p_input, bool p_enable);
	bool is_input_loop_broken_at_end(int p_input) const;

	void set_input_reset(int p_input, bool p_enable);
	bool is_input_reset(int p_input) const;

	void set_xfade_time(double p_fade);
	double get_xfade_time() const;

	void set_xfade_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_xfade_curve() const;

	void set_allow_transition_to_self(bool p_enable);
	bool is_allow_transition_to_self() const;

	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	AnimationNodeTransition();
};

class AnimationNodeOutput : public AnimationNode {
	GDCLASS(AnimationNodeOutput, AnimationNode);

public:
	virtual String get_caption() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;
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

	RBMap<StringName, Node, StringName::AlphCompare> nodes;

	Vector2 graph_offset;

	void _node_changed(const StringName &p_node);

	void _initialize_node_tree();

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void _tree_changed() override;
	virtual void _animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) override;
	virtual void _animation_node_removed(const ObjectID &p_oid, const StringName &p_node) override;

	virtual void reset_state() override;

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

	virtual void get_child_nodes(List<ChildNode> *r_child_nodes) override;

	void connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node);
	void disconnect_node(const StringName &p_node, int p_input_index);

	struct NodeConnection {
		StringName input_node;
		int input_index = 0;
		StringName output_node;
	};

	ConnectionError can_connect_node(const StringName &p_input_node, int p_input_index, const StringName &p_output_node) const;
	void get_node_connections(List<NodeConnection> *r_connections) const;

	virtual String get_caption() const override;
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	void get_node_list(List<StringName> *r_list);

	void set_graph_offset(const Vector2 &p_graph_offset);
	Vector2 get_graph_offset() const;

	virtual Ref<AnimationNode> get_child_by_name(const StringName &p_name) const override;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	AnimationNodeBlendTree();
	~AnimationNodeBlendTree();
};

VARIANT_ENUM_CAST(AnimationNodeBlendTree::ConnectionError)

#endif // ANIMATION_BLEND_TREE_H
