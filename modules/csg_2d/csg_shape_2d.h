/**************************************************************************/
/*  csg_shape_2d.h                                                        */
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

#ifndef CSG_SHAPE_2D_H
#define CSG_SHAPE_2D_H

#include "csg_2d.h"

#include "scene/2d/node_2d.h"

class TPPLPoly;

class CSGShape2D : public Node2D {
	GDCLASS(CSGShape2D, Node2D);

public:
	enum Operation {
		OPERATION_UNION,
		OPERATION_INTERSECTION,
		OPERATION_SUBTRACTION,
	};

	enum CollisionShapeType {
		COLLISION_SHAPE_TYPE_CONCAVE_SEGMENTS,
		COLLISION_SHAPE_TYPE_CONVEX_POLYGONS,
		COLLISION_SHAPE_TYPE_MAX,
	};

private:
	Operation operation = OPERATION_UNION;
	CSGShape2D *parent_shape = nullptr;

	CSGBrush2D *brush = nullptr;

	Rect2 node_rect;

	bool dirty = false;
	bool last_visible = false;

	bool shape_update_forced = false;

	bool use_vertex_color = false;
	Color vertex_color = Color(1.0, 1.0, 1.0, 1.0);

	bool use_collision = false;
	CollisionShapeType collision_shape_type = CollisionShapeType::COLLISION_SHAPE_TYPE_CONCAVE_SEGMENTS;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t collision_priority = 1.0;
	LocalVector<Ref<ConvexPolygonShape2D>> root_collision_shapes_convex;
	LocalVector<Ref<ConcavePolygonShape2D>> root_collision_shapes_concave;
	RID root_collision_instance;
	RID root_collision_debug_instance;
	Transform2D debug_shape_old_transform;

	Ref<ArrayMesh> root_mesh;
	Ref<ArrayMesh> root_edge_line_mesh;
	Ref<ArrayMesh> root_outline_mesh;

protected:
	Ref<ArrayMesh> brush_mesh;
	LocalVector<Vector<Vector2>> brush_outlines;

private:
	Vector<Vector2> mesh_vertices;
	Vector<int> mesh_triangles;
	LocalVector<Vector<int>> mesh_convex_polygons;
	LocalVector<Vector<Vector2>> mesh_outlines;
	HashMap<Vector2, int> points_map;

	void _update_shape();
	void _update_collision_shapes();
	bool _is_debug_collision_shape_visible();
	void _update_debug_collision_shape();
	void _clear_debug_collision_shape();
	void _on_transform_changed();

	void draw_shape();

	void _create_collision();
	void _free_collision();

	bool debug_show_brush = false;

protected:
	void _notification(int p_what);
	virtual CSGBrush2D *_build_brush() = 0;
	void _make_dirty(bool p_parent_removing = false);

	static void _bind_methods();

	friend class CSGCombiner2D;
	CSGBrush2D *_get_brush();

	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_debug_show_brush(bool p_enable);
	bool get_debug_show_brush() const;

	Vector<Vector2> get_mesh_vertices() { return mesh_vertices; }
	Vector<int> get_mesh_triangles() { return mesh_triangles; }
	const LocalVector<Vector<int>> &get_mesh_convex_polygons() { return mesh_convex_polygons; }
	const LocalVector<Vector<Vector2>> &get_mesh_outlines() { return mesh_outlines; }

	void set_operation(Operation p_operation);
	Operation get_operation() const;

	virtual Rect2 get_rect() const;

	void set_use_vertex_color(bool p_enable);
	bool is_using_vertex_color() const;

	void set_vertex_color(const Color &p_color);
	Color get_vertex_color() const;

	void set_use_collision(bool p_enable);
	bool is_using_collision() const;

	void set_collision_shape_type(CollisionShapeType p_type);
	CollisionShapeType get_collision_shape_type() const;

	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;

	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	void set_collision_layer_value(int p_layer_number, bool p_value);
	bool get_collision_layer_value(int p_layer_number) const;

	void set_collision_mask_value(int p_layer_number, bool p_value);
	bool get_collision_mask_value(int p_layer_number) const;

	void set_collision_priority(real_t p_priority);
	real_t get_collision_priority() const;

	bool is_root_shape() const;
	void force_shape_update();

	Ref<ArrayMesh> bake_static_mesh() const;
	Array bake_collision_shapes() const;
	Array bake_light_occluders() const;
	Ref<NavigationPolygon> bake_navigation_mesh() const;

	bool is_point_in_outlines(const Point2 &p_point) const;

	static void _recursive_process_polytree_items(List<TPPLPoly> &p_tppl_in_polygon, const Clipper2Lib::PolyPathD *p_polypath_item, LocalVector<Vector<Vector2>> &p_mesh_outlines);

	CSGShape2D();
	~CSGShape2D();
};

VARIANT_ENUM_CAST(CSGShape2D::Operation)
VARIANT_ENUM_CAST(CSGShape2D::CollisionShapeType);

class CSGCombiner2D : public CSGShape2D {
	GDCLASS(CSGCombiner2D, CSGShape2D);

	mutable Rect2 item_rect;
	mutable bool rect_cache_dirty = true;

private:
	virtual CSGBrush2D *_build_brush() override;

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;
#endif // DEBUG_ENABLED

	CSGCombiner2D() {}
};

class CSGPrimitive2D : public CSGShape2D {
	GDCLASS(CSGPrimitive2D, CSGShape2D);

protected:
	static void _bind_methods();

public:
	CSGPrimitive2D() {}
};

#endif // CSG_SHAPE_2D_H
