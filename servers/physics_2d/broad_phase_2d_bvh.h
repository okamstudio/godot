/**************************************************************************/
/*  broad_phase_2d_bvh.h                                                  */
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

#ifndef BROAD_PHASE_2D_BVH_H
#define BROAD_PHASE_2D_BVH_H

#include "broad_phase_2d_sw.h"
#include "core/math/bvh.h"
#include "core/math/rect2.h"
#include "core/math/vector2.h"

class BroadPhase2DBVH : public BroadPhase2DSW {
	template <class T>
	class UserPairTestFunction {
	public:
		static bool user_pair_check(const T *p_a, const T *p_b) {
			// return false if no collision, decided by masks etc
			return p_a->test_collision_mask(p_b);
		}
	};

	template <class T>
	class UserCullTestFunction {
	public:
		static bool user_cull_check(const T *p_a, const T *p_b) {
			return true;
		}
	};

	enum Tree {
		TREE_DYNAMIC = 0,
		TREE_AREA = 1,
		TREE_STATIC = 2,
	};

	enum TreeFlag {
		TREE_FLAG_DYNAMIC = 1 << TREE_DYNAMIC,
		TREE_FLAG_AREA = 1 << TREE_AREA,
		TREE_FLAG_STATIC = 1 << TREE_STATIC,
	};

	BVH_Manager<CollisionObject2DSW, 3, true, 128, UserPairTestFunction<CollisionObject2DSW>, UserCullTestFunction<CollisionObject2DSW>, Rect2, Vector2> bvh;

	static void *_pair_callback(void *p_self, uint32_t p_id_A, CollisionObject2DSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObject2DSW *p_object_B, int p_subindex_B);
	static void _unpair_callback(void *p_self, uint32_t p_id_A, CollisionObject2DSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObject2DSW *p_object_B, int p_subindex_B, void *p_pair_data);
	static void *_check_pair_callback(void *p_self, uint32_t p_id_A, CollisionObject2DSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObject2DSW *p_object_B, int p_subindex_B, void *p_pair_data);

	PairCallback pair_callback;
	void *pair_userdata;
	UnpairCallback unpair_callback;
	void *unpair_userdata;

	uint32_t _find_tree(bool p_static, int p_collision_object_type, uint32_t &r_tree_collision_mask) const;

public:
	// 0 is an invalid ID
	virtual ID create(CollisionObject2DSW *p_object, int p_subindex, const Rect2 &p_aabb, bool p_static, int p_collision_object_type);
	virtual void move(ID p_id, const Rect2 &p_aabb);
	virtual void recheck_pairs(ID p_id);
	virtual void set_static(ID p_id, bool p_static, int p_collision_object_type);
	virtual void remove(ID p_id);

	virtual CollisionObject2DSW *get_object(ID p_id) const;
	virtual bool is_static(ID p_id) const;
	virtual int get_subindex(ID p_id) const;

	virtual int cull_segment(const Vector2 &p_from, const Vector2 &p_to, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices = nullptr);
	virtual int cull_aabb(const Rect2 &p_aabb, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices = nullptr);

	virtual void set_pair_callback(PairCallback p_pair_callback, void *p_userdata);
	virtual void set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata);

	virtual void update();

	static BroadPhase2DSW *_create();
	BroadPhase2DBVH();
};

#endif // BROAD_PHASE_2D_BVH_H
