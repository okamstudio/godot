/**************************************************************************/
/*  openxr_interaction_profile_editor.cpp                                 */
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

#include "openxr_interaction_profile_editor.h"

#include "editor/editor_string_names.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"

///////////////////////////////////////////////////////////////////////////
// Interaction profile editor base

void OpenXRInteractionProfileEditorBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_add_binding", "action", "path"), &OpenXRInteractionProfileEditorBase::_add_binding);
	ClassDB::bind_method(D_METHOD("_remove_binding", "action", "path"), &OpenXRInteractionProfileEditorBase::_remove_binding);
}

void OpenXRInteractionProfileEditorBase::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_interaction_profile();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_theme_changed();
		} break;
	}
}

void OpenXRInteractionProfileEditorBase::_do_update_interaction_profile() {
	if (!is_dirty) {
		is_dirty = true;
		callable_mp(this, &OpenXRInteractionProfileEditorBase::_update_interaction_profile).call_deferred();
	}
}

void OpenXRInteractionProfileEditorBase::_add_binding(const String p_action, const String p_path) {
	ERR_FAIL_COND(action_map.is_null());
	ERR_FAIL_COND(interaction_profile.is_null());

	Ref<OpenXRAction> action = action_map->get_action(p_action);
	ERR_FAIL_COND(action.is_null());

	Ref<OpenXRIPBinding> binding = interaction_profile->find_binding(action, p_path);
	if (binding.is_null()) {
		// create a new binding
		binding.instantiate();
		binding->set_action(action);
		binding->set_binding_path(p_path);

		// add it to our interaction profile
		interaction_profile->add_binding(binding);
		interaction_profile->set_edited(true);

		binding->set_edited(true);
	}

	// Update our toplevel paths
	action->set_toplevel_paths(action_map->get_top_level_paths(action));

	_do_update_interaction_profile();
}

void OpenXRInteractionProfileEditorBase::_remove_binding(const String p_action, const String p_path) {
	ERR_FAIL_COND(action_map.is_null());
	ERR_FAIL_COND(interaction_profile.is_null());

	Ref<OpenXRAction> action = action_map->get_action(p_action);
	ERR_FAIL_COND(action.is_null());

	Ref<OpenXRIPBinding> binding = interaction_profile->find_binding(action, p_path);
	if (binding.is_valid()) {
		interaction_profile->remove_binding(binding);
		interaction_profile->set_edited(true);

		// Update our toplevel paths
		action->set_toplevel_paths(action_map->get_top_level_paths(action));

		_do_update_interaction_profile();
	}
}

void OpenXRInteractionProfileEditorBase::remove_all_bindings_for_action(Ref<OpenXRAction> p_action) {
	Vector<Ref<OpenXRIPBinding>> bindings = interaction_profile->get_bindings_for_action(p_action);
	if (bindings.size() > 0) {
		String action_name = p_action->get_name_with_set();

		// for our undo/redo we process all paths
		undo_redo->create_action(TTR("Remove action from interaction profile"));
		for (const Ref<OpenXRIPBinding> &binding : bindings) {
			undo_redo->add_do_method(this, "_remove_binding", action_name, binding->get_binding_path());
			undo_redo->add_undo_method(this, "_add_binding", action_name, binding->get_binding_path());
		}
		undo_redo->commit_action(false);

		// but we take a shortcut here :)
		for (const Ref<OpenXRIPBinding> &binding : bindings) {
			interaction_profile->remove_binding(binding);
		}
		interaction_profile->set_edited(true);

		// Update our toplevel paths
		p_action->set_toplevel_paths(action_map->get_top_level_paths(p_action));

		_do_update_interaction_profile();
	}
}

OpenXRInteractionProfileEditorBase::OpenXRInteractionProfileEditorBase(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRInteractionProfile> p_interaction_profile) {
	undo_redo = EditorUndoRedoManager::get_singleton();

	action_map = p_action_map;
	interaction_profile = p_interaction_profile;
	String profile_path = interaction_profile->get_interaction_profile_path();
	String profile_name = profile_path;

	profile_def = OpenXRInteractionProfileMetadata::get_singleton()->get_profile(profile_path);
	if (profile_def != nullptr) {
		profile_name = profile_def->display_name;
	}

	set_name(profile_name);
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);

	// Make sure it is updated when it enters the tree...
	is_dirty = true;
}

///////////////////////////////////////////////////////////////////////////
// Default interaction profile editor

void OpenXRInteractionProfileEditor::select_action_for(const String p_io_path) {
	selecting_for_io_path = p_io_path;
	select_action_dialog->open();
}

void OpenXRInteractionProfileEditor::action_selected(const String p_action) {
	undo_redo->create_action(TTR("Add binding"));
	undo_redo->add_do_method(this, "_add_binding", p_action, selecting_for_io_path);
	undo_redo->add_undo_method(this, "_remove_binding", p_action, selecting_for_io_path);
	undo_redo->commit_action(true);

	selecting_for_io_path = "";
}

void OpenXRInteractionProfileEditor::_on_remove_pressed(const String p_action, const String p_for_io_path) {
	undo_redo->create_action(TTR("Remove binding"));
	undo_redo->add_do_method(this, "_remove_binding", p_action, p_for_io_path);
	undo_redo->add_undo_method(this, "_add_binding", p_action, p_for_io_path);
	undo_redo->commit_action(true);
}

void OpenXRInteractionProfileEditor::_add_io_path(VBoxContainer *p_container, const OpenXRInteractionProfileMetadata::IOPath *p_io_path) {
	HBoxContainer *path_hb = memnew(HBoxContainer);
	path_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_container->add_child(path_hb);

	Label *path_label = memnew(Label);
	path_label->set_text(p_io_path->display_name);
	path_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	path_hb->add_child(path_label);

	Label *type_label = memnew(Label);
	switch (p_io_path->action_type) {
		case OpenXRAction::OPENXR_ACTION_BOOL: {
			type_label->set_text(TTR("Boolean"));
		} break;
		case OpenXRAction::OPENXR_ACTION_FLOAT: {
			type_label->set_text(TTR("Float"));
		} break;
		case OpenXRAction::OPENXR_ACTION_VECTOR2: {
			type_label->set_text(TTR("Vector2"));
		} break;
		case OpenXRAction::OPENXR_ACTION_POSE: {
			type_label->set_text(TTR("Pose"));
		} break;
		case OpenXRAction::OPENXR_ACTION_HAPTIC: {
			type_label->set_text(TTR("Haptic"));
		} break;
		default: {
			type_label->set_text(TTR("Unknown"));
		} break;
	}
	type_label->set_custom_minimum_size(Size2(50.0, 0.0));
	path_hb->add_child(type_label);

	Button *path_add = memnew(Button);
	path_add->set_icon(get_theme_icon(SNAME("Add"), EditorStringName(EditorIcons)));
	path_add->set_flat(true);
	path_add->connect(SceneStringName(pressed), callable_mp(this, &OpenXRInteractionProfileEditor::select_action_for).bind(String(p_io_path->openxr_path)));
	path_hb->add_child(path_add);

	if (interaction_profile.is_valid()) {
		String io_path = String(p_io_path->openxr_path);
		Array bindings = interaction_profile->get_bindings();
		for (Ref<OpenXRIPBinding> binding : bindings) {
			if (binding->get_binding_path() == io_path) {
				Ref<OpenXRAction> action = binding->get_action();

				HBoxContainer *action_hb = memnew(HBoxContainer);
				action_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				p_container->add_child(action_hb);

				Control *indent_node = memnew(Control);
				indent_node->set_custom_minimum_size(Size2(10.0, 0.0));
				action_hb->add_child(indent_node);

				Label *action_label = memnew(Label);
				action_label->set_text(action->get_name_with_set() + ": " + action->get_localized_name());
				action_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				action_hb->add_child(action_label);

				Button *action_rem = memnew(Button);
				action_rem->set_flat(true);
				action_rem->set_icon(get_theme_icon(SNAME("Remove"), EditorStringName(EditorIcons)));
				action_rem->connect(SceneStringName(pressed), callable_mp((OpenXRInteractionProfileEditor *)this, &OpenXRInteractionProfileEditor::_on_remove_pressed).bind(action->get_name_with_set(), String(p_io_path->openxr_path)));
				action_hb->add_child(action_rem);
			}
		}
	}
}

void OpenXRInteractionProfileEditor::_update_interaction_profile() {
	ERR_FAIL_NULL(profile_def);

	if (!is_dirty) {
		// no need to update
		return;
	}

	// out with the old...
	while (main_hb->get_child_count() > 0) {
		memdelete(main_hb->get_child(0));
	}

	// in with the new...

	// Determine toplevel paths
	Vector<String> top_level_paths;
	for (int i = 0; i < profile_def->io_paths.size(); i++) {
		const OpenXRInteractionProfileMetadata::IOPath *io_path = &profile_def->io_paths[i];

		if (!top_level_paths.has(io_path->top_level_path)) {
			top_level_paths.push_back(io_path->top_level_path);
		}
	}

	for (int i = 0; i < top_level_paths.size(); i++) {
		PanelContainer *panel = memnew(PanelContainer);
		panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		main_hb->add_child(panel);
		panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("TabContainer")));

		VBoxContainer *container = memnew(VBoxContainer);
		panel->add_child(container);

		Label *label = memnew(Label);
		label->set_text(OpenXRInteractionProfileMetadata::get_singleton()->get_top_level_name(top_level_paths[i]));
		container->add_child(label);

		for (int j = 0; j < profile_def->io_paths.size(); j++) {
			const OpenXRInteractionProfileMetadata::IOPath *io_path = &profile_def->io_paths[j];
			if (io_path->top_level_path == top_level_paths[i]) {
				_add_io_path(container, io_path);
			}
		}
	}

	// and we've updated it...
	is_dirty = false;
}

void OpenXRInteractionProfileEditor::_theme_changed() {
	for (int i = 0; i < main_hb->get_child_count(); i++) {
		Control *panel = Object::cast_to<Control>(main_hb->get_child(i));
		if (panel) {
			panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("TabContainer")));
		}
	}
}

OpenXRInteractionProfileEditor::OpenXRInteractionProfileEditor(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRInteractionProfile> p_interaction_profile) :
		OpenXRInteractionProfileEditorBase(p_action_map, p_interaction_profile) {
	main_hb = memnew(HBoxContainer);
	add_child(main_hb);

	select_action_dialog = memnew(OpenXRSelectActionDialog(p_action_map));
	select_action_dialog->connect("action_selected", callable_mp(this, &OpenXRInteractionProfileEditor::action_selected));
	add_child(select_action_dialog);
}
