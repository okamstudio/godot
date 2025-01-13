// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShapeVisitors.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/ScopeExit.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(StaticCompoundShapeSettings)
{
	JPH_ADD_BASE_CLASS(StaticCompoundShapeSettings, CompoundShapeSettings)
}

ShapeSettings::ShapeResult StaticCompoundShapeSettings::Create(TempAllocator &inTempAllocator) const
{
	if (mCachedResult.IsEmpty())
	{
		if (mSubShapes.size() == 0)
		{
			// It's an error to create a compound with no subshapes (the compound cannot encode this)
			mCachedResult.SetError("Compound needs a sub shape!");
		}
		else if (mSubShapes.size() == 1)
		{
			// If there's only 1 part we don't need a StaticCompoundShape
			const SubShapeSettings &s = mSubShapes[0];
			if (s.mPosition == Vec3::sZero()
				&& s.mRotation == Quat::sIdentity())
			{
				// No rotation or translation, we can use the shape directly
				if (s.mShapePtr != nullptr)
					mCachedResult.Set(const_cast<Shape *>(s.mShapePtr.GetPtr()));
				else if (s.mShape != nullptr)
					mCachedResult = s.mShape->Create();
				else
					mCachedResult.SetError("Sub shape is null!");
			}
			else
			{
				// We can use a RotatedTranslatedShape instead
				RotatedTranslatedShapeSettings settings;
				settings.mPosition = s.mPosition;
				settings.mRotation = s.mRotation;
				settings.mInnerShape = s.mShape;
				settings.mInnerShapePtr = s.mShapePtr;
				Ref<Shape> shape = new RotatedTranslatedShape(settings, mCachedResult);
			}
		}
		else
		{
			// Build a regular compound shape
			Ref<Shape> shape = new StaticCompoundShape(*this, inTempAllocator, mCachedResult);
		}
	}
	return mCachedResult;
}

ShapeSettings::ShapeResult StaticCompoundShapeSettings::Create() const
{
	TempAllocatorMalloc allocator;
	return Create(allocator);
}

void StaticCompoundShape::Node::SetChildInvalid(uint inIndex)
{
	// Make this an invalid node
	mNodeProperties[inIndex] = INVALID_NODE;

	// Make bounding box invalid
	mBoundsMinX[inIndex] = HALF_FLT_MAX;
	mBoundsMinY[inIndex] = HALF_FLT_MAX;
	mBoundsMinZ[inIndex] = HALF_FLT_MAX;
	mBoundsMaxX[inIndex] = HALF_FLT_MAX;
	mBoundsMaxY[inIndex] = HALF_FLT_MAX;
	mBoundsMaxZ[inIndex] = HALF_FLT_MAX;
}

void StaticCompoundShape::Node::SetChildBounds(uint inIndex, const AABox &inBounds)
{
	mBoundsMinX[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(inBounds.mMin.GetX());
	mBoundsMinY[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(inBounds.mMin.GetY());
	mBoundsMinZ[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_NEG_INF>(inBounds.mMin.GetZ());
	mBoundsMaxX[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(inBounds.mMax.GetX());
	mBoundsMaxY[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(inBounds.mMax.GetY());
	mBoundsMaxZ[inIndex] = HalfFloatConversion::FromFloat<HalfFloatConversion::ROUND_TO_POS_INF>(inBounds.mMax.GetZ());
}

void StaticCompoundShape::sPartition(uint *ioBodyIdx, AABox *ioBounds, int inNumber, int &outMidPoint)
{
	// Handle trivial case
	if (inNumber <= 4)
	{
		outMidPoint = inNumber / 2;
		return;
	}

	// Calculate bounding box of box centers
	Vec3 center_min = Vec3::sReplicate(FLT_MAX);
	Vec3 center_max = Vec3::sReplicate(-FLT_MAX);
	for (const AABox *b = ioBounds, *b_end = ioBounds + inNumber; b < b_end; ++b)
	{
		Vec3 center = b->GetCenter();
		center_min = Vec3::sMin(center_min, center);
		center_max = Vec3::sMax(center_max, center);
	}

	// Calculate split plane
	int dimension = (center_max - center_min).GetHighestComponentIndex();
	float split = 0.5f * (center_min + center_max)[dimension];

	// Divide bodies
	int start = 0, end = inNumber;
	while (start < end)
	{
		// Search for first element that is on the right hand side of the split plane
		while (start < end && ioBounds[start].GetCenter()[dimension] < split)
			++start;

		// Search for the first element that is on the left hand side of the split plane
		while (start < end && ioBounds[end - 1].GetCenter()[dimension] >= split)
			--end;

		if (start < end)
		{
			// Swap the two elements
			std::swap(ioBodyIdx[start], ioBodyIdx[end - 1]);
			std::swap(ioBounds[start], ioBounds[end - 1]);
			++start;
			--end;
		}
	}
	JPH_ASSERT(start == end);

	if (start > 0 && start < inNumber)
	{
		// Success!
		outMidPoint = start;
	}
	else
	{
		// Failed to divide bodies
		outMidPoint = inNumber / 2;
	}
}

void StaticCompoundShape::sPartition4(uint *ioBodyIdx, AABox *ioBounds, int inBegin, int inEnd, int *outSplit)
{
	uint *body_idx = ioBodyIdx + inBegin;
	AABox *node_bounds = ioBounds + inBegin;
	int number = inEnd - inBegin;

	// Partition entire range
	sPartition(body_idx, node_bounds, number, outSplit[2]);

	// Partition lower half
	sPartition(body_idx, node_bounds, outSplit[2], outSplit[1]);

	// Partition upper half
	sPartition(body_idx + outSplit[2], node_bounds + outSplit[2], number - outSplit[2], outSplit[3]);

	// Convert to proper range
	outSplit[0] = inBegin;
	outSplit[1] += inBegin;
	outSplit[2] += inBegin;
	outSplit[3] += outSplit[2];
	outSplit[4] = inEnd;
}

StaticCompoundShape::StaticCompoundShape(const StaticCompoundShapeSettings &inSettings, TempAllocator &inTempAllocator, ShapeResult &outResult) :
	CompoundShape(EShapeSubType::StaticCompound, inSettings, outResult)
{
	// Check that there's at least 1 shape
	uint num_subshapes = (uint)inSettings.mSubShapes.size();
	if (num_subshapes < 2)
	{
		outResult.SetError("Compound needs at least 2 sub shapes, otherwise you should use a RotatedTranslatedShape!");
		return;
	}

	// Keep track of total mass to calculate center of mass
	float mass = 0.0f;

	mSubShapes.resize(num_subshapes);
	for (uint i = 0; i < num_subshapes; ++i)
	{
		const CompoundShapeSettings::SubShapeSettings &shape = inSettings.mSubShapes[i];

		// Start constructing the runtime sub shape
		SubShape &out_shape = mSubShapes[i];
		if (!out_shape.FromSettings(shape, outResult))
			return;

		// Calculate mass properties of child
		MassProperties child = out_shape.mShape->GetMassProperties();

		// Accumulate center of mass
		mass += child.mMass;
		mCenterOfMass += out_shape.GetPositionCOM() * child.mMass;
	}

	if (mass > 0.0f)
		mCenterOfMass /= mass;

	// Cache the inner radius as it can take a while to recursively iterate over all sub shapes
	CalculateInnerRadius();

	// Temporary storage for the bounding boxes of all shapes
	uint bounds_size = num_subshapes * sizeof(AABox);
	AABox *bounds = (AABox *)inTempAllocator.Allocate(bounds_size);
	JPH_SCOPE_EXIT([&inTempAllocator, bounds, bounds_size]{ inTempAllocator.Free(bounds, bounds_size); });

	// Temporary storage for body indexes (we're shuffling them)
	uint body_idx_size = num_subshapes * sizeof(uint);
	uint *body_idx = (uint *)inTempAllocator.Allocate(body_idx_size);
	JPH_SCOPE_EXIT([&inTempAllocator, body_idx, body_idx_size]{ inTempAllocator.Free(body_idx, body_idx_size); });

	// Shift all shapes so that the center of mass is now at the origin and calculate bounds
	for (uint i = 0; i < num_subshapes; ++i)
	{
		SubShape &shape = mSubShapes[i];

		// Shift the shape so it's centered around our center of mass
		shape.SetPositionCOM(shape.GetPositionCOM() - mCenterOfMass);

		// Transform the shape's bounds into our local space
		Mat44 transform = Mat44::sRotationTranslation(shape.GetRotation(), shape.GetPositionCOM());
		AABox shape_bounds = shape.mShape->GetWorldSpaceBounds(transform, Vec3::sReplicate(1.0f));

		// Store bounds and body index for tree construction
		bounds[i] = shape_bounds;
		body_idx[i] = i;

		// Update our local bounds
		mLocalBounds.Encapsulate(shape_bounds);
	}

	// The algorithm is a recursive tree build, but to avoid the call overhead we keep track of a stack here
	struct StackEntry
	{
		uint32			mNodeIdx;					// Node index of node that is generated
		int				mChildIdx;					// Index of child that we're currently processing
		int				mSplit[5];					// Indices where the node ID's have been split to form 4 partitions
		AABox			mBounds;					// Bounding box of this node
	};
	uint stack_size = num_subshapes * sizeof(StackEntry);
	StackEntry *stack = (StackEntry *)inTempAllocator.Allocate(stack_size);
	JPH_SCOPE_EXIT([&inTempAllocator, stack, stack_size]{ inTempAllocator.Free(stack, stack_size); });
	int top = 0;

	// Reserve enough space so that every sub shape gets its own leaf node
	uint next_node_idx = 0;
	mNodes.resize(num_subshapes + (num_subshapes + 2) / 3); // = Sum(num_subshapes * 4^-i) with i = [0, Inf].

	// Create root node
	stack[0].mNodeIdx = next_node_idx++;
	stack[0].mChildIdx = -1;
	stack[0].mBounds = AABox();
	sPartition4(body_idx, bounds, 0, num_subshapes, stack[0].mSplit);

	for (;;)
	{
		StackEntry &cur_stack = stack[top];

		// Next child
		cur_stack.mChildIdx++;

		// Check if all children processed
		if (cur_stack.mChildIdx >= 4)
		{
			// Terminate if there's nothing left to pop
			if (top <= 0)
				break;

			// Add our bounds to our parents bounds
			StackEntry &prev_stack = stack[top - 1];
			prev_stack.mBounds.Encapsulate(cur_stack.mBounds);

			// Store this node's properties in the parent node
			Node &parent_node = mNodes[prev_stack.mNodeIdx];
			parent_node.mNodeProperties[prev_stack.mChildIdx] = cur_stack.mNodeIdx;
			parent_node.SetChildBounds(prev_stack.mChildIdx, cur_stack.mBounds);

			// Pop entry from stack
			--top;
		}
		else
		{
			// Get low and high index to bodies to process
			int low = cur_stack.mSplit[cur_stack.mChildIdx];
			int high = cur_stack.mSplit[cur_stack.mChildIdx + 1];
			int num_bodies = high - low;

			if (num_bodies == 0)
			{
				// Mark invalid
				Node &node = mNodes[cur_stack.mNodeIdx];
				node.SetChildInvalid(cur_stack.mChildIdx);
			}
			else if (num_bodies == 1)
			{
				// Get body info
				uint child_node_idx = body_idx[low];
				const AABox &child_bounds = bounds[low];

				// Update node
				Node &node = mNodes[cur_stack.mNodeIdx];
				node.mNodeProperties[cur_stack.mChildIdx] = child_node_idx | IS_SUBSHAPE;
				node.SetChildBounds(cur_stack.mChildIdx, child_bounds);

				// Encapsulate bounding box in parent
				cur_stack.mBounds.Encapsulate(child_bounds);
			}
			else
			{
				// Allocate new node
				StackEntry &new_stack = stack[++top];
				JPH_ASSERT(top < (int)num_subshapes);
				new_stack.mNodeIdx = next_node_idx++;
				new_stack.mChildIdx = -1;
				new_stack.mBounds = AABox();
				sPartition4(body_idx, bounds, low, high, new_stack.mSplit);
			}
		}
	}

	// Resize nodes to actual size
	JPH_ASSERT(next_node_idx <= mNodes.size());
	mNodes.resize(next_node_idx);
	mNodes.shrink_to_fit();

	// Check if we ran out of bits for addressing a node
	if (next_node_idx > IS_SUBSHAPE)
	{
		outResult.SetError("Compound hierarchy has too many nodes");
		return;
	}

	// Check if we're not exceeding the amount of sub shape id bits
	if (GetSubShapeIDBitsRecursive() > SubShapeID::MaxBits)
	{
		outResult.SetError("Compound hierarchy is too deep and exceeds the amount of available sub shape ID bits");
		return;
	}

	outResult.Set(this);
}

template <class Visitor>
inline void StaticCompoundShape::WalkTree(Visitor &ioVisitor) const
{
	uint32 node_stack[cStackSize];
	node_stack[0] = 0;
	int top = 0;
	do
	{
		// Test if the node is valid, the node should rarely be invalid but it is possible when testing
		// a really large box against the tree that the invalid nodes will intersect with the box
		uint32 node_properties = node_stack[top];
		if (node_properties != INVALID_NODE)
		{
			// Test if node contains triangles
			bool is_node = (node_properties & IS_SUBSHAPE) == 0;
			if (is_node)
			{
				const Node &node = mNodes[node_properties];

				// Unpack bounds
				UVec4 bounds_minxy = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node.mBoundsMinX[0]));
				Vec4 bounds_minx = HalfFloatConversion::ToFloat(bounds_minxy);
				Vec4 bounds_miny = HalfFloatConversion::ToFloat(bounds_minxy.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

				UVec4 bounds_minzmaxx = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node.mBoundsMinZ[0]));
				Vec4 bounds_minz = HalfFloatConversion::ToFloat(bounds_minzmaxx);
				Vec4 bounds_maxx = HalfFloatConversion::ToFloat(bounds_minzmaxx.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

				UVec4 bounds_maxyz = UVec4::sLoadInt4(reinterpret_cast<const uint32 *>(&node.mBoundsMaxY[0]));
				Vec4 bounds_maxy = HalfFloatConversion::ToFloat(bounds_maxyz);
				Vec4 bounds_maxz = HalfFloatConversion::ToFloat(bounds_maxyz.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());

				// Load properties for 4 children
				UVec4 properties = UVec4::sLoadInt4(&node.mNodeProperties[0]);

				// Check which sub nodes to visit
				int num_results = ioVisitor.VisitNodes(bounds_minx, bounds_miny, bounds_minz, bounds_maxx, bounds_maxy, bounds_maxz, properties, top);

				// Push them onto the stack
				JPH_ASSERT(top + 4 < cStackSize);
				properties.StoreInt4(&node_stack[top]);
				top += num_results;
			}
			else
			{
				// Points to a sub shape
				uint32 sub_shape_idx = node_properties ^ IS_SUBSHAPE;
				const SubShape &sub_shape = mSubShapes[sub_shape_idx];

				ioVisitor.VisitShape(sub_shape, sub_shape_idx);
			}

			// Check if we're done
			if (ioVisitor.ShouldAbort())
				break;
		}

		// Fetch next node until we find one that the visitor wants to see
		do
			--top;
		while (top >= 0 && !ioVisitor.ShouldVisitNode(top));
	}
	while (top >= 0);
}

bool StaticCompoundShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastRayVisitor
	{
		using CastRayVisitor::CastRayVisitor;

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mHit.mFraction;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mHit.mFraction, ioProperties, &mDistanceStack[inStackTop]);
		}

		float				mDistanceStack[cStackSize];
	};

	Visitor visitor(inRay, this, inSubShapeIDCreator, ioHit);
	WalkTree(visitor);
	return visitor.mReturnValue;
}

void StaticCompoundShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastRayVisitorCollector
	{
		using CastRayVisitorCollector::CastRayVisitorCollector;

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetEarlyOutFraction();
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		float				mDistanceStack[cStackSize];
	};

	Visitor visitor(inRay, inRayCastSettings, this, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkTree(visitor);
}

void StaticCompoundShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CollidePointVisitor
	{
		using CollidePointVisitor::CollidePointVisitor;

		JPH_INLINE bool		ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Test if point overlaps with box
			UVec4 collides = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
			return CountAndSortTrues(collides, ioProperties);
		}
	};

	Visitor visitor(inPoint, this, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkTree(visitor);
}

void StaticCompoundShape::sCastShapeVsCompound(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CastShapeVisitor
	{
		using CastShapeVisitor::CastShapeVisitor;

		JPH_INLINE bool		ShouldVisitNode(int inStackTop) const
		{
			return mDistanceStack[inStackTop] < mCollector.GetPositiveEarlyOutFraction();
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, int inStackTop)
		{
			// Test bounds of 4 children
			Vec4 distance = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);

			// Sort so that highest values are first (we want to first process closer hits and we process stack top to bottom)
			return SortReverseAndStore(distance, mCollector.GetPositiveEarlyOutFraction(), ioProperties, &mDistanceStack[inStackTop]);
		}

		float				mDistanceStack[cStackSize];
	};

	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::StaticCompound);
	const StaticCompoundShape *shape = static_cast<const StaticCompoundShape *>(inShape);

	Visitor visitor(inShapeCast, inShapeCastSettings, shape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
	shape->WalkTree(visitor);
}

void StaticCompoundShape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	struct Visitor : public CollectTransformedShapesVisitor
	{
		using CollectTransformedShapesVisitor::CollectTransformedShapesVisitor;

		JPH_INLINE bool		ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Test which nodes collide
			UVec4 collides = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
			return CountAndSortTrues(collides, ioProperties);
		}
	};

	Visitor visitor(inBox, this, inPositionCOM, inRotation, inScale, inSubShapeIDCreator, ioCollector, inShapeFilter);
	WalkTree(visitor);
}

int StaticCompoundShape::GetIntersectingSubShapes(const AABox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const
{
	JPH_PROFILE_FUNCTION();

	GetIntersectingSubShapesVisitorSC<AABox> visitor(inBox, outSubShapeIndices, inMaxSubShapeIndices);
	WalkTree(visitor);
	return visitor.GetNumResults();
}

int StaticCompoundShape::GetIntersectingSubShapes(const OrientedBox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const
{
	JPH_PROFILE_FUNCTION();

	GetIntersectingSubShapesVisitorSC<OrientedBox> visitor(inBox, outSubShapeIndices, inMaxSubShapeIndices);
	WalkTree(visitor);
	return visitor.GetNumResults();
}

void StaticCompoundShape::sCollideCompoundVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::StaticCompound);
	const StaticCompoundShape *shape1 = static_cast<const StaticCompoundShape *>(inShape1);

	struct Visitor : public CollideCompoundVsShapeVisitor
	{
		using CollideCompoundVsShapeVisitor::CollideCompoundVsShapeVisitor;

		JPH_INLINE bool		ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Test which nodes collide
			UVec4 collides = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
			return CountAndSortTrues(collides, ioProperties);
		}
	};

	Visitor visitor(shape1, inShape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
	shape1->WalkTree(visitor);
}

void StaticCompoundShape::sCollideShapeVsCompound(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	struct Visitor : public CollideShapeVsCompoundVisitor
	{
		using CollideShapeVsCompoundVisitor::CollideShapeVsCompoundVisitor;

		JPH_INLINE bool		ShouldVisitNode([[maybe_unused]] int inStackTop) const
		{
			return true;
		}

		JPH_INLINE int		VisitNodes(Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ, UVec4 &ioProperties, [[maybe_unused]] int inStackTop) const
		{
			// Test which nodes collide
			UVec4 collides = TestBounds(inBoundsMinX, inBoundsMinY, inBoundsMinZ, inBoundsMaxX, inBoundsMaxY, inBoundsMaxZ);
			return CountAndSortTrues(collides, ioProperties);
		}
	};

	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::StaticCompound);
	const StaticCompoundShape *shape2 = static_cast<const StaticCompoundShape *>(inShape2);

	Visitor visitor(inShape1, shape2, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
	shape2->WalkTree(visitor);
}

void StaticCompoundShape::SaveBinaryState(StreamOut &inStream) const
{
	CompoundShape::SaveBinaryState(inStream);

	inStream.Write(mNodes);
}

void StaticCompoundShape::RestoreBinaryState(StreamIn &inStream)
{
	CompoundShape::RestoreBinaryState(inStream);

	inStream.Read(mNodes);
}

void StaticCompoundShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::StaticCompound);
	f.mConstruct = []() -> Shape * { return new StaticCompoundShape; };
	f.mColor = Color::sOrange;

	for (EShapeSubType s : sAllSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::StaticCompound, s, sCollideCompoundVsShape);
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::StaticCompound, sCollideShapeVsCompound);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::StaticCompound, sCastShapeVsCompound);
	}
}

JPH_NAMESPACE_END
