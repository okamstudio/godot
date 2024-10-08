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

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/math/math_funcs.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/gui/box_container.h"
#include "scene/gui/text_edit.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

#include <limits.h>

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
		if (icon_overlay.is_valid()) {
			Vector2 offset = icon->get_size() - icon_overlay->get_size();
			icon_overlay->draw_rect_region(p_where, Rect2(p_pos + offset, dsize), Rect2(Point2(), icon_overlay->get_size()), p_color);
		}
	} else {
		icon->draw_rect_region(p_where, Rect2(p_pos, dsize), icon_region, p_color);
		if (icon_overlay.is_valid()) {
			icon_overlay->draw_rect_region(p_where, Rect2(p_pos, dsize), icon_region, p_color);
		}
	}
}

void TreeItem::_changed_notify(int p_cell) {
	if (tree) {
		tree->item_changed(p_cell, this);
	}
}

void TreeItem::_changed_notify() {
	if (tree) {
		tree->item_changed(-1, this);
	}
}

void TreeItem::_cell_selected(int p_cell) {
	if (tree) {
		tree->item_selected(p_cell, this);
	}
}

void TreeItem::_cell_deselected(int p_cell) {
	if (tree) {
		tree->item_deselected(p_cell, this);
	}
}

void TreeItem::_change_tree(Tree *p_tree) {
	if (p_tree == tree) {
		return;
	}

	TreeItem *c = first_child;
	while (c) {
		c->_change_tree(p_tree);
		c = c->next;
	}

	if (tree) {
		if (tree->root == this) {
			tree->root = nullptr;
		}

		if (tree->popup_edited_item == this) {
			tree->popup_edited_item = nullptr;
			tree->popup_pressing_edited_item = nullptr;
			tree->pressing_for_editor = false;
		}

		if (tree->cache.hover_item == this) {
			tree->cache.hover_item = nullptr;
		}

		if (tree->selected_item == this) {
			for (int i = 0; i < tree->selected_item->cells.size(); i++) {
				tree->selected_item->cells.write[i].selected = false;
			}

			tree->selected_item = nullptr;
		}

		if (tree->drop_mode_over == this) {
			tree->drop_mode_over = nullptr;
		}

		if (tree->single_select_defer == this) {
			tree->single_select_defer = nullptr;
		}

		if (tree->edited_item == this) {
			tree->edited_item = nullptr;
			tree->pressing_for_editor = false;
		}

		tree->queue_redraw();
	}

	tree = p_tree;

	if (tree) {
		tree->queue_redraw();
		cells.resize(tree->columns.size());
	}
}

/* cell mode */
void TreeItem::set_cell_mode(int p_column, TreeCellMode p_mode) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].mode == p_mode) {
		return;
	}

	Cell &c = cells.write[p_column];
	c.mode = p_mode;
	c.min = 0;
	c.max = 100;
	c.step = 1;
	c.val = 0;
	c.checked = false;
	c.icon = Ref<Texture2D>();
	c.text = "";
	c.dirty = true;
	c.icon_max_w = 0;
	c.cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

TreeItem::TreeCellMode TreeItem::get_cell_mode(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), TreeItem::CELL_MODE_STRING);
	return cells[p_column].mode;
}

/* auto translate mode */
void TreeItem::set_auto_translate_mode(int p_column, Node::AutoTranslateMode p_mode) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].auto_translate_mode == p_mode) {
		return;
	}

	cells.write[p_column].auto_translate_mode = p_mode;
	cells.write[p_column].dirty = true;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Node::AutoTranslateMode TreeItem::get_auto_translate_mode(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Node::AUTO_TRANSLATE_MODE_INHERIT);
	return cells[p_column].auto_translate_mode;
}

/* multiline editable */
void TreeItem::set_edit_multiline(int p_column, bool p_multiline) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].edit_multiline = p_multiline;
	_changed_notify(p_column);
}

bool TreeItem::is_edit_multiline(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].edit_multiline;
}

/* check mode */
void TreeItem::set_checked(int p_column, bool p_checked) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].checked == p_checked) {
		return;
	}

	cells.write[p_column].checked = p_checked;
	cells.write[p_column].indeterminate = false;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

void TreeItem::set_indeterminate(int p_column, bool p_indeterminate) {
	ERR_FAIL_INDEX(p_column, cells.size());

	// Prevent uncheck if indeterminate set to false twice
	if (p_indeterminate == cells[p_column].indeterminate) {
		return;
	}

	cells.write[p_column].indeterminate = p_indeterminate;
	cells.write[p_column].checked = false;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

bool TreeItem::is_checked(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].checked;
}

bool TreeItem::is_indeterminate(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].indeterminate;
}

void TreeItem::propagate_check(int p_column, bool p_emit_signal) {
	bool ch = cells[p_column].checked;

	if (p_emit_signal) {
		tree->emit_signal(SNAME("check_propagated_to_item"), this, p_column);
	}
	_propagate_check_through_children(p_column, ch, p_emit_signal);
	_propagate_check_through_parents(p_column, p_emit_signal);
}

String TreeItem::atr(int p_column, const String &p_text) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), tree->atr(p_text));

	switch (cells[p_column].auto_translate_mode) {
		case Node::AUTO_TRANSLATE_MODE_INHERIT: {
			return tree->atr(p_text);
		} break;
		case Node::AUTO_TRANSLATE_MODE_ALWAYS: {
			return tree->tr(p_text);
		} break;
		case Node::AUTO_TRANSLATE_MODE_DISABLED: {
			return p_text;
		} break;
	}

	ERR_FAIL_V_MSG(tree->atr(p_text), "Unexpected auto translate mode: " + itos(cells[p_column].auto_translate_mode));
}

void TreeItem::_propagate_check_through_children(int p_column, bool p_checked, bool p_emit_signal) {
	TreeItem *current = get_first_child();
	while (current) {
		current->set_checked(p_column, p_checked);
		if (p_emit_signal) {
			current->tree->emit_signal(SNAME("check_propagated_to_item"), current, p_column);
		}
		current->_propagate_check_through_children(p_column, p_checked, p_emit_signal);
		current = current->get_next();
	}
}

void TreeItem::_propagate_check_through_parents(int p_column, bool p_emit_signal) {
	TreeItem *current = get_parent();
	if (!current) {
		return;
	}

	bool any_checked = false;
	bool any_unchecked = false;
	bool any_indeterminate = false;

	TreeItem *child_item = current->get_first_child();
	while (child_item) {
		if (!child_item->is_checked(p_column)) {
			any_unchecked = true;
			if (child_item->is_indeterminate(p_column)) {
				any_indeterminate = true;
				break;
			}
		} else {
			any_checked = true;
		}
		child_item = child_item->get_next();
	}

	if (any_indeterminate || (any_checked && any_unchecked)) {
		current->set_indeterminate(p_column, true);
	} else if (current->is_indeterminate(p_column) && !any_checked) {
		current->set_indeterminate(p_column, false);
	} else {
		current->set_checked(p_column, any_checked);
	}

	if (p_emit_signal) {
		current->tree->emit_signal(SNAME("check_propagated_to_item"), current, p_column);
	}
	current->_propagate_check_through_parents(p_column, p_emit_signal);
}

void TreeItem::set_text(int p_column, String p_text) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].text == p_text) {
		return;
	}

	cells.write[p_column].text = p_text;
	cells.write[p_column].dirty = true;

	if (cells[p_column].mode == TreeItem::CELL_MODE_RANGE) {
		Vector<String> strings = p_text.split(",");
		cells.write[p_column].min = INT_MAX;
		cells.write[p_column].max = INT_MIN;
		for (int i = 0; i < strings.size(); i++) {
			int value = i;
			if (!strings[i].get_slicec(':', 1).is_empty()) {
				value = strings[i].get_slicec(':', 1).to_int();
			}
			cells.write[p_column].min = MIN(cells[p_column].min, value);
			cells.write[p_column].max = MAX(cells[p_column].max, value);
		}
		cells.write[p_column].step = 0;
	} else {
		// Don't auto translate if it's in string mode and editable, as the text can be changed to anything by the user.
		if (tree && (!cells[p_column].editable || cells[p_column].mode != TreeItem::CELL_MODE_STRING)) {
			cells.write[p_column].xl_text = atr(p_column, p_text);
		} else {
			cells.write[p_column].xl_text = p_text;
		}
	}

	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

String TreeItem::get_text(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].text;
}

void TreeItem::set_text_direction(int p_column, Control::TextDirection p_text_direction) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);

	if (cells[p_column].text_direction == p_text_direction) {
		return;
	}

	cells.write[p_column].text_direction = p_text_direction;
	cells.write[p_column].dirty = true;
	_changed_notify(p_column);
	cells.write[p_column].cached_minimum_size_dirty = true;
}

Control::TextDirection TreeItem::get_text_direction(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Control::TEXT_DIRECTION_INHERITED);
	return cells[p_column].text_direction;
}

void TreeItem::set_autowrap_mode(int p_column, TextServer::AutowrapMode p_mode) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_COND(p_mode < TextServer::AUTOWRAP_OFF || p_mode > TextServer::AUTOWRAP_WORD_SMART);

	if (cells[p_column].autowrap_mode == p_mode) {
		return;
	}

	cells.write[p_column].autowrap_mode = p_mode;
	cells.write[p_column].dirty = true;
	_changed_notify(p_column);
	cells.write[p_column].cached_minimum_size_dirty = true;
}

TextServer::AutowrapMode TreeItem::get_autowrap_mode(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), TextServer::AUTOWRAP_OFF);
	return cells[p_column].autowrap_mode;
}

void TreeItem::set_text_overrun_behavior(int p_column, TextServer::OverrunBehavior p_behavior) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].text_buf->get_text_overrun_behavior() == p_behavior) {
		return;
	}

	cells.write[p_column].text_buf->set_text_overrun_behavior(p_behavior);
	cells.write[p_column].dirty = true;
	cells.write[p_column].cached_minimum_size_dirty = true;
	_changed_notify(p_column);
}

TextServer::OverrunBehavior TreeItem::get_text_overrun_behavior(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), TextServer::OVERRUN_TRIM_ELLIPSIS);
	return cells[p_column].text_buf->get_text_overrun_behavior();
}

void TreeItem::set_structured_text_bidi_override(int p_column, TextServer::StructuredTextParser p_parser) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].st_parser != p_parser) {
		cells.write[p_column].st_parser = p_parser;
		cells.write[p_column].dirty = true;
		cells.write[p_column].cached_minimum_size_dirty = true;

		_changed_notify(p_column);
	}
}

TextServer::StructuredTextParser TreeItem::get_structured_text_bidi_override(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), TextServer::STRUCTURED_TEXT_DEFAULT);
	return cells[p_column].st_parser;
}

void TreeItem::set_structured_text_bidi_override_options(int p_column, Array p_args) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].st_args == p_args) {
		return;
	}

	cells.write[p_column].st_args = p_args;
	cells.write[p_column].dirty = true;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Array TreeItem::get_structured_text_bidi_override_options(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Array());
	return cells[p_column].st_args;
}

void TreeItem::set_language(int p_column, const String &p_language) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].language != p_language) {
		cells.write[p_column].language = p_language;
		cells.write[p_column].dirty = true;
		cells.write[p_column].cached_minimum_size_dirty = true;

		_changed_notify(p_column);
	}
}

String TreeItem::get_language(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].language;
}

void TreeItem::set_suffix(int p_column, String p_suffix) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].suffix == p_suffix) {
		return;
	}

	cells.write[p_column].suffix = p_suffix;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

String TreeItem::get_suffix(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].suffix;
}

void TreeItem::set_icon(int p_column, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].icon == p_icon) {
		return;
	}

	cells.write[p_column].icon = p_icon;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Ref<Texture2D> TreeItem::get_icon(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Texture2D>());
	return cells[p_column].icon;
}

void TreeItem::set_icon_overlay(int p_column, const Ref<Texture2D> &p_icon_overlay) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].icon_overlay == p_icon_overlay) {
		return;
	}

	cells.write[p_column].icon_overlay = p_icon_overlay;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Ref<Texture2D> TreeItem::get_icon_overlay(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Texture2D>());
	return cells[p_column].icon_overlay;
}

void TreeItem::set_icon_region(int p_column, const Rect2 &p_icon_region) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].icon_region == p_icon_region) {
		return;
	}

	cells.write[p_column].icon_region = p_icon_region;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Rect2 TreeItem::get_icon_region(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Rect2());
	return cells[p_column].icon_region;
}

void TreeItem::set_icon_modulate(int p_column, const Color &p_modulate) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].icon_color == p_modulate) {
		return;
	}

	cells.write[p_column].icon_color = p_modulate;
	_changed_notify(p_column);
}

Color TreeItem::get_icon_modulate(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Color());
	return cells[p_column].icon_color;
}

void TreeItem::set_icon_max_width(int p_column, int p_max) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].icon_max_w == p_max) {
		return;
	}

	cells.write[p_column].icon_max_w = p_max;
	cells.write[p_column].cached_minimum_size_dirty = true;

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
		p_value = Math::snapped(p_value, cells[p_column].step);
	}
	if (p_value < cells[p_column].min) {
		p_value = cells[p_column].min;
	}
	if (p_value > cells[p_column].max) {
		p_value = cells[p_column].max;
	}

	if (cells[p_column].val == p_value) {
		return;
	}

	cells.write[p_column].val = p_value;
	cells.write[p_column].dirty = true;
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

	if (cells[p_column].min == p_min && cells[p_column].max == p_max && cells[p_column].step == p_step && cells[p_column].expr == p_exp) {
		return;
	}

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

#ifndef DISABLE_DEPRECATED
void TreeItem::set_custom_draw(int p_column, Object *p_object, const StringName &p_callback) {
	WARN_DEPRECATED_MSG(R"*(The "set_custom_draw()" method is deprecated, use "set_custom_draw_callback()" instead.)*");
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_NULL(p_object);

	cells.write[p_column].custom_draw_callback = Callable(p_object, p_callback);

	_changed_notify(p_column);
}
#endif // DISABLE_DEPRECATED

void TreeItem::set_custom_draw_callback(int p_column, const Callable &p_callback) {
	ERR_FAIL_INDEX(p_column, cells.size());

	cells.write[p_column].custom_draw_callback = p_callback;

	_changed_notify(p_column);
}

Callable TreeItem::get_custom_draw_callback(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Callable());

	return cells[p_column].custom_draw_callback;
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
				emit_signal(SNAME("cell_selected"));
			} else {
				select(tree->selected_col);
			}

			tree->queue_redraw();
		}
	}

	_changed_notify();
	tree->emit_signal(SNAME("item_collapsed"), this);
}

bool TreeItem::is_collapsed() {
	return collapsed;
}

void TreeItem::set_collapsed_recursive(bool p_collapsed) {
	if (!tree) {
		return;
	}

	set_collapsed(p_collapsed);

	TreeItem *child = get_first_child();
	while (child) {
		child->set_collapsed_recursive(p_collapsed);
		child = child->get_next();
	}
}

bool TreeItem::_is_any_collapsed(bool p_only_visible) {
	TreeItem *child = get_first_child();

	// Check on children directly first (avoid recursing if possible).
	while (child) {
		if (child->get_first_child() && child->is_collapsed() && (!p_only_visible || (child->is_visible() && child->get_visible_child_count()))) {
			return true;
		}
		child = child->get_next();
	}

	child = get_first_child();

	// Otherwise recurse on children.
	while (child) {
		if (child->get_first_child() && (!p_only_visible || (child->is_visible() && child->get_visible_child_count())) && child->_is_any_collapsed(p_only_visible)) {
			return true;
		}
		child = child->get_next();
	}

	return false;
}

bool TreeItem::is_any_collapsed(bool p_only_visible) {
	if (p_only_visible && !is_visible_in_tree()) {
		return false;
	}

	// Collapsed if this is collapsed and it has children (only considers visible if only visible is set).
	if (is_collapsed() && get_first_child() && (!p_only_visible || get_visible_child_count())) {
		return true;
	}

	return _is_any_collapsed(p_only_visible);
}

void TreeItem::set_visible(bool p_visible) {
	if (visible == p_visible) {
		return;
	}
	visible = p_visible;
	if (tree) {
		tree->queue_redraw();
		_changed_notify();
	}

	_handle_visibility_changed(p_visible);
}

bool TreeItem::is_visible() {
	return visible;
}

bool TreeItem::is_visible_in_tree() const {
	return visible && parent_visible_in_tree;
}

void TreeItem::_handle_visibility_changed(bool p_visible) {
	TreeItem *child = get_first_child();
	while (child) {
		child->_propagate_visibility_changed(p_visible);
		child = child->get_next();
	}
}

void TreeItem::_propagate_visibility_changed(bool p_parent_visible_in_tree) {
	parent_visible_in_tree = p_parent_visible_in_tree;
	_handle_visibility_changed(p_parent_visible_in_tree);
}

void TreeItem::uncollapse_tree() {
	TreeItem *t = this;
	while (t) {
		t->set_collapsed(false);
		t = t->parent;
	}
}

void TreeItem::set_custom_minimum_height(int p_height) {
	if (custom_min_height == p_height) {
		return;
	}

	custom_min_height = p_height;

	for (Cell &c : cells) {
		c.cached_minimum_size_dirty = true;
	}

	_changed_notify();
}

int TreeItem::get_custom_minimum_height() const {
	return custom_min_height;
}

/* Item manipulation */

TreeItem *TreeItem::create_child(int p_index) {
	TreeItem *ti = memnew(TreeItem(tree));
	if (tree) {
		ti->cells.resize(tree->columns.size());
		tree->queue_redraw();
	}

	TreeItem *item_prev = nullptr;
	TreeItem *item_next = first_child;

	if (p_index < 0 && last_child) {
		item_prev = last_child;
	} else {
		int idx = 0;
		while (item_next) {
			if (idx == p_index) {
				item_next->prev = ti;
				ti->next = item_next;
				break;
			}

			item_prev = item_next;
			item_next = item_next->next;
			idx++;
		}
	}

	if (item_prev) {
		item_prev->next = ti;
		ti->prev = item_prev;

		if (!children_cache.is_empty()) {
			if (ti->next) {
				children_cache.insert(p_index, ti);
			} else {
				children_cache.append(ti);
			}
		}
	} else {
		first_child = ti;
		if (!children_cache.is_empty()) {
			children_cache.insert(0, ti);
		}
	}

	if (item_prev == last_child) {
		last_child = ti;
	}

	ti->parent = this;
	ti->parent_visible_in_tree = is_visible_in_tree();

	return ti;
}

void TreeItem::add_child(TreeItem *p_item) {
	ERR_FAIL_NULL(p_item);
	ERR_FAIL_COND(p_item->tree);
	ERR_FAIL_COND(p_item->parent);

	p_item->_change_tree(tree);
	p_item->parent = this;
	p_item->parent_visible_in_tree = is_visible_in_tree();
	p_item->_handle_visibility_changed(p_item->parent_visible_in_tree);

	if (last_child) {
		last_child->next = p_item;
		p_item->prev = last_child;
	} else {
		first_child = p_item;
	}
	last_child = p_item;

	if (!children_cache.is_empty()) {
		children_cache.append(p_item);
	}

	validate_cache();
}

void TreeItem::remove_child(TreeItem *p_item) {
	ERR_FAIL_NULL(p_item);
	ERR_FAIL_COND(p_item->parent != this);

	p_item->_unlink_from_tree();
	p_item->_change_tree(nullptr);
	p_item->prev = nullptr;
	p_item->next = nullptr;
	p_item->parent = nullptr;

	validate_cache();
}

Tree *TreeItem::get_tree() const {
	return tree;
}

TreeItem *TreeItem::get_next() const {
	return next;
}

TreeItem *TreeItem::get_prev() {
	if (prev) {
		return prev;
	}

	if (!parent || parent->first_child == this) {
		return nullptr;
	}
	// This is an edge case
	TreeItem *l_prev = parent->first_child;
	while (l_prev && l_prev->next != this) {
		l_prev = l_prev->next;
	}

	prev = l_prev;

	return prev;
}

TreeItem *TreeItem::get_parent() const {
	return parent;
}

TreeItem *TreeItem::get_first_child() const {
	return first_child;
}

TreeItem *TreeItem::_get_prev_in_tree(bool p_wrap, bool p_include_invisible) {
	TreeItem *current = this;

	TreeItem *prev_item = current->get_prev();

	if (!prev_item) {
		current = current->parent;
		if (current == tree->root && tree->hide_root) {
			return nullptr;
		} else if (!current) {
			if (p_wrap) {
				current = this;
				TreeItem *temp = get_next_visible();
				while (temp) {
					current = temp;
					temp = temp->get_next_visible();
				}
			} else {
				return nullptr;
			}
		}
	} else {
		current = prev_item;
		while ((!current->collapsed || p_include_invisible) && current->last_child) {
			current = current->last_child;
		}
	}

	return current;
}

TreeItem *TreeItem::get_prev_visible(bool p_wrap) {
	TreeItem *loop = this;
	TreeItem *prev_item = _get_prev_in_tree(p_wrap);
	while (prev_item && !prev_item->is_visible_in_tree()) {
		prev_item = prev_item->_get_prev_in_tree(p_wrap);
		if (prev_item == loop) {
			// Check that we haven't looped all the way around to the start.
			prev_item = nullptr;
			break;
		}
	}
	return prev_item;
}

TreeItem *TreeItem::_get_next_in_tree(bool p_wrap, bool p_include_invisible) {
	TreeItem *current = this;

	if ((!current->collapsed || p_include_invisible) && current->first_child) {
		current = current->first_child;

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

TreeItem *TreeItem::get_next_visible(bool p_wrap) {
	TreeItem *loop = this;
	TreeItem *next_item = _get_next_in_tree(p_wrap);
	while (next_item && !next_item->is_visible_in_tree()) {
		next_item = next_item->_get_next_in_tree(p_wrap);
		if (next_item == loop) {
			// Check that we haven't looped all the way around to the start.
			next_item = nullptr;
			break;
		}
	}
	return next_item;
}

TreeItem *TreeItem::get_prev_in_tree(bool p_wrap) {
	TreeItem *prev_item = _get_prev_in_tree(p_wrap, true);
	return prev_item;
}

TreeItem *TreeItem::get_next_in_tree(bool p_wrap) {
	TreeItem *next_item = _get_next_in_tree(p_wrap, true);
	return next_item;
}

TreeItem *TreeItem::get_child(int p_index) {
	_create_children_cache();

	if (p_index < 0) {
		p_index += children_cache.size();
	}
	ERR_FAIL_INDEX_V(p_index, children_cache.size(), nullptr);

	return children_cache.get(p_index);
}

int TreeItem::get_visible_child_count() {
	_create_children_cache();
	int visible_count = 0;
	for (int i = 0; i < children_cache.size(); i++) {
		if (children_cache[i]->is_visible()) {
			visible_count += 1;
		}
	}
	return visible_count;
}

int TreeItem::get_child_count() {
	_create_children_cache();
	return children_cache.size();
}

TypedArray<TreeItem> TreeItem::get_children() {
	// Don't need to explicitly create children cache, because get_child_count creates it.
	int size = get_child_count();
	TypedArray<TreeItem> arr;
	arr.resize(size);
	for (int i = 0; i < size; i++) {
		arr[i] = children_cache[i];
	}

	return arr;
}

void TreeItem::clear_children() {
	TreeItem *c = first_child;
	while (c) {
		TreeItem *aux = c;
		c = c->get_next();
		aux->parent = nullptr; // So it won't try to recursively auto-remove from me in here.
		memdelete(aux);
	}

	first_child = nullptr;
	last_child = nullptr;
	children_cache.clear();
};

int TreeItem::get_index() {
	int idx = 0;
	TreeItem *c = this;

	while (c) {
		c = c->get_prev();
		idx++;
	}
	return idx - 1;
}

#ifdef DEV_ENABLED
void TreeItem::validate_cache() const {
	if (!parent || parent->children_cache.is_empty()) {
		return;
	}
	TreeItem *scan = parent->first_child;
	int index = 0;
	while (scan) {
		DEV_ASSERT(parent->children_cache[index] == scan);
		++index;
		scan = scan->get_next();
	}
	DEV_ASSERT(index == parent->children_cache.size());
}
#endif

void TreeItem::move_before(TreeItem *p_item) {
	ERR_FAIL_NULL(p_item);
	ERR_FAIL_COND(is_root);
	ERR_FAIL_NULL(p_item->parent);

	if (p_item == this) {
		return;
	}

	TreeItem *p = p_item->parent;
	while (p) {
		ERR_FAIL_COND_MSG(p == this, "Can't move to a descendant");
		p = p->parent;
	}

	Tree *old_tree = tree;
	_unlink_from_tree();
	_change_tree(p_item->tree);

	parent = p_item->parent;

	TreeItem *item_prev = p_item->get_prev();
	if (item_prev) {
		item_prev->next = this;
		parent->children_cache.clear();
	} else {
		parent->first_child = this;
		// If the cache is empty, it has not been built but there
		// are items in the tree (note p_item != nullptr) so we cannot update it.
		if (!parent->children_cache.is_empty()) {
			parent->children_cache.insert(0, this);
		}
	}

	prev = item_prev;
	next = p_item;
	p_item->prev = this;

	if (tree && old_tree == tree) {
		tree->queue_redraw();
	}

	validate_cache();
}

void TreeItem::move_after(TreeItem *p_item) {
	ERR_FAIL_NULL(p_item);
	ERR_FAIL_COND(is_root);
	ERR_FAIL_NULL(p_item->parent);

	if (p_item == this) {
		return;
	}

	TreeItem *p = p_item->parent;
	while (p) {
		ERR_FAIL_COND_MSG(p == this, "Can't move to a descendant");
		p = p->parent;
	}

	Tree *old_tree = tree;
	_unlink_from_tree();
	_change_tree(p_item->tree);

	if (p_item->next) {
		p_item->next->prev = this;
	}
	parent = p_item->parent;
	prev = p_item;
	next = p_item->next;
	p_item->next = this;

	if (next) {
		parent->children_cache.clear();
	} else {
		parent->last_child = this;
		// If the cache is empty, it has not been built but there
		// are items in the tree (note p_item != nullptr,) so we cannot update it.
		if (!parent->children_cache.is_empty()) {
			parent->children_cache.append(this);
		}
	}

	if (tree && old_tree == tree) {
		tree->queue_redraw();
	}
	validate_cache();
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
	if (tree->selected_item == this && tree->selected_col == p_column) {
		return;
	}
	tree->selected_item = this;
	tree->selected_col = p_column;
	tree->queue_redraw();
}

void TreeItem::select(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	_cell_selected(p_column);
}

void TreeItem::deselect(int p_column) {
	ERR_FAIL_INDEX(p_column, cells.size());
	_cell_deselected(p_column);
}

void TreeItem::add_button(int p_column, const Ref<Texture2D> &p_button, int p_id, bool p_disabled, const String &p_tooltip) {
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
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

int TreeItem::get_button_count(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	return cells[p_column].buttons.size();
}

Ref<Texture2D> TreeItem::get_button(int p_column, int p_index) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Texture2D>());
	ERR_FAIL_INDEX_V(p_index, cells[p_column].buttons.size(), Ref<Texture2D>());
	return cells[p_column].buttons[p_index].texture;
}

String TreeItem::get_button_tooltip_text(int p_column, int p_index) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), String());
	ERR_FAIL_INDEX_V(p_index, cells[p_column].buttons.size(), String());
	return cells[p_column].buttons[p_index].tooltip;
}

int TreeItem::get_button_id(int p_column, int p_index) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	ERR_FAIL_INDEX_V(p_index, cells[p_column].buttons.size(), -1);
	return cells[p_column].buttons[p_index].id;
}

void TreeItem::erase_button(int p_column, int p_index) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_index, cells[p_column].buttons.size());
	cells.write[p_column].buttons.remove_at(p_index);
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

Color TreeItem::get_button_color(int p_column, int p_index) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Color());
	ERR_FAIL_INDEX_V(p_index, cells[p_column].buttons.size(), Color());
	return cells[p_column].buttons[p_index].color;
}

void TreeItem::set_button_tooltip_text(int p_column, int p_index, const String &p_tooltip) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_index, cells[p_column].buttons.size());
	cells.write[p_column].buttons.write[p_index].tooltip = p_tooltip;
}

void TreeItem::set_button(int p_column, int p_index, const Ref<Texture2D> &p_button) {
	ERR_FAIL_COND(p_button.is_null());
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_index, cells[p_column].buttons.size());

	if (cells[p_column].buttons[p_index].texture == p_button) {
		return;
	}

	cells.write[p_column].buttons.write[p_index].texture = p_button;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

void TreeItem::set_button_color(int p_column, int p_index, const Color &p_color) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_index, cells[p_column].buttons.size());

	if (cells[p_column].buttons[p_index].color == p_color) {
		return;
	}

	cells.write[p_column].buttons.write[p_index].color = p_color;
	_changed_notify(p_column);
}

void TreeItem::set_button_disabled(int p_column, int p_index, bool p_disabled) {
	ERR_FAIL_INDEX(p_column, cells.size());
	ERR_FAIL_INDEX(p_index, cells[p_column].buttons.size());

	if (cells[p_column].buttons[p_index].disabled == p_disabled) {
		return;
	}

	cells.write[p_column].buttons.write[p_index].disabled = p_disabled;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

bool TreeItem::is_button_disabled(int p_column, int p_index) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	ERR_FAIL_INDEX_V(p_index, cells[p_column].buttons.size(), false);

	return cells[p_column].buttons[p_index].disabled;
}

void TreeItem::set_editable(int p_column, bool p_editable) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].editable == p_editable) {
		return;
	}

	cells.write[p_column].editable = p_editable;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

bool TreeItem::is_editable(int p_column) {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].editable;
}

void TreeItem::set_custom_color(int p_column, const Color &p_color) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].custom_color && cells[p_column].color == p_color) {
		return;
	}

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

void TreeItem::set_custom_font(int p_column, const Ref<Font> &p_font) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].custom_font == p_font) {
		return;
	}

	cells.write[p_column].custom_font = p_font;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

Ref<Font> TreeItem::get_custom_font(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Ref<Font>());
	return cells[p_column].custom_font;
}

void TreeItem::set_custom_font_size(int p_column, int p_font_size) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].custom_font_size == p_font_size) {
		return;
	}

	cells.write[p_column].custom_font_size = p_font_size;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

int TreeItem::get_custom_font_size(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), -1);
	return cells[p_column].custom_font_size;
}

void TreeItem::set_tooltip_text(int p_column, const String &p_tooltip) {
	ERR_FAIL_INDEX(p_column, cells.size());
	cells.write[p_column].tooltip = p_tooltip;
}

String TreeItem::get_tooltip_text(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), "");
	return cells[p_column].tooltip;
}

void TreeItem::set_custom_bg_color(int p_column, const Color &p_color, bool p_bg_outline) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].custom_bg_color && cells[p_column].custom_bg_outline == p_bg_outline && cells[p_column].bg_color == p_color) {
		return;
	}

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

	if (cells[p_column].custom_button == p_button) {
		return;
	}

	cells.write[p_column].custom_button = p_button;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

bool TreeItem::is_custom_set_as_button(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].custom_button;
}

void TreeItem::set_text_alignment(int p_column, HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].text_alignment == p_alignment) {
		return;
	}

	cells.write[p_column].text_alignment = p_alignment;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

HorizontalAlignment TreeItem::get_text_alignment(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), HORIZONTAL_ALIGNMENT_LEFT);
	return cells[p_column].text_alignment;
}

void TreeItem::set_expand_right(int p_column, bool p_enable) {
	ERR_FAIL_INDEX(p_column, cells.size());

	if (cells[p_column].expand_right == p_enable) {
		return;
	}

	cells.write[p_column].expand_right = p_enable;
	cells.write[p_column].cached_minimum_size_dirty = true;

	_changed_notify(p_column);
}

bool TreeItem::get_expand_right(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, cells.size(), false);
	return cells[p_column].expand_right;
}

void TreeItem::set_disable_folding(bool p_disable) {
	if (disable_folding == p_disable) {
		return;
	}

	disable_folding = p_disable;

	for (Cell &c : cells) {
		c.cached_minimum_size_dirty = true;
	}

	_changed_notify(0);
}

bool TreeItem::is_folding_disabled() const {
	return disable_folding;
}

Size2 TreeItem::get_minimum_size(int p_column) {
	ERR_FAIL_INDEX_V(p_column, cells.size(), Size2());
	Tree *parent_tree = get_tree();
	ERR_FAIL_NULL_V(parent_tree, Size2());

	const TreeItem::Cell &cell = cells[p_column];

	if (cell.cached_minimum_size_dirty) {
		Size2 size = Size2(
				parent_tree->theme_cache.inner_item_margin_left + parent_tree->theme_cache.inner_item_margin_right,
				parent_tree->theme_cache.inner_item_margin_top + parent_tree->theme_cache.inner_item_margin_bottom);

		// Text.
		if (!cell.text.is_empty()) {
			if (cell.dirty) {
				parent_tree->update_item_cell(this, p_column);
			}
			Size2 text_size = cell.text_buf->get_size();
			if (get_text_overrun_behavior(p_column) == TextServer::OVERRUN_NO_TRIMMING) {
				size.width += text_size.width;
			}
			size.height = MAX(size.height, text_size.height);
		}

		// Icon.
		if (cell.mode == CELL_MODE_CHECK) {
			size.width += parent_tree->theme_cache.checked->get_width() + parent_tree->theme_cache.h_separation;
		}
		if (cell.icon.is_valid()) {
			Size2i icon_size = parent_tree->_get_cell_icon_size(cell);
			size.width += icon_size.width + parent_tree->theme_cache.h_separation;
			size.height = MAX(size.height, icon_size.height);
		}

		// Buttons.
		for (int i = 0; i < cell.buttons.size(); i++) {
			Ref<Texture2D> texture = cell.buttons[i].texture;
			if (texture.is_valid()) {
				Size2 button_size = texture->get_size() + parent_tree->theme_cache.button_pressed->get_minimum_size();
				size.width += button_size.width + parent_tree->theme_cache.button_margin;
				size.height = MAX(size.height, button_size.height);
			}
		}

		cells.write[p_column].cached_minimum_size = size;
		cells.write[p_column].cached_minimum_size_dirty = false;
	}

	return cell.cached_minimum_size;
}

void TreeItem::_call_recursive_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return;
	}

	if (!p_args[0]->is_string()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING_NAME;
		return;
	}

	StringName method = *p_args[0];

	call_recursive(method, &p_args[1], p_argcount - 1, r_error);
}

void recursive_call_aux(TreeItem *p_item, const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!p_item) {
		return;
	}
	p_item->callp(p_method, p_args, p_argcount, r_error);
	TreeItem *c = p_item->get_first_child();
	while (c) {
		recursive_call_aux(c, p_method, p_args, p_argcount, r_error);
		c = c->get_next();
	}
}

void TreeItem::call_recursive(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	recursive_call_aux(this, p_method, p_args, p_argcount, r_error);
}

void TreeItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cell_mode", "column", "mode"), &TreeItem::set_cell_mode);
	ClassDB::bind_method(D_METHOD("get_cell_mode", "column"), &TreeItem::get_cell_mode);

	ClassDB::bind_method(D_METHOD("set_auto_translate_mode", "column", "mode"), &TreeItem::set_auto_translate_mode);
	ClassDB::bind_method(D_METHOD("get_auto_translate_mode", "column"), &TreeItem::get_auto_translate_mode);

	ClassDB::bind_method(D_METHOD("set_edit_multiline", "column", "multiline"), &TreeItem::set_edit_multiline);
	ClassDB::bind_method(D_METHOD("is_edit_multiline", "column"), &TreeItem::is_edit_multiline);

	ClassDB::bind_method(D_METHOD("set_checked", "column", "checked"), &TreeItem::set_checked);
	ClassDB::bind_method(D_METHOD("set_indeterminate", "column", "indeterminate"), &TreeItem::set_indeterminate);
	ClassDB::bind_method(D_METHOD("is_checked", "column"), &TreeItem::is_checked);
	ClassDB::bind_method(D_METHOD("is_indeterminate", "column"), &TreeItem::is_indeterminate);

	ClassDB::bind_method(D_METHOD("propagate_check", "column", "emit_signal"), &TreeItem::propagate_check, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("set_text", "column", "text"), &TreeItem::set_text);
	ClassDB::bind_method(D_METHOD("get_text", "column"), &TreeItem::get_text);

	ClassDB::bind_method(D_METHOD("set_text_direction", "column", "direction"), &TreeItem::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction", "column"), &TreeItem::get_text_direction);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "column", "autowrap_mode"), &TreeItem::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode", "column"), &TreeItem::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_text_overrun_behavior", "column", "overrun_behavior"), &TreeItem::set_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("get_text_overrun_behavior", "column"), &TreeItem::get_text_overrun_behavior);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "column", "parser"), &TreeItem::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override", "column"), &TreeItem::get_structured_text_bidi_override);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "column", "args"), &TreeItem::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options", "column"), &TreeItem::get_structured_text_bidi_override_options);

	ClassDB::bind_method(D_METHOD("set_language", "column", "language"), &TreeItem::set_language);
	ClassDB::bind_method(D_METHOD("get_language", "column"), &TreeItem::get_language);

	ClassDB::bind_method(D_METHOD("set_suffix", "column", "text"), &TreeItem::set_suffix);
	ClassDB::bind_method(D_METHOD("get_suffix", "column"), &TreeItem::get_suffix);

	ClassDB::bind_method(D_METHOD("set_icon", "column", "texture"), &TreeItem::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon", "column"), &TreeItem::get_icon);

	ClassDB::bind_method(D_METHOD("set_icon_overlay", "column", "texture"), &TreeItem::set_icon_overlay);
	ClassDB::bind_method(D_METHOD("get_icon_overlay", "column"), &TreeItem::get_icon_overlay);

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

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_custom_draw", "column", "object", "callback"), &TreeItem::set_custom_draw);
#endif // DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_custom_draw_callback", "column", "callback"), &TreeItem::set_custom_draw_callback);
	ClassDB::bind_method(D_METHOD("get_custom_draw_callback", "column"), &TreeItem::get_custom_draw_callback);

	ClassDB::bind_method(D_METHOD("set_collapsed", "enable"), &TreeItem::set_collapsed);
	ClassDB::bind_method(D_METHOD("is_collapsed"), &TreeItem::is_collapsed);

	ClassDB::bind_method(D_METHOD("set_collapsed_recursive", "enable"), &TreeItem::set_collapsed_recursive);
	ClassDB::bind_method(D_METHOD("is_any_collapsed", "only_visible"), &TreeItem::is_any_collapsed, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_visible", "enable"), &TreeItem::set_visible);
	ClassDB::bind_method(D_METHOD("is_visible"), &TreeItem::is_visible);
	ClassDB::bind_method(D_METHOD("is_visible_in_tree"), &TreeItem::is_visible_in_tree);

	ClassDB::bind_method(D_METHOD("uncollapse_tree"), &TreeItem::uncollapse_tree);

	ClassDB::bind_method(D_METHOD("set_custom_minimum_height", "height"), &TreeItem::set_custom_minimum_height);
	ClassDB::bind_method(D_METHOD("get_custom_minimum_height"), &TreeItem::get_custom_minimum_height);

	ClassDB::bind_method(D_METHOD("set_selectable", "column", "selectable"), &TreeItem::set_selectable);
	ClassDB::bind_method(D_METHOD("is_selectable", "column"), &TreeItem::is_selectable);

	ClassDB::bind_method(D_METHOD("is_selected", "column"), &TreeItem::is_selected);
	ClassDB::bind_method(D_METHOD("select", "column"), &TreeItem::select);
	ClassDB::bind_method(D_METHOD("deselect", "column"), &TreeItem::deselect);

	ClassDB::bind_method(D_METHOD("set_editable", "column", "enabled"), &TreeItem::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable", "column"), &TreeItem::is_editable);

	ClassDB::bind_method(D_METHOD("set_custom_color", "column", "color"), &TreeItem::set_custom_color);
	ClassDB::bind_method(D_METHOD("get_custom_color", "column"), &TreeItem::get_custom_color);
	ClassDB::bind_method(D_METHOD("clear_custom_color", "column"), &TreeItem::clear_custom_color);

	ClassDB::bind_method(D_METHOD("set_custom_font", "column", "font"), &TreeItem::set_custom_font);
	ClassDB::bind_method(D_METHOD("get_custom_font", "column"), &TreeItem::get_custom_font);

	ClassDB::bind_method(D_METHOD("set_custom_font_size", "column", "font_size"), &TreeItem::set_custom_font_size);
	ClassDB::bind_method(D_METHOD("get_custom_font_size", "column"), &TreeItem::get_custom_font_size);

	ClassDB::bind_method(D_METHOD("set_custom_bg_color", "column", "color", "just_outline"), &TreeItem::set_custom_bg_color, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("clear_custom_bg_color", "column"), &TreeItem::clear_custom_bg_color);
	ClassDB::bind_method(D_METHOD("get_custom_bg_color", "column"), &TreeItem::get_custom_bg_color);

	ClassDB::bind_method(D_METHOD("set_custom_as_button", "column", "enable"), &TreeItem::set_custom_as_button);
	ClassDB::bind_method(D_METHOD("is_custom_set_as_button", "column"), &TreeItem::is_custom_set_as_button);

	ClassDB::bind_method(D_METHOD("add_button", "column", "button", "id", "disabled", "tooltip_text"), &TreeItem::add_button, DEFVAL(-1), DEFVAL(false), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("get_button_count", "column"), &TreeItem::get_button_count);
	ClassDB::bind_method(D_METHOD("get_button_tooltip_text", "column", "button_index"), &TreeItem::get_button_tooltip_text);
	ClassDB::bind_method(D_METHOD("get_button_id", "column", "button_index"), &TreeItem::get_button_id);
	ClassDB::bind_method(D_METHOD("get_button_by_id", "column", "id"), &TreeItem::get_button_by_id);
	ClassDB::bind_method(D_METHOD("get_button_color", "column", "id"), &TreeItem::get_button_color);
	ClassDB::bind_method(D_METHOD("get_button", "column", "button_index"), &TreeItem::get_button);
	ClassDB::bind_method(D_METHOD("set_button_tooltip_text", "column", "button_index", "tooltip"), &TreeItem::set_button_tooltip_text);
	ClassDB::bind_method(D_METHOD("set_button", "column", "button_index", "button"), &TreeItem::set_button);
	ClassDB::bind_method(D_METHOD("erase_button", "column", "button_index"), &TreeItem::erase_button);
	ClassDB::bind_method(D_METHOD("set_button_disabled", "column", "button_index", "disabled"), &TreeItem::set_button_disabled);
	ClassDB::bind_method(D_METHOD("set_button_color", "column", "button_index", "color"), &TreeItem::set_button_color);
	ClassDB::bind_method(D_METHOD("is_button_disabled", "column", "button_index"), &TreeItem::is_button_disabled);

	ClassDB::bind_method(D_METHOD("set_tooltip_text", "column", "tooltip"), &TreeItem::set_tooltip_text);
	ClassDB::bind_method(D_METHOD("get_tooltip_text", "column"), &TreeItem::get_tooltip_text);
	ClassDB::bind_method(D_METHOD("set_text_alignment", "column", "text_alignment"), &TreeItem::set_text_alignment);
	ClassDB::bind_method(D_METHOD("get_text_alignment", "column"), &TreeItem::get_text_alignment);

	ClassDB::bind_method(D_METHOD("set_expand_right", "column", "enable"), &TreeItem::set_expand_right);
	ClassDB::bind_method(D_METHOD("get_expand_right", "column"), &TreeItem::get_expand_right);

	ClassDB::bind_method(D_METHOD("set_disable_folding", "disable"), &TreeItem::set_disable_folding);
	ClassDB::bind_method(D_METHOD("is_folding_disabled"), &TreeItem::is_folding_disabled);

	ClassDB::bind_method(D_METHOD("create_child", "index"), &TreeItem::create_child, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("add_child", "child"), &TreeItem::add_child);
	ClassDB::bind_method(D_METHOD("remove_child", "child"), &TreeItem::remove_child);

	ClassDB::bind_method(D_METHOD("get_tree"), &TreeItem::get_tree);

	ClassDB::bind_method(D_METHOD("get_next"), &TreeItem::get_next);
	ClassDB::bind_method(D_METHOD("get_prev"), &TreeItem::get_prev);
	ClassDB::bind_method(D_METHOD("get_parent"), &TreeItem::get_parent);
	ClassDB::bind_method(D_METHOD("get_first_child"), &TreeItem::get_first_child);

	ClassDB::bind_method(D_METHOD("get_next_in_tree", "wrap"), &TreeItem::get_next_in_tree, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_prev_in_tree", "wrap"), &TreeItem::get_prev_in_tree, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_next_visible", "wrap"), &TreeItem::get_next_visible, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_prev_visible", "wrap"), &TreeItem::get_prev_visible, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_child", "index"), &TreeItem::get_child);
	ClassDB::bind_method(D_METHOD("get_child_count"), &TreeItem::get_child_count);
	ClassDB::bind_method(D_METHOD("get_children"), &TreeItem::get_children);
	ClassDB::bind_method(D_METHOD("get_index"), &TreeItem::get_index);

	ClassDB::bind_method(D_METHOD("move_before", "item"), &TreeItem::move_before);
	ClassDB::bind_method(D_METHOD("move_after", "item"), &TreeItem::move_after);

	{
		MethodInfo mi;
		mi.name = "call_recursive";
		mi.arguments.push_back(PropertyInfo(Variant::STRING_NAME, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_recursive", &TreeItem::_call_recursive_bind, mi);
	}

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collapsed"), "set_collapsed", "is_collapsed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "is_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_folding"), "set_disable_folding", "is_folding_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "custom_minimum_height", PROPERTY_HINT_RANGE, "0,1000,1"), "set_custom_minimum_height", "get_custom_minimum_height");

	BIND_ENUM_CONSTANT(CELL_MODE_STRING);
	BIND_ENUM_CONSTANT(CELL_MODE_CHECK);
	BIND_ENUM_CONSTANT(CELL_MODE_RANGE);
	BIND_ENUM_CONSTANT(CELL_MODE_ICON);
	BIND_ENUM_CONSTANT(CELL_MODE_CUSTOM);
}

TreeItem::TreeItem(Tree *p_tree) {
	tree = p_tree;
}

TreeItem::~TreeItem() {
	_unlink_from_tree();
	_change_tree(nullptr);

	validate_cache();
	prev = nullptr;
	clear_children();
}

/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/
/**********************************************/

void Tree::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.base_scale = get_theme_default_base_scale();
}

Size2 Tree::_get_cell_icon_size(const TreeItem::Cell &p_cell) const {
	Size2i icon_size = p_cell.get_icon_size();

	int max_width = 0;
	if (theme_cache.icon_max_width > 0) {
		max_width = theme_cache.icon_max_width;
	}
	if (p_cell.icon_max_w > 0 && (max_width == 0 || p_cell.icon_max_w < max_width)) {
		max_width = p_cell.icon_max_w;
	}

	if (max_width > 0 && icon_size.width > max_width) {
		icon_size.height = icon_size.height * max_width / icon_size.width;
		icon_size.width = max_width;
	}

	return icon_size;
}

int Tree::compute_item_height(TreeItem *p_item) const {
	if ((p_item == root && hide_root) || !p_item->is_visible_in_tree()) {
		return 0;
	}

	ERR_FAIL_COND_V(theme_cache.font.is_null(), 0);
	int height = 0;

	for (int i = 0; i < columns.size(); i++) {
		if (p_item->cells[i].dirty) {
			const_cast<Tree *>(this)->update_item_cell(p_item, i);
		}
		height = MAX(height, p_item->cells[i].text_buf->get_size().y);
		for (int j = 0; j < p_item->cells[i].buttons.size(); j++) {
			Size2i s; // = cache.button_pressed->get_minimum_size();
			s += p_item->cells[i].buttons[j].texture->get_size();
			if (s.height > height) {
				height = s.height;
			}
		}

		switch (p_item->cells[i].mode) {
			case TreeItem::CELL_MODE_CHECK: {
				int check_icon_h = theme_cache.checked->get_height();
				if (height < check_icon_h) {
					height = check_icon_h;
				}
				[[fallthrough]];
			}
			case TreeItem::CELL_MODE_STRING:
			case TreeItem::CELL_MODE_CUSTOM:
			case TreeItem::CELL_MODE_ICON: {
				Ref<Texture2D> icon = p_item->cells[i].icon;
				if (!icon.is_null()) {
					Size2i s = _get_cell_icon_size(p_item->cells[i]);
					if (s.height > height) {
						height = s.height;
					}
				}
				if (p_item->cells[i].mode == TreeItem::CELL_MODE_CUSTOM && p_item->cells[i].custom_button) {
					height += theme_cache.custom_button->get_minimum_size().height;
				}

			} break;
			default: {
			}
		}
	}
	int item_min_height = MAX(theme_cache.font->get_height(theme_cache.font_size), p_item->get_custom_minimum_height());
	if (height < item_min_height) {
		height = item_min_height;
	}

	height += theme_cache.v_separation;

	return height;
}

int Tree::get_item_height(TreeItem *p_item) const {
	if (!p_item->is_visible_in_tree()) {
		return 0;
	}
	int height = compute_item_height(p_item);
	height += theme_cache.v_separation;

	if (!p_item->collapsed) { /* if not collapsed, check the children */

		TreeItem *c = p_item->first_child;

		while (c) {
			height += get_item_height(c);

			c = c->next;
		}
	}

	return height;
}

void Tree::draw_item_rect(TreeItem::Cell &p_cell, const Rect2i &p_rect, const Color &p_color, const Color &p_icon_color, int p_ol_size, const Color &p_ol_color) {
	ERR_FAIL_COND(theme_cache.font.is_null());

	Rect2i rect = p_rect.grow_individual(-theme_cache.inner_item_margin_left, -theme_cache.inner_item_margin_top, -theme_cache.inner_item_margin_right, -theme_cache.inner_item_margin_bottom);
	Size2 ts = p_cell.text_buf->get_size();
	bool rtl = is_layout_rtl();

	int w = 0;
	Size2i bmsize;
	if (!p_cell.icon.is_null()) {
		bmsize = _get_cell_icon_size(p_cell);
		w += bmsize.width + theme_cache.h_separation;
		if (rect.size.width > 0 && (w + ts.width) > rect.size.width) {
			ts.width = rect.size.width - w;
		}
	}
	w += ts.width;

	switch (p_cell.text_alignment) {
		case HORIZONTAL_ALIGNMENT_FILL:
		case HORIZONTAL_ALIGNMENT_LEFT: {
			if (rtl) {
				rect.position.x += MAX(0, (rect.size.width - w));
			}
		} break;
		case HORIZONTAL_ALIGNMENT_CENTER:
			rect.position.x += MAX(0, (rect.size.width - w) / 2);
			break;
		case HORIZONTAL_ALIGNMENT_RIGHT:
			if (!rtl) {
				rect.position.x += MAX(0, (rect.size.width - w));
			}
			break;
	}

	RID ci = get_canvas_item();

	if (rtl && rect.size.width > 0) {
		Point2 draw_pos = rect.position;
		draw_pos.y += Math::floor((rect.size.y - p_cell.text_buf->get_size().y) * 0.5);
		if (p_ol_size > 0 && p_ol_color.a > 0) {
			p_cell.text_buf->draw_outline(ci, draw_pos, p_ol_size, p_ol_color);
		}
		p_cell.text_buf->draw(ci, draw_pos, p_color);
		rect.position.x += ts.width + theme_cache.h_separation;
		rect.size.x -= ts.width + theme_cache.h_separation;
	}

	if (!p_cell.icon.is_null()) {
		p_cell.draw_icon(ci, rect.position + Size2i(0, Math::floor((real_t)(rect.size.y - bmsize.y) / 2)), bmsize, p_icon_color);
		rect.position.x += bmsize.x + theme_cache.h_separation;
		rect.size.x -= bmsize.x + theme_cache.h_separation;
	}

	if (!rtl && rect.size.width > 0) {
		Point2 draw_pos = rect.position;
		draw_pos.y += Math::floor((rect.size.y - p_cell.text_buf->get_size().y) * 0.5);
		if (p_ol_size > 0 && p_ol_color.a > 0) {
			p_cell.text_buf->draw_outline(ci, draw_pos, p_ol_size, p_ol_color);
		}
		p_cell.text_buf->draw(ci, draw_pos, p_color);
	}
}

void Tree::update_column(int p_col) {
	columns.write[p_col].text_buf->clear();
	if (columns[p_col].text_direction == Control::TEXT_DIRECTION_INHERITED) {
		columns.write[p_col].text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		columns.write[p_col].text_buf->set_direction((TextServer::Direction)columns[p_col].text_direction);
	}

	columns.write[p_col].xl_title = atr(columns[p_col].title);
	columns.write[p_col].text_buf->add_string(columns[p_col].xl_title, theme_cache.tb_font, theme_cache.tb_font_size, columns[p_col].language);
	columns.write[p_col].cached_minimum_width_dirty = true;
}

void Tree::update_item_cell(TreeItem *p_item, int p_col) {
	String valtext;

	p_item->cells.write[p_col].text_buf->clear();
	if (p_item->cells[p_col].mode == TreeItem::CELL_MODE_RANGE) {
		if (!p_item->cells[p_col].text.is_empty()) {
			if (!p_item->cells[p_col].editable) {
				return;
			}

			int option = (int)p_item->cells[p_col].val;

			valtext = p_item->atr(p_col, ETR("(Other)"));
			Vector<String> strings = p_item->cells[p_col].text.split(",");
			for (int j = 0; j < strings.size(); j++) {
				int value = j;
				if (!strings[j].get_slicec(':', 1).is_empty()) {
					value = strings[j].get_slicec(':', 1).to_int();
				}
				if (option == value) {
					valtext = p_item->atr(p_col, strings[j].get_slicec(':', 0));
					break;
				}
			}

		} else {
			valtext = String::num(p_item->cells[p_col].val, Math::range_step_decimals(p_item->cells[p_col].step));
		}
	} else {
		// Don't auto translate if it's in string mode and editable, as the text can be changed to anything by the user.
		if (!p_item->cells[p_col].editable || p_item->cells[p_col].mode != TreeItem::CELL_MODE_STRING) {
			p_item->cells.write[p_col].xl_text = p_item->atr(p_col, p_item->cells[p_col].text);
		} else {
			p_item->cells.write[p_col].xl_text = p_item->cells[p_col].text;
		}

		valtext = p_item->cells[p_col].xl_text;
	}

	if (!p_item->cells[p_col].suffix.is_empty()) {
		if (!valtext.is_empty()) {
			valtext += " ";
		}
		valtext += p_item->cells[p_col].suffix;
	}

	if (p_item->cells[p_col].text_direction == Control::TEXT_DIRECTION_INHERITED) {
		p_item->cells.write[p_col].text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		p_item->cells.write[p_col].text_buf->set_direction((TextServer::Direction)p_item->cells[p_col].text_direction);
	}

	Ref<Font> font;
	if (p_item->cells[p_col].custom_font.is_valid()) {
		font = p_item->cells[p_col].custom_font;
	} else {
		font = theme_cache.font;
	}

	int font_size;
	if (p_item->cells[p_col].custom_font_size > 0) {
		font_size = p_item->cells[p_col].custom_font_size;
	} else {
		font_size = theme_cache.font_size;
	}
	p_item->cells.write[p_col].text_buf->add_string(valtext, font, font_size, p_item->cells[p_col].language);

	BitField<TextServer::LineBreakFlag> break_flags = TextServer::BREAK_MANDATORY | TextServer::BREAK_TRIM_EDGE_SPACES;
	switch (p_item->cells.write[p_col].autowrap_mode) {
		case TextServer::AUTOWRAP_OFF:
			break;
		case TextServer::AUTOWRAP_ARBITRARY:
			break_flags.set_flag(TextServer::BREAK_GRAPHEME_BOUND);
			break;
		case TextServer::AUTOWRAP_WORD:
			break_flags.set_flag(TextServer::BREAK_WORD_BOUND);
			break;
		case TextServer::AUTOWRAP_WORD_SMART:
			break_flags.set_flag(TextServer::BREAK_WORD_BOUND);
			break_flags.set_flag(TextServer::BREAK_ADAPTIVE);
			break;
	}
	p_item->cells.write[p_col].text_buf->set_break_flags(break_flags);

	TS->shaped_text_set_bidi_override(p_item->cells[p_col].text_buf->get_rid(), structured_text_parser(p_item->cells[p_col].st_parser, p_item->cells[p_col].st_args, valtext));
	p_item->cells.write[p_col].dirty = false;
}

void Tree::update_item_cache(TreeItem *p_item) {
	for (int i = 0; i < p_item->cells.size(); i++) {
		update_item_cell(p_item, i);
	}

	TreeItem *c = p_item->first_child;
	while (c) {
		update_item_cache(c);
		c = c->next;
	}
}

int Tree::draw_item(const Point2i &p_pos, const Point2 &p_draw_ofs, const Size2 &p_draw_size, TreeItem *p_item, int &r_self_height) {
	if (p_pos.y - theme_cache.offset.y > (p_draw_size.height)) {
		return -1; //draw no more!
	}

	if (!p_item->is_visible_in_tree()) {
		return 0;
	}

	RID ci = get_canvas_item();

	int htotal = 0;

	int label_h = 0;
	bool rtl = cache.rtl;

	/* Draw label, if height fits */

	bool skip = (p_item == root && hide_root);

	if (!skip) {
		// Draw separation.

		ERR_FAIL_COND_V(theme_cache.font.is_null(), -1);

		int ofs = p_pos.x + ((p_item->disable_folding || hide_folding) ? theme_cache.h_separation : theme_cache.item_margin);
		int skip2 = 0;
		for (int i = 0; i < columns.size(); i++) {
			if (skip2) {
				skip2--;
				continue;
			}

			int item_width = get_column_width(i);

			if (i == 0) {
				item_width -= ofs;

				if (item_width <= 0) {
					ofs = get_column_width(0);
					continue;
				}
			} else {
				ofs += theme_cache.h_separation;
				item_width -= theme_cache.h_separation;
			}

			if (p_item->cells[i].expand_right) {
				int plus = 1;
				while (i + plus < columns.size() && !p_item->cells[i + plus].editable && p_item->cells[i + plus].mode == TreeItem::CELL_MODE_STRING && p_item->cells[i + plus].xl_text.is_empty() && p_item->cells[i + plus].icon.is_null()) {
					item_width += get_column_width(i + plus);
					plus++;
					skip2++;
				}
			}

			if (!rtl && p_item->cells[i].buttons.size()) {
				int buttons_width = 0;
				for (int j = p_item->cells[i].buttons.size() - 1; j >= 0; j--) {
					Ref<Texture2D> button_texture = p_item->cells[i].buttons[j].texture;
					buttons_width += button_texture->get_size().width + theme_cache.button_pressed->get_minimum_size().width + theme_cache.button_margin;
				}

				int total_ofs = ofs - theme_cache.offset.x;

				if (total_ofs + item_width > p_draw_size.width) {
					item_width = MAX(buttons_width, p_draw_size.width - total_ofs);
				}
			}

			int item_width_with_buttons = item_width; // used later for drawing buttons
			int buttons_width = 0;
			for (int j = p_item->cells[i].buttons.size() - 1; j >= 0; j--) {
				Ref<Texture2D> button_texture = p_item->cells[i].buttons[j].texture;
				Size2 button_size = button_texture->get_size() + theme_cache.button_pressed->get_minimum_size();

				item_width -= button_size.width + theme_cache.button_margin;
				buttons_width += button_size.width + theme_cache.button_margin;
			}

			int text_width = item_width - theme_cache.inner_item_margin_left - theme_cache.inner_item_margin_right;
			if (p_item->cells[i].icon.is_valid()) {
				text_width -= _get_cell_icon_size(p_item->cells[i]).x + theme_cache.h_separation;
			}

			p_item->cells.write[i].text_buf->set_width(text_width);

			r_self_height = compute_item_height(p_item);
			label_h = r_self_height + theme_cache.v_separation;

			if (p_pos.y + label_h - theme_cache.offset.y < 0) {
				continue; // No need to draw.
			}

			Rect2i item_rect = Rect2i(Point2i(ofs, p_pos.y) - theme_cache.offset + p_draw_ofs, Size2i(item_width, label_h));
			Rect2i cell_rect = item_rect;
			if (i != 0) {
				cell_rect.position.x -= theme_cache.h_separation;
				cell_rect.size.x += theme_cache.h_separation;
			}

			if (theme_cache.draw_guides) {
				Rect2 r = cell_rect;
				if (rtl) {
					r.position.x = get_size().width - r.position.x - r.size.x;
				}
				RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(r.position.x, r.position.y + r.size.height), r.position + r.size, theme_cache.guide_color, 1);
			}

			if (i == 0) {
				if (p_item->cells[0].selected && select_mode == SELECT_ROW) {
					const Rect2 content_rect = _get_content_rect();
					Rect2i row_rect = Rect2i(Point2i(content_rect.position.x, item_rect.position.y), Size2i(content_rect.size.x, item_rect.size.y));
					if (rtl) {
						row_rect.position.x = get_size().width - row_rect.position.x - row_rect.size.x;
					}
					if (has_focus()) {
						theme_cache.selected_focus->draw(ci, row_rect);
					} else {
						theme_cache.selected->draw(ci, row_rect);
					}
				}
			}

			if ((select_mode == SELECT_ROW && selected_item == p_item) || p_item->cells[i].selected || !p_item->has_meta("__focus_rect")) {
				Rect2i r = cell_rect;

				if (select_mode != SELECT_ROW) {
					p_item->set_meta("__focus_rect", Rect2(r.position, r.size));
					if (rtl) {
						r.position.x = get_size().width - r.position.x - r.size.x;
					}
					if (p_item->cells[i].selected) {
						if (has_focus()) {
							theme_cache.selected_focus->draw(ci, r);
						} else {
							theme_cache.selected->draw(ci, r);
						}
					}
				} else {
					p_item->set_meta("__focus_col_" + itos(i), Rect2(r.position, r.size));
				}
			}

			if (p_item->cells[i].custom_bg_color) {
				Rect2 r = cell_rect;
				if (i == 0) {
					r.position.x = p_draw_ofs.x;
					r.size.x = item_width + ofs;
				} else {
					r.position.x -= theme_cache.h_separation;
					r.size.x += theme_cache.h_separation;
				}
				if (rtl) {
					r.position.x = get_size().width - r.position.x - r.size.x;
				}
				if (p_item->cells[i].custom_bg_outline) {
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), p_item->cells[i].bg_color);
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y + r.size.y - 1, r.size.x, 1), p_item->cells[i].bg_color);
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, 1, r.size.y), p_item->cells[i].bg_color);
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x + r.size.x - 1, r.position.y, 1, r.size.y), p_item->cells[i].bg_color);
				} else {
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, r, p_item->cells[i].bg_color);
				}
			}

			if (drop_mode_flags && drop_mode_over) {
				Rect2 r = cell_rect;
				if (rtl) {
					r.position.x = get_size().width - r.position.x - r.size.x;
				}
				if (drop_mode_over == p_item) {
					if (drop_mode_section == 0 || drop_mode_section == -1) {
						// Line above.
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), theme_cache.drop_position_color);
					}
					if (drop_mode_section == 0) {
						// Side lines.
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, 1, r.size.y), theme_cache.drop_position_color);
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x + r.size.x - 1, r.position.y, 1, r.size.y), theme_cache.drop_position_color);
					}
					if (drop_mode_section == 0 || (drop_mode_section == 1 && (!p_item->get_first_child() || p_item->is_collapsed()))) {
						// Line below.
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y + r.size.y, r.size.x, 1), theme_cache.drop_position_color);
					}
				} else if (drop_mode_over == p_item->get_parent()) {
					if (drop_mode_section == 1 && !p_item->get_prev() /* && !drop_mode_over->is_collapsed() */) { // The drop_mode_over shouldn't ever be collapsed in here, otherwise we would be drawing a child of a collapsed item.
						// Line above.
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(r.position.x, r.position.y, r.size.x, 1), theme_cache.drop_position_color);
					}
				}
			}

			Color cell_color;
			if (p_item->cells[i].custom_color) {
				cell_color = p_item->cells[i].color;
			} else {
				cell_color = p_item->cells[i].selected ? theme_cache.font_selected_color : theme_cache.font_color;
			}

			Color font_outline_color = theme_cache.font_outline_color;
			int outline_size = theme_cache.font_outline_size;
			Color icon_col = p_item->cells[i].icon_color;

			if (p_item->cells[i].dirty) {
				const_cast<Tree *>(this)->update_item_cell(p_item, i);
			}

			if (rtl) {
				item_rect.position.x = get_size().width - item_rect.position.x - item_rect.size.x;
			}

			Point2i text_pos = item_rect.position;
			text_pos.y += Math::floor(p_draw_ofs.y) - _get_title_button_height();

			switch (p_item->cells[i].mode) {
				case TreeItem::CELL_MODE_STRING: {
					draw_item_rect(p_item->cells.write[i], item_rect, cell_color, icon_col, outline_size, font_outline_color);
				} break;
				case TreeItem::CELL_MODE_CHECK: {
					Point2i check_ofs = item_rect.position;
					check_ofs.y += Math::floor((real_t)(item_rect.size.y - theme_cache.checked->get_height()) / 2);

					if (p_item->cells[i].editable) {
						if (p_item->cells[i].indeterminate) {
							theme_cache.indeterminate->draw(ci, check_ofs);
						} else if (p_item->cells[i].checked) {
							theme_cache.checked->draw(ci, check_ofs);
						} else {
							theme_cache.unchecked->draw(ci, check_ofs);
						}
					} else {
						if (p_item->cells[i].indeterminate) {
							theme_cache.indeterminate_disabled->draw(ci, check_ofs);
						} else if (p_item->cells[i].checked) {
							theme_cache.checked_disabled->draw(ci, check_ofs);
						} else {
							theme_cache.unchecked_disabled->draw(ci, check_ofs);
						}
					}

					int check_w = theme_cache.checked->get_width() + theme_cache.h_separation;

					text_pos.x += check_w;

					item_rect.size.x -= check_w;
					item_rect.position.x += check_w;

					if (!p_item->cells[i].editable) {
						cell_color = theme_cache.font_disabled_color;
					}

					draw_item_rect(p_item->cells.write[i], item_rect, cell_color, icon_col, outline_size, font_outline_color);

				} break;
				case TreeItem::CELL_MODE_RANGE: {
					if (!p_item->cells[i].text.is_empty()) {
						if (!p_item->cells[i].editable) {
							break;
						}

						Ref<Texture2D> downarrow = theme_cache.select_arrow;
						int cell_width = item_rect.size.x - downarrow->get_width();

						if (rtl) {
							if (outline_size > 0 && font_outline_color.a > 0) {
								p_item->cells[i].text_buf->draw_outline(ci, text_pos + Vector2(cell_width - text_width, 0), outline_size, font_outline_color);
							}
							p_item->cells[i].text_buf->draw(ci, text_pos + Vector2(cell_width - text_width, 0), cell_color);
						} else {
							if (outline_size > 0 && font_outline_color.a > 0) {
								p_item->cells[i].text_buf->draw_outline(ci, text_pos, outline_size, font_outline_color);
							}
							p_item->cells[i].text_buf->draw(ci, text_pos, cell_color);
						}

						Point2i arrow_pos = item_rect.position;
						arrow_pos.x += item_rect.size.x - downarrow->get_width();
						arrow_pos.y += Math::floor(((item_rect.size.y - downarrow->get_height())) / 2.0);

						downarrow->draw(ci, arrow_pos);
					} else {
						Ref<Texture2D> updown = theme_cache.updown;

						int cell_width = item_rect.size.x - updown->get_width();

						if (rtl) {
							if (outline_size > 0 && font_outline_color.a > 0) {
								p_item->cells[i].text_buf->draw_outline(ci, text_pos + Vector2(cell_width - text_width, 0), outline_size, font_outline_color);
							}
							p_item->cells[i].text_buf->draw(ci, text_pos + Vector2(cell_width - text_width, 0), cell_color);
						} else {
							if (outline_size > 0 && font_outline_color.a > 0) {
								p_item->cells[i].text_buf->draw_outline(ci, text_pos, outline_size, font_outline_color);
							}
							p_item->cells[i].text_buf->draw(ci, text_pos, cell_color);
						}

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
					Size2i icon_size = _get_cell_icon_size(p_item->cells[i]);
					Point2i icon_ofs = (item_rect.size - icon_size) / 2;
					icon_ofs += item_rect.position;

					draw_texture_rect(p_item->cells[i].icon, Rect2(icon_ofs, icon_size), false, icon_col);

				} break;
				case TreeItem::CELL_MODE_CUSTOM: {
					if (p_item->cells[i].custom_draw_callback.is_valid()) {
						Variant args[] = { p_item, Rect2(item_rect) };
						const Variant *argptrs[] = { &args[0], &args[1] };

						Callable::CallError ce;
						Variant ret;
						p_item->cells[i].custom_draw_callback.callp(argptrs, 2, ret, ce);
						if (ce.error != Callable::CallError::CALL_OK) {
							ERR_PRINT("Error calling custom draw method: " + Variant::get_callable_error_text(p_item->cells[i].custom_draw_callback, argptrs, 2, ce) + ".");
						}
					}

					if (!p_item->cells[i].editable) {
						draw_item_rect(p_item->cells.write[i], item_rect, cell_color, icon_col, outline_size, font_outline_color);
						break;
					}

					Ref<Texture2D> downarrow = theme_cache.select_arrow;

					Rect2i ir = item_rect;

					Point2i arrow_pos = item_rect.position;
					arrow_pos.x += item_rect.size.x - downarrow->get_width();
					arrow_pos.y += Math::floor(((item_rect.size.y - downarrow->get_height())) / 2.0);
					ir.size.width -= downarrow->get_width();

					if (p_item->cells[i].custom_button) {
						if (cache.hover_item == p_item && cache.hover_cell == i) {
							if (Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
								draw_style_box(theme_cache.custom_button_pressed, ir);
							} else {
								draw_style_box(theme_cache.custom_button_hover, ir);
								cell_color = theme_cache.custom_button_font_highlight;
							}
						} else {
							draw_style_box(theme_cache.custom_button, ir);
						}
						ir.size -= theme_cache.custom_button->get_minimum_size();
						ir.position += theme_cache.custom_button->get_offset();
					}

					draw_item_rect(p_item->cells.write[i], ir, cell_color, icon_col, outline_size, font_outline_color);

					downarrow->draw(ci, arrow_pos);

				} break;
			}

			for (int j = p_item->cells[i].buttons.size() - 1; j >= 0; j--) {
				Ref<Texture2D> button_texture = p_item->cells[i].buttons[j].texture;
				Size2 button_size = button_texture->get_size() + theme_cache.button_pressed->get_minimum_size();

				Point2i button_ofs = Point2i(ofs + item_width_with_buttons - button_size.width, p_pos.y) - theme_cache.offset + p_draw_ofs;

				if (cache.click_type == Cache::CLICK_BUTTON && cache.click_item == p_item && cache.click_column == i && cache.click_index == j && !p_item->cells[i].buttons[j].disabled) {
					// Being pressed.
					Point2 od = button_ofs;
					if (rtl) {
						od.x = get_size().width - od.x - button_size.x;
					}
					theme_cache.button_pressed->draw(get_canvas_item(), Rect2(od.x, od.y, button_size.width, MAX(button_size.height, label_h)));
				}

				button_ofs.y += (label_h - button_size.height) / 2;
				button_ofs += theme_cache.button_pressed->get_offset();

				if (rtl) {
					button_ofs.x = get_size().width - button_ofs.x - button_texture->get_width();
				}
				button_texture->draw(ci, button_ofs, p_item->cells[i].buttons[j].disabled ? Color(1, 1, 1, 0.5) : p_item->cells[i].buttons[j].color);
				item_width_with_buttons -= button_size.width + theme_cache.button_margin;
			}

			if (i == 0) {
				ofs = get_column_width(0);
			} else {
				ofs += item_width + buttons_width;
			}

			if (select_mode == SELECT_MULTI && selected_item == p_item && selected_col == i) {
				if (is_layout_rtl()) {
					cell_rect.position.x = get_size().width - cell_rect.position.x - cell_rect.size.x;
				}
				if (has_focus()) {
					theme_cache.cursor->draw(ci, cell_rect);
				} else {
					theme_cache.cursor_unfocus->draw(ci, cell_rect);
				}
			}
		}

		if (!p_item->disable_folding && !hide_folding && p_item->first_child && p_item->get_visible_child_count() != 0) { //has visible children, draw the guide box

			Ref<Texture2D> arrow;

			if (p_item->collapsed) {
				if (is_layout_rtl()) {
					arrow = theme_cache.arrow_collapsed_mirrored;
				} else {
					arrow = theme_cache.arrow_collapsed;
				}
			} else {
				arrow = theme_cache.arrow;
			}

			Point2 apos = p_pos + Point2i(0, (label_h - arrow->get_height()) / 2) - theme_cache.offset + p_draw_ofs;
			apos.x += theme_cache.item_margin - arrow->get_width();

			if (rtl) {
				apos.x = get_size().width - apos.x - arrow->get_width();
			}

			arrow->draw(ci, apos);
		}
	}

	Point2 children_pos = p_pos;

	if (!skip) {
		children_pos.x += theme_cache.item_margin;
		htotal += label_h;
		children_pos.y += htotal;
	}

	if (!p_item->collapsed) { /* if not collapsed, check the children */
		TreeItem *c = p_item->first_child;

		int base_ofs = children_pos.y - theme_cache.offset.y + p_draw_ofs.y;
		int prev_ofs = base_ofs;
		int prev_hl_ofs = base_ofs;

		while (c) {
			int child_h = -1;
			int child_self_height = 0;
			if (htotal >= 0) {
				child_h = draw_item(children_pos, p_draw_ofs, p_draw_size, c, child_self_height);
				child_self_height += theme_cache.v_separation;
			}

			// Draw relationship lines.
			if (theme_cache.draw_relationship_lines > 0 && (!hide_root || c->parent != root) && c->is_visible_in_tree()) {
				int root_ofs = children_pos.x + ((p_item->disable_folding || hide_folding) ? theme_cache.h_separation : theme_cache.item_margin);
				int parent_ofs = p_pos.x + theme_cache.item_margin;
				Point2i root_pos = Point2i(root_ofs, children_pos.y + child_self_height / 2) - theme_cache.offset + p_draw_ofs;

				if (c->get_visible_child_count() > 0) {
					root_pos -= Point2i(theme_cache.arrow->get_width(), 0);
				}

				float line_width = theme_cache.relationship_line_width * Math::round(theme_cache.base_scale);
				float parent_line_width = theme_cache.parent_hl_line_width * Math::round(theme_cache.base_scale);
				float children_line_width = theme_cache.children_hl_line_width * Math::round(theme_cache.base_scale);

				Point2i parent_pos = Point2i(parent_ofs - theme_cache.arrow->get_width() / 2, p_pos.y + label_h / 2 + theme_cache.arrow->get_height() / 2) - theme_cache.offset + p_draw_ofs;

				int more_prev_ofs = 0;

				if (root_pos.y + line_width >= 0) {
					if (rtl) {
						root_pos.x = get_size().width - root_pos.x;
						parent_pos.x = get_size().width - parent_pos.x;
					}

					// Order of parts on this bend: the horizontal line first, then the vertical line.
					if (_is_branch_selected(c)) {
						// If this item or one of its children is selected, we draw the line using parent highlight style.
						if (htotal >= 0) {
							RenderingServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x + Math::floor(parent_line_width / 2), root_pos.y), theme_cache.parent_hl_line_color, parent_line_width);
						}
						RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y + Math::floor(parent_line_width / 2)), Point2i(parent_pos.x, prev_hl_ofs), theme_cache.parent_hl_line_color, parent_line_width);

						more_prev_ofs = theme_cache.parent_hl_line_margin;
						prev_hl_ofs = root_pos.y + Math::floor(parent_line_width / 2);
					} else if (p_item->is_selected(0)) {
						// If parent item is selected (but this item is not), we draw the line using children highlight style.
						// Siblings of the selected branch can be drawn with a slight offset and their vertical line must appear as highlighted.
						if (_is_sibling_branch_selected(c)) {
							if (htotal >= 0) {
								RenderingServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x + Math::floor(parent_line_width / 2), root_pos.y), theme_cache.children_hl_line_color, children_line_width);
							}
							RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y + Math::floor(parent_line_width / 2)), Point2i(parent_pos.x, prev_hl_ofs), theme_cache.parent_hl_line_color, parent_line_width);

							prev_hl_ofs = root_pos.y + Math::floor(parent_line_width / 2);
						} else {
							if (htotal >= 0) {
								RenderingServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x + Math::floor(children_line_width / 2), root_pos.y), theme_cache.children_hl_line_color, children_line_width);
							}
							RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y + Math::floor(children_line_width / 2)), Point2i(parent_pos.x, prev_ofs + Math::floor(children_line_width / 2)), theme_cache.children_hl_line_color, children_line_width);
						}
					} else {
						// If nothing of the above is true, we draw the line using normal style.
						// Siblings of the selected branch can be drawn with a slight offset and their vertical line must appear as highlighted.
						if (_is_sibling_branch_selected(c)) {
							if (htotal >= 0) {
								RenderingServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x + theme_cache.parent_hl_line_margin, root_pos.y), theme_cache.relationship_line_color, line_width);
							}
							RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y + Math::floor(parent_line_width / 2)), Point2i(parent_pos.x, prev_hl_ofs), theme_cache.parent_hl_line_color, parent_line_width);

							prev_hl_ofs = root_pos.y + Math::floor(parent_line_width / 2);
						} else {
							if (htotal >= 0) {
								RenderingServer::get_singleton()->canvas_item_add_line(ci, root_pos, Point2i(parent_pos.x + Math::floor(line_width / 2), root_pos.y), theme_cache.relationship_line_color, line_width);
							}
							RenderingServer::get_singleton()->canvas_item_add_line(ci, Point2i(parent_pos.x, root_pos.y + Math::floor(line_width / 2)), Point2i(parent_pos.x, prev_ofs + Math::floor(line_width / 2)), theme_cache.relationship_line_color, line_width);
						}
					}
				}

				prev_ofs = root_pos.y + more_prev_ofs;
			}

			if (child_h < 0) {
				if (htotal == -1) {
					break; // Last loop done, stop.
				}

				if (theme_cache.draw_relationship_lines == 0) {
					return -1; // No need to draw anymore, full stop.
				}

				htotal = -1;
				children_pos.y = theme_cache.offset.y + p_draw_size.height;
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

	for (TreeItem *c = p_from->get_first_child(); c; c = c->get_next()) {
		count += _count_selected_items(c);
	}

	return count;
}

bool Tree::_is_branch_selected(TreeItem *p_from) const {
	for (int i = 0; i < columns.size(); i++) {
		if (p_from->is_selected(i)) {
			return true;
		}
	}

	TreeItem *child_item = p_from->get_first_child();
	while (child_item) {
		if (_is_branch_selected(child_item)) {
			return true;
		}
		child_item = child_item->get_next();
	}

	return false;
}

bool Tree::_is_sibling_branch_selected(TreeItem *p_from) const {
	TreeItem *sibling_item = p_from->get_next();
	while (sibling_item) {
		if (_is_branch_selected(sibling_item)) {
			return true;
		}
		sibling_item = sibling_item->get_next();
	}

	return false;
}

void Tree::select_single_item(TreeItem *p_selected, TreeItem *p_current, int p_col, TreeItem *p_prev, bool *r_in_range, bool p_force_deselect) {
	popup_editor->hide();

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
				if (!emitted_row) {
					emit_signal(SceneStringName(item_selected));
					emitted_row = true;
				}
			} else if (c.selected) {
				if (p_selected != p_current) {
					// Deselect other rows.
					c.selected = false;
				}
			}
			if (&selected_cell == &c) {
				selected_col = i;
			}
		} else if (select_mode == SELECT_SINGLE || select_mode == SELECT_MULTI) {
			if (!r_in_range && &selected_cell == &c) {
				if (!selected_cell.selected || allow_reselect) {
					selected_cell.selected = true;

					selected_item = p_selected;
					selected_col = i;

					emit_signal(SNAME("cell_selected"));
					if (select_mode == SELECT_MULTI) {
						emit_signal(SNAME("multi_selected"), p_current, i, true);
					} else if (select_mode == SELECT_SINGLE) {
						emit_signal(SceneStringName(item_selected));
					}

				} else if (select_mode == SELECT_MULTI && (selected_item != p_selected || selected_col != i)) {
					selected_item = p_selected;
					selected_col = i;
					emit_signal(SNAME("cell_selected"));
				}
			} else {
				if (r_in_range && *r_in_range && !p_force_deselect) {
					if (!c.selected && c.selectable) {
						c.selected = true;
						emit_signal(SNAME("multi_selected"), p_current, i, true);
					}

				} else if (!r_in_range || p_force_deselect) {
					if (select_mode == SELECT_MULTI && c.selected) {
						emit_signal(SNAME("multi_selected"), p_current, i, false);
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

	TreeItem *c = p_current->first_child;

	while (c) {
		select_single_item(p_selected, c, p_col, p_prev, r_in_range, p_current->is_collapsed() || p_force_deselect);
		c = c->next;
	}
}

Rect2 Tree::search_item_rect(TreeItem *p_from, TreeItem *p_item) {
	return Rect2();
}

void Tree::_range_click_timeout() {
	if (range_item_last && !range_drag_enabled && Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
		Point2 pos = get_local_mouse_position() - theme_cache.panel_style->get_offset();
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
		mb.instantiate();

		int x_limit = get_size().width - theme_cache.panel_style->get_minimum_size().width;
		if (v_scroll->is_visible()) {
			x_limit -= v_scroll->get_minimum_size().width;
		}

		cache.rtl = is_layout_rtl();

		propagate_mouse_activated = false; // done from outside, so signal handler can't clear the tree in the middle of emit (which is a common case)
		blocked++;
		propagate_mouse_event(pos + theme_cache.offset, 0, 0, x_limit + theme_cache.offset.width, false, root, MouseButton::LEFT, mb);
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
			emit_signal(SNAME("item_activated"));
			propagate_mouse_activated = false;
		}

	} else {
		range_click_timer->stop();
	}
}

int Tree::propagate_mouse_event(const Point2i &p_pos, int x_ofs, int y_ofs, int x_limit, bool p_double_click, TreeItem *p_item, MouseButton p_button, const Ref<InputEventWithModifiers> &p_mod) {
	if (p_item && !p_item->is_visible_in_tree()) {
		// Skip any processing of invisible items.
		return 0;
	}

	int item_h = compute_item_height(p_item) + theme_cache.v_separation;

	bool skip = (p_item == root && hide_root);

	if (!skip && p_pos.y < item_h) {
		// check event!

		if (range_click_timer->get_time_left() > 0 && p_item != range_item_last) {
			return -1;
		}

		if (!p_item->disable_folding && !hide_folding && p_item->first_child && (p_pos.x < (x_ofs + theme_cache.item_margin))) {
			if (enable_recursive_folding && p_mod->is_shift_pressed()) {
				p_item->set_collapsed_recursive(!p_item->is_collapsed());
			} else {
				p_item->set_collapsed(!p_item->is_collapsed());
			}
			return -1;
		}

		int x = p_pos.x;
		/* find clicked column */
		int col = -1;
		int col_ofs = 0;
		int col_width = 0;

		int limit_w = x_limit;

		for (int i = 0; i < columns.size(); i++) {
			col_width = get_column_width(i);

			if (p_item->cells[i].expand_right) {
				int plus = 1;
				while (i + plus < columns.size() && !p_item->cells[i + plus].editable && p_item->cells[i + plus].mode == TreeItem::CELL_MODE_STRING && p_item->cells[i + plus].text.is_empty() && p_item->cells[i + plus].icon.is_null()) {
					col_width += theme_cache.h_separation;
					col_width += get_column_width(i + plus);
					plus++;
				}
			}

			if (x > col_width) {
				col_ofs += col_width;
				x -= col_width;
				limit_w -= col_width;
				continue;
			}

			col = i;
			break;
		}

		if (col == -1) {
			return -1;
		} else if (col == 0) {
			int margin = x_ofs + theme_cache.item_margin; //-theme_cache.h_separation;
			//int lm = theme_cache.panel_style->get_margin(SIDE_LEFT);
			col_width -= margin;
			limit_w -= margin;
			col_ofs += margin;
			x -= margin;
		} else {
			col_width -= theme_cache.h_separation;
			limit_w -= theme_cache.h_separation;
			x -= theme_cache.h_separation;
		}

		const TreeItem::Cell &c = p_item->cells[col];

		if (!cache.rtl && !p_item->cells[col].buttons.is_empty()) {
			int button_w = 0;
			for (int j = p_item->cells[col].buttons.size() - 1; j >= 0; j--) {
				Ref<Texture2D> b = p_item->cells[col].buttons[j].texture;
				button_w += b->get_size().width + theme_cache.button_pressed->get_minimum_size().width + theme_cache.button_margin;
			}

			col_width = MAX(button_w, MIN(limit_w, col_width));
		}

		for (int j = c.buttons.size() - 1; j >= 0; j--) {
			Ref<Texture2D> b = c.buttons[j].texture;
			int w = b->get_size().width + theme_cache.button_pressed->get_minimum_size().width;

			if (x > col_width - w) {
				if (c.buttons[j].disabled) {
					pressed_button = -1;
					cache.click_type = Cache::CLICK_NONE;
					return -1;
				}

				// Make sure the click is correct.
				Point2 click_pos = get_global_mouse_position() - get_global_position();
				if (!get_item_at_position(click_pos)) {
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
				cache.click_pos = click_pos;
				queue_redraw();
				return -1;
			}

			col_width -= w + theme_cache.button_margin;
		}

		if (!p_item->disable_folding && !hide_folding && !p_item->cells[col].editable && !p_item->cells[col].selectable && p_item->get_first_child()) {
			if (enable_recursive_folding && p_mod->is_shift_pressed()) {
				p_item->set_collapsed_recursive(!p_item->is_collapsed());
			} else {
				p_item->set_collapsed(!p_item->is_collapsed());
			}
			return -1; // Collapse/uncollapse, because nothing can be done with the item.
		}

		bool already_selected = c.selected;
		bool already_cursor = (p_item == selected_item) && col == selected_col;

		if (p_button == MouseButton::LEFT || (p_button == MouseButton::RIGHT && allow_rmb_select)) {
			/* process selection */

			if (p_double_click && (!c.editable || c.mode == TreeItem::CELL_MODE_CUSTOM || c.mode == TreeItem::CELL_MODE_ICON /*|| c.mode==TreeItem::CELL_MODE_CHECK*/)) { //it's confusing for check
				// Emits the "item_activated" signal.
				propagate_mouse_activated = true;

				incr_search.clear();
				return -1;
			}

			if (c.selectable) {
				if (select_mode == SELECT_MULTI && p_mod->is_command_or_control_pressed()) {
					if (c.selected && p_button == MouseButton::LEFT) {
						p_item->deselect(col);
						emit_signal(SNAME("multi_selected"), p_item, col, false);
					} else {
						p_item->select(col);
						emit_signal(SNAME("multi_selected"), p_item, col, true);
						emit_signal(SNAME("item_mouse_selected"), get_local_mouse_position(), p_button);
					}
				} else {
					if (select_mode == SELECT_MULTI && p_mod->is_shift_pressed() && selected_item && selected_item != p_item) {
						bool inrange = false;

						select_single_item(p_item, root, col, selected_item, &inrange);
						emit_signal(SNAME("item_mouse_selected"), get_local_mouse_position(), p_button);
					} else {
						int icount = _count_selected_items(root);

						if (select_mode == SELECT_MULTI && icount > 1 && p_button != MouseButton::RIGHT) {
							single_select_defer = p_item;
							single_select_defer_column = col;
						} else {
							if (p_button != MouseButton::RIGHT || !c.selected) {
								select_single_item(p_item, root, col);
							}

							emit_signal(SNAME("item_mouse_selected"), get_local_mouse_position(), p_button);
						}
					}
					queue_redraw();
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

				if (select_mode == SELECT_MULTI && (get_viewport()->get_processed_events_count() == focus_in_id || !already_cursor)) {
					bring_up_editor = false;
				}

			} break;
			case TreeItem::CELL_MODE_CHECK: {
				bring_up_editor = false; //checkboxes are not edited with editor
				if (force_edit_checkbox_only_on_checkbox) {
					if (x < theme_cache.checked->get_width()) {
						p_item->set_checked(col, !c.checked);
						item_edited(col, p_item, p_button);
					}
				} else {
					p_item->set_checked(col, !c.checked);
					item_edited(col, p_item, p_button);
				}
				click_handled = true;
				//p_item->edited_signal.call(col);

			} break;
			case TreeItem::CELL_MODE_RANGE: {
				if (!c.text.is_empty()) {
					//if (x >= (get_column_width(col)-item_h/2)) {
					popup_menu->clear();
					for (int i = 0; i < c.text.get_slice_count(","); i++) {
						String s = c.text.get_slicec(',', i);
						popup_menu->add_item(s.get_slicec(':', 0), s.get_slicec(':', 1).is_empty() ? i : s.get_slicec(':', 1).to_int());
					}

					popup_menu->set_size(Size2(col_width, 0));
					popup_menu->set_position(get_screen_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs + item_h) - theme_cache.offset);
					popup_menu->popup();
					popup_edited_item = p_item;
					popup_edited_item_col = col;
					//}
					bring_up_editor = false;
				} else {
					if (x >= (col_width - item_h / 2)) {
						/* touching the combo */
						bool up = p_pos.y < (item_h / 2);

						if (p_button == MouseButton::LEFT) {
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

							item_edited(col, p_item, p_button);

						} else if (p_button == MouseButton::RIGHT) {
							p_item->set_range(col, (up ? c.max : c.min));
							item_edited(col, p_item, p_button);
						} else if (p_button == MouseButton::WHEEL_UP) {
							p_item->set_range(col, c.val + c.step);
							item_edited(col, p_item, p_button);
						} else if (p_button == MouseButton::WHEEL_DOWN) {
							p_item->set_range(col, c.val - c.step);
							item_edited(col, p_item, p_button);
						}

						//p_item->edited_signal.call(col);
						bring_up_editor = false;

					} else {
						editor_text = String::num(p_item->cells[col].val, Math::range_step_decimals(p_item->cells[col].step));
						if (select_mode == SELECT_MULTI && get_viewport()->get_processed_events_count() == focus_in_id) {
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
				bool on_arrow = x > col_width - theme_cache.select_arrow->get_width();

				custom_popup_rect = Rect2i(get_global_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs + item_h - theme_cache.offset.y), Size2(get_column_width(col), item_h));

				if (on_arrow || !p_item->cells[col].custom_button) {
					emit_signal(SNAME("custom_popup_edited"), ((bool)(x >= (col_width - item_h / 2))));
				}

				if (!p_item->cells[col].custom_button || !on_arrow) {
					item_edited(col, p_item, p_button);
				}
				click_handled = true;
				return -1;
			} break;
		};

		if (!bring_up_editor || p_button != MouseButton::LEFT) {
			return -1;
		}

		click_handled = true;
		popup_pressing_edited_item = p_item;
		popup_pressing_edited_item_column = col;

		pressing_item_rect = Rect2(get_global_position() + Point2i(col_ofs, _get_title_button_height() + y_ofs) - theme_cache.offset, Size2(col_width, item_h));
		pressing_for_editor_text = editor_text;
		pressing_for_editor = true;

		return -1; //select
	} else {
		Point2i new_pos = p_pos;

		if (!skip) {
			x_ofs += theme_cache.item_margin;
			//new_pos.x-=theme_cache.item_margin;
			y_ofs += item_h;
			new_pos.y -= item_h;
		}

		if (!p_item->collapsed) { /* if not collapsed, check the children */

			TreeItem *c = p_item->first_child;

			while (c) {
				int child_h = propagate_mouse_event(new_pos, x_ofs, y_ofs, x_limit, p_double_click, c, p_button, p_mod);

				if (child_h < 0) {
					return -1; // break, stop propagating, no need to anymore
				}

				new_pos.y -= child_h;
				y_ofs += child_h;
				c = c->next;
				item_h += child_h;
			}
		}
		if (p_item == root) {
			emit_signal(SNAME("empty_clicked"), get_local_mouse_position(), p_button);
		}
	}

	return item_h; // nothing found
}

void Tree::_text_editor_popup_modal_close() {
	if (popup_edit_commited) {
		return; // Already processed by LineEdit/TextEdit commit.
	}

	if (popup_editor->get_hide_reason() == Popup::HIDE_REASON_CANCELED) {
		return; // ESC pressed, app focus lost, or forced close from code.
	}

	if (value_editor->has_point(value_editor->get_local_mouse_position())) {
		return;
	}

	if (!popup_edited_item) {
		return;
	}

	if (popup_edited_item->is_edit_multiline(popup_edited_item_col) && popup_edited_item->get_cell_mode(popup_edited_item_col) == TreeItem::CELL_MODE_STRING) {
		_apply_multiline_edit();
	} else {
		_line_editor_submit(line_editor->get_text());
	}
}

void Tree::_text_editor_gui_input(const Ref<InputEvent> &p_event) {
	if (popup_edit_commited) {
		return; // Already processed by _text_editor_popup_modal_close
	}

	if (popup_editor->get_hide_reason() == Popup::HIDE_REASON_CANCELED) {
		return; // ESC pressed, app focus lost, or forced close from code.
	}

	if (p_event->is_action_pressed("ui_text_newline_blank", true)) {
		accept_event();
	} else if (p_event->is_action_pressed("ui_text_newline")) {
		popup_edit_commited = true; // End edit popup processing.
		popup_editor->hide();
		_apply_multiline_edit();
		accept_event();
	}
}

void Tree::_apply_multiline_edit() {
	if (!popup_edited_item) {
		return;
	}

	if (popup_edited_item_col < 0 || popup_edited_item_col > columns.size()) {
		return;
	}

	TreeItem::Cell &c = popup_edited_item->cells.write[popup_edited_item_col];
	switch (c.mode) {
		case TreeItem::CELL_MODE_STRING: {
			c.text = text_editor->get_text();
		} break;
		default: {
			ERR_FAIL();
		}
	}

	item_edited(popup_edited_item_col, popup_edited_item);
	queue_redraw();
}

void Tree::_line_editor_submit(String p_text) {
	if (popup_edit_commited) {
		return; // Already processed by _text_editor_popup_modal_close
	}

	if (popup_editor->get_hide_reason() == Popup::HIDE_REASON_CANCELED) {
		return; // ESC pressed, app focus lost, or forced close from code.
	}

	popup_edit_commited = true; // End edit popup processing.
	popup_editor->hide();

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
			c.val = p_text.to_float();
			if (c.step > 0) {
				c.val = Math::snapped(c.val, c.step);
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
	queue_redraw();
}

void Tree::value_editor_changed(double p_value) {
	if (updating_value_editor) {
		return;
	}
	if (!popup_edited_item) {
		return;
	}

	const TreeItem::Cell &c = popup_edited_item->cells[popup_edited_item_col];

	line_editor->set_text(String::num(p_value, Math::range_step_decimals(c.step)));

	queue_redraw();
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
	queue_redraw();
	item_edited(popup_edited_item_col, popup_edited_item);
}

void Tree::_go_left() {
	if (selected_col == 0) {
		if (selected_item->get_first_child() != nullptr && !selected_item->is_collapsed()) {
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
			emit_signal(SNAME("cell_selected"));
		} else {
			selected_item->select(selected_col - 1);
		}
	}
	queue_redraw();
	accept_event();
	ensure_cursor_is_visible();
}

void Tree::_go_right() {
	if (selected_col == (columns.size() - 1)) {
		if (selected_item->get_first_child() != nullptr && selected_item->is_collapsed()) {
			selected_item->set_collapsed(false);
		} else if (selected_item->get_next_visible()) {
			selected_col = 0;
			_go_down();
		}
	} else {
		if (select_mode == SELECT_MULTI) {
			selected_col++;
			emit_signal(SNAME("cell_selected"));
		} else {
			selected_item->select(selected_col + 1);
		}
	}
	queue_redraw();
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
	}

	int col = MAX(selected_col, 0);

	if (select_mode == SELECT_MULTI) {
		if (!prev) {
			return;
		}

		select_single_item(prev, get_root(), col);
		queue_redraw();
	} else {
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
	}

	int col = MAX(selected_col, 0);

	if (select_mode == SELECT_MULTI) {
		if (!next) {
			return;
		}

		select_single_item(next, get_root(), col);
		queue_redraw();
	} else {
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

bool Tree::_scroll(bool p_horizontal, float p_pages) {
	ScrollBar *scroll = p_horizontal ? (ScrollBar *)h_scroll : (ScrollBar *)v_scroll;

	double prev_value = scroll->get_value();
	scroll->set_value(scroll->get_value() + scroll->get_page() * p_pages);

	return scroll->get_value() != prev_value;
}

Rect2 Tree::_get_scrollbar_layout_rect() const {
	const Size2 control_size = get_size();
	const Ref<StyleBox> background = theme_cache.panel_style;

	// This is the background stylebox's content rect.
	const real_t width = control_size.x - background->get_margin(SIDE_LEFT) - background->get_margin(SIDE_RIGHT);
	const real_t height = control_size.y - background->get_margin(SIDE_TOP) - background->get_margin(SIDE_BOTTOM);
	const Rect2 content_rect = Rect2(background->get_offset(), Size2(width, height));

	// Use the stylebox's margins by default. Can be overridden by `scrollbar_margin_*`.
	const real_t top = theme_cache.scrollbar_margin_top < 0 ? content_rect.get_position().y : theme_cache.scrollbar_margin_top;
	const real_t right = theme_cache.scrollbar_margin_right < 0 ? content_rect.get_end().x : (control_size.x - theme_cache.scrollbar_margin_right);
	const real_t bottom = theme_cache.scrollbar_margin_bottom < 0 ? content_rect.get_end().y : (control_size.y - theme_cache.scrollbar_margin_bottom);
	const real_t left = theme_cache.scrollbar_margin_left < 0 ? content_rect.get_position().x : theme_cache.scrollbar_margin_left;

	return Rect2(left, top, right - left, bottom - top);
}

Rect2 Tree::_get_content_rect() const {
	const Size2 control_size = get_size();
	const Ref<StyleBox> background = theme_cache.panel_style;

	// This is the background stylebox's content rect.
	const real_t width = control_size.x - background->get_margin(SIDE_LEFT) - background->get_margin(SIDE_RIGHT);
	const real_t height = control_size.y - background->get_margin(SIDE_TOP) - background->get_margin(SIDE_BOTTOM);
	const Rect2 content_rect = Rect2(background->get_offset(), Size2(width, height));

	// Scrollbars won't affect Tree's content rect if they're not visible or placed inside the stylebox margin area.
	const real_t v_size = v_scroll->is_visible() ? (v_scroll->get_combined_minimum_size().x + theme_cache.scrollbar_h_separation) : 0;
	const real_t h_size = h_scroll->is_visible() ? (h_scroll->get_combined_minimum_size().y + theme_cache.scrollbar_v_separation) : 0;
	const Point2 scroll_begin = _get_scrollbar_layout_rect().get_end() - Vector2(v_size, h_size);
	const Size2 offset = (content_rect.get_end() - scroll_begin).maxf(0);

	return content_rect.grow_individual(0, 0, -offset.x, -offset.y);
}

void Tree::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;

	bool is_command = k.is_valid() && k->is_command_or_control_pressed();
	if (p_event->is_action("ui_right") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		if (!selected_item || selected_col > (columns.size() - 1)) {
			return;
		}

		if (k.is_valid() && k->is_shift_pressed()) {
			selected_item->set_collapsed_recursive(false);
		} else if (select_mode != SELECT_ROW) {
			_go_right();
		} else if (selected_item->get_first_child() != nullptr && selected_item->is_collapsed()) {
			selected_item->set_collapsed(false);
		} else {
			_go_down();
		}
	} else if (p_event->is_action("ui_left") && p_event->is_pressed()) {
		if (!cursor_can_exit_tree) {
			accept_event();
		}

		if (!selected_item || selected_col < 0) {
			return;
		}

		if (k.is_valid() && k->is_shift_pressed()) {
			selected_item->set_collapsed_recursive(true);
		} else if (select_mode != SELECT_ROW) {
			_go_left();
		} else if (selected_item->get_first_child() != nullptr && !selected_item->is_collapsed()) {
			selected_item->set_collapsed(true);
		} else {
			_go_up();
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
			emit_signal(SNAME("cell_selected"));
			queue_redraw();
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
			emit_signal(SNAME("cell_selected"));
			queue_redraw();
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
				emit_signal(SNAME("item_activated"));
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
				emit_signal(SNAME("multi_selected"), selected_item, selected_col, false);
			} else if (selected_item->is_selectable(selected_col)) {
				selected_item->select(selected_col);
				emit_signal(SNAME("multi_selected"), selected_item, selected_col, true);
			}
		}
		accept_event();
	}

	if (allow_search && k.is_valid()) { // Incremental search

		if (!k->is_pressed()) {
			return;
		}
		if (k->is_command_or_control_pressed() || (k->is_shift_pressed() && k->get_unicode() == 0) || k->is_meta_pressed()) {
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
			if (k->get_keycode() != Key::SHIFT) {
				last_keypress = 0;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Ref<StyleBox> bg = theme_cache.panel_style;
		bool rtl = is_layout_rtl();

		Point2 pos = mm->get_position();
		if (rtl) {
			pos.x = get_size().width - pos.x;
		}
		pos -= theme_cache.panel_style->get_offset();

		Cache::ClickType old_hover = cache.hover_type;
		int old_index = cache.hover_index;

		cache.hover_type = Cache::CLICK_NONE;
		cache.hover_index = 0;
		if (show_column_titles) {
			pos.y -= _get_title_button_height();
			if (pos.y < 0) {
				pos.x += theme_cache.offset.x;
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
			if (rtl) {
				mpos.x = get_size().width - mpos.x;
			}
			mpos -= theme_cache.panel_style->get_offset();
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

				int col, h, section;
				TreeItem *it = _find_item_at_pos(root, mpos, col, h, section);

				if (drop_mode_flags) {
					if (it != drop_mode_over) {
						drop_mode_over = it;
						queue_redraw();
					}
					if (it && section != drop_mode_section) {
						drop_mode_section = section;
						queue_redraw();
					}
				}

				cache.hover_item = it;
				cache.hover_cell = col;

				if (it != old_it || col != old_col) {
					if (old_it && old_col >= old_it->cells.size()) {
						// Columns may have changed since last redraw().
						queue_redraw();
					} else {
						// Only need to update if mouse enters/exits a button
						bool was_over_button = old_it && old_it->cells[old_col].custom_button;
						bool is_over_button = it && it->cells[col].custom_button;
						if (was_over_button || is_over_button) {
							queue_redraw();
						}
					}
				}
			}
		}

		// Update if mouse enters/exits columns
		if (cache.hover_type != old_hover || cache.hover_index != old_index) {
			queue_redraw();
		}

		if (pressing_for_editor && popup_pressing_edited_item && (popup_pressing_edited_item->get_cell_mode(popup_pressing_edited_item_column) == TreeItem::CELL_MODE_RANGE)) {
			/* This needs to happen now, because the popup can be closed when pressing another item, and must remain the popup edited item until it actually closes */
			popup_edited_item = popup_pressing_edited_item;
			popup_edited_item_col = popup_pressing_edited_item_column;

			popup_pressing_edited_item = nullptr;
			popup_pressing_edited_item_column = -1;

			if (!range_drag_enabled) {
				//range drag
				Vector2 cpos = mm->get_position();
				if (rtl) {
					cpos.x = get_size().width - cpos.x;
				}
				if (cpos.distance_to(pressing_pos) > 2) {
					range_drag_enabled = true;
					range_drag_capture_pos = cpos;
					range_drag_base = popup_edited_item->get_range(popup_edited_item_col);
					Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
				}
			} else {
				const TreeItem::Cell &c = popup_edited_item->cells[popup_edited_item_col];
				float diff_y = -mm->get_relative().y;
				diff_y = Math::pow(ABS(diff_y), 1.8f) * SIGN(diff_y);
				diff_y *= 0.1;
				range_drag_base = CLAMP(range_drag_base + c.step * diff_y, c.min, c.max);
				popup_edited_item->set_range(popup_edited_item_col, range_drag_base);
				item_edited(popup_edited_item_col, popup_edited_item);
			}
		}

		if (drag_touching && !drag_touching_deaccel) {
			drag_accum -= mm->get_relative().y;
			v_scroll->set_value(drag_from + drag_accum);
			drag_speed = -mm->get_velocity().y;
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		bool rtl = is_layout_rtl();

		if (!mb->is_pressed()) {
			if (mb->get_button_index() == MouseButton::LEFT ||
					mb->get_button_index() == MouseButton::RIGHT) {
				Point2 pos = mb->get_position();
				if (rtl) {
					pos.x = get_size().width - pos.x;
				}
				pos -= theme_cache.panel_style->get_offset();
				if (show_column_titles) {
					pos.y -= _get_title_button_height();

					if (pos.y < 0) {
						pos.x += theme_cache.offset.x;
						int len = 0;
						for (int i = 0; i < columns.size(); i++) {
							len += get_column_width(i);
							if (pos.x < static_cast<real_t>(len)) {
								emit_signal(SNAME("column_title_clicked"), i, mb->get_button_index());
								break;
							}
						}
					}
				}
			}

			if (mb->get_button_index() == MouseButton::LEFT) {
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
						Rect2 rect;
						if (select_mode == SELECT_ROW) {
							rect = get_selected()->get_meta("__focus_col_" + itos(selected_col));
						} else {
							rect = get_selected()->get_meta("__focus_rect");
						}
						Point2 mpos = mb->get_position();
						int icon_size_x = 0;
						Ref<Texture2D> icon = get_selected()->get_icon(selected_col);
						if (icon.is_valid()) {
							Rect2i icon_region = get_selected()->get_icon_region(selected_col);
							if (icon_region == Rect2i()) {
								icon_size_x = icon->get_width();
							} else {
								icon_size_x = icon_region.size.width;
							}
						}
						// Icon is treated as if it is outside of the rect so that double clicking on it will emit the item_icon_double_clicked signal.
						if (rtl) {
							mpos.x = get_size().width - (mpos.x + icon_size_x);
						} else {
							mpos.x -= icon_size_x;
						}
						if (rect.has_point(mpos)) {
							if (!edit_selected()) {
								emit_signal(SNAME("item_icon_double_clicked"));
							}
						} else {
							emit_signal(SNAME("item_icon_double_clicked"));
						}
					}
					pressing_for_editor = false;
				}

				if (drag_touching) {
					if (drag_speed == 0) {
						drag_touching_deaccel = false;
						drag_touching = false;
						set_physics_process_internal(false);
					} else {
						drag_touching_deaccel = true;
					}
				}
			}

			if (cache.click_type == Cache::CLICK_BUTTON && cache.click_item != nullptr) {
				// make sure in case of wrong reference after reconstructing whole TreeItems
				cache.click_item = get_item_at_position(cache.click_pos);
				emit_signal("button_clicked", cache.click_item, cache.click_column, cache.click_id, mb->get_button_index());
			}

			cache.click_type = Cache::CLICK_NONE;
			cache.click_index = -1;
			cache.click_id = -1;
			cache.click_item = nullptr;
			cache.click_column = 0;
			queue_redraw();
			return;
		}

		if (range_drag_enabled) {
			return;
		}

		switch (mb->get_button_index()) {
			case MouseButton::RIGHT:
			case MouseButton::LEFT: {
				Ref<StyleBox> bg = theme_cache.panel_style;

				Point2 pos = mb->get_position();
				if (rtl) {
					pos.x = get_size().width - pos.x;
				}
				pos -= bg->get_offset();
				cache.click_type = Cache::CLICK_NONE;
				if (show_column_titles) {
					pos.y -= _get_title_button_height();

					if (pos.y < 0) {
						pos.x += theme_cache.offset.x;
						int len = 0;
						for (int i = 0; i < columns.size(); i++) {
							len += get_column_width(i);
							if (pos.x < static_cast<real_t>(len)) {
								cache.click_type = Cache::CLICK_TITLE;
								cache.click_index = i;
								queue_redraw();
								break;
							}
						}
						break;
					}
				}

				if (!root || (!root->get_first_child() && hide_root)) {
					emit_signal(SNAME("empty_clicked"), get_local_mouse_position(), mb->get_button_index());
					break;
				}

				click_handled = false;
				pressing_for_editor = false;
				propagate_mouse_activated = false;

				int x_limit = get_size().width - theme_cache.panel_style->get_minimum_size().width;
				if (v_scroll->is_visible()) {
					x_limit -= v_scroll->get_minimum_size().width;
				}

				cache.rtl = is_layout_rtl();
				blocked++;
				propagate_mouse_event(pos + theme_cache.offset, 0, 0, x_limit + theme_cache.offset.width, mb->is_double_click(), root, mb->get_button_index(), mb);
				blocked--;

				if (pressing_for_editor) {
					pressing_pos = mb->get_position();
					if (rtl) {
						pressing_pos.x = get_size().width - pressing_pos.x;
					}
				}

				if (mb->get_button_index() == MouseButton::RIGHT) {
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
					drag_touching = DisplayServer::get_singleton()->is_touchscreen_available();
					drag_touching_deaccel = false;
					if (drag_touching) {
						set_physics_process_internal(true);
					}

					if (mb->get_button_index() == MouseButton::LEFT) {
						if (get_item_at_position(mb->get_position()) == nullptr && !mb->is_shift_pressed() && !mb->is_command_or_control_pressed()) {
							emit_signal(SNAME("nothing_selected"));
						}
					}
				}

				if (propagate_mouse_activated) {
					emit_signal(SNAME("item_activated"));
					propagate_mouse_activated = false;
				}

			} break;
			case MouseButton::WHEEL_UP: {
				if (_scroll(mb->is_shift_pressed(), -mb->get_factor() / 8)) {
					accept_event();
				}

			} break;
			case MouseButton::WHEEL_DOWN: {
				if (_scroll(mb->is_shift_pressed(), mb->get_factor() / 8)) {
					accept_event();
				}

			} break;
			case MouseButton::WHEEL_LEFT: {
				if (_scroll(!mb->is_shift_pressed(), -mb->get_factor() / 8)) {
					accept_event();
				}

			} break;
			case MouseButton::WHEEL_RIGHT: {
				if (_scroll(!mb->is_shift_pressed(), mb->get_factor() / 8)) {
					accept_event();
				}

			} break;
			default:
				break;
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		double prev_v = v_scroll->get_value();
		v_scroll->set_value(v_scroll->get_value() + v_scroll->get_page() * pan_gesture->get_delta().y / 8);

		double prev_h = h_scroll->get_value();
		if (is_layout_rtl()) {
			h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * -pan_gesture->get_delta().x / 8);
		} else {
			h_scroll->set_value(h_scroll->get_value() + h_scroll->get_page() * pan_gesture->get_delta().x / 8);
		}

		if (v_scroll->get_value() != prev_v || h_scroll->get_value() != prev_h) {
			accept_event();
		}
	}
}

bool Tree::edit_selected(bool p_force_edit) {
	TreeItem *s = get_selected();
	ERR_FAIL_NULL_V_MSG(s, false, "No item selected.");
	ensure_cursor_is_visible();
	int col = get_selected_column();
	ERR_FAIL_INDEX_V_MSG(col, columns.size(), false, "No item column selected.");

	if (!s->cells[col].editable && !p_force_edit) {
		return false;
	}

	float popup_scale = popup_editor->is_embedded() ? 1.0 : popup_editor->get_parent_visible_window()->get_content_scale_factor();
	Rect2 rect;
	if (select_mode == SELECT_ROW) {
		rect = s->get_meta("__focus_col_" + itos(selected_col));
	} else {
		rect = s->get_meta("__focus_rect");
	}
	rect.position *= popup_scale;
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
		emit_signal(SNAME("custom_popup_edited"), false);
		item_edited(col, s);

		return true;
	} else if (c.mode == TreeItem::CELL_MODE_RANGE && !c.text.is_empty()) {
		popup_menu->clear();
		for (int i = 0; i < c.text.get_slice_count(","); i++) {
			String s2 = c.text.get_slicec(',', i);
			popup_menu->add_item(s2.get_slicec(':', 0), s2.get_slicec(':', 1).is_empty() ? i : s2.get_slicec(':', 1).to_int());
		}

		popup_menu->set_size(Size2(rect.size.width, 0));
		popup_menu->set_position(get_screen_position() + rect.position + Point2i(0, rect.size.height));
		popup_menu->popup();
		popup_edited_item = s;
		popup_edited_item_col = col;

		return true;
	} else if ((c.mode == TreeItem::CELL_MODE_STRING && !c.edit_multiline) || c.mode == TreeItem::CELL_MODE_RANGE) {
		Rect2 popup_rect;

		int value_editor_height = c.mode == TreeItem::CELL_MODE_RANGE ? value_editor->get_minimum_size().height : 0;
		// "floor()" centers vertically.
		Vector2 ofs(0, Math::floor((MAX(line_editor->get_minimum_size().height, rect.size.height - value_editor_height) - rect.size.height) / 2));

		popup_rect.position = get_screen_position() + rect.position - ofs;
		popup_rect.size = rect.size;

		// Account for icon.
		Size2 icon_size = _get_cell_icon_size(c) * popup_scale;
		popup_rect.position.x += icon_size.x;
		popup_rect.size.x -= icon_size.x;

		line_editor->clear();
		line_editor->set_text(c.mode == TreeItem::CELL_MODE_STRING ? c.text : String::num(c.val, Math::range_step_decimals(c.step)));
		line_editor->select_all();
		line_editor->show();

		text_editor->hide();

		if (c.mode == TreeItem::CELL_MODE_RANGE) {
			popup_rect.size.y += value_editor_height;

			value_editor->show();
			updating_value_editor = true;
			value_editor->set_min(c.min);
			value_editor->set_max(c.max);
			value_editor->set_step(c.step);
			value_editor->set_value(c.val);
			value_editor->set_exp_ratio(c.expr);
			updating_value_editor = false;
		} else {
			value_editor->hide();
		}

		popup_editor->set_position(popup_rect.position);
		popup_editor->set_size(popup_rect.size * popup_scale);
		if (!popup_editor->is_embedded()) {
			popup_editor->set_content_scale_factor(popup_scale);
		}
		popup_edit_commited = false; // Start edit popup processing.
		popup_editor->popup();
		popup_editor->child_controls_changed();

		line_editor->grab_focus();

		return true;
	} else if (c.mode == TreeItem::CELL_MODE_STRING && c.edit_multiline) {
		line_editor->hide();

		text_editor->clear();
		text_editor->set_text(c.text);
		text_editor->select_all();
		text_editor->show();

		popup_editor->set_position(get_screen_position() + rect.position);
		popup_editor->set_size(rect.size * popup_scale);
		if (!popup_editor->is_embedded()) {
			popup_editor->set_content_scale_factor(popup_scale);
		}
		popup_edit_commited = false; // Start edit popup processing.
		popup_editor->popup();
		popup_editor->child_controls_changed();

		text_editor->grab_focus();

		return true;
	}

	return false;
}

bool Tree::is_editing() {
	return popup_editor->is_visible();
}

void Tree::set_editor_selection(int p_from_line, int p_to_line, int p_from_column, int p_to_column, int p_caret) {
	if (p_from_column == -1 || p_to_column == -1) {
		line_editor->select(p_from_line, p_to_line);
	} else {
		text_editor->select(p_from_line, p_from_column, p_to_line, p_to_column, p_caret);
	}
}

Size2 Tree::get_internal_min_size() const {
	Size2i size;
	if (root) {
		size.height += get_item_height(root);
	}
	for (int i = 0; i < columns.size(); i++) {
		size.width += get_column_minimum_width(i);
	}

	return size;
}

void Tree::update_scrollbars() {
	const Size2 control_size = get_size();
	const Ref<StyleBox> background = theme_cache.panel_style;

	// This is the background stylebox's content rect.
	const real_t width = control_size.x - background->get_margin(SIDE_LEFT) - background->get_margin(SIDE_RIGHT);
	const real_t height = control_size.y - background->get_margin(SIDE_TOP) - background->get_margin(SIDE_BOTTOM);
	const Rect2 content_rect = Rect2(background->get_offset(), Size2(width, height));

	const Size2 hmin = h_scroll->get_combined_minimum_size();
	const Size2 vmin = v_scroll->get_combined_minimum_size();

	const Size2 internal_min_size = get_internal_min_size();
	const int title_button_height = _get_title_button_height();

	Size2 tree_content_size = content_rect.get_size() - Vector2(0, title_button_height);
	bool display_vscroll = internal_min_size.height > tree_content_size.height;
	bool display_hscroll = internal_min_size.width > tree_content_size.width;
	for (int i = 0; i < 2; i++) {
		// Check twice, as both values are dependent on each other.
		if (display_hscroll) {
			tree_content_size.height = content_rect.get_size().height - title_button_height - hmin.height;
			display_vscroll = internal_min_size.height > tree_content_size.height;
		}
		if (display_vscroll) {
			tree_content_size.width = content_rect.get_size().width - vmin.width;
			display_hscroll = internal_min_size.width > tree_content_size.width;
		}
	}

	if (display_vscroll) {
		v_scroll->show();
		v_scroll->set_max(internal_min_size.height);
		v_scroll->set_page(tree_content_size.height);
		theme_cache.offset.y = v_scroll->get_value();
	} else {
		v_scroll->hide();
		theme_cache.offset.y = 0;
	}

	if (display_hscroll) {
		h_scroll->show();
		h_scroll->set_max(internal_min_size.width);
		h_scroll->set_page(tree_content_size.width);
		theme_cache.offset.x = h_scroll->get_value();
	} else {
		h_scroll->hide();
		theme_cache.offset.x = 0;
	}

	const Rect2 scroll_rect = _get_scrollbar_layout_rect();
	v_scroll->set_begin(scroll_rect.get_position() + Vector2(scroll_rect.get_size().x - vmin.width, 0));
	v_scroll->set_end(scroll_rect.get_end() - Vector2(0, display_hscroll ? hmin.height : 0));
	h_scroll->set_begin(scroll_rect.get_position() + Vector2(0, scroll_rect.get_size().y - hmin.height));
	h_scroll->set_end(scroll_rect.get_end() - Vector2(display_vscroll ? vmin.width : 0, 0));
}

int Tree::_get_title_button_height() const {
	ERR_FAIL_COND_V(theme_cache.tb_font.is_null() || theme_cache.title_button.is_null(), 0);
	int h = 0;
	if (show_column_titles) {
		for (int i = 0; i < columns.size(); i++) {
			h = MAX(h, columns[i].text_buf->get_size().y + theme_cache.title_button->get_minimum_size().height);
		}
	}
	return h;
}

void Tree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_FOCUS_ENTER: {
			if (get_viewport()) {
				focus_in_id = get_viewport()->get_processed_events_count();
			}
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			if (cache.hover_type != Cache::CLICK_NONE) {
				cache.hover_type = Cache::CLICK_NONE;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			drag_touching = false;
		} break;

		case NOTIFICATION_DRAG_END: {
			drop_mode_flags = 0;
			scrolling = false;
			set_physics_process_internal(false);
			queue_redraw();
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			single_select_defer = nullptr;
			if (theme_cache.scroll_speed > 0) {
				scrolling = true;
				set_physics_process_internal(true);
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
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
			if (scrolling && get_rect().grow(theme_cache.scroll_border).has_point(mouse_position)) {
				Point2 point;

				if ((ABS(mouse_position.x) < ABS(mouse_position.x - get_size().width)) && (ABS(mouse_position.x) < theme_cache.scroll_border)) {
					point.x = mouse_position.x - theme_cache.scroll_border;
				} else if (ABS(mouse_position.x - get_size().width) < theme_cache.scroll_border) {
					point.x = mouse_position.x - (get_size().width - theme_cache.scroll_border);
				}

				if ((ABS(mouse_position.y) < ABS(mouse_position.y - get_size().height)) && (ABS(mouse_position.y) < theme_cache.scroll_border)) {
					point.y = mouse_position.y - theme_cache.scroll_border;
				} else if (ABS(mouse_position.y - get_size().height) < theme_cache.scroll_border) {
					point.y = mouse_position.y - (get_size().height - theme_cache.scroll_border);
				}

				point *= theme_cache.scroll_speed * get_physics_process_delta_time();
				point += get_scroll();
				h_scroll->set_value(point.x);
				v_scroll->set_value(point.y);
			}
		} break;

		case NOTIFICATION_DRAW: {
			v_scroll->set_custom_step(theme_cache.font->get_height(theme_cache.font_size));

			update_scrollbars();
			RID ci = get_canvas_item();

			Ref<StyleBox> bg = theme_cache.panel_style;
			const Rect2 content_rect = _get_content_rect();

			Point2 draw_ofs = content_rect.position;
			Size2 draw_size = content_rect.size;

			bg->draw(ci, Rect2(Point2(), get_size()));

			int tbh = _get_title_button_height();

			draw_ofs.y += tbh;
			draw_size.y -= tbh;

			cache.rtl = is_layout_rtl();

			if (root && get_size().x > 0 && get_size().y > 0) {
				int self_height = 0; // Just to pass a reference, we don't need the root's `self_height`.
				draw_item(Point2(), draw_ofs, draw_size, root, self_height);
			}

			if (show_column_titles) {
				//title buttons
				int ofs2 = theme_cache.panel_style->get_margin(SIDE_LEFT);
				for (int i = 0; i < columns.size(); i++) {
					Ref<StyleBox> sb = (cache.click_type == Cache::CLICK_TITLE && cache.click_index == i) ? theme_cache.title_button_pressed : ((cache.hover_type == Cache::CLICK_TITLE && cache.hover_index == i) ? theme_cache.title_button_hover : theme_cache.title_button);
					Rect2 tbrect = Rect2(ofs2 - theme_cache.offset.x, bg->get_margin(SIDE_TOP), get_column_width(i), tbh);
					if (cache.rtl) {
						tbrect.position.x = get_size().width - tbrect.size.x - tbrect.position.x;
					}
					sb->draw(ci, tbrect);
					ofs2 += tbrect.size.width;
					//text
					int clip_w = tbrect.size.width - sb->get_minimum_size().width;
					columns.write[i].text_buf->set_width(clip_w);
					columns.write[i].cached_minimum_width_dirty = true;

					Vector2 text_pos = Point2i(tbrect.position.x, tbrect.position.y + (tbrect.size.height - columns[i].text_buf->get_size().y) / 2);
					switch (columns[i].title_alignment) {
						case HorizontalAlignment::HORIZONTAL_ALIGNMENT_LEFT: {
							text_pos.x += cache.rtl ? tbrect.size.width - (sb->get_offset().x + columns[i].text_buf->get_size().x) : sb->get_offset().x;
							break;
						}

						case HorizontalAlignment::HORIZONTAL_ALIGNMENT_RIGHT: {
							text_pos.x += cache.rtl ? sb->get_offset().x : tbrect.size.width - (sb->get_offset().x + columns[i].text_buf->get_size().x);
							break;
						}

						default: {
							text_pos.x += (tbrect.size.width - columns[i].text_buf->get_size().x) / 2;
							break;
						}
					}

					if (theme_cache.font_outline_size > 0 && theme_cache.font_outline_color.a > 0) {
						columns[i].text_buf->draw_outline(ci, text_pos, theme_cache.font_outline_size, theme_cache.font_outline_color);
					}
					columns[i].text_buf->draw(ci, text_pos, theme_cache.title_button_color);
				}
			}

			// Draw the focus outline last, so that it is drawn in front of the section headings.
			// Otherwise, section heading backgrounds can appear to be in front of the focus outline when scrolling.
			if (has_focus()) {
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, true);
				theme_cache.focus_style->draw(ci, Rect2(Point2(), get_size()));
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, false);
			}
		} break;

		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			_update_all();
		} break;

		case NOTIFICATION_RESIZED:
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (popup_edited_item != nullptr) {
				Rect2 rect = popup_edited_item->get_meta("__focus_rect");

				popup_editor->set_position(get_global_position() + rect.position);
				popup_editor->set_size(rect.size);
				popup_editor->child_controls_changed();
			}
		} break;
	}
}

void Tree::_update_all() {
	for (int i = 0; i < columns.size(); i++) {
		update_column(i);
	}
	if (root) {
		update_item_cache(root);
	}
}

Size2 Tree::get_minimum_size() const {
	Vector2 min_size = Vector2(0, _get_title_button_height());

	if (theme_cache.panel_style.is_valid()) {
		min_size += theme_cache.panel_style->get_minimum_size();
	}

	Vector2 content_min_size = get_internal_min_size();
	if (h_scroll_enabled) {
		content_min_size.x = 0;
		min_size.y += h_scroll->get_combined_minimum_size().height;
	}
	if (v_scroll_enabled) {
		min_size.x += v_scroll->get_combined_minimum_size().width;
		content_min_size.y = 0;
	}

	return min_size + content_min_size;
}

TreeItem *Tree::create_item(TreeItem *p_parent, int p_index) {
	ERR_FAIL_COND_V(blocked > 0, nullptr);

	TreeItem *ti = nullptr;

	if (p_parent) {
		ERR_FAIL_COND_V_MSG(p_parent->tree != this, nullptr, "A different tree owns the given parent");
		ti = p_parent->create_child(p_index);
	} else {
		if (!root) {
			// No root exists, make the given item the new root.
			ti = memnew(TreeItem(this));
			ERR_FAIL_NULL_V(ti, nullptr);
			ti->cells.resize(columns.size());
			ti->is_root = true;
			root = ti;
		} else {
			// Root exists, append or insert to root.
			ti = create_item(root, p_index);
		}
	}

	return ti;
}

TreeItem *Tree::get_root() const {
	return root;
}

TreeItem *Tree::get_last_item() const {
	TreeItem *last = root;
	while (last && last->last_child && !last->collapsed) {
		last = last->last_child;
	}

	return last;
}

void Tree::item_edited(int p_column, TreeItem *p_item, MouseButton p_custom_mouse_index) {
	edited_item = p_item;
	edited_col = p_column;
	if (p_item != nullptr && p_column >= 0 && p_column < p_item->cells.size()) {
		edited_item->cells.write[p_column].dirty = true;
	}
	emit_signal(SNAME("item_edited"));
	if (p_custom_mouse_index != MouseButton::NONE) {
		emit_signal(SNAME("custom_item_clicked"), p_custom_mouse_index);
	}
}

void Tree::item_changed(int p_column, TreeItem *p_item) {
	if (p_item != nullptr) {
		if (p_column >= 0 && p_column < p_item->cells.size()) {
			p_item->cells.write[p_column].dirty = true;
			columns.write[p_column].cached_minimum_width_dirty = true;
		} else if (p_column == -1) {
			for (int i = 0; i < p_item->cells.size(); i++) {
				p_item->cells.write[i].dirty = true;
				columns.write[i].cached_minimum_width_dirty = true;
			}
		}
	}
	queue_redraw();
}

void Tree::item_selected(int p_column, TreeItem *p_item) {
	if (select_mode == SELECT_MULTI) {
		if (!p_item->cells[p_column].selectable) {
			return;
		}

		p_item->cells.write[p_column].selected = true;
		//emit_signal(SNAME("multi_selected"),p_item,p_column,true); - NO this is for TreeItem::select

		selected_col = p_column;
		selected_item = p_item;
	} else {
		select_single_item(p_item, root, p_column);
	}
	queue_redraw();
}

void Tree::item_deselected(int p_column, TreeItem *p_item) {
	if (select_mode == SELECT_SINGLE && selected_item == p_item && selected_col == p_column) {
		selected_item = nullptr;
		selected_col = -1;
	} else {
		if (select_mode == SELECT_ROW && selected_item == p_item) {
			selected_item = nullptr;
			selected_col = -1;
		} else {
			if (select_mode == SELECT_MULTI) {
				selected_item = p_item;
				selected_col = p_column;
			}
		}
	}

	if (select_mode == SELECT_MULTI || select_mode == SELECT_SINGLE) {
		p_item->cells.write[p_column].selected = false;
	} else if (select_mode == SELECT_ROW) {
		for (int i = 0; i < p_item->cells.size(); i++) {
			p_item->cells.write[i].selected = false;
		}
	}
	queue_redraw();
}

void Tree::set_select_mode(SelectMode p_mode) {
	select_mode = p_mode;
}

Tree::SelectMode Tree::get_select_mode() const {
	return select_mode;
}

void Tree::deselect_all() {
	if (root) {
		TreeItem *item = root;
		while (item) {
			if (select_mode == SELECT_ROW) {
				item->deselect(0);
			} else {
				for (int i = 0; i < columns.size(); i++) {
					item->deselect(i);
				}
			}
			TreeItem *prev_item = item;
			item = get_next_selected(root);
			ERR_FAIL_COND(item == prev_item);
		}
	}

	selected_item = nullptr;
	selected_col = -1;

	queue_redraw();
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
	popup_pressing_edited_item = nullptr;

	queue_redraw();
};

void Tree::set_hide_root(bool p_enabled) {
	if (hide_root == p_enabled) {
		return;
	}

	hide_root = p_enabled;
	queue_redraw();
	update_minimum_size();
}

bool Tree::is_root_hidden() const {
	return hide_root;
}

void Tree::set_column_custom_minimum_width(int p_column, int p_min_width) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (columns[p_column].custom_min_width == p_min_width) {
		return;
	}

	if (p_min_width < 0) {
		return;
	}
	columns.write[p_column].custom_min_width = p_min_width;
	columns.write[p_column].cached_minimum_width_dirty = true;
	queue_redraw();
}

void Tree::set_column_expand(int p_column, bool p_expand) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (columns[p_column].expand == p_expand) {
		return;
	}

	columns.write[p_column].expand = p_expand;
	columns.write[p_column].cached_minimum_width_dirty = true;
	queue_redraw();
}

void Tree::set_column_expand_ratio(int p_column, int p_ratio) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (columns[p_column].expand_ratio == p_ratio) {
		return;
	}

	columns.write[p_column].expand_ratio = p_ratio;
	columns.write[p_column].cached_minimum_width_dirty = true;
	queue_redraw();
}

void Tree::set_column_clip_content(int p_column, bool p_fit) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (columns[p_column].clip_content == p_fit) {
		return;
	}

	columns.write[p_column].clip_content = p_fit;
	columns.write[p_column].cached_minimum_width_dirty = true;
	queue_redraw();
}

bool Tree::is_column_expanding(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), false);

	return columns[p_column].expand;
}

int Tree::get_column_expand_ratio(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), 1);

	return columns[p_column].expand_ratio;
}

bool Tree::is_column_clipping_content(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), false);

	return columns[p_column].clip_content;
}

TreeItem *Tree::get_selected() const {
	return selected_item;
}

void Tree::set_selected(TreeItem *p_item, int p_column) {
	ERR_FAIL_INDEX(p_column, columns.size());
	ERR_FAIL_NULL(p_item);
	ERR_FAIL_COND_MSG(p_item->get_tree() != this, "The provided TreeItem does not belong to this Tree. Ensure that the TreeItem is a part of the Tree before setting it as selected.");

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
	if (!root) {
		return nullptr;
	}

	while (true) {
		if (!p_item) {
			p_item = root;
		} else {
			if (p_item->first_child) {
				p_item = p_item->first_child;

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

int Tree::get_column_minimum_width(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), -1);

	if (columns[p_column].cached_minimum_width_dirty) {
		// Use the custom minimum width.
		int min_width = columns[p_column].custom_min_width;

		// Check if the visible title of the column is wider.
		if (show_column_titles) {
			const float padding = theme_cache.title_button->get_margin(SIDE_LEFT) + theme_cache.title_button->get_margin(SIDE_RIGHT);
			min_width = MAX(theme_cache.font->get_string_size(columns[p_column].xl_title, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size).width + padding, min_width);
		}

		if (root && !columns[p_column].clip_content) {
			int depth = 1;

			TreeItem *last = nullptr;
			TreeItem *first = hide_root ? root->get_next_visible() : root;
			for (TreeItem *item = first; item; last = item, item = item->get_next_visible()) {
				// Get column indentation.
				int indent;
				if (p_column == 0) {
					if (last) {
						if (item->parent == last) {
							depth += 1;
						} else if (item->parent != last->parent) {
							depth = hide_root ? 0 : 1;
							for (TreeItem *iter = item->parent; iter; iter = iter->parent) {
								depth += 1;
							}
						}
					}
					indent = theme_cache.item_margin * depth;
				} else {
					indent = theme_cache.h_separation;
				}

				// Get the item minimum size.
				Size2 item_size = item->get_minimum_size(p_column);
				item_size.width += indent;

				// Check if the item is wider.
				min_width = MAX(min_width, item_size.width);
			}
		}

		columns.get(p_column).cached_minimum_width = min_width;
		columns.get(p_column).cached_minimum_width_dirty = false;
	}

	return columns[p_column].cached_minimum_width;
}

int Tree::get_column_width(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), -1);

	int column_width = get_column_minimum_width(p_column);

	if (columns[p_column].expand) {
		int expand_area = _get_content_rect().size.width;
		int expanding_total = 0;

		for (int i = 0; i < columns.size(); i++) {
			expand_area -= get_column_minimum_width(i);
			if (columns[i].expand) {
				expanding_total += columns[i].expand_ratio;
			}
		}

		if (expand_area >= expanding_total && expanding_total > 0) {
			column_width += expand_area * columns[p_column].expand_ratio / expanding_total;
		}
	}

	return column_width;
}

void Tree::propagate_set_columns(TreeItem *p_item) {
	p_item->cells.resize(columns.size());

	TreeItem *c = p_item->get_first_child();
	while (c) {
		propagate_set_columns(c);
		c = c->next;
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
	queue_redraw();
}

int Tree::get_columns() const {
	return columns.size();
}

void Tree::_scroll_moved(float) {
	queue_redraw();
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

		if ((it != root || !hide_root) && it->is_visible_in_tree()) {
			ofs += compute_item_height(it);
			ofs += theme_cache.v_separation;
		}

		if (it->first_child && !it->collapsed) {
			it = it->first_child;

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

	// Note: Code below similar to Tree::scroll_to_item(), in case of bug fix both.
	const Size2 area_size = _get_content_rect().size;

	int y_offset = get_item_offset(selected_item);
	if (y_offset != -1) {
		const int tbh = _get_title_button_height();
		y_offset -= tbh;

		const int cell_h = compute_item_height(selected_item) + theme_cache.v_separation;
		int screen_h = area_size.height - tbh;

		if (cell_h > screen_h) { // Screen size is too small, maybe it was not resized yet.
			v_scroll->set_value(y_offset);
		} else if (y_offset + cell_h > v_scroll->get_value() + screen_h) {
			callable_mp((Range *)v_scroll, &Range::set_value).call_deferred(y_offset - screen_h + cell_h);
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
		const int screen_w = area_size.width;

		if (cell_w > screen_w) {
			h_scroll->set_value(x_offset);
		} else if (x_offset + cell_w > h_scroll->get_value() + screen_w) {
			callable_mp((Range *)h_scroll, &Range::set_value).call_deferred(x_offset - screen_w + cell_w);
		} else if (x_offset < h_scroll->get_value()) {
			h_scroll->set_value(x_offset);
		}
	}
}

int Tree::get_pressed_button() const {
	return pressed_button;
}

Rect2 Tree::get_item_rect(TreeItem *p_item, int p_column, int p_button) const {
	ERR_FAIL_NULL_V(p_item, Rect2());
	ERR_FAIL_COND_V(p_item->tree != this, Rect2());
	if (p_column != -1) {
		ERR_FAIL_INDEX_V(p_column, columns.size(), Rect2());
	}
	if (p_button != -1) {
		ERR_FAIL_COND_V(p_column == -1, Rect2()); // pass a column if you want to pass a button
		ERR_FAIL_INDEX_V(p_button, p_item->cells[p_column].buttons.size(), Rect2());
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
		if (p_button != -1) {
			const TreeItem::Cell &c = p_item->cells[p_column];
			Vector2 ofst = Vector2(r.position.x + r.size.x, r.position.y);
			for (int j = c.buttons.size() - 1; j >= 0; j--) {
				Ref<Texture2D> b = c.buttons[j].texture;
				Size2 size = b->get_size() + theme_cache.button_pressed->get_minimum_size();
				ofst.x -= size.x;

				if (j == p_button) {
					return Rect2(ofst, size);
				}
			}
		}
	}

	return r;
}

void Tree::set_column_titles_visible(bool p_show) {
	if (show_column_titles == p_show) {
		return;
	}

	show_column_titles = p_show;
	queue_redraw();
	update_minimum_size();
}

bool Tree::are_column_titles_visible() const {
	return show_column_titles;
}

void Tree::set_column_title(int p_column, const String &p_title) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (columns[p_column].title == p_title) {
		return;
	}

	columns.write[p_column].title = p_title;
	columns.write[p_column].xl_title = atr(p_title);
	update_column(p_column);
	queue_redraw();
}

String Tree::get_column_title(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), "");
	return columns[p_column].title;
}

void Tree::set_column_title_alignment(int p_column, HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX(p_column, columns.size());

	if (p_alignment == HORIZONTAL_ALIGNMENT_FILL) {
		WARN_PRINT("HORIZONTAL_ALIGNMENT_FILL is not supported for column titles.");
	}

	if (columns[p_column].title_alignment == p_alignment) {
		return;
	}

	columns.write[p_column].title_alignment = p_alignment;
	update_column(p_column);
	queue_redraw();
}

HorizontalAlignment Tree::get_column_title_alignment(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	return columns[p_column].title_alignment;
}

void Tree::set_column_title_direction(int p_column, Control::TextDirection p_text_direction) {
	ERR_FAIL_INDEX(p_column, columns.size());
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (columns[p_column].text_direction != p_text_direction) {
		columns.write[p_column].text_direction = p_text_direction;
		update_column(p_column);
		queue_redraw();
	}
}

Control::TextDirection Tree::get_column_title_direction(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), TEXT_DIRECTION_INHERITED);
	return columns[p_column].text_direction;
}

void Tree::set_column_title_language(int p_column, const String &p_language) {
	ERR_FAIL_INDEX(p_column, columns.size());
	if (columns[p_column].language != p_language) {
		columns.write[p_column].language = p_language;
		update_column(p_column);
		queue_redraw();
	}
}

String Tree::get_column_title_language(int p_column) const {
	ERR_FAIL_INDEX_V(p_column, columns.size(), "");
	return columns[p_column].language;
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

void Tree::scroll_to_item(TreeItem *p_item, bool p_center_on_item) {
	ERR_FAIL_NULL(p_item);

	update_scrollbars();

	// Note: Code below similar to Tree::ensure_cursor_is_visible(), in case of bug fix both.
	const Size2 area_size = _get_content_rect().size;

	int y_offset = get_item_offset(p_item);
	if (y_offset != -1) {
		const int tbh = _get_title_button_height();
		y_offset -= tbh;

		const int cell_h = compute_item_height(p_item) + theme_cache.v_separation;
		int screen_h = area_size.height - tbh;

		if (p_center_on_item) {
			v_scroll->set_value(y_offset - (screen_h - cell_h) / 2.0f);
		} else {
			if (cell_h > screen_h) { // Screen size is too small, maybe it was not resized yet.
				v_scroll->set_value(y_offset);
			} else if (y_offset + cell_h > v_scroll->get_value() + screen_h) {
				v_scroll->set_value(y_offset - screen_h + cell_h);
			} else if (y_offset < v_scroll->get_value()) {
				v_scroll->set_value(y_offset);
			}
		}
	}
}

void Tree::set_h_scroll_enabled(bool p_enable) {
	if (h_scroll_enabled == p_enable) {
		return;
	}

	h_scroll_enabled = p_enable;
	update_minimum_size();
}

bool Tree::is_h_scroll_enabled() const {
	return h_scroll_enabled;
}

void Tree::set_v_scroll_enabled(bool p_enable) {
	if (v_scroll_enabled == p_enable) {
		return;
	}

	v_scroll_enabled = p_enable;
	update_minimum_size();
}

bool Tree::is_v_scroll_enabled() const {
	return v_scroll_enabled;
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

TreeItem *Tree::get_item_with_metadata(const Variant &p_find, int p_column) const {
	if (p_column < 0) {
		for (TreeItem *current = root; current; current = current->get_next_in_tree()) {
			for (int i = 0; i < columns.size(); i++) {
				if (current->get_metadata(i) == p_find) {
					return current;
				}
			}
		}
		return nullptr;
	}

	for (TreeItem *current = root; current; current = current->get_next_in_tree()) {
		if (current->get_metadata(p_column) == p_find) {
			return current;
		}
	}
	return nullptr;
}

void Tree::_do_incr_search(const String &p_add) {
	uint64_t time = OS::get_singleton()->get_ticks_usec() / 1000; // convert to msec
	uint64_t diff = time - last_keypress;
	if (diff > uint64_t(GLOBAL_GET("gui/timers/incremental_search_max_interval_msec"))) {
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

	if (select_mode == SELECT_MULTI) {
		item->set_as_cursor(col);
	} else {
		item->select(col);
	}
	ensure_cursor_is_visible();
}

TreeItem *Tree::_find_item_at_pos(TreeItem *p_item, const Point2 &p_pos, int &r_column, int &h, int &section) const {
	Point2 pos = p_pos;

	if ((root != p_item || !hide_root) && p_item->is_visible_in_tree()) {
		h = compute_item_height(p_item) + theme_cache.v_separation;
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

	if (p_item->is_collapsed() || !p_item->is_visible_in_tree()) {
		return nullptr; // do not try children, it's collapsed
	}

	TreeItem *n = p_item->get_first_child();
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

// When on a button, r_index is valid.
// When on an item, both r_item and r_column are valid.
// Otherwise, all output arguments are invalid.
void Tree::_find_button_at_pos(const Point2 &p_pos, TreeItem *&r_item, int &r_column, int &r_index) const {
	r_item = nullptr;
	r_column = -1;
	r_index = -1;

	if (!root) {
		return;
	}

	Point2 pos = p_pos - theme_cache.panel_style->get_offset();
	pos.y -= _get_title_button_height();
	if (pos.y < 0) {
		return;
	}

	if (cache.rtl) {
		pos.x = get_size().width - pos.x;
	}
	pos += theme_cache.offset; // Scrolling.

	int col, h, section;
	TreeItem *it = _find_item_at_pos(root, pos, col, h, section);
	if (!it) {
		return;
	}

	r_item = it;
	r_column = col;

	const TreeItem::Cell &c = it->cells[col];
	if (c.buttons.is_empty()) {
		return;
	}

	int x_limit = get_size().width - theme_cache.panel_style->get_minimum_size().width + theme_cache.offset.x;
	if (v_scroll->is_visible_in_tree()) {
		x_limit -= v_scroll->get_minimum_size().width;
	}

	for (int i = 0; i < col; i++) {
		const int col_w = get_column_width(i) + theme_cache.h_separation;
		pos.x -= col_w;
		x_limit -= col_w;
	}

	int x_check;
	if (cache.rtl) {
		x_check = get_column_width(col);
	} else {
		// Right edge of the buttons area, relative to the start of the column.
		int buttons_area_min = 0;
		if (col == 0) {
			// Content of column 0 should take indentation into account.
			for (TreeItem *current = it; current && (current != root || !hide_root); current = current->parent) {
				buttons_area_min += theme_cache.item_margin;
			}
		}
		for (int i = c.buttons.size() - 1; i >= 0; i--) {
			Ref<Texture2D> b = c.buttons[i].texture;
			buttons_area_min += b->get_size().width + theme_cache.button_pressed->get_minimum_size().width + theme_cache.button_margin;
		}

		x_check = MAX(buttons_area_min, MIN(get_column_width(col), x_limit));
	}

	for (int i = c.buttons.size() - 1; i >= 0; i--) {
		Ref<Texture2D> b = c.buttons[i].texture;
		Size2 size = b->get_size() + theme_cache.button_pressed->get_minimum_size();
		if (pos.x > x_check - size.width) {
			x_limit -= theme_cache.item_margin;
			r_index = i;
			return;
		}
		x_check -= size.width + theme_cache.button_margin;
	}
}

int Tree::get_column_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		if (is_layout_rtl()) {
			pos.x = get_size().width - pos.x;
		}
		pos -= theme_cache.panel_style->get_offset();
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
		if (is_layout_rtl()) {
			pos.x = get_size().width - pos.x;
		}
		pos -= theme_cache.panel_style->get_offset();
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

bool Tree::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	if (drag_touching) {
		// Disable data drag & drop when touch dragging.
		return false;
	}

	return Control::can_drop_data(p_point, p_data);
}

Variant Tree::get_drag_data(const Point2 &p_point) {
	if (drag_touching) {
		// Disable data drag & drop when touch dragging.
		return Variant();
	}

	return Control::get_drag_data(p_point);
}

TreeItem *Tree::get_item_at_position(const Point2 &p_pos) const {
	if (root) {
		Point2 pos = p_pos;
		if (is_layout_rtl()) {
			pos.x = get_size().width - pos.x;
		}
		pos -= theme_cache.panel_style->get_offset();
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
	TreeItem *it;
	int col, index;
	_find_button_at_pos(p_pos, it, col, index);

	if (index == -1) {
		return -1;
	}
	return it->cells[col].buttons[index].id;
}

String Tree::get_tooltip(const Point2 &p_pos) const {
	Point2 pos = p_pos - theme_cache.panel_style->get_offset();
	pos.y -= _get_title_button_height();
	if (pos.y < 0) {
		return Control::get_tooltip(p_pos);
	}

	TreeItem *it;
	int col, index;
	_find_button_at_pos(p_pos, it, col, index);

	if (index != -1) {
		return it->cells[col].buttons[index].tooltip;
	}

	if (it) {
		const String item_tooltip = it->get_tooltip_text(col);
		if (item_tooltip.is_empty()) {
			return it->get_text(col);
		}
		return item_tooltip;
	}

	return Control::get_tooltip(p_pos);
}

void Tree::set_cursor_can_exit_tree(bool p_enable) {
	cursor_can_exit_tree = p_enable;
}

void Tree::set_hide_folding(bool p_hide) {
	if (hide_folding == p_hide) {
		return;
	}

	hide_folding = p_hide;
	queue_redraw();
}

bool Tree::is_folding_hidden() const {
	return hide_folding;
}

void Tree::set_enable_recursive_folding(bool p_enable) {
	enable_recursive_folding = p_enable;
}

bool Tree::is_recursive_folding_enabled() const {
	return enable_recursive_folding;
}

void Tree::set_drop_mode_flags(int p_flags) {
	if (drop_mode_flags == p_flags) {
		return;
	}
	drop_mode_flags = p_flags;
	if (drop_mode_flags == 0) {
		drop_mode_over = nullptr;
	}

	queue_redraw();
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
	ClassDB::bind_method(D_METHOD("clear"), &Tree::clear);
	ClassDB::bind_method(D_METHOD("create_item", "parent", "index"), &Tree::create_item, DEFVAL(Variant()), DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("get_root"), &Tree::get_root);
	ClassDB::bind_method(D_METHOD("set_column_custom_minimum_width", "column", "min_width"), &Tree::set_column_custom_minimum_width);
	ClassDB::bind_method(D_METHOD("set_column_expand", "column", "expand"), &Tree::set_column_expand);
	ClassDB::bind_method(D_METHOD("set_column_expand_ratio", "column", "ratio"), &Tree::set_column_expand_ratio);
	ClassDB::bind_method(D_METHOD("set_column_clip_content", "column", "enable"), &Tree::set_column_clip_content);
	ClassDB::bind_method(D_METHOD("is_column_expanding", "column"), &Tree::is_column_expanding);
	ClassDB::bind_method(D_METHOD("is_column_clipping_content", "column"), &Tree::is_column_clipping_content);
	ClassDB::bind_method(D_METHOD("get_column_expand_ratio", "column"), &Tree::get_column_expand_ratio);

	ClassDB::bind_method(D_METHOD("get_column_width", "column"), &Tree::get_column_width);

	ClassDB::bind_method(D_METHOD("set_hide_root", "enable"), &Tree::set_hide_root);
	ClassDB::bind_method(D_METHOD("is_root_hidden"), &Tree::is_root_hidden);
	ClassDB::bind_method(D_METHOD("get_next_selected", "from"), &Tree::get_next_selected);
	ClassDB::bind_method(D_METHOD("get_selected"), &Tree::get_selected);
	ClassDB::bind_method(D_METHOD("set_selected", "item", "column"), &Tree::set_selected);
	ClassDB::bind_method(D_METHOD("get_selected_column"), &Tree::get_selected_column);
	ClassDB::bind_method(D_METHOD("get_pressed_button"), &Tree::get_pressed_button);
	ClassDB::bind_method(D_METHOD("set_select_mode", "mode"), &Tree::set_select_mode);
	ClassDB::bind_method(D_METHOD("get_select_mode"), &Tree::get_select_mode);
	ClassDB::bind_method(D_METHOD("deselect_all"), &Tree::deselect_all);

	ClassDB::bind_method(D_METHOD("set_columns", "amount"), &Tree::set_columns);
	ClassDB::bind_method(D_METHOD("get_columns"), &Tree::get_columns);

	ClassDB::bind_method(D_METHOD("get_edited"), &Tree::get_edited);
	ClassDB::bind_method(D_METHOD("get_edited_column"), &Tree::get_edited_column);
	ClassDB::bind_method(D_METHOD("edit_selected", "force_edit"), &Tree::edit_selected, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_custom_popup_rect"), &Tree::get_custom_popup_rect);
	ClassDB::bind_method(D_METHOD("get_item_area_rect", "item", "column", "button_index"), &Tree::get_item_rect, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_item_at_position", "position"), &Tree::get_item_at_position);
	ClassDB::bind_method(D_METHOD("get_column_at_position", "position"), &Tree::get_column_at_position);
	ClassDB::bind_method(D_METHOD("get_drop_section_at_position", "position"), &Tree::get_drop_section_at_position);
	ClassDB::bind_method(D_METHOD("get_button_id_at_position", "position"), &Tree::get_button_id_at_position);

	ClassDB::bind_method(D_METHOD("ensure_cursor_is_visible"), &Tree::ensure_cursor_is_visible);

	ClassDB::bind_method(D_METHOD("set_column_titles_visible", "visible"), &Tree::set_column_titles_visible);
	ClassDB::bind_method(D_METHOD("are_column_titles_visible"), &Tree::are_column_titles_visible);

	ClassDB::bind_method(D_METHOD("set_column_title", "column", "title"), &Tree::set_column_title);
	ClassDB::bind_method(D_METHOD("get_column_title", "column"), &Tree::get_column_title);

	ClassDB::bind_method(D_METHOD("set_column_title_alignment", "column", "title_alignment"), &Tree::set_column_title_alignment);
	ClassDB::bind_method(D_METHOD("get_column_title_alignment", "column"), &Tree::get_column_title_alignment);

	ClassDB::bind_method(D_METHOD("set_column_title_direction", "column", "direction"), &Tree::set_column_title_direction);
	ClassDB::bind_method(D_METHOD("get_column_title_direction", "column"), &Tree::get_column_title_direction);

	ClassDB::bind_method(D_METHOD("set_column_title_language", "column", "language"), &Tree::set_column_title_language);
	ClassDB::bind_method(D_METHOD("get_column_title_language", "column"), &Tree::get_column_title_language);

	ClassDB::bind_method(D_METHOD("get_scroll"), &Tree::get_scroll);
	ClassDB::bind_method(D_METHOD("scroll_to_item", "item", "center_on_item"), &Tree::scroll_to_item, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_h_scroll_enabled", "h_scroll"), &Tree::set_h_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_h_scroll_enabled"), &Tree::is_h_scroll_enabled);

	ClassDB::bind_method(D_METHOD("set_v_scroll_enabled", "h_scroll"), &Tree::set_v_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_v_scroll_enabled"), &Tree::is_v_scroll_enabled);

	ClassDB::bind_method(D_METHOD("set_hide_folding", "hide"), &Tree::set_hide_folding);
	ClassDB::bind_method(D_METHOD("is_folding_hidden"), &Tree::is_folding_hidden);

	ClassDB::bind_method(D_METHOD("set_enable_recursive_folding", "enable"), &Tree::set_enable_recursive_folding);
	ClassDB::bind_method(D_METHOD("is_recursive_folding_enabled"), &Tree::is_recursive_folding_enabled);

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
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_recursive_folding"), "set_enable_recursive_folding", "is_recursive_folding_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hide_root"), "set_hide_root", "is_root_hidden");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drop_mode_flags", PROPERTY_HINT_FLAGS, "On Item,In Between"), "set_drop_mode_flags", "get_drop_mode_flags");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "select_mode", PROPERTY_HINT_ENUM, "Single,Row,Multi"), "set_select_mode", "get_select_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_horizontal_enabled"), "set_h_scroll_enabled", "is_h_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_vertical_enabled"), "set_v_scroll_enabled", "is_v_scroll_enabled");

	ADD_SIGNAL(MethodInfo("item_selected"));
	ADD_SIGNAL(MethodInfo("cell_selected"));
	ADD_SIGNAL(MethodInfo("multi_selected", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem"), PropertyInfo(Variant::INT, "column"), PropertyInfo(Variant::BOOL, "selected")));
	ADD_SIGNAL(MethodInfo("item_mouse_selected", PropertyInfo(Variant::VECTOR2, "mouse_position"), PropertyInfo(Variant::INT, "mouse_button_index")));
	ADD_SIGNAL(MethodInfo("empty_clicked", PropertyInfo(Variant::VECTOR2, "click_position"), PropertyInfo(Variant::INT, "mouse_button_index")));
	ADD_SIGNAL(MethodInfo("item_edited"));
	ADD_SIGNAL(MethodInfo("custom_item_clicked", PropertyInfo(Variant::INT, "mouse_button_index")));
	ADD_SIGNAL(MethodInfo("item_icon_double_clicked"));
	ADD_SIGNAL(MethodInfo("item_collapsed", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem")));
	ADD_SIGNAL(MethodInfo("check_propagated_to_item", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem"), PropertyInfo(Variant::INT, "column")));
	ADD_SIGNAL(MethodInfo("button_clicked", PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "TreeItem"), PropertyInfo(Variant::INT, "column"), PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "mouse_button_index")));
	ADD_SIGNAL(MethodInfo("custom_popup_edited", PropertyInfo(Variant::BOOL, "arrow_clicked")));
	ADD_SIGNAL(MethodInfo("item_activated"));
	ADD_SIGNAL(MethodInfo("column_title_clicked", PropertyInfo(Variant::INT, "column"), PropertyInfo(Variant::INT, "mouse_button_index")));
	ADD_SIGNAL(MethodInfo("nothing_selected"));

	BIND_ENUM_CONSTANT(SELECT_SINGLE);
	BIND_ENUM_CONSTANT(SELECT_ROW);
	BIND_ENUM_CONSTANT(SELECT_MULTI);

	BIND_ENUM_CONSTANT(DROP_MODE_DISABLED);
	BIND_ENUM_CONSTANT(DROP_MODE_ON_ITEM);
	BIND_ENUM_CONSTANT(DROP_MODE_INBETWEEN);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Tree, panel_style, "panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Tree, focus_style, "focus");

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, Tree, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, Tree, font_size);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT, Tree, tb_font, "title_button_font");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT_SIZE, Tree, tb_font_size, "title_button_font_size");

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, selected);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, selected_focus);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, cursor);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Tree, cursor_unfocus, "cursor_unfocused");
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, button_pressed);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, checked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, unchecked);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, checked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, unchecked_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, indeterminate);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, indeterminate_disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, arrow_collapsed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, arrow_collapsed_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, select_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Tree, updown);

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, custom_button);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, custom_button_hover);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, custom_button_pressed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, custom_button_font_highlight);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, font_selected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, font_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, drop_position_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, v_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, inner_item_margin_bottom);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, inner_item_margin_left);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, inner_item_margin_right);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, inner_item_margin_top);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, item_margin);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, button_margin);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, icon_max_width);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, font_outline_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, Tree, font_outline_size, "outline_size");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, draw_guides);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, guide_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, draw_relationship_lines);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, relationship_line_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, parent_hl_line_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, children_hl_line_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, parent_hl_line_margin);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, relationship_line_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, parent_hl_line_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, children_hl_line_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scroll_border);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scroll_speed);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_margin_top);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_margin_right);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_margin_bottom);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_margin_left);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Tree, scrollbar_v_separation);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, Tree, title_button, "title_button_normal");
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, title_button_pressed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Tree, title_button_hover);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Tree, title_button_color);
}

Tree::Tree() {
	columns.resize(1);

	set_focus_mode(FOCUS_ALL);

	popup_menu = memnew(PopupMenu);
	popup_menu->hide();
	add_child(popup_menu, false, INTERNAL_MODE_FRONT);

	popup_editor = memnew(Popup);
	add_child(popup_editor, false, INTERNAL_MODE_FRONT);

	popup_editor_vb = memnew(VBoxContainer);
	popup_editor_vb->add_theme_constant_override("separation", 0);
	popup_editor_vb->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	popup_editor->add_child(popup_editor_vb);

	line_editor = memnew(LineEdit);
	line_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	line_editor->hide();
	popup_editor_vb->add_child(line_editor);

	text_editor = memnew(TextEdit);
	text_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	text_editor->hide();
	popup_editor_vb->add_child(text_editor);

	value_editor = memnew(HSlider);
	value_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	value_editor->hide();
	popup_editor_vb->add_child(value_editor);

	h_scroll = memnew(HScrollBar);
	v_scroll = memnew(VScrollBar);

	add_child(h_scroll, false, INTERNAL_MODE_FRONT);
	add_child(v_scroll, false, INTERNAL_MODE_FRONT);

	range_click_timer = memnew(Timer);
	range_click_timer->connect("timeout", callable_mp(this, &Tree::_range_click_timeout));
	add_child(range_click_timer, false, INTERNAL_MODE_FRONT);

	h_scroll->connect(SceneStringName(value_changed), callable_mp(this, &Tree::_scroll_moved));
	v_scroll->connect(SceneStringName(value_changed), callable_mp(this, &Tree::_scroll_moved));
	line_editor->connect("text_submitted", callable_mp(this, &Tree::_line_editor_submit));
	text_editor->connect(SceneStringName(gui_input), callable_mp(this, &Tree::_text_editor_gui_input));
	popup_editor->connect("popup_hide", callable_mp(this, &Tree::_text_editor_popup_modal_close));
	popup_menu->connect(SceneStringName(id_pressed), callable_mp(this, &Tree::popup_select));
	value_editor->connect(SceneStringName(value_changed), callable_mp(this, &Tree::value_editor_changed));

	set_notify_transform(true);

	set_mouse_filter(MOUSE_FILTER_STOP);

	set_clip_contents(true);
}

Tree::~Tree() {
	if (root) {
		memdelete(root);
	}
}
