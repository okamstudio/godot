/**************************************************************************/
/*  openxr_action_map_editor.h                                            */
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

#include "../action_map/openxr_action_map.h"
#include "openxr_action_set_editor.h"
#include "openxr_interaction_profile_editor.h"
#include "openxr_select_interaction_profile_dialog.h"

#include "core/templates/hash_map.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/tab_container.h"

class OpenXRActionMapEditor : public VBoxContainer {
	GDCLASS(OpenXRActionMapEditor, VBoxContainer);

private:
	static HashMap<String, String> interaction_profile_editors; // interaction profile path, interaction profile editor
	static HashMap<String, String> binding_modifier_editors; // binding modifier class, binding modifiers editor

	EditorUndoRedoManager *undo_redo;
	String edited_path;
	Ref<OpenXRActionMap> action_map;

	HBoxContainer *top_hb = nullptr;
	Label *header_label = nullptr;
	Button *add_action_set = nullptr;
	Button *add_interaction_profile = nullptr;
	Button *load = nullptr;
	Button *save_as = nullptr;
	Button *_default = nullptr;
	TabContainer *tabs = nullptr;
	ScrollContainer *actionsets_scroll = nullptr;
	VBoxContainer *actionsets_vb = nullptr;
	OpenXRSelectInteractionProfileDialog *select_interaction_profile_dialog = nullptr;

	OpenXRActionSetEditor *_add_action_set_editor(Ref<OpenXRActionSet> p_action_set);
	void _create_action_sets();
	OpenXRInteractionProfileEditorBase *_add_interaction_profile_editor(Ref<OpenXRInteractionProfile> p_interaction_profile);
	void _create_interaction_profiles();

	OpenXRActionSetEditor *_add_action_set(String p_name);
	void _remove_action_set(String p_name);

	void _on_add_action_set();
	void _set_focus_on_action_set(OpenXRActionSetEditor *p_action_set_editor);
	void _on_remove_action_set(Object *p_action_set_editor);
	void _on_action_removed(Ref<OpenXRAction> p_action);

	void _on_add_interaction_profile();
	void _on_interaction_profile_selected(const String p_path);

	void _load_action_map(const String p_path, bool p_create_new_if_missing = false);
	void _on_save_action_map();
	void _on_reset_to_default_layout();

	void _on_tabs_tab_changed(int p_tab);
	void _on_tab_button_pressed(int p_tab);

protected:
	static void _bind_methods();
	void _notification(int p_what);

	void _clear_action_map();

	// used for undo/redo
	void _do_add_action_set_editor(OpenXRActionSetEditor *p_action_set_editor);
	void _do_remove_action_set_editor(OpenXRActionSetEditor *p_action_set_editor);
	void _do_add_interaction_profile_editor(OpenXRInteractionProfileEditorBase *p_interaction_profile_editor);
	void _do_remove_interaction_profile_editor(OpenXRInteractionProfileEditorBase *p_interaction_profile_editor);

public:
	static void register_interaction_profile_editor(const String &p_for_path, const String &p_editor_class);
	static void register_binding_modifier_editor(const String &p_binding_modifier_class, const String &p_editor_class);
	static String get_binding_modifier_editor_class(const String &p_binding_modifier_class);

	void open_action_map(String p_path);

	OpenXRActionMapEditor();
	~OpenXRActionMapEditor();
};
