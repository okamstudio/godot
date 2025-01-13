/**************************************************************************/
/*  local_debugger.cpp                                                    */
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

#include "local_debugger.h"

#include "core/debugger/script_debugger.h"
#include "core/os/os.h"

struct LocalDebugger::ScriptsProfiler {
	struct ProfileInfoSort {
		bool operator()(const ScriptLanguage::ProfilingInfo &A, const ScriptLanguage::ProfilingInfo &B) const {
			return A.total_time > B.total_time;
		}
	};

	double frame_time = 0;
	uint64_t idle_accum = 0;
	Vector<ScriptLanguage::ProfilingInfo> pinfo;

	void toggle(bool p_enable, const Array &p_opts) {
		if (p_enable) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_start();
			}

			print_line("BEGIN PROFILING");
			pinfo.resize(32768);
		} else {
			_print_frame_data(true);
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->profiling_stop();
			}
		}
	}

	void tick(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
		frame_time = p_frame_time;
		_print_frame_data(false);
	}

	void _print_frame_data(bool p_accumulated) {
		uint64_t diff = OS::get_singleton()->get_ticks_usec() - idle_accum;

		if (!p_accumulated && diff < 1000000) { //show every one second
			return;
		}

		idle_accum = OS::get_singleton()->get_ticks_usec();

		int ofs = 0;
		for (int i = 0; i < ScriptServer::get_language_count(); i++) {
			if (p_accumulated) {
				ofs += ScriptServer::get_language(i)->profiling_get_accumulated_data(&pinfo.write[ofs], pinfo.size() - ofs);
			} else {
				ofs += ScriptServer::get_language(i)->profiling_get_frame_data(&pinfo.write[ofs], pinfo.size() - ofs);
			}
		}

		SortArray<ScriptLanguage::ProfilingInfo, ProfileInfoSort> sort;
		sort.sort(pinfo.ptrw(), ofs);

		// compute total script frame time
		uint64_t script_time_us = 0;
		for (int i = 0; i < ofs; i++) {
			script_time_us += pinfo[i].self_time;
		}
		double script_time = USEC_TO_SEC(script_time_us);
		double total_time = p_accumulated ? script_time : frame_time;

		if (!p_accumulated) {
			print_line("FRAME: total: " + rtos(total_time) + " script: " + rtos(script_time) + "/" + itos(script_time * 100 / total_time) + " %");
		} else {
			print_line("ACCUMULATED: total: " + rtos(total_time));
		}

		for (int i = 0; i < ofs; i++) {
			print_line(itos(i) + ":" + pinfo[i].signature);
			double tt = USEC_TO_SEC(pinfo[i].total_time);
			double st = USEC_TO_SEC(pinfo[i].self_time);
			print_line("\ttotal: " + rtos(tt) + "/" + itos(tt * 100 / total_time) + " % \tself: " + rtos(st) + "/" + itos(st * 100 / total_time) + " % tcalls: " + itos(pinfo[i].call_count));
		}
	}

	ScriptsProfiler() {
		idle_accum = OS::get_singleton()->get_ticks_usec();
	}
};

void LocalDebugger::debug(bool p_can_continue, bool p_is_error_breakpoint) {
	ScriptLanguage *script_lang = script_debugger->get_break_language();

	if (!target_function.is_empty()) {
		String current_function = script_lang->debug_get_stack_level_function(0);
		if (current_function != target_function) {
			script_debugger->set_depth(0);
			script_debugger->set_lines_left(1);
			return;
		}
		target_function = "";
	}

	print_line("\nDebugger Break, Reason: '" + script_lang->debug_get_error() + "'");
	print_line("*Frame " + itos(0) + " - " + script_lang->debug_get_stack_level_source(0) + ":" + itos(script_lang->debug_get_stack_level_line(0)) + " in function '" + script_lang->debug_get_stack_level_function(0) + "'");
	print_line("Enter \"help\" for assistance.");
	int current_frame = 0;
	int total_frames = script_lang->debug_get_stack_level_count();
	while (true) {
		OS::get_singleton()->print("debug> ");
		String line = OS::get_singleton()->get_stdin_string().strip_edges();

		// Cache options
		String variable_prefix = options["variable_prefix"];

		if (line.is_empty() && !feof(stdin)) {
			print_line("\nDebugger Break, Reason: '" + script_lang->debug_get_error() + "'");
			print_line("*Frame " + itos(current_frame) + " - " + script_lang->debug_get_stack_level_source(current_frame) + ":" + itos(script_lang->debug_get_stack_level_line(current_frame)) + " in function '" + script_lang->debug_get_stack_level_function(current_frame) + "'");
			print_line("Enter \"help\" for assistance.");
		} else if (line == "c" || line == "continue") {
			break;
		} else if (line == "bt" || line == "breakpoint") {
			for (int i = 0; i < total_frames; i++) {
				String cfi = (current_frame == i) ? "*" : " "; //current frame indicator
				print_line(cfi + "Frame " + itos(i) + " - " + script_lang->debug_get_stack_level_source(i) + ":" + itos(script_lang->debug_get_stack_level_line(i)) + " in function '" + script_lang->debug_get_stack_level_function(i) + "'");
			}

		} else if (line.begins_with("fr") || line.begins_with("frame")) {
			if (line.get_slice_count(" ") == 1) {
				print_line("*Frame " + itos(current_frame) + " - " + script_lang->debug_get_stack_level_source(current_frame) + ":" + itos(script_lang->debug_get_stack_level_line(current_frame)) + " in function '" + script_lang->debug_get_stack_level_function(current_frame) + "'");
			} else {
				int frame = line.get_slicec(' ', 1).to_int();
				if (frame < 0 || frame >= total_frames) {
					print_line("Error: Invalid frame.");
				} else {
					current_frame = frame;
					print_line("*Frame " + itos(frame) + " - " + script_lang->debug_get_stack_level_source(frame) + ":" + itos(script_lang->debug_get_stack_level_line(frame)) + " in function '" + script_lang->debug_get_stack_level_function(frame) + "'");
				}
			}

		} else if (line.begins_with("set")) {
			if (line.get_slice_count(" ") == 1) {
				for (const KeyValue<String, String> &E : options) {
					print_line("\t" + E.key + "=" + E.value);
				}

			} else {
				String key_value = line.get_slicec(' ', 1);
				int value_pos = key_value.find_char('=');

				if (value_pos < 0) {
					print_line("Error: Invalid set format. Use: set key=value");
				} else {
					String key = key_value.left(value_pos);

					if (!options.has(key)) {
						print_line("Error: Unknown option " + key);
					} else {
						// Allow explicit tab character
						String value = key_value.substr(value_pos + 1).replace("\\t", "\t");

						options[key] = value;
					}
				}
			}

		} else if (line == "lv" || line == "locals") {
			List<String> locals;
			List<Variant> values;
			script_lang->debug_get_stack_level_locals(current_frame, &locals, &values);
			print_variables(locals, values, variable_prefix);

		} else if (line == "gv" || line == "globals") {
			List<String> globals;
			List<Variant> values;
			script_lang->debug_get_globals(&globals, &values);
			print_variables(globals, values, variable_prefix);

		} else if (line == "mv" || line == "members") {
			List<String> members;
			List<Variant> values;
			script_lang->debug_get_stack_level_members(current_frame, &members, &values);
			print_variables(members, values, variable_prefix);

		} else if (line.begins_with("p") || line.begins_with("print")) {
			if (line.find_char(' ') < 0) {
				print_line("Usage: print <expression>");
			} else {
				String expr = line.split(" ", true, 1)[1];
				String res = script_lang->debug_parse_stack_level_expression(current_frame, expr);
				print_line(res);
			}

		} else if (line == "s" || line == "step") {
			script_debugger->set_depth(-1);
			script_debugger->set_lines_left(1);
			break;
		} else if (line == "n" || line == "next") {
			script_debugger->set_depth(0);
			script_debugger->set_lines_left(1);
			break;
		} else if (line == "fin" || line == "finish") {
			String current_function = script_lang->debug_get_stack_level_function(0);

			for (int i = 0; i < total_frames; i++) {
				target_function = script_lang->debug_get_stack_level_function(i);
				if (target_function != current_function) {
					script_debugger->set_depth(0);
					script_debugger->set_lines_left(1);
					return;
				}
			}

			print_line("Error: Reached last frame.");
			target_function = "";

		} else if (line.begins_with("br") || line.begins_with("break")) {
			if (line.get_slice_count(" ") <= 1) {
				const HashMap<int, HashSet<StringName>> &breakpoints = script_debugger->get_breakpoints();
				if (breakpoints.size() == 0) {
					print_line("No Breakpoints.");
					continue;
				}

				print_line("Breakpoint(s): " + itos(breakpoints.size()));
				for (const KeyValue<int, HashSet<StringName>> &E : breakpoints) {
					print_line("\t" + String(*E.value.begin()) + ":" + itos(E.key));
				}

			} else {
				Pair<String, int> breakpoint = to_breakpoint(line);

				String source = breakpoint.first;
				int linenr = breakpoint.second;

				if (source.is_empty()) {
					continue;
				}

				script_debugger->insert_breakpoint(linenr, source);

				print_line("Added breakpoint at " + source + ":" + itos(linenr));
			}

		} else if (line == "q" || line == "quit" ||
				(line.is_empty() && feof(stdin))) {
			// Do not stop again on quit
			script_debugger->clear_breakpoints();
			script_debugger->set_depth(-1);
			script_debugger->set_lines_left(-1);

			MainLoop *main_loop = OS::get_singleton()->get_main_loop();
			if (main_loop->get_class() == "SceneTree") {
				main_loop->call("quit");
			}
			break;
		} else if (line.begins_with("delete")) {
			if (line.get_slice_count(" ") <= 1) {
				script_debugger->clear_breakpoints();
			} else {
				Pair<String, int> breakpoint = to_breakpoint(line);

				String source = breakpoint.first;
				int linenr = breakpoint.second;

				if (source.is_empty()) {
					continue;
				}

				script_debugger->remove_breakpoint(linenr, source);

				print_line("Removed breakpoint at " + source + ":" + itos(linenr));
			}

		} else if (line == "h" || line == "help") {
			print_line("Built-In Debugger command list:\n");
			print_line("\tc,continue\t\t Continue execution.");
			print_line("\tbt,backtrace\t\t Show stack trace (frames).");
			print_line("\tfr,frame <frame>:\t Change current frame.");
			print_line("\tlv,locals\t\t Show local variables for current frame.");
			print_line("\tmv,members\t\t Show member variables for \"this\" in frame.");
			print_line("\tgv,globals\t\t Show global variables.");
			print_line("\tp,print <expr>\t\t Execute and print variable in expression.");
			print_line("\ts,step\t\t\t Step to next line.");
			print_line("\tn,next\t\t\t Next line.");
			print_line("\tfin,finish\t\t Step out of current frame.");
			print_line("\tbr,break [source:line]\t List all breakpoints or place a breakpoint.");
			print_line("\tdelete [source:line]:\t Delete one/all breakpoints.");
			print_line("\tset [key=value]:\t List all options, or set one.");
			print_line("\tq,quit\t\t\t Quit application.");
		} else {
			print_line("Error: Invalid command, enter \"help\" for assistance.");
		}
	}
}

void LocalDebugger::print_variables(const List<String> &names, const List<Variant> &values, const String &variable_prefix) {
	String value;
	Vector<String> value_lines;
	const List<Variant>::Element *V = values.front();
	for (const String &E : names) {
		value = String(V->get());

		if (variable_prefix.is_empty()) {
			print_line(E + ": " + String(V->get()));
		} else {
			print_line(E + ":");
			value_lines = value.split("\n");
			for (int i = 0; i < value_lines.size(); ++i) {
				print_line(variable_prefix + value_lines[i]);
			}
		}

		V = V->next();
	}
}

Pair<String, int> LocalDebugger::to_breakpoint(const String &p_line) {
	String breakpoint_part = p_line.get_slicec(' ', 1);
	Pair<String, int> breakpoint;

	int last_colon = breakpoint_part.rfind_char(':');
	if (last_colon < 0) {
		print_line("Error: Invalid breakpoint format. Expected [source:line]");
		return breakpoint;
	}

	breakpoint.first = script_debugger->breakpoint_find_source(breakpoint_part.left(last_colon).strip_edges());
	breakpoint.second = breakpoint_part.substr(last_colon).strip_edges().to_int();

	return breakpoint;
}

void LocalDebugger::send_message(const String &p_message, const Array &p_args) {
	// This needs to be cleaned up entirely.
	// print_line("MESSAGE: '" + p_message + "' - " + String(Variant(p_args)));
}

void LocalDebugger::send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, bool p_editor_notify, ErrorHandlerType p_type) {
	print_line("ERROR: '" + (p_descr.is_empty() ? p_err : p_descr) + "'");
}

LocalDebugger::LocalDebugger() {
	options["variable_prefix"] = "";

	// Bind scripts profiler.
	scripts_profiler = memnew(ScriptsProfiler);
	Profiler scr_prof(
			scripts_profiler,
			[](void *p_user, bool p_enable, const Array &p_opts) {
				static_cast<ScriptsProfiler *>(p_user)->toggle(p_enable, p_opts);
			},
			nullptr,
			[](void *p_user, double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
				static_cast<ScriptsProfiler *>(p_user)->tick(p_frame_time, p_process_time, p_physics_time, p_physics_frame_time);
			});
	register_profiler("scripts", scr_prof);
}

LocalDebugger::~LocalDebugger() {
	unregister_profiler("scripts");
	if (scripts_profiler) {
		memdelete(scripts_profiler);
	}
}
