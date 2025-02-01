/**************************************************************************/
/*  test_navigation_obstacle_3d.h                                         */
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

#include "scene/3d/navigation_obstacle_3d.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestNavigationObstacle3D {

TEST_SUITE("[Navigation]") {
	TEST_CASE("[SceneTree][NavigationObstacle3D] New obstacle should have valid RID") {
		NavigationObstacle3D *obstacle_node = memnew(NavigationObstacle3D);
		CHECK(obstacle_node->get_rid().is_valid());
		memdelete(obstacle_node);
	}

	TEST_CASE("[SceneTree][NavigationObstacle3D] New obstacle should attach to default map") {
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);

		NavigationObstacle3D *obstacle_node = memnew(NavigationObstacle3D);
		// obstacle should not be attached to any map when outside of tree
		CHECK_FALSE(obstacle_node->get_navigation_map().is_valid());

		SUBCASE("Obstacle should attach to default map when it enters the tree") {
			node_3d->add_child(obstacle_node);
			CHECK(obstacle_node->get_navigation_map().is_valid());
			CHECK(obstacle_node->get_navigation_map() == node_3d->get_world_3d()->get_navigation_map());
		}

		memdelete(obstacle_node);
		memdelete(node_3d);
	}
}

} //namespace TestNavigationObstacle3D
