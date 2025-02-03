/**************************************************************************/
/*  broad_phase_bvh.cpp                                                   */
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

#include "broad_phase_bvh.h"
#include "collision_object_sw.h"
#include "core/project_settings.h"

BroadPhaseSW::ID BroadPhaseBVH::create(CollisionObjectSW *p_object, int p_subindex, const AABB &p_aabb, bool p_static, int p_collision_object_type) {
	uint32_t tree_collision_mask = 0;
	uint32_t tree_id = _find_tree(p_static, p_collision_object_type, tree_collision_mask);
	ID oid = bvh.create(p_object, true, tree_id, tree_collision_mask, p_aabb, p_subindex); // Pair everything, don't care?
	return oid + 1;
}

uint32_t BroadPhaseBVH::_find_tree(bool p_static, int p_collision_object_type, uint32_t &r_tree_collision_mask) const {
	uint32_t tree_id = p_static ? TREE_STATIC : TREE_DYNAMIC;
	if ((p_collision_object_type == CollisionObjectSW::Type::TYPE_AREA) && (tree_id == TREE_STATIC)) {
		tree_id = TREE_AREA;
	}
	switch (tree_id) {
		default: {
			r_tree_collision_mask = TREE_FLAG_DYNAMIC | TREE_FLAG_AREA | TREE_FLAG_STATIC;
		} break;
		case TREE_AREA: {
			r_tree_collision_mask = TREE_FLAG_DYNAMIC | TREE_FLAG_STATIC;
		} break;
		case TREE_STATIC: {
			r_tree_collision_mask = TREE_FLAG_DYNAMIC | TREE_FLAG_AREA;
		} break;
	}

	return tree_id;
}

void BroadPhaseBVH::move(ID p_id, const AABB &p_aabb) {
	bvh.move(p_id - 1, p_aabb);
}

void BroadPhaseBVH::recheck_pairs(ID p_id) {
	bvh.recheck_pairs(p_id - 1);
}

void BroadPhaseBVH::set_static(ID p_id, bool p_static, int p_collision_object_type) {
	uint32_t tree_collision_mask = 0;
	uint32_t tree_id = _find_tree(p_static, p_collision_object_type, tree_collision_mask);
	bvh.set_tree(p_id - 1, tree_id, tree_collision_mask, false);
}

void BroadPhaseBVH::remove(ID p_id) {
	bvh.erase(p_id - 1);
}

CollisionObjectSW *BroadPhaseBVH::get_object(ID p_id) const {
	CollisionObjectSW *it = bvh.get(p_id - 1);
	ERR_FAIL_COND_V(!it, nullptr);
	return it;
}

bool BroadPhaseBVH::is_static(ID p_id) const {
	uint32_t tree_id = bvh.get_tree_id(p_id - 1);
	return tree_id != 0;
}

int BroadPhaseBVH::get_subindex(ID p_id) const {
	return bvh.get_subindex(p_id - 1);
}

int BroadPhaseBVH::cull_point(const Vector3 &p_point, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return bvh.cull_point(p_point, p_results, p_max_results, nullptr, 0xFFFFFFFF, p_result_indices);
}

int BroadPhaseBVH::cull_segment(const Vector3 &p_from, const Vector3 &p_to, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return bvh.cull_segment(p_from, p_to, p_results, p_max_results, nullptr, 0xFFFFFFFF, p_result_indices);
}

int BroadPhaseBVH::cull_aabb(const AABB &p_aabb, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return bvh.cull_aabb(p_aabb, p_results, p_max_results, nullptr, 0xFFFFFFFF, p_result_indices);
}

void *BroadPhaseBVH::_pair_callback(void *p_self, uint32_t p_id_A, CollisionObjectSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObjectSW *p_object_B, int p_subindex_B) {
	BroadPhaseBVH *bpo = (BroadPhaseBVH *)(p_self);
	if (!bpo->pair_callback) {
		return nullptr;
	}

	return bpo->pair_callback(p_object_A, p_subindex_A, p_object_B, p_subindex_B, nullptr, bpo->pair_userdata);
}

void BroadPhaseBVH::_unpair_callback(void *p_self, uint32_t p_id_A, CollisionObjectSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObjectSW *p_object_B, int p_subindex_B, void *p_pair_data) {
	BroadPhaseBVH *bpo = (BroadPhaseBVH *)(p_self);
	if (!bpo->unpair_callback) {
		return;
	}

	bpo->unpair_callback(p_object_A, p_subindex_A, p_object_B, p_subindex_B, p_pair_data, bpo->unpair_userdata);
}

void *BroadPhaseBVH::_check_pair_callback(void *p_self, uint32_t p_id_A, CollisionObjectSW *p_object_A, int p_subindex_A, uint32_t p_id_B, CollisionObjectSW *p_object_B, int p_subindex_B, void *p_pair_data) {
	BroadPhaseBVH *bpo = (BroadPhaseBVH *)(p_self);
	if (!bpo->pair_callback) {
		return nullptr;
	}

	return bpo->pair_callback(p_object_A, p_subindex_A, p_object_B, p_subindex_B, p_pair_data, bpo->pair_userdata);
}

void BroadPhaseBVH::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {
	pair_callback = p_pair_callback;
	pair_userdata = p_userdata;
}

void BroadPhaseBVH::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {
	unpair_callback = p_unpair_callback;
	unpair_userdata = p_userdata;
}

void BroadPhaseBVH::update() {
	bvh.update();
}

BroadPhaseSW *BroadPhaseBVH::_create() {
	return memnew(BroadPhaseBVH);
}

BroadPhaseBVH::BroadPhaseBVH() {
	bvh.params_set_thread_safe(GLOBAL_GET("rendering/threads/thread_safe_bvh"));
	bvh.params_set_pairing_expansion(GLOBAL_GET("physics/3d/godot_physics/bvh_collision_margin"));
	bvh.set_pair_callback(_pair_callback, this);
	bvh.set_unpair_callback(_unpair_callback, this);
	bvh.set_check_pair_callback(_check_pair_callback, this);
	pair_callback = nullptr;
	pair_userdata = nullptr;
	unpair_userdata = nullptr;
}
