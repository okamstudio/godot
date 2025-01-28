/**************************************************************************/
/*  graph_node.cpp                                                        */
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

#include "graph_node.h"

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/theme/theme_db.h"

bool GraphNode::_set(const StringName &p_name, const Variant &p_value) {
	String str = p_name;

	if (!str.begins_with("slot/")) {
		return false;
	}

	int idx = str.get_slice("/", 1).to_int();
	String slot_property_name = str.get_slice("/", 2);

	Slot slot;
	if (slot_table.has(idx)) {
		slot = slot_table[idx];
	}

	if (slot_property_name == "left_enabled") {
		slot.enable_left = p_value;
	} else if (slot_property_name == "left_type") {
		slot.type_left = p_value;
	} else if (slot_property_name == "left_icon") {
		slot.custom_port_icon_left = p_value;
	} else if (slot_property_name == "left_color") {
		slot.color_left = p_value;
	} else if (slot_property_name == "right_enabled") {
		slot.enable_right = p_value;
	} else if (slot_property_name == "right_type") {
		slot.type_right = p_value;
	} else if (slot_property_name == "right_color") {
		slot.color_right = p_value;
	} else if (slot_property_name == "right_icon") {
		slot.custom_port_icon_right = p_value;
	} else if (slot_property_name == "draw_stylebox") {
		slot.draw_stylebox = p_value;
	} else {
		return false;
	}

	set_slot(idx,
			slot.enable_left,
			slot.type_left,
			slot.color_left,
			slot.enable_right,
			slot.type_right,
			slot.color_right,
			slot.custom_port_icon_left,
			slot.custom_port_icon_right,
			slot.draw_stylebox);

	queue_redraw();
	return true;
}

bool GraphNode::_get(const StringName &p_name, Variant &r_ret) const {
	String str = p_name;

	if (!str.begins_with("slot/")) {
		return false;
	}

	int idx = str.get_slice("/", 1).to_int();
	StringName slot_property_name = str.get_slice("/", 2);

	Slot slot;
	if (slot_table.has(idx)) {
		slot = slot_table[idx];
	}

	if (slot_property_name == "left_enabled") {
		r_ret = slot.enable_left;
	} else if (slot_property_name == "left_type") {
		r_ret = slot.type_left;
	} else if (slot_property_name == "left_color") {
		r_ret = slot.color_left;
	} else if (slot_property_name == "left_icon") {
		r_ret = slot.custom_port_icon_left;
	} else if (slot_property_name == "right_enabled") {
		r_ret = slot.enable_right;
	} else if (slot_property_name == "right_type") {
		r_ret = slot.type_right;
	} else if (slot_property_name == "right_color") {
		r_ret = slot.color_right;
	} else if (slot_property_name == "right_icon") {
		r_ret = slot.custom_port_icon_right;
	} else if (slot_property_name == "draw_stylebox") {
		r_ret = slot.draw_stylebox;
	} else {
		return false;
	}

	return true;
}

void GraphNode::_get_property_list(List<PropertyInfo> *p_list) const {
	int idx = 0;
	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false), SortableVisibilityMode::IGNORE);
		if (!child) {
			continue;
		}

		String base = "slot/" + itos(idx) + "/";

		p_list->push_back(PropertyInfo(Variant::BOOL, base + "left_enabled"));
		p_list->push_back(PropertyInfo(Variant::INT, base + "left_type"));
		p_list->push_back(PropertyInfo(Variant::COLOR, base + "left_color"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, base + "left_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
		p_list->push_back(PropertyInfo(Variant::BOOL, base + "right_enabled"));
		p_list->push_back(PropertyInfo(Variant::INT, base + "right_type"));
		p_list->push_back(PropertyInfo(Variant::COLOR, base + "right_color"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, base + "right_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORE_IF_NULL));
		p_list->push_back(PropertyInfo(Variant::BOOL, base + "draw_stylebox"));
		idx++;
	}
}

void GraphNode::_resort() {
	Size2 new_size = get_size();
	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;

	// Resort titlebar first.
	Size2 titlebar_size = Size2(new_size.width, titlebar_hbox->get_size().height);
	titlebar_size -= sb_titlebar->get_minimum_size();
	Rect2 titlebar_rect = Rect2(sb_titlebar->get_offset(), titlebar_size);
	fit_child_in_rect(titlebar_hbox, titlebar_rect);

	// After resort, the children of the titlebar container may have changed their height (e.g. Label autowrap).
	Size2i titlebar_min_size = titlebar_hbox->get_combined_minimum_size();

	// First pass, determine minimum size AND amount of stretchable elements.
	Ref<StyleBox> sb_slot = theme_cache.slot;
	int separation = theme_cache.separation;

	int children_count = 0;
	int stretch_min = 0;
	int available_stretch_space = 0;
	float stretch_ratio_total = 0;
	HashMap<Control *, _MinSizeCache> min_size_cache;

	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false));
		if (!child) {
			continue;
		}

		Size2i size = child->get_combined_minimum_size() + (slot_table[i].draw_stylebox ? sb_slot->get_minimum_size() : Size2());

		stretch_min += size.height;

		_MinSizeCache msc;
		msc.min_size = size.height;
		msc.will_stretch = child->get_v_size_flags().has_flag(SIZE_EXPAND);
		msc.final_size = msc.min_size;
		min_size_cache[child] = msc;

		if (msc.will_stretch) {
			available_stretch_space += msc.min_size;
			stretch_ratio_total += child->get_stretch_ratio();
		}

		children_count++;
	}

	if (children_count == 0) {
		return;
	}

	int stretch_max = new_size.height - (children_count - 1) * separation;
	int stretch_diff = stretch_max - stretch_min;

	// Avoid negative stretch space.
	stretch_diff = MAX(stretch_diff, 0);

	available_stretch_space += stretch_diff - sb_panel->get_margin(SIDE_BOTTOM) - sb_panel->get_margin(SIDE_TOP) - titlebar_min_size.height - sb_titlebar->get_minimum_size().height;

	// Second pass, discard elements that can't be stretched, this will run while stretchable elements exist.

	while (stretch_ratio_total > 0) {
		// First of all, don't even be here if no stretchable objects exist.
		bool refit_successful = true;

		for (int i = 0; i < get_child_count(false); i++) {
			Control *child = as_sortable_control(get_child(i, false));
			if (!child) {
				continue;
			}

			ERR_FAIL_COND(!min_size_cache.has(child));
			_MinSizeCache &msc = min_size_cache[child];

			if (msc.will_stretch) {
				int final_pixel_size = available_stretch_space * child->get_stretch_ratio() / stretch_ratio_total;
				if (final_pixel_size < msc.min_size) {
					// If the available stretching area is too small for a Control,
					// then remove it from stretching area.
					msc.will_stretch = false;
					stretch_ratio_total -= child->get_stretch_ratio();
					refit_successful = false;
					available_stretch_space -= msc.min_size;
					msc.final_size = msc.min_size;
					break;
				} else {
					msc.final_size = final_pixel_size;
				}
			}
		}

		if (refit_successful) {
			break;
		}
	}

	// Final pass, draw and stretch elements.

	int ofs_y = sb_panel->get_margin(SIDE_TOP) + titlebar_min_size.height + sb_titlebar->get_minimum_size().height;

	slot_y_cache.clear();
	int width = new_size.width - sb_panel->get_minimum_size().width;
	int valid_children_idx = 0;
	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false));
		if (!child) {
			continue;
		}

		_MinSizeCache &msc = min_size_cache[child];

		if (valid_children_idx > 0) {
			ofs_y += separation;
		}

		int from_y_pos = ofs_y;
		int to_y_pos = ofs_y + msc.final_size;

		// Adjust so the last valid child always fits perfect, compensating for numerical imprecision.
		if (msc.will_stretch && valid_children_idx == children_count - 1) {
			to_y_pos = new_size.height - sb_panel->get_margin(SIDE_BOTTOM);
		}

		int height = to_y_pos - from_y_pos;
		float margin = sb_panel->get_margin(SIDE_LEFT) + (slot_table[i].draw_stylebox ? sb_slot->get_margin(SIDE_LEFT) : 0);
		float final_width = width - (slot_table[i].draw_stylebox ? sb_slot->get_minimum_size().x : 0);
		Rect2 rect(margin, from_y_pos, final_width, height);
		fit_child_in_rect(child, rect);

		slot_y_cache.push_back(from_y_pos - sb_panel->get_margin(SIDE_TOP) + height * 0.5);

		ofs_y = to_y_pos;
		valid_children_idx++;
	}

	queue_redraw();
	port_pos_dirty = true;
}

void GraphNode::draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) {
	if (GDVIRTUAL_CALL(_draw_port, p_slot_index, p_pos, p_left, p_color)) {
		return;
	}

	Slot slot = slot_table[p_slot_index];
	Ref<Texture2D> port_icon = p_left ? slot.custom_port_icon_left : slot.custom_port_icon_right;

	Point2 icon_offset;
	if (port_icon.is_null()) {
		port_icon = theme_cache.port;
	}

	icon_offset = -port_icon->get_size() * 0.5;
	port_icon->draw(get_canvas_item(), p_pos + icon_offset, p_color);
}

void GraphNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			// Used for layout calculations.
			Ref<StyleBox> sb_panel = theme_cache.panel;
			Ref<StyleBox> sb_titlebar = theme_cache.titlebar;
			// Used for drawing.
			Ref<StyleBox> sb_to_draw_panel = selected ? theme_cache.panel_selected : theme_cache.panel;
			Ref<StyleBox> sb_to_draw_titlebar = selected ? theme_cache.titlebar_selected : theme_cache.titlebar;

			Ref<StyleBox> sb_slot = theme_cache.slot;

			int port_h_offset = theme_cache.port_h_offset;

			Rect2 titlebar_rect(Point2(), titlebar_hbox->get_size() + sb_titlebar->get_minimum_size());
			Size2 body_size = get_size();
			titlebar_rect.size.width = body_size.width;
			body_size.height -= titlebar_rect.size.height;
			Rect2 body_rect(0, titlebar_rect.size.height, body_size.width, body_size.height);

			// Draw body (slots area) stylebox.
			draw_style_box(sb_to_draw_panel, body_rect);

			// Draw title bar stylebox above.
			draw_style_box(sb_to_draw_titlebar, titlebar_rect);

			int width = get_size().width - sb_panel->get_minimum_size().x;

			// Take the HboxContainer child into account.
			if (get_child_count(false) > 0) {
				int slot_index = 0;
				for (const KeyValue<int, Slot> &E : slot_table) {
					if (E.key < 0 || E.key >= slot_y_cache.size()) {
						continue;
					}
					if (!slot_table.has(E.key)) {
						continue;
					}
					const Slot &slot = slot_table[E.key];

					// Left port.
					if (slot.enable_left) {
						draw_port(slot_index, Point2i(port_h_offset, slot_y_cache[E.key] + sb_panel->get_margin(SIDE_TOP)), true, slot.color_left);
					}

					// Right port.
					if (slot.enable_right) {
						draw_port(slot_index, Point2i(get_size().x - port_h_offset, slot_y_cache[E.key] + sb_panel->get_margin(SIDE_TOP)), false, slot.color_right);
					}

					// Draw slot stylebox.
					if (slot.draw_stylebox) {
						Control *child = Object::cast_to<Control>(get_child(E.key, false));
						if (!child || !child->is_visible_in_tree()) {
							continue;
						}
						Rect2 child_rect = child->get_rect();
						child_rect.position.x = sb_panel->get_margin(SIDE_LEFT);
						child_rect.size.width = width;
						draw_style_box(sb_slot, child_rect);
					}

					slot_index++;
				}
			}

			if (resizable) {
				draw_texture(theme_cache.resizer, get_size() - theme_cache.resizer->get_size(), theme_cache.resizer_color);
			}
		} break;
	}
}

void GraphNode::set_slot(int p_slot_index, bool p_enable_left, int p_type_left, const Color &p_color_left, bool p_enable_right, int p_type_right, const Color &p_color_right, const Ref<Texture2D> &p_custom_left, const Ref<Texture2D> &p_custom_right, bool p_draw_stylebox) {
	ERR_FAIL_COND_MSG(p_slot_index < 0, vformat("Cannot set slot with index (%d) lesser than zero.", p_slot_index));

	if (!p_enable_left && p_type_left == 0 && p_color_left == Color(1, 1, 1, 1) &&
			!p_enable_right && p_type_right == 0 && p_color_right == Color(1, 1, 1, 1) &&
			p_custom_left.is_null() && p_custom_right.is_null()) {
		slot_table.erase(p_slot_index);
		return;
	}

	Slot slot;
	slot.enable_left = p_enable_left;
	slot.type_left = p_type_left;
	slot.color_left = p_color_left;
	slot.enable_right = p_enable_right;
	slot.type_right = p_type_right;
	slot.color_right = p_color_right;
	slot.custom_port_icon_left = p_custom_left;
	slot.custom_port_icon_right = p_custom_right;
	slot.draw_stylebox = p_draw_stylebox;
	slot_table[p_slot_index] = slot;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

void GraphNode::clear_slot(int p_slot_index) {
	slot_table.erase(p_slot_index);
	queue_redraw();
	port_pos_dirty = true;
}

void GraphNode::clear_all_slots() {
	slot_table.clear();
	queue_redraw();
	port_pos_dirty = true;
}

bool GraphNode::is_slot_enabled_left(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return false;
	}
	return slot_table[p_slot_index].enable_left;
}

void GraphNode::set_slot_enabled_left(int p_slot_index, bool p_enable) {
	ERR_FAIL_COND_MSG(p_slot_index < 0, vformat("Cannot set enable_left for the slot with index (%d) lesser than zero.", p_slot_index));

	if (slot_table[p_slot_index].enable_left == p_enable) {
		return;
	}

	slot_table[p_slot_index].enable_left = p_enable;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

void GraphNode::set_slot_type_left(int p_slot_index, int p_type) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set type_left for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].type_left == p_type) {
		return;
	}

	slot_table[p_slot_index].type_left = p_type;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

int GraphNode::get_slot_type_left(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return 0;
	}
	return slot_table[p_slot_index].type_left;
}

void GraphNode::set_slot_color_left(int p_slot_index, const Color &p_color) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set color_left for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].color_left == p_color) {
		return;
	}

	slot_table[p_slot_index].color_left = p_color;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

Color GraphNode::get_slot_color_left(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return Color(1, 1, 1, 1);
	}
	return slot_table[p_slot_index].color_left;
}

void GraphNode::set_slot_custom_icon_left(int p_slot_index, const Ref<Texture2D> &p_custom_icon) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set custom_port_icon_left for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].custom_port_icon_left == p_custom_icon) {
		return;
	}

	slot_table[p_slot_index].custom_port_icon_left = p_custom_icon;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

Ref<Texture2D> GraphNode::get_slot_custom_icon_left(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return Ref<Texture2D>();
	}
	return slot_table[p_slot_index].custom_port_icon_left;
}

bool GraphNode::is_slot_enabled_right(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return false;
	}
	return slot_table[p_slot_index].enable_right;
}

void GraphNode::set_slot_enabled_right(int p_slot_index, bool p_enable) {
	ERR_FAIL_COND_MSG(p_slot_index < 0, vformat("Cannot set enable_right for the slot with index (%d) lesser than zero.", p_slot_index));

	if (slot_table[p_slot_index].enable_right == p_enable) {
		return;
	}

	slot_table[p_slot_index].enable_right = p_enable;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

void GraphNode::set_slot_type_right(int p_slot_index, int p_type) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set type_right for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].type_right == p_type) {
		return;
	}

	slot_table[p_slot_index].type_right = p_type;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

int GraphNode::get_slot_type_right(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return 0;
	}
	return slot_table[p_slot_index].type_right;
}

void GraphNode::set_slot_color_right(int p_slot_index, const Color &p_color) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set color_right for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].color_right == p_color) {
		return;
	}

	slot_table[p_slot_index].color_right = p_color;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

Color GraphNode::get_slot_color_right(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return Color(1, 1, 1, 1);
	}
	return slot_table[p_slot_index].color_right;
}

void GraphNode::set_slot_custom_icon_right(int p_slot_index, const Ref<Texture2D> &p_custom_icon) {
	ERR_FAIL_COND_MSG(!slot_table.has(p_slot_index), vformat("Cannot set custom_port_icon_right for the slot with index '%d' because it hasn't been enabled.", p_slot_index));

	if (slot_table[p_slot_index].custom_port_icon_right == p_custom_icon) {
		return;
	}

	slot_table[p_slot_index].custom_port_icon_right = p_custom_icon;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

Ref<Texture2D> GraphNode::get_slot_custom_icon_right(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return Ref<Texture2D>();
	}
	return slot_table[p_slot_index].custom_port_icon_right;
}

bool GraphNode::is_slot_draw_stylebox(int p_slot_index) const {
	if (!slot_table.has(p_slot_index)) {
		return false;
	}
	return slot_table[p_slot_index].draw_stylebox;
}

void GraphNode::set_slot_draw_stylebox(int p_slot_index, bool p_enable) {
	ERR_FAIL_COND_MSG(p_slot_index < 0, vformat("Cannot set draw_stylebox for the slot with p_index (%d) lesser than zero.", p_slot_index));

	slot_table[p_slot_index].draw_stylebox = p_enable;
	queue_redraw();
	port_pos_dirty = true;

	emit_signal(SNAME("slot_updated"), p_slot_index);
}

void GraphNode::set_ignore_invalid_connection_type(bool p_ignore) {
	ignore_invalid_connection_type = p_ignore;
}

bool GraphNode::is_ignoring_valid_connection_type() const {
	return ignore_invalid_connection_type;
}

Size2 GraphNode::get_minimum_size() const {
	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;
	Ref<StyleBox> sb_slot = theme_cache.slot;

	int separation = theme_cache.separation;
	Size2 minsize = titlebar_hbox->get_minimum_size() + sb_titlebar->get_minimum_size();

	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false));
		if (!child) {
			continue;
		}

		Size2i size = child->get_combined_minimum_size();
		size.width += sb_panel->get_minimum_size().width;
		if (slot_table.has(i)) {
			size += slot_table[i].draw_stylebox ? sb_slot->get_minimum_size() : Size2();
		}

		minsize.height += size.height;
		minsize.width = MAX(minsize.width, size.width);

		if (i > 0) {
			minsize.height += separation;
		}
	}

	minsize.height += sb_panel->get_minimum_size().height;

	return minsize;
}

void GraphNode::_port_pos_update() {
	int edgeofs = theme_cache.port_h_offset;
	int separation = theme_cache.separation;

	Ref<StyleBox> sb_panel = theme_cache.panel;
	Ref<StyleBox> sb_titlebar = theme_cache.titlebar;

	left_port_cache.clear();
	right_port_cache.clear();
	int vertical_ofs = titlebar_hbox->get_size().height + sb_titlebar->get_minimum_size().height + sb_panel->get_margin(SIDE_TOP);
	int slot_index = 0;

	for (int i = 0; i < get_child_count(false); i++) {
		Control *child = as_sortable_control(get_child(i, false), SortableVisibilityMode::IGNORE);
		if (!child) {
			continue;
		}

		Size2i size = child->get_rect().size;

		if (slot_table.has(slot_index)) {
			if (slot_table[slot_index].enable_left) {
				PortCache port_cache;
				port_cache.pos = Point2i(edgeofs, vertical_ofs + size.height / 2);
				port_cache.type = slot_table[slot_index].type_left;
				port_cache.color = slot_table[slot_index].color_left;
				port_cache.slot_index = slot_index;
				left_port_cache.push_back(port_cache);
			}
			if (slot_table[slot_index].enable_right) {
				PortCache port_cache;
				port_cache.pos = Point2i(get_size().width - edgeofs, vertical_ofs + size.height / 2);
				port_cache.type = slot_table[slot_index].type_right;
				port_cache.color = slot_table[slot_index].color_right;
				port_cache.slot_index = slot_index;
				right_port_cache.push_back(port_cache);
			}
		}

		vertical_ofs += separation;
		vertical_ofs += size.height;
		slot_index++;
	}

	port_pos_dirty = false;
}

int GraphNode::get_input_port_count() {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	return left_port_cache.size();
}

int GraphNode::get_output_port_count() {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	return right_port_cache.size();
}

Vector2 GraphNode::get_input_port_position(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, left_port_cache.size(), Vector2());
	Vector2 pos = left_port_cache[p_port_idx].pos;
	return pos;
}

int GraphNode::get_input_port_type(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, left_port_cache.size(), 0);
	return left_port_cache[p_port_idx].type;
}

Color GraphNode::get_input_port_color(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, left_port_cache.size(), Color());
	return left_port_cache[p_port_idx].color;
}

int GraphNode::get_input_port_slot(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, left_port_cache.size(), -1);
	return left_port_cache[p_port_idx].slot_index;
}

Vector2 GraphNode::get_output_port_position(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, right_port_cache.size(), Vector2());
	Vector2 pos = right_port_cache[p_port_idx].pos;
	return pos;
}

int GraphNode::get_output_port_type(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, right_port_cache.size(), 0);
	return right_port_cache[p_port_idx].type;
}

Color GraphNode::get_output_port_color(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, right_port_cache.size(), Color());
	return right_port_cache[p_port_idx].color;
}

int GraphNode::get_output_port_slot(int p_port_idx) {
	if (port_pos_dirty) {
		_port_pos_update();
	}

	ERR_FAIL_INDEX_V(p_port_idx, right_port_cache.size(), -1);
	return right_port_cache[p_port_idx].slot_index;
}

void GraphNode::set_title(const String &p_title) {
	if (title == p_title) {
		return;
	}
	title = p_title;
	if (title_label) {
		title_label->set_text(title);
	}
	update_minimum_size();
}

String GraphNode::get_title() const {
	return title;
}

HBoxContainer *GraphNode::get_titlebar_hbox() {
	return titlebar_hbox;
}

Control::CursorShape GraphNode::get_cursor_shape(const Point2 &p_pos) const {
	if (resizable) {
		if (resizing || (p_pos.x > get_size().x - theme_cache.resizer->get_width() && p_pos.y > get_size().y - theme_cache.resizer->get_height())) {
			return CURSOR_FDIAGSIZE;
		}
	}

	return Control::get_cursor_shape(p_pos);
}

Vector<int> GraphNode::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

Vector<int> GraphNode::get_allowed_size_flags_vertical() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	flags.append(SIZE_EXPAND);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

void GraphNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &GraphNode::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &GraphNode::get_title);

	ClassDB::bind_method(D_METHOD("get_titlebar_hbox"), &GraphNode::get_titlebar_hbox);

	ClassDB::bind_method(D_METHOD("set_slot", "slot_index", "enable_left_port", "type_left", "color_left", "enable_right_port", "type_right", "color_right", "custom_icon_left", "custom_icon_right", "draw_stylebox"), &GraphNode::set_slot, DEFVAL(Ref<Texture2D>()), DEFVAL(Ref<Texture2D>()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("clear_slot", "slot_index"), &GraphNode::clear_slot);
	ClassDB::bind_method(D_METHOD("clear_all_slots"), &GraphNode::clear_all_slots);

	ClassDB::bind_method(D_METHOD("is_slot_enabled_left", "slot_index"), &GraphNode::is_slot_enabled_left);
	ClassDB::bind_method(D_METHOD("set_slot_enabled_left", "slot_index", "enable"), &GraphNode::set_slot_enabled_left);

	ClassDB::bind_method(D_METHOD("set_slot_type_left", "slot_index", "type"), &GraphNode::set_slot_type_left);
	ClassDB::bind_method(D_METHOD("get_slot_type_left", "slot_index"), &GraphNode::get_slot_type_left);

	ClassDB::bind_method(D_METHOD("set_slot_color_left", "slot_index", "color"), &GraphNode::set_slot_color_left);
	ClassDB::bind_method(D_METHOD("get_slot_color_left", "slot_index"), &GraphNode::get_slot_color_left);

	ClassDB::bind_method(D_METHOD("set_slot_custom_icon_left", "slot_index", "custom_icon"), &GraphNode::set_slot_custom_icon_left);
	ClassDB::bind_method(D_METHOD("get_slot_custom_icon_left", "slot_index"), &GraphNode::get_slot_custom_icon_left);

	ClassDB::bind_method(D_METHOD("is_slot_enabled_right", "slot_index"), &GraphNode::is_slot_enabled_right);
	ClassDB::bind_method(D_METHOD("set_slot_enabled_right", "slot_index", "enable"), &GraphNode::set_slot_enabled_right);

	ClassDB::bind_method(D_METHOD("set_slot_type_right", "slot_index", "type"), &GraphNode::set_slot_type_right);
	ClassDB::bind_method(D_METHOD("get_slot_type_right", "slot_index"), &GraphNode::get_slot_type_right);

	ClassDB::bind_method(D_METHOD("set_slot_color_right", "slot_index", "color"), &GraphNode::set_slot_color_right);
	ClassDB::bind_method(D_METHOD("get_slot_color_right", "slot_index"), &GraphNode::get_slot_color_right);

	ClassDB::bind_method(D_METHOD("set_slot_custom_icon_right", "slot_index", "custom_icon"), &GraphNode::set_slot_custom_icon_right);
	ClassDB::bind_method(D_METHOD("get_slot_custom_icon_right", "slot_index"), &GraphNode::get_slot_custom_icon_right);

	ClassDB::bind_method(D_METHOD("is_slot_draw_stylebox", "slot_index"), &GraphNode::is_slot_draw_stylebox);
	ClassDB::bind_method(D_METHOD("set_slot_draw_stylebox", "slot_index", "enable"), &GraphNode::set_slot_draw_stylebox);

	ClassDB::bind_method(D_METHOD("set_ignore_invalid_connection_type", "ignore"), &GraphNode::set_ignore_invalid_connection_type);
	ClassDB::bind_method(D_METHOD("is_ignoring_valid_connection_type"), &GraphNode::is_ignoring_valid_connection_type);

	ClassDB::bind_method(D_METHOD("get_input_port_count"), &GraphNode::get_input_port_count);
	ClassDB::bind_method(D_METHOD("get_input_port_position", "port_idx"), &GraphNode::get_input_port_position);
	ClassDB::bind_method(D_METHOD("get_input_port_type", "port_idx"), &GraphNode::get_input_port_type);
	ClassDB::bind_method(D_METHOD("get_input_port_color", "port_idx"), &GraphNode::get_input_port_color);
	ClassDB::bind_method(D_METHOD("get_input_port_slot", "port_idx"), &GraphNode::get_input_port_slot);

	ClassDB::bind_method(D_METHOD("get_output_port_count"), &GraphNode::get_output_port_count);
	ClassDB::bind_method(D_METHOD("get_output_port_position", "port_idx"), &GraphNode::get_output_port_position);
	ClassDB::bind_method(D_METHOD("get_output_port_type", "port_idx"), &GraphNode::get_output_port_type);
	ClassDB::bind_method(D_METHOD("get_output_port_color", "port_idx"), &GraphNode::get_output_port_color);
	ClassDB::bind_method(D_METHOD("get_output_port_slot", "port_idx"), &GraphNode::get_output_port_slot);

	GDVIRTUAL_BIND(_draw_port, "slot_index", "position", "left", "color")

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ignore_invalid_connection_type"), "set_ignore_invalid_connection_type", "is_ignoring_valid_connection_type");

	ADD_SIGNAL(MethodInfo("slot_updated", PropertyInfo(Variant::INT, "slot_index")));

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphNode, panel);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphNode, panel_selected);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphNode, titlebar);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphNode, titlebar_selected);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, GraphNode, slot);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, GraphNode, separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, GraphNode, port_h_offset);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, GraphNode, port);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, GraphNode, resizer);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, GraphNode, resizer_color);
}

GraphNode::GraphNode() {
	titlebar_hbox = memnew(HBoxContainer);
	titlebar_hbox->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(titlebar_hbox, false, INTERNAL_MODE_FRONT);

	title_label = memnew(Label);
	title_label->set_theme_type_variation("GraphNodeTitleLabel");
	title_label->set_h_size_flags(SIZE_EXPAND_FILL);
	titlebar_hbox->add_child(title_label);

	set_mouse_filter(MOUSE_FILTER_STOP);
}
