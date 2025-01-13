/**************************************************************************/
/*  embedded_process.cpp                                                  */
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

#include "embedded_process.h"

#include "editor/editor_string_names.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "scene/theme/theme_db.h"

void EmbeddedProcess::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			window = get_window();
		} break;
		case NOTIFICATION_PROCESS: {
			_check_focused_process_id();
			_check_mouse_over();

			// We need to detect when the control globally changes location or size on the screen.
			// NOTIFICATION_RESIZED and NOTIFICATION_WM_POSITION_CHANGED are not enough to detect
			// resized parent to siblings controls that can affect global position.
			Rect2i new_global_rect = get_global_rect();
			if (last_global_rect != new_global_rect) {
				last_global_rect = new_global_rect;
				_queue_update_embedded_process();
			}

		} break;
		case NOTIFICATION_DRAW: {
			_draw();
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_WM_POSITION_CHANGED: {
			_queue_update_embedded_process();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			focus_style_box = get_theme_stylebox(SNAME("FocusViewport"), EditorStringName(EditorStyles));
			Ref<StyleBoxFlat> focus_style_box_flat = focus_style_box;
			if (focus_style_box_flat.is_valid()) {
				margin_top_left = Point2i(focus_style_box_flat->get_border_width(SIDE_LEFT), focus_style_box_flat->get_border_width(SIDE_TOP));
				margin_bottom_right = Point2i(focus_style_box_flat->get_border_width(SIDE_RIGHT), focus_style_box_flat->get_border_width(SIDE_BOTTOM));
			} else if (focus_style_box.is_valid()) {
				margin_top_left = Point2i(focus_style_box->get_margin(SIDE_LEFT), focus_style_box->get_margin(SIDE_TOP));
				margin_bottom_right = Point2i(focus_style_box->get_margin(SIDE_RIGHT), focus_style_box->get_margin(SIDE_BOTTOM));
			} else {
				margin_top_left = Point2i();
				margin_bottom_right = Point2i();
			}
		} break;
		case NOTIFICATION_FOCUS_ENTER: {
			_queue_update_embedded_process();
		} break;
		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			application_has_focus = true;
			if (embedded_process_was_focused) {
				embedded_process_was_focused = false;
				// Refocus the embedded process if it was focused when the application lost focus,
				// but do not refocus if the embedded process is currently focused (indicating it just lost focus)
				// or if the current window is a different popup or secondary window.
				if (embedding_completed && current_process_id != focused_process_id && window && window->has_focus()) {
					grab_focus();
					_queue_update_embedded_process();
				}
			}
		} break;
		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			application_has_focus = false;
			embedded_process_was_focused = embedding_completed && current_process_id == focused_process_id;
		} break;
	}
}

void EmbeddedProcess::set_window_size(const Size2i p_window_size) {
	if (window_size != p_window_size) {
		window_size = p_window_size;
		_queue_update_embedded_process();
	}
}

void EmbeddedProcess::set_keep_aspect(bool p_keep_aspect) {
	if (keep_aspect != p_keep_aspect) {
		keep_aspect = p_keep_aspect;
		_queue_update_embedded_process();
	}
}

Rect2i EmbeddedProcess::_get_global_embedded_window_rect() {
	Rect2i control_rect = get_global_rect();
	control_rect = Rect2i(control_rect.position, control_rect.size.maxi(1));
	if (keep_aspect) {
		Rect2i desired_rect = control_rect;
		float ratio = MIN((float)control_rect.size.x / window_size.x, (float)control_rect.size.y / window_size.y);
		desired_rect.size = Size2i(window_size.x * ratio, window_size.y * ratio).maxi(1);
		desired_rect.position = Size2i(control_rect.position.x + ((control_rect.size.x - desired_rect.size.x) / 2), control_rect.position.y + ((control_rect.size.y - desired_rect.size.y) / 2));
		return desired_rect;
	} else {
		return control_rect;
	}
}

Rect2i EmbeddedProcess::get_screen_embedded_window_rect() {
	Rect2i rect = _get_global_embedded_window_rect();
	if (window) {
		rect.position += window->get_position();
	}

	// Removing margins to make space for the focus border style.
	return Rect2i(rect.position.x + margin_top_left.x, rect.position.y + margin_top_left.y, MAX(rect.size.x - (margin_top_left.x + margin_bottom_right.x), 1), MAX(rect.size.y - (margin_top_left.y + margin_bottom_right.y), 1));
}

bool EmbeddedProcess::is_embedding_in_progress() {
	return !timer_embedding->is_stopped();
}

bool EmbeddedProcess::is_embedding_completed() {
	return embedding_completed;
}

void EmbeddedProcess::embed_process(OS::ProcessID p_pid) {
	if (!window) {
		return;
	}

	ERR_FAIL_COND_MSG(!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING), "Embedded process not supported by this display server.");

	if (current_process_id != 0) {
		// Stop embedding the last process.
		OS::get_singleton()->kill(current_process_id);
	}

	reset();

	current_process_id = p_pid;
	start_embedding_time = OS::get_singleton()->get_ticks_msec();
	embedding_grab_focus = has_focus();
	set_process(true);
	set_notify_transform(true);

	// Attempt to embed the process, but if it has just started and the window is not ready yet,
	// we will retry in this case.
	_try_embed_process();
}

void EmbeddedProcess::reset() {
	if (current_process_id != 0 && embedding_completed) {
		DisplayServer::get_singleton()->remove_embedded_process(current_process_id);
	}
	current_process_id = 0;
	embedding_completed = false;
	start_embedding_time = 0;
	embedding_grab_focus = false;
	timer_embedding->stop();
	set_process(false);
	set_notify_transform(false);
	queue_redraw();
}

void EmbeddedProcess::_try_embed_process() {
	bool is_visible = is_visible_in_tree();
	Error err = DisplayServer::get_singleton()->embed_process(window->get_window_id(), current_process_id, get_screen_embedded_window_rect(), is_visible, is_visible && application_has_focus && embedding_grab_focus);
	if (err == OK) {
		embedding_completed = true;
		queue_redraw();
		emit_signal(SNAME("embedding_completed"));
	} else if (err == ERR_DOES_NOT_EXIST) {
		if (OS::get_singleton()->get_ticks_msec() - start_embedding_time >= (uint64_t)embedding_timeout) {
			// Embedding process timed out.
			reset();
			emit_signal(SNAME("embedding_failed"));
		} else {
			// Tries another shot.
			timer_embedding->start();
		}
	} else {
		// Another unknown error.
		reset();
		emit_signal(SNAME("embedding_failed"));
	}
}

bool EmbeddedProcess::_is_embedded_process_updatable() {
	return window && current_process_id != 0 && embedding_completed;
}

void EmbeddedProcess::_queue_update_embedded_process() {
	if (updated_embedded_process_queued || !_is_embedded_process_updatable()) {
		return;
	}

	updated_embedded_process_queued = true;

	callable_mp(this, &EmbeddedProcess::_update_embedded_process).call_deferred();
}

void EmbeddedProcess::_update_embedded_process() {
	updated_embedded_process_queued = false;

	if (!_is_embedded_process_updatable()) {
		return;
	}

	bool must_grab_focus = false;
	bool focus = has_focus();
	if (last_updated_embedded_process_focused != focus) {
		if (focus) {
			must_grab_focus = true;
		}
		last_updated_embedded_process_focused = focus;
	}

	DisplayServer::get_singleton()->embed_process(window->get_window_id(), current_process_id, get_screen_embedded_window_rect(), is_visible_in_tree(), must_grab_focus);
	emit_signal(SNAME("embedded_process_updated"));
}

void EmbeddedProcess::_timer_embedding_timeout() {
	_try_embed_process();
}

void EmbeddedProcess::_draw() {
	if (focused_process_id == current_process_id && has_focus() && focus_style_box.is_valid()) {
		Size2 size = get_size();
		Rect2 r = Rect2(Point2(), size);
		focus_style_box->draw(get_canvas_item(), r);
	}
}

void EmbeddedProcess::_check_mouse_over() {
	// This method checks if the mouse is over the embedded process while the current application is focused.
	// The goal is to give focus to the embedded process as soon as the mouse hovers over it,
	// allowing the user to interact with it immediately without needing to click first.
	if (!is_visible_in_tree() || !embedding_completed || !application_has_focus || !window || !window->has_focus() || Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT) || Input::get_singleton()->is_mouse_button_pressed(MouseButton::RIGHT)) {
		return;
	}

	bool focused = has_focus();

	// Not stealing focus from a textfield.
	if (!focused && get_viewport()->gui_get_focus_owner() && get_viewport()->gui_get_focus_owner()->is_text_field()) {
		return;
	}

	Vector2 mouse_position = DisplayServer::get_singleton()->mouse_get_position();
	Rect2i window_rect = get_screen_embedded_window_rect();
	if (!window_rect.has_point(mouse_position)) {
		return;
	}

	// Don't grab the focus if mouse over another window.
	DisplayServer::WindowID window_id_over = DisplayServer::get_singleton()->get_window_at_screen_position(mouse_position);
	if (window_id_over > 0 && window_id_over != window->get_window_id()) {
		return;
	}

	// When we already have the focus and the user moves the mouse over the embedded process,
	// we just need to refocus the process.
	if (focused) {
		_queue_update_embedded_process();
	} else {
		grab_focus();
		queue_redraw();
	}
}

void EmbeddedProcess::_check_focused_process_id() {
	OS::ProcessID process_id = DisplayServer::get_singleton()->get_focused_process_id();
	if (process_id != focused_process_id) {
		focused_process_id = process_id;
		if (focused_process_id == current_process_id) {
			// The embedded process got the focus.
			emit_signal(SNAME("embedded_process_focused"));
			if (has_focus()) {
				// Redraw to updated the focus style.
				queue_redraw();
			} else {
				grab_focus();
			}
		} else if (has_focus()) {
			release_focus();
		}
	}
}

void EmbeddedProcess::_bind_methods() {
	ADD_SIGNAL(MethodInfo("embedding_completed"));
	ADD_SIGNAL(MethodInfo("embedding_failed"));
	ADD_SIGNAL(MethodInfo("embedded_process_updated"));
	ADD_SIGNAL(MethodInfo("embedded_process_focused"));
}

EmbeddedProcess::EmbeddedProcess() {
	timer_embedding = memnew(Timer);
	timer_embedding->set_wait_time(0.1);
	timer_embedding->set_one_shot(true);
	add_child(timer_embedding);
	timer_embedding->connect("timeout", callable_mp(this, &EmbeddedProcess::_timer_embedding_timeout));
	set_focus_mode(FOCUS_ALL);
}

EmbeddedProcess::~EmbeddedProcess() {
	if (current_process_id != 0) {
		// Stop embedding the last process.
		OS::get_singleton()->kill(current_process_id);
		reset();
	}
}
