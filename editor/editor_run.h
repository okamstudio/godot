/**************************************************************************/
/*  editor_run.h                                                          */
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

#ifndef EDITOR_RUN_H
#define EDITOR_RUN_H

#include "core/os/os.h"

typedef void (*EditorRunInstanceStarting)(int p_index, List<String> &r_arguments);

class EditorRun {
public:
	enum Status {
		STATUS_PLAY,
		STATUS_PAUSED,
		STATUS_STOP
	};

	List<OS::ProcessID> pids;

	struct WindowPlacement {
		int screen = 0;
		Point2i position = Point2i(INT_MAX, INT_MAX);
		Size2i size;
		bool force_maximized = false;
		bool force_fullscreen = false;
	};

private:
	Status status;
	String running_scene;

public:
	inline static EditorRunInstanceStarting instance_starting_callback = nullptr;

	Status get_status() const;
	String get_running_scene() const;

	Error run(const String &p_scene, const String &p_write_movie = "", const Vector<String> &p_run_args = Vector<String>());
	void run_native_notify() { status = STATUS_PLAY; }
	void stop();

	void stop_child_process(OS::ProcessID p_pid);
	bool has_child_process(OS::ProcessID p_pid) const;
	int get_child_process_count() const { return pids.size(); }
	OS::ProcessID get_current_process() const;

	static WindowPlacement get_window_placement();

	EditorRun();
};

#endif // EDITOR_RUN_H
