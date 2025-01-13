// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/GetTrianglesContext.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
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

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(CapsuleShapeSettings)
{
	JPH_ADD_BASE_CLASS(CapsuleShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(CapsuleShapeSettings, mRadius)
	JPH_ADD_ATTRIBUTE(CapsuleShapeSettings, mHalfHeightOfCylinder)
}

static const int cCapsuleDetailLevel = 2;

static const StaticArray<Vec3, 192> sCapsuleTopTriangles = []() {
	StaticArray<Vec3, 192> verts;
	GetTrianglesContextVertexList::sCreateHalfUnitSphereTop(verts, cCapsuleDetailLevel);
	return verts;
}();

static const StaticArray<Vec3, 96> sCapsuleMiddleTriangles = []() {
	StaticArray<Vec3, 96> verts;
	GetTrianglesContextVertexList::sCreateUnitOpenCylinder(verts, cCapsuleDetailLevel);
	return verts;
}();

static const StaticArray<Vec3, 192> sCapsuleBottomTriangles = []() {
	StaticArray<Vec3, 192> verts;
	GetTrianglesContextVertexList::sCreateHalfUnitSphereBottom(verts, cCapsuleDetailLevel);
	return verts;
}();

ShapeSettings::ShapeResult CapsuleShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
	{
		Ref<Shape> shape;
		if (IsValid() && IsSphere())
		{
			// If the capsule has no height, use a sphere instead
			shape = new SphereShape(mRadius, mMaterial);
			mCachedResult.Set(shape);
		}
		else
			shape = new CapsuleShape(*this, mCachedResult);
	}
	return mCachedResult;
}

CapsuleShape::CapsuleShape(const CapsuleShapeSettings &inSettings, ShapeResult &outResult) :
	ConvexShape(EShapeSubType::Capsule, inSettings, outResult),
	mRadius(inSettings.mRadius),
	mHalfHeightOfCylinder(inSettings.mHalfHeightOfCylinder)
{
	if (inSettings.mHalfHeightOfCylinder <= 0.0f)
	{
		outResult.SetError("Invalid height");
		return;
	}

	if (inSettings.mRadius <= 0.0f)
	{
		outResult.SetError("Invalid radius");
		return;
	}

	outResult.Set(this);
}

class CapsuleShape::CapsuleNoConvex final : public Support
{
public:
					CapsuleNoConvex(Vec3Arg inHalfHeightOfCylinder, float inConvexRadius) :
		mHalfHeightOfCylinder(inHalfHeightOfCylinder),
		mConvexRadius(inConvexRadius)
	{
		static_assert(sizeof(CapsuleNoConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(CapsuleNoConvex)));
	}

	virtual Vec3	GetSupport(Vec3Arg inDirection) const override
	{
		if (inDirection.GetY() > 0)
			return mHalfHeightOfCylinder;
		else
			return -mHalfHeightOfCylinder;
	}

	virtual float	GetConvexRadius() const override
	{
		return mConvexRadius;
	}

private:
	Vec3			mHalfHeightOfCylinder;
	float			mConvexRadius;
};

class CapsuleShape::CapsuleWithConvex final : public Support
{
public:
					CapsuleWithConvex(Vec3Arg inHalfHeightOfCylinder, float inRadius) :
		mHalfHeightOfCylinder(inHalfHeightOfCylinder),
		mRadius(inRadius)
	{
		static_assert(sizeof(CapsuleWithConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(CapsuleWithConvex)));
	}

	virtual Vec3	GetSupport(Vec3Arg inDirection) const override
	{
		float len = inDirection.Length();
		Vec3 radius = len > 0.0f? inDirection * (mRadius / len) : Vec3::sZero();

		if (inDirection.GetY() > 0)
			return radius + mHalfHeightOfCylinder;
		else
			return radius - mHalfHeightOfCylinder;
	}

	virtual float	GetConvexRadius() const override
	{
		return 0.0f;
	}

private:
	Vec3			mHalfHeightOfCylinder;
	float			mRadius;
};

const ConvexShape::Support *CapsuleShape::GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	// Get scaled capsule
	Vec3 abs_scale = inScale.Abs();
	float scale = abs_scale.GetX();
	Vec3 scaled_half_height_of_cylinder = Vec3(0, scale * mHalfHeightOfCylinder, 0);
	float scaled_radius = scale * mRadius;

	switch (inMode)
	{
	case ESupportMode::IncludeConvexRadius:
		return new (&inBuffer) CapsuleWithConvex(scaled_half_height_of_cylinder, scaled_radius);

	case ESupportMode::ExcludeConvexRadius:
	case ESupportMode::Default:
		return new (&inBuffer) CapsuleNoConvex(scaled_half_height_of_cylinder, scaled_radius);
	}

	JPH_ASSERT(false);
	return nullptr;
}

void CapsuleShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");
	JPH_ASSERT(IsValidScale(inScale));

	// Get direction in horizontal plane
	Vec3 direction = inDirection;
	direction.SetComponent(1, 0.0f);

	// Check zero vector, in this case we're hitting from top/bottom so there's no supporting face
	float len = direction.Length();
	if (len == 0.0f)
		return;

	// Get scaled capsule
	Vec3 abs_scale = inScale.Abs();
	float scale = abs_scale.GetX();
	Vec3 scaled_half_height_of_cylinder = Vec3(0, scale * mHalfHeightOfCylinder, 0);
	float scaled_radius = scale * mRadius;

	// Get support point for top and bottom sphere in the opposite of 'direction' (including convex radius)
	Vec3 support = (scaled_radius / len) * direction;
	Vec3 support_top = scaled_half_height_of_cylinder - support;
	Vec3 support_bottom = -scaled_half_height_of_cylinder - support;

	// Get projection on inDirection
	// Note that inDirection is not normalized, so we need to divide by inDirection.Length() to get the actual projection
	// We've multiplied both sides of the if below with inDirection.Length()
	float proj_top = support_top.Dot(inDirection);
	float proj_bottom = support_bottom.Dot(inDirection);

	// If projection is roughly equal then return line, otherwise we return nothing as there's only 1 point
	if (abs(proj_top - proj_bottom) < cCapsuleProjectionSlop * inDirection.Length())
	{
		outVertices.push_back(inCenterOfMassTransform * support_top);
		outVertices.push_back(inCenterOfMassTransform * support_bottom);
	}
}

MassProperties CapsuleShape::GetMassProperties() const
{
	MassProperties p;

	float density = GetDensity();

	// Calculate inertia and mass according to:
	// https://www.gamedev.net/resources/_/technical/math-and-physics/capsule-inertia-tensor-r3856
	// Note that there is an error in eq 14, H^2/2 should be H^2/4 in Ixx and Izz, eq 12 does contain the correct value
	float radius_sq = Square(mRadius);
	float height = 2.0f * mHalfHeightOfCylinder;
	float cylinder_mass = JPH_PI * height * radius_sq * density;
	float hemisphere_mass = (2.0f * JPH_PI / 3.0f) * radius_sq * mRadius * density;

	// From cylinder
	float height_sq = Square(height);
	float inertia_y = radius_sq * cylinder_mass * 0.5f;
	float inertia_xz = inertia_y * 0.5f + cylinder_mass * height_sq / 12.0f;

	// From hemispheres
	float temp = hemisphere_mass * 4.0f * radius_sq / 5.0f;
	inertia_y += temp;
	inertia_xz += temp + hemisphere_mass * (0.5f * height_sq + (3.0f / 4.0f) * height * mRadius);

	// Mass is cylinder + hemispheres
	p.mMass = cylinder_mass + hemisphere_mass * 2.0f;

	// Set inertia
	p.mInertia = Mat44::sScale(Vec3(inertia_xz, inertia_y, inertia_xz));

	return p;
}

Vec3 CapsuleShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	if (inLocalSurfacePosition.GetY() > mHalfHeightOfCylinder)
		return (inLocalSurfacePosition - Vec3(0, mHalfHeightOfCylinder, 0)).Normalized();
	else if (inLocalSurfacePosition.GetY() < -mHalfHeightOfCylinder)
		return (inLocalSurfacePosition - Vec3(0, -mHalfHeightOfCylinder, 0)).Normalized();
	else
		return Vec3(inLocalSurfacePosition.GetX(), 0, inLocalSurfacePosition.GetZ()).NormalizedOr(Vec3::sAxisX());
}

AABox CapsuleShape::GetLocalBounds() const
{
	Vec3 extent = Vec3::sReplicate(mRadius) + Vec3(0, mHalfHeightOfCylinder, 0);
	return AABox(-extent, extent);
}

AABox CapsuleShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 abs_scale = inScale.Abs();
	float scale = abs_scale.GetX();
	Vec3 extent = Vec3::sReplicate(scale * mRadius);
	Vec3 height = Vec3(0, scale * mHalfHeightOfCylinder, 0);
	Vec3 p1 = inCenterOfMassTransform * -height;
	Vec3 p2 = inCenterOfMassTransform * height;
	return AABox(Vec3::sMin(p1, p2) - extent, Vec3::sMax(p1, p2) + extent);
}

#ifdef JPH_DEBUG_RENDERER
void CapsuleShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;
	inRenderer->DrawCapsule(inCenterOfMassTransform * Mat44::sScale(inScale.Abs().GetX()), mHalfHeightOfCylinder, mRadius, inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor, DebugRenderer::ECastShadow::On, draw_mode);
}
#endif // JPH_DEBUG_RENDERER

bool CapsuleShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	// Test ray against capsule
	float fraction = RayCapsule(inRay.mOrigin, inRay.mDirection, mHalfHeightOfCylinder, mRadius);
	if (fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void CapsuleShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	float radius_sq = Square(mRadius);

	// Get vertical distance to the top/bottom sphere centers
	float delta_y = abs(inPoint.GetY()) - mHalfHeightOfCylinder;

	// Get distance in horizontal plane
	float xz_sq = Square(inPoint.GetX()) + Square(inPoint.GetZ());

	// Check if the point is in one of the two spheres
	bool in_sphere = xz_sq + Square(delta_y) <= radius_sq;

	// Check if the point is in the cylinder in the middle
	bool in_cylinder = delta_y <= 0.0f && xz_sq <= radius_sq;

	if (in_sphere || in_cylinder)
		ioCollector.AddHit({ TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID() });
}

void CapsuleShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Mat44 inverse_transform = inCenterOfMassTransform.InversedRotationTranslation();

	// Get scaled capsule
	float scale = abs(inScale.GetX());
	float half_height_of_cylinder = scale * mHalfHeightOfCylinder;
	float radius = scale * mRadius;

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			// Calculate penetration
			Vec3 local_pos = inverse_transform * v.GetPosition();
			if (abs(local_pos.GetY()) <= half_height_of_cylinder)
			{
				// Near cylinder
				Vec3 normal = local_pos;
				normal.SetY(0.0f);
				float normal_length = normal.Length();
				float penetration = radius - normal_length;
				if (v.UpdatePenetration(penetration))
				{
					// Calculate contact point and normal
					normal = normal_length > 0.0f? normal / normal_length : Vec3::sAxisX();
					Vec3 point = radius * normal;

					// Store collision
					v.SetCollision(Plane::sFromPointAndNormal(point, normal).GetTransformed(inCenterOfMassTransform), inCollidingShapeIndex);
				}
			}
			else
			{
				// Near cap
				Vec3 center = Vec3(0, Sign(local_pos.GetY()) * half_height_of_cylinder, 0);
				Vec3 delta = local_pos - center;
				float distance = delta.Length();
				float penetration = radius - distance;
				if (v.UpdatePenetration(penetration))
				{
					// Calculate contact point and normal
					Vec3 normal = delta / distance;
					Vec3 point = center + radius * normal;

					// Store collision
					v.SetCollision(Plane::sFromPointAndNormal(point, normal).GetTransformed(inCenterOfMassTransform), inCollidingShapeIndex);
				}
			}
		}
}

void CapsuleShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 abs_scale = inScale.Abs();
	float scale = abs_scale.GetX();

	GetTrianglesContextMultiVertexList *context = new (&ioContext) GetTrianglesContextMultiVertexList(false, GetMaterial());

	Mat44 world_matrix = Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(scale);

	Mat44 top_matrix = world_matrix * Mat44(Vec4(mRadius, 0, 0, 0), Vec4(0, mRadius, 0, 0), Vec4(0, 0, mRadius, 0), Vec4(0, mHalfHeightOfCylinder, 0, 1));
	context->AddPart(top_matrix, sCapsuleTopTriangles.data(), sCapsuleTopTriangles.size());

	Mat44 middle_matrix = world_matrix * Mat44::sScale(Vec3(mRadius, mHalfHeightOfCylinder, mRadius));
	context->AddPart(middle_matrix, sCapsuleMiddleTriangles.data(), sCapsuleMiddleTriangles.size());

	Mat44 bottom_matrix = world_matrix * Mat44(Vec4(mRadius, 0, 0, 0), Vec4(0, mRadius, 0, 0), Vec4(0, 0, mRadius, 0), Vec4(0, -mHalfHeightOfCylinder, 0, 1));
	context->AddPart(bottom_matrix, sCapsuleBottomTriangles.data(), sCapsuleBottomTriangles.size());
}

int CapsuleShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	return ((GetTrianglesContextMultiVertexList &)ioContext).GetTrianglesNext(inMaxTrianglesRequested, outTriangleVertices, outMaterials);
}

void CapsuleShape::SaveBinaryState(StreamOut &inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mRadius);
	inStream.Write(mHalfHeightOfCylinder);
}

void CapsuleShape::RestoreBinaryState(StreamIn &inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mRadius);
	inStream.Read(mHalfHeightOfCylinder);
}

bool CapsuleShape::IsValidScale(Vec3Arg inScale) const
{
	return ConvexShape::IsValidScale(inScale) && ScaleHelpers::IsUniformScale(inScale.Abs());
}

Vec3 CapsuleShape::MakeScaleValid(Vec3Arg inScale) const
{
	Vec3 scale = ScaleHelpers::MakeNonZeroScale(inScale);

	return scale.GetSign() * ScaleHelpers::MakeUniformScale(scale.Abs());
}

void CapsuleShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Capsule);
	f.mConstruct = []() -> Shape * { return new CapsuleShape; };
	f.mColor = Color::sGreen;
}

JPH_NAMESPACE_END
