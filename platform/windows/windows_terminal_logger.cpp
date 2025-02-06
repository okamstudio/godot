/**************************************************************************/
/*  windows_terminal_logger.cpp                                           */
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

#include "windows_terminal_logger.h"

#include "core/os/os.h"

#ifdef WINDOWS_ENABLED

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void WindowsTerminalLogger::logv(const char *p_format, va_list p_list, bool p_err) {
	if (!should_log(p_err)) {
		return;
	}

	const int static_buffer_size = 1024;
	char static_buf[static_buffer_size];
	char *buf = static_buf;
	va_list list_copy;
	va_copy(list_copy, p_list);
	int len = vsnprintf(buf, static_buffer_size, p_format, p_list);
	if (len >= static_buffer_size) {
		buf = (char *)memalloc(len + 1);
		len = vsnprintf(buf, len + 1, p_format, list_copy);
	}
	va_end(list_copy);

	String str_buf = String::utf8(buf, len).replace("\r\n", "\n").replace("\n", "\r\n");
	if (len >= static_buffer_size) {
		memfree(buf);
	}
	CharString cstr_buf = str_buf.utf8();
	if (cstr_buf.length() == 0) {
		return;
	}

	DWORD written = 0;
	HANDLE h = p_err ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
	WriteFile(h, cstr_buf.ptr(), cstr_buf.length(), &written, nullptr);

#ifdef DEBUG_ENABLED
	FlushFileBuffers(h);
#endif
}

void WindowsTerminalLogger::log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type) {
	if (!should_log(true)) {
		return;
	}

	HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	if (OS::get_singleton()->get_stdout_type() != OS::STD_HANDLE_CONSOLE || !hCon || hCon == INVALID_HANDLE_VALUE) {
		StdLogger::log_error(p_function, p_file, p_line, p_code, p_rationale, p_type);
	} else {
		CONSOLE_SCREEN_BUFFER_INFO sbi; //original
		GetConsoleScreenBufferInfo(hCon, &sbi);

		WORD current_bg = sbi.wAttributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);

		uint32_t basecol = 0;
		switch (p_type) {
			case ERR_ERROR:
				basecol = FOREGROUND_RED;
				break;
			case ERR_WARNING:
				basecol = FOREGROUND_RED | FOREGROUND_GREEN;
				break;
			case ERR_SCRIPT:
				basecol = FOREGROUND_RED | FOREGROUND_BLUE;
				break;
			case ERR_SHADER:
				basecol = FOREGROUND_GREEN | FOREGROUND_BLUE;
				break;
		}

		basecol |= current_bg;

		SetConsoleTextAttribute(hCon, basecol | FOREGROUND_INTENSITY);
		logf_error("%s:", error_type_string(p_type));

		SetConsoleTextAttribute(hCon, basecol);
		if (p_rationale && p_rationale[0]) {
			logf_error(" %s\n", p_rationale);
		} else {
			logf_error(" %s\n", p_code);
		}

		// `FOREGROUND_INTENSITY` alone results in gray text.
		SetConsoleTextAttribute(hCon, FOREGROUND_INTENSITY);
		switch (p_type) {
			case ERR_ERROR:
				logf_error("   at: ");
				break;
			case ERR_WARNING:
				logf_error("     at: ");
				break;
			case ERR_SCRIPT:
				logf_error("          at: ");
				break;
			case ERR_SHADER:
				logf_error("          at: ");
				break;
		}

		if (p_rationale && p_rationale[0]) {
			logf_error("(%s:%i)\n", p_file, p_line);
		} else {
			logf_error("%s (%s:%i)\n", p_function, p_file, p_line);
		}

		SetConsoleTextAttribute(hCon, sbi.wAttributes);
	}
}

WindowsTerminalLogger::~WindowsTerminalLogger() {}

#endif // WINDOWS_ENABLED
