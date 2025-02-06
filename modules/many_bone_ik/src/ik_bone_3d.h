/**************************************************************************/
/*  ik_bone_3d.h                                                          */
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

#ifndef IK_BONE_3D_H
#define IK_BONE_3D_H

#include "ik_effector_template_3d.h"
#include "ik_kusudama_3d.h"
#include "ik_open_cone_3d.h"
#include "math/ik_node_3d.h"

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "scene/3d/skeleton_3d.h"

class IKEffector3D;
class ManyBoneIK3D;
class IKBone3D;

class IKBone3D : public Resource {
	GDCLASS(IKBone3D, Resource);

	BoneId bone_id = -1;
	Ref<IKBone3D> parent;
	Vector<Ref<IKBone3D>> children;
	Ref<IKEffector3D> pin;

	float default_dampening = Math_PI;
	float dampening = get_parent().is_null() ? Math_PI : default_dampening;
	float cos_half_dampen = Math::cos(dampening / 2.0f);
	double cos_half_return_damp = 0.0f;
	double return_damp = 0.0f;
	Vector<float> cos_half_returnfulness_dampened;
	Vector<float> half_returnfulness_dampened;
	double stiffness = 0.0;
	Ref<IKKusudama3D> constraint;
	// In the space of the local parent bone transform.
	// The origin is the origin of the bone direction transform
	// Can be independent and should be calculated
	// to keep -y to be the opposite of its bone forward orientation
	// To avoid singularity that is ambiguous.
	Ref<IKNode3D> constraint_orientation_transform = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> constraint_twist_transform = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> godot_skeleton_aligned_transform = Ref<IKNode3D>(memnew(IKNode3D())); // The bone's actual transform.
	Ref<IKNode3D> bone_direction_transform = Ref<IKNode3D>(memnew(IKNode3D())); // Physical direction of the bone. Calculate Y is the bone up.

protected:
	static void _bind_methods();

public:
	Vector<float> &get_cos_half_returnfullness_dampened();
	void set_cos_half_returnfullness_dampened(const Vector<float> &p_value);
	Vector<float> &get_half_returnfullness_dampened();
	void set_half_returnfullness_dampened(const Vector<float> &p_value);
	void set_stiffness(double p_stiffness);
	double get_stiffness() const;
	bool is_axially_constrained();
	bool is_orientationally_constrained();
	Transform3D get_bone_direction_global_pose() const;
	Ref<IKNode3D> get_bone_direction_transform();
	void set_bone_direction_transform(Ref<IKNode3D> p_bone_direction);
	void update_default_bone_direction_transform(Skeleton3D *p_skeleton);
	void set_constraint_orientation_transform(Ref<IKNode3D> p_transform);
	Ref<IKNode3D> get_constraint_orientation_transform();
	Ref<IKNode3D> get_constraint_twist_transform();
	void update_default_constraint_transform();
	void add_constraint(Ref<IKKusudama3D> p_constraint);
	Ref<IKKusudama3D> get_constraint() const;
	void set_bone_id(BoneId p_bone_id, Skeleton3D *p_skeleton = nullptr);
	BoneId get_bone_id() const;
	void set_parent(const Ref<IKBone3D> &p_parent);
	Ref<IKBone3D> get_parent() const;
	void set_pin(const Ref<IKEffector3D> &p_pin);
	Ref<IKEffector3D> get_pin() const;
	void set_global_pose(const Transform3D &p_transform);
	Transform3D get_global_pose() const;
	void set_pose(const Transform3D &p_transform);
	Transform3D get_pose() const;
	void set_initial_pose(Skeleton3D *p_skeleton);
	void set_skeleton_bone_pose(Skeleton3D *p_skeleton);
	void create_pin();
	bool is_pinned() const;
	Ref<IKNode3D> get_ik_transform();
	IKBone3D() {}
	IKBone3D(StringName p_bone, Skeleton3D *p_skeleton, const Ref<IKBone3D> &p_parent, Vector<Ref<IKEffectorTemplate3D>> &p_pins, float p_default_dampening = Math_PI, ManyBoneIK3D *p_many_bone_ik = nullptr);
	~IKBone3D() {}
	float get_cos_half_dampen() const;
	void set_cos_half_dampen(float p_cos_half_dampen);
	Transform3D get_parent_bone_aligned_transform();
	Transform3D get_set_constraint_twist_transform() const;
	float calculate_total_radius_sum(const TypedArray<IKLimitCone3D> &p_cones) const;
	Vector3 calculate_weighted_direction(const TypedArray<IKLimitCone3D> &p_cones, float p_total_radius_sum) const;
};

#endif // IK_BONE_3D_H
