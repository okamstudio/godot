/**************************************************************************/
/*  editor_log.cpp                                                        */
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

#include "editor_log.h"

#include "core/os/keyboard.h"
#include "core/version.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "scene/gui/center_container.h"
#include "scene/resources/dynamic_font.h"

void EditorLog::_error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, ErrorHandlerType p_type) {
	EditorLog *self = (EditorLog *)p_self;
	if (self->current != Thread::get_caller_id()) {
		return;
	}

	String err_str;
	if (p_errorexp && p_errorexp[0]) {
		err_str = String::utf8(p_errorexp);
	} else {
		err_str = String::utf8(p_file) + ":" + itos(p_line) + " - " + String::utf8(p_error);
	}

	if (p_type == ERR_HANDLER_WARNING) {
		self->add_message(err_str, MSG_TYPE_WARNING);
	} else {
		self->add_message(err_str, MSG_TYPE_ERROR);
	}
}

void EditorLog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (log != nullptr) {
				Ref<DynamicFont> df_output_code = get_font("output_source", "EditorFonts");
				if (df_output_code.is_valid()) {
					log->add_font_override("normal_font", get_font("output_source", "EditorFonts"));
					log->add_color_override("selection_color", get_color("accent_color", "Editor") * Color(1, 1, 1, 0.4));
				}
			}

			theme_cache.error_color = get_color("error_color", "Editor");
			theme_cache.error_icon = get_icon("Error", "EditorIcons");
			theme_cache.warning_color = get_color("warning_color", "Editor");
			theme_cache.warning_icon = get_icon("Warning", "EditorIcons");
			theme_cache.message_color = get_color("font_color", "Editor") * Color(1, 1, 1, 0.6);
		} break;
	}
}

void EditorLog::_clear_request() {
	log->clear();
	tool_button->set_icon(Ref<Texture>());
}

void EditorLog::_copy_request() {
	String text = log->get_selected_text();

	if (text == "") {
		text = log->get_text();
	}

	if (text != "") {
		OS::get_singleton()->set_clipboard(text);
	}
}

void EditorLog::clear() {
	_clear_request();
}

void EditorLog::copy() {
	_copy_request();
}

void EditorLog::add_message(const String &p_msg, MessageType p_type) {
	bool restore = p_type != MSG_TYPE_STD;
	switch (p_type) {
		case MSG_TYPE_STD: {
		} break;
		case MSG_TYPE_ERROR: {
			log->push_color(theme_cache.error_color);
			Ref<Texture> icon = theme_cache.error_icon;
			log->add_image(icon);
			log->add_text(" ");
			tool_button->set_icon(icon);
		} break;
		case MSG_TYPE_WARNING: {
			log->push_color(theme_cache.warning_color);
			Ref<Texture> icon = theme_cache.warning_icon;
			log->add_image(icon);
			log->add_text(" ");
			tool_button->set_icon(icon);
		} break;
		case MSG_TYPE_EDITOR: {
			// Distinguish editor messages from messages printed by the project
			log->push_color(theme_cache.message_color);
		} break;
	}

	log->add_text(p_msg);
	log->add_newline();

	if (restore) {
		log->pop();
	}
}

void EditorLog::set_tool_button(ToolButton *p_tool_button) {
	tool_button = p_tool_button;
}

void EditorLog::_undo_redo_cbk(void *p_self, const String &p_name) {
	EditorLog *self = (EditorLog *)p_self;
	self->add_message(p_name, EditorLog::MSG_TYPE_EDITOR);
}

void EditorLog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_clear_request"), &EditorLog::_clear_request);
	ClassDB::bind_method(D_METHOD("_copy_request"), &EditorLog::_copy_request);
	ADD_SIGNAL(MethodInfo("clear_request"));
	ADD_SIGNAL(MethodInfo("copy_request"));
}

EditorLog::EditorLog() {
	VBoxContainer *vb = this;

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);
	title = memnew(Label);
	title->set_text(TTR("Output:"));
	title->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(title);

	copybutton = memnew(Button);
	hb->add_child(copybutton);
	copybutton->set_text(TTR("Copy"));
	copybutton->set_shortcut(ED_SHORTCUT("editor/copy_output", TTR("Copy Selection"), KEY_MASK_CMD | KEY_C));
	copybutton->connect("pressed", this, "_copy_request");

	clearbutton = memnew(Button);
	hb->add_child(clearbutton);
	clearbutton->set_text(TTR("Clear"));
	clearbutton->set_shortcut(ED_SHORTCUT("editor/clear_output", TTR("Clear Output"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_K));
	clearbutton->connect("pressed", this, "_clear_request");

	log = memnew(RichTextLabel);
	log->set_scroll_follow(true);
	log->set_selection_enabled(true);
	log->set_focus_mode(FOCUS_CLICK);
	log->set_custom_minimum_size(Size2(0, 180) * EDSCALE);
	log->set_v_size_flags(SIZE_EXPAND_FILL);
	log->set_h_size_flags(SIZE_EXPAND_FILL);
	log->set_deselect_on_focus_loss_enabled(false);
	vb->add_child(log);
	add_message(VERSION_FULL_NAME " (c) 2007-2022 Juan Linietsky, Ariel Manzur & Godot Contributors.");

	eh.errfunc = _error_handler;
	eh.userdata = this;
	add_error_handler(&eh);

	current = Thread::get_caller_id();

	add_constant_override("separation", get_constant("separation", "VBoxContainer"));

	EditorNode::get_undo_redo()->set_commit_notify_callback(_undo_redo_cbk, this);
}

void EditorLog::deinit() {
	remove_error_handler(&eh);
}

void EditorLog::set_scroll_follow(bool p_follow) {
	log->set_scroll_follow(p_follow);
}

EditorLog::~EditorLog() {
}
