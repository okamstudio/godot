/**************************************************************************/
/*  bvh_tree.h                                                            */
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

// BVH Tree
// This is an implementation of a dynamic BVH with templated leaf size.
// This differs from most dynamic BVH in that it can handle more than 1 object
// in leaf nodes. This can make it far more efficient in certain circumstances.
// It also means that the splitting logic etc have to be completely different
// to a simpler tree.
// Note that MAX_CHILDREN should be fixed at 2 for now.

#include "core/math/aabb.h"
#include "core/math/bvh_abb.h"
#include "core/math/geometry_3d.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "core/templates/pooled_list.h"
#include <limits.h>

#define BVHABB_CLASS BVH_ABB<BOUNDS, POINT>

// not sure if this is better yet so making optional
#define BVH_EXPAND_LEAF_AABBS

// never do these checks in release
#ifdef DEV_ENABLED
//#define BVH_VERBOSE
//#define BVH_VERBOSE_TREE
//#define BVH_VERBOSE_PAIRING
//#define BVH_VERBOSE_MOVES

//#define BVH_VERBOSE_FRAME
//#define BVH_CHECKS
//#define BVH_INTEGRITY_CHECKS
#endif

// debug only assert
#ifdef BVH_CHECKS
#define BVH_ASSERT(a) CRASH_COND((a) == false)
#else
#define BVH_ASSERT(a)
#endif

#ifdef BVH_VERBOSE
#define VERBOSE_PRINT print_line
#else
#define VERBOSE_PRINT(a)
#endif

// really just a namespace
struct BVHCommon {
	// these could possibly also be the same constant,
	// although this may be useful for debugging.
	// or use zero for invalid and +1 based indices.
	static const uint32_t INVALID = (0xffffffff);
	static const uint32_t INACTIVE = (0xfffffffe);
};

// really a handle, can be anything
// note that zero is a valid reference for the BVH .. this may involve using
// a plus one based ID for clients that expect 0 to be invalid.
struct BVHHandle {
	// conversion operator
	operator uint32_t() const { return _data; }
	void set(uint32_t p_value) { _data = p_value; }

	uint32_t _data;

	void set_invalid() { _data = BVHCommon::INVALID; }
	bool is_invalid() const { return _data == BVHCommon::INVALID; }
	uint32_t id() const { return _data; }
	void set_id(uint32_t p_id) { _data = p_id; }

	bool operator==(const BVHHandle &p_h) const { return _data == p_h._data; }
	bool operator!=(const BVHHandle &p_h) const { return (*this == p_h) == false; }
};

// helper class to make iterative versions of recursive functions
template <typename T>
class BVH_IterativeInfo {
public:
	constexpr static const size_t ALLOCA_STACK_SIZE = 128;

	int32_t depth = 1;
	int32_t threshold = ALLOCA_STACK_SIZE - 2;
	T *stack;
	//only used in rare occasions when you run out of alloca memory
	// because tree is too unbalanced.
	LocalVector<T> aux_stack;
	int32_t get_alloca_stacksize() const { return ALLOCA_STACK_SIZE * sizeof(T); }

	T *get_first() const {
		return &stack[0];
	}

	// pop the last member of the stack, or return false
	bool pop(T &r_value) {
		if (!depth) {
			return false;
		}

		depth--;
		r_value = stack[depth];
		return true;
	}

	// request new addition to stack
	T *request() {
		if (depth > threshold) {
			if (aux_stack.is_empty()) {
				aux_stack.resize(ALLOCA_STACK_SIZE * 2);
				memcpy(aux_stack.ptr(), stack, get_alloca_stacksize());
			} else {
				aux_stack.resize(aux_stack.size() * 2);
			}
			stack = aux_stack.ptr();
			threshold = aux_stack.size() - 2;
		}
		return &stack[depth++];
	}
};

template <typename T>
class BVH_DummyPairTestFunction {
public:
	static bool user_collision_check(T *p_a, T *p_b) {
		// return false if no collision, decided by masks etc
		return true;
	}
};

template <typename T>
class BVH_DummyCullTestFunction {
public:
	static bool user_cull_check(T *p_a, T *p_b) {
		// return false if no collision
		return true;
	}
};

template <typename T, int NUM_TREES, int MAX_CHILDREN, int MAX_ITEMS, typename USER_PAIR_TEST_FUNCTION = BVH_DummyPairTestFunction<T>, typename USER_CULL_TEST_FUNCTION = BVH_DummyCullTestFunction<T>, bool USE_PAIRS = false, typename BOUNDS = AABB, typename POINT = Vector3>
class BVH_Tree {
	friend class BVH;

#include "bvh_pair.inc"
#include "bvh_structs.inc"

public:
	BVH_Tree() {
		for (int n = 0; n < NUM_TREES; n++) {
			_root_node_id[n] = BVHCommon::INVALID;
		}

		// disallow zero leaf ids
		// (as these ids are stored as negative numbers in the node)
		uint32_t dummy_leaf_id;
		_leaves.request(dummy_leaf_id);

		// In many cases you may want to change this default in the client code,
		// or expose this value to the user.
		// This default may make sense for a typically scaled 3d game, but maybe not for 2d on a pixel scale.
		params_set_pairing_expansion(0.1);
	}

private:
	bool node_add_child(uint32_t p_node_id, uint32_t p_child_node_id) {
		TNode &tnode = _nodes[p_node_id];
		if (tnode.is_full_of_children()) {
			return false;
		}

		tnode.children[tnode.num_children] = p_child_node_id;
		tnode.num_children += 1;

		// back link in the child to the parent
		TNode &tnode_child = _nodes[p_child_node_id];
		tnode_child.parent_id = p_node_id;

		return true;
	}

	void node_replace_child(uint32_t p_parent_id, uint32_t p_old_child_id, uint32_t p_new_child_id) {
		TNode &parent = _nodes[p_parent_id];
		BVH_ASSERT(!parent.is_leaf());

		int child_num = parent.find_child(p_old_child_id);
		BVH_ASSERT(child_num != -1);
		parent.children[child_num] = p_new_child_id;

		TNode &new_child = _nodes[p_new_child_id];
		new_child.parent_id = p_parent_id;
	}

	void node_remove_child(uint32_t p_parent_id, uint32_t p_child_id, uint32_t p_tree_id, bool p_prevent_sibling = false) {
		TNode &parent = _nodes[p_parent_id];
		BVH_ASSERT(!parent.is_leaf());

		int child_num = parent.find_child(p_child_id);
		BVH_ASSERT(child_num != -1);

		parent.remove_child_internal(child_num);

		// no need to keep back references for children at the moment

		uint32_t sibling_id = 0; // always a node id, as tnode is never a leaf
		bool sibling_present = false;

		// if there are more children, or this is the root node, don't try and delete
		if (parent.num_children > 1) {
			return;
		}

		// if there is 1 sibling, it can be moved to be a child of the
		if (parent.num_children == 1) {
			// else there is now a redundant node with one child, which can be removed
			sibling_id = parent.children[0];
			sibling_present = true;
		}

		// now there may be no children in this node .. in which case it can be deleted
		// remove node if empty
		// remove link from parent
		uint32_t grandparent_id = parent.parent_id;

		// special case for root node
		if (grandparent_id == BVHCommon::INVALID) {
			if (sibling_present) {
				// change the root node
				change_root_node(sibling_id, p_tree_id);

				// delete the old root node as no longer needed
				node_free_node_and_leaf(p_parent_id);
			}

			return;
		}

		if (sibling_present) {
			node_replace_child(grandparent_id, p_parent_id, sibling_id);
		} else {
			node_remove_child(grandparent_id, p_parent_id, p_tree_id, true);
		}

		// put the node on the free list to recycle
		node_free_node_and_leaf(p_parent_id);
	}

	// A node can either be a node, or a node AND a leaf combo.
	// Both must be deleted to prevent a leak.
	void node_free_node_and_leaf(uint32_t p_node_id) {
		TNode &node = _nodes[p_node_id];
		if (node.is_leaf()) {
			int leaf_id = node.get_leaf_id();
			_leaves.free(leaf_id);
		}

		_nodes.free(p_node_id);
	}

	void change_root_node(uint32_t p_new_root_id, uint32_t p_tree_id) {
		_root_node_id[p_tree_id] = p_new_root_id;
		TNode &root = _nodes[p_new_root_id];

		// mark no parent
		root.parent_id = BVHCommon::INVALID;
	}

	void node_make_leaf(uint32_t p_node_id) {
		uint32_t child_leaf_id;
		TLeaf *child_leaf = _leaves.request(child_leaf_id);
		child_leaf->clear();

		// zero is reserved at startup, to prevent this id being used
		// (as they are stored as negative values in the node, and zero is already taken)
		BVH_ASSERT(child_leaf_id != 0);

		TNode &node = _nodes[p_node_id];
		node.neg_leaf_id = -(int)child_leaf_id;
	}

	void node_remove_item(uint32_t p_ref_id, uint32_t p_tree_id, BVHABB_CLASS *r_old_aabb = nullptr) {
		// get the reference
		ItemRef &ref = _refs[p_ref_id];
		uint32_t owner_node_id = ref.tnode_id;

		// debug draw special
		// This may not be needed
		if (owner_node_id == BVHCommon::INVALID) {
			return;
		}

		TNode &tnode = _nodes[owner_node_id];
		CRASH_COND(!tnode.is_leaf());

		TLeaf &leaf = _node_get_leaf(tnode);

		// if the aabb is not determining the corner size, then there is no need to refit!
		// (optimization, as merging AABBs takes a lot of time)
		const BVHABB_CLASS &old_aabb = leaf.get_aabb(ref.item_id);

		// shrink a little to prevent using corner aabbs
		// in order to miss the corners first we shrink by node_expansion
		// (which is added to the overall bound of the leaf), then we also
		// shrink by an epsilon, in order to miss out the very corner aabbs
		// which are important in determining the bound. Any other aabb
		// within this can be removed and not affect the overall bound.
		BVHABB_CLASS node_bound = tnode.aabb;
		node_bound.expand(-_node_expansion - 0.001f);
		bool refit = true;

		if (node_bound.is_other_within(old_aabb)) {
			refit = false;
		}

		// record the old aabb if required (for incremental remove_and_reinsert)
		if (r_old_aabb) {
			*r_old_aabb = old_aabb;
		}

		leaf.remove_item_unordered(ref.item_id);

		if (leaf.num_items) {
			// the swapped item has to have its reference changed to, to point to the new item id
			uint32_t swapped_ref_id = leaf.get_item_ref_id(ref.item_id);

			ItemRef &swapped_ref = _refs[swapped_ref_id];

			swapped_ref.item_id = ref.item_id;

			// only have to refit if it is an edge item
			// This is a VERY EXPENSIVE STEP
			// we defer the refit updates until the update function is called once per frame
			if (refit) {
				leaf.set_dirty(true);
			}
		} else {
			// remove node if empty
			// remove link from parent
			if (tnode.parent_id != BVHCommon::INVALID) {
				// DANGER .. this can potentially end up with root node with 1 child ...
				// we don't want this and must check for it

				uint32_t parent_id = tnode.parent_id;

				node_remove_child(parent_id, owner_node_id, p_tree_id);
				refit_upward(parent_id);

				// put the node on the free list to recycle
				node_free_node_and_leaf(owner_node_id);
			}

			// else if no parent, it is the root node. Do not delete
		}

		ref.tnode_id = BVHCommon::INVALID;
		ref.item_id = BVHCommon::INVALID; // unset
	}

	// returns true if needs refit of PARENT tree only, the node itself AABB is calculated
	// within this routine
	bool _node_add_item(uint32_t p_node_id, uint32_t p_ref_id, const BVHABB_CLASS &p_aabb) {
		ItemRef &ref = _refs[p_ref_id];
		ref.tnode_id = p_node_id;

		TNode &node = _nodes[p_node_id];
		BVH_ASSERT(node.is_leaf());
		TLeaf &leaf = _node_get_leaf(node);

		// optimization - we only need to do a refit
		// if the added item is changing the AABB of the node.
		// in most cases it won't.
		bool needs_refit = true;

		// expand bound now
		BVHABB_CLASS expanded = p_aabb;
		expanded.expand(_node_expansion);

		// the bound will only be valid if there is an item in there already
		if (leaf.num_items) {
			if (node.aabb.is_other_within(expanded)) {
				// no change to node AABBs
				needs_refit = false;
			} else {
				node.aabb.merge(expanded);
			}
		} else {
			// bound of the node = the new aabb
			node.aabb = expanded;
		}

		ref.item_id = leaf.request_item();
		BVH_ASSERT(ref.item_id != BVHCommon::INVALID);

		// set the aabb of the new item
		leaf.get_aabb(ref.item_id) = p_aabb;

		// back reference on the item back to the item reference
		leaf.get_item_ref_id(ref.item_id) = p_ref_id;

		return needs_refit;
	}

	uint32_t _node_create_another_child(uint32_t p_node_id, const BVHABB_CLASS &p_aabb) {
		uint32_t child_node_id;
		TNode *child_node = _nodes.request(child_node_id);
		child_node->clear();

		// may not be necessary
		child_node->aabb = p_aabb;

		node_add_child(p_node_id, child_node_id);

		return child_node_id;
	}

#include "bvh_cull.inc"
#include "bvh_debug.inc"
#include "bvh_integrity.inc"
#include "bvh_logic.inc"
#include "bvh_misc.inc"
#include "bvh_public.inc"
#include "bvh_refit.inc"
#include "bvh_split.inc"
};

#undef VERBOSE_PRINT
