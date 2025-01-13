/**************************************************************************/
/*  test_regex.h                                                          */
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

#ifndef TEST_REGEX_H
#define TEST_REGEX_H

#include "../regex.h"

#include "core/string/ustring.h"

#include "tests/test_macros.h"

namespace TestRegEx {

TEST_CASE("[RegEx] Initialization") {
	const String pattern = "(?<vowel>[aeiou])";

	RegEx re1(pattern);
	CHECK(re1.is_valid());
	CHECK(re1.get_pattern() == pattern);
	CHECK(re1.get_group_count() == 1);

	PackedStringArray names = re1.get_names();
	CHECK(names.size() == 1);
	CHECK(names[0] == "vowel");

	RegEx re2;
	CHECK(re2.is_valid() == false);
	CHECK(re2.compile(pattern) == OK);
	CHECK(re2.is_valid());

	CHECK(re1.get_pattern() == re2.get_pattern());
	CHECK(re1.get_group_count() == re2.get_group_count());

	names = re2.get_names();
	CHECK(names.size() == 1);
	CHECK(names[0] == "vowel");
}

TEST_CASE("[RegEx] Clearing") {
	RegEx re("Godot");
	REQUIRE(re.is_valid());
	re.clear();
	CHECK(re.is_valid() == false);
}

TEST_CASE("[RegEx] Searching") {
	const String s = "Searching";
	const String vowels = "[aeiou]{1,2}";
	const String numerics = "\\d";

	RegEx re(vowels);
	REQUIRE(re.is_valid());

	Ref<RegExMatch> match = re.search(s);
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == "ea");

	match = re.search(s, 1, 2);
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == "e");
	match = re.search(s, 2, 4);
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == "a");
	match = re.search(s, 3, 5);
	CHECK(match.is_null());
	match = re.search(s, 6, 2);
	CHECK(match.is_null());

	const Array all_results = re.search_all(s);
	CHECK(all_results.size() == 2);
	match = all_results[0];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == "ea");
	match = all_results[1];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == "i");

	CHECK(re.compile(numerics) == OK);
	CHECK(re.is_valid());
	CHECK(re.search(s).is_null());
	CHECK(re.search_all(s).size() == 0);
}

TEST_CASE("[RegEx] Substitution") {
	const String s1 = "Double all the vowels.";

	RegEx re1("(?<vowel>[aeiou])");
	REQUIRE(re1.is_valid());
	CHECK(re1.sub(s1, "$0$vowel", true) == "Doouublee aall thee vooweels.");

	const String s2 = "Substitution with group.";

	RegEx re2("Substitution (.+)");
	REQUIRE(re2.is_valid());
	CHECK(re2.sub(s2, "Test ${1}") == "Test with group.");

	const String s3 = "Useless substitution";

	RegEx re3("Anything");
	REQUIRE(re3.is_valid());
	CHECK(re3.sub(s3, "Something") == "Useless substitution");

	const String s4 = "acacac";

	RegEx re4("(a)(b){0}(c)");
	REQUIRE(re4.is_valid());
	CHECK(re4.sub(s4, "${1}.${3}.", true) == "a.c.a.c.a.c.");

	const String s5 = "aaaa";

	RegEx re5("a");
	REQUIRE(re5.is_valid());
	CHECK(re5.sub(s5, "b", true, 0, 2) == "bbaa");
	CHECK(re5.sub(s5, "b", true, 1, 3) == "abba");
	CHECK(re5.sub(s5, "b", true, 0, 0) == "aaaa");
	CHECK(re5.sub(s5, "b", true, 1, 1) == "aaaa");
	CHECK(re5.sub(s5, "cc", true, 0, 2) == "ccccaa");
	CHECK(re5.sub(s5, "cc", true, 1, 3) == "acccca");
	CHECK(re5.sub(s5, "", true, 0, 2) == "aa");

	const String s6 = "property get_property set_property";

	RegEx re6("(get_|set_)?property");
	REQUIRE(re6.is_valid());
	CHECK(re6.sub(s6, "$1new_property", true) == "new_property get_new_property set_new_property");
	ERR_PRINT_OFF;
	CHECK(re6.sub(s6, "$5new_property", true) == "new_property new_property new_property");
	ERR_PRINT_ON;
}

TEST_CASE("[RegEx] Substitution with empty input and/or replacement") {
	const String s1 = "";
	const String s2 = "gogogo";

	RegEx re1("");
	REQUIRE(re1.is_valid());
	CHECK(re1.sub(s1, "") == "");
	CHECK(re1.sub(s1, "a") == "a");
	CHECK(re1.sub(s2, "") == "gogogo");

	RegEx re2("go");
	REQUIRE(re2.is_valid());
	CHECK(re2.sub(s2, "") == "gogo");
	CHECK(re2.sub(s2, "", true) == "");
}

TEST_CASE("[RegEx] Uninitialized use") {
	const String s = "Godot";

	RegEx re;
	ERR_PRINT_OFF;
	CHECK(re.search(s).is_null());
	CHECK(re.search_all(s).size() == 0);
	CHECK(re.sub(s, "") == "");
	CHECK(re.get_group_count() == 0);
	CHECK(re.get_names().size() == 0);
	ERR_PRINT_ON
}

TEST_CASE("[RegEx] Empty pattern") {
	const String s = "Godot";

	RegEx re;
	CHECK(re.compile("") == OK);
	CHECK(re.is_valid());
}

TEST_CASE("[RegEx] Complex Grouping") {
	const String test = "https://docs.godotengine.org/en/latest/contributing/";

	// Ignored protocol in grouping.
	RegEx re("^(?:https?://)([a-zA-Z]{2,4})\\.([a-zA-Z][a-zA-Z0-9_\\-]{2,64})\\.([a-zA-Z]{2,4})");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> expr = re.search(test);

	CHECK(expr->get_group_count() == 3);

	CHECK(expr->get_string(0) == "https://docs.godotengine.org");

	CHECK(expr->get_string(1) == "docs");
	CHECK(expr->get_string(2) == "godotengine");
	CHECK(expr->get_string(3) == "org");
}

TEST_CASE("[RegEx] Number Expression") {
	const String test = "(2.5e-3 + 35 + 46) / 2.8e0 = 28.9294642857";

	// Not an exact regex for number but a good test.
	RegEx re("([+-]?\\d+)(\\.\\d+([eE][+-]?\\d+)?)?");
	REQUIRE(re.is_valid());
	Array number_match = re.search_all(test);

	CHECK(number_match.size() == 5);

	Ref<RegExMatch> number = number_match[0];
	CHECK(number->get_string(0) == "2.5e-3");
	CHECK(number->get_string(1) == "2");
	number = number_match[1];
	CHECK(number->get_string(0) == "35");
	number = number_match[2];
	CHECK(number->get_string(0) == "46");
	number = number_match[3];
	CHECK(number->get_string(0) == "2.8e0");
	number = number_match[4];
	CHECK(number->get_string(0) == "28.9294642857");
	CHECK(number->get_string(1) == "28");
	CHECK(number->get_string(2) == ".9294642857");
}

TEST_CASE("[RegEx] Invalid end position") {
	const String s = "Godot";

	RegEx re("o");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match = re.search(s, 0, 10);
	CHECK(match->get_string(0) == "o");

	const Array all_results = re.search_all(s, 0, 10);
	CHECK(all_results.size() == 2);
	match = all_results[0];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == String("o"));
	match = all_results[1];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == String("o"));

	CHECK(re.sub(s, "", true, 0, 10) == "Gdt");
}

TEST_CASE("[RegEx] Get match string list") {
	const String s = "Godot Engine";

	RegEx re("(Go)(dot)");
	Ref<RegExMatch> match = re.search(s);
	REQUIRE(match.is_valid());
	PackedStringArray result;
	result.append("Godot");
	result.append("Go");
	result.append("dot");
	CHECK(match->get_strings() == result);
}

TEST_CASE("[RegEx] Match start and end positions") {
	const String s = "Whole pattern";

	RegEx re1("pattern");
	REQUIRE(re1.is_valid());
	Ref<RegExMatch> match = re1.search(s);
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 6);
	CHECK(match->get_end(0) == 13);

	RegEx re2("(?<vowel>[aeiou])");
	REQUIRE(re2.is_valid());
	match = re2.search(s);
	REQUIRE(match.is_valid());
	CHECK(match->get_start("vowel") == 2);
	CHECK(match->get_end("vowel") == 3);
}

TEST_CASE("[RegEx] Asterisk search all") {
	const String s = "Godot Engine";

	RegEx re("o*");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match;
	const Array all_results = re.search_all(s);
	CHECK(all_results.size() == 13);

	match = all_results[0];
	CHECK(match->get_string(0) == "");
	match = all_results[1];
	CHECK(match->get_string(0) == "o");
	match = all_results[2];
	CHECK(match->get_string(0) == "");
	match = all_results[3];
	CHECK(match->get_string(0) == "o");

	for (int i = 4; i < 13; i++) {
		match = all_results[i];
		CHECK(match->get_string(0) == "");
	}
}

TEST_CASE("[RegEx] Simple lookahead") {
	const String s = "Godot Engine";

	RegEx re("o(?=t)");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match = re.search(s);
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 3);
	CHECK(match->get_end(0) == 4);
}

TEST_CASE("[RegEx] Lookahead groups empty matches") {
	const String s = "12";

	RegEx re("(?=(\\d+))");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match = re.search(s);
	CHECK(match->get_string(0) == "");
	CHECK(match->get_string(1) == "12");

	const Array all_results = re.search_all(s);
	CHECK(all_results.size() == 2);

	match = all_results[0];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == String(""));
	CHECK(match->get_string(1) == String("12"));

	match = all_results[1];
	REQUIRE(match.is_valid());
	CHECK(match->get_string(0) == String(""));
	CHECK(match->get_string(1) == String("2"));
}

TEST_CASE("[RegEx] Simple lookbehind") {
	const String s = "Godot Engine";

	RegEx re("(?<=d)o");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match = re.search(s);
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 3);
	CHECK(match->get_end(0) == 4);
}

TEST_CASE("[RegEx] Simple lookbehind search all") {
	const String s = "ababbaabab";

	RegEx re("(?<=a)b");
	REQUIRE(re.is_valid());
	const Array all_results = re.search_all(s);
	CHECK(all_results.size() == 4);

	Ref<RegExMatch> match = all_results[0];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 1);
	CHECK(match->get_end(0) == 2);

	match = all_results[1];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 3);
	CHECK(match->get_end(0) == 4);

	match = all_results[2];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 7);
	CHECK(match->get_end(0) == 8);

	match = all_results[3];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 9);
	CHECK(match->get_end(0) == 10);
}

TEST_CASE("[RegEx] Lookbehind groups empty matches") {
	const String s = "abaaabab";

	RegEx re("(?<=(b))");
	REQUIRE(re.is_valid());
	Ref<RegExMatch> match;

	const Array all_results = re.search_all(s);
	CHECK(all_results.size() == 3);

	match = all_results[0];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 2);
	CHECK(match->get_end(0) == 2);
	CHECK(match->get_start(1) == 1);
	CHECK(match->get_end(1) == 2);
	CHECK(match->get_string(0) == String(""));
	CHECK(match->get_string(1) == String("b"));

	match = all_results[1];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 6);
	CHECK(match->get_end(0) == 6);
	CHECK(match->get_start(1) == 5);
	CHECK(match->get_end(1) == 6);
	CHECK(match->get_string(0) == String(""));
	CHECK(match->get_string(1) == String("b"));

	match = all_results[2];
	REQUIRE(match.is_valid());
	CHECK(match->get_start(0) == 8);
	CHECK(match->get_end(0) == 8);
	CHECK(match->get_start(1) == 7);
	CHECK(match->get_end(1) == 8);
	CHECK(match->get_string(0) == String(""));
	CHECK(match->get_string(1) == String("b"));
}

} // namespace TestRegEx

#endif // TEST_REGEX_H
