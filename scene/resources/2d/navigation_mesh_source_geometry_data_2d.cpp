/**************************************************************************/
/*  navigation_mesh_source_geometry_data_2d.cpp                           */
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

#include "navigation_mesh_source_geometry_data_2d.h"

#include "core/variant/typed_array.h"

void NavigationMeshSourceGeometryData2D::clear() {
	RWLockWrite write_lock(geometry_rwlock);
	traversable_outlines.clear();
	obstruction_outlines.clear();
	_projected_obstructions.clear();
	bounds_dirty = true;
}

bool NavigationMeshSourceGeometryData2D::has_data() {
	RWLockRead read_lock(geometry_rwlock);
	return traversable_outlines.size();
}

void NavigationMeshSourceGeometryData2D::clear_projected_obstructions() {
	RWLockWrite write_lock(geometry_rwlock);
	_projected_obstructions.clear();
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::_set_traversable_outlines(const Vector<Vector<Vector2>> &p_traversable_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	traversable_outlines = p_traversable_outlines;
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::_set_obstruction_outlines(const Vector<Vector<Vector2>> &p_obstruction_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	obstruction_outlines = p_obstruction_outlines;
	bounds_dirty = true;
}

const Vector<Vector<Vector2>> &NavigationMeshSourceGeometryData2D::_get_traversable_outlines() const {
	RWLockRead read_lock(geometry_rwlock);
	return traversable_outlines;
}

const Vector<Vector<Vector2>> &NavigationMeshSourceGeometryData2D::_get_obstruction_outlines() const {
	RWLockRead read_lock(geometry_rwlock);
	return obstruction_outlines;
}

void NavigationMeshSourceGeometryData2D::_add_traversable_outline(const Vector<Vector2> &p_shape_outline) {
	if (p_shape_outline.size() > 1) {
		RWLockWrite write_lock(geometry_rwlock);
		traversable_outlines.push_back(p_shape_outline);
		bounds_dirty = true;
	}
}

void NavigationMeshSourceGeometryData2D::_add_obstruction_outline(const Vector<Vector2> &p_shape_outline) {
	if (p_shape_outline.size() > 1) {
		RWLockWrite write_lock(geometry_rwlock);
		obstruction_outlines.push_back(p_shape_outline);
		bounds_dirty = true;
	}
}

void NavigationMeshSourceGeometryData2D::set_traversable_outlines(const TypedArray<Vector<Vector2>> &p_traversable_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	traversable_outlines.resize(p_traversable_outlines.size());
	for (int i = 0; i < p_traversable_outlines.size(); i++) {
		traversable_outlines.write[i] = p_traversable_outlines[i];
	}
	bounds_dirty = true;
}

TypedArray<Vector<Vector2>> NavigationMeshSourceGeometryData2D::get_traversable_outlines() const {
	RWLockRead read_lock(geometry_rwlock);
	TypedArray<Vector<Vector2>> typed_array_traversable_outlines;
	typed_array_traversable_outlines.resize(traversable_outlines.size());
	for (int i = 0; i < typed_array_traversable_outlines.size(); i++) {
		typed_array_traversable_outlines[i] = traversable_outlines[i];
	}

	return typed_array_traversable_outlines;
}

void NavigationMeshSourceGeometryData2D::set_obstruction_outlines(const TypedArray<Vector<Vector2>> &p_obstruction_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	obstruction_outlines.resize(p_obstruction_outlines.size());
	for (int i = 0; i < p_obstruction_outlines.size(); i++) {
		obstruction_outlines.write[i] = p_obstruction_outlines[i];
	}
	bounds_dirty = true;
}

TypedArray<Vector<Vector2>> NavigationMeshSourceGeometryData2D::get_obstruction_outlines() const {
	RWLockRead read_lock(geometry_rwlock);
	TypedArray<Vector<Vector2>> typed_array_obstruction_outlines;
	typed_array_obstruction_outlines.resize(obstruction_outlines.size());
	for (int i = 0; i < typed_array_obstruction_outlines.size(); i++) {
		typed_array_obstruction_outlines[i] = obstruction_outlines[i];
	}

	return typed_array_obstruction_outlines;
}

void NavigationMeshSourceGeometryData2D::append_traversable_outlines(const TypedArray<Vector<Vector2>> &p_traversable_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	int traversable_outlines_size = traversable_outlines.size();
	traversable_outlines.resize(traversable_outlines_size + p_traversable_outlines.size());
	for (int i = traversable_outlines_size; i < p_traversable_outlines.size(); i++) {
		traversable_outlines.write[i] = p_traversable_outlines[i];
	}
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::append_obstruction_outlines(const TypedArray<Vector<Vector2>> &p_obstruction_outlines) {
	RWLockWrite write_lock(geometry_rwlock);
	int obstruction_outlines_size = obstruction_outlines.size();
	obstruction_outlines.resize(obstruction_outlines_size + p_obstruction_outlines.size());
	for (int i = obstruction_outlines_size; i < p_obstruction_outlines.size(); i++) {
		obstruction_outlines.write[i] = p_obstruction_outlines[i];
	}
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::add_traversable_outline(const PackedVector2Array &p_shape_outline) {
	if (p_shape_outline.size() > 1) {
		RWLockWrite write_lock(geometry_rwlock);
		Vector<Vector2> traversable_outline;
		traversable_outline.resize(p_shape_outline.size());
		for (int i = 0; i < p_shape_outline.size(); i++) {
			traversable_outline.write[i] = p_shape_outline[i];
		}
		traversable_outlines.push_back(traversable_outline);
		bounds_dirty = true;
	}
}

void NavigationMeshSourceGeometryData2D::add_obstruction_outline(const PackedVector2Array &p_shape_outline) {
	if (p_shape_outline.size() > 1) {
		RWLockWrite write_lock(geometry_rwlock);
		Vector<Vector2> obstruction_outline;
		obstruction_outline.resize(p_shape_outline.size());
		for (int i = 0; i < p_shape_outline.size(); i++) {
			obstruction_outline.write[i] = p_shape_outline[i];
		}
		obstruction_outlines.push_back(obstruction_outline);
		bounds_dirty = true;
	}
}

void NavigationMeshSourceGeometryData2D::merge(const Ref<NavigationMeshSourceGeometryData2D> &p_other_geometry) {
	ERR_FAIL_COND(p_other_geometry.is_null());

	Vector<Vector<Vector2>> other_traversable_outlines;
	Vector<Vector<Vector2>> other_obstruction_outlines;
	Vector<ProjectedObstruction> other_projected_obstructions;

	p_other_geometry->get_data(other_traversable_outlines, other_obstruction_outlines, other_projected_obstructions);

	RWLockWrite write_lock(geometry_rwlock);
	traversable_outlines.append_array(other_traversable_outlines);
	obstruction_outlines.append_array(other_obstruction_outlines);
	_projected_obstructions.append_array(other_projected_obstructions);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::add_projected_obstruction(const Vector<Vector2> &p_vertices, bool p_carve) {
	ERR_FAIL_COND(p_vertices.size() < 2);

	ProjectedObstruction projected_obstruction;
	projected_obstruction.vertices.resize(p_vertices.size() * 2);
	projected_obstruction.carve = p_carve;

	float *obstruction_vertices_ptrw = projected_obstruction.vertices.ptrw();

	int vertex_index = 0;
	for (const Vector2 &vertex : p_vertices) {
		obstruction_vertices_ptrw[vertex_index++] = vertex.x;
		obstruction_vertices_ptrw[vertex_index++] = vertex.y;
	}

	RWLockWrite write_lock(geometry_rwlock);
	_projected_obstructions.push_back(projected_obstruction);
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::set_projected_obstructions(const Array &p_array) {
	clear_projected_obstructions();

	for (int i = 0; i < p_array.size(); i++) {
		Dictionary data = p_array[i];
		ERR_FAIL_COND(!data.has("version"));

		uint32_t po_version = data["version"];

		if (po_version == 1) {
			ERR_FAIL_COND(!data.has("vertices"));
			ERR_FAIL_COND(!data.has("carve"));
		}

		ProjectedObstruction projected_obstruction;
		projected_obstruction.vertices = Vector<float>(data["vertices"]);
		projected_obstruction.carve = data["carve"];

		RWLockWrite write_lock(geometry_rwlock);
		_projected_obstructions.push_back(projected_obstruction);
		bounds_dirty = true;
	}
}

Vector<NavigationMeshSourceGeometryData2D::ProjectedObstruction> NavigationMeshSourceGeometryData2D::_get_projected_obstructions() const {
	RWLockRead read_lock(geometry_rwlock);
	return _projected_obstructions;
}

Array NavigationMeshSourceGeometryData2D::get_projected_obstructions() const {
	RWLockRead read_lock(geometry_rwlock);

	Array ret;
	ret.resize(_projected_obstructions.size());

	for (int i = 0; i < _projected_obstructions.size(); i++) {
		const ProjectedObstruction &projected_obstruction = _projected_obstructions[i];

		Dictionary data;
		data["version"] = (int)ProjectedObstruction::VERSION;
		data["vertices"] = projected_obstruction.vertices;
		data["carve"] = projected_obstruction.carve;

		ret[i] = data;
	}

	return ret;
}

bool NavigationMeshSourceGeometryData2D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "projected_obstructions") {
		set_projected_obstructions(p_value);
		return true;
	}
	return false;
}

bool NavigationMeshSourceGeometryData2D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "projected_obstructions") {
		r_ret = get_projected_obstructions();
		return true;
	}
	return false;
}

void NavigationMeshSourceGeometryData2D::set_data(const Vector<Vector<Vector2>> &p_traversable_outlines, const Vector<Vector<Vector2>> &p_obstruction_outlines, Vector<ProjectedObstruction> &p_projected_obstructions) {
	RWLockWrite write_lock(geometry_rwlock);
	traversable_outlines = p_traversable_outlines;
	obstruction_outlines = p_obstruction_outlines;
	_projected_obstructions = p_projected_obstructions;
	bounds_dirty = true;
}

void NavigationMeshSourceGeometryData2D::get_data(Vector<Vector<Vector2>> &r_traversable_outlines, Vector<Vector<Vector2>> &r_obstruction_outlines, Vector<ProjectedObstruction> &r_projected_obstructions) {
	RWLockRead read_lock(geometry_rwlock);
	r_traversable_outlines = traversable_outlines;
	r_obstruction_outlines = obstruction_outlines;
	r_projected_obstructions = _projected_obstructions;
}

Rect2 NavigationMeshSourceGeometryData2D::get_bounds() {
	geometry_rwlock.read_lock();

	if (bounds_dirty) {
		geometry_rwlock.read_unlock();
		RWLockWrite write_lock(geometry_rwlock);

		bounds_dirty = false;
		bounds = Rect2();
		bool first_vertex = true;

		for (const Vector<Vector2> &traversable_outline : traversable_outlines) {
			for (const Vector2 &traversable_point : traversable_outline) {
				if (first_vertex) {
					first_vertex = false;
					bounds.position = traversable_point;
				} else {
					bounds.expand_to(traversable_point);
				}
			}
		}

		for (const Vector<Vector2> &obstruction_outline : obstruction_outlines) {
			for (const Vector2 &obstruction_point : obstruction_outline) {
				if (first_vertex) {
					first_vertex = false;
					bounds.position = obstruction_point;
				} else {
					bounds.expand_to(obstruction_point);
				}
			}
		}

		for (const ProjectedObstruction &projected_obstruction : _projected_obstructions) {
			for (int i = 0; i < projected_obstruction.vertices.size() / 2; i++) {
				const Vector2 vertex = Vector2(projected_obstruction.vertices[i * 2], projected_obstruction.vertices[i * 2 + 1]);
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

void NavigationMeshSourceGeometryData2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &NavigationMeshSourceGeometryData2D::clear);
	ClassDB::bind_method(D_METHOD("has_data"), &NavigationMeshSourceGeometryData2D::has_data);

	ClassDB::bind_method(D_METHOD("set_traversable_outlines", "traversable_outlines"), &NavigationMeshSourceGeometryData2D::set_traversable_outlines);
	ClassDB::bind_method(D_METHOD("get_traversable_outlines"), &NavigationMeshSourceGeometryData2D::get_traversable_outlines);

	ClassDB::bind_method(D_METHOD("set_obstruction_outlines", "obstruction_outlines"), &NavigationMeshSourceGeometryData2D::set_obstruction_outlines);
	ClassDB::bind_method(D_METHOD("get_obstruction_outlines"), &NavigationMeshSourceGeometryData2D::get_obstruction_outlines);

	ClassDB::bind_method(D_METHOD("append_traversable_outlines", "traversable_outlines"), &NavigationMeshSourceGeometryData2D::append_traversable_outlines);
	ClassDB::bind_method(D_METHOD("append_obstruction_outlines", "obstruction_outlines"), &NavigationMeshSourceGeometryData2D::append_obstruction_outlines);

	ClassDB::bind_method(D_METHOD("add_traversable_outline", "shape_outline"), &NavigationMeshSourceGeometryData2D::add_traversable_outline);
	ClassDB::bind_method(D_METHOD("add_obstruction_outline", "shape_outline"), &NavigationMeshSourceGeometryData2D::add_obstruction_outline);

	ClassDB::bind_method(D_METHOD("merge", "other_geometry"), &NavigationMeshSourceGeometryData2D::merge);

	ClassDB::bind_method(D_METHOD("add_projected_obstruction", "vertices", "carve"), &NavigationMeshSourceGeometryData2D::add_projected_obstruction);
	ClassDB::bind_method(D_METHOD("clear_projected_obstructions"), &NavigationMeshSourceGeometryData2D::clear_projected_obstructions);
	ClassDB::bind_method(D_METHOD("set_projected_obstructions", "projected_obstructions"), &NavigationMeshSourceGeometryData2D::set_projected_obstructions);
	ClassDB::bind_method(D_METHOD("get_projected_obstructions"), &NavigationMeshSourceGeometryData2D::get_projected_obstructions);

	ClassDB::bind_method(D_METHOD("get_bounds"), &NavigationMeshSourceGeometryData2D::get_bounds);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "traversable_outlines", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_traversable_outlines", "get_traversable_outlines");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "obstruction_outlines", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_obstruction_outlines", "get_obstruction_outlines");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "projected_obstructions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_projected_obstructions", "get_projected_obstructions");
}
