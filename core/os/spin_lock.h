/**************************************************************************/
/*  spin_lock.h                                                           */
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

#ifndef SPIN_LOCK_H
#define SPIN_LOCK_H

#include "core/os/thread.h"
#include "core/typedefs.h"

#ifdef THREADS_ENABLED

// Note the implementations below avoid false sharing by ensuring their
// sizes match the assumed cache line. We can't use align attributes
// because these objects may end up unaligned in semi-tightly packed arrays.

#ifdef _MSC_VER
#include <intrin.h>
#endif

#if defined(__APPLE__)

#include <os/lock.h>

class SpinLock {
	union {
		mutable os_unfair_lock _lock = OS_UNFAIR_LOCK_INIT;
		char aligner[Thread::CACHE_LINE_BYTES];
	};

public:
	_ALWAYS_INLINE_ void lock() const {
		os_unfair_lock_lock(&_lock);
	}

	_ALWAYS_INLINE_ void unlock() const {
		os_unfair_lock_unlock(&_lock);
	}
};

#else // __APPLE__

#include <atomic>

_ALWAYS_INLINE_ static void _cpu_pause() {
#if defined(_MSC_VER)
// ----- MSVC.
#if defined(_M_ARM) || defined(_M_ARM64) // ARM.
	__yield();
#elif defined(_M_IX86) || defined(_M_X64) // x86.
	_mm_pause();
#endif
#elif defined(__GNUC__) || defined(__clang__)
// ----- GCC/Clang.
#if defined(__i386__) || defined(__x86_64__) // x86.
	__builtin_ia32_pause();
#elif defined(__arm__) || defined(__aarch64__) // ARM.
	asm volatile("yield");
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) // PowerPC.
	asm volatile("or 27,27,27");
#elif defined(__riscv) // RISC-V.
	asm volatile(".insn i 0x0F, 0, x0, x0, 0x010");
#endif
#endif
}

static_assert(std::atomic_bool::is_always_lock_free);

class SpinLock {
	union {
		mutable std::atomic<bool> locked = ATOMIC_VAR_INIT(false);
		char aligner[Thread::CACHE_LINE_BYTES];
	};

public:
	_ALWAYS_INLINE_ void lock() const {
		while (true) {
			bool expected = false;
			if (locked.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
				break;
			}
			do {
				_cpu_pause();
			} while (locked.load(std::memory_order_relaxed));
		}
	}

	_ALWAYS_INLINE_ void unlock() const {
		locked.store(false, std::memory_order_release);
	}
};

#endif // __APPLE__

#else // THREADS_ENABLED

class SpinLock {
public:
	void lock() const {}
	void unlock() const {}
};

#endif // THREADS_ENABLED

#endif // SPIN_LOCK_H
