/**************************************************************************/
/*  core_string_names.cpp                                                 */
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

#include "core_string_names.h"

CoreStringNames *CoreStringNames::singleton = nullptr;

CoreStringNames::CoreStringNames() :
		_free(StaticCString::create("free")),
		changed(StaticCString::create("changed")),
		_meta(StaticCString::create("__meta__")),
		_script(StaticCString::create("script")),
		script_changed(StaticCString::create("script_changed")),
		___pdcdata(StaticCString::create("___pdcdata")),
		__getvar(StaticCString::create("__getvar")),
		_iter_init(StaticCString::create("_iter_init")),
		_iter_next(StaticCString::create("_iter_next")),
		_iter_get(StaticCString::create("_iter_get")),
		get_rid(StaticCString::create("get_rid")),
		_to_string(StaticCString::create("_to_string")),
#ifdef TOOLS_ENABLED
		_sections_unfolded(StaticCString::create("_sections_unfolded")),
#endif
		_custom_features(StaticCString::create("_custom_features")),
		x(StaticCString::create("x")),
		y(StaticCString::create("y")),
		z(StaticCString::create("z")),
		w(StaticCString::create("w")),
		xy(StaticCString::create("xy")),
		xz(StaticCString::create("xz")),
		yx(StaticCString::create("yx")),
		yz(StaticCString::create("yz")),
		zx(StaticCString::create("zx")),
		zy(StaticCString::create("zy")),
		r(StaticCString::create("r")),
		g(StaticCString::create("g")),
		b(StaticCString::create("b")),
		a(StaticCString::create("a")),
		position(StaticCString::create("position")),
		size(StaticCString::create("size")),
		end(StaticCString::create("end")),
		basis(StaticCString::create("basis")),
		origin(StaticCString::create("origin")),
		normal(StaticCString::create("normal")),
		d(StaticCString::create("d")),
		h(StaticCString::create("h")),
		s(StaticCString::create("s")),
		v(StaticCString::create("v")),
		r8(StaticCString::create("r8")),
		g8(StaticCString::create("g8")),
		b8(StaticCString::create("b8")),
		a8(StaticCString::create("a8")) {
}
