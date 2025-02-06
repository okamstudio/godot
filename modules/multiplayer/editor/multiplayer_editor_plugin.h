/**************************************************************************/
/*  multiplayer_editor_plugin.h                                           */
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

#include "editor/plugins/editor_debugger_plugin.h"
#include "editor/plugins/editor_plugin.h"

class EditorNetworkProfiler;
class MultiplayerEditorDebugger : public EditorDebuggerPlugin {
	GDCLASS(MultiplayerEditorDebugger, EditorDebuggerPlugin);

private:
	HashMap<int, EditorNetworkProfiler *> profilers;

	void _open_request(const String &p_path);
	void _profiler_activate(bool p_enable, int p_session_id);

protected:
	static void _bind_methods();

public:
	virtual bool has_capture(const String &p_capture) const override;
	virtual bool capture(const String &p_message, const Array &p_data, int p_index) override;
	virtual void setup_session(int p_session_id) override;

	MultiplayerEditorDebugger() {}
};

class ReplicationEditor;

class MultiplayerEditorPlugin : public EditorPlugin {
	GDCLASS(MultiplayerEditorPlugin, EditorPlugin);

private:
	Button *button = nullptr;
	ReplicationEditor *repl_editor = nullptr;
	Ref<MultiplayerEditorDebugger> debugger;

	void _open_request(const String &p_path);
	void _node_removed(Node *p_node);

	void _pinned();

protected:
	void _notification(int p_what);

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	MultiplayerEditorPlugin();
};
