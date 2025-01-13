// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;
class OrientedBox;

/// Base class settings to construct a compound shape
class JPH_EXPORT CompoundShapeSettings : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(JPH_EXPORT, CompoundShapeSettings)

public:
	/// Constructor. Use AddShape to add the parts.
									CompoundShapeSettings() = default;

	/// Add a shape to the compound.
	void							AddShape(Vec3Arg inPosition, QuatArg inRotation, const ShapeSettings *inShape, uint32 inUserData = 0);

	/// Add a shape to the compound. Variant that uses a concrete shape, which means this object cannot be serialized.
	void							AddShape(Vec3Arg inPosition, QuatArg inRotation, const Shape *inShape, uint32 inUserData = 0);

	struct SubShapeSettings
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SubShapeSettings)

		RefConst<ShapeSettings>		mShape;													///< Sub shape (either this or mShapePtr needs to be filled up)
		RefConst<Shape>				mShapePtr;												///< Sub shape (either this or mShape needs to be filled up)
		Vec3						mPosition;												///< Position of the sub shape
		Quat						mRotation;												///< Rotation of the sub shape
		uint32						mUserData = 0;											///< User data value (can be used by the application for any purpose)
	};

	using SubShapes = Array<SubShapeSettings>;

	SubShapes						mSubShapes;
};

/// Base class for a compound shape
class JPH_EXPORT CompoundShape : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit						CompoundShape(EShapeSubType inSubType) : Shape(EShapeType::Compound, inSubType) { }
									CompoundShape(EShapeSubType inSubType, const ShapeSettings &inSettings, ShapeResult &outResult) : Shape(EShapeType::Compound, inSubType, inSettings, outResult) { }

	// See Shape::GetCenterOfMass
	virtual Vec3					GetCenterOfMass() const override						{ return mCenterOfMass; }

	// See Shape::MustBeStatic
	virtual bool					MustBeStatic() const override;

	// See Shape::GetLocalBounds
	virtual AABox					GetLocalBounds() const override							{ return mLocalBounds; }

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override;

	// See Shape::GetWorldSpaceBounds
	virtual AABox					GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const override;
	using Shape::GetWorldSpaceBounds;

	// See Shape::GetInnerRadius
	virtual float					GetInnerRadius() const override							{ return mInnerRadius; }

	// See Shape::GetMassProperties
	virtual MassProperties			GetMassProperties() const override;

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override;

	// See Shape::GetLeafShape
	virtual const Shape *			GetLeafShape(const SubShapeID &inSubShapeID, SubShapeID &outRemainder) const override;

	// See Shape::GetSubShapeUserData
	virtual uint64					GetSubShapeUserData(const SubShapeID &inSubShapeID) const override;

	// See Shape::GetSubShapeTransformedShape
	virtual TransformedShape		GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const override;

	// See Shape::GetSurfaceNormal
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See Shape::GetSubmergedVolume
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;

	// See Shape::DrawGetSupportFunction
	virtual void					DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const override;

	// See Shape::DrawGetSupportingFace
	virtual void					DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const override;
#endif // JPH_DEBUG_RENDERER

	// See: Shape::CollideSoftBodyVertices
	virtual void					CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::TransformShape
	virtual void					TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const override;

	// See Shape::GetTrianglesStart
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override { JPH_ASSERT(false, "Cannot call on non-leaf shapes, use CollectTransformedShapes to collect the leaves first!"); }

	// See Shape::GetTrianglesNext
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override { JPH_ASSERT(false, "Cannot call on non-leaf shapes, use CollectTransformedShapes to collect the leaves first!"); return 0; }

	/// Get which sub shape's bounding boxes overlap with an axis aligned box
	/// @param inBox The axis aligned box to test against (relative to the center of mass of this shape)
	/// @param outSubShapeIndices Buffer where to place the indices of the sub shapes that intersect
	/// @param inMaxSubShapeIndices How many indices will fit in the buffer (normally you'd provide a buffer of GetNumSubShapes() indices)
	/// @return How many indices were placed in outSubShapeIndices
	virtual int						GetIntersectingSubShapes(const AABox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const = 0;

	/// Get which sub shape's bounding boxes overlap with an axis aligned box
	/// @param inBox The axis aligned box to test against (relative to the center of mass of this shape)
	/// @param outSubShapeIndices Buffer where to place the indices of the sub shapes that intersect
	/// @param inMaxSubShapeIndices How many indices will fit in the buffer (normally you'd provide a buffer of GetNumSubShapes() indices)
	/// @return How many indices were placed in outSubShapeIndices
	virtual int						GetIntersectingSubShapes(const OrientedBox &inBox, uint *outSubShapeIndices, int inMaxSubShapeIndices) const = 0;

	struct SubShape
	{
		/// Initialize sub shape from sub shape settings
		/// @param inSettings Settings object
		/// @param outResult Result object, only used in case of error
		/// @return True on success, false on failure
		bool						FromSettings(const CompoundShapeSettings::SubShapeSettings &inSettings, ShapeResult &outResult)
		{
			if (inSettings.mShapePtr != nullptr)
			{
				// Use provided shape
				mShape = inSettings.mShapePtr;
			}
			else
			{
				// Create child shape
				ShapeResult child_result = inSettings.mShape->Create();
				if (!child_result.IsValid())
				{
					outResult = child_result;
					return false;
				}
				mShape = child_result.Get();
			}

			// Copy user data
			mUserData = inSettings.mUserData;

			SetTransform(inSettings.mPosition, inSettings.mRotation, Vec3::sZero() /* Center of mass not yet calculated */);
			return true;
		}

		/// Update the transform of this sub shape
		/// @param inPosition New position
		/// @param inRotation New orientation
		/// @param inCenterOfMass The center of mass of the compound shape
		JPH_INLINE void				SetTransform(Vec3Arg inPosition, QuatArg inRotation, Vec3Arg inCenterOfMass)
		{
			SetPositionCOM(inPosition - inCenterOfMass + inRotation * mShape->GetCenterOfMass());

			mIsRotationIdentity = inRotation.IsClose(Quat::sIdentity()) || inRotation.IsClose(-Quat::sIdentity());
			SetRotation(mIsRotationIdentity? Quat::sIdentity() : inRotation);
		}

		/// Get the local transform for this shape given the scale of the child shape
		/// The total transform of the child shape will be GetLocalTransformNoScale(inScale) * Mat44::sScaling(TransformScale(inScale))
		/// @param inScale The scale of the child shape (in local space of this shape)
		JPH_INLINE Mat44			GetLocalTransformNoScale(Vec3Arg inScale) const
		{
			JPH_ASSERT(IsValidScale(inScale));
			return Mat44::sRotationTranslation(GetRotation(), inScale * GetPositionCOM());
		}

		/// Test if inScale is valid for this sub shape
		inline bool					IsValidScale(Vec3Arg inScale) const
		{
			// We can always handle uniform scale or identity rotations
			if (mIsRotationIdentity || ScaleHelpers::IsUniformScale(inScale))
				return true;

			return ScaleHelpers::CanScaleBeRotated(GetRotation(), inScale);
		}

		/// Transform the scale to the local space of the child shape
		inline Vec3					TransformScale(Vec3Arg inScale) const
		{
			// We don't need to transform uniform scale or if the rotation is identity
			if (mIsRotationIdentity || ScaleHelpers::IsUniformScale(inScale))
				return inScale;

			return ScaleHelpers::RotateScale(GetRotation(), inScale);
		}

		/// Compress the center of mass position
		JPH_INLINE void				SetPositionCOM(Vec3Arg inPositionCOM)
		{
			inPositionCOM.StoreFloat3(&mPositionCOM);
		}

		/// Uncompress the center of mass position
		JPH_INLINE Vec3				GetPositionCOM() const
		{
			return Vec3::sLoadFloat3Unsafe(mPositionCOM);
		}

		/// Compress the rotation
		JPH_INLINE void				SetRotation(QuatArg inRotation)
		{
			inRotation.StoreFloat3(&mRotation);
		}

		/// Uncompress the rotation
		JPH_INLINE Quat				GetRotation() const
		{
			return mIsRotationIdentity? Quat::sIdentity() : Quat::sLoadFloat3Unsafe(mRotation);
		}

		RefConst<Shape>				mShape;
		Float3						mPositionCOM;											///< Note: Position of center of mass of sub shape!
		Float3						mRotation;												///< Note: X, Y, Z of rotation quaternion - note we read 4 bytes beyond this so make sure there's something there
		uint32						mUserData;												///< User data value (put here because it falls in padding bytes)
		bool						mIsRotationIdentity;									///< If mRotation is close to identity (put here because it falls in padding bytes)
		// 3 padding bytes left
	};

	static_assert(sizeof(SubShape) == (JPH_CPU_ADDRESS_BITS == 64? 40 : 36), "Compiler added unexpected padding");

	using SubShapes = Array<SubShape>;

	/// Access to the sub shapes of this compound
	const SubShapes &				GetSubShapes() const									{ return mSubShapes; }

	/// Get the total number of sub shapes
	uint							GetNumSubShapes() const									{ return uint(mSubShapes.size()); }

	/// Access to a particular sub shape
	const SubShape &				GetSubShape(uint inIdx) const							{ return mSubShapes[inIdx]; }

	/// Get the user data associated with a shape in this compound
	uint32							GetCompoundUserData(uint inIdx) const					{ return mSubShapes[inIdx].mUserData; }

	/// Set the user data associated with a shape in this compound
	void							SetCompoundUserData(uint inIdx, uint32 inUserData)		{ mSubShapes[inIdx].mUserData = inUserData; }

	/// Check if a sub shape ID is still valid for this shape
	/// @param inSubShapeID Sub shape id that indicates the leaf shape relative to this shape
	/// @return True if the ID is valid, false if not
	inline bool						IsSubShapeIDValid(SubShapeID inSubShapeID) const
	{
		SubShapeID remainder;
		return inSubShapeID.PopID(GetSubShapeIDBits(), remainder) < mSubShapes.size();
	}

	/// Convert SubShapeID to sub shape index
	/// @param inSubShapeID Sub shape id that indicates the leaf shape relative to this shape
	/// @param outRemainder This is the sub shape ID for the sub shape of the compound after popping off the index
	/// @return The index of the sub shape of this compound
	inline uint32					GetSubShapeIndexFromID(SubShapeID inSubShapeID, SubShapeID &outRemainder) const
	{
		uint32 idx = inSubShapeID.PopID(GetSubShapeIDBits(), outRemainder);
		JPH_ASSERT(idx < mSubShapes.size(), "Invalid SubShapeID");
		return idx;
	}

	/// @brief Convert a sub shape index to a sub shape ID
	/// @param inIdx Index of the sub shape of this compound
	/// @param inParentSubShapeID Parent SubShapeID (describing the path to the compound shape)
	/// @return A sub shape ID creator that contains the full path to the sub shape with index inIdx
	inline SubShapeIDCreator		GetSubShapeIDFromIndex(int inIdx, const SubShapeIDCreator &inParentSubShapeID) const
	{
		return inParentSubShapeID.PushID(inIdx, GetSubShapeIDBits());
	}

	// See Shape
	virtual void					SaveBinaryState(StreamOut &inStream) const override;
	virtual void					SaveSubShapeState(ShapeList &outSubShapes) const override;
	virtual void					RestoreSubShapeState(const ShapeRefC *inSubShapes, uint inNumShapes) override;

	// See Shape::GetStatsRecursive
	virtual Stats					GetStatsRecursive(VisitedShapes &ioVisitedShapes) const override;

	// See Shape::GetVolume
	virtual float					GetVolume() const override;

	// See Shape::IsValidScale
	virtual bool					IsValidScale(Vec3Arg inScale) const override;

	// See Shape::MakeScaleValid
	virtual Vec3					MakeScaleValid(Vec3Arg inScale) const override;

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

	// Visitors for collision detection
	struct CastRayVisitor;
	struct CastRayVisitorCollector;
	struct CollidePointVisitor;
	struct CastShapeVisitor;
	struct CollectTransformedShapesVisitor;
	struct CollideCompoundVsShapeVisitor;
	struct CollideShapeVsCompoundVisitor;
	template <class BoxType> struct GetIntersectingSubShapesVisitor;

	/// Determine amount of bits needed to encode sub shape id
	inline uint						GetSubShapeIDBits() const
	{
		// Ensure we have enough bits to encode our shape [0, n - 1]
		uint32 n = uint32(mSubShapes.size()) - 1;
		return 32 - CountLeadingZeros(n);
	}

	/// Determine the inner radius of this shape
	inline void						CalculateInnerRadius()
	{
		mInnerRadius = FLT_MAX;
		for (const SubShape &s : mSubShapes)
			mInnerRadius = min(mInnerRadius, s.mShape->GetInnerRadius());
	}

	Vec3							mCenterOfMass { Vec3::sZero() };						///< Center of mass of the compound
	AABox							mLocalBounds;
	SubShapes						mSubShapes;
	float							mInnerRadius = FLT_MAX;									///< Smallest radius of GetInnerRadius() of child shapes

private:
	// Helper functions called by CollisionDispatch
	static void						sCastCompoundVsShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);
};

JPH_NAMESPACE_END
