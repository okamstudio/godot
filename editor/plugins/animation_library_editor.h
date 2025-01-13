/**************************************************************************/
/*  animation_library_editor.h                                            */
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

#ifndef ANIMATION_LIBRARY_EDITOR_H
#define ANIMATION_LIBRARY_EDITOR_H

#include "core/io/config_file.h"
#include "core/templates/vector.h"
#include "editor/animation_track_editor.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/animation/animation_mixer.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class AnimationMixer;
class EditorFileDialog;

class AnimationLibraryEditor : public AcceptDialog {
	GDCLASS(AnimationLibraryEditor, AcceptDialog)

	enum {
		LIB_BUTTON_ADD,
		LIB_BUTTON_LOAD,
		LIB_BUTTON_PASTE,
		LIB_BUTTON_FILE,
		LIB_BUTTON_DELETE,
	};
	enum {
		ANIM_BUTTON_COPY,
		ANIM_BUTTON_FILE,
		ANIM_BUTTON_DELETE,
	};

	enum FileMenuAction {
		FILE_MENU_SAVE_LIBRARY,
		FILE_MENU_SAVE_AS_LIBRARY,
		FILE_MENU_MAKE_LIBRARY_UNIQUE,
		FILE_MENU_EDIT_LIBRARY,

		FILE_MENU_SAVE_ANIMATION,
		FILE_MENU_SAVE_AS_ANIMATION,
		FILE_MENU_MAKE_ANIMATION_UNIQUE,
		FILE_MENU_EDIT_ANIMATION,
	};

	enum FileDialogAction {
		FILE_DIALOG_ACTION_OPEN_LIBRARY,
		FILE_DIALOG_ACTION_SAVE_LIBRARY,
		FILE_DIALOG_ACTION_OPEN_ANIMATION,
		FILE_DIALOG_ACTION_SAVE_ANIMATION,
	};

	FileDialogAction file_dialog_action = FILE_DIALOG_ACTION_OPEN_ANIMATION;

	StringName file_dialog_animation;
	StringName file_dialog_library;

	Button *new_library_button = nullptr;
	Button *load_library_button = nullptr;

	AcceptDialog *error_dialog = nullptr;
	bool adding_animation = false;
	StringName adding_animation_to_library;
	EditorFileDialog *file_dialog = nullptr;
	ConfirmationDialog *add_library_dialog = nullptr;
	LineEdit *add_library_name = nullptr;
	Label *add_library_validate = nullptr;
	PopupMenu *file_popup = nullptr;

	Tree *tree = nullptr;

	AnimationMixer *mixer = nullptr;

	void _add_library();
	void _add_library_validate(const String &p_name);
	void _add_library_confirm();
	void _load_library();
	void _load_file(const String &p_path);
	void _load_files(const PackedStringArray &p_paths);

	void _save_mixer_lib_folding(TreeItem *p_item);
	Vector<uint64_t> _load_mixer_libs_folding();
	void _load_config_libs_folding(Vector<uint64_t> &p_lib_ids, ConfigFile *p_config, String p_section);
	String _get_mixer_signature() const;

	void _item_renamed();
	void _button_pressed(TreeItem *p_item, int p_column, int p_id, MouseButton p_button);

	void _file_popup_selected(int p_id);

	bool updating = false;

protected:
	void _notification(int p_what);
	void _update_editor(Object *p_mixer);
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();

public:
	void set_animation_mixer(Object *p_mixer);
	void show_dialog();
	void update_tree();
	AnimationLibraryEditor();
};

#endif // ANIMATION_LIBRARY_EDITOR_H
