/**************************************************************************/
/*  tree.h                                                                */
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

#ifndef TREE_H
#define TREE_H

#include "scene/gui/control.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/slider.h"
#include "scene/resources/text_paragraph.h"

class TextEdit;
class Tree;

class TreeItem : public Object {
	GDCLASS(TreeItem, Object);

public:
	enum TreeCellMode {
		CELL_MODE_STRING, ///< just a string
		CELL_MODE_CHECK, ///< string + check
		CELL_MODE_RANGE, ///< Contains a range
		CELL_MODE_ICON, ///< Contains an icon, not editable
		CELL_MODE_CUSTOM, ///< Contains a custom value, show a string, and an edit button
	};

private:
	friend class Tree;

	struct Cell {
		TreeCellMode mode = TreeItem::CELL_MODE_STRING;

		Ref<Texture2D> icon;
		Ref<Texture2D> icon_overlay;
		Rect2i icon_region;
		String text;
		String xl_text;
		Node::AutoTranslateMode auto_translate_mode = Node::AUTO_TRANSLATE_MODE_INHERIT;
		bool edit_multiline = false;
		String suffix;
		Ref<TextParagraph> text_buf;
		String language;
		TextServer::StructuredTextParser st_parser = TextServer::STRUCTURED_TEXT_DEFAULT;
		Array st_args;
		Control::TextDirection text_direction = Control::TEXT_DIRECTION_INHERITED;
		TextServer::AutowrapMode autowrap_mode = TextServer::AUTOWRAP_OFF;
		bool dirty = true;
		double min = 0.0;
		double max = 100.0;
		double step = 1.0;
		double val = 0.0;
		int icon_max_w = 0;
		bool expr = false;
		bool checked = false;
		bool indeterminate = false;
		bool editable = false;
		bool selected = false;
		bool selectable = true;
		bool custom_color = false;
		Color color;
		bool custom_bg_color = false;
		bool custom_bg_outline = false;
		Color bg_color;
		bool custom_button = false;
		bool expand_right = false;
		Color icon_color = Color(1, 1, 1);

		Size2i cached_minimum_size;
		bool cached_minimum_size_dirty = true;

		HorizontalAlignment text_alignment = HORIZONTAL_ALIGNMENT_LEFT;

		Variant meta;
		String tooltip;

		Callable custom_draw_callback;

		struct Button {
			int id = 0;
			bool disabled = false;
			Ref<Texture2D> texture;
			Color color = Color(1, 1, 1, 1);
			String tooltip;
		};

		Vector<Button> buttons;

		Ref<Font> custom_font;
		int custom_font_size = -1;

		Cell() {
			text_buf.instantiate();
			text_buf->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		}

		Size2 get_icon_size() const;
		void draw_icon(const RID &p_where, const Point2 &p_pos, const Size2 &p_size = Size2(), const Color &p_color = Color()) const;
	};

	Vector<Cell> cells;

	bool collapsed = false; // won't show children
	bool visible = true;
	bool parent_visible_in_tree = true;
	bool disable_folding = false;
	int custom_min_height = 0;

	TreeItem *parent = nullptr; // parent item
	TreeItem *prev = nullptr; // previous in list
	TreeItem *next = nullptr; // next in list
	TreeItem *first_child = nullptr;
	TreeItem *last_child = nullptr;

	Vector<TreeItem *> children_cache;
	bool is_root = false; // for tree root
	Tree *tree = nullptr; // tree (for reference)

	TreeItem(Tree *p_tree);

	void _changed_notify(int p_cell);
	void _changed_notify();
	void _cell_selected(int p_cell);
	void _cell_deselected(int p_cell);
	void _handle_visibility_changed(bool p_visible);
	void _propagate_visibility_changed(bool p_parent_visible_in_tree);

	void _change_tree(Tree *p_tree);

	_FORCE_INLINE_ void _create_children_cache() {
		if (children_cache.is_empty()) {
			TreeItem *c = first_child;
			while (c) {
				children_cache.append(c);
				c = c->next;
			}
		}
	}

	_FORCE_INLINE_ void _unlink_from_tree() {
		TreeItem *p = get_prev();
		if (p) {
			p->next = next;
		}
		if (next) {
			next->prev = p;
		}
		if (parent) {
			if (!parent->children_cache.is_empty()) {
				parent->children_cache.remove_at(get_index());
			}
			if (parent->first_child == this) {
				parent->first_child = next;
			}
			if (parent->last_child == this) {
				parent->last_child = prev;
			}
		}
	}

	bool _is_any_collapsed(bool p_only_visible);

protected:
	static void _bind_methods();

	// Bind helpers
	Dictionary _get_range_config(int p_column) {
		Dictionary d;
		double min = 0.0, max = 0.0, step = 0.0;
		get_range_config(p_column, min, max, step);
		d["min"] = min;
		d["max"] = max;
		d["step"] = step;
		d["expr"] = false;

		return d;
	}

	void _call_recursive_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

public:
	/* cell mode */
	void set_cell_mode(int p_column, TreeCellMode p_mode);
	TreeCellMode get_cell_mode(int p_column) const;

	/* auto translate mode */
	void set_auto_translate_mode(int p_column, Node::AutoTranslateMode p_mode);
	Node::AutoTranslateMode get_auto_translate_mode(int p_column) const;

	/* multiline editable */
	void set_edit_multiline(int p_column, bool p_multiline);
	bool is_edit_multiline(int p_column) const;

	/* check mode */
	void set_checked(int p_column, bool p_checked);
	void set_indeterminate(int p_column, bool p_indeterminate);
	bool is_checked(int p_column) const;
	bool is_indeterminate(int p_column) const;

	void propagate_check(int p_column, bool p_emit_signal = true);

	String atr(int p_column, const String &p_text) const;

private:
	// Check helpers.
	void _propagate_check_through_children(int p_column, bool p_checked, bool p_emit_signal);
	void _propagate_check_through_parents(int p_column, bool p_emit_signal);

	TreeItem *_get_prev_in_tree(bool p_wrap = false, bool p_include_invisible = false);
	TreeItem *_get_next_in_tree(bool p_wrap = false, bool p_include_invisible = false);

public:
	void set_text(int p_column, String p_text);
	String get_text(int p_column) const;

	void set_text_direction(int p_column, Control::TextDirection p_text_direction);
	Control::TextDirection get_text_direction(int p_column) const;

	void set_autowrap_mode(int p_column, TextServer::AutowrapMode p_mode);
	TextServer::AutowrapMode get_autowrap_mode(int p_column) const;

	void set_text_overrun_behavior(int p_column, TextServer::OverrunBehavior p_behavior);
	TextServer::OverrunBehavior get_text_overrun_behavior(int p_column) const;

	void set_structured_text_bidi_override(int p_column, TextServer::StructuredTextParser p_parser);
	TextServer::StructuredTextParser get_structured_text_bidi_override(int p_column) const;

	void set_structured_text_bidi_override_options(int p_column, Array p_args);
	Array get_structured_text_bidi_override_options(int p_column) const;

	void set_language(int p_column, const String &p_language);
	String get_language(int p_column) const;

	void set_suffix(int p_column, String p_suffix);
	String get_suffix(int p_column) const;

	void set_icon(int p_column, const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_icon(int p_column) const;

	void set_icon_overlay(int p_column, const Ref<Texture2D> &p_icon_overlay);
	Ref<Texture2D> get_icon_overlay(int p_column) const;

	void set_icon_region(int p_column, const Rect2 &p_icon_region);
	Rect2 get_icon_region(int p_column) const;

	void set_icon_modulate(int p_column, const Color &p_modulate);
	Color get_icon_modulate(int p_column) const;

	void set_icon_max_width(int p_column, int p_max);
	int get_icon_max_width(int p_column) const;

	void clear_buttons();
	void add_button(int p_column, const Ref<Texture2D> &p_button, int p_id = -1, bool p_disabled = false, const String &p_tooltip = "");
	int get_button_count(int p_column) const;
	String get_button_tooltip_text(int p_column, int p_index) const;
	Ref<Texture2D> get_button(int p_column, int p_index) const;
	int get_button_id(int p_column, int p_index) const;
	void erase_button(int p_column, int p_index);
	int get_button_by_id(int p_column, int p_id) const;
	Color get_button_color(int p_column, int p_index) const;
	void set_button_tooltip_text(int p_column, int p_index, const String &p_tooltip);
	void set_button(int p_column, int p_index, const Ref<Texture2D> &p_button);
	void set_button_color(int p_column, int p_index, const Color &p_color);
	void set_button_disabled(int p_column, int p_index, bool p_disabled);
	bool is_button_disabled(int p_column, int p_index) const;

	/* range works for mode number or mode combo */

	void set_range(int p_column, double p_value);
	double get_range(int p_column) const;

	void set_range_config(int p_column, double p_min, double p_max, double p_step, bool p_exp = false);
	void get_range_config(int p_column, double &r_min, double &r_max, double &r_step) const;
	bool is_range_exponential(int p_column) const;

	void set_metadata(int p_column, const Variant &p_meta);
	Variant get_metadata(int p_column) const;

#ifndef DISABLE_DEPRECATED
	void set_custom_draw(int p_column, Object *p_object, const StringName &p_callback);
#endif // DISABLE_DEPRECATED
	void set_custom_draw_callback(int p_column, const Callable &p_callback);
	Callable get_custom_draw_callback(int p_column) const;

	void set_collapsed(bool p_collapsed);
	bool is_collapsed();

	void set_collapsed_recursive(bool p_collapsed);
	bool is_any_collapsed(bool p_only_visible = false);

	void set_visible(bool p_visible);
	bool is_visible();
	bool is_visible_in_tree() const;

	void uncollapse_tree();

	void set_custom_minimum_height(int p_height);
	int get_custom_minimum_height() const;

	void set_selectable(int p_column, bool p_selectable);
	bool is_selectable(int p_column) const;

	bool is_selected(int p_column);
	void select(int p_column);
	void deselect(int p_column);
	void set_as_cursor(int p_column);

	void set_editable(int p_column, bool p_editable);
	bool is_editable(int p_column);

	void set_custom_color(int p_column, const Color &p_color);
	Color get_custom_color(int p_column) const;
	void clear_custom_color(int p_column);

	void set_custom_font(int p_column, const Ref<Font> &p_font);
	Ref<Font> get_custom_font(int p_column) const;

	void set_custom_font_size(int p_column, int p_font_size);
	int get_custom_font_size(int p_column) const;

	void set_custom_bg_color(int p_column, const Color &p_color, bool p_bg_outline = false);
	void clear_custom_bg_color(int p_column);
	Color get_custom_bg_color(int p_column) const;

	void set_custom_as_button(int p_column, bool p_button);
	bool is_custom_set_as_button(int p_column) const;

	void set_tooltip_text(int p_column, const String &p_tooltip);
	String get_tooltip_text(int p_column) const;

	void set_text_alignment(int p_column, HorizontalAlignment p_alignment);
	HorizontalAlignment get_text_alignment(int p_column) const;

	void set_expand_right(int p_column, bool p_enable);
	bool get_expand_right(int p_column) const;

	void set_disable_folding(bool p_disable);
	bool is_folding_disabled() const;

	Size2 get_minimum_size(int p_column);

	/* Item manipulation */

	TreeItem *create_child(int p_index = -1);
	void add_child(TreeItem *p_item);
	void remove_child(TreeItem *p_item);

	Tree *get_tree() const;

	TreeItem *get_prev();
	TreeItem *get_next() const;
	TreeItem *get_parent() const;
	TreeItem *get_first_child() const;

	TreeItem *get_prev_in_tree(bool p_wrap = false);
	TreeItem *get_next_in_tree(bool p_wrap = false);

	TreeItem *get_prev_visible(bool p_wrap = false);
	TreeItem *get_next_visible(bool p_wrap = false);

	TreeItem *get_child(int p_index);
	int get_visible_child_count();
	int get_child_count();
	TypedArray<TreeItem> get_children();
	void clear_children();
	int get_index();

#ifdef DEV_ENABLED
	// This debugging code can be removed once the current refactoring of this class is complete.
	void validate_cache() const;
#else
	void validate_cache() const {}
#endif

	void move_before(TreeItem *p_item);
	void move_after(TreeItem *p_item);

	void call_recursive(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	~TreeItem();
};

VARIANT_ENUM_CAST(TreeItem::TreeCellMode);

class VBoxContainer;

class Tree : public Control {
	GDCLASS(Tree, Control);

public:
	enum SelectMode {
		SELECT_SINGLE,
		SELECT_ROW,
		SELECT_MULTI
	};

	enum DropModeFlags {
		DROP_MODE_DISABLED = 0,
		DROP_MODE_ON_ITEM = 1,
		DROP_MODE_INBETWEEN = 2
	};

private:
	friend class TreeItem;

	TreeItem *root = nullptr;
	TreeItem *popup_edited_item = nullptr;
	TreeItem *selected_item = nullptr;
	TreeItem *edited_item = nullptr;

	TreeItem *popup_pressing_edited_item = nullptr; // Candidate.
	int popup_pressing_edited_item_column = -1;

	TreeItem *drop_mode_over = nullptr;
	int drop_mode_section = 0;

	TreeItem *single_select_defer = nullptr;
	int single_select_defer_column = 0;

	int pressed_button = -1;
	bool pressing_for_editor = false;
	String pressing_for_editor_text;
	Vector2 pressing_pos;
	Rect2 pressing_item_rect;

	Vector2 hovered_pos;
	bool is_mouse_hovering = false;

	float range_drag_base = 0.0;
	bool range_drag_enabled = false;
	Vector2 range_drag_capture_pos;

	bool propagate_mouse_activated = false;

	//TreeItem *cursor_item;
	//int cursor_column;

	Rect2 custom_popup_rect;
	int edited_col = -1;
	int selected_col = -1;
	int popup_edited_item_col = -1;
	bool hide_root = false;
	SelectMode select_mode = SELECT_SINGLE;

	int blocked = 0;

	int drop_mode_flags = 0;

	struct ColumnInfo {
		int custom_min_width = 0;
		int expand_ratio = 1;
		bool expand = true;
		bool clip_content = false;
		String title;
		String xl_title;
		HorizontalAlignment title_alignment = HORIZONTAL_ALIGNMENT_CENTER;
		Ref<TextParagraph> text_buf;
		String language;
		Control::TextDirection text_direction = Control::TEXT_DIRECTION_INHERITED;

		mutable int cached_minimum_width = 0;
		mutable bool cached_minimum_width_dirty = true;

		ColumnInfo() {
			text_buf.instantiate();
		}
	};

	bool show_column_titles = false;

	VBoxContainer *popup_editor_vb = nullptr;

	bool popup_edit_committed = true;
	Popup *popup_editor = nullptr;
	LineEdit *line_editor = nullptr;
	TextEdit *text_editor = nullptr;
	HSlider *value_editor = nullptr;
	bool updating_value_editor = false;
	uint64_t focus_in_id = 0;
	PopupMenu *popup_menu = nullptr;

	Vector<ColumnInfo> columns;

	Timer *range_click_timer = nullptr;
	TreeItem *range_item_last = nullptr;
	bool range_up_last = false;
	void _range_click_timeout();

	int compute_item_height(TreeItem *p_item) const;
	int get_item_height(TreeItem *p_item) const;
	void _update_all();
	void update_column(int p_col);
	void update_item_cell(TreeItem *p_item, int p_col) const;
	void update_item_cache(TreeItem *p_item) const;
	//void draw_item_text(String p_text,const Ref<Texture2D>& p_icon,int p_icon_max_w,bool p_tool,Rect2i p_rect,const Color& p_color);
	void draw_item_rect(TreeItem::Cell &p_cell, const Rect2i &p_rect, const Color &p_color, const Color &p_icon_color, int p_ol_size, const Color &p_ol_color);
	int draw_item(const Point2i &p_pos, const Point2 &p_draw_ofs, const Size2 &p_draw_size, TreeItem *p_item, int &r_self_height);
	void select_single_item(TreeItem *p_selected, TreeItem *p_current, int p_col, TreeItem *p_prev = nullptr, bool *r_in_range = nullptr, bool p_force_deselect = false);
	int propagate_mouse_event(const Point2i &p_pos, int x_ofs, int y_ofs, int x_limit, bool p_double_click, TreeItem *p_item, MouseButton p_button, const Ref<InputEventWithModifiers> &p_mod);
	void _line_editor_submit(String p_text);
	void _apply_multiline_edit();
	void _text_editor_popup_modal_close();
	void _text_editor_gui_input(const Ref<InputEvent> &p_event);
	void value_editor_changed(double p_value);

	void popup_select(int p_option);

	void item_edited(int p_column, TreeItem *p_item, MouseButton p_custom_mouse_index = MouseButton::NONE);
	void item_changed(int p_column, TreeItem *p_item);
	void item_selected(int p_column, TreeItem *p_item);
	void item_deselected(int p_column, TreeItem *p_item);

	void propagate_set_columns(TreeItem *p_item);

	struct ThemeCache {
		Ref<StyleBox> panel_style;
		Ref<StyleBox> focus_style;

		Ref<Font> font;
		Ref<Font> tb_font;
		int font_size = 0;
		int tb_font_size = 0;

		Ref<StyleBox> hovered;
		Ref<StyleBox> hovered_dimmed;
		Ref<StyleBox> selected;
		Ref<StyleBox> selected_focus;
		Ref<StyleBox> cursor;
		Ref<StyleBox> cursor_unfocus;
		Ref<StyleBox> button_hover;
		Ref<StyleBox> button_pressed;
		Ref<StyleBox> title_button;
		Ref<StyleBox> title_button_hover;
		Ref<StyleBox> title_button_pressed;
		Ref<StyleBox> custom_button;
		Ref<StyleBox> custom_button_hover;
		Ref<StyleBox> custom_button_pressed;

		Color title_button_color;

		Ref<Texture2D> checked;
		Ref<Texture2D> unchecked;
		Ref<Texture2D> checked_disabled;
		Ref<Texture2D> unchecked_disabled;
		Ref<Texture2D> indeterminate;
		Ref<Texture2D> indeterminate_disabled;
		Ref<Texture2D> arrow;
		Ref<Texture2D> arrow_collapsed;
		Ref<Texture2D> arrow_collapsed_mirrored;
		Ref<Texture2D> select_arrow;
		Ref<Texture2D> updown;

		Color font_color;
		Color font_hovered_color;
		Color font_hovered_dimmed_color;
		Color font_selected_color;
		Color font_disabled_color;
		Color guide_color;
		Color drop_position_color;
		Color relationship_line_color;
		Color parent_hl_line_color;
		Color children_hl_line_color;
		Color custom_button_font_highlight;
		Color font_outline_color;

		float base_scale = 1.0;
		int font_outline_size = 0;

		int h_separation = 0;
		int v_separation = 0;
		int inner_item_margin_bottom = 0;
		int inner_item_margin_left = 0;
		int inner_item_margin_right = 0;
		int inner_item_margin_top = 0;
		int item_margin = 0;
		int button_margin = 0;
		int icon_max_width = 0;
		Point2 offset;

		int draw_relationship_lines = 0;
		int relationship_line_width = 0;
		int parent_hl_line_width = 0;
		int children_hl_line_width = 0;
		int parent_hl_line_margin = 0;
		int draw_guides = 0;

		int scroll_border = 0;
		int scroll_speed = 0;

		int scrollbar_margin_top = -1;
		int scrollbar_margin_right = -1;
		int scrollbar_margin_bottom = -1;
		int scrollbar_margin_left = -1;
		int scrollbar_h_separation = 0;
		int scrollbar_v_separation = 0;
	} theme_cache;

	struct Cache {
		enum ClickType {
			CLICK_NONE,
			CLICK_TITLE,
			CLICK_BUTTON,

		};

		ClickType click_type = Cache::CLICK_NONE;
		int click_index = -1;
		int click_id = -1;
		TreeItem *click_item = nullptr;
		int click_column = 0;
		int hover_header_column = -1;
		bool hover_header_row = false;
		Point2 click_pos;

		TreeItem *hover_item = nullptr;
		int hover_column = -1;
		int hover_button_index_in_column = -1;

		bool rtl = false;
	} cache;

	int _get_title_button_height() const;
	Size2 _get_cell_icon_size(const TreeItem::Cell &p_cell) const;

	void _scroll_moved(float p_value);
	HScrollBar *h_scroll = nullptr;
	VScrollBar *v_scroll = nullptr;

	bool h_scroll_enabled = true;
	bool v_scroll_enabled = true;

	Size2 get_internal_min_size() const;
	void update_scrollbars();

	Rect2 search_item_rect(TreeItem *p_from, TreeItem *p_item);
	//Rect2 get_item_rect(TreeItem *p_item);
	uint64_t last_keypress = 0;
	String incr_search;
	bool cursor_can_exit_tree = true;
	void _do_incr_search(const String &p_add);

	TreeItem *_search_item_text(TreeItem *p_at, const String &p_find, int *r_col, bool p_selectable, bool p_backwards = false);

	TreeItem *_find_item_at_pos(TreeItem *p_item, const Point2 &p_pos, int &r_column, int &h, int &section) const;
	int _get_item_h_offset(TreeItem *p_item) const;

	void _find_button_at_pos(const Point2 &p_pos, TreeItem *&r_item, int &r_column, int &r_index) const;

	/*	float drag_speed;
	float drag_accum;

	float last_drag_accum;
	float last_drag_time;
	float time_since_motion;*/

	float drag_speed = 0.0;
	float drag_from = 0.0;
	float drag_accum = 0.0;
	Vector2 last_speed;
	bool drag_touching = false;
	bool drag_touching_deaccel = false;
	bool click_handled = false;
	bool allow_rmb_select = false;
	bool scrolling = false;

	bool allow_reselect = false;
	bool allow_search = true;

	bool force_edit_checkbox_only_on_checkbox = false;

	bool hide_folding = false;

	bool enable_recursive_folding = true;

	bool enable_auto_tooltip = true;

	void _determine_hovered_item();

	int _count_selected_items(TreeItem *p_from) const;
	bool _is_branch_selected(TreeItem *p_from) const;
	bool _is_sibling_branch_selected(TreeItem *p_from) const;
	void _go_left();
	void _go_right();
	void _go_down();
	void _go_up();

	bool _scroll(bool p_horizontal, float p_pages);

	Rect2 _get_scrollbar_layout_rect() const;
	Rect2 _get_content_rect() const; // Considering the background stylebox and scrollbars.
	Rect2 _get_item_focus_rect(const TreeItem *p_item) const;

protected:
	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	virtual String get_tooltip(const Point2 &p_pos) const override;

	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual Variant get_drag_data(const Point2 &p_point) override;
	TreeItem *get_item_at_position(const Point2 &p_pos) const;
	int get_column_at_position(const Point2 &p_pos) const;
	int get_drop_section_at_position(const Point2 &p_pos) const;
	int get_button_id_at_position(const Point2 &p_pos) const;

	void clear();

	TreeItem *create_item(TreeItem *p_parent = nullptr, int p_index = -1);
	TreeItem *get_root() const;
	TreeItem *get_last_item() const;

	void set_column_custom_minimum_width(int p_column, int p_min_width);
	void set_column_expand(int p_column, bool p_expand);
	void set_column_expand_ratio(int p_column, int p_ratio);
	void set_column_clip_content(int p_column, bool p_fit);
	int get_column_minimum_width(int p_column) const;
	int get_column_width(int p_column) const;
	int get_column_expand_ratio(int p_column) const;

	bool is_column_expanding(int p_column) const;
	bool is_column_clipping_content(int p_column) const;

	void set_hide_root(bool p_enabled);
	bool is_root_hidden() const;
	TreeItem *get_next_selected(TreeItem *p_item);
	TreeItem *get_selected() const;
	void set_selected(TreeItem *p_item, int p_column = 0);
	int get_selected_column() const;
	int get_pressed_button() const;
	void set_select_mode(SelectMode p_mode);
	SelectMode get_select_mode() const;
	void deselect_all();
	bool is_anything_selected();

	void set_columns(int p_columns);
	int get_columns() const;

	void set_column_title(int p_column, const String &p_title);
	String get_column_title(int p_column) const;

	void set_column_title_alignment(int p_column, HorizontalAlignment p_alignment);
	HorizontalAlignment get_column_title_alignment(int p_column) const;

	void set_column_title_direction(int p_column, Control::TextDirection p_text_direction);
	Control::TextDirection get_column_title_direction(int p_column) const;

	void set_column_title_language(int p_column, const String &p_language);
	String get_column_title_language(int p_column) const;

	void set_column_titles_visible(bool p_show);
	bool are_column_titles_visible() const;

	TreeItem *get_edited() const;
	int get_edited_column() const;

	void ensure_cursor_is_visible();

	Rect2 get_custom_popup_rect() const;

	int get_item_offset(TreeItem *p_item) const;
	Rect2 get_item_rect(TreeItem *p_item, int p_column = -1, int p_button = -1) const;
	bool edit_selected(bool p_force_edit = false);
	bool is_editing();
	void set_editor_selection(int p_from_line, int p_to_line, int p_from_column = -1, int p_to_column = -1, int p_caret = 0);

	// First item that starts with the text, from the current focused item down and wraps around.
	TreeItem *search_item_text(const String &p_find, int *r_col = nullptr, bool p_selectable = false);
	// First item that matches the whole text, from the first item down.
	TreeItem *get_item_with_text(const String &p_find) const;
	TreeItem *get_item_with_metadata(const Variant &p_find, int p_column = -1) const;

	Point2 get_scroll() const;
	void scroll_to_item(TreeItem *p_item, bool p_center_on_item = false);
	void set_h_scroll_enabled(bool p_enable);
	bool is_h_scroll_enabled() const;
	void set_v_scroll_enabled(bool p_enable);
	bool is_v_scroll_enabled() const;

	void set_cursor_can_exit_tree(bool p_enable);

	VScrollBar *get_vscroll_bar() { return v_scroll; }

	void set_hide_folding(bool p_hide);
	bool is_folding_hidden() const;

	void set_enable_recursive_folding(bool p_enable);
	bool is_recursive_folding_enabled() const;

	void set_drop_mode_flags(int p_flags);
	int get_drop_mode_flags() const;

	void set_edit_checkbox_cell_only_when_checkbox_is_pressed(bool p_enable);
	bool get_edit_checkbox_cell_only_when_checkbox_is_pressed() const;

	void set_allow_rmb_select(bool p_allow);
	bool get_allow_rmb_select() const;

	void set_allow_reselect(bool p_allow);
	bool get_allow_reselect() const;

	void set_allow_search(bool p_allow);
	bool get_allow_search() const;

	void set_auto_tooltip(bool p_enable);
	bool is_auto_tooltip_enabled() const;

	Size2 get_minimum_size() const override;

	Tree();
	~Tree();
};

VARIANT_ENUM_CAST(Tree::SelectMode);
VARIANT_ENUM_CAST(Tree::DropModeFlags);

#endif // TREE_H
