/**************************************************************************/
/*  jolt_custom_user_data_shape.cpp                                       */
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

#include "jolt_custom_user_data_shape.h"

#include "core/error/error_macros.h"

#include "Jolt/Physics/Collision/CollisionDispatch.h"
#include "Jolt/Physics/Collision/ShapeCast.h"

namespace {

JPH::Shape *construct_override_user_data() {
	return new JoltCustomUserDataShape();
}

void collide_override_user_data_vs_shape(const JPH::Shape *p_shape1, const JPH::Shape *p_shape2, JPH::Vec3Arg p_scale1, JPH::Vec3Arg p_scale2, JPH::Mat44Arg p_center_of_mass_transform1, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, const JPH::CollideShapeSettings &p_collide_shape_settings, JPH::CollideShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter) {
	ERR_FAIL_COND(p_shape1->GetSubType() != JoltCustomShapeSubType::OVERRIDE_USER_DATA);

	const JoltCustomUserDataShape *shape1 = static_cast<const JoltCustomUserDataShape *>(p_shape1);

	JPH::CollisionDispatch::sCollideShapeVsShape(shape1->GetInnerShape(), p_shape2, p_scale1, p_scale2, p_center_of_mass_transform1, p_center_of_mass_transform2, p_sub_shape_id_creator1, p_sub_shape_id_creator2, p_collide_shape_settings, p_collector, p_shape_filter);
}

void collide_shape_vs_override_user_data(const JPH::Shape *p_shape1, const JPH::Shape *p_shape2, JPH::Vec3Arg p_scale1, JPH::Vec3Arg p_scale2, JPH::Mat44Arg p_center_of_mass_transform1, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, const JPH::CollideShapeSettings &p_collide_shape_settings, JPH::CollideShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter) {
	ERR_FAIL_COND(p_shape2->GetSubType() != JoltCustomShapeSubType::OVERRIDE_USER_DATA);

	const JoltCustomUserDataShape *shape2 = static_cast<const JoltCustomUserDataShape *>(p_shape2);

	JPH::CollisionDispatch::sCollideShapeVsShape(p_shape1, shape2->GetInnerShape(), p_scale1, p_scale2, p_center_of_mass_transform1, p_center_of_mass_transform2, p_sub_shape_id_creator1, p_sub_shape_id_creator2, p_collide_shape_settings, p_collector, p_shape_filter);
}

void cast_override_user_data_vs_shape(const JPH::ShapeCast &p_shape_cast, const JPH::ShapeCastSettings &p_shape_cast_settings, const JPH::Shape *p_shape, JPH::Vec3Arg p_scale, const JPH::ShapeFilter &p_shape_filter, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, JPH::CastShapeCollector &p_collector) {
	ERR_FAIL_COND(p_shape_cast.mShape->GetSubType() != JoltCustomShapeSubType::OVERRIDE_USER_DATA);

	const JoltCustomUserDataShape *shape = static_cast<const JoltCustomUserDataShape *>(p_shape_cast.mShape);
	const JPH::ShapeCast shape_cast(shape->GetInnerShape(), p_shape_cast.mScale, p_shape_cast.mCenterOfMassStart, p_shape_cast.mDirection);

	JPH::CollisionDispatch::sCastShapeVsShapeLocalSpace(shape_cast, p_shape_cast_settings, p_shape, p_scale, p_shape_filter, p_center_of_mass_transform2, p_sub_shape_id_creator1, p_sub_shape_id_creator2, p_collector);
}

void cast_shape_vs_override_user_data(const JPH::ShapeCast &p_shape_cast, const JPH::ShapeCastSettings &p_shape_cast_settings, const JPH::Shape *p_shape, JPH::Vec3Arg p_scale, const JPH::ShapeFilter &p_shape_filter, JPH::Mat44Arg p_center_of_mass_transform2, const JPH::SubShapeIDCreator &p_sub_shape_id_creator1, const JPH::SubShapeIDCreator &p_sub_shape_id_creator2, JPH::CastShapeCollector &p_collector) {
	ERR_FAIL_COND(p_shape->GetSubType() != JoltCustomShapeSubType::OVERRIDE_USER_DATA);

	const JoltCustomUserDataShape *shape = static_cast<const JoltCustomUserDataShape *>(p_shape);

	JPH::CollisionDispatch::sCastShapeVsShapeLocalSpace(p_shape_cast, p_shape_cast_settings, shape->GetInnerShape(), p_scale, p_shape_filter, p_center_of_mass_transform2, p_sub_shape_id_creator1, p_sub_shape_id_creator2, p_collector);
}

} // namespace

JPH::ShapeSettings::ShapeResult JoltCustomUserDataShapeSettings::Create() const {
	if (mCachedResult.IsEmpty()) {
		new JoltCustomUserDataShape(*this, mCachedResult);
	}

	return mCachedResult;
}

void JoltCustomUserDataShape::register_type() {
	JPH::ShapeFunctions &shape_functions = JPH::ShapeFunctions::sGet(JoltCustomShapeSubType::OVERRIDE_USER_DATA);

	shape_functions.mConstruct = construct_override_user_data;
	shape_functions.mColor = JPH::Color::sCyan;

	for (const JPH::EShapeSubType sub_type : JPH::sAllSubShapeTypes) {
		JPH::CollisionDispatch::sRegisterCollideShape(JoltCustomShapeSubType::OVERRIDE_USER_DATA, sub_type, collide_override_user_data_vs_shape);
		JPH::CollisionDispatch::sRegisterCollideShape(sub_type, JoltCustomShapeSubType::OVERRIDE_USER_DATA, collide_shape_vs_override_user_data);
		JPH::CollisionDispatch::sRegisterCastShape(JoltCustomShapeSubType::OVERRIDE_USER_DATA, sub_type, cast_override_user_data_vs_shape);
		JPH::CollisionDispatch::sRegisterCastShape(sub_type, JoltCustomShapeSubType::OVERRIDE_USER_DATA, cast_shape_vs_override_user_data);
	}
}
