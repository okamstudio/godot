/**************************************************************************/
/*  editor_run_bar.cpp                                                    */
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

#include "editor_run_bar.h"

#include "core/config/project_settings.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/editor_run_native.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/panel_container.h"

EditorRunBar *EditorRunBar::singleton = nullptr;

void EditorRunBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_reset_play_buttons();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_play_buttons();
			pause_button->set_icon(get_editor_theme_icon(SNAME("Pause")));
			stop_button->set_icon(get_editor_theme_icon(SNAME("Stop")));

			if (is_movie_maker_enabled()) {
				main_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadMovieMode"), EditorStringName(EditorStyles)));
				write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonPressed"), EditorStringName(EditorStyles)));
			} else {
				main_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadNormal"), EditorStringName(EditorStyles)));
				write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonNormal"), EditorStringName(EditorStyles)));
			}

			write_movie_button->set_icon(get_editor_theme_icon(SNAME("MainMovieWrite")));
			// This button behaves differently, so color it as such.
			write_movie_button->begin_bulk_theme_override();
			write_movie_button->add_theme_color_override("icon_normal_color", get_theme_color(SNAME("movie_writer_icon_normal"), EditorStringName(EditorStyles)));
			write_movie_button->add_theme_color_override("icon_pressed_color", get_theme_color(SNAME("movie_writer_icon_pressed"), EditorStringName(EditorStyles)));
			write_movie_button->add_theme_color_override("icon_hover_color", get_theme_color(SNAME("movie_writer_icon_hover"), EditorStringName(EditorStyles)));
			write_movie_button->add_theme_color_override("icon_hover_pressed_color", get_theme_color(SNAME("movie_writer_icon_hover_pressed"), EditorStringName(EditorStyles)));
			write_movie_button->end_bulk_theme_override();
		} break;
	}
}

void EditorRunBar::_reset_play_buttons() {
	play_button->set_pressed(false);
	play_button->set_icon(get_editor_theme_icon(SNAME("MainPlay")));
	play_button->set_tooltip_text(TTR("Play the project."));

	play_scene_button->set_pressed(false);
	play_scene_button->set_icon(get_editor_theme_icon(SNAME("PlayScene")));
	play_scene_button->set_tooltip_text(TTR("Play the edited scene."));

	play_custom_scene_button->set_pressed(false);
	play_custom_scene_button->set_icon(get_editor_theme_icon(SNAME("PlayCustom")));
	play_custom_scene_button->set_tooltip_text(TTR("Play a custom scene."));
}

void EditorRunBar::_update_play_buttons() {
	_reset_play_buttons();
	if (!is_playing()) {
		return;
	}

	Button *active_button = nullptr;
	if (current_mode == RUN_CURRENT) {
		active_button = play_scene_button;
	} else if (current_mode == RUN_CUSTOM) {
		active_button = play_custom_scene_button;
	} else {
		active_button = play_button;
	}

	if (active_button) {
		active_button->set_pressed(true);
		active_button->set_icon(get_editor_theme_icon(SNAME("Reload")));
		active_button->set_tooltip_text(TTR("Reload the played scene."));
	}
}

void EditorRunBar::_write_movie_toggled(bool p_enabled) {
	if (p_enabled) {
		add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadMovieMode"), EditorStringName(EditorStyles)));
		write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonPressed"), EditorStringName(EditorStyles)));
	} else {
		add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadNormal"), EditorStringName(EditorStyles)));
		write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonNormal"), EditorStringName(EditorStyles)));
	}
}

void EditorRunBar::_quick_run_selected(const String &p_file_path) {
	play_custom_scene(p_file_path);
}

void EditorRunBar::_play_custom_pressed() {
	if (editor_run.get_status() == EditorRun::STATUS_STOP || current_mode != RunMode::RUN_CUSTOM) {
		stop_playing();

		EditorNode::get_singleton()->get_quick_open_dialog()->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorRunBar::_quick_run_selected));
		play_custom_scene_button->set_pressed(false);
	} else {
		// Reload if already running a custom scene.
		String last_custom_scene = run_custom_filename; // This is necessary to have a copy of the string.
		play_custom_scene(last_custom_scene);
	}
}

void EditorRunBar::_play_current_pressed() {
	if (editor_run.get_status() == EditorRun::STATUS_STOP || current_mode != RunMode::RUN_CURRENT) {
		play_current_scene();
	} else {
		// Reload if already running the current scene.
		play_current_scene(true);
	}
}

void EditorRunBar::_run_scene(const String &p_scene_path) {
	ERR_FAIL_COND_MSG(current_mode == RUN_CUSTOM && p_scene_path.is_empty(), "Attempting to run a custom scene with an empty path.");

	if (editor_run.get_status() == EditorRun::STATUS_PLAY) {
		return;
	}

	_reset_play_buttons();

	String write_movie_file;
	if (is_movie_maker_enabled()) {
		if (current_mode == RUN_CURRENT) {
			Node *scene_root = nullptr;
			if (p_scene_path.is_empty()) {
				scene_root = get_tree()->get_edited_scene_root();
			} else {
				int scene_index = EditorNode::get_editor_data().get_edited_scene_from_path(p_scene_path);
				if (scene_index >= 0) {
					scene_root = EditorNode::get_editor_data().get_edited_scene_root(scene_index);
				}
			}

			if (scene_root && scene_root->has_meta("movie_file")) {
				// If the scene file has a movie_file metadata set, use this as file.
				// Quick workaround if you want to have multiple scenes that write to
				// multiple movies.
				write_movie_file = scene_root->get_meta("movie_file");
			}
		}

		if (write_movie_file.is_empty()) {
			write_movie_file = GLOBAL_GET("editor/movie_writer/movie_file");
		}

		if (write_movie_file.is_empty()) {
			// TODO: Provide options to directly resolve the issue with a custom dialog.
			EditorNode::get_singleton()->show_accept(TTR("Movie Maker mode is enabled, but no movie file path has been specified.\nA default movie file path can be specified in the project settings under the Editor > Movie Writer category.\nAlternatively, for running single scenes, a `movie_file` string metadata can be added to the root node,\nspecifying the path to a movie file that will be used when recording that scene."), TTR("OK"));
			return;
		}
	}

	String run_filename;
	switch (current_mode) {
		case RUN_CUSTOM: {
			run_filename = p_scene_path;
			run_custom_filename = run_filename;
		} break;

		case RUN_CURRENT: {
			if (!p_scene_path.is_empty()) {
				run_filename = p_scene_path;
				run_current_filename = run_filename;
				break;
			}

			Node *scene_root = get_tree()->get_edited_scene_root();
			if (!scene_root) {
				EditorNode::get_singleton()->show_accept(TTR("There is no defined scene to run."), TTR("OK"));
				return;
			}

			if (scene_root->get_scene_file_path().is_empty()) {
				EditorNode::get_singleton()->save_before_run();
				return;
			}

			run_filename = scene_root->get_scene_file_path();
			run_current_filename = run_filename;
		} break;

		default: {
			if (!EditorNode::get_singleton()->ensure_main_scene(false)) {
				return;
			}

			run_filename = GLOBAL_GET("application/run/main_scene");
		} break;
	}

	EditorNode::get_singleton()->try_autosave();
	if (!EditorNode::get_singleton()->call_build()) {
		return;
	}

	EditorDebuggerNode::get_singleton()->start();
	Error error = editor_run.run(run_filename, write_movie_file);
	if (error != OK) {
		EditorDebuggerNode::get_singleton()->stop();
		EditorNode::get_singleton()->show_accept(TTR("Could not start subprocess(es)!"), TTR("OK"));
		return;
	}

	_update_play_buttons();
	stop_button->set_disabled(false);

	emit_signal(SNAME("play_pressed"));
}

void EditorRunBar::_run_native(const Ref<EditorExportPreset> &p_preset) {
	EditorNode::get_singleton()->try_autosave();

	if (run_native->is_deploy_debug_remote_enabled()) {
		stop_playing();

		if (!EditorNode::get_singleton()->call_build()) {
			return; // Build failed.
		}

		EditorDebuggerNode::get_singleton()->start(p_preset->get_platform()->get_debug_protocol());
		emit_signal(SNAME("play_pressed"));
		editor_run.run_native_notify();
	}
}

void EditorRunBar::play_main_scene(bool p_from_native) {
	if (p_from_native) {
		run_native->resume_run_native();
	} else {
		stop_playing();

		current_mode = RunMode::RUN_MAIN;
		_run_scene();
	}
}

void EditorRunBar::play_current_scene(bool p_reload) {
	String last_current_scene = run_current_filename; // This is necessary to have a copy of the string.

	EditorNode::get_singleton()->save_default_environment();
	stop_playing();

	current_mode = RunMode::RUN_CURRENT;
	if (p_reload) {
		_run_scene(last_current_scene);
	} else {
		_run_scene();
	}
}

void EditorRunBar::play_custom_scene(const String &p_custom) {
	stop_playing();

	current_mode = RunMode::RUN_CUSTOM;
	_run_scene(p_custom);
}

void EditorRunBar::stop_playing() {
	if (editor_run.get_status() == EditorRun::STATUS_STOP) {
		return;
	}

	current_mode = RunMode::STOPPED;
	editor_run.stop();
	EditorDebuggerNode::get_singleton()->stop();

	run_custom_filename.clear();
	run_current_filename.clear();
	stop_button->set_pressed(false);
	stop_button->set_disabled(true);
	_reset_play_buttons();

	emit_signal(SNAME("stop_pressed"));
}

bool EditorRunBar::is_playing() const {
	EditorRun::Status status = editor_run.get_status();
	return (status == EditorRun::STATUS_PLAY || status == EditorRun::STATUS_PAUSED);
}

String EditorRunBar::get_playing_scene() const {
	String run_filename = editor_run.get_running_scene();
	if (run_filename.is_empty() && is_playing()) {
		run_filename = GLOBAL_GET("application/run/main_scene"); // Must be the main scene then.
	}

	return run_filename;
}

Error EditorRunBar::start_native_device(int p_device_id) {
	return run_native->start_run_native(p_device_id);
}

OS::ProcessID EditorRunBar::has_child_process(OS::ProcessID p_pid) const {
	return editor_run.has_child_process(p_pid);
}

void EditorRunBar::stop_child_process(OS::ProcessID p_pid) {
	if (!has_child_process(p_pid)) {
		return;
	}

	editor_run.stop_child_process(p_pid);
	if (!editor_run.get_child_process_count()) { // All children stopped. Closing.
		stop_playing();
	}
}

void EditorRunBar::set_movie_maker_enabled(bool p_enabled) {
	write_movie_button->set_pressed(p_enabled);
}

bool EditorRunBar::is_movie_maker_enabled() const {
	return write_movie_button->is_pressed();
}

HBoxContainer *EditorRunBar::get_buttons_container() {
	return main_hbox;
}

void EditorRunBar::_bind_methods() {
	ADD_SIGNAL(MethodInfo("play_pressed"));
	ADD_SIGNAL(MethodInfo("stop_pressed"));
}

EditorRunBar::EditorRunBar() {
	singleton = this;

	main_panel = memnew(PanelContainer);
	add_child(main_panel);

	main_hbox = memnew(HBoxContainer);
	main_panel->add_child(main_hbox);

	play_button = memnew(Button);
	main_hbox->add_child(play_button);
	play_button->set_theme_type_variation("RunBarButton");
	play_button->set_toggle_mode(true);
	play_button->set_focus_mode(Control::FOCUS_NONE);
	play_button->set_tooltip_text(TTR("Run the project's default scene."));
	play_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::play_main_scene).bind(false));

	ED_SHORTCUT_AND_COMMAND("editor/run_project", TTR("Run Project"), Key::F5);
	ED_SHORTCUT_OVERRIDE("editor/run_project", "macos", KeyModifierMask::META | Key::B);
	play_button->set_shortcut(ED_GET_SHORTCUT("editor/run_project"));

	pause_button = memnew(Button);
	main_hbox->add_child(pause_button);
	pause_button->set_theme_type_variation("RunBarButton");
	pause_button->set_toggle_mode(true);
	pause_button->set_focus_mode(Control::FOCUS_NONE);
	pause_button->set_tooltip_text(TTR("Pause the running project's execution for debugging."));
	pause_button->set_disabled(true);

	ED_SHORTCUT("editor/pause_running_project", TTR("Pause Running Project"), Key::F7);
	ED_SHORTCUT_OVERRIDE("editor/pause_running_project", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::Y);
	pause_button->set_shortcut(ED_GET_SHORTCUT("editor/pause_running_project"));

	stop_button = memnew(Button);
	main_hbox->add_child(stop_button);
	stop_button->set_theme_type_variation("RunBarButton");
	stop_button->set_focus_mode(Control::FOCUS_NONE);
	stop_button->set_tooltip_text(TTR("Stop the currently running project."));
	stop_button->set_disabled(true);
	stop_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::stop_playing));

	ED_SHORTCUT("editor/stop_running_project", TTR("Stop Running Project"), Key::F8);
	ED_SHORTCUT_OVERRIDE("editor/stop_running_project", "macos", KeyModifierMask::META | Key::PERIOD);
	stop_button->set_shortcut(ED_GET_SHORTCUT("editor/stop_running_project"));

	run_native = memnew(EditorRunNative);
	main_hbox->add_child(run_native);
	run_native->connect("native_run", callable_mp(this, &EditorRunBar::_run_native));

	play_scene_button = memnew(Button);
	main_hbox->add_child(play_scene_button);
	play_scene_button->set_theme_type_variation("RunBarButton");
	play_scene_button->set_toggle_mode(true);
	play_scene_button->set_focus_mode(Control::FOCUS_NONE);
	play_scene_button->set_tooltip_text(TTR("Run the currently edited scene."));
	play_scene_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_play_current_pressed));

	ED_SHORTCUT_AND_COMMAND("editor/run_current_scene", TTR("Run Current Scene"), Key::F6);
	ED_SHORTCUT_OVERRIDE("editor/run_current_scene", "macos", KeyModifierMask::META | Key::R);
	play_scene_button->set_shortcut(ED_GET_SHORTCUT("editor/run_current_scene"));

	play_custom_scene_button = memnew(Button);
	main_hbox->add_child(play_custom_scene_button);
	play_custom_scene_button->set_theme_type_variation("RunBarButton");
	play_custom_scene_button->set_toggle_mode(true);
	play_custom_scene_button->set_focus_mode(Control::FOCUS_NONE);
	play_custom_scene_button->set_tooltip_text(TTR("Run a specific scene."));
	play_custom_scene_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_play_custom_pressed));

	ED_SHORTCUT_AND_COMMAND("editor/run_specific_scene", TTR("Run Specific Scene"), KeyModifierMask::CTRL | KeyModifierMask::SHIFT | Key::F5);
	ED_SHORTCUT_OVERRIDE("editor/run_specific_scene", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::R);
	play_custom_scene_button->set_shortcut(ED_GET_SHORTCUT("editor/run_specific_scene"));

	write_movie_panel = memnew(PanelContainer);
	main_hbox->add_child(write_movie_panel);

	write_movie_button = memnew(Button);
	write_movie_panel->add_child(write_movie_button);
	write_movie_button->set_theme_type_variation("RunBarButton");
	write_movie_button->set_toggle_mode(true);
	write_movie_button->set_pressed(false);
	write_movie_button->set_focus_mode(Control::FOCUS_NONE);
	write_movie_button->set_tooltip_text(TTR("Enable Movie Maker mode.\nThe project will run at stable FPS and the visual and audio output will be recorded to a video file."));
	write_movie_button->connect(SceneStringName(toggled), callable_mp(this, &EditorRunBar::_write_movie_toggled));
}
