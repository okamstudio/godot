/**************************************************************************/
/*  pin_joint_3d.cpp                                                      */
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

#include "pin_joint_3d.h"

void PinJoint3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &PinJoint3D::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &PinJoint3D::get_param);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "params/bias", PROPERTY_HINT_RANGE, "0.01,0.99,0.01"), "set_param", "get_param", PARAM_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "params/damping", PROPERTY_HINT_RANGE, U"0.01,8.0,0.01,suffix:kg\u22C5m\u00B2/s/rad (N\u22C5s/m)"), "set_param", "get_param", PARAM_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "params/impulse_clamp", PROPERTY_HINT_RANGE, U"0.0,64.0,0.01,suffix:kg\u22C5m\u00B2/s/rad (N\u22C5s)"), "set_param", "get_param", PARAM_IMPULSE_CLAMP);

	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_IMPULSE_CLAMP);
}

void PinJoint3D::set_param(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, 3);
	params[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->pin_joint_set_param(get_rid(), PhysicsServer3D::PinJointParam(p_param), p_value);
	}
}

real_t PinJoint3D::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, 3, 0);
	return params[p_param];
}

void PinJoint3D::_configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) {
	Vector3 pinpos = get_global_transform().origin;
	Vector3 local_a = body_a->to_local(pinpos);
	Vector3 local_b;

	if (body_b) {
		local_b = body_b->to_local(pinpos);
	} else {
		local_b = pinpos;
	}

	PhysicsServer3D::get_singleton()->joint_make_pin(p_joint, body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < 3; i++) {
		PhysicsServer3D::get_singleton()->pin_joint_set_param(p_joint, PhysicsServer3D::PinJointParam(i), params[i]);
	}
}

PinJoint3D::PinJoint3D() {
	params[PARAM_BIAS] = 0.3;
	params[PARAM_DAMPING] = 1;
	params[PARAM_IMPULSE_CLAMP] = 0;
}
