/**************************************************************************/
/*  csg_polygon_2d.cpp                                                    */
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

#include "csg_polygon_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

#ifdef DEBUG_ENABLED
Rect2 CSGPolygon2D::_edit_get_rect() const {
	Rect2 rect = get_rect();
	rect.position -= rect.size * 0.3;
	rect.size += rect.size * 0.6;
	return rect;
}

bool CSGPolygon2D::_edit_use_rect() const {
	return true;
}

bool CSGPolygon2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return is_point_in_outlines(p_point);
}
#endif // DEBUG_ENABLED

CSGBrush2D *CSGPolygon2D::_build_brush() {
	CSGBrush2D *new_brush = memnew(CSGBrush2D);

	if (polygon.size() < 3) {
		return new_brush;
	}

	LocalVector<CSGBrush2D::Outline> &outlines = new_brush->outlines;
	outlines.resize(1);
	CSGBrush2D::Outline &outline = outlines[0];

	LocalVector<Vector2> &vertices = outline.vertices;
	vertices = polygon;
	vertices.push_back(vertices[0]);

	brush_outlines.resize(1);
	brush_outlines[0] = vertices;

	new_brush->build_from_outlines(outlines);

	return new_brush;
}

void CSGPolygon2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &CSGPolygon2D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &CSGPolygon2D::get_polygon);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
}

void CSGPolygon2D::set_polygon(const Vector<Vector2> &p_polygon) {
	polygon = p_polygon;
	_make_dirty();
}

Vector<Vector2> CSGPolygon2D::get_polygon() const {
	return polygon;
}
