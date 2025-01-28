/**************************************************************************/
/*  spring_bone_simulator_3d.h                                            */
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

#ifndef SPRING_BONE_SIMULATOR_3D_H
#define SPRING_BONE_SIMULATOR_3D_H

#include "scene/3d/skeleton_modifier_3d.h"

class SpringBoneSimulator3D : public SkeletonModifier3D {
	GDCLASS(SpringBoneSimulator3D, SkeletonModifier3D);

	bool joints_dirty = false;

	LocalVector<ObjectID> collisions; // To process collisions for sync position with skeleton.
	bool collisions_dirty = false;
	void _find_collisions();
	void _process_collisions();
	void _make_collisions_dirty();

public:
	enum BoneDirection {
		BONE_DIRECTION_PLUS_X,
		BONE_DIRECTION_MINUS_X,
		BONE_DIRECTION_PLUS_Y,
		BONE_DIRECTION_MINUS_Y,
		BONE_DIRECTION_PLUS_Z,
		BONE_DIRECTION_MINUS_Z,
		BONE_DIRECTION_FROM_PARENT,
	};

	enum CenterFrom {
		CENTER_FROM_WORLD_ORIGIN,
		CENTER_FROM_NODE,
		CENTER_FROM_BONE,
	};

	enum RotationAxis {
		ROTATION_AXIS_X,
		ROTATION_AXIS_Y,
		ROTATION_AXIS_Z,
		ROTATION_AXIS_ALL,
	};

	struct SpringBone3DVerletInfo {
		Vector3 prev_tail;
		Vector3 current_tail;
		Vector3 forward_vector;
		Quaternion current_rot;
		float length = 0.0;
	};

	struct SpringBone3DJointSetting {
		String bone_name;
		int bone = -1;

		RotationAxis rotation_axis = ROTATION_AXIS_ALL;
		float radius = 0.1;
		float stiffness = 1.0;
		float drag = 0.0;
		float gravity = 0.0;
		Vector3 gravity_direction = Vector3(0, -1, 0);

		// To process.
		SpringBone3DVerletInfo *verlet = nullptr;
	};

	struct SpringBone3DSetting {
		bool joints_dirty = false;

		String root_bone_name;
		int root_bone = -1;

		String end_bone_name;
		int end_bone = -1;

		// To make virtual end joint.
		bool extend_end_bone = false;
		BoneDirection end_bone_direction = BONE_DIRECTION_FROM_PARENT;
		float end_bone_length = 0.0;

		CenterFrom center_from = CENTER_FROM_WORLD_ORIGIN;
		NodePath center_node;
		String center_bone_name;
		int center_bone = -1;

		// Cache into joints.
		bool individual_config = false;
		float radius = 0.02;
		Ref<Curve> radius_damping_curve;
		float stiffness = 1.0;
		Ref<Curve> stiffness_damping_curve;
		float drag = 0.4;
		Ref<Curve> drag_damping_curve;
		float gravity = 0.0;
		Ref<Curve> gravity_damping_curve;
		Vector3 gravity_direction = Vector3(0, -1, 0);
		RotationAxis rotation_axis = ROTATION_AXIS_ALL;
		Vector<SpringBone3DJointSetting *> joints;

		// Cache into collisions.
		bool enable_all_child_collisions = true;
		Vector<NodePath> collisions;
		Vector<NodePath> exclude_collisions;
		LocalVector<ObjectID> cached_collisions;

		// To process.
		bool simulation_dirty = false;
		Transform3D cached_center;
		Transform3D cached_inverted_center;
	};

protected:
	Vector<SpringBone3DSetting *> settings;

	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;

	void _notification(int p_what);

	static void _bind_methods();

	virtual void _set_active(bool p_active) override;
	virtual void _process_modification() override;
	void _init_joints(Skeleton3D *p_skeleton, SpringBone3DSetting *p_setting);
	void _process_joints(double p_delta, Skeleton3D *p_skeleton, Vector<SpringBone3DJointSetting *> &p_joints, const LocalVector<ObjectID> &p_collisions, const Transform3D &p_center_transform, const Transform3D &p_inverted_center_transform, const Quaternion &p_inverted_center_rotation);

	void _make_joints_dirty(int p_index);
	void _make_all_joints_dirty();

	void _update_joint_array(int p_index);
	void _update_joints();

	virtual void add_child_notify(Node *p_child) override;
	virtual void move_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;

	void _validate_rotation_axes(Skeleton3D *p_skeleton) const;
	void _validate_rotation_axis(Skeleton3D *p_skeleton, int p_index, int p_joint) const;

public:
	// Setting.
	void set_root_bone_name(int p_index, const String &p_bone_name);
	String get_root_bone_name(int p_index) const;
	void set_root_bone(int p_index, int p_bone);
	int get_root_bone(int p_index) const;

	void set_end_bone_name(int p_index, const String &p_bone_name);
	String get_end_bone_name(int p_index) const;
	void set_end_bone(int p_index, int p_bone);
	int get_end_bone(int p_index) const;

	void set_extend_end_bone(int p_index, bool p_enabled);
	bool is_end_bone_extended(int p_index) const;
	void set_end_bone_direction(int p_index, BoneDirection p_bone_direction);
	BoneDirection get_end_bone_direction(int p_index) const;
	void set_end_bone_length(int p_index, float p_length);
	float get_end_bone_length(int p_index) const;
	Vector3 get_end_bone_axis(int p_end_bone, BoneDirection p_direction) const; // Helper.

	void set_center_from(int p_index, CenterFrom p_center_from);
	CenterFrom get_center_from(int p_index) const;
	void set_center_node(int p_index, const NodePath &p_node_path);
	NodePath get_center_node(int p_index) const;
	void set_center_bone_name(int p_index, const String &p_bone_name);
	String get_center_bone_name(int p_index) const;
	void set_center_bone(int p_index, int p_bone);
	int get_center_bone(int p_index) const;

	void set_rotation_axis(int p_index, RotationAxis p_axis);
	RotationAxis get_rotation_axis(int p_index) const;
	void set_radius(int p_index, float p_radius);
	float get_radius(int p_index) const;
	void set_radius_damping_curve(int p_index, const Ref<Curve> &p_damping_curve);
	Ref<Curve> get_radius_damping_curve(int p_index) const;
	void set_stiffness(int p_index, float p_stiffness);
	float get_stiffness(int p_index) const;
	void set_stiffness_damping_curve(int p_index, const Ref<Curve> &p_damping_curve);
	Ref<Curve> get_stiffness_damping_curve(int p_index) const;
	void set_drag(int p_index, float p_drag);
	float get_drag(int p_index) const;
	void set_drag_damping_curve(int p_index, const Ref<Curve> &p_damping_curve);
	Ref<Curve> get_drag_damping_curve(int p_index) const;
	void set_gravity(int p_index, float p_gravity);
	float get_gravity(int p_index) const;
	void set_gravity_damping_curve(int p_index, const Ref<Curve> &p_damping_curve);
	Ref<Curve> get_gravity_damping_curve(int p_index) const;
	void set_gravity_direction(int p_index, const Vector3 &p_gravity_direction);
	Vector3 get_gravity_direction(int p_index) const;

	void set_setting_count(int p_count);
	int get_setting_count() const;
	void clear_settings();

	// Individual joints.
	void set_individual_config(int p_index, bool p_enabled);
	bool is_config_individual(int p_index) const;

	void set_joint_bone_name(int p_index, int p_joint, const String &p_bone_name);
	String get_joint_bone_name(int p_index, int p_joint) const;
	void set_joint_bone(int p_index, int p_joint, int p_bone);
	int get_joint_bone(int p_index, int p_joint) const;

	void set_joint_rotation_axis(int p_index, int p_joint, RotationAxis p_axis);
	RotationAxis get_joint_rotation_axis(int p_index, int p_joint) const;
	void set_joint_radius(int p_index, int p_joint, float p_radius);
	float get_joint_radius(int p_index, int p_joint) const;
	void set_joint_stiffness(int p_index, int p_joint, float p_stiffness);
	float get_joint_stiffness(int p_index, int p_joint) const;
	void set_joint_drag(int p_index, int p_joint, float p_drag);
	float get_joint_drag(int p_index, int p_joint) const;
	void set_joint_gravity(int p_index, int p_joint, float p_gravity);
	float get_joint_gravity(int p_index, int p_joint) const;
	void set_joint_gravity_direction(int p_index, int p_joint, const Vector3 &p_gravity_direction);
	Vector3 get_joint_gravity_direction(int p_index, int p_joint) const;

	void set_joint_count(int p_index, int p_count);
	int get_joint_count(int p_index) const;

	// Individual collisions.
	void set_enable_all_child_collisions(int p_index, bool p_enabled);
	bool are_all_child_collisions_enabled(int p_index) const;

	void set_exclude_collision_path(int p_index, int p_collision, const NodePath &p_node_path);
	NodePath get_exclude_collision_path(int p_index, int p_collision) const;

	void set_exclude_collision_count(int p_index, int p_count);
	int get_exclude_collision_count(int p_index) const;
	void clear_exclude_collisions(int p_index);

	void set_collision_path(int p_index, int p_collision, const NodePath &p_node_path);
	NodePath get_collision_path(int p_index, int p_collision) const;

	void set_collision_count(int p_index, int p_count);
	int get_collision_count(int p_index) const;
	void clear_collisions(int p_index);

	LocalVector<ObjectID> get_valid_collision_instance_ids(int p_index);

	// Helper.
	static Quaternion get_local_pose_rotation(Skeleton3D *p_skeleton, int p_bone, const Quaternion &p_global_pose_rotation);
	static Quaternion get_from_to_rotation(const Vector3 &p_from, const Vector3 &p_to, const Quaternion &p_prev_rot);
	static Vector3 snap_position_to_plane(const Transform3D &p_rest, RotationAxis p_axis, const Vector3 &p_position);
	static Vector3 limit_length(const Vector3 &p_origin, const Vector3 &p_destination, float p_length);

	// To process manually.
	void reset();
};

VARIANT_ENUM_CAST(SpringBoneSimulator3D::BoneDirection);
VARIANT_ENUM_CAST(SpringBoneSimulator3D::CenterFrom);
VARIANT_ENUM_CAST(SpringBoneSimulator3D::RotationAxis);

#endif // SPRING_BONE_SIMULATOR_3D_H
