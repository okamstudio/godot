#ifndef ANIMATION_NODE_MOTION_MATCH_H
#define ANIMATION_NODE_MOTION_MATCH_H

#include "frame_model.h"

#include "core/reference.h"
#include "editor/plugins/animation_tree_editor_plugin.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/animation/animation_tree.h"
#include <limits>

class AnimationNodeMotionMatch : public AnimationRootNode {
	GDCLASS(AnimationNodeMotionMatch, AnimationRootNode)

	Vector<NodePath> matching_tracks;
	// parameters
	StringName vel;
	StringName pos;
	StringName min;
	StringName pvst;
	StringName samples;
	StringName f_time;

	StringName main;
	// variables used during matching
	bool first_time = true;
	float c_time = 0;
	bool timeout = false;
	Vector3 v = Vector3();
	// KDNode Struct
	struct KDNode : public Reference {
		/*th -> Threshold*/
		// Variables
		Vector<float> point_indices;
		Ref<KDNode> left;
		Ref<KDNode> right;
		float split_th;
		int32_t split_axis;
		// Methods
		bool are_all_points_same(Vector<float> point_coordinates, int32_t dim_len);
		KDNode *prev;
		void calculate_threshold(Vector<float> points, int32_t dim_len);

		void add_index(uint32_t i);
		void clear_indices();
		Vector<float> get_indices();

		void leaf_split(Vector<float> point_coordinates, int32_t dim_len,
				int32_t dim, int32_t min_leaves);

		KDNode *get_left();
		KDNode *get_right();
		KDNode *get_prev() { return prev; }

		int32_t get_split_axis() { return split_axis; }

		void set_th(float th);
		float get_th();
		KDNode();
	};

	Vector<frame_model *> *keys = new Vector<frame_model *>();
	Vector<float> future_traj;
	Vector<float> point_coordinates;
	Ref<KDNode> root;
	int dim_len; /*no of dimensions*/
	int32_t start_index; /*Axis relative to which the first split occurs*/
	int32_t min_leaves; /*Minimum leafs in nodes at the end level*/
	bool error = false;

	enum errortype { LOAD_POINT_ERROR,
		QUERY_POINT_ERROR,
		K_ERROR };

	Vector3 velocity;

protected:
	static void _bind_methods();
	Variant get_parameter_default_value(
			const StringName &p_parameter) const override;

public:
	Skeleton3D *skeleton;
	NodePath root_track = NodePath();
	int r_index;
	bool done = false;
	bool editing = false;
	float delta_time;
	void get_parameter_list(List<PropertyInfo> *r_list) const override;

	float process(float p_time, bool p_seek) override;
	void add_matching_track(const NodePath &p_track_path);
	void remove_matching_track(const NodePath &p_track_path);
	bool is_matching_track(const NodePath &p_track_path) const;
	Vector<NodePath> get_matching_tracks();
	void update_motion_database(AnimationPlayer *p_animation_player);

	errortype err;
	void set_start_index(int32_t si);
	int32_t get_start_index();

	void set_min_leaves(int32_t min_l);
	int32_t get_min_leaves();

	void add_coordinates(Vector<float> point);
	void load_coordinates(Vector<float> points);
	Vector<float> get_coordinates();
	void clear_coordinates();

	void set_dim_len(int32_t dim_len);
	int32_t get_dim_len();

	void calc_root_threshold();

	Vector<float> KNNSearch(Vector<float> point, int32_t k);

	void build_tree();
	KDNode *get_root();
	void clear_root();

	Vector<frame_model *> *get_keys_data() { return keys; }

	void set_keys_data(Vector<frame_model *> *kys) { keys = kys; }
	void clear_keys() {
		while (keys->size() != 0) {
			keys->remove(0);
		}
		c_time = 0;
		timeout = false;
	}

	Vector<float> Predict_traj(Vector3 L_Velocity, int samples);

	int get_traj_samples() { return get_parameter(samples); }
	void set_traj_samples(int sa) { set_parameter(samples, sa); }

	Vector3 get_velocity() { return velocity; }

	void set_velocity(Vector3 v) { velocity = v; }

	// for trajectory drawing

	int get_key_size() { return keys->size(); }
	Vector<float> get_key_traj(int k_n) { return (*keys)[k_n]->traj; }
	Vector<float> get_future_traj() { return future_traj; }

	void print_array(Vector<float> ar);
	AnimationNodeMotionMatch();
	~AnimationNodeMotionMatch();
};

#endif // ANIMATION_NODE_MOTION_MATCH_H
