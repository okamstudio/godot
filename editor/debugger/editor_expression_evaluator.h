/**************************************************************************/
/*  editor_expression_evaluator.h                                         */
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

#include "scene/gui/box_container.h"

class Button;
class CheckBox;
class EditorDebuggerInspector;
class LineEdit;
class RemoteDebuggerPeer;
class ScriptEditorDebugger;

class EditorExpressionEvaluator : public VBoxContainer {
	GDCLASS(EditorExpressionEvaluator, VBoxContainer)

private:
	Ref<RemoteDebuggerPeer> peer;

	LineEdit *expression_input = nullptr;
	CheckBox *clear_on_run_checkbox = nullptr;
	Button *evaluate_btn = nullptr;
	Button *clear_btn = nullptr;

	EditorDebuggerInspector *inspector = nullptr;

	void _evaluate();
	void _clear();

	void _remote_object_selected(ObjectID p_id);
	void _on_expression_input_changed(const String &p_expression);
	void _on_debugger_breaked(bool p_breaked, bool p_can_debug);
	void _on_debugger_clear_execution(Ref<Script> p_stack_script);

protected:
	ScriptEditorDebugger *editor_debugger = nullptr;

	void _notification(int p_what);

public:
	void on_start();
	void set_editor_debugger(ScriptEditorDebugger *p_editor_debugger);
	void add_value(const Array &p_array);

	EditorExpressionEvaluator();
};
