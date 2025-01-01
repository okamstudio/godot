/**************************************************************************/
/*  node_3d_editor_gizmos.h                                               */
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

#ifndef NODE_3D_EDITOR_GIZMOS_H
#define NODE_3D_EDITOR_GIZMOS_H

#include "core/math/dynamic_bvh.h"
#include "core/templates/hash_map.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/skeleton_3d.h"

class Timer;
class EditorNode3DGizmoPlugin;

class EditorNode3DGizmo : public Node3DGizmo {
	GDCLASS(EditorNode3DGizmo, Node3DGizmo);

	struct Instance {
		RID instance;
		Ref<Mesh> mesh;
		Ref<Material> material;
		Ref<SkinReference> skin_reference;
		bool extra_margin = false;
		Transform3D xform;

		void create_instance(Node3D *p_base, bool p_hidden = false);
	};

	bool selected;

	Vector<Vector3> collision_segments;
	Ref<TriangleMesh> collision_mesh;

	Vector<Vector3> handles;
	Vector<int> handle_ids;
	Vector<Vector3> secondary_handles;
	Vector<int> secondary_handle_ids;

	real_t selectable_icon_size;
	bool billboard_handle;

	bool valid;
	bool hidden;
	Vector<Instance> instances;
	Node3D *spatial_node = nullptr;

	DynamicBVH::ID bvh_node_id;

	void _set_node_3d(Node *p_node) { set_node_3d(Object::cast_to<Node3D>(p_node)); }

	void _update_bvh();

protected:
	static void _bind_methods();

	EditorNode3DGizmoPlugin *gizmo_plugin = nullptr;

	GDVIRTUAL0(_redraw)
	GDVIRTUAL2RC(String, _get_handle_name, int, bool)
	GDVIRTUAL2RC(bool, _is_handle_highlighted, int, bool)
	GDVIRTUAL2RC(Variant, _get_handle_value, int, bool)
	GDVIRTUAL2(_begin_handle_action, int, bool)
	GDVIRTUAL4(_set_handle, int, bool, const Camera3D *, Vector2)
	GDVIRTUAL4(_commit_handle, int, bool, Variant, bool)

	GDVIRTUAL2RC(int, _subgizmos_intersect_ray, const Camera3D *, Vector2)
	GDVIRTUAL2RC(Vector<int>, _subgizmos_intersect_frustum, const Camera3D *, TypedArray<Plane>)
	GDVIRTUAL1RC(Transform3D, _get_subgizmo_transform, int)
	GDVIRTUAL2(_set_subgizmo_transform, int, Transform3D)
	GDVIRTUAL3(_commit_subgizmos, Vector<int>, TypedArray<Transform3D>, bool)
public:
	void add_lines(const Vector<Vector3> &p_lines, const Ref<Material> &p_material, bool p_billboard = false, const Color &p_modulate = Color(1, 1, 1));
	void add_vertices(const Vector<Vector3> &p_vertices, const Ref<Material> &p_material, Mesh::PrimitiveType p_primitive_type, bool p_billboard = false, const Color &p_modulate = Color(1, 1, 1));
	void add_mesh(const Ref<Mesh> &p_mesh, const Ref<Material> &p_material = Ref<Material>(), const Transform3D &p_xform = Transform3D(), const Ref<SkinReference> &p_skin_reference = Ref<SkinReference>());
	void add_collision_segments(const Vector<Vector3> &p_lines);
	void add_collision_triangles(const Ref<TriangleMesh> &p_tmesh);
	void add_unscaled_billboard(const Ref<Material> &p_material, real_t p_scale = 1, const Color &p_modulate = Color(1, 1, 1));
	void add_handles(const Vector<Vector3> &p_handles, const Ref<Material> &p_material, const Vector<int> &p_ids = Vector<int>(), bool p_billboard = false, bool p_secondary = false);
	void add_solid_box(const Ref<Material> &p_material, Vector3 p_size, Vector3 p_position = Vector3(), const Transform3D &p_xform = Transform3D());

	virtual bool is_handle_highlighted(int p_id, bool p_secondary) const;
	virtual String get_handle_name(int p_id, bool p_secondary) const;
	virtual Variant get_handle_value(int p_id, bool p_secondary) const;
	virtual void begin_handle_action(int p_id, bool p_secondary);
	virtual void set_handle(int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point);
	virtual void commit_handle(int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel = false);

	virtual int subgizmos_intersect_ray(Camera3D *p_camera, const Vector2 &p_point) const;
	virtual Vector<int> subgizmos_intersect_frustum(const Camera3D *p_camera, const Vector<Plane> &p_frustum) const;
	virtual Transform3D get_subgizmo_transform(int p_id) const;
	virtual void set_subgizmo_transform(int p_id, Transform3D p_transform);
	virtual void commit_subgizmos(const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel = false);

	void set_selected(bool p_selected) { selected = p_selected; }
	bool is_selected() const { return selected; }

	void set_node_3d(Node3D *p_node);
	Node3D *get_node_3d() const { return spatial_node; }
	Ref<EditorNode3DGizmoPlugin> get_plugin() const { return gizmo_plugin; }
	bool intersect_frustum(const Camera3D *p_camera, const Vector<Plane> &p_frustum);
	void handles_intersect_ray(Camera3D *p_camera, const Vector2 &p_point, bool p_shift_pressed, int &r_id, bool &r_secondary);
	bool intersect_ray(Camera3D *p_camera, const Point2 &p_point, Vector3 &r_pos, Vector3 &r_normal);
	bool is_subgizmo_selected(int p_id) const;
	Vector<int> get_subgizmo_selection() const;

	virtual void clear() override;
	virtual void create() override;
	virtual void transform() override;
	virtual void redraw() override;
	virtual void free() override;

	virtual bool is_editable() const;

	void set_hidden(bool p_hidden);
	void set_plugin(EditorNode3DGizmoPlugin *p_plugin);

	EditorNode3DGizmo();
	~EditorNode3DGizmo();
};

class EditorNode3DGizmoPlugin : public Resource {
	GDCLASS(EditorNode3DGizmoPlugin, Resource);

public:
	static const int VISIBLE = 0;
	static const int HIDDEN = 1;
	static const int ON_TOP = 2;

protected:
	int current_state;
	HashSet<EditorNode3DGizmo *> current_gizmos;
	HashMap<String, Vector<Ref<StandardMaterial3D>>> materials;

	static void _bind_methods();
	virtual bool has_gizmo(Node3D *p_spatial);
	virtual Ref<EditorNode3DGizmo> create_gizmo(Node3D *p_spatial);

	GDVIRTUAL1RC(bool, _has_gizmo, Node3D *)
	GDVIRTUAL1RC(Ref<EditorNode3DGizmo>, _create_gizmo, Node3D *)

	GDVIRTUAL0RC(String, _get_gizmo_name)
	GDVIRTUAL0RC(int, _get_priority)
	GDVIRTUAL0RC(bool, _can_be_hidden)
	GDVIRTUAL0RC(bool, _is_selectable_when_hidden)

	GDVIRTUAL1(_redraw, Ref<EditorNode3DGizmo>)
	GDVIRTUAL3RC(String, _get_handle_name, Ref<EditorNode3DGizmo>, int, bool)
	GDVIRTUAL3RC(bool, _is_handle_highlighted, Ref<EditorNode3DGizmo>, int, bool)
	GDVIRTUAL3RC(Variant, _get_handle_value, Ref<EditorNode3DGizmo>, int, bool)

	GDVIRTUAL3(_begin_handle_action, Ref<EditorNode3DGizmo>, int, bool)
	GDVIRTUAL5(_set_handle, Ref<EditorNode3DGizmo>, int, bool, const Camera3D *, Vector2)
	GDVIRTUAL5(_commit_handle, Ref<EditorNode3DGizmo>, int, bool, Variant, bool)

	GDVIRTUAL3RC(int, _subgizmos_intersect_ray, Ref<EditorNode3DGizmo>, const Camera3D *, Vector2)
	GDVIRTUAL3RC(Vector<int>, _subgizmos_intersect_frustum, Ref<EditorNode3DGizmo>, const Camera3D *, TypedArray<Plane>)
	GDVIRTUAL2RC(Transform3D, _get_subgizmo_transform, Ref<EditorNode3DGizmo>, int)
	GDVIRTUAL3(_set_subgizmo_transform, Ref<EditorNode3DGizmo>, int, Transform3D)
	GDVIRTUAL4(_commit_subgizmos, Ref<EditorNode3DGizmo>, Vector<int>, TypedArray<Transform3D>, bool)

public:
	void create_material(const String &p_name, const Color &p_color, bool p_billboard = false, bool p_on_top = false, bool p_use_vertex_color = false);
	void create_icon_material(const String &p_name, const Ref<Texture2D> &p_texture, bool p_on_top = false, const Color &p_albedo = Color(1, 1, 1, 1));
	void create_handle_material(const String &p_name, bool p_billboard = false, const Ref<Texture2D> &p_texture = nullptr);
	void add_material(const String &p_name, Ref<StandardMaterial3D> p_material);

	Ref<StandardMaterial3D> get_material(const String &p_name, const Ref<EditorNode3DGizmo> &p_gizmo = Ref<EditorNode3DGizmo>());

	virtual String get_gizmo_name() const;
	virtual int get_priority() const;
	virtual bool can_be_hidden() const;
	virtual bool is_selectable_when_hidden() const;

	virtual void redraw(EditorNode3DGizmo *p_gizmo);
	virtual bool is_handle_highlighted(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const;
	virtual String get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const;
	virtual Variant get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const;
	virtual void begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary);
	virtual void set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point);
	virtual void commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel = false);

	virtual int subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const;
	virtual Vector<int> subgizmos_intersect_frustum(const EditorNode3DGizmo *p_gizmo, const Camera3D *p_camera, const Vector<Plane> &p_frustum) const;
	virtual Transform3D get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const;
	virtual void set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform);
	virtual void commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel = false);

	Ref<EditorNode3DGizmo> get_gizmo(Node3D *p_spatial);
	void set_state(int p_state);
	int get_state() const;
	void unregister_gizmo(EditorNode3DGizmo *p_gizmo);

	EditorNode3DGizmoPlugin();
	virtual ~EditorNode3DGizmoPlugin();
};

#endif // NODE_3D_EDITOR_GIZMOS_H
