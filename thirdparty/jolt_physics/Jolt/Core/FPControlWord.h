// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

#if defined(JPH_CPU_WASM)

// Not supported

#elif defined(JPH_USE_SSE)

/// Helper class that needs to be put on the stack to update the state of the floating point control word.
/// This state is kept per thread.
template <uint Value, uint Mask>
class FPControlWord : public NonCopyable
{
public:
				FPControlWord()
	{
		mPrevState = _mm_getcsr();
		_mm_setcsr((mPrevState & ~Mask) | Value);
	}

				~FPControlWord()
	{
		_mm_setcsr((_mm_getcsr() & ~Mask) | (mPrevState & Mask));
	}

private:
	uint		mPrevState;
};

#elif defined(JPH_CPU_ARM) && defined(JPH_COMPILER_MSVC)

/// Helper class that needs to be put on the stack to update the state of the floating point control word.
/// This state is kept per thread.
template <unsigned int Value, unsigned int Mask>
class FPControlWord : public NonCopyable
{
public:
				FPControlWord()
	{
		// Read state before change
		_controlfp_s(&mPrevState, 0, 0);

		// Update the state
		unsigned int dummy;
		_controlfp_s(&dummy, Value, Mask);
	}

				~FPControlWord()
	{
		// Restore state
		unsigned int dummy;
		_controlfp_s(&dummy, mPrevState, Mask);
	}

private:
	unsigned int mPrevState;
};

#elif defined(JPH_CPU_ARM) && defined(JPH_USE_NEON)

/// Helper class that needs to be put on the stack to update the state of the floating point control word.
/// This state is kept per thread.
template <uint64 Value, uint64 Mask>
class FPControlWord : public NonCopyable
{
public:
				FPControlWord()
	{
		uint64 val;
		asm volatile("mrs %0, fpcr" : "=r" (val));
		mPrevState = val;
		val &= ~Mask;
		val |= Value;
		asm volatile("msr fpcr, %0" : /* no output */ : "r" (val));
	}

				~FPControlWord()
	{
		uint64 val;
		asm volatile("mrs %0, fpcr" : "=r" (val));
		val &= ~Mask;
		val |= mPrevState & Mask;
		asm volatile("msr fpcr, %0" : /* no output */ : "r" (val));
	}

private:
	uint64		mPrevState;
};

#elif defined(JPH_CPU_ARM)

/// Helper class that needs to be put on the stack to update the state of the floating point control word.
/// This state is kept per thread.
template <uint32 Value, uint32 Mask>
class FPControlWord : public NonCopyable
{
public:
	FPControlWord()
	{
		uint32 val;
		asm volatile("vmrs %0, fpscr" : "=r" (val));
		mPrevState = val;
		val &= ~Mask;
		val |= Value;
		asm volatile("vmsr fpscr, %0" : /* no output */ : "r" (val));
	}

	~FPControlWord()
	{
		uint32 val;
		asm volatile("vmrs %0, fpscr" : "=r" (val));
		val &= ~Mask;
		val |= mPrevState & Mask;
		asm volatile("vmsr fpscr, %0" : /* no output */ : "r" (val));
	}

private:
	uint32		mPrevState;
};

#else

#error Unsupported CPU architecture

#endif

JPH_NAMESPACE_END
