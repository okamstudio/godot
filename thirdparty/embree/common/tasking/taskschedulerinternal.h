// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../../include/embree4/rtcore.h"
#include "../sys/platform.h"
#include "../sys/alloc.h"
#include "../sys/barrier.h"
#include "../sys/thread.h"
#include "../sys/mutex.h"
#include "../sys/condition.h"
#include "../sys/ref.h"
#include "../sys/atomic.h"
#include "../math/range.h"

#include <list>

namespace embree
{

  /* The tasking system exports some symbols to be used by the tutorials. Thus we 
     hide is also in the API namespace when requested. */
  RTC_NAMESPACE_BEGIN

  struct TaskScheduler : public RefCount
  {
    ALIGNED_STRUCT_(64);
    friend class Device;

    static const size_t TASK_STACK_SIZE = 4*1024;           //!< task structure stack
    static const size_t CLOSURE_STACK_SIZE = 512*1024;    //!< stack for task closures

    struct Thread;

    /*! virtual interface for all tasks */
    struct TaskFunction {
      virtual void execute() = 0;
    };


    struct TaskGroupContext {
      TaskGroupContext() : cancellingException(nullptr) {}

      std::exception_ptr cancellingException;
    };

    /*! builds a task interface from a closure */
    template<typename Closure>
    struct ClosureTaskFunction : public TaskFunction
    {
      Closure closure;
      __forceinline ClosureTaskFunction (const Closure& closure) : closure(closure) {}
      void execute() { closure(); };
    };

    struct __aligned(64) Task
    {
      /*! states a task can be in */
      enum { DONE, INITIALIZED };

      /*! switch from one state to another */
      __forceinline void switch_state(int from, int to)
      {
	__memory_barrier();
        MAYBE_UNUSED bool success = state.compare_exchange_strong(from,to);
	assert(success);
      }

      /*! try to switch from one state to another */
      __forceinline bool try_switch_state(int from, int to) {
	__memory_barrier();
	return state.compare_exchange_strong(from,to);
      }

       /*! increment/decrement dependency counter */
      void add_dependencies(int n) {
	dependencies+=n;
      }

      /*! initialize all tasks to DONE state by default */
      __forceinline Task()
	: state(DONE) {}

      /*! construction of new task */
      __forceinline Task (TaskFunction* closure, Task* parent, TaskGroupContext* context, size_t stackPtr, size_t N)
        : dependencies(1), stealable(true), closure(closure), parent(parent), context(context), stackPtr(stackPtr), N(N)
      {
        if (parent) parent->add_dependencies(+1);
	switch_state(DONE,INITIALIZED);
      }

      /*! construction of stolen task, stealing thread will decrement initial dependency */
      __forceinline Task (TaskFunction* closure, Task* parent, TaskGroupContext* context)
        : dependencies(1), stealable(false), closure(closure), parent(parent), context(context), stackPtr(-1), N(1)
      {
	switch_state(DONE,INITIALIZED);
      }

      /*! try to steal this task */
      bool try_steal(Task& child)
      {
        if (!stealable) return false;
	if (!try_switch_state(INITIALIZED,DONE)) return false;
	new (&child) Task(closure, this, context);
        return true;
      }

      /*! run this task */
      dll_export void run(Thread& thread);

      void run_internal(Thread& thread);

    public:
      std::atomic<int> state;            //!< state this task is in
      std::atomic<int> dependencies;     //!< dependencies to wait for
      std::atomic<bool> stealable;       //!< true if task can be stolen
      TaskFunction* closure;             //!< the closure to execute
      Task* parent;                      //!< parent task to signal when we are finished
      TaskGroupContext* context;
      size_t stackPtr;                   //!< stack location where closure is stored
      size_t N;                          //!< approximative size of task
    };

    struct TaskQueue
    {
      TaskQueue ()
      : left(0), right(0), stackPtr(0) {}

      __forceinline void* alloc(size_t bytes, size_t align = 64)
      {
        size_t ofs = bytes + ((align - stackPtr) & (align-1));
        // -- GODOT start --
        // if (stackPtr + ofs > CLOSURE_STACK_SIZE)
        //   throw std::runtime_error("closure stack overflow");
        if (stackPtr + ofs > CLOSURE_STACK_SIZE) {
          abort();
        }
        // -- GODOT end --
        stackPtr += ofs;
        return &stack[stackPtr-bytes];
      }

      template<typename Closure>
      __forceinline void push_right(Thread& thread, const size_t size, const Closure& closure, TaskGroupContext* context)
      {
        // -- GODOT start --
        // if (right >= TASK_STACK_SIZE)
        //   throw std::runtime_error("task stack overflow");
        if (right >= TASK_STACK_SIZE) {
          abort();
        }
        // -- GODOT end --

	/* allocate new task on right side of stack */
        size_t oldStackPtr = stackPtr;
        TaskFunction* func = new (alloc(sizeof(ClosureTaskFunction<Closure>))) ClosureTaskFunction<Closure>(closure);
        new (&tasks[right.load()]) Task(func,thread.task,context,oldStackPtr,size);
        right++;

	/* also move left pointer */
	if (left >= right-1) left = right-1;
      }

      dll_export bool execute_local(Thread& thread, Task* parent);
      bool execute_local_internal(Thread& thread, Task* parent);
      bool steal(Thread& thread);
      size_t getTaskSizeAtLeft();

      bool empty() { return right == 0; }

    public:

      /* task stack */
      Task tasks[TASK_STACK_SIZE];
      __aligned(64) std::atomic<size_t> left;   //!< threads steal from left
      __aligned(64) std::atomic<size_t> right;  //!< new tasks are added to the right

      /* closure stack */
      __aligned(64) char stack[CLOSURE_STACK_SIZE];
      size_t stackPtr;
    };

    /*! thread local structure for each thread */
    struct Thread
    {
      ALIGNED_STRUCT_(64);

      Thread (size_t threadIndex, const Ref<TaskScheduler>& scheduler)
      : threadIndex(threadIndex), task(nullptr), scheduler(scheduler) {}

      __forceinline size_t threadCount() {
          return scheduler->threadCounter;
      }

      size_t threadIndex;              //!< ID of this thread
      TaskQueue tasks;                 //!< local task queue
      Task* task;                      //!< current active task
      Ref<TaskScheduler> scheduler;     //!< pointer to task scheduler
    };

    /*! pool of worker threads */
    struct ThreadPool
    {
      ThreadPool (bool set_affinity);
      ~ThreadPool ();

      /*! starts the threads */
      dll_export void startThreads();

      /*! sets number of threads to use */
      void setNumThreads(size_t numThreads, bool startThreads = false);

      /*! adds a task scheduler object for scheduling */
      dll_export void add(const Ref<TaskScheduler>& scheduler);

      /*! remove the task scheduler object again */
      dll_export void remove(const Ref<TaskScheduler>& scheduler);

      /*! returns number of threads of the thread pool */
      size_t size() const { return numThreads; }

      /*! main loop for all threads */
      void thread_loop(size_t threadIndex);

    private:
      std::atomic<size_t> numThreads;
      std::atomic<size_t> numThreadsRunning;
      bool set_affinity;
      std::atomic<bool> running;
      std::vector<thread_t> threads;

    private:
      MutexSys mutex;
      ConditionSys condition;
      std::list<Ref<TaskScheduler> > schedulers;
    };

    TaskScheduler ();
    ~TaskScheduler ();

    /*! initializes the task scheduler */
    static void create(size_t numThreads, bool set_affinity, bool start_threads);

    /*! destroys the task scheduler again */
    static void destroy();

    /*! lets new worker threads join the tasking system */
    void join();
    void reset();

    /*! let a worker thread allocate a thread index */
    dll_export ssize_t allocThreadIndex();

    /*! wait for some number of threads available (threadCount includes main thread) */
    void wait_for_threads(size_t threadCount);

    /*! thread loop for all worker threads */
    void thread_loop(size_t threadIndex);

    /*! steals a task from a different thread */
    bool steal_from_other_threads(Thread& thread);

    template<typename Predicate, typename Body>
      static void steal_loop(Thread& thread, const Predicate& pred, const Body& body);

    /* spawn a new task at the top of the threads task stack */
    template<typename Closure>
      void spawn_root(const Closure& closure, TaskGroupContext* context, size_t size = 1, bool useThreadPool = true)
    {
      if (useThreadPool) startThreads();

      size_t threadIndex = allocThreadIndex();
      std::unique_ptr<Thread> mthread(new Thread(threadIndex,this)); // too large for stack allocation
      Thread& thread = *mthread;
      assert(threadLocal[threadIndex].load() == nullptr);
      threadLocal[threadIndex] = &thread;
      Thread* oldThread = swapThread(&thread);
      thread.tasks.push_right(thread,size,closure,context);
      {
        Lock<MutexSys> lock(mutex);
	anyTasksRunning++;
        hasRootTask = true;
        condition.notify_all();
      }

      if (useThreadPool) addScheduler(this);

      while (thread.tasks.execute_local(thread,nullptr));
      anyTasksRunning--;
      if (useThreadPool) removeScheduler(this);

      threadLocal[threadIndex] = nullptr;
      swapThread(oldThread);

      /* remember exception to throw */
      std::exception_ptr except = nullptr;
      if (context->cancellingException != nullptr) except = context->cancellingException;

      /* wait for all threads to terminate */
      threadCounter--;
      while (threadCounter > 0) yield();
      context->cancellingException = nullptr;

      /* re-throw proper exception */
      if (except != nullptr) {
        // -- GODOT start --
        //std::rethrow_exception(except);
        abort();
        // -- GODOT end --
      }
    }

    /* spawn a new task at the top of the threads task stack */
    template<typename Closure>
    static __forceinline void spawn(size_t size, const Closure& closure, TaskGroupContext* context)
    {
      Thread* thread = TaskScheduler::thread();
      if (likely(thread != nullptr)) thread->tasks.push_right(*thread,size,closure,context);
      else                           instance()->spawn_root(closure,context,size);
    }

    /* spawn a new task at the top of the threads task stack */
    template<typename Closure>
    static __forceinline void spawn(const Closure& closure, TaskGroupContext* taskGroupContext) {
      spawn(1,closure,taskGroupContext);
    }

    /* spawn a new task set  */
    template<typename Index, typename Closure>
    static void spawn(const Index begin, const Index end, const Index blockSize, const Closure& closure, TaskGroupContext* context)
    {
      spawn(end-begin, [=]()
      {
        if (end-begin <= blockSize) {
          return closure(range<Index>(begin,end));
        }
        const Index center = (begin+end)/2;
        spawn(begin,center,blockSize,closure,context);
        spawn(center,end  ,blockSize,closure,context);
        wait();
      },context);
    }

    /* work on spawned subtasks and wait until all have finished */
    dll_export static void wait();

    /* returns the ID of the current thread */
    dll_export static size_t threadID();

    /* returns the index (0..threadCount-1) of the current thread */
    dll_export static size_t threadIndex();

    /* returns the total number of threads */
    dll_export static size_t threadCount();

  private:

    /* returns the thread local task list of this worker thread */
    dll_export static Thread* thread();

    /* sets the thread local task list of this worker thread */
    dll_export static Thread* swapThread(Thread* thread);

    /*! returns the taskscheduler object to be used by the master thread */
    dll_export static TaskScheduler* instance();

    /*! starts the threads */
    dll_export static void startThreads();

    /*! adds a task scheduler object for scheduling */
    dll_export static void addScheduler(const Ref<TaskScheduler>& scheduler);

    /*! remove the task scheduler object again */
    dll_export static void removeScheduler(const Ref<TaskScheduler>& scheduler);

  private:
    std::vector<atomic<Thread*>> threadLocal;
    std::atomic<size_t> threadCounter;
    std::atomic<size_t> anyTasksRunning;
    std::atomic<bool> hasRootTask;
    MutexSys mutex;
    ConditionSys condition;

  private:
    static size_t g_numThreads;
    static __thread TaskScheduler* g_instance;
    static __thread Thread* thread_local_thread;
    static ThreadPool* threadPool;
  };

  RTC_NAMESPACE_END

#if defined(RTC_NAMESPACE)
    using RTC_NAMESPACE::TaskScheduler;
#endif
}
