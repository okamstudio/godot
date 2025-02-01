/**************************************************************************/
/*  spring_bone_3d_gizmo_plugin.h                                         */
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

#pragma once

#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/spring_bone_collision_3d.h"
#include "scene/3d/spring_bone_simulator_3d.h"
#include "scene/resources/surface_tool.h"

class SpringBoneSimulator3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(SpringBoneSimulator3DGizmoPlugin, EditorNode3DGizmoPlugin);

	struct SelectionMaterials {
		Ref<StandardMaterial3D> unselected_mat;
		Ref<ShaderMaterial> selected_mat;
	};
	static SelectionMaterials selection_materials;

public:
	static Ref<ArrayMesh> get_joints_mesh(Skeleton3D *p_skeleton, SpringBoneSimulator3D *p_simulator, bool p_is_selected);
	static void draw_sphere(Ref<SurfaceTool> &p_surface_tool, const Basis &p_basis, const Vector3 &p_center, float p_radius, const Color &p_color);
	static void draw_line(Ref<SurfaceTool> &p_surface_tool, const Vector3 &p_begin_pos, const Vector3 &p_end_pos, const Color &p_color);

	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;

	void redraw(EditorNode3DGizmo *p_gizmo) override;

	SpringBoneSimulator3DGizmoPlugin();
	~SpringBoneSimulator3DGizmoPlugin();
};

class SpringBoneCollision3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(SpringBoneCollision3DGizmoPlugin, EditorNode3DGizmoPlugin);

	struct SelectionMaterials {
		Ref<StandardMaterial3D> unselected_mat;
		Ref<ShaderMaterial> selected_mat;
	};
	static SelectionMaterials selection_materials;

public:
	static Ref<ArrayMesh> get_collision_mesh(SpringBoneCollision3D *p_collision, bool p_is_selected);
	static void draw_sphere(Ref<SurfaceTool> &p_surface_tool, float p_radius, const Color &p_color);
	static void draw_capsule(Ref<SurfaceTool> &p_surface_tool, float p_radius, float p_height, const Color &p_color);
	static void draw_plane(Ref<SurfaceTool> &p_surface_tool, const Color &p_color);

	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;

	void redraw(EditorNode3DGizmo *p_gizmo) override;

	SpringBoneCollision3DGizmoPlugin();
	~SpringBoneCollision3DGizmoPlugin();
};
