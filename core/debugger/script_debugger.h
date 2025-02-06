/**************************************************************************/
/*  script_debugger.h                                                     */
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

#ifndef SCRIPT_DEBUGGER_H
#define SCRIPT_DEBUGGER_H

#include "core/object/script_language.h"
#include "core/string/string_name.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

struct Breakpoint {
public:
	String source;
	int line = 0;
	bool enabled = true;
	bool suspend = true;
	String condition;
	String print;

	static uint32_t hash(const Breakpoint &p_val) {
		uint32_t h = HashMapHasherDefault::hash(p_val.source);
		return hash_murmur3_one_32(p_val.line, h);
	}
	bool operator==(const Breakpoint &p_b) const {
		return (line == p_b.line && source == p_b.source);
	}

	bool operator<(const Breakpoint &p_b) const {
		if (line == p_b.line) {
			return source < p_b.source;
		}
		return line < p_b.line;
	}

	Dictionary serialize() const {
		Dictionary dict;
		dict["source"] = source;
		dict["enabled"] = enabled;
		dict["suspend"] = suspend;
		dict["line"] = line;
		dict["condition"] = condition;
		dict["print"] = print;
		return dict;
	}

	static Breakpoint deserialize(const Dictionary &dict) {
		Breakpoint bp = Breakpoint();
		bp.source = dict["source"];
		bp.enabled = dict["enabled"];
		bp.suspend = dict["suspend"];
		bp.line = dict["line"];
		bp.condition = dict["condition"];
		bp.print = dict["print"];
		return bp;
	}

	Breakpoint() {}

	Breakpoint(const String &p_source, int p_line, bool p_enabled = true, bool p_suspend = true, const String &p_condition = "", const String &p_print = "") {
		line = p_line;
		source = p_source;
		enabled = p_enabled;
		suspend = p_suspend;
		condition = p_condition;
		print = p_print;
	}
};

class ScriptDebugger {
	typedef ScriptLanguage::StackInfo StackInfo;

	HashMap<StringName, HashMap<int, Breakpoint>> breakpoints;
	bool skip_breakpoints = false;

	static thread_local int lines_left;
	static thread_local int depth;
	static thread_local ScriptLanguage *break_lang;
	static thread_local Vector<StackInfo> error_stack_info;

public:
	void set_lines_left(int p_left);
	_ALWAYS_INLINE_ int get_lines_left() const {
		return lines_left;
	}

	void set_depth(int p_depth);
	_ALWAYS_INLINE_ int get_depth() const {
		return depth;
	}

	String breakpoint_find_source(const String &p_source) const;
	void set_break_language(ScriptLanguage *p_lang) { break_lang = p_lang; }
	ScriptLanguage *get_break_language() { return break_lang; }
	void set_skip_breakpoints(bool p_skip_breakpoints);
	bool is_skipping_breakpoints();
	void insert_breakpoint(int p_line, const StringName &p_source, const Breakpoint &p_breakpoint);
	void remove_breakpoint(int p_line, const StringName &p_source);
	_ALWAYS_INLINE_ bool is_breakpoint(int p_line, const StringName &p_source) const {
		if (likely(!breakpoints.has(p_source))) {
			return false;
		}
		return breakpoints[p_source].has(p_line);
	}
	void clear_breakpoints();
	const HashMap<StringName, HashMap<int, Breakpoint>> &get_breakpoints() const { return breakpoints; }

	void debug(ScriptLanguage *p_lang, bool p_can_continue = true, bool p_is_error_breakpoint = false);
	ScriptLanguage *get_break_language() const;

	void send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, bool p_editor_notify, ErrorHandlerType p_type, const Vector<StackInfo> &p_stack_info);
	Vector<StackInfo> get_error_stack_info() const;
	ScriptDebugger() {}
};

#endif // SCRIPT_DEBUGGER_H
