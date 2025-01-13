/**************************************************************************/
/*  worker_thread_pool.h                                                  */
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

#ifndef WORKER_THREAD_POOL_H
#define WORKER_THREAD_POOL_H

#include "core/os/condition_variable.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"

class WorkerThreadPool : public Object {
	GDCLASS(WorkerThreadPool, Object)
public:
	enum {
		INVALID_TASK_ID = -1
	};

	typedef int64_t TaskID;
	typedef int64_t GroupID;

private:
	struct Task;

	struct BaseTemplateUserdata {
		virtual void callback() {}
		virtual void callback_indexed(uint32_t p_index) {}
		virtual ~BaseTemplateUserdata() {}
	};

	struct Group {
		GroupID self = -1;
		SafeNumeric<uint32_t> index;
		SafeNumeric<uint32_t> completed_index;
		uint32_t max = 0;
		Semaphore done_semaphore;
		SafeFlag completed;
		SafeNumeric<uint32_t> finished;
		uint32_t tasks_used = 0;
	};

	struct Task {
		TaskID self = -1;
		Callable callable;
		void (*native_func)(void *) = nullptr;
		void (*native_group_func)(void *, uint32_t) = nullptr;
		void *native_func_userdata = nullptr;
		String description;
		Semaphore done_semaphore; // For user threads awaiting.
		bool completed : 1;
		bool pending_notify_yield_over : 1;
		Group *group = nullptr;
		SelfList<Task> task_elem;
		uint32_t waiting_pool = 0;
		uint32_t waiting_user = 0;
		bool low_priority = false;
		BaseTemplateUserdata *template_userdata = nullptr;
		int pool_thread_index = -1;

		void free_template_userdata();
		Task() :
				completed(false),
				pending_notify_yield_over(false),
				task_elem(this) {}
	};

	static const uint32_t TASKS_PAGE_SIZE = 1024;
	static const uint32_t GROUPS_PAGE_SIZE = 256;

	PagedAllocator<Task, false, TASKS_PAGE_SIZE> task_allocator;
	PagedAllocator<Group, false, GROUPS_PAGE_SIZE> group_allocator;

	SelfList<Task>::List low_priority_task_queue;
	SelfList<Task>::List task_queue;

	BinaryMutex task_mutex;

	struct ThreadData {
		static Task *const YIELDING; // Too bad constexpr doesn't work here.

		uint32_t index = 0;
		Thread thread;
		bool signaled : 1;
		bool yield_is_over : 1;
		bool pre_exited_languages : 1;
		bool exited_languages : 1;
		Task *current_task = nullptr;
		Task *awaited_task = nullptr; // Null if not awaiting the condition variable, or special value (YIELDING).
		ConditionVariable cond_var;

		ThreadData() :
				signaled(false),
				yield_is_over(false),
				pre_exited_languages(false),
				exited_languages(false) {}
	};

	TightLocalVector<ThreadData> threads;
	enum Runlevel {
		RUNLEVEL_NORMAL,
		RUNLEVEL_PRE_EXIT_LANGUAGES, // Block adding new tasks
		RUNLEVEL_EXIT_LANGUAGES, // All threads detach from scripting threads.
		RUNLEVEL_EXIT,
	} runlevel = RUNLEVEL_NORMAL;
	union { // Cleared on every runlevel change.
		struct {
			uint32_t num_idle_threads;
		} pre_exit_languages;
		struct {
			uint32_t num_exited_threads;
		} exit_languages;
	} runlevel_data;
	ConditionVariable control_cond_var;

	HashMap<Thread::ID, int> thread_ids;
	HashMap<
			TaskID,
			Task *,
			HashMapHasherDefault,
			HashMapComparatorDefault<TaskID>,
			PagedAllocator<HashMapElement<TaskID, Task *>, false, TASKS_PAGE_SIZE>>
			tasks;
	HashMap<
			GroupID,
			Group *,
			HashMapHasherDefault,
			HashMapComparatorDefault<GroupID>,
			PagedAllocator<HashMapElement<GroupID, Group *>, false, GROUPS_PAGE_SIZE>>
			groups;

	uint32_t max_low_priority_threads = 0;
	uint32_t low_priority_threads_used = 0;
	uint32_t notify_index = 0; // For rotating across threads, no help distributing load.

	uint64_t last_task = 1;

	static void _thread_function(void *p_user);

	void _process_task(Task *task);

	void _post_tasks(Task **p_tasks, uint32_t p_count, bool p_high_priority, MutexLock<BinaryMutex> &p_lock);
	void _notify_threads(const ThreadData *p_current_thread_data, uint32_t p_process_count, uint32_t p_promote_count);

	bool _try_promote_low_priority_task();

	static WorkerThreadPool *singleton;

#ifdef THREADS_ENABLED
	static const uint32_t MAX_UNLOCKABLE_LOCKS = 2;
	struct UnlockableLocks {
		THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> *ulock = nullptr;
		uint32_t rc = 0;
	};
	static thread_local UnlockableLocks unlockable_locks[MAX_UNLOCKABLE_LOCKS];
#endif

	TaskID _add_task(const Callable &p_callable, void (*p_func)(void *), void *p_userdata, BaseTemplateUserdata *p_template_userdata, bool p_high_priority, const String &p_description);
	GroupID _add_group_task(const Callable &p_callable, void (*p_func)(void *, uint32_t), void *p_userdata, BaseTemplateUserdata *p_template_userdata, int p_elements, int p_tasks, bool p_high_priority, const String &p_description);

	template <typename C, typename M, typename U>
	struct TaskUserData : public BaseTemplateUserdata {
		C *instance;
		M method;
		U userdata;
		virtual void callback() override {
			(instance->*method)(userdata);
		}
	};

	template <typename C, typename M, typename U>
	struct GroupUserData : public BaseTemplateUserdata {
		C *instance;
		M method;
		U userdata;
		virtual void callback_indexed(uint32_t p_index) override {
			(instance->*method)(p_index, userdata);
		}
	};

	void _wait_collaboratively(ThreadData *p_caller_pool_thread, Task *p_task);

	void _switch_runlevel(Runlevel p_runlevel);
	bool _handle_runlevel(ThreadData *p_thread_data, MutexLock<BinaryMutex> &p_lock);

#ifdef THREADS_ENABLED
	static uint32_t _thread_enter_unlock_allowance_zone(THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> &p_ulock);
#endif

	void _lock_unlockable_mutexes();
	void _unlock_unlockable_mutexes();

protected:
	static void _bind_methods();

public:
	template <typename C, typename M, typename U>
	TaskID add_template_task(C *p_instance, M p_method, U p_userdata, bool p_high_priority = false, const String &p_description = String()) {
		typedef TaskUserData<C, M, U> TUD;
		TUD *ud = memnew(TUD);
		ud->instance = p_instance;
		ud->method = p_method;
		ud->userdata = p_userdata;
		return _add_task(Callable(), nullptr, nullptr, ud, p_high_priority, p_description);
	}
	TaskID add_native_task(void (*p_func)(void *), void *p_userdata, bool p_high_priority = false, const String &p_description = String());
	TaskID add_task(const Callable &p_action, bool p_high_priority = false, const String &p_description = String());

	bool is_task_completed(TaskID p_task_id) const;
	Error wait_for_task_completion(TaskID p_task_id);

	void yield();
	void notify_yield_over(TaskID p_task_id);

	template <typename C, typename M, typename U>
	GroupID add_template_group_task(C *p_instance, M p_method, U p_userdata, int p_elements, int p_tasks = -1, bool p_high_priority = false, const String &p_description = String()) {
		typedef GroupUserData<C, M, U> GroupUD;
		GroupUD *ud = memnew(GroupUD);
		ud->instance = p_instance;
		ud->method = p_method;
		ud->userdata = p_userdata;
		return _add_group_task(Callable(), nullptr, nullptr, ud, p_elements, p_tasks, p_high_priority, p_description);
	}
	GroupID add_native_group_task(void (*p_func)(void *, uint32_t), void *p_userdata, int p_elements, int p_tasks = -1, bool p_high_priority = false, const String &p_description = String());
	GroupID add_group_task(const Callable &p_action, int p_elements, int p_tasks = -1, bool p_high_priority = false, const String &p_description = String());
	uint32_t get_group_processed_element_count(GroupID p_group) const;
	bool is_group_task_completed(GroupID p_group) const;
	void wait_for_group_task_completion(GroupID p_group);

	_FORCE_INLINE_ int get_thread_count() const {
#ifdef THREADS_ENABLED
		return threads.size();
#else
		return 1;
#endif
	}

	static WorkerThreadPool *get_singleton() { return singleton; }
	static int get_thread_index();
	static TaskID get_caller_task_id();

#ifdef THREADS_ENABLED
	_ALWAYS_INLINE_ static uint32_t thread_enter_unlock_allowance_zone(const MutexLock<BinaryMutex> &p_lock) { return _thread_enter_unlock_allowance_zone(p_lock._get_lock()); }
	template <int Tag>
	_ALWAYS_INLINE_ static uint32_t thread_enter_unlock_allowance_zone(const SafeBinaryMutex<Tag> &p_mutex) { return _thread_enter_unlock_allowance_zone(p_mutex._get_lock()); }
	static void thread_exit_unlock_allowance_zone(uint32_t p_zone_id);
#else
	static uint32_t thread_enter_unlock_allowance_zone(const MutexLock<BinaryMutex> &p_lock) { return UINT32_MAX; }
	template <int Tag>
	static uint32_t thread_enter_unlock_allowance_zone(const SafeBinaryMutex<Tag> &p_mutex) { return UINT32_MAX; }
	static void thread_exit_unlock_allowance_zone(uint32_t p_zone_id) {}
#endif

	void init(int p_thread_count = -1, float p_low_priority_task_ratio = 0.3);
	void exit_languages_threads();
	void finish();
	WorkerThreadPool();
	~WorkerThreadPool();
};

#endif // WORKER_THREAD_POOL_H
