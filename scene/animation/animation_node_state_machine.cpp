/**************************************************************************/
/*  animation_node_state_machine.cpp                                      */
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

#include "animation_node_state_machine.h"
#include "scene/main/window.h"

/////////////////////////////////////////////////

void AnimationNodeStateMachineTransition::set_switch_mode(SwitchMode p_mode) {
	switch_mode = p_mode;
}

AnimationNodeStateMachineTransition::SwitchMode AnimationNodeStateMachineTransition::get_switch_mode() const {
	return switch_mode;
}

void AnimationNodeStateMachineTransition::set_advance_mode(AdvanceMode p_mode) {
	advance_mode = p_mode;
}

AnimationNodeStateMachineTransition::AdvanceMode AnimationNodeStateMachineTransition::get_advance_mode() const {
	return advance_mode;
}

void AnimationNodeStateMachineTransition::set_advance_condition(const StringName &p_condition) {
	String cs = p_condition;
	ERR_FAIL_COND(cs.contains_char('/') || cs.contains_char(':'));
	advance_condition = p_condition;
	if (!cs.is_empty()) {
		advance_condition_name = "conditions/" + cs;
	} else {
		advance_condition_name = StringName();
	}
	emit_signal(SNAME("advance_condition_changed"));
}

StringName AnimationNodeStateMachineTransition::get_advance_condition() const {
	return advance_condition;
}

StringName AnimationNodeStateMachineTransition::get_advance_condition_name() const {
	return advance_condition_name;
}

void AnimationNodeStateMachineTransition::set_advance_expression(const String &p_expression) {
	advance_expression = p_expression;

	String advance_expression_stripped = advance_expression.strip_edges();
	if (advance_expression_stripped == String()) {
		expression.unref();
		return;
	}

	if (expression.is_null()) {
		expression.instantiate();
	}

	expression->parse(advance_expression_stripped);
}

String AnimationNodeStateMachineTransition::get_advance_expression() const {
	return advance_expression;
}

void AnimationNodeStateMachineTransition::set_xfade_time(float p_xfade) {
	ERR_FAIL_COND(p_xfade < 0);
	xfade_time = p_xfade;
	emit_changed();
}

float AnimationNodeStateMachineTransition::get_xfade_time() const {
	return xfade_time;
}

void AnimationNodeStateMachineTransition::set_xfade_curve(const Ref<Curve> &p_curve) {
	xfade_curve = p_curve;
	emit_changed();
}

Ref<Curve> AnimationNodeStateMachineTransition::get_xfade_curve() const {
	return xfade_curve;
}

void AnimationNodeStateMachineTransition::set_break_loop_at_end(bool p_enable) {
	break_loop_at_end = p_enable;
	emit_changed();
}

bool AnimationNodeStateMachineTransition::is_loop_broken_at_end() const {
	return break_loop_at_end;
}

void AnimationNodeStateMachineTransition::set_reset(bool p_reset) {
	reset = p_reset;
	emit_changed();
}

bool AnimationNodeStateMachineTransition::is_reset() const {
	return reset;
}

void AnimationNodeStateMachineTransition::set_priority(int p_priority) {
	priority = p_priority;
	emit_changed();
}

int AnimationNodeStateMachineTransition::get_priority() const {
	return priority;
}

void AnimationNodeStateMachineTransition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_switch_mode", "mode"), &AnimationNodeStateMachineTransition::set_switch_mode);
	ClassDB::bind_method(D_METHOD("get_switch_mode"), &AnimationNodeStateMachineTransition::get_switch_mode);

	ClassDB::bind_method(D_METHOD("set_advance_mode", "mode"), &AnimationNodeStateMachineTransition::set_advance_mode);
	ClassDB::bind_method(D_METHOD("get_advance_mode"), &AnimationNodeStateMachineTransition::get_advance_mode);

	ClassDB::bind_method(D_METHOD("set_advance_condition", "name"), &AnimationNodeStateMachineTransition::set_advance_condition);
	ClassDB::bind_method(D_METHOD("get_advance_condition"), &AnimationNodeStateMachineTransition::get_advance_condition);

	ClassDB::bind_method(D_METHOD("set_xfade_time", "secs"), &AnimationNodeStateMachineTransition::set_xfade_time);
	ClassDB::bind_method(D_METHOD("get_xfade_time"), &AnimationNodeStateMachineTransition::get_xfade_time);

	ClassDB::bind_method(D_METHOD("set_xfade_curve", "curve"), &AnimationNodeStateMachineTransition::set_xfade_curve);
	ClassDB::bind_method(D_METHOD("get_xfade_curve"), &AnimationNodeStateMachineTransition::get_xfade_curve);

	ClassDB::bind_method(D_METHOD("set_break_loop_at_end", "enable"), &AnimationNodeStateMachineTransition::set_break_loop_at_end);
	ClassDB::bind_method(D_METHOD("is_loop_broken_at_end"), &AnimationNodeStateMachineTransition::is_loop_broken_at_end);

	ClassDB::bind_method(D_METHOD("set_reset", "reset"), &AnimationNodeStateMachineTransition::set_reset);
	ClassDB::bind_method(D_METHOD("is_reset"), &AnimationNodeStateMachineTransition::is_reset);

	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &AnimationNodeStateMachineTransition::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &AnimationNodeStateMachineTransition::get_priority);

	ClassDB::bind_method(D_METHOD("set_advance_expression", "text"), &AnimationNodeStateMachineTransition::set_advance_expression);
	ClassDB::bind_method(D_METHOD("get_advance_expression"), &AnimationNodeStateMachineTransition::get_advance_expression);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "xfade_time", PROPERTY_HINT_RANGE, "0,240,0.01,suffix:s"), "set_xfade_time", "get_xfade_time");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "xfade_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_xfade_curve", "get_xfade_curve");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "break_loop_at_end"), "set_break_loop_at_end", "is_loop_broken_at_end");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reset"), "set_reset", "is_reset");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority", PROPERTY_HINT_RANGE, "0,32,1"), "set_priority", "get_priority");
	ADD_GROUP("Switch", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "switch_mode", PROPERTY_HINT_ENUM, "Immediate,Sync,At End"), "set_switch_mode", "get_switch_mode");
	ADD_GROUP("Advance", "advance_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "advance_mode", PROPERTY_HINT_ENUM, "Disabled,Enabled,Auto"), "set_advance_mode", "get_advance_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "advance_condition"), "set_advance_condition", "get_advance_condition");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "advance_expression", PROPERTY_HINT_EXPRESSION, ""), "set_advance_expression", "get_advance_expression");

	BIND_ENUM_CONSTANT(SWITCH_MODE_IMMEDIATE);
	BIND_ENUM_CONSTANT(SWITCH_MODE_SYNC);
	BIND_ENUM_CONSTANT(SWITCH_MODE_AT_END);

	BIND_ENUM_CONSTANT(ADVANCE_MODE_DISABLED);
	BIND_ENUM_CONSTANT(ADVANCE_MODE_ENABLED);
	BIND_ENUM_CONSTANT(ADVANCE_MODE_AUTO);

	ADD_SIGNAL(MethodInfo("advance_condition_changed"));
}

AnimationNodeStateMachineTransition::AnimationNodeStateMachineTransition() {
}

////////////////////////////////////////////////////////

void AnimationNodeStateMachinePlayback::_set_current(AnimationNodeStateMachine *p_state_machine, const StringName &p_state) {
	current = p_state;
	if (current == StringName()) {
		group_start_transition = Ref<AnimationNodeStateMachineTransition>();
		group_end_transition = Ref<AnimationNodeStateMachineTransition>();
		return;
	}

	Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(current);
	if (!anodesm.is_valid()) {
		group_start_transition = Ref<AnimationNodeStateMachineTransition>();
		group_end_transition = Ref<AnimationNodeStateMachineTransition>();
		return;
	}

	Vector<int> indices = p_state_machine->find_transition_to(current);
	int group_start_size = indices.size();
	if (group_start_size) {
		group_start_transition = p_state_machine->get_transition(indices[0]);
	} else {
		group_start_transition = Ref<AnimationNodeStateMachineTransition>();
	}

	indices = p_state_machine->find_transition_from(current);
	int group_end_size = indices.size();
	if (group_end_size) {
		group_end_transition = p_state_machine->get_transition(indices[0]);
	} else {
		group_end_transition = Ref<AnimationNodeStateMachineTransition>();
	}

	// Validation.
	if (anodesm->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		indices = anodesm->find_transition_from(SceneStringName(Start));
		int anodesm_start_size = indices.size();
		indices = anodesm->find_transition_to(SceneStringName(End));
		int anodesm_end_size = indices.size();
		if (group_start_size > 1) {
			WARN_PRINT_ED("There are two or more transitions to the Grouped AnimationNodeStateMachine in AnimationNodeStateMachine: " + base_path + ", which may result in unintended transitions.");
		}
		if (group_end_size > 1) {
			WARN_PRINT_ED("There are two or more transitions from the Grouped AnimationNodeStateMachine in AnimationNodeStateMachine: " + base_path + ", which may result in unintended transitions.");
		}
		if (anodesm_start_size > 1) {
			WARN_PRINT_ED("There are two or more transitions from the Start of Grouped AnimationNodeStateMachine in AnimationNodeStateMachine: " + base_path + current + ", which may result in unintended transitions.");
		}
		if (anodesm_end_size > 1) {
			WARN_PRINT_ED("There are two or more transitions to the End of Grouped AnimationNodeStateMachine in AnimationNodeStateMachine: " + base_path + current + ", which may result in unintended transitions.");
		}
		if (anodesm_start_size != group_start_size) {
			ERR_PRINT_ED("There is a mismatch in the number of start transitions in and out of the Grouped AnimationNodeStateMachine on AnimationNodeStateMachine: " + base_path + current + ".");
		}
		if (anodesm_end_size != group_end_size) {
			ERR_PRINT_ED("There is a mismatch in the number of end transitions in and out of the Grouped AnimationNodeStateMachine on AnimationNodeStateMachine: " + base_path + current + ".");
		}
	}
}

void AnimationNodeStateMachinePlayback::_set_grouped(bool p_is_grouped) {
	is_grouped = p_is_grouped;
}

void AnimationNodeStateMachinePlayback::travel(const StringName &p_state, bool p_reset_on_teleport) {
	ERR_FAIL_COND_EDMSG(is_grouped, "Grouped AnimationNodeStateMachinePlayback must be handled by parent AnimationNodeStateMachinePlayback. You need to retrieve the parent Root/Nested AnimationNodeStateMachine.");
	ERR_FAIL_COND_EDMSG(String(p_state).contains("/Start") || String(p_state).contains("/End"), "Grouped AnimationNodeStateMachinePlayback doesn't allow to play Start/End directly. Instead, play the prev or next state of group in the parent AnimationNodeStateMachine.");
	_travel_main(p_state, p_reset_on_teleport);
}

void AnimationNodeStateMachinePlayback::start(const StringName &p_state, bool p_reset) {
	ERR_FAIL_COND_EDMSG(is_grouped, "Grouped AnimationNodeStateMachinePlayback must be handled by parent AnimationNodeStateMachinePlayback. You need to retrieve the parent Root/Nested AnimationNodeStateMachine.");
	ERR_FAIL_COND_EDMSG(String(p_state).contains("/Start") || String(p_state).contains("/End"), "Grouped AnimationNodeStateMachinePlayback doesn't allow to play Start/End directly. Instead, play the prev or next state of group in the parent AnimationNodeStateMachine.");
	_start_main(p_state, p_reset);
}

void AnimationNodeStateMachinePlayback::next() {
	ERR_FAIL_COND_EDMSG(is_grouped, "Grouped AnimationNodeStateMachinePlayback must be handled by parent AnimationNodeStateMachinePlayback. You need to retrieve the parent Root/Nested AnimationNodeStateMachine.");
	_next_main();
}

void AnimationNodeStateMachinePlayback::stop() {
	ERR_FAIL_COND_EDMSG(is_grouped, "Grouped AnimationNodeStateMachinePlayback must be handled by parent AnimationNodeStateMachinePlayback. You need to retrieve the parent Root/Nested AnimationNodeStateMachine.");
	_stop_main();
}

void AnimationNodeStateMachinePlayback::_travel_main(const StringName &p_state, bool p_reset_on_teleport) {
	travel_request = p_state;
	reset_request_on_teleport = p_reset_on_teleport;
	stop_request = false;
}

void AnimationNodeStateMachinePlayback::_start_main(const StringName &p_state, bool p_reset) {
	travel_request = StringName();
	path.clear();
	reset_request = p_reset;
	start_request = p_state;
	stop_request = false;
}

void AnimationNodeStateMachinePlayback::_next_main() {
	next_request = true;
}

void AnimationNodeStateMachinePlayback::_stop_main() {
	stop_request = true;
}

bool AnimationNodeStateMachinePlayback::is_playing() const {
	return playing;
}

bool AnimationNodeStateMachinePlayback::is_end() const {
	return current == SceneStringName(End) && fading_from == StringName();
}

StringName AnimationNodeStateMachinePlayback::get_current_node() const {
	return current;
}

StringName AnimationNodeStateMachinePlayback::get_fading_from_node() const {
	return fading_from;
}

Vector<StringName> AnimationNodeStateMachinePlayback::get_travel_path() const {
	return path;
}

TypedArray<StringName> AnimationNodeStateMachinePlayback::_get_travel_path() const {
	return Variant(get_travel_path()).operator Array();
}

float AnimationNodeStateMachinePlayback::get_current_play_pos() const {
	return current_nti.position;
}

float AnimationNodeStateMachinePlayback::get_current_length() const {
	return current_nti.length;
}

float AnimationNodeStateMachinePlayback::get_fade_from_play_pos() const {
	return fadeing_from_nti.position;
}

float AnimationNodeStateMachinePlayback::get_fade_from_length() const {
	return fadeing_from_nti.length;
}

float AnimationNodeStateMachinePlayback::get_fading_time() const {
	return fading_time;
}

float AnimationNodeStateMachinePlayback::get_fading_pos() const {
	return fading_pos;
}

void AnimationNodeStateMachinePlayback::_clear_path_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_test_only) {
	List<AnimationNode::ChildNode> child_nodes;
	p_state_machine->get_child_nodes(&child_nodes);
	for (const AnimationNode::ChildNode &child_node : child_nodes) {
		Ref<AnimationNodeStateMachine> anodesm = child_node.node;
		if (anodesm.is_valid() && anodesm->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
			Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(base_path + child_node.name + "/playback");
			ERR_FAIL_COND(!playback.is_valid());
			playback->_set_base_path(base_path + child_node.name + "/");
			if (p_test_only) {
				playback = playback->duplicate();
			}
			playback->path.clear();
			playback->_clear_path_children(p_tree, anodesm.ptr(), p_test_only);
			if (current != child_node.name) {
				playback->_start(anodesm.ptr()); // Can restart.
			}
		}
	}
}

void AnimationNodeStateMachinePlayback::_start_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const String &p_path, bool p_test_only) {
	if (p_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		return; // This function must be fired only by the top state machine, do nothing in child state machine.
	}
	Vector<String> temp_path = p_path.split("/");
	if (temp_path.size() > 1) {
		for (int i = 1; i < temp_path.size(); i++) {
			String concatenated;
			for (int j = 0; j < i; j++) {
				concatenated += temp_path[j] + (j == i - 1 ? "" : "/");
			}
			Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(concatenated);
			if (anodesm.is_valid() && anodesm->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
				ERR_FAIL_MSG("Root/Nested AnimationNodeStateMachine can't have path from parent AnimationNodeStateMachine.");
			}
			Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(base_path + concatenated + "/playback");
			ERR_FAIL_COND(!playback.is_valid());
			playback->_set_base_path(base_path + concatenated + "/");
			if (p_test_only) {
				playback = playback->duplicate();
			}
			playback->_start_main(temp_path[i], i == temp_path.size() - 1 ? reset_request : false);
		}
		reset_request = false;
	}
}

bool AnimationNodeStateMachinePlayback::_travel_children(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const String &p_path, bool p_is_allow_transition_to_self, bool p_is_parent_same_state, bool p_test_only) {
	if (p_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		return false; // This function must be fired only by the top state machine, do nothing in child state machine.
	}
	Vector<String> temp_path = p_path.split("/");
	Vector<ChildStateMachineInfo> children;

	bool found_route = true;
	bool is_parent_same_state = p_is_parent_same_state;
	if (temp_path.size() > 1) {
		for (int i = 1; i < temp_path.size(); i++) {
			String concatenated;
			for (int j = 0; j < i; j++) {
				concatenated += temp_path[j] + (j == i - 1 ? "" : "/");
			}

			Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(concatenated);
			if (anodesm.is_valid() && anodesm->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
				ERR_FAIL_V_MSG(false, "Root/Nested AnimationNodeStateMachine can't have path from parent AnimationNodeStateMachine.");
			}
			Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(base_path + concatenated + "/playback");
			ERR_FAIL_COND_V(!playback.is_valid(), false);
			playback->_set_base_path(base_path + concatenated + "/");
			if (p_test_only) {
				playback = playback->duplicate();
			}
			if (!playback->is_playing()) {
				playback->_start(anodesm.ptr());
			}
			ChildStateMachineInfo child_info;
			child_info.playback = playback;

			// Process for the case that parent state is changed.
			bool child_found_route = true;
			bool is_current_same_state = temp_path[i] == playback->get_current_node();
			if (!is_parent_same_state) {
				// Force travel to end current child state machine.
				String child_path = "/" + playback->get_current_node();
				while (true) {
					Ref<AnimationNodeStateMachine> child_anodesm = p_state_machine->find_node_by_path(concatenated + child_path);
					if (!child_anodesm.is_valid() || child_anodesm->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
						break;
					}
					Ref<AnimationNodeStateMachinePlayback> child_playback = p_tree->get(base_path + concatenated + child_path + "/playback");
					ERR_FAIL_COND_V(!child_playback.is_valid(), false);
					child_playback->_set_base_path(base_path + concatenated + "/");
					if (p_test_only) {
						child_playback = child_playback->duplicate();
					}
					child_playback->_travel_main(SceneStringName(End));
					child_found_route &= child_playback->_travel(p_tree, child_anodesm.ptr(), false, p_test_only);
					child_path += "/" + child_playback->get_current_node();
				}
				// Force restart target state machine.
				playback->_start(anodesm.ptr());
			}
			is_parent_same_state = is_current_same_state;

			bool is_deepest_state = i == temp_path.size() - 1;
			child_info.is_reset = is_deepest_state ? reset_request_on_teleport : false;
			playback->_travel_main(temp_path[i], child_info.is_reset);
			if (playback->_make_travel_path(p_tree, anodesm.ptr(), is_deepest_state ? p_is_allow_transition_to_self : false, child_info.path, p_test_only)) {
				found_route &= child_found_route;
			} else {
				child_info.path.push_back(temp_path[i]);
				found_route = false;
			}
			children.push_back(child_info);
		}
		reset_request_on_teleport = false;
	}

	if (found_route) {
		for (int i = 0; i < children.size(); i++) {
			children.write[i].playback->clear_path();
			for (int j = 0; j < children[i].path.size(); j++) {
				children.write[i].playback->push_path(children[i].path[j]);
			}
		}
	} else {
		for (int i = 0; i < children.size(); i++) {
			children.write[i].playback->_travel_main(StringName(), children[i].is_reset); // Clear travel.
			if (children[i].path.size()) {
				children.write[i].playback->_start_main(children[i].path[children[i].path.size() - 1], children[i].is_reset);
			}
		}
	}
	return found_route;
}

void AnimationNodeStateMachinePlayback::_start(AnimationNodeStateMachine *p_state_machine) {
	playing = true;
	_set_current(p_state_machine, start_request != StringName() ? start_request : SceneStringName(Start));
	teleport_request = true;
	stop_request = false;
	start_request = StringName();
}

bool AnimationNodeStateMachinePlayback::_travel(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_is_allow_transition_to_self, bool p_test_only) {
	return _make_travel_path(p_tree, p_state_machine, p_is_allow_transition_to_self, path, p_test_only);
}

String AnimationNodeStateMachinePlayback::_validate_path(AnimationNodeStateMachine *p_state_machine, const String &p_path) {
	if (p_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		return p_path; // Grouped state machine doesn't allow validat-able request.
	}
	String target = p_path;
	Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(target);
	while (anodesm.is_valid() && anodesm->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		Vector<int> indices = anodesm->find_transition_from(SceneStringName(Start));
		if (indices.size()) {
			target = target + "/" + anodesm->get_transition_to(indices[0]); // Find next state of Start.
		} else {
			break; // There is no transition in Start state of grouped state machine.
		}
		anodesm = p_state_machine->find_node_by_path(target);
	}
	return target;
}

bool AnimationNodeStateMachinePlayback::_make_travel_path(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, bool p_is_allow_transition_to_self, Vector<StringName> &r_path, bool p_test_only) {
	StringName travel = travel_request;
	travel_request = StringName();

	if (!playing) {
		_start(p_state_machine);
	}

	ERR_FAIL_COND_V(!p_state_machine->states.has(travel), false);
	ERR_FAIL_COND_V(!p_state_machine->states.has(current), false);

	if (current == travel) {
		return !p_is_allow_transition_to_self;
	}

	Vector<StringName> new_path;

	Vector2 current_pos = p_state_machine->states[current].position;
	Vector2 target_pos = p_state_machine->states[travel].position;

	bool found_route = false;
	HashMap<StringName, AStarCost> cost_map;

	List<int> open_list;

	// Build open list.
	for (int i = 0; i < p_state_machine->transitions.size(); i++) {
		if (p_state_machine->transitions[i].transition->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED) {
			continue;
		}

		if (p_state_machine->transitions[i].from == current) {
			open_list.push_back(i);
			float cost = p_state_machine->states[p_state_machine->transitions[i].to].position.distance_to(current_pos);
			cost *= p_state_machine->transitions[i].transition->get_priority();
			AStarCost ap;
			ap.prev = current;
			ap.distance = cost;
			cost_map[p_state_machine->transitions[i].to] = ap;

			if (p_state_machine->transitions[i].to == travel) { // Prematurely found it! :D
				found_route = true;
				break;
			}
		}
	}

	// Begin astar.
	while (!found_route) {
		if (open_list.size() == 0) {
			break; // No path found.
		}

		// Find the last cost transition.
		List<int>::Element *least_cost_transition = nullptr;
		float least_cost = 1e20;

		for (List<int>::Element *E = open_list.front(); E; E = E->next()) {
			float cost = cost_map[p_state_machine->transitions[E->get()].to].distance;
			cost += p_state_machine->states[p_state_machine->transitions[E->get()].to].position.distance_to(target_pos);

			if (cost < least_cost) {
				least_cost_transition = E;
				least_cost = cost;
			}
		}

		StringName transition_prev = p_state_machine->transitions[least_cost_transition->get()].from;
		StringName transition = p_state_machine->transitions[least_cost_transition->get()].to;

		for (int i = 0; i < p_state_machine->transitions.size(); i++) {
			if (p_state_machine->transitions[i].transition->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED) {
				continue;
			}

			if (p_state_machine->transitions[i].from != transition || p_state_machine->transitions[i].to == transition_prev) {
				continue; // Not interested on those.
			}

			float distance = p_state_machine->states[p_state_machine->transitions[i].from].position.distance_to(p_state_machine->states[p_state_machine->transitions[i].to].position);
			distance *= p_state_machine->transitions[i].transition->get_priority();
			distance += cost_map[p_state_machine->transitions[i].from].distance;

			if (cost_map.has(p_state_machine->transitions[i].to)) {
				// Oh this was visited already, can we win the cost?
				if (distance < cost_map[p_state_machine->transitions[i].to].distance) {
					cost_map[p_state_machine->transitions[i].to].distance = distance;
					cost_map[p_state_machine->transitions[i].to].prev = p_state_machine->transitions[i].from;
				}
			} else {
				// Add to open list.
				AStarCost ac;
				ac.prev = p_state_machine->transitions[i].from;
				ac.distance = distance;
				cost_map[p_state_machine->transitions[i].to] = ac;

				open_list.push_back(i);

				if (p_state_machine->transitions[i].to == travel) {
					found_route = true;
					break;
				}
			}
		}

		if (found_route) {
			break;
		}

		open_list.erase(least_cost_transition);
	}

	// Check child grouped state machine.
	if (found_route) {
		// Make path.
		StringName at = travel;
		while (at != current) {
			new_path.push_back(at);
			at = cost_map[at].prev;
		}
		new_path.reverse();

		// Check internal paths of child grouped state machine.
		// For example:
		// [current - End] - [Start - End] - [Start - End] - [Start - target]
		String current_path = current;
		int len = new_path.size() + 1;
		for (int i = 0; i < len; i++) {
			Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(current_path);
			if (anodesm.is_valid() && anodesm->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
				Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(base_path + current_path + "/playback");
				ERR_FAIL_COND_V(!playback.is_valid(), false);
				playback->_set_base_path(base_path + current_path + "/");
				if (p_test_only) {
					playback = playback->duplicate();
				}
				if (i > 0) {
					playback->_start(anodesm.ptr());
				}
				if (i >= new_path.size()) {
					break; // Tracing has been finished, needs to break.
				}
				playback->_travel_main(SceneStringName(End));
				if (!playback->_travel(p_tree, anodesm.ptr(), false, p_test_only)) {
					found_route = false;
					break;
				}
			}
			if (i >= new_path.size()) {
				break; // Tracing has been finished, needs to break.
			}
			current_path = new_path[i];
		}
	}

	// Finally, rewrite path if route is found.
	if (found_route) {
		r_path = new_path;
		return true;
	} else {
		return false;
	}
}

AnimationNode::NodeTimeInfo AnimationNodeStateMachinePlayback::process(const String &p_base_path, AnimationNodeStateMachine *p_state_machine, const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	AnimationNode::NodeTimeInfo nti = _process(p_base_path, p_state_machine, p_playback_info, p_test_only);
	start_request = StringName();
	next_request = false;
	stop_request = false;
	reset_request_on_teleport = false;
	return nti;
}

AnimationNode::NodeTimeInfo AnimationNodeStateMachinePlayback::_process(const String &p_base_path, AnimationNodeStateMachine *p_state_machine, const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	_set_base_path(p_base_path);

	AnimationTree *tree = p_state_machine->process_state->tree;

	double p_time = p_playback_info.time;
	double p_delta = p_playback_info.delta;
	bool p_seek = p_playback_info.seeked;
	bool p_is_external_seeking = p_playback_info.is_external_seeking;

	// Check seek to 0 (means reset) by parent AnimationNode.
	if (Math::is_zero_approx(p_time) && p_seek && !p_is_external_seeking) {
		if (p_state_machine->state_machine_type != AnimationNodeStateMachine::STATE_MACHINE_TYPE_NESTED || is_end() || !playing) {
			// Restart state machine.
			if (p_state_machine->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
				path.clear();
				_clear_path_children(tree, p_state_machine, p_test_only);
			}
			_start(p_state_machine);
			reset_request = true;
		} else {
			// Reset current state.
			reset_request = true;
			teleport_request = true;
		}
	}

	if (stop_request) {
		start_request = StringName();
		travel_request = StringName();
		path.clear();
		playing = false;
		return AnimationNode::NodeTimeInfo();
	}

	if (!playing && start_request != StringName() && travel_request != StringName()) {
		return AnimationNode::NodeTimeInfo();
	}

	// Process start/travel request.
	if (start_request != StringName() || travel_request != StringName()) {
		if (p_state_machine->get_state_machine_type() != AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
			_clear_path_children(tree, p_state_machine, p_test_only);
		}
	}

	if (start_request != StringName()) {
		path.clear();
		String start_target = _validate_path(p_state_machine, start_request);
		Vector<String> start_path = String(start_target).split("/");
		start_request = start_path[0];
		if (start_path.size()) {
			_start_children(tree, p_state_machine, start_target, p_test_only);
		}
		// Teleport to start.
		if (p_state_machine->states.has(start_request)) {
			_start(p_state_machine);
		} else {
			StringName node = start_request;
			ERR_FAIL_V_MSG(AnimationNode::NodeTimeInfo(), "No such node: '" + node + "'");
		}
	}

	if (travel_request != StringName()) {
		// Fix path.
		String travel_target = _validate_path(p_state_machine, travel_request);
		Vector<String> travel_path = travel_target.split("/");
		travel_request = travel_path[0];
		StringName temp_travel_request = travel_request; // For the case that can't travel.
		// Process children.
		Vector<StringName> new_path;
		bool can_travel = _make_travel_path(tree, p_state_machine, travel_path.size() <= 1 ? p_state_machine->is_allow_transition_to_self() : false, new_path, p_test_only);
		if (travel_path.size()) {
			if (can_travel) {
				can_travel = _travel_children(tree, p_state_machine, travel_target, p_state_machine->is_allow_transition_to_self(), travel_path[0] == current, p_test_only);
			} else {
				_start_children(tree, p_state_machine, travel_target, p_test_only);
			}
		}

		// Process to travel.
		if (can_travel) {
			path = new_path;
		} else {
			// Can't travel, then teleport.
			if (p_state_machine->states.has(temp_travel_request)) {
				path.clear();
				if (current != temp_travel_request || reset_request_on_teleport) {
					_set_current(p_state_machine, temp_travel_request);
					reset_request = reset_request_on_teleport;
					teleport_request = true;
				}
			} else {
				ERR_FAIL_V_MSG(AnimationNode::NodeTimeInfo(), "No such node: '" + temp_travel_request + "'");
			}
		}
	}

	AnimationMixer::PlaybackInfo pi = p_playback_info;

	if (teleport_request) {
		teleport_request = false;
		// Clear fadeing on teleport.
		fading_from = StringName();
		fadeing_from_nti = AnimationNode::NodeTimeInfo();
		fading_pos = 0;
		// Init current length.
		pi.time = 0;
		pi.seeked = true;
		pi.is_external_seeking = false;
		pi.weight = 0;
		current_nti = p_state_machine->blend_node(p_state_machine->states[current].node, current, pi, AnimationNode::FILTER_IGNORE, true, true);
		// Don't process first node if not necessary, insteads process next node.
		_transition_to_next_recursive(tree, p_state_machine, p_delta, p_test_only);
	}

	// Check current node existence.
	if (!p_state_machine->states.has(current)) {
		playing = false; // Current does not exist.
		_set_current(p_state_machine, StringName());
		return AnimationNode::NodeTimeInfo();
	}

	// Special case for grouped state machine Start/End to make priority with parent blend (means don't treat Start and End states as RESET animations).
	bool is_start_of_group = false;
	bool is_end_of_group = false;
	if (!p_state_machine->are_ends_reset() || p_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		is_start_of_group = fading_from == SceneStringName(Start);
		is_end_of_group = current == SceneStringName(End);
	}

	// Calc blend amount by cross-fade.
	float fade_blend = 1.0;
	if (fading_time && fading_from != StringName()) {
		if (!p_state_machine->states.has(fading_from)) {
			fading_from = StringName();
		} else {
			if (!p_seek) {
				fading_pos += Math::abs(p_delta);
			}
			fade_blend = MIN(1.0, fading_pos / fading_time);
		}
	}
	if (current_curve.is_valid()) {
		fade_blend = current_curve->sample(fade_blend);
	}
	fade_blend = Math::is_zero_approx(fade_blend) ? CMP_EPSILON : fade_blend;
	if (is_start_of_group) {
		fade_blend = 1.0;
	} else if (is_end_of_group) {
		fade_blend = 0.0;
	}

	// Main process.
	pi = p_playback_info;
	pi.weight = fade_blend;
	if (reset_request) {
		reset_request = false;
		pi.time = 0;
		pi.seeked = true;
	}
	current_nti = p_state_machine->blend_node(p_state_machine->states[current].node, current, pi, AnimationNode::FILTER_IGNORE, true, p_test_only); // Blend values must be more than CMP_EPSILON to process discrete keys in edge.

	// Cross-fade process.
	if (fading_from != StringName()) {
		double fade_blend_inv = 1.0 - fade_blend;
		fade_blend_inv = Math::is_zero_approx(fade_blend_inv) ? CMP_EPSILON : fade_blend_inv;
		if (is_start_of_group) {
			fade_blend_inv = 0.0;
		} else if (is_end_of_group) {
			fade_blend_inv = 1.0;
		}

		pi = p_playback_info;
		pi.weight = fade_blend_inv;
		if (_reset_request_for_fading_from) {
			_reset_request_for_fading_from = false;
			pi.time = 0;
			pi.seeked = true;
		}
		fadeing_from_nti = p_state_machine->blend_node(p_state_machine->states[fading_from].node, fading_from, pi, AnimationNode::FILTER_IGNORE, true, p_test_only); // Blend values must be more than CMP_EPSILON to process discrete keys in edge.

		if (Animation::is_greater_or_equal_approx(fading_pos, fading_time)) {
			// Finish fading.
			fading_from = StringName();
			fadeing_from_nti = AnimationNode::NodeTimeInfo();
		}
	}

	// Find next and see when to transition.
	bool will_end = _transition_to_next_recursive(tree, p_state_machine, p_delta, p_test_only) || current == SceneStringName(End);

	// Predict remaining time.
	if (will_end || ((p_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_NESTED) && !p_state_machine->has_transition_from(current))) {
		// There is no next transition.
		if (fading_from != StringName()) {
			return Animation::is_greater_approx(current_nti.get_remain(), fadeing_from_nti.get_remain()) ? current_nti : fadeing_from_nti;
		}
		return current_nti;
	}

	if (!is_end()) {
		current_nti.is_infinity = true;
	}

	return current_nti;
}

bool AnimationNodeStateMachinePlayback::_transition_to_next_recursive(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, double p_delta, bool p_test_only) {
	_reset_request_for_fading_from = false;

	AnimationMixer::PlaybackInfo pi;
	pi.delta = p_delta;
	NextInfo next;
	Vector<StringName> transition_path;
	transition_path.push_back(current);
	while (true) {
		next = _find_next(p_tree, p_state_machine);

		if (!_can_transition_to_next(p_tree, p_state_machine, next, p_test_only)) {
			break; // Finish transition.
		}

		if (transition_path.has(next.node)) {
			WARN_PRINT_ONCE_ED("AnimationNodeStateMachinePlayback: " + base_path + "playback aborts the transition by detecting one or more looped transitions in the same frame to prevent to infinity loop. You may need to check the transition settings.");
			break; // Maybe infinity loop, do nothing more.
		}

		transition_path.push_back(next.node);

		// Setting for fading.
		if (next.xfade) {
			// Time to fade.
			fading_from = current;
			fading_time = next.xfade;
			fading_pos = 0;
		} else {
			if (reset_request) {
				// There is no possibility of processing doubly. Now we can apply reset actually in here.
				pi.time = 0;
				pi.seeked = true;
				pi.is_external_seeking = false;
				pi.weight = 0;
				p_state_machine->blend_node(p_state_machine->states[current].node, current, pi, AnimationNode::FILTER_IGNORE, true, p_test_only);
			}
			fading_from = StringName();
			fadeing_from_nti = AnimationNode::NodeTimeInfo();
			fading_time = 0;
			fading_pos = 0;
		}

		// If it came from path, remove path.
		if (path.size()) {
			path.remove_at(0);
		}

		// Update current status.
		_set_current(p_state_machine, next.node);
		current_curve = next.curve;

		if (current == SceneStringName(End)) {
			break;
		}

		_reset_request_for_fading_from = reset_request; // To avoid processing doubly, it must be reset in the fading process within _process().
		reset_request = next.is_reset;

		fadeing_from_nti = current_nti;

		if (next.switch_mode == AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC) {
			pi.time = current_nti.position;
			pi.seeked = true;
			pi.is_external_seeking = false;
			pi.weight = 0;
			p_state_machine->blend_node(p_state_machine->states[current].node, current, pi, AnimationNode::FILTER_IGNORE, true, p_test_only);
		}

		// Just get length to find next recursive.
		pi.time = 0;
		pi.is_external_seeking = false;
		pi.weight = 0;
		pi.seeked = next.is_reset;
		current_nti = p_state_machine->blend_node(p_state_machine->states[current].node, current, pi, AnimationNode::FILTER_IGNORE, true, true); // Just retrieve remain length, don't process.

		// Fading must be processed.
		if (fading_time) {
			break;
		}
	}

	return next.node == SceneStringName(End);
}

bool AnimationNodeStateMachinePlayback::_can_transition_to_next(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, NextInfo p_next, bool p_test_only) {
	if (p_next.node == StringName()) {
		return false;
	}

	if (next_request) {
		// Process request only once.
		next_request = false;
		// Next request must be applied to only deepest state machine.
		Ref<AnimationNodeStateMachine> anodesm = p_state_machine->find_node_by_path(current);
		if (anodesm.is_valid() && anodesm->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
			Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(base_path + current + "/playback");
			ERR_FAIL_COND_V(!playback.is_valid(), false);
			playback->_set_base_path(base_path + current + "/");
			if (p_test_only) {
				playback = playback->duplicate();
			}
			playback->_next_main();
			// Then, fadeing should be end.
			fading_from = StringName();
			fadeing_from_nti = AnimationNode::NodeTimeInfo();
			fading_pos = 0;
		} else {
			return true;
		}
	}

	if (fading_from != StringName()) {
		return false;
	}

	if (current != SceneStringName(Start) && p_next.switch_mode == AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END) {
		return Animation::is_less_or_equal_approx(current_nti.get_remain(p_next.break_loop_at_end), p_next.xfade);
	}
	return true;
}

Ref<AnimationNodeStateMachineTransition> AnimationNodeStateMachinePlayback::_check_group_transition(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine, const AnimationNodeStateMachine::Transition &p_transition, Ref<AnimationNodeStateMachine> &r_state_machine, bool &r_bypass) const {
	Ref<AnimationNodeStateMachineTransition> temp_transition;
	Ref<AnimationNodeStateMachinePlayback> parent_playback;
	if (r_state_machine->get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		if (p_transition.from == SceneStringName(Start)) {
			parent_playback = _get_parent_playback(p_tree);
			if (parent_playback.is_valid()) {
				r_bypass = true;
				temp_transition = parent_playback->_get_group_start_transition();
			}
		} else if (p_transition.to == SceneStringName(End)) {
			parent_playback = _get_parent_playback(p_tree);
			if (parent_playback.is_valid()) {
				temp_transition = parent_playback->_get_group_end_transition();
			}
		}
		if (temp_transition.is_valid()) {
			r_state_machine = _get_parent_state_machine(p_tree);
			return temp_transition;
		}
	}
	return p_transition.transition;
}

AnimationNodeStateMachinePlayback::NextInfo AnimationNodeStateMachinePlayback::_find_next(AnimationTree *p_tree, AnimationNodeStateMachine *p_state_machine) const {
	NextInfo next;
	if (path.size()) {
		for (int i = 0; i < p_state_machine->transitions.size(); i++) {
			Ref<AnimationNodeStateMachine> anodesm = p_state_machine;
			bool bypass = false;
			Ref<AnimationNodeStateMachineTransition> ref_transition = _check_group_transition(p_tree, p_state_machine, p_state_machine->transitions[i], anodesm, bypass);
			if (ref_transition->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED) {
				continue;
			}
			if (p_state_machine->transitions[i].from == current && p_state_machine->transitions[i].to == path[0]) {
				next.node = path[0];
				next.xfade = ref_transition->get_xfade_time();
				next.curve = ref_transition->get_xfade_curve();
				next.switch_mode = ref_transition->get_switch_mode();
				next.is_reset = ref_transition->is_reset();
				next.break_loop_at_end = ref_transition->is_loop_broken_at_end();
			}
		}
	} else {
		int auto_advance_to = -1;
		float priority_best = 1e20;
		for (int i = 0; i < p_state_machine->transitions.size(); i++) {
			Ref<AnimationNodeStateMachine> anodesm = p_state_machine;
			bool bypass = false;
			Ref<AnimationNodeStateMachineTransition> ref_transition = _check_group_transition(p_tree, p_state_machine, p_state_machine->transitions[i], anodesm, bypass);
			if (ref_transition->get_advance_mode() == AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED) {
				continue;
			}
			if (p_state_machine->transitions[i].from == current && (_check_advance_condition(anodesm, ref_transition) || bypass)) {
				if (ref_transition->get_priority() <= priority_best) {
					priority_best = ref_transition->get_priority();
					auto_advance_to = i;
				}
			}
		}

		if (auto_advance_to != -1) {
			next.node = p_state_machine->transitions[auto_advance_to].to;
			Ref<AnimationNodeStateMachine> anodesm = p_state_machine;
			bool bypass = false;
			Ref<AnimationNodeStateMachineTransition> ref_transition = _check_group_transition(p_tree, p_state_machine, p_state_machine->transitions[auto_advance_to], anodesm, bypass);
			next.xfade = ref_transition->get_xfade_time();
			next.curve = ref_transition->get_xfade_curve();
			next.switch_mode = ref_transition->get_switch_mode();
			next.is_reset = ref_transition->is_reset();
			next.break_loop_at_end = ref_transition->is_loop_broken_at_end();
		}
	}

	return next;
}

bool AnimationNodeStateMachinePlayback::_check_advance_condition(const Ref<AnimationNodeStateMachine> state_machine, const Ref<AnimationNodeStateMachineTransition> transition) const {
	if (transition->get_advance_mode() != AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO) {
		return false;
	}

	StringName advance_condition_name = transition->get_advance_condition_name();

	if (advance_condition_name != StringName() && !bool(state_machine->get_parameter(advance_condition_name))) {
		return false;
	}

	if (transition->expression.is_valid()) {
		AnimationTree *tree_base = state_machine->get_animation_tree();
		ERR_FAIL_NULL_V(tree_base, false);

		NodePath advance_expression_base_node_path = tree_base->get_advance_expression_base_node();
		Node *expression_base = tree_base->get_node_or_null(advance_expression_base_node_path);

		if (expression_base) {
			Ref<Expression> exp = transition->expression;
			bool ret = exp->execute(Array(), expression_base, false, Engine::get_singleton()->is_editor_hint()); // Avoids allowing the user to crash the system with an expression by only allowing const calls.
			if (exp->has_execute_failed() || !ret) {
				return false;
			}
		} else {
			WARN_PRINT_ONCE("Animation transition has a valid expression, but no expression base node was set on its AnimationTree.");
		}
	}

	return true;
}

void AnimationNodeStateMachinePlayback::clear_path() {
	path.clear();
}

void AnimationNodeStateMachinePlayback::push_path(const StringName &p_state) {
	path.push_back(p_state);
}

void AnimationNodeStateMachinePlayback::_set_base_path(const String &p_base_path) {
	base_path = p_base_path;
}

Ref<AnimationNodeStateMachinePlayback> AnimationNodeStateMachinePlayback::_get_parent_playback(AnimationTree *p_tree) const {
	if (base_path.is_empty()) {
		return Ref<AnimationNodeStateMachinePlayback>();
	}
	Vector<String> split = base_path.split("/");
	ERR_FAIL_COND_V_MSG(split.size() < 2, Ref<AnimationNodeStateMachinePlayback>(), "Path is too short.");
	StringName self_path = split[split.size() - 2];
	split.remove_at(split.size() - 2);
	String playback_path = String("/").join(split) + "playback";
	Ref<AnimationNodeStateMachinePlayback> playback = p_tree->get(playback_path);
	if (!playback.is_valid()) {
		ERR_PRINT_ONCE("Can't get parent AnimationNodeStateMachinePlayback with path: " + playback_path + ". Maybe there is no Root/Nested AnimationNodeStateMachine in the parent of the Grouped AnimationNodeStateMachine.");
		return Ref<AnimationNodeStateMachinePlayback>();
	}
	if (playback->get_current_node() != self_path) {
		return Ref<AnimationNodeStateMachinePlayback>();
	}
	return playback;
}

Ref<AnimationNodeStateMachine> AnimationNodeStateMachinePlayback::_get_parent_state_machine(AnimationTree *p_tree) const {
	if (base_path.is_empty()) {
		return Ref<AnimationNodeStateMachine>();
	}
	Vector<String> split = base_path.split("/");
	ERR_FAIL_COND_V_MSG(split.size() < 3, Ref<AnimationNodeStateMachine>(), "Path is too short.");
	split = split.slice(1, split.size() - 2);
	Ref<AnimationNode> root = p_tree->get_root_animation_node();
	ERR_FAIL_COND_V_MSG(root.is_null(), Ref<AnimationNodeStateMachine>(), "There is no root AnimationNode in AnimationTree: " + String(p_tree->get_name()));
	String anodesm_path = String("/").join(split);
	Ref<AnimationNodeStateMachine> anodesm = !anodesm_path.size() ? root : root->find_node_by_path(anodesm_path);
	ERR_FAIL_COND_V_MSG(anodesm.is_null(), Ref<AnimationNodeStateMachine>(), "Can't get state machine with path: " + anodesm_path);
	return anodesm;
}

Ref<AnimationNodeStateMachineTransition> AnimationNodeStateMachinePlayback::_get_group_start_transition() const {
	ERR_FAIL_COND_V_MSG(group_start_transition.is_null(), Ref<AnimationNodeStateMachineTransition>(), "Group start transition is null.");
	return group_start_transition;
}

Ref<AnimationNodeStateMachineTransition> AnimationNodeStateMachinePlayback::_get_group_end_transition() const {
	ERR_FAIL_COND_V_MSG(group_end_transition.is_null(), Ref<AnimationNodeStateMachineTransition>(), "Group end transition is null.");
	return group_end_transition;
}

void AnimationNodeStateMachinePlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("travel", "to_node", "reset_on_teleport"), &AnimationNodeStateMachinePlayback::travel, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("start", "node", "reset"), &AnimationNodeStateMachinePlayback::start, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("next"), &AnimationNodeStateMachinePlayback::next);
	ClassDB::bind_method(D_METHOD("stop"), &AnimationNodeStateMachinePlayback::stop);
	ClassDB::bind_method(D_METHOD("is_playing"), &AnimationNodeStateMachinePlayback::is_playing);
	ClassDB::bind_method(D_METHOD("get_current_node"), &AnimationNodeStateMachinePlayback::get_current_node);
	ClassDB::bind_method(D_METHOD("get_current_play_position"), &AnimationNodeStateMachinePlayback::get_current_play_pos);
	ClassDB::bind_method(D_METHOD("get_current_length"), &AnimationNodeStateMachinePlayback::get_current_length);
	ClassDB::bind_method(D_METHOD("get_fading_from_node"), &AnimationNodeStateMachinePlayback::get_fading_from_node);
	ClassDB::bind_method(D_METHOD("get_travel_path"), &AnimationNodeStateMachinePlayback::_get_travel_path);
}

AnimationNodeStateMachinePlayback::AnimationNodeStateMachinePlayback() {
	set_local_to_scene(true); // Only one per instantiated scene.
	default_transition.instantiate();
	default_transition->set_xfade_time(0);
	default_transition->set_reset(true);
	default_transition->set_advance_mode(AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
	default_transition->set_switch_mode(AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE);
}

///////////////////////////////////////////////////////

void AnimationNodeStateMachine::get_parameter_list(List<PropertyInfo> *r_list) const {
	AnimationNode::get_parameter_list(r_list);
	r_list->push_back(PropertyInfo(Variant::OBJECT, playback, PROPERTY_HINT_RESOURCE_TYPE, "AnimationNodeStateMachinePlayback", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ALWAYS_DUPLICATE)); // Don't store this object in .tres, it always needs to be made as unique object.
	List<StringName> advance_conditions;
	for (int i = 0; i < transitions.size(); i++) {
		StringName ac = transitions[i].transition->get_advance_condition_name();
		if (ac != StringName() && advance_conditions.find(ac) == nullptr) {
			advance_conditions.push_back(ac);
		}
	}

	advance_conditions.sort_custom<StringName::AlphCompare>();
	for (const StringName &E : advance_conditions) {
		r_list->push_back(PropertyInfo(Variant::BOOL, E));
	}
}

Variant AnimationNodeStateMachine::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret = AnimationNode::get_parameter_default_value(p_parameter);
	if (ret != Variant()) {
		return ret;
	}

	if (p_parameter == playback) {
		Ref<AnimationNodeStateMachinePlayback> p;
		p.instantiate();
		return p;
	} else {
		return false; // Advance condition.
	}
}

bool AnimationNodeStateMachine::is_parameter_read_only(const StringName &p_parameter) const {
	if (AnimationNode::is_parameter_read_only(p_parameter)) {
		return true;
	}

	if (p_parameter == playback) {
		return true;
	}
	return false;
}

void AnimationNodeStateMachine::add_node(const StringName &p_name, Ref<AnimationNode> p_node, const Vector2 &p_position) {
	ERR_FAIL_COND(states.has(p_name));
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(String(p_name).contains_char('/'));

	State state_new;
	state_new.node = p_node;
	state_new.position = p_position;

	states[p_name] = state_new;

	emit_changed();
	emit_signal(SNAME("tree_changed"));

	p_node->connect("tree_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed), CONNECT_REFERENCE_COUNTED);
	p_node->connect("animation_node_renamed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_renamed), CONNECT_REFERENCE_COUNTED);
	p_node->connect("animation_node_removed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_removed), CONNECT_REFERENCE_COUNTED);
}

void AnimationNodeStateMachine::replace_node(const StringName &p_name, Ref<AnimationNode> p_node) {
	ERR_FAIL_COND(states.has(p_name) == false);
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(String(p_name).contains_char('/'));

	{
		Ref<AnimationNode> node = states[p_name].node;
		if (node.is_valid()) {
			node->disconnect("tree_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed));
			node->disconnect("animation_node_renamed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_renamed));
			node->disconnect("animation_node_removed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_removed));
		}
	}

	states[p_name].node = p_node;

	emit_changed();
	emit_signal(SNAME("tree_changed"));

	p_node->connect("tree_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed), CONNECT_REFERENCE_COUNTED);
	p_node->connect("animation_node_renamed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_renamed), CONNECT_REFERENCE_COUNTED);
	p_node->connect("animation_node_removed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_removed), CONNECT_REFERENCE_COUNTED);
}

void AnimationNodeStateMachine::set_state_machine_type(StateMachineType p_state_machine_type) {
	state_machine_type = p_state_machine_type;
	emit_changed();
	emit_signal(SNAME("tree_changed"));
	notify_property_list_changed();
}

AnimationNodeStateMachine::StateMachineType AnimationNodeStateMachine::get_state_machine_type() const {
	return state_machine_type;
}

void AnimationNodeStateMachine::set_allow_transition_to_self(bool p_enable) {
	allow_transition_to_self = p_enable;
}

bool AnimationNodeStateMachine::is_allow_transition_to_self() const {
	return allow_transition_to_self;
}

void AnimationNodeStateMachine::set_reset_ends(bool p_enable) {
	reset_ends = p_enable;
}

bool AnimationNodeStateMachine::are_ends_reset() const {
	return reset_ends;
}

bool AnimationNodeStateMachine::can_edit_node(const StringName &p_name) const {
	if (states.has(p_name)) {
		return !(states[p_name].node->is_class("AnimationNodeStartState") || states[p_name].node->is_class("AnimationNodeEndState"));
	}

	return true;
}

Ref<AnimationNode> AnimationNodeStateMachine::get_node(const StringName &p_name) const {
	ERR_FAIL_COND_V_EDMSG(!states.has(p_name), Ref<AnimationNode>(), String(p_name) + " is not found current state.");

	return states[p_name].node;
}

StringName AnimationNodeStateMachine::get_node_name(const Ref<AnimationNode> &p_node) const {
	for (const KeyValue<StringName, State> &E : states) {
		if (E.value.node == p_node) {
			return E.key;
		}
	}

	ERR_FAIL_V(StringName());
}

void AnimationNodeStateMachine::get_child_nodes(List<ChildNode> *r_child_nodes) {
	Vector<StringName> nodes;

	for (const KeyValue<StringName, State> &E : states) {
		nodes.push_back(E.key);
	}

	nodes.sort_custom<StringName::AlphCompare>();

	for (int i = 0; i < nodes.size(); i++) {
		ChildNode cn;
		cn.name = nodes[i];
		cn.node = states[cn.name].node;
		r_child_nodes->push_back(cn);
	}
}

bool AnimationNodeStateMachine::has_node(const StringName &p_name) const {
	return states.has(p_name);
}

void AnimationNodeStateMachine::remove_node(const StringName &p_name) {
	ERR_FAIL_COND(!states.has(p_name));

	if (!can_edit_node(p_name)) {
		return;
	}

	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_name || transitions[i].to == p_name) {
			remove_transition_by_index(i);
			i--;
		}
	}

	{
		Ref<AnimationNode> node = states[p_name].node;
		ERR_FAIL_COND(node.is_null());
		node->disconnect("tree_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed));
		node->disconnect("animation_node_renamed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_renamed));
		node->disconnect("animation_node_removed", callable_mp(this, &AnimationNodeStateMachine::_animation_node_removed));
	}

	states.erase(p_name);

	emit_signal(SNAME("animation_node_removed"), get_instance_id(), p_name);
	emit_changed();
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeStateMachine::rename_node(const StringName &p_name, const StringName &p_new_name) {
	ERR_FAIL_COND(!states.has(p_name));
	ERR_FAIL_COND(states.has(p_new_name));
	ERR_FAIL_COND(!can_edit_node(p_name));

	states[p_new_name] = states[p_name];
	states.erase(p_name);

	_rename_transitions(p_name, p_new_name);

	emit_signal(SNAME("animation_node_renamed"), get_instance_id(), p_name, p_new_name);
	emit_changed();
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeStateMachine::_rename_transitions(const StringName &p_name, const StringName &p_new_name) {
	if (updating_transitions) {
		return;
	}

	updating_transitions = true;
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_name) {
			transitions.write[i].from = p_new_name;
		}
		if (transitions[i].to == p_name) {
			transitions.write[i].to = p_new_name;
		}
	}
	updating_transitions = false;
}

void AnimationNodeStateMachine::get_node_list(List<StringName> *r_nodes) const {
	List<StringName> nodes;
	for (const KeyValue<StringName, State> &E : states) {
		nodes.push_back(E.key);
	}
	nodes.sort_custom<StringName::AlphCompare>();

	for (const StringName &E : nodes) {
		r_nodes->push_back(E);
	}
}

bool AnimationNodeStateMachine::has_transition(const StringName &p_from, const StringName &p_to) const {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_from && transitions[i].to == p_to) {
			return true;
		}
	}
	return false;
}

bool AnimationNodeStateMachine::has_transition_from(const StringName &p_from) const {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_from) {
			return true;
		}
	}
	return false;
}

bool AnimationNodeStateMachine::has_transition_to(const StringName &p_to) const {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].to == p_to) {
			return true;
		}
	}
	return false;
}

int AnimationNodeStateMachine::find_transition(const StringName &p_from, const StringName &p_to) const {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_from && transitions[i].to == p_to) {
			return i;
		}
	}
	return -1;
}

Vector<int> AnimationNodeStateMachine::find_transition_from(const StringName &p_from) const {
	Vector<int> ret;
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_from) {
			ret.push_back(i);
		}
	}
	return ret;
}

Vector<int> AnimationNodeStateMachine::find_transition_to(const StringName &p_to) const {
	Vector<int> ret;
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].to == p_to) {
			ret.push_back(i);
		}
	}
	return ret;
}

bool AnimationNodeStateMachine::_can_connect(const StringName &p_name) {
	if (states.has(p_name)) {
		return true;
	}

	String node_name = p_name;
	Vector<String> path = node_name.split("/");

	if (path.size() < 2) {
		return false;
	}

	return false;
}

void AnimationNodeStateMachine::add_transition(const StringName &p_from, const StringName &p_to, const Ref<AnimationNodeStateMachineTransition> &p_transition) {
	if (updating_transitions) {
		return;
	}

	ERR_FAIL_COND(p_from == SceneStringName(End) || p_to == SceneStringName(Start));
	ERR_FAIL_COND(p_from == p_to);
	ERR_FAIL_COND(!_can_connect(p_from));
	ERR_FAIL_COND(!_can_connect(p_to));
	ERR_FAIL_COND(p_transition.is_null());

	for (int i = 0; i < transitions.size(); i++) {
		ERR_FAIL_COND(transitions[i].from == p_from && transitions[i].to == p_to);
	}

	updating_transitions = true;

	Transition tr;
	tr.from = p_from;
	tr.to = p_to;
	tr.transition = p_transition;

	tr.transition->connect("advance_condition_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed), CONNECT_REFERENCE_COUNTED);

	transitions.push_back(tr);

	updating_transitions = false;
}

Ref<AnimationNodeStateMachineTransition> AnimationNodeStateMachine::get_transition(int p_transition) const {
	ERR_FAIL_INDEX_V(p_transition, transitions.size(), Ref<AnimationNodeStateMachineTransition>());
	return transitions[p_transition].transition;
}

StringName AnimationNodeStateMachine::get_transition_from(int p_transition) const {
	ERR_FAIL_INDEX_V(p_transition, transitions.size(), StringName());
	return transitions[p_transition].from;
}

StringName AnimationNodeStateMachine::get_transition_to(int p_transition) const {
	ERR_FAIL_INDEX_V(p_transition, transitions.size(), StringName());
	return transitions[p_transition].to;
}

bool AnimationNodeStateMachine::is_transition_across_group(int p_transition) const {
	ERR_FAIL_INDEX_V(p_transition, transitions.size(), false);
	if (get_state_machine_type() == AnimationNodeStateMachine::STATE_MACHINE_TYPE_GROUPED) {
		if (transitions[p_transition].from == SceneStringName(Start) || transitions[p_transition].to == SceneStringName(End)) {
			return true;
		}
	}
	return false;
}

int AnimationNodeStateMachine::get_transition_count() const {
	return transitions.size();
}

void AnimationNodeStateMachine::remove_transition(const StringName &p_from, const StringName &p_to) {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].from == p_from && transitions[i].to == p_to) {
			remove_transition_by_index(i);
			return;
		}
	}
}

void AnimationNodeStateMachine::remove_transition_by_index(const int p_transition) {
	ERR_FAIL_INDEX(p_transition, transitions.size());
	transitions.write[p_transition].transition->disconnect("advance_condition_changed", callable_mp(this, &AnimationNodeStateMachine::_tree_changed));
	transitions.remove_at(p_transition);
}

void AnimationNodeStateMachine::_remove_transition(const Ref<AnimationNodeStateMachineTransition> p_transition) {
	for (int i = 0; i < transitions.size(); i++) {
		if (transitions[i].transition == p_transition) {
			remove_transition_by_index(i);
			return;
		}
	}
}

void AnimationNodeStateMachine::set_graph_offset(const Vector2 &p_offset) {
	graph_offset = p_offset;
}

Vector2 AnimationNodeStateMachine::get_graph_offset() const {
	return graph_offset;
}

AnimationNode::NodeTimeInfo AnimationNodeStateMachine::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	Ref<AnimationNodeStateMachinePlayback> playback_new = get_parameter(playback);
	ERR_FAIL_COND_V(playback_new.is_null(), AnimationNode::NodeTimeInfo());
	playback_new->_set_grouped(state_machine_type == STATE_MACHINE_TYPE_GROUPED);
	if (p_test_only) {
		playback_new = playback_new->duplicate(); // Don't process original when testing.
	}

	return playback_new->process(node_state.get_base_path(), this, p_playback_info, p_test_only);
}

String AnimationNodeStateMachine::get_caption() const {
	return "StateMachine";
}

Ref<AnimationNode> AnimationNodeStateMachine::get_child_by_name(const StringName &p_name) const {
	return get_node(p_name);
}

bool AnimationNodeStateMachine::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name.begins_with("states/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			Ref<AnimationNode> anode = p_value;
			if (anode.is_valid()) {
				add_node(node_name, p_value);
			}
			return true;
		}

		if (what == "position") {
			if (states.has(node_name)) {
				states[node_name].position = p_value;
			}
			return true;
		}
	} else if (prop_name == "transitions") {
		Array trans = p_value;
		ERR_FAIL_COND_V(trans.size() % 3 != 0, false);

		for (int i = 0; i < trans.size(); i += 3) {
			add_transition(trans[i], trans[i + 1], trans[i + 2]);
		}
		return true;
	} else if (prop_name == "graph_offset") {
		set_graph_offset(p_value);
		return true;
	}

	return false;
}

bool AnimationNodeStateMachine::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (prop_name.begins_with("states/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			if (states.has(node_name) && can_edit_node(node_name)) {
				r_ret = states[node_name].node;
				return true;
			}
		}

		if (what == "position") {
			if (states.has(node_name)) {
				r_ret = states[node_name].position;
				return true;
			}
		}
	} else if (prop_name == "transitions") {
		Array trans;
		for (int i = 0; i < transitions.size(); i++) {
			String from = transitions[i].from;
			String to = transitions[i].to;

			trans.push_back(from);
			trans.push_back(to);
			trans.push_back(transitions[i].transition);
		}

		r_ret = trans;
		return true;
	} else if (prop_name == "graph_offset") {
		r_ret = get_graph_offset();
		return true;
	}

	return false;
}

void AnimationNodeStateMachine::_get_property_list(List<PropertyInfo> *p_list) const {
	List<StringName> names;
	for (const KeyValue<StringName, State> &E : states) {
		names.push_back(E.key);
	}
	names.sort_custom<StringName::AlphCompare>();

	for (const StringName &prop_name : names) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, "states/" + prop_name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "AnimationNode", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "states/" + prop_name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "transitions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	p_list->push_back(PropertyInfo(Variant::VECTOR2, "graph_offset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));

	for (PropertyInfo &E : *p_list) {
		_validate_property(E);
	}
}

void AnimationNodeStateMachine::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "allow_transition_to_self" || p_property.name == "reset_ends") {
		if (state_machine_type == STATE_MACHINE_TYPE_GROUPED) {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

void AnimationNodeStateMachine::reset_state() {
	states.clear();
	transitions.clear();
	playback = "playback";
	graph_offset = Vector2();

	Ref<AnimationNodeStartState> s;
	s.instantiate();
	State start;
	start.node = s;
	start.position = Vector2(200, 100);
	states[SceneStringName(Start)] = start;

	Ref<AnimationNodeEndState> e;
	e.instantiate();
	State end;
	end.node = e;
	end.position = Vector2(900, 100);
	states[SceneStringName(End)] = end;

	emit_changed();
	emit_signal(SNAME("tree_changed"));
}

void AnimationNodeStateMachine::set_node_position(const StringName &p_name, const Vector2 &p_position) {
	ERR_FAIL_COND(!states.has(p_name));
	states[p_name].position = p_position;
}

Vector2 AnimationNodeStateMachine::get_node_position(const StringName &p_name) const {
	ERR_FAIL_COND_V(!states.has(p_name), Vector2());
	return states[p_name].position;
}

void AnimationNodeStateMachine::_tree_changed() {
	emit_changed();
	AnimationRootNode::_tree_changed();
}

void AnimationNodeStateMachine::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	AnimationRootNode::_animation_node_renamed(p_oid, p_old_name, p_new_name);
}

void AnimationNodeStateMachine::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	AnimationRootNode::_animation_node_removed(p_oid, p_node);
}

#ifdef TOOLS_ENABLED
void AnimationNodeStateMachine::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	bool add_state_options = false;
	if (p_idx == 0) {
		add_state_options = (pf == "get_node" || pf == "has_node" || pf == "rename_node" || pf == "remove_node" || pf == "replace_node" || pf == "set_node_position" || pf == "get_node_position");
	} else if (p_idx <= 1) {
		add_state_options = (pf == "has_transition" || pf == "add_transition" || pf == "remove_transition");
	}
	if (add_state_options) {
		for (const KeyValue<StringName, State> &E : states) {
			r_options->push_back(String(E.key).quote());
		}
	}
	AnimationRootNode::get_argument_options(p_function, p_idx, r_options);
}
#endif

void AnimationNodeStateMachine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_node", "name", "node", "position"), &AnimationNodeStateMachine::add_node, DEFVAL(Vector2()));
	ClassDB::bind_method(D_METHOD("replace_node", "name", "node"), &AnimationNodeStateMachine::replace_node);
	ClassDB::bind_method(D_METHOD("get_node", "name"), &AnimationNodeStateMachine::get_node);
	ClassDB::bind_method(D_METHOD("remove_node", "name"), &AnimationNodeStateMachine::remove_node);
	ClassDB::bind_method(D_METHOD("rename_node", "name", "new_name"), &AnimationNodeStateMachine::rename_node);
	ClassDB::bind_method(D_METHOD("has_node", "name"), &AnimationNodeStateMachine::has_node);
	ClassDB::bind_method(D_METHOD("get_node_name", "node"), &AnimationNodeStateMachine::get_node_name);

	ClassDB::bind_method(D_METHOD("set_node_position", "name", "position"), &AnimationNodeStateMachine::set_node_position);
	ClassDB::bind_method(D_METHOD("get_node_position", "name"), &AnimationNodeStateMachine::get_node_position);

	ClassDB::bind_method(D_METHOD("has_transition", "from", "to"), &AnimationNodeStateMachine::has_transition);
	ClassDB::bind_method(D_METHOD("add_transition", "from", "to", "transition"), &AnimationNodeStateMachine::add_transition);
	ClassDB::bind_method(D_METHOD("get_transition", "idx"), &AnimationNodeStateMachine::get_transition);
	ClassDB::bind_method(D_METHOD("get_transition_from", "idx"), &AnimationNodeStateMachine::get_transition_from);
	ClassDB::bind_method(D_METHOD("get_transition_to", "idx"), &AnimationNodeStateMachine::get_transition_to);
	ClassDB::bind_method(D_METHOD("get_transition_count"), &AnimationNodeStateMachine::get_transition_count);
	ClassDB::bind_method(D_METHOD("remove_transition_by_index", "idx"), &AnimationNodeStateMachine::remove_transition_by_index);
	ClassDB::bind_method(D_METHOD("remove_transition", "from", "to"), &AnimationNodeStateMachine::remove_transition);

	ClassDB::bind_method(D_METHOD("set_graph_offset", "offset"), &AnimationNodeStateMachine::set_graph_offset);
	ClassDB::bind_method(D_METHOD("get_graph_offset"), &AnimationNodeStateMachine::get_graph_offset);

	ClassDB::bind_method(D_METHOD("set_state_machine_type", "state_machine_type"), &AnimationNodeStateMachine::set_state_machine_type);
	ClassDB::bind_method(D_METHOD("get_state_machine_type"), &AnimationNodeStateMachine::get_state_machine_type);

	ClassDB::bind_method(D_METHOD("set_allow_transition_to_self", "enable"), &AnimationNodeStateMachine::set_allow_transition_to_self);
	ClassDB::bind_method(D_METHOD("is_allow_transition_to_self"), &AnimationNodeStateMachine::is_allow_transition_to_self);

	ClassDB::bind_method(D_METHOD("set_reset_ends", "enable"), &AnimationNodeStateMachine::set_reset_ends);
	ClassDB::bind_method(D_METHOD("are_ends_reset"), &AnimationNodeStateMachine::are_ends_reset);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "state_machine_type", PROPERTY_HINT_ENUM, "Root,Nested,Grouped"), "set_state_machine_type", "get_state_machine_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_transition_to_self"), "set_allow_transition_to_self", "is_allow_transition_to_self");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reset_ends"), "set_reset_ends", "are_ends_reset");

	BIND_ENUM_CONSTANT(STATE_MACHINE_TYPE_ROOT);
	BIND_ENUM_CONSTANT(STATE_MACHINE_TYPE_NESTED);
	BIND_ENUM_CONSTANT(STATE_MACHINE_TYPE_GROUPED);
}

Vector<StringName> AnimationNodeStateMachine::get_nodes_with_transitions_from(const StringName &p_node) const {
	Vector<StringName> result;
	for (const Transition &transition : transitions) {
		if (transition.from == p_node) {
			result.push_back(transition.to);
		}
	}
	return result;
}

Vector<StringName> AnimationNodeStateMachine::get_nodes_with_transitions_to(const StringName &p_node) const {
	Vector<StringName> result;
	for (const Transition &transition : transitions) {
		if (transition.to == p_node) {
			result.push_back(transition.from);
		}
	}
	return result;
}

AnimationNodeStateMachine::AnimationNodeStateMachine() {
	Ref<AnimationNodeStartState> s;
	s.instantiate();
	State start;
	start.node = s;
	start.position = Vector2(200, 100);
	states[SceneStringName(Start)] = start;

	Ref<AnimationNodeEndState> e;
	e.instantiate();
	State end;
	end.node = e;
	end.position = Vector2(900, 100);
	states[SceneStringName(End)] = end;
}
