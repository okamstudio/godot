/**************************************************************************/
/*  godot_collision_solver_3d.h                                           */
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

#include "godot_shape_3d.h"

class GodotCollisionSolver3D {
public:
	typedef void (*CallbackResult)(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata);

private:
	static bool soft_body_query_callback(uint32_t p_node_index, void *p_userdata);
	static void soft_body_contact_callback(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata);
	static bool soft_body_concave_callback(void *p_userdata, GodotShape3D *p_convex);
	static bool concave_callback(void *p_userdata, GodotShape3D *p_convex);
	static bool solve_static_world_boundary(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin = 0);
	static bool solve_separation_ray(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin = 0);
	static bool solve_soft_body(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result);
	static bool solve_concave(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin_A = 0, real_t p_margin_B = 0);
	static bool concave_distance_callback(void *p_userdata, GodotShape3D *p_convex);
	static bool solve_distance_world_boundary(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, Vector3 &r_point_A, Vector3 &r_point_B);

public:
	static bool solve_static(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, Vector3 *r_sep_axis = nullptr, real_t p_margin_A = 0, real_t p_margin_B = 0);
	static bool solve_distance(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, Vector3 &r_point_A, Vector3 &r_point_B, const AABB &p_concave_hint, Vector3 *r_sep_axis = nullptr);
};
