/**************************************************************************/
/*  physics_joint.h                                                       */
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

#ifndef PHYSICS_JOINT_H
#define PHYSICS_JOINT_H

#include "scene/3d/physics_body.h"
#include "scene/3d/spatial.h"

#include "core/math/quat.h"

class Joint : public Spatial {
	GDCLASS(Joint, Spatial);

	RID ba, bb;

	RID joint;

	NodePath a;
	NodePath b;

	int solver_priority;
	bool exclude_from_collision;
	String warning;

protected:
	void _disconnect_signals();
	void _body_exit_tree();
	void _update_joint(bool p_only_free = false);

	void _notification(int p_what);

	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) = 0;

	static void _bind_methods();

public:
	virtual String get_configuration_warning() const;

	void set_node_a(const NodePath &p_node_a);
	NodePath get_node_a() const;

	void set_node_b(const NodePath &p_node_b);
	NodePath get_node_b() const;

	void set_solver_priority(int p_priority);
	int get_solver_priority() const;

	void set_exclude_nodes_from_collision(bool p_enable);
	bool get_exclude_nodes_from_collision() const;

	RID get_joint() const { return joint; }
	Joint();
};

///////////////////////////////////////////

class PinJoint : public Joint {
	GDCLASS(PinJoint, Joint);

public:
	enum Param {
		PARAM_BIAS = PhysicsServer::PIN_JOINT_BIAS,
		PARAM_DAMPING = PhysicsServer::PIN_JOINT_DAMPING,
		PARAM_IMPULSE_CLAMP = PhysicsServer::PIN_JOINT_IMPULSE_CLAMP
	};

protected:
	float params[3];
	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b);
	static void _bind_methods();

public:
	void set_param(Param p_param, float p_value);
	float get_param(Param p_param) const;

	PinJoint();
};

VARIANT_ENUM_CAST(PinJoint::Param);

class HingeJoint : public Joint {
	GDCLASS(HingeJoint, Joint);

public:
	enum Param {
		PARAM_BIAS = PhysicsServer::HINGE_JOINT_BIAS,
		PARAM_LIMIT_UPPER = PhysicsServer::HINGE_JOINT_LIMIT_UPPER,
		PARAM_LIMIT_LOWER = PhysicsServer::HINGE_JOINT_LIMIT_LOWER,
		PARAM_LIMIT_BIAS = PhysicsServer::HINGE_JOINT_LIMIT_BIAS,
		PARAM_LIMIT_SOFTNESS = PhysicsServer::HINGE_JOINT_LIMIT_SOFTNESS,
		PARAM_LIMIT_RELAXATION = PhysicsServer::HINGE_JOINT_LIMIT_RELAXATION,
		PARAM_MOTOR_TARGET_VELOCITY = PhysicsServer::HINGE_JOINT_MOTOR_TARGET_VELOCITY,
		PARAM_MOTOR_MAX_IMPULSE = PhysicsServer::HINGE_JOINT_MOTOR_MAX_IMPULSE,
		PARAM_MAX = PhysicsServer::HINGE_JOINT_MAX
	};

	enum Flag {
		FLAG_USE_LIMIT = PhysicsServer::HINGE_JOINT_FLAG_USE_LIMIT,
		FLAG_ENABLE_MOTOR = PhysicsServer::HINGE_JOINT_FLAG_ENABLE_MOTOR,
		FLAG_MAX = PhysicsServer::HINGE_JOINT_FLAG_MAX
	};

protected:
	float params[PARAM_MAX];
	bool flags[FLAG_MAX];
	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b);
	static void _bind_methods();

	void _set_upper_limit(float p_limit);
	float _get_upper_limit() const;

	void _set_lower_limit(float p_limit);
	float _get_lower_limit() const;

public:
	void set_param(Param p_param, float p_value);
	float get_param(Param p_param) const;

	void set_flag(Flag p_flag, bool p_value);
	bool get_flag(Flag p_flag) const;

	HingeJoint();
};

VARIANT_ENUM_CAST(HingeJoint::Param);
VARIANT_ENUM_CAST(HingeJoint::Flag);

class SliderJoint : public Joint {
	GDCLASS(SliderJoint, Joint);

public:
	enum Param {
		PARAM_LINEAR_LIMIT_UPPER = PhysicsServer::SLIDER_JOINT_LINEAR_LIMIT_UPPER,
		PARAM_LINEAR_LIMIT_LOWER = PhysicsServer::SLIDER_JOINT_LINEAR_LIMIT_LOWER,
		PARAM_LINEAR_LIMIT_SOFTNESS = PhysicsServer::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS,
		PARAM_LINEAR_LIMIT_RESTITUTION = PhysicsServer::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION,
		PARAM_LINEAR_LIMIT_DAMPING = PhysicsServer::SLIDER_JOINT_LINEAR_LIMIT_DAMPING,
		PARAM_LINEAR_MOTION_SOFTNESS = PhysicsServer::SLIDER_JOINT_LINEAR_MOTION_SOFTNESS,
		PARAM_LINEAR_MOTION_RESTITUTION = PhysicsServer::SLIDER_JOINT_LINEAR_MOTION_RESTITUTION,
		PARAM_LINEAR_MOTION_DAMPING = PhysicsServer::SLIDER_JOINT_LINEAR_MOTION_DAMPING,
		PARAM_LINEAR_ORTHOGONAL_SOFTNESS = PhysicsServer::SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS,
		PARAM_LINEAR_ORTHOGONAL_RESTITUTION = PhysicsServer::SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION,
		PARAM_LINEAR_ORTHOGONAL_DAMPING = PhysicsServer::SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING,

		PARAM_ANGULAR_LIMIT_UPPER = PhysicsServer::SLIDER_JOINT_ANGULAR_LIMIT_UPPER,
		PARAM_ANGULAR_LIMIT_LOWER = PhysicsServer::SLIDER_JOINT_ANGULAR_LIMIT_LOWER,
		PARAM_ANGULAR_LIMIT_SOFTNESS = PhysicsServer::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS,
		PARAM_ANGULAR_LIMIT_RESTITUTION = PhysicsServer::SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION,
		PARAM_ANGULAR_LIMIT_DAMPING = PhysicsServer::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING,
		PARAM_ANGULAR_MOTION_SOFTNESS = PhysicsServer::SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS,
		PARAM_ANGULAR_MOTION_RESTITUTION = PhysicsServer::SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION,
		PARAM_ANGULAR_MOTION_DAMPING = PhysicsServer::SLIDER_JOINT_ANGULAR_MOTION_DAMPING,
		PARAM_ANGULAR_ORTHOGONAL_SOFTNESS = PhysicsServer::SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS,
		PARAM_ANGULAR_ORTHOGONAL_RESTITUTION = PhysicsServer::SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION,
		PARAM_ANGULAR_ORTHOGONAL_DAMPING = PhysicsServer::SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING,
		PARAM_MAX = PhysicsServer::SLIDER_JOINT_MAX

	};

protected:
	void _set_upper_limit_angular(float p_limit_angular);
	float _get_upper_limit_angular() const;

	void _set_lower_limit_angular(float p_limit_angular);
	float _get_lower_limit_angular() const;

	float params[PARAM_MAX];
	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b);
	static void _bind_methods();

public:
	void set_param(Param p_param, float p_value);
	float get_param(Param p_param) const;

	SliderJoint();
};

VARIANT_ENUM_CAST(SliderJoint::Param);

class ConeTwistJoint : public Joint {
	GDCLASS(ConeTwistJoint, Joint);

public:
	enum Param {

		PARAM_SWING_SPAN,
		PARAM_TWIST_SPAN,
		PARAM_BIAS,
		PARAM_SOFTNESS,
		PARAM_RELAXATION,
		PARAM_MAX
	};

protected:
	void _set_swing_span(float p_limit_angular);
	float _get_swing_span() const;

	void _set_twist_span(float p_limit_angular);
	float _get_twist_span() const;

	float params[PARAM_MAX];
	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b);
	static void _bind_methods();

public:
	void set_param(Param p_param, float p_value);
	float get_param(Param p_param) const;

	ConeTwistJoint();
};

VARIANT_ENUM_CAST(ConeTwistJoint::Param);

class Generic6DOFJoint : public Joint {
	GDCLASS(Generic6DOFJoint, Joint);

public:
	enum Param {

		PARAM_LINEAR_LOWER_LIMIT = PhysicsServer::G6DOF_JOINT_LINEAR_LOWER_LIMIT,
		PARAM_LINEAR_UPPER_LIMIT = PhysicsServer::G6DOF_JOINT_LINEAR_UPPER_LIMIT,
		PARAM_LINEAR_LIMIT_SOFTNESS = PhysicsServer::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS,
		PARAM_LINEAR_RESTITUTION = PhysicsServer::G6DOF_JOINT_LINEAR_RESTITUTION,
		PARAM_LINEAR_DAMPING = PhysicsServer::G6DOF_JOINT_LINEAR_DAMPING,
		PARAM_LINEAR_MOTOR_TARGET_VELOCITY = PhysicsServer::G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY,
		PARAM_LINEAR_MOTOR_FORCE_LIMIT = PhysicsServer::G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT,
		PARAM_LINEAR_SPRING_STIFFNESS = PhysicsServer::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS,
		PARAM_LINEAR_SPRING_DAMPING = PhysicsServer::G6DOF_JOINT_LINEAR_SPRING_DAMPING,
		PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT = PhysicsServer::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT,
		PARAM_ANGULAR_LOWER_LIMIT = PhysicsServer::G6DOF_JOINT_ANGULAR_LOWER_LIMIT,
		PARAM_ANGULAR_UPPER_LIMIT = PhysicsServer::G6DOF_JOINT_ANGULAR_UPPER_LIMIT,
		PARAM_ANGULAR_LIMIT_SOFTNESS = PhysicsServer::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS,
		PARAM_ANGULAR_DAMPING = PhysicsServer::G6DOF_JOINT_ANGULAR_DAMPING,
		PARAM_ANGULAR_RESTITUTION = PhysicsServer::G6DOF_JOINT_ANGULAR_RESTITUTION,
		PARAM_ANGULAR_FORCE_LIMIT = PhysicsServer::G6DOF_JOINT_ANGULAR_FORCE_LIMIT,
		PARAM_ANGULAR_ERP = PhysicsServer::G6DOF_JOINT_ANGULAR_ERP,
		PARAM_ANGULAR_MOTOR_TARGET_VELOCITY = PhysicsServer::G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY,
		PARAM_ANGULAR_MOTOR_FORCE_LIMIT = PhysicsServer::G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT,
		PARAM_ANGULAR_SPRING_STIFFNESS = PhysicsServer::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS,
		PARAM_ANGULAR_SPRING_DAMPING = PhysicsServer::G6DOF_JOINT_ANGULAR_SPRING_DAMPING,
		PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT = PhysicsServer::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT,
		PARAM_MAX = PhysicsServer::G6DOF_JOINT_MAX,
	};

	enum Flag {
		FLAG_ENABLE_LINEAR_LIMIT = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT,
		FLAG_ENABLE_ANGULAR_LIMIT = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT,
		FLAG_ENABLE_LINEAR_SPRING = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING,
		FLAG_ENABLE_ANGULAR_SPRING = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING,
		FLAG_ENABLE_MOTOR = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_MOTOR,
		FLAG_ENABLE_LINEAR_MOTOR = PhysicsServer::G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR,
		FLAG_MAX = PhysicsServer::G6DOF_JOINT_FLAG_MAX
	};

protected:
	void _set_angular_hi_limit_x(float p_limit_angular);
	float _get_angular_hi_limit_x() const;

	void _set_angular_hi_limit_y(float p_limit_angular);
	float _get_angular_hi_limit_y() const;

	void _set_angular_hi_limit_z(float p_limit_angular);
	float _get_angular_hi_limit_z() const;

	void _set_angular_lo_limit_x(float p_limit_angular);
	float _get_angular_lo_limit_x() const;

	void _set_angular_lo_limit_y(float p_limit_angular);
	float _get_angular_lo_limit_y() const;

	void _set_angular_lo_limit_z(float p_limit_angular);
	float _get_angular_lo_limit_z() const;

	float params_x[PARAM_MAX];
	bool flags_x[FLAG_MAX];
	float params_y[PARAM_MAX];
	bool flags_y[FLAG_MAX];
	float params_z[PARAM_MAX];
	bool flags_z[FLAG_MAX];

	virtual RID _configure_joint(PhysicsBody *body_a, PhysicsBody *body_b);
	static void _bind_methods();

	virtual void _validate_property(PropertyInfo &p_property) const;

private:
	bool using_global_rotation;
	bool using_quaternion_rotation_equilibrium;
	Quat quaternion_rotation_equilibrium;

public:
	void set_param_x(Param p_param, float p_value);
	float get_param_x(Param p_param) const;

	void set_param_y(Param p_param, float p_value);
	float get_param_y(Param p_param) const;

	void set_param_z(Param p_param, float p_value);
	float get_param_z(Param p_param) const;

	void set_flag_x(Flag p_flag, bool p_enabled);
	bool get_flag_x(Flag p_flag) const;

	void set_flag_y(Flag p_flag, bool p_enabled);
	bool get_flag_y(Flag p_flag) const;

	void set_flag_z(Flag p_flag, bool p_enabled);
	bool get_flag_z(Flag p_flag) const;

	void set_use_global_rotation(bool p_enabled);
	bool get_use_global_rotation() const;

	void set_use_quaternion_rotation_equilibrium(bool p_enabled);
	bool get_use_quaternion_rotation_equilibrium() const;

	void set_quaternion_rotation_equilibrium(Quat p_value);
	Quat get_quaternion_rotation_equilibrium() const;

	Generic6DOFJoint();
};

VARIANT_ENUM_CAST(Generic6DOFJoint::Param);
VARIANT_ENUM_CAST(Generic6DOFJoint::Flag);

#endif // PHYSICS_JOINT_H
