/**************************************************************************/
/*  pot_generator.h                                                       */
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

#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

//#define DEBUG_POT

class POTGenerator {
	static POTGenerator *singleton;

	struct MsgidData {
		String ctx;
		String plural;
		HashSet<String> locations;
		HashSet<String> comments;
	};
	// Store msgid as key and the additional data around the msgid - if it's under a context, has plurals and its file locations.
	HashMap<String, Vector<MsgidData>> all_translation_strings;

	void _write_to_pot(const String &p_file);
	void _write_msgid(Ref<FileAccess> r_file, const String &p_id, bool p_plural);
	void _add_new_msgid(const String &p_msgid, const String &p_context, const String &p_plural, const String &p_location, const String &p_comment);

#ifdef DEBUG_POT
	void _print_all_translation_strings();
#endif

public:
	static POTGenerator *get_singleton();
	void generate_pot(const String &p_file);

	POTGenerator();
	~POTGenerator();
};
