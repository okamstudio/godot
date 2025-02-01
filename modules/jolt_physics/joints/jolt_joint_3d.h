/**************************************************************************/
/*  jolt_joint_3d.h                                                       */
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

#include "servers/physics_server_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Constraints/Constraint.h"

class JoltBody3D;
class JoltSpace3D;

class JoltJoint3D {
protected:
	bool enabled = true;
	bool collision_disabled = false;

	int velocity_iterations = 0;
	int position_iterations = 0;

	JPH::Ref<JPH::Constraint> jolt_ref;

	JoltBody3D *body_a = nullptr;
	JoltBody3D *body_b = nullptr;

	RID rid;

	Transform3D local_ref_a;
	Transform3D local_ref_b;

	void _shift_reference_frames(const Vector3 &p_linear_shift, const Vector3 &p_angular_shift, Transform3D &r_shifted_ref_a, Transform3D &r_shifted_ref_b);

	void _wake_up_bodies();

	void _update_enabled();
	void _update_iterations();

	void _enabled_changed();
	void _iterations_changed();

	String _bodies_to_string() const;

public:
	JoltJoint3D() = default;
	JoltJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b);
	virtual ~JoltJoint3D();

	virtual PhysicsServer3D::JointType get_type() const { return PhysicsServer3D::JOINT_TYPE_MAX; }

	RID get_rid() const { return rid; }
	void set_rid(const RID &p_rid) { rid = p_rid; }

	JoltSpace3D *get_space() const;

	JPH::Constraint *get_jolt_ref() const { return jolt_ref; }

	bool is_enabled() const { return enabled; }
	void set_enabled(bool p_enabled);

	int get_solver_priority() const;
	void set_solver_priority(int p_priority);

	int get_solver_velocity_iterations() const { return velocity_iterations; }
	void set_solver_velocity_iterations(int p_iterations);

	int get_solver_position_iterations() const { return position_iterations; }
	void set_solver_position_iterations(int p_iterations);

	bool is_collision_disabled() const { return collision_disabled; }
	void set_collision_disabled(bool p_disabled);

	void destroy();

	virtual void rebuild() {}
};
