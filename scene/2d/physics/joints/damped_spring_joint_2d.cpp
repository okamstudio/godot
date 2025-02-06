/**************************************************************************/
/*  damped_spring_joint_2d.cpp                                            */
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

#include "damped_spring_joint_2d.h"

#include "scene/2d/physics/physics_body_2d.h"

void DampedSpringJoint2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			if (!is_inside_tree()) {
				break;
			}

			if (!Engine::get_singleton()->is_editor_hint() && !get_tree()->is_debugging_collisions_hint()) {
				break;
			}

			draw_line(Point2(-10, 0), Point2(+10, 0), Color(0.7, 0.6, 0.0, 0.5), 3);
			draw_line(Point2(-10, length), Point2(+10, length), Color(0.7, 0.6, 0.0, 0.5), 3);
			draw_line(Point2(0, 0), Point2(0, length), Color(0.7, 0.6, 0.0, 0.5), 3);
		} break;
	}
}

void DampedSpringJoint2D::_configure_joint(RID p_joint, PhysicsBody2D *body_a, PhysicsBody2D *body_b) {
	Transform2D gt = get_global_transform();
	Vector2 anchor_A = gt.get_origin();
	Vector2 anchor_B = gt.xform(Vector2(0, length));

	PhysicsServer2D::get_singleton()->joint_make_damped_spring(p_joint, anchor_A, anchor_B, body_a->get_rid(), body_b->get_rid());
	if (rest_length) {
		PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(p_joint, PhysicsServer2D::DAMPED_SPRING_REST_LENGTH, rest_length);
	}
	PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(p_joint, PhysicsServer2D::DAMPED_SPRING_STIFFNESS, stiffness);
	PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(p_joint, PhysicsServer2D::DAMPED_SPRING_DAMPING, damping);
}

void DampedSpringJoint2D::set_length(real_t p_length) {
	length = p_length;
	queue_redraw();
}

real_t DampedSpringJoint2D::get_length() const {
	return length;
}

void DampedSpringJoint2D::set_rest_length(real_t p_rest_length) {
	rest_length = p_rest_length;
	queue_redraw();
	if (is_configured()) {
		PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(get_rid(), PhysicsServer2D::DAMPED_SPRING_REST_LENGTH, p_rest_length ? p_rest_length : length);
	}
}

real_t DampedSpringJoint2D::get_rest_length() const {
	return rest_length;
}

void DampedSpringJoint2D::set_stiffness(real_t p_stiffness) {
	stiffness = p_stiffness;
	queue_redraw();
	if (is_configured()) {
		PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(get_rid(), PhysicsServer2D::DAMPED_SPRING_STIFFNESS, p_stiffness);
	}
}

real_t DampedSpringJoint2D::get_stiffness() const {
	return stiffness;
}

void DampedSpringJoint2D::set_damping(real_t p_damping) {
	damping = p_damping;
	queue_redraw();
	if (is_configured()) {
		PhysicsServer2D::get_singleton()->damped_spring_joint_set_param(get_rid(), PhysicsServer2D::DAMPED_SPRING_DAMPING, p_damping);
	}
}

real_t DampedSpringJoint2D::get_damping() const {
	return damping;
}

void DampedSpringJoint2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_length", "length"), &DampedSpringJoint2D::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &DampedSpringJoint2D::get_length);
	ClassDB::bind_method(D_METHOD("set_rest_length", "rest_length"), &DampedSpringJoint2D::set_rest_length);
	ClassDB::bind_method(D_METHOD("get_rest_length"), &DampedSpringJoint2D::get_rest_length);
	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &DampedSpringJoint2D::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &DampedSpringJoint2D::get_stiffness);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &DampedSpringJoint2D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &DampedSpringJoint2D::get_damping);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "1,65535,1,exp,suffix:px"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rest_length", PROPERTY_HINT_RANGE, "0,65535,1,exp,suffix:px"), "set_rest_length", "get_rest_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness", PROPERTY_HINT_RANGE, U"0.1,64,0.1,exp,suffix:kg/s\u00B2"), "set_stiffness", "get_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0.01,16,0.01,exp,suffix:kg/s"), "set_damping", "get_damping");
}

DampedSpringJoint2D::DampedSpringJoint2D() {
}
