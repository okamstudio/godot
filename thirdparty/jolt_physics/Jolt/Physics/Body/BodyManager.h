// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Core/Mutex.h>
#include <Jolt/Core/MutexArray.h>

JPH_NAMESPACE_BEGIN

// Classes
class BodyCreationSettings;
class SoftBodyCreationSettings;
class BodyActivationListener;
class StateRecorderFilter;
struct PhysicsSettings;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
class BodyDrawFilter;
#endif // JPH_DEBUG_RENDERER

#ifdef JPH_DEBUG_RENDERER

/// Defines how to color soft body constraints
enum class ESoftBodyConstraintColor
{
	ConstraintType,				/// Draw different types of constraints in different colors
	ConstraintGroup,			/// Draw constraints in the same group in the same color, non-parallel group will be red
	ConstraintOrder,			/// Draw constraints in the same group in the same color, non-parallel group will be red, and order within each group will be indicated with gradient
};

#endif // JPH_DEBUG_RENDERER

/// Array of bodies
using BodyVector = Array<Body *>;

/// Array of body ID's
using BodyIDVector = Array<BodyID>;

/// Class that contains all bodies
class JPH_EXPORT BodyManager : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Destructor
									~BodyManager();

	/// Initialize the manager
	void							Init(uint inMaxBodies, uint inNumBodyMutexes, const BroadPhaseLayerInterface &inLayerInterface);

	/// Gets the current amount of bodies that are in the body manager
	uint							GetNumBodies() const;

	/// Gets the max bodies that we can support
	uint							GetMaxBodies() const						{ return uint(mBodies.capacity()); }

	/// Helper struct that counts the number of bodies of each type
	struct BodyStats
	{
		uint						mNumBodies					= 0;			///< Total number of bodies in the body manager
		uint						mMaxBodies					= 0;			///< Max allowed number of bodies in the body manager (as configured in Init(...))

		uint						mNumBodiesStatic			= 0;			///< Number of static bodies

		uint						mNumBodiesDynamic			= 0;			///< Number of dynamic bodies
		uint						mNumActiveBodiesDynamic		= 0;			///< Number of dynamic bodies that are currently active

		uint						mNumBodiesKinematic			= 0;			///< Number of kinematic bodies
		uint						mNumActiveBodiesKinematic	= 0;			///< Number of kinematic bodies that are currently active

		uint						mNumSoftBodies				= 0;			///< Number of soft bodies
		uint						mNumActiveSoftBodies		= 0;			///< Number of soft bodies that are currently active
	};

	/// Get stats about the bodies in the body manager (slow, iterates through all bodies)
	BodyStats						GetBodyStats() const;

	/// Create a body using creation settings. The returned body will not be part of the body manager yet.
	Body *							AllocateBody(const BodyCreationSettings &inBodyCreationSettings) const;

	/// Create a soft body using creation settings. The returned body will not be part of the body manager yet.
	Body *							AllocateSoftBody(const SoftBodyCreationSettings &inSoftBodyCreationSettings) const;

	/// Free a body that has not been added to the body manager yet (if it has, use DestroyBodies).
	void							FreeBody(Body *inBody) const;

	/// Add a body to the body manager, assigning it the next available ID. Returns false if no more IDs are available.
	bool							AddBody(Body *ioBody);

	/// Add a body to the body manager, assigning it a custom ID. Returns false if the ID is not valid.
	bool							AddBodyWithCustomID(Body *ioBody, const BodyID &inBodyID);

	/// Remove a list of bodies from the body manager
	void							RemoveBodies(const BodyID *inBodyIDs, int inNumber, Body **outBodies);

	/// Remove a set of bodies from the body manager and frees them.
	void							DestroyBodies(const BodyID *inBodyIDs, int inNumber);

	/// Activate a list of bodies.
	/// This function should only be called when an exclusive lock for the bodies are held.
	void							ActivateBodies(const BodyID *inBodyIDs, int inNumber);

	/// Deactivate a list of bodies.
	/// This function should only be called when an exclusive lock for the bodies are held.
	void							DeactivateBodies(const BodyID *inBodyIDs, int inNumber);

	/// Update the motion quality for a body
	void							SetMotionQuality(Body &ioBody, EMotionQuality inMotionQuality);

	/// Get copy of the list of active bodies under protection of a lock.
	void							GetActiveBodies(EBodyType inType, BodyIDVector &outBodyIDs) const;

	/// Get the list of active bodies. Note: Not thread safe. The active bodies list can change at any moment.
	const BodyID *					GetActiveBodiesUnsafe(EBodyType inType) const { return mActiveBodies[int(inType)]; }

	/// Get the number of active bodies.
	uint32							GetNumActiveBodies(EBodyType inType) const	{ return mNumActiveBodies[int(inType)].load(memory_order_acquire); }

	/// Get the number of active bodies that are using continuous collision detection
	uint32							GetNumActiveCCDBodies() const				{ return mNumActiveCCDBodies; }

	/// Listener that is notified whenever a body is activated/deactivated
	void							SetBodyActivationListener(BodyActivationListener *inListener);
	BodyActivationListener *		GetBodyActivationListener() const			{ return mActivationListener; }

	/// Check if this is a valid body pointer. When a body is freed the memory that the pointer occupies is reused to store a freelist.
	static inline bool				sIsValidBodyPointer(const Body *inBody)		{ return (uintptr_t(inBody) & cIsFreedBody) == 0; }

	/// Get all bodies. Note that this can contain invalid body pointers, call sIsValidBodyPointer to check.
	const BodyVector &				GetBodies() const							{ return mBodies; }

	/// Get all bodies. Note that this can contain invalid body pointers, call sIsValidBodyPointer to check.
	BodyVector &					GetBodies()									{ return mBodies; }

	/// Get all body IDs under the protection of a lock
	void							GetBodyIDs(BodyIDVector &outBodies) const;

	/// Access a body (not protected by lock)
	const Body &					GetBody(const BodyID &inID) const			{ return *mBodies[inID.GetIndex()]; }

	/// Access a body (not protected by lock)
	Body &							GetBody(const BodyID &inID)					{ return *mBodies[inID.GetIndex()]; }

	/// Access a body, will return a nullptr if the body ID is no longer valid (not protected by lock)
	const Body *					TryGetBody(const BodyID &inID) const
	{
		uint32 idx = inID.GetIndex();
		if (idx >= mBodies.size())
			return nullptr;

		const Body *body = mBodies[idx];
		if (sIsValidBodyPointer(body) && body->GetID() == inID)
			return body;

		return nullptr;
	}

	/// Access a body, will return a nullptr if the body ID is no longer valid (not protected by lock)
	Body *							TryGetBody(const BodyID &inID)
	{
		uint32 idx = inID.GetIndex();
		if (idx >= mBodies.size())
			return nullptr;

		Body *body = mBodies[idx];
		if (sIsValidBodyPointer(body) && body->GetID() == inID)
			return body;

		return nullptr;
	}

	/// Access the mutex for a single body
	SharedMutex &					GetMutexForBody(const BodyID &inID) const	{ return mBodyMutexes.GetMutexByObjectIndex(inID.GetIndex()); }

	/// Bodies are protected using an array of mutexes (so a fixed number, not 1 per body). Each bit in this mask indicates a locked mutex.
	using MutexMask = uint64;

	///@name Batch body mutex access (do not use directly)
	///@{
	MutexMask						GetAllBodiesMutexMask() const				{ return mBodyMutexes.GetNumMutexes() == sizeof(MutexMask) * 8? ~MutexMask(0) : (MutexMask(1) << mBodyMutexes.GetNumMutexes()) - 1; }
	MutexMask						GetMutexMask(const BodyID *inBodies, int inNumber) const;
	void							LockRead(MutexMask inMutexMask) const;
	void							UnlockRead(MutexMask inMutexMask) const;
	void							LockWrite(MutexMask inMutexMask) const;
	void							UnlockWrite(MutexMask inMutexMask) const;
	///@}

	/// Lock all bodies. This should only be done during PhysicsSystem::Update().
	void							LockAllBodies() const;

	/// Unlock all bodies. This should only be done during PhysicsSystem::Update().
	void							UnlockAllBodies() const;

	/// Function to update body's layer (should only be called by the BodyInterface since it also requires updating the broadphase)
	inline void						SetBodyObjectLayerInternal(Body &ioBody, ObjectLayer inLayer) const { ioBody.mObjectLayer = inLayer; ioBody.mBroadPhaseLayer = mBroadPhaseLayerInterface->GetBroadPhaseLayer(inLayer); }

	/// Set the Body::EFlags::InvalidateContactCache flag for the specified body. This means that the collision cache is invalid for any body pair involving that body until the next physics step.
	void							InvalidateContactCacheForBody(Body &ioBody);

	/// Reset the Body::EFlags::InvalidateContactCache flag for all bodies. All contact pairs in the contact cache will now by valid again.
	void							ValidateContactCacheForAllBodies();

	/// Saving state for replay
	void							SaveState(StateRecorder &inStream, const StateRecorderFilter *inFilter) const;

	/// Restoring state for replay. Returns false if failed.
	bool							RestoreState(StateRecorder &inStream);

	/// Save the state of a single body for replay
	void							SaveBodyState(const Body &inBody, StateRecorder &inStream) const;

	/// Save the state of a single body for replay
	void							RestoreBodyState(Body &inBody, StateRecorder &inStream);

#ifdef JPH_DEBUG_RENDERER
	enum class EShapeColor
	{
		InstanceColor,				///< Random color per instance
		ShapeTypeColor,				///< Convex = green, scaled = yellow, compound = orange, mesh = red
		MotionTypeColor,			///< Static = grey, keyframed = green, dynamic = random color per instance
		SleepColor,					///< Static = grey, keyframed = green, dynamic = yellow, sleeping = red
		IslandColor,				///< Static = grey, active = random color per island, sleeping = light grey
		MaterialColor,				///< Color as defined by the PhysicsMaterial of the shape
	};

	/// Draw settings
	struct DrawSettings
	{
		bool						mDrawGetSupportFunction = false;				///< Draw the GetSupport() function, used for convex collision detection
		bool						mDrawSupportDirection = false;					///< When drawing the support function, also draw which direction mapped to a specific support point
		bool						mDrawGetSupportingFace = false;					///< Draw the faces that were found colliding during collision detection
		bool						mDrawShape = true;								///< Draw the shapes of all bodies
		bool						mDrawShapeWireframe = false;					///< When mDrawShape is true and this is true, the shapes will be drawn in wireframe instead of solid.
		EShapeColor					mDrawShapeColor = EShapeColor::MotionTypeColor; ///< Coloring scheme to use for shapes
		bool						mDrawBoundingBox = false;						///< Draw a bounding box per body
		bool						mDrawCenterOfMassTransform = false;				///< Draw the center of mass for each body
		bool						mDrawWorldTransform = false;					///< Draw the world transform (which can be different than the center of mass) for each body
		bool						mDrawVelocity = false;							///< Draw the velocity vector for each body
		bool						mDrawMassAndInertia = false;					///< Draw the mass and inertia (as the box equivalent) for each body
		bool						mDrawSleepStats = false;						///< Draw stats regarding the sleeping algorithm of each body
		bool						mDrawSoftBodyVertices = false;					///< Draw the vertices of soft bodies
		bool						mDrawSoftBodyVertexVelocities = false;			///< Draw the velocities of the vertices of soft bodies
		bool						mDrawSoftBodyEdgeConstraints = false;			///< Draw the edge constraints of soft bodies
		bool						mDrawSoftBodyBendConstraints = false;			///< Draw the bend constraints of soft bodies
		bool						mDrawSoftBodyVolumeConstraints = false;			///< Draw the volume constraints of soft bodies
		bool						mDrawSoftBodySkinConstraints = false;			///< Draw the skin constraints of soft bodies
		bool						mDrawSoftBodyLRAConstraints = false;			///< Draw the LRA constraints of soft bodies
		bool						mDrawSoftBodyPredictedBounds = false;			///< Draw the predicted bounds of soft bodies
		ESoftBodyConstraintColor	mDrawSoftBodyConstraintColor = ESoftBodyConstraintColor::ConstraintType; ///< Coloring scheme to use for soft body constraints
	};

	/// Draw the state of the bodies (debugging purposes)
	void							Draw(const DrawSettings &inSettings, const PhysicsSettings &inPhysicsSettings, DebugRenderer *inRenderer, const BodyDrawFilter *inBodyFilter = nullptr);
#endif // JPH_DEBUG_RENDERER

#ifdef JPH_ENABLE_ASSERTS
	/// Lock the active body list, asserts when Activate/DeactivateBody is called.
	void							SetActiveBodiesLocked(bool inLocked)		{ mActiveBodiesLocked = inLocked; }

	/// Per thread override of the locked state, to be used by the PhysicsSystem only!
	class GrantActiveBodiesAccess
	{
	public:
		inline GrantActiveBodiesAccess(bool inAllowActivation, bool inAllowDeactivation)
		{
			JPH_ASSERT(!sGetOverrideAllowActivation());
			sSetOverrideAllowActivation(inAllowActivation);

			JPH_ASSERT(!sGetOverrideAllowDeactivation());
			sSetOverrideAllowDeactivation(inAllowDeactivation);
		}

		inline ~GrantActiveBodiesAccess()
		{
			sSetOverrideAllowActivation(false);
			sSetOverrideAllowDeactivation(false);
		}
	};
#endif

#ifdef JPH_DEBUG
	/// Validate if the cached bounding boxes are correct for all active bodies
	void							ValidateActiveBodyBounds();
#endif // JPH_DEBUG

private:
	/// Increment and get the sequence number of the body
#ifdef JPH_COMPILER_CLANG
	__attribute__((no_sanitize("implicit-conversion"))) // We intentionally overflow the uint8 sequence number
#endif
	inline uint8					GetNextSequenceNumber(int inBodyIndex)		{ return ++mBodySequenceNumbers[inBodyIndex]; }

	/// Add a single body to mActiveBodies, note doesn't lock the active body mutex!
	inline void						AddBodyToActiveBodies(Body &ioBody);

	/// Remove a single body from mActiveBodies, note doesn't lock the active body mutex!
	inline void						RemoveBodyFromActiveBodies(Body &ioBody);

	/// Helper function to remove a body from the manager
	JPH_INLINE Body *				RemoveBodyInternal(const BodyID &inBodyID);

	/// Helper function to delete a body (which could actually be a BodyWithMotionProperties)
	inline static void				sDeleteBody(Body *inBody);

#if defined(JPH_DEBUG) && defined(JPH_ENABLE_ASSERTS)
	/// Function to check that the free list is not corrupted
	void							ValidateFreeList() const;
#endif // defined(JPH_DEBUG) && _defined(JPH_ENABLE_ASSERTS)

	/// List of pointers to all bodies. Contains invalid pointers for deleted bodies, check with sIsValidBodyPointer. Note that this array is reserved to the max bodies that is passed in the Init function so that adding bodies will not reallocate the array.
	BodyVector						mBodies;

	/// Current number of allocated bodies
	uint							mNumBodies = 0;

	/// Indicates that there are no more freed body IDs
	static constexpr uintptr_t		cBodyIDFreeListEnd = ~uintptr_t(0);

	/// Bit that indicates a pointer in mBodies is actually the index of the next freed body. We use the lowest bit because we know that Bodies need to be 16 byte aligned so addresses can never end in a 1 bit.
	static constexpr uintptr_t		cIsFreedBody = uintptr_t(1);

	/// Amount of bits to shift to get an index to the next freed body
	static constexpr uint			cFreedBodyIndexShift = 1;

	/// Index of first entry in mBodies that is unused
	uintptr_t						mBodyIDFreeListStart = cBodyIDFreeListEnd;

	/// Protects mBodies array (but not the bodies it points to), mNumBodies and mBodyIDFreeListStart
	mutable Mutex					mBodiesMutex;

	/// An array of mutexes protecting the bodies in the mBodies array
	using BodyMutexes = MutexArray<SharedMutex>;
	mutable BodyMutexes				mBodyMutexes;

	/// List of next sequence number for a body ID
	Array<uint8>					mBodySequenceNumbers;

	/// Mutex that protects the mActiveBodies array
	mutable Mutex					mActiveBodiesMutex;

	/// List of all active dynamic bodies (size is equal to max amount of bodies)
	BodyID *						mActiveBodies[cBodyTypeCount] = { };

	/// How many bodies there are in the list of active bodies
	atomic<uint32>					mNumActiveBodies[cBodyTypeCount] = { };

	/// How many of the active bodies have continuous collision detection enabled
	uint32							mNumActiveCCDBodies = 0;

	/// Mutex that protects the mBodiesCacheInvalid array
	mutable Mutex					mBodiesCacheInvalidMutex;

	/// List of all bodies that should have their cache invalidated
	BodyIDVector					mBodiesCacheInvalid;

	/// Listener that is notified whenever a body is activated/deactivated
	BodyActivationListener *		mActivationListener = nullptr;

	/// Cached broadphase layer interface
	const BroadPhaseLayerInterface *mBroadPhaseLayerInterface = nullptr;

#ifdef JPH_ENABLE_ASSERTS
	static bool						sGetOverrideAllowActivation();
	static void						sSetOverrideAllowActivation(bool inValue);

	static bool						sGetOverrideAllowDeactivation();
	static void						sSetOverrideAllowDeactivation(bool inValue);

	/// Debug system that tries to limit changes to active bodies during the PhysicsSystem::Update()
	bool							mActiveBodiesLocked = false;
#endif
};

JPH_NAMESPACE_END
