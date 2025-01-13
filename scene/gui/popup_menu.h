/**************************************************************************/
/*  popup_menu.h                                                          */
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

#ifndef POPUP_MENU_H
#define POPUP_MENU_H

#include "core/input/shortcut.h"
#include "scene/gui/popup.h"
#include "scene/gui/scroll_container.h"
#include "scene/property_list_helper.h"
#include "scene/resources/text_line.h"

class PopupMenu : public Popup {
	GDCLASS(PopupMenu, Popup);

	static HashMap<NativeMenu::SystemMenus, PopupMenu *> system_menus;

	struct Item {
		Ref<Texture2D> icon;
		int icon_max_width = 0;
		Color icon_modulate = Color(1, 1, 1, 1);
		String text;
		String xl_text;
		Ref<TextLine> text_buf;
		Ref<TextLine> accel_text_buf;

		String language;
		Control::TextDirection text_direction = Control::TEXT_DIRECTION_AUTO;

		bool checked = false;
		enum {
			CHECKABLE_TYPE_NONE,
			CHECKABLE_TYPE_CHECK_BOX,
			CHECKABLE_TYPE_RADIO_BUTTON,
		} checkable_type = CHECKABLE_TYPE_NONE;
		int max_states = 0;
		int state = 0;
		bool separator = false;
		bool disabled = false;
		bool dirty = true;
		int id = 0;
		Variant metadata;
		String submenu_name; // Compatibility.
		PopupMenu *submenu = nullptr;
		String tooltip;
		Key accel = Key::NONE;
		int _ofs_cache = 0;
		int _height_cache = 0;
		int indent = 0;
		Ref<Shortcut> shortcut;
		bool shortcut_is_global = false;
		bool shortcut_is_disabled = false;
		bool allow_echo = false;
		bool submenu_bound = false;

		// Returns (0,0) if icon is null.
		Size2 get_icon_size() const {
			return icon.is_null() ? Size2() : icon->get_size();
		}

		Item() {
			text_buf.instantiate();
			accel_text_buf.instantiate();
			checkable_type = CHECKABLE_TYPE_NONE;
		}

		Item(bool p_dummy) {}
	};

	static inline PropertyListHelper base_property_helper;
	PropertyListHelper property_helper;

	// To make Item available.
	friend class OptionButton;
	friend class MenuButton;

	RID global_menu;
	RID system_menu;
	NativeMenu::SystemMenus system_menu_id = NativeMenu::INVALID_MENU_ID;
	bool prefer_native = false;

	bool close_allowed = false;
	bool activated_by_keyboard = false;

	Timer *minimum_lifetime_timer = nullptr;
	Timer *submenu_timer = nullptr;
	List<Rect2> autohide_areas;
	mutable Vector<Item> items;
	BitField<MouseButtonMask> initial_button_mask;
	bool during_grabbed_click = false;
	bool is_scrolling = false;
	int mouse_over = -1;
	int submenu_over = -1;
	String _get_accel_text(const Item &p_item) const;
	int _get_mouse_over(const Point2 &p_over) const;
	void _mouse_over_update(const Point2 &p_over);
	virtual Size2 _get_contents_minimum_size() const override;

	int _get_item_height(int p_idx) const;
	int _get_items_total_height() const;
	Size2 _get_item_icon_size(int p_idx) const;

	void _shape_item(int p_idx) const;

	void _activate_submenu(int p_over, bool p_by_keyboard = false);
	void _submenu_timeout();

	uint64_t popup_time_msec = 0;
	bool hide_on_item_selection = true;
	bool hide_on_checkable_item_selection = true;
	bool hide_on_multistate_item_selection = false;
	Vector2 moved;

	HashMap<Ref<Shortcut>, int> shortcut_refcount;

	void _ref_shortcut(Ref<Shortcut> p_sc);
	void _unref_shortcut(Ref<Shortcut> p_sc);

	void _shortcut_changed();

	bool allow_search = true;
	uint64_t search_time_msec = 0;
	String search_string = "";

	ScrollContainer *scroll_container = nullptr;
	Control *control = nullptr;

	const float DEFAULT_GAMEPAD_EVENT_DELAY_MS = 0.5;
	const float GAMEPAD_EVENT_REPEAT_RATE_MS = 1.0 / 20;
	float gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;

	struct ThemeCache {
		Ref<StyleBox> panel_style;
		Ref<StyleBox> hover_style;

		Ref<StyleBox> separator_style;
		Ref<StyleBox> labeled_separator_left;
		Ref<StyleBox> labeled_separator_right;

		int v_separation = 0;
		int h_separation = 0;
		int indent = 0;
		int item_start_padding = 0;
		int item_end_padding = 0;
		int icon_max_width = 0;

		Ref<Texture2D> checked;
		Ref<Texture2D> checked_disabled;
		Ref<Texture2D> unchecked;
		Ref<Texture2D> unchecked_disabled;
		Ref<Texture2D> radio_checked;
		Ref<Texture2D> radio_checked_disabled;
		Ref<Texture2D> radio_unchecked;
		Ref<Texture2D> radio_unchecked_disabled;

		Ref<Texture2D> submenu;
		Ref<Texture2D> submenu_mirrored;

		Ref<Font> font;
		int font_size = 0;
		Ref<Font> font_separator;
		int font_separator_size = 0;

		Color font_color;
		Color font_hover_color;
		Color font_disabled_color;
		Color font_accelerator_color;
		int font_outline_size = 0;
		Color font_outline_color;

		Color font_separator_color;
		int font_separator_outline_size = 0;
		Color font_separator_outline_color;
	} theme_cache;

	void _draw_items();

	void _minimum_lifetime_timeout();
	void _close_pressed();
	void _menu_changed();
	void _input_from_window_internal(const Ref<InputEvent> &p_event);
	bool _set_item_accelerator(int p_index, const Ref<InputEventKey> &p_ie);
	void _set_item_checkable_type(int p_index, int p_checkable_type);
	int _get_item_checkable_type(int p_index) const;

protected:
	virtual void add_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;
	virtual void _input_from_window(const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const { return property_helper.property_get_value(p_name, r_ret); }
	void _get_property_list(List<PropertyInfo> *p_list) const { property_helper.get_property_list(p_list); }
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _add_shortcut_bind_compat_36493(const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);
	void _add_icon_shortcut_bind_compat_36493(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);
	void _clear_bind_compat_79965();

	void _set_system_menu_root_compat_87452(const String &p_special);
	String _get_system_menu_root_compat_87452() const;

	static void _bind_compatibility_methods();
#endif

public:
	// ATTENTION: This is used by the POT generator's scene parser. If the number of properties returned by `_get_items()` ever changes,
	// this value should be updated to reflect the new size.
	static const int ITEM_PROPERTY_SIZE = 10;

	virtual void _parent_focused() override;

	RID bind_global_menu();
	void unbind_global_menu();
	bool is_system_menu() const;

	void set_system_menu(NativeMenu::SystemMenus p_system_menu_id);
	NativeMenu::SystemMenus get_system_menu() const;

	void add_item(const String &p_label, int p_id = -1, Key p_accel = Key::NONE);
	void add_icon_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id = -1, Key p_accel = Key::NONE);
	void add_check_item(const String &p_label, int p_id = -1, Key p_accel = Key::NONE);
	void add_icon_check_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id = -1, Key p_accel = Key::NONE);
	void add_radio_check_item(const String &p_label, int p_id = -1, Key p_accel = Key::NONE);
	void add_icon_radio_check_item(const Ref<Texture2D> &p_icon, const String &p_label, int p_id = -1, Key p_accel = Key::NONE);

	void add_multistate_item(const String &p_label, int p_max_states, int p_default_state = 0, int p_id = -1, Key p_accel = Key::NONE);

	void add_shortcut(const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false, bool p_allow_echo = false);
	void add_icon_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false, bool p_allow_echo = false);
	void add_check_shortcut(const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);
	void add_icon_check_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);
	void add_radio_check_shortcut(const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);
	void add_icon_radio_check_shortcut(const Ref<Texture2D> &p_icon, const Ref<Shortcut> &p_shortcut, int p_id = -1, bool p_global = false);

	void add_submenu_item(const String &p_label, const String &p_submenu, int p_id = -1);
	void add_submenu_node_item(const String &p_label, PopupMenu *p_submenu, int p_id = -1);

	void set_item_text(int p_idx, const String &p_text);

	void set_item_text_direction(int p_idx, Control::TextDirection p_text_direction);
	void set_item_language(int p_idx, const String &p_language);
	void set_item_icon(int p_idx, const Ref<Texture2D> &p_icon);
	void set_item_icon_max_width(int p_idx, int p_width);
	void set_item_icon_modulate(int p_idx, const Color &p_modulate);
	void set_item_checked(int p_idx, bool p_checked);
	void set_item_id(int p_idx, int p_id);
	void set_item_accelerator(int p_idx, Key p_accel);
	void set_item_metadata(int p_idx, const Variant &p_meta);
	void set_item_disabled(int p_idx, bool p_disabled);
	void set_item_submenu(int p_idx, const String &p_submenu);
	void set_item_submenu_node(int p_idx, PopupMenu *p_submenu);
	void set_item_as_separator(int p_idx, bool p_separator);
	void set_item_as_checkable(int p_idx, bool p_checkable);
	void set_item_as_radio_checkable(int p_idx, bool p_radio_checkable);
	void set_item_tooltip(int p_idx, const String &p_tooltip);
	void set_item_shortcut(int p_idx, const Ref<Shortcut> &p_shortcut, bool p_global = false);
	void set_item_indent(int p_idx, int p_indent);
	void set_item_max_states(int p_idx, int p_max_states);
	void set_item_multistate(int p_idx, int p_state);
	void toggle_item_multistate(int p_idx);
	void set_item_shortcut_disabled(int p_idx, bool p_disabled);

	void toggle_item_checked(int p_idx);

	String get_item_text(int p_idx) const;
	String get_item_xl_text(int p_idx) const;
	Control::TextDirection get_item_text_direction(int p_idx) const;
	String get_item_language(int p_idx) const;
	int get_item_idx_from_text(const String &text) const;
	Ref<Texture2D> get_item_icon(int p_idx) const;
	int get_item_icon_max_width(int p_idx) const;
	Color get_item_icon_modulate(int p_idx) const;
	bool is_item_checked(int p_idx) const;
	int get_item_id(int p_idx) const;
	int get_item_index(int p_id) const;
	Key get_item_accelerator(int p_idx) const;
	Variant get_item_metadata(int p_idx) const;
	bool is_item_disabled(int p_idx) const;
	String get_item_submenu(int p_idx) const;
	PopupMenu *get_item_submenu_node(int p_idx) const;
	bool is_item_separator(int p_idx) const;
	bool is_item_checkable(int p_idx) const;
	bool is_item_radio_checkable(int p_idx) const;
	bool is_item_shortcut_disabled(int p_idx) const;
	bool is_item_shortcut_global(int p_idx) const;
	String get_item_tooltip(int p_idx) const;
	Ref<Shortcut> get_item_shortcut(int p_idx) const;
	int get_item_indent(int p_idx) const;
	int get_item_max_states(int p_idx) const;
	int get_item_state(int p_idx) const;

	void set_focused_item(int p_idx);
	int get_focused_item() const;

	void set_item_count(int p_count);
	int get_item_count() const;

	void set_prefer_native_menu(bool p_enabled);
	bool is_prefer_native_menu() const;

	bool is_native_menu() const;

	void scroll_to_item(int p_idx);

	bool activate_item_by_event(const Ref<InputEvent> &p_event, bool p_for_global_only = false);
	void activate_item(int p_idx);

	void _about_to_popup();
	void _about_to_close();

	void remove_item(int p_idx);

	void add_separator(const String &p_text = String(), int p_id = -1);

	void clear(bool p_free_submenus = true);

	virtual String get_tooltip(const Point2 &p_pos) const;

	void add_autohide_area(const Rect2 &p_area);
	void clear_autohide_areas();

	void set_hide_on_item_selection(bool p_enabled);
	bool is_hide_on_item_selection() const;

	void set_hide_on_checkable_item_selection(bool p_enabled);
	bool is_hide_on_checkable_item_selection() const;

	void set_hide_on_multistate_item_selection(bool p_enabled);
	bool is_hide_on_multistate_item_selection() const;

	void set_submenu_popup_delay(float p_time);
	float get_submenu_popup_delay() const;

	void set_allow_search(bool p_allow);
	bool get_allow_search() const;

	virtual void popup(const Rect2i &p_bounds = Rect2i()) override;
	virtual void set_visible(bool p_visible) override;

	PopupMenu();
	~PopupMenu();
};

#endif // POPUP_MENU_H
