/**************************************************************************/
/*  localization_editor.h                                                 */
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

#include "editor/editor_locale_dialog.h"
#include "scene/gui/check_box.h"
#include "scene/gui/tree.h"

class EditorFileDialog;
class FileSystemDock;

class LocalizationEditor : public VBoxContainer {
	GDCLASS(LocalizationEditor, VBoxContainer);

	Tree *translation_list = nullptr;

	EditorLocaleDialog *locale_select = nullptr;
	EditorFileDialog *translation_file_open = nullptr;

	Button *translation_res_option_add_button = nullptr;
	EditorFileDialog *translation_res_file_open_dialog = nullptr;
	EditorFileDialog *translation_res_option_file_open_dialog = nullptr;
	Tree *translation_remap = nullptr;
	Tree *translation_remap_options = nullptr;

	Tree *translation_pot_list = nullptr;
	CheckBox *translation_pot_add_builtin = nullptr;
	EditorFileDialog *pot_file_open_dialog = nullptr;
	EditorFileDialog *pot_generate_dialog = nullptr;
	Button *pot_generate_button = nullptr;

	bool updating_translations = false;
	String localization_changed;

	void _translation_file_open();
	void _translation_add(const PackedStringArray &p_paths);
	void _translation_delete(Object *p_item, int p_column, int p_button, MouseButton p_mouse_button);

	void _translation_res_file_open();
	void _translation_res_add(const PackedStringArray &p_paths);
	void _translation_res_delete(Object *p_item, int p_column, int p_button, MouseButton p_mouse_button);
	void _translation_res_select();
	void _translation_res_option_file_open();
	void _translation_res_option_add(const PackedStringArray &p_paths);
	void _translation_res_option_changed();
	void _translation_res_option_delete(Object *p_item, int p_column, int p_button, MouseButton p_mouse_button);
	void _translation_res_option_popup(bool p_arrow_clicked);
	void _translation_res_option_selected(const String &p_locale);

	void _pot_add(const PackedStringArray &p_paths);
	void _pot_delete(Object *p_item, int p_column, int p_button, MouseButton p_mouse_button);
	void _pot_file_open();
	void _pot_generate_open();
	void _pot_add_builtin_toggled();
	void _pot_generate(const String &p_file);
	void _update_pot_file_extensions();

	void _filesystem_files_moved(const String &p_old_file, const String &p_new_file);
	void _filesystem_file_removed(const String &p_file);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void add_translation(const String &p_translation);
	void update_translations();
	void connect_filesystem_dock_signals(FileSystemDock *p_fs_dock);

	LocalizationEditor();
};
