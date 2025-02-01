/**************************************************************************/
/*  test_tree.h                                                           */
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

#include "scene/gui/tree.h"

#include "tests/test_macros.h"

namespace TestTree {

TEST_CASE("[SceneTree][Tree]") {
	SUBCASE("[Tree] Create and remove items.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item();
		CHECK_EQ(root->get_child_count(), 1);

		TreeItem *child2 = tree->create_item(root);
		CHECK_EQ(root->get_child_count(), 2);

		TreeItem *child3 = tree->create_item(root, 0);
		CHECK_EQ(root->get_child_count(), 3);

		CHECK_EQ(root->get_child(0), child3);
		CHECK_EQ(root->get_child(1), child1);
		CHECK_EQ(root->get_child(2), child2);

		root->remove_child(child3);
		CHECK_EQ(root->get_child_count(), 2);

		root->add_child(child3);
		CHECK_EQ(root->get_child_count(), 3);

		TreeItem *child4 = root->create_child();
		CHECK_EQ(root->get_child_count(), 4);

		CHECK_EQ(root->get_child(0), child1);
		CHECK_EQ(root->get_child(1), child2);
		CHECK_EQ(root->get_child(2), child3);
		CHECK_EQ(root->get_child(3), child4);

		memdelete(tree);
	}

	SUBCASE("[Tree] Clear items.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		for (int i = 0; i < 10; i++) {
			tree->create_item();
		}
		CHECK_EQ(root->get_child_count(), 10);

		root->clear_children();
		CHECK_EQ(root->get_child_count(), 0);

		memdelete(tree);
	}

	SUBCASE("[Tree] Get last item.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		TreeItem *last;
		for (int i = 0; i < 10; i++) {
			last = tree->create_item();
		}
		CHECK_EQ(root->get_child_count(), 10);
		CHECK_EQ(tree->get_last_item(), last);

		// Check nested.
		TreeItem *old_last = last;
		for (int i = 0; i < 10; i++) {
			last = tree->create_item(old_last);
		}
		CHECK_EQ(tree->get_last_item(), last);

		memdelete(tree);
	}

	// https://github.com/godotengine/godot/issues/96205
	SUBCASE("[Tree] Get last item after removal.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item(root);
		TreeItem *child2 = tree->create_item(root);

		CHECK_EQ(root->get_child_count(), 2);
		CHECK_EQ(tree->get_last_item(), child2);

		root->remove_child(child2);

		CHECK_EQ(root->get_child_count(), 1);
		CHECK_EQ(tree->get_last_item(), child1);

		root->add_child(child2);

		CHECK_EQ(root->get_child_count(), 2);
		CHECK_EQ(tree->get_last_item(), child2);

		memdelete(tree);
	}

	SUBCASE("[Tree] Previous and Next items.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item();
		TreeItem *child2 = tree->create_item();
		TreeItem *child3 = tree->create_item();
		CHECK_EQ(root->get_next(), nullptr);
		CHECK_EQ(root->get_next_visible(), child1);
		CHECK_EQ(root->get_next_in_tree(), child1);
		CHECK_EQ(child1->get_next(), child2);
		CHECK_EQ(child1->get_next_visible(), child2);
		CHECK_EQ(child1->get_next_in_tree(), child2);
		CHECK_EQ(child2->get_next(), child3);
		CHECK_EQ(child2->get_next_visible(), child3);
		CHECK_EQ(child2->get_next_in_tree(), child3);
		CHECK_EQ(child3->get_next(), nullptr);
		CHECK_EQ(child3->get_next_visible(), nullptr);
		CHECK_EQ(child3->get_next_in_tree(), nullptr);

		CHECK_EQ(root->get_prev(), nullptr);
		CHECK_EQ(root->get_prev_visible(), nullptr);
		CHECK_EQ(root->get_prev_in_tree(), nullptr);
		CHECK_EQ(child1->get_prev(), nullptr);
		CHECK_EQ(child1->get_prev_visible(), root);
		CHECK_EQ(child1->get_prev_in_tree(), root);
		CHECK_EQ(child2->get_prev(), child1);
		CHECK_EQ(child2->get_prev_visible(), child1);
		CHECK_EQ(child2->get_prev_in_tree(), child1);
		CHECK_EQ(child3->get_prev(), child2);
		CHECK_EQ(child3->get_prev_visible(), child2);
		CHECK_EQ(child3->get_prev_in_tree(), child2);

		TreeItem *nested1 = tree->create_item(child2);
		TreeItem *nested2 = tree->create_item(child2);
		TreeItem *nested3 = tree->create_item(child2);

		CHECK_EQ(child1->get_next(), child2);
		CHECK_EQ(child1->get_next_visible(), child2);
		CHECK_EQ(child1->get_next_in_tree(), child2);
		CHECK_EQ(child2->get_next(), child3);
		CHECK_EQ(child2->get_next_visible(), nested1);
		CHECK_EQ(child2->get_next_in_tree(), nested1);
		CHECK_EQ(child3->get_prev(), child2);
		CHECK_EQ(child3->get_prev_visible(), nested3);
		CHECK_EQ(child3->get_prev_in_tree(), nested3);
		CHECK_EQ(nested1->get_prev_in_tree(), child2);
		CHECK_EQ(nested1->get_next_in_tree(), nested2);
		CHECK_EQ(nested3->get_next_in_tree(), child3);

		memdelete(tree);
	}

	SUBCASE("[Tree] Previous and Next items with hide root.") {
		Tree *tree = memnew(Tree);
		tree->set_hide_root(true);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item();
		TreeItem *child2 = tree->create_item();
		TreeItem *child3 = tree->create_item();
		CHECK_EQ(root->get_next(), nullptr);
		CHECK_EQ(root->get_next_visible(), child1);
		CHECK_EQ(root->get_next_in_tree(), child1);
		CHECK_EQ(child1->get_next(), child2);
		CHECK_EQ(child1->get_next_visible(), child2);
		CHECK_EQ(child1->get_next_in_tree(), child2);
		CHECK_EQ(child2->get_next(), child3);
		CHECK_EQ(child2->get_next_visible(), child3);
		CHECK_EQ(child2->get_next_in_tree(), child3);
		CHECK_EQ(child3->get_next(), nullptr);
		CHECK_EQ(child3->get_next_visible(), nullptr);
		CHECK_EQ(child3->get_next_in_tree(), nullptr);

		CHECK_EQ(root->get_prev(), nullptr);
		CHECK_EQ(root->get_prev_visible(), nullptr);
		CHECK_EQ(root->get_prev_in_tree(), nullptr);
		CHECK_EQ(child1->get_prev(), nullptr);
		CHECK_EQ(child1->get_prev_visible(), nullptr);
		CHECK_EQ(child1->get_prev_in_tree(), nullptr);
		CHECK_EQ(child2->get_prev(), child1);
		CHECK_EQ(child2->get_prev_visible(), child1);
		CHECK_EQ(child2->get_prev_in_tree(), child1);
		CHECK_EQ(child3->get_prev(), child2);
		CHECK_EQ(child3->get_prev_visible(), child2);
		CHECK_EQ(child3->get_prev_in_tree(), child2);

		memdelete(tree);
	}

	SUBCASE("[Tree] Previous and Next items wrapping.") {
		Tree *tree = memnew(Tree);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item();
		TreeItem *child2 = tree->create_item();
		TreeItem *child3 = tree->create_item();
		CHECK_EQ(root->get_next_visible(true), child1);
		CHECK_EQ(root->get_next_in_tree(true), child1);
		CHECK_EQ(child1->get_next_visible(true), child2);
		CHECK_EQ(child1->get_next_in_tree(true), child2);
		CHECK_EQ(child2->get_next_visible(true), child3);
		CHECK_EQ(child2->get_next_in_tree(true), child3);
		CHECK_EQ(child3->get_next_visible(true), root);
		CHECK_EQ(child3->get_next_in_tree(true), root);

		CHECK_EQ(root->get_prev_visible(true), child3);
		CHECK_EQ(root->get_prev_in_tree(true), child3);
		CHECK_EQ(child1->get_prev_visible(true), root);
		CHECK_EQ(child1->get_prev_in_tree(true), root);
		CHECK_EQ(child2->get_prev_visible(true), child1);
		CHECK_EQ(child2->get_prev_in_tree(true), child1);
		CHECK_EQ(child3->get_prev_visible(true), child2);
		CHECK_EQ(child3->get_prev_in_tree(true), child2);

		TreeItem *nested1 = tree->create_item(child2);
		TreeItem *nested2 = tree->create_item(child2);
		TreeItem *nested3 = tree->create_item(child2);

		CHECK_EQ(child1->get_next_visible(true), child2);
		CHECK_EQ(child1->get_next_in_tree(true), child2);
		CHECK_EQ(child2->get_next_visible(true), nested1);
		CHECK_EQ(child2->get_next_in_tree(true), nested1);
		CHECK_EQ(nested3->get_next_visible(true), child3);
		CHECK_EQ(nested3->get_next_in_tree(true), child3);
		CHECK_EQ(child3->get_prev_visible(true), nested3);
		CHECK_EQ(child3->get_prev_in_tree(true), nested3);
		CHECK_EQ(nested1->get_prev_in_tree(true), child2);
		CHECK_EQ(nested1->get_next_in_tree(true), nested2);
		CHECK_EQ(nested3->get_next_in_tree(true), child3);

		memdelete(tree);
	}

	SUBCASE("[Tree] Previous and Next items wrapping with hide root.") {
		Tree *tree = memnew(Tree);
		tree->set_hide_root(true);
		TreeItem *root = tree->create_item();

		TreeItem *child1 = tree->create_item();
		TreeItem *child2 = tree->create_item();
		TreeItem *child3 = tree->create_item();
		CHECK_EQ(root->get_next_visible(true), child1);
		CHECK_EQ(root->get_next_in_tree(true), child1);
		CHECK_EQ(child1->get_next_visible(true), child2);
		CHECK_EQ(child1->get_next_in_tree(true), child2);
		CHECK_EQ(child2->get_next_visible(true), child3);
		CHECK_EQ(child2->get_next_in_tree(true), child3);
		CHECK_EQ(child3->get_next_visible(true), root);
		CHECK_EQ(child3->get_next_in_tree(true), root);

		CHECK_EQ(root->get_prev_visible(true), child3);
		CHECK_EQ(root->get_prev_in_tree(true), child3);
		CHECK_EQ(child1->get_prev_visible(true), child3);
		CHECK_EQ(child1->get_prev_in_tree(true), child3);
		CHECK_EQ(child2->get_prev_visible(true), child1);
		CHECK_EQ(child2->get_prev_in_tree(true), child1);
		CHECK_EQ(child3->get_prev_visible(true), child2);
		CHECK_EQ(child3->get_prev_in_tree(true), child2);

		memdelete(tree);
	}
}

} // namespace TestTree
