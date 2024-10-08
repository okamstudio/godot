/**************************************************************************/
/*  test_string.h                                                         */
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

#ifndef TEST_STRING_H
#define TEST_STRING_H

#include "core/string/ustring.h"

#include "tests/test_macros.h"

namespace TestString {

int u32scmp(const char32_t *l, const char32_t *r) {
	for (; *l == *r && *l && *r; l++, r++) {
		// Continue.
	}
	return *l - *r;
}

TEST_CASE("[String] Assign Latin-1 char string") {
	String s = "Hello";
	CHECK(u32scmp(s.get_data(), U"Hello") == 0);
}

TEST_CASE("[String] Assign from Latin-1 char string (operator=)") {
	String s = "Dolly";
	const String &t = s;
	CHECK(u32scmp(t.get_data(), U"Dolly") == 0);
}

TEST_CASE("[String] Assign from Latin-1 char string (copycon)") {
	String s("Sheep");
	const String &t1(s);
	CHECK(u32scmp(t1.get_data(), U"Sheep") == 0);

	String t2 = String("Sheep", 3);
	CHECK(u32scmp(t2.get_data(), U"She") == 0);
}

TEST_CASE("[String] Assign from wchar_t string (operator=)") {
	String s = L"Give me";
	CHECK(u32scmp(s.get_data(), U"Give me") == 0);
}

TEST_CASE("[String] Assign from wchar_t string (copycon)") {
	String s(L"Wool");
	CHECK(u32scmp(s.get_data(), U"Wool") == 0);
}

TEST_CASE("[String] Assign from char32_t string (operator=)") {
	String s = U"Give me";
	CHECK(u32scmp(s.get_data(), U"Give me") == 0);
}

TEST_CASE("[String] Assign from char32_t string (copycon)") {
	String s(U"Wool");
	CHECK(u32scmp(s.get_data(), U"Wool") == 0);
}

TEST_CASE("[String] UTF8") {
	/* how can i embed UTF in here? */
	static const char32_t u32str[] = { 0x0045, 0x0020, 0x304A, 0x360F, 0x3088, 0x3046, 0x1F3A4, 0 };
	static const uint8_t u8str[] = { 0x45, 0x20, 0xE3, 0x81, 0x8A, 0xE3, 0x98, 0x8F, 0xE3, 0x82, 0x88, 0xE3, 0x81, 0x86, 0xF0, 0x9F, 0x8E, 0xA4, 0 };
	String s = u32str;
	Error err = s.parse_utf8(s.utf8().get_data());
	CHECK(err == OK);
	CHECK(s == u32str);

	err = s.parse_utf8((const char *)u8str);
	CHECK(err == OK);
	CHECK(s == u32str);

	CharString cs = (const char *)u8str;
	CHECK(String::utf8(cs) == s);
}

TEST_CASE("[String] UTF16") {
	/* how can i embed UTF in here? */
	static const char32_t u32str[] = { 0x0045, 0x0020, 0x304A, 0x360F, 0x3088, 0x3046, 0x1F3A4, 0 };
	static const char16_t u16str[] = { 0x0045, 0x0020, 0x304A, 0x360F, 0x3088, 0x3046, 0xD83C, 0xDFA4, 0 };
	String s = u32str;
	Error err = s.parse_utf16(s.utf16().get_data());
	CHECK(err == OK);
	CHECK(s == u32str);

	err = s.parse_utf16(u16str);
	CHECK(err == OK);
	CHECK(s == u32str);

	Char16String cs = u16str;
	CHECK(String::utf16(cs) == s);
}

TEST_CASE("[String] UTF8 with BOM") {
	/* how can i embed UTF in here? */
	static const char32_t u32str[] = { 0x0045, 0x0020, 0x304A, 0x360F, 0x3088, 0x3046, 0x1F3A4, 0 };
	static const uint8_t u8str[] = { 0xEF, 0xBB, 0xBF, 0x45, 0x20, 0xE3, 0x81, 0x8A, 0xE3, 0x98, 0x8F, 0xE3, 0x82, 0x88, 0xE3, 0x81, 0x86, 0xF0, 0x9F, 0x8E, 0xA4, 0 };
	String s;
	Error err = s.parse_utf8((const char *)u8str);
	CHECK(err == OK);
	CHECK(s == u32str);

	CharString cs = (const char *)u8str;
	CHECK(String::utf8(cs) == s);
}

TEST_CASE("[String] UTF16 with BOM") {
	/* how can i embed UTF in here? */
	static const char32_t u32str[] = { 0x0020, 0x0045, 0x304A, 0x360F, 0x3088, 0x3046, 0x1F3A4, 0 };
	static const char16_t u16str[] = { 0xFEFF, 0x0020, 0x0045, 0x304A, 0x360F, 0x3088, 0x3046, 0xD83C, 0xDFA4, 0 };
	static const char16_t u16str_swap[] = { 0xFFFE, 0x2000, 0x4500, 0x4A30, 0x0F36, 0x8830, 0x4630, 0x3CD8, 0xA4DF, 0 };
	String s;
	Error err = s.parse_utf16(u16str);
	CHECK(err == OK);
	CHECK(s == u32str);

	err = s.parse_utf16(u16str_swap);
	CHECK(err == OK);
	CHECK(s == u32str);

	Char16String cs = u16str;
	CHECK(String::utf16(cs) == s);

	cs = u16str_swap;
	CHECK(String::utf16(cs) == s);
}

TEST_CASE("[String] UTF8 with CR") {
	const String base = U"Hello darkness\r\nMy old friend\nI've come to talk\rWith you again";

	String keep_cr;
	Error err = keep_cr.parse_utf8(base.utf8().get_data());
	CHECK(err == OK);
	CHECK(keep_cr == base);

	String no_cr;
	err = no_cr.parse_utf8(base.utf8().get_data(), -1, true); // Skip CR.
	CHECK(err == OK);
	CHECK(no_cr == base.replace("\r", ""));
}

TEST_CASE("[String] Invalid UTF8 (non-standard)") {
	ERR_PRINT_OFF
	static const uint8_t u8str[] = { 0x45, 0xE3, 0x81, 0x8A, 0xE3, 0x82, 0x88, 0xE3, 0x81, 0x86, 0xF0, 0x9F, 0x8E, 0xA4, 0xF0, 0x82, 0x82, 0xAC, 0xED, 0xA0, 0x81, 0 };
	//                               +     +2                +2                +2                +3                      overlong +3             unpaired +2
	static const char32_t u32str[] = { 0x45, 0x304A, 0x3088, 0x3046, 0x1F3A4, 0x20AC, 0xFFFD, 0 };
	String s;
	Error err = s.parse_utf8((const char *)u8str);
	CHECK(err == ERR_INVALID_DATA);
	CHECK(s == u32str);

	CharString cs = (const char *)u8str;
	CHECK(String::utf8(cs) == s);
	ERR_PRINT_ON
}

TEST_CASE("[String] Invalid UTF8 (unrecoverable)") {
	ERR_PRINT_OFF
	static const uint8_t u8str[] = { 0x45, 0xE3, 0x81, 0x8A, 0x8F, 0xE3, 0xE3, 0x98, 0x8F, 0xE3, 0x82, 0x88, 0xE3, 0x81, 0x86, 0xC0, 0x80, 0xF0, 0x9F, 0x8E, 0xA4, 0xF0, 0x82, 0x82, 0xAC, 0xED, 0xA0, 0x81, 0 };
	//                               +     +2                inv   +2    inv   inv   inv   +2                +2                ovl NUL +1  +3                      overlong +3             unpaired +2
	static const char32_t u32str[] = { 0x45, 0x304A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x3088, 0x3046, 0xFFFD, 0x1F3A4, 0x20AC, 0xFFFD, 0 };
	String s;
	Error err = s.parse_utf8((const char *)u8str);
	CHECK(err == ERR_INVALID_DATA);
	CHECK(s == u32str);

	CharString cs = (const char *)u8str;
	CHECK(String::utf8(cs) == s);
	ERR_PRINT_ON
}

TEST_CASE("[String] Invalid UTF16 (non-standard)") {
	ERR_PRINT_OFF
	static const char16_t u16str[] = { 0x0045, 0x304A, 0x3088, 0x3046, 0xDFA4, 0 };
	//                                 +       +       +       +       unpaired
	static const char32_t u32str[] = { 0x0045, 0x304A, 0x3088, 0x3046, 0xDFA4, 0 };
	String s;
	Error err = s.parse_utf16(u16str);
	CHECK(err == ERR_PARSE_ERROR);
	CHECK(s == u32str);

	Char16String cs = u16str;
	CHECK(String::utf16(cs) == s);
	ERR_PRINT_ON
}

TEST_CASE("[String] ASCII") {
	String s = U"Primero Leche";
	String t = s.ascii(false).get_data();
	CHECK(s == t);

	t = s.ascii(true).get_data();
	CHECK(s == t);
}

TEST_CASE("[String] Comparisons (equal)") {
	String s = "Test Compare";
	CHECK(s == "Test Compare");
	CHECK(s == U"Test Compare");
	CHECK(s == L"Test Compare");
	CHECK(s == String("Test Compare"));

	CharString empty = "";
	CharString cs = "Test Compare";
	CHECK(!(empty == cs));
	CHECK(!(cs == empty));
	CHECK(cs == CharString("Test Compare"));
}

TEST_CASE("[String] Comparisons (not equal)") {
	String s = "Test Compare";
	CHECK(s != "Peanut");
	CHECK(s != U"Coconut");
	CHECK(s != L"Coconut");
	CHECK(s != String("Butter"));
}

TEST_CASE("[String] Comparisons (operator <)") {
	String s = "Bees";
	CHECK(s < "Elephant");
	CHECK(!(s < U"Amber"));
	CHECK(!(s < L"Amber"));
	CHECK(!(s < String("Beatrix")));
}

TEST_CASE("[String] Concatenation") {
	String s;

	s += "Have";
	s += ' ';
	s += 'a';
	s += String(" ");
	s = s + U"Nice";
	s = s + " ";
	s = s + String("Day");

	CHECK(s == "Have a Nice Day");
}

TEST_CASE("[String] Testing size and length of string") {
	// todo: expand this test to do more tests on size() as it is complicated under the hood.
	CHECK(String("Mellon").size() == 7);
	CHECK(String("Mellon1").size() == 8);

	// length works fine and is easier to test
	CHECK(String("Mellon").length() == 6);
	CHECK(String("Mellon1").length() == 7);
	CHECK(String("Mellon2").length() == 7);
	CHECK(String("Mellon3").length() == 7);
}

TEST_CASE("[String] Testing for empty string") {
	CHECK(!String("Mellon").is_empty());
	// do this more than once, to check for string corruption
	CHECK(String("").is_empty());
	CHECK(String("").is_empty());
	CHECK(String("").is_empty());
}

TEST_CASE("[String] Contains") {
	String s = "C:\\Godot\\project\\string_test.tscn";
	CHECK(s.contains(":\\"));
	CHECK(s.contains("Godot"));
	CHECK(s.contains(String("project\\string_test")));
	CHECK(s.contains(String("\\string_test.tscn")));

	CHECK(!s.contains("://"));
	CHECK(!s.contains("Godoh"));
	CHECK(!s.contains(String("project\\string test")));
	CHECK(!s.contains(String("\\char_test.tscn")));
}

TEST_CASE("[String] Contains case insensitive") {
	String s = "C:\\Godot\\project\\string_test.tscn";
	CHECK(s.containsn("Godot"));
	CHECK(s.containsn("godot"));
	CHECK(s.containsn(String("Project\\string_test")));
	CHECK(s.containsn(String("\\string_Test.tscn")));

	CHECK(!s.containsn("Godoh"));
	CHECK(!s.containsn("godoh"));
	CHECK(!s.containsn(String("project\\string test")));
	CHECK(!s.containsn(String("\\char_test.tscn")));
}

TEST_CASE("[String] Test chr") {
	CHECK(String::chr('H') == "H");
	CHECK(String::chr(0x3012)[0] == 0x3012);
	ERR_PRINT_OFF
	CHECK(String::chr(0xd812)[0] == 0xfffd); // Unpaired UTF-16 surrogate
	CHECK(String::chr(0x20d812)[0] == 0xfffd); // Outside UTF-32 range
	ERR_PRINT_ON
}

TEST_CASE("[String] Operator []") {
	String a = "Kugar Sane";
	a[0] = 'S';
	a[6] = 'C';
	CHECK(a == "Sugar Cane");
	CHECK(a[1] == 'u');
	CHECK(a.unicode_at(1) == 'u');
}

TEST_CASE("[String] Case function test") {
	String a = "MoMoNgA";

	CHECK(a.to_upper() == "MOMONGA");
	CHECK(a.to_lower() == "momonga");
}

TEST_CASE("[String] Case compare function test") {
	String a = "MoMoNgA";

	CHECK(a.casecmp_to("momonga") != 0);
	CHECK(a.nocasecmp_to("momonga") == 0);
}

TEST_CASE("[String] Natural compare function test") {
	String a = "img2.png";

	CHECK(a.nocasecmp_to("img10.png") > 0);
	CHECK(a.naturalnocasecmp_to("img10.png") < 0);
}

TEST_CASE("[String] File compare function test") {
	String a = "_img2.png";
	String b = "img2.png";

	CHECK(a.nocasecmp_to("img10.png") > 0);
	CHECK_MESSAGE(a.filenocasecmp_to("img10.png") < 0, "Should sort before letters.");
	CHECK_MESSAGE(a.filenocasecmp_to(".img10.png") > 0, "Should sort after period.");
	CHECK(b.filenocasecmp_to("img3.png") < 0);
}

TEST_CASE("[String] hex_encode_buffer") {
	static const uint8_t u8str[] = { 0x45, 0xE3, 0x81, 0x8A, 0x8F, 0xE3 };
	String s = String::hex_encode_buffer(u8str, 6);
	CHECK(s == U"45e3818a8fe3");
}

TEST_CASE("[String] Substr") {
	String s = "Killer Baby";
	CHECK(s.substr(3, 4) == "ler ");
	CHECK(s.substr(3) == "ler Baby");
}

TEST_CASE("[String] Find") {
	String s = "Pretty Woman Woman";
	MULTICHECK_STRING_EQ(s, find, "tty", 3);
	MULTICHECK_STRING_EQ(s, find, "Revenge of the Monster Truck", -1);
	MULTICHECK_STRING_INT_EQ(s, find, "Wo", 9, 13);
	MULTICHECK_STRING_EQ(s, find, "", -1);
	MULTICHECK_STRING_EQ(s, find, "Pretty Woman Woman", 0);
	MULTICHECK_STRING_EQ(s, find, "WOMAN", -1);
	MULTICHECK_STRING_INT_EQ(s, find, "", 9, -1);

	MULTICHECK_STRING_EQ(s, rfind, "", -1);
	MULTICHECK_STRING_EQ(s, rfind, "foo", -1);
	MULTICHECK_STRING_EQ(s, rfind, "Pretty Woman Woman", 0);
	MULTICHECK_STRING_EQ(s, rfind, "man", 15);
	MULTICHECK_STRING_EQ(s, rfind, "WOMAN", -1);
	MULTICHECK_STRING_INT_EQ(s, rfind, "", 15, -1);
}

TEST_CASE("[String] Find case insensitive") {
	String s = "Pretty Whale Whale";
	MULTICHECK_STRING_EQ(s, findn, "WHA", 7);
	MULTICHECK_STRING_INT_EQ(s, findn, "WHA", 9, 13);
	MULTICHECK_STRING_EQ(s, findn, "Revenge of the Monster SawFish", -1);
	MULTICHECK_STRING_EQ(s, findn, "", -1);
	MULTICHECK_STRING_EQ(s, findn, "wha", 7);
	MULTICHECK_STRING_EQ(s, findn, "Wha", 7);
	MULTICHECK_STRING_INT_EQ(s, findn, "", 3, -1);

	MULTICHECK_STRING_EQ(s, rfindn, "WHA", 13);
	MULTICHECK_STRING_EQ(s, rfindn, "", -1);
	MULTICHECK_STRING_EQ(s, rfindn, "wha", 13);
	MULTICHECK_STRING_EQ(s, rfindn, "Wha", 13);
	MULTICHECK_STRING_INT_EQ(s, rfindn, "", 13, -1);
}

TEST_CASE("[String] Find MK") {
	Vector<String> keys;
	keys.push_back("sty");
	keys.push_back("tty");
	keys.push_back("man");

	String s = "Pretty Woman";
	int key = 0;

	CHECK(s.findmk(keys, 0, &key) == 3);
	CHECK(key == 1);

	CHECK(s.findmk(keys, 5, &key) == 9);
	CHECK(key == 2);
}

TEST_CASE("[String] Find and replace") {
	String s = "Happy Birthday, Anna!";
	MULTICHECK_STRING_STRING_EQ(s, replace, "Birthday", "Halloween", "Happy Halloween, Anna!");
	MULTICHECK_STRING_STRING_EQ(s, replace_first, "y", "Y", "HappY Birthday, Anna!");
	MULTICHECK_STRING_STRING_EQ(s, replacen, "Y", "Y", "HappY BirthdaY, Anna!");
}

TEST_CASE("[String] Insertion") {
	String s = "Who is Frederic?";
	s = s.insert(s.find("?"), " Chopin");
	CHECK(s == "Who is Frederic Chopin?");

	s = "foobar";
	CHECK(s.insert(0, "X") == "Xfoobar");
	CHECK(s.insert(-100, "X") == "foobar");
	CHECK(s.insert(6, "X") == "foobarX");
	CHECK(s.insert(100, "X") == "foobarX");
	CHECK(s.insert(2, "") == "foobar");

	s = "";
	CHECK(s.insert(0, "abc") == "abc");
	CHECK(s.insert(100, "abc") == "abc");
	CHECK(s.insert(-100, "abc") == "");
	CHECK(s.insert(0, "") == "");
}

TEST_CASE("[String] Erasing") {
	String s = "Josephine is such a cute girl!";
	s = s.erase(s.find("cute "), String("cute ").length());
	CHECK(s == "Josephine is such a girl!");
}

TEST_CASE("[String] Number to string") {
	CHECK(String::num(0) == "0");
	CHECK(String::num(0.0) == "0"); // No trailing zeros.
	CHECK(String::num(-0.0) == "-0"); // Includes sign even for zero.
	CHECK(String::num(3.141593) == "3.141593");
	CHECK(String::num(3.141593, 3) == "3.142");
	CHECK(String::num_scientific(30000000) == "3e+07");
	CHECK(String::num_int64(3141593) == "3141593");
	CHECK(String::num_int64(0xA141593, 16) == "a141593");
	CHECK(String::num_int64(0xA141593, 16, true) == "A141593");
	CHECK(String::num(42.100023, 4) == "42.1"); // No trailing zeros.

	// String::num_real tests.
	CHECK(String::num_real(1.0) == "1.0");
	CHECK(String::num_real(1.0, false) == "1");
	CHECK(String::num_real(9.9) == "9.9");
	CHECK(String::num_real(9.99) == "9.99");
	CHECK(String::num_real(9.999) == "9.999");
	CHECK(String::num_real(9.9999) == "9.9999");
	CHECK(String::num_real(3.141593) == "3.141593");
	CHECK(String::num_real(3.141) == "3.141"); // No trailing zeros.
#ifdef REAL_T_IS_DOUBLE
	CHECK_MESSAGE(String::num_real(123.456789) == "123.456789", "Prints the appropriate amount of digits for real_t = double.");
	CHECK_MESSAGE(String::num_real(-123.456789) == "-123.456789", "Prints the appropriate amount of digits for real_t = double.");
	CHECK_MESSAGE(String::num_real(Math_PI) == "3.14159265358979", "Prints the appropriate amount of digits for real_t = double.");
	CHECK_MESSAGE(String::num_real(3.1415f) == "3.1414999961853", "Prints more digits of 32-bit float when real_t = double (ones that would be reliable for double) and no trailing zero.");
#else
	CHECK_MESSAGE(String::num_real(123.456789) == "123.4568", "Prints the appropriate amount of digits for real_t = float.");
	CHECK_MESSAGE(String::num_real(-123.456789) == "-123.4568", "Prints the appropriate amount of digits for real_t = float.");
	CHECK_MESSAGE(String::num_real(Math_PI) == "3.141593", "Prints the appropriate amount of digits for real_t = float.");
	CHECK_MESSAGE(String::num_real(3.1415f) == "3.1415", "Prints only reliable digits of 32-bit float when real_t = float.");
#endif // REAL_T_IS_DOUBLE

	// Checks doubles with many decimal places.
	CHECK(String::num(0.0000012345432123454321, -1) == "0.00000123454321"); // -1 uses 14 as sane default.
	CHECK(String::num(0.0000012345432123454321) == "0.00000123454321"); // -1 is the default value.
	CHECK(String::num(-0.0000012345432123454321) == "-0.00000123454321");
	CHECK(String::num(-10000.0000012345432123454321) == "-10000.0000012345");
	CHECK(String::num(0.0000000000012345432123454321) == "0.00000000000123");
	CHECK(String::num(0.0000000000012345432123454321, 3) == "0");

	// Note: When relevant (remainder > 0.5), the last digit gets rounded up,
	// which can also lead to not include a trailing zero, e.g. "...89" -> "...9".
	CHECK(String::num(0.0000056789876567898765) == "0.00000567898766"); // Should round last digit.
	CHECK(String::num(10000.000005678999999999) == "10000.000005679"); // We cut at ...789|99 which is rounded to ...79, so only 13 decimals.
	CHECK(String::num(42.12999999, 6) == "42.13"); // Also happens with lower decimals count.

	// 32 is MAX_DECIMALS. We can't reliably store that many so we can't compare against a string,
	// but we can check that the string length is 34 (32 + 2 for "0.").
	CHECK(String::num(0.00000123456789987654321123456789987654321, 32).length() == 34);
	CHECK(String::num(0.00000123456789987654321123456789987654321, 42).length() == 34); // Should enforce MAX_DECIMALS.
	CHECK(String::num(10000.00000123456789987654321123456789987654321, 42).length() == 38); // 32 decimals + "10000.".
}

TEST_CASE("[String] String to integer") {
	static const char *nums[14] = { "1237461283", "- 22", "0", " - 1123412", "", "10_000_000", "-1_2_3_4", "10__000", "  1  2  34 ", "-0", "007", "--45", "---46", "-7-2" };
	static const int num[14] = { 1237461283, -22, 0, -1123412, 0, 10000000, -1234, 10000, 1234, 0, 7, 45, -46, -72 };

	for (int i = 0; i < 14; i++) {
		CHECK(String(nums[i]).to_int() == num[i]);
	}
	CHECK(String("0b1011").to_int() == 1011); // Looks like a binary number, but to_int() handles this as a base-10 number, "b" is just ignored.
	CHECK(String("0x1012").to_int() == 1012); // Looks like a hexadecimal number, but to_int() handles this as a base-10 number, "x" is just ignored.

	ERR_PRINT_OFF
	CHECK(String("999999999999999999999999999999999999999999999999999999999").to_int() == INT64_MAX); // Too large, largest possible is returned.
	CHECK(String("-999999999999999999999999999999999999999999999999999999999").to_int() == INT64_MIN); // Too small, smallest possible is returned.
	ERR_PRINT_ON
}

TEST_CASE("[String] Hex to integer") {
	static const char *nums[12] = { "0xFFAE", "22", "0", "AADDAD", "0x7FFFFFFFFFFFFFFF", "-0xf", "", "000", "000f", "0xaA", "-ff", "-" };
	static const int64_t num[12] = { 0xFFAE, 0x22, 0, 0xAADDAD, 0x7FFFFFFFFFFFFFFF, -0xf, 0, 0, 0xf, 0xaa, -0xff, 0x0 };

	for (int i = 0; i < 12; i++) {
		CHECK(String(nums[i]).hex_to_int() == num[i]);
	}

	// Invalid hex strings should return 0.
	static const char *invalid_nums[15] = { "qwerty", "QWERTY", "0xqwerty", "0x00qwerty", "qwerty00", "0x", "0x__", "__", "x12", "+", " ff", "ff ", "f f", "+ff", "--0x78" };

	ERR_PRINT_OFF
	for (int i = 0; i < 15; i++) {
		CHECK(String(invalid_nums[i]).hex_to_int() == 0);
	}

	CHECK(String("0xFFFFFFFFFFFFFFFFFFFFFFF").hex_to_int() == INT64_MAX); // Too large, largest possible is returned.
	CHECK(String("-0xFFFFFFFFFFFFFFFFFFFFFFF").hex_to_int() == INT64_MIN); // Too small, smallest possible is returned.
	ERR_PRINT_ON
}

TEST_CASE("[String] Bin to integer") {
	static const char *nums[10] = { "", "0", "0b0", "0b1", "0b", "1", "0b1010", "-0b11", "-1010", "0b0111111111111111111111111111111111111111111111111111111111111111" };
	static const int64_t num[10] = { 0, 0, 0, 1, 0, 1, 10, -3, -10, 0x7FFFFFFFFFFFFFFF };

	for (int i = 0; i < 10; i++) {
		CHECK(String(nums[i]).bin_to_int() == num[i]);
	}

	// Invalid bin strings should return 0. The long "0x11...11" is just too long for a 64 bit int.
	static const char *invalid_nums[16] = { "qwerty", "QWERTY", "0bqwerty", "0b00qwerty", "qwerty00", "0x__", "0b__", "__", "b12", "+", "-", "0x12ab", " 11", "11 ", "1 1", "--0b11" };

	for (int i = 0; i < 16; i++) {
		CHECK(String(invalid_nums[i]).bin_to_int() == 0);
	}

	ERR_PRINT_OFF
	CHECK(String("0b111111111111111111111111111111111111111111111111111111111111111111111111111111111").bin_to_int() == INT64_MAX); // Too large, largest possible is returned.
	CHECK(String("-0b111111111111111111111111111111111111111111111111111111111111111111111111111111111").bin_to_int() == INT64_MIN); // Too small, smallest possible is returned.
	ERR_PRINT_ON
}

TEST_CASE("[String] String to float") {
	static const char *nums[12] = { "-12348298412.2", "0.05", "2.0002", " -0.0001", "0", "000", "123", "0.0", "000.000", "000.007", "234__", "3..14" };
	static const double num[12] = { -12348298412.2, 0.05, 2.0002, -0.0001, 0.0, 0.0, 123.0, 0.0, 0.0, 0.007, 234.0, 3.0 };

	for (int i = 0; i < 12; i++) {
		CHECK(!(ABS(String(nums[i]).to_float() - num[i]) > 0.00001));
	}

	// Invalid float strings should return 0.
	static const char *invalid_nums[6] = { "qwerty", "qwerty123", "0xffff", "0b1010", "--3.13", "__345" };

	for (int i = 0; i < 6; i++) {
		CHECK(String(invalid_nums[i]).to_float() == 0);
	}

	// Very large exponents.
	CHECK(String("1e308").to_float() == 1e308);
	CHECK(String("-1e308").to_float() == -1e308);

	// Exponent is so high that value is INFINITY/-INFINITY.
	CHECK(String("1e309").to_float() == INFINITY);
	CHECK(String("1e511").to_float() == INFINITY);
	CHECK(String("-1e309").to_float() == -INFINITY);
	CHECK(String("-1e511").to_float() == -INFINITY);

	// Exponent is so high that a warning message is printed. Value is INFINITY/-INFINITY.
	ERR_PRINT_OFF
	CHECK(String("1e512").to_float() == INFINITY);
	CHECK(String("-1e512").to_float() == -INFINITY);
	ERR_PRINT_ON
}

TEST_CASE("[String] Slicing") {
	String s = "Mars,Jupiter,Saturn,Uranus";
	const char *slices[4] = { "Mars", "Jupiter", "Saturn", "Uranus" };
	MULTICHECK_GET_SLICE(s, ",", slices);
}

TEST_CASE("[String] Begins with") {
	// Test cases for true:
	MULTICHECK_STRING_EQ(String("res://foobar"), begins_with, "res://", true);
	MULTICHECK_STRING_EQ(String("abc"), begins_with, "abc", true);
	MULTICHECK_STRING_EQ(String("abc"), begins_with, "", true);
	MULTICHECK_STRING_EQ(String(""), begins_with, "", true);

	// Test cases for false:
	MULTICHECK_STRING_EQ(String("res"), begins_with, "res://", false);
	MULTICHECK_STRING_EQ(String("abcdef"), begins_with, "foo", false);
	MULTICHECK_STRING_EQ(String("abc"), begins_with, "ax", false);
	MULTICHECK_STRING_EQ(String(""), begins_with, "abc", false);

	// Test "const char *" version also with nullptr.
	String s("foo");
	bool state = s.begins_with(nullptr) == false;
	CHECK_MESSAGE(state, "nullptr check failed");

	String empty("");
	state = empty.begins_with(nullptr) == false;
	CHECK_MESSAGE(state, "nullptr check with empty string failed");
}

TEST_CASE("[String] Ends with") {
	// Test cases for true:
	MULTICHECK_STRING_EQ(String("res://foobar"), ends_with, "foobar", true);
	MULTICHECK_STRING_EQ(String("abc"), ends_with, "abc", true);
	MULTICHECK_STRING_EQ(String("abc"), ends_with, "", true);
	MULTICHECK_STRING_EQ(String(""), ends_with, "", true);

	// Test cases for false:
	MULTICHECK_STRING_EQ(String("res"), ends_with, "res://", false);
	MULTICHECK_STRING_EQ(String("abcdef"), ends_with, "foo", false);
	MULTICHECK_STRING_EQ(String("abc"), ends_with, "ax", false);
	MULTICHECK_STRING_EQ(String(""), ends_with, "abc", false);

	// Test "const char *" version also with nullptr.
	String s("foo");
	bool state = s.ends_with(nullptr) == false;
	CHECK_MESSAGE(state, "nullptr check failed");

	String empty("");
	state = empty.ends_with(nullptr) == false;
	CHECK_MESSAGE(state, "nullptr check with empty string failed");
}

TEST_CASE("[String] Splitting") {
	{
		const String s = "Mars,Jupiter,Saturn,Uranus";

		const char *slices_l[3] = { "Mars", "Jupiter", "Saturn,Uranus" };
		MULTICHECK_SPLIT(s, split, ",", true, 2, slices_l, 3);

		const char *slices_r[3] = { "Mars,Jupiter", "Saturn", "Uranus" };
		MULTICHECK_SPLIT(s, rsplit, ",", true, 2, slices_r, 3);
	}

	{
		const String s = "test";
		const char *slices[4] = { "t", "e", "s", "t" };
		MULTICHECK_SPLIT(s, split, "", true, 0, slices, 4);
	}

	{
		const String s = "";
		const char *slices[1] = { "" };
		MULTICHECK_SPLIT(s, split, "", true, 0, slices, 1);
		MULTICHECK_SPLIT(s, split, "", false, 0, slices, 0);
	}

	{
		const String s = "Mars Jupiter Saturn Uranus";
		const char *slices[4] = { "Mars", "Jupiter", "Saturn", "Uranus" };
		Vector<String> l = s.split_spaces();
		for (int i = 0; i < l.size(); i++) {
			CHECK(l[i] == slices[i]);
		}
	}

	{
		const String s = "1.2;2.3 4.5";
		const double slices[3] = { 1.2, 2.3, 4.5 };

		const Vector<double> d_arr = s.split_floats(";");
		CHECK(d_arr.size() == 2);
		for (int i = 0; i < d_arr.size(); i++) {
			CHECK(ABS(d_arr[i] - slices[i]) <= 0.00001);
		}

		const Vector<String> keys = { ";", " " };
		const Vector<float> f_arr = s.split_floats_mk(keys);
		CHECK(f_arr.size() == 3);
		for (int i = 0; i < f_arr.size(); i++) {
			CHECK(ABS(f_arr[i] - slices[i]) <= 0.00001);
		}
	}

	{
		const String s = " -2.0        5";
		const double slices[10] = { 0, -2, 0, 0, 0, 0, 0, 0, 0, 5 };

		const Vector<double> arr = s.split_floats(" ");
		CHECK(arr.size() == 10);
		for (int i = 0; i < arr.size(); i++) {
			CHECK(ABS(arr[i] - slices[i]) <= 0.00001);
		}

		const Vector<String> keys = { ";", " " };
		const Vector<float> mk = s.split_floats_mk(keys);
		CHECK(mk.size() == 10);
		for (int i = 0; i < mk.size(); i++) {
			CHECK(mk[i] == slices[i]);
		}
	}

	{
		const String s = "1;2 4";
		const int slices[3] = { 1, 2, 4 };

		const Vector<int> arr = s.split_ints(";");
		CHECK(arr.size() == 2);
		for (int i = 0; i < arr.size(); i++) {
			CHECK(arr[i] == slices[i]);
		}

		const Vector<String> keys = { ";", " " };
		const Vector<int> mk = s.split_ints_mk(keys);
		CHECK(mk.size() == 3);
		for (int i = 0; i < mk.size(); i++) {
			CHECK(mk[i] == slices[i]);
		}
	}
}

TEST_CASE("[String] format") {
	const String value_format = "red=\"$red\" green=\"$green\" blue=\"$blue\" alpha=\"$alpha\"";

	Dictionary value_dictionary;
	value_dictionary["red"] = 10;
	value_dictionary["green"] = 20;
	value_dictionary["blue"] = "bla";
	value_dictionary["alpha"] = 0.4;
	String value = value_format.format(value_dictionary, "$_");

	CHECK(value == "red=\"10\" green=\"20\" blue=\"bla\" alpha=\"0.4\"");
}

TEST_CASE("[String] sprintf") {
	String format, output;
	Array args;
	bool error;

	// %%
	format = "fish %% frog";
	args.clear();
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish % frog"));
	///// Ints

	// Int
	format = "fish %d frog";
	args.clear();
	args.push_back(5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 5 frog"));

	// Int left padded with zeroes.
	format = "fish %05d frog";
	args.clear();
	args.push_back(5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 00005 frog"));

	// Int left padded with spaces.
	format = "fish %5d frog";
	args.clear();
	args.push_back(5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish     5 frog"));

	// Int right padded with spaces.
	format = "fish %-5d frog";
	args.clear();
	args.push_back(5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 5     frog"));

	// Int with sign (positive).
	format = "fish %+d frog";
	args.clear();
	args.push_back(5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish +5 frog"));

	// Negative int.
	format = "fish %d frog";
	args.clear();
	args.push_back(-5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish -5 frog"));

	// Negative int left padded with spaces.
	format = "fish %5d frog";
	args.clear();
	args.push_back(-5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish    -5 frog"));

	// Negative int left padded with zeros.
	format = "fish %05d frog";
	args.clear();
	args.push_back(-5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish -0005 frog"));

	// Negative int right padded with spaces.
	format = "fish %-5d frog";
	args.clear();
	args.push_back(-5);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish -5    frog"));

	// Negative int right padded with zeros. (0 ignored)
	format = "fish %-05d frog";
	args.clear();
	args.push_back(-5);
	ERR_PRINT_OFF; // Silence warning about 0 ignored.
	output = format.sprintf(args, &error);
	ERR_PRINT_ON;
	REQUIRE(error == false);
	CHECK(output == String("fish -5    frog"));

	// Hex (lower)
	format = "fish %x frog";
	args.clear();
	args.push_back(45);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 2d frog"));

	// Hex (upper)
	format = "fish %X frog";
	args.clear();
	args.push_back(45);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 2D frog"));

	// Octal
	format = "fish %o frog";
	args.clear();
	args.push_back(99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 143 frog"));

	///// Reals

	// Real
	format = "fish %f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 99.990000 frog"));

	// Real left-padded.
	format = "fish %11f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish   99.990000 frog"));

	// Real (infinity) left-padded
	format = "fish %11f frog";
	args.clear();
	args.push_back(INFINITY);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish         inf frog"));

	// Real right-padded.
	format = "fish %-11f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 99.990000   frog"));

	// Real given int.
	format = "fish %f frog";
	args.clear();
	args.push_back(99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 99.000000 frog"));

	// Real with sign (positive).
	format = "fish %+f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish +99.990000 frog"));

	// Real with sign (negative zero).
	format = "fish %+f frog";
	args.clear();
	args.push_back(-0.0);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish -0.000000 frog"));

	// Real with sign (positive zero).
	format = "fish %+f frog";
	args.clear();
	args.push_back(0.0);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish +0.000000 frog"));

	// Real with 1 decimal.
	format = "fish %.1f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 100.0 frog"));

	// Real with 12 decimals.
	format = "fish %.12f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 99.990000000000 frog"));

	// Real with no decimals.
	format = "fish %.f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish 100 frog"));

	// Negative real right padded with zeros. (0 ignored)
	format = "fish %-011f frog";
	args.clear();
	args.push_back(-99.99);
	ERR_PRINT_OFF; // Silence warning about 0 ignored.
	output = format.sprintf(args, &error);
	ERR_PRINT_ON;
	REQUIRE(error == false);
	CHECK(output == String("fish -99.990000  frog"));

	///// Vectors

	// Vector2
	format = "fish %v frog";
	args.clear();
	args.push_back(Variant(Vector2(19.99, 1.00)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.990000, 1.000000) frog"));

	// Vector3
	format = "fish %v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.990000, 1.000000, -2.050000) frog"));

	// Vector4
	format = "fish %v frog";
	args.clear();
	args.push_back(Variant(Vector4(19.99, 1.00, -2.05, 5.5)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.990000, 1.000000, -2.050000, 5.500000) frog"));

	// Vector with negative values.
	format = "fish %v frog";
	args.clear();
	args.push_back(Variant(Vector2(-19.99, -1.00)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (-19.990000, -1.000000) frog"));

	// Vector left-padded.
	format = "fish %11v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (  19.990000,    1.000000,   -2.050000) frog"));

	// Vector left-padded with inf/nan
	format = "fish %11v frog";
	args.clear();
	args.push_back(Variant(Vector2(INFINITY, NAN)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (        inf,         nan) frog"));

	// Vector right-padded.
	format = "fish %-11v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.990000  , 1.000000   , -2.050000  ) frog"));

	// Vector left-padded with zeros.
	format = "fish %011v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (0019.990000, 0001.000000, -002.050000) frog"));

	// Vector given Vector3i.
	format = "fish %v frog";
	args.clear();
	args.push_back(Variant(Vector3i(19, 1, -2)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.000000, 1.000000, -2.000000) frog"));

	// Vector with 1 decimal.
	format = "fish %.1v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (20.0, 1.0, -2.0) frog"));

	// Vector with 12 decimals.
	format = "fish %.12v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.00, 1.00, -2.00)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (19.000000000000, 1.000000000000, -2.000000000000) frog"));

	// Vector with no decimals.
	format = "fish %.v frog";
	args.clear();
	args.push_back(Variant(Vector3(19.99, 1.00, -2.05)));
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish (20, 1, -2) frog"));

	///// Strings

	// String
	format = "fish %s frog";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish cheese frog"));

	// String left-padded.
	format = "fish %10s frog";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish     cheese frog"));

	// String right-padded.
	format = "fish %-10s frog";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish cheese     frog"));

	///// Characters

	// Character as string.
	format = "fish %c frog";
	args.clear();
	args.push_back("A");
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish A frog"));

	// Character as int.
	format = "fish %c frog";
	args.clear();
	args.push_back(65);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish A frog"));

	///// Dynamic width

	// String dynamic width.
	format = "fish %*s frog";
	args.clear();
	args.push_back(10);
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	REQUIRE(output == String("fish     cheese frog"));

	// Int dynamic width.
	format = "fish %*d frog";
	args.clear();
	args.push_back(10);
	args.push_back(99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	REQUIRE(output == String("fish         99 frog"));

	// Float dynamic width.
	format = "fish %*.*f frog";
	args.clear();
	args.push_back(10);
	args.push_back(3);
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error == false);
	CHECK(output == String("fish     99.990 frog"));

	///// Errors

	// More formats than arguments.
	format = "fish %s %s frog";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "not enough arguments for format string");

	// More arguments than formats.
	format = "fish %s frog";
	args.clear();
	args.push_back("hello");
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "not all arguments converted during string formatting");

	// Incomplete format.
	format = "fish %10";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "incomplete format");

	// Bad character in format string.
	format = "fish %&f frog";
	args.clear();
	args.push_back("cheese");
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "unsupported format character");

	// Too many decimals.
	format = "fish %2.2.2f frog";
	args.clear();
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "too many decimal points in format");

	// * not a number or vector.
	format = "fish %*f frog";
	args.clear();
	args.push_back("cheese");
	args.push_back(99.99);
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "* wants number or vector");

	// Character too long.
	format = "fish %c frog";
	args.clear();
	args.push_back("sc");
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "%c requires number or single-character string");

	// Character bad type.
	format = "fish %c frog";
	args.clear();
	args.push_back(Array());
	output = format.sprintf(args, &error);
	REQUIRE(error);
	CHECK(output == "%c requires number or single-character string");
}

TEST_CASE("[String] is_numeric") {
	CHECK(String("12").is_numeric());
	CHECK(String("1.2").is_numeric());
	CHECK(!String("AF").is_numeric());
	CHECK(String("-12").is_numeric());
	CHECK(String("-1.2").is_numeric());
}

TEST_CASE("[String] pad") {
	String s = String("test");
	CHECK(s.lpad(10, "x") == U"xxxxxxtest");
	CHECK(s.rpad(10, "x") == U"testxxxxxx");

	s = String("10.10");
	CHECK(s.pad_decimals(4) == U"10.1000");
	CHECK(s.pad_decimals(1) == U"10.1");
	CHECK(s.pad_zeros(4) == U"0010.10");
	CHECK(s.pad_zeros(1) == U"10.10");
}

TEST_CASE("[String] is_subsequence_of") {
	String a = "is subsequence of";
	CHECK(String("sub").is_subsequence_of(a));
	CHECK(!String("Sub").is_subsequence_of(a));
	CHECK(String("Sub").is_subsequence_ofn(a));
}

TEST_CASE("[String] match") {
	CHECK(String("img1.png").match("*.png"));
	CHECK(!String("img1.jpeg").match("*.png"));
	CHECK(!String("img1.Png").match("*.png"));
	CHECK(String("img1.Png").matchn("*.png"));
}

TEST_CASE("[String] IPVX address to string") {
	IPAddress ip0("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
	IPAddress ip(0x0123, 0x4567, 0x89ab, 0xcdef, true);
	IPAddress ip2("fe80::52e5:49ff:fe93:1baf");
	IPAddress ip3("::ffff:192.168.0.1");
	String ip4 = "192.168.0.1";
	CHECK(ip4.is_valid_ip_address());

	ip4 = "192.368.0.1";
	CHECK(!ip4.is_valid_ip_address());

	String ip6 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
	CHECK(ip6.is_valid_ip_address());

	ip6 = "2001:0db8:85j3:0000:0000:8a2e:0370:7334";
	CHECK(!ip6.is_valid_ip_address());

	ip6 = "2001:0db8:85f345:0000:0000:8a2e:0370:7334";
	CHECK(!ip6.is_valid_ip_address());

	ip6 = "2001:0db8::0:8a2e:370:7334";
	CHECK(ip6.is_valid_ip_address());

	ip6 = "::ffff:192.168.0.1";
	CHECK(ip6.is_valid_ip_address());
}

TEST_CASE("[String] Capitalize against many strings") {
	String input = "2D";
	String output = "2d";
	CHECK(input.capitalize() == output);

	input = "2d";
	output = "2d";
	CHECK(input.capitalize() == output);

	input = "2db";
	output = "2 Db";
	CHECK(input.capitalize() == output);

	input = "HTML5 Html5 html5 html_5";
	output = "Html 5 Html 5 Html 5 Html 5";
	CHECK(input.capitalize() == output);

	input = "Node2D Node2d NODE2D NODE_2D node_2d";
	output = "Node 2d Node 2d Node 2d Node 2d Node 2d";
	CHECK(input.capitalize() == output);

	input = "Node2DPosition";
	output = "Node 2d Position";
	CHECK(input.capitalize() == output);

	input = "Number2Digits";
	output = "Number 2 Digits";
	CHECK(input.capitalize() == output);

	input = "bytes2var";
	output = "Bytes 2 Var";
	CHECK(input.capitalize() == output);

	input = "linear2db";
	output = "Linear 2 Db";
	CHECK(input.capitalize() == output);

	input = "vector3";
	output = "Vector 3";
	CHECK(input.capitalize() == output);

	input = "sha256";
	output = "Sha 256";
	CHECK(input.capitalize() == output);

	input = "PascalCase";
	output = "Pascal Case";
	CHECK(input.capitalize() == output);

	input = "PascalPascalCase";
	output = "Pascal Pascal Case";
	CHECK(input.capitalize() == output);

	input = "snake_case";
	output = "Snake Case";
	CHECK(input.capitalize() == output);

	input = "snake_snake_case";
	output = "Snake Snake Case";
	CHECK(input.capitalize() == output);

	input = "sha256sum";
	output = "Sha 256 Sum";
	CHECK(input.capitalize() == output);

	input = "cat2dog";
	output = "Cat 2 Dog";
	CHECK(input.capitalize() == output);

	input = "function(name)";
	output = "Function(name)";
	CHECK(input.capitalize() == output);

	input = "snake_case_function(snake_case_arg)";
	output = "Snake Case Function(snake Case Arg)";
	CHECK(input.capitalize() == output);

	input = "snake_case_function( snake_case_arg )";
	output = "Snake Case Function( Snake Case Arg )";
	CHECK(input.capitalize() == output);

	input = U"словоСлово_слово слово";
	output = U"Слово Слово Слово Слово";
	CHECK(input.capitalize() == output);

	input = U"λέξηΛέξη_λέξη λέξη";
	output = U"Λέξη Λέξη Λέξη Λέξη";
	CHECK(input.capitalize() == output);

	input = U"բառԲառ_բառ բառ";
	output = U"Բառ Բառ Բառ Բառ";
	CHECK(input.capitalize() == output);
}

struct StringCasesTestCase {
	const char32_t *input;
	const char32_t *camel_case;
	const char32_t *pascal_case;
	const char32_t *snake_case;
};

TEST_CASE("[String] Checking case conversion methods") {
	StringCasesTestCase test_cases[] = {
		/* clang-format off */
		{ U"2D",                     U"2d",                   U"2d",                   U"2d"                      },
		{ U"2d",                     U"2d",                   U"2d",                   U"2d"                      },
		{ U"2db",                    U"2Db",                  U"2Db",                  U"2_db"                    },
		{ U"Vector3",                U"vector3",              U"Vector3",              U"vector_3"                },
		{ U"sha256",                 U"sha256",               U"Sha256",               U"sha_256"                 },
		{ U"Node2D",                 U"node2d",               U"Node2d",               U"node_2d"                 },
		{ U"RichTextLabel",          U"richTextLabel",        U"RichTextLabel",        U"rich_text_label"         },
		{ U"HTML5",                  U"html5",                U"Html5",                U"html_5"                  },
		{ U"Node2DPosition",         U"node2dPosition",       U"Node2dPosition",       U"node_2d_position"        },
		{ U"Number2Digits",          U"number2Digits",        U"Number2Digits",        U"number_2_digits"         },
		{ U"get_property_list",      U"getPropertyList",      U"GetPropertyList",      U"get_property_list"       },
		{ U"get_camera_2d",          U"getCamera2d",          U"GetCamera2d",          U"get_camera_2d"           },
		{ U"_physics_process",       U"physicsProcess",       U"PhysicsProcess",       U"_physics_process"        },
		{ U"bytes2var",              U"bytes2Var",            U"Bytes2Var",            U"bytes_2_var"             },
		{ U"linear2db",              U"linear2Db",            U"Linear2Db",            U"linear_2_db"             },
		{ U"sha256sum",              U"sha256Sum",            U"Sha256Sum",            U"sha_256_sum"             },
		{ U"camelCase",              U"camelCase",            U"CamelCase",            U"camel_case"              },
		{ U"PascalCase",             U"pascalCase",           U"PascalCase",           U"pascal_case"             },
		{ U"snake_case",             U"snakeCase",            U"SnakeCase",            U"snake_case"              },
		{ U"Test TEST test",         U"testTestTest",         U"TestTestTest",         U"test_test_test"          },
		{ U"словоСлово_слово слово", U"словоСловоСловоСлово", U"СловоСловоСловоСлово", U"слово_слово_слово_слово" },
		{ U"λέξηΛέξη_λέξη λέξη",     U"λέξηΛέξηΛέξηΛέξη",     U"ΛέξηΛέξηΛέξηΛέξη",     U"λέξη_λέξη_λέξη_λέξη"     },
		{ U"բառԲառ_բառ բառ",         U"բառԲառԲառԲառ",         U"ԲառԲառԲառԲառ",         U"բառ_բառ_բառ_բառ"         },
		{ nullptr,                   nullptr,                 nullptr,                 nullptr                    },
		/* clang-format on */
	};

	int idx = 0;
	while (test_cases[idx].input != nullptr) {
		String input = test_cases[idx].input;
		CHECK(input.to_camel_case() == test_cases[idx].camel_case);
		CHECK(input.to_pascal_case() == test_cases[idx].pascal_case);
		CHECK(input.to_snake_case() == test_cases[idx].snake_case);
		idx++;
	}
}

TEST_CASE("[String] Checking string is empty when it should be") {
	bool state = true;
	bool success;

	String a = "";
	success = a[0] == 0;
	if (!success) {
		state = false;
	}
	String b = "Godot";
	success = b[b.size()] == 0;
	if (!success) {
		state = false;
	}
	const String c = "";
	success = c[0] == 0;
	if (!success) {
		state = false;
	}

	const String d = "Godot";
	success = d[d.size()] == 0;
	if (!success) {
		state = false;
	}

	CHECK(state);
}

TEST_CASE("[String] lstrip and rstrip") {
#define STRIP_TEST(x)             \
	{                             \
		bool success = x;         \
		state = state && success; \
	}

	bool state = true;

	// strip none
	STRIP_TEST(String("abc").lstrip("") == "abc");
	STRIP_TEST(String("abc").rstrip("") == "abc");
	// strip one
	STRIP_TEST(String("abc").lstrip("a") == "bc");
	STRIP_TEST(String("abc").rstrip("c") == "ab");
	// strip lots
	STRIP_TEST(String("bababbababccc").lstrip("ab") == "ccc");
	STRIP_TEST(String("aaabcbcbcbbcbbc").rstrip("cb") == "aaa");
	// strip empty string
	STRIP_TEST(String("").lstrip("") == "");
	STRIP_TEST(String("").rstrip("") == "");
	// strip to empty string
	STRIP_TEST(String("abcabcabc").lstrip("bca") == "");
	STRIP_TEST(String("abcabcabc").rstrip("bca") == "");
	// don't strip wrong end
	STRIP_TEST(String("abc").lstrip("c") == "abc");
	STRIP_TEST(String("abca").lstrip("a") == "bca");
	STRIP_TEST(String("abc").rstrip("a") == "abc");
	STRIP_TEST(String("abca").rstrip("a") == "abc");
	// in utf-8 "¿" (\u00bf) has the same first byte as "µ" (\u00b5)
	// and the same second as "ÿ" (\u00ff)
	STRIP_TEST(String::utf8("¿").lstrip(String::utf8("µÿ")) == String::utf8("¿"));
	STRIP_TEST(String::utf8("¿").rstrip(String::utf8("µÿ")) == String::utf8("¿"));
	STRIP_TEST(String::utf8("µ¿ÿ").lstrip(String::utf8("µÿ")) == String::utf8("¿ÿ"));
	STRIP_TEST(String::utf8("µ¿ÿ").rstrip(String::utf8("µÿ")) == String::utf8("µ¿"));

	// the above tests repeated with additional superfluous strip chars

	// strip none
	STRIP_TEST(String("abc").lstrip("qwjkl") == "abc");
	STRIP_TEST(String("abc").rstrip("qwjkl") == "abc");
	// strip one
	STRIP_TEST(String("abc").lstrip("qwajkl") == "bc");
	STRIP_TEST(String("abc").rstrip("qwcjkl") == "ab");
	// strip lots
	STRIP_TEST(String("bababbababccc").lstrip("qwabjkl") == "ccc");
	STRIP_TEST(String("aaabcbcbcbbcbbc").rstrip("qwcbjkl") == "aaa");
	// strip empty string
	STRIP_TEST(String("").lstrip("qwjkl") == "");
	STRIP_TEST(String("").rstrip("qwjkl") == "");
	// strip to empty string
	STRIP_TEST(String("abcabcabc").lstrip("qwbcajkl") == "");
	STRIP_TEST(String("abcabcabc").rstrip("qwbcajkl") == "");
	// don't strip wrong end
	STRIP_TEST(String("abc").lstrip("qwcjkl") == "abc");
	STRIP_TEST(String("abca").lstrip("qwajkl") == "bca");
	STRIP_TEST(String("abc").rstrip("qwajkl") == "abc");
	STRIP_TEST(String("abca").rstrip("qwajkl") == "abc");
	// in utf-8 "¿" (\u00bf) has the same first byte as "µ" (\u00b5)
	// and the same second as "ÿ" (\u00ff)
	STRIP_TEST(String::utf8("¿").lstrip(String::utf8("qwaµÿjkl")) == String::utf8("¿"));
	STRIP_TEST(String::utf8("¿").rstrip(String::utf8("qwaµÿjkl")) == String::utf8("¿"));
	STRIP_TEST(String::utf8("µ¿ÿ").lstrip(String::utf8("qwaµÿjkl")) == String::utf8("¿ÿ"));
	STRIP_TEST(String::utf8("µ¿ÿ").rstrip(String::utf8("qwaµÿjkl")) == String::utf8("µ¿"));

	CHECK(state);

#undef STRIP_TEST
}

TEST_CASE("[String] Ensuring empty string into parse_utf8 passes empty string") {
	String empty;
	CHECK(empty.parse_utf8(nullptr, -1) == ERR_INVALID_DATA);
}

TEST_CASE("[String] Cyrillic to_lower()") {
	String upper = U"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
	String lower = U"абвгдеёжзийклмнопрстуфхцчшщъыьэюя";

	String test = upper.to_lower();

	bool state = test == lower;

	CHECK(state);
}

TEST_CASE("[String] Count and countn functionality") {
	String s = String("");
	MULTICHECK_STRING_EQ(s, count, "Test", 0);

	s = "Test";
	MULTICHECK_STRING_EQ(s, count, "", 0);

	s = "Test";
	MULTICHECK_STRING_EQ(s, count, "test", 0);

	s = "Test";
	MULTICHECK_STRING_EQ(s, count, "TEST", 0);

	s = "TEST";
	MULTICHECK_STRING_EQ(s, count, "TEST", 1);

	s = "Test";
	MULTICHECK_STRING_EQ(s, count, "Test", 1);

	s = "aTest";
	MULTICHECK_STRING_EQ(s, count, "Test", 1);

	s = "Testa";
	MULTICHECK_STRING_EQ(s, count, "Test", 1);

	s = "TestTestTest";
	MULTICHECK_STRING_EQ(s, count, "Test", 3);

	s = "TestTestTest";
	MULTICHECK_STRING_EQ(s, count, "TestTest", 1);

	s = "TestGodotTestGodotTestGodot";
	MULTICHECK_STRING_EQ(s, count, "Test", 3);

	s = "TestTestTestTest";
	MULTICHECK_STRING_INT_INT_EQ(s, count, "Test", 4, 8, 1);

	s = "TestTestTestTest";
	MULTICHECK_STRING_INT_INT_EQ(s, count, "Test", 4, 12, 2);

	s = "TestTestTestTest";
	MULTICHECK_STRING_INT_INT_EQ(s, count, "Test", 4, 16, 3);

	s = "TestTestTestTest";
	MULTICHECK_STRING_INT_EQ(s, count, "Test", 4, 3);

	s = "Test";
	MULTICHECK_STRING_EQ(s, countn, "test", 1);

	s = "Test";
	MULTICHECK_STRING_EQ(s, countn, "TEST", 1);

	s = "testTest-Testatest";
	MULTICHECK_STRING_EQ(s, countn, "tEst", 4);

	s = "testTest-TeStatest";
	MULTICHECK_STRING_INT_INT_EQ(s, countn, "tEsT", 4, 16, 2);
}

TEST_CASE("[String] Bigrams") {
	String s = "abcd";
	Vector<String> bigr = s.bigrams();

	CHECK(bigr.size() == 3);
	CHECK(bigr[0] == "ab");
	CHECK(bigr[1] == "bc");
	CHECK(bigr[2] == "cd");
}

TEST_CASE("[String] c-escape/unescape") {
	String s = "\\1\a2\b\f3\n45\r6\t7\v8\'9\?0\"";
	CHECK(s.c_escape().c_unescape() == s);
}

TEST_CASE("[String] indent") {
	static const char *input[] = {
		"",
		"aaa\nbbb",
		"\tcontains\n\tindent",
		"empty\n\nline",
	};
	static const char *expected[] = {
		"",
		"\taaa\n\tbbb",
		"\t\tcontains\n\t\tindent",
		"\tempty\n\n\tline",
	};

	for (int i = 0; i < 3; i++) {
		CHECK(String(input[i]).indent("\t") == expected[i]);
	}
}

TEST_CASE("[String] dedent") {
	String s = "      aaa\n    bbb";
	String t = "aaa\nbbb";
	CHECK(s.dedent() == t);
}

TEST_CASE("[String] Path functions") {
	static const char *path[8] = { "C:\\Godot\\project\\test.tscn", "/Godot/project/test.xscn", "../Godot/project/test.scn", "Godot\\test.doc", "C:\\test.", "res://test", "user://test", "/.test" };
	static const char *base_dir[8] = { "C:\\Godot\\project", "/Godot/project", "../Godot/project", "Godot", "C:\\", "res://", "user://", "/" };
	static const char *base_name[8] = { "C:\\Godot\\project\\test", "/Godot/project/test", "../Godot/project/test", "Godot\\test", "C:\\test", "res://test", "user://test", "/" };
	static const char *ext[8] = { "tscn", "xscn", "scn", "doc", "", "", "", "test" };
	static const char *file[8] = { "test.tscn", "test.xscn", "test.scn", "test.doc", "test.", "test", "test", ".test" };
	static const char *simplified[8] = { "C:/Godot/project/test.tscn", "/Godot/project/test.xscn", "../Godot/project/test.scn", "Godot/test.doc", "C:/test.", "res://test", "user://test", "/.test" };
	static const bool abs[8] = { true, true, false, false, true, true, true, true };

	for (int i = 0; i < 8; i++) {
		CHECK(String(path[i]).get_base_dir() == base_dir[i]);
		CHECK(String(path[i]).get_basename() == base_name[i]);
		CHECK(String(path[i]).get_extension() == ext[i]);
		CHECK(String(path[i]).get_file() == file[i]);
		CHECK(String(path[i]).is_absolute_path() == abs[i]);
		CHECK(String(path[i]).is_relative_path() != abs[i]);
		CHECK(String(path[i]).simplify_path() == String(simplified[i]));
		CHECK(String(path[i]).simplify_path().get_base_dir().path_join(file[i]) == String(path[i]).simplify_path());
	}

	static const char *file_name[3] = { "test.tscn", "test://.xscn", "?tes*t.scn" };
	static const bool valid[3] = { true, false, false };
	for (int i = 0; i < 3; i++) {
		CHECK(String(file_name[i]).is_valid_filename() == valid[i]);
	}
}

TEST_CASE("[String] hash") {
	String a = "Test";
	String b = "Test";
	String c = "West";
	CHECK(a.hash() == b.hash());
	CHECK(a.hash() != c.hash());

	CHECK(a.hash64() == b.hash64());
	CHECK(a.hash64() != c.hash64());
}

TEST_CASE("[String] uri_encode/unescape") {
	String s = "Godot Engine:'docs'";
	String t = "Godot%20Engine%3A%27docs%27";

	String x1 = "T%C4%93%C5%A1t";
	static const uint8_t u8str[] = { 0x54, 0xC4, 0x93, 0xC5, 0xA1, 0x74, 0x00 };
	String x2 = String::utf8((const char *)u8str);
	String x3 = U"Tēšt";

	CHECK(x1.uri_decode() == x2);
	CHECK(x1.uri_decode() == x3);
	CHECK((x1 + x3).uri_decode() == (x2 + x3)); // Mixed unicode and URL encoded string, e.g. GTK+ bookmark.
	CHECK(x2.uri_encode() == x1);
	CHECK(x3.uri_encode() == x1);

	CHECK(s.uri_encode() == t);
	CHECK(t.uri_decode() == s);
}

TEST_CASE("[String] xml_escape/unescape") {
	String s = "\"Test\" <test@test&'test'>";
	CHECK(s.xml_escape(true).xml_unescape() == s);
	CHECK(s.xml_escape(false).xml_unescape() == s);
}

TEST_CASE("[String] xml_unescape") {
	// Named entities
	String input = "&quot;&amp;&apos;&lt;&gt;";
	CHECK(input.xml_unescape() == "\"&\'<>");

	// Numeric entities
	input = "&#x41;&#66;";
	CHECK(input.xml_unescape() == "AB");

	input = "&#0;&x#0;More text";
	String result = input.xml_unescape();
	// Didn't put in a leading NUL and terminate the string
	CHECK(input.length() > 0);
	CHECK(input[0] != '\0');
	// Entity should be left as-is if invalid
	CHECK(input.xml_unescape() == input);

	// Check near char32_t range
	input = "&#xFFFFFFFF;";
	result = input.xml_unescape();
	CHECK(result.length() == 1);
	CHECK(result[0] == 0xFFFFFFFF);
	input = "&#4294967295;";
	result = input.xml_unescape();
	CHECK(result.length() == 1);
	CHECK(result[0] == 0xFFFFFFFF);

	// Check out of range of char32_t
	input = "&#xFFFFFFFFF;";
	CHECK(input.xml_unescape() == input);
	input = "&#4294967296;";
	CHECK(input.xml_unescape() == input);

	// Shouldn't consume without ending in a ';'
	input = "&#66";
	CHECK(input.xml_unescape() == input);
	input = "&#x41";
	CHECK(input.xml_unescape() == input);

	// Invalid characters should make the entity ignored
	input = "&#x41SomeIrrelevantText;";
	CHECK(input.xml_unescape() == input);
	input = "&#66SomeIrrelevantText;";
	CHECK(input.xml_unescape() == input);
}

TEST_CASE("[String] Strip escapes") {
	String s = "\t\tTest Test\r\n Test";
	CHECK(s.strip_escapes() == "Test Test Test");
}

TEST_CASE("[String] Similarity") {
	String a = "Test";
	String b = "West";
	String c = "Toad";
	CHECK(a.similarity(b) > a.similarity(c));
}

TEST_CASE("[String] Strip edges") {
	String s = "\t Test Test   ";
	CHECK(s.strip_edges(true, false) == "Test Test   ");
	CHECK(s.strip_edges(false, true) == "\t Test Test");
	CHECK(s.strip_edges(true, true) == "Test Test");
}

TEST_CASE("[String] Trim") {
	String s = "aaaTestbbb";
	MULTICHECK_STRING_EQ(s, trim_prefix, "aaa", "Testbbb");
	MULTICHECK_STRING_EQ(s, trim_prefix, "Test", s);
	MULTICHECK_STRING_EQ(s, trim_prefix, "", s);
	MULTICHECK_STRING_EQ(s, trim_prefix, "aaaTestbbb", "");
	MULTICHECK_STRING_EQ(s, trim_prefix, "bbb", s);
	MULTICHECK_STRING_EQ(s, trim_prefix, "AAA", s);

	MULTICHECK_STRING_EQ(s, trim_suffix, "bbb", "aaaTest");
	MULTICHECK_STRING_EQ(s, trim_suffix, "Test", s);
	MULTICHECK_STRING_EQ(s, trim_suffix, "", s);
	MULTICHECK_STRING_EQ(s, trim_suffix, "aaaTestbbb", "");
	MULTICHECK_STRING_EQ(s, trim_suffix, "aaa", s);
	MULTICHECK_STRING_EQ(s, trim_suffix, "BBB", s);
}

TEST_CASE("[String] Right/Left") {
	String s = "aaaTestbbb";
	//                ^
	CHECK(s.right(6) == "estbbb");
	CHECK(s.right(-6) == "tbbb");
	CHECK(s.left(6) == "aaaTes");
	CHECK(s.left(-6) == "aaaT");
}

TEST_CASE("[String] Repeat") {
	String s = "abababab";
	String x = "ab";
	String t = x.repeat(4);
	CHECK(t == s);
}

TEST_CASE("[String] Reverse") {
	String s = "Abcd";
	CHECK(s.reverse() == "dcbA");
}

TEST_CASE("[String] SHA1/SHA256/MD5") {
	String s = "Godot";
	String sha1 = "a1e91f39b9fce6a9998b14bdbe2aa2b39dc2d201";
	static uint8_t sha1_buf[20] = {
		0xA1, 0xE9, 0x1F, 0x39, 0xB9, 0xFC, 0xE6, 0xA9, 0x99, 0x8B, 0x14, 0xBD, 0xBE, 0x2A, 0xA2, 0xB3,
		0x9D, 0xC2, 0xD2, 0x01
	};
	String sha256 = "2a02b2443f7985d89d09001086ae3dcfa6eb0f55c6ef170715d42328e16e6cb8";
	static uint8_t sha256_buf[32] = {
		0x2A, 0x02, 0xB2, 0x44, 0x3F, 0x79, 0x85, 0xD8, 0x9D, 0x09, 0x00, 0x10, 0x86, 0xAE, 0x3D, 0xCF,
		0xA6, 0xEB, 0x0F, 0x55, 0xC6, 0xEF, 0x17, 0x07, 0x15, 0xD4, 0x23, 0x28, 0xE1, 0x6E, 0x6C, 0xB8
	};
	String md5 = "4a336d087aeb0390da10ee2ea7cb87f8";
	static uint8_t md5_buf[16] = {
		0x4A, 0x33, 0x6D, 0x08, 0x7A, 0xEB, 0x03, 0x90, 0xDA, 0x10, 0xEE, 0x2E, 0xA7, 0xCB, 0x87, 0xF8
	};

	PackedByteArray buf = s.sha1_buffer();
	CHECK(memcmp(sha1_buf, buf.ptr(), 20) == 0);
	CHECK(s.sha1_text() == sha1);

	buf = s.sha256_buffer();
	CHECK(memcmp(sha256_buf, buf.ptr(), 32) == 0);
	CHECK(s.sha256_text() == sha256);

	buf = s.md5_buffer();
	CHECK(memcmp(md5_buf, buf.ptr(), 16) == 0);
	CHECK(s.md5_text() == md5);
}

TEST_CASE("[String] Join") {
	String comma = ", ";
	String empty = "";
	Vector<String> parts;

	CHECK(comma.join(parts) == "");
	CHECK(empty.join(parts) == "");

	parts.push_back("One");
	CHECK(comma.join(parts) == "One");
	CHECK(empty.join(parts) == "One");

	parts.push_back("B");
	parts.push_back("C");
	CHECK(comma.join(parts) == "One, B, C");
	CHECK(empty.join(parts) == "OneBC");

	parts.push_back("");
	CHECK(comma.join(parts) == "One, B, C, ");
	CHECK(empty.join(parts) == "OneBC");
}

TEST_CASE("[String] Is_*") {
	static const char *data[13] = { "-30", "100", "10.1", "10,1", "1e2", "1e-2", "1e2e3", "0xAB", "AB", "Test1", "1Test", "Test*1", "文字" };
	static bool isnum[13] = { true, true, true, false, false, false, false, false, false, false, false, false, false };
	static bool isint[13] = { true, true, false, false, false, false, false, false, false, false, false, false, false };
	static bool ishex[13] = { true, true, false, false, true, false, true, false, true, false, false, false, false };
	static bool ishex_p[13] = { false, false, false, false, false, false, false, true, false, false, false, false, false };
	static bool isflt[13] = { true, true, true, false, true, true, false, false, false, false, false, false, false };
	static bool isaid[13] = { false, false, false, false, false, false, false, false, true, true, false, false, false };
	static bool isuid[13] = { false, false, false, false, false, false, false, false, true, true, false, false, true };
	for (int i = 0; i < 12; i++) {
		String s = String::utf8(data[i]);
		CHECK(s.is_numeric() == isnum[i]);
		CHECK(s.is_valid_int() == isint[i]);
		CHECK(s.is_valid_hex_number(false) == ishex[i]);
		CHECK(s.is_valid_hex_number(true) == ishex_p[i]);
		CHECK(s.is_valid_float() == isflt[i]);
		CHECK(s.is_valid_ascii_identifier() == isaid[i]);
		CHECK(s.is_valid_unicode_identifier() == isuid[i]);
	}
}

TEST_CASE("[String] humanize_size") {
	CHECK(String::humanize_size(1000) == "1000 B");
	CHECK(String::humanize_size(1025) == "1.00 KiB");
	CHECK(String::humanize_size(1025300) == "1001.2 KiB");
	CHECK(String::humanize_size(100523550) == "95.86 MiB");
	CHECK(String::humanize_size(5345555000) == "4.97 GiB");
}

TEST_CASE("[String] validate_node_name") {
	String numeric_only = "12345";
	CHECK(numeric_only.validate_node_name() == "12345");

	String name_with_spaces = "Name with spaces";
	CHECK(name_with_spaces.validate_node_name() == "Name with spaces");

	String name_with_kana = U"Name with kana ゴドツ";
	CHECK(name_with_kana.validate_node_name() == U"Name with kana ゴドツ");

	String name_with_invalid_chars = "Name with invalid characters :.@%removed!";
	CHECK(name_with_invalid_chars.validate_node_name() == "Name with invalid characters ____removed!");
}

TEST_CASE("[String] validate_ascii_identifier") {
	String empty_string;
	CHECK(empty_string.validate_ascii_identifier() == "_");

	String numeric_only = "12345";
	CHECK(numeric_only.validate_ascii_identifier() == "_12345");

	String name_with_spaces = "Name with spaces";
	CHECK(name_with_spaces.validate_ascii_identifier() == "Name_with_spaces");

	String name_with_invalid_chars = U"Invalid characters:@*#&世界";
	CHECK(name_with_invalid_chars.validate_ascii_identifier() == "Invalid_characters_______");
}

TEST_CASE("[String] validate_unicode_identifier") {
	String empty_string;
	CHECK(empty_string.validate_unicode_identifier() == "_");

	String numeric_only = "12345";
	CHECK(numeric_only.validate_unicode_identifier() == "_12345");

	String name_with_spaces = "Name with spaces";
	CHECK(name_with_spaces.validate_unicode_identifier() == "Name_with_spaces");

	String name_with_invalid_chars = U"Invalid characters:@*#&世界";
	CHECK(name_with_invalid_chars.validate_unicode_identifier() == U"Invalid_characters_____世界");
}

TEST_CASE("[String] Variant indexed get") {
	Variant s = String("abcd");
	bool valid = false;
	bool oob = true;

	String r = s.get_indexed(1, valid, oob);

	CHECK(valid);
	CHECK_FALSE(oob);
	CHECK_EQ(r, String("b"));
}

TEST_CASE("[String] Variant validated indexed get") {
	Variant s = String("abcd");

	Variant::ValidatedIndexedGetter getter = Variant::get_member_validated_indexed_getter(Variant::STRING);

	Variant r;
	bool oob = true;
	getter(&s, 1, &r, &oob);

	CHECK_FALSE(oob);
	CHECK_EQ(r, String("b"));
}

TEST_CASE("[String] Variant ptr indexed get") {
	String s("abcd");

	Variant::PTRIndexedGetter getter = Variant::get_member_ptr_indexed_getter(Variant::STRING);

	String r;
	getter(&s, 1, &r);

	CHECK_EQ(r, String("b"));
}

TEST_CASE("[String] Variant indexed set") {
	Variant s = String("abcd");
	bool valid = false;
	bool oob = true;

	s.set_indexed(1, String("z"), valid, oob);

	CHECK(valid);
	CHECK_FALSE(oob);
	CHECK_EQ(s, String("azcd"));
}

TEST_CASE("[String] Variant validated indexed set") {
	Variant s = String("abcd");

	Variant::ValidatedIndexedSetter setter = Variant::get_member_validated_indexed_setter(Variant::STRING);

	Variant v = String("z");
	bool oob = true;
	setter(&s, 1, &v, &oob);

	CHECK_FALSE(oob);
	CHECK_EQ(s, String("azcd"));
}

TEST_CASE("[String] Variant ptr indexed set") {
	String s("abcd");

	Variant::PTRIndexedSetter setter = Variant::get_member_ptr_indexed_setter(Variant::STRING);

	String v("z");
	setter(&s, 1, &v);

	CHECK_EQ(s, String("azcd"));
}

TEST_CASE("[String][URL] Parse URL") {
#define CHECK_URL(m_url_to_parse, m_expected_schema, m_expected_host, m_expected_port, m_expected_path, m_expected_fragment, m_expected_error) \
	if (true) {                                                                                                                                \
		int port;                                                                                                                              \
		String url(m_url_to_parse), schema, host, path, fragment;                                                                              \
                                                                                                                                               \
		CHECK_EQ(url.parse_url(schema, host, port, path, fragment), m_expected_error);                                                         \
		CHECK_EQ(schema, m_expected_schema);                                                                                                   \
		CHECK_EQ(host, m_expected_host);                                                                                                       \
		CHECK_EQ(path, m_expected_path);                                                                                                       \
		CHECK_EQ(fragment, m_expected_fragment);                                                                                               \
		CHECK_EQ(port, m_expected_port);                                                                                                       \
	} else                                                                                                                                     \
		((void)0)

	// All elements.
	CHECK_URL("https://www.example.com:8080/path/to/file.html#fragment", "https://", "www.example.com", 8080, "/path/to/file.html", "fragment", Error::OK);

	// Valid URLs.
	CHECK_URL("https://godotengine.org", "https://", "godotengine.org", 0, "", "", Error::OK);
	CHECK_URL("https://godotengine.org/", "https://", "godotengine.org", 0, "/", "", Error::OK);
	CHECK_URL("godotengine.org/", "", "godotengine.org", 0, "/", "", Error::OK);
	CHECK_URL("HTTPS://godotengine.org/", "https://", "godotengine.org", 0, "/", "", Error::OK);
	CHECK_URL("https://GODOTENGINE.ORG/", "https://", "godotengine.org", 0, "/", "", Error::OK);
	CHECK_URL("http://godotengine.org", "http://", "godotengine.org", 0, "", "", Error::OK);
	CHECK_URL("https://godotengine.org:8080", "https://", "godotengine.org", 8080, "", "", Error::OK);
	CHECK_URL("https://godotengine.org/blog", "https://", "godotengine.org", 0, "/blog", "", Error::OK);
	CHECK_URL("https://godotengine.org/blog/", "https://", "godotengine.org", 0, "/blog/", "", Error::OK);
	CHECK_URL("https://docs.godotengine.org/en/stable", "https://", "docs.godotengine.org", 0, "/en/stable", "", Error::OK);
	CHECK_URL("https://docs.godotengine.org/en/stable/", "https://", "docs.godotengine.org", 0, "/en/stable/", "", Error::OK);
	CHECK_URL("https://me:secret@godotengine.org", "https://", "godotengine.org", 0, "", "", Error::OK);
	CHECK_URL("https://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]/ipv6", "https://", "fedc:ba98:7654:3210:fedc:ba98:7654:3210", 0, "/ipv6", "", Error::OK);

	// Scheme vs Fragment.
	CHECK_URL("google.com/#goto=http://redirect_url/", "", "google.com", 0, "/", "goto=http://redirect_url/", Error::OK);

	// Invalid URLs.

	// Invalid Scheme.
	CHECK_URL("https_://godotengine.org", "", "https_", 0, "//godotengine.org", "", Error::ERR_INVALID_PARAMETER);

	// Multiple ports.
	CHECK_URL("https://godotengine.org:8080:433", "https://", "", 0, "", "", Error::ERR_INVALID_PARAMETER);
	// Missing ] on literal IPv6.
	CHECK_URL("https://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210/ipv6", "https://", "", 0, "/ipv6", "", Error::ERR_INVALID_PARAMETER);
	// Missing host.
	CHECK_URL("https:///blog", "https://", "", 0, "/blog", "", Error::ERR_INVALID_PARAMETER);
	// Invalid ports.
	CHECK_URL("https://godotengine.org:notaport", "https://", "godotengine.org", 0, "", "", Error::ERR_INVALID_PARAMETER);
	CHECK_URL("https://godotengine.org:-8080", "https://", "godotengine.org", -8080, "", "", Error::ERR_INVALID_PARAMETER);
	CHECK_URL("https://godotengine.org:88888", "https://", "godotengine.org", 88888, "", "", Error::ERR_INVALID_PARAMETER);

#undef CHECK_URL
}

TEST_CASE("[Stress][String] Empty via ' == String()'") {
	for (int i = 0; i < 100000; ++i) {
		String str = "Hello World!";
		if (str == String()) {
			continue;
		}
	}
}

TEST_CASE("[Stress][String] Empty via `is_empty()`") {
	for (int i = 0; i < 100000; ++i) {
		String str = "Hello World!";
		if (str.is_empty()) {
			continue;
		}
	}
}
} // namespace TestString

#endif // TEST_STRING_H
