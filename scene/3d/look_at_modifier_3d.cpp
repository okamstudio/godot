/**************************************************************************/
/*  look_at_modifier_3d.cpp                                               */
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

#include "look_at_modifier_3d.h"

void LookAtModifier3D::_validate_property(PropertyInfo &p_property) const {
	SkeletonModifier3D::_validate_property(p_property);

	if (p_property.name == "bone_name" || p_property.name == "origin_bone_name") {
		Skeleton3D *skeleton = get_skeleton();
		if (skeleton) {
			p_property.hint = PROPERTY_HINT_ENUM;
			p_property.hint_string = skeleton->get_concatenated_bone_names();
		} else {
			p_property.hint = PROPERTY_HINT_NONE;
			p_property.hint_string = "";
		}
	}

	if (origin_from == ORIGIN_FROM_SPECIFIC_BONE) {
		if (p_property.name == "origin_external_node") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	} else if (origin_from == ORIGIN_FROM_EXTERNAL_NODE) {
		if (p_property.name == "origin_bone" || p_property.name == "origin_bone_name") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	} else {
		if (p_property.name == "origin_external_node" || p_property.name == "origin_bone" || p_property.name == "origin_bone_name") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}

	if ((!use_angle_limitation &&
				(p_property.name == "symmetry_limitation" || p_property.name.ends_with("limit_angle") || p_property.name.ends_with("damp_threshold"))) ||
			(!use_secondary_rotation && p_property.name.begins_with("secondary_")) ||
			(!symmetry_limitation && (p_property.name == "primary_limit_angle" || p_property.name == "primary_damp_threshold" || p_property.name == "secondary_limit_angle" || p_property.name == "secondary_damp_threshold")) ||
			(symmetry_limitation && (p_property.name.begins_with("primary_positive") || p_property.name.begins_with("primary_negative") || p_property.name.begins_with("secondary_positive") || (p_property.name.begins_with("secondary_negative"))))) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

PackedStringArray LookAtModifier3D::get_configuration_warnings() const {
	PackedStringArray warnings = SkeletonModifier3D::get_configuration_warnings();
	if (get_axis_from_bone_axis(forward_axis) == primary_rotation_axis) {
		warnings.push_back(RTR("Forward axis and primary rotation axis must not be parallel."));
	}
	return warnings;
}

void LookAtModifier3D::set_bone_name(const String &p_bone_name) {
	bone_name = p_bone_name;
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		set_bone(sk->find_bone(bone_name));
	}
}

String LookAtModifier3D::get_bone_name() const {
	return bone_name;
}

void LookAtModifier3D::set_bone(int p_bone) {
	bone = p_bone;
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		if (bone <= -1 || bone >= sk->get_bone_count()) {
			WARN_PRINT("Bone index out of range!");
			bone = -1;
		} else {
			bone_name = sk->get_bone_name(bone);
		}
	}
}

int LookAtModifier3D::get_bone() const {
	return bone;
}

void LookAtModifier3D::set_forward_axis(BoneAxis p_axis) {
	forward_axis = p_axis;
	update_configuration_warnings();
}

SkeletonModifier3D::BoneAxis LookAtModifier3D::get_forward_axis() const {
	return forward_axis;
}

void LookAtModifier3D::set_primary_rotation_axis(Vector3::Axis p_axis) {
	primary_rotation_axis = p_axis;
	update_configuration_warnings();
}

Vector3::Axis LookAtModifier3D::get_primary_rotation_axis() const {
	return primary_rotation_axis;
}

void LookAtModifier3D::set_use_secondary_rotation(bool p_enabled) {
	use_secondary_rotation = p_enabled;
	notify_property_list_changed();
}

bool LookAtModifier3D::is_using_secondary_rotation() const {
	return use_secondary_rotation;
}

void LookAtModifier3D::set_target_node(const NodePath &p_target_node) {
	if (target_node != p_target_node) {
		init_transition();
	}
	target_node = p_target_node;
}

NodePath LookAtModifier3D::get_target_node() const {
	return target_node;
}

// For origin settings.

void LookAtModifier3D::set_origin_from(OriginFrom p_origin_from) {
	origin_from = p_origin_from;
	notify_property_list_changed();
}

LookAtModifier3D::OriginFrom LookAtModifier3D::get_origin_from() const {
	return origin_from;
}

void LookAtModifier3D::set_origin_bone_name(const String &p_bone_name) {
	origin_bone_name = p_bone_name;
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		set_origin_bone(sk->find_bone(origin_bone_name));
	}
}

String LookAtModifier3D::get_origin_bone_name() const {
	return origin_bone_name;
}

void LookAtModifier3D::set_origin_bone(int p_bone) {
	origin_bone = p_bone;
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		if (origin_bone <= -1 || origin_bone >= sk->get_bone_count()) {
			WARN_PRINT("Bone index out of range!");
			origin_bone = -1;
		} else {
			origin_bone_name = sk->get_bone_name(origin_bone);
		}
	}
}

int LookAtModifier3D::get_origin_bone() const {
	return origin_bone;
}

void LookAtModifier3D::set_origin_external_node(const NodePath &p_external_node) {
	origin_external_node = p_external_node;
}

NodePath LookAtModifier3D::get_origin_external_node() const {
	return origin_external_node;
}

void LookAtModifier3D::set_origin_offset(const Vector3 &p_offset) {
	origin_offset = p_offset;
}

Vector3 LookAtModifier3D::get_origin_offset() const {
	return origin_offset;
}

void LookAtModifier3D::set_origin_safe_margin(float p_margin) {
	origin_safe_margin = p_margin;
}

float LookAtModifier3D::get_origin_safe_margin() const {
	return origin_safe_margin;
}

// For time-based interpolation.

void LookAtModifier3D::set_duration(float p_duration) {
	duration = p_duration;
	if (Math::is_zero_approx(p_duration)) {
		time_step = 0;
		remaining = 0;
	} else {
		time_step = 1.0 / p_duration; // Cache to avoid division.
	}
}

float LookAtModifier3D::get_duration() const {
	return duration;
}

void LookAtModifier3D::set_transition_type(Tween::TransitionType p_transition_type) {
	transition_type = p_transition_type;
}

Tween::TransitionType LookAtModifier3D::get_transition_type() const {
	return transition_type;
}

void LookAtModifier3D::set_ease_type(Tween::EaseType p_ease_type) {
	ease_type = p_ease_type;
}

Tween::EaseType LookAtModifier3D::get_ease_type() const {
	return ease_type;
}

// For angle limitation.

void LookAtModifier3D::set_use_angle_limitation(bool p_enabled) {
	use_angle_limitation = p_enabled;
	notify_property_list_changed();
}

bool LookAtModifier3D::is_using_angle_limitation() const {
	return use_angle_limitation;
}

void LookAtModifier3D::set_symmetry_limitation(bool p_enabled) {
	symmetry_limitation = p_enabled;
	notify_property_list_changed();
}

bool LookAtModifier3D::is_limitation_symmetry() const {
	return symmetry_limitation;
}

void LookAtModifier3D::set_primary_limit_angle(float p_angle) {
	primary_limit_angle = p_angle;
}

float LookAtModifier3D::get_primary_limit_angle() const {
	return primary_limit_angle;
}

void LookAtModifier3D::set_primary_damp_threshold(float p_power) {
	primary_damp_threshold = p_power;
}

float LookAtModifier3D::get_primary_damp_threshold() const {
	return primary_damp_threshold;
}

void LookAtModifier3D::set_primary_positive_limit_angle(float p_angle) {
	primary_positive_limit_angle = p_angle;
}

float LookAtModifier3D::get_primary_positive_limit_angle() const {
	return primary_positive_limit_angle;
}

void LookAtModifier3D::set_primary_positive_damp_threshold(float p_power) {
	primary_positive_damp_threshold = p_power;
}

float LookAtModifier3D::get_primary_positive_damp_threshold() const {
	return primary_positive_damp_threshold;
}

void LookAtModifier3D::set_primary_negative_limit_angle(float p_angle) {
	primary_negative_limit_angle = p_angle;
}

float LookAtModifier3D::get_primary_negative_limit_angle() const {
	return primary_negative_limit_angle;
}

void LookAtModifier3D::set_primary_negative_damp_threshold(float p_power) {
	primary_negative_damp_threshold = p_power;
}

float LookAtModifier3D::get_primary_negative_damp_threshold() const {
	return primary_negative_damp_threshold;
}

void LookAtModifier3D::set_secondary_limit_angle(float p_angle) {
	secondary_limit_angle = p_angle;
}

float LookAtModifier3D::get_secondary_limit_angle() const {
	return secondary_limit_angle;
}

void LookAtModifier3D::set_secondary_damp_threshold(float p_power) {
	secondary_damp_threshold = p_power;
}

float LookAtModifier3D::get_secondary_damp_threshold() const {
	return secondary_damp_threshold;
}

void LookAtModifier3D::set_secondary_positive_limit_angle(float p_angle) {
	secondary_positive_limit_angle = p_angle;
}

float LookAtModifier3D::get_secondary_positive_limit_angle() const {
	return secondary_positive_limit_angle;
}

void LookAtModifier3D::set_secondary_positive_damp_threshold(float p_power) {
	secondary_positive_damp_threshold = p_power;
}

float LookAtModifier3D::get_secondary_positive_damp_threshold() const {
	return secondary_positive_damp_threshold;
}

void LookAtModifier3D::set_secondary_negative_limit_angle(float p_angle) {
	secondary_negative_limit_angle = p_angle;
}

float LookAtModifier3D::get_secondary_negative_limit_angle() const {
	return secondary_negative_limit_angle;
}

void LookAtModifier3D::set_secondary_negative_damp_threshold(float p_power) {
	secondary_negative_damp_threshold = p_power;
}

float LookAtModifier3D::get_secondary_negative_damp_threshold() const {
	return secondary_negative_damp_threshold;
}

bool LookAtModifier3D::is_target_within_limitation() const {
	return is_within_limitations;
}

float LookAtModifier3D::get_interpolation_remaining() const {
	return remaining * duration;
}

bool LookAtModifier3D::is_interpolating() const {
	return Math::is_zero_approx(remaining);
}

// General API.

void LookAtModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node", "target_node"), &LookAtModifier3D::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &LookAtModifier3D::get_target_node);

	ClassDB::bind_method(D_METHOD("set_bone_name", "bone_name"), &LookAtModifier3D::set_bone_name);
	ClassDB::bind_method(D_METHOD("get_bone_name"), &LookAtModifier3D::get_bone_name);
	ClassDB::bind_method(D_METHOD("set_bone", "bone"), &LookAtModifier3D::set_bone);
	ClassDB::bind_method(D_METHOD("get_bone"), &LookAtModifier3D::get_bone);
	ClassDB::bind_method(D_METHOD("set_forward_axis", "forward_axis"), &LookAtModifier3D::set_forward_axis);
	ClassDB::bind_method(D_METHOD("get_forward_axis"), &LookAtModifier3D::get_forward_axis);
	ClassDB::bind_method(D_METHOD("set_primary_rotation_axis", "axis"), &LookAtModifier3D::set_primary_rotation_axis);
	ClassDB::bind_method(D_METHOD("get_primary_rotation_axis"), &LookAtModifier3D::get_primary_rotation_axis);
	ClassDB::bind_method(D_METHOD("set_use_secondary_rotation", "enabled"), &LookAtModifier3D::set_use_secondary_rotation);
	ClassDB::bind_method(D_METHOD("is_using_secondary_rotation"), &LookAtModifier3D::is_using_secondary_rotation);
	ClassDB::bind_method(D_METHOD("set_origin_safe_margin", "margin"), &LookAtModifier3D::set_origin_safe_margin);
	ClassDB::bind_method(D_METHOD("get_origin_safe_margin"), &LookAtModifier3D::get_origin_safe_margin);

	ClassDB::bind_method(D_METHOD("set_origin_from", "origin_from"), &LookAtModifier3D::set_origin_from);
	ClassDB::bind_method(D_METHOD("get_origin_from"), &LookAtModifier3D::get_origin_from);
	ClassDB::bind_method(D_METHOD("set_origin_bone_name", "bone_name"), &LookAtModifier3D::set_origin_bone_name);
	ClassDB::bind_method(D_METHOD("get_origin_bone_name"), &LookAtModifier3D::get_origin_bone_name);
	ClassDB::bind_method(D_METHOD("set_origin_bone", "bone"), &LookAtModifier3D::set_origin_bone);
	ClassDB::bind_method(D_METHOD("get_origin_bone"), &LookAtModifier3D::get_origin_bone);
	ClassDB::bind_method(D_METHOD("set_origin_external_node", "external_node"), &LookAtModifier3D::set_origin_external_node);
	ClassDB::bind_method(D_METHOD("get_origin_external_node"), &LookAtModifier3D::get_origin_external_node);

	ClassDB::bind_method(D_METHOD("set_origin_offset", "offset"), &LookAtModifier3D::set_origin_offset);
	ClassDB::bind_method(D_METHOD("get_origin_offset"), &LookAtModifier3D::get_origin_offset);

	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &LookAtModifier3D::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &LookAtModifier3D::get_duration);
	ClassDB::bind_method(D_METHOD("set_transition_type", "transition_type"), &LookAtModifier3D::set_transition_type);
	ClassDB::bind_method(D_METHOD("get_transition_type"), &LookAtModifier3D::get_transition_type);
	ClassDB::bind_method(D_METHOD("set_ease_type", "ease_type"), &LookAtModifier3D::set_ease_type);
	ClassDB::bind_method(D_METHOD("get_ease_type"), &LookAtModifier3D::get_ease_type);

	ClassDB::bind_method(D_METHOD("set_use_angle_limitation", "enabled"), &LookAtModifier3D::set_use_angle_limitation);
	ClassDB::bind_method(D_METHOD("is_using_angle_limitation"), &LookAtModifier3D::is_using_angle_limitation);
	ClassDB::bind_method(D_METHOD("set_symmetry_limitation", "enabled"), &LookAtModifier3D::set_symmetry_limitation);
	ClassDB::bind_method(D_METHOD("is_limitation_symmetry"), &LookAtModifier3D::is_limitation_symmetry);

	ClassDB::bind_method(D_METHOD("set_primary_limit_angle", "angle"), &LookAtModifier3D::set_primary_limit_angle);
	ClassDB::bind_method(D_METHOD("get_primary_limit_angle"), &LookAtModifier3D::get_primary_limit_angle);
	ClassDB::bind_method(D_METHOD("set_primary_damp_threshold", "power"), &LookAtModifier3D::set_primary_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_primary_damp_threshold"), &LookAtModifier3D::get_primary_damp_threshold);

	ClassDB::bind_method(D_METHOD("set_primary_positive_limit_angle", "angle"), &LookAtModifier3D::set_primary_positive_limit_angle);
	ClassDB::bind_method(D_METHOD("get_primary_positive_limit_angle"), &LookAtModifier3D::get_primary_positive_limit_angle);
	ClassDB::bind_method(D_METHOD("set_primary_positive_damp_threshold", "power"), &LookAtModifier3D::set_primary_positive_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_primary_positive_damp_threshold"), &LookAtModifier3D::get_primary_positive_damp_threshold);
	ClassDB::bind_method(D_METHOD("set_primary_negative_limit_angle", "angle"), &LookAtModifier3D::set_primary_negative_limit_angle);
	ClassDB::bind_method(D_METHOD("get_primary_negative_limit_angle"), &LookAtModifier3D::get_primary_negative_limit_angle);
	ClassDB::bind_method(D_METHOD("set_primary_negative_damp_threshold", "power"), &LookAtModifier3D::set_primary_negative_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_primary_negative_damp_threshold"), &LookAtModifier3D::get_primary_negative_damp_threshold);

	ClassDB::bind_method(D_METHOD("set_secondary_limit_angle", "angle"), &LookAtModifier3D::set_secondary_limit_angle);
	ClassDB::bind_method(D_METHOD("get_secondary_limit_angle"), &LookAtModifier3D::get_secondary_limit_angle);
	ClassDB::bind_method(D_METHOD("set_secondary_damp_threshold", "power"), &LookAtModifier3D::set_secondary_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_secondary_damp_threshold"), &LookAtModifier3D::get_secondary_damp_threshold);

	ClassDB::bind_method(D_METHOD("set_secondary_positive_limit_angle", "angle"), &LookAtModifier3D::set_secondary_positive_limit_angle);
	ClassDB::bind_method(D_METHOD("get_secondary_positive_limit_angle"), &LookAtModifier3D::get_secondary_positive_limit_angle);
	ClassDB::bind_method(D_METHOD("set_secondary_positive_damp_threshold", "power"), &LookAtModifier3D::set_secondary_positive_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_secondary_positive_damp_threshold"), &LookAtModifier3D::get_secondary_positive_damp_threshold);
	ClassDB::bind_method(D_METHOD("set_secondary_negative_limit_angle", "angle"), &LookAtModifier3D::set_secondary_negative_limit_angle);
	ClassDB::bind_method(D_METHOD("get_secondary_negative_limit_angle"), &LookAtModifier3D::get_secondary_negative_limit_angle);
	ClassDB::bind_method(D_METHOD("set_secondary_negative_damp_threshold", "power"), &LookAtModifier3D::set_secondary_negative_damp_threshold);
	ClassDB::bind_method(D_METHOD("get_secondary_negative_damp_threshold"), &LookAtModifier3D::get_secondary_negative_damp_threshold);

	ClassDB::bind_method(D_METHOD("get_interpolation_remaining"), &LookAtModifier3D::get_interpolation_remaining);
	ClassDB::bind_method(D_METHOD("is_interpolating"), &LookAtModifier3D::is_interpolating);
	ClassDB::bind_method(D_METHOD("is_target_within_limitation"), &LookAtModifier3D::is_target_within_limitation);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_node", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_target_node", "get_target_node");

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bone_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_bone_name", "get_bone_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_bone", "get_bone");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "forward_axis", PROPERTY_HINT_ENUM, "+X,-X,+Y,-Y,+Z,-Z"), "set_forward_axis", "get_forward_axis");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "primary_rotation_axis", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_primary_rotation_axis", "get_primary_rotation_axis");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_secondary_rotation"), "set_use_secondary_rotation", "is_using_secondary_rotation");

	ADD_GROUP("Origin Settings", "origin_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "origin_from", PROPERTY_HINT_ENUM, "Self,SpecificBone,ExternalNode"), "set_origin_from", "get_origin_from");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "origin_bone_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_origin_bone_name", "get_origin_bone_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "origin_bone", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_origin_bone", "get_origin_bone");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "origin_external_node", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_origin_external_node", "get_origin_external_node");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "origin_offset"), "set_origin_offset", "get_origin_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_safe_margin", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater,suffix:m"), "set_origin_safe_margin", "get_origin_safe_margin");

	ADD_GROUP("Time Based Interpolation", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duration", PROPERTY_HINT_RANGE, "0,10,0.001,or_greater,suffix:s"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transition_type", PROPERTY_HINT_ENUM, "Linear,Sine,Quint,Quart,Quad,Expo,Elastic,Cubic,Circ,Bounce,Back,Spring"), "set_transition_type", "get_transition_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ease_type", PROPERTY_HINT_ENUM, "In,Out,InOut,OutIn"), "set_ease_type", "get_ease_type");

	ADD_GROUP("Angle Limitation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_angle_limitation"), "set_use_angle_limitation", "is_using_angle_limitation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "symmetry_limitation"), "set_symmetry_limitation", "is_limitation_symmetry");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_limit_angle", PROPERTY_HINT_RANGE, "0,360,0.01,radians_as_degrees"), "set_primary_limit_angle", "get_primary_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_primary_damp_threshold", "get_primary_damp_threshold");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_positive_limit_angle", PROPERTY_HINT_RANGE, "0,180,0.01,radians_as_degrees"), "set_primary_positive_limit_angle", "get_primary_positive_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_positive_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_primary_positive_damp_threshold", "get_primary_positive_damp_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_negative_limit_angle", PROPERTY_HINT_RANGE, "0,180,0.01,radians_as_degrees"), "set_primary_negative_limit_angle", "get_primary_negative_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "primary_negative_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_primary_negative_damp_threshold", "get_primary_negative_damp_threshold");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_limit_angle", PROPERTY_HINT_RANGE, "0,360,0.01,radians_as_degrees"), "set_secondary_limit_angle", "get_secondary_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_secondary_damp_threshold", "get_secondary_damp_threshold");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_positive_limit_angle", PROPERTY_HINT_RANGE, "0,180,0.01,radians_as_degrees"), "set_secondary_positive_limit_angle", "get_secondary_positive_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_positive_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_secondary_positive_damp_threshold", "get_secondary_positive_damp_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_negative_limit_angle", PROPERTY_HINT_RANGE, "0,180,0.01,radians_as_degrees"), "set_secondary_negative_limit_angle", "get_secondary_negative_limit_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "secondary_negative_damp_threshold", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_secondary_negative_damp_threshold", "get_secondary_negative_damp_threshold");

	BIND_ENUM_CONSTANT(ORIGIN_FROM_SELF);
	BIND_ENUM_CONSTANT(ORIGIN_FROM_SPECIFIC_BONE);
	BIND_ENUM_CONSTANT(ORIGIN_FROM_EXTERNAL_NODE);
}

void LookAtModifier3D::_process_modification() {
	if (!is_inside_tree()) {
		return;
	}

	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton || bone < 0 || bone >= skeleton->get_bone_count()) {
		return;
	}

	// Calculate bone rest space in the world.
	Transform3D bone_rest_space;
	int parent_bone = skeleton->get_bone_parent(bone);
	if (parent_bone < 0) {
		bone_rest_space = skeleton->get_global_transform();
		bone_rest_space.origin += skeleton->get_bone_rest(bone).origin;
	} else {
		bone_rest_space = skeleton->get_global_transform() * skeleton->get_bone_global_pose(parent_bone);
		bone_rest_space.origin += skeleton->get_bone_rest(bone).origin;
	}

	// Calculate forward_vector and destination.
	is_within_limitations = true;
	Vector3 prev_forward_vector = forward_vector;
	Quaternion destination;
	Node3D *target = Object::cast_to<Node3D>(get_node_or_null(target_node));
	if (!target) {
		destination = skeleton->get_bone_pose_rotation(bone);
	} else {
		Transform3D origin_tr;
		if (origin_from == ORIGIN_FROM_SPECIFIC_BONE && origin_bone >= 0 && origin_bone < skeleton->get_bone_count()) {
			origin_tr = skeleton->get_global_transform() * skeleton->get_bone_global_pose(origin_bone);
		} else if (origin_from == ORIGIN_FROM_EXTERNAL_NODE) {
			Node3D *origin_src = Object::cast_to<Node3D>(get_node_or_null(origin_external_node));
			if (origin_src) {
				origin_tr = origin_src->get_global_transform();
			} else {
				origin_tr = bone_rest_space;
			}
		} else {
			origin_tr = bone_rest_space;
		}
		forward_vector = bone_rest_space.orthonormalized().basis.xform_inv((target->get_global_position() - origin_tr.translated_local(origin_offset).origin));
		forward_vector_nrm = forward_vector.normalized();
		if (forward_vector_nrm.abs().is_equal_approx(get_vector_from_axis(primary_rotation_axis))) {
			destination = skeleton->get_bone_pose_rotation(bone);
			forward_vector = Vector3(0, 0, 0); // The zero-vector to be used for checking in the line immediately below to avoid animation glitch.
		} else {
			destination = look_at_with_axes(skeleton->get_bone_rest(bone)).basis.get_rotation_quaternion();
		}
	}

	// Detect flipping.
	bool is_not_max_influence = influence < 1.0;
	bool is_flippable = use_angle_limitation || is_not_max_influence;
	Vector3::Axis current_forward_axis = get_axis_from_bone_axis(forward_axis);
	if (is_intersecting_axis(prev_forward_vector, forward_vector, current_forward_axis, secondary_rotation_axis) ||
			is_intersecting_axis(prev_forward_vector, forward_vector, primary_rotation_axis, primary_rotation_axis, true) ||
			is_intersecting_axis(prev_forward_vector, forward_vector, secondary_rotation_axis, current_forward_axis) ||
			(prev_forward_vector != Vector3(0, 0, 0) && forward_vector == Vector3(0, 0, 0)) ||
			(prev_forward_vector == Vector3(0, 0, 0) && forward_vector != Vector3(0, 0, 0))) {
		init_transition();
	} else if (is_flippable && signbit(prev_forward_vector[secondary_rotation_axis]) != signbit(forward_vector[secondary_rotation_axis])) {
		// Flipping by angle_limitation can be detected by sign of secondary rotation axes during forward_vector is rotated more than 90 degree from forward_axis (means dot production is negative).
		Vector3 prev_forward_vector_nrm = forward_vector.normalized();
		Vector3 rest_forward_vector = get_vector_from_bone_axis(forward_axis);
		if (symmetry_limitation) {
			if ((is_not_max_influence || !Math::is_equal_approx(primary_limit_angle, (float)Math_TAU)) &&
					prev_forward_vector_nrm.dot(rest_forward_vector) < 0 &&
					forward_vector_nrm.dot(rest_forward_vector) < 0) {
				init_transition();
			}
		} else {
			if ((is_not_max_influence || !Math::is_equal_approx(primary_positive_limit_angle + primary_negative_limit_angle, (float)Math_TAU)) &&
					prev_forward_vector_nrm.dot(rest_forward_vector) < 0 &&
					forward_vector_nrm.dot(rest_forward_vector) < 0) {
				init_transition();
			}
		}
	}

	// Do time-based interpolation.
	if (remaining > 0) {
		double delta = 0.0;
		if (skeleton->get_modifier_callback_mode_process() == Skeleton3D::MODIFIER_CALLBACK_MODE_PROCESS_IDLE) {
			delta = get_process_delta_time();
		} else {
			delta = get_physics_process_delta_time();
		}
		remaining = MAX(0, remaining - time_step * delta);
		if (is_flippable) {
			// Interpolate through the rest same as AnimationTree blending for preventing to penetrate the bone into the body.
			Quaternion rest = skeleton->get_bone_rest(bone).basis.get_rotation_quaternion();
			float weight = Tween::run_equation(transition_type, ease_type, 1 - remaining, 0.0, 1.0, 1.0);
			destination = rest * Quaternion().slerp(rest.inverse() * from_q, 1 - weight) * Quaternion().slerp(rest.inverse() * destination, weight);
		} else {
			destination = from_q.slerp(destination, Tween::run_equation(transition_type, ease_type, 1 - remaining, 0.0, 1.0, 1.0));
		}
	}

	skeleton->set_bone_pose_rotation(bone, destination);
	prev_q = destination;
}

bool LookAtModifier3D::is_intersecting_axis(const Vector3 &p_prev, const Vector3 &p_current, Vector3::Axis p_flipping_axis, Vector3::Axis p_check_axis, bool p_check_plane) const {
	// Prevent that the angular velocity does not become too large.
	// Check that is p_flipping_axis flipped nearby p_check_axis (close than origin_safe_margin) or not. If p_check_plane is true, check two axes of crossed plane.
	if (p_check_plane) {
		if (get_projection_vector(p_prev, p_check_axis).length() > origin_safe_margin && get_projection_vector(p_current, p_check_axis).length() > origin_safe_margin) {
			return false;
		}
	} else if (Math::abs(p_prev[p_check_axis]) > origin_safe_margin && Math::abs(p_current[p_check_axis]) > origin_safe_margin) {
		return false;
	}

	return signbit(p_prev[p_flipping_axis]) != signbit(p_current[p_flipping_axis]);
}

Vector3 LookAtModifier3D::get_basis_vector_from_bone_axis(const Basis &p_basis, BoneAxis p_axis) {
	Vector3 ret;
	switch (p_axis) {
		case BONE_AXIS_PLUS_X: {
			ret = p_basis.get_column(0);
		} break;
		case BONE_AXIS_MINUS_X: {
			ret = -p_basis.get_column(0);
		} break;
		case BONE_AXIS_PLUS_Y: {
			ret = p_basis.get_column(1);
		} break;
		case BONE_AXIS_MINUS_Y: {
			ret = -p_basis.get_column(1);
		} break;
		case BONE_AXIS_PLUS_Z: {
			ret = p_basis.get_column(2);
		} break;
		case BONE_AXIS_MINUS_Z: {
			ret = -p_basis.get_column(2);
		} break;
	}
	return ret;
}

Vector3::Axis LookAtModifier3D::get_secondary_rotation_axis(BoneAxis p_forward_axis, Vector3::Axis p_primary_rotation_axis) {
	Vector3 secondary_plane = get_vector_from_bone_axis(p_forward_axis) + get_vector_from_axis(p_primary_rotation_axis);
	return Math::is_zero_approx(secondary_plane.x) ? Vector3::AXIS_X : (Math::is_zero_approx(secondary_plane.y) ? Vector3::AXIS_Y : Vector3::AXIS_Z);
}

Vector2 LookAtModifier3D::get_projection_vector(const Vector3 &p_vector, Vector3::Axis p_axis) {
	// NOTE: axis is swapped between 2D and 3D.
	Vector2 ret;
	switch (p_axis) {
		case Vector3::AXIS_X: {
			ret = Vector2(p_vector.z, p_vector.y);
		} break;
		case Vector3::AXIS_Y: {
			ret = Vector2(p_vector.x, p_vector.z);
		} break;
		case Vector3::AXIS_Z: {
			ret = Vector2(p_vector.y, p_vector.x);
		} break;
	}
	return ret;
}

float LookAtModifier3D::remap_damped(float p_from, float p_to, float p_damp_threshold, float p_value) const {
	float sign = signbit(p_value) ? -1.0f : 1.0f;
	float abs_value = Math::abs(p_value);

	if (Math::is_equal_approx(p_damp_threshold, 1.0f) || Math::is_zero_approx(p_to)) {
		return sign * CLAMP(abs_value, p_from, p_to); // Avoid division by zero.
	}

	float value = Math::inverse_lerp(p_from, p_to, abs_value);

	if (value <= p_damp_threshold) {
		return sign * CLAMP(abs_value, p_from, p_to);
	}

	double limit = Math_PI;
	double inv_to = 1.0 / p_to;
	double end_x = limit * inv_to;
	double position = abs_value * inv_to;
	Vector2 start = Vector2(p_damp_threshold, p_damp_threshold);
	Vector2 mid = Vector2(1.0, 1.0);
	Vector2 end = Vector2(end_x, 1.0);
	value = get_bspline_y(start, mid, end, position);

	return sign * Math::lerp(p_from, p_to, value);
}

double LookAtModifier3D::get_bspline_y(const Vector2 &p_from, const Vector2 &p_control, const Vector2 &p_to, double p_x) const {
	double a = p_from.x - 2.0 * p_control.x + p_to.x;
	double b = -2.0 * p_from.x + 2.0 * p_control.x;
	double c = p_from.x - p_x;
	double t = 0.0;
	if (Math::is_zero_approx(a)) {
		t = -c / b; // Almost linear.
	} else {
		double discriminant = b * b - 4.0 * a * c;
		double sqrt_discriminant = Math::sqrt(discriminant);
		double e = 1.0 / (2.0 * a);
		double t1 = (-b + sqrt_discriminant) * e;
		t = (0.0 <= t1 && t1 <= 1.0) ? t1 : (-b - sqrt_discriminant) * e;
	}
	double u = 1.0 - t;
	double y = u * u * p_from.y + 2.0 * u * t * p_control.y + t * t * p_to.y;
	return y;
}

Transform3D LookAtModifier3D::look_at_with_axes(const Transform3D &p_rest) {
	// Primary rotation by projection to 2D plane by xform_inv and picking elements.
	Vector3 current_vector = get_basis_vector_from_bone_axis(p_rest.basis, forward_axis).normalized();
	Vector2 src_vec2 = get_projection_vector(p_rest.basis.xform_inv(forward_vector_nrm), primary_rotation_axis).normalized();
	Vector2 dst_vec2 = get_projection_vector(p_rest.basis.xform_inv(current_vector), primary_rotation_axis).normalized();
	real_t calculated_angle = src_vec2.angle_to(dst_vec2);
	Transform3D primary_result = p_rest.rotated_local(get_vector_from_axis(primary_rotation_axis), calculated_angle);
	Transform3D current_result = primary_result; // primary_result will be used by calculation of secondary rotation, current_result is rotated by that.
	float limit_angle = 0.0;
	float damp_threshold = 0.0;

	if (use_angle_limitation) {
		if (symmetry_limitation) {
			limit_angle = primary_limit_angle * 0.5f;
			damp_threshold = primary_damp_threshold;
		} else {
			if (signbit(calculated_angle)) {
				limit_angle = primary_negative_limit_angle;
				damp_threshold = primary_negative_damp_threshold;
			} else {
				limit_angle = primary_positive_limit_angle;
				damp_threshold = primary_positive_damp_threshold;
			}
		}
		if (Math::abs(calculated_angle) > limit_angle) {
			is_within_limitations = false;
		}
		calculated_angle = remap_damped(0, limit_angle, damp_threshold, calculated_angle);
		current_result = p_rest.rotated_local(get_vector_from_axis(primary_rotation_axis), calculated_angle);
	}

	// Needs for detecting flipping even if use_secondary_rotation is false.
	secondary_rotation_axis = get_secondary_rotation_axis(forward_axis, primary_rotation_axis);

	if (!use_secondary_rotation) {
		return current_result;
	}

	// Secondary rotation by projection to 2D plane by xform_inv and picking elements.
	current_vector = get_basis_vector_from_bone_axis(primary_result.basis, forward_axis).normalized();
	src_vec2 = get_projection_vector(primary_result.basis.xform_inv(forward_vector_nrm), secondary_rotation_axis).normalized();
	dst_vec2 = get_projection_vector(primary_result.basis.xform_inv(current_vector), secondary_rotation_axis).normalized();
	calculated_angle = src_vec2.angle_to(dst_vec2);

	if (use_angle_limitation) {
		if (symmetry_limitation) {
			limit_angle = secondary_limit_angle * 0.5f;
			damp_threshold = secondary_damp_threshold;
		} else {
			if (signbit(calculated_angle)) {
				limit_angle = secondary_negative_limit_angle;
				damp_threshold = secondary_negative_damp_threshold;
			} else {
				limit_angle = secondary_positive_limit_angle;
				damp_threshold = secondary_positive_damp_threshold;
			}
		}
		if (Math::abs(calculated_angle) > limit_angle) {
			is_within_limitations = false;
		}
		calculated_angle = remap_damped(0, limit_angle, damp_threshold, calculated_angle);
	}

	current_result = current_result.rotated_local(get_vector_from_axis(secondary_rotation_axis), calculated_angle);

	return current_result;
}

void LookAtModifier3D::init_transition() {
	if (Math::is_zero_approx(duration)) {
		return;
	}
	from_q = prev_q;
	remaining = 1.0;
}
