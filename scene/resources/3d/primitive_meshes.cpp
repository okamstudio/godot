/**************************************************************************/
/*  primitive_meshes.cpp                                                  */
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

#include "primitive_meshes.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "scene/resources/theme.h"
#include "scene/theme/theme_db.h"
#include "servers/rendering_server.h"
#include "thirdparty/misc/polypartition.h"

#define PADDING_REF_SIZE 1024.0

/**
  PrimitiveMesh
*/
void PrimitiveMesh::_update() const {
	Array arr;
	if (GDVIRTUAL_CALL(_create_mesh_array, arr)) {
		ERR_FAIL_COND_MSG(arr.size() != RS::ARRAY_MAX, "_create_mesh_array must return an array of Mesh.ARRAY_MAX elements.");
	} else {
		arr.resize(RS::ARRAY_MAX);
		_create_mesh_array(arr);
	}

	Vector<Vector3> points = arr[RS::ARRAY_VERTEX];

	ERR_FAIL_COND_MSG(points.is_empty(), "_create_mesh_array must return at least a vertex array.");

	aabb = AABB();

	int pc = points.size();
	ERR_FAIL_COND(pc == 0);
	{
		const Vector3 *r = points.ptr();
		for (int i = 0; i < pc; i++) {
			if (i == 0) {
				aabb.position = r[i];
			} else {
				aabb.expand_to(r[i]);
			}
		}
	}

	Vector<int> indices = arr[RS::ARRAY_INDEX];

	if (flip_faces) {
		Vector<Vector3> normals = arr[RS::ARRAY_NORMAL];

		if (normals.size() && indices.size()) {
			{
				int nc = normals.size();
				Vector3 *w = normals.ptrw();
				for (int i = 0; i < nc; i++) {
					w[i] = -w[i];
				}
			}

			{
				int ic = indices.size();
				int *w = indices.ptrw();
				for (int i = 0; i < ic; i += 3) {
					SWAP(w[i + 0], w[i + 1]);
				}
			}
			arr[RS::ARRAY_NORMAL] = normals;
			arr[RS::ARRAY_INDEX] = indices;
		}
	}

	if (add_uv2) {
		// _create_mesh_array should populate our UV2, this is a fallback in case it doesn't.
		// As we don't know anything about the geometry we only pad the right and bottom edge
		// of our texture.
		Vector<Vector2> uv = arr[RS::ARRAY_TEX_UV];
		Vector<Vector2> uv2 = arr[RS::ARRAY_TEX_UV2];

		if (uv.size() > 0 && uv2.size() == 0) {
			Vector2 uv2_scale = get_uv2_scale();
			uv2.resize(uv.size());

			Vector2 *uv2w = uv2.ptrw();
			for (int i = 0; i < uv.size(); i++) {
				uv2w[i] = uv[i] * uv2_scale;
			}
		}

		arr[RS::ARRAY_TEX_UV2] = uv2;
	}

	array_len = pc;
	index_array_len = indices.size();
	// in with the new
	RenderingServer::get_singleton()->mesh_clear(mesh);
	RenderingServer::get_singleton()->mesh_add_surface_from_arrays(mesh, (RenderingServer::PrimitiveType)primitive_type, arr);
	RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, material.is_null() ? RID() : material->get_rid());

	pending_request = false;

	clear_cache();

	const_cast<PrimitiveMesh *>(this)->emit_changed();
}

void PrimitiveMesh::request_update() {
	if (pending_request) {
		return;
	}
	_update();
}

int PrimitiveMesh::get_surface_count() const {
	if (pending_request) {
		_update();
	}
	return 1;
}

int PrimitiveMesh::surface_get_array_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, -1);
	if (pending_request) {
		_update();
	}

	return array_len;
}

int PrimitiveMesh::surface_get_array_index_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, -1);
	if (pending_request) {
		_update();
	}

	return index_array_len;
}

Array PrimitiveMesh::surface_get_arrays(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, 1, Array());
	if (pending_request) {
		_update();
	}

	return RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh, 0);
}

Dictionary PrimitiveMesh::surface_get_lods(int p_surface) const {
	return Dictionary(); //not really supported
}

TypedArray<Array> PrimitiveMesh::surface_get_blend_shape_arrays(int p_surface) const {
	return TypedArray<Array>(); //not really supported
}

BitField<Mesh::ArrayFormat> PrimitiveMesh::surface_get_format(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, 0);

	uint64_t mesh_format = RS::ARRAY_FORMAT_VERTEX | RS::ARRAY_FORMAT_NORMAL | RS::ARRAY_FORMAT_TANGENT | RS::ARRAY_FORMAT_TEX_UV | RS::ARRAY_FORMAT_INDEX;
	if (add_uv2) {
		mesh_format |= RS::ARRAY_FORMAT_TEX_UV2;
	}

	return mesh_format;
}

Mesh::PrimitiveType PrimitiveMesh::surface_get_primitive_type(int p_idx) const {
	return primitive_type;
}

void PrimitiveMesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {
	ERR_FAIL_INDEX(p_idx, 1);

	set_material(p_material);
}

Ref<Material> PrimitiveMesh::surface_get_material(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, 1, nullptr);

	return material;
}

int PrimitiveMesh::get_blend_shape_count() const {
	return 0;
}

StringName PrimitiveMesh::get_blend_shape_name(int p_index) const {
	return StringName();
}

void PrimitiveMesh::set_blend_shape_name(int p_index, const StringName &p_name) {
}

AABB PrimitiveMesh::get_aabb() const {
	if (pending_request) {
		_update();
	}

	return aabb;
}

RID PrimitiveMesh::get_rid() const {
	if (pending_request) {
		_update();
	}
	return mesh;
}

void PrimitiveMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material", "material"), &PrimitiveMesh::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &PrimitiveMesh::get_material);

	ClassDB::bind_method(D_METHOD("get_mesh_arrays"), &PrimitiveMesh::get_mesh_arrays);

	ClassDB::bind_method(D_METHOD("set_custom_aabb", "aabb"), &PrimitiveMesh::set_custom_aabb);
	ClassDB::bind_method(D_METHOD("get_custom_aabb"), &PrimitiveMesh::get_custom_aabb);

	ClassDB::bind_method(D_METHOD("set_flip_faces", "flip_faces"), &PrimitiveMesh::set_flip_faces);
	ClassDB::bind_method(D_METHOD("get_flip_faces"), &PrimitiveMesh::get_flip_faces);

	ClassDB::bind_method(D_METHOD("set_add_uv2", "add_uv2"), &PrimitiveMesh::set_add_uv2);
	ClassDB::bind_method(D_METHOD("get_add_uv2"), &PrimitiveMesh::get_add_uv2);

	ClassDB::bind_method(D_METHOD("set_uv2_padding", "uv2_padding"), &PrimitiveMesh::set_uv2_padding);
	ClassDB::bind_method(D_METHOD("get_uv2_padding"), &PrimitiveMesh::get_uv2_padding);

	ClassDB::bind_method(D_METHOD("request_update"), &PrimitiveMesh::request_update);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "custom_aabb", PROPERTY_HINT_NONE, "suffix:m"), "set_custom_aabb", "get_custom_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_faces"), "set_flip_faces", "get_flip_faces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "add_uv2"), "set_add_uv2", "get_add_uv2");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv2_padding", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_uv2_padding", "get_uv2_padding");

	GDVIRTUAL_BIND(_create_mesh_array);
}

void PrimitiveMesh::set_material(const Ref<Material> &p_material) {
	if (p_material == material) {
		return;
	}
	material = p_material;
	if (!pending_request) {
		// just apply it, else it'll happen when _update is called.
		RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, material.is_null() ? RID() : material->get_rid());
		notify_property_list_changed();
		emit_changed();
	}
}

Ref<Material> PrimitiveMesh::get_material() const {
	return material;
}

Array PrimitiveMesh::get_mesh_arrays() const {
	return surface_get_arrays(0);
}

void PrimitiveMesh::set_custom_aabb(const AABB &p_custom) {
	if (p_custom.is_equal_approx(custom_aabb)) {
		return;
	}
	custom_aabb = p_custom;
	RS::get_singleton()->mesh_set_custom_aabb(mesh, custom_aabb);
	emit_changed();
}

AABB PrimitiveMesh::get_custom_aabb() const {
	return custom_aabb;
}

void PrimitiveMesh::set_flip_faces(bool p_enable) {
	if (p_enable == flip_faces) {
		return;
	}
	flip_faces = p_enable;
	request_update();
}

bool PrimitiveMesh::get_flip_faces() const {
	return flip_faces;
}

void PrimitiveMesh::set_add_uv2(bool p_enable) {
	if (p_enable == add_uv2) {
		return;
	}
	add_uv2 = p_enable;
	_update_lightmap_size();
	request_update();
}

void PrimitiveMesh::set_uv2_padding(float p_padding) {
	if (Math::is_equal_approx(p_padding, uv2_padding)) {
		return;
	}
	uv2_padding = p_padding;
	_update_lightmap_size();
	request_update();
}

Vector2 PrimitiveMesh::get_uv2_scale(Vector2 p_margin_scale) const {
	Vector2 uv2_scale;
	Vector2 lightmap_size = get_lightmap_size_hint();

	// Calculate it as a margin, if no lightmap size hint is given we assume "PADDING_REF_SIZE" as our texture size.
	uv2_scale.x = p_margin_scale.x * uv2_padding / (lightmap_size.x == 0.0 ? PADDING_REF_SIZE : lightmap_size.x);
	uv2_scale.y = p_margin_scale.y * uv2_padding / (lightmap_size.y == 0.0 ? PADDING_REF_SIZE : lightmap_size.y);

	// Inverse it to turn our margin into a scale
	uv2_scale = Vector2(1.0, 1.0) - uv2_scale;

	return uv2_scale;
}

float PrimitiveMesh::get_lightmap_texel_size() const {
	return texel_size;
}

void PrimitiveMesh::_on_settings_changed() {
	float new_texel_size = float(GLOBAL_GET("rendering/lightmapping/primitive_meshes/texel_size"));
	if (new_texel_size <= 0.0) {
		new_texel_size = 0.2;
	}
	if (texel_size == new_texel_size) {
		return;
	}

	texel_size = new_texel_size;
	_update_lightmap_size();
	request_update();
}

PrimitiveMesh::PrimitiveMesh() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	mesh = RenderingServer::get_singleton()->mesh_create();

	ERR_FAIL_NULL(ProjectSettings::get_singleton());
	texel_size = float(GLOBAL_GET("rendering/lightmapping/primitive_meshes/texel_size"));
	if (texel_size <= 0.0) {
		texel_size = 0.2;
	}
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	project_settings->connect("settings_changed", callable_mp(this, &PrimitiveMesh::_on_settings_changed));
}

PrimitiveMesh::~PrimitiveMesh() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(mesh);

	ERR_FAIL_NULL(ProjectSettings::get_singleton());
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	project_settings->disconnect("settings_changed", callable_mp(this, &PrimitiveMesh::_on_settings_changed));
}

/**
	CapsuleMesh
*/

void CapsuleMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		float radial_length = radius * Math_PI * 0.5; // circumference of 90 degree bend
		float vertical_length = radial_length * 2 + (height - 2.0 * radius); // total vertical length

		_lightmap_size_hint.x = MAX(1.0, 4.0 * radial_length / texel_size) + padding;
		_lightmap_size_hint.y = MAX(1.0, vertical_length / texel_size) + padding;

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void CapsuleMesh::_create_mesh_array(Array &p_arr) const {
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	create_mesh_array(p_arr, radius, height, radial_segments, rings, _add_uv2, _uv2_padding);
}

void CapsuleMesh::create_mesh_array(Array &p_arr, const float radius, const float height, const int radial_segments, const int rings, bool p_add_uv2, const float p_uv2_padding) {
	int i, j, prevrow, thisrow, point;
	float x, y, z, u, v, w;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	// Only used if we calculate UV2
	float radial_width = 2.0 * radius * Math_PI;
	float radial_h = radial_width / (radial_width + p_uv2_padding);
	float radial_length = radius * Math_PI * 0.5; // circumference of 90 degree bend
	float vertical_length = radial_length * 2 + (height - 2.0 * radius) + p_uv2_padding; // total vertical length
	float radial_v = radial_length / vertical_length; // v size of top and bottom section
	float height_v = (height - 2.0 * radius) / vertical_length; // v size of height section

	// note, this has been aligned with our collision shape but I've left the descriptions as top/middle/bottom

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* top hemisphere */
	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;

		v /= (rings + 1);
		if (j == (rings + 1)) {
			w = 1.0;
			y = 0.0;
		} else {
			w = Math::sin(0.5 * Math_PI * v);
			y = Math::cos(0.5 * Math_PI * v);
		}

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = -Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			Vector3 p = Vector3(x * w, y, -z * w);
			points.push_back(p * radius + Vector3(0.0, 0.5 * height - radius, 0.0));
			normals.push_back(p);
			ADD_TANGENT(-z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, v * onethird));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u * radial_h, v * radial_v));
			}
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	/* cylinder */
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		v /= (rings + 1);

		y = (height - 2.0 * radius) * v;
		y = (0.5 * height - radius) - y;

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = -Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			Vector3 p = Vector3(x * radius, y, -z * radius);
			points.push_back(p);
			normals.push_back(Vector3(x, 0.0, -z));
			ADD_TANGENT(-z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, onethird + (v * onethird)));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u * radial_h, radial_v + (v * height_v)));
			}
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	/* bottom hemisphere */
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;

		v /= (rings + 1);
		if (j == (rings + 1)) {
			w = 0.0;
			y = -1.0;
		} else {
			w = Math::cos(0.5 * Math_PI * v);
			y = -Math::sin(0.5 * Math_PI * v);
		}

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = -Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			Vector3 p = Vector3(x * w, y, -z * w);
			points.push_back(p * radius + Vector3(0.0, -0.5 * height + radius, 0.0));
			normals.push_back(p);
			ADD_TANGENT(-z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, twothirds + v * onethird));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u * radial_h, radial_v + height_v + v * radial_v));
			}
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (p_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void CapsuleMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CapsuleMesh::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CapsuleMesh::get_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &CapsuleMesh::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CapsuleMesh::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "segments"), &CapsuleMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CapsuleMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &CapsuleMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &CapsuleMesh::get_rings);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_rings", "get_rings");

	ADD_LINKED_PROPERTY("radius", "height");
	ADD_LINKED_PROPERTY("height", "radius");
}

void CapsuleMesh::set_radius(const float p_radius) {
	if (Math::is_equal_approx(radius, p_radius)) {
		return;
	}

	radius = p_radius;
	if (radius > height * 0.5) {
		height = radius * 2.0;
	}
	_update_lightmap_size();
	request_update();
}

float CapsuleMesh::get_radius() const {
	return radius;
}

void CapsuleMesh::set_height(const float p_height) {
	if (Math::is_equal_approx(height, p_height)) {
		return;
	}

	height = p_height;
	if (radius > height * 0.5) {
		radius = height * 0.5;
	}
	_update_lightmap_size();
	request_update();
}

float CapsuleMesh::get_height() const {
	return height;
}

void CapsuleMesh::set_radial_segments(const int p_segments) {
	if (radial_segments == p_segments) {
		return;
	}

	radial_segments = p_segments > 4 ? p_segments : 4;
	request_update();
}

int CapsuleMesh::get_radial_segments() const {
	return radial_segments;
}

void CapsuleMesh::set_rings(const int p_rings) {
	if (rings == p_rings) {
		return;
	}

	ERR_FAIL_COND(p_rings < 0);
	rings = p_rings;
	request_update();
}

int CapsuleMesh::get_rings() const {
	return rings;
}

CapsuleMesh::CapsuleMesh() {}

/**
  BoxMesh
*/

void BoxMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		float width = (size.x + size.z) / texel_size;
		float length = (size.y + size.y + MAX(size.x, size.z)) / texel_size;

		_lightmap_size_hint.x = MAX(1.0, width) + 2.0 * padding;
		_lightmap_size_hint.y = MAX(1.0, length) + 3.0 * padding;

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void BoxMesh::_create_mesh_array(Array &p_arr) const {
	// Note about padding, with our box each face of the box faces a different direction so we want a seam
	// around every face. We thus add our padding to the right and bottom of each face.
	// With 3 faces along the width and 2 along the height of the texture we need to adjust our scale
	// accordingly.
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	BoxMesh::create_mesh_array(p_arr, size, subdivide_w, subdivide_h, subdivide_d, _add_uv2, _uv2_padding);
}

void BoxMesh::create_mesh_array(Array &p_arr, Vector3 size, int subdivide_w, int subdivide_h, int subdivide_d, bool p_add_uv2, const float p_uv2_padding) {
	int i, j, prevrow, thisrow, point;
	float x, y, z;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	// Only used if we calculate UV2
	// TODO this could be improved by changing the order depending on which side is the longest (basically the below works best if size.y is the longest)
	float total_h = (size.x + size.z + (2.0 * p_uv2_padding));
	float padding_h = p_uv2_padding / total_h;
	float width_h = size.x / total_h;
	float depth_h = size.z / total_h;
	float total_v = (size.y + size.y + MAX(size.x, size.z) + (3.0 * p_uv2_padding));
	float padding_v = p_uv2_padding / total_v;
	float width_v = size.x / total_v;
	float height_v = size.y / total_v;
	float depth_v = size.z / total_v;

	Vector3 start_pos = size * -0.5;

	// set our bounding box

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	// front + back
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= subdivide_h + 1; j++) {
		float v = j;
		float v2 = v / (subdivide_w + 1.0);
		v /= (2.0 * (subdivide_h + 1.0));

		x = start_pos.x;
		for (i = 0; i <= subdivide_w + 1; i++) {
			float u = i;
			float u2 = u / (subdivide_w + 1.0);
			u /= (3.0 * (subdivide_w + 1.0));

			// front
			points.push_back(Vector3(x, -y, -start_pos.z)); // double negative on the Z!
			normals.push_back(Vector3(0.0, 0.0, 1.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(u, v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u2 * width_h, v2 * height_v));
			}
			point++;

			// back
			points.push_back(Vector3(-x, -y, start_pos.z));
			normals.push_back(Vector3(0.0, 0.0, -1.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(twothirds + u, v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u2 * width_h, height_v + padding_v + (v2 * height_v)));
			}
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// front
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// back
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	// left + right
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		float v = j;
		float v2 = v / (subdivide_h + 1.0);
		v /= (2.0 * (subdivide_h + 1.0));

		z = start_pos.z;
		for (i = 0; i <= (subdivide_d + 1); i++) {
			float u = i;
			float u2 = u / (subdivide_d + 1.0);
			u /= (3.0 * (subdivide_d + 1.0));

			// right
			points.push_back(Vector3(-start_pos.x, -y, -z));
			normals.push_back(Vector3(1.0, 0.0, 0.0));
			ADD_TANGENT(0.0, 0.0, -1.0, 1.0);
			uvs.push_back(Vector2(onethird + u, v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(width_h + padding_h + (u2 * depth_h), v2 * height_v));
			}
			point++;

			// left
			points.push_back(Vector3(start_pos.x, -y, z));
			normals.push_back(Vector3(-1.0, 0.0, 0.0));
			ADD_TANGENT(0.0, 0.0, 1.0, 1.0);
			uvs.push_back(Vector2(u, 0.5 + v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(width_h + padding_h + (u2 * depth_h), height_v + padding_v + (v2 * height_v)));
			}
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// right
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// left
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			}

			z += size.z / (subdivide_d + 1.0);
		}

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	// top + bottom
	z = start_pos.z;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		float v = j;
		float v2 = v / (subdivide_d + 1.0);
		v /= (2.0 * (subdivide_d + 1.0));

		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float u2 = u / (subdivide_w + 1.0);
			u /= (3.0 * (subdivide_w + 1.0));

			// top
			points.push_back(Vector3(-x, -start_pos.y, -z));
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(onethird + u, 0.5 + v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(u2 * width_h, ((height_v + padding_v) * 2.0) + (v2 * depth_v)));
			}
			point++;

			// bottom
			points.push_back(Vector3(x, start_pos.y, -z));
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(twothirds + u, 0.5 + v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(width_h + padding_h + (u2 * depth_h), ((height_v + padding_v) * 2.0) + (v2 * width_v)));
			}
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				// top
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				// bottom
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.z / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (p_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void BoxMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &BoxMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &BoxMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "subdivide"), &BoxMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &BoxMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_height", "divisions"), &BoxMesh::set_subdivide_height);
	ClassDB::bind_method(D_METHOD("get_subdivide_height"), &BoxMesh::get_subdivide_height);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "divisions"), &BoxMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &BoxMesh::get_subdivide_depth);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_height", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_height", "get_subdivide_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
}

void BoxMesh::set_size(const Vector3 &p_size) {
	if (p_size.is_equal_approx(size)) {
		return;
	}

	size = p_size;
	_update_lightmap_size();
	request_update();
}

Vector3 BoxMesh::get_size() const {
	return size;
}

void BoxMesh::set_subdivide_width(const int p_divisions) {
	if (p_divisions == subdivide_w) {
		return;
	}

	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int BoxMesh::get_subdivide_width() const {
	return subdivide_w;
}

void BoxMesh::set_subdivide_height(const int p_divisions) {
	if (p_divisions == subdivide_h) {
		return;
	}

	subdivide_h = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int BoxMesh::get_subdivide_height() const {
	return subdivide_h;
}

void BoxMesh::set_subdivide_depth(const int p_divisions) {
	if (p_divisions == subdivide_d) {
		return;
	}

	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int BoxMesh::get_subdivide_depth() const {
	return subdivide_d;
}

BoxMesh::BoxMesh() {}

/**
	CylinderMesh
*/

void CylinderMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		float top_circumference = top_radius * Math_PI * 2.0;
		float bottom_circumference = bottom_radius * Math_PI * 2.0;

		float _width = MAX(top_circumference, bottom_circumference) / texel_size + padding;
		_width = MAX(_width, (((top_radius + bottom_radius) / texel_size) + padding) * 2.0); // this is extremely unlikely to be larger, will only happen if padding is larger then our diameter.
		_lightmap_size_hint.x = MAX(1.0, _width);

		float _height = ((height + (MAX(top_radius, bottom_radius) * 2.0)) / texel_size) + (2.0 * padding);

		_lightmap_size_hint.y = MAX(1.0, _height);

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void CylinderMesh::_create_mesh_array(Array &p_arr) const {
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	create_mesh_array(p_arr, top_radius, bottom_radius, height, radial_segments, rings, cap_top, cap_bottom, _add_uv2, _uv2_padding);
}

void CylinderMesh::create_mesh_array(Array &p_arr, float top_radius, float bottom_radius, float height, int radial_segments, int rings, bool cap_top, bool cap_bottom, bool p_add_uv2, const float p_uv2_padding) {
	int i, j, prevrow, thisrow, point;
	float x, y, z, u, v, radius, radius_h;

	// Only used if we calculate UV2
	float top_circumference = top_radius * Math_PI * 2.0;
	float bottom_circumference = bottom_radius * Math_PI * 2.0;
	float vertical_length = height + MAX(2.0 * top_radius, 2.0 * bottom_radius) + (2.0 * p_uv2_padding);
	float height_v = height / vertical_length;
	float padding_v = p_uv2_padding / vertical_length;

	float horizontal_length = MAX(MAX(2.0 * (top_radius + bottom_radius + p_uv2_padding), top_circumference + p_uv2_padding), bottom_circumference + p_uv2_padding);
	float center_h = 0.5 * (horizontal_length - p_uv2_padding) / horizontal_length;
	float top_h = top_circumference / horizontal_length;
	float bottom_h = bottom_circumference / horizontal_length;
	float padding_h = p_uv2_padding / horizontal_length;

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	thisrow = 0;
	prevrow = 0;
	const real_t side_normal_y = (bottom_radius - top_radius) / height;
	for (j = 0; j <= (rings + 1); j++) {
		v = j;
		v /= (rings + 1);

		radius = top_radius + ((bottom_radius - top_radius) * v);
		radius_h = top_h + ((bottom_h - top_h) * v);

		y = height * v;
		y = (height * 0.5) - y;

		for (i = 0; i <= radial_segments; i++) {
			u = i;
			u /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			Vector3 p = Vector3(x * radius, y, z * radius);
			points.push_back(p);
			normals.push_back(Vector3(x, side_normal_y, z).normalized());
			ADD_TANGENT(z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, v * 0.5));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(center_h + (u - 0.5) * radius_h, v * height_v));
			}
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	// Adjust for bottom section, only used if we calculate UV2s.
	top_h = top_radius / horizontal_length;
	float top_v = top_radius / vertical_length;
	bottom_h = bottom_radius / horizontal_length;
	float bottom_v = bottom_radius / vertical_length;

	// Add top.
	if (cap_top && top_radius > 0.0) {
		y = height * 0.5;

		thisrow = point;
		points.push_back(Vector3(0.0, y, 0.0));
		normals.push_back(Vector3(0.0, 1.0, 0.0));
		ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
		uvs.push_back(Vector2(0.25, 0.75));
		if (p_add_uv2) {
			uv2s.push_back(Vector2(top_h, height_v + padding_v + MAX(top_v, bottom_v)));
		}
		point++;

		for (i = 0; i <= radial_segments; i++) {
			float r = i;
			r /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = Math::sin(r * Math_TAU);
				z = Math::cos(r * Math_TAU);
			}

			u = ((x + 1.0) * 0.25);
			v = 0.5 + ((z + 1.0) * 0.25);

			Vector3 p = Vector3(x * top_radius, y, z * top_radius);
			points.push_back(p);
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
			uvs.push_back(Vector2(u, v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(top_h + (x * top_h), height_v + padding_v + MAX(top_v, bottom_v) + (z * top_v)));
			}
			point++;

			if (i > 0) {
				indices.push_back(thisrow);
				indices.push_back(point - 1);
				indices.push_back(point - 2);
			}
		}
	}

	// Add bottom.
	if (cap_bottom && bottom_radius > 0.0) {
		y = height * -0.5;

		thisrow = point;
		points.push_back(Vector3(0.0, y, 0.0));
		normals.push_back(Vector3(0.0, -1.0, 0.0));
		ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
		uvs.push_back(Vector2(0.75, 0.75));
		if (p_add_uv2) {
			uv2s.push_back(Vector2(top_h + top_h + padding_h + bottom_h, height_v + padding_v + MAX(top_v, bottom_v)));
		}
		point++;

		for (i = 0; i <= radial_segments; i++) {
			float r = i;
			r /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = Math::sin(r * Math_TAU);
				z = Math::cos(r * Math_TAU);
			}

			u = 0.5 + ((x + 1.0) * 0.25);
			v = 1.0 - ((z + 1.0) * 0.25);

			Vector3 p = Vector3(x * bottom_radius, y, z * bottom_radius);
			points.push_back(p);
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
			uvs.push_back(Vector2(u, v));
			if (p_add_uv2) {
				uv2s.push_back(Vector2(top_h + top_h + padding_h + bottom_h + (x * bottom_h), height_v + padding_v + MAX(top_v, bottom_v) - (z * bottom_v)));
			}
			point++;

			if (i > 0) {
				indices.push_back(thisrow);
				indices.push_back(point - 2);
				indices.push_back(point - 1);
			}
		}
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (p_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void CylinderMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_top_radius", "radius"), &CylinderMesh::set_top_radius);
	ClassDB::bind_method(D_METHOD("get_top_radius"), &CylinderMesh::get_top_radius);
	ClassDB::bind_method(D_METHOD("set_bottom_radius", "radius"), &CylinderMesh::set_bottom_radius);
	ClassDB::bind_method(D_METHOD("get_bottom_radius"), &CylinderMesh::get_bottom_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &CylinderMesh::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CylinderMesh::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "segments"), &CylinderMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CylinderMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &CylinderMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &CylinderMesh::get_rings);

	ClassDB::bind_method(D_METHOD("set_cap_top", "cap_top"), &CylinderMesh::set_cap_top);
	ClassDB::bind_method(D_METHOD("is_cap_top"), &CylinderMesh::is_cap_top);

	ClassDB::bind_method(D_METHOD("set_cap_bottom", "cap_bottom"), &CylinderMesh::set_cap_bottom);
	ClassDB::bind_method(D_METHOD("is_cap_bottom"), &CylinderMesh::is_cap_bottom);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "top_radius", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater,suffix:m"), "set_top_radius", "get_top_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bottom_radius", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater,suffix:m"), "set_bottom_radius", "get_bottom_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_rings", "get_rings");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cap_top"), "set_cap_top", "is_cap_top");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cap_bottom"), "set_cap_bottom", "is_cap_bottom");
}

void CylinderMesh::set_top_radius(const float p_radius) {
	if (Math::is_equal_approx(p_radius, top_radius)) {
		return;
	}

	top_radius = p_radius;
	_update_lightmap_size();
	request_update();
}

float CylinderMesh::get_top_radius() const {
	return top_radius;
}

void CylinderMesh::set_bottom_radius(const float p_radius) {
	if (Math::is_equal_approx(p_radius, bottom_radius)) {
		return;
	}

	bottom_radius = p_radius;
	_update_lightmap_size();
	request_update();
}

float CylinderMesh::get_bottom_radius() const {
	return bottom_radius;
}

void CylinderMesh::set_height(const float p_height) {
	if (Math::is_equal_approx(p_height, height)) {
		return;
	}

	height = p_height;
	_update_lightmap_size();
	request_update();
}

float CylinderMesh::get_height() const {
	return height;
}

void CylinderMesh::set_radial_segments(const int p_segments) {
	if (p_segments == radial_segments) {
		return;
	}

	radial_segments = p_segments > 4 ? p_segments : 4;
	request_update();
}

int CylinderMesh::get_radial_segments() const {
	return radial_segments;
}

void CylinderMesh::set_rings(const int p_rings) {
	if (p_rings == rings) {
		return;
	}

	ERR_FAIL_COND(p_rings < 0);
	rings = p_rings;
	request_update();
}

int CylinderMesh::get_rings() const {
	return rings;
}

void CylinderMesh::set_cap_top(bool p_cap_top) {
	if (p_cap_top == cap_top) {
		return;
	}

	cap_top = p_cap_top;
	request_update();
}

bool CylinderMesh::is_cap_top() const {
	return cap_top;
}

void CylinderMesh::set_cap_bottom(bool p_cap_bottom) {
	if (p_cap_bottom == cap_bottom) {
		return;
	}

	cap_bottom = p_cap_bottom;
	request_update();
}

bool CylinderMesh::is_cap_bottom() const {
	return cap_bottom;
}

CylinderMesh::CylinderMesh() {}

/**
  PlaneMesh
*/

void PlaneMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		_lightmap_size_hint.x = MAX(1.0, (size.x / texel_size) + padding);
		_lightmap_size_hint.y = MAX(1.0, (size.y / texel_size) + padding);

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void PlaneMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, z;

	// Plane mesh can use default UV2 calculation as implemented in Primitive Mesh

	Size2 start_pos = size * -0.5;

	Vector3 normal = Vector3(0.0, 1.0, 0.0);
	if (orientation == FACE_X) {
		normal = Vector3(1.0, 0.0, 0.0);
	} else if (orientation == FACE_Z) {
		normal = Vector3(0.0, 0.0, 1.0);
	}

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* top + bottom */
	z = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float v = j;
			u /= (subdivide_w + 1.0);
			v /= (subdivide_d + 1.0);

			if (orientation == FACE_X) {
				points.push_back(Vector3(0.0, z, x) + center_offset);
			} else if (orientation == FACE_Y) {
				points.push_back(Vector3(-x, 0.0, -z) + center_offset);
			} else if (orientation == FACE_Z) {
				points.push_back(Vector3(-x, z, 0.0) + center_offset);
			}
			normals.push_back(normal);
			if (orientation == FACE_X) {
				ADD_TANGENT(0.0, 0.0, -1.0, 1.0);
			} else {
				ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
			}
			uvs.push_back(Vector2(1.0 - u, 1.0 - v)); /* 1.0 - uv to match orientation with Quad */
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.y / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_INDEX] = indices;
}

void PlaneMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PlaneMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PlaneMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "subdivide"), &PlaneMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &PlaneMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "subdivide"), &PlaneMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &PlaneMesh::get_subdivide_depth);

	ClassDB::bind_method(D_METHOD("set_center_offset", "offset"), &PlaneMesh::set_center_offset);
	ClassDB::bind_method(D_METHOD("get_center_offset"), &PlaneMesh::get_center_offset);

	ClassDB::bind_method(D_METHOD("set_orientation", "orientation"), &PlaneMesh::set_orientation);
	ClassDB::bind_method(D_METHOD("get_orientation"), &PlaneMesh::get_orientation);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_center_offset", "get_center_offset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "orientation", PROPERTY_HINT_ENUM, "Face X,Face Y,Face Z"), "set_orientation", "get_orientation");

	BIND_ENUM_CONSTANT(FACE_X)
	BIND_ENUM_CONSTANT(FACE_Y)
	BIND_ENUM_CONSTANT(FACE_Z)
}

void PlaneMesh::set_size(const Size2 &p_size) {
	if (p_size == size) {
		return;
	}
	size = p_size;
	_update_lightmap_size();
	request_update();
}

Size2 PlaneMesh::get_size() const {
	return size;
}

void PlaneMesh::set_subdivide_width(const int p_divisions) {
	if (p_divisions == subdivide_w || (subdivide_w == 0 && p_divisions < 0)) {
		return;
	}
	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int PlaneMesh::get_subdivide_width() const {
	return subdivide_w;
}

void PlaneMesh::set_subdivide_depth(const int p_divisions) {
	if (p_divisions == subdivide_d || (subdivide_d == 0 && p_divisions < 0)) {
		return;
	}
	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int PlaneMesh::get_subdivide_depth() const {
	return subdivide_d;
}

void PlaneMesh::set_center_offset(const Vector3 p_offset) {
	if (p_offset.is_equal_approx(center_offset)) {
		return;
	}
	center_offset = p_offset;
	request_update();
}

Vector3 PlaneMesh::get_center_offset() const {
	return center_offset;
}

void PlaneMesh::set_orientation(const Orientation p_orientation) {
	if (p_orientation == orientation) {
		return;
	}
	orientation = p_orientation;
	request_update();
}

PlaneMesh::Orientation PlaneMesh::get_orientation() const {
	return orientation;
}

PlaneMesh::PlaneMesh() {}

/**
  PrismMesh
*/

void PrismMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		// left_to_right does not effect the surface area of the prism so we ignore that.
		// TODO we could combine the two triangles and save some space but we need to re-align the uv1 and adjust the tangent.

		float width = (size.x + size.z) / texel_size;
		float length = (size.y + size.y + size.z) / texel_size;

		_lightmap_size_hint.x = MAX(1.0, width) + 2.0 * padding;
		_lightmap_size_hint.y = MAX(1.0, length) + 3.0 * padding;

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void PrismMesh::_create_mesh_array(Array &p_arr) const {
	int i, j, prevrow, thisrow, point;
	float x, y, z;
	float onethird = 1.0 / 3.0;
	float twothirds = 2.0 / 3.0;

	// Only used if we calculate UV2
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	float horizontal_total = size.x + size.z + 2.0 * _uv2_padding;
	float width_h = size.x / horizontal_total;
	float depth_h = size.z / horizontal_total;
	float padding_h = _uv2_padding / horizontal_total;

	float vertical_total = (size.y + size.y + size.z) + (3.0 * _uv2_padding);
	float height_v = size.y / vertical_total;
	float depth_v = size.z / vertical_total;
	float padding_v = _uv2_padding / vertical_total;

	// and start building

	Vector3 start_pos = size * -0.5;

	// set our bounding box

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	/* front + back */
	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		float scale = j / (subdivide_h + 1.0);
		float scaled_size_x = size.x * scale;
		float start_x = start_pos.x + (1.0 - scale) * size.x * left_to_right;
		float offset_front = (1.0 - scale) * onethird * left_to_right;
		float offset_back = (1.0 - scale) * onethird * (1.0 - left_to_right);

		float v = j;
		float v2 = scale;
		v /= 2.0 * (subdivide_h + 1.0);

		x = 0.0;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float u2 = i / (subdivide_w + 1.0);
			u /= (3.0 * (subdivide_w + 1.0));

			u *= scale;

			/* front */
			points.push_back(Vector3(start_x + x, -y, -start_pos.z)); // double negative on the Z!
			normals.push_back(Vector3(0.0, 0.0, 1.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(offset_front + u, v));
			if (_add_uv2) {
				uv2s.push_back(Vector2(u2 * scale * width_h, v2 * height_v));
			}
			point++;

			/* back */
			points.push_back(Vector3(start_x + scaled_size_x - x, -y, start_pos.z));
			normals.push_back(Vector3(0.0, 0.0, -1.0));
			ADD_TANGENT(-1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(twothirds + offset_back + u, v));
			if (_add_uv2) {
				uv2s.push_back(Vector2(u2 * scale * width_h, height_v + padding_v + v2 * height_v));
			}
			point++;

			if (i > 0 && j == 1) {
				int i2 = i * 2;

				/* front */
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* back */
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			} else if (i > 0 && j > 0) {
				int i2 = i * 2;

				/* front */
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* back */
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			}

			x += scale * size.x / (subdivide_w + 1.0);
		}

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	/* left + right */
	Vector3 normal_left, normal_right;

	normal_left = Vector3(-size.y, size.x * left_to_right, 0.0);
	normal_right = Vector3(size.y, size.x * (1.0 - left_to_right), 0.0);
	normal_left.normalize();
	normal_right.normalize();

	y = start_pos.y;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_h + 1); j++) {
		float left, right;
		float scale = j / (subdivide_h + 1.0);

		left = start_pos.x + (size.x * (1.0 - scale) * left_to_right);
		right = left + (size.x * scale);

		float v = j;
		float v2 = scale;
		v /= 2.0 * (subdivide_h + 1.0);

		z = start_pos.z;
		for (i = 0; i <= (subdivide_d + 1); i++) {
			float u = i;
			float u2 = u / (subdivide_d + 1.0);
			u /= (3.0 * (subdivide_d + 1.0));

			/* right */
			points.push_back(Vector3(right, -y, -z));
			normals.push_back(normal_right);
			ADD_TANGENT(0.0, 0.0, -1.0, 1.0);
			uvs.push_back(Vector2(onethird + u, v));
			if (_add_uv2) {
				uv2s.push_back(Vector2(width_h + padding_h + u2 * depth_h, v2 * height_v));
			}
			point++;

			/* left */
			points.push_back(Vector3(left, -y, z));
			normals.push_back(normal_left);
			ADD_TANGENT(0.0, 0.0, 1.0, 1.0);
			uvs.push_back(Vector2(u, 0.5 + v));
			if (_add_uv2) {
				uv2s.push_back(Vector2(width_h + padding_h + u2 * depth_h, height_v + padding_v + v2 * height_v));
			}
			point++;

			if (i > 0 && j > 0) {
				int i2 = i * 2;

				/* right */
				indices.push_back(prevrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2 - 2);
				indices.push_back(prevrow + i2);
				indices.push_back(thisrow + i2);
				indices.push_back(thisrow + i2 - 2);

				/* left */
				indices.push_back(prevrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
				indices.push_back(prevrow + i2 + 1);
				indices.push_back(thisrow + i2 + 1);
				indices.push_back(thisrow + i2 - 1);
			}

			z += size.z / (subdivide_d + 1.0);
		}

		y += size.y / (subdivide_h + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	/* bottom */
	z = start_pos.z;
	thisrow = point;
	prevrow = 0;
	for (j = 0; j <= (subdivide_d + 1); j++) {
		float v = j;
		float v2 = v / (subdivide_d + 1.0);
		v /= (2.0 * (subdivide_d + 1.0));

		x = start_pos.x;
		for (i = 0; i <= (subdivide_w + 1); i++) {
			float u = i;
			float u2 = u / (subdivide_w + 1.0);
			u /= (3.0 * (subdivide_w + 1.0));

			/* bottom */
			points.push_back(Vector3(x, start_pos.y, -z));
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0);
			uvs.push_back(Vector2(twothirds + u, 0.5 + v));
			if (_add_uv2) {
				uv2s.push_back(Vector2(u2 * width_h, 2.0 * (height_v + padding_v) + v2 * depth_v));
			}
			point++;

			if (i > 0 && j > 0) {
				/* bottom */
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}

			x += size.x / (subdivide_w + 1.0);
		}

		z += size.z / (subdivide_d + 1.0);
		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void PrismMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_left_to_right", "left_to_right"), &PrismMesh::set_left_to_right);
	ClassDB::bind_method(D_METHOD("get_left_to_right"), &PrismMesh::get_left_to_right);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &PrismMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PrismMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_subdivide_width", "segments"), &PrismMesh::set_subdivide_width);
	ClassDB::bind_method(D_METHOD("get_subdivide_width"), &PrismMesh::get_subdivide_width);
	ClassDB::bind_method(D_METHOD("set_subdivide_height", "segments"), &PrismMesh::set_subdivide_height);
	ClassDB::bind_method(D_METHOD("get_subdivide_height"), &PrismMesh::get_subdivide_height);
	ClassDB::bind_method(D_METHOD("set_subdivide_depth", "segments"), &PrismMesh::set_subdivide_depth);
	ClassDB::bind_method(D_METHOD("get_subdivide_depth"), &PrismMesh::get_subdivide_depth);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "left_to_right", PROPERTY_HINT_RANGE, "-2.0,2.0,0.1"), "set_left_to_right", "get_left_to_right");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_width", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_width", "get_subdivide_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_height", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_height", "get_subdivide_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivide_depth", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_subdivide_depth", "get_subdivide_depth");
}

void PrismMesh::set_left_to_right(const float p_left_to_right) {
	if (Math::is_equal_approx(p_left_to_right, left_to_right)) {
		return;
	}
	left_to_right = p_left_to_right;
	request_update();
}

float PrismMesh::get_left_to_right() const {
	return left_to_right;
}

void PrismMesh::set_size(const Vector3 &p_size) {
	if (p_size.is_equal_approx(size)) {
		return;
	}
	size = p_size;
	_update_lightmap_size();
	request_update();
}

Vector3 PrismMesh::get_size() const {
	return size;
}

void PrismMesh::set_subdivide_width(const int p_divisions) {
	if (p_divisions == subdivide_w || (p_divisions < 0 && subdivide_w == 0)) {
		return;
	}
	subdivide_w = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int PrismMesh::get_subdivide_width() const {
	return subdivide_w;
}

void PrismMesh::set_subdivide_height(const int p_divisions) {
	if (p_divisions == subdivide_h || (p_divisions < 0 && subdivide_h == 0)) {
		return;
	}
	subdivide_h = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int PrismMesh::get_subdivide_height() const {
	return subdivide_h;
}

void PrismMesh::set_subdivide_depth(const int p_divisions) {
	if (p_divisions == subdivide_d || (p_divisions < 0 && subdivide_d == 0)) {
		return;
	}
	subdivide_d = p_divisions > 0 ? p_divisions : 0;
	request_update();
}

int PrismMesh::get_subdivide_depth() const {
	return subdivide_d;
}

PrismMesh::PrismMesh() {}

/**
  SphereMesh
*/

void SphereMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		float _width = radius * Math_TAU;
		_lightmap_size_hint.x = MAX(1.0, (_width / texel_size) + padding);
		float _height = (is_hemisphere ? 1.0 : 0.5) * height * Math_PI; // note, with hemisphere height is our radius, while with a full sphere it is the diameter..
		_lightmap_size_hint.y = MAX(1.0, (_height / texel_size) + padding);

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void SphereMesh::_create_mesh_array(Array &p_arr) const {
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	create_mesh_array(p_arr, radius, height, radial_segments, rings, is_hemisphere, _add_uv2, _uv2_padding);
}

void SphereMesh::create_mesh_array(Array &p_arr, float radius, float height, int radial_segments, int rings, bool is_hemisphere, bool p_add_uv2, const float p_uv2_padding) {
	int i, j, prevrow, thisrow, point;
	float x, y, z;

	float scale = height / radius * (is_hemisphere ? 1.0 : 0.5);

	// Only used if we calculate UV2
	float circumference = radius * Math_TAU;
	float horizontal_length = circumference + p_uv2_padding;
	float center_h = 0.5 * circumference / horizontal_length;

	float height_v = scale * Math_PI / ((scale * Math_PI) + p_uv2_padding / radius);

	// set our bounding box

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;
	point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		float v = j;
		float w;

		v /= (rings + 1);
		if (j == (rings + 1)) {
			w = 0.0;
			y = -1.0;
		} else {
			w = Math::sin(Math_PI * v);
			y = Math::cos(Math_PI * v);
		}

		for (i = 0; i <= radial_segments; i++) {
			float u = i;
			u /= radial_segments;

			if (i == radial_segments) {
				x = 0.0;
				z = 1.0;
			} else {
				x = Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			if (is_hemisphere && y < 0.0) {
				points.push_back(Vector3(x * radius * w, 0.0, z * radius * w));
				normals.push_back(Vector3(0.0, -1.0, 0.0));
			} else {
				Vector3 p = Vector3(x * w, y * scale, z * w);
				points.push_back(p * radius);
				Vector3 normal = Vector3(x * w * scale, y, z * w * scale);
				normals.push_back(normal.normalized());
			}
			ADD_TANGENT(z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, v));
			if (p_add_uv2) {
				float w_h = w * 2.0 * center_h;
				uv2s.push_back(Vector2(center_h + ((u - 0.5) * w_h), v * height_v));
			}
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (p_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void SphereMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &SphereMesh::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &SphereMesh::get_radius);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &SphereMesh::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &SphereMesh::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "radial_segments"), &SphereMesh::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &SphereMesh::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &SphereMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &SphereMesh::get_rings);

	ClassDB::bind_method(D_METHOD("set_is_hemisphere", "is_hemisphere"), &SphereMesh::set_is_hemisphere);
	ClassDB::bind_method(D_METHOD("get_is_hemisphere"), &SphereMesh::get_is_hemisphere);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "1,100,1,or_greater"), "set_rings", "get_rings");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_hemisphere"), "set_is_hemisphere", "get_is_hemisphere");
}

void SphereMesh::set_radius(const float p_radius) {
	if (Math::is_equal_approx(p_radius, radius)) {
		return;
	}
	radius = p_radius;
	_update_lightmap_size();
	request_update();
}

float SphereMesh::get_radius() const {
	return radius;
}

void SphereMesh::set_height(const float p_height) {
	if (Math::is_equal_approx(height, p_height)) {
		return;
	}
	height = p_height;
	_update_lightmap_size();
	request_update();
}

float SphereMesh::get_height() const {
	return height;
}

void SphereMesh::set_radial_segments(const int p_radial_segments) {
	if (p_radial_segments == radial_segments || (radial_segments == 4 && p_radial_segments < 4)) {
		return;
	}
	radial_segments = p_radial_segments > 4 ? p_radial_segments : 4;
	request_update();
}

int SphereMesh::get_radial_segments() const {
	return radial_segments;
}

void SphereMesh::set_rings(const int p_rings) {
	if (p_rings == rings) {
		return;
	}
	ERR_FAIL_COND(p_rings < 1);
	rings = p_rings;
	request_update();
}

int SphereMesh::get_rings() const {
	return rings;
}

void SphereMesh::set_is_hemisphere(const bool p_is_hemisphere) {
	if (p_is_hemisphere == is_hemisphere) {
		return;
	}
	is_hemisphere = p_is_hemisphere;
	_update_lightmap_size();
	request_update();
}

bool SphereMesh::get_is_hemisphere() const {
	return is_hemisphere;
}

SphereMesh::SphereMesh() {}

/**
  TorusMesh
*/

void TorusMesh::_update_lightmap_size() {
	if (get_add_uv2()) {
		// size must have changed, update lightmap size hint
		Size2i _lightmap_size_hint;
		float padding = get_uv2_padding();

		float min_radius = inner_radius;
		float max_radius = outer_radius;

		if (min_radius > max_radius) {
			SWAP(min_radius, max_radius);
		}

		float radius = (max_radius - min_radius) * 0.5;

		float _width = max_radius * Math_TAU;
		_lightmap_size_hint.x = MAX(1.0, (_width / texel_size) + padding);
		float _height = radius * Math_TAU;
		_lightmap_size_hint.y = MAX(1.0, (_height / texel_size) + padding);

		set_lightmap_size_hint(_lightmap_size_hint);
	}
}

void TorusMesh::_create_mesh_array(Array &p_arr) const {
	// set our bounding box

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<Vector2> uv2s;
	Vector<int> indices;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	ERR_FAIL_COND_MSG(inner_radius == outer_radius, "Inner radius and outer radius cannot be the same.");

	float min_radius = inner_radius;
	float max_radius = outer_radius;

	if (min_radius > max_radius) {
		SWAP(min_radius, max_radius);
	}

	float radius = (max_radius - min_radius) * 0.5;

	// Only used if we calculate UV2
	bool _add_uv2 = get_add_uv2();
	float _uv2_padding = get_uv2_padding() * texel_size;

	float horizontal_total = max_radius * Math_TAU + _uv2_padding;
	float max_h = max_radius * Math_TAU / horizontal_total;
	float delta_h = (max_radius - min_radius) * Math_TAU / horizontal_total;

	float height_v = radius * Math_TAU / (radius * Math_TAU + _uv2_padding);

	for (int i = 0; i <= rings; i++) {
		int prevrow = (i - 1) * (ring_segments + 1);
		int thisrow = i * (ring_segments + 1);
		float inci = float(i) / rings;
		float angi = inci * Math_TAU;

		Vector2 normali = (i == rings) ? Vector2(0.0, -1.0) : Vector2(-Math::sin(angi), -Math::cos(angi));

		for (int j = 0; j <= ring_segments; j++) {
			float incj = float(j) / ring_segments;
			float angj = incj * Math_TAU;

			Vector2 normalj = (j == ring_segments) ? Vector2(-1.0, 0.0) : Vector2(-Math::cos(angj), Math::sin(angj));
			Vector2 normalk = normalj * radius + Vector2(min_radius + radius, 0);

			float offset_h = 0.5 * (1.0 - normalj.x) * delta_h;
			float adj_h = max_h - offset_h;
			offset_h *= 0.5;

			points.push_back(Vector3(normali.x * normalk.x, normalk.y, normali.y * normalk.x));
			normals.push_back(Vector3(normali.x * normalj.x, normalj.y, normali.y * normalj.x));
			ADD_TANGENT(normali.y, 0.0, -normali.x, 1.0);
			uvs.push_back(Vector2(inci, incj));
			if (_add_uv2) {
				uv2s.push_back(Vector2(offset_h + inci * adj_h, incj * height_v));
			}

			if (i > 0 && j > 0) {
				indices.push_back(thisrow + j - 1);
				indices.push_back(prevrow + j);
				indices.push_back(prevrow + j - 1);

				indices.push_back(thisrow + j - 1);
				indices.push_back(thisrow + j);
				indices.push_back(prevrow + j);
			}
		}
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	if (_add_uv2) {
		p_arr[RS::ARRAY_TEX_UV2] = uv2s;
	}
	p_arr[RS::ARRAY_INDEX] = indices;
}

void TorusMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_inner_radius", "radius"), &TorusMesh::set_inner_radius);
	ClassDB::bind_method(D_METHOD("get_inner_radius"), &TorusMesh::get_inner_radius);

	ClassDB::bind_method(D_METHOD("set_outer_radius", "radius"), &TorusMesh::set_outer_radius);
	ClassDB::bind_method(D_METHOD("get_outer_radius"), &TorusMesh::get_outer_radius);

	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &TorusMesh::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &TorusMesh::get_rings);

	ClassDB::bind_method(D_METHOD("set_ring_segments", "rings"), &TorusMesh::set_ring_segments);
	ClassDB::bind_method(D_METHOD("get_ring_segments"), &TorusMesh::get_ring_segments);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inner_radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp"), "set_inner_radius", "get_inner_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "outer_radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp"), "set_outer_radius", "get_outer_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "3,128,1,or_greater"), "set_rings", "get_rings");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ring_segments", PROPERTY_HINT_RANGE, "3,64,1,or_greater"), "set_ring_segments", "get_ring_segments");
}

void TorusMesh::set_inner_radius(const float p_inner_radius) {
	if (Math::is_equal_approx(p_inner_radius, inner_radius)) {
		return;
	}
	inner_radius = p_inner_radius;
	request_update();
}

float TorusMesh::get_inner_radius() const {
	return inner_radius;
}

void TorusMesh::set_outer_radius(const float p_outer_radius) {
	if (Math::is_equal_approx(p_outer_radius, outer_radius)) {
		return;
	}
	outer_radius = p_outer_radius;
	request_update();
}

float TorusMesh::get_outer_radius() const {
	return outer_radius;
}

void TorusMesh::set_rings(const int p_rings) {
	if (p_rings == rings) {
		return;
	}
	ERR_FAIL_COND(p_rings < 3);
	rings = p_rings;
	request_update();
}

int TorusMesh::get_rings() const {
	return rings;
}

void TorusMesh::set_ring_segments(const int p_ring_segments) {
	if (p_ring_segments == ring_segments) {
		return;
	}
	ERR_FAIL_COND(p_ring_segments < 3);
	ring_segments = p_ring_segments;
	request_update();
}

int TorusMesh::get_ring_segments() const {
	return ring_segments;
}

TorusMesh::TorusMesh() {}

/**
  PointMesh
*/

void PointMesh::_create_mesh_array(Array &p_arr) const {
	Vector<Vector3> faces;
	faces.resize(1);
	faces.set(0, Vector3(0.0, 0.0, 0.0));

	p_arr[RS::ARRAY_VERTEX] = faces;
}

PointMesh::PointMesh() {
	primitive_type = PRIMITIVE_POINTS;
}
// TUBE TRAIL

void TubeTrailMesh::set_radius(const float p_radius) {
	if (Math::is_equal_approx(p_radius, radius)) {
		return;
	}
	radius = p_radius;
	request_update();
}
float TubeTrailMesh::get_radius() const {
	return radius;
}

void TubeTrailMesh::set_radial_steps(const int p_radial_steps) {
	if (p_radial_steps == radial_steps) {
		return;
	}
	ERR_FAIL_COND(p_radial_steps < 3 || p_radial_steps > 128);
	radial_steps = p_radial_steps;
	request_update();
}
int TubeTrailMesh::get_radial_steps() const {
	return radial_steps;
}

void TubeTrailMesh::set_sections(const int p_sections) {
	if (p_sections == sections) {
		return;
	}
	ERR_FAIL_COND(p_sections < 2 || p_sections > 128);
	sections = p_sections;
	request_update();
}
int TubeTrailMesh::get_sections() const {
	return sections;
}

void TubeTrailMesh::set_section_length(float p_section_length) {
	if (p_section_length == section_length) {
		return;
	}
	section_length = p_section_length;
	request_update();
}
float TubeTrailMesh::get_section_length() const {
	return section_length;
}

void TubeTrailMesh::set_section_rings(const int p_section_rings) {
	if (p_section_rings == section_rings) {
		return;
	}
	ERR_FAIL_COND(p_section_rings < 1 || p_section_rings > 1024);
	section_rings = p_section_rings;
	request_update();
}
int TubeTrailMesh::get_section_rings() const {
	return section_rings;
}

void TubeTrailMesh::set_cap_top(bool p_cap_top) {
	if (p_cap_top == cap_top) {
		return;
	}
	cap_top = p_cap_top;
	request_update();
}

bool TubeTrailMesh::is_cap_top() const {
	return cap_top;
}

void TubeTrailMesh::set_cap_bottom(bool p_cap_bottom) {
	if (p_cap_bottom == cap_bottom) {
		return;
	}
	cap_bottom = p_cap_bottom;
	request_update();
}

bool TubeTrailMesh::is_cap_bottom() const {
	return cap_bottom;
}

void TubeTrailMesh::set_curve(const Ref<Curve> &p_curve) {
	if (curve == p_curve) {
		return;
	}
	if (curve.is_valid()) {
		curve->disconnect_changed(callable_mp(this, &TubeTrailMesh::_curve_changed));
	}
	curve = p_curve;
	if (curve.is_valid()) {
		curve->connect_changed(callable_mp(this, &TubeTrailMesh::_curve_changed));
	}
	request_update();
}
Ref<Curve> TubeTrailMesh::get_curve() const {
	return curve;
}

void TubeTrailMesh::_curve_changed() {
	request_update();
}
int TubeTrailMesh::get_builtin_bind_pose_count() const {
	return sections + 1;
}

Transform3D TubeTrailMesh::get_builtin_bind_pose(int p_index) const {
	float depth = section_length * sections;

	Transform3D xform;
	xform.origin.y = depth / 2.0 - section_length * float(p_index);
	xform.origin.y = -xform.origin.y; //bind is an inverse transform, so negate y

	return xform;
}

void TubeTrailMesh::_create_mesh_array(Array &p_arr) const {
	// Seeing use case for TubeTrailMesh, no need to do anything more then default UV2 calculation

	PackedVector3Array points;
	PackedVector3Array normals;
	PackedFloat32Array tangents;
	PackedVector2Array uvs;
	PackedInt32Array bone_indices;
	PackedFloat32Array bone_weights;
	PackedInt32Array indices;

	int point = 0;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	int thisrow = 0;
	int prevrow = 0;

	int total_rings = section_rings * sections;
	float depth = section_length * sections;

	for (int j = 0; j <= total_rings; j++) {
		float v = j;
		v /= total_rings;

		float y = depth * v;
		y = (depth * 0.5) - y;

		int bone = j / section_rings;
		float blend = 1.0 - float(j % section_rings) / float(section_rings);

		for (int i = 0; i <= radial_steps; i++) {
			float u = i;
			u /= radial_steps;

			float r = radius;
			if (curve.is_valid() && curve->get_point_count() > 0) {
				r *= curve->sample_baked(v);
			}
			float x = 0.0;
			float z = 1.0;
			if (i < radial_steps) {
				x = Math::sin(u * Math_TAU);
				z = Math::cos(u * Math_TAU);
			}

			Vector3 p = Vector3(x * r, y, z * r);
			points.push_back(p);
			normals.push_back(Vector3(x, 0, z));
			ADD_TANGENT(z, 0.0, -x, 1.0)
			uvs.push_back(Vector2(u, v * 0.5));
			point++;
			{
				bone_indices.push_back(bone);
				bone_indices.push_back(MIN(sections, bone + 1));
				bone_indices.push_back(0);
				bone_indices.push_back(0);

				bone_weights.push_back(blend);
				bone_weights.push_back(1.0 - blend);
				bone_weights.push_back(0);
				bone_weights.push_back(0);
			}

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			}
		}

		prevrow = thisrow;
		thisrow = point;
	}

	if (cap_top) {
		// add top
		float scale_pos = 1.0;
		if (curve.is_valid() && curve->get_point_count() > 0) {
			scale_pos = curve->sample_baked(0);
		}

		if (scale_pos > CMP_EPSILON) {
			float y = depth * 0.5;

			thisrow = point;
			points.push_back(Vector3(0.0, y, 0));
			normals.push_back(Vector3(0.0, 1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
			uvs.push_back(Vector2(0.25, 0.75));
			point++;

			bone_indices.push_back(0);
			bone_indices.push_back(0);
			bone_indices.push_back(0);
			bone_indices.push_back(0);

			bone_weights.push_back(1.0);
			bone_weights.push_back(0);
			bone_weights.push_back(0);
			bone_weights.push_back(0);

			float rm = radius * scale_pos;

			for (int i = 0; i <= radial_steps; i++) {
				float r = i;
				r /= radial_steps;

				float x = 0.0;
				float z = 1.0;
				if (i < radial_steps) {
					x = Math::sin(r * Math_TAU);
					z = Math::cos(r * Math_TAU);
				}

				float u = ((x + 1.0) * 0.25);
				float v = 0.5 + ((z + 1.0) * 0.25);

				Vector3 p = Vector3(x * rm, y, z * rm);
				points.push_back(p);
				normals.push_back(Vector3(0.0, 1.0, 0.0));
				ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
				uvs.push_back(Vector2(u, v));
				point++;

				bone_indices.push_back(0);
				bone_indices.push_back(0);
				bone_indices.push_back(0);
				bone_indices.push_back(0);

				bone_weights.push_back(1.0);
				bone_weights.push_back(0);
				bone_weights.push_back(0);
				bone_weights.push_back(0);

				if (i > 0) {
					indices.push_back(thisrow);
					indices.push_back(point - 1);
					indices.push_back(point - 2);
				}
			}
		}
	}

	if (cap_bottom) {
		float scale_neg = 1.0;
		if (curve.is_valid() && curve->get_point_count() > 0) {
			scale_neg = curve->sample_baked(1.0);
		}

		if (scale_neg > CMP_EPSILON) {
			// add bottom
			float y = depth * -0.5;

			thisrow = point;
			points.push_back(Vector3(0.0, y, 0.0));
			normals.push_back(Vector3(0.0, -1.0, 0.0));
			ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
			uvs.push_back(Vector2(0.75, 0.75));
			point++;

			bone_indices.push_back(sections);
			bone_indices.push_back(0);
			bone_indices.push_back(0);
			bone_indices.push_back(0);

			bone_weights.push_back(1.0);
			bone_weights.push_back(0);
			bone_weights.push_back(0);
			bone_weights.push_back(0);

			float rm = radius * scale_neg;

			for (int i = 0; i <= radial_steps; i++) {
				float r = i;
				r /= radial_steps;

				float x = 0.0;
				float z = 1.0;
				if (i < radial_steps) {
					x = Math::sin(r * Math_TAU);
					z = Math::cos(r * Math_TAU);
				}

				float u = 0.5 + ((x + 1.0) * 0.25);
				float v = 1.0 - ((z + 1.0) * 0.25);

				Vector3 p = Vector3(x * rm, y, z * rm);
				points.push_back(p);
				normals.push_back(Vector3(0.0, -1.0, 0.0));
				ADD_TANGENT(1.0, 0.0, 0.0, 1.0)
				uvs.push_back(Vector2(u, v));
				point++;

				bone_indices.push_back(sections);
				bone_indices.push_back(0);
				bone_indices.push_back(0);
				bone_indices.push_back(0);

				bone_weights.push_back(1.0);
				bone_weights.push_back(0);
				bone_weights.push_back(0);
				bone_weights.push_back(0);

				if (i > 0) {
					indices.push_back(thisrow);
					indices.push_back(point - 2);
					indices.push_back(point - 1);
				}
			}
		}
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_BONES] = bone_indices;
	p_arr[RS::ARRAY_WEIGHTS] = bone_weights;
	p_arr[RS::ARRAY_INDEX] = indices;
}

void TubeTrailMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &TubeTrailMesh::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &TubeTrailMesh::get_radius);

	ClassDB::bind_method(D_METHOD("set_radial_steps", "radial_steps"), &TubeTrailMesh::set_radial_steps);
	ClassDB::bind_method(D_METHOD("get_radial_steps"), &TubeTrailMesh::get_radial_steps);

	ClassDB::bind_method(D_METHOD("set_sections", "sections"), &TubeTrailMesh::set_sections);
	ClassDB::bind_method(D_METHOD("get_sections"), &TubeTrailMesh::get_sections);

	ClassDB::bind_method(D_METHOD("set_section_length", "section_length"), &TubeTrailMesh::set_section_length);
	ClassDB::bind_method(D_METHOD("get_section_length"), &TubeTrailMesh::get_section_length);

	ClassDB::bind_method(D_METHOD("set_section_rings", "section_rings"), &TubeTrailMesh::set_section_rings);
	ClassDB::bind_method(D_METHOD("get_section_rings"), &TubeTrailMesh::get_section_rings);

	ClassDB::bind_method(D_METHOD("set_cap_top", "cap_top"), &TubeTrailMesh::set_cap_top);
	ClassDB::bind_method(D_METHOD("is_cap_top"), &TubeTrailMesh::is_cap_top);

	ClassDB::bind_method(D_METHOD("set_cap_bottom", "cap_bottom"), &TubeTrailMesh::set_cap_bottom);
	ClassDB::bind_method(D_METHOD("is_cap_bottom"), &TubeTrailMesh::is_cap_bottom);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &TubeTrailMesh::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &TubeTrailMesh::get_curve);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_radius", "get_radius");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_steps", PROPERTY_HINT_RANGE, "3,128,1"), "set_radial_steps", "get_radial_steps");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sections", PROPERTY_HINT_RANGE, "2,128,1"), "set_sections", "get_sections");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "section_length", PROPERTY_HINT_RANGE, "0.001,1024.0,0.001,or_greater,suffix:m"), "set_section_length", "get_section_length");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "section_rings", PROPERTY_HINT_RANGE, "1,128,1"), "set_section_rings", "get_section_rings");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cap_top"), "set_cap_top", "is_cap_top");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cap_bottom"), "set_cap_bottom", "is_cap_bottom");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}

TubeTrailMesh::TubeTrailMesh() {
}

// RIBBON TRAIL

void RibbonTrailMesh::set_shape(Shape p_shape) {
	if (p_shape == shape) {
		return;
	}
	shape = p_shape;
	request_update();
}
RibbonTrailMesh::Shape RibbonTrailMesh::get_shape() const {
	return shape;
}

void RibbonTrailMesh::set_size(const float p_size) {
	if (Math::is_equal_approx(p_size, size)) {
		return;
	}
	size = p_size;
	request_update();
}
float RibbonTrailMesh::get_size() const {
	return size;
}

void RibbonTrailMesh::set_sections(const int p_sections) {
	if (p_sections == sections) {
		return;
	}
	ERR_FAIL_COND(p_sections < 2 || p_sections > 128);
	sections = p_sections;
	request_update();
}
int RibbonTrailMesh::get_sections() const {
	return sections;
}

void RibbonTrailMesh::set_section_length(float p_section_length) {
	if (p_section_length == section_length) {
		return;
	}
	section_length = p_section_length;
	request_update();
}
float RibbonTrailMesh::get_section_length() const {
	return section_length;
}

void RibbonTrailMesh::set_section_segments(const int p_section_segments) {
	if (p_section_segments == section_segments) {
		return;
	}
	ERR_FAIL_COND(p_section_segments < 1 || p_section_segments > 1024);
	section_segments = p_section_segments;
	request_update();
}
int RibbonTrailMesh::get_section_segments() const {
	return section_segments;
}

void RibbonTrailMesh::set_curve(const Ref<Curve> &p_curve) {
	if (curve == p_curve) {
		return;
	}
	if (curve.is_valid()) {
		curve->disconnect_changed(callable_mp(this, &RibbonTrailMesh::_curve_changed));
	}
	curve = p_curve;
	if (curve.is_valid()) {
		curve->connect_changed(callable_mp(this, &RibbonTrailMesh::_curve_changed));
	}
	request_update();
}
Ref<Curve> RibbonTrailMesh::get_curve() const {
	return curve;
}

void RibbonTrailMesh::_curve_changed() {
	request_update();
}
int RibbonTrailMesh::get_builtin_bind_pose_count() const {
	return sections + 1;
}

Transform3D RibbonTrailMesh::get_builtin_bind_pose(int p_index) const {
	float depth = section_length * sections;

	Transform3D xform;
	xform.origin.y = depth / 2.0 - section_length * float(p_index);
	xform.origin.y = -xform.origin.y; //bind is an inverse transform, so negate y

	return xform;
}

void RibbonTrailMesh::_create_mesh_array(Array &p_arr) const {
	// Seeing use case of ribbon trail mesh, no need to implement special UV2 calculation

	PackedVector3Array points;
	PackedVector3Array normals;
	PackedFloat32Array tangents;
	PackedVector2Array uvs;
	PackedInt32Array bone_indices;
	PackedFloat32Array bone_weights;
	PackedInt32Array indices;

#define ADD_TANGENT(m_x, m_y, m_z, m_d) \
	tangents.push_back(m_x);            \
	tangents.push_back(m_y);            \
	tangents.push_back(m_z);            \
	tangents.push_back(m_d);

	int total_segments = section_segments * sections;
	float depth = section_length * sections;

	for (int j = 0; j <= total_segments; j++) {
		float v = j;
		v /= total_segments;

		float y = depth * v;
		y = (depth * 0.5) - y;

		int bone = j / section_segments;
		float blend = 1.0 - float(j % section_segments) / float(section_segments);

		float s = size;

		if (curve.is_valid() && curve->get_point_count() > 0) {
			s *= curve->sample_baked(v);
		}

		points.push_back(Vector3(-s * 0.5, y, 0));
		points.push_back(Vector3(+s * 0.5, y, 0));
		if (shape == SHAPE_CROSS) {
			points.push_back(Vector3(0, y, -s * 0.5));
			points.push_back(Vector3(0, y, +s * 0.5));
		}

		normals.push_back(Vector3(0, 0, 1));
		normals.push_back(Vector3(0, 0, 1));
		if (shape == SHAPE_CROSS) {
			normals.push_back(Vector3(1, 0, 0));
			normals.push_back(Vector3(1, 0, 0));
		}

		uvs.push_back(Vector2(0, v));
		uvs.push_back(Vector2(1, v));
		if (shape == SHAPE_CROSS) {
			uvs.push_back(Vector2(0, v));
			uvs.push_back(Vector2(1, v));
		}

		ADD_TANGENT(0.0, 1.0, 0.0, 1.0)
		ADD_TANGENT(0.0, 1.0, 0.0, 1.0)
		if (shape == SHAPE_CROSS) {
			ADD_TANGENT(0.0, 1.0, 0.0, 1.0)
			ADD_TANGENT(0.0, 1.0, 0.0, 1.0)
		}

		for (int i = 0; i < (shape == SHAPE_CROSS ? 4 : 2); i++) {
			bone_indices.push_back(bone);
			bone_indices.push_back(MIN(sections, bone + 1));
			bone_indices.push_back(0);
			bone_indices.push_back(0);

			bone_weights.push_back(blend);
			bone_weights.push_back(1.0 - blend);
			bone_weights.push_back(0);
			bone_weights.push_back(0);
		}

		if (j > 0) {
			if (shape == SHAPE_CROSS) {
				int base = j * 4 - 4;
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 4);

				indices.push_back(base + 1);
				indices.push_back(base + 5);
				indices.push_back(base + 4);

				indices.push_back(base + 2);
				indices.push_back(base + 3);
				indices.push_back(base + 6);

				indices.push_back(base + 3);
				indices.push_back(base + 7);
				indices.push_back(base + 6);
			} else {
				int base = j * 2 - 2;
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 2);

				indices.push_back(base + 1);
				indices.push_back(base + 3);
				indices.push_back(base + 2);
			}
		}
	}

	p_arr[RS::ARRAY_VERTEX] = points;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_BONES] = bone_indices;
	p_arr[RS::ARRAY_WEIGHTS] = bone_weights;
	p_arr[RS::ARRAY_INDEX] = indices;
}

void RibbonTrailMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &RibbonTrailMesh::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &RibbonTrailMesh::get_size);

	ClassDB::bind_method(D_METHOD("set_sections", "sections"), &RibbonTrailMesh::set_sections);
	ClassDB::bind_method(D_METHOD("get_sections"), &RibbonTrailMesh::get_sections);

	ClassDB::bind_method(D_METHOD("set_section_length", "section_length"), &RibbonTrailMesh::set_section_length);
	ClassDB::bind_method(D_METHOD("get_section_length"), &RibbonTrailMesh::get_section_length);

	ClassDB::bind_method(D_METHOD("set_section_segments", "section_segments"), &RibbonTrailMesh::set_section_segments);
	ClassDB::bind_method(D_METHOD("get_section_segments"), &RibbonTrailMesh::get_section_segments);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &RibbonTrailMesh::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &RibbonTrailMesh::get_curve);

	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &RibbonTrailMesh::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &RibbonTrailMesh::get_shape);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "Flat,Cross"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "size", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,or_greater,suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sections", PROPERTY_HINT_RANGE, "2,128,1"), "set_sections", "get_sections");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "section_length", PROPERTY_HINT_RANGE, "0.001,1024.0,0.001,or_greater,suffix:m"), "set_section_length", "get_section_length");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "section_segments", PROPERTY_HINT_RANGE, "1,128,1"), "set_section_segments", "get_section_segments");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");

	BIND_ENUM_CONSTANT(SHAPE_FLAT)
	BIND_ENUM_CONSTANT(SHAPE_CROSS)
}

RibbonTrailMesh::RibbonTrailMesh() {
}

/*************************************************************************/
/*  TextMesh                                                             */
/*************************************************************************/

void TextMesh::_generate_glyph_mesh_data(const GlyphMeshKey &p_key, const Glyph &p_gl) const {
	if (cache.has(p_key)) {
		return;
	}

	GlyphMeshData &gl_data = cache[p_key];

	Dictionary d = TS->font_get_glyph_contours(p_gl.font_rid, p_gl.font_size, p_gl.index);

	PackedVector3Array points = d["points"];
	PackedInt32Array contours = d["contours"];
	bool orientation = d["orientation"];

	if (points.size() < 3 || contours.size() < 1) {
		return; // No full contours, only glyph control points (or nothing), ignore.
	}

	// Approximate Bezier curves as polygons.
	// See https://freetype.org/freetype2/docs/glyphs/glyphs-6.html, for more info.
	for (int i = 0; i < contours.size(); i++) {
		int32_t start = (i == 0) ? 0 : (contours[i - 1] + 1);
		int32_t end = contours[i];
		Vector<ContourPoint> polygon;

		for (int32_t j = start; j <= end; j++) {
			if (points[j].z == TextServer::CONTOUR_CURVE_TAG_ON) {
				// Point on the curve.
				Vector2 p = Vector2(points[j].x, points[j].y) * pixel_size;
				polygon.push_back(ContourPoint(p, true));
			} else if (points[j].z == TextServer::CONTOUR_CURVE_TAG_OFF_CONIC) {
				// Conic Bezier arc.
				int32_t next = (j == end) ? start : (j + 1);
				int32_t prev = (j == start) ? end : (j - 1);
				Vector2 p0;
				Vector2 p1 = Vector2(points[j].x, points[j].y);
				Vector2 p2;

				// For successive conic OFF points add a virtual ON point in the middle.
				if (points[prev].z == TextServer::CONTOUR_CURVE_TAG_OFF_CONIC) {
					p0 = (Vector2(points[prev].x, points[prev].y) + Vector2(points[j].x, points[j].y)) / 2.0;
				} else if (points[prev].z == TextServer::CONTOUR_CURVE_TAG_ON) {
					p0 = Vector2(points[prev].x, points[prev].y);
				} else {
					ERR_FAIL_MSG(vformat("Invalid conic arc point sequence at %d:%d", i, j));
				}
				if (points[next].z == TextServer::CONTOUR_CURVE_TAG_OFF_CONIC) {
					p2 = (Vector2(points[j].x, points[j].y) + Vector2(points[next].x, points[next].y)) / 2.0;
				} else if (points[next].z == TextServer::CONTOUR_CURVE_TAG_ON) {
					p2 = Vector2(points[next].x, points[next].y);
				} else {
					ERR_FAIL_MSG(vformat("Invalid conic arc point sequence at %d:%d", i, j));
				}

				real_t step = CLAMP(curve_step / (p0 - p2).length(), 0.01, 0.5);
				real_t t = step;
				while (t < 1.0) {
					real_t omt = (1.0 - t);
					real_t omt2 = omt * omt;
					real_t t2 = t * t;

					Vector2 point = p1 + omt2 * (p0 - p1) + t2 * (p2 - p1);
					Vector2 p = point * pixel_size;
					polygon.push_back(ContourPoint(p, false));
					t += step;
				}
			} else if (points[j].z == TextServer::CONTOUR_CURVE_TAG_OFF_CUBIC) {
				// Cubic Bezier arc.
				int32_t cur = j;
				int32_t next1 = (j == end) ? start : (j + 1);
				int32_t next2 = (next1 == end) ? start : (next1 + 1);
				int32_t prev = (j == start) ? end : (j - 1);

				// There must be exactly two OFF points and two ON points for each cubic arc.
				if (points[prev].z != TextServer::CONTOUR_CURVE_TAG_ON) {
					cur = (cur == 0) ? end : cur - 1;
					next1 = (next1 == 0) ? end : next1 - 1;
					next2 = (next2 == 0) ? end : next2 - 1;
					prev = (prev == 0) ? end : prev - 1;
				} else {
					j++;
				}
				ERR_FAIL_COND_MSG(points[prev].z != TextServer::CONTOUR_CURVE_TAG_ON, vformat("Invalid cubic arc point sequence at %d:%d", i, prev));
				ERR_FAIL_COND_MSG(points[cur].z != TextServer::CONTOUR_CURVE_TAG_OFF_CUBIC, vformat("Invalid cubic arc point sequence at %d:%d", i, cur));
				ERR_FAIL_COND_MSG(points[next1].z != TextServer::CONTOUR_CURVE_TAG_OFF_CUBIC, vformat("Invalid cubic arc point sequence at %d:%d", i, next1));
				ERR_FAIL_COND_MSG(points[next2].z != TextServer::CONTOUR_CURVE_TAG_ON, vformat("Invalid cubic arc point sequence at %d:%d", i, next2));

				Vector2 p0 = Vector2(points[prev].x, points[prev].y);
				Vector2 p1 = Vector2(points[cur].x, points[cur].y);
				Vector2 p2 = Vector2(points[next1].x, points[next1].y);
				Vector2 p3 = Vector2(points[next2].x, points[next2].y);

				real_t step = CLAMP(curve_step / (p0 - p3).length(), 0.01, 0.5);
				real_t t = step;
				while (t < 1.0) {
					Vector2 point = p0.bezier_interpolate(p1, p2, p3, t);
					Vector2 p = point * pixel_size;
					polygon.push_back(ContourPoint(p, false));
					t += step;
				}
			} else {
				ERR_FAIL_MSG(vformat("Unknown point tag at %d:%d", i, j));
			}
		}

		if (polygon.size() < 3) {
			continue; // Skip glyph control points.
		}

		if (!orientation) {
			polygon.reverse();
		}

		gl_data.contours.push_back(polygon);
	}

	// Calculate bounds.
	List<TPPLPoly> in_poly;
	for (int i = 0; i < gl_data.contours.size(); i++) {
		TPPLPoly inp;
		inp.Init(gl_data.contours[i].size());
		real_t length = 0.0;
		for (int j = 0; j < gl_data.contours[i].size(); j++) {
			int next = (j + 1 == gl_data.contours[i].size()) ? 0 : (j + 1);

			gl_data.min_p = gl_data.min_p.min(gl_data.contours[i][j].point);
			gl_data.max_p = gl_data.max_p.max(gl_data.contours[i][j].point);
			length += (gl_data.contours[i][next].point - gl_data.contours[i][j].point).length();

			inp.GetPoint(j) = gl_data.contours[i][j].point;
		}
		TPPLOrientation poly_orient = inp.GetOrientation();
		if (poly_orient == TPPL_ORIENTATION_CW) {
			inp.SetHole(true);
		}
		in_poly.push_back(inp);
		gl_data.contours_info.push_back(ContourInfo(length, poly_orient == TPPL_ORIENTATION_CCW));
	}

	TPPLPartition tpart;

	//Decompose and triangulate.
	List<TPPLPoly> out_poly;
	if (tpart.ConvexPartition_HM(&in_poly, &out_poly) == 0) {
		ERR_FAIL_MSG("Convex decomposing failed. Make sure the font doesn't contain self-intersecting lines, as these are not supported in TextMesh.");
	}
	List<TPPLPoly> out_tris;
	for (List<TPPLPoly>::Element *I = out_poly.front(); I; I = I->next()) {
		if (tpart.Triangulate_OPT(&(I->get()), &out_tris) == 0) {
			ERR_FAIL_MSG("Triangulation failed. Make sure the font doesn't contain self-intersecting lines, as these are not supported in TextMesh.");
		}
	}

	for (List<TPPLPoly>::Element *I = out_tris.front(); I; I = I->next()) {
		TPPLPoly &tp = I->get();
		ERR_FAIL_COND(tp.GetNumPoints() != 3); // Triangles only.

		for (int i = 0; i < 3; i++) {
			gl_data.triangles.push_back(Vector2(tp.GetPoint(i).x, tp.GetPoint(i).y));
		}
	}
}

void TextMesh::_create_mesh_array(Array &p_arr) const {
	Ref<Font> font = _get_font_or_default();
	ERR_FAIL_COND(font.is_null());

	if (dirty_cache) {
		cache.clear();
		dirty_cache = false;
	}

	// When a shaped text is invalidated by an external source, we want to reshape it.
	if (!TS->shaped_text_is_ready(text_rid)) {
		dirty_text = true;
	}

	for (const RID &line_rid : lines_rid) {
		if (!TS->shaped_text_is_ready(line_rid)) {
			dirty_lines = true;
			break;
		}
	}

	// Update text buffer.
	if (dirty_text) {
		TS->shaped_text_clear(text_rid);
		TS->shaped_text_set_direction(text_rid, text_direction);

		String txt = (uppercase) ? TS->string_to_upper(xl_text, language) : xl_text;
		TS->shaped_text_add_string(text_rid, txt, font->get_rids(), font_size, font->get_opentype_features(), language);

		TypedArray<Vector3i> stt;
		if (st_parser == TextServer::STRUCTURED_TEXT_CUSTOM) {
			GDVIRTUAL_CALL(_structured_text_parser, st_args, txt, stt);
		} else {
			stt = TS->parse_structured_text(st_parser, st_args, txt);
		}
		TS->shaped_text_set_bidi_override(text_rid, stt);

		dirty_text = false;
		dirty_font = false;
		dirty_lines = true;
	} else if (dirty_font) {
		int spans = TS->shaped_get_span_count(text_rid);
		for (int i = 0; i < spans; i++) {
			TS->shaped_set_span_update_font(text_rid, i, font->get_rids(), font_size, font->get_opentype_features());
		}

		dirty_font = false;
		dirty_lines = true;
	}

	if (dirty_lines) {
		for (int i = 0; i < lines_rid.size(); i++) {
			TS->free_rid(lines_rid[i]);
		}
		lines_rid.clear();

		BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
		switch (autowrap_mode) {
			case TextServer::AUTOWRAP_WORD_SMART:
				autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_WORD:
				autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_ARBITRARY:
				autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
				break;
			case TextServer::AUTOWRAP_OFF:
				break;
		}
		PackedInt32Array line_breaks = TS->shaped_text_get_line_breaks(text_rid, width, 0, autowrap_flags);

		float max_line_w = 0.0;
		for (int i = 0; i < line_breaks.size(); i = i + 2) {
			RID line = TS->shaped_text_substr(text_rid, line_breaks[i], line_breaks[i + 1] - line_breaks[i]);
			max_line_w = MAX(max_line_w, TS->shaped_text_get_width(line));
			lines_rid.push_back(line);
		}

		if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL) {
			int jst_to_line = lines_rid.size();
			if (lines_rid.size() == 1 && jst_flags.has_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE)) {
				jst_to_line = lines_rid.size();
			} else {
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE)) {
					jst_to_line = lines_rid.size() - 1;
				}
				if (jst_flags.has_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS)) {
					for (int i = lines_rid.size() - 1; i >= 0; i--) {
						if (TS->shaped_text_has_visible_chars(lines_rid[i])) {
							jst_to_line = i;
							break;
						}
					}
				}
			}
			for (int i = 0; i < jst_to_line; i++) {
				TS->shaped_text_fit_to_width(lines_rid[i], (width > 0) ? width : max_line_w, jst_flags);
			}
		}
		dirty_lines = false;
	}

	float total_h = 0.0;
	for (int i = 0; i < lines_rid.size(); i++) {
		total_h += (TS->shaped_text_get_size(lines_rid[i]).y + line_spacing) * pixel_size;
	}

	float vbegin = 0.0;
	switch (vertical_alignment) {
		case VERTICAL_ALIGNMENT_FILL:
		case VERTICAL_ALIGNMENT_TOP: {
			// Nothing.
		} break;
		case VERTICAL_ALIGNMENT_CENTER: {
			vbegin = (total_h - line_spacing * pixel_size) / 2.0;
		} break;
		case VERTICAL_ALIGNMENT_BOTTOM: {
			vbegin = (total_h - line_spacing * pixel_size);
		} break;
	}

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<float> tangents;
	Vector<Vector2> uvs;
	Vector<int32_t> indices;

	Vector2 min_p = Vector2(INFINITY, INFINITY);
	Vector2 max_p = Vector2(-INFINITY, -INFINITY);

	int32_t p_size = 0;
	int32_t i_size = 0;

	Vector2 offset = Vector2(0, vbegin + lbl_offset.y * pixel_size);
	for (int i = 0; i < lines_rid.size(); i++) {
		const Glyph *glyphs = TS->shaped_text_get_glyphs(lines_rid[i]);
		int gl_size = TS->shaped_text_get_glyph_count(lines_rid[i]);
		float line_width = TS->shaped_text_get_width(lines_rid[i]) * pixel_size;

		switch (horizontal_alignment) {
			case HORIZONTAL_ALIGNMENT_LEFT:
				offset.x = 0.0;
				break;
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_CENTER: {
				offset.x = -line_width / 2.0;
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				offset.x = -line_width;
			} break;
		}
		offset.x += lbl_offset.x * pixel_size;
		offset.y -= TS->shaped_text_get_ascent(lines_rid[i]) * pixel_size;

		bool has_depth = !Math::is_zero_approx(depth);

		for (int j = 0; j < gl_size; j++) {
			if (glyphs[j].index == 0) {
				offset.x += glyphs[j].advance * pixel_size * glyphs[j].repeat;
				continue;
			}
			if (glyphs[j].font_rid != RID()) {
				GlyphMeshKey key = GlyphMeshKey(glyphs[j].font_rid.get_id(), glyphs[j].index);
				_generate_glyph_mesh_data(key, glyphs[j]);
				GlyphMeshData &gl_data = cache[key];
				const Vector2 gl_of = Vector2(glyphs[j].x_off, glyphs[j].y_off) * pixel_size;

				p_size += glyphs[j].repeat * gl_data.triangles.size() * ((has_depth) ? 2 : 1);
				i_size += glyphs[j].repeat * gl_data.triangles.size() * ((has_depth) ? 2 : 1);

				if (has_depth) {
					for (int k = 0; k < gl_data.contours.size(); k++) {
						p_size += glyphs[j].repeat * gl_data.contours[k].size() * 4;
						i_size += glyphs[j].repeat * gl_data.contours[k].size() * 6;
					}
				}

				for (int r = 0; r < glyphs[j].repeat; r++) {
					min_p.x = MIN(gl_data.min_p.x + offset.x + gl_of.x, min_p.x);
					min_p.y = MIN(gl_data.min_p.y - offset.y + gl_of.y, min_p.y);
					max_p.x = MAX(gl_data.max_p.x + offset.x + gl_of.x, max_p.x);
					max_p.y = MAX(gl_data.max_p.y - offset.y + gl_of.y, max_p.y);

					offset.x += glyphs[j].advance * pixel_size;
				}
			} else {
				p_size += glyphs[j].repeat * 4;
				i_size += glyphs[j].repeat * 6;

				offset.x += glyphs[j].advance * pixel_size * glyphs[j].repeat;
			}
		}
		offset.y -= (TS->shaped_text_get_descent(lines_rid[i]) + line_spacing) * pixel_size;
	}

	vertices.resize(p_size);
	normals.resize(p_size);
	uvs.resize(p_size);
	tangents.resize(p_size * 4);
	indices.resize(i_size);

	Vector3 *vertices_ptr = vertices.ptrw();
	Vector3 *normals_ptr = normals.ptrw();
	float *tangents_ptr = tangents.ptrw();
	Vector2 *uvs_ptr = uvs.ptrw();
	int32_t *indices_ptr = indices.ptrw();

	// Generate mesh.
	int32_t p_idx = 0;
	int32_t i_idx = 0;

	offset = Vector2(0, vbegin + lbl_offset.y * pixel_size);
	for (int i = 0; i < lines_rid.size(); i++) {
		const Glyph *glyphs = TS->shaped_text_get_glyphs(lines_rid[i]);
		int gl_size = TS->shaped_text_get_glyph_count(lines_rid[i]);
		float line_width = TS->shaped_text_get_width(lines_rid[i]) * pixel_size;

		switch (horizontal_alignment) {
			case HORIZONTAL_ALIGNMENT_LEFT:
				offset.x = 0.0;
				break;
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_CENTER: {
				offset.x = -line_width / 2.0;
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				offset.x = -line_width;
			} break;
		}
		offset.x += lbl_offset.x * pixel_size;
		offset.y -= TS->shaped_text_get_ascent(lines_rid[i]) * pixel_size;

		bool has_depth = !Math::is_zero_approx(depth);

		// Generate glyph data, precalculate size of the arrays and mesh bounds for UV.
		for (int j = 0; j < gl_size; j++) {
			if (glyphs[j].index == 0) {
				offset.x += glyphs[j].advance * pixel_size * glyphs[j].repeat;
				continue;
			}
			if (glyphs[j].font_rid != RID()) {
				GlyphMeshKey key = GlyphMeshKey(glyphs[j].font_rid.get_id(), glyphs[j].index);
				_generate_glyph_mesh_data(key, glyphs[j]);
				const GlyphMeshData &gl_data = cache[key];

				int64_t ts = gl_data.triangles.size();
				const Vector2 *ts_ptr = gl_data.triangles.ptr();
				const Vector2 gl_of = Vector2(glyphs[j].x_off, glyphs[j].y_off) * pixel_size;

				for (int r = 0; r < glyphs[j].repeat; r++) {
					for (int k = 0; k < ts; k += 3) {
						// Add front face.
						for (int l = 0; l < 3; l++) {
							Vector3 point = Vector3(ts_ptr[k + l].x + offset.x + gl_of.x, -ts_ptr[k + l].y + offset.y - gl_of.y, depth / 2.0);
							vertices_ptr[p_idx] = point;
							normals_ptr[p_idx] = Vector3(0.0, 0.0, 1.0);
							if (has_depth) {
								uvs_ptr[p_idx] = Vector2(Math::remap(point.x, min_p.x, max_p.x, real_t(0.0), real_t(1.0)), Math::remap(point.y, -max_p.y, -min_p.y, real_t(0.4), real_t(0.0)));
							} else {
								uvs_ptr[p_idx] = Vector2(Math::remap(point.x, min_p.x, max_p.x, real_t(0.0), real_t(1.0)), Math::remap(point.y, -max_p.y, -min_p.y, real_t(1.0), real_t(0.0)));
							}
							tangents_ptr[p_idx * 4 + 0] = 1.0;
							tangents_ptr[p_idx * 4 + 1] = 0.0;
							tangents_ptr[p_idx * 4 + 2] = 0.0;
							tangents_ptr[p_idx * 4 + 3] = 1.0;
							indices_ptr[i_idx++] = p_idx;
							p_idx++;
						}
						if (has_depth) {
							// Add back face.
							for (int l = 2; l >= 0; l--) {
								Vector3 point = Vector3(ts_ptr[k + l].x + offset.x + gl_of.x, -ts_ptr[k + l].y + offset.y - gl_of.y, -depth / 2.0);
								vertices_ptr[p_idx] = point;
								normals_ptr[p_idx] = Vector3(0.0, 0.0, -1.0);
								uvs_ptr[p_idx] = Vector2(Math::remap(point.x, min_p.x, max_p.x, real_t(0.0), real_t(1.0)), Math::remap(point.y, -max_p.y, -min_p.y, real_t(0.8), real_t(0.4)));
								tangents_ptr[p_idx * 4 + 0] = -1.0;
								tangents_ptr[p_idx * 4 + 1] = 0.0;
								tangents_ptr[p_idx * 4 + 2] = 0.0;
								tangents_ptr[p_idx * 4 + 3] = 1.0;
								indices_ptr[i_idx++] = p_idx;
								p_idx++;
							}
						}
					}
					// Add sides.
					if (has_depth) {
						for (int k = 0; k < gl_data.contours.size(); k++) {
							int64_t ps = gl_data.contours[k].size();
							const ContourPoint *ps_ptr = gl_data.contours[k].ptr();
							const ContourInfo &ps_info = gl_data.contours_info[k];
							real_t length = 0.0;
							for (int l = 0; l < ps; l++) {
								int prev = (l == 0) ? (ps - 1) : (l - 1);
								int next = (l + 1 == ps) ? 0 : (l + 1);
								Vector2 d1;
								Vector2 d2 = (ps_ptr[next].point - ps_ptr[l].point).normalized();
								if (ps_ptr[l].sharp) {
									d1 = d2;
								} else {
									d1 = (ps_ptr[l].point - ps_ptr[prev].point).normalized();
								}
								real_t seg_len = (ps_ptr[next].point - ps_ptr[l].point).length();

								Vector3 quad_faces[4] = {
									Vector3(ps_ptr[l].point.x + offset.x + gl_of.x, -ps_ptr[l].point.y + offset.y - gl_of.y, -depth / 2.0),
									Vector3(ps_ptr[next].point.x + offset.x + gl_of.x, -ps_ptr[next].point.y + offset.y - gl_of.y, -depth / 2.0),
									Vector3(ps_ptr[l].point.x + offset.x + gl_of.x, -ps_ptr[l].point.y + offset.y - gl_of.y, depth / 2.0),
									Vector3(ps_ptr[next].point.x + offset.x + gl_of.x, -ps_ptr[next].point.y + offset.y - gl_of.y, depth / 2.0),
								};
								for (int m = 0; m < 4; m++) {
									const Vector2 &d = ((m % 2) == 0) ? d1 : d2;
									real_t u_pos = ((m % 2) == 0) ? length : length + seg_len;
									vertices_ptr[p_idx + m] = quad_faces[m];
									normals_ptr[p_idx + m] = Vector3(d.y, d.x, 0.0);
									if (m < 2) {
										uvs_ptr[p_idx + m] = Vector2(Math::remap(u_pos, 0, ps_info.length, real_t(0.0), real_t(1.0)), (ps_info.ccw) ? 0.8 : 0.9);
									} else {
										uvs_ptr[p_idx + m] = Vector2(Math::remap(u_pos, 0, ps_info.length, real_t(0.0), real_t(1.0)), (ps_info.ccw) ? 0.9 : 1.0);
									}
									tangents_ptr[(p_idx + m) * 4 + 0] = d.x;
									tangents_ptr[(p_idx + m) * 4 + 1] = -d.y;
									tangents_ptr[(p_idx + m) * 4 + 2] = 0.0;
									tangents_ptr[(p_idx + m) * 4 + 3] = 1.0;
								}

								indices_ptr[i_idx++] = p_idx;
								indices_ptr[i_idx++] = p_idx + 1;
								indices_ptr[i_idx++] = p_idx + 2;

								indices_ptr[i_idx++] = p_idx + 1;
								indices_ptr[i_idx++] = p_idx + 3;
								indices_ptr[i_idx++] = p_idx + 2;

								length += seg_len;
								p_idx += 4;
							}
						}
					}
					offset.x += glyphs[j].advance * pixel_size;
				}
			} else {
				// Add fallback quad for missing glyphs.
				for (int r = 0; r < glyphs[j].repeat; r++) {
					Size2 sz = TS->get_hex_code_box_size(glyphs[j].font_size, glyphs[j].index) * pixel_size;
					Vector3 quad_faces[4] = {
						Vector3(offset.x, offset.y, 0.0),
						Vector3(offset.x, sz.y + offset.y, 0.0),
						Vector3(sz.x + offset.x, sz.y + offset.y, 0.0),
						Vector3(sz.x + offset.x, offset.y, 0.0),
					};
					for (int k = 0; k < 4; k++) {
						vertices_ptr[p_idx + k] = quad_faces[k];
						normals_ptr[p_idx + k] = Vector3(0.0, 0.0, 1.0);
						if (has_depth) {
							uvs_ptr[p_idx + k] = Vector2(Math::remap(quad_faces[k].x, min_p.x, max_p.x, real_t(0.0), real_t(1.0)), Math::remap(quad_faces[k].y, -max_p.y, -min_p.y, real_t(0.4), real_t(0.0)));
						} else {
							uvs_ptr[p_idx + k] = Vector2(Math::remap(quad_faces[k].x, min_p.x, max_p.x, real_t(0.0), real_t(1.0)), Math::remap(quad_faces[k].y, -max_p.y, -min_p.y, real_t(1.0), real_t(0.0)));
						}
						tangents_ptr[(p_idx + k) * 4 + 0] = 1.0;
						tangents_ptr[(p_idx + k) * 4 + 1] = 0.0;
						tangents_ptr[(p_idx + k) * 4 + 2] = 0.0;
						tangents_ptr[(p_idx + k) * 4 + 3] = 1.0;
					}

					indices_ptr[i_idx++] = p_idx;
					indices_ptr[i_idx++] = p_idx + 1;
					indices_ptr[i_idx++] = p_idx + 2;

					indices_ptr[i_idx++] = p_idx + 0;
					indices_ptr[i_idx++] = p_idx + 2;
					indices_ptr[i_idx++] = p_idx + 3;
					p_idx += 4;

					offset.x += glyphs[j].advance * pixel_size;
				}
			}
		}
		offset.y -= (TS->shaped_text_get_descent(lines_rid[i]) + line_spacing) * pixel_size;
	}

	if (indices.is_empty()) {
		// If empty, add single triangle to suppress errors.
		vertices.push_back(Vector3());
		normals.push_back(Vector3());
		uvs.push_back(Vector2());
		tangents.push_back(1.0);
		tangents.push_back(0.0);
		tangents.push_back(0.0);
		tangents.push_back(1.0);
		indices.push_back(0);
		indices.push_back(0);
		indices.push_back(0);
	}

	p_arr[RS::ARRAY_VERTEX] = vertices;
	p_arr[RS::ARRAY_NORMAL] = normals;
	p_arr[RS::ARRAY_TANGENT] = tangents;
	p_arr[RS::ARRAY_TEX_UV] = uvs;
	p_arr[RS::ARRAY_INDEX] = indices;
}

void TextMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &TextMesh::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &TextMesh::get_horizontal_alignment);

	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &TextMesh::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &TextMesh::get_vertical_alignment);

	ClassDB::bind_method(D_METHOD("set_text", "text"), &TextMesh::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &TextMesh::get_text);

	ClassDB::bind_method(D_METHOD("set_font", "font"), &TextMesh::set_font);
	ClassDB::bind_method(D_METHOD("get_font"), &TextMesh::get_font);

	ClassDB::bind_method(D_METHOD("set_font_size", "font_size"), &TextMesh::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &TextMesh::get_font_size);

	ClassDB::bind_method(D_METHOD("set_line_spacing", "line_spacing"), &TextMesh::set_line_spacing);
	ClassDB::bind_method(D_METHOD("get_line_spacing"), &TextMesh::get_line_spacing);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &TextMesh::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &TextMesh::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_justification_flags", "justification_flags"), &TextMesh::set_justification_flags);
	ClassDB::bind_method(D_METHOD("get_justification_flags"), &TextMesh::get_justification_flags);

	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &TextMesh::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &TextMesh::get_depth);

	ClassDB::bind_method(D_METHOD("set_width", "width"), &TextMesh::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &TextMesh::get_width);

	ClassDB::bind_method(D_METHOD("set_pixel_size", "pixel_size"), &TextMesh::set_pixel_size);
	ClassDB::bind_method(D_METHOD("get_pixel_size"), &TextMesh::get_pixel_size);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &TextMesh::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &TextMesh::get_offset);

	ClassDB::bind_method(D_METHOD("set_curve_step", "curve_step"), &TextMesh::set_curve_step);
	ClassDB::bind_method(D_METHOD("get_curve_step"), &TextMesh::get_curve_step);

	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &TextMesh::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &TextMesh::get_text_direction);

	ClassDB::bind_method(D_METHOD("set_language", "language"), &TextMesh::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &TextMesh::get_language);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &TextMesh::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &TextMesh::get_structured_text_bidi_override);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &TextMesh::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &TextMesh::get_structured_text_bidi_override_options);

	ClassDB::bind_method(D_METHOD("set_uppercase", "enable"), &TextMesh::set_uppercase);
	ClassDB::bind_method(D_METHOD("is_uppercase"), &TextMesh::is_uppercase);

	ADD_GROUP("Text", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT, ""), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_font", "get_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_size", PROPERTY_HINT_RANGE, "1,256,1,or_greater,suffix:px"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "uppercase"), "set_uppercase", "is_uppercase");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_spacing", PROPERTY_HINT_NONE, "suffix:px"), "set_line_spacing", "get_line_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "justification_flags", PROPERTY_HINT_FLAGS, "Kashida Justification:1,Word Justification:2,Justify Only After Last Tab:8,Skip Last Line:32,Skip Last Line With Visible Characters:64,Do Not Skip Single Line:128"), "set_justification_flags", "get_justification_flags");

	ADD_GROUP("Mesh", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pixel_size", PROPERTY_HINT_RANGE, "0.0001,128,0.0001,suffix:m"), "set_pixel_size", "get_pixel_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "curve_step", PROPERTY_HINT_RANGE, "0.1,10,0.1,suffix:px"), "set_curve_step", "get_curve_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth", PROPERTY_HINT_RANGE, "0.0,100.0,0.001,or_greater,suffix:m"), "set_depth", "get_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_NONE, "suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");
}

void TextMesh::_notification(int p_what) {
	switch (p_what) {
		case MainLoop::NOTIFICATION_TRANSLATION_CHANGED: {
			String new_text = tr(text);
			if (new_text == xl_text) {
				return; // Nothing new.
			}
			xl_text = new_text;
			dirty_text = true;
			request_update();
		} break;
	}
}

TextMesh::TextMesh() {
	primitive_type = PRIMITIVE_TRIANGLES;
	text_rid = TS->create_shaped_text();
}

TextMesh::~TextMesh() {
	for (int i = 0; i < lines_rid.size(); i++) {
		TS->free_rid(lines_rid[i]);
	}
	lines_rid.clear();

	TS->free_rid(text_rid);
}

void TextMesh::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	if (horizontal_alignment != p_alignment) {
		if (horizontal_alignment == HORIZONTAL_ALIGNMENT_FILL || p_alignment == HORIZONTAL_ALIGNMENT_FILL) {
			dirty_lines = true;
		}
		horizontal_alignment = p_alignment;
		request_update();
	}
}

HorizontalAlignment TextMesh::get_horizontal_alignment() const {
	return horizontal_alignment;
}

void TextMesh::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	if (vertical_alignment != p_alignment) {
		vertical_alignment = p_alignment;
		request_update();
	}
}

VerticalAlignment TextMesh::get_vertical_alignment() const {
	return vertical_alignment;
}

void TextMesh::set_text(const String &p_string) {
	if (text != p_string) {
		text = p_string;
		xl_text = tr(text);
		dirty_text = true;
		request_update();
	}
}

String TextMesh::get_text() const {
	return text;
}

void TextMesh::_font_changed() {
	dirty_font = true;
	dirty_cache = true;
	callable_mp(static_cast<PrimitiveMesh *>(this), &PrimitiveMesh::request_update).call_deferred();
}

void TextMesh::set_font(const Ref<Font> &p_font) {
	if (font_override != p_font) {
		const Callable font_changed = callable_mp(this, &TextMesh::_font_changed);

		if (font_override.is_valid()) {
			font_override->disconnect_changed(font_changed);
		}
		font_override = p_font;
		dirty_font = true;
		dirty_cache = true;
		if (font_override.is_valid()) {
			font_override->connect_changed(font_changed);
		}
		request_update();
	}
}

Ref<Font> TextMesh::get_font() const {
	return font_override;
}

Ref<Font> TextMesh::_get_font_or_default() const {
	// Similar code taken from `FontVariation::_get_base_font_or_default`.

	if (font_override.is_valid()) {
		return font_override;
	}

	StringName theme_name = "font";
	Vector<StringName> theme_types;
	ThemeDB::get_singleton()->get_native_type_dependencies(get_class_name(), theme_types);

	ThemeContext *global_context = ThemeDB::get_singleton()->get_default_theme_context();
	Vector<Ref<Theme>> themes = global_context->get_themes();
	if (Engine::get_singleton()->is_editor_hint()) {
		themes.insert(0, ThemeDB::get_singleton()->get_project_theme());
	}

	for (const Ref<Theme> &theme : themes) {
		if (theme.is_null()) {
			continue;
		}

		for (const StringName &E : theme_types) {
			if (theme->has_font(theme_name, E)) {
				return theme->get_font(theme_name, E);
			}
		}
	}

	return global_context->get_fallback_theme()->get_font(theme_name, StringName());
}

void TextMesh::set_font_size(int p_size) {
	if (font_size != p_size) {
		font_size = CLAMP(p_size, 1, 127);
		dirty_font = true;
		dirty_cache = true;
		request_update();
	}
}

int TextMesh::get_font_size() const {
	return font_size;
}

void TextMesh::set_line_spacing(float p_line_spacing) {
	if (line_spacing != p_line_spacing) {
		line_spacing = p_line_spacing;
		request_update();
	}
}

float TextMesh::get_line_spacing() const {
	return line_spacing;
}

void TextMesh::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode != p_mode) {
		autowrap_mode = p_mode;
		dirty_lines = true;
		request_update();
	}
}

TextServer::AutowrapMode TextMesh::get_autowrap_mode() const {
	return autowrap_mode;
}

void TextMesh::set_justification_flags(BitField<TextServer::JustificationFlag> p_flags) {
	if (jst_flags != p_flags) {
		jst_flags = p_flags;
		dirty_lines = true;
		request_update();
	}
}

BitField<TextServer::JustificationFlag> TextMesh::get_justification_flags() const {
	return jst_flags;
}

void TextMesh::set_depth(real_t p_depth) {
	if (depth != p_depth) {
		depth = MAX(p_depth, 0.0);
		request_update();
	}
}

real_t TextMesh::get_depth() const {
	return depth;
}

void TextMesh::set_width(real_t p_width) {
	if (width != p_width) {
		width = p_width;
		dirty_lines = true;
		request_update();
	}
}

real_t TextMesh::get_width() const {
	return width;
}

void TextMesh::set_pixel_size(real_t p_amount) {
	if (pixel_size != p_amount) {
		pixel_size = CLAMP(p_amount, 0.0001, 128.0);
		dirty_cache = true;
		request_update();
	}
}

real_t TextMesh::get_pixel_size() const {
	return pixel_size;
}

void TextMesh::set_offset(const Point2 &p_offset) {
	if (lbl_offset != p_offset) {
		lbl_offset = p_offset;
		request_update();
	}
}

Point2 TextMesh::get_offset() const {
	return lbl_offset;
}

void TextMesh::set_curve_step(real_t p_step) {
	if (curve_step != p_step) {
		curve_step = CLAMP(p_step, 0.1, 10.0);
		dirty_cache = true;
		request_update();
	}
}

real_t TextMesh::get_curve_step() const {
	return curve_step;
}

void TextMesh::set_text_direction(TextServer::Direction p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		dirty_text = true;
		request_update();
	}
}

TextServer::Direction TextMesh::get_text_direction() const {
	return text_direction;
}

void TextMesh::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		dirty_text = true;
		request_update();
	}
}

String TextMesh::get_language() const {
	return language;
}

void TextMesh::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		st_parser = p_parser;
		dirty_text = true;
		request_update();
	}
}

TextServer::StructuredTextParser TextMesh::get_structured_text_bidi_override() const {
	return st_parser;
}

void TextMesh::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args != p_args) {
		st_args = p_args;
		dirty_text = true;
		request_update();
	}
}

Array TextMesh::get_structured_text_bidi_override_options() const {
	return st_args;
}

void TextMesh::set_uppercase(bool p_uppercase) {
	if (uppercase != p_uppercase) {
		uppercase = p_uppercase;
		dirty_text = true;
		request_update();
	}
}

bool TextMesh::is_uppercase() const {
	return uppercase;
}
