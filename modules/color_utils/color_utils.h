/**************************************************************************/
/*  color_utils.h                                                         */
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

#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include "core/math/color.h"
#include "core/math/vector3.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"

class ColorUtils : public Object {
	GDCLASS(ColorUtils, Object);

protected:
	static void _bind_methods();

public:
	static bool html_is_valid(const String &p_html);
	static Color from_html(const String &p_html);
	static bool name_is_valid(const String &p_name);
	static Color from_name(const String &p_name);
	static Color from_string(const String &p_string, const Color &p_default = Color());

	static Color from_hsv(const Vector3 &p_hsv);
	static Color from_hsl(const Vector3 &p_hsl);
	static Color from_ok_hsv(const Vector3 &p_hsv);
	static Color from_ok_hsl(const Vector3 &p_hsl);

	static Color from_rgba32(uint32_t p_rgba);
	static Color from_argb32(uint32_t p_argb);
	static Color from_abgr32(uint32_t p_abgr);
	static Color from_rgba64(uint64_t p_rgba);
	static Color from_argb64(uint64_t p_argb);
	static Color from_abgr64(uint64_t p_abgr);
	static Color from_rgbe9995(uint32_t p_rgbe);

	static String to_html(const Color &p_color, bool p_with_alpha = true);

	static Vector3 to_hsv(const Color &p_color);
	static Vector3 to_hsl(const Color &p_color);
	static Vector3 to_ok_hsv(const Color &p_color);
	static Vector3 to_ok_hsl(const Color &p_color);

	static uint32_t to_rgba32(const Color &p_color);
	static uint32_t to_argb32(const Color &p_color);
	static uint32_t to_abgr32(const Color &p_color);
	static uint64_t to_rgba64(const Color &p_color);
	static uint64_t to_argb64(const Color &p_color);
	static uint64_t to_abgr64(const Color &p_color);
	static uint32_t to_rgbe9995(const Color &p_color);

	static Color srgb_to_linear(const Color &p_color);
	static Color linear_to_srgb(const Color &p_color);
};

#endif // COLOR_UTILS_H
