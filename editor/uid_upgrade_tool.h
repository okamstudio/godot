/**************************************************************************/
/*  uid_upgrade_tool.h                                                    */
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

class UIDUpgradeTool : public Object {
	GDCLASS(UIDUpgradeTool, Object);

	inline static UIDUpgradeTool *singleton = nullptr;

	static constexpr const char *UPGRADE_FINISHED = "upgrade_finished";
	static constexpr const char *META_RESAVE_PATHS = "resave_paths";

	void _add_files(EditorFileSystemDirectory *p_dir, Vector<String> &r_resave_paths);

protected:
	static void _bind_methods();

public:
	static constexpr const char *META_UID_UPGRADE_TOOL = "uid_upgrade_tool";
	static constexpr const char *META_RUN_ON_RESTART = "run_on_restart";

	static UIDUpgradeTool *get_singleton() { return singleton; }

	void prepare_upgrade();
	void begin_upgrade();
	void finish_upgrade();

	UIDUpgradeTool();
	~UIDUpgradeTool();
};

class UIDUpgradeDialog : public ConfirmationDialog {
	GDCLASS(UIDUpgradeDialog, ConfirmationDialog);

	static constexpr const char *UID_UPGRADE_LEARN_MORE = "uid_upgrade_learn_more";

	Button *learn_more_button = nullptr;

protected:
	void _on_custom_action(const String &p_action);
	void _notification(int p_what);

public:
	void popup_on_demand();

	UIDUpgradeDialog();
};
