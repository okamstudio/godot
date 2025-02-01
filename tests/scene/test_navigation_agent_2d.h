/**************************************************************************/
/*  test_navigation_agent_2d.h                                            */
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

#include "scene/2d/navigation_agent_2d.h"
#include "scene/2d/node_2d.h"
#include "scene/main/window.h"
#include "scene/resources/world_2d.h"

#include "tests/test_macros.h"

namespace TestNavigationAgent2D {

TEST_SUITE("[Navigation]") {
	TEST_CASE("[SceneTree][NavigationAgent2D] New agent should have valid RID") {
		NavigationAgent2D *agent_node = memnew(NavigationAgent2D);
		CHECK(agent_node->get_rid().is_valid());
		memdelete(agent_node);
	}

	TEST_CASE("[SceneTree][NavigationAgent2D] New agent should attach to default map") {
		Node2D *node_2d = memnew(Node2D);
		SceneTree::get_singleton()->get_root()->add_child(node_2d);

		NavigationAgent2D *agent_node = memnew(NavigationAgent2D);

		// agent should not be attached to any map when outside of tree
		CHECK_FALSE(agent_node->get_navigation_map().is_valid());

		SUBCASE("Agent should attach to default map when it enters the tree") {
			node_2d->add_child(agent_node);
			CHECK(agent_node->get_navigation_map().is_valid());
			CHECK(agent_node->get_navigation_map() == node_2d->get_world_2d()->get_navigation_map());
		}

		memdelete(agent_node);
		memdelete(node_2d);
	}
}

} //namespace TestNavigationAgent2D
