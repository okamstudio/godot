/**************************************************************************/
/*  editor_bottom_panel.h                                                 */
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

#ifndef EDITOR_BOTTOM_PANEL_H
#define EDITOR_BOTTOM_PANEL_H

#include "scene/gui/panel_container.h"

class Button;
class ConfigFile;
class EditorToaster;
class HBoxContainer;
class VBoxContainer;

class EditorBottomPanel : public PanelContainer {
	GDCLASS(EditorBottomPanel, PanelContainer);

	struct BottomPanelItem {
		String name;
		Control *control = nullptr;
		Button *button = nullptr;
	};

	Vector<BottomPanelItem> items;
	bool lock_panel_switching = false;

	VBoxContainer *item_vbox = nullptr;
	HBoxContainer *bottom_hbox = nullptr;
	HBoxContainer *button_hbox = nullptr;
	EditorToaster *editor_toaster = nullptr;
	Button *pin_button = nullptr;
	Button *expand_button = nullptr;
	Control *last_opened_control = nullptr;

	void _switch_by_control(bool p_visible, Control *p_control, bool p_ignore_lock = false);
	void _switch_to_item(bool p_visible, int p_idx, bool p_ignore_lock = false);
	void _pin_button_toggled(bool p_pressed);
	void _expand_button_toggled(bool p_pressed);

	bool _button_drag_hover(const Vector2 &, const Variant &, Button *p_button, Control *p_control);

protected:
	void _notification(int p_what);

public:
	void save_layout_to_config(Ref<ConfigFile> p_config_file, const String &p_section) const;
	void load_layout_from_config(Ref<ConfigFile> p_config_file, const String &p_section);

	Button *add_item(String p_text, Control *p_item, const Ref<Shortcut> &p_shortcut = nullptr, bool p_at_front = false);
	void remove_item(Control *p_item);
	void make_item_visible(Control *p_item, bool p_visible = true, bool p_ignore_lock = false);
	void move_item_to_end(Control *p_item);
	void hide_bottom_panel();
	void toggle_last_opened_bottom_panel();

	EditorBottomPanel();
};

#endif // EDITOR_BOTTOM_PANEL_H
