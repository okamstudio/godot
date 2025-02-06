/**************************************************************************/
/*  navigation_mesh_source_geometry_data_3d.h                             */
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

#pragma once

#include "core/os/rw_lock.h"
#include "scene/resources/mesh.h"

class NavigationMeshSourceGeometryData3D : public Resource {
	GDCLASS(NavigationMeshSourceGeometryData3D, Resource);
	RWLock geometry_rwlock;

	Vector<float> vertices;
	Vector<int> indices;

	AABB bounds;
	bool bounds_dirty = true;

public:
	struct ProjectedObstruction;

private:
	Vector<ProjectedObstruction> _projected_obstructions;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	static void _bind_methods();

private:
	void _add_vertex(const Vector3 &p_vec3);
	void _add_mesh(const Ref<Mesh> &p_mesh, const Transform3D &p_xform);
	void _add_mesh_array(const Array &p_array, const Transform3D &p_xform);
	void _add_faces(const PackedVector3Array &p_faces, const Transform3D &p_xform);

public:
	struct ProjectedObstruction {
		static inline uint32_t VERSION = 1; // Increase when format changes so we can detect outdated formats and provide compatibility.

		Vector<float> vertices;
		float elevation = 0.0;
		float height = 0.0;
		bool carve = false;
	};

	// kept root node transform here on the geometry data
	// if we add this transform to all exposed functions we need to break comp on all functions later
	// when navmesh changes from global transform to relative to navregion
	// but if it stays here we can just remove it and change the internal functions only
	Transform3D root_node_transform;

	void set_vertices(const Vector<float> &p_vertices);
	const Vector<float> &get_vertices() const;

	void set_indices(const Vector<int> &p_indices);
	const Vector<int> &get_indices() const;

	void append_arrays(const Vector<float> &p_vertices, const Vector<int> &p_indices);

	bool has_data();
	void clear();
	void clear_projected_obstructions();

	void add_mesh(const Ref<Mesh> &p_mesh, const Transform3D &p_xform);
	void add_mesh_array(const Array &p_mesh_array, const Transform3D &p_xform);
	void add_faces(const PackedVector3Array &p_faces, const Transform3D &p_xform);

	void merge(const Ref<NavigationMeshSourceGeometryData3D> &p_other_geometry);

	void add_projected_obstruction(const Vector<Vector3> &p_vertices, float p_elevation, float p_height, bool p_carve);
	Vector<ProjectedObstruction> _get_projected_obstructions() const;

	void set_projected_obstructions(const Array &p_array);
	Array get_projected_obstructions() const;

	void set_data(const Vector<float> &p_vertices, const Vector<int> &p_indices, Vector<ProjectedObstruction> &p_projected_obstructions);
	void get_data(Vector<float> &r_vertices, Vector<int> &r_indices, Vector<ProjectedObstruction> &r_projected_obstructions);

	AABB get_bounds();

	NavigationMeshSourceGeometryData3D() {}
	~NavigationMeshSourceGeometryData3D() { clear(); }
};
