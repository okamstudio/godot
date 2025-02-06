/**************************************************************************/
/*  color_utils.cpp                                                       */
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

#include "color_utils.h"

#include "named_colors.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/typedefs.h"

#include "thirdparty/misc/ok_color.h"

void ColorUtils::_bind_methods() {
	ClassDB::bind_static_method("ColorUtils", D_METHOD("html_is_valid", "html"), &ColorUtils::html_is_valid);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_html", "html"), &ColorUtils::from_html);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("name_is_valid", "name"), &ColorUtils::name_is_valid);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_name", "name"), &ColorUtils::from_name);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_string", "string", "default"), &ColorUtils::from_string, DEFVAL(Color()));

	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_hsv", "hsv"), &ColorUtils::from_hsv);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_hsl", "hsl"), &ColorUtils::from_hsl);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_ok_hsv", "hsv"), &ColorUtils::from_ok_hsv);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_ok_hsl", "hsl"), &ColorUtils::from_ok_hsl);

	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_rgba32", "rgba"), &ColorUtils::from_rgba32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_argb32", "argb"), &ColorUtils::from_argb32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_abgr32", "abgr"), &ColorUtils::from_abgr32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_rgba64", "rgba"), &ColorUtils::from_rgba64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_argb64", "argb"), &ColorUtils::from_argb64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_abgr64", "abgr"), &ColorUtils::from_abgr64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("from_rgbe9995", "rgbe"), &ColorUtils::from_rgbe9995);

	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_html", "color", "with_alpha"), &ColorUtils::to_html, DEFVAL(true));

	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_hsv", "color"), &ColorUtils::to_hsv);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_hsl", "color"), &ColorUtils::to_hsl);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_ok_hsv", "color"), &ColorUtils::to_ok_hsv);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_ok_hsl", "color"), &ColorUtils::to_ok_hsl);

	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_rgba32", "color"), &ColorUtils::to_rgba32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_argb32", "color"), &ColorUtils::to_argb32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_abgr32", "color"), &ColorUtils::to_abgr32);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_rgba64", "color"), &ColorUtils::to_rgba64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_argb64", "color"), &ColorUtils::to_argb64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_abgr64", "color"), &ColorUtils::to_abgr64);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("to_rgbe9995", "color"), &ColorUtils::to_rgbe9995);

	ClassDB::bind_static_method("ColorUtils", D_METHOD("srgb_to_linear", "color"), &ColorUtils::srgb_to_linear);
	ClassDB::bind_static_method("ColorUtils", D_METHOD("linear_to_srgb", "color"), &ColorUtils::linear_to_srgb);
}

static int _parse_col4(const String &p_str, int p_ofs) {
	char character = p_str[p_ofs];

	if (character >= '0' && character <= '9') {
		return character - '0';
	} else if (character >= 'a' && character <= 'f') {
		return character + (10 - 'a');
	} else if (character >= 'A' && character <= 'F') {
		return character + (10 - 'A');
	}

	return -1;
}

static int _parse_col8(const String &p_str, int p_ofs) {
	return _parse_col4(p_str, p_ofs) * 16 + _parse_col4(p_str, p_ofs + 1);
}

bool ColorUtils::html_is_valid(const String &p_html) {
	String color = p_html;

	if (color.length() == 0) {
		return false;
	}
	if (color[0] == '#') {
		color = color.substr(1);
	}

	// Check if the amount of hex digits is valid.
	int len = color.length();
	if (!(len == 3 || len == 4 || len == 6 || len == 8)) {
		return false;
	}

	// Check if each hex digit is valid.
	for (int i = 0; i < len; i++) {
		if (_parse_col4(color, i) == -1) {
			return false;
		}
	}

	return true;
}

Color ColorUtils::from_html(const String &p_html) {
	String color = p_html;

	if (color.length() == 0) {
		return Color();
	}
	if (color[0] == '#') {
		color = color.substr(1);
	}

	// If enabled, use 1 hex digit per channel instead of 2.
	// Other sizes aren't in the HTML/CSS spec but we could add them if desired.
	bool is_shorthand = color.length() < 5;
	bool with_alpha = false;

	if (color.length() == 8) {
		with_alpha = true;
	} else if (color.length() == 6) {
		with_alpha = false;
	} else if (color.length() == 4) {
		with_alpha = true;
	} else if (color.length() == 3) {
		with_alpha = false;
	} else {
		ERR_FAIL_V_MSG(Color(), "Invalid color code: " + p_html + ".");
	}

	float r, g, b, a = 1.0f;

	if (is_shorthand) {
		r = _parse_col4(color, 0) / 15.0f;
		g = _parse_col4(color, 1) / 15.0f;
		b = _parse_col4(color, 2) / 15.0f;
		if (with_alpha) {
			a = _parse_col4(color, 3) / 15.0f;
		}
	} else {
		r = _parse_col8(color, 0) / 255.0f;
		g = _parse_col8(color, 2) / 255.0f;
		b = _parse_col8(color, 4) / 255.0f;
		if (with_alpha) {
			a = _parse_col8(color, 6) / 255.0f;
		}
	}

	ERR_FAIL_COND_V_MSG(r < 0.0f, Color(), "Invalid color code: " + p_html + ".");
	ERR_FAIL_COND_V_MSG(g < 0.0f, Color(), "Invalid color code: " + p_html + ".");
	ERR_FAIL_COND_V_MSG(b < 0.0f, Color(), "Invalid color code: " + p_html + ".");
	ERR_FAIL_COND_V_MSG(a < 0.0f, Color(), "Invalid color code: " + p_html + ".");

	return Color(r, g, b, a);
}

static int _get_index(const String &p_name) {
	String name;

	for (const char32_t *s = p_name.ptr(); *s; s++) {
		if (is_ascii_upper_case(*s)) {
			name += *s;
		} else if (is_ascii_lower_case(*s)) {
			name += String::char_uppercase(*s);
		}
	}

	int index = 0;

	for (const NamedColor &named_color : named_colors) {
		if (name == named_color.name.replace("_", "")) {
			return index;
		}

		index++;
	}

	return -1;
}

bool ColorUtils::name_is_valid(const String &p_name) {
	int index = _get_index(p_name);

	if (index == -1) {
		return false;
	}

	return true;
}

Color ColorUtils::from_name(const String &p_name) {
	int index = _get_index(p_name);

	if (index == -1) {
		ERR_FAIL_V_MSG(Color(), "Invalid color name: " + p_name + ".");
	}

	return named_colors[index].color;
}

Color ColorUtils::from_string(const String &p_string, const Color &p_default) {
	if (html_is_valid(p_string)) {
		return from_html(p_string);
	}

	int index = _get_index(p_string);

	if (index == -1) {
		return p_default;
	}

	return named_colors[index].color;
}

Color _from_hcxm(float p_h, float p_c, float p_x, float p_m) {
	p_h = Math::fmod(6.0f * p_h, 6.0f);

	p_c += p_m;
	p_x += p_m;

	switch (static_cast<int>(p_h)) {
		case 0:
			return Color(p_c, p_x, p_m);
		case 1:
			return Color(p_x, p_c, p_m);
		case 2:
			return Color(p_m, p_c, p_x);
		case 3:
			return Color(p_m, p_x, p_c);
		case 4:
			return Color(p_x, p_m, p_c);
		default:
			return Color(p_c, p_m, p_x);
	}
}

Color ColorUtils::from_hsv(const Vector3 &p_hsv) {
	float h = static_cast<float>(p_hsv.x);
	float s = static_cast<float>(p_hsv.y);
	float v = static_cast<float>(p_hsv.z);

	if (s == 0.0f) {
		return Color(v, v, v);
	}

	float c = v * s;
	float x = c * (1.0f - Math::abs(Math::fmod(6.0f * h, 2.0f) - 1.0f));
	float m = v - c;

	return _from_hcxm(h, c, x, m);
}

Color ColorUtils::from_hsl(const Vector3 &p_hsl) {
	float h = static_cast<float>(p_hsl.x);
	float s = static_cast<float>(p_hsl.y);
	float l = static_cast<float>(p_hsl.z);

	if (s == 0.0f) {
		return Color(l, l, l);
	}

	float c = (1.0f - Math::abs(2.0f * l - 1.0f)) * s;
	float x = c * (1.0f - Math::abs(Math::fmod(6.0f * h, 2.0f) - 1.0f));
	float m = l - c / 2.0f;

	return _from_hcxm(h, c, x, m);
}

Color ColorUtils::from_ok_hsv(const Vector3 &p_hsv) {
	float h = static_cast<float>(p_hsv.x);
	float s = static_cast<float>(p_hsv.y);
	float v = static_cast<float>(p_hsv.z);

	ok_color::HSV hsv = { h, s, v };
	ok_color::RGB rgb = ok_color::okhsv_to_srgb(hsv);

	return Color(rgb.r, rgb.g, rgb.b);
}

Color ColorUtils::from_ok_hsl(const Vector3 &p_hsl) {
	float h = static_cast<float>(p_hsl.x);
	float s = static_cast<float>(p_hsl.y);
	float l = static_cast<float>(p_hsl.z);

	ok_color::HSL hsl = { h, s, l };
	ok_color::RGB rgb = ok_color::okhsl_to_srgb(hsl);

	return Color(rgb.r, rgb.g, rgb.b);
}

Color ColorUtils::from_rgba32(uint32_t p_rgba) {
	float a = (p_rgba & 0xFF) / 255.0f;
	p_rgba >>= 8;
	float b = (p_rgba & 0xFF) / 255.0f;
	p_rgba >>= 8;
	float g = (p_rgba & 0xFF) / 255.0f;
	p_rgba >>= 8;
	float r = (p_rgba & 0xFF) / 255.0f;

	return Color(r, g, b, a);
}

Color ColorUtils::from_argb32(uint32_t p_argb) {
	Color color = from_rgba32(p_argb);

	return Color(color.a, color.r, color.g, color.b);
}

Color ColorUtils::from_abgr32(uint32_t p_abgr) {
	Color color = from_rgba32(p_abgr);

	return Color(color.a, color.b, color.g, color.r);
}

Color ColorUtils::from_rgba64(uint64_t p_rgba) {
	float a = (p_rgba & 0xFFFF) / 65535.0f;
	p_rgba >>= 16;
	float b = (p_rgba & 0xFFFF) / 65535.0f;
	p_rgba >>= 16;
	float g = (p_rgba & 0xFFFF) / 65535.0f;
	p_rgba >>= 16;
	float r = (p_rgba & 0xFFFF) / 65535.0f;

	return Color(r, g, b, a);
}

Color ColorUtils::from_argb64(uint64_t p_argb) {
	Color color = from_rgba64(p_argb);

	return Color(color.a, color.r, color.g, color.b);
}

Color ColorUtils::from_abgr64(uint64_t p_abgr) {
	Color color = from_rgba64(p_abgr);

	return Color(color.a, color.b, color.g, color.r);
}

Color ColorUtils::from_rgbe9995(uint32_t p_rgbe) {
	float r = p_rgbe & 0x1ff;
	float g = (p_rgbe >> 9) & 0x1ff;
	float b = (p_rgbe >> 18) & 0x1ff;
	float e = (p_rgbe >> 27);
	float m = Math::pow(2.0f, e - 15.0f - 9.0f);

	float rd = r * m;
	float gd = g * m;
	float bd = b * m;

	return Color(rd, gd, bd, 1.0f);
}

static String _to_hex(float p_val) {
	String hex;

	int v = Math::round(p_val * 255.0f);
	v = CLAMP(v, 0, 255);

	for (int i = 0; i < 2; i++) {
		char32_t c[2] = { 0, 0 };
		int lv = v & 0xF;
		if (lv < 10) {
			c[0] = '0' + lv;
		} else {
			c[0] = 'a' + lv - 10;
		}

		v >>= 4;
		String cs = const_cast<const char32_t *>(c);
		hex = cs + hex;
	}

	return hex;
}

String ColorUtils::to_html(const Color &p_color, bool p_with_alpha) {
	String html;

	html += _to_hex(p_color.r);
	html += _to_hex(p_color.g);
	html += _to_hex(p_color.b);

	if (p_with_alpha) {
		html += _to_hex(p_color.a);
	}

	return html;
}

static float _calc_hue(const Color &p_color, float p_max, float p_delta) {
	if (p_delta == 0.0f) {
		return 0.0f;
	}

	float h;
	if (p_color.r == p_max) {
		h = 0.0f + (p_color.g - p_color.b) / p_delta;
	} else if (p_color.g == p_max) {
		h = 2.0f + (p_color.b - p_color.r) / p_delta;
	} else {
		h = 4.0f + (p_color.r - p_color.g) / p_delta;
	}

	return Math::fposmod(h / 6.0f, 1.0f);
}

Vector3 ColorUtils::to_hsv(const Color &p_color) {
	float min = MIN(MIN(p_color.r, p_color.g), p_color.b);
	float max = MAX(MAX(p_color.r, p_color.g), p_color.b);
	float delta = max - min;

	float v = max;
	float s = (v != 0.0f) ? (delta / v) : 0.0f;
	float h = _calc_hue(p_color, max, delta);

	return Vector3(static_cast<real_t>(h), static_cast<real_t>(s), static_cast<real_t>(v));
}

Vector3 ColorUtils::to_hsl(const Color &p_color) {
	float min = MIN(MIN(p_color.r, p_color.g), p_color.b);
	float max = MAX(MAX(p_color.r, p_color.g), p_color.b);
	float delta = max - min;

	float l = (min + max) / 2.0f;
	float s = (l != 0.0f && l != 1.0f) ? (delta / (1.0f - Math::abs(2.0f * l - 1.0f))) : 0.0f;
	float h = _calc_hue(p_color, max, delta);

	return Vector3(static_cast<real_t>(h), static_cast<real_t>(s), static_cast<real_t>(l));
}

static float _clamp_safe(float p_value) {
	if (Math::is_nan(p_value)) {
		return 0.0f;
	}

	return CLAMP(p_value, 0.0f, 1.0f);
}

Vector3 ColorUtils::to_ok_hsv(const Color &p_color) {
	if (p_color.r == p_color.g && p_color.g == p_color.b) {
		// Fallback to avoid edge cases with NaN value.
		return to_ok_hsl(p_color);
	}

	ok_color::RGB rgb = { p_color.r, p_color.g, p_color.b };
	ok_color::HSV hsv = ok_color::srgb_to_okhsv(rgb);

	hsv.h = _clamp_safe(hsv.h);
	hsv.s = _clamp_safe(hsv.s);
	hsv.v = _clamp_safe(hsv.v);

	return Vector3(static_cast<real_t>(hsv.h), static_cast<real_t>(hsv.s), static_cast<real_t>(hsv.v));
}

Vector3 ColorUtils::to_ok_hsl(const Color &p_color) {
	ok_color::RGB rgb = { p_color.r, p_color.g, p_color.b };
	ok_color::HSL hsl = ok_color::srgb_to_okhsl(rgb);

	if (p_color.r == p_color.g && p_color.g == p_color.b) {
		hsl.h = 0.0f;
		hsl.s = 0.0f;
	}

	hsl.h = _clamp_safe(hsl.h);
	hsl.s = _clamp_safe(hsl.s);
	hsl.l = _clamp_safe(hsl.l);

	return Vector3(static_cast<real_t>(hsl.h), static_cast<real_t>(hsl.s), static_cast<real_t>(hsl.l));
}

uint32_t ColorUtils::to_rgba32(const Color &p_color) {
	uint32_t rgba = static_cast<uint8_t>(Math::round(p_color.r * 255.0f));
	rgba <<= 8;
	rgba |= static_cast<uint8_t>(Math::round(p_color.g * 255.0f));
	rgba <<= 8;
	rgba |= static_cast<uint8_t>(Math::round(p_color.b * 255.0f));
	rgba <<= 8;
	rgba |= static_cast<uint8_t>(Math::round(p_color.a * 255.0f));

	return rgba;
}

uint32_t ColorUtils::to_argb32(const Color &p_color) {
	Color color = Color(p_color.a, p_color.r, p_color.g, p_color.b);

	return to_rgba32(color);
}

uint32_t ColorUtils::to_abgr32(const Color &p_color) {
	Color color = Color(p_color.a, p_color.b, p_color.g, p_color.r);

	return to_rgba32(color);
}

uint64_t ColorUtils::to_rgba64(const Color &p_color) {
	uint64_t rgba = static_cast<uint16_t>(Math::round(p_color.r * 65535.0f));
	rgba <<= 16;
	rgba |= static_cast<uint16_t>(Math::round(p_color.g * 65535.0f));
	rgba <<= 16;
	rgba |= static_cast<uint16_t>(Math::round(p_color.b * 65535.0f));
	rgba <<= 16;
	rgba |= static_cast<uint16_t>(Math::round(p_color.a * 65535.0f));

	return rgba;
}

uint64_t ColorUtils::to_argb64(const Color &p_color) {
	Color color = Color(p_color.a, p_color.r, p_color.g, p_color.b);

	return to_rgba64(color);
}

uint64_t ColorUtils::to_abgr64(const Color &p_color) {
	Color color = Color(p_color.a, p_color.b, p_color.g, p_color.r);

	return to_rgba64(color);
}

uint32_t ColorUtils::to_rgbe9995(const Color &p_color) {
	const float pow2to9 = 512.0f;
	const float B = 15.0f;
	const float N = 9.0f;

	// Result of: ((pow2to9 - 1.0f) / pow2to9) * powf(2.0f, 31.0f - 15.0f).
	float sharedexp = 65408.000f;
	float cRed = MAX(0.0f, MIN(sharedexp, p_color.r));
	float cGreen = MAX(0.0f, MIN(sharedexp, p_color.g));
	float cBlue = MAX(0.0f, MIN(sharedexp, p_color.b));

	float cMax = MAX(cRed, MAX(cGreen, cBlue));
	float expp = MAX(-B - 1.0f, Math::floor(Math::log(cMax) / static_cast<real_t>(Math_LN2))) + 1.0f + B;
	float sMax = Math::floor((cMax / Math::pow(2.0f, expp - B - N)) + 0.5f);
	float exps = expp + 1.0f;

	if (0.0f <= sMax && sMax < pow2to9) {
		exps = expp;
	}

	float sRed = Math::floor((cRed / Math::pow(2.0f, exps - B - N)) + 0.5f);
	float sGreen = Math::floor((cGreen / Math::pow(2.0f, exps - B - N)) + 0.5f);
	float sBlue = Math::floor((cBlue / Math::pow(2.0f, exps - B - N)) + 0.5f);

	uint32_t r = (static_cast<uint32_t>(Math::fast_ftoi(sRed)) & 0x1FF);
	uint32_t g = (static_cast<uint32_t>(Math::fast_ftoi(sGreen)) & 0x1FF) << 9;
	uint32_t b = (static_cast<uint32_t>(Math::fast_ftoi(sBlue)) & 0x1FF) << 18;
	uint32_t e = (static_cast<uint32_t>(Math::fast_ftoi(exps)) & 0x1F) << 27;

	return r | g | b | e;
}

float _srgb_to_linear(float x) {
	if (x <= 0.04045f) {
		return x / 12.92f;
	} else {
		return Math::pow((x + 0.055f) / 1.055f, 2.4f);
	}
}

Color ColorUtils::srgb_to_linear(const Color &p_color) {
	float r = _srgb_to_linear(p_color.r);
	float g = _srgb_to_linear(p_color.g);
	float b = _srgb_to_linear(p_color.b);

	return Color(r, g, b, p_color.a);
}

float _linear_to_srgb(float x) {
	if (x <= 0.0031308f) {
		return x * 12.92f;
	} else {
		return 1.055f * Math::pow(x, 0.4166666666666667f) - 0.055f;
	}
}

Color ColorUtils::linear_to_srgb(const Color &p_color) {
	float r = _linear_to_srgb(p_color.r);
	float g = _linear_to_srgb(p_color.g);
	float b = _linear_to_srgb(p_color.b);

	return Color(r, g, b, p_color.a);
}
