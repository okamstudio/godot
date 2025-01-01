/**************************************************************************/
/*  node_3d_editor_plugin.cpp                                             */
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

#include "node_3d_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/math/math_funcs.h"
#include "core/math/projection.h"
#include "core/os/keyboard.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/gizmos/audio_listener_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/audio_stream_player_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/camera_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/collision_object_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/collision_polygon_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/collision_shape_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/cpu_particles_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/decal_gizmo_plugin.h"
#include "editor/plugins/gizmos/fog_volume_gizmo_plugin.h"
#include "editor/plugins/gizmos/geometry_instance_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/gpu_particles_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/gpu_particles_collision_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/joint_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/label_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/light_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/lightmap_gi_gizmo_plugin.h"
#include "editor/plugins/gizmos/lightmap_probe_gizmo_plugin.h"
#include "editor/plugins/gizmos/marker_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/mesh_instance_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/navigation_link_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/navigation_region_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/occluder_instance_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/physics_bone_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/ray_cast_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/reflection_probe_gizmo_plugin.h"
#include "editor/plugins/gizmos/shape_cast_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/soft_body_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/spring_arm_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/sprite_base_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/vehicle_body_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/visible_on_screen_notifier_3d_gizmo_plugin.h"
#include "editor/plugins/gizmos/voxel_gi_gizmo_plugin.h"
#include "editor/plugins/node_3d_editor_gizmos.h"
#include "editor/scene_tree_dock.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/decal.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/physics_body_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/gui/center_container.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/subviewport_container.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

constexpr real_t DISTANCE_DEFAULT = 4;

constexpr real_t GIZMO_ARROW_SIZE = 0.35;
constexpr real_t GIZMO_RING_HALF_WIDTH = 0.1;
constexpr real_t GIZMO_PLANE_SIZE = 0.2;
constexpr real_t GIZMO_PLANE_DST = 0.3;
constexpr real_t GIZMO_CIRCLE_SIZE = 1.1;
constexpr real_t GIZMO_SCALE_OFFSET = GIZMO_CIRCLE_SIZE + 0.3;
constexpr real_t GIZMO_ARROW_OFFSET = GIZMO_CIRCLE_SIZE + 0.3;

constexpr real_t ZOOM_FREELOOK_MIN = 0.01;
constexpr real_t ZOOM_FREELOOK_MULTIPLIER = 1.08;
constexpr real_t ZOOM_FREELOOK_INDICATOR_DELAY_S = 1.5;

#ifdef REAL_T_IS_DOUBLE
constexpr double ZOOM_FREELOOK_MAX = 1'000'000'000'000;
#else
constexpr float ZOOM_FREELOOK_MAX = 10'000;
#endif

constexpr real_t MIN_Z = 0.01;
constexpr real_t MAX_Z = 1000000.0;

constexpr real_t MIN_FOV = 0.01;
constexpr real_t MAX_FOV = 179;

void ViewportNavigationControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!is_connected(SceneStringName(mouse_exited), callable_mp(this, &ViewportNavigationControl::_on_mouse_exited))) {
				connect(SceneStringName(mouse_exited), callable_mp(this, &ViewportNavigationControl::_on_mouse_exited));
			}
			if (!is_connected(SceneStringName(mouse_entered), callable_mp(this, &ViewportNavigationControl::_on_mouse_entered))) {
				connect(SceneStringName(mouse_entered), callable_mp(this, &ViewportNavigationControl::_on_mouse_entered));
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (viewport != nullptr) {
				_draw();
				_update_navigation();
			}
		} break;
	}
}

void ViewportNavigationControl::_draw() {
	if (nav_mode == Node3DEditorViewport::NAVIGATION_NONE) {
		return;
	}

	Vector2 center = get_size() / 2.0;
	float radius = get_size().x / 2.0;

	const bool focused = focused_index != -1;
	draw_circle(center, radius, Color(0.5, 0.5, 0.5, focused || hovered ? 0.35 : 0.15));

	const Color c = focused ? Color(0.9, 0.9, 0.9, 0.9) : Color(0.5, 0.5, 0.5, 0.25);

	Vector2 circle_pos = focused ? center.move_toward(focused_pos, radius) : center;

	draw_circle(circle_pos, AXIS_CIRCLE_RADIUS, c);
	draw_circle(circle_pos, AXIS_CIRCLE_RADIUS * 0.8, c.darkened(0.4));
}

void ViewportNavigationControl::_process_click(int p_index, Vector2 p_position, bool p_pressed) {
	hovered = false;
	queue_redraw();

	if (focused_index != -1 && focused_index != p_index) {
		return;
	}
	if (p_pressed) {
		if (p_position.distance_to(get_size() / 2.0) < get_size().x / 2.0) {
			focused_pos = p_position;
			focused_index = p_index;
			queue_redraw();
		}
	} else {
		focused_index = -1;
		if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			Input::get_singleton()->warp_mouse(focused_mouse_start);
		}
	}
}

void ViewportNavigationControl::_process_drag(int p_index, Vector2 p_position, Vector2 p_relative_position) {
	if (focused_index == p_index) {
		if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_VISIBLE) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			focused_mouse_start = p_position;
		}
		focused_pos += p_relative_position;
		queue_redraw();
	}
}

void ViewportNavigationControl::gui_input(const Ref<InputEvent> &p_event) {
	// Mouse events
	const Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid() && mouse_button->get_button_index() == MouseButton::LEFT) {
		_process_click(100, mouse_button->get_position(), mouse_button->is_pressed());
	}

	const Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid()) {
		_process_drag(100, mouse_motion->get_global_position(), viewport->_get_warped_mouse_motion(mouse_motion));
	}

	// Touch events
	const Ref<InputEventScreenTouch> screen_touch = p_event;
	if (screen_touch.is_valid()) {
		_process_click(screen_touch->get_index(), screen_touch->get_position(), screen_touch->is_pressed());
	}

	const Ref<InputEventScreenDrag> screen_drag = p_event;
	if (screen_drag.is_valid()) {
		_process_drag(screen_drag->get_index(), screen_drag->get_position(), screen_drag->get_relative());
	}
}

void ViewportNavigationControl::_update_navigation() {
	if (focused_index == -1) {
		return;
	}

	Vector2 delta = focused_pos - (get_size() / 2.0);
	Vector2 delta_normalized = delta.normalized();
	switch (nav_mode) {
		case Node3DEditorViewport::NavigationMode::NAVIGATION_MOVE: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x * 100.0), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;

			const Node3DEditorViewport::FreelookNavigationScheme navigation_scheme = (Node3DEditorViewport::FreelookNavigationScheme)EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_navigation_scheme").operator int();

			Vector3 forward;
			if (navigation_scheme == Node3DEditorViewport::FreelookNavigationScheme::FREELOOK_FULLY_AXIS_LOCKED) {
				// Forward/backward keys will always go straight forward/backward, never moving on the Y axis.
				forward = Vector3(0, 0, delta_normalized.y).rotated(Vector3(0, 1, 0), viewport->camera->get_rotation().y);
			} else {
				// Forward/backward keys will be relative to the camera pitch.
				forward = viewport->camera->get_transform().basis.xform(Vector3(0, 0, delta_normalized.y));
			}

			const Vector3 right = viewport->camera->get_transform().basis.xform(Vector3(delta_normalized.x, 0, 0));

			const Vector3 direction = forward + right;
			const Vector3 motion = direction * speed;
			viewport->cursor.pos += motion;
			viewport->cursor.eye_pos += motion;
		} break;

		case Node3DEditorViewport::NavigationMode::NAVIGATION_LOOK: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x * 2.5), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_look(nullptr, delta_normalized * speed);
		} break;

		case Node3DEditorViewport::NAVIGATION_PAN: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_pan(nullptr, -delta_normalized * speed);
		} break;
		case Node3DEditorViewport::NAVIGATION_ZOOM: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_zoom(nullptr, delta_normalized * speed);
		} break;
		case Node3DEditorViewport::NAVIGATION_ORBIT: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_orbit(nullptr, delta_normalized * speed);
		} break;
		case Node3DEditorViewport::NAVIGATION_NONE: {
		} break;
	}
}

void ViewportNavigationControl::_on_mouse_entered() {
	hovered = true;
	queue_redraw();
}

void ViewportNavigationControl::_on_mouse_exited() {
	hovered = false;
	queue_redraw();
}

void ViewportNavigationControl::set_navigation_mode(Node3DEditorViewport::NavigationMode p_nav_mode) {
	nav_mode = p_nav_mode;
}

void ViewportNavigationControl::set_viewport(Node3DEditorViewport *p_viewport) {
	viewport = p_viewport;
}

void ViewportRotationControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			axis_menu_options.clear();
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_RIGHT);
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_TOP);
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_FRONT);
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_LEFT);
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_BOTTOM);
			axis_menu_options.push_back(Node3DEditorViewport::VIEW_REAR);

			axis_colors.clear();
			axis_colors.push_back(get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor)));
			axis_colors.push_back(get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor)));
			axis_colors.push_back(get_theme_color(SNAME("axis_z_color"), EditorStringName(Editor)));
			queue_redraw();

			if (!is_connected(SceneStringName(mouse_exited), callable_mp(this, &ViewportRotationControl::_on_mouse_exited))) {
				connect(SceneStringName(mouse_exited), callable_mp(this, &ViewportRotationControl::_on_mouse_exited));
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (viewport != nullptr) {
				_draw();
			}
		} break;
	}
}

void ViewportRotationControl::_draw() {
	const Vector2 center = get_size() / 2.0;
	const real_t radius = get_size().x / 2.0;

	if (focused_axis > -2 || orbiting_index != -1) {
		draw_circle(center, radius, Color(0.5, 0.5, 0.5, 0.25), true, -1.0, true);
	}

	Vector<Axis2D> axis_to_draw;
	_get_sorted_axis(axis_to_draw);
	for (int i = 0; i < axis_to_draw.size(); ++i) {
		_draw_axis(axis_to_draw[i]);
	}
}

void ViewportRotationControl::_draw_axis(const Axis2D &p_axis) {
	const bool focused = focused_axis == p_axis.axis;
	const bool positive = p_axis.axis < 3;
	const int direction = p_axis.axis % 3;

	const Color axis_color = axis_colors[direction];
	const double min_alpha = 0.35;
	const double alpha = focused ? 1.0 : Math::remap((p_axis.z_axis + 1.0) / 2.0, 0, 0.5, min_alpha, 1.0);
	const Color c = focused ? Color(axis_color.lightened(0.75), 1.0) : Color(axis_color, alpha);

	if (positive) {
		// Draw axis lines for the positive axes.
		const Vector2 center = get_size() / 2.0;
		const Vector2 diff = p_axis.screen_point - center;
		const float line_length = MAX(diff.length() - AXIS_CIRCLE_RADIUS - 0.5 * EDSCALE, 0);

		draw_line(center + diff.limit_length(0.5 * EDSCALE), center + diff.limit_length(line_length), c, 1.5 * EDSCALE, true);

		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS, c, true, -1.0, true);

		// Draw the axis letter for the positive axes.
		const String axis_name = direction == 0 ? "X" : (direction == 1 ? "Y" : "Z");
		const Ref<Font> &font = get_theme_font(SNAME("rotation_control"), EditorStringName(EditorFonts));
		const int font_size = get_theme_font_size(SNAME("rotation_control_size"), EditorStringName(EditorFonts));
		const Size2 char_size = font->get_char_size(axis_name[0], font_size);
		const Vector2 char_offset = Vector2(-char_size.width / 2.0, char_size.height * 0.25);
		draw_char(font, p_axis.screen_point + char_offset, axis_name, font_size, Color(0.0, 0.0, 0.0, alpha * 0.6));
	} else {
		// Draw an outline around the negative axes.
		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS, c, true, -1.0, true);
		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS * 0.8, c.darkened(0.4), true, -1.0, true);
	}
}

void ViewportRotationControl::_get_sorted_axis(Vector<Axis2D> &r_axis) {
	const Vector2 center = get_size() / 2.0;
	const real_t radius = get_size().x / 2.0 - AXIS_CIRCLE_RADIUS - 2.0 * EDSCALE;
	const Basis camera_basis = viewport->to_camera_transform(viewport->cursor).get_basis().inverse();

	for (int i = 0; i < 3; ++i) {
		Vector3 axis_3d = camera_basis.get_column(i);
		Vector2 axis_vector = Vector2(axis_3d.x, -axis_3d.y) * radius;

		if (Math::abs(axis_3d.z) <= 1.0) {
			Axis2D pos_axis;
			pos_axis.axis = i;
			pos_axis.screen_point = center + axis_vector;
			pos_axis.z_axis = axis_3d.z;
			r_axis.push_back(pos_axis);

			Axis2D neg_axis;
			neg_axis.axis = i + 3;
			neg_axis.screen_point = center - axis_vector;
			neg_axis.z_axis = -axis_3d.z;
			r_axis.push_back(neg_axis);
		} else {
			// Special case when the camera is aligned with one axis
			Axis2D axis;
			axis.axis = i + (axis_3d.z <= 0 ? 0 : 3);
			axis.screen_point = center;
			axis.z_axis = 1.0;
			r_axis.push_back(axis);
		}
	}

	r_axis.sort_custom<Axis2DCompare>();
}

void ViewportRotationControl::_process_click(int p_index, Vector2 p_position, bool p_pressed) {
	if (orbiting_index != -1 && orbiting_index != p_index) {
		return;
	}
	if (p_pressed) {
		if (p_position.distance_to(get_size() / 2.0) < get_size().x / 2.0) {
			orbiting_index = p_index;
		}
	} else {
		if (focused_axis > -1) {
			viewport->_menu_option(axis_menu_options[focused_axis]);
			_update_focus();
		}
		orbiting_index = -1;
		if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			Input::get_singleton()->warp_mouse(orbiting_mouse_start);
		}
	}
}

void ViewportRotationControl::_process_drag(Ref<InputEventWithModifiers> p_event, int p_index, Vector2 p_position, Vector2 p_relative_position) {
	if (orbiting_index == p_index) {
		if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_VISIBLE) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
			orbiting_mouse_start = p_position;
		}
		viewport->_nav_orbit(p_event, p_relative_position);
		focused_axis = -1;
	} else {
		_update_focus();
	}
}

void ViewportRotationControl::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	// Mouse events
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		_process_click(100, mb->get_position(), mb->is_pressed());
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		_process_drag(mm, 100, mm->get_global_position(), viewport->_get_warped_mouse_motion(mm));
	}

	// Touch events
	const Ref<InputEventScreenTouch> screen_touch = p_event;
	if (screen_touch.is_valid()) {
		_process_click(screen_touch->get_index(), screen_touch->get_position(), screen_touch->is_pressed());
	}

	const Ref<InputEventScreenDrag> screen_drag = p_event;
	if (screen_drag.is_valid()) {
		_process_drag(screen_drag, screen_drag->get_index(), screen_drag->get_position(), screen_drag->get_relative());
	}
}

void ViewportRotationControl::_update_focus() {
	int original_focus = focused_axis;
	focused_axis = -2;
	Vector2 mouse_pos = get_local_mouse_position();

	if (mouse_pos.distance_to(get_size() / 2.0) < get_size().x / 2.0) {
		focused_axis = -1;
	}

	Vector<Axis2D> axes;
	_get_sorted_axis(axes);

	for (int i = 0; i < axes.size(); i++) {
		const Axis2D &axis = axes[i];
		if (mouse_pos.distance_to(axis.screen_point) < AXIS_CIRCLE_RADIUS) {
			focused_axis = axis.axis;
		}
	}

	if (focused_axis != original_focus) {
		queue_redraw();
	}
}

void ViewportRotationControl::_on_mouse_exited() {
	focused_axis = -2;
	queue_redraw();
}

void ViewportRotationControl::set_viewport(Node3DEditorViewport *p_viewport) {
	viewport = p_viewport;
}

void Node3DEditorViewport::_view_settings_confirmed(real_t p_interp_delta) {
	// Set FOV override multiplier back to the default, so that the FOV
	// setting specified in the View menu is correctly applied.
	cursor.fov_scale = 1.0;

	_update_camera(p_interp_delta);
}

void Node3DEditorViewport::_update_navigation_controls_visibility() {
	bool show_viewport_rotation_gizmo = EDITOR_GET("editors/3d/navigation/show_viewport_rotation_gizmo") && (!previewing_cinema && !previewing_camera);
	rotation_control->set_visible(show_viewport_rotation_gizmo);

	bool show_viewport_navigation_gizmo = EDITOR_GET("editors/3d/navigation/show_viewport_navigation_gizmo") && (!previewing_cinema && !previewing_camera);
	position_control->set_visible(show_viewport_navigation_gizmo);
	look_control->set_visible(show_viewport_navigation_gizmo);
}

void Node3DEditorViewport::_update_camera(real_t p_interp_delta) {
	bool is_orthogonal = camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;

	Cursor old_camera_cursor = camera_cursor;
	camera_cursor = cursor;

	if (p_interp_delta > 0) {
		//-------
		// Perform smoothing

		if (is_freelook_active()) {
			// Higher inertia should increase "lag" (lerp with factor between 0 and 1)
			// Inertia of zero should produce instant movement (lerp with factor of 1) in this case it returns a really high value and gets clamped to 1.
			const real_t inertia = EDITOR_GET("editors/3d/freelook/freelook_inertia");
			real_t factor = (1.0 / inertia) * p_interp_delta;

			// We interpolate a different point here, because in freelook mode the focus point (cursor.pos) orbits around eye_pos
			camera_cursor.eye_pos = old_camera_cursor.eye_pos.lerp(cursor.eye_pos, CLAMP(factor, 0, 1));

			const real_t orbit_inertia = EDITOR_GET("editors/3d/navigation_feel/orbit_inertia");
			camera_cursor.x_rot = Math::lerp(old_camera_cursor.x_rot, cursor.x_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));
			camera_cursor.y_rot = Math::lerp(old_camera_cursor.y_rot, cursor.y_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));

			if (Math::abs(camera_cursor.x_rot - cursor.x_rot) < 0.1) {
				camera_cursor.x_rot = cursor.x_rot;
			}

			if (Math::abs(camera_cursor.y_rot - cursor.y_rot) < 0.1) {
				camera_cursor.y_rot = cursor.y_rot;
			}

			Vector3 forward = to_camera_transform(camera_cursor).basis.xform(Vector3(0, 0, -1));
			camera_cursor.pos = camera_cursor.eye_pos + forward * camera_cursor.distance;

		} else {
			const real_t orbit_inertia = EDITOR_GET("editors/3d/navigation_feel/orbit_inertia");
			const real_t translation_inertia = EDITOR_GET("editors/3d/navigation_feel/translation_inertia");
			const real_t zoom_inertia = EDITOR_GET("editors/3d/navigation_feel/zoom_inertia");

			camera_cursor.x_rot = Math::lerp(old_camera_cursor.x_rot, cursor.x_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));
			camera_cursor.y_rot = Math::lerp(old_camera_cursor.y_rot, cursor.y_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));

			if (Math::abs(camera_cursor.x_rot - cursor.x_rot) < 0.1) {
				camera_cursor.x_rot = cursor.x_rot;
			}

			if (Math::abs(camera_cursor.y_rot - cursor.y_rot) < 0.1) {
				camera_cursor.y_rot = cursor.y_rot;
			}

			camera_cursor.pos = old_camera_cursor.pos.lerp(cursor.pos, MIN(1.f, p_interp_delta * (1 / translation_inertia)));
			camera_cursor.distance = Math::lerp(old_camera_cursor.distance, cursor.distance, MIN((real_t)1.0, p_interp_delta * (1 / zoom_inertia)));
		}
	}

	//-------
	// Apply camera transform

	real_t tolerance = 0.001;
	bool equal = true;
	if (!Math::is_equal_approx(old_camera_cursor.x_rot, camera_cursor.x_rot, tolerance) || !Math::is_equal_approx(old_camera_cursor.y_rot, camera_cursor.y_rot, tolerance)) {
		equal = false;
	} else if (!old_camera_cursor.pos.is_equal_approx(camera_cursor.pos)) {
		equal = false;
	} else if (!Math::is_equal_approx(old_camera_cursor.distance, camera_cursor.distance, tolerance)) {
		equal = false;
	} else if (!Math::is_equal_approx(old_camera_cursor.fov_scale, camera_cursor.fov_scale, tolerance)) {
		equal = false;
	}

	if (!equal || p_interp_delta == 0 || is_orthogonal != orthogonal) {
		last_camera_transform = to_camera_transform(camera_cursor);
		camera->set_global_transform(last_camera_transform);

		if (orthogonal) {
			float half_fov = Math::deg_to_rad(get_fov()) / 2.0;
			float height = 2.0 * cursor.distance * Math::tan(half_fov);
			camera->set_orthogonal(height, get_znear(), get_zfar());
		} else {
			camera->set_perspective(get_fov(), get_znear(), get_zfar());
		}

		update_transform_gizmo_view();
		rotation_control->queue_redraw();
		position_control->queue_redraw();
		look_control->queue_redraw();
		spatial_editor->update_grid();
	}
}

Transform3D Node3DEditorViewport::to_camera_transform(const Cursor &p_cursor) const {
	Transform3D camera_transform;
	camera_transform.translate_local(p_cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -p_cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -p_cursor.y_rot);

	if (orthogonal) {
		camera_transform.translate_local(0, 0, (get_zfar() - get_znear()) / 2.0);
	} else {
		camera_transform.translate_local(0, 0, p_cursor.distance);
	}

	return camera_transform;
}

int Node3DEditorViewport::get_selected_count() const {
	const HashMap<Node *, Object *> &selection = editor_selection->get_selection();

	int count = 0;

	for (const KeyValue<Node *, Object *> &E : selection) {
		Node3D *sp = Object::cast_to<Node3D>(E.key);
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		count++;
	}

	return count;
}

void Node3DEditorViewport::cancel_transform() {
	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Node3D *sp = Object::cast_to<Node3D>(E->get());
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		if (se && se->gizmo.is_valid()) {
			Vector<int> ids;
			Vector<Transform3D> restore;

			for (const KeyValue<int, Transform3D> &GE : se->subgizmos) {
				ids.push_back(GE.key);
				restore.push_back(GE.value);
			}

			se->gizmo->commit_subgizmos(ids, restore, true);
		}

		sp->set_global_transform(se->original);
	}

	collision_reposition = false;
	finish_transform();
	set_message(TTR("Transform Aborted."), 3);
}

void Node3DEditorViewport::_update_shrink() {
	bool shrink = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION));
	subviewport_container->set_stretch_shrink(shrink ? 2 : 1);
	subviewport_container->set_texture_filter(shrink ? TEXTURE_FILTER_NEAREST : TEXTURE_FILTER_PARENT_NODE);
}

float Node3DEditorViewport::get_znear() const {
	return CLAMP(spatial_editor->get_znear(), MIN_Z, MAX_Z);
}

float Node3DEditorViewport::get_zfar() const {
	return CLAMP(spatial_editor->get_zfar(), MIN_Z, MAX_Z);
}

float Node3DEditorViewport::get_fov() const {
	return CLAMP(spatial_editor->get_fov() * cursor.fov_scale, MIN_FOV, MAX_FOV);
}

Transform3D Node3DEditorViewport::_get_camera_transform() const {
	return camera->get_global_transform();
}

Vector3 Node3DEditorViewport::_get_camera_position() const {
	return _get_camera_transform().origin;
}

Point2 Node3DEditorViewport::point_to_screen(const Vector3 &p_point) {
	return camera->unproject_position(p_point) * subviewport_container->get_stretch_shrink();
}

Vector3 Node3DEditorViewport::get_ray_pos(const Vector2 &p_pos) const {
	return camera->project_ray_origin(p_pos / subviewport_container->get_stretch_shrink());
}

Vector3 Node3DEditorViewport::_get_camera_normal() const {
	return -_get_camera_transform().basis.get_column(2);
}

Vector3 Node3DEditorViewport::get_ray(const Vector2 &p_pos) const {
	return camera->project_ray_normal(p_pos / subviewport_container->get_stretch_shrink());
}

void Node3DEditorViewport::_clear_selected() {
	_edit.gizmo = Ref<EditorNode3DGizmo>();
	_edit.gizmo_handle = -1;
	_edit.gizmo_handle_secondary = false;
	_edit.gizmo_initial_value = Variant();

	Node3D *selected = spatial_editor->get_single_selected_node();
	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;

	if (se && se->gizmo.is_valid()) {
		se->subgizmos.clear();
		se->gizmo->redraw();
		se->gizmo.unref();
		spatial_editor->update_transform_gizmo();
	} else {
		editor_selection->clear();
		Node3DEditor::get_singleton()->edit(nullptr);
	}
}

void Node3DEditorViewport::_select_clicked(bool p_allow_locked) {
	Node *node = Object::cast_to<Node3D>(ObjectDB::get_instance(clicked));
	Node3D *selected = Object::cast_to<Node3D>(node);
	clicked = ObjectID();

	if (!selected) {
		return;
	}

	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

	// Prevent selection of nodes not owned by the edited scene.
	while (node && node != edited_scene->get_parent()) {
		Node *node_owner = node->get_owner();
		if (node_owner == edited_scene || node == edited_scene || (node_owner != nullptr && edited_scene->is_editable_instance(node_owner))) {
			break;
		}
		node = node->get_parent();
		selected = Object::cast_to<Node3D>(node);
	}

	if (!p_allow_locked) {
		// Replace the node by the group if grouped
		while (node && node != edited_scene->get_parent()) {
			Node3D *selected_tmp = Object::cast_to<Node3D>(node);
			if (selected_tmp && node->has_meta("_edit_group_")) {
				selected = selected_tmp;
			}
			node = node->get_parent();
		}
	}

	if (p_allow_locked || (selected != nullptr && !_is_node_locked(selected))) {
		if (clicked_wants_append) {
			if (editor_selection->is_selected(selected)) {
				editor_selection->remove_node(selected);
			} else {
				editor_selection->add_node(selected);
			}
		} else {
			if (!editor_selection->is_selected(selected)) {
				editor_selection->clear();
				editor_selection->add_node(selected);
				EditorNode::get_singleton()->edit_node(selected);
			}
		}

		if (editor_selection->get_selected_node_list().size() == 1) {
			EditorNode::get_singleton()->edit_node(editor_selection->get_selected_node_list().front()->get());
		}
	}
}

ObjectID Node3DEditorViewport::_select_ray(const Point2 &p_pos) const {
	Vector3 ray = get_ray(p_pos);
	Vector3 pos = get_ray_pos(p_pos);
	Vector2 shrinked_pos = p_pos / subviewport_container->get_stretch_shrink();

	if (viewport->get_debug_draw() == Viewport::DEBUG_DRAW_SDFGI_PROBES) {
		RS::get_singleton()->sdfgi_set_debug_probe_select(pos, ray);
	}

	HashSet<Ref<EditorNode3DGizmo>> found_gizmos;

	Node *edited_scene = get_tree()->get_edited_scene_root();
	ObjectID closest;
	Node *item = nullptr;
	float closest_dist = 1e20;

	Vector<Node3D *> nodes_with_gizmos = Node3DEditor::get_singleton()->gizmo_bvh_ray_query(pos, pos + ray * camera->get_far());

	for (Node3D *spat : nodes_with_gizmos) {
		if (!spat || _is_node_locked(spat)) {
			continue;
		}

		Vector<Ref<Node3DGizmo>> gizmos = spat->get_gizmos();

		for (int j = 0; j < gizmos.size(); j++) {
			Ref<EditorNode3DGizmo> seg = gizmos[j];

			if (seg.is_null() || found_gizmos.has(seg)) {
				continue;
			}

			found_gizmos.insert(seg);
			Vector3 point;
			Vector3 normal;

			bool inters = seg->intersect_ray(camera, shrinked_pos, point, normal);

			if (!inters) {
				continue;
			}

			const real_t dist = pos.distance_to(point);

			if (dist < 0) {
				continue;
			}

			if (dist < closest_dist) {
				item = Object::cast_to<Node>(spat);
				if (item != edited_scene) {
					item = edited_scene->get_deepest_editable_node(item);
				}

				closest = item->get_instance_id();
				closest_dist = dist;
			}
		}
	}

	if (!item) {
		return ObjectID();
	}

	return closest;
}

void Node3DEditorViewport::_find_items_at_pos(const Point2 &p_pos, Vector<_RayResult> &r_results, bool p_include_locked_nodes) {
	Vector3 ray = get_ray(p_pos);
	Vector3 pos = get_ray_pos(p_pos);

	Vector<Node3D *> nodes_with_gizmos = Node3DEditor::get_singleton()->gizmo_bvh_ray_query(pos, pos + ray * camera->get_far());

	HashSet<Node3D *> found_nodes;

	for (Node3D *spat : nodes_with_gizmos) {
		if (!spat) {
			continue;
		}

		if (found_nodes.has(spat)) {
			continue;
		}

		if (!p_include_locked_nodes && _is_node_locked(spat)) {
			continue;
		}

		Vector<Ref<Node3DGizmo>> gizmos = spat->get_gizmos();
		for (int j = 0; j < gizmos.size(); j++) {
			Ref<EditorNode3DGizmo> seg = gizmos[j];

			if (seg.is_null()) {
				continue;
			}

			Vector3 point;
			Vector3 normal;

			bool inters = seg->intersect_ray(camera, p_pos, point, normal);

			if (!inters) {
				continue;
			}

			const real_t dist = pos.distance_to(point);

			if (dist < 0) {
				continue;
			}

			found_nodes.insert(spat);

			_RayResult res;
			res.item = spat;
			res.depth = dist;
			r_results.push_back(res);
			break;
		}
	}

	r_results.sort();
}

Vector3 Node3DEditorViewport::_get_screen_to_space(const Vector3 &p_vector3) {
	Projection cm;
	if (orthogonal) {
		cm.set_orthogonal(camera->get_size(), get_size().aspect(), get_znear() + p_vector3.z, get_zfar());
	} else {
		cm.set_perspective(get_fov(), get_size().aspect(), get_znear() + p_vector3.z, get_zfar());
	}
	Vector2 screen_he = cm.get_viewport_half_extents();

	Transform3D camera_transform;
	camera_transform.translate_local(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
	camera_transform.translate_local(0, 0, cursor.distance);

	return camera_transform.xform(Vector3(((p_vector3.x / get_size().width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (p_vector3.y / get_size().height)) * 2.0 - 1.0) * screen_he.y, -(get_znear() + p_vector3.z)));
}

void Node3DEditorViewport::_select_region() {
	if (cursor.region_begin == cursor.region_end) {
		if (!clicked_wants_append) {
			_clear_selected();
		}
		return; //nothing really
	}

	const real_t z_offset = MAX(0.0, 5.0 - get_znear());

	Vector3 box[4] = {
		Vector3(
				MIN(cursor.region_begin.x, cursor.region_end.x),
				MIN(cursor.region_begin.y, cursor.region_end.y),
				z_offset),
		Vector3(
				MAX(cursor.region_begin.x, cursor.region_end.x),
				MIN(cursor.region_begin.y, cursor.region_end.y),
				z_offset),
		Vector3(
				MAX(cursor.region_begin.x, cursor.region_end.x),
				MAX(cursor.region_begin.y, cursor.region_end.y),
				z_offset),
		Vector3(
				MIN(cursor.region_begin.x, cursor.region_end.x),
				MAX(cursor.region_begin.y, cursor.region_end.y),
				z_offset)
	};

	Vector<Plane> frustum;

	Vector3 cam_pos = _get_camera_position();

	for (int i = 0; i < 4; i++) {
		Vector3 a = _get_screen_to_space(box[i]);
		Vector3 b = _get_screen_to_space(box[(i + 1) % 4]);
		if (orthogonal) {
			frustum.push_back(Plane((a - b).normalized(), a));
		} else {
			frustum.push_back(Plane(a, b, cam_pos));
		}
	}

	Plane near_plane = Plane(-_get_camera_normal(), cam_pos);
	near_plane.d -= get_znear();
	frustum.push_back(near_plane);

	Plane far_plane = -near_plane;
	far_plane.d += get_zfar();
	frustum.push_back(far_plane);

	if (spatial_editor->get_single_selected_node()) {
		Node3D *single_selected = spatial_editor->get_single_selected_node();
		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(single_selected);

		if (se) {
			Ref<EditorNode3DGizmo> old_gizmo;
			if (!clicked_wants_append) {
				se->subgizmos.clear();
				old_gizmo = se->gizmo;
				se->gizmo.unref();
			}

			bool found_subgizmos = false;
			Vector<Ref<Node3DGizmo>> gizmos = single_selected->get_gizmos();
			for (int j = 0; j < gizmos.size(); j++) {
				Ref<EditorNode3DGizmo> seg = gizmos[j];
				if (seg.is_null()) {
					continue;
				}

				if (se->gizmo.is_valid() && se->gizmo != seg) {
					continue;
				}

				Vector<int> subgizmos = seg->subgizmos_intersect_frustum(camera, frustum);
				if (!subgizmos.is_empty()) {
					se->gizmo = seg;
					for (int i = 0; i < subgizmos.size(); i++) {
						int subgizmo_id = subgizmos[i];
						if (!se->subgizmos.has(subgizmo_id)) {
							se->subgizmos.insert(subgizmo_id, se->gizmo->get_subgizmo_transform(subgizmo_id));
						}
					}
					found_subgizmos = true;
					break;
				}
			}

			if (!clicked_wants_append || found_subgizmos) {
				if (se->gizmo.is_valid()) {
					se->gizmo->redraw();
				}

				if (old_gizmo != se->gizmo && old_gizmo.is_valid()) {
					old_gizmo->redraw();
				}

				spatial_editor->update_transform_gizmo();
			}

			if (found_subgizmos) {
				return;
			}
		}
	}

	if (!clicked_wants_append) {
		_clear_selected();
	}

	Vector<Node3D *> nodes_with_gizmos = Node3DEditor::get_singleton()->gizmo_bvh_frustum_query(frustum);
	HashSet<Node3D *> found_nodes;
	Vector<Node *> selected;

	Node *edited_scene = get_tree()->get_edited_scene_root();
	if (edited_scene == nullptr) {
		return;
	}

	for (Node3D *sp : nodes_with_gizmos) {
		if (!sp || _is_node_locked(sp)) {
			continue;
		}

		if (found_nodes.has(sp)) {
			continue;
		}
		found_nodes.insert(sp);

		Node *node = Object::cast_to<Node>(sp);

		// Selection requires that the node is the edited scene or its descendant, and has an owner.
		if (node != edited_scene) {
			if (!node->get_owner() || !edited_scene->is_ancestor_of(node)) {
				continue;
			}
			node = edited_scene->get_deepest_editable_node(node);
			while (node != edited_scene) {
				Node *node_owner = node->get_owner();
				if (node_owner == edited_scene || (node_owner != nullptr && edited_scene->is_editable_instance(node_owner))) {
					break;
				}
				node = node->get_parent();
			}
		}

		// Replace the node by the group if grouped
		if (node->is_class("Node3D")) {
			Node3D *sel = Object::cast_to<Node3D>(node);
			while (node && node != EditorNode::get_singleton()->get_edited_scene()->get_parent()) {
				Node3D *selected_tmp = Object::cast_to<Node3D>(node);
				if (selected_tmp && node->has_meta("_edit_group_")) {
					sel = selected_tmp;
				}
				node = node->get_parent();
			}
			node = sel;
		}

		if (_is_node_locked(node)) {
			continue;
		}

		Vector<Ref<Node3DGizmo>> gizmos = sp->get_gizmos();
		for (int j = 0; j < gizmos.size(); j++) {
			Ref<EditorNode3DGizmo> seg = gizmos[j];
			if (seg.is_null()) {
				continue;
			}

			if (seg->intersect_frustum(camera, frustum)) {
				selected.push_back(node);
			}
		}
	}

	for (int i = 0; i < selected.size(); i++) {
		if (!editor_selection->is_selected(selected[i])) {
			editor_selection->add_node(selected[i]);
		}
	}

	if (editor_selection->get_selected_node_list().size() == 1) {
		EditorNode::get_singleton()->edit_node(editor_selection->get_selected_node_list().front()->get());
	}
}

void Node3DEditorViewport::_update_name() {
	String name;

	switch (view_type) {
		case VIEW_TYPE_USER: {
			if (orthogonal) {
				name = TTR("Orthogonal");
			} else {
				name = TTR("Perspective");
			}
		} break;
		case VIEW_TYPE_TOP: {
			if (orthogonal) {
				name = TTR("Top Orthogonal");
			} else {
				name = TTR("Top Perspective");
			}
		} break;
		case VIEW_TYPE_BOTTOM: {
			if (orthogonal) {
				name = TTR("Bottom Orthogonal");
			} else {
				name = TTR("Bottom Perspective");
			}
		} break;
		case VIEW_TYPE_LEFT: {
			if (orthogonal) {
				name = TTR("Left Orthogonal");
			} else {
				name = TTR("Left Perspective");
			}
		} break;
		case VIEW_TYPE_RIGHT: {
			if (orthogonal) {
				name = TTR("Right Orthogonal");
			} else {
				name = TTR("Right Perspective");
			}
		} break;
		case VIEW_TYPE_FRONT: {
			if (orthogonal) {
				name = TTR("Front Orthogonal");
			} else {
				name = TTR("Front Perspective");
			}
		} break;
		case VIEW_TYPE_REAR: {
			if (orthogonal) {
				name = TTR("Rear Orthogonal");
			} else {
				name = TTR("Rear Perspective");
			}
		} break;
	}

	if (auto_orthogonal) {
		// TRANSLATORS: This will be appended to the view name when Auto Orthogonal is enabled.
		name += " " + TTR("[auto]");
	}

	view_menu->set_text(name);
	view_menu->reset_size();
}

void Node3DEditorViewport::_compute_edit(const Point2 &p_point) {
	_edit.original_local = spatial_editor->are_local_coords_enabled();
	_edit.click_ray = get_ray(p_point);
	_edit.click_ray_pos = get_ray_pos(p_point);
	_edit.plane = TRANSFORM_VIEW;
	spatial_editor->update_transform_gizmo();
	_edit.center = spatial_editor->get_gizmo_transform().origin;

	Node3D *selected = spatial_editor->get_single_selected_node();
	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;

	if (se && se->gizmo.is_valid()) {
		for (const KeyValue<int, Transform3D> &E : se->subgizmos) {
			int subgizmo_id = E.key;
			se->subgizmos[subgizmo_id] = se->gizmo->get_subgizmo_transform(subgizmo_id);
		}
		se->original_local = selected->get_transform();
		se->original = selected->get_global_transform();
	} else {
		List<Node *> &selection = editor_selection->get_selected_node_list();

		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			Node3D *sp = Object::cast_to<Node3D>(E->get());
			if (!sp) {
				continue;
			}

			Node3DEditorSelectedItem *sel_item = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);

			if (!sel_item) {
				continue;
			}

			sel_item->original_local = sel_item->sp->get_local_gizmo_transform();
			sel_item->original = sel_item->sp->get_global_gizmo_transform();
		}
	}
}

static Key _get_key_modifier_setting(const String &p_property) {
	switch (EDITOR_GET(p_property).operator int()) {
		case 0:
			return Key::NONE;
		case 1:
			return Key::SHIFT;
		case 2:
			return Key::ALT;
		case 3:
			return Key::META;
		case 4:
			return Key::CTRL;
	}
	return Key::NONE;
}

static Key _get_key_modifier(Ref<InputEventWithModifiers> e) {
	if (e->is_shift_pressed()) {
		return Key::SHIFT;
	}
	if (e->is_alt_pressed()) {
		return Key::ALT;
	}
	if (e->is_ctrl_pressed()) {
		return Key::CTRL;
	}
	if (e->is_meta_pressed()) {
		return Key::META;
	}
	return Key::NONE;
}

bool Node3DEditorViewport::_transform_gizmo_select(const Vector2 &p_screenpos, bool p_highlight_only) {
	if (!spatial_editor->is_gizmo_visible()) {
		return false;
	}
	if (get_selected_count() == 0) {
		if (p_highlight_only) {
			spatial_editor->select_gizmo_highlight_axis(-1);
		}
		return false;
	}

	Vector3 ray_pos = get_ray_pos(p_screenpos);
	Vector3 ray = get_ray(p_screenpos);

	Transform3D gt = spatial_editor->get_gizmo_transform();

	if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_MOVE) {
		int col_axis = -1;
		real_t col_d = 1e20;

		for (int i = 0; i < 3; i++) {
			const Vector3 grabber_pos = gt.origin + gt.basis.get_column(i).normalized() * gizmo_scale * (GIZMO_ARROW_OFFSET + (GIZMO_ARROW_SIZE * 0.5));
			const real_t grabber_radius = gizmo_scale * GIZMO_ARROW_SIZE;

			Vector3 r;

			if (Geometry3D::segment_intersects_sphere(ray_pos, ray_pos + ray * MAX_Z, grabber_pos, grabber_radius, &r)) {
				const real_t d = r.distance_to(ray_pos);
				if (d < col_d) {
					col_d = d;
					col_axis = i;
				}
			}
		}

		bool is_plane_translate = false;
		// plane select
		if (col_axis == -1) {
			col_d = 1e20;

			for (int i = 0; i < 3; i++) {
				Vector3 ivec2 = gt.basis.get_column((i + 1) % 3).normalized();
				Vector3 ivec3 = gt.basis.get_column((i + 2) % 3).normalized();

				// Allow some tolerance to make the plane easier to click,
				// even if the click is actually slightly outside the plane.
				const Vector3 grabber_pos = gt.origin + (ivec2 + ivec3) * gizmo_scale * (GIZMO_PLANE_SIZE + GIZMO_PLANE_DST * 0.6667);

				Vector3 r;
				Plane plane(gt.basis.get_column(i).normalized(), gt.origin);

				if (plane.intersects_ray(ray_pos, ray, &r)) {
					const real_t dist = r.distance_to(grabber_pos);
					// Allow some tolerance to make the plane easier to click,
					// even if the click is actually slightly outside the plane.
					if (dist < (gizmo_scale * GIZMO_PLANE_SIZE * 1.5)) {
						const real_t d = ray_pos.distance_to(r);
						if (d < col_d) {
							col_d = d;
							col_axis = i;

							is_plane_translate = true;
						}
					}
				}
			}
		}

		if (col_axis != -1) {
			if (p_highlight_only) {
				spatial_editor->select_gizmo_highlight_axis(col_axis + (is_plane_translate ? 6 : 0));

			} else {
				//handle plane translate
				_edit.mode = TRANSFORM_TRANSLATE;
				_compute_edit(p_screenpos);
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis + (is_plane_translate ? 3 : 0));
			}
			return true;
		}
	}

	if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_ROTATE) {
		int col_axis = -1;

		Vector3 hit_position;
		Vector3 hit_normal;
		real_t ray_length = gt.origin.distance_to(ray_pos) + (GIZMO_CIRCLE_SIZE * gizmo_scale) * 4.0f;
		if (Geometry3D::segment_intersects_sphere(ray_pos, ray_pos + ray * ray_length, gt.origin, gizmo_scale * (GIZMO_CIRCLE_SIZE), &hit_position, &hit_normal)) {
			if (hit_normal.dot(_get_camera_normal()) < 0.05) {
				hit_position = gt.xform_inv(hit_position).abs();
				int min_axis = hit_position.min_axis_index();
				if (hit_position[min_axis] < gizmo_scale * GIZMO_RING_HALF_WIDTH) {
					col_axis = min_axis;
				}
			}
		}

		if (col_axis == -1) {
			float col_d = 1e20;

			for (int i = 0; i < 3; i++) {
				Plane plane(gt.basis.get_column(i).normalized(), gt.origin);
				Vector3 r;
				if (!plane.intersects_ray(ray_pos, ray, &r)) {
					continue;
				}

				const real_t dist = r.distance_to(gt.origin);
				const Vector3 r_dir = (r - gt.origin).normalized();

				if (_get_camera_normal().dot(r_dir) <= 0.005) {
					if (dist > gizmo_scale * (GIZMO_CIRCLE_SIZE - GIZMO_RING_HALF_WIDTH) && dist < gizmo_scale * (GIZMO_CIRCLE_SIZE + GIZMO_RING_HALF_WIDTH)) {
						const real_t d = ray_pos.distance_to(r);
						if (d < col_d) {
							col_d = d;
							col_axis = i;
						}
					}
				}
			}
		}

		if (col_axis != -1) {
			if (p_highlight_only) {
				spatial_editor->select_gizmo_highlight_axis(col_axis + 3);
			} else {
				//handle rotate
				_edit.mode = TRANSFORM_ROTATE;
				_compute_edit(p_screenpos);
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis);
			}
			return true;
		}
	}

	if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SCALE) {
		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {
			const Vector3 grabber_pos = gt.origin + gt.basis.get_column(i).normalized() * gizmo_scale * GIZMO_SCALE_OFFSET;
			const real_t grabber_radius = gizmo_scale * GIZMO_ARROW_SIZE;

			Vector3 r;

			if (Geometry3D::segment_intersects_sphere(ray_pos, ray_pos + ray * MAX_Z, grabber_pos, grabber_radius, &r)) {
				const real_t d = r.distance_to(ray_pos);
				if (d < col_d) {
					col_d = d;
					col_axis = i;
				}
			}
		}

		bool is_plane_scale = false;
		// plane select
		if (col_axis == -1) {
			col_d = 1e20;

			for (int i = 0; i < 3; i++) {
				const Vector3 ivec2 = gt.basis.get_column((i + 1) % 3).normalized();
				const Vector3 ivec3 = gt.basis.get_column((i + 2) % 3).normalized();

				// Allow some tolerance to make the plane easier to click,
				// even if the click is actually slightly outside the plane.
				const Vector3 grabber_pos = gt.origin + (ivec2 + ivec3) * gizmo_scale * (GIZMO_PLANE_SIZE + GIZMO_PLANE_DST * 0.6667);

				Vector3 r;
				Plane plane(gt.basis.get_column(i).normalized(), gt.origin);

				if (plane.intersects_ray(ray_pos, ray, &r)) {
					const real_t dist = r.distance_to(grabber_pos);
					// Allow some tolerance to make the plane easier to click,
					// even if the click is actually slightly outside the plane.
					if (dist < (gizmo_scale * GIZMO_PLANE_SIZE * 1.5)) {
						const real_t d = ray_pos.distance_to(r);
						if (d < col_d) {
							col_d = d;
							col_axis = i;

							is_plane_scale = true;
						}
					}
				}
			}
		}

		if (col_axis != -1) {
			if (p_highlight_only) {
				spatial_editor->select_gizmo_highlight_axis(col_axis + (is_plane_scale ? 12 : 9));

			} else {
				//handle scale
				_edit.mode = TRANSFORM_SCALE;
				_compute_edit(p_screenpos);
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis + (is_plane_scale ? 3 : 0));
			}
			return true;
		}
	}

	if (p_highlight_only) {
		spatial_editor->select_gizmo_highlight_axis(-1);
	}

	return false;
}

void Node3DEditorViewport::_transform_gizmo_apply(Node3D *p_node, const Transform3D &p_transform, bool p_local) {
	if (p_transform.basis.determinant() == 0) {
		return;
	}

	if (p_local) {
		p_node->set_transform(p_transform);
	} else {
		p_node->set_global_transform(p_transform);
	}
}

Transform3D Node3DEditorViewport::_compute_transform(TransformMode p_mode, const Transform3D &p_original, const Transform3D &p_original_local, Vector3 p_motion, double p_extra, bool p_local, bool p_orthogonal) {
	switch (p_mode) {
		case TRANSFORM_SCALE: {
			if (_edit.snap || spatial_editor->is_snap_enabled()) {
				p_motion.snapf(p_extra);
			}
			Transform3D s;
			if (p_local) {
				s.basis = p_original_local.basis.scaled_local(p_motion + Vector3(1, 1, 1));
				s.origin = p_original_local.origin;
			} else {
				s.basis.scale(p_motion + Vector3(1, 1, 1));
				Transform3D base = Transform3D(Basis(), _edit.center);
				s = base * (s * (base.inverse() * p_original));

				// Recalculate orthogonalized scale without moving origin.
				if (p_orthogonal) {
					s.basis = p_original.basis.scaled_orthogonal(p_motion + Vector3(1, 1, 1));
				}
			}

			return s;
		}
		case TRANSFORM_TRANSLATE: {
			if (_edit.snap || spatial_editor->is_snap_enabled()) {
				p_motion.snapf(p_extra);
			}

			if (p_local) {
				return p_original_local.translated_local(p_motion);
			}

			return p_original.translated(p_motion);
		}
		case TRANSFORM_ROTATE: {
			Transform3D r;

			if (p_local) {
				Vector3 axis = p_original_local.basis.xform(p_motion);
				r.basis = Basis(axis.normalized(), p_extra) * p_original_local.basis;
				r.origin = p_original_local.origin;
			} else {
				Basis local = p_original.basis * p_original_local.basis.inverse();
				Vector3 axis = local.xform_inv(p_motion);
				r.basis = local * Basis(axis.normalized(), p_extra) * p_original_local.basis;
				r.origin = Basis(p_motion, p_extra).xform(p_original.origin - _edit.center) + _edit.center;
			}

			return r;
		}
		default: {
			ERR_FAIL_V_MSG(Transform3D(), "Invalid mode in '_compute_transform'");
		}
	}
}

void Node3DEditorViewport::_surface_mouse_enter() {
	if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
		return;
	}

	if (!surface->has_focus() && (!get_viewport()->gui_get_focus_owner() || !get_viewport()->gui_get_focus_owner()->is_text_field())) {
		surface->grab_focus();
	}
}

void Node3DEditorViewport::_surface_mouse_exit() {
	_remove_preview_node();
	_reset_preview_material();
	_remove_preview_material();
}

void Node3DEditorViewport::_surface_focus_enter() {
	view_menu->set_disable_shortcuts(false);
}

void Node3DEditorViewport::_surface_focus_exit() {
	view_menu->set_disable_shortcuts(true);
}

bool Node3DEditorViewport::_is_node_locked(const Node *p_node) const {
	return p_node->get_meta("_edit_lock_", false);
}

void Node3DEditorViewport::_list_select(Ref<InputEventMouseButton> b) {
	Vector<_RayResult> potential_selection_results;
	_find_items_at_pos(b->get_position(), potential_selection_results, b->is_alt_pressed());

	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

	// Filter to a list of nodes which include either the edited scene or nodes directly owned by the edited scene.
	// If a node has an invalid owner, recursively check their parents until a valid node is found.
	for (int i = 0; i < potential_selection_results.size(); i++) {
		Node3D *node = potential_selection_results[i].item;
		while (true) {
			if (node == nullptr || node == edited_scene->get_parent()) {
				break;
			} else {
				Node *node_owner = node->get_owner();
				if (node == edited_scene || node_owner == edited_scene || (node_owner != nullptr && edited_scene->is_editable_instance(node_owner))) {
					if (!selection_results.has(node)) {
						selection_results.append(node);
					}
					break;
				}
			}
			node = Object::cast_to<Node3D>(node->get_parent());
		}
	}

	clicked_wants_append = b->is_shift_pressed();

	if (selection_results.size() == 1) {
		clicked = selection_results[0]->get_instance_id();
		selection_results.clear();

		if (clicked.is_valid()) {
			_select_clicked(b->is_alt_pressed());
		}
	} else if (!selection_results.is_empty()) {
		NodePath root_path = get_tree()->get_edited_scene_root()->get_path();
		StringName root_name = root_path.get_name(root_path.get_name_count() - 1);

		for (int i = 0; i < selection_results.size(); i++) {
			Node3D *spat = selection_results[i];

			Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(spat, "Node");

			String node_path = "/" + root_name + "/" + root_path.rel_path_to(spat->get_path());

			int locked = 0;
			if (_is_node_locked(spat)) {
				locked = 1;
			} else {
				Node *ed_scene = EditorNode::get_singleton()->get_edited_scene();
				Node *node = spat;

				while (node && node != ed_scene->get_parent()) {
					Node3D *selected_tmp = Object::cast_to<Node3D>(node);
					if (selected_tmp && node->has_meta("_edit_group_")) {
						locked = 2;
					}
					node = node->get_parent();
				}
			}

			String suffix;
			if (locked == 1) {
				suffix = " (" + TTR("Locked") + ")";
			} else if (locked == 2) {
				suffix = " (" + TTR("Grouped") + ")";
			}
			selection_menu->add_item((String)spat->get_name() + suffix);
			selection_menu->set_item_icon(i, icon);
			selection_menu->set_item_metadata(i, node_path);
			selection_menu->set_item_tooltip(i, String(spat->get_name()) + "\nType: " + spat->get_class() + "\nPath: " + node_path);
		}

		selection_results_menu = selection_results;
		selection_menu->set_position(get_screen_position() + b->get_position());
		selection_menu->reset_size();
		selection_menu->popup();
	}
}

// This is only active during instant transforms,
// to capture and wrap mouse events outside the control.
void Node3DEditorViewport::input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(!_edit.instant);
	Ref<InputEventMouseMotion> m = p_event;

	if (m.is_valid()) {
		_edit.mouse_pos += _get_warped_mouse_motion(p_event);
		update_transform(_get_key_modifier(m) == Key::SHIFT);
	}
}

void Node3DEditorViewport::_sinput(const Ref<InputEvent> &p_event) {
	if (previewing || get_viewport()->gui_get_drag_data()) {
		return; //do NONE
	}

	EditorPlugin::AfterGUIInput after = EditorPlugin::AFTER_GUI_INPUT_PASS;
	{
		EditorNode *en = EditorNode::get_singleton();
		EditorPluginList *force_input_forwarding_list = en->get_editor_plugins_force_input_forwarding();
		if (!force_input_forwarding_list->is_empty()) {
			EditorPlugin::AfterGUIInput discard = force_input_forwarding_list->forward_3d_gui_input(camera, p_event, true);
			if (discard == EditorPlugin::AFTER_GUI_INPUT_STOP) {
				return;
			}
			if (discard == EditorPlugin::AFTER_GUI_INPUT_CUSTOM) {
				after = EditorPlugin::AFTER_GUI_INPUT_CUSTOM;
			}
		}
	}
	{
		EditorNode *en = EditorNode::get_singleton();
		EditorPluginList *over_plugin_list = en->get_editor_plugins_over();
		if (!over_plugin_list->is_empty()) {
			EditorPlugin::AfterGUIInput discard = over_plugin_list->forward_3d_gui_input(camera, p_event, false);
			if (discard == EditorPlugin::AFTER_GUI_INPUT_STOP) {
				return;
			}
			if (discard == EditorPlugin::AFTER_GUI_INPUT_CUSTOM) {
				after = EditorPlugin::AFTER_GUI_INPUT_CUSTOM;
			}
		}
	}

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		emit_signal(SNAME("clicked"));

		ViewportNavMouseButton orbit_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/orbit_mouse_button").operator int();
		ViewportNavMouseButton pan_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/pan_mouse_button").operator int();
		ViewportNavMouseButton zoom_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/zoom_mouse_button").operator int();

		const real_t zoom_factor = 1 + (ZOOM_FREELOOK_MULTIPLIER - 1) * b->get_factor();
		switch (b->get_button_index()) {
			case MouseButton::WHEEL_UP: {
				if (is_freelook_active()) {
					scale_freelook_speed(zoom_factor);
				} else {
					scale_cursor_distance(1.0 / zoom_factor);
				}
			} break;
			case MouseButton::WHEEL_DOWN: {
				if (is_freelook_active()) {
					scale_freelook_speed(1.0 / zoom_factor);
				} else {
					scale_cursor_distance(zoom_factor);
				}
			} break;
			case MouseButton::RIGHT: {
				if (b->is_pressed()) {
					if (_edit.gizmo.is_valid()) {
						// Restore.
						_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_handle_secondary, _edit.gizmo_initial_value, true);
						_edit.gizmo = Ref<EditorNode3DGizmo>();
					}

					if (_edit.mode == TRANSFORM_NONE) {
						if (orbit_mouse_preference == NAVIGATION_RIGHT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_2")) {
							break;
						} else if (pan_mouse_preference == NAVIGATION_RIGHT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_2")) {
							break;
						} else if (zoom_mouse_preference == NAVIGATION_RIGHT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_2")) {
							break;
						}
					}

					if (b->is_alt_pressed()) {
						_list_select(b);
						return;
					}

					if (_edit.mode != TRANSFORM_NONE) {
						cancel_transform();
						break;
					}

					const Key mod = _get_key_modifier(b);
					if (!orthogonal) {
						if (mod == _get_key_modifier_setting("editors/3d/freelook/freelook_activation_modifier")) {
							set_freelook_active(true);
						}
					}
				} else {
					set_freelook_active(false);
				}

				if (freelook_active && !surface->has_focus()) {
					// Focus usually doesn't trigger on right-click, but in case of freelook it should,
					// otherwise using keyboard navigation would misbehave
					surface->grab_focus();
				}

			} break;
			case MouseButton::MIDDLE: {
				if (b->is_pressed() && _edit.mode != TRANSFORM_NONE) {
					if (orbit_mouse_preference == NAVIGATION_MIDDLE_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_2")) {
						break;
					} else if (pan_mouse_preference == NAVIGATION_MIDDLE_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_2")) {
						break;
					} else if (zoom_mouse_preference == NAVIGATION_MIDDLE_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_2")) {
						break;
					}

					switch (_edit.plane) {
						case TRANSFORM_VIEW: {
							_edit.plane = TRANSFORM_X_AXIS;
							set_message(TTR("X-Axis Transform."), 2);
							view_type = VIEW_TYPE_USER;
							_update_name();
						} break;
						case TRANSFORM_X_AXIS: {
							_edit.plane = TRANSFORM_Y_AXIS;
							set_message(TTR("Y-Axis Transform."), 2);

						} break;
						case TRANSFORM_Y_AXIS: {
							_edit.plane = TRANSFORM_Z_AXIS;
							set_message(TTR("Z-Axis Transform."), 2);

						} break;
						case TRANSFORM_Z_AXIS: {
							_edit.plane = TRANSFORM_VIEW;
							// TRANSLATORS: This refers to the transform of the view plane.
							set_message(TTR("View Plane Transform."), 2);

						} break;
						case TRANSFORM_YZ:
						case TRANSFORM_XZ:
						case TRANSFORM_XY: {
						} break;
					}
				}
			} break;
			case MouseButton::LEFT: {
				if (b->is_pressed()) {
					clicked_wants_append = b->is_shift_pressed();

					if (_edit.mode != TRANSFORM_NONE && (_edit.instant || collision_reposition)) {
						commit_transform();
						break; // just commit the edit, stop processing the event so we don't deselect the object
					}
					if (orbit_mouse_preference == NAVIGATION_LEFT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_2")) {
						break;
					} else if (pan_mouse_preference == NAVIGATION_LEFT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_2")) {
						break;
					} else if (zoom_mouse_preference == NAVIGATION_LEFT_MOUSE && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_2")) {
						break;
					}

					if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_RULER) {
						EditorNode::get_singleton()->get_scene_root()->add_child(ruler);
						collision_reposition = true;
						break;
					}

					if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_LIST_SELECT) {
						_list_select(b);
						break;
					}

					_edit.mouse_pos = b->get_position();
					_edit.original_mouse_pos = b->get_position();
					_edit.snap = spatial_editor->is_snap_enabled();
					_edit.mode = TRANSFORM_NONE;
					_edit.original = spatial_editor->get_gizmo_transform(); // To prevent to break when flipping with scale.

					bool can_select_gizmos = spatial_editor->get_single_selected_node();

					{
						int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
						int idx2 = view_menu->get_popup()->get_item_index(VIEW_TRANSFORM_GIZMO);
						can_select_gizmos = can_select_gizmos && view_menu->get_popup()->is_item_checked(idx);
						transform_gizmo_visible = view_menu->get_popup()->is_item_checked(idx2);
					}

					// Gizmo handles
					if (can_select_gizmos) {
						Vector<Ref<Node3DGizmo>> gizmos = spatial_editor->get_single_selected_node()->get_gizmos();

						bool intersected_handle = false;
						for (int i = 0; i < gizmos.size(); i++) {
							Ref<EditorNode3DGizmo> seg = gizmos[i];

							if (seg.is_null()) {
								continue;
							}

							int gizmo_handle = -1;
							bool gizmo_secondary = false;
							seg->handles_intersect_ray(camera, _edit.mouse_pos, b->is_shift_pressed(), gizmo_handle, gizmo_secondary);
							if (gizmo_handle != -1) {
								_edit.gizmo = seg;
								seg->begin_handle_action(gizmo_handle, gizmo_secondary);
								_edit.gizmo_handle = gizmo_handle;
								_edit.gizmo_handle_secondary = gizmo_secondary;
								_edit.gizmo_initial_value = seg->get_handle_value(gizmo_handle, gizmo_secondary);
								intersected_handle = true;
								break;
							}
						}

						if (intersected_handle) {
							break;
						}
					}

					// Transform gizmo
					if (_transform_gizmo_select(_edit.mouse_pos)) {
						break;
					}

					// Subgizmos
					if (can_select_gizmos) {
						Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(spatial_editor->get_single_selected_node());
						Vector<Ref<Node3DGizmo>> gizmos = spatial_editor->get_single_selected_node()->get_gizmos();

						bool intersected_subgizmo = false;
						for (int i = 0; i < gizmos.size(); i++) {
							Ref<EditorNode3DGizmo> seg = gizmos[i];

							if (seg.is_null()) {
								continue;
							}

							int subgizmo_id = seg->subgizmos_intersect_ray(camera, _edit.mouse_pos);
							if (subgizmo_id != -1) {
								ERR_CONTINUE(!se);
								if (b->is_shift_pressed()) {
									if (se->subgizmos.has(subgizmo_id)) {
										se->subgizmos.erase(subgizmo_id);
									} else {
										se->subgizmos.insert(subgizmo_id, seg->get_subgizmo_transform(subgizmo_id));
									}
								} else {
									se->subgizmos.clear();
									se->subgizmos.insert(subgizmo_id, seg->get_subgizmo_transform(subgizmo_id));
								}

								if (se->subgizmos.is_empty()) {
									se->gizmo = Ref<EditorNode3DGizmo>();
								} else {
									se->gizmo = seg;
								}

								seg->redraw();
								spatial_editor->update_transform_gizmo();
								intersected_subgizmo = true;
								break;
							}
						}

						if (intersected_subgizmo) {
							break;
						}
					}

					clicked = ObjectID();

					bool node_selected = get_selected_count() > 0;

					if (node_selected && ((spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT && b->is_command_or_control_pressed()) || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_ROTATE)) {
						begin_transform(TRANSFORM_ROTATE, false);
						break;
					}

					if (node_selected && spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_MOVE) {
						begin_transform(TRANSFORM_TRANSLATE, false);
						break;
					}

					if (node_selected && spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SCALE) {
						begin_transform(TRANSFORM_SCALE, false);
						break;
					}

					if (after != EditorPlugin::AFTER_GUI_INPUT_CUSTOM) {
						// Single item selection.
						clicked = _select_ray(b->get_position());

						selection_in_progress = true;

						if (clicked.is_null()) {
							// Default to region select.
							cursor.region_select = true;
							cursor.region_begin = b->get_position();
							cursor.region_end = b->get_position();
						}
					}

					surface->queue_redraw();
				} else {
					if (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_RULER) {
						EditorNode::get_singleton()->get_scene_root()->remove_child(ruler);
						ruler_start_point->set_visible(false);
						ruler_end_point->set_visible(false);
						ruler_label->set_visible(false);
						collision_reposition = false;
						break;
					}

					if (_edit.gizmo.is_valid()) {
						_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_handle_secondary, _edit.gizmo_initial_value, false);
						spatial_editor->get_single_selected_node()->update_gizmos();
						_edit.gizmo = Ref<EditorNode3DGizmo>();
						break;
					}

					if (after != EditorPlugin::AFTER_GUI_INPUT_CUSTOM) {
						selection_in_progress = false;

						if (clicked.is_valid()) {
							_select_clicked(false);
						}

						if (cursor.region_select) {
							_select_region();
							cursor.region_select = false;
							surface->queue_redraw();
						}
					}

					if (!_edit.instant && _edit.mode != TRANSFORM_NONE) {
						Node3D *selected = spatial_editor->get_single_selected_node();
						Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;

						if (se && se->gizmo.is_valid()) {
							Vector<int> ids;
							Vector<Transform3D> restore;

							for (const KeyValue<int, Transform3D> &GE : se->subgizmos) {
								ids.push_back(GE.key);
								restore.push_back(GE.value);
							}

							se->gizmo->commit_subgizmos(ids, restore, false);
						} else {
							if (_edit.original_mouse_pos != _edit.mouse_pos) {
								commit_transform();
							}
						}
						_edit.mode = TRANSFORM_NONE;
						set_message("");
						spatial_editor->update_transform_gizmo();
					}
					surface->queue_redraw();
				}

			} break;
			default:
				break;
		}
	}

	ViewportNavMouseButton orbit_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/orbit_mouse_button").operator int();
	ViewportNavMouseButton pan_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/pan_mouse_button").operator int();
	ViewportNavMouseButton zoom_mouse_preference = (ViewportNavMouseButton)EDITOR_GET("editors/3d/navigation/zoom_mouse_button").operator int();
	bool orbit_mod_pressed = _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_orbit_modifier_2");
	bool pan_mod_pressed = _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_pan_modifier_2");
	bool zoom_mod_pressed = _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_1") && _is_nav_modifier_pressed("spatial_editor/viewport_zoom_modifier_2");
	int orbit_mod_input_count = _get_shortcut_input_count("spatial_editor/viewport_orbit_modifier_1") + _get_shortcut_input_count("spatial_editor/viewport_orbit_modifier_2");
	int pan_mod_input_count = _get_shortcut_input_count("spatial_editor/viewport_pan_modifier_1") + _get_shortcut_input_count("spatial_editor/viewport_pan_modifier_2");
	int zoom_mod_input_count = _get_shortcut_input_count("spatial_editor/viewport_zoom_modifier_1") + _get_shortcut_input_count("spatial_editor/viewport_zoom_modifier_2");
	bool orbit_not_empty = !_is_shortcut_empty("spatial_editor/viewport_orbit_modifier_1") || !_is_shortcut_empty("spatial_editor/viewport_orbit_modifier_2");
	bool pan_not_empty = !_is_shortcut_empty("spatial_editor/viewport_pan_modifier_1") || !_is_shortcut_empty("spatial_editor/viewport_pan_modifier_2");
	bool zoom_not_empty = !_is_shortcut_empty("spatial_editor/viewport_zoom_modifier_1") || !_is_shortcut_empty("spatial_editor/viewport_zoom_modifier_2");
	Vector<ShortcutCheckSet> shortcut_check_sets;
	shortcut_check_sets.push_back(ShortcutCheckSet(orbit_mod_pressed, orbit_not_empty, orbit_mod_input_count, orbit_mouse_preference, NAVIGATION_ORBIT));
	shortcut_check_sets.push_back(ShortcutCheckSet(pan_mod_pressed, pan_not_empty, pan_mod_input_count, pan_mouse_preference, NAVIGATION_PAN));
	shortcut_check_sets.push_back(ShortcutCheckSet(zoom_mod_pressed, zoom_not_empty, zoom_mod_input_count, zoom_mouse_preference, NAVIGATION_ZOOM));
	shortcut_check_sets.sort_custom<ShortcutCheckSetComparator>();

	Ref<InputEventMouseMotion> m = p_event;

	// Instant transforms process mouse motion in input() to handle wrapping.
	if (m.is_valid() && !_edit.instant) {
		_edit.mouse_pos = m->get_position();

		if (spatial_editor->get_single_selected_node()) {
			Vector<Ref<Node3DGizmo>> gizmos = spatial_editor->get_single_selected_node()->get_gizmos();

			Ref<EditorNode3DGizmo> found_gizmo;
			int found_handle = -1;
			bool found_handle_secondary = false;

			for (int i = 0; i < gizmos.size(); i++) {
				Ref<EditorNode3DGizmo> seg = gizmos[i];
				if (seg.is_null()) {
					continue;
				}

				seg->handles_intersect_ray(camera, _edit.mouse_pos, false, found_handle, found_handle_secondary);

				if (found_handle != -1) {
					found_gizmo = seg;
					break;
				}
			}

			if (found_gizmo.is_valid()) {
				spatial_editor->select_gizmo_highlight_axis(-1);
			}

			bool current_hover_handle_secondary = false;
			int curreny_hover_handle = spatial_editor->get_current_hover_gizmo_handle(current_hover_handle_secondary);
			if (found_gizmo != spatial_editor->get_current_hover_gizmo() || found_handle != curreny_hover_handle || found_handle_secondary != current_hover_handle_secondary) {
				spatial_editor->set_current_hover_gizmo(found_gizmo);
				spatial_editor->set_current_hover_gizmo_handle(found_handle, found_handle_secondary);
				spatial_editor->get_single_selected_node()->update_gizmos();
			}
		}

		if (spatial_editor->get_current_hover_gizmo().is_null() && !m->get_button_mask().has_flag(MouseButtonMask::LEFT) && _edit.gizmo.is_null()) {
			_transform_gizmo_select(_edit.mouse_pos, true);
		}

		NavigationMode nav_mode = NAVIGATION_NONE;

		if (_edit.gizmo.is_valid()) {
			_edit.gizmo->set_handle(_edit.gizmo_handle, _edit.gizmo_handle_secondary, camera, m->get_position());
			Variant v = _edit.gizmo->get_handle_value(_edit.gizmo_handle, _edit.gizmo_handle_secondary);
			String n = _edit.gizmo->get_handle_name(_edit.gizmo_handle, _edit.gizmo_handle_secondary);
			set_message(n + ": " + String(v));

		} else if (m->get_button_mask().has_flag(MouseButtonMask::LEFT)) {
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_LEFT_MOUSE, shortcut_check_sets, false);
			if (change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			} else {
				const bool movement_threshold_passed = _edit.original_mouse_pos.distance_to(_edit.mouse_pos) > 8 * EDSCALE;

				if (selection_in_progress && movement_threshold_passed && clicked.is_valid()) {
					if (clicked_wants_append || !editor_selection->is_selected(Object::cast_to<Node>(ObjectDB::get_instance(clicked)))) {
						cursor.region_select = true;
						cursor.region_begin = _edit.original_mouse_pos;
						clicked = ObjectID();
					}
				}

				if (cursor.region_select) {
					cursor.region_end = m->get_position();
					surface->queue_redraw();
					return;
				}

				if (clicked.is_valid() && movement_threshold_passed) {
					_compute_edit(_edit.original_mouse_pos);
					clicked = ObjectID();
					_edit.mode = TRANSFORM_TRANSLATE;
				}

				if (_edit.mode == TRANSFORM_NONE || _edit.numeric_input != 0 || _edit.numeric_next_decimal != 0) {
					return;
				}

				update_transform(_get_key_modifier(m) == Key::SHIFT);
			}
		} else if (m->get_button_mask().has_flag(MouseButtonMask::RIGHT) || freelook_active) {
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_RIGHT_MOUSE, shortcut_check_sets, false);
			if (m->get_button_mask().has_flag(MouseButtonMask::RIGHT) && change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			} else if (freelook_active) {
				nav_mode = NAVIGATION_LOOK;
			} else if (orthogonal) {
				nav_mode = NAVIGATION_PAN;
			}

		} else if (m->get_button_mask().has_flag(MouseButtonMask::MIDDLE)) {
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_MIDDLE_MOUSE, shortcut_check_sets, false);
			if (change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			}

		} else if (m->get_button_mask().has_flag(MouseButtonMask::MB_XBUTTON1)) {
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_MOUSE_4, shortcut_check_sets, false);
			if (change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			}

		} else if (m->get_button_mask().has_flag(MouseButtonMask::MB_XBUTTON2)) {
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_MOUSE_5, shortcut_check_sets, false);
			if (change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			}

		} else if (EDITOR_GET("editors/3d/navigation/emulate_3_button_mouse")) {
			// Handle trackpad (no external mouse) use case
			NavigationMode change_nav_from_shortcut = _get_nav_mode_from_shortcut_check(NAVIGATION_LEFT_MOUSE, shortcut_check_sets, true);
			if (change_nav_from_shortcut != NAVIGATION_NONE) {
				nav_mode = change_nav_from_shortcut;
			}
		}

		switch (nav_mode) {
			case NAVIGATION_PAN: {
				_nav_pan(m, _get_warped_mouse_motion(m));

			} break;

			case NAVIGATION_ZOOM: {
				_nav_zoom(m, m->get_relative());

			} break;

			case NAVIGATION_ORBIT: {
				_nav_orbit(m, _get_warped_mouse_motion(m));

			} break;

			case NAVIGATION_LOOK: {
				_nav_look(m, _get_warped_mouse_motion(m));

			} break;

			default: {
			}
		}
	}

	Ref<InputEventMagnifyGesture> magnify_gesture = p_event;
	if (magnify_gesture.is_valid()) {
		if (is_freelook_active()) {
			scale_freelook_speed(magnify_gesture->get_factor());
		} else {
			scale_cursor_distance(1.0 / magnify_gesture->get_factor());
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		NavigationMode nav_mode = NAVIGATION_NONE;

		for (const ShortcutCheckSet &shortcut_check_set : shortcut_check_sets) {
			if (shortcut_check_set.mod_pressed) {
				nav_mode = shortcut_check_set.result_nav_mode;
				break;
			}
		}

		switch (nav_mode) {
			case NAVIGATION_PAN: {
				_nav_pan(pan_gesture, -pan_gesture->get_delta());

			} break;

			case NAVIGATION_ZOOM: {
				_nav_zoom(pan_gesture, pan_gesture->get_delta());

			} break;

			case NAVIGATION_ORBIT: {
				_nav_orbit(pan_gesture, -pan_gesture->get_delta());

			} break;

			case NAVIGATION_LOOK: {
				_nav_look(pan_gesture, pan_gesture->get_delta());

			} break;

			default: {
			}
		}
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (!k->is_pressed()) {
			return;
		}

		if (_edit.instant) {
			// In a Blender-style transform, numbers set the magnitude of the transform.
			// E.g. pressing g4.5x means "translate 4.5 units along the X axis".
			// Use the Unicode value because we care about the text, not the actual keycode.
			// This ensures numbers work consistently across different keyboard language layouts.
			bool processed = true;
			Key key = k->get_physical_keycode();
			char32_t unicode = k->get_unicode();
			if (unicode >= '0' && unicode <= '9') {
				uint32_t value = uint32_t(unicode - Key::KEY_0);
				if (_edit.numeric_next_decimal < 0) {
					_edit.numeric_input = _edit.numeric_input + value * Math::pow(10.0, _edit.numeric_next_decimal--);
				} else {
					_edit.numeric_input = _edit.numeric_input * 10 + value;
				}
				update_transform_numeric();
			} else if (unicode == '-') {
				_edit.numeric_negate = !_edit.numeric_negate;
				update_transform_numeric();
			} else if (unicode == '.') {
				if (_edit.numeric_next_decimal == 0) {
					_edit.numeric_next_decimal = -1;
				}
			} else if (key == Key::ENTER || key == Key::KP_ENTER || key == Key::SPACE) {
				commit_transform();
			} else {
				processed = false;
			}

			if (processed) {
				// Ignore mouse inputs once we receive a numeric input.
				set_process_input(false);
				accept_event();
				return;
			}
		}

		if (EDITOR_GET("editors/3d/navigation/emulate_numpad")) {
			const Key code = k->get_physical_keycode();
			if (code >= Key::KEY_0 && code <= Key::KEY_9) {
				k->set_keycode(code - Key::KEY_0 + Key::KP_0);
			}
		}

		if (_edit.mode == TRANSFORM_NONE) {
			if (_edit.gizmo.is_null() && is_freelook_active() && k->get_keycode() == Key::ESCAPE) {
				set_freelook_active(false);
				return;
			}

			if (_edit.gizmo.is_valid() && (k->get_keycode() == Key::ESCAPE || k->get_keycode() == Key::BACKSPACE)) {
				// Restore.
				_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_handle_secondary, _edit.gizmo_initial_value, true);
				_edit.gizmo = Ref<EditorNode3DGizmo>();
			}
			if (k->get_keycode() == Key::ESCAPE && !cursor.region_select) {
				_clear_selected();
				return;
			}
		} else {
			// We're actively transforming, handle keys specially
			TransformPlane new_plane = TRANSFORM_VIEW;
			if (ED_IS_SHORTCUT("spatial_editor/lock_transform_x", p_event)) {
				new_plane = TRANSFORM_X_AXIS;
			} else if (ED_IS_SHORTCUT("spatial_editor/lock_transform_y", p_event)) {
				new_plane = TRANSFORM_Y_AXIS;
			} else if (ED_IS_SHORTCUT("spatial_editor/lock_transform_z", p_event)) {
				new_plane = TRANSFORM_Z_AXIS;
			} else if (_edit.mode != TRANSFORM_ROTATE) { // rotating on a plane doesn't make sense
				if (ED_IS_SHORTCUT("spatial_editor/lock_transform_yz", p_event)) {
					new_plane = TRANSFORM_YZ;
				} else if (ED_IS_SHORTCUT("spatial_editor/lock_transform_xz", p_event)) {
					new_plane = TRANSFORM_XZ;
				} else if (ED_IS_SHORTCUT("spatial_editor/lock_transform_xy", p_event)) {
					new_plane = TRANSFORM_XY;
				}
			}

			if (new_plane != TRANSFORM_VIEW) {
				if (new_plane != _edit.plane) {
					// lock me once and get a global constraint
					_edit.plane = new_plane;
					spatial_editor->set_local_coords_enabled(false);
				} else if (!spatial_editor->are_local_coords_enabled()) {
					// lock me twice and get a local constraint
					spatial_editor->set_local_coords_enabled(true);
				} else {
					// lock me thrice and we're back where we started
					_edit.plane = TRANSFORM_VIEW;
					spatial_editor->set_local_coords_enabled(false);
				}
				if (_edit.numeric_input != 0 || _edit.numeric_next_decimal != 0) {
					update_transform_numeric();
				} else {
					update_transform(Input::get_singleton()->is_key_pressed(Key::SHIFT));
				}
				accept_event();
				return;
			}
		}
		if (ED_IS_SHORTCUT("spatial_editor/snap", p_event)) {
			if (_edit.mode != TRANSFORM_NONE) {
				_edit.snap = !_edit.snap;
			}
		}
		if (ED_IS_SHORTCUT("spatial_editor/bottom_view", p_event)) {
			_menu_option(VIEW_BOTTOM);
		}
		if (ED_IS_SHORTCUT("spatial_editor/top_view", p_event)) {
			_menu_option(VIEW_TOP);
		}
		if (ED_IS_SHORTCUT("spatial_editor/rear_view", p_event)) {
			_menu_option(VIEW_REAR);
		}
		if (ED_IS_SHORTCUT("spatial_editor/front_view", p_event)) {
			_menu_option(VIEW_FRONT);
		}
		if (ED_IS_SHORTCUT("spatial_editor/left_view", p_event)) {
			_menu_option(VIEW_LEFT);
		}
		if (ED_IS_SHORTCUT("spatial_editor/right_view", p_event)) {
			_menu_option(VIEW_RIGHT);
		}
		if (ED_IS_SHORTCUT("spatial_editor/orbit_view_down", p_event)) {
			// Clamp rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
			cursor.x_rot = CLAMP(cursor.x_rot - Math_PI / 12.0, -1.57, 1.57);
			view_type = VIEW_TYPE_USER;
			_update_name();
		}
		if (ED_IS_SHORTCUT("spatial_editor/orbit_view_up", p_event)) {
			// Clamp rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
			cursor.x_rot = CLAMP(cursor.x_rot + Math_PI / 12.0, -1.57, 1.57);
			view_type = VIEW_TYPE_USER;
			_update_name();
		}
		if (ED_IS_SHORTCUT("spatial_editor/orbit_view_right", p_event)) {
			cursor.y_rot -= Math_PI / 12.0;
			view_type = VIEW_TYPE_USER;
			_update_name();
		}
		if (ED_IS_SHORTCUT("spatial_editor/orbit_view_left", p_event)) {
			cursor.y_rot += Math_PI / 12.0;
			view_type = VIEW_TYPE_USER;
			_update_name();
		}
		if (ED_IS_SHORTCUT("spatial_editor/orbit_view_180", p_event)) {
			cursor.y_rot += Math_PI;
			view_type = VIEW_TYPE_USER;
			_update_name();
		}
		if (ED_IS_SHORTCUT("spatial_editor/focus_origin", p_event)) {
			_menu_option(VIEW_CENTER_TO_ORIGIN);
		}
		if (ED_IS_SHORTCUT("spatial_editor/focus_selection", p_event)) {
			_menu_option(VIEW_CENTER_TO_SELECTION);
		}
		if (ED_IS_SHORTCUT("spatial_editor/align_transform_with_view", p_event)) {
			_menu_option(VIEW_ALIGN_TRANSFORM_WITH_VIEW);
		}
		if (ED_IS_SHORTCUT("spatial_editor/align_rotation_with_view", p_event)) {
			_menu_option(VIEW_ALIGN_ROTATION_WITH_VIEW);
		}
		if (ED_IS_SHORTCUT("spatial_editor/insert_anim_key", p_event)) {
			if (!get_selected_count() || _edit.mode != TRANSFORM_NONE) {
				return;
			}

			if (!AnimationPlayerEditor::get_singleton()->get_track_editor()->has_keying()) {
				set_message(TTR("Keying is disabled (no key inserted)."));
				return;
			}

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (Node *E : selection) {
				Node3D *sp = Object::cast_to<Node3D>(E);
				if (!sp) {
					continue;
				}

				spatial_editor->emit_signal(SNAME("transform_key_request"), sp, "", sp->get_transform());
			}

			set_message(TTR("Animation Key Inserted."));
		}
		if (ED_IS_SHORTCUT("spatial_editor/cancel_transform", p_event) && _edit.mode != TRANSFORM_NONE) {
			cancel_transform();
		}
		if (!is_freelook_active() && !k->is_echo()) {
			if (ED_IS_SHORTCUT("spatial_editor/instant_translate", p_event) && (_edit.mode != TRANSFORM_TRANSLATE || collision_reposition)) {
				if (_edit.mode == TRANSFORM_NONE) {
					begin_transform(TRANSFORM_TRANSLATE, true);
				} else if (_edit.instant || collision_reposition) {
					commit_transform();
					begin_transform(TRANSFORM_TRANSLATE, true);
				}
			}
			if (ED_IS_SHORTCUT("spatial_editor/instant_rotate", p_event) && _edit.mode != TRANSFORM_ROTATE) {
				if (_edit.mode == TRANSFORM_NONE) {
					begin_transform(TRANSFORM_ROTATE, true);
				} else if (_edit.instant || collision_reposition) {
					commit_transform();
					begin_transform(TRANSFORM_ROTATE, true);
				}
			}
			if (ED_IS_SHORTCUT("spatial_editor/instant_scale", p_event) && _edit.mode != TRANSFORM_SCALE) {
				if (_edit.mode == TRANSFORM_NONE) {
					begin_transform(TRANSFORM_SCALE, true);
				} else if (_edit.instant || collision_reposition) {
					commit_transform();
					begin_transform(TRANSFORM_SCALE, true);
				}
			}
			if (ED_IS_SHORTCUT("spatial_editor/collision_reposition", p_event) && editor_selection->get_selected_node_list().size() == 1 && !collision_reposition) {
				if (_edit.mode == TRANSFORM_NONE || _edit.instant) {
					if (_edit.mode == TRANSFORM_NONE) {
						_compute_edit(_edit.mouse_pos);
					} else {
						commit_transform();
						_compute_edit(_edit.mouse_pos);
					}
					_edit.mode = TRANSFORM_TRANSLATE;
					collision_reposition = true;
				}
			}
		}

		// Freelook doesn't work in orthogonal mode.
		if (!orthogonal && ED_IS_SHORTCUT("spatial_editor/freelook_toggle", p_event)) {
			set_freelook_active(!is_freelook_active());

		} else if (k->get_keycode() == Key::ESCAPE) {
			set_freelook_active(false);
		}

		if (k->get_keycode() == Key::SPACE) {
			if (!k->is_pressed()) {
				emit_signal(SNAME("toggle_maximize_view"), this);
			}
		}

		if (ED_IS_SHORTCUT("spatial_editor/decrease_fov", p_event)) {
			scale_fov(-0.05);
		}

		if (ED_IS_SHORTCUT("spatial_editor/increase_fov", p_event)) {
			scale_fov(0.05);
		}

		if (ED_IS_SHORTCUT("spatial_editor/reset_fov", p_event)) {
			reset_fov();
		}
	}

	// freelook uses most of the useful shortcuts, like save, so its ok
	// to consider freelook active as end of the line for future events.
	if (freelook_active) {
		accept_event();
	}
}

int Node3DEditorViewport::_get_shortcut_input_count(const String &p_name) {
	Ref<Shortcut> check_shortcut = ED_GET_SHORTCUT(p_name);

	ERR_FAIL_COND_V_MSG(check_shortcut.is_null(), 0, "The Shortcut was null, possible name mismatch.");

	return check_shortcut->get_events().size();
}

Node3DEditorViewport::NavigationMode Node3DEditorViewport::_get_nav_mode_from_shortcut_check(ViewportNavMouseButton p_mouse_button, Vector<ShortcutCheckSet> p_shortcut_check_sets, bool p_use_not_empty) {
	if (p_use_not_empty) {
		for (const ShortcutCheckSet &shortcut_check_set : p_shortcut_check_sets) {
			if (shortcut_check_set.mod_pressed && shortcut_check_set.shortcut_not_empty) {
				return shortcut_check_set.result_nav_mode;
			}
		}
	} else {
		for (const ShortcutCheckSet &shortcut_check_set : p_shortcut_check_sets) {
			if (shortcut_check_set.mouse_preference == p_mouse_button && shortcut_check_set.mod_pressed) {
				return shortcut_check_set.result_nav_mode;
			}
		}
	}

	return NAVIGATION_NONE;
}

void Node3DEditorViewport::_nav_pan(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	const NavigationScheme nav_scheme = (NavigationScheme)EDITOR_GET("editors/3d/navigation/navigation_scheme").operator int();
	const real_t translation_sensitivity = EDITOR_GET("editors/3d/navigation_feel/translation_sensitivity");

	real_t pan_speed = translation_sensitivity / 150.0;
	if (p_event.is_valid() && nav_scheme == NAVIGATION_MAYA && p_event->is_shift_pressed()) {
		pan_speed *= 10;
	}

	Transform3D camera_transform;

	camera_transform.translate_local(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
	const bool invert_x_axis = EDITOR_GET("editors/3d/navigation/invert_x_axis");
	const bool invert_y_axis = EDITOR_GET("editors/3d/navigation/invert_y_axis");
	Vector3 translation(
			(invert_x_axis ? -1 : 1) * -p_relative.x * pan_speed,
			(invert_y_axis ? -1 : 1) * p_relative.y * pan_speed,
			0);
	translation *= cursor.distance / DISTANCE_DEFAULT;
	camera_transform.translate_local(translation);
	cursor.pos = camera_transform.origin;
}

void Node3DEditorViewport::_nav_zoom(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	const NavigationScheme nav_scheme = (NavigationScheme)EDITOR_GET("editors/3d/navigation/navigation_scheme").operator int();

	real_t zoom_speed = 1 / 80.0;
	if (p_event.is_valid() && nav_scheme == NAVIGATION_MAYA && p_event->is_shift_pressed()) {
		zoom_speed *= 10;
	}

	NavigationZoomStyle zoom_style = (NavigationZoomStyle)EDITOR_GET("editors/3d/navigation/zoom_style").operator int();
	if (zoom_style == NAVIGATION_ZOOM_HORIZONTAL) {
		if (p_relative.x > 0) {
			scale_cursor_distance(1 - p_relative.x * zoom_speed);
		} else if (p_relative.x < 0) {
			scale_cursor_distance(1.0 / (1 + p_relative.x * zoom_speed));
		}
	} else {
		if (p_relative.y > 0) {
			scale_cursor_distance(1 + p_relative.y * zoom_speed);
		} else if (p_relative.y < 0) {
			scale_cursor_distance(1.0 / (1 - p_relative.y * zoom_speed));
		}
	}
}

void Node3DEditorViewport::_nav_orbit(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	if (lock_rotation) {
		_nav_pan(p_event, p_relative);
		return;
	}

	if (orthogonal && auto_orthogonal) {
		_menu_option(VIEW_PERSPECTIVE);
	}

	const real_t degrees_per_pixel = EDITOR_GET("editors/3d/navigation_feel/orbit_sensitivity");
	const real_t radians_per_pixel = Math::deg_to_rad(degrees_per_pixel);
	const bool invert_y_axis = EDITOR_GET("editors/3d/navigation/invert_y_axis");
	const bool invert_x_axis = EDITOR_GET("editors/3d/navigation/invert_x_axis");

	if (invert_y_axis) {
		cursor.x_rot -= p_relative.y * radians_per_pixel;
	} else {
		cursor.x_rot += p_relative.y * radians_per_pixel;
	}
	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	cursor.x_rot = CLAMP(cursor.x_rot, -1.57, 1.57);

	if (invert_x_axis) {
		cursor.y_rot -= p_relative.x * radians_per_pixel;
	} else {
		cursor.y_rot += p_relative.x * radians_per_pixel;
	}
	view_type = VIEW_TYPE_USER;
	_update_name();
}

void Node3DEditorViewport::_nav_look(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	if (orthogonal) {
		_nav_pan(p_event, p_relative);
		return;
	}

	if (orthogonal && auto_orthogonal) {
		_menu_option(VIEW_PERSPECTIVE);
	}

	// Scale mouse sensitivity with camera FOV scale when zoomed in to make it easier to point at things.
	const real_t degrees_per_pixel = real_t(EDITOR_GET("editors/3d/freelook/freelook_sensitivity")) * MIN(1.0, cursor.fov_scale);
	const real_t radians_per_pixel = Math::deg_to_rad(degrees_per_pixel);
	const bool invert_y_axis = EDITOR_GET("editors/3d/navigation/invert_y_axis");

	// Note: do NOT assume the camera has the "current" transform, because it is interpolated and may have "lag".
	const Transform3D prev_camera_transform = to_camera_transform(cursor);

	if (invert_y_axis) {
		cursor.x_rot -= p_relative.y * radians_per_pixel;
	} else {
		cursor.x_rot += p_relative.y * radians_per_pixel;
	}
	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	cursor.x_rot = CLAMP(cursor.x_rot, -1.57, 1.57);

	cursor.y_rot += p_relative.x * radians_per_pixel;

	// Look is like the opposite of Orbit: the focus point rotates around the camera
	Transform3D camera_transform = to_camera_transform(cursor);
	Vector3 pos = camera_transform.xform(Vector3(0, 0, 0));
	Vector3 prev_pos = prev_camera_transform.xform(Vector3(0, 0, 0));
	Vector3 diff = prev_pos - pos;
	cursor.pos += diff;

	view_type = VIEW_TYPE_USER;
	_update_name();
}

void Node3DEditorViewport::set_freelook_active(bool active_now) {
	if (!freelook_active && active_now) {
		// Sync camera cursor to cursor to "cut" interpolation jumps due to changing referential
		cursor = camera_cursor;

		// Make sure eye_pos is synced, because freelook referential is eye pos rather than orbit pos
		Vector3 forward = to_camera_transform(cursor).basis.xform(Vector3(0, 0, -1));
		cursor.eye_pos = cursor.pos - cursor.distance * forward;
		// Also sync the camera cursor, otherwise switching to freelook will be trippy if inertia is active
		camera_cursor.eye_pos = cursor.eye_pos;

		if (EDITOR_GET("editors/3d/freelook/freelook_speed_zoom_link")) {
			// Re-adjust freelook speed from the current zoom level
			real_t base_speed = EDITOR_GET("editors/3d/freelook/freelook_base_speed");
			freelook_speed = base_speed * cursor.distance;
		}

		previous_mouse_position = get_local_mouse_position();

		// Hide mouse like in an FPS (warping doesn't work)
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);

	} else if (freelook_active && !active_now) {
		// Sync camera cursor to cursor to "cut" interpolation jumps due to changing referential
		cursor = camera_cursor;

		// Restore mouse
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);

		// Restore the previous mouse position when leaving freelook mode.
		// This is done because leaving `Input.MOUSE_MODE_CAPTURED` will center the cursor
		// due to OS limitations.
		warp_mouse(previous_mouse_position);
	}

	freelook_active = active_now;
}

void Node3DEditorViewport::scale_fov(real_t p_fov_offset) {
	cursor.fov_scale = CLAMP(cursor.fov_scale + p_fov_offset, 0.1, 2.5);
	surface->queue_redraw();
}

void Node3DEditorViewport::reset_fov() {
	cursor.fov_scale = 1.0;
	surface->queue_redraw();
}

void Node3DEditorViewport::scale_cursor_distance(real_t scale) {
	real_t min_distance = MAX(camera->get_near() * 4, ZOOM_FREELOOK_MIN);
	real_t max_distance = MIN(camera->get_far() / 4, ZOOM_FREELOOK_MAX);
	if (unlikely(min_distance > max_distance)) {
		cursor.distance = (min_distance + max_distance) / 2;
	} else {
		cursor.distance = CLAMP(cursor.distance * scale, min_distance, max_distance);
	}

	if (cursor.distance == max_distance || cursor.distance == min_distance) {
		zoom_failed_attempts_count++;
	} else {
		zoom_failed_attempts_count = 0;
	}

	zoom_indicator_delay = ZOOM_FREELOOK_INDICATOR_DELAY_S;
	surface->queue_redraw();
}

void Node3DEditorViewport::scale_freelook_speed(real_t scale) {
	real_t min_speed = MAX(camera->get_near() * 4, ZOOM_FREELOOK_MIN);
	real_t max_speed = MIN(camera->get_far() / 4, ZOOM_FREELOOK_MAX);
	if (unlikely(min_speed > max_speed)) {
		freelook_speed = (min_speed + max_speed) / 2;
	} else {
		freelook_speed = CLAMP(freelook_speed * scale, min_speed, max_speed);
	}

	zoom_indicator_delay = ZOOM_FREELOOK_INDICATOR_DELAY_S;
	surface->queue_redraw();
}

bool Node3DEditorViewport::_is_nav_modifier_pressed(const String &p_name) {
	return _is_shortcut_empty(p_name) || Input::get_singleton()->is_action_pressed(p_name);
}

bool Node3DEditorViewport::_is_shortcut_empty(const String &p_name) {
	Ref<Shortcut> check_shortcut = ED_GET_SHORTCUT(p_name);

	ERR_FAIL_COND_V_MSG(check_shortcut.is_null(), true, "The Shortcut was null, possible name mismatch.");

	return check_shortcut->get_events().is_empty();
}

Point2 Node3DEditorViewport::_get_warped_mouse_motion(const Ref<InputEventMouseMotion> &p_ev_mouse_motion) const {
	Point2 relative;
	if (bool(EDITOR_GET("editors/3d/navigation/warped_mouse_panning"))) {
		relative = Input::get_singleton()->warp_mouse_motion(p_ev_mouse_motion, surface->get_global_rect());
	} else {
		relative = p_ev_mouse_motion->get_relative();
	}
	return relative;
}

void Node3DEditorViewport::_update_freelook(real_t delta) {
	if (!is_freelook_active()) {
		return;
	}

	const FreelookNavigationScheme navigation_scheme = (FreelookNavigationScheme)EDITOR_GET("editors/3d/freelook/freelook_navigation_scheme").operator int();

	Vector3 forward;
	if (navigation_scheme == FREELOOK_FULLY_AXIS_LOCKED) {
		// Forward/backward keys will always go straight forward/backward, never moving on the Y axis.
		forward = Vector3(0, 0, -1).rotated(Vector3(0, 1, 0), camera->get_rotation().y);
	} else {
		// Forward/backward keys will be relative to the camera pitch.
		forward = camera->get_transform().basis.xform(Vector3(0, 0, -1));
	}

	const Vector3 right = camera->get_transform().basis.xform(Vector3(1, 0, 0));

	Vector3 up;
	if (navigation_scheme == FREELOOK_PARTIALLY_AXIS_LOCKED || navigation_scheme == FREELOOK_FULLY_AXIS_LOCKED) {
		// Up/down keys will always go up/down regardless of camera pitch.
		up = Vector3(0, 1, 0);
	} else {
		// Up/down keys will be relative to the camera pitch.
		up = camera->get_transform().basis.xform(Vector3(0, 1, 0));
	}

	Vector3 direction;

	// Use actions from the inputmap, as this is the only way to reliably detect input in this method.
	// See #54469 for more discussion and explanation.
	Input *inp = Input::get_singleton();
	if (inp->is_action_pressed("spatial_editor/freelook_left")) {
		direction -= right;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_right")) {
		direction += right;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_forward")) {
		direction += forward;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_backwards")) {
		direction -= forward;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_up")) {
		direction += up;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_down")) {
		direction -= up;
	}

	real_t speed = freelook_speed;

	if (inp->is_action_pressed("spatial_editor/freelook_speed_modifier")) {
		speed *= 3.0;
	}
	if (inp->is_action_pressed("spatial_editor/freelook_slow_modifier")) {
		speed *= 0.333333;
	}

	const Vector3 motion = direction * speed * delta;
	cursor.pos += motion;
	cursor.eye_pos += motion;
}

void Node3DEditorViewport::set_message(const String &p_message, float p_time) {
	message = p_message;
	message_time = p_time;
}

void Node3DEditorPlugin::edited_scene_changed() {
	for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
		Node3DEditorViewport *viewport = Node3DEditor::get_singleton()->get_editor_viewport(i);
		if (viewport->is_visible()) {
			viewport->notification(Control::NOTIFICATION_VISIBILITY_CHANGED);
		}
	}
}

void Node3DEditorViewport::_project_settings_changed() {
	// Update shadow atlas if changed.
	int shadowmap_size = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_size");
	bool shadowmap_16_bits = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_16_bits");
	int atlas_q0 = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_0_subdiv");
	int atlas_q1 = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_1_subdiv");
	int atlas_q2 = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_2_subdiv");
	int atlas_q3 = GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/atlas_quadrant_3_subdiv");

	viewport->set_positional_shadow_atlas_size(shadowmap_size);
	viewport->set_positional_shadow_atlas_16_bits(shadowmap_16_bits);
	viewport->set_positional_shadow_atlas_quadrant_subdiv(0, Viewport::PositionalShadowAtlasQuadrantSubdiv(atlas_q0));
	viewport->set_positional_shadow_atlas_quadrant_subdiv(1, Viewport::PositionalShadowAtlasQuadrantSubdiv(atlas_q1));
	viewport->set_positional_shadow_atlas_quadrant_subdiv(2, Viewport::PositionalShadowAtlasQuadrantSubdiv(atlas_q2));
	viewport->set_positional_shadow_atlas_quadrant_subdiv(3, Viewport::PositionalShadowAtlasQuadrantSubdiv(atlas_q3));

	_update_shrink();

	// Update MSAA, screen-space AA and debanding if changed

	const int msaa_mode = GLOBAL_GET("rendering/anti_aliasing/quality/msaa_3d");
	viewport->set_msaa_3d(Viewport::MSAA(msaa_mode));
	const int ssaa_mode = GLOBAL_GET("rendering/anti_aliasing/quality/screen_space_aa");
	viewport->set_screen_space_aa(Viewport::ScreenSpaceAA(ssaa_mode));
	const bool use_taa = GLOBAL_GET("rendering/anti_aliasing/quality/use_taa");
	viewport->set_use_taa(use_taa);

	const bool transparent_background = GLOBAL_GET("rendering/viewport/transparent_background");
	viewport->set_transparent_background(transparent_background);

	const bool use_hdr_2d = GLOBAL_GET("rendering/viewport/hdr_2d");
	viewport->set_use_hdr_2d(use_hdr_2d);

	const bool use_debanding = GLOBAL_GET("rendering/anti_aliasing/quality/use_debanding");
	viewport->set_use_debanding(use_debanding);

	const bool use_occlusion_culling = GLOBAL_GET("rendering/occlusion_culling/use_occlusion_culling");
	viewport->set_use_occlusion_culling(use_occlusion_culling);

	const float mesh_lod_threshold = GLOBAL_GET("rendering/mesh_lod/lod_change/threshold_pixels");
	viewport->set_mesh_lod_threshold(mesh_lod_threshold);

	const Viewport::Scaling3DMode scaling_3d_mode = Viewport::Scaling3DMode(int(GLOBAL_GET("rendering/scaling_3d/mode")));
	viewport->set_scaling_3d_mode(scaling_3d_mode);

	const float scaling_3d_scale = GLOBAL_GET("rendering/scaling_3d/scale");
	viewport->set_scaling_3d_scale(scaling_3d_scale);

	const float fsr_sharpness = GLOBAL_GET("rendering/scaling_3d/fsr_sharpness");
	viewport->set_fsr_sharpness(fsr_sharpness);

	const float texture_mipmap_bias = GLOBAL_GET("rendering/textures/default_filters/texture_mipmap_bias");
	viewport->set_texture_mipmap_bias(texture_mipmap_bias);

	const Viewport::AnisotropicFiltering anisotropic_filtering_level = Viewport::AnisotropicFiltering(int(GLOBAL_GET("rendering/textures/default_filters/anisotropic_filtering_level")));
	viewport->set_anisotropic_filtering_level(anisotropic_filtering_level);
}

void Node3DEditorViewport::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &Node3DEditorViewport::_project_settings_changed));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			bool vp_visible = is_visible_in_tree();

			set_process(vp_visible);
			set_physics_process(vp_visible);

			if (vp_visible) {
				orthogonal = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL));
				_update_name();
				_update_camera(0);
			} else {
				set_freelook_active(false);
			}
			callable_mp(this, &Node3DEditorViewport::update_transform_gizmo_view).call_deferred();
		} break;

		case NOTIFICATION_RESIZED: {
			callable_mp(this, &Node3DEditorViewport::update_transform_gizmo_view).call_deferred();
		} break;

		case NOTIFICATION_PROCESS: {
			if (ruler->is_inside_tree()) {
				Vector3 start_pos = ruler_start_point->get_global_position();
				Vector3 end_pos = ruler_end_point->get_global_position();

				geometry->clear_surfaces();
				geometry->surface_begin(Mesh::PRIMITIVE_LINES);
				geometry->surface_add_vertex(start_pos);
				geometry->surface_add_vertex(end_pos);
				geometry->surface_end();

				float distance = start_pos.distance_to(end_pos);
				ruler_label->set_text(TS->format_number(vformat("%.3f m", distance)));

				Vector3 center = (start_pos + end_pos) / 2;
				Vector2 screen_position = camera->unproject_position(center) - (ruler_label->get_custom_minimum_size() / 2);
				ruler_label->set_position(screen_position);
			}

			real_t delta = get_process_delta_time();

			if (zoom_indicator_delay > 0) {
				zoom_indicator_delay -= delta;
				if (zoom_indicator_delay <= 0) {
					surface->queue_redraw();
					zoom_limit_label->hide();
				}
			}

			_update_navigation_controls_visibility();
			_update_freelook(delta);

			Node *scene_root = SceneTreeDock::get_singleton()->get_editor_data()->get_edited_scene_root();
			if (previewing_cinema && scene_root != nullptr) {
				Camera3D *cam = scene_root->get_viewport()->get_camera_3d();
				if (cam != nullptr && cam != previewing) {
					//then switch the viewport's camera to the scene's viewport camera
					if (previewing != nullptr) {
						previewing->disconnect(SceneStringName(tree_exited), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
					}
					previewing = cam;
					previewing->connect(SceneStringName(tree_exited), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
					RS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), cam->get_camera());
					surface->queue_redraw();
				}
			}

			if (_camera_moved_externally()) {
				// If camera moved after this plugin last set it, presumably a tool script has moved it, accept the new camera transform as the cursor position.
				_apply_camera_transform_to_cursor();
			}

			_update_camera(delta);

			const HashMap<Node *, Object *> &selection = editor_selection->get_selection();

			bool changed = false;
			bool exist = false;

			for (const KeyValue<Node *, Object *> &E : selection) {
				Node3D *sp = Object::cast_to<Node3D>(E.key);
				if (!sp) {
					continue;
				}

				Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
				if (!se) {
					continue;
				}

				Transform3D t = sp->get_global_gizmo_transform();
				AABB new_aabb = _calculate_spatial_bounds(sp);

				exist = true;
				if (se->last_xform == t && se->aabb == new_aabb && !se->last_xform_dirty) {
					continue;
				}
				changed = true;
				se->last_xform_dirty = false;
				se->last_xform = t;

				se->aabb = new_aabb;

				Transform3D t_offset = t;

				// apply AABB scaling before item's global transform
				{
					const Vector3 offset(0.005, 0.005, 0.005);
					Basis aabb_s;
					aabb_s.scale(se->aabb.size + offset);
					t.translate_local(se->aabb.position - offset / 2);
					t.basis = t.basis * aabb_s;
				}
				{
					const Vector3 offset(0.01, 0.01, 0.01);
					Basis aabb_s;
					aabb_s.scale(se->aabb.size + offset);
					t_offset.translate_local(se->aabb.position - offset / 2);
					t_offset.basis = t_offset.basis * aabb_s;
				}

				RenderingServer::get_singleton()->instance_set_transform(se->sbox_instance, t);
				RenderingServer::get_singleton()->instance_set_transform(se->sbox_instance_offset, t_offset);
				RenderingServer::get_singleton()->instance_set_transform(se->sbox_instance_xray, t);
				RenderingServer::get_singleton()->instance_set_transform(se->sbox_instance_xray_offset, t_offset);
			}

			if (changed || (spatial_editor->is_gizmo_visible() && !exist)) {
				spatial_editor->update_transform_gizmo();
			}

			if (message_time > 0) {
				if (message != last_message) {
					surface->queue_redraw();
					last_message = message;
				}

				message_time -= get_process_delta_time();
				if (message_time < 0) {
					surface->queue_redraw();
				}
			}

			bool show_info = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_INFORMATION));
			if (show_info != info_panel->is_visible()) {
				info_panel->set_visible(show_info);
			}

			Camera3D *current_camera;

			if (previewing) {
				current_camera = previewing;
			} else {
				current_camera = camera;
			}

			if (show_info) {
				const String viewport_size = vformat(U"%d × %d", viewport->get_size().x, viewport->get_size().y);
				String text;
				text += vformat(TTR("X: %s\n"), rtos(current_camera->get_position().x).pad_decimals(1));
				text += vformat(TTR("Y: %s\n"), rtos(current_camera->get_position().y).pad_decimals(1));
				text += vformat(TTR("Z: %s\n"), rtos(current_camera->get_position().z).pad_decimals(1));
				text += "\n";
				text += vformat(
						TTR("Size: %s (%.1fMP)\n"),
						viewport_size,
						viewport->get_size().x * viewport->get_size().y * 0.000001);

				text += "\n";
				text += vformat(TTR("Objects: %d\n"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_OBJECTS_IN_FRAME));
				text += vformat(TTR("Primitives: %d\n"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_PRIMITIVES_IN_FRAME));
				text += vformat(TTR("Draw Calls: %d"), viewport->get_render_info(Viewport::RENDER_INFO_TYPE_VISIBLE, Viewport::RENDER_INFO_DRAW_CALLS_IN_FRAME));

				info_label->set_text(text);
			}

			// FPS Counter.
			bool show_fps = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_FRAME_TIME));

			if (show_fps != frame_time_panel->is_visible()) {
				frame_time_panel->set_visible(show_fps);
				RS::get_singleton()->viewport_set_measure_render_time(viewport->get_viewport_rid(), show_fps);
				for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
					// Initialize to 120 FPS, so that the initial estimation until we get enough data is always reasonable.
					cpu_time_history[i] = 8.333333;
					gpu_time_history[i] = 8.333333;
				}
				cpu_time_history_index = 0;
				gpu_time_history_index = 0;
			}
			if (show_fps) {
				cpu_time_history[cpu_time_history_index] = RS::get_singleton()->viewport_get_measured_render_time_cpu(viewport->get_viewport_rid());
				cpu_time_history_index = (cpu_time_history_index + 1) % FRAME_TIME_HISTORY;
				double cpu_time = 0.0;
				for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
					cpu_time += cpu_time_history[i];
				}
				cpu_time /= FRAME_TIME_HISTORY;
				// Prevent unrealistically low values.
				cpu_time = MAX(0.01, cpu_time);

				gpu_time_history[gpu_time_history_index] = RS::get_singleton()->viewport_get_measured_render_time_gpu(viewport->get_viewport_rid());
				gpu_time_history_index = (gpu_time_history_index + 1) % FRAME_TIME_HISTORY;
				double gpu_time = 0.0;
				for (int i = 0; i < FRAME_TIME_HISTORY; i++) {
					gpu_time += gpu_time_history[i];
				}
				gpu_time /= FRAME_TIME_HISTORY;
				// Prevent division by zero for the FPS counter (and unrealistically low values).
				// This limits the reported FPS to 100000.
				gpu_time = MAX(0.01, gpu_time);

				// Color labels depending on performance level ("good" = green, "OK" = yellow, "bad" = red).
				// Middle point is at 15 ms.
				cpu_time_label->set_text(vformat(TTR("CPU Time: %s ms"), rtos(cpu_time).pad_decimals(2)));
				cpu_time_label->add_theme_color_override(
						SceneStringName(font_color),
						frame_time_gradient->get_color_at_offset(
								Math::remap(cpu_time, 0, 30, 0, 1)));

				gpu_time_label->set_text(vformat(TTR("GPU Time: %s ms"), rtos(gpu_time).pad_decimals(2)));
				// Middle point is at 15 ms.
				gpu_time_label->add_theme_color_override(
						SceneStringName(font_color),
						frame_time_gradient->get_color_at_offset(
								Math::remap(gpu_time, 0, 30, 0, 1)));

				const double fps = 1000.0 / gpu_time;
				fps_label->set_text(vformat(TTR("FPS: %d"), fps));
				// Middle point is at 60 FPS.
				fps_label->add_theme_color_override(
						SceneStringName(font_color),
						frame_time_gradient->get_color_at_offset(
								Math::remap(fps, 110, 10, 0, 1)));
			}

			bool show_cinema = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW));
			cinema_label->set_visible(show_cinema);
			if (show_cinema) {
				float cinema_half_width = cinema_label->get_size().width / 2.0f;
				cinema_label->set_anchor_and_offset(SIDE_LEFT, 0.5f, -cinema_half_width);
			}

			if (lock_rotation) {
				float locked_half_width = locked_label->get_size().width / 2.0f;
				locked_label->set_anchor_and_offset(SIDE_LEFT, 0.5f, -locked_half_width);
			}
		} break;

		case NOTIFICATION_PHYSICS_PROCESS: {
			if (collision_reposition) {
				Node3D *selected_node = nullptr;

				if (ruler->is_inside_tree()) {
					if (ruler_start_point->is_visible()) {
						selected_node = ruler_end_point;
					} else {
						selected_node = ruler_start_point;
					}
				} else {
					List<Node *> &selection = editor_selection->get_selected_node_list();
					if (selection.size() == 1) {
						selected_node = Object::cast_to<Node3D>(selection.front()->get());
					}
				}

				if (selected_node) {
					if (!ruler->is_inside_tree()) {
						double snap = EDITOR_GET("interface/inspector/default_float_step");
						int snap_step_decimals = Math::range_step_decimals(snap);
						set_message(TTR("Translating:") + " (" + String::num(selected_node->get_global_position().x, snap_step_decimals) + ", " +
								String::num(selected_node->get_global_position().y, snap_step_decimals) + ", " + String::num(selected_node->get_global_position().z, snap_step_decimals) + ")");
					}

					selected_node->set_global_position(spatial_editor->snap_point(_get_instance_position(_edit.mouse_pos, selected_node)));

					if (ruler->is_inside_tree() && !ruler_start_point->is_visible()) {
						ruler_end_point->set_global_position(ruler_start_point->get_global_position());
						ruler_start_point->set_visible(true);
						ruler_end_point->set_visible(true);
						ruler_label->set_visible(true);
					}
				}
			}

			if (!update_preview_node) {
				return;
			}
			if (preview_node->is_inside_tree()) {
				preview_node_pos = spatial_editor->snap_point(_get_instance_position(preview_node_viewport_pos, preview_node));
				double snap = EDITOR_GET("interface/inspector/default_float_step");
				int snap_step_decimals = Math::range_step_decimals(snap);
				set_message(TTR("Instantiating:") + " (" + String::num(preview_node_pos.x, snap_step_decimals) + ", " +
						String::num(preview_node_pos.y, snap_step_decimals) + ", " + String::num(preview_node_pos.z, snap_step_decimals) + ")");
				Transform3D preview_gl_transform = Transform3D(Basis(), preview_node_pos);
				preview_node->set_global_transform(preview_gl_transform);
				if (!preview_node->is_visible()) {
					preview_node->show();
				}
			}
			update_preview_node = false;
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_OUT:
		case NOTIFICATION_WM_WINDOW_FOCUS_OUT: {
			set_freelook_active(false);
			cursor.region_select = false;
			surface->queue_redraw();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			surface->connect(SceneStringName(draw), callable_mp(this, &Node3DEditorViewport::_draw));
			surface->connect(SceneStringName(gui_input), callable_mp(this, &Node3DEditorViewport::_sinput));
			surface->connect(SceneStringName(mouse_entered), callable_mp(this, &Node3DEditorViewport::_surface_mouse_enter));
			surface->connect(SceneStringName(mouse_exited), callable_mp(this, &Node3DEditorViewport::_surface_mouse_exit));
			surface->connect(SceneStringName(focus_entered), callable_mp(this, &Node3DEditorViewport::_surface_focus_enter));
			surface->connect(SceneStringName(focus_exited), callable_mp(this, &Node3DEditorViewport::_surface_focus_exit));

			_init_gizmo_instance(index);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_finish_gizmo_instances();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			view_menu->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			preview_camera->set_button_icon(get_editor_theme_icon(SNAME("Camera3D")));
			Control *gui_base = EditorNode::get_singleton()->get_gui_base();

			const Ref<StyleBox> &information_3d_stylebox = gui_base->get_theme_stylebox(SNAME("Information3dViewport"), EditorStringName(EditorStyles));

			view_menu->begin_bulk_theme_override();
			view_menu->add_theme_style_override(CoreStringName(normal), information_3d_stylebox);
			view_menu->add_theme_style_override("normal_mirrored", information_3d_stylebox);
			view_menu->add_theme_style_override(SceneStringName(hover), information_3d_stylebox);
			view_menu->add_theme_style_override("hover_mirrored", information_3d_stylebox);
			view_menu->add_theme_style_override("hover_pressed", information_3d_stylebox);
			view_menu->add_theme_style_override("hover_pressed_mirrored", information_3d_stylebox);
			view_menu->add_theme_style_override(SceneStringName(pressed), information_3d_stylebox);
			view_menu->add_theme_style_override("pressed_mirrored", information_3d_stylebox);
			view_menu->add_theme_style_override("focus", information_3d_stylebox);
			view_menu->add_theme_style_override("focus_mirrored", information_3d_stylebox);
			view_menu->add_theme_style_override("disabled", information_3d_stylebox);
			view_menu->add_theme_style_override("disabled_mirrored", information_3d_stylebox);
			view_menu->end_bulk_theme_override();

			preview_camera->begin_bulk_theme_override();
			preview_camera->add_theme_style_override(CoreStringName(normal), information_3d_stylebox);
			preview_camera->add_theme_style_override("normal_mirrored", information_3d_stylebox);
			preview_camera->add_theme_style_override(SceneStringName(hover), information_3d_stylebox);
			preview_camera->add_theme_style_override("hover_mirrored", information_3d_stylebox);
			preview_camera->add_theme_style_override("hover_pressed", information_3d_stylebox);
			preview_camera->add_theme_style_override("hover_pressed_mirrored", information_3d_stylebox);
			preview_camera->add_theme_style_override(SceneStringName(pressed), information_3d_stylebox);
			preview_camera->add_theme_style_override("pressed_mirrored", information_3d_stylebox);
			preview_camera->add_theme_style_override("focus", information_3d_stylebox);
			preview_camera->add_theme_style_override("focus_mirrored", information_3d_stylebox);
			preview_camera->add_theme_style_override("disabled", information_3d_stylebox);
			preview_camera->add_theme_style_override("disabled_mirrored", information_3d_stylebox);
			preview_camera->end_bulk_theme_override();

			frame_time_gradient->set_color(0, get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
			frame_time_gradient->set_color(1, get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
			frame_time_gradient->set_color(2, get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

			info_panel->add_theme_style_override(SceneStringName(panel), information_3d_stylebox);

			frame_time_panel->add_theme_style_override(SceneStringName(panel), information_3d_stylebox);
			// Set a minimum width to prevent the width from changing all the time
			// when numbers vary rapidly. This minimum width is set based on a
			// GPU time of 999.99 ms in the current editor language.
			const float min_width = get_theme_font(SNAME("main"), EditorStringName(EditorFonts))->get_string_size(vformat(TTR("GPU Time: %s ms"), 999.99)).x;
			frame_time_panel->set_custom_minimum_size(Size2(min_width, 0) * EDSCALE);
			frame_time_vbox->add_theme_constant_override("separation", Math::round(-1 * EDSCALE));

			cinema_label->add_theme_style_override(CoreStringName(normal), information_3d_stylebox);
			locked_label->add_theme_style_override(CoreStringName(normal), information_3d_stylebox);

			ruler_label->add_theme_color_override(SceneStringName(font_color), Color(1.0, 0.9, 0.0, 1.0));
			ruler_label->add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 1.0));
			ruler_label->add_theme_constant_override("outline_size", 4 * EDSCALE);
			ruler_label->add_theme_font_size_override(SceneStringName(font_size), 15 * EDSCALE);
			ruler_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
		} break;

		case NOTIFICATION_DRAG_END: {
			// Clear preview material when dropped outside applicable object.
			if (spatial_editor->get_preview_material().is_valid() && !is_drag_successful()) {
				_reset_preview_material();
				_remove_preview_material();
			} else {
				_remove_preview_node();
			}
		} break;
	}
}

static void draw_indicator_bar(Control &p_surface, real_t p_fill, const Ref<Texture2D> p_icon, const Ref<Font> p_font, int p_font_size, const String &p_text, const Color &p_color) {
	// Adjust bar size from control height
	const Vector2 surface_size = p_surface.get_size();
	const real_t h = surface_size.y / 2.0;
	const real_t y = (surface_size.y - h) / 2.0;

	const Rect2 r(10 * EDSCALE, y, 6 * EDSCALE, h);
	const real_t sy = r.size.y * p_fill;

	// Note: because this bar appears over the viewport, it has to stay readable for any background color
	// Draw both neutral dark and bright colors to account this
	p_surface.draw_rect(r, p_color * Color(1, 1, 1, 0.2));
	p_surface.draw_rect(Rect2(r.position.x, r.position.y + r.size.y - sy, r.size.x, sy), p_color * Color(1, 1, 1, 0.6));
	p_surface.draw_rect(r.grow(1), Color(0, 0, 0, 0.7), false, Math::round(EDSCALE));

	const Vector2 icon_size = p_icon->get_size();
	const Vector2 icon_pos = Vector2(r.position.x - (icon_size.x - r.size.x) / 2, r.position.y + r.size.y + 2 * EDSCALE);
	p_surface.draw_texture(p_icon, icon_pos, p_color);

	// Draw text below the bar (for speed/zoom information).
	p_surface.draw_string_outline(p_font, Vector2(icon_pos.x, icon_pos.y + icon_size.y + 16 * EDSCALE), p_text, HORIZONTAL_ALIGNMENT_LEFT, -1.f, p_font_size, Math::round(2 * EDSCALE), Color(0, 0, 0));
	p_surface.draw_string(p_font, Vector2(icon_pos.x, icon_pos.y + icon_size.y + 16 * EDSCALE), p_text, HORIZONTAL_ALIGNMENT_LEFT, -1.f, p_font_size, p_color);
}

void Node3DEditorViewport::_draw() {
	EditorPluginList *over_plugin_list = EditorNode::get_singleton()->get_editor_plugins_over();
	if (!over_plugin_list->is_empty()) {
		over_plugin_list->forward_3d_draw_over_viewport(surface);
	}

	EditorPluginList *force_over_plugin_list = EditorNode::get_singleton()->get_editor_plugins_force_over();
	if (!force_over_plugin_list->is_empty()) {
		force_over_plugin_list->forward_3d_force_draw_over_viewport(surface);
	}

	if (surface->has_focus()) {
		Size2 size = surface->get_size();
		Rect2 r = Rect2(Point2(), size);
		get_theme_stylebox(SNAME("FocusViewport"), EditorStringName(EditorStyles))->draw(surface->get_canvas_item(), r);
	}

	if (cursor.region_select) {
		const Rect2 selection_rect = Rect2(cursor.region_begin, cursor.region_end - cursor.region_begin);

		surface->draw_rect(
				selection_rect,
				get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor)));

		surface->draw_rect(
				selection_rect,
				get_theme_color(SNAME("box_selection_stroke_color"), EditorStringName(Editor)),
				false,
				Math::round(EDSCALE));
	}

	RID ci = surface->get_canvas_item();

	if (message_time > 0) {
		Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
		int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
		Point2 msgpos = Point2(10 * EDSCALE, get_size().y - 14 * EDSCALE);
		font->draw_string(ci, msgpos + Point2(1, 1), message, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0, 0, 0, 0.8));
		font->draw_string(ci, msgpos + Point2(-1, -1), message, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0, 0, 0, 0.8));
		font->draw_string(ci, msgpos, message, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(1, 1, 1, 1));
	}

	if (_edit.mode == TRANSFORM_ROTATE && _edit.show_rotation_line) {
		Point2 center = point_to_screen(_edit.center);

		Color handle_color;
		switch (_edit.plane) {
			case TRANSFORM_X_AXIS:
				handle_color = get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor));
				break;
			case TRANSFORM_Y_AXIS:
				handle_color = get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor));
				break;
			case TRANSFORM_Z_AXIS:
				handle_color = get_theme_color(SNAME("axis_z_color"), EditorStringName(Editor));
				break;
			default:
				handle_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
				break;
		}
		handle_color = handle_color.from_hsv(handle_color.get_h(), 0.25, 1.0, 1);

		RenderingServer::get_singleton()->canvas_item_add_line(
				ci,
				_edit.mouse_pos,
				center,
				handle_color,
				Math::round(2 * EDSCALE));
	}
	if (previewing) {
		Size2 ss = Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
		float aspect = ss.aspect();
		Size2 s = get_size();

		Rect2 draw_rect;

		switch (previewing->get_keep_aspect_mode()) {
			case Camera3D::KEEP_WIDTH: {
				draw_rect.size = Size2(s.width, s.width / aspect);
				draw_rect.position.x = 0;
				draw_rect.position.y = (s.height - draw_rect.size.y) * 0.5;

			} break;
			case Camera3D::KEEP_HEIGHT: {
				draw_rect.size = Size2(s.height * aspect, s.height);
				draw_rect.position.y = 0;
				draw_rect.position.x = (s.width - draw_rect.size.x) * 0.5;

			} break;
		}

		draw_rect = Rect2(Vector2(), s).intersection(draw_rect);

		surface->draw_rect(draw_rect, Color(0.6, 0.6, 0.1, 0.5), false, Math::round(2 * EDSCALE));

	} else {
		if (zoom_indicator_delay > 0.0) {
			if (is_freelook_active()) {
				// Show speed

				real_t min_speed = MAX(camera->get_near() * 4, ZOOM_FREELOOK_MIN);
				real_t max_speed = MIN(camera->get_far() / 4, ZOOM_FREELOOK_MAX);
				real_t scale_length = (max_speed - min_speed);

				if (!Math::is_zero_approx(scale_length)) {
					real_t logscale_t = 1.0 - Math::log1p(freelook_speed - min_speed) / Math::log1p(scale_length);

					// Display the freelook speed to help the user get a better sense of scale.
					const int precision = freelook_speed < 1.0 ? 2 : 1;
					draw_indicator_bar(
							*surface,
							1.0 - logscale_t,
							get_editor_theme_icon(SNAME("ViewportSpeed")),
							get_theme_font(SceneStringName(font), SNAME("Label")),
							get_theme_font_size(SceneStringName(font_size), SNAME("Label")),
							vformat("%s m/s", String::num(freelook_speed).pad_decimals(precision)),
							Color(1.0, 0.95, 0.7));
				}

			} else {
				// Show zoom
				zoom_limit_label->set_visible(zoom_failed_attempts_count > 15);

				real_t min_distance = MAX(camera->get_near() * 4, ZOOM_FREELOOK_MIN);
				real_t max_distance = MIN(camera->get_far() / 4, ZOOM_FREELOOK_MAX);
				real_t scale_length = (max_distance - min_distance);

				if (!Math::is_zero_approx(scale_length)) {
					real_t logscale_t = 1.0 - Math::log1p(cursor.distance - min_distance) / Math::log1p(scale_length);

					// Display the zoom center distance to help the user get a better sense of scale.
					const int precision = cursor.distance < 1.0 ? 2 : 1;
					draw_indicator_bar(
							*surface,
							logscale_t,
							get_editor_theme_icon(SNAME("ViewportZoom")),
							get_theme_font(SceneStringName(font), SNAME("Label")),
							get_theme_font_size(SceneStringName(font_size), SNAME("Label")),
							vformat("%s m", String::num(cursor.distance).pad_decimals(precision)),
							Color(0.7, 0.95, 1.0));
				}
			}
		}
	}
}

bool Node3DEditorViewport::_camera_moved_externally() {
	Transform3D t = camera->get_global_transform();
	return !t.is_equal_approx(last_camera_transform);
}

void Node3DEditorViewport::_apply_camera_transform_to_cursor() {
	// Effectively the reverse of to_camera_transform, use camera transform to set cursor position and rotation.
	Transform3D camera_transform = camera->get_camera_transform();
	cursor.pos = camera_transform.origin;
	cursor.x_rot = -camera_transform.basis.get_euler().x;
	cursor.y_rot = -camera_transform.basis.get_euler().y;
}

void Node3DEditorViewport::_menu_option(int p_option) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	switch (p_option) {
		case VIEW_TOP: {
			cursor.y_rot = 0;
			cursor.x_rot = Math_PI / 2.0;
			set_message(TTR("Top View."), 2);
			view_type = VIEW_TYPE_TOP;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_BOTTOM: {
			cursor.y_rot = 0;
			cursor.x_rot = -Math_PI / 2.0;
			set_message(TTR("Bottom View."), 2);
			view_type = VIEW_TYPE_BOTTOM;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_LEFT: {
			cursor.x_rot = 0;
			cursor.y_rot = Math_PI / 2.0;
			set_message(TTR("Left View."), 2);
			view_type = VIEW_TYPE_LEFT;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_RIGHT: {
			cursor.x_rot = 0;
			cursor.y_rot = -Math_PI / 2.0;
			set_message(TTR("Right View."), 2);
			view_type = VIEW_TYPE_RIGHT;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_FRONT: {
			cursor.x_rot = 0;
			cursor.y_rot = 0;
			set_message(TTR("Front View."), 2);
			view_type = VIEW_TYPE_FRONT;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_REAR: {
			cursor.x_rot = 0;
			cursor.y_rot = Math_PI;
			set_message(TTR("Rear View."), 2);
			view_type = VIEW_TYPE_REAR;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_CENTER_TO_ORIGIN: {
			cursor.pos = Vector3(0, 0, 0);

		} break;
		case VIEW_CENTER_TO_SELECTION: {
			focus_selection();

		} break;
		case VIEW_ALIGN_TRANSFORM_WITH_VIEW: {
			if (!get_selected_count()) {
				break;
			}

			Transform3D camera_transform = camera->get_global_transform();

			List<Node *> &selection = editor_selection->get_selected_node_list();

			undo_redo->create_action(TTR("Align Transform with View"));

			for (Node *E : selection) {
				Node3D *sp = Object::cast_to<Node3D>(E);
				if (!sp) {
					continue;
				}

				Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
				if (!se) {
					continue;
				}

				Transform3D xform;
				if (orthogonal) {
					xform = sp->get_global_transform();
					xform.basis = Basis::from_euler(camera_transform.basis.get_euler());
				} else {
					xform = camera_transform;
					xform.scale_basis(sp->get_scale());
				}

				if (Object::cast_to<Decal>(E)) {
					// Adjust rotation to match Decal's default orientation.
					// This makes the decal "look" in the same direction as the camera,
					// rather than pointing down relative to the camera orientation.
					xform.basis.rotate_local(Vector3(1, 0, 0), Math_TAU * 0.25);
				}

				Node3D *parent = sp->get_parent_node_3d();
				Transform3D local_xform = parent ? parent->get_global_transform().affine_inverse() * xform : xform;
				undo_redo->add_do_method(sp, "set_transform", local_xform);
				undo_redo->add_undo_method(sp, "set_transform", sp->get_local_gizmo_transform());
			}
			undo_redo->commit_action();

		} break;
		case VIEW_ALIGN_ROTATION_WITH_VIEW: {
			if (!get_selected_count()) {
				break;
			}

			Transform3D camera_transform = camera->get_global_transform();

			List<Node *> &selection = editor_selection->get_selected_node_list();

			undo_redo->create_action(TTR("Align Rotation with View"));
			for (Node *E : selection) {
				Node3D *sp = Object::cast_to<Node3D>(E);
				if (!sp) {
					continue;
				}

				Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
				if (!se) {
					continue;
				}

				Basis basis = camera_transform.basis;

				if (Object::cast_to<Decal>(E)) {
					// Adjust rotation to match Decal's default orientation.
					// This makes the decal "look" in the same direction as the camera,
					// rather than pointing down relative to the camera orientation.
					basis.rotate_local(Vector3(1, 0, 0), Math_TAU * 0.25);
				}

				undo_redo->add_do_method(sp, "set_rotation", basis.get_euler_normalized());
				undo_redo->add_undo_method(sp, "set_rotation", sp->get_rotation());
			}
			undo_redo->commit_action();

		} break;
		case VIEW_ENVIRONMENT: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			if (current) {
				camera->set_environment(Ref<Resource>());
			} else {
				camera->set_environment(Node3DEditor::get_singleton()->get_viewport_environment());
			}

			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_PERSPECTIVE: {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), false);
			orthogonal = false;
			auto_orthogonal = false;
			callable_mp(this, &Node3DEditorViewport::update_transform_gizmo_view).call_deferred();
			_update_camera(0);
			_update_name();

		} break;
		case VIEW_ORTHOGONAL: {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), true);
			orthogonal = true;
			auto_orthogonal = false;
			callable_mp(this, &Node3DEditorViewport::update_transform_gizmo_view).call_deferred();
			_update_camera(0);
			_update_name();
		} break;
		case VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL: {
			_menu_option(orthogonal ? VIEW_PERSPECTIVE : VIEW_ORTHOGONAL);

		} break;
		case VIEW_AUTO_ORTHOGONAL: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			view_menu->get_popup()->set_item_checked(idx, current);
			if (auto_orthogonal) {
				auto_orthogonal = false;
				_update_name();
			}
		} break;
		case VIEW_LOCK_ROTATION: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_LOCK_ROTATION);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			_set_lock_view_rotation(!current);

		} break;
		case VIEW_AUDIO_LISTENER: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			viewport->set_as_audio_listener_3d(current);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_AUDIO_DOPPLER: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			camera->set_doppler_tracking(current ? Camera3D::DOPPLER_TRACKING_IDLE_STEP : Camera3D::DOPPLER_TRACKING_DISABLED);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_CINEMATIC_PREVIEW: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			view_menu->get_popup()->set_item_checked(idx, current);
			previewing_cinema = true;
			_toggle_cinema_preview(current);

			if (current) {
				preview_camera->hide();
			} else {
				if (previewing != nullptr) {
					preview_camera->show();
				}
			}
		} break;
		case VIEW_GIZMOS: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			uint32_t layers = camera->get_cull_mask();
			layers &= ~(1 << GIZMO_EDIT_LAYER);
			if (current) {
				layers |= (1 << GIZMO_EDIT_LAYER);
			}
			camera->set_cull_mask(layers);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_TRANSFORM_GIZMO: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_TRANSFORM_GIZMO);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			transform_gizmo_visible = current;

			spatial_editor->update_transform_gizmo();
			view_menu->get_popup()->set_item_checked(idx, current);
		} break;
		case VIEW_HALF_RESOLUTION: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);
			_update_shrink();
		} break;
		case VIEW_INFORMATION: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_INFORMATION);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);

		} break;
		case VIEW_FRAME_TIME: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_FRAME_TIME);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);
		} break;
		case VIEW_GRID: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_GRID);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			uint32_t layers = camera->get_cull_mask();
			layers &= ~(1 << GIZMO_GRID_LAYER);
			if (current) {
				layers |= (1 << GIZMO_GRID_LAYER);
			}
			camera->set_cull_mask(layers);
			view_menu->get_popup()->set_item_checked(idx, current);
		} break;
		case VIEW_DISPLAY_NORMAL:
		case VIEW_DISPLAY_WIREFRAME:
		case VIEW_DISPLAY_OVERDRAW:
		case VIEW_DISPLAY_UNSHADED:
		case VIEW_DISPLAY_LIGHTING:
		case VIEW_DISPLAY_NORMAL_BUFFER:
		case VIEW_DISPLAY_DEBUG_SHADOW_ATLAS:
		case VIEW_DISPLAY_DEBUG_DIRECTIONAL_SHADOW_ATLAS:
		case VIEW_DISPLAY_DEBUG_VOXEL_GI_ALBEDO:
		case VIEW_DISPLAY_DEBUG_VOXEL_GI_LIGHTING:
		case VIEW_DISPLAY_DEBUG_VOXEL_GI_EMISSION:
		case VIEW_DISPLAY_DEBUG_SCENE_LUMINANCE:
		case VIEW_DISPLAY_DEBUG_SSAO:
		case VIEW_DISPLAY_DEBUG_SSIL:
		case VIEW_DISPLAY_DEBUG_PSSM_SPLITS:
		case VIEW_DISPLAY_DEBUG_DECAL_ATLAS:
		case VIEW_DISPLAY_DEBUG_SDFGI:
		case VIEW_DISPLAY_DEBUG_SDFGI_PROBES:
		case VIEW_DISPLAY_DEBUG_GI_BUFFER:
		case VIEW_DISPLAY_DEBUG_DISABLE_LOD:
		case VIEW_DISPLAY_DEBUG_CLUSTER_OMNI_LIGHTS:
		case VIEW_DISPLAY_DEBUG_CLUSTER_SPOT_LIGHTS:
		case VIEW_DISPLAY_DEBUG_CLUSTER_DECALS:
		case VIEW_DISPLAY_DEBUG_CLUSTER_REFLECTION_PROBES:
		case VIEW_DISPLAY_DEBUG_OCCLUDERS:
		case VIEW_DISPLAY_MOTION_VECTORS:
		case VIEW_DISPLAY_INTERNAL_BUFFER: {
			static const int display_options[] = {
				VIEW_DISPLAY_NORMAL,
				VIEW_DISPLAY_WIREFRAME,
				VIEW_DISPLAY_OVERDRAW,
				VIEW_DISPLAY_UNSHADED,
				VIEW_DISPLAY_LIGHTING,
				VIEW_DISPLAY_NORMAL_BUFFER,
				VIEW_DISPLAY_DEBUG_SHADOW_ATLAS,
				VIEW_DISPLAY_DEBUG_DIRECTIONAL_SHADOW_ATLAS,
				VIEW_DISPLAY_DEBUG_VOXEL_GI_ALBEDO,
				VIEW_DISPLAY_DEBUG_VOXEL_GI_LIGHTING,
				VIEW_DISPLAY_DEBUG_VOXEL_GI_EMISSION,
				VIEW_DISPLAY_DEBUG_SCENE_LUMINANCE,
				VIEW_DISPLAY_DEBUG_SSAO,
				VIEW_DISPLAY_DEBUG_SSIL,
				VIEW_DISPLAY_DEBUG_GI_BUFFER,
				VIEW_DISPLAY_DEBUG_DISABLE_LOD,
				VIEW_DISPLAY_DEBUG_PSSM_SPLITS,
				VIEW_DISPLAY_DEBUG_DECAL_ATLAS,
				VIEW_DISPLAY_DEBUG_SDFGI,
				VIEW_DISPLAY_DEBUG_SDFGI_PROBES,
				VIEW_DISPLAY_DEBUG_CLUSTER_OMNI_LIGHTS,
				VIEW_DISPLAY_DEBUG_CLUSTER_SPOT_LIGHTS,
				VIEW_DISPLAY_DEBUG_CLUSTER_DECALS,
				VIEW_DISPLAY_DEBUG_CLUSTER_REFLECTION_PROBES,
				VIEW_DISPLAY_DEBUG_OCCLUDERS,
				VIEW_DISPLAY_MOTION_VECTORS,
				VIEW_DISPLAY_INTERNAL_BUFFER,
				VIEW_MAX
			};
			static const Viewport::DebugDraw debug_draw_modes[] = {
				Viewport::DEBUG_DRAW_DISABLED,
				Viewport::DEBUG_DRAW_WIREFRAME,
				Viewport::DEBUG_DRAW_OVERDRAW,
				Viewport::DEBUG_DRAW_UNSHADED,
				Viewport::DEBUG_DRAW_LIGHTING,
				Viewport::DEBUG_DRAW_NORMAL_BUFFER,
				Viewport::DEBUG_DRAW_SHADOW_ATLAS,
				Viewport::DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS,
				Viewport::DEBUG_DRAW_VOXEL_GI_ALBEDO,
				Viewport::DEBUG_DRAW_VOXEL_GI_LIGHTING,
				Viewport::DEBUG_DRAW_VOXEL_GI_EMISSION,
				Viewport::DEBUG_DRAW_SCENE_LUMINANCE,
				Viewport::DEBUG_DRAW_SSAO,
				Viewport::DEBUG_DRAW_SSIL,
				Viewport::DEBUG_DRAW_GI_BUFFER,
				Viewport::DEBUG_DRAW_DISABLE_LOD,
				Viewport::DEBUG_DRAW_PSSM_SPLITS,
				Viewport::DEBUG_DRAW_DECAL_ATLAS,
				Viewport::DEBUG_DRAW_SDFGI,
				Viewport::DEBUG_DRAW_SDFGI_PROBES,
				Viewport::DEBUG_DRAW_CLUSTER_OMNI_LIGHTS,
				Viewport::DEBUG_DRAW_CLUSTER_SPOT_LIGHTS,
				Viewport::DEBUG_DRAW_CLUSTER_DECALS,
				Viewport::DEBUG_DRAW_CLUSTER_REFLECTION_PROBES,
				Viewport::DEBUG_DRAW_OCCLUDERS,
				Viewport::DEBUG_DRAW_MOTION_VECTORS,
				Viewport::DEBUG_DRAW_INTERNAL_BUFFER,
			};

			for (int idx = 0; display_options[idx] != VIEW_MAX; idx++) {
				int id = display_options[idx];
				int item_idx = view_menu->get_popup()->get_item_index(id);
				if (item_idx != -1) {
					view_menu->get_popup()->set_item_checked(item_idx, id == p_option);
				}
				item_idx = display_submenu->get_item_index(id);
				if (item_idx != -1) {
					display_submenu->set_item_checked(item_idx, id == p_option);
				}

				if (id == p_option) {
					viewport->set_debug_draw(debug_draw_modes[idx]);
				}
			}
		} break;
	}
}

void Node3DEditorViewport::_set_auto_orthogonal() {
	if (!orthogonal && view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL))) {
		_menu_option(VIEW_ORTHOGONAL);
		auto_orthogonal = true;
	}
}

void Node3DEditorViewport::_preview_exited_scene() {
	preview_camera->disconnect(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview));
	preview_camera->set_pressed(false);
	_toggle_camera_preview(false);
	preview_camera->connect(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview));
	view_menu->show();
}

void Node3DEditorViewport::_init_gizmo_instance(int p_idx) {
	uint32_t layer = 1 << (GIZMO_BASE_LAYER + p_idx);

	for (int i = 0; i < 3; i++) {
		move_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(move_gizmo_instance[i], spatial_editor->get_move_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(move_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(move_gizmo_instance[i], false);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(move_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(move_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(move_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(move_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		move_plane_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(move_plane_gizmo_instance[i], spatial_editor->get_move_plane_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(move_plane_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], false);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(move_plane_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(move_plane_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(move_plane_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(move_plane_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		rotate_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(rotate_gizmo_instance[i], spatial_editor->get_rotate_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(rotate_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], false);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(rotate_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(rotate_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		scale_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(scale_gizmo_instance[i], spatial_editor->get_scale_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(scale_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(scale_gizmo_instance[i], false);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(scale_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(scale_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(scale_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(scale_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		scale_plane_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(scale_plane_gizmo_instance[i], spatial_editor->get_scale_plane_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(scale_plane_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], false);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(scale_plane_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(scale_plane_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(scale_plane_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(scale_plane_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		axis_gizmo_instance[i] = RS::get_singleton()->instance_create();
		RS::get_singleton()->instance_set_base(axis_gizmo_instance[i], spatial_editor->get_axis_gizmo(i)->get_rid());
		RS::get_singleton()->instance_set_scenario(axis_gizmo_instance[i], get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_visible(axis_gizmo_instance[i], true);
		RS::get_singleton()->instance_geometry_set_cast_shadows_setting(axis_gizmo_instance[i], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(axis_gizmo_instance[i], layer);
		RS::get_singleton()->instance_geometry_set_flag(axis_gizmo_instance[i], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(axis_gizmo_instance[i], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
	}

	// Rotation white outline
	rotate_gizmo_instance[3] = RS::get_singleton()->instance_create();
	RS::get_singleton()->instance_set_base(rotate_gizmo_instance[3], spatial_editor->get_rotate_gizmo(3)->get_rid());
	RS::get_singleton()->instance_set_scenario(rotate_gizmo_instance[3], get_tree()->get_root()->get_world_3d()->get_scenario());
	RS::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], false);
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(rotate_gizmo_instance[3], RS::SHADOW_CASTING_SETTING_OFF);
	RS::get_singleton()->instance_set_layer_mask(rotate_gizmo_instance[3], layer);
	RS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[3], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(rotate_gizmo_instance[3], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
}

void Node3DEditorViewport::_finish_gizmo_instances() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	for (int i = 0; i < 3; i++) {
		RS::get_singleton()->free(move_gizmo_instance[i]);
		RS::get_singleton()->free(move_plane_gizmo_instance[i]);
		RS::get_singleton()->free(rotate_gizmo_instance[i]);
		RS::get_singleton()->free(scale_gizmo_instance[i]);
		RS::get_singleton()->free(scale_plane_gizmo_instance[i]);
		RS::get_singleton()->free(axis_gizmo_instance[i]);
	}
	// Rotation white outline
	RS::get_singleton()->free(rotate_gizmo_instance[3]);
}

void Node3DEditorViewport::_toggle_camera_preview(bool p_activate) {
	ERR_FAIL_COND(p_activate && !preview);
	ERR_FAIL_COND(!p_activate && !previewing);

	previewing_camera = p_activate;
	_update_navigation_controls_visibility();

	if (!p_activate) {
		previewing->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
		previewing = nullptr;
		RS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), camera->get_camera()); //restore
		if (!preview) {
			preview_camera->hide();
		}
		surface->queue_redraw();

	} else {
		previewing = preview;
		previewing->connect(SceneStringName(tree_exiting), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
		RS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), preview->get_camera()); //replace
		surface->queue_redraw();
	}
}

void Node3DEditorViewport::_toggle_cinema_preview(bool p_activate) {
	previewing_cinema = p_activate;
	_update_navigation_controls_visibility();

	if (!previewing_cinema) {
		if (previewing != nullptr) {
			previewing->disconnect(SceneStringName(tree_exited), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
		}

		previewing = nullptr;
		RS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), camera->get_camera()); //restore
		preview_camera->set_pressed(false);
		if (!preview) {
			preview_camera->hide();
		} else {
			preview_camera->show();
		}
		view_menu->show();
		surface->queue_redraw();
	}
}

void Node3DEditorViewport::_selection_result_pressed(int p_result) {
	if (selection_results_menu.size() <= p_result) {
		return;
	}

	clicked = selection_results_menu[p_result]->get_instance_id();

	if (clicked.is_valid()) {
		_select_clicked(true);
	}

	selection_results_menu.clear();
}

void Node3DEditorViewport::_selection_menu_hide() {
	selection_results.clear();
	selection_menu->clear();
	selection_menu->reset_size();
}

void Node3DEditorViewport::set_can_preview(Camera3D *p_preview) {
	preview = p_preview;

	if (!preview_camera->is_pressed() && !previewing_cinema) {
		preview_camera->set_visible(p_preview);
	}
}

void Node3DEditorViewport::update_transform_gizmo_view() {
	if (!is_visible_in_tree()) {
		return;
	}

	Transform3D xform = spatial_editor->get_gizmo_transform();

	Transform3D camera_xform = camera->get_transform();

	if (xform.origin.is_equal_approx(camera_xform.origin)) {
		for (int i = 0; i < 3; i++) {
			RenderingServer::get_singleton()->instance_set_visible(move_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(scale_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(axis_gizmo_instance[i], false);
		}
		// Rotation white outline
		RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], false);
		return;
	}

	const Vector3 camz = -camera_xform.get_basis().get_column(2).normalized();
	const Vector3 camy = -camera_xform.get_basis().get_column(1).normalized();
	const Plane p = Plane(camz, camera_xform.origin);
	const real_t gizmo_d = MAX(Math::abs(p.distance_to(xform.origin)), CMP_EPSILON);
	const real_t d0 = camera->unproject_position(camera_xform.origin + camz * gizmo_d).y;
	const real_t d1 = camera->unproject_position(camera_xform.origin + camz * gizmo_d + camy).y;
	const real_t dd = MAX(Math::abs(d0 - d1), CMP_EPSILON);

	const real_t gizmo_size = EDITOR_GET("editors/3d/manipulator_gizmo_size");
	// At low viewport heights, multiply the gizmo scale based on the viewport height.
	// This prevents the gizmo from growing very large and going outside the viewport.
	const int viewport_base_height = 400 * MAX(1, EDSCALE);
	gizmo_scale =
			(gizmo_size / Math::abs(dd)) * MAX(1, EDSCALE) *
			MIN(viewport_base_height, subviewport_container->get_size().height) / viewport_base_height /
			subviewport_container->get_stretch_shrink();
	Vector3 scale = Vector3(1, 1, 1) * gizmo_scale;

	// if the determinant is zero, we should disable the gizmo from being rendered
	// this prevents supplying bad values to the renderer and then having to filter it out again
	if (xform.basis.determinant() == 0) {
		for (int i = 0; i < 3; i++) {
			RenderingServer::get_singleton()->instance_set_visible(move_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(scale_gizmo_instance[i], false);
			RenderingServer::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], false);
		}
		// Rotation white outline
		RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], false);
		return;
	}

	bool show_gizmo = spatial_editor->is_gizmo_visible() && !_edit.instant && transform_gizmo_visible && !collision_reposition;
	for (int i = 0; i < 3; i++) {
		Transform3D axis_angle;
		if (xform.basis.get_column(i).normalized().dot(xform.basis.get_column((i + 1) % 3).normalized()) < 1.0) {
			axis_angle = axis_angle.looking_at(xform.basis.get_column(i).normalized(), xform.basis.get_column((i + 1) % 3).normalized());
		}
		axis_angle.basis.scale(scale);
		axis_angle.origin = xform.origin;
		RenderingServer::get_singleton()->instance_set_transform(move_gizmo_instance[i], axis_angle);
		RenderingServer::get_singleton()->instance_set_visible(move_gizmo_instance[i], show_gizmo && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_MOVE));
		RenderingServer::get_singleton()->instance_set_transform(move_plane_gizmo_instance[i], axis_angle);
		RenderingServer::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], show_gizmo && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_MOVE));
		RenderingServer::get_singleton()->instance_set_transform(rotate_gizmo_instance[i], axis_angle);
		RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], show_gizmo && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_ROTATE));
		RenderingServer::get_singleton()->instance_set_transform(scale_gizmo_instance[i], axis_angle);
		RenderingServer::get_singleton()->instance_set_visible(scale_gizmo_instance[i], show_gizmo && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SCALE));
		RenderingServer::get_singleton()->instance_set_transform(scale_plane_gizmo_instance[i], axis_angle);
		RenderingServer::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], show_gizmo && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SCALE));
		RenderingServer::get_singleton()->instance_set_transform(axis_gizmo_instance[i], xform);
	}

	bool show_axes = spatial_editor->is_gizmo_visible() && _edit.mode != TRANSFORM_NONE;
	RenderingServer *rs = RenderingServer::get_singleton();
	rs->instance_set_visible(axis_gizmo_instance[0], show_axes && (_edit.plane == TRANSFORM_X_AXIS || _edit.plane == TRANSFORM_XY || _edit.plane == TRANSFORM_XZ));
	rs->instance_set_visible(axis_gizmo_instance[1], show_axes && (_edit.plane == TRANSFORM_Y_AXIS || _edit.plane == TRANSFORM_XY || _edit.plane == TRANSFORM_YZ));
	rs->instance_set_visible(axis_gizmo_instance[2], show_axes && (_edit.plane == TRANSFORM_Z_AXIS || _edit.plane == TRANSFORM_XZ || _edit.plane == TRANSFORM_YZ));

	// Rotation white outline
	xform.orthonormalize();
	xform.basis.scale(scale);
	RenderingServer::get_singleton()->instance_set_transform(rotate_gizmo_instance[3], xform);
	RenderingServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], spatial_editor->is_gizmo_visible() && !_edit.instant && transform_gizmo_visible && !collision_reposition && (spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == Node3DEditor::TOOL_MODE_ROTATE));
}

void Node3DEditorViewport::set_state(const Dictionary &p_state) {
	if (p_state.has("position")) {
		cursor.pos = p_state["position"];
	}
	if (p_state.has("x_rotation")) {
		cursor.x_rot = p_state["x_rotation"];
	}
	if (p_state.has("y_rotation")) {
		cursor.y_rot = p_state["y_rotation"];
	}
	if (p_state.has("distance")) {
		cursor.distance = p_state["distance"];
	}
	if (p_state.has("orthogonal")) {
		bool orth = p_state["orthogonal"];
		_menu_option(orth ? VIEW_ORTHOGONAL : VIEW_PERSPECTIVE);
	}
	if (p_state.has("view_type")) {
		view_type = ViewType(p_state["view_type"].operator int());
		_update_name();
	}
	if (p_state.has("auto_orthogonal")) {
		auto_orthogonal = p_state["auto_orthogonal"];
		_update_name();
	}
	if (p_state.has("auto_orthogonal_enabled")) {
		bool enabled = p_state["auto_orthogonal_enabled"];
		view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL), enabled);
	}
	if (p_state.has("display_mode")) {
		int display = p_state["display_mode"];

		int idx = view_menu->get_popup()->get_item_index(display);
		if (idx != -1 && !view_menu->get_popup()->is_item_checked(idx)) {
			_menu_option(display);
		} else {
			idx = display_submenu->get_item_index(display);
			if (idx != -1 && !display_submenu->is_item_checked(idx)) {
				_menu_option(display);
			}
		}
	}
	if (p_state.has("lock_rotation")) {
		_set_lock_view_rotation(p_state["lock_rotation"]);
	}
	if (p_state.has("use_environment")) {
		bool env = p_state["use_environment"];

		if (env != camera->get_environment().is_valid()) {
			_menu_option(VIEW_ENVIRONMENT);
		}
	}
	if (p_state.has("listener")) {
		bool listener = p_state["listener"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER);
		viewport->set_as_audio_listener_3d(listener);
		view_menu->get_popup()->set_item_checked(idx, listener);
	}
	if (p_state.has("doppler")) {
		bool doppler = p_state["doppler"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER);
		camera->set_doppler_tracking(doppler ? Camera3D::DOPPLER_TRACKING_IDLE_STEP : Camera3D::DOPPLER_TRACKING_DISABLED);
		view_menu->get_popup()->set_item_checked(idx, doppler);
	}
	if (p_state.has("gizmos")) {
		bool gizmos = p_state["gizmos"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
		if (view_menu->get_popup()->is_item_checked(idx) != gizmos) {
			_menu_option(VIEW_GIZMOS);
		}
	}
	if (p_state.has("transform_gizmo")) {
		bool transform_gizmo = p_state["transform_gizmo"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_TRANSFORM_GIZMO);
		if (view_menu->get_popup()->is_item_checked(idx) != transform_gizmo) {
			_menu_option(VIEW_TRANSFORM_GIZMO);
		}
	}
	if (p_state.has("grid")) {
		bool grid = p_state["grid"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_GRID);
		if (view_menu->get_popup()->is_item_checked(idx) != grid) {
			_menu_option(VIEW_GRID);
		}
	}
	if (p_state.has("information")) {
		bool information = p_state["information"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_INFORMATION);
		if (view_menu->get_popup()->is_item_checked(idx) != information) {
			_menu_option(VIEW_INFORMATION);
		}
	}
	if (p_state.has("frame_time")) {
		bool fps = p_state["frame_time"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_FRAME_TIME);
		if (view_menu->get_popup()->is_item_checked(idx) != fps) {
			_menu_option(VIEW_FRAME_TIME);
		}
	}
	if (p_state.has("half_res")) {
		bool half_res = p_state["half_res"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION);
		view_menu->get_popup()->set_item_checked(idx, half_res);
		_update_shrink();
	}
	if (p_state.has("cinematic_preview")) {
		previewing_cinema = p_state["cinematic_preview"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW);
		view_menu->get_popup()->set_item_checked(idx, previewing_cinema);
	}

	if (preview_camera->is_connected(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview))) {
		preview_camera->disconnect(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview));
	}
	if (p_state.has("previewing")) {
		Node *pv = EditorNode::get_singleton()->get_edited_scene()->get_node(p_state["previewing"]);
		if (Object::cast_to<Camera3D>(pv)) {
			previewing = Object::cast_to<Camera3D>(pv);
			previewing->connect(SceneStringName(tree_exiting), callable_mp(this, &Node3DEditorViewport::_preview_exited_scene));
			RS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), previewing->get_camera()); //replace
			surface->queue_redraw();
			preview_camera->set_pressed(true);
			preview_camera->show();
		}
	}
	preview_camera->connect(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview));
}

Dictionary Node3DEditorViewport::get_state() const {
	Dictionary d;
	d["position"] = cursor.pos;
	d["x_rotation"] = cursor.x_rot;
	d["y_rotation"] = cursor.y_rot;
	d["distance"] = cursor.distance;
	d["use_environment"] = camera->get_environment().is_valid();
	d["orthogonal"] = camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;
	d["view_type"] = view_type;
	d["auto_orthogonal"] = auto_orthogonal;
	d["auto_orthogonal_enabled"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL));

	// Find selected display mode.
	int display_mode = VIEW_DISPLAY_NORMAL;
	for (int i = VIEW_DISPLAY_NORMAL; i < VIEW_DISPLAY_ADVANCED; i++) {
		if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(i))) {
			display_mode = i;
			break;
		}
	}
	for (int i = VIEW_DISPLAY_ADVANCED + 1; i < VIEW_DISPLAY_MAX; i++) {
		if (display_submenu->is_item_checked(display_submenu->get_item_index(i))) {
			display_mode = i;
			break;
		}
	}
	d["display_mode"] = display_mode;

	d["listener"] = viewport->is_audio_listener_3d();
	d["doppler"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER));
	d["gizmos"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_GIZMOS));
	d["transform_gizmo"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_TRANSFORM_GIZMO));
	d["grid"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_GRID));
	d["information"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_INFORMATION));
	d["frame_time"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_FRAME_TIME));
	d["half_res"] = subviewport_container->get_stretch_shrink() > 1;
	d["cinematic_preview"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW));
	if (previewing) {
		d["previewing"] = EditorNode::get_singleton()->get_edited_scene()->get_path_to(previewing);
	}
	d["lock_rotation"] = lock_rotation;

	return d;
}

void Node3DEditorViewport::_bind_methods() {
	ADD_SIGNAL(MethodInfo("toggle_maximize_view", PropertyInfo(Variant::OBJECT, "viewport")));
	ADD_SIGNAL(MethodInfo("clicked"));
}

void Node3DEditorViewport::reset() {
	orthogonal = false;
	auto_orthogonal = false;
	lock_rotation = false;
	message_time = 0;
	message = "";
	last_message = "";
	view_type = VIEW_TYPE_USER;

	cursor = Cursor();
	_update_name();
}

void Node3DEditorViewport::focus_selection() {
	Vector3 center;
	int count = 0;

	const List<Node *> &selection = editor_selection->get_selected_node_list();

	for (Node *E : selection) {
		Node3D *sp = Object::cast_to<Node3D>(E);
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		if (se->gizmo.is_valid()) {
			for (const KeyValue<int, Transform3D> &GE : se->subgizmos) {
				center += se->gizmo->get_subgizmo_transform(GE.key).origin;
				count++;
			}
		}

		center += sp->get_global_gizmo_transform().origin;
		count++;
	}

	if (count != 0) {
		center /= count;
	}

	cursor.pos = center;
}

void Node3DEditorViewport::assign_pending_data_pointers(Node3D *p_preview_node, AABB *p_preview_bounds, AcceptDialog *p_accept) {
	preview_node = p_preview_node;
	preview_bounds = p_preview_bounds;
	accept = p_accept;
}

void _insert_rid_recursive(Node *node, HashSet<RID> &rids) {
	CollisionObject3D *co = Object::cast_to<CollisionObject3D>(node);
	if (co) {
		rids.insert(co->get_rid());
	}

	for (int i = 0; i < node->get_child_count(); i++) {
		Node *child = node->get_child(i);
		_insert_rid_recursive(child, rids);
	}
}

Vector3 Node3DEditorViewport::_get_instance_position(const Point2 &p_pos, Node3D *p_node) const {
	const float MAX_DISTANCE = 50.0;
	const float FALLBACK_DISTANCE = 5.0;

	Vector3 world_ray = get_ray(p_pos);
	Vector3 world_pos = get_ray_pos(p_pos);

	PhysicsDirectSpaceState3D *ss = get_tree()->get_root()->get_world_3d()->get_direct_space_state();

	HashSet<RID> rids;

	if (!preview_node->is_inside_tree() && !ruler->is_inside_tree()) {
		List<Node *> &selection = editor_selection->get_selected_node_list();

		Node3D *first_selected_node = Object::cast_to<Node3D>(selection.front()->get());

		Array children = first_selected_node->get_children();

		if (first_selected_node) {
			_insert_rid_recursive(first_selected_node, rids);
		}
	}

	PhysicsDirectSpaceState3D::RayParameters ray_params;
	ray_params.exclude = rids;
	ray_params.from = world_pos;
	ray_params.to = world_pos + world_ray * camera->get_far();

	PhysicsDirectSpaceState3D::RayResult result;
	if (ss->intersect_ray(ray_params, result) && (preview_node->get_child_count() > 0 || !preview_node->is_inside_tree())) {
		// Calculate an offset for the `p_node` such that the its bounding box is on top of and touching the contact surface's plane.

		// Use the Gram-Schmidt process to get an orthonormal Basis aligned with the surface normal.
		const Vector3 bb_basis_x = result.normal;
		Vector3 bb_basis_y = Vector3(0, 1, 0);
		bb_basis_y = bb_basis_y - bb_basis_y.project(bb_basis_x);
		if (bb_basis_y.is_zero_approx()) {
			bb_basis_y = Vector3(0, 0, 1);
			bb_basis_y = bb_basis_y - bb_basis_y.project(bb_basis_x);
		}
		bb_basis_y = bb_basis_y.normalized();
		const Vector3 bb_basis_z = bb_basis_x.cross(bb_basis_y);
		const Basis bb_basis = Basis(bb_basis_x, bb_basis_y, bb_basis_z);

		// This normal-aligned Basis allows us to create an AABB that can fit on the surface plane as snugly as possible.
		const Transform3D bb_transform = Transform3D(bb_basis, p_node->get_transform().origin);
		const AABB p_node_bb = _calculate_spatial_bounds(p_node, true, &bb_transform);
		// The x-axis's alignment with the surface normal also makes it trivial to get the distance from `p_node`'s origin at (0, 0, 0) to the correct AABB face.
		const float offset_distance = -p_node_bb.position.x;

		// `result_offset` is in global space.
		const Vector3 result_offset = result.position + result.normal * offset_distance;

		return result_offset;
	}

	const bool is_orthogonal = camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;

	// The XZ plane.
	Vector3 intersection;
	Plane plane(Vector3(0, 1, 0));
	if (plane.intersects_ray(world_pos, world_ray, &intersection)) {
		if (is_orthogonal || world_pos.distance_to(intersection) <= MAX_DISTANCE) {
			return intersection;
		}
	}

	// Plane facing the camera using fallback distance.
	if (is_orthogonal) {
		plane = Plane(world_ray, cursor.pos - world_ray * (cursor.distance - FALLBACK_DISTANCE));
	} else {
		plane = Plane(world_ray, world_pos + world_ray * FALLBACK_DISTANCE);
	}
	if (plane.intersects_ray(world_pos, world_ray, &intersection)) {
		return intersection;
	}

	// Not likely, but just in case...
	return world_pos + world_ray * FALLBACK_DISTANCE;
}

AABB Node3DEditorViewport::_calculate_spatial_bounds(const Node3D *p_parent, bool p_omit_top_level, const Transform3D *p_bounds_orientation) {
	AABB bounds;

	Transform3D bounds_orientation;
	if (p_bounds_orientation) {
		bounds_orientation = *p_bounds_orientation;
	} else {
		bounds_orientation = p_parent->get_global_transform();
	}

	if (!p_parent) {
		return AABB(Vector3(-0.2, -0.2, -0.2), Vector3(0.4, 0.4, 0.4));
	}

	const Transform3D xform_to_top_level_parent_space = bounds_orientation.affine_inverse() * p_parent->get_global_transform();

	const VisualInstance3D *visual_instance = Object::cast_to<VisualInstance3D>(p_parent);
	if (visual_instance) {
		bounds = visual_instance->get_aabb();
	} else {
		bounds = AABB();
	}
	bounds = xform_to_top_level_parent_space.xform(bounds);

	for (int i = 0; i < p_parent->get_child_count(); i++) {
		const Node3D *child = Object::cast_to<Node3D>(p_parent->get_child(i));
		if (child && !(p_omit_top_level && child->is_set_as_top_level())) {
			const AABB child_bounds = _calculate_spatial_bounds(child, p_omit_top_level, &bounds_orientation);
			bounds.merge_with(child_bounds);
		}
	}

	return bounds;
}

Node *Node3DEditorViewport::_sanitize_preview_node(Node *p_node) const {
	Node3D *node_3d = Object::cast_to<Node3D>(p_node);
	if (node_3d == nullptr) {
		Node3D *replacement_node = memnew(Node3D);
		replacement_node->set_name(p_node->get_name());
		p_node->replace_by(replacement_node);
		memdelete(p_node);
		p_node = replacement_node;
	} else {
		VisualInstance3D *visual_instance = Object::cast_to<VisualInstance3D>(node_3d);
		if (visual_instance == nullptr) {
			Node3D *replacement_node = memnew(Node3D);
			replacement_node->set_name(node_3d->get_name());
			replacement_node->set_visible(node_3d->is_visible());
			replacement_node->set_transform(node_3d->get_transform());
			replacement_node->set_rotation_edit_mode(node_3d->get_rotation_edit_mode());
			replacement_node->set_rotation_order(node_3d->get_rotation_order());
			replacement_node->set_as_top_level(node_3d->is_set_as_top_level());
			p_node->replace_by(replacement_node);
			memdelete(p_node);
			p_node = replacement_node;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_sanitize_preview_node(p_node->get_child(i));
	}

	return p_node;
}

void Node3DEditorViewport::_create_preview_node(const Vector<String> &files) const {
	bool add_preview = false;
	for (const String &path : files) {
		Ref<Resource> res = ResourceLoader::load(path);
		ERR_CONTINUE(res.is_null());

		Ref<PackedScene> scene = res;
		if (scene.is_valid()) {
			Node *instance = scene->instantiate();
			if (instance) {
				instance = _sanitize_preview_node(instance);
				preview_node->add_child(instance);
				Node3D *node_3d = Object::cast_to<Node3D>(instance);
				if (node_3d) {
					node_3d->set_as_top_level(false);
				}
			}
			add_preview = true;
		}

		Ref<Mesh> mesh = res;
		if (mesh.is_valid()) {
			MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_mesh(mesh);
			preview_node->add_child(mesh_instance);
			add_preview = true;
		}

		Ref<AudioStream> audio = res;
		if (audio.is_valid()) {
			Sprite3D *sprite = memnew(Sprite3D);
			sprite->set_texture(get_editor_theme_icon(SNAME("Gizmo3DSamplePlayer")));
			sprite->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
			sprite->set_pixel_size(0.005);
			preview_node->add_child(sprite);
			add_preview = true;
		}
	}
	if (add_preview) {
		EditorNode::get_singleton()->get_scene_root()->add_child(preview_node);
	}

	*preview_bounds = _calculate_spatial_bounds(preview_node);
}

void Node3DEditorViewport::_remove_preview_node() {
	set_message("");
	if (preview_node->get_parent()) {
		for (int i = preview_node->get_child_count() - 1; i >= 0; i--) {
			Node *node = preview_node->get_child(i);
			node->queue_free();
			preview_node->remove_child(node);
		}
		EditorNode::get_singleton()->get_scene_root()->remove_child(preview_node);
	}
}

bool Node3DEditorViewport::_apply_preview_material(ObjectID p_target, const Point2 &p_point) const {
	_reset_preview_material();

	if (p_target.is_null()) {
		return false;
	}

	spatial_editor->set_preview_material_target(p_target);

	Object *target_inst = ObjectDB::get_instance(p_target);

	bool is_ctrl = Input::get_singleton()->is_key_pressed(Key::CTRL);

	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(target_inst);
	if (is_ctrl && mesh_instance) {
		Ref<Mesh> mesh = mesh_instance->get_mesh();
		int surface_count = mesh->get_surface_count();

		Vector3 world_ray = get_ray(p_point);
		Vector3 world_pos = get_ray_pos(p_point);

		int closest_surface = -1;
		float closest_dist = 1e20;

		Transform3D gt = mesh_instance->get_global_transform();

		Transform3D ai = gt.affine_inverse();
		Vector3 xform_ray = ai.basis.xform(world_ray).normalized();
		Vector3 xform_pos = ai.xform(world_pos);

		for (int surface_idx = 0; surface_idx < surface_count; surface_idx++) {
			Ref<TriangleMesh> surface_mesh = mesh->generate_surface_triangle_mesh(surface_idx);

			Vector3 rpos, rnorm;
			if (surface_mesh->intersect_ray(xform_pos, xform_ray, rpos, rnorm)) {
				Vector3 hitpos = gt.xform(rpos);

				const real_t dist = world_pos.distance_to(hitpos);

				if (dist < 0) {
					continue;
				}

				if (dist < closest_dist) {
					closest_surface = surface_idx;
					closest_dist = dist;
				}
			}
		}

		if (closest_surface == -1) {
			return false;
		}

		spatial_editor->set_preview_material_surface(closest_surface);
		spatial_editor->set_preview_reset_material(mesh_instance->get_surface_override_material(closest_surface));
		mesh_instance->set_surface_override_material(closest_surface, spatial_editor->get_preview_material());

		return true;
	}

	GeometryInstance3D *geometry_instance = Object::cast_to<GeometryInstance3D>(target_inst);
	if (geometry_instance) {
		spatial_editor->set_preview_material_surface(-1);
		spatial_editor->set_preview_reset_material(geometry_instance->get_material_override());
		geometry_instance->set_material_override(spatial_editor->get_preview_material());
		return true;
	}

	return false;
}

void Node3DEditorViewport::_reset_preview_material() const {
	ObjectID last_target = spatial_editor->get_preview_material_target();
	if (last_target.is_null()) {
		return;
	}
	Object *last_target_inst = ObjectDB::get_instance(last_target);

	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(last_target_inst);
	GeometryInstance3D *geometry_instance = Object::cast_to<GeometryInstance3D>(last_target_inst);
	if (mesh_instance && spatial_editor->get_preview_material_surface() != -1) {
		mesh_instance->set_surface_override_material(spatial_editor->get_preview_material_surface(), spatial_editor->get_preview_reset_material());
	} else if (geometry_instance) {
		geometry_instance->set_material_override(spatial_editor->get_preview_reset_material());
	}
}

void Node3DEditorViewport::_remove_preview_material() {
	preview_material_label->hide();
	preview_material_label_desc->hide();

	spatial_editor->set_preview_material(Ref<Material>());
	spatial_editor->set_preview_reset_material(Ref<Material>());
	spatial_editor->set_preview_material_target(ObjectID());
	spatial_editor->set_preview_material_surface(-1);
}

bool Node3DEditorViewport::_cyclical_dependency_exists(const String &p_target_scene_path, Node *p_desired_node) const {
	if (p_desired_node->get_scene_file_path() == p_target_scene_path) {
		return true;
	}

	int childCount = p_desired_node->get_child_count();
	for (int i = 0; i < childCount; i++) {
		Node *child = p_desired_node->get_child(i);
		if (_cyclical_dependency_exists(p_target_scene_path, child)) {
			return true;
		}
	}
	return false;
}

bool Node3DEditorViewport::_create_instance(Node *p_parent, const String &p_path, const Point2 &p_point) {
	Ref<Resource> res = ResourceLoader::load(p_path);
	ERR_FAIL_COND_V(res.is_null(), false);

	Ref<PackedScene> scene = res;
	Ref<Mesh> mesh = res;

	Node *instantiated_scene = nullptr;

	if (mesh.is_valid() || scene.is_valid()) {
		if (mesh.is_valid()) {
			MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
			mesh_instance->set_mesh(mesh);

			// Adjust casing according to project setting. The file name is expected to be in snake_case, but will work for others.
			const String &node_name = Node::adjust_name_casing(p_path.get_file().get_basename());
			if (!node_name.is_empty()) {
				mesh_instance->set_name(node_name);
			}

			instantiated_scene = mesh_instance;
		} else {
			if (scene.is_null()) { // invalid scene
				return false;
			} else {
				instantiated_scene = scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
			}
		}
	}

	if (instantiated_scene == nullptr) {
		return false;
	}

	if (!EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path().is_empty()) { // Cyclic instantiation.
		if (_cyclical_dependency_exists(EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path(), instantiated_scene)) {
			memdelete(instantiated_scene);
			return false;
		}
	}

	if (scene.is_valid()) {
		instantiated_scene->set_scene_file_path(ProjectSettings::get_singleton()->localize_path(p_path));
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(p_parent, "add_child", instantiated_scene, true);
	undo_redo->add_do_method(instantiated_scene, "set_owner", EditorNode::get_singleton()->get_edited_scene());
	undo_redo->add_do_reference(instantiated_scene);
	undo_redo->add_undo_method(p_parent, "remove_child", instantiated_scene);
	undo_redo->add_do_method(editor_selection, "add_node", instantiated_scene);

	String new_name = p_parent->validate_child_name(instantiated_scene);
	EditorDebuggerNode *ed = EditorDebuggerNode::get_singleton();
	undo_redo->add_do_method(ed, "live_debug_instantiate_node", EditorNode::get_singleton()->get_edited_scene()->get_path_to(p_parent), p_path, new_name);
	undo_redo->add_undo_method(ed, "live_debug_remove_node", NodePath(String(EditorNode::get_singleton()->get_edited_scene()->get_path_to(p_parent)) + "/" + new_name));

	Node3D *node3d = Object::cast_to<Node3D>(instantiated_scene);
	if (node3d) {
		Transform3D parent_tf;
		Node3D *parent_node3d = Object::cast_to<Node3D>(p_parent);
		if (parent_node3d) {
			parent_tf = parent_node3d->get_global_gizmo_transform();
		}

		Transform3D new_tf = node3d->get_transform();
		if (node3d->is_set_as_top_level()) {
			new_tf.origin += preview_node_pos;
		} else {
			new_tf.origin = parent_tf.affine_inverse().xform(preview_node_pos + node3d->get_position());
			new_tf.basis = parent_tf.affine_inverse().basis * new_tf.basis;
		}

		undo_redo->add_do_method(instantiated_scene, "set_transform", new_tf);
	}

	return true;
}

bool Node3DEditorViewport::_create_audio_node(Node *p_parent, const String &p_path, const Point2 &p_point) {
	Ref<AudioStream> audio = ResourceLoader::load(p_path);
	ERR_FAIL_COND_V(audio.is_null(), false);

	AudioStreamPlayer3D *audio_player = memnew(AudioStreamPlayer3D);
	audio_player->set_stream(audio);

	// Adjust casing according to project setting. The file name is expected to be in snake_case, but will work for others.
	const String &node_name = Node::adjust_name_casing(p_path.get_file().get_basename());
	if (!node_name.is_empty()) {
		audio_player->set_name(node_name);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(p_parent, "add_child", audio_player, true);
	undo_redo->add_do_method(audio_player, "set_owner", EditorNode::get_singleton()->get_edited_scene());
	undo_redo->add_do_reference(audio_player);
	undo_redo->add_undo_method(p_parent, "remove_child", audio_player);
	undo_redo->add_do_method(editor_selection, "add_node", audio_player);

	const String new_name = p_parent->validate_child_name(audio_player);
	EditorDebuggerNode *ed = EditorDebuggerNode::get_singleton();
	undo_redo->add_do_method(ed, "live_debug_create_node", EditorNode::get_singleton()->get_edited_scene()->get_path_to(p_parent), audio_player->get_class(), new_name);
	undo_redo->add_undo_method(ed, "live_debug_remove_node", NodePath(String(EditorNode::get_singleton()->get_edited_scene()->get_path_to(p_parent)) + "/" + new_name));

	Transform3D parent_tf;
	Node3D *parent_node3d = Object::cast_to<Node3D>(p_parent);
	if (parent_node3d) {
		parent_tf = parent_node3d->get_global_gizmo_transform();
	}

	Transform3D new_tf = audio_player->get_transform();
	new_tf.origin = parent_tf.affine_inverse().xform(preview_node_pos + audio_player->get_position());
	new_tf.basis = parent_tf.affine_inverse().basis * new_tf.basis;

	undo_redo->add_do_method(audio_player, "set_transform", new_tf);

	return true;
}

void Node3DEditorViewport::_perform_drop_data() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (spatial_editor->get_preview_material_target().is_valid()) {
		GeometryInstance3D *geometry_instance = Object::cast_to<GeometryInstance3D>(ObjectDB::get_instance(spatial_editor->get_preview_material_target()));
		MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(ObjectDB::get_instance(spatial_editor->get_preview_material_target()));
		if (mesh_instance && spatial_editor->get_preview_material_surface() != -1) {
			undo_redo->create_action(vformat(TTR("Set Surface %d Override Material"), spatial_editor->get_preview_material_surface()));
			undo_redo->add_do_method(geometry_instance, "set_surface_override_material", spatial_editor->get_preview_material_surface(), spatial_editor->get_preview_material());
			undo_redo->add_undo_method(geometry_instance, "set_surface_override_material", spatial_editor->get_preview_material_surface(), spatial_editor->get_preview_reset_material());
			undo_redo->commit_action();
		} else if (geometry_instance) {
			undo_redo->create_action(TTR("Set Material Override"));
			undo_redo->add_do_method(geometry_instance, "set_material_override", spatial_editor->get_preview_material());
			undo_redo->add_undo_method(geometry_instance, "set_material_override", spatial_editor->get_preview_reset_material());
			undo_redo->commit_action();
		}

		_remove_preview_material();
		return;
	}

	_remove_preview_node();

	PackedStringArray error_files;

	undo_redo->create_action(TTR("Create Node"), UndoRedo::MERGE_DISABLE, target_node);
	undo_redo->add_do_method(editor_selection, "clear");

	for (int i = 0; i < selected_files.size(); i++) {
		String path = selected_files[i];
		Ref<Resource> res = ResourceLoader::load(path);
		if (res.is_null()) {
			continue;
		}

		Ref<PackedScene> scene = res;
		Ref<Mesh> mesh = res;
		if (mesh.is_valid() || scene.is_valid()) {
			if (!_create_instance(target_node, path, drop_pos)) {
				error_files.push_back(path.get_file());
			}
		}

		Ref<AudioStream> audio = res;
		if (audio.is_valid()) {
			if (!_create_audio_node(target_node, path, drop_pos)) {
				error_files.push_back(path.get_file());
			}
		}
	}

	undo_redo->commit_action();

	if (error_files.size() > 0) {
		accept->set_text(vformat(TTR("Error instantiating scene from %s."), String(", ").join(error_files)));
		accept->popup_centered();
	}
}

bool Node3DEditorViewport::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	preview_node_viewport_pos = p_point;

	bool can_instantiate = false;
	bool is_cyclical_dep = false;
	String error_file;

	if (!preview_node->is_inside_tree() && spatial_editor->get_preview_material().is_null()) {
		Dictionary d = p_data;
		if (d.has("type") && (String(d["type"]) == "files")) {
			Vector<String> files = d["files"];

			// Track whether a type other than PackedScene is valid to stop checking them and only
			// continue to check if the rest of the scenes are valid (don't have cyclic dependencies).
			bool is_other_valid = false;
			// Check if at least one of the dragged files is a mesh, material, texture or scene.
			for (int i = 0; i < files.size(); i++) {
				const String &res_type = ResourceLoader::get_resource_type(files[i]);
				bool is_scene = ClassDB::is_parent_class(res_type, "PackedScene");
				bool is_mesh = ClassDB::is_parent_class(res_type, "Mesh");
				bool is_material = ClassDB::is_parent_class(res_type, "Material");
				bool is_texture = ClassDB::is_parent_class(res_type, "Texture");
				bool is_audio = ClassDB::is_parent_class(res_type, "AudioStream");

				if (is_mesh || is_scene || is_material || is_texture || is_audio) {
					Ref<Resource> res = ResourceLoader::load(files[i]);
					if (res.is_null()) {
						continue;
					}
					Ref<PackedScene> scn = res;
					Ref<Mesh> mesh = res;
					Ref<Material> mat = res;
					Ref<Texture2D> tex = res;
					Ref<AudioStream> audio = res;
					if (scn.is_valid()) {
						Node *instantiated_scene = scn->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
						if (!instantiated_scene) {
							continue;
						}
						Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
						if (edited_scene && !edited_scene->get_scene_file_path().is_empty() && _cyclical_dependency_exists(edited_scene->get_scene_file_path(), instantiated_scene)) {
							memdelete(instantiated_scene);
							can_instantiate = false;
							is_cyclical_dep = true;
							error_file = files[i].get_file();
							break;
						}
						memdelete(instantiated_scene);
					} else if (!is_other_valid && mat.is_valid()) {
						Ref<BaseMaterial3D> base_mat = res;
						Ref<ShaderMaterial> shader_mat = res;

						if (base_mat.is_null() && shader_mat.is_null()) {
							continue;
						}

						spatial_editor->set_preview_material(mat);
						is_other_valid = true;
						continue;
					} else if (!is_other_valid && mesh.is_valid()) {
						// Let the mesh pass.
						is_other_valid = true;
					} else if (!is_other_valid && tex.is_valid()) {
						Ref<StandardMaterial3D> new_mat = memnew(StandardMaterial3D);
						new_mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);

						spatial_editor->set_preview_material(new_mat);
						is_other_valid = true;
						continue;
					} else if (!is_other_valid && audio.is_valid()) {
						is_other_valid = true;
					} else {
						continue;
					}
					can_instantiate = true;
				}
			}
			if (can_instantiate) {
				_create_preview_node(files);
				preview_node->hide();
			}
		}
	} else {
		if (preview_node->is_inside_tree()) {
			can_instantiate = true;
		}
	}

	if (is_cyclical_dep) {
		set_message(vformat(TTR("Can't instantiate: %s."), vformat(TTR("Circular dependency found at %s"), error_file)));
		return false;
	}

	if (can_instantiate) {
		update_preview_node = true;
		return true;
	}

	if (spatial_editor->get_preview_material().is_valid()) {
		preview_material_label->show();
		preview_material_label_desc->show();

		ObjectID new_preview_material_target = _select_ray(p_point);
		return _apply_preview_material(new_preview_material_target, p_point);
	}

	return false;
}

void Node3DEditorViewport::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	bool is_shift = Input::get_singleton()->is_key_pressed(Key::SHIFT);
	bool is_alt = Input::get_singleton()->is_key_pressed(Key::ALT);

	selected_files.clear();
	Dictionary d = p_data;
	if (d.has("type") && String(d["type"]) == "files") {
		selected_files = d["files"];
	}

	List<Node *> selected_nodes = EditorNode::get_singleton()->get_editor_selection()->get_selected_node_list();
	Node *root_node = EditorNode::get_singleton()->get_edited_scene();
	if (selected_nodes.size() > 0) {
		Node *selected_node = selected_nodes.front()->get();
		if (is_alt) {
			target_node = root_node;
		} else if (is_shift) {
			target_node = selected_node;
		} else { // Default behavior.
			target_node = (selected_node != root_node) ? selected_node->get_parent() : root_node;
		}
	} else {
		if (root_node) {
			target_node = root_node;
		} else {
			// Create a root node so we can add child nodes to it.
			SceneTreeDock::get_singleton()->add_root_node(memnew(Node3D));
			target_node = get_tree()->get_edited_scene_root();
		}
	}

	drop_pos = p_point;

	_perform_drop_data();
}

void Node3DEditorViewport::begin_transform(TransformMode p_mode, bool instant) {
	if (get_selected_count() > 0) {
		_edit.mode = p_mode;
		_compute_edit(_edit.mouse_pos);
		_edit.instant = instant;
		_edit.snap = spatial_editor->is_snap_enabled();
		update_transform_gizmo_view();
		set_process_input(instant);
	}
}

// Apply the current transform operation.
void Node3DEditorViewport::commit_transform() {
	ERR_FAIL_COND(_edit.mode == TRANSFORM_NONE);
	static const char *_transform_name[4] = {
		TTRC("None"),
		TTRC("Rotate"),
		// TRANSLATORS: This refers to the movement that changes the position of an object.
		TTRC("Translate"),
		TTRC("Scale"),
	};
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(_transform_name[_edit.mode]);

	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Node3D *sp = Object::cast_to<Node3D>(E->get());
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		undo_redo->add_do_method(sp, "set_transform", sp->get_local_gizmo_transform());
		undo_redo->add_undo_method(sp, "set_transform", se->original_local);
	}
	undo_redo->commit_action();

	collision_reposition = false;
	finish_transform();
	set_message("");
}

void Node3DEditorViewport::apply_transform(Vector3 p_motion, double p_snap) {
	bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW);
	List<Node *> &selection = editor_selection->get_selected_node_list();
	for (Node *E : selection) {
		Node3D *sp = Object::cast_to<Node3D>(E);
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		if (sp->has_meta("_edit_lock_")) {
			continue;
		}

		if (se->gizmo.is_valid()) {
			for (KeyValue<int, Transform3D> &GE : se->subgizmos) {
				Transform3D xform = GE.value;
				Transform3D new_xform = _compute_transform(_edit.mode, se->original * xform, xform, p_motion, p_snap, local_coords, _edit.plane != TRANSFORM_VIEW); // Force orthogonal with subgizmo.
				if (!local_coords) {
					new_xform = se->original.affine_inverse() * new_xform;
				}
				se->gizmo->set_subgizmo_transform(GE.key, new_xform);
			}
		} else {
			Transform3D new_xform = _compute_transform(_edit.mode, se->original, se->original_local, p_motion, p_snap, local_coords, sp->get_rotation_edit_mode() != Node3D::ROTATION_EDIT_MODE_BASIS && _edit.plane != TRANSFORM_VIEW);
			_transform_gizmo_apply(se->sp, new_xform, local_coords);
		}
	}

	spatial_editor->update_transform_gizmo();
	surface->queue_redraw();
}

// Update the current transform operation in response to an input.
void Node3DEditorViewport::update_transform(bool p_shift) {
	Vector3 ray_pos = get_ray_pos(_edit.mouse_pos);
	Vector3 ray = get_ray(_edit.mouse_pos);
	double snap = EDITOR_GET("interface/inspector/default_float_step");
	int snap_step_decimals = Math::range_step_decimals(snap);

	switch (_edit.mode) {
		case TRANSFORM_SCALE: {
			Vector3 motion_mask;
			Plane plane;
			bool plane_mv = false;

			switch (_edit.plane) {
				case TRANSFORM_VIEW:
					motion_mask = Vector3(0, 0, 0);
					plane = Plane(_get_camera_normal(), _edit.center);
					break;
				case TRANSFORM_X_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(0).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_Y_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(1).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_Z_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(2).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_YZ:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(2).normalized() + spatial_editor->get_gizmo_transform().basis.get_column(1).normalized();
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(0).normalized(), _edit.center);
					plane_mv = true;
					break;
				case TRANSFORM_XZ:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(2).normalized() + spatial_editor->get_gizmo_transform().basis.get_column(0).normalized();
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(1).normalized(), _edit.center);
					plane_mv = true;
					break;
				case TRANSFORM_XY:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(0).normalized() + spatial_editor->get_gizmo_transform().basis.get_column(1).normalized();
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(2).normalized(), _edit.center);
					plane_mv = true;
					break;
			}

			Vector3 intersection;
			if (!plane.intersects_ray(ray_pos, ray, &intersection)) {
				break;
			}

			Vector3 click;
			if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click)) {
				break;
			}

			Vector3 motion = intersection - click;
			if (_edit.plane != TRANSFORM_VIEW) {
				if (!plane_mv) {
					motion = motion_mask.dot(motion) * motion_mask;

				} else {
					// Alternative planar scaling mode
					if (p_shift) {
						motion = motion_mask.dot(motion) * motion_mask;
					}
				}

			} else {
				const real_t center_click_dist = click.distance_to(_edit.center);
				const real_t center_inters_dist = intersection.distance_to(_edit.center);
				if (center_click_dist == 0) {
					break;
				}

				const real_t scale = center_inters_dist - center_click_dist;
				motion = Vector3(scale, scale, scale);
			}

			motion /= click.distance_to(_edit.center);

			// Disable local transformation for TRANSFORM_VIEW
			bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW);

			if (_edit.snap || spatial_editor->is_snap_enabled()) {
				snap = spatial_editor->get_scale_snap() / 100;
			}
			Vector3 motion_snapped = motion;
			motion_snapped.snapf(snap);
			// This might not be necessary anymore after issue #288 is solved (in 4.0?).
			// TRANSLATORS: Refers to changing the scale of a node in the 3D editor.
			set_message(TTR("Scaling:") + " (" + String::num(motion_snapped.x, snap_step_decimals) + ", " +
					String::num(motion_snapped.y, snap_step_decimals) + ", " + String::num(motion_snapped.z, snap_step_decimals) + ")");
			if (local_coords) {
				// TODO: needed?
				motion = _edit.original.basis.inverse().xform(motion);
			}

			apply_transform(motion, snap);
		} break;

		case TRANSFORM_TRANSLATE: {
			Vector3 motion_mask;
			Plane plane;
			bool plane_mv = false;

			switch (_edit.plane) {
				case TRANSFORM_VIEW:
					plane = Plane(_get_camera_normal(), _edit.center);
					break;
				case TRANSFORM_X_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(0).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_Y_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(1).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_Z_AXIS:
					motion_mask = spatial_editor->get_gizmo_transform().basis.get_column(2).normalized();
					plane = Plane(motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized(), _edit.center);
					break;
				case TRANSFORM_YZ:
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(0).normalized(), _edit.center);
					plane_mv = true;
					break;
				case TRANSFORM_XZ:
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(1).normalized(), _edit.center);
					plane_mv = true;
					break;
				case TRANSFORM_XY:
					plane = Plane(spatial_editor->get_gizmo_transform().basis.get_column(2).normalized(), _edit.center);
					plane_mv = true;
					break;
			}

			Vector3 intersection;
			if (!plane.intersects_ray(ray_pos, ray, &intersection)) {
				break;
			}

			Vector3 click;
			if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click)) {
				break;
			}

			Vector3 motion = intersection - click;
			if (_edit.plane != TRANSFORM_VIEW) {
				if (!plane_mv) {
					motion = motion_mask.dot(motion) * motion_mask;
				}
			}

			// Disable local transformation for TRANSFORM_VIEW
			bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW);

			if (_edit.snap || spatial_editor->is_snap_enabled()) {
				snap = spatial_editor->get_translate_snap();
			}
			Vector3 motion_snapped = motion;
			motion_snapped.snapf(snap);
			// TRANSLATORS: Refers to changing the position of a node in the 3D editor.
			set_message(TTR("Translating:") + " (" + String::num(motion_snapped.x, snap_step_decimals) + ", " +
					String::num(motion_snapped.y, snap_step_decimals) + ", " + String::num(motion_snapped.z, snap_step_decimals) + ")");
			if (local_coords) {
				motion = spatial_editor->get_gizmo_transform().basis.inverse().xform(motion);
			}

			apply_transform(motion, snap);
		} break;

		case TRANSFORM_ROTATE: {
			Plane plane;
			if (camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE) {
				Vector3 cam_to_obj = _edit.center - _get_camera_position();
				if (!cam_to_obj.is_zero_approx()) {
					plane = Plane(cam_to_obj.normalized(), _edit.center);
				} else {
					plane = Plane(_get_camera_normal(), _edit.center);
				}
			} else {
				plane = Plane(_get_camera_normal(), _edit.center);
			}

			Vector3 local_axis;
			Vector3 global_axis;
			switch (_edit.plane) {
				case TRANSFORM_VIEW:
					// local_axis unused
					global_axis = plane.normal;
					break;
				case TRANSFORM_X_AXIS:
					local_axis = Vector3(1, 0, 0);
					break;
				case TRANSFORM_Y_AXIS:
					local_axis = Vector3(0, 1, 0);
					break;
				case TRANSFORM_Z_AXIS:
					local_axis = Vector3(0, 0, 1);
					break;
				case TRANSFORM_YZ:
				case TRANSFORM_XZ:
				case TRANSFORM_XY:
					break;
			}

			if (_edit.plane != TRANSFORM_VIEW) {
				global_axis = spatial_editor->get_gizmo_transform().basis.xform(local_axis).normalized();
			}

			Vector3 intersection;
			if (!plane.intersects_ray(ray_pos, ray, &intersection)) {
				break;
			}

			Vector3 click;
			if (!plane.intersects_ray(_edit.click_ray_pos, _edit.click_ray, &click)) {
				break;
			}

			static const float orthogonal_threshold = Math::cos(Math::deg_to_rad(85.0f));
			bool axis_is_orthogonal = ABS(plane.normal.dot(global_axis)) < orthogonal_threshold;

			double angle = 0.0f;
			if (axis_is_orthogonal) {
				_edit.show_rotation_line = false;
				Vector3 projection_axis = plane.normal.cross(global_axis);
				Vector3 delta = intersection - click;
				float projection = delta.dot(projection_axis);
				angle = (projection * (Math_PI / 2.0f)) / (gizmo_scale * GIZMO_CIRCLE_SIZE);
			} else {
				_edit.show_rotation_line = true;
				Vector3 click_axis = (click - _edit.center).normalized();
				Vector3 current_axis = (intersection - _edit.center).normalized();
				angle = click_axis.signed_angle_to(current_axis, global_axis);
			}

			if (_edit.snap || spatial_editor->is_snap_enabled()) {
				snap = spatial_editor->get_rotate_snap();
			}
			angle = Math::snapped(Math::rad_to_deg(angle), snap);
			set_message(vformat(TTR("Rotating %s degrees."), String::num(angle, snap_step_decimals)));
			angle = Math::deg_to_rad(angle);

			bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW); // Disable local transformation for TRANSFORM_VIEW

			Vector3 compute_axis = local_coords ? local_axis : global_axis;
			apply_transform(compute_axis, angle);
		} break;
		default: {
		}
	}
}

void Node3DEditorViewport::update_transform_numeric() {
	Vector3 motion;
	switch (_edit.plane) {
		case TRANSFORM_VIEW: {
			switch (_edit.mode) {
				case TRANSFORM_TRANSLATE:
					motion = Vector3(1, 0, 0);
					break;
				case TRANSFORM_ROTATE:
					motion = spatial_editor->get_gizmo_transform().basis.xform_inv(_get_camera_normal()).normalized();
					break;
				case TRANSFORM_SCALE:
					motion = Vector3(1, 1, 1);
					break;
				case TRANSFORM_NONE:
					ERR_FAIL_MSG("_edit.mode cannot be TRANSFORM_NONE in update_transform_numeric.");
			}
			break;
		}
		case TRANSFORM_X_AXIS:
			motion = Vector3(1, 0, 0);
			break;
		case TRANSFORM_Y_AXIS:
			motion = Vector3(0, 1, 0);
			break;
		case TRANSFORM_Z_AXIS:
			motion = Vector3(0, 0, 1);
			break;
		case TRANSFORM_XY:
			motion = Vector3(1, 1, 0);
			break;
		case TRANSFORM_XZ:
			motion = Vector3(1, 0, 1);
			break;
		case TRANSFORM_YZ:
			motion = Vector3(0, 1, 1);
			break;
	}

	double value = _edit.numeric_input * (_edit.numeric_negate ? -1 : 1);
	double extra = 0.0;
	switch (_edit.mode) {
		case TRANSFORM_TRANSLATE:
			motion *= value;
			set_message(vformat(TTR("Translating %s."), motion));
			break;
		case TRANSFORM_ROTATE:
			extra = Math::deg_to_rad(value);
			set_message(vformat(TTR("Rotating %f degrees."), value));
			break;
		case TRANSFORM_SCALE:
			// To halve the size of an object in Blender, you scale it by 0.5.
			// Doing the same in Godot is considered scaling it by -0.5.
			motion *= (value - 1.0);
			set_message(vformat(TTR("Scaling %s."), motion));
			break;
		case TRANSFORM_NONE:
			ERR_FAIL_MSG("_edit.mode cannot be TRANSFORM_NONE in update_transform_numeric.");
	}

	apply_transform(motion, extra);
}

// Perform cleanup after a transform operation is committed or canceled.
void Node3DEditorViewport::finish_transform() {
	_edit.mode = TRANSFORM_NONE;
	_edit.instant = false;
	_edit.numeric_input = 0;
	_edit.numeric_next_decimal = 0;
	_edit.numeric_negate = false;
	spatial_editor->set_local_coords_enabled(_edit.original_local);
	spatial_editor->update_transform_gizmo();
	surface->queue_redraw();
	set_process_input(false);
}

// Register a shortcut and also add it as an input action with the same events.
void Node3DEditorViewport::register_shortcut_action(const String &p_path, const String &p_name, Key p_keycode, bool p_physical) {
	Ref<Shortcut> sc = ED_SHORTCUT(p_path, p_name, p_keycode, p_physical);
	shortcut_changed_callback(sc, p_path);
	// Connect to the change event on the shortcut so the input binding can be updated.
	sc->connect_changed(callable_mp(this, &Node3DEditorViewport::shortcut_changed_callback).bind(sc, p_path));
}

// Update the action in the InputMap to the provided shortcut events.
void Node3DEditorViewport::shortcut_changed_callback(const Ref<Shortcut> p_shortcut, const String &p_shortcut_path) {
	InputMap *im = InputMap::get_singleton();
	if (im->has_action(p_shortcut_path)) {
		im->action_erase_events(p_shortcut_path);
	} else {
		im->add_action(p_shortcut_path);
	}

	for (int i = 0; i < p_shortcut->get_events().size(); i++) {
		im->action_add_event(p_shortcut_path, p_shortcut->get_events()[i]);
	}
}

void Node3DEditorViewport::_set_lock_view_rotation(bool p_lock_rotation) {
	lock_rotation = p_lock_rotation;
	int idx = view_menu->get_popup()->get_item_index(VIEW_LOCK_ROTATION);
	view_menu->get_popup()->set_item_checked(idx, p_lock_rotation);
	if (p_lock_rotation) {
		locked_label->show();
	} else {
		locked_label->hide();
	}
}

Node3DEditorViewport::Node3DEditorViewport(Node3DEditor *p_spatial_editor, int p_index) {
	cpu_time_history_index = 0;
	gpu_time_history_index = 0;

	_edit.mode = TRANSFORM_NONE;
	_edit.plane = TRANSFORM_VIEW;
	_edit.snap = true;
	_edit.show_rotation_line = true;
	_edit.instant = false;
	_edit.gizmo_handle = -1;
	_edit.gizmo_handle_secondary = false;

	index = p_index;
	editor_selection = EditorNode::get_singleton()->get_editor_selection();

	orthogonal = false;
	auto_orthogonal = false;
	lock_rotation = false;
	message_time = 0;
	zoom_indicator_delay = 0.0;

	spatial_editor = p_spatial_editor;
	SubViewportContainer *c = memnew(SubViewportContainer);
	subviewport_container = c;
	c->set_stretch(true);
	add_child(c);
	c->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	viewport = memnew(SubViewport);
	viewport->set_disable_input(true);

	c->add_child(viewport);
	surface = memnew(Control);
	SET_DRAG_FORWARDING_CD(surface, Node3DEditorViewport);
	add_child(surface);
	surface->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	surface->set_clip_contents(true);
	camera = memnew(Camera3D);
	camera->set_disable_gizmos(true);
	camera->set_cull_mask(((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + p_index)) | (1 << GIZMO_EDIT_LAYER) | (1 << GIZMO_GRID_LAYER) | (1 << MISC_TOOL_LAYER));
	viewport->add_child(camera);
	camera->make_current();
	surface->set_focus_mode(FOCUS_ALL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	surface->add_child(vbox);
	vbox->set_offset(SIDE_LEFT, 10 * EDSCALE);
	vbox->set_offset(SIDE_TOP, 10 * EDSCALE);

	view_menu = memnew(MenuButton);
	view_menu->set_flat(false);
	view_menu->set_h_size_flags(0);
	view_menu->set_shortcut_context(this);
	vbox->add_child(view_menu);

	view_menu->get_popup()->set_hide_on_checkable_item_selection(false);

	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/top_view"), VIEW_TOP);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/bottom_view"), VIEW_BOTTOM);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/left_view"), VIEW_LEFT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/right_view"), VIEW_RIGHT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/front_view"), VIEW_FRONT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/rear_view"), VIEW_REAR);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal"), VIEW_SWITCH_PERSPECTIVE_ORTHOGONAL);
	view_menu->get_popup()->add_radio_check_item(TTR("Perspective"), VIEW_PERSPECTIVE);
	view_menu->get_popup()->add_radio_check_item(TTR("Orthogonal"), VIEW_ORTHOGONAL);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
	view_menu->get_popup()->add_check_item(TTR("Auto Orthogonal Enabled"), VIEW_AUTO_ORTHOGONAL);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_lock_rotation", TTRC("Lock View Rotation")), VIEW_LOCK_ROTATION);
	view_menu->get_popup()->add_separator();
	// TRANSLATORS: "Normal" as in "normal life", not "normal vector".
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_normal", TTRC("Display Normal")), VIEW_DISPLAY_NORMAL);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_wireframe", TTRC("Display Wireframe")), VIEW_DISPLAY_WIREFRAME);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_overdraw", TTRC("Display Overdraw")), VIEW_DISPLAY_OVERDRAW);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_lighting", TTRC("Display Lighting")), VIEW_DISPLAY_LIGHTING);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_unshaded", TTRC("Display Unshaded")), VIEW_DISPLAY_UNSHADED);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), true);

	display_submenu = memnew(PopupMenu);
	display_submenu->set_hide_on_checkable_item_selection(false);
	display_submenu->add_radio_check_item(TTR("Directional Shadow Splits"), VIEW_DISPLAY_DEBUG_PSSM_SPLITS);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("Normal Buffer"), VIEW_DISPLAY_NORMAL_BUFFER);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("Shadow Atlas"), VIEW_DISPLAY_DEBUG_SHADOW_ATLAS);
	display_submenu->add_radio_check_item(TTR("Directional Shadow Map"), VIEW_DISPLAY_DEBUG_DIRECTIONAL_SHADOW_ATLAS);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("Decal Atlas"), VIEW_DISPLAY_DEBUG_DECAL_ATLAS);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("VoxelGI Lighting"), VIEW_DISPLAY_DEBUG_VOXEL_GI_LIGHTING);
	display_submenu->add_radio_check_item(TTR("VoxelGI Albedo"), VIEW_DISPLAY_DEBUG_VOXEL_GI_ALBEDO);
	display_submenu->add_radio_check_item(TTR("VoxelGI Emission"), VIEW_DISPLAY_DEBUG_VOXEL_GI_EMISSION);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("SDFGI Cascades"), VIEW_DISPLAY_DEBUG_SDFGI);
	display_submenu->add_radio_check_item(TTR("SDFGI Probes"), VIEW_DISPLAY_DEBUG_SDFGI_PROBES);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("Scene Luminance"), VIEW_DISPLAY_DEBUG_SCENE_LUMINANCE);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("SSAO"), VIEW_DISPLAY_DEBUG_SSAO);
	display_submenu->add_radio_check_item(TTR("SSIL"), VIEW_DISPLAY_DEBUG_SSIL);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("VoxelGI/SDFGI Buffer"), VIEW_DISPLAY_DEBUG_GI_BUFFER);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("Disable Mesh LOD"), VIEW_DISPLAY_DEBUG_DISABLE_LOD);
	display_submenu->add_separator();
	display_submenu->add_radio_check_item(TTR("OmniLight3D Cluster"), VIEW_DISPLAY_DEBUG_CLUSTER_OMNI_LIGHTS);
	display_submenu->add_radio_check_item(TTR("SpotLight3D Cluster"), VIEW_DISPLAY_DEBUG_CLUSTER_SPOT_LIGHTS);
	display_submenu->add_radio_check_item(TTR("Decal Cluster"), VIEW_DISPLAY_DEBUG_CLUSTER_DECALS);
	display_submenu->add_radio_check_item(TTR("ReflectionProbe Cluster"), VIEW_DISPLAY_DEBUG_CLUSTER_REFLECTION_PROBES);
	display_submenu->add_radio_check_item(TTR("Occlusion Culling Buffer"), VIEW_DISPLAY_DEBUG_OCCLUDERS);
	display_submenu->add_radio_check_item(TTR("Motion Vectors"), VIEW_DISPLAY_MOTION_VECTORS);
	display_submenu->add_radio_check_item(TTR("Internal Buffer"), VIEW_DISPLAY_INTERNAL_BUFFER);
	view_menu->get_popup()->add_submenu_node_item(TTR("Display Advanced..."), display_submenu, VIEW_DISPLAY_ADVANCED);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_environment", TTRC("View Environment")), VIEW_ENVIRONMENT);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_gizmos", TTRC("View Gizmos")), VIEW_GIZMOS);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_transform_gizmo", TTRC("View Transform Gizmo")), VIEW_TRANSFORM_GIZMO);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_grid_lines", TTRC("View Grid")), VIEW_GRID);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_information", TTRC("View Information")), VIEW_INFORMATION);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_fps", TTRC("View Frame Time")), VIEW_FRAME_TIME);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_half_resolution", TTRC("Half Resolution")), VIEW_HALF_RESOLUTION);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_audio_listener", TTRC("Audio Listener")), VIEW_AUDIO_LISTENER);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_audio_doppler", TTRC("Enable Doppler")), VIEW_AUDIO_DOPPLER);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_GIZMOS), true);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_TRANSFORM_GIZMO), true);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_GRID), true);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_cinematic_preview", TTRC("Cinematic Preview")), VIEW_CINEMATIC_PREVIEW);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_origin"), VIEW_CENTER_TO_ORIGIN);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_selection"), VIEW_CENTER_TO_SELECTION);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_transform_with_view"), VIEW_ALIGN_TRANSFORM_WITH_VIEW);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_rotation_with_view"), VIEW_ALIGN_ROTATION_WITH_VIEW);
	view_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditorViewport::_menu_option));
	display_submenu->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditorViewport::_menu_option));
	view_menu->set_disable_shortcuts(true);

	// TODO: Re-evaluate with new OpenGL3 renderer, and implement.
	//if (OS::get_singleton()->get_current_video_driver() == OS::RENDERING_DRIVER_OPENGL3) {
	if (false) {
		// Alternate display modes only work when using the Vulkan renderer; make this explicit.
		const int normal_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL);
		const int wireframe_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME);
		const int overdraw_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW);
		const int shadeless_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_UNSHADED);
		const String unsupported_tooltip = TTR("Not available when using the OpenGL renderer.");

		view_menu->get_popup()->set_item_disabled(normal_idx, true);
		view_menu->get_popup()->set_item_tooltip(normal_idx, unsupported_tooltip);
		view_menu->get_popup()->set_item_disabled(wireframe_idx, true);
		view_menu->get_popup()->set_item_tooltip(wireframe_idx, unsupported_tooltip);
		view_menu->get_popup()->set_item_disabled(overdraw_idx, true);
		view_menu->get_popup()->set_item_tooltip(overdraw_idx, unsupported_tooltip);
		view_menu->get_popup()->set_item_disabled(shadeless_idx, true);
		view_menu->get_popup()->set_item_tooltip(shadeless_idx, unsupported_tooltip);
	}

	// Registering with Key::NONE intentionally creates an empty Array.
	register_shortcut_action("spatial_editor/viewport_orbit_modifier_1", TTRC("Viewport Orbit Modifier 1"), Key::NONE);
	register_shortcut_action("spatial_editor/viewport_orbit_modifier_2", TTRC("Viewport Orbit Modifier 2"), Key::NONE);
	register_shortcut_action("spatial_editor/viewport_pan_modifier_1", TTRC("Viewport Pan Modifier 1"), Key::SHIFT);
	register_shortcut_action("spatial_editor/viewport_pan_modifier_2", TTRC("Viewport Pan Modifier 2"), Key::NONE);
	register_shortcut_action("spatial_editor/viewport_zoom_modifier_1", TTRC("Viewport Zoom Modifier 1"), Key::SHIFT);
	register_shortcut_action("spatial_editor/viewport_zoom_modifier_2", TTRC("Viewport Zoom Modifier 2"), Key::CTRL);

	register_shortcut_action("spatial_editor/freelook_left", TTRC("Freelook Left"), Key::A, true);
	register_shortcut_action("spatial_editor/freelook_right", TTRC("Freelook Right"), Key::D, true);
	register_shortcut_action("spatial_editor/freelook_forward", TTRC("Freelook Forward"), Key::W, true);
	register_shortcut_action("spatial_editor/freelook_backwards", TTRC("Freelook Backwards"), Key::S, true);
	register_shortcut_action("spatial_editor/freelook_up", TTRC("Freelook Up"), Key::E, true);
	register_shortcut_action("spatial_editor/freelook_down", TTRC("Freelook Down"), Key::Q, true);
	register_shortcut_action("spatial_editor/freelook_speed_modifier", TTRC("Freelook Speed Modifier"), Key::SHIFT);
	register_shortcut_action("spatial_editor/freelook_slow_modifier", TTRC("Freelook Slow Modifier"), Key::ALT);

	ED_SHORTCUT("spatial_editor/lock_transform_x", TTRC("Lock Transformation to X axis"), Key::X);
	ED_SHORTCUT("spatial_editor/lock_transform_y", TTRC("Lock Transformation to Y axis"), Key::Y);
	ED_SHORTCUT("spatial_editor/lock_transform_z", TTRC("Lock Transformation to Z axis"), Key::Z);
	ED_SHORTCUT("spatial_editor/lock_transform_yz", TTRC("Lock Transformation to YZ plane"), KeyModifierMask::SHIFT | Key::X);
	ED_SHORTCUT("spatial_editor/lock_transform_xz", TTRC("Lock Transformation to XZ plane"), KeyModifierMask::SHIFT | Key::Y);
	ED_SHORTCUT("spatial_editor/lock_transform_xy", TTRC("Lock Transformation to XY plane"), KeyModifierMask::SHIFT | Key::Z);
	ED_SHORTCUT("spatial_editor/cancel_transform", TTRC("Cancel Transformation"), Key::ESCAPE);
	ED_SHORTCUT("spatial_editor/instant_translate", TTRC("Begin Translate Transformation"));
	ED_SHORTCUT("spatial_editor/instant_rotate", TTRC("Begin Rotate Transformation"));
	ED_SHORTCUT("spatial_editor/instant_scale", TTRC("Begin Scale Transformation"));
	ED_SHORTCUT("spatial_editor/collision_reposition", TTRC("Reposition Using Collisions"), KeyModifierMask::SHIFT | Key::G);

	preview_camera = memnew(CheckBox);
	preview_camera->set_text(TTR("Preview"));
	// Using Control even on macOS to avoid conflict with Quick Open shortcut.
	preview_camera->set_shortcut(ED_SHORTCUT("spatial_editor/toggle_camera_preview", TTRC("Toggle Camera Preview"), KeyModifierMask::CTRL | Key::P));
	vbox->add_child(preview_camera);
	preview_camera->set_h_size_flags(0);
	preview_camera->hide();
	preview_camera->connect(SceneStringName(toggled), callable_mp(this, &Node3DEditorViewport::_toggle_camera_preview));
	previewing = nullptr;
	gizmo_scale = 1.0;

	preview_node = nullptr;

	bottom_center_vbox = memnew(VBoxContainer);
	bottom_center_vbox->set_anchors_preset(LayoutPreset::PRESET_CENTER);
	bottom_center_vbox->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -20 * EDSCALE);
	bottom_center_vbox->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -10 * EDSCALE);
	bottom_center_vbox->set_h_grow_direction(GROW_DIRECTION_BOTH);
	bottom_center_vbox->set_v_grow_direction(GROW_DIRECTION_BEGIN);
	surface->add_child(bottom_center_vbox);

	info_panel = memnew(PanelContainer);
	info_panel->set_anchor_and_offset(SIDE_LEFT, ANCHOR_END, -90 * EDSCALE);
	info_panel->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -90 * EDSCALE);
	info_panel->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, -10 * EDSCALE);
	info_panel->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -10 * EDSCALE);
	info_panel->set_h_grow_direction(GROW_DIRECTION_BEGIN);
	info_panel->set_v_grow_direction(GROW_DIRECTION_BEGIN);
	info_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
	surface->add_child(info_panel);
	info_panel->hide();

	info_label = memnew(Label);
	info_panel->add_child(info_label);

	cinema_label = memnew(Label);
	cinema_label->set_anchor_and_offset(SIDE_TOP, ANCHOR_BEGIN, 10 * EDSCALE);
	cinema_label->set_h_grow_direction(GROW_DIRECTION_END);
	cinema_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	cinema_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	surface->add_child(cinema_label);
	cinema_label->set_text(TTR("Cinematic Preview"));
	cinema_label->hide();
	previewing_cinema = false;

	locked_label = memnew(Label);
	locked_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	locked_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	locked_label->set_h_size_flags(SIZE_SHRINK_CENTER);
	bottom_center_vbox->add_child(locked_label);
	locked_label->set_text(TTR("View Rotation Locked"));
	locked_label->hide();

	zoom_limit_label = memnew(Label);
	zoom_limit_label->set_text(TTR("To zoom further, change the camera's clipping planes (View -> Settings...)"));
	zoom_limit_label->set_name("ZoomLimitMessageLabel");
	zoom_limit_label->add_theme_color_override(SceneStringName(font_color), Color(1, 1, 1, 1));
	zoom_limit_label->hide();
	bottom_center_vbox->add_child(zoom_limit_label);

	preview_material_label = memnew(Label);
	preview_material_label->set_anchors_and_offsets_preset(LayoutPreset::PRESET_BOTTOM_LEFT);
	preview_material_label->set_offset(Side::SIDE_TOP, -70 * EDSCALE);
	preview_material_label->set_text(TTR("Overriding material..."));
	preview_material_label->add_theme_color_override(SceneStringName(font_color), Color(1, 1, 1, 1));
	preview_material_label->hide();
	surface->add_child(preview_material_label);

	preview_material_label_desc = memnew(Label);
	preview_material_label_desc->set_anchors_and_offsets_preset(LayoutPreset::PRESET_BOTTOM_LEFT);
	preview_material_label_desc->set_offset(Side::SIDE_TOP, -50 * EDSCALE);
	Key key = (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) ? Key::META : Key::CTRL;
	preview_material_label_desc->set_text(vformat(TTR("Drag and drop to override the material of any geometry node.\nHold %s when dropping to override a specific surface."), find_keycode_name(key)));
	preview_material_label_desc->add_theme_color_override(SceneStringName(font_color), Color(0.8, 0.8, 0.8, 1));
	preview_material_label_desc->add_theme_constant_override("line_spacing", 0);
	preview_material_label_desc->hide();
	surface->add_child(preview_material_label_desc);

	frame_time_gradient = memnew(Gradient);
	// The color is set when the theme changes.
	frame_time_gradient->add_point(0.5, Color());

	top_right_vbox = memnew(VBoxContainer);
	top_right_vbox->add_theme_constant_override("separation", 10.0 * EDSCALE);
	top_right_vbox->set_anchors_and_offsets_preset(PRESET_TOP_RIGHT, PRESET_MODE_MINSIZE, 10.0 * EDSCALE);
	top_right_vbox->set_h_grow_direction(GROW_DIRECTION_BEGIN);

	const int navigation_control_size = 150;

	position_control = memnew(ViewportNavigationControl);
	position_control->set_navigation_mode(Node3DEditorViewport::NAVIGATION_MOVE);
	position_control->set_custom_minimum_size(Size2(navigation_control_size, navigation_control_size) * EDSCALE);
	position_control->set_h_size_flags(SIZE_SHRINK_END);
	position_control->set_anchor_and_offset(SIDE_LEFT, ANCHOR_BEGIN, 0);
	position_control->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -navigation_control_size * EDSCALE);
	position_control->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_BEGIN, navigation_control_size * EDSCALE);
	position_control->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
	position_control->set_viewport(this);
	surface->add_child(position_control);

	look_control = memnew(ViewportNavigationControl);
	look_control->set_navigation_mode(Node3DEditorViewport::NAVIGATION_LOOK);
	look_control->set_custom_minimum_size(Size2(navigation_control_size, navigation_control_size) * EDSCALE);
	look_control->set_h_size_flags(SIZE_SHRINK_END);
	look_control->set_anchor_and_offset(SIDE_LEFT, ANCHOR_END, -navigation_control_size * EDSCALE);
	look_control->set_anchor_and_offset(SIDE_TOP, ANCHOR_END, -navigation_control_size * EDSCALE);
	look_control->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, 0);
	look_control->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
	look_control->set_viewport(this);
	surface->add_child(look_control);

	rotation_control = memnew(ViewportRotationControl);
	rotation_control->set_custom_minimum_size(Size2(80, 80) * EDSCALE);
	rotation_control->set_h_size_flags(SIZE_SHRINK_END);
	rotation_control->set_viewport(this);
	top_right_vbox->add_child(rotation_control);

	frame_time_panel = memnew(PanelContainer);
	frame_time_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
	top_right_vbox->add_child(frame_time_panel);
	frame_time_panel->hide();

	frame_time_vbox = memnew(VBoxContainer);
	frame_time_panel->add_child(frame_time_vbox);

	// Individual Labels are used to allow coloring each label with its own color.
	cpu_time_label = memnew(Label);
	frame_time_vbox->add_child(cpu_time_label);

	gpu_time_label = memnew(Label);
	frame_time_vbox->add_child(gpu_time_label);

	fps_label = memnew(Label);
	frame_time_vbox->add_child(fps_label);

	surface->add_child(top_right_vbox);

	accept = nullptr;

	freelook_active = false;
	freelook_speed = EDITOR_GET("editors/3d/freelook/freelook_base_speed");

	selection_menu = memnew(PopupMenu);
	add_child(selection_menu);
	selection_menu->set_min_size(Size2(100, 0) * EDSCALE);
	selection_menu->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditorViewport::_selection_result_pressed));
	selection_menu->connect("popup_hide", callable_mp(this, &Node3DEditorViewport::_selection_menu_hide));

	if (p_index == 0) {
		view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER), true);
		viewport->set_as_audio_listener_3d(true);
	}

	ruler = memnew(Node);

	ruler_start_point = memnew(Node3D);
	ruler_start_point->set_visible(false);

	ruler_end_point = memnew(Node3D);
	ruler_end_point->set_visible(false);

	ruler_material.instantiate();
	ruler_material->set_albedo(Color(1.0, 0.9, 0.0, 1.0));
	ruler_material->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ruler_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ruler_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);

	ruler_material_xray.instantiate();
	ruler_material_xray->set_albedo(Color(1.0, 0.9, 0.0, 0.15));
	ruler_material_xray->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	ruler_material_xray->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ruler_material_xray->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	ruler_material_xray->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	ruler_material_xray->set_render_priority(BaseMaterial3D::RENDER_PRIORITY_MAX);

	geometry.instantiate();

	ruler_line = memnew(MeshInstance3D);
	ruler_line->set_mesh(geometry);
	ruler_line->set_material_override(ruler_material);

	ruler_line_xray = memnew(MeshInstance3D);
	ruler_line_xray->set_mesh(geometry);
	ruler_line_xray->set_material_override(ruler_material_xray);

	ruler_label = memnew(Label);
	ruler_label->set_visible(false);

	ruler->add_child(ruler_start_point);
	ruler->add_child(ruler_end_point);
	ruler->add_child(ruler_line);
	ruler->add_child(ruler_line_xray);

	viewport->add_child(ruler_label);

	view_type = VIEW_TYPE_USER;
	_update_name();

	EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &Node3DEditorViewport::update_transform_gizmo_view));
}

Node3DEditorViewport::~Node3DEditorViewport() {
	memdelete(ruler);
	memdelete(frame_time_gradient);
}

//////////////////////////////////////////////////////////////

void Node3DEditorViewportContainer::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			Vector2 size = get_size();

			int h_sep = get_theme_constant(SNAME("separation"), SNAME("HSplitContainer"));
			int v_sep = get_theme_constant(SNAME("separation"), SNAME("VSplitContainer"));

			int mid_w = size.width * ratio_h;
			int mid_h = size.height * ratio_v;

			dragging_h = mb->get_position().x > (mid_w - h_sep / 2) && mb->get_position().x < (mid_w + h_sep / 2);
			dragging_v = mb->get_position().y > (mid_h - v_sep / 2) && mb->get_position().y < (mid_h + v_sep / 2);

			drag_begin_pos = mb->get_position();
			drag_begin_ratio.x = ratio_h;
			drag_begin_ratio.y = ratio_v;

			switch (view) {
				case VIEW_USE_1_VIEWPORT: {
					dragging_h = false;
					dragging_v = false;

				} break;
				case VIEW_USE_2_VIEWPORTS: {
					dragging_h = false;

				} break;
				case VIEW_USE_2_VIEWPORTS_ALT: {
					dragging_v = false;

				} break;
				case VIEW_USE_3_VIEWPORTS:
				case VIEW_USE_3_VIEWPORTS_ALT:
				case VIEW_USE_4_VIEWPORTS: {
					// Do nothing.

				} break;
			}
		} else {
			dragging_h = false;
			dragging_v = false;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (view == VIEW_USE_3_VIEWPORTS || view == VIEW_USE_3_VIEWPORTS_ALT || view == VIEW_USE_4_VIEWPORTS) {
			Vector2 size = get_size();

			int h_sep = get_theme_constant(SNAME("separation"), SNAME("HSplitContainer"));
			int v_sep = get_theme_constant(SNAME("separation"), SNAME("VSplitContainer"));

			int mid_w = size.width * ratio_h;
			int mid_h = size.height * ratio_v;

			bool was_hovering_h = hovering_h;
			bool was_hovering_v = hovering_v;
			hovering_h = mm->get_position().x > (mid_w - h_sep / 2) && mm->get_position().x < (mid_w + h_sep / 2);
			hovering_v = mm->get_position().y > (mid_h - v_sep / 2) && mm->get_position().y < (mid_h + v_sep / 2);

			if (was_hovering_h != hovering_h || was_hovering_v != hovering_v) {
				queue_redraw();
			}
		}

		if (dragging_h) {
			real_t new_ratio = drag_begin_ratio.x + (mm->get_position().x - drag_begin_pos.x) / get_size().width;
			new_ratio = CLAMP(new_ratio, 40 / get_size().width, (get_size().width - 40) / get_size().width);
			ratio_h = new_ratio;
			queue_sort();
			queue_redraw();
		}
		if (dragging_v) {
			real_t new_ratio = drag_begin_ratio.y + (mm->get_position().y - drag_begin_pos.y) / get_size().height;
			new_ratio = CLAMP(new_ratio, 40 / get_size().height, (get_size().height - 40) / get_size().height);
			ratio_v = new_ratio;
			queue_sort();
			queue_redraw();
		}
	}
}

void Node3DEditorViewportContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_ENTER:
		case NOTIFICATION_MOUSE_EXIT: {
			mouseover = (p_what == NOTIFICATION_MOUSE_ENTER);
			queue_redraw();
		} break;

		case NOTIFICATION_DRAW: {
			if (mouseover) {
				Ref<Texture2D> h_grabber = get_theme_icon(SNAME("grabber"), SNAME("HSplitContainer"));
				Ref<Texture2D> v_grabber = get_theme_icon(SNAME("grabber"), SNAME("VSplitContainer"));

				Ref<Texture2D> hdiag_grabber = get_editor_theme_icon(SNAME("GuiViewportHdiagsplitter"));
				Ref<Texture2D> vdiag_grabber = get_editor_theme_icon(SNAME("GuiViewportVdiagsplitter"));
				Ref<Texture2D> vh_grabber = get_editor_theme_icon(SNAME("GuiViewportVhsplitter"));

				Vector2 size = get_size();

				int h_sep = get_theme_constant(SNAME("separation"), SNAME("HSplitContainer"));

				int v_sep = get_theme_constant(SNAME("separation"), SNAME("VSplitContainer"));

				int mid_w = size.width * ratio_h;
				int mid_h = size.height * ratio_v;

				int size_left = mid_w - h_sep / 2;
				int size_bottom = size.height - mid_h - v_sep / 2;

				switch (view) {
					case VIEW_USE_1_VIEWPORT: {
						// Nothing to show.

					} break;
					case VIEW_USE_2_VIEWPORTS: {
						draw_texture(v_grabber, Vector2((size.width - v_grabber->get_width()) / 2, mid_h - v_grabber->get_height() / 2));
						set_default_cursor_shape(CURSOR_VSPLIT);

					} break;
					case VIEW_USE_2_VIEWPORTS_ALT: {
						draw_texture(h_grabber, Vector2(mid_w - h_grabber->get_width() / 2, (size.height - h_grabber->get_height()) / 2));
						set_default_cursor_shape(CURSOR_HSPLIT);

					} break;
					case VIEW_USE_3_VIEWPORTS: {
						if ((hovering_v && hovering_h && !dragging_v && !dragging_h) || (dragging_v && dragging_h)) {
							draw_texture(hdiag_grabber, Vector2(mid_w - hdiag_grabber->get_width() / 2, mid_h - v_grabber->get_height() / 4));
							set_default_cursor_shape(CURSOR_DRAG);
						} else if ((hovering_v && !dragging_h) || dragging_v) {
							draw_texture(v_grabber, Vector2((size.width - v_grabber->get_width()) / 2, mid_h - v_grabber->get_height() / 2));
							set_default_cursor_shape(CURSOR_VSPLIT);
						} else if (hovering_h || dragging_h) {
							draw_texture(h_grabber, Vector2(mid_w - h_grabber->get_width() / 2, mid_h + v_grabber->get_height() / 2 + (size_bottom - h_grabber->get_height()) / 2));
							set_default_cursor_shape(CURSOR_HSPLIT);
						}

					} break;
					case VIEW_USE_3_VIEWPORTS_ALT: {
						if ((hovering_v && hovering_h && !dragging_v && !dragging_h) || (dragging_v && dragging_h)) {
							draw_texture(vdiag_grabber, Vector2(mid_w - vdiag_grabber->get_width() + v_grabber->get_height() / 4, mid_h - vdiag_grabber->get_height() / 2));
							set_default_cursor_shape(CURSOR_DRAG);
						} else if ((hovering_v && !dragging_h) || dragging_v) {
							draw_texture(v_grabber, Vector2((size_left - v_grabber->get_width()) / 2, mid_h - v_grabber->get_height() / 2));
							set_default_cursor_shape(CURSOR_VSPLIT);
						} else if (hovering_h || dragging_h) {
							draw_texture(h_grabber, Vector2(mid_w - h_grabber->get_width() / 2, (size.height - h_grabber->get_height()) / 2));
							set_default_cursor_shape(CURSOR_HSPLIT);
						}

					} break;
					case VIEW_USE_4_VIEWPORTS: {
						Vector2 half(mid_w, mid_h);
						if ((hovering_v && hovering_h && !dragging_v && !dragging_h) || (dragging_v && dragging_h)) {
							draw_texture(vh_grabber, half - vh_grabber->get_size() / 2.0);
							set_default_cursor_shape(CURSOR_DRAG);
						} else if ((hovering_v && !dragging_h) || dragging_v) {
							draw_texture(v_grabber, half - v_grabber->get_size() / 2.0);
							set_default_cursor_shape(CURSOR_VSPLIT);
						} else if (hovering_h || dragging_h) {
							draw_texture(h_grabber, half - h_grabber->get_size() / 2.0);
							set_default_cursor_shape(CURSOR_HSPLIT);
						}

					} break;
				}
			}
		} break;

		case NOTIFICATION_SORT_CHILDREN: {
			Node3DEditorViewport *viewports[4];
			int vc = 0;
			for (int i = 0; i < get_child_count(); i++) {
				viewports[vc] = Object::cast_to<Node3DEditorViewport>(get_child(i));
				if (viewports[vc]) {
					vc++;
				}
			}

			ERR_FAIL_COND(vc != 4);

			Size2 size = get_size();

			if (size.x < 10 || size.y < 10) {
				for (int i = 0; i < 4; i++) {
					viewports[i]->hide();
				}
				return;
			}
			int h_sep = get_theme_constant(SNAME("separation"), SNAME("HSplitContainer"));

			int v_sep = get_theme_constant(SNAME("separation"), SNAME("VSplitContainer"));

			int mid_w = size.width * ratio_h;
			int mid_h = size.height * ratio_v;

			int size_left = mid_w - h_sep / 2;
			int size_right = size.width - mid_w - h_sep / 2;

			int size_top = mid_h - v_sep / 2;
			int size_bottom = size.height - mid_h - v_sep / 2;

			switch (view) {
				case VIEW_USE_1_VIEWPORT: {
					viewports[0]->show();
					for (int i = 1; i < 4; i++) {
						viewports[i]->hide();
					}

					fit_child_in_rect(viewports[0], Rect2(Vector2(), size));

				} break;
				case VIEW_USE_2_VIEWPORTS: {
					for (int i = 0; i < 4; i++) {
						if (i == 1 || i == 3) {
							viewports[i]->hide();
						} else {
							viewports[i]->show();
						}
					}

					fit_child_in_rect(viewports[0], Rect2(Vector2(), Vector2(size.width, size_top)));
					fit_child_in_rect(viewports[2], Rect2(Vector2(0, mid_h + v_sep / 2), Vector2(size.width, size_bottom)));

				} break;
				case VIEW_USE_2_VIEWPORTS_ALT: {
					for (int i = 0; i < 4; i++) {
						if (i == 1 || i == 3) {
							viewports[i]->hide();
						} else {
							viewports[i]->show();
						}
					}
					fit_child_in_rect(viewports[0], Rect2(Vector2(), Vector2(size_left, size.height)));
					fit_child_in_rect(viewports[2], Rect2(Vector2(mid_w + h_sep / 2, 0), Vector2(size_right, size.height)));

				} break;
				case VIEW_USE_3_VIEWPORTS: {
					for (int i = 0; i < 4; i++) {
						if (i == 1) {
							viewports[i]->hide();
						} else {
							viewports[i]->show();
						}
					}

					fit_child_in_rect(viewports[0], Rect2(Vector2(), Vector2(size.width, size_top)));
					fit_child_in_rect(viewports[2], Rect2(Vector2(0, mid_h + v_sep / 2), Vector2(size_left, size_bottom)));
					fit_child_in_rect(viewports[3], Rect2(Vector2(mid_w + h_sep / 2, mid_h + v_sep / 2), Vector2(size_right, size_bottom)));

				} break;
				case VIEW_USE_3_VIEWPORTS_ALT: {
					for (int i = 0; i < 4; i++) {
						if (i == 1) {
							viewports[i]->hide();
						} else {
							viewports[i]->show();
						}
					}

					fit_child_in_rect(viewports[0], Rect2(Vector2(), Vector2(size_left, size_top)));
					fit_child_in_rect(viewports[2], Rect2(Vector2(0, mid_h + v_sep / 2), Vector2(size_left, size_bottom)));
					fit_child_in_rect(viewports[3], Rect2(Vector2(mid_w + h_sep / 2, 0), Vector2(size_right, size.height)));

				} break;
				case VIEW_USE_4_VIEWPORTS: {
					for (int i = 0; i < 4; i++) {
						viewports[i]->show();
					}

					fit_child_in_rect(viewports[0], Rect2(Vector2(), Vector2(size_left, size_top)));
					fit_child_in_rect(viewports[1], Rect2(Vector2(mid_w + h_sep / 2, 0), Vector2(size_right, size_top)));
					fit_child_in_rect(viewports[2], Rect2(Vector2(0, mid_h + v_sep / 2), Vector2(size_left, size_bottom)));
					fit_child_in_rect(viewports[3], Rect2(Vector2(mid_w + h_sep / 2, mid_h + v_sep / 2), Vector2(size_right, size_bottom)));

				} break;
			}
		} break;
	}
}

void Node3DEditorViewportContainer::set_view(View p_view) {
	view = p_view;
	queue_sort();
}

Node3DEditorViewportContainer::View Node3DEditorViewportContainer::get_view() {
	return view;
}

Node3DEditorViewportContainer::Node3DEditorViewportContainer() {
	set_clip_contents(true);
	view = VIEW_USE_1_VIEWPORT;
	mouseover = false;
	ratio_h = 0.5;
	ratio_v = 0.5;
	hovering_v = false;
	hovering_h = false;
	dragging_v = false;
	dragging_h = false;
}

///////////////////////////////////////////////////////////////////

Node3DEditor *Node3DEditor::singleton = nullptr;

Node3DEditorSelectedItem::~Node3DEditorSelectedItem() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (sbox_instance.is_valid()) {
		RenderingServer::get_singleton()->free(sbox_instance);
	}
	if (sbox_instance_offset.is_valid()) {
		RenderingServer::get_singleton()->free(sbox_instance_offset);
	}
	if (sbox_instance_xray.is_valid()) {
		RenderingServer::get_singleton()->free(sbox_instance_xray);
	}
	if (sbox_instance_xray_offset.is_valid()) {
		RenderingServer::get_singleton()->free(sbox_instance_xray_offset);
	}
}

void Node3DEditor::select_gizmo_highlight_axis(int p_axis) {
	for (int i = 0; i < 3; i++) {
		move_gizmo[i]->surface_set_material(0, i == p_axis ? gizmo_color_hl[i] : gizmo_color[i]);
		move_plane_gizmo[i]->surface_set_material(0, (i + 6) == p_axis ? plane_gizmo_color_hl[i] : plane_gizmo_color[i]);
		rotate_gizmo[i]->surface_set_material(0, (i + 3) == p_axis ? rotate_gizmo_color_hl[i] : rotate_gizmo_color[i]);
		scale_gizmo[i]->surface_set_material(0, (i + 9) == p_axis ? gizmo_color_hl[i] : gizmo_color[i]);
		scale_plane_gizmo[i]->surface_set_material(0, (i + 12) == p_axis ? plane_gizmo_color_hl[i] : plane_gizmo_color[i]);
	}
}

void Node3DEditor::update_transform_gizmo() {
	int count = 0;
	bool local_gizmo_coords = are_local_coords_enabled();

	Vector3 gizmo_center;
	Basis gizmo_basis;

	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;

	if (se && se->gizmo.is_valid()) {
		for (const KeyValue<int, Transform3D> &E : se->subgizmos) {
			Transform3D xf = se->sp->get_global_transform() * se->gizmo->get_subgizmo_transform(E.key);
			gizmo_center += xf.origin;
			if (count == 0 && local_gizmo_coords) {
				gizmo_basis = xf.basis;
			}
			count++;
		}
	} else {
		List<Node *> &selection = editor_selection->get_selected_node_list();
		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			Node3D *sp = Object::cast_to<Node3D>(E->get());
			if (!sp) {
				continue;
			}

			if (sp->has_meta("_edit_lock_")) {
				continue;
			}

			Node3DEditorSelectedItem *sel_item = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
			if (!sel_item) {
				continue;
			}

			Transform3D xf = sel_item->sp->get_global_transform();
			gizmo_center += xf.origin;
			if (count == 0 && local_gizmo_coords) {
				gizmo_basis = xf.basis;
			}
			count++;
		}
	}

	gizmo.visible = count > 0;
	gizmo.transform.origin = (count > 0) ? gizmo_center / count : Vector3();
	gizmo.transform.basis = (count == 1) ? gizmo_basis : Basis();

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->update_transform_gizmo_view();
	}
}

void _update_all_gizmos(Node *p_node) {
	for (int i = p_node->get_child_count() - 1; 0 <= i; --i) {
		Node3D *spatial_node = Object::cast_to<Node3D>(p_node->get_child(i));
		if (spatial_node) {
			spatial_node->update_gizmos();
		}

		_update_all_gizmos(p_node->get_child(i));
	}
}

void Node3DEditor::update_all_gizmos(Node *p_node) {
	if (!p_node && is_inside_tree()) {
		p_node = get_tree()->get_edited_scene_root();
	}

	if (!p_node) {
		// No edited scene, so nothing to update.
		return;
	}
	_update_all_gizmos(p_node);
}

Object *Node3DEditor::_get_editor_data(Object *p_what) {
	Node3D *sp = Object::cast_to<Node3D>(p_what);
	if (!sp) {
		return nullptr;
	}

	Node3DEditorSelectedItem *si = memnew(Node3DEditorSelectedItem);

	si->sp = sp;
	si->sbox_instance = RenderingServer::get_singleton()->instance_create2(
			selection_box->get_rid(),
			sp->get_world_3d()->get_scenario());
	si->sbox_instance_offset = RenderingServer::get_singleton()->instance_create2(
			selection_box->get_rid(),
			sp->get_world_3d()->get_scenario());
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance,
			RS::SHADOW_CASTING_SETTING_OFF);
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance_offset,
			RS::SHADOW_CASTING_SETTING_OFF);
	// Use the Edit layer to hide the selection box when View Gizmos is disabled, since it is a bit distracting.
	// It's still possible to approximately guess what is selected by looking at the manipulation gizmo position.
	RS::get_singleton()->instance_set_layer_mask(si->sbox_instance, 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER);
	RS::get_singleton()->instance_set_layer_mask(si->sbox_instance_offset, 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_offset, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_offset, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
	si->sbox_instance_xray = RenderingServer::get_singleton()->instance_create2(
			selection_box_xray->get_rid(),
			sp->get_world_3d()->get_scenario());
	si->sbox_instance_xray_offset = RenderingServer::get_singleton()->instance_create2(
			selection_box_xray->get_rid(),
			sp->get_world_3d()->get_scenario());
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance_xray,
			RS::SHADOW_CASTING_SETTING_OFF);
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance_xray_offset,
			RS::SHADOW_CASTING_SETTING_OFF);
	// Use the Edit layer to hide the selection box when View Gizmos is disabled, since it is a bit distracting.
	// It's still possible to approximately guess what is selected by looking at the manipulation gizmo position.
	RS::get_singleton()->instance_set_layer_mask(si->sbox_instance_xray, 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER);
	RS::get_singleton()->instance_set_layer_mask(si->sbox_instance_xray_offset, 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_xray, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_xray, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_xray_offset, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(si->sbox_instance_xray_offset, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

	return si;
}

void Node3DEditor::_generate_selection_boxes() {
	// Use two AABBs to create the illusion of a slightly thicker line.
	AABB aabb(Vector3(), Vector3(1, 1, 1));

	// Create a x-ray (visible through solid surfaces) and standard version of the selection box.
	// Both will be drawn at the same position, but with different opacity.
	// This lets the user see where the selection is while still having a sense of depth.
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<SurfaceTool> st_xray = memnew(SurfaceTool);

	st->begin(Mesh::PRIMITIVE_LINES);
	st_xray->begin(Mesh::PRIMITIVE_LINES);
	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);

		st->add_vertex(a);
		st->add_vertex(b);
		st_xray->add_vertex(a);
		st_xray->add_vertex(b);
	}

	Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
	mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	const Color selection_box_color = EDITOR_GET("editors/3d/selection_box_color");
	mat->set_albedo(selection_box_color);
	mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	st->set_material(mat);
	selection_box = st->commit();

	Ref<StandardMaterial3D> mat_xray = memnew(StandardMaterial3D);
	mat_xray->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat_xray->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	mat_xray->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	mat_xray->set_albedo(selection_box_color * Color(1, 1, 1, 0.15));
	mat_xray->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	st_xray->set_material(mat_xray);
	selection_box_xray = st_xray->commit();
}

Dictionary Node3DEditor::get_state() const {
	Dictionary d;

	d["snap_enabled"] = snap_enabled;
	d["translate_snap"] = snap_translate_value;
	d["rotate_snap"] = snap_rotate_value;
	d["scale_snap"] = snap_scale_value;

	d["local_coords"] = tool_option_button[TOOL_OPT_LOCAL_COORDS]->is_pressed();

	int vc = 0;
	if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT))) {
		vc = 1;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS))) {
		vc = 2;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS))) {
		vc = 3;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS))) {
		vc = 4;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT))) {
		vc = 5;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT))) {
		vc = 6;
	}

	d["viewport_mode"] = vc;
	Array vpdata;
	for (int i = 0; i < 4; i++) {
		vpdata.push_back(viewports[i]->get_state());
	}

	d["viewports"] = vpdata;

	d["show_grid"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID));
	d["show_origin"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN));
	d["fov"] = get_fov();
	d["znear"] = get_znear();
	d["zfar"] = get_zfar();

	Dictionary gizmos_status;
	for (int i = 0; i < gizmo_plugins_by_name.size(); i++) {
		if (!gizmo_plugins_by_name[i]->can_be_hidden()) {
			continue;
		}
		int state = gizmos_menu->get_item_state(gizmos_menu->get_item_index(i));
		String name = gizmo_plugins_by_name[i]->get_gizmo_name();
		gizmos_status[name] = state;
	}

	d["gizmos_status"] = gizmos_status;
	{
		Dictionary pd;

		pd["sun_rotation"] = sun_rotation;

		pd["environ_sky_color"] = environ_sky_color->get_pick_color();
		pd["environ_ground_color"] = environ_ground_color->get_pick_color();
		pd["environ_energy"] = environ_energy->get_value();
		pd["environ_glow_enabled"] = environ_glow_button->is_pressed();
		pd["environ_tonemap_enabled"] = environ_tonemap_button->is_pressed();
		pd["environ_ao_enabled"] = environ_ao_button->is_pressed();
		pd["environ_gi_enabled"] = environ_gi_button->is_pressed();
		pd["sun_max_distance"] = sun_max_distance->get_value();

		pd["sun_color"] = sun_color->get_pick_color();
		pd["sun_energy"] = sun_energy->get_value();

		pd["sun_enabled"] = sun_button->is_pressed();
		pd["environ_enabled"] = environ_button->is_pressed();

		d["preview_sun_env"] = pd;
	}

	return d;
}

void Node3DEditor::set_state(const Dictionary &p_state) {
	Dictionary d = p_state;

	if (d.has("snap_enabled")) {
		snap_enabled = d["snap_enabled"];
		tool_option_button[TOOL_OPT_USE_SNAP]->set_pressed(d["snap_enabled"]);
	}

	if (d.has("translate_snap")) {
		snap_translate_value = d["translate_snap"];
	}

	if (d.has("rotate_snap")) {
		snap_rotate_value = d["rotate_snap"];
	}

	if (d.has("scale_snap")) {
		snap_scale_value = d["scale_snap"];
	}

	_snap_update();

	if (d.has("local_coords")) {
		tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_pressed(d["local_coords"]);
		update_transform_gizmo();
	}

	if (d.has("viewport_mode")) {
		int vc = d["viewport_mode"];

		if (vc == 1) {
			_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);
		} else if (vc == 2) {
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS);
		} else if (vc == 3) {
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS);
		} else if (vc == 4) {
			_menu_item_pressed(MENU_VIEW_USE_4_VIEWPORTS);
		} else if (vc == 5) {
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS_ALT);
		} else if (vc == 6) {
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS_ALT);
		}
	}

	if (d.has("viewports")) {
		Array vp = d["viewports"];
		uint32_t vp_size = static_cast<uint32_t>(vp.size());
		if (vp_size > VIEWPORTS_COUNT) {
			WARN_PRINT("Ignoring superfluous viewport settings from spatial editor state.");
			vp_size = VIEWPORTS_COUNT;
		}

		for (uint32_t i = 0; i < vp_size; i++) {
			viewports[i]->set_state(vp[i]);
		}
	}

	if (d.has("zfar")) {
		settings_zfar->set_value(double(d["zfar"]));
	}
	if (d.has("znear")) {
		settings_znear->set_value(double(d["znear"]));
	}
	if (d.has("fov")) {
		settings_fov->set_value(double(d["fov"]));
	}
	if (d.has("show_grid")) {
		bool use = d["show_grid"];

		if (use != view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID))) {
			_menu_item_pressed(MENU_VIEW_GRID);
		}
	}

	if (d.has("show_origin")) {
		bool use = d["show_origin"];

		if (use != view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN))) {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN), use);
			RenderingServer::get_singleton()->instance_set_visible(origin_instance, use);
		}
	}

	if (d.has("gizmos_status")) {
		Dictionary gizmos_status = d["gizmos_status"];
		List<Variant> keys;
		gizmos_status.get_key_list(&keys);

		for (int j = 0; j < gizmo_plugins_by_name.size(); ++j) {
			if (!gizmo_plugins_by_name[j]->can_be_hidden()) {
				continue;
			}
			int state = EditorNode3DGizmoPlugin::VISIBLE;
			for (const Variant &key : keys) {
				if (gizmo_plugins_by_name.write[j]->get_gizmo_name() == String(key)) {
					state = gizmos_status[key];
					break;
				}
			}

			gizmo_plugins_by_name.write[j]->set_state(state);
		}
		_update_gizmos_menu();
	}

	if (d.has("preview_sun_env")) {
		sun_environ_updating = true;
		Dictionary pd = d["preview_sun_env"];
		sun_rotation = pd["sun_rotation"];

		environ_sky_color->set_pick_color(pd["environ_sky_color"]);
		environ_ground_color->set_pick_color(pd["environ_ground_color"]);
		environ_energy->set_value(pd["environ_energy"]);
		environ_glow_button->set_pressed(pd["environ_glow_enabled"]);
		environ_tonemap_button->set_pressed(pd["environ_tonemap_enabled"]);
		environ_ao_button->set_pressed(pd["environ_ao_enabled"]);
		environ_gi_button->set_pressed(pd["environ_gi_enabled"]);
		sun_max_distance->set_value(pd["sun_max_distance"]);

		sun_color->set_pick_color(pd["sun_color"]);
		sun_energy->set_value(pd["sun_energy"]);

		sun_button->set_pressed(pd["sun_enabled"]);
		environ_button->set_pressed(pd["environ_enabled"]);

		sun_environ_updating = false;

		_preview_settings_changed();
		_update_preview_environment();
	} else {
		_load_default_preview_settings();
		sun_button->set_pressed(true);
		environ_button->set_pressed(true);
		_preview_settings_changed();
		_update_preview_environment();
	}
}

void Node3DEditor::edit(Node3D *p_spatial) {
	if (p_spatial != selected) {
		if (selected) {
			Vector<Ref<Node3DGizmo>> gizmos = selected->get_gizmos();
			for (int i = 0; i < gizmos.size(); i++) {
				Ref<EditorNode3DGizmo> seg = gizmos[i];
				if (seg.is_null()) {
					continue;
				}
				seg->set_selected(false);
			}

			Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected);
			if (se) {
				se->gizmo.unref();
				se->subgizmos.clear();
			}

			selected->update_gizmos();
		}

		selected = p_spatial;
		current_hover_gizmo = Ref<EditorNode3DGizmo>();
		current_hover_gizmo_handle = -1;
		current_hover_gizmo_handle_secondary = false;

		if (selected) {
			Vector<Ref<Node3DGizmo>> gizmos = selected->get_gizmos();
			for (int i = 0; i < gizmos.size(); i++) {
				Ref<EditorNode3DGizmo> seg = gizmos[i];
				if (seg.is_null()) {
					continue;
				}
				seg->set_selected(true);
			}
			selected->update_gizmos();
		}
	}
}

void Node3DEditor::_snap_changed() {
	snap_translate_value = snap_translate->get_text().to_float();
	snap_rotate_value = snap_rotate->get_text().to_float();
	snap_scale_value = snap_scale->get_text().to_float();

	EditorSettings::get_singleton()->set_project_metadata("3d_editor", "snap_translate_value", snap_translate_value);
	EditorSettings::get_singleton()->set_project_metadata("3d_editor", "snap_rotate_value", snap_rotate_value);
	EditorSettings::get_singleton()->set_project_metadata("3d_editor", "snap_scale_value", snap_scale_value);
}

void Node3DEditor::_snap_update() {
	double snap = EDITOR_GET("interface/inspector/default_float_step");
	int snap_step_decimals = Math::range_step_decimals(snap);

	snap_translate->set_text(String::num(snap_translate_value, snap_step_decimals));
	snap_rotate->set_text(String::num(snap_rotate_value, snap_step_decimals));
	snap_scale->set_text(String::num(snap_scale_value, snap_step_decimals));
}

void Node3DEditor::_xform_dialog_action() {
	Transform3D t;
	//translation
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;

	for (int i = 0; i < 3; i++) {
		translate[i] = xform_translate[i]->get_text().to_float();
		rotate[i] = Math::deg_to_rad(xform_rotate[i]->get_text().to_float());
		scale[i] = xform_scale[i]->get_text().to_float();
	}

	t.basis.scale(scale);
	t.basis.rotate(rotate);
	t.origin = translate;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("XForm Dialog"));

	const List<Node *> &selection = editor_selection->get_selected_node_list();

	for (Node *E : selection) {
		Node3D *sp = Object::cast_to<Node3D>(E);
		if (!sp) {
			continue;
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		bool post = xform_type->get_selected() > 0;

		Transform3D tr = sp->get_global_gizmo_transform();
		if (post) {
			tr = tr * t;
		} else {
			tr.basis = t.basis * tr.basis;
			tr.origin += t.origin;
		}

		Node3D *parent = sp->get_parent_node_3d();
		Transform3D local_tr = parent ? parent->get_global_transform().affine_inverse() * tr : tr;
		undo_redo->add_do_method(sp, "set_transform", local_tr);
		undo_redo->add_undo_method(sp, "set_transform", sp->get_transform());
	}
	undo_redo->commit_action();
}

void Node3DEditor::_menu_item_toggled(bool pressed, int p_option) {
	switch (p_option) {
		case MENU_TOOL_LOCAL_COORDS: {
			tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_pressed(pressed);
			update_transform_gizmo();
		} break;

		case MENU_TOOL_USE_SNAP: {
			tool_option_button[TOOL_OPT_USE_SNAP]->set_pressed(pressed);
			snap_enabled = pressed;
		} break;
	}
}

void Node3DEditor::_menu_gizmo_toggled(int p_option) {
	const int idx = gizmos_menu->get_item_index(p_option);
	gizmos_menu->toggle_item_multistate(idx);

	// Change icon
	const int state = gizmos_menu->get_item_state(idx);
	switch (state) {
		case EditorNode3DGizmoPlugin::VISIBLE:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_theme_icon(SNAME("visibility_visible")));
			break;
		case EditorNode3DGizmoPlugin::ON_TOP:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_theme_icon(SNAME("visibility_xray")));
			break;
		case EditorNode3DGizmoPlugin::HIDDEN:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_theme_icon(SNAME("visibility_hidden")));
			break;
	}

	gizmo_plugins_by_name.write[p_option]->set_state(state);

	update_all_gizmos();
}

void Node3DEditor::_menu_item_pressed(int p_option) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	switch (p_option) {
		case MENU_TOOL_SELECT:
		case MENU_TOOL_MOVE:
		case MENU_TOOL_ROTATE:
		case MENU_TOOL_SCALE:
		case MENU_TOOL_LIST_SELECT: {
			for (int i = 0; i < TOOL_MAX; i++) {
				tool_button[i]->set_pressed(i == p_option);
			}
			tool_mode = (ToolMode)p_option;
			update_transform_gizmo();

		} break;
		case MENU_TRANSFORM_CONFIGURE_SNAP: {
			snap_dialog->popup_centered(Size2(200, 180));
		} break;
		case MENU_TRANSFORM_DIALOG: {
			for (int i = 0; i < 3; i++) {
				xform_translate[i]->set_text("0");
				xform_rotate[i]->set_text("0");
				xform_scale[i]->set_text("1");
			}

			xform_dialog->popup_centered(Size2(320, 240) * EDSCALE);

		} break;
		case MENU_VIEW_USE_1_VIEWPORT: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_1_VIEWPORT);
			if (last_used_viewport > 0) {
				last_used_viewport = 0;
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_2_VIEWPORTS);
			if (last_used_viewport > 1) {
				last_used_viewport = 0;
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS_ALT: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_2_VIEWPORTS_ALT);
			if (last_used_viewport > 1) {
				last_used_viewport = 0;
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_3_VIEWPORTS);
			if (last_used_viewport > 2) {
				last_used_viewport = 0;
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS_ALT: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_3_VIEWPORTS_ALT);
			if (last_used_viewport > 2) {
				last_used_viewport = 0;
			}

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), true);

		} break;
		case MENU_VIEW_USE_4_VIEWPORTS: {
			viewport_base->set_view(Node3DEditorViewportContainer::VIEW_USE_4_VIEWPORTS);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_ORIGIN: {
			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			origin_enabled = !is_checked;
			RenderingServer::get_singleton()->instance_set_visible(origin_instance, origin_enabled);
			// Update the grid since its appearance depends on whether the origin is enabled
			_finish_grid();
			_init_grid();

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), origin_enabled);
		} break;
		case MENU_VIEW_GRID: {
			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));

			grid_enabled = !is_checked;

			for (int i = 0; i < 3; ++i) {
				if (grid_enable[i]) {
					grid_visible[i] = grid_enabled;
				}
			}
			_finish_grid();
			_init_grid();

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(p_option), grid_enabled);

		} break;
		case MENU_VIEW_CAMERA_SETTINGS: {
			settings_dialog->popup_centered(settings_vbc->get_combined_minimum_size() + Size2(50, 50));
		} break;
		case MENU_SNAP_TO_FLOOR: {
			snap_selected_nodes_to_floor();
		} break;
		case MENU_LOCK_SELECTED: {
			undo_redo->create_action(TTR("Lock Selected"));

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (Node *E : selection) {
				Node3D *spatial = Object::cast_to<Node3D>(E);
				if (!spatial || !spatial->is_inside_tree()) {
					continue;
				}

				if (spatial->get_viewport() != EditorNode::get_singleton()->get_scene_root()) {
					continue;
				}

				undo_redo->add_do_method(spatial, "set_meta", "_edit_lock_", true);
				undo_redo->add_undo_method(spatial, "remove_meta", "_edit_lock_");
				undo_redo->add_do_method(this, "emit_signal", "item_lock_status_changed");
				undo_redo->add_undo_method(this, "emit_signal", "item_lock_status_changed");
			}

			undo_redo->add_do_method(this, "_refresh_menu_icons");
			undo_redo->add_undo_method(this, "_refresh_menu_icons");
			undo_redo->commit_action();
		} break;
		case MENU_UNLOCK_SELECTED: {
			undo_redo->create_action(TTR("Unlock Selected"));

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (Node *E : selection) {
				Node3D *spatial = Object::cast_to<Node3D>(E);
				if (!spatial || !spatial->is_inside_tree()) {
					continue;
				}

				if (spatial->get_viewport() != EditorNode::get_singleton()->get_scene_root()) {
					continue;
				}

				undo_redo->add_do_method(spatial, "remove_meta", "_edit_lock_");
				undo_redo->add_undo_method(spatial, "set_meta", "_edit_lock_", true);
				undo_redo->add_do_method(this, "emit_signal", "item_lock_status_changed");
				undo_redo->add_undo_method(this, "emit_signal", "item_lock_status_changed");
			}

			undo_redo->add_do_method(this, "_refresh_menu_icons");
			undo_redo->add_undo_method(this, "_refresh_menu_icons");
			undo_redo->commit_action();
		} break;
		case MENU_GROUP_SELECTED: {
			undo_redo->create_action(TTR("Group Selected"));

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (Node *E : selection) {
				Node3D *spatial = Object::cast_to<Node3D>(E);
				if (!spatial || !spatial->is_inside_tree()) {
					continue;
				}

				if (spatial->get_viewport() != EditorNode::get_singleton()->get_scene_root()) {
					continue;
				}

				undo_redo->add_do_method(spatial, "set_meta", "_edit_group_", true);
				undo_redo->add_undo_method(spatial, "remove_meta", "_edit_group_");
				undo_redo->add_do_method(this, "emit_signal", "item_group_status_changed");
				undo_redo->add_undo_method(this, "emit_signal", "item_group_status_changed");
			}

			undo_redo->add_do_method(this, "_refresh_menu_icons");
			undo_redo->add_undo_method(this, "_refresh_menu_icons");
			undo_redo->commit_action();
		} break;
		case MENU_UNGROUP_SELECTED: {
			undo_redo->create_action(TTR("Ungroup Selected"));
			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (Node *E : selection) {
				Node3D *spatial = Object::cast_to<Node3D>(E);
				if (!spatial || !spatial->is_inside_tree()) {
					continue;
				}

				if (spatial->get_viewport() != EditorNode::get_singleton()->get_scene_root()) {
					continue;
				}

				undo_redo->add_do_method(spatial, "remove_meta", "_edit_group_");
				undo_redo->add_undo_method(spatial, "set_meta", "_edit_group_", true);
				undo_redo->add_do_method(this, "emit_signal", "item_group_status_changed");
				undo_redo->add_undo_method(this, "emit_signal", "item_group_status_changed");
			}

			undo_redo->add_do_method(this, "_refresh_menu_icons");
			undo_redo->add_undo_method(this, "_refresh_menu_icons");
			undo_redo->commit_action();
		} break;
		case MENU_RULER: {
			for (int i = 0; i < TOOL_MAX; i++) {
				tool_button[i]->set_pressed(i == p_option);
			}
			tool_button[TOOL_RULER]->set_pressed(true);
			tool_mode = ToolMode::TOOL_RULER;
			update_transform_gizmo();
		} break;
	}
}

void Node3DEditor::_init_indicators() {
	{
		origin_enabled = true;
		grid_enabled = true;

		Ref<Shader> origin_shader = memnew(Shader);
		origin_shader->set_code(R"(
// 3D editor origin line shader.

shader_type spatial;
render_mode blend_mix, cull_disabled, unshaded, fog_disabled;

void vertex() {
	vec3 point_a = MODEL_MATRIX[3].xyz;
	// Encoded in scale.
	vec3 point_b = vec3(MODEL_MATRIX[0].x, MODEL_MATRIX[1].y, MODEL_MATRIX[2].z);

	// Points are already in world space, so no need for MODEL_MATRIX anymore.
	vec4 clip_a = PROJECTION_MATRIX * (VIEW_MATRIX * vec4(point_a, 1.0));
	vec4 clip_b = PROJECTION_MATRIX * (VIEW_MATRIX * vec4(point_b, 1.0));

	vec2 screen_a = VIEWPORT_SIZE * (0.5 * clip_a.xy / clip_a.w + 0.5);
	vec2 screen_b = VIEWPORT_SIZE * (0.5 * clip_b.xy / clip_b.w + 0.5);

	vec2 x_basis = normalize(screen_b - screen_a);
	vec2 y_basis = vec2(-x_basis.y, x_basis.x);

	float width = 3.0;
	vec2 screen_point_a = screen_a + width * (VERTEX.x * x_basis + VERTEX.y * y_basis);
	vec2 screen_point_b = screen_b + width * (VERTEX.x * x_basis + VERTEX.y * y_basis);
	vec2 screen_point_final = mix(screen_point_a, screen_point_b, VERTEX.z);

	vec4 clip_final = mix(clip_a, clip_b, VERTEX.z);

	POSITION = vec4(clip_final.w * ((2.0 * screen_point_final) / VIEWPORT_SIZE - 1.0), clip_final.z, clip_final.w);
	UV = VERTEX.yz * clip_final.w;

	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix(pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb * (1.0 / 12.92), lessThan(COLOR.rgb, vec3(0.04045)));
	}
}

void fragment() {
	// Multiply by 0.5 since UV is actually UV is [-1, 1].
	float line_width = fwidth(UV.x * 0.5);
	float line_uv = abs(UV.x * 0.5);
	float line = smoothstep(line_width * 1.0, line_width * 0.25, line_uv);

	ALBEDO = COLOR.rgb;
	ALPHA *= COLOR.a * line;
}
)");

		origin_mat.instantiate();
		origin_mat->set_shader(origin_shader);

		Vector<Vector3> origin_points;
		origin_points.resize(6);

		origin_points.set(0, Vector3(0.0, -0.5, 0.0));
		origin_points.set(1, Vector3(0.0, -0.5, 1.0));
		origin_points.set(2, Vector3(0.0, 0.5, 1.0));

		origin_points.set(3, Vector3(0.0, -0.5, 0.0));
		origin_points.set(4, Vector3(0.0, 0.5, 1.0));
		origin_points.set(5, Vector3(0.0, 0.5, 0.0));

		Array d;
		d.resize(RS::ARRAY_MAX);
		d[RenderingServer::ARRAY_VERTEX] = origin_points;

		origin_mesh = RenderingServer::get_singleton()->mesh_create();

		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(origin_mesh, RenderingServer::PRIMITIVE_TRIANGLES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(origin_mesh, 0, origin_mat->get_rid());

		origin_multimesh = RenderingServer::get_singleton()->multimesh_create();
		RenderingServer::get_singleton()->multimesh_set_mesh(origin_multimesh, origin_mesh);
		RenderingServer::get_singleton()->multimesh_allocate_data(origin_multimesh, 12, RS::MultimeshTransformFormat::MULTIMESH_TRANSFORM_3D, true, false);
		RenderingServer::get_singleton()->multimesh_set_visible_instances(origin_multimesh, -1);

		LocalVector<float> distances;
		distances.resize(5);
		distances[0] = -1000000.0;
		distances[1] = -1000.0;
		distances[2] = 0.0;
		distances[3] = 1000.0;
		distances[4] = 1000000.0;

		for (int i = 0; i < 3; i++) {
			Color origin_color;
			switch (i) {
				case 0:
					origin_color = get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor));
					break;
				case 1:
					origin_color = get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor));
					break;
				case 2:
					origin_color = get_theme_color(SNAME("axis_z_color"), EditorStringName(Editor));
					break;
				default:
					origin_color = Color();
					break;
			}

			Vector3 axis;
			axis[i] = 1;

			for (int j = 0; j < 4; j++) {
				Transform3D t = Transform3D();
				if (distances[j] > 0.0) {
					t = t.scaled(axis * distances[j + 1]);
					t = t.translated(axis * distances[j]);
				} else {
					t = t.scaled(axis * distances[j]);
					t = t.translated(axis * distances[j + 1]);
				}
				RenderingServer::get_singleton()->multimesh_instance_set_transform(origin_multimesh, i * 4 + j, t);
				RenderingServer::get_singleton()->multimesh_instance_set_color(origin_multimesh, i * 4 + j, origin_color);
			}
		}

		origin_instance = RenderingServer::get_singleton()->instance_create2(origin_multimesh, get_tree()->get_root()->get_world_3d()->get_scenario());
		RS::get_singleton()->instance_set_layer_mask(origin_instance, 1 << Node3DEditorViewport::GIZMO_GRID_LAYER);
		RS::get_singleton()->instance_geometry_set_flag(origin_instance, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(origin_instance, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

		RenderingServer::get_singleton()->instance_geometry_set_cast_shadows_setting(origin_instance, RS::SHADOW_CASTING_SETTING_OFF);

		Ref<Shader> grid_shader = memnew(Shader);
		grid_shader->set_code(R"(
// 3D editor grid shader.

shader_type spatial;

render_mode unshaded, fog_disabled;

uniform bool orthogonal;
uniform float grid_size;

void vertex() {
	// From FLAG_SRGB_VERTEX_COLOR.
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix(pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb * (1.0 / 12.92), lessThan(COLOR.rgb, vec3(0.04045)));
	}
}

void fragment() {
	ALBEDO = COLOR.rgb;
	vec3 dir = orthogonal ? -vec3(0, 0, 1) : VIEW;
	float angle_fade = abs(dot(dir, NORMAL));
	angle_fade = smoothstep(0.05, 0.2, angle_fade);

	vec3 world_pos = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 world_normal = (INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz;
	vec3 camera_world_pos = INV_VIEW_MATRIX[3].xyz;
	vec3 camera_world_pos_on_plane = camera_world_pos * (1.0 - world_normal);
	float dist_fade = 1.0 - (distance(world_pos, camera_world_pos_on_plane) / grid_size);
	dist_fade = smoothstep(0.02, 0.3, dist_fade);

	ALPHA = COLOR.a * dist_fade * angle_fade;
}
)");

		for (int i = 0; i < 3; i++) {
			grid_mat[i].instantiate();
			grid_mat[i]->set_shader(grid_shader);
		}

		grid_enable[0] = EDITOR_GET("editors/3d/grid_xy_plane");
		grid_enable[1] = EDITOR_GET("editors/3d/grid_yz_plane");
		grid_enable[2] = EDITOR_GET("editors/3d/grid_xz_plane");
		grid_visible[0] = grid_enable[0];
		grid_visible[1] = grid_enable[1];
		grid_visible[2] = grid_enable[2];

		_init_grid();
	}

	{
		//move gizmo

		// Inverted zxy.
		Vector3 ivec = Vector3(0, 0, -1);
		Vector3 nivec = Vector3(-1, -1, 0);
		Vector3 ivec2 = Vector3(-1, 0, 0);
		Vector3 ivec3 = Vector3(0, -1, 0);

		for (int i = 0; i < 3; i++) {
			Color col;
			switch (i) {
				case 0:
					col = get_theme_color(SNAME("axis_x_color"), EditorStringName(Editor));
					break;
				case 1:
					col = get_theme_color(SNAME("axis_y_color"), EditorStringName(Editor));
					break;
				case 2:
					col = get_theme_color(SNAME("axis_z_color"), EditorStringName(Editor));
					break;
				default:
					col = Color();
					break;
			}

			col.a = EDITOR_GET("editors/3d/manipulator_gizmo_opacity");

			move_gizmo[i].instantiate();
			move_plane_gizmo[i].instantiate();
			rotate_gizmo[i].instantiate();
			scale_gizmo[i].instantiate();
			scale_plane_gizmo[i].instantiate();
			axis_gizmo[i].instantiate();

			Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
			mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
			mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
			mat->set_on_top_of_alpha();
			mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
			mat->set_albedo(col);
			gizmo_color[i] = mat;

			Ref<StandardMaterial3D> mat_hl = mat->duplicate();
			const Color albedo = col.from_hsv(col.get_h(), 0.25, 1.0, 1);
			mat_hl->set_albedo(albedo);
			gizmo_color_hl[i] = mat_hl;

			//translate
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				// Arrow profile
				const int arrow_points = 5;
				Vector3 arrow[5] = {
					nivec * 0.0 + ivec * 0.0,
					nivec * 0.01 + ivec * 0.0,
					nivec * 0.01 + ivec * GIZMO_ARROW_OFFSET,
					nivec * 0.065 + ivec * GIZMO_ARROW_OFFSET,
					nivec * 0.0 + ivec * (GIZMO_ARROW_OFFSET + GIZMO_ARROW_SIZE),
				};

				int arrow_sides = 16;

				const real_t arrow_sides_step = Math_TAU / arrow_sides;
				for (int k = 0; k < arrow_sides; k++) {
					Basis ma(ivec, k * arrow_sides_step);
					Basis mb(ivec, (k + 1) * arrow_sides_step);

					for (int j = 0; j < arrow_points - 1; j++) {
						Vector3 points[4] = {
							ma.xform(arrow[j]),
							mb.xform(arrow[j]),
							mb.xform(arrow[j + 1]),
							ma.xform(arrow[j + 1]),
						};
						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[1]);
						surftool->add_vertex(points[2]);

						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[2]);
						surftool->add_vertex(points[3]);
					}
				}

				surftool->set_material(mat);
				surftool->commit(move_gizmo[i]);
			}

			// Plane Translation
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				Vector3 vec = ivec2 - ivec3;
				Vector3 plane[4] = {
					vec * GIZMO_PLANE_DST,
					vec * GIZMO_PLANE_DST + ivec2 * GIZMO_PLANE_SIZE,
					vec * (GIZMO_PLANE_DST + GIZMO_PLANE_SIZE),
					vec * GIZMO_PLANE_DST - ivec3 * GIZMO_PLANE_SIZE
				};

				Basis ma(ivec, Math_PI / 2);

				Vector3 points[4] = {
					ma.xform(plane[0]),
					ma.xform(plane[1]),
					ma.xform(plane[2]),
					ma.xform(plane[3]),
				};
				surftool->add_vertex(points[0]);
				surftool->add_vertex(points[1]);
				surftool->add_vertex(points[2]);

				surftool->add_vertex(points[0]);
				surftool->add_vertex(points[2]);
				surftool->add_vertex(points[3]);

				Ref<StandardMaterial3D> plane_mat = memnew(StandardMaterial3D);
				plane_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
				plane_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
				plane_mat->set_on_top_of_alpha();
				plane_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
				plane_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
				plane_mat->set_albedo(col);
				plane_gizmo_color[i] = plane_mat; // needed, so we can draw planes from both sides
				surftool->set_material(plane_mat);
				surftool->commit(move_plane_gizmo[i]);

				Ref<StandardMaterial3D> plane_mat_hl = plane_mat->duplicate();
				plane_mat_hl->set_albedo(albedo);
				plane_gizmo_color_hl[i] = plane_mat_hl; // needed, so we can draw planes from both sides
			}

			// Rotate
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				int n = 128; // number of circle segments
				int m = 3; // number of thickness segments

				real_t step = Math_TAU / n;
				for (int j = 0; j < n; ++j) {
					Basis basis = Basis(ivec, j * step);

					Vector3 vertex = basis.xform(ivec2 * GIZMO_CIRCLE_SIZE);

					for (int k = 0; k < m; ++k) {
						Vector2 ofs = Vector2(Math::cos((Math_TAU * k) / m), Math::sin((Math_TAU * k) / m));
						Vector3 normal = ivec * ofs.x + ivec2 * ofs.y;

						surftool->set_normal(basis.xform(normal));
						surftool->add_vertex(vertex);
					}
				}

				for (int j = 0; j < n; ++j) {
					for (int k = 0; k < m; ++k) {
						int current_ring = j * m;
						int next_ring = ((j + 1) % n) * m;
						int current_segment = k;
						int next_segment = (k + 1) % m;

						surftool->add_index(current_ring + next_segment);
						surftool->add_index(current_ring + current_segment);
						surftool->add_index(next_ring + current_segment);

						surftool->add_index(next_ring + current_segment);
						surftool->add_index(next_ring + next_segment);
						surftool->add_index(current_ring + next_segment);
					}
				}

				Ref<Shader> rotate_shader = memnew(Shader);

				rotate_shader->set_code(R"(
// 3D editor rotation manipulator gizmo shader.

shader_type spatial;

render_mode unshaded, depth_test_disabled, fog_disabled;

uniform vec4 albedo;

mat3 orthonormalize(mat3 m) {
	vec3 x = normalize(m[0]);
	vec3 y = normalize(m[1] - x * dot(x, m[1]));
	vec3 z = m[2] - x * dot(x, m[2]);
	z = normalize(z - y * (dot(y, m[2])));
	return mat3(x,y,z);
}

void vertex() {
	mat3 mv = orthonormalize(mat3(MODELVIEW_MATRIX));
	vec3 n = mv * VERTEX;
	float orientation = dot(vec3(0.0, 0.0, -1.0), n);
	if (orientation <= 0.005) {
		VERTEX += NORMAL * 0.02;
	}
}

void fragment() {
	ALBEDO = albedo.rgb;
	ALPHA = albedo.a;
}
)");

				Ref<ShaderMaterial> rotate_mat = memnew(ShaderMaterial);
				rotate_mat->set_render_priority(Material::RENDER_PRIORITY_MAX);
				rotate_mat->set_shader(rotate_shader);
				rotate_mat->set_shader_parameter("albedo", col);
				rotate_gizmo_color[i] = rotate_mat;

				Array arrays = surftool->commit_to_arrays();
				rotate_gizmo[i]->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
				rotate_gizmo[i]->surface_set_material(0, rotate_mat);

				Ref<ShaderMaterial> rotate_mat_hl = rotate_mat->duplicate();
				rotate_mat_hl->set_shader_parameter("albedo", albedo);
				rotate_gizmo_color_hl[i] = rotate_mat_hl;

				if (i == 2) { // Rotation white outline
					Ref<ShaderMaterial> border_mat = rotate_mat->duplicate();

					Ref<Shader> border_shader = memnew(Shader);
					border_shader->set_code(R"(
// 3D editor rotation manipulator gizmo shader (white outline).

shader_type spatial;

render_mode unshaded, depth_test_disabled, fog_disabled;

uniform vec4 albedo;

mat3 orthonormalize(mat3 m) {
	vec3 x = normalize(m[0]);
	vec3 y = normalize(m[1] - x * dot(x, m[1]));
	vec3 z = m[2] - x * dot(x, m[2]);
	z = normalize(z - y * (dot(y, m[2])));
	return mat3(x, y, z);
}

void vertex() {
	mat3 mv = orthonormalize(mat3(MODELVIEW_MATRIX));
	mv = inverse(mv);
	VERTEX += NORMAL * 0.008;
	vec3 camera_dir_local = mv * vec3(0.0, 0.0, 1.0);
	vec3 camera_up_local = mv * vec3(0.0, 1.0, 0.0);
	mat3 rotation_matrix = mat3(cross(camera_dir_local, camera_up_local), camera_up_local, camera_dir_local);
	VERTEX = rotation_matrix * VERTEX;
}

void fragment() {
	ALBEDO = albedo.rgb;
	ALPHA = albedo.a;
}
)");

					border_mat->set_shader(border_shader);
					border_mat->set_shader_parameter("albedo", Color(0.75, 0.75, 0.75, col.a / 3.0));

					rotate_gizmo[3].instantiate();
					rotate_gizmo[3]->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
					rotate_gizmo[3]->surface_set_material(0, border_mat);
				}
			}

			// Scale
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				// Cube arrow profile
				const int arrow_points = 6;
				Vector3 arrow[6] = {
					nivec * 0.0 + ivec * 0.0,
					nivec * 0.01 + ivec * 0.0,
					nivec * 0.01 + ivec * 1.0 * GIZMO_SCALE_OFFSET,
					nivec * 0.07 + ivec * 1.0 * GIZMO_SCALE_OFFSET,
					nivec * 0.07 + ivec * 1.11 * GIZMO_SCALE_OFFSET,
					nivec * 0.0 + ivec * 1.11 * GIZMO_SCALE_OFFSET,
				};

				int arrow_sides = 4;

				const real_t arrow_sides_step = Math_TAU / arrow_sides;
				for (int k = 0; k < 4; k++) {
					Basis ma(ivec, k * arrow_sides_step);
					Basis mb(ivec, (k + 1) * arrow_sides_step);

					for (int j = 0; j < arrow_points - 1; j++) {
						Vector3 points[4] = {
							ma.xform(arrow[j]),
							mb.xform(arrow[j]),
							mb.xform(arrow[j + 1]),
							ma.xform(arrow[j + 1]),
						};
						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[1]);
						surftool->add_vertex(points[2]);

						surftool->add_vertex(points[0]);
						surftool->add_vertex(points[2]);
						surftool->add_vertex(points[3]);
					}
				}

				surftool->set_material(mat);
				surftool->commit(scale_gizmo[i]);
			}

			// Plane Scale
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				Vector3 vec = ivec2 - ivec3;
				Vector3 plane[4] = {
					vec * GIZMO_PLANE_DST,
					vec * GIZMO_PLANE_DST + ivec2 * GIZMO_PLANE_SIZE,
					vec * (GIZMO_PLANE_DST + GIZMO_PLANE_SIZE),
					vec * GIZMO_PLANE_DST - ivec3 * GIZMO_PLANE_SIZE
				};

				Basis ma(ivec, Math_PI / 2);

				Vector3 points[4] = {
					ma.xform(plane[0]),
					ma.xform(plane[1]),
					ma.xform(plane[2]),
					ma.xform(plane[3]),
				};
				surftool->add_vertex(points[0]);
				surftool->add_vertex(points[1]);
				surftool->add_vertex(points[2]);

				surftool->add_vertex(points[0]);
				surftool->add_vertex(points[2]);
				surftool->add_vertex(points[3]);

				Ref<StandardMaterial3D> plane_mat = memnew(StandardMaterial3D);
				plane_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
				plane_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
				plane_mat->set_on_top_of_alpha();
				plane_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
				plane_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
				plane_mat->set_albedo(col);
				plane_gizmo_color[i] = plane_mat; // needed, so we can draw planes from both sides
				surftool->set_material(plane_mat);
				surftool->commit(scale_plane_gizmo[i]);

				Ref<StandardMaterial3D> plane_mat_hl = plane_mat->duplicate();
				plane_mat_hl->set_albedo(col.from_hsv(col.get_h(), 0.25, 1.0, 1));
				plane_gizmo_color_hl[i] = plane_mat_hl; // needed, so we can draw planes from both sides
			}

			// Lines to visualize transforms locked to an axis/plane
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_LINE_STRIP);

				Vector3 vec;
				vec[i] = 1;

				// line extending through infinity(ish)
				surftool->add_vertex(vec * -1048576);
				surftool->add_vertex(Vector3());
				surftool->add_vertex(vec * 1048576);
				surftool->set_material(mat_hl);
				surftool->commit(axis_gizmo[i]);
			}
		}
	}

	_generate_selection_boxes();
}

void Node3DEditor::_update_gizmos_menu() {
	gizmos_menu->clear();

	for (int i = 0; i < gizmo_plugins_by_name.size(); ++i) {
		if (!gizmo_plugins_by_name[i]->can_be_hidden()) {
			continue;
		}
		String plugin_name = gizmo_plugins_by_name[i]->get_gizmo_name();
		const int plugin_state = gizmo_plugins_by_name[i]->get_state();
		gizmos_menu->add_multistate_item(plugin_name, 3, plugin_state, i);
		const int idx = gizmos_menu->get_item_index(i);
		gizmos_menu->set_item_tooltip(
				idx,
				TTR("Click to toggle between visibility states.\n\nOpen eye: Gizmo is visible.\nClosed eye: Gizmo is hidden.\nHalf-open eye: Gizmo is also visible through opaque surfaces (\"x-ray\")."));
		switch (plugin_state) {
			case EditorNode3DGizmoPlugin::VISIBLE:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_visible")));
				break;
			case EditorNode3DGizmoPlugin::ON_TOP:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_xray")));
				break;
			case EditorNode3DGizmoPlugin::HIDDEN:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_hidden")));
				break;
		}
	}
}

void Node3DEditor::_update_gizmos_menu_theme() {
	for (int i = 0; i < gizmo_plugins_by_name.size(); ++i) {
		if (!gizmo_plugins_by_name[i]->can_be_hidden()) {
			continue;
		}
		const int plugin_state = gizmo_plugins_by_name[i]->get_state();
		const int idx = gizmos_menu->get_item_index(i);
		switch (plugin_state) {
			case EditorNode3DGizmoPlugin::VISIBLE:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_visible")));
				break;
			case EditorNode3DGizmoPlugin::ON_TOP:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_xray")));
				break;
			case EditorNode3DGizmoPlugin::HIDDEN:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_theme_icon(SNAME("visibility_hidden")));
				break;
		}
	}
}

void Node3DEditor::_init_grid() {
	if (!grid_enabled) {
		return;
	}
	Camera3D *camera = get_editor_viewport(0)->camera;
	Vector3 camera_position = camera->get_position();
	if (camera_position == Vector3()) {
		return; // Camera3D is invalid, don't draw the grid.
	}

	bool orthogonal = camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL;

	static LocalVector<Color> grid_colors[3];
	static LocalVector<Vector3> grid_points[3];
	static LocalVector<Vector3> grid_normals[3];

	for (uint32_t n = 0; n < 3; n++) {
		grid_colors[n].clear();
		grid_points[n].clear();
		grid_normals[n].clear();
	}

	Color primary_grid_color = EDITOR_GET("editors/3d/primary_grid_color");
	Color secondary_grid_color = EDITOR_GET("editors/3d/secondary_grid_color");
	int grid_size = EDITOR_GET("editors/3d/grid_size");
	int primary_grid_steps = EDITOR_GET("editors/3d/primary_grid_steps");

	// Which grid planes are enabled? Which should we generate?
	grid_enable[0] = grid_visible[0] = orthogonal || EDITOR_GET("editors/3d/grid_xy_plane");
	grid_enable[1] = grid_visible[1] = orthogonal || EDITOR_GET("editors/3d/grid_yz_plane");
	grid_enable[2] = grid_visible[2] = orthogonal || EDITOR_GET("editors/3d/grid_xz_plane");

	// Offsets division_level for bigger or smaller grids.
	// Default value is -0.2. -1.0 gives Blender-like behavior, 0.5 gives huge grids.
	real_t division_level_bias = EDITOR_GET("editors/3d/grid_division_level_bias");
	// Default largest grid size is 8^2 (default value is 2) when primary_grid_steps is 8 (64m apart, so primary grid lines are 512m apart).
	int division_level_max = EDITOR_GET("editors/3d/grid_division_level_max");
	// Default smallest grid size is 8^0 (default value is 0) when primary_grid_steps is 8.
	int division_level_min = EDITOR_GET("editors/3d/grid_division_level_min");
	ERR_FAIL_COND_MSG(division_level_max < division_level_min, "The 3D grid's maximum division level cannot be lower than its minimum division level.");

	if (primary_grid_steps != 10) { // Log10 of 10 is 1.
		// Change of base rule, divide by ln(10).
		real_t div = Math::log((real_t)primary_grid_steps) / (real_t)2.302585092994045901094;
		// Truncation (towards zero) is intentional.
		division_level_max = (int)(division_level_max / div);
		division_level_min = (int)(division_level_min / div);
	}

	for (int a = 0; a < 3; a++) {
		if (!grid_enable[a]) {
			continue; // If this grid plane is disabled, skip generation.
		}
		int b = (a + 1) % 3;
		int c = (a + 2) % 3;

		Vector3 normal;
		normal[c] = 1.0;

		real_t camera_distance = Math::abs(camera_position[c]);

		if (orthogonal) {
			camera_distance = camera->get_size() / 2.0;
			Vector3 camera_direction = -camera->get_global_transform().get_basis().get_column(2);
			Plane grid_plane = Plane(normal);
			Vector3 intersection;
			if (grid_plane.intersects_ray(camera_position, camera_direction, &intersection)) {
				camera_position = intersection;
			}
		}

		real_t division_level = Math::log(Math::abs(camera_distance)) / Math::log((double)primary_grid_steps) + division_level_bias;

		real_t clamped_division_level = CLAMP(division_level, division_level_min, division_level_max);
		real_t division_level_floored = Math::floor(clamped_division_level);
		real_t division_level_decimals = clamped_division_level - division_level_floored;

		real_t small_step_size = Math::pow(primary_grid_steps, division_level_floored);
		real_t large_step_size = small_step_size * primary_grid_steps;
		real_t center_a = large_step_size * (int)(camera_position[a] / large_step_size);
		real_t center_b = large_step_size * (int)(camera_position[b] / large_step_size);

		real_t bgn_a = center_a - grid_size * small_step_size;
		real_t end_a = center_a + grid_size * small_step_size;
		real_t bgn_b = center_b - grid_size * small_step_size;
		real_t end_b = center_b + grid_size * small_step_size;

		real_t fade_size = Math::pow(primary_grid_steps, division_level - 1.0);
		real_t min_fade_size = Math::pow(primary_grid_steps, float(division_level_min));
		real_t max_fade_size = Math::pow(primary_grid_steps, float(division_level_max));
		fade_size = CLAMP(fade_size, min_fade_size, max_fade_size);

		real_t grid_fade_size = (grid_size - primary_grid_steps) * fade_size;
		grid_mat[c]->set_shader_parameter("grid_size", grid_fade_size);
		grid_mat[c]->set_shader_parameter("orthogonal", orthogonal);

		LocalVector<Vector3> &ref_grid = grid_points[c];
		LocalVector<Vector3> &ref_grid_normals = grid_normals[c];
		LocalVector<Color> &ref_grid_colors = grid_colors[c];

		// Count our elements same as code below it.
		int expected_size = 0;
		for (int i = -grid_size; i <= grid_size; i++) {
			const real_t position_a = center_a + i * small_step_size;
			const real_t position_b = center_b + i * small_step_size;

			// Don't draw lines over the origin if it's enabled.
			if (!(origin_enabled && Math::is_zero_approx(position_a))) {
				expected_size += 2;
			}

			if (!(origin_enabled && Math::is_zero_approx(position_b))) {
				expected_size += 2;
			}
		}

		int idx = 0;
		ref_grid.resize(expected_size);
		ref_grid_normals.resize(expected_size);
		ref_grid_colors.resize(expected_size);

		// In each iteration of this loop, draw one line in each direction (so two lines per loop, in each if statement).
		for (int i = -grid_size; i <= grid_size; i++) {
			Color line_color;
			// Is this a primary line? Set the appropriate color.
			if (i % primary_grid_steps == 0) {
				line_color = primary_grid_color.lerp(secondary_grid_color, division_level_decimals);
			} else {
				line_color = secondary_grid_color;
				line_color.a = line_color.a * (1 - division_level_decimals);
			}

			real_t position_a = center_a + i * small_step_size;
			real_t position_b = center_b + i * small_step_size;

			// Don't draw lines over the origin if it's enabled.
			if (!(origin_enabled && Math::is_zero_approx(position_a))) {
				Vector3 line_bgn;
				Vector3 line_end;
				line_bgn[a] = position_a;
				line_end[a] = position_a;
				line_bgn[b] = bgn_b;
				line_end[b] = end_b;
				ref_grid[idx] = line_bgn;
				ref_grid[idx + 1] = line_end;
				ref_grid_colors[idx] = line_color;
				ref_grid_colors[idx + 1] = line_color;
				ref_grid_normals[idx] = normal;
				ref_grid_normals[idx + 1] = normal;
				idx += 2;
			}

			if (!(origin_enabled && Math::is_zero_approx(position_b))) {
				Vector3 line_bgn;
				Vector3 line_end;
				line_bgn[b] = position_b;
				line_end[b] = position_b;
				line_bgn[a] = bgn_a;
				line_end[a] = end_a;
				ref_grid[idx] = line_bgn;
				ref_grid[idx + 1] = line_end;
				ref_grid_colors[idx] = line_color;
				ref_grid_colors[idx + 1] = line_color;
				ref_grid_normals[idx] = normal;
				ref_grid_normals[idx + 1] = normal;
				idx += 2;
			}
		}

		// Create a mesh from the pushed vector points and colors.
		grid[c] = RenderingServer::get_singleton()->mesh_create();
		Array d;
		d.resize(RS::ARRAY_MAX);
		d[RenderingServer::ARRAY_VERTEX] = (Vector<Vector3>)grid_points[c];
		d[RenderingServer::ARRAY_COLOR] = (Vector<Color>)grid_colors[c];
		d[RenderingServer::ARRAY_NORMAL] = (Vector<Vector3>)grid_normals[c];
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(grid[c], RenderingServer::PRIMITIVE_LINES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(grid[c], 0, grid_mat[c]->get_rid());
		grid_instance[c] = RenderingServer::get_singleton()->instance_create2(grid[c], get_tree()->get_root()->get_world_3d()->get_scenario());

		// Yes, the end of this line is supposed to be a.
		RenderingServer::get_singleton()->instance_set_visible(grid_instance[c], grid_visible[a]);
		RenderingServer::get_singleton()->instance_geometry_set_cast_shadows_setting(grid_instance[c], RS::SHADOW_CASTING_SETTING_OFF);
		RS::get_singleton()->instance_set_layer_mask(grid_instance[c], 1 << Node3DEditorViewport::GIZMO_GRID_LAYER);
		RS::get_singleton()->instance_geometry_set_flag(grid_instance[c], RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
		RS::get_singleton()->instance_geometry_set_flag(grid_instance[c], RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
	}
}

void Node3DEditor::_finish_indicators() {
	RenderingServer::get_singleton()->free(origin_instance);
	RenderingServer::get_singleton()->free(origin_multimesh);
	RenderingServer::get_singleton()->free(origin_mesh);

	_finish_grid();
}

void Node3DEditor::_finish_grid() {
	for (int i = 0; i < 3; i++) {
		RenderingServer::get_singleton()->free(grid_instance[i]);
		RenderingServer::get_singleton()->free(grid[i]);
	}
}

void Node3DEditor::update_grid() {
	const Camera3D::ProjectionType current_projection = viewports[0]->camera->get_projection();

	if (current_projection != grid_camera_last_update_perspective) {
		grid_init_draw = false; // redraw
		grid_camera_last_update_perspective = current_projection;
	}

	// Gets a orthogonal or perspective position correctly (for the grid comparison)
	const Vector3 camera_position = get_editor_viewport(0)->camera->get_position();

	if (!grid_init_draw || grid_camera_last_update_position.distance_squared_to(camera_position) >= 100.0f) {
		_finish_grid();
		_init_grid();
		grid_init_draw = true;
		grid_camera_last_update_position = camera_position;
	}
}

void Node3DEditor::_selection_changed() {
	_refresh_menu_icons();
	if (selected && editor_selection->get_selected_node_list().size() != 1) {
		Vector<Ref<Node3DGizmo>> gizmos = selected->get_gizmos();
		for (int i = 0; i < gizmos.size(); i++) {
			Ref<EditorNode3DGizmo> seg = gizmos[i];
			if (seg.is_null()) {
				continue;
			}
			seg->set_selected(false);
		}

		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected);
		if (se) {
			se->gizmo.unref();
			se->subgizmos.clear();
		}
		selected->update_gizmos();
		selected = nullptr;
	}
	update_transform_gizmo();
}

void Node3DEditor::_refresh_menu_icons() {
	bool all_locked = true;
	bool all_grouped = true;
	bool has_node3d_item = false;

	List<Node *> &selection = editor_selection->get_selected_node_list();

	if (selection.is_empty()) {
		all_locked = false;
		all_grouped = false;
	} else {
		for (Node *E : selection) {
			Node3D *node = Object::cast_to<Node3D>(E);
			if (node) {
				if (all_locked && !node->has_meta("_edit_lock_")) {
					all_locked = false;
				}
				if (all_grouped && !node->has_meta("_edit_group_")) {
					all_grouped = false;
				}
				has_node3d_item = true;
			}
			if (!all_locked && !all_grouped) {
				break;
			}
		}
	}

	all_locked = all_locked && has_node3d_item;
	all_grouped = all_grouped && has_node3d_item;

	tool_button[TOOL_LOCK_SELECTED]->set_visible(!all_locked);
	tool_button[TOOL_LOCK_SELECTED]->set_disabled(!has_node3d_item);
	tool_button[TOOL_UNLOCK_SELECTED]->set_visible(all_locked);
	tool_button[TOOL_UNLOCK_SELECTED]->set_disabled(!has_node3d_item);

	tool_button[TOOL_GROUP_SELECTED]->set_visible(!all_grouped);
	tool_button[TOOL_GROUP_SELECTED]->set_disabled(!has_node3d_item);
	tool_button[TOOL_UNGROUP_SELECTED]->set_visible(all_grouped);
	tool_button[TOOL_UNGROUP_SELECTED]->set_disabled(!has_node3d_item);
}

template <typename T>
HashSet<T *> _get_child_nodes(Node *parent_node) {
	HashSet<T *> nodes = HashSet<T *>();
	T *node = Node::cast_to<T>(parent_node);
	if (node) {
		nodes.insert(node);
	}

	for (int i = 0; i < parent_node->get_child_count(); i++) {
		Node *child_node = parent_node->get_child(i);
		HashSet<T *> child_nodes = _get_child_nodes<T>(child_node);
		for (T *I : child_nodes) {
			nodes.insert(I);
		}
	}

	return nodes;
}

HashSet<RID> _get_physics_bodies_rid(Node *node) {
	HashSet<RID> rids = HashSet<RID>();
	PhysicsBody3D *pb = Node::cast_to<PhysicsBody3D>(node);
	if (pb) {
		rids.insert(pb->get_rid());
	}
	HashSet<PhysicsBody3D *> child_nodes = _get_child_nodes<PhysicsBody3D>(node);
	for (const PhysicsBody3D *I : child_nodes) {
		rids.insert(I->get_rid());
	}

	return rids;
}

void Node3DEditor::snap_selected_nodes_to_floor() {
	do_snap_selected_nodes_to_floor = true;
}

void Node3DEditor::_snap_selected_nodes_to_floor() {
	const List<Node *> &selection = editor_selection->get_selected_node_list();
	Dictionary snap_data;

	for (Node *E : selection) {
		Node3D *sp = Object::cast_to<Node3D>(E);
		if (sp) {
			Vector3 from;
			Vector3 position_offset;

			// Priorities for snapping to floor are CollisionShapes, VisualInstances and then origin
			HashSet<VisualInstance3D *> vi = _get_child_nodes<VisualInstance3D>(sp);
			HashSet<CollisionShape3D *> cs = _get_child_nodes<CollisionShape3D>(sp);
			bool found_valid_shape = false;

			if (cs.size()) {
				AABB aabb;
				HashSet<CollisionShape3D *>::Iterator I = cs.begin();
				if ((*I)->get_shape().is_valid()) {
					CollisionShape3D *collision_shape = *cs.begin();
					aabb = collision_shape->get_global_transform().xform(collision_shape->get_shape()->get_debug_mesh()->get_aabb());
					found_valid_shape = true;
				}

				for (++I; I; ++I) {
					CollisionShape3D *col_shape = *I;
					if (col_shape->get_shape().is_valid()) {
						aabb.merge_with(col_shape->get_global_transform().xform(col_shape->get_shape()->get_debug_mesh()->get_aabb()));
						found_valid_shape = true;
					}
				}
				if (found_valid_shape) {
					Vector3 size = aabb.size * Vector3(0.5, 0.0, 0.5);
					from = aabb.position + size;
					position_offset.y = from.y - sp->get_global_transform().origin.y;
				}
			}
			if (!found_valid_shape && vi.size()) {
				VisualInstance3D *begin = *vi.begin();
				AABB aabb = begin->get_global_transform().xform(begin->get_aabb());
				for (const VisualInstance3D *I : vi) {
					aabb.merge_with(I->get_global_transform().xform(I->get_aabb()));
				}
				Vector3 size = aabb.size * Vector3(0.5, 0.0, 0.5);
				from = aabb.position + size;
				position_offset.y = from.y - sp->get_global_transform().origin.y;
			} else if (!found_valid_shape) {
				from = sp->get_global_transform().origin;
			}

			// We add a bit of margin to the from position to avoid it from snapping
			// when the spatial is already on a floor and there's another floor under
			// it
			from = from + Vector3(0.0, 1, 0.0);

			Dictionary d;

			d["from"] = from;
			d["position_offset"] = position_offset;
			snap_data[sp] = d;
		}
	}

	PhysicsDirectSpaceState3D *ss = get_tree()->get_root()->get_world_3d()->get_direct_space_state();
	PhysicsDirectSpaceState3D::RayResult result;

	Array keys = snap_data.keys();

	// The maximum height an object can travel to be snapped
	const float max_snap_height = 500.0;

	// Will be set to `true` if at least one node from the selection was successfully snapped
	bool snapped_to_floor = false;

	if (keys.size()) {
		// For snapping to be performed, there must be solid geometry under at least one of the selected nodes.
		// We need to check this before snapping to register the undo/redo action only if needed.
		for (int i = 0; i < keys.size(); i++) {
			Node *node = Object::cast_to<Node>(keys[i]);
			Node3D *sp = Object::cast_to<Node3D>(node);
			Dictionary d = snap_data[node];
			Vector3 from = d["from"];
			Vector3 to = from - Vector3(0.0, max_snap_height, 0.0);
			HashSet<RID> excluded = _get_physics_bodies_rid(sp);

			PhysicsDirectSpaceState3D::RayParameters ray_params;
			ray_params.from = from;
			ray_params.to = to;
			ray_params.exclude = excluded;

			if (ss->intersect_ray(ray_params, result)) {
				snapped_to_floor = true;
			}
		}

		if (snapped_to_floor) {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Snap Nodes to Floor"));

			// Perform snapping if at least one node can be snapped
			for (int i = 0; i < keys.size(); i++) {
				Node *node = Object::cast_to<Node>(keys[i]);
				Node3D *sp = Object::cast_to<Node3D>(node);
				Dictionary d = snap_data[node];
				Vector3 from = d["from"];
				Vector3 to = from - Vector3(0.0, max_snap_height, 0.0);
				HashSet<RID> excluded = _get_physics_bodies_rid(sp);

				PhysicsDirectSpaceState3D::RayParameters ray_params;
				ray_params.from = from;
				ray_params.to = to;
				ray_params.exclude = excluded;

				if (ss->intersect_ray(ray_params, result)) {
					Vector3 position_offset = d["position_offset"];
					Transform3D new_transform = sp->get_global_transform();

					new_transform.origin.y = result.position.y;
					new_transform.origin = new_transform.origin - position_offset;

					Node3D *parent = sp->get_parent_node_3d();
					Transform3D new_local_xform = parent ? parent->get_global_transform().affine_inverse() * new_transform : new_transform;
					undo_redo->add_do_method(sp, "set_transform", new_local_xform);
					undo_redo->add_undo_method(sp, "set_transform", sp->get_transform());
				}
			}

			undo_redo->commit_action();
		} else {
			EditorNode::get_singleton()->show_warning(TTR("Couldn't find a solid floor to snap the selection to."));
		}
	}
}

void Node3DEditor::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_visible_in_tree()) {
		return;
	}

	snap_key_enabled = Input::get_singleton()->is_key_pressed(Key::CTRL);
}

void Node3DEditor::_sun_environ_settings_pressed() {
	Vector2 pos = sun_environ_settings->get_screen_position() + sun_environ_settings->get_size();
	sun_environ_popup->set_position(pos - Vector2(sun_environ_popup->get_contents_minimum_size().width / 2, 0));
	sun_environ_popup->reset_size();
	sun_environ_popup->popup();
	// Grabbing the focus is required for Shift modifier checking to be functional
	// (when the Add sun/environment buttons are pressed).
	sun_environ_popup->grab_focus();
}

void Node3DEditor::_add_sun_to_scene(bool p_already_added_environment) {
	sun_environ_popup->hide();

	if (!p_already_added_environment && world_env_count == 0 && Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		// Prevent infinite feedback loop between the sun and environment methods.
		_add_environment_to_scene(true);
	}

	Node *base = get_tree()->get_edited_scene_root();
	if (!base) {
		// Create a root node so we can add child nodes to it.
		SceneTreeDock::get_singleton()->add_root_node(memnew(Node3D));
		base = get_tree()->get_edited_scene_root();
	}
	ERR_FAIL_NULL(base);
	Node *new_sun = preview_sun->duplicate();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Preview Sun to Scene"));
	undo_redo->add_do_method(base, "add_child", new_sun, true);
	// Move to the beginning of the scene tree since more "global" nodes
	// generally look better when placed at the top.
	undo_redo->add_do_method(base, "move_child", new_sun, 0);
	undo_redo->add_do_method(new_sun, "set_owner", base);
	undo_redo->add_undo_method(base, "remove_child", new_sun);
	undo_redo->add_do_reference(new_sun);
	undo_redo->commit_action();
}

void Node3DEditor::_add_environment_to_scene(bool p_already_added_sun) {
	sun_environ_popup->hide();

	if (!p_already_added_sun && directional_light_count == 0 && Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		// Prevent infinite feedback loop between the sun and environment methods.
		_add_sun_to_scene(true);
	}

	Node *base = get_tree()->get_edited_scene_root();
	if (!base) {
		// Create a root node so we can add child nodes to it.
		SceneTreeDock::get_singleton()->add_root_node(memnew(Node3D));
		base = get_tree()->get_edited_scene_root();
	}
	ERR_FAIL_NULL(base);

	WorldEnvironment *new_env = memnew(WorldEnvironment);
	new_env->set_environment(preview_environment->get_environment()->duplicate(true));
	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		new_env->set_camera_attributes(preview_environment->get_camera_attributes()->duplicate(true));
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Preview Environment to Scene"));
	undo_redo->add_do_method(base, "add_child", new_env, true);
	// Move to the beginning of the scene tree since more "global" nodes
	// generally look better when placed at the top.
	undo_redo->add_do_method(base, "move_child", new_env, 0);
	undo_redo->add_do_method(new_env, "set_owner", base);
	undo_redo->add_undo_method(base, "remove_child", new_env);
	undo_redo->add_do_reference(new_env);
	undo_redo->commit_action();
}

void Node3DEditor::_update_theme() {
	tool_button[TOOL_MODE_SELECT]->set_button_icon(get_editor_theme_icon(SNAME("ToolSelect")));
	tool_button[TOOL_MODE_MOVE]->set_button_icon(get_editor_theme_icon(SNAME("ToolMove")));
	tool_button[TOOL_MODE_ROTATE]->set_button_icon(get_editor_theme_icon(SNAME("ToolRotate")));
	tool_button[TOOL_MODE_SCALE]->set_button_icon(get_editor_theme_icon(SNAME("ToolScale")));
	tool_button[TOOL_MODE_LIST_SELECT]->set_button_icon(get_editor_theme_icon(SNAME("ListSelect")));
	tool_button[TOOL_LOCK_SELECTED]->set_button_icon(get_editor_theme_icon(SNAME("Lock")));
	tool_button[TOOL_UNLOCK_SELECTED]->set_button_icon(get_editor_theme_icon(SNAME("Unlock")));
	tool_button[TOOL_GROUP_SELECTED]->set_button_icon(get_editor_theme_icon(SNAME("Group")));
	tool_button[TOOL_UNGROUP_SELECTED]->set_button_icon(get_editor_theme_icon(SNAME("Ungroup")));
	tool_button[TOOL_RULER]->set_button_icon(get_editor_theme_icon(SNAME("Ruler")));

	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_button_icon(get_editor_theme_icon(SNAME("Object")));
	tool_option_button[TOOL_OPT_USE_SNAP]->set_button_icon(get_editor_theme_icon(SNAME("Snap")));

	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), get_editor_theme_icon(SNAME("Panels1")));
	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), get_editor_theme_icon(SNAME("Panels2")));
	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), get_editor_theme_icon(SNAME("Panels2Alt")));
	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), get_editor_theme_icon(SNAME("Panels3")));
	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), get_editor_theme_icon(SNAME("Panels3Alt")));
	view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), get_editor_theme_icon(SNAME("Panels4")));

	sun_button->set_button_icon(get_editor_theme_icon(SNAME("PreviewSun")));
	environ_button->set_button_icon(get_editor_theme_icon(SNAME("PreviewEnvironment")));
	sun_environ_settings->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));

	sun_title->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("title_font"), SNAME("Window")));
	environ_title->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("title_font"), SNAME("Window")));

	sun_color->set_custom_minimum_size(Size2(0, get_theme_constant(SNAME("color_picker_button_height"), EditorStringName(Editor))));
	environ_sky_color->set_custom_minimum_size(Size2(0, get_theme_constant(SNAME("color_picker_button_height"), EditorStringName(Editor))));
	environ_ground_color->set_custom_minimum_size(Size2(0, get_theme_constant(SNAME("color_picker_button_height"), EditorStringName(Editor))));

	context_toolbar_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("ContextualToolbar"), EditorStringName(EditorStyles)));
}

void Node3DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);

			_refresh_menu_icons();

			get_tree()->connect("node_removed", callable_mp(this, &Node3DEditor::_node_removed));
			get_tree()->connect("node_added", callable_mp(this, &Node3DEditor::_node_added));
			SceneTreeDock::get_singleton()->get_tree_editor()->connect("node_changed", callable_mp(this, &Node3DEditor::_refresh_menu_icons));
			editor_selection->connect("selection_changed", callable_mp(this, &Node3DEditor::_selection_changed));

			_update_preview_environment();

			sun_state->set_custom_minimum_size(sun_vb->get_combined_minimum_size());
			environ_state->set_custom_minimum_size(environ_vb->get_combined_minimum_size());

			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &Node3DEditor::update_all_gizmos).bind(Variant()));
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_update_theme();
			_register_all_gizmos();
			_update_gizmos_menu();
			_init_indicators();
			update_all_gizmos();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_finish_indicators();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
			_update_gizmos_menu_theme();
			sun_title->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("title_font"), SNAME("Window")));
			environ_title->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("title_font"), SNAME("Window")));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			// Update grid color by rebuilding grid.
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/3d")) {
				_finish_grid();
				_init_grid();
			}
		} break;

		case NOTIFICATION_PHYSICS_PROCESS: {
			if (do_snap_selected_nodes_to_floor) {
				_snap_selected_nodes_to_floor();
				do_snap_selected_nodes_to_floor = false;
			}
		}
	}
}

bool Node3DEditor::is_subgizmo_selected(int p_id) {
	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;
	if (se) {
		return se->subgizmos.has(p_id);
	}
	return false;
}

bool Node3DEditor::is_current_selected_gizmo(const EditorNode3DGizmo *p_gizmo) {
	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;
	if (se) {
		return se->gizmo == p_gizmo;
	}
	return false;
}

Vector<int> Node3DEditor::get_subgizmo_selection() {
	Node3DEditorSelectedItem *se = selected ? editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected) : nullptr;

	Vector<int> ret;
	if (se) {
		for (const KeyValue<int, Transform3D> &E : se->subgizmos) {
			ret.push_back(E.key);
		}
	}
	return ret;
}

void Node3DEditor::clear_subgizmo_selection(Object *p_obj) {
	_clear_subgizmo_selection(p_obj);
}

void Node3DEditor::add_control_to_menu_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_COND(p_control->get_parent());

	VSeparator *sep = memnew(VSeparator);
	context_toolbar_hbox->add_child(sep);
	context_toolbar_hbox->add_child(p_control);
	context_toolbar_separators[p_control] = sep;

	p_control->connect(SceneStringName(visibility_changed), callable_mp(this, &Node3DEditor::_update_context_toolbar));

	_update_context_toolbar();
}

void Node3DEditor::remove_control_from_menu_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_COND(p_control->get_parent() != context_toolbar_hbox);

	p_control->disconnect(SceneStringName(visibility_changed), callable_mp(this, &Node3DEditor::_update_context_toolbar));

	VSeparator *sep = context_toolbar_separators[p_control];
	context_toolbar_hbox->remove_child(sep);
	context_toolbar_hbox->remove_child(p_control);
	context_toolbar_separators.erase(p_control);
	memdelete(sep);

	_update_context_toolbar();
}

void Node3DEditor::_update_context_toolbar() {
	bool has_visible = false;
	bool first_visible = false;

	for (int i = 0; i < context_toolbar_hbox->get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(context_toolbar_hbox->get_child(i));
		if (!child || !context_toolbar_separators.has(child)) {
			continue;
		}
		if (child->is_visible()) {
			first_visible = !has_visible;
			has_visible = true;
		}

		VSeparator *sep = context_toolbar_separators[child];
		sep->set_visible(!first_visible && child->is_visible());
	}

	context_toolbar_panel->set_visible(has_visible);
}

void Node3DEditor::set_can_preview(Camera3D *p_preview) {
	for (int i = 0; i < 4; i++) {
		viewports[i]->set_can_preview(p_preview);
	}
}

VSplitContainer *Node3DEditor::get_shader_split() {
	return shader_split;
}

Node3DEditorViewport *Node3DEditor::get_last_used_viewport() {
	return viewports[last_used_viewport];
}

void Node3DEditor::add_control_to_left_panel(Control *p_control) {
	left_panel_split->add_child(p_control);
	left_panel_split->move_child(p_control, 0);
}

void Node3DEditor::add_control_to_right_panel(Control *p_control) {
	right_panel_split->add_child(p_control);
	right_panel_split->move_child(p_control, 1);
}

void Node3DEditor::remove_control_from_left_panel(Control *p_control) {
	left_panel_split->remove_child(p_control);
}

void Node3DEditor::remove_control_from_right_panel(Control *p_control) {
	right_panel_split->remove_child(p_control);
}

void Node3DEditor::move_control_to_left_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	if (p_control->get_parent() == left_panel_split) {
		return;
	}

	ERR_FAIL_COND(p_control->get_parent() != right_panel_split);
	right_panel_split->remove_child(p_control);

	add_control_to_left_panel(p_control);
}

void Node3DEditor::move_control_to_right_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	if (p_control->get_parent() == right_panel_split) {
		return;
	}

	ERR_FAIL_COND(p_control->get_parent() != left_panel_split);
	left_panel_split->remove_child(p_control);

	add_control_to_right_panel(p_control);
}

void Node3DEditor::_request_gizmo(Object *p_obj) {
	Node3D *sp = Object::cast_to<Node3D>(p_obj);
	if (!sp) {
		return;
	}

	bool is_selected = (sp == selected);

	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (edited_scene && (sp == edited_scene || (sp->get_owner() && edited_scene->is_ancestor_of(sp)))) {
		for (int i = 0; i < gizmo_plugins_by_priority.size(); ++i) {
			Ref<EditorNode3DGizmo> seg = gizmo_plugins_by_priority.write[i]->get_gizmo(sp);

			if (seg.is_valid()) {
				sp->add_gizmo(seg);

				if (is_selected != seg->is_selected()) {
					seg->set_selected(is_selected);
				}
			}
		}
		if (!sp->get_gizmos().is_empty()) {
			sp->update_gizmos();
		}
	}
}

void Node3DEditor::_request_gizmo_for_id(ObjectID p_id) {
	Node3D *node = Object::cast_to<Node3D>(ObjectDB::get_instance(p_id));
	if (node) {
		_request_gizmo(node);
	}
}

void Node3DEditor::_set_subgizmo_selection(Object *p_obj, Ref<Node3DGizmo> p_gizmo, int p_id, Transform3D p_transform) {
	if (p_id == -1) {
		_clear_subgizmo_selection(p_obj);
		return;
	}

	Node3D *sp = nullptr;
	if (p_obj) {
		sp = Object::cast_to<Node3D>(p_obj);
	} else {
		sp = selected;
	}

	if (!sp) {
		return;
	}

	Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
	if (se) {
		se->subgizmos.clear();
		se->subgizmos.insert(p_id, p_transform);
		se->gizmo = p_gizmo;
		sp->update_gizmos();
		update_transform_gizmo();
	}
}

void Node3DEditor::_clear_subgizmo_selection(Object *p_obj) {
	Node3D *sp = nullptr;
	if (p_obj) {
		sp = Object::cast_to<Node3D>(p_obj);
	} else {
		sp = selected;
	}

	if (!sp) {
		return;
	}

	Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(sp);
	if (se) {
		se->subgizmos.clear();
		se->gizmo.unref();
		sp->update_gizmos();
		update_transform_gizmo();
	}
}

void Node3DEditor::_toggle_maximize_view(Object *p_viewport) {
	if (!p_viewport) {
		return;
	}
	Node3DEditorViewport *current_viewport = Object::cast_to<Node3DEditorViewport>(p_viewport);
	if (!current_viewport) {
		return;
	}

	int index = -1;
	bool maximized = false;
	for (int i = 0; i < 4; i++) {
		if (viewports[i] == current_viewport) {
			index = i;
			if (current_viewport->get_global_rect() == viewport_base->get_global_rect()) {
				maximized = true;
			}
			break;
		}
	}
	if (index == -1) {
		return;
	}

	if (!maximized) {
		for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
			if (i == (uint32_t)index) {
				viewports[i]->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
			} else {
				viewports[i]->hide();
			}
		}
	} else {
		for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
			viewports[i]->show();
		}

		if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT))) {
			_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);
		} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS))) {
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS);
		} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT))) {
			_menu_item_pressed(MENU_VIEW_USE_2_VIEWPORTS_ALT);
		} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS))) {
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS);
		} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT))) {
			_menu_item_pressed(MENU_VIEW_USE_3_VIEWPORTS_ALT);
		} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS))) {
			_menu_item_pressed(MENU_VIEW_USE_4_VIEWPORTS);
		}
	}
}

void Node3DEditor::_viewport_clicked(int p_viewport_idx) {
	last_used_viewport = p_viewport_idx;
}

void Node3DEditor::_node_added(Node *p_node) {
	if (EditorNode::get_singleton()->get_scene_root()->is_ancestor_of(p_node)) {
		if (Object::cast_to<WorldEnvironment>(p_node)) {
			world_env_count++;
			if (world_env_count == 1) {
				_update_preview_environment();
			}
		} else if (Object::cast_to<DirectionalLight3D>(p_node)) {
			directional_light_count++;
			if (directional_light_count == 1) {
				_update_preview_environment();
			}
		}
	}
}

void Node3DEditor::_node_removed(Node *p_node) {
	if (EditorNode::get_singleton()->get_scene_root()->is_ancestor_of(p_node)) {
		if (Object::cast_to<WorldEnvironment>(p_node)) {
			world_env_count--;
			if (world_env_count == 0) {
				_update_preview_environment();
			}
		} else if (Object::cast_to<DirectionalLight3D>(p_node)) {
			directional_light_count--;
			if (directional_light_count == 0) {
				_update_preview_environment();
			}
		}
	}

	if (p_node == selected) {
		Node3DEditorSelectedItem *se = editor_selection->get_node_editor_data<Node3DEditorSelectedItem>(selected);
		if (se) {
			se->gizmo.unref();
			se->subgizmos.clear();
		}
		selected = nullptr;
		update_transform_gizmo();
	}
}

void Node3DEditor::_register_all_gizmos() {
	add_gizmo_plugin(Ref<Camera3DGizmoPlugin>(memnew(Camera3DGizmoPlugin)));
	add_gizmo_plugin(Ref<Light3DGizmoPlugin>(memnew(Light3DGizmoPlugin)));
	add_gizmo_plugin(Ref<AudioStreamPlayer3DGizmoPlugin>(memnew(AudioStreamPlayer3DGizmoPlugin)));
	add_gizmo_plugin(Ref<AudioListener3DGizmoPlugin>(memnew(AudioListener3DGizmoPlugin)));
	add_gizmo_plugin(Ref<MeshInstance3DGizmoPlugin>(memnew(MeshInstance3DGizmoPlugin)));
	add_gizmo_plugin(Ref<OccluderInstance3DGizmoPlugin>(memnew(OccluderInstance3DGizmoPlugin)));
	add_gizmo_plugin(Ref<SoftBody3DGizmoPlugin>(memnew(SoftBody3DGizmoPlugin)));
	add_gizmo_plugin(Ref<SpriteBase3DGizmoPlugin>(memnew(SpriteBase3DGizmoPlugin)));
	add_gizmo_plugin(Ref<Label3DGizmoPlugin>(memnew(Label3DGizmoPlugin)));
	add_gizmo_plugin(Ref<GeometryInstance3DGizmoPlugin>(memnew(GeometryInstance3DGizmoPlugin)));
	add_gizmo_plugin(Ref<Marker3DGizmoPlugin>(memnew(Marker3DGizmoPlugin)));
	add_gizmo_plugin(Ref<RayCast3DGizmoPlugin>(memnew(RayCast3DGizmoPlugin)));
	add_gizmo_plugin(Ref<ShapeCast3DGizmoPlugin>(memnew(ShapeCast3DGizmoPlugin)));
	add_gizmo_plugin(Ref<SpringArm3DGizmoPlugin>(memnew(SpringArm3DGizmoPlugin)));
	add_gizmo_plugin(Ref<VehicleWheel3DGizmoPlugin>(memnew(VehicleWheel3DGizmoPlugin)));
	add_gizmo_plugin(Ref<VisibleOnScreenNotifier3DGizmoPlugin>(memnew(VisibleOnScreenNotifier3DGizmoPlugin)));
	add_gizmo_plugin(Ref<GPUParticles3DGizmoPlugin>(memnew(GPUParticles3DGizmoPlugin)));
	add_gizmo_plugin(Ref<GPUParticlesCollision3DGizmoPlugin>(memnew(GPUParticlesCollision3DGizmoPlugin)));
	add_gizmo_plugin(Ref<CPUParticles3DGizmoPlugin>(memnew(CPUParticles3DGizmoPlugin)));
	add_gizmo_plugin(Ref<ReflectionProbeGizmoPlugin>(memnew(ReflectionProbeGizmoPlugin)));
	add_gizmo_plugin(Ref<DecalGizmoPlugin>(memnew(DecalGizmoPlugin)));
	add_gizmo_plugin(Ref<VoxelGIGizmoPlugin>(memnew(VoxelGIGizmoPlugin)));
	add_gizmo_plugin(Ref<LightmapGIGizmoPlugin>(memnew(LightmapGIGizmoPlugin)));
	add_gizmo_plugin(Ref<LightmapProbeGizmoPlugin>(memnew(LightmapProbeGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionObject3DGizmoPlugin>(memnew(CollisionObject3DGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionShape3DGizmoPlugin>(memnew(CollisionShape3DGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionPolygon3DGizmoPlugin>(memnew(CollisionPolygon3DGizmoPlugin)));
	add_gizmo_plugin(Ref<NavigationLink3DGizmoPlugin>(memnew(NavigationLink3DGizmoPlugin)));
	add_gizmo_plugin(Ref<NavigationRegion3DGizmoPlugin>(memnew(NavigationRegion3DGizmoPlugin)));
	add_gizmo_plugin(Ref<Joint3DGizmoPlugin>(memnew(Joint3DGizmoPlugin)));
	add_gizmo_plugin(Ref<PhysicalBone3DGizmoPlugin>(memnew(PhysicalBone3DGizmoPlugin)));
	add_gizmo_plugin(Ref<FogVolumeGizmoPlugin>(memnew(FogVolumeGizmoPlugin)));
}

void Node3DEditor::_bind_methods() {
	ClassDB::bind_method("_get_editor_data", &Node3DEditor::_get_editor_data);
	ClassDB::bind_method("_request_gizmo", &Node3DEditor::_request_gizmo);
	ClassDB::bind_method("_request_gizmo_for_id", &Node3DEditor::_request_gizmo_for_id);
	ClassDB::bind_method("_set_subgizmo_selection", &Node3DEditor::_set_subgizmo_selection);
	ClassDB::bind_method("_clear_subgizmo_selection", &Node3DEditor::_clear_subgizmo_selection);
	ClassDB::bind_method("_refresh_menu_icons", &Node3DEditor::_refresh_menu_icons);

	ClassDB::bind_method("update_all_gizmos", &Node3DEditor::update_all_gizmos);
	ClassDB::bind_method("update_transform_gizmo", &Node3DEditor::update_transform_gizmo);

	ADD_SIGNAL(MethodInfo("transform_key_request"));
	ADD_SIGNAL(MethodInfo("item_lock_status_changed"));
	ADD_SIGNAL(MethodInfo("item_group_status_changed"));
}

void Node3DEditor::clear() {
	settings_fov->set_value(EDITOR_GET("editors/3d/default_fov"));
	settings_znear->set_value(EDITOR_GET("editors/3d/default_z_near"));
	settings_zfar->set_value(EDITOR_GET("editors/3d/default_z_far"));

	snap_translate_value = EditorSettings::get_singleton()->get_project_metadata("3d_editor", "snap_translate_value", 1);
	snap_rotate_value = EditorSettings::get_singleton()->get_project_metadata("3d_editor", "snap_rotate_value", 15);
	snap_scale_value = EditorSettings::get_singleton()->get_project_metadata("3d_editor", "snap_scale_value", 10);
	_snap_update();

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->reset();
	}

	if (origin_instance.is_valid()) {
		RenderingServer::get_singleton()->instance_set_visible(origin_instance, true);
	}

	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN), true);
	for (int i = 0; i < 3; ++i) {
		if (grid_enable[i]) {
			grid_visible[i] = true;
		}
	}

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->view_menu->get_popup()->set_item_checked(viewports[i]->view_menu->get_popup()->get_item_index(Node3DEditorViewport::VIEW_AUDIO_LISTENER), i == 0);
		viewports[i]->viewport->set_as_audio_listener_3d(i == 0);
	}

	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID), true);
	grid_enabled = true;
	grid_init_draw = false;
}

void Node3DEditor::_sun_direction_draw() {
	sun_direction->draw_rect(Rect2(Vector2(), sun_direction->get_size()), Color(1, 1, 1, 1));
	Vector3 z_axis = preview_sun->get_transform().basis.get_column(Vector3::AXIS_Z);
	z_axis = get_editor_viewport(0)->camera->get_camera_transform().basis.xform_inv(z_axis);
	sun_direction_material->set_shader_parameter("sun_direction", Vector3(z_axis.x, -z_axis.y, z_axis.z));
	Color color = sun_color->get_pick_color() * sun_energy->get_value();
	sun_direction_material->set_shader_parameter("sun_color", Vector3(color.r, color.g, color.b));
}

void Node3DEditor::_preview_settings_changed() {
	if (sun_environ_updating) {
		return;
	}

	{ // preview sun
		Transform3D t;
		t.basis = Basis::from_euler(Vector3(sun_rotation.x, sun_rotation.y, 0));
		preview_sun->set_transform(t);
		sun_direction->queue_redraw();
		preview_sun->set_param(Light3D::PARAM_ENERGY, sun_energy->get_value());
		preview_sun->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, sun_max_distance->get_value());
		preview_sun->set_color(sun_color->get_pick_color());
	}

	{ //preview env
		sky_material->set_energy_multiplier(environ_energy->get_value());
		Color hz_color = environ_sky_color->get_pick_color().lerp(environ_ground_color->get_pick_color(), 0.5);
		float hz_lum = hz_color.get_luminance() * 3.333;
		hz_color = hz_color.lerp(Color(hz_lum, hz_lum, hz_lum), 0.5);
		sky_material->set_sky_top_color(environ_sky_color->get_pick_color());
		sky_material->set_sky_horizon_color(hz_color);
		sky_material->set_ground_bottom_color(environ_ground_color->get_pick_color());
		sky_material->set_ground_horizon_color(hz_color);

		environment->set_ssao_enabled(environ_ao_button->is_pressed());
		environment->set_glow_enabled(environ_glow_button->is_pressed());
		environment->set_sdfgi_enabled(environ_gi_button->is_pressed());
		environment->set_tonemapper(environ_tonemap_button->is_pressed() ? Environment::TONE_MAPPER_FILMIC : Environment::TONE_MAPPER_LINEAR);
	}
}

void Node3DEditor::_load_default_preview_settings() {
	sun_environ_updating = true;

	// These default rotations place the preview sun at an angular altitude
	// of 60 degrees (must be negative) and an azimuth of 30 degrees clockwise
	// from north (or 150 CCW from south), from north east, facing south west.
	// On any not-tidally-locked planet, a sun would have an angular altitude
	// of 60 degrees as the average of all points on the sphere at noon.
	// The azimuth choice is arbitrary, but ideally shouldn't be on an axis.
	sun_rotation = Vector2(-Math::deg_to_rad(60.0), Math::deg_to_rad(150.0));

	sun_angle_altitude->set_value(-Math::rad_to_deg(sun_rotation.x));
	sun_angle_azimuth->set_value(180.0 - Math::rad_to_deg(sun_rotation.y));
	sun_direction->queue_redraw();
	environ_sky_color->set_pick_color(Color(0.385, 0.454, 0.55));
	environ_ground_color->set_pick_color(Color(0.2, 0.169, 0.133));
	environ_energy->set_value(1.0);
	if (OS::get_singleton()->get_current_rendering_method() != "gl_compatibility") {
		environ_glow_button->set_pressed(true);
	}
	environ_tonemap_button->set_pressed(true);
	environ_ao_button->set_pressed(false);
	environ_gi_button->set_pressed(false);
	sun_max_distance->set_value(100);

	sun_color->set_pick_color(Color(1, 1, 1));
	sun_energy->set_value(1.0);

	sun_environ_updating = false;
}

void Node3DEditor::_update_preview_environment() {
	bool disable_light = directional_light_count > 0 || !sun_button->is_pressed();

	sun_button->set_disabled(directional_light_count > 0);

	if (disable_light) {
		if (preview_sun->get_parent()) {
			preview_sun->get_parent()->remove_child(preview_sun);
			sun_state->show();
			sun_vb->hide();
			preview_sun_dangling = true;
		}

		if (directional_light_count > 0) {
			sun_state->set_text(TTR("Scene contains\nDirectionalLight3D.\nPreview disabled."));
		} else {
			sun_state->set_text(TTR("Preview disabled."));
		}

	} else {
		if (!preview_sun->get_parent()) {
			add_child(preview_sun, true);
			sun_state->hide();
			sun_vb->show();
			preview_sun_dangling = false;
		}
	}

	sun_angle_altitude->set_value(-Math::rad_to_deg(sun_rotation.x));
	sun_angle_azimuth->set_value(180.0 - Math::rad_to_deg(sun_rotation.y));

	bool disable_env = world_env_count > 0 || !environ_button->is_pressed();

	environ_button->set_disabled(world_env_count > 0);

	if (disable_env) {
		if (preview_environment->get_parent()) {
			preview_environment->get_parent()->remove_child(preview_environment);
			environ_state->show();
			environ_vb->hide();
			preview_env_dangling = true;
		}
		if (world_env_count > 0) {
			environ_state->set_text(TTR("Scene contains\nWorldEnvironment.\nPreview disabled."));
		} else {
			environ_state->set_text(TTR("Preview disabled."));
		}

	} else {
		if (!preview_environment->get_parent()) {
			add_child(preview_environment);
			environ_state->hide();
			environ_vb->show();
			preview_env_dangling = false;
		}
	}
}

void Node3DEditor::_sun_direction_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		sun_rotation.x += mm->get_relative().y * (0.02 * EDSCALE);
		sun_rotation.y -= mm->get_relative().x * (0.02 * EDSCALE);
		sun_rotation.x = CLAMP(sun_rotation.x, -Math_TAU / 4, Math_TAU / 4);
		sun_angle_altitude->set_value(-Math::rad_to_deg(sun_rotation.x));
		sun_angle_azimuth->set_value(180.0 - Math::rad_to_deg(sun_rotation.y));
		_preview_settings_changed();
	}
}

void Node3DEditor::_sun_direction_angle_set() {
	sun_rotation.x = Math::deg_to_rad(-sun_angle_altitude->get_value());
	sun_rotation.y = Math::deg_to_rad(180.0 - sun_angle_azimuth->get_value());
	_preview_settings_changed();
}

Node3DEditor::Node3DEditor() {
	gizmo.visible = true;
	gizmo.scale = 1.0;

	viewport_environment.instantiate();
	VBoxContainer *vbc = this;

	custom_camera = nullptr;
	ERR_FAIL_COND_MSG(singleton != nullptr, "A Node3DEditor singleton already exists.");
	singleton = this;
	editor_selection = EditorNode::get_singleton()->get_editor_selection();
	editor_selection->add_editor_plugin(this);

	snap_enabled = false;
	snap_key_enabled = false;
	tool_mode = TOOL_MODE_SELECT;

	// Add some margin to the sides for better aesthetics.
	// This prevents the first button's hover/pressed effect from "touching" the panel's border,
	// which looks ugly.
	MarginContainer *toolbar_margin = memnew(MarginContainer);
	toolbar_margin->add_theme_constant_override("margin_left", 4 * EDSCALE);
	toolbar_margin->add_theme_constant_override("margin_right", 4 * EDSCALE);
	vbc->add_child(toolbar_margin);

	// A fluid container for all toolbars.
	HFlowContainer *main_flow = memnew(HFlowContainer);
	toolbar_margin->add_child(main_flow);

	// Main toolbars.
	HBoxContainer *main_menu_hbox = memnew(HBoxContainer);
	main_flow->add_child(main_menu_hbox);

	String sct;

	tool_button[TOOL_MODE_SELECT] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_SELECT]);
	tool_button[TOOL_MODE_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SELECT]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_MODE_SELECT]->set_pressed(true);
	tool_button[TOOL_MODE_SELECT]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_TOOL_SELECT));
	tool_button[TOOL_MODE_SELECT]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_select", TTRC("Select Mode"), Key::Q));
	tool_button[TOOL_MODE_SELECT]->set_shortcut_context(this);
	tool_button[TOOL_MODE_SELECT]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Drag: Rotate selected node around pivot.") + "\n" + TTR("Alt+RMB: Show list of all nodes at position clicked, including locked.") + "\n" + TTR("(Available in all modes.)"));
	main_menu_hbox->add_child(memnew(VSeparator));

	tool_button[TOOL_MODE_MOVE] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_MOVE]);
	tool_button[TOOL_MODE_MOVE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_MOVE]->set_theme_type_variation(SceneStringName(FlatButton));

	tool_button[TOOL_MODE_MOVE]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_TOOL_MOVE));
	tool_button[TOOL_MODE_MOVE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_move", TTRC("Move Mode"), Key::W));
	tool_button[TOOL_MODE_MOVE]->set_shortcut_context(this);
	tool_button[TOOL_MODE_MOVE]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Drag: Use snap.") + "\n" + TTR("Alt+RMB: Show list of all nodes at position clicked, including locked."));

	tool_button[TOOL_MODE_ROTATE] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_ROTATE]);
	tool_button[TOOL_MODE_ROTATE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_ROTATE]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_MODE_ROTATE]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_TOOL_ROTATE));
	tool_button[TOOL_MODE_ROTATE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_rotate", TTRC("Rotate Mode"), Key::E));
	tool_button[TOOL_MODE_ROTATE]->set_shortcut_context(this);
	tool_button[TOOL_MODE_ROTATE]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Drag: Use snap.") + "\n" + TTR("Alt+RMB: Show list of all nodes at position clicked, including locked."));

	tool_button[TOOL_MODE_SCALE] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_SCALE]);
	tool_button[TOOL_MODE_SCALE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SCALE]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_MODE_SCALE]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_TOOL_SCALE));
	tool_button[TOOL_MODE_SCALE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_scale", TTRC("Scale Mode"), Key::R));
	tool_button[TOOL_MODE_SCALE]->set_shortcut_context(this);
	tool_button[TOOL_MODE_SCALE]->set_tooltip_text(keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Drag: Use snap.") + "\n" + TTR("Alt+RMB: Show list of all nodes at position clicked, including locked."));

	main_menu_hbox->add_child(memnew(VSeparator));

	tool_button[TOOL_MODE_LIST_SELECT] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_LIST_SELECT]);
	tool_button[TOOL_MODE_LIST_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_LIST_SELECT]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_MODE_LIST_SELECT]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_TOOL_LIST_SELECT));
	tool_button[TOOL_MODE_LIST_SELECT]->set_tooltip_text(TTR("Show list of selectable nodes at position clicked."));

	tool_button[TOOL_LOCK_SELECTED] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_LOCK_SELECTED]);
	tool_button[TOOL_LOCK_SELECTED]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_LOCK_SELECTED]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_LOCK_SELECTED));
	tool_button[TOOL_LOCK_SELECTED]->set_tooltip_text(TTRC("Lock selected node, preventing selection and movement."));
	// Define the shortcut globally (without a context) so that it works if the Scene tree dock is currently focused.
	tool_button[TOOL_LOCK_SELECTED]->set_shortcut(ED_GET_SHORTCUT("editor/lock_selected_nodes"));

	tool_button[TOOL_UNLOCK_SELECTED] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_UNLOCK_SELECTED]);
	tool_button[TOOL_UNLOCK_SELECTED]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_UNLOCK_SELECTED]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_UNLOCK_SELECTED));
	tool_button[TOOL_UNLOCK_SELECTED]->set_tooltip_text(TTRC("Unlock selected node, allowing selection and movement."));
	// Define the shortcut globally (without a context) so that it works if the Scene tree dock is currently focused.
	tool_button[TOOL_UNLOCK_SELECTED]->set_shortcut(ED_GET_SHORTCUT("editor/unlock_selected_nodes"));

	tool_button[TOOL_GROUP_SELECTED] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_GROUP_SELECTED]);
	tool_button[TOOL_GROUP_SELECTED]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_GROUP_SELECTED]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_GROUP_SELECTED));
	tool_button[TOOL_GROUP_SELECTED]->set_tooltip_text(TTRC("Groups the selected node with its children. This selects the parent when any child node is clicked in 2D and 3D view."));
	// Define the shortcut globally (without a context) so that it works if the Scene tree dock is currently focused.
	tool_button[TOOL_GROUP_SELECTED]->set_shortcut(ED_GET_SHORTCUT("editor/group_selected_nodes"));

	tool_button[TOOL_UNGROUP_SELECTED] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_UNGROUP_SELECTED]);
	tool_button[TOOL_UNGROUP_SELECTED]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_button[TOOL_UNGROUP_SELECTED]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_UNGROUP_SELECTED));
	tool_button[TOOL_UNGROUP_SELECTED]->set_tooltip_text(TTRC("Ungroups the selected node from its children. Child nodes will be individual items in 2D and 3D view."));
	// Define the shortcut globally (without a context) so that it works if the Scene tree dock is currently focused.
	tool_button[TOOL_UNGROUP_SELECTED]->set_shortcut(ED_GET_SHORTCUT("editor/ungroup_selected_nodes"));

	tool_button[TOOL_RULER] = memnew(Button);
	main_menu_hbox->add_child(tool_button[TOOL_RULER]);
	tool_button[TOOL_RULER]->set_toggle_mode(true);
	tool_button[TOOL_RULER]->set_theme_type_variation("FlatButton");
	tool_button[TOOL_RULER]->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed).bind(MENU_RULER));
	tool_button[TOOL_RULER]->set_tooltip_text(TTRC("LMB+Drag: Measure the distance between two points in 3D space."));
	// Define the shortcut globally (without a context) so that it works if the Scene tree dock is currently focused.
	tool_button[TOOL_RULER]->set_shortcut(ED_SHORTCUT("spatial_editor/measure", TTRC("Ruler Mode"), Key::M));

	main_menu_hbox->add_child(memnew(VSeparator));

	tool_option_button[TOOL_OPT_LOCAL_COORDS] = memnew(Button);
	main_menu_hbox->add_child(tool_option_button[TOOL_OPT_LOCAL_COORDS]);
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_toggle_mode(true);
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->connect(SceneStringName(toggled), callable_mp(this, &Node3DEditor::_menu_item_toggled).bind(MENU_TOOL_LOCAL_COORDS));
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_shortcut(ED_SHORTCUT("spatial_editor/local_coords", TTRC("Use Local Space"), Key::T));
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_shortcut_context(this);

	tool_option_button[TOOL_OPT_USE_SNAP] = memnew(Button);
	main_menu_hbox->add_child(tool_option_button[TOOL_OPT_USE_SNAP]);
	tool_option_button[TOOL_OPT_USE_SNAP]->set_toggle_mode(true);
	tool_option_button[TOOL_OPT_USE_SNAP]->set_theme_type_variation(SceneStringName(FlatButton));
	tool_option_button[TOOL_OPT_USE_SNAP]->connect(SceneStringName(toggled), callable_mp(this, &Node3DEditor::_menu_item_toggled).bind(MENU_TOOL_USE_SNAP));
	tool_option_button[TOOL_OPT_USE_SNAP]->set_shortcut(ED_SHORTCUT("spatial_editor/snap", TTRC("Use Snap"), Key::Y));
	tool_option_button[TOOL_OPT_USE_SNAP]->set_shortcut_context(this);

	main_menu_hbox->add_child(memnew(VSeparator));
	sun_button = memnew(Button);
	sun_button->set_tooltip_text(TTR("Toggle preview sunlight.\nIf a DirectionalLight3D node is added to the scene, preview sunlight is disabled."));
	sun_button->set_toggle_mode(true);
	sun_button->set_theme_type_variation(SceneStringName(FlatButton));
	sun_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_update_preview_environment), CONNECT_DEFERRED);
	// Preview is enabled by default - ensure this applies on editor startup when there is no state yet.
	sun_button->set_pressed(true);

	main_menu_hbox->add_child(sun_button);

	environ_button = memnew(Button);
	environ_button->set_tooltip_text(TTR("Toggle preview environment.\nIf a WorldEnvironment node is added to the scene, preview environment is disabled."));
	environ_button->set_toggle_mode(true);
	environ_button->set_theme_type_variation(SceneStringName(FlatButton));
	environ_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_update_preview_environment), CONNECT_DEFERRED);
	// Preview is enabled by default - ensure this applies on editor startup when there is no state yet.
	environ_button->set_pressed(true);

	main_menu_hbox->add_child(environ_button);

	sun_environ_settings = memnew(Button);
	sun_environ_settings->set_tooltip_text(TTR("Edit Sun and Environment settings."));
	sun_environ_settings->set_theme_type_variation(SceneStringName(FlatButton));
	sun_environ_settings->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_sun_environ_settings_pressed));

	main_menu_hbox->add_child(sun_environ_settings);

	main_menu_hbox->add_child(memnew(VSeparator));

	// Drag and drop support;
	preview_node = memnew(Node3D);
	preview_bounds = AABB();

	ED_SHORTCUT("spatial_editor/bottom_view", TTRC("Bottom View"), KeyModifierMask::ALT + Key::KP_7);
	ED_SHORTCUT("spatial_editor/top_view", TTRC("Top View"), Key::KP_7);
	ED_SHORTCUT("spatial_editor/rear_view", TTRC("Rear View"), KeyModifierMask::ALT + Key::KP_1);
	ED_SHORTCUT("spatial_editor/front_view", TTRC("Front View"), Key::KP_1);
	ED_SHORTCUT("spatial_editor/left_view", TTRC("Left View"), KeyModifierMask::ALT + Key::KP_3);
	ED_SHORTCUT("spatial_editor/right_view", TTRC("Right View"), Key::KP_3);
	ED_SHORTCUT("spatial_editor/orbit_view_down", TTRC("Orbit View Down"), Key::KP_2);
	ED_SHORTCUT("spatial_editor/orbit_view_left", TTRC("Orbit View Left"), Key::KP_4);
	ED_SHORTCUT("spatial_editor/orbit_view_right", TTRC("Orbit View Right"), Key::KP_6);
	ED_SHORTCUT("spatial_editor/orbit_view_up", TTRC("Orbit View Up"), Key::KP_8);
	ED_SHORTCUT("spatial_editor/orbit_view_180", TTRC("Orbit View 180"), Key::KP_9);
	ED_SHORTCUT("spatial_editor/switch_perspective_orthogonal", TTRC("Switch Perspective/Orthogonal View"), Key::KP_5);
	ED_SHORTCUT("spatial_editor/insert_anim_key", TTRC("Insert Animation Key"), Key::K);
	ED_SHORTCUT("spatial_editor/focus_origin", TTRC("Focus Origin"), Key::O);
	ED_SHORTCUT("spatial_editor/focus_selection", TTRC("Focus Selection"), Key::F);
	ED_SHORTCUT_ARRAY("spatial_editor/align_transform_with_view", TTRC("Align Transform with View"),
			{ int32_t(KeyModifierMask::ALT | KeyModifierMask::CTRL | Key::KP_0),
					int32_t(KeyModifierMask::ALT | KeyModifierMask::CTRL | Key::M),
					int32_t(KeyModifierMask::ALT | KeyModifierMask::CTRL | Key::G) });
	ED_SHORTCUT_OVERRIDE_ARRAY("spatial_editor/align_transform_with_view", "macos",
			{ int32_t(KeyModifierMask::ALT | KeyModifierMask::META | Key::KP_0),
					int32_t(KeyModifierMask::ALT | KeyModifierMask::META | Key::G) });
	ED_SHORTCUT("spatial_editor/align_rotation_with_view", TTRC("Align Rotation with View"), KeyModifierMask::ALT + KeyModifierMask::CMD_OR_CTRL + Key::F);
	ED_SHORTCUT("spatial_editor/freelook_toggle", TTRC("Toggle Freelook"), KeyModifierMask::SHIFT + Key::F);
	ED_SHORTCUT("spatial_editor/decrease_fov", TTRC("Decrease Field of View"), KeyModifierMask::CMD_OR_CTRL + Key::EQUAL); // Usually direct access key for `KEY_PLUS`.
	ED_SHORTCUT("spatial_editor/increase_fov", TTRC("Increase Field of View"), KeyModifierMask::CMD_OR_CTRL + Key::MINUS);
	ED_SHORTCUT("spatial_editor/reset_fov", TTRC("Reset Field of View to Default"), KeyModifierMask::CMD_OR_CTRL + Key::KEY_0);

	PopupMenu *p;

	transform_menu = memnew(MenuButton);
	transform_menu->set_flat(false);
	transform_menu->set_theme_type_variation("FlatMenuButton");
	transform_menu->set_text(TTR("Transform"));
	transform_menu->set_switch_on_hover(true);
	transform_menu->set_shortcut_context(this);
	main_menu_hbox->add_child(transform_menu);

	p = transform_menu->get_popup();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/snap_to_floor", TTRC("Snap Object to Floor"), Key::PAGEDOWN), MENU_SNAP_TO_FLOOR);
	p->add_shortcut(ED_SHORTCUT("spatial_editor/transform_dialog", TTRC("Transform Dialog...")), MENU_TRANSFORM_DIALOG);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/configure_snap", TTRC("Configure Snap...")), MENU_TRANSFORM_CONFIGURE_SNAP);

	p->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed));

	view_menu = memnew(MenuButton);
	view_menu->set_flat(false);
	view_menu->set_theme_type_variation("FlatMenuButton");
	// TRANSLATORS: Noun, name of the 2D/3D View menus.
	view_menu->set_text(TTR("View"));
	view_menu->set_switch_on_hover(true);
	view_menu->set_shortcut_context(this);
	main_menu_hbox->add_child(view_menu);

	main_menu_hbox->add_child(memnew(VSeparator));

	context_toolbar_panel = memnew(PanelContainer);
	context_toolbar_hbox = memnew(HBoxContainer);
	context_toolbar_panel->add_child(context_toolbar_hbox);
	main_flow->add_child(context_toolbar_panel);

	// Get the view menu popup and have it stay open when a checkable item is selected
	p = view_menu->get_popup();
	p->set_hide_on_checkable_item_selection(false);

	accept = memnew(AcceptDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(accept);

	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/1_viewport", TTRC("1 Viewport"), KeyModifierMask::CMD_OR_CTRL + Key::KEY_1), MENU_VIEW_USE_1_VIEWPORT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports", TTRC("2 Viewports"), KeyModifierMask::CMD_OR_CTRL + Key::KEY_2), MENU_VIEW_USE_2_VIEWPORTS);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports_alt", TTRC("2 Viewports (Alt)"), KeyModifierMask::ALT + KeyModifierMask::CMD_OR_CTRL + Key::KEY_2), MENU_VIEW_USE_2_VIEWPORTS_ALT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports", TTRC("3 Viewports"), KeyModifierMask::CMD_OR_CTRL + Key::KEY_3), MENU_VIEW_USE_3_VIEWPORTS);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports_alt", TTRC("3 Viewports (Alt)"), KeyModifierMask::ALT + KeyModifierMask::CMD_OR_CTRL + Key::KEY_3), MENU_VIEW_USE_3_VIEWPORTS_ALT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/4_viewports", TTRC("4 Viewports"), KeyModifierMask::CMD_OR_CTRL + Key::KEY_4), MENU_VIEW_USE_4_VIEWPORTS);
	p->add_separator();

	gizmos_menu = memnew(PopupMenu);
	gizmos_menu->set_hide_on_checkable_item_selection(false);
	p->add_submenu_node_item(TTR("Gizmos"), gizmos_menu);
	gizmos_menu->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditor::_menu_gizmo_toggled));

	p->add_separator();
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_origin", TTRC("View Origin")), MENU_VIEW_ORIGIN);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_grid", TTRC("View Grid"), Key::NUMBERSIGN), MENU_VIEW_GRID);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/settings", TTRC("Settings...")), MENU_VIEW_CAMERA_SETTINGS);

	p->set_item_checked(p->get_item_index(MENU_VIEW_ORIGIN), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_GRID), true);

	p->connect(SceneStringName(id_pressed), callable_mp(this, &Node3DEditor::_menu_item_pressed));

	/* REST OF MENU */

	left_panel_split = memnew(HSplitContainer);
	left_panel_split->set_v_size_flags(SIZE_EXPAND_FILL);
	vbc->add_child(left_panel_split);

	right_panel_split = memnew(HSplitContainer);
	right_panel_split->set_v_size_flags(SIZE_EXPAND_FILL);
	left_panel_split->add_child(right_panel_split);

	shader_split = memnew(VSplitContainer);
	shader_split->set_h_size_flags(SIZE_EXPAND_FILL);
	right_panel_split->add_child(shader_split);
	viewport_base = memnew(Node3DEditorViewportContainer);
	shader_split->add_child(viewport_base);
	viewport_base->set_v_size_flags(SIZE_EXPAND_FILL);
	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i] = memnew(Node3DEditorViewport(this, i));
		viewports[i]->connect("toggle_maximize_view", callable_mp(this, &Node3DEditor::_toggle_maximize_view));
		viewports[i]->connect("clicked", callable_mp(this, &Node3DEditor::_viewport_clicked).bind(i));
		viewports[i]->assign_pending_data_pointers(preview_node, &preview_bounds, accept);
		viewport_base->add_child(viewports[i]);
	}

	/* SNAP DIALOG */

	snap_dialog = memnew(ConfirmationDialog);
	snap_dialog->set_title(TTR("Snap Settings"));
	add_child(snap_dialog);
	snap_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Node3DEditor::_snap_changed));
	snap_dialog->get_cancel_button()->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_snap_update));

	VBoxContainer *snap_dialog_vbc = memnew(VBoxContainer);
	snap_dialog->add_child(snap_dialog_vbc);

	snap_translate = memnew(LineEdit);
	snap_translate->set_select_all_on_focus(true);
	snap_dialog_vbc->add_margin_child(TTR("Translate Snap:"), snap_translate);

	snap_rotate = memnew(LineEdit);
	snap_rotate->set_select_all_on_focus(true);
	snap_dialog_vbc->add_margin_child(TTR("Rotate Snap (deg.):"), snap_rotate);

	snap_scale = memnew(LineEdit);
	snap_scale->set_select_all_on_focus(true);
	snap_dialog_vbc->add_margin_child(TTR("Scale Snap (%):"), snap_scale);

	/* SETTINGS DIALOG */

	settings_dialog = memnew(ConfirmationDialog);
	settings_dialog->set_title(TTR("Viewport Settings"));
	add_child(settings_dialog);
	settings_vbc = memnew(VBoxContainer);
	settings_vbc->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	settings_dialog->add_child(settings_vbc);

	settings_fov = memnew(SpinBox);
	settings_fov->set_max(MAX_FOV);
	settings_fov->set_min(MIN_FOV);
	settings_fov->set_step(0.1);
	settings_fov->set_value(EDITOR_GET("editors/3d/default_fov"));
	settings_fov->set_select_all_on_focus(true);
	settings_fov->set_tooltip_text(TTR("FOV is defined as a vertical value, as the editor camera always uses the Keep Height aspect mode."));
	settings_vbc->add_margin_child(TTR("Perspective VFOV (deg.):"), settings_fov);

	settings_znear = memnew(SpinBox);
	settings_znear->set_max(MAX_Z);
	settings_znear->set_min(MIN_Z);
	settings_znear->set_step(0.01);
	settings_znear->set_value(EDITOR_GET("editors/3d/default_z_near"));
	settings_znear->set_select_all_on_focus(true);
	settings_vbc->add_margin_child(TTR("View Z-Near:"), settings_znear);

	settings_zfar = memnew(SpinBox);
	settings_zfar->set_max(MAX_Z);
	settings_zfar->set_min(MIN_Z);
	settings_zfar->set_step(0.1);
	settings_zfar->set_value(EDITOR_GET("editors/3d/default_z_far"));
	settings_zfar->set_select_all_on_focus(true);
	settings_vbc->add_margin_child(TTR("View Z-Far:"), settings_zfar);

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; ++i) {
		settings_dialog->connect(SceneStringName(confirmed), callable_mp(viewports[i], &Node3DEditorViewport::_view_settings_confirmed).bind(0.0));
	}

	/* XFORM DIALOG */

	xform_dialog = memnew(ConfirmationDialog);
	xform_dialog->set_title(TTR("Transform Change"));
	add_child(xform_dialog);

	VBoxContainer *xform_vbc = memnew(VBoxContainer);
	xform_dialog->add_child(xform_vbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Translate:"));
	xform_vbc->add_child(l);

	HBoxContainer *xform_hbc = memnew(HBoxContainer);
	xform_vbc->add_child(xform_hbc);

	for (int i = 0; i < 3; i++) {
		xform_translate[i] = memnew(LineEdit);
		xform_translate[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		xform_translate[i]->set_select_all_on_focus(true);
		xform_hbc->add_child(xform_translate[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Rotate (deg.):"));
	xform_vbc->add_child(l);

	xform_hbc = memnew(HBoxContainer);
	xform_vbc->add_child(xform_hbc);

	for (int i = 0; i < 3; i++) {
		xform_rotate[i] = memnew(LineEdit);
		xform_rotate[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		xform_rotate[i]->set_select_all_on_focus(true);
		xform_hbc->add_child(xform_rotate[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Scale (ratio):"));
	xform_vbc->add_child(l);

	xform_hbc = memnew(HBoxContainer);
	xform_vbc->add_child(xform_hbc);

	for (int i = 0; i < 3; i++) {
		xform_scale[i] = memnew(LineEdit);
		xform_scale[i]->set_h_size_flags(SIZE_EXPAND_FILL);
		xform_scale[i]->set_select_all_on_focus(true);
		xform_hbc->add_child(xform_scale[i]);
	}

	l = memnew(Label);
	l->set_text(TTR("Transform Type"));
	xform_vbc->add_child(l);

	xform_type = memnew(OptionButton);
	xform_type->set_h_size_flags(SIZE_EXPAND_FILL);
	xform_type->add_item(TTR("Pre"));
	xform_type->add_item(TTR("Post"));
	xform_vbc->add_child(xform_type);

	xform_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Node3DEditor::_xform_dialog_action));

	selected = nullptr;

	set_process_shortcut_input(true);
	add_to_group(SceneStringName(_spatial_editor_group));

	current_hover_gizmo_handle = -1;
	current_hover_gizmo_handle_secondary = false;
	{
		// Sun/preview environment popup.
		sun_environ_popup = memnew(PopupPanel);
		add_child(sun_environ_popup);

		HBoxContainer *sun_environ_hb = memnew(HBoxContainer);

		sun_environ_popup->add_child(sun_environ_hb);

		sun_vb = memnew(VBoxContainer);
		sun_environ_hb->add_child(sun_vb);
		sun_vb->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
		sun_vb->hide();

		sun_title = memnew(Label);
		sun_title->set_theme_type_variation("HeaderMedium");
		sun_vb->add_child(sun_title);
		sun_title->set_text(TTR("Preview Sun"));
		sun_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);

		CenterContainer *sun_direction_center = memnew(CenterContainer);
		sun_direction = memnew(Control);
		sun_direction->set_custom_minimum_size(Size2(128, 128) * EDSCALE);
		sun_direction_center->add_child(sun_direction);
		sun_vb->add_margin_child(TTR("Sun Direction"), sun_direction_center);
		sun_direction->connect(SceneStringName(gui_input), callable_mp(this, &Node3DEditor::_sun_direction_input));
		sun_direction->connect(SceneStringName(draw), callable_mp(this, &Node3DEditor::_sun_direction_draw));
		sun_direction->set_default_cursor_shape(CURSOR_MOVE);

		sun_direction_shader.instantiate();
		sun_direction_shader->set_code(R"(
// 3D editor Preview Sun direction shader.

shader_type canvas_item;

uniform vec3 sun_direction;
uniform vec3 sun_color;

void fragment() {
	vec3 n;
	n.xy = UV * 2.0 - 1.0;
	n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
	COLOR.rgb = dot(n, sun_direction) * sun_color;
	COLOR.a = 1.0 - smoothstep(0.99, 1.0, length(n.xy));
}
)");
		sun_direction_material.instantiate();
		sun_direction_material->set_shader(sun_direction_shader);
		sun_direction_material->set_shader_parameter("sun_direction", Vector3(0, 0, 1));
		sun_direction_material->set_shader_parameter("sun_color", Vector3(1, 1, 1));
		sun_direction->set_material(sun_direction_material);

		HBoxContainer *sun_angle_hbox = memnew(HBoxContainer);
		sun_angle_hbox->set_h_size_flags(SIZE_EXPAND_FILL);
		VBoxContainer *sun_angle_altitude_vbox = memnew(VBoxContainer);
		sun_angle_altitude_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
		Label *sun_angle_altitude_label = memnew(Label);
		sun_angle_altitude_label->set_text(TTR("Angular Altitude"));
		sun_angle_altitude_vbox->add_child(sun_angle_altitude_label);
		sun_angle_altitude = memnew(EditorSpinSlider);
		sun_angle_altitude->set_suffix(U"\u00B0");
		sun_angle_altitude->set_max(90);
		sun_angle_altitude->set_min(-90);
		sun_angle_altitude->set_step(0.1);
		sun_angle_altitude->connect(SceneStringName(value_changed), callable_mp(this, &Node3DEditor::_sun_direction_angle_set).unbind(1));
		sun_angle_altitude_vbox->add_child(sun_angle_altitude);
		sun_angle_hbox->add_child(sun_angle_altitude_vbox);
		VBoxContainer *sun_angle_azimuth_vbox = memnew(VBoxContainer);
		sun_angle_azimuth_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
		sun_angle_azimuth_vbox->set_custom_minimum_size(Vector2(100, 0));
		Label *sun_angle_azimuth_label = memnew(Label);
		sun_angle_azimuth_label->set_text(TTR("Azimuth"));
		sun_angle_azimuth_vbox->add_child(sun_angle_azimuth_label);
		sun_angle_azimuth = memnew(EditorSpinSlider);
		sun_angle_azimuth->set_suffix(U"\u00B0");
		sun_angle_azimuth->set_max(180);
		sun_angle_azimuth->set_min(-180);
		sun_angle_azimuth->set_step(0.1);
		sun_angle_azimuth->set_allow_greater(true);
		sun_angle_azimuth->set_allow_lesser(true);
		sun_angle_azimuth->connect(SceneStringName(value_changed), callable_mp(this, &Node3DEditor::_sun_direction_angle_set).unbind(1));
		sun_angle_azimuth_vbox->add_child(sun_angle_azimuth);
		sun_angle_hbox->add_child(sun_angle_azimuth_vbox);
		sun_angle_hbox->add_theme_constant_override("separation", 10);
		sun_vb->add_child(sun_angle_hbox);

		sun_color = memnew(ColorPickerButton);
		sun_color->set_edit_alpha(false);
		sun_vb->add_margin_child(TTR("Sun Color"), sun_color);
		sun_color->connect("color_changed", callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));
		sun_color->get_popup()->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(sun_color->get_picker()));

		sun_energy = memnew(EditorSpinSlider);
		sun_energy->set_max(64.0);
		sun_energy->set_min(0);
		sun_energy->set_step(0.05);
		sun_vb->add_margin_child(TTR("Sun Energy"), sun_energy);
		sun_energy->connect(SceneStringName(value_changed), callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));

		sun_max_distance = memnew(EditorSpinSlider);
		sun_vb->add_margin_child(TTR("Shadow Max Distance"), sun_max_distance);
		sun_max_distance->connect(SceneStringName(value_changed), callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));
		sun_max_distance->set_min(1);
		sun_max_distance->set_max(4096);

		sun_add_to_scene = memnew(Button);
		sun_add_to_scene->set_text(TTR("Add Sun to Scene"));
		sun_add_to_scene->set_tooltip_text(TTR("Adds a DirectionalLight3D node matching the preview sun settings to the current scene.\nHold Shift while clicking to also add the preview environment to the current scene."));
		sun_add_to_scene->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_add_sun_to_scene).bind(false));
		sun_vb->add_spacer();
		sun_vb->add_child(sun_add_to_scene);

		sun_state = memnew(Label);
		sun_environ_hb->add_child(sun_state);
		sun_state->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		sun_state->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		sun_state->set_h_size_flags(SIZE_EXPAND_FILL);

		VSeparator *sc = memnew(VSeparator);
		sc->set_custom_minimum_size(Size2(10 * EDSCALE, 0));
		sc->set_v_size_flags(SIZE_EXPAND_FILL);
		sun_environ_hb->add_child(sc);

		environ_vb = memnew(VBoxContainer);
		sun_environ_hb->add_child(environ_vb);
		environ_vb->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
		environ_vb->hide();

		environ_title = memnew(Label);
		environ_title->set_theme_type_variation("HeaderMedium");

		environ_vb->add_child(environ_title);
		environ_title->set_text(TTR("Preview Environment"));
		environ_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);

		environ_sky_color = memnew(ColorPickerButton);
		environ_sky_color->set_edit_alpha(false);
		environ_sky_color->connect("color_changed", callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));
		environ_sky_color->get_popup()->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(environ_sky_color->get_picker()));
		environ_vb->add_margin_child(TTR("Sky Color"), environ_sky_color);
		environ_ground_color = memnew(ColorPickerButton);
		environ_ground_color->connect("color_changed", callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));
		environ_ground_color->set_edit_alpha(false);
		environ_ground_color->get_popup()->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(environ_ground_color->get_picker()));
		environ_vb->add_margin_child(TTR("Ground Color"), environ_ground_color);
		environ_energy = memnew(EditorSpinSlider);
		environ_energy->set_max(8.0);
		environ_energy->set_min(0);
		environ_energy->set_step(0.05);
		environ_energy->connect(SceneStringName(value_changed), callable_mp(this, &Node3DEditor::_preview_settings_changed).unbind(1));
		environ_vb->add_margin_child(TTR("Sky Energy"), environ_energy);
		HBoxContainer *fx_vb = memnew(HBoxContainer);
		fx_vb->set_h_size_flags(SIZE_EXPAND_FILL);

		environ_ao_button = memnew(Button);
		environ_ao_button->set_text(TTR("AO"));
		environ_ao_button->set_h_size_flags(SIZE_EXPAND_FILL);
		environ_ao_button->set_toggle_mode(true);
		environ_ao_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_preview_settings_changed), CONNECT_DEFERRED);
		fx_vb->add_child(environ_ao_button);
		environ_glow_button = memnew(Button);
		environ_glow_button->set_text(TTR("Glow"));
		environ_glow_button->set_h_size_flags(SIZE_EXPAND_FILL);
		environ_glow_button->set_toggle_mode(true);
		environ_glow_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_preview_settings_changed), CONNECT_DEFERRED);
		fx_vb->add_child(environ_glow_button);
		environ_tonemap_button = memnew(Button);
		environ_tonemap_button->set_text(TTR("Tonemap"));
		environ_tonemap_button->set_h_size_flags(SIZE_EXPAND_FILL);
		environ_tonemap_button->set_toggle_mode(true);
		environ_tonemap_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_preview_settings_changed), CONNECT_DEFERRED);
		fx_vb->add_child(environ_tonemap_button);
		environ_gi_button = memnew(Button);
		environ_gi_button->set_text(TTR("GI"));
		environ_gi_button->set_h_size_flags(SIZE_EXPAND_FILL);
		environ_gi_button->set_toggle_mode(true);
		environ_gi_button->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_preview_settings_changed), CONNECT_DEFERRED);
		fx_vb->add_child(environ_gi_button);
		environ_vb->add_margin_child(TTR("Post Process"), fx_vb);

		environ_add_to_scene = memnew(Button);
		environ_add_to_scene->set_text(TTR("Add Environment to Scene"));
		environ_add_to_scene->set_tooltip_text(TTR("Adds a WorldEnvironment node matching the preview environment settings to the current scene.\nHold Shift while clicking to also add the preview sun to the current scene."));
		environ_add_to_scene->connect(SceneStringName(pressed), callable_mp(this, &Node3DEditor::_add_environment_to_scene).bind(false));
		environ_vb->add_spacer();
		environ_vb->add_child(environ_add_to_scene);

		environ_state = memnew(Label);
		sun_environ_hb->add_child(environ_state);
		environ_state->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		environ_state->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		environ_state->set_h_size_flags(SIZE_EXPAND_FILL);

		preview_sun = memnew(DirectionalLight3D);
		preview_sun->set_shadow(true);
		preview_sun->set_shadow_mode(DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS);
		preview_environment = memnew(WorldEnvironment);
		environment.instantiate();
		preview_environment->set_environment(environment);
		if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
			camera_attributes.instantiate();
			preview_environment->set_camera_attributes(camera_attributes);
		}
		Ref<Sky> sky;
		sky.instantiate();
		sky_material.instantiate();
		sky->set_material(sky_material);
		environment->set_sky(sky);
		environment->set_background(Environment::BG_SKY);

		_load_default_preview_settings();
		_preview_settings_changed();
	}
	clear(); // Make sure values are initialized. Will call _snap_update() for us.
}
Node3DEditor::~Node3DEditor() {
	singleton = nullptr;
	memdelete(preview_node);
	if (preview_sun_dangling && preview_sun) {
		memdelete(preview_sun);
	}
	if (preview_env_dangling && preview_environment) {
		memdelete(preview_environment);
	}
}

void Node3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		spatial_editor->show();
		spatial_editor->set_process(true);
		spatial_editor->set_physics_process(true);

	} else {
		spatial_editor->hide();
		spatial_editor->set_process(false);
		spatial_editor->set_physics_process(false);
	}
}

void Node3DEditorPlugin::edit(Object *p_object) {
	spatial_editor->edit(Object::cast_to<Node3D>(p_object));
}

bool Node3DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("Node3D");
}

Dictionary Node3DEditorPlugin::get_state() const {
	return spatial_editor->get_state();
}

void Node3DEditorPlugin::set_state(const Dictionary &p_state) {
	spatial_editor->set_state(p_state);
}

Size2i Node3DEditor::get_camera_viewport_size(Camera3D *p_camera) {
	Viewport *viewport = p_camera->get_viewport();

	Window *window = Object::cast_to<Window>(viewport);
	if (window) {
		return window->get_size();
	}

	SubViewport *sub_viewport = Object::cast_to<SubViewport>(viewport);
	ERR_FAIL_NULL_V(sub_viewport, Size2i());

	if (sub_viewport == EditorNode::get_singleton()->get_scene_root()) {
		return Size2(GLOBAL_GET("display/window/size/viewport_width"), GLOBAL_GET("display/window/size/viewport_height"));
	}

	return sub_viewport->get_size();
}

Vector3 Node3DEditor::snap_point(Vector3 p_target, Vector3 p_start) const {
	if (is_snap_enabled()) {
		real_t snap = get_translate_snap();
		p_target.snapf(snap);
	}
	return p_target;
}

bool Node3DEditor::is_gizmo_visible() const {
	if (selected) {
		return gizmo.visible && selected->is_transform_gizmo_visible();
	}
	return gizmo.visible;
}

real_t Node3DEditor::get_translate_snap() const {
	real_t snap_value = snap_translate_value;
	if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		snap_value /= 10.0f;
	}
	return snap_value;
}

real_t Node3DEditor::get_rotate_snap() const {
	real_t snap_value = snap_rotate_value;
	if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		snap_value /= 3.0f;
	}
	return snap_value;
}

real_t Node3DEditor::get_scale_snap() const {
	real_t snap_value = snap_scale_value;
	if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		snap_value /= 2.0f;
	}
	return snap_value;
}

struct _GizmoPluginPriorityComparator {
	bool operator()(const Ref<EditorNode3DGizmoPlugin> &p_a, const Ref<EditorNode3DGizmoPlugin> &p_b) const {
		if (p_a->get_priority() == p_b->get_priority()) {
			return p_a->get_gizmo_name() < p_b->get_gizmo_name();
		}
		return p_a->get_priority() > p_b->get_priority();
	}
};

struct _GizmoPluginNameComparator {
	bool operator()(const Ref<EditorNode3DGizmoPlugin> &p_a, const Ref<EditorNode3DGizmoPlugin> &p_b) const {
		return p_a->get_gizmo_name() < p_b->get_gizmo_name();
	}
};

void Node3DEditor::add_gizmo_plugin(Ref<EditorNode3DGizmoPlugin> p_plugin) {
	ERR_FAIL_COND(p_plugin.is_null());

	gizmo_plugins_by_priority.push_back(p_plugin);
	gizmo_plugins_by_priority.sort_custom<_GizmoPluginPriorityComparator>();

	gizmo_plugins_by_name.push_back(p_plugin);
	gizmo_plugins_by_name.sort_custom<_GizmoPluginNameComparator>();

	_update_gizmos_menu();
}

void Node3DEditor::remove_gizmo_plugin(Ref<EditorNode3DGizmoPlugin> p_plugin) {
	gizmo_plugins_by_priority.erase(p_plugin);
	gizmo_plugins_by_name.erase(p_plugin);
	_update_gizmos_menu();
}

DynamicBVH::ID Node3DEditor::insert_gizmo_bvh_node(Node3D *p_node, const AABB &p_aabb) {
	return gizmo_bvh.insert(p_aabb, p_node);
}

void Node3DEditor::update_gizmo_bvh_node(DynamicBVH::ID p_id, const AABB &p_aabb) {
	gizmo_bvh.update(p_id, p_aabb);
	gizmo_bvh.optimize_incremental(1);
}

void Node3DEditor::remove_gizmo_bvh_node(DynamicBVH::ID p_id) {
	gizmo_bvh.remove(p_id);
}

Vector<Node3D *> Node3DEditor::gizmo_bvh_ray_query(const Vector3 &p_ray_start, const Vector3 &p_ray_end) {
	struct Result {
		Vector<Node3D *> nodes;
		bool operator()(void *p_data) {
			nodes.append((Node3D *)p_data);
			return false;
		}
	} result;

	gizmo_bvh.ray_query(p_ray_start, p_ray_end, result);

	return result.nodes;
}

Vector<Node3D *> Node3DEditor::gizmo_bvh_frustum_query(const Vector<Plane> &p_frustum) {
	Vector<Vector3> points = Geometry3D::compute_convex_mesh_points(&p_frustum[0], p_frustum.size());

	struct Result {
		Vector<Node3D *> nodes;
		bool operator()(void *p_data) {
			nodes.append((Node3D *)p_data);
			return false;
		}
	} result;

	gizmo_bvh.convex_query(p_frustum.ptr(), p_frustum.size(), points.ptr(), points.size(), result);

	return result.nodes;
}

Node3DEditorPlugin::Node3DEditorPlugin() {
	spatial_editor = memnew(Node3DEditor);
	spatial_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(spatial_editor);

	spatial_editor->hide();
}

Node3DEditorPlugin::~Node3DEditorPlugin() {
}
