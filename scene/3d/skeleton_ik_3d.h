/**************************************************************************/
/*  skeleton_ik_3d.h                                                      */
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

#include "scene/3d/skeleton_modifier_3d.h"

class FabrikInverseKinematic {
	struct EndEffector {
		BoneId tip_bone;
		Transform3D goal_transform;
	};

	struct ChainItem {
		Vector<ChainItem> children;
		ChainItem *parent_item = nullptr;

		// Bone info
		BoneId bone = -1;

		real_t length = 0.0;
		/// Positions relative to root bone
		Transform3D initial_transform;
		Vector3 current_pos;
		// Direction from this bone to child
		Vector3 current_ori;

		ChainItem *find_child(const BoneId p_bone_id);
		ChainItem *add_child(const BoneId p_bone_id);
	};

	struct ChainTip {
		ChainItem *chain_item = nullptr;
		const EndEffector *end_effector = nullptr;

		ChainTip() {}

		ChainTip(ChainItem *p_chain_item, const EndEffector *p_end_effector) :
				chain_item(p_chain_item),
				end_effector(p_end_effector) {}
	};

	struct Chain {
		ChainItem chain_root;
		ChainItem *middle_chain_item = nullptr;
		Vector<ChainTip> tips;
		Vector3 magnet_position;
	};

public:
	struct Task {
		RID self;
		Skeleton3D *skeleton = nullptr;

		Chain chain;

		// Settings
		real_t min_distance = 0.01;
		int max_iterations = 10;

		// Bone data
		BoneId root_bone = -1;
		Vector<EndEffector> end_effectors;

		Transform3D goal_global_transform;

		Task() {}
	};

private:
	/// Init a chain that starts from the root to tip
	static bool build_chain(Task *p_task, bool p_force_simple_chain = true);

	static void solve_simple(Task *p_task, bool p_solve_magnet, Vector3 p_origin_pos);
	/// Special solvers that solve only chains with one end effector
	static void solve_simple_backwards(const Chain &r_chain, bool p_solve_magnet);
	static void solve_simple_forwards(Chain &r_chain, bool p_solve_magnet, Vector3 p_origin_pos);

public:
	static Task *create_simple_task(Skeleton3D *p_sk, BoneId root_bone, BoneId tip_bone, const Transform3D &goal_transform);
	static void free_task(Task *p_task);
	// The goal of chain should be always in local space
	static void set_goal(Task *p_task, const Transform3D &p_goal);
	static void make_goal(Task *p_task, const Transform3D &p_inverse_transf);
	static void solve(Task *p_task, bool override_tip_basis, bool p_use_magnet, const Vector3 &p_magnet_position);

	static void _update_chain(const Skeleton3D *p_skeleton, ChainItem *p_chain_item);
};

class SkeletonIK3D : public SkeletonModifier3D {
	GDCLASS(SkeletonIK3D, SkeletonModifier3D);

	bool internal_active = false;

	StringName root_bone;
	StringName tip_bone;
	Transform3D target;
	NodePath target_node_path_override;
	bool override_tip_basis = true;
	bool use_magnet = false;
	Vector3 magnet_position;

	real_t min_distance = 0.01;
	int max_iterations = 10;

	Variant target_node_override_ref;
	FabrikInverseKinematic::Task *task = nullptr;

#ifndef DISABLE_DEPRECATED
	void _set_interpolation(real_t p_interpolation);
	real_t _get_interpolation() const;
#endif // DISABLE_DEPRECATED

protected:
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();
	virtual void _notification(int p_what);

	virtual void _process_modification() override;

public:
	SkeletonIK3D();
	virtual ~SkeletonIK3D();

	void set_root_bone(const StringName &p_root_bone);
	StringName get_root_bone() const;

	void set_tip_bone(const StringName &p_tip_bone);
	StringName get_tip_bone() const;

	void set_target_transform(const Transform3D &p_target);
	const Transform3D &get_target_transform() const;

	void set_target_node(const NodePath &p_node);
	NodePath get_target_node();

	void set_override_tip_basis(bool p_override);
	bool is_override_tip_basis() const;

	void set_use_magnet(bool p_use);
	bool is_using_magnet() const;

	void set_magnet_position(const Vector3 &p_local_position);
	const Vector3 &get_magnet_position() const;

	void set_min_distance(real_t p_min_distance);
	real_t get_min_distance() const { return min_distance; }

	void set_max_iterations(int p_iterations);
	int get_max_iterations() const { return max_iterations; }

	Skeleton3D *get_parent_skeleton() const;

	bool is_running();

	void start(bool p_one_time = false);
	void stop();

private:
	Transform3D _get_target_transform();
	void reload_chain();
	void reload_goal();
	void _solve_chain();
};
