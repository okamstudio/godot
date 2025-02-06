/**************************************************************************/
/*  editor_validation_panel.h                                             */
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

#include "scene/gui/panel_container.h"

class Button;
class Label;
class VBoxContainer;

class EditorValidationPanel : public PanelContainer {
	GDCLASS(EditorValidationPanel, PanelContainer);

public:
	enum MessageType {
		MSG_OK,
		MSG_WARNING,
		MSG_ERROR,
		MSG_INFO,
	};

	static const int MSG_ID_DEFAULT = 0; // Avoids hard-coding ID in dialogs with single-line validation.

private:
	VBoxContainer *message_container = nullptr;

	HashMap<int, String> valid_messages;
	HashMap<int, Label *> labels;

	bool valid = false;
	bool pending_update = false;

	struct ThemeCache {
		Color valid_color;
		Color warning_color;
		Color error_color;
	} theme_cache;

	void _update();

	Callable update_callback;
	Button *accept_button = nullptr;

protected:
	void _notification(int p_what);

public:
	void add_line(int p_id, const String &p_valid_message = "");
	void set_accept_button(Button *p_button);
	void set_update_callback(const Callable &p_callback);

	void update();
	void set_message(int p_id, const String &p_text, MessageType p_type, bool p_auto_prefix = true);
	bool is_valid() const;

	EditorValidationPanel();
};
