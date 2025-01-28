/**************************************************************************/
/*  static_body_2d.cpp                                                    */
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

#include "static_body_2d.h"

#include "scene/resources/2d/capsule_shape_2d.h"
#include "scene/resources/2d/circle_shape_2d.h"
#include "scene/resources/2d/concave_polygon_shape_2d.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"
#include "scene/resources/2d/navigation_mesh_source_geometry_data_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "servers/navigation_server_2d.h"

Callable StaticBody2D::_navmesh_source_geometry_parsing_callback;
RID StaticBody2D::_navmesh_source_geometry_parser;

void StaticBody2D::set_constant_linear_velocity(const Vector2 &p_vel) {
	constant_linear_velocity = p_vel;

	PhysicsServer2D::get_singleton()->body_set_state(get_rid(), PhysicsServer2D::BODY_STATE_LINEAR_VELOCITY, constant_linear_velocity);
}

void StaticBody2D::set_constant_angular_velocity(real_t p_vel) {
	constant_angular_velocity = p_vel;

	PhysicsServer2D::get_singleton()->body_set_state(get_rid(), PhysicsServer2D::BODY_STATE_ANGULAR_VELOCITY, constant_angular_velocity);
}

Vector2 StaticBody2D::get_constant_linear_velocity() const {
	return constant_linear_velocity;
}

real_t StaticBody2D::get_constant_angular_velocity() const {
	return constant_angular_velocity;
}

void StaticBody2D::set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override) {
	if (physics_material_override.is_valid()) {
		physics_material_override->disconnect_changed(callable_mp(this, &StaticBody2D::_reload_physics_characteristics));
	}

	physics_material_override = p_physics_material_override;

	if (physics_material_override.is_valid()) {
		physics_material_override->connect_changed(callable_mp(this, &StaticBody2D::_reload_physics_characteristics));
	}
	_reload_physics_characteristics();
}

Ref<PhysicsMaterial> StaticBody2D::get_physics_material_override() const {
	return physics_material_override;
}

void StaticBody2D::_reload_physics_characteristics() {
	if (physics_material_override.is_null()) {
		PhysicsServer2D::get_singleton()->body_set_param(get_rid(), PhysicsServer2D::BODY_PARAM_BOUNCE, 0);
		PhysicsServer2D::get_singleton()->body_set_param(get_rid(), PhysicsServer2D::BODY_PARAM_FRICTION, 1);
	} else {
		PhysicsServer2D::get_singleton()->body_set_param(get_rid(), PhysicsServer2D::BODY_PARAM_BOUNCE, physics_material_override->computed_bounce());
		PhysicsServer2D::get_singleton()->body_set_param(get_rid(), PhysicsServer2D::BODY_PARAM_FRICTION, physics_material_override->computed_friction());
	}
}

void StaticBody2D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&StaticBody2D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer2D::get_singleton()->source_geometry_parser_create();
		NavigationServer2D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void StaticBody2D::navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node) {
	StaticBody2D *static_body = Object::cast_to<StaticBody2D>(p_node);

	if (static_body == nullptr) {
		return;
	}

	NavigationPolygon::ParsedGeometryType parsed_geometry_type = p_navigation_mesh->get_parsed_geometry_type();
	if (!(parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_STATIC_COLLIDERS || parsed_geometry_type == NavigationPolygon::PARSED_GEOMETRY_BOTH)) {
		return;
	}

	uint32_t parsed_collision_mask = p_navigation_mesh->get_parsed_collision_mask();
	if (!(static_body->get_collision_layer() & parsed_collision_mask)) {
		return;
	}

	List<uint32_t> shape_owners;
	static_body->get_shape_owners(&shape_owners);

	for (uint32_t shape_owner : shape_owners) {
		if (static_body->is_shape_owner_disabled(shape_owner)) {
			continue;
		}

		const int shape_count = static_body->shape_owner_get_shape_count(shape_owner);

		for (int shape_index = 0; shape_index < shape_count; shape_index++) {
			Ref<Shape2D> s = static_body->shape_owner_get_shape(shape_owner, shape_index);

			if (s.is_null()) {
				continue;
			}

			const Transform2D static_body_xform = p_source_geometry_data->root_node_transform * static_body->get_global_transform() * static_body->shape_owner_get_transform(shape_owner);

			RectangleShape2D *rectangle_shape = Object::cast_to<RectangleShape2D>(*s);
			if (rectangle_shape) {
				Vector<Vector2> shape_outline;

				const Vector2 &rectangle_size = rectangle_shape->get_size();

				shape_outline.resize(5);
				shape_outline.write[0] = static_body_xform.xform(-rectangle_size * 0.5);
				shape_outline.write[1] = static_body_xform.xform(Vector2(rectangle_size.x, -rectangle_size.y) * 0.5);
				shape_outline.write[2] = static_body_xform.xform(rectangle_size * 0.5);
				shape_outline.write[3] = static_body_xform.xform(Vector2(-rectangle_size.x, rectangle_size.y) * 0.5);
				shape_outline.write[4] = static_body_xform.xform(-rectangle_size * 0.5);

				p_source_geometry_data->add_obstruction_outline(shape_outline);
			}

			CapsuleShape2D *capsule_shape = Object::cast_to<CapsuleShape2D>(*s);
			if (capsule_shape) {
				const real_t capsule_height = capsule_shape->get_height();
				const real_t capsule_radius = capsule_shape->get_radius();

				Vector<Vector2> shape_outline;
				const real_t turn_step = Math_TAU / 12.0;
				shape_outline.resize(14);
				int shape_outline_inx = 0;
				for (int i = 0; i < 12; i++) {
					Vector2 ofs = Vector2(0, (i > 3 && i <= 9) ? -capsule_height * 0.5 + capsule_radius : capsule_height * 0.5 - capsule_radius);

					shape_outline.write[shape_outline_inx] = static_body_xform.xform(Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * capsule_radius + ofs);
					shape_outline_inx += 1;
					if (i == 3 || i == 9) {
						shape_outline.write[shape_outline_inx] = static_body_xform.xform(Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * capsule_radius - ofs);
						shape_outline_inx += 1;
					}
				}

				p_source_geometry_data->add_obstruction_outline(shape_outline);
			}

			CircleShape2D *circle_shape = Object::cast_to<CircleShape2D>(*s);
			if (circle_shape) {
				const real_t circle_radius = circle_shape->get_radius();

				Vector<Vector2> shape_outline;
				int circle_edge_count = 12;
				shape_outline.resize(circle_edge_count);

				const real_t turn_step = Math_TAU / real_t(circle_edge_count);
				for (int i = 0; i < circle_edge_count; i++) {
					shape_outline.write[i] = static_body_xform.xform(Vector2(Math::cos(i * turn_step), Math::sin(i * turn_step)) * circle_radius);
				}

				p_source_geometry_data->add_obstruction_outline(shape_outline);
			}

			ConcavePolygonShape2D *concave_polygon_shape = Object::cast_to<ConcavePolygonShape2D>(*s);
			if (concave_polygon_shape) {
				Vector<Vector2> shape_outline = concave_polygon_shape->get_segments();

				for (int i = 0; i < shape_outline.size(); i++) {
					shape_outline.write[i] = static_body_xform.xform(shape_outline[i]);
				}

				p_source_geometry_data->add_obstruction_outline(shape_outline);
			}

			ConvexPolygonShape2D *convex_polygon_shape = Object::cast_to<ConvexPolygonShape2D>(*s);
			if (convex_polygon_shape) {
				Vector<Vector2> shape_outline = convex_polygon_shape->get_points();

				for (int i = 0; i < shape_outline.size(); i++) {
					shape_outline.write[i] = static_body_xform.xform(shape_outline[i]);
				}

				p_source_geometry_data->add_obstruction_outline(shape_outline);
			}
		}
	}
}

void StaticBody2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant_linear_velocity", "vel"), &StaticBody2D::set_constant_linear_velocity);
	ClassDB::bind_method(D_METHOD("set_constant_angular_velocity", "vel"), &StaticBody2D::set_constant_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_constant_linear_velocity"), &StaticBody2D::get_constant_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_constant_angular_velocity"), &StaticBody2D::get_constant_angular_velocity);

	ClassDB::bind_method(D_METHOD("set_physics_material_override", "physics_material_override"), &StaticBody2D::set_physics_material_override);
	ClassDB::bind_method(D_METHOD("get_physics_material_override"), &StaticBody2D::get_physics_material_override);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material_override", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material_override", "get_physics_material_override");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "constant_linear_velocity", PROPERTY_HINT_NONE, "suffix:px/s"), "set_constant_linear_velocity", "get_constant_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "constant_angular_velocity", PROPERTY_HINT_NONE, U"radians_as_degrees,suffix:\u00B0/s"), "set_constant_angular_velocity", "get_constant_angular_velocity");
}

StaticBody2D::StaticBody2D(PhysicsServer2D::BodyMode p_mode) :
		PhysicsBody2D(p_mode) {
}
