/**************************************************************************/
/*  game_view_plugin.cpp                                                  */
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

#include "game_view_plugin.h"

#include "core/config/project_settings.h"
#include "core/debugger/debugger_marshalls.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/plugins/embedded_process.h"
#include "editor/themes/editor_scale.h"
#include "editor/window_wrapper.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/separator.h"

void GameViewDebugger::_session_started(Ref<EditorDebuggerSession> p_session) {
	if (!is_feature_enabled) {
		return;
	}

	Array setup_data;
	Dictionary settings;
	settings["editors/panning/2d_editor_panning_scheme"] = EDITOR_GET("editors/panning/2d_editor_panning_scheme");
	settings["editors/panning/simple_panning"] = EDITOR_GET("editors/panning/simple_panning");
	settings["editors/panning/warped_mouse_panning"] = EDITOR_GET("editors/panning/warped_mouse_panning");
	settings["editors/panning/2d_editor_pan_speed"] = EDITOR_GET("editors/panning/2d_editor_pan_speed");
	settings["canvas_item_editor/pan_view"] = DebuggerMarshalls::serialize_key_shortcut(ED_GET_SHORTCUT("canvas_item_editor/pan_view"));
	setup_data.append(settings);
	p_session->send_message("scene:runtime_node_select_setup", setup_data);

	Array type;
	type.append(node_type);
	p_session->send_message("scene:runtime_node_select_set_type", type);
	Array visible;
	visible.append(selection_visible);
	p_session->send_message("scene:runtime_node_select_set_visible", visible);
	Array mode;
	mode.append(select_mode);
	p_session->send_message("scene:runtime_node_select_set_mode", mode);

	emit_signal(SNAME("session_started"));
}

void GameViewDebugger::_session_stopped() {
	if (!is_feature_enabled) {
		return;
	}

	emit_signal(SNAME("session_stopped"));
}

void GameViewDebugger::set_is_feature_enabled(bool p_enabled) {
	is_feature_enabled = p_enabled;
}

void GameViewDebugger::set_suspend(bool p_enabled) {
	Array message;
	message.append(p_enabled);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:suspend_changed", message);
		}
	}
}

void GameViewDebugger::next_frame() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:next_frame", Array());
		}
	}
}

void GameViewDebugger::set_node_type(int p_type) {
	node_type = p_type;

	Array message;
	message.append(p_type);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_type", message);
		}
	}
}

void GameViewDebugger::set_selection_visible(bool p_visible) {
	selection_visible = p_visible;

	Array message;
	message.append(p_visible);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_visible", message);
		}
	}
}

void GameViewDebugger::set_select_mode(int p_mode) {
	select_mode = p_mode;

	Array message;
	message.append(p_mode);

	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_set_mode", message);
		}
	}
}

void GameViewDebugger::set_camera_override(bool p_enabled) {
	EditorDebuggerNode::get_singleton()->set_camera_override(p_enabled ? camera_override_mode : EditorDebuggerNode::OVERRIDE_NONE);
}

void GameViewDebugger::set_camera_manipulate_mode(EditorDebuggerNode::CameraOverride p_mode) {
	camera_override_mode = p_mode;

	if (EditorDebuggerNode::get_singleton()->get_camera_override() != EditorDebuggerNode::OVERRIDE_NONE) {
		set_camera_override(true);
	}
}

void GameViewDebugger::reset_camera_2d_position() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_reset_camera_2d", Array());
		}
	}
}

void GameViewDebugger::reset_camera_3d_position() {
	for (Ref<EditorDebuggerSession> &I : sessions) {
		if (I->is_active()) {
			I->send_message("scene:runtime_node_select_reset_camera_3d", Array());
		}
	}
}

void GameViewDebugger::setup_session(int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());

	sessions.append(session);

	session->connect("started", callable_mp(this, &GameViewDebugger::_session_started).bind(session));
	session->connect("stopped", callable_mp(this, &GameViewDebugger::_session_stopped));
}

void GameViewDebugger::_bind_methods() {
	ADD_SIGNAL(MethodInfo("session_started"));
	ADD_SIGNAL(MethodInfo("session_stopped"));
}

///////

void GameView::_sessions_changed() {
	// The debugger session's `session_started/stopped` signal can be unreliable, so count it manually.
	active_sessions = 0;
	Array sessions = debugger->get_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (Object::cast_to<EditorDebuggerSession>(sessions[i])->is_active()) {
			active_sessions++;
		}
	}

	_update_debugger_buttons();
}

void GameView::_instance_starting_static(int p_idx, List<String> &r_arguments) {
	ERR_FAIL_NULL(singleton);
	singleton->_instance_starting(p_idx, r_arguments);
}

void GameView::_instance_starting(int p_idx, List<String> &r_arguments) {
	if (!is_feature_enabled) {
		return;
	}
	if (p_idx == 0 && embed_on_play && make_floating_on_play && !window_wrapper->get_window_enabled() && EditorNode::get_singleton()->is_multi_window_enabled()) {
		window_wrapper->restore_window_from_saved_position(floating_window_rect, floating_window_screen, floating_window_screen_rect);
	}

	_update_arguments_for_instance(p_idx, r_arguments);
}

void GameView::_play_pressed() {
	if (!is_feature_enabled) {
		return;
	}

	OS::ProcessID current_process_id = EditorRunBar::get_singleton()->get_current_process();
	if (current_process_id == 0) {
		return;
	}

	if (!window_wrapper->get_window_enabled()) {
		screen_index_before_start = EditorNode::get_singleton()->get_editor_main_screen()->get_selected_index();
	}

	if (embed_on_play) {
		// It's important to disable the low power mode when unfocused because otherwise
		// the button in the editor are not responsive and if the user moves the mouse quickly,
		// the mouse clicks are not registered.
		EditorNode::get_singleton()->set_unfocused_low_processor_usage_mode_enabled(false);
		_update_embed_window_size();
		if (!window_wrapper->get_window_enabled()) {
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_GAME);
			embedded_process->grab_focus();
		}
		embedded_process->embed_process(current_process_id);
		_update_ui();
	}
}

void GameView::_stop_pressed() {
	if (!is_feature_enabled) {
		return;
	}

	EditorNode::get_singleton()->set_unfocused_low_processor_usage_mode_enabled(true);
	embedded_process->reset();
	_update_ui();

	if (window_wrapper->get_window_enabled()) {
		window_wrapper->set_window_enabled(false);
	}

	if (screen_index_before_start >= 0 && EditorNode::get_singleton()->get_editor_main_screen()->get_selected_index() == EditorMainScreen::EDITOR_GAME) {
		// We go back to the screen where the user was before starting the game.
		EditorNode::get_singleton()->get_editor_main_screen()->select(screen_index_before_start);
	}

	screen_index_before_start = -1;
}

void GameView::_embedding_completed() {
	_update_ui();
}

void GameView::_embedding_failed() {
	state_label->set_text(TTR("Connection impossible to the game process."));
}

void GameView::_embedded_process_updated() {
	const Rect2i game_rect = embedded_process->get_screen_embedded_window_rect();
	game_size_label->set_text(vformat("%dx%d", game_rect.size.x, game_rect.size.y));
}

void GameView::_embedded_process_focused() {
	if (embed_on_play && !window_wrapper->get_window_enabled()) {
		EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_GAME);
	}
}

void GameView::_project_settings_changed() {
	// Update the window size and aspect ratio.
	_update_embed_window_size();
}

void GameView::_update_debugger_buttons() {
	bool empty = active_sessions == 0;

	suspend_button->set_disabled(empty);
	camera_override_button->set_disabled(empty);

	PopupMenu *menu = camera_override_menu->get_popup();

	bool disable_camera_reset = empty || !camera_override_button->is_pressed() || !menu->is_item_checked(menu->get_item_index(CAMERA_MODE_INGAME));
	menu->set_item_disabled(CAMERA_RESET_2D, disable_camera_reset);
	menu->set_item_disabled(CAMERA_RESET_3D, disable_camera_reset);

	if (empty) {
		suspend_button->set_pressed(false);
		camera_override_button->set_pressed(false);
	}
	next_frame_button->set_disabled(!suspend_button->is_pressed());
}

void GameView::_suspend_button_toggled(bool p_pressed) {
	_update_debugger_buttons();

	debugger->set_suspend(p_pressed);
}

void GameView::_node_type_pressed(int p_option) {
	RuntimeNodeSelect::NodeType type = (RuntimeNodeSelect::NodeType)p_option;
	for (int i = 0; i < RuntimeNodeSelect::NODE_TYPE_MAX; i++) {
		node_type_button[i]->set_pressed_no_signal(i == type);
	}

	_update_debugger_buttons();

	debugger->set_node_type(type);
}

void GameView::_select_mode_pressed(int p_option) {
	RuntimeNodeSelect::SelectMode mode = (RuntimeNodeSelect::SelectMode)p_option;
	for (int i = 0; i < RuntimeNodeSelect::SELECT_MODE_MAX; i++) {
		select_mode_button[i]->set_pressed_no_signal(i == mode);
	}

	debugger->set_select_mode(mode);
}

void GameView::_embed_options_menu_menu_id_pressed(int p_id) {
	switch (p_id) {
		case EMBED_RUN_GAME_EMBEDDED: {
			embed_on_play = !embed_on_play;
			EditorSettings::get_singleton()->set_project_metadata("game_view", "embed_on_play", embed_on_play);
		} break;
		case EMBED_MAKE_FLOATING_ON_PLAY: {
			make_floating_on_play = !make_floating_on_play;
			EditorSettings::get_singleton()->set_project_metadata("game_view", "make_floating_on_play", make_floating_on_play);
		} break;
	}
	_update_embed_menu_options();
}

void GameView::_keep_aspect_button_pressed() {
	embedded_process->set_keep_aspect(keep_aspect_button->is_pressed());
}

void GameView::_update_ui() {
	bool show_game_size = false;
	if (!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
		state_label->set_text(TTR("Game embedding not available on your OS."));
	} else if (embedded_process->is_embedding_completed()) {
		state_label->set_text("");
		show_game_size = true;
	} else if (embedded_process->is_embedding_in_progress()) {
		state_label->set_text(TTR("Game starting..."));
	} else if (EditorRunBar::get_singleton()->is_playing()) {
		state_label->set_text(TTR("Game running not embedded."));
	} else if (embed_on_play) {
		state_label->set_text(TTR("Press play to start the game."));
	} else {
		state_label->set_text(TTR("Embedding is disabled."));
	}

	game_size_label->set_visible(show_game_size);
}

void GameView::_update_embed_menu_options() {
	PopupMenu *menu = embed_options_menu->get_popup();
	menu->set_item_checked(menu->get_item_index(EMBED_RUN_GAME_EMBEDDED), embed_on_play);
	menu->set_item_checked(menu->get_item_index(EMBED_MAKE_FLOATING_ON_PLAY), make_floating_on_play);

	// When embed is Off or in single window mode, Make floating is not available.
	menu->set_item_disabled(menu->get_item_index(EMBED_MAKE_FLOATING_ON_PLAY), !embed_on_play || !EditorNode::get_singleton()->is_multi_window_enabled());
}

void GameView::_update_embed_window_size() {
	Size2 window_size;
	window_size.x = GLOBAL_GET("display/window/size/viewport_width");
	window_size.y = GLOBAL_GET("display/window/size/viewport_height");

	Size2 desired_size;
	desired_size.x = GLOBAL_GET("display/window/size/window_width_override");
	desired_size.y = GLOBAL_GET("display/window/size/window_height_override");
	if (desired_size.x > 0 && desired_size.y > 0) {
		window_size = desired_size;
	}
	embedded_process->set_window_size(window_size);
}

void GameView::_hide_selection_toggled(bool p_pressed) {
	hide_selection->set_button_icon(get_editor_theme_icon(p_pressed ? SNAME("GuiVisibilityHidden") : SNAME("GuiVisibilityVisible")));

	debugger->set_selection_visible(!p_pressed);
}

void GameView::_camera_override_button_toggled(bool p_pressed) {
	_update_debugger_buttons();

	debugger->set_camera_override(p_pressed);
}

void GameView::_camera_override_menu_id_pressed(int p_id) {
	PopupMenu *menu = camera_override_menu->get_popup();
	if (p_id != CAMERA_RESET_2D && p_id != CAMERA_RESET_3D) {
		for (int i = 0; i < menu->get_item_count(); i++) {
			menu->set_item_checked(i, false);
		}
	}

	switch (p_id) {
		case CAMERA_RESET_2D: {
			debugger->reset_camera_2d_position();
		} break;
		case CAMERA_RESET_3D: {
			debugger->reset_camera_3d_position();
		} break;
		case CAMERA_MODE_INGAME: {
			debugger->set_camera_manipulate_mode(EditorDebuggerNode::OVERRIDE_INGAME);
			menu->set_item_checked(menu->get_item_index(p_id), true);

			_update_debugger_buttons();
		} break;
		case CAMERA_MODE_EDITORS: {
			debugger->set_camera_manipulate_mode(EditorDebuggerNode::OVERRIDE_EDITORS);
			menu->set_item_checked(menu->get_item_index(p_id), true);

			_update_debugger_buttons();
		} break;
	}
}

void GameView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			suspend_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
			next_frame_button->set_button_icon(get_editor_theme_icon(SNAME("NextFrame")));

			node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_button_icon(get_editor_theme_icon(SNAME("InputEventJoypadMotion")));
			node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_button_icon(get_editor_theme_icon(SNAME("2DNodes")));
#ifndef _3D_DISABLED
			node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_button_icon(get_editor_theme_icon(SNAME("Node3D")));
#endif // _3D_DISABLED

			select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_button_icon(get_editor_theme_icon(SNAME("ToolSelect")));
			select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_button_icon(get_editor_theme_icon(SNAME("ListSelect")));

			hide_selection->set_button_icon(get_editor_theme_icon(hide_selection->is_pressed() ? SNAME("GuiVisibilityHidden") : SNAME("GuiVisibilityVisible")));
			keep_aspect_button->set_button_icon(get_editor_theme_icon(SNAME("KeepAspect")));
			embed_options_menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));

			camera_override_button->set_button_icon(get_editor_theme_icon(SNAME("Camera")));
			camera_override_menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
		} break;

		case NOTIFICATION_READY: {
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
				// Embedding available.
				embed_on_play = EditorSettings::get_singleton()->get_project_metadata("game_view", "embed_on_play", true);
				make_floating_on_play = EditorSettings::get_singleton()->get_project_metadata("game_view", "make_floating_on_play", true);
				keep_aspect_button->set_pressed(EditorSettings::get_singleton()->get_project_metadata("game_view", "keep_aspect", true));
				_update_embed_menu_options();

				EditorRunBar::get_singleton()->connect("play_pressed", callable_mp(this, &GameView::_play_pressed));
				EditorRunBar::get_singleton()->connect("stop_pressed", callable_mp(this, &GameView::_stop_pressed));
				EditorRun::instance_starting_callback = _instance_starting_static;

				// Listen for project settings changes to update the window size and aspect ratio.
				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &GameView::_project_settings_changed));

				embedded_process->set_keep_aspect(keep_aspect_button->is_pressed());
			} else {
				// Embedding not available.
				embedding_separator->hide();
				embed_options_menu->hide();
				keep_aspect_button->hide();
				keep_aspect_button->hide();
			}

			_update_ui();
		} break;
		case NOTIFICATION_WM_POSITION_CHANGED: {
			if (window_wrapper->get_window_enabled()) {
				_update_floating_window_settings();
			}
		} break;
	}
}

void GameView::set_is_feature_enabled(bool p_enabled) {
	is_feature_enabled = p_enabled;
}

void GameView::set_state(const Dictionary &p_state) {
	if (p_state.has("hide_selection")) {
		hide_selection->set_pressed(p_state["hide_selection"]);
		_hide_selection_toggled(hide_selection->is_pressed());
	}
	if (p_state.has("select_mode")) {
		_select_mode_pressed(p_state["select_mode"]);
	}
	if (p_state.has("camera_override_mode")) {
		_camera_override_menu_id_pressed(p_state["camera_override_mode"]);
	}
}

Dictionary GameView::get_state() const {
	Dictionary d;
	d["hide_selection"] = hide_selection->is_pressed();

	for (int i = 0; i < RuntimeNodeSelect::SELECT_MODE_MAX; i++) {
		if (select_mode_button[i]->is_pressed()) {
			d["select_mode"] = i;
			break;
		}
	}

	PopupMenu *menu = camera_override_menu->get_popup();
	for (int i = CAMERA_MODE_INGAME; i < CAMERA_MODE_EDITORS + 1; i++) {
		if (menu->is_item_checked(menu->get_item_index(i))) {
			d["camera_override_mode"] = i;
			break;
		}
	}

	return d;
}

void GameView::set_window_layout(Ref<ConfigFile> p_layout) {
	floating_window_rect = p_layout->get_value("GameView", "floating_window_rect", Rect2i());
	floating_window_screen = p_layout->get_value("GameView", "floating_window_screen", -1);
	floating_window_screen_rect = p_layout->get_value("GameView", "floating_window_screen_rect", Rect2i());
}

void GameView::get_window_layout(Ref<ConfigFile> p_layout) {
	if (window_wrapper->get_window_enabled()) {
		_update_floating_window_settings();
	}

	p_layout->set_value("GameView", "floating_window_rect", floating_window_rect);
	p_layout->set_value("GameView", "floating_window_screen", floating_window_screen);
	p_layout->set_value("GameView", "floating_window_screen_rect", floating_window_screen_rect);
}

void GameView::_update_floating_window_settings() {
	if (window_wrapper->get_window_enabled()) {
		floating_window_rect = window_wrapper->get_window_rect();
		floating_window_screen = window_wrapper->get_window_screen();
		floating_window_screen_rect = DisplayServer::get_singleton()->screen_get_usable_rect(floating_window_screen);
	}
}

void GameView::_update_arguments_for_instance(int p_idx, List<String> &r_arguments) {
	if (p_idx != 0 || !embed_on_play || !DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING)) {
		return;
	}

	// Remove duplicates/unwanted parameters.
	List<String>::Element *E = r_arguments.front();
	while (E) {
		List<String>::Element *N = E->next();

		//For these parameters, we need to also renove the value.
		if (E->get() == "--position" || E->get() == "--resolution" || E->get() == "--screen") {
			r_arguments.erase(E);
			if (N) {
				List<String>::Element *V = N->next();
				r_arguments.erase(N);
				N = V;
			}
		} else if (E->get() == "-f" || E->get() == "--fullscreen" || E->get() == "-m" || E->get() == "--maximized" || E->get() == "-t" || E->get() == "-always-on-top") {
			r_arguments.erase(E);
		}

		E = N;
	}

	// Add the editor window's native ID so the started game can directly set it as its parent.
	r_arguments.push_back("--wid");
	r_arguments.push_back(itos(DisplayServer::get_singleton()->window_get_native_handle(DisplayServer::WINDOW_HANDLE, get_window()->get_window_id())));

	// Be sure to have the correct window size in the embedded_process control.
	_update_embed_window_size();

	Rect2i rect = embedded_process->get_screen_embedded_window_rect();
	r_arguments.push_back("--position");
	r_arguments.push_back(itos(rect.position.x) + "," + itos(rect.position.y));
	r_arguments.push_back("--resolution");
	r_arguments.push_back(itos(rect.size.x) + "x" + itos(rect.size.y));
}

void GameView::_window_before_closing() {
	// Before the parent window closed, we close the embedded game. That prevents
	// the embedded game to be seen without a parent window for a fraction of second.
	if (EditorRunBar::get_singleton()->is_playing() && (embedded_process->is_embedding_completed() || embedded_process->is_embedding_in_progress())) {
		embedded_process->reset();
		// Call deferred to prevent the _stop_pressed callback to be executed before the wrapper window
		// actually closes.
		callable_mp(EditorRunBar::get_singleton(), &EditorRunBar::stop_playing).call_deferred();
	}
}

GameView::GameView(Ref<GameViewDebugger> p_debugger, WindowWrapper *p_wrapper) {
	singleton = this;

	debugger = p_debugger;
	window_wrapper = p_wrapper;

	// Add some margin to the sides for better aesthetics.
	// This prevents the first button's hover/pressed effect from "touching" the panel's border,
	// which looks ugly.
	MarginContainer *toolbar_margin = memnew(MarginContainer);
	toolbar_margin->add_theme_constant_override("margin_left", 4 * EDSCALE);
	toolbar_margin->add_theme_constant_override("margin_right", 4 * EDSCALE);
	add_child(toolbar_margin);

	HBoxContainer *main_menu_hbox = memnew(HBoxContainer);
	toolbar_margin->add_child(main_menu_hbox);

	suspend_button = memnew(Button);
	main_menu_hbox->add_child(suspend_button);
	suspend_button->set_toggle_mode(true);
	suspend_button->set_theme_type_variation(SceneStringName(FlatButton));
	suspend_button->connect(SceneStringName(toggled), callable_mp(this, &GameView::_suspend_button_toggled));
	suspend_button->set_tooltip_text(TTR("Suspend"));

	next_frame_button = memnew(Button);
	main_menu_hbox->add_child(next_frame_button);
	next_frame_button->set_theme_type_variation(SceneStringName(FlatButton));
	next_frame_button->connect(SceneStringName(pressed), callable_mp(*debugger, &GameViewDebugger::next_frame));
	next_frame_button->set_tooltip_text(TTR("Next Frame"));

	main_menu_hbox->add_child(memnew(VSeparator));

	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_text(TTR("Input"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_pressed(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_theme_type_variation(SceneStringName(FlatButton));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->connect(SceneStringName(pressed), callable_mp(this, &GameView::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_NONE));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_NONE]->set_tooltip_text(TTR("Allow game input."));

	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_text(TTR("2D"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_theme_type_variation(SceneStringName(FlatButton));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->connect(SceneStringName(pressed), callable_mp(this, &GameView::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_2D));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_2D]->set_tooltip_text(TTR("Disable game input and allow to select Node2Ds, Controls, and manipulate the 2D camera."));

#ifndef _3D_DISABLED
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D] = memnew(Button);
	main_menu_hbox->add_child(node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_text(TTR("3D"));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_toggle_mode(true);
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_theme_type_variation(SceneStringName(FlatButton));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->connect(SceneStringName(pressed), callable_mp(this, &GameView::_node_type_pressed).bind(RuntimeNodeSelect::NODE_TYPE_3D));
	node_type_button[RuntimeNodeSelect::NODE_TYPE_3D]->set_tooltip_text(TTR("Disable game input and allow to select Node3Ds and manipulate the 3D camera."));
#endif // _3D_DISABLED

	main_menu_hbox->add_child(memnew(VSeparator));

	hide_selection = memnew(Button);
	main_menu_hbox->add_child(hide_selection);
	hide_selection->set_toggle_mode(true);
	hide_selection->set_theme_type_variation(SceneStringName(FlatButton));
	hide_selection->connect(SceneStringName(toggled), callable_mp(this, &GameView::_hide_selection_toggled));
	hide_selection->set_tooltip_text(TTR("Toggle Selection Visibility"));

	main_menu_hbox->add_child(memnew(VSeparator));

	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE] = memnew(Button);
	main_menu_hbox->add_child(select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_toggle_mode(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_pressed(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_theme_type_variation(SceneStringName(FlatButton));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->connect(SceneStringName(pressed), callable_mp(this, &GameView::_select_mode_pressed).bind(RuntimeNodeSelect::SELECT_MODE_SINGLE));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_select", TTRC("Select Mode"), Key::Q));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_shortcut_context(this);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_SINGLE]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Alt+RMB: Show list of all nodes at position clicked."));

	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST] = memnew(Button);
	main_menu_hbox->add_child(select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_toggle_mode(true);
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_theme_type_variation(SceneStringName(FlatButton));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->connect(SceneStringName(pressed), callable_mp(this, &GameView::_select_mode_pressed).bind(RuntimeNodeSelect::SELECT_MODE_LIST));
	select_mode_button[RuntimeNodeSelect::SELECT_MODE_LIST]->set_tooltip_text(TTR("Show list of selectable nodes at position clicked."));

	main_menu_hbox->add_child(memnew(VSeparator));

	camera_override_button = memnew(Button);
	main_menu_hbox->add_child(camera_override_button);
	camera_override_button->set_toggle_mode(true);
	camera_override_button->set_theme_type_variation(SceneStringName(FlatButton));
	camera_override_button->connect(SceneStringName(toggled), callable_mp(this, &GameView::_camera_override_button_toggled));
	camera_override_button->set_tooltip_text(TTR("Override the in-game camera."));

	camera_override_menu = memnew(MenuButton);
	main_menu_hbox->add_child(camera_override_menu);
	camera_override_menu->set_flat(false);
	camera_override_menu->set_theme_type_variation("FlatMenuButton");
	camera_override_menu->set_h_size_flags(SIZE_SHRINK_END);
	camera_override_menu->set_tooltip_text(TTR("Camera Override Options"));

	PopupMenu *menu = camera_override_menu->get_popup();
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &GameView::_camera_override_menu_id_pressed));
	menu->add_item(TTR("Reset 2D Camera"), CAMERA_RESET_2D);
	menu->add_item(TTR("Reset 3D Camera"), CAMERA_RESET_3D);
	menu->add_separator();
	menu->add_radio_check_item(TTR("Manipulate In-Game"), CAMERA_MODE_INGAME);
	menu->set_item_checked(menu->get_item_index(CAMERA_MODE_INGAME), true);
	menu->add_radio_check_item(TTR("Manipulate From Editors"), CAMERA_MODE_EDITORS);

	embedding_separator = memnew(VSeparator);
	main_menu_hbox->add_child(embedding_separator);

	keep_aspect_button = memnew(Button);
	main_menu_hbox->add_child(keep_aspect_button);
	keep_aspect_button->set_toggle_mode(true);
	keep_aspect_button->set_theme_type_variation("FlatButton");
	keep_aspect_button->set_tooltip_text(TTR("Keep the aspect ratio of the embedded game."));
	keep_aspect_button->connect(SceneStringName(pressed), callable_mp(this, &GameView::_keep_aspect_button_pressed));

	embed_options_menu = memnew(MenuButton);
	main_menu_hbox->add_child(embed_options_menu);
	embed_options_menu->set_flat(false);
	embed_options_menu->set_theme_type_variation("FlatMenuButton");
	embed_options_menu->set_h_size_flags(SIZE_SHRINK_END);
	embed_options_menu->set_tooltip_text(TTR("Embedding Options"));

	menu = embed_options_menu->get_popup();
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &GameView::_embed_options_menu_menu_id_pressed));
	menu->add_check_item(TTR("Embed Game on Next Play"), EMBED_RUN_GAME_EMBEDDED);
	menu->add_check_item(TTR("Make Game Workspace Floating on Next Play"), EMBED_MAKE_FLOATING_ON_PLAY);

	main_menu_hbox->add_spacer();

	game_size_label = memnew(Label());
	main_menu_hbox->add_child(game_size_label);
	game_size_label->hide();

	panel = memnew(Panel);
	add_child(panel);
	panel->set_theme_type_variation("GamePanel");
	panel->set_v_size_flags(SIZE_EXPAND_FILL);

	embedded_process = memnew(EmbeddedProcess);
	panel->add_child(embedded_process);
	embedded_process->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	embedded_process->connect("embedding_failed", callable_mp(this, &GameView::_embedding_failed));
	embedded_process->connect("embedding_completed", callable_mp(this, &GameView::_embedding_completed));
	embedded_process->connect("embedded_process_updated", callable_mp(this, &GameView::_embedded_process_updated));
	embedded_process->connect("embedded_process_focused", callable_mp(this, &GameView::_embedded_process_focused));
	embedded_process->set_custom_minimum_size(Size2i(100, 100));

	state_label = memnew(Label());
	panel->add_child(state_label);
	state_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	state_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	state_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	state_label->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	_update_debugger_buttons();

	p_debugger->connect("session_started", callable_mp(this, &GameView::_sessions_changed));
	p_debugger->connect("session_stopped", callable_mp(this, &GameView::_sessions_changed));

	p_wrapper->connect("window_before_closing", callable_mp(this, &GameView::_window_before_closing));
	p_wrapper->connect("window_size_changed", callable_mp(this, &GameView::_update_floating_window_settings));
}

///////

void GameViewPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		window_wrapper->show();
	} else {
		window_wrapper->hide();
	}
}

void GameViewPlugin::selected_notify() {
	if (window_wrapper->get_window_enabled()) {
		window_wrapper->grab_window_focus();
		_focus_another_editor();
	}
}

void GameViewPlugin::set_window_layout(Ref<ConfigFile> p_layout) {
	game_view->set_window_layout(p_layout);
}

void GameViewPlugin::get_window_layout(Ref<ConfigFile> p_layout) {
	game_view->get_window_layout(p_layout);
}

void GameViewPlugin::set_state(const Dictionary &p_state) {
	game_view->set_state(p_state);
}

Dictionary GameViewPlugin::get_state() const {
	return game_view->get_state();
}

void GameViewPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_debugger_plugin(debugger);
			connect("main_screen_changed", callable_mp(this, &GameViewPlugin::_save_last_editor));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_debugger_plugin(debugger);
			disconnect("main_screen_changed", callable_mp(this, &GameViewPlugin::_save_last_editor));
		} break;
	}
}

void GameViewPlugin::_feature_profile_changed() {
	bool is_feature_enabled = true;
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_valid()) {
		is_feature_enabled = !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_GAME);
	}

	if (debugger.is_valid()) {
		debugger->set_is_feature_enabled(is_feature_enabled);
	}

	if (game_view) {
		game_view->set_is_feature_enabled(is_feature_enabled);
	}
}

void GameViewPlugin::_window_visibility_changed(bool p_visible) {
	_focus_another_editor();
}

void GameViewPlugin::_save_last_editor(const String &p_editor) {
	if (p_editor != get_name()) {
		last_editor = p_editor;
	}
}

void GameViewPlugin::_focus_another_editor() {
	if (window_wrapper->get_window_enabled()) {
		ERR_FAIL_COND(last_editor.is_empty());
		EditorInterface::get_singleton()->set_main_screen_editor(last_editor);
	}
}

GameViewPlugin::GameViewPlugin() {
	window_wrapper = memnew(WindowWrapper);
	window_wrapper->set_window_title(vformat(TTR("%s - Godot Engine"), TTR("Game Workspace")));
	window_wrapper->set_margins_enabled(true);

	debugger.instantiate();

	game_view = memnew(GameView(debugger, window_wrapper));
	game_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	window_wrapper->set_wrapped_control(game_view, nullptr);

	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(window_wrapper);
	window_wrapper->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	window_wrapper->hide();
	window_wrapper->connect("window_visibility_changed", callable_mp(this, &GameViewPlugin::_window_visibility_changed));

	EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", callable_mp(this, &GameViewPlugin::_feature_profile_changed));
}

GameViewPlugin::~GameViewPlugin() {
}
