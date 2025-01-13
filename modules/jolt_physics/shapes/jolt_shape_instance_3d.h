/**************************************************************************/
/*  jolt_shape_instance_3d.h                                              */
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

#ifndef JOLT_SHAPE_INSTANCE_3D_H
#define JOLT_SHAPE_INSTANCE_3D_H

#include "core/math/transform_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/Shape/Shape.h"

class JoltShapedObject3D;
class JoltShape3D;

class JoltShapeInstance3D {
	// This RAII helper exists solely to avoid needing to maintain copy construction/assignment in the shape instance.
	// Ideally this would be move-only instead, but Godot's containers don't support that at the moment.
	struct ShapeReference {
		JoltShapedObject3D *parent = nullptr;
		JoltShape3D *shape = nullptr;

		ShapeReference() = default;
		ShapeReference(JoltShapedObject3D *p_parent, JoltShape3D *p_shape);
		ShapeReference(const ShapeReference &p_other);
		ShapeReference(ShapeReference &&p_other) = delete;
		~ShapeReference();

		ShapeReference &operator=(const ShapeReference &p_other);
		ShapeReference &operator=(ShapeReference &&p_other) = delete;

		JoltShape3D *operator*() const { return shape; }
		JoltShape3D *operator->() const { return shape; }
		operator JoltShape3D *() const { return shape; }
	};

	inline static uint32_t next_id = 1;

	Transform3D transform;
	Vector3 scale;
	ShapeReference shape;
	JPH::ShapeRefC jolt_ref;
	uint32_t id = next_id++;
	bool disabled = false;

public:
	JoltShapeInstance3D() = default;
	JoltShapeInstance3D(JoltShapedObject3D *p_parent, JoltShape3D *p_shape, const Transform3D &p_transform = Transform3D(), const Vector3 &p_scale = Vector3(1.0f, 1.0f, 1.0f), bool p_disabled = false);

	uint32_t get_id() const { return id; }

	JoltShape3D *get_shape() const { return shape; }

	const JPH::Shape *get_jolt_ref() const { return jolt_ref; }

	const Transform3D &get_transform_unscaled() const { return transform; }
	Transform3D get_transform_scaled() const { return transform.scaled_local(scale); }
	void set_transform(const Transform3D &p_transform) { transform = p_transform; }

	const Vector3 &get_scale() const { return scale; }
	void set_scale(const Vector3 &p_scale) { scale = p_scale; }

	AABB get_aabb() const;

	bool is_built() const { return jolt_ref != nullptr; }

	bool is_enabled() const { return !disabled; }
	bool is_disabled() const { return disabled; }

	void enable() { disabled = false; }
	void disable() { disabled = true; }

	bool try_build();
};

#endif // JOLT_SHAPE_INSTANCE_3D_H
