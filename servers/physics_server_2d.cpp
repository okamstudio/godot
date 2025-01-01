/**************************************************************************/
/*  physics_server_2d.cpp                                                 */
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

#include "physics_server_2d.h"

#include "core/config/project_settings.h"
#include "core/variant/typed_array.h"

PhysicsServer2D *PhysicsServer2D::singleton = nullptr;

void PhysicsDirectBodyState2D::integrate_forces() {
	real_t step = get_step();
	Vector2 lv = get_linear_velocity();
	lv += get_total_gravity() * step;

	real_t av = get_angular_velocity();

	real_t damp = 1.0 - step * get_total_linear_damp();

	if (damp < 0) { // reached zero in the given time
		damp = 0;
	}

	lv *= damp;

	damp = 1.0 - step * get_total_angular_damp();

	if (damp < 0) { // reached zero in the given time
		damp = 0;
	}

	av *= damp;

	set_linear_velocity(lv);
	set_angular_velocity(av);
}

Object *PhysicsDirectBodyState2D::get_contact_collider_object(int p_contact_idx) const {
	ObjectID objid = get_contact_collider_id(p_contact_idx);
	Object *obj = ObjectDB::get_instance(objid);
	return obj;
}

PhysicsServer2D *PhysicsServer2D::get_singleton() {
	return singleton;
}

void PhysicsDirectBodyState2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_total_gravity"), &PhysicsDirectBodyState2D::get_total_gravity);
	ClassDB::bind_method(D_METHOD("get_total_linear_damp"), &PhysicsDirectBodyState2D::get_total_linear_damp);
	ClassDB::bind_method(D_METHOD("get_total_angular_damp"), &PhysicsDirectBodyState2D::get_total_angular_damp);

	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &PhysicsDirectBodyState2D::get_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_local"), &PhysicsDirectBodyState2D::get_center_of_mass_local);
	ClassDB::bind_method(D_METHOD("get_inverse_mass"), &PhysicsDirectBodyState2D::get_inverse_mass);
	ClassDB::bind_method(D_METHOD("get_inverse_inertia"), &PhysicsDirectBodyState2D::get_inverse_inertia);

	ClassDB::bind_method(D_METHOD("set_linear_velocity", "velocity"), &PhysicsDirectBodyState2D::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysicsDirectBodyState2D::get_linear_velocity);

	ClassDB::bind_method(D_METHOD("set_angular_velocity", "velocity"), &PhysicsDirectBodyState2D::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysicsDirectBodyState2D::get_angular_velocity);

	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsDirectBodyState2D::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsDirectBodyState2D::get_transform);

	ClassDB::bind_method(D_METHOD("get_velocity_at_local_position", "local_position"), &PhysicsDirectBodyState2D::get_velocity_at_local_position);

	ClassDB::bind_method(D_METHOD("apply_central_impulse", "impulse"), &PhysicsDirectBodyState2D::apply_central_impulse);
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &PhysicsDirectBodyState2D::apply_torque_impulse);
	ClassDB::bind_method(D_METHOD("apply_impulse", "impulse", "position"), &PhysicsDirectBodyState2D::apply_impulse, Vector2());

	ClassDB::bind_method(D_METHOD("apply_central_force", "force"), &PhysicsDirectBodyState2D::apply_central_force, Vector2());
	ClassDB::bind_method(D_METHOD("apply_force", "force", "position"), &PhysicsDirectBodyState2D::apply_force, Vector2());
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &PhysicsDirectBodyState2D::apply_torque);

	ClassDB::bind_method(D_METHOD("add_constant_central_force", "force"), &PhysicsDirectBodyState2D::add_constant_central_force, Vector2());
	ClassDB::bind_method(D_METHOD("add_constant_force", "force", "position"), &PhysicsDirectBodyState2D::add_constant_force, Vector2());
	ClassDB::bind_method(D_METHOD("add_constant_torque", "torque"), &PhysicsDirectBodyState2D::add_constant_torque);

	ClassDB::bind_method(D_METHOD("set_constant_force", "force"), &PhysicsDirectBodyState2D::set_constant_force);
	ClassDB::bind_method(D_METHOD("get_constant_force"), &PhysicsDirectBodyState2D::get_constant_force);

	ClassDB::bind_method(D_METHOD("set_constant_torque", "torque"), &PhysicsDirectBodyState2D::set_constant_torque);
	ClassDB::bind_method(D_METHOD("get_constant_torque"), &PhysicsDirectBodyState2D::get_constant_torque);

	ClassDB::bind_method(D_METHOD("set_sleep_state", "enabled"), &PhysicsDirectBodyState2D::set_sleep_state);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &PhysicsDirectBodyState2D::is_sleeping);

	ClassDB::bind_method(D_METHOD("get_contact_count"), &PhysicsDirectBodyState2D::get_contact_count);

	ClassDB::bind_method(D_METHOD("get_contact_local_position", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_local_position);
	ClassDB::bind_method(D_METHOD("get_contact_local_normal", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_local_normal);
	ClassDB::bind_method(D_METHOD("get_contact_local_shape", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_local_shape);
	ClassDB::bind_method(D_METHOD("get_contact_local_velocity_at_position", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_local_velocity_at_position);
	ClassDB::bind_method(D_METHOD("get_contact_collider", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider);
	ClassDB::bind_method(D_METHOD("get_contact_collider_position", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider_position);
	ClassDB::bind_method(D_METHOD("get_contact_collider_id", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider_id);
	ClassDB::bind_method(D_METHOD("get_contact_collider_object", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider_object);
	ClassDB::bind_method(D_METHOD("get_contact_collider_shape", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider_shape);
	ClassDB::bind_method(D_METHOD("get_contact_collider_velocity_at_position", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_collider_velocity_at_position);
	ClassDB::bind_method(D_METHOD("get_contact_impulse", "contact_idx"), &PhysicsDirectBodyState2D::get_contact_impulse);
	ClassDB::bind_method(D_METHOD("get_step"), &PhysicsDirectBodyState2D::get_step);
	ClassDB::bind_method(D_METHOD("integrate_forces"), &PhysicsDirectBodyState2D::integrate_forces);
	ClassDB::bind_method(D_METHOD("get_space_state"), &PhysicsDirectBodyState2D::get_space_state);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step"), "", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inverse_mass"), "", "get_inverse_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inverse_inertia"), "", "get_inverse_inertia");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_angular_damp"), "", "get_total_angular_damp");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "total_linear_damp"), "", "get_total_linear_damp");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "total_gravity"), "", "get_total_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "center_of_mass"), "", "get_center_of_mass");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "center_of_mass_local"), "", "get_center_of_mass_local");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sleeping"), "set_sleep_state", "is_sleeping");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
}

PhysicsDirectBodyState2D::PhysicsDirectBodyState2D() {}

///////////////////////////////////////////////////////

Ref<PhysicsRayQueryParameters2D> PhysicsRayQueryParameters2D::create(Vector2 p_from, Vector2 p_to, uint32_t p_mask, const TypedArray<RID> &p_exclude) {
	Ref<PhysicsRayQueryParameters2D> params;
	params.instantiate();
	params->set_from(p_from);
	params->set_to(p_to);
	params->set_collision_mask(p_mask);
	params->set_exclude(p_exclude);
	return params;
}

void PhysicsRayQueryParameters2D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsRayQueryParameters2D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsRayQueryParameters2D::_bind_methods() {
	ClassDB::bind_static_method("PhysicsRayQueryParameters2D", D_METHOD("create", "from", "to", "collision_mask", "exclude"), &PhysicsRayQueryParameters2D::create, DEFVAL(UINT32_MAX), DEFVAL(TypedArray<RID>()));

	ClassDB::bind_method(D_METHOD("set_from", "from"), &PhysicsRayQueryParameters2D::set_from);
	ClassDB::bind_method(D_METHOD("get_from"), &PhysicsRayQueryParameters2D::get_from);

	ClassDB::bind_method(D_METHOD("set_to", "to"), &PhysicsRayQueryParameters2D::set_to);
	ClassDB::bind_method(D_METHOD("get_to"), &PhysicsRayQueryParameters2D::get_to);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsRayQueryParameters2D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsRayQueryParameters2D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsRayQueryParameters2D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsRayQueryParameters2D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsRayQueryParameters2D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsRayQueryParameters2D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsRayQueryParameters2D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsRayQueryParameters2D::is_collide_with_areas_enabled);

	ClassDB::bind_method(D_METHOD("set_hit_from_inside", "enable"), &PhysicsRayQueryParameters2D::set_hit_from_inside);
	ClassDB::bind_method(D_METHOD("is_hit_from_inside_enabled"), &PhysicsRayQueryParameters2D::is_hit_from_inside_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "from"), "set_from", "get_from");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "to"), "set_to", "get_to");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hit_from_inside"), "set_hit_from_inside", "is_hit_from_inside_enabled");
}

///////////////////////////////////////////////////////

void PhysicsPointQueryParameters2D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsPointQueryParameters2D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsPointQueryParameters2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_position", "position"), &PhysicsPointQueryParameters2D::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysicsPointQueryParameters2D::get_position);

	ClassDB::bind_method(D_METHOD("set_canvas_instance_id", "canvas_instance_id"), &PhysicsPointQueryParameters2D::set_canvas_instance_id);
	ClassDB::bind_method(D_METHOD("get_canvas_instance_id"), &PhysicsPointQueryParameters2D::get_canvas_instance_id);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsPointQueryParameters2D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsPointQueryParameters2D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsPointQueryParameters2D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsPointQueryParameters2D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsPointQueryParameters2D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsPointQueryParameters2D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsPointQueryParameters2D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsPointQueryParameters2D::is_collide_with_areas_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_instance_id", PROPERTY_HINT_OBJECT_ID), "set_canvas_instance_id", "get_canvas_instance_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

///////////////////////////////////////////////////////

void PhysicsShapeQueryParameters2D::set_shape(const Ref<Resource> &p_shape_ref) {
	ERR_FAIL_COND(p_shape_ref.is_null());
	shape_ref = p_shape_ref;
	parameters.shape_rid = p_shape_ref->get_rid();
}

void PhysicsShapeQueryParameters2D::set_shape_rid(const RID &p_shape) {
	if (parameters.shape_rid != p_shape) {
		shape_ref = Ref<Resource>();
		parameters.shape_rid = p_shape;
	}
}

void PhysicsShapeQueryParameters2D::set_exclude(const TypedArray<RID> &p_exclude) {
	parameters.exclude.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude.insert(p_exclude[i]);
	}
}

TypedArray<RID> PhysicsShapeQueryParameters2D::get_exclude() const {
	TypedArray<RID> ret;
	ret.resize(parameters.exclude.size());
	int idx = 0;
	for (const RID &E : parameters.exclude) {
		ret[idx++] = E;
	}
	return ret;
}

void PhysicsShapeQueryParameters2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysicsShapeQueryParameters2D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysicsShapeQueryParameters2D::get_shape);

	ClassDB::bind_method(D_METHOD("set_shape_rid", "shape"), &PhysicsShapeQueryParameters2D::set_shape_rid);
	ClassDB::bind_method(D_METHOD("get_shape_rid"), &PhysicsShapeQueryParameters2D::get_shape_rid);

	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &PhysicsShapeQueryParameters2D::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &PhysicsShapeQueryParameters2D::get_transform);

	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &PhysicsShapeQueryParameters2D::set_motion);
	ClassDB::bind_method(D_METHOD("get_motion"), &PhysicsShapeQueryParameters2D::get_motion);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &PhysicsShapeQueryParameters2D::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &PhysicsShapeQueryParameters2D::get_margin);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &PhysicsShapeQueryParameters2D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysicsShapeQueryParameters2D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_exclude", "exclude"), &PhysicsShapeQueryParameters2D::set_exclude);
	ClassDB::bind_method(D_METHOD("get_exclude"), &PhysicsShapeQueryParameters2D::get_exclude);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &PhysicsShapeQueryParameters2D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &PhysicsShapeQueryParameters2D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &PhysicsShapeQueryParameters2D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &PhysicsShapeQueryParameters2D::is_collide_with_areas_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude", "get_exclude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0,100,0.01"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "motion"), "set_motion", "get_motion");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape2D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::RID, "shape_rid"), "set_shape_rid", "get_shape_rid");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies"), "set_collide_with_bodies", "is_collide_with_bodies_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas"), "set_collide_with_areas", "is_collide_with_areas_enabled");
}

///////////////////////////////////////////////////////

Dictionary PhysicsDirectSpaceState2D::_intersect_ray(const Ref<PhysicsRayQueryParameters2D> &p_ray_query) {
	ERR_FAIL_COND_V(p_ray_query.is_null(), Dictionary());

	RayResult result;
	bool res = intersect_ray(p_ray_query->get_parameters(), result);

	if (!res) {
		return Dictionary();
	}

	Dictionary d;
	d["position"] = result.position;
	d["normal"] = result.normal;
	d["collider_id"] = result.collider_id;
	d["collider"] = result.collider;
	d["shape"] = result.shape;
	d["rid"] = result.rid;

	return d;
}

TypedArray<Dictionary> PhysicsDirectSpaceState2D::_intersect_point(const Ref<PhysicsPointQueryParameters2D> &p_point_query, int p_max_results) {
	ERR_FAIL_COND_V(p_point_query.is_null(), Array());

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

TypedArray<Dictionary> PhysicsDirectSpaceState2D::_intersect_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results) {
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

Vector<real_t> PhysicsDirectSpaceState2D::_cast_motion(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), Vector<real_t>());

	real_t closest_safe, closest_unsafe;
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

TypedArray<Vector2> PhysicsDirectSpaceState2D::_collide_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results) {
	ERR_FAIL_COND_V(p_shape_query.is_null(), TypedArray<Vector2>());

	Vector<Vector2> ret;
	ret.resize(p_max_results * 2);
	int rc = 0;
	bool res = collide_shape(p_shape_query->get_parameters(), ret.ptrw(), p_max_results, rc);
	if (!res) {
		return TypedArray<Vector2>();
	}
	TypedArray<Vector2> r;
	r.resize(rc * 2);
	for (int i = 0; i < rc * 2; i++) {
		r[i] = ret[i];
	}
	return r;
}

Dictionary PhysicsDirectSpaceState2D::_get_rest_info(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query) {
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

PhysicsDirectSpaceState2D::PhysicsDirectSpaceState2D() {
}

void PhysicsDirectSpaceState2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("intersect_point", "parameters", "max_results"), &PhysicsDirectSpaceState2D::_intersect_point, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("intersect_ray", "parameters"), &PhysicsDirectSpaceState2D::_intersect_ray);
	ClassDB::bind_method(D_METHOD("intersect_shape", "parameters", "max_results"), &PhysicsDirectSpaceState2D::_intersect_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("cast_motion", "parameters"), &PhysicsDirectSpaceState2D::_cast_motion);
	ClassDB::bind_method(D_METHOD("collide_shape", "parameters", "max_results"), &PhysicsDirectSpaceState2D::_collide_shape, DEFVAL(32));
	ClassDB::bind_method(D_METHOD("get_rest_info", "parameters"), &PhysicsDirectSpaceState2D::_get_rest_info);
}

///////////////////////////////

TypedArray<RID> PhysicsTestMotionParameters2D::get_exclude_bodies() const {
	TypedArray<RID> exclude;
	exclude.resize(parameters.exclude_bodies.size());

	int body_index = 0;
	for (RID body : parameters.exclude_bodies) {
		exclude[body_index++] = body;
	}

	return exclude;
}

void PhysicsTestMotionParameters2D::set_exclude_bodies(const TypedArray<RID> &p_exclude) {
	parameters.exclude_bodies.clear();
	for (int i = 0; i < p_exclude.size(); i++) {
		parameters.exclude_bodies.insert(p_exclude[i]);
	}
}

TypedArray<uint64_t> PhysicsTestMotionParameters2D::get_exclude_objects() const {
	TypedArray<uint64_t> exclude;
	exclude.resize(parameters.exclude_objects.size());

	int object_index = 0;
	for (ObjectID object_id : parameters.exclude_objects) {
		exclude[object_index++] = object_id;
	}

	return exclude;
}

void PhysicsTestMotionParameters2D::set_exclude_objects(const TypedArray<uint64_t> &p_exclude) {
	parameters.exclude_objects.clear();
	for (int i = 0; i < p_exclude.size(); ++i) {
		ObjectID object_id = p_exclude[i];
		ERR_CONTINUE(object_id.is_null());
		parameters.exclude_objects.insert(object_id);
	}
}

void PhysicsTestMotionParameters2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_from"), &PhysicsTestMotionParameters2D::get_from);
	ClassDB::bind_method(D_METHOD("set_from", "from"), &PhysicsTestMotionParameters2D::set_from);

	ClassDB::bind_method(D_METHOD("get_motion"), &PhysicsTestMotionParameters2D::get_motion);
	ClassDB::bind_method(D_METHOD("set_motion", "motion"), &PhysicsTestMotionParameters2D::set_motion);

	ClassDB::bind_method(D_METHOD("get_margin"), &PhysicsTestMotionParameters2D::get_margin);
	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &PhysicsTestMotionParameters2D::set_margin);

	ClassDB::bind_method(D_METHOD("is_collide_separation_ray_enabled"), &PhysicsTestMotionParameters2D::is_collide_separation_ray_enabled);
	ClassDB::bind_method(D_METHOD("set_collide_separation_ray_enabled", "enabled"), &PhysicsTestMotionParameters2D::set_collide_separation_ray_enabled);

	ClassDB::bind_method(D_METHOD("get_exclude_bodies"), &PhysicsTestMotionParameters2D::get_exclude_bodies);
	ClassDB::bind_method(D_METHOD("set_exclude_bodies", "exclude_list"), &PhysicsTestMotionParameters2D::set_exclude_bodies);

	ClassDB::bind_method(D_METHOD("get_exclude_objects"), &PhysicsTestMotionParameters2D::get_exclude_objects);
	ClassDB::bind_method(D_METHOD("set_exclude_objects", "exclude_list"), &PhysicsTestMotionParameters2D::set_exclude_objects);

	ClassDB::bind_method(D_METHOD("is_recovery_as_collision_enabled"), &PhysicsTestMotionParameters2D::is_recovery_as_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_recovery_as_collision_enabled", "enabled"), &PhysicsTestMotionParameters2D::set_recovery_as_collision_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "from"), "set_from", "get_from");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "motion"), "set_motion", "get_motion");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_separation_ray"), "set_collide_separation_ray_enabled", "is_collide_separation_ray_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude_bodies", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_exclude_bodies", "get_exclude_bodies");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "exclude_objects"), "set_exclude_objects", "get_exclude_objects");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "recovery_as_collision"), "set_recovery_as_collision_enabled", "is_recovery_as_collision_enabled");
}

///////////////////////////////

Vector2 PhysicsTestMotionResult2D::get_travel() const {
	return result.travel;
}

Vector2 PhysicsTestMotionResult2D::get_remainder() const {
	return result.remainder;
}

Vector2 PhysicsTestMotionResult2D::get_collision_point() const {
	return result.collision_point;
}

Vector2 PhysicsTestMotionResult2D::get_collision_normal() const {
	return result.collision_normal;
}

Vector2 PhysicsTestMotionResult2D::get_collider_velocity() const {
	return result.collider_velocity;
}

ObjectID PhysicsTestMotionResult2D::get_collider_id() const {
	return result.collider_id;
}

RID PhysicsTestMotionResult2D::get_collider_rid() const {
	return result.collider;
}

Object *PhysicsTestMotionResult2D::get_collider() const {
	return ObjectDB::get_instance(result.collider_id);
}

int PhysicsTestMotionResult2D::get_collider_shape() const {
	return result.collider_shape;
}

int PhysicsTestMotionResult2D::get_collision_local_shape() const {
	return result.collision_local_shape;
}

real_t PhysicsTestMotionResult2D::get_collision_depth() const {
	return result.collision_depth;
}

real_t PhysicsTestMotionResult2D::get_collision_safe_fraction() const {
	return result.collision_safe_fraction;
}

real_t PhysicsTestMotionResult2D::get_collision_unsafe_fraction() const {
	return result.collision_unsafe_fraction;
}

void PhysicsTestMotionResult2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_travel"), &PhysicsTestMotionResult2D::get_travel);
	ClassDB::bind_method(D_METHOD("get_remainder"), &PhysicsTestMotionResult2D::get_remainder);
	ClassDB::bind_method(D_METHOD("get_collision_point"), &PhysicsTestMotionResult2D::get_collision_point);
	ClassDB::bind_method(D_METHOD("get_collision_normal"), &PhysicsTestMotionResult2D::get_collision_normal);
	ClassDB::bind_method(D_METHOD("get_collider_velocity"), &PhysicsTestMotionResult2D::get_collider_velocity);
	ClassDB::bind_method(D_METHOD("get_collider_id"), &PhysicsTestMotionResult2D::get_collider_id);
	ClassDB::bind_method(D_METHOD("get_collider_rid"), &PhysicsTestMotionResult2D::get_collider_rid);
	ClassDB::bind_method(D_METHOD("get_collider"), &PhysicsTestMotionResult2D::get_collider);
	ClassDB::bind_method(D_METHOD("get_collider_shape"), &PhysicsTestMotionResult2D::get_collider_shape);
	ClassDB::bind_method(D_METHOD("get_collision_local_shape"), &PhysicsTestMotionResult2D::get_collision_local_shape);
	ClassDB::bind_method(D_METHOD("get_collision_depth"), &PhysicsTestMotionResult2D::get_collision_depth);
	ClassDB::bind_method(D_METHOD("get_collision_safe_fraction"), &PhysicsTestMotionResult2D::get_collision_safe_fraction);
	ClassDB::bind_method(D_METHOD("get_collision_unsafe_fraction"), &PhysicsTestMotionResult2D::get_collision_unsafe_fraction);
}

///////////////////////////////////////

bool PhysicsServer2D::_body_test_motion(RID p_body, const Ref<PhysicsTestMotionParameters2D> &p_parameters, const Ref<PhysicsTestMotionResult2D> &p_result) {
	ERR_FAIL_COND_V(p_parameters.is_null(), false);

	MotionResult *result_ptr = nullptr;
	if (p_result.is_valid()) {
		result_ptr = p_result->get_result_ptr();
	}

	return body_test_motion(p_body, p_parameters->get_parameters(), result_ptr);
}

void PhysicsServer2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("world_boundary_shape_create"), &PhysicsServer2D::world_boundary_shape_create);
	ClassDB::bind_method(D_METHOD("separation_ray_shape_create"), &PhysicsServer2D::separation_ray_shape_create);
	ClassDB::bind_method(D_METHOD("segment_shape_create"), &PhysicsServer2D::segment_shape_create);
	ClassDB::bind_method(D_METHOD("circle_shape_create"), &PhysicsServer2D::circle_shape_create);
	ClassDB::bind_method(D_METHOD("rectangle_shape_create"), &PhysicsServer2D::rectangle_shape_create);
	ClassDB::bind_method(D_METHOD("capsule_shape_create"), &PhysicsServer2D::capsule_shape_create);
	ClassDB::bind_method(D_METHOD("convex_polygon_shape_create"), &PhysicsServer2D::convex_polygon_shape_create);
	ClassDB::bind_method(D_METHOD("concave_polygon_shape_create"), &PhysicsServer2D::concave_polygon_shape_create);

	ClassDB::bind_method(D_METHOD("shape_set_data", "shape", "data"), &PhysicsServer2D::shape_set_data);

	ClassDB::bind_method(D_METHOD("shape_get_type", "shape"), &PhysicsServer2D::shape_get_type);
	ClassDB::bind_method(D_METHOD("shape_get_data", "shape"), &PhysicsServer2D::shape_get_data);

	ClassDB::bind_method(D_METHOD("space_create"), &PhysicsServer2D::space_create);
	ClassDB::bind_method(D_METHOD("space_set_active", "space", "active"), &PhysicsServer2D::space_set_active);
	ClassDB::bind_method(D_METHOD("space_is_active", "space"), &PhysicsServer2D::space_is_active);
	ClassDB::bind_method(D_METHOD("space_set_param", "space", "param", "value"), &PhysicsServer2D::space_set_param);
	ClassDB::bind_method(D_METHOD("space_get_param", "space", "param"), &PhysicsServer2D::space_get_param);
	ClassDB::bind_method(D_METHOD("space_get_direct_state", "space"), &PhysicsServer2D::space_get_direct_state);

	ClassDB::bind_method(D_METHOD("area_create"), &PhysicsServer2D::area_create);
	ClassDB::bind_method(D_METHOD("area_set_space", "area", "space"), &PhysicsServer2D::area_set_space);
	ClassDB::bind_method(D_METHOD("area_get_space", "area"), &PhysicsServer2D::area_get_space);

	ClassDB::bind_method(D_METHOD("area_add_shape", "area", "shape", "transform", "disabled"), &PhysicsServer2D::area_add_shape, DEFVAL(Transform2D()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("area_set_shape", "area", "shape_idx", "shape"), &PhysicsServer2D::area_set_shape);
	ClassDB::bind_method(D_METHOD("area_set_shape_transform", "area", "shape_idx", "transform"), &PhysicsServer2D::area_set_shape_transform);
	ClassDB::bind_method(D_METHOD("area_set_shape_disabled", "area", "shape_idx", "disabled"), &PhysicsServer2D::area_set_shape_disabled);

	ClassDB::bind_method(D_METHOD("area_get_shape_count", "area"), &PhysicsServer2D::area_get_shape_count);
	ClassDB::bind_method(D_METHOD("area_get_shape", "area", "shape_idx"), &PhysicsServer2D::area_get_shape);
	ClassDB::bind_method(D_METHOD("area_get_shape_transform", "area", "shape_idx"), &PhysicsServer2D::area_get_shape_transform);

	ClassDB::bind_method(D_METHOD("area_remove_shape", "area", "shape_idx"), &PhysicsServer2D::area_remove_shape);
	ClassDB::bind_method(D_METHOD("area_clear_shapes", "area"), &PhysicsServer2D::area_clear_shapes);

	ClassDB::bind_method(D_METHOD("area_set_collision_layer", "area", "layer"), &PhysicsServer2D::area_set_collision_layer);
	ClassDB::bind_method(D_METHOD("area_get_collision_layer", "area"), &PhysicsServer2D::area_get_collision_layer);

	ClassDB::bind_method(D_METHOD("area_set_collision_mask", "area", "mask"), &PhysicsServer2D::area_set_collision_mask);
	ClassDB::bind_method(D_METHOD("area_get_collision_mask", "area"), &PhysicsServer2D::area_get_collision_mask);

	ClassDB::bind_method(D_METHOD("area_set_param", "area", "param", "value"), &PhysicsServer2D::area_set_param);
	ClassDB::bind_method(D_METHOD("area_set_transform", "area", "transform"), &PhysicsServer2D::area_set_transform);

	ClassDB::bind_method(D_METHOD("area_get_param", "area", "param"), &PhysicsServer2D::area_get_param);
	ClassDB::bind_method(D_METHOD("area_get_transform", "area"), &PhysicsServer2D::area_get_transform);

	ClassDB::bind_method(D_METHOD("area_attach_object_instance_id", "area", "id"), &PhysicsServer2D::area_attach_object_instance_id);
	ClassDB::bind_method(D_METHOD("area_get_object_instance_id", "area"), &PhysicsServer2D::area_get_object_instance_id);

	ClassDB::bind_method(D_METHOD("area_attach_canvas_instance_id", "area", "id"), &PhysicsServer2D::area_attach_canvas_instance_id);
	ClassDB::bind_method(D_METHOD("area_get_canvas_instance_id", "area"), &PhysicsServer2D::area_get_canvas_instance_id);

	ClassDB::bind_method(D_METHOD("area_set_monitor_callback", "area", "callback"), &PhysicsServer2D::area_set_monitor_callback);
	ClassDB::bind_method(D_METHOD("area_set_area_monitor_callback", "area", "callback"), &PhysicsServer2D::area_set_area_monitor_callback);
	ClassDB::bind_method(D_METHOD("area_set_monitorable", "area", "monitorable"), &PhysicsServer2D::area_set_monitorable);

	ClassDB::bind_method(D_METHOD("body_create"), &PhysicsServer2D::body_create);

	ClassDB::bind_method(D_METHOD("body_set_space", "body", "space"), &PhysicsServer2D::body_set_space);
	ClassDB::bind_method(D_METHOD("body_get_space", "body"), &PhysicsServer2D::body_get_space);

	ClassDB::bind_method(D_METHOD("body_set_mode", "body", "mode"), &PhysicsServer2D::body_set_mode);
	ClassDB::bind_method(D_METHOD("body_get_mode", "body"), &PhysicsServer2D::body_get_mode);

	ClassDB::bind_method(D_METHOD("body_add_shape", "body", "shape", "transform", "disabled"), &PhysicsServer2D::body_add_shape, DEFVAL(Transform2D()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("body_set_shape", "body", "shape_idx", "shape"), &PhysicsServer2D::body_set_shape);
	ClassDB::bind_method(D_METHOD("body_set_shape_transform", "body", "shape_idx", "transform"), &PhysicsServer2D::body_set_shape_transform);

	ClassDB::bind_method(D_METHOD("body_get_shape_count", "body"), &PhysicsServer2D::body_get_shape_count);
	ClassDB::bind_method(D_METHOD("body_get_shape", "body", "shape_idx"), &PhysicsServer2D::body_get_shape);
	ClassDB::bind_method(D_METHOD("body_get_shape_transform", "body", "shape_idx"), &PhysicsServer2D::body_get_shape_transform);

	ClassDB::bind_method(D_METHOD("body_remove_shape", "body", "shape_idx"), &PhysicsServer2D::body_remove_shape);
	ClassDB::bind_method(D_METHOD("body_clear_shapes", "body"), &PhysicsServer2D::body_clear_shapes);

	ClassDB::bind_method(D_METHOD("body_set_shape_disabled", "body", "shape_idx", "disabled"), &PhysicsServer2D::body_set_shape_disabled);
	ClassDB::bind_method(D_METHOD("body_set_shape_as_one_way_collision", "body", "shape_idx", "enable", "margin"), &PhysicsServer2D::body_set_shape_as_one_way_collision);

	ClassDB::bind_method(D_METHOD("body_attach_object_instance_id", "body", "id"), &PhysicsServer2D::body_attach_object_instance_id);
	ClassDB::bind_method(D_METHOD("body_get_object_instance_id", "body"), &PhysicsServer2D::body_get_object_instance_id);

	ClassDB::bind_method(D_METHOD("body_attach_canvas_instance_id", "body", "id"), &PhysicsServer2D::body_attach_canvas_instance_id);
	ClassDB::bind_method(D_METHOD("body_get_canvas_instance_id", "body"), &PhysicsServer2D::body_get_canvas_instance_id);

	ClassDB::bind_method(D_METHOD("body_set_continuous_collision_detection_mode", "body", "mode"), &PhysicsServer2D::body_set_continuous_collision_detection_mode);
	ClassDB::bind_method(D_METHOD("body_get_continuous_collision_detection_mode", "body"), &PhysicsServer2D::body_get_continuous_collision_detection_mode);

	ClassDB::bind_method(D_METHOD("body_set_collision_layer", "body", "layer"), &PhysicsServer2D::body_set_collision_layer);
	ClassDB::bind_method(D_METHOD("body_get_collision_layer", "body"), &PhysicsServer2D::body_get_collision_layer);

	ClassDB::bind_method(D_METHOD("body_set_collision_mask", "body", "mask"), &PhysicsServer2D::body_set_collision_mask);
	ClassDB::bind_method(D_METHOD("body_get_collision_mask", "body"), &PhysicsServer2D::body_get_collision_mask);

	ClassDB::bind_method(D_METHOD("body_set_collision_priority", "body", "priority"), &PhysicsServer2D::body_set_collision_priority);
	ClassDB::bind_method(D_METHOD("body_get_collision_priority", "body"), &PhysicsServer2D::body_get_collision_priority);

	ClassDB::bind_method(D_METHOD("body_set_param", "body", "param", "value"), &PhysicsServer2D::body_set_param);
	ClassDB::bind_method(D_METHOD("body_get_param", "body", "param"), &PhysicsServer2D::body_get_param);

	ClassDB::bind_method(D_METHOD("body_reset_mass_properties", "body"), &PhysicsServer2D::body_reset_mass_properties);

	ClassDB::bind_method(D_METHOD("body_set_state", "body", "state", "value"), &PhysicsServer2D::body_set_state);
	ClassDB::bind_method(D_METHOD("body_get_state", "body", "state"), &PhysicsServer2D::body_get_state);

	ClassDB::bind_method(D_METHOD("body_apply_central_impulse", "body", "impulse"), &PhysicsServer2D::body_apply_central_impulse);
	ClassDB::bind_method(D_METHOD("body_apply_torque_impulse", "body", "impulse"), &PhysicsServer2D::body_apply_torque_impulse);
	ClassDB::bind_method(D_METHOD("body_apply_impulse", "body", "impulse", "position"), &PhysicsServer2D::body_apply_impulse, Vector2());

	ClassDB::bind_method(D_METHOD("body_apply_central_force", "body", "force"), &PhysicsServer2D::body_apply_central_force);
	ClassDB::bind_method(D_METHOD("body_apply_force", "body", "force", "position"), &PhysicsServer2D::body_apply_force, Vector2());
	ClassDB::bind_method(D_METHOD("body_apply_torque", "body", "torque"), &PhysicsServer2D::body_apply_torque);

	ClassDB::bind_method(D_METHOD("body_add_constant_central_force", "body", "force"), &PhysicsServer2D::body_add_constant_central_force);
	ClassDB::bind_method(D_METHOD("body_add_constant_force", "body", "force", "position"), &PhysicsServer2D::body_add_constant_force, Vector2());
	ClassDB::bind_method(D_METHOD("body_add_constant_torque", "body", "torque"), &PhysicsServer2D::body_add_constant_torque);

	ClassDB::bind_method(D_METHOD("body_set_constant_force", "body", "force"), &PhysicsServer2D::body_set_constant_force);
	ClassDB::bind_method(D_METHOD("body_get_constant_force", "body"), &PhysicsServer2D::body_get_constant_force);

	ClassDB::bind_method(D_METHOD("body_set_constant_torque", "body", "torque"), &PhysicsServer2D::body_set_constant_torque);
	ClassDB::bind_method(D_METHOD("body_get_constant_torque", "body"), &PhysicsServer2D::body_get_constant_torque);

	ClassDB::bind_method(D_METHOD("body_set_axis_velocity", "body", "axis_velocity"), &PhysicsServer2D::body_set_axis_velocity);

	ClassDB::bind_method(D_METHOD("body_add_collision_exception", "body", "excepted_body"), &PhysicsServer2D::body_add_collision_exception);
	ClassDB::bind_method(D_METHOD("body_remove_collision_exception", "body", "excepted_body"), &PhysicsServer2D::body_remove_collision_exception);

	ClassDB::bind_method(D_METHOD("body_set_max_contacts_reported", "body", "amount"), &PhysicsServer2D::body_set_max_contacts_reported);
	ClassDB::bind_method(D_METHOD("body_get_max_contacts_reported", "body"), &PhysicsServer2D::body_get_max_contacts_reported);

	ClassDB::bind_method(D_METHOD("body_set_omit_force_integration", "body", "enable"), &PhysicsServer2D::body_set_omit_force_integration);
	ClassDB::bind_method(D_METHOD("body_is_omitting_force_integration", "body"), &PhysicsServer2D::body_is_omitting_force_integration);

	ClassDB::bind_method(D_METHOD("body_set_state_sync_callback", "body", "callable"), &PhysicsServer2D::body_set_state_sync_callback);

	ClassDB::bind_method(D_METHOD("body_set_force_integration_callback", "body", "callable", "userdata"), &PhysicsServer2D::body_set_force_integration_callback, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("body_test_motion", "body", "parameters", "result"), &PhysicsServer2D::_body_test_motion, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("body_get_direct_state", "body"), &PhysicsServer2D::body_get_direct_state);

	/* JOINT API */

	ClassDB::bind_method(D_METHOD("joint_create"), &PhysicsServer2D::joint_create);

	ClassDB::bind_method(D_METHOD("joint_clear", "joint"), &PhysicsServer2D::joint_clear);

	ClassDB::bind_method(D_METHOD("joint_set_param", "joint", "param", "value"), &PhysicsServer2D::joint_set_param);
	ClassDB::bind_method(D_METHOD("joint_get_param", "joint", "param"), &PhysicsServer2D::joint_get_param);

	ClassDB::bind_method(D_METHOD("joint_disable_collisions_between_bodies", "joint", "disable"), &PhysicsServer2D::joint_disable_collisions_between_bodies);
	ClassDB::bind_method(D_METHOD("joint_is_disabled_collisions_between_bodies", "joint"), &PhysicsServer2D::joint_is_disabled_collisions_between_bodies);

	ClassDB::bind_method(D_METHOD("joint_make_pin", "joint", "anchor", "body_a", "body_b"), &PhysicsServer2D::joint_make_pin, DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("joint_make_groove", "joint", "groove1_a", "groove2_a", "anchor_b", "body_a", "body_b"), &PhysicsServer2D::joint_make_groove, DEFVAL(RID()), DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("joint_make_damped_spring", "joint", "anchor_a", "anchor_b", "body_a", "body_b"), &PhysicsServer2D::joint_make_damped_spring, DEFVAL(RID()));

	ClassDB::bind_method(D_METHOD("pin_joint_set_flag", "joint", "flag", "enabled"), &PhysicsServer2D::pin_joint_set_flag);
	ClassDB::bind_method(D_METHOD("pin_joint_get_flag", "joint", "flag"), &PhysicsServer2D::pin_joint_get_flag);

	ClassDB::bind_method(D_METHOD("pin_joint_set_param", "joint", "param", "value"), &PhysicsServer2D::pin_joint_set_param);
	ClassDB::bind_method(D_METHOD("pin_joint_get_param", "joint", "param"), &PhysicsServer2D::pin_joint_get_param);

	ClassDB::bind_method(D_METHOD("damped_spring_joint_set_param", "joint", "param", "value"), &PhysicsServer2D::damped_spring_joint_set_param);
	ClassDB::bind_method(D_METHOD("damped_spring_joint_get_param", "joint", "param"), &PhysicsServer2D::damped_spring_joint_get_param);

	ClassDB::bind_method(D_METHOD("joint_get_type", "joint"), &PhysicsServer2D::joint_get_type);

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &PhysicsServer2D::free);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &PhysicsServer2D::set_active);

	ClassDB::bind_method(D_METHOD("get_process_info", "process_info"), &PhysicsServer2D::get_process_info);

	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_RECYCLE_RADIUS);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_SEPARATION);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONTACT_DEFAULT_BIAS);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD);
	BIND_ENUM_CONSTANT(SPACE_PARAM_BODY_TIME_TO_SLEEP);
	BIND_ENUM_CONSTANT(SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS);
	BIND_ENUM_CONSTANT(SPACE_PARAM_SOLVER_ITERATIONS);

	BIND_ENUM_CONSTANT(SHAPE_WORLD_BOUNDARY);
	BIND_ENUM_CONSTANT(SHAPE_SEPARATION_RAY);
	BIND_ENUM_CONSTANT(SHAPE_SEGMENT);
	BIND_ENUM_CONSTANT(SHAPE_CIRCLE);
	BIND_ENUM_CONSTANT(SHAPE_RECTANGLE);
	BIND_ENUM_CONSTANT(SHAPE_CAPSULE);
	BIND_ENUM_CONSTANT(SHAPE_CONVEX_POLYGON);
	BIND_ENUM_CONSTANT(SHAPE_CONCAVE_POLYGON);
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

	BIND_ENUM_CONSTANT(JOINT_TYPE_PIN);
	BIND_ENUM_CONSTANT(JOINT_TYPE_GROOVE);
	BIND_ENUM_CONSTANT(JOINT_TYPE_DAMPED_SPRING);
	BIND_ENUM_CONSTANT(JOINT_TYPE_MAX);

	BIND_ENUM_CONSTANT(JOINT_PARAM_BIAS);
	BIND_ENUM_CONSTANT(JOINT_PARAM_MAX_BIAS);
	BIND_ENUM_CONSTANT(JOINT_PARAM_MAX_FORCE);

	BIND_ENUM_CONSTANT(PIN_JOINT_SOFTNESS);
	BIND_ENUM_CONSTANT(PIN_JOINT_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PIN_JOINT_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PIN_JOINT_MOTOR_TARGET_VELOCITY);

	BIND_ENUM_CONSTANT(PIN_JOINT_FLAG_ANGULAR_LIMIT_ENABLED);
	BIND_ENUM_CONSTANT(PIN_JOINT_FLAG_MOTOR_ENABLED);

	BIND_ENUM_CONSTANT(DAMPED_SPRING_REST_LENGTH);
	BIND_ENUM_CONSTANT(DAMPED_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(DAMPED_SPRING_DAMPING);

	BIND_ENUM_CONSTANT(CCD_MODE_DISABLED);
	BIND_ENUM_CONSTANT(CCD_MODE_CAST_RAY);
	BIND_ENUM_CONSTANT(CCD_MODE_CAST_SHAPE);

	BIND_ENUM_CONSTANT(AREA_BODY_ADDED);
	BIND_ENUM_CONSTANT(AREA_BODY_REMOVED);

	BIND_ENUM_CONSTANT(INFO_ACTIVE_OBJECTS);
	BIND_ENUM_CONSTANT(INFO_COLLISION_PAIRS);
	BIND_ENUM_CONSTANT(INFO_ISLAND_COUNT);
}

PhysicsServer2D::PhysicsServer2D() {
	singleton = this;

	// World2D physics space
	GLOBAL_DEF_BASIC("physics/2d/default_gravity", 980.0);
	GLOBAL_DEF_BASIC("physics/2d/default_gravity_vector", Vector2(0, 1));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/default_linear_damp", PROPERTY_HINT_RANGE, "-1,100,0.001,or_greater"), 0.1);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/default_angular_damp", PROPERTY_HINT_RANGE, "-1,100,0.001,or_greater"), 1.0);

	// PhysicsServer2D
	GLOBAL_DEF("physics/2d/sleep_threshold_linear", 2.0);
	GLOBAL_DEF("physics/2d/sleep_threshold_angular", Math::deg_to_rad(8.0));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/time_before_sleep", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), 0.5);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/2d/solver/solver_iterations", PROPERTY_HINT_RANGE, "1,32,1,or_greater"), 16);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/solver/contact_recycle_radius", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), 1.0);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/solver/contact_max_separation", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), 1.5);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/solver/contact_max_allowed_penetration", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater"), 0.3);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/solver/default_contact_bias", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.8);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/2d/solver/default_constraint_bias", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.2);
}

PhysicsServer2D::~PhysicsServer2D() {
	singleton = nullptr;
}

PhysicsServer2DManager *PhysicsServer2DManager::singleton = nullptr;
const String PhysicsServer2DManager::setting_property_name(PNAME("physics/2d/physics_engine"));

void PhysicsServer2DManager::on_servers_changed() {
	String physics_servers("DEFAULT");
	for (int i = get_servers_count() - 1; 0 <= i; --i) {
		physics_servers += "," + get_server_name(i);
	}
	ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, setting_property_name, PROPERTY_HINT_ENUM, physics_servers));
	ProjectSettings::get_singleton()->set_restart_if_changed(setting_property_name, true);
	ProjectSettings::get_singleton()->set_as_basic(setting_property_name, true);
}

void PhysicsServer2DManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_server", "name", "create_callback"), &PhysicsServer2DManager::register_server);
	ClassDB::bind_method(D_METHOD("set_default_server", "name", "priority"), &PhysicsServer2DManager::set_default_server);
}

PhysicsServer2DManager *PhysicsServer2DManager::get_singleton() {
	return singleton;
}

void PhysicsServer2DManager::register_server(const String &p_name, const Callable &p_create_callback) {
	//ERR_FAIL_COND(!p_create_callback.is_valid());
	ERR_FAIL_COND(find_server_id(p_name) != -1);
	physics_2d_servers.push_back(ClassInfo(p_name, p_create_callback));
	on_servers_changed();
}

void PhysicsServer2DManager::set_default_server(const String &p_name, int p_priority) {
	const int id = find_server_id(p_name);
	ERR_FAIL_COND(id == -1); // Not found
	if (default_server_priority < p_priority) {
		default_server_id = id;
		default_server_priority = p_priority;
	}
}

int PhysicsServer2DManager::find_server_id(const String &p_name) {
	for (int i = physics_2d_servers.size() - 1; 0 <= i; --i) {
		if (p_name == physics_2d_servers[i].name) {
			return i;
		}
	}
	return -1;
}

int PhysicsServer2DManager::get_servers_count() {
	return physics_2d_servers.size();
}

String PhysicsServer2DManager::get_server_name(int p_id) {
	ERR_FAIL_INDEX_V(p_id, get_servers_count(), "");
	return physics_2d_servers[p_id].name;
}

PhysicsServer2D *PhysicsServer2DManager::new_default_server() {
	if (default_server_id == -1) {
		return nullptr;
	}
	Variant ret;
	Callable::CallError ce;
	physics_2d_servers[default_server_id].create_callback.callp(nullptr, 0, ret, ce);
	ERR_FAIL_COND_V(ce.error != Callable::CallError::CALL_OK, nullptr);
	return Object::cast_to<PhysicsServer2D>(ret.get_validated_object());
}

PhysicsServer2D *PhysicsServer2DManager::new_server(const String &p_name) {
	int id = find_server_id(p_name);
	if (id == -1) {
		return nullptr;
	} else {
		Variant ret;
		Callable::CallError ce;
		physics_2d_servers[id].create_callback.callp(nullptr, 0, ret, ce);
		ERR_FAIL_COND_V(ce.error != Callable::CallError::CALL_OK, nullptr);
		return Object::cast_to<PhysicsServer2D>(ret.get_validated_object());
	}
}

PhysicsServer2DManager::PhysicsServer2DManager() {
	singleton = this;
}

PhysicsServer2DManager::~PhysicsServer2DManager() {
	singleton = nullptr;
}
