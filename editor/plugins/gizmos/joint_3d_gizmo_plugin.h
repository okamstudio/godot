/**************************************************************************/
/*  joint_3d_gizmo_plugin.h                                               */
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

#include "editor/plugins/node_3d_editor_gizmos.h"

class Joint3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(Joint3DGizmoPlugin, EditorNode3DGizmoPlugin);

	Timer *update_timer = nullptr;
	EditorNode3DGizmo *last_drawn = nullptr;

	void incremental_update_gizmos();

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	static void CreatePinJointGizmo(const Transform3D &p_offset, Vector<Vector3> &r_cursor_points);
	static void CreateHingeJointGizmo(const Transform3D &p_offset, const Transform3D &p_trs_joint, const Transform3D &p_trs_body_a, const Transform3D &p_trs_body_b, real_t p_limit_lower, real_t p_limit_upper, bool p_use_limit, Vector<Vector3> &r_common_points, Vector<Vector3> *r_body_a_points, Vector<Vector3> *r_body_b_points);
	static void CreateSliderJointGizmo(const Transform3D &p_offset, const Transform3D &p_trs_joint, const Transform3D &p_trs_body_a, const Transform3D &p_trs_body_b, real_t p_angular_limit_lower, real_t p_angular_limit_upper, real_t p_linear_limit_lower, real_t p_linear_limit_upper, Vector<Vector3> &r_points, Vector<Vector3> *r_body_a_points, Vector<Vector3> *r_body_b_points);
	static void CreateConeTwistJointGizmo(const Transform3D &p_offset, const Transform3D &p_trs_joint, const Transform3D &p_trs_body_a, const Transform3D &p_trs_body_b, real_t p_swing, real_t p_twist, Vector<Vector3> *r_body_a_points, Vector<Vector3> *r_body_b_points);
	static void CreateGeneric6DOFJointGizmo(
			const Transform3D &p_offset,
			const Transform3D &p_trs_joint,
			const Transform3D &p_trs_body_a,
			const Transform3D &p_trs_body_b,
			real_t p_angular_limit_lower_x,
			real_t p_angular_limit_upper_x,
			real_t p_linear_limit_lower_x,
			real_t p_linear_limit_upper_x,
			bool p_enable_angular_limit_x,
			bool p_enable_linear_limit_x,
			real_t p_angular_limit_lower_y,
			real_t p_angular_limit_upper_y,
			real_t p_linear_limit_lower_y,
			real_t p_linear_limit_upper_y,
			bool p_enable_angular_limit_y,
			bool p_enable_linear_limit_y,
			real_t p_angular_limit_lower_z,
			real_t p_angular_limit_upper_z,
			real_t p_linear_limit_lower_z,
			real_t p_linear_limit_upper_z,
			bool p_enable_angular_limit_z,
			bool p_enable_linear_limit_z,
			Vector<Vector3> &r_points,
			Vector<Vector3> *r_body_a_points,
			Vector<Vector3> *r_body_b_points);

	Joint3DGizmoPlugin();
};

class JointGizmosDrawer {
public:
	static Basis look_body(const Transform3D &p_joint_transform, const Transform3D &p_body_transform);
	static Basis look_body_toward(Vector3::Axis p_axis, const Transform3D &joint_transform, const Transform3D &body_transform);
	static Basis look_body_toward_x(const Transform3D &p_joint_transform, const Transform3D &p_body_transform);
	static Basis look_body_toward_y(const Transform3D &p_joint_transform, const Transform3D &p_body_transform);
	/// Special function just used for physics joints, it returns a basis constrained toward Joint Z axis
	/// with axis X and Y that are looking toward the body and oriented toward up
	static Basis look_body_toward_z(const Transform3D &p_joint_transform, const Transform3D &p_body_transform);

	// Draw circle around p_axis
	static void draw_circle(Vector3::Axis p_axis, real_t p_radius, const Transform3D &p_offset, const Basis &p_base, real_t p_limit_lower, real_t p_limit_upper, Vector<Vector3> &r_points, bool p_inverse = false);
	static void draw_cone(const Transform3D &p_offset, const Basis &p_base, real_t p_swing, real_t p_twist, Vector<Vector3> &r_points);
};
