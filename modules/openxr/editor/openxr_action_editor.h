/**************************************************************************/
/*  openxr_action_editor.h                                                */
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

#include "../action_map/openxr_action.h"

#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"

class OpenXRActionEditor : public HBoxContainer {
	GDCLASS(OpenXRActionEditor, HBoxContainer);

private:
	EditorUndoRedoManager *undo_redo;
	Ref<OpenXRAction> action;

	LineEdit *action_name = nullptr;
	LineEdit *action_localized_name = nullptr;
	OptionButton *action_type_button = nullptr;
	Button *rem_action = nullptr;

	void _theme_changed();
	void _on_action_name_changed(const String p_new_text);
	void _on_action_localized_name_changed(const String p_new_text);
	void _on_item_selected(int p_idx);
	void _on_remove_action();

protected:
	static void _bind_methods();
	void _notification(int p_what);

	// used for undo/redo
	void _do_set_name(const String p_new_text);
	void _do_set_localized_name(const String p_new_text);
	void _do_set_action_type(OpenXRAction::ActionType p_action_type);

public:
	Ref<OpenXRAction> get_action() { return action; }
	OpenXRActionEditor(Ref<OpenXRAction> p_action);
};
