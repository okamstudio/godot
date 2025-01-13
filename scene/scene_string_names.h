/**************************************************************************/
/*  scene_string_names.h                                                  */
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

#ifndef SCENE_STRING_NAMES_H
#define SCENE_STRING_NAMES_H

#include "core/string/node_path.h"
#include "core/string/string_name.h"

class SceneStringNames {
	inline static SceneStringNames *singleton = nullptr;

public:
	static void create() { singleton = memnew(SceneStringNames); }
	static void free() {
		memdelete(singleton);
		singleton = nullptr;
	}

	_FORCE_INLINE_ static SceneStringNames *get_singleton() { return singleton; }

	const StringName resized = StaticCString::create("resized");
	const StringName draw = StaticCString::create("draw");
	const StringName hidden = StaticCString::create("hidden");
	const StringName visibility_changed = StaticCString::create("visibility_changed");

	const StringName input_event = StaticCString::create("input_event");
	const StringName gui_input = StaticCString::create("gui_input");
	const StringName window_input = StaticCString::create("window_input");

	const StringName tree_entered = StaticCString::create("tree_entered");
	const StringName tree_exiting = StaticCString::create("tree_exiting");
	const StringName tree_exited = StaticCString::create("tree_exited");
	const StringName ready = StaticCString::create("ready");
	const StringName _ready = StaticCString::create("_ready");

	const StringName item_rect_changed = StaticCString::create("item_rect_changed");
	const StringName size_flags_changed = StaticCString::create("size_flags_changed");
	const StringName minimum_size_changed = StaticCString::create("minimum_size_changed");
	const StringName sleeping_state_changed = StaticCString::create("sleeping_state_changed");
	const StringName node_configuration_warning_changed = StaticCString::create("node_configuration_warning_changed");
	const StringName update = StaticCString::create("update");
	const StringName updated = StaticCString::create("updated");

	const StringName line_separation = StaticCString::create("line_separation");
	const StringName font = StaticCString::create("font");
	const StringName font_size = StaticCString::create("font_size");
	const StringName font_color = StaticCString::create("font_color");

	const StringName mouse_entered = StaticCString::create("mouse_entered");
	const StringName mouse_exited = StaticCString::create("mouse_exited");
	const StringName mouse_shape_entered = StaticCString::create("mouse_shape_entered");
	const StringName mouse_shape_exited = StaticCString::create("mouse_shape_exited");
	const StringName focus_entered = StaticCString::create("focus_entered");
	const StringName focus_exited = StaticCString::create("focus_exited");

	const StringName pre_sort_children = StaticCString::create("pre_sort_children");
	const StringName sort_children = StaticCString::create("sort_children");

	const StringName finished = StaticCString::create("finished");
	const StringName animation_finished = StaticCString::create("animation_finished");
	const StringName animation_changed = StaticCString::create("animation_changed");
	const StringName animation_started = StaticCString::create("animation_started");
	const StringName RESET = StaticCString::create("RESET");

	const StringName pose_updated = StaticCString::create("pose_updated");
	const StringName skeleton_updated = StaticCString::create("skeleton_updated");
	const StringName bone_enabled_changed = StaticCString::create("bone_enabled_changed");
	const StringName show_rest_only_changed = StaticCString::create("show_rest_only_changed");

	const StringName body_shape_entered = StaticCString::create("body_shape_entered");
	const StringName body_entered = StaticCString::create("body_entered");
	const StringName body_shape_exited = StaticCString::create("body_shape_exited");
	const StringName body_exited = StaticCString::create("body_exited");

	const StringName area_shape_entered = StaticCString::create("area_shape_entered");
	const StringName area_shape_exited = StaticCString::create("area_shape_exited");

	const StringName screen_entered = StaticCString::create("screen_entered");
	const StringName screen_exited = StaticCString::create("screen_exited");

	const StringName _spatial_editor_group = StaticCString::create("_spatial_editor_group");
	const StringName _request_gizmo = StaticCString::create("_request_gizmo");

	const StringName offset = StaticCString::create("offset");
	const StringName rotation_mode = StaticCString::create("rotation_mode");
	const StringName rotate = StaticCString::create("rotate");
	const StringName h_offset = StaticCString::create("h_offset");
	const StringName v_offset = StaticCString::create("v_offset");

	const StringName area_entered = StaticCString::create("area_entered");
	const StringName area_exited = StaticCString::create("area_exited");

	const StringName frame_changed = StaticCString::create("frame_changed");
	const StringName texture_changed = StaticCString::create("texture_changed");

	const StringName autoplay = StaticCString::create("autoplay");
	const StringName blend_times = StaticCString::create("blend_times");
	const StringName speed = StaticCString::create("speed");

	const NodePath path_pp = NodePath("..");

	const StringName default_ = StaticCString::create("default"); // default would conflict with C++ keyword.
	const StringName output = StaticCString::create("output");

	const StringName Master = StaticCString::create("Master"); // Audio bus name.

	const StringName theme_changed = StaticCString::create("theme_changed");
	const StringName shader = StaticCString::create("shader");
	const StringName shader_overrides_group = StaticCString::create("_shader_overrides_group_");
	const StringName shader_overrides_group_active = StaticCString::create("_shader_overrides_group_active_");

	const StringName _custom_type_script = StaticCString::create("_custom_type_script");

	const StringName pressed = StaticCString::create("pressed");
	const StringName id_pressed = StaticCString::create("id_pressed");
	const StringName toggled = StaticCString::create("toggled");
	const StringName hover = StaticCString::create("hover");

	const StringName panel = StaticCString::create("panel");
	const StringName item_selected = StaticCString::create("item_selected");
	const StringName confirmed = StaticCString::create("confirmed");

	const StringName text_changed = StaticCString::create("text_changed");
	const StringName text_submitted = StaticCString::create("text_submitted");
	const StringName value_changed = StaticCString::create("value_changed");

	const StringName Start = StaticCString::create("Start");
	const StringName End = StaticCString::create("End");

	const StringName FlatButton = StaticCString::create("FlatButton");
};

#define SceneStringName(m_name) SceneStringNames::get_singleton()->m_name

#endif // SCENE_STRING_NAMES_H
