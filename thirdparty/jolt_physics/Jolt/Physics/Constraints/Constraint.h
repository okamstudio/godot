// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/Result.h>
#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class BodyID;
class IslandBuilder;
class LargeIslandSplitter;
class BodyManager;
class StateRecorder;
class StreamIn;
class StreamOut;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
#endif // JPH_DEBUG_RENDERER

/// Enum to identify constraint type
enum class EConstraintType
{
	Constraint,
	TwoBodyConstraint,
};

/// Enum to identify constraint sub type
enum class EConstraintSubType
{
	Fixed,
	Point,
	Hinge,
	Slider,
	Distance,
	Cone,
	SwingTwist,
	SixDOF,
	Path,
	Vehicle,
	RackAndPinion,
	Gear,
	Pulley,

	/// User defined constraint types start here
	User1,
	User2,
	User3,
	User4
};

/// Certain constraints support setting them up in local or world space. This governs what is used.
enum class EConstraintSpace
{
	LocalToBodyCOM,				///< All constraint properties are specified in local space to center of mass of the bodies that are being constrained (so e.g. 'constraint position 1' will be local to body 1 COM, 'constraint position 2' will be local to body 2 COM). Note that this means you need to subtract Shape::GetCenterOfMass() from positions!
	WorldSpace,					///< All constraint properties are specified in world space
};

/// Class used to store the configuration of a constraint. Allows run-time creation of constraints.
class JPH_EXPORT ConstraintSettings : public SerializableObject, public RefTarget<ConstraintSettings>
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, ConstraintSettings)

public:
	using ConstraintResult = Result<Ref<ConstraintSettings>>;

	/// Saves the contents of the constraint settings in binary form to inStream.
	virtual void				SaveBinaryState(StreamOut &inStream) const;

	/// Creates a constraint of the correct type and restores its contents from the binary stream inStream.
	static ConstraintResult		sRestoreFromBinaryState(StreamIn &inStream);

	/// If this constraint is enabled initially. Use Constraint::SetEnabled to toggle after creation.
	bool						mEnabled = true;

	/// Priority of the constraint when solving. Higher numbers have are more likely to be solved correctly.
	/// Note that if you want a deterministic simulation and you cannot guarantee the order in which constraints are added/removed, you can make the priority for all constraints unique to get a deterministic ordering.
	uint32						mConstraintPriority = 0;

	/// Used only when the constraint is active. Override for the number of solver velocity iterations to run, 0 means use the default in PhysicsSettings::mNumVelocitySteps. The number of iterations to use is the max of all contacts and constraints in the island.
	uint						mNumVelocityStepsOverride = 0;

	/// Used only when the constraint is active. Override for the number of solver position iterations to run, 0 means use the default in PhysicsSettings::mNumPositionSteps. The number of iterations to use is the max of all contacts and constraints in the island.
	uint						mNumPositionStepsOverride = 0;

	/// Size of constraint when drawing it through the debug renderer
	float						mDrawConstraintSize = 1.0f;

	/// User data value (can be used by application)
	uint64						mUserData = 0;

protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void				RestoreBinaryState(StreamIn &inStream);
};

/// Base class for all physics constraints. A constraint removes one or more degrees of freedom for a rigid body.
class JPH_EXPORT Constraint : public RefTarget<Constraint>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit					Constraint(const ConstraintSettings &inSettings) :
#ifdef JPH_DEBUG_RENDERER
		mDrawConstraintSize(inSettings.mDrawConstraintSize),
#endif // JPH_DEBUG_RENDERER
		mConstraintPriority(inSettings.mConstraintPriority),
		mNumVelocityStepsOverride(uint8(inSettings.mNumVelocityStepsOverride)),
		mNumPositionStepsOverride(uint8(inSettings.mNumPositionStepsOverride)),
		mEnabled(inSettings.mEnabled),
		mUserData(inSettings.mUserData)
	{
		JPH_ASSERT(inSettings.mNumVelocityStepsOverride < 256);
		JPH_ASSERT(inSettings.mNumPositionStepsOverride < 256);
	}

	/// Virtual destructor
	virtual						~Constraint() = default;

	/// Get the type of a constraint
	virtual EConstraintType		GetType() const								{ return EConstraintType::Constraint; }

	/// Get the sub type of a constraint
	virtual EConstraintSubType	GetSubType() const = 0;

	/// Priority of the constraint when solving. Higher numbers have are more likely to be solved correctly.
	/// Note that if you want a deterministic simulation and you cannot guarantee the order in which constraints are added/removed, you can make the priority for all constraints unique to get a deterministic ordering.
	uint32						GetConstraintPriority() const				{ return mConstraintPriority; }
	void						SetConstraintPriority(uint32 inPriority)	{ mConstraintPriority = inPriority; }

	/// Used only when the constraint is active. Override for the number of solver velocity iterations to run, 0 means use the default in PhysicsSettings::mNumVelocitySteps. The number of iterations to use is the max of all contacts and constraints in the island.
	void						SetNumVelocityStepsOverride(uint inN)		{ JPH_ASSERT(inN < 256); mNumVelocityStepsOverride = uint8(inN); }
	uint						GetNumVelocityStepsOverride() const			{ return mNumVelocityStepsOverride; }

	/// Used only when the constraint is active. Override for the number of solver position iterations to run, 0 means use the default in PhysicsSettings::mNumPositionSteps. The number of iterations to use is the max of all contacts and constraints in the island.
	void						SetNumPositionStepsOverride(uint inN)		{ JPH_ASSERT(inN < 256); mNumPositionStepsOverride = uint8(inN); }
	uint						GetNumPositionStepsOverride() const			{ return mNumPositionStepsOverride; }

	/// Enable / disable this constraint. This can e.g. be used to implement a breakable constraint by detecting that the constraint impulse
	/// (see e.g. PointConstraint::GetTotalLambdaPosition) went over a certain limit and then disabling the constraint.
	/// Note that although a disabled constraint will not affect the simulation in any way anymore, it does incur some processing overhead.
	/// Alternatively you can remove a constraint from the constraint manager (which may be more costly if you want to disable the constraint for a short while).
	void						SetEnabled(bool inEnabled)					{ mEnabled = inEnabled; }

	/// Test if a constraint is enabled.
	bool						GetEnabled() const							{ return mEnabled; }

	/// Access to the user data, can be used for anything by the application
	uint64						GetUserData() const							{ return mUserData; }
	void						SetUserData(uint64 inUserData)				{ mUserData = inUserData; }

	/// Notify the constraint that the shape of a body has changed and that its center of mass has moved by inDeltaCOM.
	/// Bodies don't know which constraints are connected to them so the user is responsible for notifying the relevant constraints when a body changes.
	/// @param inBodyID ID of the body that has changed
	/// @param inDeltaCOM The delta of the center of mass of the body (shape->GetCenterOfMass() - shape_before_change->GetCenterOfMass())
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) = 0;

	/// Notify the system that the configuration of the bodies and/or constraint has changed enough so that the warm start impulses should not be applied the next frame.
	/// You can use this function for example when repositioning a ragdoll through Ragdoll::SetPose in such a way that the orientation of the bodies completely changes so that
	/// the previous frame impulses are no longer a good approximation of what the impulses will be in the next frame. Calling this function when there are no big changes
	/// will result in the constraints being much 'softer' than usual so they are more easily violated (e.g. a long chain of bodies might sag a bit if you call this every frame).
	virtual void				ResetWarmStart() = 0;

	///@name Solver interface
	///@{
	virtual bool				IsActive() const							{ return mEnabled; }
	virtual void				SetupVelocityConstraint(float inDeltaTime) = 0;
	virtual void				WarmStartVelocityConstraint(float inWarmStartImpulseRatio) = 0;
	virtual bool				SolveVelocityConstraint(float inDeltaTime) = 0;
	virtual bool				SolvePositionConstraint(float inDeltaTime, float inBaumgarte) = 0;
	///@}

	/// Link bodies that are connected by this constraint in the island builder
	virtual void				BuildIslands(uint32 inConstraintIndex, IslandBuilder &ioBuilder, BodyManager &inBodyManager) = 0;

	/// Link bodies that are connected by this constraint in the same split. Returns the split index.
	virtual uint				BuildIslandSplits(LargeIslandSplitter &ioSplitter) const = 0;

#ifdef JPH_DEBUG_RENDERER
	// Drawing interface
	virtual void				DrawConstraint(DebugRenderer *inRenderer) const = 0;
	virtual void				DrawConstraintLimits([[maybe_unused]] DebugRenderer *inRenderer) const { }
	virtual void				DrawConstraintReferenceFrame([[maybe_unused]] DebugRenderer *inRenderer) const { }

	/// Size of constraint when drawing it through the debug renderer
	float						GetDrawConstraintSize() const				{ return mDrawConstraintSize; }
	void						SetDrawConstraintSize(float inSize)			{ mDrawConstraintSize = inSize; }
#endif // JPH_DEBUG_RENDERER

	/// Saving state for replay
	virtual void				SaveState(StateRecorder &inStream) const;

	/// Restoring state for replay
	virtual void				RestoreState(StateRecorder &inStream);

	/// Debug function to convert a constraint to its settings, note that this will not save to which bodies the constraint is connected to
	virtual Ref<ConstraintSettings> GetConstraintSettings() const = 0;

protected:
	/// Helper function to copy settings back to constraint settings for this base class
	void						ToConstraintSettings(ConstraintSettings &outSettings) const;

#ifdef JPH_DEBUG_RENDERER
	/// Size of constraint when drawing it through the debug renderer
	float						mDrawConstraintSize;
#endif // JPH_DEBUG_RENDERER

private:
	friend class ConstraintManager;

	/// Index that indicates this constraint is not in the constraint manager
	static constexpr uint32		cInvalidConstraintIndex = 0xffffffff;

	/// Index in the mConstraints list of the ConstraintManager for easy finding
	uint32						mConstraintIndex = cInvalidConstraintIndex;

	/// Priority of the constraint when solving. Higher numbers have are more likely to be solved correctly.
	uint32						mConstraintPriority = 0;

	/// Used only when the constraint is active. Override for the number of solver velocity iterations to run, 0 means use the default in PhysicsSettings::mNumVelocitySteps. The number of iterations to use is the max of all contacts and constraints in the island.
	uint8						mNumVelocityStepsOverride = 0;

	/// Used only when the constraint is active. Override for the number of solver position iterations to run, 0 means use the default in PhysicsSettings::mNumPositionSteps. The number of iterations to use is the max of all contacts and constraints in the island.
	uint8						mNumPositionStepsOverride = 0;

	/// If this constraint is currently enabled
	bool						mEnabled = true;

	/// User data value (can be used by application)
	uint64						mUserData;
};

JPH_NAMESPACE_END
