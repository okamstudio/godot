#include "animation_node_motion_match.h"
#include "scene/main/node.h"

void AnimationNodeMotionMatch::get_parameter_list(
		List<PropertyInfo> *r_list) const {
	r_list->push_back(PropertyInfo(Variant::INT, min, PROPERTY_HINT_NONE, ""));
	r_list->push_back(PropertyInfo(Variant::INT, samples, PROPERTY_HINT_RANGE,
			"5,40,1,or_greater"));
	r_list->push_back(PropertyInfo(Variant::FLOAT, pvst, PROPERTY_HINT_RANGE,
			"0,1,0.01,or_greater"));
	r_list->push_back(PropertyInfo(Variant::FLOAT, f_time, PROPERTY_HINT_RANGE,
			"0,2,0.01,or_greater"));
}

Variant AnimationNodeMotionMatch::get_parameter_default_value(
		const StringName &p_parameter) const {
	if (p_parameter == min) {
		return 0;
	} else if (p_parameter == samples) {
		return 10;
	} else if (p_parameter == pvst) {
		return 0.5;
	} else if (p_parameter == f_time) {
		return 0.25;
	} else {
		return Variant();
	}
}

void AnimationNodeMotionMatch::add_matching_track(
		const NodePath &p_track_path) {
	matching_tracks.push_back(p_track_path);
} // Adds tracks to matching_tracks

void AnimationNodeMotionMatch::remove_matching_track(
		const NodePath &p_track_path) {
	matching_tracks.erase(p_track_path);
} // removes tracks

bool AnimationNodeMotionMatch::is_matching_track(
		const NodePath &p_track_path) const {
	return matching_tracks.find(p_track_path) != -1;
}

void AnimationNodeMotionMatch::update_motion_database(
		AnimationPlayer *p_animation_player) {
	for (int i = 0; i < matching_tracks.size(); i++) {
		print_line("track " + itos(i) + ": " + String(matching_tracks[i]));
	}
}

Vector<NodePath> AnimationNodeMotionMatch::get_matching_tracks() {
	return matching_tracks;
}

void AnimationNodeMotionMatch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_matching_track", "track"),
			&AnimationNodeMotionMatch::add_matching_track);
	ClassDB::bind_method(D_METHOD("remove_matching_track", "track"),
			&AnimationNodeMotionMatch::remove_matching_track);
	ClassDB::bind_method(D_METHOD("is_matching_track", "track"),
			&AnimationNodeMotionMatch::is_matching_track);

	ClassDB::bind_method(D_METHOD("add_coordinates", "point"),
			&AnimationNodeMotionMatch::add_coordinates);
	ClassDB::bind_method(D_METHOD("load_coordinates", "points"),
			&AnimationNodeMotionMatch::load_coordinates);
	ClassDB::bind_method(D_METHOD("clear_coordinates"),
			&AnimationNodeMotionMatch::clear_coordinates);
	ClassDB::bind_method(D_METHOD("get_coordinates"),
			&AnimationNodeMotionMatch::get_coordinates);

	ClassDB::bind_method(D_METHOD("set_dim_len", "dim_len"),
			&AnimationNodeMotionMatch::set_dim_len);
	ClassDB::bind_method(D_METHOD("get_dim_len"),
			&AnimationNodeMotionMatch::get_dim_len);

	ClassDB::bind_method(D_METHOD("set_min_leaves", "min_leaves"),
			&AnimationNodeMotionMatch::set_min_leaves);
	ClassDB::bind_method(D_METHOD("get_min_leaves"),
			&AnimationNodeMotionMatch::get_min_leaves);

	ClassDB::bind_method(D_METHOD("get_root"),
			&AnimationNodeMotionMatch::get_root);
	ClassDB::bind_method(D_METHOD("build_tree"),
			&AnimationNodeMotionMatch::build_tree);

	ClassDB::bind_method(D_METHOD("set_start_index", "si"),
			&AnimationNodeMotionMatch::set_start_index);
	ClassDB::bind_method(D_METHOD("get_start_index"),
			&AnimationNodeMotionMatch::get_start_index);

	ClassDB::bind_method(D_METHOD("calc_root_threshold"),
			&AnimationNodeMotionMatch::calc_root_threshold);
	ClassDB::bind_method(D_METHOD("KNNSearch", "point", "k"),
			&AnimationNodeMotionMatch::KNNSearch);

	ClassDB::bind_method(D_METHOD("calculate_threshold", "point_coordinates"),
			&AnimationNodeMotionMatch::KDNode::calculate_threshold);

	ClassDB::bind_method(D_METHOD("add_index", "ind"),
			&AnimationNodeMotionMatch::KDNode::add_index);
	ClassDB::bind_method(D_METHOD("clear_indices"),
			&AnimationNodeMotionMatch::KDNode::clear_indices);
	ClassDB::bind_method(D_METHOD("get_indices"),
			&AnimationNodeMotionMatch::KDNode::get_indices);

	ClassDB::bind_method(D_METHOD("leaf_split", "point_coordinates_data",
								 "no_of_dims", "start_dim"),
			&AnimationNodeMotionMatch::KDNode::leaf_split);

	ClassDB::bind_method(D_METHOD("get_left"),
			&AnimationNodeMotionMatch::KDNode::get_left);
	ClassDB::bind_method(D_METHOD("get_right"),
			&AnimationNodeMotionMatch::KDNode::get_right);

	ClassDB::bind_method(D_METHOD("get_prev"),
			&AnimationNodeMotionMatch::KDNode::get_prev);

	ClassDB::bind_method(D_METHOD("set_th", "th"),
			&AnimationNodeMotionMatch::KDNode::set_th);
	ClassDB::bind_method(D_METHOD("get_th"),
			&AnimationNodeMotionMatch::KDNode::get_th);

	ClassDB::bind_method(D_METHOD("Predict_traj", "vel", "samples"),
			&AnimationNodeMotionMatch::Predict_traj);

	ClassDB::bind_method(D_METHOD("set_velocity", "vel"),
			&AnimationNodeMotionMatch::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"),
			&AnimationNodeMotionMatch::get_velocity);

	// for trajectory drawing

	ClassDB::bind_method(D_METHOD("get_keys_size"),
			&AnimationNodeMotionMatch::get_key_size);

	ClassDB::bind_method(D_METHOD("get_key_traj", "key number"),
			&AnimationNodeMotionMatch::get_key_traj);

	ClassDB::bind_method(D_METHOD("get_future_traj"),
			&AnimationNodeMotionMatch::get_future_traj);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity"), "set_velocity",
			"get_velocity");
}

AnimationNodeMotionMatch::AnimationNodeMotionMatch() {
	root.instance();
	dim_len = 2;
	start_index = 0;
	point_coordinates = {};
	min_leaves = 1;

	this->pos = "position";
	this->min = "min_key";
	this->pvst = "Pose vs Trajectory";
	this->f_time = "Check rate";
	this->samples = "Future Traj Samples";
}

AnimationNodeMotionMatch::~AnimationNodeMotionMatch() {}

void AnimationNodeMotionMatch::add_coordinates(Vector<float> point) {
	if (point.size() != dim_len) {
		print_line("ERROR: Point is of wrong size.");
		error = true;
		err = LOAD_POINT_ERROR;
	} else {
		for (int i = 0; i < point.size(); i++) {
			this->point_coordinates.append(point[i]);
		}
		root->add_index(root->get_indices().size());
	}
} // adds point to KD-tree

void AnimationNodeMotionMatch::load_coordinates(Vector<float> points) {
	if (dim_len < 0) {
		print_line("ERROR: Length is negative");
		error = true;
		err = LOAD_POINT_ERROR;
	} else if (points.size() % dim_len != 0) {
		print_line("ERROR: Point is of wrong size.");
		error = true;
		err = LOAD_POINT_ERROR;
	} else {
		for (int i = 0; i < points.size() / dim_len; i++) {
			Vector<float> point;
			for (int j = 0; j < dim_len; j++) {
				point.append(points[i * dim_len + j]);
			}
			add_coordinates(point);
		}
	}
} // load multiple points at a go to KD-Tree

Vector<float> AnimationNodeMotionMatch::get_coordinates() {
	return this->point_coordinates;
}

void AnimationNodeMotionMatch::clear_coordinates() {
	this->point_coordinates.resize(0);
}

void AnimationNodeMotionMatch::set_dim_len(int32_t dim_len) {
	this->dim_len = dim_len;
} // set dimension length

int32_t AnimationNodeMotionMatch::get_dim_len() {
	return this->dim_len;
}

AnimationNodeMotionMatch::KDNode *AnimationNodeMotionMatch::get_root() {
	return root.ptr();
} // returns root of the KDTree

void AnimationNodeMotionMatch::clear_root() {
	root->point_indices = {};
	root->split_th = 0;
	root->split_axis = 0;
	root->left = Ref<KDNode>();
	root->right = Ref<KDNode>();
} // resets KDtree

void AnimationNodeMotionMatch::set_start_index(int32_t si) {
	this->start_index = si;
} // set from which point you want to start evaluation

int32_t AnimationNodeMotionMatch::get_start_index() {
	return this->start_index;
}

void AnimationNodeMotionMatch::calc_root_threshold() {
	if (this->point_coordinates.size() == 0) {
		print_line("ERROR:Load Points first!");
	} else {
		root->calculate_threshold(point_coordinates, dim_len);
	}
} // Calculating root threshold TODO : Add options for threshold

void AnimationNodeMotionMatch::set_min_leaves(int32_t min_l) {
	min_leaves = min_l;
} // min_leaves per node

int32_t AnimationNodeMotionMatch::get_min_leaves() {
	return min_leaves;
}

void AnimationNodeMotionMatch::build_tree() {
	if (error == true) {
		if (err == LOAD_POINT_ERROR)
			print_line("ERROR: Check the input points");
		else if (err == QUERY_POINT_ERROR)
			print_line("ERROR: Check your query point");
		else if (err == K_ERROR)
			print_line("ERROR: Invalid value for number of neighbors");

	} else {
		print_line("PROCESS : Building KD-Tree..");
		root->leaf_split(point_coordinates, dim_len, start_index, min_leaves);
		print_line("KD-Tree built SUCCESSFULLY");
	}
} // Builds the tree with all the given parameters

float dist_between(Vector<float> point_coordinates, Vector<float> p1,
		uint32_t index) {
	float n = 0;
	for (int i = 0; i < p1.size(); i++) {
		n += (p1[i] - point_coordinates[p1.size() * index + i]) *
			 (p1[i] - point_coordinates[p1.size() * index + i]);
	}
	return sqrt(n);
}

bool in_array(Vector<float> points, uint32_t query) {
	for (int i = 0; i < points.size(); i++) {
		if (points[i] == query)
			return true;
	}
	return false;
}

Vector<float> AnimationNodeMotionMatch::KNNSearch(Vector<float> point,
		int32_t k) {
	if (error == false && point.size() % dim_len != 0) {
		error = true;
		err = QUERY_POINT_ERROR;
	} else if (error == false && k > point_coordinates.size() / dim_len) {
		error = true;
		err = K_ERROR;
	}

	if (error == true) {
		if (err == LOAD_POINT_ERROR)
			print_line("ERROR: Check the input points");
		else if (err == QUERY_POINT_ERROR)
			print_line("ERROR: Check your query point");
		else if (err == K_ERROR)
			print_line("ERROR: Invalid value for number of neighbors");

		return {};

	} else {
		Vector<float> Knn = {};
		KDNode *m_node; /*node where the query point can be placed*/
		KDNode *node = root.ptr();

		while (node->get_indices().size() > min_leaves) {
			if (point[node->get_split_axis()] > node->get_th()) {
				node = node->get_left();
			} else {
				node = node->get_right();
			}
		}
		m_node = node;
		for (int j = 0; j < k; j++) {
			uint32_t nn = 0; /*nearest neighbour*/
			float nd = std::numeric_limits<float>::max(); /*nearest distance*/
			for (int i = 0; i < node->get_indices().size(); i++) {
				if (j == 0) {
					float dist =
							dist_between(point_coordinates, point, node->get_indices()[i]);
					if (nd > dist || nd == 0) {
						nd = dist;
						nn = node->get_indices()[i];
					}
				} else if (!in_array(Knn, node->get_indices()[i])) {
					float dist =
							dist_between(point_coordinates, point, node->get_indices()[i]);
					if (nd > dist || nd == 0) {
						nd = dist;
						nn = node->get_indices()[i];
					}
				}
			}
			while (node->get_prev() != NULL) {
				node = node->get_prev();
				if ((point[node->get_split_axis()] - node->get_th()) > nd) {
					break;
				} else {
					for (int i = 0; i < node->get_indices().size(); i++) {
						if (j == 0) {
							float dist = dist_between(point_coordinates, point,
									node->get_indices()[i]);
							if (nd > dist || nd == 0) {
								nd = dist;
								nn = node->get_indices()[i];
							}
						} else if (!in_array(Knn, node->get_indices()[i])) {
							float dist = dist_between(point_coordinates, point,
									node->get_indices()[i]);
							if (nd > dist || nd == 0) {
								nd = dist;
								nn = node->get_indices()[i];
							}
						}
					}
				}
			}

			Knn.append(nn);
			node = m_node;
			print_line("Round" + itos(j) + ":" + itos(nn));
		}
		return Knn;
	}
} /*returns K nearest neighbours in a Vector<float>*/

void AnimationNodeMotionMatch::KDNode::calculate_threshold(
		Vector<float> point_coordinates, int32_t dim_len) {
	if (point_coordinates.size() % dim_len != 0) {
		print_line("ERROR: Point coordinates array is of wrong size.");
	} else {
		this->split_th = 0;
		for (int i = 0; i < this->point_indices.size(); i++) {
			this->split_th +=
					point_coordinates[(point_indices[i] * dim_len) + split_axis];
		}
		this->split_th = this->split_th / this->point_indices.size();
	}
} // threshold calculating function

bool AnimationNodeMotionMatch::KDNode::are_all_points_same(
		Vector<float> point_coordinates, int32_t dim_len) {
	for (int i = 0; i < dim_len; i++) {
		for (int j = 0; j < point_indices.size(); j++) {
			float p = point_coordinates[(point_indices[j] * dim_len) + i];
			for (int k = 0; k < point_indices.size(); k++) {
				if (point_coordinates[(point_indices[k] * dim_len) + i] != p) {
					return false;
				}
			}
		}
	}

	return true;
}

void AnimationNodeMotionMatch::KDNode::set_th(float t) {
	this->split_th = t;
}

float AnimationNodeMotionMatch::KDNode::get_th() {
	return this->split_th;
}

void AnimationNodeMotionMatch::KDNode::add_index(uint32_t i) {
	point_indices.append(i);
}

void AnimationNodeMotionMatch::KDNode::clear_indices() {
	point_indices.resize(0);
}

Vector<float> AnimationNodeMotionMatch::KDNode::get_indices() {
	return point_indices;
}

AnimationNodeMotionMatch::KDNode::KDNode() {
	this->point_indices = {};
	this->split_th = 0;
	this->split_axis = 0;
}

void AnimationNodeMotionMatch::KDNode::leaf_split(
		Vector<float> point_coordinates, int32_t dim_len, int32_t dim,
		int32_t min_leaves) {
	if (point_coordinates.size() % dim_len != 0) {
		print_line("ERROR: Point coordinates array is of wrong size.");
	} else {
		split_axis = dim % dim_len;
		calculate_threshold(point_coordinates, dim_len);

		left.instance();
		right.instance();

		left->prev = this;
		right->prev = this;
		for (int j = 0; j < this->point_indices.size(); j++) {
			if (point_coordinates[(point_indices[j] * dim_len) + split_axis] >
					split_th) {
				left->point_indices.append(point_indices[j]);
			} else {
				right->point_indices.append(point_indices[j]);
			}
		}

		if (left->point_indices.size() > min_leaves &&
				!are_all_points_same(point_coordinates, dim_len)) {
			left->leaf_split(point_coordinates, dim_len, dim + 1, min_leaves);
		}
		if (right->point_indices.size() > min_leaves &&
				!are_all_points_same(point_coordinates, dim_len)) {
			right->leaf_split(point_coordinates, dim_len, dim + 1, min_leaves);
		}
	}
} // update tree with given point

AnimationNodeMotionMatch::KDNode *AnimationNodeMotionMatch::KDNode::get_left() {
	return left.ptr();
}

AnimationNodeMotionMatch::KDNode *
AnimationNodeMotionMatch::KDNode::get_right() {
	return right.ptr();
}

float AnimationNodeMotionMatch::process(float p_time, bool p_seek) {
	AnimationPlayer *player = state->player;

	List<StringName> a_nam;
	player->get_animation_list(&a_nam);
	// Tracker->Dummy track for modifications
	if (!player->has_animation("Tracker")) {
		main = a_nam[0];
		Animation *a = player->get_animation(a_nam[0]).ptr();

		r_index = player->get_animation(a_nam[0]).ptr()->find_track(
				state->tree->get_root_motion_track());
		a->track_set_enabled(r_index, false);

		player->add_animation("Tracker", a);
		a_nam.clear();
		player->get_animation_list(&a_nam);
	}

	if (!timeout && keys->size() != 0 && !editing) {
		Vector3 l_v = Vector3();
		l_v = get("velocity");
		future_traj = Predict_traj(l_v, get_parameter(samples));
		float min_cost = std::numeric_limits<float>::max();
		float min_cost_time = 0;
		int dup;

		if (first_time) {
			dup = -1;
			first_time = false;
			player->play("Tracker");
		} else {
			dup = get_parameter(min);
		}

		int p = 0;
		print_line(itos(get_instance_id()));
		for (p = 0; p < keys->size(); p++) {
			if (p != dup) {
				float pos_cost = 0.0f;
				float traj_cost = 0.0f;
				float tot_cost = 0.0f;

				for (int i = 0; i < matching_tracks.size(); i++) {
					Vector<String> s = String(matching_tracks[i]).split(":");
					Vector3 pos =
							skeleton->get_bone_global_pose(skeleton->find_bone(s[1]))
									.get_origin();

					for (int po = 0; po < 2; po++) {
						pos_cost += (pos[po] - (*(*keys)[p]->bone_data)[i][po]) *
									(pos[po] - (*(*keys)[p]->bone_data)[i][po]);
					}
				} // calculating pose costs

				for (int t = 0; t < int(get_parameter(samples)) * 2; t++) {
					traj_cost += ((*keys)[p]->traj[t] - future_traj[t]) *
								 ((*keys)[p]->traj[t] - future_traj[t]);
				} // calculating traj costs

				real_t lbd = get_parameter(pvst); // Pose vs Trajectory cost
				tot_cost = lbd * pos_cost + (1 - lbd) * traj_cost;

				if (tot_cost < min_cost) {
					min_cost = tot_cost;
					min_cost_time = (*keys)[p]->time;
					set_parameter(min, p);
				} // set min
			}
		}
		player->seek(min_cost_time); // play min for every frame
		timeout = true;
	}
	c_time += p_time;
	// f_time parameter decides the check rate
	if (c_time > real_t(get_parameter(f_time))) {
		timeout = false;
		c_time = 0;
	}
	return 0.0;
}

Vector<float> AnimationNodeMotionMatch::Predict_traj(Vector3 L_Velocity,
		int samples) {
	// used exponential decay here
	// TODO : Add multiple options to pick for the user
	// Can use interpolation functions
	Vector<float> futurepath = {};
	Vector3 c_pos = Vector3();

	float time = 0;

	for (int i = 0; i < samples; i++) {
		c_pos[0] = c_pos[0] + L_Velocity[0] * (1 - Math::exp(-time));
		futurepath.append(c_pos[0]);
		c_pos[2] = c_pos[2] + L_Velocity[2] * (1 - Math::exp(-time));
		futurepath.append(c_pos[2]);

		time += delta_time;
	}
	return futurepath;
}

void AnimationNodeMotionMatch::print_array(Vector<float> ar) {
	String s = "";
	for (int k = 0; k < ar.size(); k++) {
		s += itos(ar[k]) + ",";
	}
	print_line(s);
}
