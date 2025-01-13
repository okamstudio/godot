/**************************************************************************/
/*  jolt_shape_instance_3d.cpp                                            */
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

#include "jolt_shape_instance_3d.h"

#include "jolt_shape_3d.h"

#include "Jolt/Physics/Collision/Shape/DecoratedShape.h"

JoltShapeInstance3D::ShapeReference::ShapeReference(JoltShapedObject3D *p_parent, JoltShape3D *p_shape) :
		parent(p_parent),
		shape(p_shape) {
	if (shape != nullptr) {
		shape->add_owner(parent);
	}
}

JoltShapeInstance3D::ShapeReference::ShapeReference(const ShapeReference &p_other) :
		parent(p_other.parent),
		shape(p_other.shape) {
	if (shape != nullptr) {
		shape->add_owner(parent);
	}
}

JoltShapeInstance3D::ShapeReference::~ShapeReference() {
	if (shape != nullptr) {
		shape->remove_owner(parent);
	}
}

JoltShapeInstance3D::ShapeReference &JoltShapeInstance3D::ShapeReference::operator=(const ShapeReference &p_other) {
	if (shape != nullptr) {
		shape->remove_owner(parent);
	}

	parent = p_other.parent;
	shape = p_other.shape;

	if (shape != nullptr) {
		shape->add_owner(parent);
	}

	return *this;
}

JoltShapeInstance3D::JoltShapeInstance3D(JoltShapedObject3D *p_parent, JoltShape3D *p_shape, const Transform3D &p_transform, const Vector3 &p_scale, bool p_disabled) :
		transform(p_transform),
		scale(p_scale),
		shape(p_parent, p_shape),
		disabled(p_disabled) {
}

AABB JoltShapeInstance3D::get_aabb() const {
	return get_transform_scaled().xform(shape->get_aabb());
}

bool JoltShapeInstance3D::try_build() {
	ERR_FAIL_COND_V(is_disabled(), false);

	const JPH::ShapeRefC maybe_new_shape = shape->try_build();

	if (maybe_new_shape == nullptr) {
		jolt_ref = nullptr;
		return false;
	}

	if (jolt_ref != nullptr) {
		const JPH::DecoratedShape *outer_shape = static_cast<const JPH::DecoratedShape *>(jolt_ref.GetPtr());

		if (outer_shape->GetInnerShape() == maybe_new_shape) {
			return true;
		}
	}

	jolt_ref = JoltShape3D::with_user_data(maybe_new_shape, (uint64_t)id);

	return true;
}
