/**************************************************************************/
/*  spring_bone_3d_gizmo_plugin.cpp                                       */
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

#include "spring_bone_3d_gizmo_plugin.h"

#include "editor/editor_settings.h"
#include "scene/3d/spring_bone_collision_capsule_3d.h"
#include "scene/3d/spring_bone_collision_plane_3d.h"
#include "scene/3d/spring_bone_collision_sphere_3d.h"

// SpringBoneSimulator3D

SpringBoneSimulator3DGizmoPlugin::SelectionMaterials SpringBoneSimulator3DGizmoPlugin::selection_materials;

SpringBoneSimulator3DGizmoPlugin::SpringBoneSimulator3DGizmoPlugin() {
	selection_materials.unselected_mat.instantiate();
	selection_materials.unselected_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	selection_materials.unselected_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

	selection_materials.selected_mat.instantiate();
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(R"(
// Skeleton 3D gizmo bones shader.

shader_type spatial;
render_mode unshaded, shadows_disabled;
void vertex() {
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix( pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb* (1.0 / 12.92), lessThan(COLOR.rgb,vec3(0.04045)) );
	}
	VERTEX = VERTEX;
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * MODEL_MATRIX * vec4(VERTEX.xyz, 1.0);
	POSITION.z = mix(POSITION.z, POSITION.w, 0.998);
}
void fragment() {
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)");
	selection_materials.selected_mat->set_shader(sh);
}

SpringBoneSimulator3DGizmoPlugin::~SpringBoneSimulator3DGizmoPlugin() {
	selection_materials.unselected_mat.unref();
	selection_materials.selected_mat.unref();
}

bool SpringBoneSimulator3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<SpringBoneSimulator3D>(p_spatial) != nullptr;
}

String SpringBoneSimulator3DGizmoPlugin::get_gizmo_name() const {
	return "SpringBoneSimulator3D";
}

int SpringBoneSimulator3DGizmoPlugin::get_priority() const {
	return -1;
}

void SpringBoneSimulator3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	SpringBoneSimulator3D *simulator = Object::cast_to<SpringBoneSimulator3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	if (!simulator->get_setting_count()) {
		return;
	}

	Skeleton3D *skeleton = simulator->get_skeleton();
	if (!skeleton) {
		return;
	}

	Ref<ArrayMesh> mesh = get_joints_mesh(skeleton, simulator, p_gizmo->is_selected());
	Transform3D skel_tr = simulator->get_global_transform().inverse() * skeleton->get_global_transform();
	p_gizmo->add_mesh(mesh, Ref<Material>(), skel_tr, skeleton->register_skin(skeleton->create_skin_from_rest_transforms()));
}

Ref<ArrayMesh> SpringBoneSimulator3DGizmoPlugin::get_joints_mesh(Skeleton3D *p_skeleton, SpringBoneSimulator3D *p_simulator, bool p_is_selected) {
	Color bone_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/spring_bone_joint");

	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->begin(Mesh::PRIMITIVE_LINES);

	if (p_is_selected) {
		surface_tool->set_material(selection_materials.selected_mat);
	} else {
		selection_materials.unselected_mat->set_albedo(bone_color);
		surface_tool->set_material(selection_materials.unselected_mat);
	}

	LocalVector<int> bones;
	LocalVector<float> weights;
	bones.resize(4);
	weights.resize(4);
	for (int i = 0; i < 4; i++) {
		bones[i] = 0;
		weights[i] = 0;
	}
	weights[0] = 1;

	for (int i = 0; i < p_simulator->get_setting_count(); i++) {
		int current_bone = -1;
		int prev_bone = -1;
		int joint_end = p_simulator->get_joint_count(i) - 1;
		for (int j = 0; j <= joint_end; j++) {
			current_bone = p_simulator->get_joint_bone(i, j);
			if (j > 0) {
				Transform3D parent_global_pose = p_skeleton->get_bone_global_rest(prev_bone);
				Transform3D global_pose = p_skeleton->get_bone_global_rest(current_bone);
				draw_line(surface_tool, parent_global_pose.origin, global_pose.origin, bone_color);
				draw_sphere(surface_tool, global_pose.basis, global_pose.origin, p_simulator->get_joint_radius(i, j - 1), bone_color);
			}
			if (j == joint_end && p_simulator->is_end_bone_extended(i) && p_simulator->get_end_bone_length(i) > 0) {
				Vector3 axis = p_simulator->get_end_bone_axis(current_bone, p_simulator->get_end_bone_direction(i));
				if (axis.is_zero_approx()) {
					continue;
				}
				bones[0] = current_bone;
				surface_tool->set_bones(bones);
				surface_tool->set_weights(weights);
				Transform3D global_pose = p_skeleton->get_bone_global_rest(current_bone);
				axis = global_pose.xform(axis * p_simulator->get_end_bone_length(i));
				draw_line(surface_tool, global_pose.origin, axis, bone_color);
				draw_sphere(surface_tool, global_pose.basis, axis, p_simulator->get_joint_radius(i, j), bone_color);
			} else {
				bones[0] = current_bone;
				surface_tool->set_bones(bones);
				surface_tool->set_weights(weights);
			}
			prev_bone = current_bone;
		}
	}

	return surface_tool->commit();
}

void SpringBoneSimulator3DGizmoPlugin::draw_sphere(Ref<SurfaceTool> &p_surface_tool, const Basis &p_basis, const Vector3 &p_center, float p_radius, const Color &p_color) {
	static const Vector3 VECTOR3_RIGHT = Vector3(1, 0, 0);
	static const Vector3 VECTOR3_UP = Vector3(0, 1, 0);
	static const Vector3 VECTOR3_FORWARD = Vector3(0, 0, 1);
	static const int STEP = 16;
	static const float SPPI = Math_TAU / (float)STEP;

	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_UP * p_radius)).rotated(p_basis.xform(VECTOR3_RIGHT), SPPI * ((i - 1) % STEP))));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_UP * p_radius)).rotated(p_basis.xform(VECTOR3_RIGHT), SPPI * (i % STEP))));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_RIGHT * p_radius)).rotated(p_basis.xform(VECTOR3_FORWARD), SPPI * ((i - 1) % STEP))));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_RIGHT * p_radius)).rotated(p_basis.xform(VECTOR3_FORWARD), SPPI * (i % STEP))));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_FORWARD * p_radius)).rotated(p_basis.xform(VECTOR3_UP), SPPI * ((i - 1) % STEP))));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(p_center + ((p_basis.xform(VECTOR3_FORWARD * p_radius)).rotated(p_basis.xform(VECTOR3_UP), SPPI * (i % STEP))));
	}
}

void SpringBoneSimulator3DGizmoPlugin::draw_line(Ref<SurfaceTool> &p_surface_tool, const Vector3 &p_begin_pos, const Vector3 &p_end_pos, const Color &p_color) {
	p_surface_tool->set_color(p_color);
	p_surface_tool->add_vertex(p_begin_pos);
	p_surface_tool->set_color(p_color);
	p_surface_tool->add_vertex(p_end_pos);
}

// SpringBoneCollision3D

SpringBoneCollision3DGizmoPlugin::SelectionMaterials SpringBoneCollision3DGizmoPlugin::selection_materials;

SpringBoneCollision3DGizmoPlugin::SpringBoneCollision3DGizmoPlugin() {
	selection_materials.unselected_mat.instantiate();
	selection_materials.unselected_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	selection_materials.unselected_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	selection_materials.unselected_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

	selection_materials.selected_mat.instantiate();
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(R"(
// Skeleton 3D gizmo bones shader.

shader_type spatial;
render_mode unshaded, shadows_disabled;
void vertex() {
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix( pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), COLOR.rgb* (1.0 / 12.92), lessThan(COLOR.rgb,vec3(0.04045)) );
	}
	VERTEX = VERTEX;
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * MODEL_MATRIX * vec4(VERTEX.xyz, 1.0);
	POSITION.z = mix(POSITION.z, POSITION.w, 0.998);
}
void fragment() {
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)");
	selection_materials.selected_mat->set_shader(sh);
}

SpringBoneCollision3DGizmoPlugin::~SpringBoneCollision3DGizmoPlugin() {
	selection_materials.unselected_mat.unref();
	selection_materials.selected_mat.unref();
}

bool SpringBoneCollision3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<SpringBoneCollision3D>(p_spatial) != nullptr;
}

String SpringBoneCollision3DGizmoPlugin::get_gizmo_name() const {
	return "SpringBoneCollision3D";
}

int SpringBoneCollision3DGizmoPlugin::get_priority() const {
	return -1;
}

void SpringBoneCollision3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	SpringBoneCollision3D *collision = Object::cast_to<SpringBoneCollision3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	Ref<ArrayMesh> mesh = get_collision_mesh(collision, p_gizmo->is_selected());
	p_gizmo->add_mesh(mesh);
}

Ref<ArrayMesh> SpringBoneCollision3DGizmoPlugin::get_collision_mesh(SpringBoneCollision3D *p_collision, bool p_is_selected) {
	Color collision_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/spring_bone_collision");
	Color inside_collision_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/spring_bone_inside_collision");

	Ref<SurfaceTool> surface_tool;
	surface_tool.instantiate();
	surface_tool->begin(Mesh::PRIMITIVE_LINES);

	if (p_is_selected) {
		surface_tool->set_material(selection_materials.selected_mat);
	} else {
		selection_materials.unselected_mat->set_albedo(collision_color);
		surface_tool->set_material(selection_materials.unselected_mat);
	}

	SpringBoneCollisionSphere3D *sphere = Object::cast_to<SpringBoneCollisionSphere3D>(p_collision);
	if (sphere) {
		draw_sphere(surface_tool, sphere->get_radius(), sphere->is_inside() ? inside_collision_color : collision_color);
		return surface_tool->commit();
	}

	SpringBoneCollisionCapsule3D *capsule = Object::cast_to<SpringBoneCollisionCapsule3D>(p_collision);
	if (capsule) {
		draw_capsule(surface_tool, capsule->get_radius(), capsule->get_height(), capsule->is_inside() ? inside_collision_color : collision_color);
		return surface_tool->commit();
	}

	SpringBoneCollisionPlane3D *plane = Object::cast_to<SpringBoneCollisionPlane3D>(p_collision);
	if (plane) {
		draw_plane(surface_tool, collision_color);
		return surface_tool->commit();
	}

	return surface_tool->commit();
}

void SpringBoneCollision3DGizmoPlugin::draw_sphere(Ref<SurfaceTool> &p_surface_tool, float p_radius, const Color &p_color) {
	static const Vector3 VECTOR3_RIGHT = Vector3(1, 0, 0);
	static const Vector3 VECTOR3_UP = Vector3(0, 1, 0);
	static const Vector3 VECTOR3_FORWARD = Vector3(0, 0, 1);
	static const int STEP = 16;
	static const float SPPI = Math_TAU / (float)STEP;

	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_UP * p_radius).rotated(VECTOR3_RIGHT, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_UP * p_radius).rotated(VECTOR3_RIGHT, SPPI * (i % STEP)));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_RIGHT * p_radius).rotated(VECTOR3_FORWARD, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_RIGHT * p_radius).rotated(VECTOR3_FORWARD, SPPI * (i % STEP)));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * (i % STEP)));
	}
}

void SpringBoneCollision3DGizmoPlugin::draw_capsule(Ref<SurfaceTool> &p_surface_tool, float p_radius, float p_height, const Color &p_color) {
	static const Vector3 VECTOR3_RIGHT = Vector3(1, 0, 0);
	static const Vector3 VECTOR3_UP = Vector3(0, 1, 0);
	static const Vector3 VECTOR3_FORWARD = Vector3(0, 0, 1);
	static const int STEP = 16;
	static const int HALF_STEP = 8;
	static const float SPPI = Math_TAU / (float)STEP;
	static const float HALF_PI = Math_PI * 0.5;

	Vector3 top = VECTOR3_UP * (p_height * 0.5 - p_radius);
	Vector3 bottom = -top;

	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((i - 1 < HALF_STEP ? top : bottom) + (VECTOR3_UP * p_radius).rotated(VECTOR3_RIGHT, -HALF_PI + SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((i - 1 < HALF_STEP ? top : bottom) + (VECTOR3_UP * p_radius).rotated(VECTOR3_RIGHT, -HALF_PI + SPPI * (i % STEP)));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((i - 1 < HALF_STEP ? top : bottom) + (VECTOR3_RIGHT * p_radius).rotated(VECTOR3_FORWARD, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex((i - 1 < HALF_STEP ? top : bottom) + (VECTOR3_RIGHT * p_radius).rotated(VECTOR3_FORWARD, SPPI * (i % STEP)));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(top + (VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(top + (VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * (i % STEP)));
	}
	for (int i = 1; i <= STEP; i++) {
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(bottom + (VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * ((i - 1) % STEP)));
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(bottom + (VECTOR3_FORWARD * p_radius).rotated(VECTOR3_UP, SPPI * (i % STEP)));
	}
	LocalVector<Vector3> directions;
	directions.resize(4);
	directions[0] = VECTOR3_RIGHT;
	directions[1] = -VECTOR3_RIGHT;
	directions[2] = VECTOR3_FORWARD;
	directions[3] = -VECTOR3_FORWARD;
	for (int i = 0; i < 4; i++) {
		Vector3 dir = directions[i] * p_radius;
		p_surface_tool->set_color(p_color);
		p_surface_tool->add_vertex(top + dir);
		p_surface_tool->add_vertex(bottom + dir);
	}
}

void SpringBoneCollision3DGizmoPlugin::draw_plane(Ref<SurfaceTool> &p_surface_tool, const Color &p_color) {
	static const Vector3 VECTOR3_UP = Vector3(0, 1, 0);
	static const float HALF_PI = Math_PI * 0.5;
	static const float ARROW_LENGTH = 0.3;
	static const float ARROW_HALF_WIDTH = 0.05;
	static const float ARROW_TOP_HALF_WIDTH = 0.1;
	static const float ARROW_TOP = 0.5;
	static const float RECT_SIZE = 1.0;
	static const int RECT_STEP_COUNT = 9;
	static const float RECT_HALF_SIZE = RECT_SIZE * 0.5;
	static const float RECT_STEP = RECT_SIZE / (float)RECT_STEP_COUNT;

	p_surface_tool->set_color(p_color);

	// Draw arrow of the normal.
	LocalVector<Vector3> arrow;
	arrow.resize(7);
	arrow[0] = Vector3(0, ARROW_TOP, 0);
	arrow[1] = Vector3(-ARROW_TOP_HALF_WIDTH, ARROW_LENGTH, 0);
	arrow[2] = Vector3(-ARROW_HALF_WIDTH, ARROW_LENGTH, 0);
	arrow[3] = Vector3(-ARROW_HALF_WIDTH, 0, 0);
	arrow[4] = Vector3(ARROW_HALF_WIDTH, 0, 0);
	arrow[5] = Vector3(ARROW_HALF_WIDTH, ARROW_LENGTH, 0);
	arrow[6] = Vector3(ARROW_TOP_HALF_WIDTH, ARROW_LENGTH, 0);
	for (int i = 0; i < 2; i++) {
		Basis ma(VECTOR3_UP, HALF_PI * i);
		for (uint32_t j = 0; j < arrow.size(); j++) {
			Vector3 v1 = arrow[j];
			Vector3 v2 = arrow[(j + 1) % arrow.size()];
			p_surface_tool->add_vertex(ma.xform(v1));
			p_surface_tool->add_vertex(ma.xform(v2));
		}
	}

	// Draw dashed line of the rect.
	for (int i = 0; i < 4; i++) {
		Basis ma(VECTOR3_UP, HALF_PI * i);
		for (int j = 0; j < RECT_STEP_COUNT; j++) {
			if (j % 2 == 1) {
				continue;
			}
			Vector3 v1 = Vector3(RECT_HALF_SIZE, 0, RECT_HALF_SIZE - RECT_STEP * j);
			Vector3 v2 = Vector3(RECT_HALF_SIZE, 0, RECT_HALF_SIZE - RECT_STEP * (j + 1));
			p_surface_tool->add_vertex(ma.xform(v1));
			p_surface_tool->add_vertex(ma.xform(v2));
		}
	}
}
