/**************************************************************************/
/*  project_settings_gdextension.h                                        */
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

#ifndef PROJECT_SETTINGS_GDEXTENSION_H
#define PROJECT_SETTINGS_GDEXTENSION_H

#include "scene/gui/box_container.h"

class GDExtensionCreateDialog;
class GDExtensionEditDialog;
class Tree;

class ProjectSettingsGDExtension : public VBoxContainer {
	GDCLASS(ProjectSettingsGDExtension, VBoxContainer);

	enum {
		COLUMN_PADDING_LEFT,
		COLUMN_PATH,
		COLUMN_EDIT,
		COLUMN_PADDING_RIGHT,
		COLUMN_MAX,
	};

	GDExtensionCreateDialog *create_dialog = nullptr;
	GDExtensionEditDialog *config_dialog = nullptr;
	Tree *extension_list = nullptr;

	void _on_create_gdextension_pressed();
	void _on_gdextension_created();
	void _cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _update_extension_tree();

protected:
	void _notification(int p_what);

public:
	ProjectSettingsGDExtension();
};

#endif // PROJECT_SETTINGS_GDEXTENSION_H
