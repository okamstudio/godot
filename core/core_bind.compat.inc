/**************************************************************************/
/*  core_bind.compat.inc                                                  */
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

#ifndef DISABLE_DEPRECATED

namespace core_bind {

// Semaphore

void Semaphore::_post_bind_compat_93605() {
	post(1);
}

void Semaphore::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("post"), &Semaphore::_post_bind_compat_93605);
}

// OS

String OS::_read_string_from_stdin_bind_compat_91201() {
	return read_string_from_stdin(1024);
}

Dictionary OS::_execute_with_pipe_bind_compat_94434(const String &p_path, const Vector<String> &p_arguments) {
	return execute_with_pipe(p_path, p_arguments, true);
}

void OS::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("read_string_from_stdin"), &OS::_read_string_from_stdin_bind_compat_91201);
	ClassDB::bind_compatibility_method(D_METHOD("execute_with_pipe", "path", "arguments"), &OS::_execute_with_pipe_bind_compat_94434);
}

// Debugger

void EngineDebugger::_insert_breakpoint_bind_compat_100516(int p_line, const StringName &p_source) {
	insert_breakpoint(p_line, p_source);
}

void EngineDebugger::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("insert_breakpoint", "line", "source"), &EngineDebugger::_insert_breakpoint_bind_compat_100516);
}

} // namespace core_bind

#endif // DISABLE_DEPRECATED
