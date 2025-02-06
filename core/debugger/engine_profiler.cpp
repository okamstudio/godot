/**************************************************************************/
/*  engine_profiler.cpp                                                   */
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

#include "engine_profiler.h"

#include "core/debugger/engine_debugger.h"

void EngineProfiler::_bind_methods() {
	GDVIRTUAL_BIND(_toggle, "enable", "options");
	GDVIRTUAL_BIND(_add_frame, "data");
	GDVIRTUAL_BIND(_tick, "frame_time", "process_time", "physics_time", "physics_frame_time");
}

void EngineProfiler::toggle(bool p_enable, const Array &p_array) {
	GDVIRTUAL_CALL(_toggle, p_enable, p_array);
}

void EngineProfiler::add(const Array &p_data) {
	GDVIRTUAL_CALL(_add_frame, p_data);
}

void EngineProfiler::tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
	GDVIRTUAL_CALL(_tick, p_frame_time, p_process_time, p_physics_time, p_physics_frame_time);
}

Error EngineProfiler::bind(const String &p_name) {
	ERR_FAIL_COND_V(is_bound(), ERR_ALREADY_IN_USE);
	EngineDebugger::Profiler prof(
			this,
			[](void *p_user, bool p_enable, const Array &p_opts) {
				static_cast<EngineProfiler *>(p_user)->toggle(p_enable, p_opts);
			},
			[](void *p_user, const Array &p_data) {
				static_cast<EngineProfiler *>(p_user)->add(p_data);
			},
			[](void *p_user, double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
				static_cast<EngineProfiler *>(p_user)->tick(p_frame_time, p_process_time, p_physics_time, p_physics_frame_time);
			});
	registration = p_name;
	EngineDebugger::register_profiler(p_name, prof);
	return OK;
}

Error EngineProfiler::unbind() {
	ERR_FAIL_COND_V(!is_bound(), ERR_UNCONFIGURED);
	EngineDebugger::unregister_profiler(registration);

	// Mark this instance unbound by clearing the registration name.
	registration.clear();
	return OK;
}

EngineProfiler::~EngineProfiler() {
	if (is_bound()) {
		unbind();
	}
}
