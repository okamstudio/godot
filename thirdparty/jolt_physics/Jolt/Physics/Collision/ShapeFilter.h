// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

class Shape;
class SubShapeID;

/// Filter class
class ShapeFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~ShapeFilter() = default;

	/// Filter function to determine if we should collide with a shape. Returns true if the filter passes.
	/// This overload is called when the query doesn't have a source shape (e.g. ray cast / collide point)
	/// @param inShape2 Shape we're colliding against
	/// @param inSubShapeIDOfShape2 The sub shape ID that will lead from the root shape to inShape2 (i.e. the shape of mBodyID2)
	virtual bool			ShouldCollide([[maybe_unused]] const Shape *inShape2, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape2) const
	{
		return true;
	}

	/// Filter function to determine if two shapes should collide. Returns true if the filter passes.
	/// This overload is called when querying a shape vs a shape (e.g. collide object / cast object).
	/// It is called at each level of the shape hierarchy, so if you have a compound shape with a box, this function will be called twice.
	/// It will not be called on triangles that are part of another shape, i.e a mesh shape will not trigger a callback per triangle. You can filter out individual triangles in the CollisionCollector::AddHit function by their sub shape ID.
	/// @param inShape1 1st shape that is colliding
	/// @param inSubShapeIDOfShape1 The sub shape ID that will lead from the root shape to inShape1 (i.e. the shape that is used to collide or cast against shape 2)
	/// @param inShape2 2nd shape that is colliding
	/// @param inSubShapeIDOfShape2 The sub shape ID that will lead from the root shape to inShape2 (i.e. the shape of mBodyID2)
	virtual bool			ShouldCollide([[maybe_unused]] const Shape *inShape1, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape1, [[maybe_unused]] const Shape *inShape2, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape2) const
	{
		return true;
	}

	/// Used during NarrowPhase queries and TransformedShape queries. Set to the body ID of inShape2 before calling ShouldCollide.
	/// Provides context to the filter to indicate which body is colliding.
	mutable BodyID			mBodyID2;
};

/// Helper class to reverse the order of the shapes in the ShouldCollide function
class ReversedShapeFilter : public ShapeFilter
{
public:
	/// Constructor
	explicit				ReversedShapeFilter(const ShapeFilter &inFilter) : mFilter(inFilter)
	{
		mBodyID2 = inFilter.mBodyID2;
	}

	virtual bool			ShouldCollide(const Shape *inShape2, const SubShapeID &inSubShapeIDOfShape2) const override
	{
		return mFilter.ShouldCollide(inShape2, inSubShapeIDOfShape2);
	}

	virtual bool			ShouldCollide(const Shape *inShape1, const SubShapeID &inSubShapeIDOfShape1, const Shape *inShape2, const SubShapeID &inSubShapeIDOfShape2) const override
	{
		return mFilter.ShouldCollide(inShape2, inSubShapeIDOfShape2, inShape1, inSubShapeIDOfShape1);
	}

private:
	const ShapeFilter &		mFilter;
};

JPH_NAMESPACE_END
