/**************************************************************************/
/*  directory_create_dialog.h                                             */
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

#include "scene/gui/dialogs.h"

class EditorValidationPanel;
class Label;
class LineEdit;

class DirectoryCreateDialog : public ConfirmationDialog {
	GDCLASS(DirectoryCreateDialog, ConfirmationDialog);

public:
	enum Mode {
		MODE_FILE,
		MODE_DIRECTORY,
	};

private:
	String base_dir;
	Callable accept_callback;
	int mode = MODE_FILE;

	Label *base_path_label = nullptr;
	LineEdit *dir_path = nullptr;
	EditorValidationPanel *validation_panel = nullptr;

	String _sanitize_input(const String &p_input) const;
	String _validate_path(const String &p_path) const;
	void _on_dir_path_changed();

protected:
	virtual void ok_pressed() override;
	virtual void _post_popup() override;

public:
	void config(const String &p_base_dir, const Callable &p_accept_callback, int p_mode, const String &p_title, const String &p_default_name = "");

	DirectoryCreateDialog();
};
