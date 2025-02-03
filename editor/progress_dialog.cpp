/**************************************************************************/
/*  progress_dialog.cpp                                                   */
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

#include "progress_dialog.h"

#include "core/message_queue.h"
#include "core/os/os.h"
#include "editor_scale.h"
#include "main/main.h"

void BackgroundProgress::_add_task(const String &p_task, const String &p_label, int p_steps) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_MSG(tasks.has(p_task), "Task '" + p_task + "' already exists.");
	BackgroundProgress::Task t;
	t.hb = memnew(HBoxContainer);
	Label *l = memnew(Label);
	l->set_text(p_label + " ");
	t.hb->add_child(l);
	t.progress = memnew(ProgressBar);
	t.progress->set_max(p_steps);
	t.progress->set_value(p_steps);
	Control *ec = memnew(Control);
	ec->set_h_size_flags(SIZE_EXPAND_FILL);
	ec->set_v_size_flags(SIZE_EXPAND_FILL);
	t.progress->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	ec->add_child(t.progress);
	ec->set_custom_minimum_size(Size2(80, 5) * EDSCALE);
	t.hb->add_child(ec);

	add_child(t.hb);

	tasks[p_task] = t;
}

void BackgroundProgress::_update() {
	_THREAD_SAFE_METHOD_

	for (Map<String, int>::Element *E = updates.front(); E; E = E->next()) {
		if (tasks.has(E->key())) {
			_task_step(E->key(), E->get());
		}
	}

	updates.clear();
}

void BackgroundProgress::_task_step(const String &p_task, int p_step) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!tasks.has(p_task));

	Task &t = tasks[p_task];
	if (p_step < 0) {
		t.progress->set_value(t.progress->get_value() + 1);
	} else {
		t.progress->set_value(p_step);
	}
}
void BackgroundProgress::_end_task(const String &p_task) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!tasks.has(p_task));
	Task &t = tasks[p_task];

	memdelete(t.hb);
	tasks.erase(p_task);
}

void BackgroundProgress::_bind_methods() {
	ClassDB::bind_method("_add_task", &BackgroundProgress::_add_task);
	ClassDB::bind_method("_task_step", &BackgroundProgress::_task_step);
	ClassDB::bind_method("_end_task", &BackgroundProgress::_end_task);
	ClassDB::bind_method("_update", &BackgroundProgress::_update);
}

void BackgroundProgress::add_task(const String &p_task, const String &p_label, int p_steps) {
	MessageQueue::get_singleton()->push_call(this, "_add_task", p_task, p_label, p_steps);
}
void BackgroundProgress::task_step(const String &p_task, int p_step) {
	//this code is weird, but it prevents deadlock.
	bool no_updates = true;
	{
		_THREAD_SAFE_METHOD_
		no_updates = updates.empty();
	}

	if (no_updates) {
		MessageQueue::get_singleton()->push_call(this, "_update");
	}

	{
		_THREAD_SAFE_METHOD_
		updates[p_task] = p_step;
	}
}

void BackgroundProgress::end_task(const String &p_task) {
	MessageQueue::get_singleton()->push_call(this, "_end_task", p_task);
}

////////////////////////////////////////////////

ProgressDialog *ProgressDialog::singleton = nullptr;

void ProgressDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			Ref<StyleBox> style = get_stylebox("panel", "PopupMenu");
			draw_style_box(style, Rect2(Point2(), get_size()));

		} break;
	}
}

void ProgressDialog::_popup() {
	Size2 ms = main->get_combined_minimum_size();
	ms.width = MAX(500 * EDSCALE, ms.width);

	Ref<StyleBox> style = get_stylebox("panel", "PopupMenu");
	ms += style->get_minimum_size();
	main->set_margin(MARGIN_LEFT, style->get_margin(MARGIN_LEFT));
	main->set_margin(MARGIN_RIGHT, -style->get_margin(MARGIN_RIGHT));
	main->set_margin(MARGIN_TOP, style->get_margin(MARGIN_TOP));
	main->set_margin(MARGIN_BOTTOM, -style->get_margin(MARGIN_BOTTOM));

	raise();
	popup_centered(ms);
}

void ProgressDialog::add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {
	if (MessageQueue::get_singleton()->is_flushing()) {
		ERR_PRINT("Do not use progress dialog (task) while flushing the message queue or using call_deferred()!");
		return;
	}

	ERR_FAIL_COND_MSG(tasks.has(p_task), "Task '" + p_task + "' already exists.");
	ProgressDialog::Task t;
	t.vb = memnew(VBoxContainer);
	VBoxContainer *vb2 = memnew(VBoxContainer);
	t.vb->add_margin_child(p_label, vb2);
	t.progress = memnew(ProgressBar);
	t.progress->set_max(p_steps);
	t.progress->set_value(p_steps);
	t.last_progress_tick = 0;
	vb2->add_child(t.progress);

	t.time_info = memnew(Label);
	// Make the time spent and ETA less prominent.
	t.time_info->set_modulate(Color(1, 1, 1, 0.55));
	t.time_started = OS::get_singleton()->get_ticks_msec();
	vb2->add_child(t.time_info);

	t.state = memnew(Label);
	t.state->set_clip_text(true);
	vb2->add_child(t.state);

	main->add_child(t.vb);

	tasks[p_task] = t;
	if (p_can_cancel) {
		cancel_hb->show();
	} else {
		cancel_hb->hide();
	}
	cancel_hb->raise();
	cancelled = false;
	_popup();
	if (p_can_cancel) {
		cancel->grab_focus();
	}
}

String ProgressDialog::get_time_string(int p_seconds) const {
	return vformat("%02d:%02d:%02d", p_seconds / 3600, (p_seconds % 3600) / 60, p_seconds % 60);
}

bool ProgressDialog::task_step(const String &p_task, const String &p_state, int p_step, bool p_force_redraw) {
	ERR_FAIL_COND_V(!tasks.has(p_task), cancelled);

	Task &t = tasks[p_task];
	if (!p_force_redraw) {
		uint64_t tus = OS::get_singleton()->get_ticks_usec();
		if (tus - t.last_progress_tick < 200000) { //200ms
			return cancelled;
		}
	}

	if (p_step < int(Math::round(t.progress->get_value()))) {
		// Going backwards (which means we entered a new subtask).
		// Reset the wall clock time (which also resets the ETA).
		t.time_started = OS::get_singleton()->get_ticks_msec();
	}

	if (p_step < 0) {
		t.progress->set_value(t.progress->get_value() + 1);
	} else {
		t.progress->set_value(p_step);
	}

	t.state->set_text(p_state);
	t.last_progress_tick = OS::get_singleton()->get_ticks_usec();

	if (cancel_hb->is_visible()) {
		OS::get_singleton()->force_process_input();
	}

#ifndef ANDROID_ENABLED
	Main::iteration(); // this will not work on a lot of platforms, so it's only meant for the editor
#endif
	return cancelled;
}

void ProgressDialog::end_task(const String &p_task) {
	ERR_FAIL_COND(!tasks.has(p_task));
	Task &t = tasks[p_task];

	memdelete(t.vb);
	tasks.erase(p_task);

	if (tasks.empty()) {
		hide();
	} else {
		_popup();
	}
}

void ProgressDialog::_time_info_timer_timeout() {
	for (Map<String, Task>::Element *E = tasks.front(); E; E = E->next()) {
		Task task = E->get();

		const uint32_t time_spent = OS::get_singleton()->get_ticks_msec() - task.time_started;
		// Don't display an ETA if less than 4 seconds have passed.
		// This is because ETA requires some data to be accurate, and 1-3 seconds of data
		// is just not accurate enough to give an useful ETA.
		if (time_spent >= 4000 && !Math::is_zero_approx(task.progress->get_value())) {
			// Avoid division by zero.
			const uint32_t eta = time_spent / MAX(0.001, task.progress->get_value() / MAX(0.001, task.progress->get_max())) - time_spent;
			task.time_info->set_text(vformat("%s (ETA: %s)", get_time_string(time_spent * 0.001), get_time_string(eta * 0.001)));
		} else if (time_spent >= 1000) {
			// No ETA since progress hasn't started yet.
			task.time_info->set_text(vformat("%s", get_time_string(time_spent * 0.001)));
		} else {
			// Don't display "00:00:00".
			task.time_info->set_text("");
		}
	}
}

void ProgressDialog::_cancel_pressed() {
	cancelled = true;
}

void ProgressDialog::_bind_methods() {
	ClassDB::bind_method("_cancel_pressed", &ProgressDialog::_cancel_pressed);
	ClassDB::bind_method("_time_info_timer_timeout", &ProgressDialog::_time_info_timer_timeout);
}

ProgressDialog::ProgressDialog() {
	main = memnew(VBoxContainer);
	add_child(main);
	main->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	set_exclusive(true);
	singleton = this;
	cancel_hb = memnew(HBoxContainer);
	main->add_child(cancel_hb);
	cancel_hb->hide();
	cancel = memnew(Button);
	cancel_hb->add_spacer();
	cancel_hb->add_child(cancel);
	cancel->set_text(TTR("Cancel"));
	cancel_hb->add_spacer();
	cancel->connect("pressed", this, "_cancel_pressed");

	time_info_timer = memnew(Timer);
	// Setting the update time to 1 second cause some seconds to be "missed"
	// due to the heavy load during the progress dialog.
	// Lower values make the time update in a more predictable way, so use the same
	// timeout as the progres bar itself.
	time_info_timer->set_wait_time(0.2);
	time_info_timer->set_autostart(true);
	add_child(time_info_timer);
	time_info_timer->connect("timeout", this, "_time_info_timer_timeout");
}
