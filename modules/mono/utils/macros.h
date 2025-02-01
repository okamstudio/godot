/**************************************************************************/
/*  macros.h                                                              */
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

#pragma once

#define _GD_VARNAME_CONCAT_B_(m_ignore, m_name) m_name
#define _GD_VARNAME_CONCAT_A_(m_a, m_b, m_c) _GD_VARNAME_CONCAT_B_(hello there, m_a##m_b##m_c)
#define _GD_VARNAME_CONCAT_(m_a, m_b, m_c) _GD_VARNAME_CONCAT_A_(m_a, m_b, m_c)
#define GD_UNIQUE_NAME(m_name) _GD_VARNAME_CONCAT_(m_name, _, __COUNTER__)

// unreachable

#if defined(_MSC_VER)
#define GD_UNREACHABLE() __assume(0)
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 405
#define GD_UNREACHABLE() __builtin_unreachable()
#else
#define GD_UNREACHABLE() \
	CRASH_NOW();         \
	do {                 \
	} while (true)
#endif

namespace gdmono {

template <typename F>
struct ScopeExit {
	ScopeExit(F p_exit_func) :
			exit_func(p_exit_func) {}
	~ScopeExit() { exit_func(); }
	F exit_func;
};

class ScopeExitAux {
public:
	template <typename F>
	ScopeExit<F> operator+(F p_exit_func) { return ScopeExit<F>(p_exit_func); }
};
} // namespace gdmono

#define SCOPE_EXIT \
	auto GD_UNIQUE_NAME(gd_scope_exit) = gdmono::ScopeExitAux() + [=]() -> void
