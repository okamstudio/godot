/**************************************************************************/
/*  animation_tree.cpp                                                    */
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

#include "animation_tree.h"
#include "animation_tree.compat.inc"

#include "animation_blend_tree.h"
#include "scene/animation/animation_player.h"

void AnimationNode::get_parameter_list(List<PropertyInfo> *r_list) const {
	Array parameters;

	if (GDVIRTUAL_CALL(_get_parameter_list, parameters)) {
		for (int i = 0; i < parameters.size(); i++) {
			Dictionary d = parameters[i];
			ERR_CONTINUE(d.is_empty());
			r_list->push_back(PropertyInfo::from_dict(d));
		}
	}

	r_list->push_back(PropertyInfo(Variant::FLOAT, current_length, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY));
	r_list->push_back(PropertyInfo(Variant::FLOAT, current_position, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY));
	r_list->push_back(PropertyInfo(Variant::FLOAT, current_delta, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY));
}

Variant AnimationNode::get_parameter_default_value(const StringName &p_parameter) const {
	Variant ret;
	GDVIRTUAL_CALL(_get_parameter_default_value, p_parameter, ret);
	return ret;
}

bool AnimationNode::is_parameter_read_only(const StringName &p_parameter) const {
	bool ret = false;
	if (GDVIRTUAL_CALL(_is_parameter_read_only, p_parameter, ret) && ret) {
		return true;
	}

	if (p_parameter == current_length || p_parameter == current_position || p_parameter == current_delta) {
		return true;
	}

	return false;
}

void AnimationNode::set_parameter(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_NULL(process_state);
	if (process_state->is_testing) {
		return;
	}

	const AHashMap<StringName, int>::Iterator it = property_cache.find(p_name);
	if (it) {
		process_state->tree->property_map.get_by_index(it->value).value.first = p_value;
		return;
	}

	ERR_FAIL_COND(!process_state->tree->property_parent_map.has(node_state.base_path));
	ERR_FAIL_COND(!process_state->tree->property_parent_map[node_state.base_path].has(p_name));
	StringName path = process_state->tree->property_parent_map[node_state.base_path][p_name];
	int idx = process_state->tree->property_map.get_index(path);
	property_cache.insert_new(p_name, idx);
	process_state->tree->property_map.get_by_index(idx).value.first = p_value;
}

Variant AnimationNode::get_parameter(const StringName &p_name) const {
	ERR_FAIL_NULL_V(process_state, Variant());
	const AHashMap<StringName, int>::ConstIterator it = property_cache.find(p_name);
	if (it) {
		return process_state->tree->property_map.get_by_index(it->value).value.first;
	}
	ERR_FAIL_COND_V(!process_state->tree->property_parent_map.has(node_state.base_path), Variant());
	ERR_FAIL_COND_V(!process_state->tree->property_parent_map[node_state.base_path].has(p_name), Variant());

	StringName path = process_state->tree->property_parent_map[node_state.base_path][p_name];
	int idx = process_state->tree->property_map.get_index(path);
	property_cache.insert_new(p_name, idx);
	return process_state->tree->property_map.get_by_index(idx).value.first;
}

void AnimationNode::set_node_time_info(const NodeTimeInfo &p_node_time_info) {
	set_parameter(current_length, p_node_time_info.length);
	set_parameter(current_position, p_node_time_info.position);
	set_parameter(current_delta, p_node_time_info.delta);
}

AnimationNode::NodeTimeInfo AnimationNode::get_node_time_info() const {
	NodeTimeInfo nti;
	nti.length = get_parameter(current_length);
	nti.position = get_parameter(current_position);
	nti.delta = get_parameter(current_delta);
	return nti;
}

void AnimationNode::get_child_nodes(List<ChildNode> *r_child_nodes) {
	Dictionary cn;
	if (GDVIRTUAL_CALL(_get_child_nodes, cn)) {
		List<Variant> keys;
		cn.get_key_list(&keys);
		for (const Variant &E : keys) {
			ChildNode child;
			child.name = E;
			child.node = cn[E];
			r_child_nodes->push_back(child);
		}
	}
}

void AnimationNode::blend_animation(const StringName &p_animation, AnimationMixer::PlaybackInfo p_playback_info) {
	ERR_FAIL_NULL(process_state);
	p_playback_info.track_weights = node_state.track_weights;
	process_state->tree->make_animation_instance(p_animation, p_playback_info);
}

AnimationNode::NodeTimeInfo AnimationNode::_pre_process(ProcessState *p_process_state, AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	process_state = p_process_state;
	NodeTimeInfo nti = process(p_playback_info, p_test_only);
	process_state = nullptr;
	return nti;
}

void AnimationNode::make_invalid(const String &p_reason) {
	ERR_FAIL_NULL(process_state);
	process_state->valid = false;
	if (!process_state->invalid_reasons.is_empty()) {
		process_state->invalid_reasons += "\n";
	}
	process_state->invalid_reasons += String::utf8("•  ") + p_reason;
}

AnimationTree *AnimationNode::get_animation_tree() const {
	ERR_FAIL_NULL_V(process_state, nullptr);
	return process_state->tree;
}

AnimationNode::NodeTimeInfo AnimationNode::blend_input(int p_input, AnimationMixer::PlaybackInfo p_playback_info, FilterAction p_filter, bool p_sync, bool p_test_only) {
	ERR_FAIL_INDEX_V(p_input, (int64_t)inputs.size(), NodeTimeInfo());

	AnimationNodeBlendTree *blend_tree = Object::cast_to<AnimationNodeBlendTree>(node_state.parent);
	ERR_FAIL_NULL_V(blend_tree, NodeTimeInfo());

	// Update connections.
	StringName current_name = blend_tree->get_node_name(Ref<AnimationNode>(this));
	node_state.connections = blend_tree->get_node_connection_array(current_name);

	// Get node which is connected input port.
	StringName node_name = node_state.connections[p_input];
	if (!blend_tree->has_node(node_name)) {
		make_invalid(vformat(RTR("Nothing connected to input '%s' of node '%s'."), get_input_name(p_input), current_name));
		return NodeTimeInfo();
	}

	Ref<AnimationNode> node = blend_tree->get_node(node_name);
	ERR_FAIL_COND_V(node.is_null(), NodeTimeInfo());

	real_t activity = 0.0;
	LocalVector<AnimationTree::Activity> *activity_ptr = process_state->tree->input_activity_map.getptr(node_state.base_path);
	NodeTimeInfo nti = _blend_node(node, node_name, nullptr, p_playback_info, p_filter, p_sync, p_test_only, &activity);

	if (activity_ptr && p_input < (int64_t)activity_ptr->size()) {
		(*activity_ptr)[p_input].last_pass = process_state->last_pass;
		(*activity_ptr)[p_input].activity = activity;
	}
	return nti;
}

AnimationNode::NodeTimeInfo AnimationNode::blend_node(Ref<AnimationNode> p_node, const StringName &p_subpath, AnimationMixer::PlaybackInfo p_playback_info, FilterAction p_filter, bool p_sync, bool p_test_only) {
	ERR_FAIL_COND_V(p_node.is_null(), NodeTimeInfo());
	p_node->node_state.connections.clear();
	return _blend_node(p_node, p_subpath, this, p_playback_info, p_filter, p_sync, p_test_only, nullptr);
}

AnimationNode::NodeTimeInfo AnimationNode::_blend_node(Ref<AnimationNode> p_node, const StringName &p_subpath, AnimationNode *p_new_parent, AnimationMixer::PlaybackInfo p_playback_info, FilterAction p_filter, bool p_sync, bool p_test_only, real_t *r_activity) {
	ERR_FAIL_NULL_V(process_state, NodeTimeInfo());

	int blend_count = node_state.track_weights.size();

	if ((int64_t)p_node->node_state.track_weights.size() != blend_count) {
		p_node->node_state.track_weights.resize(blend_count);
	}

	real_t *blendw = p_node->node_state.track_weights.ptr();
	const real_t *blendr = node_state.track_weights.ptr();

	bool any_valid = false;

	if (has_filter() && is_filter_enabled() && p_filter != FILTER_IGNORE) {
		for (int i = 0; i < blend_count; i++) {
			blendw[i] = 0.0; // All to zero by default.
		}

		for (const KeyValue<NodePath, bool> &E : filter) {
			const AHashMap<NodePath, int> &map = *process_state->track_map;
			if (!map.has(E.key)) {
				continue;
			}
			int idx = map[E.key];
			blendw[idx] = 1.0; // Filtered goes to one.
		}

		switch (p_filter) {
			case FILTER_IGNORE:
				break; // Will not happen anyway.
			case FILTER_PASS: {
				// Values filtered pass, the rest don't.
				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] == 0) { // Not filtered, does not pass.
						continue;
					}

					blendw[i] = blendr[i] * p_playback_info.weight;
					if (!Math::is_zero_approx(blendw[i])) {
						any_valid = true;
					}
				}

			} break;
			case FILTER_STOP: {
				// Values filtered don't pass, the rest are blended.

				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] > 0) { // Filtered, does not pass.
						continue;
					}

					blendw[i] = blendr[i] * p_playback_info.weight;
					if (!Math::is_zero_approx(blendw[i])) {
						any_valid = true;
					}
				}

			} break;
			case FILTER_BLEND: {
				// Filtered values are blended, the rest are passed without blending.

				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] == 1.0) {
						blendw[i] = blendr[i] * p_playback_info.weight; // Filtered, blend.
					} else {
						blendw[i] = blendr[i]; // Not filtered, do not blend.
					}

					if (!Math::is_zero_approx(blendw[i])) {
						any_valid = true;
					}
				}

			} break;
		}
	} else {
		for (int i = 0; i < blend_count; i++) {
			// Regular blend.
			blendw[i] = blendr[i] * p_playback_info.weight;
			if (!Math::is_zero_approx(blendw[i])) {
				any_valid = true;
			}
		}
	}

	if (r_activity) {
		*r_activity = 0;
		for (int i = 0; i < blend_count; i++) {
			*r_activity = MAX(*r_activity, Math::abs(blendw[i]));
		}
	}

	String new_path;
	AnimationNode *new_parent;

	// This is the slowest part of processing, but as strings process in powers of 2, and the paths always exist, it will not result in that many allocations.
	if (p_new_parent) {
		new_parent = p_new_parent;
		new_path = String(node_state.base_path) + String(p_subpath) + "/";
	} else {
		ERR_FAIL_NULL_V(node_state.parent, NodeTimeInfo());
		new_parent = node_state.parent;
		new_path = String(new_parent->node_state.base_path) + String(p_subpath) + "/";
	}

	// This process, which depends on p_sync is needed to process sync correctly in the case of
	// that a synced AnimationNodeSync exists under the un-synced AnimationNodeSync.
	p_node->set_node_state_base_path(new_path);
	p_node->node_state.parent = new_parent;
	if (!p_playback_info.seeked && !p_sync && !any_valid) {
		p_playback_info.delta = 0.0;
		return p_node->_pre_process(process_state, p_playback_info, p_test_only);
	}
	return p_node->_pre_process(process_state, p_playback_info, p_test_only);
}

String AnimationNode::get_caption() const {
	String ret = "Node";
	GDVIRTUAL_CALL(_get_caption, ret);
	return ret;
}

bool AnimationNode::add_input(const String &p_name) {
	// Root nodes can't add inputs.
	ERR_FAIL_COND_V(Object::cast_to<AnimationRootNode>(this) != nullptr, false);
	Input input;
	ERR_FAIL_COND_V(p_name.contains_char('.') || p_name.contains_char('/'), false);
	input.name = p_name;
	inputs.push_back(input);
	emit_changed();
	return true;
}

void AnimationNode::remove_input(int p_index) {
	ERR_FAIL_INDEX(p_index, (int64_t)inputs.size());
	inputs.remove_at(p_index);
	emit_changed();
}

bool AnimationNode::set_input_name(int p_input, const String &p_name) {
	ERR_FAIL_INDEX_V(p_input, (int64_t)inputs.size(), false);
	ERR_FAIL_COND_V(p_name.contains_char('.') || p_name.contains_char('/'), false);
	inputs[p_input].name = p_name;
	emit_changed();
	return true;
}

String AnimationNode::get_input_name(int p_input) const {
	ERR_FAIL_INDEX_V(p_input, (int64_t)inputs.size(), String());
	return inputs[p_input].name;
}

int AnimationNode::get_input_count() const {
	return inputs.size();
}

int AnimationNode::find_input(const String &p_name) const {
	int idx = -1;
	for (int i = 0; i < (int64_t)inputs.size(); i++) {
		if (inputs[i].name == p_name) {
			idx = i;
			break;
		}
	}
	return idx;
}

AnimationNode::NodeTimeInfo AnimationNode::process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	process_state->is_testing = p_test_only;

	AnimationMixer::PlaybackInfo pi = p_playback_info;
	if (p_playback_info.seeked) {
		if (p_playback_info.is_external_seeking) {
			pi.delta = get_node_time_info().position - p_playback_info.time;
		}
	} else {
		pi.time = get_node_time_info().position + p_playback_info.delta;
	}

	NodeTimeInfo nti = _process(pi, p_test_only);

	if (!p_test_only) {
		set_node_time_info(nti);
	}

	return nti;
}

AnimationNode::NodeTimeInfo AnimationNode::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	double r_ret = 0.0;
	GDVIRTUAL_CALL(_process, p_playback_info.time, p_playback_info.seeked, p_playback_info.is_external_seeking, p_test_only, r_ret);
	NodeTimeInfo nti;
	nti.delta = r_ret;
	return nti;
}

void AnimationNode::set_filter_path(const NodePath &p_path, bool p_enable) {
	if (p_enable) {
		filter[p_path] = true;
	} else {
		filter.erase(p_path);
	}
}

void AnimationNode::set_filter_enabled(bool p_enable) {
	filter_enabled = p_enable;
}

bool AnimationNode::is_filter_enabled() const {
	return filter_enabled;
}

void AnimationNode::set_deletable(bool p_closable) {
	closable = p_closable;
}

bool AnimationNode::is_deletable() const {
	return closable;
}

ObjectID AnimationNode::get_processing_animation_tree_instance_id() const {
	ERR_FAIL_NULL_V(process_state, ObjectID());
	return process_state->tree->get_instance_id();
}

bool AnimationNode::is_process_testing() const {
	ERR_FAIL_NULL_V(process_state, false);
	return process_state->is_testing;
}

bool AnimationNode::is_path_filtered(const NodePath &p_path) const {
	return filter.has(p_path);
}

bool AnimationNode::has_filter() const {
	bool ret = false;
	GDVIRTUAL_CALL(_has_filter, ret);
	return ret;
}

Array AnimationNode::_get_filters() const {
	Array paths;

	for (const KeyValue<NodePath, bool> &E : filter) {
		paths.push_back(String(E.key)); // Use strings, so sorting is possible.
	}
	paths.sort(); // Done so every time the scene is saved, it does not change.

	return paths;
}

void AnimationNode::_set_filters(const Array &p_filters) {
	filter.clear();
	for (int i = 0; i < p_filters.size(); i++) {
		set_filter_path(p_filters[i], true);
	}
}

void AnimationNode::_validate_property(PropertyInfo &p_property) const {
	if (!has_filter() && (p_property.name == "filter_enabled" || p_property.name == "filters")) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

Ref<AnimationNode> AnimationNode::get_child_by_name(const StringName &p_name) const {
	Ref<AnimationNode> ret;
	GDVIRTUAL_CALL(_get_child_by_name, p_name, ret);
	return ret;
}

Ref<AnimationNode> AnimationNode::find_node_by_path(const String &p_name) const {
	Vector<String> split = p_name.split("/");
	Ref<AnimationNode> ret = const_cast<AnimationNode *>(this);
	for (int i = 0; i < split.size(); i++) {
		ret = ret->get_child_by_name(split[i]);
		if (ret.is_null()) {
			break;
		}
	}
	return ret;
}

void AnimationNode::blend_animation_ex(const StringName &p_animation, double p_time, double p_delta, bool p_seeked, bool p_is_external_seeking, real_t p_blend, Animation::LoopedFlag p_looped_flag) {
	AnimationMixer::PlaybackInfo info;
	info.time = p_time;
	info.delta = p_delta;
	info.seeked = p_seeked;
	info.is_external_seeking = p_is_external_seeking;
	info.weight = p_blend;
	info.looped_flag = p_looped_flag;
	blend_animation(p_animation, info);
}

double AnimationNode::blend_node_ex(const StringName &p_sub_path, Ref<AnimationNode> p_node, double p_time, bool p_seek, bool p_is_external_seeking, real_t p_blend, FilterAction p_filter, bool p_sync, bool p_test_only) {
	AnimationMixer::PlaybackInfo info;
	info.time = p_time;
	info.seeked = p_seek;
	info.is_external_seeking = p_is_external_seeking;
	info.weight = p_blend;
	NodeTimeInfo nti = blend_node(p_node, p_sub_path, info, p_filter, p_sync, p_test_only);
	return nti.length - nti.position;
}

double AnimationNode::blend_input_ex(int p_input, double p_time, bool p_seek, bool p_is_external_seeking, real_t p_blend, FilterAction p_filter, bool p_sync, bool p_test_only) {
	AnimationMixer::PlaybackInfo info;
	info.time = p_time;
	info.seeked = p_seek;
	info.is_external_seeking = p_is_external_seeking;
	info.weight = p_blend;
	NodeTimeInfo nti = blend_input(p_input, info, p_filter, p_sync, p_test_only);
	return nti.length - nti.position;
}

#ifdef TOOLS_ENABLED
void AnimationNode::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		if (pf == "find_input") {
			for (const AnimationNode::Input &E : inputs) {
				r_options->push_back(E.name.quote());
			}
		} else if (pf == "get_parameter" || pf == "set_parameter") {
			bool is_setter = pf == "set_parameter";
			List<PropertyInfo> parameters;
			get_parameter_list(&parameters);
			for (const PropertyInfo &E : parameters) {
				if (is_setter && is_parameter_read_only(E.name)) {
					continue;
				}
				r_options->push_back(E.name.quote());
			}
		} else if (pf == "set_filter_path" || pf == "is_path_filtered") {
			for (const KeyValue<NodePath, bool> &E : filter) {
				r_options->push_back(String(E.key).quote());
			}
		}
	}
	Resource::get_argument_options(p_function, p_idx, r_options);
}
#endif

void AnimationNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_input", "name"), &AnimationNode::add_input);
	ClassDB::bind_method(D_METHOD("remove_input", "index"), &AnimationNode::remove_input);
	ClassDB::bind_method(D_METHOD("set_input_name", "input", "name"), &AnimationNode::set_input_name);
	ClassDB::bind_method(D_METHOD("get_input_name", "input"), &AnimationNode::get_input_name);
	ClassDB::bind_method(D_METHOD("get_input_count"), &AnimationNode::get_input_count);
	ClassDB::bind_method(D_METHOD("find_input", "name"), &AnimationNode::find_input);

	ClassDB::bind_method(D_METHOD("set_filter_path", "path", "enable"), &AnimationNode::set_filter_path);
	ClassDB::bind_method(D_METHOD("is_path_filtered", "path"), &AnimationNode::is_path_filtered);

	ClassDB::bind_method(D_METHOD("set_filter_enabled", "enable"), &AnimationNode::set_filter_enabled);
	ClassDB::bind_method(D_METHOD("is_filter_enabled"), &AnimationNode::is_filter_enabled);

	ClassDB::bind_method(D_METHOD("get_processing_animation_tree_instance_id"), &AnimationNode::get_processing_animation_tree_instance_id);

	ClassDB::bind_method(D_METHOD("is_process_testing"), &AnimationNode::is_process_testing);

	ClassDB::bind_method(D_METHOD("_set_filters", "filters"), &AnimationNode::_set_filters);
	ClassDB::bind_method(D_METHOD("_get_filters"), &AnimationNode::_get_filters);

	ClassDB::bind_method(D_METHOD("blend_animation", "animation", "time", "delta", "seeked", "is_external_seeking", "blend", "looped_flag"), &AnimationNode::blend_animation_ex, DEFVAL(Animation::LOOPED_FLAG_NONE));
	ClassDB::bind_method(D_METHOD("blend_node", "name", "node", "time", "seek", "is_external_seeking", "blend", "filter", "sync", "test_only"), &AnimationNode::blend_node_ex, DEFVAL(FILTER_IGNORE), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("blend_input", "input_index", "time", "seek", "is_external_seeking", "blend", "filter", "sync", "test_only"), &AnimationNode::blend_input_ex, DEFVAL(FILTER_IGNORE), DEFVAL(true), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_parameter", "name", "value"), &AnimationNode::set_parameter);
	ClassDB::bind_method(D_METHOD("get_parameter", "name"), &AnimationNode::get_parameter);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "filter_enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_filter_enabled", "is_filter_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "filters", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_filters", "_get_filters");

	GDVIRTUAL_BIND(_get_child_nodes);
	GDVIRTUAL_BIND(_get_parameter_list);
	GDVIRTUAL_BIND(_get_child_by_name, "name");
	GDVIRTUAL_BIND(_get_parameter_default_value, "parameter");
	GDVIRTUAL_BIND(_is_parameter_read_only, "parameter");
	GDVIRTUAL_BIND(_process, "time", "seek", "is_external_seeking", "test_only");
	GDVIRTUAL_BIND(_get_caption);
	GDVIRTUAL_BIND(_has_filter);

	ADD_SIGNAL(MethodInfo("tree_changed"));
	ADD_SIGNAL(MethodInfo("animation_node_renamed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
	ADD_SIGNAL(MethodInfo("animation_node_removed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "name")));

	BIND_ENUM_CONSTANT(FILTER_IGNORE);
	BIND_ENUM_CONSTANT(FILTER_PASS);
	BIND_ENUM_CONSTANT(FILTER_STOP);
	BIND_ENUM_CONSTANT(FILTER_BLEND);
}

AnimationNode::AnimationNode() {
}

////////////////////

void AnimationRootNode::_tree_changed() {
	emit_signal(SNAME("tree_changed"));
}

void AnimationRootNode::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	emit_signal(SNAME("animation_node_renamed"), p_oid, p_old_name, p_new_name);
}

void AnimationRootNode::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	emit_signal(SNAME("animation_node_removed"), p_oid, p_node);
}

////////////////////

void AnimationTree::set_root_animation_node(const Ref<AnimationRootNode> &p_animation_node) {
	if (root_animation_node.is_valid()) {
		root_animation_node->disconnect(SNAME("tree_changed"), callable_mp(this, &AnimationTree::_tree_changed));
		root_animation_node->disconnect(SNAME("animation_node_renamed"), callable_mp(this, &AnimationTree::_animation_node_renamed));
		root_animation_node->disconnect(SNAME("animation_node_removed"), callable_mp(this, &AnimationTree::_animation_node_removed));
	}

	root_animation_node = p_animation_node;

	if (root_animation_node.is_valid()) {
		root_animation_node->connect(SNAME("tree_changed"), callable_mp(this, &AnimationTree::_tree_changed));
		root_animation_node->connect(SNAME("animation_node_renamed"), callable_mp(this, &AnimationTree::_animation_node_renamed));
		root_animation_node->connect(SNAME("animation_node_removed"), callable_mp(this, &AnimationTree::_animation_node_removed));
	}

	properties_dirty = true;

	update_configuration_warnings();
}

Ref<AnimationRootNode> AnimationTree::get_root_animation_node() const {
	return root_animation_node;
}

bool AnimationTree::_blend_pre_process(double p_delta, int p_track_count, const AHashMap<NodePath, int> &p_track_map) {
	_update_properties(); // If properties need updating, update them.

	if (root_animation_node.is_null()) {
		return false;
	}

	{ // Setup.
		process_pass++;

		// Init process state.
		process_state = AnimationNode::ProcessState();
		process_state.tree = this;
		process_state.valid = true;
		process_state.invalid_reasons = "";
		process_state.last_pass = process_pass;
		process_state.track_map = &p_track_map;

		// Init node state for root AnimationNode.
		root_animation_node->node_state.track_weights.resize(p_track_count);
		real_t *src_blendsw = root_animation_node->node_state.track_weights.ptr();
		for (int i = 0; i < p_track_count; i++) {
			src_blendsw[i] = 1.0; // By default all go to 1 for the root input.
		}
		root_animation_node->set_node_state_base_path(SNAME(Animation::PARAMETERS_BASE_PATH.ascii().get_data()));
		root_animation_node->node_state.parent = nullptr;
	}

	// Process.
	{
		PlaybackInfo pi;

		if (started) {
			// If started, seek.
			pi.seeked = true;
			pi.delta = p_delta;
			root_animation_node->_pre_process(&process_state, pi, false);
			started = false;
		} else {
			pi.seeked = false;
			pi.delta = p_delta;
			root_animation_node->_pre_process(&process_state, pi, false);
		}
	}

	if (!process_state.valid) {
		return false; // State is not valid, abort process.
	}

	return true;
}

void AnimationTree::_set_active(bool p_active) {
	_set_process(p_active);
	started = p_active;
}

void AnimationTree::set_advance_expression_base_node(const NodePath &p_path) {
	advance_expression_base_node = p_path;
}

NodePath AnimationTree::get_advance_expression_base_node() const {
	return advance_expression_base_node;
}

bool AnimationTree::is_state_invalid() const {
	return !process_state.valid;
}

String AnimationTree::get_invalid_state_reason() const {
	return process_state.invalid_reasons;
}

uint64_t AnimationTree::get_last_process_pass() const {
	return process_pass;
}

PackedStringArray AnimationTree::get_configuration_warnings() const {
	PackedStringArray warnings = AnimationMixer::get_configuration_warnings();
	if (root_animation_node.is_null()) {
		warnings.push_back(RTR("No root AnimationNode for the graph is set."));
	}
	return warnings;
}

void AnimationTree::_tree_changed() {
	if (properties_dirty) {
		return;
	}

	callable_mp(this, &AnimationTree::_update_properties).call_deferred();
	properties_dirty = true;
}

void AnimationTree::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	ERR_FAIL_COND(!property_reference_map.has(p_oid));
	String base_path = property_reference_map[p_oid];
	String old_base = base_path + p_old_name;
	String new_base = base_path + p_new_name;
	for (const PropertyInfo &E : properties) {
		if (E.name.begins_with(old_base)) {
			String new_name = E.name.replace_first(old_base, new_base);
			property_map[new_name] = property_map[E.name];
			property_map.erase(E.name);
		}
	}

	// Update tree second.
	properties_dirty = true;
	_update_properties();
}

void AnimationTree::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	ERR_FAIL_COND(!property_reference_map.has(p_oid));
	String base_path = String(property_reference_map[p_oid]) + String(p_node);
	for (const PropertyInfo &E : properties) {
		if (E.name.begins_with(base_path)) {
			property_map.erase(E.name);
		}
	}

	// Update tree second.
	properties_dirty = true;
	_update_properties();
}

void AnimationTree::_update_properties_for_node(const String &p_base_path, Ref<AnimationNode> p_node) const {
	ERR_FAIL_COND(p_node.is_null());
	if (!property_parent_map.has(p_base_path)) {
		property_parent_map[p_base_path] = AHashMap<StringName, StringName>();
	}
	if (!property_reference_map.has(p_node->get_instance_id())) {
		property_reference_map[p_node->get_instance_id()] = p_base_path;
	}

	if (p_node->get_input_count() && !input_activity_map.has(p_base_path)) {
		LocalVector<Activity> activity;
		for (int i = 0; i < p_node->get_input_count(); i++) {
			Activity a;
			a.activity = 0;
			a.last_pass = 0;
			activity.push_back(a);
		}
		input_activity_map[p_base_path] = activity;
		input_activity_map_get[String(p_base_path).substr(0, String(p_base_path).length() - 1)] = &input_activity_map[p_base_path];
	}

	List<PropertyInfo> plist;
	p_node->get_parameter_list(&plist);
	for (PropertyInfo &pinfo : plist) {
		StringName key = pinfo.name;

		if (!property_map.has(p_base_path + key)) {
			Pair<Variant, bool> param;
			param.first = p_node->get_parameter_default_value(key);
			param.second = p_node->is_parameter_read_only(key);
			property_map[p_base_path + key] = param;
		}

		property_parent_map[p_base_path][key] = p_base_path + key;

		pinfo.name = p_base_path + key;
		properties.push_back(pinfo);
	}
	p_node->make_cache_dirty();
	List<AnimationNode::ChildNode> children;
	p_node->get_child_nodes(&children);

	for (const AnimationNode::ChildNode &E : children) {
		_update_properties_for_node(p_base_path + E.name + "/", E.node);
	}
}

void AnimationTree::_update_properties() const {
	if (!properties_dirty) {
		return;
	}

	properties.clear();
	property_reference_map.clear();
	property_parent_map.clear();
	input_activity_map.clear();
	input_activity_map_get.clear();

	if (root_animation_node.is_valid()) {
		_update_properties_for_node(Animation::PARAMETERS_BASE_PATH, root_animation_node);
	}

	properties_dirty = false;

	const_cast<AnimationTree *>(this)->notify_property_list_changed();
}

void AnimationTree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_setup_animation_player();
			if (active) {
				_set_process(true);
			}
		} break;
	}
}

void AnimationTree::set_animation_player(const NodePath &p_path) {
	animation_player = p_path;
	if (p_path.is_empty()) {
		set_root_node(SceneStringName(path_pp));
		while (animation_libraries.size()) {
			remove_animation_library(animation_libraries[0].name);
		}
	}
	emit_signal(SNAME("animation_player_changed")); // Needs to unpin AnimationPlayerEditor.
	_setup_animation_player();
	notify_property_list_changed();
}

NodePath AnimationTree::get_animation_player() const {
	return animation_player;
}

void AnimationTree::_setup_animation_player() {
	if (!is_inside_tree()) {
		return;
	}

	cache_valid = false;

	if (animation_player.is_empty()) {
		clear_caches();
		return;
	}

	// Using AnimationPlayer here is for compatibility. Changing to AnimationMixer needs extra work like error handling.
	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(get_node_or_null(animation_player));
	if (player) {
		if (!player->is_connected(SNAME("caches_cleared"), callable_mp(this, &AnimationTree::_setup_animation_player))) {
			player->connect(SNAME("caches_cleared"), callable_mp(this, &AnimationTree::_setup_animation_player), CONNECT_DEFERRED);
		}
		if (!player->is_connected(SNAME("animation_list_changed"), callable_mp(this, &AnimationTree::_setup_animation_player))) {
			player->connect(SNAME("animation_list_changed"), callable_mp(this, &AnimationTree::_setup_animation_player), CONNECT_DEFERRED);
		}
		Node *root = player->get_node_or_null(player->get_root_node());
		if (root) {
			set_root_node(get_path_to(root, true));
		}
		while (animation_libraries.size()) {
			remove_animation_library(animation_libraries[0].name);
		}
		List<StringName> list;
		player->get_animation_library_list(&list);
		for (const StringName &E : list) {
			Ref<AnimationLibrary> lib = player->get_animation_library(E);
			if (lib.is_valid()) {
				add_animation_library(E, lib);
			}
		}
	}

	clear_caches();
}

void AnimationTree::_validate_property(PropertyInfo &p_property) const {
	AnimationMixer::_validate_property(p_property);

	if (!animation_player.is_empty()) {
		if (p_property.name == "root_node" || p_property.name.begins_with("libraries")) {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
		if (p_property.name.begins_with("libraries")) {
			p_property.usage &= ~PROPERTY_USAGE_STORAGE;
		}
	}
}

bool AnimationTree::_set(const StringName &p_name, const Variant &p_value) {
#ifndef DISABLE_DEPRECATED
	String name = p_name;
	if (name == "process_callback") {
		set_callback_mode_process(static_cast<AnimationCallbackModeProcess>((int)p_value));
		return true;
	}
#endif // DISABLE_DEPRECATED
	if (properties_dirty) {
		_update_properties();
	}

	if (property_map.has(p_name)) {
		if (is_inside_tree() && property_map[p_name].second) {
			return false; // Prevent to set property by user.
		}
		property_map[p_name].first = p_value;
		return true;
	}

	return false;
}

bool AnimationTree::_get(const StringName &p_name, Variant &r_ret) const {
#ifndef DISABLE_DEPRECATED
	if (p_name == "process_callback") {
		r_ret = get_callback_mode_process();
		return true;
	}
#endif // DISABLE_DEPRECATED
	if (properties_dirty) {
		_update_properties();
	}

	if (property_map.has(p_name)) {
		r_ret = property_map[p_name].first;
		return true;
	}

	return false;
}

void AnimationTree::_get_property_list(List<PropertyInfo> *p_list) const {
	if (properties_dirty) {
		_update_properties();
	}

	for (const PropertyInfo &E : properties) {
		p_list->push_back(E);
	}
}

real_t AnimationTree::get_connection_activity(const StringName &p_path, int p_connection) const {
	if (!input_activity_map_get.has(p_path)) {
		return 0;
	}
	const LocalVector<Activity> *activity = input_activity_map_get[p_path];

	if (!activity || p_connection < 0 || p_connection >= (int64_t)activity->size()) {
		return 0;
	}

	if ((*activity)[p_connection].last_pass != process_pass) {
		return 0;
	}

	return (*activity)[p_connection].activity;
}

void AnimationTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tree_root", "animation_node"), &AnimationTree::set_root_animation_node);
	ClassDB::bind_method(D_METHOD("get_tree_root"), &AnimationTree::get_root_animation_node);

	ClassDB::bind_method(D_METHOD("set_advance_expression_base_node", "path"), &AnimationTree::set_advance_expression_base_node);
	ClassDB::bind_method(D_METHOD("get_advance_expression_base_node"), &AnimationTree::get_advance_expression_base_node);

	ClassDB::bind_method(D_METHOD("set_animation_player", "path"), &AnimationTree::set_animation_player);
	ClassDB::bind_method(D_METHOD("get_animation_player"), &AnimationTree::get_animation_player);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tree_root", PROPERTY_HINT_RESOURCE_TYPE, "AnimationRootNode"), "set_tree_root", "get_tree_root");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "advance_expression_base_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node"), "set_advance_expression_base_node", "get_advance_expression_base_node");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "anim_player", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "AnimationPlayer"), "set_animation_player", "get_animation_player");

	ADD_SIGNAL(MethodInfo(SNAME("animation_player_changed")));
}

AnimationTree::AnimationTree() {
	deterministic = true;
	callback_mode_discrete = ANIMATION_CALLBACK_MODE_DISCRETE_FORCE_CONTINUOUS;
}

AnimationTree::~AnimationTree() {
}
