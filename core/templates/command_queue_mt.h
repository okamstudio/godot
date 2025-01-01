/**************************************************************************/
/*  command_queue_mt.h                                                    */
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

#ifndef COMMAND_QUEUE_MT_H
#define COMMAND_QUEUE_MT_H

#include "core/object/worker_thread_pool.h"
#include "core/os/condition_variable.h"
#include "core/os/memory.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/simple_type.h"
#include "core/typedefs.h"

#define COMMA(N) _COMMA_##N
#define _COMMA_0
#define _COMMA_1 ,
#define _COMMA_2 ,
#define _COMMA_3 ,
#define _COMMA_4 ,
#define _COMMA_5 ,
#define _COMMA_6 ,
#define _COMMA_7 ,
#define _COMMA_8 ,
#define _COMMA_9 ,
#define _COMMA_10 ,
#define _COMMA_11 ,
#define _COMMA_12 ,
#define _COMMA_13 ,
#define _COMMA_14 ,
#define _COMMA_15 ,

// 1-based comma separated list of ITEMs
#define COMMA_SEP_LIST(ITEM, LENGTH) _COMMA_SEP_LIST_##LENGTH(ITEM)
#define _COMMA_SEP_LIST_15(ITEM) \
	_COMMA_SEP_LIST_14(ITEM)     \
	, ITEM(15)
#define _COMMA_SEP_LIST_14(ITEM) \
	_COMMA_SEP_LIST_13(ITEM)     \
	, ITEM(14)
#define _COMMA_SEP_LIST_13(ITEM) \
	_COMMA_SEP_LIST_12(ITEM)     \
	, ITEM(13)
#define _COMMA_SEP_LIST_12(ITEM) \
	_COMMA_SEP_LIST_11(ITEM)     \
	, ITEM(12)
#define _COMMA_SEP_LIST_11(ITEM) \
	_COMMA_SEP_LIST_10(ITEM)     \
	, ITEM(11)
#define _COMMA_SEP_LIST_10(ITEM) \
	_COMMA_SEP_LIST_9(ITEM)      \
	, ITEM(10)
#define _COMMA_SEP_LIST_9(ITEM) \
	_COMMA_SEP_LIST_8(ITEM)     \
	, ITEM(9)
#define _COMMA_SEP_LIST_8(ITEM) \
	_COMMA_SEP_LIST_7(ITEM)     \
	, ITEM(8)
#define _COMMA_SEP_LIST_7(ITEM) \
	_COMMA_SEP_LIST_6(ITEM)     \
	, ITEM(7)
#define _COMMA_SEP_LIST_6(ITEM) \
	_COMMA_SEP_LIST_5(ITEM)     \
	, ITEM(6)
#define _COMMA_SEP_LIST_5(ITEM) \
	_COMMA_SEP_LIST_4(ITEM)     \
	, ITEM(5)
#define _COMMA_SEP_LIST_4(ITEM) \
	_COMMA_SEP_LIST_3(ITEM)     \
	, ITEM(4)
#define _COMMA_SEP_LIST_3(ITEM) \
	_COMMA_SEP_LIST_2(ITEM)     \
	, ITEM(3)
#define _COMMA_SEP_LIST_2(ITEM) \
	_COMMA_SEP_LIST_1(ITEM)     \
	, ITEM(2)
#define _COMMA_SEP_LIST_1(ITEM) \
	_COMMA_SEP_LIST_0(ITEM)     \
	ITEM(1)
#define _COMMA_SEP_LIST_0(ITEM)

// 1-based semicolon separated list of ITEMs
#define SEMIC_SEP_LIST(ITEM, LENGTH) _SEMIC_SEP_LIST_##LENGTH(ITEM)
#define _SEMIC_SEP_LIST_15(ITEM) \
	_SEMIC_SEP_LIST_14(ITEM);    \
	ITEM(15)
#define _SEMIC_SEP_LIST_14(ITEM) \
	_SEMIC_SEP_LIST_13(ITEM);    \
	ITEM(14)
#define _SEMIC_SEP_LIST_13(ITEM) \
	_SEMIC_SEP_LIST_12(ITEM);    \
	ITEM(13)
#define _SEMIC_SEP_LIST_12(ITEM) \
	_SEMIC_SEP_LIST_11(ITEM);    \
	ITEM(12)
#define _SEMIC_SEP_LIST_11(ITEM) \
	_SEMIC_SEP_LIST_10(ITEM);    \
	ITEM(11)
#define _SEMIC_SEP_LIST_10(ITEM) \
	_SEMIC_SEP_LIST_9(ITEM);     \
	ITEM(10)
#define _SEMIC_SEP_LIST_9(ITEM) \
	_SEMIC_SEP_LIST_8(ITEM);    \
	ITEM(9)
#define _SEMIC_SEP_LIST_8(ITEM) \
	_SEMIC_SEP_LIST_7(ITEM);    \
	ITEM(8)
#define _SEMIC_SEP_LIST_7(ITEM) \
	_SEMIC_SEP_LIST_6(ITEM);    \
	ITEM(7)
#define _SEMIC_SEP_LIST_6(ITEM) \
	_SEMIC_SEP_LIST_5(ITEM);    \
	ITEM(6)
#define _SEMIC_SEP_LIST_5(ITEM) \
	_SEMIC_SEP_LIST_4(ITEM);    \
	ITEM(5)
#define _SEMIC_SEP_LIST_4(ITEM) \
	_SEMIC_SEP_LIST_3(ITEM);    \
	ITEM(4)
#define _SEMIC_SEP_LIST_3(ITEM) \
	_SEMIC_SEP_LIST_2(ITEM);    \
	ITEM(3)
#define _SEMIC_SEP_LIST_2(ITEM) \
	_SEMIC_SEP_LIST_1(ITEM);    \
	ITEM(2)
#define _SEMIC_SEP_LIST_1(ITEM) \
	_SEMIC_SEP_LIST_0(ITEM)     \
	ITEM(1)
#define _SEMIC_SEP_LIST_0(ITEM)

// 1-based space separated list of ITEMs
#define SPACE_SEP_LIST(ITEM, LENGTH) _SPACE_SEP_LIST_##LENGTH(ITEM)
#define _SPACE_SEP_LIST_15(ITEM) \
	_SPACE_SEP_LIST_14(ITEM)     \
	ITEM(15)
#define _SPACE_SEP_LIST_14(ITEM) \
	_SPACE_SEP_LIST_13(ITEM)     \
	ITEM(14)
#define _SPACE_SEP_LIST_13(ITEM) \
	_SPACE_SEP_LIST_12(ITEM)     \
	ITEM(13)
#define _SPACE_SEP_LIST_12(ITEM) \
	_SPACE_SEP_LIST_11(ITEM)     \
	ITEM(12)
#define _SPACE_SEP_LIST_11(ITEM) \
	_SPACE_SEP_LIST_10(ITEM)     \
	ITEM(11)
#define _SPACE_SEP_LIST_10(ITEM) \
	_SPACE_SEP_LIST_9(ITEM)      \
	ITEM(10)
#define _SPACE_SEP_LIST_9(ITEM) \
	_SPACE_SEP_LIST_8(ITEM)     \
	ITEM(9)
#define _SPACE_SEP_LIST_8(ITEM) \
	_SPACE_SEP_LIST_7(ITEM)     \
	ITEM(8)
#define _SPACE_SEP_LIST_7(ITEM) \
	_SPACE_SEP_LIST_6(ITEM)     \
	ITEM(7)
#define _SPACE_SEP_LIST_6(ITEM) \
	_SPACE_SEP_LIST_5(ITEM)     \
	ITEM(6)
#define _SPACE_SEP_LIST_5(ITEM) \
	_SPACE_SEP_LIST_4(ITEM)     \
	ITEM(5)
#define _SPACE_SEP_LIST_4(ITEM) \
	_SPACE_SEP_LIST_3(ITEM)     \
	ITEM(4)
#define _SPACE_SEP_LIST_3(ITEM) \
	_SPACE_SEP_LIST_2(ITEM)     \
	ITEM(3)
#define _SPACE_SEP_LIST_2(ITEM) \
	_SPACE_SEP_LIST_1(ITEM)     \
	ITEM(2)
#define _SPACE_SEP_LIST_1(ITEM) \
	_SPACE_SEP_LIST_0(ITEM)     \
	ITEM(1)
#define _SPACE_SEP_LIST_0(ITEM)

#define ARG(N) p##N
#define PARAM(N) P##N p##N
#define TYPE_PARAM(N) typename P##N
#define PARAM_DECL(N) GetSimpleTypeT<P##N> p##N

#define DECL_CMD(N)                                                          \
	template <typename T, typename M COMMA(N) COMMA_SEP_LIST(TYPE_PARAM, N)> \
	struct Command##N : public CommandBase {                                 \
		T *instance;                                                         \
		M method;                                                            \
		SEMIC_SEP_LIST(PARAM_DECL, N);                                       \
		virtual void call() override {                                       \
			(instance->*method)(COMMA_SEP_LIST(ARG, N));                     \
		}                                                                    \
	};

#define DECL_CMD_RET(N)                                                                  \
	template <typename T, typename M, COMMA_SEP_LIST(TYPE_PARAM, N) COMMA(N) typename R> \
	struct CommandRet##N : public SyncCommand {                                          \
		R *ret;                                                                          \
		T *instance;                                                                     \
		M method;                                                                        \
		SEMIC_SEP_LIST(PARAM_DECL, N);                                                   \
		virtual void call() override {                                                   \
			*ret = (instance->*method)(COMMA_SEP_LIST(ARG, N));                          \
		}                                                                                \
	};

#define DECL_CMD_SYNC(N)                                                     \
	template <typename T, typename M COMMA(N) COMMA_SEP_LIST(TYPE_PARAM, N)> \
	struct CommandSync##N : public SyncCommand {                             \
		T *instance;                                                         \
		M method;                                                            \
		SEMIC_SEP_LIST(PARAM_DECL, N);                                       \
		virtual void call() override {                                       \
			(instance->*method)(COMMA_SEP_LIST(ARG, N));                     \
		}                                                                    \
	};

#define TYPE_ARG(N) P##N
#define CMD_TYPE(N) Command##N<T, M COMMA(N) COMMA_SEP_LIST(TYPE_ARG, N)>
#define CMD_ASSIGN_PARAM(N) cmd->p##N = p##N

#define DECL_PUSH(N)                                                            \
	template <typename T, typename M COMMA(N) COMMA_SEP_LIST(TYPE_PARAM, N)>    \
	void push(T *p_instance, M p_method COMMA(N) COMMA_SEP_LIST(PARAM, N)) {    \
		MutexLock mlock(mutex);                                                 \
		CMD_TYPE(N) *cmd = allocate<CMD_TYPE(N)>();                             \
		cmd->instance = p_instance;                                             \
		cmd->method = p_method;                                                 \
		SEMIC_SEP_LIST(CMD_ASSIGN_PARAM, N);                                    \
		if (pump_task_id != WorkerThreadPool::INVALID_TASK_ID) {                \
			WorkerThreadPool::get_singleton()->notify_yield_over(pump_task_id); \
		}                                                                       \
	}

#define CMD_RET_TYPE(N) CommandRet##N<T, M, COMMA_SEP_LIST(TYPE_ARG, N) COMMA(N) R>

#define DECL_PUSH_AND_RET(N)                                                                   \
	template <typename T, typename M, COMMA_SEP_LIST(TYPE_PARAM, N) COMMA(N) typename R>       \
	void push_and_ret(T *p_instance, M p_method, COMMA_SEP_LIST(PARAM, N) COMMA(N) R *r_ret) { \
		MutexLock mlock(mutex);                                                                \
		CMD_RET_TYPE(N) *cmd = allocate<CMD_RET_TYPE(N)>();                                    \
		cmd->instance = p_instance;                                                            \
		cmd->method = p_method;                                                                \
		SEMIC_SEP_LIST(CMD_ASSIGN_PARAM, N);                                                   \
		cmd->ret = r_ret;                                                                      \
		if (pump_task_id != WorkerThreadPool::INVALID_TASK_ID) {                               \
			WorkerThreadPool::get_singleton()->notify_yield_over(pump_task_id);                \
		}                                                                                      \
		sync_tail++;                                                                           \
		_wait_for_sync(mlock);                                                                 \
	}

#define CMD_SYNC_TYPE(N) CommandSync##N<T, M COMMA(N) COMMA_SEP_LIST(TYPE_ARG, N)>

#define DECL_PUSH_AND_SYNC(N)                                                         \
	template <typename T, typename M COMMA(N) COMMA_SEP_LIST(TYPE_PARAM, N)>          \
	void push_and_sync(T *p_instance, M p_method COMMA(N) COMMA_SEP_LIST(PARAM, N)) { \
		MutexLock mlock(mutex);                                                       \
		CMD_SYNC_TYPE(N) *cmd = allocate<CMD_SYNC_TYPE(N)>();                         \
		cmd->instance = p_instance;                                                   \
		cmd->method = p_method;                                                       \
		SEMIC_SEP_LIST(CMD_ASSIGN_PARAM, N);                                          \
		if (pump_task_id != WorkerThreadPool::INVALID_TASK_ID) {                      \
			WorkerThreadPool::get_singleton()->notify_yield_over(pump_task_id);       \
		}                                                                             \
		sync_tail++;                                                                  \
		_wait_for_sync(mlock);                                                        \
	}

#define MAX_CMD_PARAMS 15

class CommandQueueMT {
	struct CommandBase {
		bool sync = false;
		virtual void call() = 0;
		virtual ~CommandBase() = default;
	};

	struct SyncCommand : public CommandBase {
		virtual void call() override {}
		SyncCommand() {
			sync = true;
		}
	};

	DECL_CMD(0)
	SPACE_SEP_LIST(DECL_CMD, 15)

	// Commands that return.
	DECL_CMD_RET(0)
	SPACE_SEP_LIST(DECL_CMD_RET, 15)

	/* commands that don't return but sync */
	DECL_CMD_SYNC(0)
	SPACE_SEP_LIST(DECL_CMD_SYNC, 15)

	/***** BASE *******/

	static const uint32_t DEFAULT_COMMAND_MEM_SIZE_KB = 64;

	BinaryMutex mutex;
	LocalVector<uint8_t> command_mem;
	ConditionVariable sync_cond_var;
	uint32_t sync_head = 0;
	uint32_t sync_tail = 0;
	uint32_t sync_awaiters = 0;
	WorkerThreadPool::TaskID pump_task_id = WorkerThreadPool::INVALID_TASK_ID;
	uint64_t flush_read_ptr = 0;

	template <typename T>
	T *allocate() {
		// alloc size is size+T+safeguard
		static_assert(sizeof(T) < UINT32_MAX, "Type too large to fit in the command queue.");

		uint32_t alloc_size = ((sizeof(T) + 8U - 1U) & ~(8U - 1U));
		uint64_t size = command_mem.size();
		command_mem.resize(size + alloc_size + 8);
		*(uint64_t *)&command_mem[size] = alloc_size;
		T *cmd = memnew_placement(&command_mem[size + 8], T);
		return cmd;
	}

	_FORCE_INLINE_ void _prevent_sync_wraparound() {
		bool safe_to_reset = !sync_awaiters;
		bool already_sync_to_latest = sync_head == sync_tail;
		if (safe_to_reset && already_sync_to_latest) {
			sync_head = 0;
			sync_tail = 0;
		}
	}

	void _flush() {
		if (unlikely(flush_read_ptr)) {
			// Re-entrant call.
			return;
		}

		MutexLock lock(mutex);

		while (flush_read_ptr < command_mem.size()) {
			uint64_t size = *(uint64_t *)&command_mem[flush_read_ptr];
			flush_read_ptr += 8;
			CommandBase *cmd = reinterpret_cast<CommandBase *>(&command_mem[flush_read_ptr]);
			uint32_t allowance_id = WorkerThreadPool::thread_enter_unlock_allowance_zone(lock);
			cmd->call();
			WorkerThreadPool::thread_exit_unlock_allowance_zone(allowance_id);

			// Handle potential realloc due to the command and unlock allowance.
			cmd = reinterpret_cast<CommandBase *>(&command_mem[flush_read_ptr]);

			if (unlikely(cmd->sync)) {
				sync_head++;
				lock.~MutexLock(); // Give an opportunity to awaiters right away.
				sync_cond_var.notify_all();
				new (&lock) MutexLock(mutex);
				// Handle potential realloc happened during unlock.
				cmd = reinterpret_cast<CommandBase *>(&command_mem[flush_read_ptr]);
			}

			cmd->~CommandBase();

			flush_read_ptr += size;
		}

		command_mem.clear();
		flush_read_ptr = 0;

		_prevent_sync_wraparound();
	}

	_FORCE_INLINE_ void _wait_for_sync(MutexLock<BinaryMutex> &p_lock) {
		sync_awaiters++;
		uint32_t sync_head_goal = sync_tail;
		do {
			sync_cond_var.wait(p_lock);
		} while (sync_head < sync_head_goal);
		sync_awaiters--;
		_prevent_sync_wraparound();
	}

	void _no_op() {}

public:
	/* NORMAL PUSH COMMANDS */
	DECL_PUSH(0)
	SPACE_SEP_LIST(DECL_PUSH, 15)

	/* PUSH AND RET COMMANDS */
	DECL_PUSH_AND_RET(0)
	SPACE_SEP_LIST(DECL_PUSH_AND_RET, 15)

	/* PUSH AND RET SYNC COMMANDS*/
	DECL_PUSH_AND_SYNC(0)
	SPACE_SEP_LIST(DECL_PUSH_AND_SYNC, 15)

	_FORCE_INLINE_ void flush_if_pending() {
		if (unlikely(command_mem.size() > 0)) {
			_flush();
		}
	}

	void flush_all() {
		_flush();
	}

	void sync() {
		push_and_sync(this, &CommandQueueMT::_no_op);
	}

	void wait_and_flush() {
		ERR_FAIL_COND(pump_task_id == WorkerThreadPool::INVALID_TASK_ID);
		WorkerThreadPool::get_singleton()->wait_for_task_completion(pump_task_id);
		_flush();
	}

	void set_pump_task_id(WorkerThreadPool::TaskID p_task_id) {
		MutexLock lock(mutex);
		pump_task_id = p_task_id;
	}

	CommandQueueMT();
	~CommandQueueMT();
};

#undef ARG
#undef PARAM
#undef TYPE_PARAM
#undef PARAM_DECL
#undef DECL_CMD
#undef DECL_CMD_RET
#undef DECL_CMD_SYNC
#undef TYPE_ARG
#undef CMD_TYPE
#undef CMD_ASSIGN_PARAM
#undef DECL_PUSH
#undef CMD_RET_TYPE
#undef DECL_PUSH_AND_RET
#undef CMD_SYNC_TYPE
#undef DECL_CMD_SYNC

#endif // COMMAND_QUEUE_MT_H
