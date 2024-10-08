/**************************************************************************/
/*  tab_container.cpp                                                     */
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

#include "tab_container.h"

#include "scene/theme/theme_db.h"

int TabContainer::_get_tab_height() const {
	int height = 0;
	if (tabs_visible && get_tab_count() > 0) {
		height = tab_bar->get_minimum_size().height;
	}

	return height;
}

void TabContainer::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;

	Popup *popup = get_popup();

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		Point2 pos = mb->get_position();
		Size2 size = get_size();
		real_t content_height = size.height - _get_tab_height();

		// Click must be on tabs in the tab header area.
		if (tabs_position == POSITION_TOP && pos.y > _get_tab_height()) {
			return;
		}
		if (tabs_position == POSITION_BOTTOM && pos.y < content_height) {
			return;
		}

		// Handle menu button.
		if (popup) {
			if (is_layout_rtl() ? pos.x < theme_cache.menu_icon->get_width() : pos.x > size.width - theme_cache.menu_icon->get_width()) {
				emit_signal(SNAME("pre_popup_pressed"));

				Vector2 popup_pos = get_screen_position();
				if (!is_layout_rtl()) {
					popup_pos.x += size.width - popup->get_size().width;
				}
				popup_pos.y += _get_tab_height() / 2.0;
				if (tabs_position == POSITION_BOTTOM) {
					popup_pos.y += content_height;
					popup_pos.y -= popup->get_size().height;
					popup_pos.y -= theme_cache.menu_icon->get_height() / 2.0;
				} else {
					popup_pos.y += theme_cache.menu_icon->get_height() / 2.0;
				}

				popup->set_position(popup_pos);
				popup->popup();
				return;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Point2 pos = mm->get_position();
		Size2 size = get_size();

		// Mouse must be on tabs in the tab header area.
		if (tabs_position == POSITION_TOP && pos.y > _get_tab_height()) {
			if (menu_hovered) {
				menu_hovered = false;
				queue_redraw();
			}
			return;
		}
		if (tabs_position == POSITION_BOTTOM && pos.y < size.height - _get_tab_height()) {
			if (menu_hovered) {
				menu_hovered = false;
				queue_redraw();
			}
			return;
		}

		if (popup) {
			if (is_layout_rtl()) {
				if (pos.x <= theme_cache.menu_icon->get_width()) {
					if (!menu_hovered) {
						menu_hovered = true;
						queue_redraw();
						return;
					}
				} else if (menu_hovered) {
					menu_hovered = false;
					queue_redraw();
				}
			} else {
				if (pos.x >= size.width - theme_cache.menu_icon->get_width()) {
					if (!menu_hovered) {
						menu_hovered = true;
						queue_redraw();
						return;
					}
				} else if (menu_hovered) {
					menu_hovered = false;
					queue_redraw();
				}
			}

			if (menu_hovered) {
				return;
			}
		}
	}
}

void TabContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// If some nodes happen to be renamed outside the tree, the tab names need to be updated manually.
			if (get_tab_count() > 0) {
				_refresh_tab_names();
			}

			if (setup_current_tab >= -1) {
				set_current_tab(setup_current_tab);
				setup_current_tab = -2;
			}
		} break;

		case NOTIFICATION_READY:
		case NOTIFICATION_RESIZED: {
			_update_margins();
		} break;

		case NOTIFICATION_DRAW: {
			RID canvas = get_canvas_item();
			Size2 size = get_size();

			// Draw only the tab area if the header is hidden.
			if (!tabs_visible) {
				theme_cache.panel_style->draw(canvas, Rect2(0, 0, size.width, size.height));
				return;
			}

			int header_height = _get_tab_height();
			int header_voffset = int(tabs_position == POSITION_BOTTOM) * (size.height - header_height);

			// Draw background for the tabbar.
			theme_cache.tabbar_style->draw(canvas, Rect2(0, header_voffset, size.width, header_height));
			// Draw the background for the tab's content.
			theme_cache.panel_style->draw(canvas, Rect2(0, int(tabs_position == POSITION_TOP) * header_height, size.width, size.height - header_height));

			// Draw the popup menu.
			if (get_popup()) {
				int x = is_layout_rtl() ? 0 : get_size().width - theme_cache.menu_icon->get_width();

				if (menu_hovered) {
					theme_cache.menu_hl_icon->draw(get_canvas_item(), Point2(x, header_voffset + (header_height - theme_cache.menu_hl_icon->get_height()) / 2));
				} else {
					theme_cache.menu_icon->draw(get_canvas_item(), Point2(x, header_voffset + (header_height - theme_cache.menu_icon->get_height()) / 2));
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				return;
			}

			updating_visibility = true;

			// As the visibility change notification will be triggered for all children soon after,
			// beat it to the punch and make sure that the correct node is the only one visible first.
			// Otherwise, it can prevent a tab change done right before this container was made visible.
			Vector<Control *> controls = _get_tab_controls();
			int current = setup_current_tab > -2 ? setup_current_tab : get_current_tab();
			for (int i = 0; i < controls.size(); i++) {
				controls[i]->set_visible(i == current);
			}

			updating_visibility = false;
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			theme_changing = true;
			callable_mp(this, &TabContainer::_on_theme_changed).call_deferred(); // Wait until all changed theme.
		} break;
	}
}

void TabContainer::_on_theme_changed() {
	if (!theme_changing) {
		return;
	}

	tab_bar->begin_bulk_theme_override();

	tab_bar->add_theme_style_override(SNAME("tab_unselected"), theme_cache.tab_unselected_style);
	tab_bar->add_theme_style_override(SNAME("tab_hovered"), theme_cache.tab_hovered_style);
	tab_bar->add_theme_style_override(SNAME("tab_selected"), theme_cache.tab_selected_style);
	tab_bar->add_theme_style_override(SNAME("tab_disabled"), theme_cache.tab_disabled_style);
	tab_bar->add_theme_style_override(SNAME("tab_focus"), theme_cache.tab_focus_style);

	tab_bar->add_theme_icon_override(SNAME("increment"), theme_cache.increment_icon);
	tab_bar->add_theme_icon_override(SNAME("increment_highlight"), theme_cache.increment_hl_icon);
	tab_bar->add_theme_icon_override(SNAME("decrement"), theme_cache.decrement_icon);
	tab_bar->add_theme_icon_override(SNAME("decrement_highlight"), theme_cache.decrement_hl_icon);
	tab_bar->add_theme_icon_override(SNAME("drop_mark"), theme_cache.drop_mark_icon);
	tab_bar->add_theme_color_override(SNAME("drop_mark_color"), theme_cache.drop_mark_color);

	tab_bar->add_theme_color_override(SNAME("font_selected_color"), theme_cache.font_selected_color);
	tab_bar->add_theme_color_override(SNAME("font_hovered_color"), theme_cache.font_hovered_color);
	tab_bar->add_theme_color_override(SNAME("font_unselected_color"), theme_cache.font_unselected_color);
	tab_bar->add_theme_color_override(SNAME("font_disabled_color"), theme_cache.font_disabled_color);
	tab_bar->add_theme_color_override(SNAME("font_outline_color"), theme_cache.font_outline_color);

	tab_bar->add_theme_font_override(SceneStringName(font), theme_cache.tab_font);
	tab_bar->add_theme_font_size_override(SceneStringName(font_size), theme_cache.tab_font_size);

	tab_bar->add_theme_constant_override(SNAME("h_separation"), theme_cache.icon_separation);
	tab_bar->add_theme_constant_override(SNAME("icon_max_width"), theme_cache.icon_max_width);
	tab_bar->add_theme_constant_override(SNAME("outline_size"), theme_cache.outline_size);

	tab_bar->end_bulk_theme_override();

	_update_margins();
	if (get_tab_count() > 0) {
		_repaint();
	} else {
		update_minimum_size();
	}
	queue_redraw();

	theme_changing = false;
}

void TabContainer::_repaint() {
	Vector<Control *> controls = _get_tab_controls();
	int current = get_current_tab();

	// Move the TabBar to the top or bottom.
	// Don't change the left and right offsets since the TabBar will resize and may change tab offset.
	if (tabs_position == POSITION_BOTTOM) {
		tab_bar->set_anchor_and_offset(SIDE_BOTTOM, 1.0, 0.0);
		tab_bar->set_anchor_and_offset(SIDE_TOP, 1.0, -_get_tab_height());
	} else {
		tab_bar->set_anchor_and_offset(SIDE_BOTTOM, 0.0, _get_tab_height());
		tab_bar->set_anchor_and_offset(SIDE_TOP, 0.0, 0.0);
	}

	updating_visibility = true;
	for (int i = 0; i < controls.size(); i++) {
		Control *c = controls[i];

		if (i == current) {
			c->show();
			c->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

			if (tabs_visible) {
				if (tabs_position == POSITION_BOTTOM) {
					c->set_offset(SIDE_BOTTOM, -_get_tab_height());
				} else {
					c->set_offset(SIDE_TOP, _get_tab_height());
				}
			}

			c->set_offset(SIDE_TOP, c->get_offset(SIDE_TOP) + theme_cache.panel_style->get_margin(SIDE_TOP));
			c->set_offset(SIDE_LEFT, c->get_offset(SIDE_LEFT) + theme_cache.panel_style->get_margin(SIDE_LEFT));
			c->set_offset(SIDE_RIGHT, c->get_offset(SIDE_RIGHT) - theme_cache.panel_style->get_margin(SIDE_RIGHT));
			c->set_offset(SIDE_BOTTOM, c->get_offset(SIDE_BOTTOM) - theme_cache.panel_style->get_margin(SIDE_BOTTOM));
		} else {
			c->hide();
		}
	}
	updating_visibility = false;

	update_minimum_size();
}

void TabContainer::_update_margins() {
	int menu_width = theme_cache.menu_icon->get_width();

	// Directly check for validity, to avoid errors when quitting.
	bool has_popup = popup_obj_id.is_valid();

	if (get_tab_count() == 0) {
		tab_bar->set_offset(SIDE_LEFT, 0);
		tab_bar->set_offset(SIDE_RIGHT, has_popup ? -menu_width : 0);

		return;
	}

	switch (get_tab_alignment()) {
		case TabBar::ALIGNMENT_LEFT: {
			tab_bar->set_offset(SIDE_LEFT, theme_cache.side_margin);
			tab_bar->set_offset(SIDE_RIGHT, has_popup ? -menu_width : 0);
		} break;

		case TabBar::ALIGNMENT_CENTER: {
			tab_bar->set_offset(SIDE_LEFT, 0);
			tab_bar->set_offset(SIDE_RIGHT, has_popup ? -menu_width : 0);
		} break;

		case TabBar::ALIGNMENT_RIGHT: {
			tab_bar->set_offset(SIDE_LEFT, 0);

			if (has_popup) {
				tab_bar->set_offset(SIDE_RIGHT, -menu_width);
				return;
			}

			int first_tab_pos = tab_bar->get_tab_rect(0).position.x;
			Rect2 last_tab_rect = tab_bar->get_tab_rect(get_tab_count() - 1);
			int total_tabs_width = last_tab_rect.position.x - first_tab_pos + last_tab_rect.size.width;

			// Calculate if all the tabs would still fit if the margin was present.
			if (get_clip_tabs() && (tab_bar->get_offset_buttons_visible() || (get_tab_count() > 1 && (total_tabs_width + theme_cache.side_margin) > get_size().width))) {
				tab_bar->set_offset(SIDE_RIGHT, has_popup ? -menu_width : 0);
			} else {
				tab_bar->set_offset(SIDE_RIGHT, -theme_cache.side_margin);
			}
		} break;

		case TabBar::ALIGNMENT_MAX:
			break; // Can't happen, but silences warning.
	}
}

void TabContainer::_on_mouse_exited() {
	if (menu_hovered) {
		menu_hovered = false;
		queue_redraw();
	}
}

Vector<Control *> TabContainer::_get_tab_controls() const {
	Vector<Control *> controls;
	for (int i = 0; i < get_child_count(); i++) {
		Control *control = as_sortable_control(get_child(i), SortableVisbilityMode::IGNORE);
		if (!control || control == tab_bar || children_removing.has(control)) {
			continue;
		}

		controls.push_back(control);
	}

	return controls;
}

Variant TabContainer::_get_drag_data_fw(const Point2 &p_point, Control *p_from_control) {
	if (!drag_to_rearrange_enabled) {
		return Variant();
	}
	return tab_bar->_handle_get_drag_data("tab_container_tab", p_point);
}

bool TabContainer::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control) const {
	if (!drag_to_rearrange_enabled) {
		return false;
	}
	return tab_bar->_handle_can_drop_data("tab_container_tab", p_point, p_data);
}

void TabContainer::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control) {
	if (!drag_to_rearrange_enabled) {
		return;
	}
	return tab_bar->_handle_drop_data("tab_container_tab", p_point, p_data, callable_mp(this, &TabContainer::_drag_move_tab), callable_mp(this, &TabContainer::_drag_move_tab_from));
}

void TabContainer::_drag_move_tab(int p_from_index, int p_to_index) {
	move_child(get_tab_control(p_from_index), get_tab_control(p_to_index)->get_index(false));
}

void TabContainer::_drag_move_tab_from(TabBar *p_from_tabbar, int p_from_index, int p_to_index) {
	Node *parent = p_from_tabbar->get_parent();
	if (!parent) {
		return;
	}
	TabContainer *from_tab_container = Object::cast_to<TabContainer>(parent);
	if (!from_tab_container) {
		return;
	}
	move_tab_from_tab_container(from_tab_container, p_from_index, p_to_index);
}

void TabContainer::move_tab_from_tab_container(TabContainer *p_from, int p_from_index, int p_to_index) {
	ERR_FAIL_NULL(p_from);
	ERR_FAIL_INDEX(p_from_index, p_from->get_tab_count());
	ERR_FAIL_INDEX(p_to_index, get_tab_count() + 1);

	// Get the tab properties before they get erased by the child removal.
	String tab_title = p_from->get_tab_title(p_from_index);
	String tab_tooltip = p_from->get_tab_tooltip(p_from_index);
	Ref<Texture2D> tab_icon = p_from->get_tab_icon(p_from_index);
	Ref<Texture2D> tab_button_icon = p_from->get_tab_button_icon(p_from_index);
	bool tab_disabled = p_from->is_tab_disabled(p_from_index);
	bool tab_hidden = p_from->is_tab_hidden(p_from_index);
	Variant tab_metadata = p_from->get_tab_metadata(p_from_index);
	int tab_icon_max_width = p_from->get_tab_bar()->get_tab_icon_max_width(p_from_index);

	Control *moving_tabc = p_from->get_tab_control(p_from_index);
	p_from->remove_child(moving_tabc);
	add_child(moving_tabc, true);

	if (p_to_index < 0 || p_to_index > get_tab_count() - 1) {
		p_to_index = get_tab_count() - 1;
	}
	move_child(moving_tabc, get_tab_control(p_to_index)->get_index(false));

	set_tab_title(p_to_index, tab_title);
	set_tab_tooltip(p_to_index, tab_tooltip);
	set_tab_icon(p_to_index, tab_icon);
	set_tab_button_icon(p_to_index, tab_button_icon);
	set_tab_disabled(p_to_index, tab_disabled);
	set_tab_hidden(p_to_index, tab_hidden);
	set_tab_metadata(p_to_index, tab_metadata);
	get_tab_bar()->set_tab_icon_max_width(p_to_index, tab_icon_max_width);

	if (!is_tab_disabled(p_to_index)) {
		set_current_tab(p_to_index);
	}
}

void TabContainer::_on_tab_clicked(int p_tab) {
	emit_signal(SNAME("tab_clicked"), p_tab);
}

void TabContainer::_on_tab_hovered(int p_tab) {
	emit_signal(SNAME("tab_hovered"), p_tab);
}

void TabContainer::_on_tab_changed(int p_tab) {
	callable_mp(this, &TabContainer::_repaint).call_deferred();
	queue_redraw();

	emit_signal(SNAME("tab_changed"), p_tab);
}

void TabContainer::_on_tab_selected(int p_tab) {
	if (p_tab != get_previous_tab()) {
		callable_mp(this, &TabContainer::_repaint).call_deferred();
	}

	emit_signal(SNAME("tab_selected"), p_tab);
}

void TabContainer::_on_tab_button_pressed(int p_tab) {
	emit_signal(SNAME("tab_button_pressed"), p_tab);
}

void TabContainer::_on_active_tab_rearranged(int p_tab) {
	emit_signal(SNAME("active_tab_rearranged"), p_tab);
}

void TabContainer::_on_tab_visibility_changed(Control *p_child) {
	if (updating_visibility) {
		return;
	}
	int tab_index = get_tab_idx_from_control(p_child);
	if (tab_index == -1) {
		return;
	}
	// Only allow one tab to be visible.
	bool made_visible = p_child->is_visible();
	updating_visibility = true;

	if (!made_visible && get_current_tab() == tab_index) {
		if (get_deselect_enabled() || get_tab_count() == 0) {
			// Deselect.
			set_current_tab(-1);
		} else if (get_tab_count() == 1) {
			// Only tab, cannot deselect.
			p_child->show();
		} else {
			// Set a different tab to be the current tab.
			bool selected = select_next_available();
			if (!selected) {
				selected = select_previous_available();
			}
			if (!selected) {
				// No available tabs, deselect.
				set_current_tab(-1);
			}
		}
	} else if (made_visible && get_current_tab() != tab_index) {
		set_current_tab(tab_index);
	}

	updating_visibility = false;
}

void TabContainer::_refresh_tab_indices() {
	Vector<Control *> controls = _get_tab_controls();
	for (int i = 0; i < controls.size(); i++) {
		controls[i]->set_meta("_tab_index", i);
	}
}

void TabContainer::_refresh_tab_names() {
	Vector<Control *> controls = _get_tab_controls();
	for (int i = 0; i < controls.size(); i++) {
		if (!controls[i]->has_meta("_tab_name") && String(controls[i]->get_name()) != get_tab_title(i)) {
			tab_bar->set_tab_title(i, controls[i]->get_name());
		}
	}
}

void TabContainer::add_child_notify(Node *p_child) {
	Container::add_child_notify(p_child);

	if (p_child == tab_bar) {
		return;
	}

	Control *c = as_sortable_control(p_child, SortableVisbilityMode::IGNORE);
	if (!c) {
		return;
	}
	c->hide();

	tab_bar->add_tab(p_child->get_name());
	c->set_meta("_tab_index", tab_bar->get_tab_count() - 1);

	_update_margins();
	if (get_tab_count() == 1) {
		queue_redraw();
	}

	p_child->connect("renamed", callable_mp(this, &TabContainer::_refresh_tab_names));
	p_child->connect(SceneStringName(visibility_changed), callable_mp(this, &TabContainer::_on_tab_visibility_changed).bind(c));

	// TabBar won't emit the "tab_changed" signal when not inside the tree.
	if (!is_inside_tree()) {
		callable_mp(this, &TabContainer::_repaint).call_deferred();
	}
}

void TabContainer::move_child_notify(Node *p_child) {
	Container::move_child_notify(p_child);

	if (p_child == tab_bar) {
		return;
	}

	Control *c = as_sortable_control(p_child, SortableVisbilityMode::IGNORE);
	if (c) {
		tab_bar->move_tab(c->get_meta("_tab_index"), get_tab_idx_from_control(c));
	}

	_refresh_tab_indices();
}

void TabContainer::remove_child_notify(Node *p_child) {
	Container::remove_child_notify(p_child);

	if (p_child == tab_bar) {
		return;
	}

	Control *c = as_sortable_control(p_child, SortableVisbilityMode::IGNORE);
	if (!c) {
		return;
	}

	int idx = get_tab_idx_from_control(c);

	// As the child hasn't been removed yet, keep track of it so when the "tab_changed" signal is fired it can be ignored.
	children_removing.push_back(c);

	tab_bar->remove_tab(idx);
	_refresh_tab_indices();

	children_removing.erase(c);

	_update_margins();
	if (get_tab_count() == 0) {
		queue_redraw();
	}

	p_child->remove_meta("_tab_index");
	p_child->remove_meta("_tab_name");
	p_child->disconnect("renamed", callable_mp(this, &TabContainer::_refresh_tab_names));
	p_child->disconnect(SceneStringName(visibility_changed), callable_mp(this, &TabContainer::_on_tab_visibility_changed));

	// TabBar won't emit the "tab_changed" signal when not inside the tree.
	if (!is_inside_tree()) {
		callable_mp(this, &TabContainer::_repaint).call_deferred();
	}
}

TabBar *TabContainer::get_tab_bar() const {
	return tab_bar;
}

int TabContainer::get_tab_count() const {
	return tab_bar->get_tab_count();
}

void TabContainer::set_current_tab(int p_current) {
	if (!is_inside_tree()) {
		setup_current_tab = p_current;
		return;
	}

	tab_bar->set_current_tab(p_current);
}

int TabContainer::get_current_tab() const {
	return tab_bar->get_current_tab();
}

int TabContainer::get_previous_tab() const {
	return tab_bar->get_previous_tab();
}

bool TabContainer::select_previous_available() {
	return tab_bar->select_previous_available();
}

bool TabContainer::select_next_available() {
	return tab_bar->select_next_available();
}

void TabContainer::set_deselect_enabled(bool p_enabled) {
	tab_bar->set_deselect_enabled(p_enabled);
}

bool TabContainer::get_deselect_enabled() const {
	return tab_bar->get_deselect_enabled();
}

Control *TabContainer::get_tab_control(int p_idx) const {
	Vector<Control *> controls = _get_tab_controls();
	if (p_idx >= 0 && p_idx < controls.size()) {
		return controls[p_idx];
	} else {
		return nullptr;
	}
}

Control *TabContainer::get_current_tab_control() const {
	return get_tab_control(tab_bar->get_current_tab());
}

int TabContainer::get_tab_idx_at_point(const Point2 &p_point) const {
	return tab_bar->get_tab_idx_at_point(p_point);
}

int TabContainer::get_tab_idx_from_control(Control *p_child) const {
	ERR_FAIL_NULL_V(p_child, -1);
	ERR_FAIL_COND_V(p_child->get_parent() != this, -1);

	Vector<Control *> controls = _get_tab_controls();
	for (int i = 0; i < controls.size(); i++) {
		if (controls[i] == p_child) {
			return i;
		}
	}

	return -1;
}

void TabContainer::set_tab_alignment(TabBar::AlignmentMode p_alignment) {
	if (tab_bar->get_tab_alignment() == p_alignment) {
		return;
	}

	tab_bar->set_tab_alignment(p_alignment);
	_update_margins();
}

TabBar::AlignmentMode TabContainer::get_tab_alignment() const {
	return tab_bar->get_tab_alignment();
}

void TabContainer::set_tabs_position(TabPosition p_tabs_position) {
	ERR_FAIL_INDEX(p_tabs_position, POSITION_MAX);
	if (p_tabs_position == tabs_position) {
		return;
	}
	tabs_position = p_tabs_position;

	tab_bar->set_tab_style_v_flip(tabs_position == POSITION_BOTTOM);

	callable_mp(this, &TabContainer::_repaint).call_deferred();
	queue_redraw();
}

TabContainer::TabPosition TabContainer::get_tabs_position() const {
	return tabs_position;
}

void TabContainer::set_tab_focus_mode(Control::FocusMode p_focus_mode) {
	tab_bar->set_focus_mode(p_focus_mode);
}

Control::FocusMode TabContainer::get_tab_focus_mode() const {
	return tab_bar->get_focus_mode();
}

void TabContainer::set_clip_tabs(bool p_clip_tabs) {
	tab_bar->set_clip_tabs(p_clip_tabs);
}

bool TabContainer::get_clip_tabs() const {
	return tab_bar->get_clip_tabs();
}

void TabContainer::set_tabs_visible(bool p_visible) {
	if (p_visible == tabs_visible) {
		return;
	}

	tabs_visible = p_visible;
	tab_bar->set_visible(tabs_visible);

	callable_mp(this, &TabContainer::_repaint).call_deferred();
	queue_redraw();
}

bool TabContainer::are_tabs_visible() const {
	return tabs_visible;
}

void TabContainer::set_all_tabs_in_front(bool p_in_front) {
	if (p_in_front == all_tabs_in_front) {
		return;
	}

	all_tabs_in_front = p_in_front;

	remove_child(tab_bar);
	add_child(tab_bar, false, all_tabs_in_front ? INTERNAL_MODE_FRONT : INTERNAL_MODE_BACK);
}

bool TabContainer::is_all_tabs_in_front() const {
	return all_tabs_in_front;
}

void TabContainer::set_tab_title(int p_tab, const String &p_title) {
	Control *child = get_tab_control(p_tab);
	ERR_FAIL_NULL(child);

	if (tab_bar->get_tab_title(p_tab) == p_title) {
		return;
	}

	tab_bar->set_tab_title(p_tab, p_title);

	if (p_title == child->get_name()) {
		child->remove_meta("_tab_name");
	} else {
		child->set_meta("_tab_name", p_title);
	}

	_repaint();
	queue_redraw();
}

String TabContainer::get_tab_title(int p_tab) const {
	return tab_bar->get_tab_title(p_tab);
}

void TabContainer::set_tab_tooltip(int p_tab, const String &p_tooltip) {
	tab_bar->set_tab_tooltip(p_tab, p_tooltip);
}

String TabContainer::get_tab_tooltip(int p_tab) const {
	return tab_bar->get_tab_tooltip(p_tab);
}

void TabContainer::set_tab_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	if (tab_bar->get_tab_icon(p_tab) == p_icon) {
		return;
	}

	tab_bar->set_tab_icon(p_tab, p_icon);

	_update_margins();
	_repaint();
	queue_redraw();
}

Ref<Texture2D> TabContainer::get_tab_icon(int p_tab) const {
	return tab_bar->get_tab_icon(p_tab);
}

void TabContainer::set_tab_icon_max_width(int p_tab, int p_width) {
	if (tab_bar->get_tab_icon_max_width(p_tab) == p_width) {
		return;
	}

	tab_bar->set_tab_icon_max_width(p_tab, p_width);

	_update_margins();
	_repaint();
	queue_redraw();
}

int TabContainer::get_tab_icon_max_width(int p_tab) const {
	return tab_bar->get_tab_icon_max_width(p_tab);
}

void TabContainer::set_tab_disabled(int p_tab, bool p_disabled) {
	if (tab_bar->is_tab_disabled(p_tab) == p_disabled) {
		return;
	}

	tab_bar->set_tab_disabled(p_tab, p_disabled);

	_update_margins();
	if (!get_clip_tabs()) {
		update_minimum_size();
	}
}

bool TabContainer::is_tab_disabled(int p_tab) const {
	return tab_bar->is_tab_disabled(p_tab);
}

void TabContainer::set_tab_hidden(int p_tab, bool p_hidden) {
	Control *child = get_tab_control(p_tab);
	ERR_FAIL_NULL(child);

	if (tab_bar->is_tab_hidden(p_tab) == p_hidden) {
		return;
	}

	tab_bar->set_tab_hidden(p_tab, p_hidden);
	child->hide();

	_update_margins();
	if (!get_clip_tabs()) {
		update_minimum_size();
	}
	callable_mp(this, &TabContainer::_repaint).call_deferred();
}

bool TabContainer::is_tab_hidden(int p_tab) const {
	return tab_bar->is_tab_hidden(p_tab);
}

void TabContainer::set_tab_metadata(int p_tab, const Variant &p_metadata) {
	tab_bar->set_tab_metadata(p_tab, p_metadata);
}

Variant TabContainer::get_tab_metadata(int p_tab) const {
	return tab_bar->get_tab_metadata(p_tab);
}

void TabContainer::set_tab_button_icon(int p_tab, const Ref<Texture2D> &p_icon) {
	tab_bar->set_tab_button_icon(p_tab, p_icon);

	_update_margins();
	_repaint();
}

Ref<Texture2D> TabContainer::get_tab_button_icon(int p_tab) const {
	return tab_bar->get_tab_button_icon(p_tab);
}

Size2 TabContainer::get_minimum_size() const {
	Size2 ms;

	if (tabs_visible) {
		ms = tab_bar->get_minimum_size();

		if (!get_clip_tabs()) {
			if (get_popup()) {
				ms.x += theme_cache.menu_icon->get_width();
			}

			if (theme_cache.side_margin > 0 && get_tab_alignment() != TabBar::ALIGNMENT_CENTER &&
					(get_tab_alignment() != TabBar::ALIGNMENT_RIGHT || !get_popup())) {
				ms.x += theme_cache.side_margin;
			}
		}
	}

	Vector<Control *> controls = _get_tab_controls();
	Size2 largest_child_min_size;
	for (int i = 0; i < controls.size(); i++) {
		Control *c = controls[i];

		if (!c->is_visible() && !use_hidden_tabs_for_min_size) {
			continue;
		}

		Size2 cms = c->get_combined_minimum_size();
		largest_child_min_size = largest_child_min_size.max(cms);
	}
	ms.y += largest_child_min_size.y;

	Size2 panel_ms = theme_cache.panel_style->get_minimum_size();

	ms.x = MAX(ms.x, largest_child_min_size.x + panel_ms.x);
	ms.y += panel_ms.y;

	return ms;
}

void TabContainer::set_popup(Node *p_popup) {
	bool had_popup = get_popup();

	Popup *popup = Object::cast_to<Popup>(p_popup);
	ObjectID popup_id = popup ? popup->get_instance_id() : ObjectID();
	if (popup_obj_id == popup_id) {
		return;
	}
	popup_obj_id = popup_id;

	if (had_popup != bool(popup)) {
		queue_redraw();
		_update_margins();
		if (!get_clip_tabs()) {
			update_minimum_size();
		}
	}
}

Popup *TabContainer::get_popup() const {
	if (popup_obj_id.is_valid()) {
		Popup *popup = Object::cast_to<Popup>(ObjectDB::get_instance(popup_obj_id));
		if (popup) {
			return popup;
		} else {
#ifdef DEBUG_ENABLED
			ERR_PRINT("Popup assigned to TabContainer is gone!");
#endif
			popup_obj_id = ObjectID();
		}
	}

	return nullptr;
}

void TabContainer::set_drag_to_rearrange_enabled(bool p_enabled) {
	drag_to_rearrange_enabled = p_enabled;
}

bool TabContainer::get_drag_to_rearrange_enabled() const {
	return drag_to_rearrange_enabled;
}

void TabContainer::set_tabs_rearrange_group(int p_group_id) {
	tab_bar->set_tabs_rearrange_group(p_group_id);
}

int TabContainer::get_tabs_rearrange_group() const {
	return tab_bar->get_tabs_rearrange_group();
}

void TabContainer::set_use_hidden_tabs_for_min_size(bool p_use_hidden_tabs) {
	if (use_hidden_tabs_for_min_size == p_use_hidden_tabs) {
		return;
	}

	use_hidden_tabs_for_min_size = p_use_hidden_tabs;
	update_minimum_size();
}

bool TabContainer::get_use_hidden_tabs_for_min_size() const {
	return use_hidden_tabs_for_min_size;
}

Vector<int> TabContainer::get_allowed_size_flags_horizontal() const {
	return Vector<int>();
}

Vector<int> TabContainer::get_allowed_size_flags_vertical() const {
	return Vector<int>();
}

void TabContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tab_count"), &TabContainer::get_tab_count);
	ClassDB::bind_method(D_METHOD("set_current_tab", "tab_idx"), &TabContainer::set_current_tab);
	ClassDB::bind_method(D_METHOD("get_current_tab"), &TabContainer::get_current_tab);
	ClassDB::bind_method(D_METHOD("get_previous_tab"), &TabContainer::get_previous_tab);
	ClassDB::bind_method(D_METHOD("select_previous_available"), &TabContainer::select_previous_available);
	ClassDB::bind_method(D_METHOD("select_next_available"), &TabContainer::select_next_available);
	ClassDB::bind_method(D_METHOD("get_current_tab_control"), &TabContainer::get_current_tab_control);
	ClassDB::bind_method(D_METHOD("get_tab_bar"), &TabContainer::get_tab_bar);
	ClassDB::bind_method(D_METHOD("get_tab_control", "tab_idx"), &TabContainer::get_tab_control);
	ClassDB::bind_method(D_METHOD("set_tab_alignment", "alignment"), &TabContainer::set_tab_alignment);
	ClassDB::bind_method(D_METHOD("get_tab_alignment"), &TabContainer::get_tab_alignment);
	ClassDB::bind_method(D_METHOD("set_tabs_position", "tabs_position"), &TabContainer::set_tabs_position);
	ClassDB::bind_method(D_METHOD("get_tabs_position"), &TabContainer::get_tabs_position);
	ClassDB::bind_method(D_METHOD("set_clip_tabs", "clip_tabs"), &TabContainer::set_clip_tabs);
	ClassDB::bind_method(D_METHOD("get_clip_tabs"), &TabContainer::get_clip_tabs);
	ClassDB::bind_method(D_METHOD("set_tabs_visible", "visible"), &TabContainer::set_tabs_visible);
	ClassDB::bind_method(D_METHOD("are_tabs_visible"), &TabContainer::are_tabs_visible);
	ClassDB::bind_method(D_METHOD("set_all_tabs_in_front", "is_front"), &TabContainer::set_all_tabs_in_front);
	ClassDB::bind_method(D_METHOD("is_all_tabs_in_front"), &TabContainer::is_all_tabs_in_front);
	ClassDB::bind_method(D_METHOD("set_tab_title", "tab_idx", "title"), &TabContainer::set_tab_title);
	ClassDB::bind_method(D_METHOD("get_tab_title", "tab_idx"), &TabContainer::get_tab_title);
	ClassDB::bind_method(D_METHOD("set_tab_tooltip", "tab_idx", "tooltip"), &TabContainer::set_tab_tooltip);
	ClassDB::bind_method(D_METHOD("get_tab_tooltip", "tab_idx"), &TabContainer::get_tab_tooltip);
	ClassDB::bind_method(D_METHOD("set_tab_icon", "tab_idx", "icon"), &TabContainer::set_tab_icon);
	ClassDB::bind_method(D_METHOD("get_tab_icon", "tab_idx"), &TabContainer::get_tab_icon);
	ClassDB::bind_method(D_METHOD("set_tab_icon_max_width", "tab_idx", "width"), &TabContainer::set_tab_icon_max_width);
	ClassDB::bind_method(D_METHOD("get_tab_icon_max_width", "tab_idx"), &TabContainer::get_tab_icon_max_width);
	ClassDB::bind_method(D_METHOD("set_tab_disabled", "tab_idx", "disabled"), &TabContainer::set_tab_disabled);
	ClassDB::bind_method(D_METHOD("is_tab_disabled", "tab_idx"), &TabContainer::is_tab_disabled);
	ClassDB::bind_method(D_METHOD("set_tab_hidden", "tab_idx", "hidden"), &TabContainer::set_tab_hidden);
	ClassDB::bind_method(D_METHOD("is_tab_hidden", "tab_idx"), &TabContainer::is_tab_hidden);
	ClassDB::bind_method(D_METHOD("set_tab_metadata", "tab_idx", "metadata"), &TabContainer::set_tab_metadata);
	ClassDB::bind_method(D_METHOD("get_tab_metadata", "tab_idx"), &TabContainer::get_tab_metadata);
	ClassDB::bind_method(D_METHOD("set_tab_button_icon", "tab_idx", "icon"), &TabContainer::set_tab_button_icon);
	ClassDB::bind_method(D_METHOD("get_tab_button_icon", "tab_idx"), &TabContainer::get_tab_button_icon);
	ClassDB::bind_method(D_METHOD("get_tab_idx_at_point", "point"), &TabContainer::get_tab_idx_at_point);
	ClassDB::bind_method(D_METHOD("get_tab_idx_from_control", "control"), &TabContainer::get_tab_idx_from_control);
	ClassDB::bind_method(D_METHOD("set_popup", "popup"), &TabContainer::set_popup);
	ClassDB::bind_method(D_METHOD("get_popup"), &TabContainer::get_popup);
	ClassDB::bind_method(D_METHOD("set_drag_to_rearrange_enabled", "enabled"), &TabContainer::set_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("get_drag_to_rearrange_enabled"), &TabContainer::get_drag_to_rearrange_enabled);
	ClassDB::bind_method(D_METHOD("set_tabs_rearrange_group", "group_id"), &TabContainer::set_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("get_tabs_rearrange_group"), &TabContainer::get_tabs_rearrange_group);
	ClassDB::bind_method(D_METHOD("set_use_hidden_tabs_for_min_size", "enabled"), &TabContainer::set_use_hidden_tabs_for_min_size);
	ClassDB::bind_method(D_METHOD("get_use_hidden_tabs_for_min_size"), &TabContainer::get_use_hidden_tabs_for_min_size);
	ClassDB::bind_method(D_METHOD("set_tab_focus_mode", "focus_mode"), &TabContainer::set_tab_focus_mode);
	ClassDB::bind_method(D_METHOD("get_tab_focus_mode"), &TabContainer::get_tab_focus_mode);
	ClassDB::bind_method(D_METHOD("set_deselect_enabled", "enabled"), &TabContainer::set_deselect_enabled);
	ClassDB::bind_method(D_METHOD("get_deselect_enabled"), &TabContainer::get_deselect_enabled);

	ADD_SIGNAL(MethodInfo("active_tab_rearranged", PropertyInfo(Variant::INT, "idx_to")));
	ADD_SIGNAL(MethodInfo("tab_changed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_clicked", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_hovered", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_selected", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("tab_button_pressed", PropertyInfo(Variant::INT, "tab")));
	ADD_SIGNAL(MethodInfo("pre_popup_pressed"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_tab_alignment", "get_tab_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_tab", PROPERTY_HINT_RANGE, "-1,4096,1"), "set_current_tab", "get_current_tab");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tabs_position", PROPERTY_HINT_ENUM, "Top,Bottom"), "set_tabs_position", "get_tabs_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_tabs"), "set_clip_tabs", "get_clip_tabs");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tabs_visible"), "set_tabs_visible", "are_tabs_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "all_tabs_in_front"), "set_all_tabs_in_front", "is_all_tabs_in_front");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_to_rearrange_enabled"), "set_drag_to_rearrange_enabled", "get_drag_to_rearrange_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tabs_rearrange_group"), "set_tabs_rearrange_group", "get_tabs_rearrange_group");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_hidden_tabs_for_min_size"), "set_use_hidden_tabs_for_min_size", "get_use_hidden_tabs_for_min_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_focus_mode", PROPERTY_HINT_ENUM, "None,Click,All"), "set_tab_focus_mode", "get_tab_focus_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_enabled"), "set_deselect_enabled", "get_deselect_enabled");

	BIND_ENUM_CONSTANT(POSITION_TOP);
	BIND_ENUM_CONSTANT(POSITION_BOTTOM);
	BIND_ENUM_CONSTANT(POSITION_MAX);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabContainer, side_margin);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, panel_style, "panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tabbar_style, "tabbar_background");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, menu_icon, "menu");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, menu_hl_icon, "menu_highlight");

	// TabBar overrides.
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabContainer, icon_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabContainer, icon_max_width);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tab_unselected_style, "tab_unselected");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tab_hovered_style, "tab_hovered");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tab_selected_style, "tab_selected");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tab_disabled_style, "tab_disabled");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, TabContainer, tab_focus_style, "tab_focus");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, increment_icon, "increment");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, increment_hl_icon, "increment_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, decrement_icon, "decrement");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, decrement_hl_icon, "decrement_highlight");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_ICON, TabContainer, drop_mark_icon, "drop_mark");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, drop_mark_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, font_selected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, font_hovered_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, font_unselected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, font_disabled_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, TabContainer, font_outline_color);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT, TabContainer, tab_font, "font");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT_SIZE, TabContainer, tab_font_size, "font_size");
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, TabContainer, outline_size);
}

TabContainer::TabContainer() {
	tab_bar = memnew(TabBar);
	SET_DRAG_FORWARDING_GCDU(tab_bar, TabContainer);
	add_child(tab_bar, false, INTERNAL_MODE_FRONT);
	tab_bar->set_anchors_and_offsets_preset(Control::PRESET_TOP_WIDE);
	tab_bar->connect("tab_changed", callable_mp(this, &TabContainer::_on_tab_changed));
	tab_bar->connect("tab_clicked", callable_mp(this, &TabContainer::_on_tab_clicked));
	tab_bar->connect("tab_hovered", callable_mp(this, &TabContainer::_on_tab_hovered));
	tab_bar->connect("tab_selected", callable_mp(this, &TabContainer::_on_tab_selected));
	tab_bar->connect("tab_button_pressed", callable_mp(this, &TabContainer::_on_tab_button_pressed));
	tab_bar->connect("active_tab_rearranged", callable_mp(this, &TabContainer::_on_active_tab_rearranged));

	connect(SceneStringName(mouse_exited), callable_mp(this, &TabContainer::_on_mouse_exited));
}
