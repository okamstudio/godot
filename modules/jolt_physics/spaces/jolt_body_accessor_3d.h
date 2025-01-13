/**************************************************************************/
/*  jolt_body_accessor_3d.h                                               */
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

#ifndef JOLT_BODY_ACCESSOR_3D_H
#define JOLT_BODY_ACCESSOR_3D_H

#include "../objects/jolt_object_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/BodyLockInterface.h"

#include <variant>

class JoltArea3D;
class JoltBody3D;
class JoltShapedObject3D;
class JoltSpace3D;

class JoltBodyAccessor3D {
protected:
	struct BodyIDSpan {
		BodyIDSpan(const JPH::BodyID *p_ptr, int p_count) :
				ptr(p_ptr), count(p_count) {}

		const JPH::BodyID *ptr;
		int count;
	};

	virtual void _acquire_internal(const JPH::BodyID *p_ids, int p_id_count) = 0;
	virtual void _release_internal() = 0;

	const JoltSpace3D *space = nullptr;

	const JPH::BodyLockInterface *lock_iface = nullptr;

	std::variant<JPH::BodyID, JPH::BodyIDVector, BodyIDSpan> ids;

public:
	explicit JoltBodyAccessor3D(const JoltSpace3D *p_space);
	virtual ~JoltBodyAccessor3D() = 0;

	void acquire(const JPH::BodyID *p_ids, int p_id_count);
	void acquire(const JPH::BodyID &p_id);
	void acquire_active();
	void acquire_all();
	void release();

	bool is_acquired() const { return lock_iface != nullptr; }
	bool not_acquired() const { return lock_iface == nullptr; }

	const JoltSpace3D &get_space() const { return *space; }
	const JPH::BodyID *get_ids() const;
	int get_count() const;

	const JPH::BodyID &get_at(int p_index) const;
};

class JoltBodyReader3D final : public JoltBodyAccessor3D {
	virtual void _acquire_internal(const JPH::BodyID *p_ids, int p_id_count) override;
	virtual void _release_internal() override;

	JPH::BodyLockInterface::MutexMask mutex_mask = 0;

public:
	explicit JoltBodyReader3D(const JoltSpace3D *p_space);

	const JPH::Body *try_get(const JPH::BodyID &p_id) const;
	const JPH::Body *try_get(int p_index) const;
	const JPH::Body *try_get() const;
};

class JoltBodyWriter3D final : public JoltBodyAccessor3D {
	virtual void _acquire_internal(const JPH::BodyID *p_ids, int p_id_count) override;
	virtual void _release_internal() override;

	JPH::BodyLockInterface::MutexMask mutex_mask = 0;

public:
	explicit JoltBodyWriter3D(const JoltSpace3D *p_space);

	JPH::Body *try_get(const JPH::BodyID &p_id) const;
	JPH::Body *try_get(int p_index) const;
	JPH::Body *try_get() const;
};

template <typename TBodyAccessor>
class JoltScopedBodyAccessor3D {
	TBodyAccessor inner;

public:
	JoltScopedBodyAccessor3D(const JoltSpace3D &p_space, const JPH::BodyID *p_ids, int p_id_count) :
			inner(&p_space) { inner.acquire(p_ids, p_id_count); }

	JoltScopedBodyAccessor3D(const JoltSpace3D &p_space, const JPH::BodyID &p_id) :
			inner(&p_space) { inner.acquire(p_id); }

	JoltScopedBodyAccessor3D(const JoltScopedBodyAccessor3D &p_other) = delete;
	JoltScopedBodyAccessor3D(JoltScopedBodyAccessor3D &&p_other) = default;
	~JoltScopedBodyAccessor3D() { inner.release(); }

	const JoltSpace3D &get_space() const { return inner.get_space(); }
	int get_count() const { return inner.get_count(); }
	const JPH::BodyID &get_at(int p_index) const { return inner.get_at(p_index); }

	JoltScopedBodyAccessor3D &operator=(const JoltScopedBodyAccessor3D &p_other) = delete;
	JoltScopedBodyAccessor3D &operator=(JoltScopedBodyAccessor3D &&p_other) = default;

	decltype(auto) try_get(const JPH::BodyID &p_id) const { return inner.try_get(p_id); }
	decltype(auto) try_get(int p_index) const { return inner.try_get(p_index); }
	decltype(auto) try_get() const { return inner.try_get(); }
};

template <typename TAccessor, typename TBody>
class JoltAccessibleBody3D {
	TAccessor accessor;
	TBody *body = nullptr;

public:
	JoltAccessibleBody3D(const JoltSpace3D &p_space, const JPH::BodyID &p_id) :
			accessor(p_space, p_id), body(accessor.try_get()) {}

	bool is_valid() const { return body != nullptr; }
	bool is_invalid() const { return body == nullptr; }

	JoltObject3D *as_object() const {
		if (body != nullptr) {
			return reinterpret_cast<JoltObject3D *>(body->GetUserData());
		} else {
			return nullptr;
		}
	}

	JoltShapedObject3D *as_shaped() const {
		if (JoltObject3D *object = as_object(); object != nullptr && object->is_shaped()) {
			return reinterpret_cast<JoltShapedObject3D *>(body->GetUserData());
		} else {
			return nullptr;
		}
	}

	JoltBody3D *as_body() const {
		if (JoltObject3D *object = as_object(); object != nullptr && object->is_body()) {
			return reinterpret_cast<JoltBody3D *>(body->GetUserData());
		} else {
			return nullptr;
		}
	}

	JoltArea3D *as_area() const {
		if (JoltObject3D *object = as_object(); object != nullptr && object->is_area()) {
			return reinterpret_cast<JoltArea3D *>(body->GetUserData());
		} else {
			return nullptr;
		}
	}

	TBody *operator->() const { return body; }
	TBody &operator*() const { return *body; }

	explicit operator TBody *() const { return body; }
};

template <typename TAccessor, typename TBody>
class JoltAccessibleBodies3D {
	TAccessor accessor;

public:
	JoltAccessibleBodies3D(const JoltSpace3D &p_space, const JPH::BodyID *p_ids, int p_id_count) :
			accessor(p_space, p_ids, p_id_count) {}

	JoltAccessibleBody3D<TAccessor, TBody> operator[](int p_index) const {
		const JPH::BodyID &body_id = p_index < accessor.get_count() ? accessor.get_at(p_index) : JPH::BodyID();
		return JoltAccessibleBody3D<TAccessor, TBody>(accessor.get_space(), body_id);
	}
};

typedef JoltScopedBodyAccessor3D<JoltBodyReader3D> JoltScopedBodyReader3D;
typedef JoltScopedBodyAccessor3D<JoltBodyWriter3D> JoltScopedBodyWriter3D;

typedef JoltAccessibleBody3D<JoltScopedBodyReader3D, const JPH::Body> JoltReadableBody3D;
typedef JoltAccessibleBody3D<JoltScopedBodyWriter3D, JPH::Body> JoltWritableBody3D;

typedef JoltAccessibleBodies3D<JoltScopedBodyReader3D, const JPH::Body> JoltReadableBodies3D;
typedef JoltAccessibleBodies3D<JoltScopedBodyWriter3D, JPH::Body> JoltWritableBodies3D;

#endif // JOLT_BODY_ACCESSOR_3D_H
