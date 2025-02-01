/**************************************************************************/
/*  script_iterator.h                                                     */
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

#ifdef GDEXTENSION

// Headers for building as GDExtension plug-in.
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

#elif defined(GODOT_MODULE)

// Headers for building as built-in module.
#include "core/string/ustring.h"
#include "core/templates/vector.h"

#endif

#include <unicode/uchar.h>
#include <unicode/uloc.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

#include <hb-icu.h>
#include <hb.h>

class ScriptIterator {
	static const int PAREN_STACK_DEPTH = 128;

public:
	struct ScriptRange {
		int start = 0;
		int end = 0;
		hb_script_t script = HB_SCRIPT_COMMON;
	};
	Vector<ScriptRange> script_ranges;

private:
	static bool same_script(int32_t p_script_one, int32_t p_script_two);

public:
	ScriptIterator(const String &p_string, int p_start, int p_length);
};
