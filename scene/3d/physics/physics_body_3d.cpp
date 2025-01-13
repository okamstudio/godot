/**************************************************************************/
/*  physics_body_3d.cpp                                                   */
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

#include "physics_body_3d.h"

void PhysicsBody3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("move_and_collide", "motion", "test_only", "safe_margin", "recovery_as_collision", "max_collisions"), &PhysicsBody3D::_move, DEFVAL(false), DEFVAL(0.001), DEFVAL(false), DEFVAL(1));
	ClassDB::bind_method(D_METHOD("test_move", "from", "motion", "collision", "safe_margin", "recovery_as_collision", "max_collisions"), &PhysicsBody3D::test_move, DEFVAL(Variant()), DEFVAL(0.001), DEFVAL(false), DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_gravity"), &PhysicsBody3D::get_gravity);

	ClassDB::bind_method(D_METHOD("set_axis_lock", "axis", "lock"), &PhysicsBody3D::set_axis_lock);
	ClassDB::bind_method(D_METHOD("get_axis_lock", "axis"), &PhysicsBody3D::get_axis_lock);

	ClassDB::bind_method(D_METHOD("get_collision_exceptions"), &PhysicsBody3D::get_collision_exceptions);
	ClassDB::bind_method(D_METHOD("add_collision_exception_with", "body"), &PhysicsBody3D::add_collision_exception_with);
	ClassDB::bind_method(D_METHOD("remove_collision_exception_with", "body"), &PhysicsBody3D::remove_collision_exception_with);

	ADD_GROUP("Axis Lock", "axis_lock_");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_linear_x"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_X);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_linear_y"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Y);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_linear_z"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_LINEAR_Z);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_angular_x"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_ANGULAR_X);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_angular_y"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_ANGULAR_Y);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "axis_lock_angular_z"), "set_axis_lock", "get_axis_lock", PhysicsServer3D::BODY_AXIS_ANGULAR_Z);
}

PhysicsBody3D::PhysicsBody3D(PhysicsServer3D::BodyMode p_mode) :
		CollisionObject3D(PhysicsServer3D::get_singleton()->body_create(), false) {
	set_body_mode(p_mode);
}

TypedArray<PhysicsBody3D> PhysicsBody3D::get_collision_exceptions() {
	List<RID> exceptions;
	PhysicsServer3D::get_singleton()->body_get_collision_exceptions(get_rid(), &exceptions);
	Array ret;
	for (const RID &body : exceptions) {
		ObjectID instance_id = PhysicsServer3D::get_singleton()->body_get_object_instance_id(body);
		Object *obj = ObjectDB::get_instance(instance_id);
		PhysicsBody3D *physics_body = Object::cast_to<PhysicsBody3D>(obj);
		ret.append(physics_body);
	}
	return ret;
}

void PhysicsBody3D::add_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	CollisionObject3D *collision_object = Object::cast_to<CollisionObject3D>(p_node);
	ERR_FAIL_NULL_MSG(collision_object, "Collision exception only works between two nodes that inherit from CollisionObject3D (such as Area3D or PhysicsBody3D).");
	PhysicsServer3D::get_singleton()->body_add_collision_exception(get_rid(), collision_object->get_rid());
}

void PhysicsBody3D::remove_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	CollisionObject3D *collision_object = Object::cast_to<CollisionObject3D>(p_node);
	ERR_FAIL_NULL_MSG(collision_object, "Collision exception only works between two nodes that inherit from CollisionObject3D (such as Area3D or PhysicsBody3D).");
	PhysicsServer3D::get_singleton()->body_remove_collision_exception(get_rid(), collision_object->get_rid());
}

Ref<KinematicCollision3D> PhysicsBody3D::_move(const Vector3 &p_motion, bool p_test_only, real_t p_margin, bool p_recovery_as_collision, int p_max_collisions) {
	PhysicsServer3D::MotionParameters parameters(get_global_transform(), p_motion, p_margin);
	parameters.max_collisions = p_max_collisions;
	parameters.recovery_as_collision = p_recovery_as_collision;

	PhysicsServer3D::MotionResult result;

	if (move_and_collide(parameters, result, p_test_only)) {
		// Create a new instance when the cached reference is invalid or still in use in script.
		if (motion_cache.is_null() || motion_cache->get_reference_count() > 1) {
			motion_cache.instantiate();
			motion_cache->owner_id = get_instance_id();
		}

		motion_cache->result = result;

		return motion_cache;
	}

	return Ref<KinematicCollision3D>();
}

bool PhysicsBody3D::move_and_collide(const PhysicsServer3D::MotionParameters &p_parameters, PhysicsServer3D::MotionResult &r_result, bool p_test_only, bool p_cancel_sliding) {
	bool colliding = PhysicsServer3D::get_singleton()->body_test_motion(get_rid(), p_parameters, &r_result);

	// Restore direction of motion to be along original motion,
	// in order to avoid sliding due to recovery,
	// but only if collision depth is low enough to avoid tunneling.
	if (p_cancel_sliding) {
		real_t motion_length = p_parameters.motion.length();
		real_t precision = 0.001;

		if (colliding) {
			// Can't just use margin as a threshold because collision depth is calculated on unsafe motion,
			// so even in normal resting cases the depth can be a bit more than the margin.
			precision += motion_length * (r_result.collision_unsafe_fraction - r_result.collision_safe_fraction);

			if (r_result.collisions[0].depth > p_parameters.margin + precision) {
				p_cancel_sliding = false;
			}
		}

		if (p_cancel_sliding) {
			// When motion is null, recovery is the resulting motion.
			Vector3 motion_normal;
			if (motion_length > CMP_EPSILON) {
				motion_normal = p_parameters.motion / motion_length;
			}

			// Check depth of recovery.
			real_t projected_length = r_result.travel.dot(motion_normal);
			Vector3 recovery = r_result.travel - motion_normal * projected_length;
			real_t recovery_length = recovery.length();
			// Fixes cases where canceling slide causes the motion to go too deep into the ground,
			// because we're only taking rest information into account and not general recovery.
			if (recovery_length < p_parameters.margin + precision) {
				// Apply adjustment to motion.
				r_result.travel = motion_normal * projected_length;
				r_result.remainder = p_parameters.motion - r_result.travel;
			}
		}
	}

	for (int i = 0; i < 3; i++) {
		if (locked_axis & (1 << i)) {
			r_result.travel[i] = 0;
		}
	}

	if (!p_test_only) {
		Transform3D gt = p_parameters.from;
		gt.origin += r_result.travel;
		set_global_transform(gt);
	}

	return colliding;
}

bool PhysicsBody3D::test_move(const Transform3D &p_from, const Vector3 &p_motion, const Ref<KinematicCollision3D> &r_collision, real_t p_margin, bool p_recovery_as_collision, int p_max_collisions) {
	ERR_FAIL_COND_V(!is_inside_tree(), false);

	PhysicsServer3D::MotionResult *r = nullptr;
	PhysicsServer3D::MotionResult temp_result;
	if (r_collision.is_valid()) {
		r = &r_collision->result;
	} else {
		r = &temp_result;
	}

	PhysicsServer3D::MotionParameters parameters(p_from, p_motion, p_margin);
	parameters.recovery_as_collision = p_recovery_as_collision;
	parameters.max_collisions = p_max_collisions;

	return PhysicsServer3D::get_singleton()->body_test_motion(get_rid(), parameters, r);
}

Vector3 PhysicsBody3D::get_gravity() const {
	PhysicsDirectBodyState3D *state = PhysicsServer3D::get_singleton()->body_get_direct_state(get_rid());
	ERR_FAIL_NULL_V(state, Vector3());
	return state->get_total_gravity();
}

void PhysicsBody3D::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
	if (p_lock) {
		locked_axis |= p_axis;
	} else {
		locked_axis &= (~p_axis);
	}
	PhysicsServer3D::get_singleton()->body_set_axis_lock(get_rid(), p_axis, p_lock);
}

bool PhysicsBody3D::get_axis_lock(PhysicsServer3D::BodyAxis p_axis) const {
	return (locked_axis & p_axis);
}

Vector3 PhysicsBody3D::get_linear_velocity() const {
	return Vector3();
}

Vector3 PhysicsBody3D::get_angular_velocity() const {
	return Vector3();
}

real_t PhysicsBody3D::get_inverse_mass() const {
	return 0;
}

///////////////////////////////////////

//so, if you pass 45 as limit, avoid numerical precision errors when angle is 45.

///////////////////////////////////////

///////////////////////////////////////
