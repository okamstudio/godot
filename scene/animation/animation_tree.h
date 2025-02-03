/**************************************************************************/
/*  animation_tree.h                                                      */
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

#ifndef ANIMATION_TREE_H
#define ANIMATION_TREE_H

#include "animation_player.h"
#include "scene/3d/skeleton.h"
#include "scene/3d/spatial.h"
#include "scene/resources/animation.h"

class AnimationNodeBlendTree;
class AnimationPlayer;
class AnimationTree;

class AnimationNode : public Resource {
	GDCLASS(AnimationNode, Resource);

public:
	enum FilterAction {
		FILTER_IGNORE,
		FILTER_PASS,
		FILTER_STOP,
		FILTER_BLEND
	};

	struct Input {
		String name;
	};

	Vector<Input> inputs;

	friend class AnimationTree;

	struct AnimationState {
		Ref<Animation> animation;
		float time;
		float delta;
		const Vector<float> *track_blends;
		float blend;
		bool seeked;
		bool use_blend;
	};

	struct State {
		int track_count;
		HashMap<NodePath, int> track_map;
		List<AnimationState> animation_states;
		bool valid;
		AnimationPlayer *player;
		AnimationTree *tree;
		String invalid_reasons;
		uint64_t last_pass;
	};

	Vector<float> blends;
	State *state;

	float _pre_process(const StringName &p_base_path, AnimationNode *p_parent, State *p_state, float p_time, bool p_seek, const Vector<StringName> &p_connections);

	//all this is temporary
	StringName base_path;
	Vector<StringName> connections;
	AnimationNode *parent;

	HashMap<NodePath, bool> filter;
	bool filter_enabled;
	bool use_blend;

	Array _get_filters() const;
	void _set_filters(const Array &p_filters);
	friend class AnimationNodeBlendTree;
	float _blend_node(const StringName &p_subpath, const Vector<StringName> &p_connections, AnimationNode *p_new_parent, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true, float *r_max = nullptr, bool p_use_blend = true);

protected:
	void blend_animation(const StringName &p_animation, float p_time, float p_delta, bool p_seeked, float p_blend);
	float blend_node(const StringName &p_sub_path, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true, bool p_use_blend = true);
	float blend_input(int p_input, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true, bool p_use_blend = true);
	void make_invalid(const String &p_reason);

	static void _bind_methods();

	void _validate_property(PropertyInfo &property) const;

public:
	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

	void set_parameter(const StringName &p_name, const Variant &p_value);
	Variant get_parameter(const StringName &p_name) const;

	struct ChildNode {
		StringName name;
		Ref<AnimationNode> node;
	};

	virtual void get_child_nodes(List<ChildNode> *r_child_nodes);

	virtual float process(float p_time, bool p_seek);
	virtual String get_caption() const;

	int get_input_count() const;
	String get_input_name(int p_input);

	void add_input(const String &p_name);
	void set_input_name(int p_input, const String &p_name);
	void remove_input(int p_index);

	void set_filter_path(const NodePath &p_path, bool p_enable);
	bool is_path_filtered(const NodePath &p_path) const;

	void set_filter_enabled(bool p_enable);
	bool is_filter_enabled() const;

	virtual bool has_filter() const;

	virtual Ref<AnimationNode> get_child_by_name(const StringName &p_name);

	AnimationNode();
};

VARIANT_ENUM_CAST(AnimationNode::FilterAction)

//root node does not allow inputs
class AnimationRootNode : public AnimationNode {
	GDCLASS(AnimationRootNode, AnimationNode);

public:
	AnimationRootNode() {}
};

class AnimationTree : public Node {
	GDCLASS(AnimationTree, Node);

public:
	enum AnimationProcessMode {
		ANIMATION_PROCESS_PHYSICS,
		ANIMATION_PROCESS_IDLE,
		ANIMATION_PROCESS_MANUAL,
	};

private:
	struct TrackCache {
		bool root_motion;
		uint64_t setup_pass;
		uint64_t process_pass;
		Animation::TrackType type;
		Object *object;
		ObjectID object_id;

		TrackCache() {
			root_motion = false;
			setup_pass = 0;
			process_pass = 0;
			object = nullptr;
			object_id = 0;
		}
		virtual ~TrackCache() {}
	};

	struct TrackCacheTransform : public TrackCache {
		Spatial *spatial;
		Skeleton *skeleton;
		int bone_idx;
		Vector3 loc;
		Quat rot;
		float rot_blend_accum;
		Vector3 scale;

		TrackCacheTransform() {
			type = Animation::TYPE_TRANSFORM;
			spatial = nullptr;
			bone_idx = -1;
			skeleton = nullptr;
		}
	};

	struct TrackCacheValue : public TrackCache {
		Variant value;
		Vector<StringName> subpath;
		TrackCacheValue() { type = Animation::TYPE_VALUE; }
	};

	struct TrackCacheMethod : public TrackCache {
		TrackCacheMethod() { type = Animation::TYPE_METHOD; }
	};

	struct TrackCacheBezier : public TrackCache {
		float value;
		Vector<StringName> subpath;
		TrackCacheBezier() {
			type = Animation::TYPE_BEZIER;
			value = 0;
		}
	};

	struct TrackCacheAudio : public TrackCache {
		bool playing;
		float start;
		float len;

		TrackCacheAudio() {
			type = Animation::TYPE_AUDIO;
			playing = false;
			start = 0;
			len = 0;
		}
	};

	struct TrackCacheAnimation : public TrackCache {
		bool playing;

		TrackCacheAnimation() {
			type = Animation::TYPE_ANIMATION;
			playing = false;
		}
	};

	HashMap<NodePath, TrackCache *> track_cache;
	Set<TrackCache *> playing_caches;

	Ref<AnimationNode> root;

	AnimationProcessMode process_mode;
	bool active;
	NodePath animation_player;

	AnimationNode::State state;
	bool cache_valid;
	void _node_removed(Node *p_node);

	void _clear_caches();
	bool _update_caches(AnimationPlayer *player);
	void _process_graph(float p_delta);

	uint64_t setup_pass;
	uint64_t process_pass;

	bool started;

	NodePath root_motion_track;
	Transform root_motion_transform;

	friend class AnimationNode;
	bool properties_dirty;
	void _tree_changed();
	void _update_properties();
	List<PropertyInfo> properties;
	HashMap<StringName, HashMap<StringName, StringName>> property_parent_map;
	HashMap<StringName, Variant> property_map;

	struct Activity {
		uint64_t last_pass;
		float activity;
	};

	HashMap<StringName, Vector<Activity>> input_activity_map;
	HashMap<StringName, Vector<Activity> *> input_activity_map_get;

	void _update_properties_for_node(const String &p_base_path, Ref<AnimationNode> node);

	ObjectID last_animation_player;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_tree_root(const Ref<AnimationNode> &p_root);
	Ref<AnimationNode> get_tree_root() const;

	void set_active(bool p_active);
	bool is_active() const;

	void set_process_mode(AnimationProcessMode p_mode);
	AnimationProcessMode get_process_mode() const;

	void set_animation_player(const NodePath &p_player);
	NodePath get_animation_player() const;

	virtual String get_configuration_warning() const;

	bool is_state_invalid() const;
	String get_invalid_state_reason() const;

	void set_root_motion_track(const NodePath &p_track);
	NodePath get_root_motion_track() const;

	Transform get_root_motion_transform() const;

	float get_connection_activity(const StringName &p_path, int p_connection) const;
	void advance(float p_time);

	void rename_parameter(const String &p_base, const String &p_new_base);

	uint64_t get_last_process_pass() const;
	AnimationTree();
	~AnimationTree();
};

VARIANT_ENUM_CAST(AnimationTree::AnimationProcessMode)

#endif // ANIMATION_TREE_H
