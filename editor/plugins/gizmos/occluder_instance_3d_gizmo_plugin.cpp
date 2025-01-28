/**************************************************************************/
/*  occluder_instance_3d_gizmo_plugin.cpp                                 */
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

#include "occluder_instance_3d_gizmo_plugin.h"

#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/occluder_instance_3d.h"

OccluderInstance3DGizmoPlugin::OccluderInstance3DGizmoPlugin() {
	create_material("line_material", EDITOR_GET("editors/3d_gizmos/gizmo_colors/occluder"));
	create_handle_material("handles");
}

bool OccluderInstance3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<OccluderInstance3D>(p_spatial) != nullptr;
}

String OccluderInstance3DGizmoPlugin::get_gizmo_name() const {
	return "OccluderInstance3D";
}

int OccluderInstance3DGizmoPlugin::get_priority() const {
	return -1;
}

String OccluderInstance3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	const OccluderInstance3D *cs = Object::cast_to<OccluderInstance3D>(p_gizmo->get_node_3d());

	Ref<Occluder3D> o = cs->get_occluder();
	if (o.is_null()) {
		return "";
	}

	if (Object::cast_to<SphereOccluder3D>(*o)) {
		return "Radius";
	}

	if (Object::cast_to<BoxOccluder3D>(*o) || Object::cast_to<QuadOccluder3D>(*o)) {
		return "Size";
	}

	return "";
}

Variant OccluderInstance3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	OccluderInstance3D *oi = Object::cast_to<OccluderInstance3D>(p_gizmo->get_node_3d());

	Ref<Occluder3D> o = oi->get_occluder();
	if (o.is_null()) {
		return Variant();
	}

	if (Object::cast_to<SphereOccluder3D>(*o)) {
		Ref<SphereOccluder3D> so = o;
		return so->get_radius();
	}

	if (Object::cast_to<BoxOccluder3D>(*o)) {
		Ref<BoxOccluder3D> bo = o;
		return bo->get_size();
	}

	if (Object::cast_to<QuadOccluder3D>(*o)) {
		Ref<QuadOccluder3D> qo = o;
		return qo->get_size();
	}

	return Variant();
}

void OccluderInstance3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	OccluderInstance3D *oi = Object::cast_to<OccluderInstance3D>(p_gizmo->get_node_3d());

	Ref<Occluder3D> o = oi->get_occluder();
	if (o.is_null()) {
		return;
	}

	Transform3D gt = oi->get_global_transform();
	Transform3D gi = gt.affine_inverse();

	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	Vector3 sg[2] = { gi.xform(ray_from), gi.xform(ray_from + ray_dir * 4096) };

	bool snap_enabled = Node3DEditor::get_singleton()->is_snap_enabled();
	float snap = Node3DEditor::get_singleton()->get_translate_snap();

	if (Object::cast_to<SphereOccluder3D>(*o)) {
		Ref<SphereOccluder3D> so = o;
		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(), Vector3(4096, 0, 0), sg[0], sg[1], ra, rb);
		float d = ra.x;
		if (snap_enabled) {
			d = Math::snapped(d, snap);
		}

		if (d < 0.001) {
			d = 0.001;
		}

		so->set_radius(d);
	}

	if (Object::cast_to<BoxOccluder3D>(*o)) {
		Vector3 axis;
		axis[p_id] = 1.0;
		Ref<BoxOccluder3D> bo = o;
		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(), axis * 4096, sg[0], sg[1], ra, rb);
		float d = ra[p_id] * 2;
		if (snap_enabled) {
			d = Math::snapped(d, snap);
		}

		if (d < 0.001) {
			d = 0.001;
		}

		Vector3 he = bo->get_size();
		he[p_id] = d;
		bo->set_size(he);
	}

	if (Object::cast_to<QuadOccluder3D>(*o)) {
		Ref<QuadOccluder3D> qo = o;
		Plane p = Plane(Vector3(0.0f, 0.0f, 1.0f), 0.0f);
		Vector3 intersection;
		if (!p.intersects_segment(sg[0], sg[1], &intersection)) {
			return;
		}

		if (p_id == 2) {
			Vector2 s = Vector2(intersection.x, intersection.y) * 2.0f;
			if (snap_enabled) {
				s = s.snappedf(snap);
			}
			s = s.maxf(0.001);
			qo->set_size(s);
		} else {
			float d = intersection[p_id];
			if (snap_enabled) {
				d = Math::snapped(d, snap);
			}

			if (d < 0.001) {
				d = 0.001;
			}

			Vector2 he = qo->get_size();
			he[p_id] = d * 2.0f;
			qo->set_size(he);
		}
	}
}

void OccluderInstance3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	OccluderInstance3D *oi = Object::cast_to<OccluderInstance3D>(p_gizmo->get_node_3d());

	Ref<Occluder3D> o = oi->get_occluder();
	if (o.is_null()) {
		return;
	}

	if (Object::cast_to<SphereOccluder3D>(*o)) {
		Ref<SphereOccluder3D> so = o;
		if (p_cancel) {
			so->set_radius(p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Sphere Shape Radius"));
		ur->add_do_method(so.ptr(), "set_radius", so->get_radius());
		ur->add_undo_method(so.ptr(), "set_radius", p_restore);
		ur->commit_action();
	}

	if (Object::cast_to<BoxOccluder3D>(*o)) {
		Ref<BoxOccluder3D> bo = o;
		if (p_cancel) {
			bo->set_size(p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Box Shape Size"));
		ur->add_do_method(bo.ptr(), "set_size", bo->get_size());
		ur->add_undo_method(bo.ptr(), "set_size", p_restore);
		ur->commit_action();
	}

	if (Object::cast_to<QuadOccluder3D>(*o)) {
		Ref<QuadOccluder3D> qo = o;
		if (p_cancel) {
			qo->set_size(p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Box Shape Size"));
		ur->add_do_method(qo.ptr(), "set_size", qo->get_size());
		ur->add_undo_method(qo.ptr(), "set_size", p_restore);
		ur->commit_action();
	}
}

void OccluderInstance3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	OccluderInstance3D *occluder_instance = Object::cast_to<OccluderInstance3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Ref<Occluder3D> o = occluder_instance->get_occluder();

	if (o.is_null()) {
		return;
	}

	Vector<Vector3> lines = o->get_debug_lines();
	if (!lines.is_empty()) {
		Ref<Material> material = get_material("line_material", p_gizmo);
		p_gizmo->add_lines(lines, material);
		p_gizmo->add_collision_segments(lines);
	}

	Ref<Material> handles_material = get_material("handles");
	if (Object::cast_to<SphereOccluder3D>(*o)) {
		Ref<SphereOccluder3D> so = o;
		float r = so->get_radius();
		Vector<Vector3> handles = { Vector3(r, 0, 0) };
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<BoxOccluder3D>(*o)) {
		Ref<BoxOccluder3D> bo = o;

		Vector<Vector3> handles;
		for (int i = 0; i < 3; i++) {
			Vector3 ax;
			ax[i] = bo->get_size()[i] / 2;
			handles.push_back(ax);
		}

		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<QuadOccluder3D>(*o)) {
		Ref<QuadOccluder3D> qo = o;
		Vector2 size = qo->get_size();
		Vector3 s = Vector3(size.x, size.y, 0.0f) / 2.0f;
		Vector<Vector3> handles = { Vector3(s.x, 0.0f, 0.0f), Vector3(0.0f, s.y, 0.0f), Vector3(s.x, s.y, 0.0f) };
		p_gizmo->add_handles(handles, handles_material);
	}
}
