/**************************************************************************/
/*  regex.h                                                               */
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

#ifndef REGEX_H
#define REGEX_H

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class RegExMatch : public RefCounted {
	GDCLASS(RegExMatch, RefCounted);

	struct Range {
		int start = 0;
		int end = 0;
	};

	String subject;
	Vector<Range> data;
	HashMap<String, int> names;

	friend class RegEx;

protected:
	static void _bind_methods();

	int _find(const Variant &p_name) const;

public:
	String get_subject() const;
	int get_group_count() const;
	Dictionary get_names() const;

	PackedStringArray get_strings() const;
	String get_string(const Variant &p_name) const;
	int get_start(const Variant &p_name) const;
	int get_end(const Variant &p_name) const;
};

class RegEx : public RefCounted {
	GDCLASS(RegEx, RefCounted);

	void *general_ctx = nullptr;
	void *code = nullptr;
	String pattern;

	void _pattern_info(uint32_t what, void *where) const;

	int _sub(const String &p_subject, const String &p_replacement, int p_offset, int p_end, uint32_t p_flags, String &r_output) const;

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	static Ref<RegEx> _create_from_string_bind_compat_95212(const String &p_pattern);
	Error _compile_bind_compat_95212(const String &p_pattern);
	static void _bind_compatibility_methods();
#endif

public:
	static Ref<RegEx> create_from_string(const String &p_pattern, bool p_show_error = true);

	void clear();
	Error compile(const String &p_pattern, bool p_show_error = true);

	Ref<RegExMatch> search(const String &p_subject, int p_offset = 0, int p_end = -1) const;
	TypedArray<RegExMatch> search_all(const String &p_subject, int p_offset = 0, int p_end = -1) const;
	String sub(const String &p_subject, const String &p_replacement, bool p_all = false, int p_offset = 0, int p_end = -1) const;

	bool is_valid() const;
	String get_pattern() const;
	int get_group_count() const;
	PackedStringArray get_names() const;

	RegEx();
	RegEx(const String &p_pattern);
	~RegEx();
};

#endif // REGEX_H
