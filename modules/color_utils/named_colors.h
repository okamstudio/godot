/**************************************************************************/
/*  named_colors.h                                                        */
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

#ifndef NAMED_COLORS_H
#define NAMED_COLORS_H

#include "color_utils.h"

#include "core/math/color.h"
#include "core/string/ustring.h"

// Names from https://en.wikipedia.org/wiki/X11_color_names

struct NamedColor {
	String name;
	Color color;
};

// NOTE: This data is duplicated in the file:
// modules/mono/glue/GodotSharp/GodotSharp/Core/Colors.cs

static NamedColor named_colors[] = {
	{ String("ALICE_BLUE"), ColorUtils::from_rgba32(0xF0F8FFFF) },
	{ String("ANTIQUE_WHITE"), ColorUtils::from_rgba32(0xFAEBD7FF) },
	{ String("AQUA"), ColorUtils::from_rgba32(0x00FFFFFF) },
	{ String("AQUAMARINE"), ColorUtils::from_rgba32(0x7FFFD4FF) },
	{ String("AZURE"), ColorUtils::from_rgba32(0xF0FFFFFF) },
	{ String("BEIGE"), ColorUtils::from_rgba32(0xF5F5DCFF) },
	{ String("BISQUE"), ColorUtils::from_rgba32(0xFFE4C4FF) },
	{ String("BLACK"), ColorUtils::from_rgba32(0x000000FF) },
	{ String("BLANCHED_ALMOND"), ColorUtils::from_rgba32(0xFFEBCDFF) },
	{ String("BLUE"), ColorUtils::from_rgba32(0x0000FFFF) },
	{ String("BLUE_VIOLET"), ColorUtils::from_rgba32(0x8A2BE2FF) },
	{ String("BROWN"), ColorUtils::from_rgba32(0xA52A2AFF) },
	{ String("BURLYWOOD"), ColorUtils::from_rgba32(0xDEB887FF) },
	{ String("CADET_BLUE"), ColorUtils::from_rgba32(0x5F9EA0FF) },
	{ String("CHARTREUSE"), ColorUtils::from_rgba32(0x7FFF00FF) },
	{ String("CHOCOLATE"), ColorUtils::from_rgba32(0xD2691EFF) },
	{ String("CORAL"), ColorUtils::from_rgba32(0xFF7F50FF) },
	{ String("CORNFLOWER_BLUE"), ColorUtils::from_rgba32(0x6495EDFF) },
	{ String("CORNSILK"), ColorUtils::from_rgba32(0xFFF8DCFF) },
	{ String("CRIMSON"), ColorUtils::from_rgba32(0xDC143CFF) },
	{ String("CYAN"), ColorUtils::from_rgba32(0x00FFFFFF) },
	{ String("DARK_BLUE"), ColorUtils::from_rgba32(0x00008BFF) },
	{ String("DARK_CYAN"), ColorUtils::from_rgba32(0x008B8BFF) },
	{ String("DARK_GOLDENROD"), ColorUtils::from_rgba32(0xB8860BFF) },
	{ String("DARK_GRAY"), ColorUtils::from_rgba32(0xA9A9A9FF) },
	{ String("DARK_GREEN"), ColorUtils::from_rgba32(0x006400FF) },
	{ String("DARK_KHAKI"), ColorUtils::from_rgba32(0xBDB76BFF) },
	{ String("DARK_MAGENTA"), ColorUtils::from_rgba32(0x8B008BFF) },
	{ String("DARK_OLIVE_GREEN"), ColorUtils::from_rgba32(0x556B2FFF) },
	{ String("DARK_ORANGE"), ColorUtils::from_rgba32(0xFF8C00FF) },
	{ String("DARK_ORCHID"), ColorUtils::from_rgba32(0x9932CCFF) },
	{ String("DARK_RED"), ColorUtils::from_rgba32(0x8B0000FF) },
	{ String("DARK_SALMON"), ColorUtils::from_rgba32(0xE9967AFF) },
	{ String("DARK_SEA_GREEN"), ColorUtils::from_rgba32(0x8FBC8FFF) },
	{ String("DARK_SLATE_BLUE"), ColorUtils::from_rgba32(0x483D8BFF) },
	{ String("DARK_SLATE_GRAY"), ColorUtils::from_rgba32(0x2F4F4FFF) },
	{ String("DARK_TURQUOISE"), ColorUtils::from_rgba32(0x00CED1FF) },
	{ String("DARK_VIOLET"), ColorUtils::from_rgba32(0x9400D3FF) },
	{ String("DEEP_PINK"), ColorUtils::from_rgba32(0xFF1493FF) },
	{ String("DEEP_SKY_BLUE"), ColorUtils::from_rgba32(0x00BFFFFF) },
	{ String("DIM_GRAY"), ColorUtils::from_rgba32(0x696969FF) },
	{ String("DODGER_BLUE"), ColorUtils::from_rgba32(0x1E90FFFF) },
	{ String("FIREBRICK"), ColorUtils::from_rgba32(0xB22222FF) },
	{ String("FLORAL_WHITE"), ColorUtils::from_rgba32(0xFFFAF0FF) },
	{ String("FOREST_GREEN"), ColorUtils::from_rgba32(0x228B22FF) },
	{ String("FUCHSIA"), ColorUtils::from_rgba32(0xFF00FFFF) },
	{ String("GAINSBORO"), ColorUtils::from_rgba32(0xDCDCDCFF) },
	{ String("GHOST_WHITE"), ColorUtils::from_rgba32(0xF8F8FFFF) },
	{ String("GOLD"), ColorUtils::from_rgba32(0xFFD700FF) },
	{ String("GOLDENROD"), ColorUtils::from_rgba32(0xDAA520FF) },
	{ String("GRAY"), ColorUtils::from_rgba32(0xBEBEBEFF) },
	{ String("GREEN"), ColorUtils::from_rgba32(0x00FF00FF) },
	{ String("GREEN_YELLOW"), ColorUtils::from_rgba32(0xADFF2FFF) },
	{ String("HONEYDEW"), ColorUtils::from_rgba32(0xF0FFF0FF) },
	{ String("HOT_PINK"), ColorUtils::from_rgba32(0xFF69B4FF) },
	{ String("INDIAN_RED"), ColorUtils::from_rgba32(0xCD5C5CFF) },
	{ String("INDIGO"), ColorUtils::from_rgba32(0x4B0082FF) },
	{ String("IVORY"), ColorUtils::from_rgba32(0xFFFFF0FF) },
	{ String("KHAKI"), ColorUtils::from_rgba32(0xF0E68CFF) },
	{ String("LAVENDER"), ColorUtils::from_rgba32(0xE6E6FAFF) },
	{ String("LAVENDER_BLUSH"), ColorUtils::from_rgba32(0xFFF0F5FF) },
	{ String("LAWN_GREEN"), ColorUtils::from_rgba32(0x7CFC00FF) },
	{ String("LEMON_CHIFFON"), ColorUtils::from_rgba32(0xFFFACDFF) },
	{ String("LIGHT_BLUE"), ColorUtils::from_rgba32(0xADD8E6FF) },
	{ String("LIGHT_CORAL"), ColorUtils::from_rgba32(0xF08080FF) },
	{ String("LIGHT_CYAN"), ColorUtils::from_rgba32(0xE0FFFFFF) },
	{ String("LIGHT_GOLDENROD"), ColorUtils::from_rgba32(0xFAFAD2FF) },
	{ String("LIGHT_GRAY"), ColorUtils::from_rgba32(0xD3D3D3FF) },
	{ String("LIGHT_GREEN"), ColorUtils::from_rgba32(0x90EE90FF) },
	{ String("LIGHT_PINK"), ColorUtils::from_rgba32(0xFFB6C1FF) },
	{ String("LIGHT_SALMON"), ColorUtils::from_rgba32(0xFFA07AFF) },
	{ String("LIGHT_SEA_GREEN"), ColorUtils::from_rgba32(0x20B2AAFF) },
	{ String("LIGHT_SKY_BLUE"), ColorUtils::from_rgba32(0x87CEFAFF) },
	{ String("LIGHT_SLATE_GRAY"), ColorUtils::from_rgba32(0x778899FF) },
	{ String("LIGHT_STEEL_BLUE"), ColorUtils::from_rgba32(0xB0C4DEFF) },
	{ String("LIGHT_YELLOW"), ColorUtils::from_rgba32(0xFFFFE0FF) },
	{ String("LIME"), ColorUtils::from_rgba32(0x00FF00FF) },
	{ String("LIME_GREEN"), ColorUtils::from_rgba32(0x32CD32FF) },
	{ String("LINEN"), ColorUtils::from_rgba32(0xFAF0E6FF) },
	{ String("MAGENTA"), ColorUtils::from_rgba32(0xFF00FFFF) },
	{ String("MAROON"), ColorUtils::from_rgba32(0xB03060FF) },
	{ String("MEDIUM_AQUAMARINE"), ColorUtils::from_rgba32(0x66CDAAFF) },
	{ String("MEDIUM_BLUE"), ColorUtils::from_rgba32(0x0000CDFF) },
	{ String("MEDIUM_ORCHID"), ColorUtils::from_rgba32(0xBA55D3FF) },
	{ String("MEDIUM_PURPLE"), ColorUtils::from_rgba32(0x9370DBFF) },
	{ String("MEDIUM_SEA_GREEN"), ColorUtils::from_rgba32(0x3CB371FF) },
	{ String("MEDIUM_SLATE_BLUE"), ColorUtils::from_rgba32(0x7B68EEFF) },
	{ String("MEDIUM_SPRING_GREEN"), ColorUtils::from_rgba32(0x00FA9AFF) },
	{ String("MEDIUM_TURQUOISE"), ColorUtils::from_rgba32(0x48D1CCFF) },
	{ String("MEDIUM_VIOLET_RED"), ColorUtils::from_rgba32(0xC71585FF) },
	{ String("MIDNIGHT_BLUE"), ColorUtils::from_rgba32(0x191970FF) },
	{ String("MINT_CREAM"), ColorUtils::from_rgba32(0xF5FFFAFF) },
	{ String("MISTY_ROSE"), ColorUtils::from_rgba32(0xFFE4E1FF) },
	{ String("MOCCASIN"), ColorUtils::from_rgba32(0xFFE4B5FF) },
	{ String("NAVAJO_WHITE"), ColorUtils::from_rgba32(0xFFDEADFF) },
	{ String("NAVY_BLUE"), ColorUtils::from_rgba32(0x000080FF) },
	{ String("OLD_LACE"), ColorUtils::from_rgba32(0xFDF5E6FF) },
	{ String("OLIVE"), ColorUtils::from_rgba32(0x808000FF) },
	{ String("OLIVE_DRAB"), ColorUtils::from_rgba32(0x6B8E23FF) },
	{ String("ORANGE"), ColorUtils::from_rgba32(0xFFA500FF) },
	{ String("ORANGE_RED"), ColorUtils::from_rgba32(0xFF4500FF) },
	{ String("ORCHID"), ColorUtils::from_rgba32(0xDA70D6FF) },
	{ String("PALE_GOLDENROD"), ColorUtils::from_rgba32(0xEEE8AAFF) },
	{ String("PALE_GREEN"), ColorUtils::from_rgba32(0x98FB98FF) },
	{ String("PALE_TURQUOISE"), ColorUtils::from_rgba32(0xAFEEEEFF) },
	{ String("PALE_VIOLET_RED"), ColorUtils::from_rgba32(0xDB7093FF) },
	{ String("PAPAYA_WHIP"), ColorUtils::from_rgba32(0xFFEFD5FF) },
	{ String("PEACH_PUFF"), ColorUtils::from_rgba32(0xFFDAB9FF) },
	{ String("PERU"), ColorUtils::from_rgba32(0xCD853FFF) },
	{ String("PINK"), ColorUtils::from_rgba32(0xFFC0CBFF) },
	{ String("PLUM"), ColorUtils::from_rgba32(0xDDA0DDFF) },
	{ String("POWDER_BLUE"), ColorUtils::from_rgba32(0xB0E0E6FF) },
	{ String("PURPLE"), ColorUtils::from_rgba32(0xA020F0FF) },
	{ String("REBECCA_PURPLE"), ColorUtils::from_rgba32(0x663399FF) },
	{ String("RED"), ColorUtils::from_rgba32(0xFF0000FF) },
	{ String("ROSY_BROWN"), ColorUtils::from_rgba32(0xBC8F8FFF) },
	{ String("ROYAL_BLUE"), ColorUtils::from_rgba32(0x4169E1FF) },
	{ String("SADDLE_BROWN"), ColorUtils::from_rgba32(0x8B4513FF) },
	{ String("SALMON"), ColorUtils::from_rgba32(0xFA8072FF) },
	{ String("SANDY_BROWN"), ColorUtils::from_rgba32(0xF4A460FF) },
	{ String("SEA_GREEN"), ColorUtils::from_rgba32(0x2E8B57FF) },
	{ String("SEASHELL"), ColorUtils::from_rgba32(0xFFF5EEFF) },
	{ String("SIENNA"), ColorUtils::from_rgba32(0xA0522DFF) },
	{ String("SILVER"), ColorUtils::from_rgba32(0xC0C0C0FF) },
	{ String("SKY_BLUE"), ColorUtils::from_rgba32(0x87CEEBFF) },
	{ String("SLATE_BLUE"), ColorUtils::from_rgba32(0x6A5ACDFF) },
	{ String("SLATE_GRAY"), ColorUtils::from_rgba32(0x708090FF) },
	{ String("SNOW"), ColorUtils::from_rgba32(0xFFFAFAFF) },
	{ String("SPRING_GREEN"), ColorUtils::from_rgba32(0x00FF7FFF) },
	{ String("STEEL_BLUE"), ColorUtils::from_rgba32(0x4682B4FF) },
	{ String("TAN"), ColorUtils::from_rgba32(0xD2B48CFF) },
	{ String("TEAL"), ColorUtils::from_rgba32(0x008080FF) },
	{ String("THISTLE"), ColorUtils::from_rgba32(0xD8BFD8FF) },
	{ String("TOMATO"), ColorUtils::from_rgba32(0xFF6347FF) },
	{ String("TRANSPARENT"), ColorUtils::from_rgba32(0xFFFFFF00) },
	{ String("TURQUOISE"), ColorUtils::from_rgba32(0x40E0D0FF) },
	{ String("VIOLET"), ColorUtils::from_rgba32(0xEE82EEFF) },
	{ String("WEB_GRAY"), ColorUtils::from_rgba32(0x808080FF) },
	{ String("WEB_GREEN"), ColorUtils::from_rgba32(0x008000FF) },
	{ String("WEB_MAROON"), ColorUtils::from_rgba32(0x800000FF) },
	{ String("WEB_PURPLE"), ColorUtils::from_rgba32(0x800080FF) },
	{ String("WHEAT"), ColorUtils::from_rgba32(0xF5DEB3FF) },
	{ String("WHITE"), ColorUtils::from_rgba32(0xFFFFFFFF) },
	{ String("WHITE_SMOKE"), ColorUtils::from_rgba32(0xF5F5F5FF) },
	{ String("YELLOW"), ColorUtils::from_rgba32(0xFFFF00FF) },
	{ String("YELLOW_GREEN"), ColorUtils::from_rgba32(0x9ACD32FF) },
};

#endif // NAMED_COLORS_H
