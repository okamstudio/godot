/**************************************************************************/
/*  polygon_2d.h                                                          */
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

#ifndef POLYGON_2D_H
#define POLYGON_2D_H

#include "scene/2d/node_2d.h"

class NavigationPolygon;
class NavigationMeshSourceGeometryData2D;

class Polygon2D : public Node2D {
	GDCLASS(Polygon2D, Node2D);

	Vector<Vector2> polygon;
	Vector<Vector2> uv;
	Vector<Color> vertex_colors;
	Array polygons;
	int internal_vertices = 0;

	struct Bone {
		NodePath path;
		Vector<float> weights;
	};

	Vector<Bone> bone_weights;

	Color color = Color(1, 1, 1);
	Ref<Texture2D> texture;

	Size2 tex_scale = Vector2(1, 1);
	Vector2 tex_ofs;
	bool tex_tile = true;
	real_t tex_rot = 0.0;
	bool invert = false;
	real_t invert_border = 100.0;
	bool antialiased = false;

	Vector2 offset;
	mutable bool rect_cache_dirty = true;
	mutable Rect2 item_rect;

	NodePath skeleton;
	ObjectID current_skeleton_id;

	Array _get_bones() const;
	void _set_bones(const Array &p_bones);

	void _skeleton_bone_setup_changed();

	RID mesh;

protected:
	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
#ifdef TOOLS_ENABLED
	virtual Dictionary _edit_get_state() const override;
	virtual void _edit_set_state(const Dictionary &p_state) override;

	virtual void _edit_set_pivot(const Point2 &p_pivot) override;
	virtual Point2 _edit_get_pivot() const override;
	virtual bool _edit_use_pivot() const override;
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;

	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;
#endif // DEBUG_ENABLED

	void set_polygon(const Vector<Vector2> &p_polygon);
	Vector<Vector2> get_polygon() const;

	void set_internal_vertex_count(int p_count);
	int get_internal_vertex_count() const;

	void set_uv(const Vector<Vector2> &p_uv);
	Vector<Vector2> get_uv() const;

	void set_polygons(const Array &p_polygons);
	Array get_polygons() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_vertex_colors(const Vector<Color> &p_colors);
	Vector<Color> get_vertex_colors() const;

	void set_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_texture() const;

	void set_texture_offset(const Vector2 &p_offset);
	Vector2 get_texture_offset() const;

	void set_texture_rotation(real_t p_rot);
	real_t get_texture_rotation() const;

	void set_texture_scale(const Size2 &p_scale);
	Size2 get_texture_scale() const;

	void set_invert(bool p_invert);
	bool get_invert() const;

	void set_antialiased(bool p_antialiased);
	bool get_antialiased() const;

	void set_invert_border(real_t p_invert_border);
	real_t get_invert_border() const;

	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	void add_bone(const NodePath &p_path = NodePath(), const Vector<float> &p_weights = Vector<float>());
	int get_bone_count() const;
	NodePath get_bone_path(int p_index) const;
	Vector<float> get_bone_weights(int p_index) const;
	void erase_bone(int p_idx);
	void clear_bones();
	void set_bone_weights(int p_index, const Vector<float> &p_weights);
	void set_bone_path(int p_index, const NodePath &p_path);

	void set_skeleton(const NodePath &p_skeleton);
	NodePath get_skeleton() const;

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node);

	Polygon2D();
	~Polygon2D();
};

#endif // POLYGON_2D_H
