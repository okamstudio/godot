/**************************************************************************/
/*  segment_shape_2d.cpp                                                  */
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

#include "segment_shape_2d.h"

#include "core/math/geometry_2d.h"
#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

bool SegmentShape2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	Vector2 l[2] = { a, b };
	Vector2 closest = Geometry2D::get_closest_point_to_segment(p_point, l);
	return p_point.distance_to(closest) < p_tolerance;
}

void SegmentShape2D::_update_shape() {
	Rect2 r;
	r.position = a;
	r.size = b;
	PhysicsServer2D::get_singleton()->shape_set_data(get_rid(), r);
	emit_changed();
}

void SegmentShape2D::set_a(const Vector2 &p_a) {
	if (a == p_a) {
		return;
	}
	a = p_a;
	_update_shape();
}

Vector2 SegmentShape2D::get_a() const {
	return a;
}

void SegmentShape2D::set_b(const Vector2 &p_b) {
	if (b == p_b) {
		return;
	}
	b = p_b;
	_update_shape();
}

Vector2 SegmentShape2D::get_b() const {
	return b;
}

void SegmentShape2D::draw(const RID &p_to_rid, const Color &p_color) {
	RenderingServer::get_singleton()->canvas_item_add_line(p_to_rid, a, b, p_color, 3);
}

Rect2 SegmentShape2D::get_rect() const {
	Rect2 rect;
	rect.position = a;
	rect.expand_to(b);
	return rect;
}

real_t SegmentShape2D::get_enclosing_radius() const {
	return (a + b).length();
}

void SegmentShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_a", "a"), &SegmentShape2D::set_a);
	ClassDB::bind_method(D_METHOD("get_a"), &SegmentShape2D::get_a);

	ClassDB::bind_method(D_METHOD("set_b", "b"), &SegmentShape2D::set_b);
	ClassDB::bind_method(D_METHOD("get_b"), &SegmentShape2D::get_b);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "a", PROPERTY_HINT_NONE, "suffix:px"), "set_a", "get_a");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "b", PROPERTY_HINT_NONE, "suffix:px"), "set_b", "get_b");
}

SegmentShape2D::SegmentShape2D() :
		Shape2D(PhysicsServer2D::get_singleton()->segment_shape_create()) {
	a = Vector2();
	b = Vector2(0, 10);
	_update_shape();
}
