/**************************************************************************/
/*  timer.h                                                               */
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

#ifndef TIMER_H
#define TIMER_H

#include "scene/main/node.h"

class Timer : public Node {
	GDCLASS(Timer, Node);

	double wait_time = 1.0;
	bool one_shot = false;
	bool autostart = false;
	bool processing = false;
	bool paused = false;
	bool ignore_time_scale = false;

	double time_left = -1.0;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	enum TimerProcessCallback {
		TIMER_PROCESS_PHYSICS,
		TIMER_PROCESS_IDLE,
	};

	void set_wait_time(double p_time);
	double get_wait_time() const;

	void set_one_shot(bool p_one_shot);
	bool is_one_shot() const;

	void set_autostart(bool p_start);
	bool has_autostart() const;

	void start(double p_time = -1);
	void stop();

	void set_paused(bool p_paused);
	bool is_paused() const;

	void set_ignore_time_scale(bool p_ignore);
	bool is_ignoring_time_scale();

	bool is_stopped() const;

	double get_time_left() const;

	PackedStringArray get_configuration_warnings() const override;

	void set_timer_process_callback(TimerProcessCallback p_callback);
	TimerProcessCallback get_timer_process_callback() const;
	Timer();

private:
	TimerProcessCallback timer_process_callback = TIMER_PROCESS_IDLE;
	void _set_process(bool p_process, bool p_force = false);
};

VARIANT_ENUM_CAST(Timer::TimerProcessCallback);

#endif // TIMER_H
