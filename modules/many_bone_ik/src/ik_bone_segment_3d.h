/**************************************************************************/
/*  ik_bone_segment_3d.h                                                  */
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

#ifndef IK_BONE_SEGMENT_3D_H
#define IK_BONE_SEGMENT_3D_H

#include "ik_bone_3d.h"
#include "ik_effector_3d.h"
#include "ik_effector_template_3d.h"
#include "math/qcp.h"
#include "scene/3d/skeleton_3d.h"

#include "core/io/resource.h"
#include "core/object/ref_counted.h"

class IKEffector3D;
class IKBone3D;
class IKLimitCone3D;

class IKBoneSegment3D : public Resource {
	GDCLASS(IKBoneSegment3D, Resource);
	Ref<IKBone3D> root;
	Ref<IKBone3D> tip;
	Vector<Ref<IKBone3D>> bones;
	Vector<Ref<IKBone3D>> pinned_bones;
	Vector<Ref<IKBoneSegment3D>> child_segments; // Contains only direct child chains that end with effectors or have child that end with effectors
	Ref<IKBoneSegment3D> parent_segment;
	Ref<IKBoneSegment3D> root_segment;
	Vector<Ref<IKEffector3D>> effector_list;
	PackedVector3Array target_headings;
	PackedVector3Array tip_headings;
	PackedVector3Array tip_headings_uniform;
	Vector<double> heading_weights;
	Skeleton3D *skeleton = nullptr;
	bool pinned_descendants = false;
	double previous_deviation = INFINITY;
	int32_t default_stabilizing_pass_count = 0; // Move to the stabilizing pass to the ik solver. Set it free.
	bool _has_pinned_descendants();
	void _enable_pinned_descendants();
	void _update_target_headings(Ref<IKBone3D> p_for_bone, Vector<double> *r_weights, PackedVector3Array *r_htarget);
	void _update_tip_headings(Ref<IKBone3D> p_for_bone, PackedVector3Array *r_heading_tip);
	void _set_optimal_rotation(Ref<IKBone3D> p_for_bone, PackedVector3Array *r_htip, PackedVector3Array *r_heading_tip, Vector<double> *r_weights, float p_dampening = -1, bool p_translate = false, bool p_constraint_mode = false, double current_iteration = 0, double total_iterations = 0);
	void _qcp_solver(const Vector<float> &p_damp, float p_default_damp, bool p_translate, bool p_constraint_mode, int32_t p_current_iteration, int32_t p_total_iterations);
	void _update_optimal_rotation(Ref<IKBone3D> p_for_bone, double p_damp, bool p_translate, bool p_constraint_mode, int32_t current_iteration, int32_t total_iterations);
	float _get_manual_msd(const PackedVector3Array &r_htip, const PackedVector3Array &r_htarget, const Vector<double> &p_weights);
	HashMap<BoneId, Ref<IKBone3D>> bone_map;
	bool _is_parent_of_tip(Ref<IKBone3D> p_current_tip, BoneId p_tip_bone);
	bool _has_multiple_children_or_pinned(Vector<BoneId> &r_children, Ref<IKBone3D> p_current_tip);
	void _process_children(Vector<BoneId> &r_children, Ref<IKBone3D> p_current_tip, Vector<Ref<IKEffectorTemplate3D>> &r_pins, BoneId p_root_bone, BoneId p_tip_bone, ManyBoneIK3D *p_many_bone_ik);
	Ref<IKBoneSegment3D> _create_child_segment(String &p_child_name, Vector<Ref<IKEffectorTemplate3D>> &p_pins, BoneId p_root_bone, BoneId p_tip_bone, ManyBoneIK3D *p_many_bone_ik, Ref<IKBoneSegment3D> &p_parent);
	Ref<IKBone3D> _create_next_bone(BoneId p_bone_id, Ref<IKBone3D> p_current_tip, Vector<Ref<IKEffectorTemplate3D>> &p_pins, ManyBoneIK3D *p_many_bone_ik);
	void _finalize_segment(Ref<IKBone3D> p_current_tip);

protected:
	static void _bind_methods();

public:
	const double evec_prec = static_cast<double>(1E-6);
	void update_pinned_list(Vector<Vector<double>> &r_weights);
	static Quaternion clamp_to_cos_half_angle(Quaternion p_quat, double p_cos_half_angle);
	static void recursive_create_headings_arrays_for(Ref<IKBoneSegment3D> p_bone_segment);
	void create_headings_arrays();
	void recursive_create_penalty_array(Ref<IKBoneSegment3D> p_bone_segment, Vector<Vector<double>> &r_penalty_array, Vector<Ref<IKBone3D>> &r_pinned_bones, double p_falloff);
	void segment_solver(const Vector<float> &p_damp, float p_default_damp, bool p_constraint_mode, int32_t p_current_iteration, int32_t p_total_iteration);
	Ref<IKBone3D> get_root() const;
	Ref<IKBone3D> get_tip() const;
	bool is_pinned() const;
	Vector<Ref<IKBoneSegment3D>> get_child_segments() const;
	void create_bone_list(Vector<Ref<IKBone3D>> &p_list, bool p_recursive = false) const;
	Ref<IKBone3D> get_ik_bone(BoneId p_bone) const;
	void generate_default_segments(Vector<Ref<IKEffectorTemplate3D>> &p_pins, BoneId p_root_bone, BoneId p_tip_bone, ManyBoneIK3D *p_many_bone_ik);
	IKBoneSegment3D() {}
	IKBoneSegment3D(Skeleton3D *p_skeleton, StringName p_root_bone_name, Vector<Ref<IKEffectorTemplate3D>> &p_pins, ManyBoneIK3D *p_many_bone_ik, const Ref<IKBoneSegment3D> &p_parent = nullptr,
			BoneId root = -1, BoneId tip = -1, int32_t p_stabilizing_pass_count = 0);
	~IKBoneSegment3D() {}
};

#endif // IK_BONE_SEGMENT_3D_H
