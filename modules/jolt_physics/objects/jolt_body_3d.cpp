/**************************************************************************/
/*  jolt_body_3d.cpp                                                      */
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

#include "jolt_body_3d.h"

#include "../joints/jolt_joint_3d.h"
#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"
#include "../shapes/jolt_shape_3d.h"
#include "../spaces/jolt_broad_phase_layer.h"
#include "../spaces/jolt_space_3d.h"
#include "jolt_area_3d.h"
#include "jolt_group_filter.h"
#include "jolt_physics_direct_body_state_3d.h"
#include "jolt_soft_body_3d.h"

namespace {

template <typename TValue, typename TGetter>
bool integrate(TValue &p_value, PhysicsServer3D::AreaSpaceOverrideMode p_mode, TGetter &&p_getter) {
	switch (p_mode) {
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED: {
			return false;
		}
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE: {
			p_value += p_getter();
			return false;
		}
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE: {
			p_value += p_getter();
			return true;
		}
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE: {
			p_value = p_getter();
			return true;
		}
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE: {
			p_value = p_getter();
			return false;
		}
		default: {
			ERR_FAIL_V_MSG(false, vformat("Unhandled override mode: '%d'. This should not happen. Please report this.", p_mode));
		}
	}
}

} // namespace

JPH::BroadPhaseLayer JoltBody3D::_get_broad_phase_layer() const {
	switch (mode) {
		case PhysicsServer3D::BODY_MODE_STATIC: {
			return _is_big() ? JoltBroadPhaseLayer::BODY_STATIC_BIG : JoltBroadPhaseLayer::BODY_STATIC;
		}
		case PhysicsServer3D::BODY_MODE_KINEMATIC:
		case PhysicsServer3D::BODY_MODE_RIGID:
		case PhysicsServer3D::BODY_MODE_RIGID_LINEAR: {
			return JoltBroadPhaseLayer::BODY_DYNAMIC;
		}
		default: {
			ERR_FAIL_V_MSG(JoltBroadPhaseLayer::BODY_STATIC, vformat("Unhandled body mode: '%d'. This should not happen. Please report this.", mode));
		}
	}
}

JPH::ObjectLayer JoltBody3D::_get_object_layer() const {
	ERR_FAIL_NULL_V(space, 0);

	return space->map_to_object_layer(_get_broad_phase_layer(), collision_layer, collision_mask);
}

JPH::EMotionType JoltBody3D::_get_motion_type() const {
	switch (mode) {
		case PhysicsServer3D::BODY_MODE_STATIC: {
			return JPH::EMotionType::Static;
		}
		case PhysicsServer3D::BODY_MODE_KINEMATIC: {
			return JPH::EMotionType::Kinematic;
		}
		case PhysicsServer3D::BODY_MODE_RIGID:
		case PhysicsServer3D::BODY_MODE_RIGID_LINEAR: {
			return JPH::EMotionType::Dynamic;
		}
		default: {
			ERR_FAIL_V_MSG(JPH::EMotionType::Static, vformat("Unhandled body mode: '%d'. This should not happen. Please report this.", mode));
		}
	}
}

void JoltBody3D::_add_to_space() {
	jolt_shape = build_shapes(true);

	JPH::CollisionGroup::GroupID group_id = 0;
	JPH::CollisionGroup::SubGroupID sub_group_id = 0;
	JoltGroupFilter::encode_object(this, group_id, sub_group_id);

	jolt_settings->mUserData = reinterpret_cast<JPH::uint64>(this);
	jolt_settings->mObjectLayer = _get_object_layer();
	jolt_settings->mCollisionGroup = JPH::CollisionGroup(nullptr, group_id, sub_group_id);
	jolt_settings->mMotionType = _get_motion_type();
	jolt_settings->mAllowedDOFs = _calculate_allowed_dofs();
	jolt_settings->mAllowDynamicOrKinematic = true;
	jolt_settings->mCollideKinematicVsNonDynamic = reports_all_kinematic_contacts();
	jolt_settings->mUseManifoldReduction = !reports_contacts();
	jolt_settings->mAllowSleeping = is_sleep_actually_allowed();
	jolt_settings->mLinearDamping = 0.0f;
	jolt_settings->mAngularDamping = 0.0f;
	jolt_settings->mMaxLinearVelocity = JoltProjectSettings::max_linear_velocity;
	jolt_settings->mMaxAngularVelocity = JoltProjectSettings::max_angular_velocity;

	if (JoltProjectSettings::use_enhanced_internal_edge_removal_for_bodies) {
		jolt_settings->mEnhancedInternalEdgeRemoval = true;
	}

	jolt_settings->mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
	jolt_settings->mMassPropertiesOverride = _calculate_mass_properties();

	jolt_settings->SetShape(jolt_shape);

	const JPH::BodyID new_jolt_id = space->add_rigid_body(*this, *jolt_settings, sleep_initially);
	if (new_jolt_id.IsInvalid()) {
		return;
	}

	jolt_id = new_jolt_id;

	delete jolt_settings;
	jolt_settings = nullptr;
}

void JoltBody3D::_enqueue_call_queries() {
	if (space != nullptr) {
		space->enqueue_call_queries(&call_queries_element);
	}
}

void JoltBody3D::_dequeue_call_queries() {
	if (space != nullptr) {
		space->dequeue_call_queries(&call_queries_element);
	}
}

void JoltBody3D::_integrate_forces(float p_step, JPH::Body &p_jolt_body) {
	_update_gravity(p_jolt_body);

	if (!custom_integrator) {
		JPH::MotionProperties &motion_properties = *p_jolt_body.GetMotionPropertiesUnchecked();

		JPH::Vec3 linear_velocity = motion_properties.GetLinearVelocity();
		JPH::Vec3 angular_velocity = motion_properties.GetAngularVelocity();

		// Jolt applies damping differently from Godot Physics, where Godot Physics applies damping before integrating
		// forces whereas Jolt does it after integrating forces. The way Godot Physics does it seems to yield more
		// consistent results across different update frequencies when using high (>1) damping values, so we apply the
		// damping ourselves instead, before any force integration happens.

		linear_velocity *= MAX(1.0f - total_linear_damp * p_step, 0.0f);
		angular_velocity *= MAX(1.0f - total_angular_damp * p_step, 0.0f);

		linear_velocity += to_jolt(gravity) * p_step;

		motion_properties.SetLinearVelocityClamped(linear_velocity);
		motion_properties.SetAngularVelocityClamped(angular_velocity);

		p_jolt_body.AddForce(to_jolt(constant_force));
		p_jolt_body.AddTorque(to_jolt(constant_torque));
	}
}

void JoltBody3D::_move_kinematic(float p_step, JPH::Body &p_jolt_body) {
	p_jolt_body.SetLinearVelocity(JPH::Vec3::sZero());
	p_jolt_body.SetAngularVelocity(JPH::Vec3::sZero());

	const JPH::RVec3 current_position = p_jolt_body.GetPosition();
	const JPH::Quat current_rotation = p_jolt_body.GetRotation();

	const JPH::RVec3 new_position = to_jolt_r(kinematic_transform.origin);
	const JPH::Quat new_rotation = to_jolt(kinematic_transform.basis);

	if (new_position == current_position && new_rotation == current_rotation) {
		return;
	}

	p_jolt_body.MoveKinematic(new_position, new_rotation, p_step);
}

JPH::EAllowedDOFs JoltBody3D::_calculate_allowed_dofs() const {
	if (is_static()) {
		return JPH::EAllowedDOFs::All;
	}

	JPH::EAllowedDOFs allowed_dofs = JPH::EAllowedDOFs::All;

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_LINEAR_X)) {
		allowed_dofs &= ~JPH::EAllowedDOFs::TranslationX;
	}

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_LINEAR_Y)) {
		allowed_dofs &= ~JPH::EAllowedDOFs::TranslationY;
	}

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_LINEAR_Z)) {
		allowed_dofs &= ~JPH::EAllowedDOFs::TranslationZ;
	}

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_ANGULAR_X) || is_rigid_linear()) {
		allowed_dofs &= ~JPH::EAllowedDOFs::RotationX;
	}

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_ANGULAR_Y) || is_rigid_linear()) {
		allowed_dofs &= ~JPH::EAllowedDOFs::RotationY;
	}

	if (is_axis_locked(PhysicsServer3D::BODY_AXIS_ANGULAR_Z) || is_rigid_linear()) {
		allowed_dofs &= ~JPH::EAllowedDOFs::RotationZ;
	}

	ERR_FAIL_COND_V_MSG(allowed_dofs == JPH::EAllowedDOFs::None, JPH::EAllowedDOFs::All, vformat("Invalid axis locks for '%s'. Locking all axes is not supported when using Jolt Physics. All axes will be unlocked. Considering freezing the body instead.", to_string()));

	return allowed_dofs;
}

JPH::MassProperties JoltBody3D::_calculate_mass_properties(const JPH::Shape &p_shape) const {
	const bool calculate_mass = mass <= 0;
	const bool calculate_inertia = inertia.x <= 0 || inertia.y <= 0 || inertia.z <= 0;

	JPH::MassProperties mass_properties = p_shape.GetMassProperties();

	if (calculate_mass && calculate_inertia) {
		// Use the mass properties calculated by the shape
	} else if (calculate_inertia) {
		mass_properties.ScaleToMass(mass);
	} else {
		mass_properties.mMass = mass;
	}

	if (inertia.x > 0) {
		mass_properties.mInertia(0, 0) = (float)inertia.x;
		mass_properties.mInertia(0, 1) = 0;
		mass_properties.mInertia(0, 2) = 0;
		mass_properties.mInertia(1, 0) = 0;
		mass_properties.mInertia(2, 0) = 0;
	}

	if (inertia.y > 0) {
		mass_properties.mInertia(1, 1) = (float)inertia.y;
		mass_properties.mInertia(1, 0) = 0;
		mass_properties.mInertia(1, 2) = 0;
		mass_properties.mInertia(0, 1) = 0;
		mass_properties.mInertia(2, 1) = 0;
	}

	if (inertia.z > 0) {
		mass_properties.mInertia(2, 2) = (float)inertia.z;
		mass_properties.mInertia(2, 0) = 0;
		mass_properties.mInertia(2, 1) = 0;
		mass_properties.mInertia(0, 2) = 0;
		mass_properties.mInertia(1, 2) = 0;
	}

	mass_properties.mInertia(3, 3) = 1.0f;

	return mass_properties;
}

JPH::MassProperties JoltBody3D::_calculate_mass_properties() const {
	return _calculate_mass_properties(*jolt_shape);
}

void JoltBody3D::_update_mass_properties() {
	if (!in_space()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->GetMotionPropertiesUnchecked()->SetMassProperties(_calculate_allowed_dofs(), _calculate_mass_properties());
}

void JoltBody3D::_update_gravity(JPH::Body &p_jolt_body) {
	gravity = Vector3();

	const Vector3 position = to_godot(p_jolt_body.GetPosition());

	bool gravity_done = false;

	for (const JoltArea3D *area : areas) {
		gravity_done = integrate(gravity, area->get_gravity_mode(), [&]() {
			return area->compute_gravity(position);
		});

		if (gravity_done) {
			break;
		}
	}

	if (!gravity_done) {
		gravity += space->get_default_area()->compute_gravity(position);
	}

	gravity *= gravity_scale;
}

void JoltBody3D::_update_damp() {
	if (!in_space()) {
		return;
	}

	total_linear_damp = 0.0;
	total_angular_damp = 0.0;

	bool linear_damp_done = linear_damp_mode == PhysicsServer3D::BODY_DAMP_MODE_REPLACE;
	bool angular_damp_done = angular_damp_mode == PhysicsServer3D::BODY_DAMP_MODE_REPLACE;

	for (const JoltArea3D *area : areas) {
		if (!linear_damp_done) {
			linear_damp_done = integrate(total_linear_damp, area->get_linear_damp_mode(), [&]() {
				return area->get_linear_damp();
			});
		}

		if (!angular_damp_done) {
			angular_damp_done = integrate(total_angular_damp, area->get_angular_damp_mode(), [&]() {
				return area->get_angular_damp();
			});
		}

		if (linear_damp_done && angular_damp_done) {
			break;
		}
	}

	const JoltArea3D *default_area = space->get_default_area();

	if (!linear_damp_done) {
		total_linear_damp += default_area->get_linear_damp();
	}

	if (!angular_damp_done) {
		total_angular_damp += default_area->get_angular_damp();
	}

	switch (linear_damp_mode) {
		case PhysicsServer3D::BODY_DAMP_MODE_COMBINE: {
			total_linear_damp += linear_damp;
		} break;
		case PhysicsServer3D::BODY_DAMP_MODE_REPLACE: {
			total_linear_damp = linear_damp;
		} break;
	}

	switch (angular_damp_mode) {
		case PhysicsServer3D::BODY_DAMP_MODE_COMBINE: {
			total_angular_damp += angular_damp;
		} break;
		case PhysicsServer3D::BODY_DAMP_MODE_REPLACE: {
			total_angular_damp = angular_damp;
		} break;
	}

	_motion_changed();
}

void JoltBody3D::_update_kinematic_transform() {
	if (is_kinematic()) {
		kinematic_transform = get_transform_unscaled();
	}
}

void JoltBody3D::_update_joint_constraints() {
	for (JoltJoint3D *joint : joints) {
		joint->rebuild();
	}
}

void JoltBody3D::_update_possible_kinematic_contacts() {
	const bool value = reports_all_kinematic_contacts();

	if (!in_space()) {
		jolt_settings->mCollideKinematicVsNonDynamic = value;
	} else {
		const JoltWritableBody3D body = space->write_body(jolt_id);
		ERR_FAIL_COND(body.is_invalid());

		body->SetCollideKinematicVsNonDynamic(value);
	}
}

void JoltBody3D::_update_sleep_allowed() {
	const bool value = is_sleep_actually_allowed();

	if (!in_space()) {
		jolt_settings->mAllowSleeping = value;
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->SetAllowSleeping(value);
}

void JoltBody3D::_destroy_joint_constraints() {
	for (JoltJoint3D *joint : joints) {
		joint->destroy();
	}
}

void JoltBody3D::_exit_all_areas() {
	for (JoltArea3D *area : areas) {
		area->body_exited(jolt_id, false);
	}

	areas.clear();
}

void JoltBody3D::_update_group_filter() {
	JPH::GroupFilter *group_filter = !exceptions.is_empty() ? JoltGroupFilter::instance : nullptr;

	if (!in_space()) {
		jolt_settings->mCollisionGroup.SetGroupFilter(group_filter);
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->GetCollisionGroup().SetGroupFilter(group_filter);
}

void JoltBody3D::_mode_changed() {
	_update_object_layer();
	_update_kinematic_transform();
	_update_mass_properties();
	_update_sleep_allowed();
	wake_up();
}

void JoltBody3D::_shapes_committed() {
	JoltShapedObject3D::_shapes_committed();

	_update_mass_properties();
	_update_joint_constraints();
	wake_up();
}

void JoltBody3D::_space_changing() {
	JoltShapedObject3D::_space_changing();

	sleep_initially = is_sleeping();

	_destroy_joint_constraints();
	_exit_all_areas();
	_dequeue_call_queries();
}

void JoltBody3D::_space_changed() {
	JoltShapedObject3D::_space_changed();

	_update_kinematic_transform();
	_update_group_filter();
	_update_joint_constraints();
	_update_sleep_allowed();
	_areas_changed();
}

void JoltBody3D::_areas_changed() {
	_update_damp();
	wake_up();
}

void JoltBody3D::_joints_changed() {
	wake_up();
}

void JoltBody3D::_transform_changed() {
	wake_up();
}

void JoltBody3D::_motion_changed() {
	wake_up();
}

void JoltBody3D::_exceptions_changed() {
	_update_group_filter();
}

void JoltBody3D::_axis_lock_changed() {
	_update_mass_properties();
	wake_up();
}

void JoltBody3D::_contact_reporting_changed() {
	_update_possible_kinematic_contacts();
	_update_sleep_allowed();
	wake_up();
}

void JoltBody3D::_sleep_allowed_changed() {
	_update_sleep_allowed();
	wake_up();
}

JoltBody3D::JoltBody3D() :
		JoltShapedObject3D(OBJECT_TYPE_BODY),
		call_queries_element(this) {
}

JoltBody3D::~JoltBody3D() {
	if (direct_state != nullptr) {
		memdelete(direct_state);
		direct_state = nullptr;
	}
}

void JoltBody3D::set_transform(Transform3D p_transform) {
	JOLT_ENSURE_SCALE_NOT_ZERO(p_transform, vformat("An invalid transform was passed to physics body '%s'.", to_string()));

	const Vector3 new_scale = p_transform.basis.get_scale();

	// Ideally we would do an exact comparison here, but due to floating-point precision this would be invalidated very often.
	if (!scale.is_equal_approx(new_scale)) {
		scale = new_scale;
		_shapes_changed();
	}

	p_transform.basis.orthonormalize();

	if (!in_space()) {
		jolt_settings->mPosition = to_jolt_r(p_transform.origin);
		jolt_settings->mRotation = to_jolt(p_transform.basis);
	} else if (is_kinematic()) {
		kinematic_transform = p_transform;
	} else {
		space->get_body_iface().SetPositionAndRotation(jolt_id, to_jolt_r(p_transform.origin), to_jolt(p_transform.basis), JPH::EActivation::DontActivate);
	}

	_transform_changed();
}

Variant JoltBody3D::get_state(PhysicsServer3D::BodyState p_state) const {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			return get_transform_scaled();
		}
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			return get_linear_velocity();
		}
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			return get_angular_velocity();
		}
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			return is_sleeping();
		}
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			return is_sleep_allowed();
		}
		default: {
			ERR_FAIL_V_MSG(Variant(), vformat("Unhandled body state: '%d'. This should not happen. Please report this.", p_state));
		}
	}
}

void JoltBody3D::set_state(PhysicsServer3D::BodyState p_state, const Variant &p_value) {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			set_transform(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			set_linear_velocity(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			set_angular_velocity(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			set_is_sleeping(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			set_is_sleep_allowed(p_value);
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled body state: '%d'. This should not happen. Please report this.", p_state));
		} break;
	}
}

Variant JoltBody3D::get_param(PhysicsServer3D::BodyParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE: {
			return get_bounce();
		}
		case PhysicsServer3D::BODY_PARAM_FRICTION: {
			return get_friction();
		}
		case PhysicsServer3D::BODY_PARAM_MASS: {
			return get_mass();
		}
		case PhysicsServer3D::BODY_PARAM_INERTIA: {
			return get_inertia();
		}
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS: {
			return get_center_of_mass_custom();
		}
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE: {
			return get_gravity_scale();
		}
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE: {
			return get_linear_damp_mode();
		}
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE: {
			return get_angular_damp_mode();
		}
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP: {
			return get_linear_damp();
		}
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP: {
			return get_angular_damp();
		}
		default: {
			ERR_FAIL_V_MSG(Variant(), vformat("Unhandled body parameter: '%d'. This should not happen. Please report this.", p_param));
		}
	}
}

void JoltBody3D::set_param(PhysicsServer3D::BodyParameter p_param, const Variant &p_value) {
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE: {
			set_bounce(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_FRICTION: {
			set_friction(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_MASS: {
			set_mass(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_INERTIA: {
			set_inertia(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS: {
			set_center_of_mass_custom(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE: {
			set_gravity_scale(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE: {
			set_linear_damp_mode((DampMode)(int)p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE: {
			set_angular_damp_mode((DampMode)(int)p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP: {
			set_linear_damp(p_value);
		} break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP: {
			set_angular_damp(p_value);
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled body parameter: '%d'. This should not happen. Please report this.", p_param));
		} break;
	}
}

void JoltBody3D::set_custom_integrator(bool p_enabled) {
	if (custom_integrator == p_enabled) {
		return;
	}

	custom_integrator = p_enabled;

	if (!in_space()) {
		_motion_changed();
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->ResetForce();
	body->ResetTorque();

	_motion_changed();
}

bool JoltBody3D::is_sleeping() const {
	if (!in_space()) {
		return sleep_initially;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), false);

	return !body->IsActive();
}

bool JoltBody3D::is_sleep_actually_allowed() const {
	return sleep_allowed && !(is_kinematic() && reports_contacts());
}

void JoltBody3D::set_is_sleeping(bool p_enabled) {
	if (!in_space()) {
		sleep_initially = p_enabled;
		return;
	}

	JPH::BodyInterface &body_iface = space->get_body_iface();

	if (p_enabled) {
		body_iface.DeactivateBody(jolt_id);
	} else {
		body_iface.ActivateBody(jolt_id);
	}
}

void JoltBody3D::set_is_sleep_allowed(bool p_enabled) {
	if (sleep_allowed == p_enabled) {
		return;
	}

	sleep_allowed = p_enabled;

	_sleep_allowed_changed();
}

Basis JoltBody3D::get_principal_inertia_axes() const {
	ERR_FAIL_NULL_V_MSG(space, Basis(), vformat("Failed to retrieve principal inertia axes of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(is_static() || is_kinematic())) {
		return Basis();
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Basis());

	return to_godot(body->GetRotation() * body->GetMotionProperties()->GetInertiaRotation());
}

Vector3 JoltBody3D::get_inverse_inertia() const {
	ERR_FAIL_NULL_V_MSG(space, Vector3(), vformat("Failed to retrieve inverse inertia of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(is_static() || is_kinematic())) {
		return Vector3();
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	const JPH::MotionProperties &motion_properties = *body->GetMotionPropertiesUnchecked();

	return to_godot(motion_properties.GetLocalSpaceInverseInertia().GetDiagonal3());
}

Basis JoltBody3D::get_inverse_inertia_tensor() const {
	ERR_FAIL_NULL_V_MSG(space, Basis(), vformat("Failed to retrieve inverse inertia tensor of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(is_static() || is_kinematic())) {
		return Basis();
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Basis());

	return to_godot(body->GetInverseInertia()).basis;
}

void JoltBody3D::set_linear_velocity(const Vector3 &p_velocity) {
	if (is_static() || is_kinematic()) {
		linear_surface_velocity = p_velocity;
		_motion_changed();
		return;
	}

	if (!in_space()) {
		jolt_settings->mLinearVelocity = to_jolt(p_velocity);
		_motion_changed();
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->GetMotionPropertiesUnchecked()->SetLinearVelocityClamped(to_jolt(p_velocity));

	_motion_changed();
}

void JoltBody3D::set_angular_velocity(const Vector3 &p_velocity) {
	if (is_static() || is_kinematic()) {
		angular_surface_velocity = p_velocity;
		_motion_changed();
		return;
	}

	if (!in_space()) {
		jolt_settings->mAngularVelocity = to_jolt(p_velocity);
		_motion_changed();
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->GetMotionPropertiesUnchecked()->SetAngularVelocityClamped(to_jolt(p_velocity));

	_motion_changed();
}

void JoltBody3D::set_axis_velocity(const Vector3 &p_axis_velocity) {
	const Vector3 axis = p_axis_velocity.normalized();

	if (!in_space()) {
		Vector3 linear_velocity = to_godot(jolt_settings->mLinearVelocity);
		linear_velocity -= axis * axis.dot(linear_velocity);
		linear_velocity += p_axis_velocity;
		jolt_settings->mLinearVelocity = to_jolt(linear_velocity);
	} else {
		const JoltWritableBody3D body = space->write_body(jolt_id);
		ERR_FAIL_COND(body.is_invalid());

		Vector3 linear_velocity = get_linear_velocity();
		linear_velocity -= axis * axis.dot(linear_velocity);
		linear_velocity += p_axis_velocity;
		set_linear_velocity(linear_velocity);
	}

	_motion_changed();
}

Vector3 JoltBody3D::get_velocity_at_position(const Vector3 &p_position) const {
	if (unlikely(!in_space())) {
		return Vector3();
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	const JPH::MotionProperties &motion_properties = *body->GetMotionPropertiesUnchecked();

	const Vector3 total_linear_velocity = to_godot(motion_properties.GetLinearVelocity()) + linear_surface_velocity;
	const Vector3 total_angular_velocity = to_godot(motion_properties.GetAngularVelocity()) + angular_surface_velocity;
	const Vector3 com_to_pos = p_position - to_godot(body->GetCenterOfMassPosition());

	return total_linear_velocity + total_angular_velocity.cross(com_to_pos);
}

void JoltBody3D::set_center_of_mass_custom(const Vector3 &p_center_of_mass) {
	if (custom_center_of_mass && p_center_of_mass == center_of_mass_custom) {
		return;
	}

	custom_center_of_mass = true;
	center_of_mass_custom = p_center_of_mass;

	_shapes_changed();
}

void JoltBody3D::set_max_contacts_reported(int p_count) {
	ERR_FAIL_COND(p_count < 0);

	if (unlikely((int)contacts.size() == p_count)) {
		return;
	}

	contacts.resize(p_count);
	contact_count = MIN(contact_count, p_count);

	const bool use_manifold_reduction = !reports_contacts();

	if (!in_space()) {
		jolt_settings->mUseManifoldReduction = use_manifold_reduction;
		_contact_reporting_changed();
		return;
	}

	JPH::BodyInterface &body_iface = space->get_body_iface();

	body_iface.SetUseManifoldReduction(jolt_id, use_manifold_reduction);

	_contact_reporting_changed();
}

bool JoltBody3D::reports_all_kinematic_contacts() const {
	return reports_contacts() && JoltProjectSettings::generate_all_kinematic_contacts;
}

void JoltBody3D::add_contact(const JoltBody3D *p_collider, float p_depth, int p_shape_index, int p_collider_shape_index, const Vector3 &p_normal, const Vector3 &p_position, const Vector3 &p_collider_position, const Vector3 &p_velocity, const Vector3 &p_collider_velocity, const Vector3 &p_impulse) {
	const int max_contacts = get_max_contacts_reported();

	if (max_contacts == 0) {
		return;
	}

	Contact *contact = nullptr;

	if (contact_count < max_contacts) {
		contact = &contacts[contact_count++];
	} else {
		Contact *shallowest_contact = &contacts[0];

		for (int i = 1; i < (int)contacts.size(); i++) {
			Contact &other_contact = contacts[i];
			if (other_contact.depth < shallowest_contact->depth) {
				shallowest_contact = &other_contact;
			}
		}

		if (shallowest_contact->depth < p_depth) {
			contact = shallowest_contact;
		}
	}

	if (contact != nullptr) {
		contact->normal = p_normal;
		contact->position = p_position;
		contact->collider_position = p_collider_position;
		contact->velocity = p_velocity;
		contact->collider_velocity = p_collider_velocity;
		contact->impulse = p_impulse;
		contact->collider_id = p_collider->get_instance_id();
		contact->collider_rid = p_collider->get_rid();
		contact->shape_index = p_shape_index;
		contact->collider_shape_index = p_collider_shape_index;
	}
}

void JoltBody3D::reset_mass_properties() {
	if (custom_center_of_mass) {
		custom_center_of_mass = false;
		center_of_mass_custom.zero();

		_shapes_changed();
	}

	inertia.zero();

	_update_mass_properties();
}

void JoltBody3D::apply_force(const Vector3 &p_force, const Vector3 &p_position) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply force to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (custom_integrator || p_force == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddForce(to_jolt(p_force), body->GetPosition() + to_jolt(p_position));

	_motion_changed();
}

void JoltBody3D::apply_central_force(const Vector3 &p_force) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply central force to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (custom_integrator || p_force == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddForce(to_jolt(p_force));

	_motion_changed();
}

void JoltBody3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply impulse to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (p_impulse == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddImpulse(to_jolt(p_impulse), body->GetPosition() + to_jolt(p_position));

	_motion_changed();
}

void JoltBody3D::apply_central_impulse(const Vector3 &p_impulse) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply central impulse to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (p_impulse == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddImpulse(to_jolt(p_impulse));

	_motion_changed();
}

void JoltBody3D::apply_torque(const Vector3 &p_torque) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply torque to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (custom_integrator || p_torque == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddTorque(to_jolt(p_torque));

	_motion_changed();
}

void JoltBody3D::apply_torque_impulse(const Vector3 &p_impulse) {
	ERR_FAIL_NULL_MSG(space, vformat("Failed to apply torque impulse to '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	if (unlikely(!is_rigid())) {
		return;
	}

	if (p_impulse == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->AddAngularImpulse(to_jolt(p_impulse));

	_motion_changed();
}

void JoltBody3D::add_constant_central_force(const Vector3 &p_force) {
	if (p_force == Vector3()) {
		return;
	}

	constant_force += p_force;

	_motion_changed();
}

void JoltBody3D::add_constant_force(const Vector3 &p_force, const Vector3 &p_position) {
	if (p_force == Vector3()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	constant_force += p_force;
	constant_torque += (p_position - get_center_of_mass_relative()).cross(p_force);

	_motion_changed();
}

void JoltBody3D::add_constant_torque(const Vector3 &p_torque) {
	if (p_torque == Vector3()) {
		return;
	}

	constant_torque += p_torque;

	_motion_changed();
}

Vector3 JoltBody3D::get_constant_force() const {
	return constant_force;
}

void JoltBody3D::set_constant_force(const Vector3 &p_force) {
	if (constant_force == p_force) {
		return;
	}

	constant_force = p_force;

	_motion_changed();
}

Vector3 JoltBody3D::get_constant_torque() const {
	return constant_torque;
}

void JoltBody3D::set_constant_torque(const Vector3 &p_torque) {
	if (constant_torque == p_torque) {
		return;
	}

	constant_torque = p_torque;

	_motion_changed();
}

void JoltBody3D::add_collision_exception(const RID &p_excepted_body) {
	exceptions.push_back(p_excepted_body);

	_exceptions_changed();
}

void JoltBody3D::remove_collision_exception(const RID &p_excepted_body) {
	exceptions.erase(p_excepted_body);

	_exceptions_changed();
}

bool JoltBody3D::has_collision_exception(const RID &p_excepted_body) const {
	return exceptions.find(p_excepted_body) >= 0;
}

void JoltBody3D::add_area(JoltArea3D *p_area) {
	int i = 0;
	for (; i < (int)areas.size(); i++) {
		if (p_area->get_priority() > areas[i]->get_priority()) {
			break;
		}
	}

	areas.insert(i, p_area);

	_areas_changed();
}

void JoltBody3D::remove_area(JoltArea3D *p_area) {
	areas.erase(p_area);

	_areas_changed();
}

void JoltBody3D::add_joint(JoltJoint3D *p_joint) {
	joints.push_back(p_joint);

	_joints_changed();
}

void JoltBody3D::remove_joint(JoltJoint3D *p_joint) {
	joints.erase(p_joint);

	_joints_changed();
}

void JoltBody3D::call_queries() {
	if (custom_integration_callback.is_valid()) {
		const Variant direct_state_variant = get_direct_state();
		const Variant *args[2] = { &direct_state_variant, &custom_integration_userdata };
		const int argc = custom_integration_userdata.get_type() != Variant::NIL ? 2 : 1;

		Callable::CallError ce;
		Variant ret;
		custom_integration_callback.callp(args, argc, ret, ce);

		if (unlikely(ce.error != Callable::CallError::CALL_OK)) {
			ERR_PRINT_ONCE(vformat("Failed to call force integration callback for '%s'. It returned the following error: '%s'.", to_string(), Variant::get_callable_error_text(custom_integration_callback, args, argc, ce)));
		}
	}

	if (state_sync_callback.is_valid()) {
		const Variant direct_state_variant = get_direct_state();
		const Variant *args[1] = { &direct_state_variant };

		Callable::CallError ce;
		Variant ret;
		state_sync_callback.callp(args, 1, ret, ce);

		if (unlikely(ce.error != Callable::CallError::CALL_OK)) {
			ERR_PRINT_ONCE(vformat("Failed to call state synchronization callback for '%s'. It returned the following error: '%s'.", to_string(), Variant::get_callable_error_text(state_sync_callback, args, 1, ce)));
		}
	}
}

void JoltBody3D::pre_step(float p_step, JPH::Body &p_jolt_body) {
	JoltObject3D::pre_step(p_step, p_jolt_body);

	switch (mode) {
		case PhysicsServer3D::BODY_MODE_STATIC: {
			// Will never happen.
		} break;
		case PhysicsServer3D::BODY_MODE_RIGID:
		case PhysicsServer3D::BODY_MODE_RIGID_LINEAR: {
			_integrate_forces(p_step, p_jolt_body);
		} break;
		case PhysicsServer3D::BODY_MODE_KINEMATIC: {
			_update_gravity(p_jolt_body);
			_move_kinematic(p_step, p_jolt_body);
		} break;
	}

	if (_should_call_queries()) {
		_enqueue_call_queries();
	}

	contact_count = 0;
}

JoltPhysicsDirectBodyState3D *JoltBody3D::get_direct_state() {
	if (direct_state == nullptr) {
		direct_state = memnew(JoltPhysicsDirectBodyState3D(this));
	}

	return direct_state;
}

void JoltBody3D::set_mode(PhysicsServer3D::BodyMode p_mode) {
	if (p_mode == mode) {
		return;
	}

	mode = p_mode;

	if (!in_space()) {
		_mode_changed();
		return;
	}

	const JPH::EMotionType motion_type = _get_motion_type();

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	if (motion_type == JPH::EMotionType::Static) {
		put_to_sleep();
	}

	body->SetMotionType(motion_type);

	if (motion_type != JPH::EMotionType::Static) {
		wake_up();
	}

	if (motion_type == JPH::EMotionType::Kinematic) {
		body->SetLinearVelocity(JPH::Vec3::sZero());
		body->SetAngularVelocity(JPH::Vec3::sZero());
	}

	linear_surface_velocity = Vector3();
	angular_surface_velocity = Vector3();

	_mode_changed();
}

bool JoltBody3D::is_ccd_enabled() const {
	if (!in_space()) {
		return jolt_settings->mMotionQuality == JPH::EMotionQuality::LinearCast;
	}

	const JPH::BodyInterface &body_iface = space->get_body_iface();

	return body_iface.GetMotionQuality(jolt_id) == JPH::EMotionQuality::LinearCast;
}

void JoltBody3D::set_ccd_enabled(bool p_enabled) {
	const JPH::EMotionQuality motion_quality = p_enabled ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;

	if (!in_space()) {
		jolt_settings->mMotionQuality = motion_quality;
		return;
	}

	JPH::BodyInterface &body_iface = space->get_body_iface();

	body_iface.SetMotionQuality(jolt_id, motion_quality);
}

void JoltBody3D::set_mass(float p_mass) {
	if (p_mass != mass) {
		mass = p_mass;
		_update_mass_properties();
	}
}

void JoltBody3D::set_inertia(const Vector3 &p_inertia) {
	if (p_inertia != inertia) {
		inertia = p_inertia;
		_update_mass_properties();
	}
}

float JoltBody3D::get_bounce() const {
	if (!in_space()) {
		return jolt_settings->mRestitution;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), 0.0f);

	return body->GetRestitution();
}

void JoltBody3D::set_bounce(float p_bounce) {
	if (!in_space()) {
		jolt_settings->mRestitution = p_bounce;
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->SetRestitution(p_bounce);
}

float JoltBody3D::get_friction() const {
	if (!in_space()) {
		return jolt_settings->mFriction;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), 0.0f);

	return body->GetFriction();
}

void JoltBody3D::set_friction(float p_friction) {
	if (!in_space()) {
		jolt_settings->mFriction = p_friction;
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->SetFriction(p_friction);
}

void JoltBody3D::set_gravity_scale(float p_scale) {
	if (gravity_scale == p_scale) {
		return;
	}

	gravity_scale = p_scale;

	_motion_changed();
}

void JoltBody3D::set_linear_damp(float p_damp) {
	p_damp = MAX(0.0f, p_damp);

	if (p_damp == linear_damp) {
		return;
	}

	linear_damp = p_damp;

	_update_damp();
}

void JoltBody3D::set_angular_damp(float p_damp) {
	p_damp = MAX(0.0f, p_damp);

	if (p_damp == angular_damp) {
		return;
	}

	angular_damp = p_damp;

	_update_damp();
}

void JoltBody3D::set_linear_damp_mode(DampMode p_mode) {
	if (p_mode == linear_damp_mode) {
		return;
	}

	linear_damp_mode = p_mode;

	_update_damp();
}

void JoltBody3D::set_angular_damp_mode(DampMode p_mode) {
	if (p_mode == angular_damp_mode) {
		return;
	}

	angular_damp_mode = p_mode;

	_update_damp();
}

bool JoltBody3D::is_axis_locked(PhysicsServer3D::BodyAxis p_axis) const {
	return (locked_axes & (uint32_t)p_axis) != 0;
}

void JoltBody3D::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_enabled) {
	const uint32_t previous_locked_axes = locked_axes;

	if (p_enabled) {
		locked_axes |= (uint32_t)p_axis;
	} else {
		locked_axes &= ~(uint32_t)p_axis;
	}

	if (previous_locked_axes != locked_axes) {
		_axis_lock_changed();
	}
}

bool JoltBody3D::can_interact_with(const JoltBody3D &p_other) const {
	return (can_collide_with(p_other) || p_other.can_collide_with(*this)) && !has_collision_exception(p_other.get_rid()) && !p_other.has_collision_exception(rid);
}

bool JoltBody3D::can_interact_with(const JoltSoftBody3D &p_other) const {
	return p_other.can_interact_with(*this);
}

bool JoltBody3D::can_interact_with(const JoltArea3D &p_other) const {
	return p_other.can_interact_with(*this);
}
