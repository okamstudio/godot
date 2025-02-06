/**************************************************************************/
/*  test_skeleton_3d.h                                                    */
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

#include "tests/test_macros.h"

#include "scene/3d/skeleton_3d.h"

namespace TestSkeleton3D {

TEST_CASE("[Skeleton3D] Test per-bone meta") {
	Skeleton3D *skeleton = memnew(Skeleton3D);
	skeleton->add_bone("root");
	skeleton->set_bone_rest(0, Transform3D());

	// Adding meta to bone.
	skeleton->set_bone_meta(0, "key1", "value1");
	skeleton->set_bone_meta(0, "key2", 12345);
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Rename bone and check if meta persists.
	skeleton->set_bone_name(0, "renamed_root");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Retrieve list of keys.
	List<StringName> keys;
	skeleton->get_bone_meta_list(0, &keys);
	CHECK_MESSAGE(keys.size() == 2, "Wrong number of bone meta keys.");
	CHECK_MESSAGE(keys.find("key1"), "key1 not found in bone meta list");
	CHECK_MESSAGE(keys.find("key2"), "key2 not found in bone meta list");

	// Removing meta.
	skeleton->set_bone_meta(0, "key1", Variant());
	skeleton->set_bone_meta(0, "key2", Variant());
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key1"), "Bone meta key1 should be deleted.");
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key2"), "Bone meta key2 should be deleted.");
	List<StringName> should_be_empty_keys;
	skeleton->get_bone_meta_list(0, &should_be_empty_keys);
	CHECK_MESSAGE(should_be_empty_keys.size() == 0, "Wrong number of bone meta keys.");

	// Deleting non-existing key should succeed.
	skeleton->set_bone_meta(0, "non-existing-key", Variant());
	memdelete(skeleton);
}
} // namespace TestSkeleton3D
