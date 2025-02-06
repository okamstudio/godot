/**************************************************************************/
/*  generic_6dof_joint_3d.cpp                                             */
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

#include "generic_6dof_joint_3d.h"

#include "core/config/project_settings.h"

void Generic6DOFJoint3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param_x", "param", "value"), &Generic6DOFJoint3D::set_param_x);
	ClassDB::bind_method(D_METHOD("get_param_x", "param"), &Generic6DOFJoint3D::get_param_x);

	ClassDB::bind_method(D_METHOD("set_param_y", "param", "value"), &Generic6DOFJoint3D::set_param_y);
	ClassDB::bind_method(D_METHOD("get_param_y", "param"), &Generic6DOFJoint3D::get_param_y);

	ClassDB::bind_method(D_METHOD("set_param_z", "param", "value"), &Generic6DOFJoint3D::set_param_z);
	ClassDB::bind_method(D_METHOD("get_param_z", "param"), &Generic6DOFJoint3D::get_param_z);

	ClassDB::bind_method(D_METHOD("set_flag_x", "flag", "value"), &Generic6DOFJoint3D::set_flag_x);
	ClassDB::bind_method(D_METHOD("get_flag_x", "flag"), &Generic6DOFJoint3D::get_flag_x);

	ClassDB::bind_method(D_METHOD("set_flag_y", "flag", "value"), &Generic6DOFJoint3D::set_flag_y);
	ClassDB::bind_method(D_METHOD("get_flag_y", "flag"), &Generic6DOFJoint3D::get_flag_y);

	ClassDB::bind_method(D_METHOD("set_flag_z", "flag", "value"), &Generic6DOFJoint3D::set_flag_z);
	ClassDB::bind_method(D_METHOD("get_flag_z", "flag"), &Generic6DOFJoint3D::get_flag_z);

	ADD_GROUP("Linear Limit", "linear_limit_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_x/upper_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_x", "get_param_x", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_x/lower_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_x", "get_param_x", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_x/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_x/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_x/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_DAMPING);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_y/upper_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_y", "get_param_y", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_y/lower_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_y", "get_param_y", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_y/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_y/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_y/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_DAMPING);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_z/upper_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_z", "get_param_z", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_z/lower_distance", PROPERTY_HINT_NONE, "suffix:m"), "set_param_z", "get_param_z", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_z/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_z/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_limit_z/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_DAMPING);

	ADD_GROUP("Linear Motor", "linear_motor_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_x/target_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_param_x", "get_param_x", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_x/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m/s\u00B2 (N)"), "set_param_x", "get_param_x", PARAM_LINEAR_MOTOR_FORCE_LIMIT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_y/target_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_param_y", "get_param_y", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_y/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m/s\u00B2 (N)"), "set_param_y", "get_param_y", PARAM_LINEAR_MOTOR_FORCE_LIMIT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_z/target_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_param_z", "get_param_z", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_motor_z/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m/s\u00B2 (N)"), "set_param_z", "get_param_z", PARAM_LINEAR_MOTOR_FORCE_LIMIT);

	ADD_GROUP("Linear Spring", "linear_spring_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_x/stiffness"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_x/damping"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_x/equilibrium_point", PROPERTY_HINT_NONE, "suffix:m"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_y/stiffness"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_y/damping"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_y/equilibrium_point", PROPERTY_HINT_NONE, "suffix:m"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_z/stiffness"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_z/damping"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_spring_z/equilibrium_point", PROPERTY_HINT_NONE, "suffix:m"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);

	ADD_GROUP("Angular Limit", "angular_limit_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_x", "get_param_x", PARAM_ANGULAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_x", "get_param_x", PARAM_ANGULAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_x", "get_param_x", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_x/erp"), "set_param_x", "get_param_x", PARAM_ANGULAR_ERP);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_y", "get_param_y", PARAM_ANGULAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_y", "get_param_y", PARAM_ANGULAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_y", "get_param_y", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_y/erp"), "set_param_y", "get_param_y", PARAM_ANGULAR_ERP);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_z", "get_param_z", PARAM_ANGULAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_z", "get_param_z", PARAM_ANGULAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_z", "get_param_z", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_limit_z/erp"), "set_param_z", "get_param_z", PARAM_ANGULAR_ERP);

	ADD_GROUP("Angular Motor", "angular_motor_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_x/target_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_param_x", "get_param_x", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_x/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_x", "get_param_x", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_y/target_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_param_y", "get_param_y", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_y/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_y", "get_param_y", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_z/target_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_param_z", "get_param_z", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_motor_z/force_limit", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2 (Nm)"), "set_param_z", "get_param_z", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);

	ADD_GROUP("Angular Spring", "angular_spring_");

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_x/stiffness"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_x/damping"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_x/equilibrium_point", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_y/stiffness"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_y/damping"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_y/equilibrium_point", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_z/stiffness"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_z/damping"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_spring_z/equilibrium_point", PROPERTY_HINT_RANGE, "-180,180,0.01,radians_as_degrees"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	BIND_ENUM_CONSTANT(PARAM_LINEAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ERP);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_ANGULAR_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_SPRING);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_ANGULAR_SPRING);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_MAX);
}

PackedStringArray Generic6DOFJoint3D::get_configuration_warnings() const {
	PackedStringArray warnings = Joint3D::get_configuration_warnings();

	const String physics_engine = GLOBAL_GET("physics/3d/physics_engine");
	const bool using_godot_physics = physics_engine == "DEFAULT" || physics_engine == "GodotPhysics3D";

	if (using_godot_physics) {
		if (get_flag_x(FLAG_ENABLE_LINEAR_MOTOR) || get_flag_y(FLAG_ENABLE_LINEAR_MOTOR) || get_flag_z(FLAG_ENABLE_LINEAR_MOTOR)) {
			warnings.push_back(RTR("Linear motors are not supported in GodotPhysics3D.\nTo use linear motors in Generic6DOFJoint3D, install a third-party physics engine that supports linear motors and switch to it in the project settings."));
		}

		if (get_flag_x(FLAG_ENABLE_LINEAR_SPRING) || get_flag_y(FLAG_ENABLE_LINEAR_SPRING) || get_flag_z(FLAG_ENABLE_LINEAR_SPRING)) {
			warnings.push_back(RTR("Linear springs are not supported in GodotPhysics3D.\nTo use linear springs in Generic6DOFJoint3D, install a third-party physics engine that supports linear springs and switch to it in the project settings."));
		}

		if (get_flag_x(FLAG_ENABLE_ANGULAR_SPRING) || get_flag_y(FLAG_ENABLE_ANGULAR_SPRING) || get_flag_z(FLAG_ENABLE_ANGULAR_SPRING)) {
			warnings.push_back(RTR("Angular springs are not supported in GodotPhysics3D.\nTo use angular springs in Generic6DOFJoint3D, install a third-party physics engine that supports angular springs and switch to it in the project settings."));
		}
	}

	return warnings;
}

void Generic6DOFJoint3D::set_param_x(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_x[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(get_rid(), Vector3::AXIS_X, PhysicsServer3D::G6DOFJointAxisParam(p_param), p_value);
	}

	update_gizmos();
}

real_t Generic6DOFJoint3D::get_param_x(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_x[p_param];
}

void Generic6DOFJoint3D::set_param_y(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_y[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(get_rid(), Vector3::AXIS_Y, PhysicsServer3D::G6DOFJointAxisParam(p_param), p_value);
	}
	update_gizmos();
}

real_t Generic6DOFJoint3D::get_param_y(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_y[p_param];
}

void Generic6DOFJoint3D::set_param_z(Param p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_z[p_param] = p_value;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(get_rid(), Vector3::AXIS_Z, PhysicsServer3D::G6DOFJointAxisParam(p_param), p_value);
	}
	update_gizmos();
}

real_t Generic6DOFJoint3D::get_param_z(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_z[p_param];
}

void Generic6DOFJoint3D::set_flag_x(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_x[p_flag] = p_enabled;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(get_rid(), Vector3::AXIS_X, PhysicsServer3D::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_configuration_warnings();
	update_gizmos();
}

bool Generic6DOFJoint3D::get_flag_x(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_x[p_flag];
}

void Generic6DOFJoint3D::set_flag_y(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_y[p_flag] = p_enabled;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(get_rid(), Vector3::AXIS_Y, PhysicsServer3D::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_configuration_warnings();
	update_gizmos();
}

bool Generic6DOFJoint3D::get_flag_y(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_y[p_flag];
}

void Generic6DOFJoint3D::set_flag_z(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_z[p_flag] = p_enabled;
	if (is_configured()) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(get_rid(), Vector3::AXIS_Z, PhysicsServer3D::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_configuration_warnings();
	update_gizmos();
}

bool Generic6DOFJoint3D::get_flag_z(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_z[p_flag];
}

void Generic6DOFJoint3D::_configure_joint(RID p_joint, PhysicsBody3D *body_a, PhysicsBody3D *body_b) {
	Transform3D gt = get_global_transform();
	//Vector3 cone_twistpos = gt.origin;
	//Vector3 cone_twistdir = gt.basis.get_axis(2);

	Transform3D ainv = body_a->get_global_transform().affine_inverse();

	Transform3D local_a = ainv * gt;
	local_a.orthonormalize();
	Transform3D local_b = gt;

	if (body_b) {
		Transform3D binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	PhysicsServer3D::get_singleton()->joint_make_generic_6dof(p_joint, body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(p_joint, Vector3::AXIS_X, PhysicsServer3D::G6DOFJointAxisParam(i), params_x[i]);
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(p_joint, Vector3::AXIS_Y, PhysicsServer3D::G6DOFJointAxisParam(i), params_y[i]);
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(p_joint, Vector3::AXIS_Z, PhysicsServer3D::G6DOFJointAxisParam(i), params_z[i]);
	}
	for (int i = 0; i < FLAG_MAX; i++) {
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(p_joint, Vector3::AXIS_X, PhysicsServer3D::G6DOFJointAxisFlag(i), flags_x[i]);
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(p_joint, Vector3::AXIS_Y, PhysicsServer3D::G6DOFJointAxisFlag(i), flags_y[i]);
		PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(p_joint, Vector3::AXIS_Z, PhysicsServer3D::G6DOFJointAxisFlag(i), flags_z[i]);
	}
}

Generic6DOFJoint3D::Generic6DOFJoint3D() {
	set_param_x(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_x(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_x(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_x(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_x(PARAM_LINEAR_DAMPING, 1.0);
	set_param_x(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_x(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_x(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_x(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_x(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_x(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_x(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_x(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_x(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_ERP, 0.5);
	set_param_x(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_x(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_x(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_x(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_x(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_x(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_x(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_x(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_x(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_x(FLAG_ENABLE_MOTOR, false);
	set_flag_x(FLAG_ENABLE_LINEAR_MOTOR, false);

	set_param_y(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_y(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_y(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_y(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_y(PARAM_LINEAR_DAMPING, 1.0);
	set_param_y(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_y(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_y(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_y(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_y(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_y(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_y(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_y(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_y(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_ERP, 0.5);
	set_param_y(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_y(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_y(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_y(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_y(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_y(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_y(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_y(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_y(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_y(FLAG_ENABLE_MOTOR, false);
	set_flag_y(FLAG_ENABLE_LINEAR_MOTOR, false);

	set_param_z(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_z(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_z(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_z(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_z(PARAM_LINEAR_DAMPING, 1.0);
	set_param_z(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_z(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_z(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_z(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_z(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_z(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_z(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_z(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_z(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_ERP, 0.5);
	set_param_z(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_z(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_z(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_z(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_z(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_z(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_z(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_z(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_z(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_z(FLAG_ENABLE_MOTOR, false);
	set_flag_z(FLAG_ENABLE_LINEAR_MOTOR, false);
}
