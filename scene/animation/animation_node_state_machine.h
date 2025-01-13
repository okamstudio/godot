/**************************************************************************/
/*  animation_node_state_machine.h                                        */
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

#ifndef ANIMATION_NODE_STATE_MACHINE_H
#define ANIMATION_NODE_STATE_MACHINE_H

#include "core/math/expression.h"
#include "scene/animation/animation_tree.h"

class AnimationNodeStateMachineTransition : public Resource {
	GDCLASS(AnimationNodeStateMachineTransition, Resource);

public:
	enum SwitchMode {
		SWITCH_MODE_IMMEDIATE,
		SWITCH_MODE_SYNC,
		SWITCH_MODE_AT_END,
	};

	enum AdvanceMode {
		ADVANCE_MODE_DISABLED,
		ADVANCE_MODE_ENABLED,
		ADVANCE_MODE_AUTO,
	};

private:
	SwitchMode switch_mode = SWITCH_MODE_IMMEDIATE;
	AdvanceMode advance_mode = ADVANCE_MODE_ENABLED;
	StringName advance_condition;
	StringName advance_condition_name;
	float xfade_time = 0.0;
	Ref<Curve> xfade_curve;
	bool break_loop_at_end = false;
	bool reset = true;
	int priority = 1;
	String advance_expression;

	friend class AnimationNodeStateMachinePlayback;
	Ref<Expression> expression;

protected:
	static void _bind_methods();

public:
	void set_switch_mode(SwitchMode p_mode);
	SwitchMode get_switch_mode() const;

	void set_advance_mode(AdvanceMode p_mode);
	AdvanceMode get_advance_mode() const;

	void set_advance_condition(const StringName &p_condition);
	StringName get_advance_condition() const;

	StringName get_advance_condition_name() const;

	void set_advance_expression(const String &p_expression);
	String get_advance_expression() const;

	void set_xfade_time(float p_xfade);
	float get_xfade_time() const;

	void set_break_loop_at_end(bool p_enable);
	bool is_loop_broken_at_end() const;

	void set_reset(bool p_reset);
	bool is_reset() const;

	void set_xfade_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_xfade_curve() const;

	void set_priority(int p_priority);
	int get_priority() const;

	AnimationNodeStateMachineTransition();
};

VARIANT_ENUM_CAST(AnimationNodeStateMachineTransition::SwitchMode)
VARIANT_ENUM_CAST(AnimationNodeStateMachineTransition::AdvanceMode)

class AnimationNodeStateMachinePlayback;

class AnimationNodeStateMachine : public AnimationRootNode {
	GDCLASS(AnimationNodeStateMachine, AnimationRootNode);

public:
	enum StateMachineType {
		STATE_MACHINE_TYPE_ROOT,
		STATE_MACHINE_TYPE_NESTED,
		STATE_MACHINE_TYPE_GROUPED,
	};

private:
	friend class AnimationNodeStateMachinePlayback;

	StateMachineType state_machine_type = STATE_MACHINE_TYPE_ROOT;

	struct State {
		Ref<AnimationRootNode> node;
		Vector2 position;
	};

	HashMap<StringName, State> states;
	bool allow_transition_to_self = false;
	bool reset_ends = false;

	struct Transition {
		StringName from;
		StringName to;
		Ref<AnimationNodeStateMachineTransition> transition;
	};

	Vector<Transition> transitions;

	StringName playback = "playback";
	bool updating_transitions = false;

	Vector2 graph_offset;

	void _remove_transition(const Ref<AnimationNodeStateMachineTransition> p_transition);
	void _rename_transitions(const StringName &p_name, const StringName &p_new_name);
	bool _can_connect(const StringName &p_name);

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;

	bool _check_advance_condition(const Ref<AnimationNodeStateMachine> p_state_machine, const Ref<AnimationNodeStateMachineTransition> p_transition) const;

	virtual void _tree_changed() override;
	virtual void _animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) override;
	virtual void _animation_node_removed(const ObjectID &p_oid, const StringName &p_node) override;

	virtual void reset_state() override;

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const override;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const override;
	virtual bool is_parameter_read_only(const StringName &p_parameter) const override;

	void add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position = Vector2());
	void replace_node(const StringName &p_name, Ref<AnimationNode> p_node);
	Ref<AnimationNode> get_node(const StringName &p_name) const;
	void remove_node(const StringName &p_name);
	void rename_node(const StringName &p_name, const StringName &p_new_name);
	bool has_node(const StringName &p_name) const;
	StringName get_node_name(const Ref<AnimationNode> &p_node) const;
	void get_node_list(List<StringName> *r_nodes) const;

	void set_node_position(const StringName &p_name, const Vector2 &p_position);
	Vector2 get_node_position(const StringName &p_name) const;

	virtual void get_child_nodes(List<ChildNode> *r_child_nodes) override;

	bool has_transition(const StringName &p_from, const StringName &p_to) const;
	bool has_transition_from(const StringName &p_from) const;
	bool has_transition_to(const StringName &p_to) const;
	int find_transition(const StringName &p_from, const StringName &p_to) const;
	Vector<int> find_transition_from(const StringName &p_from) const;
	Vector<int> find_transition_to(const StringName &p_to) const;
	void add_transition(const StringName &p_from, const StringName &p_to, const Ref<AnimationNodeStateMachineTransition> &p_transition);
	Ref<AnimationNodeStateMachineTransition> get_transition(int p_transition) const;
	StringName get_transition_from(int p_transition) const;
	StringName get_transition_to(int p_transition) const;
	int get_transition_count() const;
	bool is_transition_across_group(int p_transition) const;
	void remove_transition_by_index(const int p_transition);
	void remove_transition(const StringName &p_from, const StringName &p_to);

	void set_state_machine_type(StateMachineType p_state_machine_type);
	StateMachineType get_state_machine_type() const;

	void set_allow_transition_to_self(bool p_enable);
	bool is_allow_transition_to_self() const;

	void set_reset_ends(bool p_enable);
	bool are_ends_reset() const;

	bool can_edit_node(const StringName &p_name) const;

	void set_graph_offset(const Vector2 &p_offset);
	Vector2 get_graph_offset() const;

	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;
	virtual String get_caption() const override;

	virtual Ref<AnimationNode> get_child_by_name(const StringName &p_name) const override;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	Vector<StringName> get_nodes_with_transitions_from(const StringName &p_node) const;
	Vector<StringName> get_nodes_with_transitions_to(const StringName &p_node) const;

	AnimationNodeStateMachine();
};

VARIANT_ENUM_CAST(AnimationNodeStateMachine::StateMachineType);

class AnimationNodeStateMachinePlayback : public Resource {
	GDCLASS(AnimationNodeStateMachinePlayback, Resource);

	friend class AnimationNodeStateMachine;

	struct AStarCost {
		float distance = 0.0;
		StringName prev;
	};

	struct TransitionInfo {
		StringName from;
		StringName to;
		StringName next;
	};

	struct NextInfo {
		StringName node;
		double xfade;
		Ref<Curve> curve;
		AnimationNodeStateMachineTransition::SwitchMode switch_mode;
		bool is_reset;
		bool break_loop_at_end;
	};

	struct ChildStateMachineInfo {
		Ref<AnimationNodeStateMachinePlayback> playback;
		Vector<StringName> path;
		bool is_reset = false;
	};

	Ref<AnimationNodeStateMachineTransition> default_transition;
	String base_path;

	AnimationNode::NodeTimeInfo current_nti;
	StringName current;
	Ref<Curve> current_curve;

	Ref<AnimationNodeStateMachineTransition> group_start_transition;
	Ref<AnimationNodeStateMachineTransition> group_end_transition;

	AnimationNode::NodeTimeInfo fadeing_from_nti;
	StringName fading_from;
	float fading_time = 0.0;
	float fading_pos = 0.0;

	Vector<StringName> path;
	bool playing = false;

	StringName start_request;
	StringName travel_request;
	bool reset_request = false;
	bool reset_request_on_teleport = false;
	bool _reset_request_for_fading_from = false;
	bool next_request = false;
	bool stop_request = false;
	bool teleport_request = false;

	bool is_grouped = false;

	void _travel_main(const StringName &p_state, bool p_reset_on_teleport = true);
	void _start_main(const StringName &p_state, bool p_reset = true);
	void _next_main();
	void _stop_main();

	bool _make_travel_path(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_is_allow_transition_to_self, Vector<StringName> &r_path, bool p_test_only);
	String _validate_path(AnimationNodeStateMachine *p_state_machine, const String &p_path);
	bool _travel(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_is_allow_transition_to_self, bool p_test_only);
	void _start(AnimationNodeStateMachine *p_state_machine);

	void _clear_path_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_test_only);
	bool _travel_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const String &p_path, bool p_is_allow_transition_to_self, bool p_is_parent_same_state, bool p_test_only);
	void _start_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const String &p_path, bool p_test_only);

	AnimationNode::NodeTimeInfo process(const String &p_base_path, AnimationNodeStateMachine *p_state_machine, const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only);
	AnimationNode::NodeTimeInfo _process(const String &p_base_path, AnimationNodeStateMachine *p_state_machine, const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only);

	bool _check_advance_condition(const Ref<AnimationNodeStateMachine> p_state_machine, const Ref<AnimationNodeStateMachineTransition> p_transition) const;
	bool _transition_to_next_recursive(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, double p_delta, bool p_test_only);
	NextInfo _find_next(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine) const;
	Ref<AnimationNodeStateMachineTransition> _check_group_transition(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const AnimationNodeStateMachine::Transition &p_transition, Ref<AnimationNodeStateMachine> &r_state_machine, bool &r_bypass) const;
	bool _can_transition_to_next(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, NextInfo p_next, bool p_test_only);

	void _set_current(AnimationNodeStateMachine *p_state_machine, const StringName &p_state);
	void _set_grouped(bool p_is_grouped);
	void _set_base_path(const String &p_base_path);
	Ref<AnimationNodeStateMachinePlayback> _get_parent_playback(AnimationTree *p_tree) const;
	Ref<AnimationNodeStateMachine> _get_parent_state_machine(AnimationTree *p_tree) const;
	Ref<AnimationNodeStateMachineTransition> _get_group_start_transition() const;
	Ref<AnimationNodeStateMachineTransition> _get_group_end_transition() const;

	TypedArray<StringName> _get_travel_path() const;

protected:
	static void _bind_methods();

public:
	void travel(const StringName &p_state, bool p_reset_on_teleport = true);
	void start(const StringName &p_state, bool p_reset = true);
	void next();
	void stop();
	bool is_playing() const;
	bool is_end() const;
	StringName get_current_node() const;
	StringName get_fading_from_node() const;
	Vector<StringName> get_travel_path() const;
	float get_current_play_pos() const;
	float get_current_length() const;

	float get_fade_from_play_pos() const;
	float get_fade_from_length() const;

	float get_fading_time() const;
	float get_fading_pos() const;

	void clear_path();
	void push_path(const StringName &p_state);

	AnimationNodeStateMachinePlayback();
};

#endif // ANIMATION_NODE_STATE_MACHINE_H
