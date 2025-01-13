int _handle_get_tree_id(BVHHandle p_handle) const {
	if (USE_PAIRS) {
		return _extra[p_handle.id()].tree_id;
	}
	return 0;
}

public:
void _handle_sort(BVHHandle &p_ha, BVHHandle &p_hb) const {
	if (p_ha.id() > p_hb.id()) {
		BVHHandle temp = p_hb;
		p_hb = p_ha;
		p_ha = temp;
	}
}

private:
void create_root_node(int p_tree) {
	// if there is no root node, create one
	if (_root_node_id[p_tree] == BVHCommon::INVALID) {
		uint32_t root_node_id;
		TNode *node = _nodes.request(root_node_id);
		node->clear();
		_root_node_id[p_tree] = root_node_id;

		// make the root node a leaf
		uint32_t leaf_id;
		TLeaf *leaf = _leaves.request(leaf_id);
		leaf->clear();
		node->neg_leaf_id = -(int)leaf_id;
	}
}

bool node_is_leaf_full(TNode &tnode) const {
	const TLeaf &leaf = _node_get_leaf(tnode);
	return leaf.is_full();
}

public:
TLeaf &_node_get_leaf(TNode &tnode) {
	BVH_ASSERT(tnode.is_leaf());
	return _leaves[tnode.get_leaf_id()];
}

const TLeaf &_node_get_leaf(const TNode &tnode) const {
	BVH_ASSERT(tnode.is_leaf());
	return _leaves[tnode.get_leaf_id()];
}

private:
