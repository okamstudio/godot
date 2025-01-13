/**************************************************************************/
/*  jolt_height_map_shape_3d.cpp                                          */
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

#include "jolt_height_map_shape_3d.h"

#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"

#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"

namespace {

bool _is_vertex_hole(const JPH::VertexList &p_vertices, int p_index) {
	const float height = p_vertices[(size_t)p_index].y;
	return height == FLT_MAX || Math::is_nan(height);
}

bool _is_triangle_hole(const JPH::VertexList &p_vertices, int p_index0, int p_index1, int p_index2) {
	return _is_vertex_hole(p_vertices, p_index0) || _is_vertex_hole(p_vertices, p_index1) || _is_vertex_hole(p_vertices, p_index2);
}

} // namespace

JPH::ShapeRefC JoltHeightMapShape3D::_build() const {
	const int height_count = (int)heights.size();
	if (unlikely(height_count == 0)) {
		return nullptr;
	}

	ERR_FAIL_COND_V_MSG(height_count != width * depth, nullptr, vformat("Failed to build Jolt Physics height map shape with %s. Height count must be the product of width and depth. This shape belongs to %s.", to_string(), _owners_to_string()));
	ERR_FAIL_COND_V_MSG(width < 2 || depth < 2, nullptr, vformat("Failed to build Jolt Physics height map shape with %s. The height map must be at least 2x2. This shape belongs to %s.", to_string(), _owners_to_string()));

	if (width != depth) {
		return JoltShape3D::with_double_sided(_build_mesh(), true);
	}

	const int block_size = 2; // Default of JPH::HeightFieldShapeSettings::mBlockSize
	const int block_count = width / block_size;

	if (block_count < 2) {
		return JoltShape3D::with_double_sided(_build_mesh(), true);
	}

	return JoltShape3D::with_double_sided(_build_height_field(), true);
}

JPH::ShapeRefC JoltHeightMapShape3D::_build_height_field() const {
	const int quad_count_x = width - 1;
	const int quad_count_y = depth - 1;

	const float offset_x = (float)-quad_count_x / 2.0f;
	const float offset_y = (float)-quad_count_y / 2.0f;

	// Jolt triangulates the height map differently from how Godot Physics does it, so we mirror the shape along the
	// Z-axis to get the desired triangulation and reverse the rows to undo the mirroring.

	LocalVector<float> heights_rev;
	heights_rev.resize(heights.size());

	const real_t *heights_ptr = heights.ptr();
	float *heights_rev_ptr = heights_rev.ptr();

	for (int z = 0; z < depth; ++z) {
		const int z_rev = (depth - 1) - z;

		const real_t *row = heights_ptr + ptrdiff_t(z * width);
		float *row_rev = heights_rev_ptr + ptrdiff_t(z_rev * width);

		for (int x = 0; x < width; ++x) {
			const real_t height = row[x];

			// Godot has undocumented (accidental?) support for holes by passing NaN as the height value, whereas Jolt
			// uses `FLT_MAX` instead, so we translate any NaN to `FLT_MAX` in order to be drop-in compatible.
			row_rev[x] = Math::is_nan(height) ? FLT_MAX : (float)height;
		}
	}

	JPH::HeightFieldShapeSettings shape_settings(heights_rev.ptr(), JPH::Vec3(offset_x, 0, offset_y), JPH::Vec3::sReplicate(1.0f), (JPH::uint32)width);

	shape_settings.mBitsPerSample = shape_settings.CalculateBitsPerSampleForError(0.0f);
	shape_settings.mActiveEdgeCosThresholdAngle = JoltProjectSettings::get_active_edge_threshold();

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to build Jolt Physics height map shape with %s. It returned the following error: '%s'. This shape belongs to %s.", to_string(), to_godot(shape_result.GetError()), _owners_to_string()));

	return with_scale(shape_result.Get(), Vector3(1, 1, -1));
}

JPH::ShapeRefC JoltHeightMapShape3D::_build_mesh() const {
	const int height_count = (int)heights.size();

	const int quad_count_x = width - 1;
	const int quad_count_z = depth - 1;

	const int quad_count = quad_count_x * quad_count_z;
	const int triangle_count = quad_count * 2;

	JPH::VertexList vertices;
	vertices.reserve((size_t)height_count);

	JPH::IndexedTriangleList indices;
	indices.reserve((size_t)triangle_count);

	const float offset_x = (float)-quad_count_x / 2.0f;
	const float offset_z = (float)-quad_count_z / 2.0f;

	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			const float vertex_x = offset_x + (float)x;
			const float vertex_y = (float)heights[z * width + x];
			const float vertex_z = offset_z + (float)z;

			vertices.emplace_back(vertex_x, vertex_y, vertex_z);
		}
	}

	for (int z = 0; z < quad_count_z; ++z) {
		for (int x = 0; x < quad_count_x; ++x) {
			const int index_lower_right = z * width + x;
			const int index_lower_left = z * width + (x + 1);
			const int index_upper_right = (z + 1) * width + x;
			const int index_upper_left = (z + 1) * width + (x + 1);

			if (!_is_triangle_hole(vertices, index_lower_right, index_upper_right, index_lower_left)) {
				indices.emplace_back(index_lower_right, index_upper_right, index_lower_left);
			}

			if (!_is_triangle_hole(vertices, index_lower_left, index_upper_right, index_upper_left)) {
				indices.emplace_back(index_lower_left, index_upper_right, index_upper_left);
			}
		}
	}

	JPH::MeshShapeSettings shape_settings(std::move(vertices), std::move(indices));
	shape_settings.mActiveEdgeCosThresholdAngle = JoltProjectSettings::get_active_edge_threshold();

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to build Jolt Physics height map shape (as polygon) with %s. It returned the following error: '%s'. This shape belongs to %s.", to_string(), to_godot(shape_result.GetError()), _owners_to_string()));

	return shape_result.Get();
}

AABB JoltHeightMapShape3D::_calculate_aabb() const {
	AABB result;

	const int quad_count_x = width - 1;
	const int quad_count_z = depth - 1;

	const float offset_x = (float)-quad_count_x / 2.0f;
	const float offset_z = (float)-quad_count_z / 2.0f;

	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			const Vector3 vertex(offset_x + (float)x, (float)heights[z * width + x], offset_z + (float)z);

			if (x == 0 && z == 0) {
				result.position = vertex;
			} else {
				result.expand_to(vertex);
			}
		}
	}

	return result;
}

Variant JoltHeightMapShape3D::get_data() const {
	Dictionary data;
	data["width"] = width;
	data["depth"] = depth;
	data["heights"] = heights;
	return data;
}

void JoltHeightMapShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_heights = data.get("heights", Variant());

#ifdef REAL_T_IS_DOUBLE
	ERR_FAIL_COND(maybe_heights.get_type() != Variant::PACKED_FLOAT64_ARRAY);
#else
	ERR_FAIL_COND(maybe_heights.get_type() != Variant::PACKED_FLOAT32_ARRAY);
#endif

	const Variant maybe_width = data.get("width", Variant());
	ERR_FAIL_COND(maybe_width.get_type() != Variant::INT);

	const Variant maybe_depth = data.get("depth", Variant());
	ERR_FAIL_COND(maybe_depth.get_type() != Variant::INT);

	heights = maybe_heights;
	width = maybe_width;
	depth = maybe_depth;

	aabb = _calculate_aabb();

	destroy();
}

String JoltHeightMapShape3D::to_string() const {
	return vformat("{height_count=%d width=%d depth=%d}", heights.size(), width, depth);
}
