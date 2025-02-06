/**************************************************************************/
/*  openxr_interaction_profile_editor.h                                   */
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
#include "../action_map/openxr_interaction_profile.h"
#include "../action_map/openxr_interaction_profile_metadata.h"
#include "../editor/openxr_binding_modifiers_dialog.h"
#include "editor/editor_undo_redo_manager.h"
#include "openxr_select_action_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

class OpenXRInteractionProfileEditorBase : public HBoxContainer {
	GDCLASS(OpenXRInteractionProfileEditorBase, HBoxContainer);

private:
	OpenXRBindingModifiersDialog *binding_modifiers_dialog = nullptr;
	VBoxContainer *toolbar_vb = nullptr;
	Button *binding_modifiers_btn = nullptr;

	void _on_open_binding_modifiers();

protected:
	EditorUndoRedoManager *undo_redo;
	Ref<OpenXRInteractionProfile> interaction_profile;
	Ref<OpenXRActionMap> action_map;

	ScrollContainer *interaction_profile_sc = nullptr;

	bool is_dirty = false;

	static void _bind_methods();
	void _notification(int p_what);

	const OpenXRInteractionProfileMetadata::InteractionProfile *profile_def = nullptr;

public:
	String tooltip; // Tooltip text to show on tab

	Ref<OpenXRInteractionProfile> get_interaction_profile() { return interaction_profile; }

	virtual void _update_interaction_profile();
	virtual void _theme_changed();

	void _do_update_interaction_profile();
	void _add_binding(const String p_action, const String p_path);
	void _remove_binding(const String p_action, const String p_path);

	void remove_all_for_action_set(Ref<OpenXRActionSet> p_action_set);
	void remove_all_for_action(Ref<OpenXRAction> p_action);

	virtual void setup(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRInteractionProfile> p_interaction_profile);

	OpenXRInteractionProfileEditorBase();
};

class OpenXRInteractionProfileEditor : public OpenXRInteractionProfileEditorBase {
	GDCLASS(OpenXRInteractionProfileEditor, OpenXRInteractionProfileEditorBase);

private:
	String selecting_for_io_path;
	HBoxContainer *interaction_profile_hb = nullptr;

	OpenXRSelectActionDialog *select_action_dialog = nullptr;

	void _add_io_path(VBoxContainer *p_container, const OpenXRInteractionProfileMetadata::IOPath *p_io_path);

public:
	void select_action_for(const String p_io_path);
	void _on_action_selected(const String p_action);
	void _on_remove_pressed(const String p_action, const String p_for_io_path);

	virtual void _update_interaction_profile() override;
	virtual void _theme_changed() override;
	virtual void setup(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRInteractionProfile> p_interaction_profile) override;

	OpenXRInteractionProfileEditor();
};
