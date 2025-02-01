/**************************************************************************/
/*  godot_soft_body_3d.h                                                  */
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

#pragma once

#include "godot_area_3d.h"
#include "godot_collision_object_3d.h"

#include "core/math/aabb.h"
#include "core/math/dynamic_bvh.h"
#include "core/math/vector3.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/templates/vset.h"

class GodotConstraint3D;

class GodotSoftBody3D : public GodotCollisionObject3D {
	RID soft_mesh;

	struct Node {
		Vector3 s; // Source position
		Vector3 x; // Position
		Vector3 q; // Previous step position/Test position
		Vector3 f; // Force accumulator
		Vector3 v; // Velocity
		Vector3 bv; // Biased Velocity
		Vector3 n; // Normal
		real_t area = 0.0; // Area
		real_t im = 0.0; // 1/mass
		DynamicBVH::ID leaf; // Leaf data
		uint32_t index = 0;
	};

	struct Link {
		Vector3 c3; // gradient
		Node *n[2] = { nullptr, nullptr }; // Node pointers
		real_t rl = 0.0; // Rest length
		real_t c0 = 0.0; // (ima+imb)*kLST
		real_t c1 = 0.0; // rl^2
		real_t c2 = 0.0; // |gradient|^2/c0
	};

	struct Face {
		Vector3 centroid;
		Node *n[3] = { nullptr, nullptr, nullptr }; // Node pointers
		Vector3 normal; // Normal
		real_t ra = 0.0; // Rest area
		DynamicBVH::ID leaf; // Leaf data
		uint32_t index = 0;
	};

	LocalVector<Node> nodes;
	LocalVector<Link> links;
	LocalVector<Face> faces;

	DynamicBVH node_tree;
	DynamicBVH face_tree;

	LocalVector<uint32_t> map_visual_to_physics;

	AABB bounds;

	real_t collision_margin = 0.05;

	real_t total_mass = 1.0;
	real_t inv_total_mass = 1.0;

	int iteration_count = 5;
	real_t linear_stiffness = 0.5; // [0,1]
	real_t pressure_coefficient = 0.0; // [-inf,+inf]
	real_t damping_coefficient = 0.01; // [0,1]
	real_t drag_coefficient = 0.0; // [0,1]
	LocalVector<int> pinned_vertices;

	SelfList<GodotSoftBody3D> active_list;

	HashSet<GodotConstraint3D *> constraints;

	Vector<AreaCMP> areas;

	VSet<RID> exceptions;

	uint64_t island_step = 0;

	_FORCE_INLINE_ Vector3 _compute_area_windforce(const GodotArea3D *p_area, const Face *p_face);

public:
	GodotSoftBody3D();

	const AABB &get_bounds() const { return bounds; }

	void set_state(PhysicsServer3D::BodyState p_state, const Variant &p_variant);
	Variant get_state(PhysicsServer3D::BodyState p_state) const;

	_FORCE_INLINE_ void add_constraint(GodotConstraint3D *p_constraint) { constraints.insert(p_constraint); }
	_FORCE_INLINE_ void remove_constraint(GodotConstraint3D *p_constraint) { constraints.erase(p_constraint); }
	_FORCE_INLINE_ const HashSet<GodotConstraint3D *> &get_constraints() const { return constraints; }
	_FORCE_INLINE_ void clear_constraints() { constraints.clear(); }

	_FORCE_INLINE_ void add_exception(const RID &p_exception) { exceptions.insert(p_exception); }
	_FORCE_INLINE_ void remove_exception(const RID &p_exception) { exceptions.erase(p_exception); }
	_FORCE_INLINE_ bool has_exception(const RID &p_exception) const { return exceptions.has(p_exception); }
	_FORCE_INLINE_ const VSet<RID> &get_exceptions() const { return exceptions; }

	_FORCE_INLINE_ uint64_t get_island_step() const { return island_step; }
	_FORCE_INLINE_ void set_island_step(uint64_t p_step) { island_step = p_step; }

	_FORCE_INLINE_ void add_area(GodotArea3D *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas.write[index].refCount += 1;
		} else {
			areas.ordered_insert(AreaCMP(p_area));
		}
	}

	_FORCE_INLINE_ void remove_area(GodotArea3D *p_area) {
		int index = areas.find(AreaCMP(p_area));
		if (index > -1) {
			areas.write[index].refCount -= 1;
			if (areas[index].refCount < 1) {
				areas.remove_at(index);
			}
		}
	}

	virtual void set_space(GodotSpace3D *p_space) override;

	void set_mesh(RID p_mesh);

	void update_rendering_server(PhysicsServer3DRenderingServerHandler *p_rendering_server_handler);

	Vector3 get_vertex_position(int p_index) const;
	void set_vertex_position(int p_index, const Vector3 &p_position);

	void pin_vertex(int p_index);
	void unpin_vertex(int p_index);
	void unpin_all_vertices();
	bool is_vertex_pinned(int p_index) const;

	uint32_t get_node_count() const;
	real_t get_node_inv_mass(uint32_t p_node_index) const;
	Vector3 get_node_position(uint32_t p_node_index) const;
	Vector3 get_node_velocity(uint32_t p_node_index) const;
	Vector3 get_node_biased_velocity(uint32_t p_node_index) const;
	void apply_node_impulse(uint32_t p_node_index, const Vector3 &p_impulse);
	void apply_node_bias_impulse(uint32_t p_node_index, const Vector3 &p_impulse);

	uint32_t get_face_count() const;
	void get_face_points(uint32_t p_face_index, Vector3 &r_point_1, Vector3 &r_point_2, Vector3 &r_point_3) const;
	Vector3 get_face_normal(uint32_t p_face_index) const;

	void set_iteration_count(int p_val);
	_FORCE_INLINE_ real_t get_iteration_count() const { return iteration_count; }

	void set_total_mass(real_t p_val);
	_FORCE_INLINE_ real_t get_total_mass() const { return total_mass; }
	_FORCE_INLINE_ real_t get_total_inv_mass() const { return inv_total_mass; }

	void set_collision_margin(real_t p_val);
	_FORCE_INLINE_ real_t get_collision_margin() const { return collision_margin; }

	void set_linear_stiffness(real_t p_val);
	_FORCE_INLINE_ real_t get_linear_stiffness() const { return linear_stiffness; }

	void set_pressure_coefficient(real_t p_val);
	_FORCE_INLINE_ real_t get_pressure_coefficient() const { return pressure_coefficient; }

	void set_damping_coefficient(real_t p_val);
	_FORCE_INLINE_ real_t get_damping_coefficient() const { return damping_coefficient; }

	void set_drag_coefficient(real_t p_val);
	_FORCE_INLINE_ real_t get_drag_coefficient() const { return drag_coefficient; }

	void predict_motion(real_t p_delta);
	void solve_constraints(real_t p_delta);

	_FORCE_INLINE_ uint32_t get_node_index(void *p_node) const { return static_cast<Node *>(p_node)->index; }
	_FORCE_INLINE_ uint32_t get_face_index(void *p_face) const { return static_cast<Face *>(p_face)->index; }

	// Return true to stop the query.
	// p_index is the node index for AABB query, face index for Ray query.
	typedef bool (*QueryResultCallback)(uint32_t p_index, void *p_userdata);

	void query_aabb(const AABB &p_aabb, QueryResultCallback p_result_callback, void *p_userdata);
	void query_ray(const Vector3 &p_from, const Vector3 &p_to, QueryResultCallback p_result_callback, void *p_userdata);

protected:
	virtual void _shapes_changed() override;

private:
	void update_normals_and_centroids();
	void update_bounds();
	void update_constants();
	void update_area();
	void reset_link_rest_lengths();
	void update_link_constants();

	void apply_nodes_transform(const Transform3D &p_transform);

	void add_velocity(const Vector3 &p_velocity);

	void apply_forces(const LocalVector<GodotArea3D *> &p_wind_areas);

	bool create_from_trimesh(const Vector<int> &p_indices, const Vector<Vector3> &p_vertices);
	void generate_bending_constraints(int p_distance);
	void reoptimize_link_order();
	void append_link(uint32_t p_node1, uint32_t p_node2);
	void append_face(uint32_t p_node1, uint32_t p_node2, uint32_t p_node3);

	void solve_links(real_t kst, real_t ti);

	void initialize_face_tree();
	void update_face_tree(real_t p_delta);

	void initialize_shape(bool p_force_move = true);
	void deinitialize_shape();

	void destroy();
};

class GodotSoftBodyShape3D : public GodotShape3D {
	GodotSoftBody3D *soft_body = nullptr;

public:
	GodotSoftBody3D *get_soft_body() const { return soft_body; }

	virtual PhysicsServer3D::ShapeType get_type() const override { return PhysicsServer3D::SHAPE_SOFT_BODY; }
	virtual void project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const override { r_min = r_max = 0.0; }
	virtual Vector3 get_support(const Vector3 &p_normal) const override { return Vector3(); }
	virtual void get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const override { r_amount = 0; }

	virtual bool intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const override;
	virtual bool intersect_point(const Vector3 &p_point) const override;
	virtual Vector3 get_closest_point_to(const Vector3 &p_point) const override;
	virtual Vector3 get_moment_of_inertia(real_t p_mass) const override { return Vector3(); }

	virtual void set_data(const Variant &p_data) override {}
	virtual Variant get_data() const override { return Variant(); }

	void update_bounds();

	GodotSoftBodyShape3D(GodotSoftBody3D *p_soft_body);
	~GodotSoftBodyShape3D() {}
};
