/**************************************************************************/
/*  jolt_capsule_shape_3d.cpp                                             */
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

#include "jolt_capsule_shape_3d.h"

#include "../misc/jolt_type_conversions.h"

#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

JPH::ShapeRefC JoltCapsuleShape3D::_build() const {
	ERR_FAIL_COND_V_MSG(radius <= 0.0f, nullptr, vformat("Failed to build Jolt Physics capsule shape with %s. Its radius must be greater than 0. This shape belongs to %s.", to_string(), _owners_to_string()));
	ERR_FAIL_COND_V_MSG(height <= 0.0f, nullptr, vformat("Failed to build Jolt Physics capsule shape with %s. Its height must be greater than 0. This shape belongs to %s.", to_string(), _owners_to_string()));
	ERR_FAIL_COND_V_MSG(height < radius * 2.0f, nullptr, vformat("Failed to build Jolt Physics capsule shape with %s. Its height must be at least double that of its radius. This shape belongs to %s.", to_string(), _owners_to_string()));

	const float half_height = height / 2.0f;
	const float cylinder_height = half_height - radius;

	const JPH::CapsuleShapeSettings shape_settings(cylinder_height, radius);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to build Jolt Physics capsule shape with %s. It returned the following error: '%s'. This shape belongs to %s.", to_string(), to_godot(shape_result.GetError()), _owners_to_string()));

	return shape_result.Get();
}

Variant JoltCapsuleShape3D::get_data() const {
	Dictionary data;
	data["height"] = height;
	data["radius"] = radius;
	return data;
}

void JoltCapsuleShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_height = data.get("height", Variant());
	ERR_FAIL_COND(maybe_height.get_type() != Variant::FLOAT);

	const Variant maybe_radius = data.get("radius", Variant());
	ERR_FAIL_COND(maybe_radius.get_type() != Variant::FLOAT);

	const float new_height = maybe_height;
	const float new_radius = maybe_radius;

	if (unlikely(new_height == height && new_radius == radius)) {
		return;
	}

	height = new_height;
	radius = new_radius;

	destroy();
}

AABB JoltCapsuleShape3D::get_aabb() const {
	const Vector3 half_extents(radius, height / 2.0f, radius);
	return AABB(-half_extents, half_extents * 2.0f);
}

String JoltCapsuleShape3D::to_string() const {
	return vformat("{height=%f radius=%f}", height, radius);
}
