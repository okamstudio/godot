/**************************************************************************/
/*  translation_po.h                                                      */
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

//#define DEBUG_TRANSLATION_PO

#include "core/math/expression.h"
#include "core/string/translation.h"

class TranslationPO : public Translation {
	GDCLASS(TranslationPO, Translation);

	// TLDR: Maps context to a list of source strings and translated strings. In PO terms, maps msgctxt to a list of msgid and msgstr.
	// The first key corresponds to context, and the second key (of the contained HashMap) corresponds to source string.
	// The value Vector<StringName> in the second map stores the translated strings. Index 0, 1, 2 matches msgstr[0], msgstr[1], msgstr[2]... in the case of plurals.
	// Otherwise index 0 matches to msgstr in a singular translation.
	// Strings without context have "" as first key.
	HashMap<StringName, HashMap<StringName, Vector<StringName>>> translation_map;

	int plural_forms = 0; // 0 means no "Plural-Forms" is given in the PO header file. The min for all languages is 1.
	String plural_rule;

	// Cache temporary variables related to _get_plural_index() to make it faster
	class EQNode : public RefCounted {
	public:
		String regex;
		Ref<EQNode> left;
		Ref<EQNode> right;
	};
	Ref<EQNode> equi_tests;

	int _find_unquoted(const String &p_src, char32_t p_chr) const;
	int _eq_test(const Ref<EQNode> &p_node, const Variant &p_result) const;

	Vector<String> input_name;
	mutable Ref<Expression> expr;
	mutable Array input_val;
	mutable StringName last_plural_key;
	mutable StringName last_plural_context;
	mutable int last_plural_n = -1; // Set it to an impossible value at the beginning.
	mutable int last_plural_mapped_index = 0;

	void _cache_plural_tests(const String &p_plural_rule, Ref<EQNode> &p_node);
	int _get_plural_index(int p_n) const;

	Vector<String> _get_message_list() const override;
	Dictionary _get_messages() const override;
	void _set_messages(const Dictionary &p_messages) override;

protected:
	static void _bind_methods();

public:
	Vector<String> get_translated_message_list() const override;
	void get_message_list(List<StringName> *r_messages) const override;
	int get_message_count() const override;
	void add_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context = "") override;
	void add_plural_message(const StringName &p_src_text, const Vector<String> &p_plural_xlated_texts, const StringName &p_context = "") override;
	StringName get_message(const StringName &p_src_text, const StringName &p_context = "") const override;
	StringName get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context = "") const override;
	void erase_message(const StringName &p_src_text, const StringName &p_context = "") override;

	void set_plural_rule(const String &p_plural_rule);
	int get_plural_forms() const;
	String get_plural_rule() const;

#ifdef DEBUG_TRANSLATION_PO
	void print_translation_map();
#endif

	TranslationPO() {}
};
