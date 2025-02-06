/**************************************************************************/
/*  key_mapping_x11.h                                                     */
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
#include "core/templates/hash_map.h"

#include <X11/XF86keysym.h>
#include <X11/Xlib.h>

#define XK_MISCELLANY
#define XK_LATIN1
#define XK_XKB_KEYS
#include <X11/keysymdef.h>

class KeyMappingX11 {
	struct HashMapHasherKeys {
		static _FORCE_INLINE_ uint32_t hash(const Key p_key) { return hash_fmix32(static_cast<uint32_t>(p_key)); }
		static _FORCE_INLINE_ uint32_t hash(const char32_t p_uchar) { return hash_fmix32(p_uchar); }
		static _FORCE_INLINE_ uint32_t hash(const unsigned p_key) { return hash_fmix32(p_key); }
		static _FORCE_INLINE_ uint32_t hash(const KeySym p_key) { return hash_fmix32(p_key); }
	};

	static inline HashMap<KeySym, Key, HashMapHasherKeys> xkeysym_map;
	static inline HashMap<unsigned int, Key, HashMapHasherKeys> scancode_map;
	static inline HashMap<Key, unsigned int, HashMapHasherKeys> scancode_map_inv;
	static inline HashMap<KeySym, char32_t, HashMapHasherKeys> xkeysym_unicode_map;
	static inline HashMap<unsigned int, KeyLocation, HashMapHasherKeys> location_map;

	KeyMappingX11() {}

public:
	static void initialize();

	static bool is_sym_numpad(KeySym p_keysym);
	static Key get_keycode(KeySym p_keysym);
	static unsigned int get_xlibcode(Key p_keysym);
	static Key get_scancode(unsigned int p_code);
	static char32_t get_unicode_from_keysym(KeySym p_keysym);
	static KeyLocation get_location(unsigned int p_code);
};
