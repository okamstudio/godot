/**************************************************************************/
/*  translation_server.cpp                                                */
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

#include "translation_server.h"
#include "translation_server.compat.inc"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "core/string/locales.h"

#ifdef TOOLS_ENABLED
#include "main/main.h"
#endif

Vector<TranslationServer::LocaleScriptInfo> TranslationServer::locale_script_info;

HashMap<String, String> TranslationServer::language_map;
HashMap<String, String> TranslationServer::script_map;
HashMap<String, String> TranslationServer::locale_rename_map;
HashMap<String, String> TranslationServer::country_name_map;
HashMap<String, String> TranslationServer::variant_map;
HashMap<String, String> TranslationServer::country_rename_map;

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
		if (!String(locale_renames[idx][1]).is_empty()) {
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
		if (!String(country_renames[idx][1]).is_empty()) {
			country_rename_map[country_renames[idx][0]] = country_renames[idx][1];
		}
		idx++;
	}
}

TranslationServer::Locale::operator String() const {
	String out = language;
	if (!script.is_empty()) {
		out = out + "_" + script;
	}
	if (!country.is_empty()) {
		out = out + "_" + country;
	}
	if (!variant.is_empty()) {
		out = out + "_" + variant;
	}
	return out;
}

TranslationServer::Locale::Locale(const TranslationServer &p_server, const String &p_locale, bool p_add_defaults) {
	// Replaces '-' with '_' for macOS style locales.
	String univ_locale = p_locale.replace("-", "_");

	// Extract locale elements.
	Vector<String> locale_elements = univ_locale.get_slice("@", 0).split("_");
	language = locale_elements[0];
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
		} else if (p_server.variant_map.has(locale_elements[2].to_lower()) && p_server.variant_map[locale_elements[2].to_lower()] == language) {
			variant = locale_elements[2].to_lower();
		}
	}
	if (locale_elements.size() >= 4) {
		if (p_server.variant_map.has(locale_elements[3].to_lower()) && p_server.variant_map[locale_elements[3].to_lower()] == language) {
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
		} else if (p_server.variant_map.has(script_extra[i].to_lower()) && p_server.variant_map[script_extra[i].to_lower()] == language) {
			variant = script_extra[i].to_lower();
		}
	}

	// Handles known non-ISO language names used e.g. on Windows.
	if (p_server.locale_rename_map.has(language)) {
		language = p_server.locale_rename_map[language];
	}

	// Handle country renames.
	if (p_server.country_rename_map.has(country)) {
		country = p_server.country_rename_map[country];
	}

	// Remove unsupported script codes.
	if (!p_server.script_map.has(script)) {
		script = "";
	}

	// Add script code base on language and country codes for some ambiguous cases.
	if (p_add_defaults) {
		if (script.is_empty()) {
			for (int i = 0; i < p_server.locale_script_info.size(); i++) {
				const LocaleScriptInfo &info = p_server.locale_script_info[i];
				if (info.name == language) {
					if (country.is_empty() || info.supported_countries.has(country)) {
						script = info.script;
						break;
					}
				}
			}
		}
		if (!script.is_empty() && country.is_empty()) {
			// Add conntry code based on script for some ambiguous cases.
			for (int i = 0; i < p_server.locale_script_info.size(); i++) {
				const LocaleScriptInfo &info = p_server.locale_script_info[i];
				if (info.name == language && info.script == script) {
					country = info.default_country;
					break;
				}
			}
		}
	}
}

String TranslationServer::standardize_locale(const String &p_locale, bool p_add_defaults) const {
	return Locale(*this, p_locale, p_add_defaults).operator String();
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

	Locale locale_a = Locale(*this, p_locale_a, true);
	Locale locale_b = Locale(*this, p_locale_b, true);

	if (locale_a == locale_b) {
		// Exact match.
		locale_compare_cache.insert(cache_key, 10);
		return 10;
	}

	if (locale_a.language != locale_b.language) {
		// No match.
		locale_compare_cache.insert(cache_key, 0);
		return 0;
	}

	// Matching language, both locales have extra parts. Compare the
	// remaining elements. If both elements are non-empty, check the
	// match to increase or decrease the score. If either element or
	// both are empty, leave the score as is.
	int score = 5;
	if (!locale_a.script.is_empty() && !locale_b.script.is_empty()) {
		if (locale_a.script == locale_b.script) {
			score++;
		} else {
			score--;
		}
	}
	if (!locale_a.country.is_empty() && !locale_b.country.is_empty()) {
		if (locale_a.country == locale_b.country) {
			score++;
		} else {
			score--;
		}
	}
	if (!locale_a.variant.is_empty() && !locale_b.variant.is_empty()) {
		if (locale_a.variant == locale_b.variant) {
			score++;
		} else {
			score--;
		}
	}

	locale_compare_cache.insert(cache_key, score);
	return score;
}

String TranslationServer::get_locale_name(const String &p_locale) const {
	String lang_name, script_name, country_name;
	Vector<String> locale_elements = standardize_locale(p_locale).split("_");
	lang_name = locale_elements[0];
	if (locale_elements.size() >= 2) {
		if (locale_elements[1].length() == 4 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_lower_case(locale_elements[1][1]) && is_ascii_lower_case(locale_elements[1][2]) && is_ascii_lower_case(locale_elements[1][3])) {
			script_name = locale_elements[1];
		}
		if (locale_elements[1].length() == 2 && is_ascii_upper_case(locale_elements[1][0]) && is_ascii_upper_case(locale_elements[1][1])) {
			country_name = locale_elements[1];
		}
	}
	if (locale_elements.size() >= 3) {
		if (locale_elements[2].length() == 2 && is_ascii_upper_case(locale_elements[2][0]) && is_ascii_upper_case(locale_elements[2][1])) {
			country_name = locale_elements[2];
		}
	}

	String name = language_map[lang_name];
	if (!script_name.is_empty()) {
		name = name + " (" + script_map[script_name] + ")";
	}
	if (!country_name.is_empty()) {
		name = name + ", " + country_name_map[country_name];
	}
	return name;
}

Vector<String> TranslationServer::get_all_languages() const {
	Vector<String> languages;

	for (const KeyValue<String, String> &E : language_map) {
		languages.push_back(E.key);
	}

	return languages;
}

String TranslationServer::get_language_name(const String &p_language) const {
	return language_map[p_language];
}

Vector<String> TranslationServer::get_all_scripts() const {
	Vector<String> scripts;

	for (const KeyValue<String, String> &E : script_map) {
		scripts.push_back(E.key);
	}

	return scripts;
}

String TranslationServer::get_script_name(const String &p_script) const {
	return script_map[p_script];
}

Vector<String> TranslationServer::get_all_countries() const {
	Vector<String> countries;

	for (const KeyValue<String, String> &E : country_name_map) {
		countries.push_back(E.key);
	}

	return countries;
}

String TranslationServer::get_country_name(const String &p_country) const {
	return country_name_map[p_country];
}

void TranslationServer::set_locale(const String &p_locale) {
	String new_locale = standardize_locale(p_locale);
	if (locale == new_locale) {
		return;
	}

	locale = new_locale;
	ResourceLoader::reload_translation_remaps();

	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

String TranslationServer::get_locale() const {
	return locale;
}

String TranslationServer::get_fallback_locale() const {
	return fallback;
}

PackedStringArray TranslationServer::get_loaded_locales() const {
	return main_domain->get_loaded_locales();
}

void TranslationServer::add_translation(const Ref<Translation> &p_translation) {
	main_domain->add_translation(p_translation);
}

void TranslationServer::remove_translation(const Ref<Translation> &p_translation) {
	main_domain->remove_translation(p_translation);
}

Ref<Translation> TranslationServer::get_translation_object(const String &p_locale) {
	return main_domain->get_translation_object(p_locale);
}

void TranslationServer::clear() {
	main_domain->clear();
}

StringName TranslationServer::translate(const StringName &p_message, const StringName &p_context) const {
	if (!enabled) {
		return p_message;
	}

	return main_domain->translate(p_message, p_context);
}

StringName TranslationServer::translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	if (!enabled) {
		if (p_n == 1) {
			return p_message;
		}
		return p_message_plural;
	}

	return main_domain->translate_plural(p_message, p_message_plural, p_n, p_context);
}

bool TranslationServer::_load_translations(const String &p_from) {
	if (ProjectSettings::get_singleton()->has_setting(p_from)) {
		const Vector<String> &translation_names = GLOBAL_GET(p_from);

		int tcount = translation_names.size();

		if (tcount) {
			const String *r = translation_names.ptr();

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

bool TranslationServer::has_domain(const StringName &p_domain) const {
	if (p_domain == StringName()) {
		return true;
	}
	return custom_domains.has(p_domain);
}

Ref<TranslationDomain> TranslationServer::get_or_add_domain(const StringName &p_domain) {
	if (p_domain == StringName()) {
		return main_domain;
	}
	const Ref<TranslationDomain> *domain = custom_domains.getptr(p_domain);
	if (domain) {
		if (domain->is_valid()) {
			return *domain;
		}
		ERR_PRINT("Bug (please report): Found invalid translation domain.");
	}
	Ref<TranslationDomain> new_domain = memnew(TranslationDomain);
	custom_domains[p_domain] = new_domain;
	return new_domain;
}

void TranslationServer::remove_domain(const StringName &p_domain) {
	ERR_FAIL_COND_MSG(p_domain == StringName(), "Cannot remove main translation domain.");
	custom_domains.erase(p_domain);
}

void TranslationServer::setup() {
	String test = GLOBAL_DEF("internationalization/locale/test", "");
	test = test.strip_edges();
	if (!test.is_empty()) {
		set_locale(test);
	} else {
		set_locale(OS::get_singleton()->get_locale());
	}

	fallback = GLOBAL_DEF("internationalization/locale/fallback", "en");
	main_domain->set_pseudolocalization_enabled(GLOBAL_DEF("internationalization/pseudolocalization/use_pseudolocalization", false));
	main_domain->set_pseudolocalization_accents_enabled(GLOBAL_DEF("internationalization/pseudolocalization/replace_with_accents", true));
	main_domain->set_pseudolocalization_double_vowels_enabled(GLOBAL_DEF("internationalization/pseudolocalization/double_vowels", false));
	main_domain->set_pseudolocalization_fake_bidi_enabled(GLOBAL_DEF("internationalization/pseudolocalization/fake_bidi", false));
	main_domain->set_pseudolocalization_override_enabled(GLOBAL_DEF("internationalization/pseudolocalization/override", false));
	main_domain->set_pseudolocalization_expansion_ratio(GLOBAL_DEF("internationalization/pseudolocalization/expansion_ratio", 0.0));
	main_domain->set_pseudolocalization_prefix(GLOBAL_DEF("internationalization/pseudolocalization/prefix", "["));
	main_domain->set_pseudolocalization_suffix(GLOBAL_DEF("internationalization/pseudolocalization/suffix", "]"));
	main_domain->set_pseudolocalization_skip_placeholders_enabled(GLOBAL_DEF("internationalization/pseudolocalization/skip_placeholders", true));

#ifdef TOOLS_ENABLED
	ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, "internationalization/locale/test", PROPERTY_HINT_LOCALE_ID, ""));
	ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, "internationalization/locale/fallback", PROPERTY_HINT_LOCALE_ID, ""));
#endif
}

String TranslationServer::get_tool_locale() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
		const PackedStringArray &locales = editor_domain->get_loaded_locales();
		if (locales.is_empty()) {
			return "en";
		}
		return locales[0];
	} else {
#else
	{
#endif
		// Look for best matching loaded translation.
		Ref<Translation> t = main_domain->get_translation_object(locale);
		if (t.is_null()) {
			return "en";
		}
		return t->get_locale();
	}
}

StringName TranslationServer::tool_translate(const StringName &p_message, const StringName &p_context) const {
	return editor_domain->translate(p_message, p_context);
}

StringName TranslationServer::tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	return editor_domain->translate_plural(p_message, p_message_plural, p_n, p_context);
}

StringName TranslationServer::property_translate(const StringName &p_message, const StringName &p_context) const {
	return property_domain->translate(p_message, p_context);
}

StringName TranslationServer::doc_translate(const StringName &p_message, const StringName &p_context) const {
	return doc_domain->translate(p_message, p_context);
}

StringName TranslationServer::doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	return doc_domain->translate_plural(p_message, p_message_plural, p_n, p_context);
}

bool TranslationServer::is_pseudolocalization_enabled() const {
	return main_domain->is_pseudolocalization_enabled();
}

void TranslationServer::set_pseudolocalization_enabled(bool p_enabled) {
	main_domain->set_pseudolocalization_enabled(p_enabled);

	ResourceLoader::reload_translation_remaps();

	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void TranslationServer::reload_pseudolocalization() {
	main_domain->set_pseudolocalization_accents_enabled(GLOBAL_GET("internationalization/pseudolocalization/replace_with_accents"));
	main_domain->set_pseudolocalization_double_vowels_enabled(GLOBAL_GET("internationalization/pseudolocalization/double_vowels"));
	main_domain->set_pseudolocalization_fake_bidi_enabled(GLOBAL_GET("internationalization/pseudolocalization/fake_bidi"));
	main_domain->set_pseudolocalization_override_enabled(GLOBAL_GET("internationalization/pseudolocalization/override"));
	main_domain->set_pseudolocalization_expansion_ratio(GLOBAL_GET("internationalization/pseudolocalization/expansion_ratio"));
	main_domain->set_pseudolocalization_prefix(GLOBAL_GET("internationalization/pseudolocalization/prefix"));
	main_domain->set_pseudolocalization_suffix(GLOBAL_GET("internationalization/pseudolocalization/suffix"));
	main_domain->set_pseudolocalization_skip_placeholders_enabled(GLOBAL_GET("internationalization/pseudolocalization/skip_placeholders"));

	ResourceLoader::reload_translation_remaps();

	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

StringName TranslationServer::pseudolocalize(const StringName &p_message) const {
	return main_domain->pseudolocalize(p_message);
}

#ifdef TOOLS_ENABLED
void TranslationServer::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		HashMap<String, String> *target_hash_map = nullptr;
		if (pf == "get_language_name") {
			target_hash_map = &language_map;
		} else if (pf == "get_script_name") {
			target_hash_map = &script_map;
		} else if (pf == "get_country_name") {
			target_hash_map = &country_name_map;
		}

		if (target_hash_map) {
			for (const KeyValue<String, String> &E : *target_hash_map) {
				r_options->push_back(E.key.quote());
			}
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif // TOOLS_ENABLED

void TranslationServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &TranslationServer::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &TranslationServer::get_locale);
	ClassDB::bind_method(D_METHOD("get_tool_locale"), &TranslationServer::get_tool_locale);

	ClassDB::bind_method(D_METHOD("compare_locales", "locale_a", "locale_b"), &TranslationServer::compare_locales);
	ClassDB::bind_method(D_METHOD("standardize_locale", "locale", "add_defaults"), &TranslationServer::standardize_locale, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_all_languages"), &TranslationServer::get_all_languages);
	ClassDB::bind_method(D_METHOD("get_language_name", "language"), &TranslationServer::get_language_name);

	ClassDB::bind_method(D_METHOD("get_all_scripts"), &TranslationServer::get_all_scripts);
	ClassDB::bind_method(D_METHOD("get_script_name", "script"), &TranslationServer::get_script_name);

	ClassDB::bind_method(D_METHOD("get_all_countries"), &TranslationServer::get_all_countries);
	ClassDB::bind_method(D_METHOD("get_country_name", "country"), &TranslationServer::get_country_name);

	ClassDB::bind_method(D_METHOD("get_locale_name", "locale"), &TranslationServer::get_locale_name);

	ClassDB::bind_method(D_METHOD("translate", "message", "context"), &TranslationServer::translate, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("translate_plural", "message", "plural_message", "n", "context"), &TranslationServer::translate_plural, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("add_translation", "translation"), &TranslationServer::add_translation);
	ClassDB::bind_method(D_METHOD("remove_translation", "translation"), &TranslationServer::remove_translation);
	ClassDB::bind_method(D_METHOD("get_translation_object", "locale"), &TranslationServer::get_translation_object);

	ClassDB::bind_method(D_METHOD("has_domain", "domain"), &TranslationServer::has_domain);
	ClassDB::bind_method(D_METHOD("get_or_add_domain", "domain"), &TranslationServer::get_or_add_domain);
	ClassDB::bind_method(D_METHOD("remove_domain", "domain"), &TranslationServer::remove_domain);

	ClassDB::bind_method(D_METHOD("clear"), &TranslationServer::clear);

	ClassDB::bind_method(D_METHOD("get_loaded_locales"), &TranslationServer::get_loaded_locales);

	ClassDB::bind_method(D_METHOD("is_pseudolocalization_enabled"), &TranslationServer::is_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("set_pseudolocalization_enabled", "enabled"), &TranslationServer::set_pseudolocalization_enabled);
	ClassDB::bind_method(D_METHOD("reload_pseudolocalization"), &TranslationServer::reload_pseudolocalization);
	ClassDB::bind_method(D_METHOD("pseudolocalize", "message"), &TranslationServer::pseudolocalize);
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "pseudolocalization_enabled"), "set_pseudolocalization_enabled", "is_pseudolocalization_enabled");
}

void TranslationServer::load_translations() {
	_load_translations("internationalization/locale/translations"); //all
	_load_translations("internationalization/locale/translations_" + locale.substr(0, 2));

	if (locale.substr(0, 2) != locale) {
		_load_translations("internationalization/locale/translations_" + locale);
	}
}

TranslationServer::TranslationServer() {
	singleton = this;
	main_domain.instantiate();
	editor_domain = get_or_add_domain("godot.editor");
	property_domain = get_or_add_domain("godot.properties");
	doc_domain = get_or_add_domain("godot.documentation");
	init_locale_info();
}
