/**************************************************************************/
/*  jolt_soft_body_3d.cpp                                                 */
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

#include "jolt_soft_body_3d.h"

#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"
#include "../spaces/jolt_broad_phase_layer.h"
#include "../spaces/jolt_space_3d.h"
#include "jolt_area_3d.h"
#include "jolt_body_3d.h"
#include "jolt_group_filter.h"

#include "servers/rendering_server.h"

#include "Jolt/Physics/SoftBody/SoftBodyMotionProperties.h"

namespace {

bool is_face_degenerate(const int p_face[3]) {
	return p_face[0] == p_face[1] || p_face[0] == p_face[2] || p_face[1] == p_face[2];
}

template <typename TJoltVertex>
void pin_vertices(const JoltSoftBody3D &p_body, const HashSet<int> &p_pinned_vertices, const LocalVector<int> &p_mesh_to_physics, JPH::Array<TJoltVertex> &r_physics_vertices) {
	const int mesh_vertex_count = p_mesh_to_physics.size();
	const int physics_vertex_count = (int)r_physics_vertices.size();

	for (int mesh_index : p_pinned_vertices) {
		ERR_CONTINUE_MSG(mesh_index < 0 || mesh_index >= mesh_vertex_count, vformat("Index %d of pinned vertex in soft body '%s' is out of bounds. There are only %d vertices in the current mesh.", mesh_index, p_body.to_string(), mesh_vertex_count));

		const int physics_index = p_mesh_to_physics[mesh_index];
		ERR_CONTINUE_MSG(physics_index < 0 || physics_index >= physics_vertex_count, vformat("Index %d of pinned vertex in soft body '%s' is out of bounds. There are only %d vertices in the current mesh. This should not happen. Please report this.", physics_index, p_body.to_string(), physics_vertex_count));

		r_physics_vertices[physics_index].mInvMass = 0.0f;
	}
}

} // namespace

JPH::BroadPhaseLayer JoltSoftBody3D::_get_broad_phase_layer() const {
	return JoltBroadPhaseLayer::BODY_DYNAMIC;
}

JPH::ObjectLayer JoltSoftBody3D::_get_object_layer() const {
	ERR_FAIL_NULL_V(space, 0);

	return space->map_to_object_layer(_get_broad_phase_layer(), collision_layer, collision_mask);
}

void JoltSoftBody3D::_space_changing() {
	JoltObject3D::_space_changing();

	_deref_shared_data();

	if (space != nullptr && !jolt_id.IsInvalid()) {
		const JoltReadableBody3D body = space->read_body(jolt_id);
		ERR_FAIL_COND(body.is_invalid());

		jolt_settings = new JPH::SoftBodyCreationSettings(body->GetSoftBodyCreationSettings());
		jolt_settings->mSettings = nullptr;
	}
}

void JoltSoftBody3D::_space_changed() {
	JoltObject3D::_space_changed();

	_update_mass();
	_update_pressure();
	_update_damping();
	_update_simulation_precision();
	_update_group_filter();
}

void JoltSoftBody3D::_add_to_space() {
	if (unlikely(space == nullptr || !mesh.is_valid())) {
		return;
	}

	const bool has_valid_shared = _ref_shared_data();
	ERR_FAIL_COND(!has_valid_shared);

	JPH::CollisionGroup::GroupID group_id = 0;
	JPH::CollisionGroup::SubGroupID sub_group_id = 0;
	JoltGroupFilter::encode_object(this, group_id, sub_group_id);

	jolt_settings->mSettings = shared->settings;
	jolt_settings->mUserData = reinterpret_cast<JPH::uint64>(this);
	jolt_settings->mObjectLayer = _get_object_layer();
	jolt_settings->mCollisionGroup = JPH::CollisionGroup(nullptr, group_id, sub_group_id);
	jolt_settings->mMaxLinearVelocity = JoltProjectSettings::get_max_linear_velocity();

	const JPH::BodyID new_jolt_id = space->add_soft_body(*this, *jolt_settings);
	if (new_jolt_id.IsInvalid()) {
		return;
	}

	jolt_id = new_jolt_id;

	delete jolt_settings;
	jolt_settings = nullptr;
}

bool JoltSoftBody3D::_ref_shared_data() {
	HashMap<RID, Shared>::Iterator iter_shared_data = mesh_to_shared.find(mesh);

	if (iter_shared_data == mesh_to_shared.end()) {
		RenderingServer *rendering = RenderingServer::get_singleton();

		const Array mesh_data = rendering->mesh_surface_get_arrays(mesh, 0);
		ERR_FAIL_COND_V(mesh_data.is_empty(), false);

		const PackedInt32Array mesh_indices = mesh_data[RenderingServer::ARRAY_INDEX];
		ERR_FAIL_COND_V(mesh_indices.is_empty(), false);

		const PackedVector3Array mesh_vertices = mesh_data[RenderingServer::ARRAY_VERTEX];
		ERR_FAIL_COND_V(mesh_vertices.is_empty(), false);

		iter_shared_data = mesh_to_shared.insert(mesh, Shared());

		LocalVector<int> &mesh_to_physics = iter_shared_data->value.mesh_to_physics;

		JPH::SoftBodySharedSettings &settings = *iter_shared_data->value.settings;
		settings.mVertexRadius = JoltProjectSettings::get_soft_body_point_radius();

		JPH::Array<JPH::SoftBodySharedSettings::Vertex> &physics_vertices = settings.mVertices;
		JPH::Array<JPH::SoftBodySharedSettings::Face> &physics_faces = settings.mFaces;

		HashMap<Vector3, int> vertex_to_physics;

		const int mesh_vertex_count = mesh_vertices.size();
		const int mesh_index_count = mesh_indices.size();

		mesh_to_physics.resize(mesh_vertex_count);
		physics_vertices.reserve(mesh_vertex_count);
		vertex_to_physics.reserve(mesh_vertex_count);

		int physics_index_count = 0;

		for (int i = 0; i < mesh_index_count; i += 3) {
			int physics_face[3];
			int mesh_face[3];

			for (int j = 0; j < 3; ++j) {
				const int mesh_index = mesh_indices[i + j];
				const Vector3 vertex = mesh_vertices[mesh_index];

				HashMap<Vector3, int>::Iterator iter_physics_index = vertex_to_physics.find(vertex);

				if (iter_physics_index == vertex_to_physics.end()) {
					physics_vertices.emplace_back(JPH::Float3((float)vertex.x, (float)vertex.y, (float)vertex.z), JPH::Float3(0.0f, 0.0f, 0.0f), 1.0f);

					iter_physics_index = vertex_to_physics.insert(vertex, physics_index_count++);
				}

				mesh_face[j] = mesh_index;
				physics_face[j] = iter_physics_index->value;
				mesh_to_physics[mesh_index] = iter_physics_index->value;
			}

			ERR_CONTINUE_MSG(is_face_degenerate(physics_face), vformat("Failed to append face to soft body '%s'. Face was found to be degenerate. Face consist of indices %d, %d and %d.", to_string(), mesh_face[0], mesh_face[1], mesh_face[2]));

			// Jolt uses a different winding order, so we swap the indices to account for that.
			physics_faces.emplace_back((JPH::uint32)physics_face[2], (JPH::uint32)physics_face[1], (JPH::uint32)physics_face[0]);
		}

		// Pin whatever pinned vertices we have currently. This is used during the `Optimize` call below to order the
		// constraints. Note that it's fine if the pinned vertices change later, but that will reduce the effectiveness
		// of the constraints a bit.
		pin_vertices(*this, pinned_vertices, mesh_to_physics, physics_vertices);

		// Since Godot's stiffness is input as a coefficient between 0 and 1, and Jolt uses actual stiffness for its
		// edge constraints, we crudely map one to the other with an arbitrary constant.
		const float stiffness = MAX(Math::pow(stiffness_coefficient, 3.0f) * 100000.0f, 0.000001f);
		const float inverse_stiffness = 1.0f / stiffness;

		JPH::SoftBodySharedSettings::VertexAttributes vertex_attrib;
		vertex_attrib.mCompliance = vertex_attrib.mShearCompliance = inverse_stiffness;

		settings.CreateConstraints(&vertex_attrib, 1, JPH::SoftBodySharedSettings::EBendType::None);
		settings.Optimize();
	} else {
		iter_shared_data->value.ref_count++;
	}

	shared = &iter_shared_data->value;

	return true;
}

void JoltSoftBody3D::_deref_shared_data() {
	if (unlikely(shared == nullptr)) {
		return;
	}

	HashMap<RID, Shared>::Iterator iter = mesh_to_shared.find(mesh);
	if (unlikely(iter == mesh_to_shared.end())) {
		return;
	}

	if (--iter->value.ref_count == 0) {
		mesh_to_shared.remove(iter);
	}

	shared = nullptr;
}

void JoltSoftBody3D::_update_mass() {
	if (!in_space()) {
		return;
	}

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	JPH::Array<JPH::SoftBodyVertex> &physics_vertices = motion_properties.GetVertices();

	const float inverse_vertex_mass = mass == 0.0f ? 1.0f : (float)physics_vertices.size() / mass;

	for (JPH::SoftBodyVertex &vertex : physics_vertices) {
		vertex.mInvMass = inverse_vertex_mass;
	}

	pin_vertices(*this, pinned_vertices, shared->mesh_to_physics, physics_vertices);
}

void JoltSoftBody3D::_update_pressure() {
	if (!in_space()) {
		jolt_settings->mPressure = pressure;
		return;
	}

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	motion_properties.SetPressure(pressure);
}

void JoltSoftBody3D::_update_damping() {
	if (!in_space()) {
		jolt_settings->mLinearDamping = linear_damping;
		return;
	}

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	motion_properties.SetLinearDamping(linear_damping);
}

void JoltSoftBody3D::_update_simulation_precision() {
	if (!in_space()) {
		jolt_settings->mNumIterations = (JPH::uint32)simulation_precision;
		return;
	}

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	motion_properties.SetNumIterations((JPH::uint32)simulation_precision);
}

void JoltSoftBody3D::_update_group_filter() {
	JPH::GroupFilter *group_filter = !exceptions.is_empty() ? JoltGroupFilter::instance : nullptr;

	if (!in_space()) {
		jolt_settings->mCollisionGroup.SetGroupFilter(group_filter);
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->GetCollisionGroup().SetGroupFilter(group_filter);
}

void JoltSoftBody3D::_try_rebuild() {
	if (space != nullptr) {
		_reset_space();
	}
}

void JoltSoftBody3D::_mesh_changed() {
	_try_rebuild();
}

void JoltSoftBody3D::_simulation_precision_changed() {
	wake_up();
}

void JoltSoftBody3D::_mass_changed() {
	wake_up();
}

void JoltSoftBody3D::_pressure_changed() {
	_update_pressure();
	wake_up();
}

void JoltSoftBody3D::_damping_changed() {
	_update_damping();
	wake_up();
}

void JoltSoftBody3D::_pins_changed() {
	_update_mass();
	wake_up();
}

void JoltSoftBody3D::_vertices_changed() {
	wake_up();
}

void JoltSoftBody3D::_exceptions_changed() {
	_update_group_filter();
}

JoltSoftBody3D::JoltSoftBody3D() :
		JoltObject3D(OBJECT_TYPE_SOFT_BODY) {
	jolt_settings->mRestitution = 0.0f;
	jolt_settings->mFriction = 1.0f;
	jolt_settings->mUpdatePosition = false;
	jolt_settings->mMakeRotationIdentity = false;
}

JoltSoftBody3D::~JoltSoftBody3D() {
	if (jolt_settings != nullptr) {
		delete jolt_settings;
		jolt_settings = nullptr;
	}
}

bool JoltSoftBody3D::in_space() const {
	return JoltObject3D::in_space() && shared != nullptr;
}

void JoltSoftBody3D::add_collision_exception(const RID &p_excepted_body) {
	exceptions.push_back(p_excepted_body);

	_exceptions_changed();
}

void JoltSoftBody3D::remove_collision_exception(const RID &p_excepted_body) {
	exceptions.erase(p_excepted_body);

	_exceptions_changed();
}

bool JoltSoftBody3D::has_collision_exception(const RID &p_excepted_body) const {
	return exceptions.find(p_excepted_body) >= 0;
}

bool JoltSoftBody3D::can_interact_with(const JoltBody3D &p_other) const {
	return (can_collide_with(p_other) || p_other.can_collide_with(*this)) && !has_collision_exception(p_other.get_rid()) && !p_other.has_collision_exception(rid);
}

bool JoltSoftBody3D::can_interact_with(const JoltSoftBody3D &p_other) const {
	return (can_collide_with(p_other) || p_other.can_collide_with(*this)) && !has_collision_exception(p_other.get_rid()) && !p_other.has_collision_exception(rid);
}

bool JoltSoftBody3D::can_interact_with(const JoltArea3D &p_other) const {
	return p_other.can_interact_with(*this);
}

Vector3 JoltSoftBody3D::get_velocity_at_position(const Vector3 &p_position) const {
	return Vector3();
}

void JoltSoftBody3D::set_mesh(const RID &p_mesh) {
	if (unlikely(mesh == p_mesh)) {
		return;
	}

	_deref_shared_data();

	mesh = p_mesh;

	_mesh_changed();
}

bool JoltSoftBody3D::is_sleeping() const {
	if (!in_space()) {
		return false;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), false);

	return !body->IsActive();
}

void JoltSoftBody3D::set_is_sleeping(bool p_enabled) {
	if (!in_space()) {
		return;
	}

	JPH::BodyInterface &body_iface = space->get_body_iface();

	if (p_enabled) {
		body_iface.DeactivateBody(jolt_id);
	} else {
		body_iface.ActivateBody(jolt_id);
	}
}

bool JoltSoftBody3D::can_sleep() const {
	if (!in_space()) {
		return true;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), false);

	return body->GetAllowSleeping();
}

void JoltSoftBody3D::set_can_sleep(bool p_enabled) {
	if (!in_space()) {
		return;
	}

	const JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	body->SetAllowSleeping(p_enabled);
}

void JoltSoftBody3D::set_simulation_precision(int p_precision) {
	if (unlikely(simulation_precision == p_precision)) {
		return;
	}

	simulation_precision = MAX(p_precision, 0);

	_simulation_precision_changed();
}

void JoltSoftBody3D::set_mass(float p_mass) {
	if (unlikely(mass == p_mass)) {
		return;
	}

	mass = MAX(p_mass, 0.0f);

	_mass_changed();
}

float JoltSoftBody3D::get_stiffness_coefficient() const {
	return stiffness_coefficient;
}

void JoltSoftBody3D::set_stiffness_coefficient(float p_coefficient) {
	stiffness_coefficient = CLAMP(p_coefficient, 0.0f, 1.0f);
}

void JoltSoftBody3D::set_pressure(float p_pressure) {
	if (unlikely(pressure == p_pressure)) {
		return;
	}

	pressure = MAX(p_pressure, 0.0f);

	_pressure_changed();
}

void JoltSoftBody3D::set_linear_damping(float p_damping) {
	if (unlikely(linear_damping == p_damping)) {
		return;
	}

	linear_damping = MAX(p_damping, 0.0f);

	_damping_changed();
}

float JoltSoftBody3D::get_drag() const {
	// Drag is not a thing in Jolt, and not supported by Godot Physics either.
	return 0.0f;
}

void JoltSoftBody3D::set_drag(float p_drag) {
	// Drag is not a thing in Jolt, and not supported by Godot Physics either.
}

Variant JoltSoftBody3D::get_state(PhysicsServer3D::BodyState p_state) const {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			return get_transform();
		}
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			ERR_FAIL_V_MSG(Variant(), "Linear velocity is not supported for soft bodies.");
		}
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			ERR_FAIL_V_MSG(Variant(), "Angular velocity is not supported for soft bodies.");
		}
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			return is_sleeping();
		}
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			return can_sleep();
		}
		default: {
			ERR_FAIL_V_MSG(Variant(), vformat("Unhandled body state: '%d'. This should not happen. Please report this.", p_state));
		}
	}
}

void JoltSoftBody3D::set_state(PhysicsServer3D::BodyState p_state, const Variant &p_value) {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			set_transform(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			ERR_FAIL_MSG("Linear velocity is not supported for soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			ERR_FAIL_MSG("Angular velocity is not supported for soft bodies.");
		} break;
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			set_is_sleeping(p_value);
		} break;
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			set_can_sleep(p_value);
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled body state: '%d'. This should not happen. Please report this.", p_state));
		} break;
	}
}

Transform3D JoltSoftBody3D::get_transform() const {
	// Since any transform gets baked into the vertices anyway we can just return identity here.
	return Transform3D();
}

void JoltSoftBody3D::set_transform(const Transform3D &p_transform) {
	ERR_FAIL_COND_MSG(!in_space(), vformat("Failed to set transform for '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	// For whatever reason this has to be interpreted as a relative global-space transform rather than an absolute one,
	// because `SoftBody3D` will immediately upon entering the scene tree set itself to be top-level and also set its
	// transform to be identity, while still expecting to stay in its original position.
	//
	// We also discard any scaling, since we have no way of scaling the actual edge lengths.
	const JPH::Mat44 relative_transform = to_jolt(p_transform.orthonormalized());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());
	JPH::Array<JPH::SoftBodyVertex> &physics_vertices = motion_properties.GetVertices();

	for (JPH::SoftBodyVertex &vertex : physics_vertices) {
		vertex.mPosition = vertex.mPreviousPosition = relative_transform * vertex.mPosition;
		vertex.mVelocity = JPH::Vec3::sZero();
	}
}

AABB JoltSoftBody3D::get_bounds() const {
	ERR_FAIL_COND_V_MSG(!in_space(), AABB(), vformat("Failed to retrieve world bounds of '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), AABB());

	return to_godot(body->GetWorldSpaceBounds());
}

void JoltSoftBody3D::update_rendering_server(PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) {
	// Ideally we would emit an actual error here, but that would spam the logs to the point where the actual cause will be drowned out.
	if (unlikely(!in_space())) {
		return;
	}

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	const JPH::SoftBodyMotionProperties &motion_properties = static_cast<const JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	typedef JPH::SoftBodyMotionProperties::Vertex SoftBodyVertex;
	typedef JPH::SoftBodyMotionProperties::Face SoftBodyFace;

	const JPH::Array<SoftBodyVertex> &physics_vertices = motion_properties.GetVertices();
	const JPH::Array<SoftBodyFace> &physics_faces = motion_properties.GetFaces();

	const int physics_vertex_count = (int)physics_vertices.size();

	normals.resize(physics_vertex_count);

	for (const SoftBodyFace &physics_face : physics_faces) {
		// Jolt uses a different winding order, so we swap the indices to account for that.

		const uint32_t i0 = physics_face.mVertex[2];
		const uint32_t i1 = physics_face.mVertex[1];
		const uint32_t i2 = physics_face.mVertex[0];

		const Vector3 v0 = to_godot(physics_vertices[i0].mPosition);
		const Vector3 v1 = to_godot(physics_vertices[i1].mPosition);
		const Vector3 v2 = to_godot(physics_vertices[i2].mPosition);

		const Vector3 normal = (v2 - v0).cross(v1 - v0).normalized();

		normals[i0] = normal;
		normals[i1] = normal;
		normals[i2] = normal;
	}

	const int mesh_vertex_count = shared->mesh_to_physics.size();

	for (int i = 0; i < mesh_vertex_count; ++i) {
		const int physics_index = shared->mesh_to_physics[i];

		const Vector3 vertex = to_godot(physics_vertices[(size_t)physics_index].mPosition);
		const Vector3 normal = normals[(uint32_t)physics_index];

		p_rendering_server_handler->set_vertex(i, vertex);
		p_rendering_server_handler->set_normal(i, normal);
	}

	p_rendering_server_handler->set_aabb(get_bounds());
}

Vector3 JoltSoftBody3D::get_vertex_position(int p_index) {
	ERR_FAIL_COND_V_MSG(!in_space(), Vector3(), vformat("Failed to retrieve point position for '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	ERR_FAIL_NULL_V(shared, Vector3());
	ERR_FAIL_INDEX_V(p_index, (int)shared->mesh_to_physics.size(), Vector3());
	const size_t physics_index = (size_t)shared->mesh_to_physics[p_index];

	const JoltReadableBody3D body = space->read_body(jolt_id);
	ERR_FAIL_COND_V(body.is_invalid(), Vector3());

	const JPH::SoftBodyMotionProperties &motion_properties = static_cast<const JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());
	const JPH::Array<JPH::SoftBodyVertex> &physics_vertices = motion_properties.GetVertices();
	const JPH::SoftBodyVertex &physics_vertex = physics_vertices[physics_index];

	return to_godot(body->GetCenterOfMassPosition() + physics_vertex.mPosition);
}

void JoltSoftBody3D::set_vertex_position(int p_index, const Vector3 &p_position) {
	ERR_FAIL_COND_MSG(!in_space(), vformat("Failed to set point position for '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	ERR_FAIL_NULL(shared);
	ERR_FAIL_INDEX(p_index, (int)shared->mesh_to_physics.size());
	const size_t physics_index = (size_t)shared->mesh_to_physics[p_index];

	const float last_step = space->get_last_step();
	if (unlikely(last_step == 0.0f)) {
		return;
	}

	JoltWritableBody3D body = space->write_body(jolt_id);
	ERR_FAIL_COND(body.is_invalid());

	JPH::SoftBodyMotionProperties &motion_properties = static_cast<JPH::SoftBodyMotionProperties &>(*body->GetMotionPropertiesUnchecked());

	JPH::Array<JPH::SoftBodyVertex> &physics_vertices = motion_properties.GetVertices();
	JPH::SoftBodyVertex &physics_vertex = physics_vertices[physics_index];

	const JPH::RVec3 center_of_mass = body->GetCenterOfMassPosition();
	const JPH::Vec3 local_position = JPH::Vec3(to_jolt_r(p_position) - center_of_mass);
	const JPH::Vec3 displacement = local_position - physics_vertex.mPosition;
	const JPH::Vec3 velocity = displacement / last_step;

	physics_vertex.mVelocity = velocity;

	_vertices_changed();
}

void JoltSoftBody3D::pin_vertex(int p_index) {
	pinned_vertices.insert(p_index);

	_pins_changed();
}

void JoltSoftBody3D::unpin_vertex(int p_index) {
	pinned_vertices.erase(p_index);

	_pins_changed();
}

void JoltSoftBody3D::unpin_all_vertices() {
	pinned_vertices.clear();

	_pins_changed();
}

bool JoltSoftBody3D::is_vertex_pinned(int p_index) const {
	ERR_FAIL_COND_V_MSG(!in_space(), false, vformat("Failed retrieve pin status of point for '%s'. Doing so without a physics space is not supported when using Jolt Physics. If this relates to a node, try adding the node to a scene tree first.", to_string()));

	ERR_FAIL_NULL_V(shared, false);
	ERR_FAIL_INDEX_V(p_index, (int)shared->mesh_to_physics.size(), false);
	const int physics_index = shared->mesh_to_physics[p_index];

	return pinned_vertices.has(physics_index);
}

String JoltSoftBody3D::to_string() const {
	Object *instance = get_instance();
	return instance != nullptr ? instance->to_string() : "<unknown>";
}
