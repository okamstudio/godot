/**************************************************************************/
/*  ik_open_cone_3d.h                                                     */
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

#ifndef IK_OPEN_CONE_3D_H
#define IK_OPEN_CONE_3D_H

#include "core/io/resource.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"

class IKKusudama3D;
class IKLimitCone3D : public Resource {
	GDCLASS(IKLimitCone3D, Resource);
	void compute_triangles(Ref<IKLimitCone3D> p_next);

	Vector3 control_point = Vector3(0, 1, 0);
	Vector3 radial_point;

	// Radius stored as cosine to save on the acos call necessary for the angle between.
	double radius_cosine = 0;
	double radius = 0;
	Vector3 _closest_cone(Ref<IKLimitCone3D> next, Vector3 input) const;
	void set_tangent_circle_radius_next(double rad);
	WeakRef parent_kusudama;

	Vector3 tangent_circle_center_next_1;
	Vector3 tangent_circle_center_next_2;
	double tangent_circle_radius_next = 0;
	double tangent_circle_radius_next_cos = 0;

	/**
	 * A triangle where the [1] is the tangent_circle_next_n, and [0] and [2]
	 * are the points at which the tangent circle intersects this IKLimitCone and the
	 * next IKLimitCone.
	 */
	Vector<Vector3> first_triangle_next = { Vector3(), Vector3(), Vector3() };
	Vector<Vector3> second_triangle_next = { Vector3(), Vector3(), Vector3() };

	/**
	 *
	 * @param next
	 * @param input
	 * @return null if the input point is already in bounds, or the point's rectified position
	 * if the point was out of bounds.
	 */
	Vector3 _get_closest_collision(Ref<IKLimitCone3D> next, Vector3 input) const;

	/**
	 * Determines if a ray emanating from the origin to given point in local space
	 * lies within the path from this cone to the next cone. This function relies on
	 * an optimization trick for a performance boost, but the trick ruins everything
	 * if the input isn't normalized. So it is ABSOLUTELY VITAL
	 * that @param input have unit length in order for this function to work correctly.
	 * @param next
	 * @param input
	 * @return
	 */
	bool _determine_if_in_bounds(Ref<IKLimitCone3D> next, Vector3 input) const;
	Vector3 _get_on_path_sequence(Ref<IKLimitCone3D> next, Vector3 input) const;

	/**
	 * returns null if no rectification is required.
	 * @param next
	 * @param input
	 * @param in_bounds
	 * @return
	 */
	Vector3 _closest_point_on_closest_cone(Ref<IKLimitCone3D> next, Vector3 input, Vector<double> *in_bounds) const;

	double _get_tangent_circle_radius_next_cos();

public:
	IKLimitCone3D() {}
	virtual ~IKLimitCone3D() {}
	void set_attached_to(Ref<IKKusudama3D> p_attached_to);
	Ref<IKKusudama3D> get_attached_to();
	void update_tangent_handles(Ref<IKLimitCone3D> p_next);
	void set_tangent_circle_center_next_1(Vector3 point);
	void set_tangent_circle_center_next_2(Vector3 point);
	/**
	 *
	 * @param next
	 * @param input
	 * @return null if inapplicable for rectification. the original point if in bounds, or the point rectified to the closest boundary on the path sequence
	 * between two cones if the point is out of bounds and applicable for rectification.
	 */
	Vector3 get_on_great_tangent_triangle(Ref<IKLimitCone3D> next, Vector3 input) const;
	double get_tangent_circle_radius_next();
	Vector3 get_tangent_circle_center_next_1();
	Vector3 get_tangent_circle_center_next_2();

	/**
	 * returns null if no rectification is required.
	 * @param input
	 * @param in_bounds
	 * @return
	 */
	Vector3 closest_to_cone(Vector3 input, Vector<double> *in_bounds) const;
	Vector3 get_closest_path_point(Ref<IKLimitCone3D> next, Vector3 input) const;
	Vector3 get_control_point() const;
	void set_control_point(Vector3 p_control_point);
	double get_radius() const;
	double get_radius_cosine() const;
	void set_radius(double radius);
	static Vector3 get_orthogonal(Vector3 p_input);
};

#endif // IK_OPEN_CONE_3D_H
