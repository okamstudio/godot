/**************************************************************************/
/*  kinematic_collision_3d.h                                              */
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

#pragma once

#include "core/object/ref_counted.h"
#include "servers/physics_server_3d.h"

class KinematicCollision3D : public RefCounted {
	GDCLASS(KinematicCollision3D, RefCounted);

	ObjectID owner_id;
	friend class PhysicsBody3D;
	friend class CharacterBody3D;
	PhysicsServer3D::MotionResult result;

protected:
	static void _bind_methods();

public:
	Vector3 get_travel() const;
	Vector3 get_remainder() const;
	int get_collision_count() const;
	real_t get_depth() const;
	Vector3 get_position(int p_collision_index = 0) const;
	Vector3 get_normal(int p_collision_index = 0) const;
	real_t get_angle(int p_collision_index = 0, const Vector3 &p_up_direction = Vector3(0.0, 1.0, 0.0)) const;
	Object *get_local_shape(int p_collision_index = 0) const;
	Object *get_collider(int p_collision_index = 0) const;
	ObjectID get_collider_id(int p_collision_index = 0) const;
	RID get_collider_rid(int p_collision_index = 0) const;
	Object *get_collider_shape(int p_collision_index = 0) const;
	int get_collider_shape_index(int p_collision_index = 0) const;
	Vector3 get_collider_velocity(int p_collision_index = 0) const;
};
