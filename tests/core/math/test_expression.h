/**************************************************************************/
/*  test_expression.h                                                     */
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

#include "core/math/expression.h"

#include "tests/test_macros.h"

namespace TestExpression {

TEST_CASE("[Expression] Integer arithmetic") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("-123456") == OK,
			"Integer identity should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == -123456,
			"Integer identity should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2 + 3") == OK,
			"Integer addition should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 5,
			"Integer addition should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("999999999999 + 999999999999") == OK,
			"Large integer addition should parse successfully.");
	CHECK_MESSAGE(
			int64_t(expression.execute()) == 1'999'999'999'998,
			"Large integer addition should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("25 / 10") == OK,
			"Integer / integer division should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 2,
			"Integer / integer division should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2 * (6 + 14) / 2 - 5") == OK,
			"Integer multiplication-addition-subtraction-division should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 15,
			"Integer multiplication-addition-subtraction-division should return the expected result.");
}

TEST_CASE("[Expression] Floating-point arithmetic") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("-123.456") == OK,
			"Float identity should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(-123.456),
			"Float identity should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2.0 + 3.0") == OK,
			"Float addition should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(5),
			"Float addition should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("3.0 / 10") == OK,
			"Float / integer division should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.3),
			"Float / integer division should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("3 / 10.0") == OK,
			"Basic integer / float division should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.3),
			"Basic integer / float division should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("3.0 / 10.0") == OK,
			"Float / float division should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.3),
			"Float / float division should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2.5 * (6.0 + 14.25) / 2.0 - 5.12345") == OK,
			"Float multiplication-addition-subtraction-division should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(20.18905),
			"Float multiplication-addition-subtraction-division should return the expected result.");
}

TEST_CASE("[Expression] Floating-point notation") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("2.") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(2.0),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("(2.)") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(2.0),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse(".3") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.3),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2.+5.") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(7.0),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse(".3-.8") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(-0.5),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2.+.2") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(2.2),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse(".0*0.") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.0),
			"The expression should return the expected result.");
}

TEST_CASE("[Expression] Scientific notation") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("2.e5") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("2.E5") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(200'000),
			"The expression should return the expected result.");

	// The middle "e" is ignored here.
	CHECK_MESSAGE(
			expression.parse("2e5") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(2e5),
			"The expression should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("2e.5") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(2),
			"The expression should return the expected result.");
}

TEST_CASE("[Expression] Underscored numeric literals") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("1_000_000") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("1_000.000") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("0xff_99_00") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("0Xff_99_00") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("0b10_11_00") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			expression.parse("0B10_11_00") == OK,
			"The expression should parse successfully.");
}

TEST_CASE("[Expression] Built-in functions") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("sqrt(pow(3, 2) + pow(4, 2))") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 5,
			"`sqrt(pow(3, 2) + pow(4, 2))` should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("snapped(sin(0.5), 0.01)") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			double(expression.execute()) == doctest::Approx(0.48),
			"`snapped(sin(0.5), 0.01)` should return the expected result.");

	CHECK_MESSAGE(
			expression.parse("pow(2.0, -2500)") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			Math::is_zero_approx(double(expression.execute())),
			"`pow(2.0, -2500)` should return the expected result (asymptotically zero).");
}

TEST_CASE("[Expression] Boolean expressions") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("24 >= 12") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			bool(expression.execute()),
			"The boolean expression should evaluate to `true`.");

	CHECK_MESSAGE(
			expression.parse("1.0 < 1.25 && 1.25 < 2.0") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			bool(expression.execute()),
			"The boolean expression should evaluate to `true`.");

	CHECK_MESSAGE(
			expression.parse("!2") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			!bool(expression.execute()),
			"The boolean expression should evaluate to `false`.");

	CHECK_MESSAGE(
			expression.parse("!!2") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			bool(expression.execute()),
			"The boolean expression should evaluate to `true`.");

	CHECK_MESSAGE(
			expression.parse("!0") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			bool(expression.execute()),
			"The boolean expression should evaluate to `true`.");

	CHECK_MESSAGE(
			expression.parse("!!0") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			!bool(expression.execute()),
			"The boolean expression should evaluate to `false`.");

	CHECK_MESSAGE(
			expression.parse("2 && 5") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			bool(expression.execute()),
			"The boolean expression should evaluate to `true`.");

	CHECK_MESSAGE(
			expression.parse("0 || 0") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			!bool(expression.execute()),
			"The boolean expression should evaluate to `false`.");

	CHECK_MESSAGE(
			expression.parse("(2 <= 4) && (2 > 5)") == OK,
			"The boolean expression should parse successfully.");
	CHECK_MESSAGE(
			!bool(expression.execute()),
			"The boolean expression should evaluate to `false`.");
}

TEST_CASE("[Expression] Expressions with variables") {
	Expression expression;

	PackedStringArray parameter_names;
	parameter_names.push_back("foo");
	parameter_names.push_back("bar");
	CHECK_MESSAGE(
			expression.parse("foo + bar + 50", parameter_names) == OK,
			"The expression should parse successfully.");
	Array values;
	values.push_back(60);
	values.push_back(20);
	CHECK_MESSAGE(
			int(expression.execute(values)) == 130,
			"The expression should return the expected value.");

	PackedStringArray parameter_names_invalid;
	parameter_names_invalid.push_back("foo");
	parameter_names_invalid.push_back("baz"); // Invalid parameter name.
	CHECK_MESSAGE(
			expression.parse("foo + bar + 50", parameter_names_invalid) == OK,
			"The expression should parse successfully.");
	Array values_invalid;
	values_invalid.push_back(60);
	values_invalid.push_back(20);
	// Invalid parameters will parse successfully but print an error message when executing.
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			int(expression.execute(values_invalid)) == 0,
			"The expression should return the expected value.");
	ERR_PRINT_ON;

	// Mismatched argument count (more values than parameters).
	PackedStringArray parameter_names_mismatch;
	parameter_names_mismatch.push_back("foo");
	parameter_names_mismatch.push_back("bar");
	CHECK_MESSAGE(
			expression.parse("foo + bar + 50", parameter_names_mismatch) == OK,
			"The expression should parse successfully.");
	Array values_mismatch;
	values_mismatch.push_back(60);
	values_mismatch.push_back(20);
	values_mismatch.push_back(110);
	CHECK_MESSAGE(
			int(expression.execute(values_mismatch)) == 130,
			"The expression should return the expected value.");

	// Mismatched argument count (more parameters than values).
	PackedStringArray parameter_names_mismatch2;
	parameter_names_mismatch2.push_back("foo");
	parameter_names_mismatch2.push_back("bar");
	parameter_names_mismatch2.push_back("baz");
	CHECK_MESSAGE(
			expression.parse("foo + bar + baz + 50", parameter_names_mismatch2) == OK,
			"The expression should parse successfully.");
	Array values_mismatch2;
	values_mismatch2.push_back(60);
	values_mismatch2.push_back(20);
	// Having more parameters than values will parse successfully but print an
	// error message when executing.
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			int(expression.execute(values_mismatch2)) == 0,
			"The expression should return the expected value.");
	ERR_PRINT_ON;
}

TEST_CASE("[Expression] Invalid expressions") {
	Expression expression;

	CHECK_MESSAGE(
			expression.parse("\\") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("0++") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("()") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("()()") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("() - ()") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("() * 12345") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("() * 12345") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("123'456") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");

	CHECK_MESSAGE(
			expression.parse("123\"456") == ERR_INVALID_PARAMETER,
			"The expression shouldn't parse successfully.");
}

TEST_CASE("[Expression] Unusual expressions") {
	Expression expression;

	// Redundant parentheses don't cause a parse error as long as they're matched.
	CHECK_MESSAGE(
			expression.parse("(((((((((((((((666)))))))))))))))") == OK,
			"The expression should parse successfully.");

	// Using invalid identifiers doesn't cause a parse error.
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			expression.parse("hello + hello") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 0,
			"The expression should return the expected result.");
	ERR_PRINT_ON;

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			expression.parse("$1.00 + ???5") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 0,
			"The expression should return the expected result.");
	ERR_PRINT_ON;

	// Commas can't be used as a decimal parameter.
	CHECK_MESSAGE(
			expression.parse("123,456") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 123,
			"The expression should return the expected result.");

	// Spaces can't be used as a separator for large numbers.
	CHECK_MESSAGE(
			expression.parse("123 456") == OK,
			"The expression should parse successfully.");
	CHECK_MESSAGE(
			int(expression.execute()) == 123,
			"The expression should return the expected result.");

	// Division by zero is accepted, even though it prints an error message normally.
	CHECK_MESSAGE(
			expression.parse("-25.4 / 0") == OK,
			"The expression should parse successfully.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			Math::is_inf(double(expression.execute())),
			"`-25.4 / 0` should return inf.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			expression.parse("0 / 0") == OK,
			"The expression should parse successfully.");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			int(expression.execute()) == 0,
			"`0 / 0` should return 0.");
	ERR_PRINT_ON;

	// The tests below currently crash the engine.
	//
	//CHECK_MESSAGE(
	//		expression.parse("(-9223372036854775807 - 1) % -1") == OK,
	//		"The expression should parse successfully.");
	//CHECK_MESSAGE(
	//		int64_t(expression.execute()) == 0,
	//		"`(-9223372036854775807 - 1) % -1` should return the expected result.");
	//
	//CHECK_MESSAGE(
	//		expression.parse("(-9223372036854775807 - 1) / -1") == OK,
	//		"The expression should parse successfully.");
	//CHECK_MESSAGE(
	//		int64_t(expression.execute()) == 0,
	//		"`(-9223372036854775807 - 1) / -1` should return the expected result.");
}
} // namespace TestExpression
