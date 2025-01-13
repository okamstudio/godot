/**************************************************************************/
/*  jolt_custom_ray_shape.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "jolt_custom_ray_shape.h"

#include "../spaces/jolt_query_collectors.h"

#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/TransformedShape.h"

#ifdef JPH_DEBUG_RENDERER
#include "Jolt/Renderer/DebugRenderer.h"
#endif

namespace {

class JoltCustomRayShapeSupport final : public JPH::ConvexShape::Support {
public:
	explicit JoltCustomRayShapeSupport(float p_length) :
			length(p_length) {}

	virtual JPH::Vec3 GetSupport(JPH::Vec3Arg p_direction) const override {
		if (p_direction.GetZ() > 0.0f) {
			return JPH::Vec3(0.0f, 0.0f, length);
		} else {
			return JPH::Vec3::sZero();
		}
	}

	virtual float GetConvexRadius() const override { return 0.0f; }

private:
	float length = 0.0f;
};

static_assert(sizeof(JoltCustomRayShapeSupport) <= sizeof(JPH::ConvexShape::SupportBuffer), "Size of SeparationRayShape3D support is larger than size of support buffer.");

JPH::Shape *construct_ray() {
	return new JoltCustomRayShape();
}

void collide_ray_vs_shape(const JPH::Shape *p_shape1, const JPH::Shape *p_shape2, JPH::Vec3Arg p_scale1, JPH::Vec3Arg p_scale2, JPH::Mat44Arg p_center_of_mass_transform1, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, const JPH::CollideShapeSettings &p_collide_shape_settings, JPH::CollideShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter) {
	ERR_FAIL_COND(p_shape1->GetSubType() != JoltCustomShapeSubType::RAY);

	const JoltCustomRayShape *shape1 = static_cast<const JoltCustomRayShape *>(p_shape1);

	const float margin = p_collide_shape_settings.mMaxSeparationDistance;
	const float ray_length = shape1->length;
	const float ray_length_padded = ray_length + margin;

	const JPH::Mat44 transform1 = p_center_of_mass_transform1 * JPH::Mat44::sScale(p_scale1);
	const JPH::Mat44 transform2 = p_center_of_mass_transform2 * JPH::Mat44::sScale(p_scale2);
	const JPH::Mat44 transform_inv2 = transform2.Inversed();

	const JPH::Vec3 ray_start = transform1.GetTranslation();
	const JPH::Vec3 ray_direction = transform1.GetAxisZ();
	const JPH::Vec3 ray_vector = ray_direction * ray_length;
	const JPH::Vec3 ray_vector_padded = ray_direction * ray_length_padded;

	const JPH::Vec3 ray_start2 = transform_inv2 * ray_start;
	const JPH::Vec3 ray_direction2 = transform_inv2.Multiply3x3(ray_direction);
	const JPH::Vec3 ray_vector_padded2 = transform_inv2.Multiply3x3(ray_vector_padded);

	const JPH::RayCast ray_cast(ray_start2, ray_vector_padded2);

	JPH::RayCastSettings ray_cast_settings;
	ray_cast_settings.mTreatConvexAsSolid = false;
	ray_cast_settings.mBackFaceModeTriangles = p_collide_shape_settings.mBackFaceMode;

	JoltQueryCollectorClosest<JPH::CastRayCollector> ray_collector;

	p_shape2->CastRay(ray_cast, ray_cast_settings, p_sub_shape_id_creator2, ray_collector);

	if (!ray_collector.had_hit()) {
		return;
	}

	const JPH::RayCastResult &hit = ray_collector.get_hit();

	const float hit_distance = ray_length_padded * hit.mFraction;
	const float hit_depth = ray_length - hit_distance;

	if (-hit_depth >= p_collector.GetEarlyOutFraction()) {
		return;
	}

	// Since `hit.mSubShapeID2` could represent a path not only from `p_shape2` but also any
	// compound shape that it's contained within, we need to split this path into something that
	// `p_shape2` can actually understand.
	JPH::SubShapeID local_sub_shape_id2;
	hit.mSubShapeID2.PopID(p_sub_shape_id_creator2.GetNumBitsWritten(), local_sub_shape_id2);

	const JPH::Vec3 hit_point2 = ray_cast.GetPointOnRay(hit.mFraction);

	const JPH::Vec3 hit_point_on_1 = ray_start + ray_vector;
	const JPH::Vec3 hit_point_on_2 = transform2 * hit_point2;

	JPH::Vec3 hit_normal2 = JPH::Vec3::sZero();

	if (shape1->slide_on_slope) {
		hit_normal2 = p_shape2->GetSurfaceNormal(local_sub_shape_id2, hit_point2);

		// If we got a back-face normal we need to flip it.
		if (hit_normal2.Dot(ray_direction2) > 0) {
			hit_normal2 = -hit_normal2;
		}
	} else {
		hit_normal2 = -ray_direction2;
	}

	const JPH::Vec3 hit_normal = transform2.Multiply3x3(hit_normal2);

	JPH::CollideShapeResult result(hit_point_on_1, hit_point_on_2, -hit_normal, hit_depth, p_sub_shape_id_creator1.GetID(), hit.mSubShapeID2, JPH::TransformedShape::sGetBodyID(p_collector.GetContext()));

	if (p_collide_shape_settings.mCollectFacesMode == JPH::ECollectFacesMode::CollectFaces) {
		p_shape2->GetSupportingFace(local_sub_shape_id2, ray_direction2, p_scale2, p_center_of_mass_transform2, result.mShape2Face);
	}

	p_collector.AddHit(result);
}

void collide_noop(const JPH::Shape *p_shape1, const JPH::Shape *p_shape2, JPH::Vec3Arg p_scale1, JPH::Vec3Arg p_scale2, JPH::Mat44Arg p_center_of_mass_transform1, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, const JPH::CollideShapeSettings &p_collide_shape_settings, JPH::CollideShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter) {
}

void cast_noop(const JPH::ShapeCast &p_shape_cast, const JPH::ShapeCastSettings &p_shape_cast_settings, const JPH::Shape *p_shape, JPH::Vec3Arg p_scale, const JPH::ShapeFilter &p_shape_filter, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, JPH::CastShapeCollector &p_collector) {
}

} // namespace

JPH::ShapeSettings::ShapeResult JoltCustomRayShapeSettings::Create() const {
	if (mCachedResult.IsEmpty()) {
		new JoltCustomRayShape(*this, mCachedResult);
	}

	return mCachedResult;
}

void JoltCustomRayShape::register_type() {
	JPH::ShapeFunctions &shape_functions = JPH::ShapeFunctions::sGet(JoltCustomShapeSubType::RAY);

	shape_functions.mConstruct = construct_ray;
	shape_functions.mColor = JPH::Color::sDarkRed;

	static constexpr JPH::EShapeSubType concrete_sub_types[] = {
		JPH::EShapeSubType::Sphere,
		JPH::EShapeSubType::Box,
		JPH::EShapeSubType::Triangle,
		JPH::EShapeSubType::Capsule,
		JPH::EShapeSubType::TaperedCapsule,
		JPH::EShapeSubType::Cylinder,
		JPH::EShapeSubType::ConvexHull,
		JPH::EShapeSubType::Mesh,
		JPH::EShapeSubType::HeightField,
		JPH::EShapeSubType::Plane,
		JPH::EShapeSubType::TaperedCylinder
	};

	for (const JPH::EShapeSubType concrete_sub_type : concrete_sub_types) {
		JPH::CollisionDispatch::sRegisterCollideShape(JoltCustomShapeSubType::RAY, concrete_sub_type, collide_ray_vs_shape);
		JPH::CollisionDispatch::sRegisterCollideShape(concrete_sub_type, JoltCustomShapeSubType::RAY, JPH::CollisionDispatch::sReversedCollideShape);
	}

	JPH::CollisionDispatch::sRegisterCollideShape(JoltCustomShapeSubType::RAY, JoltCustomShapeSubType::RAY, collide_noop);

	for (const JPH::EShapeSubType sub_type : JPH::sAllSubShapeTypes) {
		JPH::CollisionDispatch::sRegisterCastShape(JoltCustomShapeSubType::RAY, sub_type, cast_noop);
		JPH::CollisionDispatch::sRegisterCastShape(sub_type, JoltCustomShapeSubType::RAY, cast_noop);
	}
}

JPH::AABox JoltCustomRayShape::GetLocalBounds() const {
	const float radius = GetInnerRadius();
	return JPH::AABox(JPH::Vec3(-radius, -radius, 0.0f), JPH::Vec3(radius, radius, length));
}

float JoltCustomRayShape::GetInnerRadius() const {
	// There is no sensible value here, since this shape is infinitely thin, so we pick something
	// that's hopefully small enough to effectively be zero, but big enough to not cause any
	// numerical issues.
	return 0.0001f;
}

JPH::MassProperties JoltCustomRayShape::GetMassProperties() const {
	JPH::MassProperties mass_properties;

	// Since this shape has no volume we can't really give it a correct set of mass properties, so
	// instead we just give it some arbitrary ones.
	mass_properties.mMass = 1.0f;
	mass_properties.mInertia = JPH::Mat44::sIdentity();

	return mass_properties;
}

#ifdef JPH_DEBUG_RENDERER

void JoltCustomRayShape::Draw(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_use_material_colors, bool p_draw_wireframe) const {
	p_renderer->DrawArrow(p_center_of_mass_transform.GetTranslation(), p_center_of_mass_transform * JPH::Vec3(0, 0, length * p_scale.GetZ()), p_use_material_colors ? GetMaterial()->GetDebugColor() : p_color, 0.1f);
}

#endif

const JPH::ConvexShape::Support *JoltCustomRayShape::GetSupportFunction(JPH::ConvexShape::ESupportMode p_mode, JPH::ConvexShape::SupportBuffer &p_buffer, JPH::Vec3Arg p_scale) const {
	return new (&p_buffer) JoltCustomRayShapeSupport(p_scale.GetZ() * length);
}
