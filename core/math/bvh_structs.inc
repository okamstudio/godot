public:
struct ItemRef {
	uint32_t tnode_id; // -1 is invalid
	uint32_t item_id; // in the leaf

	bool is_active() const { return tnode_id != BVHCommon::INACTIVE; }
	void set_inactive() {
		tnode_id = BVHCommon::INACTIVE;
		item_id = BVHCommon::INACTIVE;
	}
};

// extra info kept in separate parallel list to the references,
// as this is less used as keeps cache better
struct ItemExtra {
	// Before doing user defined pairing checks (especially in the find_leavers function),
	// we may want to check that two items have compatible tree ids and tree masks,
	// as if they are incompatible they should not pair / collide.
	bool are_item_trees_compatible(const ItemExtra &p_other) const {
		uint32_t other_type = 1 << p_other.tree_id;
		if (tree_collision_mask & other_type) {
			return true;
		}
		uint32_t our_type = 1 << tree_id;
		if (p_other.tree_collision_mask & our_type) {
			return true;
		}
		return false;
	}

	// There can be multiple user defined trees
	uint32_t tree_id;

	// Defines which trees this item should collision check against.
	// 1 << tree_id, and normally items would collide against there own
	// tree (but not always).
	uint32_t tree_collision_mask;

	uint32_t last_updated_tick;
	int32_t subindex;

	T *userdata;

	// the active reference is a separate list of which references
	// are active so that we can slowly iterate through it over many frames for
	// slow optimize.
	uint32_t active_ref_id;
};

// tree leaf
struct TLeaf {
	uint16_t num_items;

private:
	uint16_t dirty;
	// separate data orientated lists for faster SIMD traversal
	uint32_t item_ref_ids[MAX_ITEMS];
	BVHABB_CLASS aabbs[MAX_ITEMS];

public:
	// accessors
	BVHABB_CLASS &get_aabb(uint32_t p_id) {
		BVH_ASSERT(p_id < MAX_ITEMS);
		return aabbs[p_id];
	}
	const BVHABB_CLASS &get_aabb(uint32_t p_id) const {
		BVH_ASSERT(p_id < MAX_ITEMS);
		return aabbs[p_id];
	}

	uint32_t &get_item_ref_id(uint32_t p_id) {
		BVH_ASSERT(p_id < MAX_ITEMS);
		return item_ref_ids[p_id];
	}
	const uint32_t &get_item_ref_id(uint32_t p_id) const {
		BVH_ASSERT(p_id < MAX_ITEMS);
		return item_ref_ids[p_id];
	}

	bool is_dirty() const { return dirty; }
	void set_dirty(bool p) { dirty = p; }

	void clear() {
		num_items = 0;
		set_dirty(false);
	}
	bool is_full() const { return num_items >= MAX_ITEMS; }

	void remove_item_unordered(uint32_t p_id) {
		BVH_ASSERT(p_id < num_items);
		num_items--;
		aabbs[p_id] = aabbs[num_items];
		item_ref_ids[p_id] = item_ref_ids[num_items];
	}

	uint32_t request_item() {
		if (num_items < MAX_ITEMS) {
			uint32_t id = num_items;
			num_items++;
			return id;
		}
#ifdef DEV_ENABLED
		return -1;
#else
		ERR_FAIL_V_MSG(0, "BVH request_item error.");
#endif
	}
};

// tree node
struct TNode {
	BVHABB_CLASS aabb;
	// either number of children if positive
	// or leaf id if negative (leaf id 0 is disallowed)
	union {
		int32_t num_children;
		int32_t neg_leaf_id;
	};
	uint32_t parent_id; // or -1
	uint16_t children[MAX_CHILDREN];

	// height in the tree, where leaves are 0, and all above are 1+
	// (or the highest where there is a tie off)
	int32_t height;

	bool is_leaf() const { return num_children < 0; }
	void set_leaf_id(int id) { neg_leaf_id = -id; }
	int get_leaf_id() const { return -neg_leaf_id; }

	void clear() {
		num_children = 0;
		parent_id = BVHCommon::INVALID;
		height = 0; // or -1 for testing

		// for safety set to improbable value
		aabb.set_to_max_opposite_extents();

		// other members are not blanked for speed .. they may be uninitialized
	}

	bool is_full_of_children() const { return num_children >= MAX_CHILDREN; }

	void remove_child_internal(uint32_t child_num) {
		children[child_num] = children[num_children - 1];
		num_children--;
	}

	int find_child(uint32_t p_child_node_id) {
		BVH_ASSERT(!is_leaf());

		for (int n = 0; n < num_children; n++) {
			if (children[n] == p_child_node_id) {
				return n;
			}
		}

		// not found
		return -1;
	}
};

// instead of using linked list we maintain
// item references (for quick lookup)
PooledList<ItemRef, uint32_t, true> _refs;
PooledList<ItemExtra, uint32_t, true> _extra;
PooledList<ItemPairs> _pairs;

// these 2 are not in sync .. nodes != leaves!
PooledList<TNode, uint32_t, true> _nodes;
PooledList<TLeaf, uint32_t, true> _leaves;

// we can maintain an un-ordered list of which references are active,
// in order to do a slow incremental optimize of the tree over each frame.
// This will work best if dynamic objects and static objects are in a different tree.
LocalVector<uint32_t, uint32_t, true> _active_refs;
uint32_t _current_active_ref = 0;

// instead of translating directly to the userdata output,
// we keep an intermediate list of hits as reference IDs, which can be used
// for pairing collision detection
LocalVector<uint32_t, uint32_t, true> _cull_hits;

// We can now have a user definable number of trees.
// This allows using e.g. a non-pairable and pairable tree,
// which can be more efficient for example, if we only need check non pairable against the pairable tree.
// It also may be more efficient in terms of separating static from dynamic objects, by reducing housekeeping.
// However this is a trade off, as there is a cost of traversing two trees.
uint32_t _root_node_id[NUM_TREES];

// these values may need tweaking according to the project
// the bound of the world, and the average velocities of the objects

// node expansion is important in the rendering tree
// larger values give less re-insertion as items move...
// but on the other hand over estimates the bounding box of nodes.
// we can either use auto mode, where the expansion is based on the root node size, or specify manually
real_t _node_expansion = 0.5;
bool _auto_node_expansion = true;

// pairing expansion important for physics pairing
// larger values gives more 'sticky' pairing, and is less likely to exhibit tunneling
// we can either use auto mode, where the expansion is based on the root node size, or specify manually
real_t _pairing_expansion = 0.1;

#ifdef BVH_ALLOW_AUTO_EXPANSION
bool _auto_pairing_expansion = true;
#endif

// when using an expanded bound, we must detect the condition where a new AABB
// is significantly smaller than the expanded bound, as this is a special case where we
// should override the optimization and create a new expanded bound.
// This threshold is derived from the _pairing_expansion, and should be recalculated
// if _pairing_expansion is changed.
real_t _aabb_shrinkage_threshold = 0.0;
