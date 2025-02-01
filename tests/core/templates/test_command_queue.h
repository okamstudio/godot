/**************************************************************************/
/*  test_command_queue.h                                                  */
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

#include "core/config/project_settings.h"
#include "core/math/random_number_generator.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/command_queue_mt.h"
#include "tests/test_macros.h"

namespace TestCommandQueue {

class ThreadWork {
	Semaphore thread_sem;
	Semaphore main_sem;
	Mutex mut;
	int threading_errors = 0;
	enum State {
		MAIN_START,
		MAIN_DONE,
		THREAD_START,
		THREAD_DONE,
	} state;

public:
	ThreadWork() {
		mut.lock();
		state = MAIN_START;
	}
	~ThreadWork() {
		CHECK_MESSAGE(threading_errors == 0, "threads did not lock/unlock correctly");
	}
	void thread_wait_for_work() {
		thread_sem.wait();
		mut.lock();
		if (state != MAIN_DONE) {
			threading_errors++;
		}
		state = THREAD_START;
	}
	void thread_done_work() {
		if (state != THREAD_START) {
			threading_errors++;
		}
		state = THREAD_DONE;
		mut.unlock();
		main_sem.post();
	}

	void main_wait_for_done() {
		main_sem.wait();
		mut.lock();
		if (state != THREAD_DONE) {
			threading_errors++;
		}
		state = MAIN_START;
	}
	void main_start_work() {
		if (state != MAIN_START) {
			threading_errors++;
		}
		state = MAIN_DONE;
		mut.unlock();
		thread_sem.post();
	}
};

class SharedThreadState {
public:
	ThreadWork reader_threadwork;
	ThreadWork writer_threadwork;

	CommandQueueMT command_queue;

	enum TestMsgType {
		TEST_MSG_FUNC1_TRANSFORM,
		TEST_MSG_FUNC2_TRANSFORM_FLOAT,
		TEST_MSG_FUNC3_TRANSFORMx6,
		TEST_MSGSYNC_FUNC1_TRANSFORM,
		TEST_MSGSYNC_FUNC2_TRANSFORM_FLOAT,
		TEST_MSGRET_FUNC1_TRANSFORM,
		TEST_MSGRET_FUNC2_TRANSFORM_FLOAT,
		TEST_MSG_MAX
	};

	Vector<TestMsgType> message_types_to_write;
	bool during_writing = false;
	int message_count_to_read = 0;
	bool exit_threads = false;

	Thread reader_thread;
	WorkerThreadPool::TaskID reader_task_id = WorkerThreadPool::INVALID_TASK_ID;
	Thread writer_thread;

	int func1_count = 0;

	void func1(Transform3D t) {
		func1_count++;
	}
	void func2(Transform3D t, float f) {
		func1_count++;
	}
	void func3(Transform3D t1, Transform3D t2, Transform3D t3, Transform3D t4, Transform3D t5, Transform3D t6) {
		func1_count++;
	}
	Transform3D func1r(Transform3D t) {
		func1_count++;
		return t;
	}
	Transform3D func2r(Transform3D t, float f) {
		func1_count++;
		return t;
	}

	void add_msg_to_write(TestMsgType type) {
		message_types_to_write.push_back(type);
	}

	void reader_thread_loop() {
		reader_threadwork.thread_wait_for_work();
		while (!exit_threads) {
			if (reader_task_id == WorkerThreadPool::INVALID_TASK_ID) {
				command_queue.flush_all();
			} else {
				if (message_count_to_read < 0) {
					command_queue.flush_all();
				}
				for (int i = 0; i < message_count_to_read; i++) {
					WorkerThreadPool::get_singleton()->yield();
					command_queue.wait_and_flush();
				}
			}
			message_count_to_read = 0;

			reader_threadwork.thread_done_work();
			reader_threadwork.thread_wait_for_work();
		}
		command_queue.flush_all();
		reader_threadwork.thread_done_work();
	}
	static void static_reader_thread_loop(void *stsvoid) {
		SharedThreadState *sts = static_cast<SharedThreadState *>(stsvoid);
		sts->reader_thread_loop();
	}

	void writer_thread_loop() {
		during_writing = false;
		writer_threadwork.thread_wait_for_work();
		while (!exit_threads) {
			Transform3D tr;
			Transform3D otr;
			float f = 1;
			during_writing = true;
			for (int i = 0; i < message_types_to_write.size(); i++) {
				TestMsgType msg_type = message_types_to_write[i];
				switch (msg_type) {
					case TEST_MSG_FUNC1_TRANSFORM:
						command_queue.push(this, &SharedThreadState::func1, tr);
						break;
					case TEST_MSG_FUNC2_TRANSFORM_FLOAT:
						command_queue.push(this, &SharedThreadState::func2, tr, f);
						break;
					case TEST_MSG_FUNC3_TRANSFORMx6:
						command_queue.push(this, &SharedThreadState::func3, tr, tr, tr, tr, tr, tr);
						break;
					case TEST_MSGSYNC_FUNC1_TRANSFORM:
						command_queue.push_and_sync(this, &SharedThreadState::func1, tr);
						break;
					case TEST_MSGSYNC_FUNC2_TRANSFORM_FLOAT:
						command_queue.push_and_sync(this, &SharedThreadState::func2, tr, f);
						break;
					case TEST_MSGRET_FUNC1_TRANSFORM:
						command_queue.push_and_ret(this, &SharedThreadState::func1r, &otr, tr);
						break;
					case TEST_MSGRET_FUNC2_TRANSFORM_FLOAT:
						command_queue.push_and_ret(this, &SharedThreadState::func2r, &otr, tr, f);
						break;
					default:
						break;
				}
			}
			message_types_to_write.clear();
			during_writing = false;

			writer_threadwork.thread_done_work();
			writer_threadwork.thread_wait_for_work();
		}
		writer_threadwork.thread_done_work();
	}
	static void static_writer_thread_loop(void *stsvoid) {
		SharedThreadState *sts = static_cast<SharedThreadState *>(stsvoid);
		sts->writer_thread_loop();
	}

	void init_threads(bool p_use_thread_pool_sync = false) {
		if (p_use_thread_pool_sync) {
			reader_task_id = WorkerThreadPool::get_singleton()->add_native_task(&SharedThreadState::static_reader_thread_loop, this, true);
			command_queue.set_pump_task_id(reader_task_id);
		} else {
			reader_thread.start(&SharedThreadState::static_reader_thread_loop, this);
		}
		writer_thread.start(&SharedThreadState::static_writer_thread_loop, this);
	}
	void destroy_threads() {
		exit_threads = true;
		reader_threadwork.main_start_work();
		writer_threadwork.main_start_work();

		if (reader_task_id != WorkerThreadPool::INVALID_TASK_ID) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(reader_task_id);
		} else {
			reader_thread.wait_to_finish();
		}
		writer_thread.wait_to_finish();
	}

	struct CopyMoveTestType {
		inline static int copy_count;
		inline static int move_count;
		int value = 0;

		CopyMoveTestType(int p_value = 0) :
				value(p_value) {}

		CopyMoveTestType(const CopyMoveTestType &p_other) :
				value(p_other.value) {
			copy_count++;
		}

		CopyMoveTestType(CopyMoveTestType &&p_other) :
				value(p_other.value) {
			move_count++;
		}

		CopyMoveTestType &operator=(const CopyMoveTestType &p_other) {
			value = p_other.value;
			copy_count++;
			return *this;
		}

		CopyMoveTestType &operator=(CopyMoveTestType &&p_other) {
			value = p_other.value;
			move_count++;
			return *this;
		}
	};

	void copy_move_test_copy(CopyMoveTestType p_test_type) {
	}
	void copy_move_test_ref(const CopyMoveTestType &p_test_type) {
	}
	void copy_move_test_move(CopyMoveTestType &&p_test_type) {
	}
};

static void test_command_queue_basic(bool p_use_thread_pool_sync) {
	const char *COMMAND_QUEUE_SETTING = "memory/limits/command_queue/multithreading_queue_size_kb";
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING, 1);
	SharedThreadState sts;
	sts.init_threads(p_use_thread_pool_sync);

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC1_TRANSFORM);
	sts.writer_threadwork.main_start_work();
	sts.writer_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 0,
			"Control: no messages read before reader has run.");

	sts.message_count_to_read = 1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 1,
			"Reader should have read one message");

	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 1,
			"Reader should have read no additional messages from flush_all");

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC1_TRANSFORM);
	sts.writer_threadwork.main_start_work();
	sts.writer_threadwork.main_wait_for_done();

	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 2,
			"Reader should have read one additional message from flush_all");

	sts.destroy_threads();

	CHECK_MESSAGE(sts.func1_count == 2,
			"Reader should have read no additional messages after join");
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING,
			ProjectSettings::get_singleton()->property_get_revert(COMMAND_QUEUE_SETTING));
}

TEST_CASE("[CommandQueue] Test Queue Basics") {
	test_command_queue_basic(false);
}

TEST_CASE("[CommandQueue] Test Queue Basics with WorkerThreadPool sync.") {
	test_command_queue_basic(true);
}

TEST_CASE("[CommandQueue] Test Queue Wrapping to same spot.") {
	const char *COMMAND_QUEUE_SETTING = "memory/limits/command_queue/multithreading_queue_size_kb";
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING, 1);
	SharedThreadState sts;
	sts.init_threads();

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC1_TRANSFORM);
	sts.writer_threadwork.main_start_work();
	sts.writer_threadwork.main_wait_for_done();

	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 3,
			"Reader should have read at least three messages");

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.writer_threadwork.main_start_work();
	sts.writer_threadwork.main_wait_for_done();
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC1_TRANSFORM);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.writer_threadwork.main_start_work();
	OS::get_singleton()->delay_usec(1000);

	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	OS::get_singleton()->delay_usec(1000);

	sts.writer_threadwork.main_wait_for_done();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count >= 3,
			"Reader should have read at least three messages");

	sts.message_count_to_read = 6 - sts.func1_count;
	sts.reader_threadwork.main_start_work();

	// The following will fail immediately.
	// The reason it hangs indefinitely in engine, is all subsequent calls to
	// CommandQueue.wait_and_flush_one will also fail.
	sts.reader_threadwork.main_wait_for_done();

	// Because looping around uses an extra message, easiest to consume all.
	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 6,
			"Reader should have read both message sets");

	sts.destroy_threads();

	CHECK_MESSAGE(sts.func1_count == 6,
			"Reader should have read no additional messages after join");
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING,
			ProjectSettings::get_singleton()->property_get_revert(COMMAND_QUEUE_SETTING));
}

TEST_CASE("[CommandQueue] Test Queue Lapping") {
	const char *COMMAND_QUEUE_SETTING = "memory/limits/command_queue/multithreading_queue_size_kb";
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING, 1);
	SharedThreadState sts;
	sts.init_threads();

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC1_TRANSFORM);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.writer_threadwork.main_start_work();
	sts.writer_threadwork.main_wait_for_done();

	// We need to read an extra message so that it triggers the dealloc logic once.
	// Otherwise, the queue will be considered full.
	sts.message_count_to_read = 3;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();
	CHECK_MESSAGE(sts.func1_count == 3,
			"Reader should have read first set of messages");

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC3_TRANSFORMx6);
	sts.writer_threadwork.main_start_work();
	// Don't wait for these, because the queue isn't big enough.
	sts.writer_threadwork.main_wait_for_done();

	sts.add_msg_to_write(SharedThreadState::TEST_MSG_FUNC2_TRANSFORM_FLOAT);
	sts.writer_threadwork.main_start_work();
	OS::get_singleton()->delay_usec(1000);

	sts.message_count_to_read = 3;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();

	sts.writer_threadwork.main_wait_for_done();

	sts.message_count_to_read = -1;
	sts.reader_threadwork.main_start_work();
	sts.reader_threadwork.main_wait_for_done();

	CHECK_MESSAGE(sts.func1_count == 6,
			"Reader should have read rest of the messages after lapping writers.");

	sts.destroy_threads();

	CHECK_MESSAGE(sts.func1_count == 6,
			"Reader should have read no additional messages after join");
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING,
			ProjectSettings::get_singleton()->property_get_revert(COMMAND_QUEUE_SETTING));
}

TEST_CASE("[Stress][CommandQueue] Stress test command queue") {
	const char *COMMAND_QUEUE_SETTING = "memory/limits/command_queue/multithreading_queue_size_kb";
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING, 1);
	SharedThreadState sts;
	sts.init_threads();

	RandomNumberGenerator rng;

	rng.set_seed(1837267);

	int msgs_to_add = 2048;

	for (int i = 0; i < msgs_to_add; i++) {
		// randi_range is inclusive, so allow any enum value except MAX.
		sts.add_msg_to_write((SharedThreadState::TestMsgType)rng.randi_range(0, SharedThreadState::TEST_MSG_MAX - 1));
	}
	sts.writer_threadwork.main_start_work();

	int max_loop_iters = msgs_to_add * 2;
	int loop_iters = 0;
	while (sts.func1_count < msgs_to_add && loop_iters < max_loop_iters) {
		int remaining = (msgs_to_add - sts.func1_count);
		sts.message_count_to_read = rng.randi_range(1, remaining < 128 ? remaining : 128);
		if (loop_iters % 3 == 0) {
			sts.message_count_to_read = -1;
		}
		sts.reader_threadwork.main_start_work();
		sts.reader_threadwork.main_wait_for_done();
		loop_iters++;
	}
	CHECK_MESSAGE(loop_iters < max_loop_iters,
			"Reader needed too many iterations to read messages!");
	sts.writer_threadwork.main_wait_for_done();

	sts.destroy_threads();

	CHECK_MESSAGE(sts.func1_count == msgs_to_add,
			"Reader should have read no additional messages after join");
	ProjectSettings::get_singleton()->set_setting(COMMAND_QUEUE_SETTING,
			ProjectSettings::get_singleton()->property_get_revert(COMMAND_QUEUE_SETTING));
}

TEST_CASE("[CommandQueue] Test Parameter Passing Semantics") {
	SharedThreadState sts;
	sts.init_threads();

	SUBCASE("Testing with lvalue") {
		SharedThreadState::CopyMoveTestType::copy_count = 0;
		SharedThreadState::CopyMoveTestType::move_count = 0;

		SharedThreadState::CopyMoveTestType lvalue(42);

		SUBCASE("Pass by copy") {
			sts.command_queue.push(&sts, &SharedThreadState::copy_move_test_copy, lvalue);

			sts.message_count_to_read = -1;
			sts.reader_threadwork.main_start_work();
			sts.reader_threadwork.main_wait_for_done();

			CHECK(SharedThreadState::CopyMoveTestType::copy_count == 1);
			CHECK(SharedThreadState::CopyMoveTestType::move_count == 1);
		}

		SUBCASE("Pass by reference") {
			sts.command_queue.push(&sts, &SharedThreadState::copy_move_test_ref, lvalue);

			sts.message_count_to_read = -1;
			sts.reader_threadwork.main_start_work();
			sts.reader_threadwork.main_wait_for_done();

			CHECK(SharedThreadState::CopyMoveTestType::copy_count == 1);
			CHECK(SharedThreadState::CopyMoveTestType::move_count == 0);
		}
	}

	SUBCASE("Testing with rvalue") {
		SharedThreadState::CopyMoveTestType::copy_count = 0;
		SharedThreadState::CopyMoveTestType::move_count = 0;

		SUBCASE("Pass by copy") {
			sts.command_queue.push(&sts, &SharedThreadState::copy_move_test_copy,
					SharedThreadState::CopyMoveTestType(43));

			sts.message_count_to_read = -1;
			sts.reader_threadwork.main_start_work();
			sts.reader_threadwork.main_wait_for_done();

			CHECK(SharedThreadState::CopyMoveTestType::copy_count == 0);
			CHECK(SharedThreadState::CopyMoveTestType::move_count == 2);
		}

		SUBCASE("Pass by reference") {
			sts.command_queue.push(&sts, &SharedThreadState::copy_move_test_ref,
					SharedThreadState::CopyMoveTestType(43));

			sts.message_count_to_read = -1;
			sts.reader_threadwork.main_start_work();
			sts.reader_threadwork.main_wait_for_done();

			CHECK(SharedThreadState::CopyMoveTestType::copy_count == 0);
			CHECK(SharedThreadState::CopyMoveTestType::move_count == 1);
		}

		SUBCASE("Pass by rvalue reference") {
			sts.command_queue.push(&sts, &SharedThreadState::copy_move_test_move,
					SharedThreadState::CopyMoveTestType(43));

			sts.message_count_to_read = -1;
			sts.reader_threadwork.main_start_work();
			sts.reader_threadwork.main_wait_for_done();

			CHECK(SharedThreadState::CopyMoveTestType::copy_count == 0);
			CHECK(SharedThreadState::CopyMoveTestType::move_count == 1);
		}
	}

	sts.destroy_threads();
}
} // namespace TestCommandQueue
