/**************************************************************************/
/*  key_mapping_xkb.h                                                     */
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

#ifdef SOWRAP_ENABLED
#include "xkbcommon-so_wrap.h"
#else
#include <xkbcommon/xkbcommon.h>
#endif // SOWRAP_ENABLED

class KeyMappingXKB {
	struct HashMapHasherKeys {
		static _FORCE_INLINE_ uint32_t hash(Key p_key) { return hash_fmix32(static_cast<uint32_t>(p_key)); }
		static _FORCE_INLINE_ uint32_t hash(unsigned p_key) { return hash_fmix32(p_key); }
	};

	static inline HashMap<xkb_keycode_t, Key, HashMapHasherKeys> xkb_keycode_map;
	static inline HashMap<unsigned int, Key, HashMapHasherKeys> scancode_map;
	static inline HashMap<Key, unsigned int, HashMapHasherKeys> scancode_map_inv;
	static inline HashMap<unsigned int, KeyLocation, HashMapHasherKeys> location_map;

	KeyMappingXKB() {}

public:
	static void initialize();

	static bool is_sym_numpad(xkb_keysym_t p_keysym);
	static Key get_keycode(xkb_keysym_t p_keysym);
	static xkb_keycode_t get_xkb_keycode(Key p_keycode);
	static Key get_scancode(unsigned int p_code);
	static KeyLocation get_location(unsigned int p_code);
};
