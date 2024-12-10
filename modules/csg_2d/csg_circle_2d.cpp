/**************************************************************************/
/*  csg_circle_2d.cpp                                                     */
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

#include "csg_circle_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

#ifdef DEBUG_ENABLED
Rect2 CSGCircle2D::_edit_get_rect() const {
	Rect2 rect = Rect2(Vector2(-radius, -radius), Vector2(radius, radius) * 2.0);
	rect.position -= rect.size * 0.3;
	rect.size += rect.size * 0.6;
	return rect;
}

bool CSGCircle2D::_edit_use_rect() const {
	return true;
}

bool CSGCircle2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return p_point.length() <= radius;
}
#endif // DEBUG_ENABLED

CSGBrush2D *CSGCircle2D::_build_brush() {
	CSGBrush2D *new_brush = memnew(CSGBrush2D);

	LocalVector<CSGBrush2D::Outline> &outlines = new_brush->outlines;
	outlines.resize(1);
	CSGBrush2D::Outline &outline = outlines[0];

	LocalVector<Vector2> &vertices = outline.vertices;
	vertices.resize(radial_segments + 1);

	const real_t turn_step = Math_TAU / float(radial_segments);
	for (int i = 0; i < radial_segments; i++) {
		vertices[i] = Vector2(Math::cos(i * turn_step), Math::sin(i * turn_step)) * get_radius();
	}
	vertices[radial_segments] = vertices[0];

	brush_outlines.resize(1);
	brush_outlines[0] = vertices;

	new_brush->build_from_outlines(outlines);

	return new_brush;
}

void CSGCircle2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CSGCircle2D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CSGCircle2D::get_radius);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "radial_segments"), &CSGCircle2D::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CSGCircle2D::get_radial_segments);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,suffix:px"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "3,100,1"), "set_radial_segments", "get_radial_segments");
}

void CSGCircle2D::set_radius(const float p_radius) {
	ERR_FAIL_COND(p_radius <= 0);
	radius = p_radius;
	_make_dirty();
}

float CSGCircle2D::get_radius() const {
	return radius;
}

void CSGCircle2D::set_radial_segments(const int p_radial_segments) {
	radial_segments = p_radial_segments > 3 ? p_radial_segments : 3;
	_make_dirty();
}

int CSGCircle2D::get_radial_segments() const {
	return radial_segments;
}
