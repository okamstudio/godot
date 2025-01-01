/**************************************************************************/
/*  editor_debugger_node.cpp                                              */
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

#include "editor_debugger_node.h"

#include "core/object/undo_redo.h"
#include "editor/debugger/editor_debugger_tree.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/editor_debugger_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/tab_container.h"
#include "scene/resources/packed_scene.h"

template <typename Func>
void _for_all(TabContainer *p_node, const Func &p_func) {
	for (int i = 0; i < p_node->get_tab_count(); i++) {
		ScriptEditorDebugger *dbg = Object::cast_to<ScriptEditorDebugger>(p_node->get_tab_control(i));
		ERR_FAIL_NULL(dbg);
		p_func(dbg);
	}
}

EditorDebuggerNode *EditorDebuggerNode::singleton = nullptr;

EditorDebuggerNode::EditorDebuggerNode() {
	if (!singleton) {
		singleton = this;
	}

	add_theme_constant_override("margin_left", -EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles))->get_margin(SIDE_LEFT));
	add_theme_constant_override("margin_right", -EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles))->get_margin(SIDE_RIGHT));

	tabs = memnew(TabContainer);
	tabs->set_tabs_visible(false);
	tabs->connect("tab_changed", callable_mp(this, &EditorDebuggerNode::_debugger_changed));
	add_child(tabs);

	Ref<StyleBoxEmpty> empty;
	empty.instantiate();
	tabs->add_theme_style_override(SceneStringName(panel), empty);

	auto_switch_remote_scene_tree = EDITOR_GET("debugger/auto_switch_to_remote_scene_tree");
	_add_debugger();

	// Remote scene tree
	remote_scene_tree = memnew(EditorDebuggerTree);
	remote_scene_tree->connect("object_selected", callable_mp(this, &EditorDebuggerNode::_remote_object_requested));
	remote_scene_tree->connect("save_node", callable_mp(this, &EditorDebuggerNode::_save_node_requested));
	remote_scene_tree->connect("button_clicked", callable_mp(this, &EditorDebuggerNode::_remote_tree_button_pressed));
	SceneTreeDock::get_singleton()->add_remote_tree_editor(remote_scene_tree);
	SceneTreeDock::get_singleton()->connect("remote_tree_selected", callable_mp(this, &EditorDebuggerNode::request_remote_tree));

	remote_scene_tree_timeout = EDITOR_GET("debugger/remote_scene_tree_refresh_interval");
	inspect_edited_object_timeout = EDITOR_GET("debugger/remote_inspect_refresh_interval");

	EditorRunBar::get_singleton()->get_pause_button()->connect(SceneStringName(pressed), callable_mp(this, &EditorDebuggerNode::_paused));
}

ScriptEditorDebugger *EditorDebuggerNode::_add_debugger() {
	ScriptEditorDebugger *node = memnew(ScriptEditorDebugger);

	int id = tabs->get_tab_count();
	node->connect("stop_requested", callable_mp(this, &EditorDebuggerNode::_debugger_wants_stop).bind(id));
	node->connect("stopped", callable_mp(this, &EditorDebuggerNode::_debugger_stopped).bind(id));
	node->connect("stack_frame_selected", callable_mp(this, &EditorDebuggerNode::_stack_frame_selected).bind(id));
	node->connect("error_selected", callable_mp(this, &EditorDebuggerNode::_error_selected).bind(id));
	node->connect("breakpoint_selected", callable_mp(this, &EditorDebuggerNode::_error_selected).bind(id));
	node->connect("clear_execution", callable_mp(this, &EditorDebuggerNode::_clear_execution));
	node->connect("breaked", callable_mp(this, &EditorDebuggerNode::_breaked).bind(id));
	node->connect("remote_tree_select_requested", callable_mp(this, &EditorDebuggerNode::_remote_tree_select_requested).bind(id));
	node->connect("remote_tree_updated", callable_mp(this, &EditorDebuggerNode::_remote_tree_updated).bind(id));
	node->connect("remote_object_updated", callable_mp(this, &EditorDebuggerNode::_remote_object_updated).bind(id));
	node->connect("remote_object_property_updated", callable_mp(this, &EditorDebuggerNode::_remote_object_property_updated).bind(id));
	node->connect("remote_object_requested", callable_mp(this, &EditorDebuggerNode::_remote_object_requested).bind(id));
	node->connect("set_breakpoint", callable_mp(this, &EditorDebuggerNode::_breakpoint_set_in_tree).bind(id));
	node->connect("clear_breakpoints", callable_mp(this, &EditorDebuggerNode::_breakpoints_cleared_in_tree).bind(id));
	node->connect("errors_cleared", callable_mp(this, &EditorDebuggerNode::_update_errors));

	if (tabs->get_tab_count() > 0) {
		get_debugger(0)->clear_style();
	}

	tabs->add_child(node);

	node->set_name(vformat(TTR("Session %d"), tabs->get_tab_count()));
	if (tabs->get_tab_count() > 1) {
		node->clear_style();
		tabs->set_tabs_visible(true);
		tabs->add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("DebuggerPanel"), EditorStringName(EditorStyles)));
	}

	if (!debugger_plugins.is_empty()) {
		for (Ref<EditorDebuggerPlugin> plugin : debugger_plugins) {
			plugin->create_session(node);
		}
	}

	return node;
}

void EditorDebuggerNode::_stack_frame_selected(int p_debugger) {
	const ScriptEditorDebugger *dbg = get_debugger(p_debugger);
	ERR_FAIL_NULL(dbg);
	if (dbg != get_current_debugger()) {
		return;
	}
	_text_editor_stack_goto(dbg);
}

void EditorDebuggerNode::_error_selected(const String &p_file, int p_line, int p_debugger) {
	Ref<Script> s = ResourceLoader::load(p_file);
	emit_signal(SNAME("goto_script_line"), s, p_line - 1);
}

void EditorDebuggerNode::_text_editor_stack_goto(const ScriptEditorDebugger *p_debugger) {
	String file = p_debugger->get_stack_script_file();
	if (file.is_empty()) {
		return;
	}
	if (file.is_resource_file()) {
		stack_script = ResourceLoader::load(file);
	} else {
		// If the script is built-in, it can be opened only if the scene is loaded in memory.
		int i = file.find("::");
		int j = file.rfind_char('(', i);
		if (j > -1) { // If the script is named, the string is "name (file)", so we need to extract the path.
			file = file.substr(j + 1, file.find_char(')', i) - j - 1);
		}
		Ref<PackedScene> ps = ResourceLoader::load(file.get_slice("::", 0));
		stack_script = ResourceLoader::load(file);
	}
	const int line = p_debugger->get_stack_script_line() - 1;
	emit_signal(SNAME("goto_script_line"), stack_script, line);
	emit_signal(SNAME("set_execution"), stack_script, line);
	stack_script.unref(); // Why?!?
}

void EditorDebuggerNode::_text_editor_stack_clear(const ScriptEditorDebugger *p_debugger) {
	String file = p_debugger->get_stack_script_file();
	if (file.is_empty()) {
		return;
	}
	if (file.is_resource_file()) {
		stack_script = ResourceLoader::load(file);
	} else {
		// If the script is built-in, it can be opened only if the scene is loaded in memory.
		int i = file.find("::");
		int j = file.rfind_char('(', i);
		if (j > -1) { // If the script is named, the string is "name (file)", so we need to extract the path.
			file = file.substr(j + 1, file.find_char(')', i) - j - 1);
		}
		Ref<PackedScene> ps = ResourceLoader::load(file.get_slice("::", 0));
		stack_script = ResourceLoader::load(file);
	}
	emit_signal(SNAME("clear_execution"), stack_script);
	stack_script.unref(); // Why?!?
}

void EditorDebuggerNode::_bind_methods() {
	// LiveDebug.
	ClassDB::bind_method("live_debug_create_node", &EditorDebuggerNode::live_debug_create_node);
	ClassDB::bind_method("live_debug_instantiate_node", &EditorDebuggerNode::live_debug_instantiate_node);
	ClassDB::bind_method("live_debug_remove_node", &EditorDebuggerNode::live_debug_remove_node);
	ClassDB::bind_method("live_debug_remove_and_keep_node", &EditorDebuggerNode::live_debug_remove_and_keep_node);
	ClassDB::bind_method("live_debug_restore_node", &EditorDebuggerNode::live_debug_restore_node);
	ClassDB::bind_method("live_debug_duplicate_node", &EditorDebuggerNode::live_debug_duplicate_node);
	ClassDB::bind_method("live_debug_reparent_node", &EditorDebuggerNode::live_debug_reparent_node);

	ADD_SIGNAL(MethodInfo("goto_script_line"));
	ADD_SIGNAL(MethodInfo("set_execution", PropertyInfo("script"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("clear_execution", PropertyInfo("script")));
	ADD_SIGNAL(MethodInfo("breaked", PropertyInfo(Variant::BOOL, "reallydid"), PropertyInfo(Variant::BOOL, "can_debug")));
	ADD_SIGNAL(MethodInfo("breakpoint_toggled", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("breakpoint_set_in_tree", PropertyInfo("script"), PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::BOOL, "enabled"), PropertyInfo(Variant::INT, "debugger")));
	ADD_SIGNAL(MethodInfo("breakpoints_cleared_in_tree", PropertyInfo(Variant::INT, "debugger")));
}

void EditorDebuggerNode::register_undo_redo(UndoRedo *p_undo_redo) {
	p_undo_redo->set_method_notify_callback(_methods_changed, this);
	p_undo_redo->set_property_notify_callback(_properties_changed, this);
}

EditorDebuggerRemoteObject *EditorDebuggerNode::get_inspected_remote_object() {
	return Object::cast_to<EditorDebuggerRemoteObject>(ObjectDB::get_instance(EditorNode::get_singleton()->get_editor_selection_history()->get_current()));
}

ScriptEditorDebugger *EditorDebuggerNode::get_debugger(int p_id) const {
	return Object::cast_to<ScriptEditorDebugger>(tabs->get_tab_control(p_id));
}

ScriptEditorDebugger *EditorDebuggerNode::get_previous_debugger() const {
	return Object::cast_to<ScriptEditorDebugger>(tabs->get_tab_control(tabs->get_previous_tab()));
}

ScriptEditorDebugger *EditorDebuggerNode::get_current_debugger() const {
	return Object::cast_to<ScriptEditorDebugger>(tabs->get_tab_control(tabs->get_current_tab()));
}

ScriptEditorDebugger *EditorDebuggerNode::get_default_debugger() const {
	return Object::cast_to<ScriptEditorDebugger>(tabs->get_tab_control(0));
}

String EditorDebuggerNode::get_server_uri() const {
	ERR_FAIL_COND_V(server.is_null(), "");
	return server->get_uri();
}

void EditorDebuggerNode::set_keep_open(bool p_keep_open) {
	keep_open = p_keep_open;
	if (keep_open) {
		if (server.is_null() || !server->is_active()) {
			start();
		}
	} else {
		bool found = false;
		_for_all(tabs, [&](ScriptEditorDebugger *p_debugger) {
			if (p_debugger->is_session_active()) {
				found = true;
			}
		});
		if (!found) {
			stop();
		}
	}
}

Error EditorDebuggerNode::start(const String &p_uri) {
	ERR_FAIL_COND_V(!p_uri.contains("://"), ERR_INVALID_PARAMETER);
	if (keep_open && current_uri == p_uri && server.is_valid()) {
		return OK;
	}
	stop(true);
	current_uri = p_uri;

	server = Ref<EditorDebuggerServer>(EditorDebuggerServer::create(p_uri.substr(0, p_uri.find("://") + 3)));
	const Error err = server->start(p_uri);
	if (err != OK) {
		return err;
	}
	set_process(true);
	EditorNode::get_log()->add_message("--- Debugging process started ---", EditorLog::MSG_TYPE_EDITOR);
	return OK;
}

void EditorDebuggerNode::stop(bool p_force) {
	if (keep_open && !p_force) {
		return;
	}
	current_uri.clear();
	if (server.is_valid()) {
		server->stop();
		EditorNode::get_log()->add_message("--- Debugging process stopped ---", EditorLog::MSG_TYPE_EDITOR);

		if (EditorRunBar::get_singleton()->is_movie_maker_enabled()) {
			// Request attention in case the user was doing something else when movie recording is finished.
			DisplayServer::get_singleton()->window_request_attention();
		}

		server.unref();
	}
	// Also close all debugging sessions.
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		if (dbg->is_session_active()) {
			dbg->_stop_and_notify();
		}
	});
	_break_state_changed();
	breakpoints.clear();
	EditorUndoRedoManager::get_singleton()->clear_history(EditorUndoRedoManager::REMOTE_HISTORY, false);
	set_process(false);
}

void EditorDebuggerNode::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorThemeManager::is_generated_theme_outdated()) {
				return;
			}

			if (tabs->get_tab_count() > 1) {
				add_theme_constant_override("margin_left", -EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles))->get_margin(SIDE_LEFT));
				add_theme_constant_override("margin_right", -EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles))->get_margin(SIDE_RIGHT));

				tabs->add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("DebuggerPanel"), EditorStringName(EditorStyles)));
			}

			remote_scene_tree->update_icon_max_width();
		} break;

		case NOTIFICATION_READY: {
			_update_debug_options();
			initializing = false;
		} break;

		case NOTIFICATION_PROCESS: {
			if (server.is_null()) {
				return;
			}

			if (!server->is_active()) {
				stop();
				return;
			}
			server->poll();

			_update_errors();

			// Remote scene tree update
			remote_scene_tree_timeout -= get_process_delta_time();
			if (remote_scene_tree_timeout < 0) {
				remote_scene_tree_timeout = EDITOR_GET("debugger/remote_scene_tree_refresh_interval");
				if (remote_scene_tree->is_visible_in_tree()) {
					get_current_debugger()->request_remote_tree();
				}
			}

			// Remote inspector update
			inspect_edited_object_timeout -= get_process_delta_time();
			if (inspect_edited_object_timeout < 0) {
				inspect_edited_object_timeout = EDITOR_GET("debugger/remote_inspect_refresh_interval");
				if (EditorDebuggerRemoteObject *obj = get_inspected_remote_object()) {
					get_current_debugger()->request_remote_object(obj->remote_object_id);
				}
			}

			// Take connections.
			if (server->is_connection_available()) {
				ScriptEditorDebugger *debugger = nullptr;
				_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
					if (debugger || dbg->is_session_active()) {
						return;
					}
					debugger = dbg;
				});
				if (debugger == nullptr) {
					if (tabs->get_tab_count() <= 4) { // Max 4 debugging sessions active.
						debugger = _add_debugger();
					} else {
						// We already have too many sessions, disconnecting new clients to prevent them from hanging.
						server->take_connection()->close();
						return; // Can't add, stop here.
					}
				}

				EditorRunBar::get_singleton()->get_pause_button()->set_disabled(false);
				// Switch to remote tree view if so desired.
				auto_switch_remote_scene_tree = (bool)EDITOR_GET("debugger/auto_switch_to_remote_scene_tree");
				if (auto_switch_remote_scene_tree) {
					SceneTreeDock::get_singleton()->show_remote_tree();
				}
				// Good to go.
				SceneTreeDock::get_singleton()->show_tab_buttons();
				debugger->set_editor_remote_tree(remote_scene_tree);
				debugger->start(server->take_connection());
				// Send breakpoints.
				for (const KeyValue<Breakpoint, bool> &E : breakpoints) {
					const Breakpoint &bp = E.key;
					debugger->set_breakpoint(bp.source, bp.line, E.value);
				} // Will arrive too late, how does the regular run work?

				debugger->update_live_edit_root();
			}
		} break;
	}
}

void EditorDebuggerNode::_update_errors() {
	int error_count = 0;
	int warning_count = 0;
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		error_count += dbg->get_error_count();
		warning_count += dbg->get_warning_count();
	});

	if (error_count != last_error_count || warning_count != last_warning_count) {
		_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
			dbg->update_tabs();
		});

		if (error_count == 0 && warning_count == 0) {
			debugger_button->set_text(TTR("Debugger"));
			debugger_button->remove_theme_color_override(SceneStringName(font_color));
			debugger_button->set_button_icon(Ref<Texture2D>());
		} else {
			debugger_button->set_text(TTR("Debugger") + " (" + itos(error_count + warning_count) + ")");
			if (error_count >= 1 && warning_count >= 1) {
				debugger_button->set_button_icon(get_editor_theme_icon(SNAME("ErrorWarning")));
				// Use error color to represent the highest level of severity reported.
				debugger_button->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
			} else if (error_count >= 1) {
				debugger_button->set_button_icon(get_editor_theme_icon(SNAME("Error")));
				debugger_button->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
			} else {
				debugger_button->set_button_icon(get_editor_theme_icon(SNAME("Warning")));
				debugger_button->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
			}
		}
		last_error_count = error_count;
		last_warning_count = warning_count;
	}
}

void EditorDebuggerNode::_debugger_stopped(int p_id) {
	ScriptEditorDebugger *dbg = get_debugger(p_id);
	ERR_FAIL_NULL(dbg);

	bool found = false;
	_for_all(tabs, [&](ScriptEditorDebugger *p_debugger) {
		if (p_debugger->is_session_active()) {
			found = true;
		}
	});
	if (!found) {
		EditorRunBar::get_singleton()->get_pause_button()->set_pressed(false);
		EditorRunBar::get_singleton()->get_pause_button()->set_disabled(true);
		SceneTreeDock::get_singleton()->hide_remote_tree();
		SceneTreeDock::get_singleton()->hide_tab_buttons();
		EditorNode::get_singleton()->notify_all_debug_sessions_exited();
	}
}

void EditorDebuggerNode::_debugger_wants_stop(int p_id) {
	// Ask editor to kill PID.
	int pid = get_debugger(p_id)->get_remote_pid();
	if (pid) {
		callable_mp(EditorNode::get_singleton(), &EditorNode::stop_child_process).call_deferred(pid);
	}
}

void EditorDebuggerNode::_debugger_changed(int p_tab) {
	if (get_inspected_remote_object()) {
		// Clear inspected object, you can only inspect objects in selected debugger.
		// Hopefully, in the future, we will have one inspector per debugger.
		EditorNode::get_singleton()->push_item(nullptr);
	}

	if (get_previous_debugger()) {
		_text_editor_stack_clear(get_previous_debugger());
	}
	if (remote_scene_tree->is_visible_in_tree()) {
		get_current_debugger()->request_remote_tree();
	}
	if (get_current_debugger()->is_breaked()) {
		_text_editor_stack_goto(get_current_debugger());
	}

	_break_state_changed();
}

void EditorDebuggerNode::set_script_debug_button(MenuButton *p_button) {
	script_menu = p_button;
	script_menu->set_text(TTR("Debug"));
	script_menu->set_switch_on_hover(true);
	PopupMenu *p = script_menu->get_popup();
	p->add_shortcut(ED_GET_SHORTCUT("debugger/step_into"), DEBUG_STEP);
	p->add_shortcut(ED_GET_SHORTCUT("debugger/step_over"), DEBUG_NEXT);
	p->add_separator();
	p->add_shortcut(ED_GET_SHORTCUT("debugger/break"), DEBUG_BREAK);
	p->add_shortcut(ED_GET_SHORTCUT("debugger/continue"), DEBUG_CONTINUE);
	p->add_separator();
	p->add_check_shortcut(ED_GET_SHORTCUT("debugger/debug_with_external_editor"), DEBUG_WITH_EXTERNAL_EDITOR);
	p->connect(SceneStringName(id_pressed), callable_mp(this, &EditorDebuggerNode::_menu_option));

	_break_state_changed();
	script_menu->show();
}

void EditorDebuggerNode::_break_state_changed() {
	const bool breaked = get_current_debugger()->is_breaked();
	const bool can_debug = get_current_debugger()->is_debuggable();
	if (breaked) { // Show debugger.
		EditorNode::get_bottom_panel()->make_item_visible(this);
	}

	// Update script menu.
	if (!script_menu) {
		return;
	}
	PopupMenu *p = script_menu->get_popup();
	p->set_item_disabled(p->get_item_index(DEBUG_NEXT), !(breaked && can_debug));
	p->set_item_disabled(p->get_item_index(DEBUG_STEP), !(breaked && can_debug));
	p->set_item_disabled(p->get_item_index(DEBUG_BREAK), breaked);
	p->set_item_disabled(p->get_item_index(DEBUG_CONTINUE), !breaked);
}

void EditorDebuggerNode::_menu_option(int p_id) {
	switch (p_id) {
		case DEBUG_NEXT: {
			debug_next();
		} break;
		case DEBUG_STEP: {
			debug_step();
		} break;
		case DEBUG_BREAK: {
			debug_break();
		} break;
		case DEBUG_CONTINUE: {
			debug_continue();
		} break;
		case DEBUG_WITH_EXTERNAL_EDITOR: {
			bool ischecked = script_menu->get_popup()->is_item_checked(script_menu->get_popup()->get_item_index(DEBUG_WITH_EXTERNAL_EDITOR));
			debug_with_external_editor = !ischecked;
			script_menu->get_popup()->set_item_checked(script_menu->get_popup()->get_item_index(DEBUG_WITH_EXTERNAL_EDITOR), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "debug_with_external_editor", !ischecked);
			}
		} break;
	}
}

void EditorDebuggerNode::_update_debug_options() {
	if (EditorSettings::get_singleton()->get_project_metadata("debug_options", "debug_with_external_editor", false).operator bool()) {
		_menu_option(DEBUG_WITH_EXTERNAL_EDITOR);
	}
}

void EditorDebuggerNode::_paused() {
	const bool paused = EditorRunBar::get_singleton()->get_pause_button()->is_pressed();
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		if (paused && !dbg->is_breaked()) {
			dbg->debug_break();
		} else if (!paused && dbg->is_breaked()) {
			dbg->debug_continue();
		}
	});
}

void EditorDebuggerNode::_breaked(bool p_breaked, bool p_can_debug, const String &p_message, bool p_has_stackdump, int p_debugger) {
	if (get_current_debugger() != get_debugger(p_debugger)) {
		if (!p_breaked) {
			return;
		}
		tabs->set_current_tab(p_debugger);
	}
	_break_state_changed();
	EditorRunBar::get_singleton()->get_pause_button()->set_pressed(p_breaked);
	emit_signal(SNAME("breaked"), p_breaked, p_can_debug);
}

bool EditorDebuggerNode::is_skip_breakpoints() const {
	return get_current_debugger()->is_skip_breakpoints();
}

void EditorDebuggerNode::set_breakpoint(const String &p_path, int p_line, bool p_enabled) {
	breakpoints[Breakpoint(p_path, p_line)] = p_enabled;
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->set_breakpoint(p_path, p_line, p_enabled);
	});

	emit_signal(SNAME("breakpoint_toggled"), p_path, p_line, p_enabled);
}

void EditorDebuggerNode::set_breakpoints(const String &p_path, const Array &p_lines) {
	for (int i = 0; i < p_lines.size(); i++) {
		set_breakpoint(p_path, p_lines[i], true);
	}

	for (const KeyValue<Breakpoint, bool> &E : breakpoints) {
		Breakpoint b = E.key;
		if (b.source == p_path && !p_lines.has(b.line)) {
			set_breakpoint(p_path, b.line, false);
		}
	}
}

void EditorDebuggerNode::reload_all_scripts() {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->reload_all_scripts();
	});
}

void EditorDebuggerNode::reload_scripts(const Vector<String> &p_script_paths) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->reload_scripts(p_script_paths);
	});
}

void EditorDebuggerNode::debug_next() {
	get_current_debugger()->debug_next();
}

void EditorDebuggerNode::debug_step() {
	get_current_debugger()->debug_step();
}

void EditorDebuggerNode::debug_break() {
	get_current_debugger()->debug_break();
}

void EditorDebuggerNode::debug_continue() {
	get_current_debugger()->debug_continue();
}

String EditorDebuggerNode::get_var_value(const String &p_var) const {
	return get_current_debugger()->get_var_value(p_var);
}

// LiveEdit/Inspector
void EditorDebuggerNode::request_remote_tree() {
	get_current_debugger()->request_remote_tree();
}

void EditorDebuggerNode::_remote_tree_select_requested(ObjectID p_id, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	remote_scene_tree->select_node(p_id);
}

void EditorDebuggerNode::_remote_tree_updated(int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	remote_scene_tree->clear();
	remote_scene_tree->update_scene_tree(get_current_debugger()->get_remote_tree(), p_debugger);
}

void EditorDebuggerNode::_remote_tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	ERR_FAIL_NULL(item);

	if (p_id == EditorDebuggerTree::BUTTON_SUBSCENE) {
		remote_scene_tree->emit_signal(SNAME("open"), item->get_meta("scene_file_path"));
	} else if (p_id == EditorDebuggerTree::BUTTON_VISIBILITY) {
		ObjectID obj_id = item->get_metadata(0);
		ERR_FAIL_COND(obj_id.is_null());
		get_current_debugger()->update_remote_object(obj_id, "visible", !item->get_meta("visible"));
		get_current_debugger()->request_remote_tree();
	}
}

void EditorDebuggerNode::_remote_object_updated(ObjectID p_id, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	if (EditorDebuggerRemoteObject *obj = get_inspected_remote_object()) {
		if (obj->remote_object_id == p_id) {
			return; // Already being edited
		}
	}

	EditorNode::get_singleton()->push_item(get_current_debugger()->get_remote_object(p_id));
}

void EditorDebuggerNode::_remote_object_property_updated(ObjectID p_id, const String &p_property, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	if (EditorDebuggerRemoteObject *obj = get_inspected_remote_object()) {
		if (obj->remote_object_id != p_id) {
			return;
		}
		InspectorDock::get_inspector_singleton()->update_property(p_property);
	}
}

void EditorDebuggerNode::_remote_object_requested(ObjectID p_id, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	inspect_edited_object_timeout = 0.7; // Temporarily disable timeout to avoid multiple requests.
	get_current_debugger()->request_remote_object(p_id);
}

void EditorDebuggerNode::_save_node_requested(ObjectID p_id, const String &p_file, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}
	get_current_debugger()->save_node(p_id, p_file);
}

void EditorDebuggerNode::_breakpoint_set_in_tree(Ref<RefCounted> p_script, int p_line, bool p_enabled, int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}

	emit_signal(SNAME("breakpoint_set_in_tree"), p_script, p_line, p_enabled);
}

void EditorDebuggerNode::_breakpoints_cleared_in_tree(int p_debugger) {
	if (p_debugger != tabs->get_current_tab()) {
		return;
	}

	emit_signal(SNAME("breakpoints_cleared_in_tree"));
}

// Remote inspector/edit.
void EditorDebuggerNode::_methods_changed(void *p_ud, Object *p_base, const StringName &p_name, const Variant **p_args, int p_argcount) {
	if (!singleton) {
		return;
	}
	_for_all(singleton->tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->_method_changed(p_base, p_name, p_args, p_argcount);
	});
}

void EditorDebuggerNode::_properties_changed(void *p_ud, Object *p_base, const StringName &p_property, const Variant &p_value) {
	if (!singleton) {
		return;
	}
	_for_all(singleton->tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->_property_changed(p_base, p_property, p_value);
	});
}

// LiveDebug
void EditorDebuggerNode::set_live_debugging(bool p_enabled) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->set_live_debugging(p_enabled);
	});
}

void EditorDebuggerNode::update_live_edit_root() {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->update_live_edit_root();
	});
}

void EditorDebuggerNode::live_debug_create_node(const NodePath &p_parent, const String &p_type, const String &p_name) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_create_node(p_parent, p_type, p_name);
	});
}

void EditorDebuggerNode::live_debug_instantiate_node(const NodePath &p_parent, const String &p_path, const String &p_name) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_instantiate_node(p_parent, p_path, p_name);
	});
}

void EditorDebuggerNode::live_debug_remove_node(const NodePath &p_at) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_remove_node(p_at);
	});
}

void EditorDebuggerNode::live_debug_remove_and_keep_node(const NodePath &p_at, ObjectID p_keep_id) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_remove_and_keep_node(p_at, p_keep_id);
	});
}

void EditorDebuggerNode::live_debug_restore_node(ObjectID p_id, const NodePath &p_at, int p_at_pos) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_restore_node(p_id, p_at, p_at_pos);
	});
}

void EditorDebuggerNode::live_debug_duplicate_node(const NodePath &p_at, const String &p_new_name) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_duplicate_node(p_at, p_new_name);
	});
}

void EditorDebuggerNode::live_debug_reparent_node(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->live_debug_reparent_node(p_at, p_new_place, p_new_name, p_at_pos);
	});
}

void EditorDebuggerNode::set_camera_override(CameraOverride p_override) {
	_for_all(tabs, [&](ScriptEditorDebugger *dbg) {
		dbg->set_camera_override(p_override);
	});
	camera_override = p_override;
}

EditorDebuggerNode::CameraOverride EditorDebuggerNode::get_camera_override() {
	return camera_override;
}

void EditorDebuggerNode::add_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin) {
	ERR_FAIL_COND_MSG(p_plugin.is_null(), "Debugger plugin is null.");
	ERR_FAIL_COND_MSG(debugger_plugins.has(p_plugin), "Debugger plugin already exists.");
	debugger_plugins.insert(p_plugin);

	Ref<EditorDebuggerPlugin> plugin = p_plugin;
	for (int i = 0; get_debugger(i); i++) {
		plugin->create_session(get_debugger(i));
	}
}

void EditorDebuggerNode::remove_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin) {
	ERR_FAIL_COND_MSG(p_plugin.is_null(), "Debugger plugin is null.");
	ERR_FAIL_COND_MSG(!debugger_plugins.has(p_plugin), "Debugger plugin doesn't exists.");
	debugger_plugins.erase(p_plugin);
	Ref<EditorDebuggerPlugin>(p_plugin)->clear();
}

bool EditorDebuggerNode::plugins_capture(ScriptEditorDebugger *p_debugger, const String &p_message, const Array &p_data) {
	int session_index = tabs->get_tab_idx_from_control(p_debugger);
	ERR_FAIL_COND_V(session_index < 0, false);
	int colon_index = p_message.find_char(':');
	ERR_FAIL_COND_V_MSG(colon_index < 1, false, "Invalid message received.");

	const String cap = p_message.substr(0, colon_index);
	bool parsed = false;
	for (Ref<EditorDebuggerPlugin> plugin : debugger_plugins) {
		if (plugin->has_capture(cap)) {
			parsed |= plugin->capture(p_message, p_data, session_index);
		}
	}
	return parsed;
}
