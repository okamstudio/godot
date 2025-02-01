/**************************************************************************/
/*  collision_polygon_2d.h                                                */
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

class CollisionObject2D;

class CollisionPolygon2D : public Node2D {
	GDCLASS(CollisionPolygon2D, Node2D);

public:
	enum BuildMode {
		BUILD_SOLIDS,
		BUILD_SEGMENTS,
	};

protected:
	Rect2 aabb = Rect2(-10, -10, 20, 20);
	BuildMode build_mode = BUILD_SOLIDS;
	Vector<Point2> polygon;
	uint32_t owner_id = 0;
	CollisionObject2D *collision_object = nullptr;
	bool disabled = false;
	bool one_way_collision = false;
	real_t one_way_collision_margin = 1.0;

	Vector<Vector<Vector2>> _decompose_in_convex();

	void _build_polygon();

	void _update_in_shape_owner(bool p_xform_only = false);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;
#endif

	void set_build_mode(BuildMode p_mode);
	BuildMode get_build_mode() const;

	void set_polygon(const Vector<Point2> &p_polygon);
	Vector<Point2> get_polygon() const;

	PackedStringArray get_configuration_warnings() const override;

	void set_disabled(bool p_disabled);
	bool is_disabled() const;

	void set_one_way_collision(bool p_enable);
	bool is_one_way_collision_enabled() const;

	void set_one_way_collision_margin(real_t p_margin);
	real_t get_one_way_collision_margin() const;

	CollisionPolygon2D();
};

VARIANT_ENUM_CAST(CollisionPolygon2D::BuildMode);
