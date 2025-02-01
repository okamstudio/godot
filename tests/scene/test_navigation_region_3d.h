/**************************************************************************/
/*  test_navigation_region_3d.h                                           */
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

#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation_region_3d.h"
#include "scene/main/window.h"
#include "scene/resources/3d/primitive_meshes.h"

#include "tests/test_macros.h"

namespace TestNavigationRegion3D {

TEST_SUITE("[Navigation]") {
	TEST_CASE("[SceneTree][NavigationRegion3D] New region should have valid RID") {
		NavigationRegion3D *region_node = memnew(NavigationRegion3D);
		CHECK(region_node->get_rid().is_valid());
		memdelete(region_node);
	}

	TEST_CASE("[SceneTree][NavigationRegion3D] Region should bake successfully from valid geometry") {
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);
		Ref<NavigationMesh> navigation_mesh = memnew(NavigationMesh);
		NavigationRegion3D *navigation_region = memnew(NavigationRegion3D);
		navigation_region->set_navigation_mesh(navigation_mesh);
		node_3d->add_child(navigation_region);
		Ref<PlaneMesh> plane_mesh = memnew(PlaneMesh);
		plane_mesh->set_size(Size2(10.0, 10.0));
		MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_mesh(plane_mesh);
		navigation_region->add_child(mesh_instance);

		CHECK_FALSE(navigation_region->is_baking());
		CHECK_EQ(navigation_mesh->get_polygon_count(), 0);
		CHECK_EQ(navigation_mesh->get_vertices().size(), 0);

		SUBCASE("Synchronous bake should have immediate effects") {
			ERR_PRINT_OFF; // Suppress warning about baking from visual meshes as source geometry.
			navigation_region->bake_navigation_mesh(false);
			ERR_PRINT_ON;
			CHECK_FALSE(navigation_region->is_baking());
			CHECK_NE(navigation_mesh->get_polygon_count(), 0);
			CHECK_NE(navigation_mesh->get_vertices().size(), 0);
		}

		memdelete(mesh_instance);
		memdelete(navigation_region);
		memdelete(node_3d);
	}
}

} //namespace TestNavigationRegion3D
