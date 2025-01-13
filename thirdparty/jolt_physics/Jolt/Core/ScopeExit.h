// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

/// Class that calls a function when it goes out of scope
template <class F>
class ScopeExit : public NonCopyable
{
public:
	/// Constructor specifies the exit function
	JPH_INLINE explicit	ScopeExit(F &&inFunction) : mFunction(std::move(inFunction)) { }

	/// Destructor calls the exit function
	JPH_INLINE			~ScopeExit() { if (!mInvoked) mFunction(); }

	/// Call the exit function now instead of when going out of scope
	JPH_INLINE void		Invoke()
	{
		if (!mInvoked)
		{
			mFunction();
			mInvoked = true;
		}
	}

	/// No longer call the exit function when going out of scope
	JPH_INLINE void		Release()
	{
		mInvoked = true;
	}

private:
	F					mFunction;
	bool				mInvoked = false;
};

#define JPH_SCOPE_EXIT_TAG2(line)			scope_exit##line
#define JPH_SCOPE_EXIT_TAG(line)			JPH_SCOPE_EXIT_TAG2(line)

/// Usage: JPH_SCOPE_EXIT([]{ code to call on scope exit });
#define JPH_SCOPE_EXIT(...) ScopeExit JPH_SCOPE_EXIT_TAG(__LINE__)(__VA_ARGS__)

JPH_NAMESPACE_END
