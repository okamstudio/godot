/**************************************************************************/
/*  translation_domain.h                                                  */
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

#ifndef TRANSLATION_DOMAIN_H
#define TRANSLATION_DOMAIN_H

#include "core/object/ref_counted.h"

class Translation;

class TranslationDomain : public RefCounted {
	GDCLASS(TranslationDomain, RefCounted);

	struct PseudolocalizationConfig {
		bool enabled = false;
		bool accents_enabled = true;
		bool double_vowels_enabled = false;
		bool fake_bidi_enabled = false;
		bool override_enabled = false;
		bool skip_placeholders_enabled = true;
		float expansion_ratio = 0.0;
		String prefix = "[";
		String suffix = "]";
	};

	HashSet<Ref<Translation>> translations;
	PseudolocalizationConfig pseudolocalization;

	String _get_override_string(const String &p_message) const;
	String _double_vowels(const String &p_message) const;
	String _replace_with_accented_string(const String &p_message) const;
	String _wrap_with_fakebidi_characters(const String &p_message) const;
	String _add_padding(const String &p_message, int p_length) const;
	const char32_t *_get_accented_version(char32_t p_character) const;
	bool _is_placeholder(const String &p_message, int p_index) const;

protected:
	static void _bind_methods();

public:
	// Methods in this section are not intended for scripting.
	StringName get_message_from_translations(const String &p_locale, const StringName &p_message, const StringName &p_context) const;
	StringName get_message_from_translations(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const;
	PackedStringArray get_loaded_locales() const;

public:
	Ref<Translation> get_translation_object(const String &p_locale) const;

	void add_translation(const Ref<Translation> &p_translation);
	void remove_translation(const Ref<Translation> &p_translation);
	void clear();

	StringName translate(const StringName &p_message, const StringName &p_context) const;
	StringName translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const;

	bool is_pseudolocalization_enabled() const;
	void set_pseudolocalization_enabled(bool p_enabled);
	bool is_pseudolocalization_accents_enabled() const;
	void set_pseudolocalization_accents_enabled(bool p_enabled);
	bool is_pseudolocalization_double_vowels_enabled() const;
	void set_pseudolocalization_double_vowels_enabled(bool p_enabled);
	bool is_pseudolocalization_fake_bidi_enabled() const;
	void set_pseudolocalization_fake_bidi_enabled(bool p_enabled);
	bool is_pseudolocalization_override_enabled() const;
	void set_pseudolocalization_override_enabled(bool p_enabled);
	bool is_pseudolocalization_skip_placeholders_enabled() const;
	void set_pseudolocalization_skip_placeholders_enabled(bool p_enabled);
	float get_pseudolocalization_expansion_ratio() const;
	void set_pseudolocalization_expansion_ratio(float p_ratio);
	String get_pseudolocalization_prefix() const;
	void set_pseudolocalization_prefix(const String &p_prefix);
	String get_pseudolocalization_suffix() const;
	void set_pseudolocalization_suffix(const String &p_suffix);

	StringName pseudolocalize(const StringName &p_message) const;

	TranslationDomain();
};

#endif // TRANSLATION_DOMAIN_H
