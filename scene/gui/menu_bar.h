/**************************************************************************/
/*  menu_bar.h                                                            */
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

#include "scene/gui/popup_menu.h"

class MenuBar : public Control {
	GDCLASS(MenuBar, Control);

	Mutex mutex;

	bool switch_on_hover = true;
	bool disable_shortcuts = false;
	bool prefer_native = true;
	bool flat = false;
	int start_index = -1;

	String language;
	TextDirection text_direction = TEXT_DIRECTION_AUTO;

	struct Menu {
		String name;
		String tooltip;

		Ref<TextLine> text_buf;
		bool hidden = false;
		bool disabled = false;
		RID submenu_rid;

		Menu(const String &p_name) {
			name = p_name;
			text_buf.instantiate();
		}

		Menu() {
			text_buf.instantiate();
		}
	};
	Vector<Menu> menu_cache;

	int focused_menu = -1;
	int selected_menu = -1;
	int active_menu = -1;

	Vector2i old_mouse_pos;
	ObjectID shortcut_context;

	struct ThemeCache {
		Ref<StyleBox> normal;
		Ref<StyleBox> normal_mirrored;
		Ref<StyleBox> disabled;
		Ref<StyleBox> disabled_mirrored;
		Ref<StyleBox> pressed;
		Ref<StyleBox> pressed_mirrored;
		Ref<StyleBox> hover;
		Ref<StyleBox> hover_mirrored;
		Ref<StyleBox> hover_pressed;
		Ref<StyleBox> hover_pressed_mirrored;

		Ref<Font> font;
		int font_size = 0;
		int outline_size = 0;
		Color font_outline_color;

		Color font_color;
		Color font_disabled_color;
		Color font_pressed_color;
		Color font_hover_color;
		Color font_hover_pressed_color;
		Color font_focus_color;

		int h_separation = 0;
	} theme_cache;

	int _get_index_at_point(const Point2 &p_point) const;
	Rect2 _get_menu_item_rect(int p_index) const;
	void _draw_menu_item(int p_index);

	void shape(Menu &p_menu);
	void _refresh_menu_names();
	Vector<PopupMenu *> _get_popups() const;
	int get_menu_idx_from_control(PopupMenu *p_child) const;

	void _open_popup(int p_index, bool p_focus_item = false);
	void _popup_visibility_changed(bool p_visible);

	String global_menu_tag;

	int _find_global_start_index() {
		if (global_menu_tag.is_empty()) {
			return -1;
		}

		NativeMenu *nmenu = NativeMenu::get_singleton();
		if (!nmenu) {
			return -1;
		}
		RID main_menu = nmenu->get_system_menu(NativeMenu::MAIN_MENU_ID);
		int count = nmenu->get_item_count(main_menu);
		for (int i = 0; i < count; i++) {
			if (nmenu->get_item_tag(main_menu, i).operator String().begins_with(global_menu_tag)) {
				return i;
			}
		}
		return -1;
	}

	void _popup_changed(ObjectID p_menu);

	void bind_global_menu();
	void unbind_global_menu();

protected:
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);
	virtual void add_child_notify(Node *p_child) override;
	virtual void move_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;
	static void _bind_methods();

public:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	void set_switch_on_hover(bool p_enabled);
	bool is_switch_on_hover();
	void set_disable_shortcuts(bool p_disabled);

	void set_prefer_global_menu(bool p_enabled);
	bool is_prefer_global_menu() const;

	bool is_native_menu() const;

	virtual Size2 get_minimum_size() const override;

	int get_menu_count() const;

	void set_text_direction(TextDirection p_text_direction);
	TextDirection get_text_direction() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_start_index(int p_index);
	int get_start_index() const;

	void set_flat(bool p_enabled);
	bool is_flat() const;

	void set_menu_title(int p_menu, const String &p_title);
	String get_menu_title(int p_menu) const;

	void set_menu_tooltip(int p_menu, const String &p_tooltip);
	String get_menu_tooltip(int p_menu) const;

	void set_menu_disabled(int p_menu, bool p_disabled);
	bool is_menu_disabled(int p_menu) const;

	void set_menu_hidden(int p_menu, bool p_hidden);
	bool is_menu_hidden(int p_menu) const;

	PopupMenu *get_menu_popup(int p_menu) const;

	virtual String get_tooltip(const Point2 &p_pos) const override;

	MenuBar();
	~MenuBar();
};
