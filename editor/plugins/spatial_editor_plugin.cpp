/**************************************************************************/
/*  spatial_editor_plugin.cpp                                             */
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

#include "spatial_editor_plugin.h"

#include "core/math/camera_matrix.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/sort_array.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/script_editor_debugger.h"
#include "editor/spatial_editor_gizmos.h"
#include "scene/3d/camera.h"
#include "scene/3d/collision_shape.h"
#include "scene/3d/lod_manager.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/multimesh_instance.h"
#include "scene/3d/physics_body.h"
#include "scene/3d/room_manager.h"
#include "scene/3d/visual_instance.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/viewport_container.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

#define DISTANCE_DEFAULT 4

#define GIZMO_ARROW_SIZE 0.35
#define GIZMO_RING_HALF_WIDTH 0.1
#define GIZMO_SCALE_DEFAULT 0.15
#define GIZMO_PLANE_SIZE 0.2
#define GIZMO_PLANE_DST 0.3
#define GIZMO_CIRCLE_SIZE 1.1
#define GIZMO_SCALE_OFFSET (GIZMO_CIRCLE_SIZE + 0.3)
#define GIZMO_ARROW_OFFSET (GIZMO_CIRCLE_SIZE + 0.3)

#define ZOOM_FREELOOK_MIN 0.01
#define ZOOM_FREELOOK_MULTIPLIER 1.08
#define ZOOM_FREELOOK_INDICATOR_DELAY_S 1.5

#define ZOOM_FREELOOK_MAX 10'000

#define MIN_Z 0.01
#define MAX_Z 1000000.0

#define MIN_FOV 0.01
#define MAX_FOV 179

bool SpatialEditor::_prevent_gizmo_generation = false;

void ViewportNavigationControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (!is_connected("mouse_exited", this, "_on_mouse_exited")) {
			connect("mouse_exited", this, "_on_mouse_exited");
		}
		if (!is_connected("mouse_entered", this, "_on_mouse_entered")) {
			connect("mouse_entered", this, "_on_mouse_entered");
		}
	}

	if (p_what == NOTIFICATION_DRAW && viewport != nullptr) {
		_draw();
		_update_navigation();
	}
}

void ViewportNavigationControl::_draw() {
	if (nav_mode == SpatialEditorViewport::NAVIGATION_NONE) {
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
	update();

	if (focused_index != -1 && focused_index != p_index) {
		return;
	}
	if (p_pressed) {
		if (p_position.distance_to(get_size() / 2.0) < get_size().x / 2.0) {
			focused_pos = p_position;
			focused_index = p_index;
			update();
		}
	} else {
		focused_index = -1;
		if (Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
			Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
			Input::get_singleton()->warp_mouse_position(focused_mouse_start);
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
		update();
	}
}

void ViewportNavigationControl::_gui_input(Ref<InputEvent> p_event) {
	// Mouse events
	const Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid() && mouse_button->get_button_index() == BUTTON_LEFT) {
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
		case SpatialEditorViewport::NavigationMode::NAVIGATION_MOVE: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x * 100.0), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;

			const SpatialEditorViewport::FreelookNavigationScheme navigation_scheme = (SpatialEditorViewport::FreelookNavigationScheme)EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_navigation_scheme").operator int();

			Vector3 forward;
			if (navigation_scheme == SpatialEditorViewport::FreelookNavigationScheme::FREELOOK_FULLY_AXIS_LOCKED) {
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

		case SpatialEditorViewport::NavigationMode::NAVIGATION_LOOK: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x * 2.5), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_look(nullptr, delta_normalized * speed);
		} break;

		case SpatialEditorViewport::NAVIGATION_PAN: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_pan(nullptr, -delta_normalized * speed);
		} break;
		case SpatialEditorViewport::NAVIGATION_ZOOM: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_zoom(nullptr, delta_normalized * speed);
		} break;
		case SpatialEditorViewport::NAVIGATION_ORBIT: {
			real_t speed_multiplier = MIN(delta.length() / (get_size().x), 3.0);
			real_t speed = viewport->freelook_speed * speed_multiplier;
			viewport->_nav_orbit(nullptr, delta_normalized * speed);
		} break;
		case SpatialEditorViewport::NAVIGATION_NONE: {
		} break;
	}
}

void ViewportNavigationControl::_on_mouse_entered() {
	hovered = true;
	update();
}

void ViewportNavigationControl::_on_mouse_exited() {
	hovered = false;
	update();
}

void ViewportNavigationControl::set_navigation_mode(SpatialEditorViewport::NavigationMode p_nav_mode) {
	nav_mode = p_nav_mode;
}

void ViewportNavigationControl::set_viewport(SpatialEditorViewport *p_viewport) {
	viewport = p_viewport;
}

void ViewportNavigationControl::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_gui_input"), &ViewportNavigationControl::_gui_input);
	ClassDB::bind_method(D_METHOD("_on_mouse_entered"), &ViewportNavigationControl::_on_mouse_entered);
	ClassDB::bind_method(D_METHOD("_on_mouse_exited"), &ViewportNavigationControl::_on_mouse_exited);
}

void ViewportRotationControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		axis_menu_options.clear();
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_RIGHT);
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_TOP);
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_REAR);
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_LEFT);
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_BOTTOM);
		axis_menu_options.push_back(SpatialEditorViewport::VIEW_FRONT);

		axis_colors.clear();
		axis_colors.push_back(get_color("axis_x_color", "Editor"));
		axis_colors.push_back(get_color("axis_y_color", "Editor"));
		axis_colors.push_back(get_color("axis_z_color", "Editor"));
		update();

		if (!is_connected("mouse_exited", this, "_on_mouse_exited")) {
			connect("mouse_exited", this, "_on_mouse_exited");
		}
	}

	if (p_what == NOTIFICATION_DRAW && viewport != nullptr) {
		_draw();
	}
}

void ViewportRotationControl::_draw() {
	Vector2i center = get_size() / 2.0;
	float radius = get_size().x / 2.0;

	if (focused_axis > -2 || orbiting_index != -1) {
		draw_circle(center, radius, Color(0.5, 0.5, 0.5, 0.25));
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
	const double alpha = focused ? 1.0 : ((p_axis.z_axis + 1.0) / 2.0) * 0.5 + 0.5;
	const Color c = focused ? Color(0.9, 0.9, 0.9) : Color(axis_color.r, axis_color.g, axis_color.b, alpha);

	if (positive) {
		// Draw axis lines for the positive axes.
		const Vector2i center = get_size() / 2.0;
		draw_line(center, p_axis.screen_point, c, 1.5 * EDSCALE, true);

		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS, c);

		// Draw the axis letter for the positive axes.
		const String axis_name = direction == 0 ? "X" : (direction == 1 ? "Y" : "Z");
		draw_char(get_font("rotation_control", "EditorFonts"), p_axis.screen_point + Vector2(-4.0, 5.0) * EDSCALE, axis_name, "", Color(0.0, 0.0, 0.0, alpha));
	} else {
		// Draw an outline around the negative axes.
		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS, c);
		draw_circle(p_axis.screen_point, AXIS_CIRCLE_RADIUS * 0.8, c.darkened(0.4));
	}
}

void ViewportRotationControl::_get_sorted_axis(Vector<Axis2D> &r_axis) {
	Vector2i center = get_size() / 2.0;
	float radius = get_size().x / 2.0;

	float axis_radius = radius - AXIS_CIRCLE_RADIUS - 2.0 * EDSCALE;
	Basis camera_basis = viewport->to_camera_transform(viewport->cursor).get_basis().inverse();

	for (int i = 0; i < 3; ++i) {
		Vector3 axis_3d = camera_basis.get_axis(i);
		Vector2i axis_vector = Vector2(axis_3d.x, -axis_3d.y) * axis_radius;

		if (Math::abs(axis_3d.z) < 1.0) {
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
			axis.axis = i + (axis_3d.z < 0 ? 0 : 3);
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
			Input::get_singleton()->warp_mouse_position(orbiting_mouse_start);
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

void ViewportRotationControl::_gui_input(Ref<InputEvent> p_event) {
	// Mouse events
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == BUTTON_LEFT) {
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
		update();
	}
}

void ViewportRotationControl::_on_mouse_exited() {
	focused_axis = -2;
	update();
}

void ViewportRotationControl::set_viewport(SpatialEditorViewport *p_viewport) {
	viewport = p_viewport;
}

void ViewportRotationControl::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_gui_input"), &ViewportRotationControl::_gui_input);
	ClassDB::bind_method(D_METHOD("_on_mouse_exited"), &ViewportRotationControl::_on_mouse_exited);
}

void SpatialEditorViewport::_view_settings_confirmed(real_t p_interp_delta) {
	// Set FOV override multiplier back to the default, so that the FOV
	// setting specified in the View menu is correctly applied.
	cursor.fov_scale = 1.0;

	_update_camera(p_interp_delta);
}

void SpatialEditorViewport::_update_navigation_controls_visibility() {
	bool show_viewport_rotation_gizmo = EditorSettings::get_singleton()->get("editors/3d/navigation/show_viewport_rotation_gizmo") && (!previewing_cinema && !previewing_camera);
	rotation_control->set_visible(show_viewport_rotation_gizmo);

	bool show_viewport_navigation_gizmo = EditorSettings::get_singleton()->get("editors/3d/navigation/show_viewport_navigation_gizmo") && (!previewing_cinema && !previewing_camera);
	position_control->set_visible(show_viewport_navigation_gizmo);
	look_control->set_visible(show_viewport_navigation_gizmo);
}

void SpatialEditorViewport::_update_camera(float p_interp_delta) {
	bool is_orthogonal = camera->get_projection() == Camera::PROJECTION_ORTHOGONAL;

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
			camera_cursor.eye_pos = old_camera_cursor.eye_pos.linear_interpolate(cursor.eye_pos, CLAMP(factor, 0, 1));

			const float orbit_inertia = EDITOR_GET("editors/3d/navigation_feel/orbit_inertia");
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
			const float orbit_inertia = EDITOR_GET("editors/3d/navigation_feel/orbit_inertia");
			const float translation_inertia = EDITOR_GET("editors/3d/navigation_feel/translation_inertia");
			const float zoom_inertia = EDITOR_GET("editors/3d/navigation_feel/zoom_inertia");

			camera_cursor.x_rot = Math::lerp(old_camera_cursor.x_rot, cursor.x_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));
			camera_cursor.y_rot = Math::lerp(old_camera_cursor.y_rot, cursor.y_rot, MIN(1.f, p_interp_delta * (1 / orbit_inertia)));

			if (Math::abs(camera_cursor.x_rot - cursor.x_rot) < 0.1) {
				camera_cursor.x_rot = cursor.x_rot;
			}
			if (Math::abs(camera_cursor.y_rot - cursor.y_rot) < 0.1) {
				camera_cursor.y_rot = cursor.y_rot;
			}

			camera_cursor.pos = old_camera_cursor.pos.linear_interpolate(cursor.pos, MIN(1.f, p_interp_delta * (1 / translation_inertia)));
			camera_cursor.distance = Math::lerp(old_camera_cursor.distance, cursor.distance, MIN(1.f, p_interp_delta * (1 / zoom_inertia)));
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
		camera->set_global_transform(to_camera_transform(camera_cursor));

		if (orthogonal) {
			float half_fov = Math::deg2rad(get_fov()) / 2.0;
			float height = 2.0 * cursor.distance * Math::tan(half_fov);
			camera->set_orthogonal(height, get_znear(), get_zfar());
		} else {
			camera->set_perspective(get_fov(), get_znear(), get_zfar());
		}

		update_transform_gizmo_view();
		rotation_control->update();
		position_control->update();
		look_control->update();
		spatial_editor->update_grid();
	}
}

Transform SpatialEditorViewport::to_camera_transform(const Cursor &p_cursor) const {
	Transform camera_transform;
	camera_transform.translate(p_cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -p_cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -p_cursor.y_rot);

	if (orthogonal) {
		camera_transform.translate(0, 0, (get_zfar() - get_znear()) / 2.0);
	} else {
		camera_transform.translate(0, 0, p_cursor.distance);
	}

	return camera_transform;
}

int SpatialEditorViewport::get_selected_count() const {
	Map<Node *, Object *> &selection = editor_selection->get_selection();

	int count = 0;

	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->key());
		if (!sp) {
			continue;
		}

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		count++;
	}

	return count;
}

float SpatialEditorViewport::get_znear() const {
	return CLAMP(spatial_editor->get_znear(), MIN_Z, MAX_Z);
}
float SpatialEditorViewport::get_zfar() const {
	return CLAMP(spatial_editor->get_zfar(), MIN_Z, MAX_Z);
}
float SpatialEditorViewport::get_fov() const {
	return CLAMP(spatial_editor->get_fov() * cursor.fov_scale, MIN_FOV, MAX_FOV);
}

Transform SpatialEditorViewport::_get_camera_transform() const {
	return camera->get_global_transform();
}

Vector3 SpatialEditorViewport::_get_camera_position() const {
	return _get_camera_transform().origin;
}

Point2 SpatialEditorViewport::_point_to_screen(const Vector3 &p_point) {
	return camera->unproject_position(p_point) * viewport_container->get_stretch_shrink();
}

Vector3 SpatialEditorViewport::_get_ray_pos(const Vector2 &p_pos) const {
	return camera->project_ray_origin(p_pos / viewport_container->get_stretch_shrink());
}

Vector3 SpatialEditorViewport::_get_camera_normal() const {
	return -_get_camera_transform().basis.get_axis(2);
}

Vector3 SpatialEditorViewport::_get_ray(const Vector2 &p_pos) const {
	return camera->project_ray_normal(p_pos / viewport_container->get_stretch_shrink());
}

void SpatialEditorViewport::_clear_selected() {
	editor_selection->clear();
}

void SpatialEditorViewport::_select_clicked(bool p_append, bool p_single, bool p_allow_locked) {
	if (!clicked) {
		return;
	}

	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(clicked));
	Spatial *selected = Object::cast_to<Spatial>(node);
	if (!selected) {
		return;
	}

	if (!p_allow_locked) {
		// Replace the node by the group if grouped.
		while (node && node != editor->get_edited_scene()->get_parent()) {
			Spatial *selected_tmp = Object::cast_to<Spatial>(node);
			if (selected_tmp && node->has_meta("_edit_group_")) {
				selected = selected_tmp;
			}
			node = node->get_parent();
		}
	}

	if (p_allow_locked || !_is_node_locked(selected)) {
		_select(selected, clicked_wants_append, true);
	}
}

void SpatialEditorViewport::_select(Node *p_node, bool p_append, bool p_single) {
	if (!p_append) {
		editor_selection->clear();
	}

	if (editor_selection->is_selected(p_node)) {
		//erase
		editor_selection->remove_node(p_node);
	} else {
		editor_selection->add_node(p_node);
	}

	if (p_single) {
		if (Engine::get_singleton()->is_editor_hint()) {
			editor->call("edit_node", p_node);
		}
	}
}

ObjectID SpatialEditorViewport::_select_ray(const Point2 &p_pos, bool p_append, bool &r_includes_current, int *r_gizmo_handle, bool p_alt_select) {
	if (r_gizmo_handle) {
		*r_gizmo_handle = -1;
	}

	Vector3 ray = _get_ray(p_pos);
	Vector3 pos = _get_ray_pos(p_pos);
	Vector2 shrinked_pos = p_pos / viewport_container->get_stretch_shrink();

	Vector<ObjectID> instances = VisualServer::get_singleton()->instances_cull_ray(pos, ray, get_tree()->get_root()->get_world()->get_scenario());
	Set<Ref<EditorSpatialGizmo>> found_gizmos;

	Node *edited_scene = get_tree()->get_edited_scene_root();
	ObjectID closest = 0;
	Node *item = nullptr;
	float closest_dist = 1e20;
	int selected_handle = -1;

	for (int i = 0; i < instances.size(); i++) {
		Spatial *spat = Object::cast_to<Spatial>(ObjectDB::get_instance(instances[i]));

		if (!spat) {
			continue;
		}

		Ref<EditorSpatialGizmo> seg = spat->get_gizmo();

		if ((!seg.is_valid()) || found_gizmos.has(seg)) {
			continue;
		}

		found_gizmos.insert(seg);
		Vector3 point;
		Vector3 normal;

		int handle = -1;
		bool inters = seg->intersect_ray(camera, shrinked_pos, point, normal, &handle, p_alt_select);

		if (!inters) {
			continue;
		}

		float dist = pos.distance_to(point);

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
			selected_handle = handle;
		}
	}

	if (!item) {
		return 0;
	}

	if (!editor_selection->is_selected(item) || (r_gizmo_handle && selected_handle >= 0)) {
		if (r_gizmo_handle) {
			*r_gizmo_handle = selected_handle;
		}
	}

	return closest;
}

void SpatialEditorViewport::_find_items_at_pos(const Point2 &p_pos, bool &r_includes_current, Vector<_RayResult> &results, bool p_alt_select) {
	Vector3 ray = _get_ray(p_pos);
	Vector3 pos = _get_ray_pos(p_pos);

	Vector<ObjectID> instances = VisualServer::get_singleton()->instances_cull_ray(pos, ray, get_tree()->get_root()->get_world()->get_scenario());
	Set<Ref<EditorSpatialGizmo>> found_gizmos;

	r_includes_current = false;

	for (int i = 0; i < instances.size(); i++) {
		Spatial *spat = Object::cast_to<Spatial>(ObjectDB::get_instance(instances[i]));

		if (!spat) {
			continue;
		}

		Ref<EditorSpatialGizmo> seg = spat->get_gizmo();

		if (!seg.is_valid()) {
			continue;
		}

		if (found_gizmos.has(seg)) {
			continue;
		}

		found_gizmos.insert(seg);
		Vector3 point;
		Vector3 normal;

		int handle = -1;
		bool inters = seg->intersect_ray(camera, p_pos, point, normal, nullptr, p_alt_select);

		if (!inters) {
			continue;
		}

		float dist = pos.distance_to(point);

		if (dist < 0) {
			continue;
		}

		if (editor_selection->is_selected(spat)) {
			r_includes_current = true;
		}

		_RayResult res;
		res.item = spat;
		res.depth = dist;
		res.handle = handle;
		results.push_back(res);
	}

	if (results.empty()) {
		return;
	}

	results.sort();
}

Vector3 SpatialEditorViewport::_get_screen_to_space(const Vector3 &p_vector3) {
	CameraMatrix cm;
	if (orthogonal) {
		cm.set_orthogonal(camera->get_size(), get_size().aspect(), get_znear() + p_vector3.z, get_zfar());
	} else {
		cm.set_perspective(get_fov(), get_size().aspect(), get_znear() + p_vector3.z, get_zfar());
	}
	Vector2 screen_he = cm.get_viewport_half_extents();

	Transform camera_transform;
	camera_transform.translate(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
	camera_transform.translate(0, 0, cursor.distance);

	return camera_transform.xform(Vector3(((p_vector3.x / get_size().width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (p_vector3.y / get_size().height)) * 2.0 - 1.0) * screen_he.y, -(get_znear() + p_vector3.z)));
}

void SpatialEditorViewport::_select_region() {
	if (cursor.region_begin == cursor.region_end) {
		return; //nothing really
	}

	float z_offset = MAX(0.0, 5.0 - get_znear());

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
			frustum.push_back(Plane(a, (a - b).normalized()));
		} else {
			frustum.push_back(Plane(a, b, cam_pos));
		}
	}

	Plane near(cam_pos, -_get_camera_normal());
	near.d -= get_znear();
	frustum.push_back(near);

	Plane far = -near;
	far.d += get_zfar();
	frustum.push_back(far);

	Vector<ObjectID> instances = VisualServer::get_singleton()->instances_cull_convex(frustum, get_tree()->get_root()->get_world()->get_scenario());
	Vector<Node *> selected;

	Node *edited_scene = get_tree()->get_edited_scene_root();

	for (int i = 0; i < instances.size(); i++) {
		Spatial *sp = Object::cast_to<Spatial>(ObjectDB::get_instance(instances[i]));
		if (!sp || _is_node_locked(sp)) {
			continue;
		}

		Node *item = Object::cast_to<Node>(sp);
		if (item != edited_scene) {
			item = edited_scene->get_deepest_editable_node(item);
		}

		// Replace the node by the group if grouped
		if (item->is_class("Spatial")) {
			Spatial *sel = Object::cast_to<Spatial>(item);
			while (item && item != editor->get_edited_scene()->get_parent()) {
				Spatial *selected_tmp = Object::cast_to<Spatial>(item);
				if (selected_tmp && item->has_meta("_edit_group_")) {
					sel = selected_tmp;
				}
				item = item->get_parent();
			}
			item = sel;
		}

		if (selected.find(item) != -1) {
			continue;
		}

		if (_is_node_locked(item)) {
			continue;
		}

		Ref<EditorSpatialGizmo> seg = sp->get_gizmo();

		if (!seg.is_valid()) {
			continue;
		}

		if (seg->intersect_frustum(camera, frustum)) {
			selected.push_back(item);
		}
	}

	bool single = selected.size() == 1;
	for (int i = 0; i < selected.size(); i++) {
		_select(selected[i], true, single);
	}
}

void SpatialEditorViewport::_update_name() {
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
		name += TTR(" [auto]");
	}

	if (RoomManager::static_rooms_get_active_and_loaded()) {
		// TRANSLATORS: This will be appended to the view name when Portal Occlusion is enabled.
		name += TTR(" [portals active]");
	}

	view_menu->set_text(name);
	view_menu->set_size(Vector2(0, 0)); // resets the button size
}

void SpatialEditorViewport::_compute_edit(const Point2 &p_point) {
	_edit.click_ray = _get_ray(Vector2(p_point.x, p_point.y));
	_edit.click_ray_pos = _get_ray_pos(Vector2(p_point.x, p_point.y));
	_edit.plane = TRANSFORM_VIEW;
	spatial_editor->update_transform_gizmo();
	_edit.center = spatial_editor->get_gizmo_transform().origin;

	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->get());
		if (!sp) {
			continue;
		}

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		se->original = se->sp->get_global_gizmo_transform();
		se->original_local = se->sp->get_local_gizmo_transform();
	}
}

static int _get_key_modifier_setting(const String &p_property) {
	switch (EditorSettings::get_singleton()->get(p_property).operator int()) {
		case 0:
			return 0;
		case 1:
			return KEY_SHIFT;
		case 2:
			return KEY_ALT;
		case 3:
			return KEY_META;
		case 4:
			return KEY_CONTROL;
	}
	return 0;
}

static int _get_key_modifier(Ref<InputEventWithModifiers> e) {
	if (e->get_shift()) {
		return KEY_SHIFT;
	}
	if (e->get_alt()) {
		return KEY_ALT;
	}
	if (e->get_control()) {
		return KEY_CONTROL;
	}
	if (e->get_metakey()) {
		return KEY_META;
	}
	return 0;
}

bool SpatialEditorViewport::_gizmo_select(const Vector2 &p_screenpos, bool p_highlight_only) {
	if (!spatial_editor->is_gizmo_visible()) {
		return false;
	}
	if (get_selected_count() == 0) {
		if (p_highlight_only) {
			spatial_editor->select_gizmo_highlight_axis(-1);
		}
		return false;
	}

	Vector3 ray_pos = _get_ray_pos(Vector2(p_screenpos.x, p_screenpos.y));
	Vector3 ray = _get_ray(Vector2(p_screenpos.x, p_screenpos.y));

	Transform gt = spatial_editor->get_gizmo_transform();
	float gs = gizmo_scale;

	if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE) {
		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {
			Vector3 grabber_pos = gt.origin + gt.basis.get_axis(i) * gs * (GIZMO_ARROW_OFFSET + (GIZMO_ARROW_SIZE * 0.5));
			float grabber_radius = gs * GIZMO_ARROW_SIZE;

			Vector3 r;

			if (Geometry::segment_intersects_sphere(ray_pos, ray_pos + ray * MAX_Z, grabber_pos, grabber_radius, &r)) {
				float d = r.distance_to(ray_pos);
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
				Vector3 ivec2 = gt.basis.get_axis((i + 1) % 3).normalized();
				Vector3 ivec3 = gt.basis.get_axis((i + 2) % 3).normalized();

				// Allow some tolerance to make the plane easier to click,
				// even if the click is actually slightly outside the plane.
				const Vector3 grabber_pos = gt.origin + (ivec2 + ivec3) * gs * (GIZMO_PLANE_SIZE + GIZMO_PLANE_DST * 0.6667);

				Vector3 r;
				Plane plane(gt.origin, gt.basis.get_axis(i).normalized());

				if (plane.intersects_ray(ray_pos, ray, &r)) {
					float dist = r.distance_to(grabber_pos);
					// Allow some tolerance to make the plane easier to click,
					// even if the click is actually slightly outside the plane.
					if (dist < (gs * GIZMO_PLANE_SIZE * 1.5)) {
						float d = ray_pos.distance_to(r);
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
				_compute_edit(Point2(p_screenpos.x, p_screenpos.y));
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis + (is_plane_translate ? 3 : 0));
			}
			return true;
		}
	}

	if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE) {
		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {
			Plane plane(gt.origin, gt.basis.get_axis(i).normalized());
			Vector3 r;
			if (!plane.intersects_ray(ray_pos, ray, &r)) {
				continue;
			}

			float dist = r.distance_to(gt.origin);
			Vector3 r_dir = (r - gt.origin).normalized();

			if (_get_camera_normal().dot(r_dir) <= 0.005) {
				if (dist > gs * (GIZMO_CIRCLE_SIZE - GIZMO_RING_HALF_WIDTH) && dist < gs * (GIZMO_CIRCLE_SIZE + GIZMO_RING_HALF_WIDTH)) {
					float d = ray_pos.distance_to(r);
					if (d < col_d) {
						col_d = d;
						col_axis = i;
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
				_compute_edit(Point2(p_screenpos.x, p_screenpos.y));
				_edit.plane = TransformPlane(TRANSFORM_X_AXIS + col_axis);
			}
			return true;
		}
	}

	if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SCALE) {
		int col_axis = -1;
		float col_d = 1e20;

		for (int i = 0; i < 3; i++) {
			Vector3 grabber_pos = gt.origin + gt.basis.get_axis(i) * gs * GIZMO_SCALE_OFFSET;
			float grabber_radius = gs * GIZMO_ARROW_SIZE;

			Vector3 r;

			if (Geometry::segment_intersects_sphere(ray_pos, ray_pos + ray * MAX_Z, grabber_pos, grabber_radius, &r)) {
				float d = r.distance_to(ray_pos);
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
				Vector3 ivec2 = gt.basis.get_axis((i + 1) % 3).normalized();
				Vector3 ivec3 = gt.basis.get_axis((i + 2) % 3).normalized();

				// Allow some tolerance to make the plane easier to click,
				// even if the click is actually slightly outside the plane.
				Vector3 grabber_pos = gt.origin + (ivec2 + ivec3) * gs * (GIZMO_PLANE_SIZE + GIZMO_PLANE_DST * 0.6667);

				Vector3 r;
				Plane plane(gt.origin, gt.basis.get_axis(i).normalized());

				if (plane.intersects_ray(ray_pos, ray, &r)) {
					float dist = r.distance_to(grabber_pos);
					// Allow some tolerance to make the plane easier to click,
					// even if the click is actually slightly outside the plane.
					if (dist < (gs * GIZMO_PLANE_SIZE * 1.5)) {
						float d = ray_pos.distance_to(r);
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
				_compute_edit(Point2(p_screenpos.x, p_screenpos.y));
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

void SpatialEditorViewport::_surface_mouse_enter() {
	if (!surface->has_focus() && (!get_focus_owner() || !get_focus_owner()->is_text_field())) {
		surface->grab_focus();
	}
}

void SpatialEditorViewport::_surface_mouse_exit() {
	_remove_preview();
}

void SpatialEditorViewport::_surface_focus_enter() {
	view_menu->set_disable_shortcuts(false);
}

void SpatialEditorViewport::_surface_focus_exit() {
	view_menu->set_disable_shortcuts(true);
}
bool SpatialEditorViewport ::_is_node_locked(const Node *p_node) {
	return p_node->has_meta("_edit_lock_") && p_node->get_meta("_edit_lock_");
}
void SpatialEditorViewport::_list_select(Ref<InputEventMouseButton> b) {
	_find_items_at_pos(b->get_position(), clicked_includes_current, selection_results, b->get_shift());

	Node *scene = editor->get_edited_scene();

	for (int i = 0; i < selection_results.size(); i++) {
		Spatial *item = selection_results[i].item;
		if (item != scene && item->get_owner() != scene && item != scene->get_deepest_editable_node(item)) {
			//invalid result
			selection_results.remove(i);
			i--;
		}
	}

	clicked_wants_append = b->get_shift();

	bool allow_locked = b->get_alt() && spatial_editor->get_tool_mode() != SpatialEditor::TOOL_MODE_LIST_SELECT;
	if (selection_results.size() == 1) {
		clicked = selection_results[0].item->get_instance_id();
		selection_results.clear();

		if (clicked) {
			_select_clicked(clicked_wants_append, true, allow_locked);
			clicked = 0;
		}

	} else if (!selection_results.empty()) {
		NodePath root_path = get_tree()->get_edited_scene_root()->get_path();
		StringName root_name = root_path.get_name(root_path.get_name_count() - 1);

		for (int i = 0; i < selection_results.size(); i++) {
			Spatial *spat = selection_results[i].item;

			Ref<Texture> icon = EditorNode::get_singleton()->get_object_icon(spat, "Node");

			String node_path = "/" + root_name + "/" + root_path.rel_path_to(spat->get_path());

			int locked = 0;
			if (_is_node_locked(spat)) {
				locked = 1;
			} else {
				Node *ed_scene = editor->get_edited_scene();
				Node *node = spat;

				while (node && node != ed_scene->get_parent()) {
					Spatial *selected_tmp = Object::cast_to<Spatial>(node);
					if (selected_tmp && node->has_meta("_edit_group_")) {
						locked = 2;
					}
					node = node->get_parent();
				}
			}

			if (!allow_locked && locked > 0) {
				continue;
			}

			String suffix = String();
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

		if (selection_menu->get_item_count() > 0) {
			selection_menu->set_global_position(b->get_global_position());
			selection_menu->popup();
		}
	}
}

void SpatialEditorViewport::_sinput(const Ref<InputEvent> &p_event) {
	if (previewing) {
		return; //do NONE
	}

	{
		EditorNode *en = editor;
		EditorPluginList *force_input_forwarding_list = en->get_editor_plugins_force_input_forwarding();
		if (!force_input_forwarding_list->empty()) {
			bool discard = force_input_forwarding_list->forward_spatial_gui_input(camera, p_event, true);
			if (discard) {
				return;
			}
		}
	}
	{
		EditorNode *en = editor;
		EditorPluginList *over_plugin_list = en->get_editor_plugins_over();
		if (!over_plugin_list->empty()) {
			bool discard = over_plugin_list->forward_spatial_gui_input(camera, p_event, false);
			if (discard) {
				return;
			}
		}
	}

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		emit_signal("clicked", this);

		float zoom_factor = 1 + (ZOOM_FREELOOK_MULTIPLIER - 1) * b->get_factor();
		switch (b->get_button_index()) {
			case BUTTON_WHEEL_UP: {
				if (is_freelook_active()) {
					scale_freelook_speed(zoom_factor);
				} else {
					scale_cursor_distance(1.0 / zoom_factor);
				}
			} break;

			case BUTTON_WHEEL_DOWN: {
				if (is_freelook_active()) {
					scale_freelook_speed(1.0 / zoom_factor);
				} else {
					scale_cursor_distance(zoom_factor);
				}
			} break;

			case BUTTON_RIGHT: {
				NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();

				if (b->is_pressed() && _edit.gizmo.is_valid()) {
					//restore
					_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_initial_value, true);
					_edit.gizmo = Ref<EditorSpatialGizmo>();
				}

				if (_edit.mode == TRANSFORM_NONE && b->is_pressed()) {
					if (b->get_alt()) {
						if (nav_scheme == NAVIGATION_MAYA) {
							break;
						}

						_list_select(b);
						return;
					}
				}

				if (_edit.mode != TRANSFORM_NONE && b->is_pressed()) {
					//cancel motion
					_edit.mode = TRANSFORM_NONE;

					List<Node *> &selection = editor_selection->get_selected_node_list();

					for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
						Spatial *sp = Object::cast_to<Spatial>(E->get());
						if (!sp) {
							continue;
						}

						SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
						if (!se) {
							continue;
						}

						sp->set_global_transform(se->original);
					}
					surface->update();
					set_message(TTR("Transform Aborted."), 3);
				}

				if (b->is_pressed()) {
					const int mod = _get_key_modifier(b);
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
			case BUTTON_MIDDLE: {
				if (b->is_pressed() && _edit.mode != TRANSFORM_NONE) {
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
							set_message(TTR("View Plane Transform."), 2);

						} break;
						case TRANSFORM_YZ:
						case TRANSFORM_XZ:
						case TRANSFORM_XY: {
						} break;
					}
				}
			} break;
			case BUTTON_LEFT: {
				if (b->is_pressed()) {
					NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();
					if ((nav_scheme == NAVIGATION_MAYA || nav_scheme == NAVIGATION_MODO) && b->get_alt()) {
						break;
					}

					if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_LIST_SELECT) {
						_list_select(b);
						break;
					}

					_edit.mouse_pos = b->get_position();
					_edit.original_mouse_pos = b->get_position();
					_edit.snap = spatial_editor->is_snap_enabled();
					_edit.mode = TRANSFORM_NONE;

					//gizmo has priority over everything

					bool can_select_gizmos = true;

					{
						int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
						can_select_gizmos = view_menu->get_popup()->is_item_checked(idx);
					}

					if (can_select_gizmos && spatial_editor->get_selected()) {
						Ref<EditorSpatialGizmo> seg = spatial_editor->get_selected()->get_gizmo();
						if (seg.is_valid()) {
							int handle = -1;
							Vector3 point;
							Vector3 normal;
							bool inters = seg->intersect_ray(camera, _edit.mouse_pos, point, normal, &handle, b->get_shift());
							if (inters && handle != -1) {
								_edit.gizmo = seg;
								_edit.gizmo_handle = handle;
								_edit.gizmo_initial_value = seg->get_handle_value(handle);
								break;
							}
						}
					}

					if (_gizmo_select(_edit.mouse_pos)) {
						break;
					}

					clicked = 0;
					clicked_includes_current = false;

					if ((spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT && b->get_command()) || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE) {
						/* HANDLE ROTATION */
						if (get_selected_count() == 0) {
							break; //bye
						}
						//handle rotate
						_edit.mode = TRANSFORM_ROTATE;
						_compute_edit(b->get_position());
						break;
					}

					if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE) {
						if (get_selected_count() == 0) {
							break; //bye
						}
						//handle translate
						_edit.mode = TRANSFORM_TRANSLATE;
						_compute_edit(b->get_position());
						break;
					}

					if (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SCALE) {
						if (get_selected_count() == 0) {
							break; //bye
						}
						//handle scale
						_edit.mode = TRANSFORM_SCALE;
						_compute_edit(b->get_position());
						break;
					}

					// todo scale

					int gizmo_handle = -1;

					clicked = _select_ray(b->get_position(), b->get_shift(), clicked_includes_current, &gizmo_handle, b->get_shift());
					selection_in_progress = true;

					//clicking is always deferred to either move or release

					clicked_wants_append = b->get_shift();

					if (!clicked) {
						if (!clicked_wants_append) {
							_clear_selected();
						}

						//default to regionselect
						cursor.region_select = true;
						cursor.region_begin = b->get_position();
						cursor.region_end = b->get_position();
					}

					if (clicked && gizmo_handle >= 0) {
						Spatial *spa = Object::cast_to<Spatial>(ObjectDB::get_instance(clicked));
						if (spa) {
							Ref<EditorSpatialGizmo> seg = spa->get_gizmo();
							if (seg.is_valid()) {
								_edit.gizmo = seg;
								_edit.gizmo_handle = gizmo_handle;
								_edit.gizmo_initial_value = seg->get_handle_value(gizmo_handle);
								break;
							}
						}
					}

					surface->update();
				} else {
					if (_edit.gizmo.is_valid()) {
						_edit.gizmo->commit_handle(_edit.gizmo_handle, _edit.gizmo_initial_value, false);
						_edit.gizmo = Ref<EditorSpatialGizmo>();
						break;
					}
					if (clicked) {
						_select_clicked(clicked_wants_append, true);
						// Processing was deferred.
						clicked = 0;
					}

					if (cursor.region_select) {
						if (!clicked_wants_append) {
							_clear_selected();
						}

						_select_region();
						cursor.region_select = false;
						surface->update();
					}

					selection_in_progress = false;
					if (_edit.mode != TRANSFORM_NONE) {
						static const char *_transform_name[4] = {
							TTRC("None"),
							TTRC("Rotate"),
							// TRANSLATORS: This refers to the movement that changes the position of an object.
							TTRC("Translate"),
							TTRC("Scale"),
						};
						undo_redo->create_action(TTRGET(_transform_name[_edit.mode]));

						List<Node *> &selection = editor_selection->get_selected_node_list();

						for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
							Spatial *sp = Object::cast_to<Spatial>(E->get());
							if (!sp) {
								continue;
							}

							SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
							if (!se) {
								continue;
							}

							undo_redo->add_do_method(sp, "set_global_transform", sp->get_global_gizmo_transform());
							undo_redo->add_undo_method(sp, "set_global_transform", se->original);
						}
						undo_redo->commit_action();
						_edit.mode = TRANSFORM_NONE;
						set_message("");
					}

					surface->update();
				}

			} break;
		}
	}

	Ref<InputEventMouseMotion> m = p_event;

	if (m.is_valid()) {
		_edit.mouse_pos = m->get_position();

		if (spatial_editor->get_selected()) {
			Ref<EditorSpatialGizmo> seg = spatial_editor->get_selected()->get_gizmo();
			if (seg.is_valid()) {
				int selected_handle = -1;

				int handle = -1;
				Vector3 point;
				Vector3 normal;
				bool inters = seg->intersect_ray(camera, _edit.mouse_pos, point, normal, &handle, false);
				if (inters && handle != -1) {
					selected_handle = handle;
				}

				if (selected_handle != spatial_editor->get_over_gizmo_handle()) {
					spatial_editor->set_over_gizmo_handle(selected_handle);
					spatial_editor->get_selected()->update_gizmo();
					if (selected_handle != -1) {
						spatial_editor->select_gizmo_highlight_axis(-1);
					}
				}
			}
		}

		if (spatial_editor->get_over_gizmo_handle() == -1 && !(m->get_button_mask() & 1) && !_edit.gizmo.is_valid()) {
			_gizmo_select(_edit.mouse_pos, true);
		}

		NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();
		NavigationMode nav_mode = NAVIGATION_NONE;

		if (_edit.gizmo.is_valid()) {
			_edit.gizmo->set_handle(_edit.gizmo_handle, camera, m->get_position());
			Variant v = _edit.gizmo->get_handle_value(_edit.gizmo_handle);
			String n = _edit.gizmo->get_handle_name(_edit.gizmo_handle);
			set_message(n + ": " + String(v));

		} else if (m->get_button_mask() & BUTTON_MASK_LEFT) {
			if (nav_scheme == NAVIGATION_MAYA && m->get_alt()) {
				nav_mode = NAVIGATION_ORBIT;
			} else if (nav_scheme == NAVIGATION_MODO && m->get_alt() && m->get_shift()) {
				nav_mode = NAVIGATION_PAN;
			} else if (nav_scheme == NAVIGATION_MODO && m->get_alt() && m->get_control()) {
				nav_mode = NAVIGATION_ZOOM;
			} else if (nav_scheme == NAVIGATION_MODO && m->get_alt()) {
				nav_mode = NAVIGATION_ORBIT;
			} else {
				const bool movement_threshold_passed = _edit.original_mouse_pos.distance_to(_edit.mouse_pos) > 8 * EDSCALE;
				if (selection_in_progress && movement_threshold_passed) {
					if (get_selected_count() == 0 || clicked_wants_append) {
						cursor.region_select = true;
						cursor.region_begin = _edit.original_mouse_pos;
						clicked = 0;
					}
				}

				if (cursor.region_select) {
					cursor.region_end = m->get_position();
					surface->update();
					return;
				}

				if (clicked && movement_threshold_passed) {
					_compute_edit(_edit.mouse_pos);
					clicked = 0;
					_edit.mode = TRANSFORM_TRANSLATE;
				}

				if (_edit.mode == TRANSFORM_NONE) {
					return;
				}

				Vector3 ray_pos = _get_ray_pos(m->get_position());
				Vector3 ray = _get_ray(m->get_position());
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
								plane = Plane(_edit.center, _get_camera_normal());
								break;
							case TRANSFORM_X_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(0);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_Y_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(1);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_Z_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(2);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_YZ:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(2) + spatial_editor->get_gizmo_transform().basis.get_axis(1);
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(0));
								plane_mv = true;
								break;
							case TRANSFORM_XZ:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(2) + spatial_editor->get_gizmo_transform().basis.get_axis(0);
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(1));
								plane_mv = true;
								break;
							case TRANSFORM_XY:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(0) + spatial_editor->get_gizmo_transform().basis.get_axis(1);
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(2));
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
								if (_get_key_modifier(m) != KEY_SHIFT) {
									motion = motion_mask.dot(motion) * motion_mask;
								}
							}

						} else {
							float center_click_dist = click.distance_to(_edit.center);
							float center_inters_dist = intersection.distance_to(_edit.center);
							if (center_click_dist == 0) {
								break;
							}

							float scale = center_inters_dist - center_click_dist;
							motion = Vector3(scale, scale, scale);
						}

						motion /= click.distance_to(_edit.center);

						List<Node *> &selection = editor_selection->get_selected_node_list();

						// Disable local transformation for TRANSFORM_VIEW
						bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW);

						if (_edit.snap || spatial_editor->is_snap_enabled()) {
							snap = spatial_editor->get_scale_snap() / 100;
						}
						Vector3 motion_snapped = motion;
						motion_snapped.snap(Vector3(snap, snap, snap));
						// This might not be necessary anymore after issue #288 is solved (in 4.0?).
						// TRANSLATORS: Refers to changing the scale of a node in the 3D editor.
						set_message(TTR("Scaling:") + " (" + String::num(motion_snapped.x, snap_step_decimals) + ", " +
								String::num(motion_snapped.y, snap_step_decimals) + ", " + String::num(motion_snapped.z, snap_step_decimals) + ")");

						for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
							Spatial *sp = Object::cast_to<Spatial>(E->get());
							if (!sp) {
								continue;
							}

							SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
							if (!se) {
								continue;
							}

							if (sp->has_meta("_edit_lock_")) {
								continue;
							}

							Transform original = se->original;
							Transform original_local = se->original_local;
							Transform base = Transform(Basis(), _edit.center);
							Transform t;
							Vector3 local_scale;

							if (local_coords) {
								Basis g = original.basis.orthonormalized();
								Vector3 local_motion = g.inverse().xform(motion);

								if (_edit.snap || spatial_editor->is_snap_enabled()) {
									local_motion.snap(Vector3(snap, snap, snap));
								}

								local_scale = original_local.basis.get_scale() * (local_motion + Vector3(1, 1, 1));

								// Prevent scaling to 0 it would break the gizmo
								Basis check = original_local.basis;
								check.scale(local_scale);
								if (check.determinant() != 0) {
									// Apply scale
									sp->set_scale(local_scale);
								}

							} else {
								if (_edit.snap || spatial_editor->is_snap_enabled()) {
									motion.snap(Vector3(snap, snap, snap));
								}

								Transform r;
								r.basis.scale(motion + Vector3(1, 1, 1));
								t = base * (r * (base.inverse() * original));

								// Apply scale
								sp->set_global_transform(t);
							}
						}

						surface->update();

					} break;

					case TRANSFORM_TRANSLATE: {
						Vector3 motion_mask;
						Plane plane;
						bool plane_mv = false;

						switch (_edit.plane) {
							case TRANSFORM_VIEW:
								plane = Plane(_edit.center, _get_camera_normal());
								break;
							case TRANSFORM_X_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(0);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_Y_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(1);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_Z_AXIS:
								motion_mask = spatial_editor->get_gizmo_transform().basis.get_axis(2);
								plane = Plane(_edit.center, motion_mask.cross(motion_mask.cross(_get_camera_normal())).normalized());
								break;
							case TRANSFORM_YZ:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(0));
								plane_mv = true;
								break;
							case TRANSFORM_XZ:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(1));
								plane_mv = true;
								break;
							case TRANSFORM_XY:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(2));
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

						List<Node *> &selection = editor_selection->get_selected_node_list();

						// Disable local transformation for TRANSFORM_VIEW
						bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW);

						if (_edit.snap || spatial_editor->is_snap_enabled()) {
							snap = spatial_editor->get_translate_snap();
						}
						Vector3 motion_snapped = motion;
						motion_snapped.snap(Vector3(snap, snap, snap));
						// TRANSLATORS: Refers to changing the position of a node in the 3D editor.
						set_message(TTR("Translating:") + " (" + String::num(motion_snapped.x, snap_step_decimals) + ", " +
								String::num(motion_snapped.y, snap_step_decimals) + ", " + String::num(motion_snapped.z, snap_step_decimals) + ")");

						for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
							Spatial *sp = Object::cast_to<Spatial>(E->get());
							if (!sp) {
								continue;
							}

							SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
							if (!se) {
								continue;
							}

							if (sp->has_meta("_edit_lock_")) {
								continue;
							}

							Transform original = se->original;
							Transform t;

							if (local_coords) {
								if (_edit.snap || spatial_editor->is_snap_enabled()) {
									Basis g = original.basis.orthonormalized();
									Vector3 local_motion = g.inverse().xform(motion);
									local_motion.snap(Vector3(snap, snap, snap));

									motion = g.xform(local_motion);
								}

							} else {
								if (_edit.snap || spatial_editor->is_snap_enabled()) {
									motion.snap(Vector3(snap, snap, snap));
								}
							}

							// Apply translation
							t = original;
							t.origin += motion;
							sp->set_global_transform(t);
						}

						surface->update();

					} break;

					case TRANSFORM_ROTATE: {
						Plane plane;
						Vector3 axis;

						switch (_edit.plane) {
							case TRANSFORM_VIEW:
								plane = Plane(_edit.center, _get_camera_normal());
								break;
							case TRANSFORM_X_AXIS:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(0));
								axis = Vector3(1, 0, 0);
								break;
							case TRANSFORM_Y_AXIS:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(1));
								axis = Vector3(0, 1, 0);
								break;
							case TRANSFORM_Z_AXIS:
								plane = Plane(_edit.center, spatial_editor->get_gizmo_transform().basis.get_axis(2));
								axis = Vector3(0, 0, 1);
								break;
							case TRANSFORM_YZ:
							case TRANSFORM_XZ:
							case TRANSFORM_XY:
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

						Vector3 y_axis = (click - _edit.center).normalized();
						Vector3 x_axis = plane.normal.cross(y_axis).normalized();

						double angle = Math::atan2(x_axis.dot(intersection - _edit.center), y_axis.dot(intersection - _edit.center));

						if (_edit.snap || spatial_editor->is_snap_enabled()) {
							snap = spatial_editor->get_rotate_snap();
						}
						angle = Math::rad2deg(angle) + snap * 0.5; //else it won't reach +180
						angle -= Math::fmod(angle, snap);
						set_message(vformat(TTR("Rotating %s degrees."), String::num(angle, snap_step_decimals)));
						angle = Math::deg2rad(angle);

						List<Node *> &selection = editor_selection->get_selected_node_list();

						bool local_coords = (spatial_editor->are_local_coords_enabled() && _edit.plane != TRANSFORM_VIEW); // Disable local transformation for TRANSFORM_VIEW

						for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
							Spatial *sp = Object::cast_to<Spatial>(E->get());
							if (!sp) {
								continue;
							}

							SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
							if (!se) {
								continue;
							}

							if (sp->has_meta("_edit_lock_")) {
								continue;
							}

							Transform t;

							if (local_coords) {
								Transform original_local = se->original_local;
								Basis rot = Basis(axis, angle);

								t.basis = original_local.get_basis().orthonormalized() * rot;
								t.origin = original_local.origin;

								// Apply rotation
								sp->set_transform(t);
								sp->set_scale(original_local.basis.get_scale()); // re-apply original scale

							} else {
								Transform original = se->original;
								Transform r;
								Transform base = Transform(Basis(), _edit.center);

								r.basis.rotate(plane.normal, angle);
								t = base * r * base.inverse() * original;

								// Apply rotation
								sp->set_global_transform(t);
							}
						}

						surface->update();

					} break;
					default: {
					}
				}
			}

		} else if ((m->get_button_mask() & BUTTON_MASK_RIGHT) || freelook_active) {
			if (nav_scheme == NAVIGATION_MAYA && m->get_alt()) {
				nav_mode = NAVIGATION_ZOOM;
			} else if (freelook_active) {
				nav_mode = NAVIGATION_LOOK;
			} else if (orthogonal) {
				nav_mode = NAVIGATION_PAN;
			}

		} else if (m->get_button_mask() & BUTTON_MASK_MIDDLE) {
			const int mod = _get_key_modifier(m);
			if (nav_scheme == NAVIGATION_GODOT) {
				if (mod == _get_key_modifier_setting("editors/3d/navigation/pan_modifier")) {
					nav_mode = NAVIGATION_PAN;
				} else if (mod == _get_key_modifier_setting("editors/3d/navigation/zoom_modifier")) {
					nav_mode = NAVIGATION_ZOOM;
				} else if (mod == KEY_ALT || mod == _get_key_modifier_setting("editors/3d/navigation/orbit_modifier")) {
					// Always allow Alt as a modifier to better support graphic tablets.
					nav_mode = NAVIGATION_ORBIT;
				}

			} else if (nav_scheme == NAVIGATION_MAYA) {
				if (mod == _get_key_modifier_setting("editors/3d/navigation/pan_modifier")) {
					nav_mode = NAVIGATION_PAN;
				}
			}

		} else if (EditorSettings::get_singleton()->get("editors/3d/navigation/emulate_3_button_mouse")) {
			// Handle trackpad (no external mouse) use case
			const int mod = _get_key_modifier(m);

			if (mod) {
				if (mod == _get_key_modifier_setting("editors/3d/navigation/pan_modifier")) {
					nav_mode = NAVIGATION_PAN;
				} else if (mod == _get_key_modifier_setting("editors/3d/navigation/zoom_modifier")) {
					nav_mode = NAVIGATION_ZOOM;
				} else if (mod == KEY_ALT || mod == _get_key_modifier_setting("editors/3d/navigation/orbit_modifier")) {
					// Always allow Alt as a modifier to better support graphic tablets.
					nav_mode = NAVIGATION_ORBIT;
				}
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
		NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();
		NavigationMode nav_mode = NAVIGATION_NONE;

		if (nav_scheme == NAVIGATION_GODOT) {
			const int mod = _get_key_modifier(pan_gesture);

			if (mod == _get_key_modifier_setting("editors/3d/navigation/pan_modifier")) {
				nav_mode = NAVIGATION_PAN;
			} else if (mod == _get_key_modifier_setting("editors/3d/navigation/zoom_modifier")) {
				nav_mode = NAVIGATION_ZOOM;
			} else if (mod == KEY_ALT || mod == _get_key_modifier_setting("editors/3d/navigation/orbit_modifier")) {
				// Always allow Alt as a modifier to better support graphic tablets.
				nav_mode = NAVIGATION_ORBIT;
			}

		} else if (nav_scheme == NAVIGATION_MAYA) {
			if (pan_gesture->get_alt()) {
				nav_mode = NAVIGATION_PAN;
			}
		}

		switch (nav_mode) {
			case NAVIGATION_PAN: {
				_nav_pan(pan_gesture, pan_gesture->get_delta());

			} break;

			case NAVIGATION_ZOOM: {
				_nav_zoom(pan_gesture, pan_gesture->get_delta());

			} break;

			case NAVIGATION_ORBIT: {
				_nav_orbit(pan_gesture, pan_gesture->get_delta());

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

		if (_edit.mode == TRANSFORM_NONE && !cursor.region_select) {
			if (k->get_scancode() == KEY_ESCAPE) {
				_clear_selected();
				return;
			}
		}

		if (EditorSettings::get_singleton()->get("editors/3d/navigation/emulate_numpad")) {
			const uint32_t code = k->get_physical_scancode();
			if (code >= KEY_0 && code <= KEY_9) {
				k->set_scancode(code - KEY_0 + KEY_KP_0);
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
		// Orthgonal mode doesn't work in freelook.
		if (!freelook_active && ED_IS_SHORTCUT("spatial_editor/switch_perspective_orthogonal", p_event)) {
			_menu_option(orthogonal ? VIEW_PERSPECTIVE : VIEW_ORTHOGONAL);
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

			if (!AnimationPlayerEditor::singleton->get_track_editor()->has_keying()) {
				set_message(TTR("Keying is disabled (no key inserted)."));
				return;
			}

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *sp = Object::cast_to<Spatial>(E->get());
				if (!sp) {
					continue;
				}

				spatial_editor->emit_signal("transform_key_request", sp, "", sp->get_transform());
			}

			set_message(TTR("Animation Key Inserted."));
		}

		// Freelook doesn't work in orthogonal mode.
		if (!orthogonal && ED_IS_SHORTCUT("spatial_editor/freelook_toggle", p_event)) {
			set_freelook_active(!is_freelook_active());

		} else if (k->get_scancode() == KEY_ESCAPE) {
			set_freelook_active(false);
		}

		if (k->get_scancode() == KEY_SPACE) {
			if (!k->is_pressed()) {
				emit_signal("toggle_maximize_view", this);
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

void SpatialEditorViewport::_nav_pan(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	const NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();

	real_t pan_speed = 1 / 150.0;
	int pan_speed_modifier = 10;
	if (p_event.is_valid() && nav_scheme == NAVIGATION_MAYA && p_event->get_shift()) {
		pan_speed *= pan_speed_modifier;
	}

	Transform camera_transform;

	camera_transform.translate(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
	const bool invert_x_axis = EditorSettings::get_singleton()->get("editors/3d/navigation/invert_x_axis");
	const bool invert_y_axis = EditorSettings::get_singleton()->get("editors/3d/navigation/invert_y_axis");
	Vector3 translation(
			(invert_x_axis ? -1 : 1) * -p_relative.x * pan_speed,
			(invert_y_axis ? -1 : 1) * p_relative.y * pan_speed,
			0);
	translation *= cursor.distance / DISTANCE_DEFAULT;
	camera_transform.translate(translation);
	cursor.pos = camera_transform.origin;
}

void SpatialEditorViewport::_nav_zoom(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	const NavigationScheme nav_scheme = (NavigationScheme)EditorSettings::get_singleton()->get("editors/3d/navigation/navigation_scheme").operator int();

	real_t zoom_speed = 1 / 80.0;
	int zoom_speed_modifier = 10;
	if (p_event.is_valid() && nav_scheme == NAVIGATION_MAYA && p_event->get_shift()) {
		zoom_speed *= zoom_speed_modifier;
	}

	NavigationZoomStyle zoom_style = (NavigationZoomStyle)EditorSettings::get_singleton()->get("editors/3d/navigation/zoom_style").operator int();
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

void SpatialEditorViewport::_nav_orbit(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	if (lock_rotation) {
		_nav_pan(p_event, p_relative);
		return;
	}

	if (orthogonal && auto_orthogonal) {
		_menu_option(VIEW_PERSPECTIVE);
	}

	const real_t degrees_per_pixel = EditorSettings::get_singleton()->get("editors/3d/navigation_feel/orbit_sensitivity");
	const real_t radians_per_pixel = Math::deg2rad(degrees_per_pixel);
	const bool invert_y_axis = EditorSettings::get_singleton()->get("editors/3d/navigation/invert_y_axis");
	const bool invert_x_axis = EditorSettings::get_singleton()->get("editors/3d/navigation/invert_x_axis");

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

void SpatialEditorViewport::_nav_look(Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	if (orthogonal) {
		_nav_pan(p_event, p_relative);
		return;
	}

	if (orthogonal && auto_orthogonal) {
		_menu_option(VIEW_PERSPECTIVE);
	}

	// Scale mouse sensitivity with camera FOV scale when zoomed in to make it easier to point at things.
	const real_t degrees_per_pixel = real_t(EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_sensitivity")) * MIN(1.0, cursor.fov_scale);
	const real_t radians_per_pixel = Math::deg2rad(degrees_per_pixel);
	const bool invert_y_axis = EditorSettings::get_singleton()->get("editors/3d/navigation/invert_y_axis");

	// Note: do NOT assume the camera has the "current" transform, because it is interpolated and may have "lag".
	const Transform prev_camera_transform = to_camera_transform(cursor);

	if (invert_y_axis) {
		cursor.x_rot -= p_relative.y * radians_per_pixel;
	} else {
		cursor.x_rot += p_relative.y * radians_per_pixel;
	}
	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	cursor.x_rot = CLAMP(cursor.x_rot, -1.57, 1.57);

	cursor.y_rot += p_relative.x * radians_per_pixel;

	// Look is like the opposite of Orbit: the focus point rotates around the camera
	Transform camera_transform = to_camera_transform(cursor);
	Vector3 pos = camera_transform.xform(Vector3(0, 0, 0));
	Vector3 prev_pos = prev_camera_transform.xform(Vector3(0, 0, 0));
	Vector3 diff = prev_pos - pos;
	cursor.pos += diff;

	view_type = VIEW_TYPE_USER;
	_update_name();
}

void SpatialEditorViewport::set_freelook_active(bool active_now) {
	if (!freelook_active && active_now) {
		// Sync camera cursor to cursor to "cut" interpolation jumps due to changing referential
		cursor = camera_cursor;

		// Make sure eye_pos is synced, because freelook referential is eye pos rather than orbit pos
		Vector3 forward = to_camera_transform(cursor).basis.xform(Vector3(0, 0, -1));
		cursor.eye_pos = cursor.pos - cursor.distance * forward;
		// Also sync the camera cursor, otherwise switching to freelook will be trippy if inertia is active
		camera_cursor.eye_pos = cursor.eye_pos;

		if (EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_speed_zoom_link")) {
			// Re-adjust freelook speed from the current zoom level
			real_t base_speed = EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_base_speed");
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

void SpatialEditorViewport::scale_fov(real_t p_fov_offset) {
	cursor.fov_scale = CLAMP(cursor.fov_scale + p_fov_offset, 0.1, 2.5);
	surface->update();
}

void SpatialEditorViewport::reset_fov() {
	cursor.fov_scale = 1.0;
	surface->update();
}

void SpatialEditorViewport::scale_cursor_distance(real_t scale) {
	real_t min_distance = MAX(camera->get_znear() * 4, ZOOM_FREELOOK_MIN);
	real_t max_distance = MIN(camera->get_zfar() / 2, ZOOM_FREELOOK_MAX);
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
	surface->update();
}

void SpatialEditorViewport::scale_freelook_speed(real_t scale) {
	real_t min_speed = MAX(camera->get_znear() * 4, ZOOM_FREELOOK_MIN);
	real_t max_speed = MIN(camera->get_zfar() / 2, ZOOM_FREELOOK_MAX);
	if (unlikely(min_speed > max_speed)) {
		freelook_speed = (min_speed + max_speed) / 2;
	} else {
		freelook_speed = CLAMP(freelook_speed * scale, min_speed, max_speed);
	}

	zoom_indicator_delay = ZOOM_FREELOOK_INDICATOR_DELAY_S;
	surface->update();
}

Point2i SpatialEditorViewport::_get_warped_mouse_motion(const Ref<InputEventMouseMotion> &p_ev_mouse_motion) const {
	Point2i relative;
	if (bool(EDITOR_DEF("editors/3d/navigation/warped_mouse_panning", false))) {
		relative = Input::get_singleton()->warp_mouse_motion(p_ev_mouse_motion, surface->get_global_rect());
	} else {
		relative = p_ev_mouse_motion->get_relative();
	}
	return relative;
}

static bool is_shortcut_pressed(const String &p_path) {
	Ref<ShortCut> shortcut = ED_GET_SHORTCUT(p_path);
	if (shortcut.is_null()) {
		return false;
	}
	InputEventKey *k = Object::cast_to<InputEventKey>(shortcut->get_shortcut().ptr());
	if (k == nullptr) {
		return false;
	}
	const Input &input = *Input::get_singleton();
	int scancode = k->get_scancode();
	return input.is_key_pressed(scancode);
}

void SpatialEditorViewport::_update_freelook(real_t delta) {
	if (!is_freelook_active()) {
		return;
	}

	const FreelookNavigationScheme navigation_scheme = (FreelookNavigationScheme)EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_navigation_scheme").operator int();

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

	if (is_shortcut_pressed("spatial_editor/freelook_left")) {
		direction -= right;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_right")) {
		direction += right;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_forward")) {
		direction += forward;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_backwards")) {
		direction -= forward;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_up")) {
		direction += up;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_down")) {
		direction -= up;
	}

	real_t speed = freelook_speed;

	if (is_shortcut_pressed("spatial_editor/freelook_speed_modifier")) {
		speed *= 3.0;
	}
	if (is_shortcut_pressed("spatial_editor/freelook_slow_modifier")) {
		speed *= 0.333333;
	}

	const Vector3 motion = direction * speed * delta;
	cursor.pos += motion;
	cursor.eye_pos += motion;
}

void SpatialEditorViewport::set_message(String p_message, float p_time) {
	message = p_message;
	message_time = p_time;
}

void SpatialEditorPlugin::edited_scene_changed() {
	for (uint32_t i = 0; i < SpatialEditor::VIEWPORTS_COUNT; i++) {
		SpatialEditorViewport *viewport = SpatialEditor::get_singleton()->get_editor_viewport(i);
		if (viewport->is_visible()) {
			viewport->notification(Control::NOTIFICATION_VISIBILITY_CHANGED);
		}
	}
}

void SpatialEditorViewport::_project_settings_changed() {
	if (viewport) {
		_project_settings_change_pending = false;

		//update shadow atlas if changed
		int shadowmap_size = ProjectSettings::get_singleton()->get("rendering/quality/shadow_atlas/size");
		int atlas_q0 = ProjectSettings::get_singleton()->get("rendering/quality/shadow_atlas/quadrant_0_subdiv");
		int atlas_q1 = ProjectSettings::get_singleton()->get("rendering/quality/shadow_atlas/quadrant_1_subdiv");
		int atlas_q2 = ProjectSettings::get_singleton()->get("rendering/quality/shadow_atlas/quadrant_2_subdiv");
		int atlas_q3 = ProjectSettings::get_singleton()->get("rendering/quality/shadow_atlas/quadrant_3_subdiv");

		viewport->set_shadow_atlas_size(shadowmap_size);
		viewport->set_shadow_atlas_quadrant_subdiv(0, Viewport::ShadowAtlasQuadrantSubdiv(atlas_q0));
		viewport->set_shadow_atlas_quadrant_subdiv(1, Viewport::ShadowAtlasQuadrantSubdiv(atlas_q1));
		viewport->set_shadow_atlas_quadrant_subdiv(2, Viewport::ShadowAtlasQuadrantSubdiv(atlas_q2));
		viewport->set_shadow_atlas_quadrant_subdiv(3, Viewport::ShadowAtlasQuadrantSubdiv(atlas_q3));

		// Update MSAA, FXAA, transparent background, debanding, sharpening and HDR if changed.
		int msaa_mode = ProjectSettings::get_singleton()->get("rendering/quality/filters/msaa");
		viewport->set_msaa(Viewport::MSAA(msaa_mode));

		bool use_fxaa = ProjectSettings::get_singleton()->get("rendering/quality/filters/use_fxaa");
		viewport->set_use_fxaa(use_fxaa);

		const bool transparent_background = GLOBAL_GET("rendering/viewport/transparent_background");
		viewport->set_transparent_background(transparent_background);

		bool use_debanding = ProjectSettings::get_singleton()->get("rendering/quality/filters/use_debanding");
		viewport->set_use_debanding(use_debanding);

		float sharpen_intensity = ProjectSettings::get_singleton()->get("rendering/quality/filters/sharpen_intensity");
		viewport->set_sharpen_intensity(sharpen_intensity);

		bool hdr = ProjectSettings::get_singleton()->get("rendering/quality/depth/hdr");
		viewport->set_hdr(hdr);

		const bool use_32_bpc_depth = ProjectSettings::get_singleton()->get("rendering/quality/depth/use_32_bpc_depth");
		viewport->set_use_32_bpc_depth(use_32_bpc_depth);

	} else {
		// Could not update immediately, set a pending update.
		// This may never happen, but is included for safety
		_project_settings_change_pending = true;
	}
}

void SpatialEditorViewport::_notification(int p_what) {
	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		bool visible = is_visible_in_tree();

		set_process(visible);

		if (visible) {
			orthogonal = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL));
			_update_name();
			_update_camera(0);
		} else {
			set_freelook_active(false);
		}

		call_deferred("update_transform_gizmo_view");

		if (camera) {
			camera->set_affect_lod(visible);
		}
	}

	if (p_what == NOTIFICATION_RESIZED) {
		call_deferred("update_transform_gizmo_view");
	}

	if (p_what == NOTIFICATION_PROCESS) {
		real_t delta = get_process_delta_time();

		if (zoom_indicator_delay > 0) {
			zoom_indicator_delay -= delta;
			if (zoom_indicator_delay <= 0) {
				surface->update();
				zoom_limit_label->hide();
			}
		}

		_update_navigation_controls_visibility();
		_update_freelook(delta);

		Node *scene_root = editor->get_scene_tree_dock()->get_editor_data()->get_edited_scene_root();
		if (previewing_cinema && scene_root != nullptr) {
			Camera *cam = scene_root->get_viewport()->get_camera();
			if (cam != nullptr && cam != previewing) {
				//then switch the viewport's camera to the scene's viewport camera
				if (previewing != nullptr) {
					previewing->disconnect("tree_exited", this, "_preview_exited_scene");
				}
				previewing = cam;
				previewing->connect("tree_exited", this, "_preview_exited_scene");
				VS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), cam->get_camera());
				surface->update();
			}
		}

		_update_camera(delta);

		bool show_selected_info = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_SELECTED_INFO)) && (message_time <= 0);

		struct SelectedInfo {
			bool filled = false;
			uint32_t tri_count = 0;
			uint32_t vertex_count = 0;
			uint32_t index_count = 0;
			uint32_t mesh_count = 0;
			uint32_t multimesh_count = 0;
			uint32_t surface_count = 0;
			uint32_t array_format = 0;
		} selected_info;

		Map<Node *, Object *> &selection = editor_selection->get_selection();

		bool changed = false;
		bool exist = false;

		for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {
			Spatial *sp = Object::cast_to<Spatial>(E->key());
			if (!sp) {
				continue;
			}

			// Only retrieve stats for selected items if we are currently showing the selection stats box.
			if (show_selected_info) {
				MeshInstance *mi = Object::cast_to<MeshInstance>(sp);
				if (mi) {
					const Ref<Mesh> &mesh = mi->get_mesh();
					if (mesh.is_valid()) {
						selected_info.filled = true;
						const Mesh::CachedStats &stats = mesh->get_cached_stats();
						selected_info.tri_count += stats.triangle_count;
						selected_info.vertex_count += stats.vertex_count;
						selected_info.index_count += stats.index_count;
						selected_info.mesh_count += 1;
						selected_info.surface_count += mesh->get_surface_count();
						selected_info.array_format |= stats.array_format;
					}
				}

				MultiMeshInstance *mmi = Object::cast_to<MultiMeshInstance>(sp);
				if (mmi) {
					const Ref<MultiMesh> &mm = mmi->get_multimesh();
					if (mm.is_valid()) {
						const Ref<Mesh> &mesh = mm->get_mesh();
						int icount = mm->get_visible_instance_count();
						if (icount < 0) {
							icount = mm->get_instance_count();
						}

						if (mesh.is_valid() && icount) {
							selected_info.filled = true;
							const Mesh::CachedStats &stats = mesh->get_cached_stats();
							selected_info.tri_count += stats.triangle_count * icount;
							selected_info.vertex_count += stats.vertex_count * icount;
							selected_info.index_count += stats.index_count * icount;
							selected_info.mesh_count += icount;
							selected_info.multimesh_count += 1;
							selected_info.surface_count += mesh->get_surface_count() * icount;
							selected_info.array_format |= stats.array_format;
						}
					}
				}
			}

			SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
			if (!se) {
				continue;
			}

			Transform t = sp->get_global_gizmo_transform();
			VisualInstance *vi = Object::cast_to<VisualInstance>(sp);
			AABB new_aabb = vi ? vi->get_aabb() : _calculate_spatial_bounds(sp);

			exist = true;
			if (se->last_xform == t && se->aabb == new_aabb && !se->last_xform_dirty) {
				continue;
			}
			changed = true;
			se->last_xform_dirty = false;
			se->last_xform = t;

			se->aabb = new_aabb;

			Transform t_offset = t;

			// apply AABB scaling before item's global transform
			{
				const Vector3 offset(0.005, 0.005, 0.005);
				Basis aabb_s;
				aabb_s.scale(se->aabb.size + offset);
				t.translate(se->aabb.position - offset / 2);
				t.basis = t.basis * aabb_s;
			}
			{
				const Vector3 offset(0.01, 0.01, 0.01);
				Basis aabb_s;
				aabb_s.scale(se->aabb.size + offset);
				t_offset.translate(se->aabb.position - offset / 2);
				t_offset.basis = t_offset.basis * aabb_s;
			}

			VisualServer::get_singleton()->instance_set_transform(se->sbox_instance, t);
			VisualServer::get_singleton()->instance_set_transform(se->sbox_instance_offset, t_offset);
			VisualServer::get_singleton()->instance_set_transform(se->sbox_instance_xray, t);
			VisualServer::get_singleton()->instance_set_transform(se->sbox_instance_xray_offset, t_offset);
		}

		if (changed || (spatial_editor->is_gizmo_visible() && !exist)) {
			spatial_editor->update_transform_gizmo();
		}

		if (message_time > 0) {
			if (message != last_message) {
				surface->update();
				last_message = message;

				// If there is now no message,
				// disable the timing counter.
				if (message == "") {
					message_time = 0;
				}
			} else {
				message_time -= get_physics_process_delta_time();
				if (message_time < 0) {
					surface->update();
				}
			}
		}

		if (_project_settings_change_pending) {
			_project_settings_changed();
		}

		bool shrink = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION));

		if (shrink != (viewport_container->get_stretch_shrink() > 1)) {
			viewport_container->set_stretch_shrink(shrink ? 2 : 1);
		}

		bool show_info = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_INFORMATION));
		info_label->set_visible(show_info);

		Camera *current_camera;

		if (previewing) {
			current_camera = previewing;
		} else {
			current_camera = camera;
		}

		if (show_info) {
			const String viewport_size = vformat(String::utf8("%d × %d"), viewport->get_size().x, viewport->get_size().y);
			String text;
			text += "X: " + rtos(current_camera->get_translation().x).pad_decimals(1) + "\n";
			text += "Y: " + rtos(current_camera->get_translation().y).pad_decimals(1) + "\n";
			text += "Z: " + rtos(current_camera->get_translation().z).pad_decimals(1) + "\n";
			text += TTR("Pitch:") + " " + itos(Math::round(current_camera->get_rotation_degrees().x)) + "\n";
			text += TTR("Yaw:") + " " + itos(Math::round(current_camera->get_rotation_degrees().y)) + "\n\n";

			text += TTR("Size:") +
					vformat(
							" %s (%.1fMP)\n",
							viewport_size,
							viewport->get_size().x * viewport->get_size().y * 0.000'001);
			text += TTR("Objects Drawn:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_OBJECTS_IN_FRAME)) + "\n";
			text += TTR("Material Changes:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_MATERIAL_CHANGES_IN_FRAME)) + "\n";
			text += TTR("Shader Changes:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_SHADER_CHANGES_IN_FRAME)) + "\n";
			text += TTR("Surface Changes:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_SURFACE_CHANGES_IN_FRAME)) + "\n";
			text += TTR("Draw Calls:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_DRAW_CALLS_IN_FRAME)) + "\n";
			text += TTR("Vertices:") + " " + itos(viewport->get_render_info(Viewport::RENDER_INFO_VERTICES_IN_FRAME));
			info_label->set_text(text);
		}

		selected_info_label->set_visible(show_selected_info && selected_info.filled);
		if (show_selected_info) {
			if (selected_info.filled) {
				String text;
				if (selected_info.multimesh_count > 0) {
					text += TTR("MultiMeshes:") + " " + itos(selected_info.multimesh_count) + "\n";
				}
				if (selected_info.mesh_count > 1) {
					text += TTR("Meshes:") + " " + itos(selected_info.mesh_count) + "\n";
				}
				if (selected_info.surface_count > 1) {
					text += TTR("Surfaces:") + " " + itos(selected_info.surface_count) + "\n";
				}
				text += TTR("Triangles:") + " " + itos(selected_info.tri_count) + "\n";
				text += TTR("Vertices:") + " " + itos(selected_info.vertex_count) + "\n";
				text += TTR("Indices:") + " " + itos(selected_info.index_count);

				selected_info_label->set_text(text);
			} else {
				selected_info_label->set_text("");
			}
		}

		// FPS Counter.
		bool show_fps = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_FPS));
		fps_label->set_visible(show_fps);

		if (show_fps) {
			const float fps = Engine::get_singleton()->get_frames_per_second();
			fps_label->set_text(vformat(TTR("FPS: %d (%s ms)"), fps, rtos(1000.0f / fps).pad_decimals(2)));
			// Middle point is at 60 FPS.
			fps_label->add_color_override(
					"font_color",
					frame_time_gradient->get_color_at_offset(
							Math::range_lerp(fps, 110, 10, 0, 1)));
		}

		bool show_cinema = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW));
		cinema_label->set_visible(show_cinema);
		if (show_cinema) {
			float cinema_half_width = cinema_label->get_size().width / 2.0f;
			cinema_label->set_anchor_and_margin(MARGIN_LEFT, 0.5f, -cinema_half_width);
		}

		if (lock_rotation) {
			float locked_half_width = locked_label->get_size().width / 2.0f;
			locked_label->set_anchor_and_margin(MARGIN_LEFT, 0.5f, -locked_half_width);
		}
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {
		surface->connect("draw", this, "_draw");
		surface->connect("gui_input", this, "_sinput");
		surface->connect("mouse_entered", this, "_surface_mouse_enter");
		surface->connect("mouse_exited", this, "_surface_mouse_exit");
		surface->connect("focus_entered", this, "_surface_focus_enter");
		surface->connect("focus_exited", this, "_surface_focus_exit");

		// Ensure we are up to date with project settings
		_project_settings_changed();

		// Any further changes to project settings get a signal
		ProjectSettings::get_singleton()->connect("project_settings_changed", this, "_project_settings_changed");

		_init_gizmo_instance(index);
	}

	if (p_what == NOTIFICATION_EXIT_TREE) {
		ProjectSettings::get_singleton()->disconnect("project_settings_changed", this, "_project_settings_changed");
		_finish_gizmo_instances();
	}

	if (p_what == NOTIFICATION_THEME_CHANGED) {
		view_menu->set_icon(get_icon("GuiTabMenuHl", "EditorIcons"));
		preview_camera->set_icon(get_icon("Camera", "EditorIcons"));

		view_menu->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		view_menu->add_style_override("hover", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		view_menu->add_style_override("pressed", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		view_menu->add_style_override("focus", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		view_menu->add_style_override("disabled", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));

		preview_camera->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		preview_camera->add_style_override("hover", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		preview_camera->add_style_override("pressed", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		preview_camera->add_style_override("focus", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		preview_camera->add_style_override("disabled", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));

		info_label->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		selected_info_label->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		fps_label->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		cinema_label->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));
		locked_label->add_style_override("normal", editor->get_gui_base()->get_stylebox("Information3dViewport", "EditorStyles"));

		frame_time_gradient->set_color(0, get_color("success_color", "Editor"));
		frame_time_gradient->set_color(1, get_color("warning_color", "Editor"));
		frame_time_gradient->set_color(2, get_color("error_color", "Editor"));
	}
}

static void draw_indicator_bar(Control &p_surface, real_t p_fill, const Ref<Texture> p_icon, const Ref<Font> p_font, const String &p_text, const Color &p_color) {
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

	// Draw a shadow for the text to make it easier to read.
	p_surface.draw_string(p_font, Vector2(icon_pos.x + EDSCALE, icon_pos.y + icon_size.y + 17 * EDSCALE), p_text, Color(0, 0, 0));
	// Draw text below the bar (for speed/zoom information).
	p_surface.draw_string(p_font, Vector2(icon_pos.x, icon_pos.y + icon_size.y + 16 * EDSCALE), p_text, p_color);
}

void SpatialEditorViewport::_draw() {
	EditorPluginList *over_plugin_list = EditorNode::get_singleton()->get_editor_plugins_over();
	if (!over_plugin_list->empty()) {
		over_plugin_list->forward_spatial_draw_over_viewport(surface);
	}

	EditorPluginList *force_over_plugin_list = editor->get_editor_plugins_force_over();
	if (!force_over_plugin_list->empty()) {
		force_over_plugin_list->forward_spatial_force_draw_over_viewport(surface);
	}

	if (surface->has_focus()) {
		Size2 size = surface->get_size();
		Rect2 r = Rect2(Point2(), size);
		get_stylebox("Focus", "EditorStyles")->draw(surface->get_canvas_item(), r);
	}

	if (cursor.region_select) {
		const Rect2 selection_rect = Rect2(cursor.region_begin, cursor.region_end - cursor.region_begin);

		surface->draw_rect(
				selection_rect,
				get_color("box_selection_fill_color", "Editor"));

		surface->draw_rect(
				selection_rect,
				get_color("box_selection_stroke_color", "Editor"),
				false,
				Math::round(EDSCALE));
	}

	RID ci = surface->get_canvas_item();

	if (message_time > 0) {
		Ref<Font> font = get_font("font", "Label");
		Point2 msgpos = Point2(5, get_size().y - 20);
		font->draw(ci, msgpos + Point2(1, 1), message, Color(0, 0, 0, 0.8));
		font->draw(ci, msgpos + Point2(-1, -1), message, Color(0, 0, 0, 0.8));
		font->draw(ci, msgpos, message, Color(1, 1, 1, 1));
	}

	if (_edit.mode == TRANSFORM_ROTATE) {
		Point2 center = _point_to_screen(_edit.center);

		Color handle_color;
		switch (_edit.plane) {
			case TRANSFORM_X_AXIS:
				handle_color = get_color("axis_x_color", "Editor");
				break;
			case TRANSFORM_Y_AXIS:
				handle_color = get_color("axis_y_color", "Editor");
				break;
			case TRANSFORM_Z_AXIS:
				handle_color = get_color("axis_z_color", "Editor");
				break;
			default:
				handle_color = get_color("accent_color", "Editor");
				break;
		}
		handle_color = handle_color.from_hsv(handle_color.get_h(), 0.25, 1.0, 1);

		VisualServer::get_singleton()->canvas_item_add_line(
				ci,
				_edit.mouse_pos,
				center,
				handle_color,
				Math::round(2 * EDSCALE),
				true);
	}
	if (previewing) {
		Size2 ss = Size2(ProjectSettings::get_singleton()->get("display/window/size/width"), ProjectSettings::get_singleton()->get("display/window/size/height"));
		float aspect = ss.aspect();
		Size2 s = get_size();

		Rect2 draw_rect;

		switch (previewing->get_keep_aspect_mode()) {
			case Camera::KEEP_WIDTH: {
				draw_rect.size = Size2(s.width, s.width / aspect);
				draw_rect.position.x = 0;
				draw_rect.position.y = (s.height - draw_rect.size.y) * 0.5;

			} break;
			case Camera::KEEP_HEIGHT: {
				draw_rect.size = Size2(s.height * aspect, s.height);
				draw_rect.position.y = 0;
				draw_rect.position.x = (s.width - draw_rect.size.x) * 0.5;

			} break;
		}

		draw_rect = Rect2(Vector2(), s).clip(draw_rect);

		surface->draw_rect(draw_rect, Color(0.6, 0.6, 0.1, 0.5), false, Math::round(2 * EDSCALE));

	} else {
		if (zoom_indicator_delay > 0.0) {
			if (is_freelook_active()) {
				// Show speed

				real_t min_speed = MAX(camera->get_znear() * 4, ZOOM_FREELOOK_MIN);
				real_t max_speed = MIN(camera->get_zfar() / 2, ZOOM_FREELOOK_MAX);
				real_t scale_length = (max_speed - min_speed);

				if (!Math::is_zero_approx(scale_length)) {
					real_t logscale_t = 1.0 - Math::log(1 + freelook_speed - min_speed) / Math::log(1 + scale_length);

					// Display the freelook speed to help the user get a better sense of scale.
					const int precision = freelook_speed < 1.0 ? 2 : 1;
					draw_indicator_bar(
							*surface,
							1.0 - logscale_t,
							get_icon("ViewportSpeed", "EditorIcons"),
							get_font("font", "Label"),
							vformat("%s u/s", String::num(freelook_speed).pad_decimals(precision)),
							Color(1.0, 0.95, 0.7));
				}

			} else {
				// Show zoom
				zoom_limit_label->set_visible(zoom_failed_attempts_count > 15);

				real_t min_distance = MAX(camera->get_znear() * 4, ZOOM_FREELOOK_MIN);
				real_t max_distance = MIN(camera->get_zfar() / 2, ZOOM_FREELOOK_MAX);
				real_t scale_length = (max_distance - min_distance);

				if (!Math::is_zero_approx(scale_length)) {
					real_t logscale_t = 1.0 - Math::log(1 + cursor.distance - min_distance) / Math::log(1 + scale_length);

					// Display the zoom center distance to help the user get a better sense of scale.
					const int precision = cursor.distance < 1.0 ? 2 : 1;
					draw_indicator_bar(
							*surface,
							logscale_t,
							get_icon("ViewportZoom", "EditorIcons"),
							get_font("font", "Label"),
							vformat("%s u", String::num(cursor.distance).pad_decimals(precision)),
							Color(0.7, 0.95, 1.0));
				}
			}
		}
	}
}

void SpatialEditorViewport::_menu_option(int p_option) {
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
			cursor.y_rot = Math_PI;
			set_message(TTR("Front View."), 2);
			view_type = VIEW_TYPE_FRONT;
			_set_auto_orthogonal();
			_update_name();

		} break;
		case VIEW_REAR: {
			cursor.x_rot = 0;
			cursor.y_rot = 0;
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

			Transform camera_transform = camera->get_global_transform();

			List<Node *> &selection = editor_selection->get_selected_node_list();

			undo_redo->create_action(TTR("Align Transform with View"));

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *sp = Object::cast_to<Spatial>(E->get());
				if (!sp) {
					continue;
				}

				SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
				if (!se) {
					continue;
				}

				Transform xform;
				if (orthogonal) {
					xform = sp->get_global_transform();
					xform.basis.set_euler(camera_transform.basis.get_euler());
				} else {
					xform = camera_transform;
					xform.scale_basis(sp->get_scale());
				}

				undo_redo->add_do_method(sp, "set_global_transform", xform);
				undo_redo->add_undo_method(sp, "set_global_transform", sp->get_global_gizmo_transform());
			}
			undo_redo->commit_action();

		} break;
		case VIEW_ALIGN_ROTATION_WITH_VIEW: {
			if (!get_selected_count()) {
				break;
			}

			Transform camera_transform = camera->get_global_transform();

			List<Node *> &selection = editor_selection->get_selected_node_list();

			undo_redo->create_action(TTR("Align Rotation with View"));
			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *sp = Object::cast_to<Spatial>(E->get());
				if (!sp) {
					continue;
				}

				SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
				if (!se) {
					continue;
				}

				undo_redo->add_do_method(sp, "set_rotation", camera_transform.basis.get_rotation());
				undo_redo->add_undo_method(sp, "set_rotation", sp->get_rotation());
			}
			undo_redo->commit_action();

		} break;
		case VIEW_ENVIRONMENT: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			if (current) {
				camera->set_environment(RES());
			} else {
				camera->set_environment(SpatialEditor::get_singleton()->get_viewport_environment());
			}

			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_PERSPECTIVE: {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), false);
			orthogonal = false;
			auto_orthogonal = false;
			call_deferred("update_transform_gizmo_view");
			_update_name();

		} break;
		case VIEW_ORTHOGONAL: {
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ORTHOGONAL), true);
			orthogonal = true;
			auto_orthogonal = false;
			call_deferred("update_transform_gizmo_view");
			_update_name();

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
			lock_rotation = !current;
			view_menu->get_popup()->set_item_checked(idx, !current);
			if (lock_rotation) {
				locked_label->show();
			} else {
				locked_label->hide();
			}

		} break;
		case VIEW_AUDIO_LISTENER: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			viewport->set_as_audio_listener(current);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_AUDIO_DOPPLER: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			camera->set_doppler_tracking(current ? Camera::DOPPLER_TRACKING_IDLE_STEP : Camera::DOPPLER_TRACKING_DISABLED);
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
			uint32_t layers = ((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + index)) | (1 << GIZMO_GRID_LAYER) | (1 << MISC_TOOL_LAYER);
			if (current) {
				layers |= (1 << GIZMO_EDIT_LAYER);
			}
			camera->set_cull_mask(layers);
			view_menu->get_popup()->set_item_checked(idx, current);

		} break;
		case VIEW_HALF_RESOLUTION: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			current = !current;
			view_menu->get_popup()->set_item_checked(idx, current);
		} break;
		case VIEW_INFORMATION: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_INFORMATION);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);

		} break;
		case VIEW_SELECTED_INFO: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_SELECTED_INFO);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);

		} break;
		case VIEW_FPS: {
			int idx = view_menu->get_popup()->get_item_index(VIEW_FPS);
			bool current = view_menu->get_popup()->is_item_checked(idx);
			view_menu->get_popup()->set_item_checked(idx, !current);

		} break;
		case VIEW_DISPLAY_NORMAL: {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_DISABLED);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING), false);
		} break;
		case VIEW_DISPLAY_WIREFRAME: {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_WIREFRAME);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING), false);
		} break;
		case VIEW_DISPLAY_OVERDRAW: {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_OVERDRAW);
			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_OVERDRAW);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING), false);

		} break;
		case VIEW_DISPLAY_SHADELESS: {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_UNSHADED);
			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_SHADELESS);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING), false);
		} break;
		case VIEW_DISPLAY_LIGHTING: {
			viewport->set_debug_draw(Viewport::DEBUG_DRAW_LIGHTING);
			VisualServer::get_singleton()->scenario_set_debug(get_tree()->get_root()->get_world()->get_scenario(), VisualServer::SCENARIO_DEBUG_LIGHTING);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING), true);
		} break;
	}
}

void SpatialEditorViewport::_set_auto_orthogonal() {
	if (!orthogonal && view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL))) {
		_menu_option(VIEW_ORTHOGONAL);
		auto_orthogonal = true;
	}
}

void SpatialEditorViewport::_preview_exited_scene() {
	preview_camera->disconnect("toggled", this, "_toggle_camera_preview");
	preview_camera->set_pressed(false);
	_toggle_camera_preview(false);
	preview_camera->connect("toggled", this, "_toggle_camera_preview");
	view_menu->show();
}

void SpatialEditorViewport::_init_gizmo_instance(int p_idx) {
	uint32_t layer = 1 << (GIZMO_BASE_LAYER + p_idx);

	for (int i = 0; i < 3; i++) {
		move_gizmo_instance[i] = RID_PRIME(VS::get_singleton()->instance_create());
		VS::get_singleton()->instance_set_base(move_gizmo_instance[i], spatial_editor->get_move_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(move_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_visible(move_gizmo_instance[i], false);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(move_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(move_gizmo_instance[i], layer);
		VS::get_singleton()->instance_set_portal_mode(move_gizmo_instance[i], VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);

		move_plane_gizmo_instance[i] = RID_PRIME(VS::get_singleton()->instance_create());
		VS::get_singleton()->instance_set_base(move_plane_gizmo_instance[i], spatial_editor->get_move_plane_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(move_plane_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], false);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(move_plane_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(move_plane_gizmo_instance[i], layer);
		VS::get_singleton()->instance_set_portal_mode(move_plane_gizmo_instance[i], VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);

		rotate_gizmo_instance[i] = RID_PRIME(VS::get_singleton()->instance_create());
		VS::get_singleton()->instance_set_base(rotate_gizmo_instance[i], spatial_editor->get_rotate_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(rotate_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], false);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(rotate_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(rotate_gizmo_instance[i], layer);
		VS::get_singleton()->instance_set_portal_mode(rotate_gizmo_instance[i], VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);

		scale_gizmo_instance[i] = RID_PRIME(VS::get_singleton()->instance_create());
		VS::get_singleton()->instance_set_base(scale_gizmo_instance[i], spatial_editor->get_scale_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(scale_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_visible(scale_gizmo_instance[i], false);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(scale_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(scale_gizmo_instance[i], layer);
		VS::get_singleton()->instance_set_portal_mode(scale_gizmo_instance[i], VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);

		scale_plane_gizmo_instance[i] = RID_PRIME(VS::get_singleton()->instance_create());
		VS::get_singleton()->instance_set_base(scale_plane_gizmo_instance[i], spatial_editor->get_scale_plane_gizmo(i)->get_rid());
		VS::get_singleton()->instance_set_scenario(scale_plane_gizmo_instance[i], get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], false);
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(scale_plane_gizmo_instance[i], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(scale_plane_gizmo_instance[i], layer);
		VS::get_singleton()->instance_set_portal_mode(scale_plane_gizmo_instance[i], VisualServer::INSTANCE_PORTAL_MODE_GLOBAL);
	}

	// Rotation white outline
	rotate_gizmo_instance[3] = RID_PRIME(VS::get_singleton()->instance_create());
	VS::get_singleton()->instance_set_base(rotate_gizmo_instance[3], spatial_editor->get_rotate_gizmo(3)->get_rid());
	VS::get_singleton()->instance_set_scenario(rotate_gizmo_instance[3], get_tree()->get_root()->get_world()->get_scenario());
	VS::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], false);
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(rotate_gizmo_instance[3], VS::SHADOW_CASTING_SETTING_OFF);
	VS::get_singleton()->instance_set_layer_mask(rotate_gizmo_instance[3], layer);
}

void SpatialEditorViewport::_finish_gizmo_instances() {
	for (int i = 0; i < 3; i++) {
		if (move_gizmo_instance[i].is_valid()) {
			VS::get_singleton()->free(move_gizmo_instance[i]);
			move_gizmo_instance[i] = RID();
		}

		if (move_plane_gizmo_instance[i].is_valid()) {
			VS::get_singleton()->free(move_plane_gizmo_instance[i]);
			move_plane_gizmo_instance[i] = RID();
		}

		if (rotate_gizmo_instance[i].is_valid()) {
			VS::get_singleton()->free(rotate_gizmo_instance[i]);
			rotate_gizmo_instance[i] = RID();
		}

		if (scale_gizmo_instance[i].is_valid()) {
			VS::get_singleton()->free(scale_gizmo_instance[i]);
			scale_gizmo_instance[i] = RID();
		}

		if (scale_plane_gizmo_instance[i].is_valid()) {
			VS::get_singleton()->free(scale_plane_gizmo_instance[i]);
			scale_plane_gizmo_instance[i] = RID();
		}
	}

	// Rotation white outline. All of the arrays above have 3 elements, this has 4.
	if (rotate_gizmo_instance[3].is_valid()) {
		VS::get_singleton()->free(rotate_gizmo_instance[3]);
		rotate_gizmo_instance[3] = RID();
	}
}

void SpatialEditorViewport::_toggle_camera_preview(bool p_activate) {
	ERR_FAIL_COND(p_activate && !preview);
	ERR_FAIL_COND(!p_activate && !previewing);

	previewing_camera = p_activate;
	_update_navigation_controls_visibility();

	if (!p_activate) {
		previewing->disconnect("tree_exiting", this, "_preview_exited_scene");
		previewing = nullptr;
		VS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), camera->get_camera()); //restore
		if (!preview) {
			preview_camera->hide();
		}
		surface->update();

	} else {
		previewing = preview;
		previewing->connect("tree_exiting", this, "_preview_exited_scene");
		VS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), preview->get_camera()); //replace
		surface->update();
	}
}

void SpatialEditorViewport::_toggle_cinema_preview(bool p_activate) {
	previewing_cinema = p_activate;
	_update_navigation_controls_visibility();

	if (!previewing_cinema) {
		if (previewing != nullptr) {
			previewing->disconnect("tree_exited", this, "_preview_exited_scene");
		}

		previewing = nullptr;
		VS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), camera->get_camera()); //restore
		preview_camera->set_pressed(false);
		if (!preview) {
			preview_camera->hide();
		} else {
			preview_camera->show();
		}
		view_menu->show();
		surface->update();
	}
}

void SpatialEditorViewport::_selection_result_pressed(int p_result) {
	if (selection_results.size() <= p_result) {
		return;
	}

	clicked = selection_results[p_result].item->get_instance_id();

	if (clicked) {
		_select_clicked(clicked_wants_append, true, spatial_editor->get_tool_mode() != SpatialEditor::TOOL_MODE_LIST_SELECT);
		clicked = 0;
	}
}

void SpatialEditorViewport::_selection_menu_hide() {
	selection_results.clear();
	selection_menu->clear();
	selection_menu->set_size(Vector2(0, 0));
}

void SpatialEditorViewport::set_can_preview(Camera *p_preview) {
	preview = p_preview;

	if (!preview_camera->is_pressed() && !previewing_cinema) {
		preview_camera->set_visible(p_preview);
	}
}

void SpatialEditorViewport::update_transform_gizmo_view() {
	if (!is_visible_in_tree()) {
		return;
	}

	Transform xform = spatial_editor->get_gizmo_transform();

	Transform camera_xform = camera->get_transform();

	if (xform.origin.distance_squared_to(camera_xform.origin) < 0.01) {
		for (int i = 0; i < 3; i++) {
			VisualServer::get_singleton()->instance_set_visible(move_gizmo_instance[i], false);
			VisualServer::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], false);
			VisualServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], false);
			VisualServer::get_singleton()->instance_set_visible(scale_gizmo_instance[i], false);
			VisualServer::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], false);
		}
		// Rotation white outline
		VisualServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], false);

		return;
	}

	Vector3 camz = -camera_xform.get_basis().get_axis(2).normalized();
	Vector3 camy = -camera_xform.get_basis().get_axis(1).normalized();
	Plane p(camera_xform.origin, camz);
	float gizmo_d = MAX(Math::abs(p.distance_to(xform.origin)), CMP_EPSILON);
	float d0 = camera->unproject_position(camera_xform.origin + camz * gizmo_d).y;
	float d1 = camera->unproject_position(camera_xform.origin + camz * gizmo_d + camy).y;
	float dd = Math::abs(d0 - d1);
	if (dd == 0) {
		dd = 0.0001;
	}

	float gizmo_size = EditorSettings::get_singleton()->get("editors/3d/manipulator_gizmo_size");
	// At low viewport heights, multiply the gizmo scale based on the viewport height.
	// This prevents the gizmo from growing very large and going outside the viewport.
	const int viewport_base_height = 400 * MAX(1, EDSCALE);
	gizmo_scale =
			(gizmo_size / Math::abs(dd)) * MAX(1, EDSCALE) *
			MIN(viewport_base_height, viewport_container->get_size().height) / viewport_base_height /
			viewport_container->get_stretch_shrink();
	Vector3 scale = Vector3(1, 1, 1) * gizmo_scale;

	xform.basis.scale(scale);

	for (int i = 0; i < 3; i++) {
		VisualServer::get_singleton()->instance_set_transform(move_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_set_visible(move_gizmo_instance[i], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE));
		VisualServer::get_singleton()->instance_set_transform(move_plane_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_set_visible(move_plane_gizmo_instance[i], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_MOVE));
		VisualServer::get_singleton()->instance_set_transform(rotate_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[i], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE));
		VisualServer::get_singleton()->instance_set_transform(scale_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_set_visible(scale_gizmo_instance[i], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SCALE));
		VisualServer::get_singleton()->instance_set_transform(scale_plane_gizmo_instance[i], xform);
		VisualServer::get_singleton()->instance_set_visible(scale_plane_gizmo_instance[i], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SCALE));
	}
	// Rotation white outline
	VisualServer::get_singleton()->instance_set_transform(rotate_gizmo_instance[3], xform);
	VisualServer::get_singleton()->instance_set_visible(rotate_gizmo_instance[3], spatial_editor->is_gizmo_visible() && (spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_SELECT || spatial_editor->get_tool_mode() == SpatialEditor::TOOL_MODE_ROTATE));
}

void SpatialEditorViewport::set_state(const Dictionary &p_state) {
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

	if (p_state.has("use_orthogonal")) {
		bool orth = p_state["use_orthogonal"];

		if (orth) {
			_menu_option(VIEW_ORTHOGONAL);
		} else {
			_menu_option(VIEW_PERSPECTIVE);
		}
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
		if (!view_menu->get_popup()->is_item_checked(idx)) {
			_menu_option(display);
		}
	}
	if (p_state.has("lock_rotation")) {
		lock_rotation = p_state["lock_rotation"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_LOCK_ROTATION);
		view_menu->get_popup()->set_item_checked(idx, lock_rotation);
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
		viewport->set_as_audio_listener(listener);
		view_menu->get_popup()->set_item_checked(idx, listener);
	}
	if (p_state.has("doppler")) {
		bool doppler = p_state["doppler"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER);
		camera->set_doppler_tracking(doppler ? Camera::DOPPLER_TRACKING_IDLE_STEP : Camera::DOPPLER_TRACKING_DISABLED);
		view_menu->get_popup()->set_item_checked(idx, doppler);
	}
	if (p_state.has("gizmos")) {
		bool gizmos = p_state["gizmos"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_GIZMOS);
		if (view_menu->get_popup()->is_item_checked(idx) != gizmos) {
			_menu_option(VIEW_GIZMOS);
		}
	}
	if (p_state.has("information")) {
		bool information = p_state["information"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_INFORMATION);
		if (view_menu->get_popup()->is_item_checked(idx) != information) {
			_menu_option(VIEW_INFORMATION);
		}
	}
	if (p_state.has("selected_info")) {
		bool selected_info = p_state["selected_info"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_SELECTED_INFO);
		if (view_menu->get_popup()->is_item_checked(idx) != selected_info) {
			_menu_option(VIEW_SELECTED_INFO);
		}
	}
	if (p_state.has("fps")) {
		bool fps = p_state["fps"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_FPS);
		if (view_menu->get_popup()->is_item_checked(idx) != fps) {
			_menu_option(VIEW_FPS);
		}
	}
	if (p_state.has("half_res")) {
		bool half_res = p_state["half_res"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_HALF_RESOLUTION);
		view_menu->get_popup()->set_item_checked(idx, half_res);
	}
	if (p_state.has("cinematic_preview")) {
		previewing_cinema = p_state["cinematic_preview"];

		int idx = view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW);
		view_menu->get_popup()->set_item_checked(idx, previewing_cinema);
	}

	if (preview_camera->is_connected("toggled", this, "_toggle_camera_preview")) {
		preview_camera->disconnect("toggled", this, "_toggle_camera_preview");
	}
	if (p_state.has("previewing")) {
		Node *pv = EditorNode::get_singleton()->get_edited_scene()->get_node(p_state["previewing"]);
		if (Object::cast_to<Camera>(pv)) {
			previewing = Object::cast_to<Camera>(pv);
			previewing->connect("tree_exiting", this, "_preview_exited_scene");
			VS::get_singleton()->viewport_attach_camera(viewport->get_viewport_rid(), previewing->get_camera()); //replace
			surface->update();
			preview_camera->set_pressed(true);
			preview_camera->show();
		}
	}
	preview_camera->connect("toggled", this, "_toggle_camera_preview");
}

Dictionary SpatialEditorViewport::get_state() const {
	Dictionary d;
	d["position"] = cursor.pos;
	d["x_rotation"] = cursor.x_rot;
	d["y_rotation"] = cursor.y_rot;
	d["distance"] = cursor.distance;
	d["use_environment"] = camera->get_environment().is_valid();
	d["use_orthogonal"] = camera->get_projection() == Camera::PROJECTION_ORTHOGONAL;
	d["view_type"] = view_type;
	d["auto_orthogonal"] = auto_orthogonal;
	d["auto_orthogonal_enabled"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL));
	if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL))) {
		d["display_mode"] = VIEW_DISPLAY_NORMAL;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME))) {
		d["display_mode"] = VIEW_DISPLAY_WIREFRAME;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW))) {
		d["display_mode"] = VIEW_DISPLAY_OVERDRAW;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS))) {
		d["display_mode"] = VIEW_DISPLAY_SHADELESS;
	} else if (view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING))) {
		d["display_mode"] = VIEW_DISPLAY_LIGHTING;
	}
	d["listener"] = viewport->is_audio_listener();
	d["doppler"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER));
	d["gizmos"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_GIZMOS));
	d["information"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_INFORMATION));
	d["selected_info"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_SELECTED_INFO));
	d["fps"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_FPS));
	d["half_res"] = viewport_container->get_stretch_shrink() > 1;
	d["cinematic_preview"] = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(VIEW_CINEMATIC_PREVIEW));
	if (previewing) {
		d["previewing"] = EditorNode::get_singleton()->get_edited_scene()->get_path_to(previewing);
	}
	if (lock_rotation) {
		d["lock_rotation"] = lock_rotation;
	}

	return d;
}

void SpatialEditorViewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_draw"), &SpatialEditorViewport::_draw);

	ClassDB::bind_method(D_METHOD("_surface_mouse_enter"), &SpatialEditorViewport::_surface_mouse_enter);
	ClassDB::bind_method(D_METHOD("_surface_mouse_exit"), &SpatialEditorViewport::_surface_mouse_exit);
	ClassDB::bind_method(D_METHOD("_surface_focus_enter"), &SpatialEditorViewport::_surface_focus_enter);
	ClassDB::bind_method(D_METHOD("_surface_focus_exit"), &SpatialEditorViewport::_surface_focus_exit);
	ClassDB::bind_method(D_METHOD("_sinput"), &SpatialEditorViewport::_sinput);
	ClassDB::bind_method(D_METHOD("_menu_option"), &SpatialEditorViewport::_menu_option);
	ClassDB::bind_method(D_METHOD("_toggle_camera_preview"), &SpatialEditorViewport::_toggle_camera_preview);
	ClassDB::bind_method(D_METHOD("_preview_exited_scene"), &SpatialEditorViewport::_preview_exited_scene);
	ClassDB::bind_method(D_METHOD("_view_settings_confirmed"), &SpatialEditorViewport::_view_settings_confirmed);
	ClassDB::bind_method(D_METHOD("_update_camera"), &SpatialEditorViewport::_update_camera);
	ClassDB::bind_method(D_METHOD("update_transform_gizmo_view"), &SpatialEditorViewport::update_transform_gizmo_view);
	ClassDB::bind_method(D_METHOD("_selection_result_pressed"), &SpatialEditorViewport::_selection_result_pressed);
	ClassDB::bind_method(D_METHOD("_selection_menu_hide"), &SpatialEditorViewport::_selection_menu_hide);
	ClassDB::bind_method(D_METHOD("can_drop_data_fw"), &SpatialEditorViewport::can_drop_data_fw);
	ClassDB::bind_method(D_METHOD("drop_data_fw"), &SpatialEditorViewport::drop_data_fw);
	ClassDB::bind_method(D_METHOD("_project_settings_changed"), &SpatialEditorViewport::_project_settings_changed);

	ADD_SIGNAL(MethodInfo("toggle_maximize_view", PropertyInfo(Variant::OBJECT, "viewport")));
	ADD_SIGNAL(MethodInfo("clicked", PropertyInfo(Variant::OBJECT, "viewport")));
}

void SpatialEditorViewport::reset() {
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

void SpatialEditorViewport::focus_selection() {
	Vector3 center;
	int count = 0;

	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->get());
		if (!sp) {
			continue;
		}

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		center += sp->get_global_gizmo_transform().origin;
		count++;
	}

	if (count != 0) {
		center /= float(count);
	}

	cursor.pos = center;
}

void SpatialEditorViewport::assign_pending_data_pointers(Spatial *p_preview_node, AABB *p_preview_bounds, AcceptDialog *p_accept) {
	preview_node = p_preview_node;
	preview_bounds = p_preview_bounds;
	accept = p_accept;
}

Vector3 SpatialEditorViewport::_get_instance_position(const Point2 &p_pos) const {
	const float MAX_DISTANCE = 50.0;
	const float FALLBACK_DISTANCE = 5.0;

	Vector3 world_ray = _get_ray(p_pos);
	Vector3 world_pos = _get_ray_pos(p_pos);

	PhysicsDirectSpaceState *ss = get_tree()->get_root()->get_world()->get_direct_space_state();
	PhysicsDirectSpaceState::RayResult result;

	if (ss->intersect_ray(world_pos, world_pos + world_ray * camera->get_zfar(), result)) {
		return result.position;
	}

	const bool is_orthogonal = camera->get_projection() == Camera::PROJECTION_ORTHOGONAL;

	// The XZ plane.
	Vector3 intersection;
	Plane plane(Vector3(0, 1, 0), 0);
	if (plane.intersects_ray(world_pos, world_ray, &intersection)) {
		if (is_orthogonal || world_pos.distance_to(intersection) <= MAX_DISTANCE) {
			return intersection;
		}
	}

	// Plane facing the camera using fallback distance.
	if (is_orthogonal) {
		plane = Plane(cursor.pos - world_ray * (cursor.distance - FALLBACK_DISTANCE), world_ray);
	} else {
		plane = Plane(world_pos + world_ray * FALLBACK_DISTANCE, world_ray);
	}
	if (plane.intersects_ray(world_pos, world_ray, &intersection)) {
		return intersection;
	}

	// Not likely, but just in case...
	return world_pos + world_ray * FALLBACK_DISTANCE;
}

AABB SpatialEditorViewport::_calculate_spatial_bounds(const Spatial *p_parent, bool p_exclude_toplevel_transform) {
	AABB bounds;

	const VisualInstance *visual_instance = Object::cast_to<VisualInstance>(p_parent);
	if (visual_instance) {
		bounds = visual_instance->get_aabb();
	}

	for (int i = 0; i < p_parent->get_child_count(); i++) {
		Spatial *child = Object::cast_to<Spatial>(p_parent->get_child(i));
		if (child) {
			AABB child_bounds = _calculate_spatial_bounds(child, false);

			if (bounds.size == Vector3() && p_parent->get_class_name() == StringName("Spatial")) {
				bounds = child_bounds;
			} else {
				bounds.merge_with(child_bounds);
			}
		}
	}

	if (bounds.size == Vector3() && p_parent->get_class_name() != StringName("Spatial")) {
#ifdef TOOLS_ENABLED
		bounds = p_parent->get_fallback_gizmo_aabb();
#else
		bounds = AABB(Vector3(-0.2, -0.2, -0.2), Vector3(0.4, 0.4, 0.4));
#endif
	}

	if (!p_exclude_toplevel_transform) {
		bounds = p_parent->get_transform().xform(bounds);
	}

	return bounds;
}

Node *SpatialEditorViewport::_sanitize_preview_node(Node *p_node) const {
	Spatial *spatial = Object::cast_to<Spatial>(p_node);
	if (spatial == nullptr) {
		Spatial *replacement_node = memnew(Spatial);
		replacement_node->set_name(p_node->get_name());
		p_node->replace_by(replacement_node);
		memdelete(p_node);
		p_node = replacement_node;
	} else {
		VisualInstance *visual_instance = Object::cast_to<VisualInstance>(spatial);
		if (visual_instance == nullptr) {
			Spatial *replacement_node = memnew(Spatial);
			replacement_node->set_name(spatial->get_name());
			replacement_node->set_visible(spatial->is_visible());
			replacement_node->set_transform(spatial->get_transform());
			replacement_node->set_as_toplevel(spatial->is_set_as_toplevel());
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

void SpatialEditorViewport::_create_preview(const Vector<String> &files) const {
	for (int i = 0; i < files.size(); i++) {
		String path = files[i];
		RES res = ResourceLoader::load(path);
		ERR_CONTINUE(res.is_null());
		Ref<PackedScene> scene = Ref<PackedScene>(Object::cast_to<PackedScene>(*res));
		Ref<Mesh> mesh = Ref<Mesh>(Object::cast_to<Mesh>(*res));
		if (mesh != nullptr || scene != nullptr) {
			if (mesh != nullptr) {
				MeshInstance *mesh_instance = memnew(MeshInstance);
				mesh_instance->set_mesh(mesh);
				preview_node->add_child(mesh_instance);
			} else {
				if (scene.is_valid()) {
					Node *instance = scene->instance();
					if (instance) {
						instance = _sanitize_preview_node(instance);
						preview_node->add_child(instance);
					}
				}
			}
			editor->get_scene_root()->add_child(preview_node);
		}
	}
	*preview_bounds = _calculate_spatial_bounds(preview_node);
}

void SpatialEditorViewport::_remove_preview() {
	if (preview_node->get_parent()) {
		for (int i = preview_node->get_child_count() - 1; i >= 0; i--) {
			Node *node = preview_node->get_child(i);
			node->queue_delete();
			preview_node->remove_child(node);
		}
		editor->get_scene_root()->remove_child(preview_node);
	}
}

bool SpatialEditorViewport::_cyclical_dependency_exists(const String &p_target_scene_path, Node *p_desired_node) {
	if (p_desired_node->get_filename() == p_target_scene_path) {
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

bool SpatialEditorViewport::_create_instance(Node *parent, String &path, const Point2 &p_point) {
	RES res = ResourceLoader::load(path);
	ERR_FAIL_COND_V(res.is_null(), false);

	Ref<PackedScene> scene = Ref<PackedScene>(Object::cast_to<PackedScene>(*res));
	Ref<Mesh> mesh = Ref<Mesh>(Object::cast_to<Mesh>(*res));

	Node *instanced_scene = nullptr;

	if (mesh != nullptr || scene != nullptr) {
		if (mesh != nullptr) {
			MeshInstance *mesh_instance = memnew(MeshInstance);
			mesh_instance->set_mesh(mesh);

			// Adjust casing according to project setting. The file name is expected to be in snake_case, but will work for others.
			String name = path.get_file().get_basename();
			switch (ProjectSettings::get_singleton()->get("node/name_casing").operator int()) {
				case NAME_CASING_PASCAL_CASE:
					name = name.capitalize().replace(" ", "");
					break;
				case NAME_CASING_CAMEL_CASE:
					name = name.capitalize().replace(" ", "");
					name[0] = name.to_lower()[0];
					break;
				case NAME_CASING_SNAKE_CASE:
					name = name.capitalize().replace(" ", "_").to_lower();
					break;
			}
			mesh_instance->set_name(name);

			instanced_scene = mesh_instance;
		} else {
			if (!scene.is_valid()) { // invalid scene
				return false;
			} else {
				instanced_scene = scene->instance(PackedScene::GEN_EDIT_STATE_INSTANCE);
			}
		}
	}

	if (instanced_scene == nullptr) {
		return false;
	}

	if (editor->get_edited_scene()->get_filename() != "") { // cyclical instancing
		if (_cyclical_dependency_exists(editor->get_edited_scene()->get_filename(), instanced_scene)) {
			memdelete(instanced_scene);
			return false;
		}
	}

	if (scene != nullptr) {
		instanced_scene->set_filename(ProjectSettings::get_singleton()->localize_path(path));
	}

	editor_data->get_undo_redo().add_do_method(parent, "add_child", instanced_scene);
	editor_data->get_undo_redo().add_do_method(instanced_scene, "set_owner", editor->get_edited_scene());
	editor_data->get_undo_redo().add_do_reference(instanced_scene);
	editor_data->get_undo_redo().add_undo_method(parent, "remove_child", instanced_scene);

	String new_name = parent->validate_child_name(instanced_scene);
	ScriptEditorDebugger *sed = ScriptEditor::get_singleton()->get_debugger();
	editor_data->get_undo_redo().add_do_method(sed, "live_debug_instance_node", editor->get_edited_scene()->get_path_to(parent), path, new_name);
	editor_data->get_undo_redo().add_undo_method(sed, "live_debug_remove_node", NodePath(String(editor->get_edited_scene()->get_path_to(parent)) + "/" + new_name));

	Spatial *spatial = Object::cast_to<Spatial>(instanced_scene);
	if (spatial) {
		Transform global_transform;
		Spatial *parent_spatial = Object::cast_to<Spatial>(parent);
		if (parent_spatial) {
			global_transform = parent_spatial->get_global_gizmo_transform();
		}

		global_transform.origin = spatial_editor->snap_point(_get_instance_position(p_point));
		global_transform.basis *= spatial->get_transform().basis;

		editor_data->get_undo_redo().add_do_method(instanced_scene, "set_global_transform", global_transform);
	}

	return true;
}

void SpatialEditorViewport::_perform_drop_data() {
	_remove_preview();

	Vector<String> error_files;

	editor_data->get_undo_redo().create_action(TTR("Create Node"));

	for (int i = 0; i < selected_files.size(); i++) {
		String path = selected_files[i];
		RES res = ResourceLoader::load(path);
		if (res.is_null()) {
			continue;
		}
		Ref<PackedScene> scene = Ref<PackedScene>(Object::cast_to<PackedScene>(*res));
		Ref<Mesh> mesh = Ref<Mesh>(Object::cast_to<Mesh>(*res));
		if (mesh != nullptr || scene != nullptr) {
			bool success = _create_instance(target_node, path, drop_pos);
			if (!success) {
				error_files.push_back(path);
			}
		}
	}

	editor_data->get_undo_redo().commit_action();

	if (error_files.size() > 0) {
		String files_str;
		for (int i = 0; i < error_files.size(); i++) {
			files_str += error_files[i].get_file().get_basename() + ",";
		}
		files_str = files_str.substr(0, files_str.length() - 1);
		accept->set_text(vformat(TTR("Error instancing scene from %s"), files_str.c_str()));
		accept->popup_centered_minsize();
	}
}

bool SpatialEditorViewport::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	bool can_instance = false;

	if (!preview_node->is_inside_tree()) {
		Dictionary d = p_data;
		if (d.has("type") && (String(d["type"]) == "files")) {
			Vector<String> files = d["files"];

			List<String> scene_extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &scene_extensions);
			List<String> mesh_extensions;
			ResourceLoader::get_recognized_extensions_for_type("Mesh", &mesh_extensions);

			for (int i = 0; i < files.size(); i++) {
				String extension = files[i].get_extension().to_lower();
				if (mesh_extensions.find(extension) || scene_extensions.find(extension)) {
					RES res = ResourceLoader::load(files[i]);
					if (res.is_null()) {
						continue;
					}

					String type = res->get_class();
					if (type == "PackedScene") {
						Ref<PackedScene> sdata = ResourceLoader::load(files[i]);
						Node *instanced_scene = sdata->instance(PackedScene::GEN_EDIT_STATE_INSTANCE);
						if (!instanced_scene) {
							continue;
						}
						memdelete(instanced_scene);
					} else if (type == "Mesh" || type == "ArrayMesh" || type == "PrimitiveMesh") {
						Ref<Mesh> mesh = ResourceLoader::load(files[i]);
						if (!mesh.is_valid()) {
							continue;
						}
					} else {
						continue;
					}
					can_instance = true;
					break;
				}
			}
			if (can_instance) {
				_create_preview(files);
			}
		}
	} else {
		can_instance = true;
	}

	if (can_instance) {
		Transform global_transform = Transform(Basis(), spatial_editor->snap_point(_get_instance_position(p_point)));
		preview_node->set_global_transform(global_transform);
	}

	return can_instance;
}

void SpatialEditorViewport::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	bool is_shift = Input::get_singleton()->is_key_pressed(KEY_SHIFT);
	bool is_ctrl = Input::get_singleton()->is_key_pressed(KEY_CONTROL);

	selected_files.clear();
	Dictionary d = p_data;
	if (d.has("type") && String(d["type"]) == "files") {
		selected_files = d["files"];
	}

	List<Node *> selected_nodes = editor->get_editor_selection()->get_selected_node_list();
	Node *root_node = editor->get_edited_scene();
	if (selected_nodes.size() == 1) {
		Node *selected_node = selected_nodes[0];
		target_node = root_node;
		if (is_ctrl) {
			target_node = selected_node;
		} else if (is_shift && selected_node != root_node) {
			target_node = selected_node->get_parent();
		}
	} else if (selected_nodes.size() == 0) {
		if (root_node) {
			target_node = root_node;
		} else {
			// Create a root node so we can add child nodes to it.
			EditorNode::get_singleton()->get_scene_tree_dock()->add_root_node(memnew(Spatial));
			target_node = get_tree()->get_edited_scene_root();
		}
	} else {
		accept->set_text(TTR("Cannot drag and drop into multiple selected nodes."));
		accept->popup_centered();
		_remove_preview();
		return;
	}

	drop_pos = p_point;

	_perform_drop_data();
}

SpatialEditorViewport::SpatialEditorViewport(SpatialEditor *p_spatial_editor, EditorNode *p_editor, int p_index) {
	_edit.mode = TRANSFORM_NONE;
	_edit.plane = TRANSFORM_VIEW;
	_edit.edited_gizmo = 0;
	_edit.snap = true;
	_edit.gizmo_handle = 0;

	index = p_index;
	editor = p_editor;
	editor_data = editor->get_scene_tree_dock()->get_editor_data();
	editor_selection = editor->get_editor_selection();
	undo_redo = editor->get_undo_redo();
	clicked = 0;
	clicked_includes_current = false;
	orthogonal = false;
	auto_orthogonal = false;
	lock_rotation = false;
	message_time = 0;
	zoom_indicator_delay = 0.0;

	spatial_editor = p_spatial_editor;
	ViewportContainer *c = memnew(ViewportContainer);
	viewport_container = c;
	c->set_stretch(true);
	add_child(c);
	c->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	viewport = memnew(Viewport);
	viewport->set_disable_input(true);

	c->add_child(viewport);
	surface = memnew(Control);
	surface->set_drag_forwarding(this);
	add_child(surface);
	surface->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	surface->set_clip_contents(true);
	camera = memnew(Camera);
	camera->set_disable_gizmo(true);
	camera->set_cull_mask(((1 << 20) - 1) | (1 << (GIZMO_BASE_LAYER + p_index)) | (1 << GIZMO_EDIT_LAYER) | (1 << GIZMO_GRID_LAYER) | (1 << MISC_TOOL_LAYER));
	viewport->add_child(camera);
	camera->make_current();
	surface->set_focus_mode(FOCUS_ALL);

	VBoxContainer *vbox = memnew(VBoxContainer);
	surface->add_child(vbox);
	vbox->set_position(Point2(10, 10) * EDSCALE);

	view_menu = memnew(MenuButton);
	view_menu->set_flat(false);
	vbox->add_child(view_menu);
	view_menu->set_h_size_flags(0);
	view_menu->get_popup()->set_hide_on_checkable_item_selection(false);

	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/top_view"), VIEW_TOP);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/bottom_view"), VIEW_BOTTOM);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/left_view"), VIEW_LEFT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/right_view"), VIEW_RIGHT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/front_view"), VIEW_FRONT);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/rear_view"), VIEW_REAR);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_radio_check_item(TTR("Perspective") + " (" + ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal")->get_as_text() + ")", VIEW_PERSPECTIVE);
	view_menu->get_popup()->add_radio_check_item(TTR("Orthogonal") + " (" + ED_GET_SHORTCUT("spatial_editor/switch_perspective_orthogonal")->get_as_text() + ")", VIEW_ORTHOGONAL);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_PERSPECTIVE), true);
	view_menu->get_popup()->add_check_item(TTR("Auto Orthogonal Enabled"), VIEW_AUTO_ORTHOGONAL);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUTO_ORTHOGONAL), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_lock_rotation", TTR("Lock View Rotation")), VIEW_LOCK_ROTATION);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_normal", TTR("Display Normal")), VIEW_DISPLAY_NORMAL);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_wireframe", TTR("Display Wireframe")), VIEW_DISPLAY_WIREFRAME);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_overdraw", TTR("Display Overdraw")), VIEW_DISPLAY_OVERDRAW);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_unshaded", TTR("Display Unshaded")), VIEW_DISPLAY_SHADELESS);
	view_menu->get_popup()->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/view_display_lighting", TTR("Display Lighting")), VIEW_DISPLAY_LIGHTING);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL), true);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_environment", TTR("View Environment")), VIEW_ENVIRONMENT);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_ENVIRONMENT), true);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_gizmos", TTR("View Gizmos")), VIEW_GIZMOS);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_GIZMOS), true);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_information", TTR("View Information")), VIEW_INFORMATION);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_selected_info", TTR("View Selected Mesh Stats")), VIEW_SELECTED_INFO);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_fps", TTR("View FPS")), VIEW_FPS);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_half_resolution", TTR("Half Resolution")), VIEW_HALF_RESOLUTION);
	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_audio_listener", TTR("Audio Listener")), VIEW_AUDIO_LISTENER);
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_audio_doppler", TTR("Enable Doppler")), VIEW_AUDIO_DOPPLER);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_DOPPLER), true);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_cinematic_preview", TTR("Cinematic Preview")), VIEW_CINEMATIC_PREVIEW);

	view_menu->get_popup()->add_separator();
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_origin"), VIEW_CENTER_TO_ORIGIN);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/focus_selection"), VIEW_CENTER_TO_SELECTION);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_transform_with_view"), VIEW_ALIGN_TRANSFORM_WITH_VIEW);
	view_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("spatial_editor/align_rotation_with_view"), VIEW_ALIGN_ROTATION_WITH_VIEW);
	view_menu->get_popup()->connect("id_pressed", this, "_menu_option");

	view_menu->set_disable_shortcuts(true);

	if (OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2) {
		// Alternate display modes only work when using the GLES3 renderer; make this explicit.
		const int normal_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_NORMAL);
		const int wireframe_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_WIREFRAME);
		const int overdraw_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_OVERDRAW);
		const int shadeless_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_SHADELESS);
		const int lighting_idx = view_menu->get_popup()->get_item_index(VIEW_DISPLAY_LIGHTING);
		const String unsupported_suffix = " " + TTR("(Not in GLES2)");
		const String unsupported_tooltip = TTR("Debug draw modes are only available when using the GLES3 renderer, not GLES2.");

		view_menu->get_popup()->set_item_tooltip(normal_idx, unsupported_tooltip);

		view_menu->get_popup()->set_item_disabled(wireframe_idx, true);
		view_menu->get_popup()->set_item_text(wireframe_idx, TTR("Display Wireframe") + unsupported_suffix);
		view_menu->get_popup()->set_item_tooltip(wireframe_idx, unsupported_tooltip);

		view_menu->get_popup()->set_item_disabled(overdraw_idx, true);
		view_menu->get_popup()->set_item_text(overdraw_idx, TTR("Display Overdraw") + unsupported_suffix);
		view_menu->get_popup()->set_item_tooltip(overdraw_idx, unsupported_tooltip);

		view_menu->get_popup()->set_item_disabled(shadeless_idx, true);
		view_menu->get_popup()->set_item_text(shadeless_idx, TTR("Display Unshaded") + unsupported_suffix);
		view_menu->get_popup()->set_item_tooltip(shadeless_idx, unsupported_tooltip);

		view_menu->get_popup()->set_item_disabled(lighting_idx, true);
		view_menu->get_popup()->set_item_text(lighting_idx, TTR("Display Lighting") + unsupported_suffix);
		view_menu->get_popup()->set_item_tooltip(lighting_idx, unsupported_tooltip);
	}

	ED_SHORTCUT("spatial_editor/freelook_left", TTR("Freelook Left"), KEY_A);
	ED_SHORTCUT("spatial_editor/freelook_right", TTR("Freelook Right"), KEY_D);
	ED_SHORTCUT("spatial_editor/freelook_forward", TTR("Freelook Forward"), KEY_W);
	ED_SHORTCUT("spatial_editor/freelook_backwards", TTR("Freelook Backwards"), KEY_S);
	ED_SHORTCUT("spatial_editor/freelook_up", TTR("Freelook Up"), KEY_E);
	ED_SHORTCUT("spatial_editor/freelook_down", TTR("Freelook Down"), KEY_Q);
	ED_SHORTCUT("spatial_editor/freelook_speed_modifier", TTR("Freelook Speed Modifier"), KEY_SHIFT);
	ED_SHORTCUT("spatial_editor/freelook_slow_modifier", TTR("Freelook Slow Modifier"), KEY_ALT);

	preview_camera = memnew(CheckBox);
	preview_camera->set_text(TTR("Preview"));
	preview_camera->set_shortcut(ED_SHORTCUT("spatial_editor/toggle_camera_preview", TTR("Toggle Camera Preview"), KEY_MASK_CMD | KEY_P));
	vbox->add_child(preview_camera);
	preview_camera->set_h_size_flags(0);
	preview_camera->hide();
	preview_camera->connect("toggled", this, "_toggle_camera_preview");
	previewing = nullptr;
	gizmo_scale = 1.0;

	preview_node = nullptr;
	_project_settings_change_pending = false;

	bottom_center_vbox = memnew(VBoxContainer);
	bottom_center_vbox->set_anchors_preset(LayoutPreset::PRESET_CENTER);
	bottom_center_vbox->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -20 * EDSCALE);
	bottom_center_vbox->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, -10 * EDSCALE);
	bottom_center_vbox->set_h_grow_direction(GROW_DIRECTION_BOTH);
	bottom_center_vbox->set_v_grow_direction(GROW_DIRECTION_BEGIN);
	surface->add_child(bottom_center_vbox);

	info_label = memnew(Label);
	info_label->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, -90 * EDSCALE);
	info_label->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -90 * EDSCALE);
	info_label->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, -10 * EDSCALE);
	info_label->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, -10 * EDSCALE);
	info_label->set_h_grow_direction(GROW_DIRECTION_BEGIN);
	info_label->set_v_grow_direction(GROW_DIRECTION_BEGIN);
	surface->add_child(info_label);
	info_label->hide();

	selected_info_label = memnew(Label);
	selected_info_label->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 10 * EDSCALE);
	selected_info_label->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -20 * EDSCALE);
	selected_info_label->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_BEGIN, 90 * EDSCALE);
	selected_info_label->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, -10 * EDSCALE);
	selected_info_label->set_h_grow_direction(GROW_DIRECTION_END);
	selected_info_label->set_v_grow_direction(GROW_DIRECTION_BEGIN);
	surface->add_child(selected_info_label);
	selected_info_label->hide();

	cinema_label = memnew(Label);
	cinema_label->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 10 * EDSCALE);
	cinema_label->set_h_grow_direction(GROW_DIRECTION_END);
	cinema_label->set_align(Label::ALIGN_CENTER);
	surface->add_child(cinema_label);
	cinema_label->set_text(TTR("Cinematic Preview"));
	cinema_label->hide();
	previewing_cinema = false;

	locked_label = memnew(Label);
	locked_label->set_align(Label::ALIGN_CENTER);
	locked_label->set_h_size_flags(SIZE_SHRINK_CENTER);
	bottom_center_vbox->add_child(locked_label);
	locked_label->set_text(TTR("View Rotation Locked"));
	locked_label->hide();

	zoom_limit_label = memnew(Label);
	zoom_limit_label->set_text(TTR("To zoom further, change the camera's clipping planes (View -> Settings...)"));
	zoom_limit_label->set_name("ZoomLimitMessageLabel");
	zoom_limit_label->add_color_override("font_color", Color(1, 1, 1, 1));
	zoom_limit_label->hide();
	bottom_center_vbox->add_child(zoom_limit_label);

	top_right_vbox = memnew(VBoxContainer);
	top_right_vbox->set_anchors_and_margins_preset(PRESET_TOP_RIGHT, PRESET_MODE_MINSIZE, 2.0 * EDSCALE);
	top_right_vbox->set_h_grow_direction(GROW_DIRECTION_BEGIN);

	const int navigation_control_size = 150;

	position_control = memnew(ViewportNavigationControl);
	position_control->set_navigation_mode(SpatialEditorViewport::NAVIGATION_MOVE);
	position_control->set_custom_minimum_size(Size2(navigation_control_size, navigation_control_size) * EDSCALE);
	position_control->set_h_size_flags(SIZE_SHRINK_END);
	position_control->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 0 * EDSCALE);
	position_control->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -navigation_control_size * EDSCALE);
	position_control->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_BEGIN, navigation_control_size * EDSCALE);
	position_control->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0 * EDSCALE);
	position_control->set_viewport(this);
	surface->add_child(position_control);

	look_control = memnew(ViewportNavigationControl);
	look_control->set_navigation_mode(SpatialEditorViewport::NAVIGATION_LOOK);
	look_control->set_custom_minimum_size(Size2(navigation_control_size, navigation_control_size) * EDSCALE);
	look_control->set_h_size_flags(SIZE_SHRINK_END);
	look_control->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, -navigation_control_size * EDSCALE);
	look_control->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -navigation_control_size * EDSCALE);
	look_control->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0 * EDSCALE);
	look_control->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0 * EDSCALE);
	look_control->set_viewport(this);
	surface->add_child(look_control);

	rotation_control = memnew(ViewportRotationControl);
	rotation_control->set_custom_minimum_size(Size2(80, 80) * EDSCALE);
	rotation_control->set_h_size_flags(SIZE_SHRINK_END);
	rotation_control->set_viewport(this);
	top_right_vbox->add_child(rotation_control);

	frame_time_gradient = memnew(Gradient);
	// The color is set when the theme changes.
	frame_time_gradient->add_point(0.5, Color());

	fps_label = memnew(Label);
	fps_label->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_END, -90 * EDSCALE);
	fps_label->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 10 * EDSCALE);
	fps_label->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, -10 * EDSCALE);
	fps_label->set_h_grow_direction(GROW_DIRECTION_BEGIN);
	fps_label->set_tooltip(TTR("Note: The FPS value displayed is the editor's framerate.\nIt cannot be used as a reliable indication of in-game performance."));
	fps_label->set_mouse_filter(MOUSE_FILTER_PASS); // Otherwise tooltip doesn't show.
	top_right_vbox->add_child(fps_label);
	fps_label->hide();

	surface->add_child(top_right_vbox);

	accept = nullptr;

	freelook_active = false;
	freelook_speed = EditorSettings::get_singleton()->get("editors/3d/freelook/freelook_base_speed");

	selection_menu = memnew(PopupMenu);
	add_child(selection_menu);
	selection_menu->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
	selection_menu->connect("id_pressed", this, "_selection_result_pressed");
	selection_menu->connect("popup_hide", this, "_selection_menu_hide");

	if (p_index == 0) {
		view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(VIEW_AUDIO_LISTENER), true);
		viewport->set_as_audio_listener(true);
	}

	view_type = VIEW_TYPE_USER;
	_update_name();

	EditorSettings::get_singleton()->connect("settings_changed", this, "update_transform_gizmo_view");
}

SpatialEditorViewport::~SpatialEditorViewport() {
	memdelete(frame_time_gradient);
}

//////////////////////////////////////////////////////////////

void SpatialEditorViewportContainer::_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->get_button_index() == BUTTON_LEFT) {
		if (mb->is_pressed()) {
			Vector2 size = get_size();

			int h_sep = get_constant("separation", "HSplitContainer");
			int v_sep = get_constant("separation", "VSplitContainer");

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

			int h_sep = get_constant("separation", "HSplitContainer");
			int v_sep = get_constant("separation", "VSplitContainer");

			int mid_w = size.width * ratio_h;
			int mid_h = size.height * ratio_v;

			bool was_hovering_h = hovering_h;
			bool was_hovering_v = hovering_v;
			hovering_h = mm->get_position().x > (mid_w - h_sep / 2) && mm->get_position().x < (mid_w + h_sep / 2);
			hovering_v = mm->get_position().y > (mid_h - v_sep / 2) && mm->get_position().y < (mid_h + v_sep / 2);

			if (was_hovering_h != hovering_h || was_hovering_v != hovering_v) {
				update();
			}
		}

		if (dragging_h) {
			float new_ratio = drag_begin_ratio.x + (mm->get_position().x - drag_begin_pos.x) / get_size().width;
			new_ratio = CLAMP(new_ratio, 40 / get_size().width, (get_size().width - 40) / get_size().width);
			ratio_h = new_ratio;
			queue_sort();
			update();
		}
		if (dragging_v) {
			float new_ratio = drag_begin_ratio.y + (mm->get_position().y - drag_begin_pos.y) / get_size().height;
			new_ratio = CLAMP(new_ratio, 40 / get_size().height, (get_size().height - 40) / get_size().height);
			ratio_v = new_ratio;
			queue_sort();
			update();
		}
	}
}

void SpatialEditorViewportContainer::_notification(int p_what) {
	if (p_what == NOTIFICATION_MOUSE_ENTER || p_what == NOTIFICATION_MOUSE_EXIT) {
		mouseover = (p_what == NOTIFICATION_MOUSE_ENTER);
		update();
	}

	if (p_what == NOTIFICATION_DRAW && mouseover) {
		Ref<Texture> h_grabber = get_icon("grabber", "HSplitContainer");
		Ref<Texture> v_grabber = get_icon("grabber", "VSplitContainer");

		Ref<Texture> hdiag_grabber = get_icon("GuiViewportHdiagsplitter", "EditorIcons");
		Ref<Texture> vdiag_grabber = get_icon("GuiViewportVdiagsplitter", "EditorIcons");
		Ref<Texture> vh_grabber = get_icon("GuiViewportVhsplitter", "EditorIcons");

		Vector2 size = get_size();

		int h_sep = get_constant("separation", "HSplitContainer");

		int v_sep = get_constant("separation", "VSplitContainer");

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

	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		SpatialEditorViewport *viewports[4];
		int vc = 0;
		for (int i = 0; i < get_child_count(); i++) {
			viewports[vc] = Object::cast_to<SpatialEditorViewport>(get_child(i));
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
		int h_sep = get_constant("separation", "HSplitContainer");

		int v_sep = get_constant("separation", "VSplitContainer");

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
	}
}

void SpatialEditorViewportContainer::set_view(View p_view) {
	view = p_view;
	queue_sort();
}

SpatialEditorViewportContainer::View SpatialEditorViewportContainer::get_view() {
	return view;
}

void SpatialEditorViewportContainer::_bind_methods() {
	ClassDB::bind_method("_gui_input", &SpatialEditorViewportContainer::_gui_input);
}

SpatialEditorViewportContainer::SpatialEditorViewportContainer() {
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

SpatialEditor *SpatialEditor::singleton = nullptr;

SpatialEditorSelectedItem::~SpatialEditorSelectedItem() {
	if (sbox_instance.is_valid()) {
		VisualServer::get_singleton()->free(sbox_instance);
	}
	if (sbox_instance_offset.is_valid()) {
		VisualServer::get_singleton()->free(sbox_instance_offset);
	}
	if (sbox_instance_xray.is_valid()) {
		VisualServer::get_singleton()->free(sbox_instance_xray);
	}
	if (sbox_instance_xray_offset.is_valid()) {
		VisualServer::get_singleton()->free(sbox_instance_xray_offset);
	}
}

void SpatialEditor::select_gizmo_highlight_axis(int p_axis) {
	for (int i = 0; i < 3; i++) {
		move_gizmo[i]->surface_set_material(0, i == p_axis ? gizmo_color_hl[i] : gizmo_color[i]);
		move_plane_gizmo[i]->surface_set_material(0, (i + 6) == p_axis ? plane_gizmo_color_hl[i] : plane_gizmo_color[i]);
		rotate_gizmo[i]->surface_set_material(0, (i + 3) == p_axis ? rotate_gizmo_color_hl[i] : rotate_gizmo_color[i]);
		scale_gizmo[i]->surface_set_material(0, (i + 9) == p_axis ? gizmo_color_hl[i] : gizmo_color[i]);
		scale_plane_gizmo[i]->surface_set_material(0, (i + 12) == p_axis ? plane_gizmo_color_hl[i] : plane_gizmo_color[i]);
	}
}

void SpatialEditor::show_advanced_portal_tools(bool p_show) {
	// toolbar button
	Button *const button = tool_button[TOOL_CONVERT_ROOMS];
	if (p_show) {
		button->set_text(TTR("Convert Rooms"));
	} else {
		button->set_text("");
	}
}

void SpatialEditor::update_portal_tools() {
	// just some protection against calling null pointers, hopefully not needed
	if (view_menu && view_menu->get_popup()) {
		// the view portal culling toggle
		int view_portal_item_index = view_menu->get_popup()->get_item_index(MENU_VIEW_PORTAL_CULLING);
		if (RoomManager::active_room_manager) {
			view_menu->get_popup()->set_item_disabled(view_portal_item_index, false);

			bool active = RoomManager::static_rooms_get_active();
			view_menu->get_popup()->set_item_checked(view_portal_item_index, active);
		} else {
			view_menu->get_popup()->set_item_disabled(view_portal_item_index, true);
		}

		// toolbar button
		Button *const button = tool_button[TOOL_CONVERT_ROOMS];

		if (RoomManager::active_room_manager) {
			button->show();
		} else {
			button->hide();
		}

		for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
			viewports[i]->_update_name();
		}
	}
}

void SpatialEditor::update_transform_gizmo() {
	List<Node *> &selection = editor_selection->get_selected_node_list();
	AABB center;
	bool first = true;

	Basis gizmo_basis;
	bool local_gizmo_coords = are_local_coords_enabled();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->get());
		if (!sp) {
			continue;
		}

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		Transform xf = se->sp->get_global_gizmo_transform();

		if (first) {
			center.position = xf.origin;
			first = false;
			if (local_gizmo_coords) {
				gizmo_basis = xf.basis;
				gizmo_basis.orthonormalize();
			}
		} else {
			center.expand_to(xf.origin);
			gizmo_basis = Basis();
		}
	}

	Vector3 pcenter = center.position + center.size * 0.5;
	gizmo.visible = !first;
	gizmo.transform.origin = pcenter;
	gizmo.transform.basis = gizmo_basis;

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->update_transform_gizmo_view();
	}
}

void _update_all_gizmos(Node *p_node) {
	for (int i = p_node->get_child_count() - 1; 0 <= i; --i) {
		Spatial *spatial_node = Object::cast_to<Spatial>(p_node->get_child(i));
		if (spatial_node) {
			spatial_node->update_gizmo();
		}

		_update_all_gizmos(p_node->get_child(i));
	}
}

void SpatialEditor::update_all_gizmos(Node *p_node) {
	if (!p_node) {
		if (SceneTree::get_singleton()) {
			p_node = SceneTree::get_singleton()->get_root();
		} else {
			// No scene tree, so nothing to update.
			return;
		}
	}
	_update_all_gizmos(p_node);
}

Object *SpatialEditor::_get_editor_data(Object *p_what) {
	Spatial *sp = Object::cast_to<Spatial>(p_what);
	if (!sp) {
		return nullptr;
	}

	SpatialEditorSelectedItem *si = memnew(SpatialEditorSelectedItem);

	si->sp = sp;
	si->sbox_instance = VisualServer::get_singleton()->instance_create2(
			selection_box->get_rid(),
			sp->get_world()->get_scenario());
	si->sbox_instance_offset = VisualServer::get_singleton()->instance_create2(
			selection_box->get_rid(),
			sp->get_world()->get_scenario());
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance,
			VS::SHADOW_CASTING_SETTING_OFF);
	// Use the Edit layer to hide the selection box when View Gizmos is disabled, since it is a bit distracting.
	// It's still possible to approximately guess what is selected by looking at the manipulation gizmo position.
	VS::get_singleton()->instance_set_layer_mask(si->sbox_instance, 1 << SpatialEditorViewport::GIZMO_EDIT_LAYER);
	VS::get_singleton()->instance_set_layer_mask(si->sbox_instance_offset, 1 << SpatialEditorViewport::GIZMO_EDIT_LAYER);
	si->sbox_instance_xray = VisualServer::get_singleton()->instance_create2(
			selection_box_xray->get_rid(),
			sp->get_world()->get_scenario());
	si->sbox_instance_xray_offset = VisualServer::get_singleton()->instance_create2(
			selection_box_xray->get_rid(),
			sp->get_world()->get_scenario());
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance_xray,
			VS::SHADOW_CASTING_SETTING_OFF);
	VS::get_singleton()->instance_geometry_set_cast_shadows_setting(
			si->sbox_instance_xray_offset,
			VS::SHADOW_CASTING_SETTING_OFF);
	// Use the Edit layer to hide the selection box when View Gizmos is disabled, since it is a bit distracting.
	// It's still possible to approximately guess what is selected by looking at the manipulation gizmo position.
	VS::get_singleton()->instance_set_layer_mask(si->sbox_instance_xray, 1 << SpatialEditorViewport::GIZMO_EDIT_LAYER);
	VS::get_singleton()->instance_set_layer_mask(si->sbox_instance_xray_offset, 1 << SpatialEditorViewport::GIZMO_EDIT_LAYER);

	return si;
}

void SpatialEditor::_generate_selection_boxes() {
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

	Ref<Material3D> mat = memnew(SpatialMaterial);
	mat->set_flag(Material3D::FLAG_UNSHADED, true);
	const Color selection_box_color = EDITOR_GET("editors/3d/selection_box_color");
	mat->set_albedo(selection_box_color);
	mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	st->set_material(mat);
	selection_box = st->commit();

	Ref<Material3D> mat_xray = memnew(SpatialMaterial);
	mat_xray->set_flag(Material3D::FLAG_UNSHADED, true);
	mat_xray->set_flag(Material3D::FLAG_DISABLE_DEPTH_TEST, true);
	mat_xray->set_albedo(selection_box_color * Color(1, 1, 1, 0.15));
	mat_xray->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	st_xray->set_material(mat_xray);
	selection_box_xray = st_xray->commit();
}

Dictionary SpatialEditor::get_state() const {
	Dictionary d;

	d["snap_enabled"] = snap_enabled;
	d["translate_snap"] = get_translate_snap();
	d["rotate_snap"] = get_rotate_snap();
	d["scale_snap"] = get_scale_snap();

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
		String name = gizmo_plugins_by_name[i]->get_name();
		gizmos_status[name] = state;
	}

	d["gizmos_status"] = gizmos_status;

	return d;
}
void SpatialEditor::set_state(const Dictionary &p_state) {
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
		settings_zfar->set_value(float(d["zfar"]));
	}
	if (d.has("znear")) {
		settings_znear->set_value(float(d["znear"]));
	}
	if (d.has("fov")) {
		settings_fov->set_value(float(d["fov"]));
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
			VisualServer::get_singleton()->instance_set_visible(origin_instance, use);
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
			int state = EditorSpatialGizmoPlugin::VISIBLE;
			for (int i = 0; i < keys.size(); i++) {
				if (gizmo_plugins_by_name.write[j]->get_name() == keys[i]) {
					state = gizmos_status[keys[i]];
					break;
				}
			}

			gizmo_plugins_by_name.write[j]->set_state(state);
		}
		_update_gizmos_menu();
	}
}

void SpatialEditor::edit(Spatial *p_spatial) {
	if (p_spatial != selected) {
		if (selected) {
			Ref<EditorSpatialGizmo> seg = selected->get_gizmo();
			if (seg.is_valid()) {
				seg->set_selected(false);
				selected->update_gizmo();
			}
		}

		selected = p_spatial;
		over_gizmo_handle = -1;

		if (selected) {
			Ref<EditorSpatialGizmo> seg = selected->get_gizmo();
			if (seg.is_valid()) {
				seg->set_selected(true);
				selected->update_gizmo();
			}
		}
	}
}

void SpatialEditor::_snap_changed() {
	snap_translate_value = snap_translate->get_text().to_double();
	snap_rotate_value = snap_rotate->get_text().to_double();
	snap_scale_value = snap_scale->get_text().to_double();
}

void SpatialEditor::_snap_update() {
	snap_translate->set_text(String::num(snap_translate_value));
	snap_rotate->set_text(String::num(snap_rotate_value));
	snap_scale->set_text(String::num(snap_scale_value));
}

void SpatialEditor::_xform_dialog_action() {
	Transform t;
	//translation
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;

	for (int i = 0; i < 3; i++) {
		translate[i] = xform_translate[i]->get_text().to_double();
		rotate[i] = Math::deg2rad(xform_rotate[i]->get_text().to_double());
		scale[i] = xform_scale[i]->get_text().to_double();
	}

	t.basis.scale(scale);
	t.basis.rotate(rotate);
	t.origin = translate;

	undo_redo->create_action(TTR("XForm Dialog"));

	List<Node *> &selection = editor_selection->get_selected_node_list();

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->get());
		if (!sp) {
			continue;
		}

		SpatialEditorSelectedItem *se = editor_selection->get_node_editor_data<SpatialEditorSelectedItem>(sp);
		if (!se) {
			continue;
		}

		bool post = xform_type->get_selected() > 0;

		Transform tr = sp->get_global_gizmo_transform();
		if (post) {
			tr = tr * t;
		} else {
			tr.basis = t.basis * tr.basis;
			tr.origin += t.origin;
		}

		undo_redo->add_do_method(sp, "set_global_transform", tr);
		undo_redo->add_undo_method(sp, "set_global_transform", sp->get_global_gizmo_transform());
	}
	undo_redo->commit_action();
}

void SpatialEditor::_menu_item_toggled(bool pressed, int p_option) {
	switch (p_option) {
		case MENU_TOOL_LOCAL_COORDS: {
			tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_pressed(pressed);
			update_transform_gizmo();
		} break;

		case MENU_TOOL_USE_SNAP: {
			tool_option_button[TOOL_OPT_USE_SNAP]->set_pressed(pressed);
			snap_enabled = pressed;
		} break;

		case MENU_TOOL_OVERRIDE_CAMERA: {
			ScriptEditorDebugger *const debugger = ScriptEditor::get_singleton()->get_debugger();

			if (pressed) {
				using Override = ScriptEditorDebugger::CameraOverride;

				debugger->set_camera_override((Override)(Override::OVERRIDE_3D_1 + camera_override_viewport_id));
			} else {
				debugger->set_camera_override(ScriptEditorDebugger::OVERRIDE_NONE);
			}

		} break;
	}
}

void SpatialEditor::_menu_gizmo_toggled(int p_option) {
	const int idx = gizmos_menu->get_item_index(p_option);
	gizmos_menu->toggle_item_multistate(idx);

	// Change icon
	const int state = gizmos_menu->get_item_state(idx);
	switch (state) {
		case EditorSpatialGizmoPlugin::VISIBLE:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_icon("visibility_visible"));
			break;
		case EditorSpatialGizmoPlugin::ON_TOP:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_icon("visibility_xray"));
			break;
		case EditorSpatialGizmoPlugin::HIDDEN:
			gizmos_menu->set_item_icon(idx, view_menu->get_popup()->get_icon("visibility_hidden"));
			break;
	}

	gizmo_plugins_by_name.write[p_option]->set_state(state);

	update_all_gizmos();
}

void SpatialEditor::_update_camera_override_button(bool p_game_running) {
	Button *const button = tool_option_button[TOOL_OPT_OVERRIDE_CAMERA];

	if (p_game_running) {
		button->set_disabled(false);
		button->set_tooltip(TTR("Project Camera Override\nOverrides the running project's camera with the editor viewport camera."));
	} else {
		button->set_disabled(true);
		button->set_pressed(false);
		button->set_tooltip(TTR("Project Camera Override\nNo project instance running. Run the project from the editor to use this feature."));
	}
}

void SpatialEditor::_update_camera_override_viewport(Object *p_viewport) {
	SpatialEditorViewport *current_viewport = Object::cast_to<SpatialEditorViewport>(p_viewport);

	if (!current_viewport) {
		return;
	}

	ScriptEditorDebugger *const debugger = ScriptEditor::get_singleton()->get_debugger();

	camera_override_viewport_id = current_viewport->index;
	if (debugger->get_camera_override() >= ScriptEditorDebugger::OVERRIDE_3D_1) {
		using Override = ScriptEditorDebugger::CameraOverride;

		debugger->set_camera_override((Override)(Override::OVERRIDE_3D_1 + camera_override_viewport_id));
	}
}

void SpatialEditor::_menu_item_pressed(int p_option) {
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
		case MENU_TOOL_CONVERT_ROOMS: {
			RoomManager::static_rooms_convert();
			update_portal_tools();
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
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_1_VIEWPORT);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS: {
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_2_VIEWPORTS);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_2_VIEWPORTS_ALT: {
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_2_VIEWPORTS_ALT);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS: {
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_3_VIEWPORTS);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), true);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), false);

		} break;
		case MENU_VIEW_USE_3_VIEWPORTS_ALT: {
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_3_VIEWPORTS_ALT);

			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), false);
			view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), true);

		} break;
		case MENU_VIEW_USE_4_VIEWPORTS: {
			viewport_base->set_view(SpatialEditorViewportContainer::VIEW_USE_4_VIEWPORTS);

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
			VisualServer::get_singleton()->instance_set_visible(origin_instance, origin_enabled);
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
		case MENU_VIEW_PORTAL_CULLING: {
			bool is_checked = view_menu->get_popup()->is_item_checked(view_menu->get_popup()->get_item_index(p_option));
			RoomManager::static_rooms_set_active(!is_checked);
			update_portal_tools();
		} break;
		case MENU_VIEW_OCCLUSION_CULLING: {
			int checkbox_id = view_menu->get_popup()->get_item_index(p_option);
			bool is_checked = view_menu->get_popup()->is_item_checked(checkbox_id);
			VisualServer::get_singleton()->set_use_occlusion_culling(!is_checked);
			view_menu->get_popup()->set_item_checked(checkbox_id, !is_checked);

		} break;
		case MENU_VIEW_LEVEL_OF_DETAIL: {
			int checkbox_id = view_menu->get_popup()->get_item_index(p_option);
			bool is_checked = view_menu->get_popup()->is_item_checked(checkbox_id);
			LODManager::set_enabled(!is_checked);
			view_menu->get_popup()->set_item_checked(checkbox_id, !is_checked);
			EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor()->update_tree();

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

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *spatial = Object::cast_to<Spatial>(E->get());
				if (!spatial || !spatial->is_visible_in_tree()) {
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

			undo_redo->add_do_method(this, "_refresh_menu_icons", Variant());
			undo_redo->add_undo_method(this, "_refresh_menu_icons", Variant());
			undo_redo->commit_action();
		} break;
		case MENU_UNLOCK_SELECTED: {
			undo_redo->create_action(TTR("Unlock Selected"));

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *spatial = Object::cast_to<Spatial>(E->get());
				if (!spatial || !spatial->is_visible_in_tree()) {
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

			undo_redo->add_do_method(this, "_refresh_menu_icons", Variant());
			undo_redo->add_undo_method(this, "_refresh_menu_icons", Variant());
			undo_redo->commit_action();
		} break;
		case MENU_GROUP_SELECTED: {
			undo_redo->create_action(TTR("Group Selected"));

			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *spatial = Object::cast_to<Spatial>(E->get());
				if (!spatial || !spatial->is_visible_in_tree()) {
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

			undo_redo->add_do_method(this, "_refresh_menu_icons", Variant());
			undo_redo->add_undo_method(this, "_refresh_menu_icons", Variant());
			undo_redo->commit_action();
		} break;
		case MENU_UNGROUP_SELECTED: {
			undo_redo->create_action(TTR("Ungroup Selected"));
			List<Node *> &selection = editor_selection->get_selected_node_list();

			for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
				Spatial *spatial = Object::cast_to<Spatial>(E->get());
				if (!spatial || !spatial->is_visible_in_tree()) {
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

			undo_redo->add_do_method(this, "_refresh_menu_icons", Variant());
			undo_redo->add_undo_method(this, "_refresh_menu_icons", Variant());
			undo_redo->commit_action();
		} break;
	}
}

void SpatialEditor::_init_indicators() {
	{
		origin_enabled = true;
		grid_enabled = true;

		indicator_mat.instance();
		indicator_mat->set_flag(Material3D::FLAG_UNSHADED, true);
		indicator_mat->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		indicator_mat->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
		indicator_mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);

		Vector<Color> origin_colors;
		Vector<Vector3> origin_points;

		for (int i = 0; i < 3; i++) {
			Vector3 axis;
			axis[i] = 1;
			Color origin_color;
			switch (i) {
				case 0:
					origin_color = get_color("axis_x_color", "Editor");
					break;
				case 1:
					origin_color = get_color("axis_y_color", "Editor");
					break;
				case 2:
					origin_color = get_color("axis_z_color", "Editor");
					break;
				default:
					origin_color = Color();
					break;
			}

			grid_enable[i] = false;
			grid_visible[i] = false;

			origin_colors.push_back(origin_color);
			origin_colors.push_back(origin_color);
			origin_colors.push_back(origin_color);
			origin_colors.push_back(origin_color);
			origin_colors.push_back(origin_color);
			origin_colors.push_back(origin_color);
			// To both allow having a large origin size and avoid jitter
			// at small scales, we should segment the line into pieces.
			// 3 pieces seems to do the trick, and let's use powers of 2.
			origin_points.push_back(axis * 1048576);
			origin_points.push_back(axis * 1024);
			origin_points.push_back(axis * 1024);
			origin_points.push_back(axis * -1024);
			origin_points.push_back(axis * -1024);
			origin_points.push_back(axis * -1048576);
		}

		Ref<Shader> grid_shader = memnew(Shader);
		grid_shader->set_code(
				"\n"
				"shader_type spatial; \n"
				"render_mode unshaded; \n"
				"uniform bool orthogonal; \n"
				"uniform float grid_size; \n"
				"\n"
				"void vertex() { \n"
				"	// From FLAG_SRGB_VERTEX_COLOR \n"
				"	if (!OUTPUT_IS_SRGB) { \n"
				"		COLOR.rgb = mix(pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb * (1.0 / 12.92), lessThan(COLOR.rgb, vec3(0.04045))); \n"
				"	} \n"
				"} \n"
				"\n"
				"void fragment() { \n"
				"	ALBEDO = COLOR.rgb; \n"
				"	vec3 dir = orthogonal ? -vec3(0, 0, 1) : VIEW; \n"
				"	float angle_fade = abs(dot(dir, NORMAL)); \n"
				"	angle_fade = smoothstep(0.05, 0.2, angle_fade); \n"
				"	\n"
				"	vec3 world_pos = (CAMERA_MATRIX * vec4(VERTEX, 1.0)).xyz; \n"
				"	vec3 world_normal = (CAMERA_MATRIX * vec4(NORMAL, 0.0)).xyz; \n"
				"	vec3 camera_world_pos = CAMERA_MATRIX[3].xyz; \n"
				"	vec3 camera_world_pos_on_plane = camera_world_pos * (1.0 - world_normal); \n"
				"	float dist_fade = 1.0 - (distance(world_pos, camera_world_pos_on_plane) / grid_size); \n"
				"	dist_fade = smoothstep(0.02, 0.3, dist_fade); \n"
				"	\n"
				"	ALPHA = COLOR.a * dist_fade * angle_fade; \n"
				"}");

		for (int i = 0; i < 3; i++) {
			grid_mat[i].instance();
			grid_mat[i]->set_shader(grid_shader);
		}

		grid_enable[0] = EditorSettings::get_singleton()->get("editors/3d/grid_xy_plane");
		grid_enable[1] = EditorSettings::get_singleton()->get("editors/3d/grid_yz_plane");
		grid_enable[2] = EditorSettings::get_singleton()->get("editors/3d/grid_xz_plane");
		grid_visible[0] = grid_enable[0];
		grid_visible[1] = grid_enable[1];
		grid_visible[2] = grid_enable[2];

		_init_grid();

		origin = RID_PRIME(VisualServer::get_singleton()->mesh_create());
		Array d;
		d.resize(VS::ARRAY_MAX);
		d[VisualServer::ARRAY_VERTEX] = origin_points;
		d[VisualServer::ARRAY_COLOR] = origin_colors;

		VisualServer::get_singleton()->mesh_add_surface_from_arrays(origin, VisualServer::PRIMITIVE_LINES, d);
		VisualServer::get_singleton()->mesh_surface_set_material(origin, 0, indicator_mat->get_rid());

		origin_instance = VisualServer::get_singleton()->instance_create2(origin, get_tree()->get_root()->get_world()->get_scenario());
		VS::get_singleton()->instance_set_layer_mask(origin_instance, 1 << SpatialEditorViewport::GIZMO_GRID_LAYER);

		VisualServer::get_singleton()->instance_geometry_set_cast_shadows_setting(origin_instance, VS::SHADOW_CASTING_SETTING_OFF);
	}

	{
		//move gizmo

		for (int i = 0; i < 3; i++) {
			Color col;
			switch (i) {
				case 0:
					col = get_color("axis_x_color", "Editor");
					break;
				case 1:
					col = get_color("axis_y_color", "Editor");
					break;
				case 2:
					col = get_color("axis_z_color", "Editor");
					break;
				default:
					col = Color();
					break;
			}

			col.a = EditorSettings::get_singleton()->get("editors/3d/manipulator_gizmo_opacity");

			move_gizmo[i] = Ref<ArrayMesh>(memnew(ArrayMesh));
			move_plane_gizmo[i] = Ref<ArrayMesh>(memnew(ArrayMesh));
			rotate_gizmo[i] = Ref<ArrayMesh>(memnew(ArrayMesh));
			scale_gizmo[i] = Ref<ArrayMesh>(memnew(ArrayMesh));
			scale_plane_gizmo[i] = Ref<ArrayMesh>(memnew(ArrayMesh));

			Ref<Material3D> mat = memnew(SpatialMaterial);
			mat->set_flag(Material3D::FLAG_UNSHADED, true);
			mat->set_on_top_of_alpha();
			mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);
			mat->set_albedo(col);
			gizmo_color[i] = mat;

			Ref<Material3D> mat_hl = mat->duplicate();
			const Color albedo = col.from_hsv(col.get_h(), 0.25, 1.0, 1);
			mat_hl->set_albedo(albedo);
			gizmo_color_hl[i] = mat_hl;

			Vector3 ivec;
			ivec[i] = 1;
			Vector3 nivec;
			nivec[(i + 1) % 3] = 1;
			nivec[(i + 2) % 3] = 1;
			Vector3 ivec2;
			ivec2[(i + 1) % 3] = 1;
			Vector3 ivec3;
			ivec3[(i + 2) % 3] = 1;

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

				for (int k = 0; k < arrow_sides; k++) {
					Basis ma(ivec, Math_PI * 2 * float(k) / arrow_sides);
					Basis mb(ivec, Math_PI * 2 * float(k + 1) / arrow_sides);

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

				Ref<Material3D> plane_mat = memnew(SpatialMaterial);
				plane_mat->set_flag(Material3D::FLAG_UNSHADED, true);
				plane_mat->set_on_top_of_alpha();
				plane_mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);
				plane_mat->set_cull_mode(Material3D::CULL_DISABLED);
				plane_mat->set_albedo(col);
				plane_gizmo_color[i] = plane_mat; // needed, so we can draw planes from both sides
				surftool->set_material(plane_mat);
				surftool->commit(move_plane_gizmo[i]);

				Ref<Material3D> plane_mat_hl = plane_mat->duplicate();
				plane_mat_hl->set_albedo(albedo);
				plane_gizmo_color_hl[i] = plane_mat_hl; // needed, so we can draw planes from both sides
			}

			// Rotate
			{
				Ref<SurfaceTool> surftool = memnew(SurfaceTool);
				surftool->begin(Mesh::PRIMITIVE_TRIANGLES);

				int n = 128; // number of circle segments
				int m = 3; // number of thickness segments

				for (int j = 0; j < n; ++j) {
					Basis basis = Basis(ivec, (Math_PI * 2.0f * j) / n);
					Vector3 vertex = basis.xform(ivec2 * GIZMO_CIRCLE_SIZE);
					for (int k = 0; k < m; ++k) {
						Vector2 ofs = Vector2(Math::cos((Math_PI * 2.0 * k) / m), Math::sin((Math_PI * 2.0 * k) / m));
						Vector3 normal = ivec * ofs.x + ivec2 * ofs.y;
						surftool->add_normal(basis.xform(normal));
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

				rotate_shader->set_code(
						"\n"
						"shader_type spatial; \n"
						"render_mode unshaded, depth_test_disable; \n"
						"uniform vec4 albedo; \n"
						"\n"
						"mat3 orthonormalize(mat3 m) { \n"
						"	vec3 x = normalize(m[0]); \n"
						"	vec3 y = normalize(m[1] - x * dot(x, m[1])); \n"
						"	vec3 z = m[2] - x * dot(x, m[2]); \n"
						"	z = normalize(z - y * (dot(y,m[2]))); \n"
						"	return mat3(x,y,z); \n"
						"} \n"
						"\n"
						"void vertex() { \n"
						"	mat3 mv = orthonormalize(mat3(MODELVIEW_MATRIX)); \n"
						"	vec3 n = mv * VERTEX; \n"
						"	float orientation = dot(vec3(0,0,-1),n); \n"
						"	if (orientation <= 0.005) { \n"
						"		VERTEX += NORMAL*0.02; \n"
						"	} \n"
						"} \n"
						"\n"
						"void fragment() { \n"
						"	ALBEDO = albedo.rgb; \n"
						"	ALPHA = albedo.a; \n"
						"}");

				Ref<ShaderMaterial> rotate_mat = memnew(ShaderMaterial);
				rotate_mat->set_render_priority(Material::RENDER_PRIORITY_MAX);
				rotate_mat->set_shader(rotate_shader);
				rotate_mat->set_shader_param("albedo", col);
				rotate_gizmo_color[i] = rotate_mat;

				Array arrays = surftool->commit_to_arrays();
				rotate_gizmo[i]->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
				rotate_gizmo[i]->surface_set_material(0, rotate_mat);

				Ref<ShaderMaterial> rotate_mat_hl = rotate_mat->duplicate();
				rotate_mat_hl->set_shader_param("albedo", albedo);
				rotate_gizmo_color_hl[i] = rotate_mat_hl;

				if (i == 2) { // Rotation white outline
					Ref<ShaderMaterial> border_mat = rotate_mat->duplicate();

					Ref<Shader> border_shader = memnew(Shader);
					border_shader->set_code(
							"\n"
							"shader_type spatial; \n"
							"render_mode unshaded, depth_test_disable; \n"
							"uniform vec4 albedo; \n"
							"\n"
							"mat3 orthonormalize(mat3 m) { \n"
							"	vec3 x = normalize(m[0]); \n"
							"	vec3 y = normalize(m[1] - x * dot(x, m[1])); \n"
							"	vec3 z = m[2] - x * dot(x, m[2]); \n"
							"	z = normalize(z - y * (dot(y,m[2]))); \n"
							"	return mat3(x,y,z); \n"
							"} \n"
							"\n"
							"void vertex() { \n"
							"	mat3 mv = orthonormalize(mat3(MODELVIEW_MATRIX)); \n"
							"	mv = inverse(mv); \n"
							"	VERTEX += NORMAL*0.008; \n"
							"	vec3 camera_dir_local = mv * vec3(0,0,1); \n"
							"	vec3 camera_up_local = mv * vec3(0,1,0); \n"
							"	mat3 rotation_matrix = mat3(cross(camera_dir_local, camera_up_local), camera_up_local, camera_dir_local); \n"
							"	VERTEX = rotation_matrix * VERTEX; \n"
							"} \n"
							"\n"
							"void fragment() { \n"
							"	ALBEDO = albedo.rgb; \n"
							"	ALPHA = albedo.a; \n"
							"}");

					border_mat->set_shader(border_shader);
					border_mat->set_shader_param("albedo", Color(0.75, 0.75, 0.75, col.a / 3.0));

					rotate_gizmo[3] = Ref<ArrayMesh>(memnew(ArrayMesh));
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

				for (int k = 0; k < 4; k++) {
					Basis ma(ivec, Math_PI * 2 * float(k) / arrow_sides);
					Basis mb(ivec, Math_PI * 2 * float(k + 1) / arrow_sides);

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

				Ref<Material3D> plane_mat = memnew(SpatialMaterial);
				plane_mat->set_flag(Material3D::FLAG_UNSHADED, true);
				plane_mat->set_on_top_of_alpha();
				plane_mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);
				plane_mat->set_cull_mode(Material3D::CULL_DISABLED);
				plane_mat->set_albedo(col);
				plane_gizmo_color[i] = plane_mat; // needed, so we can draw planes from both sides
				surftool->set_material(plane_mat);
				surftool->commit(scale_plane_gizmo[i]);

				Ref<Material3D> plane_mat_hl = plane_mat->duplicate();
				plane_mat_hl->set_albedo(col.from_hsv(col.get_h(), 0.25, 1.0, 1));
				plane_gizmo_color_hl[i] = plane_mat_hl; // needed, so we can draw planes from both sides
			}
		}
	}

	_generate_selection_boxes();
}

void SpatialEditor::_update_context_menu_stylebox() {
	// This must be called when the theme changes to follow the new accent color.
	Ref<StyleBoxFlat> context_menu_stylebox = memnew(StyleBoxFlat);
	const Color accent_color = EditorNode::get_singleton()->get_gui_base()->get_color("accent_color", "Editor");
	context_menu_stylebox->set_bg_color(accent_color * Color(1, 1, 1, 0.1));
	// Add an underline to the StyleBox, but prevent its minimum vertical size from changing.
	context_menu_stylebox->set_border_color(accent_color);
	context_menu_stylebox->set_border_width(MARGIN_BOTTOM, Math::round(2 * EDSCALE));
	context_menu_stylebox->set_default_margin(MARGIN_BOTTOM, 0);
	context_menu_panel->add_style_override("panel", context_menu_stylebox);
}

void SpatialEditor::_update_gizmos_menu() {
	gizmos_menu->clear();

	for (int i = 0; i < gizmo_plugins_by_name.size(); ++i) {
		if (!gizmo_plugins_by_name[i]->can_be_hidden()) {
			continue;
		}
		String plugin_name = gizmo_plugins_by_name[i]->get_name();
		const int plugin_state = gizmo_plugins_by_name[i]->get_state();
		gizmos_menu->add_multistate_item(TTR(plugin_name), 3, plugin_state, i);
		const int idx = gizmos_menu->get_item_index(i);
		gizmos_menu->set_item_tooltip(
				idx,
				TTR("Click to toggle between visibility states.\n\nOpen eye: Gizmo is visible.\nClosed eye: Gizmo is hidden.\nHalf-open eye: Gizmo is also visible through opaque surfaces (\"x-ray\")."));
		switch (plugin_state) {
			case EditorSpatialGizmoPlugin::VISIBLE:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_visible"));
				break;
			case EditorSpatialGizmoPlugin::ON_TOP:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_xray"));
				break;
			case EditorSpatialGizmoPlugin::HIDDEN:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_hidden"));
				break;
		}
	}
}

void SpatialEditor::_update_gizmos_menu_theme() {
	for (int i = 0; i < gizmo_plugins_by_name.size(); ++i) {
		if (!gizmo_plugins_by_name[i]->can_be_hidden()) {
			continue;
		}
		const int plugin_state = gizmo_plugins_by_name[i]->get_state();
		const int idx = gizmos_menu->get_item_index(i);
		switch (plugin_state) {
			case EditorSpatialGizmoPlugin::VISIBLE:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_visible"));
				break;
			case EditorSpatialGizmoPlugin::ON_TOP:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_xray"));
				break;
			case EditorSpatialGizmoPlugin::HIDDEN:
				gizmos_menu->set_item_icon(idx, gizmos_menu->get_icon("visibility_hidden"));
				break;
		}
	}
}

void SpatialEditor::_init_grid() {
	if (!grid_enabled) {
		return;
	}
	Camera *camera = get_editor_viewport(0)->camera;
	Vector3 camera_position = camera->get_translation();
	if (camera_position == Vector3()) {
		return; // Camera is invalid, don't draw the grid.
	}

	bool orthogonal = camera->get_projection() == Camera::PROJECTION_ORTHOGONAL;

	static LocalVector<Color> grid_colors[3];
	static LocalVector<Vector3> grid_points[3];
	static LocalVector<Vector3> grid_normals[3];

	for (uint32_t n = 0; n < 3; n++) {
		grid_colors[n].clear();
		grid_points[n].clear();
		grid_normals[n].clear();
	}

	Color primary_grid_color = EditorSettings::get_singleton()->get("editors/3d/primary_grid_color");
	Color secondary_grid_color = EditorSettings::get_singleton()->get("editors/3d/secondary_grid_color");
	int grid_size = EditorSettings::get_singleton()->get("editors/3d/grid_size");
	int primary_grid_steps = EditorSettings::get_singleton()->get("editors/3d/primary_grid_steps");

	// Which grid planes are enabled? Which should we generate?
	grid_enable[0] = grid_visible[0] = EditorSettings::get_singleton()->get("editors/3d/grid_xy_plane");
	grid_enable[1] = grid_visible[1] = EditorSettings::get_singleton()->get("editors/3d/grid_yz_plane");
	grid_enable[2] = grid_visible[2] = EditorSettings::get_singleton()->get("editors/3d/grid_xz_plane");

	// Offsets division_level for bigger or smaller grids.
	// Default value is -0.2. -1.0 gives Blender-like behavior, 0.5 gives huge grids.
	real_t division_level_bias = EditorSettings::get_singleton()->get("editors/3d/grid_division_level_bias");
	// Default largest grid size is 8^2 when primary_grid_steps is 8 (64m apart, so primary grid lines are 512m apart).
	int division_level_max = EditorSettings::get_singleton()->get("editors/3d/grid_division_level_max");
	// Default smallest grid size is 1cm, 10^-2 (default value is -2).
	int division_level_min = EditorSettings::get_singleton()->get("editors/3d/grid_division_level_min");
	ERR_FAIL_COND_MSG(division_level_max < division_level_min, "The 3D grid's maximum division level cannot be lower than its minimum division level.");

	if (primary_grid_steps != 10) { // Log10 of 10 is 1.
		// Change of base rule, divide by ln(10).
		real_t div = Math::log((real_t)primary_grid_steps) / (real_t)2.302585092994045901094;
		// Trucation (towards zero) is intentional.
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
			Vector3 camera_direction = -camera->get_global_transform().get_basis().get_axis(2);
			Plane grid_plane = Plane(Vector3(), normal);
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
		grid_mat[c]->set_shader_param("grid_size", grid_fade_size);
		grid_mat[c]->set_shader_param("orthogonal", orthogonal);

		// In each iteration of this loop, draw one line in each direction (so two lines per loop, in each if statement).
		for (int i = -grid_size; i <= grid_size; i++) {
			Color line_color;
			// Is this a primary line? Set the appropriate color.
			if (i % primary_grid_steps == 0) {
				line_color = primary_grid_color.linear_interpolate(secondary_grid_color, division_level_decimals);
			} else {
				line_color = secondary_grid_color;
				line_color.a = line_color.a * (1 - division_level_decimals);
			}

			real_t position_a = center_a + i * small_step_size;
			real_t position_b = center_b + i * small_step_size;

			// Don't draw lines over the origin if it's enabled.
			if (!(origin_enabled && Math::is_zero_approx(position_a))) {
				Vector3 line_bgn = Vector3();
				Vector3 line_end = Vector3();
				line_bgn[a] = position_a;
				line_end[a] = position_a;
				line_bgn[b] = bgn_b;
				line_end[b] = end_b;
				grid_points[c].push_back(line_bgn);
				grid_points[c].push_back(line_end);
				grid_colors[c].push_back(line_color);
				grid_colors[c].push_back(line_color);
				grid_normals[c].push_back(normal);
				grid_normals[c].push_back(normal);
			}

			if (!(origin_enabled && Math::is_zero_approx(position_b))) {
				Vector3 line_bgn = Vector3();
				Vector3 line_end = Vector3();
				line_bgn[b] = position_b;
				line_end[b] = position_b;
				line_bgn[a] = bgn_a;
				line_end[a] = end_a;
				grid_points[c].push_back(line_bgn);
				grid_points[c].push_back(line_end);
				grid_colors[c].push_back(line_color);
				grid_colors[c].push_back(line_color);
				grid_normals[c].push_back(normal);
				grid_normals[c].push_back(normal);
			}
		}

		// Create a mesh from the pushed vector points and colors.
		grid[c] = RID_PRIME(VisualServer::get_singleton()->mesh_create());
		Array d;
		d.resize(VS::ARRAY_MAX);
		d[VisualServer::ARRAY_VERTEX] = (PoolVector<Vector3>)grid_points[c];
		d[VisualServer::ARRAY_COLOR] = (PoolVector<Color>)grid_colors[c];
		d[VisualServer::ARRAY_NORMAL] = (PoolVector<Vector3>)grid_normals[c];
		VisualServer::get_singleton()->mesh_add_surface_from_arrays(grid[c], VisualServer::PRIMITIVE_LINES, d);
		VisualServer::get_singleton()->mesh_surface_set_material(grid[c], 0, grid_mat[c]->get_rid());
		grid_instance[c] = VisualServer::get_singleton()->instance_create2(grid[c], get_tree()->get_root()->get_world()->get_scenario());

		// Yes, the end of this line is supposed to be a.
		VisualServer::get_singleton()->instance_set_visible(grid_instance[c], grid_visible[a]);
		VisualServer::get_singleton()->instance_geometry_set_cast_shadows_setting(grid_instance[c], VS::SHADOW_CASTING_SETTING_OFF);
		VS::get_singleton()->instance_set_layer_mask(grid_instance[c], 1 << SpatialEditorViewport::GIZMO_GRID_LAYER);
	}
}

void SpatialEditor::_finish_indicators() {
	if (origin_instance.is_valid()) {
		VisualServer::get_singleton()->free(origin_instance);
		origin_instance = RID();
	}

	if (origin.is_valid()) {
		VisualServer::get_singleton()->free(origin);
		origin = RID();
	}

	_finish_grid();
}

void SpatialEditor::_finish_grid() {
	for (int i = 0; i < 3; i++) {
		if (grid_instance[i].is_valid()) {
			VisualServer::get_singleton()->free(grid_instance[i]);
			grid_instance[i] = RID();
		}

		if (grid[i].is_valid()) {
			VisualServer::get_singleton()->free(grid[i]);
			grid[i] = RID();
		}
	}
}

void SpatialEditor::update_grid() {
	_finish_grid();
	_init_grid();
}

bool SpatialEditor::is_any_freelook_active() const {
	for (unsigned int i = 0; i < VIEWPORTS_COUNT; ++i) {
		if (viewports[i]->is_freelook_active()) {
			return true;
		}
	}
	return false;
}

void SpatialEditor::_selection_changed() {
	_refresh_menu_icons();

	if (selected && editor_selection->get_selected_node_list().size() != 1) {
		Ref<EditorSpatialGizmo> gizmo = selected->get_gizmo();
		if (gizmo.is_valid()) {
			gizmo->set_selected(false);
			selected->update_gizmo();
		}
		selected = nullptr;
		over_gizmo_handle = -1;
	}
}

void SpatialEditor::_refresh_menu_icons() {
	bool all_locked = true;
	bool all_grouped = true;

	List<Node *> &selection = editor_selection->get_selected_node_list();

	if (selection.empty()) {
		all_locked = false;
		all_grouped = false;
	} else {
		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			if (Object::cast_to<Spatial>(E->get()) && !Object::cast_to<Spatial>(E->get())->has_meta("_edit_lock_")) {
				all_locked = false;
				break;
			}
		}
		for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
			if (Object::cast_to<Spatial>(E->get()) && !Object::cast_to<Spatial>(E->get())->has_meta("_edit_group_")) {
				all_grouped = false;
				break;
			}
		}
	}

	tool_button[TOOL_LOCK_SELECTED]->set_visible(!all_locked);
	tool_button[TOOL_LOCK_SELECTED]->set_disabled(selection.empty());
	tool_button[TOOL_UNLOCK_SELECTED]->set_visible(all_locked);

	tool_button[TOOL_GROUP_SELECTED]->set_visible(!all_grouped);
	tool_button[TOOL_GROUP_SELECTED]->set_disabled(selection.empty());
	tool_button[TOOL_UNGROUP_SELECTED]->set_visible(all_grouped);
}

template <typename T>
Set<T *> _get_child_nodes(Node *parent_node) {
	Set<T *> nodes = Set<T *>();
	T *node = Node::cast_to<T>(parent_node);
	if (node) {
		nodes.insert(node);
	}

	for (int i = 0; i < parent_node->get_child_count(); i++) {
		Node *child_node = parent_node->get_child(i);
		Set<T *> child_nodes = _get_child_nodes<T>(child_node);
		for (typename Set<T *>::Element *I = child_nodes.front(); I; I = I->next()) {
			nodes.insert(I->get());
		}
	}

	return nodes;
}

Set<RID> _get_physics_bodies_rid(Node *node) {
	Set<RID> rids = Set<RID>();
	PhysicsBody *pb = Node::cast_to<PhysicsBody>(node);
	if (pb) {
		rids.insert(pb->get_rid());
	}
	Set<PhysicsBody *> child_nodes = _get_child_nodes<PhysicsBody>(node);
	for (Set<PhysicsBody *>::Element *I = child_nodes.front(); I; I = I->next()) {
		rids.insert(I->get()->get_rid());
	}

	return rids;
}

void SpatialEditor::snap_selected_nodes_to_floor() {
	List<Node *> &selection = editor_selection->get_selected_node_list();
	Dictionary snap_data;

	for (List<Node *>::Element *E = selection.front(); E; E = E->next()) {
		Spatial *sp = Object::cast_to<Spatial>(E->get());
		if (sp) {
			Vector3 from = Vector3();
			Vector3 position_offset = Vector3();

			// Priorities for snapping to floor are CollisionShapes, VisualInstances and then origin
			Set<VisualInstance *> vi = _get_child_nodes<VisualInstance>(sp);
			Set<CollisionShape *> cs = _get_child_nodes<CollisionShape>(sp);
			bool found_valid_shape = false;

			if (cs.size()) {
				AABB aabb;
				Set<CollisionShape *>::Element *I = cs.front();
				if (I->get()->get_shape().is_valid()) {
					CollisionShape *collision_shape = cs.front()->get();
					aabb = collision_shape->get_global_transform().xform(collision_shape->get_shape()->get_debug_mesh()->get_aabb());
					found_valid_shape = true;
				}
				for (I = I->next(); I; I = I->next()) {
					CollisionShape *col_shape = I->get();
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
				AABB aabb = vi.front()->get()->get_transformed_aabb();
				for (Set<VisualInstance *>::Element *I = vi.front(); I; I = I->next()) {
					aabb.merge_with(I->get()->get_transformed_aabb());
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

	PhysicsDirectSpaceState *ss = get_tree()->get_root()->get_world()->get_direct_space_state();
	PhysicsDirectSpaceState::RayResult result;

	Array keys = snap_data.keys();

	// The maximum height an object can travel to be snapped
	const float max_snap_height = 500.0;

	// Will be set to `true` if at least one node from the selection was successfully snapped
	bool snapped_to_floor = false;

	if (keys.size()) {
		// For snapping to be performed, there must be solid geometry under at least one of the selected nodes.
		// We need to check this before snapping to register the undo/redo action only if needed.
		for (int i = 0; i < keys.size(); i++) {
			Node *node = keys[i];
			Spatial *sp = Object::cast_to<Spatial>(node);
			Dictionary d = snap_data[node];
			Vector3 from = d["from"];
			Vector3 to = from - Vector3(0.0, max_snap_height, 0.0);
			Set<RID> excluded = _get_physics_bodies_rid(sp);

			if (ss->intersect_ray(from, to, result, excluded)) {
				snapped_to_floor = true;
			}
		}

		if (snapped_to_floor) {
			undo_redo->create_action(TTR("Snap Nodes to Floor"));

			// Perform snapping if at least one node can be snapped
			for (int i = 0; i < keys.size(); i++) {
				Node *node = keys[i];
				Spatial *sp = Object::cast_to<Spatial>(node);
				Dictionary d = snap_data[node];
				Vector3 from = d["from"];
				Vector3 to = from - Vector3(0.0, max_snap_height, 0.0);
				Set<RID> excluded = _get_physics_bodies_rid(sp);

				if (ss->intersect_ray(from, to, result, excluded)) {
					Vector3 position_offset = d["position_offset"];
					Transform new_transform = sp->get_global_transform();

					new_transform.origin.y = result.position.y;
					new_transform.origin = new_transform.origin - position_offset;

					undo_redo->add_do_method(sp, "set_global_transform", new_transform);
					undo_redo->add_undo_method(sp, "set_global_transform", sp->get_global_transform());
				}
			}

			undo_redo->commit_action();
		} else {
			EditorNode::get_singleton()->show_warning(TTR("Couldn't find a solid floor to snap the selection to."));
		}
	}
}

void SpatialEditor::_unhandled_key_input(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_visible_in_tree() || get_viewport()->gui_has_modal_stack()) {
		return;
	}

	snap_key_enabled = Input::get_singleton()->is_key_pressed(KEY_CONTROL);
}
void SpatialEditor::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		tool_button[SpatialEditor::TOOL_MODE_SELECT]->set_icon(get_icon("ToolSelect", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_MOVE]->set_icon(get_icon("ToolMove", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_ROTATE]->set_icon(get_icon("ToolRotate", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_SCALE]->set_icon(get_icon("ToolScale", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_LIST_SELECT]->set_icon(get_icon("ListSelect", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_LOCK_SELECTED]->set_icon(get_icon("Lock", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_UNLOCK_SELECTED]->set_icon(get_icon("Unlock", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_GROUP_SELECTED]->set_icon(get_icon("Group", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_UNGROUP_SELECTED]->set_icon(get_icon("Ungroup", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_CONVERT_ROOMS]->set_icon(get_icon("RoomGroup", "EditorIcons"));

		tool_option_button[SpatialEditor::TOOL_OPT_LOCAL_COORDS]->set_icon(get_icon("Object", "EditorIcons"));
		tool_option_button[SpatialEditor::TOOL_OPT_USE_SNAP]->set_icon(get_icon("Snap", "EditorIcons"));
		tool_option_button[SpatialEditor::TOOL_OPT_OVERRIDE_CAMERA]->set_icon(get_icon("Camera", "EditorIcons"));

		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), get_icon("Panels1", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), get_icon("Panels2", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), get_icon("Panels2Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), get_icon("Panels3", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), get_icon("Panels3Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), get_icon("Panels4", "EditorIcons"));

		_menu_item_pressed(MENU_VIEW_USE_1_VIEWPORT);

		_refresh_menu_icons();

		get_tree()->connect("node_removed", this, "_node_removed");
		EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor()->connect("node_changed", this, "_refresh_menu_icons");
		editor_selection->connect("selection_changed", this, "_selection_changed");

		editor->connect("stop_pressed", this, "_update_camera_override_button", make_binds(false));
		editor->connect("play_pressed", this, "_update_camera_override_button", make_binds(true));
	} else if (p_what == NOTIFICATION_ENTER_TREE) {
		_register_all_gizmos();
		_update_gizmos_menu();
		_init_indicators();
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		_update_gizmos_menu_theme();
		_update_context_menu_stylebox();
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		_finish_indicators();
	} else if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
		tool_button[SpatialEditor::TOOL_MODE_SELECT]->set_icon(get_icon("ToolSelect", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_MOVE]->set_icon(get_icon("ToolMove", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_ROTATE]->set_icon(get_icon("ToolRotate", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_SCALE]->set_icon(get_icon("ToolScale", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_MODE_LIST_SELECT]->set_icon(get_icon("ListSelect", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_LOCK_SELECTED]->set_icon(get_icon("Lock", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_UNLOCK_SELECTED]->set_icon(get_icon("Unlock", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_GROUP_SELECTED]->set_icon(get_icon("Group", "EditorIcons"));
		tool_button[SpatialEditor::TOOL_UNGROUP_SELECTED]->set_icon(get_icon("Ungroup", "EditorIcons"));

		tool_option_button[SpatialEditor::TOOL_OPT_LOCAL_COORDS]->set_icon(get_icon("Object", "EditorIcons"));
		tool_option_button[SpatialEditor::TOOL_OPT_USE_SNAP]->set_icon(get_icon("Snap", "EditorIcons"));

		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_1_VIEWPORT), get_icon("Panels1", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS), get_icon("Panels2", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_2_VIEWPORTS_ALT), get_icon("Panels2Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS), get_icon("Panels3", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_3_VIEWPORTS_ALT), get_icon("Panels3Alt", "EditorIcons"));
		view_menu->get_popup()->set_item_icon(view_menu->get_popup()->get_item_index(MENU_VIEW_USE_4_VIEWPORTS), get_icon("Panels4", "EditorIcons"));

		// Update grid color by rebuilding grid.
		_finish_grid();
		_init_grid();
	} else if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		if (!is_visible() && tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->is_pressed()) {
			ScriptEditorDebugger *debugger = ScriptEditor::get_singleton()->get_debugger();

			debugger->set_camera_override(ScriptEditorDebugger::OVERRIDE_NONE);
			tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->set_pressed(false);
		}
	}
}

void SpatialEditor::add_control_to_menu_panel(Control *p_control) {
	context_menu_hbox->add_child(p_control);
}

void SpatialEditor::remove_control_from_menu_panel(Control *p_control) {
	context_menu_hbox->remove_child(p_control);
}

void SpatialEditor::set_can_preview(Camera *p_preview) {
	for (int i = 0; i < 4; i++) {
		viewports[i]->set_can_preview(p_preview);
	}
}

void SpatialEditor::set_message(String p_message, float p_time) {
	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		SpatialEditorViewport *viewport = get_editor_viewport(i);
		if (viewport->is_visible()) {
			viewport->set_message(p_message, p_time);
		}
	}
}

VSplitContainer *SpatialEditor::get_shader_split() {
	return shader_split;
}

void SpatialEditor::add_control_to_left_panel(Control *p_control) {
	left_panel_split->add_child(p_control);
	left_panel_split->move_child(p_control, 0);
}

void SpatialEditor::add_control_to_right_panel(Control *p_control) {
	right_panel_split->add_child(p_control);
	right_panel_split->move_child(p_control, 1);
}

void SpatialEditor::remove_control_from_left_panel(Control *p_control) {
	left_panel_split->remove_child(p_control);
}

void SpatialEditor::remove_control_from_right_panel(Control *p_control) {
	right_panel_split->remove_child(p_control);
}

void SpatialEditor::move_control_to_left_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	if (p_control->get_parent() == left_panel_split) {
		return;
	}

	ERR_FAIL_COND(p_control->get_parent() != right_panel_split);
	right_panel_split->remove_child(p_control);

	add_control_to_left_panel(p_control);
}

void SpatialEditor::move_control_to_right_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	if (p_control->get_parent() == right_panel_split) {
		return;
	}

	ERR_FAIL_COND(p_control->get_parent() != left_panel_split);
	left_panel_split->remove_child(p_control);

	add_control_to_right_panel(p_control);
}

void SpatialEditor::_request_gizmo(Object *p_obj) {
	if (_prevent_gizmo_generation) {
		return;
	}

	Spatial *sp = Object::cast_to<Spatial>(p_obj);
	if (!sp) {
		return;
	}
	if (editor->get_edited_scene() && (sp == editor->get_edited_scene() || (sp->get_owner() && editor->get_edited_scene()->is_a_parent_of(sp)))) {
		Ref<EditorSpatialGizmo> seg;

		for (int i = 0; i < gizmo_plugins_by_priority.size(); ++i) {
			seg = gizmo_plugins_by_priority.write[i]->get_gizmo(sp);

			if (seg.is_valid()) {
				sp->set_gizmo(seg);

				if (sp == selected) {
					seg->set_selected(true);
					selected->update_gizmo();
				}

				break;
			}
		}
	}
}

void SpatialEditor::_toggle_maximize_view(Object *p_viewport) {
	if (!p_viewport) {
		return;
	}
	SpatialEditorViewport *current_viewport = Object::cast_to<SpatialEditorViewport>(p_viewport);
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
				viewports[i]->set_anchors_and_margins_preset(Control::PRESET_WIDE);
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

void SpatialEditor::_node_removed(Node *p_node) {
	if (p_node == selected) {
		selected = nullptr;
	}
}

void SpatialEditor::_register_all_gizmos() {
	add_gizmo_plugin(Ref<CameraSpatialGizmoPlugin>(memnew(CameraSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<LightSpatialGizmoPlugin>(memnew(LightSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<AudioStreamPlayer3DSpatialGizmoPlugin>(memnew(AudioStreamPlayer3DSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<ListenerSpatialGizmoPlugin>(memnew(ListenerSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<MeshInstanceSpatialGizmoPlugin>(memnew(MeshInstanceSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<SoftBodySpatialGizmoPlugin>(memnew(SoftBodySpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<SpriteBase3DSpatialGizmoPlugin>(memnew(SpriteBase3DSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<Label3DSpatialGizmoPlugin>(memnew(Label3DSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<SkeletonSpatialGizmoPlugin>(memnew(SkeletonSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<Position3DSpatialGizmoPlugin>(memnew(Position3DSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<RayCastSpatialGizmoPlugin>(memnew(RayCastSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<ShapeCastGizmoPlugin>(memnew(ShapeCastGizmoPlugin)));
	add_gizmo_plugin(Ref<SpringArmSpatialGizmoPlugin>(memnew(SpringArmSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<VehicleWheelSpatialGizmoPlugin>(memnew(VehicleWheelSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<VisibilityNotifierGizmoPlugin>(memnew(VisibilityNotifierGizmoPlugin)));
	add_gizmo_plugin(Ref<ParticlesGizmoPlugin>(memnew(ParticlesGizmoPlugin)));
	add_gizmo_plugin(Ref<CPUParticlesGizmoPlugin>(memnew(CPUParticlesGizmoPlugin)));
	add_gizmo_plugin(Ref<ReflectionProbeGizmoPlugin>(memnew(ReflectionProbeGizmoPlugin)));
	add_gizmo_plugin(Ref<GIProbeGizmoPlugin>(memnew(GIProbeGizmoPlugin)));
	add_gizmo_plugin(Ref<BakedIndirectLightGizmoPlugin>(memnew(BakedIndirectLightGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionObjectGizmoPlugin>(memnew(CollisionObjectGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionShapeSpatialGizmoPlugin>(memnew(CollisionShapeSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<CollisionPolygonSpatialGizmoPlugin>(memnew(CollisionPolygonSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<NavigationMeshSpatialGizmoPlugin>(memnew(NavigationMeshSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<JointSpatialGizmoPlugin>(memnew(JointSpatialGizmoPlugin)));
	add_gizmo_plugin(Ref<PhysicalBoneSpatialGizmoPlugin>(memnew(PhysicalBoneSpatialGizmoPlugin)));
}

void SpatialEditor::_bind_methods() {
	ClassDB::bind_method("_unhandled_key_input", &SpatialEditor::_unhandled_key_input);
	ClassDB::bind_method("_node_removed", &SpatialEditor::_node_removed);
	ClassDB::bind_method("_menu_item_pressed", &SpatialEditor::_menu_item_pressed);
	ClassDB::bind_method("_menu_gizmo_toggled", &SpatialEditor::_menu_gizmo_toggled);
	ClassDB::bind_method("_menu_item_toggled", &SpatialEditor::_menu_item_toggled);
	ClassDB::bind_method("_xform_dialog_action", &SpatialEditor::_xform_dialog_action);
	ClassDB::bind_method("_get_editor_data", &SpatialEditor::_get_editor_data);
	ClassDB::bind_method("_request_gizmo", &SpatialEditor::_request_gizmo);
	ClassDB::bind_method("_toggle_maximize_view", &SpatialEditor::_toggle_maximize_view);
	ClassDB::bind_method("_selection_changed", &SpatialEditor::_selection_changed);
	ClassDB::bind_method("_refresh_menu_icons", &SpatialEditor::_refresh_menu_icons);
	ClassDB::bind_method("_update_camera_override_button", &SpatialEditor::_update_camera_override_button);
	ClassDB::bind_method("_update_camera_override_viewport", &SpatialEditor::_update_camera_override_viewport);
	ClassDB::bind_method("_snap_changed", &SpatialEditor::_snap_changed);
	ClassDB::bind_method("_snap_update", &SpatialEditor::_snap_update);

	ADD_SIGNAL(MethodInfo("transform_key_request"));
	ADD_SIGNAL(MethodInfo("item_lock_status_changed"));
	ADD_SIGNAL(MethodInfo("item_group_status_changed"));
}

void SpatialEditor::clear() {
	settings_fov->set_value(EDITOR_GET("editors/3d/default_fov"));
	settings_znear->set_value(EDITOR_GET("editors/3d/default_z_near"));
	settings_zfar->set_value(EDITOR_GET("editors/3d/default_z_far"));

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->reset();
	}

	VisualServer::get_singleton()->instance_set_visible(origin_instance, true);
	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_ORIGIN), true);
	for (int i = 0; i < 3; ++i) {
		if (grid_enable[i]) {
			grid_visible[i] = true;
		}
	}

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i]->view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(SpatialEditorViewport::VIEW_AUDIO_LISTENER), i == 0);
		viewports[i]->viewport->set_as_audio_listener(i == 0);
	}

	view_menu->get_popup()->set_item_checked(view_menu->get_popup()->get_item_index(MENU_VIEW_GRID), true);
}

SpatialEditor::SpatialEditor(EditorNode *p_editor) {
	gizmo.visible = true;
	gizmo.scale = 1.0;

	viewport_environment = Ref<Environment>(memnew(Environment));
	undo_redo = p_editor->get_undo_redo();
	VBoxContainer *vbc = this;

	custom_camera = nullptr;
	singleton = this;
	editor = p_editor;
	editor_selection = editor->get_editor_selection();
	editor_selection->add_editor_plugin(this);

	snap_enabled = false;
	snap_key_enabled = false;
	tool_mode = TOOL_MODE_SELECT;

	camera_override_viewport_id = 0;

	// A fluid container for all toolbars.
	HFlowContainer *main_flow = memnew(HFlowContainer);
	vbc->add_child(main_flow);

	// Main toolbars.
	HBoxContainer *main_menu_hbox = memnew(HBoxContainer);
	main_flow->add_child(main_menu_hbox);

	Vector<Variant> button_binds;
	button_binds.resize(1);
	String sct;

	tool_button[TOOL_MODE_SELECT] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_SELECT]);
	tool_button[TOOL_MODE_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SELECT]->set_flat(true);
	tool_button[TOOL_MODE_SELECT]->set_pressed(true);
	button_binds.write[0] = MENU_TOOL_SELECT;
	tool_button[TOOL_MODE_SELECT]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_SELECT]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_select", TTR("Select Mode"), KEY_Q));
	tool_button[TOOL_MODE_SELECT]->set_tooltip(keycode_get_string(KEY_MASK_CMD) + TTR("Drag: Rotate selected node around pivot.") + "\n" + TTR("Alt+RMB: Show list of all nodes at position clicked, including locked."));
	main_menu_hbox->add_child(memnew(VSeparator));

	tool_button[TOOL_MODE_MOVE] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_MOVE]);
	tool_button[TOOL_MODE_MOVE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_MOVE]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_MOVE;
	tool_button[TOOL_MODE_MOVE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_MOVE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_move", TTR("Move Mode"), KEY_W));

	tool_button[TOOL_MODE_ROTATE] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_ROTATE]);
	tool_button[TOOL_MODE_ROTATE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_ROTATE]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_ROTATE;
	tool_button[TOOL_MODE_ROTATE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_ROTATE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_rotate", TTR("Rotate Mode"), KEY_E));

	tool_button[TOOL_MODE_SCALE] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_SCALE]);
	tool_button[TOOL_MODE_SCALE]->set_toggle_mode(true);
	tool_button[TOOL_MODE_SCALE]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_SCALE;
	tool_button[TOOL_MODE_SCALE]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_SCALE]->set_shortcut(ED_SHORTCUT("spatial_editor/tool_scale", TTR("Scale Mode"), KEY_R));

	main_menu_hbox->add_child(memnew(VSeparator));

	tool_button[TOOL_MODE_LIST_SELECT] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_MODE_LIST_SELECT]);
	tool_button[TOOL_MODE_LIST_SELECT]->set_toggle_mode(true);
	tool_button[TOOL_MODE_LIST_SELECT]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_LIST_SELECT;
	tool_button[TOOL_MODE_LIST_SELECT]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_MODE_LIST_SELECT]->set_tooltip(TTR("Show a list of all objects at the position clicked\n(same as Alt+RMB in select mode)."));

	tool_button[TOOL_LOCK_SELECTED] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_LOCK_SELECTED]);
	button_binds.write[0] = MENU_LOCK_SELECTED;
	tool_button[TOOL_LOCK_SELECTED]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_LOCK_SELECTED]->set_tooltip(TTR("Lock the selected object in place (can't be moved)."));
	tool_button[TOOL_LOCK_SELECTED]->set_shortcut(ED_SHORTCUT("editor/lock_selected_nodes", TTR("Lock Selected Node(s)"), KEY_MASK_CMD | KEY_L));

	tool_button[TOOL_UNLOCK_SELECTED] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_UNLOCK_SELECTED]);
	button_binds.write[0] = MENU_UNLOCK_SELECTED;
	tool_button[TOOL_UNLOCK_SELECTED]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_UNLOCK_SELECTED]->set_tooltip(TTR("Unlock the selected object (can be moved)."));
	tool_button[TOOL_UNLOCK_SELECTED]->set_shortcut(ED_SHORTCUT("editor/unlock_selected_nodes", TTR("Unlock Selected Node(s)"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_L));

	tool_button[TOOL_GROUP_SELECTED] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_GROUP_SELECTED]);
	button_binds.write[0] = MENU_GROUP_SELECTED;
	tool_button[TOOL_GROUP_SELECTED]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_GROUP_SELECTED]->set_tooltip(TTR("Make selected node's children not selectable."));
	tool_button[TOOL_GROUP_SELECTED]->set_shortcut(ED_SHORTCUT("editor/group_selected_nodes", TTR("Group Selected Node(s)"), KEY_MASK_CMD | KEY_G));

	tool_button[TOOL_UNGROUP_SELECTED] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_UNGROUP_SELECTED]);
	button_binds.write[0] = MENU_UNGROUP_SELECTED;
	tool_button[TOOL_UNGROUP_SELECTED]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_UNGROUP_SELECTED]->set_tooltip(TTR("Make selected node's children selectable."));
	tool_button[TOOL_UNGROUP_SELECTED]->set_shortcut(ED_SHORTCUT("editor/ungroup_selected_nodes", TTR("Ungroup Selected Node(s)"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_G));

	main_menu_hbox->add_child(memnew(VSeparator));

	tool_option_button[TOOL_OPT_LOCAL_COORDS] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_option_button[TOOL_OPT_LOCAL_COORDS]);
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_toggle_mode(true);
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_LOCAL_COORDS;
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->connect("toggled", this, "_menu_item_toggled", button_binds);
	tool_option_button[TOOL_OPT_LOCAL_COORDS]->set_shortcut(ED_SHORTCUT("spatial_editor/local_coords", TTR("Use Local Space"), KEY_T));

	tool_option_button[TOOL_OPT_USE_SNAP] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_option_button[TOOL_OPT_USE_SNAP]);
	tool_option_button[TOOL_OPT_USE_SNAP]->set_toggle_mode(true);
	tool_option_button[TOOL_OPT_USE_SNAP]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_USE_SNAP;
	tool_option_button[TOOL_OPT_USE_SNAP]->connect("toggled", this, "_menu_item_toggled", button_binds);
	tool_option_button[TOOL_OPT_USE_SNAP]->set_shortcut(ED_SHORTCUT("spatial_editor/snap", TTR("Use Snap"), KEY_Y));

	main_menu_hbox->add_child(memnew(VSeparator));

	tool_option_button[TOOL_OPT_OVERRIDE_CAMERA] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]);
	tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->set_toggle_mode(true);
	tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->set_flat(true);
	tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->set_disabled(true);
	button_binds.write[0] = MENU_TOOL_OVERRIDE_CAMERA;
	tool_option_button[TOOL_OPT_OVERRIDE_CAMERA]->connect("toggled", this, "_menu_item_toggled", button_binds);
	_update_camera_override_button(false);

	tool_button[TOOL_CONVERT_ROOMS] = memnew(ToolButton);
	main_menu_hbox->add_child(tool_button[TOOL_CONVERT_ROOMS]);
	tool_button[TOOL_CONVERT_ROOMS]->set_toggle_mode(false);
	tool_button[TOOL_CONVERT_ROOMS]->set_flat(true);
	button_binds.write[0] = MENU_TOOL_CONVERT_ROOMS;
	tool_button[TOOL_CONVERT_ROOMS]->connect("pressed", this, "_menu_item_pressed", button_binds);
	tool_button[TOOL_CONVERT_ROOMS]->set_shortcut(ED_SHORTCUT("spatial_editor/convert_rooms", TTR("Convert Rooms"), KEY_MASK_ALT | KEY_C));
	tool_button[TOOL_CONVERT_ROOMS]->set_tooltip(TTR("Converts rooms for portal culling."));

	main_menu_hbox->add_child(memnew(VSeparator));

	// Drag and drop support;
	preview_node = memnew(Spatial);
	preview_bounds = AABB();

	ED_SHORTCUT("spatial_editor/bottom_view", TTR("Bottom View"), KEY_MASK_ALT + KEY_KP_7);
	ED_SHORTCUT("spatial_editor/top_view", TTR("Top View"), KEY_KP_7);
	ED_SHORTCUT("spatial_editor/rear_view", TTR("Rear View"), KEY_MASK_ALT + KEY_KP_1);
	ED_SHORTCUT("spatial_editor/front_view", TTR("Front View"), KEY_KP_1);
	ED_SHORTCUT("spatial_editor/left_view", TTR("Left View"), KEY_MASK_ALT + KEY_KP_3);
	ED_SHORTCUT("spatial_editor/right_view", TTR("Right View"), KEY_KP_3);
	ED_SHORTCUT("spatial_editor/orbit_view_down", TTR("Orbit View Down"), KEY_KP_2);
	ED_SHORTCUT("spatial_editor/orbit_view_left", TTR("Orbit View Left"), KEY_KP_4);
	ED_SHORTCUT("spatial_editor/orbit_view_right", TTR("Orbit View Right"), KEY_KP_6);
	ED_SHORTCUT("spatial_editor/orbit_view_up", TTR("Orbit View Up"), KEY_KP_8);
	ED_SHORTCUT("spatial_editor/orbit_view_180", TTR("Orbit View 180"), KEY_KP_9);
	ED_SHORTCUT("spatial_editor/switch_perspective_orthogonal", TTR("Switch Perspective/Orthogonal View"), KEY_KP_5);
	ED_SHORTCUT("spatial_editor/insert_anim_key", TTR("Insert Animation Key"), KEY_K);
	ED_SHORTCUT("spatial_editor/focus_origin", TTR("Focus Origin"), KEY_O);
	ED_SHORTCUT("spatial_editor/focus_selection", TTR("Focus Selection"), KEY_F);
	ED_SHORTCUT("spatial_editor/align_transform_with_view", TTR("Align Transform with View"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_M);
	ED_SHORTCUT("spatial_editor/align_rotation_with_view", TTR("Align Rotation with View"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_F);
	ED_SHORTCUT("spatial_editor/freelook_toggle", TTR("Toggle Freelook"), KEY_MASK_SHIFT + KEY_F);
	ED_SHORTCUT("spatial_editor/decrease_fov", TTR("Decrease Field of View"), KEY_MASK_CMD + KEY_EQUAL); // Usually direct access key for `KEY_PLUS`.
	ED_SHORTCUT("spatial_editor/increase_fov", TTR("Increase Field of View"), KEY_MASK_CMD + KEY_MINUS);
	ED_SHORTCUT("spatial_editor/reset_fov", TTR("Reset Field of View to Default"), KEY_MASK_CMD + KEY_0);

	PopupMenu *p;

	transform_menu = memnew(MenuButton);
	transform_menu->set_text(TTR("Transform"));
	transform_menu->set_switch_on_hover(true);
	main_menu_hbox->add_child(transform_menu);

	p = transform_menu->get_popup();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/snap_to_floor", TTR("Snap Object to Floor"), KEY_PAGEDOWN), MENU_SNAP_TO_FLOOR);
	p->add_shortcut(ED_SHORTCUT("spatial_editor/transform_dialog", TTR("Transform Dialog...")), MENU_TRANSFORM_DIALOG);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/configure_snap", TTR("Configure Snap...")), MENU_TRANSFORM_CONFIGURE_SNAP);

	p->connect("id_pressed", this, "_menu_item_pressed");

	view_menu = memnew(MenuButton);
	// TRANSLATORS: Noun, name of the 2D/3D View menus.
	view_menu->set_text(TTR("View"));
	view_menu->set_switch_on_hover(true);
	main_menu_hbox->add_child(view_menu);

	main_menu_hbox->add_child(memnew(VSeparator));

	context_menu_panel = memnew(PanelContainer);
	context_menu_hbox = memnew(HBoxContainer);
	context_menu_panel->add_child(context_menu_hbox);
	// Use a custom stylebox to make contextual menu items stand out from the rest.
	// This helps with editor usability as contextual menu items change when selecting nodes,
	// even though it may not be immediately obvious at first.
	main_flow->add_child(context_menu_panel);
	_update_context_menu_stylebox();

	// Get the view menu popup and have it stay open when a checkable item is selected
	p = view_menu->get_popup();
	p->set_hide_on_checkable_item_selection(false);

	accept = memnew(AcceptDialog);
	editor->get_gui_base()->add_child(accept);

	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/1_viewport", TTR("1 Viewport"), KEY_MASK_CMD + KEY_1), MENU_VIEW_USE_1_VIEWPORT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports", TTR("2 Viewports"), KEY_MASK_CMD + KEY_2), MENU_VIEW_USE_2_VIEWPORTS);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/2_viewports_alt", TTR("2 Viewports (Alt)"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_2), MENU_VIEW_USE_2_VIEWPORTS_ALT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports", TTR("3 Viewports"), KEY_MASK_CMD + KEY_3), MENU_VIEW_USE_3_VIEWPORTS);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/3_viewports_alt", TTR("3 Viewports (Alt)"), KEY_MASK_ALT + KEY_MASK_CMD + KEY_3), MENU_VIEW_USE_3_VIEWPORTS_ALT);
	p->add_radio_check_shortcut(ED_SHORTCUT("spatial_editor/4_viewports", TTR("4 Viewports"), KEY_MASK_CMD + KEY_4), MENU_VIEW_USE_4_VIEWPORTS);
	p->add_separator();

	p->add_submenu_item(TTR("Gizmos"), "GizmosMenu");

	p->add_separator();
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_origin", TTR("View Origin")), MENU_VIEW_ORIGIN);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_grid", TTR("View Grid"), KEY_NUMBERSIGN), MENU_VIEW_GRID);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_portal_culling", TTR("View Portal Culling"), KEY_MASK_ALT | KEY_P), MENU_VIEW_PORTAL_CULLING);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_occlusion_culling", TTR("View Occlusion Culling")), MENU_VIEW_OCCLUSION_CULLING);
	p->add_check_shortcut(ED_SHORTCUT("spatial_editor/view_level_of_detail", TTR("View Level of Detail")), MENU_VIEW_LEVEL_OF_DETAIL);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("spatial_editor/settings", TTR("Settings...")), MENU_VIEW_CAMERA_SETTINGS);

	p->set_item_checked(p->get_item_index(MENU_VIEW_ORIGIN), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_GRID), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_OCCLUSION_CULLING), true);
	p->set_item_checked(p->get_item_index(MENU_VIEW_LEVEL_OF_DETAIL), true);

	p->connect("id_pressed", this, "_menu_item_pressed");

	gizmos_menu = memnew(PopupMenu);
	p->add_child(gizmos_menu);
	gizmos_menu->set_name("GizmosMenu");
	gizmos_menu->set_hide_on_checkable_item_selection(false);
	gizmos_menu->connect("id_pressed", this, "_menu_gizmo_toggled");

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
	viewport_base = memnew(SpatialEditorViewportContainer);
	shader_split->add_child(viewport_base);
	viewport_base->set_v_size_flags(SIZE_EXPAND_FILL);
	for (uint32_t i = 0; i < VIEWPORTS_COUNT; i++) {
		viewports[i] = memnew(SpatialEditorViewport(this, editor, i));
		viewports[i]->connect("toggle_maximize_view", this, "_toggle_maximize_view");
		viewports[i]->connect("clicked", this, "_update_camera_override_viewport");
		viewports[i]->assign_pending_data_pointers(preview_node, &preview_bounds, accept);
		viewport_base->add_child(viewports[i]);
	}

	/* SNAP DIALOG */

	snap_translate_value = 1;
	snap_rotate_value = 15;
	snap_scale_value = 10;

	snap_dialog = memnew(ConfirmationDialog);
	snap_dialog->set_title(TTR("Snap Settings"));
	add_child(snap_dialog);
	snap_dialog->connect("confirmed", this, "_snap_changed");
	snap_dialog->get_cancel()->connect("pressed", this, "_snap_update");

	VBoxContainer *snap_dialog_vbc = memnew(VBoxContainer);
	snap_dialog->add_child(snap_dialog_vbc);

	snap_translate = memnew(LineEdit);
	snap_dialog_vbc->add_margin_child(TTR("Translate Snap:"), snap_translate);

	snap_rotate = memnew(LineEdit);
	snap_dialog_vbc->add_margin_child(TTR("Rotate Snap (deg.):"), snap_rotate);

	snap_scale = memnew(LineEdit);
	snap_dialog_vbc->add_margin_child(TTR("Scale Snap (%):"), snap_scale);

	_snap_update();

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
	settings_fov->set_step(0.01);
	settings_fov->set_value(EDITOR_GET("editors/3d/default_fov"));
	settings_vbc->add_margin_child(TTR("Perspective FOV (deg.):"), settings_fov);

	settings_znear = memnew(SpinBox);
	settings_znear->set_max(MAX_Z);
	settings_znear->set_min(MIN_Z);
	settings_znear->set_step(0.01);
	settings_znear->set_value(EDITOR_GET("editors/3d/default_z_near"));
	settings_vbc->add_margin_child(TTR("View Z-Near:"), settings_znear);

	settings_zfar = memnew(SpinBox);
	settings_zfar->set_max(MAX_Z);
	settings_zfar->set_min(MIN_Z);
	settings_zfar->set_step(0.01);
	settings_zfar->set_value(EDITOR_GET("editors/3d/default_z_far"));
	settings_vbc->add_margin_child(TTR("View Z-Far:"), settings_zfar);

	for (uint32_t i = 0; i < VIEWPORTS_COUNT; ++i) {
		settings_dialog->connect("confirmed", viewports[i], "_view_settings_confirmed", varray(0.0));
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

	xform_dialog->connect("confirmed", this, "_xform_dialog_action");

	scenario_debug = VisualServer::SCENARIO_DEBUG_DISABLED;

	selected = nullptr;

	set_process_unhandled_key_input(true);
	add_to_group("_spatial_editor_group");

	EDITOR_DEF("editors/3d/manipulator_gizmo_size", 80);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "editors/3d/manipulator_gizmo_size", PROPERTY_HINT_RANGE, "16,160,1"));
	EDITOR_DEF("editors/3d/manipulator_gizmo_opacity", 0.9);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::REAL, "editors/3d/manipulator_gizmo_opacity", PROPERTY_HINT_RANGE, "0,1,0.01"));
	EDITOR_DEF("editors/3d/navigation/show_viewport_rotation_gizmo", true);
	EDITOR_DEF("editors/3d/navigation/show_viewport_navigation_gizmo", OS::get_singleton()->has_touchscreen_ui_hint());

	over_gizmo_handle = -1;

	// make sure the portal tools are off by default
	// (when no RoomManager is present)
	update_portal_tools();
}

SpatialEditor::~SpatialEditor() {
	memdelete(preview_node);
}

void SpatialEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		spatial_editor->show();
		spatial_editor->set_process(true);

	} else {
		spatial_editor->hide();
		spatial_editor->set_process(false);
	}
}
void SpatialEditorPlugin::edit(Object *p_object) {
	spatial_editor->edit(Object::cast_to<Spatial>(p_object));
}

bool SpatialEditorPlugin::handles(Object *p_object) const {
	if (p_object->is_class("Spatial")) {
		return true;
	}
	if (!p_object->is_class("Resource")) {
		// This ensures that gizmos are cleared when selecting a non-Spatial node.
		const_cast<SpatialEditorPlugin *>(this)->edit((Object *)nullptr);
	}
	return false;
}

Dictionary SpatialEditorPlugin::get_state() const {
	return spatial_editor->get_state();
}

void SpatialEditorPlugin::set_state(const Dictionary &p_state) {
	spatial_editor->set_state(p_state);
}

void SpatialEditor::snap_cursor_to_plane(const Plane &p_plane) {
	//cursor.pos=p_plane.project(cursor.pos);
}

Vector3 SpatialEditor::snap_point(Vector3 p_target, Vector3 p_start) const {
	if (is_snap_enabled()) {
		p_target.x = Math::snap_scalar(0.0, get_translate_snap(), p_target.x);
		p_target.y = Math::snap_scalar(0.0, get_translate_snap(), p_target.y);
		p_target.z = Math::snap_scalar(0.0, get_translate_snap(), p_target.z);
	}
	return p_target;
}

float SpatialEditor::get_translate_snap() const {
	float snap_value;
	if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
		snap_value = snap_translate->get_text().to_double() / 10.0;
	} else {
		snap_value = snap_translate->get_text().to_double();
	}

	return snap_value;
}

float SpatialEditor::get_rotate_snap() const {
	float snap_value;
	if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
		snap_value = snap_rotate->get_text().to_double() / 3.0;
	} else {
		snap_value = snap_rotate->get_text().to_double();
	}

	return snap_value;
}

float SpatialEditor::get_scale_snap() const {
	float snap_value;
	if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
		snap_value = snap_scale->get_text().to_double() / 2.0;
	} else {
		snap_value = snap_scale->get_text().to_double();
	}

	return snap_value;
}

void SpatialEditorPlugin::_bind_methods() {
	ClassDB::bind_method("snap_cursor_to_plane", &SpatialEditorPlugin::snap_cursor_to_plane);
}

void SpatialEditorPlugin::snap_cursor_to_plane(const Plane &p_plane) {
	spatial_editor->snap_cursor_to_plane(p_plane);
}

struct _GizmoPluginPriorityComparator {
	bool operator()(const Ref<EditorSpatialGizmoPlugin> &p_a, const Ref<EditorSpatialGizmoPlugin> &p_b) const {
		if (p_a->get_priority() == p_b->get_priority()) {
			return p_a->get_name() < p_b->get_name();
		}
		return p_a->get_priority() > p_b->get_priority();
	}
};

struct _GizmoPluginNameComparator {
	bool operator()(const Ref<EditorSpatialGizmoPlugin> &p_a, const Ref<EditorSpatialGizmoPlugin> &p_b) const {
		return p_a->get_name() < p_b->get_name();
	}
};

void SpatialEditor::add_gizmo_plugin(Ref<EditorSpatialGizmoPlugin> p_plugin) {
	ERR_FAIL_NULL(p_plugin.ptr());

	gizmo_plugins_by_priority.push_back(p_plugin);
	gizmo_plugins_by_priority.sort_custom<_GizmoPluginPriorityComparator>();

	gizmo_plugins_by_name.push_back(p_plugin);
	gizmo_plugins_by_name.sort_custom<_GizmoPluginNameComparator>();

	_update_gizmos_menu();
	SpatialEditor::get_singleton()->update_all_gizmos();
}

void SpatialEditor::remove_gizmo_plugin(Ref<EditorSpatialGizmoPlugin> p_plugin) {
	gizmo_plugins_by_priority.erase(p_plugin);
	gizmo_plugins_by_name.erase(p_plugin);
	_update_gizmos_menu();
}

SpatialEditorPlugin::SpatialEditorPlugin(EditorNode *p_node) {
	editor = p_node;
	spatial_editor = memnew(SpatialEditor(p_node));
	spatial_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	editor->get_viewport()->add_child(spatial_editor);

	spatial_editor->hide();
	spatial_editor->connect("transform_key_request", editor->get_inspector_dock(), "_transform_keyed");
}

SpatialEditorPlugin::~SpatialEditorPlugin() {
}

void EditorSpatialGizmoPlugin::create_material(const String &p_name, const Color &p_color, bool p_billboard, bool p_on_top, bool p_use_vertex_color) {
	Color instanced_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/instanced");

	Vector<Ref<Material3D>> mats;

	for (int i = 0; i < 4; i++) {
		bool selected = i % 2 == 1;
		bool instanced = i < 2;

		Ref<Material3D> material = Ref<Material3D>(memnew(SpatialMaterial));

		Color color = instanced ? instanced_color : p_color;

		if (!selected) {
			color.a *= 0.3;
		}

		material->set_albedo(color);
		material->set_flag(Material3D::FLAG_UNSHADED, true);
		material->set_feature(Material3D::FEATURE_TRANSPARENT, true);
		material->set_render_priority(Material3D::RENDER_PRIORITY_MIN + 1);

		if (p_use_vertex_color) {
			material->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			material->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
		}

		if (p_billboard) {
			material->set_billboard_mode(Material3D::BILLBOARD_ENABLED);
		}

		if (p_on_top && selected) {
			material->set_on_top_of_alpha();
		}

		mats.push_back(material);
	}

	materials[p_name] = mats;
}

void EditorSpatialGizmoPlugin::create_icon_material(const String &p_name, const Ref<Texture> &p_texture, bool p_on_top, const Color &p_albedo) {
	Color instanced_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/instanced");

	Vector<Ref<Material3D>> icons;

	for (int i = 0; i < 4; i++) {
		bool selected = i % 2 == 1;
		bool instanced = i < 2;

		Ref<Material3D> icon = Ref<Material3D>(memnew(SpatialMaterial));

		Color color = instanced ? instanced_color : p_albedo;

		if (!selected) {
			color.a *= 0.85;
		}

		icon->set_albedo(color);

		icon->set_flag(Material3D::FLAG_UNSHADED, true);
		icon->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		icon->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
		icon->set_cull_mode(Material3D::CULL_DISABLED);
		icon->set_depth_draw_mode(Material3D::DEPTH_DRAW_DISABLED);
		icon->set_feature(Material3D::FEATURE_TRANSPARENT, true);
		icon->set_texture(Material3D::TEXTURE_ALBEDO, p_texture);
		icon->set_flag(Material3D::FLAG_FIXED_SIZE, true);
		icon->set_billboard_mode(Material3D::BILLBOARD_ENABLED);
		icon->set_render_priority(Material3D::RENDER_PRIORITY_MIN);

		if (p_on_top && selected) {
			icon->set_on_top_of_alpha();
		}

		icons.push_back(icon);
	}

	materials[p_name] = icons;
}

void EditorSpatialGizmoPlugin::create_handle_material(const String &p_name, bool p_billboard, const Ref<Texture> &p_icon) {
	Ref<Material3D> handle_material = Ref<Material3D>(memnew(SpatialMaterial));

	handle_material->set_flag(Material3D::FLAG_UNSHADED, true);
	handle_material->set_flag(Material3D::FLAG_USE_POINT_SIZE, true);

	Ref<Texture> handle_t = p_icon != nullptr ? p_icon : SpatialEditor::get_singleton()->get_icon("Editor3DHandle", "EditorIcons");
	handle_material->set_point_size(handle_t->get_width());
	handle_material->set_texture(Material3D::TEXTURE_ALBEDO, handle_t);
	handle_material->set_albedo(Color(1, 1, 1));
	handle_material->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	handle_material->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	handle_material->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	handle_material->set_on_top_of_alpha();
	if (p_billboard) {
		handle_material->set_billboard_mode(Material3D::BILLBOARD_ENABLED);
		handle_material->set_on_top_of_alpha();
	}

	materials[p_name] = Vector<Ref<Material3D>>();
	materials[p_name].push_back(handle_material);
}

void EditorSpatialGizmoPlugin::add_material(const String &p_name, Ref<Material3D> p_material) {
	materials[p_name] = Vector<Ref<Material3D>>();
	materials[p_name].push_back(p_material);
}

Ref<Material3D> EditorSpatialGizmoPlugin::get_material(const String &p_name, const Ref<EditorSpatialGizmo> &p_gizmo) {
	ERR_FAIL_COND_V(!materials.has(p_name), Ref<Material3D>());
	ERR_FAIL_COND_V(materials[p_name].size() == 0, Ref<Material3D>());

	if (p_gizmo.is_null() || materials[p_name].size() == 1) {
		return materials[p_name][0];
	}

	int index = (p_gizmo->is_selected() ? 1 : 0) + (p_gizmo->is_editable() ? 2 : 0);

	Ref<Material3D> mat = materials[p_name][index];

	if (current_state == ON_TOP && p_gizmo->is_selected()) {
		mat->set_flag(Material3D::FLAG_DISABLE_DEPTH_TEST, true);
	} else {
		mat->set_flag(Material3D::FLAG_DISABLE_DEPTH_TEST, false);
	}

	return mat;
}

String EditorSpatialGizmoPlugin::get_name() const {
	if (get_script_instance() && get_script_instance()->has_method("get_name")) {
		return get_script_instance()->call("get_name");
	}
	WARN_PRINT_ONCE("A 3D editor gizmo has no name defined (it will appear as \"Unnamed Gizmo\" in the \"View > Gizmos\" menu). To resolve this, override the `get_name()` function to return a String in the script that extends EditorSpatialGizmoPlugin.");
	return TTR("Unnamed Gizmo");
}

int EditorSpatialGizmoPlugin::get_priority() const {
	if (get_script_instance() && get_script_instance()->has_method("get_priority")) {
		return get_script_instance()->call("get_priority");
	}
	return 0;
}

Ref<EditorSpatialGizmo> EditorSpatialGizmoPlugin::get_gizmo(Spatial *p_spatial) {
	if (get_script_instance() && get_script_instance()->has_method("get_gizmo")) {
		return get_script_instance()->call("get_gizmo", p_spatial);
	}

	Ref<EditorSpatialGizmo> ref = create_gizmo(p_spatial);

	if (ref.is_null()) {
		return ref;
	}

	ref->set_plugin(this);
	ref->set_spatial_node(p_spatial);
	ref->set_hidden(current_state == HIDDEN);

	current_gizmos.push_back(ref.ptr());
	return ref;
}

void EditorSpatialGizmoPlugin::_bind_methods() {
#define GIZMO_REF PropertyInfo(Variant::OBJECT, "gizmo", PROPERTY_HINT_RESOURCE_TYPE, "EditorSpatialGizmo")

	BIND_VMETHOD(MethodInfo(Variant::BOOL, "has_gizmo", PropertyInfo(Variant::OBJECT, "spatial", PROPERTY_HINT_RESOURCE_TYPE, "Spatial")));
	BIND_VMETHOD(MethodInfo(GIZMO_REF, "create_gizmo", PropertyInfo(Variant::OBJECT, "spatial", PROPERTY_HINT_RESOURCE_TYPE, "Spatial")));

	ClassDB::bind_method(D_METHOD("create_material", "name", "color", "billboard", "on_top", "use_vertex_color"), &EditorSpatialGizmoPlugin::create_material, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("create_icon_material", "name", "texture", "on_top", "color"), &EditorSpatialGizmoPlugin::create_icon_material, DEFVAL(false), DEFVAL(Color(1, 1, 1, 1)));
	ClassDB::bind_method(D_METHOD("create_handle_material", "name", "billboard", "texture"), &EditorSpatialGizmoPlugin::create_handle_material, DEFVAL(false), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("add_material", "name", "material"), &EditorSpatialGizmoPlugin::add_material);

	ClassDB::bind_method(D_METHOD("get_material", "name", "gizmo"), &EditorSpatialGizmoPlugin::get_material, DEFVAL(Ref<EditorSpatialGizmo>()));

	BIND_VMETHOD(MethodInfo(Variant::STRING, "get_name"));
	BIND_VMETHOD(MethodInfo(Variant::INT, "get_priority"));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "can_be_hidden"));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "is_selectable_when_hidden"));

	BIND_VMETHOD(MethodInfo("redraw", GIZMO_REF));
	BIND_VMETHOD(MethodInfo(Variant::STRING, "get_handle_name", GIZMO_REF, PropertyInfo(Variant::INT, "index")));

	MethodInfo hvget(Variant::NIL, "get_handle_value", GIZMO_REF, PropertyInfo(Variant::INT, "index"));
	hvget.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
	BIND_VMETHOD(hvget);

	BIND_VMETHOD(MethodInfo("set_handle", GIZMO_REF, PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::OBJECT, "camera", PROPERTY_HINT_RESOURCE_TYPE, "Camera"), PropertyInfo(Variant::VECTOR2, "point")));
	MethodInfo cm = MethodInfo("commit_handle", GIZMO_REF, PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::NIL, "restore"), PropertyInfo(Variant::BOOL, "cancel"));
	cm.default_arguments.push_back(false);
	BIND_VMETHOD(cm);

	BIND_VMETHOD(MethodInfo(Variant::BOOL, "is_handle_highlighted", GIZMO_REF, PropertyInfo(Variant::INT, "index")));

#undef GIZMO_REF
}

bool EditorSpatialGizmoPlugin::has_gizmo(Spatial *p_spatial) {
	if (get_script_instance() && get_script_instance()->has_method("has_gizmo")) {
		return get_script_instance()->call("has_gizmo", p_spatial);
	}
	return false;
}

Ref<EditorSpatialGizmo> EditorSpatialGizmoPlugin::create_gizmo(Spatial *p_spatial) {
	if (get_script_instance() && get_script_instance()->has_method("create_gizmo")) {
		return get_script_instance()->call("create_gizmo", p_spatial);
	}

	Ref<EditorSpatialGizmo> ref;
	if (has_gizmo(p_spatial)) {
		ref.instance();
	}
	return ref;
}

bool EditorSpatialGizmoPlugin::can_be_hidden() const {
	if (get_script_instance() && get_script_instance()->has_method("can_be_hidden")) {
		return get_script_instance()->call("can_be_hidden");
	}
	return true;
}

bool EditorSpatialGizmoPlugin::is_selectable_when_hidden() const {
	if (get_script_instance() && get_script_instance()->has_method("is_selectable_when_hidden")) {
		return get_script_instance()->call("is_selectable_when_hidden");
	}
	return false;
}

void EditorSpatialGizmoPlugin::redraw(EditorSpatialGizmo *p_gizmo) {
	if (get_script_instance() && get_script_instance()->has_method("redraw")) {
		Ref<EditorSpatialGizmo> ref(p_gizmo);
		get_script_instance()->call("redraw", ref);
	}
}

String EditorSpatialGizmoPlugin::get_handle_name(const EditorSpatialGizmo *p_gizmo, int p_idx) const {
	if (get_script_instance() && get_script_instance()->has_method("get_handle_name")) {
		return get_script_instance()->call("get_handle_name", p_gizmo, p_idx);
	}
	return "";
}

Variant EditorSpatialGizmoPlugin::get_handle_value(EditorSpatialGizmo *p_gizmo, int p_idx) const {
	if (get_script_instance() && get_script_instance()->has_method("get_handle_value")) {
		return get_script_instance()->call("get_handle_value", p_gizmo, p_idx);
	}
	return Variant();
}

void EditorSpatialGizmoPlugin::set_handle(EditorSpatialGizmo *p_gizmo, int p_idx, Camera *p_camera, const Point2 &p_point) {
	if (get_script_instance() && get_script_instance()->has_method("set_handle")) {
		get_script_instance()->call("set_handle", p_gizmo, p_idx, p_camera, p_point);
	}
}

void EditorSpatialGizmoPlugin::commit_handle(EditorSpatialGizmo *p_gizmo, int p_idx, const Variant &p_restore, bool p_cancel) {
	if (get_script_instance() && get_script_instance()->has_method("commit_handle")) {
		get_script_instance()->call("commit_handle", p_gizmo, p_idx, p_restore, p_cancel);
	}
}

bool EditorSpatialGizmoPlugin::is_handle_highlighted(const EditorSpatialGizmo *p_gizmo, int p_idx) const {
	if (get_script_instance() && get_script_instance()->has_method("is_handle_highlighted")) {
		return get_script_instance()->call("is_handle_highlighted", p_gizmo, p_idx);
	}
	return false;
}

void EditorSpatialGizmoPlugin::set_state(int p_state) {
	current_state = p_state;
	for (int i = 0; i < current_gizmos.size(); ++i) {
		current_gizmos[i]->set_hidden(current_state == HIDDEN);
	}
}

int EditorSpatialGizmoPlugin::get_state() const {
	return current_state;
}

void EditorSpatialGizmoPlugin::unregister_gizmo(EditorSpatialGizmo *p_gizmo) {
	current_gizmos.erase(p_gizmo);
}

EditorSpatialGizmoPlugin::EditorSpatialGizmoPlugin() {
	current_state = VISIBLE;
}

EditorSpatialGizmoPlugin::~EditorSpatialGizmoPlugin() {
	for (int i = 0; i < current_gizmos.size(); ++i) {
		current_gizmos[i]->set_plugin(nullptr);
		current_gizmos[i]->get_spatial_node()->set_gizmo(nullptr);
	}
	if (SpatialEditor::get_singleton()) {
		SpatialEditor::get_singleton()->update_all_gizmos();
	}
}
