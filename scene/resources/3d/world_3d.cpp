/**************************************************************************/
/*  world_3d.cpp                                                          */
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

#include "world_3d.h"

#include "core/config/project_settings.h"
#include "scene/3d/camera_3d.h"
#include "scene/resources/camera_attributes.h"
#include "scene/resources/environment.h"
#include "servers/navigation_server_3d.h"

void World3D::_register_camera(Camera3D *p_camera) {
	cameras.insert(p_camera);
}

void World3D::_remove_camera(Camera3D *p_camera) {
	cameras.erase(p_camera);
}

RID World3D::get_space() const {
	if (space.is_null()) {
		space = PhysicsServer3D::get_singleton()->space_create();
		PhysicsServer3D::get_singleton()->space_set_active(space, true);
		PhysicsServer3D::get_singleton()->area_set_param(space, PhysicsServer3D::AREA_PARAM_GRAVITY, GLOBAL_GET("physics/3d/default_gravity"));
		PhysicsServer3D::get_singleton()->area_set_param(space, PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR, GLOBAL_GET("physics/3d/default_gravity_vector"));
		PhysicsServer3D::get_singleton()->area_set_param(space, PhysicsServer3D::AREA_PARAM_LINEAR_DAMP, GLOBAL_GET("physics/3d/default_linear_damp"));
		PhysicsServer3D::get_singleton()->area_set_param(space, PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP, GLOBAL_GET("physics/3d/default_angular_damp"));
	}
	return space;
}

RID World3D::get_navigation_map() const {
	if (navigation_map.is_null()) {
		navigation_map = NavigationServer3D::get_singleton()->map_create();
		NavigationServer3D::get_singleton()->map_set_active(navigation_map, true);
		NavigationServer3D::get_singleton()->map_set_cell_size(navigation_map, GLOBAL_GET("navigation/3d/default_cell_size"));
		NavigationServer3D::get_singleton()->map_set_cell_height(navigation_map, GLOBAL_GET("navigation/3d/default_cell_height"));
		NavigationServer3D::get_singleton()->map_set_up(navigation_map, GLOBAL_GET("navigation/3d/default_up"));
		NavigationServer3D::get_singleton()->map_set_merge_rasterizer_cell_scale(navigation_map, GLOBAL_GET("navigation/3d/merge_rasterizer_cell_scale"));
		NavigationServer3D::get_singleton()->map_set_use_edge_connections(navigation_map, GLOBAL_GET("navigation/3d/use_edge_connections"));
		NavigationServer3D::get_singleton()->map_set_edge_connection_margin(navigation_map, GLOBAL_GET("navigation/3d/default_edge_connection_margin"));
		NavigationServer3D::get_singleton()->map_set_link_connection_radius(navigation_map, GLOBAL_GET("navigation/3d/default_link_connection_radius"));
	}
	return navigation_map;
}

RID World3D::get_scenario() const {
	return scenario;
}

void World3D::set_environment(const Ref<Environment> &p_environment) {
	if (environment == p_environment) {
		return;
	}

	environment = p_environment;
	if (environment.is_valid()) {
		RS::get_singleton()->scenario_set_environment(scenario, environment->get_rid());
	} else {
		RS::get_singleton()->scenario_set_environment(scenario, RID());
	}

	emit_changed();
}

Ref<Environment> World3D::get_environment() const {
	return environment;
}

void World3D::set_fallback_environment(const Ref<Environment> &p_environment) {
	if (fallback_environment == p_environment) {
		return;
	}

	fallback_environment = p_environment;
	if (fallback_environment.is_valid()) {
		RS::get_singleton()->scenario_set_fallback_environment(scenario, p_environment->get_rid());
	} else {
		RS::get_singleton()->scenario_set_fallback_environment(scenario, RID());
	}

	emit_changed();
}

Ref<Environment> World3D::get_fallback_environment() const {
	return fallback_environment;
}

void World3D::set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes) {
	camera_attributes = p_camera_attributes;
	if (camera_attributes.is_valid()) {
		RS::get_singleton()->scenario_set_camera_attributes(scenario, camera_attributes->get_rid());
	} else {
		RS::get_singleton()->scenario_set_camera_attributes(scenario, RID());
	}
}

Ref<CameraAttributes> World3D::get_camera_attributes() const {
	return camera_attributes;
}

void World3D::set_compositor(const Ref<Compositor> &p_compositor) {
	compositor = p_compositor;
	if (compositor.is_valid()) {
		RS::get_singleton()->scenario_set_compositor(scenario, compositor->get_rid());
	} else {
		RS::get_singleton()->scenario_set_compositor(scenario, RID());
	}
}

Ref<Compositor> World3D::get_compositor() const {
	return compositor;
}

PhysicsDirectSpaceState3D *World3D::get_direct_space_state() {
	return PhysicsServer3D::get_singleton()->space_get_direct_state(get_space());
}

void World3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_space"), &World3D::get_space);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &World3D::get_navigation_map);
	ClassDB::bind_method(D_METHOD("get_scenario"), &World3D::get_scenario);
	ClassDB::bind_method(D_METHOD("set_environment", "env"), &World3D::set_environment);
	ClassDB::bind_method(D_METHOD("get_environment"), &World3D::get_environment);
	ClassDB::bind_method(D_METHOD("set_fallback_environment", "env"), &World3D::set_fallback_environment);
	ClassDB::bind_method(D_METHOD("get_fallback_environment"), &World3D::get_fallback_environment);
	ClassDB::bind_method(D_METHOD("set_camera_attributes", "attributes"), &World3D::set_camera_attributes);
	ClassDB::bind_method(D_METHOD("get_camera_attributes"), &World3D::get_camera_attributes);
	ClassDB::bind_method(D_METHOD("get_direct_space_state"), &World3D::get_direct_space_state);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_environment", "get_environment");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_fallback_environment", "get_fallback_environment");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "camera_attributes", PROPERTY_HINT_RESOURCE_TYPE, "CameraAttributesPractical,CameraAttributesPhysical"), "set_camera_attributes", "get_camera_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::RID, "space", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_space");
	ADD_PROPERTY(PropertyInfo(Variant::RID, "navigation_map", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_navigation_map");
	ADD_PROPERTY(PropertyInfo(Variant::RID, "scenario", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_scenario");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "direct_space_state", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsDirectSpaceState3D", PROPERTY_USAGE_NONE), "", "get_direct_space_state");
}

World3D::World3D() {
	scenario = RenderingServer::get_singleton()->scenario_create();
}

World3D::~World3D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	ERR_FAIL_NULL(PhysicsServer3D::get_singleton());
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());

	RenderingServer::get_singleton()->free(scenario);
	if (space.is_valid()) {
		PhysicsServer3D::get_singleton()->free(space);
	}
	if (navigation_map.is_valid()) {
		NavigationServer3D::get_singleton()->free(navigation_map);
	}
}
