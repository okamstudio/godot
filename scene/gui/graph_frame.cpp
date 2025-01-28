/**************************************************************************/
/*  graph_frame.cpp                                                       */
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

#include "graph_frame.h"

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_texture.h"
#include "scene/theme/theme_db.h"

void GraphFrame::gui_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid()) {
		ERR_FAIL_NULL_MSG(get_parent_control(), "GraphFrame must be the child of a GraphEdit node.");

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			Vector2 mpos = mb->get_position();

			Ref<Texture2D> resizer = theme_cache.resizer;

			if (resizable && mpos.x > get_size().x - resizer->get_width() && mpos.y > get_size().y - resizer->get_height()) {
				resizing = true;
				resizing_from = mpos;
				resizing_from_size = get_size();
				accept_event();
				return;
			}

			emit_signal(SNAME("raise_request"));
		}

		if (!mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (resizing) {
				resizing = false;
				emit_signal(SNAME("resize_end"), get_size());
				return;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_ev;

	// Only resize if the frame is not auto-resizing based on linked nodes.
	if (resizing && !autoshrink_enabled && mm.is_valid()) {
		Vector2 mpos = mm->get_position();

		Vector2 diff = mpos - resizing_from;

		emit_signal(SNAME("resize_request"), resizing_from_size + diff);
	}
}

Control::CursorShape GraphFrame::get_cursor_shape(const Point2 &p_pos) const {
	if (resizable && !autoshrink_enabled) {
		if (resizing || (p_pos.x > get_size().x - theme_cache.resizer->get_width() && p_pos.y > get_size().y - theme_cache.resizer->get_height())) {
			return CURSOR_FDIAGSIZE;
		}
	}

	return Control::get_cursor_shape(p_pos);
}

void GraphFrame::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			// Used for layout calculations.
			Ref<StyleBox> sb_panel = theme_cache.panel;
			Ref<StyleBox> sb_titlebar = theme_cache.titlebar;

			// Used for drawing.
			Ref<StyleBox> sb_to_draw_panel = selected ? theme_cache.panel_selected : sb_panel;
			Ref<StyleBox> sb_to_draw_titlebar = selected ? theme_cache.titlebar_selected : sb_titlebar;
			Ref<StyleBoxFlat> sb_panel_flat = sb_to_draw_panel;
			Ref<StyleBoxTexture> sb_panel_texture = sb_to_draw_panel;

			Rect2 titlebar_rect(Point2(), titlebar_hbox->get_size() + sb_titlebar->get_minimum_size());
			Size2 body_size = get_size();
			body_size.y -= titlebar_rect.size.height;
			Rect2 body_rect(Point2(0, titlebar_rect.size.height), body_size);

			// Draw body stylebox.
			if (tint_color_enabled) {
				if (sb_panel_flat.is_valid()) {
					Color original_border_color = sb_panel_flat->get_border_color();
					sb_panel_flat = sb_panel_flat->duplicate();
					sb_panel_flat->set_bg_color(tint_color);
					sb_panel_flat->set_border_color(selected ? original_border_color : tint_color.lightened(0.3));
					draw_style_box(sb_panel_flat, body_rect);
				} else if (sb_panel_texture.is_valid()) {
					sb_panel_texture = sb_panel_texture->duplicate();
					sb_panel_texture->set_modulate(tint_color);
					draw_style_box(sb_panel_texture, body_rect);
				}
			} else {
				draw_style_box(sb_panel_flat, body_rect);
			}

			// Draw title bar stylebox above.
			draw_style_box(sb_to_draw_titlebar, titlebar_rect);

			// Only draw the resize handle if the frame is not auto-resizing.
			if (resizable && !autoshrink_enabled) {
				Ref<Texture2D> resizer = theme_cache.resizer;
				Color resizer_color = theme_cache.resizer_color;
				if (resizable) {
					draw_texture(resizer, get_size() - resizer->get_size(), resizer_color);
				}
			}

		} break;
	}
}

void GraphFrame::_resort() {
	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;

	// Resort titlebar first.
	Size2 titlebar_size = Size2(get_size().width, titlebar_hbox->get_size().height);
	titlebar_size -= sb_titlebar->get_minimum_size();
	Rect2 titlebar_rect = Rect2(sb_titlebar->get_offset(), titlebar_size);

	fit_child_in_rect(titlebar_hbox, titlebar_rect);

	// After resort the children of the titlebar container may have changed their height (e.g. Label autowrap).
	Size2i titlebar_min_size = titlebar_hbox->get_combined_minimum_size();

	Size2 size = get_size() - sb_panel->get_minimum_size() - Size2(0, titlebar_min_size.height + sb_titlebar->get_minimum_size().height);
	Point2 offset = Point2(sb_panel->get_margin(SIDE_LEFT), sb_panel->get_margin(SIDE_TOP) + titlebar_min_size.height + sb_titlebar->get_minimum_size().height);

	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false));
		if (!child) {
			continue;
		}
		fit_child_in_rect(child, Rect2(offset, size));
	}
}

void GraphFrame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &GraphFrame::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &GraphFrame::get_title);

	ClassDB::bind_method(D_METHOD("get_titlebar_hbox"), &GraphFrame::get_titlebar_hbox);

	ClassDB::bind_method(D_METHOD("set_autoshrink_enabled", "shrink"), &GraphFrame::set_autoshrink_enabled);
	ClassDB::bind_method(D_METHOD("is_autoshrink_enabled"), &GraphFrame::is_autoshrink_enabled);

	ClassDB::bind_method(D_METHOD("set_autoshrink_margin", "autoshrink_margin"), &GraphFrame::set_autoshrink_margin);
	ClassDB::bind_method(D_METHOD("get_autoshrink_margin"), &GraphFrame::get_autoshrink_margin);

	ClassDB::bind_method(D_METHOD("set_drag_margin", "drag_margin"), &GraphFrame::set_drag_margin);
	ClassDB::bind_method(D_METHOD("get_drag_margin"), &GraphFrame::get_drag_margin);

	ClassDB::bind_method(D_METHOD("set_tint_color_enabled", "enable"), &GraphFrame::set_tint_color_enabled);
	ClassDB::bind_method(D_METHOD("is_tint_color_enabled"), &GraphFrame::is_tint_color_enabled);

	ClassDB::bind_method(D_METHOD("set_tint_color", "color"), &GraphFrame::set_tint_color);
	ClassDB::bind_method(D_METHOD("get_tint_color"), &GraphFrame::get_tint_color);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoshrink_enabled"), "set_autoshrink_enabled", "is_autoshrink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autoshrink_margin", PROPERTY_HINT_RANGE, "0,128,1"), "set_autoshrink_margin", "get_autoshrink_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drag_margin", PROPERTY_HINT_RANGE, "0,128,1"), "set_drag_margin", "get_drag_margin");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tint_color_enabled"), "set_tint_color_enabled", "is_tint_color_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_color"), "set_tint_color", "get_tint_color");

	ADD_SIGNAL(MethodInfo(SNAME("autoshrink_changed")));

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphFrame, panel);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphFrame, panel_selected);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphFrame, titlebar);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphFrame, titlebar_selected);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, GraphFrame, resizer);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, GraphFrame, resizer_color);
}

void GraphFrame::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "resizable") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void GraphFrame::set_title(const String &p_title) {
	if (title == p_title) {
		return;
	}
	title = p_title;
	if (title_label) {
		title_label->set_text(title);
	}
	update_minimum_size();
}

String GraphFrame::get_title() const {
	return title;
}

void GraphFrame::set_autoshrink_enabled(bool p_shrink) {
	if (autoshrink_enabled == p_shrink) {
		return;
	}

	autoshrink_enabled = p_shrink;

	emit_signal("autoshrink_changed", get_size());
	queue_redraw();
}

bool GraphFrame::is_autoshrink_enabled() const {
	return autoshrink_enabled;
}

void GraphFrame::set_autoshrink_margin(const int &p_margin) {
	if (autoshrink_margin == p_margin) {
		return;
	}

	autoshrink_margin = p_margin;

	emit_signal("autoshrink_changed", get_size());
}

int GraphFrame::get_autoshrink_margin() const {
	return autoshrink_margin;
}

HBoxContainer *GraphFrame::get_titlebar_hbox() {
	return titlebar_hbox;
}

Size2 GraphFrame::get_titlebar_size() const {
	return titlebar_hbox->get_size() + theme_cache.titlebar->get_minimum_size();
}

void GraphFrame::set_drag_margin(int p_margin) {
	drag_margin = p_margin;
}

int GraphFrame::get_drag_margin() const {
	return drag_margin;
}

void GraphFrame::set_tint_color_enabled(bool p_enable) {
	tint_color_enabled = p_enable;
	queue_redraw();
}

bool GraphFrame::is_tint_color_enabled() const {
	return tint_color_enabled;
}

void GraphFrame::set_tint_color(const Color &p_color) {
	tint_color = p_color;
	queue_redraw();
}

Color GraphFrame::get_tint_color() const {
	return tint_color;
}

bool GraphFrame::has_point(const Point2 &p_point) const {
	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;
	Ref<Texture2D> resizer = theme_cache.resizer;

	if (Rect2(get_size() - resizer->get_size(), resizer->get_size()).has_point(p_point)) {
		return true;
	}

	// For grabbing on the titlebar.
	int titlebar_height = titlebar_hbox->get_size().height + sb_titlebar->get_minimum_size().height;
	if (Rect2(0, 0, get_size().width, titlebar_height).has_point(p_point)) {
		return true;
	}

	// Allow grabbing on all sides of the frame.
	Rect2 frame_rect = Rect2(0, 0, get_size().width, get_size().height);
	Rect2 no_drag_rect = frame_rect.grow(-drag_margin);

	if (frame_rect.has_point(p_point) && !no_drag_rect.has_point(p_point)) {
		return true;
	}

	return false;
}

Size2 GraphFrame::get_minimum_size() const {
	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;

	Size2 minsize = titlebar_hbox->get_minimum_size() + sb_titlebar->get_minimum_size();

	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false));
		if (!child) {
			continue;
		}

		Size2i size = child->get_combined_minimum_size();
		size.width += sb_panel->get_minimum_size().width;

		minsize.x = MAX(minsize.x, size.x);
		minsize.y += MAX(minsize.y, size.y);
	}

	minsize.height += sb_panel->get_minimum_size().height;

	return minsize;
}

GraphFrame::GraphFrame() {
	titlebar_hbox = memnew(HBoxContainer);
	titlebar_hbox->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(titlebar_hbox, false, INTERNAL_MODE_FRONT);

	title_label = memnew(Label);
	title_label->set_theme_type_variation("GraphFrameTitleLabel");
	title_label->set_h_size_flags(SIZE_EXPAND_FILL);
	title_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	titlebar_hbox->add_child(title_label);

	set_mouse_filter(MOUSE_FILTER_STOP);
}
