/**************************************************************************/
/*  plugin_config_dialog.h                                                */
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

#ifndef PLUGIN_CONFIG_DIALOG_H
#define PLUGIN_CONFIG_DIALOG_H

#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"

class ConfigFile;
class EditorValidationPanel;

class PluginConfigDialog : public ConfirmationDialog {
	GDCLASS(PluginConfigDialog, ConfirmationDialog);

	enum {
		MSG_ID_PLUGIN,
		MSG_ID_SUBFOLDER,
		MSG_ID_SCRIPT,
		MSG_ID_ACTIVE,
	};

	LineEdit *name_edit = nullptr;
	LineEdit *subfolder_edit = nullptr;
	TextEdit *desc_edit = nullptr;
	LineEdit *author_edit = nullptr;
	LineEdit *version_edit = nullptr;
	OptionButton *script_option_edit = nullptr;
	LineEdit *script_edit = nullptr;
	CheckBox *active_edit = nullptr;

	LocalVector<Control *> plugin_edit_hidden_controls;

	EditorValidationPanel *validation_panel = nullptr;

	bool _edit_mode = false;

	void _clear_fields();
	void _on_confirmed();
	void _on_canceled();
	void _on_required_text_changed();
	void _create_script_for_plugin(const String &p_plugin_path, Ref<ConfigFile> p_config_file, int p_script_lang_index);
	String _get_subfolder();

	static String _to_absolute_plugin_path(const String &p_plugin_name);

protected:
	virtual void _notification(int p_what);
	static void _bind_methods();

public:
	void config(const String &p_config_path);

	PluginConfigDialog();
	~PluginConfigDialog();
};

#endif // PLUGIN_CONFIG_DIALOG_H
