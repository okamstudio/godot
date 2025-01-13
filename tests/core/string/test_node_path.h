/**************************************************************************/
/*  test_node_path.h                                                      */
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

#ifndef TEST_NODE_PATH_H
#define TEST_NODE_PATH_H

#include "core/string/node_path.h"

#include "tests/test_macros.h"

namespace TestNodePath {

TEST_CASE("[NodePath] Relative path") {
	const NodePath node_path_relative = NodePath("Path2D/PathFollow2D/Sprite2D:position:x");

	CHECK_MESSAGE(
			node_path_relative.get_as_property_path() == NodePath(":Path2D/PathFollow2D/Sprite2D:position:x"),
			"The returned property path should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_concatenated_subnames() == "position:x",
			"The returned concatenated subnames should match the expected value.");

	CHECK_MESSAGE(
			node_path_relative.get_name(0) == "Path2D",
			"The returned name at index 0 should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_name(1) == "PathFollow2D",
			"The returned name at index 1 should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_name(2) == "Sprite2D",
			"The returned name at index 2 should match the expected value.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			node_path_relative.get_name(3) == "",
			"The returned name at invalid index 3 should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_name(-1) == "",
			"The returned name at invalid index -1 should match the expected value.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			node_path_relative.get_name_count() == 3,
			"The returned number of names should match the expected value.");

	CHECK_MESSAGE(
			node_path_relative.get_subname(0) == "position",
			"The returned subname at index 0 should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_subname(1) == "x",
			"The returned subname at index 1 should match the expected value.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			node_path_relative.get_subname(2) == "",
			"The returned subname at invalid index 2 should match the expected value.");
	CHECK_MESSAGE(
			node_path_relative.get_subname(-1) == "",
			"The returned subname at invalid index -1 should match the expected value.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			node_path_relative.get_subname_count() == 2,
			"The returned number of subnames should match the expected value.");

	CHECK_MESSAGE(
			!node_path_relative.is_absolute(),
			"The node path should be considered relative.");

	CHECK_MESSAGE(
			!node_path_relative.is_empty(),
			"The node path shouldn't be considered empty.");
}

TEST_CASE("[NodePath] Absolute path") {
	const NodePath node_path_absolute = NodePath("/root/Sprite2D");

	CHECK_MESSAGE(
			node_path_absolute.get_as_property_path() == NodePath(":root/Sprite2D"),
			"The returned property path should match the expected value.");
	CHECK_MESSAGE(
			node_path_absolute.get_concatenated_subnames() == "",
			"The returned concatenated subnames should match the expected value.");

	CHECK_MESSAGE(
			node_path_absolute.get_name(0) == "root",
			"The returned name at index 0 should match the expected value.");
	CHECK_MESSAGE(
			node_path_absolute.get_name(1) == "Sprite2D",
			"The returned name at index 1 should match the expected value.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			node_path_absolute.get_name(2) == "",
			"The returned name at invalid index 2 should match the expected value.");
	CHECK_MESSAGE(
			node_path_absolute.get_name(-1) == "",
			"The returned name at invalid index -1 should match the expected value.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			node_path_absolute.get_name_count() == 2,
			"The returned number of names should match the expected value.");

	CHECK_MESSAGE(
			node_path_absolute.get_subname_count() == 0,
			"The returned number of subnames should match the expected value.");

	CHECK_MESSAGE(
			node_path_absolute.is_absolute(),
			"The node path should be considered absolute.");

	CHECK_MESSAGE(
			!node_path_absolute.is_empty(),
			"The node path shouldn't be considered empty.");
}

TEST_CASE("[NodePath] Empty path") {
	const NodePath node_path_empty = NodePath();

	CHECK_MESSAGE(
			node_path_empty.get_as_property_path() == NodePath(),
			"The returned property path should match the expected value.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			node_path_empty.get_concatenated_subnames() == "",
			"The returned concatenated subnames should match the expected value.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			node_path_empty.get_name_count() == 0,
			"The returned number of names should match the expected value.");

	CHECK_MESSAGE(
			node_path_empty.get_subname_count() == 0,
			"The returned number of subnames should match the expected value.");

	CHECK_MESSAGE(
			!node_path_empty.is_absolute(),
			"The node path shouldn't be considered absolute.");

	CHECK_MESSAGE(
			node_path_empty.is_empty(),
			"The node path should be considered empty.");
}

TEST_CASE("[NodePath] Slice") {
	const NodePath node_path_relative = NodePath("Parent/Child:prop:subprop");
	const NodePath node_path_absolute = NodePath("/root/Parent/Child:prop");
	CHECK_MESSAGE(
			node_path_relative.slice(0, 2) == NodePath("Parent/Child"),
			"The slice lower bound should be inclusive and the slice upper bound should be exclusive.");
	CHECK_MESSAGE(
			node_path_relative.slice(3) == NodePath(":subprop"),
			"Slicing on the last index (length - 1) should return the last entry.");
	CHECK_MESSAGE(
			node_path_relative.slice(1) == NodePath("Child:prop:subprop"),
			"Slicing without upper bound should return remaining entries after index.");
	CHECK_MESSAGE(
			node_path_relative.slice(1, 3) == NodePath("Child:prop"),
			"Slicing should include names and subnames.");
	CHECK_MESSAGE(
			node_path_relative.slice(-1) == NodePath(":subprop"),
			"Slicing on -1 should return the last entry.");
	CHECK_MESSAGE(
			node_path_relative.slice(0, -1) == NodePath("Parent/Child:prop"),
			"Slicing up to -1 should include the second-to-last entry.");
	CHECK_MESSAGE(
			node_path_relative.slice(-2, -1) == NodePath(":prop"),
			"Slicing from negative to negative should treat lower bound as inclusive and upper bound as exclusive.");
	CHECK_MESSAGE(
			node_path_relative.slice(0, 10) == NodePath("Parent/Child:prop:subprop"),
			"Slicing past the length of the path should work like slicing up to the last entry.");
	CHECK_MESSAGE(
			node_path_relative.slice(-10, 2) == NodePath("Parent/Child"),
			"Slicing negatively past the length of the path should work like slicing from the first entry.");
	CHECK_MESSAGE(
			node_path_relative.slice(1, 1) == NodePath(""),
			"Slicing with a lower bound equal to upper bound should return empty path.");

	CHECK_MESSAGE(
			node_path_absolute.slice(0, 2) == NodePath("/root/Parent"),
			"Slice from beginning of an absolute path should be an absolute path.");
	CHECK_MESSAGE(
			node_path_absolute.slice(1, 4) == NodePath("Parent/Child:prop"),
			"Slice of an absolute path that does not start at the beginning should be a relative path.");
	CHECK_MESSAGE(
			node_path_absolute.slice(3, 4) == NodePath(":prop"),
			"Slice of an absolute path that does not start at the beginning should be a relative path.");

	CHECK_MESSAGE(
			NodePath("").slice(0, 1) == NodePath(""),
			"Slice of an empty path should be an empty path.");
	CHECK_MESSAGE(
			NodePath("").slice(-1, 2) == NodePath(""),
			"Slice of an empty path should be an empty path.");
	CHECK_MESSAGE(
			NodePath("/").slice(-1, 2) == NodePath("/"),
			"Slice of an empty absolute path should be an empty absolute path.");
}

} // namespace TestNodePath

#endif // TEST_NODE_PATH_H
