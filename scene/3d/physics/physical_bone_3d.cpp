/**************************************************************************/
/*  physical_bone_3d.cpp                                                  */
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

#include "physical_bone_3d.h"

#include "scene/3d/physical_bone_simulator_3d.h"
#ifndef DISABLE_DEPRECATED
#include "scene/3d/skeleton_3d.h"
#endif //_DISABLE_DEPRECATED

bool PhysicalBone3D::JointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	return false;
}

bool PhysicalBone3D::JointData::_get(const StringName &p_name, Variant &r_ret) const {
	return false;
}

void PhysicalBone3D::JointData::_get_property_list(List<PropertyInfo> *p_list) const {
}

void PhysicalBone3D::apply_central_impulse(const Vector3 &p_impulse) {
	PhysicsServer3D::get_singleton()->body_apply_central_impulse(get_rid(), p_impulse);
}

void PhysicalBone3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	PhysicsServer3D::get_singleton()->body_apply_impulse(get_rid(), p_impulse, p_position);
}

void PhysicalBone3D::set_linear_velocity(const Vector3 &p_velocity) {
	linear_velocity = p_velocity;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, linear_velocity);
}

Vector3 PhysicalBone3D::get_linear_velocity() const {
	return linear_velocity;
}

void PhysicalBone3D::set_angular_velocity(const Vector3 &p_velocity) {
	angular_velocity = p_velocity;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, angular_velocity);
}

Vector3 PhysicalBone3D::get_angular_velocity() const {
	return angular_velocity;
}

void PhysicalBone3D::set_use_custom_integrator(bool p_enable) {
	if (custom_integrator == p_enable) {
		return;
	}

	custom_integrator = p_enable;
	PhysicsServer3D::get_singleton()->body_set_omit_force_integration(get_rid(), p_enable);
}

bool PhysicalBone3D::is_using_custom_integrator() {
	return custom_integrator;
}

void PhysicalBone3D::reset_physics_simulation_state() {
	if (simulate_physics) {
		_start_physics_simulation();
	} else {
		_stop_physics_simulation();
	}
}

void PhysicalBone3D::reset_to_rest_position() {
	PhysicalBoneSimulator3D *simulator = get_simulator();
	Skeleton3D *skeleton = get_skeleton();
	if (simulator && skeleton) {
		if (bone_id == -1) {
			set_global_transform((skeleton->get_global_transform() * body_offset).orthonormalized());
		} else {
			set_global_transform((skeleton->get_global_transform() * simulator->get_bone_global_pose(bone_id) * body_offset).orthonormalized());
		}
	}
}

bool PhysicalBone3D::PinJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	if (JointData::_set(p_name, p_value, j)) {
		return true;
	}

	bool is_valid_pin = j.is_valid() && PhysicsServer3D::get_singleton()->joint_get_type(j) == PhysicsServer3D::JOINT_TYPE_PIN;
	if ("joint_constraints/bias" == p_name) {
		bias = p_value;
		if (is_valid_pin) {
			PhysicsServer3D::get_singleton()->pin_joint_set_param(j, PhysicsServer3D::PIN_JOINT_BIAS, bias);
		}

	} else if ("joint_constraints/damping" == p_name) {
		damping = p_value;
		if (is_valid_pin) {
			PhysicsServer3D::get_singleton()->pin_joint_set_param(j, PhysicsServer3D::PIN_JOINT_DAMPING, damping);
		}

	} else if ("joint_constraints/impulse_clamp" == p_name) {
		impulse_clamp = p_value;
		if (is_valid_pin) {
			PhysicsServer3D::get_singleton()->pin_joint_set_param(j, PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP, impulse_clamp);
		}

	} else {
		return false;
	}

	return true;
}

bool PhysicalBone3D::PinJointData::_get(const StringName &p_name, Variant &r_ret) const {
	if (JointData::_get(p_name, r_ret)) {
		return true;
	}

	if ("joint_constraints/bias" == p_name) {
		r_ret = bias;
	} else if ("joint_constraints/damping" == p_name) {
		r_ret = damping;
	} else if ("joint_constraints/impulse_clamp" == p_name) {
		r_ret = impulse_clamp;
	} else {
		return false;
	}

	return true;
}

void PhysicalBone3D::PinJointData::_get_property_list(List<PropertyInfo> *p_list) const {
	JointData::_get_property_list(p_list);

	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/bias"), PROPERTY_HINT_RANGE, "0.01,0.99,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/damping"), PROPERTY_HINT_RANGE, "0.01,8.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/impulse_clamp"), PROPERTY_HINT_RANGE, "0.0,64.0,0.01"));
}

bool PhysicalBone3D::ConeJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	if (JointData::_set(p_name, p_value, j)) {
		return true;
	}

	bool is_valid_cone = j.is_valid() && PhysicsServer3D::get_singleton()->joint_get_type(j) == PhysicsServer3D::JOINT_TYPE_CONE_TWIST;
	if ("joint_constraints/swing_span" == p_name) {
		swing_span = Math::deg_to_rad(real_t(p_value));
		if (is_valid_cone) {
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN, swing_span);
		}

	} else if ("joint_constraints/twist_span" == p_name) {
		twist_span = Math::deg_to_rad(real_t(p_value));
		if (is_valid_cone) {
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN, twist_span);
		}

	} else if ("joint_constraints/bias" == p_name) {
		bias = p_value;
		if (is_valid_cone) {
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer3D::CONE_TWIST_JOINT_BIAS, bias);
		}

	} else if ("joint_constraints/softness" == p_name) {
		softness = p_value;
		if (is_valid_cone) {
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS, softness);
		}

	} else if ("joint_constraints/relaxation" == p_name) {
		relaxation = p_value;
		if (is_valid_cone) {
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION, relaxation);
		}

	} else {
		return false;
	}

	return true;
}

bool PhysicalBone3D::ConeJointData::_get(const StringName &p_name, Variant &r_ret) const {
	if (JointData::_get(p_name, r_ret)) {
		return true;
	}

	if ("joint_constraints/swing_span" == p_name) {
		r_ret = Math::rad_to_deg(swing_span);
	} else if ("joint_constraints/twist_span" == p_name) {
		r_ret = Math::rad_to_deg(twist_span);
	} else if ("joint_constraints/bias" == p_name) {
		r_ret = bias;
	} else if ("joint_constraints/softness" == p_name) {
		r_ret = softness;
	} else if ("joint_constraints/relaxation" == p_name) {
		r_ret = relaxation;
	} else {
		return false;
	}

	return true;
}

void PhysicalBone3D::ConeJointData::_get_property_list(List<PropertyInfo> *p_list) const {
	JointData::_get_property_list(p_list);

	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/swing_span"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/twist_span"), PROPERTY_HINT_RANGE, "-40000,40000,0.1,or_less,or_greater"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/bias"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/softness"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/relaxation"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
}

bool PhysicalBone3D::HingeJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	if (JointData::_set(p_name, p_value, j)) {
		return true;
	}

	bool is_valid_hinge = j.is_valid() && PhysicsServer3D::get_singleton()->joint_get_type(j) == PhysicsServer3D::JOINT_TYPE_HINGE;
	if ("joint_constraints/angular_limit_enabled" == p_name) {
		angular_limit_enabled = p_value;
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_flag(j, PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT, angular_limit_enabled);
		}

	} else if ("joint_constraints/angular_limit_upper" == p_name) {
		angular_limit_upper = Math::deg_to_rad(real_t(p_value));
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(j, PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER, angular_limit_upper);
		}

	} else if ("joint_constraints/angular_limit_lower" == p_name) {
		angular_limit_lower = Math::deg_to_rad(real_t(p_value));
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(j, PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER, angular_limit_lower);
		}

	} else if ("joint_constraints/angular_limit_bias" == p_name) {
		angular_limit_bias = p_value;
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(j, PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS, angular_limit_bias);
		}

	} else if ("joint_constraints/angular_limit_softness" == p_name) {
		angular_limit_softness = p_value;
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(j, PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS, angular_limit_softness);
		}

	} else if ("joint_constraints/angular_limit_relaxation" == p_name) {
		angular_limit_relaxation = p_value;
		if (is_valid_hinge) {
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(j, PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION, angular_limit_relaxation);
		}

	} else {
		return false;
	}

	return true;
}

bool PhysicalBone3D::HingeJointData::_get(const StringName &p_name, Variant &r_ret) const {
	if (JointData::_get(p_name, r_ret)) {
		return true;
	}

	if ("joint_constraints/angular_limit_enabled" == p_name) {
		r_ret = angular_limit_enabled;
	} else if ("joint_constraints/angular_limit_upper" == p_name) {
		r_ret = Math::rad_to_deg(angular_limit_upper);
	} else if ("joint_constraints/angular_limit_lower" == p_name) {
		r_ret = Math::rad_to_deg(angular_limit_lower);
	} else if ("joint_constraints/angular_limit_bias" == p_name) {
		r_ret = angular_limit_bias;
	} else if ("joint_constraints/angular_limit_softness" == p_name) {
		r_ret = angular_limit_softness;
	} else if ("joint_constraints/angular_limit_relaxation" == p_name) {
		r_ret = angular_limit_relaxation;
	} else {
		return false;
	}

	return true;
}

void PhysicalBone3D::HingeJointData::_get_property_list(List<PropertyInfo> *p_list) const {
	JointData::_get_property_list(p_list);

	p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("joint_constraints/angular_limit_enabled")));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_upper"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_lower"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_bias"), PROPERTY_HINT_RANGE, "0.01,0.99,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_softness"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_relaxation"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
}

bool PhysicalBone3D::SliderJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	if (JointData::_set(p_name, p_value, j)) {
		return true;
	}

	bool is_valid_slider = j.is_valid() && PhysicsServer3D::get_singleton()->joint_get_type(j) == PhysicsServer3D::JOINT_TYPE_SLIDER;
	if ("joint_constraints/linear_limit_upper" == p_name) {
		linear_limit_upper = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER, linear_limit_upper);
		}

	} else if ("joint_constraints/linear_limit_lower" == p_name) {
		linear_limit_lower = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER, linear_limit_lower);
		}

	} else if ("joint_constraints/linear_limit_softness" == p_name) {
		linear_limit_softness = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS, linear_limit_softness);
		}

	} else if ("joint_constraints/linear_limit_restitution" == p_name) {
		linear_limit_restitution = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION, linear_limit_restitution);
		}

	} else if ("joint_constraints/linear_limit_damping" == p_name) {
		linear_limit_damping = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING, linear_limit_restitution);
		}

	} else if ("joint_constraints/angular_limit_upper" == p_name) {
		angular_limit_upper = Math::deg_to_rad(real_t(p_value));
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER, angular_limit_upper);
		}

	} else if ("joint_constraints/angular_limit_lower" == p_name) {
		angular_limit_lower = Math::deg_to_rad(real_t(p_value));
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER, angular_limit_lower);
		}

	} else if ("joint_constraints/angular_limit_softness" == p_name) {
		angular_limit_softness = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, angular_limit_softness);
		}

	} else if ("joint_constraints/angular_limit_restitution" == p_name) {
		angular_limit_restitution = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, angular_limit_softness);
		}

	} else if ("joint_constraints/angular_limit_damping" == p_name) {
		angular_limit_damping = p_value;
		if (is_valid_slider) {
			PhysicsServer3D::get_singleton()->slider_joint_set_param(j, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING, angular_limit_damping);
		}

	} else {
		return false;
	}

	return true;
}

bool PhysicalBone3D::SliderJointData::_get(const StringName &p_name, Variant &r_ret) const {
	if (JointData::_get(p_name, r_ret)) {
		return true;
	}

	if ("joint_constraints/linear_limit_upper" == p_name) {
		r_ret = linear_limit_upper;
	} else if ("joint_constraints/linear_limit_lower" == p_name) {
		r_ret = linear_limit_lower;
	} else if ("joint_constraints/linear_limit_softness" == p_name) {
		r_ret = linear_limit_softness;
	} else if ("joint_constraints/linear_limit_restitution" == p_name) {
		r_ret = linear_limit_restitution;
	} else if ("joint_constraints/linear_limit_damping" == p_name) {
		r_ret = linear_limit_damping;
	} else if ("joint_constraints/angular_limit_upper" == p_name) {
		r_ret = Math::rad_to_deg(angular_limit_upper);
	} else if ("joint_constraints/angular_limit_lower" == p_name) {
		r_ret = Math::rad_to_deg(angular_limit_lower);
	} else if ("joint_constraints/angular_limit_softness" == p_name) {
		r_ret = angular_limit_softness;
	} else if ("joint_constraints/angular_limit_restitution" == p_name) {
		r_ret = angular_limit_restitution;
	} else if ("joint_constraints/angular_limit_damping" == p_name) {
		r_ret = angular_limit_damping;
	} else {
		return false;
	}

	return true;
}

void PhysicalBone3D::SliderJointData::_get_property_list(List<PropertyInfo> *p_list) const {
	JointData::_get_property_list(p_list);

	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/linear_limit_upper")));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/linear_limit_lower")));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/linear_limit_softness"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/linear_limit_restitution"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/linear_limit_damping"), PROPERTY_HINT_RANGE, "0,16.0,0.01"));

	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_upper"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_lower"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_softness"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_restitution"), PROPERTY_HINT_RANGE, "0.01,16.0,0.01"));
	p_list->push_back(PropertyInfo(Variant::FLOAT, PNAME("joint_constraints/angular_limit_damping"), PROPERTY_HINT_RANGE, "0,16.0,0.01"));
}

bool PhysicalBone3D::SixDOFJointData::_set(const StringName &p_name, const Variant &p_value, RID j) {
	if (JointData::_set(p_name, p_value, j)) {
		return true;
	}

	String path = p_name;

	if (!path.begins_with("joint_constraints/")) {
		return false;
	}

	Vector3::Axis axis;
	{
		const String axis_s = path.get_slicec('/', 1);
		if ("x" == axis_s) {
			axis = Vector3::AXIS_X;
		} else if ("y" == axis_s) {
			axis = Vector3::AXIS_Y;
		} else if ("z" == axis_s) {
			axis = Vector3::AXIS_Z;
		} else {
			return false;
		}
	}

	String var_name = path.get_slicec('/', 2);
	bool is_valid_6dof = j.is_valid() && PhysicsServer3D::get_singleton()->joint_get_type(j) == PhysicsServer3D::JOINT_TYPE_6DOF;
	if ("linear_limit_enabled" == var_name) {
		axis_data[axis].linear_limit_enabled = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis, PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT, axis_data[axis].linear_limit_enabled);
		}

	} else if ("linear_limit_upper" == var_name) {
		axis_data[axis].linear_limit_upper = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT, axis_data[axis].linear_limit_upper);
		}

	} else if ("linear_limit_lower" == var_name) {
		axis_data[axis].linear_limit_lower = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT, axis_data[axis].linear_limit_lower);
		}

	} else if ("linear_limit_softness" == var_name) {
		axis_data[axis].linear_limit_softness = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS, axis_data[axis].linear_limit_softness);
		}

	} else if ("linear_spring_enabled" == var_name) {
		axis_data[axis].linear_spring_enabled = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis, PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING, axis_data[axis].linear_spring_enabled);
		}

	} else if ("linear_spring_stiffness" == var_name) {
		axis_data[axis].linear_spring_stiffness = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS, axis_data[axis].linear_spring_stiffness);
		}

	} else if ("linear_spring_damping" == var_name) {
		axis_data[axis].linear_spring_damping = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING, axis_data[axis].linear_spring_damping);
		}

	} else if ("linear_equilibrium_point" == var_name) {
		axis_data[axis].linear_equilibrium_point = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT, axis_data[axis].linear_equilibrium_point);
		}

	} else if ("linear_restitution" == var_name) {
		axis_data[axis].linear_restitution = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION, axis_data[axis].linear_restitution);
		}

	} else if ("linear_damping" == var_name) {
		axis_data[axis].linear_damping = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING, axis_data[axis].linear_damping);
		}

	} else if ("angular_limit_enabled" == var_name) {
		axis_data[axis].angular_limit_enabled = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis, PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT, axis_data[axis].angular_limit_enabled);
		}

	} else if ("angular_limit_upper" == var_name) {
		axis_data[axis].angular_limit_upper = Math::deg_to_rad(real_t(p_value));
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT, axis_data[axis].angular_limit_upper);
		}

	} else if ("angular_limit_lower" == var_name) {
		axis_data[axis].angular_limit_lower = Math::deg_to_rad(real_t(p_value));
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT, axis_data[axis].angular_limit_lower);
		}

	} else if ("angular_limit_softness" == var_name) {
		axis_data[axis].angular_limit_softness = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS, axis_data[axis].angular_limit_softness);
		}

	} else if ("angular_restitution" == var_name) {
		axis_data[axis].angular_restitution = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION, axis_data[axis].angular_restitution);
		}

	} else if ("angular_damping" == var_name) {
		axis_data[axis].angular_damping = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING, axis_data[axis].angular_damping);
		}

	} else if ("erp" == var_name) {
		axis_data[axis].erp = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP, axis_data[axis].erp);
		}

	} else if ("angular_spring_enabled" == var_name) {
		axis_data[axis].angular_spring_enabled = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(j, axis, PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING, axis_data[axis].angular_spring_enabled);
		}

	} else if ("angular_spring_stiffness" == var_name) {
		axis_data[axis].angular_spring_stiffness = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS, axis_data[axis].angular_spring_stiffness);
		}

	} else if ("angular_spring_damping" == var_name) {
		axis_data[axis].angular_spring_damping = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING, axis_data[axis].angular_spring_damping);
		}

	} else if ("angular_equilibrium_point" == var_name) {
		axis_data[axis].angular_equilibrium_point = p_value;
		if (is_valid_6dof) {
			PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(j, axis, PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT, axis_data[axis].angular_equilibrium_point);
		}

	} else {
		return false;
	}

	return true;
}

bool PhysicalBone3D::SixDOFJointData::_get(const StringName &p_name, Variant &r_ret) const {
	if (JointData::_get(p_name, r_ret)) {
		return true;
	}

	String path = p_name;

	if (!path.begins_with("joint_constraints/")) {
		return false;
	}

	int axis;
	{
		const String axis_s = path.get_slicec('/', 1);
		if ("x" == axis_s) {
			axis = 0;
		} else if ("y" == axis_s) {
			axis = 1;
		} else if ("z" == axis_s) {
			axis = 2;
		} else {
			return false;
		}
	}

	String var_name = path.get_slicec('/', 2);

	if ("linear_limit_enabled" == var_name) {
		r_ret = axis_data[axis].linear_limit_enabled;
	} else if ("linear_limit_upper" == var_name) {
		r_ret = axis_data[axis].linear_limit_upper;
	} else if ("linear_limit_lower" == var_name) {
		r_ret = axis_data[axis].linear_limit_lower;
	} else if ("linear_limit_softness" == var_name) {
		r_ret = axis_data[axis].linear_limit_softness;
	} else if ("linear_spring_enabled" == var_name) {
		r_ret = axis_data[axis].linear_spring_enabled;
	} else if ("linear_spring_stiffness" == var_name) {
		r_ret = axis_data[axis].linear_spring_stiffness;
	} else if ("linear_spring_damping" == var_name) {
		r_ret = axis_data[axis].linear_spring_damping;
	} else if ("linear_equilibrium_point" == var_name) {
		r_ret = axis_data[axis].linear_equilibrium_point;
	} else if ("linear_restitution" == var_name) {
		r_ret = axis_data[axis].linear_restitution;
	} else if ("linear_damping" == var_name) {
		r_ret = axis_data[axis].linear_damping;
	} else if ("angular_limit_enabled" == var_name) {
		r_ret = axis_data[axis].angular_limit_enabled;
	} else if ("angular_limit_upper" == var_name) {
		r_ret = Math::rad_to_deg(axis_data[axis].angular_limit_upper);
	} else if ("angular_limit_lower" == var_name) {
		r_ret = Math::rad_to_deg(axis_data[axis].angular_limit_lower);
	} else if ("angular_limit_softness" == var_name) {
		r_ret = axis_data[axis].angular_limit_softness;
	} else if ("angular_restitution" == var_name) {
		r_ret = axis_data[axis].angular_restitution;
	} else if ("angular_damping" == var_name) {
		r_ret = axis_data[axis].angular_damping;
	} else if ("erp" == var_name) {
		r_ret = axis_data[axis].erp;
	} else if ("angular_spring_enabled" == var_name) {
		r_ret = axis_data[axis].angular_spring_enabled;
	} else if ("angular_spring_stiffness" == var_name) {
		r_ret = axis_data[axis].angular_spring_stiffness;
	} else if ("angular_spring_damping" == var_name) {
		r_ret = axis_data[axis].angular_spring_damping;
	} else if ("angular_equilibrium_point" == var_name) {
		r_ret = axis_data[axis].angular_equilibrium_point;
	} else {
		return false;
	}

	return true;
}

void PhysicalBone3D::SixDOFJointData::_get_property_list(List<PropertyInfo> *p_list) const {
	const StringName axis_names[] = { PNAME("x"), PNAME("y"), PNAME("z") };
	for (int i = 0; i < 3; ++i) {
		const String prefix = vformat("%s/%s/", PNAME("joint_constraints"), axis_names[i]);
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + PNAME("linear_limit_enabled")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_limit_upper")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_limit_lower")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_limit_softness"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + PNAME("linear_spring_enabled")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_spring_stiffness")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_spring_damping")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_equilibrium_point")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_restitution"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("linear_damping"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + PNAME("angular_limit_enabled")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_limit_upper"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_limit_lower"), PROPERTY_HINT_RANGE, "-180,180,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_limit_softness"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_restitution"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_damping"), PROPERTY_HINT_RANGE, "0.01,16,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("erp")));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + PNAME("angular_spring_enabled")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_spring_stiffness")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_spring_damping")));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + PNAME("angular_equilibrium_point")));
	}
}

bool PhysicalBone3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "bone_name") {
		set_bone_name(p_value);
		return true;
	}

	if (joint_data) {
		if (joint_data->_set(p_name, p_value, joint)) {
#ifdef TOOLS_ENABLED
			update_gizmos();
#endif
			return true;
		}
	}

	return false;
}

bool PhysicalBone3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "bone_name") {
		r_ret = get_bone_name();
		return true;
	}

	if (joint_data) {
		return joint_data->_get(p_name, r_ret);
	}

	return false;
}

void PhysicalBone3D::_get_property_list(List<PropertyInfo> *p_list) const {
	Skeleton3D *skeleton = get_skeleton();
	if (skeleton) {
		p_list->push_back(PropertyInfo(Variant::STRING_NAME, PNAME("bone_name"), PROPERTY_HINT_ENUM, skeleton->get_concatenated_bone_names()));
	} else {
		p_list->push_back(PropertyInfo(Variant::STRING_NAME, PNAME("bone_name")));
	}

	if (joint_data) {
		joint_data->_get_property_list(p_list);
	}
}

void PhysicalBone3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_PARENTED:
			_update_simulator_path();
			update_bone_id();
			reset_to_rest_position();
			reset_physics_simulation_state();
			if (joint_data) {
				_reload_joint();
			}
			break;

		case NOTIFICATION_EXIT_TREE: {
			PhysicalBoneSimulator3D *simulator = get_simulator();
			if (simulator) {
				if (bone_id != -1) {
					simulator->unbind_physical_bone_from_bone(bone_id);
					bone_id = -1;
				}
			}
			PhysicsServer3D::get_singleton()->joint_clear(joint);
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (Engine::get_singleton()->is_editor_hint()) {
				update_offset();
			}
		} break;
	}
}

void PhysicalBone3D::_sync_body_state(PhysicsDirectBodyState3D *p_state) {
	set_ignore_transform_notification(true);
	set_global_transform(p_state->get_transform());
	set_ignore_transform_notification(false);

	linear_velocity = p_state->get_linear_velocity();
	angular_velocity = p_state->get_angular_velocity();
}

void PhysicalBone3D::_body_state_changed(PhysicsDirectBodyState3D *p_state) {
	if (!simulate_physics || !_internal_simulate_physics) {
		return;
	}

	if (GDVIRTUAL_IS_OVERRIDDEN(_integrate_forces)) {
		_sync_body_state(p_state);

		Transform3D old_transform = get_global_transform();
		GDVIRTUAL_CALL(_integrate_forces, p_state);
		Transform3D new_transform = get_global_transform();

		if (new_transform != old_transform) {
			// Update the physics server with the new transform, to prevent it from being overwritten at the sync below.
			PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_TRANSFORM, new_transform);
		}
	}

	_sync_body_state(p_state);
	_on_transform_changed();

	Transform3D global_transform(p_state->get_transform());

	// Update simulator
	PhysicalBoneSimulator3D *simulator = get_simulator();
	Skeleton3D *skeleton = get_skeleton();
	if (simulator && skeleton) {
		if (bone_id != -1) {
			simulator->set_bone_global_pose(bone_id, skeleton->get_global_transform().affine_inverse() * (global_transform * body_offset_inverse));
		}
	}
}

void PhysicalBone3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_central_impulse", "impulse"), &PhysicalBone3D::apply_central_impulse);
	ClassDB::bind_method(D_METHOD("apply_impulse", "impulse", "position"), &PhysicalBone3D::apply_impulse, Vector3());

	ClassDB::bind_method(D_METHOD("set_joint_type", "joint_type"), &PhysicalBone3D::set_joint_type);
	ClassDB::bind_method(D_METHOD("get_joint_type"), &PhysicalBone3D::get_joint_type);

	ClassDB::bind_method(D_METHOD("set_joint_offset", "offset"), &PhysicalBone3D::set_joint_offset);
	ClassDB::bind_method(D_METHOD("get_joint_offset"), &PhysicalBone3D::get_joint_offset);
	ClassDB::bind_method(D_METHOD("set_joint_rotation", "euler"), &PhysicalBone3D::set_joint_rotation);
	ClassDB::bind_method(D_METHOD("get_joint_rotation"), &PhysicalBone3D::get_joint_rotation);

	ClassDB::bind_method(D_METHOD("set_body_offset", "offset"), &PhysicalBone3D::set_body_offset);
	ClassDB::bind_method(D_METHOD("get_body_offset"), &PhysicalBone3D::get_body_offset);

	ClassDB::bind_method(D_METHOD("get_simulate_physics"), &PhysicalBone3D::get_simulate_physics);

	ClassDB::bind_method(D_METHOD("is_simulating_physics"), &PhysicalBone3D::is_simulating_physics);

	ClassDB::bind_method(D_METHOD("get_bone_id"), &PhysicalBone3D::get_bone_id);

	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &PhysicalBone3D::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &PhysicalBone3D::get_mass);

	ClassDB::bind_method(D_METHOD("set_friction", "friction"), &PhysicalBone3D::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &PhysicalBone3D::get_friction);

	ClassDB::bind_method(D_METHOD("set_bounce", "bounce"), &PhysicalBone3D::set_bounce);
	ClassDB::bind_method(D_METHOD("get_bounce"), &PhysicalBone3D::get_bounce);

	ClassDB::bind_method(D_METHOD("set_gravity_scale", "gravity_scale"), &PhysicalBone3D::set_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_gravity_scale"), &PhysicalBone3D::get_gravity_scale);

	ClassDB::bind_method(D_METHOD("set_linear_damp_mode", "linear_damp_mode"), &PhysicalBone3D::set_linear_damp_mode);
	ClassDB::bind_method(D_METHOD("get_linear_damp_mode"), &PhysicalBone3D::get_linear_damp_mode);

	ClassDB::bind_method(D_METHOD("set_angular_damp_mode", "angular_damp_mode"), &PhysicalBone3D::set_angular_damp_mode);
	ClassDB::bind_method(D_METHOD("get_angular_damp_mode"), &PhysicalBone3D::get_angular_damp_mode);

	ClassDB::bind_method(D_METHOD("set_linear_damp", "linear_damp"), &PhysicalBone3D::set_linear_damp);
	ClassDB::bind_method(D_METHOD("get_linear_damp"), &PhysicalBone3D::get_linear_damp);

	ClassDB::bind_method(D_METHOD("set_angular_damp", "angular_damp"), &PhysicalBone3D::set_angular_damp);
	ClassDB::bind_method(D_METHOD("get_angular_damp"), &PhysicalBone3D::get_angular_damp);

	ClassDB::bind_method(D_METHOD("set_linear_velocity", "linear_velocity"), &PhysicalBone3D::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysicalBone3D::get_linear_velocity);

	ClassDB::bind_method(D_METHOD("set_angular_velocity", "angular_velocity"), &PhysicalBone3D::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysicalBone3D::get_angular_velocity);

	ClassDB::bind_method(D_METHOD("set_use_custom_integrator", "enable"), &PhysicalBone3D::set_use_custom_integrator);
	ClassDB::bind_method(D_METHOD("is_using_custom_integrator"), &PhysicalBone3D::is_using_custom_integrator);

	ClassDB::bind_method(D_METHOD("set_can_sleep", "able_to_sleep"), &PhysicalBone3D::set_can_sleep);
	ClassDB::bind_method(D_METHOD("is_able_to_sleep"), &PhysicalBone3D::is_able_to_sleep);

	GDVIRTUAL_BIND(_integrate_forces, "state");

	ADD_GROUP("Joint", "joint_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "joint_type", PROPERTY_HINT_ENUM, "None,PinJoint,ConeJoint,HingeJoint,SliderJoint,6DOFJoint"), "set_joint_type", "get_joint_type");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "joint_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_joint_offset", "get_joint_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "joint_rotation", PROPERTY_HINT_RANGE, "-360,360,0.01,or_less,or_greater,radians_as_degrees"), "set_joint_rotation", "get_joint_rotation");

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "body_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_body_offset", "get_body_offset");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass", PROPERTY_HINT_RANGE, "0.01,1000,0.01,or_greater,exp,suffix:kg"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_friction", "get_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_bounce", "get_bounce");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_scale", PROPERTY_HINT_RANGE, "-8,8,0.001,or_less,or_greater"), "set_gravity_scale", "get_gravity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "custom_integrator"), "set_use_custom_integrator", "is_using_custom_integrator");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "linear_damp_mode", PROPERTY_HINT_ENUM, "Combine,Replace"), "set_linear_damp_mode", "get_linear_damp_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_linear_damp", "get_linear_damp");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "angular_damp_mode", PROPERTY_HINT_ENUM, "Combine,Replace"), "set_angular_damp_mode", "get_angular_damp_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_angular_damp", "get_angular_damp");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_linear_velocity", "get_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_angular_velocity", "get_angular_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), "set_can_sleep", "is_able_to_sleep");

	BIND_ENUM_CONSTANT(DAMP_MODE_COMBINE);
	BIND_ENUM_CONSTANT(DAMP_MODE_REPLACE);

	BIND_ENUM_CONSTANT(JOINT_TYPE_NONE);
	BIND_ENUM_CONSTANT(JOINT_TYPE_PIN);
	BIND_ENUM_CONSTANT(JOINT_TYPE_CONE);
	BIND_ENUM_CONSTANT(JOINT_TYPE_HINGE);
	BIND_ENUM_CONSTANT(JOINT_TYPE_SLIDER);
	BIND_ENUM_CONSTANT(JOINT_TYPE_6DOF);
}

void PhysicalBone3D::_update_joint_offset() {
	_fix_joint_offset();

	set_ignore_transform_notification(true);
	reset_to_rest_position();
	set_ignore_transform_notification(false);

#ifdef TOOLS_ENABLED
	update_gizmos();
#endif
}

void PhysicalBone3D::_fix_joint_offset() {
	// Clamp joint origin to bone origin
	PhysicalBoneSimulator3D *simulator = get_simulator();
	if (simulator) {
		joint_offset.origin = body_offset.affine_inverse().origin;
	}
}

void PhysicalBone3D::_reload_joint() {
	PhysicalBoneSimulator3D *simulator = get_simulator();
	if (!simulator || !simulator->get_skeleton()) {
		PhysicsServer3D::get_singleton()->joint_clear(joint);
		return;
	}

	PhysicalBone3D *body_a = simulator->get_physical_bone_parent(bone_id);
	if (!body_a) {
		PhysicsServer3D::get_singleton()->joint_clear(joint);
		return;
	}

	Transform3D joint_transf = get_global_transform() * joint_offset;
	Transform3D local_a = body_a->get_global_transform().affine_inverse() * joint_transf;
	local_a.orthonormalize();

	switch (get_joint_type()) {
		case JOINT_TYPE_PIN: {
			PhysicsServer3D::get_singleton()->joint_make_pin(joint, body_a->get_rid(), local_a.origin, get_rid(), joint_offset.origin);
			const PinJointData *pjd(static_cast<const PinJointData *>(joint_data));
			PhysicsServer3D::get_singleton()->pin_joint_set_param(joint, PhysicsServer3D::PIN_JOINT_BIAS, pjd->bias);
			PhysicsServer3D::get_singleton()->pin_joint_set_param(joint, PhysicsServer3D::PIN_JOINT_DAMPING, pjd->damping);
			PhysicsServer3D::get_singleton()->pin_joint_set_param(joint, PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP, pjd->impulse_clamp);

		} break;
		case JOINT_TYPE_CONE: {
			PhysicsServer3D::get_singleton()->joint_make_cone_twist(joint, body_a->get_rid(), local_a, get_rid(), joint_offset);
			const ConeJointData *cjd(static_cast<const ConeJointData *>(joint_data));
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(joint, PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN, cjd->swing_span);
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(joint, PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN, cjd->twist_span);
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(joint, PhysicsServer3D::CONE_TWIST_JOINT_BIAS, cjd->bias);
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(joint, PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS, cjd->softness);
			PhysicsServer3D::get_singleton()->cone_twist_joint_set_param(joint, PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION, cjd->relaxation);

		} break;
		case JOINT_TYPE_HINGE: {
			PhysicsServer3D::get_singleton()->joint_make_hinge(joint, body_a->get_rid(), local_a, get_rid(), joint_offset);
			const HingeJointData *hjd(static_cast<const HingeJointData *>(joint_data));
			PhysicsServer3D::get_singleton()->hinge_joint_set_flag(joint, PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT, hjd->angular_limit_enabled);
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(joint, PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER, hjd->angular_limit_upper);
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(joint, PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER, hjd->angular_limit_lower);
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(joint, PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS, hjd->angular_limit_bias);
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(joint, PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS, hjd->angular_limit_softness);
			PhysicsServer3D::get_singleton()->hinge_joint_set_param(joint, PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION, hjd->angular_limit_relaxation);

		} break;
		case JOINT_TYPE_SLIDER: {
			PhysicsServer3D::get_singleton()->joint_make_slider(joint, body_a->get_rid(), local_a, get_rid(), joint_offset);
			const SliderJointData *sjd(static_cast<const SliderJointData *>(joint_data));
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER, sjd->linear_limit_upper);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER, sjd->linear_limit_lower);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS, sjd->linear_limit_softness);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION, sjd->linear_limit_restitution);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING, sjd->linear_limit_restitution);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER, sjd->angular_limit_upper);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER, sjd->angular_limit_lower);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, sjd->angular_limit_softness);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS, sjd->angular_limit_softness);
			PhysicsServer3D::get_singleton()->slider_joint_set_param(joint, PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING, sjd->angular_limit_damping);

		} break;
		case JOINT_TYPE_6DOF: {
			PhysicsServer3D::get_singleton()->joint_make_generic_6dof(joint, body_a->get_rid(), local_a, get_rid(), joint_offset);
			const SixDOFJointData *g6dofjd(static_cast<const SixDOFJointData *>(joint_data));
			for (int axis = 0; axis < 3; ++axis) {
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT, g6dofjd->axis_data[axis].linear_limit_enabled);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT, g6dofjd->axis_data[axis].linear_limit_upper);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT, g6dofjd->axis_data[axis].linear_limit_lower);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS, g6dofjd->axis_data[axis].linear_limit_softness);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING, g6dofjd->axis_data[axis].linear_spring_enabled);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS, g6dofjd->axis_data[axis].linear_spring_stiffness);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING, g6dofjd->axis_data[axis].linear_spring_damping);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT, g6dofjd->axis_data[axis].linear_equilibrium_point);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION, g6dofjd->axis_data[axis].linear_restitution);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING, g6dofjd->axis_data[axis].linear_damping);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT, g6dofjd->axis_data[axis].angular_limit_enabled);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT, g6dofjd->axis_data[axis].angular_limit_upper);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT, g6dofjd->axis_data[axis].angular_limit_lower);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS, g6dofjd->axis_data[axis].angular_limit_softness);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION, g6dofjd->axis_data[axis].angular_restitution);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING, g6dofjd->axis_data[axis].angular_damping);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP, g6dofjd->axis_data[axis].erp);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_flag(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING, g6dofjd->axis_data[axis].angular_spring_enabled);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS, g6dofjd->axis_data[axis].angular_spring_stiffness);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING, g6dofjd->axis_data[axis].angular_spring_damping);
				PhysicsServer3D::get_singleton()->generic_6dof_joint_set_param(joint, static_cast<Vector3::Axis>(axis), PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT, g6dofjd->axis_data[axis].angular_equilibrium_point);
			}

		} break;
		case JOINT_TYPE_NONE: {
		} break;
	}
}

void PhysicalBone3D::_on_bone_parent_changed() {
	_reload_joint();
}

void PhysicalBone3D::_update_simulator_path() {
	simulator_id = ObjectID();
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(get_parent());
	if (sim) {
		simulator_id = sim->get_instance_id();
		return;
	}
#ifndef DISABLE_DEPRECATED
	Skeleton3D *sk = cast_to<Skeleton3D>(get_parent());
	if (sk) {
		PhysicalBoneSimulator3D *ssim = cast_to<PhysicalBoneSimulator3D>(sk->get_simulator());
		if (ssim) {
			simulator_id = ssim->get_instance_id();
		}
	}
#endif // _DISABLE_DEPRECATED
}

PhysicalBoneSimulator3D *PhysicalBone3D::get_simulator() const {
	return Object::cast_to<PhysicalBoneSimulator3D>(ObjectDB::get_instance(simulator_id));
}

Skeleton3D *PhysicalBone3D::get_skeleton() const {
	PhysicalBoneSimulator3D *simulator = get_simulator();
	if (simulator) {
		return simulator->get_skeleton();
	}
	return nullptr;
}

#ifdef TOOLS_ENABLED
void PhysicalBone3D::_set_gizmo_move_joint(bool p_move_joint) {
	gizmo_move_joint = p_move_joint;
}

Transform3D PhysicalBone3D::get_global_gizmo_transform() const {
	return gizmo_move_joint ? get_global_transform() * joint_offset : get_global_transform();
}

Transform3D PhysicalBone3D::get_local_gizmo_transform() const {
	return gizmo_move_joint ? get_transform() * joint_offset : get_transform();
}
#endif

const PhysicalBone3D::JointData *PhysicalBone3D::get_joint_data() const {
	return joint_data;
}

void PhysicalBone3D::set_joint_type(JointType p_joint_type) {
	if (p_joint_type == get_joint_type()) {
		return;
	}

	if (joint_data) {
		memdelete(joint_data);
	}
	joint_data = nullptr;
	switch (p_joint_type) {
		case JOINT_TYPE_PIN:
			joint_data = memnew(PinJointData);
			break;
		case JOINT_TYPE_CONE:
			joint_data = memnew(ConeJointData);
			break;
		case JOINT_TYPE_HINGE:
			joint_data = memnew(HingeJointData);
			break;
		case JOINT_TYPE_SLIDER:
			joint_data = memnew(SliderJointData);
			break;
		case JOINT_TYPE_6DOF:
			joint_data = memnew(SixDOFJointData);
			break;
		case JOINT_TYPE_NONE:
			break;
	}

	_reload_joint();

#ifdef TOOLS_ENABLED
	notify_property_list_changed();
	update_gizmos();
#endif
}

PhysicalBone3D::JointType PhysicalBone3D::get_joint_type() const {
	return joint_data ? joint_data->get_joint_type() : JOINT_TYPE_NONE;
}

void PhysicalBone3D::set_joint_offset(const Transform3D &p_offset) {
	joint_offset = p_offset;

	_update_joint_offset();
}

const Transform3D &PhysicalBone3D::get_joint_offset() const {
	return joint_offset;
}

void PhysicalBone3D::set_joint_rotation(const Vector3 &p_euler_rad) {
	joint_offset.basis.set_euler_scale(p_euler_rad, joint_offset.basis.get_scale());

	_update_joint_offset();
}

Vector3 PhysicalBone3D::get_joint_rotation() const {
	return joint_offset.basis.get_euler_normalized();
}

const Transform3D &PhysicalBone3D::get_body_offset() const {
	return body_offset;
}

void PhysicalBone3D::set_body_offset(const Transform3D &p_offset) {
	body_offset = p_offset;
	body_offset_inverse = body_offset.affine_inverse();

	_update_joint_offset();
}

void PhysicalBone3D::set_simulate_physics(bool p_simulate) {
	if (simulate_physics == p_simulate) {
		return;
	}

	simulate_physics = p_simulate;
	reset_physics_simulation_state();
}

bool PhysicalBone3D::get_simulate_physics() {
	return simulate_physics;
}

bool PhysicalBone3D::is_simulating_physics() {
	return _internal_simulate_physics;
}

void PhysicalBone3D::set_bone_name(const String &p_name) {
	bone_name = p_name;
	bone_id = -1;

	update_bone_id();
	reset_to_rest_position();
}

const String &PhysicalBone3D::get_bone_name() const {
	return bone_name;
}

void PhysicalBone3D::set_mass(real_t p_mass) {
	ERR_FAIL_COND(p_mass <= 0);
	mass = p_mass;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_MASS, mass);
}

real_t PhysicalBone3D::get_mass() const {
	return mass;
}

void PhysicalBone3D::set_friction(real_t p_friction) {
	ERR_FAIL_COND(p_friction < 0 || p_friction > 1);

	friction = p_friction;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, friction);
}

real_t PhysicalBone3D::get_friction() const {
	return friction;
}

void PhysicalBone3D::set_bounce(real_t p_bounce) {
	ERR_FAIL_COND(p_bounce < 0 || p_bounce > 1);

	bounce = p_bounce;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, bounce);
}

real_t PhysicalBone3D::get_bounce() const {
	return bounce;
}

void PhysicalBone3D::set_gravity_scale(real_t p_gravity_scale) {
	gravity_scale = p_gravity_scale;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE, gravity_scale);
}

real_t PhysicalBone3D::get_gravity_scale() const {
	return gravity_scale;
}

void PhysicalBone3D::set_linear_damp_mode(DampMode p_mode) {
	linear_damp_mode = p_mode;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE, linear_damp_mode);
}

PhysicalBone3D::DampMode PhysicalBone3D::get_linear_damp_mode() const {
	return linear_damp_mode;
}

void PhysicalBone3D::set_angular_damp_mode(DampMode p_mode) {
	angular_damp_mode = p_mode;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE, angular_damp_mode);
}

PhysicalBone3D::DampMode PhysicalBone3D::get_angular_damp_mode() const {
	return angular_damp_mode;
}

void PhysicalBone3D::set_linear_damp(real_t p_linear_damp) {
	ERR_FAIL_COND(p_linear_damp < 0);

	linear_damp = p_linear_damp;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_LINEAR_DAMP, linear_damp);
}

real_t PhysicalBone3D::get_linear_damp() const {
	return linear_damp;
}

void PhysicalBone3D::set_angular_damp(real_t p_angular_damp) {
	ERR_FAIL_COND(p_angular_damp < 0);

	angular_damp = p_angular_damp;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP, angular_damp);
}

real_t PhysicalBone3D::get_angular_damp() const {
	return angular_damp;
}

void PhysicalBone3D::set_can_sleep(bool p_active) {
	can_sleep = p_active;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_CAN_SLEEP, p_active);
}

bool PhysicalBone3D::is_able_to_sleep() const {
	return can_sleep;
}

PhysicalBone3D::PhysicalBone3D() :
		PhysicsBody3D(PhysicsServer3D::BODY_MODE_STATIC) {
	joint = PhysicsServer3D::get_singleton()->joint_create();
	reset_physics_simulation_state();
}

PhysicalBone3D::~PhysicalBone3D() {
	if (joint_data) {
		memdelete(joint_data);
	}
	ERR_FAIL_NULL(PhysicsServer3D::get_singleton());
	PhysicsServer3D::get_singleton()->free(joint);
}

void PhysicalBone3D::update_bone_id() {
	PhysicalBoneSimulator3D *simulator = get_simulator();
	if (!simulator) {
		return;
	}

	const int new_bone_id = simulator->find_bone(bone_name);

	if (new_bone_id != bone_id) {
		if (bone_id != -1) {
			// Assert the unbind from old node
			simulator->unbind_physical_bone_from_bone(bone_id);
		}

		bone_id = new_bone_id;

		simulator->bind_physical_bone_to_bone(bone_id, this);

		_fix_joint_offset();
		reset_physics_simulation_state();
	}
}

void PhysicalBone3D::update_offset() {
#ifdef TOOLS_ENABLED
	PhysicalBoneSimulator3D *simulator = get_simulator();
	Skeleton3D *skeleton = get_skeleton();
	if (simulator && skeleton) {
		Transform3D bone_transform(skeleton->get_global_transform());
		if (bone_id != -1) {
			bone_transform *= simulator->get_bone_global_pose(bone_id);
		}

		if (gizmo_move_joint) {
			bone_transform *= body_offset;
			set_joint_offset(bone_transform.affine_inverse() * get_global_transform());
		} else {
			set_body_offset(bone_transform.affine_inverse() * get_global_transform());
		}
	}
#endif
}

void PhysicalBone3D::_start_physics_simulation() {
	if (_internal_simulate_physics || !simulator_id.is_valid() || bone_id == -1) {
		return;
	}
	reset_to_rest_position();
	set_body_mode(PhysicsServer3D::BODY_MODE_RIGID);
	PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), get_collision_layer());
	PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), get_collision_mask());
	PhysicsServer3D::get_singleton()->body_set_collision_priority(get_rid(), get_collision_priority());
	PhysicsServer3D::get_singleton()->body_set_state_sync_callback(get_rid(), callable_mp(this, &PhysicalBone3D::_body_state_changed));
	set_as_top_level(true);
	_internal_simulate_physics = true;
}

void PhysicalBone3D::_stop_physics_simulation() {
	PhysicalBoneSimulator3D *simulator = get_simulator();
	if (simulator) {
		if (simulator->is_active() && bone_id != -1) {
			set_body_mode(PhysicsServer3D::BODY_MODE_KINEMATIC);
			PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), get_collision_layer());
			PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), get_collision_mask());
			PhysicsServer3D::get_singleton()->body_set_collision_priority(get_rid(), get_collision_priority());
		} else {
			set_body_mode(PhysicsServer3D::BODY_MODE_STATIC);
			PhysicsServer3D::get_singleton()->body_set_collision_layer(get_rid(), 0);
			PhysicsServer3D::get_singleton()->body_set_collision_mask(get_rid(), 0);
			PhysicsServer3D::get_singleton()->body_set_collision_priority(get_rid(), 1.0);
		}
	}
	if (_internal_simulate_physics) {
		PhysicsServer3D::get_singleton()->body_set_state_sync_callback(get_rid(), Callable());
		set_as_top_level(false);
		_internal_simulate_physics = false;
	}
}
