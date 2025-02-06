/**************************************************************************/
/*  occluder_instance_3d.cpp                                              */
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

#include "occluder_instance_3d.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"
#include "core/math/geometry_2d.h"
#include "core/math/triangulate.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/surface_tool.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif

RID Occluder3D::get_rid() const {
	return occluder;
}

void Occluder3D::_update() {
	_update_arrays(vertices, indices);

	aabb = AABB();

	const Vector3 *ptr = vertices.ptr();
	for (int i = 0; i < vertices.size(); i++) {
		aabb.expand_to(ptr[i]);
	}

	debug_lines.clear();
	debug_mesh.unref();

	RS::get_singleton()->occluder_set_mesh(occluder, vertices, indices);
	emit_changed();
}

PackedVector3Array Occluder3D::get_vertices() const {
	return vertices;
}

PackedInt32Array Occluder3D::get_indices() const {
	return indices;
}

Vector<Vector3> Occluder3D::get_debug_lines() const {
	if (!debug_lines.is_empty()) {
		return debug_lines;
	}

	if (indices.size() % 3 != 0) {
		return Vector<Vector3>();
	}

	const Vector3 *vertices_ptr = vertices.ptr();
	debug_lines.resize(indices.size() / 3 * 6);
	Vector3 *line_ptr = debug_lines.ptrw();
	int line_i = 0;
	for (int i = 0; i < indices.size() / 3; i++) {
		for (int j = 0; j < 3; j++) {
			int a = indices[i * 3 + j];
			int b = indices[i * 3 + (j + 1) % 3];
			ERR_FAIL_INDEX_V_MSG(a, vertices.size(), Vector<Vector3>(), "Occluder indices are out of range.");
			ERR_FAIL_INDEX_V_MSG(b, vertices.size(), Vector<Vector3>(), "Occluder indices are out of range.");
			line_ptr[line_i++] = vertices_ptr[a];
			line_ptr[line_i++] = vertices_ptr[b];
		}
	}
	return debug_lines;
}

Ref<ArrayMesh> Occluder3D::get_debug_mesh() const {
	if (debug_mesh.is_valid()) {
		return debug_mesh;
	}

	if (vertices.is_empty() || indices.is_empty() || indices.size() % 3 != 0) {
		return debug_mesh;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;

	debug_mesh.instantiate();
	debug_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return debug_mesh;
}

AABB Occluder3D::get_aabb() const {
	return aabb;
}

void Occluder3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_update();
		} break;
	}
}

Occluder3D::Occluder3D() {
	occluder = RS::get_singleton()->occluder_create();
}

Occluder3D::~Occluder3D() {
	if (occluder.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RS::get_singleton()->free(occluder);
	}
}

/////////////////////////////////////////////////

void ArrayOccluder3D::set_arrays(PackedVector3Array p_vertices, PackedInt32Array p_indices) {
	vertices = p_vertices;
	indices = p_indices;
	_update();
}

void ArrayOccluder3D::set_vertices(PackedVector3Array p_vertices) {
	vertices = p_vertices;
	_update();
}

void ArrayOccluder3D::set_indices(PackedInt32Array p_indices) {
	indices = p_indices;
	_update();
}

void ArrayOccluder3D::_update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	r_vertices = vertices;
	r_indices = indices;
}

void ArrayOccluder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_arrays", "vertices", "indices"), &ArrayOccluder3D::set_arrays);

	ClassDB::bind_method(D_METHOD("set_vertices", "vertices"), &ArrayOccluder3D::set_vertices);
	ClassDB::bind_method(D_METHOD("get_vertices"), &ArrayOccluder3D::get_vertices);

	ClassDB::bind_method(D_METHOD("set_indices", "indices"), &ArrayOccluder3D::set_indices);
	ClassDB::bind_method(D_METHOD("get_indices"), &ArrayOccluder3D::get_indices);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "vertices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_vertices", "get_vertices");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "indices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_indices", "get_indices");
}

ArrayOccluder3D::ArrayOccluder3D() {
}

ArrayOccluder3D::~ArrayOccluder3D() {
}

/////////////////////////////////////////////////

void QuadOccluder3D::set_size(const Size2 &p_size) {
	if (size == p_size) {
		return;
	}

	size = p_size.maxf(0);
	_update();
}

Size2 QuadOccluder3D::get_size() const {
	return size;
}

void QuadOccluder3D::_update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	Size2 _size = Size2(size.x / 2.0f, size.y / 2.0f);

	r_vertices = {
		Vector3(-_size.x, -_size.y, 0),
		Vector3(-_size.x, _size.y, 0),
		Vector3(_size.x, _size.y, 0),
		Vector3(_size.x, -_size.y, 0),
	};

	r_indices = {
		0, 1, 2,
		0, 2, 3
	};
}

void QuadOccluder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &QuadOccluder3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &QuadOccluder3D::get_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
}

QuadOccluder3D::QuadOccluder3D() {
}

QuadOccluder3D::~QuadOccluder3D() {
}

/////////////////////////////////////////////////

void BoxOccluder3D::set_size(const Vector3 &p_size) {
	if (size == p_size) {
		return;
	}

	size = p_size.maxf(0);
	_update();
}

Vector3 BoxOccluder3D::get_size() const {
	return size;
}

void BoxOccluder3D::_update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	Vector3 _size = Vector3(size.x / 2.0f, size.y / 2.0f, size.z / 2.0f);

	r_vertices = {
		// front
		Vector3(-_size.x, -_size.y, _size.z),
		Vector3(_size.x, -_size.y, _size.z),
		Vector3(_size.x, _size.y, _size.z),
		Vector3(-_size.x, _size.y, _size.z),
		// back
		Vector3(-_size.x, -_size.y, -_size.z),
		Vector3(_size.x, -_size.y, -_size.z),
		Vector3(_size.x, _size.y, -_size.z),
		Vector3(-_size.x, _size.y, -_size.z),
	};

	r_indices = {
		// front
		0, 1, 2,
		2, 3, 0,
		// right
		1, 5, 6,
		6, 2, 1,
		// back
		7, 6, 5,
		5, 4, 7,
		// left
		4, 0, 3,
		3, 7, 4,
		// bottom
		4, 5, 1,
		1, 0, 4,
		// top
		3, 2, 6,
		6, 7, 3
	};
}

void BoxOccluder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &BoxOccluder3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &BoxOccluder3D::get_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
}

BoxOccluder3D::BoxOccluder3D() {
}

BoxOccluder3D::~BoxOccluder3D() {
}

/////////////////////////////////////////////////

void SphereOccluder3D::set_radius(float p_radius) {
	if (radius == p_radius) {
		return;
	}

	radius = MAX(p_radius, 0.0f);
	_update();
}

float SphereOccluder3D::get_radius() const {
	return radius;
}

void SphereOccluder3D::_update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	r_vertices.resize((RINGS + 2) * (RADIAL_SEGMENTS + 1));
	int vertex_i = 0;
	Vector3 *vertex_ptr = r_vertices.ptrw();

	r_indices.resize((RINGS + 1) * RADIAL_SEGMENTS * 6);
	int idx_i = 0;
	int *idx_ptr = r_indices.ptrw();

	int current_row = 0;
	int previous_row = 0;
	int point = 0;
	for (int j = 0; j <= (RINGS + 1); j++) {
		float v = j / float(RINGS + 1);
		float w = Math::sin(Math_PI * v);
		float y = Math::cos(Math_PI * v);
		for (int i = 0; i <= RADIAL_SEGMENTS; i++) {
			float u = i / float(RADIAL_SEGMENTS);

			float x = Math::cos(u * Math_TAU);
			float z = Math::sin(u * Math_TAU);
			vertex_ptr[vertex_i++] = Vector3(x * w, y, z * w) * radius;

			if (i > 0 && j > 0) {
				idx_ptr[idx_i++] = previous_row + i - 1;
				idx_ptr[idx_i++] = previous_row + i;
				idx_ptr[idx_i++] = current_row + i - 1;

				idx_ptr[idx_i++] = previous_row + i;
				idx_ptr[idx_i++] = current_row + i;
				idx_ptr[idx_i++] = current_row + i - 1;
			}

			point++;
		}

		previous_row = current_row;
		current_row = point;
	}
}

void SphereOccluder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &SphereOccluder3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &SphereOccluder3D::get_radius);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_NONE, "suffix:m"), "set_radius", "get_radius");
}

SphereOccluder3D::SphereOccluder3D() {
}

SphereOccluder3D::~SphereOccluder3D() {
}

/////////////////////////////////////////////////

void PolygonOccluder3D::set_polygon(const Vector<Vector2> &p_polygon) {
	polygon = p_polygon;
	_update();
}

Vector<Vector2> PolygonOccluder3D::get_polygon() const {
	return polygon;
}

void PolygonOccluder3D::_update_arrays(PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	if (polygon.size() < 3) {
		r_vertices.clear();
		r_indices.clear();
		return;
	}

	Vector<Point2> occluder_polygon = polygon;
	if (Triangulate::get_area(occluder_polygon) > 0) {
		occluder_polygon.reverse();
	}

	Vector<int> occluder_indices = Geometry2D::triangulate_polygon(occluder_polygon);

	if (occluder_indices.size() < 3) {
		r_vertices.clear();
		r_indices.clear();
		ERR_FAIL_MSG("Failed to triangulate PolygonOccluder3D. Make sure the polygon doesn't have any intersecting edges.");
	}

	r_vertices.resize(occluder_polygon.size());
	Vector3 *vertex_ptr = r_vertices.ptrw();
	const Vector2 *polygon_ptr = occluder_polygon.ptr();
	for (int i = 0; i < occluder_polygon.size(); i++) {
		vertex_ptr[i] = Vector3(polygon_ptr[i].x, polygon_ptr[i].y, 0.0);
	}

	r_indices.resize(occluder_indices.size());
	memcpy(r_indices.ptrw(), occluder_indices.ptr(), occluder_indices.size() * sizeof(int));
}

bool PolygonOccluder3D::_has_editable_3d_polygon_no_depth() const {
	return false;
}

void PolygonOccluder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &PolygonOccluder3D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &PolygonOccluder3D::get_polygon);

	ClassDB::bind_method(D_METHOD("_has_editable_3d_polygon_no_depth"), &PolygonOccluder3D::_has_editable_3d_polygon_no_depth);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
}

PolygonOccluder3D::PolygonOccluder3D() {
}

PolygonOccluder3D::~PolygonOccluder3D() {
}

/////////////////////////////////////////////////

AABB OccluderInstance3D::get_aabb() const {
	if (occluder.is_valid()) {
		return occluder->get_aabb();
	}
	return AABB();
}

void OccluderInstance3D::set_occluder(const Ref<Occluder3D> &p_occluder) {
	if (occluder == p_occluder) {
		return;
	}

	if (occluder.is_valid()) {
		occluder->disconnect_changed(callable_mp(this, &OccluderInstance3D::_occluder_changed));
	}

	occluder = p_occluder;

	if (occluder.is_valid()) {
		set_base(occluder->get_rid());
		occluder->connect_changed(callable_mp(this, &OccluderInstance3D::_occluder_changed));
	} else {
		set_base(RID());
	}

	update_gizmos();
	update_configuration_warnings();

#ifdef TOOLS_ENABLED
	// PolygonOccluder3D is edited via an editor plugin, this ensures the plugin is shown/hidden when necessary
	if (Engine::get_singleton()->is_editor_hint()) {
		callable_mp(EditorNode::get_singleton(), &EditorNode::edit_current).call_deferred();
	}
#endif
}

void OccluderInstance3D::_occluder_changed() {
	update_gizmos();
	update_configuration_warnings();
}

void OccluderInstance3D::_project_settings_changed() {
	// Use Occlusion Culling project setting affects node configuration warnings.
	update_configuration_warnings();
}

Ref<Occluder3D> OccluderInstance3D::get_occluder() const {
	return occluder;
}

void OccluderInstance3D::set_bake_mask(uint32_t p_mask) {
	bake_mask = p_mask;
	update_configuration_warnings();
}

uint32_t OccluderInstance3D::get_bake_mask() const {
	return bake_mask;
}

void OccluderInstance3D::set_bake_simplification_distance(float p_dist) {
	bake_simplification_dist = MAX(p_dist, 0.0f);
}

float OccluderInstance3D::get_bake_simplification_distance() const {
	return bake_simplification_dist;
}

void OccluderInstance3D::set_bake_mask_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Render layer number must be between 1 and 20 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 20, "Render layer number must be between 1 and 20 inclusive.");
	uint32_t mask = get_bake_mask();
	if (p_value) {
		mask |= 1 << (p_layer_number - 1);
	} else {
		mask &= ~(1 << (p_layer_number - 1));
	}
	set_bake_mask(mask);
}

bool OccluderInstance3D::get_bake_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Render layer number must be between 1 and 20 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 20, false, "Render layer number must be between 1 and 20 inclusive.");
	return bake_mask & (1 << (p_layer_number - 1));
}

bool OccluderInstance3D::_bake_material_check(Ref<Material> p_material) {
	StandardMaterial3D *standard_mat = Object::cast_to<StandardMaterial3D>(p_material.ptr());
	if (standard_mat && standard_mat->get_transparency() != StandardMaterial3D::TRANSPARENCY_DISABLED) {
		return false;
	}
	return true;
}

void OccluderInstance3D::_bake_surface(const Transform3D &p_transform, Array p_surface_arrays, Ref<Material> p_material, float p_simplification_dist, PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	if (!_bake_material_check(p_material)) {
		return;
	}
	ERR_FAIL_COND_MSG(p_surface_arrays.size() != Mesh::ARRAY_MAX, "Invalid surface array.");

	PackedVector3Array vertices = p_surface_arrays[Mesh::ARRAY_VERTEX];
	PackedInt32Array indices = p_surface_arrays[Mesh::ARRAY_INDEX];

	if (vertices.size() == 0 || indices.size() == 0) {
		return;
	}

	Vector3 *vertices_ptr = vertices.ptrw();
	for (int j = 0; j < vertices.size(); j++) {
		vertices_ptr[j] = p_transform.xform(vertices_ptr[j]);
	}

	if (!Math::is_zero_approx(p_simplification_dist) && SurfaceTool::simplify_func) {
		Vector<float> vertices_f32 = vector3_to_float32_array(vertices.ptr(), vertices.size());

		float error_scale = SurfaceTool::simplify_scale_func(vertices_f32.ptr(), vertices.size(), sizeof(float) * 3);
		float target_error = p_simplification_dist / error_scale;
		float error = -1.0f;
		int target_index_count = MIN(indices.size(), 36);

		const int simplify_options = SurfaceTool::SIMPLIFY_LOCK_BORDER;

		uint32_t index_count = SurfaceTool::simplify_func(
				(unsigned int *)indices.ptrw(),
				(unsigned int *)indices.ptr(),
				indices.size(),
				vertices_f32.ptr(), vertices.size(), sizeof(float) * 3,
				target_index_count, target_error, simplify_options, &error);
		indices.resize(index_count);
	}

	SurfaceTool::strip_mesh_arrays(vertices, indices);

	int vertex_offset = r_vertices.size();
	r_vertices.resize(vertex_offset + vertices.size());
	memcpy(r_vertices.ptrw() + vertex_offset, vertices.ptr(), vertices.size() * sizeof(Vector3));

	int index_offset = r_indices.size();
	r_indices.resize(index_offset + indices.size());
	int *idx_ptr = r_indices.ptrw();
	for (int j = 0; j < indices.size(); j++) {
		idx_ptr[index_offset + j] = vertex_offset + indices[j];
	}
}

void OccluderInstance3D::_bake_node(Node *p_node, PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
	if (mi && mi->is_visible_in_tree()) {
		Ref<Mesh> mesh = mi->get_mesh();
		bool valid = true;

		if (mesh.is_null()) {
			valid = false;
		}

		if (valid && !_bake_material_check(mi->get_material_override())) {
			valid = false;
		}

		if ((mi->get_layer_mask() & bake_mask) == 0) {
			valid = false;
		}

		if (valid) {
			Transform3D global_to_local = get_global_transform().affine_inverse() * mi->get_global_transform();

			for (int i = 0; i < mesh->get_surface_count(); i++) {
				_bake_surface(global_to_local, mesh->surface_get_arrays(i), mi->get_active_material(i), bake_simplification_dist, r_vertices, r_indices);
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (!child->get_owner()) {
			continue; // may be a helper
		}

		_bake_node(child, r_vertices, r_indices);
	}
}

void OccluderInstance3D::bake_single_node(const Node3D *p_node, float p_simplification_distance, PackedVector3Array &r_vertices, PackedInt32Array &r_indices) {
	ERR_FAIL_NULL(p_node);

	Transform3D xform = p_node->is_inside_tree() ? p_node->get_global_transform() : p_node->get_transform();

	const MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
	if (mi) {
		Ref<Mesh> mesh = mi->get_mesh();
		bool valid = true;

		if (mesh.is_null()) {
			valid = false;
		}

		if (valid && !_bake_material_check(mi->get_material_override())) {
			valid = false;
		}

		if (valid) {
			for (int i = 0; i < mesh->get_surface_count(); i++) {
				_bake_surface(xform, mesh->surface_get_arrays(i), mi->get_active_material(i), p_simplification_distance, r_vertices, r_indices);
			}
		}
	}

	const ImporterMeshInstance3D *imi = Object::cast_to<ImporterMeshInstance3D>(p_node);
	if (imi) {
		Ref<ImporterMesh> mesh = imi->get_mesh();
		bool valid = true;

		if (mesh.is_null()) {
			valid = false;
		}

		if (valid) {
			for (int i = 0; i < mesh->get_surface_count(); i++) {
				Ref<Material> material = imi->get_surface_material(i);
				if (material.is_null()) {
					material = mesh->get_surface_material(i);
				}
				_bake_surface(xform, mesh->get_surface_arrays(i), material, p_simplification_distance, r_vertices, r_indices);
			}
		}
	}
}

OccluderInstance3D::BakeError OccluderInstance3D::bake_scene(Node *p_from_node, String p_occluder_path) {
	if (p_occluder_path.is_empty()) {
		if (get_occluder().is_null()) {
			return BAKE_ERROR_NO_SAVE_PATH;
		}
		p_occluder_path = get_occluder()->get_path();
		if (!p_occluder_path.is_resource_file()) {
			return BAKE_ERROR_NO_SAVE_PATH;
		}
	}

	PackedVector3Array vertices;
	PackedInt32Array indices;

	_bake_node(p_from_node, vertices, indices);

	if (vertices.is_empty() || indices.is_empty()) {
		return BAKE_ERROR_NO_MESHES;
	}

	Ref<ArrayOccluder3D> occ;
	if (get_occluder().is_valid()) {
		occ = get_occluder();
		set_occluder(Ref<Occluder3D>()); // clear
	}

	if (occ.is_null()) {
		occ.instantiate();
	}

	occ->set_arrays(vertices, indices);

	Error err = ResourceSaver::save(occ, p_occluder_path);

	if (err != OK) {
		return BAKE_ERROR_CANT_SAVE;
	}

	occ->set_path(p_occluder_path);
	set_occluder(occ);

	return BAKE_ERROR_OK;
}

PackedStringArray OccluderInstance3D::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();

	if (!bool(GLOBAL_GET("rendering/occlusion_culling/use_occlusion_culling"))) {
		warnings.push_back(RTR("Occlusion culling is disabled in the Project Settings, which means occlusion culling won't be performed in the root viewport.\nTo resolve this, open the Project Settings and enable Rendering > Occlusion Culling > Use Occlusion Culling."));
	}

	if (bake_mask == 0) {
		warnings.push_back(RTR("The Bake Mask has no bits enabled, which means baking will not produce any occluder meshes for this OccluderInstance3D.\nTo resolve this, enable at least one bit in the Bake Mask property."));
	}

	if (occluder.is_null()) {
		warnings.push_back(RTR("No occluder mesh is defined in the Occluder property, so no occlusion culling will be performed using this OccluderInstance3D.\nTo resolve this, set the Occluder property to one of the primitive occluder types or bake the scene meshes by selecting the OccluderInstance3D and pressing the Bake Occluders button at the top of the 3D editor viewport."));
	} else {
		Ref<ArrayOccluder3D> arr_occluder = occluder;
		if (arr_occluder.is_valid() && arr_occluder->get_indices().size() < 3) {
			// Setting a new ArrayOccluder3D from the inspector will create an empty occluder,
			// so warn the user about this.
			warnings.push_back(RTR("The occluder mesh has less than 3 vertices, so no occlusion culling will be performed using this OccluderInstance3D.\nTo generate a proper occluder mesh, select the OccluderInstance3D then use the Bake Occluders button at the top of the 3D editor viewport."));
		}
		Ref<PolygonOccluder3D> poly_occluder = occluder;
		if (poly_occluder.is_valid() && poly_occluder->get_polygon().size() < 3) {
			warnings.push_back(RTR("The polygon occluder has less than 3 vertices, so no occlusion culling will be performed using this OccluderInstance3D.\nVertices can be added in the inspector or using the polygon editing tools at the top of the 3D editor viewport."));
		}
	}

	return warnings;
}

bool OccluderInstance3D::_is_editable_3d_polygon() const {
	return Ref<PolygonOccluder3D>(occluder).is_valid();
}

Ref<Resource> OccluderInstance3D::_get_editable_3d_polygon_resource() const {
	return occluder;
}

void OccluderInstance3D::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				// Use Occlusion Culling project setting affects node configuration warnings.
				EditorNode::get_singleton()->connect("project_settings_changed", callable_mp(this, &OccluderInstance3D::_project_settings_changed));
			}
		} break;
	}
#endif
}

void OccluderInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bake_mask", "mask"), &OccluderInstance3D::set_bake_mask);
	ClassDB::bind_method(D_METHOD("get_bake_mask"), &OccluderInstance3D::get_bake_mask);
	ClassDB::bind_method(D_METHOD("set_bake_mask_value", "layer_number", "value"), &OccluderInstance3D::set_bake_mask_value);
	ClassDB::bind_method(D_METHOD("get_bake_mask_value", "layer_number"), &OccluderInstance3D::get_bake_mask_value);
	ClassDB::bind_method(D_METHOD("set_bake_simplification_distance", "simplification_distance"), &OccluderInstance3D::set_bake_simplification_distance);
	ClassDB::bind_method(D_METHOD("get_bake_simplification_distance"), &OccluderInstance3D::get_bake_simplification_distance);

	ClassDB::bind_method(D_METHOD("set_occluder", "occluder"), &OccluderInstance3D::set_occluder);
	ClassDB::bind_method(D_METHOD("get_occluder"), &OccluderInstance3D::get_occluder);

	ClassDB::bind_method(D_METHOD("_is_editable_3d_polygon"), &OccluderInstance3D::_is_editable_3d_polygon);
	ClassDB::bind_method(D_METHOD("_get_editable_3d_polygon_resource"), &OccluderInstance3D::_get_editable_3d_polygon_resource);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "occluder", PROPERTY_HINT_RESOURCE_TYPE, "Occluder3D"), "set_occluder", "get_occluder");
	ADD_GROUP("Bake", "bake_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_bake_mask", "get_bake_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_simplification_distance", PROPERTY_HINT_RANGE, "0.0,2.0,0.01,suffix:m"), "set_bake_simplification_distance", "get_bake_simplification_distance");
}

OccluderInstance3D::OccluderInstance3D() {
}

OccluderInstance3D::~OccluderInstance3D() {
}
