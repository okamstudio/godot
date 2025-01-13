/**************************************************************************/
/*  jolt_body_accessor_3d.cpp                                             */
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

#include "jolt_body_accessor_3d.h"

#include "jolt_space_3d.h"

namespace {

template <class... TTypes>
struct VariantVisitors : TTypes... {
	using TTypes::operator()...;
};

template <class... TTypes>
VariantVisitors(TTypes...) -> VariantVisitors<TTypes...>;

} // namespace

JoltBodyAccessor3D::JoltBodyAccessor3D(const JoltSpace3D *p_space) :
		space(p_space) {
}

JoltBodyAccessor3D::~JoltBodyAccessor3D() = default;

void JoltBodyAccessor3D::acquire(const JPH::BodyID *p_ids, int p_id_count) {
	ERR_FAIL_NULL(space);

	lock_iface = &space->get_lock_iface();
	ids = BodyIDSpan(p_ids, p_id_count);
	_acquire_internal(p_ids, p_id_count);
}

void JoltBodyAccessor3D::acquire(const JPH::BodyID &p_id) {
	ERR_FAIL_NULL(space);

	lock_iface = &space->get_lock_iface();
	ids = p_id;
	_acquire_internal(&p_id, 1);
}

void JoltBodyAccessor3D::acquire_active() {
	const JPH::PhysicsSystem &physics_system = space->get_physics_system();

	acquire(physics_system.GetActiveBodiesUnsafe(JPH::EBodyType::RigidBody), (int)physics_system.GetNumActiveBodies(JPH::EBodyType::RigidBody));
}

void JoltBodyAccessor3D::acquire_all() {
	ERR_FAIL_NULL(space);

	lock_iface = &space->get_lock_iface();

	JPH::BodyIDVector *vector = std::get_if<JPH::BodyIDVector>(&ids);

	if (vector == nullptr) {
		ids = JPH::BodyIDVector();
		vector = std::get_if<JPH::BodyIDVector>(&ids);
	}

	space->get_physics_system().GetBodies(*vector);

	_acquire_internal(vector->data(), (int)vector->size());
}

void JoltBodyAccessor3D::release() {
	_release_internal();
	lock_iface = nullptr;
}

const JPH::BodyID *JoltBodyAccessor3D::get_ids() const {
	ERR_FAIL_COND_V(not_acquired(), nullptr);

	return std::visit(
			VariantVisitors{
					[](const JPH::BodyID &p_id) { return &p_id; },
					[](const JPH::BodyIDVector &p_vector) { return p_vector.data(); },
					[](const BodyIDSpan &p_span) { return p_span.ptr; } },
			ids);
}

int JoltBodyAccessor3D::get_count() const {
	ERR_FAIL_COND_V(not_acquired(), 0);

	return std::visit(
			VariantVisitors{
					[](const JPH::BodyID &p_id) { return 1; },
					[](const JPH::BodyIDVector &p_vector) { return (int)p_vector.size(); },
					[](const BodyIDSpan &p_span) { return p_span.count; } },
			ids);
}

const JPH::BodyID &JoltBodyAccessor3D::get_at(int p_index) const {
	CRASH_BAD_INDEX(p_index, get_count());
	return get_ids()[p_index];
}

void JoltBodyReader3D::_acquire_internal(const JPH::BodyID *p_ids, int p_id_count) {
	mutex_mask = lock_iface->GetMutexMask(p_ids, p_id_count);
	lock_iface->LockRead(mutex_mask);
}

void JoltBodyReader3D::_release_internal() {
	ERR_FAIL_COND(not_acquired());
	lock_iface->UnlockRead(mutex_mask);
}

JoltBodyReader3D::JoltBodyReader3D(const JoltSpace3D *p_space) :
		JoltBodyAccessor3D(p_space) {
}

const JPH::Body *JoltBodyReader3D::try_get(const JPH::BodyID &p_id) const {
	if (unlikely(p_id.IsInvalid())) {
		return nullptr;
	}

	ERR_FAIL_COND_V(not_acquired(), nullptr);

	return lock_iface->TryGetBody(p_id);
}

const JPH::Body *JoltBodyReader3D::try_get(int p_index) const {
	const int count = get_count();
	if (unlikely(p_index < 0 || p_index >= count)) {
		return nullptr;
	}

	return try_get(get_at(p_index));
}

const JPH::Body *JoltBodyReader3D::try_get() const {
	return try_get(0);
}

void JoltBodyWriter3D::_acquire_internal(const JPH::BodyID *p_ids, int p_id_count) {
	mutex_mask = lock_iface->GetMutexMask(p_ids, p_id_count);
	lock_iface->LockWrite(mutex_mask);
}

void JoltBodyWriter3D::_release_internal() {
	ERR_FAIL_COND(not_acquired());
	lock_iface->UnlockWrite(mutex_mask);
}

JoltBodyWriter3D::JoltBodyWriter3D(const JoltSpace3D *p_space) :
		JoltBodyAccessor3D(p_space) {
}

JPH::Body *JoltBodyWriter3D::try_get(const JPH::BodyID &p_id) const {
	if (unlikely(p_id.IsInvalid())) {
		return nullptr;
	}

	ERR_FAIL_COND_V(not_acquired(), nullptr);

	return lock_iface->TryGetBody(p_id);
}

JPH::Body *JoltBodyWriter3D::try_get(int p_index) const {
	const int count = get_count();
	if (unlikely(p_index < 0 || p_index >= count)) {
		return nullptr;
	}

	return try_get(get_at(p_index));
}

JPH::Body *JoltBodyWriter3D::try_get() const {
	return try_get(0);
}
