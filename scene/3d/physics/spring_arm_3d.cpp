/**************************************************************************/
/*  spring_arm_3d.cpp                                                     */
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

#include "spring_arm_3d.h"

#include "scene/3d/camera_3d.h"
#include "scene/resources/3d/shape_3d.h"

void SpringArm3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(false);
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			process_spring();
		} break;
	}
}

void SpringArm3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_hit_length"), &SpringArm3D::get_hit_length);

	ClassDB::bind_method(D_METHOD("set_length", "length"), &SpringArm3D::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &SpringArm3D::get_length);

	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &SpringArm3D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &SpringArm3D::get_shape);

	ClassDB::bind_method(D_METHOD("add_excluded_object", "RID"), &SpringArm3D::add_excluded_object);
	ClassDB::bind_method(D_METHOD("remove_excluded_object", "RID"), &SpringArm3D::remove_excluded_object);
	ClassDB::bind_method(D_METHOD("clear_excluded_objects"), &SpringArm3D::clear_excluded_objects);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &SpringArm3D::set_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &SpringArm3D::get_mask);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &SpringArm3D::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &SpringArm3D::get_margin);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_length", PROPERTY_HINT_NONE, "suffix:m"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_NONE, "suffix:m"), "set_margin", "get_margin");
}

real_t SpringArm3D::get_length() const {
	return spring_length;
}

void SpringArm3D::set_length(real_t p_length) {
	if (is_inside_tree() && (Engine::get_singleton()->is_editor_hint() || get_tree()->is_debugging_collisions_hint())) {
		update_gizmos();
	}

	spring_length = p_length;
}

void SpringArm3D::set_shape(Ref<Shape3D> p_shape) {
	shape = p_shape;
}

Ref<Shape3D> SpringArm3D::get_shape() const {
	return shape;
}

void SpringArm3D::set_mask(uint32_t p_mask) {
	mask = p_mask;
}

uint32_t SpringArm3D::get_mask() {
	return mask;
}

real_t SpringArm3D::get_margin() {
	return margin;
}

void SpringArm3D::set_margin(real_t p_margin) {
	margin = p_margin;
}

void SpringArm3D::add_excluded_object(RID p_rid) {
	excluded_objects.insert(p_rid);
}

bool SpringArm3D::remove_excluded_object(RID p_rid) {
	return excluded_objects.erase(p_rid);
}

void SpringArm3D::clear_excluded_objects() {
	excluded_objects.clear();
}

real_t SpringArm3D::get_hit_length() {
	return current_spring_length;
}

void SpringArm3D::process_spring() {
	// From
	real_t motion_delta(1);
	real_t motion_delta_unsafe(1);

	Vector3 motion;
	const Vector3 cast_direction(get_global_transform().basis.xform(Vector3(0, 0, 1)));

	motion = Vector3(cast_direction * (spring_length));

	if (shape.is_null()) {
		Camera3D *camera = nullptr;
		for (int i = get_child_count() - 1; 0 <= i; --i) {
			camera = Object::cast_to<Camera3D>(get_child(i));
			if (camera) {
				break;
			}
		}

		if (camera != nullptr) {
			//use camera rotation, but spring arm position
			Transform3D base_transform = camera->get_global_transform();
			base_transform.origin = get_global_transform().origin;

			PhysicsDirectSpaceState3D::ShapeParameters shape_params;
			shape_params.shape_rid = camera->get_pyramid_shape_rid();
			shape_params.transform = base_transform;
			shape_params.motion = motion;
			shape_params.exclude = excluded_objects;
			shape_params.collision_mask = mask;

			get_world_3d()->get_direct_space_state()->cast_motion(shape_params, motion_delta, motion_delta_unsafe);
			if (motion_delta != 1) {
				motion_delta -= margin / spring_length;
			}
		} else {
			PhysicsDirectSpaceState3D::RayParameters ray_params;
			ray_params.from = get_global_transform().origin;
			ray_params.to = get_global_transform().origin + motion;
			ray_params.exclude = excluded_objects;
			ray_params.collision_mask = mask;

			PhysicsDirectSpaceState3D::RayResult r;
			bool intersected = get_world_3d()->get_direct_space_state()->intersect_ray(ray_params, r);
			if (intersected) {
				real_t dist = get_global_transform().origin.distance_to(r.position);
				dist -= margin;
				motion_delta = dist / (spring_length);
			}
		}
	} else {
		PhysicsDirectSpaceState3D::ShapeParameters shape_params;
		shape_params.shape_rid = shape->get_rid();
		shape_params.transform = get_global_transform();
		shape_params.motion = motion;
		shape_params.exclude = excluded_objects;
		shape_params.collision_mask = mask;

		get_world_3d()->get_direct_space_state()->cast_motion(shape_params, motion_delta, motion_delta_unsafe);
		if (motion_delta != 1) {
			motion_delta -= margin / spring_length;
		}
	}

	current_spring_length = spring_length * motion_delta;
	Transform3D child_transform;
	child_transform.origin = get_global_transform().origin + cast_direction * (spring_length * motion_delta);

	for (int i = get_child_count() - 1; 0 <= i; --i) {
		Node3D *child = Object::cast_to<Node3D>(get_child(i));
		if (child) {
			child_transform.basis = child->get_global_transform().basis;
			child->set_global_transform(child_transform);
		}
	}
}
