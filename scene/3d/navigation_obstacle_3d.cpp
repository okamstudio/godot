/**************************************************************************/
/*  navigation_obstacle_3d.cpp                                            */
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

#include "navigation_obstacle_3d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/navigation_server_3d.h"

Callable NavigationObstacle3D::_navmesh_source_geometry_parsing_callback;
RID NavigationObstacle3D::_navmesh_source_geometry_parser;

void NavigationObstacle3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationObstacle3D::get_rid);

	ClassDB::bind_method(D_METHOD("set_avoidance_enabled", "enabled"), &NavigationObstacle3D::set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("get_avoidance_enabled"), &NavigationObstacle3D::get_avoidance_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationObstacle3D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationObstacle3D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &NavigationObstacle3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &NavigationObstacle3D::get_radius);

	ClassDB::bind_method(D_METHOD("set_height", "height"), &NavigationObstacle3D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &NavigationObstacle3D::get_height);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &NavigationObstacle3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &NavigationObstacle3D::get_velocity);

	ClassDB::bind_method(D_METHOD("set_vertices", "vertices"), &NavigationObstacle3D::set_vertices);
	ClassDB::bind_method(D_METHOD("get_vertices"), &NavigationObstacle3D::get_vertices);

	ClassDB::bind_method(D_METHOD("set_avoidance_layers", "layers"), &NavigationObstacle3D::set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("get_avoidance_layers"), &NavigationObstacle3D::get_avoidance_layers);

	ClassDB::bind_method(D_METHOD("set_avoidance_layer_value", "layer_number", "value"), &NavigationObstacle3D::set_avoidance_layer_value);
	ClassDB::bind_method(D_METHOD("get_avoidance_layer_value", "layer_number"), &NavigationObstacle3D::get_avoidance_layer_value);

	ClassDB::bind_method(D_METHOD("set_use_3d_avoidance", "enabled"), &NavigationObstacle3D::set_use_3d_avoidance);
	ClassDB::bind_method(D_METHOD("get_use_3d_avoidance"), &NavigationObstacle3D::get_use_3d_avoidance);

	ClassDB::bind_method(D_METHOD("set_affect_navigation_mesh", "enabled"), &NavigationObstacle3D::set_affect_navigation_mesh);
	ClassDB::bind_method(D_METHOD("get_affect_navigation_mesh"), &NavigationObstacle3D::get_affect_navigation_mesh);

	ClassDB::bind_method(D_METHOD("set_carve_navigation_mesh", "enabled"), &NavigationObstacle3D::set_carve_navigation_mesh);
	ClassDB::bind_method(D_METHOD("get_carve_navigation_mesh"), &NavigationObstacle3D::get_carve_navigation_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.0,100,0.01,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.0,100,0.01,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "vertices"), "set_vertices", "get_vertices");
	ADD_GROUP("NavigationMesh", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "affect_navigation_mesh"), "set_affect_navigation_mesh", "get_affect_navigation_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "carve_navigation_mesh"), "set_carve_navigation_mesh", "get_carve_navigation_mesh");
	ADD_GROUP("Avoidance", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "avoidance_enabled"), "set_avoidance_enabled", "get_avoidance_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "avoidance_layers", PROPERTY_HINT_LAYERS_AVOIDANCE), "set_avoidance_layers", "get_avoidance_layers");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_3d_avoidance"), "set_use_3d_avoidance", "get_use_3d_avoidance");
}

void NavigationObstacle3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			if (map_override.is_valid()) {
				_update_map(map_override);
			} else if (is_inside_tree()) {
				_update_map(get_world_3d()->get_navigation_map());
			} else {
				_update_map(RID());
			}
			// need to trigger map controlled agent assignment somehow for the fake_agent since obstacles use no callback like regular agents
			NavigationServer3D::get_singleton()->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);
			_update_transform();
			set_physics_process_internal(true);
#ifdef DEBUG_ENABLED
			_update_debug();
#endif // DEBUG_ENABLED
		} break;

#ifdef TOOLS_ENABLED
		case NOTIFICATION_TRANSFORM_CHANGED: {
			update_gizmos();
		} break;
#endif // TOOLS_ENABLED

		case NOTIFICATION_EXIT_TREE: {
			set_physics_process_internal(false);
			_update_map(RID());
#ifdef DEBUG_ENABLED
			_clear_debug();
#endif // DEBUG_ENABLED
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_PAUSED: {
			if (!can_process()) {
				map_before_pause = map_current;
				_update_map(RID());
			} else if (can_process() && !(map_before_pause == RID())) {
				_update_map(map_before_pause);
				map_before_pause = RID();
			}
			NavigationServer3D::get_singleton()->obstacle_set_paused(obstacle, !can_process());
		} break;

		case NOTIFICATION_UNSUSPENDED: {
			if (get_tree()->is_paused()) {
				break;
			}
			[[fallthrough]];
		}

		case NOTIFICATION_UNPAUSED: {
			if (!can_process()) {
				map_before_pause = map_current;
				_update_map(RID());
			} else if (can_process() && !(map_before_pause == RID())) {
				_update_map(map_before_pause);
				map_before_pause = RID();
			}
			NavigationServer3D::get_singleton()->obstacle_set_paused(obstacle, !can_process());
		} break;

#ifdef DEBUG_ENABLED
		case NOTIFICATION_VISIBILITY_CHANGED: {
			_update_debug();
		} break;
#endif // DEBUG_ENABLED

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_inside_tree()) {
				_update_transform();

				if (velocity_submitted) {
					velocity_submitted = false;
					// only update if there is a noticeable change, else the rvo agent preferred velocity stays the same
					if (!previous_velocity.is_equal_approx(velocity)) {
						NavigationServer3D::get_singleton()->obstacle_set_velocity(obstacle, velocity);
					}
					previous_velocity = velocity;
				}
#ifdef DEBUG_ENABLED
				if (fake_agent_radius_debug_instance_rid.is_valid() && radius > 0.0) {
					// Prevent non-positive scaling.
					const Vector3 safe_scale = get_global_basis().get_scale().abs().maxf(0.001);
					// Agent radius is a scalar value and does not support non-uniform scaling, choose the largest axis.
					const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
					const Vector3 uniform_max_scale = Vector3(scaling_max_value, scaling_max_value, scaling_max_value);
					const Transform3D debug_transform = Transform3D(Basis().scaled(uniform_max_scale), get_global_position());

					RS::get_singleton()->instance_set_transform(fake_agent_radius_debug_instance_rid, debug_transform);
				}
				if (static_obstacle_debug_instance_rid.is_valid() && get_vertices().size() > 0) {
					// Prevent non-positive scaling.
					const Vector3 safe_scale = get_global_basis().get_scale().abs().maxf(0.001);
					// Obstacles are projected to the xz-plane, so only rotation around the y-axis can be taken into account.
					const Transform3D debug_transform = Transform3D(Basis().scaled(safe_scale).rotated(Vector3(0.0, 1.0, 0.0), get_global_rotation().y), get_global_position());

					RS::get_singleton()->instance_set_transform(static_obstacle_debug_instance_rid, debug_transform);
				}
#endif // DEBUG_ENABLED
			}
		} break;
	}
}

NavigationObstacle3D::NavigationObstacle3D() {
	NavigationServer3D *ns3d = NavigationServer3D::get_singleton();

	obstacle = ns3d->obstacle_create();

	ns3d->obstacle_set_height(obstacle, height);
	ns3d->obstacle_set_radius(obstacle, radius);
	ns3d->obstacle_set_vertices(obstacle, vertices);
	ns3d->obstacle_set_avoidance_layers(obstacle, avoidance_layers);
	ns3d->obstacle_set_use_3d_avoidance(obstacle, use_3d_avoidance);
	ns3d->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);

#ifdef DEBUG_ENABLED
	RenderingServer *rs = RenderingServer::get_singleton();

	fake_agent_radius_debug_mesh_rid = rs->mesh_create();
	static_obstacle_debug_mesh_rid = rs->mesh_create();

	fake_agent_radius_debug_instance_rid = rs->instance_create();
	static_obstacle_debug_instance_rid = rs->instance_create();

	rs->instance_set_base(fake_agent_radius_debug_instance_rid, fake_agent_radius_debug_mesh_rid);
	rs->instance_set_base(static_obstacle_debug_instance_rid, static_obstacle_debug_mesh_rid);

	ns3d->connect("avoidance_debug_changed", callable_mp(this, &NavigationObstacle3D::_update_fake_agent_radius_debug));
	ns3d->connect("avoidance_debug_changed", callable_mp(this, &NavigationObstacle3D::_update_static_obstacle_debug));
	_update_fake_agent_radius_debug();
	_update_static_obstacle_debug();
#endif // DEBUG_ENABLED

#ifdef TOOLS_ENABLED
	set_notify_transform(true);
#endif // TOOLS_ENABLED
}

NavigationObstacle3D::~NavigationObstacle3D() {
	NavigationServer3D *ns3d = NavigationServer3D::get_singleton();
	ERR_FAIL_NULL(ns3d);

	ns3d->free(obstacle);
	obstacle = RID();

#ifdef DEBUG_ENABLED
	ns3d->disconnect("avoidance_debug_changed", callable_mp(this, &NavigationObstacle3D::_update_fake_agent_radius_debug));
	ns3d->disconnect("avoidance_debug_changed", callable_mp(this, &NavigationObstacle3D::_update_static_obstacle_debug));

	RenderingServer *rs = RenderingServer::get_singleton();
	ERR_FAIL_NULL(rs);
	if (fake_agent_radius_debug_instance_rid.is_valid()) {
		rs->free(fake_agent_radius_debug_instance_rid);
		fake_agent_radius_debug_instance_rid = RID();
	}
	if (fake_agent_radius_debug_mesh_rid.is_valid()) {
		rs->free(fake_agent_radius_debug_mesh_rid);
		fake_agent_radius_debug_mesh_rid = RID();
	}
	if (static_obstacle_debug_instance_rid.is_valid()) {
		rs->free(static_obstacle_debug_instance_rid);
		static_obstacle_debug_instance_rid = RID();
	}
	if (static_obstacle_debug_mesh_rid.is_valid()) {
		rs->free(static_obstacle_debug_mesh_rid);
		static_obstacle_debug_mesh_rid = RID();
	}
#endif // DEBUG_ENABLED
}

void NavigationObstacle3D::set_vertices(const Vector<Vector3> &p_vertices) {
	vertices = p_vertices;

	Vector<Vector2> vertices_2d;
	vertices_2d.resize(vertices.size());

	const Vector3 *vertices_ptr = vertices.ptr();
	Vector2 *vertices_2d_ptrw = vertices_2d.ptrw();

	for (int i = 0; i < vertices.size(); i++) {
		vertices_2d_ptrw[i] = Vector2(vertices_ptr[i].x, vertices_ptr[i].z);
	}

	vertices_are_clockwise = !Geometry2D::is_polygon_clockwise(vertices_2d); // Geometry2D is inverted.
	vertices_are_valid = !Geometry2D::triangulate_polygon(vertices_2d).is_empty();

	const Basis basis = is_inside_tree() ? get_global_basis() : get_basis();
	const float rotation_y = is_inside_tree() ? get_global_rotation().y : get_rotation().y;
	const Vector3 safe_scale = basis.get_scale().abs().maxf(0.001);
	const Transform3D safe_transform = Transform3D(Basis().scaled(safe_scale).rotated(Vector3(0.0, 1.0, 0.0), rotation_y), Vector3());
	NavigationServer3D::get_singleton()->obstacle_set_vertices(obstacle, safe_transform.xform(vertices));
#ifdef DEBUG_ENABLED
	_update_static_obstacle_debug();
	update_gizmos();
#endif // DEBUG_ENABLED
}

void NavigationObstacle3D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}
	map_override = p_navigation_map;
	_update_map(map_override);
}

RID NavigationObstacle3D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_3d()->get_navigation_map();
	}
	return RID();
}

void NavigationObstacle3D::set_radius(real_t p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	if (Math::is_equal_approx(radius, p_radius)) {
		return;
	}

	radius = p_radius;

	// Prevent non-positive or non-uniform scaling of dynamic obstacle radius.
	const Vector3 safe_scale = (is_inside_tree() ? get_global_basis() : get_basis()).get_scale().abs().maxf(0.001);
	NavigationServer3D::get_singleton()->obstacle_set_radius(obstacle, safe_scale[safe_scale.max_axis_index()] * radius);

#ifdef DEBUG_ENABLED
	_update_fake_agent_radius_debug();
	update_gizmos();
#endif // DEBUG_ENABLED
}

void NavigationObstacle3D::set_height(real_t p_height) {
	ERR_FAIL_COND_MSG(p_height < 0.0, "Height must be positive.");
	if (Math::is_equal_approx(height, p_height)) {
		return;
	}

	height = p_height;
	const float scale_factor = MAX(Math::abs((is_inside_tree() ? get_global_basis() : get_basis()).get_scale().y), 0.001);
	NavigationServer3D::get_singleton()->obstacle_set_height(obstacle, scale_factor * height);

#ifdef DEBUG_ENABLED
	_update_static_obstacle_debug();
	update_gizmos();
#endif // DEBUG_ENABLED
}

void NavigationObstacle3D::set_avoidance_layers(uint32_t p_layers) {
	avoidance_layers = p_layers;
	NavigationServer3D::get_singleton()->obstacle_set_avoidance_layers(obstacle, avoidance_layers);
}

uint32_t NavigationObstacle3D::get_avoidance_layers() const {
	return avoidance_layers;
}

void NavigationObstacle3D::set_avoidance_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Avoidance layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Avoidance layer number must be between 1 and 32 inclusive.");
	uint32_t avoidance_layers_new = get_avoidance_layers();
	if (p_value) {
		avoidance_layers_new |= 1 << (p_layer_number - 1);
	} else {
		avoidance_layers_new &= ~(1 << (p_layer_number - 1));
	}
	set_avoidance_layers(avoidance_layers_new);
}

bool NavigationObstacle3D::get_avoidance_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	return get_avoidance_layers() & (1 << (p_layer_number - 1));
}

void NavigationObstacle3D::set_avoidance_enabled(bool p_enabled) {
	if (avoidance_enabled == p_enabled) {
		return;
	}

	avoidance_enabled = p_enabled;
	NavigationServer3D::get_singleton()->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);
}

bool NavigationObstacle3D::get_avoidance_enabled() const {
	return avoidance_enabled;
}

void NavigationObstacle3D::set_use_3d_avoidance(bool p_use_3d_avoidance) {
	use_3d_avoidance = p_use_3d_avoidance;
	_update_use_3d_avoidance(use_3d_avoidance);
	notify_property_list_changed();
}

void NavigationObstacle3D::set_velocity(const Vector3 p_velocity) {
	velocity = p_velocity;
	velocity_submitted = true;
}

void NavigationObstacle3D::set_affect_navigation_mesh(bool p_enabled) {
	affect_navigation_mesh = p_enabled;
}

bool NavigationObstacle3D::get_affect_navigation_mesh() const {
	return affect_navigation_mesh;
}

void NavigationObstacle3D::set_carve_navigation_mesh(bool p_enabled) {
	carve_navigation_mesh = p_enabled;
}

bool NavigationObstacle3D::get_carve_navigation_mesh() const {
	return carve_navigation_mesh;
}

PackedStringArray NavigationObstacle3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (get_global_rotation().x != 0.0 || get_global_rotation().z != 0.0) {
		warnings.push_back(RTR("NavigationObstacle3D only takes global rotation around the y-axis into account. Rotations around the x-axis or z-axis might lead to unexpected results."));
	}

	const Vector3 global_scale = get_global_basis().get_scale();
	if (global_scale.x < 0.001 || global_scale.y < 0.001 || global_scale.z < 0.001) {
		warnings.push_back(RTR("NavigationObstacle3D does not support negative or zero scaling."));
	}

	if (radius > 0.0 && !get_global_basis().is_conformal()) {
		warnings.push_back(RTR("The agent radius can only be scaled uniformly. The largest scale value along the three axes will be used."));
	}

	return warnings;
}

void NavigationObstacle3D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&NavigationObstacle3D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer3D::get_singleton()->source_geometry_parser_create();
		NavigationServer3D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void NavigationObstacle3D::navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node) {
	NavigationObstacle3D *obstacle = Object::cast_to<NavigationObstacle3D>(p_node);

	if (obstacle == nullptr) {
		return;
	}

	if (!obstacle->get_affect_navigation_mesh()) {
		return;
	}

	const float elevation = obstacle->get_global_position().y + p_source_geometry_data->root_node_transform.origin.y;
	// Prevent non-positive scaling.
	const Vector3 safe_scale = obstacle->get_global_basis().get_scale().abs().maxf(0.001);
	const float obstacle_radius = obstacle->get_radius();

	if (obstacle_radius > 0.0) {
		// Radius defined obstacle should be uniformly scaled from obstacle basis max scale axis.
		const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
		const Vector3 uniform_max_scale = Vector3(scaling_max_value, scaling_max_value, scaling_max_value);
		const Transform3D obstacle_circle_transform = p_source_geometry_data->root_node_transform * Transform3D(Basis().scaled(uniform_max_scale), obstacle->get_global_position());

		Vector<Vector3> obstruction_circle_vertices;

		// The point of this is that the moving obstacle can make a simple hole in the navigation mesh and affect the pathfinding.
		// Without, navigation paths can go directly through the middle of the obstacle and conflict with the avoidance to get agents stuck.
		// No place for excessive "round" detail here. Every additional edge adds a high cost for something that needs to be quick, not pretty.
		static const int circle_points = 12;

		obstruction_circle_vertices.resize(circle_points);
		Vector3 *circle_vertices_ptrw = obstruction_circle_vertices.ptrw();
		const real_t circle_point_step = Math_TAU / circle_points;

		for (int i = 0; i < circle_points; i++) {
			const float angle = i * circle_point_step;
			circle_vertices_ptrw[i] = obstacle_circle_transform.xform(Vector3(Math::cos(angle) * obstacle_radius, 0.0, Math::sin(angle) * obstacle_radius));
		}

		p_source_geometry_data->add_projected_obstruction(obstruction_circle_vertices, elevation - obstacle_radius, scaling_max_value * obstacle_radius, obstacle->get_carve_navigation_mesh());
	}

	// Obstacles are projected to the xz-plane, so only rotation around the y-axis can be taken into account.
	const Transform3D node_xform = p_source_geometry_data->root_node_transform * Transform3D(Basis().scaled(safe_scale).rotated(Vector3(0.0, 1.0, 0.0), obstacle->get_global_rotation().y), obstacle->get_global_position());

	const Vector<Vector3> &obstacle_vertices = obstacle->get_vertices();

	if (obstacle_vertices.is_empty()) {
		return;
	}

	Vector<Vector3> obstruction_shape_vertices;
	obstruction_shape_vertices.resize(obstacle_vertices.size());

	const Vector3 *obstacle_vertices_ptr = obstacle_vertices.ptr();
	Vector3 *obstruction_shape_vertices_ptrw = obstruction_shape_vertices.ptrw();

	for (int i = 0; i < obstacle_vertices.size(); i++) {
		obstruction_shape_vertices_ptrw[i] = node_xform.xform(obstacle_vertices_ptr[i]);
		obstruction_shape_vertices_ptrw[i].y = 0.0;
	}
	p_source_geometry_data->add_projected_obstruction(obstruction_shape_vertices, elevation, safe_scale.y * obstacle->get_height(), obstacle->get_carve_navigation_mesh());
}

void NavigationObstacle3D::_update_map(RID p_map) {
	NavigationServer3D::get_singleton()->obstacle_set_map(obstacle, p_map);
	map_current = p_map;
}

void NavigationObstacle3D::_update_position(const Vector3 p_position) {
	NavigationServer3D::get_singleton()->obstacle_set_position(obstacle, p_position);
}

void NavigationObstacle3D::_update_transform() {
	_update_position(get_global_position());

	// Prevent non-positive or non-uniform scaling of dynamic obstacle radius.
	const Vector3 safe_scale = get_global_basis().get_scale().abs().maxf(0.001);
	const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
	NavigationServer3D::get_singleton()->obstacle_set_radius(obstacle, scaling_max_value * radius);

	// Apply modified node transform which only takes y-axis rotation into account to vertices.
	const Transform3D safe_transform = Transform3D(Basis().scaled(safe_scale).rotated(Vector3(0.0, 1.0, 0.0), get_global_rotation().y), Vector3());
	NavigationServer3D::get_singleton()->obstacle_set_vertices(obstacle, safe_transform.xform(vertices));
	NavigationServer3D::get_singleton()->obstacle_set_height(obstacle, safe_scale.y * height);
}

void NavigationObstacle3D::_update_use_3d_avoidance(bool p_use_3d_avoidance) {
	NavigationServer3D::get_singleton()->obstacle_set_use_3d_avoidance(obstacle, use_3d_avoidance);
	_update_map(map_current);
}

#ifdef DEBUG_ENABLED
void NavigationObstacle3D::_update_debug() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (is_inside_tree()) {
		rs->instance_set_visible(fake_agent_radius_debug_instance_rid, is_visible_in_tree());
		rs->instance_set_visible(static_obstacle_debug_instance_rid, is_visible_in_tree());
		rs->instance_set_scenario(fake_agent_radius_debug_instance_rid, get_world_3d()->get_scenario());
		rs->instance_set_scenario(static_obstacle_debug_instance_rid, get_world_3d()->get_scenario());
		rs->instance_set_transform(fake_agent_radius_debug_instance_rid, Transform3D(Basis(), get_global_position()));
		rs->instance_set_transform(static_obstacle_debug_instance_rid, Transform3D(Basis(), get_global_position()));
		_update_fake_agent_radius_debug();
		_update_static_obstacle_debug();
	} else {
		rs->mesh_clear(fake_agent_radius_debug_mesh_rid);
		rs->mesh_clear(static_obstacle_debug_mesh_rid);
		rs->instance_set_scenario(fake_agent_radius_debug_instance_rid, RID());
		rs->instance_set_scenario(static_obstacle_debug_instance_rid, RID());
	}
}

void NavigationObstacle3D::_update_fake_agent_radius_debug() {
	NavigationServer3D *ns3d = NavigationServer3D::get_singleton();
	RenderingServer *rs = RenderingServer::get_singleton();

	bool is_debug_enabled = false;
	if (Engine::get_singleton()->is_editor_hint()) {
		is_debug_enabled = true;
	} else if (ns3d->get_debug_enabled() &&
			ns3d->get_debug_avoidance_enabled() &&
			ns3d->get_debug_navigation_avoidance_enable_obstacles_radius()) {
		is_debug_enabled = true;
	}

	rs->mesh_clear(fake_agent_radius_debug_mesh_rid);

	if (!is_debug_enabled) {
		return;
	}

	Vector<Vector3> face_vertex_array;
	Vector<int> face_indices_array;

	int i, j, prevrow, thisrow, point;
	float x, y, z;

	int rings = 16;
	int radial_segments = 32;

	point = 0;

	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		float v = j;
		float w;

		v /= (rings + 1);
		w = sin(Math_PI * v);
		y = (radius)*cos(Math_PI * v);

		for (i = 0; i <= radial_segments; i++) {
			float u = i;
			u /= radial_segments;

			x = sin(u * Math_TAU);
			z = cos(u * Math_TAU);

			Vector3 p = Vector3(x * radius * w, y, z * radius * w);
			face_vertex_array.push_back(p);

			point++;

			if (i > 0 && j > 0) {
				face_indices_array.push_back(prevrow + i - 1);
				face_indices_array.push_back(prevrow + i);
				face_indices_array.push_back(thisrow + i - 1);

				face_indices_array.push_back(prevrow + i);
				face_indices_array.push_back(thisrow + i);
				face_indices_array.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	};

	Array face_mesh_array;
	face_mesh_array.resize(Mesh::ARRAY_MAX);
	face_mesh_array[Mesh::ARRAY_VERTEX] = face_vertex_array;
	face_mesh_array[Mesh::ARRAY_INDEX] = face_indices_array;

	rs->mesh_add_surface_from_arrays(fake_agent_radius_debug_mesh_rid, RS::PRIMITIVE_TRIANGLES, face_mesh_array);

	Ref<StandardMaterial3D> face_material = ns3d->get_debug_navigation_avoidance_obstacles_radius_material();
	rs->instance_set_surface_override_material(fake_agent_radius_debug_instance_rid, 0, face_material->get_rid());

	if (is_inside_tree()) {
		rs->instance_set_scenario(fake_agent_radius_debug_instance_rid, get_world_3d()->get_scenario());
		rs->instance_set_visible(fake_agent_radius_debug_instance_rid, is_visible_in_tree());
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationObstacle3D::_update_static_obstacle_debug() {
	if (Engine::get_singleton()->is_editor_hint()) {
		// Don't update inside Editor as Node3D gizmo takes care of this.
		return;
	}

	NavigationServer3D *ns3d = NavigationServer3D::get_singleton();
	RenderingServer *rs = RenderingServer::get_singleton();

	bool is_debug_enabled = false;
	if (ns3d->get_debug_enabled() &&
			ns3d->get_debug_avoidance_enabled() &&
			ns3d->get_debug_navigation_avoidance_enable_obstacles_static()) {
		is_debug_enabled = true;
	}

	rs->mesh_clear(static_obstacle_debug_mesh_rid);

	if (!is_debug_enabled) {
		return;
	}

	const int vertex_count = vertices.size();

	if (vertex_count < 3) {
		if (static_obstacle_debug_instance_rid.is_valid()) {
			rs->instance_set_visible(static_obstacle_debug_instance_rid, false);
		}
		return;
	}

	Vector<Vector3> edge_vertex_array;
	edge_vertex_array.resize(vertex_count * 8);

	Vector3 *edge_vertex_array_ptrw = edge_vertex_array.ptrw();

	int vertex_index = 0;

	for (int i = 0; i < vertex_count; i++) {
		Vector3 point = vertices[i];
		Vector3 next_point = vertices[(i + 1) % vertex_count];

		Vector3 direction = next_point.direction_to(point);
		Vector3 arrow_dir = direction.cross(Vector3(0.0, 1.0, 0.0));
		Vector3 edge_middle = point + ((next_point - point) * 0.5);

		edge_vertex_array_ptrw[vertex_index++] = edge_middle;
		edge_vertex_array_ptrw[vertex_index++] = edge_middle + (arrow_dir * 0.5);

		edge_vertex_array_ptrw[vertex_index++] = point;
		edge_vertex_array_ptrw[vertex_index++] = next_point;

		edge_vertex_array_ptrw[vertex_index++] = Vector3(point.x, height, point.z);
		edge_vertex_array_ptrw[vertex_index++] = Vector3(next_point.x, height, next_point.z);

		edge_vertex_array_ptrw[vertex_index++] = point;
		edge_vertex_array_ptrw[vertex_index++] = Vector3(point.x, height, point.z);
	}

	Array edge_mesh_array;
	edge_mesh_array.resize(Mesh::ARRAY_MAX);
	edge_mesh_array[Mesh::ARRAY_VERTEX] = edge_vertex_array;

	rs->mesh_add_surface_from_arrays(static_obstacle_debug_mesh_rid, RS::PRIMITIVE_LINES, edge_mesh_array);

	Ref<StandardMaterial3D> edge_material;

	if (are_vertices_valid()) {
		edge_material = ns3d->get_debug_navigation_avoidance_static_obstacle_pushout_edge_material();
	} else {
		edge_material = ns3d->get_debug_navigation_avoidance_static_obstacle_pushin_edge_material();
	}

	rs->instance_set_surface_override_material(static_obstacle_debug_instance_rid, 0, edge_material->get_rid());

	if (is_inside_tree()) {
		rs->instance_set_scenario(static_obstacle_debug_instance_rid, get_world_3d()->get_scenario());
		rs->instance_set_visible(static_obstacle_debug_instance_rid, is_visible_in_tree());
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationObstacle3D::_clear_debug() {
	RenderingServer *rs = RenderingServer::get_singleton();
	ERR_FAIL_NULL(rs);
	rs->mesh_clear(fake_agent_radius_debug_mesh_rid);
	rs->mesh_clear(static_obstacle_debug_mesh_rid);
	rs->instance_set_scenario(fake_agent_radius_debug_instance_rid, RID());
	rs->instance_set_scenario(static_obstacle_debug_instance_rid, RID());
}
#endif // DEBUG_ENABLED
