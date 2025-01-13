/**************************************************************************/
/*  print_string.cpp                                                      */
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

#include "print_string.h"

#include "core/core_globals.h"
#include "core/os/os.h"

static PrintHandlerList *print_handler_list = nullptr;

void add_print_handler(PrintHandlerList *p_handler) {
	_global_lock();
	p_handler->next = print_handler_list;
	print_handler_list = p_handler;
	_global_unlock();
}

void remove_print_handler(const PrintHandlerList *p_handler) {
	_global_lock();

	PrintHandlerList *prev = nullptr;
	PrintHandlerList *l = print_handler_list;

	while (l) {
		if (l == p_handler) {
			if (prev) {
				prev->next = l->next;
			} else {
				print_handler_list = l->next;
			}
			break;
		}
		prev = l;
		l = l->next;
	}
	//OS::get_singleton()->print("print handler list is %p\n",print_handler_list);

	_global_unlock();
	ERR_FAIL_NULL(l);
}

void __print_line(const String &p_string) {
	if (!CoreGlobals::print_line_enabled) {
		return;
	}

	OS::get_singleton()->print("%s\n", p_string.utf8().get_data());

	_global_lock();
	PrintHandlerList *l = print_handler_list;
	while (l) {
		l->printfunc(l->userdata, p_string, false, false);
		l = l->next;
	}

	_global_unlock();
}

void __print_line_rich(const String &p_string) {
	if (!CoreGlobals::print_line_enabled) {
		return;
	}

	// Convert a subset of BBCode tags to ANSI escape codes for correct display in the terminal.
	// Support of those ANSI escape codes varies across terminal emulators,
	// especially for italic and strikethrough.

	String output;
	int pos = 0;
	while (pos <= p_string.length()) {
		int brk_pos = p_string.find_char('[', pos);

		if (brk_pos < 0) {
			brk_pos = p_string.length();
		}

		String txt = brk_pos > pos ? p_string.substr(pos, brk_pos - pos) : "";
		if (brk_pos == p_string.length()) {
			output += txt;
			break;
		}

		int brk_end = p_string.find_char(']', brk_pos + 1);

		if (brk_end == -1) {
			txt += p_string.substr(brk_pos, p_string.length() - brk_pos);
			output += txt;
			break;
		}
		pos = brk_end + 1;
		output += txt;

		String tag = p_string.substr(brk_pos + 1, brk_end - brk_pos - 1);
		if (tag == "b") {
			output += "\u001b[1m";
		} else if (tag == "/b") {
			output += "\u001b[22m";
		} else if (tag == "i") {
			output += "\u001b[3m";
		} else if (tag == "/i") {
			output += "\u001b[23m";
		} else if (tag == "u") {
			output += "\u001b[4m";
		} else if (tag == "/u") {
			output += "\u001b[24m";
		} else if (tag == "s") {
			output += "\u001b[9m";
		} else if (tag == "/s") {
			output += "\u001b[29m";
		} else if (tag == "indent") {
			output += "    ";
		} else if (tag == "/indent") {
			output += "";
		} else if (tag == "code") {
			output += "\u001b[2m";
		} else if (tag == "/code") {
			output += "\u001b[22m";
		} else if (tag == "url") {
			output += "";
		} else if (tag == "/url") {
			output += "";
		} else if (tag == "center") {
			output += "\n\t\t\t";
		} else if (tag == "center") {
			output += "";
		} else if (tag == "right") {
			output += "\n\t\t\t\t\t\t";
		} else if (tag == "/right") {
			output += "";
		} else if (tag.begins_with("color=")) {
			String color_name = tag.trim_prefix("color=");
			if (color_name == "black") {
				output += "\u001b[30m";
			} else if (color_name == "red") {
				output += "\u001b[91m";
			} else if (color_name == "green") {
				output += "\u001b[92m";
			} else if (color_name == "lime") {
				output += "\u001b[92m";
			} else if (color_name == "yellow") {
				output += "\u001b[93m";
			} else if (color_name == "blue") {
				output += "\u001b[94m";
			} else if (color_name == "magenta") {
				output += "\u001b[95m";
			} else if (color_name == "pink") {
				output += "\u001b[38;5;218m";
			} else if (color_name == "purple") {
				output += "\u001b[38;5;98m";
			} else if (color_name == "cyan") {
				output += "\u001b[96m";
			} else if (color_name == "white") {
				output += "\u001b[97m";
			} else if (color_name == "orange") {
				output += "\u001b[38;5;208m";
			} else if (color_name == "gray") {
				output += "\u001b[90m";
			} else {
				Color c = Color::from_string(color_name, Color());
				output += vformat("\u001b[38;2;%d;%d;%dm", c.r * 255, c.g * 255, c.b * 255);
			}
		} else if (tag == "/color") {
			output += "\u001b[39m";
		} else if (tag.begins_with("bgcolor=")) {
			String color_name = tag.trim_prefix("bgcolor=");
			if (color_name == "black") {
				output += "\u001b[40m";
			} else if (color_name == "red") {
				output += "\u001b[101m";
			} else if (color_name == "green") {
				output += "\u001b[102m";
			} else if (color_name == "lime") {
				output += "\u001b[102m";
			} else if (color_name == "yellow") {
				output += "\u001b[103m";
			} else if (color_name == "blue") {
				output += "\u001b[104m";
			} else if (color_name == "magenta") {
				output += "\u001b[105m";
			} else if (color_name == "pink") {
				output += "\u001b[48;5;218m";
			} else if (color_name == "purple") {
				output += "\u001b[48;5;98m";
			} else if (color_name == "cyan") {
				output += "\u001b[106m";
			} else if (color_name == "white") {
				output += "\u001b[107m";
			} else if (color_name == "orange") {
				output += "\u001b[48;5;208m";
			} else if (color_name == "gray") {
				output += "\u001b[100m";
			} else {
				Color c = Color::from_string(color_name, Color());
				output += vformat("\u001b[48;2;%d;%d;%dm", c.r * 255, c.g * 255, c.b * 255);
			}
		} else if (tag == "/bgcolor") {
			output += "\u001b[49m";
		} else if (tag.begins_with("fgcolor=")) {
			String color_name = tag.trim_prefix("fgcolor=");
			if (color_name == "black") {
				output += "\u001b[30;40m";
			} else if (color_name == "red") {
				output += "\u001b[91;101m";
			} else if (color_name == "green") {
				output += "\u001b[92;102m";
			} else if (color_name == "lime") {
				output += "\u001b[92;102m";
			} else if (color_name == "yellow") {
				output += "\u001b[93;103m";
			} else if (color_name == "blue") {
				output += "\u001b[94;104m";
			} else if (color_name == "magenta") {
				output += "\u001b[95;105m";
			} else if (color_name == "pink") {
				output += "\u001b[38;5;218;48;5;218m";
			} else if (color_name == "purple") {
				output += "\u001b[38;5;98;48;5;98m";
			} else if (color_name == "cyan") {
				output += "\u001b[96;106m";
			} else if (color_name == "white") {
				output += "\u001b[97;107m";
			} else if (color_name == "orange") {
				output += "\u001b[38;5;208;48;5;208m";
			} else if (color_name == "gray") {
				output += "\u001b[90;100m";
			} else {
				Color c = Color::from_string(color_name, Color());
				output += vformat("\u001b[38;2;%d;%d;%d;48;2;%d;%d;%dm", c.r * 255, c.g * 255, c.b * 255, c.r * 255, c.g * 255, c.b * 255);
			}
		} else if (tag == "/fgcolor") {
			output += "\u001b[39;49m";
		} else {
			output += vformat("[%s]", tag);
		}
	}
	output += "\u001b[0m"; // Reset.

	OS::get_singleton()->print_rich("%s\n", output.utf8().get_data());

	_global_lock();
	PrintHandlerList *l = print_handler_list;
	while (l) {
		l->printfunc(l->userdata, p_string, false, true);
		l = l->next;
	}

	_global_unlock();
}

void print_error(const String &p_string) {
	if (!CoreGlobals::print_error_enabled) {
		return;
	}

	OS::get_singleton()->printerr("%s\n", p_string.utf8().get_data());

	_global_lock();
	PrintHandlerList *l = print_handler_list;
	while (l) {
		l->printfunc(l->userdata, p_string, true, false);
		l = l->next;
	}

	_global_unlock();
}

bool is_print_verbose_enabled() {
	return OS::get_singleton()->is_stdout_verbose();
}

String stringify_variants(const Variant &p_var) {
	return p_var.operator String();
}
