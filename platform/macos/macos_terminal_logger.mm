/**************************************************************************/
/*  macos_terminal_logger.mm                                              */
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

#include "macos_terminal_logger.h"

#ifdef MACOS_ENABLED

#include <os/log.h>

void MacOSTerminalLogger::log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type) {
	if (!should_log(true)) {
		return;
	}

	const char *err_details;
	if (p_rationale && p_rationale[0]) {
		err_details = p_rationale;
	} else {
		err_details = p_code;
	}

	// Disable color codes if stdout is not a TTY.
	// This prevents Godot from writing ANSI escape codes when redirecting
	// stdout and stderr to a file.
	const bool tty = isatty(fileno(stdout));
	const char *red = tty ? "\E[0;91m" : "";
	const char *red_bold = tty ? "\E[1;91m" : "";
	const char *red_faint = tty ? "\E[2;91m" : "";
	const char *yellow = tty ? "\E[0;93m" : "";
	const char *yellow_bold = tty ? "\E[1;93m" : "";
	const char *yellow_faint = tty ? "\E[2;93m" : "";
	const char *magenta = tty ? "\E[0;95m" : "";
	const char *magenta_bold = tty ? "\E[1;95m" : "";
	const char *magenta_faint = tty ? "\E[2;95m" : "";
	const char *cyan = tty ? "\E[0;96m" : "";
	const char *cyan_bold = tty ? "\E[1;96m" : "";
	const char *cyan_faint = tty ? "\E[2;96m" : "";
	const char *reset = tty ? "\E[0m" : "";

	switch (p_type) {
		case ERR_WARNING:
			os_log_info(OS_LOG_DEFAULT,
					"WARNING: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			logf_error("%sWARNING:%s %s\n", yellow_bold, yellow, err_details);
			logf_error("%s     at: %s (%s:%i)%s\n", yellow_faint, p_function, p_file, p_line, reset);
			break;
		case ERR_SCRIPT:
			os_log_error(OS_LOG_DEFAULT,
					"SCRIPT ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			logf_error("%sSCRIPT ERROR:%s %s\n", magenta_bold, magenta, err_details);
			logf_error("%s          at: %s (%s:%i)%s\n", magenta_faint, p_function, p_file, p_line, reset);
			break;
		case ERR_SHADER:
			os_log_error(OS_LOG_DEFAULT,
					"SHADER ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			logf_error("%sSHADER ERROR:%s %s\n", cyan_bold, cyan, err_details);
			logf_error("%s          at: %s (%s:%i)%s\n", cyan_faint, p_function, p_file, p_line, reset);
			break;
		case ERR_ERROR:
		default:
			os_log_error(OS_LOG_DEFAULT,
					"ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			logf_error("%sERROR:%s %s\n", red_bold, red, err_details);
			logf_error("%s   at: %s (%s:%i)%s\n", red_faint, p_function, p_file, p_line, reset);
			break;
	}
}

#endif // MACOS_ENABLED
