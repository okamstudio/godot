// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/SimShapeFilter.h>
#include <Jolt/Physics/Body/Body.h>

JPH_NAMESPACE_BEGIN

/// Helper class to forward ShapeFilter calls to a SimShapeFilter
/// INTERNAL CLASS DO NOT USE!
class SimShapeFilterWrapper : public ShapeFilter
{
public:
	/// Constructor
							SimShapeFilterWrapper(const SimShapeFilter *inFilter, const Body *inBody1) :
		mFilter(inFilter),
		mBody1(inBody1)
	{
	}

	/// Forward to the simulation shape filter
	virtual bool			ShouldCollide(const Shape *inShape1, const SubShapeID &inSubShapeIDOfShape1, const Shape *inShape2, const SubShapeID &inSubShapeIDOfShape2) const override
	{
		return mFilter->ShouldCollide(*mBody1, inShape1, inSubShapeIDOfShape1, *mBody2, inShape2, inSubShapeIDOfShape2);
	}

	/// Forward to the simulation shape filter
	virtual bool			ShouldCollide(const Shape *inShape2, const SubShapeID &inSubShapeIDOfShape2) const override
	{
		return mFilter->ShouldCollide(*mBody1, mBody1->GetShape(), SubShapeID(), *mBody2, inShape2, inSubShapeIDOfShape2);
	}

	/// Set the body we're colliding against
	void					SetBody2(const Body *inBody2)
	{
		mBody2 = inBody2;
	}

private:
	const SimShapeFilter *	mFilter;
	const Body *			mBody1;
	const Body *			mBody2;
};

/// In case we don't have a simulation shape filter, we fall back to using a default shape filter that always returns true
/// INTERNAL CLASS DO NOT USE!
union SimShapeFilterWrapperUnion
{
public:
	/// Constructor
							SimShapeFilterWrapperUnion(const SimShapeFilter *inFilter, const Body *inBody1)
	{
		// Dirty trick: if we don't have a filter, placement new a standard ShapeFilter so that we
		// don't have to check for nullptr in the ShouldCollide function
		if (inFilter != nullptr)
			new (&mSimShapeFilterWrapper) SimShapeFilterWrapper(inFilter, inBody1);
		else
			new (&mSimShapeFilterWrapper) ShapeFilter();
	}

	/// Destructor
							~SimShapeFilterWrapperUnion()
	{
		// Doesn't need to be destructed
	}

	/// Accessor
	SimShapeFilterWrapper &	GetSimShapeFilterWrapper()
	{
		return mSimShapeFilterWrapper;
	}

private:
	SimShapeFilterWrapper	mSimShapeFilterWrapper;
	ShapeFilter				mShapeFilter;
};

JPH_NAMESPACE_END
