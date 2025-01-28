/**************************************************************************/
/*  light_3d_gizmo_plugin.cpp                                             */
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

#include "light_3d_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/light_3d.h"

Light3DGizmoPlugin::Light3DGizmoPlugin() {
	// Enable vertex colors for the materials below as the gizmo color depends on the light color.
	create_material("lines_primary", Color(1, 1, 1), false, false, true);
	create_material("lines_secondary", Color(1, 1, 1, 0.35), false, false, true);
	create_material("lines_billboard", Color(1, 1, 1), true, false, true);

	create_icon_material("light_directional_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoDirectionalLight"), EditorStringName(EditorIcons)));
	create_icon_material("light_omni_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoLight"), EditorStringName(EditorIcons)));
	create_icon_material("light_spot_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoSpotLight"), EditorStringName(EditorIcons)));

	create_handle_material("handles");
	create_handle_material("handles_billboard", true);
}

bool Light3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<Light3D>(p_spatial) != nullptr;
}

String Light3DGizmoPlugin::get_gizmo_name() const {
	return "Light3D";
}

int Light3DGizmoPlugin::get_priority() const {
	return -1;
}

String Light3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	if (p_id == 0) {
		return "Radius";
	} else {
		return "Aperture";
	}
}

Variant Light3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	Light3D *light = Object::cast_to<Light3D>(p_gizmo->get_node_3d());
	if (p_id == 0) {
		return light->get_param(Light3D::PARAM_RANGE);
	}
	if (p_id == 1) {
		return light->get_param(Light3D::PARAM_SPOT_ANGLE);
	}

	return Variant();
}

void Light3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	Light3D *light = Object::cast_to<Light3D>(p_gizmo->get_node_3d());
	Transform3D gt = light->get_global_transform();
	Transform3D gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 s[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };
	if (p_id == 0) {
		if (Object::cast_to<SpotLight3D>(light)) {
			Vector3 ra, rb;
			Geometry3D::get_closest_points_between_segments(Vector3(), Vector3(0, 0, -4096), s[0], s[1], ra, rb);

			float d = -ra.z;
			if (Node3DEditor::get_singleton()->is_snap_enabled()) {
				d = Math::snapped(d, Node3DEditor::get_singleton()->get_translate_snap());
			}

			if (d <= 0) { // Equal is here for negative zero.
				d = 0;
			}

			light->set_param(Light3D::PARAM_RANGE, d);
		} else if (Object::cast_to<OmniLight3D>(light)) {
			Plane cp = Plane(p_camera->get_transform().basis.get_column(2), gt.origin);

			Vector3 inters;
			if (cp.intersects_ray(ray_from, ray_dir, &inters)) {
				float r = inters.distance_to(gt.origin);
				if (Node3DEditor::get_singleton()->is_snap_enabled()) {
					r = Math::snapped(r, Node3DEditor::get_singleton()->get_translate_snap());
				}

				light->set_param(Light3D::PARAM_RANGE, r);
			}
		}

	} else if (p_id == 1) {
		float a = _find_closest_angle_to_half_pi_arc(s[0], s[1], light->get_param(Light3D::PARAM_RANGE), gt);
		light->set_param(Light3D::PARAM_SPOT_ANGLE, CLAMP(a, 0.01, 89.99));
	}
}

void Light3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	Light3D *light = Object::cast_to<Light3D>(p_gizmo->get_node_3d());
	if (p_cancel) {
		light->set_param(p_id == 0 ? Light3D::PARAM_RANGE : Light3D::PARAM_SPOT_ANGLE, p_restore);

	} else if (p_id == 0) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Light Radius"));
		ur->add_do_method(light, "set_param", Light3D::PARAM_RANGE, light->get_param(Light3D::PARAM_RANGE));
		ur->add_undo_method(light, "set_param", Light3D::PARAM_RANGE, p_restore);
		ur->commit_action();
	} else if (p_id == 1) {
		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Light Radius"));
		ur->add_do_method(light, "set_param", Light3D::PARAM_SPOT_ANGLE, light->get_param(Light3D::PARAM_SPOT_ANGLE));
		ur->add_undo_method(light, "set_param", Light3D::PARAM_SPOT_ANGLE, p_restore);
		ur->commit_action();
	}
}

void Light3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Light3D *light = Object::cast_to<Light3D>(p_gizmo->get_node_3d());

	Color color = light->get_color().srgb_to_linear() * light->get_correlated_color().srgb_to_linear();
	color = color.linear_to_srgb();
	// Make the gizmo color as bright as possible for better visibility
	color.set_hsv(color.get_h(), color.get_s(), 1);

	p_gizmo->clear();

	if (Object::cast_to<DirectionalLight3D>(light)) {
		if (p_gizmo->is_selected()) {
			Ref<Material> material = get_material("lines_primary", p_gizmo);

			const int arrow_points = 7;
			const float arrow_length = 1.5;

			Vector3 arrow[arrow_points] = {
				Vector3(0, 0, -1),
				Vector3(0, 0.8, 0),
				Vector3(0, 0.3, 0),
				Vector3(0, 0.3, arrow_length),
				Vector3(0, -0.3, arrow_length),
				Vector3(0, -0.3, 0),
				Vector3(0, -0.8, 0)
			};

			int arrow_sides = 2;

			Vector<Vector3> lines;

			for (int i = 0; i < arrow_sides; i++) {
				for (int j = 0; j < arrow_points; j++) {
					Basis ma(Vector3(0, 0, 1), Math_PI * i / arrow_sides);

					Vector3 v1 = arrow[j] - Vector3(0, 0, arrow_length);
					Vector3 v2 = arrow[(j + 1) % arrow_points] - Vector3(0, 0, arrow_length);

					lines.push_back(ma.xform(v1));
					lines.push_back(ma.xform(v2));
				}
			}

			p_gizmo->add_lines(lines, material, false, color);
		}

		Ref<Material> icon = get_material("light_directional_icon", p_gizmo);
		p_gizmo->add_unscaled_billboard(icon, 0.05, color);
	}

	if (Object::cast_to<OmniLight3D>(light)) {
		if (p_gizmo->is_selected()) {
			// Use both a billboard circle and 3 non-billboard circles for a better sphere-like representation
			const Ref<Material> lines_material = get_material("lines_secondary", p_gizmo);
			const Ref<Material> lines_billboard_material = get_material("lines_billboard", p_gizmo);

			OmniLight3D *on = Object::cast_to<OmniLight3D>(light);
			const float r = on->get_param(Light3D::PARAM_RANGE);
			Vector<Vector3> points;
			Vector<Vector3> points_billboard;

			for (int i = 0; i < 120; i++) {
				// Create a circle
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float rb = Math::deg_to_rad((float)((i + 1) * 3));
				const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
				const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

				// Draw axis-aligned circles
				points.push_back(Vector3(a.x, 0, a.y));
				points.push_back(Vector3(b.x, 0, b.y));
				points.push_back(Vector3(0, a.x, a.y));
				points.push_back(Vector3(0, b.x, b.y));
				points.push_back(Vector3(a.x, a.y, 0));
				points.push_back(Vector3(b.x, b.y, 0));

				// Draw a billboarded circle
				points_billboard.push_back(Vector3(a.x, a.y, 0));
				points_billboard.push_back(Vector3(b.x, b.y, 0));
			}

			p_gizmo->add_lines(points, lines_material, true, color);
			p_gizmo->add_lines(points_billboard, lines_billboard_material, true, color);

			Vector<Vector3> handles;
			handles.push_back(Vector3(r, 0, 0));
			p_gizmo->add_handles(handles, get_material("handles_billboard"), Vector<int>(), true);
		}

		const Ref<Material> icon = get_material("light_omni_icon", p_gizmo);
		p_gizmo->add_unscaled_billboard(icon, 0.05, color);
	}

	if (Object::cast_to<SpotLight3D>(light)) {
		if (p_gizmo->is_selected()) {
			const Ref<Material> material_primary = get_material("lines_primary", p_gizmo);
			const Ref<Material> material_secondary = get_material("lines_secondary", p_gizmo);

			Vector<Vector3> points_primary;
			Vector<Vector3> points_secondary;
			SpotLight3D *sl = Object::cast_to<SpotLight3D>(light);

			float r = sl->get_param(Light3D::PARAM_RANGE);
			float w = r * Math::sin(Math::deg_to_rad(sl->get_param(Light3D::PARAM_SPOT_ANGLE)));
			float d = r * Math::cos(Math::deg_to_rad(sl->get_param(Light3D::PARAM_SPOT_ANGLE)));

			for (int i = 0; i < 120; i++) {
				// Draw a circle
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float rb = Math::deg_to_rad((float)((i + 1) * 3));
				const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * w;
				const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * w;

				points_primary.push_back(Vector3(a.x, a.y, -d));
				points_primary.push_back(Vector3(b.x, b.y, -d));

				if (i % 15 == 0) {
					// Draw 8 lines from the cone origin to the sides of the circle
					points_secondary.push_back(Vector3(a.x, a.y, -d));
					points_secondary.push_back(Vector3());
				}
			}

			points_primary.push_back(Vector3(0, 0, -r));
			points_primary.push_back(Vector3());

			p_gizmo->add_lines(points_primary, material_primary, false, color);
			p_gizmo->add_lines(points_secondary, material_secondary, false, color);

			Vector<Vector3> handles = {
				Vector3(0, 0, -r),
				Vector3(w, 0, -d)
			};

			p_gizmo->add_handles(handles, get_material("handles"));
		}

		const Ref<Material> icon = get_material("light_spot_icon", p_gizmo);
		p_gizmo->add_unscaled_billboard(icon, 0.05, color);
	}
}

float Light3DGizmoPlugin::_find_closest_angle_to_half_pi_arc(const Vector3 &p_from, const Vector3 &p_to, float p_arc_radius, const Transform3D &p_arc_xform) {
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
