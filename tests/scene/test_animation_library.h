/**************************************************************************/
/*  test_animation_library.h                                              */
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

#ifndef TEST_ANIMATION_LIBRARY_H
#define TEST_ANIMATION_LIBRARY_H

#include "scene/resources/animation_library.h"

#include "tests/test_macros.h"

namespace TestAnimationLibrary {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

const int NUM_BAD_NAMES = 4;

const char *empty_name = "";

const String bad_names[NUM_BAD_NAMES] = {
	String("[left bracket]"),
	String("comma,"),
	String("colon:"),
	String("/slash")
};

const String validated_names[NUM_BAD_NAMES]{
	String("_left bracket]"),
	String("comma_"),
	String("colon_"),
	String("_slash")
};

const String good_name_1 = "The starry heavens above?";
const String good_name_2 = "The moral law within!";

const char *signal_animation_added = "animation_added";
const char *signal_animation_removed = "animation_removed";
const char *signal_animation_renamed = "animation_renamed";
const char *signal_animation_changed = "animation_changed";

TEST_CASE("[AnimationLibrary] Name Validation") {
	for (int i = 0; i < NUM_BAD_NAMES; i++) {
		CHECK_FALSE(AnimationLibrary::is_valid_animation_name(bad_names[i]));
		CHECK_FALSE(AnimationLibrary::is_valid_library_name(bad_names[i]));
		CHECK_EQ(AnimationLibrary::validate_library_name(bad_names[i]), validated_names[i]);
	}

	CHECK_FALSE(AnimationLibrary::is_valid_animation_name(empty_name));
	CHECK(AnimationLibrary::is_valid_library_name(empty_name));

	CHECK(AnimationLibrary::is_valid_animation_name(good_name_1));
	CHECK(AnimationLibrary::is_valid_animation_name(good_name_2));

	CHECK(AnimationLibrary::is_valid_library_name(good_name_1));
	CHECK(AnimationLibrary::is_valid_library_name(good_name_2));
}

TEST_CASE("[AnimationLibrary] Add Animation") { // Should this depends on the test for get animation?
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	Ref<Animation> new_animation_1 = memnew(Animation);
	Ref<Animation> new_animation_2 = memnew(Animation);

	SIGNAL_WATCH(animation_library.ptr(), signal_animation_added);
	SIGNAL_WATCH(animation_library.ptr(), signal_animation_removed);
	SUBCASE("Invalid parameter") {
		ERR_PRINT_OFF;

		for (int i = 0; i < NUM_BAD_NAMES; i++) {
			CHECK_EQ(animation_library->add_animation(bad_names[i], new_animation_1), ERR_INVALID_PARAMETER);
		}

		CHECK_EQ(animation_library->add_animation(empty_name, new_animation_1), ERR_INVALID_PARAMETER);

		CHECK_EQ(animation_library->add_animation(good_name_1, nullptr), ERR_INVALID_PARAMETER);

		SIGNAL_CHECK_FALSE(signal_animation_added);
		SIGNAL_CHECK_FALSE(signal_animation_removed);

		ERR_PRINT_ON;
	}

	SUBCASE("Add one animation") {
		CHECK_EQ(animation_library->add_animation(good_name_1, new_animation_1), OK);
		SIGNAL_CHECK(signal_animation_added, build_array(build_array(StringName(good_name_1))));
		SIGNAL_CHECK_FALSE(signal_animation_removed);

		CHECK(animation_library->has_animation(StringName(good_name_1)));
	}

	SUBCASE("Two different animations") {
		CHECK_EQ(animation_library->add_animation(good_name_1, new_animation_1), OK);
		CHECK_EQ(animation_library->add_animation(good_name_2, new_animation_2), OK);

		SIGNAL_CHECK(signal_animation_added, build_array(build_array(StringName(good_name_1)), build_array(StringName(good_name_2))));

		SIGNAL_CHECK_FALSE(signal_animation_removed);

		CHECK(animation_library->has_animation(good_name_1));
		CHECK(animation_library->has_animation(good_name_2));
	}

	SUBCASE("Two same animations") {
		CHECK_EQ(animation_library->add_animation(good_name_1, new_animation_1), OK);
		CHECK_EQ(animation_library->add_animation(good_name_1, new_animation_2), OK);

		SIGNAL_CHECK(signal_animation_added, build_array(build_array(StringName(good_name_1)), build_array(StringName(good_name_1))));

		SIGNAL_CHECK(signal_animation_removed, build_array(build_array(StringName(good_name_1))));

		CHECK(animation_library->has_animation(good_name_1));
	}

	// TODO: SUBCASE("Add the same animation with two different names")
	// Relate to: #74824

	SIGNAL_UNWATCH(animation_library.ptr(), signal_animation_removed);
	SIGNAL_UNWATCH(animation_library.ptr(), signal_animation_added);
}

TEST_CASE("[AnimationLibrary] Remove animation") {
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	Ref<Animation> new_animation_1 = memnew(Animation);

	SIGNAL_WATCH(animation_library.ptr(), signal_animation_removed);

	SUBCASE("Remove non-existing") {
		ERR_PRINT_OFF;
		animation_library->remove_animation(good_name_1);
		ERR_PRINT_ON;

		SIGNAL_CHECK_FALSE(SNAME(signal_animation_removed));
	}

	SUBCASE("Remove animation") {
		animation_library->add_animation(good_name_1, new_animation_1);
		animation_library->remove_animation(good_name_1);

		SIGNAL_CHECK(SNAME(signal_animation_removed), build_array(build_array(StringName(good_name_1))));

		CHECK_FALSE(animation_library->has_animation(good_name_1));
	}

	SIGNAL_UNWATCH(animation_library.ptr(), signal_animation_removed)
}

TEST_CASE("[AnimationLibrary] Rename animation") {
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	Ref<Animation> new_animation_1 = memnew(Animation);
	Ref<Animation> new_animation_2 = memnew(Animation);

	SIGNAL_WATCH(animation_library.ptr(), SNAME(signal_animation_renamed));

	SUBCASE("Rename non-existing animation") {
		ERR_PRINT_OFF;
		animation_library->rename_animation(good_name_1, good_name_2);
		ERR_PRINT_ON;

		SIGNAL_CHECK_FALSE(SNAME(signal_animation_renamed));
	}

	SUBCASE("Rename to an invalid name") {
		animation_library->add_animation(good_name_1, new_animation_1);

		ERR_PRINT_OFF;
		for (int i = 0; i < NUM_BAD_NAMES; i++) {
			animation_library->rename_animation(good_name_1, bad_names[i]);
		}
		animation_library->rename_animation(good_name_1, empty_name);
		ERR_PRINT_ON;

		SIGNAL_CHECK_FALSE(SNAME(signal_animation_renamed));
		CHECK(animation_library->has_animation(good_name_1));
	}

	SUBCASE("Rename to an existing name") {
		animation_library->add_animation(good_name_1, new_animation_1);
		animation_library->add_animation(good_name_2, new_animation_2);
		ERR_PRINT_OFF;
		animation_library->rename_animation(good_name_1, good_name_2);
		animation_library->rename_animation(good_name_1, good_name_1);
		ERR_PRINT_ON;

		SIGNAL_CHECK_FALSE(SNAME(signal_animation_renamed));
		CHECK(animation_library->has_animation(good_name_1));
		CHECK(animation_library->has_animation(good_name_2));
	}

	SUBCASE("Successful rename") {
		animation_library->add_animation(good_name_1, new_animation_1);
		animation_library->rename_animation(good_name_1, good_name_2);

		SIGNAL_CHECK(SNAME(signal_animation_renamed), build_array(build_array(StringName(good_name_1), StringName(good_name_2))));
		CHECK_FALSE(animation_library->has_animation(good_name_1));
		CHECK(animation_library->has_animation(good_name_2));
	}

	SIGNAL_UNWATCH(animation_library.ptr(), SNAME(signal_animation_renamed));
}

TEST_CASE("[AnimationLibrary] Get animation") {
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	Ref<Animation> new_animation_1 = memnew(Animation);
	SUBCASE("No aimation") {
		ERR_PRINT_OFF;
		CHECK_EQ(animation_library->get_animation(good_name_1).ptr(), nullptr);
		ERR_PRINT_ON;
	}

	SUBCASE("Successful returned") {
		animation_library->add_animation(good_name_1, new_animation_1);
		CHECK_EQ(animation_library->get_animation(good_name_1).ptr(), new_animation_1.ptr());
	}
}

TEST_CASE("[AnimationLibrary] Get animation list") {
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	String name_list[] = {
		String("1 one"),
		String("2 two"),
		String("3 three"),
		String("4 four"),
	};
	for (const String &name : name_list) {
		Ref<Animation> animation = memnew(Animation);
		animation_library->add_animation(name, animation);
	}
	List<StringName> animation_list;
	animation_library->get_animation_list(&animation_list);
	int i = 0;
	for (const StringName &name : animation_list) {
		CHECK_EQ(StringName(name_list[i++]), name);
	}
}

TEST_CASE("[AnimationLibrary] Signal animation_changed") {
	Ref<AnimationLibrary> animation_library = memnew(AnimationLibrary);
	animation_library.instantiate();
	Ref<Animation> new_animation = memnew(Animation);
	animation_library->add_animation(good_name_1, new_animation);
	SIGNAL_WATCH(animation_library.ptr(), SNAME(signal_animation_changed));
	new_animation->add_track(Animation::TYPE_POSITION_3D);
	SIGNAL_CHECK(SNAME(signal_animation_changed), build_array(build_array(StringName(good_name_1))));
	SIGNAL_UNWATCH(animation_library.ptr(), SNAME(signal_animation_changed));
}

} //namespace TestAnimationLibrary

#endif // TEST_ANIMATION_LIBRARY_H
