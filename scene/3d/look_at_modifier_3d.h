/**************************************************************************/
/*  look_at_modifier_3d.h                                                 */
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

#ifndef LOOK_AT_MODIFIER_3D_H
#define LOOK_AT_MODIFIER_3D_H

#include "scene/3d/skeleton_modifier_3d.h"
#include "scene/animation/tween.h"

class LookAtModifier3D : public SkeletonModifier3D {
	GDCLASS(LookAtModifier3D, SkeletonModifier3D);

public:
	enum OriginFrom {
		ORIGIN_FROM_SELF,
		ORIGIN_FROM_SPECIFIC_BONE,
		ORIGIN_FROM_EXTERNAL_NODE,
	};

private:
	String bone_name;
	int bone = -1;

	Vector3 forward_vector;
	Vector3 forward_vector_nrm;
	BoneAxis forward_axis = BONE_AXIS_PLUS_Z;
	Vector3::Axis primary_rotation_axis = Vector3::AXIS_Y;
	Vector3::Axis secondary_rotation_axis = Vector3::AXIS_X;
	bool use_secondary_rotation = true;

	OriginFrom origin_from = ORIGIN_FROM_SELF;
	String origin_bone_name;
	int origin_bone = -1;
	NodePath origin_external_node;

	Vector3 origin_offset;
	float origin_safe_margin = 0.1;

	NodePath target_node;

	float duration = 0;
	Tween::TransitionType transition_type = Tween::TRANS_LINEAR;
	Tween::EaseType ease_type = Tween::EASE_IN;

	bool use_angle_limitation = false;
	bool symmetry_limitation = true;

	float primary_limit_angle = Math_TAU;
	float primary_damp_threshold = 1.0f;
	float primary_positive_limit_angle = Math_PI;
	float primary_positive_damp_threshold = 1.0f;
	float primary_negative_limit_angle = Math_PI;
	float primary_negative_damp_threshold = 1.0f;

	float secondary_limit_angle = Math_TAU;
	float secondary_damp_threshold = 1.0f;
	float secondary_positive_limit_angle = Math_PI;
	float secondary_positive_damp_threshold = 1.0f;
	float secondary_negative_limit_angle = Math_PI;
	float secondary_negative_damp_threshold = 1.0f;

	bool is_within_limitations = false;

	// For time-based interpolation.
	Quaternion from_q;
	Quaternion prev_q;

	float remaining = 0;
	float time_step = 1.0;

	float remap_damped(float p_from, float p_to, float p_damp_threshold, float p_value) const;
	double get_bspline_y(const Vector2 &p_from, const Vector2 &p_control, const Vector2 &p_to, double p_x) const;
	bool is_intersecting_axis(const Vector3 &p_prev, const Vector3 &p_current, Vector3::Axis p_flipping_axis, Vector3::Axis p_check_axis, bool p_check_plane = false) const;

	Transform3D look_at_with_axes(const Transform3D &p_rest);
	void init_transition();

protected:
	virtual PackedStringArray get_configuration_warnings() const override;
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

	virtual void _process_modification() override;

public:
	void set_bone_name(const String &p_bone_name);
	String get_bone_name() const;
	void set_bone(int p_bone);
	int get_bone() const;

	void set_forward_axis(BoneAxis p_axis);
	BoneAxis get_forward_axis() const;
	void set_primary_rotation_axis(Vector3::Axis p_axis);
	Vector3::Axis get_primary_rotation_axis() const;
	void set_use_secondary_rotation(bool p_enabled);
	bool is_using_secondary_rotation() const;

	void set_origin_from(OriginFrom p_origin_from);
	OriginFrom get_origin_from() const;
	void set_origin_bone_name(const String &p_bone_name);
	String get_origin_bone_name() const;
	void set_origin_bone(int p_bone);
	int get_origin_bone() const;
	void set_origin_external_node(const NodePath &p_external_node);
	NodePath get_origin_external_node() const;

	void set_origin_offset(const Vector3 &p_offset);
	Vector3 get_origin_offset() const;
	void set_origin_safe_margin(float p_margin);
	float get_origin_safe_margin() const;

	void set_target_node(const NodePath &p_target_node);
	NodePath get_target_node() const;

	void set_duration(float p_duration);
	float get_duration() const;
	void set_transition_type(Tween::TransitionType p_transition_type);
	Tween::TransitionType get_transition_type() const;
	void set_ease_type(Tween::EaseType p_ease_type);
	Tween::EaseType get_ease_type() const;

	void set_use_angle_limitation(bool p_enabled);
	bool is_using_angle_limitation() const;
	void set_symmetry_limitation(bool p_enabled);
	bool is_limitation_symmetry() const;

	void set_primary_limit_angle(float p_angle);
	float get_primary_limit_angle() const;
	void set_primary_damp_threshold(float p_power);
	float get_primary_damp_threshold() const;

	void set_primary_positive_limit_angle(float p_angle);
	float get_primary_positive_limit_angle() const;
	void set_primary_positive_damp_threshold(float p_power);
	float get_primary_positive_damp_threshold() const;
	void set_primary_negative_limit_angle(float p_angle);
	float get_primary_negative_limit_angle() const;
	void set_primary_negative_damp_threshold(float p_power);
	float get_primary_negative_damp_threshold() const;

	void set_secondary_limit_angle(float p_angle);
	float get_secondary_limit_angle() const;
	void set_secondary_damp_threshold(float p_power);
	float get_secondary_damp_threshold() const;

	void set_secondary_positive_limit_angle(float p_angle);
	float get_secondary_positive_limit_angle() const;
	void set_secondary_positive_damp_threshold(float p_power);
	float get_secondary_positive_damp_threshold() const;
	void set_secondary_negative_limit_angle(float p_angle);
	float get_secondary_negative_limit_angle() const;
	void set_secondary_negative_damp_threshold(float p_power);
	float get_secondary_negative_damp_threshold() const;

	float get_interpolation_remaining() const;
	bool is_interpolating() const;
	bool is_target_within_limitation() const;

	static Vector3::Axis get_secondary_rotation_axis(BoneAxis p_forward_axis, Vector3::Axis p_primary_rotation_axis);
	static Vector3 get_basis_vector_from_bone_axis(const Basis &p_basis, BoneAxis p_axis);
	static Vector2 get_projection_vector(const Vector3 &p_vector, Vector3::Axis p_axis);
};

VARIANT_ENUM_CAST(LookAtModifier3D::OriginFrom);

#endif // LOOK_AT_MODIFIER_3D_H
