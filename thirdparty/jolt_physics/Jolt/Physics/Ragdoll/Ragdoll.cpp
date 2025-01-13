// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(RagdollSettings::Part)
{
	JPH_ADD_BASE_CLASS(RagdollSettings::Part, BodyCreationSettings)

	JPH_ADD_ATTRIBUTE(RagdollSettings::Part, mToParent)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(RagdollSettings::AdditionalConstraint)
{
	JPH_ADD_ATTRIBUTE(RagdollSettings::AdditionalConstraint, mBodyIdx)
	JPH_ADD_ATTRIBUTE(RagdollSettings::AdditionalConstraint, mConstraint)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(RagdollSettings)
{
	JPH_ADD_ATTRIBUTE(RagdollSettings, mSkeleton)
	JPH_ADD_ATTRIBUTE(RagdollSettings, mParts)
	JPH_ADD_ATTRIBUTE(RagdollSettings, mAdditionalConstraints)
}

static inline BodyInterface &sGetBodyInterface(PhysicsSystem *inSystem, bool inLockBodies)
{
	return inLockBodies? inSystem->GetBodyInterface() : inSystem->GetBodyInterfaceNoLock();
}

static inline const BodyLockInterface &sGetBodyLockInterface(const PhysicsSystem *inSystem, bool inLockBodies)
{
	return inLockBodies? static_cast<const BodyLockInterface &>(inSystem->GetBodyLockInterface()) : static_cast<const BodyLockInterface &>(inSystem->GetBodyLockInterfaceNoLock());
}

bool RagdollSettings::Stabilize()
{
	// Based on: Stop my Constraints from Blowing Up! - Oliver Strunk (Havok)
	// Do 2 things:
	// 1. Limit the mass ratios between parents and children (slide 16)
	// 2. Increase the inertia of parents so that they're bigger or equal to the sum of their children (slide 34)

	// If we don't have any joints there's nothing to stabilize
	if (mSkeleton->GetJointCount() == 0)
		return true;

	// The skeleton can contain one or more static bodies. We can't modify the mass for those so we start a new stabilization chain for each joint under a static body until we reach the next static body.
	// This array keeps track of which joints have been processed.
	Array<bool> visited;
	visited.resize(mSkeleton->GetJointCount());
	for (size_t v = 0; v < visited.size(); ++v)
	{
		// Mark static bodies as visited so we won't process these
		Part &p = mParts[v];
		bool has_mass_properties = p.HasMassProperties();
		visited[v] = !has_mass_properties;

		if (has_mass_properties && p.mOverrideMassProperties != EOverrideMassProperties::MassAndInertiaProvided)
		{
			// Mass properties not yet calculated, do it now
			p.mMassPropertiesOverride = p.GetMassProperties();
			p.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
		}
	}

	// Find first unvisited part that either has no parent or that has a parent that was visited
	for (int first_idx = 0; first_idx < mSkeleton->GetJointCount(); ++first_idx)
	{
		int first_idx_parent = mSkeleton->GetJoint(first_idx).mParentJointIndex;
		if (!visited[first_idx] && (first_idx_parent == -1 || visited[first_idx_parent]))
		{
			// Find all children of first_idx and their children up to the next static part
			int next_to_process = 0;
			Array<int> indices;
			indices.reserve(mSkeleton->GetJointCount());
			visited[first_idx] = true;
			indices.push_back(first_idx);
			do
			{
				int parent_idx = indices[next_to_process++];
				for (int child_idx = 0; child_idx < mSkeleton->GetJointCount(); ++child_idx)
					if (!visited[child_idx] && mSkeleton->GetJoint(child_idx).mParentJointIndex == parent_idx)
					{
						visited[child_idx] = true;
						indices.push_back(child_idx);
					}
			} while (next_to_process < (int)indices.size());

			// If there's only 1 body, we can't redistribute mass
			if (indices.size() == 1)
				continue;

			const float cMinMassRatio = 0.8f;
			const float cMaxMassRatio = 1.2f;

			// Ensure that the mass ratio from parent to child is within a range
			float total_mass_ratio = 1.0f;
			Array<float> mass_ratios;
			mass_ratios.resize(mSkeleton->GetJointCount());
			mass_ratios[indices[0]] = 1.0f;
			for (int i = 1; i < (int)indices.size(); ++i)
			{
				int child_idx = indices[i];
				int parent_idx = mSkeleton->GetJoint(child_idx).mParentJointIndex;
				float ratio = mParts[child_idx].mMassPropertiesOverride.mMass / mParts[parent_idx].mMassPropertiesOverride.mMass;
				mass_ratios[child_idx] = mass_ratios[parent_idx] * Clamp(ratio, cMinMassRatio, cMaxMassRatio);
				total_mass_ratio += mass_ratios[child_idx];
			}

			// Calculate total mass of this chain
			float total_mass = 0.0f;
			for (int idx : indices)
				total_mass += mParts[idx].mMassPropertiesOverride.mMass;

			// Calculate how much mass belongs to a ratio of 1
			float ratio_to_mass = total_mass / total_mass_ratio;

			// Adjust all masses and inertia tensors for the new mass
			for (int i : indices)
			{
				Part &p = mParts[i];
				float old_mass = p.mMassPropertiesOverride.mMass;
				float new_mass = mass_ratios[i] * ratio_to_mass;
				p.mMassPropertiesOverride.mMass = new_mass;
				p.mMassPropertiesOverride.mInertia *= new_mass / old_mass;
				p.mMassPropertiesOverride.mInertia.SetColumn4(3, Vec4(0, 0, 0, 1));
			}

			const float cMaxInertiaIncrease = 2.0f;

			// Get the principal moments of inertia for all parts
			struct Principal
			{
				Mat44	mRotation;
				Vec3	mDiagonal;
				float	mChildSum = 0.0f;
			};
			Array<Principal> principals;
			principals.resize(mParts.size());
			for (int i : indices)
				if (!mParts[i].mMassPropertiesOverride.DecomposePrincipalMomentsOfInertia(principals[i].mRotation, principals[i].mDiagonal))
				{
					JPH_ASSERT(false, "Failed to decompose the inertia tensor!");
					return false;
				}

			// Calculate sum of child inertias
			// Walk backwards so we sum the leaves first
			for (int i = (int)indices.size() - 1; i > 0; --i)
			{
				int child_idx = indices[i];
				int parent_idx = mSkeleton->GetJoint(child_idx).mParentJointIndex;
				principals[parent_idx].mChildSum += principals[child_idx].mDiagonal[0] + principals[child_idx].mChildSum;
			}

			// Adjust inertia tensors for all parts
			for (int i : indices)
			{
				Part &p = mParts[i];
				Principal &principal = principals[i];
				if (principal.mChildSum != 0.0f)
				{
					// Calculate minimum inertia this object should have based on it children
					float minimum = min(cMaxInertiaIncrease * principal.mDiagonal[0], principal.mChildSum);
					principal.mDiagonal = Vec3::sMax(principal.mDiagonal, Vec3::sReplicate(minimum));

					// Recalculate moment of inertia in body space
					p.mMassPropertiesOverride.mInertia = principal.mRotation * Mat44::sScale(principal.mDiagonal) * principal.mRotation.Inversed3x3();
				}
			}
		}
	}

	return true;
}

void RagdollSettings::DisableParentChildCollisions(const Mat44 *inJointMatrices, float inMinSeparationDistance)
{
	int joint_count = mSkeleton->GetJointCount();
	JPH_ASSERT(joint_count == (int)mParts.size());

	// Create a group filter table that disables collisions between parent and child
	Ref<GroupFilterTable> group_filter = new GroupFilterTable(joint_count);
	for (int joint_idx = 0; joint_idx < joint_count; ++joint_idx)
	{
		int parent_joint = mSkeleton->GetJoint(joint_idx).mParentJointIndex;
		if (parent_joint >= 0)
			group_filter->DisableCollision(joint_idx, parent_joint);
	}

	// If joint matrices are provided
	if (inJointMatrices != nullptr)
	{
		// Loop over all joints
		for (int j1 = 0; j1 < joint_count; ++j1)
		{
			// Shape and transform for joint 1
			const Part &part1 = mParts[j1];
			const Shape *shape1 = part1.GetShape();
			Vec3 scale1;
			Mat44 com1 = (inJointMatrices[j1].PreTranslated(shape1->GetCenterOfMass())).Decompose(scale1);

			// Loop over all other joints
			for (int j2 = j1 + 1; j2 < joint_count; ++j2)
				if (group_filter->IsCollisionEnabled(j1, j2)) // Only if collision is still enabled we need to test
				{
					// Shape and transform for joint 2
					const Part &part2 = mParts[j2];
					const Shape *shape2 = part2.GetShape();
					Vec3 scale2;
					Mat44 com2 = (inJointMatrices[j2].PreTranslated(shape2->GetCenterOfMass())).Decompose(scale2);

					// Collision settings
					CollideShapeSettings settings;
					settings.mActiveEdgeMode = EActiveEdgeMode::CollideWithAll;
					settings.mBackFaceMode = EBackFaceMode::CollideWithBackFaces;
					settings.mMaxSeparationDistance = inMinSeparationDistance;

					// Only check if one of the two bodies can become dynamic
					if (part1.HasMassProperties() || part2.HasMassProperties())
					{
						// If there is a collision, disable the collision between the joints
						AnyHitCollisionCollector<CollideShapeCollector> collector;
						if (part1.HasMassProperties()) // Ensure that the first shape is always a dynamic one (we can't check mesh vs convex but we can check convex vs mesh)
							CollisionDispatch::sCollideShapeVsShape(shape1, shape2, scale1, scale2, com1, com2, SubShapeIDCreator(), SubShapeIDCreator(), settings, collector);
						else
							CollisionDispatch::sCollideShapeVsShape(shape2, shape1, scale2, scale1, com2, com1, SubShapeIDCreator(), SubShapeIDCreator(), settings, collector);
						if (collector.HadHit())
							group_filter->DisableCollision(j1, j2);
					}
				}
		}
	}

	// Loop over the body parts and assign them a sub group ID and the group filter
	for (int joint_idx = 0; joint_idx < joint_count; ++joint_idx)
	{
		Part &part = mParts[joint_idx];
		part.mCollisionGroup.SetSubGroupID(joint_idx);
		part.mCollisionGroup.SetGroupFilter(group_filter);
	}
}

void RagdollSettings::SaveBinaryState(StreamOut &inStream, bool inSaveShapes, bool inSaveGroupFilter) const
{
	BodyCreationSettings::ShapeToIDMap shape_to_id;
	BodyCreationSettings::MaterialToIDMap material_to_id;
	BodyCreationSettings::GroupFilterToIDMap group_filter_to_id;

	// Save skeleton
	mSkeleton->SaveBinaryState(inStream);

	// Save parts
	inStream.Write((uint32)mParts.size());
	for (const Part &p : mParts)
	{
		// Write body creation settings
		p.SaveWithChildren(inStream, inSaveShapes? &shape_to_id : nullptr, inSaveShapes? &material_to_id : nullptr, inSaveGroupFilter? &group_filter_to_id : nullptr);

		// Save constraint
		inStream.Write(p.mToParent != nullptr);
		if (p.mToParent != nullptr)
			p.mToParent->SaveBinaryState(inStream);
	}

	// Save additional constraints
	inStream.Write((uint32)mAdditionalConstraints.size());
	for (const AdditionalConstraint &c : mAdditionalConstraints)
	{
		// Save bodies indices
		inStream.Write(c.mBodyIdx);

		// Save constraint
		c.mConstraint->SaveBinaryState(inStream);
	}
}

RagdollSettings::RagdollResult RagdollSettings::sRestoreFromBinaryState(StreamIn &inStream)
{
	RagdollResult result;

	// Restore skeleton
	Skeleton::SkeletonResult skeleton_result = Skeleton::sRestoreFromBinaryState(inStream);
	if (skeleton_result.HasError())
	{
		result.SetError(skeleton_result.GetError());
		return result;
	}

	// Create ragdoll
	Ref<RagdollSettings> ragdoll = new RagdollSettings();
	ragdoll->mSkeleton = skeleton_result.Get();

	BodyCreationSettings::IDToShapeMap id_to_shape;
	BodyCreationSettings::IDToMaterialMap id_to_material;
	BodyCreationSettings::IDToGroupFilterMap id_to_group_filter;

	// Reserve some memory to avoid frequent reallocations
	id_to_shape.reserve(1024);
	id_to_material.reserve(128);
	id_to_group_filter.reserve(128);

	// Read parts
	uint32 len = 0;
	inStream.Read(len);
	ragdoll->mParts.resize(len);
	for (Part &p : ragdoll->mParts)
	{
		// Read creation settings
		BodyCreationSettings::BCSResult bcs_result = BodyCreationSettings::sRestoreWithChildren(inStream, id_to_shape, id_to_material, id_to_group_filter);
		if (bcs_result.HasError())
		{
			result.SetError(bcs_result.GetError());
			return result;
		}
		static_cast<BodyCreationSettings &>(p) = bcs_result.Get();

		// Read constraint
		bool has_constraint = false;
		inStream.Read(has_constraint);
		if (has_constraint)
		{
			ConstraintSettings::ConstraintResult constraint_result = ConstraintSettings::sRestoreFromBinaryState(inStream);
			if (constraint_result.HasError())
			{
				result.SetError(constraint_result.GetError());
				return result;
			}
			p.mToParent = DynamicCast<TwoBodyConstraintSettings>(constraint_result.Get());
		}
	}

	// Read additional constraints
	len = 0;
	inStream.Read(len);
	ragdoll->mAdditionalConstraints.resize(len);
	for (AdditionalConstraint &c : ragdoll->mAdditionalConstraints)
	{
		// Read body indices
		inStream.Read(c.mBodyIdx);

		// Read constraint
		ConstraintSettings::ConstraintResult constraint_result = ConstraintSettings::sRestoreFromBinaryState(inStream);
		if (constraint_result.HasError())
		{
			result.SetError(constraint_result.GetError());
			return result;
		}
		c.mConstraint = DynamicCast<TwoBodyConstraintSettings>(constraint_result.Get());
	}

	// Create mapping tables
	ragdoll->CalculateBodyIndexToConstraintIndex();
	ragdoll->CalculateConstraintIndexToBodyIdxPair();

	result.Set(ragdoll);
	return result;
}

Ragdoll *RagdollSettings::CreateRagdoll(CollisionGroup::GroupID inCollisionGroup, uint64 inUserData, PhysicsSystem *inSystem) const
{
	Ragdoll *r = new Ragdoll(inSystem);
	r->mRagdollSettings = this;
	r->mBodyIDs.reserve(mParts.size());
	r->mConstraints.reserve(mParts.size() + mAdditionalConstraints.size());

	// Create bodies and constraints
	BodyInterface &bi = inSystem->GetBodyInterface();
	Body **bodies = (Body **)JPH_STACK_ALLOC(mParts.size() * sizeof(Body *));
	int joint_idx = 0;
	for (const Part &p : mParts)
	{
		Body *body2 = bi.CreateBody(p);
		if (body2 == nullptr)
		{
			// Out of bodies, failed to create ragdoll
			delete r;
			return nullptr;
		}
		body2->GetCollisionGroup().SetGroupID(inCollisionGroup);
		body2->SetUserData(inUserData);

		// Temporarily store body pointer for hooking up constraints
		bodies[joint_idx] = body2;

		// Create constraint
		if (p.mToParent != nullptr)
		{
			Body *body1 = bodies[mSkeleton->GetJoint(joint_idx).mParentJointIndex];
			r->mConstraints.push_back(p.mToParent->Create(*body1, *body2));
		}

		// Store body ID and constraint in parallel arrays
		r->mBodyIDs.push_back(body2->GetID());

		++joint_idx;
	}

	// Add additional constraints
	for (const AdditionalConstraint &c : mAdditionalConstraints)
	{
		Body *body1 = bodies[c.mBodyIdx[0]];
		Body *body2 = bodies[c.mBodyIdx[1]];
		r->mConstraints.push_back(c.mConstraint->Create(*body1, *body2));
	}

	return r;
}

void RagdollSettings::CalculateBodyIndexToConstraintIndex()
{
	mBodyIndexToConstraintIndex.clear();
	mBodyIndexToConstraintIndex.reserve(mParts.size());

	int constraint_index = 0;
	for (const Part &p : mParts)
	{
		if (p.mToParent != nullptr)
			mBodyIndexToConstraintIndex.push_back(constraint_index++);
		else
			mBodyIndexToConstraintIndex.push_back(-1);
	}
}

void RagdollSettings::CalculateConstraintIndexToBodyIdxPair()
{
	mConstraintIndexToBodyIdxPair.clear();
	mConstraintIndexToBodyIdxPair.reserve(mParts.size() + mAdditionalConstraints.size());

	// Add constraints between parts
	int joint_idx = 0;
	for (const Part &p : mParts)
	{
		if (p.mToParent != nullptr)
		{
			int parent_joint_idx = mSkeleton->GetJoint(joint_idx).mParentJointIndex;
			mConstraintIndexToBodyIdxPair.emplace_back(parent_joint_idx, joint_idx);
		}

		++joint_idx;
	}

	// Add additional constraints
	for (const AdditionalConstraint &c : mAdditionalConstraints)
		mConstraintIndexToBodyIdxPair.emplace_back(c.mBodyIdx[0], c.mBodyIdx[1]);
}

Ragdoll::~Ragdoll()
{
	// Destroy all bodies
	mSystem->GetBodyInterface().DestroyBodies(mBodyIDs.data(), (int)mBodyIDs.size());
}

void Ragdoll::AddToPhysicsSystem(EActivation inActivationMode, bool inLockBodies)
{
	// Scope for JPH_STACK_ALLOC
	{
		// Create copy of body ids since they will be shuffled
		int num_bodies = (int)mBodyIDs.size();
		BodyID *bodies = (BodyID *)JPH_STACK_ALLOC(num_bodies * sizeof(BodyID));
		memcpy(bodies, mBodyIDs.data(), num_bodies * sizeof(BodyID));

		// Insert bodies as a batch
		BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
		BodyInterface::AddState add_state = bi.AddBodiesPrepare(bodies, num_bodies);
		bi.AddBodiesFinalize(bodies, num_bodies, add_state, inActivationMode);
	}

	// Add all constraints
	mSystem->AddConstraints((Constraint **)mConstraints.data(), (int)mConstraints.size());
}

void Ragdoll::RemoveFromPhysicsSystem(bool inLockBodies)
{
	// Remove all constraints before removing the bodies
	mSystem->RemoveConstraints((Constraint **)mConstraints.data(), (int)mConstraints.size());

	// Scope for JPH_STACK_ALLOC
	{
		// Create copy of body ids since they will be shuffled
		int num_bodies = (int)mBodyIDs.size();
		BodyID *bodies = (BodyID *)JPH_STACK_ALLOC(num_bodies * sizeof(BodyID));
		memcpy(bodies, mBodyIDs.data(), num_bodies * sizeof(BodyID));

		// Remove all bodies as a batch
		sGetBodyInterface(mSystem, inLockBodies).RemoveBodies(bodies, num_bodies);
	}
}

void Ragdoll::Activate(bool inLockBodies)
{
	sGetBodyInterface(mSystem, inLockBodies).ActivateBodies(mBodyIDs.data(), (int)mBodyIDs.size());
}

bool Ragdoll::IsActive(bool inLockBodies) const
{
	// Lock the bodies
	int body_count = (int)mBodyIDs.size();
	BodyLockMultiRead lock(sGetBodyLockInterface(mSystem, inLockBodies), mBodyIDs.data(), body_count);

	// Test if any body is active
	for (int b = 0; b < body_count; ++b)
	{
		const Body *body = lock.GetBody(b);
		if (body->IsActive())
			return true;
	}

	return false;
}

void Ragdoll::SetGroupID(CollisionGroup::GroupID inGroupID, bool inLockBodies)
{
	// Lock the bodies
	int body_count = (int)mBodyIDs.size();
	BodyLockMultiWrite lock(sGetBodyLockInterface(mSystem, inLockBodies), mBodyIDs.data(), body_count);

	// Update group ID
	for (int b = 0; b < body_count; ++b)
	{
		Body *body = lock.GetBody(b);
		body->GetCollisionGroup().SetGroupID(inGroupID);
	}
}

void Ragdoll::SetPose(const SkeletonPose &inPose, bool inLockBodies)
{
	JPH_ASSERT(inPose.GetSkeleton() == mRagdollSettings->mSkeleton);

	SetPose(inPose.GetRootOffset(), inPose.GetJointMatrices().data(), inLockBodies);
}

void Ragdoll::SetPose(RVec3Arg inRootOffset, const Mat44 *inJointMatrices, bool inLockBodies)
{
	// Move bodies instantly into the correct position
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (int i = 0; i < (int)mBodyIDs.size(); ++i)
	{
		const Mat44 &joint = inJointMatrices[i];
		bi.SetPositionAndRotation(mBodyIDs[i], inRootOffset + joint.GetTranslation(), joint.GetQuaternion(), EActivation::DontActivate);
	}
}

void Ragdoll::GetPose(SkeletonPose &outPose, bool inLockBodies)
{
	JPH_ASSERT(outPose.GetSkeleton() == mRagdollSettings->mSkeleton);

	RVec3 root_offset;
	GetPose(root_offset, outPose.GetJointMatrices().data(), inLockBodies);
	outPose.SetRootOffset(root_offset);
}

void Ragdoll::GetPose(RVec3 &outRootOffset, Mat44 *outJointMatrices, bool inLockBodies)
{
	// Lock the bodies
	int body_count = (int)mBodyIDs.size();
	if (body_count == 0)
		return;
	BodyLockMultiRead lock(sGetBodyLockInterface(mSystem, inLockBodies), mBodyIDs.data(), body_count);

	// Get root matrix
	const Body *root = lock.GetBody(0);
	RMat44 root_transform = root->GetWorldTransform();
	outRootOffset = root_transform.GetTranslation();
	outJointMatrices[0] = Mat44(root_transform.GetColumn4(0), root_transform.GetColumn4(1), root_transform.GetColumn4(2), Vec4(0, 0, 0, 1));

	// Get other matrices
	for (int b = 1; b < body_count; ++b)
	{
		const Body *body = lock.GetBody(b);
		RMat44 transform = body->GetWorldTransform();
		outJointMatrices[b] = Mat44(transform.GetColumn4(0), transform.GetColumn4(1), transform.GetColumn4(2), Vec4(Vec3(transform.GetTranslation() - outRootOffset), 1));
	}
}

void Ragdoll::ResetWarmStart()
{
	for (TwoBodyConstraint *c : mConstraints)
		c->ResetWarmStart();
}

void Ragdoll::DriveToPoseUsingKinematics(const SkeletonPose &inPose, float inDeltaTime, bool inLockBodies)
{
	JPH_ASSERT(inPose.GetSkeleton() == mRagdollSettings->mSkeleton);

	DriveToPoseUsingKinematics(inPose.GetRootOffset(), inPose.GetJointMatrices().data(), inDeltaTime, inLockBodies);
}

void Ragdoll::DriveToPoseUsingKinematics(RVec3Arg inRootOffset, const Mat44 *inJointMatrices, float inDeltaTime, bool inLockBodies)
{
	// Move bodies into the correct position using kinematics
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (int i = 0; i < (int)mBodyIDs.size(); ++i)
	{
		const Mat44 &joint = inJointMatrices[i];
		bi.MoveKinematic(mBodyIDs[i], inRootOffset + joint.GetTranslation(), joint.GetQuaternion(), inDeltaTime);
	}
}

void Ragdoll::DriveToPoseUsingMotors(const SkeletonPose &inPose)
{
	JPH_ASSERT(inPose.GetSkeleton() == mRagdollSettings->mSkeleton);

	// Move bodies into the correct position using constraints
	for (int i = 0; i < (int)inPose.GetJointMatrices().size(); ++i)
	{
		int constraint_idx = mRagdollSettings->GetConstraintIndexForBodyIndex(i);
		if (constraint_idx >= 0)
		{
			// Get desired rotation of this body relative to its parent
			const SkeletalAnimation::JointState &joint_state = inPose.GetJoint(i);

			// Drive constraint to target
			TwoBodyConstraint *constraint = mConstraints[constraint_idx];
			EConstraintSubType sub_type = constraint->GetSubType();
			if (sub_type == EConstraintSubType::SwingTwist)
			{
				SwingTwistConstraint *st_constraint = static_cast<SwingTwistConstraint *>(constraint);
				st_constraint->SetSwingMotorState(EMotorState::Position);
				st_constraint->SetTwistMotorState(EMotorState::Position);
				st_constraint->SetTargetOrientationBS(joint_state.mRotation);
			}
			else
				JPH_ASSERT(false, "Constraint type not implemented!");
		}
	}
}

void Ragdoll::SetLinearAndAngularVelocity(Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, bool inLockBodies)
{
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (BodyID body_id : mBodyIDs)
		bi.SetLinearAndAngularVelocity(body_id, inLinearVelocity, inAngularVelocity);
}

void Ragdoll::SetLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies)
{
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (BodyID body_id : mBodyIDs)
		bi.SetLinearVelocity(body_id, inLinearVelocity);
}

void Ragdoll::AddLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies)
{
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (BodyID body_id : mBodyIDs)
		bi.AddLinearVelocity(body_id, inLinearVelocity);
}

void Ragdoll::AddImpulse(Vec3Arg inImpulse, bool inLockBodies)
{
	BodyInterface &bi = sGetBodyInterface(mSystem, inLockBodies);
	for (BodyID body_id : mBodyIDs)
		bi.AddImpulse(body_id, inImpulse);
}

void Ragdoll::GetRootTransform(RVec3 &outPosition, Quat &outRotation, bool inLockBodies) const
{
	BodyLockRead lock(sGetBodyLockInterface(mSystem, inLockBodies), mBodyIDs[0]);
	if (lock.Succeeded())
	{
		const Body &body = lock.GetBody();
		outPosition = body.GetPosition();
		outRotation = body.GetRotation();
	}
	else
	{
		outPosition = RVec3::sZero();
		outRotation = Quat::sIdentity();
	}
}

AABox Ragdoll::GetWorldSpaceBounds(bool inLockBodies) const
{
	// Lock the bodies
	int body_count = (int)mBodyIDs.size();
	BodyLockMultiRead lock(sGetBodyLockInterface(mSystem, inLockBodies), mBodyIDs.data(), body_count);

	// Encapsulate all bodies
	AABox bounds;
	for (int b = 0; b < body_count; ++b)
	{
		const Body *body = lock.GetBody(b);
		if (body != nullptr)
			bounds.Encapsulate(body->GetWorldSpaceBounds());
	}
	return bounds;
}

JPH_NAMESPACE_END
