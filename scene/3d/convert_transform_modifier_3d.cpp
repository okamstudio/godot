/**************************************************************************/
/*  convert_transform_modifier_3d.cpp                                     */
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

#include "convert_transform_modifier_3d.h"

constexpr const char *HINT_POSITION = "-10,10,0.01,or_greater,or_less,suffix:m";
constexpr const char *HINT_ROTATION = "-180,180,0.01,radians_as_degrees";
constexpr const char *HINT_SCALE = "0,10,0.01,or_greater";

bool ConvertTransformModifier3D::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path.begins_with("settings/")) {
		int which = path.get_slicec('/', 1).to_int();
		String where = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, settings.size(), false);
		String what = path.get_slicec('/', 3);

		if (where == "apply") {
			if (what == "transform_mode") {
				set_apply_transform_mode(which, static_cast<TransformMode>((int)p_value));
			} else if (what == "axis") {
				set_apply_axis(which, static_cast<Vector3::Axis>((int)p_value));
			} else if (what == "range_min") {
				set_apply_range_min(which, p_value);
			} else if (what == "range_max") {
				set_apply_range_max(which, p_value);
			} else if (what == "use_euler") {
				set_apply_use_euler(which, p_value);
			} else if (what == "euler_order") {
				set_apply_euler_order(which, static_cast<EulerOrder>((int)p_value));
			} else {
				return false;
			}
		} else if (where == "target") {
			if (what == "transform_mode") {
				set_target_transform_mode(which, static_cast<TransformMode>((int)p_value));
			} else if (what == "axis") {
				set_target_axis(which, static_cast<Vector3::Axis>((int)p_value));
			} else if (what == "range_min") {
				set_target_range_min(which, p_value);
			} else if (what == "range_max") {
				set_target_range_max(which, p_value);
			} else if (what == "use_euler") {
				set_target_use_euler(which, p_value);
			} else if (what == "euler_order") {
				set_target_euler_order(which, static_cast<EulerOrder>((int)p_value));
			} else {
				return false;
			}
		} else if (where == "relative") {
			set_relative(which, p_value);
		} else if (where == "additive") {
			set_additive(which, p_value);
		} else {
			return false;
		}
	}
	return true;
}

bool ConvertTransformModifier3D::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path.begins_with("settings/")) {
		int which = path.get_slicec('/', 1).to_int();
		String where = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, settings.size(), false);
		String what = path.get_slicec('/', 3);

		if (where == "apply") {
			if (what == "transform_mode") {
				r_ret = (int)get_apply_transform_mode(which);
			} else if (what == "axis") {
				r_ret = (int)get_apply_axis(which);
			} else if (what == "range_min") {
				r_ret = get_apply_range_min(which);
			} else if (what == "range_max") {
				r_ret = get_apply_range_max(which);
			} else if (what == "use_euler") {
				r_ret = is_apply_using_euler(which);
			} else if (what == "euler_order") {
				r_ret = (int)get_apply_euler_order(which);
			} else {
				return false;
			}
		} else if (where == "target") {
			if (what == "transform_mode") {
				r_ret = (int)get_target_transform_mode(which);
			} else if (what == "axis") {
				r_ret = (int)get_target_axis(which);
			} else if (what == "range_min") {
				r_ret = get_target_range_min(which);
			} else if (what == "range_max") {
				r_ret = get_target_range_max(which);
			} else if (what == "use_euler") {
				r_ret = is_target_using_euler(which);
			} else if (what == "euler_order") {
				r_ret = (int)get_target_euler_order(which);
			} else {
				return false;
			}
		} else if (where == "relative") {
			r_ret = is_relative(which);
		} else if (where == "additive") {
			r_ret = is_additive(which);
		} else {
			return false;
		}
	}
	return true;
}

void ConvertTransformModifier3D::_validate_property(PropertyInfo &p_property) const {
	PackedStringArray split = p_property.name.split("/");
	if (split.size() == 4 && split[0] == "settings") {
		int which = split[1].to_int();
		bool hide = false;
		if (split[2] == "apply") {
			if (split[3] == "range_min" || split[3] == "range_max") {
				if (get_apply_transform_mode(which) == TRANSFORM_MODE_POSITION) {
					p_property.hint_string = HINT_POSITION;
				} else if (get_apply_transform_mode(which) == TRANSFORM_MODE_ROTATION) {
					p_property.hint_string = HINT_ROTATION;
				} else {
					p_property.hint_string = HINT_SCALE;
				}
			} else if (split[3] == "use_euler") {
				hide = get_apply_transform_mode(which) != TRANSFORM_MODE_ROTATION;
			} else if (split[3] == "euler_order") {
				hide = !is_apply_using_euler(which) || get_apply_transform_mode(which) != TRANSFORM_MODE_ROTATION;
			}
		} else if (split[2] == "target") {
			if (split[3] == "range_min" || split[3] == "range_max") {
				if (get_target_transform_mode(which) == TRANSFORM_MODE_POSITION) {
					p_property.hint_string = HINT_POSITION;
				} else if (get_target_transform_mode(which) == TRANSFORM_MODE_ROTATION) {
					p_property.hint_string = HINT_ROTATION;
				} else {
					p_property.hint_string = HINT_SCALE;
				}
			} else if (split[3] == "use_euler") {
				hide = get_target_transform_mode(which) != TRANSFORM_MODE_ROTATION;
			} else if (split[3] == "euler_order") {
				hide = !is_target_using_euler(which) || get_target_transform_mode(which) != TRANSFORM_MODE_ROTATION;
			}
		}
		if (hide) {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

void ConvertTransformModifier3D::_get_property_list(List<PropertyInfo> *p_list) const {
	BoneConstraint3D::get_property_list(p_list);

	for (int i = 0; i < settings.size(); i++) {
		String path = "settings/" + itos(i) + "/";

		p_list->push_back(PropertyInfo(Variant::INT, path + "apply/transform_mode", PROPERTY_HINT_ENUM, "Position,Rotation,Scale"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "apply/axis", PROPERTY_HINT_ENUM, "X,Y,Z"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, path + "apply/range_min", PROPERTY_HINT_RANGE, HINT_POSITION));
		p_list->push_back(PropertyInfo(Variant::FLOAT, path + "apply/range_max", PROPERTY_HINT_RANGE, HINT_POSITION));
		p_list->push_back(PropertyInfo(Variant::BOOL, path + "apply/use_euler"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "apply/euler_order", PROPERTY_HINT_ENUM, "XYZ,XZY,YXZ,YZX,ZXY,ZYX"));

		p_list->push_back(PropertyInfo(Variant::INT, path + "target/transform_mode", PROPERTY_HINT_ENUM, "Position,Rotation,Scale"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "target/axis", PROPERTY_HINT_ENUM, "X,Y,Z"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, path + "target/range_min", PROPERTY_HINT_RANGE, HINT_POSITION));
		p_list->push_back(PropertyInfo(Variant::FLOAT, path + "target/range_max", PROPERTY_HINT_RANGE, HINT_POSITION));
		p_list->push_back(PropertyInfo(Variant::BOOL, path + "target/use_euler"));
		p_list->push_back(PropertyInfo(Variant::INT, path + "target/euler_order", PROPERTY_HINT_ENUM, "XYZ,XZY,YXZ,YZX,ZXY,ZYX"));

		p_list->push_back(PropertyInfo(Variant::BOOL, path + "relative"));
		p_list->push_back(PropertyInfo(Variant::BOOL, path + "additive"));
	}

	for (PropertyInfo &E : *p_list) {
		_validate_property(E);
	}
}

void ConvertTransformModifier3D::_validate_setting(int p_index) {
	settings.write[p_index] = memnew(ConvertTransform3DSetting);
}

void ConvertTransformModifier3D::set_apply_transform_mode(int p_index, TransformMode p_transform_mode) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_transform_mode = p_transform_mode;
	notify_property_list_changed();
}

ConvertTransformModifier3D::TransformMode ConvertTransformModifier3D::get_apply_transform_mode(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), TRANSFORM_MODE_POSITION);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_transform_mode;
}

void ConvertTransformModifier3D::set_apply_axis(int p_index, Vector3::Axis p_axis) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_axis = p_axis;
}

Vector3::Axis ConvertTransformModifier3D::get_apply_axis(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), Vector3::AXIS_X);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_axis;
}

void ConvertTransformModifier3D::set_apply_range_min(int p_index, float p_range_min) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_range_min = p_range_min;
}

float ConvertTransformModifier3D::get_apply_range_min(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_range_min;
}

void ConvertTransformModifier3D::set_apply_range_max(int p_index, float p_range_max) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_range_max = p_range_max;
}

float ConvertTransformModifier3D::get_apply_range_max(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_range_max;
}

void ConvertTransformModifier3D::set_apply_use_euler(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_use_euler = p_enabled;
	notify_property_list_changed();
}

bool ConvertTransformModifier3D::is_apply_using_euler(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_use_euler;
}

void ConvertTransformModifier3D::set_apply_euler_order(int p_index, EulerOrder p_euler_order) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->apply_euler_order = p_euler_order;
}

EulerOrder ConvertTransformModifier3D::get_apply_euler_order(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), EulerOrder::YXZ);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->apply_euler_order;
}

void ConvertTransformModifier3D::set_target_transform_mode(int p_index, TransformMode p_transform_mode) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_transform_mode = p_transform_mode;
	notify_property_list_changed();
}

ConvertTransformModifier3D::TransformMode ConvertTransformModifier3D::get_target_transform_mode(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), TRANSFORM_MODE_POSITION);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_transform_mode;
}

void ConvertTransformModifier3D::set_target_axis(int p_index, Vector3::Axis p_axis) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_axis = p_axis;
}

Vector3::Axis ConvertTransformModifier3D::get_target_axis(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), Vector3::AXIS_X);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_axis;
}

void ConvertTransformModifier3D::set_target_range_min(int p_index, float p_range_min) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_range_min = p_range_min;
}

float ConvertTransformModifier3D::get_target_range_min(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_range_min;
}

void ConvertTransformModifier3D::set_target_range_max(int p_index, float p_range_max) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_range_max = p_range_max;
}

float ConvertTransformModifier3D::get_target_range_max(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_range_max;
}

void ConvertTransformModifier3D::set_target_use_euler(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_use_euler = p_enabled;
	notify_property_list_changed();
}

bool ConvertTransformModifier3D::is_target_using_euler(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), false);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_use_euler;
}

void ConvertTransformModifier3D::set_target_euler_order(int p_index, EulerOrder p_euler_order) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->target_euler_order = p_euler_order;
}

EulerOrder ConvertTransformModifier3D::get_target_euler_order(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), EulerOrder::YXZ);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->target_euler_order;
}

void ConvertTransformModifier3D::set_relative(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->relative = p_enabled;
}

bool ConvertTransformModifier3D::is_relative(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->relative;
}

void ConvertTransformModifier3D::set_additive(int p_index, bool p_enabled) {
	ERR_FAIL_INDEX(p_index, settings.size());
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	setting->additive = p_enabled;
}

bool ConvertTransformModifier3D::is_additive(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, settings.size(), 0);
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);
	return setting->additive;
}

void ConvertTransformModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_apply_transform_mode", "index", "transform_mode"), &ConvertTransformModifier3D::set_apply_transform_mode);
	ClassDB::bind_method(D_METHOD("get_apply_transform_mode", "index"), &ConvertTransformModifier3D::get_apply_transform_mode);
	ClassDB::bind_method(D_METHOD("set_apply_axis", "index", "axis"), &ConvertTransformModifier3D::set_apply_axis);
	ClassDB::bind_method(D_METHOD("get_apply_axis", "index"), &ConvertTransformModifier3D::get_apply_axis);
	ClassDB::bind_method(D_METHOD("set_apply_range_min", "index", "range_min"), &ConvertTransformModifier3D::set_apply_range_min);
	ClassDB::bind_method(D_METHOD("get_apply_range_min", "index"), &ConvertTransformModifier3D::get_apply_range_min);
	ClassDB::bind_method(D_METHOD("set_apply_range_max", "index", "range_max"), &ConvertTransformModifier3D::set_apply_range_max);
	ClassDB::bind_method(D_METHOD("get_apply_range_max", "index"), &ConvertTransformModifier3D::get_apply_range_max);
	ClassDB::bind_method(D_METHOD("set_apply_use_euler", "index", "enabled"), &ConvertTransformModifier3D::set_apply_use_euler);
	ClassDB::bind_method(D_METHOD("is_apply_using_euler", "index"), &ConvertTransformModifier3D::is_apply_using_euler);
	ClassDB::bind_method(D_METHOD("set_apply_euler_order", "index", "euler_order"), &ConvertTransformModifier3D::set_apply_euler_order);
	ClassDB::bind_method(D_METHOD("get_apply_euler_order", "index"), &ConvertTransformModifier3D::get_apply_euler_order);

	ClassDB::bind_method(D_METHOD("set_target_transform_mode", "index", "transform_mode"), &ConvertTransformModifier3D::set_target_transform_mode);
	ClassDB::bind_method(D_METHOD("get_target_transform_mode", "index"), &ConvertTransformModifier3D::get_target_transform_mode);
	ClassDB::bind_method(D_METHOD("set_target_axis", "index", "axis"), &ConvertTransformModifier3D::set_target_axis);
	ClassDB::bind_method(D_METHOD("get_target_axis", "index"), &ConvertTransformModifier3D::get_target_axis);
	ClassDB::bind_method(D_METHOD("set_target_range_min", "index", "range_min"), &ConvertTransformModifier3D::set_target_range_min);
	ClassDB::bind_method(D_METHOD("get_target_range_min", "index"), &ConvertTransformModifier3D::get_target_range_min);
	ClassDB::bind_method(D_METHOD("set_target_range_max", "index", "range_max"), &ConvertTransformModifier3D::set_target_range_max);
	ClassDB::bind_method(D_METHOD("get_target_range_max", "index"), &ConvertTransformModifier3D::get_target_range_max);
	ClassDB::bind_method(D_METHOD("set_target_use_euler", "index", "enabled"), &ConvertTransformModifier3D::set_target_use_euler);
	ClassDB::bind_method(D_METHOD("is_target_using_euler", "index"), &ConvertTransformModifier3D::is_target_using_euler);
	ClassDB::bind_method(D_METHOD("set_target_euler_order", "index", "euler_order"), &ConvertTransformModifier3D::set_target_euler_order);
	ClassDB::bind_method(D_METHOD("get_target_euler_order", "index"), &ConvertTransformModifier3D::get_target_euler_order);

	ClassDB::bind_method(D_METHOD("set_relative", "index", "enabled"), &ConvertTransformModifier3D::set_relative);
	ClassDB::bind_method(D_METHOD("is_relative", "index"), &ConvertTransformModifier3D::is_relative);
	ClassDB::bind_method(D_METHOD("set_additive", "index", "enabled"), &ConvertTransformModifier3D::set_additive);
	ClassDB::bind_method(D_METHOD("is_additive", "index"), &ConvertTransformModifier3D::is_additive);

	ADD_ARRAY_COUNT("Settings", "setting_count", "set_setting_count", "get_setting_count", "settings/");

	BIND_ENUM_CONSTANT(TRANSFORM_MODE_POSITION);
	BIND_ENUM_CONSTANT(TRANSFORM_MODE_ROTATION);
	BIND_ENUM_CONSTANT(TRANSFORM_MODE_SCALE);
}

void ConvertTransformModifier3D::_process_constraint(int p_index, Skeleton3D *p_skeleton, int p_apply_bone, int p_target_bone, float p_amount) {
	ConvertTransform3DSetting *setting = static_cast<ConvertTransform3DSetting *>(settings[p_index]);

	Transform3D destination = p_skeleton->get_bone_pose(p_target_bone);
	if (setting->relative) {
		destination = p_skeleton->get_bone_rest(p_target_bone).affine_inverse() * destination;
	}

	// Retrieve point from target.
	float point = 0.0;
	int axis = (int)setting->target_axis;
	switch (setting->target_transform_mode) {
		case TRANSFORM_MODE_POSITION: {
			point = destination.origin[axis];
		} break;
		case TRANSFORM_MODE_ROTATION: {
			if (setting->target_use_euler) {
				Basis tgt_rot = destination.basis.orthonormalized();
				point = tgt_rot.get_euler(setting->target_euler_order)[axis];
			} else {
				// Axis-angle.
				Quaternion tgt_rot = destination.basis.get_rotation_quaternion();
				point = get_roll_angle(tgt_rot, get_vector_from_axis(setting->target_axis));
			}
			point = symmetrize_angle(point);
		} break;
		case TRANSFORM_MODE_SCALE: {
			point = destination.basis.get_scale()[axis];
		} break;
	}

	// Convert point to apply.
	destination = p_skeleton->get_bone_pose(p_apply_bone);
	point = Math::inverse_lerp(setting->target_range_min, setting->target_range_max, point);
	point = Math::lerp(setting->apply_range_min, setting->apply_range_max, CLAMP(point, 0, 1));
	axis = (int)setting->apply_axis;
	switch (setting->apply_transform_mode) {
		case TRANSFORM_MODE_POSITION: {
			if (setting->relative) {
				if (setting->additive) {
					point = p_skeleton->get_bone_pose(p_apply_bone).origin[axis] + point;
				} else {
					point = p_skeleton->get_bone_rest(p_apply_bone).origin[axis] + point;
				}
			} else if (setting->additive) {
				point = p_skeleton->get_bone_pose(p_apply_bone).origin[axis] + point;
			}
			destination.origin[axis] = point;
		} break;
		case TRANSFORM_MODE_ROTATION: {
			if (setting->apply_use_euler) {
				Vector3 dest_euler = destination.basis.get_euler(setting->apply_euler_order);
				if (setting->relative) {
					if (setting->additive) {
						point = p_skeleton->get_bone_pose(p_apply_bone).basis.get_euler(setting->apply_euler_order)[axis] + point;
					} else {
						point = p_skeleton->get_bone_rest(p_apply_bone).basis.get_euler(setting->apply_euler_order)[axis] + point;
					}
				} else if (setting->additive) {
					point = p_skeleton->get_bone_pose(p_apply_bone).basis.get_euler(setting->apply_euler_order)[axis] + point;
				}
				dest_euler[axis] = point;
				destination.basis.set_euler(dest_euler, setting->apply_euler_order);
			} else {
				// Axis-angle.
				Vector3 rot_axis = get_vector_from_axis(setting->apply_axis);
				if (setting->relative) {
					if (setting->additive) {
						point = get_roll_angle(p_skeleton->get_bone_pose(p_apply_bone).basis.get_rotation_quaternion(), rot_axis) + point;
					} else {
						point = get_roll_angle(p_skeleton->get_bone_rest(p_apply_bone).basis.get_rotation_quaternion(), rot_axis) + point;
					}
				} else if (setting->additive) {
					point = get_roll_angle(p_skeleton->get_bone_pose(p_apply_bone).basis.get_rotation_quaternion(), rot_axis) + point;
				}
				// Scale may not have meaning, but it might affect when it is negative.
				Vector3 dest_scl = destination.basis.get_scale();
				destination.basis.orthonormalize();
				destination.basis.set_axis_angle(rot_axis, point);
				destination.basis.scale_local(dest_scl);
			}
		} break;
		case TRANSFORM_MODE_SCALE: {
			Vector3 dest_scl = destination.basis.get_scale();
			if (setting->relative) {
				if (setting->additive) {
					point = p_skeleton->get_bone_pose(p_apply_bone).basis.get_scale()[axis] + point;
				} else {
					point = p_skeleton->get_bone_rest(p_apply_bone).basis.get_scale()[axis] + point;
				}
			} else if (setting->additive) {
				point = p_skeleton->get_bone_pose(p_apply_bone).basis.get_scale()[axis] + point;
			}
			dest_scl[axis] = point;
			destination.basis = destination.basis.orthonormalized().scaled_local(dest_scl);
		} break;
	}

	// Process interpolation depends on the amount.
	destination = p_skeleton->get_bone_pose(p_apply_bone).interpolate_with(destination, p_amount);

	// Apply transform depends on the mode.
	switch (setting->apply_transform_mode) {
		case TRANSFORM_MODE_POSITION: {
			p_skeleton->set_bone_pose_position(p_apply_bone, destination.origin);
		} break;
		case TRANSFORM_MODE_ROTATION: {
			p_skeleton->set_bone_pose_rotation(p_apply_bone, destination.basis.get_rotation_quaternion());
		} break;
		case TRANSFORM_MODE_SCALE: {
			p_skeleton->set_bone_pose_scale(p_apply_bone, destination.basis.get_scale());
		} break;
	}
}

float ConvertTransformModifier3D::symmetrize_angle(float p_angle) {
	float angle = Math::fposmod(p_angle, (float)Math_TAU);
	return angle > (float)Math_PI ? angle - (float)Math_TAU : angle;
}

float ConvertTransformModifier3D::get_roll_angle(const Quaternion &p_rotation, const Vector3 &p_roll_axis) {
	Vector3 u = Vector3(p_rotation.x, p_rotation.y, p_rotation.z).normalized();
	Vector3 v = p_roll_axis.normalized();
	double dot = u.dot(v);
	Vector3 proj = dot * v;
	proj.normalize();
	double theta = 2.0 * Math::acos(p_rotation.w);
	double alignment = proj.dot(v);
	if (alignment < 0) {
		theta = -theta; // Reverse the angle if misaligned.
	}
	return theta;
}
