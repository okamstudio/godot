/**************************************************************************/
/*  test_color.h                                                          */
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

#include "tests/test_macros.h"

namespace TestColor {

TEST_CASE("[Color] Constructor methods") {
	const Color blue_rgba = Color(0.25098, 0.376471, 1, 0.501961);
	const Color blue_html = Color::html("#4060ff80");
	const Color blue_hex = Color::hex(0x4060ff80);
	const Color blue_hex64 = Color::hex64(0x4040'6060'ffff'8080);

	CHECK_MESSAGE(
			blue_rgba.is_equal_approx(blue_html),
			"Creation with HTML notation should result in components approximately equal to the default constructor.");
	CHECK_MESSAGE(
			blue_rgba.is_equal_approx(blue_hex),
			"Creation with a 32-bit hexadecimal number should result in components approximately equal to the default constructor.");
	CHECK_MESSAGE(
			blue_rgba.is_equal_approx(blue_hex64),
			"Creation with a 64-bit hexadecimal number should result in components approximately equal to the default constructor.");

	ERR_PRINT_OFF;
	const Color html_invalid = Color::html("invalid");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			html_invalid.is_equal_approx(Color()),
			"Creation with invalid HTML notation should result in a Color with the default values.");

	const Color green_rgba = Color(0, 1, 0, 0.25);
	const Color green_hsva = Color(0, 0, 0).from_hsv(120 / 360.0, 1, 1, 0.25);

	CHECK_MESSAGE(
			green_rgba.is_equal_approx(green_hsva),
			"Creation with HSV notation should result in components approximately equal to the default constructor.");
}

TEST_CASE("[Color] Operators") {
	const Color blue = Color(0.2, 0.2, 1);
	const Color dark_red = Color(0.3, 0.1, 0.1);

	// Color components may be negative. Also, the alpha component may be greater than 1.0.
	CHECK_MESSAGE(
			(blue + dark_red).is_equal_approx(Color(0.5, 0.3, 1.1, 2)),
			"Color addition should behave as expected.");
	CHECK_MESSAGE(
			(blue - dark_red).is_equal_approx(Color(-0.1, 0.1, 0.9, 0)),
			"Color subtraction should behave as expected.");
	CHECK_MESSAGE(
			(blue * 2).is_equal_approx(Color(0.4, 0.4, 2, 2)),
			"Color multiplication with a scalar should behave as expected.");
	CHECK_MESSAGE(
			(blue / 2).is_equal_approx(Color(0.1, 0.1, 0.5, 0.5)),
			"Color division with a scalar should behave as expected.");
	CHECK_MESSAGE(
			(blue * dark_red).is_equal_approx(Color(0.06, 0.02, 0.1)),
			"Color multiplication with another Color should behave as expected.");
	CHECK_MESSAGE(
			(blue / dark_red).is_equal_approx(Color(0.666667, 2, 10)),
			"Color division with another Color should behave as expected.");
	CHECK_MESSAGE(
			(-blue).is_equal_approx(Color(0.8, 0.8, 0, 0)),
			"Color negation should behave as expected (affecting the alpha channel, unlike `invert()`).");
}

TEST_CASE("[Color] Reading methods") {
	const Color dark_blue = Color(0, 0, 0.5, 0.4);

	CHECK_MESSAGE(
			dark_blue.get_h() == doctest::Approx(240.0f / 360.0f),
			"The returned HSV hue should match the expected value.");
	CHECK_MESSAGE(
			dark_blue.get_s() == doctest::Approx(1.0f),
			"The returned HSV saturation should match the expected value.");
	CHECK_MESSAGE(
			dark_blue.get_v() == doctest::Approx(0.5f),
			"The returned HSV value should match the expected value.");
}

TEST_CASE("[Color] Conversion methods") {
	const Color cyan = Color(0, 1, 1);
	const Color cyan_transparent = Color(0, 1, 1, 0);

	CHECK_MESSAGE(
			cyan.to_html() == "00ffffff",
			"The returned RGB HTML color code should match the expected value.");
	CHECK_MESSAGE(
			cyan_transparent.to_html() == "00ffff00",
			"The returned RGBA HTML color code should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_argb32() == 0xff00ffff,
			"The returned 32-bit RGB number should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_abgr32() == 0xffffff00,
			"The returned 32-bit BGR number should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_rgba32() == 0x00ffffff,
			"The returned 32-bit BGR number should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_argb64() == 0xffff'0000'ffff'ffff,
			"The returned 64-bit RGB number should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_abgr64() == 0xffff'ffff'ffff'0000,
			"The returned 64-bit BGR number should match the expected value.");
	CHECK_MESSAGE(
			cyan.to_rgba64() == 0x0000'ffff'ffff'ffff,
			"The returned 64-bit BGR number should match the expected value.");
	CHECK_MESSAGE(
			String(cyan) == "(0.0, 1.0, 1.0, 1.0)",
			"The string representation should match the expected value.");
}

TEST_CASE("[Color] Linear <-> sRGB conversion") {
	const Color color = Color(0.35, 0.5, 0.6, 0.7);
	const Color color_linear = color.srgb_to_linear();
	const Color color_srgb = color.linear_to_srgb();
	CHECK_MESSAGE(
			color_linear.is_equal_approx(Color(0.100481, 0.214041, 0.318547, 0.7)),
			"The color converted to linear color space should match the expected value.");
	CHECK_MESSAGE(
			color_srgb.is_equal_approx(Color(0.62621, 0.735357, 0.797738, 0.7)),
			"The color converted to sRGB color space should match the expected value.");
	CHECK_MESSAGE(
			color_linear.linear_to_srgb().is_equal_approx(Color(0.35, 0.5, 0.6, 0.7)),
			"The linear color converted back to sRGB color space should match the expected value.");
	CHECK_MESSAGE(
			color_srgb.srgb_to_linear().is_equal_approx(Color(0.35, 0.5, 0.6, 0.7)),
			"The sRGB color converted back to linear color space should match the expected value.");
	CHECK_MESSAGE(
			Color(1.0, 1.0, 1.0, 1.0).srgb_to_linear() == (Color(1.0, 1.0, 1.0, 1.0)),
			"White converted from sRGB to linear should remain white.");
	CHECK_MESSAGE(
			Color(1.0, 1.0, 1.0, 1.0).linear_to_srgb() == (Color(1.0, 1.0, 1.0, 1.0)),
			"White converted from linear to sRGB should remain white.");
}

TEST_CASE("[Color] Named colors") {
	CHECK_MESSAGE(
			Color::named("red").is_equal_approx(Color::hex(0xFF0000FF)),
			"The named color \"red\" should match the expected value.");

	// Named colors have their names automatically normalized.
	CHECK_MESSAGE(
			Color::named("white_smoke").is_equal_approx(Color::hex(0xF5F5F5FF)),
			"The named color \"white_smoke\" should match the expected value.");
	CHECK_MESSAGE(
			Color::named("Slate Blue").is_equal_approx(Color::hex(0x6A5ACDFF)),
			"The named color \"Slate Blue\" should match the expected value.");

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			Color::named("doesn't exist").is_equal_approx(Color()),
			"The invalid named color \"doesn't exist\" should result in a Color with the default values.");
	ERR_PRINT_ON;
}

TEST_CASE("[Color] Validation methods") {
	CHECK_MESSAGE(
			Color::html_is_valid("#4080ff"),
			"Valid HTML color (with leading #) should be considered valid.");
	CHECK_MESSAGE(
			Color::html_is_valid("4080ff"),
			"Valid HTML color (without leading #) should be considered valid.");
	CHECK_MESSAGE(
			!Color::html_is_valid("12345"),
			"Invalid HTML color should be considered invalid.");
	CHECK_MESSAGE(
			!Color::html_is_valid("#fuf"),
			"Invalid HTML color should be considered invalid.");
}

TEST_CASE("[Color] Manipulation methods") {
	const Color blue = Color(0, 0, 1, 0.4);

	CHECK_MESSAGE(
			blue.inverted().is_equal_approx(Color(1, 1, 0, 0.4)),
			"Inverted color should have its red, green and blue components inverted.");

	const Color purple = Color(0.5, 0.2, 0.5, 0.25);

	CHECK_MESSAGE(
			purple.lightened(0.2).is_equal_approx(Color(0.6, 0.36, 0.6, 0.25)),
			"Color should be lightened by the expected amount.");
	CHECK_MESSAGE(
			purple.darkened(0.2).is_equal_approx(Color(0.4, 0.16, 0.4, 0.25)),
			"Color should be darkened by the expected amount.");

	const Color red = Color(1, 0, 0, 0.2);
	const Color yellow = Color(1, 1, 0, 0.8);

	CHECK_MESSAGE(
			red.lerp(yellow, 0.5).is_equal_approx(Color(1, 0.5, 0, 0.5)),
			"Red interpolated with yellow should be orange (with interpolated alpha).");
}
} // namespace TestColor
