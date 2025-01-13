// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsUpdateContext.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseBruteForce.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuadTree.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/AABoxCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/ManifoldBetweenTwoFaces.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/SimShapeFilterWrapper.h>
#include <Jolt/Physics/Collision/InternalEdgeRemovingCollector.h>
#include <Jolt/Physics/Constraints/CalculateSolverSteps.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>
#include <Jolt/Physics/DeterminismLog.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/QuickSort.h>
#include <Jolt/Core/ScopeExit.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

#ifdef JPH_DEBUG_RENDERER
bool PhysicsSystem::sDrawMotionQualityLinearCast = false;
#endif // JPH_DEBUG_RENDERER

//#define BROAD_PHASE BroadPhaseBruteForce
#define BROAD_PHASE BroadPhaseQuadTree

static const Color cColorUpdateBroadPhaseFinalize = Color::sGetDistinctColor(1);
static const Color cColorUpdateBroadPhasePrepare = Color::sGetDistinctColor(2);
static const Color cColorFindCollisions = Color::sGetDistinctColor(3);
static const Color cColorApplyGravity = Color::sGetDistinctColor(4);
static const Color cColorSetupVelocityConstraints = Color::sGetDistinctColor(5);
static const Color cColorBuildIslandsFromConstraints = Color::sGetDistinctColor(6);
static const Color cColorDetermineActiveConstraints = Color::sGetDistinctColor(7);
static const Color cColorFinalizeIslands = Color::sGetDistinctColor(8);
static const Color cColorContactRemovedCallbacks = Color::sGetDistinctColor(9);
static const Color cColorBodySetIslandIndex = Color::sGetDistinctColor(10);
static const Color cColorStartNextStep = Color::sGetDistinctColor(11);
static const Color cColorSolveVelocityConstraints = Color::sGetDistinctColor(12);
static const Color cColorPreIntegrateVelocity = Color::sGetDistinctColor(13);
static const Color cColorIntegrateVelocity = Color::sGetDistinctColor(14);
static const Color cColorPostIntegrateVelocity = Color::sGetDistinctColor(15);
static const Color cColorResolveCCDContacts = Color::sGetDistinctColor(16);
static const Color cColorSolvePositionConstraints = Color::sGetDistinctColor(17);
static const Color cColorFindCCDContacts = Color::sGetDistinctColor(18);
static const Color cColorStepListeners = Color::sGetDistinctColor(19);
static const Color cColorSoftBodyPrepare = Color::sGetDistinctColor(20);
static const Color cColorSoftBodyCollide = Color::sGetDistinctColor(21);
static const Color cColorSoftBodySimulate = Color::sGetDistinctColor(22);
static const Color cColorSoftBodyFinalize = Color::sGetDistinctColor(23);

PhysicsSystem::~PhysicsSystem()
{
	// Remove broadphase
	delete mBroadPhase;
}

void PhysicsSystem::Init(uint inMaxBodies, uint inNumBodyMutexes, uint inMaxBodyPairs, uint inMaxContactConstraints, const BroadPhaseLayerInterface &inBroadPhaseLayerInterface, const ObjectVsBroadPhaseLayerFilter &inObjectVsBroadPhaseLayerFilter, const ObjectLayerPairFilter &inObjectLayerPairFilter)
{
	JPH_ASSERT(inMaxBodies <= BodyID::cMaxBodyIndex, "Cannot support this many bodies");

	mObjectVsBroadPhaseLayerFilter = &inObjectVsBroadPhaseLayerFilter;
	mObjectLayerPairFilter = &inObjectLayerPairFilter;

	// Initialize body manager
	mBodyManager.Init(inMaxBodies, inNumBodyMutexes, inBroadPhaseLayerInterface);

	// Create broadphase
	mBroadPhase = new BROAD_PHASE();
	mBroadPhase->Init(&mBodyManager, inBroadPhaseLayerInterface);

	// Init contact constraint manager
	mContactManager.Init(inMaxBodyPairs, inMaxContactConstraints);

	// Init islands builder
	mIslandBuilder.Init(inMaxBodies);

	// Initialize body interface
	mBodyInterfaceLocking.Init(mBodyLockInterfaceLocking, mBodyManager, *mBroadPhase);
	mBodyInterfaceNoLock.Init(mBodyLockInterfaceNoLock, mBodyManager, *mBroadPhase);

	// Initialize narrow phase query
	mNarrowPhaseQueryLocking.Init(mBodyLockInterfaceLocking, *mBroadPhase);
	mNarrowPhaseQueryNoLock.Init(mBodyLockInterfaceNoLock, *mBroadPhase);
}

void PhysicsSystem::OptimizeBroadPhase()
{
	mBroadPhase->Optimize();
}

void PhysicsSystem::AddStepListener(PhysicsStepListener *inListener)
{
	lock_guard lock(mStepListenersMutex);

	JPH_ASSERT(std::find(mStepListeners.begin(), mStepListeners.end(), inListener) == mStepListeners.end());
	mStepListeners.push_back(inListener);
}

void PhysicsSystem::RemoveStepListener(PhysicsStepListener *inListener)
{
	lock_guard lock(mStepListenersMutex);

	StepListeners::iterator i = std::find(mStepListeners.begin(), mStepListeners.end(), inListener);
	JPH_ASSERT(i != mStepListeners.end());
	*i = mStepListeners.back();
	mStepListeners.pop_back();
}

EPhysicsUpdateError PhysicsSystem::Update(float inDeltaTime, int inCollisionSteps, TempAllocator *inTempAllocator, JobSystem *inJobSystem)
{
	JPH_PROFILE_FUNCTION();

	JPH_DET_LOG("PhysicsSystem::Update: dt: " << inDeltaTime << " steps: " << inCollisionSteps);

	JPH_ASSERT(inCollisionSteps > 0);
	JPH_ASSERT(inDeltaTime >= 0.0f);

	// Sync point for the broadphase. This will allow it to do clean up operations without having any mutexes locked yet.
	mBroadPhase->FrameSync();

	// If there are no active bodies or there's no time delta
	uint32 num_active_rigid_bodies = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);
	uint32 num_active_soft_bodies = mBodyManager.GetNumActiveBodies(EBodyType::SoftBody);
	if ((num_active_rigid_bodies == 0 && num_active_soft_bodies == 0) || inDeltaTime <= 0.0f)
	{
		mBodyManager.LockAllBodies();

		// Update broadphase
		mBroadPhase->LockModifications();
		BroadPhase::UpdateState update_state = mBroadPhase->UpdatePrepare();
		mBroadPhase->UpdateFinalize(update_state);
		mBroadPhase->UnlockModifications();

		// Call contact removal callbacks from contacts that existed in the previous update
		mContactManager.FinalizeContactCacheAndCallContactPointRemovedCallbacks(0, 0);

		mBodyManager.UnlockAllBodies();
		return EPhysicsUpdateError::None;
	}

	// Calculate ratio between current and previous frame delta time to scale initial constraint forces
	float step_delta_time = inDeltaTime / inCollisionSteps;
	float warm_start_impulse_ratio = mPhysicsSettings.mConstraintWarmStart && mPreviousStepDeltaTime > 0.0f? step_delta_time / mPreviousStepDeltaTime : 0.0f;
	mPreviousStepDeltaTime = step_delta_time;

	// Create the context used for passing information between jobs
	PhysicsUpdateContext context(*inTempAllocator);
	context.mPhysicsSystem = this;
	context.mJobSystem = inJobSystem;
	context.mBarrier = inJobSystem->CreateBarrier();
	context.mIslandBuilder = &mIslandBuilder;
	context.mStepDeltaTime = step_delta_time;
	context.mWarmStartImpulseRatio = warm_start_impulse_ratio;
	context.mSteps.resize(inCollisionSteps);

	// Allocate space for body pairs
	JPH_ASSERT(context.mBodyPairs == nullptr);
	context.mBodyPairs = static_cast<BodyPair *>(inTempAllocator->Allocate(sizeof(BodyPair) * mPhysicsSettings.mMaxInFlightBodyPairs));

	// Lock all bodies for write so that we can freely touch them
	mStepListenersMutex.lock();
	mBodyManager.LockAllBodies();
	mBroadPhase->LockModifications();

	// Get max number of concurrent jobs
	int max_concurrency = context.GetMaxConcurrency();

	// Calculate how many step listener jobs we spawn
	int num_step_listener_jobs = mStepListeners.empty()? 0 : max(1, min((int)mStepListeners.size() / mPhysicsSettings.mStepListenersBatchSize / mPhysicsSettings.mStepListenerBatchesPerJob, max_concurrency));

	// Number of gravity jobs depends on the amount of active bodies.
	// Launch max 1 job per batch of active bodies
	// Leave 1 thread for update broadphase prepare and 1 for determine active constraints
	int num_apply_gravity_jobs = max(1, min(((int)num_active_rigid_bodies + cApplyGravityBatchSize - 1) / cApplyGravityBatchSize, max_concurrency - 2));

	// Number of determine active constraints jobs to run depends on number of constraints.
	// Leave 1 thread for update broadphase prepare and 1 for apply gravity
	int num_determine_active_constraints_jobs = max(1, min(((int)mConstraintManager.GetNumConstraints() + cDetermineActiveConstraintsBatchSize - 1) / cDetermineActiveConstraintsBatchSize, max_concurrency - 2));

	// Number of setup velocity constraints jobs to run depends on number of constraints.
	int num_setup_velocity_constraints_jobs = max(1, min(((int)mConstraintManager.GetNumConstraints() + cSetupVelocityConstraintsBatchSize - 1) / cSetupVelocityConstraintsBatchSize, max_concurrency));

	// Number of find collisions jobs to run depends on number of active bodies.
	// Note that when we have more than 1 thread, we always spawn at least 2 find collisions jobs so that the first job can wait for build islands from constraints
	// (which may activate additional bodies that need to be processed) while the second job can start processing collision work.
	int num_find_collisions_jobs = max(max_concurrency == 1? 1 : 2, min(((int)num_active_rigid_bodies + cActiveBodiesBatchSize - 1) / cActiveBodiesBatchSize, max_concurrency));

	// Number of integrate velocity jobs depends on number of active bodies.
	int num_integrate_velocity_jobs = max(1, min(((int)num_active_rigid_bodies + cIntegrateVelocityBatchSize - 1) / cIntegrateVelocityBatchSize, max_concurrency));

	{
		JPH_PROFILE("Build Jobs");

		// Iterate over collision steps
		for (int step_idx = 0; step_idx < inCollisionSteps; ++step_idx)
		{
			bool is_first_step = step_idx == 0;
			bool is_last_step = step_idx == inCollisionSteps - 1;

			PhysicsUpdateContext::Step &step = context.mSteps[step_idx];
			step.mContext = &context;
			step.mIsFirst = is_first_step;
			step.mIsLast = is_last_step;

			// Create job to do broadphase finalization
			// This job must finish before integrating velocities. Until then the positions will not be updated neither will bodies be added / removed.
			step.mUpdateBroadphaseFinalize = inJobSystem->CreateJob("UpdateBroadPhaseFinalize", cColorUpdateBroadPhaseFinalize, [&context, &step]()
				{
					// Validate that all find collision jobs have stopped
					JPH_ASSERT(step.mActiveFindCollisionJobs.load(memory_order_relaxed) == 0);

					// Finalize the broadphase update
					context.mPhysicsSystem->mBroadPhase->UpdateFinalize(step.mBroadPhaseUpdateState);

					// Signal that it is done
					step.mPreIntegrateVelocity.RemoveDependency();
				}, num_find_collisions_jobs + 2); // depends on: find collisions, broadphase prepare update, finish building jobs

			// The immediate jobs below are only immediate for the first step, the all finished job will kick them for the next step
			int previous_step_dependency_count = is_first_step? 0 : 1;

			// Start job immediately: Start the prepare broadphase
			// Must be done under body lock protection since the order is body locks then broadphase mutex
			// If this is turned around the RemoveBody call will hang since it locks in that order
			step.mBroadPhasePrepare = inJobSystem->CreateJob("UpdateBroadPhasePrepare", cColorUpdateBroadPhasePrepare, [&context, &step]()
				{
					// Prepare the broadphase update
					step.mBroadPhaseUpdateState = context.mPhysicsSystem->mBroadPhase->UpdatePrepare();

					// Now the finalize can run (if other dependencies are met too)
					step.mUpdateBroadphaseFinalize.RemoveDependency();
				}, previous_step_dependency_count);

			// This job will find all collisions
			step.mBodyPairQueues.resize(max_concurrency);
			step.mMaxBodyPairsPerQueue = mPhysicsSettings.mMaxInFlightBodyPairs / max_concurrency;
			step.mActiveFindCollisionJobs.store(~PhysicsUpdateContext::JobMask(0) >> (sizeof(PhysicsUpdateContext::JobMask) * 8 - num_find_collisions_jobs), memory_order_release);
			step.mFindCollisions.resize(num_find_collisions_jobs);
			for (int i = 0; i < num_find_collisions_jobs; ++i)
			{
				// Build islands from constraints may activate additional bodies, so the first job will wait for this to finish in order to not miss any active bodies
				int num_dep_build_islands_from_constraints = i == 0? 1 : 0;
				step.mFindCollisions[i] = inJobSystem->CreateJob("FindCollisions", cColorFindCollisions, [&step, i]()
					{
						step.mContext->mPhysicsSystem->JobFindCollisions(&step, i);
					}, num_apply_gravity_jobs + num_determine_active_constraints_jobs + 1 + num_dep_build_islands_from_constraints); // depends on: apply gravity, determine active constraints, finish building jobs, build islands from constraints
			}

			if (is_first_step)
			{
			#ifdef JPH_ENABLE_ASSERTS
				// Don't allow write operations to the active bodies list
				mBodyManager.SetActiveBodiesLocked(true);
			#endif

				// Store the number of active bodies at the start of the step
				step.mNumActiveBodiesAtStepStart = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);

				// Lock all constraints
				mConstraintManager.LockAllConstraints();

				// Allocate memory for storing the active constraints
				JPH_ASSERT(context.mActiveConstraints == nullptr);
				context.mActiveConstraints = static_cast<Constraint **>(inTempAllocator->Allocate(mConstraintManager.GetNumConstraints() * sizeof(Constraint *)));

				// Prepare contact buffer
				mContactManager.PrepareConstraintBuffer(&context);

				// Setup island builder
				mIslandBuilder.PrepareContactConstraints(mContactManager.GetMaxConstraints(), context.mTempAllocator);
			}

			// This job applies gravity to all active bodies
			step.mApplyGravity.resize(num_apply_gravity_jobs);
			for (int i = 0; i < num_apply_gravity_jobs; ++i)
				step.mApplyGravity[i] = inJobSystem->CreateJob("ApplyGravity", cColorApplyGravity, [&context, &step]()
					{
						context.mPhysicsSystem->JobApplyGravity(&context, &step);

						JobHandle::sRemoveDependencies(step.mFindCollisions);
					}, num_step_listener_jobs > 0? num_step_listener_jobs : previous_step_dependency_count); // depends on: step listeners (or previous step if no step listeners)

			// This job will setup velocity constraints for non-collision constraints
			step.mSetupVelocityConstraints.resize(num_setup_velocity_constraints_jobs);
			for (int i = 0; i < num_setup_velocity_constraints_jobs; ++i)
				step.mSetupVelocityConstraints[i] = inJobSystem->CreateJob("SetupVelocityConstraints", cColorSetupVelocityConstraints, [&context, &step]()
					{
						context.mPhysicsSystem->JobSetupVelocityConstraints(context.mStepDeltaTime, &step);

						JobHandle::sRemoveDependencies(step.mSolveVelocityConstraints);
					}, num_determine_active_constraints_jobs + 1); // depends on: determine active constraints, finish building jobs

			// This job will build islands from constraints
			step.mBuildIslandsFromConstraints = inJobSystem->CreateJob("BuildIslandsFromConstraints", cColorBuildIslandsFromConstraints, [&context, &step]()
				{
					context.mPhysicsSystem->JobBuildIslandsFromConstraints(&context, &step);

					step.mFindCollisions[0].RemoveDependency(); // The first collisions job cannot start running until we've finished building islands and activated all bodies
					step.mFinalizeIslands.RemoveDependency();
				}, num_determine_active_constraints_jobs + 1); // depends on: determine active constraints, finish building jobs

			// This job determines active constraints
			step.mDetermineActiveConstraints.resize(num_determine_active_constraints_jobs);
			for (int i = 0; i < num_determine_active_constraints_jobs; ++i)
				step.mDetermineActiveConstraints[i] = inJobSystem->CreateJob("DetermineActiveConstraints", cColorDetermineActiveConstraints, [&context, &step]()
					{
						context.mPhysicsSystem->JobDetermineActiveConstraints(&step);

						step.mBuildIslandsFromConstraints.RemoveDependency();

						// Kick these jobs last as they will use up all CPU cores leaving no space for the previous job, we prefer setup velocity constraints to finish first so we kick it first
						JobHandle::sRemoveDependencies(step.mSetupVelocityConstraints);
						JobHandle::sRemoveDependencies(step.mFindCollisions);
					}, num_step_listener_jobs > 0? num_step_listener_jobs : previous_step_dependency_count); // depends on: step listeners (or previous step if no step listeners)

			// This job calls the step listeners
			step.mStepListeners.resize(num_step_listener_jobs);
			for (int i = 0; i < num_step_listener_jobs; ++i)
				step.mStepListeners[i] = inJobSystem->CreateJob("StepListeners", cColorStepListeners, [&context, &step]()
					{
						// Call the step listeners
						context.mPhysicsSystem->JobStepListeners(&step);

						// Kick apply gravity and determine active constraint jobs
						JobHandle::sRemoveDependencies(step.mApplyGravity);
						JobHandle::sRemoveDependencies(step.mDetermineActiveConstraints);
					}, previous_step_dependency_count);

			// Unblock the previous step
			if (!is_first_step)
				context.mSteps[step_idx - 1].mStartNextStep.RemoveDependency();

			// This job will finalize the simulation islands
			step.mFinalizeIslands = inJobSystem->CreateJob("FinalizeIslands", cColorFinalizeIslands, [&context, &step]()
				{
					// Validate that all find collision jobs have stopped
					JPH_ASSERT(step.mActiveFindCollisionJobs.load(memory_order_relaxed) == 0);

					context.mPhysicsSystem->JobFinalizeIslands(&context);

					JobHandle::sRemoveDependencies(step.mSolveVelocityConstraints);
					step.mBodySetIslandIndex.RemoveDependency();
				}, num_find_collisions_jobs + 2); // depends on: find collisions, build islands from constraints, finish building jobs

			// Unblock previous job
			// Note: technically we could release find collisions here but we don't want to because that could make them run before 'setup velocity constraints' which means that job won't have a thread left
			step.mBuildIslandsFromConstraints.RemoveDependency();

			// This job will call the contact removed callbacks
			step.mContactRemovedCallbacks = inJobSystem->CreateJob("ContactRemovedCallbacks", cColorContactRemovedCallbacks, [&context, &step]()
				{
					context.mPhysicsSystem->JobContactRemovedCallbacks(&step);

					if (step.mStartNextStep.IsValid())
						step.mStartNextStep.RemoveDependency();
				}, 1); // depends on the find ccd contacts

			// This job will set the island index on each body (only used for debug drawing purposes)
			// It will also delete any bodies that have been destroyed in the last frame
			step.mBodySetIslandIndex = inJobSystem->CreateJob("BodySetIslandIndex", cColorBodySetIslandIndex, [&context, &step]()
				{
					context.mPhysicsSystem->JobBodySetIslandIndex();

					JobHandle::sRemoveDependencies(step.mSolvePositionConstraints);
				}, 2); // depends on: finalize islands, finish building jobs

			// Job to start the next collision step
			if (!is_last_step)
			{
				PhysicsUpdateContext::Step *next_step = &context.mSteps[step_idx + 1];
				step.mStartNextStep = inJobSystem->CreateJob("StartNextStep", cColorStartNextStep, [this, next_step]()
					{
					#ifdef JPH_DEBUG
						// Validate that the cached bounds are correct
						mBodyManager.ValidateActiveBodyBounds();
					#endif // JPH_DEBUG

						// Store the number of active bodies at the start of the step
						next_step->mNumActiveBodiesAtStepStart = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);

						// Clear the large island splitter
						TempAllocator *temp_allocator = next_step->mContext->mTempAllocator;
						mLargeIslandSplitter.Reset(temp_allocator);

						// Clear the island builder
						mIslandBuilder.ResetIslands(temp_allocator);

						// Setup island builder
						mIslandBuilder.PrepareContactConstraints(mContactManager.GetMaxConstraints(), temp_allocator);

						// Restart the contact manager
						mContactManager.RecycleConstraintBuffer();

						// Kick the jobs of the next step (in the same order as the first step)
						next_step->mBroadPhasePrepare.RemoveDependency();
						if (next_step->mStepListeners.empty())
						{
							// Kick the gravity and active constraints jobs immediately
							JobHandle::sRemoveDependencies(next_step->mApplyGravity);
							JobHandle::sRemoveDependencies(next_step->mDetermineActiveConstraints);
						}
						else
						{
							// Kick the step listeners job first
							JobHandle::sRemoveDependencies(next_step->mStepListeners);
						}
					}, 3); // depends on: update soft bodies, contact removed callbacks, finish building the previous step
			}

			// This job will solve the velocity constraints
			step.mSolveVelocityConstraints.resize(max_concurrency);
			for (int i = 0; i < max_concurrency; ++i)
				step.mSolveVelocityConstraints[i] = inJobSystem->CreateJob("SolveVelocityConstraints", cColorSolveVelocityConstraints, [&context, &step]()
					{
						context.mPhysicsSystem->JobSolveVelocityConstraints(&context, &step);

						step.mPreIntegrateVelocity.RemoveDependency();
					}, num_setup_velocity_constraints_jobs + 2); // depends on: finalize islands, setup velocity constraints, finish building jobs.

			// We prefer setup velocity constraints to finish first so we kick it first
			JobHandle::sRemoveDependencies(step.mSetupVelocityConstraints);
			JobHandle::sRemoveDependencies(step.mFindCollisions);

			// Finalize islands is a dependency on find collisions so it can go last
			step.mFinalizeIslands.RemoveDependency();

			// This job will prepare the position update of all active bodies
			step.mPreIntegrateVelocity = inJobSystem->CreateJob("PreIntegrateVelocity", cColorPreIntegrateVelocity, [&context, &step]()
				{
					context.mPhysicsSystem->JobPreIntegrateVelocity(&context, &step);

					JobHandle::sRemoveDependencies(step.mIntegrateVelocity);
				}, 2 + max_concurrency); // depends on: broadphase update finalize, solve velocity constraints, finish building jobs.

			// Unblock previous jobs
			step.mUpdateBroadphaseFinalize.RemoveDependency();
			JobHandle::sRemoveDependencies(step.mSolveVelocityConstraints);

			// This job will update the positions of all active bodies
			step.mIntegrateVelocity.resize(num_integrate_velocity_jobs);
			for (int i = 0; i < num_integrate_velocity_jobs; ++i)
				step.mIntegrateVelocity[i] = inJobSystem->CreateJob("IntegrateVelocity", cColorIntegrateVelocity, [&context, &step]()
					{
						context.mPhysicsSystem->JobIntegrateVelocity(&context, &step);

						step.mPostIntegrateVelocity.RemoveDependency();
					}, 2); // depends on: pre integrate velocity, finish building jobs.

			// Unblock previous job
			step.mPreIntegrateVelocity.RemoveDependency();

			// This job will finish the position update of all active bodies
			step.mPostIntegrateVelocity = inJobSystem->CreateJob("PostIntegrateVelocity", cColorPostIntegrateVelocity, [&context, &step]()
				{
					context.mPhysicsSystem->JobPostIntegrateVelocity(&context, &step);

					step.mResolveCCDContacts.RemoveDependency();
				}, num_integrate_velocity_jobs + 1); // depends on: integrate velocity, finish building jobs

			// Unblock previous jobs
			JobHandle::sRemoveDependencies(step.mIntegrateVelocity);

			// This job will update the positions and velocities for all bodies that need continuous collision detection
			step.mResolveCCDContacts = inJobSystem->CreateJob("ResolveCCDContacts", cColorResolveCCDContacts, [&context, &step]()
				{
					context.mPhysicsSystem->JobResolveCCDContacts(&context, &step);

					JobHandle::sRemoveDependencies(step.mSolvePositionConstraints);
				}, 2); // depends on: integrate velocities, detect ccd contacts (added dynamically), finish building jobs.

			// Unblock previous job
			step.mPostIntegrateVelocity.RemoveDependency();

			// Fixes up drift in positions and updates the broadphase with new body positions
			step.mSolvePositionConstraints.resize(max_concurrency);
			for (int i = 0; i < max_concurrency; ++i)
				step.mSolvePositionConstraints[i] = inJobSystem->CreateJob("SolvePositionConstraints", cColorSolvePositionConstraints, [&context, &step]()
					{
						context.mPhysicsSystem->JobSolvePositionConstraints(&context, &step);

						// Kick the next step
						if (step.mSoftBodyPrepare.IsValid())
							step.mSoftBodyPrepare.RemoveDependency();
					}, 3); // depends on: resolve ccd contacts, body set island index, finish building jobs.

			// Unblock previous jobs.
			step.mResolveCCDContacts.RemoveDependency();
			step.mBodySetIslandIndex.RemoveDependency();

			// The soft body prepare job will create other jobs if needed
			step.mSoftBodyPrepare = inJobSystem->CreateJob("SoftBodyPrepare", cColorSoftBodyPrepare, [&context, &step]()
				{
					context.mPhysicsSystem->JobSoftBodyPrepare(&context, &step);
				}, max_concurrency); // depends on: solve position constraints.

			// Unblock previous jobs
			JobHandle::sRemoveDependencies(step.mSolvePositionConstraints);
		}
	}

	// Build the list of jobs to wait for
	JobSystem::Barrier *barrier = context.mBarrier;
	{
		JPH_PROFILE("Build job barrier");

		StaticArray<JobHandle, cMaxPhysicsJobs> handles;
		for (const PhysicsUpdateContext::Step &step : context.mSteps)
		{
			if (step.mBroadPhasePrepare.IsValid())
				handles.push_back(step.mBroadPhasePrepare);
			for (const JobHandle &h : step.mStepListeners)
				handles.push_back(h);
			for (const JobHandle &h : step.mDetermineActiveConstraints)
				handles.push_back(h);
			for (const JobHandle &h : step.mApplyGravity)
				handles.push_back(h);
			for (const JobHandle &h : step.mFindCollisions)
				handles.push_back(h);
			if (step.mUpdateBroadphaseFinalize.IsValid())
				handles.push_back(step.mUpdateBroadphaseFinalize);
			for (const JobHandle &h : step.mSetupVelocityConstraints)
				handles.push_back(h);
			handles.push_back(step.mBuildIslandsFromConstraints);
			handles.push_back(step.mFinalizeIslands);
			handles.push_back(step.mBodySetIslandIndex);
			for (const JobHandle &h : step.mSolveVelocityConstraints)
				handles.push_back(h);
			handles.push_back(step.mPreIntegrateVelocity);
			for (const JobHandle &h : step.mIntegrateVelocity)
				handles.push_back(h);
			handles.push_back(step.mPostIntegrateVelocity);
			handles.push_back(step.mResolveCCDContacts);
			for (const JobHandle &h : step.mSolvePositionConstraints)
				handles.push_back(h);
			handles.push_back(step.mContactRemovedCallbacks);
			if (step.mSoftBodyPrepare.IsValid())
				handles.push_back(step.mSoftBodyPrepare);
			if (step.mStartNextStep.IsValid())
				handles.push_back(step.mStartNextStep);
		}
		barrier->AddJobs(handles.data(), handles.size());
	}

	// Wait until all jobs finish
	// Note we don't just wait for the last job. If we would and another job
	// would be scheduled in between there is the possibility of a deadlock.
	// The other job could try to e.g. add/remove a body which would try to
	// lock a body mutex while this thread has already locked the mutex
	inJobSystem->WaitForJobs(barrier);

	// We're done with the barrier for this update
	inJobSystem->DestroyBarrier(barrier);

#ifdef JPH_DEBUG
	// Validate that the cached bounds are correct
	mBodyManager.ValidateActiveBodyBounds();
#endif // JPH_DEBUG

	// Clear the large island splitter
	mLargeIslandSplitter.Reset(inTempAllocator);

	// Clear the island builder
	mIslandBuilder.ResetIslands(inTempAllocator);

	// Clear the contact manager
	mContactManager.FinishConstraintBuffer();

	// Free active constraints
	inTempAllocator->Free(context.mActiveConstraints, mConstraintManager.GetNumConstraints() * sizeof(Constraint *));
	context.mActiveConstraints = nullptr;

	// Free body pairs
	inTempAllocator->Free(context.mBodyPairs, sizeof(BodyPair) * mPhysicsSettings.mMaxInFlightBodyPairs);
	context.mBodyPairs = nullptr;

	// Unlock the broadphase
	mBroadPhase->UnlockModifications();

	// Unlock all constraints
	mConstraintManager.UnlockAllConstraints();

#ifdef JPH_ENABLE_ASSERTS
	// Allow write operations to the active bodies list
	mBodyManager.SetActiveBodiesLocked(false);
#endif

	// Unlock all bodies
	mBodyManager.UnlockAllBodies();

	// Unlock step listeners
	mStepListenersMutex.unlock();

	// Return any errors
	EPhysicsUpdateError errors = static_cast<EPhysicsUpdateError>(context.mErrors.load(memory_order_acquire));
	JPH_ASSERT(errors == EPhysicsUpdateError::None, "An error occurred during the physics update, see EPhysicsUpdateError for more information");
	return errors;
}

void PhysicsSystem::JobStepListeners(PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// Read positions (broadphase updates concurrently so we can't write), read/write velocities
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::Read);

	// Can activate bodies only (we cache the amount of active bodies at the beginning of the step in mNumActiveBodiesAtStepStart so we cannot deactivate here)
	BodyManager::GrantActiveBodiesAccess grant_active(true, false);
#endif

	PhysicsStepListenerContext context;
	context.mDeltaTime = ioStep->mContext->mStepDeltaTime;
	context.mIsFirstStep = ioStep->mIsFirst;
	context.mIsLastStep = ioStep->mIsLast;
	context.mPhysicsSystem = this;

	uint32 batch_size = mPhysicsSettings.mStepListenersBatchSize;
	for (;;)
	{
		// Get the start of a new batch
		uint32 batch = ioStep->mStepListenerReadIdx.fetch_add(batch_size);
		if (batch >= mStepListeners.size())
			break;

		// Call the listeners
		for (uint32 i = batch, i_end = min((uint32)mStepListeners.size(), batch + batch_size); i < i_end; ++i)
			mStepListeners[i]->OnStep(context);
	}
}

void PhysicsSystem::JobDetermineActiveConstraints(PhysicsUpdateContext::Step *ioStep) const
{
#ifdef JPH_ENABLE_ASSERTS
	// No body access
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::None);
#endif

	uint32 num_constraints = mConstraintManager.GetNumConstraints();
	uint32 num_active_constraints;
	Constraint **active_constraints = (Constraint **)JPH_STACK_ALLOC(cDetermineActiveConstraintsBatchSize * sizeof(Constraint *));

	for (;;)
	{
		// Atomically fetch a batch of constraints
		uint32 constraint_idx = ioStep->mDetermineActiveConstraintReadIdx.fetch_add(cDetermineActiveConstraintsBatchSize);
		if (constraint_idx >= num_constraints)
			break;

		// Calculate the end of the batch
		uint32 constraint_idx_end = min(num_constraints, constraint_idx + cDetermineActiveConstraintsBatchSize);

		// Store the active constraints at the start of the step (bodies get activated during the step which in turn may activate constraints leading to an inconsistent shapshot)
		mConstraintManager.GetActiveConstraints(constraint_idx, constraint_idx_end, active_constraints, num_active_constraints);

		// Copy the block of active constraints to the global list of active constraints
		if (num_active_constraints > 0)
		{
			uint32 active_constraint_idx = ioStep->mNumActiveConstraints.fetch_add(num_active_constraints);
			memcpy(ioStep->mContext->mActiveConstraints + active_constraint_idx, active_constraints, num_active_constraints * sizeof(Constraint *));
		}
	}
}

void PhysicsSystem::JobApplyGravity(const PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We update velocities and need the rotation to do so
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::Read);
#endif

	// Get list of active bodies that we had at the start of the physics update.
	// Any body that is activated as part of the simulation step does not receive gravity this frame.
	// Note that bodies may be activated during this job but not deactivated, this means that only elements
	// will be added to the array. Since the array is made to not reallocate, this is a safe operation.
	const BodyID *active_bodies = mBodyManager.GetActiveBodiesUnsafe(EBodyType::RigidBody);
	uint32 num_active_bodies_at_step_start = ioStep->mNumActiveBodiesAtStepStart;

	// Fetch delta time once outside the loop
	float delta_time = ioContext->mStepDeltaTime;

	// Update velocities from forces
	for (;;)
	{
		// Atomically fetch a batch of bodies
		uint32 active_body_idx = ioStep->mApplyGravityReadIdx.fetch_add(cApplyGravityBatchSize);
		if (active_body_idx >= num_active_bodies_at_step_start)
			break;

		// Calculate the end of the batch
		uint32 active_body_idx_end = min(num_active_bodies_at_step_start, active_body_idx + cApplyGravityBatchSize);

		// Process the batch
		while (active_body_idx < active_body_idx_end)
		{
			Body &body = mBodyManager.GetBody(active_bodies[active_body_idx]);
			if (body.IsDynamic())
			{
				MotionProperties *mp = body.GetMotionProperties();
				Quat rotation = body.GetRotation();

				if (body.GetApplyGyroscopicForce())
					mp->ApplyGyroscopicForceInternal(rotation, delta_time);

				mp->ApplyForceTorqueAndDragInternal(rotation, mGravity, delta_time);
			}
			active_body_idx++;
		}
	}
}

void PhysicsSystem::JobSetupVelocityConstraints(float inDeltaTime, PhysicsUpdateContext::Step *ioStep) const
{
#ifdef JPH_ENABLE_ASSERTS
	// We only read positions
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::Read);
#endif

	uint32 num_constraints = ioStep->mNumActiveConstraints;

	for (;;)
	{
		// Atomically fetch a batch of constraints
		uint32 constraint_idx = ioStep->mSetupVelocityConstraintsReadIdx.fetch_add(cSetupVelocityConstraintsBatchSize);
		if (constraint_idx >= num_constraints)
			break;

		ConstraintManager::sSetupVelocityConstraints(ioStep->mContext->mActiveConstraints + constraint_idx, min<uint32>(cSetupVelocityConstraintsBatchSize, num_constraints - constraint_idx), inDeltaTime);
	}
}

void PhysicsSystem::JobBuildIslandsFromConstraints(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We read constraints and positions
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::Read);

	// Can only activate bodies
	BodyManager::GrantActiveBodiesAccess grant_active(true, false);
#endif

	// Prepare the island builder
	mIslandBuilder.PrepareNonContactConstraints(ioStep->mNumActiveConstraints, ioContext->mTempAllocator);

	// Build the islands
	ConstraintManager::sBuildIslands(ioStep->mContext->mActiveConstraints, ioStep->mNumActiveConstraints, mIslandBuilder, mBodyManager);
}

void PhysicsSystem::TrySpawnJobFindCollisions(PhysicsUpdateContext::Step *ioStep) const
{
	// Get how many jobs we can spawn and check if we can spawn more
	uint max_jobs = ioStep->mBodyPairQueues.size();
	if (CountBits(ioStep->mActiveFindCollisionJobs.load(memory_order_relaxed)) >= max_jobs)
		return;

	// Count how many body pairs we have waiting
	uint32 num_body_pairs = 0;
	for (const PhysicsUpdateContext::BodyPairQueue &queue : ioStep->mBodyPairQueues)
		num_body_pairs += queue.mWriteIdx - queue.mReadIdx;

	// Count how many active bodies we have waiting
	uint32 num_active_bodies = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody) - ioStep->mActiveBodyReadIdx;

	// Calculate how many jobs we would like
	uint desired_num_jobs = min((num_body_pairs + cNarrowPhaseBatchSize - 1) / cNarrowPhaseBatchSize + (num_active_bodies + cActiveBodiesBatchSize - 1) / cActiveBodiesBatchSize, max_jobs);

	for (;;)
	{
		// Get the bit mask of active jobs and see if we can spawn more
		PhysicsUpdateContext::JobMask current_active_jobs = ioStep->mActiveFindCollisionJobs.load(memory_order_relaxed);
		uint job_index = CountTrailingZeros(~current_active_jobs);
		if (job_index >= desired_num_jobs)
			break;

		// Try to claim the job index
		PhysicsUpdateContext::JobMask job_mask = PhysicsUpdateContext::JobMask(1) << job_index;
		PhysicsUpdateContext::JobMask prev_value = ioStep->mActiveFindCollisionJobs.fetch_or(job_mask, memory_order_acquire);
		if ((prev_value & job_mask) == 0)
		{
			// Add dependencies from the find collisions job to the next jobs
			ioStep->mUpdateBroadphaseFinalize.AddDependency();
			ioStep->mFinalizeIslands.AddDependency();

			// Start the job
			JobHandle job = ioStep->mContext->mJobSystem->CreateJob("FindCollisions", cColorFindCollisions, [step = ioStep, job_index]()
				{
					step->mContext->mPhysicsSystem->JobFindCollisions(step, job_index);
				});

			// Add the job to the job barrier so the main updating thread can execute the job too
			ioStep->mContext->mBarrier->AddJob(job);

			// Spawn only 1 extra job at a time
			return;
		}
	}
}

static void sFinalizeContactAllocator(PhysicsUpdateContext::Step &ioStep, const ContactConstraintManager::ContactAllocator &inAllocator)
{
	// Atomically accumulate the number of found manifolds and body pairs
	ioStep.mNumBodyPairs.fetch_add(inAllocator.mNumBodyPairs, memory_order_relaxed);
	ioStep.mNumManifolds.fetch_add(inAllocator.mNumManifolds, memory_order_relaxed);

	// Combine update errors
	ioStep.mContext->mErrors.fetch_or((uint32)inAllocator.mErrors, memory_order_relaxed);
}

// Disable TSAN for this function. It detects a false positive race condition on mBodyPairs.
// We have written mBodyPairs before doing mWriteIdx++ and we check mWriteIdx before reading mBodyPairs, so this should be safe.
JPH_TSAN_NO_SANITIZE
void PhysicsSystem::JobFindCollisions(PhysicsUpdateContext::Step *ioStep, int inJobIndex)
{
#ifdef JPH_ENABLE_ASSERTS
	// We read positions and read velocities (for elastic collisions)
	BodyAccess::Grant grant(BodyAccess::EAccess::Read, BodyAccess::EAccess::Read);

	// Can only activate bodies
	BodyManager::GrantActiveBodiesAccess grant_active(true, false);
#endif

	// Allocation context for allocating new contact points
	ContactAllocator contact_allocator(mContactManager.GetContactAllocator());

	// Determine initial queue to read pairs from if no broadphase work can be done
	// (always start looking at results from the next job)
	int read_queue_idx = (inJobIndex + 1) % ioStep->mBodyPairQueues.size();

	for (;;)
	{
		// Check if there are active bodies to be processed
		uint32 active_bodies_read_idx = ioStep->mActiveBodyReadIdx;
		uint32 num_active_bodies = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);
		if (active_bodies_read_idx < num_active_bodies)
		{
			// Take a batch of active bodies
			uint32 active_bodies_read_idx_end = min(num_active_bodies, active_bodies_read_idx + cActiveBodiesBatchSize);
			if (ioStep->mActiveBodyReadIdx.compare_exchange_strong(active_bodies_read_idx, active_bodies_read_idx_end))
			{
				// Callback when a new body pair is found
				class MyBodyPairCallback : public BodyPairCollector
				{
				public:
					// Constructor
											MyBodyPairCallback(PhysicsUpdateContext::Step *inStep, ContactAllocator &ioContactAllocator, int inJobIndex) :
						mStep(inStep),
						mContactAllocator(ioContactAllocator),
						mJobIndex(inJobIndex)
					{
					}

					// Callback function when a body pair is found
					virtual void			AddHit(const BodyPair &inPair) override
					{
						// Check if we have space in our write queue
						PhysicsUpdateContext::BodyPairQueue &queue = mStep->mBodyPairQueues[mJobIndex];
						uint32 body_pairs_in_queue = queue.mWriteIdx - queue.mReadIdx;
						if (body_pairs_in_queue >= mStep->mMaxBodyPairsPerQueue)
						{
							// Buffer full, process the pair now
							mStep->mContext->mPhysicsSystem->ProcessBodyPair(mContactAllocator, inPair);
						}
						else
						{
							// Store the pair in our own queue
							mStep->mContext->mBodyPairs[mJobIndex * mStep->mMaxBodyPairsPerQueue + queue.mWriteIdx % mStep->mMaxBodyPairsPerQueue] = inPair;
							++queue.mWriteIdx;
						}
					}

				private:
					PhysicsUpdateContext::Step *	mStep;
					ContactAllocator &				mContactAllocator;
					int								mJobIndex;
				};
				MyBodyPairCallback add_pair(ioStep, contact_allocator, inJobIndex);

				// Copy active bodies to temporary array, broadphase will reorder them
				uint32 batch_size = active_bodies_read_idx_end - active_bodies_read_idx;
				BodyID *active_bodies = (BodyID *)JPH_STACK_ALLOC(batch_size * sizeof(BodyID));
				memcpy(active_bodies, mBodyManager.GetActiveBodiesUnsafe(EBodyType::RigidBody) + active_bodies_read_idx, batch_size * sizeof(BodyID));

				// Find pairs in the broadphase
				mBroadPhase->FindCollidingPairs(active_bodies, batch_size, mPhysicsSettings.mSpeculativeContactDistance, *mObjectVsBroadPhaseLayerFilter, *mObjectLayerPairFilter, add_pair);

				// Check if we have enough pairs in the buffer to start a new job
				const PhysicsUpdateContext::BodyPairQueue &queue = ioStep->mBodyPairQueues[inJobIndex];
				uint32 body_pairs_in_queue = queue.mWriteIdx - queue.mReadIdx;
				if (body_pairs_in_queue >= cNarrowPhaseBatchSize)
					TrySpawnJobFindCollisions(ioStep);
			}
		}
		else
		{
			// Lockless loop to get the next body pair from the pairs buffer
			const PhysicsUpdateContext *context = ioStep->mContext;
			int first_read_queue_idx = read_queue_idx;
			for (;;)
			{
				PhysicsUpdateContext::BodyPairQueue &queue = ioStep->mBodyPairQueues[read_queue_idx];

				// Get the next pair to process
				uint32 pair_idx = queue.mReadIdx;

				// If the pair hasn't been written yet
				if (pair_idx >= queue.mWriteIdx)
				{
					// Go to the next queue
					read_queue_idx = (read_queue_idx + 1) % ioStep->mBodyPairQueues.size();

					// If we're back at the first queue, we've looked at all of them and found nothing
					if (read_queue_idx == first_read_queue_idx)
					{
						// Collect information from the contact allocator and accumulate it in the step.
						sFinalizeContactAllocator(*ioStep, contact_allocator);

						// Mark this job as inactive
						ioStep->mActiveFindCollisionJobs.fetch_and(~PhysicsUpdateContext::JobMask(1 << inJobIndex), memory_order_release);

						// Trigger the next jobs
						ioStep->mUpdateBroadphaseFinalize.RemoveDependency();
						ioStep->mFinalizeIslands.RemoveDependency();
						return;
					}

					// Try again reading from the next queue
					continue;
				}

				// Copy the body pair out of the buffer
				const BodyPair bp = context->mBodyPairs[read_queue_idx * ioStep->mMaxBodyPairsPerQueue + pair_idx % ioStep->mMaxBodyPairsPerQueue];

				// Mark this pair as taken
				if (queue.mReadIdx.compare_exchange_strong(pair_idx, pair_idx + 1))
				{
					// Process the actual body pair
					ProcessBodyPair(contact_allocator, bp);
					break;
				}
			}
		}
	}
}

void PhysicsSystem::ProcessBodyPair(ContactAllocator &ioContactAllocator, const BodyPair &inBodyPair)
{
	JPH_PROFILE_FUNCTION();

	// Fetch body pair
	Body *body1 = &mBodyManager.GetBody(inBodyPair.mBodyA);
	Body *body2 = &mBodyManager.GetBody(inBodyPair.mBodyB);
	JPH_ASSERT(body1->IsActive());

	JPH_DET_LOG("ProcessBodyPair: id1: " << inBodyPair.mBodyA << " id2: " << inBodyPair.mBodyB << " p1: " << body1->GetCenterOfMassPosition() << " p2: " << body2->GetCenterOfMassPosition() << " r1: " << body1->GetRotation() << " r2: " << body2->GetRotation());

	// Check for soft bodies
	if (body2->IsSoftBody())
	{
		// If the 2nd body is a soft body and not active, we activate it now
		if (!body2->IsActive())
			mBodyManager.ActivateBodies(&inBodyPair.mBodyB, 1);

		// Soft body processing is done later in the pipeline
		return;
	}

	// Ensure that body1 has the higher motion type (i.e. dynamic trumps kinematic), this ensures that we do the collision detection in the space of a moving body,
	// which avoids accuracy problems when testing a very large static object against a small dynamic object
	// Ensure that body1 id < body2 id when motion types are the same.
	if (body1->GetMotionType() < body2->GetMotionType()
		|| (body1->GetMotionType() == body2->GetMotionType() && inBodyPair.mBodyB < inBodyPair.mBodyA))
		std::swap(body1, body2);

	// Check if the contact points from the previous frame are reusable and if so copy them
	bool pair_handled = false, constraint_created = false;
	if (mPhysicsSettings.mUseBodyPairContactCache && !(body1->IsCollisionCacheInvalid() || body2->IsCollisionCacheInvalid()))
		mContactManager.GetContactsFromCache(ioContactAllocator, *body1, *body2, pair_handled, constraint_created);

	// If the cache hasn't handled this body pair do actual collision detection
	if (!pair_handled)
	{
		// Create entry in the cache for this body pair
		// Needs to happen irrespective if we found a collision or not (we want to remember that no collision was found too)
		ContactConstraintManager::BodyPairHandle body_pair_handle = mContactManager.AddBodyPair(ioContactAllocator, *body1, *body2);
		if (body_pair_handle == nullptr)
			return; // Out of cache space

		// If we want enhanced active edge detection for this body pair
		bool enhanced_active_edges = body1->GetEnhancedInternalEdgeRemovalWithBody(*body2);

		// Create the query settings
		CollideShapeSettings settings;
		settings.mCollectFacesMode = ECollectFacesMode::CollectFaces;
		settings.mActiveEdgeMode = mPhysicsSettings.mCheckActiveEdges && !enhanced_active_edges? EActiveEdgeMode::CollideOnlyWithActive : EActiveEdgeMode::CollideWithAll;
		settings.mMaxSeparationDistance = body1->IsSensor() || body2->IsSensor()? 0.0f : mPhysicsSettings.mSpeculativeContactDistance;
		settings.mActiveEdgeMovementDirection = body1->GetLinearVelocity() - body2->GetLinearVelocity();

		// Create shape filter
		SimShapeFilterWrapperUnion shape_filter_union(mSimShapeFilter, body1);
		SimShapeFilterWrapper &shape_filter = shape_filter_union.GetSimShapeFilterWrapper();
		shape_filter.SetBody2(body2);

		// Get transforms relative to body1
		RVec3 offset = body1->GetCenterOfMassPosition();
		Mat44 transform1 = Mat44::sRotation(body1->GetRotation());
		Mat44 transform2 = body2->GetCenterOfMassTransform().PostTranslated(-offset).ToMat44();

		if (mPhysicsSettings.mUseManifoldReduction				// Check global flag
			&& body1->GetUseManifoldReductionWithBody(*body2))	// Check body flag
		{
			// Version WITH contact manifold reduction

			class MyManifold : public ContactManifold
			{
			public:
				Vec3				mFirstWorldSpaceNormal;
			};

			// A temporary structure that allows us to keep track of the all manifolds between this body pair
			using Manifolds = StaticArray<MyManifold, 32>;

			// Create collector
			class ReductionCollideShapeCollector : public CollideShapeCollector
			{
			public:
								ReductionCollideShapeCollector(PhysicsSystem *inSystem, const Body *inBody1, const Body *inBody2) :
					mSystem(inSystem),
					mBody1(inBody1),
					mBody2(inBody2)
				{
				}

				virtual void	AddHit(const CollideShapeResult &inResult) override
				{
					// The first body should be the one with the highest motion type
					JPH_ASSERT(mBody1->GetMotionType() >= mBody2->GetMotionType());
					JPH_ASSERT(!ShouldEarlyOut());

					// Test if we want to accept this hit
					if (mValidateBodyPair)
					{
						switch (mSystem->mContactManager.ValidateContactPoint(*mBody1, *mBody2, mBody1->GetCenterOfMassPosition(), inResult))
						{
						case ValidateResult::AcceptContact:
							// We're just accepting this one, nothing to do
							break;

						case ValidateResult::AcceptAllContactsForThisBodyPair:
							// Accept and stop calling the validate callback
							mValidateBodyPair = false;
							break;

						case ValidateResult::RejectContact:
							// Skip this contact
							return;

						case ValidateResult::RejectAllContactsForThisBodyPair:
							// Skip this and early out
							ForceEarlyOut();
							return;
						}
					}

					// Calculate normal
					Vec3 world_space_normal = inResult.mPenetrationAxis.Normalized();

					// Check if we can add it to an existing manifold
					Manifolds::iterator manifold;
					float contact_normal_cos_max_delta_rot = mSystem->mPhysicsSettings.mContactNormalCosMaxDeltaRotation;
					for (manifold = mManifolds.begin(); manifold != mManifolds.end(); ++manifold)
						if (world_space_normal.Dot(manifold->mFirstWorldSpaceNormal) >= contact_normal_cos_max_delta_rot)
						{
							// Update average normal
							manifold->mWorldSpaceNormal += world_space_normal;
							manifold->mPenetrationDepth = max(manifold->mPenetrationDepth, inResult.mPenetrationDepth);
							break;
						}
					if (manifold == mManifolds.end())
					{
						// Check if array is full
						if (mManifolds.size() == mManifolds.capacity())
						{
							// Full, find manifold with least amount of penetration
							manifold = mManifolds.begin();
							for (Manifolds::iterator m = mManifolds.begin() + 1; m < mManifolds.end(); ++m)
								if (m->mPenetrationDepth < manifold->mPenetrationDepth)
									manifold = m;

							// If this contacts penetration is smaller than the smallest manifold, we skip this contact
							if (inResult.mPenetrationDepth < manifold->mPenetrationDepth)
								return;

							// Replace the manifold
							*manifold = { { mBody1->GetCenterOfMassPosition(), world_space_normal, inResult.mPenetrationDepth, inResult.mSubShapeID1, inResult.mSubShapeID2, { }, { } }, world_space_normal };
						}
						else
						{
							// Not full, create new manifold
							mManifolds.push_back({ { mBody1->GetCenterOfMassPosition(), world_space_normal, inResult.mPenetrationDepth, inResult.mSubShapeID1, inResult.mSubShapeID2, { }, { } }, world_space_normal });
							manifold = mManifolds.end() - 1;
						}
					}

					// Determine contact points
					const PhysicsSettings &settings = mSystem->mPhysicsSettings;
					ManifoldBetweenTwoFaces(inResult.mContactPointOn1, inResult.mContactPointOn2, inResult.mPenetrationAxis, Square(settings.mSpeculativeContactDistance) + settings.mManifoldToleranceSq, inResult.mShape1Face, inResult.mShape2Face, manifold->mRelativeContactPointsOn1, manifold->mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, mBody1->GetCenterOfMassPosition()));

					// Prune if we have more than 32 points (this means we could run out of space in the next iteration)
					if (manifold->mRelativeContactPointsOn1.size() > 32)
						PruneContactPoints(manifold->mFirstWorldSpaceNormal, manifold->mRelativeContactPointsOn1, manifold->mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, manifold->mBaseOffset));
				}

				PhysicsSystem *		mSystem;
				const Body *		mBody1;
				const Body *		mBody2;
				bool				mValidateBodyPair = true;
				Manifolds			mManifolds;
			};
			ReductionCollideShapeCollector collector(this, body1, body2);

			// Perform collision detection between the two shapes
			SubShapeIDCreator part1, part2;
			auto f = enhanced_active_edges? InternalEdgeRemovingCollector::sCollideShapeVsShape : CollisionDispatch::sCollideShapeVsShape;
			f(body1->GetShape(), body2->GetShape(), Vec3::sReplicate(1.0f), Vec3::sReplicate(1.0f), transform1, transform2, part1, part2, settings, collector, shape_filter);

			// Add the contacts
			for (ContactManifold &manifold : collector.mManifolds)
			{
				// Normalize the normal (is a sum of all normals from merged manifolds)
				manifold.mWorldSpaceNormal = manifold.mWorldSpaceNormal.Normalized();

				// If we still have too many points, prune them now
				if (manifold.mRelativeContactPointsOn1.size() > 4)
					PruneContactPoints(manifold.mWorldSpaceNormal, manifold.mRelativeContactPointsOn1, manifold.mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, manifold.mBaseOffset));

				// Actually add the contact points to the manager
				constraint_created |= mContactManager.AddContactConstraint(ioContactAllocator, body_pair_handle, *body1, *body2, manifold);
			}
		}
		else
		{
			// Version WITHOUT contact manifold reduction

			// Create collector
			class NonReductionCollideShapeCollector : public CollideShapeCollector
			{
			public:
								NonReductionCollideShapeCollector(PhysicsSystem *inSystem, ContactAllocator &ioContactAllocator, Body *inBody1, Body *inBody2, const ContactConstraintManager::BodyPairHandle &inPairHandle) :
					mSystem(inSystem),
					mContactAllocator(ioContactAllocator),
					mBody1(inBody1),
					mBody2(inBody2),
					mBodyPairHandle(inPairHandle)
				{
				}

				virtual void	AddHit(const CollideShapeResult &inResult) override
				{
					// The first body should be the one with the highest motion type
					JPH_ASSERT(mBody1->GetMotionType() >= mBody2->GetMotionType());
					JPH_ASSERT(!ShouldEarlyOut());

					// Test if we want to accept this hit
					if (mValidateBodyPair)
					{
						switch (mSystem->mContactManager.ValidateContactPoint(*mBody1, *mBody2, mBody1->GetCenterOfMassPosition(), inResult))
						{
						case ValidateResult::AcceptContact:
							// We're just accepting this one, nothing to do
							break;

						case ValidateResult::AcceptAllContactsForThisBodyPair:
							// Accept and stop calling the validate callback
							mValidateBodyPair = false;
							break;

						case ValidateResult::RejectContact:
							// Skip this contact
							return;

						case ValidateResult::RejectAllContactsForThisBodyPair:
							// Skip this and early out
							ForceEarlyOut();
							return;
						}
					}

					// Determine contact points
					ContactManifold manifold;
					manifold.mBaseOffset = mBody1->GetCenterOfMassPosition();
					const PhysicsSettings &settings = mSystem->mPhysicsSettings;
					ManifoldBetweenTwoFaces(inResult.mContactPointOn1, inResult.mContactPointOn2, inResult.mPenetrationAxis, Square(settings.mSpeculativeContactDistance) + settings.mManifoldToleranceSq, inResult.mShape1Face, inResult.mShape2Face, manifold.mRelativeContactPointsOn1, manifold.mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, manifold.mBaseOffset));

					// Calculate normal
					manifold.mWorldSpaceNormal = inResult.mPenetrationAxis.Normalized();

					// Store penetration depth
					manifold.mPenetrationDepth = inResult.mPenetrationDepth;

					// Prune if we have more than 4 points
					if (manifold.mRelativeContactPointsOn1.size() > 4)
						PruneContactPoints(manifold.mWorldSpaceNormal, manifold.mRelativeContactPointsOn1, manifold.mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, manifold.mBaseOffset));

					// Set other properties
					manifold.mSubShapeID1 = inResult.mSubShapeID1;
					manifold.mSubShapeID2 = inResult.mSubShapeID2;

					// Actually add the contact points to the manager
					mConstraintCreated |= mSystem->mContactManager.AddContactConstraint(mContactAllocator, mBodyPairHandle, *mBody1, *mBody2, manifold);
				}

				PhysicsSystem *		mSystem;
				ContactAllocator &	mContactAllocator;
				Body *				mBody1;
				Body *				mBody2;
				ContactConstraintManager::BodyPairHandle mBodyPairHandle;
				bool				mValidateBodyPair = true;
				bool				mConstraintCreated = false;
			};
			NonReductionCollideShapeCollector collector(this, ioContactAllocator, body1, body2, body_pair_handle);

			// Perform collision detection between the two shapes
			SubShapeIDCreator part1, part2;
			auto f = enhanced_active_edges? InternalEdgeRemovingCollector::sCollideShapeVsShape : CollisionDispatch::sCollideShapeVsShape;
			f(body1->GetShape(), body2->GetShape(), Vec3::sReplicate(1.0f), Vec3::sReplicate(1.0f), transform1, transform2, part1, part2, settings, collector, shape_filter);

			constraint_created = collector.mConstraintCreated;
		}
	}

	// If a contact constraint was created, we need to do some extra work
	if (constraint_created)
	{
		// Wake up sleeping bodies
		BodyID body_ids[2];
		int num_bodies = 0;
		if (body1->IsDynamic() && !body1->IsActive())
			body_ids[num_bodies++] = body1->GetID();
		if (body2->IsDynamic() && !body2->IsActive())
			body_ids[num_bodies++] = body2->GetID();
		if (num_bodies > 0)
			mBodyManager.ActivateBodies(body_ids, num_bodies);

		// Link the two bodies
		mIslandBuilder.LinkBodies(body1->GetIndexInActiveBodiesInternal(), body2->GetIndexInActiveBodiesInternal());
	}
}

void PhysicsSystem::JobFinalizeIslands(PhysicsUpdateContext *ioContext)
{
#ifdef JPH_ENABLE_ASSERTS
	// We only touch island data
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::None);
#endif

	// Finish collecting the islands, at this point the active body list doesn't change so it's safe to access
	mIslandBuilder.Finalize(mBodyManager.GetActiveBodiesUnsafe(EBodyType::RigidBody), mBodyManager.GetNumActiveBodies(EBodyType::RigidBody), mContactManager.GetNumConstraints(), ioContext->mTempAllocator);

	// Prepare the large island splitter
	if (mPhysicsSettings.mUseLargeIslandSplitter)
		mLargeIslandSplitter.Prepare(mIslandBuilder, mBodyManager.GetNumActiveBodies(EBodyType::RigidBody), ioContext->mTempAllocator);
}

void PhysicsSystem::JobBodySetIslandIndex()
{
#ifdef JPH_ENABLE_ASSERTS
	// We only touch island data
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::None);
#endif

	// Loop through the result and tag all bodies with an island index
	for (uint32 island_idx = 0, n = mIslandBuilder.GetNumIslands(); island_idx < n; ++island_idx)
	{
		BodyID *body_start, *body_end;
		mIslandBuilder.GetBodiesInIsland(island_idx, body_start, body_end);
		for (const BodyID *body = body_start; body < body_end; ++body)
			mBodyManager.GetBody(*body).GetMotionProperties()->SetIslandIndexInternal(island_idx);
	}
}

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wundefined-func-template") // ConstraintManager::sWarmStartVelocityConstraints / ContactConstraintManager::WarmStartVelocityConstraints is instantiated in the cpp file

void PhysicsSystem::JobSolveVelocityConstraints(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We update velocities and need to read positions to do so
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::Read);
#endif

	float delta_time = ioContext->mStepDeltaTime;
	Constraint **active_constraints = ioContext->mActiveConstraints;

	// Only the first step to correct for the delta time difference in the previous update
	float warm_start_impulse_ratio = ioStep->mIsFirst? ioContext->mWarmStartImpulseRatio : 1.0f;

	bool check_islands = true, check_split_islands = mPhysicsSettings.mUseLargeIslandSplitter;
	for (;;)
	{
		// First try to get work from large islands
		if (check_split_islands)
		{
			bool first_iteration;
			uint split_island_index;
			uint32 *constraints_begin, *constraints_end, *contacts_begin, *contacts_end;
			switch (mLargeIslandSplitter.FetchNextBatch(split_island_index, constraints_begin, constraints_end, contacts_begin, contacts_end, first_iteration))
			{
			case LargeIslandSplitter::EStatus::BatchRetrieved:
				{
					if (first_iteration)
					{
						// Iteration 0 is used to warm start the batch (we added 1 to the number of iterations in LargeIslandSplitter::SplitIsland)
						DummyCalculateSolverSteps dummy;
						ConstraintManager::sWarmStartVelocityConstraints(active_constraints, constraints_begin, constraints_end, warm_start_impulse_ratio, dummy);
						mContactManager.WarmStartVelocityConstraints(contacts_begin, contacts_end, warm_start_impulse_ratio, dummy);
					}
					else
					{
						// Solve velocity constraints
						ConstraintManager::sSolveVelocityConstraints(active_constraints, constraints_begin, constraints_end, delta_time);
						mContactManager.SolveVelocityConstraints(contacts_begin, contacts_end);
					}

					// Mark the batch as processed
					bool last_iteration, final_batch;
					mLargeIslandSplitter.MarkBatchProcessed(split_island_index, constraints_begin, constraints_end, contacts_begin, contacts_end, last_iteration, final_batch);

					// Save back the lambdas in the contact cache for the warm start of the next physics update
					if (last_iteration)
						mContactManager.StoreAppliedImpulses(contacts_begin, contacts_end);

					// We processed work, loop again
					continue;
				}

			case LargeIslandSplitter::EStatus::WaitingForBatch:
				break;

			case LargeIslandSplitter::EStatus::AllBatchesDone:
				check_split_islands = false;
				break;
			}
		}

		// If that didn't succeed try to process an island
		if (check_islands)
		{
			// Next island
			uint32 island_idx = ioStep->mSolveVelocityConstraintsNextIsland++;
			if (island_idx >= mIslandBuilder.GetNumIslands())
			{
				// We processed all islands, stop checking islands
				check_islands = false;
				continue;
			}

			JPH_PROFILE("Island");

			// Get iterators for this island
			uint32 *constraints_begin, *constraints_end, *contacts_begin, *contacts_end;
			bool has_constraints = mIslandBuilder.GetConstraintsInIsland(island_idx, constraints_begin, constraints_end);
			bool has_contacts = mIslandBuilder.GetContactsInIsland(island_idx, contacts_begin, contacts_end);

			// If we don't have any contacts or constraints, we know that none of the following islands have any contacts or constraints
			// (because they're sorted by most constraints first). This means we're done.
			if (!has_contacts && !has_constraints)
			{
			#ifdef JPH_ENABLE_ASSERTS
				// Validate our assumption that the next islands don't have any constraints or contacts
				for (; island_idx < mIslandBuilder.GetNumIslands(); ++island_idx)
				{
					JPH_ASSERT(!mIslandBuilder.GetConstraintsInIsland(island_idx, constraints_begin, constraints_end));
					JPH_ASSERT(!mIslandBuilder.GetContactsInIsland(island_idx, contacts_begin, contacts_end));
				}
			#endif // JPH_ENABLE_ASSERTS

				check_islands = false;
				continue;
			}

			// Sorting is costly but needed for a deterministic simulation, allow the user to turn this off
			if (mPhysicsSettings.mDeterministicSimulation)
			{
				// Sort constraints to give a deterministic simulation
				ConstraintManager::sSortConstraints(active_constraints, constraints_begin, constraints_end);

				// Sort contacts to give a deterministic simulation
				mContactManager.SortContacts(contacts_begin, contacts_end);
			}

			// Split up large islands
			CalculateSolverSteps steps_calculator(mPhysicsSettings);
			if (mPhysicsSettings.mUseLargeIslandSplitter
				&& mLargeIslandSplitter.SplitIsland(island_idx, mIslandBuilder, mBodyManager, mContactManager, active_constraints, steps_calculator))
				continue; // Loop again to try to fetch the newly split island

			// We didn't create a split, just run the solver now for this entire island. Begin by warm starting.
			ConstraintManager::sWarmStartVelocityConstraints(active_constraints, constraints_begin, constraints_end, warm_start_impulse_ratio, steps_calculator);
			mContactManager.WarmStartVelocityConstraints(contacts_begin, contacts_end, warm_start_impulse_ratio, steps_calculator);
			steps_calculator.Finalize();

			// Store the number of position steps for later
			mIslandBuilder.SetNumPositionSteps(island_idx, steps_calculator.GetNumPositionSteps());

			// Solve velocity constraints
			for (uint velocity_step = 0; velocity_step < steps_calculator.GetNumVelocitySteps(); ++velocity_step)
			{
				bool applied_impulse = ConstraintManager::sSolveVelocityConstraints(active_constraints, constraints_begin, constraints_end, delta_time);
				applied_impulse |= mContactManager.SolveVelocityConstraints(contacts_begin, contacts_end);
				if (!applied_impulse)
					break;
			}

			// Save back the lambdas in the contact cache for the warm start of the next physics update
			mContactManager.StoreAppliedImpulses(contacts_begin, contacts_end);

			// We processed work, loop again
			continue;
		}

		if (check_islands)
		{
			// If there are islands, we don't need to wait and can pick up new work
			continue;
		}
		else if (check_split_islands)
		{
			// If there are split islands, but we did't do any work, give up a time slice
			std::this_thread::yield();
		}
		else
		{
			// No more work
			break;
		}
	}
}

JPH_SUPPRESS_WARNING_POP

void PhysicsSystem::JobPreIntegrateVelocity(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
	// Reserve enough space for all bodies that may need a cast
	TempAllocator *temp_allocator = ioContext->mTempAllocator;
	JPH_ASSERT(ioStep->mCCDBodies == nullptr);
	ioStep->mCCDBodiesCapacity = mBodyManager.GetNumActiveCCDBodies();
	ioStep->mCCDBodies = (CCDBody *)temp_allocator->Allocate(ioStep->mCCDBodiesCapacity * sizeof(CCDBody));

	// Initialize the mapping table between active body and CCD body
	JPH_ASSERT(ioStep->mActiveBodyToCCDBody == nullptr);
	ioStep->mNumActiveBodyToCCDBody = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);
	ioStep->mActiveBodyToCCDBody = (int *)temp_allocator->Allocate(ioStep->mNumActiveBodyToCCDBody * sizeof(int));

	// Prepare the split island builder for solving the position constraints
	mLargeIslandSplitter.PrepareForSolvePositions();
}

void PhysicsSystem::JobIntegrateVelocity(const PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We update positions and need velocity to do so, we also clamp velocities so need to write to them
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::ReadWrite);
#endif

	float delta_time = ioContext->mStepDeltaTime;
	const BodyID *active_bodies = mBodyManager.GetActiveBodiesUnsafe(EBodyType::RigidBody);
	uint32 num_active_bodies = mBodyManager.GetNumActiveBodies(EBodyType::RigidBody);
	uint32 num_active_bodies_after_find_collisions = ioStep->mActiveBodyReadIdx;

	// We can move bodies that are not part of an island. In this case we need to notify the broadphase of the movement.
	static constexpr int cBodiesBatch = 64;
	BodyID *bodies_to_update_bounds = (BodyID *)JPH_STACK_ALLOC(cBodiesBatch * sizeof(BodyID));
	int num_bodies_to_update_bounds = 0;

	for (;;)
	{
		// Atomically fetch a batch of bodies
		uint32 active_body_idx = ioStep->mIntegrateVelocityReadIdx.fetch_add(cIntegrateVelocityBatchSize);
		if (active_body_idx >= num_active_bodies)
			break;

		// Calculate the end of the batch
		uint32 active_body_idx_end = min(num_active_bodies, active_body_idx + cIntegrateVelocityBatchSize);

		// Process the batch
		while (active_body_idx < active_body_idx_end)
		{
			// Update the positions using an Symplectic Euler step (which integrates using the updated velocity v1' rather
			// than the original velocity v1):
			// x1' = x1 + h * v1'
			// At this point the active bodies list does not change, so it is safe to access the array.
			BodyID body_id = active_bodies[active_body_idx];
			Body &body = mBodyManager.GetBody(body_id);
			MotionProperties *mp = body.GetMotionProperties();

			JPH_DET_LOG("JobIntegrateVelocity: id: " << body_id << " v: " << body.GetLinearVelocity() << " w: " << body.GetAngularVelocity());

			// Clamp velocities (not for kinematic bodies)
			if (body.IsDynamic())
			{
				mp->ClampLinearVelocity();
				mp->ClampAngularVelocity();
			}

			// Update the rotation of the body according to the angular velocity
			// For motion type discrete we need to do this anyway, for motion type linear cast we have multiple choices
			// 1. Rotate the body first and then sweep
			// 2. First sweep and then rotate the body at the end
			// 3. Pick some in between rotation (e.g. half way), then sweep and finally rotate the remainder
			// (1) has some clear advantages as when a long thin body hits a surface away from the center of mass, this will result in a large angular velocity and a limited reduction in linear velocity.
			// When simulation the rotation first before doing the translation, the body will be able to rotate away from the contact point allowing the center of mass to approach the surface. When using
			// approach (2) in this case what will happen is that we will immediately detect the same collision again (the body has not rotated and the body was already colliding at the end of the previous
			// time step) resulting in a lot of stolen time and the body appearing to be frozen in an unnatural pose (like it is glued at an angle to the surface). (2) obviously has some negative side effects
			// too as simulating the rotation first may cause it to tunnel through a small object that the linear cast might have otherwise detected. In any case a linear cast is not good for detecting
			// tunneling due to angular rotation, so we don't care about that too much (you'd need a full cast to take angular effects into account).
			body.AddRotationStep(body.GetAngularVelocity() * delta_time);

			// Get delta position
			Vec3 delta_pos = body.GetLinearVelocity() * delta_time;

			// If the position should be updated (or if it is delayed because of CCD)
			bool update_position = true;

			switch (mp->GetMotionQuality())
			{
			case EMotionQuality::Discrete:
				// No additional collision checking to be done
				break;

			case EMotionQuality::LinearCast:
				if (body.IsDynamic() // Kinematic bodies cannot be stopped
					&& !body.IsSensor()) // We don't support CCD sensors
				{
					// Determine inner radius (the smallest sphere that fits into the shape)
					float inner_radius = body.GetShape()->GetInnerRadius();
					JPH_ASSERT(inner_radius > 0.0f, "The shape has no inner radius, this makes the shape unsuitable for the linear cast motion quality as we cannot move it without risking tunneling.");

					// Measure translation in this step and check if it above the threshold to perform a linear cast
					float linear_cast_threshold_sq = Square(mPhysicsSettings.mLinearCastThreshold * inner_radius);
					if (delta_pos.LengthSq() > linear_cast_threshold_sq)
					{
						// This body needs a cast
						uint32 ccd_body_idx = ioStep->mNumCCDBodies++;
						JPH_ASSERT(active_body_idx < ioStep->mNumActiveBodyToCCDBody);
						ioStep->mActiveBodyToCCDBody[active_body_idx] = ccd_body_idx;
						new (&ioStep->mCCDBodies[ccd_body_idx]) CCDBody(body_id, delta_pos, linear_cast_threshold_sq, min(mPhysicsSettings.mPenetrationSlop, mPhysicsSettings.mLinearCastMaxPenetration * inner_radius));

						update_position = false;
					}
				}
				break;
			}

			if (update_position)
			{
				// Move the body now
				body.AddPositionStep(delta_pos);

				// If the body was activated due to an earlier CCD step it will have an index in the active
				// body list that it higher than the highest one we processed during FindCollisions
				// which means it hasn't been assigned an island and will not be updated by an island
				// this means that we need to update its bounds manually
				if (mp->GetIndexInActiveBodiesInternal() >= num_active_bodies_after_find_collisions)
				{
					body.CalculateWorldSpaceBoundsInternal();
					bodies_to_update_bounds[num_bodies_to_update_bounds++] = body.GetID();
					if (num_bodies_to_update_bounds == cBodiesBatch)
					{
						// Buffer full, flush now
						mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);
						num_bodies_to_update_bounds = 0;
					}
				}

				// We did not create a CCD body
				ioStep->mActiveBodyToCCDBody[active_body_idx] = -1;
			}

			active_body_idx++;
		}
	}

	// Notify change bounds on requested bodies
	if (num_bodies_to_update_bounds > 0)
		mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);
}

void PhysicsSystem::JobPostIntegrateVelocity(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep) const
{
	// Validate that our reservations were correct
	JPH_ASSERT(ioStep->mNumCCDBodies <= mBodyManager.GetNumActiveCCDBodies());

	if (ioStep->mNumCCDBodies == 0)
	{
		// No continuous collision detection jobs -> kick the next job ourselves
		ioStep->mContactRemovedCallbacks.RemoveDependency();
	}
	else
	{
		// Run the continuous collision detection jobs
		int num_continuous_collision_jobs = min(int(ioStep->mNumCCDBodies + cNumCCDBodiesPerJob - 1) / cNumCCDBodiesPerJob, ioContext->GetMaxConcurrency());
		ioStep->mResolveCCDContacts.AddDependency(num_continuous_collision_jobs);
		ioStep->mContactRemovedCallbacks.AddDependency(num_continuous_collision_jobs - 1); // Already had 1 dependency
		for (int i = 0; i < num_continuous_collision_jobs; ++i)
		{
			JobHandle job = ioContext->mJobSystem->CreateJob("FindCCDContacts", cColorFindCCDContacts, [ioContext, ioStep]()
			{
				ioContext->mPhysicsSystem->JobFindCCDContacts(ioContext, ioStep);

				ioStep->mResolveCCDContacts.RemoveDependency();
				ioStep->mContactRemovedCallbacks.RemoveDependency();
			});
			ioContext->mBarrier->AddJob(job);
		}
	}
}

// Helper function to calculate the motion of a body during this CCD step
inline static Vec3 sCalculateBodyMotion(const Body &inBody, float inDeltaTime)
{
	// If the body is linear casting, the body has not yet moved so we need to calculate its motion
	if (inBody.IsDynamic() && inBody.GetMotionProperties()->GetMotionQuality() == EMotionQuality::LinearCast)
		return inDeltaTime * inBody.GetLinearVelocity();

	// Body has already moved, so we don't need to correct for anything
	return Vec3::sZero();
}

// Helper function that finds the CCD body corresponding to a body (if it exists)
inline static PhysicsUpdateContext::Step::CCDBody *sGetCCDBody(const Body &inBody, PhysicsUpdateContext::Step *inStep)
{
	// Only rigid bodies can have a CCD body
	if (!inBody.IsRigidBody())
		return nullptr;

	// If the body has no motion properties it cannot have a CCD body
	const MotionProperties *motion_properties = inBody.GetMotionPropertiesUnchecked();
	if (motion_properties == nullptr)
		return nullptr;

	// If it is not active it cannot have a CCD body
	uint32 active_index = motion_properties->GetIndexInActiveBodiesInternal();
	if (active_index == Body::cInactiveIndex)
		return nullptr;

	// Check if the active body has a corresponding CCD body
	JPH_ASSERT(active_index < inStep->mNumActiveBodyToCCDBody); // Ensure that the body has a mapping to CCD body
	int ccd_index = inStep->mActiveBodyToCCDBody[active_index];
	if (ccd_index < 0)
		return nullptr;

	PhysicsUpdateContext::Step::CCDBody *ccd_body = &inStep->mCCDBodies[ccd_index];
	JPH_ASSERT(ccd_body->mBodyID1 == inBody.GetID(), "We found the wrong CCD body!");
	return ccd_body;
}

void PhysicsSystem::JobFindCCDContacts(const PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We only read positions, but the validate callback may read body positions and velocities
	BodyAccess::Grant grant(BodyAccess::EAccess::Read, BodyAccess::EAccess::Read);
#endif

	// Allocation context for allocating new contact points
	ContactAllocator contact_allocator(mContactManager.GetContactAllocator());

	// Settings
	ShapeCastSettings settings;
	settings.mUseShrunkenShapeAndConvexRadius = true;
	settings.mBackFaceModeTriangles = EBackFaceMode::IgnoreBackFaces;
	settings.mBackFaceModeConvex = EBackFaceMode::IgnoreBackFaces;
	settings.mReturnDeepestPoint = true;
	settings.mCollectFacesMode = ECollectFacesMode::CollectFaces;
	settings.mActiveEdgeMode = mPhysicsSettings.mCheckActiveEdges? EActiveEdgeMode::CollideOnlyWithActive : EActiveEdgeMode::CollideWithAll;

	for (;;)
	{
		// Fetch the next body to cast
		uint32 idx = ioStep->mNextCCDBody++;
		if (idx >= ioStep->mNumCCDBodies)
			break;
		CCDBody &ccd_body = ioStep->mCCDBodies[idx];
		const Body &body = mBodyManager.GetBody(ccd_body.mBodyID1);

		// Filter out layers
		DefaultBroadPhaseLayerFilter broadphase_layer_filter = GetDefaultBroadPhaseLayerFilter(body.GetObjectLayer());
		DefaultObjectLayerFilter object_layer_filter = GetDefaultLayerFilter(body.GetObjectLayer());

	#ifdef JPH_DEBUG_RENDERER
		// Draw start and end shape of cast
		if (sDrawMotionQualityLinearCast)
		{
			RMat44 com = body.GetCenterOfMassTransform();
			body.GetShape()->Draw(DebugRenderer::sInstance, com, Vec3::sReplicate(1.0f), Color::sGreen, false, true);
			DebugRenderer::sInstance->DrawArrow(com.GetTranslation(), com.GetTranslation() + ccd_body.mDeltaPosition, Color::sGreen, 0.1f);
			body.GetShape()->Draw(DebugRenderer::sInstance, com.PostTranslated(ccd_body.mDeltaPosition), Vec3::sReplicate(1.0f), Color::sRed, false, true);
		}
	#endif // JPH_DEBUG_RENDERER

		// Create a collector that will find the maximum distance allowed to travel while not penetrating more than 'max penetration'
		class CCDNarrowPhaseCollector : public CastShapeCollector
		{
		public:
										CCDNarrowPhaseCollector(const BodyManager &inBodyManager, ContactConstraintManager &inContactConstraintManager, CCDBody &inCCDBody, ShapeCastResult &inResult, float inDeltaTime) :
				mBodyManager(inBodyManager),
				mContactConstraintManager(inContactConstraintManager),
				mCCDBody(inCCDBody),
				mResult(inResult),
				mDeltaTime(inDeltaTime)
			{
			}

			virtual void				AddHit(const ShapeCastResult &inResult) override
			{
				JPH_PROFILE_FUNCTION();

				// Check if this is a possible earlier hit than the one before
				float fraction = inResult.mFraction;
				if (fraction < mCCDBody.mFractionPlusSlop)
				{
					// Normalize normal
					Vec3 normal = inResult.mPenetrationAxis.Normalized();

					// Calculate how much we can add to the fraction to penetrate the collision point by mMaxPenetration.
					// Note that the normal is pointing towards body 2!
					// Let the extra distance that we can travel along delta_pos be 'dist': mMaxPenetration / dist = cos(angle between normal and delta_pos) = normal . delta_pos / |delta_pos|
					// <=> dist = mMaxPenetration * |delta_pos| / normal . delta_pos
					// Converting to a faction: delta_fraction = dist / |delta_pos| = mLinearCastTreshold / normal . delta_pos
					float denominator = normal.Dot(mCCDBody.mDeltaPosition);
					if (denominator > mCCDBody.mMaxPenetration) // Avoid dividing by zero, if extra hit fraction > 1 there's also no point in continuing
					{
						float fraction_plus_slop = fraction + mCCDBody.mMaxPenetration / denominator;
						if (fraction_plus_slop < mCCDBody.mFractionPlusSlop)
						{
							const Body &body2 = mBodyManager.GetBody(inResult.mBodyID2);

							// Check if we've already accepted all hits from this body
							if (mValidateBodyPair)
							{
								// Validate the contact result
								const Body &body1 = mBodyManager.GetBody(mCCDBody.mBodyID1);
								ValidateResult validate_result = mContactConstraintManager.ValidateContactPoint(body1, body2, body1.GetCenterOfMassPosition(), inResult); // Note that the center of mass of body 1 is the start of the sweep and is used as base offset below
								switch (validate_result)
								{
								case ValidateResult::AcceptContact:
									// Just continue
									break;

								case ValidateResult::AcceptAllContactsForThisBodyPair:
									// Accept this and all following contacts from this body
									mValidateBodyPair = false;
									break;

								case ValidateResult::RejectContact:
									return;

								case ValidateResult::RejectAllContactsForThisBodyPair:
									// Reject this and all following contacts from this body
									mRejectAll = true;
									ForceEarlyOut();
									return;
								}
							}

							// This is the earliest hit so far, store it
							mCCDBody.mContactNormal = normal;
							mCCDBody.mBodyID2 = inResult.mBodyID2;
							mCCDBody.mSubShapeID2 = inResult.mSubShapeID2;
							mCCDBody.mFraction = fraction;
							mCCDBody.mFractionPlusSlop = fraction_plus_slop;
							mResult = inResult;

							// Result was assuming body 2 is not moving, but it is, so we need to correct for it
							Vec3 movement2 = fraction * sCalculateBodyMotion(body2, mDeltaTime);
							if (!movement2.IsNearZero())
							{
								mResult.mContactPointOn1 += movement2;
								mResult.mContactPointOn2 += movement2;
								for (Vec3 &v : mResult.mShape1Face)
									v += movement2;
								for (Vec3 &v : mResult.mShape2Face)
									v += movement2;
							}

							// Update early out fraction
							UpdateEarlyOutFraction(fraction_plus_slop);
						}
					}
				}
			}

			bool						mValidateBodyPair;				///< If we still have to call the ValidateContactPoint for this body pair
			bool						mRejectAll;						///< Reject all further contacts between this body pair

		private:
			const BodyManager &			mBodyManager;
			ContactConstraintManager &	mContactConstraintManager;
			CCDBody &					mCCDBody;
			ShapeCastResult &			mResult;
			float						mDeltaTime;
			BodyID						mAcceptedBodyID;
		};

		// Narrowphase collector
		ShapeCastResult cast_shape_result;
		CCDNarrowPhaseCollector np_collector(mBodyManager, mContactManager, ccd_body, cast_shape_result, ioContext->mStepDeltaTime);

		// This collector wraps the narrowphase collector and collects the closest hit
		class CCDBroadPhaseCollector : public CastShapeBodyCollector
		{
		public:
										CCDBroadPhaseCollector(const CCDBody &inCCDBody, const Body &inBody1, const RShapeCast &inShapeCast, ShapeCastSettings &inShapeCastSettings, SimShapeFilterWrapper &inShapeFilter, CCDNarrowPhaseCollector &ioCollector, const BodyManager &inBodyManager, PhysicsUpdateContext::Step *inStep, float inDeltaTime) :
				mCCDBody(inCCDBody),
				mBody1(inBody1),
				mBody1Extent(inShapeCast.mShapeWorldBounds.GetExtent()),
				mShapeCast(inShapeCast),
				mShapeCastSettings(inShapeCastSettings),
				mShapeFilter(inShapeFilter),
				mCollector(ioCollector),
				mBodyManager(inBodyManager),
				mStep(inStep),
				mDeltaTime(inDeltaTime)
			{
			}

			virtual void				AddHit(const BroadPhaseCastResult &inResult) override
			{
				JPH_PROFILE_FUNCTION();

				JPH_ASSERT(inResult.mFraction <= GetEarlyOutFraction(), "This hit should not have been passed on to the collector");

				// Test if we're colliding with ourselves
				if (mBody1.GetID() == inResult.mBodyID)
					return;

				// Avoid treating duplicates, if both bodies are doing CCD then only consider collision if body ID < other body ID
				const Body &body2 = mBodyManager.GetBody(inResult.mBodyID);
				const CCDBody *ccd_body2 = sGetCCDBody(body2, mStep);
				if (ccd_body2 != nullptr && mCCDBody.mBodyID1 > ccd_body2->mBodyID1)
					return;

				// Test group filter
				if (!mBody1.GetCollisionGroup().CanCollide(body2.GetCollisionGroup()))
					return;

				// TODO: For now we ignore sensors
				if (body2.IsSensor())
					return;

				// Get relative movement of these two bodies
				Vec3 direction = mShapeCast.mDirection - sCalculateBodyMotion(body2, mDeltaTime);

				// Test if the remaining movement is less than our movement threshold
				if (direction.LengthSq() < mCCDBody.mLinearCastThresholdSq)
					return;

				// Get the bounds of 2, widen it by the extent of 1 and test a ray to see if it hits earlier than the current early out fraction
				AABox bounds = body2.GetWorldSpaceBounds();
				bounds.mMin -= mBody1Extent;
				bounds.mMax += mBody1Extent;
				float hit_fraction = RayAABox(Vec3(mShapeCast.mCenterOfMassStart.GetTranslation()), RayInvDirection(direction), bounds.mMin, bounds.mMax);
				if (hit_fraction > GetPositiveEarlyOutFraction()) // If early out fraction <= 0, we have the possibility of finding a deeper hit so we need to clamp the early out fraction
					return;

				// Reset collector (this is a new body pair)
				mCollector.ResetEarlyOutFraction(GetEarlyOutFraction());
				mCollector.mValidateBodyPair = true;
				mCollector.mRejectAll = false;

				// Set body ID on shape filter
				mShapeFilter.SetBody2(&body2);

				// Provide direction as hint for the active edges algorithm
				mShapeCastSettings.mActiveEdgeMovementDirection = direction;

				// Do narrow phase collision check
				RShapeCast relative_cast(mShapeCast.mShape, mShapeCast.mScale, mShapeCast.mCenterOfMassStart, direction, mShapeCast.mShapeWorldBounds);
				body2.GetTransformedShape().CastShape(relative_cast, mShapeCastSettings, mShapeCast.mCenterOfMassStart.GetTranslation(), mCollector, mShapeFilter);

				// Update early out fraction based on narrow phase collector
				if (!mCollector.mRejectAll)
					UpdateEarlyOutFraction(mCollector.GetEarlyOutFraction());
			}

			const CCDBody &				mCCDBody;
			const Body &				mBody1;
			Vec3						mBody1Extent;
			RShapeCast					mShapeCast;
			ShapeCastSettings &			mShapeCastSettings;
			SimShapeFilterWrapper &		mShapeFilter;
			CCDNarrowPhaseCollector &	mCollector;
			const BodyManager &			mBodyManager;
			PhysicsUpdateContext::Step *mStep;
			float						mDeltaTime;
		};

		// Create shape filter
		SimShapeFilterWrapperUnion shape_filter_union(mSimShapeFilter, &body);
		SimShapeFilterWrapper &shape_filter = shape_filter_union.GetSimShapeFilterWrapper();

		// Check if we collide with any other body. Note that we use the non-locking interface as we know the broadphase cannot be modified at this point.
		RShapeCast shape_cast(body.GetShape(), Vec3::sReplicate(1.0f), body.GetCenterOfMassTransform(), ccd_body.mDeltaPosition);
		CCDBroadPhaseCollector bp_collector(ccd_body, body, shape_cast, settings, shape_filter, np_collector, mBodyManager, ioStep, ioContext->mStepDeltaTime);
		mBroadPhase->CastAABoxNoLock({ shape_cast.mShapeWorldBounds, shape_cast.mDirection }, bp_collector, broadphase_layer_filter, object_layer_filter);

		// Check if there was a hit
		if (ccd_body.mFractionPlusSlop < 1.0f)
		{
			const Body &body2 = mBodyManager.GetBody(ccd_body.mBodyID2);

			// Determine contact manifold
			ContactManifold manifold;
			manifold.mBaseOffset = shape_cast.mCenterOfMassStart.GetTranslation();
			ManifoldBetweenTwoFaces(cast_shape_result.mContactPointOn1, cast_shape_result.mContactPointOn2, cast_shape_result.mPenetrationAxis, mPhysicsSettings.mManifoldToleranceSq, cast_shape_result.mShape1Face, cast_shape_result.mShape2Face, manifold.mRelativeContactPointsOn1, manifold.mRelativeContactPointsOn2 JPH_IF_DEBUG_RENDERER(, manifold.mBaseOffset));
			manifold.mSubShapeID1 = cast_shape_result.mSubShapeID1;
			manifold.mSubShapeID2 = cast_shape_result.mSubShapeID2;
			manifold.mPenetrationDepth = cast_shape_result.mPenetrationDepth;
			manifold.mWorldSpaceNormal = ccd_body.mContactNormal;

			// Call contact point callbacks
			mContactManager.OnCCDContactAdded(contact_allocator, body, body2, manifold, ccd_body.mContactSettings);

			if (ccd_body.mContactSettings.mIsSensor)
			{
				// If this is a sensor, we don't want to solve the contact
				ccd_body.mFractionPlusSlop = 1.0f;
				ccd_body.mBodyID2 = BodyID();
			}
			else
			{
				// Calculate the average position from the manifold (this will result in the same impulse applied as when we apply impulses to all contact points)
				if (manifold.mRelativeContactPointsOn2.size() > 1)
				{
					Vec3 average_contact_point = Vec3::sZero();
					for (const Vec3 &v : manifold.mRelativeContactPointsOn2)
						average_contact_point += v;
					average_contact_point /= (float)manifold.mRelativeContactPointsOn2.size();
					ccd_body.mContactPointOn2 = manifold.mBaseOffset + average_contact_point;
				}
				else
					ccd_body.mContactPointOn2 = manifold.mBaseOffset + cast_shape_result.mContactPointOn2;
			}
		}
	}

	// Collect information from the contact allocator and accumulate it in the step.
	sFinalizeContactAllocator(*ioStep, contact_allocator);
}

void PhysicsSystem::JobResolveCCDContacts(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// Read/write body access
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::ReadWrite);

	// We activate bodies that we collide with
	BodyManager::GrantActiveBodiesAccess grant_active(true, false);
#endif

	uint32 num_active_bodies_after_find_collisions = ioStep->mActiveBodyReadIdx;
	TempAllocator *temp_allocator = ioContext->mTempAllocator;

	// Check if there's anything to do
	uint num_ccd_bodies = ioStep->mNumCCDBodies;
	if (num_ccd_bodies > 0)
	{
		// Sort on fraction so that we process earliest collisions first
		// This is needed to make the simulation deterministic and also to be able to stop contact processing
		// between body pairs if an earlier hit was found involving the body by another CCD body
		// (if it's body ID < this CCD body's body ID - see filtering logic in CCDBroadPhaseCollector)
		CCDBody **sorted_ccd_bodies = (CCDBody **)temp_allocator->Allocate(num_ccd_bodies * sizeof(CCDBody *));
		JPH_SCOPE_EXIT([temp_allocator, sorted_ccd_bodies, num_ccd_bodies]{ temp_allocator->Free(sorted_ccd_bodies, num_ccd_bodies * sizeof(CCDBody *)); });
		{
			JPH_PROFILE("Sort");

			// We don't want to copy the entire struct (it's quite big), so we create a pointer array first
			CCDBody *src_ccd_bodies = ioStep->mCCDBodies;
			CCDBody **dst_ccd_bodies = sorted_ccd_bodies;
			CCDBody **dst_ccd_bodies_end = dst_ccd_bodies + num_ccd_bodies;
			while (dst_ccd_bodies < dst_ccd_bodies_end)
				*(dst_ccd_bodies++) = src_ccd_bodies++;

			// Which we then sort
			QuickSort(sorted_ccd_bodies, sorted_ccd_bodies + num_ccd_bodies, [](const CCDBody *inBody1, const CCDBody *inBody2)
				{
					if (inBody1->mFractionPlusSlop != inBody2->mFractionPlusSlop)
						return inBody1->mFractionPlusSlop < inBody2->mFractionPlusSlop;

					return inBody1->mBodyID1 < inBody2->mBodyID1;
				});
		}

		// We can collide with bodies that are not active, we track them here so we can activate them in one go at the end.
		// This is also needed because we can't modify the active body array while we iterate it.
		static constexpr int cBodiesBatch = 64;
		BodyID *bodies_to_activate = (BodyID *)JPH_STACK_ALLOC(cBodiesBatch * sizeof(BodyID));
		int num_bodies_to_activate = 0;

		// We can move bodies that are not part of an island. In this case we need to notify the broadphase of the movement.
		BodyID *bodies_to_update_bounds = (BodyID *)JPH_STACK_ALLOC(cBodiesBatch * sizeof(BodyID));
		int num_bodies_to_update_bounds = 0;

		for (uint i = 0; i < num_ccd_bodies; ++i)
		{
			const CCDBody *ccd_body = sorted_ccd_bodies[i];
			Body &body1 = mBodyManager.GetBody(ccd_body->mBodyID1);
			MotionProperties *body_mp = body1.GetMotionProperties();

			// If there was a hit
			if (!ccd_body->mBodyID2.IsInvalid())
			{
				Body &body2 = mBodyManager.GetBody(ccd_body->mBodyID2);

				// Determine if the other body has a CCD body
				CCDBody *ccd_body2 = sGetCCDBody(body2, ioStep);
				if (ccd_body2 != nullptr)
				{
					JPH_ASSERT(ccd_body2->mBodyID2 != ccd_body->mBodyID1, "If we collided with another body, that other body should have ignored collisions with us!");

					// Check if the other body found a hit that is further away
					if (ccd_body2->mFraction > ccd_body->mFraction)
					{
						// Reset the colliding body of the other CCD body. The other body will shorten its distance traveled and will not do any collision response (we'll do that).
						// This means that at this point we have triggered a contact point add/persist for our further hit by accident for the other body.
						// We accept this as calling the contact point callbacks here would require persisting the manifolds up to this point and doing the callbacks single threaded.
						ccd_body2->mBodyID2 = BodyID();
						ccd_body2->mFractionPlusSlop = ccd_body->mFraction;
					}
				}

				// If the other body moved less than us before hitting something, we're not colliding with it so we again have triggered contact point add/persist callbacks by accident.
				// We'll just move to the collision position anyway (as that's the last position we know is good), but we won't do any collision response.
				if (ccd_body2 == nullptr || ccd_body2->mFraction >= ccd_body->mFraction)
				{
					const ContactSettings &contact_settings = ccd_body->mContactSettings;

					// Calculate contact point velocity for body 1
					Vec3 r1_plus_u = Vec3(ccd_body->mContactPointOn2 - (body1.GetCenterOfMassPosition() + ccd_body->mFraction * ccd_body->mDeltaPosition));
					Vec3 v1 = body1.GetPointVelocityCOM(r1_plus_u);

					// Calculate inverse mass for body 1
					float inv_m1 = contact_settings.mInvMassScale1 * body_mp->GetInverseMass();

					if (body2.IsRigidBody())
					{
						// Calculate contact point velocity for body 2
						Vec3 r2 = Vec3(ccd_body->mContactPointOn2 - body2.GetCenterOfMassPosition());
						Vec3 v2 = body2.GetPointVelocityCOM(r2);

						// Calculate relative contact velocity
						Vec3 relative_velocity = v2 - v1;
						float normal_velocity = relative_velocity.Dot(ccd_body->mContactNormal);

						// Calculate velocity bias due to restitution
						float normal_velocity_bias;
						if (contact_settings.mCombinedRestitution > 0.0f && normal_velocity < -mPhysicsSettings.mMinVelocityForRestitution)
							normal_velocity_bias = contact_settings.mCombinedRestitution * normal_velocity;
						else
							normal_velocity_bias = 0.0f;

						// Get inverse mass of body 2
						float inv_m2 = body2.GetMotionPropertiesUnchecked() != nullptr? contact_settings.mInvMassScale2 * body2.GetMotionPropertiesUnchecked()->GetInverseMassUnchecked() : 0.0f;

						// Solve contact constraint
						AxisConstraintPart contact_constraint;
						contact_constraint.CalculateConstraintPropertiesWithMassOverride(body1, inv_m1, contact_settings.mInvInertiaScale1, r1_plus_u, body2, inv_m2, contact_settings.mInvInertiaScale2, r2, ccd_body->mContactNormal, normal_velocity_bias);
						contact_constraint.SolveVelocityConstraintWithMassOverride(body1, inv_m1, body2, inv_m2, ccd_body->mContactNormal, -FLT_MAX, FLT_MAX);

						// Apply friction
						if (contact_settings.mCombinedFriction > 0.0f)
						{
							// Calculate friction direction by removing normal velocity from the relative velocity
							Vec3 friction_direction = relative_velocity - normal_velocity * ccd_body->mContactNormal;
							float friction_direction_len_sq = friction_direction.LengthSq();
							if (friction_direction_len_sq > 1.0e-12f)
							{
								// Normalize friction direction
								friction_direction /= sqrt(friction_direction_len_sq);

								// Calculate max friction impulse
								float max_lambda_f = contact_settings.mCombinedFriction * contact_constraint.GetTotalLambda();

								AxisConstraintPart friction;
								friction.CalculateConstraintPropertiesWithMassOverride(body1, inv_m1, contact_settings.mInvInertiaScale1, r1_plus_u, body2, inv_m2, contact_settings.mInvInertiaScale2, r2, friction_direction);
								friction.SolveVelocityConstraintWithMassOverride(body1, inv_m1, body2, inv_m2, friction_direction, -max_lambda_f, max_lambda_f);
							}
						}

						// Clamp velocity of body 2
						if (body2.IsDynamic())
						{
							MotionProperties *body2_mp = body2.GetMotionProperties();
							body2_mp->ClampLinearVelocity();
							body2_mp->ClampAngularVelocity();
						}
					}
					else
					{
						SoftBodyMotionProperties *soft_mp = static_cast<SoftBodyMotionProperties *>(body2.GetMotionProperties());
						const SoftBodyShape *soft_shape = static_cast<const SoftBodyShape *>(body2.GetShape());

						// Convert the sub shape ID of the soft body to a face
						uint32 face_idx = soft_shape->GetFaceIndex(ccd_body->mSubShapeID2);
						const SoftBodyMotionProperties::Face &face = soft_mp->GetFace(face_idx);

						// Get vertices of the face
						SoftBodyMotionProperties::Vertex &vtx0 = soft_mp->GetVertex(face.mVertex[0]);
						SoftBodyMotionProperties::Vertex &vtx1 = soft_mp->GetVertex(face.mVertex[1]);
						SoftBodyMotionProperties::Vertex &vtx2 = soft_mp->GetVertex(face.mVertex[2]);

						// Inverse mass of the face
						float vtx0_mass = vtx0.mInvMass > 0.0f? 1.0f / vtx0.mInvMass : 1.0e10f;
						float vtx1_mass = vtx1.mInvMass > 0.0f? 1.0f / vtx1.mInvMass : 1.0e10f;
						float vtx2_mass = vtx2.mInvMass > 0.0f? 1.0f / vtx2.mInvMass : 1.0e10f;
						float inv_m2 = 1.0f / (vtx0_mass + vtx1_mass + vtx2_mass);

						// Calculate barycentric coordinates of the contact point on the soft body's face
						float u, v, w;
						RMat44 inv_body2_transform = body2.GetInverseCenterOfMassTransform();
						Vec3 local_contact = Vec3(inv_body2_transform * ccd_body->mContactPointOn2);
						ClosestPoint::GetBaryCentricCoordinates(vtx0.mPosition - local_contact, vtx1.mPosition - local_contact, vtx2.mPosition - local_contact, u, v, w);

						// Calculate contact point velocity for the face
						Vec3 v2 = inv_body2_transform.Multiply3x3Transposed(u * vtx0.mVelocity + v * vtx1.mVelocity + w * vtx2.mVelocity);
						float normal_velocity = (v2 - v1).Dot(ccd_body->mContactNormal);

						// Calculate velocity bias due to restitution
						float normal_velocity_bias;
						if (contact_settings.mCombinedRestitution > 0.0f && normal_velocity < -mPhysicsSettings.mMinVelocityForRestitution)
							normal_velocity_bias = contact_settings.mCombinedRestitution * normal_velocity;
						else
							normal_velocity_bias = 0.0f;

						// Calculate resulting velocity change (the math here is similar to AxisConstraintPart but without an inertia term for body 2 as we treat it as a point mass)
						Vec3 r1_plus_u_x_n = r1_plus_u.Cross(ccd_body->mContactNormal);
						Vec3 invi1_r1_plus_u_x_n = contact_settings.mInvInertiaScale1 * body1.GetInverseInertia().Multiply3x3(r1_plus_u_x_n);
						float jv = r1_plus_u_x_n.Dot(body_mp->GetAngularVelocity()) - normal_velocity - normal_velocity_bias;
						float inv_effective_mass = inv_m1 + inv_m2 + invi1_r1_plus_u_x_n.Dot(r1_plus_u_x_n);
						float lambda = jv / inv_effective_mass;
						body_mp->SubLinearVelocityStep((lambda * inv_m1) * ccd_body->mContactNormal);
						body_mp->SubAngularVelocityStep(lambda * invi1_r1_plus_u_x_n);
						Vec3 delta_v2 = inv_body2_transform.Multiply3x3(lambda * ccd_body->mContactNormal);
						vtx0.mVelocity += delta_v2 * vtx0.mInvMass;
						vtx1.mVelocity += delta_v2 * vtx1.mInvMass;
						vtx2.mVelocity += delta_v2 * vtx2.mInvMass;
					}

					// Clamp velocity of body 1
					body_mp->ClampLinearVelocity();
					body_mp->ClampAngularVelocity();

					// Activate the 2nd body if it is not already active
					if (body2.IsDynamic() && !body2.IsActive())
					{
						bodies_to_activate[num_bodies_to_activate++] = ccd_body->mBodyID2;
						if (num_bodies_to_activate == cBodiesBatch)
						{
							// Batch is full, activate now
							mBodyManager.ActivateBodies(bodies_to_activate, num_bodies_to_activate);
							num_bodies_to_activate = 0;
						}
					}

				#ifdef JPH_DEBUG_RENDERER
					if (sDrawMotionQualityLinearCast)
					{
						// Draw the collision location
						RMat44 collision_transform = body1.GetCenterOfMassTransform().PostTranslated(ccd_body->mFraction * ccd_body->mDeltaPosition);
						body1.GetShape()->Draw(DebugRenderer::sInstance, collision_transform, Vec3::sReplicate(1.0f), Color::sYellow, false, true);

						// Draw the collision location + slop
						RMat44 collision_transform_plus_slop = body1.GetCenterOfMassTransform().PostTranslated(ccd_body->mFractionPlusSlop * ccd_body->mDeltaPosition);
						body1.GetShape()->Draw(DebugRenderer::sInstance, collision_transform_plus_slop, Vec3::sReplicate(1.0f), Color::sOrange, false, true);

						// Draw contact normal
						DebugRenderer::sInstance->DrawArrow(ccd_body->mContactPointOn2, ccd_body->mContactPointOn2 - ccd_body->mContactNormal, Color::sYellow, 0.1f);

						// Draw post contact velocity
						DebugRenderer::sInstance->DrawArrow(collision_transform.GetTranslation(), collision_transform.GetTranslation() + body1.GetLinearVelocity(), Color::sOrange, 0.1f);
						DebugRenderer::sInstance->DrawArrow(collision_transform.GetTranslation(), collision_transform.GetTranslation() + body1.GetAngularVelocity(), Color::sPurple, 0.1f);
					}
				#endif // JPH_DEBUG_RENDERER
				}
			}

			// Update body position
			body1.AddPositionStep(ccd_body->mDeltaPosition * ccd_body->mFractionPlusSlop);

			// If the body was activated due to an earlier CCD step it will have an index in the active
			// body list that it higher than the highest one we processed during FindCollisions
			// which means it hasn't been assigned an island and will not be updated by an island
			// this means that we need to update its bounds manually
			if (body_mp->GetIndexInActiveBodiesInternal() >= num_active_bodies_after_find_collisions)
			{
				body1.CalculateWorldSpaceBoundsInternal();
				bodies_to_update_bounds[num_bodies_to_update_bounds++] = body1.GetID();
				if (num_bodies_to_update_bounds == cBodiesBatch)
				{
					// Buffer full, flush now
					mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);
					num_bodies_to_update_bounds = 0;
				}
			}
		}

		// Activate the requested bodies
		if (num_bodies_to_activate > 0)
			mBodyManager.ActivateBodies(bodies_to_activate, num_bodies_to_activate);

		// Notify change bounds on requested bodies
		if (num_bodies_to_update_bounds > 0)
			mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);
	}

	// Ensure we free the CCD bodies array now, will not call the destructor!
	temp_allocator->Free(ioStep->mActiveBodyToCCDBody, ioStep->mNumActiveBodyToCCDBody * sizeof(int));
	ioStep->mActiveBodyToCCDBody = nullptr;
	ioStep->mNumActiveBodyToCCDBody = 0;
	temp_allocator->Free(ioStep->mCCDBodies, ioStep->mCCDBodiesCapacity * sizeof(CCDBody));
	ioStep->mCCDBodies = nullptr;
	ioStep->mCCDBodiesCapacity = 0;
}

void PhysicsSystem::JobContactRemovedCallbacks(const PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We don't touch any bodies
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::None);
#endif

	// Reset the Body::EFlags::InvalidateContactCache flag for all bodies
	mBodyManager.ValidateContactCacheForAllBodies();

	// Finalize the contact cache (this swaps the read and write versions of the contact cache)
	// Trigger all contact removed callbacks by looking at last step contact points that have not been flagged as reused
	mContactManager.FinalizeContactCacheAndCallContactPointRemovedCallbacks(ioStep->mNumBodyPairs, ioStep->mNumManifolds);
}

class PhysicsSystem::BodiesToSleep : public NonCopyable
{
public:
	static constexpr int	cBodiesToSleepSize = 512;
	static constexpr int	cMaxBodiesToPutInBuffer = 128;

	inline					BodiesToSleep(BodyManager &inBodyManager, BodyID *inBodiesToSleepBuffer) : mBodyManager(inBodyManager), mBodiesToSleepBuffer(inBodiesToSleepBuffer), mBodiesToSleepCur(inBodiesToSleepBuffer) { }

	inline					~BodiesToSleep()
	{
		// Flush the bodies to sleep buffer
		int num_bodies_in_buffer = int(mBodiesToSleepCur - mBodiesToSleepBuffer);
		if (num_bodies_in_buffer > 0)
			mBodyManager.DeactivateBodies(mBodiesToSleepBuffer, num_bodies_in_buffer);
	}

	inline void				PutToSleep(const BodyID *inBegin, const BodyID *inEnd)
	{
		int num_bodies_to_sleep = int(inEnd - inBegin);
		if (num_bodies_to_sleep > cMaxBodiesToPutInBuffer)
		{
			// Too many bodies, deactivate immediately
			mBodyManager.DeactivateBodies(inBegin, num_bodies_to_sleep);
		}
		else
		{
			// Check if there's enough space in the bodies to sleep buffer
			int num_bodies_in_buffer = int(mBodiesToSleepCur - mBodiesToSleepBuffer);
			if (num_bodies_in_buffer + num_bodies_to_sleep > cBodiesToSleepSize)
			{
				// Flush the bodies to sleep buffer
				mBodyManager.DeactivateBodies(mBodiesToSleepBuffer, num_bodies_in_buffer);
				mBodiesToSleepCur = mBodiesToSleepBuffer;
			}

			// Copy the bodies in the buffer
			memcpy(mBodiesToSleepCur, inBegin, num_bodies_to_sleep * sizeof(BodyID));
			mBodiesToSleepCur += num_bodies_to_sleep;
		}
	}

private:
	BodyManager &			mBodyManager;
	BodyID *				mBodiesToSleepBuffer;
	BodyID *				mBodiesToSleepCur;
};

void PhysicsSystem::CheckSleepAndUpdateBounds(uint32 inIslandIndex, const PhysicsUpdateContext *ioContext, const PhysicsUpdateContext::Step *ioStep, BodiesToSleep &ioBodiesToSleep)
{
	// Get the bodies that belong to this island
	BodyID *bodies_begin, *bodies_end;
	mIslandBuilder.GetBodiesInIsland(inIslandIndex, bodies_begin, bodies_end);

	// Only check sleeping in the last step
	// Also resets force and torque used during the apply gravity phase
	if (ioStep->mIsLast)
	{
		JPH_PROFILE("Check Sleeping");

		static_assert(int(ECanSleep::CannotSleep) == 0 && int(ECanSleep::CanSleep) == 1, "Loop below makes this assumption");
		int all_can_sleep = mPhysicsSettings.mAllowSleeping? int(ECanSleep::CanSleep) : int(ECanSleep::CannotSleep);

		float time_before_sleep = mPhysicsSettings.mTimeBeforeSleep;
		float max_movement = mPhysicsSettings.mPointVelocitySleepThreshold * time_before_sleep;

		for (const BodyID *body_id = bodies_begin; body_id < bodies_end; ++body_id)
		{
			Body &body = mBodyManager.GetBody(*body_id);

			// Update bounding box
			body.CalculateWorldSpaceBoundsInternal();

			// Update sleeping
			all_can_sleep &= int(body.UpdateSleepStateInternal(ioContext->mStepDeltaTime, max_movement, time_before_sleep));

			// Reset force and torque
			MotionProperties *mp = body.GetMotionProperties();
			mp->ResetForce();
			mp->ResetTorque();
		}

		// If all bodies indicate they can sleep we can deactivate them
		if (all_can_sleep == int(ECanSleep::CanSleep))
			ioBodiesToSleep.PutToSleep(bodies_begin, bodies_end);
	}
	else
	{
		JPH_PROFILE("Update Bounds");

		// Update bounding box only for all other steps
		for (const BodyID *body_id = bodies_begin; body_id < bodies_end; ++body_id)
		{
			Body &body = mBodyManager.GetBody(*body_id);
			body.CalculateWorldSpaceBoundsInternal();
		}
	}

	// Notify broadphase of changed objects (find ccd contacts can do linear casts in the next step, so we need to do this every step)
	// Note: Shuffles the BodyID's around!!!
	mBroadPhase->NotifyBodiesAABBChanged(bodies_begin, int(bodies_end - bodies_begin), false);
}

void PhysicsSystem::JobSolvePositionConstraints(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
#ifdef JPH_ENABLE_ASSERTS
	// We fix up position errors
	BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::ReadWrite);

	// Can only deactivate bodies
	BodyManager::GrantActiveBodiesAccess grant_active(false, true);
#endif

	float delta_time = ioContext->mStepDeltaTime;
	float baumgarte = mPhysicsSettings.mBaumgarte;
	Constraint **active_constraints = ioContext->mActiveConstraints;

	// Keep a buffer of bodies that need to go to sleep in order to not constantly lock the active bodies mutex and create contention between all solving threads
	BodiesToSleep bodies_to_sleep(mBodyManager, (BodyID *)JPH_STACK_ALLOC(BodiesToSleep::cBodiesToSleepSize * sizeof(BodyID)));

	bool check_islands = true, check_split_islands = mPhysicsSettings.mUseLargeIslandSplitter;
	for (;;)
	{
		// First try to get work from large islands
		if (check_split_islands)
		{
			bool first_iteration;
			uint split_island_index;
			uint32 *constraints_begin, *constraints_end, *contacts_begin, *contacts_end;
			switch (mLargeIslandSplitter.FetchNextBatch(split_island_index, constraints_begin, constraints_end, contacts_begin, contacts_end, first_iteration))
			{
			case LargeIslandSplitter::EStatus::BatchRetrieved:
				// Solve the batch
				ConstraintManager::sSolvePositionConstraints(active_constraints, constraints_begin, constraints_end, delta_time, baumgarte);
				mContactManager.SolvePositionConstraints(contacts_begin, contacts_end);

				// Mark the batch as processed
				bool last_iteration, final_batch;
				mLargeIslandSplitter.MarkBatchProcessed(split_island_index, constraints_begin, constraints_end, contacts_begin, contacts_end, last_iteration, final_batch);

				// The final batch will update all bounds and check sleeping
				if (final_batch)
					CheckSleepAndUpdateBounds(mLargeIslandSplitter.GetIslandIndex(split_island_index), ioContext, ioStep, bodies_to_sleep);

				// We processed work, loop again
				continue;

			case LargeIslandSplitter::EStatus::WaitingForBatch:
				break;

			case LargeIslandSplitter::EStatus::AllBatchesDone:
				check_split_islands = false;
				break;
			}
		}

		// If that didn't succeed try to process an island
		if (check_islands)
		{
			// Next island
			uint32 island_idx = ioStep->mSolvePositionConstraintsNextIsland++;
			if (island_idx >= mIslandBuilder.GetNumIslands())
			{
				// We processed all islands, stop checking islands
				check_islands = false;
				continue;
			}

			JPH_PROFILE("Island");

			// Get iterators for this island
			uint32 *constraints_begin, *constraints_end, *contacts_begin, *contacts_end;
			mIslandBuilder.GetConstraintsInIsland(island_idx, constraints_begin, constraints_end);
			mIslandBuilder.GetContactsInIsland(island_idx, contacts_begin, contacts_end);

			// If this island is a large island, it will be picked up as a batch and we don't need to do anything here
			uint num_items = uint(constraints_end - constraints_begin) + uint(contacts_end - contacts_begin);
			if (mPhysicsSettings.mUseLargeIslandSplitter
				&& num_items >= LargeIslandSplitter::cLargeIslandTreshold)
				continue;

			// Check if this island needs solving
			if (num_items > 0)
			{
				// Iterate
				uint num_position_steps = mIslandBuilder.GetNumPositionSteps(island_idx);
				for (uint position_step = 0; position_step < num_position_steps; ++position_step)
				{
					bool applied_impulse = ConstraintManager::sSolvePositionConstraints(active_constraints, constraints_begin, constraints_end, delta_time, baumgarte);
					applied_impulse |= mContactManager.SolvePositionConstraints(contacts_begin, contacts_end);
					if (!applied_impulse)
						break;
				}
			}

			// After solving we will update all bounds and check sleeping
			CheckSleepAndUpdateBounds(island_idx, ioContext, ioStep, bodies_to_sleep);

			// We processed work, loop again
			continue;
		}

		if (check_islands)
		{
			// If there are islands, we don't need to wait and can pick up new work
			continue;
		}
		else if (check_split_islands)
		{
			// If there are split islands, but we did't do any work, give up a time slice
			std::this_thread::yield();
		}
		else
		{
			// No more work
			break;
		}
	}
}

void PhysicsSystem::JobSoftBodyPrepare(PhysicsUpdateContext *ioContext, PhysicsUpdateContext::Step *ioStep)
{
	JPH_PROFILE_FUNCTION();

	{
	#ifdef JPH_ENABLE_ASSERTS
		// Reading soft body positions
		BodyAccess::Grant grant(BodyAccess::EAccess::None, BodyAccess::EAccess::Read);
	#endif

		// Get the active soft bodies
		BodyIDVector active_bodies;
		mBodyManager.GetActiveBodies(EBodyType::SoftBody, active_bodies);

		// Quit if there are no active soft bodies
		if (active_bodies.empty())
		{
			// Kick the next step
			if (ioStep->mStartNextStep.IsValid())
				ioStep->mStartNextStep.RemoveDependency();
			return;
		}

		// Sort to get a deterministic update order
		QuickSort(active_bodies.begin(), active_bodies.end());

		// Allocate soft body contexts
		ioContext->mNumSoftBodies = (uint)active_bodies.size();
		ioContext->mSoftBodyUpdateContexts = (SoftBodyUpdateContext *)ioContext->mTempAllocator->Allocate(ioContext->mNumSoftBodies * sizeof(SoftBodyUpdateContext));

		// Initialize soft body contexts
		for (SoftBodyUpdateContext *sb_ctx = ioContext->mSoftBodyUpdateContexts, *sb_ctx_end = ioContext->mSoftBodyUpdateContexts + ioContext->mNumSoftBodies; sb_ctx < sb_ctx_end; ++sb_ctx)
		{
			new (sb_ctx) SoftBodyUpdateContext;
			Body &body = mBodyManager.GetBody(active_bodies[sb_ctx - ioContext->mSoftBodyUpdateContexts]);
			SoftBodyMotionProperties *mp = static_cast<SoftBodyMotionProperties *>(body.GetMotionProperties());
			mp->InitializeUpdateContext(ioContext->mStepDeltaTime, body, *this, *sb_ctx);
		}
	}

	// We're ready to collide the first soft body
	ioContext->mSoftBodyToCollide.store(0, memory_order_release);

	// Determine number of jobs to spawn
	int num_soft_body_jobs = ioContext->GetMaxConcurrency();

	// Create finalize job
	ioStep->mSoftBodyFinalize = ioContext->mJobSystem->CreateJob("SoftBodyFinalize", cColorSoftBodyFinalize, [ioContext, ioStep]()
	{
		ioContext->mPhysicsSystem->JobSoftBodyFinalize(ioContext);

		// Kick the next step
		if (ioStep->mStartNextStep.IsValid())
			ioStep->mStartNextStep.RemoveDependency();
	}, num_soft_body_jobs); // depends on: soft body simulate
	ioContext->mBarrier->AddJob(ioStep->mSoftBodyFinalize);

	// Create simulate jobs
	ioStep->mSoftBodySimulate.resize(num_soft_body_jobs);
	for (int i = 0; i < num_soft_body_jobs; ++i)
		ioStep->mSoftBodySimulate[i] = ioContext->mJobSystem->CreateJob("SoftBodySimulate", cColorSoftBodySimulate, [ioStep, i]()
			{
				ioStep->mContext->mPhysicsSystem->JobSoftBodySimulate(ioStep->mContext, i);

				ioStep->mSoftBodyFinalize.RemoveDependency();
			}, num_soft_body_jobs); // depends on: soft body collide
	ioContext->mBarrier->AddJobs(ioStep->mSoftBodySimulate.data(), ioStep->mSoftBodySimulate.size());

	// Create collision jobs
	ioStep->mSoftBodyCollide.resize(num_soft_body_jobs);
	for (int i = 0; i < num_soft_body_jobs; ++i)
		ioStep->mSoftBodyCollide[i] = ioContext->mJobSystem->CreateJob("SoftBodyCollide", cColorSoftBodyCollide, [ioContext, ioStep]()
			{
				ioContext->mPhysicsSystem->JobSoftBodyCollide(ioContext);

				for (const JobHandle &h : ioStep->mSoftBodySimulate)
					h.RemoveDependency();
			}); // depends on: nothing
	ioContext->mBarrier->AddJobs(ioStep->mSoftBodyCollide.data(), ioStep->mSoftBodyCollide.size());
}

void PhysicsSystem::JobSoftBodyCollide(PhysicsUpdateContext *ioContext) const
{
#ifdef JPH_ENABLE_ASSERTS
	// Reading rigid body positions and velocities
	BodyAccess::Grant grant(BodyAccess::EAccess::Read, BodyAccess::EAccess::Read);
#endif

	for (;;)
	{
		// Fetch the next soft body
		uint sb_idx = ioContext->mSoftBodyToCollide.fetch_add(1, std::memory_order_acquire);
		if (sb_idx >= ioContext->mNumSoftBodies)
			break;

		// Do a broadphase check
		SoftBodyUpdateContext &sb_ctx = ioContext->mSoftBodyUpdateContexts[sb_idx];
		sb_ctx.mMotionProperties->DetermineCollidingShapes(sb_ctx, *this, GetBodyLockInterfaceNoLock());
	}
}

void PhysicsSystem::JobSoftBodySimulate(PhysicsUpdateContext *ioContext, uint inThreadIndex) const
{
#ifdef JPH_ENABLE_ASSERTS
	// Updating velocities of soft bodies, allow the contact listener to read the soft body state
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::Read);
#endif

	// Calculate at which body we start to distribute the workload across the threads
	uint num_soft_bodies = ioContext->mNumSoftBodies;
	uint start_idx = inThreadIndex * num_soft_bodies / ioContext->GetMaxConcurrency();

	// Keep running partial updates until everything has been updated
	uint status;
	do
	{
		// Reset status
		status = 0;

		// Update all soft bodies
		for (uint i = 0; i < num_soft_bodies; ++i)
		{
			// Fetch the soft body context
			SoftBodyUpdateContext &sb_ctx = ioContext->mSoftBodyUpdateContexts[(start_idx + i) % num_soft_bodies];

			// To avoid trashing the cache too much, we prefer to stick to one soft body until we cannot progress it any further
			uint sb_status;
			do
			{
				sb_status = (uint)sb_ctx.mMotionProperties->ParallelUpdate(sb_ctx, mPhysicsSettings);
				status |= sb_status;
			} while (sb_status == (uint)SoftBodyMotionProperties::EStatus::DidWork);
		}

		// If we didn't perform any work, yield the thread so that something else can run
		if (!(status & (uint)SoftBodyMotionProperties::EStatus::DidWork))
			std::this_thread::yield();
	}
	while (status != (uint)SoftBodyMotionProperties::EStatus::Done);
}

void PhysicsSystem::JobSoftBodyFinalize(PhysicsUpdateContext *ioContext)
{
#ifdef JPH_ENABLE_ASSERTS
	// Updating rigid body velocities and soft body positions / velocities
	BodyAccess::Grant grant(BodyAccess::EAccess::ReadWrite, BodyAccess::EAccess::ReadWrite);

	// Can activate and deactivate bodies
	BodyManager::GrantActiveBodiesAccess grant_active(true, true);
#endif

	static constexpr int cBodiesBatch = 64;
	BodyID *bodies_to_update_bounds = (BodyID *)JPH_STACK_ALLOC(cBodiesBatch * sizeof(BodyID));
	int num_bodies_to_update_bounds = 0;
	BodyID *bodies_to_put_to_sleep = (BodyID *)JPH_STACK_ALLOC(cBodiesBatch * sizeof(BodyID));
	int num_bodies_to_put_to_sleep = 0;

	for (SoftBodyUpdateContext *sb_ctx = ioContext->mSoftBodyUpdateContexts, *sb_ctx_end = ioContext->mSoftBodyUpdateContexts + ioContext->mNumSoftBodies; sb_ctx < sb_ctx_end; ++sb_ctx)
	{
		// Apply the rigid body velocity deltas
		sb_ctx->mMotionProperties->UpdateRigidBodyVelocities(*sb_ctx, GetBodyInterfaceNoLock());

		// Update the position
		sb_ctx->mBody->SetPositionAndRotationInternal(sb_ctx->mBody->GetPosition() + sb_ctx->mDeltaPosition, sb_ctx->mBody->GetRotation(), false);

		BodyID id = sb_ctx->mBody->GetID();
		bodies_to_update_bounds[num_bodies_to_update_bounds++] = id;
		if (num_bodies_to_update_bounds == cBodiesBatch)
		{
			// Buffer full, flush now
			mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);
			num_bodies_to_update_bounds = 0;
		}

		if (sb_ctx->mCanSleep == ECanSleep::CanSleep)
		{
			// This body should go to sleep
			bodies_to_put_to_sleep[num_bodies_to_put_to_sleep++] = id;
			if (num_bodies_to_put_to_sleep == cBodiesBatch)
			{
				mBodyManager.DeactivateBodies(bodies_to_put_to_sleep, num_bodies_to_put_to_sleep);
				num_bodies_to_put_to_sleep = 0;
			}
		}
	}

	// Notify change bounds on requested bodies
	if (num_bodies_to_update_bounds > 0)
		mBroadPhase->NotifyBodiesAABBChanged(bodies_to_update_bounds, num_bodies_to_update_bounds, false);

	// Notify bodies to go to sleep
	if (num_bodies_to_put_to_sleep > 0)
		mBodyManager.DeactivateBodies(bodies_to_put_to_sleep, num_bodies_to_put_to_sleep);

	// Free soft body contexts
	ioContext->mTempAllocator->Free(ioContext->mSoftBodyUpdateContexts, ioContext->mNumSoftBodies * sizeof(SoftBodyUpdateContext));
}

void PhysicsSystem::SaveState(StateRecorder &inStream, EStateRecorderState inState, const StateRecorderFilter *inFilter) const
{
	JPH_PROFILE_FUNCTION();

	inStream.Write(inState);

	if (uint8(inState) & uint8(EStateRecorderState::Global))
	{
		inStream.Write(mPreviousStepDeltaTime);
		inStream.Write(mGravity);
	}

	if (uint8(inState) & uint8(EStateRecorderState::Bodies))
		mBodyManager.SaveState(inStream, inFilter);

	if (uint8(inState) & uint8(EStateRecorderState::Contacts))
		mContactManager.SaveState(inStream, inFilter);

	if (uint8(inState) & uint8(EStateRecorderState::Constraints))
		mConstraintManager.SaveState(inStream, inFilter);
}

bool PhysicsSystem::RestoreState(StateRecorder &inStream, const StateRecorderFilter *inFilter)
{
	JPH_PROFILE_FUNCTION();

	EStateRecorderState state = EStateRecorderState::All; // Set this value for validation. If a partial state is saved, validation will not work anyway.
	inStream.Read(state);

	if (uint8(state) & uint8(EStateRecorderState::Global))
	{
		inStream.Read(mPreviousStepDeltaTime);
		inStream.Read(mGravity);
	}

	if (uint8(state) & uint8(EStateRecorderState::Bodies))
	{
		if (!mBodyManager.RestoreState(inStream))
			return false;

		// Update bounding boxes for all bodies in the broadphase
		if (inStream.IsLastPart())
		{
			Array<BodyID> bodies;
			for (const Body *b : mBodyManager.GetBodies())
				if (BodyManager::sIsValidBodyPointer(b) && b->IsInBroadPhase())
					bodies.push_back(b->GetID());
			if (!bodies.empty())
				mBroadPhase->NotifyBodiesAABBChanged(&bodies[0], (int)bodies.size());
		}
	}

	if (uint8(state) & uint8(EStateRecorderState::Contacts))
	{
		if (!mContactManager.RestoreState(inStream, inFilter))
			return false;
	}

	if (uint8(state) & uint8(EStateRecorderState::Constraints))
	{
		if (!mConstraintManager.RestoreState(inStream))
			return false;
	}

	return true;
}

void PhysicsSystem::SaveBodyState(const Body &inBody, StateRecorder &inStream) const
{
	mBodyManager.SaveBodyState(inBody, inStream);
}

void PhysicsSystem::RestoreBodyState(Body &ioBody, StateRecorder &inStream)
{
	mBodyManager.RestoreBodyState(ioBody, inStream);

	BodyID id = ioBody.GetID();
	mBroadPhase->NotifyBodiesAABBChanged(&id, 1);
}

JPH_NAMESPACE_END
