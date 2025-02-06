/**************************************************************************/
/*  skeleton_modification_2d_jiggle.h                                     */
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
// SkeletonModification2DJIGGLE
///////////////////////////////////////

class SkeletonModification2DJiggle : public SkeletonModification2D {
	GDCLASS(SkeletonModification2DJiggle, SkeletonModification2D);

private:
	struct Jiggle_Joint_Data2D {
		int bone_idx = -1;
		NodePath bone2d_node;
		ObjectID bone2d_node_cache;

		bool override_defaults = false;
		float stiffness = 3;
		float mass = 0.75;
		float damping = 0.75;
		bool use_gravity = false;
		Vector2 gravity = Vector2(0, 6.0);

		Vector2 force = Vector2(0, 0);
		Vector2 acceleration = Vector2(0, 0);
		Vector2 velocity = Vector2(0, 0);
		Vector2 last_position = Vector2(0, 0);
		Vector2 dynamic_position = Vector2(0, 0);

		Vector2 last_noncollision_position = Vector2(0, 0);
	};

	Vector<Jiggle_Joint_Data2D> jiggle_data_chain;

	NodePath target_node;
	ObjectID target_node_cache;
	void update_target_cache();

	float stiffness = 3;
	float mass = 0.75;
	float damping = 0.75;
	bool use_gravity = false;
	Vector2 gravity = Vector2(0, 6);

	bool use_colliders = false;
	uint32_t collision_mask = 1;

	void jiggle_joint_update_bone2d_cache(int p_joint_idx);
	void _execute_jiggle_joint(int p_joint_idx, Node2D *p_target, float p_delta);
	void _update_jiggle_joint_data();

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

	void set_stiffness(float p_stiffness);
	float get_stiffness() const;
	void set_mass(float p_mass);
	float get_mass() const;
	void set_damping(float p_damping);
	float get_damping() const;
	void set_use_gravity(bool p_use_gravity);
	bool get_use_gravity() const;
	void set_gravity(Vector2 p_gravity);
	Vector2 get_gravity() const;

	void set_use_colliders(bool p_use_colliders);
	bool get_use_colliders() const;
	void set_collision_mask(int p_mask);
	int get_collision_mask() const;

	int get_jiggle_data_chain_length();
	void set_jiggle_data_chain_length(int p_new_length);

	void set_jiggle_joint_bone2d_node(int p_joint_idx, const NodePath &p_target_node);
	NodePath get_jiggle_joint_bone2d_node(int p_joint_idx) const;
	void set_jiggle_joint_bone_index(int p_joint_idx, int p_bone_idx);
	int get_jiggle_joint_bone_index(int p_joint_idx) const;

	void set_jiggle_joint_override(int p_joint_idx, bool p_override);
	bool get_jiggle_joint_override(int p_joint_idx) const;
	void set_jiggle_joint_stiffness(int p_joint_idx, float p_stiffness);
	float get_jiggle_joint_stiffness(int p_joint_idx) const;
	void set_jiggle_joint_mass(int p_joint_idx, float p_mass);
	float get_jiggle_joint_mass(int p_joint_idx) const;
	void set_jiggle_joint_damping(int p_joint_idx, float p_damping);
	float get_jiggle_joint_damping(int p_joint_idx) const;
	void set_jiggle_joint_use_gravity(int p_joint_idx, bool p_use_gravity);
	bool get_jiggle_joint_use_gravity(int p_joint_idx) const;
	void set_jiggle_joint_gravity(int p_joint_idx, Vector2 p_gravity);
	Vector2 get_jiggle_joint_gravity(int p_joint_idx) const;

	SkeletonModification2DJiggle();
	~SkeletonModification2DJiggle();
};
