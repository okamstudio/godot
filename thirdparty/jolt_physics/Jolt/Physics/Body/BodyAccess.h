// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#ifdef JPH_ENABLE_ASSERTS

JPH_NAMESPACE_BEGIN

class JPH_EXPORT BodyAccess
{
public:
	/// Access rules, used to detect race conditions during simulation
	enum class EAccess : uint8
	{
		None		= 0,
		Read		= 1,
		ReadWrite	= 3,
	};

	/// Grant a scope specific access rights on the current thread
	class Grant
	{
	public:
		inline							Grant(EAccess inVelocity, EAccess inPosition)
		{
			EAccess &velocity = sVelocityAccess();
			EAccess &position = sPositionAccess();

			JPH_ASSERT(velocity == EAccess::ReadWrite);
			JPH_ASSERT(position == EAccess::ReadWrite);

			velocity = inVelocity;
			position = inPosition;
		}

		inline							~Grant()
		{
			sVelocityAccess() = EAccess::ReadWrite;
			sPositionAccess() = EAccess::ReadWrite;
		}
	};

	/// Check if we have permission
	static inline bool					sCheckRights(EAccess inRights, EAccess inDesiredRights)
	{
		return (uint8(inRights) & uint8(inDesiredRights)) == uint8(inDesiredRights);
	}

	/// Access to read/write velocities
	static inline EAccess &				sVelocityAccess()
	{
		static thread_local EAccess sAccess = BodyAccess::EAccess::ReadWrite;
		return sAccess;
	}

	/// Access to read/write positions
	static inline EAccess &				sPositionAccess()
	{
		static thread_local EAccess sAccess = BodyAccess::EAccess::ReadWrite;
		return sAccess;
	}
};

JPH_NAMESPACE_END

#endif // JPH_ENABLE_ASSERTS
