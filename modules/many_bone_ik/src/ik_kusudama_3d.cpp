/**************************************************************************/
/*  ik_kusudama_3d.cpp                                                    */
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

#include "ik_kusudama_3d.h"

#include "core/math/quaternion.h"
#include "ik_open_cone_3d.h"
#include "math/ik_node_3d.h"

void IKKusudama3D::_update_constraint(Ref<IKNode3D> p_limiting_axes) {
	// Avoiding antipodal singularities by reorienting the axes.
	Vector<Vector3> directions;

	if (open_cones.size() == 1 && open_cones[0] != nullptr) {
		directions.push_back(open_cones[0]->get_control_point());
	} else {
		for (int i = 0; i < open_cones.size() - 1; i++) {
			if (open_cones[i] == nullptr || open_cones[i + 1] == nullptr) {
				continue;
			}

			Vector3 this_control_point = open_cones[i]->get_control_point();
			Vector3 next_control_point = open_cones[i + 1]->get_control_point();

			Quaternion this_to_next = Quaternion(this_control_point, next_control_point);

			Vector3 axis = this_to_next.get_axis();
			double angle = this_to_next.get_angle() / 2.0;

			Vector3 half_angle = this_control_point.rotated(axis, angle);
			half_angle *= this_to_next.get_angle();
			half_angle.normalize();

			directions.push_back(half_angle);
		}
	}

	Vector3 new_y;
	for (Vector3 direction_vector : directions) {
		new_y += direction_vector;
	}

	if (!directions.is_empty()) {
		new_y /= directions.size();
		new_y.normalize();
	}

	Transform3D new_y_ray = Transform3D(Basis(), new_y);
	Quaternion old_y_to_new_y = Quaternion(p_limiting_axes->get_global_transform().get_basis().get_column(Vector3::AXIS_Y).normalized(), p_limiting_axes->get_global_transform().get_basis().xform(new_y_ray.origin).normalized());
	p_limiting_axes->rotate_local_with_global(old_y_to_new_y);

	for (Ref<IKLimitCone3D> open_cone : open_cones) {
		if (open_cone == nullptr) {
			continue;
		}

		Vector3 control_point = open_cone->get_control_point();
		open_cone->set_control_point(control_point.normalized());
	}

	update_tangent_radii();
}

void IKKusudama3D::update_tangent_radii() {
	for (int i = 0; i < open_cones.size(); i++) {
		Ref<IKLimitCone3D> current = open_cones.write[i];
		Ref<IKLimitCone3D> next;
		if (i < open_cones.size() - 1) {
			next = open_cones.write[i + 1];
		}
		Ref<IKLimitCone3D> cone = open_cones[i];
		cone->update_tangent_handles(next);
	}
}

void IKKusudama3D::set_axial_limits(real_t min_angle, real_t in_range) {
	min_axial_angle = min_angle;
	range_angle = in_range;
	Vector3 y_axis = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 z_axis = Vector3(0.0f, 0.0f, 1.0f);
	twist_min_rot = IKKusudama3D::get_quaternion_axis_angle(y_axis, min_axial_angle);
	twist_min_vec = twist_min_rot.xform(z_axis).normalized();
	twist_center_vec = twist_min_rot.xform(twist_min_vec).normalized();
	twist_center_rot = Quaternion(z_axis, twist_center_vec);
	twist_half_range_half_cos = Math::cos(in_range / real_t(4.0)); // For the quadrance angle. We need half the range angle since starting from the center, and half of that since quadrance takes cos(angle/2).
	twist_max_vec = IKKusudama3D::get_quaternion_axis_angle(y_axis, in_range).xform(twist_min_vec).normalized();
	twist_max_rot = Quaternion(z_axis, twist_max_vec);
}

void IKKusudama3D::set_snap_to_twist_limit(Ref<IKNode3D> p_bone_direction, Ref<IKNode3D> p_to_set, Ref<IKNode3D> p_constraint_axes, real_t p_dampening, real_t p_cos_half_dampen) {
	if (!is_axially_constrained()) {
		return;
	}
	Transform3D global_transform_constraint = p_constraint_axes->get_global_transform();
	Transform3D global_transform_to_set = p_to_set->get_global_transform();
	Basis parent_global_inverse = p_to_set->get_parent()->get_global_transform().basis.inverse();
	Basis global_twist_center = global_transform_constraint.basis * twist_center_rot;
	Basis align_rot = (global_twist_center.inverse() * global_transform_to_set.basis).orthonormalized();
	Quaternion twist_rotation, swing_rotation; // Hold the ik transform's decomposed swing and twist away from global_twist_centers's global basis.
	get_swing_twist(align_rot.get_rotation_quaternion(), Vector3(0, 1, 0), swing_rotation, twist_rotation);
	twist_rotation = IKBoneSegment3D::clamp_to_cos_half_angle(twist_rotation, twist_half_range_half_cos);
	Basis recomposition = (global_twist_center * (swing_rotation * twist_rotation)).orthonormalized();
	Basis rotation = parent_global_inverse * recomposition;
	p_to_set->set_transform(Transform3D(rotation, p_to_set->get_transform().origin));
}

void IKKusudama3D::get_swing_twist(
		Quaternion p_rotation,
		Vector3 p_axis,
		Quaternion &r_swing,
		Quaternion &r_twist) {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_MSG(!p_rotation.is_normalized(), "The quaternion must be normalized.");
#endif
	if (Math::is_zero_approx(p_axis.length_squared())) {
		r_swing = Quaternion();
		r_twist = Quaternion();
		return;
	}
	Quaternion rotation = p_rotation;
	if (rotation.w < real_t(0.0)) {
		rotation *= -1;
	}
	Vector3 p = p_axis * (rotation.x * p_axis.x + rotation.y * p_axis.y + rotation.z * p_axis.z);
	r_twist = Quaternion(p.x, p.y, p.z, rotation.w).normalized();
	real_t d = Vector3(r_twist.x, r_twist.y, r_twist.z).dot(p_axis);
	if (d < real_t(0.0)) {
		r_twist *= real_t(-1.0);
	}
	r_swing = (rotation * r_twist.inverse()).normalized();
}

void IKKusudama3D::add_open_cone(
		Ref<IKLimitCone3D> p_cone) {
	ERR_FAIL_COND(p_cone.is_null());
	ERR_FAIL_COND(p_cone->get_attached_to().is_null());
	open_cones.push_back(p_cone);
	update_tangent_radii();
}

void IKKusudama3D::remove_open_cone(Ref<IKLimitCone3D> limitCone) {
	ERR_FAIL_COND(limitCone.is_null());
	open_cones.erase(limitCone);
}

real_t IKKusudama3D::get_min_axial_angle() {
	return min_axial_angle;
}

real_t IKKusudama3D::get_range_angle() {
	return range_angle;
}

bool IKKusudama3D::is_axially_constrained() {
	return axially_constrained;
}

bool IKKusudama3D::is_orientationally_constrained() {
	return orientationally_constrained;
}

void IKKusudama3D::disable_orientational_limits() {
	orientationally_constrained = false;
}

void IKKusudama3D::enable_orientational_limits() {
	orientationally_constrained = true;
}

void IKKusudama3D::toggle_orientational_limits() {
	orientationally_constrained = !orientationally_constrained;
}

void IKKusudama3D::disable_axial_limits() {
	axially_constrained = false;
}

void IKKusudama3D::enable_axial_limits() {
	axially_constrained = true;
}

void IKKusudama3D::toggle_axial_limits() {
	axially_constrained = !axially_constrained;
}

bool IKKusudama3D::is_enabled() {
	return axially_constrained || orientationally_constrained;
}

void IKKusudama3D::disable() {
	axially_constrained = false;
	orientationally_constrained = false;
}

void IKKusudama3D::enable() {
	axially_constrained = true;
	orientationally_constrained = true;
}

TypedArray<IKLimitCone3D> IKKusudama3D::get_open_cones() const {
	TypedArray<IKLimitCone3D> cones;
	for (Ref<IKLimitCone3D> cone : open_cones) {
		cones.append(cone);
	}
	return cones;
}

Vector3 IKKusudama3D::local_point_on_path_sequence(Vector3 p_in_point, Ref<IKNode3D> p_limiting_axes) {
	double closest_point_dot = 0;
	Vector3 point = p_limiting_axes->get_transform().xform(p_in_point);
	point.normalize();
	Vector3 result = point;

	if (open_cones.size() == 1) {
		Ref<IKLimitCone3D> cone = open_cones[0];
		result = cone->get_control_point();
	} else {
		for (int i = 0; i < open_cones.size() - 1; i++) {
			Ref<IKLimitCone3D> next_cone = open_cones[i + 1];
			Ref<IKLimitCone3D> cone = open_cones[i];
			Vector3 closestPathPoint = cone->get_closest_path_point(next_cone, point);
			double closeDot = closestPathPoint.dot(point);
			if (closeDot > closest_point_dot) {
				result = closestPathPoint;
				closest_point_dot = closeDot;
			}
		}
	}

	return result;
}

/**
 * Given a point (in global coordinates), checks to see if a ray can be extended from the Kusudama's
 * origin to that point, such that the ray in the Kusudama's reference frame is within the range_angle allowed by the Kusudama's
 * coneLimits.
 * If such a ray exists, the original point is returned (the point is within the limits).
 * If it cannot exist, the tip of the ray within the kusudama's limits that would require the least rotation
 * to arrive at the input point is returned.
 * @param in_point the point to test.
 * @param in_bounds returns a number from -1 to 1 representing the point's distance from the boundary, 0 means the point is right on
 * the boundary, 1 means the point is within the boundary and on the path furthest from the boundary. any negative number means
 * the point is outside of the boundary, but does not signify anything about how far from the boundary the point is.
 * @return the original point, if it's in limits, or the closest point which is in limits.
 */
Vector3 IKKusudama3D::get_local_point_in_limits(Vector3 in_point, Vector<double> *in_bounds) {
	// Normalize the input point
	Vector3 point = in_point.normalized();
	real_t closest_cos = -2.0;
	in_bounds->write[0] = -1;

	Vector3 closest_collision_point = in_point;

	// Loop through each limit cone
	for (int i = 0; i < open_cones.size(); i++) {
		Ref<IKLimitCone3D> cone = open_cones[i];
		Vector3 collision_point = cone->closest_to_cone(point, in_bounds);

		// If the collision point is NaN, return the original point
		if (Math::is_nan(collision_point.x) || Math::is_nan(collision_point.y) || Math::is_nan(collision_point.z)) {
			in_bounds->write[0] = 1;
			return point;
		}

		// Calculate the cosine of the angle between the collision point and the original point
		real_t this_cos = collision_point.dot(point);

		// If the closest collision point is not set or the cosine is greater than the current closest cosine, update the closest collision point and cosine
		if (closest_collision_point.is_zero_approx() || this_cos > closest_cos) {
			closest_collision_point = collision_point;
			closest_cos = this_cos;
		}
	}

	// If we're out of bounds of all cones, check if we're in the paths between the cones
	if ((*in_bounds)[0] == -1) {
		for (int i = 0; i < open_cones.size() - 1; i++) {
			Ref<IKLimitCone3D> currCone = open_cones[i];
			Ref<IKLimitCone3D> nextCone = open_cones[i + 1];
			Vector3 collision_point = currCone->get_on_great_tangent_triangle(nextCone, point);

			// If the collision point is NaN, skip to the next iteration
			if (Math::is_nan(collision_point.x)) {
				continue;
			}

			real_t this_cos = collision_point.dot(point);

			// If the cosine is approximately 1, return the original point
			if (Math::is_equal_approx(this_cos, real_t(1.0))) {
				in_bounds->write[0] = 1;
				return point;
			}

			// If the cosine is greater than the current closest cosine, update the closest collision point and cosine
			if (this_cos > closest_cos) {
				closest_collision_point = collision_point;
				closest_cos = this_cos;
			}
		}
	}

	// Return the closest boundary point between cones
	return closest_collision_point;
}

void IKKusudama3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_open_cones"), &IKKusudama3D::get_open_cones);
	ClassDB::bind_method(D_METHOD("set_open_cones", "open_cones"), &IKKusudama3D::set_open_cones);
}

void IKKusudama3D::set_open_cones(TypedArray<IKLimitCone3D> p_cones) {
	open_cones.clear();
	open_cones.resize(p_cones.size());
	for (int32_t i = 0; i < p_cones.size(); i++) {
		open_cones.write[i] = p_cones[i];
	}
}

void IKKusudama3D::snap_to_orientation_limit(Ref<IKNode3D> bone_direction, Ref<IKNode3D> to_set, Ref<IKNode3D> limiting_axes, real_t p_dampening, real_t p_cos_half_angle_dampen) {
	if (bone_direction.is_null()) {
		return;
	}
	if (to_set.is_null()) {
		return;
	}
	if (limiting_axes.is_null()) {
		return;
	}
	Vector<double> in_bounds;
	in_bounds.resize(1);
	in_bounds.write[0] = 1.0;
	Vector3 limiting_origin = limiting_axes->get_global_transform().origin;
	Vector3 bone_dir_xform = bone_direction->get_global_transform().xform(Vector3(0.0, 1.0, 0.0));

	bone_ray->set_point_1(limiting_origin);
	bone_ray->set_point_2(bone_dir_xform);

	Vector3 bone_tip = limiting_axes->to_local(bone_ray->get_point_2());
	Vector3 in_limits = get_local_point_in_limits(bone_tip, &in_bounds);

	if (in_bounds[0] < 0) {
		constrained_ray->set_point_1(bone_ray->get_point_1());
		constrained_ray->set_point_2(limiting_axes->to_global(in_limits));

		Quaternion rectified_rot = Quaternion(bone_ray->get_heading(), constrained_ray->get_heading());
		to_set->rotate_local_with_global(rectified_rot);
	}
}

bool IKKusudama3D::is_nan_vector(const Vector3 &vec) {
	return Math::is_nan(vec.x) || Math::is_nan(vec.y) || Math::is_nan(vec.z);
}

void IKKusudama3D::set_resistance(float p_resistance) {
	resistance = p_resistance;
}

float IKKusudama3D::get_resistance() {
	return resistance;
}

Quaternion IKKusudama3D::clamp_to_quadrance_angle(Quaternion p_rotation, double p_cos_half_angle) {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_rotation.is_normalized(), Quaternion(), "The quaternion must be normalized.");
#endif
	Quaternion rotation = p_rotation;
	double newCoeff = 1.0 - (p_cos_half_angle * abs(p_cos_half_angle));
	double currentCoeff = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z;
	if (newCoeff >= currentCoeff) {
		return rotation;
	}
	double over_limit = (currentCoeff - newCoeff) / (1.0 - newCoeff);
	Quaternion clamped_rotation = rotation;
	clamped_rotation.w = rotation.w < 0 ? -p_cos_half_angle : p_cos_half_angle;
	double compositeCoeff = sqrt(newCoeff / currentCoeff);
	clamped_rotation.x *= compositeCoeff;
	clamped_rotation.y *= compositeCoeff;
	clamped_rotation.z *= compositeCoeff;
	if (!rotation.is_finite() || !clamped_rotation.is_finite()) {
		return Quaternion();
	}
	return rotation.slerp(clamped_rotation, over_limit);
}

void IKKusudama3D::clear_open_cones() {
	open_cones.clear();
}

Quaternion IKKusudama3D::get_quaternion_axis_angle(const Vector3 &p_axis, real_t p_angle) {
	real_t d = p_axis.length_squared();
	if (d == 0) {
		return Quaternion();
	} else {
		real_t sin_angle = Math::sin(p_angle * 0.5f);
		real_t cos_angle = Math::cos(p_angle * 0.5f);
		real_t s = sin_angle / d;
		return Quaternion(p_axis.x * s, p_axis.y * s, p_axis.z * s, cos_angle);
	}
}
