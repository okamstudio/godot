/**************************************************************************/
/*  test_occluder_3d.h                                                    */
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

#ifndef TEST_OCCLUDER_3D_H
#define TEST_OCCLUDER_3D_H

#include "core/math/vector3.h"
#include "scene/3d/occluder_instance_3d.h"

#include "tests/test_macros.h"
/*  At least on my system (windows 11 laptop, compiling using msvc)
	when comparing two packedvector3array's, -0.0!=0.0
	and the sphere occluder vertices for some reason have random signed zeroes sprinkled about (bug?)
	so i have to use this function to compare the arrays using is_equal_approx  */
bool is_packedVector3Array_equal(PackedVector3Array check_one, PackedVector3Array check_two) {
	MESSAGE("equal check started");
	for (int y = 0; y < check_one.size(); y++) {
		MESSAGE("size check");
		if (check_two.size() < check_one.size() || check_two.size() > check_one.size()) {
			return false;
		};
		MESSAGE("item check");
		if (not check_one[y].is_equal_approx(check_two[y])) {
			return false;
		};
		MESSAGE("item check done");
	};
	return true;
}
namespace Occluder3DTest {

TEST_CASE("[SceneTree][Occluder3D] BoxOccluder3D") {
	// Define the expected shape
	PackedInt32Array expected_indices = PackedInt32Array(
			{ 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7, 4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3 });
	PackedVector3Array expected_vertices = PackedVector3Array(
			{ Vector3(-1.0, -1.0, 1.0), Vector3(1.0, -1.0, 1.0), Vector3(1.0, 1.0, 1.0), Vector3(-1.0, 1.0, 1.0), Vector3(-1.0, -1.0, -1.0), Vector3(1.0, -1.0, -1.0), Vector3(1.0, 1.0, -1.0), Vector3(-1.0, 1.0, -1.0) });

	//suppress error about occlusion culling
	//unrelated to testing the generation of indices and vertices
	ERR_PRINT_OFF;
	// Create the test BoxOccluder3D and set the size
	BoxOccluder3D *TestBox = memnew(BoxOccluder3D);
	TestBox->set_size(Vector3(2, 2, 2));
	ERR_PRINT_ON;

	// Get its indices and vertices
	PackedInt32Array returned_indices = TestBox->get_indices();
	PackedVector3Array returned_vertices = TestBox->get_vertices();

	//compare with expected results
	CHECK(returned_indices == expected_indices);
	CHECK(returned_vertices == expected_vertices);

	memdelete(TestBox);
}
TEST_CASE("[SceneTree][Occluder3D] QuadOccluder3D") {
	// Define expected shape
	PackedInt32Array expected_indices = PackedInt32Array({ 0, 1, 2, 0, 2, 3 });
	PackedVector3Array expected_vertices = PackedVector3Array(
			{ Vector3(-1.0, -1.0, 0.0), Vector3(-1.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0), Vector3(1.0, -1.0, 0.0) });

	//suppress error about occlusion culling
	//unrelated to testing the generation of indices and vertices
	ERR_PRINT_OFF;
	// Create QuadOcculuder
	QuadOccluder3D *TestQuad = memnew(QuadOccluder3D);
	TestQuad->set_size(Vector2(2, 2));
	ERR_PRINT_ON;

	// Get returned indices and vertices
	PackedInt32Array returned_indices = TestQuad->get_indices();
	PackedVector3Array returned_vertices = TestQuad->get_vertices();

	// Check with expected result
	CHECK_EQ(returned_indices, expected_indices);
	CHECK_EQ(returned_vertices, expected_vertices);

	memdelete(TestQuad);
}
TEST_CASE("[SceneTree][Occluder3D] SphereOccluder3D") {
	// Define the expected shape
	PackedInt32Array expected_indices = PackedInt32Array({ 0, 1, 8, 1, 9, 8, 1, 2, 9, 2, 10, 9, 2, 3, 10, 3, 11, 10, 3, 4, 11, 4, 12, 11, 4, 5, 12, 5, 13, 12, 5, 6, 13, 6, 14, 13, 6, 7, 14, 7, 15, 14, 8, 9, 16, 9, 17, 16, 9, 10, 17, 10, 18, 17, 10, 11, 18, 11, 19, 18, 11, 12, 19, 12, 20, 19, 12, 13, 20, 13, 21, 20, 13, 14, 21, 14, 22, 21, 14, 15, 22, 15, 23, 22, 16, 17, 24, 17, 25, 24, 17, 18, 25, 18, 26, 25, 18, 19, 26, 19, 27, 26, 19, 20, 27, 20, 28, 27, 20, 21, 28, 21, 29, 28, 21, 22, 29, 22, 30, 29, 22, 23, 30, 23, 31, 30, 24, 25, 32, 25, 33, 32, 25, 26, 33, 26, 34, 33, 26, 27, 34, 27, 35, 34, 27, 28, 35, 28, 36, 35, 28, 29, 36, 29, 37, 36, 29, 30, 37, 30, 38, 37, 30, 31, 38, 31, 39, 38, 32, 33, 40, 33, 41, 40, 33, 34, 41, 34, 42, 41, 34, 35, 42, 35, 43, 42, 35, 36, 43, 36, 44, 43, 36, 37, 44, 37, 45, 44, 37, 38, 45, 38, 46, 45, 38, 39, 46, 39, 47, 46, 40, 41, 48, 41, 49, 48, 41, 42, 49, 42, 50, 49, 42, 43, 50, 43, 51, 50, 43, 44, 51, 44, 52, 51, 44, 45, 52, 45, 53, 52, 45, 46, 53, 46, 54, 53, 46, 47, 54, 47, 55, 54, 48, 49, 56, 49, 57, 56, 49, 50, 57, 50, 58, 57, 50, 51, 58, 51, 59, 58, 51, 52, 59, 52, 60, 59, 52, 53, 60, 53, 61, 60, 53, 54, 61, 54, 62, 61, 54, 55, 62, 55, 63, 62, 56, 57, 64, 57, 65, 64, 57, 58, 65, 58, 66, 65, 58, 59, 66, 59, 67, 66, 59, 60, 67, 60, 68, 67, 60, 61, 68, 61, 69, 68, 61, 62, 69, 62, 70, 69, 62, 63, 70, 63, 71, 70 });
	PackedVector3Array expected_vertices = PackedVector3Array({
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.0, 1.0, 0.0),
			Vector3(0.382683, 0.92388, 0),
			Vector3(0.238599, 0.92388, 0.299194),
			Vector3(-0.085155, 0.92388, 0.373089),
			Vector3(-0.344786, 0.92388, 0.16604),
			Vector3(-0.344786, 0.92388, -0.16604),
			Vector3(-0.085155, 0.92388, -0.373089),
			Vector3(0.238599, 0.92388, -0.299194),
			Vector3(0.382683, 0.92388, -0),
			Vector3(0.707107, 0.707107, 0),
			Vector3(0.440874, 0.707107, 0.552838),
			Vector3(-0.157346, 0.707107, 0.689378),
			Vector3(-0.637081, 0.707107, 0.306802),
			Vector3(-0.637081, 0.707107, -0.306802),
			Vector3(-0.157346, 0.707107, -0.689378),
			Vector3(0.440874, 0.707107, -0.552838),
			Vector3(0.707107, 0.707107, -0),
			Vector3(0.92388, 0.382683, 0),
			Vector3(0.576029, 0.382683, 0.722318),
			Vector3(-0.205583, 0.382683, 0.900716),
			Vector3(-0.832387, 0.382683, 0.400856),
			Vector3(-0.832387, 0.382683, -0.400856),
			Vector3(-0.205582, 0.382683, -0.900716),
			Vector3(0.576029, 0.382683, -0.722318),
			Vector3(0.92388, 0.382683, -0),
			Vector3(1.0, 0, 0),
			Vector3(0.62349, 0, 0.781832),
			Vector3(-0.222521, 0, 0.974928),
			Vector3(-0.900969, 0, 0.433884),
			Vector3(-0.900969, 0, -0.433884),
			Vector3(-0.222521, 0, -0.974928),
			Vector3(0.62349, 0, -0.781831),
			Vector3(1, 0, -0),
			Vector3(0.92388, -0.382683, 0),
			Vector3(0.576029, -0.382683, 0.722318),
			Vector3(-0.205583, -0.382683, 0.900716),
			Vector3(-0.832387, -0.382683, 0.400856),
			Vector3(-0.832387, -0.382683, -0.400856),
			Vector3(-0.205582, -0.382683, -0.900716),
			Vector3(0.576029, -0.382683, -0.722318),
			Vector3(0.92388, -0.382683, -0),
			Vector3(0.707107, -0.707107, 0),
			Vector3(0.440874, -0.707107, 0.552838),
			Vector3(-0.157346, -0.707107, 0.689378),
			Vector3(-0.637081, -0.707107, 0.306802),
			Vector3(-0.637081, -0.707107, -0.306802),
			Vector3(-0.157346, -0.707107, -0.689378),
			Vector3(0.440874, -0.707107, -0.552838),
			Vector3(0.707107, -0.707107, -0),
			Vector3(0.382683, -0.92388, 0),
			Vector3(0.238599, -0.92388, 0.299194),
			Vector3(-0.085155, -0.92388, 0.373089),
			Vector3(-0.344786, -0.92388, 0.16604),
			Vector3(-0.344786, -0.92388, -0.16604),
			Vector3(-0.085155, -0.92388, -0.373089),
			Vector3(0.238599, -0.92388, -0.299194),
			Vector3(0.382683, -0.92388, -0),
			Vector3(0, -1.0, 0),
			Vector3(0, -1.0, 0),
			Vector3(-0, -1.0, 0),
			Vector3(-0, -1.0, 0),
			Vector3(-0, -1.0, 0),
			Vector3(-0, -1.0, 0),
			Vector3(0, -1.0, 0),
			Vector3(0, -1.0, 0),
	});

	//suppress error about occlusion culling
	//unrelated to testing the generation of indices and vertices
	ERR_PRINT_OFF;
	// Create the test sphere
	SphereOccluder3D *test_sphere = memnew(SphereOccluder3D);
	test_sphere->set_radius(1);
	ERR_PRINT_ON;

	// Get the indices and vertices
	PackedInt32Array returned_indices = test_sphere->get_indices();
	PackedVector3Array returned_vertices = test_sphere->get_vertices();

	// Compare with expected results
	CHECK(is_packedVector3Array_equal(expected_vertices, returned_vertices));
	CHECK_EQ(expected_indices, returned_indices);

	memdelete(test_sphere);
}
TEST_CASE("[SceneTree][Occluder3D] PolygonOccluder3D") {
	// Define expected shape
	PackedInt32Array expected_indices = PackedInt32Array({ 0, 2, 1 });
	PackedVector3Array expected_vertices = PackedVector3Array({ Vector3(0.0, -2.0, 0), Vector3(0.0, 2.0, 0.0), Vector3(6.0, -2.0, 0.0) });

	//suppress error about occlusion culling
	//unrelated to testing the generation of indices and vertices
	ERR_PRINT_OFF;
	// Create polygon
	PolygonOccluder3D *TestPolygon = memnew(PolygonOccluder3D);
	TestPolygon->set_polygon(PackedVector2Array({ Vector2(0, -2.0), Vector2(0, 2.0), Vector2(6.0, -2.0) }));
	ERR_PRINT_ON;

	// Get the indices and vertices
	PackedInt32Array returned_indices = TestPolygon->get_indices();
	PackedVector3Array returned_vertices = TestPolygon->get_vertices();

	// Compare with expected results
	CHECK_EQ(returned_indices, expected_indices);
	CHECK_EQ(returned_vertices, expected_vertices);

	memdelete(TestPolygon);
}
TEST_CASE("[SceneTree][Occluder3D] ArrayOccluder3D") {
	// Define expected shape
	PackedInt32Array expected_indices = PackedInt32Array({ 0, 2, 1 });
	PackedVector3Array expected_vertices = PackedVector3Array({ Vector3(0, -2, 0), Vector3(0, 2, 0), Vector3(6, -2, 0) });

	//suppress error about occlusion culling
	//unrelated to testing the generation of indices and vertices
	ERR_PRINT_OFF;
	// Create polygon
	ArrayOccluder3D *TestPolygon = memnew(ArrayOccluder3D);
	TestPolygon->set_indices(PackedInt32Array({ 0, 2, 1 }));
	TestPolygon->set_vertices(PackedVector3Array({ Vector3(0, -2.0, 0), Vector3(0, 2.0, 0), Vector3(6, -2, 0) }));
	ERR_PRINT_ON;

	// Get indices and vertices
	PackedInt32Array returned_indices = TestPolygon->get_indices();
	PackedVector3Array returned_vertices = TestPolygon->get_vertices();

	// compare with expected results
	CHECK_EQ(returned_indices, expected_indices);
	CHECK_EQ(returned_vertices, expected_vertices);

	memdelete(TestPolygon);
}
} // namespace Occluder3DTest
#endif // TEST_OCCLUDER_3D_H
