/**************************************************************************/
/*  parallax_background.cpp                                               */
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

#include "parallax_background.h"
#include "parallax_layer.h"

void ParallaxBackground::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			group_name = "__cameras_" + itos(get_viewport().get_id());
			add_to_group(group_name);

		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_from_group(group_name);
		} break;
	}
}

void ParallaxBackground::_camera_moved(const Transform2D &p_transform, const Point2 &p_screen_offset) {
	screen_offset = p_screen_offset;

	set_scroll_scale(p_transform.get_scale().dot(Vector2(0.5, 0.5)));
	set_scroll_offset(p_transform.get_origin());
}

void ParallaxBackground::set_scroll_scale(float p_scale) {
	scale = p_scale;
}

float ParallaxBackground::get_scroll_scale() const {
	return scale;
}

void ParallaxBackground::set_scroll_offset(const Point2 &p_ofs) {
	offset = p_ofs;

	_update_scroll();
}

void ParallaxBackground::_update_scroll() {
	if (!is_inside_tree()) {
		return;
	}

	Vector2 ofs = base_offset + offset * base_scale;

	Size2 vps = get_viewport_size();

	ofs = -ofs;
	if (limit_begin.x < limit_end.x) {
		if (ofs.x < limit_begin.x) {
			ofs.x = limit_begin.x;
		} else if (ofs.x + vps.x > limit_end.x) {
			ofs.x = limit_end.x - vps.x;
		}
	}

	if (limit_begin.y < limit_end.y) {
		if (ofs.y < limit_begin.y) {
			ofs.y = limit_begin.y;
		} else if (ofs.y + vps.y > limit_end.y) {
			ofs.y = limit_end.y - vps.y;
		}
	}
	ofs = -ofs;

	final_offset = ofs;

	for (int i = 0; i < get_child_count(); i++) {
		ParallaxLayer *l = Object::cast_to<ParallaxLayer>(get_child(i));
		if (!l) {
			continue;
		}

		if (ignore_camera_zoom) {
			l->set_base_offset_and_scale((ofs + screen_offset * (scale - 1)) / scale, 1.0);
		} else {
			l->set_base_offset_and_scale(ofs, scale);
		}
	}
}

Point2 ParallaxBackground::get_scroll_offset() const {
	return offset;
}

void ParallaxBackground::set_scroll_base_offset(const Point2 &p_ofs) {
	base_offset = p_ofs;
	_update_scroll();
}

Point2 ParallaxBackground::get_scroll_base_offset() const {
	return base_offset;
}

void ParallaxBackground::set_scroll_base_scale(const Point2 &p_ofs) {
	base_scale = p_ofs;
	_update_scroll();
}

Point2 ParallaxBackground::get_scroll_base_scale() const {
	return base_scale;
}

void ParallaxBackground::set_limit_begin(const Point2 &p_ofs) {
	limit_begin = p_ofs;
	_update_scroll();
}

Point2 ParallaxBackground::get_limit_begin() const {
	return limit_begin;
}

void ParallaxBackground::set_limit_end(const Point2 &p_ofs) {
	limit_end = p_ofs;
	_update_scroll();
}

Point2 ParallaxBackground::get_limit_end() const {
	return limit_end;
}

void ParallaxBackground::set_ignore_camera_zoom(bool ignore) {
	ignore_camera_zoom = ignore;
}

bool ParallaxBackground::is_ignore_camera_zoom() {
	return ignore_camera_zoom;
}

Vector2 ParallaxBackground::get_final_offset() const {
	return final_offset;
}

void ParallaxBackground::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_camera_moved"), &ParallaxBackground::_camera_moved);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "ofs"), &ParallaxBackground::set_scroll_offset);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &ParallaxBackground::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_base_offset", "ofs"), &ParallaxBackground::set_scroll_base_offset);
	ClassDB::bind_method(D_METHOD("get_scroll_base_offset"), &ParallaxBackground::get_scroll_base_offset);
	ClassDB::bind_method(D_METHOD("set_scroll_base_scale", "scale"), &ParallaxBackground::set_scroll_base_scale);
	ClassDB::bind_method(D_METHOD("get_scroll_base_scale"), &ParallaxBackground::get_scroll_base_scale);
	ClassDB::bind_method(D_METHOD("set_limit_begin", "ofs"), &ParallaxBackground::set_limit_begin);
	ClassDB::bind_method(D_METHOD("get_limit_begin"), &ParallaxBackground::get_limit_begin);
	ClassDB::bind_method(D_METHOD("set_limit_end", "ofs"), &ParallaxBackground::set_limit_end);
	ClassDB::bind_method(D_METHOD("get_limit_end"), &ParallaxBackground::get_limit_end);
	ClassDB::bind_method(D_METHOD("set_ignore_camera_zoom", "ignore"), &ParallaxBackground::set_ignore_camera_zoom);
	ClassDB::bind_method(D_METHOD("is_ignore_camera_zoom"), &ParallaxBackground::is_ignore_camera_zoom);

	ADD_GROUP("Scroll", "scroll_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_offset"), "set_scroll_offset", "get_scroll_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_base_offset"), "set_scroll_base_offset", "get_scroll_base_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_base_scale", PROPERTY_HINT_LINK), "set_scroll_base_scale", "get_scroll_base_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_limit_begin"), "set_limit_begin", "get_limit_begin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_limit_end"), "set_limit_end", "get_limit_end");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_ignore_camera_zoom"), "set_ignore_camera_zoom", "is_ignore_camera_zoom");
}

ParallaxBackground::ParallaxBackground() {
	scale = 1.0;
	set_layer(-100); //behind all by default

	base_scale = Vector2(1, 1);
	ignore_camera_zoom = false;
}
