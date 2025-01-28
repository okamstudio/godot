/**************************************************************************/
/*  circle_shape_2d.cpp                                                   */
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

#include "circle_shape_2d.h"

#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

bool CircleShape2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return p_point.length() < get_radius() + p_tolerance;
}

void CircleShape2D::_update_shape() {
	PhysicsServer2D::get_singleton()->shape_set_data(get_rid(), radius);
	emit_changed();
}

void CircleShape2D::set_radius(real_t p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0, "CircleShape2D radius cannot be negative.");
	if (radius == p_radius) {
		return;
	}
	radius = p_radius;
	_update_shape();
}

real_t CircleShape2D::get_radius() const {
	return radius;
}

void CircleShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CircleShape2D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CircleShape2D::get_radius);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:px"), "set_radius", "get_radius");
}

Rect2 CircleShape2D::get_rect() const {
	Rect2 rect;
	rect.position = -Point2(get_radius(), get_radius());
	rect.size = Point2(get_radius(), get_radius()) * 2.0;
	return rect;
}

real_t CircleShape2D::get_enclosing_radius() const {
	return radius;
}

void CircleShape2D::draw(const RID &p_to_rid, const Color &p_color) {
	Vector<Vector2> points;
	points.resize(24);

	const real_t turn_step = Math_TAU / 24.0;
	for (int i = 0; i < 24; i++) {
		points.write[i] = Vector2(Math::cos(i * turn_step), Math::sin(i * turn_step)) * get_radius();
	}

	Vector<Color> col = { p_color };
	RenderingServer::get_singleton()->canvas_item_add_polygon(p_to_rid, points, col);

	if (is_collision_outline_enabled()) {
		points.push_back(points[0]);
		col = { Color(p_color, 1.0) };
		RenderingServer::get_singleton()->canvas_item_add_polyline(p_to_rid, points, col);
	}
}

CircleShape2D::CircleShape2D() :
		Shape2D(PhysicsServer2D::get_singleton()->circle_shape_create()) {
	_update_shape();
}
