/**************************************************************************/
/*  physics_body_3d.h                                                     */
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

#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/kinematic_collision_3d.h"
#include "scene/resources/physics_material.h"
#include "servers/physics_server_3d.h"

class PhysicsBody3D : public CollisionObject3D {
	GDCLASS(PhysicsBody3D, CollisionObject3D);

protected:
	static void _bind_methods();
	PhysicsBody3D(PhysicsServer3D::BodyMode p_mode);

	Ref<KinematicCollision3D> motion_cache;

	uint16_t locked_axis = 0;

	Ref<KinematicCollision3D> _move(const Vector3 &p_motion, bool p_test_only = false, real_t p_margin = 0.001, bool p_recovery_as_collision = false, int p_max_collisions = 1);

public:
	bool move_and_collide(const PhysicsServer3D::MotionParameters &p_parameters, PhysicsServer3D::MotionResult &r_result, bool p_test_only = false, bool p_cancel_sliding = true);
	bool test_move(const Transform3D &p_from, const Vector3 &p_motion, const Ref<KinematicCollision3D> &r_collision = Ref<KinematicCollision3D>(), real_t p_margin = 0.001, bool p_recovery_as_collision = false, int p_max_collisions = 1);
	Vector3 get_gravity() const;

	void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock);
	bool get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const;

	virtual Vector3 get_linear_velocity() const;
	virtual Vector3 get_angular_velocity() const;
	virtual real_t get_inverse_mass() const;

	TypedArray<PhysicsBody3D> get_collision_exceptions();
	void add_collision_exception_with(Node *p_node); //must be physicsbody
	void remove_collision_exception_with(Node *p_node);
};
