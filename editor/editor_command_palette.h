/**************************************************************************/
/*  editor_command_palette.h                                              */
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

#ifndef EDITOR_COMMAND_PALETTE_H
#define EDITOR_COMMAND_PALETTE_H

#include "core/input/shortcut.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class EditorCommandPalette : public ConfirmationDialog {
	GDCLASS(EditorCommandPalette, ConfirmationDialog);

	static EditorCommandPalette *singleton;
	LineEdit *command_search_box = nullptr;
	Tree *search_options = nullptr;

	struct Command {
		Callable callable;
		String name;
		Ref<Shortcut> shortcut;
		String shortcut_text;
		int last_used = 0; // Store time as int, because doubles have problems with text serialization.
	};

	struct CommandEntry {
		String key_name;
		String display_name;
		String shortcut_text;
		int last_used = 0;
		float score = 0;
	};

	struct CommandEntryComparator {
		_FORCE_INLINE_ bool operator()(const CommandEntry &A, const CommandEntry &B) const {
			return A.score > B.score;
		}
	};

	struct CommandHistoryComparator {
		_FORCE_INLINE_ bool operator()(const CommandEntry &A, const CommandEntry &B) const {
			if (A.last_used == B.last_used) {
				return A.display_name < B.display_name;
			} else {
				return A.last_used > B.last_used;
			}
		}
	};

	HashMap<String, Command> commands;
	HashMap<String, Pair<String, Ref<Shortcut>>> unregistered_shortcuts;

	void _update_command_search(const String &search_text);
	float _score_path(const String &p_search, const String &p_path);
	void _sbox_input(const Ref<InputEvent> &p_event);
	void _confirmed();
	void _add_command(String p_command_name, String p_key_name, Callable p_binded_action, String p_shortcut_text = "None");
	void _save_history() const;

	EditorCommandPalette();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void open_popup();
	void get_actions_list(List<String> *p_list) const;
	void add_command(String p_command_name, String p_key_name, Callable p_action, Vector<Variant> arguments, const Ref<Shortcut> &p_shortcut);
	void execute_command(const String &p_command_name);
	void register_shortcuts_as_command();
	Ref<Shortcut> add_shortcut_command(const String &p_command, const String &p_key, Ref<Shortcut> p_shortcut);
	void remove_command(String p_key_name);
	static EditorCommandPalette *get_singleton();
};

Ref<Shortcut> ED_SHORTCUT_AND_COMMAND(const String &p_path, const String &p_name, Key p_keycode = Key::NONE, String p_command = "");
Ref<Shortcut> ED_SHORTCUT_ARRAY_AND_COMMAND(const String &p_path, const String &p_name, const PackedInt32Array &p_keycodes, String p_command = "");

#endif // EDITOR_COMMAND_PALETTE_H
