// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/SoftBody/SoftBodyVertex.h>
#include <Jolt/Core/StridedPtr.h>

JPH_NAMESPACE_BEGIN

/// Class that allows iterating over the vertices of a soft body.
/// It tracks the largest penetration and allows storing the resulting collision in a different structure than the soft body vertex itself.
class CollideSoftBodyVertexIterator
{
public:
	/// Default constructor
									CollideSoftBodyVertexIterator() = default;
									CollideSoftBodyVertexIterator(const CollideSoftBodyVertexIterator &) = default;

	/// Construct using (strided) pointers
									CollideSoftBodyVertexIterator(const StridedPtr<const Vec3> &inPosition, const StridedPtr<const float> &inInvMass, const StridedPtr<Plane> &inCollisionPlane, const StridedPtr<float> &inLargestPenetration, const StridedPtr<int> &inCollidingShapeIndex) :
		mPosition(inPosition),
		mInvMass(inInvMass),
		mCollisionPlane(inCollisionPlane),
		mLargestPenetration(inLargestPenetration),
		mCollidingShapeIndex(inCollidingShapeIndex)
	{
	}

	/// Construct using a soft body vertex
	explicit						CollideSoftBodyVertexIterator(SoftBodyVertex *inVertices) :
		mPosition(&inVertices->mPosition, sizeof(SoftBodyVertex)),
		mInvMass(&inVertices->mInvMass, sizeof(SoftBodyVertex)),
		mCollisionPlane(&inVertices->mCollisionPlane, sizeof(SoftBodyVertex)),
		mLargestPenetration(&inVertices->mLargestPenetration, sizeof(SoftBodyVertex)),
		mCollidingShapeIndex(&inVertices->mCollidingShapeIndex, sizeof(SoftBodyVertex))
	{
	}

	/// Default assignment
	CollideSoftBodyVertexIterator &	operator = (const CollideSoftBodyVertexIterator &) = default;

	/// Equality operator.
	/// Note: Only used to determine end iterator, so we only compare position.
	bool							operator != (const CollideSoftBodyVertexIterator &inRHS) const
	{
		return mPosition != inRHS.mPosition;
	}

	/// Next vertex
	CollideSoftBodyVertexIterator &	operator ++ ()
	{
		++mPosition;
		++mInvMass;
		++mCollisionPlane;
		++mLargestPenetration;
		++mCollidingShapeIndex;
		return *this;
	}

	/// Add an offset
	/// Note: Only used to determine end iterator, so we only set position.
	CollideSoftBodyVertexIterator	operator + (int inOffset) const
	{
		return CollideSoftBodyVertexIterator(mPosition + inOffset, StridedPtr<const float>(), StridedPtr<Plane>(), StridedPtr<float>(), StridedPtr<int>());
	}

	/// Get the position of the current vertex
	Vec3							GetPosition() const
	{
		return *mPosition;
	}

	/// Get the inverse mass of the current vertex
	float							GetInvMass() const
	{
		return *mInvMass;
	}

	/// Update penetration of the current vertex
	/// @return Returns true if the vertex has the largest penetration so far, this means you need to follow up by calling SetCollision
	bool							UpdatePenetration(float inLargestPenetration) const
	{
		float &penetration = *mLargestPenetration;
		if (penetration >= inLargestPenetration)
			return false;
		penetration = inLargestPenetration;
		return true;
	}

	/// Update the collision of the current vertex
	void							SetCollision(const Plane &inCollisionPlane, int inCollidingShapeIndex) const
	{
		*mCollisionPlane = inCollisionPlane;
		*mCollidingShapeIndex = inCollidingShapeIndex;
	}

private:
	/// Input data
	StridedPtr<const Vec3>			mPosition;
	StridedPtr<const float>			mInvMass;

	/// Output data
	StridedPtr<Plane>				mCollisionPlane;
	StridedPtr<float>				mLargestPenetration;
	StridedPtr<int>					mCollidingShapeIndex;
};

JPH_NAMESPACE_END
