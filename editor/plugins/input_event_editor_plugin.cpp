/**************************************************************************/
/*  input_event_editor_plugin.cpp                                         */
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

#include "input_event_editor_plugin.h"

#include "editor/event_listener_line_edit.h"
#include "editor/input_event_configuration_dialog.h"

void InputEventConfigContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			open_config_button->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
		} break;
	}
}

void InputEventConfigContainer::_configure_pressed() {
	config_dialog->popup_and_configure(input_event);
}

void InputEventConfigContainer::_event_changed() {
	input_event_text->set_text(input_event->as_text());
}

void InputEventConfigContainer::_config_dialog_confirmed() {
	Ref<InputEvent> ie = config_dialog->get_event();
	input_event->copy_from(ie);
	_event_changed();
}

void InputEventConfigContainer::set_event(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	Ref<InputEventMouseButton> m = p_event;
	Ref<InputEventJoypadButton> jb = p_event;
	Ref<InputEventJoypadMotion> jm = p_event;

	if (k.is_valid()) {
		config_dialog->set_allowed_input_types(INPUT_KEY);
	} else if (m.is_valid()) {
		config_dialog->set_allowed_input_types(INPUT_MOUSE_BUTTON);
	} else if (jb.is_valid()) {
		config_dialog->set_allowed_input_types(INPUT_JOY_BUTTON);
	} else if (jm.is_valid()) {
		config_dialog->set_allowed_input_types(INPUT_JOY_MOTION);
	}

	input_event = p_event;
	_event_changed();
	input_event->connect_changed(callable_mp(this, &InputEventConfigContainer::_event_changed));
}

InputEventConfigContainer::InputEventConfigContainer() {
	input_event_text = memnew(Label);
	input_event_text->set_h_size_flags(SIZE_EXPAND_FILL);
	input_event_text->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD_SMART);
	input_event_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	add_child(input_event_text);

	open_config_button = EditorInspector::create_inspector_action_button(TTR("Configure"));
	open_config_button->connect(SceneStringName(pressed), callable_mp(this, &InputEventConfigContainer::_configure_pressed));
	add_child(open_config_button);

	add_child(memnew(Control));

	config_dialog = memnew(InputEventConfigurationDialog);
	config_dialog->connect(SceneStringName(confirmed), callable_mp(this, &InputEventConfigContainer::_config_dialog_confirmed));
	add_child(config_dialog);
}

///////////////////////

bool EditorInspectorPluginInputEvent::can_handle(Object *p_object) {
	Ref<InputEventKey> k = Ref<InputEventKey>(p_object);
	Ref<InputEventMouseButton> m = Ref<InputEventMouseButton>(p_object);
	Ref<InputEventJoypadButton> jb = Ref<InputEventJoypadButton>(p_object);
	Ref<InputEventJoypadMotion> jm = Ref<InputEventJoypadMotion>(p_object);

	return k.is_valid() || m.is_valid() || jb.is_valid() || jm.is_valid();
}

void EditorInspectorPluginInputEvent::parse_begin(Object *p_object) {
	Ref<InputEvent> ie = Ref<InputEvent>(p_object);

	InputEventConfigContainer *picker_controls = memnew(InputEventConfigContainer);
	picker_controls->set_event(ie);
	add_custom_control(picker_controls);
}

///////////////////////

InputEventEditorPlugin::InputEventEditorPlugin() {
	Ref<EditorInspectorPluginInputEvent> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
