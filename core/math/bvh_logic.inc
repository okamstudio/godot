// for slow incremental optimization, we will periodically remove each
// item from the tree and reinsert, to give it a chance to find a better position
void _logic_item_remove_and_reinsert(uint32_t p_ref_id) {
	// get the reference
	ItemRef &ref = _refs[p_ref_id];

	// no need to optimize inactive items
	if (!ref.is_active()) {
		return;
	}

	// special case of debug draw
	if (ref.item_id == BVHCommon::INVALID) {
		return;
	}

	BVH_ASSERT(ref.tnode_id != BVHCommon::INVALID);

	// some overlay elaborate way to find out which tree the node is in!
	BVHHandle temp_handle;
	temp_handle.set_id(p_ref_id);
	uint32_t tree_id = _handle_get_tree_id(temp_handle);

	// remove and reinsert
	BVHABB_CLASS abb;
	node_remove_item(p_ref_id, tree_id, &abb);

	// we must choose where to add to tree
	ref.tnode_id = _logic_choose_item_add_node(_root_node_id[tree_id], abb);
	_node_add_item(ref.tnode_id, p_ref_id, abb);

	refit_upward_and_balance(ref.tnode_id, tree_id);
}

// from randy gaul balance function
BVHABB_CLASS _logic_abb_merge(const BVHABB_CLASS &a, const BVHABB_CLASS &b) {
	BVHABB_CLASS c = a;
	c.merge(b);
	return c;
}

//--------------------------------------------------------------------------------------------------
/**
 * @file    q3DynamicAABBTree.h
 * @author  Randy Gaul
 * @date    10/10/2014
 *  Copyright (c) 2014 Randy Gaul http://www.randygaul.net
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *    1. The origin of this software must not be misrepresented; you must not
 *       claim that you wrote the original software. If you use this software
 *       in a product, an acknowledgment in the product documentation would be
 *       appreciated but is not required.
 *    2. Altered source versions must be plainly marked as such, and must not
 *       be misrepresented as being the original software.
 *    3. This notice may not be removed or altered from any source distribution.
 */
//--------------------------------------------------------------------------------------------------

// This function is based on the 'Balance' function from Randy Gaul's qu3e
// https://github.com/RandyGaul/qu3e
// It is MODIFIED from qu3e version.
// This is the only function used (and _logic_abb_merge helper function).
int32_t _logic_balance(int32_t iA, uint32_t p_tree_id) {
	//return iA; // uncomment this to bypass balance

	TNode *A = &_nodes[iA];

	if (A->is_leaf() || A->height == 1) {
		return iA;
	}

	/*        A
	 *      /   \
	 *     B     C
	 *    / \   / \
	 *   D   E F   G
	 */

	CRASH_COND(A->num_children != 2);
	int32_t iB = A->children[0];
	int32_t iC = A->children[1];
	TNode *B = &_nodes[iB];
	TNode *C = &_nodes[iC];

	int32_t balance = C->height - B->height;

	// C is higher, promote C
	if (balance > 1) {
		int32_t iF = C->children[0];
		int32_t iG = C->children[1];
		TNode *F = &_nodes[iF];
		TNode *G = &_nodes[iG];

		// grandParent point to C
		if (A->parent_id != BVHCommon::INVALID) {
			if (_nodes[A->parent_id].children[0] == iA) {
				_nodes[A->parent_id].children[0] = iC;

			} else {
				_nodes[A->parent_id].children[1] = iC;
			}
		} else {
			// check this .. seems dodgy
			change_root_node(iC, p_tree_id);
		}

		// Swap A and C
		C->children[0] = iA;
		C->parent_id = A->parent_id;
		A->parent_id = iC;

		// Finish rotation
		if (F->height > G->height) {
			C->children[1] = iF;
			A->children[1] = iG;
			G->parent_id = iA;
			A->aabb = _logic_abb_merge(B->aabb, G->aabb);
			C->aabb = _logic_abb_merge(A->aabb, F->aabb);

			A->height = 1 + MAX(B->height, G->height);
			C->height = 1 + MAX(A->height, F->height);
		}

		else {
			C->children[1] = iG;
			A->children[1] = iF;
			F->parent_id = iA;
			A->aabb = _logic_abb_merge(B->aabb, F->aabb);
			C->aabb = _logic_abb_merge(A->aabb, G->aabb);

			A->height = 1 + MAX(B->height, F->height);
			C->height = 1 + MAX(A->height, G->height);
		}

		return iC;
	}

	// B is higher, promote B
	else if (balance < -1) {
		int32_t iD = B->children[0];
		int32_t iE = B->children[1];
		TNode *D = &_nodes[iD];
		TNode *E = &_nodes[iE];

		// grandParent point to B
		if (A->parent_id != BVHCommon::INVALID) {
			if (_nodes[A->parent_id].children[0] == iA) {
				_nodes[A->parent_id].children[0] = iB;
			} else {
				_nodes[A->parent_id].children[1] = iB;
			}
		}

		else {
			// check this .. seems dodgy
			change_root_node(iB, p_tree_id);
		}

		// Swap A and B
		B->children[1] = iA;
		B->parent_id = A->parent_id;
		A->parent_id = iB;

		// Finish rotation
		if (D->height > E->height) {
			B->children[0] = iD;
			A->children[0] = iE;
			E->parent_id = iA;
			A->aabb = _logic_abb_merge(C->aabb, E->aabb);
			B->aabb = _logic_abb_merge(A->aabb, D->aabb);

			A->height = 1 + MAX(C->height, E->height);
			B->height = 1 + MAX(A->height, D->height);
		}

		else {
			B->children[0] = iE;
			A->children[0] = iD;
			D->parent_id = iA;
			A->aabb = _logic_abb_merge(C->aabb, D->aabb);
			B->aabb = _logic_abb_merge(A->aabb, E->aabb);

			A->height = 1 + MAX(C->height, D->height);
			B->height = 1 + MAX(A->height, E->height);
		}

		return iB;
	}

	return iA;
}

// either choose an existing node to add item to, or create a new node and return this
uint32_t _logic_choose_item_add_node(uint32_t p_node_id, const BVHABB_CLASS &p_aabb) {
	while (true) {
		BVH_ASSERT(p_node_id != BVHCommon::INVALID);
		TNode &tnode = _nodes[p_node_id];

		if (tnode.is_leaf()) {
			// if a leaf, and non full, use this to add to
			if (!node_is_leaf_full(tnode)) {
				return p_node_id;
			}

			// else split the leaf, and use one of the children to add to
			return split_leaf(p_node_id, p_aabb);
		}

		// this should not happen???
		// is still happening, need to debug and find circumstances. Is not that serious
		// but would be nice to prevent. I think it only happens with the root node.
		if (tnode.num_children == 1) {
			WARN_PRINT_ONCE("BVH::recursive_choose_item_add_node, node with 1 child, recovering");
			p_node_id = tnode.children[0];
		} else {
			BVH_ASSERT(tnode.num_children == 2);
			TNode &childA = _nodes[tnode.children[0]];
			TNode &childB = _nodes[tnode.children[1]];
			int which = p_aabb.select_by_proximity(childA.aabb, childB.aabb);

			p_node_id = tnode.children[which];
		}
	}
}
