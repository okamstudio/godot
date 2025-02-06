/**************************************************************************/
/*  test_translation_server.h                                             */
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

#include "core/string/translation_server.h"

#include "tests/test_macros.h"

namespace TestTranslationServer {
TEST_CASE("[TranslationServer] Translation operations") {
	Ref<Translation> t1 = memnew(Translation);
	t1->set_locale("uk");
	t1->add_message("Good Morning", String(U"Добрий ранок"));

	Ref<Translation> t2 = memnew(Translation);
	t2->set_locale("uk");
	t2->add_message("Hello Godot", String(U"你好戈多"));

	TranslationServer *ts = TranslationServer::get_singleton();

	// Adds translation for UK locale for the first time.
	int l_count_before = ts->get_loaded_locales().size();
	ts->add_translation(t1);
	int l_count_after = ts->get_loaded_locales().size();
	CHECK(l_count_after > l_count_before);

	// Adds translation for UK locale again.
	ts->add_translation(t2);
	CHECK_EQ(ts->get_loaded_locales().size(), l_count_after);

	// Removing that translation.
	ts->remove_translation(t2);
	CHECK_EQ(ts->get_loaded_locales().size(), l_count_after);

	CHECK(ts->get_translation_object("uk").is_valid());

	ts->set_locale("uk");
	CHECK(ts->translate("Good Morning") == String::utf8("Добрий ранок"));

	ts->remove_translation(t1);
	CHECK(ts->get_translation_object("uk").is_null());
	// If no suitable Translation object has been found - the original message should be returned.
	CHECK(ts->translate("Good Morning") == "Good Morning");
}

TEST_CASE("[TranslationServer] Locale operations") {
	TranslationServer *ts = TranslationServer::get_singleton();

	// Language variant test; we supplied the variant of Español and the result should be the same string.
	String loc = "es_Hani_ES_tradnl";
	String res = ts->standardize_locale(loc);

	CHECK(res == loc);

	// No such variant in variant_map; should return everything except the variant.
	loc = "es_Hani_ES_missing";
	res = ts->standardize_locale(loc);

	CHECK(res == "es_Hani_ES");

	// Non-ISO language name check (Windows issue).
	loc = "iw_Hani_IL";
	res = ts->standardize_locale(loc);

	CHECK(res == "he_Hani_IL");

	// Country rename check.
	loc = "uk_Hani_UK";
	res = ts->standardize_locale(loc);

	CHECK(res == "uk_Hani_GB");

	// Supplying a script name that is not in the list.
	loc = "de_Wrong_DE";
	res = ts->standardize_locale(loc);

	CHECK(res == "de_DE");

	// No added defaults.
	loc = "es_ES";
	res = ts->standardize_locale(loc, true);

	CHECK(res == "es_ES");

	// Add default script.
	loc = "az_AZ";
	res = ts->standardize_locale(loc, true);

	CHECK(res == "az_Latn_AZ");

	// Add default country.
	loc = "pa_Arab";
	res = ts->standardize_locale(loc, true);

	CHECK(res == "pa_Arab_PK");

	// Add default script and country.
	loc = "zh";
	res = ts->standardize_locale(loc, true);

	CHECK(res == "zh_Hans_CN");

	// Explicitly don't add defaults.
	loc = "zh";
	res = ts->standardize_locale(loc, false);

	CHECK(res == "zh");
}

TEST_CASE("[TranslationServer] Comparing locales") {
	TranslationServer *ts = TranslationServer::get_singleton();

	String locale_a = "es";
	String locale_b = "es";

	// Exact match check.
	int res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 10);

	locale_a = "sr-Latn-CS";
	locale_b = "sr-Latn-RS";

	// Script matches (+1) but country doesn't (-1).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 5);

	locale_a = "uz-Cyrl-UZ";
	locale_b = "uz-Latn-UZ";

	// Country matches (+1) but script doesn't (-1).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 5);

	locale_a = "aa-Latn-ER";
	locale_b = "aa-Latn-ER-saaho";

	// Script and country match (+2) with variant on one locale (+0).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 7);

	locale_a = "uz-Cyrl-UZ";
	locale_b = "uz-Latn-KG";

	// Both script and country mismatched (-2).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 3);

	locale_a = "es-ES";
	locale_b = "es-AR";

	// Mismatched country (-1).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 4);

	locale_a = "es";
	locale_b = "es-AR";

	// No country for one locale (+0).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 5);

	locale_a = "es-EC";
	locale_b = "fr-LU";

	// No match.
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 0);

	locale_a = "zh-HK";
	locale_b = "zh";

	// In full standardization, zh-HK becomes zh_Hant_HK and zh becomes
	// zh_Hans_CN. Both script and country mismatch (-2).
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 3);

	locale_a = "zh-CN";
	locale_b = "zh";

	// In full standardization, zh and zh-CN both become zh_Hans_CN for an
	// exact match.
	res = ts->compare_locales(locale_a, locale_b);

	CHECK(res == 10);
}
} // namespace TestTranslationServer
