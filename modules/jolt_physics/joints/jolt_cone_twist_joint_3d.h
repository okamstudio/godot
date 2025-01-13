/**************************************************************************/
/*  jolt_cone_twist_joint_3d.h                                            */
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

#ifndef JOLT_CONE_TWIST_JOINT_3D_H
#define JOLT_CONE_TWIST_JOINT_3D_H

#include "../jolt_physics_server_3d.h"
#include "jolt_joint_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"

class JoltConeTwistJoint3D final : public JoltJoint3D {
	typedef PhysicsServer3D::ConeTwistJointParam Parameter;
	typedef JoltPhysicsServer3D::ConeTwistJointParamJolt JoltParameter;
	typedef JoltPhysicsServer3D::ConeTwistJointFlagJolt JoltFlag;

	double swing_limit_span = 0.0;
	double twist_limit_span = 0.0;

	double swing_motor_target_speed_y = 0.0;
	double swing_motor_target_speed_z = 0.0;
	double twist_motor_target_speed = 0.0;

	double swing_motor_max_torque = FLT_MAX;
	double twist_motor_max_torque = FLT_MAX;

	bool swing_limit_enabled = true;
	bool twist_limit_enabled = true;

	bool swing_motor_enabled = false;
	bool twist_motor_enabled = false;

	JPH::Constraint *_build_swing_twist(JPH::Body *p_jolt_body_a, JPH::Body *p_jolt_body_b, const Transform3D &p_shifted_ref_a, const Transform3D &p_shifted_ref_b, float p_swing_limit_span, float p_twist_limit_span) const;

	void _update_swing_motor_state();
	void _update_twist_motor_state();
	void _update_motor_velocity();
	void _update_swing_motor_limit();
	void _update_twist_motor_limit();

	void _limits_changed();
	void _swing_motor_state_changed();
	void _twist_motor_state_changed();
	void _motor_velocity_changed();
	void _swing_motor_limit_changed();
	void _twist_motor_limit_changed();

public:
	JoltConeTwistJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b);

	virtual PhysicsServer3D::JointType get_type() const override { return PhysicsServer3D::JOINT_TYPE_CONE_TWIST; }

	double get_param(PhysicsServer3D::ConeTwistJointParam p_param) const;
	void set_param(PhysicsServer3D::ConeTwistJointParam p_param, double p_value);

	double get_jolt_param(JoltParameter p_param) const;
	void set_jolt_param(JoltParameter p_param, double p_value);

	bool get_jolt_flag(JoltFlag p_flag) const;
	void set_jolt_flag(JoltFlag p_flag, bool p_enabled);

	float get_applied_force() const;
	float get_applied_torque() const;

	virtual void rebuild() override;
};

#endif // JOLT_CONE_TWIST_JOINT_3D_H
