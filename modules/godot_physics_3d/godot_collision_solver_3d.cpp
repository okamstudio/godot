/**************************************************************************/
/*  godot_collision_solver_3d.cpp                                         */
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

#include "godot_collision_solver_3d.h"

#include "godot_collision_solver_3d_sat.h"
#include "godot_soft_body_3d.h"

#include "gjk_epa.h"

#define collision_solver sat_calculate_penetration
//#define collision_solver gjk_epa_calculate_penetration

bool GodotCollisionSolver3D::solve_static_world_boundary(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin) {
	const GodotWorldBoundaryShape3D *world_boundary = static_cast<const GodotWorldBoundaryShape3D *>(p_shape_A);
	if (p_shape_B->get_type() == PhysicsServer3D::SHAPE_WORLD_BOUNDARY) {
		return false;
	}
	Plane p = p_transform_A.xform(world_boundary->get_plane());

	static const int max_supports = 16;
	Vector3 supports[max_supports];
	int support_count;
	GodotShape3D::FeatureType support_type = GodotShape3D::FeatureType::FEATURE_POINT;
	p_shape_B->get_supports(p_transform_B.basis.xform_inv(-p.normal).normalized(), max_supports, supports, support_count, support_type);

	if (support_type == GodotShape3D::FEATURE_CIRCLE) {
		ERR_FAIL_COND_V(support_count != 3, false);

		Vector3 circle_pos = supports[0];
		Vector3 circle_axis_1 = supports[1] - circle_pos;
		Vector3 circle_axis_2 = supports[2] - circle_pos;

		// Use 3 equidistant points on the circle.
		for (int i = 0; i < 3; ++i) {
			Vector3 vertex_pos = circle_pos;
			vertex_pos += circle_axis_1 * Math::cos(2.0 * Math_PI * i / 3.0);
			vertex_pos += circle_axis_2 * Math::sin(2.0 * Math_PI * i / 3.0);
			supports[i] = vertex_pos;
		}
	}

	bool found = false;

	for (int i = 0; i < support_count; i++) {
		supports[i] += p_margin * supports[i].normalized();
		supports[i] = p_transform_B.xform(supports[i]);
		if (p.distance_to(supports[i]) >= 0) {
			continue;
		}
		found = true;

		Vector3 support_A = p.project(supports[i]);

		if (p_result_callback) {
			if (p_swap_result) {
				Vector3 normal = (support_A - supports[i]).normalized();
				p_result_callback(supports[i], 0, support_A, 0, normal, p_userdata);
			} else {
				Vector3 normal = (supports[i] - support_A).normalized();
				p_result_callback(support_A, 0, supports[i], 0, normal, p_userdata);
			}
		}
	}

	return found;
}

bool GodotCollisionSolver3D::solve_separation_ray(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin) {
	const GodotSeparationRayShape3D *ray = static_cast<const GodotSeparationRayShape3D *>(p_shape_A);

	Vector3 from = p_transform_A.origin;
	Vector3 dir = p_transform_A.basis.get_column(2);
	real_t distance = ray->get_length() + p_margin;
	Vector3 support_A = from + dir * distance;

	Transform3D ai = p_transform_B.affine_inverse();

	from = ai.xform(from);
	dir = p_transform_B.basis.transposed().xform(dir);
	real_t dir_length = dir.length();
	dir /= dir_length;
	distance *= dir_length;

	Vector3 p, n;
	int fi = -1;
	if (!p_shape_B->intersect_segment(from, dir, distance, p, n, fi, true)) {
		return false;
	}

	// Discard contacts when the ray is fully contained inside the shape.
	if (n == Vector3()) {
		return false;
	}

	// Discard contacts in the wrong direction.
	if (n.dot(dir) * distance < CMP_EPSILON) {
		return false;
	}

	Vector3 support_B = p_transform_B.xform(p);
	if (ray->get_slide_on_slope()) {
		Vector3 global_n = ai.basis.xform_inv(n).normalized();
		support_B = support_A + (support_B - support_A).length() * global_n;
	}

	if (p_result_callback) {
		Vector3 normal = (support_B - support_A).normalized();
		if (p_swap_result) {
			p_result_callback(support_B, 0, support_A, 0, -normal, p_userdata);
		} else {
			p_result_callback(support_A, 0, support_B, 0, normal, p_userdata);
		}
	}
	return true;
}

struct _SoftBodyContactCollisionInfo {
	int node_index = 0;
	GodotCollisionSolver3D::CallbackResult result_callback = nullptr;
	void *userdata = nullptr;
	bool swap_result = false;
	int contact_count = 0;
};

void GodotCollisionSolver3D::soft_body_contact_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata) {
	_SoftBodyContactCollisionInfo &cinfo = *(static_cast<_SoftBodyContactCollisionInfo *>(p_userdata));

	++cinfo.contact_count;

	if (!cinfo.result_callback) {
		return;
	}

	if (cinfo.swap_result) {
		cinfo.result_callback(p_point_B, cinfo.node_index, p_point_A, p_index_A, -normal, cinfo.userdata);
	} else {
		cinfo.result_callback(p_point_A, p_index_A, p_point_B, cinfo.node_index, normal, cinfo.userdata);
	}
}

struct _SoftBodyQueryInfo {
	GodotSoftBody3D *soft_body = nullptr;
	const GodotShape3D *shape_A = nullptr;
	const GodotShape3D *shape_B = nullptr;
	Transform3D transform_A;
	Transform3D node_transform;
	_SoftBodyContactCollisionInfo contact_info;
#ifdef DEBUG_ENABLED
	int node_query_count = 0;
	int convex_query_count = 0;
#endif
};

bool GodotCollisionSolver3D::soft_body_query_callback(uint32_t p_node_index, void *p_userdata) {
	_SoftBodyQueryInfo &query_cinfo = *(static_cast<_SoftBodyQueryInfo *>(p_userdata));

	Vector3 node_position = query_cinfo.soft_body->get_node_position(p_node_index);

	Transform3D transform_B;
	transform_B.origin = query_cinfo.node_transform.xform(node_position);

	query_cinfo.contact_info.node_index = p_node_index;
	bool collided = solve_static(query_cinfo.shape_A, query_cinfo.transform_A, query_cinfo.shape_B, transform_B, soft_body_contact_callback, &query_cinfo.contact_info);

#ifdef DEBUG_ENABLED
	++query_cinfo.node_query_count;
#endif

	// Stop at first collision if contacts are not needed.
	return (collided && !query_cinfo.contact_info.result_callback);
}

bool GodotCollisionSolver3D::soft_body_concave_callback(void *p_userdata, GodotShape3D *p_convex) {
	_SoftBodyQueryInfo &query_cinfo = *(static_cast<_SoftBodyQueryInfo *>(p_userdata));

	query_cinfo.shape_A = p_convex;

	// Calculate AABB for internal soft body query (in world space).
	AABB shape_aabb;
	for (int i = 0; i < 3; i++) {
		Vector3 axis;
		axis[i] = 1.0;

		real_t smin, smax;
		p_convex->project_range(axis, query_cinfo.transform_A, smin, smax);

		shape_aabb.position[i] = smin;
		shape_aabb.size[i] = smax - smin;
	}

	shape_aabb.grow_by(query_cinfo.soft_body->get_collision_margin());

	query_cinfo.soft_body->query_aabb(shape_aabb, soft_body_query_callback, &query_cinfo);

	bool collided = (query_cinfo.contact_info.contact_count > 0);

#ifdef DEBUG_ENABLED
	++query_cinfo.convex_query_count;
#endif

	// Stop at first collision if contacts are not needed.
	return (collided && !query_cinfo.contact_info.result_callback);
}

bool GodotCollisionSolver3D::solve_soft_body(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result) {
	const GodotSoftBodyShape3D *soft_body_shape_B = static_cast<const GodotSoftBodyShape3D *>(p_shape_B);

	GodotSoftBody3D *soft_body = soft_body_shape_B->get_soft_body();
	const Transform3D &world_to_local = soft_body->get_inv_transform();

	const real_t collision_margin = soft_body->get_collision_margin();

	GodotSphereShape3D sphere_shape;
	sphere_shape.set_data(collision_margin);

	_SoftBodyQueryInfo query_cinfo;
	query_cinfo.contact_info.result_callback = p_result_callback;
	query_cinfo.contact_info.userdata = p_userdata;
	query_cinfo.contact_info.swap_result = p_swap_result;
	query_cinfo.soft_body = soft_body;
	query_cinfo.node_transform = p_transform_B * world_to_local;
	query_cinfo.shape_A = p_shape_A;
	query_cinfo.transform_A = p_transform_A;
	query_cinfo.shape_B = &sphere_shape;

	if (p_shape_A->is_concave()) {
		// In case of concave shape, query convex shapes first.
		const GodotConcaveShape3D *concave_shape_A = static_cast<const GodotConcaveShape3D *>(p_shape_A);

		AABB soft_body_aabb = soft_body->get_bounds();
		soft_body_aabb.grow_by(collision_margin);

		// Calculate AABB for internal concave shape query (in local space).
		AABB local_aabb;
		for (int i = 0; i < 3; i++) {
			Vector3 axis(p_transform_A.basis.get_column(i));
			real_t axis_scale = 1.0 / axis.length();

			real_t smin = soft_body_aabb.position[i];
			real_t smax = smin + soft_body_aabb.size[i];

			smin *= axis_scale;
			smax *= axis_scale;

			local_aabb.position[i] = smin;
			local_aabb.size[i] = smax - smin;
		}

		concave_shape_A->cull(local_aabb, soft_body_concave_callback, &query_cinfo, true);
	} else {
		AABB shape_aabb = p_transform_A.xform(p_shape_A->get_aabb());
		shape_aabb.grow_by(collision_margin);

		soft_body->query_aabb(shape_aabb, soft_body_query_callback, &query_cinfo);
	}

	return (query_cinfo.contact_info.contact_count > 0);
}

struct _ConcaveCollisionInfo {
	const Transform3D *transform_A = nullptr;
	const GodotShape3D *shape_A = nullptr;
	const Transform3D *transform_B = nullptr;
	GodotCollisionSolver3D::CallbackResult result_callback = nullptr;
	void *userdata = nullptr;
	bool swap_result = false;
	bool collided = false;
	int aabb_tests = 0;
	int collisions = 0;
	bool tested = false;
	real_t margin_A = 0.0f;
	real_t margin_B = 0.0f;
	Vector3 close_A;
	Vector3 close_B;
};

bool GodotCollisionSolver3D::concave_callback(void *p_userdata, GodotShape3D *p_convex) {
	_ConcaveCollisionInfo &cinfo = *(static_cast<_ConcaveCollisionInfo *>(p_userdata));
	cinfo.aabb_tests++;

	bool collided = collision_solver(cinfo.shape_A, *cinfo.transform_A, p_convex, *cinfo.transform_B, cinfo.result_callback, cinfo.userdata, cinfo.swap_result, nullptr, cinfo.margin_A, cinfo.margin_B);
	if (!collided) {
		return false;
	}

	cinfo.collided = true;
	cinfo.collisions++;

	// Stop at first collision if contacts are not needed.
	return !cinfo.result_callback;
}

bool GodotCollisionSolver3D::solve_concave(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin_A, real_t p_margin_B) {
	const GodotConcaveShape3D *concave_B = static_cast<const GodotConcaveShape3D *>(p_shape_B);

	_ConcaveCollisionInfo cinfo;
	cinfo.transform_A = &p_transform_A;
	cinfo.shape_A = p_shape_A;
	cinfo.transform_B = &p_transform_B;
	cinfo.result_callback = p_result_callback;
	cinfo.userdata = p_userdata;
	cinfo.swap_result = p_swap_result;
	cinfo.collided = false;
	cinfo.collisions = 0;
	cinfo.margin_A = p_margin_A;
	cinfo.margin_B = p_margin_B;

	cinfo.aabb_tests = 0;

	Transform3D rel_transform = p_transform_A;
	rel_transform.origin -= p_transform_B.origin;

	//quickly compute a local AABB

	AABB local_aabb;
	for (int i = 0; i < 3; i++) {
		Vector3 axis(p_transform_B.basis.get_column(i));
		real_t axis_scale = 1.0 / axis.length();
		axis *= axis_scale;

		real_t smin = 0.0, smax = 0.0;
		p_shape_A->project_range(axis, rel_transform, smin, smax);
		smin -= p_margin_A;
		smax += p_margin_A;
		smin *= axis_scale;
		smax *= axis_scale;

		local_aabb.position[i] = smin;
		local_aabb.size[i] = smax - smin;
	}

	concave_B->cull(local_aabb, concave_callback, &cinfo, false);

	return cinfo.collided;
}

bool GodotCollisionSolver3D::solve_static(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, Vector3 *r_sep_axis, real_t p_margin_A, real_t p_margin_B) {
	PhysicsServer3D::ShapeType type_A = p_shape_A->get_type();
	PhysicsServer3D::ShapeType type_B = p_shape_B->get_type();
	bool concave_A = p_shape_A->is_concave();
	bool concave_B = p_shape_B->is_concave();

	bool swap = false;

	if (type_A > type_B) {
		SWAP(type_A, type_B);
		SWAP(concave_A, concave_B);
		swap = true;
	}

	if (type_A == PhysicsServer3D::SHAPE_WORLD_BOUNDARY) {
		if (type_B == PhysicsServer3D::SHAPE_WORLD_BOUNDARY) {
			WARN_PRINT_ONCE("Collisions between world boundaries are not supported.");
			return false;
		}
		if (type_B == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
			WARN_PRINT_ONCE("Collisions between world boundaries and rays are not supported.");
			return false;
		}
		if (type_B == PhysicsServer3D::SHAPE_SOFT_BODY) {
			WARN_PRINT_ONCE("Collisions between world boundaries and soft bodies are not supported.");
			return false;
		}

		if (swap) {
			return solve_static_world_boundary(p_shape_B, p_transform_B, p_shape_A, p_transform_A, p_result_callback, p_userdata, true, p_margin_A);
		} else {
			return solve_static_world_boundary(p_shape_A, p_transform_A, p_shape_B, p_transform_B, p_result_callback, p_userdata, false, p_margin_B);
		}

	} else if (type_A == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
		if (type_B == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
			WARN_PRINT_ONCE("Collisions between rays are not supported.");
			return false;
		}

		if (swap) {
			return solve_separation_ray(p_shape_B, p_transform_B, p_shape_A, p_transform_A, p_result_callback, p_userdata, true, p_margin_B);
		} else {
			return solve_separation_ray(p_shape_A, p_transform_A, p_shape_B, p_transform_B, p_result_callback, p_userdata, false, p_margin_A);
		}

	} else if (type_B == PhysicsServer3D::SHAPE_SOFT_BODY) {
		if (type_A == PhysicsServer3D::SHAPE_SOFT_BODY) {
			WARN_PRINT_ONCE("Collisions between soft bodies are not supported.");
			return false;
		}

		if (swap) {
			return solve_soft_body(p_shape_B, p_transform_B, p_shape_A, p_transform_A, p_result_callback, p_userdata, true);
		} else {
			return solve_soft_body(p_shape_A, p_transform_A, p_shape_B, p_transform_B, p_result_callback, p_userdata, false);
		}

	} else if (concave_B) {
		if (concave_A) {
			WARN_PRINT_ONCE("Collisions between two concave shapes are not supported.");
			return false;
		}

		if (!swap) {
			return solve_concave(p_shape_A, p_transform_A, p_shape_B, p_transform_B, p_result_callback, p_userdata, false, p_margin_A, p_margin_B);
		} else {
			return solve_concave(p_shape_B, p_transform_B, p_shape_A, p_transform_A, p_result_callback, p_userdata, true, p_margin_A, p_margin_B);
		}

	} else {
		return collision_solver(p_shape_A, p_transform_A, p_shape_B, p_transform_B, p_result_callback, p_userdata, false, r_sep_axis, p_margin_A, p_margin_B);
	}
}

bool GodotCollisionSolver3D::concave_distance_callback(void *p_userdata, GodotShape3D *p_convex) {
	_ConcaveCollisionInfo &cinfo = *(static_cast<_ConcaveCollisionInfo *>(p_userdata));
	cinfo.aabb_tests++;

	Vector3 close_A, close_B;
	cinfo.collided = !gjk_epa_calculate_distance(cinfo.shape_A, *cinfo.transform_A, p_convex, *cinfo.transform_B, close_A, close_B);

	if (cinfo.collided) {
		// No need to process any more result.
		return true;
	}

	if (!cinfo.tested || close_A.distance_squared_to(close_B) < cinfo.close_A.distance_squared_to(cinfo.close_B)) {
		cinfo.close_A = close_A;
		cinfo.close_B = close_B;
		cinfo.tested = true;
	}

	cinfo.collisions++;
	return false;
}

bool GodotCollisionSolver3D::solve_distance_world_boundary(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, Vector3 &r_point_A, Vector3 &r_point_B) {
	const GodotWorldBoundaryShape3D *world_boundary = static_cast<const GodotWorldBoundaryShape3D *>(p_shape_A);
	if (p_shape_B->get_type() == PhysicsServer3D::SHAPE_WORLD_BOUNDARY) {
		return false;
	}
	Plane p = p_transform_A.xform(world_boundary->get_plane());

	static const int max_supports = 16;
	Vector3 supports[max_supports];
	int support_count;
	GodotShape3D::FeatureType support_type;
	Vector3 support_direction = p_transform_B.basis.xform_inv(-p.normal).normalized();

	p_shape_B->get_supports(support_direction, max_supports, supports, support_count, support_type);

	if (support_count == 0) { // This is a poor man's way to detect shapes that don't implement get_supports, such as GodotMotionShape3D.
		Vector3 support_B = p_transform_B.xform(p_shape_B->get_support(support_direction));
		r_point_A = p.project(support_B);
		r_point_B = support_B;
		bool collided = p.distance_to(support_B) <= 0;
		return collided;
	}

	if (support_type == GodotShape3D::FEATURE_CIRCLE) {
		ERR_FAIL_COND_V(support_count != 3, false);

		Vector3 circle_pos = supports[0];
		Vector3 circle_axis_1 = supports[1] - circle_pos;
		Vector3 circle_axis_2 = supports[2] - circle_pos;

		// Use 3 equidistant points on the circle.
		for (int i = 0; i < 3; ++i) {
			Vector3 vertex_pos = circle_pos;
			vertex_pos += circle_axis_1 * Math::cos(2.0 * Math_PI * i / 3.0);
			vertex_pos += circle_axis_2 * Math::sin(2.0 * Math_PI * i / 3.0);
			supports[i] = vertex_pos;
		}
	}

	bool collided = false;
	Vector3 closest;
	real_t closest_d = 0;

	for (int i = 0; i < support_count; i++) {
		supports[i] = p_transform_B.xform(supports[i]);
		real_t d = p.distance_to(supports[i]);
		if (i == 0 || d < closest_d) {
			closest = supports[i];
			closest_d = d;
			if (d <= 0) {
				collided = true;
			}
		}
	}

	r_point_A = p.project(closest);
	r_point_B = closest;

	return collided;
}

bool GodotCollisionSolver3D::solve_distance(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, Vector3 &r_point_A, Vector3 &r_point_B, const AABB &p_concave_hint, Vector3 *r_sep_axis) {
	if (p_shape_B->get_type() == PhysicsServer3D::SHAPE_WORLD_BOUNDARY) {
		Vector3 a, b;
		bool col = solve_distance_world_boundary(p_shape_B, p_transform_B, p_shape_A, p_transform_A, a, b);
		r_point_A = b;
		r_point_B = a;
		return !col;

	} else if (p_shape_B->is_concave()) {
		if (p_shape_A->is_concave()) {
			return false;
		}

		const GodotConcaveShape3D *concave_B = static_cast<const GodotConcaveShape3D *>(p_shape_B);

		_ConcaveCollisionInfo cinfo;
		cinfo.transform_A = &p_transform_A;
		cinfo.shape_A = p_shape_A;
		cinfo.transform_B = &p_transform_B;
		cinfo.result_callback = nullptr;
		cinfo.userdata = nullptr;
		cinfo.swap_result = false;
		cinfo.collided = false;
		cinfo.collisions = 0;
		cinfo.aabb_tests = 0;
		cinfo.tested = false;

		Transform3D rel_transform = p_transform_A;
		rel_transform.origin -= p_transform_B.origin;

		//quickly compute a local AABB

		bool use_cc_hint = p_concave_hint != AABB();
		AABB cc_hint_aabb;
		if (use_cc_hint) {
			cc_hint_aabb = p_concave_hint;
			cc_hint_aabb.position -= p_transform_B.origin;
		}

		AABB local_aabb;
		for (int i = 0; i < 3; i++) {
			Vector3 axis(p_transform_B.basis.get_column(i));
			real_t axis_scale = ((real_t)1.0) / axis.length();
			axis *= axis_scale;

			real_t smin, smax;

			if (use_cc_hint) {
				cc_hint_aabb.project_range_in_plane(Plane(axis), smin, smax);
			} else {
				p_shape_A->project_range(axis, rel_transform, smin, smax);
			}

			smin *= axis_scale;
			smax *= axis_scale;

			local_aabb.position[i] = smin;
			local_aabb.size[i] = smax - smin;
		}

		concave_B->cull(local_aabb, concave_distance_callback, &cinfo, false);
		if (!cinfo.collided) {
			r_point_A = cinfo.close_A;
			r_point_B = cinfo.close_B;
		}

		return !cinfo.collided;
	} else {
		return gjk_epa_calculate_distance(p_shape_A, p_transform_A, p_shape_B, p_transform_B, r_point_A, r_point_B); //should pass sepaxis..
	}
}
