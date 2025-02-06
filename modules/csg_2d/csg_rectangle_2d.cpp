/**************************************************************************/
/*  csg_rectangle_2d.cpp                                                  */
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

#include "csg_rectangle_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

#ifdef DEBUG_ENABLED
Rect2 CSGRectangle2D::_edit_get_rect() const {
	Rect2 rect = Rect2(-size * 0.5, size);
	rect.position -= rect.size * 0.3;
	rect.size += rect.size * 0.6;
	return rect;
}

bool CSGRectangle2D::_edit_use_rect() const {
	return true;
}

bool CSGRectangle2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return Rect2(-size * 0.5, size).has_point(p_point);
}
#endif // DEBUG_ENABLED

CSGBrush2D *CSGRectangle2D::_build_brush() {
	CSGBrush2D *new_brush = memnew(CSGBrush2D);

	LocalVector<CSGBrush2D::Outline> &outlines = new_brush->outlines;
	outlines.resize(1);
	CSGBrush2D::Outline &outline = outlines[0];

	LocalVector<Vector2> &vertices = outline.vertices;
	vertices.resize(5);
	vertices[0] = -size * 0.5;
	vertices[1] = Vector2(size.x, -size.y) * 0.5;
	vertices[2] = size * 0.5;
	vertices[3] = Vector2(-size.x, size.y) * 0.5;
	vertices[4] = -size * 0.5;

	brush_outlines.resize(1);
	brush_outlines[0] = vertices;

	new_brush->build_from_outlines(outlines);

	return new_brush;
}

void CSGRectangle2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &CSGRectangle2D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &CSGRectangle2D::get_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "suffix:px"), "set_size", "get_size");
}

void CSGRectangle2D::set_size(const Vector2 &p_size) {
	size = p_size;
	_make_dirty();
}

Vector2 CSGRectangle2D::get_size() const {
	return size;
}
