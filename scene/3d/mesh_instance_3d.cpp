/**************************************************************************/
/*  mesh_instance_3d.cpp                                                  */
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

#include "mesh_instance_3d.h"

#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"

bool MeshInstance3D::_set(const StringName &p_name, const Variant &p_value) {
	//this is not _too_ bad performance wise, really. it only arrives here if the property was not set anywhere else.
	//add to it that it's probably found on first call to _set anyway.

	if (!get_instance().is_valid()) {
		return false;
	}

	HashMap<StringName, int>::Iterator E = blend_shape_properties.find(p_name);
	if (E) {
		set_blend_shape_value(E->value, p_value);
		return true;
	}

	if (p_name.operator String().begins_with("surface_material_override/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();

		if (idx >= surface_override_materials.size() || idx < 0) {
			return false;
		}

		set_surface_override_material(idx, p_value);
		return true;
	}

	return false;
}

bool MeshInstance3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (!get_instance().is_valid()) {
		return false;
	}

	HashMap<StringName, int>::ConstIterator E = blend_shape_properties.find(p_name);
	if (E) {
		r_ret = get_blend_shape_value(E->value);
		return true;
	}

	if (p_name.operator String().begins_with("surface_material_override/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx >= surface_override_materials.size() || idx < 0) {
			return false;
		}
		r_ret = surface_override_materials[idx];
		return true;
	}
	return false;
}

void MeshInstance3D::_get_property_list(List<PropertyInfo> *p_list) const {
	for (uint32_t i = 0; i < blend_shape_tracks.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::FLOAT, vformat("blend_shapes/%s", String(mesh->get_blend_shape_name(i))), PROPERTY_HINT_RANGE, "-1,1,0.00001"));
	}
	if (mesh.is_valid()) {
		for (int i = 0; i < mesh->get_surface_count(); i++) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, vformat("%s/%d", PNAME("surface_material_override"), i), PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial", PROPERTY_USAGE_DEFAULT));
		}
	}
}

void MeshInstance3D::set_mesh(const Ref<Mesh> &p_mesh) {
	if (mesh == p_mesh) {
		return;
	}

	if (mesh.is_valid()) {
		mesh->disconnect_changed(callable_mp(this, &MeshInstance3D::_mesh_changed));
	}

	mesh = p_mesh;

	if (mesh.is_valid()) {
		// If mesh is a PrimitiveMesh, calling get_rid on it can trigger a changed callback
		// so do this before connecting _mesh_changed.
		set_base(mesh->get_rid());
		mesh->connect_changed(callable_mp(this, &MeshInstance3D::_mesh_changed));
		_mesh_changed();
	} else {
		blend_shape_tracks.clear();
		blend_shape_properties.clear();
		set_base(RID());
		update_gizmos();
	}

	notify_property_list_changed();
}

Ref<Mesh> MeshInstance3D::get_mesh() const {
	return mesh;
}

int MeshInstance3D::get_blend_shape_count() const {
	if (mesh.is_null()) {
		return 0;
	}
	return mesh->get_blend_shape_count();
}

int MeshInstance3D::find_blend_shape_by_name(const StringName &p_name) {
	if (mesh.is_null()) {
		return -1;
	}
	for (int i = 0; i < mesh->get_blend_shape_count(); i++) {
		if (mesh->get_blend_shape_name(i) == p_name) {
			return i;
		}
	}
	return -1;
}

float MeshInstance3D::get_blend_shape_value(int p_blend_shape) const {
	ERR_FAIL_COND_V(mesh.is_null(), 0.0);
	ERR_FAIL_INDEX_V(p_blend_shape, (int)blend_shape_tracks.size(), 0);
	return blend_shape_tracks[p_blend_shape];
}

void MeshInstance3D::set_blend_shape_value(int p_blend_shape, float p_value) {
	ERR_FAIL_COND(mesh.is_null());
	ERR_FAIL_INDEX(p_blend_shape, (int)blend_shape_tracks.size());
	blend_shape_tracks[p_blend_shape] = p_value;
	RenderingServer::get_singleton()->instance_set_blend_shape_weight(get_instance(), p_blend_shape, p_value);
}

void MeshInstance3D::_resolve_skeleton_path() {
	Ref<SkinReference> new_skin_reference;

	if (!skeleton_path.is_empty()) {
		Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(get_node(skeleton_path));
		if (skeleton) {
			if (skin_internal.is_null()) {
				new_skin_reference = skeleton->register_skin(skeleton->create_skin_from_rest_transforms());
				//a skin was created for us
				skin_internal = new_skin_reference->get_skin();
				notify_property_list_changed();
			} else {
				new_skin_reference = skeleton->register_skin(skin_internal);
			}
		}
	}

	skin_ref = new_skin_reference;

	if (skin_ref.is_valid()) {
		RenderingServer::get_singleton()->instance_attach_skeleton(get_instance(), skin_ref->get_skeleton());
	} else {
		RenderingServer::get_singleton()->instance_attach_skeleton(get_instance(), RID());
	}
}

void MeshInstance3D::set_skin(const Ref<Skin> &p_skin) {
	skin_internal = p_skin;
	skin = p_skin;
	if (!is_inside_tree()) {
		return;
	}
	_resolve_skeleton_path();
}

Ref<Skin> MeshInstance3D::get_skin() const {
	return skin;
}

Ref<SkinReference> MeshInstance3D::get_skin_reference() const {
	return skin_ref;
}

void MeshInstance3D::set_skeleton_path(const NodePath &p_skeleton) {
	skeleton_path = p_skeleton;
	if (!is_inside_tree()) {
		return;
	}
	_resolve_skeleton_path();
}

NodePath MeshInstance3D::get_skeleton_path() {
	return skeleton_path;
}

AABB MeshInstance3D::get_aabb() const {
	if (mesh.is_valid()) {
		return mesh->get_aabb();
	}

	return AABB();
}

Node *MeshInstance3D::create_trimesh_collision_node() {
	if (mesh.is_null()) {
		return nullptr;
	}

	Ref<ConcavePolygonShape3D> shape = mesh->create_trimesh_shape();
	if (shape.is_null()) {
		return nullptr;
	}

	StaticBody3D *static_body = memnew(StaticBody3D);
	CollisionShape3D *cshape = memnew(CollisionShape3D);
	cshape->set_shape(shape);
	static_body->add_child(cshape, true);
	return static_body;
}

void MeshInstance3D::create_trimesh_collision() {
	StaticBody3D *static_body = Object::cast_to<StaticBody3D>(create_trimesh_collision_node());
	ERR_FAIL_NULL(static_body);
	static_body->set_name(String(get_name()) + "_col");

	add_child(static_body, true);
	if (get_owner()) {
		CollisionShape3D *cshape = Object::cast_to<CollisionShape3D>(static_body->get_child(0));
		static_body->set_owner(get_owner());
		cshape->set_owner(get_owner());
	}
}

Node *MeshInstance3D::create_convex_collision_node(bool p_clean, bool p_simplify) {
	if (mesh.is_null()) {
		return nullptr;
	}

	Ref<ConvexPolygonShape3D> shape = mesh->create_convex_shape(p_clean, p_simplify);
	if (shape.is_null()) {
		return nullptr;
	}

	StaticBody3D *static_body = memnew(StaticBody3D);
	CollisionShape3D *cshape = memnew(CollisionShape3D);
	cshape->set_shape(shape);
	static_body->add_child(cshape, true);
	return static_body;
}

void MeshInstance3D::create_convex_collision(bool p_clean, bool p_simplify) {
	StaticBody3D *static_body = Object::cast_to<StaticBody3D>(create_convex_collision_node(p_clean, p_simplify));
	ERR_FAIL_NULL(static_body);
	static_body->set_name(String(get_name()) + "_col");

	add_child(static_body, true);
	if (get_owner()) {
		CollisionShape3D *cshape = Object::cast_to<CollisionShape3D>(static_body->get_child(0));
		static_body->set_owner(get_owner());
		cshape->set_owner(get_owner());
	}
}

Node *MeshInstance3D::create_multiple_convex_collisions_node(const Ref<MeshConvexDecompositionSettings> &p_settings) {
	if (mesh.is_null()) {
		return nullptr;
	}

	Ref<MeshConvexDecompositionSettings> settings;
	if (p_settings.is_valid()) {
		settings = p_settings;
	} else {
		settings.instantiate();
	}

	Vector<Ref<Shape3D>> shapes = mesh->convex_decompose(settings);
	if (!shapes.size()) {
		return nullptr;
	}

	StaticBody3D *static_body = memnew(StaticBody3D);
	for (int i = 0; i < shapes.size(); i++) {
		CollisionShape3D *cshape = memnew(CollisionShape3D);
		cshape->set_shape(shapes[i]);
		static_body->add_child(cshape, true);
	}
	return static_body;
}

void MeshInstance3D::create_multiple_convex_collisions(const Ref<MeshConvexDecompositionSettings> &p_settings) {
	StaticBody3D *static_body = Object::cast_to<StaticBody3D>(create_multiple_convex_collisions_node(p_settings));
	ERR_FAIL_NULL(static_body);
	static_body->set_name(String(get_name()) + "_col");

	add_child(static_body, true);
	if (get_owner()) {
		static_body->set_owner(get_owner());
		int count = static_body->get_child_count();
		for (int i = 0; i < count; i++) {
			CollisionShape3D *cshape = Object::cast_to<CollisionShape3D>(static_body->get_child(i));
			cshape->set_owner(get_owner());
		}
	}
}

void MeshInstance3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_resolve_skeleton_path();
		} break;
		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (mesh.is_valid()) {
				mesh->notification(NOTIFICATION_TRANSLATION_CHANGED);
			}
		} break;
	}
}

int MeshInstance3D::get_surface_override_material_count() const {
	return surface_override_materials.size();
}

void MeshInstance3D::set_surface_override_material(int p_surface, const Ref<Material> &p_material) {
	ERR_FAIL_INDEX(p_surface, surface_override_materials.size());

	surface_override_materials.write[p_surface] = p_material;

	if (surface_override_materials[p_surface].is_valid()) {
		RS::get_singleton()->instance_set_surface_override_material(get_instance(), p_surface, surface_override_materials[p_surface]->get_rid());
	} else {
		RS::get_singleton()->instance_set_surface_override_material(get_instance(), p_surface, RID());
	}
}

Ref<Material> MeshInstance3D::get_surface_override_material(int p_surface) const {
	ERR_FAIL_INDEX_V(p_surface, surface_override_materials.size(), Ref<Material>());

	return surface_override_materials[p_surface];
}

Ref<Material> MeshInstance3D::get_active_material(int p_surface) const {
	Ref<Material> mat_override = get_material_override();
	if (mat_override.is_valid()) {
		return mat_override;
	}

	Ref<Material> surface_material = get_surface_override_material(p_surface);
	if (surface_material.is_valid()) {
		return surface_material;
	}

	Ref<Mesh> m = get_mesh();
	if (m.is_valid()) {
		return m->surface_get_material(p_surface);
	}

	return Ref<Material>();
}

void MeshInstance3D::_mesh_changed() {
	ERR_FAIL_COND(mesh.is_null());
	const int surface_count = mesh->get_surface_count();

	surface_override_materials.resize(surface_count);

	uint32_t initialize_bs_from = blend_shape_tracks.size();
	blend_shape_tracks.resize(mesh->get_blend_shape_count());

	if (surface_count > 0) {
		for (uint32_t i = 0; i < blend_shape_tracks.size(); i++) {
			blend_shape_properties["blend_shapes/" + String(mesh->get_blend_shape_name(i))] = i;
			if (i < initialize_bs_from) {
				set_blend_shape_value(i, blend_shape_tracks[i]);
			} else {
				set_blend_shape_value(i, 0);
			}
		}
	}

	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		if (surface_override_materials[surface_index].is_valid()) {
			RS::get_singleton()->instance_set_surface_override_material(get_instance(), surface_index, surface_override_materials[surface_index]->get_rid());
		}
	}

	update_gizmos();
}

MeshInstance3D *MeshInstance3D::create_debug_tangents_node() {
	Vector<Vector3> lines;
	Vector<Color> colors;

	Ref<Mesh> m = get_mesh();
	if (m.is_null()) {
		return nullptr;
	}

	for (int i = 0; i < m->get_surface_count(); i++) {
		Array arrays = m->surface_get_arrays(i);
		ERR_CONTINUE(arrays.size() != Mesh::ARRAY_MAX);

		Vector<Vector3> verts = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector3> norms = arrays[Mesh::ARRAY_NORMAL];
		if (norms.size() == 0) {
			continue;
		}
		Vector<float> tangents = arrays[Mesh::ARRAY_TANGENT];
		if (tangents.size() == 0) {
			continue;
		}

		for (int j = 0; j < verts.size(); j++) {
			Vector3 v = verts[j];
			Vector3 n = norms[j];
			Vector3 t = Vector3(tangents[j * 4 + 0], tangents[j * 4 + 1], tangents[j * 4 + 2]);
			Vector3 b = (n.cross(t)).normalized() * tangents[j * 4 + 3];

			lines.push_back(v); //normal
			colors.push_back(Color(0, 0, 1)); //color
			lines.push_back(v + n * 0.04); //normal
			colors.push_back(Color(0, 0, 1)); //color

			lines.push_back(v); //tangent
			colors.push_back(Color(1, 0, 0)); //color
			lines.push_back(v + t * 0.04); //tangent
			colors.push_back(Color(1, 0, 0)); //color

			lines.push_back(v); //binormal
			colors.push_back(Color(0, 1, 0)); //color
			lines.push_back(v + b * 0.04); //binormal
			colors.push_back(Color(0, 1, 0)); //color
		}
	}

	if (lines.size()) {
		Ref<StandardMaterial3D> sm;
		sm.instantiate();

		sm->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		sm->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		sm->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		sm->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

		Ref<ArrayMesh> am;
		am.instantiate();
		Array a;
		a.resize(Mesh::ARRAY_MAX);
		a[Mesh::ARRAY_VERTEX] = lines;
		a[Mesh::ARRAY_COLOR] = colors;

		am->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, a);
		am->surface_set_material(0, sm);

		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(am);
		mi->set_name("DebugTangents");
		return mi;
	}

	return nullptr;
}

void MeshInstance3D::create_debug_tangents() {
	MeshInstance3D *mi = create_debug_tangents_node();
	if (!mi) {
		return;
	}

	add_child(mi, true);
	if (is_inside_tree() && this == get_tree()->get_edited_scene_root()) {
		mi->set_owner(this);
	} else {
		mi->set_owner(get_owner());
	}
}

bool MeshInstance3D::_property_can_revert(const StringName &p_name) const {
	HashMap<StringName, int>::ConstIterator E = blend_shape_properties.find(p_name);
	if (E) {
		return true;
	}
	return false;
}

bool MeshInstance3D::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	HashMap<StringName, int>::ConstIterator E = blend_shape_properties.find(p_name);
	if (E) {
		r_property = 0.0f;
		return true;
	}
	return false;
}

Ref<ArrayMesh> MeshInstance3D::bake_mesh_from_current_blend_shape_mix(Ref<ArrayMesh> p_existing) {
	Ref<ArrayMesh> source_mesh = get_mesh();
	ERR_FAIL_COND_V_MSG(source_mesh.is_null(), Ref<ArrayMesh>(), "The source mesh must be a valid ArrayMesh.");

	Ref<ArrayMesh> bake_mesh;

	if (p_existing.is_valid()) {
		ERR_FAIL_COND_V_MSG(p_existing.is_null(), Ref<ArrayMesh>(), "The existing mesh must be a valid ArrayMesh.");
		ERR_FAIL_COND_V_MSG(source_mesh == p_existing, Ref<ArrayMesh>(), "The source mesh can not be the same mesh as the existing mesh.");

		bake_mesh = p_existing;
	} else {
		bake_mesh.instantiate();
	}

	Mesh::BlendShapeMode blend_shape_mode = source_mesh->get_blend_shape_mode();
	int mesh_surface_count = source_mesh->get_surface_count();

	bake_mesh->clear_surfaces();
	bake_mesh->set_blend_shape_mode(blend_shape_mode);

	for (int surface_index = 0; surface_index < mesh_surface_count; surface_index++) {
		uint32_t surface_format = source_mesh->surface_get_format(surface_index);

		ERR_CONTINUE(0 == (surface_format & Mesh::ARRAY_FORMAT_VERTEX));

		const Array &source_mesh_arrays = source_mesh->surface_get_arrays(surface_index);

		ERR_FAIL_COND_V(source_mesh_arrays.size() != RS::ARRAY_MAX, Ref<ArrayMesh>());

		const Vector<Vector3> &source_mesh_vertex_array = source_mesh_arrays[Mesh::ARRAY_VERTEX];
		const Vector<Vector3> &source_mesh_normal_array = source_mesh_arrays[Mesh::ARRAY_NORMAL];
		const Vector<float> &source_mesh_tangent_array = source_mesh_arrays[Mesh::ARRAY_TANGENT];

		Array new_mesh_arrays;
		new_mesh_arrays.resize(Mesh::ARRAY_MAX);
		for (int i = 0; i < source_mesh_arrays.size(); i++) {
			if (i == Mesh::ARRAY_VERTEX || i == Mesh::ARRAY_NORMAL || i == Mesh::ARRAY_TANGENT) {
				continue;
			}
			new_mesh_arrays[i] = source_mesh_arrays[i];
		}

		bool use_normal_array = source_mesh_normal_array.size() == source_mesh_vertex_array.size();
		bool use_tangent_array = source_mesh_tangent_array.size() / 4 == source_mesh_vertex_array.size();

		Vector<Vector3> lerped_vertex_array = source_mesh_vertex_array;
		Vector<Vector3> lerped_normal_array = source_mesh_normal_array;
		Vector<float> lerped_tangent_array = source_mesh_tangent_array;

		const Vector3 *source_vertices_ptr = source_mesh_vertex_array.ptr();
		const Vector3 *source_normals_ptr = source_mesh_normal_array.ptr();
		const float *source_tangents_ptr = source_mesh_tangent_array.ptr();

		Vector3 *lerped_vertices_ptrw = lerped_vertex_array.ptrw();
		Vector3 *lerped_normals_ptrw = lerped_normal_array.ptrw();
		float *lerped_tangents_ptrw = lerped_tangent_array.ptrw();

		const Array &blendshapes_mesh_arrays = source_mesh->surface_get_blend_shape_arrays(surface_index);
		int blend_shape_count = source_mesh->get_blend_shape_count();
		ERR_FAIL_COND_V(blendshapes_mesh_arrays.size() != blend_shape_count, Ref<ArrayMesh>());

		for (int blendshape_index = 0; blendshape_index < blend_shape_count; blendshape_index++) {
			float blend_weight = get_blend_shape_value(blendshape_index);
			if (abs(blend_weight) <= 0.0001) {
				continue;
			}

			const Array &blendshape_mesh_arrays = blendshapes_mesh_arrays[blendshape_index];

			const Vector<Vector3> &blendshape_vertex_array = blendshape_mesh_arrays[Mesh::ARRAY_VERTEX];
			const Vector<Vector3> &blendshape_normal_array = blendshape_mesh_arrays[Mesh::ARRAY_NORMAL];
			const Vector<float> &blendshape_tangent_array = blendshape_mesh_arrays[Mesh::ARRAY_TANGENT];

			ERR_FAIL_COND_V(source_mesh_vertex_array.size() != blendshape_vertex_array.size(), Ref<ArrayMesh>());
			ERR_FAIL_COND_V(source_mesh_normal_array.size() != blendshape_normal_array.size(), Ref<ArrayMesh>());
			ERR_FAIL_COND_V(source_mesh_tangent_array.size() != blendshape_tangent_array.size(), Ref<ArrayMesh>());

			const Vector3 *blendshape_vertices_ptr = blendshape_vertex_array.ptr();
			const Vector3 *blendshape_normals_ptr = blendshape_normal_array.ptr();
			const float *blendshape_tangents_ptr = blendshape_tangent_array.ptr();

			if (blend_shape_mode == Mesh::BLEND_SHAPE_MODE_NORMALIZED) {
				for (int i = 0; i < source_mesh_vertex_array.size(); i++) {
					const Vector3 &source_vertex = source_vertices_ptr[i];
					const Vector3 &blendshape_vertex = blendshape_vertices_ptr[i];
					Vector3 lerped_vertex = source_vertex.lerp(blendshape_vertex, blend_weight) - source_vertex;
					lerped_vertices_ptrw[i] += lerped_vertex;

					if (use_normal_array) {
						const Vector3 &source_normal = source_normals_ptr[i];
						const Vector3 &blendshape_normal = blendshape_normals_ptr[i];
						Vector3 lerped_normal = source_normal.lerp(blendshape_normal, blend_weight) - source_normal;
						lerped_normals_ptrw[i] += lerped_normal;
					}

					if (use_tangent_array) {
						int tangent_index = i * 4;
						const Vector4 source_tangent = Vector4(
								source_tangents_ptr[tangent_index],
								source_tangents_ptr[tangent_index + 1],
								source_tangents_ptr[tangent_index + 2],
								source_tangents_ptr[tangent_index + 3]);
						const Vector4 blendshape_tangent = Vector4(
								blendshape_tangents_ptr[tangent_index],
								blendshape_tangents_ptr[tangent_index + 1],
								blendshape_tangents_ptr[tangent_index + 2],
								blendshape_tangents_ptr[tangent_index + 3]);
						Vector4 lerped_tangent = source_tangent.lerp(blendshape_tangent, blend_weight);
						lerped_tangents_ptrw[tangent_index] += lerped_tangent.x;
						lerped_tangents_ptrw[tangent_index + 1] += lerped_tangent.y;
						lerped_tangents_ptrw[tangent_index + 2] += lerped_tangent.z;
						lerped_tangents_ptrw[tangent_index + 3] += lerped_tangent.w;
					}
				}
			} else if (blend_shape_mode == Mesh::BLEND_SHAPE_MODE_RELATIVE) {
				for (int i = 0; i < source_mesh_vertex_array.size(); i++) {
					const Vector3 &blendshape_vertex = blendshape_vertices_ptr[i];
					lerped_vertices_ptrw[i] += blendshape_vertex * blend_weight;

					if (use_normal_array) {
						const Vector3 &blendshape_normal = blendshape_normals_ptr[i];
						lerped_normals_ptrw[i] += blendshape_normal * blend_weight;
					}

					if (use_tangent_array) {
						int tangent_index = i * 4;
						const Vector4 blendshape_tangent = Vector4(
								blendshape_tangents_ptr[tangent_index],
								blendshape_tangents_ptr[tangent_index + 1],
								blendshape_tangents_ptr[tangent_index + 2],
								blendshape_tangents_ptr[tangent_index + 3]);
						Vector4 lerped_tangent = blendshape_tangent * blend_weight;
						lerped_tangents_ptrw[tangent_index] += lerped_tangent.x;
						lerped_tangents_ptrw[tangent_index + 1] += lerped_tangent.y;
						lerped_tangents_ptrw[tangent_index + 2] += lerped_tangent.z;
						lerped_tangents_ptrw[tangent_index + 3] += lerped_tangent.w;
					}
				}
			}
		}

		new_mesh_arrays[Mesh::ARRAY_VERTEX] = lerped_vertex_array;
		if (use_normal_array) {
			new_mesh_arrays[Mesh::ARRAY_NORMAL] = lerped_normal_array;
		}
		if (use_tangent_array) {
			new_mesh_arrays[Mesh::ARRAY_TANGENT] = lerped_tangent_array;
		}

		bake_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, new_mesh_arrays, Array(), Dictionary(), surface_format);
	}

	return bake_mesh;
}

Ref<ArrayMesh> MeshInstance3D::bake_mesh_from_current_skeleton_pose(Ref<ArrayMesh> p_existing) {
	Ref<ArrayMesh> source_mesh = get_mesh();
	ERR_FAIL_COND_V_MSG(source_mesh.is_null(), Ref<ArrayMesh>(), "The source mesh must be a valid ArrayMesh.");

	Ref<ArrayMesh> bake_mesh;

	if (p_existing.is_valid()) {
		ERR_FAIL_COND_V_MSG(source_mesh == p_existing, Ref<ArrayMesh>(), "The source mesh can not be the same mesh as the existing mesh.");

		bake_mesh = p_existing;
	} else {
		bake_mesh.instantiate();
	}

	ERR_FAIL_COND_V_MSG(skin_ref.is_null(), Ref<ArrayMesh>(), "The source mesh must have a valid skin.");
	ERR_FAIL_COND_V_MSG(skin_internal.is_null(), Ref<ArrayMesh>(), "The source mesh must have a valid skin.");
	RID skeleton = skin_ref->get_skeleton();
	ERR_FAIL_COND_V_MSG(!skeleton.is_valid(), Ref<ArrayMesh>(), "The source mesh must have its skin registered with a valid skeleton.");

	const int bone_count = RenderingServer::get_singleton()->skeleton_get_bone_count(skeleton);
	ERR_FAIL_COND_V(bone_count <= 0, Ref<ArrayMesh>());
	ERR_FAIL_COND_V(bone_count < skin_internal->get_bind_count(), Ref<ArrayMesh>());

	LocalVector<Transform3D> bone_transforms;
	bone_transforms.resize(bone_count);
	for (int bone_index = 0; bone_index < bone_count; bone_index++) {
		bone_transforms[bone_index] = RenderingServer::get_singleton()->skeleton_bone_get_transform(skeleton, bone_index);
	}

	bake_mesh->clear_surfaces();

	int mesh_surface_count = source_mesh->get_surface_count();

	for (int surface_index = 0; surface_index < mesh_surface_count; surface_index++) {
		ERR_CONTINUE(source_mesh->surface_get_primitive_type(surface_index) != Mesh::PRIMITIVE_TRIANGLES);

		uint32_t surface_format = source_mesh->surface_get_format(surface_index);

		ERR_CONTINUE(0 == (surface_format & Mesh::ARRAY_FORMAT_VERTEX));
		ERR_CONTINUE(0 == (surface_format & Mesh::ARRAY_FORMAT_BONES));
		ERR_CONTINUE(0 == (surface_format & Mesh::ARRAY_FORMAT_WEIGHTS));

		unsigned int bones_per_vertex = surface_format & Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS ? 8 : 4;

		surface_format &= ~Mesh::ARRAY_FORMAT_BONES;
		surface_format &= ~Mesh::ARRAY_FORMAT_WEIGHTS;

		const Array &source_mesh_arrays = source_mesh->surface_get_arrays(surface_index);

		ERR_FAIL_COND_V(source_mesh_arrays.size() != RS::ARRAY_MAX, Ref<ArrayMesh>());

		const Vector<Vector3> &source_mesh_vertex_array = source_mesh_arrays[Mesh::ARRAY_VERTEX];
		const Vector<Vector3> &source_mesh_normal_array = source_mesh_arrays[Mesh::ARRAY_NORMAL];
		const Vector<float> &source_mesh_tangent_array = source_mesh_arrays[Mesh::ARRAY_TANGENT];
		const Vector<int> &source_mesh_bones_array = source_mesh_arrays[Mesh::ARRAY_BONES];
		const Vector<float> &source_mesh_weights_array = source_mesh_arrays[Mesh::ARRAY_WEIGHTS];

		unsigned int vertex_count = source_mesh_vertex_array.size();
		int expected_bone_array_size = vertex_count * bones_per_vertex;
		ERR_CONTINUE(source_mesh_bones_array.size() != expected_bone_array_size);
		ERR_CONTINUE(source_mesh_weights_array.size() != expected_bone_array_size);

		Array new_mesh_arrays;
		new_mesh_arrays.resize(Mesh::ARRAY_MAX);
		for (int i = 0; i < source_mesh_arrays.size(); i++) {
			if (i == Mesh::ARRAY_VERTEX || i == Mesh::ARRAY_NORMAL || i == Mesh::ARRAY_TANGENT || i == Mesh::ARRAY_BONES || i == Mesh::ARRAY_WEIGHTS) {
				continue;
			}
			new_mesh_arrays[i] = source_mesh_arrays[i];
		}

		bool use_normal_array = source_mesh_normal_array.size() == source_mesh_vertex_array.size();
		bool use_tangent_array = source_mesh_tangent_array.size() / 4 == source_mesh_vertex_array.size();

		Vector<Vector3> lerped_vertex_array = source_mesh_vertex_array;
		Vector<Vector3> lerped_normal_array = source_mesh_normal_array;
		Vector<float> lerped_tangent_array = source_mesh_tangent_array;

		const Vector3 *source_vertices_ptr = source_mesh_vertex_array.ptr();
		const Vector3 *source_normals_ptr = source_mesh_normal_array.ptr();
		const float *source_tangents_ptr = source_mesh_tangent_array.ptr();
		const int *source_bones_ptr = source_mesh_bones_array.ptr();
		const float *source_weights_ptr = source_mesh_weights_array.ptr();

		Vector3 *lerped_vertices_ptrw = lerped_vertex_array.ptrw();
		Vector3 *lerped_normals_ptrw = lerped_normal_array.ptrw();
		float *lerped_tangents_ptrw = lerped_tangent_array.ptrw();

		for (unsigned int vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
			Vector3 lerped_vertex;
			Vector3 lerped_normal;
			Vector3 lerped_tangent;

			const Vector3 &source_vertex = source_vertices_ptr[vertex_index];

			Vector3 source_normal;
			if (use_normal_array) {
				source_normal = source_normals_ptr[vertex_index];
			}

			int tangent_index = vertex_index * 4;
			Vector4 source_tangent;
			Vector3 source_tangent_vec3;
			if (use_tangent_array) {
				source_tangent = Vector4(
						source_tangents_ptr[tangent_index],
						source_tangents_ptr[tangent_index + 1],
						source_tangents_ptr[tangent_index + 2],
						source_tangents_ptr[tangent_index + 3]);

				DEV_ASSERT(source_tangent.w == 1.0 || source_tangent.w == -1.0);

				source_tangent_vec3 = Vector3(source_tangent.x, source_tangent.y, source_tangent.z);
			}

			for (unsigned int weight_index = 0; weight_index < bones_per_vertex; weight_index++) {
				float bone_weight = source_weights_ptr[vertex_index * bones_per_vertex + weight_index];
				if (bone_weight < FLT_EPSILON) {
					continue;
				}
				int vertex_bone_index = source_bones_ptr[vertex_index * bones_per_vertex + weight_index];
				const Transform3D &bone_transform = bone_transforms[vertex_bone_index];
				const Basis bone_basis = bone_transform.basis.orthonormalized();

				ERR_FAIL_INDEX_V(vertex_bone_index, static_cast<int>(bone_transforms.size()), Ref<ArrayMesh>());

				lerped_vertex += source_vertex.lerp(bone_transform.xform(source_vertex), bone_weight) - source_vertex;
				;

				if (use_normal_array) {
					lerped_normal += source_normal.lerp(bone_basis.xform(source_normal), bone_weight) - source_normal;
				}

				if (use_tangent_array) {
					lerped_tangent += source_tangent_vec3.lerp(bone_basis.xform(source_tangent_vec3), bone_weight) - source_tangent_vec3;
				}
			}

			lerped_vertices_ptrw[vertex_index] += lerped_vertex;

			if (use_normal_array) {
				lerped_normals_ptrw[vertex_index] = (source_normal + lerped_normal).normalized();
			}

			if (use_tangent_array) {
				lerped_tangent = (source_tangent_vec3 + lerped_tangent).normalized();
				lerped_tangents_ptrw[tangent_index] = lerped_tangent.x;
				lerped_tangents_ptrw[tangent_index + 1] = lerped_tangent.y;
				lerped_tangents_ptrw[tangent_index + 2] = lerped_tangent.z;
			}
		}

		new_mesh_arrays[Mesh::ARRAY_VERTEX] = lerped_vertex_array;
		if (use_normal_array) {
			new_mesh_arrays[Mesh::ARRAY_NORMAL] = lerped_normal_array;
		}
		if (use_tangent_array) {
			new_mesh_arrays[Mesh::ARRAY_TANGENT] = lerped_tangent_array;
		}

		bake_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, new_mesh_arrays, Array(), Dictionary(), surface_format);
	}

	return bake_mesh;
}

void MeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &MeshInstance3D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &MeshInstance3D::get_mesh);
	ClassDB::bind_method(D_METHOD("set_skeleton_path", "skeleton_path"), &MeshInstance3D::set_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_skeleton_path"), &MeshInstance3D::get_skeleton_path);
	ClassDB::bind_method(D_METHOD("set_skin", "skin"), &MeshInstance3D::set_skin);
	ClassDB::bind_method(D_METHOD("get_skin"), &MeshInstance3D::get_skin);
	ClassDB::bind_method(D_METHOD("get_skin_reference"), &MeshInstance3D::get_skin_reference);

	ClassDB::bind_method(D_METHOD("get_surface_override_material_count"), &MeshInstance3D::get_surface_override_material_count);
	ClassDB::bind_method(D_METHOD("set_surface_override_material", "surface", "material"), &MeshInstance3D::set_surface_override_material);
	ClassDB::bind_method(D_METHOD("get_surface_override_material", "surface"), &MeshInstance3D::get_surface_override_material);
	ClassDB::bind_method(D_METHOD("get_active_material", "surface"), &MeshInstance3D::get_active_material);

	ClassDB::bind_method(D_METHOD("create_trimesh_collision"), &MeshInstance3D::create_trimesh_collision);
	ClassDB::set_method_flags("MeshInstance3D", "create_trimesh_collision", METHOD_FLAGS_DEFAULT);
	ClassDB::bind_method(D_METHOD("create_convex_collision", "clean", "simplify"), &MeshInstance3D::create_convex_collision, DEFVAL(true), DEFVAL(false));
	ClassDB::set_method_flags("MeshInstance3D", "create_convex_collision", METHOD_FLAGS_DEFAULT);
	ClassDB::bind_method(D_METHOD("create_multiple_convex_collisions", "settings"), &MeshInstance3D::create_multiple_convex_collisions, DEFVAL(Ref<MeshConvexDecompositionSettings>()));
	ClassDB::set_method_flags("MeshInstance3D", "create_multiple_convex_collisions", METHOD_FLAGS_DEFAULT);

	ClassDB::bind_method(D_METHOD("get_blend_shape_count"), &MeshInstance3D::get_blend_shape_count);
	ClassDB::bind_method(D_METHOD("find_blend_shape_by_name", "name"), &MeshInstance3D::find_blend_shape_by_name);
	ClassDB::bind_method(D_METHOD("get_blend_shape_value", "blend_shape_idx"), &MeshInstance3D::get_blend_shape_value);
	ClassDB::bind_method(D_METHOD("set_blend_shape_value", "blend_shape_idx", "value"), &MeshInstance3D::set_blend_shape_value);

	ClassDB::bind_method(D_METHOD("create_debug_tangents"), &MeshInstance3D::create_debug_tangents);

	ClassDB::bind_method(D_METHOD("bake_mesh_from_current_blend_shape_mix", "existing"), &MeshInstance3D::bake_mesh_from_current_blend_shape_mix, DEFVAL(Ref<ArrayMesh>()));
	ClassDB::bind_method(D_METHOD("bake_mesh_from_current_skeleton_pose", "existing"), &MeshInstance3D::bake_mesh_from_current_skeleton_pose, DEFVAL(Ref<ArrayMesh>()));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ADD_GROUP("Skeleton", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "skin", PROPERTY_HINT_RESOURCE_TYPE, "Skin"), "set_skin", "get_skin");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton3D"), "set_skeleton_path", "get_skeleton_path");
	ADD_GROUP("", "");
}

MeshInstance3D::MeshInstance3D() {
}

MeshInstance3D::~MeshInstance3D() {
}
