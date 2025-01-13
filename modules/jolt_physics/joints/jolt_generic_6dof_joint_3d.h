/**************************************************************************/
/*  jolt_generic_6dof_joint_3d.h                                          */
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

#ifndef JOLT_GENERIC_6DOF_JOINT_3D_H
#define JOLT_GENERIC_6DOF_JOINT_3D_H

#include "../jolt_physics_server_3d.h"
#include "jolt_joint_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Constraints/SixDOFConstraint.h"

class JoltGeneric6DOFJoint3D final : public JoltJoint3D {
	typedef Vector3::Axis Axis;
	typedef JPH::SixDOFConstraintSettings::EAxis JoltAxis;
	typedef PhysicsServer3D::G6DOFJointAxisParam Param;
	typedef JoltPhysicsServer3D::G6DOFJointAxisParamJolt JoltParam;
	typedef PhysicsServer3D::G6DOFJointAxisFlag Flag;
	typedef JoltPhysicsServer3D::G6DOFJointAxisFlagJolt JoltFlag;

	enum {
		AXIS_LINEAR_X = JoltAxis::TranslationX,
		AXIS_LINEAR_Y = JoltAxis::TranslationY,
		AXIS_LINEAR_Z = JoltAxis::TranslationZ,
		AXIS_ANGULAR_X = JoltAxis::RotationX,
		AXIS_ANGULAR_Y = JoltAxis::RotationY,
		AXIS_ANGULAR_Z = JoltAxis::RotationZ,
		AXIS_COUNT = JoltAxis::Num,
		AXES_LINEAR = AXIS_LINEAR_X,
		AXES_ANGULAR = AXIS_ANGULAR_X,
	};

	double limit_lower[AXIS_COUNT] = {};
	double limit_upper[AXIS_COUNT] = {};

	double limit_spring_frequency[AXIS_COUNT] = {};
	double limit_spring_damping[AXIS_COUNT] = {};

	double motor_speed[AXIS_COUNT] = {};
	double motor_limit[AXIS_COUNT] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	double spring_stiffness[AXIS_COUNT] = {};
	double spring_frequency[AXIS_COUNT] = {};
	double spring_damping[AXIS_COUNT] = {};
	double spring_equilibrium[AXIS_COUNT] = {};
	double spring_limit[AXIS_COUNT] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	bool limit_enabled[AXIS_COUNT] = {};

	bool limit_spring_enabled[AXIS_COUNT] = {};

	bool motor_enabled[AXIS_COUNT] = {};

	bool spring_enabled[AXIS_COUNT] = {};
	bool spring_use_frequency[AXIS_COUNT] = {};

	JPH::Constraint *_build_6dof(JPH::Body *p_jolt_body_a, JPH::Body *p_jolt_body_b, const Transform3D &p_shifted_ref_a, const Transform3D &p_shifted_ref_b) const;

	void _update_limit_spring_parameters(int p_axis);
	void _update_motor_state(int p_axis);
	void _update_motor_velocity(int p_axis);
	void _update_motor_limit(int p_axis);
	void _update_spring_parameters(int p_axis);
	void _update_spring_equilibrium(int p_axis);

	void _limits_changed();
	void _limit_spring_parameters_changed(int p_axis);
	void _motor_state_changed(int p_axis);
	void _motor_speed_changed(int p_axis);
	void _motor_limit_changed(int p_axis);
	void _spring_state_changed(int p_axis);
	void _spring_parameters_changed(int p_axis);
	void _spring_equilibrium_changed(int p_axis);
	void _spring_limit_changed(int p_axis);

public:
	JoltGeneric6DOFJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b);

	virtual PhysicsServer3D::JointType get_type() const override { return PhysicsServer3D::JOINT_TYPE_6DOF; }

	double get_param(Axis p_axis, Param p_param) const;
	void set_param(Axis p_axis, Param p_param, double p_value);

	bool get_flag(Axis p_axis, Flag p_flag) const;
	void set_flag(Axis p_axis, Flag p_flag, bool p_enabled);

	double get_jolt_param(Axis p_axis, JoltParam p_param) const;
	void set_jolt_param(Axis p_axis, JoltParam p_param, double p_value);

	bool get_jolt_flag(Axis p_axis, JoltFlag p_flag) const;
	void set_jolt_flag(Axis p_axis, JoltFlag p_flag, bool p_enabled);

	float get_applied_force() const;
	float get_applied_torque() const;

	virtual void rebuild() override;
};

#endif // JOLT_GENERIC_6DOF_JOINT_3D_H
