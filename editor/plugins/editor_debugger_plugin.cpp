/**************************************************************************/
/*  editor_debugger_plugin.cpp                                            */
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

#include "editor_debugger_plugin.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor_debugger_plugin.compat.inc"

void EditorDebuggerSession::_breaked(bool p_really_did, bool p_can_debug, const String &p_message, bool p_has_stackdump) {
	if (p_really_did) {
		emit_signal(SNAME("breaked"), p_can_debug);
	} else {
		emit_signal(SNAME("continued"));
	}
}

void EditorDebuggerSession::_started() {
	emit_signal(SNAME("started"));
}

void EditorDebuggerSession::_stopped() {
	emit_signal(SNAME("stopped"));
}

void EditorDebuggerSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_message", "message", "data"), &EditorDebuggerSession::send_message, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("toggle_profiler", "profiler", "enable", "data"), &EditorDebuggerSession::toggle_profiler, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("is_breaked"), &EditorDebuggerSession::is_breaked);
	ClassDB::bind_method(D_METHOD("is_debuggable"), &EditorDebuggerSession::is_debuggable);
	ClassDB::bind_method(D_METHOD("is_active"), &EditorDebuggerSession::is_active);
	ClassDB::bind_method(D_METHOD("add_session_tab", "control"), &EditorDebuggerSession::add_session_tab);
	ClassDB::bind_method(D_METHOD("remove_session_tab", "control"), &EditorDebuggerSession::remove_session_tab);
	ClassDB::bind_method(D_METHOD("set_breakpoint", "path", "line", "breakpointed", "enabled", "suspend", "condition", "print"), &EditorDebuggerSession::set_breakpoint, DEFVAL(true), DEFVAL(true), DEFVAL(String()), DEFVAL(String()));

	ADD_SIGNAL(MethodInfo("started"));
	ADD_SIGNAL(MethodInfo("stopped"));
	ADD_SIGNAL(MethodInfo("breaked", PropertyInfo(Variant::BOOL, "can_debug")));
	ADD_SIGNAL(MethodInfo("continued"));
}

void EditorDebuggerSession::add_session_tab(Control *p_tab) {
	ERR_FAIL_COND(!p_tab || !debugger);
	debugger->add_debugger_tab(p_tab);
	tabs.insert(p_tab);
}

void EditorDebuggerSession::remove_session_tab(Control *p_tab) {
	ERR_FAIL_COND(!p_tab || !debugger);
	debugger->remove_debugger_tab(p_tab);
	tabs.erase(p_tab);
}

void EditorDebuggerSession::send_message(const String &p_message, const Array &p_args) {
	ERR_FAIL_NULL_MSG(debugger, "Plugin is not attached to debugger.");
	debugger->send_message(p_message, p_args);
}

void EditorDebuggerSession::toggle_profiler(const String &p_profiler, bool p_enable, const Array &p_data) {
	ERR_FAIL_NULL_MSG(debugger, "Plugin is not attached to debugger.");
	debugger->toggle_profiler(p_profiler, p_enable, p_data);
}

bool EditorDebuggerSession::is_breaked() {
	ERR_FAIL_NULL_V_MSG(debugger, false, "Plugin is not attached to debugger.");
	return debugger->is_breaked();
}

bool EditorDebuggerSession::is_debuggable() {
	ERR_FAIL_NULL_V_MSG(debugger, false, "Plugin is not attached to debugger.");
	return debugger->is_debuggable();
}

bool EditorDebuggerSession::is_active() {
	ERR_FAIL_NULL_V_MSG(debugger, false, "Plugin is not attached to debugger.");
	return debugger->is_session_active();
}

void EditorDebuggerSession::set_breakpoint(const String &p_path, int p_line, bool p_breakpointed, bool p_enabled, bool p_suspend, const String &p_condition, const String &p_print) {
	ERR_FAIL_NULL_MSG(debugger, "Plugin is not attached to debugger.");
	debugger->set_breakpoint(p_path, p_line, p_breakpointed, p_enabled, p_suspend, p_condition, p_print);
}

void EditorDebuggerSession::detach_debugger() {
	if (!debugger) {
		return;
	}
	debugger->disconnect("started", callable_mp(this, &EditorDebuggerSession::_started));
	debugger->disconnect("stopped", callable_mp(this, &EditorDebuggerSession::_stopped));
	debugger->disconnect("breaked", callable_mp(this, &EditorDebuggerSession::_breaked));
	debugger->disconnect(SceneStringName(tree_exited), callable_mp(this, &EditorDebuggerSession::_debugger_gone_away));
	for (Control *tab : tabs) {
		debugger->remove_debugger_tab(tab);
	}
	tabs.clear();
	debugger = nullptr;
}

void EditorDebuggerSession::_debugger_gone_away() {
	debugger = nullptr;
	tabs.clear();
}

EditorDebuggerSession::EditorDebuggerSession(ScriptEditorDebugger *p_debugger) {
	ERR_FAIL_NULL(p_debugger);
	debugger = p_debugger;
	debugger->connect("started", callable_mp(this, &EditorDebuggerSession::_started));
	debugger->connect("stopped", callable_mp(this, &EditorDebuggerSession::_stopped));
	debugger->connect("breaked", callable_mp(this, &EditorDebuggerSession::_breaked));
	debugger->connect(SceneStringName(tree_exited), callable_mp(this, &EditorDebuggerSession::_debugger_gone_away), CONNECT_ONE_SHOT);
}

EditorDebuggerSession::~EditorDebuggerSession() {
	detach_debugger();
}

/// EditorDebuggerPlugin

EditorDebuggerPlugin::~EditorDebuggerPlugin() {
	clear();
}

void EditorDebuggerPlugin::clear() {
	for (Ref<EditorDebuggerSession> &session : sessions) {
		session->detach_debugger();
	}
	sessions.clear();
}

void EditorDebuggerPlugin::create_session(ScriptEditorDebugger *p_debugger) {
	sessions.push_back(Ref<EditorDebuggerSession>(memnew(EditorDebuggerSession(p_debugger))));
	setup_session(sessions.size() - 1);
}

void EditorDebuggerPlugin::setup_session(int p_idx) {
	GDVIRTUAL_CALL(_setup_session, p_idx);
}

Ref<EditorDebuggerSession> EditorDebuggerPlugin::get_session(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, sessions.size(), nullptr);
	return sessions.get(p_idx);
}

Array EditorDebuggerPlugin::get_sessions() {
	Array ret;
	for (const Ref<EditorDebuggerSession> &session : sessions) {
		ret.push_back(session);
	}
	return ret;
}

bool EditorDebuggerPlugin::has_capture(const String &p_message) const {
	bool ret = false;
	if (GDVIRTUAL_CALL(_has_capture, p_message, ret)) {
		return ret;
	}
	return false;
}

bool EditorDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session_id) {
	bool ret = false;
	if (GDVIRTUAL_CALL(_capture, p_message, p_data, p_session_id, ret)) {
		return ret;
	}
	return false;
}

void EditorDebuggerPlugin::goto_script_line(const Ref<Script> &p_script, int p_line) {
	GDVIRTUAL_CALL(_goto_script_line, p_script, p_line);
}

void EditorDebuggerPlugin::breakpoints_cleared_in_tree() {
	GDVIRTUAL_CALL(_breakpoints_cleared_in_tree);
}

void EditorDebuggerPlugin::breakpoint_set_in_tree(const Ref<Script> &p_script, int p_line, bool p_breakpointed, bool p_enabled, bool p_suspend, const String &p_condition, const String &p_print) {
	GDVIRTUAL_CALL(_breakpoint_set_in_tree, p_script, p_line, p_breakpointed);
}

void EditorDebuggerPlugin::_bind_methods() {
	GDVIRTUAL_BIND(_setup_session, "session_id");
	GDVIRTUAL_BIND(_has_capture, "capture");
	GDVIRTUAL_BIND(_capture, "message", "data", "session_id");
	GDVIRTUAL_BIND(_goto_script_line, "script", "line");
	GDVIRTUAL_BIND(_breakpoints_cleared_in_tree);
	GDVIRTUAL_BIND(_breakpoint_set_in_tree, "script", "line", "breakpointed");
	ClassDB::bind_method(D_METHOD("get_session", "id"), &EditorDebuggerPlugin::get_session);
	ClassDB::bind_method(D_METHOD("get_sessions"), &EditorDebuggerPlugin::get_sessions);
}

EditorDebuggerPlugin::EditorDebuggerPlugin() {
	EditorDebuggerNode::get_singleton()->connect("goto_script_line", callable_mp(this, &EditorDebuggerPlugin::goto_script_line));
	EditorDebuggerNode::get_singleton()->connect("breakpoints_cleared_in_tree", callable_mp(this, &EditorDebuggerPlugin::breakpoints_cleared_in_tree));
	EditorDebuggerNode::get_singleton()->connect("breakpoint_set_in_tree", callable_mp(this, &EditorDebuggerPlugin::breakpoint_set_in_tree));
}
