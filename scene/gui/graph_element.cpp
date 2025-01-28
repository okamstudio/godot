/**************************************************************************/
/*  graph_element.cpp                                                     */
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

#include "graph_element.h"

#include "scene/gui/graph_edit.h"
#include "scene/theme/theme_db.h"

#ifdef TOOLS_ENABLED
void GraphElement::_edit_set_position(const Point2 &p_position) {
	GraphEdit *graph = Object::cast_to<GraphEdit>(get_parent());
	if (graph) {
		Point2 offset = (p_position + graph->get_scroll_offset()) * graph->get_zoom();
		set_position_offset(offset);
	}
	set_position(p_position);
}
#endif

void GraphElement::_resort() {
	Size2 size = get_size();

	for (int i = 0; i < get_child_count(); i++) {
		Control *child = as_sortable_control(get_child(i));
		if (!child) {
			continue;
		}
		fit_child_in_rect(child, Rect2(Point2(), size));
	}
}

Size2 GraphElement::get_minimum_size() const {
	Size2 minsize;
	for (int i = 0; i < get_child_count(); i++) {
		Control *child = as_sortable_control(get_child(i), SortableVisibilityMode::IGNORE);
		if (!child) {
			continue;
		}

		Size2i size = child->get_combined_minimum_size();

		minsize = minsize.max(size);
	}

	return minsize;
}

void GraphElement::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
			queue_redraw();
		} break;
	}
}

void GraphElement::_validate_property(PropertyInfo &p_property) const {
	Control::_validate_property(p_property);
	GraphEdit *graph = Object::cast_to<GraphEdit>(get_parent());
	if (graph) {
		if (p_property.name == "position") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}

void GraphElement::set_position_offset(const Vector2 &p_offset) {
	if (position_offset == p_offset) {
		return;
	}

	position_offset = p_offset;
	emit_signal(SNAME("position_offset_changed"));
	queue_redraw();
}

Vector2 GraphElement::get_position_offset() const {
	return position_offset;
}

void GraphElement::set_selected(bool p_selected) {
	if (!is_selectable() || selected == p_selected) {
		return;
	}
	selected = p_selected;
	emit_signal(p_selected ? SNAME("node_selected") : SNAME("node_deselected"));
	queue_redraw();
}

bool GraphElement::is_selected() {
	return selected;
}

void GraphElement::set_drag(bool p_drag) {
	if (p_drag) {
		drag_from = get_position_offset();
	} else {
		emit_signal(SNAME("dragged"), drag_from, get_position_offset()); // Required for undo/redo.
	}
}

Vector2 GraphElement::get_drag_from() {
	return drag_from;
}

void GraphElement::gui_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid()) {
		ERR_FAIL_NULL_MSG(get_parent_control(), "GraphElement must be the child of a GraphEdit node.");

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			Vector2 mpos = mb->get_position();

			if (resizable && mpos.x > get_size().x - theme_cache.resizer->get_width() && mpos.y > get_size().y - theme_cache.resizer->get_height()) {
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
	if (resizing && mm.is_valid()) {
		Vector2 mpos = mm->get_position();
		Vector2 diff = mpos - resizing_from;

		emit_signal(SNAME("resize_request"), resizing_from_size + diff);
	}
}

void GraphElement::set_resizable(bool p_enable) {
	if (resizable == p_enable) {
		return;
	}
	resizable = p_enable;
	queue_redraw();
}

bool GraphElement::is_resizable() const {
	return resizable;
}

void GraphElement::set_draggable(bool p_draggable) {
	draggable = p_draggable;
}

bool GraphElement::is_draggable() {
	return draggable;
}

void GraphElement::set_selectable(bool p_selectable) {
	if (!p_selectable) {
		set_selected(false);
	}
	selectable = p_selectable;
}

bool GraphElement::is_selectable() {
	return selectable;
}

void GraphElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_resizable", "resizable"), &GraphElement::set_resizable);
	ClassDB::bind_method(D_METHOD("is_resizable"), &GraphElement::is_resizable);

	ClassDB::bind_method(D_METHOD("set_draggable", "draggable"), &GraphElement::set_draggable);
	ClassDB::bind_method(D_METHOD("is_draggable"), &GraphElement::is_draggable);

	ClassDB::bind_method(D_METHOD("set_selectable", "selectable"), &GraphElement::set_selectable);
	ClassDB::bind_method(D_METHOD("is_selectable"), &GraphElement::is_selectable);

	ClassDB::bind_method(D_METHOD("set_selected", "selected"), &GraphElement::set_selected);
	ClassDB::bind_method(D_METHOD("is_selected"), &GraphElement::is_selected);

	ClassDB::bind_method(D_METHOD("set_position_offset", "offset"), &GraphElement::set_position_offset);
	ClassDB::bind_method(D_METHOD("get_position_offset"), &GraphElement::get_position_offset);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position_offset"), "set_position_offset", "get_position_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "resizable"), "set_resizable", "is_resizable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draggable"), "set_draggable", "is_draggable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selectable"), "set_selectable", "is_selectable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selected"), "set_selected", "is_selected");

	ADD_SIGNAL(MethodInfo("node_selected"));
	ADD_SIGNAL(MethodInfo("node_deselected"));

	ADD_SIGNAL(MethodInfo("raise_request"));
	ADD_SIGNAL(MethodInfo("delete_request"));
	ADD_SIGNAL(MethodInfo("resize_request", PropertyInfo(Variant::VECTOR2, "new_size")));
	ADD_SIGNAL(MethodInfo("resize_end", PropertyInfo(Variant::VECTOR2, "new_size")));

	ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(Variant::VECTOR2, "from"), PropertyInfo(Variant::VECTOR2, "to")));
	ADD_SIGNAL(MethodInfo("position_offset_changed"));

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, GraphElement, resizer);
}
