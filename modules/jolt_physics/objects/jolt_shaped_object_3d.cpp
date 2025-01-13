/**************************************************************************/
/*  jolt_shaped_object_3d.cpp                                             */
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

#include "jolt_shaped_object_3d.h"

#include "../misc/jolt_type_conversions.h"
#include "../shapes/jolt_custom_double_sided_shape.h"
#include "../shapes/jolt_shape_3d.h"
#include "../spaces/jolt_space_3d.h"

#include "Jolt/Physics/Collision/Shape/EmptyShape.h"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"

bool JoltShapedObject3D::_is_big() const {
	// This number is completely arbitrary, and mostly just needs to capture `WorldBoundaryShape3D`, which needs to be kept out of the normal broadphase layers.
	return get_aabb().get_longest_axis_size() >= 1000.0f;
}

JPH::ShapeRefC JoltShapedObject3D::_try_build_shape() {
	int built_shapes = 0;

	for (JoltShapeInstance3D &shape : shapes) {
		if (shape.is_enabled() && shape.try_build()) {
			built_shapes += 1;
		}
	}

	if (unlikely(built_shapes == 0)) {
		return nullptr;
	}

	JPH::ShapeRefC result = built_shapes == 1 ? _try_build_single_shape() : _try_build_compound_shape();
	if (unlikely(result == nullptr)) {
		return nullptr;
	}

	if (has_custom_center_of_mass()) {
		result = JoltShape3D::with_center_of_mass(result, get_center_of_mass_custom());
	}

	if (scale != Vector3(1, 1, 1)) {
		Vector3 actual_scale = scale;
		JOLT_ENSURE_SCALE_VALID(result, actual_scale, vformat("Failed to correctly scale body '%s'.", to_string()));
		result = JoltShape3D::with_scale(result, actual_scale);
	}

	if (is_area()) {
		result = JoltShape3D::with_double_sided(result, true);
	}

	return result;
}

JPH::ShapeRefC JoltShapedObject3D::_try_build_single_shape() {
	for (int shape_index = 0; shape_index < (int)shapes.size(); ++shape_index) {
		const JoltShapeInstance3D &sub_shape = shapes[shape_index];

		if (!sub_shape.is_enabled() || !sub_shape.is_built()) {
			continue;
		}

		JPH::ShapeRefC jolt_sub_shape = sub_shape.get_jolt_ref();

		Vector3 sub_shape_scale = sub_shape.get_scale();
		const Transform3D sub_shape_transform = sub_shape.get_transform_unscaled();

		if (sub_shape_scale != Vector3(1, 1, 1)) {
			JOLT_ENSURE_SCALE_VALID(jolt_sub_shape, sub_shape_scale, vformat("Failed to correctly scale shape at index %d in body '%s'.", shape_index, to_string()));
			jolt_sub_shape = JoltShape3D::with_scale(jolt_sub_shape, sub_shape_scale);
		}

		if (sub_shape_transform != Transform3D()) {
			jolt_sub_shape = JoltShape3D::with_basis_origin(jolt_sub_shape, sub_shape_transform.basis, sub_shape_transform.origin);
		}

		return jolt_sub_shape;
	}

	return nullptr;
}

JPH::ShapeRefC JoltShapedObject3D::_try_build_compound_shape() {
	JPH::StaticCompoundShapeSettings compound_shape_settings;

	for (int shape_index = 0; shape_index < (int)shapes.size(); ++shape_index) {
		const JoltShapeInstance3D &sub_shape = shapes[shape_index];

		if (!sub_shape.is_enabled() || !sub_shape.is_built()) {
			continue;
		}

		JPH::ShapeRefC jolt_sub_shape = sub_shape.get_jolt_ref();

		Vector3 sub_shape_scale = sub_shape.get_scale();
		const Transform3D sub_shape_transform = sub_shape.get_transform_unscaled();

		if (sub_shape_scale != Vector3(1, 1, 1)) {
			JOLT_ENSURE_SCALE_VALID(jolt_sub_shape, sub_shape_scale, vformat("Failed to correctly scale shape at index %d in body '%s'.", shape_index, to_string()));
			jolt_sub_shape = JoltShape3D::with_scale(jolt_sub_shape, sub_shape_scale);
		}

		compound_shape_settings.AddShape(to_jolt(sub_shape_transform.origin), to_jolt(sub_shape_transform.basis), jolt_sub_shape);
	}

	const JPH::ShapeSettings::ShapeResult shape_result = compound_shape_settings.Create();
	ERR_FAIL_COND_V_MSG(shape_result.HasError(), nullptr, vformat("Failed to create compound shape with sub-shape count '%d'. It returned the following error: '%s'.", (int)compound_shape_settings.mSubShapes.size(), to_godot(shape_result.GetError())));

	return shape_result.Get();
}

void JoltShapedObject3D::_shapes_changed() {
	_update_shape();
	_update_object_layer();
}

void JoltShapedObject3D::_space_changing() {
	JoltObject3D::_space_changing();

	if (space != nullptr) {
		const JoltWritableBody3D body = space->write_body(jolt_id);
		ERR_FAIL_COND(body.is_invalid());

		jolt_settings = new JPH::BodyCreationSettings(body->GetBodyCreationSettings());
	}
}

void JoltShapedObject3D::_update_shape() {
	if (!in_space()) {
		_shapes_built();
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	previous_jolt_shape = jolt_shape;
	jolt_shape = build_shape();

	if (jolt_shape == previous_jolt_shape) {
		return;
	}

	space->get_body_iface().SetShape(jolt_id, jolt_shape, false, JPH::EActivation::DontActivate);

	_shapes_built();
}

JoltShapedObject3D::JoltShapedObject3D(ObjectType p_object_type) :
		JoltObject3D(p_object_type) {
	jolt_settings->mAllowSleeping = true;
	jolt_settings->mFriction = 1.0f;
	jolt_settings->mRestitution = 0.0f;
	jolt_settings->mLinearDamping = 0.0f;
	jolt_settings->mAngularDamping = 0.0f;
	jolt_settings->mGravityFactor = 0.0f;
}

JoltShapedObject3D::~JoltShapedObject3D() {
	if (jolt_settings != nullptr) {
		delete jolt_settings;
		jolt_settings = nullptr;
	}
}

Transform3D JoltShapedObject3D::get_transform_unscaled() const {
	if (!in_space()) {
		return Transform3D(to_godot(jolt_settings->mRotation), to_godot(jolt_settings->mPosition));
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Transform3D());

	return Transform3D(to_godot(body->GetRotation()), to_godot(body->GetPosition()));
}

Transform3D JoltShapedObject3D::get_transform_scaled() const {
	return get_transform_unscaled().scaled_local(scale);
}

Basis JoltShapedObject3D::get_basis() const {
	if (!in_space()) {
		return to_godot(jolt_settings->mRotation);
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Basis());

	return to_godot(body->GetRotation());
}

Vector3 JoltShapedObject3D::get_position() const {
	if (!in_space()) {
		return to_godot(jolt_settings->mPosition);
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	return to_godot(body->GetPosition());
}

Vector3 JoltShapedObject3D::get_center_of_mass() const {
	ERR_FAIL_NULL_V_MSG(space, Vector3(), vformat("Failed to retrieve center-of-mass of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	return to_godot(body->GetCenterOfMassPosition());
}

Vector3 JoltShapedObject3D::get_center_of_mass_relative() const {
	return get_center_of_mass() - get_position();
}

Vector3 JoltShapedObject3D::get_center_of_mass_local() const {
	ERR_FAIL_NULL_V_MSG(space, Vector3(), vformat("Failed to retrieve local center-of-mass of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	return get_transform_scaled().xform_inv(get_center_of_mass());
}

Vector3 JoltShapedObject3D::get_linear_velocity() const {
	if (!in_space()) {
		return to_godot(jolt_settings->mLinearVelocity);
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	return to_godot(body->GetLinearVelocity());
}

Vector3 JoltShapedObject3D::get_angular_velocity() const {
	if (!in_space()) {
		return to_godot(jolt_settings->mAngularVelocity);
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	return to_godot(body->GetAngularVelocity());
}

AABB JoltShapedObject3D::get_aabb() const {
	AABB result;

	for (const JoltShapeInstance3D &shape : shapes) {
		if (shape.is_disabled()) {
			continue;
		}

		if (result == AABB()) {
			result = shape.get_aabb();
		} else {
			result.merge_with(shape.get_aabb());
		}
	}

	return get_transform_scaled().xform(result);
}

JPH::ShapeRefC JoltShapedObject3D::build_shape() {
	JPH::ShapeRefC new_shape = _try_build_shape();

	if (new_shape == nullptr) {
		if (has_custom_center_of_mass()) {
			new_shape = JPH::EmptyShapeSettings(to_jolt(get_center_of_mass_custom())).Create().Get();
		} else {
			new_shape = new JPH::EmptyShape();
		}
	}

	return new_shape;
}

void JoltShapedObject3D::add_shape(JoltShape3D *p_shape, Transform3D p_transform, bool p_disabled) {
	JOLT_ENSURE_SCALE_NOT_ZERO(p_transform, vformat("An invalid transform was passed when adding shape at index %d to physics body '%s'.", shapes.size(), to_string()));

	shapes.push_back(JoltShapeInstance3D(this, p_shape, p_transform.orthonormalized(), p_transform.basis.get_scale(), p_disabled));

	_shapes_changed();
}

void JoltShapedObject3D::remove_shape(const JoltShape3D *p_shape) {
	for (int i = shapes.size() - 1; i >= 0; i--) {
		if (shapes[i].get_shape() == p_shape) {
			shapes.remove_at(i);
		}
	}

	_shapes_changed();
}

void JoltShapedObject3D::remove_shape(int p_index) {
	ERR_FAIL_INDEX(p_index, (int)shapes.size());
	shapes.remove_at(p_index);

	_shapes_changed();
}

JoltShape3D *JoltShapedObject3D::get_shape(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)shapes.size(), nullptr);
	return shapes[p_index].get_shape();
}

void JoltShapedObject3D::set_shape(int p_index, JoltShape3D *p_shape) {
	ERR_FAIL_INDEX(p_index, (int)shapes.size());
	shapes[p_index] = JoltShapeInstance3D(this, p_shape);

	_shapes_changed();
}

void JoltShapedObject3D::clear_shapes() {
	shapes.clear();

	_shapes_changed();
}

int JoltShapedObject3D::find_shape_index(uint32_t p_shape_instance_id) const {
	for (int i = 0; i < (int)shapes.size(); ++i) {
		if (shapes[i].get_id() == p_shape_instance_id) {
			return i;
		}
	}

	return -1;
}

int JoltShapedObject3D::find_shape_index(const JPH::SubShapeID &p_sub_shape_id) const {
	ERR_FAIL_NULL_V(jolt_shape, -1);
	return find_shape_index((uint32_t)jolt_shape->GetSubShapeUserData(p_sub_shape_id));
}

JoltShape3D *JoltShapedObject3D::find_shape(uint32_t p_shape_instance_id) const {
	const int shape_index = find_shape_index(p_shape_instance_id);
	return shape_index != -1 ? shapes[shape_index].get_shape() : nullptr;
}

JoltShape3D *JoltShapedObject3D::find_shape(const JPH::SubShapeID &p_sub_shape_id) const {
	const int shape_index = find_shape_index(p_sub_shape_id);
	return shape_index != -1 ? shapes[shape_index].get_shape() : nullptr;
}

Transform3D JoltShapedObject3D::get_shape_transform_unscaled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)shapes.size(), Transform3D());
	return shapes[p_index].get_transform_unscaled();
}

Transform3D JoltShapedObject3D::get_shape_transform_scaled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)shapes.size(), Transform3D());
	return shapes[p_index].get_transform_scaled();
}

void JoltShapedObject3D::set_shape_transform(int p_index, Transform3D p_transform) {
	ERR_FAIL_INDEX(p_index, (int)shapes.size());
	JOLT_ENSURE_SCALE_NOT_ZERO(p_transform, "Failed to correctly set transform for shape at index %d in body '%s'.");

	Vector3 new_scale = p_transform.basis.get_scale();
	p_transform.basis.orthonormalize();

	JoltShapeInstance3D &shape = shapes[p_index];

	if (shape.get_transform_unscaled() == p_transform && shape.get_scale() == new_scale) {
		return;
	}

	shape.set_transform(p_transform);
	shape.set_scale(new_scale);

	_shapes_changed();
}

Vector3 JoltShapedObject3D::get_shape_scale(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)shapes.size(), Vector3());
	return shapes[p_index].get_scale();
}

bool JoltShapedObject3D::is_shape_disabled(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)shapes.size(), false);
	return shapes[p_index].is_disabled();
}

void JoltShapedObject3D::set_shape_disabled(int p_index, bool p_disabled) {
	ERR_FAIL_INDEX(p_index, (int)shapes.size());

	JoltShapeInstance3D &shape = shapes[p_index];

	if (shape.is_disabled() == p_disabled) {
		return;
	}

	if (p_disabled) {
		shape.disable();
	} else {
		shape.enable();
	}

	_shapes_changed();
}

void JoltShapedObject3D::post_step(float p_step, JPH::Body &p_jolt_body) {
	JoltObject3D::post_step(p_step, p_jolt_body);

	previous_jolt_shape = nullptr;
}
