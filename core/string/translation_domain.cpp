/**************************************************************************/
/*  translation_domain.cpp                                                */
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

#include "translation_domain.h"

#include "core/string/translation.h"
#include "core/string/translation_server.h"

struct _character_accent_pair {
	const char32_t character;
	const char32_t *accented_character;
};

static _character_accent_pair _character_to_accented[] = {
	{ 'A', U"Å" },
	{ 'B', U"ß" },
	{ 'C', U"Ç" },
	{ 'D', U"Ð" },
	{ 'E', U"É" },
	{ 'F', U"F́" },
	{ 'G', U"Ĝ" },
	{ 'H', U"Ĥ" },
	{ 'I', U"Ĩ" },
	{ 'J', U"Ĵ" },
	{ 'K', U"ĸ" },
	{ 'L', U"Ł" },
	{ 'M', U"Ḿ" },
	{ 'N', U"й" },
	{ 'O', U"Ö" },
	{ 'P', U"Ṕ" },
	{ 'Q', U"Q́" },
	{ 'R', U"Ř" },
	{ 'S', U"Ŝ" },
	{ 'T', U"Ŧ" },
	{ 'U', U"Ũ" },
	{ 'V', U"Ṽ" },
	{ 'W', U"Ŵ" },
	{ 'X', U"X́" },
	{ 'Y', U"Ÿ" },
	{ 'Z', U"Ž" },
	{ 'a', U"á" },
	{ 'b', U"ḅ" },
	{ 'c', U"ć" },
	{ 'd', U"d́" },
	{ 'e', U"é" },
	{ 'f', U"f́" },
	{ 'g', U"ǵ" },
	{ 'h', U"h̀" },
	{ 'i', U"í" },
	{ 'j', U"ǰ" },
	{ 'k', U"ḱ" },
	{ 'l', U"ł" },
	{ 'm', U"m̀" },
	{ 'n', U"ή" },
	{ 'o', U"ô" },
	{ 'p', U"ṕ" },
	{ 'q', U"q́" },
	{ 'r', U"ŕ" },
	{ 's', U"š" },
	{ 't', U"ŧ" },
	{ 'u', U"ü" },
	{ 'v', U"ṽ" },
	{ 'w', U"ŵ" },
	{ 'x', U"x́" },
	{ 'y', U"ý" },
	{ 'z', U"ź" },
};

String TranslationDomain::_get_override_string(const String &p_message) const {
	String res;
	for (int i = 0; i < p_message.length(); i++) {
		if (pseudolocalization.skip_placeholders_enabled && _is_placeholder(p_message, i)) {
			res += p_message[i];
			res += p_message[i + 1];
			i++;
			continue;
		}
		res += '*';
	}
	return res;
}

String TranslationDomain::_double_vowels(const String &p_message) const {
	String res;
	for (int i = 0; i < p_message.length(); i++) {
		if (pseudolocalization.skip_placeholders_enabled && _is_placeholder(p_message, i)) {
			res += p_message[i];
			res += p_message[i + 1];
			i++;
			continue;
		}
		res += p_message[i];
		if (p_message[i] == 'a' || p_message[i] == 'e' || p_message[i] == 'i' || p_message[i] == 'o' || p_message[i] == 'u' ||
				p_message[i] == 'A' || p_message[i] == 'E' || p_message[i] == 'I' || p_message[i] == 'O' || p_message[i] == 'U') {
			res += p_message[i];
		}
	}
	return res;
};

String TranslationDomain::_replace_with_accented_string(const String &p_message) const {
	String res;
	for (int i = 0; i < p_message.length(); i++) {
		if (pseudolocalization.skip_placeholders_enabled && _is_placeholder(p_message, i)) {
			res += p_message[i];
			res += p_message[i + 1];
			i++;
			continue;
		}
		const char32_t *accented = _get_accented_version(p_message[i]);
		if (accented) {
			res += accented;
		} else {
			res += p_message[i];
		}
	}
	return res;
}

String TranslationDomain::_wrap_with_fakebidi_characters(const String &p_message) const {
	String res;
	char32_t fakebidiprefix = U'\u202e';
	char32_t fakebidisuffix = U'\u202c';
	res += fakebidiprefix;
	// The fake bidi unicode gets popped at every newline so pushing it back at every newline.
	for (int i = 0; i < p_message.length(); i++) {
		if (p_message[i] == '\n') {
			res += fakebidisuffix;
			res += p_message[i];
			res += fakebidiprefix;
		} else if (pseudolocalization.skip_placeholders_enabled && _is_placeholder(p_message, i)) {
			res += fakebidisuffix;
			res += p_message[i];
			res += p_message[i + 1];
			res += fakebidiprefix;
			i++;
		} else {
			res += p_message[i];
		}
	}
	res += fakebidisuffix;
	return res;
}

String TranslationDomain::_add_padding(const String &p_message, int p_length) const {
	String underscores = String("_").repeat(p_length * pseudolocalization.expansion_ratio / 2);
	String prefix = pseudolocalization.prefix + underscores;
	String suffix = underscores + pseudolocalization.suffix;

	return prefix + p_message + suffix;
}

const char32_t *TranslationDomain::_get_accented_version(char32_t p_character) const {
	if (!is_ascii_alphabet_char(p_character)) {
		return nullptr;
	}

	for (unsigned int i = 0; i < sizeof(_character_to_accented) / sizeof(_character_to_accented[0]); i++) {
		if (_character_to_accented[i].character == p_character) {
			return _character_to_accented[i].accented_character;
		}
	}

	return nullptr;
}

bool TranslationDomain::_is_placeholder(const String &p_message, int p_index) const {
	return p_index < p_message.length() - 1 && p_message[p_index] == '%' &&
			(p_message[p_index + 1] == 's' || p_message[p_index + 1] == 'c' || p_message[p_index + 1] == 'd' ||
					p_message[p_index + 1] == 'o' || p_message[p_index + 1] == 'x' || p_message[p_index + 1] == 'X' || p_message[p_index + 1] == 'f');
}

StringName TranslationDomain::get_message_from_translations(const String &p_locale, const StringName &p_message, const StringName &p_context) const {
	StringName res;
	int best_score = 0;

	for (const Ref<Translation> &E : translations) {
		ERR_CONTINUE(E.is_null());
		int score = TranslationServer::get_singleton()->compare_locales(p_locale, E->get_locale());
		if (score > 0 && score >= best_score) {
			const StringName r = E->get_message(p_message, p_context);
			if (!r) {
				continue;
			}
			res = r;
			best_score = score;
			if (score == 10) {
				break; // Exact match, skip the rest.
			}
		}
	}

	return res;
}

StringName TranslationDomain::get_message_from_translations(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	StringName res;
	int best_score = 0;

	for (const Ref<Translation> &E : translations) {
		ERR_CONTINUE(E.is_null());
		int score = TranslationServer::get_singleton()->compare_locales(p_locale, E->get_locale());
		if (score > 0 && score >= best_score) {
			const StringName r = E->get_plural_message(p_message, p_message_plural, p_n, p_context);
			if (!r) {
				continue;
			}
			res = r;
			best_score = score;
			if (score == 10) {
				break; // Exact match, skip the rest.
			}
		}
	}

	return res;
}

PackedStringArray TranslationDomain::get_loaded_locales() const {
	PackedStringArray locales;
	for (const Ref<Translation> &E : translations) {
		ERR_CONTINUE(E.is_null());
		locales.push_back(E->get_locale());
	}
	return locales;
}

Ref<Translation> TranslationDomain::get_translation_object(const String &p_locale) const {
	Ref<Translation> res;
	int best_score = 0;

	for (const Ref<Translation> &E : translations) {
		ERR_CONTINUE(E.is_null());

		int score = TranslationServer::get_singleton()->compare_locales(p_locale, E->get_locale());
		if (score > 0 && score >= best_score) {
			res = E;
			best_score = score;
			if (score == 10) {
				break; // Exact match, skip the rest.
			}
		}
	}
	return res;
}

void TranslationDomain::add_translation(const Ref<Translation> &p_translation) {
	translations.insert(p_translation);
}

void TranslationDomain::remove_translation(const Ref<Translation> &p_translation) {
	translations.erase(p_translation);
}

void TranslationDomain::clear() {
	translations.clear();
}

StringName TranslationDomain::translate(const StringName &p_message, const StringName &p_context) const {
	const String &locale = TranslationServer::get_singleton()->get_locale();
	StringName res = get_message_from_translations(locale, p_message, p_context);

	const String &fallback = TranslationServer::get_singleton()->get_fallback_locale();
	if (!res && fallback.length() >= 2) {
		res = get_message_from_translations(fallback, p_message, p_context);
	}

	if (!res) {
		return pseudolocalization.enabled ? pseudolocalize(p_message) : p_message;
	}
	return pseudolocalization.enabled ? pseudolocalize(res) : res;
}

StringName TranslationDomain::translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	const String &locale = TranslationServer::get_singleton()->get_locale();
	StringName res = get_message_from_translations(locale, p_message, p_message_plural, p_n, p_context);

	const String &fallback = TranslationServer::get_singleton()->get_fallback_locale();
	if (!res && fallback.length() >= 2) {
		res = get_message_from_translations(fallback, p_message, p_message_plural, p_n, p_context);
	}

	if (!res) {
		if (p_n == 1) {
			return p_message;
		}
		return p_message_plural;
	}
	return res;
}

bool TranslationDomain::is_pseudolocalization_enabled() const {
	return pseudolocalization.enabled;
}

void TranslationDomain::set_pseudolocalization_enabled(bool p_enabled) {
	pseudolocalization.enabled = p_enabled;
}

bool TranslationDomain::is_pseudolocalization_accents_enabled() const {
	return pseudolocalization.accents_enabled;
}

void TranslationDomain::set_pseudolocalization_accents_enabled(bool p_enabled) {
	pseudolocalization.accents_enabled = p_enabled;
}

bool TranslationDomain::is_pseudolocalization_double_vowels_enabled() const {
	return pseudolocalization.double_vowels_enabled;
}

void TranslationDomain::set_pseudolocalization_double_vowels_enabled(bool p_enabled) {
	pseudolocalization.double_vowels_enabled = p_enabled;
}

bool TranslationDomain::is_pseudolocalization_fake_bidi_enabled() const {
	return pseudolocalization.fake_bidi_enabled;
}

void TranslationDomain::set_pseudolocalization_fake_bidi_enabled(bool p_enabled) {
	pseudolocalization.fake_bidi_enabled = p_enabled;
}

bool TranslationDomain::is_pseudolocalization_override_enabled() const {
	return pseudolocalization.override_enabled;
}

void TranslationDomain::set_pseudolocalization_override_enabled(bool p_enabled) {
	pseudolocalization.override_enabled = p_enabled;
}

bool TranslationDomain::is_pseudolocalization_skip_placeholders_enabled() const {
	return pseudolocalization.skip_placeholders_enabled;
}

void TranslationDomain::set_pseudolocalization_skip_placeholders_enabled(bool p_enabled) {
	pseudolocalization.skip_placeholders_enabled = p_enabled;
}

float TranslationDomain::get_pseudolocalization_expansion_ratio() const {
	return pseudolocalization.expansion_ratio;
}

void TranslationDomain::set_pseudolocalization_expansion_ratio(float p_ratio) {
	pseudolocalization.expansion_ratio = p_ratio;
}

String TranslationDomain::get_pseudolocalization_prefix() const {
	return pseudolocalization.prefix;
}

void TranslationDomain::set_pseudolocalization_prefix(const String &p_prefix) {
	pseudolocalization.prefix = p_prefix;
}

String TranslationDomain::get_pseudolocalization_suffix() const {
	return pseudolocalization.suffix;
}

void TranslationDomain::set_pseudolocalization_suffix(const String &p_suffix) {
	pseudolocalization.suffix = p_suffix;
}

StringName TranslationDomain::pseudolocalize(const StringName &p_message) const {
	if (p_message.is_empty()) {
		return p_message;
	}

	String message = p_message;
	int length = message.length();
	if (pseudolocalization.override_enabled) {
		message = _get_override_string(message);
	}

	if (pseudolocalization.double_vowels_enabled) {
		message = _double_vowels(message);
	}

	if (pseudolocalization.accents_enabled) {
		message = _replace_with_accented_string(message);
	}

	if (pseudolocalization.fake_bidi_enabled) {
		message = _wrap_with_fakebidi_characters(message);
	}

	return _add_padding(message, length);
}

void TranslationDomain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_translation_object", "locale"), &TranslationDomain::get_translation_object);
	ClassDB::bind_method(D_METHOD("add_translation", "translation"), &TranslationDomain::add_translation);
	ClassDB::bind_method(D_METHOD("remove_translation", "translation"), &TranslationDomain::remove_translation);
	ClassDB::bind_method(D_METHOD("clear"), &TranslationDomain::clear);
	ClassDB::bind_method(D_METHOD("translate", "message", "context"), &TranslationDomain::translate, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("translate_plural", "message", "message_plural", "n", "context"), &TranslationDomain::translate_plural, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("is_pseudolocalization_enabled"), &TranslationDomain::is_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_accents_enabled"), &TranslationDomain::is_pseudolocalization_accents_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_accents_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_accents_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_double_vowels_enabled"), &TranslationDomain::is_pseudolocalization_double_vowels_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_double_vowels_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_double_vowels_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_fake_bidi_enabled"), &TranslationDomain::is_pseudolocalization_fake_bidi_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_fake_bidi_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_fake_bidi_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_override_enabled"), &TranslationDomain::is_pseudolocalization_override_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_override_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_override_enabled);
	ClassDB::bind_method(D_METHOD("is_pseudolocalization_skip_placeholders_enabled"), &TranslationDomain::is_pseudolocalization_skip_placeholders_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_skip_placeholders_enabled", "enabled"), &TranslationDomain::set_pseudolocalization_skip_placeholders_enabled);
	ClassDB::bind_method(D_METHOD("get_pseudolocalization_expansion_ratio"), &TranslationDomain::get_pseudolocalization_expansion_ratio);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_expansion_ratio", "ratio"), &TranslationDomain::set_pseudolocalization_expansion_ratio);
	ClassDB::bind_method(D_METHOD("get_pseudolocalization_prefix"), &TranslationDomain::get_pseudolocalization_prefix);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_prefix", "prefix"), &TranslationDomain::set_pseudolocalization_prefix);
	ClassDB::bind_method(D_METHOD("get_pseudolocalization_suffix"), &TranslationDomain::get_pseudolocalization_suffix);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_suffix", "suffix"), &TranslationDomain::set_pseudolocalization_suffix);
	ClassDB::bind_method(D_METHOD("pseudolocalize", "message"), &TranslationDomain::pseudolocalize);

	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_enabled"), "set_pseudolocalization_enabled", "is_pseudolocalization_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_accents_enabled"), "set_pseudolocalization_accents_enabled", "is_pseudolocalization_accents_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_double_vowels_enabled"), "set_pseudolocalization_double_vowels_enabled", "is_pseudolocalization_double_vowels_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_fake_bidi_enabled"), "set_pseudolocalization_fake_bidi_enabled", "is_pseudolocalization_fake_bidi_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_override_enabled"), "set_pseudolocalization_override_enabled", "is_pseudolocalization_override_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_skip_placeholders_enabled"), "set_pseudolocalization_skip_placeholders_enabled", "is_pseudolocalization_skip_placeholders_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::Type::FLOAT, "pseudolocalization_expansion_ratio"), "set_pseudolocalization_expansion_ratio", "get_pseudolocalization_expansion_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::Type::STRING, "pseudolocalization_prefix"), "set_pseudolocalization_prefix", "get_pseudolocalization_prefix");
	ADD_PROPERTY(PropertyInfo(Variant::Type::STRING, "pseudolocalization_suffix"), "set_pseudolocalization_suffix", "get_pseudolocalization_suffix");
}

TranslationDomain::TranslationDomain() {
}
