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

#include "animation_blend_tree.h"
#include "core/engine.h"
#include "core/method_bind_ext.gen.inc"
#include "scene/scene_string_names.h"
#include "servers/audio/audio_stream.h"

void AnimationNode::get_parameter_list(List<PropertyInfo> *r_list) const {
	if (get_script_instance()) {
		Array parameters = get_script_instance()->call("get_parameter_list");
		for (int i = 0; i < parameters.size(); i++) {
			Dictionary d = parameters[i];
			ERR_CONTINUE(d.empty());
			r_list->push_back(PropertyInfo::from_dict(d));
		}
	}
}

Variant AnimationNode::get_parameter_default_value(const StringName &p_parameter) const {
	if (get_script_instance()) {
		return get_script_instance()->call("get_parameter_default_value", p_parameter);
	}
	return Variant();
}

void AnimationNode::set_parameter(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND(!state);
	ERR_FAIL_COND(!state->tree->property_parent_map.has(base_path));
	ERR_FAIL_COND(!state->tree->property_parent_map[base_path].has(p_name));
	StringName path = state->tree->property_parent_map[base_path][p_name];

	state->tree->property_map[path] = p_value;
}

Variant AnimationNode::get_parameter(const StringName &p_name) const {
	ERR_FAIL_COND_V(!state, Variant());
	ERR_FAIL_COND_V(!state->tree->property_parent_map.has(base_path), Variant());
	ERR_FAIL_COND_V(!state->tree->property_parent_map[base_path].has(p_name), Variant());

	StringName path = state->tree->property_parent_map[base_path][p_name];
	return state->tree->property_map[path];
}

void AnimationNode::get_child_nodes(List<ChildNode> *r_child_nodes) {
	if (get_script_instance()) {
		Dictionary cn = get_script_instance()->call("get_child_nodes");
		List<Variant> keys;
		cn.get_key_list(&keys);
		for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
			ChildNode child;
			child.name = E->get();
			child.node = cn[E->get()];
			r_child_nodes->push_back(child);
		}
	}
}

void AnimationNode::blend_animation(const StringName &p_animation, float p_time, float p_delta, bool p_seeked, float p_blend) {
	ERR_FAIL_COND(!state);
	ERR_FAIL_COND(!state->player->has_animation(p_animation));

	Ref<Animation> animation = state->player->get_animation(p_animation);

	if (animation.is_null()) {
		AnimationNodeBlendTree *btree = Object::cast_to<AnimationNodeBlendTree>(parent);
		if (btree) {
			String name = btree->get_node_name(Ref<AnimationNodeAnimation>(this));
			make_invalid(vformat(RTR("In node '%s', invalid animation: '%s'."), name, p_animation));
		} else {
			make_invalid(vformat(RTR("Invalid animation: '%s'."), p_animation));
		}
		return;
	}

	ERR_FAIL_COND(!animation.is_valid());

	AnimationState anim_state;
	anim_state.blend = p_blend;
	anim_state.track_blends = &blends;
	anim_state.delta = p_delta;
	anim_state.time = p_time;
	anim_state.animation = animation;
	anim_state.seeked = p_seeked;
	anim_state.add_directly = add_directly;

	state->animation_states.push_back(anim_state);
}

float AnimationNode::_pre_process(const StringName &p_base_path, AnimationNode *p_parent, State *p_state, float p_time, bool p_seek, const Vector<StringName> &p_connections) {
	base_path = p_base_path;
	parent = p_parent;
	connections = p_connections;
	state = p_state;

	float t = process(p_time, p_seek);

	state = nullptr;
	parent = nullptr;
	base_path = StringName();
	connections.clear();

	return t;
}

void AnimationNode::make_invalid(const String &p_reason) {
	ERR_FAIL_COND(!state);
	state->valid = false;
	if (state->invalid_reasons != String()) {
		state->invalid_reasons += "\n";
	}
	state->invalid_reasons += String::utf8("• ") + p_reason;
}

float AnimationNode::blend_input(int p_input, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize, bool p_add_directly) {
	ERR_FAIL_INDEX_V(p_input, inputs.size(), 0);
	ERR_FAIL_COND_V(!state, 0);

	AnimationNodeBlendTree *blend_tree = Object::cast_to<AnimationNodeBlendTree>(parent);
	ERR_FAIL_COND_V(!blend_tree, 0);

	StringName node_name = connections[p_input];

	if (!blend_tree->has_node(node_name)) {
		String name = blend_tree->get_node_name(Ref<AnimationNode>(this));
		make_invalid(vformat(RTR("Nothing connected to input '%s' of node '%s'."), get_input_name(p_input), name));
		return 0;
	} else {
		Ref<AnimationNode> node = blend_tree->get_node(node_name);
		node->add_directly = p_add_directly;
	}

	Ref<AnimationNode> node = blend_tree->get_node(node_name);

	//inputs.write[p_input].last_pass = state->last_pass;
	float activity = 0;
	float ret = _blend_node(node_name, blend_tree->get_node_connection_array(node_name), nullptr, node, p_time, p_seek, p_blend, p_filter, p_optimize, &activity);

	Vector<AnimationTree::Activity> *activity_ptr = state->tree->input_activity_map.getptr(base_path);

	if (activity_ptr && p_input < activity_ptr->size()) {
		activity_ptr->write[p_input].last_pass = state->last_pass;
		activity_ptr->write[p_input].activity = activity;
	}
	return ret;
}

float AnimationNode::blend_node(const StringName &p_sub_path, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize) {
	return _blend_node(p_sub_path, Vector<StringName>(), this, p_node, p_time, p_seek, p_blend, p_filter, p_optimize);
}

float AnimationNode::_blend_node(const StringName &p_subpath, const Vector<StringName> &p_connections, AnimationNode *p_new_parent, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter, bool p_optimize, float *r_max) {
	ERR_FAIL_COND_V(!p_node.is_valid(), 0);
	ERR_FAIL_COND_V(!state, 0);

	int blend_count = blends.size();

	if (p_node->blends.size() != blend_count) {
		p_node->blends.resize(blend_count);
	}

	float *blendw = p_node->blends.ptrw();
	const float *blendr = blends.ptr();

	bool any_valid = false;

	if (has_filter() && is_filter_enabled() && p_filter != FILTER_IGNORE) {
		for (int i = 0; i < blend_count; i++) {
			blendw[i] = 0.0; //all to zero by default
		}

		const NodePath *K = nullptr;
		while ((K = filter.next(K))) {
			if (!state->track_map.has(*K)) {
				continue;
			}
			int idx = state->track_map[*K];
			blendw[idx] = 1.0; //filtered goes to one
		}

		switch (p_filter) {
			case FILTER_IGNORE:
				break; //will not happen anyway
			case FILTER_PASS: {
				//values filtered pass, the rest don't
				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] == 0) { //not filtered, does not pass
						continue;
					}

					blendw[i] = blendr[i] * p_blend;
					if (blendw[i] > CMP_EPSILON) {
						any_valid = true;
					}
				}

			} break;
			case FILTER_STOP: {
				//values filtered don't pass, the rest are blended

				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] > 0) { //filtered, does not pass
						continue;
					}

					blendw[i] = blendr[i] * p_blend;
					if (blendw[i] > CMP_EPSILON) {
						any_valid = true;
					}
				}

			} break;
			case FILTER_BLEND: {
				//filtered values are blended, the rest are passed without blending

				for (int i = 0; i < blend_count; i++) {
					if (blendw[i] == 1.0) {
						blendw[i] = blendr[i] * p_blend; //filtered, blend
					} else {
						blendw[i] = blendr[i]; //not filtered, do not blend
					}

					if (blendw[i] > CMP_EPSILON) {
						any_valid = true;
					}
				}

			} break;
		}
	} else {
		for (int i = 0; i < blend_count; i++) {
			//regular blend
			blendw[i] = blendr[i] * p_blend;
			if (blendw[i] > CMP_EPSILON) {
				any_valid = true;
			}
		}
	}

	if (r_max) {
		*r_max = 0;
		for (int i = 0; i < blend_count; i++) {
			*r_max = MAX(*r_max, blendw[i]);
		}
	}

	if (!p_seek && p_optimize && !any_valid) { //pointless to go on, all are zero
		return 0;
	}

	String new_path;
	AnimationNode *new_parent;

	//this is the slowest part of processing, but as strings process in powers of 2, and the paths always exist, it will not result in that many allocations
	if (p_new_parent) {
		new_parent = p_new_parent;
		new_path = String(base_path) + String(p_subpath) + "/";
	} else {
		ERR_FAIL_COND_V(!parent, 0);
		new_parent = parent;
		new_path = String(parent->base_path) + String(p_subpath) + "/";
	}
	return p_node->_pre_process(new_path, new_parent, state, p_time, p_seek, p_connections);
}

int AnimationNode::get_input_count() const {
	return inputs.size();
}
String AnimationNode::get_input_name(int p_input) {
	ERR_FAIL_INDEX_V(p_input, inputs.size(), String());
	return inputs[p_input].name;
}

String AnimationNode::get_caption() const {
	if (get_script_instance()) {
		return get_script_instance()->call("get_caption");
	}

	return "Node";
}

void AnimationNode::add_input(const String &p_name) {
	//root nodes can't add inputs
	ERR_FAIL_COND(Object::cast_to<AnimationRootNode>(this) != nullptr);
	Input input;
	ERR_FAIL_COND(p_name.find(".") != -1 || p_name.find("/") != -1);
	input.name = p_name;
	inputs.push_back(input);
	emit_changed();
}

void AnimationNode::set_input_name(int p_input, const String &p_name) {
	ERR_FAIL_INDEX(p_input, inputs.size());
	ERR_FAIL_COND(p_name.find(".") != -1 || p_name.find("/") != -1);
	inputs.write[p_input].name = p_name;
	emit_changed();
}

void AnimationNode::remove_input(int p_index) {
	ERR_FAIL_INDEX(p_index, inputs.size());
	inputs.remove(p_index);
	emit_changed();
}

float AnimationNode::process(float p_time, bool p_seek) {
	if (get_script_instance()) {
		return get_script_instance()->call("process", p_time, p_seek);
	}

	return 0;
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

bool AnimationNode::is_path_filtered(const NodePath &p_path) const {
	return filter.has(p_path);
}

bool AnimationNode::has_filter() const {
	if (get_script_instance()) {
		return get_script_instance()->call("has_filter");
	}
	return false;
}

Array AnimationNode::_get_filters() const {
	Array paths;

	const NodePath *K = nullptr;
	while ((K = filter.next(K))) {
		paths.push_back(String(*K)); //use strings, so sorting is possible
	}
	paths.sort(); //done so every time the scene is saved, it does not change

	return paths;
}
void AnimationNode::_set_filters(const Array &p_filters) {
	filter.clear();
	for (int i = 0; i < p_filters.size(); i++) {
		set_filter_path(p_filters[i], true);
	}
}

void AnimationNode::_validate_property(PropertyInfo &property) const {
	if (!has_filter() && (property.name == "filter_enabled" || property.name == "filters")) {
		property.usage = 0;
	}
}

Ref<AnimationNode> AnimationNode::get_child_by_name(const StringName &p_name) {
	if (get_script_instance()) {
		return get_script_instance()->call("get_child_by_name", p_name);
	}
	return Ref<AnimationNode>();
}

void AnimationNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_input_count"), &AnimationNode::get_input_count);
	ClassDB::bind_method(D_METHOD("get_input_name", "input"), &AnimationNode::get_input_name);

	ClassDB::bind_method(D_METHOD("add_input", "name"), &AnimationNode::add_input);
	ClassDB::bind_method(D_METHOD("remove_input", "index"), &AnimationNode::remove_input);

	ClassDB::bind_method(D_METHOD("set_filter_path", "path", "enable"), &AnimationNode::set_filter_path);
	ClassDB::bind_method(D_METHOD("is_path_filtered", "path"), &AnimationNode::is_path_filtered);

	ClassDB::bind_method(D_METHOD("set_filter_enabled", "enable"), &AnimationNode::set_filter_enabled);
	ClassDB::bind_method(D_METHOD("is_filter_enabled"), &AnimationNode::is_filter_enabled);

	ClassDB::bind_method(D_METHOD("_set_filters", "filters"), &AnimationNode::_set_filters);
	ClassDB::bind_method(D_METHOD("_get_filters"), &AnimationNode::_get_filters);

	ClassDB::bind_method(D_METHOD("blend_animation", "animation", "time", "delta", "seeked", "blend"), &AnimationNode::blend_animation);
	ClassDB::bind_method(D_METHOD("blend_node", "name", "node", "time", "seek", "blend", "filter", "optimize"), &AnimationNode::blend_node, DEFVAL(FILTER_IGNORE), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("blend_input", "input_index", "time", "seek", "blend", "filter", "optimize", "add_directly"), &AnimationNode::blend_input, DEFVAL(FILTER_IGNORE), DEFVAL(true), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_parameter", "name", "value"), &AnimationNode::set_parameter);
	ClassDB::bind_method(D_METHOD("get_parameter", "name"), &AnimationNode::get_parameter);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "filter_enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_filter_enabled", "is_filter_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "filters", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_filters", "_get_filters");

	BIND_VMETHOD(MethodInfo(Variant::DICTIONARY, "get_child_nodes"));
	BIND_VMETHOD(MethodInfo(Variant::ARRAY, "get_parameter_list"));
	BIND_VMETHOD(MethodInfo(Variant::OBJECT, "get_child_by_name", PropertyInfo(Variant::STRING, "name")));
	{
		MethodInfo mi = MethodInfo(Variant::NIL, "get_parameter_default_value", PropertyInfo(Variant::STRING, "name"));
		mi.return_val.usage = PROPERTY_USAGE_NIL_IS_VARIANT;
		BIND_VMETHOD(mi);
	}
	BIND_VMETHOD(MethodInfo("process", PropertyInfo(Variant::REAL, "time"), PropertyInfo(Variant::BOOL, "seek")));
	BIND_VMETHOD(MethodInfo(Variant::STRING, "get_caption"));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "has_filter"));

	ADD_SIGNAL(MethodInfo("removed_from_graph"));

	ADD_SIGNAL(MethodInfo("tree_changed"));

	BIND_ENUM_CONSTANT(FILTER_IGNORE);
	BIND_ENUM_CONSTANT(FILTER_PASS);
	BIND_ENUM_CONSTANT(FILTER_STOP);
	BIND_ENUM_CONSTANT(FILTER_BLEND);
}

AnimationNode::AnimationNode() {
	state = nullptr;
	parent = nullptr;
	filter_enabled = false;
}

////////////////////

void AnimationTree::set_tree_root(const Ref<AnimationNode> &p_root) {
	if (root.is_valid()) {
		root->disconnect("tree_changed", this, "_tree_changed");
	}

	root = p_root;

	if (root.is_valid()) {
		root->connect("tree_changed", this, "_tree_changed");
	}

	properties_dirty = true;

	update_configuration_warning();
}

Ref<AnimationNode> AnimationTree::get_tree_root() const {
	return root;
}

void AnimationTree::set_active(bool p_active) {
	if (active == p_active) {
		return;
	}

	active = p_active;
	started = active;

	if (process_mode == ANIMATION_PROCESS_IDLE) {
		set_process_internal(active);
	} else {
		set_physics_process_internal(active);
	}

	if (!active && is_inside_tree()) {
		for (Set<TrackCache *>::Element *E = playing_caches.front(); E; E = E->next()) {
			if (ObjectDB::get_instance(E->get()->object_id)) {
				E->get()->object->call("stop");
			}
		}

		playing_caches.clear();
	}
}

bool AnimationTree::is_active() const {
	return active;
}

void AnimationTree::set_process_mode(AnimationProcessMode p_mode) {
	if (process_mode == p_mode) {
		return;
	}

	bool was_active = is_active();
	if (was_active) {
		set_active(false);
	}

	process_mode = p_mode;

	if (was_active) {
		set_active(true);
	}
}

AnimationTree::AnimationProcessMode AnimationTree::get_process_mode() const {
	return process_mode;
}

void AnimationTree::_node_removed(Node *p_node) {
	cache_valid = false;
}

bool AnimationTree::_update_caches(AnimationPlayer *player) {
	setup_pass++;

	if (!player->has_node(player->get_root())) {
		ERR_PRINT("AnimationTree: AnimationPlayer root is invalid.");
		set_active(false);
		return false;
	}
	Node *parent = player->get_node(player->get_root());

	List<StringName> sname;
	player->get_animation_list(&sname);

	for (List<StringName>::Element *E = sname.front(); E; E = E->next()) {
		Ref<Animation> anim = player->get_animation(E->get());
		for (int i = 0; i < anim->get_track_count(); i++) {
			NodePath path = anim->track_get_path(i);
			Animation::TrackType track_type = anim->track_get_type(i);

			TrackCache *track = nullptr;
			if (track_cache.has(path)) {
				track = track_cache.get(path);
			}

			//if not valid, delete track
			if (track && (track->type != track_type || ObjectDB::get_instance(track->object_id) == nullptr)) {
				playing_caches.erase(track);
				memdelete(track);
				track_cache.erase(path);
				track = nullptr;
			}

			if (!track) {
				RES resource;
				Vector<StringName> leftover_path;
				Node *child = parent->get_node_and_resource(path, resource, leftover_path);

				if (!child) {
					ERR_PRINT("AnimationTree: '" + String(E->get()) + "', couldn't resolve track:  '" + String(path) + "'");
					continue;
				}

				if (!child->is_connected("tree_exited", this, "_node_removed")) {
					child->connect("tree_exited", this, "_node_removed", varray(child));
				}

				switch (track_type) {
					case Animation::TYPE_VALUE: {
						TrackCacheValue *track_value = memnew(TrackCacheValue);

						if (resource.is_valid()) {
							track_value->object = resource.ptr();
						} else {
							track_value->object = child;
						}

						track_value->subpath = leftover_path;
						track_value->object_id = track_value->object->get_instance_id();

						track = track_value;

					} break;
					case Animation::TYPE_TRANSFORM: {
						Spatial *spatial = Object::cast_to<Spatial>(child);

						if (!spatial) {
							ERR_PRINT("AnimationTree: '" + String(E->get()) + "', transform track does not point to spatial:  '" + String(path) + "'");
							continue;
						}

						TrackCacheTransform *track_xform = memnew(TrackCacheTransform);

						track_xform->spatial = spatial;
						track_xform->skeleton = nullptr;
						track_xform->bone_idx = -1;

						if (path.get_subname_count() == 1 && Object::cast_to<Skeleton>(spatial)) {
							Skeleton *sk = Object::cast_to<Skeleton>(spatial);
							track_xform->skeleton = sk;
							int bone_idx = sk->find_bone(path.get_subname(0));
							if (bone_idx != -1) {
								track_xform->bone_idx = bone_idx;
							}
						}

						track_xform->object = spatial;
						track_xform->object_id = track_xform->object->get_instance_id();

						track = track_xform;

					} break;
					case Animation::TYPE_METHOD: {
						TrackCacheMethod *track_method = memnew(TrackCacheMethod);

						if (resource.is_valid()) {
							track_method->object = resource.ptr();
						} else {
							track_method->object = child;
						}

						track_method->object_id = track_method->object->get_instance_id();

						track = track_method;

					} break;
					case Animation::TYPE_BEZIER: {
						TrackCacheBezier *track_bezier = memnew(TrackCacheBezier);

						if (resource.is_valid()) {
							track_bezier->object = resource.ptr();
						} else {
							track_bezier->object = child;
						}

						track_bezier->subpath = leftover_path;
						track_bezier->object_id = track_bezier->object->get_instance_id();

						track = track_bezier;
					} break;
					case Animation::TYPE_AUDIO: {
						TrackCacheAudio *track_audio = memnew(TrackCacheAudio);

						track_audio->object = child;
						track_audio->object_id = track_audio->object->get_instance_id();

						track = track_audio;

					} break;
					case Animation::TYPE_ANIMATION: {
						TrackCacheAnimation *track_animation = memnew(TrackCacheAnimation);

						track_animation->object = child;
						track_animation->object_id = track_animation->object->get_instance_id();

						track = track_animation;

					} break;
					default: {
						ERR_PRINT("Animation corrupted (invalid track type)");
						continue;
					}
				}

				track_cache[path] = track;
			}

			track->setup_pass = setup_pass;
		}
	}

	List<NodePath> to_delete;

	const NodePath *K = nullptr;
	while ((K = track_cache.next(K))) {
		TrackCache *tc = track_cache[*K];
		if (tc->setup_pass != setup_pass) {
			to_delete.push_back(*K);
		}
	}

	while (to_delete.front()) {
		NodePath np = to_delete.front()->get();
		memdelete(track_cache[np]);
		track_cache.erase(np);
		to_delete.pop_front();
	}

	state.track_map.clear();

	K = nullptr;
	int idx = 0;
	while ((K = track_cache.next(K))) {
		state.track_map[*K] = idx;
		idx++;
	}

	state.track_count = idx;

	cache_valid = true;

	return true;
}

void AnimationTree::_clear_caches() {
	const NodePath *K = nullptr;
	while ((K = track_cache.next(K))) {
		memdelete(track_cache[*K]);
	}
	playing_caches.clear();
	track_cache.clear();
	cache_valid = false;
}

void AnimationTree::_process_graph(float p_delta) {
	_update_properties(); //if properties need updating, update them

	//check all tracks, see if they need modification

	root_motion_transform = Transform();

	if (!root.is_valid()) {
		ERR_PRINT("AnimationTree: root AnimationNode is not set, disabling playback.");
		set_active(false);
		cache_valid = false;
		return;
	}

	if (!has_node(animation_player)) {
		ERR_PRINT("AnimationTree: no valid AnimationPlayer path set, disabling playback");
		set_active(false);
		cache_valid = false;
		return;
	}

	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(get_node(animation_player));

	ObjectID current_animation_player = 0;

	if (player) {
		current_animation_player = player->get_instance_id();
	}

	if (last_animation_player != current_animation_player) {
		if (last_animation_player) {
			Object *old_player = ObjectDB::get_instance(last_animation_player);
			if (old_player) {
				old_player->disconnect("caches_cleared", this, "_clear_caches");
			}
		}

		if (player) {
			player->connect("caches_cleared", this, "_clear_caches");
		}

		last_animation_player = current_animation_player;
	}

	if (!player) {
		ERR_PRINT("AnimationTree: path points to a node not an AnimationPlayer, disabling playback");
		set_active(false);
		cache_valid = false;
		return;
	}

	if (!cache_valid) {
		if (!_update_caches(player)) {
			return;
		}
	}

	{ //setup

		process_pass++;

		state.valid = true;
		state.invalid_reasons = "";
		state.animation_states.clear(); //will need to be re-created
		state.valid = true;
		state.player = player;
		state.last_pass = process_pass;
		state.tree = this;

		// root source blends

		root->blends.resize(state.track_count);
		float *src_blendsw = root->blends.ptrw();
		for (int i = 0; i < state.track_count; i++) {
			src_blendsw[i] = 1.0; //by default all go to 1 for the root input
		}
	}

	//process

	{
		if (started) {
			//if started, seek
			root->_pre_process(SceneStringNames::get_singleton()->parameters_base_path, nullptr, &state, 0, true, Vector<StringName>());
			started = false;
		}

		root->_pre_process(SceneStringNames::get_singleton()->parameters_base_path, nullptr, &state, p_delta, false, Vector<StringName>());
	}

	if (!state.valid) {
		return; //state is not valid. do nothing.
	}
	//apply value/transform/bezier blends to track caches and execute method/audio/animation tracks

	{
		bool can_call = is_inside_tree() && !Engine::get_singleton()->is_editor_hint();

		for (List<AnimationNode::AnimationState>::Element *E = state.animation_states.front(); E; E = E->next()) {
			const AnimationNode::AnimationState &as = E->get();

			Ref<Animation> a = as.animation;
			float time = as.time;
			float delta = as.delta;
			float weight = as.blend;
			bool seeked = as.seeked;

			for (int i = 0; i < a->get_track_count(); i++) {
				NodePath path = a->track_get_path(i);

				TrackCache **track_pp = track_cache.getptr(path);
				ERR_CONTINUE(!track_pp);

				TrackCache *track = *track_pp;
				if (track->type != a->track_get_type(i)) {
					continue; //may happen should not
				}

				track->root_motion = root_motion_track == path;

				int *blend_idx_p = state.track_map.getptr(path);
				ERR_CONTINUE(!blend_idx_p);
				int blend_idx = *blend_idx_p;

				ERR_CONTINUE(blend_idx < 0 || blend_idx >= state.track_count);

				float blend = (*as.track_blends)[blend_idx] * weight;

				if (blend < CMP_EPSILON) {
					continue; //nothing to blend
				}

				switch (track->type) {
					case Animation::TYPE_TRANSFORM: {
						TrackCacheTransform *t = static_cast<TrackCacheTransform *>(track);

						if (track->root_motion) {
							if (t->process_pass != process_pass) {
								t->process_pass = process_pass;
								t->loc = Vector3();
								t->rot = Quat();
								t->rot_blend_accum = 0;
								t->scale = Vector3(1, 1, 1);
							}

							float prev_time = time - delta;
							if (prev_time < 0) {
								if (!a->has_loop()) {
									prev_time = 0;
								} else {
									prev_time = a->get_length() + prev_time;
								}
							}

							Vector3 loc[2];
							Quat rot[2];
							Vector3 scale[2];

							if (prev_time > time) {
								Error err = a->transform_track_interpolate(i, prev_time, &loc[0], &rot[0], &scale[0]);
								if (err != OK) {
									continue;
								}

								a->transform_track_interpolate(i, a->get_length(), &loc[1], &rot[1], &scale[1]);

								t->loc += (loc[1] - loc[0]) * blend;
								t->scale += (scale[1] - scale[0]) * blend;
								Quat q = Quat().slerp(rot[0].normalized().inverse() * rot[1].normalized(), blend).normalized();
								t->rot = (t->rot * q).normalized();

								prev_time = 0;
							}

							Error err = a->transform_track_interpolate(i, prev_time, &loc[0], &rot[0], &scale[0]);
							if (err != OK) {
								continue;
							}

							a->transform_track_interpolate(i, time, &loc[1], &rot[1], &scale[1]);

							t->loc += (loc[1] - loc[0]) * blend;
							t->scale += (scale[1] - scale[0]) * blend;
							Quat q = Quat().slerp(rot[0].normalized().inverse() * rot[1].normalized(), blend).normalized();
							t->rot = (t->rot * q).normalized();

							prev_time = 0;

						} else {
							Vector3 loc;
							Quat rot;
							Vector3 scale;

							Error err = a->transform_track_interpolate(i, time, &loc, &rot, &scale);
							//ERR_CONTINUE(err!=OK); //used for testing, should be removed

							if (t->process_pass != process_pass) {
								t->process_pass = process_pass;
								t->loc = loc;
								t->rot = rot;
								t->rot_blend_accum = 0;
								t->scale = scale;
							}

							if (err != OK) {
								continue;
							}

							if (!as.add_directly) {
								t->loc = t->loc.linear_interpolate(loc, blend);
								if (t->rot_blend_accum == 0) {
									t->rot = rot;
									t->rot_blend_accum = blend;
								} else {
									float rot_total = t->rot_blend_accum + blend;
									t->rot = rot.slerp(t->rot, t->rot_blend_accum / rot_total).normalized();
									t->rot_blend_accum = rot_total;
								}
								t->scale = t->scale.linear_interpolate(scale, blend);
							} else {
								t->loc += loc * blend;
								t->scale = t->scale.linear_interpolate(scale, blend);
								Quat q = Quat().slerp(rot.normalized(), blend).normalized();
								t->rot = (t->rot * q).normalized();
							}
						}

					} break;
					case Animation::TYPE_VALUE: {
						TrackCacheValue *t = static_cast<TrackCacheValue *>(track);

						Animation::UpdateMode update_mode = a->value_track_get_update_mode(i);

						if (update_mode == Animation::UPDATE_CONTINUOUS || update_mode == Animation::UPDATE_CAPTURE) { //delta == 0 means seek

							Variant value = a->value_track_interpolate(i, time);

							if (value == Variant()) {
								continue;
							}

							if (t->process_pass != process_pass) {
								t->value = value;
								t->process_pass = process_pass;
							}

							if (as.add_directly) {
								Variant::blend(t->value, value, blend, t->value);
							} else {
								Variant::interpolate(t->value, value, blend, t->value);
							}

						} else {
							List<int> indices;
							a->value_track_get_key_indices(i, time, delta, &indices);

							for (List<int>::Element *F = indices.front(); F; F = F->next()) {
								Variant value = a->track_get_key_value(i, F->get());
								t->object->set_indexed(t->subpath, value);
							}
						}

					} break;
					case Animation::TYPE_METHOD: {
						if (delta == 0) {
							continue;
						}
						TrackCacheMethod *t = static_cast<TrackCacheMethod *>(track);

						List<int> indices;

						a->method_track_get_key_indices(i, time, delta, &indices);

						for (List<int>::Element *F = indices.front(); F; F = F->next()) {
							StringName method = a->method_track_get_name(i, F->get());
							Vector<Variant> params = a->method_track_get_params(i, F->get());

							int s = params.size();

							static_assert(VARIANT_ARG_MAX == 8, "This code needs to be updated if VARIANT_ARG_MAX != 8");
							ERR_CONTINUE(s > VARIANT_ARG_MAX);
							if (can_call) {
								t->object->call_deferred(
										method,
										s >= 1 ? params[0] : Variant(),
										s >= 2 ? params[1] : Variant(),
										s >= 3 ? params[2] : Variant(),
										s >= 4 ? params[3] : Variant(),
										s >= 5 ? params[4] : Variant(),
										s >= 6 ? params[5] : Variant(),
										s >= 7 ? params[6] : Variant(),
										s >= 8 ? params[7] : Variant());
							}
						}

					} break;
					case Animation::TYPE_BEZIER: {
						TrackCacheBezier *t = static_cast<TrackCacheBezier *>(track);

						float bezier = a->bezier_track_interpolate(i, time);

						if (t->process_pass != process_pass) {
							t->value = bezier;
							t->process_pass = process_pass;
						}

						if (as.add_directly) {
							t->value += bezier * blend;
						} else {
							t->value = Math::lerp(t->value, bezier, blend);
						}

					} break;
					case Animation::TYPE_AUDIO: {
						TrackCacheAudio *t = static_cast<TrackCacheAudio *>(track);

						if (seeked) {
							//find whatever should be playing
							int idx = a->track_find_key(i, time);
							if (idx < 0) {
								continue;
							}

							Ref<AudioStream> stream = a->audio_track_get_key_stream(i, idx);
							if (!stream.is_valid()) {
								t->object->call("stop");
								t->playing = false;
								playing_caches.erase(t);
							} else {
								float start_ofs = a->audio_track_get_key_start_offset(i, idx);
								start_ofs += time - a->track_get_key_time(i, idx);
								float end_ofs = a->audio_track_get_key_end_offset(i, idx);
								float len = stream->get_length();

								if (start_ofs > len - end_ofs) {
									t->object->call("stop");
									t->playing = false;
									playing_caches.erase(t);
									continue;
								}

								t->object->call("set_stream", stream);
								t->object->call("play", start_ofs);

								t->playing = true;
								playing_caches.insert(t);
								if (len && end_ofs > 0) { //force a end at a time
									t->len = len - start_ofs - end_ofs;
								} else {
									t->len = 0;
								}

								t->start = time;
							}

						} else {
							//find stuff to play
							List<int> to_play;
							a->track_get_key_indices_in_range(i, time, delta, &to_play);
							if (to_play.size()) {
								int idx = to_play.back()->get();

								Ref<AudioStream> stream = a->audio_track_get_key_stream(i, idx);
								if (!stream.is_valid()) {
									t->object->call("stop");
									t->playing = false;
									playing_caches.erase(t);
								} else {
									float start_ofs = a->audio_track_get_key_start_offset(i, idx);
									float end_ofs = a->audio_track_get_key_end_offset(i, idx);
									float len = stream->get_length();

									t->object->call("set_stream", stream);
									t->object->call("play", start_ofs);

									t->playing = true;
									playing_caches.insert(t);
									if (len && end_ofs > 0) { //force a end at a time
										t->len = len - start_ofs - end_ofs;
									} else {
										t->len = 0;
									}

									t->start = time;
								}
							} else if (t->playing) {
								bool loop = a->has_loop();

								bool stop = false;

								if (!loop && time < t->start) {
									stop = true;
								} else if (t->len > 0) {
									float len = t->start > time ? (a->get_length() - t->start) + time : time - t->start;

									if (len > t->len) {
										stop = true;
									}
								}

								if (stop) {
									//time to stop
									t->object->call("stop");
									t->playing = false;
									playing_caches.erase(t);
								}
							}
						}

						float db = Math::linear2db(MAX(blend, 0.00001));
						if (t->object->has_method("set_unit_db")) {
							t->object->call("set_unit_db", db);
						} else {
							t->object->call("set_volume_db", db);
						}
					} break;
					case Animation::TYPE_ANIMATION: {
						TrackCacheAnimation *t = static_cast<TrackCacheAnimation *>(track);

						AnimationPlayer *player2 = Object::cast_to<AnimationPlayer>(t->object);

						if (!player2) {
							continue;
						}

						if (delta == 0 || seeked) {
							//seek
							int idx = a->track_find_key(i, time);
							if (idx < 0) {
								continue;
							}

							float pos = a->track_get_key_time(i, idx);

							StringName anim_name = a->animation_track_get_key_animation(i, idx);
							if (String(anim_name) == "[stop]" || !player2->has_animation(anim_name)) {
								continue;
							}

							Ref<Animation> anim = player2->get_animation(anim_name);

							float at_anim_pos;

							if (anim->has_loop()) {
								at_anim_pos = Math::fposmod(time - pos, anim->get_length()); //seek to loop
							} else {
								at_anim_pos = MAX(anim->get_length(), time - pos); //seek to end
							}

							if (player2->is_playing() || seeked) {
								player2->play(anim_name);
								player2->seek(at_anim_pos);
								t->playing = true;
								playing_caches.insert(t);
							} else {
								player2->set_assigned_animation(anim_name);
								player2->seek(at_anim_pos, true);
							}
						} else {
							//find stuff to play
							List<int> to_play;
							a->track_get_key_indices_in_range(i, time, delta, &to_play);
							if (to_play.size()) {
								int idx = to_play.back()->get();

								StringName anim_name = a->animation_track_get_key_animation(i, idx);
								if (String(anim_name) == "[stop]" || !player2->has_animation(anim_name)) {
									if (playing_caches.has(t)) {
										playing_caches.erase(t);
										player2->stop();
										t->playing = false;
									}
								} else {
									player2->play(anim_name);
									t->playing = true;
									playing_caches.insert(t);
								}
							}
						}

					} break;
				}
			}
		}
	}

	{
		// finally, set the tracks
		const NodePath *K = nullptr;
		while ((K = track_cache.next(K))) {
			TrackCache *track = track_cache[*K];
			if (track->process_pass != process_pass) {
				continue; //not processed, ignore
			}

			switch (track->type) {
				case Animation::TYPE_TRANSFORM: {
					TrackCacheTransform *t = static_cast<TrackCacheTransform *>(track);

					Transform xform;
					xform.origin = t->loc;

					xform.basis.set_quat_scale(t->rot, t->scale);

					if (t->root_motion) {
						root_motion_transform = xform;

						if (t->skeleton && t->bone_idx >= 0) {
							root_motion_transform = (t->skeleton->get_bone_rest(t->bone_idx) * root_motion_transform) * t->skeleton->get_bone_rest(t->bone_idx).affine_inverse();
						}
					} else if (t->skeleton && t->bone_idx >= 0) {
						t->skeleton->set_bone_pose(t->bone_idx, xform);

					} else if (!t->skeleton) {
						t->spatial->set_transform(xform);
					}

				} break;
				case Animation::TYPE_VALUE: {
					TrackCacheValue *t = static_cast<TrackCacheValue *>(track);

					t->object->set_indexed(t->subpath, t->value);

				} break;
				case Animation::TYPE_BEZIER: {
					TrackCacheBezier *t = static_cast<TrackCacheBezier *>(track);

					t->object->set_indexed(t->subpath, t->value);

				} break;
				default: {
				} //the rest don't matter
			}
		}
	}
}

void AnimationTree::advance(float p_time) {
	_process_graph(p_time);
}

void AnimationTree::_notification(int p_what) {
	if (active && OS::get_singleton()->is_update_pending()) {
		if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS && process_mode == ANIMATION_PROCESS_PHYSICS) {
			_process_graph(get_physics_process_delta_time());
		}

		if (p_what == NOTIFICATION_INTERNAL_PROCESS && process_mode == ANIMATION_PROCESS_IDLE) {
			_process_graph(get_process_delta_time());
		}
	}

	if (p_what == NOTIFICATION_EXIT_TREE) {
		_clear_caches();
		if (last_animation_player) {
			Object *player = ObjectDB::get_instance(last_animation_player);
			if (player) {
				player->disconnect("caches_cleared", this, "_clear_caches");
			}
		}
	} else if (p_what == NOTIFICATION_ENTER_TREE) {
		if (last_animation_player) {
			Object *player = ObjectDB::get_instance(last_animation_player);
			if (player) {
				player->connect("caches_cleared", this, "_clear_caches");
			}
		}
	}
}

void AnimationTree::set_animation_player(const NodePath &p_player) {
	animation_player = p_player;
	update_configuration_warning();
}

NodePath AnimationTree::get_animation_player() const {
	return animation_player;
}

bool AnimationTree::is_state_invalid() const {
	return !state.valid;
}
String AnimationTree::get_invalid_state_reason() const {
	return state.invalid_reasons;
}

uint64_t AnimationTree::get_last_process_pass() const {
	return process_pass;
}

String AnimationTree::get_configuration_warning() const {
	String warning = Node::get_configuration_warning();

	if (!root.is_valid()) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("No root AnimationNode for the graph is set.");
	}

	if (!has_node(animation_player)) {
		if (warning != String()) {
			warning += "\n\n";
		}

		warning += TTR("Path to an AnimationPlayer node containing animations is not set.");
		return warning;
	}

	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(get_node(animation_player));

	if (!player) {
		if (warning != String()) {
			warning += "\n\n";
		}

		warning += TTR("Path set for AnimationPlayer does not lead to an AnimationPlayer node.");
		return warning;
	}

	if (!player->has_node(player->get_root())) {
		if (warning != String()) {
			warning += "\n\n";
		}

		warning += TTR("The AnimationPlayer root node is not a valid node.");
		return warning;
	}

	return warning;
}

void AnimationTree::set_root_motion_track(const NodePath &p_track) {
	root_motion_track = p_track;
}

NodePath AnimationTree::get_root_motion_track() const {
	return root_motion_track;
}

Transform AnimationTree::get_root_motion_transform() const {
	return root_motion_transform;
}

void AnimationTree::_tree_changed() {
	if (properties_dirty) {
		return;
	}

	call_deferred("_update_properties");
	properties_dirty = true;
}

void AnimationTree::_update_properties_for_node(const String &p_base_path, Ref<AnimationNode> node) {
	ERR_FAIL_COND(node.is_null());
	if (!property_parent_map.has(p_base_path)) {
		property_parent_map[p_base_path] = HashMap<StringName, StringName>();
	}

	if (node->get_input_count() && !input_activity_map.has(p_base_path)) {
		Vector<Activity> activity;
		for (int i = 0; i < node->get_input_count(); i++) {
			Activity a;
			a.activity = 0;
			a.last_pass = 0;
			activity.push_back(a);
		}
		input_activity_map[p_base_path] = activity;
		input_activity_map_get[String(p_base_path).substr(0, String(p_base_path).length() - 1)] = &input_activity_map[p_base_path];
	}

	List<PropertyInfo> plist;
	node->get_parameter_list(&plist);
	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		PropertyInfo pinfo = E->get();

		StringName key = pinfo.name;

		if (!property_map.has(p_base_path + key)) {
			property_map[p_base_path + key] = node->get_parameter_default_value(key);
		}

		property_parent_map[p_base_path][key] = p_base_path + key;

		pinfo.name = p_base_path + key;
		properties.push_back(pinfo);
	}

	List<AnimationNode::ChildNode> children;
	node->get_child_nodes(&children);

	for (List<AnimationNode::ChildNode>::Element *E = children.front(); E; E = E->next()) {
		_update_properties_for_node(p_base_path + E->get().name + "/", E->get().node);
	}
}

void AnimationTree::_update_properties() {
	if (!properties_dirty) {
		return;
	}

	properties.clear();
	property_parent_map.clear();
	input_activity_map.clear();
	input_activity_map_get.clear();

	if (root.is_valid()) {
		_update_properties_for_node(SceneStringNames::get_singleton()->parameters_base_path, root);
	}

	properties_dirty = false;

	_change_notify();
}

bool AnimationTree::_set(const StringName &p_name, const Variant &p_value) {
	if (properties_dirty) {
		_update_properties();
	}

	if (property_map.has(p_name)) {
		property_map[p_name] = p_value;
#ifdef TOOLS_ENABLED
		_change_notify(p_name.operator String().utf8().get_data());
#endif
		return true;
	}

	return false;
}

bool AnimationTree::_get(const StringName &p_name, Variant &r_ret) const {
	if (properties_dirty) {
		const_cast<AnimationTree *>(this)->_update_properties();
	}

	if (property_map.has(p_name)) {
		r_ret = property_map[p_name];
		return true;
	}

	return false;
}
void AnimationTree::_get_property_list(List<PropertyInfo> *p_list) const {
	if (properties_dirty) {
		const_cast<AnimationTree *>(this)->_update_properties();
	}

	for (const List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
		p_list->push_back(E->get());
	}
}

void AnimationTree::rename_parameter(const String &p_base, const String &p_new_base) {
	//rename values first
	for (const List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
		if (E->get().name.begins_with(p_base)) {
			String new_name = E->get().name.replace_first(p_base, p_new_base);
			property_map[new_name] = property_map[E->get().name];
		}
	}

	//update tree second
	properties_dirty = true;
	_update_properties();
}

float AnimationTree::get_connection_activity(const StringName &p_path, int p_connection) const {
	if (!input_activity_map_get.has(p_path)) {
		return 0;
	}
	const Vector<Activity> *activity = input_activity_map_get[p_path];

	if (!activity || p_connection < 0 || p_connection >= activity->size()) {
		return 0;
	}

	if ((*activity)[p_connection].last_pass != process_pass) {
		return 0;
	}

	return (*activity)[p_connection].activity;
}

void AnimationTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_active", "active"), &AnimationTree::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &AnimationTree::is_active);

	ClassDB::bind_method(D_METHOD("set_tree_root", "root"), &AnimationTree::set_tree_root);
	ClassDB::bind_method(D_METHOD("get_tree_root"), &AnimationTree::get_tree_root);

	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &AnimationTree::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &AnimationTree::get_process_mode);

	ClassDB::bind_method(D_METHOD("set_animation_player", "root"), &AnimationTree::set_animation_player);
	ClassDB::bind_method(D_METHOD("get_animation_player"), &AnimationTree::get_animation_player);

	ClassDB::bind_method(D_METHOD("set_root_motion_track", "path"), &AnimationTree::set_root_motion_track);
	ClassDB::bind_method(D_METHOD("get_root_motion_track"), &AnimationTree::get_root_motion_track);

	ClassDB::bind_method(D_METHOD("get_root_motion_transform"), &AnimationTree::get_root_motion_transform);

	ClassDB::bind_method(D_METHOD("_tree_changed"), &AnimationTree::_tree_changed);
	ClassDB::bind_method(D_METHOD("_update_properties"), &AnimationTree::_update_properties);

	ClassDB::bind_method(D_METHOD("rename_parameter", "old_name", "new_name"), &AnimationTree::rename_parameter);

	ClassDB::bind_method(D_METHOD("advance", "delta"), &AnimationTree::advance);

	ClassDB::bind_method(D_METHOD("_node_removed"), &AnimationTree::_node_removed);
	ClassDB::bind_method(D_METHOD("_clear_caches"), &AnimationTree::_clear_caches);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tree_root", PROPERTY_HINT_RESOURCE_TYPE, "AnimationRootNode"), "set_tree_root", "get_tree_root");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "anim_player", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "AnimationPlayer"), "set_animation_player", "get_animation_player");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_process_mode", "get_process_mode");
	ADD_GROUP("Root Motion", "root_motion_");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_motion_track"), "set_root_motion_track", "get_root_motion_track");

	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(ANIMATION_PROCESS_MANUAL);
}

AnimationTree::AnimationTree() {
	process_mode = ANIMATION_PROCESS_IDLE;
	active = false;
	cache_valid = false;
	setup_pass = 1;
	process_pass = 1;
	started = true;
	properties_dirty = true;
	last_animation_player = 0;
}

AnimationTree::~AnimationTree() {
}
