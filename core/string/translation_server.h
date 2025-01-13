/**************************************************************************/
/*  translation_server.h                                                  */
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

#ifndef TRANSLATION_SERVER_H
#define TRANSLATION_SERVER_H

#include "core/string/translation.h"
#include "core/string/translation_domain.h"

class TranslationServer : public Object {
	GDCLASS(TranslationServer, Object);

	String locale = "en";
	String fallback;

	Ref<TranslationDomain> main_domain;
	Ref<TranslationDomain> editor_domain;
	Ref<TranslationDomain> property_domain;
	Ref<TranslationDomain> doc_domain;
	HashMap<StringName, Ref<TranslationDomain>> custom_domains;

	mutable HashMap<String, int> locale_compare_cache;

	bool enabled = true;

	static inline TranslationServer *singleton = nullptr;
	bool _load_translations(const String &p_from);

	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	String _standardize_locale_bind_compat_98972(const String &p_locale) const;
	static void _bind_compatibility_methods();
#endif

	struct LocaleScriptInfo {
		String name;
		String script;
		String default_country;
		HashSet<String> supported_countries;
	};
	static Vector<LocaleScriptInfo> locale_script_info;

	struct Locale {
		String language;
		String script;
		String country;
		String variant;

		bool operator==(const Locale &p_locale) const {
			return (p_locale.language == language) &&
					(p_locale.script == script) &&
					(p_locale.country == country) &&
					(p_locale.variant == variant);
		}

		operator String() const;

		Locale(const TranslationServer &p_server, const String &p_locale, bool p_add_defaults);
	};

	static HashMap<String, String> language_map;
	static HashMap<String, String> script_map;
	static HashMap<String, String> locale_rename_map;
	static HashMap<String, String> country_name_map;
	static HashMap<String, String> country_rename_map;
	static HashMap<String, String> variant_map;

	void init_locale_info();

public:
	_FORCE_INLINE_ static TranslationServer *get_singleton() { return singleton; }

	Ref<TranslationDomain> get_editor_domain() const { return editor_domain; }

	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	_FORCE_INLINE_ bool is_enabled() const { return enabled; }

	void set_locale(const String &p_locale);
	String get_locale() const;
	String get_fallback_locale() const;
	Ref<Translation> get_translation_object(const String &p_locale);

	Vector<String> get_all_languages() const;
	String get_language_name(const String &p_language) const;

	Vector<String> get_all_scripts() const;
	String get_script_name(const String &p_script) const;

	Vector<String> get_all_countries() const;
	String get_country_name(const String &p_country) const;

	String get_locale_name(const String &p_locale) const;

	PackedStringArray get_loaded_locales() const;

	void add_translation(const Ref<Translation> &p_translation);
	void remove_translation(const Ref<Translation> &p_translation);

	StringName translate(const StringName &p_message, const StringName &p_context = "") const;
	StringName translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const;

	StringName pseudolocalize(const StringName &p_message) const;

	bool is_pseudolocalization_enabled() const;
	void set_pseudolocalization_enabled(bool p_enabled);
	void reload_pseudolocalization();

	String standardize_locale(const String &p_locale, bool p_add_defaults = false) const;

	int compare_locales(const String &p_locale_a, const String &p_locale_b) const;

	String get_tool_locale();
	StringName tool_translate(const StringName &p_message, const StringName &p_context = "") const;
	StringName tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const;
	StringName property_translate(const StringName &p_message, const StringName &p_context = "") const;
	StringName doc_translate(const StringName &p_message, const StringName &p_context = "") const;
	StringName doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const;

	bool has_domain(const StringName &p_domain) const;
	Ref<TranslationDomain> get_or_add_domain(const StringName &p_domain);
	void remove_domain(const StringName &p_domain);

	void setup();

	void clear();

	void load_translations();

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif // TOOLS_ENABLED

	TranslationServer();
};

#endif // TRANSLATION_SERVER_H
