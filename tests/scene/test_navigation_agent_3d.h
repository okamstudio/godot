/**************************************************************************/
/*  test_navigation_agent_3d.h                                            */
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

#include "scene/3d/navigation_agent_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestNavigationAgent3D {

TEST_SUITE("[Navigation]") {
	TEST_CASE("[SceneTree][NavigationAgent3D] New agent should have valid RID") {
		NavigationAgent3D *agent_node = memnew(NavigationAgent3D);
		CHECK(agent_node->get_rid().is_valid());
		memdelete(agent_node);
	}

	TEST_CASE("[SceneTree][NavigationAgent3D] New agent should attach to default map") {
		Node3D *node_3d = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(node_3d);

		NavigationAgent3D *agent_node = memnew(NavigationAgent3D);

		// agent should not be attached to any map when outside of tree
		CHECK_FALSE(agent_node->get_navigation_map().is_valid());

		SUBCASE("Agent should attach to default map when it enters the tree") {
			node_3d->add_child(agent_node);
			CHECK(agent_node->get_navigation_map().is_valid());
			CHECK(agent_node->get_navigation_map() == node_3d->get_world_3d()->get_navigation_map());
		}

		memdelete(agent_node);
		memdelete(node_3d);
	}
}

} //namespace TestNavigationAgent3D
