/**************************************************************************/
/*  shape_cast_3d.cpp                                                     */
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

#include "shape_cast_3d.h"

#include "scene/3d/physics/collision_object_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"

void ShapeCast3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				_update_debug_shape_vertices();
			}
			if (enabled && !Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			} else {
				set_physics_process_internal(false);
			}

			if (get_tree()->is_debugging_collisions_hint()) {
				_update_debug_shape();
			}

			if (Object::cast_to<CollisionObject3D>(get_parent())) {
				if (exclude_parent_body) {
					exclude.insert(Object::cast_to<CollisionObject3D>(get_parent())->get_rid());
				} else {
					exclude.erase(Object::cast_to<CollisionObject3D>(get_parent())->get_rid());
				}
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (enabled) {
				set_physics_process_internal(false);
			}

			if (debug_instance.is_valid()) {
				_clear_debug_shape();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_inside_tree() && debug_instance.is_valid()) {
				RenderingServer::get_singleton()->instance_set_visible(debug_instance, is_visible_in_tree());
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!enabled) {
				break;
			}

			bool prev_collision_state = collided;
			_update_shapecast_state();
			if (get_tree()->is_debugging_collisions_hint()) {
				if (prev_collision_state != collided) {
					_update_debug_shape_material(true);
				}
				if (collided) {
					_update_debug_shape();
				}
				if (prev_collision_state == collided && !collided) {
					_update_debug_shape();
				}
				if (is_inside_tree() && debug_instance.is_valid()) {
					RenderingServer::get_singleton()->instance_set_transform(debug_instance, get_global_transform());
				}
			}
		} break;
	}
}

void ShapeCast3D::_bind_methods() {
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("resource_changed", "resource"), &ShapeCast3D::resource_changed);
#endif

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &ShapeCast3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &ShapeCast3D::is_enabled);

	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &ShapeCast3D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &ShapeCast3D::get_shape);

	ClassDB::bind_method(D_METHOD("set_target_position", "local_point"), &ShapeCast3D::set_target_position);
	ClassDB::bind_method(D_METHOD("get_target_position"), &ShapeCast3D::get_target_position);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &ShapeCast3D::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &ShapeCast3D::get_margin);

	ClassDB::bind_method(D_METHOD("set_max_results", "max_results"), &ShapeCast3D::set_max_results);
	ClassDB::bind_method(D_METHOD("get_max_results"), &ShapeCast3D::get_max_results);

	ClassDB::bind_method(D_METHOD("is_colliding"), &ShapeCast3D::is_colliding);
	ClassDB::bind_method(D_METHOD("get_collision_count"), &ShapeCast3D::get_collision_count);

	ClassDB::bind_method(D_METHOD("force_shapecast_update"), &ShapeCast3D::force_shapecast_update);

	ClassDB::bind_method(D_METHOD("get_collider", "index"), &ShapeCast3D::get_collider);
	ClassDB::bind_method(D_METHOD("get_collider_rid", "index"), &ShapeCast3D::get_collider_rid);
	ClassDB::bind_method(D_METHOD("get_collider_shape", "index"), &ShapeCast3D::get_collider_shape);
	ClassDB::bind_method(D_METHOD("get_collision_point", "index"), &ShapeCast3D::get_collision_point);
	ClassDB::bind_method(D_METHOD("get_collision_normal", "index"), &ShapeCast3D::get_collision_normal);

	ClassDB::bind_method(D_METHOD("get_closest_collision_safe_fraction"), &ShapeCast3D::get_closest_collision_safe_fraction);
	ClassDB::bind_method(D_METHOD("get_closest_collision_unsafe_fraction"), &ShapeCast3D::get_closest_collision_unsafe_fraction);

	ClassDB::bind_method(D_METHOD("add_exception_rid", "rid"), &ShapeCast3D::add_exception_rid);
	ClassDB::bind_method(D_METHOD("add_exception", "node"), &ShapeCast3D::add_exception);

	ClassDB::bind_method(D_METHOD("remove_exception_rid", "rid"), &ShapeCast3D::remove_exception_rid);
	ClassDB::bind_method(D_METHOD("remove_exception", "node"), &ShapeCast3D::remove_exception);

	ClassDB::bind_method(D_METHOD("clear_exceptions"), &ShapeCast3D::clear_exceptions);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &ShapeCast3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &ShapeCast3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_mask_value", "layer_number", "value"), &ShapeCast3D::set_collision_mask_value);
	ClassDB::bind_method(D_METHOD("get_collision_mask_value", "layer_number"), &ShapeCast3D::get_collision_mask_value);

	ClassDB::bind_method(D_METHOD("set_exclude_parent_body", "mask"), &ShapeCast3D::set_exclude_parent_body);
	ClassDB::bind_method(D_METHOD("get_exclude_parent_body"), &ShapeCast3D::get_exclude_parent_body);

	ClassDB::bind_method(D_METHOD("set_collide_with_areas", "enable"), &ShapeCast3D::set_collide_with_areas);
	ClassDB::bind_method(D_METHOD("is_collide_with_areas_enabled"), &ShapeCast3D::is_collide_with_areas_enabled);

	ClassDB::bind_method(D_METHOD("set_collide_with_bodies", "enable"), &ShapeCast3D::set_collide_with_bodies);
	ClassDB::bind_method(D_METHOD("is_collide_with_bodies_enabled"), &ShapeCast3D::is_collide_with_bodies_enabled);

	ClassDB::bind_method(D_METHOD("get_collision_result"), &ShapeCast3D::get_collision_result);

	ClassDB::bind_method(D_METHOD("set_debug_shape_custom_color", "debug_shape_custom_color"), &ShapeCast3D::set_debug_shape_custom_color);
	ClassDB::bind_method(D_METHOD("get_debug_shape_custom_color"), &ShapeCast3D::get_debug_shape_custom_color);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "exclude_parent"), "set_exclude_parent_body", "get_exclude_parent_body");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_position", PROPERTY_HINT_NONE, "suffix:m"), "set_target_position", "get_target_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0,100,0.01,suffix:m"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_results"), "set_max_results", "get_max_results");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "collision_result", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "", "get_collision_result");

	ADD_GROUP("Collide With", "collide_with");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_areas", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collide_with_areas", "is_collide_with_areas_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_bodies", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collide_with_bodies", "is_collide_with_bodies_enabled");

	ADD_GROUP("Debug Shape", "debug_shape");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_shape_custom_color"), "set_debug_shape_custom_color", "get_debug_shape_custom_color");
}

PackedStringArray ShapeCast3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (shape.is_null()) {
		warnings.push_back(RTR("This node cannot interact with other objects unless a Shape3D is assigned."));
	}
	if (shape.is_valid() && Object::cast_to<ConcavePolygonShape3D>(*shape)) {
		warnings.push_back(RTR("ShapeCast3D does not support ConcavePolygonShape3Ds. Collisions will not be reported."));
	}
	return warnings;
}

void ShapeCast3D::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	update_gizmos();

	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		set_physics_process_internal(p_enabled);
	}
	if (!p_enabled) {
		collided = false;
	}

	if (is_inside_tree() && get_tree()->is_debugging_collisions_hint()) {
		if (p_enabled) {
			_update_debug_shape();
		} else {
			_clear_debug_shape();
		}
	}
}

bool ShapeCast3D::is_enabled() const {
	return enabled;
}

void ShapeCast3D::set_target_position(const Vector3 &p_point) {
	target_position = p_point;
	if (is_inside_tree() && get_tree()->is_debugging_collisions_hint()) {
		_update_debug_shape();
	}
	update_gizmos();

	if (Engine::get_singleton()->is_editor_hint()) {
		if (is_inside_tree()) {
			_update_debug_shape_vertices();
		}
	} else if (debug_instance.is_valid()) {
		_update_debug_shape();
	}
}

Vector3 ShapeCast3D::get_target_position() const {
	return target_position;
}

void ShapeCast3D::set_margin(real_t p_margin) {
	margin = p_margin;
}

real_t ShapeCast3D::get_margin() const {
	return margin;
}

void ShapeCast3D::set_max_results(int p_max_results) {
	max_results = p_max_results;
}

int ShapeCast3D::get_max_results() const {
	return max_results;
}

void ShapeCast3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
}

uint32_t ShapeCast3D::get_collision_mask() const {
	return collision_mask;
}

void ShapeCast3D::set_collision_mask_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t mask = get_collision_mask();
	if (p_value) {
		mask |= 1 << (p_layer_number - 1);
	} else {
		mask &= ~(1 << (p_layer_number - 1));
	}
	set_collision_mask(mask);
}

bool ShapeCast3D::get_collision_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_mask() & (1 << (p_layer_number - 1));
}

int ShapeCast3D::get_collision_count() const {
	return result.size();
}

bool ShapeCast3D::is_colliding() const {
	return collided;
}

Object *ShapeCast3D::get_collider(int p_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_idx, result.size(), nullptr, "No collider found.");

	if (result[p_idx].collider_id.is_null()) {
		return nullptr;
	}
	return ObjectDB::get_instance(result[p_idx].collider_id);
}

RID ShapeCast3D::get_collider_rid(int p_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_idx, result.size(), RID(), "No collider RID found.");
	return result[p_idx].rid;
}

int ShapeCast3D::get_collider_shape(int p_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_idx, result.size(), -1, "No collider shape found.");
	return result[p_idx].shape;
}

Vector3 ShapeCast3D::get_collision_point(int p_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_idx, result.size(), Vector3(), "No collision point found.");
	return result[p_idx].point;
}

Vector3 ShapeCast3D::get_collision_normal(int p_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_idx, result.size(), Vector3(), "No collision normal found.");
	return result[p_idx].normal;
}

real_t ShapeCast3D::get_closest_collision_safe_fraction() const {
	return collision_safe_fraction;
}

real_t ShapeCast3D::get_closest_collision_unsafe_fraction() const {
	return collision_unsafe_fraction;
}

#ifndef DISABLE_DEPRECATED
void ShapeCast3D::resource_changed(Ref<Resource> p_res) {
}
#endif

void ShapeCast3D::_shape_changed() {
	update_gizmos();
	bool is_editor = Engine::get_singleton()->is_editor_hint();
	if (is_inside_tree() && (is_editor || get_tree()->is_debugging_collisions_hint())) {
		_update_debug_shape();
	}
}

void ShapeCast3D::set_shape(const Ref<Shape3D> &p_shape) {
	if (p_shape == shape) {
		return;
	}
	if (shape.is_valid()) {
		shape->disconnect_changed(callable_mp(this, &ShapeCast3D::_shape_changed));
	}
	shape = p_shape;
	if (shape.is_valid()) {
		shape->connect_changed(callable_mp(this, &ShapeCast3D::_shape_changed));
		shape_rid = shape->get_rid();
	}

	bool is_editor = Engine::get_singleton()->is_editor_hint();
	if (is_inside_tree() && (is_editor || get_tree()->is_debugging_collisions_hint())) {
		_update_debug_shape();
	}
	update_gizmos();
	update_configuration_warnings();
}

Ref<Shape3D> ShapeCast3D::get_shape() const {
	return shape;
}

void ShapeCast3D::set_exclude_parent_body(bool p_exclude_parent_body) {
	if (exclude_parent_body == p_exclude_parent_body) {
		return;
	}
	exclude_parent_body = p_exclude_parent_body;

	if (!is_inside_tree()) {
		return;
	}
	if (Object::cast_to<CollisionObject3D>(get_parent())) {
		if (exclude_parent_body) {
			exclude.insert(Object::cast_to<CollisionObject3D>(get_parent())->get_rid());
		} else {
			exclude.erase(Object::cast_to<CollisionObject3D>(get_parent())->get_rid());
		}
	}
}

bool ShapeCast3D::get_exclude_parent_body() const {
	return exclude_parent_body;
}

void ShapeCast3D::_update_shapecast_state() {
	result.clear();

	ERR_FAIL_COND_MSG(shape.is_null(), "Null reference to shape. ShapeCast3D requires a Shape3D to sweep for collisions.");

	Ref<World3D> w3d = get_world_3d();
	ERR_FAIL_COND(w3d.is_null());

	PhysicsDirectSpaceState3D *dss = PhysicsServer3D::get_singleton()->space_get_direct_state(w3d->get_space());
	ERR_FAIL_NULL(dss);

	Transform3D gt = get_global_transform();

	PhysicsDirectSpaceState3D::ShapeParameters params;
	params.shape_rid = shape_rid;
	params.transform = gt;
	params.motion = gt.basis.xform(target_position);
	params.margin = margin;
	params.exclude = exclude;
	params.collision_mask = collision_mask;
	params.collide_with_bodies = collide_with_bodies;
	params.collide_with_areas = collide_with_areas;

	collision_safe_fraction = 0.0;
	collision_unsafe_fraction = 0.0;

	if (target_position != Vector3()) {
		dss->cast_motion(params, collision_safe_fraction, collision_unsafe_fraction);
		if (collision_unsafe_fraction < 1.0) {
			// Move shape transform to the point of impact,
			// so we can collect contact info at that point.
			gt.set_origin(gt.get_origin() + params.motion * (collision_unsafe_fraction + CMP_EPSILON));
			params.transform = gt;
		}
	}
	// Regardless of whether the shape is stuck or it's moved along
	// the motion vector, we'll only consider static collisions from now on.
	params.motion = Vector3();

	bool intersected = true;
	while (intersected && result.size() < max_results) {
		PhysicsDirectSpaceState3D::ShapeRestInfo info;
		intersected = dss->rest_info(params, &info);
		if (intersected) {
			result.push_back(info);
			params.exclude.insert(info.rid);
		}
	}
	collided = !result.is_empty();
}

void ShapeCast3D::force_shapecast_update() {
	_update_shapecast_state();
}

void ShapeCast3D::add_exception_rid(const RID &p_rid) {
	exclude.insert(p_rid);
}

void ShapeCast3D::add_exception(const CollisionObject3D *p_node) {
	ERR_FAIL_NULL_MSG(p_node, "The passed Node must be an instance of CollisionObject3D.");
	add_exception_rid(p_node->get_rid());
}

void ShapeCast3D::remove_exception_rid(const RID &p_rid) {
	exclude.erase(p_rid);
}

void ShapeCast3D::remove_exception(const CollisionObject3D *p_node) {
	ERR_FAIL_NULL_MSG(p_node, "The passed Node must be an instance of CollisionObject3D.");
	remove_exception_rid(p_node->get_rid());
}

void ShapeCast3D::clear_exceptions() {
	exclude.clear();
}

void ShapeCast3D::set_collide_with_areas(bool p_clip) {
	collide_with_areas = p_clip;
}

bool ShapeCast3D::is_collide_with_areas_enabled() const {
	return collide_with_areas;
}

void ShapeCast3D::set_collide_with_bodies(bool p_clip) {
	collide_with_bodies = p_clip;
}

bool ShapeCast3D::is_collide_with_bodies_enabled() const {
	return collide_with_bodies;
}

Array ShapeCast3D::get_collision_result() const {
	Array ret;

	for (int i = 0; i < result.size(); ++i) {
		const PhysicsDirectSpaceState3D::ShapeRestInfo &sri = result[i];

		Dictionary col;
		col["point"] = sri.point;
		col["normal"] = sri.normal;
		col["rid"] = sri.rid;
		col["collider"] = ObjectDB::get_instance(sri.collider_id);
		col["collider_id"] = sri.collider_id;
		col["shape"] = sri.shape;
		col["linear_velocity"] = sri.linear_velocity;

		ret.push_back(col);
	}
	return ret;
}

void ShapeCast3D::_update_debug_shape_vertices() {
	debug_shape_vertices.clear();
	debug_line_vertices.clear();

	if (shape.is_valid()) {
		debug_shape_vertices.append_array(shape->get_debug_mesh_lines());
		for (int i = 0; i < debug_shape_vertices.size(); i++) {
			debug_shape_vertices.set(i, debug_shape_vertices[i] + Vector3(target_position * get_closest_collision_safe_fraction()));
		}
	}

	if (target_position == Vector3()) {
		return;
	}

	debug_line_vertices.push_back(Vector3());
	debug_line_vertices.push_back(target_position);
}

const Vector<Vector3> &ShapeCast3D::get_debug_shape_vertices() const {
	return debug_shape_vertices;
}

const Vector<Vector3> &ShapeCast3D::get_debug_line_vertices() const {
	return debug_line_vertices;
}

void ShapeCast3D::set_debug_shape_custom_color(const Color &p_color) {
	debug_shape_custom_color = p_color;
	if (debug_material.is_valid()) {
		_update_debug_shape_material();
	}
}

Ref<StandardMaterial3D> ShapeCast3D::get_debug_material() {
	_update_debug_shape_material();
	return debug_material;
}

const Color &ShapeCast3D::get_debug_shape_custom_color() const {
	return debug_shape_custom_color;
}

void ShapeCast3D::_create_debug_shape() {
	_update_debug_shape_material();

	if (!debug_instance.is_valid()) {
		debug_instance = RenderingServer::get_singleton()->instance_create();
	}

	if (debug_mesh.is_null()) {
		debug_mesh.instantiate();
	}
}

void ShapeCast3D::_update_debug_shape_material(bool p_check_collision) {
	if (debug_material.is_null()) {
		Ref<StandardMaterial3D> material = memnew(StandardMaterial3D);
		debug_material = material;

		material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		// Use double-sided rendering so that the RayCast can be seen if the camera is inside.
		material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	}

	Color color = debug_shape_custom_color;
	if (color == Color(0.0, 0.0, 0.0)) {
		// Use the default debug shape color defined in the Project Settings.
		color = get_tree()->get_debug_collisions_color();
	}

	if (p_check_collision && collided) {
		if ((color.get_h() < 0.055 || color.get_h() > 0.945) && color.get_s() > 0.5 && color.get_v() > 0.5) {
			// If base color is already quite reddish, highlight collision with green color
			color = Color(0.0, 1.0, 0.0, color.a);
		} else {
			// Else, highlight collision with red color
			color = Color(1.0, 0, 0, color.a);
		}
	}

	Ref<StandardMaterial3D> material = static_cast<Ref<StandardMaterial3D>>(debug_material);
	material->set_albedo(color);
}

void ShapeCast3D::_update_debug_shape() {
	if (!enabled) {
		return;
	}

	if (!debug_instance.is_valid()) {
		_create_debug_shape();
	}

	_update_debug_shape_vertices();

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!debug_instance.is_valid() || debug_mesh.is_null()) {
		return;
	}

	debug_mesh->clear_surfaces();

	Array a;
	a.resize(Mesh::ARRAY_MAX);

	uint32_t flags = 0;
	int surface_count = 0;

	if (!debug_shape_vertices.is_empty()) {
		a[Mesh::ARRAY_VERTEX] = debug_shape_vertices;
		debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, a, Array(), Dictionary(), flags);
		debug_mesh->surface_set_material(surface_count, debug_material);
		++surface_count;
	}

	if (!debug_line_vertices.is_empty()) {
		a[Mesh::ARRAY_VERTEX] = debug_line_vertices;
		debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, a, Array(), Dictionary(), flags);
		debug_mesh->surface_set_material(surface_count, debug_material);
		++surface_count;
	}

	RenderingServer::get_singleton()->instance_set_base(debug_instance, debug_mesh->get_rid());
	if (is_inside_tree()) {
		RenderingServer::get_singleton()->instance_set_scenario(debug_instance, get_world_3d()->get_scenario());
		RenderingServer::get_singleton()->instance_set_visible(debug_instance, is_visible_in_tree());
		RenderingServer::get_singleton()->instance_set_transform(debug_instance, get_global_transform());
	}
}

void ShapeCast3D::_clear_debug_shape() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (debug_instance.is_valid()) {
		RenderingServer::get_singleton()->free(debug_instance);
		debug_instance = RID();
	}
	if (debug_mesh.is_valid()) {
		RenderingServer::get_singleton()->free(debug_mesh->get_rid());
		debug_mesh = Ref<ArrayMesh>();
	}
}
