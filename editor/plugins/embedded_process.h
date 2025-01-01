/**************************************************************************/
/*  embedded_process.h                                                    */
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

#ifndef EMBEDDED_PROCESS_H
#define EMBEDDED_PROCESS_H

#include "scene/gui/control.h"

class EmbeddedProcess : public Control {
	GDCLASS(EmbeddedProcess, Control);

	bool application_has_focus = true;
	bool embedded_process_was_focused = false;
	OS::ProcessID focused_process_id = 0;
	OS::ProcessID current_process_id = 0;
	bool embedding_grab_focus = false;
	bool embedding_completed = false;
	uint64_t start_embedding_time = 0;
	bool updated_embedded_process_queued = false;
	bool last_updated_embedded_process_focused = false;

	Window *window = nullptr;
	Timer *timer_embedding = nullptr;

	const int embedding_timeout = 45000;

	bool keep_aspect = false;
	Size2i window_size;
	Ref<StyleBox> focus_style_box;
	Point2i margin_top_left;
	Point2i margin_bottom_right;
	Rect2i last_global_rect;

	void _try_embed_process();
	void _queue_update_embedded_process();
	void _update_embedded_process();
	void _timer_embedding_timeout();
	void _draw();
	void _check_mouse_over();
	void _check_focused_process_id();
	bool _is_embedded_process_updatable();
	Rect2i _get_global_embedded_window_rect();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void embed_process(OS::ProcessID p_pid);
	void reset();

	void set_window_size(const Size2i p_window_size);
	void set_keep_aspect(bool p_keep_aspect);

	Rect2i get_screen_embedded_window_rect();
	bool is_embedding_in_progress();
	bool is_embedding_completed();

	EmbeddedProcess();
	~EmbeddedProcess();
};

#endif // EMBEDDED_PROCESS_H
