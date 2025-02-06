/**************************************************************************/
/*  layout_container.cpp                                                  */
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

#include "layout_container.h"

#include "scene/theme/theme_db.h"

void LayoutContainer::_resort() {
	Size2 size = get_size();

	// Get the relative_margin values.
	const real_t *relative_margins;
	if (preset == PRESET_CUSTOM) {
		relative_margins = custom_relative_margins;
	} else {
		relative_margins = relative_margins_presets[preset];
	}

	// The "default" rectangle, independent from the child size.
	Rect2 rect;
	rect.set_position(Vector2(size.x * relative_margins[SIDE_LEFT] + margins[SIDE_LEFT] + theme_cache.margin_left, size.y * relative_margins[SIDE_TOP] + margins[SIDE_TOP] + theme_cache.margin_top));
	rect.set_end(Vector2(size.x * (1.0 - relative_margins[SIDE_RIGHT]) - margins[SIDE_RIGHT] - theme_cache.margin_right, size.y * (1.0 - relative_margins[SIDE_BOTTOM]) - margins[SIDE_BOTTOM] - theme_cache.margin_bottom));

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisbilityMode::VISIBLE);
		if (!c) {
			continue;
		}

		// In case there's not enough room we shift the child according to the relative_margins.
		Rect2 child_rect(rect);
		Vector2 child_size = c->get_combined_minimum_size();
		if (child_rect.size.x < child_size.x) {
			child_rect.position.x -= (child_size.x - child_rect.size.x) * (relative_margins[SIDE_LEFT] + 1.0 - relative_margins[SIDE_RIGHT]) / 2.0;
		}
		if (child_rect.size.y < child_size.y) {
			child_rect.position.y -= (child_size.y - child_rect.size.y) * (relative_margins[SIDE_TOP] + 1.0 - relative_margins[SIDE_BOTTOM]) / 2.0;
		}
		child_rect.size = child_rect.size.max(child_rect.size);
		fit_child_in_rect(c, child_rect);
	}
}

void LayoutContainer::_set_custom_relative_margin(Side p_side, real_t p_relative_margin) {
	set_custom_relative_margin(p_side, p_relative_margin);
}

void LayoutContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_margin_layout_preset", "preset"), &LayoutContainer::set_margin_layout_preset);
	ClassDB::bind_method(D_METHOD("get_margin_layout_preset"), &LayoutContainer::get_margin_layout_preset);

	ClassDB::bind_method(D_METHOD("_set_custom_relative_margin", "side", "relative_margin"), &LayoutContainer::_set_custom_relative_margin);
	ClassDB::bind_method(D_METHOD("set_custom_relative_margin", "side", "relative_margin", "push_opposite_margin"), &LayoutContainer::set_custom_relative_margin, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_custom_relative_margin", "side"), &LayoutContainer::get_custom_relative_margin);

	ClassDB::bind_method(D_METHOD("set_margin", "side", "offset"), &LayoutContainer::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin", "side"), &LayoutContainer::get_margin);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "preset",
						 PROPERTY_HINT_ENUM, "Top Left,Top Right,Bottom left,Bottom Right,Center left,Center Top,Center Right,Center Bottom,Center,Left Wide,Top Wide,Right Wide,Bottom Wide,Vertical Center Wide,Horizontal Center Wide,Full Rect,Custom", PROPERTY_USAGE_DEFAULT),
			"set_margin_layout_preset", "get_margin_layout_preset");

	ADD_GROUP("Custom relative margins", "custom_relative_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "custom_relative_margin_left", PROPERTY_HINT_RANGE, "0,1,0.001"), "_set_custom_relative_margin", "get_custom_relative_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "custom_relative_margin_top", PROPERTY_HINT_RANGE, "0,1,0.001"), "_set_custom_relative_margin", "get_custom_relative_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "custom_relative_margin_right", PROPERTY_HINT_RANGE, "0,1,0.001"), "_set_custom_relative_margin", "get_custom_relative_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "custom_relative_margin_bottom", PROPERTY_HINT_RANGE, "0,1,0.001"), "_set_custom_relative_margin", "get_custom_relative_margin", SIDE_BOTTOM);

	ADD_GROUP("Margins", "margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "margin_left", PROPERTY_HINT_RANGE, "-4096,4096,suffix:px"), "set_margin", "get_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "margin_top", PROPERTY_HINT_RANGE, "-4096,4096,suffix:px"), "set_margin", "get_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "margin_right", PROPERTY_HINT_RANGE, "-4096,4096,suffix:px"), "set_margin", "get_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "margin_bottom", PROPERTY_HINT_RANGE, "-4096,4096,suffix:px"), "set_margin", "get_margin", SIDE_BOTTOM);

	BIND_ENUM_CONSTANT(PRESET_TOP_LEFT);
	BIND_ENUM_CONSTANT(PRESET_TOP_RIGHT);
	BIND_ENUM_CONSTANT(PRESET_BOTTOM_LEFT);
	BIND_ENUM_CONSTANT(PRESET_BOTTOM_RIGHT);
	BIND_ENUM_CONSTANT(PRESET_CENTER_LEFT);
	BIND_ENUM_CONSTANT(PRESET_CENTER_TOP);
	BIND_ENUM_CONSTANT(PRESET_CENTER_RIGHT);
	BIND_ENUM_CONSTANT(PRESET_CENTER_BOTTOM);
	BIND_ENUM_CONSTANT(PRESET_CENTER);
	BIND_ENUM_CONSTANT(PRESET_LEFT_WIDE);
	BIND_ENUM_CONSTANT(PRESET_TOP_WIDE);
	BIND_ENUM_CONSTANT(PRESET_RIGHT_WIDE);
	BIND_ENUM_CONSTANT(PRESET_BOTTOM_WIDE);
	BIND_ENUM_CONSTANT(PRESET_VCENTER_WIDE);
	BIND_ENUM_CONSTANT(PRESET_HCENTER_WIDE);
	BIND_ENUM_CONSTANT(PRESET_FULL_RECT);
	BIND_ENUM_CONSTANT(PRESET_CUSTOM);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LayoutContainer, margin_left);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LayoutContainer, margin_top);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LayoutContainer, margin_right);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LayoutContainer, margin_bottom);
}

Size2 LayoutContainer::get_minimum_size() const {
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms = ms.max(minsize);
	}
	ms += Vector2(margins[SIDE_LEFT] + margins[SIDE_RIGHT], margins[SIDE_TOP] + margins[SIDE_BOTTOM]);
	ms += Vector2(theme_cache.margin_left + theme_cache.margin_right, theme_cache.margin_top + theme_cache.margin_bottom);
	ms = ms.maxf(0.0);
	return ms;
}

void LayoutContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_sort();
		} break;
	}
}

void LayoutContainer::_validate_property(PropertyInfo &p_property) const {
	if (preset != LayoutContainerPreset::PRESET_CUSTOM && p_property.name.begins_with("custom_relative_margin_")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

Vector<int> LayoutContainer::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	return flags;
}

Vector<int> LayoutContainer::get_allowed_size_flags_vertical() const {
	return {};
}

void LayoutContainer::set_margin_layout_preset(LayoutContainer::LayoutContainerPreset p_preset) {
	if (p_preset == preset) {
		return;
	}

	if (preset == LayoutContainerPreset::PRESET_CUSTOM || p_preset == LayoutContainerPreset::PRESET_CUSTOM) {
		// We switch to or from custom preset, so we update the property list to hide/show relative margins.
		preset = p_preset;
		notify_property_list_changed();
	} else {
		preset = p_preset;
	}

	update_minimum_size();
	queue_sort();
}

LayoutContainer::LayoutContainerPreset LayoutContainer::get_margin_layout_preset() const {
	return preset;
}

void LayoutContainer::set_custom_relative_margin(Side p_side, real_t p_relative_margin, bool p_push_opposite_margin) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX((int)p_side, 4);

	custom_relative_margins[p_side] = p_relative_margin;

	if (custom_relative_margins[p_side] > (1.0 - custom_relative_margins[(p_side + 2) % 4])) {
		if (p_push_opposite_margin) {
			custom_relative_margins[(p_side + 2) % 4] = 1.0 - custom_relative_margins[p_side];
		} else {
			custom_relative_margins[p_side] = 1.0 - custom_relative_margins[(p_side + 2) % 4];
		}
	}

	queue_sort();
}

real_t LayoutContainer::get_custom_relative_margin(Side p_side) const {
	ERR_READ_THREAD_GUARD_V(0);
	ERR_FAIL_INDEX_V(int(p_side), 4, 0.0);
	return custom_relative_margins[p_side];
}

void LayoutContainer::set_margin(Side p_side, real_t p_value) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX((int)p_side, 4);
	if (margins[p_side] == p_value) {
		return;
	}

	margins[p_side] = p_value;
	update_minimum_size();
	queue_sort();
}

real_t LayoutContainer::get_margin(Side p_side) const {
	ERR_READ_THREAD_GUARD_V(0);
	ERR_FAIL_INDEX_V((int)p_side, 4, 0);

	return margins[p_side];
}

void LayoutContainer::set_custom_relative_margin_and_offset(Side p_side, real_t p_relative_margin, real_t p_pos, bool p_push_opposite_margin) {
	ERR_MAIN_THREAD_GUARD;
	set_custom_relative_margin(p_side, p_relative_margin, p_push_opposite_margin);
	set_margin(p_side, p_pos);
}
