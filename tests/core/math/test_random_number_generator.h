/**************************************************************************/
/*  test_random_number_generator.h                                        */
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

#include "core/math/random_number_generator.h"
#include "tests/test_macros.h"

namespace TestRandomNumberGenerator {

TEST_CASE("[RandomNumberGenerator] Float") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0);

	INFO("Should give float between 0.0 and 1.0.");
	for (int i = 0; i < 1000; i++) {
		real_t n = rng->randf();
		CHECK(n >= 0.0);
		CHECK(n <= 1.0);
	}
}

TEST_CASE("[RandomNumberGenerator] Integer range via modulo") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0);

	INFO("Should give integer between 0 and 100.");
	for (int i = 0; i < 1000; i++) {
		uint32_t n = rng->randi() % 100;
		CHECK(n >= 0);
		CHECK(n <= 100);
	}
}

TEST_CASE_MAY_FAIL("[RandomNumberGenerator] Integer 32 bit") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0); // Change the seed if this fails.

	bool higher = false;
	int i;
	for (i = 0; i < 1000; i++) {
		uint32_t n = rng->randi();
		if (n > 0x0fff'ffff) {
			higher = true;
			break;
		}
	}
	INFO("Current seed: ", rng->get_seed());
	INFO("Current iteration: ", i);
	CHECK_MESSAGE(higher, "Given current seed, this should give an integer higher than 0x0fff'ffff at least once.");
}

TEST_CASE("[RandomNumberGenerator] Float and integer range") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0);
	uint64_t initial_state = rng->get_state();
	uint32_t initial_seed = rng->get_seed();

	INFO("Should give float between -100.0 and 100.0, base test.");
	for (int i = 0; i < 1000; i++) {
		real_t n0 = rng->randf_range(-100.0, 100.0);
		CHECK(n0 >= -100);
		CHECK(n0 <= 100);
	}

	rng->randomize();
	INFO("Should give float between -75.0 and 75.0.");
	INFO("Shouldn't be affected by randomize.");
	for (int i = 0; i < 1000; i++) {
		real_t n1 = rng->randf_range(-75.0, 75.0);
		CHECK(n1 >= -75);
		CHECK(n1 <= 75);
	}

	rng->set_state(initial_state);
	INFO("Should give integer between -50 and 50.");
	INFO("Shouldn't be affected by set_state.");
	for (int i = 0; i < 1000; i++) {
		real_t n2 = rng->randi_range(-50, 50);
		CHECK(n2 >= -50);
		CHECK(n2 <= 50);
	}

	rng->set_seed(initial_seed);
	INFO("Should give integer between -25 and 25.");
	INFO("Shouldn't be affected by set_seed.");
	for (int i = 0; i < 1000; i++) {
		int32_t n3 = rng->randi_range(-25, 25);
		CHECK(n3 >= -25);
		CHECK(n3 <= 25);
	}

	rng->randf();
	rng->randf();

	INFO("Should give float between -10.0 and 10.0.");
	INFO("Shouldn't be affected after generating new numbers.");
	for (int i = 0; i < 1000; i++) {
		real_t n4 = rng->randf_range(-10.0, 10.0);
		CHECK(n4 >= -10);
		CHECK(n4 <= 10);
	}

	rng->randi();
	rng->randi();

	INFO("Should give integer between -5 and 5.");
	INFO("Shouldn't be affected after generating new numbers.");
	for (int i = 0; i < 1000; i++) {
		real_t n5 = rng->randf_range(-5, 5);
		CHECK(n5 >= -5);
		CHECK(n5 <= 5);
	}
}

TEST_CASE_MAY_FAIL("[RandomNumberGenerator] Normal distribution") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(1); // Change the seed if this fails.
	INFO("Should give a number between -5 to 5 (5 std deviations away; above 99.7% chance it will be in this range).");
	INFO("Standard randfn function call.");
	for (int i = 0; i < 100; i++) {
		real_t n = rng->randfn();
		CHECK(n >= -5);
		CHECK(n <= 5);
	}

	INFO("Should give number between -5 to 5 after multiple randi/randf calls.");
	INFO("5 std deviations away; above 99.7% chance it will be in this range.");
	rng->randf();
	rng->randi();
	for (int i = 0; i < 100; i++) {
		real_t n = rng->randfn();
		CHECK(n >= -5);
		CHECK(n <= 5);
	}

	INFO("Checks if user defined mean and deviation work properly.");
	INFO("5 std deviations away; above 99.7% chance it will be in this range.");
	for (int i = 0; i < 100; i++) {
		real_t n = rng->randfn(5, 10);
		CHECK(n >= -45);
		CHECK(n <= 55);
	}

	INFO("Checks if randfn works with changed seeds.");
	INFO("5 std deviations away; above 99.7% chance it will be in this range.");
	rng->randomize();
	for (int i = 0; i < 100; i++) {
		real_t n = rng->randfn(3, 3);
		CHECK(n >= -12);
		CHECK(n <= 18);
	}
}

TEST_CASE("[RandomNumberGenerator] Zero for first number immediately after seeding") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0);
	uint32_t n1 = rng->randi();
	uint32_t n2 = rng->randi();
	INFO("Initial random values: ", n1, " ", n2);
	CHECK(n1 != 0);

	rng->set_seed(1);
	uint32_t n3 = rng->randi();
	uint32_t n4 = rng->randi();
	INFO("Values after changing the seed: ", n3, " ", n4);
	CHECK(n3 != 0);
}

TEST_CASE("[RandomNumberGenerator] Restore state") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->randomize();
	uint64_t last_seed = rng->get_seed();
	INFO("Current seed: ", last_seed);

	rng->randi();
	rng->randi();

	CHECK_MESSAGE(rng->get_seed() == last_seed,
			"The seed should remain the same after generating some numbers");

	uint64_t saved_state = rng->get_state();
	INFO("Current state: ", saved_state);

	real_t f1_before = rng->randf();
	real_t f2_before = rng->randf();
	INFO("This seed produces: ", f1_before, " ", f2_before);

	// Restore now.
	rng->set_state(saved_state);

	real_t f1_after = rng->randf();
	real_t f2_after = rng->randf();
	INFO("Resetting the state produces: ", f1_after, " ", f2_after);

	String msg = "Should restore the sequence of numbers after resetting the state";
	CHECK_MESSAGE(f1_before == f1_after, msg);
	CHECK_MESSAGE(f2_before == f2_after, msg);
}

TEST_CASE("[RandomNumberGenerator] Restore from seed") {
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	rng->set_seed(0);
	INFO("Current seed: ", rng->get_seed());
	uint32_t s0_1_before = rng->randi();
	uint32_t s0_2_before = rng->randi();
	INFO("This seed produces: ", s0_1_before, " ", s0_2_before);

	rng->set_seed(9000);
	INFO("Current seed: ", rng->get_seed());
	uint32_t s9000_1 = rng->randi();
	uint32_t s9000_2 = rng->randi();
	INFO("This seed produces: ", s9000_1, " ", s9000_2);

	rng->set_seed(0);
	INFO("Current seed: ", rng->get_seed());
	uint32_t s0_1_after = rng->randi();
	uint32_t s0_2_after = rng->randi();
	INFO("This seed produces: ", s0_1_after, " ", s0_2_after);

	String msg = "Should restore the sequence of numbers after resetting the seed";
	CHECK_MESSAGE(s0_1_before == s0_1_after, msg);
	CHECK_MESSAGE(s0_2_before == s0_2_after, msg);
}

TEST_CASE_MAY_FAIL("[RandomNumberGenerator] randi_range bias check") {
	int zeros = 0;
	int ones = 0;
	Ref<RandomNumberGenerator> rng = memnew(RandomNumberGenerator);
	for (int i = 0; i < 10000; i++) {
		int val = rng->randi_range(0, 1);
		val == 0 ? zeros++ : ones++;
	}
	CHECK_MESSAGE(abs(zeros * 1.0 / ones - 1.0) < 0.1, "The ratio of zeros to ones should be nearly 1");

	int vals[10] = { 0 };
	for (int i = 0; i < 1000000; i++) {
		vals[rng->randi_range(0, 9)]++;
	}

	for (int i = 0; i < 10; i++) {
		CHECK_MESSAGE(abs(vals[i] / 1000000.0 - 0.1) < 0.01, "Each element should appear roughly 10% of the time");
	}
}
} // namespace TestRandomNumberGenerator
