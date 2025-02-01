/**************************************************************************/
/*  android_keys_utils.h                                                  */
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

#include "core/os/keyboard.h"

#include <android/input.h>

#define AKEYCODE_MAX 0xFFFF

struct AndroidGodotCodePair {
	unsigned int android_code = 0;
	Key godot_code = Key::NONE;
};

static AndroidGodotCodePair android_godot_code_pairs[] = {
	{ AKEYCODE_UNKNOWN, Key::UNKNOWN }, // (0) Unknown key code.
	{ AKEYCODE_BACK, Key::BACK }, // (4) Back key.
	{ AKEYCODE_0, Key::KEY_0 }, // (7) '0' key.
	{ AKEYCODE_1, Key::KEY_1 }, // (8) '1' key.
	{ AKEYCODE_2, Key::KEY_2 }, // (9) '2' key.
	{ AKEYCODE_3, Key::KEY_3 }, // (10) '3' key.
	{ AKEYCODE_4, Key::KEY_4 }, // (11) '4' key.
	{ AKEYCODE_5, Key::KEY_5 }, // (12) '5' key.
	{ AKEYCODE_6, Key::KEY_6 }, // (13) '6' key.
	{ AKEYCODE_7, Key::KEY_7 }, // (14) '7' key.
	{ AKEYCODE_8, Key::KEY_8 }, // (15) '8' key.
	{ AKEYCODE_9, Key::KEY_9 }, // (16) '9' key.
	{ AKEYCODE_STAR, Key::ASTERISK }, // (17) '*' key.
	{ AKEYCODE_POUND, Key::NUMBERSIGN }, // (18) '#' key.
	{ AKEYCODE_DPAD_UP, Key::UP }, // (19) Directional Pad Up key.
	{ AKEYCODE_DPAD_DOWN, Key::DOWN }, // (20) Directional Pad Down key.
	{ AKEYCODE_DPAD_LEFT, Key::LEFT }, // (21) Directional Pad Left key.
	{ AKEYCODE_DPAD_RIGHT, Key::RIGHT }, // (22) Directional Pad Right key.
	{ AKEYCODE_DPAD_CENTER, Key::ENTER }, // (23) Directional Pad Center key.
	{ AKEYCODE_VOLUME_UP, Key::VOLUMEUP }, // (24) Volume Up key.
	{ AKEYCODE_VOLUME_DOWN, Key::VOLUMEDOWN }, // (25) Volume Down key.
	{ AKEYCODE_POWER, Key::STANDBY }, // (26) Power key.
	{ AKEYCODE_CLEAR, Key::CLEAR }, // (28) Clear key.
	{ AKEYCODE_A, Key::A }, // (29) 'A' key.
	{ AKEYCODE_B, Key::B }, // (30) 'B' key.
	{ AKEYCODE_C, Key::C }, // (31) 'C' key.
	{ AKEYCODE_D, Key::D }, // (32) 'D' key.
	{ AKEYCODE_E, Key::E }, // (33) 'E' key.
	{ AKEYCODE_F, Key::F }, // (34) 'F' key.
	{ AKEYCODE_G, Key::G }, // (35) 'G' key.
	{ AKEYCODE_H, Key::H }, // (36) 'H' key.
	{ AKEYCODE_I, Key::I }, // (37) 'I' key.
	{ AKEYCODE_J, Key::J }, // (38) 'J' key.
	{ AKEYCODE_K, Key::K }, // (39) 'K' key.
	{ AKEYCODE_L, Key::L }, // (40) 'L' key.
	{ AKEYCODE_M, Key::M }, // (41) 'M' key.
	{ AKEYCODE_N, Key::N }, // (42) 'N' key.
	{ AKEYCODE_O, Key::O }, // (43) 'O' key.
	{ AKEYCODE_P, Key::P }, // (44) 'P' key.
	{ AKEYCODE_Q, Key::Q }, // (45) 'Q' key.
	{ AKEYCODE_R, Key::R }, // (46) 'R' key.
	{ AKEYCODE_S, Key::S }, // (47) 'S' key.
	{ AKEYCODE_T, Key::T }, // (48) 'T' key.
	{ AKEYCODE_U, Key::U }, // (49) 'U' key.
	{ AKEYCODE_V, Key::V }, // (50) 'V' key.
	{ AKEYCODE_W, Key::W }, // (51) 'W' key.
	{ AKEYCODE_X, Key::X }, // (52) 'X' key.
	{ AKEYCODE_Y, Key::Y }, // (53) 'Y' key.
	{ AKEYCODE_Z, Key::Z }, // (54) 'Z' key.
	{ AKEYCODE_COMMA, Key::COMMA }, // (55) ',’ key.
	{ AKEYCODE_PERIOD, Key::PERIOD }, // (56) '.' key.
	{ AKEYCODE_ALT_LEFT, Key::ALT }, // (57) Left Alt modifier key.
	{ AKEYCODE_ALT_RIGHT, Key::ALT }, // (58) Right Alt modifier key.
	{ AKEYCODE_SHIFT_LEFT, Key::SHIFT }, // (59) Left Shift modifier key.
	{ AKEYCODE_SHIFT_RIGHT, Key::SHIFT }, // (60) Right Shift modifier key.
	{ AKEYCODE_TAB, Key::TAB }, // (61) Tab key.
	{ AKEYCODE_SPACE, Key::SPACE }, // (62) Space key.
	{ AKEYCODE_ENVELOPE, Key::LAUNCHMAIL }, // (65) Envelope special function key.
	{ AKEYCODE_ENTER, Key::ENTER }, // (66) Enter key.
	{ AKEYCODE_DEL, Key::BACKSPACE }, // (67) Backspace key.
	{ AKEYCODE_GRAVE, Key::QUOTELEFT }, // (68) '`' (backtick) key.
	{ AKEYCODE_MINUS, Key::MINUS }, // (69) '-'.
	{ AKEYCODE_EQUALS, Key::EQUAL }, // (70) '=' key.
	{ AKEYCODE_LEFT_BRACKET, Key::BRACKETLEFT }, // (71) '[' key.
	{ AKEYCODE_RIGHT_BRACKET, Key::BRACKETRIGHT }, // (72) ']' key.
	{ AKEYCODE_BACKSLASH, Key::BACKSLASH }, // (73) '\' key.
	{ AKEYCODE_SEMICOLON, Key::SEMICOLON }, // (74) ';' key.
	{ AKEYCODE_APOSTROPHE, Key::APOSTROPHE }, // (75) ''' (apostrophe) key.
	{ AKEYCODE_SLASH, Key::SLASH }, // (76) '/' key.
	{ AKEYCODE_AT, Key::AT }, // (77) '@' key.
	{ AKEYCODE_PLUS, Key::PLUS }, // (81) '+' key.
	{ AKEYCODE_MENU, Key::MENU }, // (82) Menu key.
	{ AKEYCODE_SEARCH, Key::SEARCH }, // (84) Search key.
	{ AKEYCODE_MEDIA_STOP, Key::MEDIASTOP }, // (86) Stop media key.
	{ AKEYCODE_MEDIA_NEXT, Key::MEDIANEXT }, // (87) Play Next media key.
	{ AKEYCODE_MEDIA_PREVIOUS, Key::MEDIAPREVIOUS }, // (88) Play Previous media key.
	{ AKEYCODE_PAGE_UP, Key::PAGEUP }, // (92) Page Up key.
	{ AKEYCODE_PAGE_DOWN, Key::PAGEDOWN }, // (93) Page Down key.
	{ AKEYCODE_ESCAPE, Key::ESCAPE }, // (111) Escape key.
	{ AKEYCODE_FORWARD_DEL, Key::KEY_DELETE }, // (112) Forward Delete key.
	{ AKEYCODE_CTRL_LEFT, Key::CTRL }, // (113) Left Control modifier key.
	{ AKEYCODE_CTRL_RIGHT, Key::CTRL }, // (114) Right Control modifier key.
	{ AKEYCODE_CAPS_LOCK, Key::CAPSLOCK }, // (115) Caps Lock key.
	{ AKEYCODE_SCROLL_LOCK, Key::SCROLLLOCK }, // (116) Scroll Lock key.
	{ AKEYCODE_META_LEFT, Key::META }, // (117) Left Meta modifier key.
	{ AKEYCODE_META_RIGHT, Key::META }, // (118) Right Meta modifier key.
	{ AKEYCODE_SYSRQ, Key::PRINT }, // (120) System Request / Print Screen key.
	{ AKEYCODE_BREAK, Key::PAUSE }, // (121) Break / Pause key.
	{ AKEYCODE_MOVE_HOME, Key::HOME }, // (122) Home Movement key.
	{ AKEYCODE_MOVE_END, Key::END }, // (123) End Movement key.
	{ AKEYCODE_INSERT, Key::INSERT }, // (124) Insert key.
	{ AKEYCODE_FORWARD, Key::FORWARD }, // (125) Forward key.
	{ AKEYCODE_MEDIA_PLAY, Key::MEDIAPLAY }, // (126) Play media key.
	{ AKEYCODE_MEDIA_RECORD, Key::MEDIARECORD }, // (130) Record media key.
	{ AKEYCODE_F1, Key::F1 }, // (131) F1 key.
	{ AKEYCODE_F2, Key::F2 }, // (132) F2 key.
	{ AKEYCODE_F3, Key::F3 }, // (133) F3 key.
	{ AKEYCODE_F4, Key::F4 }, // (134) F4 key.
	{ AKEYCODE_F5, Key::F5 }, // (135) F5 key.
	{ AKEYCODE_F6, Key::F6 }, // (136) F6 key.
	{ AKEYCODE_F7, Key::F7 }, // (137) F7 key.
	{ AKEYCODE_F8, Key::F8 }, // (138) F8 key.
	{ AKEYCODE_F9, Key::F9 }, // (139) F9 key.
	{ AKEYCODE_F10, Key::F10 }, // (140) F10 key.
	{ AKEYCODE_F11, Key::F11 }, // (141) F11 key.
	{ AKEYCODE_F12, Key::F12 }, // (142) F12 key.
	{ AKEYCODE_NUM_LOCK, Key::NUMLOCK }, // (143) Num Lock key.
	{ AKEYCODE_NUMPAD_0, Key::KP_0 }, // (144) Numeric keypad '0' key.
	{ AKEYCODE_NUMPAD_1, Key::KP_1 }, // (145) Numeric keypad '1' key.
	{ AKEYCODE_NUMPAD_2, Key::KP_2 }, // (146) Numeric keypad '2' key.
	{ AKEYCODE_NUMPAD_3, Key::KP_3 }, // (147) Numeric keypad '3' key.
	{ AKEYCODE_NUMPAD_4, Key::KP_4 }, // (148) Numeric keypad '4' key.
	{ AKEYCODE_NUMPAD_5, Key::KP_5 }, // (149) Numeric keypad '5' key.
	{ AKEYCODE_NUMPAD_6, Key::KP_6 }, // (150) Numeric keypad '6' key.
	{ AKEYCODE_NUMPAD_7, Key::KP_7 }, // (151) Numeric keypad '7' key.
	{ AKEYCODE_NUMPAD_8, Key::KP_8 }, // (152) Numeric keypad '8' key.
	{ AKEYCODE_NUMPAD_9, Key::KP_9 }, // (153) Numeric keypad '9' key.
	{ AKEYCODE_NUMPAD_DIVIDE, Key::KP_DIVIDE }, // (154) Numeric keypad '/' key (for division).
	{ AKEYCODE_NUMPAD_MULTIPLY, Key::KP_MULTIPLY }, // (155) Numeric keypad '*' key (for multiplication).
	{ AKEYCODE_NUMPAD_SUBTRACT, Key::KP_SUBTRACT }, // (156) Numeric keypad '-' key (for subtraction).
	{ AKEYCODE_NUMPAD_ADD, Key::KP_ADD }, // (157) Numeric keypad '+' key (for addition).
	{ AKEYCODE_NUMPAD_DOT, Key::KP_PERIOD }, // (158) Numeric keypad '.' key (for decimals or digit grouping).
	{ AKEYCODE_NUMPAD_ENTER, Key::KP_ENTER }, // (160) Numeric keypad Enter key.
	{ AKEYCODE_VOLUME_MUTE, Key::VOLUMEMUTE }, // (164) Volume Mute key.
	{ AKEYCODE_EISU, Key::JIS_EISU }, // (212) JIS EISU key.
	{ AKEYCODE_YEN, Key::YEN }, // (216) Japanese Yen key.
	{ AKEYCODE_KANA, Key::JIS_KANA }, // (218) JIS KANA key.
	{ AKEYCODE_HELP, Key::HELP }, // (259) Help key.
	{ AKEYCODE_REFRESH, Key::REFRESH }, // (285) Refresh key.
	{ AKEYCODE_MAX, Key::UNKNOWN }
};

Key godot_code_from_android_code(unsigned int p_code);

// Key location determination.
struct AndroidGodotLocationPair {
	unsigned int android_code = 0;
	KeyLocation godot_code = KeyLocation::UNSPECIFIED;
};

static AndroidGodotLocationPair android_godot_location_pairs[] = {
	{ AKEYCODE_ALT_LEFT, KeyLocation::LEFT },
	{ AKEYCODE_ALT_RIGHT, KeyLocation::RIGHT },
	{ AKEYCODE_SHIFT_LEFT, KeyLocation::LEFT },
	{ AKEYCODE_SHIFT_RIGHT, KeyLocation::RIGHT },
	{ AKEYCODE_CTRL_LEFT, KeyLocation::LEFT },
	{ AKEYCODE_CTRL_RIGHT, KeyLocation::RIGHT },
	{ AKEYCODE_META_LEFT, KeyLocation::LEFT },
	{ AKEYCODE_META_RIGHT, KeyLocation::RIGHT },
	{ AKEYCODE_MAX, KeyLocation::UNSPECIFIED }
};

KeyLocation godot_location_from_android_code(unsigned int p_code);
