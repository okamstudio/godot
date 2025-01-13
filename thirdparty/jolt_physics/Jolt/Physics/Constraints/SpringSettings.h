// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

/// Enum used by constraints to specify how the spring is defined
enum class ESpringMode : uint8
{
	FrequencyAndDamping,		///< Frequency and damping are specified
	StiffnessAndDamping,		///< Stiffness and damping are specified
};

/// Settings for a linear or angular spring
class JPH_EXPORT SpringSettings
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SpringSettings)

public:
	/// Constructor
								SpringSettings() = default;
								SpringSettings(const SpringSettings &) = default;
	SpringSettings &			operator = (const SpringSettings &) = default;
								SpringSettings(ESpringMode inMode, float inFrequencyOrStiffness, float inDamping) : mMode(inMode), mFrequency(inFrequencyOrStiffness), mDamping(inDamping) { }

	/// Saves the contents of the spring settings in binary form to inStream.
	void						SaveBinaryState(StreamOut &inStream) const;

	/// Restores contents from the binary stream inStream.
	void						RestoreBinaryState(StreamIn &inStream);

	/// Check if the spring has a valid frequency / stiffness, if not the spring will be hard
	inline bool					HasStiffness() const							{ return mFrequency > 0.0f; }

	/// Selects the way in which the spring is defined
	/// If the mode is StiffnessAndDamping then mFrequency becomes the stiffness (k) and mDamping becomes the damping ratio (c) in the spring equation F = -k * x - c * v. Otherwise the properties are as documented.
	ESpringMode					mMode = ESpringMode::FrequencyAndDamping;

	union
	{
		/// Valid when mSpringMode = ESpringMode::FrequencyAndDamping.
		/// If mFrequency > 0 the constraint will be soft and mFrequency specifies the oscillation frequency in Hz.
		/// If mFrequency <= 0, mDamping is ignored and the constraint will have hard limits (as hard as the time step / the number of velocity / position solver steps allows).
		float					mFrequency = 0.0f;

		/// Valid when mSpringMode = ESpringMode::StiffnessAndDamping.
		/// If mStiffness > 0 the constraint will be soft and mStiffness specifies the stiffness (k) in the spring equation F = -k * x - c * v for a linear or T = -k * theta - c * w for an angular spring.
		/// If mStiffness <= 0, mDamping is ignored and the constraint will have hard limits (as hard as the time step / the number of velocity / position solver steps allows).
		///
		/// Note that stiffness values are large numbers. To calculate a ballpark value for the needed stiffness you can use:
		/// force = stiffness * delta_spring_length = mass * gravity <=> stiffness = mass * gravity / delta_spring_length.
		/// So if your object weighs 1500 kg and the spring compresses by 2 meters, you need a stiffness in the order of 1500 * 9.81 / 2 ~ 7500 N/m.
		float					mStiffness;
	};

	/// When mSpringMode = ESpringMode::FrequencyAndDamping mDamping is the damping ratio (0 = no damping, 1 = critical damping).
	/// When mSpringMode = ESpringMode::StiffnessAndDamping mDamping is the damping (c) in the spring equation F = -k * x - c * v for a linear or T = -k * theta - c * w for an angular spring.
	/// Note that if you set mDamping = 0, you will not get an infinite oscillation. Because we integrate physics using an explicit Euler scheme, there is always energy loss.
	/// This is done to keep the simulation from exploding, because with a damping of 0 and even the slightest rounding error, the oscillation could become bigger and bigger until the simulation explodes.
	float						mDamping = 0.0f;
};

JPH_NAMESPACE_END
