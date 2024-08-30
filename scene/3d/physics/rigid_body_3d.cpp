/**************************************************************************/
/*  rigid_body_3d.cpp                                                     */
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

#include "rigid_body_3d.h"

void RigidBody3D::_body_enter_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);
	ERR_FAIL_NULL(contact_monitor);
	HashMap<ObjectID, BodyState>::Iterator E = contact_monitor->body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->value.in_tree);

	E->value.in_tree = true;

	contact_monitor->locked = true;

	emit_signal(SceneStringName(body_entered), node);

	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(body_shape_entered), E->value.rid, node, E->value.shapes[i].body_shape, E->value.shapes[i].local_shape);
	}

	contact_monitor->locked = false;
}

void RigidBody3D::_body_exit_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);
	ERR_FAIL_NULL(contact_monitor);
	HashMap<ObjectID, BodyState>::Iterator E = contact_monitor->body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->value.in_tree);
	E->value.in_tree = false;

	contact_monitor->locked = true;

	emit_signal(SceneStringName(body_exited), node);

	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(body_shape_exited), E->value.rid, node, E->value.shapes[i].body_shape, E->value.shapes[i].local_shape);
	}

	contact_monitor->locked = false;
}

void RigidBody3D::_body_inout(int p_status, const RID &p_body, ObjectID p_instance, int p_body_shape, int p_local_shape) {
	bool body_in = p_status == 1;
	ObjectID objid = p_instance;

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = Object::cast_to<Node>(obj);

	ERR_FAIL_NULL(contact_monitor);
	HashMap<ObjectID, BodyState>::Iterator E = contact_monitor->body_map.find(objid);

	ERR_FAIL_COND(!body_in && !E);

	if (body_in) {
		if (!E) {
			E = contact_monitor->body_map.insert(objid, BodyState());
			E->value.rid = p_body;
			//E->value.rc=0;
			E->value.in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringName(tree_entered), callable_mp(this, &RigidBody3D::_body_enter_tree).bind(objid));
				node->connect(SceneStringName(tree_exiting), callable_mp(this, &RigidBody3D::_body_exit_tree).bind(objid));
				if (E->value.in_tree) {
					emit_signal(SceneStringName(body_entered), node);
				}
			}
		}
		//E->value.rc++;
		if (node) {
			E->value.shapes.insert(ShapePair(p_body_shape, p_local_shape));
		}

		if (E->value.in_tree) {
			emit_signal(SceneStringName(body_shape_entered), p_body, node, p_body_shape, p_local_shape);
		}

	} else {
		//E->value.rc--;

		if (node) {
			E->value.shapes.erase(ShapePair(p_body_shape, p_local_shape));
		}

		bool in_tree = E->value.in_tree;

		if (E->value.shapes.is_empty()) {
			if (node) {
				node->disconnect(SceneStringName(tree_entered), callable_mp(this, &RigidBody3D::_body_enter_tree));
				node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &RigidBody3D::_body_exit_tree));
				if (in_tree) {
					emit_signal(SceneStringName(body_exited), node);
				}
			}

			contact_monitor->body_map.remove(E);
		}
		if (node && in_tree) {
			emit_signal(SceneStringName(body_shape_exited), p_body, obj, p_body_shape, p_local_shape);
		}
	}
}

struct _RigidBodyInOut {
	RID rid;
	ObjectID id;
	int shape = 0;
	int local_shape = 0;
};

void RigidBody3D::_sync_body_state(PhysicsDirectBodyState3D *p_state) {
	set_ignore_transform_notification(true);
	set_global_transform(p_state->get_transform());
	set_ignore_transform_notification(false);

	linear_velocity = p_state->get_linear_velocity();
	angular_velocity = p_state->get_angular_velocity();

	inverse_inertia_tensor = p_state->get_inverse_inertia_tensor();

	contact_count = p_state->get_contact_count();

	if (sleeping != p_state->is_sleeping()) {
		sleeping = p_state->is_sleeping();
		emit_signal(SceneStringName(sleeping_state_changed));
	}
}

void RigidBody3D::_body_state_changed(PhysicsDirectBodyState3D *p_state) {
	lock_callback();

	if (GDVIRTUAL_IS_OVERRIDDEN(_integrate_forces)) {
		_sync_body_state(p_state);

		Transform3D old_transform = get_global_transform();
		GDVIRTUAL_CALL(_integrate_forces, p_state);
		Transform3D new_transform = get_global_transform();

		if (new_transform != old_transform) {
			// Update the physics server with the new transform, to prevent it from being overwritten at the sync below.
			PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_TRANSFORM, new_transform);
		}
	}

	_sync_body_state(p_state);
	_on_transform_changed();

	if (contact_monitor) {
		contact_monitor->locked = true;

		//untag all
		int rc = 0;
		for (KeyValue<ObjectID, BodyState> &E : contact_monitor->body_map) {
			for (int i = 0; i < E.value.shapes.size(); i++) {
				E.value.shapes[i].tagged = false;
				rc++;
			}
		}

		_RigidBodyInOut *toadd = (_RigidBodyInOut *)alloca(p_state->get_contact_count() * sizeof(_RigidBodyInOut));
		int toadd_count = 0;
		RigidBody3D_RemoveAction *toremove = (RigidBody3D_RemoveAction *)alloca(rc * sizeof(RigidBody3D_RemoveAction));
		int toremove_count = 0;

		//put the ones to add

		for (int i = 0; i < p_state->get_contact_count(); i++) {
			RID col_rid = p_state->get_contact_collider(i);
			ObjectID col_obj = p_state->get_contact_collider_id(i);
			int local_shape = p_state->get_contact_local_shape(i);
			int col_shape = p_state->get_contact_collider_shape(i);

			HashMap<ObjectID, BodyState>::Iterator E = contact_monitor->body_map.find(col_obj);
			if (!E) {
				toadd[toadd_count].rid = col_rid;
				toadd[toadd_count].local_shape = local_shape;
				toadd[toadd_count].id = col_obj;
				toadd[toadd_count].shape = col_shape;
				toadd_count++;
				continue;
			}

			ShapePair sp(col_shape, local_shape);
			int idx = E->value.shapes.find(sp);
			if (idx == -1) {
				toadd[toadd_count].rid = col_rid;
				toadd[toadd_count].local_shape = local_shape;
				toadd[toadd_count].id = col_obj;
				toadd[toadd_count].shape = col_shape;
				toadd_count++;
				continue;
			}

			E->value.shapes[idx].tagged = true;
		}

		//put the ones to remove

		for (const KeyValue<ObjectID, BodyState> &E : contact_monitor->body_map) {
			for (int i = 0; i < E.value.shapes.size(); i++) {
				if (!E.value.shapes[i].tagged) {
					toremove[toremove_count].rid = E.value.rid;
					toremove[toremove_count].body_id = E.key;
					toremove[toremove_count].pair = E.value.shapes[i];
					toremove_count++;
				}
			}
		}

		//process removals

		for (int i = 0; i < toremove_count; i++) {
			_body_inout(0, toremove[i].rid, toremove[i].body_id, toremove[i].pair.body_shape, toremove[i].pair.local_shape);
		}

		//process additions

		for (int i = 0; i < toadd_count; i++) {
			_body_inout(1, toadd[i].rid, toadd[i].id, toadd[i].shape, toadd[i].local_shape);
		}

		contact_monitor->locked = false;
	}

	unlock_callback();
}

void RigidBody3D::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				set_notify_local_transform(true); // Used for warnings and only in editor.
			}
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			update_configuration_info();
		} break;
	}
#endif
}

void RigidBody3D::_apply_body_mode() {
	if (freeze) {
		switch (freeze_mode) {
			case FREEZE_MODE_STATIC: {
				set_body_mode(PhysicsServer3D::BODY_MODE_STATIC);
			} break;
			case FREEZE_MODE_KINEMATIC: {
				set_body_mode(PhysicsServer3D::BODY_MODE_KINEMATIC);
			} break;
		}
	} else if (lock_rotation) {
		set_body_mode(PhysicsServer3D::BODY_MODE_RIGID_LINEAR);
	} else {
		set_body_mode(PhysicsServer3D::BODY_MODE_RIGID);
	}
}

void RigidBody3D::set_lock_rotation_enabled(bool p_lock_rotation) {
	if (p_lock_rotation == lock_rotation) {
		return;
	}

	lock_rotation = p_lock_rotation;
	_apply_body_mode();
}

bool RigidBody3D::is_lock_rotation_enabled() const {
	return lock_rotation;
}

void RigidBody3D::set_freeze_enabled(bool p_freeze) {
	if (p_freeze == freeze) {
		return;
	}

	freeze = p_freeze;
	_apply_body_mode();
}

bool RigidBody3D::is_freeze_enabled() const {
	return freeze;
}

void RigidBody3D::set_freeze_mode(FreezeMode p_freeze_mode) {
	if (p_freeze_mode == freeze_mode) {
		return;
	}

	freeze_mode = p_freeze_mode;
	_apply_body_mode();
}

RigidBody3D::FreezeMode RigidBody3D::get_freeze_mode() const {
	return freeze_mode;
}

void RigidBody3D::set_mass(real_t p_mass) {
	ERR_FAIL_COND(p_mass <= 0);
	mass = p_mass;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_MASS, mass);
}

real_t RigidBody3D::get_mass() const {
	return mass;
}

void RigidBody3D::set_inertia(const Vector3 &p_inertia) {
	ERR_FAIL_COND(p_inertia.x < 0);
	ERR_FAIL_COND(p_inertia.y < 0);
	ERR_FAIL_COND(p_inertia.z < 0);

	inertia = p_inertia;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_INERTIA, inertia);
}

const Vector3 &RigidBody3D::get_inertia() const {
	return inertia;
}

void RigidBody3D::set_center_of_mass_mode(CenterOfMassMode p_mode) {
	if (center_of_mass_mode == p_mode) {
		return;
	}

	center_of_mass_mode = p_mode;

	switch (center_of_mass_mode) {
		case CENTER_OF_MASS_MODE_AUTO: {
			center_of_mass = Vector3();
			PhysicsServer3D::get_singleton()->body_reset_mass_properties(get_rid());
			if (inertia != Vector3()) {
				PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_INERTIA, inertia);
			}
		} break;

		case CENTER_OF_MASS_MODE_CUSTOM: {
			PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS, center_of_mass);
		} break;
	}

	notify_property_list_changed();
}

RigidBody3D::CenterOfMassMode RigidBody3D::get_center_of_mass_mode() const {
	return center_of_mass_mode;
}

void RigidBody3D::set_center_of_mass(const Vector3 &p_center_of_mass) {
	if (center_of_mass == p_center_of_mass) {
		return;
	}

	ERR_FAIL_COND(center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM);
	center_of_mass = p_center_of_mass;

	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS, center_of_mass);
}

const Vector3 &RigidBody3D::get_center_of_mass() const {
	return center_of_mass;
}

void RigidBody3D::set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override) {
	if (physics_material_override.is_valid()) {
		physics_material_override->disconnect_changed(callable_mp(this, &RigidBody3D::_reload_physics_characteristics));
	}

	physics_material_override = p_physics_material_override;

	if (physics_material_override.is_valid()) {
		physics_material_override->connect_changed(callable_mp(this, &RigidBody3D::_reload_physics_characteristics));
	}
	_reload_physics_characteristics();
}

Ref<PhysicsMaterial> RigidBody3D::get_physics_material_override() const {
	return physics_material_override;
}

void RigidBody3D::set_gravity_scale(real_t p_gravity_scale) {
	gravity_scale = p_gravity_scale;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE, gravity_scale);
}

real_t RigidBody3D::get_gravity_scale() const {
	return gravity_scale;
}

void RigidBody3D::set_linear_damp_mode(DampMode p_mode) {
	linear_damp_mode = p_mode;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE, linear_damp_mode);
}

RigidBody3D::DampMode RigidBody3D::get_linear_damp_mode() const {
	return linear_damp_mode;
}

void RigidBody3D::set_angular_damp_mode(DampMode p_mode) {
	angular_damp_mode = p_mode;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE, angular_damp_mode);
}

RigidBody3D::DampMode RigidBody3D::get_angular_damp_mode() const {
	return angular_damp_mode;
}

void RigidBody3D::set_linear_damp(real_t p_linear_damp) {
	ERR_FAIL_COND(p_linear_damp < 0.0);
	linear_damp = p_linear_damp;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_LINEAR_DAMP, linear_damp);
}

real_t RigidBody3D::get_linear_damp() const {
	return linear_damp;
}

void RigidBody3D::set_angular_damp(real_t p_angular_damp) {
	ERR_FAIL_COND(p_angular_damp < 0.0);
	angular_damp = p_angular_damp;
	PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP, angular_damp);
}

real_t RigidBody3D::get_angular_damp() const {
	return angular_damp;
}

void RigidBody3D::set_axis_velocity(const Vector3 &p_axis) {
	Vector3 axis = p_axis.normalized();
	linear_velocity -= axis * axis.dot(linear_velocity);
	linear_velocity += p_axis;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, linear_velocity);
}

void RigidBody3D::set_linear_velocity(const Vector3 &p_velocity) {
	linear_velocity = p_velocity;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, linear_velocity);
}

Vector3 RigidBody3D::get_linear_velocity() const {
	return linear_velocity;
}

void RigidBody3D::set_angular_velocity(const Vector3 &p_velocity) {
	angular_velocity = p_velocity;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, angular_velocity);
}

Vector3 RigidBody3D::get_angular_velocity() const {
	return angular_velocity;
}

Basis RigidBody3D::get_inverse_inertia_tensor() const {
	return inverse_inertia_tensor;
}

void RigidBody3D::set_use_custom_integrator(bool p_enable) {
	if (custom_integrator == p_enable) {
		return;
	}

	custom_integrator = p_enable;
	PhysicsServer3D::get_singleton()->body_set_omit_force_integration(get_rid(), p_enable);
}

bool RigidBody3D::is_using_custom_integrator() {
	return custom_integrator;
}

void RigidBody3D::set_sleeping(bool p_sleeping) {
	sleeping = p_sleeping;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_SLEEPING, sleeping);
}

void RigidBody3D::set_can_sleep(bool p_active) {
	can_sleep = p_active;
	PhysicsServer3D::get_singleton()->body_set_state(get_rid(), PhysicsServer3D::BODY_STATE_CAN_SLEEP, p_active);
}

bool RigidBody3D::is_able_to_sleep() const {
	return can_sleep;
}

bool RigidBody3D::is_sleeping() const {
	return sleeping;
}

void RigidBody3D::set_max_contacts_reported(int p_amount) {
	max_contacts_reported = p_amount;
	PhysicsServer3D::get_singleton()->body_set_max_contacts_reported(get_rid(), p_amount);
}

int RigidBody3D::get_max_contacts_reported() const {
	return max_contacts_reported;
}

int RigidBody3D::get_contact_count() const {
	return contact_count;
}

void RigidBody3D::apply_central_impulse(const Vector3 &p_impulse) {
	PhysicsServer3D::get_singleton()->body_apply_central_impulse(get_rid(), p_impulse);
}

void RigidBody3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	PhysicsServer3D *singleton = PhysicsServer3D::get_singleton();
	singleton->body_apply_impulse(get_rid(), p_impulse, p_position);
}

void RigidBody3D::apply_torque_impulse(const Vector3 &p_impulse) {
	PhysicsServer3D::get_singleton()->body_apply_torque_impulse(get_rid(), p_impulse);
}

void RigidBody3D::apply_central_force(const Vector3 &p_force) {
	PhysicsServer3D::get_singleton()->body_apply_central_force(get_rid(), p_force);
}

void RigidBody3D::apply_force(const Vector3 &p_force, const Vector3 &p_position) {
	PhysicsServer3D *singleton = PhysicsServer3D::get_singleton();
	singleton->body_apply_force(get_rid(), p_force, p_position);
}

void RigidBody3D::apply_torque(const Vector3 &p_torque) {
	PhysicsServer3D::get_singleton()->body_apply_torque(get_rid(), p_torque);
}

void RigidBody3D::add_constant_central_force(const Vector3 &p_force) {
	PhysicsServer3D::get_singleton()->body_add_constant_central_force(get_rid(), p_force);
}

void RigidBody3D::add_constant_force(const Vector3 &p_force, const Vector3 &p_position) {
	PhysicsServer3D *singleton = PhysicsServer3D::get_singleton();
	singleton->body_add_constant_force(get_rid(), p_force, p_position);
}

void RigidBody3D::add_constant_torque(const Vector3 &p_torque) {
	PhysicsServer3D::get_singleton()->body_add_constant_torque(get_rid(), p_torque);
}

void RigidBody3D::set_constant_force(const Vector3 &p_force) {
	PhysicsServer3D::get_singleton()->body_set_constant_force(get_rid(), p_force);
}

Vector3 RigidBody3D::get_constant_force() const {
	return PhysicsServer3D::get_singleton()->body_get_constant_force(get_rid());
}

void RigidBody3D::set_constant_torque(const Vector3 &p_torque) {
	PhysicsServer3D::get_singleton()->body_set_constant_torque(get_rid(), p_torque);
}

Vector3 RigidBody3D::get_constant_torque() const {
	return PhysicsServer3D::get_singleton()->body_get_constant_torque(get_rid());
}

void RigidBody3D::set_use_continuous_collision_detection(bool p_enable) {
	ccd = p_enable;
	PhysicsServer3D::get_singleton()->body_set_enable_continuous_collision_detection(get_rid(), p_enable);
}

bool RigidBody3D::is_using_continuous_collision_detection() const {
	return ccd;
}

void RigidBody3D::set_contact_monitor(bool p_enabled) {
	if (p_enabled == is_contact_monitor_enabled()) {
		return;
	}

	if (!p_enabled) {
		ERR_FAIL_COND_MSG(contact_monitor->locked, "Can't disable contact monitoring during in/out callback. Use call_deferred(\"set_contact_monitor\", false) instead.");

		for (const KeyValue<ObjectID, BodyState> &E : contact_monitor->body_map) {
			//clean up mess
			Object *obj = ObjectDB::get_instance(E.key);
			Node *node = Object::cast_to<Node>(obj);

			if (node) {
				node->disconnect(SceneStringName(tree_entered), callable_mp(this, &RigidBody3D::_body_enter_tree));
				node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &RigidBody3D::_body_exit_tree));
			}
		}

		memdelete(contact_monitor);
		contact_monitor = nullptr;
	} else {
		contact_monitor = memnew(ContactMonitor);
		contact_monitor->locked = false;
	}

	notify_property_list_changed();
}

bool RigidBody3D::is_contact_monitor_enabled() const {
	return contact_monitor != nullptr;
}

TypedArray<Node3D> RigidBody3D::get_colliding_bodies() const {
	ERR_FAIL_NULL_V(contact_monitor, TypedArray<Node3D>());

	TypedArray<Node3D> ret;
	ret.resize(contact_monitor->body_map.size());
	int idx = 0;
	for (const KeyValue<ObjectID, BodyState> &E : contact_monitor->body_map) {
		Object *obj = ObjectDB::get_instance(E.key);
		if (!obj) {
			ret.resize(ret.size() - 1); //ops
		} else {
			ret[idx++] = obj;
		}
	}

	return ret;
}

void RigidBody3D::_reload_physics_characteristics() {
	if (physics_material_override.is_null()) {
		PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, 0);
		PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, 1);
	} else {
		PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_BOUNCE, physics_material_override->computed_bounce());
		PhysicsServer3D::get_singleton()->body_set_param(get_rid(), PhysicsServer3D::BODY_PARAM_FRICTION, physics_material_override->computed_friction());
	}
}

#ifdef TOOLS_ENABLED
Vector<ConfigurationInfo> RigidBody3D::get_configuration_info() const {
	Vector<ConfigurationInfo> infos = PhysicsBody3D::get_configuration_info();

	Vector3 scale = get_transform().get_basis().get_scale();
	if (ABS(scale.x - 1.0) > 0.05 || ABS(scale.y - 1.0) > 0.05 || ABS(scale.z - 1.0) > 0.05) {
		CONFIG_WARNING(RTR("Scale changes to RigidBody3D will be overridden by the physics engine when running.\nPlease change the size in children collision shapes instead."));
	}

	return infos;
}
#endif

void RigidBody3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &RigidBody3D::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &RigidBody3D::get_mass);

	ClassDB::bind_method(D_METHOD("set_inertia", "inertia"), &RigidBody3D::set_inertia);
	ClassDB::bind_method(D_METHOD("get_inertia"), &RigidBody3D::get_inertia);

	ClassDB::bind_method(D_METHOD("set_center_of_mass_mode", "mode"), &RigidBody3D::set_center_of_mass_mode);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_mode"), &RigidBody3D::get_center_of_mass_mode);

	ClassDB::bind_method(D_METHOD("set_center_of_mass", "center_of_mass"), &RigidBody3D::set_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &RigidBody3D::get_center_of_mass);

	ClassDB::bind_method(D_METHOD("set_physics_material_override", "physics_material_override"), &RigidBody3D::set_physics_material_override);
	ClassDB::bind_method(D_METHOD("get_physics_material_override"), &RigidBody3D::get_physics_material_override);

	ClassDB::bind_method(D_METHOD("set_linear_velocity", "linear_velocity"), &RigidBody3D::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &RigidBody3D::get_linear_velocity);

	ClassDB::bind_method(D_METHOD("set_angular_velocity", "angular_velocity"), &RigidBody3D::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &RigidBody3D::get_angular_velocity);

	ClassDB::bind_method(D_METHOD("get_inverse_inertia_tensor"), &RigidBody3D::get_inverse_inertia_tensor);

	ClassDB::bind_method(D_METHOD("set_gravity_scale", "gravity_scale"), &RigidBody3D::set_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_gravity_scale"), &RigidBody3D::get_gravity_scale);

	ClassDB::bind_method(D_METHOD("set_linear_damp_mode", "linear_damp_mode"), &RigidBody3D::set_linear_damp_mode);
	ClassDB::bind_method(D_METHOD("get_linear_damp_mode"), &RigidBody3D::get_linear_damp_mode);

	ClassDB::bind_method(D_METHOD("set_angular_damp_mode", "angular_damp_mode"), &RigidBody3D::set_angular_damp_mode);
	ClassDB::bind_method(D_METHOD("get_angular_damp_mode"), &RigidBody3D::get_angular_damp_mode);

	ClassDB::bind_method(D_METHOD("set_linear_damp", "linear_damp"), &RigidBody3D::set_linear_damp);
	ClassDB::bind_method(D_METHOD("get_linear_damp"), &RigidBody3D::get_linear_damp);

	ClassDB::bind_method(D_METHOD("set_angular_damp", "angular_damp"), &RigidBody3D::set_angular_damp);
	ClassDB::bind_method(D_METHOD("get_angular_damp"), &RigidBody3D::get_angular_damp);

	ClassDB::bind_method(D_METHOD("set_max_contacts_reported", "amount"), &RigidBody3D::set_max_contacts_reported);
	ClassDB::bind_method(D_METHOD("get_max_contacts_reported"), &RigidBody3D::get_max_contacts_reported);
	ClassDB::bind_method(D_METHOD("get_contact_count"), &RigidBody3D::get_contact_count);

	ClassDB::bind_method(D_METHOD("set_use_custom_integrator", "enable"), &RigidBody3D::set_use_custom_integrator);
	ClassDB::bind_method(D_METHOD("is_using_custom_integrator"), &RigidBody3D::is_using_custom_integrator);

	ClassDB::bind_method(D_METHOD("set_contact_monitor", "enabled"), &RigidBody3D::set_contact_monitor);
	ClassDB::bind_method(D_METHOD("is_contact_monitor_enabled"), &RigidBody3D::is_contact_monitor_enabled);

	ClassDB::bind_method(D_METHOD("set_use_continuous_collision_detection", "enable"), &RigidBody3D::set_use_continuous_collision_detection);
	ClassDB::bind_method(D_METHOD("is_using_continuous_collision_detection"), &RigidBody3D::is_using_continuous_collision_detection);

	ClassDB::bind_method(D_METHOD("set_axis_velocity", "axis_velocity"), &RigidBody3D::set_axis_velocity);

	ClassDB::bind_method(D_METHOD("apply_central_impulse", "impulse"), &RigidBody3D::apply_central_impulse);
	ClassDB::bind_method(D_METHOD("apply_impulse", "impulse", "position"), &RigidBody3D::apply_impulse, Vector3());
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &RigidBody3D::apply_torque_impulse);

	ClassDB::bind_method(D_METHOD("apply_central_force", "force"), &RigidBody3D::apply_central_force);
	ClassDB::bind_method(D_METHOD("apply_force", "force", "position"), &RigidBody3D::apply_force, Vector3());
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &RigidBody3D::apply_torque);

	ClassDB::bind_method(D_METHOD("add_constant_central_force", "force"), &RigidBody3D::add_constant_central_force);
	ClassDB::bind_method(D_METHOD("add_constant_force", "force", "position"), &RigidBody3D::add_constant_force, Vector3());
	ClassDB::bind_method(D_METHOD("add_constant_torque", "torque"), &RigidBody3D::add_constant_torque);

	ClassDB::bind_method(D_METHOD("set_constant_force", "force"), &RigidBody3D::set_constant_force);
	ClassDB::bind_method(D_METHOD("get_constant_force"), &RigidBody3D::get_constant_force);

	ClassDB::bind_method(D_METHOD("set_constant_torque", "torque"), &RigidBody3D::set_constant_torque);
	ClassDB::bind_method(D_METHOD("get_constant_torque"), &RigidBody3D::get_constant_torque);

	ClassDB::bind_method(D_METHOD("set_sleeping", "sleeping"), &RigidBody3D::set_sleeping);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &RigidBody3D::is_sleeping);

	ClassDB::bind_method(D_METHOD("set_can_sleep", "able_to_sleep"), &RigidBody3D::set_can_sleep);
	ClassDB::bind_method(D_METHOD("is_able_to_sleep"), &RigidBody3D::is_able_to_sleep);

	ClassDB::bind_method(D_METHOD("set_lock_rotation_enabled", "lock_rotation"), &RigidBody3D::set_lock_rotation_enabled);
	ClassDB::bind_method(D_METHOD("is_lock_rotation_enabled"), &RigidBody3D::is_lock_rotation_enabled);

	ClassDB::bind_method(D_METHOD("set_freeze_enabled", "freeze_mode"), &RigidBody3D::set_freeze_enabled);
	ClassDB::bind_method(D_METHOD("is_freeze_enabled"), &RigidBody3D::is_freeze_enabled);

	ClassDB::bind_method(D_METHOD("set_freeze_mode", "freeze_mode"), &RigidBody3D::set_freeze_mode);
	ClassDB::bind_method(D_METHOD("get_freeze_mode"), &RigidBody3D::get_freeze_mode);

	ClassDB::bind_method(D_METHOD("get_colliding_bodies"), &RigidBody3D::get_colliding_bodies);

	GDVIRTUAL_BIND(_integrate_forces, "state");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass", PROPERTY_HINT_RANGE, "0.001,1000,0.001,or_greater,exp,suffix:kg"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material_override", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material_override", "get_physics_material_override");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_scale", PROPERTY_HINT_RANGE, "-8,8,0.001,or_less,or_greater"), "set_gravity_scale", "get_gravity_scale");
	ADD_GROUP("Mass Distribution", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "center_of_mass_mode", PROPERTY_HINT_ENUM, "Auto,Custom"), "set_center_of_mass_mode", "get_center_of_mass_mode");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass", PROPERTY_HINT_RANGE, "-10,10,0.01,or_less,or_greater,suffix:m"), "set_center_of_mass", "get_center_of_mass");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "inertia", PROPERTY_HINT_RANGE, U"0,1000,0.01,or_greater,exp,suffix:kg\u22C5m\u00B2"), "set_inertia", "get_inertia");
	ADD_GROUP("Deactivation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sleeping"), "set_sleeping", "is_sleeping");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), "set_can_sleep", "is_able_to_sleep");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_rotation"), "set_lock_rotation_enabled", "is_lock_rotation_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "freeze"), "set_freeze_enabled", "is_freeze_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "freeze_mode", PROPERTY_HINT_ENUM, "Static,Kinematic"), "set_freeze_mode", "get_freeze_mode");
	ADD_GROUP("Solver", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "custom_integrator"), "set_use_custom_integrator", "is_using_custom_integrator");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "continuous_cd"), "set_use_continuous_collision_detection", "is_using_continuous_collision_detection");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "contact_monitor"), "set_contact_monitor", "is_contact_monitor_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_contacts_reported", PROPERTY_HINT_RANGE, "0,64,1,or_greater"), "set_max_contacts_reported", "get_max_contacts_reported");
	ADD_GROUP("Linear", "linear_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_linear_velocity", "get_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "linear_damp_mode", PROPERTY_HINT_ENUM, "Combine,Replace"), "set_linear_damp_mode", "get_linear_damp_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_linear_damp", "get_linear_damp");
	ADD_GROUP("Angular", "angular_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_angular_velocity", "get_angular_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "angular_damp_mode", PROPERTY_HINT_ENUM, "Combine,Replace"), "set_angular_damp_mode", "get_angular_damp_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_angular_damp", "get_angular_damp");
	ADD_GROUP("Constant Forces", "constant_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "constant_force", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m/s\u00B2 (N)"), "set_constant_force", "get_constant_force");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "constant_torque", PROPERTY_HINT_NONE, U"suffix:kg\u22C5m\u00B2/s\u00B2/rad"), "set_constant_torque", "get_constant_torque");

	ADD_SIGNAL(MethodInfo("body_shape_entered", PropertyInfo(Variant::RID, "body_rid"), PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node"), PropertyInfo(Variant::INT, "body_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("body_shape_exited", PropertyInfo(Variant::RID, "body_rid"), PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node"), PropertyInfo(Variant::INT, "body_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("body_entered", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("body_exited", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("sleeping_state_changed"));

	BIND_ENUM_CONSTANT(FREEZE_MODE_STATIC);
	BIND_ENUM_CONSTANT(FREEZE_MODE_KINEMATIC);

	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_AUTO);
	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_CUSTOM);

	BIND_ENUM_CONSTANT(DAMP_MODE_COMBINE);
	BIND_ENUM_CONSTANT(DAMP_MODE_REPLACE);
}

void RigidBody3D::_validate_property(PropertyInfo &p_property) const {
	if (center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM && p_property.name == "center_of_mass") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (!contact_monitor && p_property.name == "max_contacts_reported") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

RigidBody3D::RigidBody3D() :
		PhysicsBody3D(PhysicsServer3D::BODY_MODE_RIGID) {
	PhysicsServer3D::get_singleton()->body_set_state_sync_callback(get_rid(), callable_mp(this, &RigidBody3D::_body_state_changed));
}

RigidBody3D::~RigidBody3D() {
	if (contact_monitor) {
		memdelete(contact_monitor);
	}
}
