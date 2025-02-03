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

#include "core/io/resource_loader.h"
#include "core/locales.h"
#include "core/os/os.h"
#include "core/project_settings.h"

PoolVector<String> Translation::_get_messages() const {
	PoolVector<String> msgs;
	msgs.resize(translation_map.size() * 2);
	int idx = 0;
	for (const Map<StringName, StringName>::Element *E = translation_map.front(); E; E = E->next()) {
		msgs.set(idx + 0, E->key());
		msgs.set(idx + 1, E->get());
		idx += 2;
	}

	return msgs;
}

PoolVector<String> Translation::_get_message_list() const {
	PoolVector<String> msgs;
	msgs.resize(translation_map.size());
	int idx = 0;
	for (const Map<StringName, StringName>::Element *E = translation_map.front(); E; E = E->next()) {
		msgs.set(idx, E->key());
		idx += 1;
	}

	return msgs;
}

void Translation::_set_messages(const PoolVector<String> &p_messages) {
	int msg_count = p_messages.size();
	ERR_FAIL_COND(msg_count % 2);

	PoolVector<String>::Read r = p_messages.read();

	for (int i = 0; i < msg_count; i += 2) {
		add_message(r[i + 0], r[i + 1]);
	}
}

void Translation::set_locale(const String &p_locale) {
	locale = TranslationServer::get_singleton()->standardize_locale(p_locale);

	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void Translation::add_context_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context.");
	}
	add_message(p_src_text, p_xlated_text);
}

StringName Translation::get_context_message(const StringName &p_src_text, const StringName &p_context) const {
	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context.");
	}
	return get_message(p_src_text);
}

void Translation::add_message(const StringName &p_src_text, const StringName &p_xlated_text) {
	translation_map[p_src_text] = p_xlated_text;
}

StringName Translation::get_message(const StringName &p_src_text) const {
	if (get_script_instance()) {
		return get_script_instance()->call("_get_message", p_src_text);
	}

	const Map<StringName, StringName>::Element *E = translation_map.find(p_src_text);
	if (!E) {
		return StringName();
	}

	return E->get();
}

void Translation::erase_message(const StringName &p_src_text) {
	translation_map.erase(p_src_text);
}

void Translation::get_message_list(List<StringName> *r_messages) const {
	for (const Map<StringName, StringName>::Element *E = translation_map.front(); E; E = E->next()) {
		r_messages->push_back(E->key());
	}
}

int Translation::get_message_count() const {
	return translation_map.size();
};

void Translation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &Translation::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &Translation::get_locale);
	ClassDB::bind_method(D_METHOD("add_message", "src_message", "xlated_message"), &Translation::add_message);
	ClassDB::bind_method(D_METHOD("get_message", "src_message"), &Translation::get_message);
	ClassDB::bind_method(D_METHOD("erase_message", "src_message"), &Translation::erase_message);
	ClassDB::bind_method(D_METHOD("get_message_list"), &Translation::_get_message_list);
	ClassDB::bind_method(D_METHOD("get_message_count"), &Translation::get_message_count);
	ClassDB::bind_method(D_METHOD("_set_messages"), &Translation::_set_messages);
	ClassDB::bind_method(D_METHOD("_get_messages"), &Translation::_get_messages);

	BIND_VMETHOD(MethodInfo(Variant::STRING, "_get_message", PropertyInfo(Variant::STRING, "src_message")));

	ADD_PROPERTY(PropertyInfo(Variant::POOL_STRING_ARRAY, "messages", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_messages", "_get_messages");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "locale"), "set_locale", "get_locale");
}

Translation::Translation() :
		locale("en") {
}

///////////////////////////////////////////////

void ContextTranslation::add_context_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
	if (p_context == StringName()) {
		add_message(p_src_text, p_xlated_text);
	} else {
		context_translation_map[p_context][p_src_text] = p_xlated_text;
	}
}

StringName ContextTranslation::get_context_message(const StringName &p_src_text, const StringName &p_context) const {
	if (p_context == StringName()) {
		return get_message(p_src_text);
	}

	const Map<StringName, Map<StringName, StringName>>::Element *context = context_translation_map.find(p_context);
	if (!context) {
		return StringName();
	}
	const Map<StringName, StringName>::Element *message = context->get().find(p_src_text);
	if (!message) {
		return StringName();
	}
	return message->get();
}

///////////////////////////////////////////////

static _FORCE_INLINE_ bool is_ascii_upper_case(char32_t c) {
	return (c >= 'A' && c <= 'Z');
}

static _FORCE_INLINE_ bool is_ascii_lower_case(char32_t c) {
	return (c >= 'a' && c <= 'z');
}

Vector<TranslationServer::LocaleScriptInfo> TranslationServer::locale_script_info;

Map<String, String> TranslationServer::language_map;
Map<String, String> TranslationServer::script_map;
Map<String, String> TranslationServer::locale_rename_map;
Map<String, String> TranslationServer::country_name_map;
Map<String, String> TranslationServer::variant_map;
Map<String, String> TranslationServer::country_rename_map;

void TranslationServer::init_locale_info() {
	// Init locale info.
	language_map.clear();
	int idx = 0;
	while (language_list[idx][0] != nullptr) {
		language_map[language_list[idx][0]] = String::utf8(language_list[idx][1]);
		idx++;
	}

	// Init locale-script map.
	locale_script_info.clear();
	idx = 0;
	while (locale_scripts[idx][0] != nullptr) {
		LocaleScriptInfo info;
		info.name = locale_scripts[idx][0];
		info.script = locale_scripts[idx][1];
		info.default_country = locale_scripts[idx][2];
		Vector<String> supported_countries = String(locale_scripts[idx][3]).split(",", false);
		for (int i = 0; i < supported_countries.size(); i++) {
			info.supported_countries.insert(supported_countries[i]);
		}
		locale_script_info.push_back(info);
		idx++;
	}

	// Init supported script list.
	script_map.clear();
	idx = 0;
	while (script_list[idx][0] != nullptr) {
		script_map[script_list[idx][1]] = String::utf8(script_list[idx][0]);
		idx++;
	}

	// Init regional variant map.
	variant_map.clear();
	idx = 0;
	while (locale_variants[idx][0] != nullptr) {
		variant_map[locale_variants[idx][0]] = locale_variants[idx][1];
		idx++;
	}

	// Init locale renames.
	locale_rename_map.clear();
	idx = 0;
	while (locale_renames[idx][0] != nullptr) {
		if (!String(locale_renames[idx][1]).empty()) {
			locale_rename_map[locale_renames[idx][0]] = locale_renames[idx][1];
		}
		idx++;
	}

	// Init country names.
	country_name_map.clear();
	idx = 0;
	while (country_names[idx][0] != nullptr) {
		country_name_map[String(country_names[idx][0])] = String::utf8(country_names[idx][1]);
		idx++;
	}

	// Init country renames.
	country_rename_map.clear();
	idx = 0;
	while (country_renames[idx][0] != nullptr) {
		if (!String(country_renames[idx][1]).empty()) {
			country_rename_map[country_renames[idx][0]] = country_renames[idx][1];
		}
		idx++;
	}
}

String TranslationServer::standardize_locale(const String &p_locale) const {
	return _standardize_locale(p_locale, false);
}

String TranslationServer::_standardize_locale(const String &p_locale, bool p_add_defaults) const {
	// Replaces '-' with '_' for macOS style locales.
	String univ_locale = p_locale.replace("-", "_");

	// Extract locale elements.
	String lang, script, country, variant;
	Vector<String> locale_elements = univ_locale.get_slice("@", 0).split("_");
	lang = locale_elements[0];
	if (locale_elements.size() >= 2) {
		if (locale_elements[1].length() == 4 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_lower_case(locale_elements[1][1]) && is_ascii_lower_case(locale_elements[1][2]) && is_ascii_lower_case(locale_elements[1][3])) {
			script = locale_elements[1];
		}
		if (locale_elements[1].length() == 2 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_upper_case(locale_elements[1][1])) {
			country = locale_elements[1];
		}
	}
	if (locale_elements.size() >= 3) {
		if (locale_elements[2].length() == 2 && is_ascii_upper_case(locale_elements[2][0]) && is_ascii_upper_case(locale_elements[2][1])) {
			country = locale_elements[2];
		} else if (variant_map.has(locale_elements[2].to_lower()) && variant_map[locale_elements[2].to_lower()] == lang) {
			variant = locale_elements[2].to_lower();
		}
	}
	if (locale_elements.size() >= 4) {
		if (variant_map.has(locale_elements[3].to_lower()) && variant_map[locale_elements[3].to_lower()] == lang) {
			variant = locale_elements[3].to_lower();
		}
	}

	// Try extract script and variant from the extra part.
	Vector<String> script_extra = univ_locale.get_slice("@", 1).split(";");
	for (int i = 0; i < script_extra.size(); i++) {
		if (script_extra[i].to_lower() == "cyrillic") {
			script = "Cyrl";
			break;
		} else if (script_extra[i].to_lower() == "latin") {
			script = "Latn";
			break;
		} else if (script_extra[i].to_lower() == "devanagari") {
			script = "Deva";
			break;
		} else if (variant_map.has(script_extra[i].to_lower()) && variant_map[script_extra[i].to_lower()] == lang) {
			variant = script_extra[i].to_lower();
		}
	}

	// Handles known non-ISO language names used e.g. on Windows.
	if (locale_rename_map.has(lang)) {
		lang = locale_rename_map[lang];
	}

	// Handle country renames.
	if (country_rename_map.has(country)) {
		country = country_rename_map[country];
	}

	// Remove unsupported script codes.
	if (!script_map.has(script)) {
		script = "";
	}

	// Add script code base on language and country codes for some ambiguous cases.
	if (p_add_defaults) {
		if (script.empty()) {
			for (int i = 0; i < locale_script_info.size(); i++) {
				const LocaleScriptInfo &info = locale_script_info[i];
				if (info.name == lang) {
					if (country.empty() || info.supported_countries.has(country)) {
						script = info.script;
						break;
					}
				}
			}
		}
		if (!script.empty() && country.empty()) {
			// Add conntry code based on script for some ambiguous cases.
			for (int i = 0; i < locale_script_info.size(); i++) {
				const LocaleScriptInfo &info = locale_script_info[i];
				if (info.name == lang && info.script == script) {
					country = info.default_country;
					break;
				}
			}
		}
	}

	// Combine results.
	String locale = lang;
	if (!script.empty()) {
		locale = locale + "_" + script;
	}
	if (!country.empty()) {
		locale = locale + "_" + country;
	}
	if (!variant.empty()) {
		locale = locale + "_" + variant;
	}
	return locale;
}

int TranslationServer::compare_locales(const String &p_locale_a, const String &p_locale_b) const {
	if (p_locale_a == p_locale_b) {
		// Exact match.
		return 10;
	}

	const String cache_key = p_locale_a + "|" + p_locale_b;
	const int *cached_result = locale_compare_cache.getptr(cache_key);
	if (cached_result) {
		return *cached_result;
	}

	String locale_a = _standardize_locale(p_locale_a, true);
	String locale_b = _standardize_locale(p_locale_b, true);

	if (locale_a == locale_b) {
		// Exact match.
		locale_compare_cache.set(cache_key, 10);
		return 10;
	}

	Vector<String> locale_a_elements = locale_a.split("_");
	Vector<String> locale_b_elements = locale_b.split("_");
	if (locale_a_elements[0] != locale_b_elements[0]) {
		// No match.
		locale_compare_cache.set(cache_key, 0);
		return 0;
	}

	// Matching language, both locales have extra parts.
	// Return number of matching elements.
	int matching_elements = 1;
	for (int i = 1; i < locale_a_elements.size(); i++) {
		for (int j = 1; j < locale_b_elements.size(); j++) {
			if (locale_a_elements[i] == locale_b_elements[j]) {
				matching_elements++;
			}
		}
	}
	locale_compare_cache.set(cache_key, matching_elements);
	return matching_elements;
}

String TranslationServer::get_locale_name(const String &p_locale) const {
	String locale = standardize_locale(p_locale);

	String lang, script, country;
	Vector<String> locale_elements = locale.split("_");
	lang = locale_elements[0];
	if (locale_elements.size() >= 2) {
		if (locale_elements[1].length() == 4 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_lower_case(locale_elements[1][1]) && is_ascii_lower_case(locale_elements[1][2]) && is_ascii_lower_case(locale_elements[1][3])) {
			script = locale_elements[1];
		}
		if (locale_elements[1].length() == 2 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_upper_case(locale_elements[1][1])) {
			country = locale_elements[1];
		}
	}
	if (locale_elements.size() >= 3) {
		if (locale_elements[2].length() == 2 && is_ascii_upper_case(locale_elements[2][0]) && is_ascii_upper_case(locale_elements[2][1])) {
			country = locale_elements[2];
		}
	}

	String name = language_map[lang];
	if (!script.empty()) {
		name = name + " (" + script_map[script] + ")";
	}
	if (!country.empty()) {
		name = name + ", " + country_name_map[country];
	}
	return name;
}

Vector<String> TranslationServer::get_all_languages() const {
	Vector<String> languages;

	for (Map<String, String>::Element *E = language_map.front(); E; E = E->next()) {
		languages.push_back(E->key());
	}

	return languages;
}

String TranslationServer::get_language_name(const String &p_language) const {
	return language_map[p_language];
}

Vector<String> TranslationServer::get_all_scripts() const {
	Vector<String> scripts;

	for (Map<String, String>::Element *E = script_map.front(); E; E = E->next()) {
		scripts.push_back(E->key());
	}

	return scripts;
}

String TranslationServer::get_script_name(const String &p_script) const {
	return script_map[p_script];
}

Vector<String> TranslationServer::get_all_countries() const {
	Vector<String> countries;

	for (Map<String, String>::Element *E = country_name_map.front(); E; E = E->next()) {
		countries.push_back(E->key());
	}

	return countries;
}

String TranslationServer::get_country_name(const String &p_country) const {
	return country_name_map[p_country];
}

void TranslationServer::set_locale(const String &p_locale) {
	locale = standardize_locale(p_locale);

	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}

	ResourceLoader::reload_translation_remaps();
}

String TranslationServer::get_locale() const {
	return locale;
}

Array TranslationServer::get_loaded_locales() const {
	Array locales;
	for (Set<Ref<Translation>>::Element *E = translations.front(); E; E = E->next()) {
		const Ref<Translation> &t = E->get();
		ERR_FAIL_COND_V(t.is_null(), Array());
		String l = t->get_locale();

		locales.push_back(l);
	}

	return locales;
}

void TranslationServer::add_translation(const Ref<Translation> &p_translation) {
	translations.insert(p_translation);
}
void TranslationServer::remove_translation(const Ref<Translation> &p_translation) {
	translations.erase(p_translation);
}

void TranslationServer::clear() {
	translations.clear();
};

StringName TranslationServer::translate(const StringName &p_message) const {
	// Match given message against the translation catalog for the project locale.

	if (!enabled) {
		return p_message;
	}

	StringName res;
	int best_score = 0;

	for (const Set<Ref<Translation>>::Element *E = translations.front(); E; E = E->next()) {
		const Ref<Translation> &t = E->get();
		ERR_FAIL_COND_V(t.is_null(), p_message);
		String l = t->get_locale();

		int score = compare_locales(locale, l);
		if (score > 0 && score >= best_score) {
			StringName r = t->get_message(p_message);
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

	if (!res && fallback.length() >= 2) {
		best_score = 0;

		for (const Set<Ref<Translation>>::Element *E = translations.front(); E; E = E->next()) {
			const Ref<Translation> &t = E->get();
			ERR_FAIL_COND_V(t.is_null(), p_message);
			String l = t->get_locale();

			int score = compare_locales(fallback, l);
			if (score > 0 && score >= best_score) {
				StringName r = t->get_message(p_message);
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
	}

	if (!res) {
		return p_message;
	}

	return res == StringName("<EMPTY STRING>") ? StringName() : res;
}

TranslationServer *TranslationServer::singleton = nullptr;

bool TranslationServer::_load_translations(const String &p_from) {
	if (ProjectSettings::get_singleton()->has_setting(p_from)) {
		PoolVector<String> translations = ProjectSettings::get_singleton()->get(p_from);

		int tcount = translations.size();

		if (tcount) {
			PoolVector<String>::Read r = translations.read();

			for (int i = 0; i < tcount; i++) {
				Ref<Translation> tr = ResourceLoader::load(r[i]);
				if (tr.is_valid()) {
					add_translation(tr);
				}
			}
		}
		return true;
	}

	return false;
}

void TranslationServer::setup() {
	String test = GLOBAL_DEF("locale/test", "");
	test = test.strip_edges();
	if (test != "") {
		set_locale(test);
	} else {
		set_locale(OS::get_singleton()->get_locale());
	}
	fallback = GLOBAL_DEF("locale/fallback", "en");
#ifdef TOOLS_ENABLED
	ProjectSettings::get_singleton()->set_custom_property_info("locale/fallback", PropertyInfo(Variant::STRING, "locale/fallback", PROPERTY_HINT_LOCALE_ID, ""));
#endif
}

void TranslationServer::set_tool_translation(const Ref<Translation> &p_translation) {
	tool_translation = p_translation;
}

StringName TranslationServer::tool_translate(const StringName &p_message, const StringName &p_context) const {
	if (tool_translation.is_valid()) {
		StringName r = tool_translation->get_context_message(p_message, p_context);
		if (r) {
			return r;
		}
	}
	return p_message;
}

void TranslationServer::set_doc_translation(const Ref<Translation> &p_translation) {
	doc_translation = p_translation;
}

StringName TranslationServer::doc_translate(const StringName &p_message) const {
	if (doc_translation.is_valid()) {
		StringName r = doc_translation->get_message(p_message);
		if (r) {
			return r;
		}
	}
	return p_message;
}

void TranslationServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &TranslationServer::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &TranslationServer::get_locale);

	ClassDB::bind_method(D_METHOD("compare_locales", "locale_a", "locale_b"), &TranslationServer::compare_locales);
	ClassDB::bind_method(D_METHOD("standardize_locale", "locale"), &TranslationServer::standardize_locale);

	ClassDB::bind_method(D_METHOD("get_all_languages"), &TranslationServer::get_all_languages);
	ClassDB::bind_method(D_METHOD("get_language_name", "language"), &TranslationServer::get_language_name);

	ClassDB::bind_method(D_METHOD("get_all_scripts"), &TranslationServer::get_all_scripts);
	ClassDB::bind_method(D_METHOD("get_script_name", "script"), &TranslationServer::get_script_name);

	ClassDB::bind_method(D_METHOD("get_all_countries"), &TranslationServer::get_all_countries);
	ClassDB::bind_method(D_METHOD("get_country_name", "country"), &TranslationServer::get_country_name);

	ClassDB::bind_method(D_METHOD("get_locale_name", "locale"), &TranslationServer::get_locale_name);

	ClassDB::bind_method(D_METHOD("translate", "message"), &TranslationServer::translate);

	ClassDB::bind_method(D_METHOD("add_translation", "translation"), &TranslationServer::add_translation);
	ClassDB::bind_method(D_METHOD("remove_translation", "translation"), &TranslationServer::remove_translation);

	ClassDB::bind_method(D_METHOD("clear"), &TranslationServer::clear);

	ClassDB::bind_method(D_METHOD("get_loaded_locales"), &TranslationServer::get_loaded_locales);
}

void TranslationServer::load_translations() {
	String locale = get_locale();
	_load_translations("locale/translations"); //all
	_load_translations("locale/translations_" + locale.substr(0, 2));

	if (locale.substr(0, 2) != locale) {
		_load_translations("locale/translations_" + locale);
	}
}

TranslationServer::TranslationServer() :
		locale("en"),
		enabled(true) {
	singleton = this;

	init_locale_info();
}
