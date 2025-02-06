/**************************************************************************/
/*  history_dock.h                                                        */
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

#include "scene/gui/box_container.h"

class CheckBox;
class ConfigFile;
class ItemList;
class EditorUndoRedoManager;

class HistoryDock : public VBoxContainer {
	GDCLASS(HistoryDock, VBoxContainer);

	EditorUndoRedoManager *ur_manager;
	ItemList *action_list = nullptr;

	CheckBox *current_scene_checkbox = nullptr;
	CheckBox *global_history_checkbox = nullptr;

	bool need_refresh = true;
	int current_version = 0;

	void on_history_changed();
	void refresh_history();
	void on_version_changed();
	void refresh_version();

	void _save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const;
	void _load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void seek_history(int p_index);

	HistoryDock();
};
