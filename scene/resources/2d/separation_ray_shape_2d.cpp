/**************************************************************************/
/*  separation_ray_shape_2d.cpp                                           */
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

#include "separation_ray_shape_2d.h"

#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

void SeparationRayShape2D::_update_shape() {
	Dictionary d;
	d["length"] = length;
	d["slide_on_slope"] = slide_on_slope;
	PhysicsServer2D::get_singleton()->shape_set_data(get_rid(), d);
	emit_changed();
}

void SeparationRayShape2D::draw(const RID &p_to_rid, const Color &p_color) {
	const Vector2 target_position = Vector2(0, get_length());

	const float max_arrow_size = 6;
	const float line_width = 1.4;
	bool no_line = target_position.length() < line_width;
	float arrow_size = CLAMP(target_position.length() * 2 / 3, line_width, max_arrow_size);

	if (no_line) {
		arrow_size = target_position.length();
	} else {
		RS::get_singleton()->canvas_item_add_line(p_to_rid, Vector2(), target_position - target_position.normalized() * arrow_size, p_color, line_width);
	}

	Transform2D xf;
	xf.rotate(target_position.angle());
	xf.translate_local(Vector2(no_line ? 0 : target_position.length() - arrow_size, 0));

	Vector<Vector2> pts = {
		xf.xform(Vector2(arrow_size, 0)),
		xf.xform(Vector2(0, 0.5 * arrow_size)),
		xf.xform(Vector2(0, -0.5 * arrow_size))
	};

	Vector<Color> cols = { p_color, p_color, p_color };

	RS::get_singleton()->canvas_item_add_primitive(p_to_rid, pts, cols, Vector<Point2>(), RID());
}

Rect2 SeparationRayShape2D::get_rect() const {
	Rect2 rect;
	rect.position = Vector2();
	rect.expand_to(Vector2(0, length));
	rect = rect.grow(Math_SQRT12 * 4);
	return rect;
}

real_t SeparationRayShape2D::get_enclosing_radius() const {
	return length;
}

void SeparationRayShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_length", "length"), &SeparationRayShape2D::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &SeparationRayShape2D::get_length);

	ClassDB::bind_method(D_METHOD("set_slide_on_slope", "active"), &SeparationRayShape2D::set_slide_on_slope);
	ClassDB::bind_method(D_METHOD("get_slide_on_slope"), &SeparationRayShape2D::get_slide_on_slope);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:px"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "slide_on_slope"), "set_slide_on_slope", "get_slide_on_slope");
}

void SeparationRayShape2D::set_length(real_t p_length) {
	if (length == p_length) {
		return;
	}
	length = p_length;
	_update_shape();
}

real_t SeparationRayShape2D::get_length() const {
	return length;
}

void SeparationRayShape2D::set_slide_on_slope(bool p_active) {
	if (slide_on_slope == p_active) {
		return;
	}
	slide_on_slope = p_active;
	_update_shape();
}

bool SeparationRayShape2D::get_slide_on_slope() const {
	return slide_on_slope;
}

SeparationRayShape2D::SeparationRayShape2D() :
		Shape2D(PhysicsServer2D::get_singleton()->separation_ray_shape_create()) {
	_update_shape();
}
