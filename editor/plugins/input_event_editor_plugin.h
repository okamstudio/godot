/**************************************************************************/
/*  input_event_editor_plugin.h                                           */
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

#include "editor/action_map_editor.h"
#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class InputEventConfigContainer : public VBoxContainer {
	GDCLASS(InputEventConfigContainer, VBoxContainer);

	Label *input_event_text = nullptr;
	Button *open_config_button = nullptr;

	Ref<InputEvent> input_event;
	InputEventConfigurationDialog *config_dialog = nullptr;

	void _config_dialog_confirmed();
	void _configure_pressed();

	void _event_changed();

protected:
	void _notification(int p_what);

public:
	void set_event(const Ref<InputEvent> &p_event);

	InputEventConfigContainer();
};

class EditorInspectorPluginInputEvent : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginInputEvent, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class InputEventEditorPlugin : public EditorPlugin {
	GDCLASS(InputEventEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "InputEvent"; }

	InputEventEditorPlugin();
};
