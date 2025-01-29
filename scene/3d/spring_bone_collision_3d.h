/**************************************************************************/
/*  spring_bone_collision_3d.h                                            */
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

#ifndef SPRING_BONE_COLLISION_3D_H
#define SPRING_BONE_COLLISION_3D_H

#include "scene/3d/skeleton_3d.h"

class SpringBoneCollision3D : public Node3D {
	GDCLASS(SpringBoneCollision3D, Node3D);

	String bone_name;
	int bone = -1;

	Vector3 position_offset;
	Quaternion rotation_offset;

protected:
	PackedStringArray get_configuration_warnings() const override;

	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();
#ifdef TOOLS_ENABLED
	virtual void _notification(int p_what);
#endif // TOOLS_ENABLED

	virtual Vector3 _collide(const Transform3D &p_center, float p_bone_radius, float p_bone_length, const Vector3 &p_current) const;

public:
	Skeleton3D *get_skeleton() const;

	void set_bone_name(const String &p_name);
	String get_bone_name() const;
	void set_bone(int p_bone);
	int get_bone() const;

	void set_position_offset(const Vector3 &p_offset);
	Vector3 get_position_offset() const;
	void set_rotation_offset(const Quaternion &p_offset);
	Quaternion get_rotation_offset() const;

	void sync_pose();
	Transform3D get_transform_from_skeleton(const Transform3D &p_center) const;

	Vector3 collide(const Transform3D &p_center, float p_bone_radius, float p_bone_length, const Vector3 &p_current) const;
};

#endif // SPRING_BONE_COLLISION_3D_H
