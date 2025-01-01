/**************************************************************************/
/*  navigation_link_2d.cpp                                                */
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

#include "navigation_link_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/navigation_server_2d.h"

void NavigationLink2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationLink2D::get_rid);

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &NavigationLink2D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &NavigationLink2D::is_enabled);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationLink2D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationLink2D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_bidirectional", "bidirectional"), &NavigationLink2D::set_bidirectional);
	ClassDB::bind_method(D_METHOD("is_bidirectional"), &NavigationLink2D::is_bidirectional);

	ClassDB::bind_method(D_METHOD("set_navigation_layers", "navigation_layers"), &NavigationLink2D::set_navigation_layers);
	ClassDB::bind_method(D_METHOD("get_navigation_layers"), &NavigationLink2D::get_navigation_layers);

	ClassDB::bind_method(D_METHOD("set_navigation_layer_value", "layer_number", "value"), &NavigationLink2D::set_navigation_layer_value);
	ClassDB::bind_method(D_METHOD("get_navigation_layer_value", "layer_number"), &NavigationLink2D::get_navigation_layer_value);

	ClassDB::bind_method(D_METHOD("set_start_position", "position"), &NavigationLink2D::set_start_position);
	ClassDB::bind_method(D_METHOD("get_start_position"), &NavigationLink2D::get_start_position);

	ClassDB::bind_method(D_METHOD("set_end_position", "position"), &NavigationLink2D::set_end_position);
	ClassDB::bind_method(D_METHOD("get_end_position"), &NavigationLink2D::get_end_position);

	ClassDB::bind_method(D_METHOD("set_global_start_position", "position"), &NavigationLink2D::set_global_start_position);
	ClassDB::bind_method(D_METHOD("get_global_start_position"), &NavigationLink2D::get_global_start_position);

	ClassDB::bind_method(D_METHOD("set_global_end_position", "position"), &NavigationLink2D::set_global_end_position);
	ClassDB::bind_method(D_METHOD("get_global_end_position"), &NavigationLink2D::get_global_end_position);

	ClassDB::bind_method(D_METHOD("set_enter_cost", "enter_cost"), &NavigationLink2D::set_enter_cost);
	ClassDB::bind_method(D_METHOD("get_enter_cost"), &NavigationLink2D::get_enter_cost);

	ClassDB::bind_method(D_METHOD("set_travel_cost", "travel_cost"), &NavigationLink2D::set_travel_cost);
	ClassDB::bind_method(D_METHOD("get_travel_cost"), &NavigationLink2D::get_travel_cost);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bidirectional"), "set_bidirectional", "is_bidirectional");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_layers", PROPERTY_HINT_LAYERS_2D_NAVIGATION), "set_navigation_layers", "get_navigation_layers");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "start_position"), "set_start_position", "get_start_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "end_position"), "set_end_position", "get_end_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "enter_cost"), "set_enter_cost", "get_enter_cost");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "travel_cost"), "set_travel_cost", "get_travel_cost");
}

#ifndef DISABLE_DEPRECATED
bool NavigationLink2D::_set(const StringName &p_name, const Variant &p_value) {
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

bool NavigationLink2D::_get(const StringName &p_name, Variant &r_ret) const {
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

void NavigationLink2D::_notification(int p_what) {
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
		case NOTIFICATION_DRAW: {
#ifdef DEBUG_ENABLED
			_update_debug_mesh();
#endif // DEBUG_ENABLED
		} break;
	}
}

#ifdef DEBUG_ENABLED
Rect2 NavigationLink2D::_edit_get_rect() const {
	if (!is_inside_tree()) {
		return Rect2();
	}

	real_t radius = NavigationServer2D::get_singleton()->map_get_link_connection_radius(get_world_2d()->get_navigation_map());

	Rect2 rect(get_start_position(), Size2());
	rect.expand_to(get_end_position());
	rect.grow_by(radius);
	return rect;
}

bool NavigationLink2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	Point2 segment[2] = { get_start_position(), get_end_position() };

	Vector2 closest_point = Geometry2D::get_closest_point_to_segment(p_point, segment);
	return p_point.distance_to(closest_point) < p_tolerance;
}
#endif // DEBUG_ENABLED

RID NavigationLink2D::get_rid() const {
	return link;
}

void NavigationLink2D::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}

	enabled = p_enabled;

	NavigationServer2D::get_singleton()->link_set_enabled(link, enabled);

#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationLink2D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}

	map_override = p_navigation_map;

	NavigationServer2D::get_singleton()->link_set_map(link, map_override);
}

RID NavigationLink2D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (is_inside_tree()) {
		return get_world_2d()->get_navigation_map();
	}
	return RID();
}

void NavigationLink2D::set_bidirectional(bool p_bidirectional) {
	if (bidirectional == p_bidirectional) {
		return;
	}

	bidirectional = p_bidirectional;

	NavigationServer2D::get_singleton()->link_set_bidirectional(link, bidirectional);
}

void NavigationLink2D::set_navigation_layers(uint32_t p_navigation_layers) {
	if (navigation_layers == p_navigation_layers) {
		return;
	}

	navigation_layers = p_navigation_layers;

	NavigationServer2D::get_singleton()->link_set_navigation_layers(link, navigation_layers);
}

void NavigationLink2D::set_navigation_layer_value(int p_layer_number, bool p_value) {
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

bool NavigationLink2D::get_navigation_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Navigation layer number must be between 1 and 32 inclusive.");

	return get_navigation_layers() & (1 << (p_layer_number - 1));
}

void NavigationLink2D::set_start_position(Vector2 p_position) {
	if (start_position.is_equal_approx(p_position)) {
		return;
	}

	start_position = p_position;

	if (!is_inside_tree()) {
		return;
	}

	NavigationServer2D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));

	update_configuration_warnings();

#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationLink2D::set_end_position(Vector2 p_position) {
	if (end_position.is_equal_approx(p_position)) {
		return;
	}

	end_position = p_position;

	if (!is_inside_tree()) {
		return;
	}

	NavigationServer2D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));

	update_configuration_warnings();

#ifdef DEBUG_ENABLED
	queue_redraw();
#endif // DEBUG_ENABLED
}

void NavigationLink2D::set_global_start_position(Vector2 p_position) {
	if (is_inside_tree()) {
		set_start_position(to_local(p_position));
	} else {
		set_start_position(p_position);
	}
}

Vector2 NavigationLink2D::get_global_start_position() const {
	if (is_inside_tree()) {
		return to_global(start_position);
	} else {
		return start_position;
	}
}

void NavigationLink2D::set_global_end_position(Vector2 p_position) {
	if (is_inside_tree()) {
		set_end_position(to_local(p_position));
	} else {
		set_end_position(p_position);
	}
}

Vector2 NavigationLink2D::get_global_end_position() const {
	if (is_inside_tree()) {
		return to_global(end_position);
	} else {
		return end_position;
	}
}

void NavigationLink2D::set_enter_cost(real_t p_enter_cost) {
	ERR_FAIL_COND_MSG(p_enter_cost < 0.0, "The enter_cost must be positive.");
	if (Math::is_equal_approx(enter_cost, p_enter_cost)) {
		return;
	}

	enter_cost = p_enter_cost;

	NavigationServer2D::get_singleton()->link_set_enter_cost(link, enter_cost);
}

void NavigationLink2D::set_travel_cost(real_t p_travel_cost) {
	ERR_FAIL_COND_MSG(p_travel_cost < 0.0, "The travel_cost must be positive.");
	if (Math::is_equal_approx(travel_cost, p_travel_cost)) {
		return;
	}

	travel_cost = p_travel_cost;

	NavigationServer2D::get_singleton()->link_set_travel_cost(link, travel_cost);
}

PackedStringArray NavigationLink2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (start_position.is_equal_approx(end_position)) {
		warnings.push_back(RTR("NavigationLink2D start position should be different than the end position to be useful."));
	}

	return warnings;
}

void NavigationLink2D::_link_enter_navigation_map() {
	if (!is_inside_tree()) {
		return;
	}

	if (map_override.is_valid()) {
		NavigationServer2D::get_singleton()->link_set_map(link, map_override);
	} else {
		NavigationServer2D::get_singleton()->link_set_map(link, get_world_2d()->get_navigation_map());
	}

	current_global_transform = get_global_transform();

	NavigationServer2D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));
	NavigationServer2D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));
	NavigationServer2D::get_singleton()->link_set_enabled(link, enabled);

	queue_redraw();
}

void NavigationLink2D::_link_exit_navigation_map() {
	NavigationServer2D::get_singleton()->link_set_map(link, RID());
}

void NavigationLink2D::_link_update_transform() {
	if (!is_inside_tree()) {
		return;
	}

	Transform2D new_global_transform = get_global_transform();
	if (current_global_transform != new_global_transform) {
		current_global_transform = new_global_transform;
		NavigationServer2D::get_singleton()->link_set_start_position(link, current_global_transform.xform(start_position));
		NavigationServer2D::get_singleton()->link_set_end_position(link, current_global_transform.xform(end_position));
		queue_redraw();
	}
}

#ifdef DEBUG_ENABLED
void NavigationLink2D::_update_debug_mesh() {
	if (!is_inside_tree()) {
		return;
	}

	if (!Engine::get_singleton()->is_editor_hint() && !NavigationServer2D::get_singleton()->get_debug_enabled()) {
		return;
	}

	Color color;
	if (enabled) {
		color = NavigationServer2D::get_singleton()->get_debug_navigation_link_connection_color();
	} else {
		color = NavigationServer2D::get_singleton()->get_debug_navigation_link_connection_disabled_color();
	}

	real_t radius = NavigationServer2D::get_singleton()->map_get_link_connection_radius(get_world_2d()->get_navigation_map());

	draw_line(get_start_position(), get_end_position(), color);
	draw_arc(get_start_position(), radius, 0, Math_TAU, 10, color);
	draw_arc(get_end_position(), radius, 0, Math_TAU, 10, color);
}
#endif // DEBUG_ENABLED

NavigationLink2D::NavigationLink2D() {
	link = NavigationServer2D::get_singleton()->link_create();

	NavigationServer2D::get_singleton()->link_set_owner_id(link, get_instance_id());
	NavigationServer2D::get_singleton()->link_set_enter_cost(link, enter_cost);
	NavigationServer2D::get_singleton()->link_set_travel_cost(link, travel_cost);
	NavigationServer2D::get_singleton()->link_set_navigation_layers(link, navigation_layers);
	NavigationServer2D::get_singleton()->link_set_bidirectional(link, bidirectional);
	NavigationServer2D::get_singleton()->link_set_enabled(link, enabled);

	set_notify_transform(true);
	set_hide_clip_children(true);
}

NavigationLink2D::~NavigationLink2D() {
	ERR_FAIL_NULL(NavigationServer2D::get_singleton());
	NavigationServer2D::get_singleton()->free(link);
	link = RID();
}
