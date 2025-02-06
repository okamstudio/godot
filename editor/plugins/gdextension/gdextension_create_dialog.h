/**************************************************************************/
/*  gdextension_create_dialog.h                                           */
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

#ifndef GDEXTENSION_CREATE_DIALOG_H
#define GDEXTENSION_CREATE_DIALOG_H

#include "scene/gui/dialogs.h"

class CheckBox;
class EditorValidationPanel;
class GDExtensionCreatorPlugin;
class OptionButton;

class GDExtensionCreateDialog : public ConfirmationDialog {
	GDCLASS(GDExtensionCreateDialog, ConfirmationDialog);

	enum {
		MSG_ID_BASE_NAME,
		MSG_ID_LIBRARY_NAME,
		MSG_ID_PATH,
		MSG_ID_MAX, // Individual GDExtension creators may add more messages.
	};

	LineEdit *base_name_edit = nullptr;
	LineEdit *library_name_edit = nullptr;
	LineEdit *path_edit = nullptr;
	OptionButton *language_option = nullptr;
	CheckBox *compile_checkbox = nullptr;

	EditorValidationPanel *validation_panel = nullptr;
	Vector<Ref<GDExtensionCreatorPlugin>> plugin_creators;
	Vector<Vector2i> language_option_index_map;

	void _clear_fields();
	void _on_canceled();
	void _on_confirmed();
	void _on_required_text_changed();

	String _get_valid_base_name();
	String _get_valid_library_name(const String &p_valid_base_name);
	String _get_valid_path(const String &p_valid_base_name);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void load_plugin_creators(const Vector<Ref<GDExtensionCreatorPlugin>> &p_plugin_creators);
	GDExtensionCreateDialog();
};

#endif // GDEXTENSION_CREATE_DIALOG_H
