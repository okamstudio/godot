/**************************************************************************/
/*  navigation_mesh_source_geometry_data_3d.cpp                           */
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

#include "navigation_mesh_source_geometry_data_3d.h"

void NavigationMeshSourceGeometryData3D::set_vertices(const Vector<float> &p_vertices) {
	RWLockWrite write_lock(geometry_rwlock);
	vertices = p_vertices;
	bounds_dirty = true;
}

const Vector<float> &NavigationMeshSourceGeometryData3D::get_vertices() const {
	RWLockRead read_lock(geometry_rwlock);
	return vertices;
}

void NavigationMeshSourceGeometryData3D::set_indices(const Vector<int> &p_indices) {
	ERR_FAIL_COND(vertices.size() < p_indices.size());
	RWLockWrite write_lock(geometry_rwlock);
	indices = p_indices;
	bounds_dirty = true;
}

const Vector<int> &NavigationMeshSourceGeometryData3D::get_indices() const {
	RWLockRead read_lock(geometry_rwlock);
	return indices;
}

void NavigationMeshSourceGeometryData3D::append_arrays(const Vector<float> &p_vertices, const Vector<int> &p_indices) {
	RWLockWrite write_lock(geometry_rwlock);

	const int64_t number_of_vertices_before_merge = vertices.size();
	const int64_t number_of_indices_before_merge = indices.size();

	vertices.append_array(p_vertices);
	indices.append_array(p_indices);

	for (int64_t i = number_of_indices_before_merge; i < indices.size(); i++) {
		indices.set(i, indices[i] + number_of_vertices_before_merge / 3);
	}
	bounds_dirty = true;
}

bool NavigationMeshSourceGeometryData3D::has_data() {
	RWLockRead read_lock(geometry_rwlock);
	return vertices.size() && indices.size();
}

void NavigationMeshSourceGeometryData3D::clear() {
	RWLockWrite write_lock(geometry_rwlock);
	vertices.clear();
	indices.clear();
	_projected_obstructions.clear();
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::clear_projected_obstructions() {
	RWLockWrite write_lock(geometry_rwlock);
	_projected_obstructions.clear();
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::_add_vertex(const Vector3 &p_vec3) {
	vertices.push_back(p_vec3.x);
	vertices.push_back(p_vec3.y);
	vertices.push_back(p_vec3.z);
}

void NavigationMeshSourceGeometryData3D::_add_mesh(const Ref<Mesh> &p_mesh, const Transform3D &p_xform) {
	int current_vertex_count;
	for (int i = 0; i < p_mesh->get_surface_count(); i++) {
		current_vertex_count = vertices.size() / 3;

		if (p_mesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}

		int index_count = 0;
		if (p_mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {
			index_count = p_mesh->surface_get_array_index_len(i);
		} else {
			index_count = p_mesh->surface_get_array_len(i);
		}

		ERR_CONTINUE((index_count == 0 || (index_count % 3) != 0));

		int face_count = index_count / 3;

		Array a = p_mesh->surface_get_arrays(i);
		ERR_CONTINUE(a.is_empty() || (a.size() != Mesh::ARRAY_MAX));

		Vector<Vector3> mesh_vertices = a[Mesh::ARRAY_VERTEX];
		ERR_CONTINUE(mesh_vertices.is_empty());
		const Vector3 *vr = mesh_vertices.ptr();

		if (p_mesh->surface_get_format(i) & Mesh::ARRAY_FORMAT_INDEX) {
			Vector<int> mesh_indices = a[Mesh::ARRAY_INDEX];
			ERR_CONTINUE(mesh_indices.is_empty() || (mesh_indices.size() != index_count));
			const int *ir = mesh_indices.ptr();

			for (int j = 0; j < mesh_vertices.size(); j++) {
				_add_vertex(p_xform.xform(vr[j]));
			}

			for (int j = 0; j < face_count; j++) {
				// CCW
				indices.push_back(current_vertex_count + (ir[j * 3 + 0]));
				indices.push_back(current_vertex_count + (ir[j * 3 + 2]));
				indices.push_back(current_vertex_count + (ir[j * 3 + 1]));
			}
		} else {
			ERR_CONTINUE(mesh_vertices.size() != index_count);
			face_count = mesh_vertices.size() / 3;
			for (int j = 0; j < face_count; j++) {
				_add_vertex(p_xform.xform(vr[j * 3 + 0]));
				_add_vertex(p_xform.xform(vr[j * 3 + 2]));
				_add_vertex(p_xform.xform(vr[j * 3 + 1]));

				indices.push_back(current_vertex_count + (j * 3 + 0));
				indices.push_back(current_vertex_count + (j * 3 + 1));
				indices.push_back(current_vertex_count + (j * 3 + 2));
			}
		}
	}
}

void NavigationMeshSourceGeometryData3D::_add_mesh_array(const Array &p_mesh_array, const Transform3D &p_xform) {
	ERR_FAIL_COND(p_mesh_array.size() != Mesh::ARRAY_MAX);

	Vector<Vector3> mesh_vertices = p_mesh_array[Mesh::ARRAY_VERTEX];
	ERR_FAIL_COND(mesh_vertices.is_empty());
	const Vector3 *vr = mesh_vertices.ptr();

	Vector<int> mesh_indices = p_mesh_array[Mesh::ARRAY_INDEX];
	ERR_FAIL_COND(mesh_indices.is_empty());
	const int *ir = mesh_indices.ptr();

	const int face_count = mesh_indices.size() / 3;
	const int current_vertex_count = vertices.size() / 3;

	for (int j = 0; j < mesh_vertices.size(); j++) {
		_add_vertex(p_xform.xform(vr[j]));
	}

	for (int j = 0; j < face_count; j++) {
		// CCW
		indices.push_back(current_vertex_count + (ir[j * 3 + 0]));
		indices.push_back(current_vertex_count + (ir[j * 3 + 2]));
		indices.push_back(current_vertex_count + (ir[j * 3 + 1]));
	}
}

void NavigationMeshSourceGeometryData3D::_add_faces(const PackedVector3Array &p_faces, const Transform3D &p_xform) {
	ERR_FAIL_COND(p_faces.is_empty());
	ERR_FAIL_COND(p_faces.size() % 3 != 0);
	int face_count = p_faces.size() / 3;
	int current_vertex_count = vertices.size() / 3;

	for (int j = 0; j < face_count; j++) {
		_add_vertex(p_xform.xform(p_faces[j * 3 + 0]));
		_add_vertex(p_xform.xform(p_faces[j * 3 + 1]));
		_add_vertex(p_xform.xform(p_faces[j * 3 + 2]));

		indices.push_back(current_vertex_count + (j * 3 + 0));
		indices.push_back(current_vertex_count + (j * 3 + 2));
		indices.push_back(current_vertex_count + (j * 3 + 1));
	}
}

void NavigationMeshSourceGeometryData3D::add_mesh(const Ref<Mesh> &p_mesh, const Transform3D &p_xform) {
	ERR_FAIL_COND(p_mesh.is_null());

#ifdef DEBUG_ENABLED
	if (!Engine::get_singleton()->is_editor_hint()) {
		WARN_PRINT_ONCE("Source geometry parsing for navigation mesh baking had to parse RenderingServer meshes at runtime.\n\
		This poses a significant performance issues as visual meshes store geometry data on the GPU and transferring this data back to the CPU blocks the rendering.\n\
		For runtime (re)baking navigation meshes use and parse collision shapes as source geometry or create geometry data procedurally in scripts.");
	}
#endif

	_add_mesh(p_mesh, root_node_transform * p_xform);
}

void NavigationMeshSourceGeometryData3D::add_mesh_array(const Array &p_mesh_array, const Transform3D &p_xform) {
	ERR_FAIL_COND(p_mesh_array.size() != Mesh::ARRAY_MAX);
	RWLockWrite write_lock(geometry_rwlock);
	_add_mesh_array(p_mesh_array, root_node_transform * p_xform);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::add_faces(const PackedVector3Array &p_faces, const Transform3D &p_xform) {
	ERR_FAIL_COND(p_faces.size() % 3 != 0);
	RWLockWrite write_lock(geometry_rwlock);
	_add_faces(p_faces, root_node_transform * p_xform);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::merge(const Ref<NavigationMeshSourceGeometryData3D> &p_other_geometry) {
	ERR_FAIL_COND(p_other_geometry.is_null());

	Vector<float> other_vertices;
	Vector<int> other_indices;
	Vector<ProjectedObstruction> other_projected_obstructions;

	p_other_geometry->get_data(other_vertices, other_indices, other_projected_obstructions);

	RWLockWrite write_lock(geometry_rwlock);
	const int64_t number_of_vertices_before_merge = vertices.size();
	const int64_t number_of_indices_before_merge = indices.size();

	vertices.append_array(other_vertices);
	indices.append_array(other_indices);

	for (int64_t i = number_of_indices_before_merge; i < indices.size(); i++) {
		indices.set(i, indices[i] + number_of_vertices_before_merge / 3);
	}

	_projected_obstructions.append_array(other_projected_obstructions);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::add_projected_obstruction(const Vector<Vector3> &p_vertices, float p_elevation, float p_height, bool p_carve) {
	ERR_FAIL_COND(p_vertices.size() < 3);
	ERR_FAIL_COND(p_height < 0.0);

	ProjectedObstruction projected_obstruction;
	projected_obstruction.vertices.resize(p_vertices.size() * 3);
	projected_obstruction.elevation = p_elevation;
	projected_obstruction.height = p_height;
	projected_obstruction.carve = p_carve;

	float *obstruction_vertices_ptrw = projected_obstruction.vertices.ptrw();

	int vertex_index = 0;
	for (const Vector3 &vertex : p_vertices) {
		obstruction_vertices_ptrw[vertex_index++] = vertex.x;
		obstruction_vertices_ptrw[vertex_index++] = vertex.y;
		obstruction_vertices_ptrw[vertex_index++] = vertex.z;
	}

	RWLockWrite write_lock(geometry_rwlock);
	_projected_obstructions.push_back(projected_obstruction);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::set_projected_obstructions(const Array &p_array) {
	clear_projected_obstructions();

	for (int i = 0; i < p_array.size(); i++) {
		Dictionary data = p_array[i];
		ERR_FAIL_COND(!data.has("version"));

		uint32_t po_version = data["version"];

		if (po_version == 1) {
			ERR_FAIL_COND(!data.has("vertices"));
			ERR_FAIL_COND(!data.has("elevation"));
			ERR_FAIL_COND(!data.has("height"));
			ERR_FAIL_COND(!data.has("carve"));
		}

		ProjectedObstruction projected_obstruction;
		projected_obstruction.vertices = Vector<float>(data["vertices"]);
		projected_obstruction.elevation = data["elevation"];
		projected_obstruction.height = data["height"];
		projected_obstruction.carve = data["carve"];

		RWLockWrite write_lock(geometry_rwlock);
		_projected_obstructions.push_back(projected_obstruction);
		bounds_dirty = true;
	}
}

Vector<NavigationMeshSourceGeometryData3D::ProjectedObstruction> NavigationMeshSourceGeometryData3D::_get_projected_obstructions() const {
	RWLockRead read_lock(geometry_rwlock);
	return _projected_obstructions;
}

Array NavigationMeshSourceGeometryData3D::get_projected_obstructions() const {
	RWLockRead read_lock(geometry_rwlock);

	Array ret;
	ret.resize(_projected_obstructions.size());

	for (int i = 0; i < _projected_obstructions.size(); i++) {
		const ProjectedObstruction &projected_obstruction = _projected_obstructions[i];

		Dictionary data;
		data["version"] = (int)ProjectedObstruction::VERSION;
		data["vertices"] = projected_obstruction.vertices;
		data["elevation"] = projected_obstruction.elevation;
		data["height"] = projected_obstruction.height;
		data["carve"] = projected_obstruction.carve;

		ret[i] = data;
	}

	return ret;
}

bool NavigationMeshSourceGeometryData3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "projected_obstructions") {
		set_projected_obstructions(p_value);
		return true;
	}
	return false;
}

bool NavigationMeshSourceGeometryData3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "projected_obstructions") {
		r_ret = get_projected_obstructions();
		return true;
	}
	return false;
}

void NavigationMeshSourceGeometryData3D::set_data(const Vector<float> &p_vertices, const Vector<int> &p_indices, Vector<ProjectedObstruction> &p_projected_obstructions) {
	RWLockWrite write_lock(geometry_rwlock);
	vertices = p_vertices;
	indices = p_indices;
	_projected_obstructions = p_projected_obstructions;
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData3D::get_data(Vector<float> &r_vertices, Vector<int> &r_indices, Vector<ProjectedObstruction> &r_projected_obstructions) {
	RWLockRead read_lock(geometry_rwlock);
	r_vertices = vertices;
	r_indices = indices;
	r_projected_obstructions = _projected_obstructions;
}

AABB NavigationMeshSourceGeometryData3D::get_bounds() {
	geometry_rwlock.read_lock();

	if (bounds_dirty) {
		geometry_rwlock.read_unlock();
		RWLockWrite write_lock(geometry_rwlock);

		bounds_dirty = false;
		bounds = AABB();
		bool first_vertex = true;

		for (int i = 0; i < vertices.size() / 3; i++) {
			const Vector3 vertex = Vector3(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
			if (first_vertex) {
				first_vertex = false;
				bounds.position = vertex;
			} else {
				bounds.expand_to(vertex);
			}
		}
		for (const ProjectedObstruction &projected_obstruction : _projected_obstructions) {
			for (int i = 0; i < projected_obstruction.vertices.size() / 3; i++) {
				const Vector3 vertex = Vector3(projected_obstruction.vertices[i * 3], projected_obstruction.vertices[i * 3 + 1], projected_obstruction.vertices[i * 3 + 2]);
				if (first_vertex) {
					first_vertex = false;
					bounds.position = vertex;
				} else {
					bounds.expand_to(vertex);
				}
			}
		}
	} else {
		geometry_rwlock.read_unlock();
	}

	RWLockRead read_lock(geometry_rwlock);
	return bounds;
}

void NavigationMeshSourceGeometryData3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_vertices", "vertices"), &NavigationMeshSourceGeometryData3D::set_vertices);
	ClassDB::bind_method(D_METHOD("get_vertices"), &NavigationMeshSourceGeometryData3D::get_vertices);

	ClassDB::bind_method(D_METHOD("set_indices", "indices"), &NavigationMeshSourceGeometryData3D::set_indices);
	ClassDB::bind_method(D_METHOD("get_indices"), &NavigationMeshSourceGeometryData3D::get_indices);

	ClassDB::bind_method(D_METHOD("append_arrays", "vertices", "indices"), &NavigationMeshSourceGeometryData3D::append_arrays);

	ClassDB::bind_method(D_METHOD("clear"), &NavigationMeshSourceGeometryData3D::clear);
	ClassDB::bind_method(D_METHOD("has_data"), &NavigationMeshSourceGeometryData3D::has_data);

	ClassDB::bind_method(D_METHOD("add_mesh", "mesh", "xform"), &NavigationMeshSourceGeometryData3D::add_mesh);
	ClassDB::bind_method(D_METHOD("add_mesh_array", "mesh_array", "xform"), &NavigationMeshSourceGeometryData3D::add_mesh_array);
	ClassDB::bind_method(D_METHOD("add_faces", "faces", "xform"), &NavigationMeshSourceGeometryData3D::add_faces);
	ClassDB::bind_method(D_METHOD("merge", "other_geometry"), &NavigationMeshSourceGeometryData3D::merge);

	ClassDB::bind_method(D_METHOD("add_projected_obstruction", "vertices", "elevation", "height", "carve"), &NavigationMeshSourceGeometryData3D::add_projected_obstruction);
	ClassDB::bind_method(D_METHOD("clear_projected_obstructions"), &NavigationMeshSourceGeometryData3D::clear_projected_obstructions);
	ClassDB::bind_method(D_METHOD("set_projected_obstructions", "projected_obstructions"), &NavigationMeshSourceGeometryData3D::set_projected_obstructions);
	ClassDB::bind_method(D_METHOD("get_projected_obstructions"), &NavigationMeshSourceGeometryData3D::get_projected_obstructions);

	ClassDB::bind_method(D_METHOD("get_bounds"), &NavigationMeshSourceGeometryData3D::get_bounds);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "vertices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_vertices", "get_vertices");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "indices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_indices", "get_indices");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "projected_obstructions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_projected_obstructions", "get_projected_obstructions");
}
