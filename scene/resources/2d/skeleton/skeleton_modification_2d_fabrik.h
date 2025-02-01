/**************************************************************************/
/*  skeleton_modification_2d_fabrik.h                                     */
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

#include "scene/2d/skeleton_2d.h"
#include "scene/resources/2d/skeleton/skeleton_modification_2d.h"

///////////////////////////////////////
// SkeletonModification2DFABRIK
///////////////////////////////////////

class SkeletonModification2DFABRIK : public SkeletonModification2D {
	GDCLASS(SkeletonModification2DFABRIK, SkeletonModification2D);

private:
	struct FABRIK_Joint_Data2D {
		int bone_idx = -1;
		NodePath bone2d_node;
		ObjectID bone2d_node_cache;

		Vector2 magnet_position = Vector2(0, 0);
		bool use_target_rotation = false;

		bool editor_draw_gizmo = true;
	};

	Vector<FABRIK_Joint_Data2D> fabrik_data_chain;

	// Unlike in 3D, we need a vector of Transform2D objects to perform FABRIK.
	// This is because FABRIK (unlike CCDIK) needs to operate on transforms that are NOT
	// affected by each other, making the transforms stored in Bone2D unusable, as well as those in Skeleton2D.
	// For this reason, this modification stores a vector of Transform2Ds used for the calculations, which are then applied at the end.
	Vector<Transform2D> fabrik_transform_chain;

	NodePath target_node;
	ObjectID target_node_cache;
	void update_target_cache();

	float chain_tolarance = 0.01;
	int chain_max_iterations = 10;
	int chain_iterations = 0;
	Transform2D target_global_pose;
	Transform2D origin_global_pose;

	void fabrik_joint_update_bone2d_cache(int p_joint_idx);
	void chain_backwards();
	void chain_forwards();

protected:
	static void _bind_methods();
	bool _set(const StringName &p_path, const Variant &p_value);
	bool _get(const StringName &p_path, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void _execute(float p_delta) override;
	void _setup_modification(SkeletonModificationStack2D *p_stack) override;

	void set_target_node(const NodePath &p_target_node);
	NodePath get_target_node() const;

	int get_fabrik_data_chain_length();
	void set_fabrik_data_chain_length(int p_new_length);

	void set_fabrik_joint_bone2d_node(int p_joint_idx, const NodePath &p_target_node);
	NodePath get_fabrik_joint_bone2d_node(int p_joint_idx) const;
	void set_fabrik_joint_bone_index(int p_joint_idx, int p_bone_idx);
	int get_fabrik_joint_bone_index(int p_joint_idx) const;

	void set_fabrik_joint_magnet_position(int p_joint_idx, Vector2 p_magnet_position);
	Vector2 get_fabrik_joint_magnet_position(int p_joint_idx) const;
	void set_fabrik_joint_use_target_rotation(int p_joint_idx, bool p_use_target_rotation);
	bool get_fabrik_joint_use_target_rotation(int p_joint_idx) const;

	SkeletonModification2DFABRIK();
	~SkeletonModification2DFABRIK();
};
