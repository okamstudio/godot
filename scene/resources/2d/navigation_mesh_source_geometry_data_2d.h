/**************************************************************************/
/*  navigation_mesh_source_geometry_data_2d.h                             */
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

#include "core/io/resource.h"
#include "core/os/rw_lock.h"

class NavigationMeshSourceGeometryData2D : public Resource {
	friend class NavMeshGenerator2D;

	GDCLASS(NavigationMeshSourceGeometryData2D, Resource);
	RWLock geometry_rwlock;

	Vector<Vector<Vector2>> traversable_outlines;
	Vector<Vector<Vector2>> obstruction_outlines;

	Rect2 bounds;
	bool bounds_dirty = true;

public:
	struct ProjectedObstruction;

private:
	Vector<ProjectedObstruction> _projected_obstructions;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	static void _bind_methods();

public:
	struct ProjectedObstruction {
		static inline uint32_t VERSION = 1; // Increase when format changes so we can detect outdated formats and provide compatibility.

		Vector<float> vertices;
		bool carve = false;
	};

	void _set_traversable_outlines(const Vector<Vector<Vector2>> &p_traversable_outlines);
	const Vector<Vector<Vector2>> &_get_traversable_outlines() const;

	void _set_obstruction_outlines(const Vector<Vector<Vector2>> &p_obstruction_outlines);
	const Vector<Vector<Vector2>> &_get_obstruction_outlines() const;

	void _add_traversable_outline(const Vector<Vector2> &p_shape_outline);
	void _add_obstruction_outline(const Vector<Vector2> &p_shape_outline);

	// kept root node transform here on the geometry data
	// if we add this transform to all exposed functions we need to break comp on all functions later
	// when navmesh changes from global transform to relative to navregion
	// but if it stays here we can just remove it and change the internal functions only
	Transform2D root_node_transform;

	void set_traversable_outlines(const TypedArray<Vector<Vector2>> &p_traversable_outlines);
	TypedArray<Vector<Vector2>> get_traversable_outlines() const;

	void set_obstruction_outlines(const TypedArray<Vector<Vector2>> &p_obstruction_outlines);
	TypedArray<Vector<Vector2>> get_obstruction_outlines() const;

	void append_traversable_outlines(const TypedArray<Vector<Vector2>> &p_traversable_outlines);
	void append_obstruction_outlines(const TypedArray<Vector<Vector2>> &p_obstruction_outlines);

	void add_traversable_outline(const PackedVector2Array &p_shape_outline);
	void add_obstruction_outline(const PackedVector2Array &p_shape_outline);

	bool has_data();
	void clear();
	void clear_projected_obstructions();

	void add_projected_obstruction(const Vector<Vector2> &p_vertices, bool p_carve);
	Vector<ProjectedObstruction> _get_projected_obstructions() const;

	void set_projected_obstructions(const Array &p_array);
	Array get_projected_obstructions() const;

	void merge(const Ref<NavigationMeshSourceGeometryData2D> &p_other_geometry);

	void set_data(const Vector<Vector<Vector2>> &p_traversable_outlines, const Vector<Vector<Vector2>> &p_obstruction_outlines, Vector<ProjectedObstruction> &p_projected_obstructions);
	void get_data(Vector<Vector<Vector2>> &r_traversable_outlines, Vector<Vector<Vector2>> &r_obstruction_outlines, Vector<ProjectedObstruction> &r_projected_obstructions);

	Rect2 get_bounds();

	NavigationMeshSourceGeometryData2D() {}
	~NavigationMeshSourceGeometryData2D() { clear(); }
};
