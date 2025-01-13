// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>

JPH_NAMESPACE_BEGIN

class Body;
class Shape;
class SubShapeID;

/// Filter class used during the simulation (PhysicsSystem::Update) to filter out collisions at shape level
class SimShapeFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~SimShapeFilter() = default;

	/// Filter function to determine if two shapes should collide. Returns true if the filter passes.
	/// This overload is called during the simulation (PhysicsSystem::Update) and must be registered with PhysicsSystem::SetSimShapeFilter.
	/// It is called at each level of the shape hierarchy, so if you have a compound shape with a box, this function will be called twice.
	/// It will not be called on triangles that are part of another shape, i.e a mesh shape will not trigger a callback per triangle.
	/// Note that this function is called from multiple threads and must be thread safe. All properties are read only.
	/// @param inBody1 1st body that is colliding
	/// @param inShape1 1st shape that is colliding
	/// @param inSubShapeIDOfShape1 The sub shape ID that will lead from inBody1.GetShape() to inShape1
	/// @param inBody2 2nd body that is colliding
	/// @param inShape2 2nd shape that is colliding
	/// @param inSubShapeIDOfShape2 The sub shape ID that will lead from inBody2.GetShape() to inShape2
	virtual bool			ShouldCollide([[maybe_unused]] const Body &inBody1, [[maybe_unused]] const Shape *inShape1, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape1,
										  [[maybe_unused]] const Body &inBody2, [[maybe_unused]] const Shape *inShape2, [[maybe_unused]] const SubShapeID &inSubShapeIDOfShape2) const
	{
		return true;
	}
};

JPH_NAMESPACE_END
