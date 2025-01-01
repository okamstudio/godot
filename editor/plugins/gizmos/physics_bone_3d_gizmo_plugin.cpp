/**************************************************************************/
/*  physics_bone_3d_gizmo_plugin.cpp                                      */
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

#include "physics_bone_3d_gizmo_plugin.h"

#include "editor/editor_settings.h"
#include "editor/plugins/gizmos/joint_3d_gizmo_plugin.h"
#include "scene/3d/physical_bone_simulator_3d.h"
#include "scene/3d/physics/physical_bone_3d.h"

PhysicalBone3DGizmoPlugin::PhysicalBone3DGizmoPlugin() {
	create_material("joint_material", EDITOR_GET("editors/3d_gizmos/gizmo_colors/joint"));
}

bool PhysicalBone3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysicalBone3D>(p_spatial) != nullptr;
}

String PhysicalBone3DGizmoPlugin::get_gizmo_name() const {
	return "PhysicalBone3D";
}

int PhysicalBone3DGizmoPlugin::get_priority() const {
	return -1;
}

void PhysicalBone3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();

	PhysicalBone3D *physical_bone = Object::cast_to<PhysicalBone3D>(p_gizmo->get_node_3d());

	if (!physical_bone) {
		return;
	}

	PhysicalBoneSimulator3D *sm(physical_bone->get_simulator());
	if (!sm) {
		return;
	}

	PhysicalBone3D *pb(sm->get_physical_bone(physical_bone->get_bone_id()));
	if (!pb) {
		return;
	}

	PhysicalBone3D *pbp(sm->get_physical_bone_parent(physical_bone->get_bone_id()));
	if (!pbp) {
		return;
	}

	Vector<Vector3> points;

	switch (physical_bone->get_joint_type()) {
		case PhysicalBone3D::JOINT_TYPE_PIN: {
			Joint3DGizmoPlugin::CreatePinJointGizmo(physical_bone->get_joint_offset(), points);
		} break;
		case PhysicalBone3D::JOINT_TYPE_CONE: {
			const PhysicalBone3D::ConeJointData *cjd(static_cast<const PhysicalBone3D::ConeJointData *>(physical_bone->get_joint_data()));
			Joint3DGizmoPlugin::CreateConeTwistJointGizmo(
					physical_bone->get_joint_offset(),
					physical_bone->get_global_transform() * physical_bone->get_joint_offset(),
					pb->get_global_transform(),
					pbp->get_global_transform(),
					cjd->swing_span,
					cjd->twist_span,
					&points,
					&points);
		} break;
		case PhysicalBone3D::JOINT_TYPE_HINGE: {
			const PhysicalBone3D::HingeJointData *hjd(static_cast<const PhysicalBone3D::HingeJointData *>(physical_bone->get_joint_data()));
			Joint3DGizmoPlugin::CreateHingeJointGizmo(
					physical_bone->get_joint_offset(),
					physical_bone->get_global_transform() * physical_bone->get_joint_offset(),
					pb->get_global_transform(),
					pbp->get_global_transform(),
					hjd->angular_limit_lower,
					hjd->angular_limit_upper,
					hjd->angular_limit_enabled,
					points,
					&points,
					&points);
		} break;
		case PhysicalBone3D::JOINT_TYPE_SLIDER: {
			const PhysicalBone3D::SliderJointData *sjd(static_cast<const PhysicalBone3D::SliderJointData *>(physical_bone->get_joint_data()));
			Joint3DGizmoPlugin::CreateSliderJointGizmo(
					physical_bone->get_joint_offset(),
					physical_bone->get_global_transform() * physical_bone->get_joint_offset(),
					pb->get_global_transform(),
					pbp->get_global_transform(),
					sjd->angular_limit_lower,
					sjd->angular_limit_upper,
					sjd->linear_limit_lower,
					sjd->linear_limit_upper,
					points,
					&points,
					&points);
		} break;
		case PhysicalBone3D::JOINT_TYPE_6DOF: {
			const PhysicalBone3D::SixDOFJointData *sdofjd(static_cast<const PhysicalBone3D::SixDOFJointData *>(physical_bone->get_joint_data()));
			Joint3DGizmoPlugin::CreateGeneric6DOFJointGizmo(
					physical_bone->get_joint_offset(),

					physical_bone->get_global_transform() * physical_bone->get_joint_offset(),
					pb->get_global_transform(),
					pbp->get_global_transform(),

					sdofjd->axis_data[0].angular_limit_lower,
					sdofjd->axis_data[0].angular_limit_upper,
					sdofjd->axis_data[0].linear_limit_lower,
					sdofjd->axis_data[0].linear_limit_upper,
					sdofjd->axis_data[0].angular_limit_enabled,
					sdofjd->axis_data[0].linear_limit_enabled,

					sdofjd->axis_data[1].angular_limit_lower,
					sdofjd->axis_data[1].angular_limit_upper,
					sdofjd->axis_data[1].linear_limit_lower,
					sdofjd->axis_data[1].linear_limit_upper,
					sdofjd->axis_data[1].angular_limit_enabled,
					sdofjd->axis_data[1].linear_limit_enabled,

					sdofjd->axis_data[2].angular_limit_lower,
					sdofjd->axis_data[2].angular_limit_upper,
					sdofjd->axis_data[2].linear_limit_lower,
					sdofjd->axis_data[2].linear_limit_upper,
					sdofjd->axis_data[2].angular_limit_enabled,
					sdofjd->axis_data[2].linear_limit_enabled,

					points,
					&points,
					&points);
		} break;
		default:
			return;
	}

	Ref<Material> material = get_material("joint_material", p_gizmo);

	p_gizmo->add_collision_segments(points);
	p_gizmo->add_lines(points, material);
}
