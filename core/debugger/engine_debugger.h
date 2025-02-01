/**************************************************************************/
/*  engine_debugger.h                                                     */
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

#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"

class RemoteDebuggerPeer;
class ScriptDebugger;

class EngineDebugger {
public:
	typedef void (*ProfilingToggle)(void *p_user, bool p_enable, const Array &p_opts);
	typedef void (*ProfilingTick)(void *p_user, double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time);
	typedef void (*ProfilingAdd)(void *p_user, const Array &p_arr);

	typedef Error (*CaptureFunc)(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured);

	typedef RemoteDebuggerPeer *(*CreatePeerFunc)(const String &p_uri);

	class Profiler {
		friend class EngineDebugger;

		ProfilingToggle toggle = nullptr;
		ProfilingAdd add = nullptr;
		ProfilingTick tick = nullptr;
		void *data = nullptr;
		bool active = false;

	public:
		Profiler() {}
		Profiler(void *p_data, ProfilingToggle p_toggle, ProfilingAdd p_add, ProfilingTick p_tick) {
			data = p_data;
			toggle = p_toggle;
			add = p_add;
			tick = p_tick;
		}
	};

	class Capture {
		friend class EngineDebugger;

		CaptureFunc capture = nullptr;
		void *data = nullptr;

	public:
		Capture() {}
		Capture(void *p_data, CaptureFunc p_capture) {
			data = p_data;
			capture = p_capture;
		}
	};

private:
	double frame_time = 0.0;
	double process_time = 0.0;
	double physics_time = 0.0;
	double physics_frame_time = 0.0;

	uint32_t poll_every = 0;

protected:
	static EngineDebugger *singleton;
	static ScriptDebugger *script_debugger;

	static HashMap<StringName, Profiler> profilers;
	static HashMap<StringName, Capture> captures;
	static HashMap<String, CreatePeerFunc> protocols;

	static void (*allow_focus_steal_fn)();

public:
	_FORCE_INLINE_ static EngineDebugger *get_singleton() { return singleton; }
	_FORCE_INLINE_ static bool is_active() { return singleton != nullptr && script_debugger != nullptr; }

	_FORCE_INLINE_ static ScriptDebugger *get_script_debugger() { return script_debugger; }

	static void initialize(const String &p_uri, bool p_skip_breakpoints, const Vector<String> &p_breakpoints, void (*p_allow_focus_steal_fn)());
	static void deinitialize();
	static void register_profiler(const StringName &p_name, const Profiler &p_profiler);
	static void unregister_profiler(const StringName &p_name);
	static bool is_profiling(const StringName &p_name);
	static bool has_profiler(const StringName &p_name);
	static void profiler_add_frame_data(const StringName &p_name, const Array &p_data);

	static void register_message_capture(const StringName &p_name, Capture p_func);
	static void unregister_message_capture(const StringName &p_name);
	static bool has_capture(const StringName &p_name);

	static void register_uri_handler(const String &p_protocol, CreatePeerFunc p_func);

	void iteration(uint64_t p_frame_ticks, uint64_t p_process_ticks, uint64_t p_physics_ticks, double p_physics_frame_time);
	void profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts = Array());
	Error capture_parse(const StringName &p_name, const String &p_msg, const Array &p_args, bool &r_captured);

	void line_poll() {
		// The purpose of this is just processing events every now and then when the script might get too busy otherwise bugs like infinite loops can't be caught.
		if (unlikely(poll_every % 2048) == 0) {
			poll_events(false);
		}
		poll_every++;
	}

	virtual void poll_events(bool p_is_idle) {}
	virtual void send_message(const String &p_msg, const Array &p_data) = 0;
	virtual void send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, bool p_editor_notify, ErrorHandlerType p_type) = 0;
	virtual void debug(bool p_can_continue = true, bool p_is_error_breakpoint = false) = 0;

	virtual ~EngineDebugger();
};
