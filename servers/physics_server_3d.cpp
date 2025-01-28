/**************************************************************************/
/*  physics_server_3d.cpp                                                 */
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

#ifndef _3D_DISABLED

#include "physics_server_3d.h"

#include "core/config/project_settings.h"
#include "core/variant/typed_array.h"

void PhysicsServer3DRenderingServerHandler::set_vertex(int p_vertex_id, const Vector3 &p_vertex) {
	GDVIRTUAL_CALL(_set_vertex, p_vertex_id, p_vertex);
}
void PhysicsServer3DRenderingServerHandler::set_normal(int p_vertex_id, const Vector3 &p_normal) {
	GDVIRTUAL_CALL(_set_normal, p_vertex_id, p_normal);
}
void PhysicsServer3DRenderingServerHandler::set_aabb(const AABB &p_aabb) {
	GDVIRTUAL_CALL(_set_aabb, p_aabb);
}

void PhysicsServer3DRenderingServerHandler::_bind_methods() {
	GDVIRTUAL_BIND(_set_vertex, "vertex_id", "vertex");
	GDVIRTUAL_BIND(_set_normal, "vertex_id", "normal");
	GDVIRTUAL_BIND(_set_aabb, "aabb");

	ClassDB::bind_method(D_METHOD("set_vertex", "vertex_id", "vertex"), &PhysicsServer3DRenderingServerHandler::set_vertex);
	ClassDB::bind_method(D_METHOD("set_normal", "vertex_id", "normal"), &PhysicsServer3DRenderingServerHandler::set_normal);
	ClassDB::bind_method(D_METHOD("set_aabb", "aabb"), &PhysicsServer3DRenderingServerHandler::set_aabb);
}

PhysicsServer3D *PhysicsServer3D::singleton = nullptr;

void PhysicsDirectBodyState3D::integrate_forces() {
	real_t step = get_step();
	Vector3 lv = get_linear_velocity();
	lv += get_total_gravity() * step;

	Vector3 av = get_angular_velocity();

	real_t linear_damp = 1.0 - step * get_total_linear_damp();

	if (linear_damp < 0) { // reached zero in the given time
		linear_damp = 0;
	}

	real_t angular_damp = 1.0 - step * get_total_angular_damp();

	if (angular_damp < 0) { // reached zero in the given time
		angular_damp = 0;
	}

	lv *= linear_damp;
	av *= angular_damp;

	set_linear_velocity(lv);
	set_angular_velocity(av);
}

Object *PhysicsDirectBodyState3D::get_contact_collider_object(int p_contact_idx) const {
	ObjectID objid = get_contact_collider_id(p_contact_idx);
	Object *obj = ObjectDB::get_instance(objid);
	return obj;
}

PhysicsServer3D *PhysicsServer3D::get_singleton() {
	return singleton;
}

void PhysicsDirectBodyState3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_total_gravity"), &PhysicsDirectBodyState3D::get_total_gravity);
	ClassDB::bind_method(D_METHOD("get_total_linear_damp"), &PhysicsDirectBodyState3D::get_total_linear_damp);
	ClassDB::bind_method(D_METHOD("get_total_angular_damp"), &PhysicsDirectBodyState3D::get_total_angular_damp);

	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &PhysicsDirectBodyState3D::get_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_local"), &PhysicsDirectBodyState3D::get_center_of_mass_local);
	ClassDB::bind_method(D_METHOD("get_principal_inertia_axes"), &PhysicsDirectBodyState3D::get_principal_inertia_axes);

	ClassDB::bind_method(D_METHOD("get_inverse_mass"), &PhysicsDirectBodyState3D::get_inverse_mass);
	ClassDB::bind_method(D_METHOD("get_inverse_inertia"), &PhysicsDirectBodyState3D::get_inverse_inertia);
	ClassDB::bind_method(D_METHOD("get_inverse_inertia_tensor"), &PhysicsDirectBodyState3D::get_inverse_inertia_tensor);

	ClassDB::bind_method(D_METHOD("set_linear_velocity", "velocity"), &PhysicsDirectBodyState3D::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysicsDirectBodyState3D::get_linear_velocity);

	ClassDB::bind_method(D_METHOD("set_angular_velocity", "velocity"), &PhysicsDirectBodyState3D::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysicsDirectBodyState3D::get_angular_velocity);

	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsDirectBodyState3D::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsDirectBodyState3D::get_transform);

	ClassDB::bind_method(D_METHOD("get_velocity_at_local_position", "local_position"), &PhysicsDirectBodyState3D::get_velocity_at_local_position);

	ClassDB::bind_method(D_METHOD("apply_central_impulse", "impulse"), &PhysicsDirectBodyState3D::apply_central_impulse, Vector3());
	ClassDB::bind_method(D_METHOD("apply_impulse", "impulse", "position"), &PhysicsDirectBodyState3D::apply_impulse, Vector3());
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &PhysicsDirectBodyState3D::apply_torque_impulse);

	ClassDB::bind_method(D_METHOD("apply_central_force", "force"), &PhysicsDirectBodyState3D::apply_central_force, Vector3());
	ClassDB::bind_method(D_METHOD("apply_force", "force", "position"), &PhysicsDirectBodyState3D::apply_force, Vector3());
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &PhysicsDirectBodyState3D::apply_torque);

	ClassDB::bind_method(D_METHOD("add_constant_central_force", "force"), &PhysicsDirectBodyState3D::add_constant_central_force, Vector3());
	ClassDB::bind_method(D_METHOD("add_constant_force", "force", "position"), &PhysicsDirectBodyState3D::add_constant_force, Vector3());
	ClassDB::bind_method(D_METHOD("add_constant_torque", "torque"), &PhysicsDirectBodyState3D::add_constant_torque);

	ClassDB::bind_method(D_METHOD("set_constant_force", "force"), &PhysicsDirectBodyState3D::set_constant_force);
	ClassDB::bind_method(D_METHOD("get_constant_force"), &PhysicsDirectBodyState3D::get_constant_force);

	ClassDB::bind_method(D_METHOD("set_constant_torque", "torque"), &PhysicsDirectBodyState3D::set_constant_torque);
	ClassDB::bind_method(D_METHOD("get_constant_torque"), &PhysicsDirectBodyState3D::get_constant_torque);

	ClassDB::bind_method(D_METHOD("set_sleep_state", "enabled"), &PhysicsDirectBodyState3D::set_sleep_state);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &PhysicsDirectBodyState3D::is_sleeping);

	ClassDB::bind_method(D_METHOD("get_contact_count"), &PhysicsDirectBodyState3D::get_contact_count);

	ClassDB::bind_method(D_METHOD("get_contact_local_position", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_local_position);
	ClassDB::bind_method(D_METHOD("get_contact_local_normal", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_local_normal);
	ClassDB::bind_method(D_METHOD("get_contact_impulse", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_impulse);
	ClassDB::bind_method(D_METHOD("get_contact_local_shape", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_local_shape);
	ClassDB::bind_method(D_METHOD("get_contact_local_velocity_at_position", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_local_velocity_at_position);
	ClassDB::bind_method(D_METHOD("get_contact_collider", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider);
	ClassDB::bind_method(D_METHOD("get_contact_collider_position", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider_position);
	ClassDB::bind_method(D_METHOD("get_contact_collider_id", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider_id);
	ClassDB::bind_method(D_METHOD("get_contact_collider_object", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider_object);
	ClassDB::bind_method(D_METHOD("get_contact_collider_shape", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider_shape);
	ClassDB::bind_method(D_METHOD("get_contact_collider_velocity_at_position", "contact_idx"), &PhysicsDirectBodyState3D::get_contact_collider_velocity_at_position);
	ClassDB::bind_method(D_METHOD("get_step"), &PhysicsDirectBodyState3D::get_step);
	ClassDB::bind_method(D_METHOD("integrate_forces"), &PhysicsDirectBodyState3D::integrate_forces);
	ClassDB::bind_method(D_METHOD("get_space_state"), &PhysicsDirectBodyState3D::get_space_state);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step"), "", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inverse_mass"), "", "get_inverse_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_angular_damp"), "", "get_total_angular_damp");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_linear_damp"), "", "get_total_linear_damp");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "inverse_inertia"), "", "get_inverse_inertia");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS, "inverse_inertia_tensor"), "", "get_inverse_inertia_tensor");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "total_gravity"), "", "get_total_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass"), "", "get_center_of_mass");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass_local"), "", "get_center_of_mass_local");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS, "principal_inertia_axes"), "", "get_principal_inertia_axes");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sleeping"), "set_sleep_state", "is_sleeping");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "transform"), "set_transform", "get_transform");
}

PhysicsDirectBodyState3D::PhysicsDirectBodyState3D() {}

///////////////////////////////////////////////////////

void PhysicsRayQueryParameters3D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsRayQueryParameters3D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsRayQueryParameters3D::_bind_methods() {
	ClassDB::bind_static_method("PhysicsRayQueryParameters3D", D_METHOD("create", "from", "to", "collision_mask", "exclude"), &PhysicsRayQueryParameters3D::create, DEFVAL(UINT32_MAX), DEFVAL(TypedArray<RID>()));

	ClassDB::bind_method(D_METHOD("set_from", "from"), &PhysicsRayQueryParameters3D::set_from);
	ClassDB::bind_method(D_METHOD("get_from"), &PhysicsRayQueryParameters3D::get_from);

	ClassDB::bind_method(D_METHOD("set_to", "to"), &PhysicsRayQueryParameters3D::set_to);
	ClassDB::bind_method(D_METHOD("get_to"), &PhysicsRayQueryParameters3D::get_to);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsRayQueryParameters3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsRayQueryParameters3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsRayQueryParameters3D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsRayQueryParameters3D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsRayQueryParameters3D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsRayQueryParameters3D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsRayQueryParameters3D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsRayQueryParameters3D::is_collide_with_areas_enabled);

	ClassDB::bind_method(D_METHOD("set_hit_from_inside", "enable"), &PhysicsRayQueryParameters3D::set_hit_from_inside);
	ClassDB::bind_method(D_METHOD("is_hit_from_inside_enabled"), &PhysicsRayQueryParameters3D::is_hit_from_inside_enabled);

	ClassDB::bind_method(D_METHOD("set_hit_back_faces", "enable"), &PhysicsRayQueryParameters3D::set_hit_back_faces);
	ClassDB::bind_method(D_METHOD("is_hit_back_faces_enabled"), &PhysicsRayQueryParameters3D::is_hit_back_faces_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "from"), "set_from", "get_from");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "to"), "set_to", "get_to");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hit_from_inside"), "set_hit_from_inside", "is_hit_from_inside_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hit_back_faces"), "set_hit_back_faces", "is_hit_back_faces_enabled");
}

///////////////////////////////////////////////////////

Ref<PhysicsRayQueryParameters3D> PhysicsRayQueryParameters3D::create(Vector3 p_from, Vector3 p_to, uint32_t p_mask, const TypedArray<RID> &p_exclude) {
	Ref<PhysicsRayQueryParameters3D> params;
	params.instantiate();
	params->set_from(p_from);
	params->set_to(p_to);
	params->set_collision_mask(p_mask);
	params->set_exclude(p_exclude);
	return params;
}

void PhysicsPointQueryParameters3D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsPointQueryParameters3D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsPointQueryParameters3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_position", "position"), &PhysicsPointQueryParameters3D::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysicsPointQueryParameters3D::get_position);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsPointQueryParameters3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsPointQueryParameters3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsPointQueryParameters3D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsPointQueryParameters3D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsPointQueryParameters3D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsPointQueryParameters3D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsPointQueryParameters3D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsPointQueryParameters3D::is_collide_with_areas_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

///////////////////////////////////////////////////////

void PhysicsShapeQueryParameters3D::set_shape(const Ref<Resource> &p_shape_ref) {
	ERR_FAIL_COND(p_shape_ref.is_null());
	shape_ref = p_shape_ref;
	parameters.shape_rid = p_shape_ref->get_rid();
}

void PhysicsShapeQueryParameters3D::set_shape_rid(const RID &p_shape) {
	if (parameters.shape_rid != p_shape) {
		shape_ref = Ref<Resource>();
		parameters.shape_rid = p_shape;
	}
}

void PhysicsShapeQueryParameters3D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsShapeQueryParameters3D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsShapeQueryParameters3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysicsShapeQueryParameters3D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysicsShapeQueryParameters3D::get_shape);

	ClassDB::bind_method(D_METHOD("set_shape_rid", "shape"), &PhysicsShapeQueryParameters3D::set_shape_rid);
	ClassDB::bind_method(D_METHOD("get_shape_rid"), &PhysicsShapeQueryParameters3D::get_shape_rid);

	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsShapeQueryParameters3D::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsShapeQueryParameters3D::get_transform);

	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &PhysicsShapeQueryParameters3D::set_motion);
	ClassDB::bind_method(D_METHOD("get_motion"), &PhysicsShapeQueryParameters3D::get_motion);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &PhysicsShapeQueryParameters3D::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &PhysicsShapeQueryParameters3D::get_margin);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsShapeQueryParameters3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsShapeQueryParameters3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsShapeQueryParameters3D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsShapeQueryParameters3D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsShapeQueryParameters3D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsShapeQueryParameters3D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsShapeQueryParameters3D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsShapeQueryParameters3D::is_collide_with_areas_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0,100,0.01"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "motion"), "set_motion", "get_motion");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::RID, "shape_rid"), "set_shape_rid", "get_shape_rid");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

/////////////////////////////////////

Dictionary PhysicsDirectSpaceState3D::_intersect_ray(const Ref<PhysicsRayQueryParameters3D> &p_ray_query) {
	ERR_FAIL_COND_V(p_ray_query.is_null(), Dictionary());

	RayResult result;
	bool res = intersect_ray(p_ray_query->get_parameters(), result);

	if (!res) {
		return Dictionary();
	}

	Dictionary d;
	d["position"] = result.position;
	d["normal"] = result.normal;
	d["face_index"] = result.face_index;
	d["collider_id"] = result.collider_id;
	d["collider"] = result.collider;
	d["shape"] = result.shape;
	d["rid"] = result.rid;

	return d;
}

TypedArray<Dictionary> PhysicsDirectSpaceState3D::_intersect_point(const Ref<PhysicsPointQueryParameters3D> &p_point_query, int p_max_results) {
	ERR_FAIL_COND_V(p_point_query.is_null(), TypedArray<Dictionary>());

	Vector<ShapeResult> ret;
	ret.resize(p_max_results);

	int rc = intersect_point(p_point_query->get_parameters(), ret.ptrw(), ret.size());

	if (rc == 0) {
		return TypedArray<Dictionary>();
	}

	TypedArray<Dictionary> r;
	r.resize(rc);
	for (int i = 0; i < rc; i++) {
		Dictionary d;
		d["rid"] = ret[i].rid;
		d["collider_id"] = ret[i].collider_id;
		d["collider"] = ret[i].collider;
		d["shape"] = ret[i].shape;
		r[i] = d;
	}
	return r;
}

TypedArray<Dictionary> PhysicsDirectSpaceState3D::_intersect_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), TypedArray<Dictionary>());

	Vector<ShapeResult> sr;
	sr.resize(p_max_results);
	int rc = intersect_shape(p_shape_query->get_parameters(), sr.ptrw(), sr.size());
	TypedArray<Dictionary> ret;
	ret.resize(rc);
	for (int i = 0; i < rc; i++) {
		Dictionary d;
		d["rid"] = sr[i].rid;
		d["collider_id"] = sr[i].collider_id;
		d["collider"] = sr[i].collider;
		d["shape"] = sr[i].shape;
		ret[i] = d;
	}

	return ret;
}

Vector<real_t> PhysicsDirectSpaceState3D::_cast_motion(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), Vector<real_t>());

	real_t closest_safe = 1.0f, closest_unsafe = 1.0f;
	bool res = cast_motion(p_shape_query->get_parameters(), closest_safe, closest_unsafe);
	if (!res) {
		return Vector<real_t>();
	}
	Vector<real_t> ret;
	ret.resize(2);
	ret.write[0] = closest_safe;
	ret.write[1] = closest_unsafe;
	return ret;
}

TypedArray<Vector3> PhysicsDirectSpaceState3D::_collide_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), TypedArray<Vector3>());

	Vector<Vector3> ret;
	ret.resize(p_max_results * 2);
	int rc = 0;
	bool res = collide_shape(p_shape_query->get_parameters(), ret.ptrw(), p_max_results, rc);
	if (!res) {
		return TypedArray<Vector3>();
	}
	TypedArray<Vector3> r;
	r.resize(rc * 2);
	for (int i = 0; i < rc * 2; i++) {
		r[i] = ret[i];
	}
	return r;
}

Dictionary PhysicsDirectSpaceState3D::_get_rest_info(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), Dictionary());

	ShapeRestInfo sri;

	bool res = rest_info(p_shape_query->get_parameters(), &sri);
	Dictionary r;
	if (!res) {
		return r;
	}

	r["point"] = sri.point;
	r["normal"] = sri.normal;
	r["rid"] = sri.rid;
	r["collider_id"] = sri.collider_id;
	r["shape"] = sri.shape;
	r["linear_velocity"] = sri.linear_velocity;

	return r;
}

PhysicsDirectSpaceState3D::PhysicsDirectSpaceState3D() {
}

void PhysicsDirectSpaceState3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("intersect_point", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_point, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("intersect_ray", "parameters"), &PhysicsDirectSpaceState3D::_intersect_ray);
	ClassDB::bind_method(D_METHOD("intersect_shape", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_intersect_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("cast_motion", "parameters"), &PhysicsDirectSpaceState3D::_cast_motion);
	ClassDB::bind_method(D_METHOD("collide_shape", "parameters", "max_results"), &PhysicsDirectSpaceState3D::_collide_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("get_rest_info", "parameters"), &PhysicsDirectSpaceState3D::_get_rest_info);
}

///////////////////////////////

TypedArray<RID> PhysicsTestMotionParameters3D::get_exclude_bodies() const {
	TypedArray<RID> exclude;
	exclude.resize(parameters.exclude_bodies.size());

	int body_index = 0;
	for (const RID &body : parameters.exclude_bodies) {
		exclude[body_index++] = body;
	}

	return exclude;
}

void PhysicsTestMotionParameters3D::set_exclude_bodies(const TypedArray<RID> &p_exclude) {
	parameters.exclude_bodies.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude_bodies.insert(p_exclude[i]);
	}
}

TypedArray<uint64_t> PhysicsTestMotionParameters3D::get_exclude_objects() const {
	TypedArray<uint64_t> exclude;
	exclude.resize(parameters.exclude_objects.size());

	int object_index = 0;
	for (const ObjectID &object_id : parameters.exclude_objects) {
		exclude[object_index++] = object_id;
	}

	return exclude;
}

void PhysicsTestMotionParameters3D::set_exclude_objects(const TypedArray<uint64_t> &p_exclude) {
	parameters.exclude_objects.clear();
	for (int i = 0; i < p_exclude.size(); ++i) {
		ObjectID object_id = p_exclude[i];
		ERR_CONTINUE(object_id.is_null());
		parameters.exclude_objects.insert(object_id);
	}
}

void PhysicsTestMotionParameters3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_from"), &PhysicsTestMotionParameters3D::get_from);
	ClassDB::bind_method(D_METHOD("set_from", "from"), &PhysicsTestMotionParameters3D::set_from);

	ClassDB::bind_method(D_METHOD("get_motion"), &PhysicsTestMotionParameters3D::get_motion);
	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &PhysicsTestMotionParameters3D::set_motion);

	ClassDB::bind_method(D_METHOD("get_margin"), &PhysicsTestMotionParameters3D::get_margin);
	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &PhysicsTestMotionParameters3D::set_margin);

	ClassDB::bind_method(D_METHOD("get_max_collisions"), &PhysicsTestMotionParameters3D::get_max_collisions);
	ClassDB::bind_method(D_METHOD("set_max_collisions", "max_collisions"), &PhysicsTestMotionParameters3D::set_max_collisions);

	ClassDB::bind_method(D_METHOD("is_collide_separation_ray_enabled"), &PhysicsTestMotionParameters3D::is_collide_separation_ray_enabled);
	ClassDB::bind_method(D_METHOD("set_collide_separation_ray_enabled", "enabled"), &PhysicsTestMotionParameters3D::set_collide_separation_ray_enabled);

	ClassDB::bind_method(D_METHOD("get_exclude_bodies"), &PhysicsTestMotionParameters3D::get_exclude_bodies);
	ClassDB::bind_method(D_METHOD("set_exclude_bodies", "exclude_list"), &PhysicsTestMotionParameters3D::set_exclude_bodies);

	ClassDB::bind_method(D_METHOD("get_exclude_objects"), &PhysicsTestMotionParameters3D::get_exclude_objects);
	ClassDB::bind_method(D_METHOD("set_exclude_objects", "exclude_list"), &PhysicsTestMotionParameters3D::set_exclude_objects);

	ClassDB::bind_method(D_METHOD("is_recovery_as_collision_enabled"), &PhysicsTestMotionParameters3D::is_recovery_as_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_recovery_as_collision_enabled", "enabled"), &PhysicsTestMotionParameters3D::set_recovery_as_collision_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "from"), "set_from", "get_from");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "motion"), "set_motion", "get_motion");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_collisions"), "set_max_collisions", "get_max_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_separation_ray"), "set_collide_separation_ray_enabled", "is_collide_separation_ray_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude_bodies", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude_bodies", "get_exclude_bodies");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude_objects"), "set_exclude_objects", "get_exclude_objects");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "recovery_as_collision"), "set_recovery_as_collision_enabled", "is_recovery_as_collision_enabled");
}

///////////////////////////////

Vector3 PhysicsTestMotionResult3D::get_travel() const {
	return result.travel;
}

Vector3 PhysicsTestMotionResult3D::get_remainder() const {
	return result.remainder;
}

real_t PhysicsTestMotionResult3D::get_collision_safe_fraction() const {
	return result.collision_safe_fraction;
}

real_t PhysicsTestMotionResult3D::get_collision_unsafe_fraction() const {
	return result.collision_unsafe_fraction;
}

int PhysicsTestMotionResult3D::get_collision_count() const {
	return result.collision_count;
}

Vector3 PhysicsTestMotionResult3D::get_collision_point(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, Vector3());
	return result.collisions[p_collision_index].position;
}

Vector3 PhysicsTestMotionResult3D::get_collision_normal(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, Vector3());
	return result.collisions[p_collision_index].normal;
}

Vector3 PhysicsTestMotionResult3D::get_collider_velocity(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, Vector3());
	return result.collisions[p_collision_index].collider_velocity;
}

ObjectID PhysicsTestMotionResult3D::get_collider_id(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, ObjectID());
	return result.collisions[p_collision_index].collider_id;
}

RID PhysicsTestMotionResult3D::get_collider_rid(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, RID());
	return result.collisions[p_collision_index].collider;
}

Object *PhysicsTestMotionResult3D::get_collider(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, nullptr);
	return ObjectDB::get_instance(result.collisions[p_collision_index].collider_id);
}

int PhysicsTestMotionResult3D::get_collider_shape(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, 0);
	return result.collisions[p_collision_index].collider_shape;
}

int PhysicsTestMotionResult3D::get_collision_local_shape(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, 0);
	return result.collisions[p_collision_index].local_shape;
}

real_t PhysicsTestMotionResult3D::get_collision_depth(int p_collision_index) const {
	ERR_FAIL_INDEX_V(p_collision_index, result.collision_count, 0.0);
	return result.collisions[p_collision_index].depth;
}

void PhysicsTestMotionResult3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_travel"), &PhysicsTestMotionResult3D::get_travel);
	ClassDB::bind_method(D_METHOD("get_remainder"), &PhysicsTestMotionResult3D::get_remainder);
	ClassDB::bind_method(D_METHOD("get_collision_safe_fraction"), &PhysicsTestMotionResult3D::get_collision_safe_fraction);
	ClassDB::bind_method(D_METHOD("get_collision_unsafe_fraction"), &PhysicsTestMotionResult3D::get_collision_unsafe_fraction);
	ClassDB::bind_method(D_METHOD("get_collision_count"), &PhysicsTestMotionResult3D::get_collision_count);
	ClassDB::bind_method(D_METHOD("get_collision_point", "collision_index"), &PhysicsTestMotionResult3D::get_collision_point, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collision_normal", "collision_index"), &PhysicsTestMotionResult3D::get_collision_normal, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collider_velocity", "collision_index"), &PhysicsTestMotionResult3D::get_collider_velocity, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collider_id", "collision_index"), &PhysicsTestMotionResult3D::get_collider_id, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collider_rid", "collision_index"), &PhysicsTestMotionResult3D::get_collider_rid, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collider", "collision_index"), &PhysicsTestMotionResult3D::get_collider, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collider_shape", "collision_index"), &PhysicsTestMotionResult3D::get_collider_shape, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collision_local_shape", "collision_index"), &PhysicsTestMotionResult3D::get_collision_local_shape, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_collision_depth", "collision_index"), &PhysicsTestMotionResult3D::get_collision_depth, DEFVAL(0));
}

///////////////////////////////////////

bool PhysicsServer3D::_body_test_motion(RID p_body, const Ref<PhysicsTestMotionParameters3D> &p_parameters, const Ref<PhysicsTestMotionResult3D> &p_result) {
	ERR_FAIL_COND_V(p_parameters.is_null(), false);

	MotionResult *result_ptr = nullptr;
	if (p_result.is_valid()) {
		result_ptr = p_result->get_result_ptr();
	}

	return body_test_motion(p_body, p_parameters->get_parameters(), result_ptr);
}

RID PhysicsServer3D::shape_create(ShapeType p_shape) {
	switch (p_shape) {
		case SHAPE_WORLD_BOUNDARY:
			return world_boundary_shape_create();
		case SHAPE_SEPARATION_RAY:
			return separation_ray_shape_create();
		case SHAPE_SPHERE:
			return sphere_shape_create();
		case SHAPE_BOX:
			return box_shape_create();
		case SHAPE_CAPSULE:
			return capsule_shape_create();
		case SHAPE_CYLINDER:
			return cylinder_shape_create();
		case SHAPE_CONVEX_POLYGON:
			return convex_polygon_shape_create();
		case SHAPE_CONCAVE_POLYGON:
			return concave_polygon_shape_create();
		case SHAPE_HEIGHTMAP:
			return heightmap_shape_create();
		case SHAPE_CUSTOM:
			return custom_shape_create();
		default:
			return RID();
	}
}

void PhysicsServer3D::_bind_methods() {
#ifndef _3D_DISABLED

	ClassDB::bind_method(D_METHOD("world_boundary_shape_create"), &PhysicsServer3D::world_boundary_shape_create);
	ClassDB::bind_method(D_METHOD("separation_ray_shape_create"), &PhysicsServer3D::separation_ray_shape_create);
	ClassDB::bind_method(D_METHOD("sphere_shape_create"), &PhysicsServer3D::sphere_shape_create);
	ClassDB::bind_method(D_METHOD("box_shape_create"), &PhysicsServer3D::box_shape_create);
	ClassDB::bind_method(D_METHOD("capsule_shape_create"), &PhysicsServer3D::capsule_shape_create);
	ClassDB::bind_method(D_METHOD("cylinder_shape_create"), &PhysicsServer3D::cylinder_shape_create);
	ClassDB::bind_method(D_METHOD("convex_polygon_shape_create"), &PhysicsServer3D::convex_polygon_shape_create);
	ClassDB::bind_method(D_METHOD("concave_polygon_shape_create"), &PhysicsServer3D::concave_polygon_shape_create);
	ClassDB::bind_method(D_METHOD("heightmap_shape_create"), &PhysicsServer3D::heightmap_shape_create);
	ClassDB::bind_method(D_METHOD("custom_shape_create"), &PhysicsServer3D::custom_shape_create);

	ClassDB::bind_method(D_METHOD("shape_set_data", "shape", "data"), &PhysicsServer3D::shape_set_data);
	ClassDB::bind_method(D_METHOD("shape_set_margin", "shape", "margin"), &PhysicsServer3D::shape_set_margin);

	ClassDB::bind_method(D_METHOD("shape_get_type", "shape"), &PhysicsServer3D::shape_get_type);
	ClassDB::bind_method(D_METHOD("shape_get_data", "shape"), &PhysicsServer3D::shape_get_data);
	ClassDB::bind_method(D_METHOD("shape_get_margin", "shape"), &PhysicsServer3D::shape_get_margin);

	ClassDB::bind_method(D_METHOD("space_create"), &PhysicsServer3D::space_create);
	ClassDB::bind_method(D_METHOD("space_set_active", "space", "active"), &PhysicsServer3D::space_set_active);
	ClassDB::bind_method(D_METHOD("space_is_active", "space"), &PhysicsServer3D::space_is_active);
	ClassDB::bind_method(D_METHOD("space_set_param", "space", "param", "value"), &PhysicsServer3D::space_set_param);
	ClassDB::bind_method(D_METHOD("space_get_param", "space", "param"), &PhysicsServer3D::space_get_param);
	ClassDB::bind_method(D_METHOD("space_get_direct_state", "space"), &PhysicsServer3D::space_get_direct_state);

	ClassDB::bind_method(D_METHOD("area_create"), &PhysicsServer3D::area_create);
	ClassDB::bind_method(D_METHOD("area_set_space", "area", "space"), &PhysicsServer3D::area_set_space);
	ClassDB::bind_method(D_METHOD("area_get_space", "area"), &PhysicsServer3D::area_get_space);

	ClassDB::bind_method(D_METHOD("area_add_shape", "area", "shape", "transform", "disabled"), &PhysicsServer3D::area_add_shape, DEFVAL(Transform3D()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("area_set_shape", "area", "shape_idx", "shape"), &PhysicsServer3D::area_set_shape);
	ClassDB::bind_method(D_METHOD("area_set_shape_transform", "area", "shape_idx", "transform"), &PhysicsServer3D::area_set_shape_transform);
	ClassDB::bind_method(D_METHOD("area_set_shape_disabled", "area", "shape_idx", "disabled"), &PhysicsServer3D::area_set_shape_disabled);

	ClassDB::bind_method(D_METHOD("area_get_shape_count", "area"), &PhysicsServer3D::area_get_shape_count);
	ClassDB::bind_method(D_METHOD("area_get_shape", "area", "shape_idx"), &PhysicsServer3D::area_get_shape);
	ClassDB::bind_method(D_METHOD("area_get_shape_transform", "area", "shape_idx"), &PhysicsServer3D::area_get_shape_transform);

	ClassDB::bind_method(D_METHOD("area_remove_shape", "area", "shape_idx"), &PhysicsServer3D::area_remove_shape);
	ClassDB::bind_method(D_METHOD("area_clear_shapes", "area"), &PhysicsServer3D::area_clear_shapes);

	ClassDB::bind_method(D_METHOD("area_set_collision_layer", "area", "layer"), &PhysicsServer3D::area_set_collision_layer);
	ClassDB::bind_method(D_METHOD("area_get_collision_layer", "area"), &PhysicsServer3D::area_get_collision_layer);

	ClassDB::bind_method(D_METHOD("area_set_collision_mask", "area", "mask"), &PhysicsServer3D::area_set_collision_mask);
	ClassDB::bind_method(D_METHOD("area_get_collision_mask", "area"), &PhysicsServer3D::area_get_collision_mask);

	ClassDB::bind_method(D_METHOD("area_set_param", "area", "param", "value"), &PhysicsServer3D::area_set_param);
	ClassDB::bind_method(D_METHOD("area_set_transform", "area", "transform"), &PhysicsServer3D::area_set_transform);

	ClassDB::bind_method(D_METHOD("area_get_param", "area", "param"), &PhysicsServer3D::area_get_param);
	ClassDB::bind_method(D_METHOD("area_get_transform", "area"), &PhysicsServer3D::area_get_transform);

	ClassDB::bind_method(D_METHOD("area_attach_object_instance_id", "area", "id"), &PhysicsServer3D::area_attach_object_instance_id);
	ClassDB::bind_method(D_METHOD("area_get_object_instance_id", "area"), &PhysicsServer3D::area_get_object_instance_id);

	ClassDB::bind_method(D_METHOD("area_set_monitor_callback", "area", "callback"), &PhysicsServer3D::area_set_monitor_callback);
	ClassDB::bind_method(D_METHOD("area_set_area_monitor_callback", "area", "callback"), &PhysicsServer3D::area_set_area_monitor_callback);
	ClassDB::bind_method(D_METHOD("area_set_monitorable", "area", "monitorable"), &PhysicsServer3D::area_set_monitorable);

	ClassDB::bind_method(D_METHOD("area_set_ray_pickable", "area", "enable"), &PhysicsServer3D::area_set_ray_pickable);

	ClassDB::bind_method(D_METHOD("body_create"), &PhysicsServer3D::body_create);

	ClassDB::bind_method(D_METHOD("body_set_space", "body", "space"), &PhysicsServer3D::body_set_space);
	ClassDB::bind_method(D_METHOD("body_get_space", "body"), &PhysicsServer3D::body_get_space);

	ClassDB::bind_method(D_METHOD("body_set_mode", "body", "mode"), &PhysicsServer3D::body_set_mode);
	ClassDB::bind_method(D_METHOD("body_get_mode", "body"), &PhysicsServer3D::body_get_mode);

	ClassDB::bind_method(D_METHOD("body_set_collision_layer", "body", "layer"), &PhysicsServer3D::body_set_collision_layer);
	ClassDB::bind_method(D_METHOD("body_get_collision_layer", "body"), &PhysicsServer3D::body_get_collision_layer);

	ClassDB::bind_method(D_METHOD("body_set_collision_mask", "body", "mask"), &PhysicsServer3D::body_set_collision_mask);
	ClassDB::bind_method(D_METHOD("body_get_collision_mask", "body"), &PhysicsServer3D::body_get_collision_mask);

	ClassDB::bind_method(D_METHOD("body_set_collision_priority", "body", "priority"), &PhysicsServer3D::body_set_collision_priority);
	ClassDB::bind_method(D_METHOD("body_get_collision_priority", "body"), &PhysicsServer3D::body_get_collision_priority);

	ClassDB::bind_method(D_METHOD("body_add_shape", "body", "shape", "transform", "disabled"), &PhysicsServer3D::body_add_shape, DEFVAL(Transform3D()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("body_set_shape", "body", "shape_idx", "shape"), &PhysicsServer3D::body_set_shape);
	ClassDB::bind_method(D_METHOD("body_set_shape_transform", "body", "shape_idx", "transform"), &PhysicsServer3D::body_set_shape_transform);
	ClassDB::bind_method(D_METHOD("body_set_shape_disabled", "body", "shape_idx", "disabled"), &PhysicsServer3D::body_set_shape_disabled);

	ClassDB::bind_method(D_METHOD("body_get_shape_count", "body"), &PhysicsServer3D::body_get_shape_count);
	ClassDB::bind_method(D_METHOD("body_get_shape", "body", "shape_idx"), &PhysicsServer3D::body_get_shape);
	ClassDB::bind_method(D_METHOD("body_get_shape_transform", "body", "shape_idx"), &PhysicsServer3D::body_get_shape_transform);

	ClassDB::bind_method(D_METHOD("body_remove_shape", "body", "shape_idx"), &PhysicsServer3D::body_remove_shape);
	ClassDB::bind_method(D_METHOD("body_clear_shapes", "body"), &PhysicsServer3D::body_clear_shapes);

	ClassDB::bind_method(D_METHOD("body_attach_object_instance_id", "body", "id"), &PhysicsServer3D::body_attach_object_instance_id);
	ClassDB::bind_method(D_METHOD("body_get_object_instance_id", "body"), &PhysicsServer3D::body_get_object_instance_id);

	ClassDB::bind_method(D_METHOD("body_set_enable_continuous_collision_detection", "body", "enable"), &PhysicsServer3D::body_set_enable_continuous_collision_detection);
	ClassDB::bind_method(D_METHOD("body_is_continuous_collision_detection_enabled", "body"), &PhysicsServer3D::body_is_continuous_collision_detection_enabled);

	ClassDB::bind_method(D_METHOD("body_set_param", "body", "param", "value"), &PhysicsServer3D::body_set_param);
	ClassDB::bind_method(D_METHOD("body_get_param", "body", "param"), &PhysicsServer3D::body_get_param);

	ClassDB::bind_method(D_METHOD("body_reset_mass_properties", "body"), &PhysicsServer3D::body_reset_mass_properties);

	ClassDB::bind_method(D_METHOD("body_set_state", "body", "state", "value"), &PhysicsServer3D::body_set_state);
	ClassDB::bind_method(D_METHOD("body_get_state", "body", "state"), &PhysicsServer3D::body_get_state);

	ClassDB::bind_method(D_METHOD("body_apply_central_impulse", "body", "impulse"), &PhysicsServer3D::body_apply_central_impulse);
	ClassDB::bind_method(D_METHOD("body_apply_impulse", "body", "impulse", "position"), &PhysicsServer3D::body_apply_impulse, Vector3());
	ClassDB::bind_method(D_METHOD("body_apply_torque_impulse", "body", "impulse"), &PhysicsServer3D::body_apply_torque_impulse);

	ClassDB::bind_method(D_METHOD("body_apply_central_force", "body", "force"), &PhysicsServer3D::body_apply_central_force);
	ClassDB::bind_method(D_METHOD("body_apply_force", "body", "force", "position"), &PhysicsServer3D::body_apply_force, Vector3());
	ClassDB::bind_method(D_METHOD("body_apply_torque", "body", "torque"), &PhysicsServer3D::body_apply_torque);

	ClassDB::bind_method(D_METHOD("body_add_constant_central_force", "body", "force"), &PhysicsServer3D::body_add_constant_central_force);
	ClassDB::bind_method(D_METHOD("body_add_constant_force", "body", "force", "position"), &PhysicsServer3D::body_add_constant_force, Vector3());
	ClassDB::bind_method(D_METHOD("body_add_constant_torque", "body", "torque"), &PhysicsServer3D::body_add_constant_torque);

	ClassDB::bind_method(D_METHOD("body_set_constant_force", "body", "force"), &PhysicsServer3D::body_set_constant_force);
	ClassDB::bind_method(D_METHOD("body_get_constant_force", "body"), &PhysicsServer3D::body_get_constant_force);

	ClassDB::bind_method(D_METHOD("body_set_constant_torque", "body", "torque"), &PhysicsServer3D::body_set_constant_torque);
	ClassDB::bind_method(D_METHOD("body_get_constant_torque", "body"), &PhysicsServer3D::body_get_constant_torque);

	ClassDB::bind_method(D_METHOD("body_set_axis_velocity", "body", "axis_velocity"), &PhysicsServer3D::body_set_axis_velocity);

	ClassDB::bind_method(D_METHOD("body_set_axis_lock", "body", "axis", "lock"), &PhysicsServer3D::body_set_axis_lock);
	ClassDB::bind_method(D_METHOD("body_is_axis_locked", "body", "axis"), &PhysicsServer3D::body_is_axis_locked);

	ClassDB::bind_method(D_METHOD("body_add_collision_exception", "body", "excepted_body"), &PhysicsServer3D::body_add_collision_exception);
	ClassDB::bind_method(D_METHOD("body_remove_collision_exception", "body", "excepted_body"), &PhysicsServer3D::body_remove_collision_exception);

	ClassDB::bind_method(D_METHOD("body_set_max_contacts_reported", "body", "amount"), &PhysicsServer3D::body_set_max_contacts_reported);
	ClassDB::bind_method(D_METHOD("body_get_max_contacts_reported", "body"), &PhysicsServer3D::body_get_max_contacts_reported);

	ClassDB::bind_method(D_METHOD("body_set_omit_force_integration", "body", "enable"), &PhysicsServer3D::body_set_omit_force_integration);
	ClassDB::bind_method(D_METHOD("body_is_omitting_force_integration", "body"), &PhysicsServer3D::body_is_omitting_force_integration);

	ClassDB::bind_method(D_METHOD("body_set_state_sync_callback", "body", "callable"), &PhysicsServer3D::body_set_state_sync_callback);

	ClassDB::bind_method(D_METHOD("body_set_force_integration_callback", "body", "callable", "userdata"), &PhysicsServer3D::body_set_force_integration_callback, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("body_set_ray_pickable", "body", "enable"), &PhysicsServer3D::body_set_ray_pickable);

	ClassDB::bind_method(D_METHOD("body_test_motion", "body", "parameters", "result"), &PhysicsServer3D::_body_test_motion, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("body_get_direct_state", "body"), &PhysicsServer3D::body_get_direct_state);

	/* SOFT BODY API */

	ClassDB::bind_method(D_METHOD("soft_body_create"), &PhysicsServer3D::soft_body_create);

	ClassDB::bind_method(D_METHOD("soft_body_update_rendering_server", "body", "rendering_server_handler"), &PhysicsServer3D::soft_body_update_rendering_server);

	ClassDB::bind_method(D_METHOD("soft_body_set_space", "body", "space"), &PhysicsServer3D::soft_body_set_space);
	ClassDB::bind_method(D_METHOD("soft_body_get_space", "body"), &PhysicsServer3D::soft_body_get_space);

	ClassDB::bind_method(D_METHOD("soft_body_set_mesh", "body", "mesh"), &PhysicsServer3D::soft_body_set_mesh);

	ClassDB::bind_method(D_METHOD("soft_body_get_bounds", "body"), &PhysicsServer3D::soft_body_get_bounds);

	ClassDB::bind_method(D_METHOD("soft_body_set_collision_layer", "body", "layer"), &PhysicsServer3D::soft_body_set_collision_layer);
	ClassDB::bind_method(D_METHOD("soft_body_get_collision_layer", "body"), &PhysicsServer3D::soft_body_get_collision_layer);

	ClassDB::bind_method(D_METHOD("soft_body_set_collision_mask", "body", "mask"), &PhysicsServer3D::soft_body_set_collision_mask);
	ClassDB::bind_method(D_METHOD("soft_body_get_collision_mask", "body"), &PhysicsServer3D::soft_body_get_collision_mask);

	ClassDB::bind_method(D_METHOD("soft_body_add_collision_exception", "body", "body_b"), &PhysicsServer3D::soft_body_add_collision_exception);
	ClassDB::bind_method(D_METHOD("soft_body_remove_collision_exception", "body", "body_b"), &PhysicsServer3D::soft_body_remove_collision_exception);

	ClassDB::bind_method(D_METHOD("soft_body_set_state", "body", "state", "variant"), &PhysicsServer3D::soft_body_set_state);
	ClassDB::bind_method(D_METHOD("soft_body_get_state", "body", "state"), &PhysicsServer3D::soft_body_get_state);

	ClassDB::bind_method(D_METHOD("soft_body_set_transform", "body", "transform"), &PhysicsServer3D::soft_body_set_transform);

	ClassDB::bind_method(D_METHOD("soft_body_set_ray_pickable", "body", "enable"), &PhysicsServer3D::soft_body_set_ray_pickable);

	ClassDB::bind_method(D_METHOD("soft_body_set_simulation_precision", "body", "simulation_precision"), &PhysicsServer3D::soft_body_set_simulation_precision);
	ClassDB::bind_method(D_METHOD("soft_body_get_simulation_precision", "body"), &PhysicsServer3D::soft_body_get_simulation_precision);

	ClassDB::bind_method(D_METHOD("soft_body_set_total_mass", "body", "total_mass"), &PhysicsServer3D::soft_body_set_total_mass);
	ClassDB::bind_method(D_METHOD("soft_body_get_total_mass", "body"), &PhysicsServer3D::soft_body_get_total_mass);

	ClassDB::bind_method(D_METHOD("soft_body_set_linear_stiffness", "body", "stiffness"), &PhysicsServer3D::soft_body_set_linear_stiffness);
	ClassDB::bind_method(D_METHOD("soft_body_get_linear_stiffness", "body"), &PhysicsServer3D::soft_body_get_linear_stiffness);

	ClassDB::bind_method(D_METHOD("soft_body_set_pressure_coefficient", "body", "pressure_coefficient"), &PhysicsServer3D::soft_body_set_pressure_coefficient);
	ClassDB::bind_method(D_METHOD("soft_body_get_pressure_coefficient", "body"), &PhysicsServer3D::soft_body_get_pressure_coefficient);

	ClassDB::bind_method(D_METHOD("soft_body_set_damping_coefficient", "body", "damping_coefficient"), &PhysicsServer3D::soft_body_set_damping_coefficient);
	ClassDB::bind_method(D_METHOD("soft_body_get_damping_coefficient", "body"), &PhysicsServer3D::soft_body_get_damping_coefficient);

	ClassDB::bind_method(D_METHOD("soft_body_set_drag_coefficient", "body", "drag_coefficient"), &PhysicsServer3D::soft_body_set_drag_coefficient);
	ClassDB::bind_method(D_METHOD("soft_body_get_drag_coefficient", "body"), &PhysicsServer3D::soft_body_get_drag_coefficient);

	ClassDB::bind_method(D_METHOD("soft_body_move_point", "body", "point_index", "global_position"), &PhysicsServer3D::soft_body_move_point);
	ClassDB::bind_method(D_METHOD("soft_body_get_point_global_position", "body", "point_index"), &PhysicsServer3D::soft_body_get_point_global_position);

	ClassDB::bind_method(D_METHOD("soft_body_remove_all_pinned_points", "body"), &PhysicsServer3D::soft_body_remove_all_pinned_points);

	ClassDB::bind_method(D_METHOD("soft_body_pin_point", "body", "point_index", "pin"), &PhysicsServer3D::soft_body_pin_point);

	ClassDB::bind_method(D_METHOD("soft_body_is_point_pinned", "body", "point_index"), &PhysicsServer3D::soft_body_is_point_pinned);

	/* JOINT API */

	ClassDB::bind_method(D_METHOD("joint_create"), &PhysicsServer3D::joint_create);
	ClassDB::bind_method(D_METHOD("joint_clear", "joint"), &PhysicsServer3D::joint_clear);

	BIND_ENUM_CONSTANT(JOINT_TYPE_PIN);
	BIND_ENUM_CONSTANT(JOINT_TYPE_HINGE);
	BIND_ENUM_CONSTANT(JOINT_TYPE_SLIDER);
	BIND_ENUM_CONSTANT(JOINT_TYPE_CONE_TWIST);
	BIND_ENUM_CONSTANT(JOINT_TYPE_6DOF);
	BIND_ENUM_CONSTANT(JOINT_TYPE_MAX);

	ClassDB::bind_method(D_METHOD("joint_make_pin", "joint", "body_A", "local_A", "body_B", "local_B"), &PhysicsServer3D::joint_make_pin);
	ClassDB::bind_method(D_METHOD("pin_joint_set_param", "joint", "param", "value"), &PhysicsServer3D::pin_joint_set_param);
	ClassDB::bind_method(D_METHOD("pin_joint_get_param", "joint", "param"), &PhysicsServer3D::pin_joint_get_param);

	ClassDB::bind_method(D_METHOD("pin_joint_set_local_a", "joint", "local_A"), &PhysicsServer3D::pin_joint_set_local_a);
	ClassDB::bind_method(D_METHOD("pin_joint_get_local_a", "joint"), &PhysicsServer3D::pin_joint_get_local_a);

	ClassDB::bind_method(D_METHOD("pin_joint_set_local_b", "joint", "local_B"), &PhysicsServer3D::pin_joint_set_local_b);
	ClassDB::bind_method(D_METHOD("pin_joint_get_local_b", "joint"), &PhysicsServer3D::pin_joint_get_local_b);

	BIND_ENUM_CONSTANT(PIN_JOINT_BIAS);
	BIND_ENUM_CONSTANT(PIN_JOINT_DAMPING);
	BIND_ENUM_CONSTANT(PIN_JOINT_IMPULSE_CLAMP);

	BIND_ENUM_CONSTANT(HINGE_JOINT_BIAS);
	BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_BIAS);
	BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(HINGE_JOINT_LIMIT_RELAXATION);
	BIND_ENUM_CONSTANT(HINGE_JOINT_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(HINGE_JOINT_MOTOR_MAX_IMPULSE);

	BIND_ENUM_CONSTANT(HINGE_JOINT_FLAG_USE_LIMIT);
	BIND_ENUM_CONSTANT(HINGE_JOINT_FLAG_ENABLE_MOTOR);

	ClassDB::bind_method(D_METHOD("joint_make_hinge", "joint", "body_A", "hinge_A", "body_B", "hinge_B"), &PhysicsServer3D::joint_make_hinge);

	ClassDB::bind_method(D_METHOD("hinge_joint_set_param", "joint", "param", "value"), &PhysicsServer3D::hinge_joint_set_param);
	ClassDB::bind_method(D_METHOD("hinge_joint_get_param", "joint", "param"), &PhysicsServer3D::hinge_joint_get_param);

	ClassDB::bind_method(D_METHOD("hinge_joint_set_flag", "joint", "flag", "enabled"), &PhysicsServer3D::hinge_joint_set_flag);
	ClassDB::bind_method(D_METHOD("hinge_joint_get_flag", "joint", "flag"), &PhysicsServer3D::hinge_joint_get_flag);

	ClassDB::bind_method(D_METHOD("joint_make_slider", "joint", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer3D::joint_make_slider);

	ClassDB::bind_method(D_METHOD("slider_joint_set_param", "joint", "param", "value"), &PhysicsServer3D::slider_joint_set_param);
	ClassDB::bind_method(D_METHOD("slider_joint_get_param", "joint", "param"), &PhysicsServer3D::slider_joint_get_param);

	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING);
	BIND_ENUM_CONSTANT(SLIDER_JOINT_MAX);

	ClassDB::bind_method(D_METHOD("joint_make_cone_twist", "joint", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer3D::joint_make_cone_twist);

	ClassDB::bind_method(D_METHOD("cone_twist_joint_set_param", "joint", "param", "value"), &PhysicsServer3D::cone_twist_joint_set_param);
	ClassDB::bind_method(D_METHOD("cone_twist_joint_get_param", "joint", "param"), &PhysicsServer3D::cone_twist_joint_get_param);

	BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_SWING_SPAN);
	BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_TWIST_SPAN);
	BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_BIAS);
	BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_SOFTNESS);
	BIND_ENUM_CONSTANT(CONE_TWIST_JOINT_RELAXATION);

	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_RESTITUTION);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_DAMPING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_DAMPING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_RESTITUTION);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_ERP);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_MAX);

	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_MOTOR);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR);
	BIND_ENUM_CONSTANT(G6DOF_JOINT_FLAG_MAX);

	ClassDB::bind_method(D_METHOD("joint_get_type", "joint"), &PhysicsServer3D::joint_get_type);

	ClassDB::bind_method(D_METHOD("joint_set_solver_priority", "joint", "priority"), &PhysicsServer3D::joint_set_solver_priority);
	ClassDB::bind_method(D_METHOD("joint_get_solver_priority", "joint"), &PhysicsServer3D::joint_get_solver_priority);

	ClassDB::bind_method(D_METHOD("joint_disable_collisions_between_bodies", "joint", "disable"), &PhysicsServer3D::joint_disable_collisions_between_bodies);
	ClassDB::bind_method(D_METHOD("joint_is_disabled_collisions_between_bodies", "joint"), &PhysicsServer3D::joint_is_disabled_collisions_between_bodies);

	ClassDB::bind_method(D_METHOD("joint_make_generic_6dof", "joint", "body_A", "local_ref_A", "body_B", "local_ref_B"), &PhysicsServer3D::joint_make_generic_6dof);

	ClassDB::bind_method(D_METHOD("generic_6dof_joint_set_param", "joint", "axis", "param", "value"), &PhysicsServer3D::generic_6dof_joint_set_param);
	ClassDB::bind_method(D_METHOD("generic_6dof_joint_get_param", "joint", "axis", "param"), &PhysicsServer3D::generic_6dof_joint_get_param);

	ClassDB::bind_method(D_METHOD("generic_6dof_joint_set_flag", "joint", "axis", "flag", "enable"), &PhysicsServer3D::generic_6dof_joint_set_flag);
	ClassDB::bind_method(D_METHOD("generic_6dof_joint_get_flag", "joint", "axis", "flag"), &PhysicsServer3D::generic_6dof_joint_get_flag);

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &PhysicsServer3D::free);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &PhysicsServer3D::set_active);

	ClassDB::bind_method(D_METHOD("get_process_info", "process_info"), &PhysicsServer3D::get_process_info);

	BIND_ENUM_CONSTANT(SHAPE_WORLD_BOUNDARY);
	BIND_ENUM_CONSTANT(SHAPE_SEPARATION_RAY);
	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(SHAPE_BOX);
	BIND_ENUM_CONSTANT(SHAPE_CAPSULE);
	BIND_ENUM_CONSTANT(SHAPE_CYLINDER);
	BIND_ENUM_CONSTANT(SHAPE_CONVEX_POLYGON);
	BIND_ENUM_CONSTANT(SHAPE_CONCAVE_POLYGON);
	BIND_ENUM_CONSTANT(SHAPE_HEIGHTMAP);
	BIND_ENUM_CONSTANT(SHAPE_SOFT_BODY);
	BIND_ENUM_CONSTANT(SHAPE_CUSTOM);

	BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_OVERRIDE_MODE);
	BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY);
	BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_VECTOR);
	BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_IS_POINT);
	BIND_ENUM_CONSTANT(AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE);
	BIND_ENUM_CONSTANT(AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE);
	BIND_ENUM_CONSTANT(AREA_PARAM_LINEAR_DAMP);
	BIND_ENUM_CONSTANT(AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE);
	BIND_ENUM_CONSTANT(AREA_PARAM_ANGULAR_DAMP);
	BIND_ENUM_CONSTANT(AREA_PARAM_PRIORITY);
	BIND_ENUM_CONSTANT(AREA_PARAM_WIND_FORCE_MAGNITUDE);
	BIND_ENUM_CONSTANT(AREA_PARAM_WIND_SOURCE);
	BIND_ENUM_CONSTANT(AREA_PARAM_WIND_DIRECTION);
	BIND_ENUM_CONSTANT(AREA_PARAM_WIND_ATTENUATION_FACTOR);

	BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_DISABLED);
	BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE);
	BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_COMBINE_REPLACE);
	BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE);
	BIND_ENUM_CONSTANT(AREA_SPACE_OVERRIDE_REPLACE_COMBINE);

	BIND_ENUM_CONSTANT(BODY_MODE_STATIC);
	BIND_ENUM_CONSTANT(BODY_MODE_KINEMATIC);
	BIND_ENUM_CONSTANT(BODY_MODE_RIGID);
	BIND_ENUM_CONSTANT(BODY_MODE_RIGID_LINEAR);

	BIND_ENUM_CONSTANT(BODY_PARAM_BOUNCE);
	BIND_ENUM_CONSTANT(BODY_PARAM_FRICTION);
	BIND_ENUM_CONSTANT(BODY_PARAM_MASS);
	BIND_ENUM_CONSTANT(BODY_PARAM_INERTIA);
	BIND_ENUM_CONSTANT(BODY_PARAM_CENTER_OF_MASS);
	BIND_ENUM_CONSTANT(BODY_PARAM_GRAVITY_SCALE);
	BIND_ENUM_CONSTANT(BODY_PARAM_LINEAR_DAMP_MODE);
	BIND_ENUM_CONSTANT(BODY_PARAM_ANGULAR_DAMP_MODE);
	BIND_ENUM_CONSTANT(BODY_PARAM_LINEAR_DAMP);
	BIND_ENUM_CONSTANT(BODY_PARAM_ANGULAR_DAMP);
	BIND_ENUM_CONSTANT(BODY_PARAM_MAX);

	BIND_ENUM_CONSTANT(BODY_DAMP_MODE_COMBINE);
	BIND_ENUM_CONSTANT(BODY_DAMP_MODE_REPLACE);

	BIND_ENUM_CONSTANT(BODY_STATE_TRANSFORM);
	BIND_ENUM_CONSTANT(BODY_STATE_LINEAR_VELOCITY);
	BIND_ENUM_CONSTANT(BODY_STATE_ANGULAR_VELOCITY);
	BIND_ENUM_CONSTANT(BODY_STATE_SLEEPING);
	BIND_ENUM_CONSTANT(BODY_STATE_CAN_SLEEP);

	BIND_ENUM_CONSTANT(AREA_BODY_ADDED);
	BIND_ENUM_CONSTANT(AREA_BODY_REMOVED);

	BIND_ENUM_CONSTANT(INFO_ACTIVE_OBJECTS);
	BIND_ENUM_CONSTANT(INFO_COLLISION_PAIRS);
	BIND_ENUM_CONSTANT(INFO_ISLAND_COUNT);

	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_RECYCLE_RADIUS);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_SEPARATION);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_DEFAULT_BIAS);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_TIME_TO_SLEEP);
	BIND_ENUM_CONSTANT(SPACE_PARAM_SOLVER_ITERATIONS);

	BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_X);
	BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_Y);
	BIND_ENUM_CONSTANT(BODY_AXIS_LINEAR_Z);
	BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_X);
	BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_Y);
	BIND_ENUM_CONSTANT(BODY_AXIS_ANGULAR_Z);

#endif
}

PhysicsServer3D::PhysicsServer3D() {
	singleton = this;

	// World3D physics space
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "physics/3d/default_gravity", PROPERTY_HINT_RANGE, U"-32,32,0.001,or_less,or_greater,suffix:m/s\u00B2"), 9.8);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::VECTOR3, "physics/3d/default_gravity_vector", PROPERTY_HINT_RANGE, "-10,10,0.001,or_less,or_greater"), Vector3(0, -1, 0));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/default_linear_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), 0.1);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/default_angular_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), 0.1);

	// PhysicsServer3D
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/sleep_threshold_linear", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), 0.1);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/sleep_threshold_angular", PROPERTY_HINT_RANGE, "0,90,0.1,radians_as_degrees"), Math::deg_to_rad(8.0));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/time_before_sleep", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), 0.5);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/3d/solver/solver_iterations", PROPERTY_HINT_RANGE, "1,32,1,or_greater"), 16);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/solver/contact_recycle_radius", PROPERTY_HINT_RANGE, "0,0.1,0.001,or_greater"), 0.01);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/solver/contact_max_separation", PROPERTY_HINT_RANGE, "0,0.1,0.001,or_greater"), 0.05);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/solver/contact_max_allowed_penetration", PROPERTY_HINT_RANGE, "0.001,0.1,0.001,or_greater"), 0.01);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/3d/solver/default_contact_bias", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.8);
}

PhysicsServer3D::~PhysicsServer3D() {
	singleton = nullptr;
}

PhysicsServer3DManager *PhysicsServer3DManager::singleton = nullptr;
const String PhysicsServer3DManager::setting_property_name(PNAME("physics/3d/physics_engine"));

void PhysicsServer3DManager::on_servers_changed() {
	String physics_servers2("DEFAULT");
	for (int i = get_servers_count() - 1; 0 <= i; --i) {
		physics_servers2 += "," + get_server_name(i);
	}
	ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, setting_property_name, PROPERTY_HINT_ENUM, physics_servers2));
	ProjectSettings::get_singleton()->set_restart_if_changed(setting_property_name, true);
	ProjectSettings::get_singleton()->set_as_basic(setting_property_name, true);
}

void PhysicsServer3DManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_server", "name", "create_callback"), &PhysicsServer3DManager::register_server);
	ClassDB::bind_method(D_METHOD("set_default_server", "name", "priority"), &PhysicsServer3DManager::set_default_server);
}

PhysicsServer3DManager *PhysicsServer3DManager::get_singleton() {
	return singleton;
}

void PhysicsServer3DManager::register_server(const String &p_name, const Callable &p_create_callback) {
	//ERR_FAIL_COND(!p_create_callback.is_valid());
	ERR_FAIL_COND(find_server_id(p_name) != -1);
	physics_servers.push_back(ClassInfo(p_name, p_create_callback));
	on_servers_changed();
}

void PhysicsServer3DManager::set_default_server(const String &p_name, int p_priority) {
	const int id = find_server_id(p_name);
	ERR_FAIL_COND(id == -1); // Not found
	if (default_server_priority < p_priority) {
		default_server_id = id;
		default_server_priority = p_priority;
	}
}

int PhysicsServer3DManager::find_server_id(const String &p_name) {
	for (int i = physics_servers.size() - 1; 0 <= i; --i) {
		if (p_name == physics_servers[i].name) {
			return i;
		}
	}
	return -1;
}

int PhysicsServer3DManager::get_servers_count() {
	return physics_servers.size();
}

String PhysicsServer3DManager::get_server_name(int p_id) {
	ERR_FAIL_INDEX_V(p_id, get_servers_count(), "");
	return physics_servers[p_id].name;
}

PhysicsServer3D *PhysicsServer3DManager::new_default_server() {
	if (default_server_id == -1) {
		return nullptr;
	}
	Variant ret;
	Callable::CallError ce;
	physics_servers[default_server_id].create_callback.callp(nullptr, 0, ret, ce);
	ERR_FAIL_COND_V(ce.error != Callable::CallError::CALL_OK, nullptr);
	return Object::cast_to<PhysicsServer3D>(ret.get_validated_object());
}

PhysicsServer3D *PhysicsServer3DManager::new_server(const String &p_name) {
	int id = find_server_id(p_name);
	if (id == -1) {
		return nullptr;
	} else {
		Variant ret;
		Callable::CallError ce;
		physics_servers[id].create_callback.callp(nullptr, 0, ret, ce);
		ERR_FAIL_COND_V(ce.error != Callable::CallError::CALL_OK, nullptr);
		return Object::cast_to<PhysicsServer3D>(ret.get_validated_object());
	}
}

PhysicsServer3DManager::PhysicsServer3DManager() {
	singleton = this;
}

PhysicsServer3DManager::~PhysicsServer3DManager() {
	singleton = nullptr;
}

#endif // _3D_DISABLED
