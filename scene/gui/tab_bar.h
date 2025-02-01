/**************************************************************************/
/*  tab_bar.h                                                             */
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

#include "scene/gui/control.h"
#include "scene/property_list_helper.h"
#include "scene/resources/text_line.h"

class TabBar : public Control {
	GDCLASS(TabBar, Control);

public:
	enum AlignmentMode {
		ALIGNMENT_LEFT,
		ALIGNMENT_CENTER,
		ALIGNMENT_RIGHT,
		ALIGNMENT_MAX,
	};

	enum CloseButtonDisplayPolicy {
		CLOSE_BUTTON_SHOW_NEVER,
		CLOSE_BUTTON_SHOW_ACTIVE_ONLY,
		CLOSE_BUTTON_SHOW_ALWAYS,
		CLOSE_BUTTON_MAX
	};

private:
	struct Tab {
		String text;
		String tooltip;

		String language;
		Control::TextDirection text_direction = Control::TEXT_DIRECTION_INHERITED;

		Ref<TextLine> text_buf;
		Ref<Texture2D> icon;
		int icon_max_width = 0;

		bool disabled = false;
		bool hidden = false;
		bool truncated = false;

		Variant metadata;
		int ofs_cache = 0;
		int size_cache = 0;
		int size_text = 0;

		Ref<Texture2D> right_button;
		Rect2 rb_rect;
		Rect2 cb_rect;

		Tab() {
			text_buf.instantiate();
		}

		Tab(bool p_dummy) {}
	};

	static inline PropertyListHelper base_property_helper;
	PropertyListHelper property_helper;

	int offset = 0;
	int max_drawn_tab = 0;
	int highlight_arrow = -1;
	bool buttons_visible = false;
	bool missing_right = false;
	Vector<Tab> tabs;
	int current = -1;
	int previous = -1;
	AlignmentMode tab_alignment = ALIGNMENT_LEFT;
	bool clip_tabs = true;
	int rb_hover = -1;
	bool rb_pressing = false;
	bool tab_style_v_flip = false;

	bool select_with_rmb = false;
	bool deselect_enabled = false;

	int cb_hover = -1;
	bool cb_pressing = false;
	CloseButtonDisplayPolicy cb_displaypolicy = CLOSE_BUTTON_SHOW_NEVER;

	int hover = -1; // Hovered tab.
	int max_width = 0;
	bool scrolling_enabled = true;
	bool drag_to_rearrange_enabled = false;
	bool dragging_valid_tab = false;
	bool scroll_to_selected = true;
	int tabs_rearrange_group = -1;

	static const int CURRENT_TAB_UNINITIALIZED = -2;
	bool initialized = false;
	int queued_current = CURRENT_TAB_UNINITIALIZED;

	const float DEFAULT_GAMEPAD_EVENT_DELAY_MS = 0.5;
	const float GAMEPAD_EVENT_REPEAT_RATE_MS = 1.0 / 20;
	float gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;

	struct ThemeCache {
		int h_separation = 0;
		int icon_max_width = 0;

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

		Ref<Font> font;
		int font_size;
		int outline_size = 0;

		Color font_selected_color;
		Color font_hovered_color;
		Color font_unselected_color;
		Color font_disabled_color;
		Color font_outline_color;

		Ref<Texture2D> close_icon;
		Ref<StyleBox> button_pressed_style;
		Ref<StyleBox> button_hl_style;
	} theme_cache;

	int get_tab_width(int p_idx) const;
	Size2 _get_tab_icon_size(int p_idx) const;
	void _ensure_no_over_offset();
	bool _can_deselect() const;

	void _update_hover();
	void _update_cache(bool p_update_hover = true);

	void _on_mouse_exited();

	void _shape(int p_tab);
	void _draw_tab(Ref<StyleBox> &p_tab_style, Color &p_font_color, int p_index, float p_x, bool p_focus);

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual String get_tooltip(const Point2 &p_pos) const override;

	bool _set(const StringName &p_name, const Variant &p_value) { return property_helper.property_set_value(p_name, p_value); }
	bool _get(const StringName &p_name, Variant &r_ret) const { return property_helper.property_get_value(p_name, r_ret); }
	void _get_property_list(List<PropertyInfo> *p_list) const { property_helper.get_property_list(p_list); }
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }
	void _notification(int p_what);
	static void _bind_methods();

	Variant get_drag_data(const Point2 &p_point) override;
	bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	void drop_data(const Point2 &p_point, const Variant &p_data) override;
	void _move_tab_from(TabBar *p_from_tabbar, int p_from_index, int p_to_index);

public:
	Variant _handle_get_drag_data(const String &p_type, const Point2 &p_point);
	bool _handle_can_drop_data(const String &p_type, const Point2 &p_point, const Variant &p_data) const;
	void _handle_drop_data(const String &p_type, const Point2 &p_point, const Variant &p_data, const Callable &p_move_tab_callback, const Callable &p_move_tab_from_other_callback);

	void add_tab(const String &p_str = "", const Ref<Texture2D> &p_icon = Ref<Texture2D>());

	void set_tab_title(int p_tab, const String &p_title);
	String get_tab_title(int p_tab) const;

	void set_tab_tooltip(int p_tab, const String &p_tooltip);
	String get_tab_tooltip(int p_tab) const;

	void set_tab_text_direction(int p_tab, TextDirection p_text_direction);
	TextDirection get_tab_text_direction(int p_tab) const;

	void set_tab_language(int p_tab, const String &p_language);
	String get_tab_language(int p_tab) const;

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

	int get_tab_idx_at_point(const Point2 &p_point) const;

	void set_tab_alignment(AlignmentMode p_alignment);
	AlignmentMode get_tab_alignment() const;

	void set_clip_tabs(bool p_clip_tabs);
	bool get_clip_tabs() const;

	void set_tab_style_v_flip(bool p_tab_style_v_flip);

	void move_tab(int p_from, int p_to);

	void set_tab_close_display_policy(CloseButtonDisplayPolicy p_policy);
	CloseButtonDisplayPolicy get_tab_close_display_policy() const;

	void set_tab_count(int p_count);
	int get_tab_count() const;

	void set_current_tab(int p_current);
	int get_current_tab() const;
	int get_previous_tab() const;
	int get_hovered_tab() const;

	bool select_previous_available();
	bool select_next_available();

	void set_tab_offset(int p_offset);
	int get_tab_offset() const;
	bool get_offset_buttons_visible() const;

	void remove_tab(int p_idx);

	void clear_tabs();

	void set_scrolling_enabled(bool p_enabled);
	bool get_scrolling_enabled() const;

	void set_drag_to_rearrange_enabled(bool p_enabled);
	bool get_drag_to_rearrange_enabled() const;
	void set_tabs_rearrange_group(int p_group_id);
	int get_tabs_rearrange_group() const;

	void set_scroll_to_selected(bool p_enabled);
	bool get_scroll_to_selected() const;

	void set_select_with_rmb(bool p_enabled);
	bool get_select_with_rmb() const;

	void set_deselect_enabled(bool p_enabled);
	bool get_deselect_enabled() const;

	void ensure_tab_visible(int p_idx);

	void set_max_tab_width(int p_width);
	int get_max_tab_width() const;

	Rect2 get_tab_rect(int p_tab) const;
	Size2 get_minimum_size() const override;

	TabBar();
};

VARIANT_ENUM_CAST(TabBar::AlignmentMode);
VARIANT_ENUM_CAST(TabBar::CloseButtonDisplayPolicy);
