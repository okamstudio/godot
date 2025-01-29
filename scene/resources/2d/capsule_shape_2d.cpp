/**************************************************************************/
/*  capsule_shape_2d.cpp                                                  */
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

#include "capsule_shape_2d.h"

#include "core/math/geometry_2d.h"
#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

Vector<Vector2> CapsuleShape2D::_get_points() const {
	Vector<Vector2> points;
	const real_t turn_step = Math_TAU / 24.0;
	for (int i = 0; i < 24; i++) {
		Vector2 ofs = Vector2(0, (i > 6 && i <= 18) ? -height * 0.5 + radius : height * 0.5 - radius);

		points.push_back(Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * radius + ofs);
		if (i == 6 || i == 18) {
			points.push_back(Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * radius - ofs);
		}
	}

	return points;
}

bool CapsuleShape2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return Geometry2D::is_point_in_polygon(p_point, _get_points());
}

void CapsuleShape2D::_update_shape() {
	PhysicsServer2D::get_singleton()->shape_set_data(get_rid(), Vector2(radius, height));
	emit_changed();
}

void CapsuleShape2D::set_radius(real_t p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0, "CapsuleShape2D radius cannot be negative.");
	if (radius == p_radius) {
		return;
	}
	radius = p_radius;
	if (radius > height * 0.5) {
		height = radius * 2.0;
	}
	_update_shape();
}

real_t CapsuleShape2D::get_radius() const {
	return radius;
}

void CapsuleShape2D::set_height(real_t p_height) {
	ERR_FAIL_COND_MSG(p_height < 0, "CapsuleShape2D height cannot be negative.");
	if (height == p_height) {
		return;
	}
	height = p_height;
	if (radius > height * 0.5) {
		radius = height * 0.5;
	}
	_update_shape();
}

real_t CapsuleShape2D::get_height() const {
	return height;
}

void CapsuleShape2D::draw(const RID &p_to_rid, const Color &p_color) {
	Vector<Vector2> points = _get_points();
	Vector<Color> col = { p_color };
	RenderingServer::get_singleton()->canvas_item_add_polygon(p_to_rid, points, col);

	if (is_collision_outline_enabled()) {
		points.push_back(points[0]);
		col = { Color(p_color, 1.0) };
		RenderingServer::get_singleton()->canvas_item_add_polyline(p_to_rid, points, col);
	}
}

Rect2 CapsuleShape2D::get_rect() const {
	const Vector2 half_size = Vector2(radius, height * 0.5);
	return Rect2(-half_size, half_size * 2.0);
}

real_t CapsuleShape2D::get_enclosing_radius() const {
	return height * 0.5;
}

void CapsuleShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CapsuleShape2D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CapsuleShape2D::get_radius);

	ClassDB::bind_method(D_METHOD("set_height", "height"), &CapsuleShape2D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CapsuleShape2D::get_height);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:px"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:px"), "set_height", "get_height");
	ADD_LINKED_PROPERTY("radius", "height");
	ADD_LINKED_PROPERTY("height", "radius");
}

CapsuleShape2D::CapsuleShape2D() :
		Shape2D(PhysicsServer2D::get_singleton()->capsule_shape_create()) {
	_update_shape();
}
