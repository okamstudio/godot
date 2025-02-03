/**************************************************************************/
/*  generic_6dof_joint_bullet.h                                           */
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

#ifndef GENERIC_6DOF_JOINT_BULLET_H
#define GENERIC_6DOF_JOINT_BULLET_H

#include "joint_bullet.h"

/**
	@author AndreaCatania
*/

class RigidBodyBullet;

class Generic6DOFJointBullet : public JointBullet {
	class btGeneric6DofSpringConstraintQuaternion *sixDOFConstraint;

	// First is linear second is angular
	Vector3 limits_lower[2];
	Vector3 limits_upper[2];
	bool flags[3][PhysicsServer::G6DOF_JOINT_FLAG_MAX];

public:
	Generic6DOFJointBullet(RigidBodyBullet *rbA, RigidBodyBullet *rbB, const Transform &frameInA, const Transform &frameInB);

	virtual PhysicsServer::JointType get_type() const { return PhysicsServer::JOINT_6DOF; }

	Transform getFrameOffsetA() const;
	Transform getFrameOffsetB() const;
	Transform getFrameOffsetA();
	Transform getFrameOffsetB();

	void set_linear_lower_limit(const Vector3 &linearLower);
	void set_linear_upper_limit(const Vector3 &linearUpper);

	void set_angular_lower_limit(const Vector3 &angularLower);
	void set_angular_upper_limit(const Vector3 &angularUpper);

	void set_param(Vector3::Axis p_axis, PhysicsServer::G6DOFJointAxisParam p_param, real_t p_value);
	real_t get_param(Vector3::Axis p_axis, PhysicsServer::G6DOFJointAxisParam p_param) const;

	void set_flag(Vector3::Axis p_axis, PhysicsServer::G6DOFJointAxisFlag p_flag, bool p_value);
	bool get_flag(Vector3::Axis p_axis, PhysicsServer::G6DOFJointAxisFlag p_flag) const;

	void set_use_global_rotation(bool p_value);
	bool get_use_global_rotation();

	void set_use_quaternion_rotation_equilibrium(bool p_enabled);
	bool get_use_quaternion_rotation_equilibrium();
	void set_quaternion_rotation_equilibrium(Quat p_value);
	Quat get_quaternion_rotation_equilibrium();
};

#endif // GENERIC_6DOF_JOINT_BULLET_H
