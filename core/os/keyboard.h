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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "core/ustring.h"

/*
	Special Key:

	The strategy here is similar to the one used by toolkits,
	which consists in leaving the 24 bits unicode range for printable
	characters, and use the upper 8 bits for special keys and
	modifiers. This way everything (char/keycode) can fit nicely in one 32 bits unsigned integer.
*/
enum {
	SPKEY = (1 << 24)
};

enum KeyList {
	/* CURSOR/FUNCTION/BROWSER/MULTIMEDIA/MISC KEYS */
	KEY_ESCAPE = SPKEY | 0x01,
	KEY_TAB = SPKEY | 0x02,
	KEY_BACKTAB = SPKEY | 0x03,
	KEY_BACKSPACE = SPKEY | 0x04,
	KEY_ENTER = SPKEY | 0x05,
	KEY_KP_ENTER = SPKEY | 0x06,
	KEY_INSERT = SPKEY | 0x07,
	KEY_DELETE = SPKEY | 0x08,
	KEY_PAUSE = SPKEY | 0x09,
	KEY_PRINT = SPKEY | 0x0A,
	KEY_SYSREQ = SPKEY | 0x0B,
	KEY_CLEAR = SPKEY | 0x0C,
	KEY_HOME = SPKEY | 0x0D,
	KEY_END = SPKEY | 0x0E,
	KEY_LEFT = SPKEY | 0x0F,
	KEY_UP = SPKEY | 0x10,
	KEY_RIGHT = SPKEY | 0x11,
	KEY_DOWN = SPKEY | 0x12,
	KEY_PAGEUP = SPKEY | 0x13,
	KEY_PAGEDOWN = SPKEY | 0x14,
	KEY_SHIFT = SPKEY | 0x15,
	KEY_CONTROL = SPKEY | 0x16,
	KEY_META = SPKEY | 0x17,
	KEY_ALT = SPKEY | 0x18,
	KEY_CAPSLOCK = SPKEY | 0x19,
	KEY_NUMLOCK = SPKEY | 0x1A,
	KEY_SCROLLLOCK = SPKEY | 0x1B,
	KEY_F1 = SPKEY | 0x1C,
	KEY_F2 = SPKEY | 0x1D,
	KEY_F3 = SPKEY | 0x1E,
	KEY_F4 = SPKEY | 0x1F,
	KEY_F5 = SPKEY | 0x20,
	KEY_F6 = SPKEY | 0x21,
	KEY_F7 = SPKEY | 0x22,
	KEY_F8 = SPKEY | 0x23,
	KEY_F9 = SPKEY | 0x24,
	KEY_F10 = SPKEY | 0x25,
	KEY_F11 = SPKEY | 0x26,
	KEY_F12 = SPKEY | 0x27,
	KEY_F13 = SPKEY | 0x28,
	KEY_F14 = SPKEY | 0x29,
	KEY_F15 = SPKEY | 0x2A,
	KEY_F16 = SPKEY | 0x2B,
	KEY_KP_MULTIPLY = SPKEY | 0x81,
	KEY_KP_DIVIDE = SPKEY | 0x82,
	KEY_KP_SUBTRACT = SPKEY | 0x83,
	KEY_KP_PERIOD = SPKEY | 0x84,
	KEY_KP_ADD = SPKEY | 0x85,
	KEY_KP_0 = SPKEY | 0x86,
	KEY_KP_1 = SPKEY | 0x87,
	KEY_KP_2 = SPKEY | 0x88,
	KEY_KP_3 = SPKEY | 0x89,
	KEY_KP_4 = SPKEY | 0x8A,
	KEY_KP_5 = SPKEY | 0x8B,
	KEY_KP_6 = SPKEY | 0x8C,
	KEY_KP_7 = SPKEY | 0x8D,
	KEY_KP_8 = SPKEY | 0x8E,
	KEY_KP_9 = SPKEY | 0x8F,
	KEY_SUPER_L = SPKEY | 0x2C,
	KEY_SUPER_R = SPKEY | 0x2D,
	KEY_MENU = SPKEY | 0x2E,
	KEY_HYPER_L = SPKEY | 0x2F,
	KEY_HYPER_R = SPKEY | 0x30,
	KEY_HELP = SPKEY | 0x31,
	KEY_DIRECTION_L = SPKEY | 0x32,
	KEY_DIRECTION_R = SPKEY | 0x33,
	KEY_BACK = SPKEY | 0x40,
	KEY_FORWARD = SPKEY | 0x41,
	KEY_STOP = SPKEY | 0x42,
	KEY_REFRESH = SPKEY | 0x43,
	KEY_VOLUMEDOWN = SPKEY | 0x44,
	KEY_VOLUMEMUTE = SPKEY | 0x45,
	KEY_VOLUMEUP = SPKEY | 0x46,
	KEY_BASSBOOST = SPKEY | 0x47,
	KEY_BASSUP = SPKEY | 0x48,
	KEY_BASSDOWN = SPKEY | 0x49,
	KEY_TREBLEUP = SPKEY | 0x4A,
	KEY_TREBLEDOWN = SPKEY | 0x4B,
	KEY_MEDIAPLAY = SPKEY | 0x4C,
	KEY_MEDIASTOP = SPKEY | 0x4D,
	KEY_MEDIAPREVIOUS = SPKEY | 0x4E,
	KEY_MEDIANEXT = SPKEY | 0x4F,
	KEY_MEDIARECORD = SPKEY | 0x50,
	KEY_HOMEPAGE = SPKEY | 0x51,
	KEY_FAVORITES = SPKEY | 0x52,
	KEY_SEARCH = SPKEY | 0x53,
	KEY_STANDBY = SPKEY | 0x54,
	KEY_OPENURL = SPKEY | 0x55,
	KEY_LAUNCHMAIL = SPKEY | 0x56,
	KEY_LAUNCHMEDIA = SPKEY | 0x57,
	KEY_LAUNCH0 = SPKEY | 0x58,
	KEY_LAUNCH1 = SPKEY | 0x59,
	KEY_LAUNCH2 = SPKEY | 0x5A,
	KEY_LAUNCH3 = SPKEY | 0x5B,
	KEY_LAUNCH4 = SPKEY | 0x5C,
	KEY_LAUNCH5 = SPKEY | 0x5D,
	KEY_LAUNCH6 = SPKEY | 0x5E,
	KEY_LAUNCH7 = SPKEY | 0x5F,
	KEY_LAUNCH8 = SPKEY | 0x60,
	KEY_LAUNCH9 = SPKEY | 0x61,
	KEY_LAUNCHA = SPKEY | 0x62,
	KEY_LAUNCHB = SPKEY | 0x63,
	KEY_LAUNCHC = SPKEY | 0x64,
	KEY_LAUNCHD = SPKEY | 0x65,
	KEY_LAUNCHE = SPKEY | 0x66,
	KEY_LAUNCHF = SPKEY | 0x67,

	KEY_UNKNOWN = SPKEY | 0xFFFFFF,

	/* PRINTABLE LATIN 1 CODES */

	KEY_SPACE = 0x0020,
	KEY_EXCLAM = 0x0021,
	KEY_QUOTEDBL = 0x0022,
	KEY_NUMBERSIGN = 0x0023,
	KEY_DOLLAR = 0x0024,
	KEY_PERCENT = 0x0025,
	KEY_AMPERSAND = 0x0026,
	KEY_APOSTROPHE = 0x0027,
	KEY_PARENLEFT = 0x0028,
	KEY_PARENRIGHT = 0x0029,
	KEY_ASTERISK = 0x002A,
	KEY_PLUS = 0x002B,
	KEY_COMMA = 0x002C,
	KEY_MINUS = 0x002D,
	KEY_PERIOD = 0x002E,
	KEY_SLASH = 0x002F,
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
	KEY_COLON = 0x003A,
	KEY_SEMICOLON = 0x003B,
	KEY_LESS = 0x003C,
	KEY_EQUAL = 0x003D,
	KEY_GREATER = 0x003E,
	KEY_QUESTION = 0x003F,
	KEY_AT = 0x0040,
	KEY_A = 0x0041,
	KEY_B = 0x0042,
	KEY_C = 0x0043,
	KEY_D = 0x0044,
	KEY_E = 0x0045,
	KEY_F = 0x0046,
	KEY_G = 0x0047,
	KEY_H = 0x0048,
	KEY_I = 0x0049,
	KEY_J = 0x004A,
	KEY_K = 0x004B,
	KEY_L = 0x004C,
	KEY_M = 0x004D,
	KEY_N = 0x004E,
	KEY_O = 0x004F,
	KEY_P = 0x0050,
	KEY_Q = 0x0051,
	KEY_R = 0x0052,
	KEY_S = 0x0053,
	KEY_T = 0x0054,
	KEY_U = 0x0055,
	KEY_V = 0x0056,
	KEY_W = 0x0057,
	KEY_X = 0x0058,
	KEY_Y = 0x0059,
	KEY_Z = 0x005A,
	KEY_BRACKETLEFT = 0x005B,
	KEY_BACKSLASH = 0x005C,
	KEY_BRACKETRIGHT = 0x005D,
	KEY_ASCIICIRCUM = 0x005E,
	KEY_UNDERSCORE = 0x005F,
	KEY_QUOTELEFT = 0x0060,
	KEY_BRACELEFT = 0x007B,
	KEY_BAR = 0x007C,
	KEY_BRACERIGHT = 0x007D,
	KEY_ASCIITILDE = 0x007E,
	KEY_NOBREAKSPACE = 0x00A0,
	KEY_EXCLAMDOWN = 0x00A1,
	KEY_CENT = 0x00A2,
	KEY_STERLING = 0x00A3,
	KEY_CURRENCY = 0x00A4,
	KEY_YEN = 0x00A5,
	KEY_BROKENBAR = 0x00A6,
	KEY_SECTION = 0x00A7,
	KEY_DIAERESIS = 0x00A8,
	KEY_COPYRIGHT = 0x00A9,
	KEY_ORDFEMININE = 0x00AA,
	KEY_GUILLEMOTLEFT = 0x00AB,
	KEY_NOTSIGN = 0x00AC,
	KEY_HYPHEN = 0x00AD,
	KEY_REGISTERED = 0x00AE,
	KEY_MACRON = 0x00AF,
	KEY_DEGREE = 0x00B0,
	KEY_PLUSMINUS = 0x00B1,
	KEY_TWOSUPERIOR = 0x00B2,
	KEY_THREESUPERIOR = 0x00B3,
	KEY_ACUTE = 0x00B4,
	KEY_MU = 0x00B5,
	KEY_PARAGRAPH = 0x00B6,
	KEY_PERIODCENTERED = 0x00B7,
	KEY_CEDILLA = 0x00B8,
	KEY_ONESUPERIOR = 0x00B9,
	KEY_MASCULINE = 0x00BA,
	KEY_GUILLEMOTRIGHT = 0x00BB,
	KEY_ONEQUARTER = 0x00BC,
	KEY_ONEHALF = 0x00BD,
	KEY_THREEQUARTERS = 0x00BE,
	KEY_QUESTIONDOWN = 0x00BF,
	KEY_AGRAVE = 0x00C0,
	KEY_AACUTE = 0x00C1,
	KEY_ACIRCUMFLEX = 0x00C2,
	KEY_ATILDE = 0x00C3,
	KEY_ADIAERESIS = 0x00C4,
	KEY_ARING = 0x00C5,
	KEY_AE = 0x00C6,
	KEY_CCEDILLA = 0x00C7,
	KEY_EGRAVE = 0x00C8,
	KEY_EACUTE = 0x00C9,
	KEY_ECIRCUMFLEX = 0x00CA,
	KEY_EDIAERESIS = 0x00CB,
	KEY_IGRAVE = 0x00CC,
	KEY_IACUTE = 0x00CD,
	KEY_ICIRCUMFLEX = 0x00CE,
	KEY_IDIAERESIS = 0x00CF,
	KEY_ETH = 0x00D0,
	KEY_NTILDE = 0x00D1,
	KEY_OGRAVE = 0x00D2,
	KEY_OACUTE = 0x00D3,
	KEY_OCIRCUMFLEX = 0x00D4,
	KEY_OTILDE = 0x00D5,
	KEY_ODIAERESIS = 0x00D6,
	KEY_MULTIPLY = 0x00D7,
	KEY_OOBLIQUE = 0x00D8,
	KEY_UGRAVE = 0x00D9,
	KEY_UACUTE = 0x00DA,
	KEY_UCIRCUMFLEX = 0x00DB,
	KEY_UDIAERESIS = 0x00DC,
	KEY_YACUTE = 0x00DD,
	KEY_THORN = 0x00DE,
	KEY_SSHARP = 0x00DF,

	KEY_DIVISION = 0x00F7,
	KEY_YDIAERESIS = 0x00FF,

};

enum KeyModifierMask {

	KEY_CODE_MASK = ((1 << 25) - 1), ///< Apply this mask to any keycode to remove modifiers.
	KEY_MODIFIER_MASK = (0xFF << 24), ///< Apply this mask to isolate modifiers.
	KEY_MASK_SHIFT = (1 << 25),
	KEY_MASK_ALT = (1 << 26),
	KEY_MASK_META = (1 << 27),
	KEY_MASK_CTRL = (1 << 28),
#ifdef APPLE_STYLE_KEYS
	KEY_MASK_CMD = KEY_MASK_META,
#else
	KEY_MASK_CMD = KEY_MASK_CTRL,
#endif

	KEY_MASK_KPAD = (1 << 29),
	KEY_MASK_GROUP_SWITCH = (1 << 30)
	// bit 31 can't be used because variant uses regular 32 bits int as datatype

};

String keycode_get_string(uint32_t p_code);
bool keycode_has_unicode(uint32_t p_keycode);
int find_keycode(const String &p_codestr);
const char *find_keycode_name(int p_keycode);
int keycode_get_count();
int keycode_get_value_by_index(int p_index);
const char *keycode_get_name_by_index(int p_index);

#endif // KEYBOARD_H
