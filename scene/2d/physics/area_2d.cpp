/**************************************************************************/
/*  area_2d.cpp                                                           */
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

#include "area_2d.h"

#include "servers/audio_server.h"

void Area2D::set_gravity_space_override_mode(SpaceOverride p_mode) {
	gravity_space_override = p_mode;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY_OVERRIDE_MODE, p_mode);
}

Area2D::SpaceOverride Area2D::get_gravity_space_override_mode() const {
	return gravity_space_override;
}

void Area2D::set_gravity_type(GravityType p_type) {
	gravity_type = p_type;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY_TYPE, p_type);
	if (gravity_type == GRAVITY_TYPE_TARGET) {
		PhysicsServer2D::get_singleton()->area_set_gravity_target_callback(get_rid(), callable_mp(this, &Area2D::calculate_gravity_target));
	} else {
		PhysicsServer2D::get_singleton()->area_set_gravity_target_callback(get_rid(), Callable());
	}
}

Area2D::GravityType Area2D::get_gravity_type() const {
	return gravity_type;
}

void Area2D::set_gravity_is_point(bool p_enabled) {
	set_gravity_type(p_enabled ? GravityType::GRAVITY_TYPE_POINT : GravityType::GRAVITY_TYPE_DIRECTIONAL);
}

bool Area2D::is_gravity_a_point() const {
	return gravity_type != GravityType::GRAVITY_TYPE_DIRECTIONAL;
}

void Area2D::set_gravity_point_unit_distance(real_t p_scale) {
	gravity_point_unit_distance = p_scale;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE, p_scale);
}

real_t Area2D::get_gravity_point_unit_distance() const {
	return gravity_point_unit_distance;
}

void Area2D::set_gravity_point_center(const Vector2 &p_center) {
	gravity_vec = p_center;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY_VECTOR, p_center);
}

const Vector2 &Area2D::get_gravity_point_center() const {
	return gravity_vec;
}

void Area2D::set_gravity_direction(const Vector2 &p_direction) {
	gravity_vec = p_direction;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY_VECTOR, p_direction);
}

const Vector2 &Area2D::get_gravity_direction() const {
	return gravity_vec;
}

void Area2D::set_gravity(real_t p_gravity) {
	gravity = p_gravity;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_GRAVITY, p_gravity);
}

real_t Area2D::get_gravity() const {
	return gravity;
}

void Area2D::set_linear_damp_space_override_mode(SpaceOverride p_mode) {
	linear_damp_space_override = p_mode;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE, p_mode);
}

Area2D::SpaceOverride Area2D::get_linear_damp_space_override_mode() const {
	return linear_damp_space_override;
}

void Area2D::set_angular_damp_space_override_mode(SpaceOverride p_mode) {
	angular_damp_space_override = p_mode;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE, p_mode);
}

Area2D::SpaceOverride Area2D::get_angular_damp_space_override_mode() const {
	return angular_damp_space_override;
}

void Area2D::set_linear_damp(real_t p_linear_damp) {
	linear_damp = p_linear_damp;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_LINEAR_DAMP, p_linear_damp);
}

real_t Area2D::get_linear_damp() const {
	return linear_damp;
}

void Area2D::set_angular_damp(real_t p_angular_damp) {
	angular_damp = p_angular_damp;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP, p_angular_damp);
}

real_t Area2D::get_angular_damp() const {
	return angular_damp;
}

void Area2D::set_priority(int p_priority) {
	priority = p_priority;
	PhysicsServer2D::get_singleton()->area_set_param(get_rid(), PhysicsServer2D::AREA_PARAM_PRIORITY, p_priority);
}

int Area2D::get_priority() const {
	return priority;
}

void Area2D::_body_enter_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);

	HashMap<ObjectID, BodyState>::Iterator E = body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->value.in_tree);

	E->value.in_tree = true;
	emit_signal(SceneStringName(body_entered), node);
	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(body_shape_entered), E->value.rid, node, E->value.shapes[i].body_shape, E->value.shapes[i].area_shape);
	}
}

void Area2D::_body_exit_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);
	HashMap<ObjectID, BodyState>::Iterator E = body_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->value.in_tree);
	E->value.in_tree = false;
	emit_signal(SceneStringName(body_exited), node);
	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(body_shape_exited), E->value.rid, node, E->value.shapes[i].body_shape, E->value.shapes[i].area_shape);
	}
}

void Area2D::_body_inout(int p_status, const RID &p_body, ObjectID p_instance, int p_body_shape, int p_area_shape) {
	bool body_in = p_status == PhysicsServer2D::AREA_BODY_ADDED;
	ObjectID objid = p_instance;

	// Exit early if instance is invalid.
	if (objid.is_null()) {
		// Emit the appropriate signals.
		lock_callback();
		locked = true;
		if (body_in) {
			emit_signal(SceneStringName(body_shape_entered), p_body, (Node *)nullptr, p_body_shape, p_area_shape);
		} else {
			emit_signal(SceneStringName(body_shape_exited), p_body, (Node *)nullptr, p_body_shape, p_area_shape);
		}
		locked = false;
		unlock_callback();
		return;
	}

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = Object::cast_to<Node>(obj);

	HashMap<ObjectID, BodyState>::Iterator E = body_map.find(objid);

	if (!body_in && !E) {
		return; //does not exist because it was likely removed from the tree
	}

	lock_callback();
	locked = true;

	if (body_in) {
		if (!E) {
			E = body_map.insert(objid, BodyState());
			E->value.rid = p_body;
			E->value.rc = 0;
			E->value.in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_body_enter_tree).bind(objid));
				node->connect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_body_exit_tree).bind(objid));
				if (E->value.in_tree) {
					emit_signal(SceneStringName(body_entered), node);
				}
			}
		}
		E->value.rc++;
		if (node) {
			E->value.shapes.insert(ShapePair(p_body_shape, p_area_shape));
		}

		if (!node || E->value.in_tree) {
			emit_signal(SceneStringName(body_shape_entered), p_body, node, p_body_shape, p_area_shape);
		}

	} else {
		E->value.rc--;

		if (node) {
			E->value.shapes.erase(ShapePair(p_body_shape, p_area_shape));
		}

		bool in_tree = E->value.in_tree;
		if (E->value.rc == 0) {
			body_map.remove(E);
			if (node) {
				node->disconnect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_body_enter_tree));
				node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_body_exit_tree));
				if (in_tree) {
					emit_signal(SceneStringName(body_exited), obj);
				}
			}
		}
		if (!node || in_tree) {
			emit_signal(SceneStringName(body_shape_exited), p_body, obj, p_body_shape, p_area_shape);
		}
	}

	locked = false;
	unlock_callback();
}

void Area2D::_area_enter_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);

	HashMap<ObjectID, AreaState>::Iterator E = area_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(E->value.in_tree);

	E->value.in_tree = true;
	emit_signal(SceneStringName(area_entered), node);
	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(area_shape_entered), E->value.rid, node, E->value.shapes[i].area_shape, E->value.shapes[i].self_shape);
	}
}

void Area2D::_area_exit_tree(ObjectID p_id) {
	Object *obj = ObjectDB::get_instance(p_id);
	Node *node = Object::cast_to<Node>(obj);
	ERR_FAIL_NULL(node);
	HashMap<ObjectID, AreaState>::Iterator E = area_map.find(p_id);
	ERR_FAIL_COND(!E);
	ERR_FAIL_COND(!E->value.in_tree);
	E->value.in_tree = false;
	emit_signal(SceneStringName(area_exited), node);
	for (int i = 0; i < E->value.shapes.size(); i++) {
		emit_signal(SceneStringName(area_shape_exited), E->value.rid, node, E->value.shapes[i].area_shape, E->value.shapes[i].self_shape);
	}
}

void Area2D::_area_inout(int p_status, const RID &p_area, ObjectID p_instance, int p_area_shape, int p_self_shape) {
	bool area_in = p_status == PhysicsServer2D::AREA_BODY_ADDED;
	ObjectID objid = p_instance;

	// Exit early if instance is invalid.
	if (objid.is_null()) {
		// Emit the appropriate signals.
		lock_callback();
		locked = true;
		if (area_in) {
			emit_signal(SceneStringName(area_shape_entered), p_area, (Node *)nullptr, p_area_shape, p_self_shape);
		} else {
			emit_signal(SceneStringName(area_shape_exited), p_area, (Node *)nullptr, p_area_shape, p_self_shape);
		}
		locked = false;
		unlock_callback();
		return;
	}

	Object *obj = ObjectDB::get_instance(objid);
	Node *node = Object::cast_to<Node>(obj);

	HashMap<ObjectID, AreaState>::Iterator E = area_map.find(objid);

	if (!area_in && !E) {
		return; //likely removed from the tree
	}

	lock_callback();
	locked = true;

	if (area_in) {
		if (!E) {
			E = area_map.insert(objid, AreaState());
			E->value.rid = p_area;
			E->value.rc = 0;
			E->value.in_tree = node && node->is_inside_tree();
			if (node) {
				node->connect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_area_enter_tree).bind(objid));
				node->connect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_area_exit_tree).bind(objid));
				if (E->value.in_tree) {
					emit_signal(SceneStringName(area_entered), node);
				}
			}
		}
		E->value.rc++;
		if (node) {
			E->value.shapes.insert(AreaShapePair(p_area_shape, p_self_shape));
		}

		if (!node || E->value.in_tree) {
			emit_signal(SceneStringName(area_shape_entered), p_area, node, p_area_shape, p_self_shape);
		}

	} else {
		E->value.rc--;

		if (node) {
			E->value.shapes.erase(AreaShapePair(p_area_shape, p_self_shape));
		}

		bool in_tree = E->value.in_tree;
		if (E->value.rc == 0) {
			area_map.remove(E);
			if (node) {
				node->disconnect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_area_enter_tree));
				node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_area_exit_tree));
				if (in_tree) {
					emit_signal(SceneStringName(area_exited), obj);
				}
			}
		}
		if (!node || in_tree) {
			emit_signal(SceneStringName(area_shape_exited), p_area, obj, p_area_shape, p_self_shape);
		}
	}

	locked = false;
	unlock_callback();
}

void Area2D::_clear_monitoring() {
	ERR_FAIL_COND_MSG(locked, "This function can't be used during the in/out signal.");

	{
		HashMap<ObjectID, BodyState> bmcopy = body_map;
		body_map.clear();
		//disconnect all monitored stuff

		for (const KeyValue<ObjectID, BodyState> &E : bmcopy) {
			Object *obj = ObjectDB::get_instance(E.key);
			Node *node = Object::cast_to<Node>(obj);

			if (!node) { //node may have been deleted in previous frame or at other legitimate point
				continue;
			}

			node->disconnect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_body_enter_tree));
			node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_body_exit_tree));

			if (!E.value.in_tree) {
				continue;
			}

			for (int i = 0; i < E.value.shapes.size(); i++) {
				emit_signal(SceneStringName(body_shape_exited), E.value.rid, node, E.value.shapes[i].body_shape, E.value.shapes[i].area_shape);
			}

			emit_signal(SceneStringName(body_exited), obj);
		}
	}

	{
		HashMap<ObjectID, AreaState> bmcopy = area_map;
		area_map.clear();
		//disconnect all monitored stuff

		for (const KeyValue<ObjectID, AreaState> &E : bmcopy) {
			Object *obj = ObjectDB::get_instance(E.key);
			Node *node = Object::cast_to<Node>(obj);

			if (!node) { //node may have been deleted in previous frame or at other legitimate point
				continue;
			}

			node->disconnect(SceneStringName(tree_entered), callable_mp(this, &Area2D::_area_enter_tree));
			node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Area2D::_area_exit_tree));

			if (!E.value.in_tree) {
				continue;
			}

			for (int i = 0; i < E.value.shapes.size(); i++) {
				emit_signal(SceneStringName(area_shape_exited), E.value.rid, node, E.value.shapes[i].area_shape, E.value.shapes[i].self_shape);
			}

			emit_signal(SceneStringName(area_exited), obj);
		}
	}
}

void Area2D::_space_changed(const RID &p_new_space) {
	if (p_new_space.is_null()) {
		_clear_monitoring();
	}
}

void Area2D::set_monitoring(bool p_enable) {
	if (p_enable == monitoring) {
		return;
	}
	ERR_FAIL_COND_MSG(locked, "Function blocked during in/out signal. Use set_deferred(\"monitoring\", true/false).");

	monitoring = p_enable;

	if (monitoring) {
		PhysicsServer2D::get_singleton()->area_set_monitor_callback(get_rid(), callable_mp(this, &Area2D::_body_inout));
		PhysicsServer2D::get_singleton()->area_set_area_monitor_callback(get_rid(), callable_mp(this, &Area2D::_area_inout));
	} else {
		PhysicsServer2D::get_singleton()->area_set_monitor_callback(get_rid(), Callable());
		PhysicsServer2D::get_singleton()->area_set_area_monitor_callback(get_rid(), Callable());
		_clear_monitoring();
	}
}

bool Area2D::is_monitoring() const {
	return monitoring;
}

void Area2D::set_monitorable(bool p_enable) {
	ERR_FAIL_COND_MSG(locked || (is_inside_tree() && PhysicsServer2D::get_singleton()->is_flushing_queries()), "Function blocked during in/out signal. Use set_deferred(\"monitorable\", true/false).");

	if (p_enable == monitorable) {
		return;
	}

	monitorable = p_enable;

	PhysicsServer2D::get_singleton()->area_set_monitorable(get_rid(), monitorable);
}

bool Area2D::is_monitorable() const {
	return monitorable;
}

TypedArray<Node2D> Area2D::get_overlapping_bodies() const {
	TypedArray<Node2D> ret;
	ERR_FAIL_COND_V_MSG(!monitoring, ret, "Can't find overlapping bodies when monitoring is off.");
	ret.resize(body_map.size());
	int idx = 0;
	for (const KeyValue<ObjectID, BodyState> &E : body_map) {
		Object *obj = ObjectDB::get_instance(E.key);
		if (obj) {
			ret[idx] = obj;
			idx++;
		}
	}

	ret.resize(idx);
	return ret;
}

TypedArray<Area2D> Area2D::get_overlapping_areas() const {
	TypedArray<Area2D> ret;
	ERR_FAIL_COND_V_MSG(!monitoring, ret, "Can't find overlapping areas when monitoring is off.");
	ret.resize(area_map.size());
	int idx = 0;
	for (const KeyValue<ObjectID, AreaState> &E : area_map) {
		Object *obj = ObjectDB::get_instance(E.key);
		if (obj) {
			ret[idx] = obj;
			idx++;
		}
	}

	ret.resize(idx);
	return ret;
}

bool Area2D::has_overlapping_bodies() const {
	ERR_FAIL_COND_V_MSG(!monitoring, false, "Can't find overlapping bodies when monitoring is off.");
	return !body_map.is_empty();
}

bool Area2D::has_overlapping_areas() const {
	ERR_FAIL_COND_V_MSG(!monitoring, false, "Can't find overlapping areas when monitoring is off.");
	return !area_map.is_empty();
}

bool Area2D::overlaps_area(Node *p_area) const {
	ERR_FAIL_NULL_V(p_area, false);
	HashMap<ObjectID, AreaState>::ConstIterator E = area_map.find(p_area->get_instance_id());
	if (!E) {
		return false;
	}
	return E->value.in_tree;
}

bool Area2D::overlaps_body(Node *p_body) const {
	ERR_FAIL_NULL_V(p_body, false);
	HashMap<ObjectID, BodyState>::ConstIterator E = body_map.find(p_body->get_instance_id());
	if (!E) {
		return false;
	}
	return E->value.in_tree;
}

void Area2D::set_audio_bus_override(bool p_override) {
	audio_bus_override = p_override;
}

bool Area2D::is_overriding_audio_bus() const {
	return audio_bus_override;
}

void Area2D::set_audio_bus_name(const StringName &p_audio_bus) {
	audio_bus = p_audio_bus;
}

StringName Area2D::get_audio_bus_name() const {
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		if (AudioServer::get_singleton()->get_bus_name(i) == audio_bus) {
			return audio_bus;
		}
	}
	return SceneStringName(Master);
}

Vector2 Area2D::calculate_gravity_target(const Vector2 &p_local_position) {
	Vector2 ret;
	GDVIRTUAL_CALL(_calculate_gravity_target, p_local_position, ret);
	return ret;
}

void Area2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "audio_bus_name") {
		String options;
		for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
			if (i > 0) {
				options += ",";
			}
			String name = AudioServer::get_singleton()->get_bus_name(i);
			options += name;
		}

		p_property.hint_string = options;
	} else if (p_property.name.begins_with("gravity") && p_property.name != "gravity_space_override") {
		if (gravity_space_override == SPACE_OVERRIDE_DISABLED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		} else {
			if (gravity_type == GRAVITY_TYPE_DIRECTIONAL) {
				if (p_property.name == "gravity_point_unit_distance") {
					p_property.usage = PROPERTY_USAGE_NO_EDITOR;
				}
			} else {
				if (p_property.name == "gravity_direction") {
					p_property.usage = PROPERTY_USAGE_NO_EDITOR;
				}
			}
			if (gravity_type != GRAVITY_TYPE_POINT) {
				if (p_property.name == "gravity_point_center") {
					p_property.usage = PROPERTY_USAGE_NO_EDITOR;
				}
			}
		}
	} else if (p_property.name.begins_with("linear_damp") && p_property.name != "linear_damp_space_override") {
		if (linear_damp_space_override == SPACE_OVERRIDE_DISABLED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	} else if (p_property.name.begins_with("angular_damp") && p_property.name != "angular_damp_space_override") {
		if (angular_damp_space_override == SPACE_OVERRIDE_DISABLED) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}
}

void Area2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_gravity_space_override_mode", "space_override_mode"), &Area2D::set_gravity_space_override_mode);
	ClassDB::bind_method(D_METHOD("get_gravity_space_override_mode"), &Area2D::get_gravity_space_override_mode);

	ClassDB::bind_method(D_METHOD("set_gravity_is_point", "enable"), &Area2D::set_gravity_is_point);
	ClassDB::bind_method(D_METHOD("is_gravity_a_point"), &Area2D::is_gravity_a_point);

	ClassDB::bind_method(D_METHOD("set_gravity_type", "enable"), &Area2D::set_gravity_type);
	ClassDB::bind_method(D_METHOD("get_gravity_type"), &Area2D::get_gravity_type);

	ClassDB::bind_method(D_METHOD("set_gravity_point_unit_distance", "distance_scale"), &Area2D::set_gravity_point_unit_distance);
	ClassDB::bind_method(D_METHOD("get_gravity_point_unit_distance"), &Area2D::get_gravity_point_unit_distance);

	ClassDB::bind_method(D_METHOD("set_gravity_point_center", "center"), &Area2D::set_gravity_point_center);
	ClassDB::bind_method(D_METHOD("get_gravity_point_center"), &Area2D::get_gravity_point_center);

	ClassDB::bind_method(D_METHOD("set_gravity_direction", "direction"), &Area2D::set_gravity_direction);
	ClassDB::bind_method(D_METHOD("get_gravity_direction"), &Area2D::get_gravity_direction);

	ClassDB::bind_method(D_METHOD("set_gravity", "gravity"), &Area2D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &Area2D::get_gravity);

	ClassDB::bind_method(D_METHOD("set_linear_damp_space_override_mode", "space_override_mode"), &Area2D::set_linear_damp_space_override_mode);
	ClassDB::bind_method(D_METHOD("get_linear_damp_space_override_mode"), &Area2D::get_linear_damp_space_override_mode);

	ClassDB::bind_method(D_METHOD("set_angular_damp_space_override_mode", "space_override_mode"), &Area2D::set_angular_damp_space_override_mode);
	ClassDB::bind_method(D_METHOD("get_angular_damp_space_override_mode"), &Area2D::get_angular_damp_space_override_mode);

	ClassDB::bind_method(D_METHOD("set_linear_damp", "linear_damp"), &Area2D::set_linear_damp);
	ClassDB::bind_method(D_METHOD("get_linear_damp"), &Area2D::get_linear_damp);

	ClassDB::bind_method(D_METHOD("set_angular_damp", "angular_damp"), &Area2D::set_angular_damp);
	ClassDB::bind_method(D_METHOD("get_angular_damp"), &Area2D::get_angular_damp);

	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &Area2D::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &Area2D::get_priority);

	ClassDB::bind_method(D_METHOD("set_monitoring", "enable"), &Area2D::set_monitoring);
	ClassDB::bind_method(D_METHOD("is_monitoring"), &Area2D::is_monitoring);

	ClassDB::bind_method(D_METHOD("set_monitorable", "enable"), &Area2D::set_monitorable);
	ClassDB::bind_method(D_METHOD("is_monitorable"), &Area2D::is_monitorable);

	ClassDB::bind_method(D_METHOD("get_overlapping_bodies"), &Area2D::get_overlapping_bodies);
	ClassDB::bind_method(D_METHOD("get_overlapping_areas"), &Area2D::get_overlapping_areas);

	ClassDB::bind_method(D_METHOD("has_overlapping_bodies"), &Area2D::has_overlapping_bodies);
	ClassDB::bind_method(D_METHOD("has_overlapping_areas"), &Area2D::has_overlapping_areas);

	ClassDB::bind_method(D_METHOD("overlaps_body", "body"), &Area2D::overlaps_body);
	ClassDB::bind_method(D_METHOD("overlaps_area", "area"), &Area2D::overlaps_area);

	ClassDB::bind_method(D_METHOD("set_audio_bus_name", "name"), &Area2D::set_audio_bus_name);
	ClassDB::bind_method(D_METHOD("get_audio_bus_name"), &Area2D::get_audio_bus_name);

	ClassDB::bind_method(D_METHOD("set_audio_bus_override", "enable"), &Area2D::set_audio_bus_override);
	ClassDB::bind_method(D_METHOD("is_overriding_audio_bus"), &Area2D::is_overriding_audio_bus);

	GDVIRTUAL_BIND(_calculate_gravity_target, "local_position");

	ADD_SIGNAL(MethodInfo("body_shape_entered", PropertyInfo(Variant::RID, "body_rid"), PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node2D"), PropertyInfo(Variant::INT, "body_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("body_shape_exited", PropertyInfo(Variant::RID, "body_rid"), PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node2D"), PropertyInfo(Variant::INT, "body_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("body_entered", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node2D")));
	ADD_SIGNAL(MethodInfo("body_exited", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "Node2D")));

	ADD_SIGNAL(MethodInfo("area_shape_entered", PropertyInfo(Variant::RID, "area_rid"), PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area2D"), PropertyInfo(Variant::INT, "area_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("area_shape_exited", PropertyInfo(Variant::RID, "area_rid"), PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area2D"), PropertyInfo(Variant::INT, "area_shape_index"), PropertyInfo(Variant::INT, "local_shape_index")));
	ADD_SIGNAL(MethodInfo("area_entered", PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area2D")));
	ADD_SIGNAL(MethodInfo("area_exited", PropertyInfo(Variant::OBJECT, "area", PROPERTY_HINT_RESOURCE_TYPE, "Area2D")));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "monitoring"), "set_monitoring", "is_monitoring");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "monitorable"), "set_monitorable", "is_monitorable");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority", PROPERTY_HINT_RANGE, "0,100000,1,or_greater,or_less"), "set_priority", "get_priority");

	ADD_GROUP("Gravity", "gravity_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gravity_space_override", PROPERTY_HINT_ENUM, "Disabled,Combine,Combine-Replace,Replace,Replace-Combine", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_gravity_space_override_mode", "get_gravity_space_override_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gravity_type", PROPERTY_HINT_ENUM, "Directional,Point,Target", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_gravity_type", "get_gravity_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gravity_point", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_gravity_is_point", "is_gravity_a_point");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_point_unit_distance", PROPERTY_HINT_RANGE, "0,1024,0.001,or_greater,exp,suffix:px"), "set_gravity_point_unit_distance", "get_gravity_point_unit_distance");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "gravity_point_center", PROPERTY_HINT_NONE, "suffix:px"), "set_gravity_point_center", "get_gravity_point_center");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "gravity_direction"), "set_gravity_direction", "get_gravity_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity", PROPERTY_HINT_RANGE, U"-4096,4096,0.001,or_less,or_greater,suffix:px/s\u00B2"), "set_gravity", "get_gravity");

	ADD_GROUP("Linear Damp", "linear_damp_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "linear_damp_space_override", PROPERTY_HINT_ENUM, "Disabled,Combine,Combine-Replace,Replace,Replace-Combine", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_linear_damp_space_override_mode", "get_linear_damp_space_override_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_linear_damp", "get_linear_damp");

	ADD_GROUP("Angular Damp", "angular_damp_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "angular_damp_space_override", PROPERTY_HINT_ENUM, "Disabled,Combine,Combine-Replace,Replace,Replace-Combine", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_angular_damp_space_override_mode", "get_angular_damp_space_override_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damp", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_angular_damp", "get_angular_damp");

	ADD_GROUP("Audio Bus", "audio_bus_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "audio_bus_override"), "set_audio_bus_override", "is_overriding_audio_bus");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "audio_bus_name", PROPERTY_HINT_ENUM, ""), "set_audio_bus_name", "get_audio_bus_name");

	BIND_ENUM_CONSTANT(SPACE_OVERRIDE_DISABLED);
	BIND_ENUM_CONSTANT(SPACE_OVERRIDE_COMBINE);
	BIND_ENUM_CONSTANT(SPACE_OVERRIDE_COMBINE_REPLACE);
	BIND_ENUM_CONSTANT(SPACE_OVERRIDE_REPLACE);
	BIND_ENUM_CONSTANT(SPACE_OVERRIDE_REPLACE_COMBINE);

	BIND_ENUM_CONSTANT(GRAVITY_TYPE_DIRECTIONAL);
	BIND_ENUM_CONSTANT(GRAVITY_TYPE_POINT);
	BIND_ENUM_CONSTANT(GRAVITY_TYPE_TARGET);
}

Area2D::Area2D() :
		CollisionObject2D(PhysicsServer2D::get_singleton()->area_create(), true) {
	set_gravity(980);
	set_gravity_direction(Vector2(0, 1));
	set_monitoring(true);
	set_monitorable(true);
	set_hide_clip_children(true);
}

Area2D::~Area2D() {
}
