/**************************************************************************/
/*  camera_3d_gizmo_plugin.cpp                                            */
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

#include "camera_3d_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"

Camera3DGizmoPlugin::Camera3DGizmoPlugin() {
	Color gizmo_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/camera");

	create_material("camera_material", gizmo_color);
	create_icon_material("camera_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoCamera3D"), EditorStringName(EditorIcons)));
	create_handle_material("handles");
}

bool Camera3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<Camera3D>(p_spatial) != nullptr;
}

String Camera3DGizmoPlugin::get_gizmo_name() const {
	return "Camera3D";
}

int Camera3DGizmoPlugin::get_priority() const {
	return -1;
}

String Camera3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	Camera3D *camera = Object::cast_to<Camera3D>(p_gizmo->get_node_3d());

	if (camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE) {
		return "FOV";
	} else {
		return "Size";
	}
}

Variant Camera3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	Camera3D *camera = Object::cast_to<Camera3D>(p_gizmo->get_node_3d());

	if (camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE) {
		return camera->get_fov();
	} else {
		return camera->get_size();
	}
}

void Camera3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	Camera3D *camera = Object::cast_to<Camera3D>(p_gizmo->get_node_3d());

	Transform3D gt = camera->get_global_transform();
	Transform3D gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 s[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };

	if (camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE) {
		Transform3D gt2 = camera->get_global_transform();
		float a = _find_closest_angle_to_half_pi_arc(s[0], s[1], 1.0, gt2);
		camera->set("fov", CLAMP(a * 2.0, 1, 179));
	} else {
		Camera3D::KeepAspect aspect = camera->get_keep_aspect_mode();
		Vector3 camera_far = aspect == Camera3D::KeepAspect::KEEP_WIDTH ? Vector3(4096, 0, -1) : Vector3(0, 4096, -1);

		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(0, 0, -1), camera_far, s[0], s[1], ra, rb);
		float d = aspect == Camera3D::KeepAspect::KEEP_WIDTH ? ra.x * 2 : ra.y * 2;
		if (Node3DEditor::get_singleton()->is_snap_enabled()) {
			d = Math::snapped(d, Node3DEditor::get_singleton()->get_translate_snap());
		}

		d = CLAMP(d, 0.1, 16384);

		camera->set("size", d);
	}
}

void Camera3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	Camera3D *camera = Object::cast_to<Camera3D>(p_gizmo->get_node_3d());

	if (camera->get_projection() == Camera3D::PROJECTION_PERSPECTIVE) {
		if (p_cancel) {
			camera->set("fov", p_restore);
		} else {
			EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
			ur->create_action(TTR("Change Camera FOV"));
			ur->add_do_property(camera, "fov", camera->get_fov());
			ur->add_undo_property(camera, "fov", p_restore);
			ur->commit_action();
		}

	} else {
		if (p_cancel) {
			camera->set("size", p_restore);
		} else {
			EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
			ur->create_action(TTR("Change Camera Size"));
			ur->add_do_property(camera, "size", camera->get_size());
			ur->add_undo_property(camera, "size", p_restore);
			ur->commit_action();
		}
	}
}

void Camera3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Camera3D *camera = Object::cast_to<Camera3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Vector<Vector3> lines;
	Vector<Vector3> handles;

	Ref<Material> material = get_material("camera_material", p_gizmo);
	Ref<Material> icon = get_material("camera_icon", p_gizmo);

	const Size2i viewport_size = Node3DEditor::get_camera_viewport_size(camera);
	const real_t viewport_aspect = viewport_size.x > 0 && viewport_size.y > 0 ? viewport_size.aspect() : 1.0;
	const Size2 size_factor = viewport_aspect > 1.0 ? Size2(1.0, 1.0 / viewport_aspect) : Size2(viewport_aspect, 1.0);

#define ADD_TRIANGLE(m_a, m_b, m_c) \
	{                               \
		lines.push_back(m_a);       \
		lines.push_back(m_b);       \
		lines.push_back(m_b);       \
		lines.push_back(m_c);       \
		lines.push_back(m_c);       \
		lines.push_back(m_a);       \
	}

#define ADD_QUAD(m_a, m_b, m_c, m_d) \
	{                                \
		lines.push_back(m_a);        \
		lines.push_back(m_b);        \
		lines.push_back(m_b);        \
		lines.push_back(m_c);        \
		lines.push_back(m_c);        \
		lines.push_back(m_d);        \
		lines.push_back(m_d);        \
		lines.push_back(m_a);        \
	}

	switch (camera->get_projection()) {
		case Camera3D::PROJECTION_PERSPECTIVE: {
			// The real FOV is halved for accurate representation
			float fov = camera->get_fov() / 2.0;

			const float hsize = Math::sin(Math::deg_to_rad(fov));
			const float depth = -Math::cos(Math::deg_to_rad(fov));
			Vector3 side = Vector3(hsize * size_factor.x, 0, depth);
			Vector3 nside = Vector3(-side.x, side.y, side.z);
			Vector3 up = Vector3(0, hsize * size_factor.y, 0);

			ADD_TRIANGLE(Vector3(), side + up, side - up);
			ADD_TRIANGLE(Vector3(), nside + up, nside - up);
			ADD_TRIANGLE(Vector3(), side + up, nside + up);
			ADD_TRIANGLE(Vector3(), side - up, nside - up);

			handles.push_back(side);
			side.x = MIN(side.x, hsize * 0.25);
			nside.x = -side.x;
			Vector3 tup(0, up.y + hsize / 2, side.z);
			ADD_TRIANGLE(tup, side + up, nside + up);
		} break;

		case Camera3D::PROJECTION_ORTHOGONAL: {
			Camera3D::KeepAspect aspect = camera->get_keep_aspect_mode();

			float size = camera->get_size();
			float keep_size = size * 0.5;

			Vector3 right, up;
			Vector3 back(0, 0, -1.0);
			Vector3 front(0, 0, 0);

			if (aspect == Camera3D::KeepAspect::KEEP_WIDTH) {
				right = Vector3(keep_size, 0, 0);
				up = Vector3(0, keep_size / viewport_aspect, 0);
				handles.push_back(right + back);
			} else {
				right = Vector3(keep_size * viewport_aspect, 0, 0);
				up = Vector3(0, keep_size, 0);
				handles.push_back(up + back);
			}

			ADD_QUAD(-up - right, -up + right, up + right, up - right);
			ADD_QUAD(-up - right + back, -up + right + back, up + right + back, up - right + back);
			ADD_QUAD(up + right, up + right + back, up - right + back, up - right);
			ADD_QUAD(-up + right, -up + right + back, -up - right + back, -up - right);

			right.x = MIN(right.x, keep_size * 0.25);
			Vector3 tup(0, up.y + keep_size / 2, back.z);
			ADD_TRIANGLE(tup, right + up + back, -right + up + back);
		} break;

		case Camera3D::PROJECTION_FRUSTUM: {
			float hsize = camera->get_size() / 2.0;

			Vector3 side = Vector3(hsize, 0, -camera->get_near()).normalized();
			side.x *= size_factor.x;
			Vector3 nside = Vector3(-side.x, side.y, side.z);
			Vector3 up = Vector3(0, hsize * size_factor.y, 0);
			Vector3 offset = Vector3(camera->get_frustum_offset().x, camera->get_frustum_offset().y, 0.0);

			ADD_TRIANGLE(Vector3(), side + up + offset, side - up + offset);
			ADD_TRIANGLE(Vector3(), nside + up + offset, nside - up + offset);
			ADD_TRIANGLE(Vector3(), side + up + offset, nside + up + offset);
			ADD_TRIANGLE(Vector3(), side - up + offset, nside - up + offset);

			side.x = MIN(side.x, hsize * 0.25);
			nside.x = -side.x;
			Vector3 tup(0, up.y + hsize / 2, side.z);
			ADD_TRIANGLE(tup + offset, side + up + offset, nside + up + offset);
		} break;
	}

#undef ADD_TRIANGLE
#undef ADD_QUAD

	p_gizmo->add_lines(lines, material);
	p_gizmo->add_unscaled_billboard(icon, 0.05);
	p_gizmo->add_collision_segments(lines);

	if (!handles.is_empty()) {
		p_gizmo->add_handles(handles, get_material("handles"));
	}
}

float Camera3DGizmoPlugin::_find_closest_angle_to_half_pi_arc(const Vector3 &p_from, const Vector3 &p_to, float p_arc_radius, const Transform3D &p_arc_xform) {
	//bleh, discrete is simpler
	static const int arc_test_points = 64;
	float min_d = 1e20;
	Vector3 min_p;

	for (int i = 0; i < arc_test_points; i++) {
		float a = i * Math_PI * 0.5 / arc_test_points;
		float an = (i + 1) * Math_PI * 0.5 / arc_test_points;
		Vector3 p = Vector3(Math::cos(a), 0, -Math::sin(a)) * p_arc_radius;
		Vector3 n = Vector3(Math::cos(an), 0, -Math::sin(an)) * p_arc_radius;

		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(p, n, p_from, p_to, ra, rb);

		float d = ra.distance_to(rb);
		if (d < min_d) {
			min_d = d;
			min_p = ra;
		}
	}

	//min_p = p_arc_xform.affine_inverse().xform(min_p);
	float a = (Math_PI * 0.5) - Vector2(min_p.x, -min_p.z).angle();
	return Math::rad_to_deg(a);
}
