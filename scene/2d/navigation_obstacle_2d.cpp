/**************************************************************************/
/*  navigation_obstacle_2d.cpp                                            */
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

#include "navigation_obstacle_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/2d/navigation_mesh_source_geometry_data_2d.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/world_2d.h"
#include "servers/navigation_server_2d.h"

Callable NavigationObstacle2D::_navmesh_source_geometry_parsing_callback;
RID NavigationObstacle2D::_navmesh_source_geometry_parser;

void NavigationObstacle2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationObstacle2D::get_rid);

	ClassDB::bind_method(D_METHOD("set_avoidance_enabled", "enabled"), &NavigationObstacle2D::set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("get_avoidance_enabled"), &NavigationObstacle2D::get_avoidance_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationObstacle2D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationObstacle2D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &NavigationObstacle2D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &NavigationObstacle2D::get_radius);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &NavigationObstacle2D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &NavigationObstacle2D::get_velocity);

	ClassDB::bind_method(D_METHOD("set_vertices", "vertices"), &NavigationObstacle2D::set_vertices);
	ClassDB::bind_method(D_METHOD("get_vertices"), &NavigationObstacle2D::get_vertices);

	ClassDB::bind_method(D_METHOD("set_avoidance_layers", "layers"), &NavigationObstacle2D::set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("get_avoidance_layers"), &NavigationObstacle2D::get_avoidance_layers);

	ClassDB::bind_method(D_METHOD("set_avoidance_layer_value", "layer_number", "value"), &NavigationObstacle2D::set_avoidance_layer_value);
	ClassDB::bind_method(D_METHOD("get_avoidance_layer_value", "layer_number"), &NavigationObstacle2D::get_avoidance_layer_value);

	ClassDB::bind_method(D_METHOD("set_affect_navigation_mesh", "enabled"), &NavigationObstacle2D::set_affect_navigation_mesh);
	ClassDB::bind_method(D_METHOD("get_affect_navigation_mesh"), &NavigationObstacle2D::get_affect_navigation_mesh);

	ClassDB::bind_method(D_METHOD("set_carve_navigation_mesh", "enabled"), &NavigationObstacle2D::set_carve_navigation_mesh);
	ClassDB::bind_method(D_METHOD("get_carve_navigation_mesh"), &NavigationObstacle2D::get_carve_navigation_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.0,500,0.01,suffix:px"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "vertices"), "set_vertices", "get_vertices");
	ADD_GROUP("NavigationMesh", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "affect_navigation_mesh"), "set_affect_navigation_mesh", "get_affect_navigation_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "carve_navigation_mesh"), "set_carve_navigation_mesh", "get_carve_navigation_mesh");
	ADD_GROUP("Avoidance", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "avoidance_enabled"), "set_avoidance_enabled", "get_avoidance_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "avoidance_layers", PROPERTY_HINT_LAYERS_AVOIDANCE), "set_avoidance_layers", "get_avoidance_layers");
}

void NavigationObstacle2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			if (map_override.is_valid()) {
				_update_map(map_override);
			} else if (is_inside_tree()) {
				_update_map(get_world_2d()->get_navigation_map());
			} else {
				_update_map(RID());
			}
			previous_transform = get_global_transform();
			// need to trigger map controlled agent assignment somehow for the fake_agent since obstacles use no callback like regular agents
			NavigationServer2D::get_singleton()->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);
			_update_transform();
			set_physics_process_internal(true);
#ifdef DEBUG_ENABLED
			RS::get_singleton()->canvas_item_set_parent(debug_canvas_item, get_world_2d()->get_canvas());
#endif // DEBUG_ENABLED
		} break;

		case NOTIFICATION_EXIT_TREE: {
			set_physics_process_internal(false);
			_update_map(RID());
#ifdef DEBUG_ENABLED
			RS::get_singleton()->canvas_item_set_parent(debug_canvas_item, RID());
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
			NavigationServer2D::get_singleton()->obstacle_set_paused(obstacle, !can_process());
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
			NavigationServer2D::get_singleton()->obstacle_set_paused(obstacle, !can_process());
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
#ifdef DEBUG_ENABLED
			RS::get_singleton()->canvas_item_set_visible(debug_canvas_item, is_visible_in_tree());
#endif // DEBUG_ENABLED
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_inside_tree()) {
				_update_transform();

				if (velocity_submitted) {
					velocity_submitted = false;
					// only update if there is a noticeable change, else the rvo agent preferred velocity stays the same
					if (!previous_velocity.is_equal_approx(velocity)) {
						NavigationServer2D::get_singleton()->obstacle_set_velocity(obstacle, velocity);
					}
					previous_velocity = velocity;
				}
			}
		} break;

		case NOTIFICATION_DRAW: {
#ifdef DEBUG_ENABLED
			if (is_inside_tree()) {
				bool is_debug_enabled = false;
				if (Engine::get_singleton()->is_editor_hint()) {
					is_debug_enabled = true;
				} else if (NavigationServer2D::get_singleton()->get_debug_enabled() && NavigationServer2D::get_singleton()->get_debug_avoidance_enabled()) {
					is_debug_enabled = true;
				}

				if (is_debug_enabled) {
					RS::get_singleton()->canvas_item_clear(debug_canvas_item);
					RS::get_singleton()->canvas_item_set_transform(debug_canvas_item, Transform2D());
					_update_fake_agent_radius_debug();
					_update_static_obstacle_debug();
				}
			}
#endif // DEBUG_ENABLED
		} break;
	}
}

NavigationObstacle2D::NavigationObstacle2D() {
	obstacle = NavigationServer2D::get_singleton()->obstacle_create();

	NavigationServer2D::get_singleton()->obstacle_set_radius(obstacle, radius);
	NavigationServer2D::get_singleton()->obstacle_set_vertices(obstacle, vertices);
	NavigationServer2D::get_singleton()->obstacle_set_avoidance_layers(obstacle, avoidance_layers);
	NavigationServer2D::get_singleton()->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);

#ifdef DEBUG_ENABLED
	debug_canvas_item = RenderingServer::get_singleton()->canvas_item_create();
	debug_mesh_rid = RenderingServer::get_singleton()->mesh_create();
#endif // DEBUG_ENABLED
}

NavigationObstacle2D::~NavigationObstacle2D() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());

	NavigationServer2D::get_singleton()->free(obstacle);
	obstacle = RID();

#ifdef DEBUG_ENABLED
	if (debug_mesh_rid.is_valid()) {
		RenderingServer::get_singleton()->free(debug_mesh_rid);
		debug_mesh_rid = RID();
	}
	if (debug_canvas_item.is_valid()) {
		RenderingServer::get_singleton()->free(debug_canvas_item);
		debug_canvas_item = RID();
	}
#endif // DEBUG_ENABLED
}

void NavigationObstacle2D::set_vertices(const Vector<Vector2> &p_vertices) {
	vertices = p_vertices;

	vertices_are_clockwise = !Geometry2D::is_polygon_clockwise(vertices); // Geometry2D is inverted.
	vertices_are_valid = !Geometry2D::triangulate_polygon(vertices).is_empty();

	const Transform2D node_transform = is_inside_tree() ? get_global_transform() : Transform2D();
	NavigationServer2D::get_singleton()->obstacle_set_vertices(obstacle, node_transform.xform(vertices));
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationObstacle2D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}
	map_override = p_navigation_map;
	_update_map(map_override);
}

RID NavigationObstacle2D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_2d()->get_navigation_map();
	}
	return RID();
}

void NavigationObstacle2D::set_radius(real_t p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	if (Math::is_equal_approx(radius, p_radius)) {
		return;
	}

	radius = p_radius;

	const Vector2 safe_scale = (is_inside_tree() ? get_global_scale() : get_scale()).abs().maxf(0.001);
	NavigationServer2D::get_singleton()->obstacle_set_radius(obstacle, safe_scale[safe_scale.max_axis_index()] * radius);
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationObstacle2D::set_avoidance_layers(uint32_t p_layers) {
	if (avoidance_layers == p_layers) {
		return;
	}
	avoidance_layers = p_layers;
	NavigationServer2D::get_singleton()->obstacle_set_avoidance_layers(obstacle, avoidance_layers);
}

uint32_t NavigationObstacle2D::get_avoidance_layers() const {
	return avoidance_layers;
}

void NavigationObstacle2D::set_avoidance_layer_value(int p_layer_number, bool p_value) {
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

bool NavigationObstacle2D::get_avoidance_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	return get_avoidance_layers() & (1 << (p_layer_number - 1));
}

void NavigationObstacle2D::set_avoidance_enabled(bool p_enabled) {
	if (avoidance_enabled == p_enabled) {
		return;
	}

	avoidance_enabled = p_enabled;
	NavigationServer2D::get_singleton()->obstacle_set_avoidance_enabled(obstacle, avoidance_enabled);
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

bool NavigationObstacle2D::get_avoidance_enabled() const {
	return avoidance_enabled;
}

void NavigationObstacle2D::set_velocity(const Vector2 p_velocity) {
	velocity = p_velocity;
	velocity_submitted = true;
}

void NavigationObstacle2D::set_affect_navigation_mesh(bool p_enabled) {
	affect_navigation_mesh = p_enabled;
}

bool NavigationObstacle2D::get_affect_navigation_mesh() const {
	return affect_navigation_mesh;
}

void NavigationObstacle2D::set_carve_navigation_mesh(bool p_enabled) {
	carve_navigation_mesh = p_enabled;
}

bool NavigationObstacle2D::get_carve_navigation_mesh() const {
	return carve_navigation_mesh;
}

PackedStringArray NavigationObstacle2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	const Vector2 global_scale = get_global_scale();
	if (global_scale.x < 0.001 || global_scale.y < 0.001) {
		warnings.push_back(RTR("NavigationObstacle2D does not support negative or zero scaling."));
	}

	if (radius > 0.0 && !get_global_transform().is_conformal()) {
		warnings.push_back(RTR("The agent radius can only be scaled uniformly. The largest value along the two axes of the global scale will be used to scale the radius. This value may change in unexpected ways when the node is rotated."));
	}

	if (radius > 0.0 && get_global_skew() != 0.0) {
		warnings.push_back(RTR("Skew has no effect on the agent radius."));
	}

	return warnings;
}

void NavigationObstacle2D::navmesh_parse_init() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	if (!_navmesh_source_geometry_parser.is_valid()) {
		_navmesh_source_geometry_parsing_callback = callable_mp_static(&NavigationObstacle2D::navmesh_parse_source_geometry);
		_navmesh_source_geometry_parser = NavigationServer2D::get_singleton()->source_geometry_parser_create();
		NavigationServer2D::get_singleton()->source_geometry_parser_set_callback(_navmesh_source_geometry_parser, _navmesh_source_geometry_parsing_callback);
	}
}

void NavigationObstacle2D::navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node) {
	NavigationObstacle2D *obstacle = Object::cast_to<NavigationObstacle2D>(p_node);

	if (obstacle == nullptr) {
		return;
	}

	if (!obstacle->get_affect_navigation_mesh()) {
		return;
	}

	const Vector2 safe_scale = obstacle->get_global_scale().abs().maxf(0.001);
	const float obstacle_radius = obstacle->get_radius();

	if (obstacle_radius > 0.0) {
		// Radius defined obstacle should be uniformly scaled from obstacle basis max scale axis.
		const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
		const Vector2 uniform_max_scale = Vector2(scaling_max_value, scaling_max_value);
		const Transform2D obstacle_circle_transform = p_source_geometry_data->root_node_transform * Transform2D(obstacle->get_global_rotation(), uniform_max_scale, 0.0, obstacle->get_global_position());

		Vector<Vector2> obstruction_circle_vertices;

		// The point of this is that the moving obstacle can make a simple hole in the navigation mesh and affect the pathfinding.
		// Without, navigation paths can go directly through the middle of the obstacle and conflict with the avoidance to get agents stuck.
		// No place for excessive "round" detail here. Every additional edge adds a high cost for something that needs to be quick, not pretty.
		static const int circle_points = 12;

		obstruction_circle_vertices.resize(circle_points);
		Vector2 *circle_vertices_ptrw = obstruction_circle_vertices.ptrw();
		const real_t circle_point_step = Math_TAU / circle_points;

		for (int i = 0; i < circle_points; i++) {
			const float angle = i * circle_point_step;
			circle_vertices_ptrw[i] = obstacle_circle_transform.xform(Vector2(Math::cos(angle) * obstacle_radius, Math::sin(angle) * obstacle_radius));
		}

		p_source_geometry_data->add_projected_obstruction(obstruction_circle_vertices, obstacle->get_carve_navigation_mesh());
	}

	// Obstacles are projected to the xz-plane, so only rotation around the y-axis can be taken into account.
	const Transform2D node_xform = p_source_geometry_data->root_node_transform * obstacle->get_global_transform();

	const Vector<Vector2> &obstacle_vertices = obstacle->get_vertices();

	if (obstacle_vertices.is_empty()) {
		return;
	}

	Vector<Vector2> obstruction_shape_vertices;
	obstruction_shape_vertices.resize(obstacle_vertices.size());

	const Vector2 *obstacle_vertices_ptr = obstacle_vertices.ptr();
	Vector2 *obstruction_shape_vertices_ptrw = obstruction_shape_vertices.ptrw();

	for (int i = 0; i < obstacle_vertices.size(); i++) {
		obstruction_shape_vertices_ptrw[i] = node_xform.xform(obstacle_vertices_ptr[i]);
	}
	p_source_geometry_data->add_projected_obstruction(obstruction_shape_vertices, obstacle->get_carve_navigation_mesh());
}

void NavigationObstacle2D::_update_map(RID p_map) {
	map_current = p_map;
	NavigationServer2D::get_singleton()->obstacle_set_map(obstacle, p_map);
}

void NavigationObstacle2D::_update_position(const Vector2 p_position) {
	NavigationServer2D::get_singleton()->obstacle_set_position(obstacle, p_position);
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationObstacle2D::_update_transform() {
	_update_position(get_global_position());
	// Prevent non-positive or non-uniform scaling of dynamic obstacle radius.
	const Vector2 safe_scale = get_global_scale().abs().maxf(0.001);
	const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
	NavigationServer2D::get_singleton()->obstacle_set_radius(obstacle, scaling_max_value * radius);
	NavigationServer2D::get_singleton()->obstacle_set_vertices(obstacle, get_global_transform().translated(-get_global_position()).xform(vertices));
#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

#ifdef DEBUG_ENABLED
void NavigationObstacle2D::_update_fake_agent_radius_debug() {
	if (radius > 0.0 && NavigationServer2D::get_singleton()->get_debug_navigation_avoidance_enable_obstacles_radius()) {
		Color debug_radius_color = NavigationServer2D::get_singleton()->get_debug_navigation_avoidance_obstacles_radius_color();
		// Prevent non-positive scaling.
		const Vector2 safe_scale = get_global_scale().abs().maxf(0.001);
		// Agent radius is a scalar value and does not support non-uniform scaling, choose the largest axis.
		const float scaling_max_value = safe_scale[safe_scale.max_axis_index()];
		RS::get_singleton()->canvas_item_add_circle(debug_canvas_item, get_global_position(), scaling_max_value * radius, debug_radius_color);
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationObstacle2D::_update_static_obstacle_debug() {
	if (get_vertices().size() < 3) {
		return;
	}

	if (!NavigationServer2D::get_singleton()->get_debug_navigation_avoidance_enable_obstacles_static()) {
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();

	rs->mesh_clear(debug_mesh_rid);

	const int vertex_count = vertices.size();

	Vector<Vector2> edge_vertex_array;
	edge_vertex_array.resize(vertex_count * 4);

	Vector2 *edge_vertex_array_ptrw = edge_vertex_array.ptrw();

	int vertex_index = 0;

	for (int i = 0; i < vertex_count; i++) {
		Vector2 point = vertices[i];
		Vector2 next_point = vertices[(i + 1) % vertex_count];

		Vector2 direction = next_point.direction_to(point);
		Vector2 arrow_dir = -direction.orthogonal();
		Vector2 edge_middle = point + ((next_point - point) * 0.5);

		edge_vertex_array_ptrw[vertex_index++] = edge_middle;
		edge_vertex_array_ptrw[vertex_index++] = edge_middle + (arrow_dir * 10.0);

		edge_vertex_array_ptrw[vertex_index++] = point;
		edge_vertex_array_ptrw[vertex_index++] = next_point;
	}

	Color debug_static_obstacle_edge_color;

	if (are_vertices_valid()) {
		debug_static_obstacle_edge_color = NavigationServer2D::get_singleton()->get_debug_navigation_avoidance_static_obstacle_pushout_edge_color();
	} else {
		debug_static_obstacle_edge_color = NavigationServer2D::get_singleton()->get_debug_navigation_avoidance_static_obstacle_pushin_edge_color();
	}

	Vector<Color> line_color_array;
	line_color_array.resize(edge_vertex_array.size());
	line_color_array.fill(debug_static_obstacle_edge_color);

	Array edge_mesh_array;
	edge_mesh_array.resize(Mesh::ARRAY_MAX);
	edge_mesh_array[Mesh::ARRAY_VERTEX] = edge_vertex_array;
	edge_mesh_array[Mesh::ARRAY_COLOR] = line_color_array;

	rs->mesh_add_surface_from_arrays(debug_mesh_rid, RS::PRIMITIVE_LINES, edge_mesh_array, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);

	rs->canvas_item_add_mesh(debug_canvas_item, debug_mesh_rid, get_global_transform());
}
#endif // DEBUG_ENABLED
