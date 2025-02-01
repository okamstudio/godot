/**************************************************************************/
/*  test_sprite_frames.h                                                  */
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

#include "scene/resources/sprite_frames.h"

#include "tests/test_macros.h"

namespace TestSpriteFrames {
const String test_animation_name = "GodotTest";

TEST_CASE("[SpriteFrames] Constructor methods") {
	const SpriteFrames frames;
	CHECK_MESSAGE(
			frames.get_animation_names().size() == 1,
			"Should be initialized with 1 entry.");
	CHECK_MESSAGE(
			frames.get_animation_names().get(0) == "default",
			"Should be initialized with default entry.");
}

TEST_CASE("[SpriteFrames] Animation addition, list getter, renaming, removal, and retrieval") {
	SpriteFrames frames;
	Vector<String> test_names = { "default", "2", "1", "3" };

	// "default" is there already
	frames.add_animation("2");
	frames.add_animation("1");
	frames.add_animation("3");

	for (int i = 0; i < test_names.size(); i++) {
		CHECK_MESSAGE(
				frames.has_animation(test_names[i]),
				"Add animation properly worked for each value");
	}

	CHECK_MESSAGE(
			!frames.has_animation("999"),
			"Return false when checking for animation that does not exist");

	List<StringName> sname_list;
	frames.get_animation_list(&sname_list);

	CHECK_MESSAGE(
			sname_list.size() == test_names.size(),
			"StringName List getter returned list of expected size");

	int idx = 0;
	for (List<StringName>::ConstIterator itr = sname_list.begin(); itr != sname_list.end(); ++itr, ++idx) {
		CHECK_MESSAGE(
				*itr == StringName(test_names[idx]),
				"StringName List getter returned expected values");
	}

	// get_animation_names() sorts the results.
	Vector<String> string_vector = frames.get_animation_names();
	test_names.sort();

	for (int i = 0; i < test_names.size(); i++) {
		CHECK_MESSAGE(
				string_vector[i] == test_names[i],
				"String Vector getter returned expected values");
	}

	// These error handling cases should not crash.
	ERR_PRINT_OFF;
	frames.rename_animation("This does not exist", "0");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			!frames.has_animation("0"),
			"Correctly handles rename error when entry does not exist");

	// These error handling cases should not crash.
	ERR_PRINT_OFF;
	frames.rename_animation("3", "1");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			frames.has_animation("3"),
			"Correctly handles rename error when entry exists, but new name already exists");

	ERR_PRINT_OFF;
	frames.add_animation("1");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			frames.get_animation_names().size() == 4,
			"Correctly does not add when entry already exists");

	frames.rename_animation("3", "9");

	CHECK_MESSAGE(
			frames.has_animation("9"),
			"Animation renamed correctly");

	frames.remove_animation("9");

	CHECK_MESSAGE(
			!frames.has_animation("9"),
			"Animation removed correctly");

	frames.clear_all();

	CHECK_MESSAGE(
			frames.get_animation_names().size() == 1,
			"Clear all removed all animations and re-added the default animation entry");
}

TEST_CASE("[SpriteFrames] Animation Speed getter and setter") {
	SpriteFrames frames;

	frames.add_animation(test_animation_name);

	CHECK_MESSAGE(
			frames.get_animation_speed(test_animation_name) == 5.0,
			"Sets new animation to default speed");

	frames.set_animation_speed(test_animation_name, 123.0004);

	CHECK_MESSAGE(
			frames.get_animation_speed(test_animation_name) == 123.0004,
			"Sets animation to positive double");

	// These error handling cases should not crash.
	ERR_PRINT_OFF;
	frames.get_animation_speed("This does not exist");
	frames.set_animation_speed("This does not exist", 100);
	frames.set_animation_speed(test_animation_name, -999.999);
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			frames.get_animation_speed(test_animation_name) == 123.0004,
			"Prevents speed of animation being set to a negative value");

	frames.set_animation_speed(test_animation_name, 0.0);

	CHECK_MESSAGE(
			frames.get_animation_speed(test_animation_name) == 0.0,
			"Sets animation to zero");
}

TEST_CASE("[SpriteFrames] Animation Loop getter and setter") {
	SpriteFrames frames;

	frames.add_animation(test_animation_name);

	CHECK_MESSAGE(
			frames.get_animation_loop(test_animation_name),
			"Sets new animation to default loop value.");

	frames.set_animation_loop(test_animation_name, true);

	CHECK_MESSAGE(
			frames.get_animation_loop(test_animation_name),
			"Sets animation loop to true");

	frames.set_animation_loop(test_animation_name, false);

	CHECK_MESSAGE(
			!frames.get_animation_loop(test_animation_name),
			"Sets animation loop to false");

	// These error handling cases should not crash.
	ERR_PRINT_OFF;
	frames.get_animation_loop("This does not exist");
	frames.set_animation_loop("This does not exist", false);
	ERR_PRINT_ON;
}

// TODO
TEST_CASE("[SpriteFrames] Frame addition, removal, and retrieval") {
	Ref<Texture2D> dummy_frame1;
	dummy_frame1.instantiate();

	SpriteFrames frames;
	frames.add_animation(test_animation_name);
	frames.add_animation("1");
	frames.add_animation("2");

	CHECK_MESSAGE(
			frames.get_frame_count(test_animation_name) == 0,
			"Animation has a default frame count of 0");

	frames.add_frame(test_animation_name, dummy_frame1, 1.0, 0);
	frames.add_frame(test_animation_name, dummy_frame1, 1.0, 1);
	frames.add_frame(test_animation_name, dummy_frame1, 1.0, 2);

	CHECK_MESSAGE(
			frames.get_frame_count(test_animation_name) == 3,
			"Adds multiple frames");

	frames.remove_frame(test_animation_name, 1);
	frames.remove_frame(test_animation_name, 0);

	CHECK_MESSAGE(
			frames.get_frame_count(test_animation_name) == 1,
			"Removes multiple frames");

	// These error handling cases should not crash.
	ERR_PRINT_OFF;
	frames.add_frame("does not exist", dummy_frame1, 1.0, 0);
	frames.remove_frame(test_animation_name, -99);
	frames.remove_frame("does not exist", 0);
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			frames.get_frame_count(test_animation_name) == 1,
			"Handles bad values when adding or removing frames.");

	frames.clear(test_animation_name);

	CHECK_MESSAGE(
			frames.get_frame_count(test_animation_name) == 0,
			"Clears frames.");
}
} // namespace TestSpriteFrames
