/**************************************************************************/
/*  test_animation.h                                                      */
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

#include "scene/resources/animation.h"

#include "tests/test_macros.h"

namespace TestAnimation {

TEST_CASE("[Animation] Empty animation getters") {
	const Ref<Animation> animation = memnew(Animation);

	CHECK(animation->get_length() == doctest::Approx(real_t(1.0)));
	CHECK(animation->get_step() == doctest::Approx(real_t(1.0 / 30)));
}

TEST_CASE("[Animation] Create value track") {
	// This creates an animation that makes the node "Enemy" move to the right by
	// 100 pixels in 0.5 seconds.
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_VALUE);
	CHECK(track_index == 0);
	animation->track_set_path(track_index, NodePath("Enemy:position:x"));
	animation->track_insert_key(track_index, 0.0, 0);
	animation->track_insert_key(track_index, 0.5, 100);

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));
	CHECK(int(animation->track_get_key_value(0, 0)) == 0);
	CHECK(int(animation->track_get_key_value(0, 1)) == 100);

	CHECK(animation->value_track_interpolate(0, -0.2) == doctest::Approx(0.0));
	CHECK(animation->value_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	CHECK(animation->value_track_interpolate(0, 0.2) == doctest::Approx(40.0));
	CHECK(animation->value_track_interpolate(0, 0.4) == doctest::Approx(80.0));
	CHECK(animation->value_track_interpolate(0, 0.5) == doctest::Approx(100.0));
	CHECK(animation->value_track_interpolate(0, 0.6) == doctest::Approx(100.0));

	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(real_t(1.0)));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(real_t(1.0)));

	ERR_PRINT_OFF;
	// Nonexistent keys.
	CHECK(animation->track_get_key_value(0, 2).is_null());
	CHECK(animation->track_get_key_value(0, -1).is_null());
	CHECK(animation->track_get_key_transition(0, 2) == doctest::Approx(real_t(-1.0)));
	// Nonexistent track (and keys).
	CHECK(animation->track_get_key_value(1, 0).is_null());
	CHECK(animation->track_get_key_value(1, 1).is_null());
	CHECK(animation->track_get_key_value(1, 2).is_null());
	CHECK(animation->track_get_key_value(1, -1).is_null());
	CHECK(animation->track_get_key_transition(1, 0) == doctest::Approx(real_t(-1.0)));

	// This is a value track, so the methods below should return errors.
	CHECK(animation->try_position_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_rotation_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_scale_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create 3D position track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_POSITION_3D);
	animation->track_set_path(track_index, NodePath("Enemy:position"));
	animation->position_track_insert_key(track_index, 0.0, Vector3(0, 1, 2));
	animation->position_track_insert_key(track_index, 0.5, Vector3(3.5, 4, 5));

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));
	CHECK(Vector3(animation->track_get_key_value(0, 0)).is_equal_approx(Vector3(0, 1, 2)));
	CHECK(Vector3(animation->track_get_key_value(0, 1)).is_equal_approx(Vector3(3.5, 4, 5)));

	Vector3 r_interpolation;

	CHECK(animation->try_position_track_interpolate(0, -0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(0, 1, 2)));

	CHECK(animation->try_position_track_interpolate(0, 0.0, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(0, 1, 2)));

	CHECK(animation->try_position_track_interpolate(0, 0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(1.4, 2.2, 3.2)));

	CHECK(animation->try_position_track_interpolate(0, 0.4, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(2.8, 3.4, 4.4)));

	CHECK(animation->try_position_track_interpolate(0, 0.5, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(3.5, 4, 5)));

	CHECK(animation->try_position_track_interpolate(0, 0.6, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(3.5, 4, 5)));

	// 3D position tracks always use linear interpolation for performance reasons.
	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(real_t(1.0)));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(real_t(1.0)));

	// This is a 3D position track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	CHECK(animation->try_rotation_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_scale_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create 3D rotation track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_ROTATION_3D);
	animation->track_set_path(track_index, NodePath("Enemy:rotation"));
	animation->rotation_track_insert_key(track_index, 0.0, Quaternion::from_euler(Vector3(0, 1, 2)));
	animation->rotation_track_insert_key(track_index, 0.5, Quaternion::from_euler(Vector3(3.5, 4, 5)));

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));
	CHECK(Quaternion(animation->track_get_key_value(0, 0)).is_equal_approx(Quaternion::from_euler(Vector3(0, 1, 2))));
	CHECK(Quaternion(animation->track_get_key_value(0, 1)).is_equal_approx(Quaternion::from_euler(Vector3(3.5, 4, 5))));

	Quaternion r_interpolation;

	CHECK(animation->try_rotation_track_interpolate(0, -0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.403423, 0.259035, 0.73846, 0.47416)));

	CHECK(animation->try_rotation_track_interpolate(0, 0.0, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.403423, 0.259035, 0.73846, 0.47416)));

	CHECK(animation->try_rotation_track_interpolate(0, 0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.336182, 0.30704, 0.751515, 0.477425)));

	CHECK(animation->try_rotation_track_interpolate(0, 0.4, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.266585, 0.352893, 0.759303, 0.477344)));

	CHECK(animation->try_rotation_track_interpolate(0, 0.5, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.231055, 0.374912, 0.761204, 0.476048)));

	CHECK(animation->try_rotation_track_interpolate(0, 0.6, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Quaternion(0.231055, 0.374912, 0.761204, 0.476048)));

	// 3D rotation tracks always use linear interpolation for performance reasons.
	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(real_t(1.0)));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(real_t(1.0)));

	// This is a 3D rotation track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	CHECK(animation->try_position_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_scale_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(real_t(0.0)));
	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create 3D scale track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_SCALE_3D);
	animation->track_set_path(track_index, NodePath("Enemy:scale"));
	animation->scale_track_insert_key(track_index, 0.0, Vector3(0, 1, 2));
	animation->scale_track_insert_key(track_index, 0.5, Vector3(3.5, 4, 5));

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));
	CHECK(Vector3(animation->track_get_key_value(0, 0)).is_equal_approx(Vector3(0, 1, 2)));
	CHECK(Vector3(animation->track_get_key_value(0, 1)).is_equal_approx(Vector3(3.5, 4, 5)));

	Vector3 r_interpolation;

	CHECK(animation->try_scale_track_interpolate(0, -0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(0, 1, 2)));

	CHECK(animation->try_scale_track_interpolate(0, 0.0, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(0, 1, 2)));

	CHECK(animation->try_scale_track_interpolate(0, 0.2, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(1.4, 2.2, 3.2)));

	CHECK(animation->try_scale_track_interpolate(0, 0.4, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(2.8, 3.4, 4.4)));

	CHECK(animation->try_scale_track_interpolate(0, 0.5, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(3.5, 4, 5)));

	CHECK(animation->try_scale_track_interpolate(0, 0.6, &r_interpolation) == OK);
	CHECK(r_interpolation.is_equal_approx(Vector3(3.5, 4, 5)));

	// 3D scale tracks always use linear interpolation for performance reasons.
	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(1.0));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(1.0));

	// This is a 3D scale track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	CHECK(animation->try_position_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_rotation_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create blend shape track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_BLEND_SHAPE);
	animation->track_set_path(track_index, NodePath("Enemy:scale"));
	// Negative values for blend shapes should work as expected.
	animation->blend_shape_track_insert_key(track_index, 0.0, -1.0);
	animation->blend_shape_track_insert_key(track_index, 0.5, 1.0);

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));

	float r_blend = 0.0f;

	CHECK(animation->blend_shape_track_get_key(0, 0, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(-1.0f));

	CHECK(animation->blend_shape_track_get_key(0, 1, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(1.0f));

	CHECK(animation->try_blend_shape_track_interpolate(0, -0.2, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(-1.0f));

	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(-1.0f));

	CHECK(animation->try_blend_shape_track_interpolate(0, 0.2, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(-0.2f));

	CHECK(animation->try_blend_shape_track_interpolate(0, 0.4, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(0.6f));

	CHECK(animation->try_blend_shape_track_interpolate(0, 0.5, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(1.0f));

	CHECK(animation->try_blend_shape_track_interpolate(0, 0.6, &r_blend) == OK);
	CHECK(r_blend == doctest::Approx(1.0f));

	// Blend shape tracks always use linear interpolation for performance reasons.
	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(real_t(1.0)));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(real_t(1.0)));

	// This is a blend shape track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	CHECK(animation->try_position_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_rotation_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_scale_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create Bezier track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_BEZIER);
	animation->track_set_path(track_index, NodePath("Enemy:scale"));
	animation->bezier_track_insert_key(track_index, 0.0, -1.0, Vector2(-1, -1), Vector2(1, 1));
	animation->bezier_track_insert_key(track_index, 0.5, 1.0, Vector2(0, 1), Vector2(1, 0.5));

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));

	CHECK(animation->bezier_track_get_key_value(0, 0) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_get_key_value(0, 1) == doctest::Approx(real_t(1.0)));

	CHECK(animation->bezier_track_interpolate(0, -0.2) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.2) == doctest::Approx(real_t(-0.76057207584381)));
	CHECK(animation->bezier_track_interpolate(0, 0.4) == doctest::Approx(real_t(-0.39975279569626)));
	CHECK(animation->bezier_track_interpolate(0, 0.5) == doctest::Approx(real_t(1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.6) == doctest::Approx(real_t(1.0)));

	// This is a bezier track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	CHECK(animation->try_position_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_rotation_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_scale_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	CHECK(animation->try_blend_shape_track_interpolate(0, 0.0, nullptr) == ERR_INVALID_PARAMETER);
	ERR_PRINT_ON;
}

} // namespace TestAnimation
