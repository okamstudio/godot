/**************************************************************************/
/*  test_rid.h                                                            */
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

#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"

#include "tests/test_macros.h"

#ifdef SANITIZERS_ENABLED
#ifdef __has_feature
#if __has_feature(thread_sanitizer)
#define TSAN_ENABLED
#endif
#elif defined(__SANITIZE_THREAD__)
#define TSAN_ENABLED
#endif
#endif

#ifdef TSAN_ENABLED
#include <sanitizer/tsan_interface.h>
#endif

namespace TestRID {
TEST_CASE("[RID] Default Constructor") {
	RID rid;

	CHECK(rid.get_id() == 0);
}

TEST_CASE("[RID] Factory method") {
	RID rid = RID::from_uint64(1);

	CHECK(rid.get_id() == 1);
}

TEST_CASE("[RID] Operators") {
	RID rid = RID::from_uint64(1);

	RID rid_zero = RID::from_uint64(0);
	RID rid_one = RID::from_uint64(1);
	RID rid_two = RID::from_uint64(2);

	CHECK_FALSE(rid == rid_zero);
	CHECK(rid == rid_one);
	CHECK_FALSE(rid == rid_two);

	CHECK_FALSE(rid < rid_zero);
	CHECK_FALSE(rid < rid_one);
	CHECK(rid < rid_two);

	CHECK_FALSE(rid <= rid_zero);
	CHECK(rid <= rid_one);
	CHECK(rid <= rid_two);

	CHECK(rid > rid_zero);
	CHECK_FALSE(rid > rid_one);
	CHECK_FALSE(rid > rid_two);

	CHECK(rid >= rid_zero);
	CHECK(rid >= rid_one);
	CHECK_FALSE(rid >= rid_two);

	CHECK(rid != rid_zero);
	CHECK_FALSE(rid != rid_one);
	CHECK(rid != rid_two);
}

TEST_CASE("[RID] 'is_valid' & 'is_null'") {
	RID rid_zero = RID::from_uint64(0);
	RID rid_one = RID::from_uint64(1);

	CHECK_FALSE(rid_zero.is_valid());
	CHECK(rid_zero.is_null());

	CHECK(rid_one.is_valid());
	CHECK_FALSE(rid_one.is_null());
}

TEST_CASE("[RID] 'get_local_index'") {
	CHECK(RID::from_uint64(1).get_local_index() == 1);
	CHECK(RID::from_uint64(4'294'967'295).get_local_index() == 4'294'967'295);
	CHECK(RID::from_uint64(4'294'967'297).get_local_index() == 1);
}

// This case would let sanitizers realize data races.
// Additionally, on purely weakly ordered architectures, it would detect synchronization issues
// if RID_Alloc failed to impose proper memory ordering and the test's threads are distributed
// among multiple L1 caches.
TEST_CASE("[RID_Owner] Thread safety") {
	struct DataHolder {
		char data[Thread::CACHE_LINE_BYTES];
	};

	struct RID_OwnerTester {
		uint32_t thread_count = 0;
		RID_Owner<DataHolder, true> rid_owner;
		TightLocalVector<Thread> threads;
		SafeNumeric<uint32_t> next_thread_idx;
		// Using std::atomic directly since SafeNumeric doesn't support relaxed ordering.
		TightLocalVector<std::atomic<uint64_t>> rids;
		std::atomic<uint32_t> sync[2] = {};
		std::atomic<uint32_t> correct = 0;

		// A barrier that doesn't introduce memory ordering constraints, only compiler ones.
		// The idea is not to cause any sync traffic that would make the code we want to test
		// seem correct as a side effect.
		void lockstep(uint32_t p_step) {
			uint32_t buf_idx = p_step % 2;
			uint32_t target = (p_step / 2 + 1) * threads.size();
			sync[buf_idx].fetch_add(1, std::memory_order_relaxed);
			do {
				std::this_thread::yield();
			} while (sync[buf_idx].load(std::memory_order_relaxed) != target);
		}

		explicit RID_OwnerTester(bool p_chunk_for_all, bool p_chunks_preallocated) :
				thread_count(OS::get_singleton()->get_processor_count()),
				rid_owner(sizeof(DataHolder) * (p_chunk_for_all ? thread_count : 1)) {
			threads.resize(thread_count);
			rids.resize(threads.size());
			if (p_chunks_preallocated) {
				LocalVector<RID> prealloc_rids;
				for (uint32_t i = 0; i < (p_chunk_for_all ? 1 : threads.size()); i++) {
					prealloc_rids.push_back(rid_owner.make_rid());
				}
				for (uint32_t i = 0; i < prealloc_rids.size(); i++) {
					rid_owner.free(prealloc_rids[i]);
				}
			}
		}

		~RID_OwnerTester() {
			for (uint32_t i = 0; i < threads.size(); i++) {
				rid_owner.free(RID::from_uint64(rids[i].load(std::memory_order_relaxed)));
			}
		}

		void test() {
			for (uint32_t i = 0; i < threads.size(); i++) {
				threads[i].start(
						[](void *p_data) {
							RID_OwnerTester *rot = (RID_OwnerTester *)p_data;

							auto _compute_thread_unique_byte = [](uint32_t p_idx) -> char {
								return ((p_idx & 0xff) ^ (0b11111110 << (p_idx % 8)));
							};

							// 1. Each thread gets a zero-based index.
							uint32_t self_th_idx = rot->next_thread_idx.postincrement();

							rot->lockstep(0);

							// 2. Each thread makes a RID holding unique data.
							DataHolder initial_data;
							memset(&initial_data, _compute_thread_unique_byte(self_th_idx), Thread::CACHE_LINE_BYTES);
							RID my_rid = rot->rid_owner.make_rid(initial_data);
							rot->rids[self_th_idx].store(my_rid.get_id(), std::memory_order_relaxed);

							rot->lockstep(1);

							// 3. Each thread verifies all the others.
							uint32_t local_correct = 0;
							for (uint32_t th_idx = 0; th_idx < rot->threads.size(); th_idx++) {
								if (th_idx == self_th_idx) {
									continue;
								}
								char expected_unique_byte = _compute_thread_unique_byte(th_idx);
								RID rid = RID::from_uint64(rot->rids[th_idx].load(std::memory_order_relaxed));
								DataHolder *data = rot->rid_owner.get_or_null(rid);
#ifdef TSAN_ENABLED
								__tsan_acquire(data); // We know not a race in practice.
#endif
								bool ok = true;
								for (uint32_t j = 0; j < Thread::CACHE_LINE_BYTES; j++) {
									if (data->data[j] != expected_unique_byte) {
										ok = false;
										break;
									}
								}
								if (ok) {
									local_correct++;
								}
#ifdef TSAN_ENABLED
								__tsan_release(data);
#endif
							}

							rot->lockstep(2);

							rot->correct.fetch_add(local_correct, std::memory_order_acq_rel);
						},
						this);
			}

			for (uint32_t i = 0; i < threads.size(); i++) {
				threads[i].wait_to_finish();
			}

			CHECK_EQ(correct.load(), threads.size() * (threads.size() - 1));
		}
	};

	SUBCASE("All items in one chunk, pre-allocated") {
		RID_OwnerTester tester(true, true);
		tester.test();
	}
	SUBCASE("All items in one chunk, NOT pre-allocated") {
		RID_OwnerTester tester(true, false);
		tester.test();
	}
	SUBCASE("One item per chunk, pre-allocated") {
		RID_OwnerTester tester(false, true);
		tester.test();
	}
	SUBCASE("One item per chunk, NOT pre-allocated") {
		RID_OwnerTester tester(false, false);
		tester.test();
	}
}
} // namespace TestRID
