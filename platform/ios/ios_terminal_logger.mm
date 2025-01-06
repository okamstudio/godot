/**************************************************************************/
/*  ios_terminal_logger.mm                                                */
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

#import "ios_terminal_logger.h"

#ifdef IOS_ENABLED

#import <os/log.h>

void IOSTerminalLogger::log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type) {
	if (!should_log(true)) {
		return;
	}

	const char *err_details;
	if (p_rationale && p_rationale[0]) {
		err_details = p_rationale;
	} else {
		err_details = p_code;
	}

	switch (p_type) {
		case ERR_WARNING:
			os_log_info(OS_LOG_DEFAULT,
					"WARNING: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			break;
		case ERR_SCRIPT:
			os_log_error(OS_LOG_DEFAULT,
					"SCRIPT ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			break;
		case ERR_SHADER:
			os_log_error(OS_LOG_DEFAULT,
					"SHADER ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			break;
		case ERR_ERROR:
		default:
			os_log_error(OS_LOG_DEFAULT,
					"ERROR: %{public}s\nat: %{public}s (%{public}s:%i)",
					err_details, p_function, p_file, p_line);
			break;
	}
}

#endif // IOS_ENABLED
