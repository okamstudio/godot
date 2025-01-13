/**************************************************************************/
/*  jolt_world_boundary_shape_3d.cpp                                      */
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

#include "jolt_world_boundary_shape_3d.h"

#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"

#include "Jolt/Physics/Collision/Shape/PlaneShape.h"

JPH::ShapeRefC JoltWorldBoundaryShape3D::_build() const {
	const Plane normalized_plane = plane.normalized();
	ERR_FAIL_COND_V_MSG(normalized_plane == Plane(), nullptr, vformat("Failed to build Jolt Physics world boundary shape with %s. The plane's normal must not be zero. This shape belongs to %s.", to_string(), _owners_to_string()));

	const float half_size = JoltProjectSettings::get_world_boundary_shape_size() / 2.0f;
	const JPH::PlaneShapeSettings shape_settings(to_jolt(normalized_plane), nullptr, half_size);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to build Jolt Physics world boundary shape with %s. It returned the following error: '%s'. This shape belongs to %s.", to_string(), to_godot(shape_result.GetError()), _owners_to_string()));

	return shape_result.Get();
}

Variant JoltWorldBoundaryShape3D::get_data() const {
	return plane;
}

void JoltWorldBoundaryShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::PLANE);

	const Plane new_plane = p_data;
	if (unlikely(new_plane == plane)) {
		return;
	}

	plane = p_data;

	destroy();
}

AABB JoltWorldBoundaryShape3D::get_aabb() const {
	const float size = JoltProjectSettings::get_world_boundary_shape_size();
	const float half_size = size / 2.0f;
	return AABB(Vector3(-half_size, -half_size, -half_size), Vector3(size, half_size, size));
}

String JoltWorldBoundaryShape3D::to_string() const {
	return vformat("{plane=%s}", plane);
}
