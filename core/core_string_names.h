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

#include "core/string_name.h"

class CoreStringNames {
	friend void register_core_types();
	friend void unregister_core_types();

	static void create() { singleton = memnew(CoreStringNames); }
	static void free() {
		memdelete(singleton);
		singleton = nullptr;
	}

	CoreStringNames();

public:
	_FORCE_INLINE_ static CoreStringNames *get_singleton() { return singleton; }

	static CoreStringNames *singleton;

	StringName _free;
	StringName changed;
	StringName _meta;
	StringName _script;
	StringName script_changed;
	StringName ___pdcdata;
	StringName __getvar;
	StringName _iter_init;
	StringName _iter_next;
	StringName _iter_get;
	StringName get_rid;
	StringName _to_string;
#ifdef TOOLS_ENABLED
	StringName _sections_unfolded;
#endif
	StringName _custom_features;

	StringName x;
	StringName y;
	StringName z;
	StringName w;
	StringName xy;
	StringName xz;
	StringName yx;
	StringName yz;
	StringName zx;
	StringName zy;
	StringName r;
	StringName g;
	StringName b;
	StringName a;
	StringName position;
	StringName size;
	StringName end;
	StringName basis;
	StringName origin;
	StringName normal;
	StringName d;
	StringName h;
	StringName s;
	StringName v;
	StringName r8;
	StringName g8;
	StringName b8;
	StringName a8;
};

#endif // CORE_STRING_NAMES_H
