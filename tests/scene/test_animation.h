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

#ifndef TEST_ANIMATION_H
#define TEST_ANIMATION_H

#include "core/math/random_pcg.h"
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

// Number of random keys to create when testing compression.
//
// Ideally this value would be higher. But if it's too high then the compression starts to use multiple pages,
// which means that keys get duplicated at page boundaries - that breaks the test, which relies on key indices
// being stable so that original and compressed key values can be compared. So, this value has been chosen to
// be reasonably high while only using a single page.
static const int COMPRESSION_RANDOM_KEY_COUNT = 200;

// Helper for printing the result of a Unorm16Check.
struct Unorm16CheckResult {
	real_t original;
	real_t decompressed;
	real_t difference;
	real_t threshold;

	Unorm16CheckResult(real_t p_original, real_t p_decompressed, real_t p_difference, real_t p_threshold) {
		original = p_original;
		decompressed = p_decompressed;
		difference = p_difference;
		threshold = p_threshold;
	}

	operator bool() const {
		return difference < threshold;
	}
};

doctest::String toString(const Unorm16CheckResult &r) {
	return vformat("original: %f, decompressed: %f, difference: %f, threshold: %f", r.original, r.decompressed, r.difference, r.threshold).utf8().get_data();
}

// Given a value that has been linearly remapped to unorm16 and back again, check if the decompressed value is within an expected error threshold.
struct Unorm16Check {
	real_t threshold;

	Unorm16Check(real_t range_min, real_t range_max) {
		threshold = 0.6 * (Math::abs(range_max - range_min) / 65535.0);
	}

	Unorm16CheckResult operator()(real_t original, real_t decompressed) const {
		real_t difference = Math::abs(original - decompressed);

		return Unorm16CheckResult(original, decompressed, difference, threshold);
	}
};

TEST_CASE("[Animation] Create compressed 3D position and scale tracks") {
	RandomPCG random;

	// Create tracks of random keys within a variety of ranges.

	Vector<Vector3> range_min_array;
	Vector<Vector3> range_max_array;

	range_min_array.push_back(Vector3(1.0, -4.0, -4.0));
	range_max_array.push_back(Vector3(4.0, 1.0, -1.0));

	range_min_array.push_back(Vector3(1.0, -10000.0, 10000.0));
	range_max_array.push_back(Vector3(10000.0, 20000.0, 10004.0));

	REQUIRE(range_min_array.size() == range_max_array.size());

	const int track_count = range_min_array.size();

	Vector<Vector<Vector3>> track_array;

	for (int track_index = 0; track_index < track_count; track_index++) {
		Vector<Vector3> key_array;

		Vector3 range_min = range_min_array[track_index];
		Vector3 range_max = range_max_array[track_index];
		Vector3 range_span = range_max - range_min;

		key_array.push_back(range_min);
		key_array.push_back(range_max);

		for (int random_key_index = 0; random_key_index < COMPRESSION_RANDOM_KEY_COUNT; random_key_index++) {
			key_array.push_back(range_min + (random.randf() * range_span));
		}

		track_array.push_back(key_array);
	}

	Ref<Animation> animation_position = memnew(Animation);
	Ref<Animation> animation_scale = memnew(Animation);

	for (int track_index = 0; track_index < track_count; track_index++) {
		REQUIRE(animation_position->add_track(Animation::TYPE_POSITION_3D, track_index) == track_index);
		REQUIRE(animation_scale->add_track(Animation::TYPE_SCALE_3D, track_index) == track_index);

		animation_position->track_set_path(track_index, NodePath(vformat("Enemy:position%d", track_index)));
		animation_scale->track_set_path(track_index, NodePath(vformat("Enemy:scale%d", track_index)));

		Vector<Vector3> key_array = track_array[track_index];

		for (int key_index = 0; key_index < key_array.size(); key_index++) {
			Vector3 key = key_array[key_index];
			double time = (double)key_index;

			REQUIRE(animation_position->position_track_insert_key(track_index, time, key) == key_index);
			REQUIRE(animation_scale->scale_track_insert_key(track_index, time, key) == key_index);
		}
	}

	animation_position->compress();
	animation_scale->compress();

	REQUIRE(track_array.size() == track_count);
	REQUIRE(animation_position->get_track_count() == track_count);
	REQUIRE(animation_scale->get_track_count() == track_count);

	for (int track_index = 0; track_index < track_count; track_index++) {
		REQUIRE(animation_position->track_is_compressed(track_index));
		REQUIRE(animation_scale->track_is_compressed(track_index));

		Vector3 range_min = range_min_array[track_index];
		Vector3 range_max = range_max_array[track_index];

		const Vector<Vector3> &key_array = track_array[track_index];

		REQUIRE(animation_position->track_get_key_count(track_index) == key_array.size());
		REQUIRE(animation_scale->track_get_key_count(track_index) == key_array.size());

		for (int key_index = 0; key_index < key_array.size(); key_index++) {
			Vector3 key = key_array[key_index];

			Vector3 decompressed_position, decompressed_scale;
			REQUIRE(animation_position->position_track_get_key(track_index, key_index, &decompressed_position) == OK);
			REQUIRE(animation_scale->scale_track_get_key(track_index, key_index, &decompressed_scale) == OK);

			for (int axis_index = 0; axis_index < 3; axis_index++) {
				Unorm16Check check(range_min[axis_index], range_max[axis_index]);

				CHECK(check(key[axis_index], decompressed_position[axis_index]));
				CHECK(check(key[axis_index], decompressed_scale[axis_index]));
			}
		}
	}
}

TEST_CASE("[Animation] Create compressed 3D rotation track") {
	RandomPCG random;

	// Start with various extremes.

	Vector<Quaternion> keys = {
		Quaternion(1.0, 0.0, 0.0, 0.0),
		Quaternion(0.0, 1.0, 0.0, 0.0),
		Quaternion(0.0, 0.0, 1.0, 0.0),
		Quaternion(0.0, 0.0, 0.0, 1.0),

		Quaternion(-1.0, 0.0, 0.0, 0.0),
		Quaternion(0.0, -1.0, 0.0, 0.0),
		Quaternion(0.0, 0.0, -1.0, 0.0),
		Quaternion(0.0, 0.0, 0.0, -1.0),
	};

	// Also add some random keys.

	for (int random_key_index = 0; random_key_index < COMPRESSION_RANDOM_KEY_COUNT; random_key_index++) {
		Quaternion q;

		do {
			q = Quaternion(random.randf(), random.randf(), random.randf(), random.randf());
		} while (q.length() < 0.001);

		q.normalize();

		keys.push_back(q);
	}

	Ref<Animation> animation = memnew(Animation);

	REQUIRE(animation->add_track(Animation::TYPE_ROTATION_3D, 0) == 0);
	animation->track_set_path(0, NodePath("Enemy:rotation"));

	for (int key_index = 0; key_index < keys.size(); key_index++) {
		double time = (double)key_index;
		Quaternion key = keys[key_index];

		REQUIRE(animation->rotation_track_insert_key(0, time, key) == key_index);
	}

	animation->compress();

	REQUIRE(animation->get_track_count() == 1);
	REQUIRE(animation->track_is_compressed(0));

	// The exact error threshold is hard to work out because the quaternion compression has some non-linear transforms.
	// This value is the maximum difference observed from 100,000,000 random keys plus a small fudge factor.

	real_t threshold = 0.000068393776 * 1.2;

	for (int key_index = 0; key_index < keys.size(); key_index++) {
		Quaternion key = keys[key_index];

		Quaternion decompressed;
		REQUIRE(animation->rotation_track_get_key(0, key_index, &decompressed) == OK);

		// Treat q and (q * -1.0) as the same. This is necessary because a quaternion with an angle close to 360 degrees
		// (w = -1.0) can be decompressed to a quaternion with an angle close to zero (w = 1.0).

		real_t difference = MIN((key - decompressed).length(), (key + decompressed).length());

		CHECK(difference < threshold);
	}
}

TEST_CASE("[Animation] Create compressed blend shape track") {
	RandomPCG random;

	// Hard code Animation::Compression::BLEND_SHAPE_RANGE since it's private.

	const float range_min = -8.0;
	const float range_max = 8.0;
	const float range_span = range_max - range_min;

	// Start with some extremes. Use zero for the first key as there's a special check for it later.

	Vector<float> keys = { 0.0, range_min, range_max };

	int zero_key_index = 0;

	// Also add some random keys.

	for (int random_key_index = 0; random_key_index < COMPRESSION_RANDOM_KEY_COUNT; random_key_index++) {
		keys.push_back(range_min + (random.randf() * range_span));
	}

	Ref<Animation> animation = memnew(Animation);

	REQUIRE(animation->add_track(Animation::TYPE_BLEND_SHAPE, 0) == 0);
	animation->track_set_path(0, NodePath("Enemy:blendshape"));

	for (int key_index = 0; key_index < keys.size(); key_index++) {
		double time = (double)key_index;
		float key = keys[key_index];

		REQUIRE(animation->blend_shape_track_insert_key(0, time, key) == key_index);
	}

	animation->compress();

	REQUIRE(animation->get_track_count() == 1);
	REQUIRE(animation->track_is_compressed(0));

	float zero_key_decompressed;
	REQUIRE(animation->blend_shape_track_get_key(0, zero_key_index, &zero_key_decompressed) == OK);

	CHECK_MESSAGE(zero_key_decompressed == 0.0f, "An original value of zero is expected to decompress to exactly zero");

	Unorm16Check check(range_min, range_max);

	for (int key_index = 0; key_index < keys.size(); key_index++) {
		float key = keys[key_index];

		float decompressed;
		REQUIRE(animation->blend_shape_track_get_key(0, key_index, &decompressed) == OK);

		CHECK(check(key, decompressed));
	}
}

} // namespace TestAnimation

#endif // TEST_ANIMATION_H
