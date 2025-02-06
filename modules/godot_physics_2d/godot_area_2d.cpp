/**************************************************************************/
/*  godot_area_2d.cpp                                                     */
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

#include "godot_area_2d.h"
#include "godot_body_2d.h"
#include "godot_space_2d.h"

GodotArea2D::BodyKey::BodyKey(GodotBody2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape) {
	rid = p_body->get_self();
	instance_id = p_body->get_instance_id();
	body_shape = p_body_shape;
	area_shape = p_area_shape;
}

GodotArea2D::BodyKey::BodyKey(GodotArea2D *p_body, uint32_t p_body_shape, uint32_t p_area_shape) {
	rid = p_body->get_self();
	instance_id = p_body->get_instance_id();
	body_shape = p_body_shape;
	area_shape = p_area_shape;
}

void GodotArea2D::_shapes_changed() {
	if (!moved_list.in_list() && get_space()) {
		get_space()->area_add_to_moved_list(&moved_list);
	}
}

void GodotArea2D::set_transform(const Transform2D &p_transform) {
	if (!moved_list.in_list() && get_space()) {
		get_space()->area_add_to_moved_list(&moved_list);
	}

	_set_transform(p_transform);
	_set_inv_transform(p_transform.affine_inverse());
}

void GodotArea2D::set_space(GodotSpace2D *p_space) {
	if (get_space()) {
		if (monitor_query_list.in_list()) {
			get_space()->area_remove_from_monitor_query_list(&monitor_query_list);
		}
		if (moved_list.in_list()) {
			get_space()->area_remove_from_moved_list(&moved_list);
		}
	}

	monitored_bodies.clear();
	monitored_areas.clear();

	_set_space(p_space);
}

void GodotArea2D::set_monitor_callback(const Callable &p_callback) {
	_unregister_shapes();

	monitor_callback = p_callback;

	monitored_bodies.clear();
	monitored_areas.clear();

	_shape_changed();

	if (!moved_list.in_list() && get_space()) {
		get_space()->area_add_to_moved_list(&moved_list);
	}
}

void GodotArea2D::set_area_monitor_callback(const Callable &p_callback) {
	_unregister_shapes();

	area_monitor_callback = p_callback;

	monitored_bodies.clear();
	monitored_areas.clear();

	_shape_changed();

	if (!moved_list.in_list() && get_space()) {
		get_space()->area_add_to_moved_list(&moved_list);
	}
}

void GodotArea2D::_set_space_override_mode(PhysicsServer2D::AreaSpaceOverrideMode &r_mode, PhysicsServer2D::AreaSpaceOverrideMode p_new_mode) {
	bool do_override = p_new_mode != PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED;
	if (do_override == (r_mode != PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED)) {
		return;
	}
	_unregister_shapes();
	r_mode = p_new_mode;
	_shape_changed();
}

void GodotArea2D::set_param(PhysicsServer2D::AreaParameter p_param, const Variant &p_value) {
	switch (p_param) {
		case PhysicsServer2D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			_set_space_override_mode(gravity_override_mode, (PhysicsServer2D::AreaSpaceOverrideMode)(int)p_value);
			break;
		case PhysicsServer2D::AREA_PARAM_GRAVITY:
			gravity = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_VECTOR:
			gravity_vector = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_IS_POINT:
			gravity_is_point = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			gravity_point_unit_distance = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			_set_space_override_mode(linear_damping_override_mode, (PhysicsServer2D::AreaSpaceOverrideMode)(int)p_value);
			break;
		case PhysicsServer2D::AREA_PARAM_LINEAR_DAMP:
			linear_damp = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			_set_space_override_mode(angular_damping_override_mode, (PhysicsServer2D::AreaSpaceOverrideMode)(int)p_value);
			break;
		case PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP:
			angular_damp = p_value;
			break;
		case PhysicsServer2D::AREA_PARAM_PRIORITY:
			priority = p_value;
			break;
	}
}

Variant GodotArea2D::get_param(PhysicsServer2D::AreaParameter p_param) const {
	switch (p_param) {
		case PhysicsServer2D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			return gravity_override_mode;
		case PhysicsServer2D::AREA_PARAM_GRAVITY:
			return gravity;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_VECTOR:
			return gravity_vector;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_IS_POINT:
			return gravity_is_point;
		case PhysicsServer2D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			return gravity_point_unit_distance;
		case PhysicsServer2D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			return linear_damping_override_mode;
		case PhysicsServer2D::AREA_PARAM_LINEAR_DAMP:
			return linear_damp;
		case PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			return angular_damping_override_mode;
		case PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP:
			return angular_damp;
		case PhysicsServer2D::AREA_PARAM_PRIORITY:
			return priority;
	}

	return Variant();
}

void GodotArea2D::_queue_monitor_update() {
	ERR_FAIL_NULL(get_space());

	if (!monitor_query_list.in_list()) {
		get_space()->area_add_to_monitor_query_list(&monitor_query_list);
	}
}

void GodotArea2D::set_monitorable(bool p_monitorable) {
	if (monitorable == p_monitorable) {
		return;
	}

	monitorable = p_monitorable;
	_set_static(!monitorable);
	_shapes_changed();
}

void GodotArea2D::call_queries() {
	if (!monitor_callback.is_null() && !monitored_bodies.is_empty()) {
		if (monitor_callback.is_valid()) {
			Variant res[5];
			Variant *resptr[5];
			for (int i = 0; i < 5; i++) {
				resptr[i] = &res[i];
			}

			for (HashMap<BodyKey, BodyState, BodyKey>::Iterator E = monitored_bodies.begin(); E;) {
				if (E->value.state == 0) { // Nothing happened
					HashMap<BodyKey, BodyState, BodyKey>::Iterator next = E;
					++next;
					monitored_bodies.remove(E);
					E = next;
					continue;
				}

				res[0] = E->value.state > 0 ? PhysicsServer2D::AREA_BODY_ADDED : PhysicsServer2D::AREA_BODY_REMOVED;
				res[1] = E->key.rid;
				res[2] = E->key.instance_id;
				res[3] = E->key.body_shape;
				res[4] = E->key.area_shape;

				HashMap<BodyKey, BodyState, BodyKey>::Iterator next = E;
				++next;
				monitored_bodies.remove(E);
				E = next;

				Callable::CallError ce;
				Variant ret;
				monitor_callback.callp((const Variant **)resptr, 5, ret, ce);

				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT_ONCE("Error calling event callback method " + Variant::get_callable_error_text(monitor_callback, (const Variant **)resptr, 5, ce));
				}
			}
		} else {
			monitored_bodies.clear();
			monitor_callback = Callable();
		}
	}

	if (!area_monitor_callback.is_null() && !monitored_areas.is_empty()) {
		if (area_monitor_callback.is_valid()) {
			Variant res[5];
			Variant *resptr[5];
			for (int i = 0; i < 5; i++) {
				resptr[i] = &res[i];
			}

			for (HashMap<BodyKey, BodyState, BodyKey>::Iterator E = monitored_areas.begin(); E;) {
				if (E->value.state == 0) { // Nothing happened
					HashMap<BodyKey, BodyState, BodyKey>::Iterator next = E;
					++next;
					monitored_areas.remove(E);
					E = next;
					continue;
				}

				res[0] = E->value.state > 0 ? PhysicsServer2D::AREA_BODY_ADDED : PhysicsServer2D::AREA_BODY_REMOVED;
				res[1] = E->key.rid;
				res[2] = E->key.instance_id;
				res[3] = E->key.body_shape;
				res[4] = E->key.area_shape;

				HashMap<BodyKey, BodyState, BodyKey>::Iterator next = E;
				++next;
				monitored_areas.remove(E);
				E = next;

				Callable::CallError ce;
				Variant ret;
				area_monitor_callback.callp((const Variant **)resptr, 5, ret, ce);

				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT_ONCE("Error calling event callback method " + Variant::get_callable_error_text(area_monitor_callback, (const Variant **)resptr, 5, ce));
				}
			}
		} else {
			monitored_areas.clear();
			area_monitor_callback = Callable();
		}
	}
}

void GodotArea2D::compute_gravity(const Vector2 &p_position, Vector2 &r_gravity) const {
	if (is_gravity_point()) {
		const real_t gr_unit_dist = get_gravity_point_unit_distance();
		Vector2 v = get_transform().xform(get_gravity_vector()) - p_position;
		if (gr_unit_dist > 0) {
			const real_t v_length_sq = v.length_squared();
			if (v_length_sq > 0) {
				const real_t gravity_strength = get_gravity() * gr_unit_dist * gr_unit_dist / v_length_sq;
				r_gravity = v.normalized() * gravity_strength;
			} else {
				r_gravity = Vector2();
			}
		} else {
			r_gravity = v.normalized() * get_gravity();
		}
	} else {
		r_gravity = get_transform().basis_xform(get_gravity_vector() * get_gravity());
	}
}

GodotArea2D::GodotArea2D() :
		GodotCollisionObject2D(TYPE_AREA),
		monitor_query_list(this),
		moved_list(this) {
	_set_static(true); //areas are not active by default
}

GodotArea2D::~GodotArea2D() {
}
