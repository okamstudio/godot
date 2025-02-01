/**************************************************************************/
/*  godot_collision_solver_2d.h                                           */
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

#include "godot_shape_2d.h"

class GodotCollisionSolver2D {
public:
	typedef void (*CallbackResult)(const Vector2 &p_point_A, const Vector2 &p_point_B, void *p_userdata);

private:
	static bool solve_static_world_boundary(const GodotShape2D *p_shape_A, const Transform2D &p_transform_A, const GodotShape2D *p_shape_B, const Transform2D &p_transform_B, const Vector2 &p_motion_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, real_t p_margin = 0);
	static bool concave_callback(void *p_userdata, GodotShape2D *p_convex);
	static bool solve_concave(const GodotShape2D *p_shape_A, const Transform2D &p_transform_A, const Vector2 &p_motion_A, const GodotShape2D *p_shape_B, const Transform2D &p_transform_B, const Vector2 &p_motion_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, Vector2 *r_sep_axis = nullptr, real_t p_margin_A = 0, real_t p_margin_B = 0);
	static bool solve_separation_ray(const GodotShape2D *p_shape_A, const Vector2 &p_motion_A, const Transform2D &p_transform_A, const GodotShape2D *p_shape_B, const Transform2D &p_transform_B, CallbackResult p_result_callback, void *p_userdata, bool p_swap_result, Vector2 *r_sep_axis = nullptr, real_t p_margin = 0);

public:
	static bool solve(const GodotShape2D *p_shape_A, const Transform2D &p_transform_A, const Vector2 &p_motion_A, const GodotShape2D *p_shape_B, const Transform2D &p_transform_B, const Vector2 &p_motion_B, CallbackResult p_result_callback, void *p_userdata, Vector2 *r_sep_axis = nullptr, real_t p_margin_A = 0, real_t p_margin_B = 0);
};
