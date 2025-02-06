/**************************************************************************/
/*  keyboard.h                                                            */
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

#include "core/string/ustring.h"

// Keep the values in this enum in sync with `_keycodes` in `keyboard.cpp`,
// and the bindings in `core_constants.cpp`.
enum class Key {
	NONE = 0,
	// Special key: The strategy here is similar to the one used by toolkits,
	// which consists in leaving the 21 bits unicode range for printable
	// characters, and use the upper 11 bits for special keys and modifiers.
	// This way everything (char/keycode) can fit nicely in one 32-bit
	// integer (the enum's underlying type is `int` by default).
	SPECIAL = (1 << 22),
	/* CURSOR/FUNCTION/BROWSER/MULTIMEDIA/MISC KEYS */
	ESCAPE = SPECIAL | 0x01,
	TAB = SPECIAL | 0x02,
	BACKTAB = SPECIAL | 0x03,
	BACKSPACE = SPECIAL | 0x04,
	ENTER = SPECIAL | 0x05,
	KP_ENTER = SPECIAL | 0x06,
	INSERT = SPECIAL | 0x07,
	KEY_DELETE = SPECIAL | 0x08, // "DELETE" is a reserved word on Windows.
	PAUSE = SPECIAL | 0x09,
	PRINT = SPECIAL | 0x0A,
	SYSREQ = SPECIAL | 0x0B,
	CLEAR = SPECIAL | 0x0C,
	HOME = SPECIAL | 0x0D,
	END = SPECIAL | 0x0E,
	LEFT = SPECIAL | 0x0F,
	UP = SPECIAL | 0x10,
	RIGHT = SPECIAL | 0x11,
	DOWN = SPECIAL | 0x12,
	PAGEUP = SPECIAL | 0x13,
	PAGEDOWN = SPECIAL | 0x14,
	SHIFT = SPECIAL | 0x15,
	CTRL = SPECIAL | 0x16,
	META = SPECIAL | 0x17,
#if defined(MACOS_ENABLED)
	CMD_OR_CTRL = META,
#else
	CMD_OR_CTRL = CTRL,
#endif
	ALT = SPECIAL | 0x18,
	CAPSLOCK = SPECIAL | 0x19,
	NUMLOCK = SPECIAL | 0x1A,
	SCROLLLOCK = SPECIAL | 0x1B,
	F1 = SPECIAL | 0x1C,
	F2 = SPECIAL | 0x1D,
	F3 = SPECIAL | 0x1E,
	F4 = SPECIAL | 0x1F,
	F5 = SPECIAL | 0x20,
	F6 = SPECIAL | 0x21,
	F7 = SPECIAL | 0x22,
	F8 = SPECIAL | 0x23,
	F9 = SPECIAL | 0x24,
	F10 = SPECIAL | 0x25,
	F11 = SPECIAL | 0x26,
	F12 = SPECIAL | 0x27,
	F13 = SPECIAL | 0x28,
	F14 = SPECIAL | 0x29,
	F15 = SPECIAL | 0x2A,
	F16 = SPECIAL | 0x2B,
	F17 = SPECIAL | 0x2C,
	F18 = SPECIAL | 0x2D,
	F19 = SPECIAL | 0x2E,
	F20 = SPECIAL | 0x2F,
	F21 = SPECIAL | 0x30,
	F22 = SPECIAL | 0x31,
	F23 = SPECIAL | 0x32,
	F24 = SPECIAL | 0x33,
	F25 = SPECIAL | 0x34,
	F26 = SPECIAL | 0x35,
	F27 = SPECIAL | 0x36,
	F28 = SPECIAL | 0x37,
	F29 = SPECIAL | 0x38,
	F30 = SPECIAL | 0x39,
	F31 = SPECIAL | 0x3A,
	F32 = SPECIAL | 0x3B,
	F33 = SPECIAL | 0x3C,
	F34 = SPECIAL | 0x3D,
	F35 = SPECIAL | 0x3E,
	KP_MULTIPLY = SPECIAL | 0x81,
	KP_DIVIDE = SPECIAL | 0x82,
	KP_SUBTRACT = SPECIAL | 0x83,
	KP_PERIOD = SPECIAL | 0x84,
	KP_ADD = SPECIAL | 0x85,
	KP_0 = SPECIAL | 0x86,
	KP_1 = SPECIAL | 0x87,
	KP_2 = SPECIAL | 0x88,
	KP_3 = SPECIAL | 0x89,
	KP_4 = SPECIAL | 0x8A,
	KP_5 = SPECIAL | 0x8B,
	KP_6 = SPECIAL | 0x8C,
	KP_7 = SPECIAL | 0x8D,
	KP_8 = SPECIAL | 0x8E,
	KP_9 = SPECIAL | 0x8F,
	MENU = SPECIAL | 0x42,
	HYPER = SPECIAL | 0x43,
	HELP = SPECIAL | 0x45,
	BACK = SPECIAL | 0x48,
	FORWARD = SPECIAL | 0x49,
	STOP = SPECIAL | 0x4A,
	REFRESH = SPECIAL | 0x4B,
	VOLUMEDOWN = SPECIAL | 0x4C,
	VOLUMEMUTE = SPECIAL | 0x4D,
	VOLUMEUP = SPECIAL | 0x4E,
	MEDIAPLAY = SPECIAL | 0x54,
	MEDIASTOP = SPECIAL | 0x55,
	MEDIAPREVIOUS = SPECIAL | 0x56,
	MEDIANEXT = SPECIAL | 0x57,
	MEDIARECORD = SPECIAL | 0x58,
	HOMEPAGE = SPECIAL | 0x59,
	FAVORITES = SPECIAL | 0x5A,
	SEARCH = SPECIAL | 0x5B,
	STANDBY = SPECIAL | 0x5C,
	OPENURL = SPECIAL | 0x5D,
	LAUNCHMAIL = SPECIAL | 0x5E,
	LAUNCHMEDIA = SPECIAL | 0x5F,
	LAUNCH0 = SPECIAL | 0x60,
	LAUNCH1 = SPECIAL | 0x61,
	LAUNCH2 = SPECIAL | 0x62,
	LAUNCH3 = SPECIAL | 0x63,
	LAUNCH4 = SPECIAL | 0x64,
	LAUNCH5 = SPECIAL | 0x65,
	LAUNCH6 = SPECIAL | 0x66,
	LAUNCH7 = SPECIAL | 0x67,
	LAUNCH8 = SPECIAL | 0x68,
	LAUNCH9 = SPECIAL | 0x69,
	LAUNCHA = SPECIAL | 0x6A,
	LAUNCHB = SPECIAL | 0x6B,
	LAUNCHC = SPECIAL | 0x6C,
	LAUNCHD = SPECIAL | 0x6D,
	LAUNCHE = SPECIAL | 0x6E,
	LAUNCHF = SPECIAL | 0x6F,

	GLOBE = SPECIAL | 0x70,
	KEYBOARD = SPECIAL | 0x71,
	JIS_EISU = SPECIAL | 0x72,
	JIS_KANA = SPECIAL | 0x73,

	UNKNOWN = SPECIAL | 0x7FFFFF,

	/* PRINTABLE LATIN 1 CODES */

	SPACE = 0x0020,
	EXCLAM = 0x0021,
	QUOTEDBL = 0x0022,
	NUMBERSIGN = 0x0023,
	DOLLAR = 0x0024,
	PERCENT = 0x0025,
	AMPERSAND = 0x0026,
	APOSTROPHE = 0x0027,
	PARENLEFT = 0x0028,
	PARENRIGHT = 0x0029,
	ASTERISK = 0x002A,
	PLUS = 0x002B,
	COMMA = 0x002C,
	MINUS = 0x002D,
	PERIOD = 0x002E,
	SLASH = 0x002F,
	KEY_0 = 0x0030,
	KEY_1 = 0x0031,
	KEY_2 = 0x0032,
	KEY_3 = 0x0033,
	KEY_4 = 0x0034,
	KEY_5 = 0x0035,
	KEY_6 = 0x0036,
	KEY_7 = 0x0037,
	KEY_8 = 0x0038,
	KEY_9 = 0x0039,
	COLON = 0x003A,
	SEMICOLON = 0x003B,
	LESS = 0x003C,
	EQUAL = 0x003D,
	GREATER = 0x003E,
	QUESTION = 0x003F,
	AT = 0x0040,
	A = 0x0041,
	B = 0x0042,
	C = 0x0043,
	D = 0x0044,
	E = 0x0045,
	F = 0x0046,
	G = 0x0047,
	H = 0x0048,
	I = 0x0049,
	J = 0x004A,
	K = 0x004B,
	L = 0x004C,
	M = 0x004D,
	N = 0x004E,
	O = 0x004F,
	P = 0x0050,
	Q = 0x0051,
	R = 0x0052,
	S = 0x0053,
	T = 0x0054,
	U = 0x0055,
	V = 0x0056,
	W = 0x0057,
	X = 0x0058,
	Y = 0x0059,
	Z = 0x005A,
	BRACKETLEFT = 0x005B,
	BACKSLASH = 0x005C,
	BRACKETRIGHT = 0x005D,
	ASCIICIRCUM = 0x005E,
	UNDERSCORE = 0x005F,
	QUOTELEFT = 0x0060,
	BRACELEFT = 0x007B,
	BAR = 0x007C,
	BRACERIGHT = 0x007D,
	ASCIITILDE = 0x007E,
	YEN = 0x00A5,
	SECTION = 0x00A7,
};

enum class KeyModifierMask {
	CODE_MASK = ((1 << 23) - 1), ///< Apply this mask to any keycode to remove modifiers.
	MODIFIER_MASK = (0x7F << 24), ///< Apply this mask to isolate modifiers.
	//RESERVED = (1 << 23),
	CMD_OR_CTRL = (1 << 24),
	SHIFT = (1 << 25),
	ALT = (1 << 26),
	META = (1 << 27),
	CTRL = (1 << 28),
	KPAD = (1 << 29),
	GROUP_SWITCH = (1 << 30)
};

enum class KeyLocation {
	UNSPECIFIED,
	LEFT,
	RIGHT
};

// To avoid having unnecessary operators, only define the ones that are needed.

constexpr Key operator-(uint32_t a, Key b) {
	return (Key)(a - (uint32_t)b);
}

constexpr Key &operator-=(Key &a, int b) {
	a = static_cast<Key>(static_cast<int>(a) - static_cast<int>(b));
	return a;
}

constexpr Key operator+(Key a, int b) {
	return (Key)((int)a + (int)b);
}

constexpr Key operator+(Key a, Key b) {
	return (Key)((int)a + (int)b);
}

constexpr Key operator-(Key a, Key b) {
	return (Key)((int)a - (int)b);
}

constexpr Key operator&(Key a, Key b) {
	return (Key)((int)a & (int)b);
}

constexpr Key operator|(Key a, Key b) {
	return (Key)((int)a | (int)b);
}

constexpr Key &operator|=(Key &a, Key b) {
	a = static_cast<Key>(static_cast<int>(a) | static_cast<int>(b));
	return a;
}

constexpr Key &operator|=(Key &a, KeyModifierMask b) {
	a = static_cast<Key>(static_cast<int>(a) | static_cast<int>(b));
	return a;
}

constexpr Key &operator&=(Key &a, KeyModifierMask b) {
	a = static_cast<Key>(static_cast<int>(a) & static_cast<int>(b));
	return a;
}

constexpr Key operator|(Key a, KeyModifierMask b) {
	return (Key)((int)a | (int)b);
}

constexpr Key operator&(Key a, KeyModifierMask b) {
	return (Key)((int)a & (int)b);
}

constexpr Key operator+(KeyModifierMask a, Key b) {
	return (Key)((int)a + (int)b);
}

constexpr Key operator|(KeyModifierMask a, Key b) {
	return (Key)((int)a | (int)b);
}

constexpr KeyModifierMask operator+(KeyModifierMask a, KeyModifierMask b) {
	return (KeyModifierMask)((int)a + (int)b);
}

constexpr KeyModifierMask operator|(KeyModifierMask a, KeyModifierMask b) {
	return (KeyModifierMask)((int)a | (int)b);
}

String keycode_get_string(Key p_code);
bool keycode_has_unicode(Key p_keycode);
Key find_keycode(const String &p_codestr);
const char *find_keycode_name(Key p_keycode);
int keycode_get_count();
int keycode_get_value_by_index(int p_index);
const char *keycode_get_name_by_index(int p_index);

char32_t fix_unicode(char32_t p_char);
Key fix_keycode(char32_t p_char, Key p_key);
Key fix_key_label(char32_t p_char, Key p_key);
