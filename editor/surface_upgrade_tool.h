/**************************************************************************/
/*  surface_upgrade_tool.h                                                */
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

class EditorFileSystemDirectory;

class SurfaceUpgradeTool : public Object {
	GDCLASS(SurfaceUpgradeTool, Object);

	static SurfaceUpgradeTool *singleton;

	Mutex mutex;

	bool show_requested = false;
	bool updating = false;

	static void _try_show_popup();
	void _show_popup();

	void _add_files(EditorFileSystemDirectory *p_dir, Vector<String> &r_reimport_paths, Vector<String> &r_resave_paths);

protected:
	static void _bind_methods();

public:
	static SurfaceUpgradeTool *get_singleton() { return singleton; }

	bool is_show_requested() const { return show_requested; }
	void show_popup() { _show_popup(); }

	void prepare_upgrade();
	void begin_upgrade();
	void finish_upgrade();

	SurfaceUpgradeTool();
	~SurfaceUpgradeTool();
};

class SurfaceUpgradeDialog : public ConfirmationDialog {
	GDCLASS(SurfaceUpgradeDialog, ConfirmationDialog);

protected:
	void _notification(int p_what);

public:
	void popup_on_demand();

	SurfaceUpgradeDialog();
};
