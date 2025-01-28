/**************************************************************************/
/*  line_edit.h                                                           */
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

#ifndef LINE_EDIT_H
#define LINE_EDIT_H

#include "scene/gui/control.h"
#include "scene/gui/popup_menu.h"

class LineEdit : public Control {
	GDCLASS(LineEdit, Control);

public:
	enum MenuItems {
		MENU_CUT,
		MENU_COPY,
		MENU_PASTE,
		MENU_CLEAR,
		MENU_SELECT_ALL,
		MENU_UNDO,
		MENU_REDO,
		MENU_SUBMENU_TEXT_DIR,
		MENU_DIR_INHERITED,
		MENU_DIR_AUTO,
		MENU_DIR_LTR,
		MENU_DIR_RTL,
		MENU_DISPLAY_UCC,
		MENU_SUBMENU_INSERT_UCC,
		MENU_INSERT_LRM,
		MENU_INSERT_RLM,
		MENU_INSERT_LRE,
		MENU_INSERT_RLE,
		MENU_INSERT_LRO,
		MENU_INSERT_RLO,
		MENU_INSERT_PDF,
		MENU_INSERT_ALM,
		MENU_INSERT_LRI,
		MENU_INSERT_RLI,
		MENU_INSERT_FSI,
		MENU_INSERT_PDI,
		MENU_INSERT_ZWJ,
		MENU_INSERT_ZWNJ,
		MENU_INSERT_WJ,
		MENU_INSERT_SHY,
		MENU_EMOJI_AND_SYMBOL,
		MENU_MAX
	};

	enum VirtualKeyboardType {
		KEYBOARD_TYPE_DEFAULT,
		KEYBOARD_TYPE_MULTILINE,
		KEYBOARD_TYPE_NUMBER,
		KEYBOARD_TYPE_NUMBER_DECIMAL,
		KEYBOARD_TYPE_PHONE,
		KEYBOARD_TYPE_EMAIL_ADDRESS,
		KEYBOARD_TYPE_PASSWORD,
		KEYBOARD_TYPE_URL
	};

private:
	HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;

	bool editing = false;
	bool keep_editing_on_text_submit = false;
	bool editable = false;
	bool pass = false;
	bool text_changed_dirty = false;

	bool alt_start = false;
	bool alt_start_no_hold = false;
	uint32_t alt_code = 0;

	String undo_text;
	String text;
	String placeholder;
	String placeholder_translated;
	String secret_character = U"•";
	String ime_text;
	Point2 ime_selection;

	RID text_rid;
	float full_width = 0.0;

	bool selecting_enabled = true;
	bool deselect_on_focus_loss_enabled = true;
	bool drag_and_drop_selection_enabled = true;

	bool context_menu_enabled = true;
	bool emoji_menu_enabled = true;
	PopupMenu *menu = nullptr;
	PopupMenu *menu_dir = nullptr;
	PopupMenu *menu_ctl = nullptr;

	bool caret_mid_grapheme_enabled = false;

	int caret_column = 0;
	float scroll_offset = 0.0;
	int max_length = 0; // 0 for no maximum.

	String language;
	TextDirection text_direction = TEXT_DIRECTION_AUTO;
	TextDirection input_direction = TEXT_DIRECTION_LTR;
	TextServer::StructuredTextParser st_parser = TextServer::STRUCTURED_TEXT_DEFAULT;
	Array st_args;
	bool draw_control_chars = false;

	bool expand_to_text_length = false;
	bool window_has_focus = true;

	bool clear_button_enabled = false;

	bool shortcut_keys_enabled = true;

	bool virtual_keyboard_enabled = true;
	VirtualKeyboardType virtual_keyboard_type = KEYBOARD_TYPE_DEFAULT;

	bool middle_mouse_paste_enabled = true;

	bool drag_action = false;
	bool drag_caret_force_displayed = false;

	Ref<Texture2D> right_icon;
	bool flat = false;

	struct Selection {
		int begin = 0;
		int end = 0;
		int start_column = 0;
		bool enabled = false;
		bool creating = false;
		bool double_click = false;
		bool drag_attempt = false;
	} selection;

	struct TextOperation {
		int caret_column = 0;
		float scroll_offset = 0.0;
		String text;
	};
	List<TextOperation> undo_stack;
	List<TextOperation>::Element *undo_stack_pos = nullptr;

	struct ClearButtonStatus {
		bool press_attempt = false;
		bool pressing_inside = false;
	} clear_button_status;

	uint64_t last_dblclk = 0;
	Vector2 last_dblclk_pos;

	bool caret_blink_enabled = false;
	bool caret_force_displayed = false;
	bool draw_caret = true;
	float caret_blink_interval = 0.65;
	double caret_blink_timer = 0.0;
	bool caret_can_draw = false;

	bool pending_select_all_on_focus = false;
	bool select_all_on_focus = false;

	struct ThemeCache {
		Ref<StyleBox> normal;
		Ref<StyleBox> read_only;
		Ref<StyleBox> focus;

		Ref<Font> font;
		int font_size = 0;
		Color font_color;
		Color font_uneditable_color;
		Color font_selected_color;
		int font_outline_size;
		Color font_outline_color;
		Color font_placeholder_color;
		int caret_width = 0;
		Color caret_color;
		int minimum_character_width = 0;
		Color selection_color;

		Ref<Texture2D> clear_icon;
		Color clear_button_color;
		Color clear_button_color_pressed;

		float base_scale = 1.0;
	} theme_cache;

	void _close_ime_window();
	void _update_ime_window_position();

	void _clear_undo_stack();
	void _clear_redo();
	void _create_undo_state();

	Key _get_menu_action_accelerator(const String &p_action);
	void _generate_context_menu();
	void _update_context_menu();

	void _shape();
	void _fit_to_width();
	void _text_changed();
	void _emit_text_change();

	void shift_selection_check_pre(bool);
	void shift_selection_check_post(bool);

	void selection_fill_at_caret();
	void set_scroll_offset(float p_pos);
	float get_scroll_offset() const;

	void set_caret_at_pixel_pos(int p_x);
	Vector2 get_caret_pixel_pos();

	void _reset_caret_blink_timer();
	void _toggle_draw_caret();
	void _validate_caret_can_draw();

	void clear_internal();

	void _editor_settings_changed();

	void _swap_current_input_direction();
	void _move_caret_left(bool p_select, bool p_move_by_word = false);
	void _move_caret_right(bool p_select, bool p_move_by_word = false);
	void _move_caret_start(bool p_select);
	void _move_caret_end(bool p_select);
	void _backspace(bool p_word = false, bool p_all_to_left = false);
	void _delete(bool p_word = false, bool p_all_to_right = false);
	void _texture_changed();

protected:
	bool _is_over_clear_button(const Point2 &p_pos) const;

	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

	virtual void unhandled_key_input(const Ref<InputEvent> &p_event) override;
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void edit();
	void unedit();
	bool is_editing() const;
	void set_keep_editing_on_text_submit(bool p_enabled);
	bool is_editing_kept_on_text_submit() const;

	bool has_ime_text() const;
	void cancel_ime();
	void apply_ime();

	void set_horizontal_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_horizontal_alignment() const;

	virtual Variant get_drag_data(const Point2 &p_point) override;
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override;

	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;

	void menu_option(int p_option);
	void set_context_menu_enabled(bool p_enable);
	bool is_context_menu_enabled();
	PopupMenu *get_menu() const;
	bool is_menu_visible() const;

	void show_emoji_and_symbol_picker();

	void set_emoji_menu_enabled(bool p_enabled);
	bool is_emoji_menu_enabled() const;

	void select(int p_from = 0, int p_to = -1);
	void select_all();
	void selection_delete();
	void deselect();
	bool has_selection() const;
	String get_selected_text();
	int get_selection_from_column() const;
	int get_selection_to_column() const;

	void delete_char();
	void delete_text(int p_from_column, int p_to_column);

	void set_text(String p_text);
	String get_text() const;
	void set_text_with_selection(const String &p_text); // Set text, while preserving selection.

	void set_text_direction(TextDirection p_text_direction);
	TextDirection get_text_direction() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_draw_control_chars(bool p_draw_control_chars);
	bool get_draw_control_chars() const;

	void set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser);
	TextServer::StructuredTextParser get_structured_text_bidi_override() const;

	void set_structured_text_bidi_override_options(Array p_args);
	Array get_structured_text_bidi_override_options() const;

	void set_placeholder(String p_text);
	String get_placeholder() const;

	void set_caret_column(int p_column);
	int get_caret_column() const;

	void set_max_length(int p_max_length);
	int get_max_length() const;

	void insert_text_at_caret(String p_text);
	void clear();

	void set_caret_mid_grapheme_enabled(const bool p_enabled);
	bool is_caret_mid_grapheme_enabled() const;

	bool is_caret_blink_enabled() const;
	void set_caret_blink_enabled(const bool p_enabled);

	float get_caret_blink_interval() const;
	void set_caret_blink_interval(const float p_interval);

	void set_caret_force_displayed(const bool p_enabled);
	bool is_caret_force_displayed() const;

	void copy_text();
	void cut_text();
	void paste_text();
	bool has_undo() const;
	bool has_redo() const;
	void undo();
	void redo();

	void set_editable(bool p_editable);
	bool is_editable() const;

	void set_secret(bool p_secret);
	bool is_secret() const;

	void set_secret_character(const String &p_string);
	String get_secret_character() const;

	virtual Size2 get_minimum_size() const override;

	void set_expand_to_text_length_enabled(bool p_enabled);
	bool is_expand_to_text_length_enabled() const;

	void set_clear_button_enabled(bool p_enabled);
	bool is_clear_button_enabled() const;

	void set_shortcut_keys_enabled(bool p_enabled);
	bool is_shortcut_keys_enabled() const;

	void set_virtual_keyboard_enabled(bool p_enable);
	bool is_virtual_keyboard_enabled() const;

	void set_virtual_keyboard_type(VirtualKeyboardType p_type);
	VirtualKeyboardType get_virtual_keyboard_type() const;

	void set_middle_mouse_paste_enabled(bool p_enabled);
	bool is_middle_mouse_paste_enabled() const;

	void set_selecting_enabled(bool p_enabled);
	bool is_selecting_enabled() const;

	void set_deselect_on_focus_loss_enabled(const bool p_enabled);
	bool is_deselect_on_focus_loss_enabled() const;

	void set_drag_and_drop_selection_enabled(const bool p_enabled);
	bool is_drag_and_drop_selection_enabled() const;

	void set_right_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_right_icon();

	void set_flat(bool p_enabled);
	bool is_flat() const;

	void set_select_all_on_focus(bool p_enabled);
	bool is_select_all_on_focus() const;
	void clear_pending_select_all_on_focus(); // For other controls, e.g. SpinBox.

	virtual bool is_text_field() const override;

	PackedStringArray get_configuration_warnings() const override;

	void show_virtual_keyboard();

	LineEdit(const String &p_placeholder = String());
	~LineEdit();
};

VARIANT_ENUM_CAST(LineEdit::MenuItems);
VARIANT_ENUM_CAST(LineEdit::VirtualKeyboardType);

#endif // LINE_EDIT_H
