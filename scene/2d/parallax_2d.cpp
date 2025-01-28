/**************************************************************************/
/*  parallax_2d.cpp                                                       */
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

#include "parallax_2d.h"

#include "scene/main/viewport.h"

void Parallax2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			group_name = "__cameras_" + itos(get_viewport_rid().get_id());
			add_to_group(group_name);
			_update_repeat();
			_update_scroll();
		} break;

		case NOTIFICATION_READY: {
			_update_process();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			Point2 offset = scroll_offset;
			offset += autoscroll * get_process_delta_time();

			if (repeat_size.x) {
				offset.x = Math::fposmod(offset.x, repeat_size.x);
			}

			if (repeat_size.y) {
				offset.y = Math::fposmod(offset.y, repeat_size.y);
			}

			scroll_offset = offset;
			_update_scroll();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_from_group(group_name);
		} break;
	}
}

#ifdef TOOLS_ENABLED
void Parallax2D::_edit_set_position(const Point2 &p_position) {
	set_scroll_offset(p_position);
}
#endif // TOOLS_ENABLED

void Parallax2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "position") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void Parallax2D::_camera_moved(const Transform2D &p_transform, const Point2 &p_screen_offset, const Point2 &p_adj_screen_pos) {
	if (!ignore_camera_scroll) {
		if (get_viewport() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
			Size2 vps = get_viewport_rect().size;
			Vector2 offset;
			offset.x = ((int)vps.width % 2) ? 0.0 : 0.5;
			offset.y = ((int)vps.height % 2) ? 0.0 : 0.5;
			set_screen_offset((p_adj_screen_pos + offset).floor());
		} else {
			set_screen_offset(p_adj_screen_pos);
		}
	}
}

void Parallax2D::_update_process() {
	set_process_internal(!Engine::get_singleton()->is_editor_hint() && (repeat_size.x || repeat_size.y) && (autoscroll.x || autoscroll.y));
}

void Parallax2D::_update_scroll() {
	if (!is_inside_tree()) {
		return;
	}

	Point2 scroll_ofs = screen_offset;

	if (!Engine::get_singleton()->is_editor_hint()) {
		Size2 vps = get_viewport_rect().size;

		if (limit_begin.x <= limit_end.x - vps.x) {
			scroll_ofs.x = CLAMP(scroll_ofs.x, limit_begin.x, limit_end.x - vps.x);
		}
		if (limit_begin.y <= limit_end.y - vps.y) {
			scroll_ofs.y = CLAMP(scroll_ofs.y, limit_begin.y, limit_end.y - vps.y);
		}
	}

	scroll_ofs *= scroll_scale;

	if (repeat_size.x) {
		real_t mod = Math::fposmod(scroll_ofs.x - scroll_offset.x, repeat_size.x * get_scale().x);
		scroll_ofs.x = screen_offset.x - mod;
	} else {
		scroll_ofs.x = screen_offset.x + scroll_offset.x - scroll_ofs.x;
	}

	if (repeat_size.y) {
		real_t mod = Math::fposmod(scroll_ofs.y - scroll_offset.y, repeat_size.y * get_scale().y);
		scroll_ofs.y = screen_offset.y - mod;
	} else {
		scroll_ofs.y = screen_offset.y + scroll_offset.y - scroll_ofs.y;
	}

	if (!follow_viewport) {
		scroll_ofs -= screen_offset;
	}

	set_position(scroll_ofs);
}

void Parallax2D::_update_repeat() {
	if (!is_inside_tree()) {
		return;
	}

	RenderingServer::get_singleton()->canvas_set_item_repeat(get_canvas_item(), repeat_size, repeat_times);
	RenderingServer::get_singleton()->canvas_item_set_interpolated(get_canvas_item(), false);
}

void Parallax2D::set_scroll_scale(const Size2 &p_scale) {
	scroll_scale = p_scale;
}

Size2 Parallax2D::get_scroll_scale() const {
	return scroll_scale;
}

void Parallax2D::set_repeat_size(const Size2 &p_repeat_size) {
	if (p_repeat_size == repeat_size) {
		return;
	}

	repeat_size = p_repeat_size.maxf(0);

	_update_process();
	_update_repeat();
	_update_scroll();
}

Size2 Parallax2D::get_repeat_size() const {
	return repeat_size;
}

void Parallax2D::set_repeat_times(int p_repeat_times) {
	if (p_repeat_times == repeat_times) {
		return;
	}

	repeat_times = MAX(p_repeat_times, 1);

	_update_repeat();
}

int Parallax2D::get_repeat_times() const {
	return repeat_times;
}

void Parallax2D::set_scroll_offset(const Point2 &p_offset) {
	if (p_offset == scroll_offset) {
		return;
	}

	scroll_offset = p_offset;

	_update_scroll();
}

Point2 Parallax2D::get_scroll_offset() const {
	return scroll_offset;
}

void Parallax2D::set_autoscroll(const Point2 &p_autoscroll) {
	if (p_autoscroll == autoscroll) {
		return;
	}

	autoscroll = p_autoscroll;

	_update_process();
	_update_scroll();
}

Point2 Parallax2D::get_autoscroll() const {
	return autoscroll;
}

void Parallax2D::set_screen_offset(const Point2 &p_offset) {
	if (p_offset == screen_offset) {
		return;
	}

	screen_offset = p_offset;

	_update_scroll();
}

Point2 Parallax2D::get_screen_offset() const {
	return screen_offset;
}

void Parallax2D::set_limit_begin(const Point2 &p_offset) {
	limit_begin = p_offset;
}

Point2 Parallax2D::get_limit_begin() const {
	return limit_begin;
}

void Parallax2D::set_limit_end(const Point2 &p_offset) {
	limit_end = p_offset;
}

Point2 Parallax2D::get_limit_end() const {
	return limit_end;
}

void Parallax2D::set_follow_viewport(bool p_follow) {
	follow_viewport = p_follow;
}

bool Parallax2D::get_follow_viewport() {
	return follow_viewport;
}

void Parallax2D::set_ignore_camera_scroll(bool p_ignore) {
	ignore_camera_scroll = p_ignore;
}

bool Parallax2D::is_ignore_camera_scroll() {
	return ignore_camera_scroll;
}

void Parallax2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_camera_moved", "transform", "screen_offset", "adj_screen_offset"), &Parallax2D::_camera_moved);
	ClassDB::bind_method(D_METHOD("set_scroll_scale", "scale"), &Parallax2D::set_scroll_scale);
	ClassDB::bind_method(D_METHOD("get_scroll_scale"), &Parallax2D::get_scroll_scale);
	ClassDB::bind_method(D_METHOD("set_repeat_size", "repeat_size"), &Parallax2D::set_repeat_size);
	ClassDB::bind_method(D_METHOD("get_repeat_size"), &Parallax2D::get_repeat_size);
	ClassDB::bind_method(D_METHOD("set_repeat_times", "repeat_times"), &Parallax2D::set_repeat_times);
	ClassDB::bind_method(D_METHOD("get_repeat_times"), &Parallax2D::get_repeat_times);
	ClassDB::bind_method(D_METHOD("set_autoscroll", "autoscroll"), &Parallax2D::set_autoscroll);
	ClassDB::bind_method(D_METHOD("get_autoscroll"), &Parallax2D::get_autoscroll);
	ClassDB::bind_method(D_METHOD("set_scroll_offset", "offset"), &Parallax2D::set_scroll_offset);
	ClassDB::bind_method(D_METHOD("get_scroll_offset"), &Parallax2D::get_scroll_offset);
	ClassDB::bind_method(D_METHOD("set_screen_offset", "offset"), &Parallax2D::set_screen_offset);
	ClassDB::bind_method(D_METHOD("get_screen_offset"), &Parallax2D::get_screen_offset);
	ClassDB::bind_method(D_METHOD("set_limit_begin", "offset"), &Parallax2D::set_limit_begin);
	ClassDB::bind_method(D_METHOD("get_limit_begin"), &Parallax2D::get_limit_begin);
	ClassDB::bind_method(D_METHOD("set_limit_end", "offset"), &Parallax2D::set_limit_end);
	ClassDB::bind_method(D_METHOD("get_limit_end"), &Parallax2D::get_limit_end);
	ClassDB::bind_method(D_METHOD("set_follow_viewport", "follow"), &Parallax2D::set_follow_viewport);
	ClassDB::bind_method(D_METHOD("get_follow_viewport"), &Parallax2D::get_follow_viewport);
	ClassDB::bind_method(D_METHOD("set_ignore_camera_scroll", "ignore"), &Parallax2D::set_ignore_camera_scroll);
	ClassDB::bind_method(D_METHOD("is_ignore_camera_scroll"), &Parallax2D::is_ignore_camera_scroll);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_scale", PROPERTY_HINT_LINK), "set_scroll_scale", "get_scroll_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scroll_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_scroll_offset", "get_scroll_offset");

	ADD_GROUP("Repeat", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "repeat_size"), "set_repeat_size", "get_repeat_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "autoscroll", PROPERTY_HINT_NONE, "suffix:px/s"), "set_autoscroll", "get_autoscroll");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "repeat_times"), "set_repeat_times", "get_repeat_times");

	ADD_GROUP("Limit", "limit_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "limit_begin", PROPERTY_HINT_NONE, "suffix:px"), "set_limit_begin", "get_limit_begin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "limit_end", PROPERTY_HINT_NONE, "suffix:px"), "set_limit_end", "get_limit_end");

	ADD_GROUP("Override", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "follow_viewport"), "set_follow_viewport", "get_follow_viewport");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_camera_scroll"), "set_ignore_camera_scroll", "is_ignore_camera_scroll");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "screen_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_screen_offset", "get_screen_offset");
}

Parallax2D::Parallax2D() {
	// Parallax2D is always updated every frame so there is no need to interpolate.
	set_physics_interpolation_mode(Node::PHYSICS_INTERPOLATION_MODE_OFF);
}
