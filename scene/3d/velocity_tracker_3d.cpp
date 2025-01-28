/**************************************************************************/
/*  velocity_tracker_3d.cpp                                               */
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

#include "velocity_tracker_3d.h"

#include "core/config/engine.h"

void VelocityTracker3D::set_track_physics_step(bool p_track_physics_step) {
	physics_step = p_track_physics_step;
}

bool VelocityTracker3D::is_tracking_physics_step() const {
	return physics_step;
}

void VelocityTracker3D::update_position(const Vector3 &p_position) {
	PositionHistory ph;
	ph.position = p_position;
	if (physics_step) {
		ph.frame = Engine::get_singleton()->get_physics_frames();
	} else {
		ph.frame = Engine::get_singleton()->get_frame_ticks();
	}

	if (position_history_len == 0 || position_history[0].frame != ph.frame) { //in same frame, use latest
		position_history_len = MIN(position_history.size(), position_history_len + 1);
		for (int i = position_history_len - 1; i > 0; i--) {
			position_history.write[i] = position_history[i - 1];
		}
	}

	position_history.write[0] = ph;
}

Vector3 VelocityTracker3D::get_tracked_linear_velocity() const {
	Vector3 linear_velocity;

	double max_time = 1 / 5.0; //maximum time to interpolate a velocity

	Vector3 distance_accum;
	double time_accum = 0.0;
	double base_time = 0.0;

	if (position_history_len) {
		if (physics_step) {
			uint64_t base = Engine::get_singleton()->get_physics_frames();
			base_time = double(base - position_history[0].frame) / Engine::get_singleton()->get_physics_ticks_per_second();
		} else {
			uint64_t base = Engine::get_singleton()->get_frame_ticks();
			base_time = double(base - position_history[0].frame) / 1000000.0;
		}
	}

	for (int i = 0; i < position_history_len - 1; i++) {
		double delta = 0.0;
		uint64_t diff = position_history[i].frame - position_history[i + 1].frame;
		Vector3 distance = position_history[i].position - position_history[i + 1].position;

		if (physics_step) {
			delta = double(diff) / Engine::get_singleton()->get_physics_ticks_per_second();
		} else {
			delta = double(diff) / 1000000.0;
		}

		if (base_time + time_accum + delta > max_time) {
			break;
		}

		distance_accum += distance;
		time_accum += delta;
	}

	if (time_accum) {
		linear_velocity = distance_accum / time_accum;
	}

	return linear_velocity;
}

void VelocityTracker3D::reset(const Vector3 &p_new_pos) {
	PositionHistory ph;
	ph.position = p_new_pos;
	if (physics_step) {
		ph.frame = Engine::get_singleton()->get_physics_frames();
	} else {
		ph.frame = Engine::get_singleton()->get_frame_ticks();
	}

	position_history.write[0] = ph;
	position_history_len = 1;
}

void VelocityTracker3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_track_physics_step", "enable"), &VelocityTracker3D::set_track_physics_step);
	ClassDB::bind_method(D_METHOD("is_tracking_physics_step"), &VelocityTracker3D::is_tracking_physics_step);
	ClassDB::bind_method(D_METHOD("update_position", "position"), &VelocityTracker3D::update_position);
	ClassDB::bind_method(D_METHOD("get_tracked_linear_velocity"), &VelocityTracker3D::get_tracked_linear_velocity);
	ClassDB::bind_method(D_METHOD("reset", "position"), &VelocityTracker3D::reset);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "track_physics_step"), "set_track_physics_step", "is_tracking_physics_step");
}

VelocityTracker3D::VelocityTracker3D() {
	position_history.resize(4); // should be configurable
}
