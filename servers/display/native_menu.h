/**************************************************************************/
/*  native_menu.h                                                         */
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

#ifndef NATIVE_MENU_H
#define NATIVE_MENU_H

#include "core/input/input.h"
#include "core/variant/callable.h"

class Texture2D;

class NativeMenu : public Object {
	GDCLASS(NativeMenu, Object)

	static NativeMenu *singleton;

protected:
	static void _bind_methods();

public:
	_FORCE_INLINE_ static NativeMenu *get_singleton() {
		return singleton;
	}

	enum Feature {
		FEATURE_GLOBAL_MENU,
		FEATURE_POPUP_MENU,
		FEATURE_OPEN_CLOSE_CALLBACK,
		FEATURE_HOVER_CALLBACK,
		FEATURE_KEY_CALLBACK,
	};

	enum SystemMenus {
		INVALID_MENU_ID,
		MAIN_MENU_ID,
		APPLICATION_MENU_ID,
		WINDOW_MENU_ID,
		HELP_MENU_ID,
		DOCK_MENU_ID,
	};

	virtual bool has_feature(Feature p_feature) const;

	virtual bool has_system_menu(SystemMenus p_menu_id) const;
	virtual RID get_system_menu(SystemMenus p_menu_id) const;
	virtual String get_system_menu_name(SystemMenus p_menu_id) const;

	virtual RID create_menu();
	virtual bool has_menu(const RID &p_rid) const;
	virtual void free_menu(const RID &p_rid);

	virtual Size2 get_size(const RID &p_rid) const;
	virtual void popup(const RID &p_rid, const Vector2i &p_position);

	virtual void set_interface_direction(const RID &p_rid, bool p_is_rtl);

	virtual void set_popup_open_callback(const RID &p_rid, const Callable &p_callback);
	virtual Callable get_popup_open_callback(const RID &p_rid) const;
	virtual void set_popup_close_callback(const RID &p_rid, const Callable &p_callback);
	virtual Callable get_popup_close_callback(const RID &p_rid) const;
	virtual void set_minimum_width(const RID &p_rid, float p_width);
	virtual float get_minimum_width(const RID &p_rid) const;

	virtual bool is_opened(const RID &p_rid) const;

	virtual int add_submenu_item(const RID &p_rid, const String &p_label, const RID &p_submenu_rid, const Variant &p_tag = Variant(), int p_index = -1);
	virtual int add_item(const RID &p_rid, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_icon_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_icon_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_radio_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_icon_radio_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_multistate_item(const RID &p_rid, const String &p_label, int p_max_states, int p_default_state, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int add_separator(const RID &p_rid, int p_index = -1);

	virtual int find_item_index_with_text(const RID &p_rid, const String &p_text) const;
	virtual int find_item_index_with_tag(const RID &p_rid, const Variant &p_tag) const;
	virtual int find_item_index_with_submenu(const RID &p_rid, const RID &p_submenu_rid) const;

	virtual bool is_item_checked(const RID &p_rid, int p_idx) const;
	virtual bool is_item_checkable(const RID &p_rid, int p_idx) const;
	virtual bool is_item_radio_checkable(const RID &p_rid, int p_idx) const;
	virtual Callable get_item_callback(const RID &p_rid, int p_idx) const;
	virtual Callable get_item_key_callback(const RID &p_rid, int p_idx) const;
	virtual Variant get_item_tag(const RID &p_rid, int p_idx) const;
	virtual String get_item_text(const RID &p_rid, int p_idx) const;
	virtual RID get_item_submenu(const RID &p_rid, int p_idx) const;
	virtual Key get_item_accelerator(const RID &p_rid, int p_idx) const;
	virtual bool is_item_disabled(const RID &p_rid, int p_idx) const;
	virtual bool is_item_hidden(const RID &p_rid, int p_idx) const;
	virtual String get_item_tooltip(const RID &p_rid, int p_idx) const;
	virtual int get_item_state(const RID &p_rid, int p_idx) const;
	virtual int get_item_max_states(const RID &p_rid, int p_idx) const;
	virtual Ref<Texture2D> get_item_icon(const RID &p_rid, int p_idx) const;
	virtual int get_item_indentation_level(const RID &p_rid, int p_idx) const;

	virtual void set_item_checked(const RID &p_rid, int p_idx, bool p_checked);
	virtual void set_item_checkable(const RID &p_rid, int p_idx, bool p_checkable);
	virtual void set_item_radio_checkable(const RID &p_rid, int p_idx, bool p_checkable);
	virtual void set_item_callback(const RID &p_rid, int p_idx, const Callable &p_callback);
	virtual void set_item_key_callback(const RID &p_rid, int p_idx, const Callable &p_key_callback);
	virtual void set_item_hover_callbacks(const RID &p_rid, int p_idx, const Callable &p_callback);
	virtual void set_item_tag(const RID &p_rid, int p_idx, const Variant &p_tag);
	virtual void set_item_text(const RID &p_rid, int p_idx, const String &p_text);
	virtual void set_item_submenu(const RID &p_rid, int p_idx, const RID &p_submenu_rid);
	virtual void set_item_accelerator(const RID &p_rid, int p_idx, Key p_keycode);
	virtual void set_item_disabled(const RID &p_rid, int p_idx, bool p_disabled);
	virtual void set_item_hidden(const RID &p_rid, int p_idx, bool p_hidden);
	virtual void set_item_tooltip(const RID &p_rid, int p_idx, const String &p_tooltip);
	virtual void set_item_state(const RID &p_rid, int p_idx, int p_state);
	virtual void set_item_max_states(const RID &p_rid, int p_idx, int p_max_states);
	virtual void set_item_icon(const RID &p_rid, int p_idx, const Ref<Texture2D> &p_icon);
	virtual void set_item_indentation_level(const RID &p_rid, int p_idx, int p_level);

	virtual int get_item_count(const RID &p_rid) const;
	virtual bool is_system_menu(const RID &p_rid) const;

	virtual void remove_item(const RID &p_rid, int p_idx);
	virtual void clear(const RID &p_rid);

	NativeMenu() {
		singleton = this;
	}

	~NativeMenu() {
		singleton = nullptr;
	}
};

VARIANT_ENUM_CAST(NativeMenu::Feature);
VARIANT_ENUM_CAST(NativeMenu::SystemMenus);

#endif // NATIVE_MENU_H
