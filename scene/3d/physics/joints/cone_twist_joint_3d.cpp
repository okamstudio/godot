/**************************************************************************/
/*  cone_twist_joint_3d.cpp                                               */
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

#include "cone_twist_joint_3d.h"

void ConeTwistJoint3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &ConeTwistJoint3D::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &ConeTwistJoint3D::get_param);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "swing_span", PROPERTY_HINT_RANGE, "-180,180,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_SWING_SPAN);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "twist_span", PROPERTY_HINT_RANGE, "-40000,40000,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_TWIST_SPAN);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "bias", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg\u22C5m\u00B2/s\u00B2/rad (N\u22C5m/rad)"), "set_param", "get_param", PARAM_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "relaxation", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:rad\u22C5s/kg/m\u00B2 (rad/N/m/s) (inverse damping)"), "set_param", "get_param", PARAM_RELAXATION);

	BIND_ENUM_CONSTANT(PARAM_SWING_SPAN);
	BIND_ENUM_CONSTANT(PARAM_TWIST_SPAN);
	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_RELAXATION);
	BIND_ENUM_CONSTANT(PARAM_MAX);
}

void ConeTwistJoint3D::set_param(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(get_rid(), PhysicsServer3D::ConeTwistJointParam(p_param), p_value);
	}

	update_gizmos();
}

real_t ConeTwistJoint3D::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

void ConeTwistJoint3D::_configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) {
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

	PhysicsServer3D::get_singleton()->joint_make_cone_twist(p_joint, body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(p_joint, PhysicsServer3D::ConeTwistJointParam(i), params[i]);
	}
}

ConeTwistJoint3D::ConeTwistJoint3D() {
	params[PARAM_SWING_SPAN] = Math_PI * 0.25;
	params[PARAM_TWIST_SPAN] = Math_PI;
	params[PARAM_BIAS] = 0.3;
	params[PARAM_SOFTNESS] = 0.8;
	params[PARAM_RELAXATION] = 1.0;
}
