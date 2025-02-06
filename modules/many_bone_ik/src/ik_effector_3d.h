/**************************************************************************/
/*  ik_effector_3d.h                                                      */
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

#ifndef IK_EFFECTOR_3D_H
#define IK_EFFECTOR_3D_H

#include "math/ik_node_3d.h"

#include "core/object/ref_counted.h"
#include "scene/3d/skeleton_3d.h"

#define MIN_SCALE 0.1

class ManyBoneIK3D;
class IKBone3D;

class IKEffector3D : public Resource {
	GDCLASS(IKEffector3D, Resource);
	friend class IKBone3D;
	friend class IKBoneSegment3D;

	Ref<IKBone3D> for_bone;
	bool use_target_node_rotation = true;
	NodePath target_node_path;
	ObjectID target_node_cache;
	Node *target_node_reference = nullptr;
	bool target_static = false;
	Transform3D target_transform;

	Transform3D target_relative_to_skeleton_origin;
	int32_t num_headings = 7;
	// See IKEffectorTemplate to change the defaults.
	real_t weight = 0.0;
	real_t motion_propagation_factor = 0.0;
	PackedVector3Array target_headings;
	PackedVector3Array tip_headings;
	Vector<real_t> heading_weights;
	Vector3 direction_priorities;

protected:
	static void _bind_methods();

public:
	IKEffector3D() = default;
	void set_weight(real_t p_weight);
	real_t get_weight() const;
	void set_direction_priorities(Vector3 p_direction_priorities);
	Vector3 get_direction_priorities() const;
	void update_target_global_transform(Skeleton3D *p_skeleton, ManyBoneIK3D *p_modification = nullptr);
	const float MAX_KUSUDAMA_OPEN_CONES = 30;
	float get_motion_propagation_factor() const;
	void set_motion_propagation_factor(float p_motion_propagation_factor);
	void set_target_node(Skeleton3D *p_skeleton, const NodePath &p_target_node_path);
	NodePath get_target_node() const;
	Transform3D get_target_global_transform() const;
	void set_target_node_rotation(bool p_use);
	bool get_target_node_rotation() const;
	Ref<IKBone3D> get_ik_bone_3d() const;
	bool is_following_translation_only() const;
	int32_t update_effector_target_headings(PackedVector3Array *p_headings, int32_t p_index, Ref<IKBone3D> p_for_bone, const Vector<double> *p_weights) const;
	int32_t update_effector_tip_headings(PackedVector3Array *p_headings, int32_t p_index, Ref<IKBone3D> p_for_bone) const;
	IKEffector3D(const Ref<IKBone3D> &p_current_bone);
};

#endif // IK_EFFECTOR_3D_H
