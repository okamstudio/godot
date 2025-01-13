// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

JPH_NAMESPACE_BEGIN

static void DummyTrace([[maybe_unused]] const char *inFMT, ...)
{
	JPH_ASSERT(false);
};

TraceFunction Trace = DummyTrace;

#ifdef JPH_ENABLE_ASSERTS

static bool DummyAssertFailed(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
	return true; // Trigger breakpoint
};

AssertFailedFunction AssertFailed = DummyAssertFailed;

#endif // JPH_ENABLE_ASSERTS

JPH_NAMESPACE_END
