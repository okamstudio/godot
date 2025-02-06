/**************************************************************************/
/*  translation.h                                                         */
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

#ifndef TRANSLATION_H
#define TRANSLATION_H

#include "core/io/resource.h"
#include "core/math/expression.h"
#include "core/object/gdvirtual.gen.inc"

class Translation : public Resource {
	GDCLASS(Translation, Resource);
	OBJ_SAVE_TYPE(Translation);
	RES_BASE_EXTENSION("translation");

	String locale = "en";
	HashMap<StringName, Vector<String>> translation_map;

	mutable StringName last_plural_key;
	mutable int last_plural_n = -1; // Set it to an impossible value at the beginning.
	mutable int last_plural_mapped_index = 0;

	virtual Vector<String> _get_message_list() const;
	virtual Dictionary _get_messages() const;
	virtual void _set_messages(const Dictionary &p_messages);

	void _notify_translation_changed_if_applies();

protected:
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

	void _cache_plural_tests(const String &p_plural_rule, Ref<EQNode> &p_node);
	int _get_plural_index(int p_n) const;

	static void _bind_methods();

	GDVIRTUAL2RC(StringName, _get_message, StringName, StringName);
	GDVIRTUAL4RC(StringName, _get_plural_message, StringName, StringName, int, StringName);
	GDVIRTUAL2RC(Vector<String>, _get_plural_messages, StringName, StringName);

public:
	void set_locale(const String &p_locale);
	_FORCE_INLINE_ String get_locale() const { return locale; }

	virtual void add_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context = "");
	virtual void add_plural_message(const StringName &p_src_text, const Vector<String> &p_plural_xlated_texts, const StringName &p_context = "");
	virtual StringName get_message(const StringName &p_src_text, const StringName &p_context = "") const; //overridable for other implementations
	virtual StringName get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context = "") const;
	virtual Vector<String> get_plural_messages(const StringName &p_src_text, const StringName &p_context = "") const;
	virtual void erase_message(const StringName &p_src_text, const StringName &p_context = "");
	virtual void get_message_list(List<StringName> *r_messages) const;
	virtual int get_message_count() const;
	virtual Vector<String> get_translated_message_list() const;

	virtual void set_plural_rule(const String &p_plural_rule);
	virtual int get_plural_forms() const;
	virtual String get_plural_rule() const;

	Translation() {}
};

#endif // TRANSLATION_H
