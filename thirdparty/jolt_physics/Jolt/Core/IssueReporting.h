// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Trace function, needs to be overridden by application. This should output a line of text to the log / TTY.
using TraceFunction = void (*)(const char *inFMT, ...);
JPH_EXPORT extern TraceFunction Trace;

// Always turn on asserts in Debug mode
#if defined(JPH_DEBUG) && !defined(JPH_ENABLE_ASSERTS)
	#define JPH_ENABLE_ASSERTS
#endif

#ifdef JPH_ENABLE_ASSERTS
	/// Function called when an assertion fails. This function should return true if a breakpoint needs to be triggered
	using AssertFailedFunction = bool(*)(const char *inExpression, const char *inMessage, const char *inFile, uint inLine);
	JPH_EXPORT extern AssertFailedFunction AssertFailed;

	// Helper functions to pass message on to failed function
	struct AssertLastParam { };
	inline bool AssertFailedParamHelper(const char *inExpression, const char *inFile, uint inLine, AssertLastParam) { return AssertFailed(inExpression, nullptr, inFile, inLine); }
	inline bool AssertFailedParamHelper(const char *inExpression, const char *inFile, uint inLine, const char *inMessage, AssertLastParam) { return AssertFailed(inExpression, inMessage, inFile, inLine); }

	/// Main assert macro, usage: JPH_ASSERT(condition, message) or JPH_ASSERT(condition)
	#define JPH_ASSERT(inExpression, ...)	do { if (!(inExpression) && AssertFailedParamHelper(#inExpression, __FILE__, JPH::uint(__LINE__), ##__VA_ARGS__, JPH::AssertLastParam())) JPH_BREAKPOINT; } while (false)

	#define JPH_IF_ENABLE_ASSERTS(...)		__VA_ARGS__
#else
	#define JPH_ASSERT(...)					((void)0)

	#define JPH_IF_ENABLE_ASSERTS(...)
#endif // JPH_ENABLE_ASSERTS

JPH_NAMESPACE_END
