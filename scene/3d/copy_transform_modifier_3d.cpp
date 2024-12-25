/**************************************************************************/
/*  copy_transform_modifier_3d.cpp                                        */
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

#include "copy_transform_modifier_3d.h"

bool CopyTransformModifier3D::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path.begins_with("settings/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, settings.size(), false);

		if (what == "copy") {
			set_copy_flags(which, static_cast<BitField<TransformFlag>>((int)p_value));
		} else if (what == "axes") {
			set_axis_flags(which, static_cast<BitField<AxisFlag>>((int)p_value));
		} else if (what == "invert") {
			set_invert_flags(which, static_cast<BitField<AxisFlag>>((int)p_value));
		} else if (what == "euler_order") {
			set_euler_order(which, static_cast<EulerOrder>((int)p_value));
		} else if (what == "relative") {
			set_relative(which, p_value);
		} else if (what == "additive") {
			set_additive(which, p_value);
		} else {
			return false;
		}
	}
	return true;
}

bool CopyTransformModifier3D::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path.begins_with("settings/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, settings.size(), false);

		if (what == "copy") {
			r_ret = (int)get_copy_flags(which);
		} else if (what == "axes") {
			r_ret = (int)get_axis_flags(which);
		} else if (what == "invert") {
			r_ret = (int)get_invert_flags(which);
		} else if (what == "euler_order") {
			r_ret = (int)get_euler_order(which);
		} else if (what == "relative") {
			r_ret = is_relative(which);
		} else if (what == "additive") {
			r_ret = is_additive(which);
		} else {
			return false;
		}
	}
	return true;
}

void CopyTransformModifier3D::_validate_property(PropertyInfo &p_property) const {
	PackedStringArray split = p_property.name.split("/");
	if (split.size() == 3 && split[0] == "settings" && split[2] == "euler_order") {
		int which = split[1].to_int();
		if (!is_rotation_copying(which) || !is_using_euler(which)) {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

void CopyTransformModifier3D::_get_property_list(List<PropertyInfo> *p_list) const {
	BoneConstraint3D::get_property_list(p_list);

	for (int i = 0; i < settings.size(); i++) {
		String path = "settings/" + itos(i) + "/";
		p_list->push_back(PropertyInfo(Variant::INT, path + "copy", PROPERTY_HINT_FLAGS, "Position,Rotation,Scale"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "axes", PROPERTY_HINT_FLAGS, "X,Y,Z"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "invert", PROPERTY_HINT_FLAGS, "X,Y,Z"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "euler_order", PROPERTY_HINT_ENUM, "XYZ,XZY,YXZ,YZX,ZXY,ZYX"));
		p_list->push_back(PropertyInfo(Variant::BOOL, path + "relative"));
		p_list->push_back(PropertyInfo(Variant::BOOL, path + "additive"));
	}

	for (PropertyInfo &E : *p_list) {
		_validate_property(E);
	}
}

void CopyTransformModifier3D::_validate_setting(int p_index) {
	settings.write[p_index] = memnew(CopyTransform3DSetting);
}

void CopyTransformModifier3D::set_copy_flags(int p_index, BitField<TransformFlag> p_copy_flags) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->copy_flags = p_copy_flags;
	notify_property_list_changed();
}

BitField<CopyTransformModifier3D::TransformFlag> CopyTransformModifier3D::get_copy_flags(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->copy_flags;
}

void CopyTransformModifier3D::set_axis_flags(int p_index, BitField<AxisFlag> p_axis_flags) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->axis_flags = p_axis_flags;
	notify_property_list_changed();
}

BitField<CopyTransformModifier3D::AxisFlag> CopyTransformModifier3D::get_axis_flags(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->axis_flags;
}

void CopyTransformModifier3D::set_invert_flags(int p_index, BitField<AxisFlag> p_axis_flags) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->invert_flags = p_axis_flags;
	notify_property_list_changed();
}

BitField<CopyTransformModifier3D::AxisFlag> CopyTransformModifier3D::get_invert_flags(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->invert_flags;
}

void CopyTransformModifier3D::set_copy_position(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->copy_flags.set_flag(TRANSFORM_FLAG_POSITION);
	} else {
		setting->copy_flags.clear_flag(TRANSFORM_FLAG_POSITION);
	}
}

bool CopyTransformModifier3D::is_position_copying(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->copy_flags.has_flag(TRANSFORM_FLAG_POSITION);
}

void CopyTransformModifier3D::set_copy_rotation(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->copy_flags.set_flag(TRANSFORM_FLAG_ROTATION);
	} else {
		setting->copy_flags.clear_flag(TRANSFORM_FLAG_ROTATION);
	}
}

bool CopyTransformModifier3D::is_rotation_copying(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->copy_flags.has_flag(TRANSFORM_FLAG_ROTATION);
}

void CopyTransformModifier3D::set_copy_scale(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->copy_flags.set_flag(TRANSFORM_FLAG_SCALE);
	} else {
		setting->copy_flags.clear_flag(TRANSFORM_FLAG_SCALE);
	}
}

bool CopyTransformModifier3D::is_scale_copying(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->copy_flags.has_flag(TRANSFORM_FLAG_SCALE);
}

void CopyTransformModifier3D::set_axis_x_enabled(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->axis_flags.set_flag(AXIS_FLAG_X);
	} else {
		setting->axis_flags.clear_flag(AXIS_FLAG_X);
	}
}

bool CopyTransformModifier3D::is_axis_x_enabled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->axis_flags.has_flag(AXIS_FLAG_X);
}

void CopyTransformModifier3D::set_axis_y_enabled(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->axis_flags.set_flag(AXIS_FLAG_Y);
	} else {
		setting->axis_flags.clear_flag(AXIS_FLAG_Y);
	}
}

bool CopyTransformModifier3D::is_axis_y_enabled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->axis_flags.has_flag(AXIS_FLAG_Y);
}

void CopyTransformModifier3D::set_axis_z_enabled(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->axis_flags.set_flag(AXIS_FLAG_Z);
	} else {
		setting->axis_flags.clear_flag(AXIS_FLAG_Z);
	}
}

bool CopyTransformModifier3D::is_axis_z_enabled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->axis_flags.has_flag(AXIS_FLAG_Z);
}

void CopyTransformModifier3D::set_axis_x_inverted(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->invert_flags.set_flag(AXIS_FLAG_X);
	} else {
		setting->invert_flags.clear_flag(AXIS_FLAG_X);
	}
}

bool CopyTransformModifier3D::is_axis_x_inverted(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->invert_flags.has_flag(AXIS_FLAG_X);
}

void CopyTransformModifier3D::set_axis_y_inverted(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->invert_flags.set_flag(AXIS_FLAG_Y);
	} else {
		setting->invert_flags.clear_flag(AXIS_FLAG_Y);
	}
}

bool CopyTransformModifier3D::is_axis_y_inverted(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->invert_flags.has_flag(AXIS_FLAG_Y);
}

void CopyTransformModifier3D::set_axis_z_inverted(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	if (p_enabled) {
		setting->invert_flags.set_flag(AXIS_FLAG_Z);
	} else {
		setting->invert_flags.clear_flag(AXIS_FLAG_Z);
	}
}

bool CopyTransformModifier3D::is_axis_z_inverted(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->invert_flags.has_flag(AXIS_FLAG_Z);
}

void CopyTransformModifier3D::set_euler_order(int p_index, EulerOrder p_euler_order) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->euler_order = p_euler_order;
}

EulerOrder CopyTransformModifier3D::get_euler_order(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), EulerOrder::YXZ);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->euler_order;
}

void CopyTransformModifier3D::set_relative(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->relative = p_enabled;
}

bool CopyTransformModifier3D::is_relative(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->relative;
}

void CopyTransformModifier3D::set_additive(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	setting->additive = p_enabled;
}

bool CopyTransformModifier3D::is_additive(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return setting->additive;
}

bool CopyTransformModifier3D::is_using_euler(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	return (setting->axis_flags != AXIS_FLAG_ALL && setting->axis_flags != 0) || (setting->invert_flags != AXIS_FLAG_ALL && setting->invert_flags != 0);
}

void CopyTransformModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_copy_flags", "index", "copy_flags"), &CopyTransformModifier3D::set_copy_flags);
	ClassDB::bind_method(D_METHOD("get_copy_flags", "index"), &CopyTransformModifier3D::get_copy_flags);
	ClassDB::bind_method(D_METHOD("set_axis_flags", "index", "axis_flags"), &CopyTransformModifier3D::set_axis_flags);
	ClassDB::bind_method(D_METHOD("get_axis_flags", "index"), &CopyTransformModifier3D::get_axis_flags);
	ClassDB::bind_method(D_METHOD("set_invert_flags", "index", "axis_flags"), &CopyTransformModifier3D::set_invert_flags);
	ClassDB::bind_method(D_METHOD("get_invert_flags", "index"), &CopyTransformModifier3D::get_invert_flags);

	ClassDB::bind_method(D_METHOD("set_copy_position", "index", "enabled"), &CopyTransformModifier3D::set_copy_position);
	ClassDB::bind_method(D_METHOD("is_position_copying", "index"), &CopyTransformModifier3D::is_position_copying);
	ClassDB::bind_method(D_METHOD("set_copy_rotation", "index", "enabled"), &CopyTransformModifier3D::set_copy_rotation);
	ClassDB::bind_method(D_METHOD("is_rotation_copying", "index"), &CopyTransformModifier3D::is_rotation_copying);
	ClassDB::bind_method(D_METHOD("set_copy_scale", "index", "enabled"), &CopyTransformModifier3D::set_copy_scale);
	ClassDB::bind_method(D_METHOD("is_scale_copying", "index"), &CopyTransformModifier3D::is_scale_copying);

	ClassDB::bind_method(D_METHOD("set_axis_x_enabled", "index", "enabled"), &CopyTransformModifier3D::set_axis_x_enabled);
	ClassDB::bind_method(D_METHOD("is_axis_x_enabled", "index"), &CopyTransformModifier3D::is_axis_x_enabled);
	ClassDB::bind_method(D_METHOD("set_axis_y_enabled", "index", "enabled"), &CopyTransformModifier3D::set_axis_y_enabled);
	ClassDB::bind_method(D_METHOD("is_axis_y_enabled", "index"), &CopyTransformModifier3D::is_axis_y_enabled);
	ClassDB::bind_method(D_METHOD("set_axis_z_enabled", "index", "enabled"), &CopyTransformModifier3D::set_axis_z_enabled);
	ClassDB::bind_method(D_METHOD("is_axis_z_enabled", "index"), &CopyTransformModifier3D::is_axis_z_enabled);

	ClassDB::bind_method(D_METHOD("set_axis_x_inverted", "index", "enabled"), &CopyTransformModifier3D::set_axis_x_inverted);
	ClassDB::bind_method(D_METHOD("is_axis_x_inverted", "index"), &CopyTransformModifier3D::is_axis_x_inverted);
	ClassDB::bind_method(D_METHOD("set_axis_y_inverted", "index", "enabled"), &CopyTransformModifier3D::set_axis_y_inverted);
	ClassDB::bind_method(D_METHOD("is_axis_y_inverted", "index"), &CopyTransformModifier3D::is_axis_y_inverted);
	ClassDB::bind_method(D_METHOD("set_axis_z_inverted", "index", "enabled"), &CopyTransformModifier3D::set_axis_z_inverted);
	ClassDB::bind_method(D_METHOD("is_axis_z_inverted", "index"), &CopyTransformModifier3D::is_axis_z_inverted);

	ClassDB::bind_method(D_METHOD("set_euler_order", "index", "euler_order"), &CopyTransformModifier3D::set_euler_order);
	ClassDB::bind_method(D_METHOD("get_euler_order", "index"), &CopyTransformModifier3D::get_euler_order);

	ClassDB::bind_method(D_METHOD("set_relative", "index", "enabled"), &CopyTransformModifier3D::set_relative);
	ClassDB::bind_method(D_METHOD("is_relative", "index"), &CopyTransformModifier3D::is_relative);
	ClassDB::bind_method(D_METHOD("set_additive", "index", "enabled"), &CopyTransformModifier3D::set_additive);
	ClassDB::bind_method(D_METHOD("is_additive", "index"), &CopyTransformModifier3D::is_additive);

	ClassDB::bind_method(D_METHOD("is_using_euler", "index"), &CopyTransformModifier3D::is_using_euler);

	ADD_ARRAY_COUNT("Settings", "setting_count", "set_setting_count", "get_setting_count", "settings/");

	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_POSITION);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_ROTATION);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_SCALE);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_ALL);

	BIND_BITFIELD_FLAG(AXIS_FLAG_X);
	BIND_BITFIELD_FLAG(AXIS_FLAG_Y);
	BIND_BITFIELD_FLAG(AXIS_FLAG_Z);
	BIND_BITFIELD_FLAG(AXIS_FLAG_ALL);
}

void CopyTransformModifier3D::_process_constraint(int p_index, Skeleton3D *p_skeleton, int p_apply_bone, int p_target_bone, float p_amount) {
	CopyTransform3DSetting *setting = static_cast<CopyTransform3DSetting *>(settings[p_index]);
	bool invert_all = setting->invert_flags == AXIS_FLAG_ALL;

	Transform3D destination = p_skeleton->get_bone_pose(p_target_bone);
	if (setting->relative) {
		destination = p_skeleton->get_bone_rest(p_target_bone).affine_inverse() * destination;
	}
	if (invert_all) {
		destination.invert();
	}
	Vector3 dest_pos = destination.origin;
	Quaternion dest_rot = destination.basis.get_rotation_quaternion();
	Vector3 dest_euler = dest_rot.get_euler(setting->euler_order);
	Vector3 dest_scl = destination.basis.get_scale();

	// Process inversion.
	if (!invert_all) {
		for (int i = 0; i < 3; i++) {
			AxisFlag axis = static_cast<AxisFlag>(1 << i);
			if (setting->axis_flags.has_flag(axis) && setting->invert_flags.has_flag(axis)) {
				dest_pos[i] *= -1;
				dest_euler[i] *= -1;
				dest_scl[i] = 1.0 / dest_scl[i];
			}
		}
		dest_rot = Basis::from_euler(dest_euler, setting->euler_order).get_rotation_quaternion();
	}
	destination.origin = dest_pos;
	destination.basis = Basis(dest_rot);
	destination.basis.scale_local(dest_scl);

	if (setting->relative) {
		if (setting->additive) {
			destination = p_skeleton->get_bone_pose(p_apply_bone) * destination;
		} else {
			destination = p_skeleton->get_bone_rest(p_apply_bone) * destination;
		}
	} else if (setting->additive) {
		destination = p_skeleton->get_bone_pose(p_apply_bone) * destination;
	}

	// Process axis mask.
	dest_pos = destination.origin;
	dest_rot = destination.basis.get_rotation_quaternion();
	dest_euler = dest_rot.get_euler(setting->euler_order);
	dest_scl = destination.basis.get_scale();
	Vector3 orig_pos = p_skeleton->get_bone_pose_position(p_apply_bone);
	Quaternion orig_rot = p_skeleton->get_bone_pose_rotation(p_apply_bone);
	Vector3 orig_euler = orig_rot.get_euler(setting->euler_order);
	Vector3 orig_scl = p_skeleton->get_bone_pose_scale(p_apply_bone);
	for (int i = 0; i < 3; i++) {
		if (!setting->axis_flags.has_flag(static_cast<AxisFlag>(1 << i))) {
			dest_pos[i] = orig_pos[i];
			dest_euler[i] = orig_euler[i];
			dest_scl[i] = orig_scl[i];
		}
	}
	if (is_using_euler(p_index)) {
		dest_rot = Basis::from_euler(dest_euler, setting->euler_order).get_rotation_quaternion();
	}
	destination.origin = dest_pos;
	destination.basis = Basis(dest_rot);
	destination.basis.scale_local(dest_scl);

	// Process interpolation depends on the amount.
	destination = p_skeleton->get_bone_pose(p_apply_bone).interpolate_with(destination, p_amount);

	// Apply transform depends on the element mask.
	if (setting->copy_flags.has_flag(TRANSFORM_FLAG_POSITION)) {
		p_skeleton->set_bone_pose_position(p_apply_bone, destination.origin);
	}
	if (setting->copy_flags.has_flag(TRANSFORM_FLAG_ROTATION)) {
		p_skeleton->set_bone_pose_rotation(p_apply_bone, destination.basis.get_rotation_quaternion());
	}
	if (setting->copy_flags.has_flag(TRANSFORM_FLAG_SCALE)) {
		p_skeleton->set_bone_pose_scale(p_apply_bone, destination.basis.get_scale());
	}
}
