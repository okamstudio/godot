/**************************************************************************/
/*  editor_dock_manager.h                                                 */
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

#include "scene/gui/popup.h"
#include "scene/gui/split_container.h"

class Button;
class ConfigFile;
class Control;
class PopupMenu;
class TabContainer;
class VBoxContainer;
class WindowWrapper;

class DockSplitContainer : public SplitContainer {
	GDCLASS(DockSplitContainer, SplitContainer);

private:
	bool is_updating = false;

protected:
	void _update_visibility();

	virtual void add_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;
};

class DockContextPopup;

class EditorDockManager : public Object {
	GDCLASS(EditorDockManager, Object);

public:
	enum DockSlot {
		DOCK_SLOT_NONE = -1,
		DOCK_SLOT_LEFT_UL,
		DOCK_SLOT_LEFT_BL,
		DOCK_SLOT_LEFT_UR,
		DOCK_SLOT_LEFT_BR,
		DOCK_SLOT_RIGHT_UL,
		DOCK_SLOT_RIGHT_BL,
		DOCK_SLOT_RIGHT_UR,
		DOCK_SLOT_RIGHT_BR,
		DOCK_SLOT_MAX
	};

private:
	friend class DockContextPopup;

	struct DockInfo {
		String title;
		bool open = false;
		bool enabled = true;
		bool at_bottom = false;
		int previous_tab_index = -1;
		bool previous_at_bottom = false;
		WindowWrapper *dock_window = nullptr;
		int dock_slot_index = DOCK_SLOT_NONE;
		Ref<Shortcut> shortcut;
		Ref<Texture2D> icon; // Only used when `icon_name` is empty.
		StringName icon_name;
	};

	static EditorDockManager *singleton;

	// To access splits easily by index.
	Vector<DockSplitContainer *> vsplits;
	Vector<DockSplitContainer *> hsplits;

	Vector<WindowWrapper *> dock_windows;
	TabContainer *dock_slot[DOCK_SLOT_MAX];
	HashMap<Control *, DockInfo> all_docks;
	bool docks_visible = true;

	DockContextPopup *dock_context_popup = nullptr;
	PopupMenu *docks_menu = nullptr;
	Vector<Control *> docks_menu_docks;
	Control *closed_dock_parent = nullptr;

	void _dock_split_dragged(int p_offset);
	void _dock_container_gui_input(const Ref<InputEvent> &p_input, TabContainer *p_dock_container);
	void _bottom_dock_button_gui_input(const Ref<InputEvent> &p_input, Control *p_dock, Button *p_bottom_button);
	void _dock_container_update_visibility(TabContainer *p_dock_container);
	void _update_layout();

	void _update_docks_menu();
	void _docks_menu_option(int p_id);

	void _window_close_request(WindowWrapper *p_wrapper);
	Control *_close_window(WindowWrapper *p_wrapper);
	void _open_dock_in_window(Control *p_dock, bool p_show_window = true, bool p_reset_size = false);
	void _restore_dock_to_saved_window(Control *p_dock, const Dictionary &p_window_dump);

	void _dock_move_to_bottom(Control *p_dock, bool p_visible);
	void _dock_remove_from_bottom(Control *p_dock);
	bool _is_dock_at_bottom(Control *p_dock);

	void _move_dock_tab_index(Control *p_dock, int p_tab_index, bool p_set_current);
	void _move_dock(Control *p_dock, Control *p_target, int p_tab_index = -1, bool p_set_current = true);

	void _update_tab_style(Control *p_dock);

public:
	static EditorDockManager *get_singleton() { return singleton; }

	void update_tab_styles();
	void set_tab_icon_max_width(int p_max_width);

	void add_vsplit(DockSplitContainer *p_split);
	void add_hsplit(DockSplitContainer *p_split);
	void register_dock_slot(DockSlot p_dock_slot, TabContainer *p_tab_container);
	int get_vsplit_count() const;
	PopupMenu *get_docks_menu();

	void save_docks_to_config(Ref<ConfigFile> p_layout, const String &p_section) const;
	void load_docks_from_config(Ref<ConfigFile> p_layout, const String &p_section, bool p_first_load = false);

	void set_dock_enabled(Control *p_dock, bool p_enabled);
	void close_dock(Control *p_dock);
	void open_dock(Control *p_dock, bool p_set_current = true);
	void focus_dock(Control *p_dock);

	TabContainer *get_dock_tab_container(Control *p_dock) const;

	void bottom_dock_show_placement_popup(const Rect2i &p_position, Control *p_dock);

	void set_docks_visible(bool p_show);
	bool are_docks_visible() const;

	void add_dock(Control *p_dock, const String &p_title = "", DockSlot p_slot = DOCK_SLOT_NONE, const Ref<Shortcut> &p_shortcut = nullptr, const StringName &p_icon_name = StringName());
	void remove_dock(Control *p_dock);

	void set_dock_tab_icon(Control *p_dock, const Ref<Texture2D> &p_icon);

	EditorDockManager();
};

class DockContextPopup : public PopupPanel {
	GDCLASS(DockContextPopup, PopupPanel);

	VBoxContainer *dock_select_popup_vb = nullptr;

	Button *make_float_button = nullptr;
	Button *tab_move_left_button = nullptr;
	Button *tab_move_right_button = nullptr;
	Button *close_button = nullptr;
	Button *dock_to_bottom_button = nullptr;

	Control *dock_select = nullptr;
	Rect2 dock_select_rects[EditorDockManager::DOCK_SLOT_MAX];
	int dock_select_rect_over_idx = -1;

	Control *context_dock = nullptr;

	EditorDockManager *dock_manager = nullptr;

	void _tab_move_left();
	void _tab_move_right();
	void _close_dock();
	void _float_dock();
	void _move_dock_to_bottom();

	void _dock_select_input(const Ref<InputEvent> &p_input);
	void _dock_select_mouse_exited();
	void _dock_select_draw();

	void _update_buttons();

protected:
	void _notification(int p_what);

public:
	void select_current_dock_in_dock_slot(int p_dock_slot);
	void set_dock(Control *p_dock);
	Control *get_dock() const;
	void docks_updated();

	DockContextPopup();
};
