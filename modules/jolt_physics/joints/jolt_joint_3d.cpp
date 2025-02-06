/**************************************************************************/
/*  jolt_joint_3d.cpp                                                     */
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

#include "jolt_joint_3d.h"

#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"
#include "../objects/jolt_body_3d.h"
#include "../spaces/jolt_space_3d.h"

namespace {

constexpr int DEFAULT_SOLVER_PRIORITY = 1;

} // namespace

void JoltJoint3D::_shift_reference_frames(const Vector3 &p_linear_shift, const Vector3 &p_angular_shift, Transform3D &r_shifted_ref_a, Transform3D &r_shifted_ref_b) {
	Vector3 origin_a = local_ref_a.origin;
	Vector3 origin_b = local_ref_b.origin;

	if (body_a != nullptr) {
		origin_a *= body_a->get_scale();
		origin_a -= to_godot(body_a->get_jolt_shape()->GetCenterOfMass());
	}

	if (body_b != nullptr) {
		origin_b *= body_b->get_scale();
		origin_b -= to_godot(body_b->get_jolt_shape()->GetCenterOfMass());
	}

	const Basis &basis_a = local_ref_a.basis;
	const Basis &basis_b = local_ref_b.basis;

	const Basis shifted_basis_a = basis_a * Basis::from_euler(p_angular_shift, EulerOrder::ZYX);
	const Vector3 shifted_origin_a = origin_a - basis_a.xform(p_linear_shift);

	r_shifted_ref_a = Transform3D(shifted_basis_a, shifted_origin_a);
	r_shifted_ref_b = Transform3D(basis_b, origin_b);
}

void JoltJoint3D::_wake_up_bodies() {
	if (body_a != nullptr) {
		body_a->wake_up();
	}

	if (body_b != nullptr) {
		body_b->wake_up();
	}
}

void JoltJoint3D::_update_enabled() {
	if (jolt_ref != nullptr) {
		jolt_ref->SetEnabled(enabled);
	}
}

void JoltJoint3D::_update_iterations() {
	if (jolt_ref != nullptr) {
		jolt_ref->SetNumVelocityStepsOverride((JPH::uint)velocity_iterations);
		jolt_ref->SetNumPositionStepsOverride((JPH::uint)position_iterations);
	}
}

void JoltJoint3D::_enabled_changed() {
	_update_enabled();
	_wake_up_bodies();
}

void JoltJoint3D::_iterations_changed() {
	_update_iterations();
	_wake_up_bodies();
}

String JoltJoint3D::_bodies_to_string() const {
	return vformat("'%s' and '%s'", body_a != nullptr ? body_a->to_string() : "<unknown>", body_b != nullptr ? body_b->to_string() : "<World>");
}

JoltJoint3D::JoltJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b) :
		enabled(p_old_joint.enabled),
		collision_disabled(p_old_joint.collision_disabled),
		body_a(p_body_a),
		body_b(p_body_b),
		rid(p_old_joint.rid),
		local_ref_a(p_local_ref_a),
		local_ref_b(p_local_ref_b) {
	if (body_a != nullptr) {
		body_a->add_joint(this);
	}

	if (body_b != nullptr) {
		body_b->add_joint(this);
	}

	if (body_b == nullptr && JoltProjectSettings::joint_world_node == JOLT_JOINT_WORLD_NODE_A) {
		// The joint scene nodes will, when omitting one of the two body nodes, always pass in a
		// null `body_b` to indicate it being the "world node", regardless of which body node you
		// leave blank. So we need to correct for that if we wish to use the arguably more intuitive
		// alternative where `body_a` is the "world node" instead.

		SWAP(body_a, body_b);
		SWAP(local_ref_a, local_ref_b);
	}
}

JoltJoint3D::~JoltJoint3D() {
	if (body_a != nullptr) {
		body_a->remove_joint(this);
	}

	if (body_b != nullptr) {
		body_b->remove_joint(this);
	}

	destroy();
}

JoltSpace3D *JoltJoint3D::get_space() const {
	if (body_a != nullptr && body_b != nullptr) {
		JoltSpace3D *space_a = body_a->get_space();
		JoltSpace3D *space_b = body_b->get_space();

		if (space_a == nullptr || space_b == nullptr) {
			return nullptr;
		}

		ERR_FAIL_COND_V_MSG(space_a != space_b, nullptr, vformat("Joint was found to connect bodies in different physics spaces. This joint will effectively be disabled. This joint connects %s.", _bodies_to_string()));

		return space_a;
	} else if (body_a != nullptr) {
		return body_a->get_space();
	} else if (body_b != nullptr) {
		return body_b->get_space();
	}

	return nullptr;
}

void JoltJoint3D::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}

	enabled = p_enabled;

	_enabled_changed();
}

int JoltJoint3D::get_solver_priority() const {
	return DEFAULT_SOLVER_PRIORITY;
}

void JoltJoint3D::set_solver_priority(int p_priority) {
	if (p_priority != DEFAULT_SOLVER_PRIORITY) {
		WARN_PRINT(vformat("Joint solver priority is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
	}
}

void JoltJoint3D::set_solver_velocity_iterations(int p_iterations) {
	if (velocity_iterations == p_iterations) {
		return;
	}

	velocity_iterations = p_iterations;

	_iterations_changed();
}

void JoltJoint3D::set_solver_position_iterations(int p_iterations) {
	if (position_iterations == p_iterations) {
		return;
	}

	position_iterations = p_iterations;

	_iterations_changed();
}

void JoltJoint3D::set_collision_disabled(bool p_disabled) {
	collision_disabled = p_disabled;

	if (body_a == nullptr || body_b == nullptr) {
		return;
	}

	PhysicsServer3D *physics_server = PhysicsServer3D::get_singleton();

	if (collision_disabled) {
		physics_server->body_add_collision_exception(body_a->get_rid(), body_b->get_rid());
		physics_server->body_add_collision_exception(body_b->get_rid(), body_a->get_rid());
	} else {
		physics_server->body_remove_collision_exception(body_a->get_rid(), body_b->get_rid());
		physics_server->body_remove_collision_exception(body_b->get_rid(), body_a->get_rid());
	}
}

void JoltJoint3D::destroy() {
	if (jolt_ref == nullptr) {
		return;
	}

	JoltSpace3D *space = get_space();

	if (space != nullptr) {
		space->remove_joint(this);
	}

	jolt_ref = nullptr;
}
