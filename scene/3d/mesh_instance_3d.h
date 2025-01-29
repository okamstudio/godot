/**************************************************************************/
/*  mesh_instance_3d.h                                                    */
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

#ifndef MESH_INSTANCE_3D_H
#define MESH_INSTANCE_3D_H

#include "core/templates/local_vector.h"
#include "scene/3d/visual_instance_3d.h"

class NavigationMesh;
class NavigationMeshSourceGeometryData3D;
class Skin;
class SkinReference;

class MeshInstance3D : public GeometryInstance3D {
	GDCLASS(MeshInstance3D, GeometryInstance3D);

protected:
	Ref<Mesh> mesh;
	Ref<Skin> skin;
	Ref<Skin> skin_internal;
	Ref<SkinReference> skin_ref;
	NodePath skeleton_path = NodePath("..");

	LocalVector<float> blend_shape_tracks;
	HashMap<StringName, int> blend_shape_properties;
	Vector<Ref<Material>> surface_override_materials;

	void _mesh_changed();
	void _resolve_skeleton_path();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool surface_index_0 = false;

	void _notification(int p_what);
	static void _bind_methods();

	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

public:
	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_skin(const Ref<Skin> &p_skin);
	Ref<Skin> get_skin() const;

	void set_skeleton_path(const NodePath &p_skeleton);
	NodePath get_skeleton_path();

	Ref<SkinReference> get_skin_reference() const;

	int get_blend_shape_count() const;
	int find_blend_shape_by_name(const StringName &p_name);
	float get_blend_shape_value(int p_blend_shape) const;
	void set_blend_shape_value(int p_blend_shape, float p_value);

	int get_surface_override_material_count() const;
	void set_surface_override_material(int p_surface, const Ref<Material> &p_material);
	Ref<Material> get_surface_override_material(int p_surface) const;
	Ref<Material> get_active_material(int p_surface) const;

	Node *create_trimesh_collision_node();
	void create_trimesh_collision();

	Node *create_convex_collision_node(bool p_clean = true, bool p_simplify = false);
	void create_convex_collision(bool p_clean = true, bool p_simplify = false);

	Node *create_multiple_convex_collisions_node(const Ref<MeshConvexDecompositionSettings> &p_settings = Ref<MeshConvexDecompositionSettings>());
	void create_multiple_convex_collisions(const Ref<MeshConvexDecompositionSettings> &p_settings = Ref<MeshConvexDecompositionSettings>());

	MeshInstance3D *create_debug_tangents_node();
	void create_debug_tangents();

	virtual AABB get_aabb() const override;

	Ref<ArrayMesh> bake_mesh_from_current_blend_shape_mix(Ref<ArrayMesh> p_existing = Ref<ArrayMesh>());
	Ref<ArrayMesh> bake_mesh_from_current_skeleton_pose(Ref<ArrayMesh> p_existing = Ref<ArrayMesh>());

	virtual Ref<TriangleMesh> generate_triangle_mesh() const override;

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node);

	MeshInstance3D();
	~MeshInstance3D();
};

#endif // MESH_INSTANCE_3D_H
