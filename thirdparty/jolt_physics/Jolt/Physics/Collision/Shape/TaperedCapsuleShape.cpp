// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Geometry/RayCapsule.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(TaperedCapsuleShapeSettings)
{
	JPH_ADD_BASE_CLASS(TaperedCapsuleShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(TaperedCapsuleShapeSettings, mHalfHeightOfTaperedCylinder)
	JPH_ADD_ATTRIBUTE(TaperedCapsuleShapeSettings, mTopRadius)
	JPH_ADD_ATTRIBUTE(TaperedCapsuleShapeSettings, mBottomRadius)
}

bool TaperedCapsuleShapeSettings::IsSphere() const
{
	return max(mTopRadius, mBottomRadius) >= 2.0f * mHalfHeightOfTaperedCylinder + min(mTopRadius, mBottomRadius);
}

ShapeSettings::ShapeResult TaperedCapsuleShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
	{
		Ref<Shape> shape;
		if (IsValid() && IsSphere())
		{
			// Determine sphere center and radius
			float radius, center;
			if (mTopRadius > mBottomRadius)
			{
				radius = mTopRadius;
				center = mHalfHeightOfTaperedCylinder;
			}
			else
			{
				radius = mBottomRadius;
				center = -mHalfHeightOfTaperedCylinder;
			}

			// Create sphere
			shape = new SphereShape(radius, mMaterial);

			// Offset sphere if needed
			if (abs(center) > 1.0e-6f)
			{
				RotatedTranslatedShapeSettings rot_trans(Vec3(0, center, 0), Quat::sIdentity(), shape);
				mCachedResult = rot_trans.Create();
			}
			else
				mCachedResult.Set(shape);
		}
		else
		{
			// Normal tapered capsule shape
			shape = new TaperedCapsuleShape(*this, mCachedResult);
		}
	}
	return mCachedResult;
}

TaperedCapsuleShapeSettings::TaperedCapsuleShapeSettings(float inHalfHeightOfTaperedCylinder, float inTopRadius, float inBottomRadius, const PhysicsMaterial *inMaterial) :
	ConvexShapeSettings(inMaterial),
	mHalfHeightOfTaperedCylinder(inHalfHeightOfTaperedCylinder),
	mTopRadius(inTopRadius),
	mBottomRadius(inBottomRadius)
{
}

TaperedCapsuleShape::TaperedCapsuleShape(const TaperedCapsuleShapeSettings &inSettings, ShapeResult &outResult) :
	ConvexShape(EShapeSubType::TaperedCapsule, inSettings, outResult),
	mTopRadius(inSettings.mTopRadius),
	mBottomRadius(inSettings.mBottomRadius)
{
	if (mTopRadius <= 0.0f)
	{
		outResult.SetError("Invalid top radius");
		return;
	}

	if (mBottomRadius <= 0.0f)
	{
		outResult.SetError("Invalid bottom radius");
		return;
	}

	if (inSettings.mHalfHeightOfTaperedCylinder <= 0.0f)
	{
		outResult.SetError("Invalid height");
		return;
	}

	// If this goes off one of the sphere ends falls totally inside the other and you should use a sphere instead
	if (inSettings.IsSphere())
	{
		outResult.SetError("One sphere embedded in other sphere, please use sphere shape instead");
		return;
	}

	// Approximation: The center of mass is exactly half way between the top and bottom cap of the tapered capsule
	mTopCenter = inSettings.mHalfHeightOfTaperedCylinder + 0.5f * (mBottomRadius - mTopRadius);
	mBottomCenter = -inSettings.mHalfHeightOfTaperedCylinder + 0.5f * (mBottomRadius - mTopRadius);

	// Calculate center of mass
	mCenterOfMass = Vec3(0, inSettings.mHalfHeightOfTaperedCylinder - mTopCenter, 0);

	// Calculate convex radius
	mConvexRadius = min(mTopRadius, mBottomRadius);
	JPH_ASSERT(mConvexRadius > 0.0f);

	// Calculate the sin and tan of the angle that the cone surface makes with the Y axis
	// See: TaperedCapsuleShape.gliffy
	mSinAlpha = (mBottomRadius - mTopRadius) / (mTopCenter - mBottomCenter);
	JPH_ASSERT(mSinAlpha >= -1.0f && mSinAlpha <= 1.0f);
	mTanAlpha = Tan(ASin(mSinAlpha));

	outResult.Set(this);
}

class TaperedCapsuleShape::TaperedCapsule final : public Support
{
public:
					TaperedCapsule(Vec3Arg inTopCenter, Vec3Arg inBottomCenter, float inTopRadius, float inBottomRadius, float inConvexRadius) :
		mTopCenter(inTopCenter),
		mBottomCenter(inBottomCenter),
		mTopRadius(inTopRadius),
		mBottomRadius(inBottomRadius),
		mConvexRadius(inConvexRadius)
	{
		static_assert(sizeof(TaperedCapsule) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(TaperedCapsule)));
	}

	virtual Vec3	GetSupport(Vec3Arg inDirection) const override
	{
		// Check zero vector
		float len = inDirection.Length();
		if (len == 0.0f)
			return mTopCenter + Vec3(0, mTopRadius, 0); // Return top

		// Check if the support of the top sphere or bottom sphere is bigger
		Vec3 support_top = mTopCenter + (mTopRadius / len) * inDirection;
		Vec3 support_bottom = mBottomCenter + (mBottomRadius / len) * inDirection;
		if (support_top.Dot(inDirection) > support_bottom.Dot(inDirection))
			return support_top;
		else
			return support_bottom;
	}

	virtual float	GetConvexRadius() const override
	{
		return mConvexRadius;
	}

private:
	Vec3			mTopCenter;
	Vec3			mBottomCenter;
	float			mTopRadius;
	float			mBottomRadius;
	float			mConvexRadius;
};

const ConvexShape::Support *TaperedCapsuleShape::GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	// Get scaled tapered capsule
	Vec3 abs_scale = inScale.Abs();
	float scale_xz = abs_scale.GetX();
	float scale_y = inScale.GetY(); // The sign of y is important as it flips the tapered capsule
	Vec3 scaled_top_center = Vec3(0, scale_y * mTopCenter, 0);
	Vec3 scaled_bottom_center = Vec3(0, scale_y * mBottomCenter, 0);
	float scaled_top_radius = scale_xz * mTopRadius;
	float scaled_bottom_radius = scale_xz * mBottomRadius;
	float scaled_convex_radius = scale_xz * mConvexRadius;

	switch (inMode)
	{
	case ESupportMode::IncludeConvexRadius:
		return new (&inBuffer) TaperedCapsule(scaled_top_center, scaled_bottom_center, scaled_top_radius, scaled_bottom_radius, 0.0f);

	case ESupportMode::ExcludeConvexRadius:
	case ESupportMode::Default:
		{
			// Get radii reduced by convex radius
			float tr = scaled_top_radius - scaled_convex_radius;
			float br = scaled_bottom_radius - scaled_convex_radius;
			JPH_ASSERT(tr >= 0.0f && br >= 0.0f);
			JPH_ASSERT(tr == 0.0f || br == 0.0f, "Convex radius should be that of the smallest sphere");
			return new (&inBuffer) TaperedCapsule(scaled_top_center, scaled_bottom_center, tr, br, scaled_convex_radius);
		}
	}

	JPH_ASSERT(false);
	return nullptr;
}

void TaperedCapsuleShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");
	JPH_ASSERT(IsValidScale(inScale));

	// Check zero vector
	float len = inDirection.Length();
	if (len == 0.0f)
		return;

	// Get scaled tapered capsule
	Vec3 abs_scale = inScale.Abs();
	float scale_xz = abs_scale.GetX();
	float scale_y = inScale.GetY(); // The sign of y is important as it flips the tapered capsule
	Vec3 scaled_top_center = Vec3(0, scale_y * mTopCenter, 0);
	Vec3 scaled_bottom_center = Vec3(0, scale_y * mBottomCenter, 0);
	float scaled_top_radius = scale_xz * mTopRadius;
	float scaled_bottom_radius = scale_xz * mBottomRadius;

	// Get support point for top and bottom sphere in the opposite of inDirection (including convex radius)
	Vec3 support_top = scaled_top_center - (scaled_top_radius / len) * inDirection;
	Vec3 support_bottom = scaled_bottom_center - (scaled_bottom_radius / len) * inDirection;

	// Get projection on inDirection
	float proj_top = support_top.Dot(inDirection);
	float proj_bottom = support_bottom.Dot(inDirection);

	// If projection is roughly equal then return line, otherwise we return nothing as there's only 1 point
	if (abs(proj_top - proj_bottom) < cCapsuleProjectionSlop * len)
	{
		outVertices.push_back(inCenterOfMassTransform * support_top);
		outVertices.push_back(inCenterOfMassTransform * support_bottom);
	}
}

MassProperties TaperedCapsuleShape::GetMassProperties() const
{
	AABox box = GetInertiaApproximation();

	MassProperties p;
	p.SetMassAndInertiaOfSolidBox(box.GetSize(), GetDensity());
	return p;
}

Vec3 TaperedCapsuleShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	// See: TaperedCapsuleShape.gliffy
	// We need to calculate ty and by in order to see if the position is on the top or bottom sphere
	// sin(alpha) = by / br = ty / tr
	// => by = sin(alpha) * br, ty = sin(alpha) * tr

	if (inLocalSurfacePosition.GetY() > mTopCenter + mSinAlpha * mTopRadius)
		return (inLocalSurfacePosition - Vec3(0, mTopCenter, 0)).Normalized();
	else if (inLocalSurfacePosition.GetY() < mBottomCenter + mSinAlpha * mBottomRadius)
		return (inLocalSurfacePosition - Vec3(0, mBottomCenter, 0)).Normalized();
	else
	{
		// Get perpendicular vector to the surface in the xz plane
		Vec3 perpendicular = Vec3(inLocalSurfacePosition.GetX(), 0, inLocalSurfacePosition.GetZ()).NormalizedOr(Vec3::sAxisX());

		// We know that the perpendicular has length 1 and that it needs a y component where tan(alpha) = y / 1 in order to align it to the surface
		perpendicular.SetY(mTanAlpha);
		return perpendicular.Normalized();
	}
}

AABox TaperedCapsuleShape::GetLocalBounds() const
{
	float max_radius = max(mTopRadius, mBottomRadius);
	return AABox(Vec3(-max_radius, mBottomCenter - mBottomRadius, -max_radius), Vec3(max_radius, mTopCenter + mTopRadius, max_radius));
}

AABox TaperedCapsuleShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 abs_scale = inScale.Abs();
	float scale_xz = abs_scale.GetX();
	float scale_y = inScale.GetY(); // The sign of y is important as it flips the tapered capsule
	Vec3 bottom_extent = Vec3::sReplicate(scale_xz * mBottomRadius);
	Vec3 bottom_center = inCenterOfMassTransform * Vec3(0, scale_y * mBottomCenter, 0);
	Vec3 top_extent = Vec3::sReplicate(scale_xz * mTopRadius);
	Vec3 top_center = inCenterOfMassTransform * Vec3(0, scale_y * mTopCenter, 0);
	Vec3 p1 = Vec3::sMin(top_center - top_extent, bottom_center - bottom_extent);
	Vec3 p2 = Vec3::sMax(top_center + top_extent, bottom_center + bottom_extent);
	return AABox(p1, p2);
}

void TaperedCapsuleShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Mat44 inverse_transform = inCenterOfMassTransform.InversedRotationTranslation();

	// Get scaled tapered capsule
	Vec3 abs_scale = inScale.Abs();
	float scale_y = abs_scale.GetY();
	float scale_xz = abs_scale.GetX();
	Vec3 scale_y_flip(1, Sign(inScale.GetY()), 1);
	Vec3 scaled_top_center(0, scale_y * mTopCenter, 0);
	Vec3 scaled_bottom_center(0, scale_y * mBottomCenter, 0);
	float scaled_top_radius = scale_xz * mTopRadius;
	float scaled_bottom_radius = scale_xz * mBottomRadius;

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			Vec3 local_pos = scale_y_flip * (inverse_transform * v.GetPosition());

			Vec3 position, normal;

			// If the vertex is inside the cone starting at the top center pointing along the y-axis with angle PI/2 - alpha then the closest point is on the top sphere
			// This corresponds to: Dot(y-axis, (local_pos - top_center) / |local_pos - top_center|) >= cos(PI/2 - alpha)
			// <=> (local_pos - top_center).y >= sin(alpha) * |local_pos - top_center|
			Vec3 top_center_to_local_pos = local_pos - scaled_top_center;
			float top_center_to_local_pos_len = top_center_to_local_pos.Length();
			if (top_center_to_local_pos.GetY() >= mSinAlpha * top_center_to_local_pos_len)
			{
				// Top sphere
				normal = top_center_to_local_pos_len != 0.0f? top_center_to_local_pos / top_center_to_local_pos_len : Vec3::sAxisY();
				position = scaled_top_center + scaled_top_radius * normal;
			}
			else
			{
				// If the vertex is outside the cone starting at the bottom center pointing along the y-axis with angle PI/2 - alpha then the closest point is on the bottom sphere
				// This corresponds to: Dot(y-axis, (local_pos - bottom_center) / |local_pos - bottom_center|) <= cos(PI/2 - alpha)
				// <=> (local_pos - bottom_center).y <= sin(alpha) * |local_pos - bottom_center|
				Vec3 bottom_center_to_local_pos = local_pos - scaled_bottom_center;
				float bottom_center_to_local_pos_len = bottom_center_to_local_pos.Length();
				if (bottom_center_to_local_pos.GetY() <= mSinAlpha * bottom_center_to_local_pos_len)
				{
					// Bottom sphere
					normal = bottom_center_to_local_pos_len != 0.0f? bottom_center_to_local_pos / bottom_center_to_local_pos_len : -Vec3::sAxisY();
				}
				else
				{
					// Tapered cylinder
					normal = Vec3(local_pos.GetX(), 0, local_pos.GetZ()).NormalizedOr(Vec3::sAxisX());
					normal.SetY(mTanAlpha);
					normal = normal.NormalizedOr(Vec3::sAxisX());
				}
				position = scaled_bottom_center + scaled_bottom_radius * normal;
			}

			Plane plane = Plane::sFromPointAndNormal(position, normal);
			float penetration = -plane.SignedDistance(local_pos);
			if (v.UpdatePenetration(penetration))
			{
				// Need to flip the normal's y if capsule is flipped (this corresponds to flipping both the point and the normal around y)
				plane.SetNormal(scale_y_flip * plane.GetNormal());

				// Store collision
				v.SetCollision(plane.GetTransformed(inCenterOfMassTransform), inCollidingShapeIndex);
			}
		}
}

#ifdef JPH_DEBUG_RENDERER
void TaperedCapsuleShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	if (mGeometry == nullptr)
	{
		SupportBuffer buffer;
		const Support *support = GetSupportFunction(ESupportMode::IncludeConvexRadius, buffer, Vec3::sReplicate(1.0f));
		mGeometry = inRenderer->CreateTriangleGeometryForConvex([support](Vec3Arg inDirection) { return support->GetSupport(inDirection); });
	}

	// Preserve flip along y axis but make sure we're not inside out
	Vec3 scale = ScaleHelpers::IsInsideOut(inScale)? Vec3(-1, 1, 1) * inScale : inScale;
	RMat44 world_transform = inCenterOfMassTransform * Mat44::sScale(scale);

	AABox bounds = Shape::GetWorldSpaceBounds(inCenterOfMassTransform, inScale);

	float lod_scale_sq = Square(max(mTopRadius, mBottomRadius));

	Color color = inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor;

	DebugRenderer::EDrawMode draw_mode = inDrawWireframe? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;

	inRenderer->DrawGeometry(world_transform, bounds, lod_scale_sq, color, mGeometry, DebugRenderer::ECullMode::CullBackFace, DebugRenderer::ECastShadow::On, draw_mode);
}
#endif // JPH_DEBUG_RENDERER

AABox TaperedCapsuleShape::GetInertiaApproximation() const
{
	// TODO: For now the mass and inertia is that of a box
	float avg_radius = 0.5f * (mTopRadius + mBottomRadius);
	return AABox(Vec3(-avg_radius, mBottomCenter - mBottomRadius, -avg_radius), Vec3(avg_radius, mTopCenter + mTopRadius, avg_radius));
}

void TaperedCapsuleShape::SaveBinaryState(StreamOut &inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mCenterOfMass);
	inStream.Write(mTopRadius);
	inStream.Write(mBottomRadius);
	inStream.Write(mTopCenter);
	inStream.Write(mBottomCenter);
	inStream.Write(mConvexRadius);
	inStream.Write(mSinAlpha);
	inStream.Write(mTanAlpha);
}

void TaperedCapsuleShape::RestoreBinaryState(StreamIn &inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mCenterOfMass);
	inStream.Read(mTopRadius);
	inStream.Read(mBottomRadius);
	inStream.Read(mTopCenter);
	inStream.Read(mBottomCenter);
	inStream.Read(mConvexRadius);
	inStream.Read(mSinAlpha);
	inStream.Read(mTanAlpha);
}

bool TaperedCapsuleShape::IsValidScale(Vec3Arg inScale) const
{
	return ConvexShape::IsValidScale(inScale) && ScaleHelpers::IsUniformScale(inScale.Abs());
}

Vec3 TaperedCapsuleShape::MakeScaleValid(Vec3Arg inScale) const
{
	Vec3 scale = ScaleHelpers::MakeNonZeroScale(inScale);

	return scale.GetSign() * ScaleHelpers::MakeUniformScale(scale.Abs());
}

void TaperedCapsuleShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::TaperedCapsule);
	f.mConstruct = []() -> Shape * { return new TaperedCapsuleShape; };
	f.mColor = Color::sGreen;
}

JPH_NAMESPACE_END
