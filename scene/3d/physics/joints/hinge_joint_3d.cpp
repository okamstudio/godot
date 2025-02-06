/**************************************************************************/
/*  hinge_joint_3d.cpp                                                    */
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

#include "hinge_joint_3d.h"

void HingeJoint3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &HingeJoint3D::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &HingeJoint3D::get_param);

	ClassDB::bind_method(D_METHOD("set_flag", "flag", "enabled"), &HingeJoint3D::set_flag);
	ClassDB::bind_method(D_METHOD("get_flag", "flag"), &HingeJoint3D::get_flag);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "params/bias", PROPERTY_HINT_RANGE, "0.00,0.99,0.01"), "set_param", "get_param", PARAM_BIAS);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit/enable"), "set_flag", "get_flag", FLAG_USE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/upper", PROPERTY_HINT_RANGE, "-180,180,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_LIMIT_UPPER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/lower", PROPERTY_HINT_RANGE, "-180,180,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_LIMIT_LOWER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/bias", PROPERTY_HINT_RANGE, "0.01,0.99,0.01"), "set_param", "get_param", PARAM_LIMIT_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/softness", PROPERTY_HINT_RANGE, U"0.01,16,0.01,suffix:kg\u22C5m\u00B2/s\u00B2/rad (N\u22C5m/rad)"), "set_param", "get_param", PARAM_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/relaxation", PROPERTY_HINT_RANGE, U"0.01,16,0.01,suffix:rad\u22C5s/kg/m\u00B2 (rad/N/m/s) (inverse damping)"), "set_param", "get_param", PARAM_LIMIT_RELAXATION);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "motor/enable"), "set_flag", "get_flag", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "motor/target_velocity", PROPERTY_HINT_RANGE, U"-200,200,0.01,or_greater,or_less,radians_as_degrees,suffix:\u00B0/s"), "set_param", "get_param", PARAM_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "motor/max_impulse", PROPERTY_HINT_RANGE, "0.01,1024,0.01"), "set_param", "get_param", PARAM_MOTOR_MAX_IMPULSE);

	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_BIAS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_RELAXATION);
	BIND_ENUM_CONSTANT(PARAM_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_MOTOR_MAX_IMPULSE);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(FLAG_USE_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_MAX);
}

void HingeJoint3D::set_param(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->hinge_joint_set_param(get_rid(), PhysicsServer3D::HingeJointParam(p_param), p_value);
	}

	update_gizmos();
}

real_t HingeJoint3D::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

void HingeJoint3D::set_flag(Flag p_flag, bool p_value) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags[p_flag] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->hinge_joint_set_flag(get_rid(), PhysicsServer3D::HingeJointFlag(p_flag), p_value);
	}

	update_gizmos();
}

bool HingeJoint3D::get_flag(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags[p_flag];
}

void HingeJoint3D::_configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) {
	Transform3D gt = get_global_transform();
	Transform3D ainv = body_a->get_global_transform().affine_inverse();

	Transform3D local_a = ainv * gt;
	local_a.orthonormalize();
	Transform3D local_b = gt;

	if (body_b) {
		Transform3D binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	PhysicsServer3D::get_singleton()->joint_make_hinge(p_joint, body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer3D::get_singleton()->hinge_joint_set_param(p_joint, PhysicsServer3D::HingeJointParam(i), params[i]);
	}
	for (int i = 0; i < FLAG_MAX; i++) {
		set_flag(Flag(i), flags[i]);
		PhysicsServer3D::get_singleton()->hinge_joint_set_flag(p_joint, PhysicsServer3D::HingeJointFlag(i), flags[i]);
	}
}

HingeJoint3D::HingeJoint3D() {
	params[PARAM_BIAS] = 0.3;
	params[PARAM_LIMIT_UPPER] = Math_PI * 0.5;
	params[PARAM_LIMIT_LOWER] = -Math_PI * 0.5;
	params[PARAM_LIMIT_BIAS] = 0.3;
	params[PARAM_LIMIT_SOFTNESS] = 0.9;
	params[PARAM_LIMIT_RELAXATION] = 1.0;
	params[PARAM_MOTOR_TARGET_VELOCITY] = 1;
	params[PARAM_MOTOR_MAX_IMPULSE] = 1;

	flags[FLAG_USE_LIMIT] = false;
	flags[FLAG_ENABLE_MOTOR] = false;
}
