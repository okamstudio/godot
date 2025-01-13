/**************************************************************************/
/*  openxr_binding_modifier_editor.cpp                                    */
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

#include "openxr_binding_modifier_editor.h"

#include "editor/editor_string_names.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorPropertyActionSet

void EditorPropertyActionSet::_set_read_only(bool p_read_only) {
	options->set_disabled(p_read_only);
}

void EditorPropertyActionSet::_option_selected(int p_which) {
	Ref<OpenXRActionSet> val = options->get_item_metadata(p_which);
	emit_changed(get_edited_property(), val);
}

void EditorPropertyActionSet::update_property() {
	Variant current = get_edited_property_value();
	if (current.get_type() == Variant::NIL) {
		options->select(-1);
		return;
	}

	Ref<OpenXRActionSet> which = current;
	for (int i = 0; i < options->get_item_count(); i++) {
		if (which == (Ref<OpenXRActionSet>)options->get_item_metadata(i)) {
			options->select(i);
			return;
		}
	}

	// Couldn't find it? deselect..
	options->select(-1);
}

void EditorPropertyActionSet::setup(const Ref<OpenXRActionMap> &p_action_map) {
	options->clear();

	Array action_sets = p_action_map->get_action_sets();
	for (Ref<OpenXRActionSet> action_set : action_sets) {
		options->add_item(action_set->get_localized_name());
		options->set_item_metadata(-1, action_set);
	}
}

void EditorPropertyActionSet::set_option_button_clip(bool p_enable) {
	options->set_clip_text(p_enable);
}

EditorPropertyActionSet::EditorPropertyActionSet() {
	options = memnew(OptionButton);
	options->set_clip_text(true);
	options->set_flat(true);
	options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	add_child(options);
	add_focusable(options);
	options->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertyActionSet::_option_selected));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorPropertyBindingPath

void EditorPropertyBindingPath::_set_read_only(bool p_read_only) {
	options->set_disabled(p_read_only);
}

void EditorPropertyBindingPath::_option_selected(int p_which) {
	String val = options->get_item_metadata(p_which);
	emit_changed(get_edited_property(), val);
}

void EditorPropertyBindingPath::update_property() {
	Variant current = get_edited_property_value();
	if (current.get_type() == Variant::NIL) {
		options->select(-1);
		return;
	}

	String which = current;
	for (int i = 0; i < options->get_item_count(); i++) {
		if (which == (String)options->get_item_metadata(i)) {
			options->select(i);
			return;
		}
	}

	// Couldn't find it? deselect..
	options->select(-1);
}

void EditorPropertyBindingPath::setup(const String &p_interaction_profile_path, Vector<OpenXRAction::ActionType> p_include_action_types) {
	options->clear();

	const OpenXRInteractionProfileMetadata::InteractionProfile *profile_def = OpenXRInteractionProfileMetadata::get_singleton()->get_profile(p_interaction_profile_path);

	// Determine toplevel paths
	Vector<String> top_level_paths;
	for (const OpenXRInteractionProfileMetadata::IOPath &io_path : profile_def->io_paths) {
		if (!top_level_paths.has(io_path.top_level_path)) {
			top_level_paths.push_back(io_path.top_level_path);
		}
	}

	for (const String &top_level_path : top_level_paths) {
		String top_level_name = OpenXRInteractionProfileMetadata::get_singleton()->get_top_level_name(top_level_path);

		for (const OpenXRInteractionProfileMetadata::IOPath &io_path : profile_def->io_paths) {
			if (io_path.top_level_path == top_level_path && p_include_action_types.has(io_path.action_type)) {
				options->add_item(top_level_name + "/" + io_path.display_name);
				options->set_item_metadata(-1, io_path.openxr_path);
			}
		}
	}
}

void EditorPropertyBindingPath::set_option_button_clip(bool p_enable) {
	options->set_clip_text(p_enable);
}

EditorPropertyBindingPath::EditorPropertyBindingPath() {
	options = memnew(OptionButton);
	options->set_clip_text(true);
	options->set_flat(true);
	options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	add_child(options);
	add_focusable(options);
	options->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertyBindingPath::_option_selected));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenXRBindingModifierEditor

bool EditorInspectorPluginBindingModifier::can_handle(Object *p_object) {
	Ref<OpenXRBindingModifier> binding_modifier(Object::cast_to<OpenXRBindingModifier>(p_object));
	return binding_modifier.is_valid();
}

bool EditorInspectorPluginBindingModifier::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	Ref<OpenXRActionBindingModifier> action_binding_modifier(Object::cast_to<OpenXRActionBindingModifier>(p_object));
	if (action_binding_modifier.is_valid()) {
		if (p_type == Variant::OBJECT && p_hint == PROPERTY_HINT_RESOURCE_TYPE && p_hint_text == OpenXRActionSet::get_class_static()) {
			OpenXRIPBinding *ip_binding = action_binding_modifier->get_ip_binding();
			ERR_FAIL_NULL_V(ip_binding, false);

			OpenXRActionMap *action_map = ip_binding->get_action_map();
			ERR_FAIL_NULL_V(action_map, false);

			EditorPropertyActionSet *action_set_property = memnew(EditorPropertyActionSet);
			action_set_property->setup(action_map);
			add_property_editor(p_path, action_set_property);
			return true;
		}

		return false;
	}

	Ref<OpenXRIPBindingModifier> ip_binding_modifier(Object::cast_to<OpenXRIPBindingModifier>(p_object));
	if (ip_binding_modifier.is_valid()) {
		if (p_type == Variant::OBJECT && p_hint == PROPERTY_HINT_RESOURCE_TYPE && p_hint_text == OpenXRActionSet::get_class_static()) {
			OpenXRInteractionProfile *interaction_profile = ip_binding_modifier->get_interaction_profile();
			ERR_FAIL_NULL_V(interaction_profile, false);

			OpenXRActionMap *action_map = interaction_profile->get_action_map();
			ERR_FAIL_NULL_V(action_map, false);

			EditorPropertyActionSet *action_set_property = memnew(EditorPropertyActionSet);
			action_set_property->setup(action_map);
			add_property_editor(p_path, action_set_property);
			return true;
		}

		if (p_type == Variant::STRING && p_hint == PROPERTY_HINT_TYPE_STRING && p_hint_text == "binding_path") {
			EditorPropertyBindingPath *binding_path_property = memnew(EditorPropertyBindingPath);

			OpenXRInteractionProfile *interaction_profile = ip_binding_modifier->get_interaction_profile();
			ERR_FAIL_NULL_V(interaction_profile, false);

			Vector<OpenXRAction::ActionType> action_types;
			action_types.push_back(OpenXRAction::OPENXR_ACTION_VECTOR2);
			binding_path_property->setup(interaction_profile->get_interaction_profile_path(), action_types);

			add_property_editor(p_path, binding_path_property);
			return true;
		}

		return false;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenXRBindingModifierEditor

void OpenXRBindingModifierEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_binding_modifier"), &OpenXRBindingModifierEditor::get_binding_modifier);
	ClassDB::bind_method(D_METHOD("setup", "action_map", "binding_modifier"), &OpenXRBindingModifierEditor::setup);

	ADD_SIGNAL(MethodInfo("binding_modifier_removed", PropertyInfo(Variant::OBJECT, "binding_modifier_editor")));
}

void OpenXRBindingModifierEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			rem_binding_modifier_btn->set_button_icon(get_theme_icon(SNAME("Remove"), EditorStringName(EditorIcons)));
		} break;
	}
}

void OpenXRBindingModifierEditor::_on_remove_binding_modifier() {
	// Tell parent to remove us
	emit_signal("binding_modifier_removed", this);
}

OpenXRBindingModifierEditor::OpenXRBindingModifierEditor() {
	undo_redo = EditorUndoRedoManager::get_singleton();

	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	main_vb = memnew(VBoxContainer);
	main_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main_vb);

	header_hb = memnew(HBoxContainer);
	header_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vb->add_child(header_hb);

	binding_modifier_title = memnew(Label);
	binding_modifier_title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_hb->add_child(binding_modifier_title);

	rem_binding_modifier_btn = memnew(Button);
	rem_binding_modifier_btn->set_tooltip_text(TTR("Remove binding modifier."));
	rem_binding_modifier_btn->connect(SceneStringName(pressed), callable_mp(this, &OpenXRBindingModifierEditor::_on_remove_binding_modifier));
	rem_binding_modifier_btn->set_flat(true);
	header_hb->add_child(rem_binding_modifier_btn);

	editor_inspector = memnew(EditorInspector);
	editor_inspector->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	editor_inspector->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	editor_inspector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vb->add_child(editor_inspector);
}

void OpenXRBindingModifierEditor::setup(Ref<OpenXRActionMap> p_action_map, Ref<OpenXRBindingModifier> p_binding_modifier) {
	ERR_FAIL_NULL(binding_modifier_title);
	ERR_FAIL_NULL(editor_inspector);

	action_map = p_action_map;
	binding_modifier = p_binding_modifier;

	if (p_binding_modifier.is_valid()) {
		binding_modifier_title->set_text(p_binding_modifier->get_description());

		editor_inspector->set_object_class(p_binding_modifier->get_class());
		editor_inspector->edit(p_binding_modifier.ptr());
	}
}
