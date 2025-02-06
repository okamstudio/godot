/**************************************************************************/
/*  key_mapping_macos.mm                                                  */
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

#include "key_mapping_macos.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

struct HashMapHasherKeys {
	static _FORCE_INLINE_ uint32_t hash(const Key p_key) { return hash_fmix32(static_cast<uint32_t>(p_key)); }
	static _FORCE_INLINE_ uint32_t hash(const char32_t p_uchar) { return hash_fmix32(p_uchar); }
	static _FORCE_INLINE_ uint32_t hash(const unsigned p_key) { return hash_fmix32(p_key); }
};

HashSet<unsigned int> numpad_keys;
HashMap<unsigned int, Key, HashMapHasherKeys> keysym_map;
HashMap<Key, unsigned int, HashMapHasherKeys> keysym_map_inv;
HashMap<Key, char32_t, HashMapHasherKeys> keycode_map;
HashMap<unsigned int, KeyLocation, HashMapHasherKeys> location_map;

void KeyMappingMacOS::initialize() {
	numpad_keys.insert(0x41); //kVK_ANSI_KeypadDecimal
	numpad_keys.insert(0x43); //kVK_ANSI_KeypadMultiply
	numpad_keys.insert(0x45); //kVK_ANSI_KeypadPlus
	numpad_keys.insert(0x47); //kVK_ANSI_KeypadClear
	numpad_keys.insert(0x4b); //kVK_ANSI_KeypadDivide
	numpad_keys.insert(0x4c); //kVK_ANSI_KeypadEnter
	numpad_keys.insert(0x4e); //kVK_ANSI_KeypadMinus
	numpad_keys.insert(0x51); //kVK_ANSI_KeypadEquals
	numpad_keys.insert(0x52); //kVK_ANSI_Keypad0
	numpad_keys.insert(0x53); //kVK_ANSI_Keypad1
	numpad_keys.insert(0x54); //kVK_ANSI_Keypad2
	numpad_keys.insert(0x55); //kVK_ANSI_Keypad3
	numpad_keys.insert(0x56); //kVK_ANSI_Keypad4
	numpad_keys.insert(0x57); //kVK_ANSI_Keypad5
	numpad_keys.insert(0x58); //kVK_ANSI_Keypad6
	numpad_keys.insert(0x59); //kVK_ANSI_Keypad7
	numpad_keys.insert(0x5b); //kVK_ANSI_Keypad8
	numpad_keys.insert(0x5c); //kVK_ANSI_Keypad9
	numpad_keys.insert(0x5f); //kVK_JIS_KeypadComma

	keysym_map[0x00] = Key::A;
	keysym_map[0x01] = Key::S;
	keysym_map[0x02] = Key::D;
	keysym_map[0x03] = Key::F;
	keysym_map[0x04] = Key::H;
	keysym_map[0x05] = Key::G;
	keysym_map[0x06] = Key::Z;
	keysym_map[0x07] = Key::X;
	keysym_map[0x08] = Key::C;
	keysym_map[0x09] = Key::V;
	keysym_map[0x0a] = Key::SECTION;
	keysym_map[0x0b] = Key::B;
	keysym_map[0x0c] = Key::Q;
	keysym_map[0x0d] = Key::W;
	keysym_map[0x0e] = Key::E;
	keysym_map[0x0f] = Key::R;
	keysym_map[0x10] = Key::Y;
	keysym_map[0x11] = Key::T;
	keysym_map[0x12] = Key::KEY_1;
	keysym_map[0x13] = Key::KEY_2;
	keysym_map[0x14] = Key::KEY_3;
	keysym_map[0x15] = Key::KEY_4;
	keysym_map[0x16] = Key::KEY_6;
	keysym_map[0x17] = Key::KEY_5;
	keysym_map[0x18] = Key::EQUAL;
	keysym_map[0x19] = Key::KEY_9;
	keysym_map[0x1a] = Key::KEY_7;
	keysym_map[0x1b] = Key::MINUS;
	keysym_map[0x1c] = Key::KEY_8;
	keysym_map[0x1d] = Key::KEY_0;
	keysym_map[0x1e] = Key::BRACKETRIGHT;
	keysym_map[0x1f] = Key::O;
	keysym_map[0x20] = Key::U;
	keysym_map[0x21] = Key::BRACKETLEFT;
	keysym_map[0x22] = Key::I;
	keysym_map[0x23] = Key::P;
	keysym_map[0x24] = Key::ENTER;
	keysym_map[0x25] = Key::L;
	keysym_map[0x26] = Key::J;
	keysym_map[0x27] = Key::APOSTROPHE;
	keysym_map[0x28] = Key::K;
	keysym_map[0x29] = Key::SEMICOLON;
	keysym_map[0x2a] = Key::BACKSLASH;
	keysym_map[0x2b] = Key::COMMA;
	keysym_map[0x2c] = Key::SLASH;
	keysym_map[0x2d] = Key::N;
	keysym_map[0x2e] = Key::M;
	keysym_map[0x2f] = Key::PERIOD;
	keysym_map[0x30] = Key::TAB;
	keysym_map[0x31] = Key::SPACE;
	keysym_map[0x32] = Key::QUOTELEFT;
	keysym_map[0x33] = Key::BACKSPACE;
	keysym_map[0x35] = Key::ESCAPE;
	keysym_map[0x36] = Key::META;
	keysym_map[0x37] = Key::META;
	keysym_map[0x38] = Key::SHIFT;
	keysym_map[0x39] = Key::CAPSLOCK;
	keysym_map[0x3a] = Key::ALT;
	keysym_map[0x3b] = Key::CTRL;
	keysym_map[0x3c] = Key::SHIFT;
	keysym_map[0x3d] = Key::ALT;
	keysym_map[0x3e] = Key::CTRL;
	keysym_map[0x40] = Key::F17;
	keysym_map[0x41] = Key::KP_PERIOD;
	keysym_map[0x43] = Key::KP_MULTIPLY;
	keysym_map[0x45] = Key::KP_ADD;
	keysym_map[0x47] = Key::NUMLOCK;
	keysym_map[0x48] = Key::VOLUMEUP;
	keysym_map[0x49] = Key::VOLUMEDOWN;
	keysym_map[0x4a] = Key::VOLUMEMUTE;
	keysym_map[0x4b] = Key::KP_DIVIDE;
	keysym_map[0x4c] = Key::KP_ENTER;
	keysym_map[0x4e] = Key::KP_SUBTRACT;
	keysym_map[0x4f] = Key::F18;
	keysym_map[0x50] = Key::F19;
	keysym_map[0x51] = Key::EQUAL;
	keysym_map[0x52] = Key::KP_0;
	keysym_map[0x53] = Key::KP_1;
	keysym_map[0x54] = Key::KP_2;
	keysym_map[0x55] = Key::KP_3;
	keysym_map[0x56] = Key::KP_4;
	keysym_map[0x57] = Key::KP_5;
	keysym_map[0x58] = Key::KP_6;
	keysym_map[0x59] = Key::KP_7;
	keysym_map[0x5a] = Key::F20;
	keysym_map[0x5b] = Key::KP_8;
	keysym_map[0x5c] = Key::KP_9;
	keysym_map[0x5d] = Key::YEN;
	keysym_map[0x5e] = Key::UNDERSCORE;
	keysym_map[0x5f] = Key::COMMA;
	keysym_map[0x60] = Key::F5;
	keysym_map[0x61] = Key::F6;
	keysym_map[0x62] = Key::F7;
	keysym_map[0x63] = Key::F3;
	keysym_map[0x64] = Key::F8;
	keysym_map[0x65] = Key::F9;
	keysym_map[0x66] = Key::JIS_EISU;
	keysym_map[0x67] = Key::F11;
	keysym_map[0x68] = Key::JIS_KANA;
	keysym_map[0x69] = Key::F13;
	keysym_map[0x6a] = Key::F16;
	keysym_map[0x6b] = Key::F14;
	keysym_map[0x6d] = Key::F10;
	keysym_map[0x6e] = Key::MENU;
	keysym_map[0x6f] = Key::F12;
	keysym_map[0x71] = Key::F15;
	keysym_map[0x72] = Key::INSERT;
	keysym_map[0x73] = Key::HOME;
	keysym_map[0x74] = Key::PAGEUP;
	keysym_map[0x75] = Key::KEY_DELETE;
	keysym_map[0x76] = Key::F4;
	keysym_map[0x77] = Key::END;
	keysym_map[0x78] = Key::F2;
	keysym_map[0x79] = Key::PAGEDOWN;
	keysym_map[0x7a] = Key::F1;
	keysym_map[0x7b] = Key::LEFT;
	keysym_map[0x7c] = Key::RIGHT;
	keysym_map[0x7d] = Key::DOWN;
	keysym_map[0x7e] = Key::UP;

	for (const KeyValue<unsigned int, Key> &E : keysym_map) {
		keysym_map_inv[E.value] = E.key;
	}

	keycode_map[Key::ESCAPE] = 0x001B;
	keycode_map[Key::TAB] = 0x0009;
	keycode_map[Key::BACKTAB] = 0x007F;
	keycode_map[Key::BACKSPACE] = 0x0008;
	keycode_map[Key::ENTER] = 0x000D;
	keycode_map[Key::INSERT] = NSInsertFunctionKey;
	keycode_map[Key::KEY_DELETE] = 0x007F;
	keycode_map[Key::PAUSE] = NSPauseFunctionKey;
	keycode_map[Key::PRINT] = NSPrintScreenFunctionKey;
	keycode_map[Key::SYSREQ] = NSSysReqFunctionKey;
	keycode_map[Key::CLEAR] = NSClearLineFunctionKey;
	keycode_map[Key::HOME] = 0x2196;
	keycode_map[Key::END] = 0x2198;
	keycode_map[Key::LEFT] = 0x001C;
	keycode_map[Key::UP] = 0x001E;
	keycode_map[Key::RIGHT] = 0x001D;
	keycode_map[Key::DOWN] = 0x001F;
	keycode_map[Key::PAGEUP] = 0x21DE;
	keycode_map[Key::PAGEDOWN] = 0x21DF;
	keycode_map[Key::NUMLOCK] = NSClearLineFunctionKey;
	keycode_map[Key::SCROLLLOCK] = NSScrollLockFunctionKey;
	keycode_map[Key::F1] = NSF1FunctionKey;
	keycode_map[Key::F2] = NSF2FunctionKey;
	keycode_map[Key::F3] = NSF3FunctionKey;
	keycode_map[Key::F4] = NSF4FunctionKey;
	keycode_map[Key::F5] = NSF5FunctionKey;
	keycode_map[Key::F6] = NSF6FunctionKey;
	keycode_map[Key::F7] = NSF7FunctionKey;
	keycode_map[Key::F8] = NSF8FunctionKey;
	keycode_map[Key::F9] = NSF9FunctionKey;
	keycode_map[Key::F10] = NSF10FunctionKey;
	keycode_map[Key::F11] = NSF11FunctionKey;
	keycode_map[Key::F12] = NSF12FunctionKey;
	keycode_map[Key::F13] = NSF13FunctionKey;
	keycode_map[Key::F14] = NSF14FunctionKey;
	keycode_map[Key::F15] = NSF15FunctionKey;
	keycode_map[Key::F16] = NSF16FunctionKey;
	keycode_map[Key::F17] = NSF17FunctionKey;
	keycode_map[Key::F18] = NSF18FunctionKey;
	keycode_map[Key::F19] = NSF19FunctionKey;
	keycode_map[Key::F20] = NSF20FunctionKey;
	keycode_map[Key::F21] = NSF21FunctionKey;
	keycode_map[Key::F22] = NSF22FunctionKey;
	keycode_map[Key::F23] = NSF23FunctionKey;
	keycode_map[Key::F24] = NSF24FunctionKey;
	keycode_map[Key::F25] = NSF25FunctionKey;
	keycode_map[Key::F26] = NSF26FunctionKey;
	keycode_map[Key::F27] = NSF27FunctionKey;
	keycode_map[Key::F28] = NSF28FunctionKey;
	keycode_map[Key::F29] = NSF29FunctionKey;
	keycode_map[Key::F30] = NSF30FunctionKey;
	keycode_map[Key::F31] = NSF31FunctionKey;
	keycode_map[Key::F32] = NSF32FunctionKey;
	keycode_map[Key::F33] = NSF33FunctionKey;
	keycode_map[Key::F34] = NSF34FunctionKey;
	keycode_map[Key::F35] = NSF35FunctionKey;
	keycode_map[Key::MENU] = NSMenuFunctionKey;
	keycode_map[Key::HELP] = NSHelpFunctionKey;
	keycode_map[Key::STOP] = NSStopFunctionKey;
	keycode_map[Key::LAUNCH0] = NSUserFunctionKey;
	keycode_map[Key::SPACE] = 0x0020;
	keycode_map[Key::EXCLAM] = '!';
	keycode_map[Key::QUOTEDBL] = '\"';
	keycode_map[Key::NUMBERSIGN] = '#';
	keycode_map[Key::DOLLAR] = '$';
	keycode_map[Key::PERCENT] = '\%';
	keycode_map[Key::AMPERSAND] = '&';
	keycode_map[Key::APOSTROPHE] = '\'';
	keycode_map[Key::PARENLEFT] = '(';
	keycode_map[Key::PARENRIGHT] = ')';
	keycode_map[Key::ASTERISK] = '*';
	keycode_map[Key::PLUS] = '+';
	keycode_map[Key::COMMA] = ',';
	keycode_map[Key::MINUS] = '-';
	keycode_map[Key::PERIOD] = '.';
	keycode_map[Key::SLASH] = '/';
	keycode_map[Key::KEY_0] = '0';
	keycode_map[Key::KEY_1] = '1';
	keycode_map[Key::KEY_2] = '2';
	keycode_map[Key::KEY_3] = '3';
	keycode_map[Key::KEY_4] = '4';
	keycode_map[Key::KEY_5] = '5';
	keycode_map[Key::KEY_6] = '6';
	keycode_map[Key::KEY_7] = '7';
	keycode_map[Key::KEY_8] = '8';
	keycode_map[Key::KEY_9] = '9';
	keycode_map[Key::COLON] = ':';
	keycode_map[Key::SEMICOLON] = ';';
	keycode_map[Key::LESS] = '<';
	keycode_map[Key::EQUAL] = '=';
	keycode_map[Key::GREATER] = '>';
	keycode_map[Key::QUESTION] = '?';
	keycode_map[Key::AT] = '@';
	keycode_map[Key::A] = 'a';
	keycode_map[Key::B] = 'b';
	keycode_map[Key::C] = 'c';
	keycode_map[Key::D] = 'd';
	keycode_map[Key::E] = 'e';
	keycode_map[Key::F] = 'f';
	keycode_map[Key::G] = 'g';
	keycode_map[Key::H] = 'h';
	keycode_map[Key::I] = 'i';
	keycode_map[Key::J] = 'j';
	keycode_map[Key::K] = 'k';
	keycode_map[Key::L] = 'l';
	keycode_map[Key::M] = 'm';
	keycode_map[Key::N] = 'n';
	keycode_map[Key::O] = 'o';
	keycode_map[Key::P] = 'p';
	keycode_map[Key::Q] = 'q';
	keycode_map[Key::R] = 'r';
	keycode_map[Key::S] = 's';
	keycode_map[Key::T] = 't';
	keycode_map[Key::U] = 'u';
	keycode_map[Key::V] = 'v';
	keycode_map[Key::W] = 'w';
	keycode_map[Key::X] = 'x';
	keycode_map[Key::Y] = 'y';
	keycode_map[Key::Z] = 'z';
	keycode_map[Key::BRACKETLEFT] = '[';
	keycode_map[Key::BACKSLASH] = '\\';
	keycode_map[Key::BRACKETRIGHT] = ']';
	keycode_map[Key::ASCIICIRCUM] = '^';
	keycode_map[Key::UNDERSCORE] = '_';
	keycode_map[Key::QUOTELEFT] = '`';
	keycode_map[Key::BRACELEFT] = '{';
	keycode_map[Key::BAR] = '|';
	keycode_map[Key::BRACERIGHT] = '}';
	keycode_map[Key::ASCIITILDE] = '~';

	// Keysym -> physical location.
	// Ctrl.
	location_map[0x3b] = KeyLocation::LEFT;
	location_map[0x3e] = KeyLocation::RIGHT;
	// Shift.
	location_map[0x38] = KeyLocation::LEFT;
	location_map[0x3c] = KeyLocation::RIGHT;
	// Alt/Option.
	location_map[0x3a] = KeyLocation::LEFT;
	location_map[0x3d] = KeyLocation::RIGHT;
	// Meta/Command (yes, right < left).
	location_map[0x36] = KeyLocation::RIGHT;
	location_map[0x37] = KeyLocation::LEFT;
}

bool KeyMappingMacOS::is_numpad_key(unsigned int p_key) {
	return numpad_keys.has(p_key);
}

// Translates a macOS keycode to a Godot keycode.
Key KeyMappingMacOS::translate_key(unsigned int p_key) {
	const Key *key = keysym_map.getptr(p_key);
	if (key) {
		return *key;
	}
	return Key::NONE;
}

// Translates a Godot keycode back to a macOS keycode.
unsigned int KeyMappingMacOS::unmap_key(Key p_key) {
	const unsigned int *key = keysym_map_inv.getptr(p_key);
	if (key) {
		return *key;
	}
	return 127;
}

// Remap key according to current keyboard layout.
Key KeyMappingMacOS::remap_key(unsigned int p_key, unsigned int p_state, bool p_unicode) {
	if (is_numpad_key(p_key)) {
		return translate_key(p_key);
	}

	TISInputSourceRef current_keyboard = TISCopyCurrentKeyboardInputSource();
	if (!current_keyboard) {
		return translate_key(p_key);
	}

	CFDataRef layout_data = (CFDataRef)TISGetInputSourceProperty(current_keyboard, kTISPropertyUnicodeKeyLayoutData);
	if (!layout_data) {
		return translate_key(p_key);
	}

	const UCKeyboardLayout *keyboard_layout = (const UCKeyboardLayout *)CFDataGetBytePtr(layout_data);

	String keysym;
	UInt32 keys_down = 0;
	UniChar chars[256] = {};
	UniCharCount real_length = 0;

	OSStatus err = UCKeyTranslate(keyboard_layout,
			p_key,
			kUCKeyActionDisplay,
			(p_unicode) ? 0 : (p_state >> 8) & 0xFF,
			LMGetKbdType(),
			kUCKeyTranslateNoDeadKeysBit,
			&keys_down,
			std::size(chars),
			&real_length,
			chars);

	if (err != noErr) {
		return translate_key(p_key);
	}

	keysym = String::utf16((char16_t *)chars, real_length);
	if (keysym.is_empty()) {
		return translate_key(p_key);
	}

	char32_t c = keysym[0];
	if (p_unicode) {
		return fix_key_label(c, translate_key(p_key));
	} else {
		return fix_keycode(c, translate_key(p_key));
	}
}

// Translates a macOS keycode to a Godot key location.
KeyLocation KeyMappingMacOS::translate_location(unsigned int p_key) {
	const KeyLocation *location = location_map.getptr(p_key);
	if (location) {
		return *location;
	}
	return KeyLocation::UNSPECIFIED;
}

String KeyMappingMacOS::keycode_get_native_string(Key p_keycode) {
	const char32_t *key = keycode_map.getptr(p_keycode);
	if (key) {
		return String::chr(*key);
	}
	return String();
}

unsigned int KeyMappingMacOS::keycode_get_native_mask(Key p_keycode) {
	unsigned int mask = 0;
	if ((p_keycode & KeyModifierMask::CTRL) != Key::NONE) {
		mask |= NSEventModifierFlagControl;
	}
	if ((p_keycode & KeyModifierMask::ALT) != Key::NONE) {
		mask |= NSEventModifierFlagOption;
	}
	if ((p_keycode & KeyModifierMask::SHIFT) != Key::NONE) {
		mask |= NSEventModifierFlagShift;
	}
	if ((p_keycode & KeyModifierMask::META) != Key::NONE) {
		mask |= NSEventModifierFlagCommand;
	}
	if ((p_keycode & KeyModifierMask::KPAD) != Key::NONE) {
		mask |= NSEventModifierFlagNumericPad;
	}
	return mask;
}
