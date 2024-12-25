/**************************************************************************/
/*  convert_transform_modifier_3d.h                                       */
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

#ifndef CONVERT_TRANSFORM_MODIFIER_3D_H
#define CONVERT_TRANSFORM_MODIFIER_3D_H

#include "scene/3d/bone_constraint_3d.h"

class ConvertTransformModifier3D : public BoneConstraint3D {
	GDCLASS(ConvertTransformModifier3D, BoneConstraint3D);

public:
	enum TransformMode {
		TRANSFORM_MODE_POSITION,
		TRANSFORM_MODE_ROTATION,
		TRANSFORM_MODE_SCALE,
	};

	struct ConvertTransform3DSetting : public BoneConstraint3DSetting {
		TransformMode apply_transform_mode = TRANSFORM_MODE_POSITION;
		Vector3::Axis apply_axis = Vector3::AXIS_X;
		float apply_range_min = 0.0;
		float apply_range_max = 0.0;
		bool apply_use_euler = false;
		EulerOrder apply_euler_order = EulerOrder::YXZ;

		TransformMode target_transform_mode = TRANSFORM_MODE_POSITION;
		Vector3::Axis target_axis = Vector3::AXIS_X;
		float target_range_min = 0.0;
		float target_range_max = 0.0;
		bool target_use_euler = false;
		EulerOrder target_euler_order = EulerOrder::YXZ;

		bool relative = true;
		bool additive = false;
	};

protected:
	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	void _validate_property(PropertyInfo &p_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	virtual void _process_constraint(int p_index, Skeleton3D *p_skeleton, int p_apply_bone, int p_target_bone, float p_amount) override;
	virtual void _validate_setting(int p_index) override;

public:
	void set_apply_transform_mode(int p_index, TransformMode p_transform_mode);
	TransformMode get_apply_transform_mode(int p_index) const;
	void set_apply_axis(int p_index, Vector3::Axis p_axis);
	Vector3::Axis get_apply_axis(int p_index) const;
	void set_apply_range_min(int p_index, float p_range_min);
	float get_apply_range_min(int p_index) const;
	void set_apply_range_max(int p_index, float p_range_max);
	float get_apply_range_max(int p_index) const;
	void set_apply_use_euler(int p_index, bool p_enabled);
	bool is_apply_using_euler(int p_index) const;
	void set_apply_euler_order(int p_index, EulerOrder p_euler_order);
	EulerOrder get_apply_euler_order(int p_index) const;

	void set_target_transform_mode(int p_index, TransformMode p_transform_mode);
	TransformMode get_target_transform_mode(int p_index) const;
	void set_target_axis(int p_index, Vector3::Axis p_axis);
	Vector3::Axis get_target_axis(int p_index) const;
	void set_target_range_min(int p_index, float p_range_min);
	float get_target_range_min(int p_index) const;
	void set_target_range_max(int p_index, float p_range_max);
	float get_target_range_max(int p_index) const;
	void set_target_use_euler(int p_index, bool p_enabled);
	bool is_target_using_euler(int p_index) const;
	void set_target_euler_order(int p_index, EulerOrder p_euler_order);
	EulerOrder get_target_euler_order(int p_index) const;

	void set_relative(int p_index, bool p_enabled);
	bool is_relative(int p_index) const;

	void set_additive(int p_index, bool p_enabled);
	bool is_additive(int p_index) const;

	static float symmetrize_angle(float p_angle); // Helper to make angle 0->TAU become -PI->PI.
	static float get_roll_angle(const Quaternion &p_rotation, const Vector3 &p_roll_axis);
};

VARIANT_ENUM_CAST(ConvertTransformModifier3D::TransformMode);

#endif // CONVERT_TRANSFORM_MODIFIER_3D_H
