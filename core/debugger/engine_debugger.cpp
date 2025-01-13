/**************************************************************************/
/*  engine_debugger.cpp                                                   */
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

#include "engine_debugger.h"

#include "core/debugger/local_debugger.h"
#include "core/debugger/remote_debugger.h"
#include "core/debugger/remote_debugger_peer.h"
#include "core/debugger/script_debugger.h"
#include "core/os/os.h"

EngineDebugger *EngineDebugger::singleton = nullptr;
ScriptDebugger *EngineDebugger::script_debugger = nullptr;

HashMap<StringName, EngineDebugger::Profiler> EngineDebugger::profilers;
HashMap<StringName, EngineDebugger::Capture> EngineDebugger::captures;
HashMap<String, EngineDebugger::CreatePeerFunc> EngineDebugger::protocols;

void (*EngineDebugger::allow_focus_steal_fn)();

void EngineDebugger::register_profiler(const StringName &p_name, const Profiler &p_func) {
	ERR_FAIL_COND_MSG(profilers.has(p_name), vformat("Profiler already registered: '%s'.", p_name));
	profilers.insert(p_name, p_func);
}

void EngineDebugger::unregister_profiler(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), vformat("Profiler not registered: '%s'.", p_name));
	Profiler &p = profilers[p_name];
	if (p.active && p.toggle) {
		p.toggle(p.data, false, Array());
		p.active = false;
	}
	profilers.erase(p_name);
}

void EngineDebugger::register_message_capture(const StringName &p_name, Capture p_func) {
	ERR_FAIL_COND_MSG(captures.has(p_name), vformat("Capture already registered: '%s'.", p_name));
	captures.insert(p_name, p_func);
}

void EngineDebugger::unregister_message_capture(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!captures.has(p_name), vformat("Capture not registered: '%s'.", p_name));
	captures.erase(p_name);
}

void EngineDebugger::register_uri_handler(const String &p_protocol, CreatePeerFunc p_func) {
	ERR_FAIL_COND_MSG(protocols.has(p_protocol), vformat("Protocol handler already registered: '%s'.", p_protocol));
	protocols.insert(p_protocol, p_func);
}

void EngineDebugger::profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), vformat("Can't change profiler state, no profiler: '%s'.", p_name));
	Profiler &p = profilers[p_name];
	if (p.toggle) {
		p.toggle(p.data, p_enabled, p_opts);
	}
	p.active = p_enabled;
}

void EngineDebugger::profiler_add_frame_data(const StringName &p_name, const Array &p_data) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), vformat("Can't add frame data, no profiler: '%s'.", p_name));
	Profiler &p = profilers[p_name];
	if (p.add) {
		p.add(p.data, p_data);
	}
}

bool EngineDebugger::is_profiling(const StringName &p_name) {
	return profilers.has(p_name) && profilers[p_name].active;
}

bool EngineDebugger::has_profiler(const StringName &p_name) {
	return profilers.has(p_name);
}

bool EngineDebugger::has_capture(const StringName &p_name) {
	return captures.has(p_name);
}

Error EngineDebugger::capture_parse(const StringName &p_name, const String &p_msg, const Array &p_args, bool &r_captured) {
	r_captured = false;
	ERR_FAIL_COND_V_MSG(!captures.has(p_name), ERR_UNCONFIGURED, vformat("Capture not registered: '%s'.", p_name));
	const Capture &cap = captures[p_name];
	return cap.capture(cap.data, p_msg, p_args, r_captured);
}

void EngineDebugger::iteration(uint64_t p_frame_ticks, uint64_t p_process_ticks, uint64_t p_physics_ticks, double p_physics_frame_time) {
	frame_time = USEC_TO_SEC(p_frame_ticks);
	process_time = USEC_TO_SEC(p_process_ticks);
	physics_time = USEC_TO_SEC(p_physics_ticks);
	physics_frame_time = p_physics_frame_time;
	// Notify tick to running profilers
	for (KeyValue<StringName, Profiler> &E : profilers) {
		Profiler &p = E.value;
		if (!p.active || !p.tick) {
			continue;
		}
		p.tick(p.data, frame_time, process_time, physics_time, physics_frame_time);
	}
	singleton->poll_events(true);
}

void EngineDebugger::initialize(const String &p_uri, bool p_skip_breakpoints, const Vector<String> &p_breakpoints, void (*p_allow_focus_steal_fn)()) {
	register_uri_handler("tcp://", RemoteDebuggerPeerTCP::create); // TCP is the default protocol. Platforms/modules can add more.
	if (p_uri.is_empty()) {
		return;
	}
	if (p_uri == "local://") {
		singleton = memnew(LocalDebugger);
		script_debugger = memnew(ScriptDebugger);
		// Tell the OS that we want to handle termination signals.
		OS::get_singleton()->initialize_debugging();
	} else if (p_uri.contains("://")) {
		const String proto = p_uri.substr(0, p_uri.find("://") + 3);
		if (!protocols.has(proto)) {
			return;
		}
		RemoteDebuggerPeer *peer = protocols[proto](p_uri);
		if (!peer) {
			return;
		}
		singleton = memnew(RemoteDebugger(Ref<RemoteDebuggerPeer>(peer)));
		script_debugger = memnew(ScriptDebugger);
		// Notify editor of our pid (to allow focus stealing).
		Array msg;
		msg.push_back(OS::get_singleton()->get_process_id());
		singleton->send_message("set_pid", msg);
	}
	if (!singleton) {
		return;
	}

	// There is a debugger, parse breakpoints.
	ScriptDebugger *singleton_script_debugger = singleton->get_script_debugger();
	singleton_script_debugger->set_skip_breakpoints(p_skip_breakpoints);

	for (int i = 0; i < p_breakpoints.size(); i++) {
		const String &bp = p_breakpoints[i];
		int sp = bp.rfind_char(':');
		ERR_CONTINUE_MSG(sp == -1, vformat("Invalid breakpoint: '%s', expected file:line format.", bp));

		singleton_script_debugger->insert_breakpoint(bp.substr(sp + 1, bp.length()).to_int(), bp.substr(0, sp));
	}

	allow_focus_steal_fn = p_allow_focus_steal_fn;
}

void EngineDebugger::deinitialize() {
	if (singleton) {
		// Stop all profilers
		for (const KeyValue<StringName, Profiler> &E : profilers) {
			if (E.value.active) {
				singleton->profiler_enable(E.key, false);
			}
		}

		// Flush any remaining message
		singleton->poll_events(false);

		memdelete(singleton);
		singleton = nullptr;
	}

	// Clear profilers/captures/protocol handlers.
	profilers.clear();
	captures.clear();
	protocols.clear();
}

EngineDebugger::~EngineDebugger() {
	if (script_debugger) {
		memdelete(script_debugger);
	}
	script_debugger = nullptr;
	singleton = nullptr;
}
