/**************************************************************************/
/*  tree.cpp                                                              */
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

#include "tree.h"

#include "core/math/math_funcs.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "scene/main/viewport.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#endif

#include <limits.h>

void TreeItem::move_to_top() {
	if (!parent || parent->children == this) {
		return; //already on top
	}
	TreeItem *prev = get_prev();
	prev->next = next;
	next = parent->children;
	parent->children = this;
}

void TreeItem::move_to_bottom() {
	if (!parent || !next) {
		return;
	}

	TreeItem *prev = get_prev();
	TreeItem *last = next;
	while (last->next) {
		last = last->next;
	}

	if (prev) {
		prev->next = next;
	} else {
		parent->children = next;
	}
	last->next = this;
	next = nullptr;
}

Size2 TreeItem::Cell::get_icon_size() const {
	if (icon.is_null()) {
		return Size2();
	}
	if (icon_region == Rect2i()) {
		return icon->get_size();
	} else {
		return icon_region.size;
	}
}

void TreeItem::Cell::draw_icon(const RID &p_where, const Point2 &p_pos, const Size2 &p_size, const Color &p_color) const {
	if (icon.is_null()) {
		return;
	}

	Size2i dsize = (p_size == Size2()) ? icon->get_size() : p_size;

	if (icon_region == Rect2i()) {
		icon->draw_rect_region(p_where, Rect2(p_pos, dsize), Rect2(Point2(), icon->get_size()), p_color);
	} else {
		icon->draw_rect_region(p_where, Rect2(p_pos, dsize), icon_region, p_color);
	}
}

void TreeItem::_changed_notify(int p_cell) {
	tree->item_changed(p_cell, this);
}

void TreeItem::_changed_notify() {
	tree->item_changed(-1, this);
}

void TreeItem::_cell_selected(int p_cell) {
	tree->item_selected(p_cell, this);
}

void TreeItem::_cell_deselected(int p_cell) {
	tree->item_deselected(p_cell, this);
}

/* cell mode */
void TreeItem::set_cell_mode(int p_column, TreeCellMode p_mode) {
	ERR_FAIL_INDEX(p_column, cells.size());
	Cell &c = cells.write[p_column];
	c.mode = p_mode;
	c.min = 0;
	c.max = 100;
	c.step = 1;
	c.val = 0;
	c.checked = false;
	c.icon = Ref<Texture>();
	c.text = "";
	c.icon_max_w = 0;
	_changed_notify(p_column);
}

TreeItem::TreeCellMode TreeItem::get_cell_mode(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), TreeItem::CELL_MODE_STRING);
	return cells[p_column].mode;
}

/* check mode */
void TreeItem::set_checked(int p_column, bool p_checked) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].checked = p_checked;
	_changed_notify(p_column);
}

bool TreeItem::is_checked(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].checked;
}

void TreeItem::set_text(int p_column, String p_text) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].text = p_text;

	if (cells[p_column].mode == TreeItem::CELL_MODE_RANGE) {
		Vector<String> strings = p_text.split(",");
		cells.write[p_column].min = INT_MAX;
		cells.write[p_column].max = INT_MIN;
		for (int i = 0; i < strings.size(); i++) {
			int value = i;
			if (!strings[i].get_slicec(':', 1).empty()) {
				value = strings[i].get_slicec(':', 1).to_int();
			}
			cells.write[p_column].min = MIN(cells[p_column].min, value);
			cells.write[p_column].max = MAX(cells[p_column].max, value);
		}
		cells.write[p_column].step = 0;
	}
	_changed_notify(p_column);
}

String TreeItem::get_text(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].text;
}

void TreeItem::set_suffix(int p_column, String p_suffix) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].suffix = p_suffix;

	_changed_notify(p_column);
}

String TreeItem::get_suffix(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].suffix;
}

void TreeItem::set_icon(int p_column, const Ref<Texture> &p_icon) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].icon = p_icon;
	_changed_notify(p_column);
}

Ref<Texture> TreeItem::get_icon(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Texture>());
	return cells[p_column].icon;
}

void TreeItem::set_icon_region(int p_column, const Rect2 &p_icon_region) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].icon_region = p_icon_region;
	_changed_notify(p_column);
}

Rect2 TreeItem::get_icon_region(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Rect2());
	return cells[p_column].icon_region;
}

void TreeItem::set_icon_modulate(int p_column, const Color &p_modulate) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].icon_color = p_modulate;
	_changed_notify(p_column);
}

Color TreeItem::get_icon_modulate(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Color());
	return cells[p_column].icon_color;
}

void TreeItem::set_icon_max_width(int p_column, int p_max) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].icon_max_w = p_max;
	_changed_notify(p_column);
}

int TreeItem::get_icon_max_width(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), 0);
	return cells[p_column].icon_max_w;
}

/* range works for mode number or mode combo */
void TreeItem::set_range(int p_column, double p_value) {
	ERR_FAIL_INDEX(p_column, cells.size());
	if (cells[p_column].step > 0) {
		p_value = Math::stepify(p_value, cells[p_column].step);
	}
	if (p_value < cells[p_column].min) {
		p_value = cells[p_column].min;
	}
	if (p_value > cells[p_column].max) {
		p_value = cells[p_column].max;
	}

	cells.write[p_column].val = p_value;
	_changed_notify(p_column);
}

double TreeItem::get_range(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), 0);
	return cells[p_column].val;
}

bool TreeItem::is_range_exponential(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].expr;
}
void TreeItem::set_range_config(int p_column, double p_min, double p_max, double p_step, bool p_exp) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].min = p_min;
	cells.write[p_column].max = p_max;
	cells.write[p_column].step = p_step;
	cells.write[p_column].expr = p_exp;
	_changed_notify(p_column);
}

void TreeItem::get_range_config(int p_column, double &r_min, double &r_max, double &r_step) const {
	ERR_FAIL_INDEX(p_column, cells.size());
	r_min = cells[p_column].min;
	r_max = cells[p_column].max;
	r_step = cells[p_column].step;
}

void TreeItem::set_metadata(int p_column, const Variant &p_meta) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].meta = p_meta;
}

Variant TreeItem::get_metadata(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Variant());

	return cells[p_column].meta;
}

void TreeItem::set_custom_draw(int p_column, Object *p_object, const StringName &p_callback) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_NULL(p_object);

	cells.write[p_column].custom_draw_obj = p_object->get_instance_id();
	cells.write[p_column].custom_draw_callback = p_callback;
}

void TreeItem::set_collapsed(bool p_collapsed) {
	if (collapsed == p_collapsed || !tree) {
		return;
	}
	collapsed = p_collapsed;
	TreeItem *ci = tree->selected_item;
	if (ci) {
		while (ci && ci != this) {
			ci = ci->parent;
		}
		if (ci) { // collapsing cursor/selected, move it!

			if (tree->select_mode == Tree::SELECT_MULTI) {
				tree->selected_item = this;
				emit_signal("cell_selected");
			} else {
				select(tree->selected_col);
			}

			tree->update();
		}
	}

	_changed_notify();
	tree->emit_signal("item_collapsed", this);
}

bool TreeItem::is_collapsed() {
	return collapsed;
}

void TreeItem::set_custom_minimum_height(int p_height) {
	custom_min_height = p_height;
	_changed_notify();
}

int TreeItem::get_custom_minimum_height() const {
	return custom_min_height;
}

TreeItem *TreeItem::get_next() {
	return next;
}

TreeItem *TreeItem::get_prev() {
	if (!parent || parent->children == this) {
		return nullptr;
	}

	TreeItem *prev = parent->children;
	while (prev && prev->next != this) {
		prev = prev->next;
	}

	return prev;
}

TreeItem *TreeItem::get_parent() {
	return parent;
}

TreeItem *TreeItem::get_children() {
	return children;
}

TreeItem *TreeItem::get_prev_visible(bool p_wrap) {
	TreeItem *current = this;

	TreeItem *prev = current->get_prev();

	if (!prev) {
		current = current->parent;
		if (current == tree->root && tree->hide_root) {
			return nullptr;
		} else if (!current) {
			if (p_wrap) {
				current = this;
				TreeItem *temp = this->get_next_visible();
				while (temp) {
					current = temp;
					temp = temp->get_next_visible();
				}
			} else {
				return nullptr;
			}
		}
	} else {
		current = prev;
		while (!current->collapsed && current->children) {
			//go to the very end

			current = current->children;
			while (current->next) {
				current = current->next;
			}
		}
	}

	return current;
}

TreeItem *TreeItem::get_next_visible(bool p_wrap) {
	TreeItem *current = this;

	if (!current->collapsed && current->children) {
		current = current->children;

	} else if (current->next) {
		current = current->next;
	} else {
		while (current && !current->next) {
			current = current->parent;
		}

		if (!current) {
			if (p_wrap) {
				return tree->root;
			} else {
				return nullptr;
			}
		} else {
			current = current->next;
		}
	}

	return current;
}

void TreeItem::remove_child(TreeItem *p_item) {
	ERR_FAIL_NULL(p_item);
	TreeItem **c = &children;

	while (*c) {
		if ((*c) == p_item) {
			TreeItem *aux = *c;

			*c = (*c)->next;

			aux->parent = nullptr;

			if (tree) {
				tree->update();
			}
			return;
		}

		c = &(*c)->next;
	}

	ERR_FAIL();
}

void TreeItem::set_selectable(int p_column, bool p_selectable) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].selectable = p_selectable;
}

bool TreeItem::is_selectable(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].selectable;
}

bool TreeItem::is_selected(int p_column) {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].selectable && cells[p_column].selected;
}

void TreeItem::set_as_cursor(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	if (!tree) {
		return;
	}
	if (tree->select_mode != Tree::SELECT_MULTI) {
		return;
	}
	tree->selected_item = this;
	tree->selected_col = p_column;
	tree->update();
}

void TreeItem::select(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	_cell_selected(p_column);
}

void TreeItem::deselect(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	_cell_deselected(p_column);
}

void TreeItem::add_button(int p_column, const Ref<Texture> &p_button, int p_id, bool p_disabled, const String &p_tooltip) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_COND(!p_button.is_valid());
	TreeItem::Cell::Button button;
	button.texture = p_button;
	if (p_id < 0) {
		p_id = cells[p_column].buttons.size();
	}
	button.id = p_id;
	button.disabled = p_disabled;
	button.tooltip = p_tooltip;
	cells.write[p_column].buttons.push_back(button);
	_changed_notify(p_column);
}

int TreeItem::get_button_count(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	return cells[p_column].buttons.size();
}
Ref<Texture> TreeItem::get_button(int p_column, int p_idx) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Texture>());
	ERR_FAIL_INDEX_V(p_idx, cells[p_column].buttons.size(), Ref<Texture>());
	return cells[p_column].buttons[p_idx].texture;
}
String TreeItem::get_button_tooltip(int p_column, int p_idx) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), String());
	ERR_FAIL_INDEX_V(p_idx, cells[p_column].buttons.size(), String());
	return cells[p_column].buttons[p_idx].tooltip;
}
int TreeItem::get_button_id(int p_column, int p_idx) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	ERR_FAIL_INDEX_V(p_idx, cells[p_column].buttons.size(), -1);
	return cells[p_column].buttons[p_idx].id;
}
void TreeItem::erase_button(int p_column, int p_idx) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_idx, cells[p_column].buttons.size());
	cells.write[p_column].buttons.remove(p_idx);
	_changed_notify(p_column);
}

int TreeItem::get_button_by_id(int p_column, int p_id) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	for (int i = 0; i < cells[p_column].buttons.size(); i++) {
		if (cells[p_column].buttons[i].id == p_id) {
			return i;
		}
	}

	return -1;
}

void TreeItem::set_button(int p_column, int p_idx, const Ref<Texture> &p_button) {
	ERR_FAIL_COND(p_button.is_null());
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_idx, cells[p_column].buttons.size());
	cells.write[p_column].buttons.write[p_idx].texture = p_button;
	_changed_notify(p_column);
}

void TreeItem::set_button_color(int p_column, int p_idx, const Color &p_color) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_idx, cells[p_column].buttons.size());
	cells.write[p_column].buttons.write[p_idx].color = p_color;
	_changed_notify(p_column);
}

void TreeItem::set_button_disabled(int p_column, int p_idx, bool p_disabled) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_idx, cells[p_column].buttons.size());

	cells.write[p_column].buttons.write[p_idx].disabled = p_disabled;
	_changed_notify(p_column);
}

bool TreeItem::is_button_disabled(int p_column, int p_idx) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	ERR_FAIL_INDEX_V(p_idx, cells[p_column].buttons.size(), false);

	return cells[p_column].buttons[p_idx].disabled;
}

void TreeItem::set_editable(int p_column, bool p_editable) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].editable = p_editable;
	_changed_notify(p_column);
}

bool TreeItem::is_editable(int p_column) {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].editable;
}

void TreeItem::set_custom_color(int p_column, const Color &p_color) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].custom_color = true;
	cells.write[p_column].color = p_color;
	_changed_notify(p_column);
}
Color TreeItem::get_custom_color(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Color());
	if (!cells[p_column].custom_color) {
		return Color();
	}
	return cells[p_column].color;
}
void TreeItem::clear_custom_color(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].custom_color = false;
	cells.write[p_column].color = Color();
	_changed_notify(p_column);
}

void TreeItem::set_tooltip(int p_column, const String &p_tooltip) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].tooltip = p_tooltip;
}

String TreeItem::get_tooltip(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].tooltip;
}

void TreeItem::set_custom_bg_color(int p_column, const Color &p_color, bool p_bg_outline) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].custom_bg_color = true;
	cells.write[p_column].custom_bg_outline = p_bg_outline;
	cells.write[p_column].bg_color = p_color;
	_changed_notify(p_column);
}

void TreeItem::clear_custom_bg_color(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].custom_bg_color = false;
	cells.write[p_column].bg_color = Color();
	_changed_notify(p_column);
}

Color TreeItem::get_custom_bg_color(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Color());
	if (!cells[p_column].custom_bg_color) {
		return Color();
	}
	return cells[p_column].bg_color;
}

void TreeItem::set_custom_as_button(int p_column, bool p_button) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].custom_button = p_button;
}

bool TreeItem::is_custom_set_as_button(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].custom_button;
}

void TreeItem::set_text_align(int p_column, TextAlign p_align) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].text_align = p_align;
	_changed_notify(p_column);
}

TreeItem::TextAlign TreeItem::get_text_align(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), ALIGN_LEFT);
	return cells[p_column].text_align;
}

void TreeItem::set_expand_right(int p_column, bool p_enable) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].expand_right = p_enable;
	_changed_notify(p_column);
}

bool TreeItem::get_expand_right(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].expand_right;
}

void TreeItem::set_disable_folding(bool p_disable) {
	disable_folding = p_disable;
	_changed_notify(0);
}

bool TreeItem::is_folding_disabled() const {
	return disable_folding;
}

Variant TreeItem::_call_recursive_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 0;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	StringName method = *p_args[0];

	call_recursive(method, &p_args[1], p_argcount - 1, r_error);
	return Variant();
}

void recursive_call_aux(TreeItem *p_item, const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (!p_item) {
		return;
	}
	p_item->call(p_method, p_args, p_argcount, r_error);
	TreeItem *c = p_item->get_children();
	while (c) {
		recursive_call_aux(c, p_method, p_args, p_argcount, r_error);
		c = c->get_next();
	}
}

void TreeItem::call_recursive(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	recursive_call_aux(this, p_method, p_args, p_argcount, r_error);
}

void TreeItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cell_mode", "column", "mode"), &TreeItem::set_cell_mode);
	ClassDB::bind_method(D_METHOD("get_cell_mode", "column"), &TreeItem::get_cell_mode);

	ClassDB::bind_method(D_METHOD("set_checked", "column", "checked"), &TreeItem::set_checked);
	ClassDB::bind_method(D_METHOD("is_checked", "column"), &TreeItem::is_checked);

	ClassDB::bind_method(D_METHOD("set_text", "column", "text"), &TreeItem::set_text);
	ClassDB::bind_method(D_METHOD("get_text", "column"), &TreeItem::get_text);

	ClassDB::bind_method(D_METHOD("set_suffix", "column", "text"), &TreeItem::set_suffix);
	ClassDB::bind_method(D_METHOD("get_suffix", "column"), &TreeItem::get_suffix);

	ClassDB::bind_method(D_METHOD("set_icon", "column", "texture"), &TreeItem::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon", "column"), &TreeItem::get_icon);

	ClassDB::bind_method(D_METHOD("set_icon_region", "column", "region"), &TreeItem::set_icon_region);
	ClassDB::bind_method(D_METHOD("get_icon_region", "column"), &TreeItem::get_icon_region);

	ClassDB::bind_method(D_METHOD("set_icon_max_width", "column", "width"), &TreeItem::set_icon_max_width);
	ClassDB::bind_method(D_METHOD("get_icon_max_width", "column"), &TreeItem::get_icon_max_width);

	ClassDB::bind_method(D_METHOD("set_icon_modulate", "column", "modulate"), &TreeItem::set_icon_modulate);
	ClassDB::bind_method(D_METHOD("get_icon_modulate", "column"), &TreeItem::get_icon_modulate);

	ClassDB::bind_method(D_METHOD("set_range", "column", "value"), &TreeItem::set_range);
	ClassDB::bind_method(D_METHOD("get_range", "column"), &TreeItem::get_range);
	ClassDB::bind_method(D_METHOD("set_range_config", "column", "min", "max", "step", "expr"), &TreeItem::set_range_config, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_range_config", "column"), &TreeItem::_get_range_config);

	ClassDB::bind_method(D_METHOD("set_metadata", "column", "meta"), &TreeItem::set_metadata);
	ClassDB::bind_method(D_METHOD("get_metadata", "column"), &TreeItem::get_metadata);

	ClassDB::bind_method(D_METHOD("set_custom_draw", "column", "object", "callback"), &TreeItem::set_custom_draw);

	ClassDB::bind_method(D_METHOD("set_collapsed", "enable"), &TreeItem::set_collapsed);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &TreeItem::is_collapsed);

	ClassDB::bind_method(D_METHOD("set_custom_minimum_height", "height"), &TreeItem::set_custom_minimum_height);
	ClassDB::bind_method(D_METHOD("get_custom_minimum_height"), &TreeItem::get_custom_minimum_height);

	ClassDB::bind_method(D_METHOD("get_next"), &TreeItem::get_next);
	ClassDB::bind_method(D_METHOD("get_prev"), &TreeItem::get_prev);
	ClassDB::bind_method(D_METHOD("get_parent"), &TreeItem::get_parent);
	ClassDB::bind_method(D_METHOD("get_children"), &TreeItem::get_children);

	ClassDB::bind_method(D_METHOD("get_next_visible", "wrap"), &TreeItem::get_next_visible, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_prev_visible", "wrap"), &TreeItem::get_prev_visible, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("remove_child", "child"), &TreeItem::_remove_child);

	ClassDB::bind_method(D_METHOD("set_selectable", "column", "selectable"), &TreeItem::set_selectable);
	ClassDB::bind_method(D_METHOD("is_selectable", "column"), &TreeItem::is_selectable);

	ClassDB::bind_method(D_METHOD("is_selected", "column"), &TreeItem::is_selected);
	ClassDB::bind_method(D_METHOD("select", "column"), &TreeItem::select);
	ClassDB::bind_method(D_METHOD("deselect", "column"), &TreeItem::deselect);

	ClassDB::bind_method(D_METHOD("set_editable", "column", "enabled"), &TreeItem::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable", "column"), &TreeItem::is_editable);

	ClassDB::bind_method(D_METHOD("set_custom_color", "column", "color"), &TreeItem::set_custom_color);
	ClassDB::bind_method(D_METHOD("clear_custom_color", "column"), &TreeItem::clear_custom_color);
	ClassDB::bind_method(D_METHOD("get_custom_color", "column"), &TreeItem::get_custom_color);

	ClassDB::bind_method(D_METHOD("set_custom_bg_color", "column", "color", "just_outline"), &TreeItem::set_custom_bg_color, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("clear_custom_bg_color", "column"), &TreeItem::clear_custom_bg_color);
	ClassDB::bind_method(D_METHOD("get_custom_bg_color", "column"), &TreeItem::get_custom_bg_color);

	ClassDB::bind_method(D_METHOD("set_custom_as_button", "column", "enable"), &TreeItem::set_custom_as_button);
	ClassDB::bind_method(D_METHOD("is_custom_set_as_button", "column"), &TreeItem::is_custom_set_as_button);

	ClassDB::bind_method(D_METHOD("add_button", "column", "button", "id", "disabled", "tooltip"), &TreeItem::add_button, DEFVAL(-1), DEFVAL(false), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_button_count", "column"), &TreeItem::get_button_count);
	ClassDB::bind_method(D_METHOD("get_button_tooltip", "column", "button_idx"), &TreeItem::get_button_tooltip);
	ClassDB::bind_method(D_METHOD("get_button_id", "column", "button_idx"), &TreeItem::get_button_id);
	ClassDB::bind_method(D_METHOD("get_button_by_id", "column", "id"), &TreeItem::get_button_by_id);
	ClassDB::bind_method(D_METHOD("get_button", "column", "button_idx"), &TreeItem::get_button);
	ClassDB::bind_method(D_METHOD("set_button", "column", "button_idx", "button"), &TreeItem::set_button);
	ClassDB::bind_method(D_METHOD("erase_button", "column", "button_idx"), &TreeItem::erase_button);
	ClassDB::bind_method(D_METHOD("set_button_disabled", "column", "button_idx", "disabled"), &TreeItem::set_button_disabled);
	ClassDB::bind_method(D_METHOD("is_button_disabled", "column", "button_idx"), &TreeItem::is_button_disabled);

	ClassDB::bind_method(D_METHOD("set_expand_right", "column", "enable"), &TreeItem::set_expand_right);
	ClassDB::bind_method(D_METHOD("get_expand_right", "column"), &TreeItem::get_expand_right);

	ClassDB::bind_method(D_METHOD("set_tooltip", "column", "tooltip"), &TreeItem::set_tooltip);
	ClassDB::bind_method(D_METHOD("get_tooltip", "column"), &TreeItem::get_tooltip);
	ClassDB::bind_method(D_METHOD("set_text_align", "column", "text_align"), &TreeItem::set_text_align);
	ClassDB::bind_method(D_METHOD("get_text_align", "column"), &TreeItem::get_text_align);
	ClassDB::bind_method(D_METHOD("move_to_top"), &TreeItem::move_to_top);
	ClassDB::bind_method(D_METHOD("move_to_bottom"), &TreeItem::move_to_bottom);

	ClassDB::bind_method(D_METHOD("set_disable_folding", "disable"), &TreeItem::set_disable_folding);
	ClassDB::bind_method(D_METHOD("is_folding_disabled"), &TreeItem::is_folding_disabled);

	{
		MethodInfo mi;
		mi.name = "call_recursive";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_recursive", &TreeItem::_call_recursive_bind, mi);
	}

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collapsed"), "set_collapsed", "is_collapsed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_folding"), "set_disable_folding", "is_folding_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "custom_minimum_height", PROPERTY_HINT_RANGE, "0,1000,1"), "set_custom_minimum_height", "get_custom_minimum_height");

	BIND_ENUM_CONSTANT(CELL_MODE_STRING);
	BIND_ENUM_CONSTANT(CELL_MODE_CHECK);
	BIND_ENUM_CONSTANT(CELL_MODE_RANGE);
	BIND_ENUM_CONSTANT(CELL_MODE_ICON);
	BIND_ENUM_CONSTANT(CELL_MODE_CUSTOM);

	BIND_ENUM_CONSTANT(ALIGN_LEFT);
	BIND_ENUM_CONSTANT(ALIGN_CENTER);
	BIND_ENUM_CONSTANT(ALIGN_RIGHT);
}

void TreeItem::clear_children() {
	TreeItem *c = children;
	while (c) {
		TreeItem *aux = c;
		c = c->get_next();
		aux->parent = nullptr; // so it won't try to recursively autoremove from me in here
		memdelete(aux);
	}

	children = nullptr;
};

TreeItem::TreeItem(Tree *p_tree) {
	tree = p_tree;
	collapsed = false;
	disable_folding = false;
	custom_min_height = 0;

	parent = nullptr; // parent item
	next = nullptr; // next in list
	children = nullptr; //child items
}

TreeItem::~TreeItem() {
	clear_children();

	if (parent) {
		parent->remove_child(this); // Also updates the Tree.
	} else if (tree) {
		tree->update();
	}

	if (tree && tree->root == this) {
		tree->root = nullptr;
	}

	if (tree && tree->popup_edited_item == this) {
		tree->popup_edited_item = nullptr;
		tree->pressing_for_editor = false;
	}

	if (tree && tree->cache.hover_item == this) {
		tree->cache.hover_item = nullptr;
	}

	if (tree && tree->selected_item == this) {
		tree->selected_item = nullptr;
	}

	if (tree && tree->drop_mode_over == this) {
		tree->drop_mode_over = nullptr;
	}

	if (tree && tree->single_select_defer == this) {
		tree->single_select_defer = nullptr;
	}

	if (tree && tree->edited_item == this) {
		tree->edited_item = nullptr;
		tree->pressing_for_editor = false;
	}
}

/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/

void Tree::update_cache() {
	cache.font = get_font("font");
	cache.tb_font = get_font("title_button_font");
	cache.bg = get_stylebox("bg");
	cache.selected = get_stylebox("selected");
	cache.selected_focus = get_stylebox("selected_focus");
	cache.cursor = get_stylebox("cursor");
	cache.cursor_unfocus = get_stylebox("cursor_unfocused");
	cache.button_pressed = get_stylebox("button_pressed");

	cache.checked = get_icon("checked");
	cache.unchecked = get_icon("unchecked");
	cache.arrow_collapsed = get_icon("arrow_collapsed");
	cache.arrow = get_icon("arrow");
	cache.select_arrow = get_icon("select_arrow");
	cache.updown = get_icon("updown");

	cache.custom_button = get_stylebox("custom_button");
	cache.custom_button_hover = get_stylebox("custom_button_hover");
	cache.custom_button_pressed = get_stylebox("custom_button_pressed");
	cache.custom_button_font_highlight = get_color("custom_button_font_highlight");

	cache.font_color = get_color("font_color");
	cache.font_color_selected = get_color("font_color_selected");
	cache.guide_color = get_color("guide_color");
	cache.drop_position_color = get_color("drop_position_color");
	cache.hseparation = get_constant("hseparation");
	cache.vseparation = get_constant("vseparation");
	cache.item_margin = get_constant("item_margin");
	cache.button_margin = get_constant("button_margin");
	cache.draw_guides = get_constant("draw_guides");
	cache.draw_relationship_lines = get_constant("draw_relationship_lines");
	cache.relationship_line_color = get_color("relationship_line_color");
	cache.scroll_border = get_constant("scroll_border");
	cache.scroll_speed = get_constant("scroll_speed");

	cache.title_button = get_stylebox("title_button_normal");
	cache.title_button_pressed = get_stylebox("title_button_pressed");
	cache.title_button_hover = get_stylebox("title_button_hover");
	cache.title_button_color = get_color("title_button_color");

	v_scroll->set_custom_step(cache.font->get_height());
}

int Tree::compute_item_height(TreeItem *p_item) const {
	if (p_item == root && hide_root) {
		return 0;
	}

	ERR_FAIL_COND_V(cache.font.is_null(), 0);
	int height = cache.font->get_height();

	for (int i = 0; i < columns.size(); i++) {
		for (int j = 0; j < p_item->cells[i].buttons.size(); j++) {
			Size2i s; // = cache.button_pressed->get_minimum_size();
			s += p_item->cells[i].buttons[j].texture->get_size();
			if (s.height > height) {
				height = s.height;
			}
		}

		switch (p_item->cells[i].mode) {
			case TreeItem::CELL_MODE_CHECK: {
				int check_icon_h = cache.checked->get_height();
				if (height < check_icon_h) {
					height = check_icon_h;
				}
				FALLTHROUGH;
			}
			case TreeItem::CELL_MODE_STRING:
			case TreeItem::CELL_MODE_CUSTOM:
			case TreeItem::CELL_MODE_ICON: {
				Ref<Texture> icon = p_item->cells[i].icon;
				if (!icon.is_null()) {
					Size2i s = p_item->cells[i].get_icon_size();
					if (p_item->cells[i].icon_max_w > 0 && s.width > p_item->cells[i].icon_max_w) {
						s.height = s.height * p_item->cells[i].icon_max_w / s.width;
					}
					if (s.height > height) {
						height = s.height;
					}
				}
				if (p_item->cells[i].mode == TreeItem::CELL_MODE_CUSTOM && p_item->cells[i].custom_button) {
					height += cache.custom_button->get_minimum_size().height;
				}

			} break;
			default: {
			}
		}
	}
	int item_min_height = p_item->get_custom_minimum_height();
	if (height < item_min_height) {
		height = item_min_height;
	}

	height += cache.vseparation;

	return height;
}

int Tree::get_item_height(TreeItem *p_item) const {
	int height = compute_item_height(p_item);
	height += cache.vseparation;

	if (!p_item->collapsed) { /* if not collapsed, check the children */

		TreeItem *c = p_item->children;

		while (c) {
			height += get_item_height(c);

			c = c->next;
		}
	}

	return height;
}

void Tree::draw_item_rect(const TreeItem::Cell &p_cell, const Rect2i &p_rect, const Color &p_color, const Color &p_icon_color) {
	ERR_FAIL_COND(cache.font.is_null());

	Rect2i rect = p_rect;
	Ref<Font> font = cache.font;
	String text = p_cell.text;
	if (p_cell.suffix != String()) {
		text += " " + p_cell.suffix;
	}

	int w = 0;
	if (!p_cell.icon.is_null()) {
		Size2i bmsize = p_cell.get_icon_size();

		if (p_cell.icon_max_w > 0 && bmsize.width > p_cell.icon_max_w) {
			bmsize.width = p_cell.icon_max_w;
		}
		w += bmsize.width + cache.hseparation;
	}
	w += font->get_string_size(text).width;

	switch (p_cell.text_align) {
		case TreeItem::ALIGN_LEFT:
			break; //do none
		case TreeItem::ALIGN_CENTER:
			rect.position.x += MAX(0, (rect.size.width - w) / 2);
			break; //do none
		case TreeItem::ALIGN_RIGHT:
			rect.position.x += MAX(0, (rect.size.width - w));
			break; //do none
	}

	RID ci = get_canvas_item();
	if (!p_cell.icon.is_null()) {
		Size2i bmsize = p_cell.get_icon_size();

		if (p_cell.icon_max_w > 0 && bmsize.width > p_cell.icon_max_w) {
			bmsize.height = bmsize.height * p_cell.icon_max_w / bmsize.width;
			bmsize.width = p_cell.icon_max_w;
		}

		p_cell.draw_icon(ci, rect.position + Size2i(0, Math::floor((real_t)(rect.size.y - bmsize.y) / 2)), bmsize, p_icon_color);
		rect.position.x += bmsize.x + cache.hseparation;
		rect.size.x -= bmsize.x + cache.hseparation;
	}

	rect.position.y += Math::floor((rect.size.y - font->get_height()) / 2.0) + font->get_ascent();
	font->draw(ci, rect.position, text, p_color, rect.size.x);
}

int Tree::draw_item(const Point2i &p_pos, const Point2 &p_draw_ofs, const Size2 &p_draw_size, TreeItem *p_item) {
	if (p_pos.y - cache.offset.y > (p_draw_size.height)) {
		return -1; //draw no more!
	}

	RID ci = get_canvas_item();

	int htotal = 0;

	int label_h = compute_item_height(p_item);

	/* Calculate height of the label part */
	label_h += cache.vseparation;

	/* Draw label, if height fits */

	bool skip = (p_item == root && hide_root);

	if (!skip && (p_pos.y + label_h - cache.offset.y) > 0) {
		//draw separation.
		//if (p_item->get_parent()!=root || !hide_root)

		ERR_FAIL_COND_V(cache.font.is_null(), -1);
		Ref<Font> font = cache.font;

		int font_ascent = font->get_ascent();

		int ofs = p_pos.x + ((p_item->disable_folding || hide_folding) ? cache.hseparation : cache.item_margin);
		int skip2 = 0;
		for (int i = 0; i < columns.size(); i++) {
			if (skip2) {
				skip2--;
				continue;
			}

			int w = get_column_width(i);

			if (i == 0) {
				w -= ofs;

				if (w <= 0) {
					ofs = get_column_width(0);
					continue;
				}
			} else {
				ofs += cache.hseparation;
				w -= cache.hseparation;
			}

			if (p_item->cells[i].expand_right) {
				int plus = 1;
				while (i + plus < columns.size() && !p_item->cells[i + plus].editable && p_item->cells[i + plus].mode == TreeItem::CELL_MODE_STRING && p_item->cells[i + plus].text == "" && p_item->cells[i + plus].icon.is_null()) {
					w += get_column_width(i + plus);
					plus++;
					skip2++;
				}
			}

			int bw = 0;
			for (int j = p_item->cells[i].buttons.size() - 1; j >= 0; j--) {
				Ref<Texture> b = p_item->cells[i].buttons[j].texture;
				Size2 s = b->get_size() + cache.button_pressed->get_minimum_size();

				Point2i o = Point2i(ofs + w - s.width, p_pos.y) - cache.offset + p_draw_ofs;

				if (cache.click_type == Cache::CLICK_BUTTON && cache.click_item == p_item && cache.click_column == i && cache.click_index == j && !p_item->cells[i].buttons[j].disabled) {
					// Being pressed.
					cache.button_pressed->draw(get_canvas_item(), Rect2(o.x, o.y, s.width, MAX(s.height, label_h)));
				}

				o.y += (label_h - s.height) / 2;
				o += cache.button_pressed->get_offset();

				b->draw(ci, o, p_item->cells[i].buttons[j].disabled ? Color(1, 1, 1, 0.5) : p_item->cells[i].buttons[j].color);
				w -= s.width + cache.button_margin;
				bw += s.width + cache.button_margin;
			}

			Rect2i item_rect = Rect2i(Point2i(ofs, p_pos.y) - cache.offset + p_draw_ofs, Size2i(w, label_h));
			Rect2i cell_rect = item_rect;
			if (i != 0) {
				cell_rect.position.x -= cache.hseparation;
				cell_rect.size.x += cache.hseparation;
			}

			if (cache.draw_guides) {
				VisualServer::get_singleton()->canvas_item_add_line(ci, Point2i(cell_rect.position.x, cell_rect.position.y + cell_rect.size.height), cell_rect.position + cell_rect.size, cache.guide_color, 1);
			}

			if (i == 0) {
				if (p_item->cells[0].selected && select_mode == SELECT_ROW) {
					Rect2i row_rect = Rect2i(Point2i(cache.bg->get_margin(MARGIN_LEFT), item_rect.position.y), Size2i(get_size().width - cache.bg->get_minimum_size().width, item_rect.size.y));
					//Rect2 r = Rect2i(row_rect.pos,row_rect.size);
					//r.grow(cache.selected->get_margin(MARGIN_LEFT));
					if (has_focus()) {
						cache.selected_focus->draw(ci, row_rect);
					} else {
						cache.selected->draw(ci, row_rect);
					}
				}
			}

			if ((select_mode == SELECT_ROW && selected_item == p_item) || p_item->cells[i].selected || !p_item->has_meta("__focus_rect")) {
				Rect2i r(cell_rect.position, cell_rect.size);

				if (p_item->cells[i].text.size() > 0) {
					float icon_width = p_item->cells[i].get_icon_size().width;
					if (p_item->get_icon_max_width(i) > 0) {
						icon_width = p_item->get_icon_max_width(i);
					}
					r.position.x += icon_width;
					r.size.x -= icon_width;
				}
				p_item->set_meta("__focus_rect", Rect2(r.position, r.size));

				if (select_mode != SELECT_ROW && p_item->cells[i].selected) {
					if (has_focus()) {
						cache.selected_focus->draw(ci, r);
					} else {
						cache.selected->draw(ci, r);
					}
				}
			}

			if (p_item->cells[i].custom_bg_color) {
				Rect2 r = cell_rect;
				if (i == 0) {
					r.position.x = p_draw_ofs.x;
					r.size.x = w + ofs;
				} else {
					r.position.x -= cache.hseparation;
					r.size.x += cache.hseparation;
				}
				if (p_item->cells[i].custom_bg_outline) {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), p_item->cells[i].bg_color);
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y + r.size.y - 1, r.size.x, 1), p_item->cells[i].bg_color);
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, 1, r.size.y), p_item->cells[i].bg_color);
					VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x + r.size.x - 1, r.position.y, 1, r.size.y), p_item->cells[i].bg_color);
				} else {
					VisualServer::get_singleton()->canvas_item_add_rect(ci, r, p_item->cells[i].bg_color);
				}
			}

			if (drop_mode_flags && drop_mode_over) {
				Rect2 r = cell_rect;
				if (drop_mode_over == p_item) {
					if (drop_mode_section == 0 || drop_mode_section == -1) {
						// Line above.
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), cache.drop_position_color);
					}
					if (drop_mode_section == 0) {
						// Side lines.
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, 1, r.size.y), cache.drop_position_color);
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x + r.size.x - 1, r.position.y, 1, r.size.y), cache.drop_position_color);
					}
					if (drop_mode_section == 0 || (drop_mode_section == 1 && (!p_item->get_children() || p_item->is_collapsed()))) {
						// Line below.
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y + r.size.y, r.size.x, 1), cache.drop_position_color);
					}
				} else if (drop_mode_over == p_item->get_parent()) {
					if (drop_mode_section == 1 && !p_item->get_prev() /* && !drop_mode_over->is_collapsed() */) { // The drop_mode_over shouldn't ever be collapsed in here, otherwise we would be drawing a child of a collapsed item.
						// Line above.
						VisualServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), cache.drop_position_color);
					}
				}
			}

			Color col = p_item->cells[i].custom_color ? p_item->cells[i].color : get_color(p_item->cells[i].selected ? "font_color_selected" : "font_color");
			Color icon_col = p_item->cells[i].icon_color;

			Point2i text_pos = item_rect.position;
			text_pos.y += Math::floor((item_rect.size.y - font->get_height()) / 2) + font_ascent;

			switch (p_item->cells[i].mode) {
				case TreeItem::CELL_MODE_STRING: {
					draw_item_rect(p_item->cells[i], item_rect, col, icon_col);
				} break;
				case TreeItem::CELL_MODE_CHECK: {
					Ref<Texture> checked = cache.checked;
					Ref<Texture> unchecked = cache.unchecked;
					Point2i check_ofs = item_rect.position;
					check_ofs.y += Math::floor((real_t)(item_rect.size.y - checked->get_height()) / 2);

					if (p_item->cells[i].checked) {
						checked->draw(ci, check_ofs);
					} else {
						unchecked->draw(ci, check_ofs);
					}

					int check_w = checked->get_width() + cache.hseparation;

					text_pos.x += check_w;

					item_rect.size.x -= check_w;
					item_rect.position.x += check_w;

					draw_item_rect(p_item->cells[i], item_rect, col, icon_col);

				} break;
				case TreeItem::CELL_MODE_RANGE: {
					if (p_item->cells[i].text != "") {
						if (!p_item->cells[i].editable) {
							break;
						}

						int option = (int)p_item->cells[i].val;

						String s = RTR("(Other)");
						Vector<String> strings = p_item->cells[i].text.split(",");
						for (int j = 0; j < strings.size(); j++) {
							int value = j;
							if (!strings[j].get_slicec(':', 1).empty()) {
								value = strings[j].get_slicec(':', 1).to_int();
							}
							if (option == value) {
								s = strings[j].get_slicec(':', 0);
								break;
							}
						}

						if (p_item->cells[i].suffix != String()) {
							s += " " + p_item->cells[i].suffix;
						}

						Ref<Texture> downarrow = cache.select_arrow;

						font->draw(ci, text_pos, s, col, item_rect.size.x - downarrow->get_width());

						Point2i arrow_pos = item_rect.position;
						arrow_pos.x += item_rect.size.x - downarrow->get_width();
						arrow_pos.y += Math::floor(((item_rect.size.y - downarrow->get_height())) / 2.0);

						downarrow->draw(ci, arrow_pos);
					} else {
						Ref<Texture> updown = cache.updown;

						String valtext = String::num(p_item->cells[i].val, Math::range_step_decimals(p_item->cells[i].step));

						if (p_item->cells[i].suffix != String()) {
							valtext += " " + p_item->cells[i].suffix;
						}

						font->draw(ci, text_pos, valtext, col, item_rect.size.x - updown->get_width());

						if (!p_item->cells[i].editable) {
							break;
						}

						Point2i updown_pos = item_rect.position;
						updown_pos.x += item_rect.size.x - updown->get_width();
						updown_pos.y += Math::floor(((item_rect.size.y - updown->get_height())) / 2.0);

						updown->draw(ci, updown_pos);
					}

				} break;
				case TreeItem::CELL_MODE_ICON: {
					if (p_item->cells[i].icon.is_null()) {
						break;
					}
					Size2i icon_size = p_item->cells[i].get_icon_size();
					if (p_item->cells[i].icon_max_w > 0 && icon_size.width > p_item->cells[i].icon_max_w) {
						icon_size.height = icon_size.height * p_item->cells[i].icon_max_w / icon_size.width;
						icon_size.width = p_item->cells[i].icon_max_w;
					}

					Point2i icon_ofs = (item_rect.size - icon_size) / 2;
					icon_ofs += item_rect.position;

					draw_texture_rect(p_item->cells[i].icon, Rect2(icon_ofs, icon_size), false, icon_col);

				} break;
				case TreeItem::CELL_MODE_CUSTOM: {
					if (p_item->cells[i].custom_draw_obj) {
						Object *cdo = ObjectDB::get_instance(p_item->cells[i].custom_draw_obj);
						if (cdo) {
							cdo->call(p_item->cells[i].custom_draw_callback, p_item, Rect2(item_rect));
						}
					}

					if (!p_item->cells[i].editable) {
						draw_item_rect(p_item->cells[i], item_rect, col, icon_col);
						break;
					}

					Ref<Texture> downarrow = cache.select_arrow;

					Rect2i ir = item_rect;

					Point2i arrow_pos = item_rect.position;
					arrow_pos.x += item_rect.size.x - downarrow->get_width();
					arrow_pos.y += Math::floor(((item_rect.size.y - downarrow->get_height())) / 2.0);
					ir.size.width -= downarrow->get_width();

					if (p_item->cells[i].custom_button) {
						if (cache.hover_item == p_item && cache.hover_cell == i) {
							if (Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT)) {
								draw_style_box(cache.custom_button_pressed, ir);
							} else {
								draw_style_box(cache.custom_button_hover, ir);
								col = cache.custom_button_font_highlight;
							}
						} else {
							draw_style_box(cache.custom_button, ir);
						}
						ir.size -= cache.custom_button->get_minimum_size();
						ir.position += cache.custom_button->get_offset();
					}

					draw_item_rect(p_item->cells[i], ir, col, icon_col);

					downarrow->draw(ci, arrow_pos);

				} break;
			}

			if (i == 0) {
				ofs = get_column_width(0);
			} else {
				ofs += w + bw;
			}

			if (select_mode == SELECT_MULTI && selected_item == p_item && selected_col == i) {
				if (has_focus()) {
					cache.cursor->draw(ci, cell_rect);
				} else {
					cache.cursor_unfocus->draw(ci, cell_rect);
				}
			}
		}

		if (!p_item->disable_folding && !hide_folding && p_item->children) { //has children, draw the guide box

			Ref<Texture> arrow;

			if (p_item->collapsed) {
				arrow = cache.arrow_collapsed;
			} else {
				arrow = cache.arrow;
			}

			Point2 apos = p_pos + Point2i(0, (label_h - arrow->get_height()) / 2) - cache.offset + p_draw_ofs;
			apos.x += cache.item_margin - arrow->get_width();

			arrow->draw(ci, apos);
		}
	}

	Point2 children_pos = p_pos;

	if (!skip) {
		children_pos.x += cache.item_margin;
		htotal += label_h;
		children_pos.y += htotal;
	}

	if (!p_item->collapsed) { /* if not collapsed, check the children */
		TreeItem *c = p_item->children;

		int prev_ofs = children_pos.y - cache.offset.y + p_draw_ofs.y;

		while (c) {
			int child_h = -1;
			if (htotal >= 0) {
				child_h = draw_item(children_pos, p_draw_ofs, p_draw_size, c);
			}

			// Draw relationship lines.
			if (cache.draw_relationship_lines > 0 && (!hide_root || c->parent != root)) {
				int root_ofs = children_pos.x + ((p_item->disable_folding || hide_folding) ? cache.hseparation : cache.item_margin);
				int parent_ofs = p_pos.x + ((p_item->disable_folding || hide_folding) ? cache.hseparation : cache.item_margin);
				Point2i root_pos = Point2i(root_ofs, children_pos.y + label_h / 2) - cache.offset + p_draw_ofs;

				if (c->get_children() != nullptr) {
					root_pos -= Point2i(cache.arrow->get_width(), 0);
				}

				float line_width = 1.0;
#ifdef TOOLS_ENABLED
				line_width *= EDSCALE;
#endif

				Point2i parent_pos = Point2i(parent_ofs - cache.arrow->get_width() / 2, p_pos.y + label_h / 2 + cache.arrow->get_height() / 2) - cache.offset + p_draw_ofs;

				if (root_pos.y + line_width >= 0) {
					// Order of parts on this bend: the horizontal line first, then the vertical line.
					if (htotal >= 0) {
						VisualServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x - Math::floor(line_width / 2), root_pos.y), cache.relationship_line_color, line_width);
					}
					VisualServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y), Point2i(parent_pos.x, prev_ofs), cache.relationship_line_color, line_width);
				}

				prev_ofs = root_pos.y;
			}

			if (child_h < 0) {
				if (htotal == -1) {
					break; // Last loop done, stop.
				}

				if (cache.draw_relationship_lines == 0) {
					return -1; // No need to draw anymore, full stop.
				}

				htotal = -1;
				children_pos.y = cache.offset.y + p_draw_size.height;
			} else {
				htotal += child_h;
				children_pos.y += child_h;
			}

			c = c->next;
		}
	}

	return htotal;
}

int Tree::_count_selected_items(TreeItem *p_from) const {
	int count = 0;
	for (int i = 0; i < columns.size(); i++) {
		if (p_from->is_selected(i)) {
			count++;
		}
	}

	if (p_from->get_children()) {
		count += _count_selected_items(p_from->get_children());
	}

	if (p_from->get_next()) {
		count += _count_selected_items(p_from->get_next());
	}

	return count;
}
void Tree::select_single_item(TreeItem *p_selected, TreeItem *p_current, int p_col, TreeItem *p_prev, bool *r_in_range, bool p_force_deselect) {
	TreeItem::Cell &selected_cell = p_selected->cells.write[p_col];

	bool switched = false;
	if (r_in_range && !*r_in_range && (p_current == p_selected || p_current == p_prev)) {
		*r_in_range = true;
		switched = true;
	}

	bool emitted_row = false;

	for (int i = 0; i < columns.size(); i++) {
		TreeItem::Cell &c = p_current->cells.write[i];

		if (!c.selectable) {
			continue;
		}

		if (select_mode == SELECT_ROW) {
			if (p_selected == p_current && (!c.selected || allow_reselect)) {
				c.selected = true;
				selected_item = p_selected;
				selected_col = 0;
				if (!emitted_row) {
					emit_signal("item_selected");
					emitted_row = true;
				}
				/*
					if (p_col==i)
						p_current->selected_signal.call(p_col);
					*/

			} else if (c.selected) {
				if (p_selected != p_current) {
					// Deselect other rows.
					c.selected = false;
				}
			}
		} else if (select_mode == SELECT_SINGLE || select_mode == SELECT_MULTI) {
			if (!r_in_range && &selected_cell == &c) {
				if (!selected_cell.selected || allow_reselect) {
					selected_cell.selected = true;

					selected_item = p_selected;
					selected_col = i;

					emit_signal("cell_selected");
					if (select_mode == SELECT_MULTI) {
						emit_signal("multi_selected", p_current, i, true);
					} else if (select_mode == SELECT_SINGLE) {
						emit_signal("item_selected");
					}

				} else if (select_mode == SELECT_MULTI && (selected_item != p_selected || selected_col != i)) {
					selected_item = p_selected;
					selected_col = i;
					emit_signal("cell_selected");
				}
			} else {
				if (r_in_range && *r_in_range && !p_force_deselect) {
					if (!c.selected && c.selectable) {
						c.selected = true;
						emit_signal("multi_selected", p_current, i, true);
					}

				} else if (!r_in_range || p_force_deselect) {
					if (select_mode == SELECT_MULTI && c.selected) {
						emit_signal("multi_selected", p_current, i, false);
					}
					c.selected = false;
				}
				//p_current->deselected_signal.call(p_col);
			}
		}
	}

	if (!switched && r_in_range && *r_in_range && (p_current == p_selected || p_current == p_prev)) {
		*r_in_range = false;
	}

	TreeItem *c = p_current->children;

	while (c) {
		select_single_item(p_selected, c, p_col, p_prev, r_in_range, p_current->is_collapsed() || p_force_deselect);
		c = c->next;
	}
}

Rect2 Tree::search_item_rect(TreeItem *p_from, TreeItem *p_item) {
	return Rect2();
}

void Tree::_range_click_timeout() {
	if (range_item_last && !range_drag_enabled && Input::get_singleton()->is_mouse_button_pressed(BUTTON_LEFT)) {
		Point2 pos = get_local_mouse_position() - cache.bg->get_offset();
		if (show_column_titles) {
			pos.y -= _get_title_button_height();

			if (pos.y < 0) {
				range_click_timer->stop();
				return;
			}
		}

		if (!root) {
			return;
		}

		click_handled = false;
		Ref<InputEventMouseButton> mb;
		mb.instance();
		;

		propagate_mouse_activated = false; // done from outside, so signal handler can't clear the tree in the middle of emit (which is a common case)
		blocked++;
		propagate_mouse_event(pos + cache.offset, 0, 0, false, root, BUTTON_LEFT, mb);
		blocked--;

		if (range_click_timer->is_one_shot()) {
			range_click_timer->set_wait_time(0.05);
			range_click_timer->set_one_shot(false);
			range_click_timer->start();
		}

		if (!click_handled) {
			range_click_timer->stop();
		}

		if (propagate_mouse_activated) {
			emit_signal("item_activated");
			propagate_mouse_activated = false;
		}

	} else {
		range_click_timer->stop();
	}
}

int Tree::propagate_mouse_event(const Point2i &p_pos, int x_ofs, int y_ofs, bool p_doubleclick, TreeItem *p_item, int p_button, const Ref<InputEventWithModifiers> &p_mod) {
	int item_h = compute_item_height(p_item) + cache.vseparation;

	bool skip = (p_item == root && hide_root);

	if (!skip && p_pos.y < item_h) {
		// check event!

		if (range_click_timer->get_time_left() > 0 && p_item != range_item_last) {
			return -1;
		}

		if (!p_item->disable_folding && !hide_folding && (p_pos.x >= x_ofs && p_pos.x < (x_ofs + cache.item_margin))) {
			if (p_item->children) {
				p_item->set_collapsed(!p_item->is_collapsed());
			}

			return -1; //handled!
		}

		int x = p_pos.x;
		/* find clicked column */
		int col = -1;
		int col_ofs = 0;
		int col_width = 0;
		for (int i = 0; i < columns.size(); i++) {
			col_width = get_column_width(i);

			if (p_item->cells[i].expand_right) {
				int plus = 1;
				while (i + plus < columns.size() && !p_item->cells[i + plus].editable && p_item->cells[i + plus].mode == TreeItem::CELL_MODE_STRING && p_item->cells[i + plus].text == "" && p_item->cells[i + plus].icon.is_null()) {
					col_width += cache.hseparation;
					col_width += get_column_width(i + plus);
					plus++;
				}
			}

			if (x > col_width) {
				col_ofs += col_width;
				x -= col_width;
				continue;
			}

			col = i;
			break;
		}

		if (col == -1) {
			return -1;
		} else if (col == 0) {
			int margin = x_ofs + cache.item_margin; //-cache.hseparation;
			//int lm = cache.bg->get_margin(MARGIN_LEFT);
			col_width -= margin;
			col_ofs += margin;
			x -= margin;
		} else {
			col_width -= cache.hseparation;
			x -= cache.hseparation;
		}

		if (!p_item->disable_folding && !hide_folding && !p_item->cells[col].editable && !p_item->cells[col].selectable && p_item->get_children()) {
			p_item->set_collapsed(!p_item->is_collapsed());
			return -1; //collapse/uncollapse because nothing can be done with item
		}

		const TreeItem::Cell &c = p_item->cells[col];

		bool already_selected = c.selected;
		bool already_cursor = (p_item == selected_item) && col == selected_col;

		for (int j = c.buttons.size() - 1; j >= 0; j--) {
			Ref<Texture> b = c.buttons[j].texture;
			int w = b->get_size().width + cache.button_pressed->get_minimum_size().width;

			if (x > col_width - w) {
				if (c.buttons[j].disabled) {
					pressed_button = -1;
					cache.click_type = Cache::CLICK_NONE;
					return -1;
				}
				pressed_button = j;
				cache.click_type = Cache::CLICK_BUTTON;
				cache.click_index = j;
				cache.click_id = c.buttons[j].id;
				cache.click_item = p_item;
				cache.click_column = col;
				cache.click_pos = get_local_mouse_position();
				update();
				//emit_signal("button_pressed");
				return -1;
			}
			col_width -= w + cache.button_margin;
		}

		if (p_button == BUTTON_LEFT || (p_button == BUTTON_RIGHT && allow_rmb_select)) {
			/* process selection */

			if (p_doubleclick && (!c.editable || c.mode == TreeItem::CELL_MODE_CUSTOM || c.mode == TreeItem::CELL_MODE_ICON /*|| c.mode==TreeItem::CELL_MODE_CHECK*/)) { //it's confusing for check

				propagate_mouse_activated = true;

				incr_search.clear();
				return -1;
			}

			if (select_mode == SELECT_MULTI && p_mod->get_command() && c.selectable) {
				if (!c.selected || p_button == BUTTON_RIGHT) {
					p_item->select(col);
					emit_signal("multi_selected", p_item, col, true);
					if (p_button == BUTTON_RIGHT) {
						emit_signal("item_rmb_selected", get_local_mouse_position());
					}

					//p_item->selected_signal.call(col);
				} else {
					p_item->deselect(col);
					emit_signal("multi_selected", p_item, col, false);
					//p_item->deselected_signal.call(col);
				}

			} else {
				if (c.selectable) {
					if (select_mode == SELECT_MULTI && p_mod->get_shift() && selected_item && selected_item != p_item) {
						bool inrange = false;

						select_single_item(p_item, root, col, selected_item, &inrange);
						if (p_button == BUTTON_RIGHT) {
							emit_signal("item_rmb_selected", get_local_mouse_position());
						}
					} else {
						int icount = _count_selected_items(root);

						if (select_mode == SELECT_MULTI && icount > 1 && p_button != BUTTON_RIGHT) {
							single_select_defer = p_item;
							single_select_defer_column = col;
						} else {
							if (p_button != BUTTON_RIGHT || !c.selected) {
								select_single_item(p_item, root, col);
							}

							if (p_button == BUTTON_RIGHT) {
								emit_signal("item_rmb_selected", get_local_mouse_position());
							}
						}
					}

					/*
					if (!c.selected && select_mode==SELECT_MULTI) {
						emit_signal("multi_selected",p_item,col,true);
					}
					*/
					update();
				}
			}
		}

		if (!c.editable) {
			return -1; // if cell is not editable, don't bother
		}

		/* editing */

		bool bring_up_editor = allow_reselect ? (c.selected && already_selected) : c.selected;
		String editor_text = c.text;

		switch (c.mode) {
			case TreeItem::CELL_MODE_STRING: {
				//nothing in particular

				if (select_mode == SELECT_MULTI && (get_tree()->get_event_count() == focus_in_id || !already_cursor)) {
					bring_up_editor = false;
				}

			} break;
			case TreeItem::CELL_MODE_CHECK: {
				bring_up_editor = false; //checkboxes are not edited with editor
				if (force_edit_checkbox_only_on_checkbox) {
					if (x < cache.checked->get_width()) {
						p_item->set_checked(col, !c.checked);
						item_edited(col, p_item);
					}
				} else {
					p_item->set_checked(col, !c.checked);
					item_edited(col, p_item);
				}
				click_handled = true;
				//p_item->edited_signal.call(col);

			} break;
			case TreeItem::CELL_MODE_RANGE: {
				if (c.text != "") {
					//if (x >= (get_column_width(col)-item_h/2)) {

					popup_menu->clear();
					for (int i = 0; i < c.text.get_slice_count(","); i++) {
						String s = c.text.get_slicec(',', i);
						popup_menu->add_item(s.get_slicec(':', 0), s.get_slicec(':', 1).empty() ? i : s.get_slicec(':', 1).to_int());
					}

					popup_menu->set_size(Size2(col_width, 0));
					popup_menu->set_position(get_global_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs + item_h) - cache.offset);
					popup_menu->popup();
					popup_edited_item = p_item;
					popup_edited_item_col = col;
					//}
					bring_up_editor = false;
				} else {
					if (x >= (col_width - item_h / 2)) {
						/* touching the combo */
						bool up = p_pos.y < (item_h / 2);

						if (p_button == BUTTON_LEFT) {
							if (range_click_timer->get_time_left() == 0) {
								range_item_last = p_item;
								range_up_last = up;

								range_click_timer->set_wait_time(0.6);
								range_click_timer->set_one_shot(true);
								range_click_timer->start();

							} else if (up != range_up_last) {
								return -1; // break. avoid changing direction on mouse held
							}

							p_item->set_range(col, c.val + (up ? 1.0 : -1.0) * c.step);

							item_edited(col, p_item);

						} else if (p_button == BUTTON_RIGHT) {
							p_item->set_range(col, (up ? c.max : c.min));
							item_edited(col, p_item);
						} else if (p_button == BUTTON_WHEEL_UP) {
							p_item->set_range(col, c.val + c.step);
							item_edited(col, p_item);
						} else if (p_button == BUTTON_WHEEL_DOWN) {
							p_item->set_range(col, c.val - c.step);
							item_edited(col, p_item);
						}

						//p_item->edited_signal.call(col);
						bring_up_editor = false;

					} else {
						editor_text = String::num(p_item->cells[col].val, Math::range_step_decimals(p_item->cells[col].step));
						if (select_mode == SELECT_MULTI && get_tree()->get_event_count() == focus_in_id) {
							bring_up_editor = false;
						}
					}
				}
				click_handled = true;

			} break;
			case TreeItem::CELL_MODE_ICON: {
				bring_up_editor = false;
			} break;
			case TreeItem::CELL_MODE_CUSTOM: {
				edited_item = p_item;
				edited_col = col;
				bool on_arrow = x > col_width - cache.select_arrow->get_width();

				custom_popup_rect = Rect2i(get_global_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs + item_h - cache.offset.y), Size2(get_column_width(col), item_h));

				if (on_arrow || !p_item->cells[col].custom_button) {
					emit_signal("custom_popup_edited", ((bool)(x >= (col_width - item_h / 2))));
				}

				if (!p_item->cells[col].custom_button || !on_arrow) {
					item_edited(col, p_item, p_button == BUTTON_LEFT);
				}
				click_handled = true;
				return -1;
			} break;
		};

		if (!bring_up_editor || p_button != BUTTON_LEFT) {
			return -1;
		}

		click_handled = true;
		popup_edited_item = p_item;
		popup_edited_item_col = col;

		pressing_item_rect = Rect2(get_global_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs) - cache.offset, Size2(col_width, item_h));
		pressing_for_editor_text = editor_text;
		pressing_for_editor = true;

		return -1; //select
	} else {
		Point2i new_pos = p_pos;

		if (!skip) {
			x_ofs += cache.item_margin;
			//new_pos.x-=cache.item_margin;
			y_ofs += item_h;
			new_pos.y -= item_h;
		}

		if (!p_item->collapsed) { /* if not collapsed, check the children */

			TreeItem *c = p_item->children;

			while (c) {
				int child_h = propagate_mouse_event(new_pos, x_ofs, y_ofs, p_doubleclick, c, p_button, p_mod);

				if (child_h < 0) {
					return -1; // break, stop propagating, no need to anymore
				}

				new_pos.y -= child_h;
				y_ofs += child_h;
				c = c->next;
				item_h += child_h;
			}
		}
		if (p_item == root && p_button == BUTTON_RIGHT) {
			emit_signal("empty_rmb", get_local_mouse_position());
		}
	}

	return item_h; // nothing found
}

void Tree::_text_editor_modal_close() {
	if (Input::get_singleton()->is_key_pressed(KEY_ESCAPE) ||
			Input::get_singleton()->is_key_pressed(KEY_KP_ENTER) ||
			Input::get_singleton()->is_key_pressed(KEY_ENTER)) {
		return;
	}

	if (value_editor->has_point(value_editor->get_local_mouse_position())) {
		return;
	}

	text_editor_enter(text_editor->get_text());
}

void Tree::text_editor_enter(String p_text) {
	text_editor->hide();
	value_editor->hide();

	if (!popup_edited_item) {
		return;
	}

	if (popup_edited_item_col < 0 || popup_edited_item_col > columns.size()) {
		return;
	}

	TreeItem::Cell &c = popup_edited_item->cells.write[popup_edited_item_col];
	switch (c.mode) {
		case TreeItem::CELL_MODE_STRING: {
			c.text = p_text;
			//popup_edited_item->edited_signal.call( popup_edited_item_col );
		} break;
		case TreeItem::CELL_MODE_RANGE: {
			c.val = p_text.to_double();
			if (c.step > 0) {
				c.val = Math::stepify(c.val, c.step);
			}
			if (c.val < c.min) {
				c.val = c.min;
			} else if (c.val > c.max) {
				c.val = c.max;
			}

			//popup_edited_item->edited_signal.call( popup_edited_item_col );
		} break;
		default: {
			ERR_FAIL();
		}
	}

	item_edited(popup_edited_item_col, popup_edited_item);
	update();
}

void Tree::value_editor_changed(double p_value) {
	if (updating_value_editor) {
		return;
	}
	if (!popup_edited_item) {
		return;
	}

	TreeItem::Cell &c = popup_edited_item->cells.write[popup_edited_item_col];
	c.val = p_value;
	item_edited(popup_edited_item_col, popup_edited_item);
	update();
}

void Tree::popup_select(int p_option) {
	if (!popup_edited_item) {
		return;
	}

	if (popup_edited_item_col < 0 || popup_edited_item_col > columns.size()) {
		return;
	}

	popup_edited_item->cells.write[popup_edited_item_col].val = p_option;
	//popup_edited_item->edited_signal.call( popup_edited_item_col );
	update();
	item_edited(popup_edited_item_col, popup_edited_item);
}

void Tree::_go_left() {
	if (selected_col == 0) {
		if (selected_item->get_children() != nullptr && !selected_item->is_collapsed()) {
			selected_item->set_collapsed(true);
		} else {
			if (columns.size() == 1) { // goto parent with one column
				TreeItem *parent = selected_item->get_parent();
				if (selected_item != get_root() && parent && parent->is_selectable(selected_col) && !(hide_root && parent == get_root())) {
					select_single_item(parent, get_root(), selected_col);
				}
			} else if (selected_item->get_prev_visible()) {
				selected_col = columns.size() - 1;
				_go_up(); // go to upper column if possible
			}
		}
	} else {
		if (select_mode == SELECT_MULTI) {
			selected_col--;
			emit_signal("cell_selected");
		} else {
			selected_item->select(selected_col - 1);
		}
	}
	update();
	accept_event();
	ensure_cursor_is_visible();
}

void Tree::_go_right() {
	if (selected_col == (columns.size() - 1)) {
		if (selected_item->get_children() != nullptr && selected_item->is_collapsed()) {
			selected_item->set_collapsed(false);
		} else if (selected_item->get_next_visible()) {
			selected_col = 0;
			_go_down();
		}
	} else {
		if (select_mode == SELECT_MULTI) {
			selected_col++;
			emit_signal("cell_selected");
		} else {
			selected_item->select(selected_col + 1);
		}
	}
	update();
	ensure_cursor_is_visible();
	accept_event();
}

void Tree::_go_up() {
	TreeItem *prev = nullptr;
	if (!selected_item) {
		prev = get_last_item();
		selected_col = 0;
	} else {
		prev = selected_item->get_prev_visible();
		if (last_keypress != 0) {
			//incr search next
			int col;
			prev = _search_item_text(prev, incr_search, &col, true, true);
			if (!prev) {
				accept_event();
				return;
			}
		}
	}

	if (select_mode == SELECT_MULTI) {
		if (!prev) {
			return;
		}
		selected_item = prev;
		emit_signal("cell_selected");
		update();
	} else {
		int col = selected_col < 0 ? 0 : selected_col;
		while (prev && !prev->cells[col].selectable) {
			prev = prev->get_prev_visible();
		}
		if (!prev) {
			return; // do nothing..
		}
		prev->select(col);
	}

	ensure_cursor_is_visible();
	accept_event();
}

void Tree::_go_down() {
	TreeItem *next = nullptr;
	if (!selected_item) {
		if (root) {
			next = hide_root ? root->get_next_visible() : root;
		}
	} else {
		next = selected_item->get_next_visible();

		if (last_keypress != 0) {
			//incr search next
			int col;
			next = _search_item_text(next, incr_search, &col, true);
			if (!next) {
				accept_event();
				return;
			}
		}
	}

	if (select_mode == SELECT_MULTI) {
		if (!next) {
			return;
		}

		selected_item = next;
		emit_signal("cell_selected");
		update();
	} else {
		int col = selected_col < 0 ? 0 : selected_col;

		while (next && !next->cells[col].selectable) {
			next = next->get_next_visible();
		}
		if (!next) {
			return; // do nothing..
		}
		next->select(col);
	}

	ensure_cursor_is_visible();
	accept_event();
}

void Tree::_gui_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;

	bool is_command = k.is_valid() && k->get_command();
	if (p_event->is_action("ui_right") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		if (!selected_item || select_mode == SELECT_ROW || selected_col > (columns.size() - 1)) {
			return;
		}
		if (k.is_valid() && k->get_alt()) {
			selected_item->set_collapsed(false);
			TreeItem *next = selected_item->get_children();
			while (next && next != selected_item->next) {
				next->set_collapsed(false);
				next = next->get_next_visible();
			}
		} else {
			_go_right();
		}
	} else if (p_event->is_action("ui_left") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		if (!selected_item || select_mode == SELECT_ROW || selected_col < 0) {
			return;
		}

		if (k.is_valid() && k->get_alt()) {
			selected_item->set_collapsed(true);
			TreeItem *next = selected_item->get_children();
			while (next && next != selected_item->next) {
				next->set_collapsed(true);
				next = next->get_next_visible();
			}
		} else {
			_go_left();
		}

	} else if (p_event->is_action("ui_up") && p_event->is_pressed() && !is_command) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		_go_up();

	} else if (p_event->is_action("ui_down") && p_event->is_pressed() && !is_command) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		_go_down();

	} else if (p_event->is_action("ui_page_down") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		TreeItem *next = nullptr;
		if (!selected_item) {
			return;
		}
		next = selected_item;

		for (int i = 0; i < 10; i++) {
			TreeItem *_n = next->get_next_visible();
			if (_n) {
				next = _n;
			} else {
				break;
			}
		}
		if (next == selected_item) {
			return;
		}

		if (select_mode == SELECT_MULTI) {
			selected_item = next;
			emit_signal("cell_selected");
			update();
		} else {
			while (next && !next->cells[selected_col].selectable) {
				next = next->get_next_visible();
			}
			if (!next) {
				return; // do nothing..
			}
			next->select(selected_col);
		}

		ensure_cursor_is_visible();
	} else if (p_event->is_action("ui_page_up") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		TreeItem *prev = nullptr;
		if (!selected_item) {
			return;
		}
		prev = selected_item;

		for (int i = 0; i < 10; i++) {
			TreeItem *_n = prev->get_prev_visible();
			if (_n) {
				prev = _n;
			} else {
				break;
			}
		}
		if (prev == selected_item) {
			return;
		}

		if (select_mode == SELECT_MULTI) {
			selected_item = prev;
			emit_signal("cell_selected");
			update();
		} else {
			while (prev && !prev->cells[selected_col].selectable) {
				prev = prev->get_prev_visible();
			}
			if (!prev) {
				return; // do nothing..
			}
			prev->select(selected_col);
		}
		ensure_cursor_is_visible();
	} else if (p_event->is_action("ui_accept") && p_event->is_pressed()) {
		if (selected_item) {
			//bring up editor if possible
			if (!edit_selected()) {
				emit_signal("item_activated");
				incr_search.clear();
			}
		}
		accept_event();
	} else if (p_event->is_action("ui_select") && p_event->is_pressed()) {
		if (select_mode == SELECT_MULTI) {
			if (!selected_item) {
				return;
			}
			if (selected_item->is_selected(selected_col)) {
				selected_item->deselect(selected_col);
				emit_signal("multi_selected", selected_item, selected_col, false);
			} else if (selected_item->is_selectable(selected_col)) {
				selected_item->select(selected_col);
				emit_signal("multi_selected", selected_item, selected_col, true);
			}
		}
		accept_event();
	}

	if (allow_search && k.is_valid()) { // Incremental search

		if (!k->is_pressed()) {
			return;
		}
		if (k->get_command() || (k->get_shift() && k->get_unicode() == 0) || k->get_metakey()) {
			return;
		}
		if (!root) {
			return;
		}

		if (hide_root && !root->get_next_visible()) {
			return;
		}

		if (k->get_unicode() > 0) {
			_do_incr_search(String::chr(k->get_unicode()));
			accept_event();

			return;
		} else {
			if (k->get_scancode() != KEY_SHIFT) {
				last_keypress = 0;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (cache.font.is_null()) { // avoid a strange case that may corrupt stuff
			update_cache();
		}

		Ref<StyleBox> bg = cache.bg;

		Point2 pos = mm->get_position() - bg->get_offset();

		Cache::ClickType old_hover = cache.hover_type;
		int old_index = cache.hover_index;

		cache.hover_type = Cache::CLICK_NONE;
		cache.hover_index = 0;
		if (show_column_titles) {
			pos.y -= _get_title_button_height();
			if (pos.y < 0) {
				pos.x += cache.offset.x;
				int len = 0;
				for (int i = 0; i < columns.size(); i++) {
					len += get_column_width(i);
					if (pos.x < len) {
						cache.hover_type = Cache::CLICK_TITLE;
						cache.hover_index = i;
						break;
					}
				}
			}
		}

		if (root) {
			Point2 mpos = mm->get_position();
			mpos -= cache.bg->get_offset();
			mpos.y -= _get_title_button_height();
			if (mpos.y >= 0) {
				if (h_scroll->is_visible_in_tree()) {
					mpos.x += h_scroll->get_value();
				}
				if (v_scroll->is_visible_in_tree()) {
					mpos.y += v_scroll->get_value();
				}

				TreeItem *old_it = cache.hover_item;
				int old_col = cache.hover_cell;

				int col = 0, h = 0, section = 0;
				TreeItem *it = _find_item_at_pos(root, mpos, col, h, section);

				if (drop_mode_flags) {
					if (it != drop_mode_over) {
						drop_mode_over = it;
						update();
					}
					if (it && section != drop_mode_section) {
						drop_mode_section = section;
						update();
					}
				}

				cache.hover_item = it;
				cache.hover_cell = col;

				if (it != old_it || col != old_col) {
					if (old_it && old_col >= old_it->cells.size()) {
						// Columns may have changed since last update().
						update();
					} else {
						// Only need to update if mouse enters/exits a button
						bool was_over_button = old_it && old_it->cells[old_col].custom_button;
						bool is_over_button = it && it->cells[col].custom_button;
						if (was_over_button || is_over_button) {
							update();
						}
					}
				}
			}
		}

		// Update if mouse enters/exits columns
		if (cache.hover_type != old_hover || cache.hover_index != old_index) {
			update();
		}

		if (pressing_for_editor && popup_edited_item && (popup_edited_item->get_cell_mode(popup_edited_item_col) == TreeItem::CELL_MODE_RANGE)) {
			//range drag

			if (!range_drag_enabled) {
				Vector2 cpos = mm->get_position();
				if (cpos.distance_to(pressing_pos) > 2) {
					range_drag_enabled = true;
					range_drag_capture_pos = cpos;
					range_drag_base = popup_edited_item->get_range(popup_edited_item_col);
					Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
				}
			} else {
				const TreeItem::Cell &c = popup_edited_item->cells[popup_edited_item_col];
				float diff_y = -mm->get_relative().y;
				diff_y = Math::pow(ABS(diff_y), 1.8f) * SGN(diff_y);
				diff_y *= 0.1;
				range_drag_base = CLAMP(range_drag_base + c.step * diff_y, c.min, c.max);
				popup_edited_item->set_range(popup_edited_item_col, range_drag_base);
				item_edited(popup_edited_item_col, popup_edited_item);
			}
		}

		if (drag_touching && !drag_touching_deaccel) {
			drag_accum -= mm->get_relative().y;
			v_scroll->set_value(drag_from + drag_accum);
			drag_speed = -mm->get_speed().y;
		}
	}

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		if (cache.font.is_null()) { // avoid a strange case that may corrupt stuff
			update_cache();
		}

		if (!b->is_pressed()) {
			if (b->get_button_index() == BUTTON_LEFT) {
				Point2 pos = b->get_position() - cache.bg->get_offset();
				if (show_column_titles) {
					pos.y -= _get_title_button_height();

					if (pos.y < 0) {
						pos.x += cache.offset.x;
						int len = 0;
						for (int i = 0; i < columns.size(); i++) {
							len += get_column_width(i);
							if (pos.x < len) {
								emit_signal("column_title_pressed", i);
								break;
							}
						}
					}
				}

				if (single_select_defer) {
					select_single_item(single_select_defer, root, single_select_defer_column);
					single_select_defer = nullptr;
				}

				range_click_timer->stop();

				if (pressing_for_editor) {
					if (range_drag_enabled) {
						range_drag_enabled = false;
						Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
						warp_mouse(range_drag_capture_pos);
					} else {
						Rect2 rect = get_selected()->get_meta("__focus_rect");
						if (rect.has_point(Point2(b->get_position().x, b->get_position().y))) {
							if (!edit_selected()) {
								emit_signal("item_double_clicked");
							}
						} else {
							emit_signal("item_double_clicked");
						}
					}
					pressing_for_editor = false;
				}

				if (cache.click_type == Cache::CLICK_BUTTON && cache.click_item != nullptr) {
					// make sure in case of wrong reference after reconstructing whole TreeItems
					cache.click_item = get_item_at_position(cache.click_pos);
					emit_signal("button_pressed", cache.click_item, cache.click_column, cache.click_id);
				}
				cache.click_type = Cache::CLICK_NONE;
				cache.click_index = -1;
				cache.click_id = -1;
				cache.click_item = nullptr;
				cache.click_column = 0;

				if (drag_touching) {
					if (drag_speed == 0) {
						drag_touching_deaccel = false;
						drag_touching = false;
						set_physics_process_internal(false);
					} else {
						drag_touching_deaccel = true;
					}
				}
				update();
			}
			return;
		}

		if (range_drag_enabled) {
			return;
		}

		switch (b->get_button_index()) {
			case BUTTON_RIGHT:
			case BUTTON_LEFT: {
				Ref<StyleBox> bg = cache.bg;

				Point2 pos = b->get_position() - bg->get_offset();
				cache.click_type = Cache::CLICK_NONE;
				if (show_column_titles) {
					pos.y -= _get_title_button_height();

					if (pos.y < 0) {
						if (b->get_button_index() == BUTTON_LEFT) {
							pos.x += cache.offset.x;
							int len = 0;
							for (int i = 0; i < columns.size(); i++) {
								len += get_column_width(i);
								if (pos.x < len) {
									cache.click_type = Cache::CLICK_TITLE;
									cache.click_index = i;
									//cache.click_id=;
									update();
									break;
								}
							}
						}
						break;
					}
				}
				if (!root || (!root->get_children() && hide_root)) {
					if (b->get_button_index() == BUTTON_RIGHT && allow_rmb_select) {
						emit_signal("empty_tree_rmb_selected", get_local_mouse_position());
					}
					break;
				}

				click_handled = false;
				pressing_for_editor = false;
				propagate_mouse_activated = false;

				blocked++;
				propagate_mouse_event(pos + cache.offset, 0, 0, b->is_doubleclick(), root, b->get_button_index(), b);
				blocked--;

				if (pressing_for_editor) {
					pressing_pos = b->get_position();
				}

				if (b->get_button_index() == BUTTON_RIGHT) {
					break;
				}

				if (drag_touching) {
					set_physics_process_internal(false);
					drag_touching_deaccel = false;
					drag_touching = false;
					drag_speed = 0;
					drag_from = 0;
				}

				if (!click_handled) {
					drag_speed = 0;
					drag_accum = 0;
					//last_drag_accum=0;
					drag_from = v_scroll->get_value();
					drag_touching = OS::get_singleton()->has_touchscreen_ui_hint();
					drag_touching_deaccel = false;
					if (drag_touching) {
						set_physics_process_internal(true);
					}

					if (b->get_button_index() == BUTTON_LEFT) {
						if (get_item_at_position(b->get_position()) == nullptr && !b->get_shift() && !b->get_control() && !b->get_command()) {
							emit_signal("nothing_selected");
						}
					}
				}

				if (propagate_mouse_activated) {
					emit_signal("item_activated");
					propagate_mouse_activated = false;
				}

			} break;
			case BUTTON_WHEEL_UP: {
				double prev_value = v_scroll->get_value();
				v_scroll->set_value(v_scroll->get_value() - v_scroll->get_page() * b->get_factor() / 8);
				if (v_scroll->get_value() != prev_value) {
					accept_event();
				}

			} break;
			case BUTTON_WHEEL_DOWN: {
				double prev_value = v_scroll->get_value();
				v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * b->get_factor() / 8);
				if (v_scroll->get_value() != prev_value) {
					accept_event();
				}

			} break;
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		double prev_v = v_scroll->get_value();
		v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);

		double prev_h = h_scroll->get_value();
		h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);

		if (v_scroll->get_value() != prev_v || h_scroll->get_value() != prev_h) {
			accept_event();
		}
	}
}

bool Tree::edit_selected() {
	TreeItem *s = get_selected();
	ERR_FAIL_COND_V_MSG(!s, false, "No item selected.");
	ensure_cursor_is_visible();
	int col = get_selected_column();
	ERR_FAIL_INDEX_V_MSG(col, columns.size(), false, "No item column selected.");

	if (!s->cells[col].editable) {
		return false;
	}

	Rect2 rect = s->get_meta("__focus_rect");
	popup_edited_item = s;
	popup_edited_item_col = col;

	const TreeItem::Cell &c = s->cells[col];

	if (c.mode == TreeItem::CELL_MODE_CHECK) {
		s->set_checked(col, !c.checked);
		item_edited(col, s);
		return true;
	} else if (c.mode == TreeItem::CELL_MODE_CUSTOM) {
		edited_item = s;
		edited_col = col;
		custom_popup_rect = Rect2i(get_global_position() + rect.position, rect.size);
		emit_signal("custom_popup_edited", false);
		item_edited(col, s);

		return true;
	} else if (c.mode == TreeItem::CELL_MODE_RANGE && c.text != "") {
		popup_menu->clear();
		for (int i = 0; i < c.text.get_slice_count(","); i++) {
			String s2 = c.text.get_slicec(',', i);
			popup_menu->add_item(s2.get_slicec(':', 0), s2.get_slicec(':', 1).empty() ? i : s2.get_slicec(':', 1).to_int());
		}

		popup_menu->set_size(Size2(rect.size.width, 0));
		popup_menu->set_position(get_global_position() + rect.position + Point2i(0, rect.size.height));
		popup_menu->popup();
		popup_edited_item = s;
		popup_edited_item_col = col;
		return true;

	} else if (c.mode == TreeItem::CELL_MODE_STRING || c.mode == TreeItem::CELL_MODE_RANGE) {
		Vector2 ofs(0, (text_editor->get_size().height - rect.size.height) / 2 - 1); // "-1" centers vertically.
		Point2i textedpos = get_global_position() + rect.position - ofs;
		cache.text_editor_position = textedpos;
		text_editor->set_position(textedpos);
		text_editor->set_size(rect.size);
		text_editor->clear();
		text_editor->set_text(c.mode == TreeItem::CELL_MODE_STRING ? c.text : String::num(c.val, Math::range_step_decimals(c.step)));
		text_editor->select_all();

		if (c.mode == TreeItem::CELL_MODE_RANGE) {
			value_editor->set_position(textedpos + Point2i(0, text_editor->get_size().height));
			value_editor->set_size(Size2(rect.size.width, 1));
			value_editor->show_modal();
			updating_value_editor = true;
			value_editor->set_min(c.min);
			value_editor->set_max(c.max);
			value_editor->set_step(c.step);
			value_editor->set_value(c.val);
			value_editor->set_exp_ratio(c.expr);
			updating_value_editor = false;
		}

		text_editor->show_modal();
		text_editor->grab_focus();
		return true;
	}

	return false;
}

Size2 Tree::get_internal_min_size() const {
	Size2i size = cache.bg->get_offset();
	if (root) {
		size.height += get_item_height(root);
	}
	for (int i = 0; i < columns.size(); i++) {
		size.width += columns[i].min_width;
	}

	return size;
}

void Tree::update_scrollbars() {
	Size2 size = get_size();

	Size2 hmin = h_scroll->get_combined_minimum_size();
	Size2 vmin = v_scroll->get_combined_minimum_size();

	v_scroll->set_begin(Point2(size.width - vmin.width, cache.bg->get_margin(MARGIN_TOP)));
	v_scroll->set_end(Point2(size.width, size.height - cache.bg->get_margin(MARGIN_TOP) - cache.bg->get_margin(MARGIN_BOTTOM)));

	h_scroll->set_begin(Point2(0, size.height - hmin.height));
	h_scroll->set_end(Point2(size.width - vmin.width, size.height));

	Size2 min = get_internal_min_size();
	const real_t tree_content_height = size.height - hmin.height - _get_title_button_height();

	if (min.height < tree_content_height) {
		v_scroll->hide();
		cache.offset.y = 0;
	} else {
		v_scroll->show();
		v_scroll->set_max(min.height);
		v_scroll->set_page(tree_content_height);
		cache.offset.y = v_scroll->get_value();
	}

	if (min.width < size.width - vmin.width) {
		h_scroll->hide();
		cache.offset.x = 0;
	} else {
		h_scroll->show();
		h_scroll->set_max(min.width);
		h_scroll->set_page(size.width - vmin.width);
		cache.offset.x = h_scroll->get_value();
	}
}

int Tree::_get_title_button_height() const {
	ERR_FAIL_COND_V(cache.font.is_null() || cache.title_button.is_null(), 0);
	return show_column_titles ? cache.font->get_height() + cache.title_button->get_minimum_size().height : 0;
}

void Tree::_notification(int p_what) {
	if (p_what == NOTIFICATION_FOCUS_ENTER) {
		if (get_tree()) {
			focus_in_id = get_tree()->get_event_count();
		}
	}
	if (p_what == NOTIFICATION_MOUSE_EXIT) {
		if (cache.hover_type != Cache::CLICK_NONE) {
			cache.hover_type = Cache::CLICK_NONE;
			update();
		}
	}

	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		drag_touching = false;
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {
		update_cache();
	}
	if (p_what == NOTIFICATION_DRAG_END) {
		drop_mode_flags = 0;
		scrolling = false;
		set_physics_process_internal(false);
		update();
	}
	if (p_what == NOTIFICATION_DRAG_BEGIN) {
		single_select_defer = nullptr;
		if (cache.scroll_speed > 0) {
			scrolling = true;
			set_physics_process_internal(true);
		}
	}
	if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS) {
		if (drag_touching) {
			if (drag_touching_deaccel) {
				float pos = v_scroll->get_value();
				pos += drag_speed * get_physics_process_delta_time();

				bool turnoff = false;
				if (pos < 0) {
					pos = 0;
					turnoff = true;
					set_physics_process_internal(false);
					drag_touching = false;
					drag_touching_deaccel = false;
				}
				if (pos > (v_scroll->get_max() - v_scroll->get_page())) {
					pos = v_scroll->get_max() - v_scroll->get_page();
					turnoff = true;
				}

				v_scroll->set_value(pos);
				float sgn = drag_speed < 0 ? -1 : 1;
				float val = Math::abs(drag_speed);
				val -= 1000 * get_physics_process_delta_time();

				if (val < 0) {
					turnoff = true;
				}
				drag_speed = sgn * val;

				if (turnoff) {
					set_physics_process_internal(false);
					drag_touching = false;
					drag_touching_deaccel = false;
				}
			}
		}

		Point2 mouse_position = get_viewport()->get_mouse_position() - get_global_position();
		if (scrolling && get_rect().grow(cache.scroll_border).has_point(mouse_position)) {
			Point2 point;

			if ((ABS(mouse_position.x) < ABS(mouse_position.x - get_size().width)) && (ABS(mouse_position.x) < cache.scroll_border)) {
				point.x = mouse_position.x - cache.scroll_border;
			} else if (ABS(mouse_position.x - get_size().width) < cache.scroll_border) {
				point.x = mouse_position.x - (get_size().width - cache.scroll_border);
			}

			if ((ABS(mouse_position.y) < ABS(mouse_position.y - get_size().height)) && (ABS(mouse_position.y) < cache.scroll_border)) {
				point.y = mouse_position.y - cache.scroll_border;
			} else if (ABS(mouse_position.y - get_size().height) < cache.scroll_border) {
				point.y = mouse_position.y - (get_size().height - cache.scroll_border);
			}

			point *= cache.scroll_speed * get_physics_process_delta_time();
			point += get_scroll();
			h_scroll->set_value(point.x);
			v_scroll->set_value(point.y);
		}
	}

	if (p_what == NOTIFICATION_DRAW) {
		update_cache();
		update_scrollbars();
		RID ci = get_canvas_item();

		Ref<StyleBox> bg = cache.bg;

		Point2 draw_ofs;
		draw_ofs += bg->get_offset();
		Size2 draw_size = get_size() - bg->get_minimum_size();

		bg->draw(ci, Rect2(Point2(), get_size()));

		int tbh = _get_title_button_height();

		draw_ofs.y += tbh;
		draw_size.y -= tbh;

		if (root) {
			draw_item(Point2(), draw_ofs, draw_size, root);
		}

		if (show_column_titles) {
			//title buttons
			int ofs2 = cache.bg->get_margin(MARGIN_LEFT);
			for (int i = 0; i < columns.size(); i++) {
				Ref<StyleBox> sb = (cache.click_type == Cache::CLICK_TITLE && cache.click_index == i) ? cache.title_button_pressed : ((cache.hover_type == Cache::CLICK_TITLE && cache.hover_index == i) ? cache.title_button_hover : cache.title_button);
				Ref<Font> f = cache.tb_font;
				Rect2 tbrect = Rect2(ofs2 - cache.offset.x, bg->get_margin(MARGIN_TOP), get_column_width(i), tbh);
				sb->draw(ci, tbrect);
				ofs2 += tbrect.size.width;
				//text
				int clip_w = tbrect.size.width - sb->get_minimum_size().width;
				f->draw_halign(ci, tbrect.position + Point2i(sb->get_offset().x, (tbrect.size.height - f->get_height()) / 2 + f->get_ascent()), HALIGN_CENTER, clip_w, columns[i].title, cache.title_button_color);
			}
		}

		// Draw the background focus outline last, so that it is drawn in front of the section headings.
		// Otherwise, section heading backgrounds can appear to be in front of the focus outline when scrolling.
		if (has_focus()) {
			VisualServer::get_singleton()->canvas_item_add_clip_ignore(ci, true);
			const Ref<StyleBox> bg_focus = get_stylebox("bg_focus");
			bg_focus->draw(ci, Rect2(Point2(), get_size()));
			VisualServer::get_singleton()->canvas_item_add_clip_ignore(ci, false);
		}
	}

	if (p_what == NOTIFICATION_THEME_CHANGED) {
		update_cache();
	}

	if (p_what == NOTIFICATION_RESIZED || p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		if (popup_edited_item != nullptr) {
			Rect2 rect = popup_edited_item->get_meta("__focus_rect");
			Vector2 ofs(0, (text_editor->get_size().height - rect.size.height) / 2);
			Point2i textedpos = get_global_position() + rect.position - ofs;

			if (cache.text_editor_position != textedpos) {
				cache.text_editor_position = textedpos;
				text_editor->set_position(textedpos);
				value_editor->set_position(textedpos + Point2i(0, text_editor->get_size().height));
			}
		}
	}
}

Size2 Tree::get_minimum_size() const {
	return Size2(1, 1);
}

TreeItem *Tree::create_item(TreeItem *p_parent, int p_idx) {
	ERR_FAIL_COND_V(blocked > 0, nullptr);

	TreeItem *ti = nullptr;

	if (p_parent) {
		// Append or insert a new item to the given parent.
		ti = memnew(TreeItem(this));
		ERR_FAIL_COND_V(!ti, nullptr);
		ti->cells.resize(columns.size());

		TreeItem *prev = nullptr;
		TreeItem *c = p_parent->children;
		int idx = 0;

		while (c) {
			if (idx++ == p_idx) {
				ti->next = c;
				break;
			}
			prev = c;
			c = c->next;
		}

		if (prev) {
			prev->next = ti;
		} else {
			p_parent->children = ti;
		}
		ti->parent = p_parent;

	} else {
		if (!root) {
			// No root exists, make the given item the new root.
			ti = memnew(TreeItem(this));
			ERR_FAIL_COND_V(!ti, nullptr);
			ti->cells.resize(columns.size());

			root = ti;
		} else {
			// Root exists, append or insert to root.
			ti = create_item(root, p_idx);
		}
	}

	return ti;
}

TreeItem *Tree::get_root() {
	return root;
}
TreeItem *Tree::get_last_item() {
	TreeItem *last = root;

	while (last) {
		if (last->next) {
			last = last->next;
		} else if (last->children) {
			last = last->children;
		} else {
			break;
		}
	}

	return last;
}

void Tree::item_edited(int p_column, TreeItem *p_item, bool p_lmb) {
	edited_item = p_item;
	edited_col = p_column;
	if (p_lmb) {
		emit_signal("item_edited");
	} else {
		emit_signal("item_rmb_edited");
	}
}

void Tree::item_changed(int p_column, TreeItem *p_item) {
	update();
}

void Tree::item_selected(int p_column, TreeItem *p_item) {
	if (select_mode == SELECT_MULTI) {
		if (!p_item->cells[p_column].selectable) {
			return;
		}

		p_item->cells.write[p_column].selected = true;
		//emit_signal("multi_selected",p_item,p_column,true); - NO this is for TreeItem::select

		selected_col = p_column;
		if (!selected_item) {
			selected_item = p_item;
		}
	} else {
		select_single_item(p_item, root, p_column);
	}
	update();
}

void Tree::item_deselected(int p_column, TreeItem *p_item) {
	if (selected_item == p_item) {
		selected_item = nullptr;

		if (selected_col == p_column) {
			selected_col = -1;
		}
	}

	if (select_mode == SELECT_MULTI || select_mode == SELECT_SINGLE) {
		p_item->cells.write[p_column].selected = false;
	}
	update();
}

void Tree::set_select_mode(SelectMode p_mode) {
	select_mode = p_mode;
}

Tree::SelectMode Tree::get_select_mode() const {
	return select_mode;
}

void Tree::deselect_all() {
	TreeItem *item = get_next_selected(get_root());
	while (item) {
		for (int i = 0; i < columns.size(); i++) {
			item->deselect(i);
		}
		TreeItem *prev_item = item;
		item = get_next_selected(get_root());
		ERR_FAIL_COND(item == prev_item);
	}

	selected_item = nullptr;
	selected_col = -1;

	update();
}

bool Tree::is_anything_selected() {
	return (selected_item != nullptr);
}

void Tree::clear() {
	ERR_FAIL_COND(blocked > 0);

	if (pressing_for_editor) {
		if (range_drag_enabled) {
			range_drag_enabled = false;
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			warp_mouse(range_drag_capture_pos);
		}
		pressing_for_editor = false;
	}

	if (root) {
		memdelete(root);
		root = nullptr;
	};

	selected_item = nullptr;
	edited_item = nullptr;
	popup_edited_item = nullptr;

	update();
};

void Tree::set_hide_root(bool p_enabled) {
	hide_root = p_enabled;
	update();
}

bool Tree::is_root_hidden() const {
	return hide_root;
}

void Tree::set_column_min_width(int p_column, int p_min_width) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (p_min_width < 1) {
		return;
	}
	columns.write[p_column].min_width = p_min_width;
	update();
}
void Tree::set_column_expand(int p_column, bool p_expand) {
	ERR_FAIL_INDEX(p_column, columns.size());

	columns.write[p_column].expand = p_expand;
	update();
}

TreeItem *Tree::get_selected() const {
	return selected_item;
}

void Tree::set_selected(TreeItem *p_item, int p_column) {
	ERR_FAIL_INDEX(p_column, columns.size());
	ERR_FAIL_COND(!p_item);
	select_single_item(p_item, get_root(), p_column);
}

int Tree::get_selected_column() const {
	return selected_col;
}

TreeItem *Tree::get_edited() const {
	return edited_item;
}

int Tree::get_edited_column() const {
	return edited_col;
}

TreeItem *Tree::get_next_selected(TreeItem *p_item) {
	/*
	if (!p_item)
		return NULL;
	*/
	if (!root) {
		return nullptr;
	}

	while (true) {
		if (!p_item) {
			p_item = root;
		} else {
			if (p_item->children) {
				p_item = p_item->children;

			} else if (p_item->next) {
				p_item = p_item->next;
			} else {
				while (!p_item->next) {
					p_item = p_item->parent;
					if (p_item == nullptr) {
						return nullptr;
					}
				}

				p_item = p_item->next;
			}
		}

		for (int i = 0; i < columns.size(); i++) {
			if (p_item->cells[i].selected) {
				return p_item;
			}
		}
	}

	return nullptr;
}

int Tree::get_column_width(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), -1);

	if (!columns[p_column].expand) {
		return columns[p_column].min_width;
	}

	int expand_area = get_size().width;

	Ref<StyleBox> bg = cache.bg;

	if (bg.is_valid()) {
		expand_area -= bg->get_margin(MARGIN_LEFT) + bg->get_margin(MARGIN_RIGHT);
	}

	if (v_scroll->is_visible_in_tree()) {
		expand_area -= v_scroll->get_combined_minimum_size().width;
	}

	int expanding_columns = 0;
	int expanding_total = 0;

	for (int i = 0; i < columns.size(); i++) {
		if (!columns[i].expand) {
			expand_area -= columns[i].min_width;
		} else {
			expanding_total += columns[i].min_width;
			expanding_columns++;
		}
	}

	if (expand_area < expanding_total) {
		return columns[p_column].min_width;
	}

	ERR_FAIL_COND_V(expanding_columns == 0, -1); // shouldn't happen

	return expand_area * columns[p_column].min_width / expanding_total;
}

void Tree::propagate_set_columns(TreeItem *p_item) {
	p_item->cells.resize(columns.size());

	TreeItem *c = p_item->get_children();
	while (c) {
		propagate_set_columns(c);
		c = c->get_next();
	}
}

void Tree::set_columns(int p_columns) {
	ERR_FAIL_COND(p_columns < 1);
	ERR_FAIL_COND(blocked > 0);
	columns.resize(p_columns);

	if (root) {
		propagate_set_columns(root);
	}
	if (selected_col >= p_columns) {
		selected_col = p_columns - 1;
	}
	update();
}

int Tree::get_columns() const {
	return columns.size();
}

void Tree::_scroll_moved(float) {
	update();
}

Rect2 Tree::get_custom_popup_rect() const {
	return custom_popup_rect;
}

int Tree::get_item_offset(TreeItem *p_item) const {
	TreeItem *it = root;
	int ofs = _get_title_button_height();
	if (!it) {
		return 0;
	}

	while (true) {
		if (it == p_item) {
			return ofs;
		}

		ofs += compute_item_height(it);
		if (it != root || !hide_root) {
			ofs += cache.vseparation;
		}

		if (it->children && !it->collapsed) {
			it = it->children;

		} else if (it->next) {
			it = it->next;
		} else {
			while (!it->next) {
				it = it->parent;
				if (it == nullptr) {
					return 0;
				}
			}

			it = it->next;
		}
	}

	return -1; //not found
}

void Tree::ensure_cursor_is_visible() {
	if (!is_inside_tree()) {
		return;
	}
	if (!selected_item || (selected_col == -1)) {
		return; // Nothing under cursor.
	}

	const Size2 area_size = get_size() - cache.bg->get_minimum_size();

	int y_offset = get_item_offset(selected_item);
	if (y_offset != -1) {
		const int tbh = _get_title_button_height();
		y_offset -= tbh;

		const int cell_h = compute_item_height(selected_item) + cache.vseparation;
		const int screen_h = area_size.height - h_scroll->get_combined_minimum_size().height - tbh;

		if (cell_h > screen_h) { // Screen size is too small, maybe it was not resized yet.
			v_scroll->set_value(y_offset);
		} else if (y_offset + cell_h > v_scroll->get_value() + screen_h) {
			v_scroll->call_deferred("set_value", y_offset - screen_h + cell_h);
		} else if (y_offset < v_scroll->get_value()) {
			v_scroll->set_value(y_offset);
		}
	}

	if (select_mode != SELECT_ROW) { // Cursor always at col 0 in this mode.
		int x_offset = 0;
		for (int i = 0; i < selected_col; i++) {
			x_offset += get_column_width(i);
		}

		const int cell_w = get_column_width(selected_col);
		const int screen_w = area_size.width - v_scroll->get_combined_minimum_size().width;

		if (cell_w > screen_w) {
			h_scroll->set_value(x_offset);
		} else if (x_offset + cell_w > h_scroll->get_value() + screen_w) {
			h_scroll->call_deferred("set_value", x_offset - screen_w + cell_w);
		} else if (x_offset < h_scroll->get_value()) {
			h_scroll->set_value(x_offset);
		}
	}
}

int Tree::get_pressed_button() const {
	return pressed_button;
}

Rect2 Tree::get_item_rect(TreeItem *p_item, int p_column) const {
	ERR_FAIL_NULL_V(p_item, Rect2());
	ERR_FAIL_COND_V(p_item->tree != this, Rect2());
	if (p_column != -1) {
		ERR_FAIL_INDEX_V(p_column, columns.size(), Rect2());
	}

	int ofs = get_item_offset(p_item);
	int height = compute_item_height(p_item);
	Rect2 r;
	r.position.y = ofs;
	r.size.height = height;

	if (p_column == -1) {
		r.position.x = 0;
		r.size.x = get_size().width;
	} else {
		int accum = 0;
		for (int i = 0; i < p_column; i++) {
			accum += get_column_width(i);
		}
		r.position.x = accum;
		r.size.x = get_column_width(p_column);
	}

	return r;
}

void Tree::set_column_titles_visible(bool p_show) {
	show_column_titles = p_show;
	update();
}

bool Tree::are_column_titles_visible() const {
	return show_column_titles;
}

void Tree::set_column_title(int p_column, const String &p_title) {
	ERR_FAIL_INDEX(p_column, columns.size());
	columns.write[p_column].title = p_title;
	update();
}

String Tree::get_column_title(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), "");
	return columns[p_column].title;
}

Point2 Tree::get_scroll() const {
	Point2 ofs;
	if (h_scroll->is_visible_in_tree()) {
		ofs.x = h_scroll->get_value();
	}
	if (v_scroll->is_visible_in_tree()) {
		ofs.y = v_scroll->get_value();
	}
	return ofs;
}

void Tree::scroll_to_item(TreeItem *p_item) {
	if (!is_visible_in_tree()) {
		// hack to work around crash in get_item_rect() if Tree is not in tree.
		return;
	}

	// make sure the scrollbar min and max are up to date with latest changes.
	update_scrollbars();

	const Rect2 r = get_item_rect(p_item);

	if (r.position.y <= v_scroll->get_value()) {
		v_scroll->set_value(r.position.y);
	} else if (r.position.y + r.size.y + 2 * cache.vseparation > v_scroll->get_value() + get_size().y) {
		v_scroll->set_value(r.position.y + r.size.y + 2 * cache.vseparation - get_size().y);
	}
}

TreeItem *Tree::_search_item_text(TreeItem *p_at, const String &p_find, int *r_col, bool p_selectable, bool p_backwards) {
	TreeItem *from = p_at;
	TreeItem *loop = nullptr; // Safe-guard against infinite loop.

	while (p_at) {
		for (int i = 0; i < columns.size(); i++) {
			if (p_at->get_text(i).findn(p_find) == 0 && (!p_selectable || p_at->is_selectable(i))) {
				if (r_col) {
					*r_col = i;
				}
				return p_at;
			}
		}

		if (p_backwards) {
			p_at = p_at->get_prev_visible(true);
		} else {
			p_at = p_at->get_next_visible(true);
		}

		if ((p_at) == from) {
			break;
		}

		if (!loop) {
			loop = p_at;
		} else if (loop == p_at) {
			break;
		}
	}

	return nullptr;
}

TreeItem *Tree::search_item_text(const String &p_find, int *r_col, bool p_selectable) {
	TreeItem *from = get_selected();

	if (!from) {
		from = root;
	}
	if (!from) {
		return nullptr;
	}

	return _search_item_text(from->get_next_visible(true), p_find, r_col, p_selectable);
}

TreeItem *Tree::get_item_with_text(const String &p_find) const {
	for (TreeItem *current = root; current; current = current->get_next_visible()) {
		for (int i = 0; i < columns.size(); i++) {
			if (current->get_text(i) == p_find) {
				return current;
			}
		}
	}
	return nullptr;
}

void Tree::_do_incr_search(const String &p_add) {
	uint64_t time = OS::get_singleton()->get_ticks_usec() / 1000; // convert to msec
	uint64_t diff = time - last_keypress;
	if (diff > uint64_t(GLOBAL_DEF("gui/timers/incremental_search_max_interval_msec", 2000))) {
		incr_search = p_add;
	} else if (incr_search != p_add) {
		incr_search += p_add;
	}

	last_keypress = time;
	int col;
	TreeItem *item = search_item_text(incr_search, &col, true);
	if (!item) {
		return;
	}

	item->select(col);
	ensure_cursor_is_visible();
}

TreeItem *Tree::_find_item_at_pos(TreeItem *p_item, const Point2 &p_pos, int &r_column, int &h, int &section) const {
	Point2 pos = p_pos;

	if (root != p_item || !hide_root) {
		h = compute_item_height(p_item) + cache.vseparation;
		if (pos.y < h) {
			if (drop_mode_flags == DROP_MODE_ON_ITEM) {
				section = 0;
			} else if (drop_mode_flags == DROP_MODE_INBETWEEN) {
				section = pos.y < h / 2 ? -1 : 1;
			} else if (pos.y < h / 4) {
				section = -1;
			} else if (pos.y >= (h * 3 / 4)) {
				section = 1;
			} else {
				section = 0;
			}

			for (int i = 0; i < columns.size(); i++) {
				int w = get_column_width(i);
				if (pos.x < w) {
					r_column = i;

					return p_item;
				}
				pos.x -= w;
			}

			return nullptr;
		} else {
			pos.y -= h;
		}
	} else {
		h = 0;
	}

	if (p_item->is_collapsed()) {
		return nullptr; // do not try children, it's collapsed
	}

	TreeItem *n = p_item->get_children();
	while (n) {
		int ch;
		TreeItem *r = _find_item_at_pos(n, pos, r_column, ch, section);
		pos.y -= ch;
		h += ch;
		if (r) {
			return r;
		}
		n = n->get_next();
	}

	return nullptr;
}

int Tree::get_column_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		pos -= cache.bg->get_offset();
		pos.y -= _get_title_button_height();
		if (pos.y < 0) {
			return -1;
		}

		if (h_scroll->is_visible_in_tree()) {
			pos.x += h_scroll->get_value();
		}
		if (v_scroll->is_visible_in_tree()) {
			pos.y += v_scroll->get_value();
		}

		int col, h, section;
		TreeItem *it = _find_item_at_pos(root, pos, col, h, section);

		if (it) {
			return col;
		}
	}

	return -1;
}

int Tree::get_drop_section_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		pos -= cache.bg->get_offset();
		pos.y -= _get_title_button_height();
		if (pos.y < 0) {
			return -100;
		}

		if (h_scroll->is_visible_in_tree()) {
			pos.x += h_scroll->get_value();
		}
		if (v_scroll->is_visible_in_tree()) {
			pos.y += v_scroll->get_value();
		}

		int col, h, section;
		TreeItem *it = _find_item_at_pos(root, pos, col, h, section);

		if (it) {
			return section;
		}
	}

	return -100;
}
TreeItem *Tree::get_item_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		pos -= cache.bg->get_offset();
		pos.y -= _get_title_button_height();
		if (pos.y < 0) {
			return nullptr;
		}

		if (h_scroll->is_visible_in_tree()) {
			pos.x += h_scroll->get_value();
		}
		if (v_scroll->is_visible_in_tree()) {
			pos.y += v_scroll->get_value();
		}

		int col, h, section;
		TreeItem *it = _find_item_at_pos(root, pos, col, h, section);

		if (it) {
			return it;
		}
	}

	return nullptr;
}

int Tree::get_button_id_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		pos -= cache.bg->get_offset();
		pos.y -= _get_title_button_height();
		if (pos.y < 0) {
			return -1;
		}

		if (h_scroll->is_visible_in_tree()) {
			pos.x += h_scroll->get_value();
		}
		if (v_scroll->is_visible_in_tree()) {
			pos.y += v_scroll->get_value();
		}

		int col, h, section;
		TreeItem *it = _find_item_at_pos(root, pos, col, h, section);

		if (it) {
			const TreeItem::Cell &c = it->cells[col];
			int col_width = get_column_width(col);

			for (int i = 0; i < col; i++) {
				pos.x -= get_column_width(i);
			}

			for (int j = c.buttons.size() - 1; j >= 0; j--) {
				Ref<Texture> b = c.buttons[j].texture;
				Size2 size = b->get_size() + cache.button_pressed->get_minimum_size();
				if (pos.x > col_width - size.width) {
					return c.buttons[j].id;
				}
				col_width -= size.width;
			}
		}
	}

	return -1;
}

String Tree::get_tooltip(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		pos -= cache.bg->get_offset();
		pos.y -= _get_title_button_height();
		if (pos.y < 0) {
			return Control::get_tooltip(p_pos);
		}

		if (h_scroll->is_visible_in_tree()) {
			pos.x += h_scroll->get_value();
		}
		if (v_scroll->is_visible_in_tree()) {
			pos.y += v_scroll->get_value();
		}

		int col, h, section;
		TreeItem *it = _find_item_at_pos(root, pos, col, h, section);

		if (it) {
			const TreeItem::Cell &c = it->cells[col];
			int col_width = get_column_width(col);

			for (int i = 0; i < col; i++) {
				pos.x -= get_column_width(i);
			}

			for (int j = c.buttons.size() - 1; j >= 0; j--) {
				Ref<Texture> b = c.buttons[j].texture;
				Size2 size = b->get_size() + cache.button_pressed->get_minimum_size();
				if (pos.x > col_width - size.width) {
					String tooltip = c.buttons[j].tooltip;
					if (tooltip != "") {
						return tooltip;
					}
				}
				col_width -= size.width;
			}
			String ret;
			if (it->get_tooltip(col) == "") {
				ret = it->get_text(col);
			} else {
				ret = it->get_tooltip(col);
			}
			return ret;
		}
	}

	return Control::get_tooltip(p_pos);
}

void Tree::set_cursor_can_exit_tree(bool p_enable) {
	cursor_can_exit_tree = p_enable;
}

bool Tree::can_cursor_exit_tree() const {
	return cursor_can_exit_tree;
}

void Tree::set_hide_folding(bool p_hide) {
	hide_folding = p_hide;
	update();
}

bool Tree::is_folding_hidden() const {
	return hide_folding;
}

void Tree::set_drop_mode_flags(int p_flags) {
	if (drop_mode_flags == p_flags) {
		return;
	}
	drop_mode_flags = p_flags;
	if (drop_mode_flags == 0) {
		drop_mode_over = nullptr;
	}

	update();
}

int Tree::get_drop_mode_flags() const {
	return drop_mode_flags;
}

void Tree::set_edit_checkbox_cell_only_when_checkbox_is_pressed(bool p_enable) {
	force_edit_checkbox_only_on_checkbox = p_enable;
}

bool Tree::get_edit_checkbox_cell_only_when_checkbox_is_pressed() const {
	return force_edit_checkbox_only_on_checkbox;
}

void Tree::set_allow_rmb_select(bool p_allow) {
	allow_rmb_select = p_allow;
}

bool Tree::get_allow_rmb_select() const {
	return allow_rmb_select;
}

void Tree::set_allow_reselect(bool p_allow) {
	allow_reselect = p_allow;
}

bool Tree::get_allow_reselect() const {
	return allow_reselect;
}

void Tree::set_allow_search(bool p_allow) {
	allow_search = p_allow;
}

bool Tree::get_allow_search() const {
	return allow_search;
}

void Tree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_range_click_timeout"), &Tree::_range_click_timeout);
	ClassDB::bind_method(D_METHOD("_gui_input"), &Tree::_gui_input);
	ClassDB::bind_method(D_METHOD("_popup_select"), &Tree::popup_select);
	ClassDB::bind_method(D_METHOD("_text_editor_enter"), &Tree::text_editor_enter);
	ClassDB::bind_method(D_METHOD("_text_editor_modal_close"), &Tree::_text_editor_modal_close);
	ClassDB::bind_method(D_METHOD("_value_editor_changed"), &Tree::value_editor_changed);
	ClassDB::bind_method(D_METHOD("_scroll_moved"), &Tree::_scroll_moved);

	ClassDB::bind_method(D_METHOD("clear"), &Tree::clear);
	ClassDB::bind_method(D_METHOD("create_item", "parent", "idx"), &Tree::_create_item, DEFVAL(Variant()), DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("get_root"), &Tree::get_root);
	ClassDB::bind_method(D_METHOD("set_column_min_width", "column", "min_width"), &Tree::set_column_min_width);
	ClassDB::bind_method(D_METHOD("set_column_expand", "column", "expand"), &Tree::set_column_expand);
	ClassDB::bind_method(D_METHOD("get_column_width", "column"), &Tree::get_column_width);

	ClassDB::bind_method(D_METHOD("set_hide_root", "enable"), &Tree::set_hide_root);
	ClassDB::bind_method(D_METHOD("is_root_hidden"), &Tree::is_root_hidden);
	ClassDB::bind_method(D_METHOD("get_next_selected", "from"), &Tree::_get_next_selected);
	ClassDB::bind_method(D_METHOD("get_selected"), &Tree::get_selected);
	ClassDB::bind_method(D_METHOD("set_selected", "item", "column"), &Tree::_set_selected);
	ClassDB::bind_method(D_METHOD("get_selected_column"), &Tree::get_selected_column);
	ClassDB::bind_method(D_METHOD("get_pressed_button"), &Tree::get_pressed_button);
	ClassDB::bind_method(D_METHOD("set_select_mode", "mode"), &Tree::set_select_mode);
	ClassDB::bind_method(D_METHOD("get_select_mode"), &Tree::get_select_mode);

	ClassDB::bind_method(D_METHOD("set_columns", "amount"), &Tree::set_columns);
	ClassDB::bind_method(D_METHOD("get_columns"), &Tree::get_columns);

	ClassDB::bind_method(D_METHOD("get_edited"), &Tree::get_edited);
	ClassDB::bind_method(D_METHOD("get_edited_column"), &Tree::get_edited_column);
	ClassDB::bind_method(D_METHOD("edit_selected"), &Tree::edit_selected);
	ClassDB::bind_method(D_METHOD("get_custom_popup_rect"), &Tree::get_custom_popup_rect);
	ClassDB::bind_method(D_METHOD("get_item_area_rect", "item", "column"), &Tree::_get_item_rect, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_item_at_position", "position"), &Tree::get_item_at_position);
	ClassDB::bind_method(D_METHOD("get_column_at_position", "position"), &Tree::get_column_at_position);
	ClassDB::bind_method(D_METHOD("get_drop_section_at_position", "position"), &Tree::get_drop_section_at_position);
	ClassDB::bind_method(D_METHOD("get_button_id_at_position", "position"), &Tree::get_button_id_at_position);

	ClassDB::bind_method(D_METHOD("ensure_cursor_is_visible"), &Tree::ensure_cursor_is_visible);

	ClassDB::bind_method(D_METHOD("set_column_titles_visible", "visible"), &Tree::set_column_titles_visible);
	ClassDB::bind_method(D_METHOD("are_column_titles_visible"), &Tree::are_column_titles_visible);

	ClassDB::bind_method(D_METHOD("set_column_title", "column", "title"), &Tree::set_column_title);
	ClassDB::bind_method(D_METHOD("get_column_title", "column"), &Tree::get_column_title);
	ClassDB::bind_method(D_METHOD("get_scroll"), &Tree::get_scroll);
	ClassDB::bind_method(D_METHOD("scroll_to_item", "item"), &Tree::_scroll_to_item);

	ClassDB::bind_method(D_METHOD("set_hide_folding", "hide"), &Tree::set_hide_folding);
	ClassDB::bind_method(D_METHOD("is_folding_hidden"), &Tree::is_folding_hidden);

	ClassDB::bind_method(D_METHOD("set_drop_mode_flags", "flags"), &Tree::set_drop_mode_flags);
	ClassDB::bind_method(D_METHOD("get_drop_mode_flags"), &Tree::get_drop_mode_flags);

	ClassDB::bind_method(D_METHOD("set_allow_rmb_select", "allow"), &Tree::set_allow_rmb_select);
	ClassDB::bind_method(D_METHOD("get_allow_rmb_select"), &Tree::get_allow_rmb_select);

	ClassDB::bind_method(D_METHOD("set_allow_reselect", "allow"), &Tree::set_allow_reselect);
	ClassDB::bind_method(D_METHOD("get_allow_reselect"), &Tree::get_allow_reselect);

	ClassDB::bind_method(D_METHOD("set_allow_search", "allow"), &Tree::set_allow_search);
	ClassDB::bind_method(D_METHOD("get_allow_search"), &Tree::get_allow_search);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "columns"), "set_columns", "get_columns");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "column_titles_visible"), "set_column_titles_visible", "are_column_titles_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_reselect"), "set_allow_reselect", "get_allow_reselect");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_rmb_select"), "set_allow_rmb_select", "get_allow_rmb_select");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_search"), "set_allow_search", "get_allow_search");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_folding"), "set_hide_folding", "is_folding_hidden");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_root"), "set_hide_root", "is_root_hidden");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drop_mode_flags", PROPERTY_HINT_FLAGS, "On Item,In between"), "set_drop_mode_flags", "get_drop_mode_flags");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "select_mode", PROPERTY_HINT_ENUM, "Single,Row,Multi"), "set_select_mode", "get_select_mode");

	ADD_SIGNAL(MethodInfo("item_selected"));
	ADD_SIGNAL(MethodInfo("cell_selected"));
	ADD_SIGNAL(MethodInfo("multi_selected", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem"), PropertyInfo(Variant::INT, "column"), PropertyInfo(Variant::BOOL, "selected")));
	ADD_SIGNAL(MethodInfo("item_rmb_selected", PropertyInfo(Variant::VECTOR2, "position")));
	ADD_SIGNAL(MethodInfo("empty_rmb", PropertyInfo(Variant::VECTOR2, "position")));
	ADD_SIGNAL(MethodInfo("empty_tree_rmb_selected", PropertyInfo(Variant::VECTOR2, "position")));
	ADD_SIGNAL(MethodInfo("item_edited"));
	ADD_SIGNAL(MethodInfo("item_rmb_edited"));
	ADD_SIGNAL(MethodInfo("item_custom_button_pressed"));
	ADD_SIGNAL(MethodInfo("item_double_clicked"));
	ADD_SIGNAL(MethodInfo("item_collapsed", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem")));
	//ADD_SIGNAL( MethodInfo("item_doubleclicked" ) );
	ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem"), PropertyInfo(Variant::INT, "column"), PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("custom_popup_edited", PropertyInfo(Variant::BOOL, "arrow_clicked")));
	ADD_SIGNAL(MethodInfo("item_activated"));
	ADD_SIGNAL(MethodInfo("column_title_pressed", PropertyInfo(Variant::INT, "column")));
	ADD_SIGNAL(MethodInfo("nothing_selected"));

	BIND_ENUM_CONSTANT(SELECT_SINGLE);
	BIND_ENUM_CONSTANT(SELECT_ROW);
	BIND_ENUM_CONSTANT(SELECT_MULTI);

	BIND_ENUM_CONSTANT(DROP_MODE_DISABLED);
	BIND_ENUM_CONSTANT(DROP_MODE_ON_ITEM);
	BIND_ENUM_CONSTANT(DROP_MODE_INBETWEEN);
}

Tree::Tree() {
	selected_col = 0;
	columns.resize(1);
	selected_item = nullptr;
	edited_item = nullptr;
	selected_col = -1;
	edited_col = -1;

	hide_root = false;
	select_mode = SELECT_SINGLE;
	root = nullptr;
	popup_menu = nullptr;
	popup_edited_item = nullptr;
	text_editor = nullptr;
	set_focus_mode(FOCUS_ALL);

	popup_menu = memnew(PopupMenu);
	popup_menu->hide();
	add_child(popup_menu);
	popup_menu->set_as_toplevel(true);
	text_editor = memnew(LineEdit);
	add_child(text_editor);
	text_editor->set_as_toplevel(true);
	text_editor->hide();
	value_editor = memnew(HSlider);
	add_child(value_editor);
	value_editor->set_as_toplevel(true);
	value_editor->hide();

	h_scroll = memnew(HScrollBar);
	v_scroll = memnew(VScrollBar);

	add_child(h_scroll);
	add_child(v_scroll);

	range_click_timer = memnew(Timer);
	range_click_timer->connect("timeout", this, "_range_click_timeout");
	add_child(range_click_timer);

	h_scroll->connect("value_changed", this, "_scroll_moved");
	v_scroll->connect("value_changed", this, "_scroll_moved");
	text_editor->connect("text_entered", this, "_text_editor_enter");
	text_editor->connect("modal_closed", this, "_text_editor_modal_close");
	popup_menu->connect("id_pressed", this, "_popup_select");
	value_editor->connect("value_changed", this, "_value_editor_changed");

	value_editor->set_as_toplevel(true);
	text_editor->set_as_toplevel(true);
	set_notify_transform(true);

	updating_value_editor = false;
	pressed_button = -1;
	show_column_titles = false;

	cache.click_type = Cache::CLICK_NONE;
	cache.hover_type = Cache::CLICK_NONE;
	cache.hover_index = -1;
	cache.click_index = -1;
	cache.click_id = -1;
	cache.click_item = nullptr;
	cache.click_column = 0;
	cache.hover_cell = -1;
	last_keypress = 0;
	focus_in_id = 0;

	blocked = 0;

	cursor_can_exit_tree = true;
	set_mouse_filter(MOUSE_FILTER_STOP);

	drag_speed = 0;
	drag_touching = false;
	drag_touching_deaccel = false;
	pressing_for_editor = false;
	range_drag_enabled = false;

	hide_folding = false;

	drop_mode_flags = 0;
	drop_mode_over = nullptr;
	drop_mode_section = 0;
	single_select_defer = nullptr;

	scrolling = false;
	allow_rmb_select = false;
	force_edit_checkbox_only_on_checkbox = false;

	set_clip_contents(true);

	cache.hover_item = nullptr;
	cache.hover_cell = -1;

	allow_reselect = false;
	allow_search = true;
	propagate_mouse_activated = false;

	update_cache();
}

Tree::~Tree() {
	if (root) {
		memdelete(root);
	}
}
