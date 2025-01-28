/**************************************************************************/
/*  spring_bone_collision_3d.cpp                                          */
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

#include "spring_bone_collision_3d.h"

#include "scene/3d/spring_bone_simulator_3d.h"

PackedStringArray SpringBoneCollision3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	SpringBoneSimulator3D *parent = Object::cast_to<SpringBoneSimulator3D>(get_parent());
	if (!parent) {
		warnings.push_back(RTR("Parent node should be a SpringBoneSimulator3D node."));
	}

	return warnings;
}

void SpringBoneCollision3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "bone_name") {
		Skeleton3D *sk = get_skeleton();
		if (sk) {
			p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
			p_property.hint_string = sk->get_concatenated_bone_names();
		} else {
			p_property.hint = PROPERTY_HINT_NONE;
			p_property.hint_string = "";
		}
	} else if (bone < 0 && (p_property.name == "position_offset" || p_property.name == "rotation_offset")) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

Skeleton3D *SpringBoneCollision3D::get_skeleton() const {
	SpringBoneSimulator3D *parent = Object::cast_to<SpringBoneSimulator3D>(get_parent());
	if (!parent) {
		return nullptr;
	}
	return parent->get_skeleton();
}

void SpringBoneCollision3D::set_bone_name(const String &p_name) {
	bone_name = p_name;
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		set_bone(sk->find_bone(bone_name));
	}
}

String SpringBoneCollision3D::get_bone_name() const {
	return bone_name;
}

void SpringBoneCollision3D::set_bone(int p_bone) {
	bone = p_bone;

	Skeleton3D *sk = get_skeleton();
	if (sk) {
		if (bone <= -1 || bone >= sk->get_bone_count()) {
			WARN_PRINT("Bone index out of range! Cannot connect BoneAttachment to node!");
			bone = -1;
		} else {
			bone_name = sk->get_bone_name(bone);
		}
	}

	notify_property_list_changed();
}

int SpringBoneCollision3D::get_bone() const {
	return bone;
}

void SpringBoneCollision3D::set_position_offset(const Vector3 &p_offset) {
	if (position_offset == p_offset) {
		return;
	}
	position_offset = p_offset;
	sync_pose();
#ifdef TOOLS_ENABLED
	update_gizmos();
#endif // TOOLS_ENABLED
}

Vector3 SpringBoneCollision3D::get_position_offset() const {
	return position_offset;
}

void SpringBoneCollision3D::set_rotation_offset(const Quaternion &p_offset) {
	if (rotation_offset == p_offset) {
		return;
	}
	rotation_offset = p_offset;
	sync_pose();
#ifdef TOOLS_ENABLED
	update_gizmos();
#endif // TOOLS_ENABLED
}

Quaternion SpringBoneCollision3D::get_rotation_offset() const {
	return rotation_offset;
}

void SpringBoneCollision3D::sync_pose() {
	if (bone >= 0) {
		Skeleton3D *sk = get_skeleton();
		if (sk) {
			Transform3D tr = sk->get_global_transform() * sk->get_bone_global_pose(bone);
			tr.origin += tr.basis.get_rotation_quaternion().xform(position_offset);
			tr.basis *= Basis(rotation_offset);
			set_global_transform(tr);
		}
	}
}

Transform3D SpringBoneCollision3D::get_transform_from_skeleton(const Transform3D &p_center) const {
	Transform3D gtr = get_global_transform();
	Skeleton3D *sk = get_skeleton();
	if (sk) {
		Transform3D tr = sk->get_global_transform();
		gtr = tr.affine_inverse() * p_center * gtr;
	}
	return gtr;
}

void SpringBoneCollision3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_skeleton"), &SpringBoneCollision3D::get_skeleton);

	ClassDB::bind_method(D_METHOD("set_bone_name", "bone_name"), &SpringBoneCollision3D::set_bone_name);
	ClassDB::bind_method(D_METHOD("get_bone_name"), &SpringBoneCollision3D::get_bone_name);

	ClassDB::bind_method(D_METHOD("set_bone", "bone"), &SpringBoneCollision3D::set_bone);
	ClassDB::bind_method(D_METHOD("get_bone"), &SpringBoneCollision3D::get_bone);

	ClassDB::bind_method(D_METHOD("set_position_offset", "offset"), &SpringBoneCollision3D::set_position_offset);
	ClassDB::bind_method(D_METHOD("get_position_offset"), &SpringBoneCollision3D::get_position_offset);

	ClassDB::bind_method(D_METHOD("set_rotation_offset", "offset"), &SpringBoneCollision3D::set_rotation_offset);
	ClassDB::bind_method(D_METHOD("get_rotation_offset"), &SpringBoneCollision3D::get_rotation_offset);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bone_name"), "set_bone_name", "get_bone_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_bone", "get_bone");

	ADD_GROUP("Offset", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position_offset"), "set_position_offset", "get_position_offset");
	ADD_PROPERTY(PropertyInfo(Variant::QUATERNION, "rotation_offset"), "set_rotation_offset", "get_rotation_offset");
}

Vector3 SpringBoneCollision3D::collide(const Transform3D &p_center, float p_bone_radius, float p_bone_length, const Vector3 &p_current) const {
	return _collide(p_center, p_bone_radius, p_bone_length, p_current);
}

Vector3 SpringBoneCollision3D::_collide(const Transform3D &p_center, float p_bone_radius, float p_bone_length, const Vector3 &p_current) const {
	return Vector3(0, 0, 0);
}

#ifdef TOOLS_ENABLED
void SpringBoneCollision3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_EDITOR_PRE_SAVE: {
			sync_pose();
		} break;
	}
}
#endif // TOOLS_ENABLED
