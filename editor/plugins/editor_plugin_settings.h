/**************************************************************************/
/*  editor_plugin_settings.h                                              */
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

#include "editor/plugins/plugin_config_dialog.h"

class TextureRect;
class Tree;

class EditorPluginSettings : public VBoxContainer {
	GDCLASS(EditorPluginSettings, VBoxContainer);

	enum {
		BUTTON_PLUGIN_EDIT
	};

	enum {
		COLUMN_PADDING_LEFT,
		COLUMN_STATUS,
		COLUMN_NAME,
		COLUMN_VERSION,
		COLUMN_AUTHOR,
		COLUMN_EDIT,
		COLUMN_PADDING_RIGHT,
		COLUMN_MAX,
	};

	PluginConfigDialog *plugin_config_dialog = nullptr;
	TextureRect *recovery_mode_icon = nullptr;
	Tree *plugin_list = nullptr;
	bool updating = false;

	void _plugin_activity_changed();
	void _create_clicked();
	void _cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);

	static Vector<String> _get_plugins(const String &p_dir);

protected:
	void _notification(int p_what);

public:
	void update_plugins();

	EditorPluginSettings();
};
