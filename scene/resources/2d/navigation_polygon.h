/**************************************************************************/
/*  navigation_polygon.h                                                  */
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

#include "scene/2d/node_2d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/navigation/navigation_globals.h"

class NavigationPolygon : public Resource {
	GDCLASS(NavigationPolygon, Resource);
	RWLock rwlock;

	Vector<Vector2> vertices;
	Vector<Vector<int>> polygons;
	Vector<Vector<Vector2>> outlines;
	Vector<Vector<Vector2>> baked_outlines;

	mutable Rect2 item_rect;
	mutable bool rect_cache_dirty = true;

	Mutex navigation_mesh_generation;
	// Navigation mesh
	Ref<NavigationMesh> navigation_mesh;

	real_t cell_size = NavigationDefaults2D::navmesh_cell_size;
	real_t border_size = 0.0f;

	Rect2 baking_rect;
	Vector2 baking_rect_offset;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	void _set_polygons(const TypedArray<Vector<int32_t>> &p_array);
	TypedArray<Vector<int32_t>> _get_polygons() const;

	void _set_outlines(const TypedArray<Vector<Vector2>> &p_array);
	TypedArray<Vector<Vector2>> _get_outlines() const;

public:
#ifdef DEBUG_ENABLED
	Rect2 _edit_get_rect() const;
	bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const;
#endif // DEBUG_ENABLED

	enum SamplePartitionType {
		SAMPLE_PARTITION_CONVEX_PARTITION = 0,
		SAMPLE_PARTITION_TRIANGULATE,
		SAMPLE_PARTITION_MAX
	};

	enum ParsedGeometryType {
		PARSED_GEOMETRY_MESH_INSTANCES = 0,
		PARSED_GEOMETRY_STATIC_COLLIDERS,
		PARSED_GEOMETRY_BOTH,
		PARSED_GEOMETRY_MAX
	};

	enum SourceGeometryMode {
		SOURCE_GEOMETRY_ROOT_NODE_CHILDREN = 0,
		SOURCE_GEOMETRY_GROUPS_WITH_CHILDREN,
		SOURCE_GEOMETRY_GROUPS_EXPLICIT,
		SOURCE_GEOMETRY_MAX
	};

	real_t agent_radius = 10.0f;

	SamplePartitionType partition_type = SAMPLE_PARTITION_CONVEX_PARTITION;
	ParsedGeometryType parsed_geometry_type = PARSED_GEOMETRY_BOTH;
	uint32_t parsed_collision_mask = 0xFFFFFFFF;

	SourceGeometryMode source_geometry_mode = SOURCE_GEOMETRY_ROOT_NODE_CHILDREN;
	StringName source_geometry_group_name = "navigation_polygon_source_geometry_group";

	void set_vertices(const Vector<Vector2> &p_vertices);
	Vector<Vector2> get_vertices() const;

	void add_polygon(const Vector<int> &p_polygon);
	int get_polygon_count() const;

	void add_outline(const Vector<Vector2> &p_outline);
	void add_outline_at_index(const Vector<Vector2> &p_outline, int p_index);
	void set_outline(int p_idx, const Vector<Vector2> &p_outline);
	Vector<Vector2> get_outline(int p_idx) const;
	void remove_outline(int p_idx);
	int get_outline_count() const;
	void set_outlines(const Vector<Vector<Vector2>> &p_outlines);
	Vector<Vector<Vector2>> get_outlines() const;

	void clear_outlines();
#ifndef DISABLE_DEPRECATED
	void make_polygons_from_outlines();
#endif // DISABLE_DEPRECATED

	void set_polygons(const Vector<Vector<int>> &p_polygons);
	Vector<Vector<int>> get_polygons() const;
	Vector<int> get_polygon(int p_idx);
	void clear_polygons();

	void set_sample_partition_type(SamplePartitionType p_value);
	SamplePartitionType get_sample_partition_type() const;

	void set_parsed_geometry_type(ParsedGeometryType p_geometry_type);
	ParsedGeometryType get_parsed_geometry_type() const;

	void set_parsed_collision_mask(uint32_t p_mask);
	uint32_t get_parsed_collision_mask() const;

	void set_parsed_collision_mask_value(int p_layer_number, bool p_value);
	bool get_parsed_collision_mask_value(int p_layer_number) const;

	void set_source_geometry_mode(SourceGeometryMode p_geometry_mode);
	SourceGeometryMode get_source_geometry_mode() const;

	void set_source_geometry_group_name(const StringName &p_group_name);
	StringName get_source_geometry_group_name() const;

	void set_agent_radius(real_t p_value);
	real_t get_agent_radius() const;

	Ref<NavigationMesh> get_navigation_mesh();

	void set_cell_size(real_t p_cell_size);
	real_t get_cell_size() const;

	void set_border_size(real_t p_value);
	real_t get_border_size() const;

	void set_baking_rect(const Rect2 &p_rect);
	Rect2 get_baking_rect() const;

	void set_baking_rect_offset(const Vector2 &p_rect_offset);
	Vector2 get_baking_rect_offset() const;

	void clear();

	void set_data(const Vector<Vector2> &p_vertices, const Vector<Vector<int>> &p_polygons);
	void set_data(const Vector<Vector2> &p_vertices, const Vector<Vector<int>> &p_polygons, const Vector<Vector<Vector2>> &p_outlines);
	void get_data(Vector<Vector2> &r_vertices, Vector<Vector<int>> &r_polygons);
	void get_data(Vector<Vector2> &r_vertices, Vector<Vector<int>> &r_polygons, Vector<Vector<Vector2>> &r_outlines);

	NavigationPolygon() {}
	~NavigationPolygon() {}
};

VARIANT_ENUM_CAST(NavigationPolygon::SamplePartitionType);
VARIANT_ENUM_CAST(NavigationPolygon::ParsedGeometryType);
VARIANT_ENUM_CAST(NavigationPolygon::SourceGeometryMode);
