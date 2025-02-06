/**************************************************************************/
/*  fbx_importer_manager.h                                                */
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

#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"

class FBXImporterManager : public ConfirmationDialog {
	GDCLASS(FBXImporterManager, ConfirmationDialog)

	bool is_importing = false;

	Label *message = nullptr;
	LineEdit *fbx_path = nullptr;
	Button *fbx_path_browse = nullptr;
	EditorFileDialog *browse_dialog = nullptr;
	Label *path_status = nullptr;

	void _validate_path(const String &p_path);
	void _select_file(const String &p_path);
	void _path_confirmed();
	void _cancel_setup();
	void _browse_install();
	void _link_clicked();

	static FBXImporterManager *singleton;

protected:
	void _notification(int p_what);

public:
	static FBXImporterManager *get_singleton() { return singleton; }

	void show_dialog(bool p_exclusive = false);

	FBXImporterManager();
};
