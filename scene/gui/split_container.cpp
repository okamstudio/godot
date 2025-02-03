/**************************************************************************/
/*  split_container.cpp                                                   */
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

#include "split_container.h"

#include "label.h"
#include "margin_container.h"

Control *SplitContainer::_getch(int p_idx) const {
	int idx = 0;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c || !c->is_visible()) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}

		if (idx == p_idx) {
			return c;
		}

		idx++;
	}

	return nullptr;
}

void SplitContainer::_resort() {
	int axis = vertical ? 1 : 0;

	Control *first = _getch(0);
	Control *second = _getch(1);

	// If we have only one element
	if (!first || !second) {
		if (first) {
			fit_child_in_rect(first, Rect2(Point2(), get_size()));
		} else if (second) {
			fit_child_in_rect(second, Rect2(Point2(), get_size()));
		}
		return;
	}

	// Determine expanded children
	bool first_expanded = (vertical ? first->get_v_size_flags() : first->get_h_size_flags()) & SIZE_EXPAND;
	bool second_expanded = (vertical ? second->get_v_size_flags() : second->get_h_size_flags()) & SIZE_EXPAND;

	// Determine the separation between items
	Ref<Texture> g = get_icon("grabber");
	int sep = get_constant("separation");
	sep = (dragger_visibility != DRAGGER_HIDDEN_COLLAPSED) ? MAX(sep, vertical ? g->get_height() : g->get_width()) : 0;

	// Compute the minimum size
	Size2 ms_first = first->get_combined_minimum_size();
	Size2 ms_second = second->get_combined_minimum_size();

	// Compute the separator position without the split offset
	float ratio = first->get_stretch_ratio() / (first->get_stretch_ratio() + second->get_stretch_ratio());
	int no_offset_middle_sep = 0;
	if (first_expanded && second_expanded) {
		no_offset_middle_sep = get_size()[axis] * ratio - sep / 2;
	} else if (first_expanded) {
		no_offset_middle_sep = get_size()[axis] - ms_second[axis] - sep;
	} else {
		no_offset_middle_sep = ms_first[axis];
	}

	// Compute the final middle separation
	middle_sep = no_offset_middle_sep;
	if (!collapsed) {
		int clamped_split_offset = CLAMP(split_offset, ms_first[axis] - no_offset_middle_sep, (get_size()[axis] - ms_second[axis] - sep) - no_offset_middle_sep);
		middle_sep += clamped_split_offset;
		if (should_clamp_split_offset) {
			split_offset = clamped_split_offset;
			_change_notify("split_offset");
			should_clamp_split_offset = false;
		}
	}

	if (vertical) {
		fit_child_in_rect(first, Rect2(Point2(0, 0), Size2(get_size().width, middle_sep)));
		int sofs = middle_sep + sep;
		fit_child_in_rect(second, Rect2(Point2(0, sofs), Size2(get_size().width, get_size().height - sofs)));
	} else {
		fit_child_in_rect(first, Rect2(Point2(0, 0), Size2(middle_sep, get_size().height)));
		int sofs = middle_sep + sep;
		fit_child_in_rect(second, Rect2(Point2(sofs, 0), Size2(get_size().width - sofs, get_size().height)));
	}

	update();
}

Size2 SplitContainer::get_minimum_size() const {
	/* Calculate MINIMUM SIZE */

	Size2i minimum;
	Ref<Texture> g = get_icon("grabber");
	int sep = get_constant("separation");
	sep = (dragger_visibility != DRAGGER_HIDDEN_COLLAPSED) ? MAX(sep, vertical ? g->get_height() : g->get_width()) : 0;

	for (int i = 0; i < 2; i++) {
		if (!_getch(i)) {
			break;
		}

		if (i == 1) {
			if (vertical) {
				minimum.height += sep;
			} else {
				minimum.width += sep;
			}
		}

		Size2 ms = _getch(i)->get_combined_minimum_size();

		if (vertical) {
			minimum.height += ms.height;
			minimum.width = MAX(minimum.width, ms.width);
		} else {
			minimum.width += ms.width;
			minimum.height = MAX(minimum.height, ms.height);
		}
	}

	return minimum;
}

void SplitContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;
		case NOTIFICATION_MOUSE_EXIT: {
			mouse_inside = false;
			if (get_constant("autohide")) {
				update();
			}
		} break;
		case NOTIFICATION_DRAW: {
			if (!_getch(0) || !_getch(1)) {
				return;
			}

			if (collapsed || (!dragging && !mouse_inside && get_constant("autohide"))) {
				return;
			}

			if (dragger_visibility != DRAGGER_VISIBLE) {
				return;
			}

			int sep = dragger_visibility != DRAGGER_HIDDEN_COLLAPSED ? get_constant("separation") : 0;
			Ref<Texture> tex = get_icon("grabber");
			Size2 size = get_size();

			if (vertical) {
				draw_texture(tex, Point2i((size.x - tex->get_width()) / 2, middle_sep + (sep - tex->get_height()) / 2));
			} else {
				draw_texture(tex, Point2i(middle_sep + (sep - tex->get_width()) / 2, (size.y - tex->get_height()) / 2));
			}
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			minimum_size_changed();
		} break;
	}
}

void SplitContainer::_gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (collapsed || !_getch(0) || !_getch(1) || dragger_visibility != DRAGGER_VISIBLE) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->get_button_index() == BUTTON_LEFT) {
			if (mb->is_pressed()) {
				int sep = get_constant("separation");

				if (vertical) {
					if (mb->get_position().y > middle_sep && mb->get_position().y < middle_sep + sep) {
						dragging = true;
						drag_from = mb->get_position().y;
						drag_ofs = split_offset;
					}
				} else {
					if (mb->get_position().x > middle_sep && mb->get_position().x < middle_sep + sep) {
						dragging = true;
						drag_from = mb->get_position().x;
						drag_ofs = split_offset;
					}
				}
			} else {
				dragging = false;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		bool mouse_inside_state = false;
		if (vertical) {
			mouse_inside_state = mm->get_position().y > middle_sep && mm->get_position().y < middle_sep + get_constant("separation");
		} else {
			mouse_inside_state = mm->get_position().x > middle_sep && mm->get_position().x < middle_sep + get_constant("separation");
		}

		if (mouse_inside != mouse_inside_state) {
			mouse_inside = mouse_inside_state;
			if (get_constant("autohide")) {
				update();
			}
		}

		if (!dragging) {
			return;
		}

		split_offset = drag_ofs + ((vertical ? mm->get_position().y : mm->get_position().x) - drag_from);
		should_clamp_split_offset = true;
		queue_sort();
		emit_signal("dragged", get_split_offset());
	}
}

Control::CursorShape SplitContainer::get_cursor_shape(const Point2 &p_pos) const {
	if (dragging) {
		return (vertical ? CURSOR_VSPLIT : CURSOR_HSPLIT);
	}

	if (!collapsed && _getch(0) && _getch(1) && dragger_visibility == DRAGGER_VISIBLE) {
		int sep = get_constant("separation");

		if (vertical) {
			if (p_pos.y > middle_sep && p_pos.y < middle_sep + sep) {
				return CURSOR_VSPLIT;
			}
		} else {
			if (p_pos.x > middle_sep && p_pos.x < middle_sep + sep) {
				return CURSOR_HSPLIT;
			}
		}
	}

	return Control::get_cursor_shape(p_pos);
}

void SplitContainer::set_split_offset(int p_offset) {
	if (split_offset == p_offset) {
		return;
	}

	split_offset = p_offset;

	queue_sort();
}

int SplitContainer::get_split_offset() const {
	return split_offset;
}

void SplitContainer::clamp_split_offset() {
	should_clamp_split_offset = true;

	queue_sort();
}

void SplitContainer::set_collapsed(bool p_collapsed) {
	if (collapsed == p_collapsed) {
		return;
	}

	collapsed = p_collapsed;
	queue_sort();
}

void SplitContainer::set_dragger_visibility(DraggerVisibility p_visibility) {
	dragger_visibility = p_visibility;
	queue_sort();
	update();
}

SplitContainer::DraggerVisibility SplitContainer::get_dragger_visibility() const {
	return dragger_visibility;
}

bool SplitContainer::is_collapsed() const {
	return collapsed;
}

/**
 * Hack to make the editor's bottom panel position movable to the right without
 * having to fiddle with the node structure. This method is intentionally not
 * exposed for now.
 */
void SplitContainer::set_vertical(bool p_vertical) {
	vertical = p_vertical;
	queue_sort();
	update();
}

bool SplitContainer::is_vertical() const {
	return vertical;
}

void SplitContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_gui_input"), &SplitContainer::_gui_input);

	ClassDB::bind_method(D_METHOD("set_split_offset", "offset"), &SplitContainer::set_split_offset);
	ClassDB::bind_method(D_METHOD("get_split_offset"), &SplitContainer::get_split_offset);
	ClassDB::bind_method(D_METHOD("clamp_split_offset"), &SplitContainer::clamp_split_offset);

	ClassDB::bind_method(D_METHOD("set_collapsed", "collapsed"), &SplitContainer::set_collapsed);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &SplitContainer::is_collapsed);

	ClassDB::bind_method(D_METHOD("set_dragger_visibility", "mode"), &SplitContainer::set_dragger_visibility);
	ClassDB::bind_method(D_METHOD("get_dragger_visibility"), &SplitContainer::get_dragger_visibility);

	ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(Variant::INT, "offset")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "split_offset"), "set_split_offset", "get_split_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collapsed"), "set_collapsed", "is_collapsed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dragger_visibility", PROPERTY_HINT_ENUM, "Visible,Hidden,Hidden & Collapsed"), "set_dragger_visibility", "get_dragger_visibility");

	BIND_ENUM_CONSTANT(DRAGGER_VISIBLE);
	BIND_ENUM_CONSTANT(DRAGGER_HIDDEN);
	BIND_ENUM_CONSTANT(DRAGGER_HIDDEN_COLLAPSED);
}

SplitContainer::SplitContainer(bool p_vertical) {
	mouse_inside = false;
	split_offset = 0;
	should_clamp_split_offset = false;
	middle_sep = 0;
	vertical = p_vertical;
	dragging = false;
	collapsed = false;
	dragger_visibility = DRAGGER_VISIBLE;
}
