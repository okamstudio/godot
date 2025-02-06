/**************************************************************************/
/*  test_occluder_instance_3d.h                                           */
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

#ifndef TEST_OCCLUDER_INSTANCE_3D_H
#define TEST_OCCLUDER_INSTANCE_3D_H

#include "core/math/vector3.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/3d/primitive_meshes.h"

#include "tests/test_macros.h"

namespace TestOccluderInstance3D {

TEST_CASE("[SceneTree][OccluderInstance3D] Test baking functionality") {
// We need to load an occluder path because when the light occluder generates
// it has to save to a file.
#ifdef WINDOWS_ENABLED
	const String occluder_path = OS::get_singleton()->get_environment("TEMP").path_join("test_occluder.occ");
#else
	const String occluder_path = "/tmp/test_occluder.occ";
#endif

	SceneTree *scene_tree = SceneTree::get_singleton();

	// Create our occluder to do tests with.
	OccluderInstance3D *occluder_instance = memnew(OccluderInstance3D);

	// Root node to put meshes under.
	Node *test_bake_scene = memnew(Node);

	scene_tree->get_root()->add_child(test_bake_scene);
	scene_tree->get_root()->add_child(occluder_instance);

	// Instantiate a mesh to generate.
	MeshInstance3D *box_mesh_instance = memnew(MeshInstance3D);
	Ref<BoxMesh> box_mesh = Ref<BoxMesh>();
	box_mesh.instantiate();
	box_mesh_instance->set_mesh(box_mesh);
	test_bake_scene->add_child(box_mesh_instance);
	box_mesh_instance->set_owner(test_bake_scene);

	// This is the vertices output that we expect for a single cube.
	const PackedVector3Array expected_vertices_output = PackedVector3Array(
			{ Vector3(-0.5, 0.5, 0.5), Vector3(0.5, 0.5, 0.5), Vector3(-0.5, -0.5, 0.5), Vector3(0.5, -0.5, 0.5), Vector3(0.5, 0.5, -0.5), Vector3(-0.5, 0.5, -0.5),
					Vector3(0.5, -0.5, -0.5), Vector3(-0.5, -0.5, -0.5) });

	// This is the indices output that we expect for a single cube.
	const PackedInt32Array expected_indices_output = PackedInt32Array(
			{ 0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 1, 4, 3, 4, 6, 3, 5, 0, 7, 0, 2, 7, 1, 0, 4, 0, 5, 4, 2, 3, 7, 3, 6, 7 });

	// Turn errors off to suppress a warning about occlusion culling disabled at build-time.
	ERR_PRINT_OFF;
	OccluderInstance3D::BakeError error = occluder_instance->bake_scene(test_bake_scene, occluder_path);
	ERR_PRINT_ON;

	// If everything's done correctly, we should get an error OK.
	CHECK_EQ(error, OccluderInstance3D::BAKE_ERROR_OK);
	CHECK_EQ(expected_vertices_output, occluder_instance->get_occluder()->get_vertices());
	CHECK_EQ(expected_indices_output, occluder_instance->get_occluder()->get_indices());

	// No path provided, we should get that error.
	ERR_PRINT_OFF;
	error = occluder_instance->bake_scene(test_bake_scene);
	ERR_PRINT_ON;
	CHECK_EQ(error, OccluderInstance3D::BAKE_ERROR_NO_SAVE_PATH);

	// No meshes, we should get that error.
	Node *empty_scene = memnew(Node);
	ERR_PRINT_OFF;
	error = occluder_instance->bake_scene(empty_scene, occluder_path);
	ERR_PRINT_ON;
	CHECK_EQ(error, OccluderInstance3D::BAKE_ERROR_NO_MESHES);

	// Tests for single node. We have to provide a vertices and indices array.
	PackedVector3Array vertices = PackedVector3Array();
	PackedInt32Array indices = PackedInt32Array();

	ERR_PRINT_OFF;
	occluder_instance->bake_single_node(box_mesh_instance, 0.1f, vertices, indices);
	ERR_PRINT_ON;

	CHECK_EQ(expected_vertices_output, vertices);
	CHECK_EQ(expected_indices_output, indices);

	// Cleanup
	scene_tree->get_root()->remove_child(test_bake_scene);
	scene_tree->get_root()->remove_child(occluder_instance);

	memdelete(empty_scene);
	memdelete(box_mesh_instance);
	memdelete(test_bake_scene);
	memdelete(occluder_instance);
}

} // namespace TestOccluderInstance3D

#endif // TEST_OCCLUDER_INSTANCE_3D_H
