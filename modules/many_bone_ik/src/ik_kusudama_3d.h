/**************************************************************************/
/*  ik_kusudama_3d.h                                                      */
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

#ifndef IK_KUSUDAMA_3D_H
#define IK_KUSUDAMA_3D_H

#include "ik_bone_3d.h"
#include "ik_bone_segment_3d.h"
#include "ik_open_cone_3d.h"
#include "ik_ray_3d.h"
#include "math/ik_node_3d.h"

#include "core/io/resource.h"
#include "core/math/quaternion.h"
#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "scene/3d/node_3d.h"

class IKBone3D;
class IKLimitCone3D;
class IKKusudama3D : public Resource {
	GDCLASS(IKKusudama3D, Resource);

	/**
	 * An array containing all of the Kusudama's open_cones. The kusudama is built up
	 * with the expectation that any limitCone in the array is connected to the cone at the previous element in the array,
	 * and the cone at the next element in the array.
	 */
	Vector<Ref<IKLimitCone3D>> open_cones;

	Quaternion twist_min_rot;
	Vector3 twist_min_vec;
	Vector3 twist_max_vec;
	Vector3 twist_center_vec;
	Quaternion twist_center_rot;
	Quaternion twist_max_rot;
	real_t twist_half_range_half_cos = 0;
	Vector3 twist_tan;
	bool flipped_bounds = false;
	real_t resistance = 0;

	/**
	 * Defined as some Angle in radians about the limiting_axes Y axis, 0 being equivalent to the
	 * limiting_axes Z axis.
	 */
	real_t min_axial_angle = 0.0;
	/**
	 * Defined as some Angle in radians about the limiting_axes Y axis, 0 being equivalent to the
	 * min_axial_angle
	 */
	real_t range_angle = Math_TAU;

	bool orientationally_constrained = false;
	bool axially_constrained = false;

protected:
	static void _bind_methods();

public:
	~IKKusudama3D() {}

	IKKusudama3D() {}

	void _update_constraint(Ref<IKNode3D> p_limiting_axes);

	void update_tangent_radii();

	Ref<IKRay3D> bone_ray = Ref<IKRay3D>(memnew(IKRay3D()));
	Ref<IKRay3D> constrained_ray = Ref<IKRay3D>(memnew(IKRay3D()));
	double unit_hyper_area = 2 * Math::pow(Math_PI, 2);
	double unit_area = 4 * Math_PI;

	/**
	 * Get the swing rotation and twist rotation for the specified axis. The twist rotation represents the rotation around the specified axis. The swing rotation represents the rotation of the specified
	 * axis itself, which is the rotation around an axis perpendicular to the specified axis. The swing and twist rotation can be
	 * used to reconstruct the original quaternion: this = swing * twist
	 *
	 * @param p_axis the X, Y, Z component of the normalized axis for which to get the swing and twist rotation
	 * @return twist represent the rotational twist
	 * @return swing represent the rotational swing
	 * @see <a href="http://www.euclideanspace.com/maths/geometry/rotations/for/decomposition">calculation</a>
	 */
	static void get_swing_twist(
			Quaternion p_rotation,
			Vector3 p_axis,
			Quaternion &r_swing,
			Quaternion &r_twist);

	static Quaternion get_quaternion_axis_angle(const Vector3 &p_axis, real_t p_angle);

public:
	/**
	 * Presumes the input axes are the bone's localAxes, and rotates
	 * them to satisfy the snap limits.
	 *
	 * @param to_set
	 */
	void snap_to_orientation_limit(Ref<IKNode3D> p_bone_direction, Ref<IKNode3D> p_to_set, Ref<IKNode3D> p_limiting_axes, real_t p_dampening, real_t p_cos_half_angle_dampen);

	bool is_nan_vector(const Vector3 &vec);

	/**
	 * Kusudama constraints decompose the bone orientation into a swing component, and a twist component.
	 * The "Swing" component is the final direction of the bone. The "Twist" component represents how much
	 * the bone is rotated about its own final direction. Where limit cones allow you to constrain the "Swing"
	 * component, this method lets you constrain the "twist" component.
	 *
	 * @param min_angle some angle in radians about the major rotation frame's y-axis to serve as the first angle within the range_angle that the bone is allowed to twist.
	 * @param in_range some angle in radians added to the min_angle. if the bone's local Z goes maxAngle radians beyond the min_angle, it is considered past the limit.
	 * This value is always interpreted as being in the positive direction. For example, if this value is -PI/2, the entire range_angle from min_angle to min_angle + 3PI/4 is
	 * considered valid.
	 */
	void set_axial_limits(real_t p_min_angle, real_t p_in_range);

	/**
	 *
	 * @param to_set
	 * @param limiting_axes
	 * @return radians of the twist required to snap bone into twist limits (0 if bone is already in twist limits)
	 */
	void set_snap_to_twist_limit(Ref<IKNode3D> p_bone_direction, Ref<IKNode3D> p_to_set, Ref<IKNode3D> p_limiting_axes, real_t p_dampening, real_t p_cos_half_dampen);

	/**
	 * Given a point (in local coordinates), checks to see if a ray can be extended from the Kusudama's
	 * origin to that point, such that the ray in the Kusudama's reference frame is within the range_angle allowed by the Kusudama's
	 * coneLimits.
	 * If such a ray exists, the original point is returned (the point is within the limits).
	 * If it cannot exist, the tip of the ray within the kusudama's limits that would require the least rotation
	 * to arrive at the input point is returned.
	 * @param in_point the point to test.
	 * @param in_bounds should be an array with at least 2 elements. The first element will be set to  a number from -1 to 1 representing the point's distance from the boundary, 0 means the point is right on
	 * the boundary, 1 means the point is within the boundary and on the path furthest from the boundary. any negative number means
	 * the point is outside of the boundary, but does not signify anything about how far from the boundary the point is.
	 * The second element will be given a value corresponding to the limit cone whose bounds were exceeded. If the bounds were exceeded on a segment between two limit cones,
	 * this value will be set to a non-integer value between the two indices of the limitcone comprising the segment whose bounds were exceeded.
	 * @return the original point, if it's in limits, or the closest point which is in limits.
	 */
	Vector3 get_local_point_in_limits(Vector3 in_point, Vector<double> *in_bounds);

	Vector3 local_point_on_path_sequence(Vector3 in_point, Ref<IKNode3D> limiting_axes);

	/**
	 * Add a IKLimitCone to the Kusudama.
	 * @param new_point where on the Kusudama to add the LimitCone (in Kusudama's local coordinate frame defined by its bone's majorRotationAxes))
	 * @param radius the radius of the limitCone
	 */
	void add_open_cone(Ref<IKLimitCone3D> p_open_cone);
	void remove_open_cone(Ref<IKLimitCone3D> limitCone);

	/**
	 *
	 * @return the lower bound on the axial constraint
	 */
	real_t get_min_axial_angle();
	real_t get_range_angle();

	bool is_axially_constrained();
	bool is_orientationally_constrained();
	void disable_orientational_limits();
	void enable_orientational_limits();
	void toggle_orientational_limits();
	void disable_axial_limits();
	void enable_axial_limits();
	void toggle_axial_limits();
	bool is_enabled();
	void disable();
	void enable();
	void clear_open_cones();
	TypedArray<IKLimitCone3D> get_open_cones() const;
	void set_open_cones(TypedArray<IKLimitCone3D> p_cones);
	float get_resistance();
	void set_resistance(float p_resistance);
	static Quaternion clamp_to_quadrance_angle(Quaternion p_rotation, double p_cos_half_angle);
};

#endif // IK_KUSUDAMA_3D_H
