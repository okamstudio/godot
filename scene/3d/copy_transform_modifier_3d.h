/**************************************************************************/
/*  copy_transform_modifier_3d.h                                          */
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

#ifndef COPY_TRANSFORM_MODIFIER_3D_H
#define COPY_TRANSFORM_MODIFIER_3D_H

#include "scene/3d/bone_constraint_3d.h"

class CopyTransformModifier3D : public BoneConstraint3D {
	GDCLASS(CopyTransformModifier3D, BoneConstraint3D);

public:
	enum TransformFlag {
		TRANSFORM_FLAG_POSITION = 1,
		TRANSFORM_FLAG_ROTATION = 2,
		TRANSFORM_FLAG_SCALE = 4,
		TRANSFORM_FLAG_ALL = TRANSFORM_FLAG_POSITION | TRANSFORM_FLAG_ROTATION | TRANSFORM_FLAG_SCALE,
	};

	enum AxisFlag {
		AXIS_FLAG_X = 1,
		AXIS_FLAG_Y = 2,
		AXIS_FLAG_Z = 4,
		AXIS_FLAG_ALL = AXIS_FLAG_X | AXIS_FLAG_Y | AXIS_FLAG_Z,
	};

	struct CopyTransform3DSetting : public BoneConstraint3DSetting {
		BitField<TransformFlag> copy_flags = TRANSFORM_FLAG_ALL;
		BitField<AxisFlag> axis_flags = AXIS_FLAG_ALL;
		BitField<AxisFlag> invert_flags = 0;
		EulerOrder euler_order = EulerOrder::YXZ;
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
	void set_copy_flags(int p_index, BitField<TransformFlag> p_copy_flags);
	BitField<TransformFlag> get_copy_flags(int p_index) const;

	void set_copy_position(int p_index, bool p_enabled);
	bool is_position_copying(int p_index) const;
	void set_copy_rotation(int p_index, bool p_enabled);
	bool is_rotation_copying(int p_index) const;
	void set_copy_scale(int p_index, bool p_enabled);
	bool is_scale_copying(int p_index) const;

	void set_axis_flags(int p_index, BitField<AxisFlag> p_axis_flags);
	BitField<AxisFlag> get_axis_flags(int p_index) const;

	void set_axis_x_enabled(int p_index, bool p_enabled);
	bool is_axis_x_enabled(int p_index) const;
	void set_axis_y_enabled(int p_index, bool p_enabled);
	bool is_axis_y_enabled(int p_index) const;
	void set_axis_z_enabled(int p_index, bool p_enabled);
	bool is_axis_z_enabled(int p_index) const;

	void set_invert_flags(int p_index, BitField<AxisFlag> p_axis_flags);
	BitField<AxisFlag> get_invert_flags(int p_index) const;

	void set_axis_x_inverted(int p_index, bool p_enabled);
	bool is_axis_x_inverted(int p_index) const;
	void set_axis_y_inverted(int p_index, bool p_enabled);
	bool is_axis_y_inverted(int p_index) const;
	void set_axis_z_inverted(int p_index, bool p_enabled);
	bool is_axis_z_inverted(int p_index) const;

	void set_euler_order(int p_index, EulerOrder p_euler_order);
	EulerOrder get_euler_order(int p_index) const;

	void set_relative(int p_index, bool p_enabled);
	bool is_relative(int p_index) const;

	void set_additive(int p_index, bool p_enabled);
	bool is_additive(int p_index) const;

	bool is_using_euler(int p_index) const; // Helper.
};

VARIANT_BITFIELD_CAST(CopyTransformModifier3D::TransformFlag);
VARIANT_BITFIELD_CAST(CopyTransformModifier3D::AxisFlag);

#endif // COPY_TRANSFORM_MODIFIER_3D_H
