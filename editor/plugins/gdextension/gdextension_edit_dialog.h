/**************************************************************************/
/*  gdextension_edit_dialog.h                                             */
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

#ifndef GDEXTENSION_EDIT_DIALOG_H
#define GDEXTENSION_EDIT_DIALOG_H

#include "scene/gui/dialogs.h"

class CheckBox;
class EditorPropertyDictionary;
class EditorValidationPanel;
class LineEdit;

class GDExtensionEditDialog : public ConfirmationDialog {
	GDCLASS(GDExtensionEditDialog, ConfirmationDialog);

	enum {
		MSG_ID_ENTRY_SYMBOL_NAME,
		MSG_ID_COMPAT_MIN_VERSION,
		MSG_ID_COMPAT_MAX_VERSION,
	};

	Label *gdextension_path = nullptr;
	LineEdit *entry_symbol_edit = nullptr;
	LineEdit *compat_max_version_edit = nullptr;
	LineEdit *compat_min_version_edit = nullptr;
	CheckBox *reloadable_checkbox = nullptr;
	EditorValidationPanel *validation_panel = nullptr;

	void _clear_fields();
	void _on_canceled();
	void _on_confirmed();
	void _on_required_text_changed();

protected:
	void _notification(int p_what);

public:
	void load_gdextension_config(const String &p_path);
	GDExtensionEditDialog();
};

#endif // GDEXTENSION_EDIT_DIALOG_H
