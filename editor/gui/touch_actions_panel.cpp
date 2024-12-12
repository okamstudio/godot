/**************************************************************************/
/*  touch_actions_panel.cpp                                               */
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

#include "touch_actions_panel.h"

#include "core/input/input.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/style_box_flat.h"

void TouchActionsPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			drag_handle->set_texture(get_editor_theme_icon(SNAME("DragHandle")));
			layout_toggle_button->set_button_icon(get_editor_theme_icon(SNAME("Orientation")));
			lock_panel_button->set_button_icon(get_editor_theme_icon(SNAME("Lock")));
			save_button->set_button_icon(get_editor_theme_icon(SNAME("Save")));
			delete_button->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			undo_button->set_button_icon(get_editor_theme_icon(SNAME("UndoRedo")));
			redo_button->set_button_icon(get_editor_theme_icon(SNAME("Redo")));
		} break;
	}
}

void TouchActionsPanel::_simulate_editor_shortcut(const String &p_shortcut_name) {
	Ref<Shortcut> shortcut = ED_GET_SHORTCUT(p_shortcut_name);

	if (shortcut.is_valid() && !shortcut->get_events().is_empty()) {
		Ref<InputEventKey> event = shortcut->get_events()[0];
		if (event.is_valid()) {
			event->set_pressed(true);
			Input::get_singleton()->parse_input_event(event);
		}
	}
}

void TouchActionsPanel::_simulate_key_press(Key p_keycode) {
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(p_keycode);
	event->set_pressed(true);
	Input::get_singleton()->parse_input_event(event);
}

Button *TouchActionsPanel::_add_new_action_button(const String &p_shortcut, Key p_keycode) {
	Button *action_button = memnew(Button);
	action_button->set_focus_mode(Control::FOCUS_NONE);
	action_button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	action_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	if (p_keycode == Key::NONE) {
		action_button->connect(SceneStringName(pressed), callable_mp(this, &TouchActionsPanel::_simulate_editor_shortcut).bind(p_shortcut));
	} else {
		action_button->connect(SceneStringName(pressed), callable_mp(this, &TouchActionsPanel::_simulate_key_press).bind(p_keycode));
	}
	box->add_child(action_button);
	return action_button;
}

void TouchActionsPanel::_on_drag_handle_gui_input(const Ref<InputEvent> &p_event) {
	if (lock_panel_position) {
		return;
	}
	Ref<InputEventMouseButton> mouse_button_event = p_event;
	if (mouse_button_event.is_valid() && mouse_button_event->get_button_index() == MouseButton::LEFT) {
		if (mouse_button_event->is_pressed()) {
			dragging = true;
			drag_offset = mouse_button_event->get_position();
		} else {
			dragging = false;
		}
	}

	Ref<InputEventMouseMotion> mouse_motion_event = p_event;
	if (dragging && mouse_motion_event.is_valid()) {
		Vector2 new_position = get_position() + mouse_motion_event->get_relative();
		const float margin = 25.0;
		Vector2 parent_size = get_parent_area_size();
		Vector2 panel_size = get_size();
		new_position = new_position.clamp(Vector2(margin, margin), parent_size - panel_size - Vector2(margin, margin));
		set_position(new_position);
	}
}

void TouchActionsPanel::_switch_layout() {
	box->set_vertical(!box->is_vertical());
	reset_size();
}

void TouchActionsPanel::_lock_panel_toggled(bool p_pressed) {
	lock_panel_position = p_pressed;
	layout_toggle_button->set_disabled(p_pressed);
}

TouchActionsPanel::TouchActionsPanel() {
	Ref<StyleBoxFlat> panel_style;
	panel_style.instantiate();
	panel_style->set_bg_color(Color(0.1, 0.1, 0.1, 1));
	panel_style->set_border_color(Color(0.3, 0.3, 0.3, 1));
	panel_style->set_border_width_all(3);
	panel_style->set_corner_radius_all(10);
	panel_style->set_content_margin_all(12);
	add_theme_style_override(SceneStringName(panel), panel_style);

	set_anchors_and_offsets_preset(Control::PRESET_CENTER_BOTTOM, Control::PRESET_MODE_MINSIZE, 80);

	box = memnew(BoxContainer);
	box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	box->add_theme_constant_override("separation", 15);
	add_child(box);

	drag_handle = memnew(TextureRect);
	drag_handle->set_custom_minimum_size(Size2(40, 40));
	drag_handle->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	drag_handle->connect(SceneStringName(gui_input), callable_mp(this, &TouchActionsPanel::_on_drag_handle_gui_input));
	box->add_child(drag_handle);

	layout_toggle_button = memnew(Button);
	layout_toggle_button->set_focus_mode(Control::FOCUS_NONE);
	layout_toggle_button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	layout_toggle_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	layout_toggle_button->connect(SceneStringName(pressed), callable_mp(this, &TouchActionsPanel::_switch_layout));
	box->add_child(layout_toggle_button);

	lock_panel_button = memnew(Button);
	lock_panel_button->set_toggle_mode(true);
	lock_panel_button->set_focus_mode(Control::FOCUS_NONE);
	lock_panel_button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	lock_panel_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	lock_panel_button->connect(SceneStringName(toggled), callable_mp(this, &TouchActionsPanel::_lock_panel_toggled));
	box->add_child(lock_panel_button);

	ColorRect *separator = memnew(ColorRect);
	separator->set_color(Color(0.5, 0.5, 0.5));
	separator->set_custom_minimum_size(Size2(2, 2));
	box->add_child(separator);

	// Add action buttons.
	save_button = _add_new_action_button("editor/save_scene");
	delete_button = _add_new_action_button("", Key::KEY_DELETE);
	undo_button = _add_new_action_button("ui_undo");
	redo_button = _add_new_action_button("ui_redo");
}
