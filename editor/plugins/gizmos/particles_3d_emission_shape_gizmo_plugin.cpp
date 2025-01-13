/**************************************************************************/
/*  particles_3d_emission_shape_gizmo_plugin.cpp                          */
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

#include "particles_3d_emission_shape_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/particle_process_material.h"

Particles3DEmissionShapeGizmoPlugin::Particles3DEmissionShapeGizmoPlugin() {
	helper.instantiate();

	Color gizmo_color = EDITOR_DEF_RST("editors/3d_gizmos/gizmo_colors/particles_emission_shape", Color(0.5, 0.7, 1));
	create_material("particles_emission_shape_material", gizmo_color);

	create_handle_material("handles");
}

bool Particles3DEmissionShapeGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<GPUParticles3D>(p_spatial) || Object::cast_to<CPUParticles3D>(p_spatial) != nullptr;
}

String Particles3DEmissionShapeGizmoPlugin::get_gizmo_name() const {
	return "Particles3DEmissionShape";
}

int Particles3DEmissionShapeGizmoPlugin::get_priority() const {
	return -1;
}

bool Particles3DEmissionShapeGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

String Particles3DEmissionShapeGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return "";
}

Variant Particles3DEmissionShapeGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return Variant();
}

void Particles3DEmissionShapeGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
}

void Particles3DEmissionShapeGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
}

void Particles3DEmissionShapeGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();

	if (Object::cast_to<GPUParticles3D>(p_gizmo->get_node_3d())) {
		const GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(p_gizmo->get_node_3d());

		if (particles->get_process_material().is_valid()) {
			const Ref<ParticleProcessMaterial> mat = particles->get_process_material();
			const ParticleProcessMaterial::EmissionShape shape = mat->get_emission_shape();

			const Ref<Material> material = get_material("particles_emission_shape_material", p_gizmo);
			const Ref<Material> handles_material = get_material("handles");

			if (shape == ParticleProcessMaterial::EMISSION_SHAPE_SPHERE || shape == ParticleProcessMaterial::EMISSION_SHAPE_SPHERE_SURFACE) {
				const Vector3 offset = mat->get_emission_shape_offset();
				const Vector3 scale = mat->get_emission_shape_scale();

				const float r = mat->get_emission_sphere_radius();
				Vector<Vector3> points;
				for (int i = 0; i <= 120; i++) {
					const float ra = Math::deg_to_rad((float)(i * 3));
					const float rb = Math::deg_to_rad((float)((i + 1) * 3));
					const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
					const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

					points.push_back(Vector3(a.x * scale.x + offset.x, offset.y, a.y * scale.z + offset.z));
					points.push_back(Vector3(b.x * scale.x + offset.x, offset.y, b.y * scale.z + offset.z));
					points.push_back(Vector3(offset.x, a.x * scale.y + offset.y, a.y * scale.z + offset.z));
					points.push_back(Vector3(offset.x, b.x * scale.y + offset.y, b.y * scale.z + offset.z));
					points.push_back(Vector3(a.x * scale.x + offset.x, a.y * scale.y + offset.y, offset.z));
					points.push_back(Vector3(b.x * scale.x + offset.x, b.y * scale.y + offset.y, offset.z));
				}

				if (p_gizmo->is_selected()) {
					p_gizmo->add_lines(points, material);
				}
			} else if (shape == ParticleProcessMaterial::EMISSION_SHAPE_BOX) {
				const Vector3 offset = mat->get_emission_shape_offset();
				const Vector3 scale = mat->get_emission_shape_scale();

				const Vector3 box_extents = mat->get_emission_box_extents();
				Ref<BoxMesh> box;
				box.instantiate();
				const AABB box_aabb = box->get_aabb();
				Vector<Vector3> lines;

				for (int i = 0; i < 12; i++) {
					Vector3 a;
					Vector3 b;
					box_aabb.get_edge(i, a, b);
					// Multiplication by 2 due to the extents being only half of the box size.
					lines.push_back(a * 2.0 * scale * box_extents + offset);
					lines.push_back(b * 2.0 * scale * box_extents + offset);
				}

				if (p_gizmo->is_selected()) {
					p_gizmo->add_lines(lines, material);
				}
			} else if (shape == ParticleProcessMaterial::EMISSION_SHAPE_RING) {
				const Vector3 offset = mat->get_emission_shape_offset();
				const Vector3 scale = mat->get_emission_shape_scale();

				const float ring_height = mat->get_emission_ring_height();
				const float half_ring_height = ring_height / 2;
				const float ring_radius = mat->get_emission_ring_radius();
				const float ring_inner_radius = mat->get_emission_ring_inner_radius();
				const Vector3 ring_axis = mat->get_emission_ring_axis();
				const float ring_cone_angle = mat->get_emission_ring_cone_angle();
				const float ring_radius_top = MAX(ring_radius - Math::tan(Math::deg_to_rad(90.0 - ring_cone_angle)) * ring_height, 0.0);
				const float ring_inner_radius_top = (ring_inner_radius / ring_radius) * ring_radius_top;

				Vector<Vector3> points;

				Basis basis;
				basis.rows[1] = ring_axis.normalized();
				basis.rows[0] = Vector3(basis[1][1], -basis[1][2], -basis[1][0]).normalized();
				basis.rows[0] = (basis[0] - basis[0].dot(basis[1]) * basis[1]).normalized();
				basis[2] = basis[0].cross(basis[1]).normalized();
				basis.invert();

				for (int i = 0; i <= 120; i++) {
					const float ra = Math::deg_to_rad((float)(i * 3));
					const float ra_sin = Math::sin(ra);
					const float ra_cos = Math::cos(ra);
					const float rb = Math::deg_to_rad((float)((i + 1) * 3));
					const float rb_sin = Math::sin(rb);
					const float rb_cos = Math::cos(rb);
					const Point2 a = Vector2(ra_sin, ra_cos) * ring_radius;
					const Point2 b = Vector2(rb_sin, rb_cos) * ring_radius;
					const Point2 a2 = Vector2(ra_sin, ra_cos) * ring_radius_top;
					const Point2 b2 = Vector2(rb_sin, rb_cos) * ring_radius_top;
					const Point2 inner_a = Vector2(ra_sin, ra_cos) * ring_inner_radius;
					const Point2 inner_b = Vector2(rb_sin, rb_cos) * ring_inner_radius;
					const Point2 inner_a2 = Vector2(ra_sin, ra_cos) * ring_inner_radius_top;
					const Point2 inner_b2 = Vector2(rb_sin, rb_cos) * ring_inner_radius_top;

					// Outer top ring cap.
					points.push_back(basis.xform(Vector3(a2.x, half_ring_height, a2.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(b2.x, half_ring_height, b2.y)) * scale + offset);

					// Outer bottom ring cap.
					points.push_back(basis.xform(Vector3(a.x, -half_ring_height, a.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(b.x, -half_ring_height, b.y)) * scale + offset);

					// Inner top ring cap.
					points.push_back(basis.xform(Vector3(inner_a2.x, half_ring_height, inner_a2.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(inner_b2.x, half_ring_height, inner_b2.y)) * scale + offset);

					// Inner bottom ring cap.
					points.push_back(basis.xform(Vector3(inner_a.x, -half_ring_height, inner_a.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(inner_b.x, -half_ring_height, inner_b.y)) * scale + offset);
				}

				for (int i = 0; i <= 120; i = i + 30) {
					const float ra = Math::deg_to_rad((float)(i * 3));
					const float ra_sin = Math::sin(ra);
					const float ra_cos = Math::cos(ra);
					const Point2 a = Vector2(ra_sin, ra_cos) * ring_radius;
					const Point2 a2 = Vector2(ra_sin, ra_cos) * ring_radius_top;
					const Point2 inner_a = Vector2(ra_sin, ra_cos) * ring_inner_radius;
					const Point2 inner_a2 = Vector2(ra_sin, ra_cos) * ring_inner_radius_top;

					// Outer 90 degrees vertical lines.
					points.push_back(basis.xform(Vector3(a2.x, half_ring_height, a2.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(a.x, -half_ring_height, a.y)) * scale + offset);

					// Inner 90 degrees vertical lines.
					points.push_back(basis.xform(Vector3(inner_a2.x, half_ring_height, inner_a2.y)) * scale + offset);
					points.push_back(basis.xform(Vector3(inner_a.x, -half_ring_height, inner_a.y)) * scale + offset);
				}

				if (p_gizmo->is_selected()) {
					p_gizmo->add_lines(points, material);
				}
			}
		}
	} else if (Object::cast_to<CPUParticles3D>(p_gizmo->get_node_3d())) {
		const CPUParticles3D *particles = Object::cast_to<CPUParticles3D>(p_gizmo->get_node_3d());
		const CPUParticles3D::EmissionShape shape = particles->get_emission_shape();

		const Ref<Material> material = get_material("particles_emission_shape_material", p_gizmo);
		const Ref<Material> handles_material = get_material("handles");

		if (shape == CPUParticles3D::EMISSION_SHAPE_SPHERE || shape == CPUParticles3D::EMISSION_SHAPE_SPHERE_SURFACE) {
			const float r = particles->get_emission_sphere_radius();
			Vector<Vector3> points;
			for (int i = 0; i <= 120; i++) {
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float rb = Math::deg_to_rad((float)((i + 1) * 3));
				const Point2 a = Vector2(Math::sin(ra), Math::cos(ra)) * r;
				const Point2 b = Vector2(Math::sin(rb), Math::cos(rb)) * r;

				points.push_back(Vector3(a.x, 0.0, a.y));
				points.push_back(Vector3(b.x, 0.0, b.y));
				points.push_back(Vector3(0.0, a.x, a.y));
				points.push_back(Vector3(0.0, b.x, b.y));
				points.push_back(Vector3(a.x, a.y, 0.0));
				points.push_back(Vector3(b.x, b.y, 0.0));
			}

			if (p_gizmo->is_selected()) {
				p_gizmo->add_lines(points, material);
			}
		} else if (shape == CPUParticles3D::EMISSION_SHAPE_BOX) {
			const Vector3 box_extents = particles->get_emission_box_extents();
			Ref<BoxMesh> box;
			box.instantiate();
			const AABB box_aabb = box->get_aabb();
			Vector<Vector3> lines;

			for (int i = 0; i < 12; i++) {
				Vector3 a;
				Vector3 b;
				box_aabb.get_edge(i, a, b);
				// Multiplication by 2 due to the extents being only half of the box size.
				lines.push_back(a * 2.0 * box_extents);
				lines.push_back(b * 2.0 * box_extents);
			}

			if (p_gizmo->is_selected()) {
				p_gizmo->add_lines(lines, material);
			}
		} else if (shape == CPUParticles3D::EMISSION_SHAPE_RING) {
			const float ring_height = particles->get_emission_ring_height();
			const float half_ring_height = ring_height / 2;
			const float ring_radius = particles->get_emission_ring_radius();
			const float ring_inner_radius = particles->get_emission_ring_inner_radius();
			const Vector3 ring_axis = particles->get_emission_ring_axis();
			const float ring_cone_angle = particles->get_emission_ring_cone_angle();
			const float ring_radius_top = MAX(ring_radius - Math::tan(Math::deg_to_rad(90.0 - ring_cone_angle)) * ring_height, 0.0);
			const float ring_inner_radius_top = (ring_inner_radius / ring_radius) * ring_radius_top;

			Vector<Vector3> points;

			Basis basis;
			basis.rows[1] = ring_axis.normalized();
			basis.rows[0] = Vector3(basis[1][1], -basis[1][2], -basis[1][0]).normalized();
			basis.rows[0] = (basis[0] - basis[0].dot(basis[1]) * basis[1]).normalized();
			basis[2] = basis[0].cross(basis[1]).normalized();
			basis.invert();

			for (int i = 0; i <= 120; i++) {
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float ra_sin = Math::sin(ra);
				const float ra_cos = Math::cos(ra);
				const float rb = Math::deg_to_rad((float)((i + 1) * 3));
				const float rb_sin = Math::sin(rb);
				const float rb_cos = Math::cos(rb);
				const Point2 a = Vector2(ra_sin, ra_cos) * ring_radius;
				const Point2 b = Vector2(rb_sin, rb_cos) * ring_radius;
				const Point2 a2 = Vector2(ra_sin, ra_cos) * ring_radius_top;
				const Point2 b2 = Vector2(rb_sin, rb_cos) * ring_radius_top;
				const Point2 inner_a = Vector2(ra_sin, ra_cos) * ring_inner_radius;
				const Point2 inner_b = Vector2(rb_sin, rb_cos) * ring_inner_radius;
				const Point2 inner_a2 = Vector2(ra_sin, ra_cos) * ring_inner_radius_top;
				const Point2 inner_b2 = Vector2(rb_sin, rb_cos) * ring_inner_radius_top;

				// Outer top ring cap.
				points.push_back(basis.xform(Vector3(a2.x, half_ring_height, a2.y)));
				points.push_back(basis.xform(Vector3(b2.x, half_ring_height, b2.y)));

				// Outer bottom ring cap.
				points.push_back(basis.xform(Vector3(a.x, -half_ring_height, a.y)));
				points.push_back(basis.xform(Vector3(b.x, -half_ring_height, b.y)));

				// Inner top ring cap.
				points.push_back(basis.xform(Vector3(inner_a2.x, half_ring_height, inner_a2.y)));
				points.push_back(basis.xform(Vector3(inner_b2.x, half_ring_height, inner_b2.y)));

				// Inner bottom ring cap.
				points.push_back(basis.xform(Vector3(inner_a.x, -half_ring_height, inner_a.y)));
				points.push_back(basis.xform(Vector3(inner_b.x, -half_ring_height, inner_b.y)));
			}

			for (int i = 0; i <= 120; i = i + 30) {
				const float ra = Math::deg_to_rad((float)(i * 3));
				const float ra_sin = Math::sin(ra);
				const float ra_cos = Math::cos(ra);
				const Point2 a = Vector2(ra_sin, ra_cos) * ring_radius;
				const Point2 a2 = Vector2(ra_sin, ra_cos) * ring_radius_top;
				const Point2 inner_a = Vector2(ra_sin, ra_cos) * ring_inner_radius;
				const Point2 inner_a2 = Vector2(ra_sin, ra_cos) * ring_inner_radius_top;

				// Outer 90 degrees vertical lines.
				points.push_back(basis.xform(Vector3(a2.x, half_ring_height, a2.y)));
				points.push_back(basis.xform(Vector3(a.x, -half_ring_height, a.y)));

				// Inner 90 degrees vertical lines.
				points.push_back(basis.xform(Vector3(inner_a2.x, half_ring_height, inner_a2.y)));
				points.push_back(basis.xform(Vector3(inner_a.x, -half_ring_height, inner_a.y)));
			}

			if (p_gizmo->is_selected()) {
				p_gizmo->add_lines(points, material);
			}
		}
	}
}
