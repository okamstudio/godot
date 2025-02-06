/**************************************************************************/
/*  editor_run_bar.h                                                      */
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

#include "editor/editor_run.h"
#include "editor/export/editor_export.h"
#include "scene/gui/margin_container.h"

class Button;
class EditorRunNative;
class PanelContainer;
class HBoxContainer;
class AcceptDialog;

class EditorRunBar : public MarginContainer {
	GDCLASS(EditorRunBar, MarginContainer);

	static EditorRunBar *singleton;

	enum RunMode {
		STOPPED = 0,
		RUN_MAIN,
		RUN_CURRENT,
		RUN_CUSTOM,
	};

	PanelContainer *main_panel = nullptr;
	HBoxContainer *main_hbox = nullptr;
	HBoxContainer *outer_hbox = nullptr;

	Button *profiler_autostart_indicator = nullptr;

	PanelContainer *recovery_mode_panel = nullptr;
	Button *recovery_mode_button = nullptr;
	Button *recovery_mode_reload_button = nullptr;
	AcceptDialog *recovery_mode_popup = nullptr;

	Button *play_button = nullptr;
	Button *pause_button = nullptr;
	Button *stop_button = nullptr;
	Button *play_scene_button = nullptr;
	Button *play_custom_scene_button = nullptr;

	EditorRun editor_run;
	EditorRunNative *run_native = nullptr;

	PanelContainer *write_movie_panel = nullptr;
	Button *write_movie_button = nullptr;

	RunMode current_mode = RunMode::STOPPED;
	String run_custom_filename;
	String run_current_filename;

	void _reset_play_buttons();
	void _update_play_buttons();

	void _write_movie_toggled(bool p_enabled);
	void _quick_run_selected(const String &p_file_path, int p_id = -1);

	void _play_current_pressed(int p_id = -1);
	void _play_custom_pressed(int p_id = -1);

	void _run_scene(const String &p_scene_path = "", const Vector<String> &p_run_args = Vector<String>());
	void _run_native(const Ref<EditorExportPreset> &p_preset);

	void _profiler_autostart_indicator_pressed();

private:
	static Vector<String> _get_xr_mode_play_args(int p_xr_mode_id);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static EditorRunBar *get_singleton() { return singleton; }

	void recovery_mode_show_dialog();
	void recovery_mode_reload_project();

	void play_main_scene(bool p_from_native = false);
	void play_current_scene(bool p_reload = false, const Vector<String> &p_play_args = Vector<String>());
	void play_custom_scene(const String &p_custom, const Vector<String> &p_play_args = Vector<String>());

	void stop_playing();
	bool is_playing() const;
	String get_playing_scene() const;

	Error start_native_device(int p_device_id);

	OS::ProcessID has_child_process(OS::ProcessID p_pid) const;
	void stop_child_process(OS::ProcessID p_pid);
	OS::ProcessID get_current_process() const;

	void set_movie_maker_enabled(bool p_enabled);
	bool is_movie_maker_enabled() const;

	void update_profiler_autostart_indicator();

	Button *get_pause_button() { return pause_button; }

	HBoxContainer *get_buttons_container();

	EditorRunBar();
};
