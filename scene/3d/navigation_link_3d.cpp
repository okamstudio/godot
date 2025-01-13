/**************************************************************************/
/*  navigation_link_3d.cpp                                                */
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

#include "navigation_link_3d.h"

#include "servers/navigation_server_3d.h"

#ifdef DEBUG_ENABLED
void NavigationLink3D::_update_debug_mesh() {
	if (!is_inside_tree()) {
		return;
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		// don't update inside Editor as node 3d gizmo takes care of this
		// as collisions and selections for Editor Viewport need to be updated
		return;
	}

	if (!NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
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

	RID nav_map = get_world_3d()->get_navigation_map();
	real_t search_radius = NavigationServer3D::get_singleton()->map_get_link_connection_radius(nav_map);
	Vector3 up_vector = NavigationServer3D::get_singleton()->map_get_up(nav_map);
	Vector3::Axis up_axis = up_vector.max_axis_index();

	debug_mesh->clear_surfaces();

	Vector<Vector3> lines;

	// Draw line between the points.
	lines.push_back(start_position);
	lines.push_back(end_position);

	// Draw start position search radius
	for (int i = 0; i < 30; i++) {
		// Create a circle
		const float ra = Math::deg_to_rad((float)(i * 12));
		const float rb = Math::deg_to_rad((float)((i + 1) * 12));
		const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * search_radius;
		const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * search_radius;

		// Draw axis-aligned circle
		switch (up_axis) {
			case Vector3::AXIS_X:
				lines.append(start_position + Vector3(0, a.x, a.y));
				lines.append(start_position + Vector3(0, b.x, b.y));
				break;
			case Vector3::AXIS_Y:
				lines.append(start_position + Vector3(a.x, 0, a.y));
				lines.append(start_position + Vector3(b.x, 0, b.y));
				break;
			case Vector3::AXIS_Z:
				lines.append(start_position + Vector3(a.x, a.y, 0));
				lines.append(start_position + Vector3(b.x, b.y, 0));
				break;
		}
	}

	// Draw end position search radius
	for (int i = 0; i < 30; i++) {
		// Create a circle
		const float ra = Math::deg_to_rad((float)(i * 12));
		const float rb = Math::deg_to_rad((float)((i + 1) * 12));
		const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * search_radius;
		const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * search_radius;

		// Draw axis-aligned circle
		switch (up_axis) {
			case Vector3::AXIS_X:
				lines.append(end_position + Vector3(0, a.x, a.y));
				lines.append(end_position + Vector3(0, b.x, b.y));
				break;
			case Vector3::AXIS_Y:
				lines.append(end_position + Vector3(a.x, 0, a.y));
				lines.append(end_position + Vector3(b.x, 0, b.y));
				break;
			case Vector3::AXIS_Z:
				lines.append(end_position + Vector3(a.x, a.y, 0));
				lines.append(end_position + Vector3(b.x, b.y, 0));
				break;
		}
	}

	const Vector3 link_segment = end_position - start_position;
	const Vector3 up = Vector3(0.0, 1.0, 0.0);
	const float arror_len = 0.5;

	{
		Vector3 anchor = start_position + (link_segment * 0.75);
		Vector3 direction = start_position.direction_to(end_position);
		Vector3 arrow_dir = direction.cross(up);
		lines.push_back(anchor);
		lines.push_back(anchor + (arrow_dir - direction) * arror_len);

		arrow_dir = -direction.cross(up);
		lines.push_back(anchor);
		lines.push_back(anchor + (arrow_dir - direction) * arror_len);
	}

	if (is_bidirectional()) {
		Vector3 anchor = start_position + (link_segment * 0.25);
		Vector3 direction = end_position.direction_to(start_position);
		Vector3 arrow_dir = direction.cross(up);
		lines.push_back(anchor);
		lines.push_back(anchor + (arrow_dir - direction) * arror_len);

		arrow_dir = -direction.cross(up);
		lines.push_back(anchor);
		lines.push_back(anchor + (arrow_dir - direction) * arror_len);
	}

	Array mesh_array;
	mesh_array.resize(Mesh::ARRAY_MAX);
	mesh_array[Mesh::ARRAY_VERTEX] = lines;

	debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, mesh_array);

	RS::get_singleton()->instance_set_base(debug_instance, debug_mesh->get_rid());
	RS::get_singleton()->instance_set_scenario(debug_instance, get_world_3d()->get_scenario());
	RS::get_singleton()->instance_set_visible(debug_instance, is_visible_in_tree());

	Ref<StandardMaterial3D> link_material = NavigationServer3D::get_singleton()->get_debug_navigation_link_connections_material();
	Ref<StandardMaterial3D> disabled_link_material = NavigationServer3D::get_singleton()->get_debug_navigation_link_connections_disabled_material();

	if (enabled) {
		RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, link_material->get_rid());
	} else {
		RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, disabled_link_material->get_rid());
	}

	RS::get_singleton()->instance_set_transform(debug_instance, current_global_transform);
}
#endif // DEBUG_ENABLED

void NavigationLink3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationLink3D::get_rid);

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &NavigationLink3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &NavigationLink3D::is_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationLink3D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationLink3D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_bidirectional", "bidirectional"), &NavigationLink3D::set_bidirectional);
	ClassDB::bind_method(D_METHOD("is_bidirectional"), &NavigationLink3D::is_bidirectional);

	ClassDB::bind_method(D_METHOD("set_navigation_layers", "navigation_layers"), &NavigationLink3D::set_navigation_layers);
	ClassDB::bind_method(D_METHOD("get_navigation_layers"), &NavigationLink3D::get_navigation_layers);

	ClassDB::bind_method(D_METHOD("set_navigation_layer_value", "layer_number", "value"), &NavigationLink3D::set_navigation_layer_value);
	ClassDB::bind_method(D_METHOD("get_navigation_layer_value", "layer_number"), &NavigationLink3D::get_navigation_layer_value);

	ClassDB::bind_method(D_METHOD("set_start_position", "position"), &NavigationLink3D::set_start_position);
	ClassDB::bind_method(D_METHOD("get_start_position"), &NavigationLink3D::get_start_position);

	ClassDB::bind_method(D_METHOD("set_end_position", "position"), &NavigationLink3D::set_end_position);
	ClassDB::bind_method(D_METHOD("get_end_position"), &NavigationLink3D::get_end_position);

	ClassDB::bind_method(D_METHOD("set_global_start_position", "position"), &NavigationLink3D::set_global_start_position);
	ClassDB::bind_method(D_METHOD("get_global_start_position"), &NavigationLink3D::get_global_start_position);

	ClassDB::bind_method(D_METHOD("set_global_end_position", "position"), &NavigationLink3D::set_global_end_position);
	ClassDB::bind_method(D_METHOD("get_global_end_position"), &NavigationLink3D::get_global_end_position);

	ClassDB::bind_method(D_METHOD("set_enter_cost", "enter_cost"), &NavigationLink3D::set_enter_cost);
	ClassDB::bind_method(D_METHOD("get_enter_cost"), &NavigationLink3D::get_enter_cost);

	ClassDB::bind_method(D_METHOD("set_travel_cost", "travel_cost"), &NavigationLink3D::set_travel_cost);
	ClassDB::bind_method(D_METHOD("get_travel_cost"), &NavigationLink3D::get_travel_cost);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bidirectional"), "set_bidirectional", "is_bidirectional");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_layers", PROPERTY_HINT_LAYERS_3D_NAVIGATION), "set_navigation_layers", "get_navigation_layers");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "start_position"), "set_start_position", "get_start_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "end_position"), "set_end_position", "get_end_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "enter_cost"), "set_enter_cost", "get_enter_cost");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "travel_cost"), "set_travel_cost", "get_travel_cost");
}

#ifndef DISABLE_DEPRECATED
bool NavigationLink3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "start_location") {
		set_start_position(p_value);
		return true;
	}
	if (p_name == "end_location") {
		set_end_position(p_value);
		return true;
	}
	return false;
}

bool NavigationLink3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "start_location") {
		r_ret = get_start_position();
		return true;
	}
	if (p_name == "end_location") {
		r_ret = get_end_position();
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

void NavigationLink3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_link_enter_navigation_map();
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			set_physics_process_internal(true);
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			set_physics_process_internal(false);
			_link_update_transform();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_link_exit_navigation_map();
		} break;
	}
}

NavigationLink3D::NavigationLink3D() {
	link = NavigationServer3D::get_singleton()->link_create();

	NavigationServer3D::get_singleton()->link_set_owner_id(link, get_instance_id());
	NavigationServer3D::get_singleton()->link_set_enter_cost(link, enter_cost);
	NavigationServer3D::get_singleton()->link_set_travel_cost(link, travel_cost);
	NavigationServer3D::get_singleton()->link_set_navigation_layers(link, navigation_layers);
	NavigationServer3D::get_singleton()->link_set_bidirectional(link, bidirectional);
	NavigationServer3D::get_singleton()->link_set_enabled(link, enabled);

	set_notify_transform(true);
}

NavigationLink3D::~NavigationLink3D() {
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	NavigationServer3D::get_singleton()->free(link);
	link = RID();

#ifdef DEBUG_ENABLED
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (debug_instance.is_valid()) {
		RenderingServer::get_singleton()->free(debug_instance);
	}
	if (debug_mesh.is_valid()) {
		RenderingServer::get_singleton()->free(debug_mesh->get_rid());
	}
#endif // DEBUG_ENABLED
}

RID NavigationLink3D::get_rid() const {
	return link;
}

void NavigationLink3D::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}

	enabled = p_enabled;

	NavigationServer3D::get_singleton()->link_set_enabled(link, enabled);

#ifdef DEBUG_ENABLED
	if (debug_instance.is_valid() && debug_mesh.is_valid()) {
		if (enabled) {
			Ref<StandardMaterial3D> link_material = NavigationServer3D::get_singleton()->get_debug_navigation_link_connections_material();
			RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, link_material->get_rid());
		} else {
			Ref<StandardMaterial3D> disabled_link_material = NavigationServer3D::get_singleton()->get_debug_navigation_link_connections_disabled_material();
			RS::get_singleton()->instance_set_surface_override_material(debug_instance, 0, disabled_link_material->get_rid());
		}
	}
#endif // DEBUG_ENABLED

	update_gizmos();
}

void NavigationLink3D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}

	map_override = p_navigation_map;

	NavigationServer3D::get_singleton()->link_set_map(link, map_override);
}

RID NavigationLink3D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_3d()->get_navigation_map();
	}
	return RID();
}

void NavigationLink3D::set_bidirectional(bool p_bidirectional) {
	if (bidirectional == p_bidirectional) {
		return;
	}

	bidirectional = p_bidirectional;

	NavigationServer3D::get_singleton()->link_set_bidirectional(link, bidirectional);

#ifdef DEBUG_ENABLED
	_update_debug_mesh();
#endif // DEBUG_ENABLED

	update_gizmos();
}

void NavigationLink3D::set_navigation_layers(uint32_t p_navigation_layers) {
	if (navigation_layers == p_navigation_layers) {
		return;
	}

	navigation_layers = p_navigation_layers;

	NavigationServer3D::get_singleton()->link_set_navigation_layers(link, navigation_layers);
}

void NavigationLink3D::set_navigation_layer_value(int p_layer_number, bool p_value) {
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

bool NavigationLink3D::get_navigation_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Navigation layer number must be between 1 and 32 inclusive.");

	return get_navigation_layers() & (1 << (p_layer_number - 1));
}

void NavigationLink3D::set_start_position(Vector3 p_position) {
	if (start_position.is_equal_approx(p_position)) {
		return;
	}

	start_position = p_position;

	if (!is_inside_tree()) {
		return;
	}

	NavigationServer3D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));

#ifdef DEBUG_ENABLED
	_update_debug_mesh();
#endif // DEBUG_ENABLED

	update_gizmos();
	update_configuration_warnings();
}

void NavigationLink3D::set_end_position(Vector3 p_position) {
	if (end_position.is_equal_approx(p_position)) {
		return;
	}

	end_position = p_position;

	if (!is_inside_tree()) {
		return;
	}

	NavigationServer3D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));

#ifdef DEBUG_ENABLED
	_update_debug_mesh();
#endif // DEBUG_ENABLED

	update_gizmos();
	update_configuration_warnings();
}

void NavigationLink3D::set_global_start_position(Vector3 p_position) {
	if (is_inside_tree()) {
		set_start_position(to_local(p_position));
	} else {
		set_start_position(p_position);
	}
}

Vector3 NavigationLink3D::get_global_start_position() const {
	if (is_inside_tree()) {
		return to_global(start_position);
	} else {
		return start_position;
	}
}

void NavigationLink3D::set_global_end_position(Vector3 p_position) {
	if (is_inside_tree()) {
		set_end_position(to_local(p_position));
	} else {
		set_end_position(p_position);
	}
}

Vector3 NavigationLink3D::get_global_end_position() const {
	if (is_inside_tree()) {
		return to_global(end_position);
	} else {
		return end_position;
	}
}

void NavigationLink3D::set_enter_cost(real_t p_enter_cost) {
	ERR_FAIL_COND_MSG(p_enter_cost < 0.0, "The enter_cost must be positive.");
	if (Math::is_equal_approx(enter_cost, p_enter_cost)) {
		return;
	}

	enter_cost = p_enter_cost;

	NavigationServer3D::get_singleton()->link_set_enter_cost(link, enter_cost);
}

void NavigationLink3D::set_travel_cost(real_t p_travel_cost) {
	ERR_FAIL_COND_MSG(p_travel_cost < 0.0, "The travel_cost must be positive.");
	if (Math::is_equal_approx(travel_cost, p_travel_cost)) {
		return;
	}

	travel_cost = p_travel_cost;

	NavigationServer3D::get_singleton()->link_set_travel_cost(link, travel_cost);
}

PackedStringArray NavigationLink3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (start_position.is_equal_approx(end_position)) {
		warnings.push_back(RTR("NavigationLink3D start position should be different than the end position to be useful."));
	}

	return warnings;
}

void NavigationLink3D::_link_enter_navigation_map() {
	if (!is_inside_tree()) {
		return;
	}

	if (map_override.is_valid()) {
		NavigationServer3D::get_singleton()->link_set_map(link, map_override);
	} else {
		NavigationServer3D::get_singleton()->link_set_map(link, get_world_3d()->get_navigation_map());
	}

	current_global_transform = get_global_transform();
	NavigationServer3D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));
	NavigationServer3D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));
	NavigationServer3D::get_singleton()->link_set_enabled(link, enabled);

#ifdef DEBUG_ENABLED
	if (NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
		_update_debug_mesh();
	}
#endif // DEBUG_ENABLED
}

void NavigationLink3D::_link_exit_navigation_map() {
	NavigationServer3D::get_singleton()->link_set_map(link, RID());
#ifdef DEBUG_ENABLED
	if (debug_instance.is_valid()) {
		RS::get_singleton()->instance_set_visible(debug_instance, false);
	}
#endif // DEBUG_ENABLED
}

void NavigationLink3D::_link_update_transform() {
	if (!is_inside_tree()) {
		return;
	}

	Transform3D new_global_transform = get_global_transform();
	if (current_global_transform != new_global_transform) {
		current_global_transform = new_global_transform;
		NavigationServer3D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));
		NavigationServer3D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));
#ifdef DEBUG_ENABLED
		if (NavigationServer3D::get_singleton()->get_debug_navigation_enabled()) {
			_update_debug_mesh();
		}
#endif // DEBUG_ENABLED
	}
}
