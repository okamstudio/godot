/**************************************************************************/
/*  test_timer.h                                                          */
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

#include "scene/main/timer.h"

#include "tests/test_macros.h"

namespace TestTimer {

TEST_CASE("[SceneTree][Timer] Check Timer Setters and Getters") {
	Timer *test_timer = memnew(Timer);

	SUBCASE("[Timer] Timer set and get wait time") {
		// check default
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 1.0));

		test_timer->set_wait_time(50.0);
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 50.0));

		test_timer->set_wait_time(42.0);
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 42.0));

		// wait time remains unchanged if we attempt to set it negative or zero
		ERR_PRINT_OFF;
		test_timer->set_wait_time(-22.0);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 42.0));

		ERR_PRINT_OFF;
		test_timer->set_wait_time(0.0);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 42.0));
	}

	SUBCASE("[Timer] Timer set and get one shot") {
		// check default
		CHECK(test_timer->is_one_shot() == false);

		test_timer->set_one_shot(true);
		CHECK(test_timer->is_one_shot() == true);

		test_timer->set_one_shot(false);
		CHECK(test_timer->is_one_shot() == false);
	}

	SUBCASE("[Timer] Timer set and get autostart") {
		// check default
		CHECK(test_timer->has_autostart() == false);

		test_timer->set_autostart(true);
		CHECK(test_timer->has_autostart() == true);

		test_timer->set_autostart(false);
		CHECK(test_timer->has_autostart() == false);
	}

	SUBCASE("[Timer] Timer start and stop") {
		test_timer->set_autostart(false);
	}

	SUBCASE("[Timer] Timer set and get paused") {
		// check default
		CHECK(test_timer->is_paused() == false);

		test_timer->set_paused(true);
		CHECK(test_timer->is_paused() == true);

		test_timer->set_paused(false);
		CHECK(test_timer->is_paused() == false);
	}

	memdelete(test_timer);
}

TEST_CASE("[SceneTree][Timer] Check Timer Start and Stop") {
	Timer *test_timer = memnew(Timer);

	SUBCASE("[Timer] Timer start and stop") {
		SceneTree::get_singleton()->get_root()->add_child(test_timer);

		test_timer->start(5.0);

		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 5.0));
		CHECK(Math::is_equal_approx(test_timer->get_time_left(), 5.0));

		test_timer->start(-2.0);

		// the wait time and time left remains unchanged when started with a negative start time
		CHECK(Math::is_equal_approx(test_timer->get_wait_time(), 5.0));
		CHECK(Math::is_equal_approx(test_timer->get_time_left(), 5.0));

		test_timer->stop();
		CHECK(test_timer->is_processing() == false);
		CHECK(test_timer->has_autostart() == false);
	}

	memdelete(test_timer);
}

TEST_CASE("[SceneTree][Timer] Check Timer process callback") {
	Timer *test_timer = memnew(Timer);

	SUBCASE("[Timer] Timer process callback") {
		// check default
		CHECK(test_timer->get_timer_process_callback() == Timer::TimerProcessCallback::TIMER_PROCESS_IDLE);

		test_timer->set_timer_process_callback(Timer::TimerProcessCallback::TIMER_PROCESS_PHYSICS);
		CHECK(test_timer->get_timer_process_callback() == Timer::TimerProcessCallback::TIMER_PROCESS_PHYSICS);

		test_timer->set_timer_process_callback(Timer::TimerProcessCallback::TIMER_PROCESS_IDLE);
		CHECK(test_timer->get_timer_process_callback() == Timer::TimerProcessCallback::TIMER_PROCESS_IDLE);
	}

	memdelete(test_timer);
}

TEST_CASE("[SceneTree][Timer] Check Timer timeout signal") {
	Timer *test_timer = memnew(Timer);
	SceneTree::get_singleton()->get_root()->add_child(test_timer);

	test_timer->set_process(true);
	test_timer->set_physics_process(true);

	SUBCASE("[Timer] Timer process timeout signal must be emitted") {
		SIGNAL_WATCH(test_timer, SNAME("timeout"));
		test_timer->start(0.1);

		SceneTree::get_singleton()->process(0.2);

		Array signal_args;
		signal_args.push_back(Array());

		SIGNAL_CHECK(SNAME("timeout"), signal_args);

		SIGNAL_UNWATCH(test_timer, SNAME("timeout"));
	}

	SUBCASE("[Timer] Timer process timeout signal must not be emitted") {
		SIGNAL_WATCH(test_timer, SNAME("timeout"));
		test_timer->start(0.1);

		SceneTree::get_singleton()->process(0.05);

		Array signal_args;
		signal_args.push_back(Array());

		SIGNAL_CHECK_FALSE(SNAME("timeout"));

		SIGNAL_UNWATCH(test_timer, SNAME("timeout"));
	}

	test_timer->set_timer_process_callback(Timer::TimerProcessCallback::TIMER_PROCESS_PHYSICS);

	SUBCASE("[Timer] Timer physics process timeout signal must be emitted") {
		SIGNAL_WATCH(test_timer, SNAME("timeout"));
		test_timer->start(0.1);

		SceneTree::get_singleton()->physics_process(0.2);

		Array signal_args;
		signal_args.push_back(Array());

		SIGNAL_CHECK(SNAME("timeout"), signal_args);

		SIGNAL_UNWATCH(test_timer, SNAME("timeout"));
	}

	SUBCASE("[Timer] Timer physics process timeout signal must not be emitted") {
		SIGNAL_WATCH(test_timer, SNAME("timeout"));
		test_timer->start(0.1);

		SceneTree::get_singleton()->physics_process(0.05);

		Array signal_args;
		signal_args.push_back(Array());

		SIGNAL_CHECK_FALSE(SNAME("timeout"));

		SIGNAL_UNWATCH(test_timer, SNAME("timeout"));
	}

	memdelete(test_timer);
}

} // namespace TestTimer
