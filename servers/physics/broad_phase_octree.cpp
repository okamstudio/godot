/**************************************************************************/
/*  broad_phase_octree.cpp                                                */
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

#include "broad_phase_octree.h"
#include "collision_object_sw.h"

BroadPhaseSW::ID BroadPhaseOctree::create(CollisionObjectSW *p_object, int p_subindex, const AABB &p_aabb, bool p_static, int p_collision_object_type) {
	ID oid = octree.create(p_object, AABB(), p_subindex, false, 1 << p_object->get_type(), 0);
	return oid;
}

void BroadPhaseOctree::move(ID p_id, const AABB &p_aabb) {
	octree.move(p_id, p_aabb);
}

void BroadPhaseOctree::recheck_pairs(ID p_id) {
	AABB aabb = octree.get_aabb(p_id);
	octree.move(p_id, aabb);
}

void BroadPhaseOctree::set_static(ID p_id, bool p_static, int p_collision_object_type) {
	CollisionObjectSW *it = octree.get(p_id);
	octree.set_pairable(p_id, !p_static, 1 << it->get_type(), p_static ? 0 : 0xFFFFF); //pair everything, don't care 1?
}

void BroadPhaseOctree::remove(ID p_id) {
	octree.erase(p_id);
}

CollisionObjectSW *BroadPhaseOctree::get_object(ID p_id) const {
	CollisionObjectSW *it = octree.get(p_id);
	ERR_FAIL_COND_V(!it, nullptr);
	return it;
}
bool BroadPhaseOctree::is_static(ID p_id) const {
	return !octree.is_pairable(p_id);
}
int BroadPhaseOctree::get_subindex(ID p_id) const {
	return octree.get_subindex(p_id);
}

int BroadPhaseOctree::cull_point(const Vector3 &p_point, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return octree.cull_point(p_point, p_results, p_max_results, p_result_indices);
}

int BroadPhaseOctree::cull_segment(const Vector3 &p_from, const Vector3 &p_to, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return octree.cull_segment(p_from, p_to, p_results, p_max_results, p_result_indices);
}

int BroadPhaseOctree::cull_aabb(const AABB &p_aabb, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	return octree.cull_aabb(p_aabb, p_results, p_max_results, p_result_indices);
}

void *BroadPhaseOctree::_pair_callback(void *self, OctreeElementID p_A, CollisionObjectSW *p_object_A, int subindex_A, OctreeElementID p_B, CollisionObjectSW *p_object_B, int subindex_B) {
	BroadPhaseOctree *bpo = (BroadPhaseOctree *)(self);
	if (!bpo->pair_callback) {
		return nullptr;
	}

	bool valid_collision_pair = p_object_A->test_collision_mask(p_object_B);

	if (!valid_collision_pair) {
		return nullptr;
	}

	return bpo->pair_callback(p_object_A, subindex_A, p_object_B, subindex_B, nullptr, bpo->pair_userdata);
}

void BroadPhaseOctree::_unpair_callback(void *self, OctreeElementID p_A, CollisionObjectSW *p_object_A, int subindex_A, OctreeElementID p_B, CollisionObjectSW *p_object_B, int subindex_B, void *pairdata) {
	BroadPhaseOctree *bpo = (BroadPhaseOctree *)(self);
	if (!bpo->unpair_callback) {
		return;
	}

	bpo->unpair_callback(p_object_A, subindex_A, p_object_B, subindex_B, pairdata, bpo->unpair_userdata);
}

void BroadPhaseOctree::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {
	pair_callback = p_pair_callback;
	pair_userdata = p_userdata;
}
void BroadPhaseOctree::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {
	unpair_callback = p_unpair_callback;
	unpair_userdata = p_userdata;
}

void BroadPhaseOctree::update() {
	// does.. not?
}

BroadPhaseSW *BroadPhaseOctree::_create() {
	return memnew(BroadPhaseOctree);
}

BroadPhaseOctree::BroadPhaseOctree() {
	octree.set_pair_callback(_pair_callback, this);
	octree.set_unpair_callback(_unpair_callback, this);
	pair_callback = nullptr;
	pair_userdata = nullptr;
	unpair_userdata = nullptr;
}
