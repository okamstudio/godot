/**************************************************************************/
/*  core_string_names.h                                                   */
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

#ifndef CORE_STRING_NAMES_H
#define CORE_STRING_NAMES_H

#include "core/string/string_name.h"

class CoreStringNames {
	inline static CoreStringNames *singleton = nullptr;

public:
	static void create() { singleton = memnew(CoreStringNames); }
	static void free() {
		memdelete(singleton);
		singleton = nullptr;
	}

	_FORCE_INLINE_ static CoreStringNames *get_singleton() { return singleton; }

	const StringName free_ = StaticCString::create("free"); // free would conflict with C++ keyword.
	const StringName changed = StaticCString::create("changed");
	const StringName script = StaticCString::create("script");
	const StringName script_changed = StaticCString::create("script_changed");
	const StringName _iter_init = StaticCString::create("_iter_init");
	const StringName _iter_next = StaticCString::create("_iter_next");
	const StringName _iter_get = StaticCString::create("_iter_get");
	const StringName get_rid = StaticCString::create("get_rid");
	const StringName _to_string = StaticCString::create("_to_string");
	const StringName _custom_features = StaticCString::create("_custom_features");

	const StringName x = StaticCString::create("x");
	const StringName y = StaticCString::create("y");
	const StringName z = StaticCString::create("z");
	const StringName w = StaticCString::create("w");
	const StringName r = StaticCString::create("r");
	const StringName g = StaticCString::create("g");
	const StringName b = StaticCString::create("b");
	const StringName a = StaticCString::create("a");
	const StringName position = StaticCString::create("position");
	const StringName size = StaticCString::create("size");
	const StringName end = StaticCString::create("end");
	const StringName basis = StaticCString::create("basis");
	const StringName origin = StaticCString::create("origin");
	const StringName normal = StaticCString::create("normal");
	const StringName d = StaticCString::create("d");
	const StringName h = StaticCString::create("h");
	const StringName s = StaticCString::create("s");
	const StringName v = StaticCString::create("v");
	const StringName r8 = StaticCString::create("r8");
	const StringName g8 = StaticCString::create("g8");
	const StringName b8 = StaticCString::create("b8");
	const StringName a8 = StaticCString::create("a8");

	const StringName call = StaticCString::create("call");
	const StringName call_deferred = StaticCString::create("call_deferred");
	const StringName bind = StaticCString::create("bind");
	const StringName notification = StaticCString::create("notification");
	const StringName property_list_changed = StaticCString::create("property_list_changed");
};

#define CoreStringName(m_name) CoreStringNames::get_singleton()->m_name

#endif // CORE_STRING_NAMES_H
