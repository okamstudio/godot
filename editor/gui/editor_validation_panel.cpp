/**************************************************************************/
/*  editor_validation_panel.cpp                                           */
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

#include "editor_validation_panel.h"

#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"

void EditorValidationPanel::_update() {
	for (const KeyValue<int, String> &E : valid_messages) {
		set_message(E.key, E.value, MSG_OK);
	}

	valid = true;
	update_callback.callv(Array());

	if (accept_button) {
		accept_button->set_disabled(!valid);
	}
	pending_update = false;
}

void EditorValidationPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			theme_cache.valid_color = get_theme_color(SNAME("success_color"), EditorStringName(Editor));
			theme_cache.warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
			theme_cache.error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
		} break;
	}
}

void EditorValidationPanel::add_line(int p_id, const String &p_valid_message) {
	ERR_FAIL_COND(valid_messages.has(p_id));

	Label *label = memnew(Label);
	label->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	message_container->add_child(label);

	valid_messages[p_id] = p_valid_message;
	labels[p_id] = label;
}

void EditorValidationPanel::set_accept_button(Button *p_button) {
	accept_button = p_button;
}

void EditorValidationPanel::set_update_callback(const Callable &p_callback) {
	update_callback = p_callback;
}

void EditorValidationPanel::update() {
	ERR_FAIL_COND(!update_callback.is_valid());

	if (pending_update) {
		return;
	}
	pending_update = true;
	callable_mp(this, &EditorValidationPanel::_update).call_deferred();
}

void EditorValidationPanel::set_message(int p_id, const String &p_text, MessageType p_type, bool p_auto_prefix) {
	ERR_FAIL_COND(!valid_messages.has(p_id));

	Label *label = labels[p_id];
	if (p_text.is_empty()) {
		label->hide();
		return;
	}
	label->show();

	if (p_auto_prefix) {
		label->set_text(String(U"•  ") + p_text);
	} else {
		label->set_text(p_text);
	}

	switch (p_type) {
		case MSG_OK:
			label->add_theme_color_override(SceneStringName(font_color), theme_cache.valid_color);
			break;
		case MSG_WARNING:
			label->add_theme_color_override(SceneStringName(font_color), theme_cache.warning_color);
			break;
		case MSG_ERROR:
			label->add_theme_color_override(SceneStringName(font_color), theme_cache.error_color);
			valid = false;
			break;
		case MSG_INFO:
			label->remove_theme_color_override(SceneStringName(font_color));
			break;
	}
}

int EditorValidationPanel::get_message_count() const {
	return valid_messages.size();
}

bool EditorValidationPanel::is_valid() const {
	return valid;
}

EditorValidationPanel::EditorValidationPanel() {
	set_v_size_flags(SIZE_EXPAND_FILL);

	message_container = memnew(VBoxContainer);
	add_child(message_container);
}
