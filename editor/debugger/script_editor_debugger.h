/**************************************************************************/
/*  script_editor_debugger.h                                              */
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
#include "core/os/os.h"
#include "editor/debugger/editor_debugger_inspector.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/editor_debugger_server.h"
#include "scene/gui/button.h"
#include "scene/gui/margin_container.h"

class Tree;
class LineEdit;
class TabContainer;
class RichTextLabel;
class TextureButton;
class AcceptDialog;
class TreeItem;
class HSplitContainer;
class ItemList;
class EditorProfiler;
class EditorFileDialog;
class EditorVisualProfiler;
class EditorPerformanceProfiler;
class SceneDebuggerTree;
class EditorDebuggerPlugin;
class DebugAdapterProtocol;
class DebugAdapterParser;
class EditorExpressionEvaluator;

class ScriptEditorDebugger : public MarginContainer {
	GDCLASS(ScriptEditorDebugger, MarginContainer);

	friend class EditorDebuggerNode;
	friend class DebugAdapterProtocol;
	friend class DebugAdapterParser;

private:
	enum MessageType {
		MESSAGE_ERROR,
		MESSAGE_WARNING,
		MESSAGE_SUCCESS,
	};

	enum ProfilerType {
		PROFILER_VISUAL,
		PROFILER_SCRIPTS_SERVERS
	};

	enum Actions {
		ACTION_COPY_ERROR,
		ACTION_OPEN_SOURCE,
		ACTION_DELETE_BREAKPOINT,
		ACTION_DELETE_BREAKPOINTS_IN_FILE,
		ACTION_DELETE_ALL_BREAKPOINTS,
	};

	AcceptDialog *msgdialog = nullptr;

	LineEdit *clicked_ctrl = nullptr;
	LineEdit *clicked_ctrl_type = nullptr;
	LineEdit *live_edit_root = nullptr;
	Button *le_set = nullptr;
	Button *le_clear = nullptr;
	Button *export_csv = nullptr;

	VBoxContainer *errors_tab = nullptr;
	Tree *error_tree = nullptr;
	Button *expand_all_button = nullptr;
	Button *collapse_all_button = nullptr;
	Button *clear_button = nullptr;
	PopupMenu *item_menu = nullptr;

	Tree *breakpoints_tree = nullptr;
	PopupMenu *breakpoints_menu = nullptr;

	EditorFileDialog *file_dialog = nullptr;
	enum FileDialogPurpose {
		SAVE_MONITORS_CSV,
		SAVE_VRAM_CSV,
	};
	FileDialogPurpose file_dialog_purpose;

	int error_count;
	int warning_count;

	bool skip_breakpoints_value = false;
	Ref<Script> stack_script;

	TabContainer *tabs = nullptr;

	Label *reason = nullptr;

	Button *skip_breakpoints = nullptr;
	Button *copy = nullptr;
	Button *step = nullptr;
	Button *next = nullptr;
	Button *dobreak = nullptr;
	Button *docontinue = nullptr;
	// Reference to "Remote" tab in scene tree. Needed by _live_edit_set and buttons state.
	// Each debugger should have it's tree in the future I guess.
	const Tree *editor_remote_tree = nullptr;

	HashMap<int, String> profiler_signature;

	Tree *vmem_tree = nullptr;
	Button *vmem_refresh = nullptr;
	Button *vmem_export = nullptr;
	LineEdit *vmem_total = nullptr;

	Tree *stack_dump = nullptr;
	LineEdit *search = nullptr;
	OptionButton *threads = nullptr;
	EditorDebuggerInspector *inspector = nullptr;
	SceneDebuggerTree *scene_tree = nullptr;

	Ref<RemoteDebuggerPeer> peer;

	HashMap<NodePath, int> node_path_cache;
	int last_path_id;
	HashMap<String, int> res_path_cache;

	EditorProfiler *profiler = nullptr;
	EditorVisualProfiler *visual_profiler = nullptr;
	EditorPerformanceProfiler *performance_profiler = nullptr;
	EditorExpressionEvaluator *expression_evaluator = nullptr;

	OS::ProcessID remote_pid = 0;
	bool move_to_foreground = true;
	bool can_request_idle_draw = false;

	bool live_debug;

	uint64_t debugging_thread_id = Thread::UNASSIGNED_ID;

	struct ThreadDebugged {
		String name;
		String error;
		bool can_debug = false;
		bool has_stackdump = false;
		uint32_t debug_order = 0;
		uint64_t thread_id = Thread::UNASSIGNED_ID; // for order
	};

	struct ThreadSort {
		bool operator()(const ThreadDebugged *a, const ThreadDebugged *b) const {
			return a->debug_order < b->debug_order;
		}
	};

	HashMap<uint64_t, ThreadDebugged> threads_debugged;
	bool thread_list_updating = false;

	void _select_thread(int p_index);

	EditorDebuggerNode::CameraOverride camera_override;

	void _stack_dump_frame_selected();

	void _file_selected(const String &p_file);
	void _parse_message(const String &p_msg, uint64_t p_thread_id, const Array &p_data);
	void _set_reason_text(const String &p_reason, MessageType p_type);
	void _update_buttons_state();
	void _remote_object_selected(ObjectID p_object);
	void _remote_object_edited(ObjectID, const String &p_prop, const Variant &p_value);
	void _remote_object_property_updated(ObjectID p_id, const String &p_property);

	void _video_mem_request();
	void _video_mem_export();

	void _resources_reimported(const PackedStringArray &p_resources);

	int _get_node_path_cache(const NodePath &p_path);

	int _get_res_path_cache(const String &p_path);

	void _live_edit_set();
	void _live_edit_clear();

	void _method_changed(Object *p_base, const StringName &p_name, const Variant **p_args, int p_argcount);
	void _property_changed(Object *p_base, const StringName &p_property, const Variant &p_value);

	void _error_activated();
	void _error_selected();

	void _expand_errors_list();
	void _collapse_errors_list();

	void _profiler_activate(bool p_enable, int p_profiler);
	void _profiler_seeked();

	void _clear_errors_list();

	void _breakpoints_item_rmb_selected(const Vector2 &p_pos, MouseButton p_button);
	void _error_tree_item_rmb_selected(const Vector2 &p_pos, MouseButton p_button);
	void _item_menu_id_pressed(int p_option);
	void _tab_changed(int p_tab);

	void _put_msg(const String &p_message, const Array &p_data, uint64_t p_thread_id = Thread::MAIN_ID);
	void _export_csv();

	void _clear_execution();
	void _stop_and_notify();

	void _set_breakpoint(const String &p_path, const int &p_line, const bool &p_enabled);
	void _clear_breakpoints();

	void _breakpoint_tree_clicked();

	String _format_frame_text(const ScriptLanguage::StackInfo *info);

	void _thread_debug_enter(uint64_t p_thread_id);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void request_remote_object(ObjectID p_obj_id);
	void update_remote_object(ObjectID p_obj_id, const String &p_prop, const Variant &p_value);
	Object *get_remote_object(ObjectID p_id);

	// Needed by _live_edit_set, buttons state.
	void set_editor_remote_tree(const Tree *p_tree) { editor_remote_tree = p_tree; }

	void request_remote_tree();
	const SceneDebuggerTree *get_remote_tree();

	void request_remote_evaluate(const String &p_expression, int p_stack_frame);

	void start(Ref<RemoteDebuggerPeer> p_peer);
	void stop();

	void debug_skip_breakpoints();
	void debug_copy();

	void debug_next();
	void debug_step();
	void debug_break();
	void debug_continue();
	bool is_breaked() const { return threads_debugged.size() > 0; }
	bool is_debuggable() const { return threads_debugged.size() > 0 && threads_debugged[debugging_thread_id].can_debug; }
	bool is_session_active() { return peer.is_valid() && peer->is_peer_connected(); }
	int get_remote_pid() const { return remote_pid; }

	bool is_move_to_foreground() const;
	void set_move_to_foreground(const bool &p_move_to_foreground);

	int get_error_count() const { return error_count; }
	int get_warning_count() const { return warning_count; }
	String get_stack_script_file() const;
	int get_stack_script_line() const;
	int get_stack_script_frame() const;

	bool request_stack_dump(const int &p_frame);

	void update_tabs();
	void clear_style();
	String get_var_value(const String &p_var) const;

	void save_node(ObjectID p_id, const String &p_file);
	void set_live_debugging(bool p_enable);

	void live_debug_create_node(const NodePath &p_parent, const String &p_type, const String &p_name);
	void live_debug_instantiate_node(const NodePath &p_parent, const String &p_path, const String &p_name);
	void live_debug_remove_node(const NodePath &p_at);
	void live_debug_remove_and_keep_node(const NodePath &p_at, ObjectID p_keep_id);
	void live_debug_restore_node(ObjectID p_id, const NodePath &p_at, int p_at_pos);
	void live_debug_duplicate_node(const NodePath &p_at, const String &p_new_name);
	void live_debug_reparent_node(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos);

	EditorDebuggerNode::CameraOverride get_camera_override() const;
	void set_camera_override(EditorDebuggerNode::CameraOverride p_override);

	void set_breakpoint(const String &p_path, int p_line, bool p_enabled);

	void update_live_edit_root();

	void reload_all_scripts();
	void reload_scripts(const Vector<String> &p_script_paths);

	bool is_skip_breakpoints();

	virtual Size2 get_minimum_size() const override;

	void add_debugger_tab(Control *p_control);
	void remove_debugger_tab(Control *p_control);
	int get_current_debugger_tab() const;
	void switch_to_debugger(int p_debugger_tab_idx);

	void send_message(const String &p_message, const Array &p_args);
	void toggle_profiler(const String &p_profiler, bool p_enable, const Array &p_data);

	ScriptEditorDebugger();
	~ScriptEditorDebugger();
};
