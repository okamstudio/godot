/**************************************************************************/
/*  generic_6dof_joint_3d.h                                               */
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

#include "scene/3d/physics/joints/joint_3d.h"

class Generic6DOFJoint3D : public Joint3D {
	GDCLASS(Generic6DOFJoint3D, Joint3D);

public:
	enum Param {
		PARAM_LINEAR_LOWER_LIMIT = PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT,
		PARAM_LINEAR_UPPER_LIMIT = PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT,
		PARAM_LINEAR_LIMIT_SOFTNESS = PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS,
		PARAM_LINEAR_RESTITUTION = PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION,
		PARAM_LINEAR_DAMPING = PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING,
		PARAM_LINEAR_MOTOR_TARGET_VELOCITY = PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY,
		PARAM_LINEAR_MOTOR_FORCE_LIMIT = PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT,
		PARAM_LINEAR_SPRING_STIFFNESS = PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS,
		PARAM_LINEAR_SPRING_DAMPING = PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING,
		PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT = PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT,
		PARAM_ANGULAR_LOWER_LIMIT = PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT,
		PARAM_ANGULAR_UPPER_LIMIT = PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT,
		PARAM_ANGULAR_LIMIT_SOFTNESS = PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS,
		PARAM_ANGULAR_DAMPING = PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING,
		PARAM_ANGULAR_RESTITUTION = PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION,
		PARAM_ANGULAR_FORCE_LIMIT = PhysicsServer3D::G6DOF_JOINT_ANGULAR_FORCE_LIMIT,
		PARAM_ANGULAR_ERP = PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP,
		PARAM_ANGULAR_MOTOR_TARGET_VELOCITY = PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY,
		PARAM_ANGULAR_MOTOR_FORCE_LIMIT = PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT,
		PARAM_ANGULAR_SPRING_STIFFNESS = PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS,
		PARAM_ANGULAR_SPRING_DAMPING = PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING,
		PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT = PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT,
		PARAM_MAX = PhysicsServer3D::G6DOF_JOINT_MAX,
	};

	enum Flag {
		FLAG_ENABLE_LINEAR_LIMIT = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT,
		FLAG_ENABLE_ANGULAR_LIMIT = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT,
		FLAG_ENABLE_LINEAR_SPRING = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING,
		FLAG_ENABLE_ANGULAR_SPRING = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING,
		FLAG_ENABLE_MOTOR = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_MOTOR,
		FLAG_ENABLE_LINEAR_MOTOR = PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR,
		FLAG_MAX = PhysicsServer3D::G6DOF_JOINT_FLAG_MAX
	};

protected:
	real_t params_x[PARAM_MAX];
	bool flags_x[FLAG_MAX];
	real_t params_y[PARAM_MAX];
	bool flags_y[FLAG_MAX];
	real_t params_z[PARAM_MAX];
	bool flags_z[FLAG_MAX];

	virtual void _configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) override;
	static void _bind_methods();

public:
	void set_param_x(Param p_param, real_t p_value);
	real_t get_param_x(Param p_param) const;

	void set_param_y(Param p_param, real_t p_value);
	real_t get_param_y(Param p_param) const;

	void set_param_z(Param p_param, real_t p_value);
	real_t get_param_z(Param p_param) const;

	void set_flag_x(Flag p_flag, bool p_enabled);
	bool get_flag_x(Flag p_flag) const;

	void set_flag_y(Flag p_flag, bool p_enabled);
	bool get_flag_y(Flag p_flag) const;

	void set_flag_z(Flag p_flag, bool p_enabled);
	bool get_flag_z(Flag p_flag) const;

	Generic6DOFJoint3D();
};

VARIANT_ENUM_CAST(Generic6DOFJoint3D::Param);
VARIANT_ENUM_CAST(Generic6DOFJoint3D::Flag);
