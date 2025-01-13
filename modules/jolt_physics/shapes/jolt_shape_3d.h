/**************************************************************************/
/*  jolt_shape_3d.h                                                       */
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

#ifndef JOLT_SHAPE_3D_H
#define JOLT_SHAPE_3D_H

#include "servers/physics_server_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/Shape/Shape.h"

class JoltShapedObject3D;

class JoltShape3D {
protected:
	HashMap<JoltShapedObject3D *, int> ref_counts_by_owner;
	Mutex jolt_ref_mutex;
	RID rid;
	JPH::ShapeRefC jolt_ref;

	virtual JPH::ShapeRefC _build() const = 0;

	String _owners_to_string() const;

public:
	typedef PhysicsServer3D::ShapeType ShapeType;

	virtual ~JoltShape3D() = 0;

	RID get_rid() const { return rid; }
	void set_rid(const RID &p_rid) { rid = p_rid; }

	void add_owner(JoltShapedObject3D *p_owner);
	void remove_owner(JoltShapedObject3D *p_owner);
	void remove_self();

	virtual ShapeType get_type() const = 0;
	virtual bool is_convex() const = 0;

	virtual Variant get_data() const = 0;
	virtual void set_data(const Variant &p_data) = 0;

	virtual float get_margin() const = 0;
	virtual void set_margin(float p_margin) = 0;

	virtual AABB get_aabb() const = 0;

	float get_solver_bias() const;
	void set_solver_bias(float p_bias);

	JPH::ShapeRefC try_build();

	void destroy();

	const JPH::Shape *get_jolt_ref() const { return jolt_ref; }

	static JPH::ShapeRefC with_scale(const JPH::Shape *p_shape, const Vector3 &p_scale);
	static JPH::ShapeRefC with_basis_origin(const JPH::Shape *p_shape, const Basis &p_basis, const Vector3 &p_origin);
	static JPH::ShapeRefC with_center_of_mass_offset(const JPH::Shape *p_shape, const Vector3 &p_offset);
	static JPH::ShapeRefC with_center_of_mass(const JPH::Shape *p_shape, const Vector3 &p_center_of_mass);
	static JPH::ShapeRefC with_user_data(const JPH::Shape *p_shape, uint64_t p_user_data);
	static JPH::ShapeRefC with_double_sided(const JPH::Shape *p_shape, bool p_back_face_collision);
	static JPH::ShapeRefC without_custom_shapes(const JPH::Shape *p_shape);

	static Vector3 make_scale_valid(const JPH::Shape *p_shape, const Vector3 &p_scale);
	static bool is_scale_valid(const Vector3 &p_scale, const Vector3 &p_valid_scale, real_t p_tolerance = 0.01f);
};

#ifdef DEBUG_ENABLED

#define JOLT_ENSURE_SCALE_NOT_ZERO(m_transform, m_msg)                                                         \
	if (unlikely((m_transform).basis.determinant() == 0.0f)) {                                                 \
		WARN_PRINT(vformat("%s "                                                                               \
						   "The basis of the transform was singular, which is not supported by Jolt Physics. " \
						   "This is likely caused by one or more axes having a scale of zero. "                \
						   "The basis (and thus its scale) will be treated as identity.",                      \
				m_msg));                                                                                       \
                                                                                                               \
		(m_transform).basis = Basis();                                                                         \
	} else                                                                                                     \
		((void)0)

#define ERR_PRINT_INVALID_SCALE_MSG(m_scale, m_valid_scale, m_msg)                               \
	if (unlikely(!JoltShape3D::is_scale_valid(m_scale, valid_scale))) {                          \
		ERR_PRINT(vformat("%s "                                                                  \
						  "A scale of %v is not supported by Jolt Physics for this shape/body. " \
						  "The scale will instead be treated as %v.",                            \
				m_msg, m_scale, valid_scale));                                                   \
	} else                                                                                       \
		((void)0)

#else

#define JOLT_ENSURE_SCALE_NOT_ZERO(m_transform, m_msg)

#define ERR_PRINT_INVALID_SCALE_MSG(m_scale, m_valid_scale, m_msg)

#endif

#define JOLT_ENSURE_SCALE_VALID(m_shape, m_scale, m_msg)                             \
	if (true) {                                                                      \
		const Vector3 valid_scale = JoltShape3D::make_scale_valid(m_shape, m_scale); \
		ERR_PRINT_INVALID_SCALE_MSG(m_scale, valid_scale, m_msg);                    \
		(m_scale) = valid_scale;                                                     \
	} else                                                                           \
		((void)0)

#endif // JOLT_SHAPE_3D_H
