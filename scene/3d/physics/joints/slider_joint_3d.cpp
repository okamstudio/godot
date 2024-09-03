/**************************************************************************/
/*  slider_joint_3d.cpp                                                   */
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

#include "slider_joint_3d.h"

void SliderJoint3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &SliderJoint3D::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &SliderJoint3D::get_param);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit/upper_distance", PROPERTY_HINT_RANGE, "-1024,1024,0.01,suffix:m"), "set_param", "get_param", PARAM_LINEAR_LIMIT_UPPER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit/lower_distance", PROPERTY_HINT_RANGE, "-1024,1024,0.01,suffix:m"), "set_param", "get_param", PARAM_LINEAR_LIMIT_LOWER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg/s\u00B2 (N/m)"), "set_param", "get_param", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg/s (N\u22C5s/m)"), "set_param", "get_param", PARAM_LINEAR_LIMIT_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motion/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg/s\u00B2 (N/m)"), "set_param", "get_param", PARAM_LINEAR_MOTION_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motion/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_MOTION_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motion/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg/s (N\u22C5s/m)"), "set_param", "get_param", PARAM_LINEAR_MOTION_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_ortho/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg/s\u00B2 (N/m)"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_ortho/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_ortho/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg/s (N\u22C5s/m)"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_DAMPING);

	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_UPPER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.1,radians_as_degrees"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_LOWER);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg\u22C5m\u00B2/s\u00B2/rad (N\u22C5m/rad)"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg\u22C5m\u00B2/s/rad (N\u22C5m\u22C5s/rad)"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motion/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg\u22C5m\u00B2/s\u00B2/rad (N\u22C5m/rad)"), "set_param", "get_param", PARAM_ANGULAR_MOTION_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motion/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_MOTION_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motion/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg\u22C5m\u00B2/s/rad (N\u22C5m\u22C5s/rad)"), "set_param", "get_param", PARAM_ANGULAR_MOTION_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_ortho/softness", PROPERTY_HINT_RANGE, U"0.01,16.0,0.01,suffix:kg\u22C5m\u00B2/s\u00B2/rad (N\u22C5m/rad)"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_ortho/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_ortho/damping", PROPERTY_HINT_RANGE, U"0,16.0,0.01,suffix:kg\u22C5m\u00B2/s/rad (N\u22C5m\u22C5s/rad)"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_MAX);
}

void SliderJoint3D::set_param(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->slider_joint_set_param(get_rid(), PhysicsServer3D::SliderJointParam(p_param), p_value);
	}
	update_gizmos();
}

real_t SliderJoint3D::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

void SliderJoint3D::_configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) {
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

	PhysicsServer3D::get_singleton()->joint_make_slider(p_joint, body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer3D::get_singleton()->slider_joint_set_param(p_joint, PhysicsServer3D::SliderJointParam(i), params[i]);
	}
}

SliderJoint3D::SliderJoint3D() {
	params[PARAM_LINEAR_LIMIT_UPPER] = 1.0;
	params[PARAM_LINEAR_LIMIT_LOWER] = -1.0;
	params[PARAM_LINEAR_LIMIT_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_LIMIT_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_LIMIT_DAMPING] = 1.0;
	params[PARAM_LINEAR_MOTION_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_MOTION_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_MOTION_DAMPING] = 0; //1.0;
	params[PARAM_LINEAR_ORTHOGONAL_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_ORTHOGONAL_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_ORTHOGONAL_DAMPING] = 1.0;

	params[PARAM_ANGULAR_LIMIT_UPPER] = 0;
	params[PARAM_ANGULAR_LIMIT_LOWER] = 0;
	params[PARAM_ANGULAR_LIMIT_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_LIMIT_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_LIMIT_DAMPING] = 0; //1.0;
	params[PARAM_ANGULAR_MOTION_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_MOTION_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_MOTION_DAMPING] = 1.0;
	params[PARAM_ANGULAR_ORTHOGONAL_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_ORTHOGONAL_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_ORTHOGONAL_DAMPING] = 1.0;
}
