/**************************************************************************/
/*  debugger_editor_plugin.cpp                                            */
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

#include "debugger_editor_plugin.h"

#include "core/os/keyboard.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/editor_debugger_server.h"
#include "editor/debugger/editor_file_server.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/run_instances_dialog.h"

DebuggerEditorPlugin::DebuggerEditorPlugin(PopupMenu *p_debug_menu) {
	EditorDebuggerServer::initialize();

	ED_SHORTCUT("debugger/step_into", TTRC("Step Into"), Key::F11);
	ED_SHORTCUT("debugger/step_over", TTRC("Step Over"), Key::F10);
	ED_SHORTCUT("debugger/break", TTRC("Break"));
	ED_SHORTCUT("debugger/continue", TTRC("Continue"), Key::F12);
	ED_SHORTCUT("debugger/debug_with_external_editor", TTRC("Debug with External Editor"));

	// File Server for deploy with remote filesystem.
	file_server = memnew(EditorFileServer);

	EditorDebuggerNode *debugger = memnew(EditorDebuggerNode);
	Button *db = EditorNode::get_bottom_panel()->add_item(TTR("Debugger"), debugger, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_debugger_bottom_panel", TTRC("Toggle Debugger Bottom Panel"), KeyModifierMask::ALT | Key::D));
	debugger->set_tool_button(db);

	// Main editor debug menu.
	debug_menu = p_debug_menu;
	debug_menu->set_hide_on_checkable_item_selection(false);
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/deploy_with_remote_debug", TTRC("Deploy with Remote Debug")), RUN_DEPLOY_REMOTE_DEBUG);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, using one-click deploy will make the executable attempt to connect to this computer's IP so the running project can be debugged.\nThis option is intended to be used for remote debugging (typically with a mobile device).\nYou don't need to enable it to use the GDScript debugger locally."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/small_deploy_with_network_fs", TTRC("Small Deploy with Network Filesystem")), RUN_FILE_SERVER);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, using one-click deploy for Android will only export an executable without the project data.\nThe filesystem will be provided from the project by the editor over the network.\nOn Android, deploying will use the USB cable for faster performance. This option speeds up testing for projects with large assets."));
	debug_menu->add_separator();
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/visible_collision_shapes", TTRC("Visible Collision Shapes")), RUN_DEBUG_COLLISIONS);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, collision shapes and raycast nodes (for 2D and 3D) will be visible in the running project."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/visible_paths", TTRC("Visible Paths")), RUN_DEBUG_PATHS);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, curve resources used by path nodes will be visible in the running project."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/visible_navigation", TTRC("Visible Navigation")), RUN_DEBUG_NAVIGATION);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, navigation meshes, and polygons will be visible in the running project."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/visible_avoidance", TTRC("Visible Avoidance")), RUN_DEBUG_AVOIDANCE);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, avoidance object shapes, radiuses, and velocities will be visible in the running project."));
	debug_menu->add_separator();
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/visible_canvas_redraw", TTRC("Debug CanvasItem Redraws")), RUN_DEBUG_CANVAS_REDRAW);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, redraw requests of 2D objects will become visible (as a short flash) in the running project.\nThis is useful to troubleshoot low processor mode."));
	debug_menu->add_separator();
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/sync_scene_changes", TTRC("Synchronize Scene Changes")), RUN_LIVE_DEBUG);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, any changes made to the scene in the editor will be replicated in the running project.\nWhen used remotely on a device, this is more efficient when the network filesystem option is enabled."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/sync_script_changes", TTRC("Synchronize Script Changes")), RUN_RELOAD_SCRIPTS);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, any script that is saved will be reloaded in the running project.\nWhen used remotely on a device, this is more efficient when the network filesystem option is enabled."));
	debug_menu->add_check_shortcut(ED_SHORTCUT("editor/keep_server_open", TTRC("Keep Debug Server Open")), SERVER_KEEP_OPEN);
	debug_menu->set_item_tooltip(-1,
			TTR("When this option is enabled, the editor debug server will stay open and listen for new sessions started outside of the editor itself."));

	// Multi-instance, start/stop.
	debug_menu->add_separator();
	debug_menu->add_item(TTR("Customize Run Instances..."), RUN_MULTIPLE_INSTANCES);
	debug_menu->connect(SceneStringName(id_pressed), callable_mp(this, &DebuggerEditorPlugin::_menu_option));

	run_instances_dialog = memnew(RunInstancesDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(run_instances_dialog);
}

DebuggerEditorPlugin::~DebuggerEditorPlugin() {
	EditorDebuggerServer::deinitialize();
	memdelete(file_server);
}

void DebuggerEditorPlugin::_menu_option(int p_option) {
	switch (p_option) {
		case RUN_FILE_SERVER: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_FILE_SERVER));

			if (ischecked) {
				file_server->stop();
				set_process(false);
			} else {
				file_server->start();
				set_process(true);
			}

			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_FILE_SERVER), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_file_server", !ischecked);
			}

		} break;
		case RUN_LIVE_DEBUG: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_LIVE_DEBUG));

			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_LIVE_DEBUG), !ischecked);
			EditorDebuggerNode::get_singleton()->set_live_debugging(!ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_live_debug", !ischecked);
			}

		} break;
		case RUN_DEPLOY_REMOTE_DEBUG: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEPLOY_REMOTE_DEBUG));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEPLOY_REMOTE_DEBUG), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_deploy_remote_debug", !ischecked);
			}

		} break;
		case RUN_DEBUG_COLLISIONS: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEBUG_COLLISIONS));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEBUG_COLLISIONS), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_collisions", !ischecked);
			}

		} break;
		case RUN_DEBUG_PATHS: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEBUG_PATHS));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEBUG_PATHS), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_paths", !ischecked);
			}

		} break;
		case RUN_DEBUG_NAVIGATION: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEBUG_NAVIGATION));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEBUG_NAVIGATION), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_navigation", !ischecked);
			}

		} break;
		case RUN_DEBUG_AVOIDANCE: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEBUG_AVOIDANCE));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEBUG_AVOIDANCE), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_avoidance", !ischecked);
			}

		} break;
		case RUN_DEBUG_CANVAS_REDRAW: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_DEBUG_CANVAS_REDRAW));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_DEBUG_CANVAS_REDRAW), !ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_canvas_redraw", !ischecked);
			}

		} break;
		case RUN_RELOAD_SCRIPTS: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(RUN_RELOAD_SCRIPTS));
			debug_menu->set_item_checked(debug_menu->get_item_index(RUN_RELOAD_SCRIPTS), !ischecked);

			ScriptEditor::get_singleton()->set_live_auto_reload_running_scripts(!ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_reload_scripts", !ischecked);
			}

		} break;
		case SERVER_KEEP_OPEN: {
			bool ischecked = debug_menu->is_item_checked(debug_menu->get_item_index(SERVER_KEEP_OPEN));
			debug_menu->set_item_checked(debug_menu->get_item_index(SERVER_KEEP_OPEN), !ischecked);

			EditorDebuggerNode::get_singleton()->set_keep_open(!ischecked);
			if (!initializing) {
				EditorSettings::get_singleton()->set_project_metadata("debug_options", "server_keep_open", !ischecked);
			}

		} break;
		case RUN_MULTIPLE_INSTANCES: {
			run_instances_dialog->popup_dialog();

		} break;
	}
}

void DebuggerEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_update_debug_options();
			initializing = false;
		} break;

		case NOTIFICATION_PROCESS: {
			file_server->poll();
		} break;
	}
}

void DebuggerEditorPlugin::_update_debug_options() {
	bool check_deploy_remote = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_deploy_remote_debug", true);
	bool check_file_server = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_file_server", false);
	bool check_debug_collisions = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_collisions", false);
	bool check_debug_paths = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_paths", false);
	bool check_debug_navigation = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_navigation", false);
	bool check_debug_avoidance = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_avoidance", false);
	bool check_debug_canvas_redraw = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_canvas_redraw", false);
	bool check_live_debug = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_live_debug", true);
	bool check_reload_scripts = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_reload_scripts", true);
	bool check_server_keep_open = EditorSettings::get_singleton()->get_project_metadata("debug_options", "server_keep_open", false);

	if (check_deploy_remote) {
		_menu_option(RUN_DEPLOY_REMOTE_DEBUG);
	}
	if (check_file_server) {
		_menu_option(RUN_FILE_SERVER);
	}
	if (check_debug_collisions) {
		_menu_option(RUN_DEBUG_COLLISIONS);
	}
	if (check_debug_paths) {
		_menu_option(RUN_DEBUG_PATHS);
	}
	if (check_debug_navigation) {
		_menu_option(RUN_DEBUG_NAVIGATION);
	}
	if (check_debug_avoidance) {
		_menu_option(RUN_DEBUG_AVOIDANCE);
	}
	if (check_debug_canvas_redraw) {
		_menu_option(RUN_DEBUG_CANVAS_REDRAW);
	}
	if (check_live_debug) {
		_menu_option(RUN_LIVE_DEBUG);
	}
	if (check_reload_scripts) {
		_menu_option(RUN_RELOAD_SCRIPTS);
	}
	if (check_server_keep_open) {
		_menu_option(SERVER_KEEP_OPEN);
	}
}
