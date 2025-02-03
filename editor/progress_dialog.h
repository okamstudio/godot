/**************************************************************************/
/*  progress_dialog.h                                                     */
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

#ifndef PROGRESS_DIALOG_H
#define PROGRESS_DIALOG_H

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/popup.h"
#include "scene/gui/progress_bar.h"

class BackgroundProgress : public HBoxContainer {
	GDCLASS(BackgroundProgress, HBoxContainer);

	_THREAD_SAFE_CLASS_

	struct Task {
		HBoxContainer *hb;
		ProgressBar *progress;
	};

	Map<String, Task> tasks;
	Map<String, int> updates;
	void _update();

protected:
	void _add_task(const String &p_task, const String &p_label, int p_steps);
	void _task_step(const String &p_task, int p_step = -1);
	void _end_task(const String &p_task);

	static void _bind_methods();

public:
	void add_task(const String &p_task, const String &p_label, int p_steps);
	void task_step(const String &p_task, int p_step = -1);
	void end_task(const String &p_task);

	BackgroundProgress() {}
};

class ProgressDialog : public Popup {
	GDCLASS(ProgressDialog, Popup);
	struct Task {
		String task;
		VBoxContainer *vb;
		ProgressBar *progress;
		Label *state;
		Label *time_info;
		uint64_t last_progress_tick;
		uint32_t time_started = 0;
	};
	HBoxContainer *cancel_hb;
	Button *cancel;

	Map<String, Task> tasks;
	VBoxContainer *main;

	Timer *time_info_timer;
	static ProgressDialog *singleton;
	void _popup();

	void _time_info_timer_timeout();
	void _cancel_pressed();
	String get_time_string(int p_seconds) const;
	bool cancelled;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static ProgressDialog *get_singleton() { return singleton; }
	void add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel = false);
	bool task_step(const String &p_task, const String &p_state, int p_step = -1, bool p_force_redraw = true);
	void end_task(const String &p_task);

	ProgressDialog();
};

#endif // PROGRESS_DIALOG_H
