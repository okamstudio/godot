/**************************************************************************/
/*  thread.cpp                                                            */
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

#include "platform_config.h"

#ifndef PLATFORM_THREAD_OVERRIDE // See details in thread.h.

#include "thread.h"

#ifdef THREADS_ENABLED
#include "core/object/script_language.h"

SafeNumeric<uint64_t> Thread::id_counter(1); // The first value after .increment() is 2, hence by default the main thread ID should be 1.
thread_local Thread::ID Thread::caller_id = Thread::id_counter.increment();

#endif

Thread::PlatformFunctions Thread::platform_functions;

void Thread::_set_platform_functions(const PlatformFunctions &p_functions) {
	platform_functions = p_functions;
}

#ifdef THREADS_ENABLED
void Thread::callback(ID p_caller_id, const Settings &p_settings, Callback p_callback, void *p_userdata) {
	Thread::caller_id = p_caller_id;
	if (platform_functions.set_priority) {
		platform_functions.set_priority(p_settings.priority);
	}
	if (platform_functions.init) {
		platform_functions.init();
	}
	ScriptServer::thread_enter(); // Scripts may need to attach a stack.
	if (platform_functions.wrapper) {
		platform_functions.wrapper(p_callback, p_userdata);
	} else {
		p_callback(p_userdata);
	}
	ScriptServer::thread_exit();
	if (platform_functions.term) {
		platform_functions.term();
	}
}

Thread::ID Thread::start(Thread::Callback p_callback, void *p_user, const Settings &p_settings) {
	ERR_FAIL_COND_V_MSG(id != UNASSIGNED_ID, UNASSIGNED_ID, "A Thread object has been re-started without wait_to_finish() having been called on it.");
	id = id_counter.increment();
	thread = THREADING_NAMESPACE::thread(&Thread::callback, id, p_settings, p_callback, p_user);
	return id;
}

bool Thread::is_started() const {
	return id != UNASSIGNED_ID;
}

void Thread::wait_to_finish() {
	ERR_FAIL_COND_MSG(id == UNASSIGNED_ID, "Attempt of waiting to finish on a thread that was never started.");
	ERR_FAIL_COND_MSG(id == get_caller_id(), "Threads can't wait to finish on themselves, another thread must wait.");
	thread.join();
	thread = THREADING_NAMESPACE::thread();
	id = UNASSIGNED_ID;
}

Error Thread::set_name(const String &p_name) {
	if (platform_functions.set_name) {
		return platform_functions.set_name(p_name);
	}

	return ERR_UNAVAILABLE;
}

Thread::Thread() {
}

Thread::~Thread() {
	if (id != UNASSIGNED_ID) {
#ifdef DEBUG_ENABLED
		WARN_PRINT(
				"A Thread object is being destroyed without its completion having been realized.\n"
				"Please call wait_to_finish() on it to ensure correct cleanup.");
#endif
		thread.detach();
	}
}

#endif // THREADS_ENABLED

#endif // PLATFORM_THREAD_OVERRIDE
