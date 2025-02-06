/**************************************************************************/
/*  navigation_link_3d_gizmo_plugin.cpp                                   */
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

#include "navigation_link_3d_gizmo_plugin.h"

#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/navigation_link_3d.h"
#include "servers/navigation_server_3d.h"

NavigationLink3DGizmoPlugin::NavigationLink3DGizmoPlugin() {
	create_material("navigation_link_material", NavigationServer3D::get_singleton()->get_debug_navigation_link_connection_color());
	create_material("navigation_link_material_disabled", NavigationServer3D::get_singleton()->get_debug_navigation_link_connection_disabled_color());
	create_handle_material("handles");
}

bool NavigationLink3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<NavigationLink3D>(p_spatial) != nullptr;
}

String NavigationLink3DGizmoPlugin::get_gizmo_name() const {
	return "NavigationLink3D";
}

int NavigationLink3DGizmoPlugin::get_priority() const {
	return -1;
}

void NavigationLink3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	NavigationLink3D *link = Object::cast_to<NavigationLink3D>(p_gizmo->get_node_3d());

	RID nav_map = link->get_world_3d()->get_navigation_map();
	real_t search_radius = NavigationServer3D::get_singleton()->map_get_link_connection_radius(nav_map);
	Vector3 up_vector = NavigationServer3D::get_singleton()->map_get_up(nav_map);
	Vector3::Axis up_axis = up_vector.max_axis_index();

	Vector3 start_position = link->get_start_position();
	Vector3 end_position = link->get_end_position();

	Ref<Material> link_material = get_material("navigation_link_material", p_gizmo);
	Ref<Material> link_material_disabled = get_material("navigation_link_material_disabled", p_gizmo);
	Ref<Material> handles_material = get_material("handles");

	p_gizmo->clear();

	// Number of points in an octant. So there will be 8 * points_in_octant points in total.
	// Correspond to the smoothness of the circle.
	const uint32_t points_in_octant = 8;
	real_t inc = (Math_PI / (4 * points_in_octant));

	Vector<Vector3> lines;
	// points_in_octant * 8 * 2 per circle * 2 circles. 2 for the start-end. 4 for the arrow, and another 4 if bidirectionnal.
	lines.resize(points_in_octant * 8 * 2 * 2 + 2 + 4 + (link->is_bidirectional() ? 4 : 0));
	uint32_t index = 0;
	Vector3 *lines_ptrw = lines.ptrw();
	// Draw line between the points.
	lines_ptrw[index++] = start_position;
	lines_ptrw[index++] = end_position;
	real_t search_radius_squared = search_radius * search_radius;

	// Draw circles at start and end positions in one go.
	real_t r = 0;
	Vector2 a = Vector2(search_radius, 0);
	for (uint32_t i = 0; i < points_in_octant; i++) {
		r += inc;
		real_t x = Math::cos(r) * search_radius;
		real_t y = Math::sqrt(search_radius_squared - (x * x));

		// Draw axis-aligned circle.
		switch (up_axis) {
			case Vector3::AXIS_X:
#define PUSH_OCTANT(_position, a, b)                          \
	lines_ptrw[index++] = _position + Vector3(0, a.x, a.y);   \
	lines_ptrw[index++] = _position + Vector3(0, x, y);       \
	lines_ptrw[index++] = _position + Vector3(0, -a.x, a.y);  \
	lines_ptrw[index++] = _position + Vector3(0, x, y);       \
	lines_ptrw[index++] = _position + Vector3(0, a.x, -a.y);  \
	lines_ptrw[index++] = _position + Vector3(0, x, -y);      \
	lines_ptrw[index++] = _position + Vector3(0, -a.x, -a.y); \
	lines_ptrw[index++] = _position + Vector3(0, x, y);       \
	lines_ptrw[index++] = _position + Vector3(0, a.y, a.x);   \
	lines_ptrw[index++] = _position + Vector3(0, y, x);       \
	lines_ptrw[index++] = _position + Vector3(0, -a.y, a.x);  \
	lines_ptrw[index++] = _position + Vector3(0, -y, x);      \
	lines_ptrw[index++] = _position + Vector3(0, a.y, -a.x);  \
	lines_ptrw[index++] = _position + Vector3(0, y, -x);      \
	lines_ptrw[index++] = _position + Vector3(0, -a.y, -a.x); \
	lines_ptrw[index++] = _position + Vector3(0, -y, -x);

				PUSH_OCTANT(start_position, a, b)
				PUSH_OCTANT(end_position, a, b)
#undef PUSH_OCTANT
				break;
			case Vector3::AXIS_Y:
#define PUSH_OCTANT(_position, a, b)                          \
	lines_ptrw[index++] = _position + Vector3(a.x, 0, a.y);   \
	lines_ptrw[index++] = _position + Vector3(x, 0, y);       \
	lines_ptrw[index++] = _position + Vector3(-a.x, 0, a.y);  \
	lines_ptrw[index++] = _position + Vector3(-x, 0, y);      \
	lines_ptrw[index++] = _position + Vector3(a.x, 0, -a.y);  \
	lines_ptrw[index++] = _position + Vector3(x, 0, -y);      \
	lines_ptrw[index++] = _position + Vector3(-a.x, 0, -a.y); \
	lines_ptrw[index++] = _position + Vector3(-x, 0, -y);     \
	lines_ptrw[index++] = _position + Vector3(a.y, 0, a.x);   \
	lines_ptrw[index++] = _position + Vector3(y, 0, x);       \
	lines_ptrw[index++] = _position + Vector3(-a.y, 0, a.x);  \
	lines_ptrw[index++] = _position + Vector3(-y, 0, x);      \
	lines_ptrw[index++] = _position + Vector3(a.y, 0, -a.x);  \
	lines_ptrw[index++] = _position + Vector3(y, 0, -x);      \
	lines_ptrw[index++] = _position + Vector3(-a.y, 0, -a.x); \
	lines_ptrw[index++] = _position + Vector3(-y, 0, -x);

				PUSH_OCTANT(start_position, a, b)
				PUSH_OCTANT(end_position, a, b)
#undef PUSH_OCTANT
				break;
			case Vector3::AXIS_Z:
#define PUSH_OCTANT(_position, a, b)                          \
	lines_ptrw[index++] = _position + Vector3(a.x, a.y, 0);   \
	lines_ptrw[index++] = _position + Vector3(x, y, 0);       \
	lines_ptrw[index++] = _position + Vector3(-a.x, a.y, 0);  \
	lines_ptrw[index++] = _position + Vector3(-x, y, 0);      \
	lines_ptrw[index++] = _position + Vector3(a.x, -a.y, 0);  \
	lines_ptrw[index++] = _position + Vector3(x, -y, 0);      \
	lines_ptrw[index++] = _position + Vector3(-a.x, -a.y, 0); \
	lines_ptrw[index++] = _position + Vector3(-x, -y, 0);     \
	lines_ptrw[index++] = _position + Vector3(a.y, a.x, 0);   \
	lines_ptrw[index++] = _position + Vector3(y, x, 0);       \
	lines_ptrw[index++] = _position + Vector3(-a.y, a.x, 0);  \
	lines_ptrw[index++] = _position + Vector3(-y, x, 0);      \
	lines_ptrw[index++] = _position + Vector3(a.y, -a.x, 0);  \
	lines_ptrw[index++] = _position + Vector3(y, -x, 0);      \
	lines_ptrw[index++] = _position + Vector3(-a.y, -a.x, 0); \
	lines_ptrw[index++] = _position + Vector3(-y, -x, 0);

				PUSH_OCTANT(start_position, a, b)
				PUSH_OCTANT(end_position, a, b)
#undef PUSH_OCTANT
				break;
		}
		a.x = x;
		a.y = y;
	}

	const Vector3 link_segment = end_position - start_position;
	const Vector3 up = Vector3(0.0, 1.0, 0.0);
	const float arror_len = 0.5;

	{
		Vector3 anchor = start_position + (link_segment * 0.75);
		Vector3 direction = start_position.direction_to(end_position);
		Vector3 arrow_dir = direction.cross(up);
		lines_ptrw[index++] = anchor;
		lines_ptrw[index++] = anchor + (arrow_dir - direction) * arror_len;

		arrow_dir = -direction.cross(up);
		lines_ptrw[index++] = anchor;
		lines_ptrw[index++] = anchor + (arrow_dir - direction) * arror_len;
	}

	if (link->is_bidirectional()) {
		Vector3 anchor = start_position + (link_segment * 0.25);
		Vector3 direction = end_position.direction_to(start_position);
		Vector3 arrow_dir = direction.cross(up);
		lines_ptrw[index++] = anchor;
		lines_ptrw[index++] = anchor + (arrow_dir - direction) * arror_len;

		arrow_dir = -direction.cross(up);
		lines_ptrw[index++] = anchor;
		lines_ptrw[index++] = anchor + (arrow_dir - direction) * arror_len;
	}

	p_gizmo->add_lines(lines, link->is_enabled() ? link_material : link_material_disabled);
	p_gizmo->add_collision_segments(lines);

	p_gizmo->add_handles(Vector({ start_position, end_position }), handles_material);
}

String NavigationLink3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return p_id == 0 ? TTR("Start Location") : TTR("End Location");
}

Variant NavigationLink3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	NavigationLink3D *link = Object::cast_to<NavigationLink3D>(p_gizmo->get_node_3d());
	return p_id == 0 ? link->get_start_position() : link->get_end_position();
}

void NavigationLink3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	NavigationLink3D *link = Object::cast_to<NavigationLink3D>(p_gizmo->get_node_3d());

	Transform3D gt = link->get_global_transform();
	Transform3D gi = gt.affine_inverse();

	Transform3D ct = p_camera->get_global_transform();
	Vector3 cam_dir = ct.basis.get_column(Vector3::AXIS_Z);

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 position = p_id == 0 ? link->get_start_position() : link->get_end_position();
	Plane move_plane = Plane(cam_dir, gt.xform(position));

	Vector3 intersection;
	if (!move_plane.intersects_ray(ray_from, ray_dir, &intersection)) {
		return;
	}

	if (Node3DEditor::get_singleton()->is_snap_enabled()) {
		double snap = Node3DEditor::get_singleton()->get_translate_snap();
		intersection.snapf(snap);
	}

	position = gi.xform(intersection);
	if (p_id == 0) {
		link->set_start_position(position);
	} else if (p_id == 1) {
		link->set_end_position(position);
	}
}

void NavigationLink3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	NavigationLink3D *link = Object::cast_to<NavigationLink3D>(p_gizmo->get_node_3d());

	if (p_cancel) {
		if (p_id == 0) {
			link->set_start_position(p_restore);
		} else {
			link->set_end_position(p_restore);
		}
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	if (p_id == 0) {
		ur->create_action(TTR("Change Start Position"));
		ur->add_do_method(link, "set_start_position", link->get_start_position());
		ur->add_undo_method(link, "set_start_position", p_restore);
	} else {
		ur->create_action(TTR("Change End Position"));
		ur->add_do_method(link, "set_end_position", link->get_end_position());
		ur->add_undo_method(link, "set_end_position", p_restore);
	}

	ur->commit_action();
}
