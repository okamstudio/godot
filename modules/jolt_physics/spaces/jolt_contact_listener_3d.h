/**************************************************************************/
/*  jolt_contact_listener_3d.h                                            */
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

#ifndef JOLT_CONTACT_LISTENER_3D_H
#define JOLT_CONTACT_LISTENER_3D_H

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/local_vector.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/variant.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Collision/ContactListener.h"
#include "Jolt/Physics/SoftBody/SoftBodyContactListener.h"

#include <stdint.h>
#include <new>

class JoltShapedObject3D;
class JoltSpace3D;

class JoltContactListener3D final
		: public JPH::ContactListener,
		  public JPH::SoftBodyContactListener {
	struct BodyIDHasher {
		static uint32_t hash(const JPH::BodyID &p_id) { return hash_fmix32(p_id.GetIndexAndSequenceNumber()); }
	};

	struct ShapePairHasher {
		static uint32_t hash(const JPH::SubShapeIDPair &p_pair) {
			uint32_t hash = hash_murmur3_one_32(p_pair.GetBody1ID().GetIndexAndSequenceNumber());
			hash = hash_murmur3_one_32(p_pair.GetSubShapeID1().GetValue(), hash);
			hash = hash_murmur3_one_32(p_pair.GetBody2ID().GetIndexAndSequenceNumber(), hash);
			hash = hash_murmur3_one_32(p_pair.GetSubShapeID2().GetValue(), hash);
			return hash_fmix32(hash);
		}
	};

	struct Contact {
		Vector3 point_self;
		Vector3 point_other;
		Vector3 normal;
		Vector3 velocity_self;
		Vector3 velocity_other;
		Vector3 impulse;
	};

	typedef LocalVector<Contact> Contacts;

	struct Manifold {
		Contacts contacts1;
		Contacts contacts2;
		float depth = 0.0f;
	};

	HashMap<JPH::SubShapeIDPair, Manifold, ShapePairHasher> manifolds_by_shape_pair;
	HashSet<JPH::SubShapeIDPair, ShapePairHasher> area_overlaps;
	HashSet<JPH::SubShapeIDPair, ShapePairHasher> area_enters;
	HashSet<JPH::SubShapeIDPair, ShapePairHasher> area_exits;
	Mutex write_mutex;
	JoltSpace3D *space = nullptr;

#ifdef DEBUG_ENABLED
	PackedVector3Array debug_contacts;
	std::atomic_int debug_contact_count = 0;
#endif

	virtual void OnContactAdded(const JPH::Body &p_body1, const JPH::Body &p_body2, const JPH::ContactManifold &p_manifold, JPH::ContactSettings &p_settings) override;
	virtual void OnContactPersisted(const JPH::Body &p_body1, const JPH::Body &p_body2, const JPH::ContactManifold &p_manifold, JPH::ContactSettings &p_settings) override;
	virtual void OnContactRemoved(const JPH::SubShapeIDPair &p_shape_pair) override;

	virtual JPH::SoftBodyValidateResult OnSoftBodyContactValidate(const JPH::Body &p_soft_body, const JPH::Body &p_other_body, JPH::SoftBodyContactSettings &p_settings) override;

#ifdef DEBUG_ENABLED
	virtual void OnSoftBodyContactAdded(const JPH::Body &p_soft_body, const JPH::SoftBodyManifold &p_manifold) override;
#endif

	bool _try_override_collision_response(const JPH::Body &p_jolt_body1, const JPH::Body &p_jolt_body2, JPH::ContactSettings &p_settings);
	bool _try_override_collision_response(const JPH::Body &p_jolt_soft_body, const JPH::Body &p_jolt_other_body, JPH::SoftBodyContactSettings &p_settings);
	bool _try_apply_surface_velocities(const JPH::Body &p_jolt_body1, const JPH::Body &p_jolt_body2, JPH::ContactSettings &p_settings);
	bool _try_add_contacts(const JPH::Body &p_jolt_body1, const JPH::Body &p_jolt_body2, const JPH::ContactManifold &p_manifold, JPH::ContactSettings &p_settings);
	bool _try_evaluate_area_overlap(const JPH::Body &p_body1, const JPH::Body &p_body2, const JPH::ContactManifold &p_manifold);
	bool _try_remove_contacts(const JPH::SubShapeIDPair &p_shape_pair);
	bool _try_remove_area_overlap(const JPH::SubShapeIDPair &p_shape_pair);

#ifdef DEBUG_ENABLED
	bool _try_add_debug_contacts(const JPH::Body &p_body1, const JPH::Body &p_body2, const JPH::ContactManifold &p_manifold);
	bool _try_add_debug_contacts(const JPH::Body &p_soft_body, const JPH::SoftBodyManifold &p_manifold);
#endif

	void _flush_contacts();
	void _flush_area_enters();
	void _flush_area_shifts();
	void _flush_area_exits();

public:
	explicit JoltContactListener3D(JoltSpace3D *p_space) :
			space(p_space) {}

	void pre_step();
	void post_step();

#ifdef DEBUG_ENABLED
	const PackedVector3Array &get_debug_contacts() const { return debug_contacts; }
	int get_debug_contact_count() const { return debug_contact_count.load(std::memory_order_acquire); }
	int get_max_debug_contacts() const { return (int)debug_contacts.size(); }
	void set_max_debug_contacts(int p_count) { debug_contacts.resize(p_count); }
#endif
};

#endif // JOLT_CONTACT_LISTENER_3D_H
