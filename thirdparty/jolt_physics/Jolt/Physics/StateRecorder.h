// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

class Body;
class Constraint;
class BodyID;

JPH_SUPPRESS_WARNING_PUSH
JPH_GCC_SUPPRESS_WARNING("-Wshadow") // GCC complains about the 'Constraints' value conflicting with the 'Constraints' typedef

/// A bit field that determines which aspects of the simulation to save
enum class EStateRecorderState : uint8
{
	None				= 0,														///< Save nothing
	Global				= 1,														///< Save global physics system state (delta time, gravity, etc.)
	Bodies				= 2,														///< Save the state of bodies
	Contacts			= 4,														///< Save the state of contacts
	Constraints			= 8,														///< Save the state of constraints
	All					= Global | Bodies | Contacts | Constraints					///< Save all state
};

JPH_SUPPRESS_WARNING_POP

/// Bitwise OR operator for EStateRecorderState
constexpr EStateRecorderState operator | (EStateRecorderState inLHS, EStateRecorderState inRHS)
{
	return EStateRecorderState(uint8(inLHS) | uint8(inRHS));
}

/// Bitwise AND operator for EStateRecorderState
constexpr EStateRecorderState operator & (EStateRecorderState inLHS, EStateRecorderState inRHS)
{
	return EStateRecorderState(uint8(inLHS) & uint8(inRHS));
}

/// Bitwise XOR operator for EStateRecorderState
constexpr EStateRecorderState operator ^ (EStateRecorderState inLHS, EStateRecorderState inRHS)
{
	return EStateRecorderState(uint8(inLHS) ^ uint8(inRHS));
}

/// Bitwise NOT operator for EStateRecorderState
constexpr EStateRecorderState operator ~ (EStateRecorderState inAllowedDOFs)
{
	return EStateRecorderState(~uint8(inAllowedDOFs));
}

/// Bitwise OR assignment operator for EStateRecorderState
constexpr EStateRecorderState & operator |= (EStateRecorderState &ioLHS, EStateRecorderState inRHS)
{
	ioLHS = ioLHS | inRHS;
	return ioLHS;
}

/// Bitwise AND assignment operator for EStateRecorderState
constexpr EStateRecorderState & operator &= (EStateRecorderState &ioLHS, EStateRecorderState inRHS)
{
	ioLHS = ioLHS & inRHS;
	return ioLHS;
}

/// Bitwise XOR assignment operator for EStateRecorderState
constexpr EStateRecorderState & operator ^= (EStateRecorderState &ioLHS, EStateRecorderState inRHS)
{
	ioLHS = ioLHS ^ inRHS;
	return ioLHS;
}

/// User callbacks that allow determining which parts of the simulation should be saved by a StateRecorder
class JPH_EXPORT StateRecorderFilter
{
public:
	/// Destructor
	virtual				~StateRecorderFilter() = default;

	///@name Functions called during SaveState
	///@{

	/// If the state of a specific body should be saved
	virtual bool		ShouldSaveBody([[maybe_unused]] const Body &inBody) const					{ return true; }

	/// If the state of a specific constraint should be saved
	virtual bool		ShouldSaveConstraint([[maybe_unused]] const Constraint &inConstraint) const	{ return true; }

	/// If the state of a specific contact should be saved
	virtual bool		ShouldSaveContact([[maybe_unused]] const BodyID &inBody1, [[maybe_unused]] const BodyID &inBody2) const { return true; }

	///@}
	///@name Functions called during RestoreState
	///@{

	/// If the state of a specific contact should be restored
	virtual bool		ShouldRestoreContact([[maybe_unused]] const BodyID &inBody1, [[maybe_unused]] const BodyID &inBody2) const { return true; }

	///@}
};

/// Class that records the state of a physics system. Can be used to check if the simulation is deterministic by putting the recorder in validation mode.
/// Can be used to restore the state to an earlier point in time. Note that only the state that is modified by the simulation is saved, configuration settings
/// like body friction or restitution, motion quality etc. are not saved and need to be saved by the user if desired.
class JPH_EXPORT StateRecorder : public StreamIn, public StreamOut
{
public:
	/// Constructor
						StateRecorder() = default;
						StateRecorder(const StateRecorder &inRHS)					: mIsValidating(inRHS.mIsValidating) { }

	/// Sets the stream in validation mode. In this case the physics system ensures that before it calls ReadBytes that it will
	/// ensure that those bytes contain the current state. This makes it possible to step and save the state, restore to the previous
	/// step and step again and when the recorded state is not the same it can restore the expected state and any byte that changes
	/// due to a ReadBytes function can be caught to find out which part of the simulation is not deterministic.
	/// Note that validation only works when saving the full state of the simulation (EStateRecorderState::All, StateRecorderFilter == nullptr).
	void				SetValidating(bool inValidating)							{ mIsValidating = inValidating; }
	bool				IsValidating() const										{ return mIsValidating; }

	/// This allows splitting the state in multiple parts. While restoring, only the last part should have this flag set to true.
	/// Note that you should ensure that the different parts contain information for disjoint sets of bodies, constraints and contacts.
	/// E.g. if you restore the same contact twice, you get undefined behavior. In order to create disjoint sets you can use the StateRecorderFilter.
	/// Note that validation is not compatible with restoring a simulation state in multiple parts.
	void				SetIsLastPart(bool inIsLastPart)							{ mIsLastPart = inIsLastPart; }
	bool				IsLastPart() const											{ return mIsLastPart; }

private:
	bool				mIsValidating = false;
	bool				mIsLastPart = true;
};

JPH_NAMESPACE_END
