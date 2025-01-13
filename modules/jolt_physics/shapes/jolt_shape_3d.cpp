/**************************************************************************/
/*  jolt_shape_3d.cpp                                                     */
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

#include "jolt_shape_3d.h"

#include "../misc/jolt_type_conversions.h"
#include "../objects/jolt_shaped_object_3d.h"
#include "jolt_custom_double_sided_shape.h"
#include "jolt_custom_user_data_shape.h"

#include "Jolt/Physics/Collision/Shape/MutableCompoundShape.h"
#include "Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"

namespace {

constexpr float DEFAULT_SOLVER_BIAS = 0.0;

} // namespace

String JoltShape3D::_owners_to_string() const {
	const int owner_count = ref_counts_by_owner.size();

	if (owner_count == 0) {
		return "'<unknown>' and 0 other object(s)";
	}

	const JoltShapedObject3D &random_owner = *ref_counts_by_owner.begin()->key;

	return vformat("'%s' and %d other object(s)", random_owner.to_string(), owner_count - 1);
}

JoltShape3D::~JoltShape3D() = default;

void JoltShape3D::add_owner(JoltShapedObject3D *p_owner) {
	ref_counts_by_owner[p_owner]++;
}

void JoltShape3D::remove_owner(JoltShapedObject3D *p_owner) {
	if (--ref_counts_by_owner[p_owner] <= 0) {
		ref_counts_by_owner.erase(p_owner);
	}
}

void JoltShape3D::remove_self() {
	// `remove_owner` will be called when we `remove_shape`, so we need to copy the map since the
	// iterator would be invalidated from underneath us.
	const HashMap<JoltShapedObject3D *, int> ref_counts_by_owner_copy = ref_counts_by_owner;

	for (const KeyValue<JoltShapedObject3D *, int> &E : ref_counts_by_owner_copy) {
		E.key->remove_shape(this);
	}
}

float JoltShape3D::get_solver_bias() const {
	return DEFAULT_SOLVER_BIAS;
}

void JoltShape3D::set_solver_bias(float p_bias) {
	if (!Math::is_equal_approx(p_bias, DEFAULT_SOLVER_BIAS)) {
		WARN_PRINT(vformat("Custom solver bias for shapes is not supported when using Jolt Physics. Any such value will be ignored. This shape belongs to %s.", _owners_to_string()));
	}
}

JPH::ShapeRefC JoltShape3D::try_build() {
	jolt_ref_mutex.lock();

	if (jolt_ref == nullptr) {
		jolt_ref = _build();
	}

	jolt_ref_mutex.unlock();

	return jolt_ref;
}

void JoltShape3D::destroy() {
	jolt_ref_mutex.lock();
	jolt_ref = nullptr;
	jolt_ref_mutex.unlock();

	for (const KeyValue<JoltShapedObject3D *, int> &E : ref_counts_by_owner) {
		E.key->_shapes_changed();
	}
}

JPH::ShapeRefC JoltShape3D::with_scale(const JPH::Shape *p_shape, const Vector3 &p_scale) {
	ERR_FAIL_NULL_V(p_shape, nullptr);

	const JPH::ScaledShapeSettings shape_settings(p_shape, to_jolt(p_scale));
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to scale shape with {scale=%v}. It returned the following error: '%s'.", p_scale, to_godot(shape_result.GetError())));

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_basis_origin(const JPH::Shape *p_shape, const Basis &p_basis, const Vector3 &p_origin) {
	ERR_FAIL_NULL_V(p_shape, nullptr);

	const JPH::RotatedTranslatedShapeSettings shape_settings(to_jolt(p_origin), to_jolt(p_basis), p_shape);

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to offset shape with {basis=%s origin=%v}. It returned the following error: '%s'.", p_basis, p_origin, to_godot(shape_result.GetError())));

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_center_of_mass_offset(const JPH::Shape *p_shape, const Vector3 &p_offset) {
	ERR_FAIL_NULL_V(p_shape, nullptr);

	const JPH::OffsetCenterOfMassShapeSettings shape_settings(to_jolt(p_offset), p_shape);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to offset center of mass with {offset=%v}. It returned the following error: '%s'.", p_offset, to_godot(shape_result.GetError())));

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_center_of_mass(const JPH::Shape *p_shape, const Vector3 &p_center_of_mass) {
	ERR_FAIL_NULL_V(p_shape, nullptr);

	const Vector3 center_of_mass_inner = to_godot(p_shape->GetCenterOfMass());
	const Vector3 center_of_mass_offset = p_center_of_mass - center_of_mass_inner;

	if (center_of_mass_offset == Vector3()) {
		return p_shape;
	}

	return with_center_of_mass_offset(p_shape, center_of_mass_offset);
}

JPH::ShapeRefC JoltShape3D::with_user_data(const JPH::Shape *p_shape, uint64_t p_user_data) {
	JoltCustomUserDataShapeSettings shape_settings(p_shape);
	shape_settings.mUserData = (JPH::uint64)p_user_data;

	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to override user data. It returned the following error: '%s'.", to_godot(shape_result.GetError())));

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::with_double_sided(const JPH::Shape *p_shape, bool p_back_face_collision) {
	ERR_FAIL_NULL_V(p_shape, nullptr);

	const JoltCustomDoubleSidedShapeSettings shape_settings(p_shape, p_back_face_collision);
	const JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to make shape double-sided. It returned the following error: '%s'.", to_godot(shape_result.GetError())));

	return shape_result.Get();
}

JPH::ShapeRefC JoltShape3D::without_custom_shapes(const JPH::Shape *p_shape) {
	switch (p_shape->GetSubType()) {
		case JoltCustomShapeSubType::RAY:
		case JoltCustomShapeSubType::MOTION: {
			// Replace unsupported shapes with a small sphere.
			return new JPH::SphereShape(0.1f);
		}

		case JoltCustomShapeSubType::OVERRIDE_USER_DATA:
		case JoltCustomShapeSubType::DOUBLE_SIDED: {
			const JPH::DecoratedShape *shape = static_cast<const JPH::DecoratedShape *>(p_shape);

			// Replace unsupported decorator shapes with the inner shape.
			return without_custom_shapes(shape->GetInnerShape());
		}

		case JPH::EShapeSubType::StaticCompound: {
			const JPH::StaticCompoundShape *shape = static_cast<const JPH::StaticCompoundShape *>(p_shape);

			JPH::StaticCompoundShapeSettings settings;

			for (const JPH::CompoundShape::SubShape &sub_shape : shape->GetSubShapes()) {
				settings.AddShape(shape->GetCenterOfMass() + sub_shape.GetPositionCOM() - sub_shape.GetRotation() * sub_shape.mShape->GetCenterOfMass(), sub_shape.GetRotation(), without_custom_shapes(sub_shape.mShape));
			}

			const JPH::ShapeSettings::ShapeResult shape_result = settings.Create();
			ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to recreate static compound shape during filtering of custom shapes. It returned the following error: '%s'.", to_godot(shape_result.GetError())));

			return shape_result.Get();
		}

		case JPH::EShapeSubType::MutableCompound: {
			const JPH::MutableCompoundShape *shape = static_cast<const JPH::MutableCompoundShape *>(p_shape);

			JPH::MutableCompoundShapeSettings settings;

			for (const JPH::MutableCompoundShape::SubShape &sub_shape : shape->GetSubShapes()) {
				settings.AddShape(shape->GetCenterOfMass() + sub_shape.GetPositionCOM() - sub_shape.GetRotation() * sub_shape.mShape->GetCenterOfMass(), sub_shape.GetRotation(), without_custom_shapes(sub_shape.mShape));
			}

			const JPH::ShapeSettings::ShapeResult shape_result = settings.Create();
			ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to recreate mutable compound shape during filtering of custom shapes. It returned the following error: '%s'.", to_godot(shape_result.GetError())));

			return shape_result.Get();
		}

		case JPH::EShapeSubType::RotatedTranslated: {
			const JPH::RotatedTranslatedShape *shape = static_cast<const JPH::RotatedTranslatedShape *>(p_shape);

			const JPH::Shape *inner_shape = shape->GetInnerShape();
			const JPH::ShapeRefC new_inner_shape = without_custom_shapes(inner_shape);

			if (inner_shape == new_inner_shape) {
				return p_shape;
			}

			return new JPH::RotatedTranslatedShape(shape->GetPosition(), shape->GetRotation(), new_inner_shape);
		}

		case JPH::EShapeSubType::Scaled: {
			const JPH::ScaledShape *shape = static_cast<const JPH::ScaledShape *>(p_shape);

			const JPH::Shape *inner_shape = shape->GetInnerShape();
			const JPH::ShapeRefC new_inner_shape = without_custom_shapes(inner_shape);

			if (inner_shape == new_inner_shape) {
				return p_shape;
			}

			return new JPH::ScaledShape(new_inner_shape, shape->GetScale());
		}

		case JPH::EShapeSubType::OffsetCenterOfMass: {
			const JPH::OffsetCenterOfMassShape *shape = static_cast<const JPH::OffsetCenterOfMassShape *>(p_shape);

			const JPH::Shape *inner_shape = shape->GetInnerShape();
			const JPH::ShapeRefC new_inner_shape = without_custom_shapes(inner_shape);

			if (inner_shape == new_inner_shape) {
				return p_shape;
			}

			return new JPH::OffsetCenterOfMassShape(new_inner_shape, shape->GetOffset());
		}

		default: {
			return p_shape;
		}
	}
}

Vector3 JoltShape3D::make_scale_valid(const JPH::Shape *p_shape, const Vector3 &p_scale) {
	return to_godot(p_shape->MakeScaleValid(to_jolt(p_scale)));
}

bool JoltShape3D::is_scale_valid(const Vector3 &p_scale, const Vector3 &p_valid_scale, real_t p_tolerance) {
	return Math::is_equal_approx(p_scale.x, p_valid_scale.x, p_tolerance) && Math::is_equal_approx(p_scale.y, p_valid_scale.y, p_tolerance) && Math::is_equal_approx(p_scale.z, p_valid_scale.z, p_tolerance);
}
