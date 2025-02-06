/**************************************************************************/
/*  editor_debugger_node.h                                                */
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

#include "core/object/script_language.h"
#include "editor/debugger/editor_debugger_server.h"
#include "scene/gui/margin_container.h"

class Button;
class DebugAdapterParser;
class EditorDebuggerPlugin;
class EditorDebuggerTree;
class EditorDebuggerRemoteObject;
class MenuButton;
class ScriptEditorDebugger;
class TabContainer;
class UndoRedo;

class EditorDebuggerNode : public MarginContainer {
	GDCLASS(EditorDebuggerNode, MarginContainer);

public:
	enum CameraOverride {
		OVERRIDE_NONE,
		OVERRIDE_INGAME,
		OVERRIDE_EDITORS,
	};

private:
	enum Options {
		DEBUG_NEXT,
		DEBUG_STEP,
		DEBUG_BREAK,
		DEBUG_CONTINUE,
		DEBUG_WITH_EXTERNAL_EDITOR,
	};

	class Breakpoint {
	public:
		String source;
		int line = 0;

		static uint32_t hash(const Breakpoint &p_val) {
			uint32_t h = HashMapHasherDefault::hash(p_val.source);
			return hash_murmur3_one_32(p_val.line, h);
		}
		bool operator==(const Breakpoint &p_b) const {
			return (line == p_b.line && source == p_b.source);
		}

		bool operator<(const Breakpoint &p_b) const {
			if (line == p_b.line) {
				return source < p_b.source;
			}
			return line < p_b.line;
		}

		Breakpoint() {}

		Breakpoint(const String &p_source, int p_line) {
			line = p_line;
			source = p_source;
		}
	};

	Ref<EditorDebuggerServer> server;
	TabContainer *tabs = nullptr;
	Button *debugger_button = nullptr;
	MenuButton *script_menu = nullptr;

	Ref<Script> stack_script; // Why?!?

	bool initializing = true;
	int last_error_count = 0;
	int last_warning_count = 0;

	float inspect_edited_object_timeout = 0;
	EditorDebuggerTree *remote_scene_tree = nullptr;
	float remote_scene_tree_timeout = 0.0;
	bool auto_switch_remote_scene_tree = false;
	bool debug_with_external_editor = false;
	bool keep_open = false;
	String current_uri;

	CameraOverride camera_override = OVERRIDE_NONE;
	HashMap<Breakpoint, bool, Breakpoint> breakpoints;

	HashSet<Ref<EditorDebuggerPlugin>> debugger_plugins;

	ScriptEditorDebugger *_add_debugger();
	EditorDebuggerRemoteObject *get_inspected_remote_object();
	void _update_errors();

	friend class DebuggerEditorPlugin;
	friend class DebugAdapterParser;
	static EditorDebuggerNode *singleton;
	EditorDebuggerNode();

protected:
	void _debugger_stopped(int p_id);
	void _debugger_wants_stop(int p_id);
	void _debugger_changed(int p_tab);
	void _remote_tree_select_requested(ObjectID p_id, int p_debugger);
	void _remote_tree_updated(int p_debugger);
	void _remote_tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _remote_object_updated(ObjectID p_id, int p_debugger);
	void _remote_object_property_updated(ObjectID p_id, const String &p_property, int p_debugger);
	void _remote_object_requested(ObjectID p_id, int p_debugger);
	void _save_node_requested(ObjectID p_id, const String &p_file, int p_debugger);

	void _breakpoint_set_in_tree(Ref<RefCounted> p_script, int p_line, bool p_enabled, int p_debugger);
	void _breakpoints_cleared_in_tree(int p_debugger);

	void _clear_execution(Ref<RefCounted> p_script) {
		emit_signal(SNAME("clear_execution"), p_script);
	}

	void _text_editor_stack_goto(const ScriptEditorDebugger *p_debugger);
	void _text_editor_stack_clear(const ScriptEditorDebugger *p_debugger);
	void _stack_frame_selected(int p_debugger);
	void _error_selected(const String &p_file, int p_line, int p_debugger);
	void _breaked(bool p_breaked, bool p_can_debug, const String &p_message, bool p_has_stackdump, int p_debugger);
	void _paused();
	void _break_state_changed();
	void _menu_option(int p_id);
	void _update_debug_options();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static EditorDebuggerNode *get_singleton() { return singleton; }
	void register_undo_redo(UndoRedo *p_undo_redo);

	ScriptEditorDebugger *get_previous_debugger() const;
	ScriptEditorDebugger *get_current_debugger() const;
	ScriptEditorDebugger *get_default_debugger() const;
	ScriptEditorDebugger *get_debugger(int p_debugger) const;

	void debug_next();
	void debug_step();
	void debug_break();
	void debug_continue();

	void set_script_debug_button(MenuButton *p_button);

	void set_tool_button(Button *p_button) {
		debugger_button = p_button;
	}

	String get_var_value(const String &p_var) const;
	Ref<Script> get_dump_stack_script() const { return stack_script; } // Why do we need this?

	bool get_debug_with_external_editor() { return debug_with_external_editor; }

	bool is_skip_breakpoints() const;
	void set_breakpoint(const String &p_path, int p_line, bool p_enabled);
	void set_breakpoints(const String &p_path, const Array &p_lines);
	void reload_all_scripts();
	void reload_scripts(const Vector<String> &p_script_paths);

	// Remote inspector/edit.
	void request_remote_tree();
	static void _methods_changed(void *p_ud, Object *p_base, const StringName &p_name, const Variant **p_args, int p_argcount);
	static void _properties_changed(void *p_ud, Object *p_base, const StringName &p_property, const Variant &p_value);

	// LiveDebug
	void set_live_debugging(bool p_enabled);
	void update_live_edit_root();
	void live_debug_create_node(const NodePath &p_parent, const String &p_type, const String &p_name);
	void live_debug_instantiate_node(const NodePath &p_parent, const String &p_path, const String &p_name);
	void live_debug_remove_node(const NodePath &p_at);
	void live_debug_remove_and_keep_node(const NodePath &p_at, ObjectID p_keep_id);
	void live_debug_restore_node(ObjectID p_id, const NodePath &p_at, int p_at_pos);
	void live_debug_duplicate_node(const NodePath &p_at, const String &p_new_name);
	void live_debug_reparent_node(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos);

	void set_camera_override(CameraOverride p_override);
	CameraOverride get_camera_override();

	String get_server_uri() const;

	void set_keep_open(bool p_keep_open);
	Error start(const String &p_uri = "tcp://");
	void stop(bool p_force = false);

	bool plugins_capture(ScriptEditorDebugger *p_debugger, const String &p_message, const Array &p_data);
	void add_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin);
	void remove_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin);
};
