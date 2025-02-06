/**************************************************************************/
/*  gpu_particles_collision_3d_gizmo_plugin.cpp                           */
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

#include "gpu_particles_collision_3d_gizmo_plugin.h"

#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/gizmos/gizmo_3d_helper.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/gpu_particles_collision_3d.h"

GPUParticlesCollision3DGizmoPlugin::GPUParticlesCollision3DGizmoPlugin() {
	helper.instantiate();
	GPUParticlesCollision3DGizmoPlugin::update_materials();
}

GPUParticlesCollision3DGizmoPlugin::~GPUParticlesCollision3DGizmoPlugin() {
}

bool GPUParticlesCollision3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return (Object::cast_to<GPUParticlesCollision3D>(p_spatial) != nullptr) || (Object::cast_to<GPUParticlesAttractor3D>(p_spatial) != nullptr);
}

String GPUParticlesCollision3DGizmoPlugin::get_gizmo_name() const {
	return "GPUParticlesCollision3D";
}

int GPUParticlesCollision3DGizmoPlugin::get_priority() const {
	return -1;
}

String GPUParticlesCollision3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	const Node3D *cs = p_gizmo->get_node_3d();

	if (Object::cast_to<GPUParticlesCollisionSphere3D>(cs) || Object::cast_to<GPUParticlesAttractorSphere3D>(cs)) {
		return "Radius";
	}

	if (Object::cast_to<GPUParticlesCollisionBox3D>(cs) || Object::cast_to<GPUParticlesAttractorBox3D>(cs) || Object::cast_to<GPUParticlesAttractorVectorField3D>(cs) || Object::cast_to<GPUParticlesCollisionSDF3D>(cs) || Object::cast_to<GPUParticlesCollisionHeightField3D>(cs)) {
		return helper->box_get_handle_name(p_id);
	}

	return "";
}

Variant GPUParticlesCollision3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	const Node3D *cs = p_gizmo->get_node_3d();

	if (Object::cast_to<GPUParticlesCollisionSphere3D>(cs) || Object::cast_to<GPUParticlesAttractorSphere3D>(cs)) {
		return p_gizmo->get_node_3d()->call("get_radius");
	}

	if (Object::cast_to<GPUParticlesCollisionBox3D>(cs) || Object::cast_to<GPUParticlesAttractorBox3D>(cs) || Object::cast_to<GPUParticlesAttractorVectorField3D>(cs) || Object::cast_to<GPUParticlesCollisionSDF3D>(cs) || Object::cast_to<GPUParticlesCollisionHeightField3D>(cs)) {
		return Vector3(p_gizmo->get_node_3d()->call("get_size"));
	}

	return Variant();
}

void GPUParticlesCollision3DGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	helper->initialize_handle_action(get_handle_value(p_gizmo, p_id, p_secondary), p_gizmo->get_node_3d()->get_global_transform());
}

void GPUParticlesCollision3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	Node3D *sn = p_gizmo->get_node_3d();

	Vector3 sg[2];
	helper->get_segment(p_camera, p_point, sg);

	if (Object::cast_to<GPUParticlesCollisionSphere3D>(sn) || Object::cast_to<GPUParticlesAttractorSphere3D>(sn)) {
		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(), Vector3(4096, 0, 0), sg[0], sg[1], ra, rb);
		float d = ra.x;
		if (Node3DEditor::get_singleton()->is_snap_enabled()) {
			d = Math::snapped(d, Node3DEditor::get_singleton()->get_translate_snap());
		}

		if (d < 0.001) {
			d = 0.001;
		}

		sn->call("set_radius", d);
	}

	if (Object::cast_to<GPUParticlesCollisionBox3D>(sn) || Object::cast_to<GPUParticlesAttractorBox3D>(sn) || Object::cast_to<GPUParticlesAttractorVectorField3D>(sn) || Object::cast_to<GPUParticlesCollisionSDF3D>(sn) || Object::cast_to<GPUParticlesCollisionHeightField3D>(sn)) {
		Vector3 size = sn->call("get_size");
		Vector3 position;
		helper->box_set_handle(sg, p_id, size, position);
		sn->call("set_size", size);
		sn->set_global_position(position);
	}
}

void GPUParticlesCollision3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	Node3D *sn = p_gizmo->get_node_3d();

	if (Object::cast_to<GPUParticlesCollisionSphere3D>(sn) || Object::cast_to<GPUParticlesAttractorSphere3D>(sn)) {
		if (p_cancel) {
			sn->call("set_radius", p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Radius"));
		ur->add_do_method(sn, "set_radius", sn->call("get_radius"));
		ur->add_undo_method(sn, "set_radius", p_restore);
		ur->commit_action();
	}

	if (Object::cast_to<GPUParticlesCollisionBox3D>(sn) || Object::cast_to<GPUParticlesAttractorBox3D>(sn) || Object::cast_to<GPUParticlesAttractorVectorField3D>(sn) || Object::cast_to<GPUParticlesCollisionSDF3D>(sn) || Object::cast_to<GPUParticlesCollisionHeightField3D>(sn)) {
		helper->box_commit_handle("Change Box Shape Size", p_cancel, sn);
	}
}

void GPUParticlesCollision3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Node3D *cs = p_gizmo->get_node_3d();

	p_gizmo->clear();

	Ref<Material> material;
	Ref<Material> material_internal;
	if (Object::cast_to<GPUParticlesAttractor3D>(cs)) {
		material = get_material("shape_material_attractor", p_gizmo);
		material_internal = get_material("shape_material_attractor_internal", p_gizmo);
	} else {
		material = get_material("shape_material_collision", p_gizmo);
		material_internal = get_material("shape_material_collision_internal", p_gizmo);
	}

	const Ref<Material> handles_material = get_material("handles");

	if (Object::cast_to<GPUParticlesCollisionSphere3D>(cs) || Object::cast_to<GPUParticlesAttractorSphere3D>(cs)) {
		float r = cs->call("get_radius");

		Vector<Vector3> points;

		for (int i = 0; i <= 360; i++) {
			float ra = Math::deg_to_rad((float)i);
			float rb = Math::deg_to_rad((float)i + 1);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

			points.push_back(Vector3(a.x, 0, a.y));
			points.push_back(Vector3(b.x, 0, b.y));
			points.push_back(Vector3(0, a.x, a.y));
			points.push_back(Vector3(0, b.x, b.y));
			points.push_back(Vector3(a.x, a.y, 0));
			points.push_back(Vector3(b.x, b.y, 0));
		}

		Vector<Vector3> collision_segments;

		for (int i = 0; i < 64; i++) {
			float ra = i * (Math_TAU / 64.0);
			float rb = (i + 1) * (Math_TAU / 64.0);
			Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
			Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

			collision_segments.push_back(Vector3(a.x, 0, a.y));
			collision_segments.push_back(Vector3(b.x, 0, b.y));
			collision_segments.push_back(Vector3(0, a.x, a.y));
			collision_segments.push_back(Vector3(0, b.x, b.y));
			collision_segments.push_back(Vector3(a.x, a.y, 0));
			collision_segments.push_back(Vector3(b.x, b.y, 0));
		}

		p_gizmo->add_lines(points, material);
		p_gizmo->add_collision_segments(collision_segments);
		Vector<Vector3> handles;
		handles.push_back(Vector3(r, 0, 0));
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<GPUParticlesCollisionBox3D>(cs) || Object::cast_to<GPUParticlesAttractorBox3D>(cs) || Object::cast_to<GPUParticlesAttractorVectorField3D>(cs) || Object::cast_to<GPUParticlesCollisionSDF3D>(cs) || Object::cast_to<GPUParticlesCollisionHeightField3D>(cs)) {
		Vector<Vector3> lines;
		AABB aabb;
		aabb.size = cs->call("get_size").operator Vector3();
		aabb.position = aabb.size / -2;

		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}

		Vector<Vector3> handles = helper->box_get_handles(aabb.size);

		p_gizmo->add_lines(lines, material);
		p_gizmo->add_collision_segments(lines);
		p_gizmo->add_handles(handles, handles_material);

		GPUParticlesCollisionSDF3D *col_sdf = Object::cast_to<GPUParticlesCollisionSDF3D>(cs);
		if (col_sdf) {
			static const int subdivs[GPUParticlesCollisionSDF3D::RESOLUTION_MAX] = { 16, 32, 64, 128, 256, 512 };
			int subdiv = subdivs[col_sdf->get_resolution()];
			float cell_size = aabb.get_longest_axis_size() / subdiv;

			lines.clear();

			for (int i = 1; i < subdiv; i++) {
				for (int j = 0; j < 3; j++) {
					if (cell_size * i > aabb.size[j]) {
						continue;
					}

					int j_n1 = (j + 1) % 3;
					int j_n2 = (j + 2) % 3;

					for (int k = 0; k < 4; k++) {
						Vector3 from = aabb.position, to = aabb.position;
						from[j] += cell_size * i;
						to[j] += cell_size * i;

						if (k & 1) {
							to[j_n1] += aabb.size[j_n1];
						} else {
							to[j_n2] += aabb.size[j_n2];
						}

						if (k & 2) {
							from[j_n1] += aabb.size[j_n1];
							from[j_n2] += aabb.size[j_n2];
						}

						lines.push_back(from);
						lines.push_back(to);
					}
				}
			}

			p_gizmo->add_lines(lines, material_internal);
		}
	}
}

void GPUParticlesCollision3DGizmoPlugin::update_materials() {
	Color gizmo_color_attractor = EDITOR_GET("editors/3d_gizmos/gizmo_colors/particle_attractor");
	create_material("shape_material_attractor", gizmo_color_attractor);
	gizmo_color_attractor.a = 0.15;
	create_material("shape_material_attractor_internal", gizmo_color_attractor);

	Color gizmo_color_collision = EDITOR_GET("editors/3d_gizmos/gizmo_colors/particle_collision");
	create_material("shape_material_collision", gizmo_color_collision);
	gizmo_color_collision.a = 0.15;
	create_material("shape_material_collision_internal", gizmo_color_collision);

	create_handle_material("handles");
}
