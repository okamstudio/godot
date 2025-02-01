/**************************************************************************/
/*  physical_bone_3d.h                                                    */
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

#include "scene/3d/physics/physics_body_3d.h"
#include "scene/3d/skeleton_3d.h"

class PhysicalBoneSimulator3D;

class PhysicalBone3D : public PhysicsBody3D {
	GDCLASS(PhysicalBone3D, PhysicsBody3D);

public:
	enum DampMode {
		DAMP_MODE_COMBINE,
		DAMP_MODE_REPLACE,
	};

	enum JointType {
		JOINT_TYPE_NONE,
		JOINT_TYPE_PIN,
		JOINT_TYPE_CONE,
		JOINT_TYPE_HINGE,
		JOINT_TYPE_SLIDER,
		JOINT_TYPE_6DOF
	};

	struct JointData {
		virtual JointType get_joint_type() { return JOINT_TYPE_NONE; }

		/// "j" is used to set the parameter inside the PhysicsServer3D
		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		virtual ~JointData() {}
	};

	struct PinJointData : public JointData {
		virtual JointType get_joint_type() { return JOINT_TYPE_PIN; }

		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		real_t bias = 0.3;
		real_t damping = 1.0;
		real_t impulse_clamp = 0.0;
	};

	struct ConeJointData : public JointData {
		virtual JointType get_joint_type() { return JOINT_TYPE_CONE; }

		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		real_t swing_span = Math_PI * 0.25;
		real_t twist_span = Math_PI;
		real_t bias = 0.3;
		real_t softness = 0.8;
		real_t relaxation = 1.;
	};

	struct HingeJointData : public JointData {
		virtual JointType get_joint_type() { return JOINT_TYPE_HINGE; }

		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		bool angular_limit_enabled = false;
		real_t angular_limit_upper = Math_PI * 0.5;
		real_t angular_limit_lower = -Math_PI * 0.5;
		real_t angular_limit_bias = 0.3;
		real_t angular_limit_softness = 0.9;
		real_t angular_limit_relaxation = 1.;
	};

	struct SliderJointData : public JointData {
		virtual JointType get_joint_type() { return JOINT_TYPE_SLIDER; }

		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		real_t linear_limit_upper = 1.0;
		real_t linear_limit_lower = -1.0;
		real_t linear_limit_softness = 1.0;
		real_t linear_limit_restitution = 0.7;
		real_t linear_limit_damping = 1.0;
		real_t angular_limit_upper = 0.0;
		real_t angular_limit_lower = 0.0;
		real_t angular_limit_softness = 1.0;
		real_t angular_limit_restitution = 0.7;
		real_t angular_limit_damping = 1.0;
	};

	struct SixDOFJointData : public JointData {
		struct SixDOFAxisData {
			bool linear_limit_enabled = true;
			real_t linear_limit_upper = 0.0;
			real_t linear_limit_lower = 0.0;
			real_t linear_limit_softness = 0.7;
			real_t linear_restitution = 0.5;
			real_t linear_damping = 1.0;
			bool linear_spring_enabled = false;
			real_t linear_spring_stiffness = 0.0;
			real_t linear_spring_damping = 0.0;
			real_t linear_equilibrium_point = 0.0;
			bool angular_limit_enabled = true;
			real_t angular_limit_upper = 0.0;
			real_t angular_limit_lower = 0.0;
			real_t angular_limit_softness = 0.5;
			real_t angular_restitution = 0.0;
			real_t angular_damping = 1.0;
			real_t erp = 0.5;
			bool angular_spring_enabled = false;
			real_t angular_spring_stiffness = 0.0;
			real_t angular_spring_damping = 0.0;
			real_t angular_equilibrium_point = 0.0;
		};

		virtual JointType get_joint_type() { return JOINT_TYPE_6DOF; }

		virtual bool _set(const StringName &p_name, const Variant &p_value, RID j);
		virtual bool _get(const StringName &p_name, Variant &r_ret) const;
		virtual void _get_property_list(List<PropertyInfo> *p_list) const;

		SixDOFAxisData axis_data[3];

		SixDOFJointData() {}
	};

private:
#ifdef TOOLS_ENABLED
	// if false gizmo move body
	bool gizmo_move_joint = false;
#endif

	JointData *joint_data = nullptr;
	Transform3D joint_offset;
	RID joint;

	ObjectID simulator_id;
	Transform3D body_offset;
	Transform3D body_offset_inverse;
	bool simulate_physics = false;
	bool _internal_simulate_physics = false;
	int bone_id = -1;

	String bone_name;
	real_t bounce = 0.0;
	real_t mass = 1.0;
	real_t friction = 1.0;
	Vector3 linear_velocity;
	Vector3 angular_velocity;
	real_t gravity_scale = 1.0;
	bool can_sleep = true;

	bool custom_integrator = false;

	DampMode linear_damp_mode = DAMP_MODE_COMBINE;
	DampMode angular_damp_mode = DAMP_MODE_COMBINE;

	real_t linear_damp = 0.0;
	real_t angular_damp = 0.0;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _notification(int p_what);
	GDVIRTUAL1(_integrate_forces, PhysicsDirectBodyState3D *)
	static void _body_state_changed_callback(void *p_instance, PhysicsDirectBodyState3D *p_state);
	void _body_state_changed(PhysicsDirectBodyState3D *p_state);

	static void _bind_methods();

private:
	void _sync_body_state(PhysicsDirectBodyState3D *p_state);

	void _update_joint_offset();
	void _fix_joint_offset();
	void _reload_joint();

	void _update_simulator_path();

public:
	void _on_bone_parent_changed();

	PhysicalBoneSimulator3D *get_simulator() const;
	Skeleton3D *get_skeleton() const;

	void set_linear_velocity(const Vector3 &p_velocity);
	Vector3 get_linear_velocity() const override;

	void set_angular_velocity(const Vector3 &p_velocity);
	Vector3 get_angular_velocity() const override;

	void set_use_custom_integrator(bool p_enable);
	bool is_using_custom_integrator();

#ifdef TOOLS_ENABLED
	void _set_gizmo_move_joint(bool p_move_joint);
	virtual Transform3D get_global_gizmo_transform() const override;
	virtual Transform3D get_local_gizmo_transform() const override;
#endif

	const JointData *get_joint_data() const;

	int get_bone_id() const {
		return bone_id;
	}

	void set_joint_type(JointType p_joint_type);
	JointType get_joint_type() const;

	void set_joint_offset(const Transform3D &p_offset);
	const Transform3D &get_joint_offset() const;

	void set_joint_rotation(const Vector3 &p_euler_rad);
	Vector3 get_joint_rotation() const;

	void set_body_offset(const Transform3D &p_offset);
	const Transform3D &get_body_offset() const;

	void set_simulate_physics(bool p_simulate);
	bool get_simulate_physics();
	bool is_simulating_physics();

	void set_bone_name(const String &p_name);
	const String &get_bone_name() const;

	void set_mass(real_t p_mass);
	real_t get_mass() const;

	void set_friction(real_t p_friction);
	real_t get_friction() const;

	void set_bounce(real_t p_bounce);
	real_t get_bounce() const;

	void set_gravity_scale(real_t p_gravity_scale);
	real_t get_gravity_scale() const;

	void set_linear_damp_mode(DampMode p_mode);
	DampMode get_linear_damp_mode() const;

	void set_angular_damp_mode(DampMode p_mode);
	DampMode get_angular_damp_mode() const;

	void set_linear_damp(real_t p_linear_damp);
	real_t get_linear_damp() const;

	void set_angular_damp(real_t p_angular_damp);
	real_t get_angular_damp() const;

	void set_can_sleep(bool p_active);
	bool is_able_to_sleep() const;

	void apply_central_impulse(const Vector3 &p_impulse);
	void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position = Vector3());

	void reset_physics_simulation_state();
	void reset_to_rest_position();

	PhysicalBone3D();
	~PhysicalBone3D();

private:
	void update_bone_id();
	void update_offset();

	void _start_physics_simulation();
	void _stop_physics_simulation();
};

VARIANT_ENUM_CAST(PhysicalBone3D::JointType);
VARIANT_ENUM_CAST(PhysicalBone3D::DampMode);
