/**************************************************************************/
/*  jolt_concave_polygon_shape_3d.cpp                                     */
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

#include "jolt_concave_polygon_shape_3d.h"

#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"

#include "Jolt/Physics/Collision/Shape/MeshShape.h"

JPH::ShapeRefC JoltConcavePolygonShape3D::_build() const {
	const int vertex_count = (int)faces.size();
	const int face_count = vertex_count / 3;
	const int excess_vertex_count = vertex_count % 3;

	if (unlikely(vertex_count == 0)) {
		return nullptr;
	}

	ERR_FAIL_COND_V_MSG(vertex_count < 3, nullptr, vformat("Failed to build Jolt Physics concave polygon shape with %s. It must have a vertex count of at least 3. This shape belongs to %s.", to_string(), _owners_to_string()));
	ERR_FAIL_COND_V_MSG(excess_vertex_count != 0, nullptr, vformat("Failed to build Jolt Physics concave polygon shape with %s. It must have a vertex count that is divisible by 3. This shape belongs to %s.", to_string(), _owners_to_string()));

	JPH::TriangleList jolt_faces;
	jolt_faces.reserve((size_t)face_count);

	const Vector3 *faces_begin = &faces[0];
	const Vector3 *faces_end = faces_begin + vertex_count;
	JPH::uint32 triangle_index = 0;

	for (const Vector3 *vertex = faces_begin; vertex != faces_end; vertex += 3) {
		const Vector3 *v0 = vertex + 0;
		const Vector3 *v1 = vertex + 1;
		const Vector3 *v2 = vertex + 2;

		// Jolt uses a different winding order, so we swizzle the vertices to account for that.
		jolt_faces.emplace_back(
				JPH::Float3((float)v2->x, (float)v2->y, (float)v2->z),
				JPH::Float3((float)v1->x, (float)v1->y, (float)v1->z),
				JPH::Float3((float)v0->x, (float)v0->y, (float)v0->z),
				0,
				triangle_index++);
	}

	JPH::MeshShapeSettings shape_settings(jolt_faces);
	shape_settings.mActiveEdgeCosThresholdAngle = JoltProjectSettings::get_active_edge_threshold();
	shape_settings.mPerTriangleUserData = JoltProjectSettings::enable_ray_cast_face_index();

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to build Jolt Physics concave polygon shape with %s. It returned the following error: '%s'. This shape belongs to %s.", to_string(), to_godot(shape_result.GetError()), _owners_to_string()));

	return JoltShape3D::with_double_sided(shape_result.Get(), back_face_collision);
}

AABB JoltConcavePolygonShape3D::_calculate_aabb() const {
	AABB result;

	for (int i = 0; i < faces.size(); ++i) {
		const Vector3 &vertex = faces[i];

		if (i == 0) {
			result.position = vertex;
		} else {
			result.expand_to(vertex);
		}
	}

	return result;
}

Variant JoltConcavePolygonShape3D::get_data() const {
	Dictionary data;
	data["faces"] = faces;
	data["backface_collision"] = back_face_collision;
	return data;
}

void JoltConcavePolygonShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_faces = data.get("faces", Variant());
	ERR_FAIL_COND(maybe_faces.get_type() != Variant::PACKED_VECTOR3_ARRAY);

	const Variant maybe_back_face_collision = data.get("backface_collision", Variant());
	ERR_FAIL_COND(maybe_back_face_collision.get_type() != Variant::BOOL);

	faces = maybe_faces;
	back_face_collision = maybe_back_face_collision;

	aabb = _calculate_aabb();

	destroy();
}

String JoltConcavePolygonShape3D::to_string() const {
	return vformat("{vertex_count=%d}", faces.size());
}
