/**************************************************************************/
/*  dialogs.cpp                                                           */
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

#include "dialogs.h"
#include "dialogs.compat.inc"

#include "scene/gui/line_edit.h"
#include "scene/theme/theme_db.h"

// AcceptDialog

void AcceptDialog::_input_from_window(const Ref<InputEvent> &p_event) {
	if (close_on_escape && p_event->is_action_pressed(SNAME("ui_cancel"), false, true)) {
		_cancel_pressed();
	}
	Window::_input_from_window(p_event);
}

void AcceptDialog::_parent_focused() {
	if (popped_up && !is_exclusive() && get_flag(FLAG_POPUP)) {
		_cancel_pressed();
	}
}

void AcceptDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			if (is_visible()) {
				get_ok_button()->grab_focus();
			}
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				if (get_ok_button()->is_inside_tree()) {
					get_ok_button()->grab_focus();
				}
				_update_child_rects();

				parent_visible = get_parent_visible_window();
				if (parent_visible) {
					parent_visible->connect(SceneStringName(focus_entered), callable_mp(this, &AcceptDialog::_parent_focused));
				}
			} else {
				popped_up = false;
				if (parent_visible) {
					parent_visible->disconnect(SceneStringName(focus_entered), callable_mp(this, &AcceptDialog::_parent_focused));
					parent_visible = nullptr;
				}
			}
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			if (!is_in_edited_scene_root()) {
				if (has_focus()) {
					popped_up = true;
				}
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			bg_panel->add_theme_style_override(SceneStringName(panel), theme_cache.panel_style);

			child_controls_changed();
			if (is_visible()) {
				_update_child_rects();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (parent_visible) {
				parent_visible->disconnect(SceneStringName(focus_entered), callable_mp(this, &AcceptDialog::_parent_focused));
				parent_visible = nullptr;
			}
		} break;

		case NOTIFICATION_READY:
		case NOTIFICATION_WM_SIZE_CHANGED: {
			if (is_visible()) {
				_update_child_rects();
			}
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			_cancel_pressed();
		} break;
	}
}

void AcceptDialog::_text_submitted(const String &p_text) {
	if (get_ok_button() && get_ok_button()->is_disabled()) {
		return; // Do not allow submission if OK button is disabled.
	}
	_ok_pressed();
}

void AcceptDialog::_post_popup() {
	Window::_post_popup();
	popped_up = true;
}

void AcceptDialog::_ok_pressed() {
	if (hide_on_ok) {
		popped_up = false;
		set_visible(false);
	}
	ok_pressed();
	emit_signal(SceneStringName(confirmed));
	set_input_as_handled();
}

void AcceptDialog::_cancel_pressed() {
	popped_up = false;
	Window *parent_window = parent_visible;
	if (parent_visible) {
		parent_visible->disconnect(SceneStringName(focus_entered), callable_mp(this, &AcceptDialog::_parent_focused));
		parent_visible = nullptr;
	}

	callable_mp((Window *)this, &Window::hide).call_deferred();

	emit_signal(SNAME("canceled"));

	cancel_pressed();

	if (parent_window) {
		//parent_window->grab_focus();
	}
	set_input_as_handled();
}

String AcceptDialog::get_text() const {
	return message_label->get_text();
}

void AcceptDialog::set_text(String p_text) {
	if (message_label->get_text() == p_text) {
		return;
	}

	message_label->set_text(p_text);

	child_controls_changed();
	if (is_visible()) {
		_update_child_rects();
	}
}

void AcceptDialog::set_hide_on_ok(bool p_hide) {
	hide_on_ok = p_hide;
}

bool AcceptDialog::get_hide_on_ok() const {
	return hide_on_ok;
}

void AcceptDialog::set_close_on_escape(bool p_hide) {
	close_on_escape = p_hide;
}

bool AcceptDialog::get_close_on_escape() const {
	return close_on_escape;
}

void AcceptDialog::set_autowrap(bool p_autowrap) {
	message_label->set_autowrap_mode(p_autowrap ? TextServer::AUTOWRAP_WORD : TextServer::AUTOWRAP_OFF);
}

bool AcceptDialog::has_autowrap() {
	return message_label->get_autowrap_mode() != TextServer::AUTOWRAP_OFF;
}

void AcceptDialog::set_ok_button_text(String p_ok_button_text) {
	ok_button->set_text(p_ok_button_text);

	child_controls_changed();
	if (is_visible()) {
		_update_child_rects();
	}
}

String AcceptDialog::get_ok_button_text() const {
	return ok_button->get_text();
}

void AcceptDialog::register_text_enter(LineEdit *p_line_edit) {
	ERR_FAIL_NULL(p_line_edit);
	p_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &AcceptDialog::_text_submitted));
}

void AcceptDialog::_update_child_rects() {
	Size2 dlg_size = Vector2(get_size()) / get_content_scale_factor();
	float h_margins = theme_cache.panel_style->get_margin(SIDE_LEFT) + theme_cache.panel_style->get_margin(SIDE_RIGHT);
	float v_margins = theme_cache.panel_style->get_margin(SIDE_TOP) + theme_cache.panel_style->get_margin(SIDE_BOTTOM);

	// Fill the entire size of the window with the background.
	bg_panel->set_position(Point2());
	bg_panel->set_size(dlg_size);

	for (int i = 0; i < buttons_hbox->get_child_count(); i++) {
		Button *b = Object::cast_to<Button>(buttons_hbox->get_child(i));
		if (!b) {
			continue;
		}

		b->set_custom_minimum_size(Size2(theme_cache.buttons_min_width, theme_cache.buttons_min_height));
	}

	// Place the buttons from the bottom edge to their minimum required size.
	Size2 buttons_minsize = buttons_hbox->get_combined_minimum_size();
	Size2 buttons_size = Size2(dlg_size.x - h_margins, buttons_minsize.y);
	Point2 buttons_position = Point2(theme_cache.panel_style->get_margin(SIDE_LEFT), dlg_size.y - theme_cache.panel_style->get_margin(SIDE_BOTTOM) - buttons_size.y);
	buttons_hbox->set_position(buttons_position);
	buttons_hbox->set_size(buttons_size);

	// Place the content from the top to fill the rest of the space (minus the separation).
	Point2 content_position = Point2(theme_cache.panel_style->get_margin(SIDE_LEFT), theme_cache.panel_style->get_margin(SIDE_TOP));
	Size2 content_size = Size2(dlg_size.x - h_margins, dlg_size.y - v_margins - buttons_size.y - theme_cache.buttons_separation);

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c == buttons_hbox || c == bg_panel || c->is_set_as_top_level()) {
			continue;
		}

		c->set_position(content_position);
		c->set_size(content_size);
	}
}

Size2 AcceptDialog::_get_contents_minimum_size() const {
	// First, we then iterate over the label and any other custom controls
	// to try and find the size that encompasses all content.
	Size2 content_minsize;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}

		// Buttons will be included afterwards.
		// The panel only displays the stylebox and doesn't contribute to the size.
		if (c == buttons_hbox || c == bg_panel || c->is_set_as_top_level()) {
			continue;
		}

		Size2 child_minsize = c->get_combined_minimum_size();
		content_minsize = child_minsize.max(content_minsize);
	}

	// Then we add buttons. Horizontally we're interested in whichever
	// value is the biggest. Vertically buttons add to the overall size.
	Size2 buttons_minsize = buttons_hbox->get_combined_minimum_size();
	content_minsize.x = MAX(buttons_minsize.x, content_minsize.x);
	content_minsize.y += buttons_minsize.y;
	// Plus there is a separation size added on top.
	content_minsize.y += theme_cache.buttons_separation;

	// Then we take the background panel as it provides the offsets,
	// which are always added to the minimum size.
	if (theme_cache.panel_style.is_valid()) {
		content_minsize += theme_cache.panel_style->get_minimum_size();
	}

	return content_minsize;
}

void AcceptDialog::_custom_action(const String &p_action) {
	emit_signal(SNAME("custom_action"), p_action);
	custom_action(p_action);
}

void AcceptDialog::_custom_button_visibility_changed(Button *button) {
	Control *right_spacer = Object::cast_to<Control>(button->get_meta("__right_spacer"));
	if (right_spacer) {
		right_spacer->set_visible(button->is_visible());
	}
}

Button *AcceptDialog::add_button(const String &p_text, bool p_right, const String &p_action) {
	Button *button = memnew(Button);
	button->set_text(p_text);

	Control *right_spacer;
	if (p_right) {
		buttons_hbox->add_child(button);
		right_spacer = buttons_hbox->add_spacer();
	} else {
		buttons_hbox->add_child(button);
		buttons_hbox->move_child(button, 0);
		right_spacer = buttons_hbox->add_spacer(true);
	}
	button->set_meta("__right_spacer", right_spacer);

	button->connect(SceneStringName(visibility_changed), callable_mp(this, &AcceptDialog::_custom_button_visibility_changed).bind(button));

	child_controls_changed();
	if (is_visible()) {
		_update_child_rects();
	}

	if (!p_action.is_empty()) {
		button->connect(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_custom_action).bind(p_action));
	}

	return button;
}

Button *AcceptDialog::add_cancel_button(const String &p_cancel) {
	String c = p_cancel;
	if (p_cancel.is_empty()) {
		c = ETR("Cancel");
	}

	Button *b = swap_cancel_ok ? add_button(c, true) : add_button(c);

	b->connect(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_cancel_pressed));

	return b;
}

void AcceptDialog::remove_button(Button *p_button) {
	ERR_FAIL_NULL(p_button);
	ERR_FAIL_COND_MSG(p_button->get_parent() != buttons_hbox, vformat("Cannot remove button %s as it does not belong to this dialog.", p_button->get_name()));
	ERR_FAIL_COND_MSG(p_button == ok_button, "Cannot remove dialog's OK button.");

	Control *right_spacer = Object::cast_to<Control>(p_button->get_meta("__right_spacer"));
	if (right_spacer) {
		ERR_FAIL_COND_MSG(right_spacer->get_parent() != buttons_hbox, vformat("Cannot remove button %s as its associated spacer does not belong to this dialog.", p_button->get_name()));
	}

	p_button->disconnect(SceneStringName(visibility_changed), callable_mp(this, &AcceptDialog::_custom_button_visibility_changed));
	if (p_button->is_connected(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_custom_action))) {
		p_button->disconnect(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_custom_action));
	}
	if (p_button->is_connected(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_cancel_pressed))) {
		p_button->disconnect(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_cancel_pressed));
	}

	if (right_spacer) {
		buttons_hbox->remove_child(right_spacer);
		p_button->remove_meta("__right_spacer");
		right_spacer->queue_free();
	}
	buttons_hbox->remove_child(p_button);

	child_controls_changed();
	if (is_visible()) {
		_update_child_rects();
	}
}

void AcceptDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_ok_button"), &AcceptDialog::get_ok_button);
	ClassDB::bind_method(D_METHOD("get_label"), &AcceptDialog::get_label);
	ClassDB::bind_method(D_METHOD("set_hide_on_ok", "enabled"), &AcceptDialog::set_hide_on_ok);
	ClassDB::bind_method(D_METHOD("get_hide_on_ok"), &AcceptDialog::get_hide_on_ok);
	ClassDB::bind_method(D_METHOD("set_close_on_escape", "enabled"), &AcceptDialog::set_close_on_escape);
	ClassDB::bind_method(D_METHOD("get_close_on_escape"), &AcceptDialog::get_close_on_escape);
	ClassDB::bind_method(D_METHOD("add_button", "text", "right", "action"), &AcceptDialog::add_button, DEFVAL(false), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("add_cancel_button", "name"), &AcceptDialog::add_cancel_button);
	ClassDB::bind_method(D_METHOD("remove_button", "button"), &AcceptDialog::remove_button);
	ClassDB::bind_method(D_METHOD("register_text_enter", "line_edit"), &AcceptDialog::register_text_enter);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &AcceptDialog::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &AcceptDialog::get_text);
	ClassDB::bind_method(D_METHOD("set_autowrap", "autowrap"), &AcceptDialog::set_autowrap);
	ClassDB::bind_method(D_METHOD("has_autowrap"), &AcceptDialog::has_autowrap);
	ClassDB::bind_method(D_METHOD("set_ok_button_text", "text"), &AcceptDialog::set_ok_button_text);
	ClassDB::bind_method(D_METHOD("get_ok_button_text"), &AcceptDialog::get_ok_button_text);

	ADD_SIGNAL(MethodInfo("confirmed"));
	ADD_SIGNAL(MethodInfo("canceled"));
	ADD_SIGNAL(MethodInfo("custom_action", PropertyInfo(Variant::STRING_NAME, "action")));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ok_button_text"), "set_ok_button_text", "get_ok_button_text");

	ADD_GROUP("Dialog", "dialog_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "dialog_text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dialog_hide_on_ok"), "set_hide_on_ok", "get_hide_on_ok");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dialog_close_on_escape"), "set_close_on_escape", "get_close_on_escape");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dialog_autowrap"), "set_autowrap", "has_autowrap");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, AcceptDialog, panel_style, "panel");
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, AcceptDialog, buttons_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, AcceptDialog, buttons_min_width);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, AcceptDialog, buttons_min_height);
}

bool AcceptDialog::swap_cancel_ok = false;
void AcceptDialog::set_swap_cancel_ok(bool p_swap) {
	swap_cancel_ok = p_swap;
}

AcceptDialog::AcceptDialog() {
	set_wrap_controls(true);
	set_visible(false);
	set_transient(true);
	set_exclusive(true);
	set_clamp_to_embedder(true);
	set_keep_title_visible(true);

	bg_panel = memnew(Panel);
	add_child(bg_panel, false, INTERNAL_MODE_FRONT);

	buttons_hbox = memnew(HBoxContainer);

	message_label = memnew(Label);
	message_label->set_anchor(SIDE_RIGHT, Control::ANCHOR_END);
	message_label->set_anchor(SIDE_BOTTOM, Control::ANCHOR_END);
	add_child(message_label, false, INTERNAL_MODE_FRONT);

	add_child(buttons_hbox, false, INTERNAL_MODE_FRONT);

	buttons_hbox->add_spacer();
	ok_button = memnew(Button);
	ok_button->set_text(ETR("OK"));
	buttons_hbox->add_child(ok_button);
	buttons_hbox->add_spacer();

	ok_button->connect(SceneStringName(pressed), callable_mp(this, &AcceptDialog::_ok_pressed));

	set_title(ETR("Alert!"));
}

AcceptDialog::~AcceptDialog() {
}

// ConfirmationDialog

void ConfirmationDialog::set_cancel_button_text(String p_cancel_button_text) {
	cancel->set_text(p_cancel_button_text);
}

String ConfirmationDialog::get_cancel_button_text() const {
	return cancel->get_text();
}

void ConfirmationDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cancel_button"), &ConfirmationDialog::get_cancel_button);
	ClassDB::bind_method(D_METHOD("set_cancel_button_text", "text"), &ConfirmationDialog::set_cancel_button_text);
	ClassDB::bind_method(D_METHOD("get_cancel_button_text"), &ConfirmationDialog::get_cancel_button_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cancel_button_text"), "set_cancel_button_text", "get_cancel_button_text");
}

Button *ConfirmationDialog::get_cancel_button() {
	return cancel;
}

ConfirmationDialog::ConfirmationDialog() {
	set_title(ETR("Please Confirm..."));
	set_min_size(Size2(200, 70));

	cancel = add_cancel_button();
}
