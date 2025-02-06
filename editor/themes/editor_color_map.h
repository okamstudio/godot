/**************************************************************************/
/*  editor_color_map.h                                                    */
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

#include "core/math/color.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

// The default icon theme is designed to be used for a dark theme. This map stores
// Color values to convert to other colors for better readability on a light theme.
class EditorColorMap {
	// Godot Color values are used to avoid the ambiguity of strings
	// (where "#ffffff", "fff", and "white" are all equivalent).
	static HashMap<Color, Color> color_conversion_map;
	// The names of the icons to never convert, even if one of their colors
	// are contained in the color map from above.
	static HashSet<StringName> color_conversion_exceptions;

public:
	static void add_conversion_color_pair(const String &p_from_color, const String &p_to_color);
	static void add_conversion_exception(const StringName &p_icon_name);

	static HashMap<Color, Color> &get_color_conversion_map() { return color_conversion_map; }
	static HashSet<StringName> &get_color_conversion_exceptions() { return color_conversion_exceptions; }

	static void create();
	static void finish();
};
