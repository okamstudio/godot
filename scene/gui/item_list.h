/**************************************************************************/
/*  item_list.h                                                           */
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

#ifndef ITEM_LIST_H
#define ITEM_LIST_H

#include "scene/gui/control.h"
#include "scene/gui/scroll_bar.h"
#include "scene/property_list_helper.h"
#include "scene/resources/text_paragraph.h"

class ItemList : public Control {
	GDCLASS(ItemList, Control);

public:
	enum IconMode {
		ICON_MODE_TOP,
		ICON_MODE_LEFT
	};

	enum SelectMode {
		SELECT_SINGLE,
		SELECT_MULTI
	};

private:
	struct Item {
		Ref<Texture2D> icon;
		bool icon_transposed = false;
		Rect2i icon_region;
		Color icon_modulate = Color(1, 1, 1, 1);
		Ref<Texture2D> tag_icon;
		String text;
		String xl_text;
		Ref<TextParagraph> text_buf;
		String language;
		TextDirection text_direction = TEXT_DIRECTION_AUTO;
		AutoTranslateMode auto_translate_mode = AUTO_TRANSLATE_MODE_INHERIT;

		bool selectable = true;
		bool selected = false;
		bool disabled = false;
		bool tooltip_enabled = true;
		Variant metadata;
		String tooltip;
		Color custom_fg;
		Color custom_bg = Color(0.0, 0.0, 0.0, 0.0);

		int column = 0;
		Rect2 rect_cache;
		Rect2 min_rect_cache;

		Size2 get_icon_size() const;

		bool operator<(const Item &p_another) const { return text < p_another.text; }

		Item() {
			text_buf.instantiate();
		}

		Item(bool p_dummy) {}
	};

	static inline PropertyListHelper base_property_helper;
	PropertyListHelper property_helper;

	int current = -1;
	int hovered = -1;

	bool shape_changed = true;

	bool ensure_selected_visible = false;
	bool same_column_width = false;
	bool allow_search = true;

	bool auto_width = false;
	float auto_width_value = 0.0;

	bool auto_height = false;
	float auto_height_value = 0.0;

	Vector<Item> items;
	Vector<int> separators;

	SelectMode select_mode = SELECT_SINGLE;
	IconMode icon_mode = ICON_MODE_LEFT;
	VScrollBar *scroll_bar = nullptr;
	TextServer::OverrunBehavior text_overrun_behavior = TextServer::OVERRUN_TRIM_ELLIPSIS;

	uint64_t search_time_msec = 0;
	String search_string;

	int current_columns = 1;
	int fixed_column_width = 0;
	int max_text_lines = 1;
	int max_columns = 1;

	Size2 fixed_icon_size;
	Size2 max_item_size_cache;
	Size2 fixed_tag_icon_size;

	int defer_select_single = -1;
	bool allow_rmb_select = false;
	bool allow_reselect = false;

	real_t icon_scale = 1.0;

	bool do_autoscroll_to_bottom = false;

	struct ThemeCache {
		int h_separation = 0;
		int v_separation = 0;

		Ref<StyleBox> panel_style;
		Ref<StyleBox> focus_style;

		Ref<Font> font;
		int font_size = 0;
		Color font_color;
		Color font_hovered_color;
		Color font_selected_color;
		int font_outline_size = 0;
		Color font_outline_color;

		int line_separation = 0;
		int icon_margin = 0;
		Ref<StyleBox> hovered_style;
		Ref<StyleBox> selected_style;
		Ref<StyleBox> selected_focus_style;
		Ref<StyleBox> cursor_style;
		Ref<StyleBox> cursor_focus_style;
		Color guide_color;
	} theme_cache;

	void _scroll_changed(double);
	void _shape_text(int p_idx);
	void _mouse_exited();

	String _atr(int p_idx, const String &p_text) const;

protected:
	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const { return property_helper.property_get_value(p_name, r_ret); }
	void _get_property_list(List<PropertyInfo> *p_list) const { property_helper.get_property_list(p_list); }
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }
	static void _bind_methods();

public:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	int add_item(const String &p_item, const Ref<Texture2D> &p_texture = Ref<Texture2D>(), bool p_selectable = true);
	int add_icon_item(const Ref<Texture2D> &p_item, bool p_selectable = true);

	void set_item_text(int p_idx, const String &p_text);
	String get_item_text(int p_idx) const;

	void set_item_text_direction(int p_idx, TextDirection p_text_direction);
	TextDirection get_item_text_direction(int p_idx) const;

	void set_item_language(int p_idx, const String &p_language);
	String get_item_language(int p_idx) const;

	void set_item_auto_translate_mode(int p_idx, AutoTranslateMode p_mode);
	AutoTranslateMode get_item_auto_translate_mode(int p_idx) const;

	void set_item_icon(int p_idx, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_item_icon(int p_idx) const;

	void set_item_icon_transposed(int p_idx, const bool transposed);
	bool is_item_icon_transposed(int p_idx) const;

	void set_item_icon_region(int p_idx, const Rect2 &p_region);
	Rect2 get_item_icon_region(int p_idx) const;

	void set_item_icon_modulate(int p_idx, const Color &p_modulate);
	Color get_item_icon_modulate(int p_idx) const;

	void set_item_selectable(int p_idx, bool p_selectable);
	bool is_item_selectable(int p_idx) const;

	void set_item_disabled(int p_idx, bool p_disabled);
	bool is_item_disabled(int p_idx) const;

	void set_item_metadata(int p_idx, const Variant &p_metadata);
	Variant get_item_metadata(int p_idx) const;

	void set_item_tag_icon(int p_idx, const Ref<Texture2D> &p_tag_icon);

	void set_item_tooltip_enabled(int p_idx, const bool p_enabled);
	bool is_item_tooltip_enabled(int p_idx) const;

	void set_item_tooltip(int p_idx, const String &p_tooltip);
	String get_item_tooltip(int p_idx) const;

	void set_item_custom_bg_color(int p_idx, const Color &p_custom_bg_color);
	Color get_item_custom_bg_color(int p_idx) const;

	void set_item_custom_fg_color(int p_idx, const Color &p_custom_fg_color);
	Color get_item_custom_fg_color(int p_idx) const;

	Rect2 get_item_rect(int p_idx, bool p_expand = true) const;

	void set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior);
	TextServer::OverrunBehavior get_text_overrun_behavior() const;

	void select(int p_idx, bool p_single = true);
	void deselect(int p_idx);
	void deselect_all();
	bool is_selected(int p_idx) const;
	Vector<int> get_selected_items();
	bool is_anything_selected();

	void set_current(int p_current);
	int get_current() const;

	void move_item(int p_from_idx, int p_to_idx);

	void set_item_count(int p_count);
	int get_item_count() const;
	void remove_item(int p_idx);

	void clear();

	void set_fixed_column_width(int p_size);
	int get_fixed_column_width() const;

	void set_same_column_width(bool p_enable);
	bool is_same_column_width() const;

	void set_max_text_lines(int p_lines);
	int get_max_text_lines() const;

	void set_max_columns(int p_amount);
	int get_max_columns() const;

	void set_select_mode(SelectMode p_mode);
	SelectMode get_select_mode() const;

	void set_icon_mode(IconMode p_mode);
	IconMode get_icon_mode() const;

	void set_fixed_icon_size(const Size2i &p_size);
	Size2i get_fixed_icon_size() const;

	void set_fixed_tag_icon_size(const Size2i &p_size);

	void set_allow_rmb_select(bool p_allow);
	bool get_allow_rmb_select() const;

	void set_allow_reselect(bool p_allow);
	bool get_allow_reselect() const;

	void set_allow_search(bool p_allow);
	bool get_allow_search() const;

	void ensure_current_is_visible();

	void sort_items_by_text();
	int find_metadata(const Variant &p_metadata) const;

	virtual String get_tooltip(const Point2 &p_pos) const override;
	int get_item_at_position(const Point2 &p_pos, bool p_exact = false) const;
	bool is_pos_at_end_of_items(const Point2 &p_pos) const;

	void set_icon_scale(real_t p_scale);
	real_t get_icon_scale() const;

	void set_auto_width(bool p_enable);
	bool has_auto_width() const;

	void set_auto_height(bool p_enable);
	bool has_auto_height() const;

	Size2 get_minimum_size() const override;

	void set_autoscroll_to_bottom(const bool p_enable);

	void force_update_list_size();

	VScrollBar *get_v_scroll_bar() { return scroll_bar; }

	ItemList();
	~ItemList();
};

VARIANT_ENUM_CAST(ItemList::SelectMode);
VARIANT_ENUM_CAST(ItemList::IconMode);

#endif // ITEM_LIST_H
