/**************************************************************************/
/*  color_names.inc                                                       */
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

// Names from https://en.wikipedia.org/wiki/X11_color_names

// So this in a way that does not require memory allocation
// the old way leaked memory
// this is not used as often as for more performance to make sense

struct NamedColor {
	const char *name;
	Color color;
};

// NOTE: This data is duplicated in the file:
// modules/mono/glue/GodotSharp/GodotSharp/Core/Colors.cs

static NamedColor named_colors[] = {
	{ "ALICE_BLUE", Color::hex(0xF0F8FFFF) },
	{ "ANTIQUE_WHITE", Color::hex(0xFAEBD7FF) },
	{ "AQUA", Color::hex(0x00FFFFFF) },
	{ "AQUAMARINE", Color::hex(0x7FFFD4FF) },
	{ "AZURE", Color::hex(0xF0FFFFFF) },
	{ "BEIGE", Color::hex(0xF5F5DCFF) },
	{ "BISQUE", Color::hex(0xFFE4C4FF) },
	{ "BLACK", Color::hex(0x000000FF) },
	{ "BLANCHED_ALMOND", Color::hex(0xFFEBCDFF) },
	{ "BLUE", Color::hex(0x0000FFFF) },
	{ "BLUE_VIOLET", Color::hex(0x8A2BE2FF) },
	{ "BROWN", Color::hex(0xA52A2AFF) },
	{ "BURLYWOOD", Color::hex(0xDEB887FF) },
	{ "CADET_BLUE", Color::hex(0x5F9EA0FF) },
	{ "CHARTREUSE", Color::hex(0x7FFF00FF) },
	{ "CHOCOLATE", Color::hex(0xD2691EFF) },
	{ "CORAL", Color::hex(0xFF7F50FF) },
	{ "CORNFLOWER_BLUE", Color::hex(0x6495EDFF) },
	{ "CORNSILK", Color::hex(0xFFF8DCFF) },
	{ "CRIMSON", Color::hex(0xDC143CFF) },
	{ "CYAN", Color::hex(0x00FFFFFF) },
	{ "DARK_BLUE", Color::hex(0x00008BFF) },
	{ "DARK_CYAN", Color::hex(0x008B8BFF) },
	{ "DARK_GOLDENROD", Color::hex(0xB8860BFF) },
	{ "DARK_GRAY", Color::hex(0xA9A9A9FF) },
	{ "DARK_GREEN", Color::hex(0x006400FF) },
	{ "DARK_KHAKI", Color::hex(0xBDB76BFF) },
	{ "DARK_MAGENTA", Color::hex(0x8B008BFF) },
	{ "DARK_OLIVE_GREEN", Color::hex(0x556B2FFF) },
	{ "DARK_ORANGE", Color::hex(0xFF8C00FF) },
	{ "DARK_ORCHID", Color::hex(0x9932CCFF) },
	{ "DARK_RED", Color::hex(0x8B0000FF) },
	{ "DARK_SALMON", Color::hex(0xE9967AFF) },
	{ "DARK_SEA_GREEN", Color::hex(0x8FBC8FFF) },
	{ "DARK_SLATE_BLUE", Color::hex(0x483D8BFF) },
	{ "DARK_SLATE_GRAY", Color::hex(0x2F4F4FFF) },
	{ "DARK_TURQUOISE", Color::hex(0x00CED1FF) },
	{ "DARK_VIOLET", Color::hex(0x9400D3FF) },
	{ "DEEP_PINK", Color::hex(0xFF1493FF) },
	{ "DEEP_SKY_BLUE", Color::hex(0x00BFFFFF) },
	{ "DIM_GRAY", Color::hex(0x696969FF) },
	{ "DODGER_BLUE", Color::hex(0x1E90FFFF) },
	{ "FIREBRICK", Color::hex(0xB22222FF) },
	{ "FLORAL_WHITE", Color::hex(0xFFFAF0FF) },
	{ "FOREST_GREEN", Color::hex(0x228B22FF) },
	{ "FUCHSIA", Color::hex(0xFF00FFFF) },
	{ "GAINSBORO", Color::hex(0xDCDCDCFF) },
	{ "GHOST_WHITE", Color::hex(0xF8F8FFFF) },
	{ "GOLD", Color::hex(0xFFD700FF) },
	{ "GOLDENROD", Color::hex(0xDAA520FF) },
	{ "GRAY", Color::hex(0xBEBEBEFF) },
	{ "GREEN", Color::hex(0x00FF00FF) },
	{ "GREEN_YELLOW", Color::hex(0xADFF2FFF) },
	{ "HONEYDEW", Color::hex(0xF0FFF0FF) },
	{ "HOT_PINK", Color::hex(0xFF69B4FF) },
	{ "INDIAN_RED", Color::hex(0xCD5C5CFF) },
	{ "INDIGO", Color::hex(0x4B0082FF) },
	{ "IVORY", Color::hex(0xFFFFF0FF) },
	{ "KHAKI", Color::hex(0xF0E68CFF) },
	{ "LAVENDER", Color::hex(0xE6E6FAFF) },
	{ "LAVENDER_BLUSH", Color::hex(0xFFF0F5FF) },
	{ "LAWN_GREEN", Color::hex(0x7CFC00FF) },
	{ "LEMON_CHIFFON", Color::hex(0xFFFACDFF) },
	{ "LIGHT_BLUE", Color::hex(0xADD8E6FF) },
	{ "LIGHT_CORAL", Color::hex(0xF08080FF) },
	{ "LIGHT_CYAN", Color::hex(0xE0FFFFFF) },
	{ "LIGHT_GOLDENROD", Color::hex(0xFAFAD2FF) },
	{ "LIGHT_GRAY", Color::hex(0xD3D3D3FF) },
	{ "LIGHT_GREEN", Color::hex(0x90EE90FF) },
	{ "LIGHT_PINK", Color::hex(0xFFB6C1FF) },
	{ "LIGHT_SALMON", Color::hex(0xFFA07AFF) },
	{ "LIGHT_SEA_GREEN", Color::hex(0x20B2AAFF) },
	{ "LIGHT_SKY_BLUE", Color::hex(0x87CEFAFF) },
	{ "LIGHT_SLATE_GRAY", Color::hex(0x778899FF) },
	{ "LIGHT_STEEL_BLUE", Color::hex(0xB0C4DEFF) },
	{ "LIGHT_YELLOW", Color::hex(0xFFFFE0FF) },
	{ "LIME", Color::hex(0x00FF00FF) },
	{ "LIME_GREEN", Color::hex(0x32CD32FF) },
	{ "LINEN", Color::hex(0xFAF0E6FF) },
	{ "MAGENTA", Color::hex(0xFF00FFFF) },
	{ "MAROON", Color::hex(0xB03060FF) },
	{ "MEDIUM_AQUAMARINE", Color::hex(0x66CDAAFF) },
	{ "MEDIUM_BLUE", Color::hex(0x0000CDFF) },
	{ "MEDIUM_ORCHID", Color::hex(0xBA55D3FF) },
	{ "MEDIUM_PURPLE", Color::hex(0x9370DBFF) },
	{ "MEDIUM_SEA_GREEN", Color::hex(0x3CB371FF) },
	{ "MEDIUM_SLATE_BLUE", Color::hex(0x7B68EEFF) },
	{ "MEDIUM_SPRING_GREEN", Color::hex(0x00FA9AFF) },
	{ "MEDIUM_TURQUOISE", Color::hex(0x48D1CCFF) },
	{ "MEDIUM_VIOLET_RED", Color::hex(0xC71585FF) },
	{ "MIDNIGHT_BLUE", Color::hex(0x191970FF) },
	{ "MINT_CREAM", Color::hex(0xF5FFFAFF) },
	{ "MISTY_ROSE", Color::hex(0xFFE4E1FF) },
	{ "MOCCASIN", Color::hex(0xFFE4B5FF) },
	{ "NAVAJO_WHITE", Color::hex(0xFFDEADFF) },
	{ "NAVY_BLUE", Color::hex(0x000080FF) },
	{ "OLD_LACE", Color::hex(0xFDF5E6FF) },
	{ "OLIVE", Color::hex(0x808000FF) },
	{ "OLIVE_DRAB", Color::hex(0x6B8E23FF) },
	{ "ORANGE", Color::hex(0xFFA500FF) },
	{ "ORANGE_RED", Color::hex(0xFF4500FF) },
	{ "ORCHID", Color::hex(0xDA70D6FF) },
	{ "PALE_GOLDENROD", Color::hex(0xEEE8AAFF) },
	{ "PALE_GREEN", Color::hex(0x98FB98FF) },
	{ "PALE_TURQUOISE", Color::hex(0xAFEEEEFF) },
	{ "PALE_VIOLET_RED", Color::hex(0xDB7093FF) },
	{ "PAPAYA_WHIP", Color::hex(0xFFEFD5FF) },
	{ "PEACH_PUFF", Color::hex(0xFFDAB9FF) },
	{ "PERU", Color::hex(0xCD853FFF) },
	{ "PINK", Color::hex(0xFFC0CBFF) },
	{ "PLUM", Color::hex(0xDDA0DDFF) },
	{ "POWDER_BLUE", Color::hex(0xB0E0E6FF) },
	{ "PURPLE", Color::hex(0xA020F0FF) },
	{ "REBECCA_PURPLE", Color::hex(0x663399FF) },
	{ "RED", Color::hex(0xFF0000FF) },
	{ "ROSY_BROWN", Color::hex(0xBC8F8FFF) },
	{ "ROYAL_BLUE", Color::hex(0x4169E1FF) },
	{ "SADDLE_BROWN", Color::hex(0x8B4513FF) },
	{ "SALMON", Color::hex(0xFA8072FF) },
	{ "SANDY_BROWN", Color::hex(0xF4A460FF) },
	{ "SEA_GREEN", Color::hex(0x2E8B57FF) },
	{ "SEASHELL", Color::hex(0xFFF5EEFF) },
	{ "SIENNA", Color::hex(0xA0522DFF) },
	{ "SILVER", Color::hex(0xC0C0C0FF) },
	{ "SKY_BLUE", Color::hex(0x87CEEBFF) },
	{ "SLATE_BLUE", Color::hex(0x6A5ACDFF) },
	{ "SLATE_GRAY", Color::hex(0x708090FF) },
	{ "SNOW", Color::hex(0xFFFAFAFF) },
	{ "SPRING_GREEN", Color::hex(0x00FF7FFF) },
	{ "STEEL_BLUE", Color::hex(0x4682B4FF) },
	{ "TAN", Color::hex(0xD2B48CFF) },
	{ "TEAL", Color::hex(0x008080FF) },
	{ "THISTLE", Color::hex(0xD8BFD8FF) },
	{ "TOMATO", Color::hex(0xFF6347FF) },
	{ "TRANSPARENT", Color::hex(0xFFFFFF00) },
	{ "TURQUOISE", Color::hex(0x40E0D0FF) },
	{ "VIOLET", Color::hex(0xEE82EEFF) },
	{ "WEB_GRAY", Color::hex(0x808080FF) },
	{ "WEB_GREEN", Color::hex(0x008000FF) },
	{ "WEB_MAROON", Color::hex(0x800000FF) },
	{ "WEB_PURPLE", Color::hex(0x800080FF) },
	{ "WHEAT", Color::hex(0xF5DEB3FF) },
	{ "WHITE", Color::hex(0xFFFFFFFF) },
	{ "WHITE_SMOKE", Color::hex(0xF5F5F5FF) },
	{ "YELLOW", Color::hex(0xFFFF00FF) },
	{ "YELLOW_GREEN", Color::hex(0x9ACD32FF) },
};
