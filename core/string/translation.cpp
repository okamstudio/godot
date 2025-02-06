/**************************************************************************/
/*  translation.cpp                                                       */
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

#include "translation.h"

#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/string/translation_server.h"

int Translation::_get_plural_index(int p_n) const {
	// Get a number between [0;number of plural forms).

	input_val.clear();
	input_val.push_back(p_n);

	return _eq_test(equi_tests, 0);
}

int Translation::_eq_test(const Ref<EQNode> &p_node, const Variant &p_result) const {
	if (p_node.is_valid()) {
		Error err = expr->parse(p_node->regex, input_name);
		ERR_FAIL_COND_V_MSG(err != OK, 0, vformat("Cannot parse expression \"%s\". Error: %s", p_node->regex, expr->get_error_text()));

		Variant result = expr->execute(input_val);
		ERR_FAIL_COND_V_MSG(expr->has_execute_failed(), 0, vformat("Cannot evaluate expression \"%s\".", p_node->regex));

		if (bool(result)) {
			return _eq_test(p_node->left, result);
		} else {
			return _eq_test(p_node->right, result);
		}
	} else {
		return p_result;
	}
}

int Translation::_find_unquoted(const String &p_src, char32_t p_chr) const {
	const int len = p_src.length();
	if (len == 0) {
		return -1;
	}

	const char32_t *src = p_src.get_data();
	bool in_quote = false;
	for (int i = 0; i < len; i++) {
		if (in_quote) {
			if (src[i] == ')') {
				in_quote = false;
			}
		} else {
			if (src[i] == '(') {
				in_quote = true;
			} else if (src[i] == p_chr) {
				return i;
			}
		}
	}

	return -1;
}

void Translation::_cache_plural_tests(const String &p_plural_rule, Ref<EQNode> &p_node) {
	// Some examples of p_plural_rule passed in can have the form:
	// "n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 && n%100<=99 ? 4 : 5" (Arabic)
	// "n >= 2" (French) // When evaluating the last, especially careful with this one.
	// "n != 1" (English)

	String rule = p_plural_rule;
	if (rule.begins_with("(") && rule.ends_with(")")) {
		int bcount = 0;
		for (int i = 1; i < rule.length() - 1 && bcount >= 0; i++) {
			if (rule[i] == '(') {
				bcount++;
			} else if (rule[i] == ')') {
				bcount--;
			}
		}
		if (bcount == 0) {
			rule = rule.substr(1, rule.length() - 2);
		}
	}

	int first_ques_mark = _find_unquoted(rule, '?');
	int first_colon = _find_unquoted(rule, ':');

	if (first_ques_mark == -1) {
		p_node->regex = rule.strip_edges();
		return;
	}

	p_node->regex = rule.substr(0, first_ques_mark).strip_edges();

	p_node->left.instantiate();
	_cache_plural_tests(rule.substr(first_ques_mark + 1, first_colon - first_ques_mark - 1).strip_edges(), p_node->left);
	p_node->right.instantiate();
	_cache_plural_tests(rule.substr(first_colon + 1).strip_edges(), p_node->right);
}

void Translation::set_plural_rule(const String &p_plural_rule) {
	// Set plural_forms and plural_rule.
	// p_plural_rule passed in has the form "Plural-Forms: nplurals=2; plural=(n >= 2);".

	int first_semi_col = p_plural_rule.find_char(';');
	plural_forms = p_plural_rule.substr(p_plural_rule.find_char('=') + 1, first_semi_col - (p_plural_rule.find_char('=') + 1)).to_int();

	int expression_start = p_plural_rule.find_char('=', first_semi_col) + 1;
	int second_semi_col = p_plural_rule.rfind_char(';');
	plural_rule = p_plural_rule.substr(expression_start, second_semi_col - expression_start).strip_edges();

	// Setup the cache to make evaluating plural rule faster later on.
	equi_tests.instantiate();
	_cache_plural_tests(plural_rule, equi_tests);

	expr.instantiate();
	input_name.push_back("n");
}

int Translation::get_plural_forms() const {
	return plural_forms;
}

String Translation::get_plural_rule() const {
	return plural_rule;
}

Dictionary Translation::_get_messages() const {
	Dictionary d;
	for (const KeyValue<StringName, Vector<String>> &E : translation_map) {
		d[E.key] = E.value;
	}
	return d;
}

void Translation::_set_messages(const Dictionary &p_messages) {
	List<Variant> keys;
	p_messages.get_key_list(&keys);
	for (const Variant &E : keys) {
		if (p_messages[E].get_type() == Variant::STRING || p_messages[E].get_type() == Variant::STRING_NAME) {
			PackedStringArray arr = { p_messages[E].operator String() };
			translation_map[E] = arr;
		} else if (p_messages[E].get_type() == Variant::PACKED_STRING_ARRAY) {
			translation_map[E] = p_messages[E];
		}
	}
}

Vector<String> Translation::_get_message_list() const {
	Vector<String> msgs;
	msgs.resize(translation_map.size());
	int idx = 0;
	for (const KeyValue<StringName, Vector<String>> &E : translation_map) {
		msgs.set(idx, E.key);
		idx += 1;
	}

	return msgs;
}

Vector<String> Translation::get_translated_message_list() const {
	Vector<String> msgs;
	for (const KeyValue<StringName, Vector<String>> &E : translation_map) {
		for (const String &F : E.value) {
			msgs.push_back(F);
		}
	}

	return msgs;
}

void Translation::set_locale(const String &p_locale) {
	locale = TranslationServer::get_singleton()->standardize_locale(p_locale);

	if (Thread::is_main_thread()) {
		_notify_translation_changed_if_applies();
	} else {
		// This has to happen on the main thread (bypassing the ResourceLoader per-thread call queue)
		// because it interacts with the generally non-thread-safe window management, leading to
		// different issues across platforms otherwise.
		MessageQueue::get_main_singleton()->push_callable(callable_mp(this, &Translation::_notify_translation_changed_if_applies));
	}
}

void Translation::_notify_translation_changed_if_applies() {
	if (OS::get_singleton()->get_main_loop() && TranslationServer::get_singleton()->get_loaded_locales().has(get_locale())) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void Translation::add_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
	Vector<String> arr = { p_xlated_text };
	translation_map[p_src_text] = arr;
}

void Translation::add_plural_message(const StringName &p_src_text, const Vector<String> &p_plural_xlated_texts, const StringName &p_context) {
	ERR_FAIL_COND_MSG(p_plural_xlated_texts.is_empty(), "Parameter vector p_plural_xlated_texts passed in is empty.");
	translation_map[p_src_text] = p_plural_xlated_texts;
}

StringName Translation::get_message(const StringName &p_src_text, const StringName &p_context) const {
	StringName ret;
	if (GDVIRTUAL_CALL(_get_message, p_src_text, p_context, ret)) {
		return ret;
	}

	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in get_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	HashMap<StringName, Vector<String>>::ConstIterator E = translation_map.find(p_src_text);
	if (!E) {
		return StringName();
	}

	return E->value[0];
}

StringName Translation::get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context) const {
	ERR_FAIL_COND_V_MSG(p_n < 0, StringName(), "N passed into translation to get a plural message should not be negative. For negative numbers, use singular translation please. Search \"gettext PO Plural Forms\" online for the documentation on translating negative numbers.");

	StringName ret;
	if (GDVIRTUAL_CALL(_get_plural_message, p_src_text, p_plural_text, p_n, p_context, ret)) {
		return ret;
	}

	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in get_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	// If the query is the same as last time, return the cached result.
	if (p_n == last_plural_n && p_src_text == last_plural_key) {
		return translation_map[p_src_text][last_plural_mapped_index];
	}

	HashMap<StringName, Vector<String>>::ConstIterator E = translation_map.find(p_src_text);
	if (!E) {
		return StringName();
	}
	int plural_index = _get_plural_index(p_n);
	ERR_FAIL_COND_V_MSG(plural_index < 0 || E->value.size() < plural_index, StringName(), "Plural index returned or number of plural translations is not valid. Please report this bug.");

	// Cache result so that if the next entry is the same, we can return directly.
	// _get_plural_index(p_n) can get very costly, especially when evaluating long plural-rule (Arabic)
	last_plural_key = p_src_text;
	last_plural_n = p_n;
	last_plural_mapped_index = plural_index;

	return E->value[plural_index];
}

Vector<String> Translation::get_plural_messages(const StringName &p_src_text, const StringName &p_context) const {
	Vector<String> ret;
	if (GDVIRTUAL_CALL(_get_plural_messages, p_src_text, p_context, ret)) {
		return ret;
	}

	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in get_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	HashMap<StringName, Vector<String>>::ConstIterator E = translation_map.find(p_src_text);
	if (!E) {
		return Vector<String>();
	}

	return E->value;
}

void Translation::erase_message(const StringName &p_src_text, const StringName &p_context) {
	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in erase_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	if (translation_map.has(p_src_text)) {
		translation_map.erase(p_src_text);
	}
}

void Translation::get_message_list(List<StringName> *r_messages) const {
	for (const KeyValue<StringName, Vector<String>> &E : translation_map) {
		r_messages->push_back(E.key);
	}
}

int Translation::get_message_count() const {
	return translation_map.size();
}

void Translation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &Translation::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &Translation::get_locale);
	ClassDB::bind_method(D_METHOD("add_message", "src_message", "xlated_message", "context"), &Translation::add_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("add_plural_message", "src_message", "xlated_messages", "context"), &Translation::add_plural_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_message", "src_message", "context"), &Translation::get_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_message", "src_message", "src_plural_message", "n", "context"), &Translation::get_plural_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_messages", "src_message", "context"), &Translation::get_plural_messages, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("erase_message", "src_message", "context"), &Translation::erase_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_message_list"), &Translation::_get_message_list);
	ClassDB::bind_method(D_METHOD("get_translated_message_list"), &Translation::get_translated_message_list);
	ClassDB::bind_method(D_METHOD("get_message_count"), &Translation::get_message_count);
	ClassDB::bind_method(D_METHOD("_set_messages", "messages"), &Translation::_set_messages);
	ClassDB::bind_method(D_METHOD("_get_messages"), &Translation::_get_messages);
	ClassDB::bind_method(D_METHOD("get_plural_forms"), &Translation::get_plural_forms);
	ClassDB::bind_method(D_METHOD("set_plural_rule", "rule"), &Translation::set_plural_rule);
	ClassDB::bind_method(D_METHOD("get_plural_rule"), &Translation::get_plural_rule);

	GDVIRTUAL_BIND(_get_plural_messages, "src_message", "context");
	GDVIRTUAL_BIND(_get_plural_message, "src_message", "src_plural_message", "n", "context");
	GDVIRTUAL_BIND(_get_message, "src_message", "context");

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "messages", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_messages", "_get_messages");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "locale"), "set_locale", "get_locale");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "plural_rule"), "set_plural_rule", "get_plural_rule");
}
