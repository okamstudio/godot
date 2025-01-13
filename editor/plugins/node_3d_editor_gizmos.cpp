/**************************************************************************/
/*  node_3d_editor_gizmos.cpp                                             */
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

#include "node_3d_editor_gizmos.h"

#include "core/math/geometry_2d.h"
#include "core/math/geometry_3d.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/resources/3d/primitive_meshes.h"

#define HANDLE_HALF_SIZE 9.5

bool EditorNode3DGizmo::is_editable() const {
	ERR_FAIL_NULL_V(spatial_node, false);
	Node *edited_root = spatial_node->get_tree()->get_edited_scene_root();
	if (spatial_node == edited_root) {
		return true;
	}
	if (spatial_node->get_owner() == edited_root) {
		return true;
	}

	if (edited_root->is_editable_instance(spatial_node->get_owner())) {
		return true;
	}

	return false;
}

void EditorNode3DGizmo::clear() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	for (int i = 0; i < instances.size(); i++) {
		if (instances[i].instance.is_valid()) {
			RS::get_singleton()->free(instances[i].instance);
		}
	}

	billboard_handle = false;
	collision_segments.clear();
	collision_mesh = Ref<TriangleMesh>();
	instances.clear();
	handles.clear();
	handle_ids.clear();
	secondary_handles.clear();
	secondary_handle_ids.clear();
}

void EditorNode3DGizmo::redraw() {
	if (!GDVIRTUAL_CALL(_redraw)) {
		ERR_FAIL_NULL(gizmo_plugin);
		gizmo_plugin->redraw(this);
	}

	_update_bvh();

	if (Node3DEditor::get_singleton()->is_current_selected_gizmo(this)) {
		Node3DEditor::get_singleton()->update_transform_gizmo();
	}
}

String EditorNode3DGizmo::get_handle_name(int p_id, bool p_secondary) const {
	String ret;
	if (GDVIRTUAL_CALL(_get_handle_name, p_id, p_secondary, ret)) {
		return ret;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, "");
	return gizmo_plugin->get_handle_name(this, p_id, p_secondary);
}

bool EditorNode3DGizmo::is_handle_highlighted(int p_id, bool p_secondary) const {
	bool success;
	if (GDVIRTUAL_CALL(_is_handle_highlighted, p_id, p_secondary, success)) {
		return success;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, false);
	return gizmo_plugin->is_handle_highlighted(this, p_id, p_secondary);
}

Variant EditorNode3DGizmo::get_handle_value(int p_id, bool p_secondary) const {
	Variant value;
	if (GDVIRTUAL_CALL(_get_handle_value, p_id, p_secondary, value)) {
		return value;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, Variant());
	return gizmo_plugin->get_handle_value(this, p_id, p_secondary);
}

void EditorNode3DGizmo::begin_handle_action(int p_id, bool p_secondary) {
	if (GDVIRTUAL_CALL(_begin_handle_action, p_id, p_secondary)) {
		return;
	}

	ERR_FAIL_NULL(gizmo_plugin);
	gizmo_plugin->begin_handle_action(this, p_id, p_secondary);
}

void EditorNode3DGizmo::set_handle(int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	if (GDVIRTUAL_CALL(_set_handle, p_id, p_secondary, p_camera, p_point)) {
		return;
	}

	ERR_FAIL_NULL(gizmo_plugin);
	gizmo_plugin->set_handle(this, p_id, p_secondary, p_camera, p_point);
}

void EditorNode3DGizmo::commit_handle(int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	if (GDVIRTUAL_CALL(_commit_handle, p_id, p_secondary, p_restore, p_cancel)) {
		return;
	}

	ERR_FAIL_NULL(gizmo_plugin);
	gizmo_plugin->commit_handle(this, p_id, p_secondary, p_restore, p_cancel);
}

int EditorNode3DGizmo::subgizmos_intersect_ray(Camera3D *p_camera, const Vector2 &p_point) const {
	int id;
	if (GDVIRTUAL_CALL(_subgizmos_intersect_ray, p_camera, p_point, id)) {
		return id;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, -1);
	return gizmo_plugin->subgizmos_intersect_ray(this, p_camera, p_point);
}

Vector<int> EditorNode3DGizmo::subgizmos_intersect_frustum(const Camera3D *p_camera, const Vector<Plane> &p_frustum) const {
	TypedArray<Plane> frustum;
	frustum.resize(p_frustum.size());
	for (int i = 0; i < p_frustum.size(); i++) {
		frustum[i] = p_frustum[i];
	}
	Vector<int> ret;
	if (GDVIRTUAL_CALL(_subgizmos_intersect_frustum, p_camera, frustum, ret)) {
		return ret;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, Vector<int>());
	return gizmo_plugin->subgizmos_intersect_frustum(this, p_camera, p_frustum);
}

Transform3D EditorNode3DGizmo::get_subgizmo_transform(int p_id) const {
	Transform3D ret;
	if (GDVIRTUAL_CALL(_get_subgizmo_transform, p_id, ret)) {
		return ret;
	}

	ERR_FAIL_NULL_V(gizmo_plugin, Transform3D());
	return gizmo_plugin->get_subgizmo_transform(this, p_id);
}

void EditorNode3DGizmo::set_subgizmo_transform(int p_id, Transform3D p_transform) {
	if (GDVIRTUAL_CALL(_set_subgizmo_transform, p_id, p_transform)) {
		return;
	}

	ERR_FAIL_NULL(gizmo_plugin);
	gizmo_plugin->set_subgizmo_transform(this, p_id, p_transform);
}

void EditorNode3DGizmo::commit_subgizmos(const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) {
	TypedArray<Transform3D> restore;
	restore.resize(p_restore.size());
	for (int i = 0; i < p_restore.size(); i++) {
		restore[i] = p_restore[i];
	}

	if (GDVIRTUAL_CALL(_commit_subgizmos, p_ids, restore, p_cancel)) {
		return;
	}

	ERR_FAIL_NULL(gizmo_plugin);
	gizmo_plugin->commit_subgizmos(this, p_ids, p_restore, p_cancel);
}

void EditorNode3DGizmo::set_node_3d(Node3D *p_node) {
	ERR_FAIL_NULL(p_node);
	spatial_node = p_node;
}

void EditorNode3DGizmo::Instance::create_instance(Node3D *p_base, bool p_hidden) {
	instance = RS::get_singleton()->instance_create2(mesh->get_rid(), p_base->get_world_3d()->get_scenario());
	RS::get_singleton()->instance_attach_object_instance_id(instance, p_base->get_instance_id());
	if (skin_reference.is_valid()) {
		RS::get_singleton()->instance_attach_skeleton(instance, skin_reference->get_skeleton());
	}
	if (extra_margin) {
		RS::get_singleton()->instance_set_extra_visibility_margin(instance, 1);
	}
	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(instance, RS::SHADOW_CASTING_SETTING_OFF);
	int layer = p_hidden ? 0 : 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER;
	RS::get_singleton()->instance_set_layer_mask(instance, layer); //gizmos are 26
	RS::get_singleton()->instance_geometry_set_flag(instance, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
	RS::get_singleton()->instance_geometry_set_flag(instance, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
}

void EditorNode3DGizmo::add_mesh(const Ref<Mesh> &p_mesh, const Ref<Material> &p_material, const Transform3D &p_xform, const Ref<SkinReference> &p_skin_reference) {
	ERR_FAIL_NULL(spatial_node);
	ERR_FAIL_COND_MSG(!p_mesh.is_valid(), "EditorNode3DGizmo.add_mesh() requires a valid Mesh resource.");

	Instance ins;

	ins.mesh = p_mesh;
	ins.skin_reference = p_skin_reference;
	ins.material = p_material;
	ins.xform = p_xform;
	if (valid) {
		ins.create_instance(spatial_node, hidden);
		RS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform() * ins.xform);
		if (ins.material.is_valid()) {
			RS::get_singleton()->instance_geometry_set_material_override(ins.instance, p_material->get_rid());
		}
	}

	instances.push_back(ins);
}

void EditorNode3DGizmo::_update_bvh() {
	ERR_FAIL_NULL(spatial_node);

	Transform3D transform = spatial_node->get_global_transform();

	float effective_icon_size = selectable_icon_size > 0.0f ? selectable_icon_size : 0.0f;
	Vector3 icon_size_vector3 = Vector3(effective_icon_size, effective_icon_size, effective_icon_size);
	AABB aabb(spatial_node->get_position() - icon_size_vector3 * 100.0f, icon_size_vector3 * 200.0f);

	for (const Vector3 &segment_end : collision_segments) {
		aabb.expand_to(transform.xform(segment_end));
	}

	if (collision_mesh.is_valid()) {
		for (const Face3 &face : collision_mesh->get_faces()) {
			aabb.expand_to(transform.xform(face.vertex[0]));
			aabb.expand_to(transform.xform(face.vertex[1]));
			aabb.expand_to(transform.xform(face.vertex[2]));
		}
	}

	Node3DEditor::get_singleton()->update_gizmo_bvh_node(
			bvh_node_id,
			aabb);
}

void EditorNode3DGizmo::add_lines(const Vector<Vector3> &p_lines, const Ref<Material> &p_material, bool p_billboard, const Color &p_modulate) {
	add_vertices(p_lines, p_material, Mesh::PRIMITIVE_LINES, p_billboard, p_modulate);
}

void EditorNode3DGizmo::add_vertices(const Vector<Vector3> &p_vertices, const Ref<Material> &p_material, Mesh::PrimitiveType p_primitive_type, bool p_billboard, const Color &p_modulate) {
	if (p_vertices.is_empty()) {
		return;
	}

	ERR_FAIL_NULL(spatial_node);
	Instance ins;

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);

	a[Mesh::ARRAY_VERTEX] = p_vertices;

	Vector<Color> color;
	color.resize(p_vertices.size());
	const Color vertex_color = (is_selected() ? Color(1, 1, 1, 0.8) : Color(1, 1, 1, 0.2)) * p_modulate;
	{
		Color *w = color.ptrw();
		for (int i = 0; i < p_vertices.size(); i++) {
			w[i] = vertex_color;
		}
	}

	a[Mesh::ARRAY_COLOR] = color;

	mesh->add_surface_from_arrays(p_primitive_type, a);
	mesh->surface_set_material(0, p_material);

	if (p_billboard) {
		float md = 0;
		for (int i = 0; i < p_vertices.size(); i++) {
			md = MAX(0, p_vertices[i].length());
		}
		if (md) {
			mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
		}
	}

	ins.mesh = mesh;
	if (valid) {
		ins.create_instance(spatial_node, hidden);
		RS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}

	instances.push_back(ins);
}

void EditorNode3DGizmo::add_unscaled_billboard(const Ref<Material> &p_material, real_t p_scale, const Color &p_modulate) {
	ERR_FAIL_NULL(spatial_node);
	Instance ins;

	Vector<Vector3> vs = {
		Vector3(-p_scale, p_scale, 0),
		Vector3(p_scale, p_scale, 0),
		Vector3(p_scale, -p_scale, 0),
		Vector3(-p_scale, -p_scale, 0)
	};

	Vector<Vector2> uv = {
		Vector2(0, 0),
		Vector2(1, 0),
		Vector2(1, 1),
		Vector2(0, 1)
	};

	Vector<Color> colors = {
		p_modulate,
		p_modulate,
		p_modulate,
		p_modulate
	};

	Vector<int> indices = { 0, 1, 2, 0, 2, 3 };

	Ref<ArrayMesh> mesh = memnew(ArrayMesh);
	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[Mesh::ARRAY_VERTEX] = vs;
	a[Mesh::ARRAY_TEX_UV] = uv;
	a[Mesh::ARRAY_INDEX] = indices;
	a[Mesh::ARRAY_COLOR] = colors;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);
	mesh->surface_set_material(0, p_material);

	float md = 0;
	for (int i = 0; i < vs.size(); i++) {
		md = MAX(0, vs[i].length());
	}
	if (md) {
		mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
	}

	selectable_icon_size = p_scale;
	mesh->set_custom_aabb(AABB(Vector3(-selectable_icon_size, -selectable_icon_size, -selectable_icon_size) * 100.0f, Vector3(selectable_icon_size, selectable_icon_size, selectable_icon_size) * 200.0f));

	ins.mesh = mesh;
	if (valid) {
		ins.create_instance(spatial_node, hidden);
		RS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}

	selectable_icon_size = p_scale;

	instances.push_back(ins);
}

void EditorNode3DGizmo::add_collision_triangles(const Ref<TriangleMesh> &p_tmesh) {
	collision_mesh = p_tmesh;
}

void EditorNode3DGizmo::add_collision_segments(const Vector<Vector3> &p_lines) {
	int from = collision_segments.size();
	collision_segments.resize(from + p_lines.size());
	for (int i = 0; i < p_lines.size(); i++) {
		collision_segments.write[from + i] = p_lines[i];
	}
}

void EditorNode3DGizmo::add_handles(const Vector<Vector3> &p_handles, const Ref<Material> &p_material, const Vector<int> &p_ids, bool p_billboard, bool p_secondary) {
	billboard_handle = p_billboard;

	if (!is_selected() || !is_editable()) {
		return;
	}

	ERR_FAIL_NULL(spatial_node);

	Vector<Vector3> &handle_list = p_secondary ? secondary_handles : handles;
	Vector<int> &id_list = p_secondary ? secondary_handle_ids : handle_ids;

	if (p_ids.is_empty()) {
		ERR_FAIL_COND_MSG(!id_list.is_empty(), "IDs must be provided for all handles, as handles with IDs already exist.");
	} else {
		ERR_FAIL_COND_MSG(p_handles.size() != p_ids.size(), "The number of IDs should be the same as the number of handles.");
	}

	bool is_current_hover_gizmo = Node3DEditor::get_singleton()->get_current_hover_gizmo() == this;
	bool current_hover_handle_secondary;
	int current_hover_handle = Node3DEditor::get_singleton()->get_current_hover_gizmo_handle(current_hover_handle_secondary);

	Instance ins;
	Ref<ArrayMesh> mesh = memnew(ArrayMesh);

	Array a;
	a.resize(RS::ARRAY_MAX);
	a[RS::ARRAY_VERTEX] = p_handles;
	Vector<Color> colors;
	{
		colors.resize(p_handles.size());
		Color *w = colors.ptrw();
		for (int i = 0; i < p_handles.size(); i++) {
			int id = p_ids.is_empty() ? i : p_ids[i];

			Color col(1, 1, 1, 1);
			if (is_handle_highlighted(id, p_secondary)) {
				col = Color(0, 0, 1, 0.9);
			}

			if (!is_current_hover_gizmo || current_hover_handle != id || p_secondary != current_hover_handle_secondary) {
				col.a = 0.8;
			}

			w[i] = col;
		}
	}
	a[RS::ARRAY_COLOR] = colors;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, a);
	mesh->surface_set_material(0, p_material);

	if (p_billboard) {
		float md = 0;
		for (int i = 0; i < p_handles.size(); i++) {
			md = MAX(0, p_handles[i].length());
		}
		if (md) {
			mesh->set_custom_aabb(AABB(Vector3(-md, -md, -md), Vector3(md, md, md) * 2.0));
		}
	}

	ins.mesh = mesh;
	ins.extra_margin = true;
	if (valid) {
		ins.create_instance(spatial_node, hidden);
		RS::get_singleton()->instance_set_transform(ins.instance, spatial_node->get_global_transform());
	}
	instances.push_back(ins);

	int current_size = handle_list.size();
	handle_list.resize(current_size + p_handles.size());
	for (int i = 0; i < p_handles.size(); i++) {
		handle_list.write[current_size + i] = p_handles[i];
	}

	if (!p_ids.is_empty()) {
		current_size = id_list.size();
		id_list.resize(current_size + p_ids.size());
		for (int i = 0; i < p_ids.size(); i++) {
			id_list.write[current_size + i] = p_ids[i];
		}
	}
}

void EditorNode3DGizmo::add_solid_box(const Ref<Material> &p_material, Vector3 p_size, Vector3 p_position, const Transform3D &p_xform) {
	ERR_FAIL_NULL(spatial_node);

	BoxMesh box_mesh;
	box_mesh.set_size(p_size);

	Array arrays = box_mesh.surface_get_arrays(0);
	PackedVector3Array vertex = arrays[RS::ARRAY_VERTEX];
	Vector3 *w = vertex.ptrw();

	for (int i = 0; i < vertex.size(); ++i) {
		w[i] += p_position;
	}

	arrays[RS::ARRAY_VERTEX] = vertex;

	Ref<ArrayMesh> m = memnew(ArrayMesh);
	m->add_surface_from_arrays(box_mesh.surface_get_primitive_type(0), arrays);
	add_mesh(m, p_material, p_xform);
}

bool EditorNode3DGizmo::intersect_frustum(const Camera3D *p_camera, const Vector<Plane> &p_frustum) {
	ERR_FAIL_NULL_V(spatial_node, false);
	ERR_FAIL_COND_V(!valid, false);

	if (hidden && !gizmo_plugin->is_selectable_when_hidden()) {
		return false;
	}

	if (selectable_icon_size > 0.0f) {
		Vector3 origin = spatial_node->get_global_transform().get_origin();

		const Plane *p = p_frustum.ptr();
		int fc = p_frustum.size();

		bool any_out = false;

		for (int j = 0; j < fc; j++) {
			if (p[j].is_point_over(origin)) {
				any_out = true;
				break;
			}
		}

		return !any_out;
	}

	if (collision_segments.size()) {
		const Plane *p = p_frustum.ptr();
		int fc = p_frustum.size();

		int vc = collision_segments.size();
		const Vector3 *vptr = collision_segments.ptr();
		Transform3D t = spatial_node->get_global_transform();

		bool any_out = false;
		for (int j = 0; j < fc; j++) {
			for (int i = 0; i < vc; i++) {
				Vector3 v = t.xform(vptr[i]);
				if (p[j].is_point_over(v)) {
					any_out = true;
					break;
				}
			}
			if (any_out) {
				break;
			}
		}

		if (!any_out) {
			return true;
		}
	}

	if (collision_mesh.is_valid()) {
		Transform3D t = spatial_node->get_global_transform();

		Vector3 mesh_scale = t.get_basis().get_scale();
		t.orthonormalize();

		Transform3D it = t.affine_inverse();

		Vector<Plane> transformed_frustum;
		int plane_count = p_frustum.size();
		transformed_frustum.resize(plane_count);

		for (int i = 0; i < plane_count; i++) {
			transformed_frustum.write[i] = it.xform(p_frustum[i]);
		}

		Vector<Vector3> convex_points = Geometry3D::compute_convex_mesh_points(transformed_frustum.ptr(), plane_count);
		if (collision_mesh->inside_convex_shape(transformed_frustum.ptr(), plane_count, convex_points.ptr(), convex_points.size(), mesh_scale)) {
			return true;
		}
	}

	return false;
}

void EditorNode3DGizmo::handles_intersect_ray(Camera3D *p_camera, const Vector2 &p_point, bool p_shift_pressed, int &r_id, bool &r_secondary) {
	r_id = -1;
	r_secondary = false;

	ERR_FAIL_NULL(spatial_node);
	ERR_FAIL_COND(!valid);

	if (hidden) {
		return;
	}

	Transform3D camera_xform = p_camera->get_global_transform();
	Transform3D t = spatial_node->get_global_transform();
	if (billboard_handle) {
		t.set_look_at(t.origin, t.origin - camera_xform.basis.get_column(2), camera_xform.basis.get_column(1));
	}

	float min_d = 1e20;

	for (int i = 0; i < secondary_handles.size(); i++) {
		Vector3 hpos = t.xform(secondary_handles[i]);
		Vector2 p = p_camera->unproject_position(hpos);

		if (p.distance_to(p_point) < HANDLE_HALF_SIZE) {
			real_t dp = p_camera->get_transform().origin.distance_to(hpos);
			if (dp < min_d) {
				min_d = dp;
				if (secondary_handle_ids.is_empty()) {
					r_id = i;
				} else {
					r_id = secondary_handle_ids[i];
				}
				r_secondary = true;
			}
		}
	}

	if (r_id != -1 && p_shift_pressed) {
		return;
	}

	min_d = 1e20;

	for (int i = 0; i < handles.size(); i++) {
		Vector3 hpos = t.xform(handles[i]);
		Vector2 p = p_camera->unproject_position(hpos);

		if (p.distance_to(p_point) < HANDLE_HALF_SIZE) {
			real_t dp = p_camera->get_transform().origin.distance_to(hpos);
			if (dp < min_d) {
				min_d = dp;
				if (handle_ids.is_empty()) {
					r_id = i;
				} else {
					r_id = handle_ids[i];
				}
				r_secondary = false;
			}
		}
	}
}

bool EditorNode3DGizmo::intersect_ray(Camera3D *p_camera, const Point2 &p_point, Vector3 &r_pos, Vector3 &r_normal) {
	ERR_FAIL_NULL_V(spatial_node, false);
	ERR_FAIL_COND_V(!valid, false);

	if (hidden && !gizmo_plugin->is_selectable_when_hidden()) {
		return false;
	}

	if (selectable_icon_size > 0.0f) {
		Transform3D t = spatial_node->get_global_transform();
		Vector3 camera_position = p_camera->get_camera_transform().origin;
		if (!camera_position.is_equal_approx(t.origin)) {
			t.set_look_at(t.origin, camera_position);
		}

		float scale = t.origin.distance_to(p_camera->get_camera_transform().origin);

		if (p_camera->get_projection() == Camera3D::PROJECTION_ORTHOGONAL) {
			float aspect = p_camera->get_viewport()->get_visible_rect().size.aspect();
			float size = p_camera->get_size();
			scale = size / aspect;
		}

		Point2 center = p_camera->unproject_position(t.origin);

		Transform3D orig_camera_transform = p_camera->get_camera_transform();

		if (!orig_camera_transform.origin.is_equal_approx(t.origin) &&
				ABS(orig_camera_transform.basis.get_column(Vector3::AXIS_Z).dot(Vector3(0, 1, 0))) < 0.99) {
			p_camera->look_at(t.origin);
		}

		Vector3 c0 = t.xform(Vector3(selectable_icon_size, selectable_icon_size, 0) * scale);
		Vector3 c1 = t.xform(Vector3(-selectable_icon_size, -selectable_icon_size, 0) * scale);

		Point2 p0 = p_camera->unproject_position(c0);
		Point2 p1 = p_camera->unproject_position(c1);

		p_camera->set_global_transform(orig_camera_transform);

		Rect2 rect(p0, (p1 - p0).abs());

		rect.set_position(center - rect.get_size() / 2.0);

		if (rect.has_point(p_point)) {
			r_pos = t.origin;
			r_normal = -p_camera->project_ray_normal(p_point);
			return true;
		}
	}

	if (collision_segments.size()) {
		Plane camp(-p_camera->get_transform().basis.get_column(2).normalized(), p_camera->get_transform().origin);

		int vc = collision_segments.size();
		const Vector3 *vptr = collision_segments.ptr();
		Transform3D t = spatial_node->get_global_transform();
		if (billboard_handle) {
			t.set_look_at(t.origin, t.origin - p_camera->get_transform().basis.get_column(2), p_camera->get_transform().basis.get_column(1));
		}

		Vector3 cp;
		float cpd = 1e20;

		for (int i = 0; i < vc / 2; i++) {
			Vector3 a = t.xform(vptr[i * 2 + 0]);
			Vector3 b = t.xform(vptr[i * 2 + 1]);
			Vector2 s[2];
			s[0] = p_camera->unproject_position(a);
			s[1] = p_camera->unproject_position(b);

			Vector2 p = Geometry2D::get_closest_point_to_segment(p_point, s);

			float pd = p.distance_to(p_point);

			if (pd < cpd) {
				float d = s[0].distance_to(s[1]);
				Vector3 tcp;
				if (d > 0) {
					float d2 = s[0].distance_to(p) / d;
					tcp = a + (b - a) * d2;

				} else {
					tcp = a;
				}

				if (camp.distance_to(tcp) < p_camera->get_near()) {
					continue;
				}
				cp = tcp;
				cpd = pd;
			}
		}

		if (cpd < 8) {
			r_pos = cp;
			r_normal = -p_camera->project_ray_normal(p_point);
			return true;
		}
	}

	if (collision_mesh.is_valid()) {
		Transform3D gt = spatial_node->get_global_transform();

		if (billboard_handle) {
			gt.set_look_at(gt.origin, gt.origin - p_camera->get_transform().basis.get_column(2), p_camera->get_transform().basis.get_column(1));
		}

		Transform3D ai = gt.affine_inverse();
		Vector3 ray_from = ai.xform(p_camera->project_ray_origin(p_point));
		Vector3 ray_dir = ai.basis.xform(p_camera->project_ray_normal(p_point)).normalized();
		Vector3 rpos, rnorm;

		if (collision_mesh->intersect_ray(ray_from, ray_dir, rpos, rnorm)) {
			r_pos = gt.xform(rpos);
			r_normal = gt.basis.xform(rnorm).normalized();
			return true;
		}
	}

	return false;
}

bool EditorNode3DGizmo::is_subgizmo_selected(int p_id) const {
	Node3DEditor *ed = Node3DEditor::get_singleton();
	ERR_FAIL_NULL_V(ed, false);
	return ed->is_current_selected_gizmo(this) && ed->is_subgizmo_selected(p_id);
}

Vector<int> EditorNode3DGizmo::get_subgizmo_selection() const {
	Vector<int> ret;

	Node3DEditor *ed = Node3DEditor::get_singleton();
	ERR_FAIL_NULL_V(ed, ret);

	if (ed->is_current_selected_gizmo(this)) {
		ret = ed->get_subgizmo_selection();
	}

	return ret;
}

void EditorNode3DGizmo::create() {
	ERR_FAIL_NULL(spatial_node);
	ERR_FAIL_COND(valid);
	valid = true;

	for (int i = 0; i < instances.size(); i++) {
		instances.write[i].create_instance(spatial_node, hidden);
	}

	bvh_node_id = Node3DEditor::get_singleton()->insert_gizmo_bvh_node(
			spatial_node,
			AABB(spatial_node->get_position(), Vector3(0, 0, 0)));

	transform();
}

void EditorNode3DGizmo::transform() {
	ERR_FAIL_NULL(spatial_node);
	ERR_FAIL_COND(!valid);
	for (int i = 0; i < instances.size(); i++) {
		RS::get_singleton()->instance_set_transform(instances[i].instance, spatial_node->get_global_transform() * instances[i].xform);
	}

	_update_bvh();
}

void EditorNode3DGizmo::free() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	ERR_FAIL_NULL(spatial_node);
	ERR_FAIL_COND(!valid);

	for (int i = 0; i < instances.size(); i++) {
		if (instances[i].instance.is_valid()) {
			RS::get_singleton()->free(instances[i].instance);
		}
		instances.write[i].instance = RID();
	}

	clear();

	Node3DEditor::get_singleton()->remove_gizmo_bvh_node(bvh_node_id);
	bvh_node_id = DynamicBVH::ID();

	valid = false;
}

void EditorNode3DGizmo::set_hidden(bool p_hidden) {
	hidden = p_hidden;
	int layer = hidden ? 0 : 1 << Node3DEditorViewport::GIZMO_EDIT_LAYER;
	for (int i = 0; i < instances.size(); ++i) {
		RS::get_singleton()->instance_set_layer_mask(instances[i].instance, layer);
	}
}

void EditorNode3DGizmo::set_plugin(EditorNode3DGizmoPlugin *p_plugin) {
	gizmo_plugin = p_plugin;
}

void EditorNode3DGizmo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_lines", "lines", "material", "billboard", "modulate"), &EditorNode3DGizmo::add_lines, DEFVAL(false), DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("add_mesh", "mesh", "material", "transform", "skeleton"), &EditorNode3DGizmo::add_mesh, DEFVAL(Variant()), DEFVAL(Transform3D()), DEFVAL(Ref<SkinReference>()));
	ClassDB::bind_method(D_METHOD("add_collision_segments", "segments"), &EditorNode3DGizmo::add_collision_segments);
	ClassDB::bind_method(D_METHOD("add_collision_triangles", "triangles"), &EditorNode3DGizmo::add_collision_triangles);
	ClassDB::bind_method(D_METHOD("add_unscaled_billboard", "material", "default_scale", "modulate"), &EditorNode3DGizmo::add_unscaled_billboard, DEFVAL(1), DEFVAL(Color(1, 1, 1)));
	ClassDB::bind_method(D_METHOD("add_handles", "handles", "material", "ids", "billboard", "secondary"), &EditorNode3DGizmo::add_handles, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_node_3d", "node"), &EditorNode3DGizmo::_set_node_3d);
	ClassDB::bind_method(D_METHOD("get_node_3d"), &EditorNode3DGizmo::get_node_3d);
	ClassDB::bind_method(D_METHOD("get_plugin"), &EditorNode3DGizmo::get_plugin);
	ClassDB::bind_method(D_METHOD("clear"), &EditorNode3DGizmo::clear);
	ClassDB::bind_method(D_METHOD("set_hidden", "hidden"), &EditorNode3DGizmo::set_hidden);
	ClassDB::bind_method(D_METHOD("is_subgizmo_selected", "id"), &EditorNode3DGizmo::is_subgizmo_selected);
	ClassDB::bind_method(D_METHOD("get_subgizmo_selection"), &EditorNode3DGizmo::get_subgizmo_selection);

	GDVIRTUAL_BIND(_redraw);
	GDVIRTUAL_BIND(_get_handle_name, "id", "secondary");
	GDVIRTUAL_BIND(_is_handle_highlighted, "id", "secondary");

	GDVIRTUAL_BIND(_get_handle_value, "id", "secondary");
	GDVIRTUAL_BIND(_begin_handle_action, "id", "secondary");
	GDVIRTUAL_BIND(_set_handle, "id", "secondary", "camera", "point");
	GDVIRTUAL_BIND(_commit_handle, "id", "secondary", "restore", "cancel");

	GDVIRTUAL_BIND(_subgizmos_intersect_ray, "camera", "point");
	GDVIRTUAL_BIND(_subgizmos_intersect_frustum, "camera", "frustum");
	GDVIRTUAL_BIND(_set_subgizmo_transform, "id", "transform");
	GDVIRTUAL_BIND(_get_subgizmo_transform, "id");
	GDVIRTUAL_BIND(_commit_subgizmos, "ids", "restores", "cancel");
}

EditorNode3DGizmo::EditorNode3DGizmo() {
	valid = false;
	billboard_handle = false;
	hidden = false;
	selected = false;
	spatial_node = nullptr;
	gizmo_plugin = nullptr;
	selectable_icon_size = -1.0f;
}

EditorNode3DGizmo::~EditorNode3DGizmo() {
	if (gizmo_plugin != nullptr) {
		gizmo_plugin->unregister_gizmo(this);
	}
	clear();
}

/////

void EditorNode3DGizmoPlugin::create_material(const String &p_name, const Color &p_color, bool p_billboard, bool p_on_top, bool p_use_vertex_color) {
	Color instantiated_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/instantiated");

	Vector<Ref<StandardMaterial3D>> mats;

	for (int i = 0; i < 4; i++) {
		bool selected = i % 2 == 1;
		bool instantiated = i < 2;

		Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));

		Color color = instantiated ? instantiated_color : p_color;

		if (!selected) {
			color.a *= 0.3;
		}

		material->set_albedo(color);
		material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 1);
		material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

		if (p_use_vertex_color) {
			material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		}

		if (p_billboard) {
			material->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
		}

		if (p_on_top && selected) {
			material->set_on_top_of_alpha();
		}

		mats.push_back(material);
	}

	materials[p_name] = mats;
}

void EditorNode3DGizmoPlugin::create_icon_material(const String &p_name, const Ref<Texture2D> &p_texture, bool p_on_top, const Color &p_albedo) {
	Color instantiated_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/instantiated");

	Vector<Ref<StandardMaterial3D>> icons;

	for (int i = 0; i < 4; i++) {
		bool selected = i % 2 == 1;
		bool instantiated = i < 2;

		Ref<StandardMaterial3D> icon = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));

		Color color = instantiated ? instantiated_color : p_albedo;

		if (!selected) {
			color.r *= 0.6;
			color.g *= 0.6;
			color.b *= 0.6;
		}

		icon->set_albedo(color);

		icon->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		icon->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		icon->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		icon->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		icon->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		icon->set_alpha_scissor_threshold(0.1);
		icon->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, p_texture);
		icon->set_flag(StandardMaterial3D::FLAG_FIXED_SIZE, true);
		icon->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
		icon->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN);

		if (p_on_top && selected) {
			icon->set_on_top_of_alpha();
		}

		icons.push_back(icon);
	}

	materials[p_name] = icons;
}

void EditorNode3DGizmoPlugin::create_handle_material(const String &p_name, bool p_billboard, const Ref<Texture2D> &p_icon) {
	Ref<StandardMaterial3D> handle_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));

	handle_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	handle_material->set_flag(StandardMaterial3D::FLAG_USE_POINT_SIZE, true);
	Ref<Texture2D> handle_t = p_icon.is_valid() ? p_icon : EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Editor3DHandle"), EditorStringName(EditorIcons));
	handle_material->set_point_size(handle_t->get_width());
	handle_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, handle_t);
	handle_material->set_albedo(Color(1, 1, 1));
	handle_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	handle_material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	handle_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	handle_material->set_on_top_of_alpha();
	if (p_billboard) {
		handle_material->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
		handle_material->set_on_top_of_alpha();
	}
	handle_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);

	materials[p_name] = Vector<Ref<StandardMaterial3D>>();
	materials[p_name].push_back(handle_material);
}

void EditorNode3DGizmoPlugin::add_material(const String &p_name, Ref<StandardMaterial3D> p_material) {
	materials[p_name] = Vector<Ref<StandardMaterial3D>>();
	materials[p_name].push_back(p_material);
}

Ref<StandardMaterial3D> EditorNode3DGizmoPlugin::get_material(const String &p_name, const Ref<EditorNode3DGizmo> &p_gizmo) {
	ERR_FAIL_COND_V(!materials.has(p_name), Ref<StandardMaterial3D>());
	ERR_FAIL_COND_V(materials[p_name].is_empty(), Ref<StandardMaterial3D>());

	if (p_gizmo.is_null() || materials[p_name].size() == 1) {
		return materials[p_name][0];
	}

	int index = (p_gizmo->is_selected() ? 1 : 0) + (p_gizmo->is_editable() ? 2 : 0);

	Ref<StandardMaterial3D> mat = materials[p_name][index];

	bool on_top_mat = mat->get_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST);

	if (!on_top_mat && current_state == ON_TOP && p_gizmo->is_selected()) {
		mat = mat->duplicate();
		mat->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}

	return mat;
}

String EditorNode3DGizmoPlugin::get_gizmo_name() const {
	String ret;
	if (GDVIRTUAL_CALL(_get_gizmo_name, ret)) {
		return ret;
	}

	WARN_PRINT_ONCE("A 3D editor gizmo has no name defined (it will appear as \"Unnamed Gizmo\" in the \"View > Gizmos\" menu). To resolve this, override the `_get_gizmo_name()` function to return a String in the script that extends EditorNode3DGizmoPlugin.");
	return TTR("Unnamed Gizmo");
}

int EditorNode3DGizmoPlugin::get_priority() const {
	int ret;
	if (GDVIRTUAL_CALL(_get_priority, ret)) {
		return ret;
	}
	return 0;
}

Ref<EditorNode3DGizmo> EditorNode3DGizmoPlugin::get_gizmo(Node3D *p_spatial) {
	if (get_script_instance() && get_script_instance()->has_method("_get_gizmo")) {
		return get_script_instance()->call("_get_gizmo", p_spatial);
	}

	Ref<EditorNode3DGizmo> ref = create_gizmo(p_spatial);

	if (ref.is_null()) {
		return ref;
	}

	ref->set_plugin(this);
	ref->set_node_3d(p_spatial);
	ref->set_hidden(current_state == HIDDEN);

	current_gizmos.insert(ref.ptr());
	return ref;
}

void EditorNode3DGizmoPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_material", "name", "color", "billboard", "on_top", "use_vertex_color"), &EditorNode3DGizmoPlugin::create_material, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("create_icon_material", "name", "texture", "on_top", "color"), &EditorNode3DGizmoPlugin::create_icon_material, DEFVAL(false), DEFVAL(Color(1, 1, 1, 1)));
	ClassDB::bind_method(D_METHOD("create_handle_material", "name", "billboard", "texture"), &EditorNode3DGizmoPlugin::create_handle_material, DEFVAL(false), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("add_material", "name", "material"), &EditorNode3DGizmoPlugin::add_material);

	ClassDB::bind_method(D_METHOD("get_material", "name", "gizmo"), &EditorNode3DGizmoPlugin::get_material, DEFVAL(Ref<EditorNode3DGizmo>()));

	GDVIRTUAL_BIND(_has_gizmo, "for_node_3d");
	GDVIRTUAL_BIND(_create_gizmo, "for_node_3d");

	GDVIRTUAL_BIND(_get_gizmo_name);
	GDVIRTUAL_BIND(_get_priority);
	GDVIRTUAL_BIND(_can_be_hidden);
	GDVIRTUAL_BIND(_is_selectable_when_hidden);

	GDVIRTUAL_BIND(_redraw, "gizmo");
	GDVIRTUAL_BIND(_get_handle_name, "gizmo", "handle_id", "secondary");
	GDVIRTUAL_BIND(_is_handle_highlighted, "gizmo", "handle_id", "secondary");
	GDVIRTUAL_BIND(_get_handle_value, "gizmo", "handle_id", "secondary");

	GDVIRTUAL_BIND(_begin_handle_action, "gizmo", "handle_id", "secondary");
	GDVIRTUAL_BIND(_set_handle, "gizmo", "handle_id", "secondary", "camera", "screen_pos");
	GDVIRTUAL_BIND(_commit_handle, "gizmo", "handle_id", "secondary", "restore", "cancel");

	GDVIRTUAL_BIND(_subgizmos_intersect_ray, "gizmo", "camera", "screen_pos");
	GDVIRTUAL_BIND(_subgizmos_intersect_frustum, "gizmo", "camera", "frustum_planes");
	GDVIRTUAL_BIND(_get_subgizmo_transform, "gizmo", "subgizmo_id");
	GDVIRTUAL_BIND(_set_subgizmo_transform, "gizmo", "subgizmo_id", "transform");
	GDVIRTUAL_BIND(_commit_subgizmos, "gizmo", "ids", "restores", "cancel");
}

bool EditorNode3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	bool success = false;
	GDVIRTUAL_CALL(_has_gizmo, p_spatial, success);
	return success;
}

Ref<EditorNode3DGizmo> EditorNode3DGizmoPlugin::create_gizmo(Node3D *p_spatial) {
	Ref<EditorNode3DGizmo> ret;
	if (GDVIRTUAL_CALL(_create_gizmo, p_spatial, ret)) {
		return ret;
	}

	Ref<EditorNode3DGizmo> ref;
	if (has_gizmo(p_spatial)) {
		ref.instantiate();
	}
	return ref;
}

bool EditorNode3DGizmoPlugin::can_be_hidden() const {
	bool ret = true;
	GDVIRTUAL_CALL(_can_be_hidden, ret);
	return ret;
}

bool EditorNode3DGizmoPlugin::is_selectable_when_hidden() const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_selectable_when_hidden, ret);
	return ret;
}

void EditorNode3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	GDVIRTUAL_CALL(_redraw, p_gizmo);
}

bool EditorNode3DGizmoPlugin::is_handle_highlighted(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_handle_highlighted, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary, ret);
	return ret;
}

String EditorNode3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	String ret;
	GDVIRTUAL_CALL(_get_handle_name, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary, ret);
	return ret;
}

Variant EditorNode3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	Variant ret;
	GDVIRTUAL_CALL(_get_handle_value, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary, ret);
	return ret;
}

void EditorNode3DGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	GDVIRTUAL_CALL(_begin_handle_action, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary);
}

void EditorNode3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	GDVIRTUAL_CALL(_set_handle, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary, p_camera, p_point);
}

void EditorNode3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	GDVIRTUAL_CALL(_commit_handle, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_secondary, p_restore, p_cancel);
}

int EditorNode3DGizmoPlugin::subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const {
	int ret = -1;
	GDVIRTUAL_CALL(_subgizmos_intersect_ray, Ref<EditorNode3DGizmo>(p_gizmo), p_camera, p_point, ret);
	return ret;
}

Vector<int> EditorNode3DGizmoPlugin::subgizmos_intersect_frustum(const EditorNode3DGizmo *p_gizmo, const Camera3D *p_camera, const Vector<Plane> &p_frustum) const {
	TypedArray<Plane> frustum;
	frustum.resize(p_frustum.size());
	for (int i = 0; i < p_frustum.size(); i++) {
		frustum[i] = p_frustum[i];
	}
	Vector<int> ret;
	GDVIRTUAL_CALL(_subgizmos_intersect_frustum, Ref<EditorNode3DGizmo>(p_gizmo), p_camera, frustum, ret);
	return ret;
}

Transform3D EditorNode3DGizmoPlugin::get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const {
	Transform3D ret;
	GDVIRTUAL_CALL(_get_subgizmo_transform, Ref<EditorNode3DGizmo>(p_gizmo), p_id, ret);
	return ret;
}

void EditorNode3DGizmoPlugin::set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform) {
	GDVIRTUAL_CALL(_set_subgizmo_transform, Ref<EditorNode3DGizmo>(p_gizmo), p_id, p_transform);
}

void EditorNode3DGizmoPlugin::commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) {
	TypedArray<Transform3D> restore;
	restore.resize(p_restore.size());
	for (int i = 0; i < p_restore.size(); i++) {
		restore[i] = p_restore[i];
	}

	GDVIRTUAL_CALL(_commit_subgizmos, Ref<EditorNode3DGizmo>(p_gizmo), p_ids, restore, p_cancel);
}

void EditorNode3DGizmoPlugin::set_state(int p_state) {
	current_state = p_state;
	for (EditorNode3DGizmo *current : current_gizmos) {
		current->set_hidden(current_state == HIDDEN);
	}
}

int EditorNode3DGizmoPlugin::get_state() const {
	return current_state;
}

void EditorNode3DGizmoPlugin::unregister_gizmo(EditorNode3DGizmo *p_gizmo) {
	current_gizmos.erase(p_gizmo);
}

EditorNode3DGizmoPlugin::EditorNode3DGizmoPlugin() {
	current_state = VISIBLE;
}

EditorNode3DGizmoPlugin::~EditorNode3DGizmoPlugin() {
	for (EditorNode3DGizmo *current : current_gizmos) {
		current->set_plugin(nullptr);
		current->get_node_3d()->remove_gizmo(current);
	}
	if (Node3DEditor::get_singleton()) {
		Node3DEditor::get_singleton()->update_all_gizmos();
	}
}

//////
