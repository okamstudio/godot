/**************************************************************************/
/*  many_bone_ik_3d_gizmo_plugin.cpp                                      */
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

#include "core/math/transform_3d.h"
#include "core/templates/local_vector.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/many_bone_ik_shader.h"
#include "editor/plugins/node_3d_editor_gizmos.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/surface_tool.h"

#include "../src/ik_bone_3d.h"
#include "../src/ik_kusudama_3d.h"
#include "../src/many_bone_ik_3d.h"
#include "many_bone_ik_3d_gizmo_plugin.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#endif

void ManyBoneIK3DGizmoPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_gizmo_name"), &ManyBoneIK3DGizmoPlugin::get_gizmo_name);
}

bool ManyBoneIK3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return cast_to<ManyBoneIK3D>(p_spatial);
}

String ManyBoneIK3DGizmoPlugin::get_gizmo_name() const {
	return "ManyBoneIK3D";
}

void ManyBoneIK3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	many_bone_ik = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d());
	Skeleton3D *skeleton = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d())->get_skeleton();
	p_gizmo->clear();
	if (!skeleton || !skeleton->get_bone_count()) {
		return;
	}
	if (handles_mesh_instance && !handles_mesh_instance->is_inside_tree()) {
		skeleton->call_deferred("add_child", handles_mesh_instance);
		handles_mesh_instance->set_skeleton_path(NodePath(""));
	}
	int selected = -1;
	Skeleton3DEditor *se = Skeleton3DEditor::get_singleton();
	if (se) {
		selected = se->get_selected_bone();
	}
	Color selected_bone_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/selected_bone");

	int current_bone_index = 0;
	Vector<int> bones_to_process = skeleton->get_parentless_bones();

	while (bones_to_process.size() > current_bone_index) {
		int current_bone_idx = bones_to_process[current_bone_index];

		Color current_bone_color = (current_bone_idx == selected) ? selected_bone_color : bone_color;

		for (const Ref<IKBoneSegment3D> &segmented_skeleton : many_bone_ik->get_segmented_skeletons()) {
			if (segmented_skeleton.is_null()) {
				continue;
			}
			Ref<IKBone3D> ik_bone = segmented_skeleton->get_ik_bone(current_bone_idx);
			if (ik_bone.is_null() || ik_bone->get_constraint().is_null()) {
				continue;
			}
			create_gizmo_mesh(current_bone_idx, ik_bone, p_gizmo, current_bone_color, skeleton, many_bone_ik);
		}

		current_bone_index++;

		Vector<int> child_bones_vector;
		child_bones_vector = skeleton->get_bone_children(current_bone_idx);
		int child_bones_size = child_bones_vector.size();
		for (int i = 0; i < child_bones_size; i++) {
			// Something wrong.
			if (child_bones_vector[i] < 0) {
				continue;
			}
			// Add the bone's children to the list of bones to be processed.
			bones_to_process.push_back(child_bones_vector[i]);
		}
	}
}

void ManyBoneIK3DGizmoPlugin::create_gizmo_mesh(BoneId current_bone_idx, Ref<IKBone3D> ik_bone, EditorNode3DGizmo *p_gizmo, Color current_bone_color, Skeleton3D *many_bone_ik_skeleton, ManyBoneIK3D *p_many_bone_ik) {
	Ref<IKKusudama3D> ik_kusudama = ik_bone->get_constraint();
	if (ik_kusudama.is_null()) {
		return;
	}
	const TypedArray<IKLimitCone3D> &open_cones = ik_kusudama->get_open_cones();
	if (!open_cones.size()) {
		return;
	}
	BoneId parent_idx = many_bone_ik_skeleton->get_bone_parent(current_bone_idx);
	LocalVector<int> bones;
	LocalVector<float> weights;
	bones.resize(4);
	weights.resize(4);
	for (int i = 0; i < 4; i++) {
		bones[i] = 0;
		weights[i] = 0;
	}
	bones[0] = parent_idx;
	weights[0] = 1;

	Transform3D constraint_relative_to_the_skeleton = p_many_bone_ik->get_relative_transform(p_many_bone_ik->get_owner()).affine_inverse() * many_bone_ik_skeleton->get_relative_transform(many_bone_ik_skeleton->get_owner()) * p_many_bone_ik->get_godot_skeleton_transform_inverse() * ik_bone->get_constraint_orientation_transform()->get_global_transform();
	PackedFloat32Array kusudama_open_cones;
	Ref<IKKusudama3D> kusudama = ik_bone->get_constraint();
	for (int32_t cone_i = 0; cone_i < open_cones.size(); cone_i++) {
		Ref<IKLimitCone3D> open_cone = open_cones[cone_i];
		Vector3 control_point = open_cone->get_control_point();
		PackedFloat32Array new_kusudama_open_cones;
		new_kusudama_open_cones.resize(4 * 3);
		new_kusudama_open_cones.fill(0.0f);
		new_kusudama_open_cones.write[0] = control_point.x;
		new_kusudama_open_cones.write[1] = control_point.y;
		new_kusudama_open_cones.write[2] = control_point.z;
		float radius = open_cone->get_radius();
		new_kusudama_open_cones.write[3] = radius;

		Vector3 tangent_center_1 = open_cone->get_tangent_circle_center_next_1();
		new_kusudama_open_cones.write[4] = tangent_center_1.x;
		new_kusudama_open_cones.write[5] = tangent_center_1.y;
		new_kusudama_open_cones.write[6] = tangent_center_1.z;
		float tangent_radius = open_cone->get_tangent_circle_radius_next();
		new_kusudama_open_cones.write[7] = tangent_radius;

		Vector3 tangent_center_2 = open_cone->get_tangent_circle_center_next_2();
		new_kusudama_open_cones.write[8] = tangent_center_2.x;
		new_kusudama_open_cones.write[9] = tangent_center_2.y;
		new_kusudama_open_cones.write[10] = tangent_center_2.z;
		new_kusudama_open_cones.write[11] = tangent_radius;

		kusudama_open_cones.append_array(new_kusudama_open_cones);
	}
	if (current_bone_idx >= many_bone_ik_skeleton->get_bone_count()) {
		return;
	}
	if (current_bone_idx == -1) {
		return;
	}
	if (parent_idx >= many_bone_ik_skeleton->get_bone_count()) {
		return;
	}
	if (parent_idx <= -1) {
		return;
	}
	// Code copied from the SphereMesh.
	int rings = 8;

	int i = 0, j = 0, prevrow = 0, thisrow = 0, point = 0;
	float x, y, z;

	Vector<Vector3> points;
	Vector<Vector3> normals;
	Vector<int> indices;
	point = 0;

	thisrow = 0;
	prevrow = 0;
	for (j = 0; j <= (rings + 1); j++) {
		int radial_segments = 8;
		float v = j;
		float w;

		v /= (rings + 1);
		w = sin(Math_PI * v);
		y = cos(Math_PI * v);

		for (i = 0; i <= radial_segments; i++) {
			float u = i;
			u /= radial_segments;

			x = sin(u * Math_TAU);
			z = cos(u * Math_TAU);

			Vector3 p = Vector3(x * w, y, z * w) * 0.02f;
			points.push_back(p);
			Vector3 normal = Vector3(x * w, y, z * w);
			normals.push_back(normal.normalized());
			point++;

			if (i > 0 && j > 0) {
				indices.push_back(prevrow + i - 1);
				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i - 1);

				indices.push_back(prevrow + i);
				indices.push_back(thisrow + i);
				indices.push_back(thisrow + i - 1);
			};
		};

		prevrow = thisrow;
		thisrow = point;
	}
	if (!indices.size()) {
		return;
	}
	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->begin(Mesh::PRIMITIVE_TRIANGLES);
	const int32_t MESH_CUSTOM_0 = 0;
	surface_tool->set_custom_format(MESH_CUSTOM_0, SurfaceTool::CustomFormat::CUSTOM_RGBA_HALF);
	for (int32_t point_i = 0; point_i < points.size(); point_i++) {
		surface_tool->set_bones(bones);
		surface_tool->set_weights(weights);
		Color c;
		c.r = normals[point_i].x;
		c.g = normals[point_i].y;
		c.b = normals[point_i].z;
		c.a = 0;
		surface_tool->set_custom(MESH_CUSTOM_0, c);
		surface_tool->set_normal(normals[point_i]);
		surface_tool->add_vertex(points[point_i]);
	}
	for (int32_t index_i : indices) {
		surface_tool->add_index(index_i);
	}
	Ref<ShaderMaterial> kusudama_material;
	kusudama_material.instantiate();
	kusudama_material->set_shader(kusudama_shader);
	kusudama_material->set_shader_parameter("cone_sequence", kusudama_open_cones);
	int32_t cone_count = kusudama->get_open_cones().size();
	kusudama_material->set_shader_parameter("cone_count", cone_count);
	kusudama_material->set_shader_parameter("kusudama_color", current_bone_color);
	p_gizmo->add_mesh(
			surface_tool->commit(Ref<Mesh>(), RS::ARRAY_CUSTOM_RGBA_HALF << RS::ARRAY_FORMAT_CUSTOM0_SHIFT),
			kusudama_material, constraint_relative_to_the_skeleton);
}

int32_t ManyBoneIK3DGizmoPlugin::get_priority() const {
	return -1;
}

EditorPluginManyBoneIK::EditorPluginManyBoneIK() {
	Ref<ManyBoneIK3DGizmoPlugin> many_bone_ik_gizmo_plugin;
	many_bone_ik_gizmo_plugin.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(many_bone_ik_gizmo_plugin);
}

int ManyBoneIK3DGizmoPlugin::subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const {
	Skeleton3D *skeleton = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d())->get_skeleton();
	ERR_FAIL_COND_V(!skeleton, -1);

	if (!edit_mode) {
		return -1;
	}

	if (Node3DEditor::get_singleton()->get_tool_mode() != Node3DEditor::TOOL_MODE_SELECT) {
		return -1;
	}

	// Select bone.
	real_t grab_threshold = 4 * EDSCALE;
	Vector3 ray_from = p_camera->get_global_transform().origin;
	Transform3D gt = skeleton->get_global_transform();
	int closest_idx = -1;
	real_t closest_dist = 1e10;
	const int bone_count = skeleton->get_bone_count();

	for (int i = 0; i < bone_count; i++) {
		Vector3 joint_pos_3d = gt.xform(skeleton->get_bone_global_pose(i).origin);
		Vector2 joint_pos_2d = p_camera->unproject_position(joint_pos_3d);
		real_t dist_3d = ray_from.distance_to(joint_pos_3d);
		real_t dist_2d = p_point.distance_to(joint_pos_2d);
		if (dist_2d < grab_threshold && dist_3d < closest_dist) {
			closest_dist = dist_3d;
			closest_idx = i;
		}
	}

	if (closest_idx >= 0) {
		many_bone_ik->set_ui_selected_bone(closest_idx);
		return closest_idx;
	}

	many_bone_ik->set_ui_selected_bone(-1);
	return -1;
}

Transform3D ManyBoneIK3DGizmoPlugin::get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const {
	Skeleton3D *skeleton = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d())->get_skeleton();
	ERR_FAIL_COND_V(!skeleton, Transform3D());

	Transform3D constraint_relative_to_the_skeleton = many_bone_ik->get_relative_transform(many_bone_ik->get_owner()).affine_inverse() *
			skeleton->get_relative_transform(skeleton->get_owner()) * many_bone_ik->get_godot_skeleton_transform_inverse();
	return constraint_relative_to_the_skeleton * skeleton->get_bone_global_pose(p_id);
}

void ManyBoneIK3DGizmoPlugin::set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform) {
	Skeleton3D *skeleton = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d())->get_skeleton();
	ERR_FAIL_COND(!skeleton);
	// Prepare for global to local.
	Transform3D original_to_local;
	int parent_idx = skeleton->get_bone_parent(p_id);
	if (parent_idx >= 0) {
		original_to_local = skeleton->get_bone_global_pose(parent_idx);
	}
	Basis to_local = original_to_local.get_basis().inverse();

	// Prepare transform.
	Transform3D t;

	// Basis.
	t.basis = to_local * p_transform.get_basis();

	Transform3D constraint_relative_to_the_skeleton = many_bone_ik->get_relative_transform(many_bone_ik->get_owner()).affine_inverse() *
			skeleton->get_relative_transform(skeleton->get_owner()) * many_bone_ik->get_godot_skeleton_transform_inverse();
	p_transform = constraint_relative_to_the_skeleton.affine_inverse() * p_transform;
	// Origin.
	Vector3 orig = skeleton->get_bone_pose(p_id).origin;
	Vector3 sub = p_transform.origin - skeleton->get_bone_global_pose(p_id).origin;
	t.origin = orig + to_local.xform(sub);

	// Apply transform.
	skeleton->set_bone_pose_position(p_id, t.origin);
	skeleton->set_bone_pose_rotation(p_id, t.basis.get_rotation_quaternion());
	skeleton->set_bone_pose_scale(p_id, t.basis.get_scale());
	many_bone_ik->update_gizmos();
}

void ManyBoneIK3DGizmoPlugin::commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) {
	Skeleton3D *skeleton = Object::cast_to<ManyBoneIK3D>(p_gizmo->get_node_3d())->get_skeleton();
	ERR_FAIL_COND(!skeleton);

	Node3DEditor *ne = Node3DEditor::get_singleton();
	ERR_FAIL_COND(!ne);

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Set Bone Transform"));
	if (ne->get_tool_mode() == Node3DEditor::TOOL_MODE_SELECT || ne->get_tool_mode() == Node3DEditor::TOOL_MODE_ROTATE) {
		for (int i = 0; i < p_ids.size(); i++) {
			int32_t constraint_i = many_bone_ik->find_constraint(skeleton->get_bone_name(p_ids[i]));
			float from_original = many_bone_ik->get_joint_twist(constraint_i).x;
			float range = many_bone_ik->get_joint_twist(constraint_i).y;
			ur->add_do_method(many_bone_ik, "set_kusudama_twist", constraint_i, Vector2(skeleton->get_bone_pose(p_ids[i]).get_basis().get_euler().y, range));
			ur->add_undo_method(many_bone_ik, "set_kusudama_twist", constraint_i, Vector2(from_original, range));
			ur->add_do_method(many_bone_ik, "set_dirty");
			ur->add_undo_method(many_bone_ik, "set_dirty");
		}
	}
	ur->commit_action();
}

void ManyBoneIK3DGizmoPlugin::_draw_handles() {
	if (!many_bone_ik) {
		return;
	}
	Skeleton3D *skeleton = many_bone_ik->get_skeleton();
	ERR_FAIL_COND(!skeleton);
	const int bone_count = skeleton->get_bone_count();

	handles_mesh->clear_surfaces();

	if (bone_count) {
		handles_mesh_instance->show();

		handles_mesh->surface_begin(Mesh::PRIMITIVE_POINTS);

		for (int i = 0; i < bone_count; i++) {
			Color c;
			if (i == many_bone_ik->get_ui_selected_bone()) {
				c = Color(1, 1, 0);
			} else {
				c = Color(0.1, 0.25, 0.8);
			}
			Vector3 point = skeleton->get_bone_global_pose(i).origin;
			handles_mesh->surface_set_color(c);
			handles_mesh->surface_add_vertex(point);
		}
		handles_mesh->surface_end();
		handles_mesh->surface_set_material(0, handle_material);
	} else {
		handles_mesh_instance->hide();
	}
}

void ManyBoneIK3DGizmoPlugin::_draw_gizmo() {
	if (!many_bone_ik) {
		return;
	}
	Skeleton3D *skeleton = many_bone_ik->get_skeleton();
	if (!skeleton) {
		return;
	}

	// If you call get_bone_global_pose() while drawing the surface, such as toggle rest mode,
	// the skeleton update will be done first and
	// the drawing surface will be interrupted once and an error will occur.
	skeleton->force_update_all_dirty_bones();

	// Handles.
	if (edit_mode) {
		_draw_handles();
	} else {
		_hide_handles();
	}
}

void ManyBoneIK3DGizmoPlugin::_update_gizmo_visible() {
	if (!many_bone_ik) {
		return;
	}
	Skeleton3D *skeleton = many_bone_ik->get_skeleton();
	if (!skeleton) {
		return;
	}
	_subgizmo_selection_change();
	if (edit_mode) {
		int32_t selected_bone = many_bone_ik->get_ui_selected_bone();
		if (selected_bone == -1) {
#ifdef TOOLS_ENABLED
			skeleton->set_transform_gizmo_visible(false);
#endif
		} else {
#ifdef TOOLS_ENABLED
			if (skeleton->is_bone_enabled(selected_bone) && !skeleton->is_show_rest_only()) {
				skeleton->set_transform_gizmo_visible(true);
			} else {
				skeleton->set_transform_gizmo_visible(false);
			}
#endif
		}
	} else {
#ifdef TOOLS_ENABLED
		skeleton->set_transform_gizmo_visible(true);
#endif
	}
	_draw_gizmo();
}

void ManyBoneIK3DGizmoPlugin::_subgizmo_selection_change() {
	if (!many_bone_ik) {
		return;
	}
	Skeleton3D *skeleton = many_bone_ik->get_skeleton();
	if (!skeleton) {
		return;
	}
	// Once validated by subgizmos_intersect_ray, but required if through inspector's bones tree.
	if (!edit_mode) {
		skeleton->clear_subgizmo_selection();
		return;
	}
	int selected = -1;
	if (many_bone_ik) {
		selected = many_bone_ik->get_ui_selected_bone();
	}

	if (selected >= 0) {
		Vector<Ref<Node3DGizmo>> gizmos = skeleton->get_gizmos();
		for (int i = 0; i < gizmos.size(); i++) {
			Ref<EditorNode3DGizmo> gizmo = gizmos[i];
			if (!gizmo.is_valid()) {
				continue;
			}
			Ref<Skeleton3DGizmoPlugin> plugin = gizmo->get_plugin();
			if (!plugin.is_valid()) {
				continue;
			}
			skeleton->set_subgizmo_selection(gizmo, selected, skeleton->get_bone_global_pose(selected));
			break;
		}
	} else {
		skeleton->clear_subgizmo_selection();
	}
}

void ManyBoneIK3DGizmoPlugin::edit_mode_toggled(const bool pressed) {
	edit_mode = pressed;
	_update_gizmo_visible();
}

void ManyBoneIK3DGizmoPlugin::_hide_handles() {
	handles_mesh_instance->hide();
}

void ManyBoneIK3DGizmoPlugin::_notifications(int32_t p_what) {
	switch (p_what) {
		case EditorNode3DGizmoPlugin::NOTIFICATION_POSTINITIALIZE: {
			kusudama_shader->set_code(MANY_BONE_IKKUSUDAMA_SHADER);
			handle_material = Ref<ShaderMaterial>(memnew(ShaderMaterial));
			handle_shader = Ref<Shader>(memnew(Shader));
			handle_shader->set_code(R"(
// Skeleton 3D gizmo handle shader.

shader_type spatial;
render_mode unshaded, shadows_disabled, depth_draw_always;
uniform sampler2D texture_albedo : source_color;
uniform float point_size : hint_range(0,128) = 32;
void vertex() {
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix( pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb* (1.0 / 12.92), lessThan(COLOR.rgb,vec3(0.04045)) );
	}
	VERTEX = VERTEX;
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * MODEL_MATRIX * vec4(VERTEX.xyz, 1.0);
	POSITION.z = mix(POSITION.z, 0, 0.999);
	POINT_SIZE = point_size;
}
void fragment() {
	vec4 albedo_tex = texture(texture_albedo,POINT_COORD);
	vec3 col = albedo_tex.rgb + COLOR.rgb;
	col = vec3(min(col.r,1.0),min(col.g,1.0),min(col.b,1.0));
	ALBEDO = col;
	if (albedo_tex.a < 0.5) { discard; }
	ALPHA = albedo_tex.a;
}
)");
			handle_material->set_shader(handle_shader);
			Ref<Texture2D> handle = EditorNode::get_singleton()->get_gui_base()->get_theme_icon(SNAME("EditorBoneHandle"), SNAME("EditorIcons"));
			handle_material->set_shader_parameter("point_size", handle->get_width());
			handle_material->set_shader_parameter("texture_albedo", handle);

			handles_mesh_instance = memnew(MeshInstance3D);
			handles_mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			handles_mesh_instance->set_mesh(handles_mesh);
			edit_mode_button = memnew(Button);
			edit_mode_button->set_text(TTR("Edit Mode"));
			edit_mode_button->set_flat(true);
			edit_mode_button->set_toggle_mode(true);
			edit_mode_button->set_focus_mode(Control::FOCUS_NONE);
			edit_mode_button->set_tooltip_text(TTR("Edit Mode\nShow buttons on joints."));
			edit_mode_button->connect("toggled", callable_mp(this, &ManyBoneIK3DGizmoPlugin::edit_mode_toggled));
			edit_mode = false;
			create_material("lines_primary", Color(0.93725490570068, 0.19215686619282, 0.22352941334248), true, true, true);

			unselected_mat = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
			unselected_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
			unselected_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
			unselected_mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			unselected_mat->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);

			selected_mat = Ref<ShaderMaterial>(memnew(ShaderMaterial));
			selected_sh = Ref<Shader>(memnew(Shader));
			selected_sh->set_code(R"(
// Skeleton 3D gizmo bones shader.

shader_type spatial;
render_mode unshaded, shadows_disabled;
void vertex() {
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix( pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb* (1.0 / 12.92), lessThan(COLOR.rgb,vec3(0.04045)) );
	}
	VERTEX = VERTEX;
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * MODEL_MATRIX * vec4(VERTEX.xyz, 1.0);
	POSITION.z = mix(POSITION.z, POSITION.w, 0.999);
}
void fragment() {
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)");
			selected_mat->set_shader(selected_sh);

			// Register properties in editor settings.
			EDITOR_DEF("editors/3d_gizmos/gizmo_colors/skeleton", Color(1, 0.8, 0.4));
			EDITOR_DEF("editors/3d_gizmos/gizmo_colors/selected_bone", Color(0.8, 0.3, 0.0));
			EDITOR_DEF("editors/3d_gizmos/gizmo_settings/bone_axis_length", (float)0.1);
			EDITOR_DEF("editors/3d_gizmos/gizmo_settings/bone_shape", 1);
			EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "editors/3d_gizmos/gizmo_settings/bone_shape", PROPERTY_HINT_ENUM, "Wire,Octahedron"));
			Node3DEditor::get_singleton()->add_control_to_menu_panel(edit_mode_button);
		} break;
		case EditorNode3DGizmoPlugin::NOTIFICATION_PREDELETE: {
			Node3DEditor::get_singleton()->remove_control_from_menu_panel(edit_mode_button);
		} break;
	}
}
