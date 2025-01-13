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

#include "scene/main/viewport.h"
#include "scene/theme/theme_db.h"

void SplitContainerDragger::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	SplitContainer *sc = Object::cast_to<SplitContainer>(get_parent());

	if (sc->collapsed || !sc->_get_sortable_child(0) || !sc->_get_sortable_child(1) || !sc->dragging_enabled) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				sc->_compute_split_offset(true);
				dragging = true;
				sc->emit_signal(SNAME("drag_started"));
				drag_ofs = sc->split_offset;
				if (sc->vertical) {
					drag_from = get_transform().xform(mb->get_position()).y;
				} else {
					drag_from = get_transform().xform(mb->get_position()).x;
				}
			} else {
				dragging = false;
				queue_redraw();
				sc->emit_signal(SNAME("drag_ended"));
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (!dragging) {
			return;
		}

		Vector2i in_parent_pos = get_transform().xform(mm->get_position());
		if (!sc->vertical && is_layout_rtl()) {
			sc->split_offset = drag_ofs - (in_parent_pos.x - drag_from);
		} else {
			sc->split_offset = drag_ofs + ((sc->vertical ? in_parent_pos.y : in_parent_pos.x) - drag_from);
		}
		sc->_compute_split_offset(true);
		sc->queue_sort();
		sc->emit_signal(SNAME("dragged"), sc->get_split_offset());
	}
}

Control::CursorShape SplitContainerDragger::get_cursor_shape(const Point2 &p_pos) const {
	SplitContainer *sc = Object::cast_to<SplitContainer>(get_parent());
	if (!sc->collapsed && sc->dragging_enabled) {
		return (sc->vertical ? CURSOR_VSPLIT : CURSOR_HSPLIT);
	}
	return Control::get_cursor_shape(p_pos);
}

void SplitContainerDragger::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_ENTER: {
			mouse_inside = true;
			SplitContainer *sc = Object::cast_to<SplitContainer>(get_parent());
			if (sc->theme_cache.autohide) {
				queue_redraw();
			}
		} break;
		case NOTIFICATION_MOUSE_EXIT: {
			mouse_inside = false;
			SplitContainer *sc = Object::cast_to<SplitContainer>(get_parent());
			if (sc->theme_cache.autohide) {
				queue_redraw();
			}
		} break;
		case NOTIFICATION_DRAW: {
			SplitContainer *sc = Object::cast_to<SplitContainer>(get_parent());
			draw_style_box(sc->theme_cache.split_bar_background, split_bar_rect);
			if (sc->dragger_visibility == sc->DRAGGER_VISIBLE && (dragging || mouse_inside || !sc->theme_cache.autohide)) {
				Ref<Texture2D> tex = sc->_get_grabber_icon();
				float available_size = sc->vertical ? (sc->get_size().x - tex->get_size().x) : (sc->get_size().y - tex->get_size().y);
				if (available_size - sc->drag_area_margin_begin - sc->drag_area_margin_end > 0) { // Draw the grabber only if it fits.
					draw_texture(tex, (split_bar_rect.get_position() + (split_bar_rect.get_size() - tex->get_size()) * 0.5));
				}
			}
			if (sc->show_drag_area && Engine::get_singleton()->is_editor_hint()) {
				draw_rect(Rect2(Vector2(0, 0), get_size()), sc->dragging_enabled ? Color(1, 1, 0, 0.3) : Color(1, 0, 0, 0.3));
			}
		} break;
	}
}

Control *SplitContainer::_get_sortable_child(int p_idx, SortableVisbilityMode p_visibility_mode) const {
	int idx = 0;
	for (int i = 0; i < get_child_count(false); i++) {
		Control *c = as_sortable_control(get_child(i, false), p_visibility_mode);
		if (!c) {
			continue;
		}

		if (idx == p_idx) {
			return c;
		}

		idx++;
	}
	return nullptr;
}

Ref<Texture2D> SplitContainer::_get_grabber_icon() const {
	if (is_fixed) {
		return theme_cache.grabber_icon;
	} else {
		if (vertical) {
			return theme_cache.grabber_icon_v;
		} else {
			return theme_cache.grabber_icon_h;
		}
	}
}

int SplitContainer::_get_separation() const {
	if (dragger_visibility == DRAGGER_HIDDEN_COLLAPSED) {
		return 0;
	}
	// DRAGGER_VISIBLE or DRAGGER_HIDDEN.
	Ref<Texture2D> g = _get_grabber_icon();
	return MAX(theme_cache.separation, vertical ? g->get_height() : g->get_width());
}

void SplitContainer::_compute_split_offset(bool p_clamp) {
	Control *first = _get_sortable_child(0);
	Control *second = _get_sortable_child(1);
	int axis_index = vertical ? 1 : 0;
	int size = get_size()[axis_index];
	int sep = _get_separation();

	// Compute the wished size.
	int wished_size = 0;
	int split_offset_with_collapse = 0;
	if (!collapsed) {
		split_offset_with_collapse = split_offset;
	}
	bool first_is_expanded = (vertical ? first->get_v_size_flags() : first->get_h_size_flags()) & SIZE_EXPAND;
	bool second_is_expanded = (vertical ? second->get_v_size_flags() : second->get_h_size_flags()) & SIZE_EXPAND;

	if (first_is_expanded && second_is_expanded) {
		float ratio = first->get_stretch_ratio() / (first->get_stretch_ratio() + second->get_stretch_ratio());
		wished_size = size * ratio - sep * 0.5 + split_offset_with_collapse;
	} else if (first_is_expanded) {
		wished_size = size - sep + split_offset_with_collapse;
	} else {
		wished_size = split_offset_with_collapse;
	}

	// Clamp the split offset to acceptable values.
	int first_min_size = first->get_combined_minimum_size()[axis_index];
	int second_min_size = second->get_combined_minimum_size()[axis_index];
	computed_split_offset = CLAMP(wished_size, first_min_size, size - sep - second_min_size);

	// Clamp the split_offset if requested.
	if (p_clamp) {
		split_offset -= wished_size - computed_split_offset;
	}
}

void SplitContainer::_resort() {
	Control *first = _get_sortable_child(0);
	Control *second = _get_sortable_child(1);

	if (!first || !second) { // Only one child.
		if (first) {
			fit_child_in_rect(first, Rect2(Point2(), get_size()));
		} else if (second) {
			fit_child_in_rect(second, Rect2(Point2(), get_size()));
		}
		dragging_area_control->hide();
		return;
	}
	dragging_area_control->set_visible(!collapsed);

	_compute_split_offset(false); // This recalculates and sets computed_split_offset.

	int sep = _get_separation();
	bool is_rtl = is_layout_rtl();

	// Move the children.
	if (vertical) {
		fit_child_in_rect(first, Rect2(Point2(0, 0), Size2(get_size().width, computed_split_offset)));
		int sofs = computed_split_offset + sep;
		fit_child_in_rect(second, Rect2(Point2(0, sofs), Size2(get_size().width, get_size().height - sofs)));
	} else {
		if (is_rtl) {
			computed_split_offset = get_size().width - computed_split_offset - sep;
			fit_child_in_rect(second, Rect2(Point2(0, 0), Size2(computed_split_offset, get_size().height)));
			int sofs = computed_split_offset + sep;
			fit_child_in_rect(first, Rect2(Point2(sofs, 0), Size2(get_size().width - sofs, get_size().height)));
		} else {
			fit_child_in_rect(first, Rect2(Point2(0, 0), Size2(computed_split_offset, get_size().height)));
			int sofs = computed_split_offset + sep;
			fit_child_in_rect(second, Rect2(Point2(sofs, 0), Size2(get_size().width - sofs, get_size().height)));
		}
	}

	dragging_area_control->set_mouse_filter(dragging_enabled ? MOUSE_FILTER_STOP : MOUSE_FILTER_IGNORE);
	const int dragger_ctrl_size = MAX(sep, theme_cache.minimum_grab_thickness);
	float split_bar_offset = (dragger_ctrl_size - sep) * 0.5;
	if (vertical) {
		Rect2 split_bar_rect = Rect2(is_rtl ? drag_area_margin_end : drag_area_margin_begin, computed_split_offset, get_size().width - drag_area_margin_begin - drag_area_margin_end, sep);
		dragging_area_control->set_rect(Rect2(split_bar_rect.position.x, split_bar_rect.position.y - split_bar_offset + drag_area_offset, split_bar_rect.size.x, dragger_ctrl_size));
		dragging_area_control->split_bar_rect = Rect2(Vector2(0.0, int(split_bar_offset) - drag_area_offset), split_bar_rect.size);
	} else {
		Rect2 split_bar_rect = Rect2(computed_split_offset, drag_area_margin_begin, sep, get_size().height - drag_area_margin_begin - drag_area_margin_end);
		dragging_area_control->set_rect(Rect2(split_bar_rect.position.x - split_bar_offset + drag_area_offset * (is_rtl ? -1 : 1), split_bar_rect.position.y, dragger_ctrl_size, split_bar_rect.size.y));
		dragging_area_control->split_bar_rect = Rect2(Vector2(int(split_bar_offset) - drag_area_offset * (is_rtl ? -1 : 1), 0.0), split_bar_rect.size);
	}
	queue_redraw();
	dragging_area_control->queue_redraw();
}

Size2 SplitContainer::get_minimum_size() const {
	Size2i minimum;
	int sep = _get_separation();

	for (int i = 0; i < 2; i++) {
		Control *child = _get_sortable_child(i, SortableVisbilityMode::VISIBLE);
		if (!child) {
			break;
		}

		if (i == 1) {
			if (vertical) {
				minimum.height += sep;
			} else {
				minimum.width += sep;
			}
		}

		Size2 ms = child->get_combined_minimum_size();

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

void SplitContainer::_validate_property(PropertyInfo &p_property) const {
	if (is_fixed && p_property.name == "vertical") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void SplitContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_sort();
		} break;
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
		} break;
	}
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
	if (!_get_sortable_child(0) || !_get_sortable_child(1)) {
		return;
	}
	_compute_split_offset(true);
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
	if (dragger_visibility == p_visibility) {
		return;
	}
	dragger_visibility = p_visibility;
	queue_sort();
}

SplitContainer::DraggerVisibility SplitContainer::get_dragger_visibility() const {
	return dragger_visibility;
}

bool SplitContainer::is_collapsed() const {
	return collapsed;
}

void SplitContainer::set_vertical(bool p_vertical) {
	ERR_FAIL_COND_MSG(is_fixed, "Can't change orientation of " + get_class() + ".");
	vertical = p_vertical;
	update_minimum_size();
	_resort();
}

bool SplitContainer::is_vertical() const {
	return vertical;
}

void SplitContainer::set_dragging_enabled(bool p_enabled) {
	if (dragging_enabled == p_enabled) {
		return;
	}
	dragging_enabled = p_enabled;
	if (!dragging_enabled && dragging_area_control->dragging) {
		dragging_area_control->dragging = false;
		// queue_redraw() is called by _resort().
		emit_signal(SNAME("drag_ended"));
	}
	if (get_viewport()) {
		get_viewport()->update_mouse_cursor_state();
	}
	_resort();
}

bool SplitContainer::is_dragging_enabled() const {
	return dragging_enabled;
}

Vector<int> SplitContainer::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	if (!vertical) {
		flags.append(SIZE_EXPAND);
	}
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

Vector<int> SplitContainer::get_allowed_size_flags_vertical() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	if (vertical) {
		flags.append(SIZE_EXPAND);
	}
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

void SplitContainer::set_drag_area_margin_begin(int p_margin) {
	if (drag_area_margin_begin == p_margin) {
		return;
	}
	drag_area_margin_begin = p_margin;
	queue_sort();
}

int SplitContainer::get_drag_area_margin_begin() const {
	return drag_area_margin_begin;
}

void SplitContainer::set_drag_area_margin_end(int p_margin) {
	if (drag_area_margin_end == p_margin) {
		return;
	}
	drag_area_margin_end = p_margin;
	queue_sort();
}

int SplitContainer::get_drag_area_margin_end() const {
	return drag_area_margin_end;
}

void SplitContainer::set_drag_area_offset(int p_offset) {
	if (drag_area_offset == p_offset) {
		return;
	}
	drag_area_offset = p_offset;
	queue_sort();
}

int SplitContainer::get_drag_area_offset() const {
	return drag_area_offset;
}

void SplitContainer::set_show_drag_area_enabled(bool p_enabled) {
	show_drag_area = p_enabled;
	dragging_area_control->queue_redraw();
}

bool SplitContainer::is_show_drag_area_enabled() const {
	return show_drag_area;
}

void SplitContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_split_offset", "offset"), &SplitContainer::set_split_offset);
	ClassDB::bind_method(D_METHOD("get_split_offset"), &SplitContainer::get_split_offset);
	ClassDB::bind_method(D_METHOD("clamp_split_offset"), &SplitContainer::clamp_split_offset);

	ClassDB::bind_method(D_METHOD("set_collapsed", "collapsed"), &SplitContainer::set_collapsed);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &SplitContainer::is_collapsed);

	ClassDB::bind_method(D_METHOD("set_dragger_visibility", "mode"), &SplitContainer::set_dragger_visibility);
	ClassDB::bind_method(D_METHOD("get_dragger_visibility"), &SplitContainer::get_dragger_visibility);

	ClassDB::bind_method(D_METHOD("set_vertical", "vertical"), &SplitContainer::set_vertical);
	ClassDB::bind_method(D_METHOD("is_vertical"), &SplitContainer::is_vertical);

	ClassDB::bind_method(D_METHOD("set_dragging_enabled", "dragging_enabled"), &SplitContainer::set_dragging_enabled);
	ClassDB::bind_method(D_METHOD("is_dragging_enabled"), &SplitContainer::is_dragging_enabled);

	ClassDB::bind_method(D_METHOD("set_drag_area_margin_begin", "margin"), &SplitContainer::set_drag_area_margin_begin);
	ClassDB::bind_method(D_METHOD("get_drag_area_margin_begin"), &SplitContainer::get_drag_area_margin_begin);

	ClassDB::bind_method(D_METHOD("set_drag_area_margin_end", "margin"), &SplitContainer::set_drag_area_margin_end);
	ClassDB::bind_method(D_METHOD("get_drag_area_margin_end"), &SplitContainer::get_drag_area_margin_end);

	ClassDB::bind_method(D_METHOD("set_drag_area_offset", "offset"), &SplitContainer::set_drag_area_offset);
	ClassDB::bind_method(D_METHOD("get_drag_area_offset"), &SplitContainer::get_drag_area_offset);

	ClassDB::bind_method(D_METHOD("set_drag_area_highlight_in_editor", "drag_area_highlight_in_editor"), &SplitContainer::set_show_drag_area_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_area_highlight_in_editor_enabled"), &SplitContainer::is_show_drag_area_enabled);

	ClassDB::bind_method(D_METHOD("get_drag_area_control"), &SplitContainer::get_drag_area_control);

	ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(Variant::INT, "offset")));
	ADD_SIGNAL(MethodInfo("drag_started"));
	ADD_SIGNAL(MethodInfo("drag_ended"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "split_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_split_offset", "get_split_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collapsed"), "set_collapsed", "is_collapsed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dragging_enabled"), "set_dragging_enabled", "is_dragging_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dragger_visibility", PROPERTY_HINT_ENUM, "Visible,Hidden,Hidden and Collapsed"), "set_dragger_visibility", "get_dragger_visibility");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vertical"), "set_vertical", "is_vertical");

	ADD_GROUP("Drag Area", "drag_area_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drag_area_margin_begin", PROPERTY_HINT_NONE, "suffix:px"), "set_drag_area_margin_begin", "get_drag_area_margin_begin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drag_area_margin_end", PROPERTY_HINT_NONE, "suffix:px"), "set_drag_area_margin_end", "get_drag_area_margin_end");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drag_area_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_drag_area_offset", "get_drag_area_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_area_highlight_in_editor"), "set_drag_area_highlight_in_editor", "is_drag_area_highlight_in_editor_enabled");

	BIND_ENUM_CONSTANT(DRAGGER_VISIBLE);
	BIND_ENUM_CONSTANT(DRAGGER_HIDDEN);
	BIND_ENUM_CONSTANT(DRAGGER_HIDDEN_COLLAPSED);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SplitContainer, separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SplitContainer, minimum_grab_thickness);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, SplitContainer, autohide);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SplitContainer, grabber_icon, "grabber");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SplitContainer, grabber_icon_h, "h_grabber");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, SplitContainer, grabber_icon_v, "v_grabber");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, SplitContainer, split_bar_background, "split_bar_background");
}

SplitContainer::SplitContainer(bool p_vertical) {
	vertical = p_vertical;

	dragging_area_control = memnew(SplitContainerDragger);
	add_child(dragging_area_control, false, Node::INTERNAL_MODE_BACK);
}
