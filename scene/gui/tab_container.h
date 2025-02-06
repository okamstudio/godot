/**************************************************************************/
/*  tab_container.h                                                       */
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

#pragma once

#include "scene/gui/container.h"
#include "scene/gui/popup.h"
#include "scene/gui/tab_bar.h"

class TabContainer : public Container {
	GDCLASS(TabContainer, Container);

public:
	enum TabPosition {
		POSITION_TOP,
		POSITION_BOTTOM,
		POSITION_MAX,
	};

private:
	TabBar *tab_bar = nullptr;
	bool tabs_visible = true;
	bool all_tabs_in_front = false;
	TabPosition tabs_position = POSITION_TOP;
	bool menu_hovered = false;
	mutable ObjectID popup_obj_id;
	bool use_hidden_tabs_for_min_size = false;
	bool theme_changing = false;
	Vector<Control *> children_removing;
	bool drag_to_rearrange_enabled = false;
	// Set the default setup current tab to be an invalid index.
	int setup_current_tab = -2;
	bool updating_visibility = false;

	struct ThemeCache {
		int side_margin = 0;

		Ref<StyleBox> panel_style;
		Ref<StyleBox> tabbar_style;

		Ref<Texture2D> menu_icon;
		Ref<Texture2D> menu_hl_icon;

		// TabBar overrides.
		int icon_separation = 0;
		int icon_max_width = 0;
		int outline_size = 0;

		Ref<StyleBox> tab_unselected_style;
		Ref<StyleBox> tab_hovered_style;
		Ref<StyleBox> tab_selected_style;
		Ref<StyleBox> tab_disabled_style;
		Ref<StyleBox> tab_focus_style;

		Ref<Texture2D> increment_icon;
		Ref<Texture2D> increment_hl_icon;
		Ref<Texture2D> decrement_icon;
		Ref<Texture2D> decrement_hl_icon;
		Ref<Texture2D> drop_mark_icon;
		Color drop_mark_color;

		Color font_selected_color;
		Color font_hovered_color;
		Color font_unselected_color;
		Color font_disabled_color;
		Color font_outline_color;

		Ref<Font> tab_font;
		int tab_font_size;
	} theme_cache;

	int _get_tab_height() const;
	Vector<Control *> _get_tab_controls() const;
	void _on_theme_changed();
	void _repaint();
	void _refresh_tab_indices();
	void _refresh_tab_names();
	void _update_margins();
	void _on_mouse_exited();
	void _on_tab_changed(int p_tab);
	void _on_tab_clicked(int p_tab);
	void _on_tab_hovered(int p_tab);
	void _on_tab_selected(int p_tab);
	void _on_tab_button_pressed(int p_tab);
	void _on_active_tab_rearranged(int p_tab);
	void _on_tab_visibility_changed(Control *p_child);

	Variant _get_drag_data_fw(const Point2 &p_point, Control *p_from_control);
	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control) const;
	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from_control);
	void _drag_move_tab(int p_from_index, int p_to_index);
	void _drag_move_tab_from(TabBar *p_from_tabbar, int p_from_index, int p_to_index);

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);
	virtual void add_child_notify(Node *p_child) override;
	virtual void move_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;
	static void _bind_methods();

public:
	TabBar *get_tab_bar() const;

	int get_tab_idx_at_point(const Point2 &p_point) const;
	int get_tab_idx_from_control(Control *p_child) const;

	void set_tab_alignment(TabBar::AlignmentMode p_alignment);
	TabBar::AlignmentMode get_tab_alignment() const;

	void set_tabs_position(TabPosition p_tab_position);
	TabPosition get_tabs_position() const;

	void set_tab_focus_mode(FocusMode p_focus_mode);
	FocusMode get_tab_focus_mode() const;

	void set_clip_tabs(bool p_clip_tabs);
	bool get_clip_tabs() const;

	void set_tabs_visible(bool p_visible);
	bool are_tabs_visible() const;

	void set_all_tabs_in_front(bool p_is_front);
	bool is_all_tabs_in_front() const;

	void set_tab_title(int p_tab, const String &p_title);
	String get_tab_title(int p_tab) const;

	void set_tab_tooltip(int p_tab, const String &p_tooltip);
	String get_tab_tooltip(int p_tab) const;

	void set_tab_icon(int p_tab, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_tab_icon(int p_tab) const;

	void set_tab_icon_max_width(int p_tab, int p_width);
	int get_tab_icon_max_width(int p_tab) const;

	void set_tab_disabled(int p_tab, bool p_disabled);
	bool is_tab_disabled(int p_tab) const;

	void set_tab_hidden(int p_tab, bool p_hidden);
	bool is_tab_hidden(int p_tab) const;

	void set_tab_metadata(int p_tab, const Variant &p_metadata);
	Variant get_tab_metadata(int p_tab) const;

	void set_tab_button_icon(int p_tab, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_tab_button_icon(int p_tab) const;

	int get_tab_count() const;
	void set_current_tab(int p_current);
	int get_current_tab() const;
	int get_previous_tab() const;

	bool select_previous_available();
	bool select_next_available();

	void set_deselect_enabled(bool p_enabled);
	bool get_deselect_enabled() const;

	Control *get_tab_control(int p_idx) const;
	Control *get_current_tab_control() const;

	virtual Size2 get_minimum_size() const override;

	void set_popup(Node *p_popup);
	Popup *get_popup() const;

	void move_tab_from_tab_container(TabContainer *p_from, int p_from_index, int p_to_index = -1);

	void set_drag_to_rearrange_enabled(bool p_enabled);
	bool get_drag_to_rearrange_enabled() const;
	void set_tabs_rearrange_group(int p_group_id);
	int get_tabs_rearrange_group() const;
	void set_use_hidden_tabs_for_min_size(bool p_use_hidden_tabs);
	bool get_use_hidden_tabs_for_min_size() const;

	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;

	TabContainer();
};

VARIANT_ENUM_CAST(TabContainer::TabPosition);
