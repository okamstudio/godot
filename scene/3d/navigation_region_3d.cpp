/**************************************************************************/
/*  navigation_region_3d.cpp                                              */
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

#include "navigation_region_3d.h"

#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "servers/navigation_server_3d.h"

RID NavigationRegion3D::get_rid() const {
	return region;
}

void NavigationRegion3D::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}

	enabled = p_enabled;

	NavigationServer3D::get_singleton()->region_set_enabled(region, enabled);

#ifdef DEBUG_ENABLED
	if (debug_instance.is_valid()) {
		if (!is_enabled()) {
			if (debug_mesh.is_valid()) {
				if (debug_mesh->get_surface_count() > 0) {
					RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, NavigationServer3D::get_singleton()->get_debug_navigation_geometry_face_disabled_material()->get_rid());
				}
				if (debug_mesh->get_surface_count() > 1) {
					RS::get_singleton()->instance_set_surface_override_material(debug_instance, 1, NavigationServer3D::get_singleton()->get_debug_navigation_geometry_edge_disabled_material()->get_rid());
				}
			}
		} else {
			if (debug_mesh.is_valid()) {
				if (debug_mesh->get_surface_count() > 0) {
					RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, RID());
				}
				if (debug_mesh->get_surface_count() > 1) {
					RS::get_singleton()->instance_set_surface_override_material(debug_instance, 1, RID());
				}
			}
		}
	}
#endif // DEBUG_ENABLED

	update_gizmos();
}

bool NavigationRegion3D::is_enabled() const {
	return enabled;
}

void NavigationRegion3D::set_use_edge_connections(bool p_enabled) {
	if (use_edge_connections == p_enabled) {
		return;
	}

	use_edge_connections = p_enabled;

	NavigationServer3D::get_singleton()->region_set_use_edge_connections(region, use_edge_connections);
}

bool NavigationRegion3D::get_use_edge_connections() const {
	return use_edge_connections;
}

void NavigationRegion3D::set_navigation_layers(uint32_t p_navigation_layers) {
	if (navigation_layers == p_navigation_layers) {
		return;
	}

	navigation_layers = p_navigation_layers;

	NavigationServer3D::get_singleton()->region_set_navigation_layers(region, navigation_layers);
}

uint32_t NavigationRegion3D::get_navigation_layers() const {
	return navigation_layers;
}

void NavigationRegion3D::set_navigation_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Navigation layer number must be between 1 and 32 inclusive.");

	uint32_t _navigation_layers = get_navigation_layers();

	if (p_value) {
		_navigation_layers |= 1 << (p_layer_number - 1);
	} else {
		_navigation_layers &= ~(1 << (p_layer_number - 1));
	}

	set_navigation_layers(_navigation_layers);
}

bool NavigationRegion3D::get_navigation_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Navigation layer number must be between 1 and 32 inclusive.");

	return get_navigation_layers() & (1 << (p_layer_number - 1));
}

void NavigationRegion3D::set_enter_cost(real_t p_enter_cost) {
	ERR_FAIL_COND_MSG(p_enter_cost < 0.0, "The enter_cost must be positive.");
	if (Math::is_equal_approx(enter_cost, p_enter_cost)) {
		return;
	}

	enter_cost = p_enter_cost;

	NavigationServer3D::get_singleton()->region_set_enter_cost(region, enter_cost);
}

real_t NavigationRegion3D::get_enter_cost() const {
	return enter_cost;
}

void NavigationRegion3D::set_travel_cost(real_t p_travel_cost) {
	ERR_FAIL_COND_MSG(p_travel_cost < 0.0, "The travel_cost must be positive.");
	if (Math::is_equal_approx(travel_cost, p_travel_cost)) {
		return;
	}

	travel_cost = p_travel_cost;

	NavigationServer3D::get_singleton()->region_set_travel_cost(region, travel_cost);
}

real_t NavigationRegion3D::get_travel_cost() const {
	return travel_cost;
}

RID NavigationRegion3D::get_region_rid() const {
	return get_rid();
}

void NavigationRegion3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_region_enter_navigation_map();
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			set_physics_process_internal(true);
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			set_physics_process_internal(false);
			_region_update_transform();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_region_exit_navigation_map();
		} break;
	}
}

void NavigationRegion3D::set_navigation_mesh(const Ref<NavigationMesh> &p_navigation_mesh) {
	if (navigation_mesh.is_valid()) {
		navigation_mesh->disconnect_changed(callable_mp(this, &NavigationRegion3D::_navigation_mesh_changed));
	}

	navigation_mesh = p_navigation_mesh;

	if (navigation_mesh.is_valid()) {
		navigation_mesh->connect_changed(callable_mp(this, &NavigationRegion3D::_navigation_mesh_changed));
	}

	NavigationServer3D::get_singleton()->region_set_navigation_mesh(region, p_navigation_mesh);

#ifdef DEBUG_ENABLED
	if (is_inside_tree() && NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		if (navigation_mesh.is_valid()) {
			_update_debug_mesh();
			_update_debug_edge_connections_mesh();
		} else {
			if (debug_instance.is_valid()) {
				RS::get_singleton()->instance_set_visible(debug_instance, false);
			}
			if (debug_edge_connections_instance.is_valid()) {
				RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
			}
		}
	}
#endif // DEBUG_ENABLED

	emit_signal(SNAME("navigation_mesh_changed"));

	update_gizmos();
	update_configuration_warnings();
}

Ref<NavigationMesh> NavigationRegion3D::get_navigation_mesh() const {
	return navigation_mesh;
}

void NavigationRegion3D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}

	map_override = p_navigation_map;

	NavigationServer3D::get_singleton()->region_set_map(region, map_override);
}

RID NavigationRegion3D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_3d()->get_navigation_map();
	}
	return RID();
}

void NavigationRegion3D::bake_navigation_mesh(bool p_on_thread) {
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "The SceneTree can only be parsed on the main thread. Call this function from the main thread or use call_deferred().");
	ERR_FAIL_COND_MSG(navigation_mesh.is_null(), "Baking the navigation mesh requires a valid `NavigationMesh` resource.");

	Ref<NavigationMeshSourceGeometryData3D> source_geometry_data;
	source_geometry_data.instantiate();

	NavigationServer3D::get_singleton()->parse_source_geometry_data(navigation_mesh, source_geometry_data, this);

	if (p_on_thread) {
		NavigationServer3D::get_singleton()->bake_from_source_geometry_data_async(navigation_mesh, source_geometry_data, callable_mp(this, &NavigationRegion3D::_bake_finished).bind(navigation_mesh));
	} else {
		NavigationServer3D::get_singleton()->bake_from_source_geometry_data(navigation_mesh, source_geometry_data, callable_mp(this, &NavigationRegion3D::_bake_finished).bind(navigation_mesh));
	}
}

void NavigationRegion3D::_bake_finished(Ref<NavigationMesh> p_navigation_mesh) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &NavigationRegion3D::_bake_finished).call_deferred(p_navigation_mesh);
		return;
	}

	set_navigation_mesh(p_navigation_mesh);
	emit_signal(SNAME("bake_finished"));
}

bool NavigationRegion3D::is_baking() const {
	return NavigationServer3D::get_singleton()->is_baking_navigation_mesh(navigation_mesh);
}

PackedStringArray NavigationRegion3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (is_visible_in_tree() && is_inside_tree()) {
		if (navigation_mesh.is_null()) {
			warnings.push_back(RTR("A NavigationMesh resource must be set or created for this node to work."));
		}
	}

	return warnings;
}

void NavigationRegion3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationRegion3D::get_rid);

	ClassDB::bind_method(D_METHOD("set_navigation_mesh", "navigation_mesh"), &NavigationRegion3D::set_navigation_mesh);
	ClassDB::bind_method(D_METHOD("get_navigation_mesh"), &NavigationRegion3D::get_navigation_mesh);

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &NavigationRegion3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &NavigationRegion3D::is_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationRegion3D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationRegion3D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_use_edge_connections", "enabled"), &NavigationRegion3D::set_use_edge_connections);
	ClassDB::bind_method(D_METHOD("get_use_edge_connections"), &NavigationRegion3D::get_use_edge_connections);

	ClassDB::bind_method(D_METHOD("set_navigation_layers", "navigation_layers"), &NavigationRegion3D::set_navigation_layers);
	ClassDB::bind_method(D_METHOD("get_navigation_layers"), &NavigationRegion3D::get_navigation_layers);

	ClassDB::bind_method(D_METHOD("set_navigation_layer_value", "layer_number", "value"), &NavigationRegion3D::set_navigation_layer_value);
	ClassDB::bind_method(D_METHOD("get_navigation_layer_value", "layer_number"), &NavigationRegion3D::get_navigation_layer_value);

	ClassDB::bind_method(D_METHOD("get_region_rid"), &NavigationRegion3D::get_region_rid);

	ClassDB::bind_method(D_METHOD("set_enter_cost", "enter_cost"), &NavigationRegion3D::set_enter_cost);
	ClassDB::bind_method(D_METHOD("get_enter_cost"), &NavigationRegion3D::get_enter_cost);

	ClassDB::bind_method(D_METHOD("set_travel_cost", "travel_cost"), &NavigationRegion3D::set_travel_cost);
	ClassDB::bind_method(D_METHOD("get_travel_cost"), &NavigationRegion3D::get_travel_cost);

	ClassDB::bind_method(D_METHOD("bake_navigation_mesh", "on_thread"), &NavigationRegion3D::bake_navigation_mesh, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("is_baking"), &NavigationRegion3D::is_baking);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "navigation_mesh", PROPERTY_HINT_RESOURCE_TYPE, "NavigationMesh"), "set_navigation_mesh", "get_navigation_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_edge_connections"), "set_use_edge_connections", "get_use_edge_connections");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_layers", PROPERTY_HINT_LAYERS_3D_NAVIGATION), "set_navigation_layers", "get_navigation_layers");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "enter_cost"), "set_enter_cost", "get_enter_cost");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "travel_cost"), "set_travel_cost", "get_travel_cost");

	ADD_SIGNAL(MethodInfo("navigation_mesh_changed"));
	ADD_SIGNAL(MethodInfo("bake_finished"));
}

#ifndef DISABLE_DEPRECATED
// Compatibility with earlier 4.0 betas.
bool NavigationRegion3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "navmesh") {
		set_navigation_mesh(p_value);
		return true;
	}
	return false;
}

bool NavigationRegion3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "navmesh") {
		r_ret = get_navigation_mesh();
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

void NavigationRegion3D::_navigation_mesh_changed() {
	update_gizmos();
	update_configuration_warnings();

#ifdef DEBUG_ENABLED
	_update_debug_edge_connections_mesh();
#endif // DEBUG_ENABLED
}

#ifdef DEBUG_ENABLED
void NavigationRegion3D::_navigation_map_changed(RID p_map) {
	if (is_inside_tree() && p_map == get_world_3d()->get_navigation_map()) {
		_update_debug_edge_connections_mesh();
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationRegion3D::_navigation_debug_changed() {
	if (is_inside_tree()) {
		_update_debug_mesh();
		_update_debug_edge_connections_mesh();
	}
}
#endif // DEBUG_ENABLED

void NavigationRegion3D::_region_enter_navigation_map() {
	if (!is_inside_tree()) {
		return;
	}

	if (map_override.is_valid()) {
		NavigationServer3D::get_singleton()->region_set_map(region, map_override);
	} else {
		NavigationServer3D::get_singleton()->region_set_map(region, get_world_3d()->get_navigation_map());
	}

	current_global_transform = get_global_transform();
	NavigationServer3D::get_singleton()->region_set_transform(region, current_global_transform);

	NavigationServer3D::get_singleton()->region_set_enabled(region, enabled);

#ifdef DEBUG_ENABLED
	if (NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		_update_debug_mesh();
	}
#endif // DEBUG_ENABLED
}

void NavigationRegion3D::_region_exit_navigation_map() {
	NavigationServer3D::get_singleton()->region_set_map(region, RID());
#ifdef DEBUG_ENABLED
	if (debug_instance.is_valid()) {
		RS::get_singleton()->instance_set_visible(debug_instance, false);
	}
	if (debug_edge_connections_instance.is_valid()) {
		RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
	}
#endif // DEBUG_ENABLED
}

void NavigationRegion3D::_region_update_transform() {
	if (!is_inside_tree()) {
		return;
	}

	Transform3D new_global_transform = get_global_transform();
	if (current_global_transform != new_global_transform) {
		current_global_transform = new_global_transform;
		NavigationServer3D::get_singleton()->region_set_transform(region, current_global_transform);
#ifdef DEBUG_ENABLED
		if (debug_instance.is_valid()) {
			RS::get_singleton()->instance_set_transform(debug_instance, current_global_transform);
		}
#endif // DEBUG_ENABLED
	}
}

NavigationRegion3D::NavigationRegion3D() {
	set_notify_transform(true);

	region = NavigationServer3D::get_singleton()->region_create();
	NavigationServer3D::get_singleton()->region_set_owner_id(region, get_instance_id());
	NavigationServer3D::get_singleton()->region_set_enter_cost(region, get_enter_cost());
	NavigationServer3D::get_singleton()->region_set_travel_cost(region, get_travel_cost());
	NavigationServer3D::get_singleton()->region_set_navigation_layers(region, navigation_layers);
	NavigationServer3D::get_singleton()->region_set_use_edge_connections(region, use_edge_connections);
	NavigationServer3D::get_singleton()->region_set_enabled(region, enabled);

#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->connect(SNAME("map_changed"), callable_mp(this, &NavigationRegion3D::_navigation_map_changed));
	NavigationServer3D::get_singleton()->connect(SNAME("navigation_debug_changed"), callable_mp(this, &NavigationRegion3D::_navigation_debug_changed));
#endif // DEBUG_ENABLED
}

NavigationRegion3D::~NavigationRegion3D() {
	if (navigation_mesh.is_valid()) {
		navigation_mesh->disconnect_changed(callable_mp(this, &NavigationRegion3D::_navigation_mesh_changed));
	}
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	NavigationServer3D::get_singleton()->free(region);

#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->disconnect(SNAME("map_changed"), callable_mp(this, &NavigationRegion3D::_navigation_map_changed));
	NavigationServer3D::get_singleton()->disconnect(SNAME("navigation_debug_changed"), callable_mp(this, &NavigationRegion3D::_navigation_debug_changed));

	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (debug_instance.is_valid()) {
		RenderingServer::get_singleton()->free(debug_instance);
	}
	if (debug_mesh.is_valid()) {
		RenderingServer::get_singleton()->free(debug_mesh->get_rid());
	}
	if (debug_edge_connections_instance.is_valid()) {
		RenderingServer::get_singleton()->free(debug_edge_connections_instance);
	}
	if (debug_edge_connections_mesh.is_valid()) {
		RenderingServer::get_singleton()->free(debug_edge_connections_mesh->get_rid());
	}
#endif // DEBUG_ENABLED
}

#ifdef DEBUG_ENABLED
void NavigationRegion3D::_update_debug_mesh() {
	if (Engine::get_singleton()->is_editor_hint()) {
		// don't update inside Editor as node 3d gizmo takes care of this
		// as collisions and selections for Editor Viewport need to be updated
		return;
	}

	if (!NavigationServer3D::get_singleton()->get_debug_enabled() || !NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		if (debug_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(debug_instance, false);
		}
		return;
	}

	if (navigation_mesh.is_null()) {
		if (debug_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(debug_instance, false);
		}
		return;
	}

	if (!debug_instance.is_valid()) {
		debug_instance = RenderingServer::get_singleton()->instance_create();
	}

	if (debug_mesh.is_null()) {
		debug_mesh.instantiate();
	}

	debug_mesh->clear_surfaces();

	Vector<Vector3> vertices = navigation_mesh->get_vertices();
	if (vertices.size() == 0) {
		return;
	}

	int polygon_count = navigation_mesh->get_polygon_count();
	if (polygon_count == 0) {
		return;
	}

	bool enabled_geometry_face_random_color = NavigationServer3D::get_singleton()->get_debug_navigation_enable_geometry_face_random_color();
	bool enabled_edge_lines = NavigationServer3D::get_singleton()->get_debug_navigation_enable_edge_lines();

	int vertex_count = 0;
	int line_count = 0;

	for (int i = 0; i < polygon_count; i++) {
		const Vector<int> &polygon = navigation_mesh->get_polygon(i);
		int polygon_size = polygon.size();
		if (polygon_size < 3) {
			continue;
		}
		line_count += polygon_size * 2;
		vertex_count += (polygon_size - 2) * 3;
	}

	Vector<Vector3> face_vertex_array;
	face_vertex_array.resize(vertex_count);

	Vector<Color> face_color_array;
	if (enabled_geometry_face_random_color) {
		face_color_array.resize(vertex_count);
	}

	Vector<Vector3> line_vertex_array;
	if (enabled_edge_lines) {
		line_vertex_array.resize(line_count);
	}

	Color debug_navigation_geometry_face_color = NavigationServer3D::get_singleton()->get_debug_navigation_geometry_face_color();

	RandomPCG rand;
	Color polygon_color = debug_navigation_geometry_face_color;

	int face_vertex_index = 0;
	int line_vertex_index = 0;

	Vector3 *face_vertex_array_ptrw = face_vertex_array.ptrw();
	Color *face_color_array_ptrw = face_color_array.ptrw();
	Vector3 *line_vertex_array_ptrw = line_vertex_array.ptrw();

	for (int polygon_index = 0; polygon_index < polygon_count; polygon_index++) {
		const Vector<int> &polygon_indices = navigation_mesh->get_polygon(polygon_index);
		int polygon_indices_size = polygon_indices.size();
		if (polygon_indices_size < 3) {
			continue;
		}

		if (enabled_geometry_face_random_color) {
			// Generate the polygon color, slightly randomly modified from the settings one.
			polygon_color.set_hsv(debug_navigation_geometry_face_color.get_h() + rand.random(-1.0, 1.0) * 0.1, debug_navigation_geometry_face_color.get_s(), debug_navigation_geometry_face_color.get_v() + rand.random(-1.0, 1.0) * 0.2);
			polygon_color.a = debug_navigation_geometry_face_color.a;
		}

		for (int polygon_indices_index = 0; polygon_indices_index < polygon_indices_size - 2; polygon_indices_index++) {
			face_vertex_array_ptrw[face_vertex_index] = vertices[polygon_indices[0]];
			face_vertex_array_ptrw[face_vertex_index + 1] = vertices[polygon_indices[polygon_indices_index + 1]];
			face_vertex_array_ptrw[face_vertex_index + 2] = vertices[polygon_indices[polygon_indices_index + 2]];
			if (enabled_geometry_face_random_color) {
				face_color_array_ptrw[face_vertex_index] = polygon_color;
				face_color_array_ptrw[face_vertex_index + 1] = polygon_color;
				face_color_array_ptrw[face_vertex_index + 2] = polygon_color;
			}
			face_vertex_index += 3;
		}

		if (enabled_edge_lines) {
			for (int polygon_indices_index = 0; polygon_indices_index < polygon_indices_size; polygon_indices_index++) {
				line_vertex_array_ptrw[line_vertex_index] = vertices[polygon_indices[polygon_indices_index]];
				line_vertex_index += 1;
				if (polygon_indices_index + 1 == polygon_indices_size) {
					line_vertex_array_ptrw[line_vertex_index] = vertices[polygon_indices[0]];
					line_vertex_index += 1;
				} else {
					line_vertex_array_ptrw[line_vertex_index] = vertices[polygon_indices[polygon_indices_index + 1]];
					line_vertex_index += 1;
				}
			}
		}
	}

	Ref<StandardMaterial3D> face_material = NavigationServer3D::get_singleton()->get_debug_navigation_geometry_face_material();

	Array face_mesh_array;
	face_mesh_array.resize(Mesh::ARRAY_MAX);
	face_mesh_array[Mesh::ARRAY_VERTEX] = face_vertex_array;
	if (enabled_geometry_face_random_color) {
		face_mesh_array[Mesh::ARRAY_COLOR] = face_color_array;
	}
	debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, face_mesh_array);
	debug_mesh->surface_set_material(0, face_material);

	if (enabled_edge_lines) {
		Ref<StandardMaterial3D> line_material = NavigationServer3D::get_singleton()->get_debug_navigation_geometry_edge_material();

		Array line_mesh_array;
		line_mesh_array.resize(Mesh::ARRAY_MAX);
		line_mesh_array[Mesh::ARRAY_VERTEX] = line_vertex_array;
		debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, line_mesh_array);
		debug_mesh->surface_set_material(1, line_material);
	}

	RS::get_singleton()->instance_set_base(debug_instance, debug_mesh->get_rid());
	if (is_inside_tree()) {
		RS::get_singleton()->instance_set_scenario(debug_instance, get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_transform(debug_instance, current_global_transform);
		RS::get_singleton()->instance_set_visible(debug_instance, is_visible_in_tree());
	}
	if (!is_enabled()) {
		if (debug_mesh.is_valid()) {
			if (debug_mesh->get_surface_count() > 0) {
				RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, NavigationServer3D::get_singleton()->get_debug_navigation_geometry_face_disabled_material()->get_rid());
			}
			if (debug_mesh->get_surface_count() > 1) {
				RS::get_singleton()->instance_set_surface_override_material(debug_instance, 1, NavigationServer3D::get_singleton()->get_debug_navigation_geometry_edge_disabled_material()->get_rid());
			}
		}
	} else {
		if (debug_mesh.is_valid()) {
			if (debug_mesh->get_surface_count() > 0) {
				RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, RID());
			}
			if (debug_mesh->get_surface_count() > 1) {
				RS::get_singleton()->instance_set_surface_override_material(debug_instance, 1, RID());
			}
		}
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationRegion3D::_update_debug_edge_connections_mesh() {
	if (!NavigationServer3D::get_singleton()->get_debug_enabled() || !NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		if (debug_edge_connections_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
		}
		return;
	}

	if (!is_inside_tree()) {
		return;
	}

	if (!use_edge_connections || !NavigationServer3D::get_singleton()->map_get_use_edge_connections(get_world_3d()->get_navigation_map())) {
		if (debug_edge_connections_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
		}
		return;
	}

	if (navigation_mesh.is_null()) {
		if (debug_edge_connections_instance.is_valid()) {
			RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
		}
		return;
	}

	if (!debug_edge_connections_instance.is_valid()) {
		debug_edge_connections_instance = RenderingServer::get_singleton()->instance_create();
	}

	if (debug_edge_connections_mesh.is_null()) {
		debug_edge_connections_mesh.instantiate();
	}

	debug_edge_connections_mesh->clear_surfaces();

	float edge_connection_margin = NavigationServer3D::get_singleton()->map_get_edge_connection_margin(get_world_3d()->get_navigation_map());
	float half_edge_connection_margin = edge_connection_margin * 0.5;
	int connections_count = NavigationServer3D::get_singleton()->region_get_connections_count(region);

	if (connections_count == 0) {
		RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
		return;
	}

	Vector<Vector3> vertex_array;
	vertex_array.resize(connections_count * 6);
	Vector3 *vertex_array_ptrw = vertex_array.ptrw();
	int vertex_array_index = 0;

	for (int i = 0; i < connections_count; i++) {
		Vector3 connection_pathway_start = NavigationServer3D::get_singleton()->region_get_connection_pathway_start(region, i);
		Vector3 connection_pathway_end = NavigationServer3D::get_singleton()->region_get_connection_pathway_end(region, i);

		Vector3 direction_start_end = connection_pathway_start.direction_to(connection_pathway_end);
		Vector3 direction_end_start = connection_pathway_end.direction_to(connection_pathway_start);

		Vector3 start_right_dir = direction_start_end.cross(Vector3(0, 1, 0));
		Vector3 start_left_dir = -start_right_dir;

		Vector3 end_right_dir = direction_end_start.cross(Vector3(0, 1, 0));
		Vector3 end_left_dir = -end_right_dir;

		Vector3 left_start_pos = connection_pathway_start + (start_left_dir * half_edge_connection_margin);
		Vector3 right_start_pos = connection_pathway_start + (start_right_dir * half_edge_connection_margin);
		Vector3 left_end_pos = connection_pathway_end + (end_right_dir * half_edge_connection_margin);
		Vector3 right_end_pos = connection_pathway_end + (end_left_dir * half_edge_connection_margin);

		vertex_array_ptrw[vertex_array_index++] = connection_pathway_start;
		vertex_array_ptrw[vertex_array_index++] = connection_pathway_end;
		vertex_array_ptrw[vertex_array_index++] = left_start_pos;
		vertex_array_ptrw[vertex_array_index++] = right_start_pos;
		vertex_array_ptrw[vertex_array_index++] = left_end_pos;
		vertex_array_ptrw[vertex_array_index++] = right_end_pos;
	}

	if (vertex_array.size() == 0) {
		return;
	}

	Ref<StandardMaterial3D> edge_connections_material = NavigationServer3D::get_singleton()->get_debug_navigation_edge_connections_material();

	Array mesh_array;
	mesh_array.resize(Mesh::ARRAY_MAX);
	mesh_array[Mesh::ARRAY_VERTEX] = vertex_array;

	debug_edge_connections_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, mesh_array);
	debug_edge_connections_mesh->surface_set_material(0, edge_connections_material);

	RS::get_singleton()->instance_set_base(debug_edge_connections_instance, debug_edge_connections_mesh->get_rid());
	RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, is_visible_in_tree());
	if (is_inside_tree()) {
		RS::get_singleton()->instance_set_scenario(debug_edge_connections_instance, get_world_3d()->get_scenario());
	}

	bool enable_edge_connections = NavigationServer3D::get_singleton()->get_debug_navigation_enable_edge_connections();
	if (!enable_edge_connections) {
		RS::get_singleton()->instance_set_visible(debug_edge_connections_instance, false);
	}
}
#endif // DEBUG_ENABLED
