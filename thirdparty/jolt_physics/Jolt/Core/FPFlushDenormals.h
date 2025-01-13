// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/FPControlWord.h>

JPH_NAMESPACE_BEGIN

#if defined(JPH_CPU_WASM)

// Not supported
class FPFlushDenormals { };

#elif defined(JPH_USE_SSE)

/// Helper class that needs to be put on the stack to enable flushing denormals to zero
/// This can make floating point operations much faster when working with very small numbers
class FPFlushDenormals : public FPControlWord<_MM_FLUSH_ZERO_ON, _MM_FLUSH_ZERO_MASK> { };

#elif defined(JPH_CPU_ARM) && defined(JPH_COMPILER_MSVC)

class FPFlushDenormals : public FPControlWord<_DN_FLUSH, _MCW_DN> { };

#elif defined(JPH_CPU_ARM)

/// Flush denormals to zero bit
static constexpr uint64 FP_FZ = 1 << 24;

/// Helper class that needs to be put on the stack to enable flushing denormals to zero
/// This can make floating point operations much faster when working with very small numbers
class FPFlushDenormals : public FPControlWord<FP_FZ, FP_FZ> { };

#else

#error Unsupported CPU architecture

#endif

JPH_NAMESPACE_END
