/**************************************************************************/
/*  test_node.h                                                           */
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

#include "core/object/class_db.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

#include "tests/test_macros.h"

namespace TestNode {

class TestNode : public Node {
	GDCLASS(TestNode, Node);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_INTERNAL_PROCESS: {
				internal_process_counter++;
				push_self();
			} break;
			case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
				internal_physics_process_counter++;
				push_self();
			} break;
			case NOTIFICATION_PROCESS: {
				process_counter++;
				push_self();
			} break;
			case NOTIFICATION_PHYSICS_PROCESS: {
				physics_process_counter++;
				push_self();
			} break;
		}
	}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_exported_node", "node"), &TestNode::set_exported_node);
		ClassDB::bind_method(D_METHOD("get_exported_node"), &TestNode::get_exported_node);
		ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "exported_node", PROPERTY_HINT_NODE_TYPE, "Node"), "set_exported_node", "get_exported_node");

		ClassDB::bind_method(D_METHOD("set_exported_nodes", "node"), &TestNode::set_exported_nodes);
		ClassDB::bind_method(D_METHOD("get_exported_nodes"), &TestNode::get_exported_nodes);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exported_nodes", PROPERTY_HINT_TYPE_STRING, "24/34:Node"), "set_exported_nodes", "get_exported_nodes");
	}

private:
	void push_self() {
		if (callback_list) {
			callback_list->push_back(this);
		}
	}

public:
	int internal_process_counter = 0;
	int internal_physics_process_counter = 0;
	int process_counter = 0;
	int physics_process_counter = 0;

	Node *exported_node = nullptr;
	Array exported_nodes;

	List<Node *> *callback_list = nullptr;

	void set_exported_node(Node *p_node) { exported_node = p_node; }
	Node *get_exported_node() const { return exported_node; }

	void set_exported_nodes(const Array &p_nodes) { exported_nodes = p_nodes; }
	Array get_exported_nodes() const { return exported_nodes; }
};

TEST_CASE("[SceneTree][Node] Testing node operations with a very simple scene tree") {
	Node *node = memnew(Node);

	// Check initial scene tree setup.
	CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 0);
	CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 1);

	// Check initial node setup.
	CHECK(node->get_name() == StringName());
	CHECK_FALSE(node->is_inside_tree());
	CHECK_EQ(node->get_parent(), nullptr);
	ERR_PRINT_OFF;
	CHECK(node->get_path().is_empty());
	ERR_PRINT_ON;
	CHECK_EQ(node->get_child_count(), 0);

	SceneTree::get_singleton()->get_root()->add_child(node);

	CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 1);
	CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 2);

	CHECK(node->get_name() != StringName());
	CHECK(node->is_inside_tree());
	CHECK_EQ(SceneTree::get_singleton()->get_root(), node->get_parent());
	CHECK_FALSE(node->get_path().is_empty());
	CHECK_EQ(node->get_child_count(), 0);

	SUBCASE("Node should be accessible as first child") {
		Node *child = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child, node);
	}

	SUBCASE("Node should be accessible via the node path") {
		Node *child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(node->get_path());
		CHECK_EQ(child_by_path, node);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("Node"));
		CHECK_EQ(child_by_path, nullptr);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("/root/Node"));
		CHECK_EQ(child_by_path, nullptr);

		node->set_name("Node");

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(node->get_path());
		CHECK_EQ(child_by_path, node);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("Node"));
		CHECK_EQ(child_by_path, node);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("/root/Node"));
		CHECK_EQ(child_by_path, node);
	}

	SUBCASE("Node should be accessible via group") {
		List<Node *> nodes;
		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK(nodes.is_empty());

		node->add_to_group("nodes");

		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK_EQ(nodes.size(), 1);
		List<Node *>::Element *E = nodes.front();
		CHECK_EQ(E->get(), node);
	}

	SUBCASE("Node should be possible to find") {
		Node *child = SceneTree::get_singleton()->get_root()->find_child("Node", true, false);
		CHECK_EQ(child, nullptr);

		node->set_name("Node");

		child = SceneTree::get_singleton()->get_root()->find_child("Node", true, false);
		CHECK_EQ(child, node);
	}

	SUBCASE("Node should be possible to remove") {
		SceneTree::get_singleton()->get_root()->remove_child(node);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 0);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 1);

		CHECK_FALSE(node->is_inside_tree());
		CHECK_EQ(node->get_parent(), nullptr);
		ERR_PRINT_OFF;
		CHECK(node->get_path().is_empty());
		ERR_PRINT_ON;
	}

	SUBCASE("Node should be possible to move") {
		SceneTree::get_singleton()->get_root()->move_child(node, 0);

		Node *child = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child, node);
		CHECK(node->is_inside_tree());
	}

	SUBCASE("Node should be possible to reparent") {
		node->reparent(SceneTree::get_singleton()->get_root());

		Node *child = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child, node);
		CHECK(node->is_inside_tree());
	}

	SUBCASE("Node should be possible to duplicate") {
		node->set_name("MyName");

		Node *duplicate = node->duplicate();

		CHECK_FALSE(node == duplicate);
		CHECK_FALSE(duplicate->is_inside_tree());
		CHECK_EQ(duplicate->get_name(), node->get_name());

		memdelete(duplicate);
	}

	memdelete(node);
}

TEST_CASE("[SceneTree][Node] Testing node operations with a more complex simple scene tree") {
	Node *node1 = memnew(Node);
	Node *node2 = memnew(Node);
	Node *node1_1 = memnew(Node);

	SceneTree::get_singleton()->get_root()->add_child(node1);
	SceneTree::get_singleton()->get_root()->add_child(node2);

	node1->add_child(node1_1);

	CHECK(node1_1->is_inside_tree());
	CHECK_EQ(node1_1->get_parent(), node1);
	CHECK_EQ(node1->get_child_count(), 1);

	CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 2);
	CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 4);

	SUBCASE("Nodes should be accessible via get_child(..)") {
		Node *child1 = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child1, node1);

		Node *child2 = SceneTree::get_singleton()->get_root()->get_child(1);
		CHECK_EQ(child2, node2);

		Node *child1_1 = node1->get_child(0);
		CHECK_EQ(child1_1, node1_1);
	}

	SUBCASE("Removed nodes should also remove their children from the scene tree") {
		// Should also remove node1_1 from the scene tree.
		SceneTree::get_singleton()->get_root()->remove_child(node1);

		CHECK_EQ(node1->get_child_count(), 1);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 1);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 2);

		// First child should now be the second node.
		Node *child1 = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child1, node2);
	}

	SUBCASE("Removed children nodes should not affect their parent in the scene tree") {
		node1->remove_child(node1_1);

		CHECK_EQ(node1_1->get_parent(), nullptr);
		CHECK_EQ(node1->get_child_count(), 0);

		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 3);
	}

	SUBCASE("Nodes should be in the expected order when a node is moved to the back") {
		SceneTree::get_singleton()->get_root()->move_child(node1, 1);

		Node *child1 = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child1, node2);

		Node *child2 = SceneTree::get_singleton()->get_root()->get_child(1);
		CHECK_EQ(child2, node1);
	}

	SUBCASE("Nodes should be in the expected order when a node is moved to the front") {
		SceneTree::get_singleton()->get_root()->move_child(node2, 0);

		Node *child1 = SceneTree::get_singleton()->get_root()->get_child(0);
		CHECK_EQ(child1, node2);

		Node *child2 = SceneTree::get_singleton()->get_root()->get_child(1);
		CHECK_EQ(child2, node1);
	}

	SUBCASE("Nodes should be in the expected order when reparented (remove/add)") {
		CHECK_EQ(node2->get_child_count(), 0);

		node1->remove_child(node1_1);
		CHECK_EQ(node1->get_child_count(), 0);
		CHECK_EQ(node1_1->get_parent(), nullptr);

		node2->add_child(node1_1);
		CHECK_EQ(node2->get_child_count(), 1);
		CHECK_EQ(node1_1->get_parent(), node2);

		Node *child = node2->get_child(0);
		CHECK_EQ(child, node1_1);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 2);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 4);
	}

	SUBCASE("Nodes should be in the expected order when reparented") {
		CHECK_EQ(node2->get_child_count(), 0);

		node1_1->reparent(node2);

		CHECK_EQ(node1->get_child_count(), 0);
		CHECK_EQ(node2->get_child_count(), 1);
		CHECK_EQ(node1_1->get_parent(), node2);

		Node *child = node2->get_child(0);
		CHECK_EQ(child, node1_1);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 2);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 4);
	}

	SUBCASE("Nodes should be possible to find") {
		Node *child = SceneTree::get_singleton()->get_root()->find_child("NestedNode", true, false);
		CHECK_EQ(child, nullptr);

		TypedArray<Node> children = SceneTree::get_singleton()->get_root()->find_children("NestedNode", "", true, false);
		CHECK_EQ(children.size(), 0);

		node1->set_name("Node1");
		node2->set_name("Node2");
		node1_1->set_name("NestedNode");

		child = SceneTree::get_singleton()->get_root()->find_child("NestedNode", true, false);
		CHECK_EQ(child, node1_1);

		children = SceneTree::get_singleton()->get_root()->find_children("NestedNode", "", true, false);
		CHECK_EQ(children.size(), 1);
		CHECK_EQ(Object::cast_to<Node>(children[0]), node1_1);

		// First node that matches with the name is node1.
		child = SceneTree::get_singleton()->get_root()->find_child("Node?", true, false);
		CHECK_EQ(child, node1);

		children = SceneTree::get_singleton()->get_root()->find_children("Node?", "", true, false);
		CHECK_EQ(children.size(), 2);
		CHECK_EQ(Object::cast_to<Node>(children[0]), node1);
		CHECK_EQ(Object::cast_to<Node>(children[1]), node2);

		SceneTree::get_singleton()->get_root()->move_child(node2, 0);

		// It should be node2, as it is now the first one in the tree.
		child = SceneTree::get_singleton()->get_root()->find_child("Node?", true, false);
		CHECK_EQ(child, node2);

		children = SceneTree::get_singleton()->get_root()->find_children("Node?", "", true, false);
		CHECK_EQ(children.size(), 2);
		CHECK_EQ(Object::cast_to<Node>(children[0]), node2);
		CHECK_EQ(Object::cast_to<Node>(children[1]), node1);
	}

	SUBCASE("Nodes should be accessible via their node path") {
		Node *child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(node1->get_path());
		CHECK_EQ(child_by_path, node1);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(node2->get_path());
		CHECK_EQ(child_by_path, node2);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(node1_1->get_path());
		CHECK_EQ(child_by_path, node1_1);

		node1->set_name("Node1");
		node1_1->set_name("NestedNode");

		child_by_path = node1->get_node_or_null(NodePath("NestedNode"));
		CHECK_EQ(child_by_path, node1_1);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("/root/Node1/NestedNode"));
		CHECK_EQ(child_by_path, node1_1);

		child_by_path = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("Node1/NestedNode"));
		CHECK_EQ(child_by_path, node1_1);
	}

	SUBCASE("Nodes should be accessible via their groups") {
		List<Node *> nodes;
		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK(nodes.is_empty());

		SceneTree::get_singleton()->get_nodes_in_group("other_nodes", &nodes);
		CHECK(nodes.is_empty());

		node1->add_to_group("nodes");
		node2->add_to_group("other_nodes");
		node1_1->add_to_group("nodes");
		node1_1->add_to_group("other_nodes");

		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK_EQ(nodes.size(), 2);

		List<Node *>::Element *E = nodes.front();
		CHECK_EQ(E->get(), node1);
		E = E->next();
		CHECK_EQ(E->get(), node1_1);

		// Clear and try again with the other group.
		nodes.clear();

		SceneTree::get_singleton()->get_nodes_in_group("other_nodes", &nodes);
		CHECK_EQ(nodes.size(), 2);

		E = nodes.front();
		CHECK_EQ(E->get(), node1_1);
		E = E->next();
		CHECK_EQ(E->get(), node2);

		// Clear and try again with the other group and one node removed.
		nodes.clear();

		node1->remove_from_group("nodes");
		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK_EQ(nodes.size(), 1);

		E = nodes.front();
		CHECK_EQ(E->get(), node1_1);
	}

	SUBCASE("Nodes added as siblings of another node should be right next to it") {
		node1->remove_child(node1_1);

		node1->add_sibling(node1_1);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 3);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 4);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child(0), node1);
		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child(1), node1_1);
		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child(2), node2);
	}

	SUBCASE("Replaced nodes should be be removed and the replacing node added") {
		SceneTree::get_singleton()->get_root()->remove_child(node2);

		node1->replace_by(node2);

		CHECK_EQ(SceneTree::get_singleton()->get_root()->get_child_count(), 1);
		CHECK_EQ(SceneTree::get_singleton()->get_node_count(), 3);

		CHECK_FALSE(node1->is_inside_tree());
		CHECK(node2->is_inside_tree());

		CHECK_EQ(node1->get_parent(), nullptr);
		CHECK_EQ(node2->get_parent(), SceneTree::get_singleton()->get_root());
		CHECK_EQ(node2->get_child_count(), 1);
		CHECK_EQ(node2->get_child(0), node1_1);
	}

	SUBCASE("Replacing nodes should keep the groups of the replaced nodes") {
		SceneTree::get_singleton()->get_root()->remove_child(node2);

		node1->add_to_group("nodes");
		node1->replace_by(node2, true);

		List<Node *> nodes;
		SceneTree::get_singleton()->get_nodes_in_group("nodes", &nodes);
		CHECK_EQ(nodes.size(), 1);

		List<Node *>::Element *E = nodes.front();
		CHECK_EQ(E->get(), node2);
	}

	SUBCASE("Duplicating a node should also duplicate the children") {
		node1->set_name("MyName1");
		node1_1->set_name("MyName1_1");
		Node *duplicate1 = node1->duplicate();

		CHECK_EQ(duplicate1->get_child_count(), node1->get_child_count());
		Node *duplicate1_1 = duplicate1->get_child(0);

		CHECK_EQ(duplicate1_1->get_child_count(), node1_1->get_child_count());

		CHECK_EQ(duplicate1->get_name(), node1->get_name());
		CHECK_EQ(duplicate1_1->get_name(), node1_1->get_name());

		CHECK_FALSE(duplicate1->is_inside_tree());
		CHECK_FALSE(duplicate1_1->is_inside_tree());

		memdelete(duplicate1_1);
		memdelete(duplicate1);
	}

	memdelete(node1_1);
	memdelete(node1);
	memdelete(node2);
}

TEST_CASE("[SceneTree][Node]Exported node checks") {
	TestNode *node = memnew(TestNode);
	SceneTree::get_singleton()->get_root()->add_child(node);

	Node *child = memnew(Node);
	child->set_name("Child");
	node->add_child(child);
	child->set_owner(node);

	Node *child2 = memnew(Node);
	child2->set_name("Child2");
	node->add_child(child2);
	child2->set_owner(node);

	Array children;
	children.append(child);

	node->set("exported_node", child);
	node->set("exported_nodes", children);

	SUBCASE("Property of duplicated node should point to duplicated child") {
		GDREGISTER_CLASS(TestNode);

		TestNode *dup = Object::cast_to<TestNode>(node->duplicate());
		Node *new_exported = Object::cast_to<Node>(dup->get("exported_node"));
		CHECK(new_exported == dup->get_child(0));

		memdelete(dup);
	}

#ifdef TOOLS_ENABLED
	SUBCASE("Saving instance with exported nodes should not store the unchanged property") {
		Ref<PackedScene> ps;
		ps.instantiate();
		ps->pack(node);

		String scene_path = TestUtils::get_temp_path("test_scene.tscn");
		ps->set_path(scene_path);

		Node *root = memnew(Node);

		Node *sub_child = ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
		root->add_child(sub_child);
		sub_child->set_owner(root);

		Ref<PackedScene> ps2;
		ps2.instantiate();
		ps2->pack(root);

		scene_path = TestUtils::get_temp_path("new_test_scene.tscn");
		ResourceSaver::save(ps2, scene_path);
		memdelete(root);

		bool is_wrong = false;
		Ref<FileAccess> fa = FileAccess::open(scene_path, FileAccess::READ);
		while (!fa->eof_reached()) {
			const String line = fa->get_line();
			if (line.begins_with("exported_node")) {
				// The property was saved, while it shouldn't.
				is_wrong = true;
				break;
			}
		}
		CHECK_FALSE(is_wrong);
	}

	SUBCASE("Saving instance with exported nodes should store property if changed") {
		Ref<PackedScene> ps;
		ps.instantiate();
		ps->pack(node);

		String scene_path = TestUtils::get_temp_path("test_scene.tscn");
		ps->set_path(scene_path);

		Node *root = memnew(Node);

		Node *sub_child = ps->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
		root->add_child(sub_child);
		sub_child->set_owner(root);

		sub_child->set("exported_node", sub_child->get_child(1));

		children = Array();
		children.append(sub_child->get_child(1));
		sub_child->set("exported_nodes", children);

		Ref<PackedScene> ps2;
		ps2.instantiate();
		ps2->pack(root);

		scene_path = TestUtils::get_temp_path("new_test_scene2.tscn");
		ResourceSaver::save(ps2, scene_path);
		memdelete(root);

		int stored_properties = 0;
		Ref<FileAccess> fa = FileAccess::open(scene_path, FileAccess::READ);
		while (!fa->eof_reached()) {
			const String line = fa->get_line();
			if (line.begins_with("exported_node")) {
				stored_properties++;
			}
		}
		CHECK_EQ(stored_properties, 2);
	}
#endif // TOOLS_ENABLED

	memdelete(node);
}

TEST_CASE("[Node] Processing checks") {
	Node *node = memnew(Node);

	SUBCASE("Processing") {
		CHECK_FALSE(node->is_processing());

		node->set_process(true);

		CHECK(node->is_processing());

		node->set_process(false);

		CHECK_FALSE(node->is_processing());
	}

	SUBCASE("Physics processing") {
		CHECK_FALSE(node->is_physics_processing());

		node->set_physics_process(true);

		CHECK(node->is_physics_processing());

		node->set_physics_process(false);

		CHECK_FALSE(node->is_physics_processing());
	}

	SUBCASE("Unhandled input processing") {
		CHECK_FALSE(node->is_processing_unhandled_input());

		node->set_process_unhandled_input(true);

		CHECK(node->is_processing_unhandled_input());

		node->set_process_unhandled_input(false);

		CHECK_FALSE(node->is_processing_unhandled_input());
	}

	SUBCASE("Input processing") {
		CHECK_FALSE(node->is_processing_input());

		node->set_process_input(true);

		CHECK(node->is_processing_input());

		node->set_process_input(false);

		CHECK_FALSE(node->is_processing_input());
	}

	SUBCASE("Unhandled key input processing") {
		CHECK_FALSE(node->is_processing_unhandled_key_input());

		node->set_process_unhandled_key_input(true);

		CHECK(node->is_processing_unhandled_key_input());

		node->set_process_unhandled_key_input(false);

		CHECK_FALSE(node->is_processing_unhandled_key_input());
	}

	SUBCASE("Shortcut input processing") {
		CHECK_FALSE(node->is_processing_shortcut_input());

		node->set_process_shortcut_input(true);

		CHECK(node->is_processing_shortcut_input());

		node->set_process_shortcut_input(false);

		CHECK_FALSE(node->is_processing_shortcut_input());
	}

	SUBCASE("Internal processing") {
		CHECK_FALSE(node->is_processing_internal());

		node->set_process_internal(true);

		CHECK(node->is_processing_internal());

		node->set_process_internal(false);

		CHECK_FALSE(node->is_processing_internal());
	}

	SUBCASE("Process priority") {
		CHECK_EQ(0, node->get_process_priority());

		node->set_process_priority(1);

		CHECK_EQ(1, node->get_process_priority());
	}

	SUBCASE("Physics process priority") {
		CHECK_EQ(0, node->get_physics_process_priority());

		node->set_physics_process_priority(1);

		CHECK_EQ(1, node->get_physics_process_priority());
	}

	memdelete(node);
}

TEST_CASE("[SceneTree][Node] Test the processing") {
	TestNode *node = memnew(TestNode);
	SceneTree::get_singleton()->get_root()->add_child(node);

	SUBCASE("No process") {
		CHECK_EQ(0, node->process_counter);
		CHECK_EQ(0, node->physics_process_counter);
	}

	SUBCASE("Process") {
		node->set_process(true);
		SceneTree::get_singleton()->process(0);

		CHECK_EQ(1, node->process_counter);
		CHECK_EQ(0, node->physics_process_counter);
		CHECK_EQ(0, node->internal_process_counter);
		CHECK_EQ(0, node->internal_physics_process_counter);
	}

	SUBCASE("Physics process") {
		node->set_physics_process(true);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(0, node->process_counter);
		CHECK_EQ(1, node->physics_process_counter);
		CHECK_EQ(0, node->internal_process_counter);
		CHECK_EQ(0, node->internal_physics_process_counter);
	}

	SUBCASE("Normal and physics process") {
		node->set_process(true);
		node->set_physics_process(true);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(1, node->process_counter);
		CHECK_EQ(1, node->physics_process_counter);
		CHECK_EQ(0, node->internal_process_counter);
		CHECK_EQ(0, node->internal_physics_process_counter);
	}

	SUBCASE("Internal, normal and physics process") {
		node->set_process_internal(true);
		node->set_physics_process_internal(true);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(0, node->process_counter);
		CHECK_EQ(0, node->physics_process_counter);
		CHECK_EQ(1, node->internal_process_counter);
		CHECK_EQ(1, node->internal_physics_process_counter);
	}

	SUBCASE("All processing") {
		node->set_process(true);
		node->set_physics_process(true);
		node->set_process_internal(true);
		node->set_physics_process_internal(true);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(1, node->process_counter);
		CHECK_EQ(1, node->physics_process_counter);
		CHECK_EQ(1, node->internal_process_counter);
		CHECK_EQ(1, node->internal_physics_process_counter);
	}

	SUBCASE("All processing twice") {
		node->set_process(true);
		node->set_physics_process(true);
		node->set_process_internal(true);
		node->set_physics_process_internal(true);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(2, node->process_counter);
		CHECK_EQ(2, node->physics_process_counter);
		CHECK_EQ(2, node->internal_process_counter);
		CHECK_EQ(2, node->internal_physics_process_counter);
	}

	SUBCASE("Enable and disable processing") {
		node->set_process(true);
		node->set_physics_process(true);
		node->set_process_internal(true);
		node->set_physics_process_internal(true);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		node->set_process(false);
		node->set_physics_process(false);
		node->set_process_internal(false);
		node->set_physics_process_internal(false);
		SceneTree::get_singleton()->process(0);
		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(1, node->process_counter);
		CHECK_EQ(1, node->physics_process_counter);
		CHECK_EQ(1, node->internal_process_counter);
		CHECK_EQ(1, node->internal_physics_process_counter);
	}

	memdelete(node);
}

TEST_CASE("[SceneTree][Node] Test the process priority") {
	List<Node *> process_order;

	TestNode *node = memnew(TestNode);
	node->callback_list = &process_order;
	SceneTree::get_singleton()->get_root()->add_child(node);

	TestNode *node2 = memnew(TestNode);
	node2->callback_list = &process_order;
	SceneTree::get_singleton()->get_root()->add_child(node2);

	TestNode *node3 = memnew(TestNode);
	node3->callback_list = &process_order;
	SceneTree::get_singleton()->get_root()->add_child(node3);

	TestNode *node4 = memnew(TestNode);
	node4->callback_list = &process_order;
	SceneTree::get_singleton()->get_root()->add_child(node4);

	SUBCASE("Process priority") {
		node->set_process(true);
		node->set_process_priority(20);
		node2->set_process(true);
		node2->set_process_priority(10);
		node3->set_process(true);
		node3->set_process_priority(40);
		node4->set_process(true);
		node4->set_process_priority(30);

		SceneTree::get_singleton()->process(0);

		CHECK_EQ(4, process_order.size());
		List<Node *>::Element *E = process_order.front();
		CHECK_EQ(E->get(), node2);
		E = E->next();
		CHECK_EQ(E->get(), node);
		E = E->next();
		CHECK_EQ(E->get(), node4);
		E = E->next();
		CHECK_EQ(E->get(), node3);
	}

	SUBCASE("Physics process priority") {
		node->set_physics_process(true);
		node->set_physics_process_priority(20);
		node2->set_physics_process(true);
		node2->set_physics_process_priority(10);
		node3->set_physics_process(true);
		node3->set_physics_process_priority(40);
		node4->set_physics_process(true);
		node4->set_physics_process_priority(30);

		SceneTree::get_singleton()->physics_process(0);

		CHECK_EQ(4, process_order.size());
		List<Node *>::Element *E = process_order.front();
		CHECK_EQ(E->get(), node2);
		E = E->next();
		CHECK_EQ(E->get(), node);
		E = E->next();
		CHECK_EQ(E->get(), node4);
		E = E->next();
		CHECK_EQ(E->get(), node3);
	}

	memdelete(node);
	memdelete(node2);
	memdelete(node3);
	memdelete(node4);
}

} // namespace TestNode
