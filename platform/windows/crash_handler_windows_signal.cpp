/**************************************************************************/
/*  crash_handler_windows_signal.cpp                                      */
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

#include "crash_handler_windows.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/version.h"
#include "main/main.h"

#ifdef CRASH_HANDLER_EXCEPTION

#include <cxxabi.h>
#include <signal.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include <psapi.h>

#include "thirdparty/libbacktrace/backtrace.h"

struct CrashHandlerData {
	int64_t index = 0;
	backtrace_state *state = nullptr;
	int64_t offset = 0;
};

int symbol_callback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function) {
	CrashHandlerData *ch_data = reinterpret_cast<CrashHandlerData *>(data);
	if (!function) {
		return 0;
	}

	char fname[1024];
	snprintf(fname, 1024, "%s", function);

	if (function[0] == '_') {
		int status;
		char *demangled = abi::__cxa_demangle(function, nullptr, nullptr, &status);

		if (status == 0 && demangled) {
			snprintf(fname, 1024, "%s", demangled);
		}

		if (demangled) {
			free(demangled);
		}
	}

	print_error(vformat("[%d] %s (%s:%d)", ch_data->index++, String::utf8(fname), String::utf8(filename), lineno));
	return 0;
}

void error_callback(void *data, const char *msg, int errnum) {
	CrashHandlerData *ch_data = reinterpret_cast<CrashHandlerData *>(data);
	if (ch_data->index == 0) {
		print_error(vformat("Error(%d): %s", errnum, String::utf8(msg)));
	} else {
		print_error(vformat("[%d] error(%d): %s", ch_data->index++, errnum, String::utf8(msg)));
	}
}

int trace_callback(void *data, uintptr_t pc) {
	CrashHandlerData *ch_data = reinterpret_cast<CrashHandlerData *>(data);
	backtrace_pcinfo(ch_data->state, pc - ch_data->offset, &symbol_callback, &error_callback, data);
	return 0;
}

int64_t get_image_base(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return 0;
	}
	{
		f->seek(0x3c);
		uint32_t pe_pos = f->get_32();

		f->seek(pe_pos);
		uint32_t magic = f->get_32();
		if (magic != 0x00004550) {
			return 0;
		}
	}
	int64_t opt_header_pos = f->get_position() + 0x14;
	f->seek(opt_header_pos);

	uint16_t opt_header_magic = f->get_16();
	if (opt_header_magic == 0x10B) {
		f->seek(opt_header_pos + 0x1C);
		return f->get_32();
	} else if (opt_header_magic == 0x20B) {
		f->seek(opt_header_pos + 0x18);
		return f->get_64();
	} else {
		return 0;
	}
}

extern void CrashHandlerException(int signal) {
	CrashHandlerData data;

	if (OS::get_singleton() == nullptr || OS::get_singleton()->is_disable_crash_handler() || IsDebuggerPresent()) {
		return;
	}

	String msg;
	const ProjectSettings *proj_settings = ProjectSettings::get_singleton();
	if (proj_settings) {
		msg = proj_settings->get("debug/settings/crash_handler/message");
	}

	// Tell MainLoop about the crash. This can be handled by users too in Node.
	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_CRASH);
	}

	print_error("\n================================================================");
	print_error(vformat("%s: Program crashed with signal %d", __FUNCTION__, signal));

	// Print the engine version just before, so that people are reminded to include the version in backtrace reports.
	if (String(VERSION_HASH).is_empty()) {
		print_error(vformat("Engine version: %s", VERSION_FULL_NAME));
	} else {
		print_error(vformat("Engine version: %s (%s)", VERSION_FULL_NAME, VERSION_HASH));
	}
	print_error(vformat("Dumping the backtrace. %s", msg));

	String _execpath = OS::get_singleton()->get_executable_path();

	// Load process and image info to determine ASLR addresses offset.
	MODULEINFO mi;
	GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &mi, sizeof(mi));
	int64_t image_mem_base = reinterpret_cast<int64_t>(mi.lpBaseOfDll);
	int64_t image_file_base = get_image_base(_execpath);
	data.offset = image_mem_base - image_file_base;

	if (FileAccess::exists(_execpath + ".debugsymbols")) {
		_execpath = _execpath + ".debugsymbols";
	}
	_execpath = _execpath.replace("/", "\\");

	CharString cs = _execpath.utf8(); // Note: should remain in scope during backtrace_simple call.
	data.state = backtrace_create_state(cs.get_data(), 0, &error_callback, reinterpret_cast<void *>(&data));
	if (data.state != nullptr) {
		data.index = 1;
		backtrace_simple(data.state, 1, &trace_callback, &error_callback, reinterpret_cast<void *>(&data));
	}

	print_error("-- END OF BACKTRACE --");
	print_error("================================================================");
}
#endif

CrashHandler::CrashHandler() {
	disabled = false;
}

CrashHandler::~CrashHandler() {
}

void CrashHandler::disable() {
	if (disabled) {
		return;
	}

#if defined(CRASH_HANDLER_EXCEPTION)
	signal(SIGSEGV, nullptr);
	signal(SIGFPE, nullptr);
	signal(SIGILL, nullptr);
#endif

	disabled = true;
}

void CrashHandler::initialize() {
#if defined(CRASH_HANDLER_EXCEPTION)
	signal(SIGSEGV, CrashHandlerException);
	signal(SIGFPE, CrashHandlerException);
	signal(SIGILL, CrashHandlerException);
#endif
}
