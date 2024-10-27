/**************************************************************************/
/*  scene_string_names.cpp                                                */
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

#include "scene_string_names.h"

SceneStringNames *SceneStringNames::singleton = nullptr;

SceneStringNames::SceneStringNames() {
	resized = StaticCString::create("resized");
	draw = StaticCString::create("draw");
	hidden = StaticCString::create("hidden");
	visibility_changed = StaticCString::create("visibility_changed");
	input_event = StaticCString::create("input_event");
	shader = StaticCString::create("shader");
	tree_entered = StaticCString::create("tree_entered");
	tree_exiting = StaticCString::create("tree_exiting");
	tree_exited = StaticCString::create("tree_exited");
	ready = StaticCString::create("ready");
	item_rect_changed = StaticCString::create("item_rect_changed");
	size_flags_changed = StaticCString::create("size_flags_changed");
	minimum_size_changed = StaticCString::create("minimum_size_changed");
	sleeping_state_changed = StaticCString::create("sleeping_state_changed");

	finished = StaticCString::create("finished");
	animation_finished = StaticCString::create("animation_finished");
	animation_changed = StaticCString::create("animation_changed");
	animation_started = StaticCString::create("animation_started");
	RESET = StaticCString::create("RESET");

	pose_updated = StaticCString::create("pose_updated");
	skeleton_updated = StaticCString::create("skeleton_updated");
	bone_enabled_changed = StaticCString::create("bone_enabled_changed");
	show_rest_only_changed = StaticCString::create("show_rest_only_changed");

	mouse_entered = StaticCString::create("mouse_entered");
	mouse_exited = StaticCString::create("mouse_exited");
	mouse_shape_entered = StaticCString::create("mouse_shape_entered");
	mouse_shape_exited = StaticCString::create("mouse_shape_exited");

	focus_entered = StaticCString::create("focus_entered");
	focus_exited = StaticCString::create("focus_exited");

	pre_sort_children = StaticCString::create("pre_sort_children");
	sort_children = StaticCString::create("sort_children");

	body_shape_entered = StaticCString::create("body_shape_entered");
	body_entered = StaticCString::create("body_entered");
	body_shape_exited = StaticCString::create("body_shape_exited");
	body_exited = StaticCString::create("body_exited");

	area_shape_entered = StaticCString::create("area_shape_entered");
	area_shape_exited = StaticCString::create("area_shape_exited");

	update = StaticCString::create("update");
	updated = StaticCString::create("updated");

	_ready = StaticCString::create("_ready");

	screen_entered = StaticCString::create("screen_entered");
	screen_exited = StaticCString::create("screen_exited");

	gui_input = StaticCString::create("gui_input");

	_spatial_editor_group = StaticCString::create("_spatial_editor_group");
	_request_gizmo = StaticCString::create("_request_gizmo");

	offset = StaticCString::create("offset");
	rotation_mode = StaticCString::create("rotation_mode");
	rotate = StaticCString::create("rotate");
	h_offset = StaticCString::create("h_offset");
	v_offset = StaticCString::create("v_offset");

	area_entered = StaticCString::create("area_entered");
	area_exited = StaticCString::create("area_exited");

	line_separation = StaticCString::create("line_separation");
	font = StaticCString::create("font");
	font_size = StaticCString::create("font_size");
	font_color = StaticCString::create("font_color");

	frame_changed = StaticCString::create("frame_changed");
	texture_changed = StaticCString::create("texture_changed");

	autoplay = StaticCString::create("autoplay");
	blend_times = StaticCString::create("blend_times");
	speed = StaticCString::create("speed");

	node_configuration_warning_changed = StaticCString::create("node_configuration_warning_changed");

	output = StaticCString::create("output");

	path_pp = NodePath("..");

	// Audio bus name.
	Master = StaticCString::create("Master");

	default_ = StaticCString::create("default");

	window_input = StaticCString::create("window_input");

	theme_changed = StaticCString::create("theme_changed");

	shader_overrides_group = StaticCString::create("_shader_overrides_group_");
	shader_overrides_group_active = StaticCString::create("_shader_overrides_group_active_");

	_custom_type_script = StaticCString::create("_custom_type_script");

	pressed = StaticCString::create("pressed");
	id_pressed = StaticCString::create("id_pressed");
	toggled = StaticCString::create("toggled");

	panel = StaticCString::create("panel");

	item_selected = StaticCString::create("item_selected");

	confirmed = StaticCString::create("confirmed");

	text_changed = StaticCString::create("text_changed");
	text_submitted = StaticCString::create("text_submitted");
	value_changed = StaticCString::create("value_changed");
}
