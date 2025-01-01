// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// If objects are closer than this distance, they are considered to be colliding (used for GJK) (unit: meter)
constexpr float cDefaultCollisionTolerance = 1.0e-4f;

/// A factor that determines the accuracy of the penetration depth calculation. If the change of the squared distance is less than tolerance * current_penetration_depth^2 the algorithm will terminate. (unit: dimensionless)
constexpr float cDefaultPenetrationTolerance = 1.0e-4f; ///< Stop when there's less than 1% change

/// How much padding to add around objects
constexpr float cDefaultConvexRadius = 0.05f;

/// Used by (Tapered)CapsuleShape to determine when supporting face is an edge rather than a point (unit: meter)
static constexpr float cCapsuleProjectionSlop = 0.02f;

/// Maximum amount of jobs to allow
constexpr int cMaxPhysicsJobs = 2048;

/// Maximum amount of barriers to allow
constexpr int cMaxPhysicsBarriers = 8;

struct PhysicsSettings
{
	JPH_OVERRIDE_NEW_DELETE

	/// Size of body pairs array, corresponds to the maximum amount of potential body pairs that can be in flight at any time.
	/// Setting this to a low value will use less memory but slow down simulation as threads may run out of narrow phase work.
	int			mMaxInFlightBodyPairs = 16384;

	/// How many PhysicsStepListeners to notify in 1 batch
	int			mStepListenersBatchSize = 8;

	/// How many step listener batches are needed before spawning another job (set to INT_MAX if no parallelism is desired)
	int			mStepListenerBatchesPerJob = 1;

	/// Baumgarte stabilization factor (how much of the position error to 'fix' in 1 update) (unit: dimensionless, 0 = nothing, 1 = 100%)
	float		mBaumgarte = 0.2f;

	/// Radius around objects inside which speculative contact points will be detected. Note that if this is too big
	/// you will get ghost collisions as speculative contacts are based on the closest points during the collision detection
	/// step which may not be the actual closest points by the time the two objects hit (unit: meters)
	float		mSpeculativeContactDistance = 0.02f;

	/// How much bodies are allowed to sink into each other (unit: meters)
	float		mPenetrationSlop = 0.02f;

	/// Fraction of its inner radius a body must move per step to enable casting for the LinearCast motion quality
	float		mLinearCastThreshold = 0.75f;

	/// Fraction of its inner radius a body may penetrate another body for the LinearCast motion quality
	float		mLinearCastMaxPenetration = 0.25f;

	/// Max squared distance to use to determine if two points are on the same plane for determining the contact manifold between two shape faces (unit: meter^2)
	float		mManifoldToleranceSq = 1.0e-6f;

	/// Maximum distance to correct in a single iteration when solving position constraints (unit: meters)
	float		mMaxPenetrationDistance = 0.2f;

	/// Maximum relative delta position for body pairs to be able to reuse collision results from last frame (units: meter^2)
	float		mBodyPairCacheMaxDeltaPositionSq = Square(0.001f); ///< 1 mm

	/// Maximum relative delta orientation for body pairs to be able to reuse collision results from last frame, stored as cos(max angle / 2)
	float		mBodyPairCacheCosMaxDeltaRotationDiv2 = 0.99984769515639123915701155881391f; ///< cos(2 degrees / 2)

	/// Maximum angle between normals that allows manifolds between different sub shapes of the same body pair to be combined
	float		mContactNormalCosMaxDeltaRotation = 0.99619469809174553229501040247389f; ///< cos(5 degree)

	/// Maximum allowed distance between old and new contact point to preserve contact forces for warm start (units: meter^2)
	float		mContactPointPreserveLambdaMaxDistSq = Square(0.01f); ///< 1 cm

	/// Number of solver velocity iterations to run
	/// Note that this needs to be >= 2 in order for friction to work (friction is applied using the non-penetration impulse from the previous iteration)
	uint		mNumVelocitySteps = 10;

	/// Number of solver position iterations to run
	uint		mNumPositionSteps = 2;

	/// Minimal velocity needed before a collision can be elastic. If the relative velocity between colliding objects
	/// in the direction of the contact normal is lower than this, the restitution will be zero regardless of the configured
	/// value. This lets an object settle sooner. Must be a positive number. (unit: m)
	float		mMinVelocityForRestitution = 1.0f;

	/// Time before object is allowed to go to sleep (unit: seconds)
	float		mTimeBeforeSleep = 0.5f;

	/// To detect if an object is sleeping, we use 3 points:
	/// - The center of mass.
	/// - The centers of the faces of the bounding box that are furthest away from the center.
	/// The movement of these points is tracked and if the velocity of all 3 points is lower than this value,
	/// the object is allowed to go to sleep. Must be a positive number. (unit: m/s)
	float		mPointVelocitySleepThreshold = 0.03f;

	/// By default the simulation is deterministic, it is possible to turn this off by setting this setting to false. This will make the simulation run faster but it will no longer be deterministic.
	bool		mDeterministicSimulation = true;

	///@name These variables are mainly for debugging purposes, they allow turning on/off certain subsystems. You probably want to leave them alone.
	///@{

	/// Whether or not to use warm starting for constraints (initially applying previous frames impulses)
	bool		mConstraintWarmStart = true;

	/// Whether or not to use the body pair cache, which removes the need for narrow phase collision detection when orientation between two bodies didn't change
	bool		mUseBodyPairContactCache = true;

	/// Whether or not to reduce manifolds with similar contact normals into one contact manifold (see description at Body::SetUseManifoldReduction)
	bool		mUseManifoldReduction = true;

	/// If we split up large islands into smaller parallel batches of work (to improve performance)
	bool		mUseLargeIslandSplitter = true;

	/// If objects can go to sleep or not
	bool		mAllowSleeping = true;

	/// When false, we prevent collision against non-active (shared) edges. Mainly for debugging the algorithm.
	bool		mCheckActiveEdges = true;

	///@}
};

JPH_NAMESPACE_END
