/**************************************************************************/
/*  mesh.cpp                                                              */
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

#include "mesh.h"

#include "core/math/convex_hull.h"
#include "core/templates/pair.h"
#include "scene/resources/surface_tool.h"

#ifndef _3D_DISABLED
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#endif // _3D_DISABLED

void MeshConvexDecompositionSettings::set_max_concavity(real_t p_max_concavity) {
	max_concavity = CLAMP(p_max_concavity, 0.001, 1.0);
}

real_t MeshConvexDecompositionSettings::get_max_concavity() const {
	return max_concavity;
}

void MeshConvexDecompositionSettings::set_symmetry_planes_clipping_bias(real_t p_symmetry_planes_clipping_bias) {
	symmetry_planes_clipping_bias = CLAMP(p_symmetry_planes_clipping_bias, 0.0, 1.0);
}

real_t MeshConvexDecompositionSettings::get_symmetry_planes_clipping_bias() const {
	return symmetry_planes_clipping_bias;
}

void MeshConvexDecompositionSettings::set_revolution_axes_clipping_bias(real_t p_revolution_axes_clipping_bias) {
	revolution_axes_clipping_bias = CLAMP(p_revolution_axes_clipping_bias, 0.0, 1.0);
}

real_t MeshConvexDecompositionSettings::get_revolution_axes_clipping_bias() const {
	return revolution_axes_clipping_bias;
}

void MeshConvexDecompositionSettings::set_min_volume_per_convex_hull(real_t p_min_volume_per_convex_hull) {
	min_volume_per_convex_hull = CLAMP(p_min_volume_per_convex_hull, 0.0001, 0.01);
}

real_t MeshConvexDecompositionSettings::get_min_volume_per_convex_hull() const {
	return min_volume_per_convex_hull;
}

void MeshConvexDecompositionSettings::set_resolution(uint32_t p_resolution) {
	resolution = p_resolution < 10'000 ? 10'000 : (p_resolution > 100'000 ? 100'000 : p_resolution);
}

uint32_t MeshConvexDecompositionSettings::get_resolution() const {
	return resolution;
}

void MeshConvexDecompositionSettings::set_max_num_vertices_per_convex_hull(uint32_t p_max_num_vertices_per_convex_hull) {
	max_num_vertices_per_convex_hull = p_max_num_vertices_per_convex_hull < 4 ? 4 : (p_max_num_vertices_per_convex_hull > 1024 ? 1024 : p_max_num_vertices_per_convex_hull);
}

uint32_t MeshConvexDecompositionSettings::get_max_num_vertices_per_convex_hull() const {
	return max_num_vertices_per_convex_hull;
}

void MeshConvexDecompositionSettings::set_plane_downsampling(uint32_t p_plane_downsampling) {
	plane_downsampling = p_plane_downsampling < 1 ? 1 : (p_plane_downsampling > 16 ? 16 : p_plane_downsampling);
}

uint32_t MeshConvexDecompositionSettings::get_plane_downsampling() const {
	return plane_downsampling;
}

void MeshConvexDecompositionSettings::set_convex_hull_downsampling(uint32_t p_convex_hull_downsampling) {
	convex_hull_downsampling = p_convex_hull_downsampling < 1 ? 1 : (p_convex_hull_downsampling > 16 ? 16 : p_convex_hull_downsampling);
}

uint32_t MeshConvexDecompositionSettings::get_convex_hull_downsampling() const {
	return convex_hull_downsampling;
}

void MeshConvexDecompositionSettings::set_normalize_mesh(bool p_normalize_mesh) {
	normalize_mesh = p_normalize_mesh;
}

bool MeshConvexDecompositionSettings::get_normalize_mesh() const {
	return normalize_mesh;
}

void MeshConvexDecompositionSettings::set_mode(Mode p_mode) {
	mode = p_mode;
}

MeshConvexDecompositionSettings::Mode MeshConvexDecompositionSettings::get_mode() const {
	return mode;
}

void MeshConvexDecompositionSettings::set_convex_hull_approximation(bool p_convex_hull_approximation) {
	convex_hull_approximation = p_convex_hull_approximation;
}

bool MeshConvexDecompositionSettings::get_convex_hull_approximation() const {
	return convex_hull_approximation;
}

void MeshConvexDecompositionSettings::set_max_convex_hulls(uint32_t p_max_convex_hulls) {
	max_convex_hulls = p_max_convex_hulls < 1 ? 1 : (p_max_convex_hulls > 32 ? 32 : p_max_convex_hulls);
}

uint32_t MeshConvexDecompositionSettings::get_max_convex_hulls() const {
	return max_convex_hulls;
}

void MeshConvexDecompositionSettings::set_project_hull_vertices(bool p_project_hull_vertices) {
	project_hull_vertices = p_project_hull_vertices;
}

bool MeshConvexDecompositionSettings::get_project_hull_vertices() const {
	return project_hull_vertices;
}

void MeshConvexDecompositionSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_concavity", "max_concavity"), &MeshConvexDecompositionSettings::set_max_concavity);
	ClassDB::bind_method(D_METHOD("get_max_concavity"), &MeshConvexDecompositionSettings::get_max_concavity);

	ClassDB::bind_method(D_METHOD("set_symmetry_planes_clipping_bias", "symmetry_planes_clipping_bias"), &MeshConvexDecompositionSettings::set_symmetry_planes_clipping_bias);
	ClassDB::bind_method(D_METHOD("get_symmetry_planes_clipping_bias"), &MeshConvexDecompositionSettings::get_symmetry_planes_clipping_bias);

	ClassDB::bind_method(D_METHOD("set_revolution_axes_clipping_bias", "revolution_axes_clipping_bias"), &MeshConvexDecompositionSettings::set_revolution_axes_clipping_bias);
	ClassDB::bind_method(D_METHOD("get_revolution_axes_clipping_bias"), &MeshConvexDecompositionSettings::get_revolution_axes_clipping_bias);

	ClassDB::bind_method(D_METHOD("set_min_volume_per_convex_hull", "min_volume_per_convex_hull"), &MeshConvexDecompositionSettings::set_min_volume_per_convex_hull);
	ClassDB::bind_method(D_METHOD("get_min_volume_per_convex_hull"), &MeshConvexDecompositionSettings::get_min_volume_per_convex_hull);

	ClassDB::bind_method(D_METHOD("set_resolution", "min_volume_per_convex_hull"), &MeshConvexDecompositionSettings::set_resolution);
	ClassDB::bind_method(D_METHOD("get_resolution"), &MeshConvexDecompositionSettings::get_resolution);

	ClassDB::bind_method(D_METHOD("set_max_num_vertices_per_convex_hull", "max_num_vertices_per_convex_hull"), &MeshConvexDecompositionSettings::set_max_num_vertices_per_convex_hull);
	ClassDB::bind_method(D_METHOD("get_max_num_vertices_per_convex_hull"), &MeshConvexDecompositionSettings::get_max_num_vertices_per_convex_hull);

	ClassDB::bind_method(D_METHOD("set_plane_downsampling", "plane_downsampling"), &MeshConvexDecompositionSettings::set_plane_downsampling);
	ClassDB::bind_method(D_METHOD("get_plane_downsampling"), &MeshConvexDecompositionSettings::get_plane_downsampling);

	ClassDB::bind_method(D_METHOD("set_convex_hull_downsampling", "convex_hull_downsampling"), &MeshConvexDecompositionSettings::set_convex_hull_downsampling);
	ClassDB::bind_method(D_METHOD("get_convex_hull_downsampling"), &MeshConvexDecompositionSettings::get_convex_hull_downsampling);

	ClassDB::bind_method(D_METHOD("set_normalize_mesh", "normalize_mesh"), &MeshConvexDecompositionSettings::set_normalize_mesh);
	ClassDB::bind_method(D_METHOD("get_normalize_mesh"), &MeshConvexDecompositionSettings::get_normalize_mesh);

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &MeshConvexDecompositionSettings::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &MeshConvexDecompositionSettings::get_mode);

	ClassDB::bind_method(D_METHOD("set_convex_hull_approximation", "convex_hull_approximation"), &MeshConvexDecompositionSettings::set_convex_hull_approximation);
	ClassDB::bind_method(D_METHOD("get_convex_hull_approximation"), &MeshConvexDecompositionSettings::get_convex_hull_approximation);

	ClassDB::bind_method(D_METHOD("set_max_convex_hulls", "max_convex_hulls"), &MeshConvexDecompositionSettings::set_max_convex_hulls);
	ClassDB::bind_method(D_METHOD("get_max_convex_hulls"), &MeshConvexDecompositionSettings::get_max_convex_hulls);

	ClassDB::bind_method(D_METHOD("set_project_hull_vertices", "project_hull_vertices"), &MeshConvexDecompositionSettings::set_project_hull_vertices);
	ClassDB::bind_method(D_METHOD("get_project_hull_vertices"), &MeshConvexDecompositionSettings::get_project_hull_vertices);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_concavity", PROPERTY_HINT_RANGE, "0.001,1.0,0.001"), "set_max_concavity", "get_max_concavity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "symmetry_planes_clipping_bias", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_symmetry_planes_clipping_bias", "get_symmetry_planes_clipping_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "revolution_axes_clipping_bias", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_revolution_axes_clipping_bias", "get_revolution_axes_clipping_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_volume_per_convex_hull", PROPERTY_HINT_RANGE, "0.0001,0.01,0.0001"), "set_min_volume_per_convex_hull", "get_min_volume_per_convex_hull");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "resolution"), "set_resolution", "get_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_num_vertices_per_convex_hull"), "set_max_num_vertices_per_convex_hull", "get_max_num_vertices_per_convex_hull");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "plane_downsampling", PROPERTY_HINT_RANGE, "1,16,1"), "set_plane_downsampling", "get_plane_downsampling");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "convex_hull_downsampling", PROPERTY_HINT_RANGE, "1,16,1"), "set_convex_hull_downsampling", "get_convex_hull_downsampling");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalize_mesh"), "set_normalize_mesh", "get_normalize_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Voxel,Tetrahedron"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "convex_hull_approximation"), "set_convex_hull_approximation", "get_convex_hull_approximation");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_convex_hulls"), "set_max_convex_hulls", "get_max_convex_hulls");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "project_hull_vertices"), "set_project_hull_vertices", "get_project_hull_vertices");

	BIND_ENUM_CONSTANT(CONVEX_DECOMPOSITION_MODE_VOXEL);
	BIND_ENUM_CONSTANT(CONVEX_DECOMPOSITION_MODE_TETRAHEDRON);
}

Mesh::ConvexDecompositionFunc Mesh::convex_decomposition_function = nullptr;

int Mesh::get_surface_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_surface_count, ret);
	return ret;
}

int Mesh::surface_get_array_len(int p_idx) const {
	int ret = 0;
	GDVIRTUAL_CALL(_surface_get_array_len, p_idx, ret);
	return ret;
}

int Mesh::surface_get_array_index_len(int p_idx) const {
	int ret = 0;
	GDVIRTUAL_CALL(_surface_get_array_index_len, p_idx, ret);
	return ret;
}

Array Mesh::surface_get_arrays(int p_surface) const {
	Array ret;
	GDVIRTUAL_CALL(_surface_get_arrays, p_surface, ret);
	return ret;
}

TypedArray<Array> Mesh::surface_get_blend_shape_arrays(int p_surface) const {
	TypedArray<Array> ret;
	GDVIRTUAL_CALL(_surface_get_blend_shape_arrays, p_surface, ret);
	return ret;
}

Dictionary Mesh::surface_get_lods(int p_surface) const {
	Dictionary ret;
	GDVIRTUAL_CALL(_surface_get_lods, p_surface, ret);
	return ret;
}

BitField<Mesh::ArrayFormat> Mesh::surface_get_format(int p_idx) const {
	uint32_t ret = 0;
	GDVIRTUAL_CALL(_surface_get_format, p_idx, ret);
	return ret;
}

Mesh::PrimitiveType Mesh::surface_get_primitive_type(int p_idx) const {
	uint32_t ret = PRIMITIVE_MAX;
	GDVIRTUAL_CALL(_surface_get_primitive_type, p_idx, ret);
	return (Mesh::PrimitiveType)ret;
}

void Mesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {
	GDVIRTUAL_CALL(_surface_set_material, p_idx, p_material);
}

Ref<Material> Mesh::surface_get_material(int p_idx) const {
	Ref<Material> ret;
	GDVIRTUAL_CALL(_surface_get_material, p_idx, ret);
	return ret;
}

int Mesh::get_blend_shape_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_blend_shape_count, ret);
	return ret;
}

StringName Mesh::get_blend_shape_name(int p_index) const {
	StringName ret;
	GDVIRTUAL_CALL(_get_blend_shape_name, p_index, ret);
	return ret;
}

void Mesh::set_blend_shape_name(int p_index, const StringName &p_name) {
	GDVIRTUAL_CALL(_set_blend_shape_name, p_index, p_name);
}

AABB Mesh::get_aabb() const {
	AABB ret;
	GDVIRTUAL_CALL(_get_aabb, ret);
	return ret;
}

Ref<TriangleMesh> Mesh::generate_triangle_mesh() const {
	if (triangle_mesh.is_valid()) {
		return triangle_mesh;
	}

	int faces_size = 0;

	for (int i = 0; i < get_surface_count(); i++) {
		switch (surface_get_primitive_type(i)) {
			case PRIMITIVE_TRIANGLES: {
				int len = (surface_get_format(i) & ARRAY_FORMAT_INDEX) ? surface_get_array_index_len(i) : surface_get_array_len(i);
				// Don't error if zero, it's valid (we'll just skip it later).
				ERR_CONTINUE_MSG((len % 3) != 0, vformat("Ignoring surface %d, incorrect %s count: %d (for PRIMITIVE_TRIANGLES).", i, (surface_get_format(i) & ARRAY_FORMAT_INDEX) ? "index" : "vertex", len));
				faces_size += len;
			} break;
			case PRIMITIVE_TRIANGLE_STRIP: {
				int len = (surface_get_format(i) & ARRAY_FORMAT_INDEX) ? surface_get_array_index_len(i) : surface_get_array_len(i);
				// Don't error if zero, it's valid (we'll just skip it later).
				ERR_CONTINUE_MSG(len != 0 && len < 3, vformat("Ignoring surface %d, incorrect %s count: %d (for PRIMITIVE_TRIANGLE_STRIP).", i, (surface_get_format(i) & ARRAY_FORMAT_INDEX) ? "index" : "vertex", len));
				faces_size += (len == 0) ? 0 : (len - 2) * 3;
			} break;
			default: {
			} break;
		}
	}

	if (faces_size == 0) {
		return triangle_mesh;
	}

	Vector<Vector3> faces;
	faces.resize(faces_size);
	Vector<int32_t> surface_indices;
	surface_indices.resize(faces_size / 3);
	Vector3 *facesw = faces.ptrw();
	int32_t *surface_indicesw = surface_indices.ptrw();

	int widx = 0;

	for (int i = 0; i < get_surface_count(); i++) {
		Mesh::PrimitiveType primitive = surface_get_primitive_type(i);
		if (primitive != PRIMITIVE_TRIANGLES && primitive != PRIMITIVE_TRIANGLE_STRIP) {
			continue;
		}
		int len = (surface_get_format(i) & ARRAY_FORMAT_INDEX) ? surface_get_array_index_len(i) : surface_get_array_len(i);
		if ((primitive == PRIMITIVE_TRIANGLES && (len == 0 || (len % 3) != 0)) ||
				(primitive == PRIMITIVE_TRIANGLE_STRIP && len < 3) ||
				(surface_get_format(i) & ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY)) {
			// Error was already shown, just skip (including zero).
			continue;
		}

		Array a = surface_get_arrays(i);
		ERR_FAIL_COND_V(a.is_empty(), Ref<TriangleMesh>());

		int vc = surface_get_array_len(i);
		Vector<Vector3> vertices = a[ARRAY_VERTEX];
		ERR_FAIL_COND_V(vertices.is_empty(), Ref<TriangleMesh>());
		const Vector3 *vr = vertices.ptr();

		int32_t from_index = widx / 3;

		if (surface_get_format(i) & ARRAY_FORMAT_INDEX) {
			int ic = surface_get_array_index_len(i);
			Vector<int> indices = a[ARRAY_INDEX];
			const int *ir = indices.ptr();

			if (primitive == PRIMITIVE_TRIANGLES) {
				for (int j = 0; j < ic; j++) {
					int index = ir[j];
					ERR_FAIL_COND_V(index >= vc, Ref<TriangleMesh>());
					facesw[widx++] = vr[index];
				}
			} else { // PRIMITIVE_TRIANGLE_STRIP
				for (int j = 2; j < ic; j++) {
					facesw[widx++] = vr[ir[j - 2]];
					facesw[widx++] = vr[ir[j - 1]];
					facesw[widx++] = vr[ir[j]];
				}
			}

		} else {
			if (primitive == PRIMITIVE_TRIANGLES) {
				for (int j = 0; j < vc; j++) {
					facesw[widx++] = vr[j];
				}
			} else { // PRIMITIVE_TRIANGLE_STRIP
				for (int j = 2; j < vc; j++) {
					facesw[widx++] = vr[j - 2];
					facesw[widx++] = vr[j - 1];
					facesw[widx++] = vr[j];
				}
			}
		}

		int32_t to_index = widx / 3;

		for (int j = from_index; j < to_index; j++) {
			surface_indicesw[j] = i;
		}
	}

	triangle_mesh.instantiate();
	triangle_mesh->create(faces);

	return triangle_mesh;
}

Ref<TriangleMesh> Mesh::generate_surface_triangle_mesh(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, get_surface_count(), Ref<TriangleMesh>());

	if (surface_triangle_meshes.size() != get_surface_count()) {
		surface_triangle_meshes.resize(get_surface_count());
	}

	if (surface_triangle_meshes[p_surface].is_valid()) {
		return surface_triangle_meshes[p_surface];
	}

	int facecount = 0;

	if (surface_get_primitive_type(p_surface) != PRIMITIVE_TRIANGLES) {
		return Ref<TriangleMesh>();
	}

	if (surface_get_format(p_surface) & ARRAY_FORMAT_INDEX) {
		facecount += surface_get_array_index_len(p_surface);
	} else {
		facecount += surface_get_array_len(p_surface);
	}

	Vector<Vector3> faces;
	faces.resize(facecount);
	Vector3 *facesw = faces.ptrw();

	Array a = surface_get_arrays(p_surface);
	ERR_FAIL_COND_V(a.is_empty(), Ref<TriangleMesh>());

	int vc = surface_get_array_len(p_surface);
	Vector<Vector3> vertices = a[ARRAY_VERTEX];
	const Vector3 *vr = vertices.ptr();
	int widx = 0;

	if (surface_get_format(p_surface) & ARRAY_FORMAT_INDEX) {
		int ic = surface_get_array_index_len(p_surface);
		Vector<int> indices = a[ARRAY_INDEX];
		const int *ir = indices.ptr();

		for (int j = 0; j < ic; j++) {
			int index = ir[j];
			facesw[widx++] = vr[index];
		}

	} else {
		for (int j = 0; j < vc; j++) {
			facesw[widx++] = vr[j];
		}
	}

	Ref<TriangleMesh> tr_mesh = Ref<TriangleMesh>(memnew(TriangleMesh));
	tr_mesh->create(faces);
	surface_triangle_meshes.set(p_surface, tr_mesh);

	return tr_mesh;
}

void Mesh::generate_debug_mesh_lines(Vector<Vector3> &r_lines) {
	if (debug_lines.size() > 0) {
		r_lines = debug_lines;
		return;
	}

	Ref<TriangleMesh> tm = generate_triangle_mesh();
	if (tm.is_null()) {
		return;
	}

	Vector<int> triangle_indices;
	tm->get_indices(&triangle_indices);
	const int triangles_num = tm->get_triangles().size();
	Vector<Vector3> vertices = tm->get_vertices();

	debug_lines.resize(tm->get_triangles().size() * 6); // 3 lines x 2 points each line

	const int *ind_r = triangle_indices.ptr();
	const Vector3 *ver_r = vertices.ptr();
	for (int j = 0, x = 0, i = 0; i < triangles_num; j += 6, x += 3, ++i) {
		// Triangle line 1
		debug_lines.write[j + 0] = ver_r[ind_r[x + 0]];
		debug_lines.write[j + 1] = ver_r[ind_r[x + 1]];

		// Triangle line 2
		debug_lines.write[j + 2] = ver_r[ind_r[x + 1]];
		debug_lines.write[j + 3] = ver_r[ind_r[x + 2]];

		// Triangle line 3
		debug_lines.write[j + 4] = ver_r[ind_r[x + 2]];
		debug_lines.write[j + 5] = ver_r[ind_r[x + 0]];
	}

	r_lines = debug_lines;
}

void Mesh::generate_debug_mesh_indices(Vector<Vector3> &r_points) {
	Ref<TriangleMesh> tm = generate_triangle_mesh();
	if (tm.is_null()) {
		return;
	}

	Vector<Vector3> vertices = tm->get_vertices();

	int vertices_size = vertices.size();
	r_points.resize(vertices_size);
	for (int i = 0; i < vertices_size; ++i) {
		r_points.write[i] = vertices[i];
	}
}

Vector<Vector3> Mesh::_get_faces() const {
	return Variant(get_faces());
}

Vector<Face3> Mesh::get_faces() const {
	Ref<TriangleMesh> tm = generate_triangle_mesh();
	if (tm.is_valid()) {
		return tm->get_faces();
	}
	return Vector<Face3>();
}

Vector<Face3> Mesh::get_surface_faces(int p_surface) const {
	Ref<TriangleMesh> tm = generate_surface_triangle_mesh(p_surface);
	if (tm.is_valid()) {
		return tm->get_faces();
	}
	return Vector<Face3>();
}

#ifndef _3D_DISABLED
Ref<ConvexPolygonShape3D> Mesh::create_convex_shape(bool p_clean, bool p_simplify) const {
	if (p_simplify) {
		Ref<MeshConvexDecompositionSettings> settings = Ref<MeshConvexDecompositionSettings>();
		settings.instantiate();
		settings->set_max_convex_hulls(1);
		settings->set_max_concavity(1.0);
		Vector<Ref<Shape3D>> decomposed = convex_decompose(settings);
		if (decomposed.size() == 1) {
			return decomposed[0];
		} else {
			ERR_PRINT("Convex shape simplification failed, falling back to simpler process.");
		}
	}

	Vector<Vector3> vertices;
	for (int i = 0; i < get_surface_count(); i++) {
		Array a = surface_get_arrays(i);
		ERR_FAIL_COND_V(a.is_empty(), Ref<ConvexPolygonShape3D>());
		Vector<Vector3> v = a[ARRAY_VERTEX];
		vertices.append_array(v);
	}

	Ref<ConvexPolygonShape3D> shape = memnew(ConvexPolygonShape3D);

	if (p_clean) {
		Geometry3D::MeshData md;
		Error err = ConvexHullComputer::convex_hull(vertices, md);
		if (err == OK) {
			shape->set_points(md.vertices);
			return shape;
		} else {
			ERR_PRINT("Convex shape cleaning failed, falling back to simpler process.");
		}
	}

	shape->set_points(vertices);
	return shape;
}

Ref<ConcavePolygonShape3D> Mesh::create_trimesh_shape() const {
	Vector<Face3> faces = get_faces();
	if (faces.size() == 0) {
		return Ref<ConcavePolygonShape3D>();
	}

	Vector<Vector3> face_points;
	face_points.resize(faces.size() * 3);

	for (int i = 0; i < face_points.size(); i += 3) {
		Face3 f = faces.get(i / 3);
		face_points.set(i, f.vertex[0]);
		face_points.set(i + 1, f.vertex[1]);
		face_points.set(i + 2, f.vertex[2]);
	}

	Ref<ConcavePolygonShape3D> shape = memnew(ConcavePolygonShape3D);
	shape->set_faces(face_points);
	return shape;
}
#endif // _3D_DISABLED

Ref<Mesh> Mesh::create_outline(float p_margin) const {
	Array arrays;
	int index_accum = 0;
	for (int i = 0; i < get_surface_count(); i++) {
		if (surface_get_primitive_type(i) != PRIMITIVE_TRIANGLES) {
			continue;
		}

		Array a = surface_get_arrays(i);
		ERR_FAIL_COND_V(a.is_empty(), Ref<ArrayMesh>());

		if (i == 0) {
			arrays = a;
			Vector<Vector3> v = a[ARRAY_VERTEX];
			index_accum += v.size();
		} else {
			int vcount = 0;
			for (int j = 0; j < arrays.size(); j++) {
				if (arrays[j].get_type() == Variant::NIL || a[j].get_type() == Variant::NIL) {
					//mismatch, do not use
					arrays[j] = Variant();
					continue;
				}

				switch (j) {
					case ARRAY_VERTEX:
					case ARRAY_NORMAL: {
						Vector<Vector3> dst = arrays[j];
						Vector<Vector3> src = a[j];
						if (j == ARRAY_VERTEX) {
							vcount = src.size();
						}
						if (dst.size() == 0 || src.size() == 0) {
							arrays[j] = Variant();
							continue;
						}
						dst.append_array(src);
						arrays[j] = dst;
					} break;
					case ARRAY_TANGENT:
					case ARRAY_BONES:
					case ARRAY_WEIGHTS: {
						Vector<real_t> dst = arrays[j];
						Vector<real_t> src = a[j];
						if (dst.size() == 0 || src.size() == 0) {
							arrays[j] = Variant();
							continue;
						}
						dst.append_array(src);
						arrays[j] = dst;

					} break;
					case ARRAY_COLOR: {
						Vector<Color> dst = arrays[j];
						Vector<Color> src = a[j];
						if (dst.size() == 0 || src.size() == 0) {
							arrays[j] = Variant();
							continue;
						}
						dst.append_array(src);
						arrays[j] = dst;

					} break;
					case ARRAY_TEX_UV:
					case ARRAY_TEX_UV2: {
						Vector<Vector2> dst = arrays[j];
						Vector<Vector2> src = a[j];
						if (dst.size() == 0 || src.size() == 0) {
							arrays[j] = Variant();
							continue;
						}
						dst.append_array(src);
						arrays[j] = dst;

					} break;
					case ARRAY_INDEX: {
						Vector<int> dst = arrays[j];
						Vector<int> src = a[j];
						if (dst.size() == 0 || src.size() == 0) {
							arrays[j] = Variant();
							continue;
						}
						{
							int ss = src.size();
							int *w = src.ptrw();
							for (int k = 0; k < ss; k++) {
								w[k] += index_accum;
							}
						}
						dst.append_array(src);
						arrays[j] = dst;
						index_accum += vcount;

					} break;
				}
			}
		}
	}

	ERR_FAIL_COND_V(arrays.size() != ARRAY_MAX, Ref<ArrayMesh>());

	{
		int *ir = nullptr;
		Vector<int> indices = arrays[ARRAY_INDEX];
		bool has_indices = false;
		Vector<Vector3> vertices = arrays[ARRAY_VERTEX];
		int vc = vertices.size();
		ERR_FAIL_COND_V(!vc, Ref<ArrayMesh>());
		Vector3 *r = vertices.ptrw();

		if (indices.size()) {
			ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ArrayMesh>());
			vc = indices.size();
			ir = indices.ptrw();
			has_indices = true;
		} else {
			// Ensure there are enough vertices to construct at least one triangle.
			ERR_FAIL_COND_V(vertices.size() % 3 != 0, Ref<ArrayMesh>());
		}

		HashMap<Vector3, Vector3> normal_accum;

		//fill normals with triangle normals
		for (int i = 0; i < vc; i += 3) {
			Vector3 t[3];

			if (has_indices) {
				t[0] = r[ir[i + 0]];
				t[1] = r[ir[i + 1]];
				t[2] = r[ir[i + 2]];
			} else {
				t[0] = r[i + 0];
				t[1] = r[i + 1];
				t[2] = r[i + 2];
			}

			Vector3 n = Plane(t[0], t[1], t[2]).normal;

			for (int j = 0; j < 3; j++) {
				HashMap<Vector3, Vector3>::Iterator E = normal_accum.find(t[j]);
				if (!E) {
					normal_accum[t[j]] = n;
				} else {
					float d = n.dot(E->value);
					if (d < 1.0) {
						E->value += n * (1.0 - d);
					}
					//E->get()+=n;
				}
			}
		}

		//normalize

		for (KeyValue<Vector3, Vector3> &E : normal_accum) {
			E.value.normalize();
		}

		//displace normals
		int vc2 = vertices.size();

		for (int i = 0; i < vc2; i++) {
			Vector3 t = r[i];

			HashMap<Vector3, Vector3>::Iterator E = normal_accum.find(t);
			ERR_CONTINUE(!E);

			t += E->value * p_margin;
			r[i] = t;
		}

		arrays[ARRAY_VERTEX] = vertices;

		if (!has_indices) {
			Vector<int> new_indices;
			new_indices.resize(vertices.size());
			int *iw = new_indices.ptrw();

			for (int j = 0; j < vc2; j += 3) {
				iw[j] = j;
				iw[j + 1] = j + 2;
				iw[j + 2] = j + 1;
			}

			arrays[ARRAY_INDEX] = new_indices;

		} else {
			for (int j = 0; j < vc; j += 3) {
				SWAP(ir[j + 1], ir[j + 2]);
			}
			arrays[ARRAY_INDEX] = indices;
		}
	}

	Ref<ArrayMesh> newmesh = memnew(ArrayMesh);
	newmesh->add_surface_from_arrays(PRIMITIVE_TRIANGLES, arrays);
	return newmesh;
}

void Mesh::set_lightmap_size_hint(const Size2i &p_size) {
	lightmap_size_hint = p_size;
}

Size2i Mesh::get_lightmap_size_hint() const {
	return lightmap_size_hint;
}

Ref<Resource> Mesh::create_placeholder() const {
	Ref<PlaceholderMesh> placeholder;
	placeholder.instantiate();
	placeholder->set_aabb(get_aabb());
	return placeholder;
}

void Mesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_lightmap_size_hint", "size"), &Mesh::set_lightmap_size_hint);
	ClassDB::bind_method(D_METHOD("get_lightmap_size_hint"), &Mesh::get_lightmap_size_hint);
	ClassDB::bind_method(D_METHOD("get_aabb"), &Mesh::get_aabb);
	ClassDB::bind_method(D_METHOD("get_faces"), &Mesh::_get_faces);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "lightmap_size_hint"), "set_lightmap_size_hint", "get_lightmap_size_hint");

	ClassDB::bind_method(D_METHOD("get_surface_count"), &Mesh::get_surface_count);
	ClassDB::bind_method(D_METHOD("surface_get_arrays", "surf_idx"), &Mesh::surface_get_arrays);
	ClassDB::bind_method(D_METHOD("surface_get_blend_shape_arrays", "surf_idx"), &Mesh::surface_get_blend_shape_arrays);
	ClassDB::bind_method(D_METHOD("surface_set_material", "surf_idx", "material"), &Mesh::surface_set_material);
	ClassDB::bind_method(D_METHOD("surface_get_material", "surf_idx"), &Mesh::surface_get_material);
	ClassDB::bind_method(D_METHOD("create_placeholder"), &Mesh::create_placeholder);

	BIND_ENUM_CONSTANT(PRIMITIVE_POINTS);
	BIND_ENUM_CONSTANT(PRIMITIVE_LINES);
	BIND_ENUM_CONSTANT(PRIMITIVE_LINE_STRIP);
	BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLES);
	BIND_ENUM_CONSTANT(PRIMITIVE_TRIANGLE_STRIP);

	BIND_ENUM_CONSTANT(ARRAY_VERTEX);
	BIND_ENUM_CONSTANT(ARRAY_NORMAL);
	BIND_ENUM_CONSTANT(ARRAY_TANGENT);
	BIND_ENUM_CONSTANT(ARRAY_COLOR);
	BIND_ENUM_CONSTANT(ARRAY_TEX_UV);
	BIND_ENUM_CONSTANT(ARRAY_TEX_UV2);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM0);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM1);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM2);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM3);
	BIND_ENUM_CONSTANT(ARRAY_BONES);
	BIND_ENUM_CONSTANT(ARRAY_WEIGHTS);
	BIND_ENUM_CONSTANT(ARRAY_INDEX);
	BIND_ENUM_CONSTANT(ARRAY_MAX);

	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA8_UNORM);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA8_SNORM);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RG_HALF);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA_HALF);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_R_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RG_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGB_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_RGBA_FLOAT);
	BIND_ENUM_CONSTANT(ARRAY_CUSTOM_MAX);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_VERTEX);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_NORMAL);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TANGENT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_COLOR);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TEX_UV);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_TEX_UV2);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM0);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM1);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM2);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM3);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_BONES);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_WEIGHTS);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_INDEX);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_BLEND_SHAPE_MASK);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_BASE);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_BITS);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM0_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM1_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM2_SHIFT);
	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM3_SHIFT);

	BIND_BITFIELD_FLAG(ARRAY_FORMAT_CUSTOM_MASK);
	BIND_BITFIELD_FLAG(ARRAY_COMPRESS_FLAGS_BASE);

	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_2D_VERTICES);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_DYNAMIC_UPDATE);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USE_8_BONE_WEIGHTS);
	BIND_BITFIELD_FLAG(ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY);

	BIND_BITFIELD_FLAG(ARRAY_FLAG_COMPRESS_ATTRIBUTES);

	BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_NORMALIZED);
	BIND_ENUM_CONSTANT(BLEND_SHAPE_MODE_RELATIVE);

	GDVIRTUAL_BIND(_get_surface_count)
	GDVIRTUAL_BIND(_surface_get_array_len, "index")
	GDVIRTUAL_BIND(_surface_get_array_index_len, "index")
	GDVIRTUAL_BIND(_surface_get_arrays, "index")
	GDVIRTUAL_BIND(_surface_get_blend_shape_arrays, "index")
	GDVIRTUAL_BIND(_surface_get_lods, "index")
	GDVIRTUAL_BIND(_surface_get_format, "index")
	GDVIRTUAL_BIND(_surface_get_primitive_type, "index")
	GDVIRTUAL_BIND(_surface_set_material, "index", "material")
	GDVIRTUAL_BIND(_surface_get_material, "index")
	GDVIRTUAL_BIND(_get_blend_shape_count)
	GDVIRTUAL_BIND(_get_blend_shape_name, "index")
	GDVIRTUAL_BIND(_set_blend_shape_name, "index", "name")
	GDVIRTUAL_BIND(_get_aabb)
}

void Mesh::clear_cache() const {
	triangle_mesh.unref();
	debug_lines.clear();
}

#ifndef _3D_DISABLED
Vector<Ref<Shape3D>> Mesh::convex_decompose(const Ref<MeshConvexDecompositionSettings> &p_settings) const {
	ERR_FAIL_NULL_V(convex_decomposition_function, Vector<Ref<Shape3D>>());

	Ref<TriangleMesh> tm = generate_triangle_mesh();
	ERR_FAIL_COND_V(tm.is_null(), Vector<Ref<Shape3D>>());

	const Vector<TriangleMesh::Triangle> &triangles = tm->get_triangles();
	int triangle_count = triangles.size();

	Vector<uint32_t> indices;
	{
		indices.resize(triangle_count * 3);
		uint32_t *w = indices.ptrw();
		for (int i = 0; i < triangle_count; i++) {
			for (int j = 0; j < 3; j++) {
				w[i * 3 + j] = triangles[i].indices[j];
			}
		}
	}

	const Vector<Vector3> &vertices = tm->get_vertices();
	int vertex_count = vertices.size();

	Vector<Vector<Vector3>> decomposed = convex_decomposition_function((real_t *)vertices.ptr(), vertex_count, indices.ptr(), triangle_count, p_settings, nullptr);

	Vector<Ref<Shape3D>> ret;

	for (int i = 0; i < decomposed.size(); i++) {
		Ref<ConvexPolygonShape3D> shape;
		shape.instantiate();
		shape->set_points(decomposed[i]);
		ret.push_back(shape);
	}

	return ret;
}
#endif // _3D_DISABLED

int Mesh::get_builtin_bind_pose_count() const {
	return 0;
}

Transform3D Mesh::get_builtin_bind_pose(int p_index) const {
	return Transform3D();
}

Mesh::Mesh() {
}

enum OldArrayType {
	OLD_ARRAY_VERTEX,
	OLD_ARRAY_NORMAL,
	OLD_ARRAY_TANGENT,
	OLD_ARRAY_COLOR,
	OLD_ARRAY_TEX_UV,
	OLD_ARRAY_TEX_UV2,
	OLD_ARRAY_BONES,
	OLD_ARRAY_WEIGHTS,
	OLD_ARRAY_INDEX,
	OLD_ARRAY_MAX,
};

enum OldArrayFormat {
	/* OLD_ARRAY FORMAT FLAGS */
	OLD_ARRAY_FORMAT_VERTEX = 1 << OLD_ARRAY_VERTEX, // mandatory
	OLD_ARRAY_FORMAT_NORMAL = 1 << OLD_ARRAY_NORMAL,
	OLD_ARRAY_FORMAT_TANGENT = 1 << OLD_ARRAY_TANGENT,
	OLD_ARRAY_FORMAT_COLOR = 1 << OLD_ARRAY_COLOR,
	OLD_ARRAY_FORMAT_TEX_UV = 1 << OLD_ARRAY_TEX_UV,
	OLD_ARRAY_FORMAT_TEX_UV2 = 1 << OLD_ARRAY_TEX_UV2,
	OLD_ARRAY_FORMAT_BONES = 1 << OLD_ARRAY_BONES,
	OLD_ARRAY_FORMAT_WEIGHTS = 1 << OLD_ARRAY_WEIGHTS,
	OLD_ARRAY_FORMAT_INDEX = 1 << OLD_ARRAY_INDEX,

	OLD_ARRAY_COMPRESS_BASE = (OLD_ARRAY_INDEX + 1),
	OLD_ARRAY_COMPRESS_VERTEX = 1 << (OLD_ARRAY_VERTEX + OLD_ARRAY_COMPRESS_BASE), // mandatory
	OLD_ARRAY_COMPRESS_NORMAL = 1 << (OLD_ARRAY_NORMAL + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_TANGENT = 1 << (OLD_ARRAY_TANGENT + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_COLOR = 1 << (OLD_ARRAY_COLOR + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_TEX_UV = 1 << (OLD_ARRAY_TEX_UV + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_TEX_UV2 = 1 << (OLD_ARRAY_TEX_UV2 + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_BONES = 1 << (OLD_ARRAY_BONES + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_WEIGHTS = 1 << (OLD_ARRAY_WEIGHTS + OLD_ARRAY_COMPRESS_BASE),
	OLD_ARRAY_COMPRESS_INDEX = 1 << (OLD_ARRAY_INDEX + OLD_ARRAY_COMPRESS_BASE),

	OLD_ARRAY_FLAG_USE_2D_VERTICES = OLD_ARRAY_COMPRESS_INDEX << 1,
	OLD_ARRAY_FLAG_USE_16_BIT_BONES = OLD_ARRAY_COMPRESS_INDEX << 2,
	OLD_ARRAY_FLAG_USE_DYNAMIC_UPDATE = OLD_ARRAY_COMPRESS_INDEX << 3,
	OLD_ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION = OLD_ARRAY_COMPRESS_INDEX << 4,
};

#ifndef DISABLE_DEPRECATED
static Array _convert_old_array(const Array &p_old) {
	Array new_array;
	new_array.resize(Mesh::ARRAY_MAX);
	new_array[Mesh::ARRAY_VERTEX] = p_old[OLD_ARRAY_VERTEX];
	new_array[Mesh::ARRAY_NORMAL] = p_old[OLD_ARRAY_NORMAL];
	new_array[Mesh::ARRAY_TANGENT] = p_old[OLD_ARRAY_TANGENT];
	new_array[Mesh::ARRAY_COLOR] = p_old[OLD_ARRAY_COLOR];
	new_array[Mesh::ARRAY_TEX_UV] = p_old[OLD_ARRAY_TEX_UV];
	new_array[Mesh::ARRAY_TEX_UV2] = p_old[OLD_ARRAY_TEX_UV2];
	new_array[Mesh::ARRAY_BONES] = p_old[OLD_ARRAY_BONES];
	new_array[Mesh::ARRAY_WEIGHTS] = p_old[OLD_ARRAY_WEIGHTS];
	new_array[Mesh::ARRAY_INDEX] = p_old[OLD_ARRAY_INDEX];
	return new_array;
}

static Mesh::PrimitiveType _old_primitives[7] = {
	Mesh::PRIMITIVE_POINTS,
	Mesh::PRIMITIVE_LINES,
	Mesh::PRIMITIVE_LINE_STRIP,
	Mesh::PRIMITIVE_LINES,
	Mesh::PRIMITIVE_TRIANGLES,
	Mesh::PRIMITIVE_TRIANGLE_STRIP,
	Mesh::PRIMITIVE_TRIANGLE_STRIP
};
#endif // DISABLE_DEPRECATED

void _fix_array_compatibility(const Vector<uint8_t> &p_src, uint64_t p_old_format, uint64_t p_new_format, uint32_t p_elements, Vector<uint8_t> &vertex_data, Vector<uint8_t> &attribute_data, Vector<uint8_t> &skin_data) {
	uint32_t dst_vertex_stride;
	uint32_t dst_normal_tangent_stride;
	uint32_t dst_attribute_stride;
	uint32_t dst_skin_stride;
	uint32_t dst_offsets[Mesh::ARRAY_MAX];
	RenderingServer::get_singleton()->mesh_surface_make_offsets_from_format(p_new_format & (~RS::ARRAY_FORMAT_INDEX), p_elements, 0, dst_offsets, dst_vertex_stride, dst_normal_tangent_stride, dst_attribute_stride, dst_skin_stride);

	vertex_data.resize((dst_vertex_stride + dst_normal_tangent_stride) * p_elements);
	attribute_data.resize(dst_attribute_stride * p_elements);
	skin_data.resize(dst_skin_stride * p_elements);

	uint8_t *dst_vertex_ptr = vertex_data.ptrw();
	uint8_t *dst_attribute_ptr = attribute_data.ptrw();
	uint8_t *dst_skin_ptr = skin_data.ptrw();

	const uint8_t *src_vertex_ptr = p_src.ptr();
	uint32_t src_vertex_stride = p_src.size() / p_elements;

	uint32_t src_offset = 0;
	for (uint32_t j = 0; j < OLD_ARRAY_INDEX; j++) {
		if (!(p_old_format & (1ULL << j))) {
			continue;
		}
		switch (j) {
			case OLD_ARRAY_VERTEX: {
				if (p_old_format & OLD_ARRAY_FLAG_USE_2D_VERTICES) {
					if (p_old_format & OLD_ARRAY_COMPRESS_VERTEX) {
						for (uint32_t i = 0; i < p_elements; i++) {
							const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride];
							float *dst = (float *)&dst_vertex_ptr[i * dst_vertex_stride];
							dst[0] = Math::half_to_float(src[0]);
							dst[1] = Math::half_to_float(src[1]);
						}
						src_offset += sizeof(uint16_t) * 2;
					} else {
						for (uint32_t i = 0; i < p_elements; i++) {
							const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride];
							float *dst = (float *)&dst_vertex_ptr[i * dst_vertex_stride];
							dst[0] = src[0];
							dst[1] = src[1];
						}
						src_offset += sizeof(float) * 2;
					}
				} else {
					if (p_old_format & OLD_ARRAY_COMPRESS_VERTEX) {
						for (uint32_t i = 0; i < p_elements; i++) {
							const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride];
							float *dst = (float *)&dst_vertex_ptr[i * dst_vertex_stride];
							dst[0] = Math::half_to_float(src[0]);
							dst[1] = Math::half_to_float(src[1]);
							dst[2] = Math::half_to_float(src[2]);
						}
						src_offset += sizeof(uint16_t) * 4; //+pad
					} else {
						for (uint32_t i = 0; i < p_elements; i++) {
							const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride];
							float *dst = (float *)&dst_vertex_ptr[i * dst_vertex_stride];
							dst[0] = src[0];
							dst[1] = src[1];
							dst[2] = src[2];
						}
						src_offset += sizeof(float) * 3;
					}
				}
			} break;
			case OLD_ARRAY_NORMAL: {
				if (p_old_format & OLD_ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					if ((p_old_format & OLD_ARRAY_COMPRESS_NORMAL) && (p_old_format & OLD_ARRAY_FORMAT_TANGENT) && (p_old_format & OLD_ARRAY_COMPRESS_TANGENT)) {
						for (uint32_t i = 0; i < p_elements; i++) {
							const int8_t *src = (const int8_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							int16_t *dst = (int16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];

							dst[0] = (int16_t)CLAMP(src[0] / 127.0f * 32767, -32768, 32767);
							dst[1] = (int16_t)CLAMP(src[1] / 127.0f * 32767, -32768, 32767);
						}
						src_offset += sizeof(int8_t) * 2;
					} else {
						for (uint32_t i = 0; i < p_elements; i++) {
							const int16_t *src = (const int16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							int16_t *dst = (int16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];

							dst[0] = src[0];
							dst[1] = src[1];
						}
						src_offset += sizeof(int16_t) * 2;
					}
				} else { // No Octahedral compression
					if (p_old_format & OLD_ARRAY_COMPRESS_NORMAL) {
						for (uint32_t i = 0; i < p_elements; i++) {
							const int8_t *src = (const int8_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							const Vector3 original_normal(src[0], src[1], src[2]);
							Vector2 res = original_normal.octahedron_encode();

							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];
							dst[0] = (uint16_t)CLAMP(res.x * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP(res.y * 65535, 0, 65535);
						}
						src_offset += sizeof(uint8_t) * 4; // 1 byte padding
					} else {
						for (uint32_t i = 0; i < p_elements; i++) {
							const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							const Vector3 original_normal(src[0], src[1], src[2]);
							Vector2 res = original_normal.octahedron_encode();

							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];
							dst[0] = (uint16_t)CLAMP(res.x * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP(res.y * 65535, 0, 65535);
						}
						src_offset += sizeof(float) * 3;
					}
				}

			} break;
			case OLD_ARRAY_TANGENT: {
				if (p_old_format & OLD_ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
					if (p_old_format & OLD_ARRAY_COMPRESS_TANGENT) { // int8 SNORM -> uint16 UNORM
						for (uint32_t i = 0; i < p_elements; i++) {
							const int8_t *src = (const int8_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_TANGENT]];

							dst[0] = (uint16_t)CLAMP((src[0] / 127.0f * .5f + .5f) * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP((src[1] / 127.0f * .5f + .5f) * 65535, 0, 65535);
						}
						src_offset += sizeof(uint8_t) * 2;
					} else { // int16 SNORM -> uint16 UNORM
						for (uint32_t i = 0; i < p_elements; i++) {
							const int16_t *src = (const int16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_TANGENT]];

							dst[0] = (uint16_t)CLAMP((src[0] / 32767.0f * .5f + .5f) * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP((src[1] / 32767.0f * .5f + .5f) * 65535, 0, 65535);
						}
						src_offset += sizeof(uint16_t) * 2;
					}
				} else { // No Octahedral compression
					if (p_old_format & OLD_ARRAY_COMPRESS_TANGENT) {
						for (uint32_t i = 0; i < p_elements; i++) {
							const int8_t *src = (const int8_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							const Vector3 original_tangent(src[0], src[1], src[2]);
							Vector2 res = original_tangent.octahedron_tangent_encode(src[3]);

							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];
							dst[0] = (uint16_t)CLAMP(res.x * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP(res.y * 65535, 0, 65535);
							if (dst[0] == 0 && dst[1] == 65535) {
								// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
								// So we sanitize here.
								dst[0] = 65535;
							}
						}
						src_offset += sizeof(uint8_t) * 4;
					} else {
						for (uint32_t i = 0; i < p_elements; i++) {
							const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
							const Vector3 original_tangent(src[0], src[1], src[2]);
							Vector2 res = original_tangent.octahedron_tangent_encode(src[3]);

							uint16_t *dst = (uint16_t *)&dst_vertex_ptr[i * dst_normal_tangent_stride + dst_offsets[Mesh::ARRAY_NORMAL]];
							dst[0] = (uint16_t)CLAMP(res.x * 65535, 0, 65535);
							dst[1] = (uint16_t)CLAMP(res.y * 65535, 0, 65535);
							if (dst[0] == 0 && dst[1] == 65535) {
								// (1, 1) and (0, 1) decode to the same value, but (0, 1) messes with our compression detection.
								// So we sanitize here.
								dst[0] = 65535;
							}
						}
						src_offset += sizeof(float) * 4;
					}
				}
			} break;
			case OLD_ARRAY_COLOR: {
				if (p_old_format & OLD_ARRAY_COMPRESS_COLOR) {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint32_t *src = (const uint32_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint32_t *dst = (uint32_t *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_COLOR]];

						*dst = *src;
					}
					src_offset += sizeof(uint32_t);
				} else {
					for (uint32_t i = 0; i < p_elements; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint8_t *dst = (uint8_t *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_COLOR]];

						dst[0] = uint8_t(CLAMP(src[0] * 255.0, 0.0, 255.0));
						dst[1] = uint8_t(CLAMP(src[1] * 255.0, 0.0, 255.0));
						dst[2] = uint8_t(CLAMP(src[2] * 255.0, 0.0, 255.0));
						dst[3] = uint8_t(CLAMP(src[3] * 255.0, 0.0, 255.0));
					}
					src_offset += sizeof(float) * 4;
				}
			} break;
			case OLD_ARRAY_TEX_UV: {
				if (p_old_format & OLD_ARRAY_COMPRESS_TEX_UV) {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						float *dst = (float *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_TEX_UV]];

						dst[0] = Math::half_to_float(src[0]);
						dst[1] = Math::half_to_float(src[1]);
					}
					src_offset += sizeof(uint16_t) * 2;
				} else {
					for (uint32_t i = 0; i < p_elements; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						float *dst = (float *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_TEX_UV]];

						dst[0] = src[0];
						dst[1] = src[1];
					}
					src_offset += sizeof(float) * 2;
				}

			} break;
			case OLD_ARRAY_TEX_UV2: {
				if (p_old_format & OLD_ARRAY_COMPRESS_TEX_UV2) {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						float *dst = (float *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_TEX_UV2]];

						dst[0] = Math::half_to_float(src[0]);
						dst[1] = Math::half_to_float(src[1]);
					}
					src_offset += sizeof(uint16_t) * 2;
				} else {
					for (uint32_t i = 0; i < p_elements; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						float *dst = (float *)&dst_attribute_ptr[i * dst_attribute_stride + dst_offsets[Mesh::ARRAY_TEX_UV2]];

						dst[0] = src[0];
						dst[1] = src[1];
					}
					src_offset += sizeof(float) * 2;
				}
			} break;
			case OLD_ARRAY_BONES: {
				if (p_old_format & OLD_ARRAY_FLAG_USE_16_BIT_BONES) {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint16_t *dst = (uint16_t *)&dst_skin_ptr[i * dst_skin_stride + dst_offsets[Mesh::ARRAY_BONES]];

						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
					}
					src_offset += sizeof(uint16_t) * 4;
				} else {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint8_t *src = (const uint8_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint16_t *dst = (uint16_t *)&dst_skin_ptr[i * dst_skin_stride + dst_offsets[Mesh::ARRAY_BONES]];

						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
					}
					src_offset += sizeof(uint8_t) * 4;
				}
			} break;
			case OLD_ARRAY_WEIGHTS: {
				if (p_old_format & OLD_ARRAY_COMPRESS_WEIGHTS) {
					for (uint32_t i = 0; i < p_elements; i++) {
						const uint16_t *src = (const uint16_t *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint16_t *dst = (uint16_t *)&dst_skin_ptr[i * dst_skin_stride + dst_offsets[Mesh::ARRAY_WEIGHTS]];

						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
					}
					src_offset += sizeof(uint16_t) * 4;
				} else {
					for (uint32_t i = 0; i < p_elements; i++) {
						const float *src = (const float *)&src_vertex_ptr[i * src_vertex_stride + src_offset];
						uint16_t *dst = (uint16_t *)&dst_skin_ptr[i * dst_skin_stride + dst_offsets[Mesh::ARRAY_WEIGHTS]];

						dst[0] = uint16_t(CLAMP(src[0] * 65535.0, 0, 65535.0));
						dst[1] = uint16_t(CLAMP(src[1] * 65535.0, 0, 65535.0));
						dst[2] = uint16_t(CLAMP(src[2] * 65535.0, 0, 65535.0));
						dst[3] = uint16_t(CLAMP(src[3] * 65535.0, 0, 65535.0));
					}
					src_offset += sizeof(float) * 4;
				}
			} break;
			default: {
			}
		}
	}
}

bool ArrayMesh::_set(const StringName &p_name, const Variant &p_value) {
	String sname = p_name;

	if (sname.begins_with("surface_")) {
		int sl = sname.find_char('/');
		if (sl == -1) {
			return false;
		}
		int idx = sname.substr(8, sl - 8).to_int();

		String what = sname.get_slicec('/', 1);
		if (what == "material") {
			surface_set_material(idx, p_value);
		} else if (what == "name") {
			surface_set_name(idx, p_value);
		}
		return true;
	}

#ifndef DISABLE_DEPRECATED
	// Kept for compatibility from 3.x to 4.0.
	if (!sname.begins_with("surfaces")) {
		return false;
	}

	WARN_DEPRECATED_MSG(vformat(
			"Mesh uses old surface format, which is deprecated (and loads slower). Consider re-importing or re-saving the scene. Path: \"%s\"",
			get_path()));

	int idx = sname.get_slicec('/', 1).to_int();
	String what = sname.get_slicec('/', 2);

	if (idx == surfaces.size()) {
		//create
		Dictionary d = p_value;
		ERR_FAIL_COND_V(!d.has("primitive"), false);

		if (d.has("arrays")) {
			//oldest format (2.x)
			ERR_FAIL_COND_V(!d.has("morph_arrays"), false);
			Array morph_arrays = d["morph_arrays"];
			for (int i = 0; i < morph_arrays.size(); i++) {
				morph_arrays[i] = _convert_old_array(morph_arrays[i]);
			}
			add_surface_from_arrays(_old_primitives[int(d["primitive"])], _convert_old_array(d["arrays"]), morph_arrays);

		} else if (d.has("array_data")) {
			//print_line("array data (old style");
			//older format (3.x)
			Vector<uint8_t> array_data = d["array_data"];
			Vector<uint8_t> array_index_data;
			if (d.has("array_index_data")) {
				array_index_data = d["array_index_data"];
			}

			ERR_FAIL_COND_V(!d.has("format"), false);
			uint64_t old_format = d["format"];

			uint32_t primitive = d["primitive"];

			primitive = _old_primitives[primitive]; //compatibility

			ERR_FAIL_COND_V(!d.has("vertex_count"), false);
			int vertex_count = d["vertex_count"];

			uint64_t new_format = ARRAY_FORMAT_VERTEX | ARRAY_FLAG_FORMAT_CURRENT_VERSION;

			if (old_format & OLD_ARRAY_FORMAT_NORMAL) {
				new_format |= ARRAY_FORMAT_NORMAL;
			}
			if (old_format & OLD_ARRAY_FORMAT_TANGENT) {
				new_format |= ARRAY_FORMAT_TANGENT;
			}
			if (old_format & OLD_ARRAY_FORMAT_COLOR) {
				new_format |= ARRAY_FORMAT_COLOR;
			}
			if (old_format & OLD_ARRAY_FORMAT_TEX_UV) {
				new_format |= ARRAY_FORMAT_TEX_UV;
			}
			if (old_format & OLD_ARRAY_FORMAT_TEX_UV2) {
				new_format |= ARRAY_FORMAT_TEX_UV2;
			}
			if (old_format & OLD_ARRAY_FORMAT_BONES) {
				new_format |= ARRAY_FORMAT_BONES;
			}
			if (old_format & OLD_ARRAY_FORMAT_WEIGHTS) {
				new_format |= ARRAY_FORMAT_WEIGHTS;
			}
			if (old_format & OLD_ARRAY_FORMAT_INDEX) {
				new_format |= ARRAY_FORMAT_INDEX;
			}
			if (old_format & OLD_ARRAY_FLAG_USE_2D_VERTICES) {
				new_format |= OLD_ARRAY_FLAG_USE_2D_VERTICES;
			}

			Vector<uint8_t> vertex_array;
			Vector<uint8_t> attribute_array;
			Vector<uint8_t> skin_array;

			_fix_array_compatibility(array_data, old_format, new_format, vertex_count, vertex_array, attribute_array, skin_array);

			int index_count = 0;
			if (d.has("index_count")) {
				index_count = d["index_count"];
			}

			Vector<uint8_t> blend_shapes_new;

			if (d.has("blend_shape_data")) {
				Array blend_shape_data = d["blend_shape_data"];
				for (int i = 0; i < blend_shape_data.size(); i++) {
					Vector<uint8_t> blend_vertex_array;
					Vector<uint8_t> blend_attribute_array;
					Vector<uint8_t> blend_skin_array;

					Vector<uint8_t> shape = blend_shape_data[i];
					_fix_array_compatibility(shape, old_format, new_format, vertex_count, blend_vertex_array, blend_attribute_array, blend_skin_array);

					blend_shapes_new.append_array(blend_vertex_array);
				}
			}

			//clear unused flags
			print_verbose("Mesh format pre-conversion: " + itos(old_format));

			print_verbose("Mesh format post-conversion: " + itos(new_format));

			ERR_FAIL_COND_V(!d.has("aabb"), false);
			AABB aabb_new = d["aabb"];

			Vector<AABB> bone_aabb;
			if (d.has("skeleton_aabb")) {
				Array baabb = d["skeleton_aabb"];
				bone_aabb.resize(baabb.size());

				for (int i = 0; i < baabb.size(); i++) {
					bone_aabb.write[i] = baabb[i];
				}
			}

			add_surface(new_format, PrimitiveType(primitive), vertex_array, attribute_array, skin_array, vertex_count, array_index_data, index_count, aabb_new, blend_shapes_new, bone_aabb);

		} else {
			ERR_FAIL_V(false);
		}

		if (d.has("material")) {
			surface_set_material(idx, d["material"]);
		}
		if (d.has("name")) {
			surface_set_name(idx, d["name"]);
		}

		return true;
	}
#endif // DISABLE_DEPRECATED

	return false;
}

void ArrayMesh::_set_blend_shape_names(const PackedStringArray &p_names) {
	ERR_FAIL_COND(surfaces.size() > 0);

	blend_shapes.resize(p_names.size());
	for (int i = 0; i < p_names.size(); i++) {
		blend_shapes.write[i] = p_names[i];
	}

	if (mesh.is_valid()) {
		RS::get_singleton()->mesh_set_blend_shape_count(mesh, blend_shapes.size());
	}
}

PackedStringArray ArrayMesh::_get_blend_shape_names() const {
	PackedStringArray sarr;
	sarr.resize(blend_shapes.size());
	for (int i = 0; i < blend_shapes.size(); i++) {
		sarr.write[i] = blend_shapes[i];
	}
	return sarr;
}

Array ArrayMesh::_get_surfaces() const {
	if (mesh.is_null()) {
		return Array();
	}

	Array ret;
	for (int i = 0; i < surfaces.size(); i++) {
		RenderingServer::SurfaceData surface = RS::get_singleton()->mesh_get_surface(mesh, i);
		Dictionary data;
		data["format"] = surface.format;
		data["primitive"] = surface.primitive;
		data["vertex_data"] = surface.vertex_data;
		data["vertex_count"] = surface.vertex_count;
		if (surface.skin_data.size()) {
			data["skin_data"] = surface.skin_data;
		}
		if (surface.attribute_data.size()) {
			data["attribute_data"] = surface.attribute_data;
		}
		data["aabb"] = surface.aabb;
		data["uv_scale"] = surface.uv_scale;
		if (surface.index_count) {
			data["index_data"] = surface.index_data;
			data["index_count"] = surface.index_count;
		};

		Array lods;
		for (int j = 0; j < surface.lods.size(); j++) {
			lods.push_back(surface.lods[j].edge_length);
			lods.push_back(surface.lods[j].index_data);
		}

		if (lods.size()) {
			data["lods"] = lods;
		}

		Array bone_aabbs;
		for (int j = 0; j < surface.bone_aabbs.size(); j++) {
			bone_aabbs.push_back(surface.bone_aabbs[j]);
		}
		if (bone_aabbs.size()) {
			data["bone_aabbs"] = bone_aabbs;
		}

		if (surface.blend_shape_data.size()) {
			data["blend_shapes"] = surface.blend_shape_data;
		}

		if (surfaces[i].material.is_valid()) {
			data["material"] = surfaces[i].material;
		}

		if (!surfaces[i].name.is_empty()) {
			data["name"] = surfaces[i].name;
		}

		if (surfaces[i].is_2d) {
			data["2d"] = true;
		}

		ret.push_back(data);
	}

	return ret;
}

void ArrayMesh::_create_if_empty() const {
	if (!mesh.is_valid()) {
		mesh = RS::get_singleton()->mesh_create();
		RS::get_singleton()->mesh_set_blend_shape_mode(mesh, (RS::BlendShapeMode)blend_shape_mode);
		RS::get_singleton()->mesh_set_blend_shape_count(mesh, blend_shapes.size());
		RS::get_singleton()->mesh_set_path(mesh, get_path());
	}
}

void ArrayMesh::_set_surfaces(const Array &p_surfaces) {
	Vector<RS::SurfaceData> surface_data;
	Vector<Ref<Material>> surface_materials;
	Vector<String> surface_names;
	Vector<bool> surface_2d;

	for (int i = 0; i < p_surfaces.size(); i++) {
		RS::SurfaceData surface;
		Dictionary d = p_surfaces[i];
		ERR_FAIL_COND(!d.has("format"));
		ERR_FAIL_COND(!d.has("primitive"));
		ERR_FAIL_COND(!d.has("vertex_data"));
		ERR_FAIL_COND(!d.has("vertex_count"));
		ERR_FAIL_COND(!d.has("aabb"));
		surface.format = d["format"];
		surface.primitive = RS::PrimitiveType(int(d["primitive"]));
		surface.vertex_data = d["vertex_data"];
		surface.vertex_count = d["vertex_count"];
		if (d.has("attribute_data")) {
			surface.attribute_data = d["attribute_data"];
		}
		if (d.has("skin_data")) {
			surface.skin_data = d["skin_data"];
		}
		surface.aabb = d["aabb"];

		if (d.has("uv_scale")) {
			surface.uv_scale = d["uv_scale"];
		}

		if (d.has("index_data")) {
			ERR_FAIL_COND(!d.has("index_count"));
			surface.index_data = d["index_data"];
			surface.index_count = d["index_count"];
		}

		if (d.has("lods")) {
			Array lods = d["lods"];
			ERR_FAIL_COND(lods.size() & 1); //must be even
			for (int j = 0; j < lods.size(); j += 2) {
				RS::SurfaceData::LOD lod;
				lod.edge_length = lods[j + 0];
				lod.index_data = lods[j + 1];
				surface.lods.push_back(lod);
			}
		}

		if (d.has("bone_aabbs")) {
			Array bone_aabbs = d["bone_aabbs"];
			for (int j = 0; j < bone_aabbs.size(); j++) {
				surface.bone_aabbs.push_back(bone_aabbs[j]);
			}
		}

		if (d.has("blend_shapes")) {
			surface.blend_shape_data = d["blend_shapes"];
		}

		Ref<Material> material;
		if (d.has("material")) {
			material = d["material"];
			if (material.is_valid()) {
				surface.material = material->get_rid();
			}
		}

		String surf_name;
		if (d.has("name")) {
			surf_name = d["name"];
		}

		bool _2d = false;
		if (d.has("2d")) {
			_2d = d["2d"];
		}

#ifndef DISABLE_DEPRECATED
		uint64_t surface_version = surface.format & (ARRAY_FLAG_FORMAT_VERSION_MASK << ARRAY_FLAG_FORMAT_VERSION_SHIFT);
		if (surface_version != ARRAY_FLAG_FORMAT_CURRENT_VERSION) {
			RS::get_singleton()->fix_surface_compatibility(surface, get_path());
			surface_version = surface.format & (RS::ARRAY_FLAG_FORMAT_VERSION_MASK << RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
			ERR_FAIL_COND_MSG(surface_version != RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION,
					vformat("Surface version provided (%d) does not match current version (%d).",
							(surface_version >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK,
							(RS::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RS::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RS::ARRAY_FLAG_FORMAT_VERSION_MASK));
		}
#endif

		surface_data.push_back(surface);
		surface_materials.push_back(material);
		surface_names.push_back(surf_name);
		surface_2d.push_back(_2d);
	}

	if (mesh.is_valid()) {
		//if mesh exists, it needs to be updated
		RS::get_singleton()->mesh_clear(mesh);
		for (int i = 0; i < surface_data.size(); i++) {
			RS::get_singleton()->mesh_add_surface(mesh, surface_data[i]);
		}
	} else {
		// if mesh does not exist (first time this is loaded, most likely),
		// we can create it with a single call, which is a lot more efficient and thread friendly
		mesh = RS::get_singleton()->mesh_create_from_surfaces(surface_data, blend_shapes.size());
		RS::get_singleton()->mesh_set_blend_shape_mode(mesh, (RS::BlendShapeMode)blend_shape_mode);
		RS::get_singleton()->mesh_set_path(mesh, get_path());
	}

	surfaces.clear();

	aabb = AABB();
	for (int i = 0; i < surface_data.size(); i++) {
		Surface s;
		s.aabb = surface_data[i].aabb;
		if (i == 0) {
			aabb = s.aabb;
		} else {
			aabb.merge_with(s.aabb);
		}

		s.material = surface_materials[i];
		s.is_2d = surface_2d[i];
		s.name = surface_names[i];

		s.format = surface_data[i].format;
		s.primitive = PrimitiveType(surface_data[i].primitive);
		s.array_length = surface_data[i].vertex_count;
		s.index_array_length = surface_data[i].index_count;

		surfaces.push_back(s);
	}
}

bool ArrayMesh::_get(const StringName &p_name, Variant &r_ret) const {
	if (_is_generated()) {
		return false;
	}

	String sname = p_name;
	if (sname.begins_with("surface_")) {
		int sl = sname.find_char('/');
		if (sl == -1) {
			return false;
		}
		int idx = sname.substr(8, sl - 8).to_int();
		String what = sname.get_slicec('/', 1);
		if (what == "material") {
			r_ret = surface_get_material(idx);
		} else if (what == "name") {
			r_ret = surface_get_name(idx);
		}
		return true;
	}

	return false;
}

void ArrayMesh::reset_state() {
	clear_surfaces();
	clear_blend_shapes();

	aabb = AABB();
	blend_shape_mode = BLEND_SHAPE_MODE_RELATIVE;
	custom_aabb = AABB();
}

void ArrayMesh::_get_property_list(List<PropertyInfo> *p_list) const {
	if (_is_generated()) {
		return;
	}

	for (int i = 0; i < surfaces.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "surface_" + itos(i) + "/name", PROPERTY_HINT_NO_NODEPATH, "", PROPERTY_USAGE_EDITOR));
		if (surfaces[i].is_2d) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "surface_" + itos(i) + "/material", PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemMaterial,ShaderMaterial", PROPERTY_USAGE_EDITOR));
		} else {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "surface_" + itos(i) + "/material", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial", PROPERTY_USAGE_EDITOR));
		}
	}
}

void ArrayMesh::_recompute_aabb() {
	// regenerate AABB
	aabb = AABB();

	for (int i = 0; i < surfaces.size(); i++) {
		if (i == 0) {
			aabb = surfaces[i].aabb;
		} else {
			aabb.merge_with(surfaces[i].aabb);
		}
	}
}

// TODO: Need to add binding to add_surface using future MeshSurfaceData object.
void ArrayMesh::add_surface(BitField<ArrayFormat> p_format, PrimitiveType p_primitive, const Vector<uint8_t> &p_array, const Vector<uint8_t> &p_attribute_array, const Vector<uint8_t> &p_skin_array, int p_vertex_count, const Vector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<uint8_t> &p_blend_shape_data, const Vector<AABB> &p_bone_aabbs, const Vector<RS::SurfaceData::LOD> &p_lods, const Vector4 p_uv_scale) {
	ERR_FAIL_COND(surfaces.size() == RS::MAX_MESH_SURFACES);
	_create_if_empty();

	Surface s;
	s.aabb = p_aabb;
	s.is_2d = p_format & ARRAY_FLAG_USE_2D_VERTICES;
	s.primitive = p_primitive;
	s.array_length = p_vertex_count;
	s.index_array_length = p_index_count;
	s.format = p_format;

	surfaces.push_back(s);
	_recompute_aabb();

	RS::SurfaceData sd;
	sd.format = p_format;
	sd.primitive = RS::PrimitiveType(p_primitive);
	sd.aabb = p_aabb;
	sd.vertex_count = p_vertex_count;
	sd.vertex_data = p_array;
	sd.attribute_data = p_attribute_array;
	sd.skin_data = p_skin_array;
	sd.index_count = p_index_count;
	sd.index_data = p_index_array;
	sd.blend_shape_data = p_blend_shape_data;
	sd.bone_aabbs = p_bone_aabbs;
	sd.lods = p_lods;
	sd.uv_scale = p_uv_scale;

	RenderingServer::get_singleton()->mesh_add_surface(mesh, sd);

	clear_cache();
	notify_property_list_changed();
	emit_changed();
}

void ArrayMesh::add_surface_from_arrays(PrimitiveType p_primitive, const Array &p_arrays, const TypedArray<Array> &p_blend_shapes, const Dictionary &p_lods, BitField<ArrayFormat> p_flags) {
	ERR_FAIL_COND(p_blend_shapes.size() != blend_shapes.size());
	ERR_FAIL_COND(p_arrays.size() != ARRAY_MAX);

	RS::SurfaceData surface;

	Error err = RS::get_singleton()->mesh_create_surface_data_from_arrays(&surface, (RenderingServer::PrimitiveType)p_primitive, p_arrays, p_blend_shapes, p_lods, p_flags);
	ERR_FAIL_COND(err != OK);

	/* Debug code.
	print_line("format: " + itos(surface.format));
	print_line("aabb: " + surface.aabb);
	print_line("array size: " + itos(surface.vertex_data.size()));
	print_line("vertex count: " + itos(surface.vertex_count));
	print_line("index size: " + itos(surface.index_data.size()));
	print_line("index count: " + itos(surface.index_count));
	print_line("primitive: " + itos(surface.primitive));
	*/

	add_surface(surface.format, PrimitiveType(surface.primitive), surface.vertex_data, surface.attribute_data, surface.skin_data, surface.vertex_count, surface.index_data, surface.index_count, surface.aabb, surface.blend_shape_data, surface.bone_aabbs, surface.lods, surface.uv_scale);
}

Array ArrayMesh::surface_get_arrays(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, surfaces.size(), Array());
	return RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh, p_surface);
}

TypedArray<Array> ArrayMesh::surface_get_blend_shape_arrays(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, surfaces.size(), TypedArray<Array>());
	return RenderingServer::get_singleton()->mesh_surface_get_blend_shape_arrays(mesh, p_surface);
}

Dictionary ArrayMesh::surface_get_lods(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, surfaces.size(), Dictionary());
	return RenderingServer::get_singleton()->mesh_surface_get_lods(mesh, p_surface);
}

int ArrayMesh::get_surface_count() const {
	return surfaces.size();
}

void ArrayMesh::add_blend_shape(const StringName &p_name) {
	ERR_FAIL_COND_MSG(surfaces.size(), "Can't add a shape key count if surfaces are already created.");

	StringName shape_name = p_name;

	if (blend_shapes.has(shape_name)) {
		int count = 2;
		do {
			shape_name = String(p_name) + " " + itos(count);
			count++;
		} while (blend_shapes.has(shape_name));
	}

	blend_shapes.push_back(shape_name);

	if (mesh.is_valid()) {
		RS::get_singleton()->mesh_set_blend_shape_count(mesh, blend_shapes.size());
	}
}

int ArrayMesh::get_blend_shape_count() const {
	return blend_shapes.size();
}

StringName ArrayMesh::get_blend_shape_name(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, blend_shapes.size(), StringName());
	return blend_shapes[p_index];
}

void ArrayMesh::set_blend_shape_name(int p_index, const StringName &p_name) {
	ERR_FAIL_INDEX(p_index, blend_shapes.size());

	StringName shape_name = p_name;
	int found = blend_shapes.find(shape_name);
	if (found != -1 && found != p_index) {
		int count = 2;
		do {
			shape_name = String(p_name) + " " + itos(count);
			count++;
		} while (blend_shapes.has(shape_name));
	}

	blend_shapes.write[p_index] = shape_name;
}

void ArrayMesh::clear_blend_shapes() {
	ERR_FAIL_COND_MSG(surfaces.size(), "Can't set shape key count if surfaces are already created.");

	blend_shapes.clear();

	if (mesh.is_valid()) {
		RS::get_singleton()->mesh_set_blend_shape_count(mesh, 0);
	}
}

void ArrayMesh::set_blend_shape_mode(BlendShapeMode p_mode) {
	blend_shape_mode = p_mode;
	if (mesh.is_valid()) {
		RS::get_singleton()->mesh_set_blend_shape_mode(mesh, (RS::BlendShapeMode)p_mode);
	}
}

ArrayMesh::BlendShapeMode ArrayMesh::get_blend_shape_mode() const {
	return blend_shape_mode;
}

int ArrayMesh::surface_get_array_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
	return surfaces[p_idx].array_length;
}

int ArrayMesh::surface_get_array_index_len(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), -1);
	return surfaces[p_idx].index_array_length;
}

BitField<Mesh::ArrayFormat> ArrayMesh::surface_get_format(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), 0);
	return surfaces[p_idx].format;
}

ArrayMesh::PrimitiveType ArrayMesh::surface_get_primitive_type(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), PRIMITIVE_LINES);
	return surfaces[p_idx].primitive;
}

void ArrayMesh::surface_set_material(int p_idx, const Ref<Material> &p_material) {
	ERR_FAIL_INDEX(p_idx, surfaces.size());
	if (surfaces[p_idx].material == p_material) {
		return;
	}
	surfaces.write[p_idx].material = p_material;
	RenderingServer::get_singleton()->mesh_surface_set_material(mesh, p_idx, p_material.is_null() ? RID() : p_material->get_rid());

	emit_changed();
}

int ArrayMesh::surface_find_by_name(const String &p_name) const {
	for (int i = 0; i < surfaces.size(); i++) {
		if (surfaces[i].name == p_name) {
			return i;
		}
	}
	return -1;
}

void ArrayMesh::surface_set_name(int p_idx, const String &p_name) {
	ERR_FAIL_INDEX(p_idx, surfaces.size());

	surfaces.write[p_idx].name = p_name;
	emit_changed();
}

String ArrayMesh::surface_get_name(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), String());
	return surfaces[p_idx].name;
}

void ArrayMesh::surface_update_vertex_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_INDEX(p_surface, surfaces.size());
	RS::get_singleton()->mesh_surface_update_vertex_region(mesh, p_surface, p_offset, p_data);
	emit_changed();
}

void ArrayMesh::surface_update_attribute_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_INDEX(p_surface, surfaces.size());
	RS::get_singleton()->mesh_surface_update_attribute_region(mesh, p_surface, p_offset, p_data);
	emit_changed();
}

void ArrayMesh::surface_update_skin_region(int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_INDEX(p_surface, surfaces.size());
	RS::get_singleton()->mesh_surface_update_skin_region(mesh, p_surface, p_offset, p_data);
	emit_changed();
}

void ArrayMesh::surface_set_custom_aabb(int p_idx, const AABB &p_aabb) {
	ERR_FAIL_INDEX(p_idx, surfaces.size());
	surfaces.write[p_idx].aabb = p_aabb;
	// set custom aabb too?
	emit_changed();
}

Ref<Material> ArrayMesh::surface_get_material(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, surfaces.size(), Ref<Material>());
	return surfaces[p_idx].material;
}

RID ArrayMesh::get_rid() const {
	_create_if_empty();
	return mesh;
}

AABB ArrayMesh::get_aabb() const {
	return aabb;
}

void ArrayMesh::clear_surfaces() {
	if (!mesh.is_valid()) {
		return;
	}
	RS::get_singleton()->mesh_clear(mesh);
	surfaces.clear();
	aabb = AABB();
}

void ArrayMesh::surface_remove(int p_surface) {
	ERR_FAIL_INDEX(p_surface, surfaces.size());
	RS::get_singleton()->mesh_surface_remove(mesh, p_surface);
	surfaces.remove_at(p_surface);

	clear_cache();
	_recompute_aabb();
	notify_property_list_changed();
	emit_changed();
}

void ArrayMesh::set_custom_aabb(const AABB &p_custom) {
	_create_if_empty();
	custom_aabb = p_custom;
	RS::get_singleton()->mesh_set_custom_aabb(mesh, custom_aabb);
	emit_changed();
}

AABB ArrayMesh::get_custom_aabb() const {
	return custom_aabb;
}

void ArrayMesh::regen_normal_maps() {
	if (surfaces.size() == 0) {
		return;
	}
	Vector<Ref<SurfaceTool>> surfs;
	Vector<uint64_t> formats;
	for (int i = 0; i < get_surface_count(); i++) {
		Ref<SurfaceTool> st = memnew(SurfaceTool);
		st->create_from(Ref<ArrayMesh>(this), i);
		surfs.push_back(st);
		formats.push_back(surface_get_format(i));
	}

	clear_surfaces();

	for (int i = 0; i < surfs.size(); i++) {
		surfs.write[i]->generate_tangents();
		surfs.write[i]->commit(Ref<ArrayMesh>(this), formats[i]);
	}
}

//dirty hack
bool (*array_mesh_lightmap_unwrap_callback)(float p_texel_size, const float *p_vertices, const float *p_normals, int p_vertex_count, const int *p_indices, int p_index_count, const uint8_t *p_cache_data, bool *r_use_cache, uint8_t **r_mesh_cache, int *r_mesh_cache_size, float **r_uv, int **r_vertex, int *r_vertex_count, int **r_index, int *r_index_count, int *r_size_hint_x, int *r_size_hint_y) = nullptr;

struct ArrayMeshLightmapSurface {
	Ref<Material> material;
	LocalVector<SurfaceTool::Vertex> vertices;
	Mesh::PrimitiveType primitive = Mesh::PrimitiveType::PRIMITIVE_MAX;
	uint64_t format = 0;
};

Error ArrayMesh::lightmap_unwrap(const Transform3D &p_base_transform, float p_texel_size) {
	Vector<uint8_t> null_cache;
	return lightmap_unwrap_cached(p_base_transform, p_texel_size, null_cache, null_cache, false);
}

Error ArrayMesh::lightmap_unwrap_cached(const Transform3D &p_base_transform, float p_texel_size, const Vector<uint8_t> &p_src_cache, Vector<uint8_t> &r_dst_cache, bool p_generate_cache) {
	ERR_FAIL_NULL_V(array_mesh_lightmap_unwrap_callback, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V_MSG(blend_shapes.size() != 0, ERR_UNAVAILABLE, "Can't unwrap mesh with blend shapes.");
	ERR_FAIL_COND_V_MSG(p_texel_size <= 0.0f, ERR_PARAMETER_RANGE_ERROR, "Texel size must be greater than 0.");

	LocalVector<float> vertices;
	LocalVector<float> normals;
	LocalVector<int> indices;
	LocalVector<float> uv;
	LocalVector<Pair<int, int>> uv_indices;

	Vector<ArrayMeshLightmapSurface> lightmap_surfaces;

	// Keep only the scale
	Basis basis = p_base_transform.get_basis();
	Vector3 scale = Vector3(basis.get_column(0).length(), basis.get_column(1).length(), basis.get_column(2).length());

	Transform3D transform;
	transform.scale(scale);

	Basis normal_basis = transform.basis.inverse().transposed();

	for (int i = 0; i < get_surface_count(); i++) {
		ArrayMeshLightmapSurface s;
		s.primitive = surface_get_primitive_type(i);

		ERR_FAIL_COND_V_MSG(s.primitive != Mesh::PRIMITIVE_TRIANGLES, ERR_UNAVAILABLE, "Only triangles are supported for lightmap unwrap.");
		s.format = surface_get_format(i);
		ERR_FAIL_COND_V_MSG(!(s.format & ARRAY_FORMAT_NORMAL), ERR_UNAVAILABLE, "Normals are required for lightmap unwrap.");

		Array arrays = surface_get_arrays(i);
		s.material = surface_get_material(i);
		SurfaceTool::create_vertex_array_from_arrays(arrays, s.vertices, &s.format);

		PackedVector3Array rvertices = arrays[Mesh::ARRAY_VERTEX];
		int vc = rvertices.size();

		PackedVector3Array rnormals = arrays[Mesh::ARRAY_NORMAL];

		int vertex_ofs = vertices.size() / 3;

		vertices.resize((vertex_ofs + vc) * 3);
		normals.resize((vertex_ofs + vc) * 3);
		uv_indices.resize(vertex_ofs + vc);

		for (int j = 0; j < vc; j++) {
			Vector3 v = transform.xform(rvertices[j]);
			Vector3 n = normal_basis.xform(rnormals[j]).normalized();

			vertices[(j + vertex_ofs) * 3 + 0] = v.x;
			vertices[(j + vertex_ofs) * 3 + 1] = v.y;
			vertices[(j + vertex_ofs) * 3 + 2] = v.z;
			normals[(j + vertex_ofs) * 3 + 0] = n.x;
			normals[(j + vertex_ofs) * 3 + 1] = n.y;
			normals[(j + vertex_ofs) * 3 + 2] = n.z;
			uv_indices[j + vertex_ofs] = Pair<int, int>(i, j);
		}

		PackedInt32Array rindices = arrays[Mesh::ARRAY_INDEX];
		int ic = rindices.size();

		float eps = 1.19209290e-7F; // Taken from xatlas.h
		if (ic == 0) {
			for (int j = 0; j < vc / 3; j++) {
				Vector3 p0 = transform.xform(rvertices[j * 3 + 0]);
				Vector3 p1 = transform.xform(rvertices[j * 3 + 1]);
				Vector3 p2 = transform.xform(rvertices[j * 3 + 2]);

				if ((p0 - p1).length_squared() < eps || (p1 - p2).length_squared() < eps || (p2 - p0).length_squared() < eps) {
					continue;
				}

				indices.push_back(vertex_ofs + j * 3 + 0);
				indices.push_back(vertex_ofs + j * 3 + 1);
				indices.push_back(vertex_ofs + j * 3 + 2);
			}

		} else {
			for (int j = 0; j < ic / 3; j++) {
				Vector3 p0 = transform.xform(rvertices[rindices[j * 3 + 0]]);
				Vector3 p1 = transform.xform(rvertices[rindices[j * 3 + 1]]);
				Vector3 p2 = transform.xform(rvertices[rindices[j * 3 + 2]]);

				if ((p0 - p1).length_squared() < eps || (p1 - p2).length_squared() < eps || (p2 - p0).length_squared() < eps) {
					continue;
				}

				indices.push_back(vertex_ofs + rindices[j * 3 + 0]);
				indices.push_back(vertex_ofs + rindices[j * 3 + 1]);
				indices.push_back(vertex_ofs + rindices[j * 3 + 2]);
			}
		}

		lightmap_surfaces.push_back(s);
	}

	//unwrap

	bool use_cache = p_generate_cache; // Used to request cache generation and to know if cache was used
	uint8_t *gen_cache;
	int gen_cache_size;
	float *gen_uvs;
	int *gen_vertices;
	int *gen_indices;
	int gen_vertex_count;
	int gen_index_count;
	int size_x;
	int size_y;

	bool ok = array_mesh_lightmap_unwrap_callback(p_texel_size, vertices.ptr(), normals.ptr(), vertices.size() / 3, indices.ptr(), indices.size(), p_src_cache.ptr(), &use_cache, &gen_cache, &gen_cache_size, &gen_uvs, &gen_vertices, &gen_vertex_count, &gen_indices, &gen_index_count, &size_x, &size_y);

	if (!ok) {
		return ERR_CANT_CREATE;
	}

	clear_surfaces();

	//create surfacetools for each surface..
	LocalVector<Ref<SurfaceTool>> surfaces_tools;

	for (int i = 0; i < lightmap_surfaces.size(); i++) {
		Ref<SurfaceTool> st;
		st.instantiate();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		st->set_material(lightmap_surfaces[i].material);
		surfaces_tools.push_back(st); //stay there
	}

	print_verbose("Mesh: Gen indices: " + itos(gen_index_count));

	//go through all indices
	for (int i = 0; i < gen_index_count; i += 3) {
		ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 0]], (int)uv_indices.size(), ERR_BUG);
		ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 1]], (int)uv_indices.size(), ERR_BUG);
		ERR_FAIL_INDEX_V(gen_vertices[gen_indices[i + 2]], (int)uv_indices.size(), ERR_BUG);

		ERR_FAIL_COND_V(uv_indices[gen_vertices[gen_indices[i + 0]]].first != uv_indices[gen_vertices[gen_indices[i + 1]]].first || uv_indices[gen_vertices[gen_indices[i + 0]]].first != uv_indices[gen_vertices[gen_indices[i + 2]]].first, ERR_BUG);

		int surface = uv_indices[gen_vertices[gen_indices[i + 0]]].first;

		for (int j = 0; j < 3; j++) {
			SurfaceTool::Vertex v = lightmap_surfaces[surface].vertices[uv_indices[gen_vertices[gen_indices[i + j]]].second];

			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_COLOR) {
				surfaces_tools[surface]->set_color(v.color);
			}
			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_TEX_UV) {
				surfaces_tools[surface]->set_uv(v.uv);
			}
			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_NORMAL) {
				surfaces_tools[surface]->set_normal(v.normal);
			}
			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_TANGENT) {
				Plane t;
				t.normal = v.tangent;
				t.d = v.binormal.dot(v.normal.cross(v.tangent)) < 0 ? -1 : 1;
				surfaces_tools[surface]->set_tangent(t);
			}
			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_BONES) {
				surfaces_tools[surface]->set_bones(v.bones);
			}
			if (lightmap_surfaces[surface].format & ARRAY_FORMAT_WEIGHTS) {
				surfaces_tools[surface]->set_weights(v.weights);
			}

			Vector2 uv2(gen_uvs[gen_indices[i + j] * 2 + 0], gen_uvs[gen_indices[i + j] * 2 + 1]);
			surfaces_tools[surface]->set_uv2(uv2);

			surfaces_tools[surface]->add_vertex(v.vertex);
		}
	}

	//generate surfaces
	for (unsigned int i = 0; i < surfaces_tools.size(); i++) {
		surfaces_tools[i]->index();
		surfaces_tools[i]->commit(Ref<ArrayMesh>((ArrayMesh *)this), lightmap_surfaces[i].format);
	}

	set_lightmap_size_hint(Size2(size_x, size_y));

	if (gen_cache_size > 0) {
		r_dst_cache.resize(gen_cache_size);
		memcpy(r_dst_cache.ptrw(), gen_cache, gen_cache_size);
		memfree(gen_cache);
	}

	if (!use_cache) {
		// Cache was not used, free the buffers
		memfree(gen_vertices);
		memfree(gen_indices);
		memfree(gen_uvs);
	}

	return OK;
}

void ArrayMesh::set_shadow_mesh(const Ref<ArrayMesh> &p_mesh) {
	ERR_FAIL_COND_MSG(p_mesh == this, "Cannot set a mesh as its own shadow mesh.");
	shadow_mesh = p_mesh;
	if (shadow_mesh.is_valid()) {
		RS::get_singleton()->mesh_set_shadow_mesh(mesh, shadow_mesh->get_rid());
	} else {
		RS::get_singleton()->mesh_set_shadow_mesh(mesh, RID());
	}
}

Ref<ArrayMesh> ArrayMesh::get_shadow_mesh() const {
	return shadow_mesh;
}

void ArrayMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_blend_shape", "name"), &ArrayMesh::add_blend_shape);
	ClassDB::bind_method(D_METHOD("get_blend_shape_count"), &ArrayMesh::get_blend_shape_count);
	ClassDB::bind_method(D_METHOD("get_blend_shape_name", "index"), &ArrayMesh::get_blend_shape_name);
	ClassDB::bind_method(D_METHOD("set_blend_shape_name", "index", "name"), &ArrayMesh::set_blend_shape_name);
	ClassDB::bind_method(D_METHOD("clear_blend_shapes"), &ArrayMesh::clear_blend_shapes);
	ClassDB::bind_method(D_METHOD("set_blend_shape_mode", "mode"), &ArrayMesh::set_blend_shape_mode);
	ClassDB::bind_method(D_METHOD("get_blend_shape_mode"), &ArrayMesh::get_blend_shape_mode);

	ClassDB::bind_method(D_METHOD("add_surface_from_arrays", "primitive", "arrays", "blend_shapes", "lods", "flags"), &ArrayMesh::add_surface_from_arrays, DEFVAL(Array()), DEFVAL(Dictionary()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("clear_surfaces"), &ArrayMesh::clear_surfaces);
	ClassDB::bind_method(D_METHOD("surface_remove", "surf_idx"), &ArrayMesh::surface_remove);
	ClassDB::bind_method(D_METHOD("surface_update_vertex_region", "surf_idx", "offset", "data"), &ArrayMesh::surface_update_vertex_region);
	ClassDB::bind_method(D_METHOD("surface_update_attribute_region", "surf_idx", "offset", "data"), &ArrayMesh::surface_update_attribute_region);
	ClassDB::bind_method(D_METHOD("surface_update_skin_region", "surf_idx", "offset", "data"), &ArrayMesh::surface_update_skin_region);
	ClassDB::bind_method(D_METHOD("surface_get_array_len", "surf_idx"), &ArrayMesh::surface_get_array_len);
	ClassDB::bind_method(D_METHOD("surface_get_array_index_len", "surf_idx"), &ArrayMesh::surface_get_array_index_len);
	ClassDB::bind_method(D_METHOD("surface_get_format", "surf_idx"), &ArrayMesh::surface_get_format);
	ClassDB::bind_method(D_METHOD("surface_get_primitive_type", "surf_idx"), &ArrayMesh::surface_get_primitive_type);
	ClassDB::bind_method(D_METHOD("surface_find_by_name", "name"), &ArrayMesh::surface_find_by_name);
	ClassDB::bind_method(D_METHOD("surface_set_name", "surf_idx", "name"), &ArrayMesh::surface_set_name);
	ClassDB::bind_method(D_METHOD("surface_get_name", "surf_idx"), &ArrayMesh::surface_get_name);
#ifndef _3D_DISABLED
	ClassDB::bind_method(D_METHOD("create_trimesh_shape"), &ArrayMesh::create_trimesh_shape);
	ClassDB::bind_method(D_METHOD("create_convex_shape", "clean", "simplify"), &ArrayMesh::create_convex_shape, DEFVAL(true), DEFVAL(false));
#endif // _3D_DISABLED
	ClassDB::bind_method(D_METHOD("create_outline", "margin"), &ArrayMesh::create_outline);
	ClassDB::bind_method(D_METHOD("regen_normal_maps"), &ArrayMesh::regen_normal_maps);
	ClassDB::set_method_flags(get_class_static(), _scs_create("regen_normal_maps"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
	ClassDB::bind_method(D_METHOD("lightmap_unwrap", "transform", "texel_size"), &ArrayMesh::lightmap_unwrap);
	ClassDB::set_method_flags(get_class_static(), _scs_create("lightmap_unwrap"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);
	ClassDB::bind_method(D_METHOD("generate_triangle_mesh"), &ArrayMesh::generate_triangle_mesh);

	ClassDB::bind_method(D_METHOD("set_custom_aabb", "aabb"), &ArrayMesh::set_custom_aabb);
	ClassDB::bind_method(D_METHOD("get_custom_aabb"), &ArrayMesh::get_custom_aabb);

	ClassDB::bind_method(D_METHOD("set_shadow_mesh", "mesh"), &ArrayMesh::set_shadow_mesh);
	ClassDB::bind_method(D_METHOD("get_shadow_mesh"), &ArrayMesh::get_shadow_mesh);

	ClassDB::bind_method(D_METHOD("_set_blend_shape_names", "blend_shape_names"), &ArrayMesh::_set_blend_shape_names);
	ClassDB::bind_method(D_METHOD("_get_blend_shape_names"), &ArrayMesh::_get_blend_shape_names);

	ClassDB::bind_method(D_METHOD("_set_surfaces", "surfaces"), &ArrayMesh::_set_surfaces);
	ClassDB::bind_method(D_METHOD("_get_surfaces"), &ArrayMesh::_get_surfaces);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "_blend_shape_names", PROPERTY_HINT_NO_NODEPATH, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_blend_shape_names", "_get_blend_shape_names");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "_surfaces", PROPERTY_HINT_NO_NODEPATH, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_surfaces", "_get_surfaces");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_shape_mode", PROPERTY_HINT_ENUM, "Normalized,Relative"), "set_blend_shape_mode", "get_blend_shape_mode");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "custom_aabb", PROPERTY_HINT_NO_NODEPATH, "suffix:m"), "set_custom_aabb", "get_custom_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow_mesh", PROPERTY_HINT_RESOURCE_TYPE, "ArrayMesh"), "set_shadow_mesh", "get_shadow_mesh");
}

void ArrayMesh::reload_from_file() {
	RenderingServer::get_singleton()->mesh_clear(mesh);
	surfaces.clear();
	clear_blend_shapes();
	clear_cache();

	Resource::reload_from_file();

	notify_property_list_changed();
}

ArrayMesh::ArrayMesh() {
	//mesh is now created on demand
	//mesh = RenderingServer::get_singleton()->mesh_create();
}

ArrayMesh::~ArrayMesh() {
	if (mesh.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(mesh);
	}
}
///////////////

void PlaceholderMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_aabb", "aabb"), &PlaceholderMesh::set_aabb);
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "aabb", PROPERTY_HINT_NONE, "suffix:m"), "set_aabb", "get_aabb");
}

PlaceholderMesh::PlaceholderMesh() {
	rid = RS::get_singleton()->mesh_create();
}

PlaceholderMesh::~PlaceholderMesh() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(rid);
}
