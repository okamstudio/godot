// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <atomic>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

// Things we're using from STL
using std::atomic;
using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

/// Atomically compute the min(ioAtomic, inValue) and store it in ioAtomic, returns true if value was updated
template <class T>
bool AtomicMin(atomic<T> &ioAtomic, const T inValue, const memory_order inMemoryOrder = memory_order_seq_cst)
{
	T cur_value = ioAtomic.load(memory_order_relaxed);
	while (cur_value > inValue)
		if (ioAtomic.compare_exchange_weak(cur_value, inValue, inMemoryOrder))
			return true;
	return false;
}

/// Atomically compute the max(ioAtomic, inValue) and store it in ioAtomic, returns true if value was updated
template <class T>
bool AtomicMax(atomic<T> &ioAtomic, const T inValue, const memory_order inMemoryOrder = memory_order_seq_cst)
{
	T cur_value = ioAtomic.load(memory_order_relaxed);
	while (cur_value < inValue)
		if (ioAtomic.compare_exchange_weak(cur_value, inValue, inMemoryOrder))
			return true;
	return false;
}

JPH_NAMESPACE_END
