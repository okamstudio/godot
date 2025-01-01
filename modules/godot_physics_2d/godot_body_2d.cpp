/**************************************************************************/
/*  godot_body_2d.cpp                                                     */
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

#include "godot_body_2d.h"

#include "godot_area_2d.h"
#include "godot_body_direct_state_2d.h"
#include "godot_constraint_2d.h"
#include "godot_space_2d.h"

void GodotBody2D::_mass_properties_changed() {
	if (get_space() && !mass_properties_update_list.in_list()) {
		get_space()->body_add_to_mass_properties_update_list(&mass_properties_update_list);
	}
}

void GodotBody2D::update_mass_properties() {
	//update shapes and motions

	switch (mode) {
		case PhysicsServer2D::BODY_MODE_RIGID: {
			real_t total_area = 0;
			for (int i = 0; i < get_shape_count(); i++) {
				if (is_shape_disabled(i)) {
					continue;
				}
				total_area += get_shape_aabb(i).get_area();
			}

			if (calculate_center_of_mass) {
				// We have to recompute the center of mass.
				center_of_mass_local = Vector2();

				if (total_area != 0.0) {
					for (int i = 0; i < get_shape_count(); i++) {
						if (is_shape_disabled(i)) {
							continue;
						}

						real_t area = get_shape_aabb(i).get_area();

						real_t mass_new = area * mass / total_area;

						// NOTE: we assume that the shape origin is also its center of mass.
						center_of_mass_local += mass_new * get_shape_transform(i).get_origin();
					}

					center_of_mass_local /= mass;
				}
			}

			if (calculate_inertia) {
				inertia = 0;

				for (int i = 0; i < get_shape_count(); i++) {
					if (is_shape_disabled(i)) {
						continue;
					}

					const GodotShape2D *shape = get_shape(i);

					real_t area = get_shape_aabb(i).get_area();
					if (area == 0.0) {
						continue;
					}

					real_t mass_new = area * mass / total_area;

					Transform2D mtx = get_shape_transform(i);
					Vector2 scale = mtx.get_scale();
					Vector2 shape_origin = mtx.get_origin() - center_of_mass_local;
					inertia += shape->get_moment_of_inertia(mass_new, scale) + mass_new * shape_origin.length_squared();
				}
			}

			_inv_inertia = inertia > 0.0 ? (1.0 / inertia) : 0.0;

			if (mass) {
				_inv_mass = 1.0 / mass;
			} else {
				_inv_mass = 0;
			}

		} break;
		case PhysicsServer2D::BODY_MODE_KINEMATIC:
		case PhysicsServer2D::BODY_MODE_STATIC: {
			_inv_inertia = 0;
			_inv_mass = 0;
		} break;
		case PhysicsServer2D::BODY_MODE_RIGID_LINEAR: {
			_inv_inertia = 0;
			_inv_mass = 1.0 / mass;

		} break;
	}

	_update_transform_dependent();
}

void GodotBody2D::reset_mass_properties() {
	calculate_inertia = true;
	calculate_center_of_mass = true;
	_mass_properties_changed();
}

void GodotBody2D::set_active(bool p_active) {
	if (active == p_active) {
		return;
	}

	active = p_active;

	if (active) {
		if (mode == PhysicsServer2D::BODY_MODE_STATIC) {
			// Static bodies can't be active.
			active = false;
		} else if (get_space()) {
			get_space()->body_add_to_active_list(&active_list);
		}
	} else if (get_space()) {
		get_space()->body_remove_from_active_list(&active_list);
	}
}

void GodotBody2D::set_param(PhysicsServer2D::BodyParameter p_param, const Variant &p_value) {
	switch (p_param) {
		case PhysicsServer2D::BODY_PARAM_BOUNCE: {
			bounce = p_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_FRICTION: {
			friction = p_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_MASS: {
			real_t mass_value = p_value;
			ERR_FAIL_COND(mass_value <= 0);
			mass = mass_value;
			if (mode >= PhysicsServer2D::BODY_MODE_RIGID) {
				_mass_properties_changed();
			}
		} break;
		case PhysicsServer2D::BODY_PARAM_INERTIA: {
			real_t inertia_value = p_value;
			if (inertia_value <= 0.0) {
				calculate_inertia = true;
				if (mode == PhysicsServer2D::BODY_MODE_RIGID) {
					_mass_properties_changed();
				}
			} else {
				calculate_inertia = false;
				inertia = inertia_value;
				if (mode == PhysicsServer2D::BODY_MODE_RIGID) {
					_inv_inertia = 1.0 / inertia;
				}
			}
		} break;
		case PhysicsServer2D::BODY_PARAM_CENTER_OF_MASS: {
			calculate_center_of_mass = false;
			center_of_mass_local = p_value;
			_update_transform_dependent();
		} break;
		case PhysicsServer2D::BODY_PARAM_GRAVITY_SCALE: {
			if (Math::is_zero_approx(gravity_scale)) {
				wakeup();
			}
			gravity_scale = p_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_LINEAR_DAMP_MODE: {
			int mode_value = p_value;
			linear_damp_mode = (PhysicsServer2D::BodyDampMode)mode_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_ANGULAR_DAMP_MODE: {
			int mode_value = p_value;
			angular_damp_mode = (PhysicsServer2D::BodyDampMode)mode_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_LINEAR_DAMP: {
			linear_damp = p_value;
		} break;
		case PhysicsServer2D::BODY_PARAM_ANGULAR_DAMP: {
			angular_damp = p_value;
		} break;
		default: {
		}
	}
}

Variant GodotBody2D::get_param(PhysicsServer2D::BodyParameter p_param) const {
	switch (p_param) {
		case PhysicsServer2D::BODY_PARAM_BOUNCE: {
			return bounce;
		}
		case PhysicsServer2D::BODY_PARAM_FRICTION: {
			return friction;
		}
		case PhysicsServer2D::BODY_PARAM_MASS: {
			return mass;
		}
		case PhysicsServer2D::BODY_PARAM_INERTIA: {
			return inertia;
		}
		case PhysicsServer2D::BODY_PARAM_CENTER_OF_MASS: {
			return center_of_mass_local;
		}
		case PhysicsServer2D::BODY_PARAM_GRAVITY_SCALE: {
			return gravity_scale;
		}
		case PhysicsServer2D::BODY_PARAM_LINEAR_DAMP_MODE: {
			return linear_damp_mode;
		}
		case PhysicsServer2D::BODY_PARAM_ANGULAR_DAMP_MODE: {
			return angular_damp_mode;
		}
		case PhysicsServer2D::BODY_PARAM_LINEAR_DAMP: {
			return linear_damp;
		}
		case PhysicsServer2D::BODY_PARAM_ANGULAR_DAMP: {
			return angular_damp;
		}
		default: {
		}
	}

	return 0;
}

void GodotBody2D::set_mode(PhysicsServer2D::BodyMode p_mode) {
	PhysicsServer2D::BodyMode prev = mode;
	mode = p_mode;

	switch (p_mode) {
		//CLEAR UP EVERYTHING IN CASE IT NOT WORKS!
		case PhysicsServer2D::BODY_MODE_STATIC:
		case PhysicsServer2D::BODY_MODE_KINEMATIC: {
			_set_inv_transform(get_transform().affine_inverse());
			_inv_mass = 0;
			_inv_inertia = 0;
			_set_static(p_mode == PhysicsServer2D::BODY_MODE_STATIC);
			set_active(p_mode == PhysicsServer2D::BODY_MODE_KINEMATIC && contacts.size());
			linear_velocity = Vector2();
			angular_velocity = 0;
			if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC && prev != mode) {
				first_time_kinematic = true;
			}
		} break;
		case PhysicsServer2D::BODY_MODE_RIGID: {
			_inv_mass = mass > 0 ? (1.0 / mass) : 0;
			if (!calculate_inertia) {
				_inv_inertia = 1.0 / inertia;
			}
			_mass_properties_changed();
			_set_static(false);
			set_active(true);

		} break;
		case PhysicsServer2D::BODY_MODE_RIGID_LINEAR: {
			_inv_mass = mass > 0 ? (1.0 / mass) : 0;
			_inv_inertia = 0;
			angular_velocity = 0;
			_set_static(false);
			set_active(true);
		}
	}
}

PhysicsServer2D::BodyMode GodotBody2D::get_mode() const {
	return mode;
}

void GodotBody2D::_shapes_changed() {
	_mass_properties_changed();
	wakeup();
	wakeup_neighbours();
}

void GodotBody2D::set_state(PhysicsServer2D::BodyState p_state, const Variant &p_variant) {
	switch (p_state) {
		case PhysicsServer2D::BODY_STATE_TRANSFORM: {
			if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
				new_transform = p_variant;
				//wakeup_neighbours();
				set_active(true);
				if (first_time_kinematic) {
					_set_transform(p_variant);
					_set_inv_transform(get_transform().affine_inverse());
					first_time_kinematic = false;
				}
			} else if (mode == PhysicsServer2D::BODY_MODE_STATIC) {
				_set_transform(p_variant);
				_set_inv_transform(get_transform().affine_inverse());
				wakeup_neighbours();
			} else {
				Transform2D t = p_variant;
				t.orthonormalize();
				new_transform = get_transform(); //used as old to compute motion
				if (t == new_transform) {
					break;
				}
				_set_transform(t);
				_set_inv_transform(get_transform().inverse());
				_update_transform_dependent();
			}
			wakeup();

		} break;
		case PhysicsServer2D::BODY_STATE_LINEAR_VELOCITY: {
			linear_velocity = p_variant;
			constant_linear_velocity = linear_velocity;
			wakeup();

		} break;
		case PhysicsServer2D::BODY_STATE_ANGULAR_VELOCITY: {
			angular_velocity = p_variant;
			constant_angular_velocity = angular_velocity;
			wakeup();

		} break;
		case PhysicsServer2D::BODY_STATE_SLEEPING: {
			if (mode == PhysicsServer2D::BODY_MODE_STATIC || mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
				break;
			}
			bool do_sleep = p_variant;
			if (do_sleep) {
				linear_velocity = Vector2();
				//biased_linear_velocity=Vector3();
				angular_velocity = 0;
				//biased_angular_velocity=Vector3();
				set_active(false);
			} else {
				if (mode != PhysicsServer2D::BODY_MODE_STATIC) {
					set_active(true);
				}
			}
		} break;
		case PhysicsServer2D::BODY_STATE_CAN_SLEEP: {
			can_sleep = p_variant;
			if (mode >= PhysicsServer2D::BODY_MODE_RIGID && !active && !can_sleep) {
				set_active(true);
			}

		} break;
	}
}

Variant GodotBody2D::get_state(PhysicsServer2D::BodyState p_state) const {
	switch (p_state) {
		case PhysicsServer2D::BODY_STATE_TRANSFORM: {
			return get_transform();
		}
		case PhysicsServer2D::BODY_STATE_LINEAR_VELOCITY: {
			return linear_velocity;
		}
		case PhysicsServer2D::BODY_STATE_ANGULAR_VELOCITY: {
			return angular_velocity;
		}
		case PhysicsServer2D::BODY_STATE_SLEEPING: {
			return !is_active();
		}
		case PhysicsServer2D::BODY_STATE_CAN_SLEEP: {
			return can_sleep;
		}
	}

	return Variant();
}

void GodotBody2D::set_space(GodotSpace2D *p_space) {
	if (get_space()) {
		wakeup_neighbours();

		if (mass_properties_update_list.in_list()) {
			get_space()->body_remove_from_mass_properties_update_list(&mass_properties_update_list);
		}
		if (active_list.in_list()) {
			get_space()->body_remove_from_active_list(&active_list);
		}
		if (direct_state_query_list.in_list()) {
			get_space()->body_remove_from_state_query_list(&direct_state_query_list);
		}
	}

	_set_space(p_space);

	if (get_space()) {
		_mass_properties_changed();

		if (active && !active_list.in_list()) {
			get_space()->body_add_to_active_list(&active_list);
		}
	}
}

void GodotBody2D::_update_transform_dependent() {
	center_of_mass = get_transform().basis_xform(center_of_mass_local);
}

void GodotBody2D::integrate_forces(real_t p_step) {
	if (mode == PhysicsServer2D::BODY_MODE_STATIC) {
		return;
	}

	ERR_FAIL_NULL(get_space());

	int ac = areas.size();

	bool gravity_done = false;
	bool linear_damp_done = false;
	bool angular_damp_done = false;

	bool stopped = false;

	gravity = Vector2(0, 0);

	total_linear_damp = 0.0;
	total_angular_damp = 0.0;

	// Combine gravity and damping from overlapping areas in priority order.
	if (ac) {
		areas.sort();
		const AreaCMP *aa = &areas[0];
		for (int i = ac - 1; i >= 0 && !stopped; i--) {
			if (!gravity_done) {
				PhysicsServer2D::AreaSpaceOverrideMode area_gravity_mode = (PhysicsServer2D::AreaSpaceOverrideMode)(int)aa[i].area->get_param(PhysicsServer2D::AREA_PARAM_GRAVITY_OVERRIDE_MODE);
				if (area_gravity_mode != PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED) {
					Vector2 area_gravity;
					aa[i].area->compute_gravity(get_transform().get_origin(), area_gravity);
					switch (area_gravity_mode) {
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
							gravity += area_gravity;
							gravity_done = area_gravity_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE;
						} break;
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
							gravity = area_gravity;
							gravity_done = area_gravity_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE;
						} break;
						default: {
						}
					}
				}
			}
			if (!linear_damp_done) {
				PhysicsServer2D::AreaSpaceOverrideMode area_linear_damp_mode = (PhysicsServer2D::AreaSpaceOverrideMode)(int)aa[i].area->get_param(PhysicsServer2D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE);
				if (area_linear_damp_mode != PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED) {
					real_t area_linear_damp = aa[i].area->get_linear_damp();
					switch (area_linear_damp_mode) {
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
							total_linear_damp += area_linear_damp;
							linear_damp_done = area_linear_damp_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE;
						} break;
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
							total_linear_damp = area_linear_damp;
							linear_damp_done = area_linear_damp_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE;
						} break;
						default: {
						}
					}
				}
			}
			if (!angular_damp_done) {
				PhysicsServer2D::AreaSpaceOverrideMode area_angular_damp_mode = (PhysicsServer2D::AreaSpaceOverrideMode)(int)aa[i].area->get_param(PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE);
				if (area_angular_damp_mode != PhysicsServer2D::AREA_SPACE_OVERRIDE_DISABLED) {
					real_t area_angular_damp = aa[i].area->get_angular_damp();
					switch (area_angular_damp_mode) {
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
							total_angular_damp += area_angular_damp;
							angular_damp_done = area_angular_damp_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE;
						} break;
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE:
						case PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
							total_angular_damp = area_angular_damp;
							angular_damp_done = area_angular_damp_mode == PhysicsServer2D::AREA_SPACE_OVERRIDE_REPLACE;
						} break;
						default: {
						}
					}
				}
			}
			stopped = gravity_done && linear_damp_done && angular_damp_done;
		}
	}

	// Add default gravity and damping from space area.
	if (!stopped) {
		GodotArea2D *default_area = get_space()->get_default_area();
		ERR_FAIL_NULL(default_area);

		if (!gravity_done) {
			Vector2 default_gravity;
			default_area->compute_gravity(get_transform().get_origin(), default_gravity);
			gravity += default_gravity;
		}

		if (!linear_damp_done) {
			total_linear_damp += default_area->get_linear_damp();
		}

		if (!angular_damp_done) {
			total_angular_damp += default_area->get_angular_damp();
		}
	}

	// Override linear damping with body's value.
	switch (linear_damp_mode) {
		case PhysicsServer2D::BODY_DAMP_MODE_COMBINE: {
			total_linear_damp += linear_damp;
		} break;
		case PhysicsServer2D::BODY_DAMP_MODE_REPLACE: {
			total_linear_damp = linear_damp;
		} break;
	}

	// Override angular damping with body's value.
	switch (angular_damp_mode) {
		case PhysicsServer2D::BODY_DAMP_MODE_COMBINE: {
			total_angular_damp += angular_damp;
		} break;
		case PhysicsServer2D::BODY_DAMP_MODE_REPLACE: {
			total_angular_damp = angular_damp;
		} break;
	}

	gravity *= gravity_scale;

	prev_linear_velocity = linear_velocity;
	prev_angular_velocity = angular_velocity;

	Vector2 motion;
	bool do_motion = false;

	if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
		//compute motion, angular and etc. velocities from prev transform
		motion = new_transform.get_origin() - get_transform().get_origin();
		linear_velocity = constant_linear_velocity + motion / p_step;

		real_t rot = new_transform.get_rotation() - get_transform().get_rotation();
		angular_velocity = constant_angular_velocity + remainder(rot, 2.0 * Math_PI) / p_step;

		do_motion = true;

	} else {
		if (!omit_force_integration) {
			//overridden by direct state query

			Vector2 force = gravity * mass + applied_force + constant_force;
			real_t torque = applied_torque + constant_torque;

			real_t damp = 1.0 - p_step * total_linear_damp;

			if (damp < 0) { // reached zero in the given time
				damp = 0;
			}

			real_t angular_damp_new = 1.0 - p_step * total_angular_damp;

			if (angular_damp_new < 0) { // reached zero in the given time
				angular_damp_new = 0;
			}

			linear_velocity *= damp;
			angular_velocity *= angular_damp_new;

			linear_velocity += _inv_mass * force * p_step;
			angular_velocity += _inv_inertia * torque * p_step;
		}

		if (continuous_cd_mode != PhysicsServer2D::CCD_MODE_DISABLED) {
			motion = linear_velocity * p_step;
			do_motion = true;
		}
	}

	applied_force = Vector2();
	applied_torque = 0.0;

	biased_angular_velocity = 0.0;
	biased_linear_velocity = Vector2();

	if (do_motion) { //shapes temporarily extend for raycast
		_update_shapes_with_motion(motion);
	}

	contact_count = 0;
}

void GodotBody2D::integrate_velocities(real_t p_step) {
	if (mode == PhysicsServer2D::BODY_MODE_STATIC) {
		return;
	}

	ERR_FAIL_NULL(get_space());

	if (fi_callback_data || body_state_callback.is_valid()) {
		get_space()->body_add_to_state_query_list(&direct_state_query_list);
	}

	if (mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
		_set_transform(new_transform, false);
		_set_inv_transform(new_transform.affine_inverse());
		if (contacts.size() == 0 && linear_velocity == Vector2() && angular_velocity == 0) {
			set_active(false); //stopped moving, deactivate
		}
		return;
	}

	real_t total_angular_velocity = angular_velocity + biased_angular_velocity;
	Vector2 total_linear_velocity = linear_velocity + biased_linear_velocity;

	real_t angle_delta = total_angular_velocity * p_step;
	real_t angle = get_transform().get_rotation() + angle_delta;
	Vector2 pos = get_transform().get_origin() + total_linear_velocity * p_step;

	if (center_of_mass.length_squared() > CMP_EPSILON2) {
		// Calculate displacement due to center of mass offset.
		pos += center_of_mass - center_of_mass.rotated(angle_delta);
	}

	_set_transform(Transform2D(angle, pos), continuous_cd_mode == PhysicsServer2D::CCD_MODE_DISABLED);
	_set_inv_transform(get_transform().inverse());

	if (continuous_cd_mode != PhysicsServer2D::CCD_MODE_DISABLED) {
		new_transform = get_transform();
	}

	_update_transform_dependent();
}

void GodotBody2D::wakeup_neighbours() {
	for (const Pair<GodotConstraint2D *, int> &E : constraint_list) {
		const GodotConstraint2D *c = E.first;
		GodotBody2D **n = c->get_body_ptr();
		int bc = c->get_body_count();

		for (int i = 0; i < bc; i++) {
			if (i == E.second) {
				continue;
			}
			GodotBody2D *b = n[i];
			if (b->mode < PhysicsServer2D::BODY_MODE_RIGID) {
				continue;
			}

			if (!b->is_active()) {
				b->set_active(true);
			}
		}
	}
}

void GodotBody2D::call_queries() {
	Variant direct_state_variant = get_direct_state();

	if (fi_callback_data) {
		if (!fi_callback_data->callable.is_valid()) {
			set_force_integration_callback(Callable());
		} else {
			const Variant *vp[2] = { &direct_state_variant, &fi_callback_data->udata };

			Callable::CallError ce;
			Variant rv;
			if (fi_callback_data->udata.get_type() != Variant::NIL) {
				fi_callback_data->callable.callp(vp, 2, rv, ce);

			} else {
				fi_callback_data->callable.callp(vp, 1, rv, ce);
			}
		}
	}

	if (body_state_callback.is_valid()) {
		body_state_callback.call(direct_state_variant);
	}
}

bool GodotBody2D::sleep_test(real_t p_step) {
	if (mode == PhysicsServer2D::BODY_MODE_STATIC || mode == PhysicsServer2D::BODY_MODE_KINEMATIC) {
		return true;
	} else if (!can_sleep) {
		return false;
	}

	ERR_FAIL_NULL_V(get_space(), true);

	if (Math::abs(angular_velocity) < get_space()->get_body_angular_velocity_sleep_threshold() && Math::abs(linear_velocity.length_squared()) < get_space()->get_body_linear_velocity_sleep_threshold() * get_space()->get_body_linear_velocity_sleep_threshold()) {
		still_time += p_step;

		return still_time > get_space()->get_body_time_to_sleep();
	} else {
		still_time = 0; //maybe this should be set to 0 on set_active?
		return false;
	}
}

void GodotBody2D::set_state_sync_callback(const Callable &p_callable) {
	body_state_callback = p_callable;
}

void GodotBody2D::set_force_integration_callback(const Callable &p_callable, const Variant &p_udata) {
	if (p_callable.is_valid()) {
		if (!fi_callback_data) {
			fi_callback_data = memnew(ForceIntegrationCallbackData);
		}
		fi_callback_data->callable = p_callable;
		fi_callback_data->udata = p_udata;
	} else if (fi_callback_data) {
		memdelete(fi_callback_data);
		fi_callback_data = nullptr;
	}
}

GodotPhysicsDirectBodyState2D *GodotBody2D::get_direct_state() {
	if (!direct_state) {
		direct_state = memnew(GodotPhysicsDirectBodyState2D);
		direct_state->body = this;
	}
	return direct_state;
}

GodotBody2D::GodotBody2D() :
		GodotCollisionObject2D(TYPE_BODY),
		active_list(this),
		mass_properties_update_list(this),
		direct_state_query_list(this) {
	_set_static(false);
}

GodotBody2D::~GodotBody2D() {
	if (fi_callback_data) {
		memdelete(fi_callback_data);
	}
	if (direct_state) {
		memdelete(direct_state);
	}
}
