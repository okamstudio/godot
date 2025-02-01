/**************************************************************************/
/*  test_math_funcs.h                                                     */
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

#include "tests/test_macros.h"

namespace TestMath {

TEST_CASE("[Math] C++ macros") {
	CHECK(MIN(-2, 2) == -2);
	CHECK(MIN(600, 2) == 2);

	CHECK(MAX(-2, 2) == 2);
	CHECK(MAX(600, 2) == 600);

	CHECK(CLAMP(600, -2, 2) == 2);
	CHECK(CLAMP(620, 600, 650) == 620);
	// `max` is lower than `min`.
	CHECK(CLAMP(620, 600, 50) == 50);

	CHECK(ABS(-5) == 5);
	CHECK(ABS(0) == 0);
	CHECK(ABS(5) == 5);

	CHECK(SIGN(-5) == -1.0);
	CHECK(SIGN(0) == 0.0);
	CHECK(SIGN(5) == 1.0);
	// Check that SIGN(NAN) returns 0.0.
	CHECK(SIGN(NAN) == 0.0);
}

TEST_CASE("[Math] Power of two functions") {
	CHECK(next_power_of_2(0) == 0);
	CHECK(next_power_of_2(1) == 1);
	CHECK(next_power_of_2(16) == 16);
	CHECK(next_power_of_2(17) == 32);
	CHECK(next_power_of_2(65535) == 65536);

	CHECK(previous_power_of_2(0) == 0);
	CHECK(previous_power_of_2(1) == 1);
	CHECK(previous_power_of_2(16) == 16);
	CHECK(previous_power_of_2(17) == 16);
	CHECK(previous_power_of_2(65535) == 32768);

	CHECK(closest_power_of_2(0) == 0);
	CHECK(closest_power_of_2(1) == 1);
	CHECK(closest_power_of_2(16) == 16);
	CHECK(closest_power_of_2(17) == 16);
	CHECK(closest_power_of_2(65535) == 65536);

	CHECK(get_shift_from_power_of_2(0) == -1);
	CHECK(get_shift_from_power_of_2(1) == 0);
	CHECK(get_shift_from_power_of_2(16) == 4);
	CHECK(get_shift_from_power_of_2(17) == -1);
	CHECK(get_shift_from_power_of_2(65535) == -1);

	CHECK(nearest_shift(0) == 0);
	CHECK(nearest_shift(1) == 1);
	CHECK(nearest_shift(16) == 5);
	CHECK(nearest_shift(17) == 5);
	CHECK(nearest_shift(65535) == 16);
}

TEST_CASE_TEMPLATE("[Math] abs", T, int, float, double) {
	CHECK(Math::abs((T)-1) == (T)1);
	CHECK(Math::abs((T)0) == (T)0);
	CHECK(Math::abs((T)1) == (T)1);
	CHECK(Math::abs((T)0.1) == (T)0.1);
}

TEST_CASE_TEMPLATE("[Math] round/floor/ceil", T, float, double) {
	CHECK(Math::round((T)1.5) == (T)2.0);
	CHECK(Math::round((T)1.6) == (T)2.0);
	CHECK(Math::round((T)-1.5) == (T)-2.0);
	CHECK(Math::round((T)-1.1) == (T)-1.0);

	CHECK(Math::floor((T)1.5) == (T)1.0);
	CHECK(Math::floor((T)-1.5) == (T)-2.0);

	CHECK(Math::ceil((T)1.5) == (T)2.0);
	CHECK(Math::ceil((T)-1.9) == (T)-1.0);
}

TEST_CASE_TEMPLATE("[Math] integer division round up unsigned", T, uint32_t, uint64_t) {
	CHECK(Math::division_round_up((T)0, (T)64) == 0);
	CHECK(Math::division_round_up((T)1, (T)64) == 1);
	CHECK(Math::division_round_up((T)63, (T)64) == 1);
	CHECK(Math::division_round_up((T)64, (T)64) == 1);
	CHECK(Math::division_round_up((T)65, (T)64) == 2);
	CHECK(Math::division_round_up((T)65, (T)1) == 65);
}

TEST_CASE_TEMPLATE("[Math] integer division round up signed", T, int32_t, int64_t) {
	CHECK(Math::division_round_up((T)0, (T)64) == 0);
	CHECK(Math::division_round_up((T)1, (T)64) == 1);
	CHECK(Math::division_round_up((T)63, (T)64) == 1);
	CHECK(Math::division_round_up((T)64, (T)64) == 1);
	CHECK(Math::division_round_up((T)65, (T)64) == 2);
	CHECK(Math::division_round_up((T)65, (T)1) == 65);
	CHECK(Math::division_round_up((T)-1, (T)64) == 0);
	CHECK(Math::division_round_up((T)-1, (T)-1) == 1);
	CHECK(Math::division_round_up((T)-1, (T)1) == -1);
	CHECK(Math::division_round_up((T)-1, (T)-2) == 1);
	CHECK(Math::division_round_up((T)-4, (T)-2) == 2);
}

TEST_CASE_TEMPLATE("[Math] sin/cos/tan", T, float, double) {
	CHECK(Math::sin((T)-0.1) == doctest::Approx((T)-0.0998334166));
	CHECK(Math::sin((T)0.1) == doctest::Approx((T)0.0998334166));
	CHECK(Math::sin((T)0.5) == doctest::Approx((T)0.4794255386));
	CHECK(Math::sin((T)1.0) == doctest::Approx((T)0.8414709848));
	CHECK(Math::sin((T)1.5) == doctest::Approx((T)0.9974949866));
	CHECK(Math::sin((T)450.0) == doctest::Approx((T)-0.683283725));

	CHECK(Math::cos((T)-0.1) == doctest::Approx((T)0.99500416530));
	CHECK(Math::cos((T)0.1) == doctest::Approx((T)0.9950041653));
	CHECK(Math::cos((T)0.5) == doctest::Approx((T)0.8775825619));
	CHECK(Math::cos((T)1.0) == doctest::Approx((T)0.5403023059));
	CHECK(Math::cos((T)1.5) == doctest::Approx((T)0.0707372017));
	CHECK(Math::cos((T)450.0) == doctest::Approx((T)-0.7301529642));

	CHECK(Math::tan((T)-0.1) == doctest::Approx((T)-0.1003346721));
	CHECK(Math::tan((T)0.1) == doctest::Approx((T)0.1003346721));
	CHECK(Math::tan((T)0.5) == doctest::Approx((T)0.5463024898));
	CHECK(Math::tan((T)1.0) == doctest::Approx((T)1.5574077247));
	CHECK(Math::tan((T)1.5) == doctest::Approx((T)14.1014199472));
	CHECK(Math::tan((T)450.0) == doctest::Approx((T)0.9358090134));
}

TEST_CASE_TEMPLATE("[Math] sinh/cosh/tanh", T, float, double) {
	CHECK(Math::sinh((T)-0.1) == doctest::Approx((T)-0.10016675));
	CHECK(Math::sinh((T)0.1) == doctest::Approx((T)0.10016675));
	CHECK(Math::sinh((T)0.5) == doctest::Approx((T)0.5210953055));
	CHECK(Math::sinh((T)1.0) == doctest::Approx((T)1.1752011936));
	CHECK(Math::sinh((T)1.5) == doctest::Approx((T)2.1292794551));

	CHECK(Math::cosh((T)-0.1) == doctest::Approx((T)1.0050041681));
	CHECK(Math::cosh((T)0.1) == doctest::Approx((T)1.0050041681));
	CHECK(Math::cosh((T)0.5) == doctest::Approx((T)1.1276259652));
	CHECK(Math::cosh((T)1.0) == doctest::Approx((T)1.5430806348));
	CHECK(Math::cosh((T)1.5) == doctest::Approx((T)2.3524096152));

	CHECK(Math::tanh((T)-0.1) == doctest::Approx((T)-0.0996679946));
	CHECK(Math::tanh((T)0.1) == doctest::Approx((T)0.0996679946));
	CHECK(Math::tanh((T)0.5) == doctest::Approx((T)0.4621171573));
	CHECK(Math::tanh((T)1.0) == doctest::Approx((T)0.761594156));
	CHECK(Math::tanh((T)1.5) == doctest::Approx((T)0.9051482536));
	CHECK(Math::tanh((T)450.0) == doctest::Approx((T)1.0));
}

TEST_CASE_TEMPLATE("[Math] asin/acos/atan", T, float, double) {
	CHECK(Math::asin((T)-0.1) == doctest::Approx((T)-0.1001674212));
	CHECK(Math::asin((T)0.1) == doctest::Approx((T)0.1001674212));
	CHECK(Math::asin((T)0.5) == doctest::Approx((T)0.5235987756));
	CHECK(Math::asin((T)1.0) == doctest::Approx((T)1.5707963268));
	CHECK(Math::asin((T)2.0) == doctest::Approx((T)1.5707963268));
	CHECK(Math::asin((T)-2.0) == doctest::Approx((T)-1.5707963268));

	CHECK(Math::acos((T)-0.1) == doctest::Approx((T)1.670963748));
	CHECK(Math::acos((T)0.1) == doctest::Approx((T)1.4706289056));
	CHECK(Math::acos((T)0.5) == doctest::Approx((T)1.0471975512));
	CHECK(Math::acos((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::acos((T)2.0) == doctest::Approx((T)0.0));
	CHECK(Math::acos((T)-2.0) == doctest::Approx((T)Math_PI));

	CHECK(Math::atan((T)-0.1) == doctest::Approx((T)-0.0996686525));
	CHECK(Math::atan((T)0.1) == doctest::Approx((T)0.0996686525));
	CHECK(Math::atan((T)0.5) == doctest::Approx((T)0.463647609));
	CHECK(Math::atan((T)1.0) == doctest::Approx((T)0.7853981634));
	CHECK(Math::atan((T)1.5) == doctest::Approx((T)0.9827937232));
	CHECK(Math::atan((T)450.0) == doctest::Approx((T)1.5685741082));
}

TEST_CASE_TEMPLATE("[Math] asinh/acosh/atanh", T, float, double) {
	CHECK(Math::asinh((T)-2.0) == doctest::Approx((T)-1.4436354751));
	CHECK(Math::asinh((T)-0.1) == doctest::Approx((T)-0.0998340788));
	CHECK(Math::asinh((T)0.1) == doctest::Approx((T)0.0998340788));
	CHECK(Math::asinh((T)0.5) == doctest::Approx((T)0.4812118250));
	CHECK(Math::asinh((T)1.0) == doctest::Approx((T)0.8813735870));
	CHECK(Math::asinh((T)2.0) == doctest::Approx((T)1.4436354751));

	CHECK(Math::acosh((T)-2.0) == doctest::Approx((T)0.0));
	CHECK(Math::acosh((T)-0.1) == doctest::Approx((T)0.0));
	CHECK(Math::acosh((T)0.1) == doctest::Approx((T)0.0));
	CHECK(Math::acosh((T)0.5) == doctest::Approx((T)0.0));
	CHECK(Math::acosh((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::acosh((T)2.0) == doctest::Approx((T)1.3169578969));
	CHECK(Math::acosh((T)450.0) == doctest::Approx((T)6.8023935287));

	CHECK(Math::is_inf(Math::atanh((T)-2.0)));
	CHECK(Math::atanh((T)-2.0) < (T)0.0);
	CHECK(Math::is_inf(Math::atanh((T)-1.0)));
	CHECK(Math::atanh((T)-1.0) < (T)0.0);
	CHECK(Math::atanh((T)-0.1) == doctest::Approx((T)-0.1003353477));
	CHECK(Math::atanh((T)0.1) == doctest::Approx((T)0.1003353477));
	CHECK(Math::atanh((T)0.5) == doctest::Approx((T)0.5493061443));
	CHECK(Math::is_inf(Math::atanh((T)1.0)));
	CHECK(Math::atanh((T)1.0) > (T)0.0);
	CHECK(Math::is_inf(Math::atanh((T)1.5)));
	CHECK(Math::atanh((T)1.5) > (T)0.0);
	CHECK(Math::is_inf(Math::atanh((T)450.0)));
	CHECK(Math::atanh((T)450.0) > (T)0.0);
}

TEST_CASE_TEMPLATE("[Math] sinc/sincn/atan2", T, float, double) {
	CHECK(Math::sinc((T)-0.1) == doctest::Approx((T)0.9983341665));
	CHECK(Math::sinc((T)0.1) == doctest::Approx((T)0.9983341665));
	CHECK(Math::sinc((T)0.5) == doctest::Approx((T)0.9588510772));
	CHECK(Math::sinc((T)1.0) == doctest::Approx((T)0.8414709848));
	CHECK(Math::sinc((T)1.5) == doctest::Approx((T)0.6649966577));
	CHECK(Math::sinc((T)450.0) == doctest::Approx((T)-0.0015184083));

	CHECK(Math::sincn((T)-0.1) == doctest::Approx((T)0.9836316431));
	CHECK(Math::sincn((T)0.1) == doctest::Approx((T)0.9836316431));
	CHECK(Math::sincn((T)0.5) == doctest::Approx((T)0.6366197724));
	CHECK(Math::sincn((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::sincn((T)1.5) == doctest::Approx((T)-0.2122065908));
	CHECK(Math::sincn((T)450.0) == doctest::Approx((T)0.0));

	CHECK(Math::atan2((T)-0.1, (T)0.5) == doctest::Approx((T)-0.1973955598));
	CHECK(Math::atan2((T)0.1, (T)-0.5) == doctest::Approx((T)2.9441970937));
	CHECK(Math::atan2((T)0.5, (T)1.5) == doctest::Approx((T)0.3217505544));
	CHECK(Math::atan2((T)1.0, (T)2.5) == doctest::Approx((T)0.3805063771));
	CHECK(Math::atan2((T)1.5, (T)1.0) == doctest::Approx((T)0.9827937232));
	CHECK(Math::atan2((T)450.0, (T)1.0) == doctest::Approx((T)1.5685741082));
}

TEST_CASE_TEMPLATE("[Math] pow/log/log2/exp/sqrt", T, float, double) {
	CHECK(Math::pow((T)-0.1, (T)2.0) == doctest::Approx((T)0.01));
	CHECK(Math::pow((T)0.1, (T)2.5) == doctest::Approx((T)0.0031622777));
	CHECK(Math::pow((T)0.5, (T)0.5) == doctest::Approx((T)0.7071067812));
	CHECK(Math::pow((T)1.0, (T)1.0) == doctest::Approx((T)1.0));
	CHECK(Math::pow((T)1.5, (T)-1.0) == doctest::Approx((T)0.6666666667));
	CHECK(Math::pow((T)450.0, (T)-2.0) == doctest::Approx((T)0.0000049383));
	CHECK(Math::pow((T)450.0, (T)0.0) == doctest::Approx((T)1.0));

	CHECK(Math::is_nan(Math::log((T)-0.1)));
	CHECK(Math::log((T)0.1) == doctest::Approx((T)-2.302585093));
	CHECK(Math::log((T)0.5) == doctest::Approx((T)-0.6931471806));
	CHECK(Math::log((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::log((T)1.5) == doctest::Approx((T)0.4054651081));
	CHECK(Math::log((T)450.0) == doctest::Approx((T)6.1092475828));

	CHECK(Math::is_nan(Math::log2((T)-0.1)));
	CHECK(Math::log2((T)0.1) == doctest::Approx((T)-3.3219280949));
	CHECK(Math::log2((T)0.5) == doctest::Approx((T)-1.0));
	CHECK(Math::log2((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::log2((T)1.5) == doctest::Approx((T)0.5849625007));
	CHECK(Math::log2((T)450.0) == doctest::Approx((T)8.8137811912));

	CHECK(Math::exp((T)-0.1) == doctest::Approx((T)0.904837418));
	CHECK(Math::exp((T)0.1) == doctest::Approx((T)1.1051709181));
	CHECK(Math::exp((T)0.5) == doctest::Approx((T)1.6487212707));
	CHECK(Math::exp((T)1.0) == doctest::Approx((T)2.7182818285));
	CHECK(Math::exp((T)1.5) == doctest::Approx((T)4.4816890703));

	CHECK(Math::is_nan(Math::sqrt((T)-0.1)));
	CHECK(Math::sqrt((T)0.1) == doctest::Approx((T)0.316228));
	CHECK(Math::sqrt((T)0.5) == doctest::Approx((T)0.707107));
	CHECK(Math::sqrt((T)1.0) == doctest::Approx((T)1.0));
	CHECK(Math::sqrt((T)1.5) == doctest::Approx((T)1.224745));
}

TEST_CASE_TEMPLATE("[Math] is_nan/is_inf", T, float, double) {
	CHECK(!Math::is_nan((T)0.0));
	CHECK(Math::is_nan((T)NAN));

	CHECK(!Math::is_inf((T)0.0));
	CHECK(Math::is_inf((T)INFINITY));
}

TEST_CASE_TEMPLATE("[Math] linear_to_db", T, float, double) {
	CHECK(Math::linear_to_db((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::linear_to_db((T)20.0) == doctest::Approx((T)26.0206));
	CHECK(Math::is_inf(Math::linear_to_db((T)0.0)));
	CHECK(Math::is_nan(Math::linear_to_db((T)-20.0)));
}

TEST_CASE_TEMPLATE("[Math] db_to_linear", T, float, double) {
	CHECK(Math::db_to_linear((T)0.0) == doctest::Approx((T)1.0));
	CHECK(Math::db_to_linear((T)1.0) == doctest::Approx((T)1.122018));
	CHECK(Math::db_to_linear((T)20.0) == doctest::Approx((T)10.0));
	CHECK(Math::db_to_linear((T)-20.0) == doctest::Approx((T)0.1));
}

TEST_CASE_TEMPLATE("[Math] step_decimals", T, float, double) {
	CHECK(Math::step_decimals((T)-0.5) == 1);
	CHECK(Math::step_decimals((T)0) == 0);
	CHECK(Math::step_decimals((T)1) == 0);
	CHECK(Math::step_decimals((T)0.1) == 1);
	CHECK(Math::step_decimals((T)0.01) == 2);
	CHECK(Math::step_decimals((T)0.001) == 3);
	CHECK(Math::step_decimals((T)0.0001) == 4);
	CHECK(Math::step_decimals((T)0.00001) == 5);
	CHECK(Math::step_decimals((T)0.000001) == 6);
	CHECK(Math::step_decimals((T)0.0000001) == 7);
	CHECK(Math::step_decimals((T)0.00000001) == 8);
	CHECK(Math::step_decimals((T)0.000000001) == 9);
	// Too many decimals to handle.
	CHECK(Math::step_decimals((T)0.0000000001) == 0);
}

TEST_CASE_TEMPLATE("[Math] range_step_decimals", T, float, double) {
	CHECK(Math::range_step_decimals((T)0.000000001) == 9);
	// Too many decimals to handle.
	CHECK(Math::range_step_decimals((T)0.0000000001) == 0);
	// Should be treated as a step of 0 for use by the editor.
	CHECK(Math::range_step_decimals((T)0.0) == 16);
	CHECK(Math::range_step_decimals((T)-0.5) == 16);
}

TEST_CASE_TEMPLATE("[Math] lerp", T, float, double) {
	CHECK(Math::lerp((T)2.0, (T)5.0, (T)-0.1) == doctest::Approx((T)1.7));
	CHECK(Math::lerp((T)2.0, (T)5.0, (T)0.0) == doctest::Approx((T)2.0));
	CHECK(Math::lerp((T)2.0, (T)5.0, (T)0.1) == doctest::Approx((T)2.3));
	CHECK(Math::lerp((T)2.0, (T)5.0, (T)1.0) == doctest::Approx((T)5.0));
	CHECK(Math::lerp((T)2.0, (T)5.0, (T)2.0) == doctest::Approx((T)8.0));

	CHECK(Math::lerp((T)-2.0, (T)-5.0, (T)-0.1) == doctest::Approx((T)-1.7));
	CHECK(Math::lerp((T)-2.0, (T)-5.0, (T)0.0) == doctest::Approx((T)-2.0));
	CHECK(Math::lerp((T)-2.0, (T)-5.0, (T)0.1) == doctest::Approx((T)-2.3));
	CHECK(Math::lerp((T)-2.0, (T)-5.0, (T)1.0) == doctest::Approx((T)-5.0));
	CHECK(Math::lerp((T)-2.0, (T)-5.0, (T)2.0) == doctest::Approx((T)-8.0));
}

TEST_CASE_TEMPLATE("[Math] inverse_lerp", T, float, double) {
	CHECK(Math::inverse_lerp((T)2.0, (T)5.0, (T)1.7) == doctest::Approx((T)-0.1));
	CHECK(Math::inverse_lerp((T)2.0, (T)5.0, (T)2.0) == doctest::Approx((T)0.0));
	CHECK(Math::inverse_lerp((T)2.0, (T)5.0, (T)2.3) == doctest::Approx((T)0.1));
	CHECK(Math::inverse_lerp((T)2.0, (T)5.0, (T)5.0) == doctest::Approx((T)1.0));
	CHECK(Math::inverse_lerp((T)2.0, (T)5.0, (T)8.0) == doctest::Approx((T)2.0));

	CHECK(Math::inverse_lerp((T)-2.0, (T)-5.0, (T)-1.7) == doctest::Approx((T)-0.1));
	CHECK(Math::inverse_lerp((T)-2.0, (T)-5.0, (T)-2.0) == doctest::Approx((T)0.0));
	CHECK(Math::inverse_lerp((T)-2.0, (T)-5.0, (T)-2.3) == doctest::Approx((T)0.1));
	CHECK(Math::inverse_lerp((T)-2.0, (T)-5.0, (T)-5.0) == doctest::Approx((T)1.0));
	CHECK(Math::inverse_lerp((T)-2.0, (T)-5.0, (T)-8.0) == doctest::Approx((T)2.0));
}

TEST_CASE_TEMPLATE("[Math] remap", T, float, double) {
	CHECK(Math::remap((T)50.0, (T)100.0, (T)200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)-500.0));
	CHECK(Math::remap((T)100.0, (T)100.0, (T)200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)0.0));
	CHECK(Math::remap((T)200.0, (T)100.0, (T)200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)1000.0));
	CHECK(Math::remap((T)250.0, (T)100.0, (T)200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)1500.0));

	CHECK(Math::remap((T)-50.0, (T)-100.0, (T)-200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)-500.0));
	CHECK(Math::remap((T)-100.0, (T)-100.0, (T)-200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)0.0));
	CHECK(Math::remap((T)-200.0, (T)-100.0, (T)-200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)1000.0));
	CHECK(Math::remap((T)-250.0, (T)-100.0, (T)-200.0, (T)0.0, (T)1000.0) == doctest::Approx((T)1500.0));

	CHECK(Math::remap((T)-50.0, (T)-100.0, (T)-200.0, (T)0.0, (T)-1000.0) == doctest::Approx((T)500.0));
	CHECK(Math::remap((T)-100.0, (T)-100.0, (T)-200.0, (T)0.0, (T)-1000.0) == doctest::Approx((T)0.0));
	CHECK(Math::remap((T)-200.0, (T)-100.0, (T)-200.0, (T)0.0, (T)-1000.0) == doctest::Approx((T)-1000.0));
	CHECK(Math::remap((T)-250.0, (T)-100.0, (T)-200.0, (T)0.0, (T)-1000.0) == doctest::Approx((T)-1500.0));

	// Note: undefined behavior can happen when `p_istart == p_istop`. We don't bother testing this as it will
	// vary between hardware and compilers properly implementing IEEE 754.
}

TEST_CASE_TEMPLATE("[Math] angle_difference", T, float, double) {
	// Loops around, should return 0.0.
	CHECK(Math::angle_difference((T)0.0, (T)Math_TAU) == doctest::Approx((T)0.0));
	CHECK(Math::angle_difference((T)Math_PI, (T)-Math_PI) == doctest::Approx((T)0.0));
	CHECK(Math::angle_difference((T)0.0, (T)Math_TAU * (T)4.0) == doctest::Approx((T)0.0));

	// Rotation is clockwise, so it should return -PI.
	CHECK(Math::angle_difference((T)0.0, (T)Math_PI) == doctest::Approx((T)-Math_PI));
	CHECK(Math::angle_difference((T)0.0, (T)-Math_PI) == doctest::Approx((T)Math_PI));
	CHECK(Math::angle_difference((T)Math_PI, (T)0.0) == doctest::Approx((T)Math_PI));
	CHECK(Math::angle_difference((T)-Math_PI, (T)0.0) == doctest::Approx((T)-Math_PI));

	CHECK(Math::angle_difference((T)0.0, (T)3.0) == doctest::Approx((T)3.0));
	CHECK(Math::angle_difference((T)1.0, (T)-2.0) == doctest::Approx((T)-3.0));
	CHECK(Math::angle_difference((T)-1.0, (T)2.0) == doctest::Approx((T)3.0));
	CHECK(Math::angle_difference((T)-2.0, (T)-4.5) == doctest::Approx((T)-2.5));
	CHECK(Math::angle_difference((T)100.0, (T)102.5) == doctest::Approx((T)2.5));
}

TEST_CASE_TEMPLATE("[Math] lerp_angle", T, float, double) {
	// Counter-clockwise rotation.
	CHECK(Math::lerp_angle((T)0.24 * Math_TAU, 0.75 * Math_TAU, 0.5) == doctest::Approx((T)-0.005 * Math_TAU));
	// Counter-clockwise rotation.
	CHECK(Math::lerp_angle((T)0.25 * Math_TAU, 0.75 * Math_TAU, 0.5) == doctest::Approx((T)0.0));
	// Clockwise rotation.
	CHECK(Math::lerp_angle((T)0.26 * Math_TAU, 0.75 * Math_TAU, 0.5) == doctest::Approx((T)0.505 * Math_TAU));

	CHECK(Math::lerp_angle((T)-0.25 * Math_TAU, 1.25 * Math_TAU, 0.5) == doctest::Approx((T)-0.5 * Math_TAU));
	CHECK(Math::lerp_angle((T)0.72 * Math_TAU, 1.44 * Math_TAU, 0.96) == doctest::Approx((T)0.4512 * Math_TAU));
	CHECK(Math::lerp_angle((T)0.72 * Math_TAU, 1.44 * Math_TAU, 1.04) == doctest::Approx((T)0.4288 * Math_TAU));

	// Initial and final angles are effectively identical, so the value returned
	// should always be the same regardless of the `weight` parameter.
	CHECK(Math::lerp_angle((T)-4 * Math_TAU, 4 * Math_TAU, -1.0) == doctest::Approx((T)-4.0 * Math_TAU));
	CHECK(Math::lerp_angle((T)-4 * Math_TAU, 4 * Math_TAU, 0.0) == doctest::Approx((T)-4.0 * Math_TAU));
	CHECK(Math::lerp_angle((T)-4 * Math_TAU, 4 * Math_TAU, 0.5) == doctest::Approx((T)-4.0 * Math_TAU));
	CHECK(Math::lerp_angle((T)-4 * Math_TAU, 4 * Math_TAU, 1.0) == doctest::Approx((T)-4.0 * Math_TAU));
	CHECK(Math::lerp_angle((T)-4 * Math_TAU, 4 * Math_TAU, 500.0) == doctest::Approx((T)-4.0 * Math_TAU));
}

TEST_CASE_TEMPLATE("[Math] move_toward", T, float, double) {
	CHECK(Math::move_toward(2.0, 5.0, -1.0) == doctest::Approx((T)1.0));
	CHECK(Math::move_toward(2.0, 5.0, 2.5) == doctest::Approx((T)4.5));
	CHECK(Math::move_toward(2.0, 5.0, 4.0) == doctest::Approx((T)5.0));
	CHECK(Math::move_toward(-2.0, -5.0, -1.0) == doctest::Approx((T)-1.0));
	CHECK(Math::move_toward(-2.0, -5.0, 2.5) == doctest::Approx((T)-4.5));
	CHECK(Math::move_toward(-2.0, -5.0, 4.0) == doctest::Approx((T)-5.0));
}

TEST_CASE_TEMPLATE("[Math] rotate_toward", T, float, double) {
	// Rotate toward.
	CHECK(Math::rotate_toward((T)0.0, (T)Math_PI * (T)0.75, (T)1.5) == doctest::Approx((T)1.5));
	CHECK(Math::rotate_toward((T)-2.0, (T)1.0, (T)2.5) == doctest::Approx((T)0.5));
	CHECK(Math::rotate_toward((T)-2.0, (T)Math_PI, (T)Math_PI) == doctest::Approx((T)-Math_PI));
	CHECK(Math::rotate_toward((T)1.0, (T)Math_PI, (T)20.0) == doctest::Approx((T)Math_PI));

	// Rotate away.
	CHECK(Math::rotate_toward((T)0.0, (T)0.0, (T)-1.5) == doctest::Approx((T)-1.5));
	CHECK(Math::rotate_toward((T)0.0, (T)0.0, (T)-Math_PI) == doctest::Approx((T)-Math_PI));
	CHECK(Math::rotate_toward((T)3.0, (T)Math_PI, (T)-Math_PI) == doctest::Approx((T)0.0));
	CHECK(Math::rotate_toward((T)2.0, (T)Math_PI, (T)-1.5) == doctest::Approx((T)0.5));
	CHECK(Math::rotate_toward((T)1.0, (T)2.0, (T)-0.5) == doctest::Approx((T)0.5));
	CHECK(Math::rotate_toward((T)2.5, (T)2.0, (T)-0.5) == doctest::Approx((T)3.0));
	CHECK(Math::rotate_toward((T)-1.0, (T)1.0, (T)-1.0) == doctest::Approx((T)-2.0));
}

TEST_CASE_TEMPLATE("[Math] smoothstep", T, float, double) {
	CHECK(Math::smoothstep((T)0.0, (T)2.0, (T)-5.0) == doctest::Approx((T)0.0));
	CHECK(Math::smoothstep((T)0.0, (T)2.0, (T)0.5) == doctest::Approx((T)0.15625));
	CHECK(Math::smoothstep((T)0.0, (T)2.0, (T)1.0) == doctest::Approx((T)0.5));
	CHECK(Math::smoothstep((T)0.0, (T)2.0, (T)2.0) == doctest::Approx((T)1.0));
}

TEST_CASE("[Math] ease") {
	CHECK(Math::ease(0.1, 1.0) == doctest::Approx(0.1));
	CHECK(Math::ease(0.1, 2.0) == doctest::Approx(0.01));
	CHECK(Math::ease(0.1, 0.5) == doctest::Approx(0.19));
	CHECK(Math::ease(0.1, 0.0) == doctest::Approx(0));
	CHECK(Math::ease(0.1, -0.5) == doctest::Approx(0.2236067977));
	CHECK(Math::ease(0.1, -1.0) == doctest::Approx(0.1));
	CHECK(Math::ease(0.1, -2.0) == doctest::Approx(0.02));

	CHECK(Math::ease(-1.0, 1.0) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, 2.0) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, 0.5) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, 0.0) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, -0.5) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, -1.0) == doctest::Approx(0));
	CHECK(Math::ease(-1.0, -2.0) == doctest::Approx(0));
}

TEST_CASE("[Math] snapped") {
	CHECK(Math::snapped(0.5, 0.04) == doctest::Approx(0.52));
	CHECK(Math::snapped(-0.5, 0.04) == doctest::Approx(-0.48));
	CHECK(Math::snapped(0.0, 0.04) == doctest::Approx(0));
	CHECK(Math::snapped(128'000.025, 0.04) == doctest::Approx(128'000.04));

	CHECK(Math::snapped(0.5, 400) == doctest::Approx(0));
	CHECK(Math::snapped(-0.5, 400) == doctest::Approx(0));
	CHECK(Math::snapped(0.0, 400) == doctest::Approx(0));
	CHECK(Math::snapped(128'000.025, 400) == doctest::Approx(128'000.0));

	CHECK(Math::snapped(0.5, 0.0) == doctest::Approx(0.5));
	CHECK(Math::snapped(-0.5, 0.0) == doctest::Approx(-0.5));
	CHECK(Math::snapped(0.0, 0.0) == doctest::Approx(0.0));
	CHECK(Math::snapped(128'000.025, 0.0) == doctest::Approx(128'000.0));

	CHECK(Math::snapped(0.5, -1.0) == doctest::Approx(0));
	CHECK(Math::snapped(-0.5, -1.0) == doctest::Approx(-1.0));
	CHECK(Math::snapped(0.0, -1.0) == doctest::Approx(0));
	CHECK(Math::snapped(128'000.025, -1.0) == doctest::Approx(128'000.0));
}

TEST_CASE("[Math] larger_prime") {
	CHECK(Math::larger_prime(0) == 5);
	CHECK(Math::larger_prime(1) == 5);
	CHECK(Math::larger_prime(2) == 5);
	CHECK(Math::larger_prime(5) == 13);
	CHECK(Math::larger_prime(500) == 769);
	CHECK(Math::larger_prime(1'000'000) == 1'572'869);
	CHECK(Math::larger_prime(1'000'000'000) == 1'610'612'741);

	// The next prime is larger than `INT32_MAX` and is not present in the built-in prime table.
	ERR_PRINT_OFF;
	CHECK(Math::larger_prime(2'000'000'000) == 0);
	ERR_PRINT_ON;
}

TEST_CASE_TEMPLATE("[Math] fmod", T, float, double) {
	CHECK(Math::fmod((T)-2.0, (T)0.3) == doctest::Approx((T)-0.2));
	CHECK(Math::fmod((T)0.0, (T)0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fmod((T)2.0, (T)0.3) == doctest::Approx((T)0.2));

	CHECK(Math::fmod((T)-2.0, (T)-0.3) == doctest::Approx((T)-0.2));
	CHECK(Math::fmod((T)0.0, (T)-0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fmod((T)2.0, (T)-0.3) == doctest::Approx((T)0.2));
}

TEST_CASE_TEMPLATE("[Math] fposmod", T, float, double) {
	CHECK(Math::fposmod((T)-2.0, (T)0.3) == doctest::Approx((T)0.1));
	CHECK(Math::fposmod((T)0.0, (T)0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fposmod((T)2.0, (T)0.3) == doctest::Approx((T)0.2));

	CHECK(Math::fposmod((T)-2.0, (T)-0.3) == doctest::Approx((T)-0.2));
	CHECK(Math::fposmod((T)0.0, (T)-0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fposmod((T)2.0, (T)-0.3) == doctest::Approx((T)-0.1));
}

TEST_CASE_TEMPLATE("[Math] fposmodp", T, float, double) {
	CHECK(Math::fposmodp((T)-2.0, (T)0.3) == doctest::Approx((T)0.1));
	CHECK(Math::fposmodp((T)0.0, (T)0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fposmodp((T)2.0, (T)0.3) == doctest::Approx((T)0.2));

	CHECK(Math::fposmodp((T)-2.0, (T)-0.3) == doctest::Approx((T)-0.5));
	CHECK(Math::fposmodp((T)0.0, (T)-0.3) == doctest::Approx((T)0.0));
	CHECK(Math::fposmodp((T)2.0, (T)-0.3) == doctest::Approx((T)0.2));
}

TEST_CASE("[Math] posmod") {
	CHECK(Math::posmod(-20, 3) == 1);
	CHECK(Math::posmod(0, 3) == 0);
	CHECK(Math::posmod(20, 3) == 2);
	CHECK(Math::posmod(-20, -3) == -2);
	CHECK(Math::posmod(0, -3) == 0);
	CHECK(Math::posmod(20, -3) == -1);
}

TEST_CASE("[Math] wrapi") {
	CHECK(Math::wrapi(-30, -20, 160) == 150);
	CHECK(Math::wrapi(30, -20, 160) == 30);
	CHECK(Math::wrapi(300, -20, 160) == 120);
	CHECK(Math::wrapi(300'000'000'000, -20, 160) == 120);
}

TEST_CASE_TEMPLATE("[Math] wrapf", T, float, double) {
	CHECK(Math::wrapf((T)-30.0, (T)-20.0, (T)160.0) == doctest::Approx((T)150.0));
	CHECK(Math::wrapf((T)30.0, (T)-2.0, (T)160.0) == doctest::Approx((T)30.0));
	CHECK(Math::wrapf((T)300.0, (T)-20.0, (T)160.0) == doctest::Approx((T)120.0));

	CHECK(Math::wrapf(300'000'000'000.0, -20.0, 160.0) == doctest::Approx((T)120.0));
	// float's precision is too low for 300'000'000'000.0, so we reduce it by a factor of 1000.
	CHECK(Math::wrapf((float)15'000'000.0, (float)-20.0, (float)160.0) == doctest::Approx((T)60.0));
}

TEST_CASE_TEMPLATE("[Math] fract", T, float, double) {
	CHECK(Math::fract((T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::fract((T)77.8) == doctest::Approx((T)0.8));
	CHECK(Math::fract((T)-10.1) == doctest::Approx((T)0.9));
}

TEST_CASE_TEMPLATE("[Math] pingpong", T, float, double) {
	CHECK(Math::pingpong((T)0.0, (T)0.0) == doctest::Approx((T)0.0));
	CHECK(Math::pingpong((T)1.0, (T)1.0) == doctest::Approx((T)1.0));
	CHECK(Math::pingpong((T)0.5, (T)2.0) == doctest::Approx((T)0.5));
	CHECK(Math::pingpong((T)3.5, (T)2.0) == doctest::Approx((T)0.5));
	CHECK(Math::pingpong((T)11.5, (T)2.0) == doctest::Approx((T)0.5));
	CHECK(Math::pingpong((T)-2.5, (T)2.0) == doctest::Approx((T)1.5));
}

TEST_CASE_TEMPLATE("[Math] deg_to_rad/rad_to_deg", T, float, double) {
	CHECK(Math::deg_to_rad((T)180.0) == doctest::Approx((T)Math_PI));
	CHECK(Math::deg_to_rad((T)-27.0) == doctest::Approx((T)-0.471239));

	CHECK(Math::rad_to_deg((T)Math_PI) == doctest::Approx((T)180.0));
	CHECK(Math::rad_to_deg((T)-1.5) == doctest::Approx((T)-85.94366927));
}

TEST_CASE_TEMPLATE("[Math] cubic_interpolate", T, float, double) {
	CHECK(Math::cubic_interpolate((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.0) == doctest::Approx((T)0.2));
	CHECK(Math::cubic_interpolate((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.25) == doctest::Approx((T)0.33125));
	CHECK(Math::cubic_interpolate((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.5) == doctest::Approx((T)0.5));
	CHECK(Math::cubic_interpolate((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.75) == doctest::Approx((T)0.66875));
	CHECK(Math::cubic_interpolate((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)1.0) == doctest::Approx((T)0.8));

	CHECK(Math::cubic_interpolate((T)20.2, (T)30.1, (T)-100.0, (T)32.0, (T)-50.0) == doctest::Approx((T)-6662732.3));
	CHECK(Math::cubic_interpolate((T)20.2, (T)30.1, (T)-100.0, (T)32.0, (T)-5.0) == doctest::Approx((T)-9356.3));
	CHECK(Math::cubic_interpolate((T)20.2, (T)30.1, (T)-100.0, (T)32.0, (T)0.0) == doctest::Approx((T)20.2));
	CHECK(Math::cubic_interpolate((T)20.2, (T)30.1, (T)-100.0, (T)32.0, (T)1.0) == doctest::Approx((T)30.1));
	CHECK(Math::cubic_interpolate((T)20.2, (T)30.1, (T)-100.0, (T)32.0, (T)4.0) == doctest::Approx((T)1853.2));
}

TEST_CASE_TEMPLATE("[Math] cubic_interpolate_angle", T, float, double) {
	CHECK(Math::cubic_interpolate_angle((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.0) == doctest::Approx((T)Math_PI * (1.0 / 6.0)));
	CHECK(Math::cubic_interpolate_angle((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.25) == doctest::Approx((T)0.973566));
	CHECK(Math::cubic_interpolate_angle((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.5) == doctest::Approx((T)Math_PI / 2.0));
	CHECK(Math::cubic_interpolate_angle((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.75) == doctest::Approx((T)2.16803));
	CHECK(Math::cubic_interpolate_angle((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)1.0) == doctest::Approx((T)Math_PI * (5.0 / 6.0)));
}

TEST_CASE_TEMPLATE("[Math] cubic_interpolate_in_time", T, float, double) {
	CHECK(Math::cubic_interpolate_in_time((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.0, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::cubic_interpolate_in_time((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.25, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.1625));
	CHECK(Math::cubic_interpolate_in_time((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.5, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.4));
	CHECK(Math::cubic_interpolate_in_time((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)0.75, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.6375));
	CHECK(Math::cubic_interpolate_in_time((T)0.2, (T)0.8, (T)0.0, (T)1.0, (T)1.0, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.8));
}

TEST_CASE_TEMPLATE("[Math] cubic_interpolate_angle_in_time", T, float, double) {
	CHECK(Math::cubic_interpolate_angle_in_time((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.0, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.0));
	CHECK(Math::cubic_interpolate_angle_in_time((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.25, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)0.494964));
	CHECK(Math::cubic_interpolate_angle_in_time((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.5, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)1.27627));
	CHECK(Math::cubic_interpolate_angle_in_time((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)0.75, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)2.07394));
	CHECK(Math::cubic_interpolate_angle_in_time((T)(Math_PI * (1.0 / 6.0)), (T)(Math_PI * (5.0 / 6.0)), (T)0.0, (T)Math_PI, (T)1.0, (T)0.5, (T)0.0, (T)1.0) == doctest::Approx((T)Math_PI * (5.0 / 6.0)));
}

TEST_CASE_TEMPLATE("[Math] bezier_interpolate", T, float, double) {
	CHECK(Math::bezier_interpolate((T)0.0, (T)0.2, (T)0.8, (T)1.0, (T)0.0) == doctest::Approx((T)0.0));
	CHECK(Math::bezier_interpolate((T)0.0, (T)0.2, (T)0.8, (T)1.0, (T)0.25) == doctest::Approx((T)0.2125));
	CHECK(Math::bezier_interpolate((T)0.0, (T)0.2, (T)0.8, (T)1.0, (T)0.5) == doctest::Approx((T)0.5));
	CHECK(Math::bezier_interpolate((T)0.0, (T)0.2, (T)0.8, (T)1.0, (T)0.75) == doctest::Approx((T)0.7875));
	CHECK(Math::bezier_interpolate((T)0.0, (T)0.2, (T)0.8, (T)1.0, (T)1.0) == doctest::Approx((T)1.0));
}

} // namespace TestMath
