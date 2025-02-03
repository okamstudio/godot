/**************************************************************************/
/*  broad_phase_basic.cpp                                                 */
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

#include "broad_phase_basic.h"
#include "core/list.h"
#include "core/print_string.h"

BroadPhaseSW::ID BroadPhaseBasic::create(CollisionObjectSW *p_object, int p_subindex, const AABB &p_aabb, bool p_static, int p_collision_object_type) {
	ERR_FAIL_COND_V(p_object == nullptr, 0);

	current++;

	Element e;
	e.owner = p_object;
	e._static = false;
	e.subindex = p_subindex;

	element_map[current] = e;
	return current;
}

void BroadPhaseBasic::move(ID p_id, const AABB &p_aabb) {
	Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND(!E);
	E->get().aabb = p_aabb;
}

void BroadPhaseBasic::recheck_pairs(ID p_id) {
	// Not supported.
}

void BroadPhaseBasic::set_static(ID p_id, bool p_static, int p_collision_object_type) {
	Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND(!E);
	E->get()._static = p_static;
}

void BroadPhaseBasic::remove(ID p_id) {
	Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND(!E);
	List<PairKey> to_erase;
	//unpair must be done immediately on removal to avoid potential invalid pointers
	for (Map<PairKey, void *>::Element *F = pair_map.front(); F; F = F->next()) {
		if (F->key().a == p_id || F->key().b == p_id) {
			if (unpair_callback) {
				Element *elem_A = &element_map[F->key().a];
				Element *elem_B = &element_map[F->key().b];
				unpair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, F->get(), unpair_userdata);
			}
			to_erase.push_back(F->key());
		}
	}
	while (to_erase.size()) {
		pair_map.erase(to_erase.front()->get());
		to_erase.pop_front();
	}
	element_map.erase(E);
}

CollisionObjectSW *BroadPhaseBasic::get_object(ID p_id) const {
	const Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND_V(!E, nullptr);
	return E->get().owner;
}
bool BroadPhaseBasic::is_static(ID p_id) const {
	const Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND_V(!E, false);
	return E->get()._static;
}
int BroadPhaseBasic::get_subindex(ID p_id) const {
	const Map<ID, Element>::Element *E = element_map.find(p_id);
	ERR_FAIL_COND_V(!E, -1);
	return E->get().subindex;
}

int BroadPhaseBasic::cull_point(const Vector3 &p_point, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	int rc = 0;

	for (Map<ID, Element>::Element *E = element_map.front(); E; E = E->next()) {
		const AABB aabb = E->get().aabb;
		if (aabb.has_point(p_point)) {
			p_results[rc] = E->get().owner;
			p_result_indices[rc] = E->get().subindex;
			rc++;
			if (rc >= p_max_results) {
				break;
			}
		}
	}

	return rc;
}

int BroadPhaseBasic::cull_segment(const Vector3 &p_from, const Vector3 &p_to, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	int rc = 0;

	for (Map<ID, Element>::Element *E = element_map.front(); E; E = E->next()) {
		const AABB aabb = E->get().aabb;
		if (aabb.intersects_segment(p_from, p_to)) {
			p_results[rc] = E->get().owner;
			p_result_indices[rc] = E->get().subindex;
			rc++;
			if (rc >= p_max_results) {
				break;
			}
		}
	}

	return rc;
}
int BroadPhaseBasic::cull_aabb(const AABB &p_aabb, CollisionObjectSW **p_results, int p_max_results, int *p_result_indices) {
	int rc = 0;

	for (Map<ID, Element>::Element *E = element_map.front(); E; E = E->next()) {
		const AABB aabb = E->get().aabb;
		if (aabb.intersects(p_aabb)) {
			p_results[rc] = E->get().owner;
			p_result_indices[rc] = E->get().subindex;
			rc++;
			if (rc >= p_max_results) {
				break;
			}
		}
	}

	return rc;
}

void BroadPhaseBasic::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {
	pair_userdata = p_userdata;
	pair_callback = p_pair_callback;
}
void BroadPhaseBasic::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {
	unpair_userdata = p_userdata;
	unpair_callback = p_unpair_callback;
}

void BroadPhaseBasic::update() {
	// recompute pairs
	for (Map<ID, Element>::Element *I = element_map.front(); I; I = I->next()) {
		for (Map<ID, Element>::Element *J = I->next(); J; J = J->next()) {
			Element *elem_A = &I->get();
			Element *elem_B = &J->get();

			if (elem_A->owner == elem_B->owner) {
				continue;
			}

			bool pair_ok = elem_A->aabb.intersects(elem_B->aabb) && (!elem_A->_static || !elem_B->_static);

			PairKey key(I->key(), J->key());

			Map<PairKey, void *>::Element *E = pair_map.find(key);

			if (!pair_ok && E) {
				if (unpair_callback) {
					unpair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, E->get(), unpair_userdata);
				}
				pair_map.erase(key);
			}

			if (pair_ok && !E) {
				void *data = nullptr;
				if (pair_callback) {
					data = pair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, nullptr, unpair_userdata);
					if (data) {
						pair_map.insert(key, data);
					}
				}
			}
		}
	}
}

BroadPhaseSW *BroadPhaseBasic::_create() {
	return memnew(BroadPhaseBasic);
}

BroadPhaseBasic::BroadPhaseBasic() {
	current = 1;
	unpair_callback = nullptr;
	unpair_userdata = nullptr;
	pair_callback = nullptr;
	pair_userdata = nullptr;
}
