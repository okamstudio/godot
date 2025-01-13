/**************************************************************************/
/*  character_body_2d.cpp                                                 */
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

#include "character_body_2d.h"

// So, if you pass 45 as limit, avoid numerical precision errors when angle is 45.
#define FLOOR_ANGLE_THRESHOLD 0.01

bool CharacterBody2D::move_and_slide() {
	// Hack in order to work with calling from _process as well as from _physics_process; calling from thread is risky.
	double delta = Engine::get_singleton()->is_in_physics_frame() ? get_physics_process_delta_time() : get_process_delta_time();

	Vector2 current_platform_velocity = platform_velocity;
	Transform2D gt = get_global_transform();
	previous_position = gt.columns[2];

	if ((on_floor || on_wall) && platform_rid.is_valid()) {
		bool excluded = false;
		if (on_floor) {
			excluded = (platform_floor_layers & platform_layer) == 0;
		} else if (on_wall) {
			excluded = (platform_wall_layers & platform_layer) == 0;
		}
		if (!excluded) {
			//this approach makes sure there is less delay between the actual body velocity and the one we saved
			PhysicsDirectBodyState2D *bs = PhysicsServer2D::get_singleton()->body_get_direct_state(platform_rid);
			if (bs) {
				Vector2 local_position = gt.columns[2] - bs->get_transform().columns[2];
				current_platform_velocity = bs->get_velocity_at_local_position(local_position);
			} else {
				// Body is removed or destroyed, invalidate floor.
				current_platform_velocity = Vector2();
				platform_rid = RID();
			}
		} else {
			current_platform_velocity = Vector2();
		}
	}

	motion_results.clear();
	last_motion = Vector2();

	bool was_on_floor = on_floor;
	on_floor = false;
	on_ceiling = false;
	on_wall = false;

	if (!current_platform_velocity.is_zero_approx()) {
		PhysicsServer2D::MotionParameters parameters(get_global_transform(), current_platform_velocity * delta, margin);
		parameters.recovery_as_collision = true; // Also report collisions generated only from recovery.
		parameters.exclude_bodies.insert(platform_rid);
		if (platform_object_id.is_valid()) {
			parameters.exclude_objects.insert(platform_object_id);
		}

		PhysicsServer2D::MotionResult floor_result;
		if (move_and_collide(parameters, floor_result, false, false)) {
			motion_results.push_back(floor_result);
			_set_collision_direction(floor_result);
		}
	}

	if (motion_mode == MOTION_MODE_GROUNDED) {
		_move_and_slide_grounded(delta, was_on_floor);
	} else {
		_move_and_slide_floating(delta);
	}

	// Compute real velocity.
	real_velocity = get_position_delta() / delta;

	if (platform_on_leave != PLATFORM_ON_LEAVE_DO_NOTHING) {
		// Add last platform velocity when just left a moving platform.
		if (!on_floor && !on_wall) {
			if (platform_on_leave == PLATFORM_ON_LEAVE_ADD_UPWARD_VELOCITY && current_platform_velocity.dot(up_direction) < 0) {
				current_platform_velocity = current_platform_velocity.slide(up_direction);
			}
			velocity += current_platform_velocity;
		}
	}

	return motion_results.size() > 0;
}

void CharacterBody2D::_move_and_slide_grounded(double p_delta, bool p_was_on_floor) {
	Vector2 motion = velocity * p_delta;
	Vector2 motion_slide_up = motion.slide(up_direction);

	Vector2 prev_floor_normal = floor_normal;

	platform_rid = RID();
	platform_object_id = ObjectID();
	floor_normal = Vector2();
	platform_velocity = Vector2();

	// No sliding on first attempt to keep floor motion stable when possible,
	// When stop on slope is enabled or when there is no up direction.
	bool sliding_enabled = !floor_stop_on_slope;
	// Constant speed can be applied only the first time sliding is enabled.
	bool can_apply_constant_speed = sliding_enabled;
	// If the platform's ceiling push down the body.
	bool apply_ceiling_velocity = false;
	bool first_slide = true;
	bool vel_dir_facing_up = velocity.dot(up_direction) > 0;
	Vector2 last_travel;

	for (int iteration = 0; iteration < max_slides; ++iteration) {
		PhysicsServer2D::MotionParameters parameters(get_global_transform(), motion, margin);
		parameters.recovery_as_collision = true; // Also report collisions generated only from recovery.

		Vector2 prev_position = parameters.from.columns[2];

		PhysicsServer2D::MotionResult result;
		bool collided = move_and_collide(parameters, result, false, !sliding_enabled);

		last_motion = result.travel;

		if (collided) {
			motion_results.push_back(result);
			_set_collision_direction(result);

			// If we hit a ceiling platform, we set the vertical velocity to at least the platform one.
			if (on_ceiling && result.collider_velocity != Vector2() && result.collider_velocity.dot(up_direction) < 0) {
				// If ceiling sliding is on, only apply when the ceiling is flat or when the motion is upward.
				if (!slide_on_ceiling || motion.dot(up_direction) < 0 || (result.collision_normal + up_direction).length() < 0.01) {
					apply_ceiling_velocity = true;
					Vector2 ceiling_vertical_velocity = up_direction * up_direction.dot(result.collider_velocity);
					Vector2 motion_vertical_velocity = up_direction * up_direction.dot(velocity);
					if (motion_vertical_velocity.dot(up_direction) > 0 || ceiling_vertical_velocity.length_squared() > motion_vertical_velocity.length_squared()) {
						velocity = ceiling_vertical_velocity + velocity.slide(up_direction);
					}
				}
			}

			if (on_floor && floor_stop_on_slope && (velocity.normalized() + up_direction).length() < 0.01) {
				Transform2D gt = get_global_transform();
				if (result.travel.length() <= margin + CMP_EPSILON) {
					gt.columns[2] -= result.travel;
				}
				set_global_transform(gt);
				velocity = Vector2();
				last_motion = Vector2();
				motion = Vector2();
				break;
			}

			if (result.remainder.is_zero_approx()) {
				motion = Vector2();
				break;
			}

			// Move on floor only checks.
			if (floor_block_on_wall && on_wall && motion_slide_up.dot(result.collision_normal) <= 0) {
				// Avoid to move forward on a wall if floor_block_on_wall is true.
				if (p_was_on_floor && !on_floor && !vel_dir_facing_up) {
					// If the movement is large the body can be prevented from reaching the walls.
					if (result.travel.length() <= margin + CMP_EPSILON) {
						// Cancels the motion.
						Transform2D gt = get_global_transform();
						gt.columns[2] -= result.travel;
						set_global_transform(gt);
					}
					// Determines if you are on the ground.
					_snap_on_floor(true, false, true);
					velocity = Vector2();
					last_motion = Vector2();
					motion = Vector2();
					break;
				}
				// Prevents the body from being able to climb a slope when it moves forward against the wall.
				else if (!on_floor) {
					motion = up_direction * up_direction.dot(result.remainder);
					motion = motion.slide(result.collision_normal);
				} else {
					motion = result.remainder;
				}
			}
			// Constant Speed when the slope is upward.
			else if (floor_constant_speed && is_on_floor_only() && can_apply_constant_speed && p_was_on_floor && motion.dot(result.collision_normal) < 0) {
				can_apply_constant_speed = false;
				Vector2 motion_slide_norm = result.remainder.slide(result.collision_normal).normalized();
				motion = motion_slide_norm * (motion_slide_up.length() - result.travel.slide(up_direction).length() - last_travel.slide(up_direction).length());
			}
			// Regular sliding, the last part of the test handle the case when you don't want to slide on the ceiling.
			else if ((sliding_enabled || !on_floor) && (!on_ceiling || slide_on_ceiling || !vel_dir_facing_up) && !apply_ceiling_velocity) {
				Vector2 slide_motion = result.remainder.slide(result.collision_normal);
				if (slide_motion.dot(velocity) > 0.0) {
					motion = slide_motion;
				} else {
					motion = Vector2();
				}
				if (slide_on_ceiling && on_ceiling) {
					// Apply slide only in the direction of the input motion, otherwise just stop to avoid jittering when moving against a wall.
					if (vel_dir_facing_up) {
						velocity = velocity.slide(result.collision_normal);
					} else {
						// Avoid acceleration in slope when falling.
						velocity = up_direction * up_direction.dot(velocity);
					}
				}
			}
			// No sliding on first attempt to keep floor motion stable when possible.
			else {
				motion = result.remainder;
				if (on_ceiling && !slide_on_ceiling && vel_dir_facing_up) {
					velocity = velocity.slide(up_direction);
					motion = motion.slide(up_direction);
				}
			}

			last_travel = result.travel;
		}
		// When you move forward in a downward slope you don’t collide because you will be in the air.
		// This test ensures that constant speed is applied, only if the player is still on the ground after the snap is applied.
		else if (floor_constant_speed && first_slide && _on_floor_if_snapped(p_was_on_floor, vel_dir_facing_up)) {
			can_apply_constant_speed = false;
			sliding_enabled = true;
			Transform2D gt = get_global_transform();
			gt.columns[2] = prev_position;
			set_global_transform(gt);

			Vector2 motion_slide_norm = motion.slide(prev_floor_normal).normalized();
			motion = motion_slide_norm * (motion_slide_up.length());
			collided = true;
		}

		can_apply_constant_speed = !can_apply_constant_speed && !sliding_enabled;
		sliding_enabled = true;
		first_slide = false;

		if (!collided || motion.is_zero_approx()) {
			break;
		}
	}

	_snap_on_floor(p_was_on_floor, vel_dir_facing_up);

	// Scales the horizontal velocity according to the wall slope.
	if (is_on_wall_only() && motion_slide_up.dot(motion_results.get(0).collision_normal) < 0) {
		Vector2 slide_motion = velocity.slide(motion_results.get(0).collision_normal);
		if (motion_slide_up.dot(slide_motion) < 0) {
			velocity = up_direction * up_direction.dot(velocity);
		} else {
			// Keeps the vertical motion from velocity and add the horizontal motion of the projection.
			velocity = up_direction * up_direction.dot(velocity) + slide_motion.slide(up_direction);
		}
	}

	// Reset the gravity accumulation when touching the ground.
	if (on_floor && !vel_dir_facing_up) {
		velocity = velocity.slide(up_direction);
	}
}

void CharacterBody2D::_move_and_slide_floating(double p_delta) {
	Vector2 motion = velocity * p_delta;

	platform_rid = RID();
	platform_object_id = ObjectID();
	floor_normal = Vector2();
	platform_velocity = Vector2();

	bool first_slide = true;
	for (int iteration = 0; iteration < max_slides; ++iteration) {
		PhysicsServer2D::MotionParameters parameters(get_global_transform(), motion, margin);
		parameters.recovery_as_collision = true; // Also report collisions generated only from recovery.

		PhysicsServer2D::MotionResult result;
		bool collided = move_and_collide(parameters, result, false, false);

		last_motion = result.travel;

		if (collided) {
			motion_results.push_back(result);
			_set_collision_direction(result);

			if (result.remainder.is_zero_approx()) {
				motion = Vector2();
				break;
			}

			if (wall_min_slide_angle != 0 && result.get_angle(-velocity.normalized()) < wall_min_slide_angle + FLOOR_ANGLE_THRESHOLD) {
				motion = Vector2();
			} else if (first_slide) {
				Vector2 motion_slide_norm = result.remainder.slide(result.collision_normal).normalized();
				motion = motion_slide_norm * (motion.length() - result.travel.length());
			} else {
				motion = result.remainder.slide(result.collision_normal);
			}

			if (motion.dot(velocity) <= 0.0) {
				motion = Vector2();
			}
		}

		if (!collided || motion.is_zero_approx()) {
			break;
		}

		first_slide = false;
	}
}
void CharacterBody2D::apply_floor_snap() {
	_apply_floor_snap();
}

// Method that avoids the p_wall_as_floor parameter for the public method.
void CharacterBody2D::_apply_floor_snap(bool p_wall_as_floor) {
	if (on_floor) {
		return;
	}

	// Snap by at least collision margin to keep floor state consistent.
	real_t length = MAX(floor_snap_length, margin);

	PhysicsServer2D::MotionParameters parameters(get_global_transform(), -up_direction * length, margin);
	parameters.recovery_as_collision = true; // Also report collisions generated only from recovery.
	parameters.collide_separation_ray = true;

	PhysicsServer2D::MotionResult result;
	if (move_and_collide(parameters, result, true, false)) {
		if ((result.get_angle(up_direction) <= floor_max_angle + FLOOR_ANGLE_THRESHOLD) ||
				(p_wall_as_floor && result.get_angle(-up_direction) > floor_max_angle + FLOOR_ANGLE_THRESHOLD)) {
			on_floor = true;
			floor_normal = result.collision_normal;
			_set_platform_data(result);

			// Ensure that we only move the body along the up axis, because
			// move_and_collide may stray the object a bit when getting it unstuck.
			// Canceling this motion should not affect move_and_slide, as previous
			// calls to move_and_collide already took care of freeing the body.
			if (result.travel.length() > margin) {
				result.travel = up_direction * up_direction.dot(result.travel);
			} else {
				result.travel = Vector2();
			}

			parameters.from.columns[2] += result.travel;
			set_global_transform(parameters.from);
		}
	}
}

void CharacterBody2D::_snap_on_floor(bool p_was_on_floor, bool p_vel_dir_facing_up, bool p_wall_as_floor) {
	if (on_floor || !p_was_on_floor || p_vel_dir_facing_up) {
		return;
	}

	_apply_floor_snap(p_wall_as_floor);
}

bool CharacterBody2D::_on_floor_if_snapped(bool p_was_on_floor, bool p_vel_dir_facing_up) {
	if (up_direction == Vector2() || on_floor || !p_was_on_floor || p_vel_dir_facing_up) {
		return false;
	}

	// Snap by at least collision margin to keep floor state consistent.
	real_t length = MAX(floor_snap_length, margin);

	PhysicsServer2D::MotionParameters parameters(get_global_transform(), -up_direction * length, margin);
	parameters.recovery_as_collision = true; // Also report collisions generated only from recovery.
	parameters.collide_separation_ray = true;

	PhysicsServer2D::MotionResult result;
	if (move_and_collide(parameters, result, true, false)) {
		if (result.get_angle(up_direction) <= floor_max_angle + FLOOR_ANGLE_THRESHOLD) {
			return true;
		}
	}

	return false;
}

void CharacterBody2D::_set_collision_direction(const PhysicsServer2D::MotionResult &p_result) {
	if (motion_mode == MOTION_MODE_GROUNDED && p_result.get_angle(up_direction) <= floor_max_angle + FLOOR_ANGLE_THRESHOLD) { //floor
		on_floor = true;
		floor_normal = p_result.collision_normal;
		_set_platform_data(p_result);
	} else if (motion_mode == MOTION_MODE_GROUNDED && p_result.get_angle(-up_direction) <= floor_max_angle + FLOOR_ANGLE_THRESHOLD) { //ceiling
		on_ceiling = true;
	} else {
		on_wall = true;
		wall_normal = p_result.collision_normal;
		// Don't apply wall velocity when the collider is a CharacterBody2D.
		if (Object::cast_to<CharacterBody2D>(ObjectDB::get_instance(p_result.collider_id)) == nullptr) {
			_set_platform_data(p_result);
		}
	}
}

void CharacterBody2D::_set_platform_data(const PhysicsServer2D::MotionResult &p_result) {
	platform_rid = p_result.collider;
	platform_object_id = p_result.collider_id;
	platform_velocity = p_result.collider_velocity;
	platform_layer = PhysicsServer2D::get_singleton()->body_get_collision_layer(platform_rid);
}

const Vector2 &CharacterBody2D::get_velocity() const {
	return velocity;
}

void CharacterBody2D::set_velocity(const Vector2 &p_velocity) {
	velocity = p_velocity;
}

bool CharacterBody2D::is_on_floor() const {
	return on_floor;
}

bool CharacterBody2D::is_on_floor_only() const {
	return on_floor && !on_wall && !on_ceiling;
}

bool CharacterBody2D::is_on_wall() const {
	return on_wall;
}

bool CharacterBody2D::is_on_wall_only() const {
	return on_wall && !on_floor && !on_ceiling;
}

bool CharacterBody2D::is_on_ceiling() const {
	return on_ceiling;
}

bool CharacterBody2D::is_on_ceiling_only() const {
	return on_ceiling && !on_floor && !on_wall;
}

const Vector2 &CharacterBody2D::get_floor_normal() const {
	return floor_normal;
}

const Vector2 &CharacterBody2D::get_wall_normal() const {
	return wall_normal;
}

const Vector2 &CharacterBody2D::get_last_motion() const {
	return last_motion;
}

Vector2 CharacterBody2D::get_position_delta() const {
	return get_global_transform().columns[2] - previous_position;
}

const Vector2 &CharacterBody2D::get_real_velocity() const {
	return real_velocity;
}

real_t CharacterBody2D::get_floor_angle(const Vector2 &p_up_direction) const {
	ERR_FAIL_COND_V(p_up_direction == Vector2(), 0);
	return Math::acos(floor_normal.dot(p_up_direction));
}

const Vector2 &CharacterBody2D::get_platform_velocity() const {
	return platform_velocity;
}

int CharacterBody2D::get_slide_collision_count() const {
	return motion_results.size();
}

PhysicsServer2D::MotionResult CharacterBody2D::get_slide_collision(int p_bounce) const {
	ERR_FAIL_INDEX_V(p_bounce, motion_results.size(), PhysicsServer2D::MotionResult());
	return motion_results[p_bounce];
}

Ref<KinematicCollision2D> CharacterBody2D::_get_slide_collision(int p_bounce) {
	ERR_FAIL_INDEX_V(p_bounce, motion_results.size(), Ref<KinematicCollision2D>());
	if (p_bounce >= slide_colliders.size()) {
		slide_colliders.resize(p_bounce + 1);
	}

	// Create a new instance when the cached reference is invalid or still in use in script.
	if (slide_colliders[p_bounce].is_null() || slide_colliders[p_bounce]->get_reference_count() > 1) {
		slide_colliders.write[p_bounce].instantiate();
		slide_colliders.write[p_bounce]->owner_id = get_instance_id();
	}

	slide_colliders.write[p_bounce]->result = motion_results[p_bounce];
	return slide_colliders[p_bounce];
}

Ref<KinematicCollision2D> CharacterBody2D::_get_last_slide_collision() {
	if (motion_results.size() == 0) {
		return Ref<KinematicCollision2D>();
	}
	return _get_slide_collision(motion_results.size() - 1);
}

void CharacterBody2D::set_safe_margin(real_t p_margin) {
	margin = p_margin;
}

real_t CharacterBody2D::get_safe_margin() const {
	return margin;
}

bool CharacterBody2D::is_floor_stop_on_slope_enabled() const {
	return floor_stop_on_slope;
}

void CharacterBody2D::set_floor_stop_on_slope_enabled(bool p_enabled) {
	floor_stop_on_slope = p_enabled;
}

bool CharacterBody2D::is_floor_constant_speed_enabled() const {
	return floor_constant_speed;
}

void CharacterBody2D::set_floor_constant_speed_enabled(bool p_enabled) {
	floor_constant_speed = p_enabled;
}

bool CharacterBody2D::is_floor_block_on_wall_enabled() const {
	return floor_block_on_wall;
}

void CharacterBody2D::set_floor_block_on_wall_enabled(bool p_enabled) {
	floor_block_on_wall = p_enabled;
}

bool CharacterBody2D::is_slide_on_ceiling_enabled() const {
	return slide_on_ceiling;
}

void CharacterBody2D::set_slide_on_ceiling_enabled(bool p_enabled) {
	slide_on_ceiling = p_enabled;
}

uint32_t CharacterBody2D::get_platform_floor_layers() const {
	return platform_floor_layers;
}

void CharacterBody2D::set_platform_floor_layers(uint32_t p_exclude_layers) {
	platform_floor_layers = p_exclude_layers;
}

uint32_t CharacterBody2D::get_platform_wall_layers() const {
	return platform_wall_layers;
}

void CharacterBody2D::set_platform_wall_layers(uint32_t p_exclude_layers) {
	platform_wall_layers = p_exclude_layers;
}

void CharacterBody2D::set_motion_mode(MotionMode p_mode) {
	motion_mode = p_mode;
}

CharacterBody2D::MotionMode CharacterBody2D::get_motion_mode() const {
	return motion_mode;
}

void CharacterBody2D::set_platform_on_leave(PlatformOnLeave p_on_leave_apply_velocity) {
	platform_on_leave = p_on_leave_apply_velocity;
}

CharacterBody2D::PlatformOnLeave CharacterBody2D::get_platform_on_leave() const {
	return platform_on_leave;
}

int CharacterBody2D::get_max_slides() const {
	return max_slides;
}

void CharacterBody2D::set_max_slides(int p_max_slides) {
	ERR_FAIL_COND(p_max_slides < 1);
	max_slides = p_max_slides;
}

real_t CharacterBody2D::get_floor_max_angle() const {
	return floor_max_angle;
}

void CharacterBody2D::set_floor_max_angle(real_t p_radians) {
	floor_max_angle = p_radians;
}

real_t CharacterBody2D::get_floor_snap_length() {
	return floor_snap_length;
}

void CharacterBody2D::set_floor_snap_length(real_t p_floor_snap_length) {
	ERR_FAIL_COND(p_floor_snap_length < 0);
	floor_snap_length = p_floor_snap_length;
}

real_t CharacterBody2D::get_wall_min_slide_angle() const {
	return wall_min_slide_angle;
}

void CharacterBody2D::set_wall_min_slide_angle(real_t p_radians) {
	wall_min_slide_angle = p_radians;
}

const Vector2 &CharacterBody2D::get_up_direction() const {
	return up_direction;
}

void CharacterBody2D::set_up_direction(const Vector2 &p_up_direction) {
	ERR_FAIL_COND_MSG(p_up_direction == Vector2(), "up_direction can't be equal to Vector2.ZERO, consider using Floating motion mode instead.");
	up_direction = p_up_direction.normalized();
}

void CharacterBody2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Reset move_and_slide() data.
			on_floor = false;
			platform_rid = RID();
			platform_object_id = ObjectID();
			on_ceiling = false;
			on_wall = false;
			motion_results.clear();
			platform_velocity = Vector2();
		} break;
	}
}

void CharacterBody2D::_validate_property(PropertyInfo &p_property) const {
	if (motion_mode == MOTION_MODE_FLOATING) {
		if (p_property.name.begins_with("floor_") || p_property.name == "up_direction" || p_property.name == "slide_on_ceiling") {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else {
		if (p_property.name == "wall_min_slide_angle") {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}
}

void CharacterBody2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("move_and_slide"), &CharacterBody2D::move_and_slide);
	ClassDB::bind_method(D_METHOD("apply_floor_snap"), &CharacterBody2D::apply_floor_snap);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &CharacterBody2D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &CharacterBody2D::get_velocity);

	ClassDB::bind_method(D_METHOD("set_safe_margin", "margin"), &CharacterBody2D::set_safe_margin);
	ClassDB::bind_method(D_METHOD("get_safe_margin"), &CharacterBody2D::get_safe_margin);
	ClassDB::bind_method(D_METHOD("is_floor_stop_on_slope_enabled"), &CharacterBody2D::is_floor_stop_on_slope_enabled);
	ClassDB::bind_method(D_METHOD("set_floor_stop_on_slope_enabled", "enabled"), &CharacterBody2D::set_floor_stop_on_slope_enabled);
	ClassDB::bind_method(D_METHOD("set_floor_constant_speed_enabled", "enabled"), &CharacterBody2D::set_floor_constant_speed_enabled);
	ClassDB::bind_method(D_METHOD("is_floor_constant_speed_enabled"), &CharacterBody2D::is_floor_constant_speed_enabled);
	ClassDB::bind_method(D_METHOD("set_floor_block_on_wall_enabled", "enabled"), &CharacterBody2D::set_floor_block_on_wall_enabled);
	ClassDB::bind_method(D_METHOD("is_floor_block_on_wall_enabled"), &CharacterBody2D::is_floor_block_on_wall_enabled);
	ClassDB::bind_method(D_METHOD("set_slide_on_ceiling_enabled", "enabled"), &CharacterBody2D::set_slide_on_ceiling_enabled);
	ClassDB::bind_method(D_METHOD("is_slide_on_ceiling_enabled"), &CharacterBody2D::is_slide_on_ceiling_enabled);

	ClassDB::bind_method(D_METHOD("set_platform_floor_layers", "exclude_layer"), &CharacterBody2D::set_platform_floor_layers);
	ClassDB::bind_method(D_METHOD("get_platform_floor_layers"), &CharacterBody2D::get_platform_floor_layers);
	ClassDB::bind_method(D_METHOD("set_platform_wall_layers", "exclude_layer"), &CharacterBody2D::set_platform_wall_layers);
	ClassDB::bind_method(D_METHOD("get_platform_wall_layers"), &CharacterBody2D::get_platform_wall_layers);

	ClassDB::bind_method(D_METHOD("get_max_slides"), &CharacterBody2D::get_max_slides);
	ClassDB::bind_method(D_METHOD("set_max_slides", "max_slides"), &CharacterBody2D::set_max_slides);
	ClassDB::bind_method(D_METHOD("get_floor_max_angle"), &CharacterBody2D::get_floor_max_angle);
	ClassDB::bind_method(D_METHOD("set_floor_max_angle", "radians"), &CharacterBody2D::set_floor_max_angle);
	ClassDB::bind_method(D_METHOD("get_floor_snap_length"), &CharacterBody2D::get_floor_snap_length);
	ClassDB::bind_method(D_METHOD("set_floor_snap_length", "floor_snap_length"), &CharacterBody2D::set_floor_snap_length);
	ClassDB::bind_method(D_METHOD("get_wall_min_slide_angle"), &CharacterBody2D::get_wall_min_slide_angle);
	ClassDB::bind_method(D_METHOD("set_wall_min_slide_angle", "radians"), &CharacterBody2D::set_wall_min_slide_angle);
	ClassDB::bind_method(D_METHOD("get_up_direction"), &CharacterBody2D::get_up_direction);
	ClassDB::bind_method(D_METHOD("set_up_direction", "up_direction"), &CharacterBody2D::set_up_direction);
	ClassDB::bind_method(D_METHOD("set_motion_mode", "mode"), &CharacterBody2D::set_motion_mode);
	ClassDB::bind_method(D_METHOD("get_motion_mode"), &CharacterBody2D::get_motion_mode);
	ClassDB::bind_method(D_METHOD("set_platform_on_leave", "on_leave_apply_velocity"), &CharacterBody2D::set_platform_on_leave);
	ClassDB::bind_method(D_METHOD("get_platform_on_leave"), &CharacterBody2D::get_platform_on_leave);

	ClassDB::bind_method(D_METHOD("is_on_floor"), &CharacterBody2D::is_on_floor);
	ClassDB::bind_method(D_METHOD("is_on_floor_only"), &CharacterBody2D::is_on_floor_only);
	ClassDB::bind_method(D_METHOD("is_on_ceiling"), &CharacterBody2D::is_on_ceiling);
	ClassDB::bind_method(D_METHOD("is_on_ceiling_only"), &CharacterBody2D::is_on_ceiling_only);
	ClassDB::bind_method(D_METHOD("is_on_wall"), &CharacterBody2D::is_on_wall);
	ClassDB::bind_method(D_METHOD("is_on_wall_only"), &CharacterBody2D::is_on_wall_only);
	ClassDB::bind_method(D_METHOD("get_floor_normal"), &CharacterBody2D::get_floor_normal);
	ClassDB::bind_method(D_METHOD("get_wall_normal"), &CharacterBody2D::get_wall_normal);
	ClassDB::bind_method(D_METHOD("get_last_motion"), &CharacterBody2D::get_last_motion);
	ClassDB::bind_method(D_METHOD("get_position_delta"), &CharacterBody2D::get_position_delta);
	ClassDB::bind_method(D_METHOD("get_real_velocity"), &CharacterBody2D::get_real_velocity);
	ClassDB::bind_method(D_METHOD("get_floor_angle", "up_direction"), &CharacterBody2D::get_floor_angle, DEFVAL(Vector2(0.0, -1.0)));
	ClassDB::bind_method(D_METHOD("get_platform_velocity"), &CharacterBody2D::get_platform_velocity);
	ClassDB::bind_method(D_METHOD("get_slide_collision_count"), &CharacterBody2D::get_slide_collision_count);
	ClassDB::bind_method(D_METHOD("get_slide_collision", "slide_idx"), &CharacterBody2D::_get_slide_collision);
	ClassDB::bind_method(D_METHOD("get_last_slide_collision"), &CharacterBody2D::_get_last_slide_collision);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "motion_mode", PROPERTY_HINT_ENUM, "Grounded,Floating", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_motion_mode", "get_motion_mode");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "up_direction"), "set_up_direction", "get_up_direction");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity", PROPERTY_HINT_NONE, "suffix:px/s", PROPERTY_USAGE_NO_EDITOR), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "slide_on_ceiling"), "set_slide_on_ceiling_enabled", "is_slide_on_ceiling_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_slides", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_max_slides", "get_max_slides");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_min_slide_angle", PROPERTY_HINT_RANGE, "0,180,0.1,radians_as_degrees", PROPERTY_USAGE_DEFAULT), "set_wall_min_slide_angle", "get_wall_min_slide_angle");

	ADD_GROUP("Floor", "floor_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "floor_stop_on_slope"), "set_floor_stop_on_slope_enabled", "is_floor_stop_on_slope_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "floor_constant_speed"), "set_floor_constant_speed_enabled", "is_floor_constant_speed_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "floor_block_on_wall"), "set_floor_block_on_wall_enabled", "is_floor_block_on_wall_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "floor_max_angle", PROPERTY_HINT_RANGE, "0,180,0.1,radians_as_degrees"), "set_floor_max_angle", "get_floor_max_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "floor_snap_length", PROPERTY_HINT_RANGE, "0,32,0.1,or_greater,suffix:px"), "set_floor_snap_length", "get_floor_snap_length");

	ADD_GROUP("Moving Platform", "platform_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "platform_on_leave", PROPERTY_HINT_ENUM, "Add Velocity,Add Upward Velocity,Do Nothing", PROPERTY_USAGE_DEFAULT), "set_platform_on_leave", "get_platform_on_leave");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "platform_floor_layers", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_platform_floor_layers", "get_platform_floor_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "platform_wall_layers", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_platform_wall_layers", "get_platform_wall_layers");

	ADD_GROUP("Collision", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "safe_margin", PROPERTY_HINT_RANGE, "0.001,256,0.001,suffix:px"), "set_safe_margin", "get_safe_margin");

	BIND_ENUM_CONSTANT(MOTION_MODE_GROUNDED);
	BIND_ENUM_CONSTANT(MOTION_MODE_FLOATING);

	BIND_ENUM_CONSTANT(PLATFORM_ON_LEAVE_ADD_VELOCITY);
	BIND_ENUM_CONSTANT(PLATFORM_ON_LEAVE_ADD_UPWARD_VELOCITY);
	BIND_ENUM_CONSTANT(PLATFORM_ON_LEAVE_DO_NOTHING);
}

CharacterBody2D::CharacterBody2D() :
		PhysicsBody2D(PhysicsServer2D::BODY_MODE_KINEMATIC) {
}
