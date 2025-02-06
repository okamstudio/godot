/**************************************************************************/
/*  many_bone_ik_3d_gizmo_plugin.h                                        */
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

#ifndef MANY_BONE_IK_3D_GIZMO_PLUGIN_H
#define MANY_BONE_IK_3D_GIZMO_PLUGIN_H

#include "../src/ik_bone_3d.h"
#include "../src/many_bone_ik_3d.h"

#include "editor/editor_inspector.h"
#include "editor/editor_settings.h"
#include "editor/plugins/skeleton_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/immediate_mesh.h"
#include "scene/resources/material.h"

class Joint;
class PhysicalBone3D;
class ManyBoneIKEditorPlugin;
class Button;

class ManyBoneIK3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(ManyBoneIK3DGizmoPlugin, EditorNode3DGizmoPlugin);
	Ref<Shader> kusudama_shader = memnew(Shader);

	Ref<StandardMaterial3D> unselected_mat;
	Ref<ShaderMaterial> selected_mat;
	Ref<Shader> selected_sh = memnew(Shader);

	MeshInstance3D *handles_mesh_instance = nullptr;
	Ref<ImmediateMesh> handles_mesh = memnew(ImmediateMesh);
	Ref<ShaderMaterial> handle_material = memnew(ShaderMaterial);
	Ref<Shader> handle_shader;
	ManyBoneIK3D *many_bone_ik = nullptr;
	Button *edit_mode_button = nullptr;
	bool edit_mode = false;

protected:
	static void _bind_methods();
	void _notifications(int32_t p_what);

public:
	const Color bone_color = EditorSettings::get_singleton()->get("editors/3d_gizmos/gizmo_colors/skeleton");
	const int32_t KUSUDAMA_MAX_CONES = 10;
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;
	int32_t get_priority() const override;
	void create_gizmo_mesh(BoneId current_bone_idx, Ref<IKBone3D> ik_bone, EditorNode3DGizmo *p_gizmo, Color current_bone_color, Skeleton3D *many_bone_ik_skeleton, ManyBoneIK3D *p_many_bone_ik);
	int subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const override;
	Transform3D get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const override;
	void set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform) override;
	void commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) override;

	void edit_mode_toggled(const bool pressed);
	void _subgizmo_selection_change();
	void _update_gizmo_visible();
	void _draw_gizmo();

	void _draw_handles();
	void _hide_handles();
};

class EditorPluginManyBoneIK : public EditorPlugin {
	GDCLASS(EditorPluginManyBoneIK, EditorPlugin);

public:
	EditorPluginManyBoneIK();
};

#endif // MANY_BONE_IK_3D_GIZMO_PLUGIN_H
