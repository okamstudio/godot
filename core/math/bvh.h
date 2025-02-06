/**************************************************************************/
/*  bvh.h                                                                 */
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

// BVH
// This class provides a wrapper around BVH tree, which contains most of the functionality
// for a dynamic BVH with templated leaf size.
// However BVH also adds facilities for pairing, to maintain compatibility with Godot 3.2.
// Pairing is a collision pairing system, on top of the basic BVH.

// Some notes on the use of BVH / Octree from Godot 3.2.
// This is not well explained elsewhere.
// The rendering tree mask and types that are sent to the BVH are NOT layer masks.
// They are INSTANCE_TYPES (defined in visual_server.h), e.g. MESH, MULTIMESH, PARTICLES etc.
// Thus the lights do no cull by layer mask in the BVH.

// Layer masks are implemented in the renderers as a later step, and light_cull_mask appears to be
// implemented in GLES3 but not GLES2. Layer masks are not yet implemented for directional lights.

// In the physics, the pairable_type is based on 1 << p_object->get_type() where:
// TYPE_AREA,
// TYPE_BODY
// and pairable_mask is either 0 if static, or set to all if non static

#include "bvh_tree.h"
#include "core/os/mutex.h"

#define BVHTREE_CLASS BVH_Tree<T, NUM_TREES, 2, MAX_ITEMS, USER_PAIR_TEST_FUNCTION, USER_CULL_TEST_FUNCTION, USE_PAIRS, BOUNDS, POINT>
#define BVH_LOCKED_FUNCTION BVHLockedFunction _lock_guard(&_mutex, BVH_THREAD_SAFE &&_thread_safe);

template <typename T, int NUM_TREES = 1, bool USE_PAIRS = false, int MAX_ITEMS = 32, typename USER_PAIR_TEST_FUNCTION = BVH_DummyPairTestFunction<T>, typename USER_CULL_TEST_FUNCTION = BVH_DummyCullTestFunction<T>, typename BOUNDS = AABB, typename POINT = Vector3, bool BVH_THREAD_SAFE = true>
class BVH_Manager {
public:
	// note we are using uint32_t instead of BVHHandle, losing type safety, but this
	// is for compatibility with octree
	typedef void *(*PairCallback)(void *, uint32_t, T *, int, uint32_t, T *, int);
	typedef void (*UnpairCallback)(void *, uint32_t, T *, int, uint32_t, T *, int, void *);
	typedef void *(*CheckPairCallback)(void *, uint32_t, T *, int, uint32_t, T *, int, void *);

	// allow locally toggling thread safety if the template has been compiled with BVH_THREAD_SAFE
	void params_set_thread_safe(bool p_enable) {
		_thread_safe = p_enable;
	}

	// these 2 are crucial for fine tuning, and can be applied manually
	// see the variable declarations for more info.
	void params_set_node_expansion(real_t p_value) {
		BVH_LOCKED_FUNCTION
		if (p_value >= 0.0) {
			tree._node_expansion = p_value;
			tree._auto_node_expansion = false;
		} else {
			tree._auto_node_expansion = true;
		}
	}

	void params_set_pairing_expansion(real_t p_value) {
		BVH_LOCKED_FUNCTION
		tree.params_set_pairing_expansion(p_value);
	}

	void set_pair_callback(PairCallback p_callback, void *p_userdata) {
		BVH_LOCKED_FUNCTION
		pair_callback = p_callback;
		pair_callback_userdata = p_userdata;
	}
	void set_unpair_callback(UnpairCallback p_callback, void *p_userdata) {
		BVH_LOCKED_FUNCTION
		unpair_callback = p_callback;
		unpair_callback_userdata = p_userdata;
	}
	void set_check_pair_callback(CheckPairCallback p_callback, void *p_userdata) {
		BVH_LOCKED_FUNCTION
		check_pair_callback = p_callback;
		check_pair_callback_userdata = p_userdata;
	}

	BVHHandle create(T *p_userdata, bool p_active = true, uint32_t p_tree_id = 0, uint32_t p_tree_collision_mask = 1, const BOUNDS &p_aabb = BOUNDS(), int p_subindex = 0) {
		BVH_LOCKED_FUNCTION

		// not sure if absolutely necessary to flush collisions here. It will cost performance to, instead
		// of waiting for update, so only uncomment this if there are bugs.
		if (USE_PAIRS) {
			//_check_for_collisions();
		}

		BVHHandle h = tree.item_add(p_userdata, p_active, p_aabb, p_subindex, p_tree_id, p_tree_collision_mask);

		if (USE_PAIRS) {
			// for safety initialize the expanded AABB
			BOUNDS &expanded_aabb = tree._pairs[h.id()].expanded_aabb;
			expanded_aabb = p_aabb;
			expanded_aabb.grow_by(tree._pairing_expansion);

			// force a collision check no matter the AABB
			if (p_active) {
				_add_changed_item(h, p_aabb, false);
				_check_for_collisions(true);
			}
		}

		return h;
	}

	////////////////////////////////////////////////////
	// wrapper versions that use uint32_t instead of handle
	// for backward compatibility. Less type safe
	void move(uint32_t p_handle, const BOUNDS &p_aabb) {
		BVHHandle h;
		h.set(p_handle);
		move(h, p_aabb);
	}

	void recheck_pairs(uint32_t p_handle) {
		BVHHandle h;
		h.set(p_handle);
		recheck_pairs(h);
	}

	void erase(uint32_t p_handle) {
		BVHHandle h;
		h.set(p_handle);
		erase(h);
	}

	void force_collision_check(uint32_t p_handle) {
		BVHHandle h;
		h.set(p_handle);
		force_collision_check(h);
	}

	bool activate(uint32_t p_handle, const BOUNDS &p_aabb, bool p_delay_collision_check = false) {
		BVHHandle h;
		h.set(p_handle);
		return activate(h, p_aabb, p_delay_collision_check);
	}

	bool deactivate(uint32_t p_handle) {
		BVHHandle h;
		h.set(p_handle);
		return deactivate(h);
	}

	void set_tree(uint32_t p_handle, uint32_t p_tree_id, uint32_t p_tree_collision_mask, bool p_force_collision_check = true) {
		BVHHandle h;
		h.set(p_handle);
		set_tree(h, p_tree_id, p_tree_collision_mask, p_force_collision_check);
	}

	uint32_t get_tree_id(uint32_t p_handle) const {
		BVHHandle h;
		h.set(p_handle);
		return item_get_tree_id(h);
	}
	int get_subindex(uint32_t p_handle) const {
		BVHHandle h;
		h.set(p_handle);
		return item_get_subindex(h);
	}

	T *get(uint32_t p_handle) const {
		BVHHandle h;
		h.set(p_handle);
		return item_get_userdata(h);
	}

	////////////////////////////////////////////////////

	void move(BVHHandle p_handle, const BOUNDS &p_aabb) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		if (tree.item_move(p_handle, p_aabb)) {
			if (USE_PAIRS) {
				_add_changed_item(p_handle, p_aabb);
			}
		}
	}

	void recheck_pairs(BVHHandle p_handle) {
		DEV_ASSERT(!p_handle.is_invalid());
		force_collision_check(p_handle);
	}

	void erase(BVHHandle p_handle) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		// call unpair and remove all references to the item
		// before deleting from the tree
		if (USE_PAIRS) {
			_remove_changed_item(p_handle);
		}

		tree.item_remove(p_handle);

		_check_for_collisions(true);
	}

	// use in conjunction with activate if you have deferred the collision check, and
	// set pairable has never been called.
	// (deferred collision checks are a workaround for visual server for historical reasons)
	void force_collision_check(BVHHandle p_handle) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		if (USE_PAIRS) {
			// the aabb should already be up to date in the BVH
			BOUNDS aabb;
			item_get_AABB(p_handle, aabb);

			// add it as changed even if aabb not different
			_add_changed_item(p_handle, aabb, false);

			// force an immediate full collision check, much like calls to set_pairable
			_check_for_collisions(true);
		}
	}

	// these should be read as set_visible for render trees,
	// but generically this makes items add or remove from the
	// tree internally, to speed things up by ignoring inactive items
	bool activate(BVHHandle p_handle, const BOUNDS &p_aabb, bool p_delay_collision_check = false) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		// sending the aabb here prevents the need for the BVH to maintain
		// a redundant copy of the aabb.
		// returns success
		if (tree.item_activate(p_handle, p_aabb)) {
			if (USE_PAIRS) {
				// in the special case of the render tree, when setting visibility we are using the combination of
				// activate then set_pairable. This would case 2 sets of collision checks. For efficiency here we allow
				// deferring to have a single collision check at the set_pairable call.
				// Watch for bugs! This may cause bugs if set_pairable is not called.
				if (!p_delay_collision_check) {
					_add_changed_item(p_handle, p_aabb, false);

					// force an immediate collision check, much like calls to set_pairable
					_check_for_collisions(true);
				}
			}
			return true;
		}

		return false;
	}

	bool deactivate(BVHHandle p_handle) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		// returns success
		if (tree.item_deactivate(p_handle)) {
			// call unpair and remove all references to the item
			// before deleting from the tree
			if (USE_PAIRS) {
				_remove_changed_item(p_handle);

				// force check for collisions, much like an erase was called
				_check_for_collisions(true);
			}
			return true;
		}

		return false;
	}

	bool get_active(BVHHandle p_handle) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		return tree.item_get_active(p_handle);
	}

	// call e.g. once per frame (this does a trickle optimize)
	void update() {
		BVH_LOCKED_FUNCTION
		tree.update();
		_check_for_collisions();
#ifdef BVH_INTEGRITY_CHECKS
		tree._integrity_check_all();
#endif
	}

	// this can be called more frequently than per frame if necessary
	void update_collisions() {
		BVH_LOCKED_FUNCTION
		_check_for_collisions();
	}

	// prefer calling this directly as type safe
	void set_tree(const BVHHandle &p_handle, uint32_t p_tree_id, uint32_t p_tree_collision_mask, bool p_force_collision_check = true) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVH_LOCKED_FUNCTION
		// Returns true if the pairing state has changed.
		bool state_changed = tree.item_set_tree(p_handle, p_tree_id, p_tree_collision_mask);

		if (USE_PAIRS) {
			// not sure if absolutely necessary to flush collisions here. It will cost performance to, instead
			// of waiting for update, so only uncomment this if there are bugs.
			//_check_for_collisions();

			if ((p_force_collision_check || state_changed) && tree.item_get_active(p_handle)) {
				// when the pairable state changes, we need to force a collision check because newly pairable
				// items may be in collision, and unpairable items might move out of collision.
				// We cannot depend on waiting for the next update, because that may come much later.
				BOUNDS aabb;
				item_get_AABB(p_handle, aabb);

				// passing false disables the optimization which prevents collision checks if
				// the aabb hasn't changed
				_add_changed_item(p_handle, aabb, false);

				// force an immediate collision check (probably just for this one item)
				// but it must be a FULL collision check, also checking pairable state and masks.
				// This is because AABB intersecting objects may have changed pairable state / mask
				// such that they should no longer be paired. E.g. lights.
				_check_for_collisions(true);
			} // only if active
		}
	}

	// cull tests
	int cull_aabb(const BOUNDS &p_aabb, T **p_result_array, int p_result_max, const T *p_tester, uint32_t p_tree_collision_mask = 0xFFFFFFFF, int *p_subindex_array = nullptr) {
		BVH_LOCKED_FUNCTION
		typename BVHTREE_CLASS::CullParams params;

		params.result_count_overall = 0;
		params.result_max = p_result_max;
		params.result_array = p_result_array;
		params.subindex_array = p_subindex_array;
		params.tree_collision_mask = p_tree_collision_mask;
		params.abb.from(p_aabb);
		params.tester = p_tester;

		tree.cull_aabb(params);

		return params.result_count_overall;
	}

	int cull_segment(const POINT &p_from, const POINT &p_to, T **p_result_array, int p_result_max, const T *p_tester, uint32_t p_tree_collision_mask = 0xFFFFFFFF, int *p_subindex_array = nullptr) {
		BVH_LOCKED_FUNCTION
		typename BVHTREE_CLASS::CullParams params;

		params.result_count_overall = 0;
		params.result_max = p_result_max;
		params.result_array = p_result_array;
		params.subindex_array = p_subindex_array;
		params.tester = p_tester;
		params.tree_collision_mask = p_tree_collision_mask;

		params.segment.from = p_from;
		params.segment.to = p_to;

		tree.cull_segment(params);

		return params.result_count_overall;
	}

	int cull_point(const POINT &p_point, T **p_result_array, int p_result_max, const T *p_tester, uint32_t p_tree_collision_mask = 0xFFFFFFFF, int *p_subindex_array = nullptr) {
		BVH_LOCKED_FUNCTION
		typename BVHTREE_CLASS::CullParams params;

		params.result_count_overall = 0;
		params.result_max = p_result_max;
		params.result_array = p_result_array;
		params.subindex_array = p_subindex_array;
		params.tester = p_tester;
		params.tree_collision_mask = p_tree_collision_mask;

		params.point = p_point;

		tree.cull_point(params);
		return params.result_count_overall;
	}

	int cull_convex(const Vector<Plane> &p_convex, T **p_result_array, int p_result_max, const T *p_tester, uint32_t p_tree_collision_mask = 0xFFFFFFFF) {
		BVH_LOCKED_FUNCTION
		if (!p_convex.size()) {
			return 0;
		}

		Vector<Vector3> convex_points = Geometry3D::compute_convex_mesh_points(&p_convex[0], p_convex.size());
		if (convex_points.size() == 0) {
			return 0;
		}

		typename BVHTREE_CLASS::CullParams params;
		params.result_count_overall = 0;
		params.result_max = p_result_max;
		params.result_array = p_result_array;
		params.subindex_array = nullptr;
		params.tester = p_tester;
		params.tree_collision_mask = p_tree_collision_mask;

		params.hull.planes = &p_convex[0];
		params.hull.num_planes = p_convex.size();
		params.hull.points = &convex_points[0];
		params.hull.num_points = convex_points.size();

		tree.cull_convex(params);

		return params.result_count_overall;
	}

private:
	// do this after moving etc.
	void _check_for_collisions(bool p_full_check = false) {
		if (!changed_items.size()) {
			// noop
			return;
		}

		BOUNDS bb;

		typename BVHTREE_CLASS::CullParams params;

		params.result_count_overall = 0;
		params.result_max = INT_MAX;
		params.result_array = nullptr;
		params.subindex_array = nullptr;

		for (const BVHHandle &h : changed_items) {
			// use the expanded aabb for pairing
			const BOUNDS &expanded_aabb = tree._pairs[h.id()].expanded_aabb;
			BVHABB_CLASS abb;
			abb.from(expanded_aabb);

			tree.item_fill_cullparams(h, params);

			// find all the existing paired aabbs that are no longer
			// paired, and send callbacks
			_find_leavers(h, abb, p_full_check);

			uint32_t changed_item_ref_id = h.id();

			params.abb = abb;

			params.result_count_overall = 0; // might not be needed
			tree.cull_aabb(params, false);

			for (const uint32_t ref_id : tree._cull_hits) {
				// don't collide against ourself
				if (ref_id == changed_item_ref_id) {
					continue;
				}

				// checkmasks is already done in the cull routine.
				BVHHandle h_collidee;
				h_collidee.set_id(ref_id);

				// find NEW enterers, and send callbacks for them only
				_collide(h, h_collidee);
			}
		}
		_reset();
	}

public:
	void item_get_AABB(BVHHandle p_handle, BOUNDS &r_aabb) {
		DEV_ASSERT(!p_handle.is_invalid());
		BVHABB_CLASS abb;
		tree.item_get_ABB(p_handle, abb);
		abb.to(r_aabb);
	}

private:
	// supplemental funcs
	uint32_t item_get_tree_id(BVHHandle p_handle) const { return _get_extra(p_handle).tree_id; }
	T *item_get_userdata(BVHHandle p_handle) const { return _get_extra(p_handle).userdata; }
	int item_get_subindex(BVHHandle p_handle) const { return _get_extra(p_handle).subindex; }

	void _unpair(BVHHandle p_from, BVHHandle p_to) {
		tree._handle_sort(p_from, p_to);

		typename BVHTREE_CLASS::ItemExtra &exa = tree._extra[p_from.id()];
		typename BVHTREE_CLASS::ItemExtra &exb = tree._extra[p_to.id()];

		// if the userdata is the same, no collisions should occur
		if ((exa.userdata == exb.userdata) && exa.userdata) {
			return;
		}

		typename BVHTREE_CLASS::ItemPairs &pairs_from = tree._pairs[p_from.id()];
		typename BVHTREE_CLASS::ItemPairs &pairs_to = tree._pairs[p_to.id()];

		void *ud_from = pairs_from.remove_pair_to(p_to);
		pairs_to.remove_pair_to(p_from);

#ifdef BVH_VERBOSE_PAIRING
		print_line("_unpair " + itos(p_from.id()) + " from " + itos(p_to.id()));
#endif

		// callback
		if (unpair_callback) {
			unpair_callback(pair_callback_userdata, p_from, exa.userdata, exa.subindex, p_to, exb.userdata, exb.subindex, ud_from);
		}
	}

	void *_recheck_pair(BVHHandle p_from, BVHHandle p_to, void *p_pair_data) {
		tree._handle_sort(p_from, p_to);

		typename BVHTREE_CLASS::ItemExtra &exa = tree._extra[p_from.id()];
		typename BVHTREE_CLASS::ItemExtra &exb = tree._extra[p_to.id()];

		// if the userdata is the same, no collisions should occur
		if ((exa.userdata == exb.userdata) && exa.userdata) {
			return p_pair_data;
		}

		// callback
		if (check_pair_callback) {
			return check_pair_callback(check_pair_callback_userdata, p_from, exa.userdata, exa.subindex, p_to, exb.userdata, exb.subindex, p_pair_data);
		}

		return p_pair_data;
	}

	// returns true if unpair
	bool _find_leavers_process_pair(typename BVHTREE_CLASS::ItemPairs &p_pairs_from, const BVHABB_CLASS &p_abb_from, BVHHandle p_from, BVHHandle p_to, bool p_full_check) {
		BVHABB_CLASS abb_to;
		tree.item_get_ABB(p_to, abb_to);

		// do they overlap?
		if (p_abb_from.intersects(abb_to)) {
			// the full check for pairable / non pairable (i.e. tree_id and tree_masks) and mask changes is extra expense
			// this need not be done in most cases (for speed) except in the case where set_tree is called
			// where the masks etc of the objects in question may have changed
			if (!p_full_check) {
				return false;
			}
			const typename BVHTREE_CLASS::ItemExtra &exa = _get_extra(p_from);
			const typename BVHTREE_CLASS::ItemExtra &exb = _get_extra(p_to);

			// Checking tree_ids and tree_collision_masks
			if (exa.are_item_trees_compatible(exb)) {
				bool pair_allowed = USER_PAIR_TEST_FUNCTION::user_pair_check(exa.userdata, exb.userdata);

				// the masks must still be compatible to pair
				// i.e. if there is a hit between the two and they intersect, then they should stay paired
				if (pair_allowed) {
					return false;
				}
			}
		}

		_unpair(p_from, p_to);
		return true;
	}

	// find all the existing paired aabbs that are no longer
	// paired, and send callbacks
	void _find_leavers(BVHHandle p_handle, const BVHABB_CLASS &expanded_abb_from, bool p_full_check) {
		typename BVHTREE_CLASS::ItemPairs &p_from = tree._pairs[p_handle.id()];

		BVHABB_CLASS abb_from = expanded_abb_from;

		// remove from pairing list for every partner
		for (unsigned int n = 0; n < p_from.extended_pairs.size(); n++) {
			BVHHandle h_to = p_from.extended_pairs[n].handle;
			if (_find_leavers_process_pair(p_from, abb_from, p_handle, h_to, p_full_check)) {
				// we need to keep the counter n up to date if we deleted a pair
				// as the number of items in p_from.extended_pairs will have decreased by 1
				// and we don't want to miss an item
				n--;
			}
		}
	}

	// find NEW enterers, and send callbacks for them only
	// handle a and b
	void _collide(BVHHandle p_ha, BVHHandle p_hb) {
		// only have to do this oneway, lower ID then higher ID
		tree._handle_sort(p_ha, p_hb);

		const typename BVHTREE_CLASS::ItemExtra &exa = _get_extra(p_ha);
		const typename BVHTREE_CLASS::ItemExtra &exb = _get_extra(p_hb);

		// user collision callback
		if (!USER_PAIR_TEST_FUNCTION::user_pair_check(exa.userdata, exb.userdata)) {
			return;
		}

		// if the userdata is the same, no collisions should occur
		if ((exa.userdata == exb.userdata) && exa.userdata) {
			return;
		}

		typename BVHTREE_CLASS::ItemPairs &p_from = tree._pairs[p_ha.id()];
		typename BVHTREE_CLASS::ItemPairs &p_to = tree._pairs[p_hb.id()];

		// does this pair exist already?
		// or only check the one with lower number of pairs for greater speed
		if (p_from.num_pairs <= p_to.num_pairs) {
			if (p_from.contains_pair_to(p_hb)) {
				return;
			}
		} else {
			if (p_to.contains_pair_to(p_ha)) {
				return;
			}
		}

		// callback
		void *callback_userdata = nullptr;

#ifdef BVH_VERBOSE_PAIRING
		print_line("_pair " + itos(p_ha.id()) + " to " + itos(p_hb.id()));
#endif

		if (pair_callback) {
			callback_userdata = pair_callback(pair_callback_userdata, p_ha, exa.userdata, exa.subindex, p_hb, exb.userdata, exb.subindex);
		}

		// new pair! .. only really need to store the userdata on the lower handle, but both have storage so...
		p_from.add_pair_to(p_hb, callback_userdata);
		p_to.add_pair_to(p_ha, callback_userdata);
	}

	// if we remove an item, we need to immediately remove the pairs, to prevent reading the pair after deletion
	void _remove_pairs_containing(BVHHandle p_handle) {
		typename BVHTREE_CLASS::ItemPairs &p_from = tree._pairs[p_handle.id()];

		// remove from pairing list for every partner.
		// can't easily use a for loop here, because removing changes the size of the list
		while (p_from.extended_pairs.size()) {
			BVHHandle h_to = p_from.extended_pairs[0].handle;
			_unpair(p_handle, h_to);
		}
	}

	// Send pair callbacks again for all existing pairs for the given handle.
	void _recheck_pairs(BVHHandle p_handle) {
		typename BVHTREE_CLASS::ItemPairs &from = tree._pairs[p_handle.id()];

		// checking pair for every partner.
		for (unsigned int n = 0; n < from.extended_pairs.size(); n++) {
			typename BVHTREE_CLASS::ItemPairs::Link &pair = from.extended_pairs[n];
			BVHHandle h_to = pair.handle;
			void *new_pair_data = _recheck_pair(p_handle, h_to, pair.userdata);

			if (new_pair_data != pair.userdata) {
				pair.userdata = new_pair_data;

				// Update pair data for the second item.
				typename BVHTREE_CLASS::ItemPairs &to = tree._pairs[h_to.id()];
				for (unsigned int to_index = 0; to_index < to.extended_pairs.size(); to_index++) {
					typename BVHTREE_CLASS::ItemPairs::Link &to_pair = to.extended_pairs[to_index];
					if (to_pair.handle == p_handle) {
						to_pair.userdata = new_pair_data;
						break;
					}
				}
			}
		}
	}

private:
	const typename BVHTREE_CLASS::ItemExtra &_get_extra(BVHHandle p_handle) const {
		return tree._extra[p_handle.id()];
	}
	const typename BVHTREE_CLASS::ItemRef &_get_ref(BVHHandle p_handle) const {
		return tree._refs[p_handle.id()];
	}

	void _reset() {
		changed_items.clear();
		_tick++;
	}

	void _add_changed_item(BVHHandle p_handle, const BOUNDS &aabb, bool p_check_aabb = true) {
		// Note that non pairable items can pair with pairable,
		// so all types must be added to the list

#ifdef BVH_EXPAND_LEAF_AABBS
		// if using expanded AABB in the leaf, the redundancy check will already have been made
		BOUNDS &expanded_aabb = tree._pairs[p_handle.id()].expanded_aabb;
		item_get_AABB(p_handle, expanded_aabb);
#else
		// aabb check with expanded aabb. This greatly decreases processing
		// at the cost of slightly less accurate pairing checks
		// Note this pairing AABB is separate from the AABB in the actual tree
		BOUNDS &expanded_aabb = tree._pairs[p_handle.id()].expanded_aabb;

		// passing p_check_aabb false disables the optimization which prevents collision checks if
		// the aabb hasn't changed. This is needed where set_pairable has been called, but the position
		// has not changed.
		if (p_check_aabb && tree.expanded_aabb_encloses_not_shrink(expanded_aabb, aabb)) {
			return;
		}

		// ALWAYS update the new expanded aabb, even if already changed once
		// this tick, because it is vital that the AABB is kept up to date
		expanded_aabb = aabb;
		expanded_aabb.grow_by(tree._pairing_expansion);
#endif

		// this code is to ensure that changed items only appear once on the updated list
		// collision checking them multiple times is not needed, and repeats the same thing
		uint32_t &last_updated_tick = tree._extra[p_handle.id()].last_updated_tick;

		if (last_updated_tick == _tick) {
			return; // already on changed list
		}

		// mark as on list
		last_updated_tick = _tick;

		// add to the list
		changed_items.push_back(p_handle);
	}

	void _remove_changed_item(BVHHandle p_handle) {
		// Care has to be taken here for items that are deleted. The ref ID
		// could be reused on the same tick for new items. This is probably
		// rare but should be taken into consideration

		// callbacks
		_remove_pairs_containing(p_handle);

		// remove from changed items (not very efficient yet)
		for (int n = 0; n < (int)changed_items.size(); n++) {
			if (changed_items[n] == p_handle) {
				changed_items.remove_at_unordered(n);

				// because we are using an unordered remove,
				// the last changed item will now be at spot 'n',
				// and we need to redo it, so we prevent moving on to
				// the next n at the next for iteration.
				n--;
			}
		}

		// reset the last updated tick (may not be necessary but just in case)
		tree._extra[p_handle.id()].last_updated_tick = 0;
	}

	PairCallback pair_callback = nullptr;
	UnpairCallback unpair_callback = nullptr;
	CheckPairCallback check_pair_callback = nullptr;
	void *pair_callback_userdata = nullptr;
	void *unpair_callback_userdata = nullptr;
	void *check_pair_callback_userdata = nullptr;

	BVHTREE_CLASS tree;

	// for collision pairing,
	// maintain a list of all items moved etc on each frame / tick
	LocalVector<BVHHandle, uint32_t, true> changed_items;
	uint32_t _tick = 1; // Start from 1 so items with 0 indicate never updated.

	class BVHLockedFunction {
	public:
		BVHLockedFunction(Mutex *p_mutex, bool p_thread_safe) {
			// will be compiled out if not set in template
			if (p_thread_safe) {
				_mutex = p_mutex;
				_mutex->lock();

			} else {
				_mutex = nullptr;
			}
		}
		~BVHLockedFunction() {
			// will be compiled out if not set in template
			if (_mutex) {
				_mutex->unlock();
			}
		}

	private:
		Mutex *_mutex = nullptr;
	};

	Mutex _mutex;

	// local toggle for turning on and off thread safety in project settings
	bool _thread_safe = BVH_THREAD_SAFE;

public:
	BVH_Manager() {}
};

#undef BVHTREE_CLASS
