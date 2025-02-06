/**************************************************************************/
/*  particle_process_material.h                                           */
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

#include "core/templates/rid.h"
#include "core/templates/self_list.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/material.h"

/*
 TODO:
-Path following
-Emitter positions deformable by bones
-Proper trails
*/

class ParticleProcessMaterial : public Material {
	GDCLASS(ParticleProcessMaterial, Material);

public:
	enum Parameter {
		PARAM_INITIAL_LINEAR_VELOCITY,
		PARAM_ANGULAR_VELOCITY,
		PARAM_ORBIT_VELOCITY,
		PARAM_LINEAR_ACCEL,
		PARAM_RADIAL_ACCEL,
		PARAM_TANGENTIAL_ACCEL,
		PARAM_DAMPING,
		PARAM_ANGLE,
		PARAM_SCALE,
		PARAM_HUE_VARIATION,
		PARAM_ANIM_SPEED,
		PARAM_ANIM_OFFSET,
		PARAM_TURB_INFLUENCE_OVER_LIFE,
		PARAM_TURB_VEL_INFLUENCE,
		PARAM_TURB_INIT_DISPLACEMENT,
		PARAM_RADIAL_VELOCITY,
		PARAM_DIRECTIONAL_VELOCITY,
		PARAM_SCALE_OVER_VELOCITY,
		PARAM_MAX
	};

	// When extending, make sure not to overflow the size of the MaterialKey below.
	enum ParticleFlags {
		PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY,
		PARTICLE_FLAG_ROTATE_Y,
		PARTICLE_FLAG_DISABLE_Z,
		PARTICLE_FLAG_DAMPING_AS_FRICTION,
		PARTICLE_FLAG_MAX
	};

	// When extending, make sure not to overflow the size of the MaterialKey below.
	enum EmissionShape {
		EMISSION_SHAPE_POINT,
		EMISSION_SHAPE_SPHERE,
		EMISSION_SHAPE_SPHERE_SURFACE,
		EMISSION_SHAPE_BOX,
		EMISSION_SHAPE_POINTS,
		EMISSION_SHAPE_DIRECTED_POINTS,
		EMISSION_SHAPE_RING,
		EMISSION_SHAPE_MAX
	};

	// When extending, make sure not to overflow the size of the MaterialKey below.
	enum SubEmitterMode {
		SUB_EMITTER_DISABLED,
		SUB_EMITTER_CONSTANT,
		SUB_EMITTER_AT_END,
		SUB_EMITTER_AT_COLLISION,
		SUB_EMITTER_AT_START,
		SUB_EMITTER_MAX
	};

	// When extending, make sure not to overflow the size of the MaterialKey below.
	enum CollisionMode {
		COLLISION_DISABLED,
		COLLISION_RIGID,
		COLLISION_HIDE_ON_CONTACT,
		COLLISION_MAX
	};

private:
	struct MaterialKey {
		// The bit size of the struct must be kept below or equal to 64 bits.
		// Consider this when extending ParticleFlags, EmissionShape, or SubEmitterMode.
		uint64_t texture_mask : PARAM_MAX;
		uint64_t texture_color : 1;
		uint64_t particle_flags : PARTICLE_FLAG_MAX;
		uint64_t emission_shape : 3;
		uint64_t invalid_key : 1;
		uint64_t has_emission_color : 1;
		uint64_t sub_emitter : 3;
		uint64_t attractor_enabled : 1;
		uint64_t collision_mode : 2;
		uint64_t collision_scale : 1;
		uint64_t turbulence_enabled : 1;
		uint64_t limiter_curve : 1;
		uint64_t alpha_curve : 1;
		uint64_t emission_curve : 1;
		uint64_t has_initial_ramp : 1;
		uint64_t orbit_uses_curve_xyz : 1;

		MaterialKey() {
			memset(this, 0, sizeof(MaterialKey));
		}

		static uint32_t hash(const MaterialKey &p_key) {
			return hash_djb2_buffer((const uint8_t *)&p_key, sizeof(MaterialKey));
		}
		bool operator==(const MaterialKey &p_key) const {
			return memcmp(this, &p_key, sizeof(MaterialKey)) == 0;
		}
		bool operator<(const MaterialKey &p_key) const {
			return memcmp(this, &p_key, sizeof(MaterialKey)) < 0;
		}
	};

	struct ShaderData {
		RID shader;
		int users = 0;
	};

	static HashMap<MaterialKey, ShaderData, MaterialKey> shader_map;
	static RBSet<String> min_max_properties;

	MaterialKey current_key;

	_FORCE_INLINE_ MaterialKey _compute_key() const {
		MaterialKey mk;

		mk.texture_color = color_ramp.is_valid() ? 1 : 0;
		mk.emission_shape = emission_shape;
		mk.has_emission_color = emission_shape >= EMISSION_SHAPE_POINTS && emission_color_texture.is_valid();
		mk.sub_emitter = sub_emitter_mode;
		mk.collision_mode = collision_mode;
		mk.attractor_enabled = attractor_interaction_enabled;
		mk.collision_scale = collision_scale;
		mk.turbulence_enabled = turbulence_enabled;
		mk.limiter_curve = velocity_limit_curve.is_valid() ? 1 : 0;
		mk.alpha_curve = alpha_curve.is_valid() ? 1 : 0;
		mk.emission_curve = emission_curve.is_valid() ? 1 : 0;
		mk.has_initial_ramp = color_initial_ramp.is_valid() ? 1 : 0;
		CurveXYZTexture *texture = Object::cast_to<CurveXYZTexture>(tex_parameters[PARAM_ORBIT_VELOCITY].ptr());
		mk.orbit_uses_curve_xyz = texture ? 1 : 0;

		for (int i = 0; i < PARAM_MAX; i++) {
			if (tex_parameters[i].is_valid()) {
				mk.texture_mask |= ((uint64_t)1 << i);
			}
		}
		for (int i = 0; i < PARTICLE_FLAG_MAX; i++) {
			if (particle_flags[i]) {
				mk.particle_flags |= ((uint64_t)1 << i);
			}
		}

		return mk;
	}

	static Mutex material_mutex;
	static SelfList<ParticleProcessMaterial>::List dirty_materials;

	struct ShaderNames {
		StringName direction;
		StringName spread;
		StringName flatness;
		StringName initial_linear_velocity_min;
		StringName initial_angle_min;
		StringName angular_velocity_min;
		StringName orbit_velocity_min;
		StringName radial_velocity_min;
		StringName linear_accel_min;
		StringName radial_accel_min;
		StringName tangent_accel_min;
		StringName damping_min;
		StringName scale_min;
		StringName scale_over_velocity_min;
		StringName hue_variation_min;
		StringName anim_speed_min;
		StringName anim_offset_min;
		StringName directional_velocity_min;

		StringName initial_linear_velocity_max;
		StringName initial_angle_max;
		StringName angular_velocity_max;
		StringName orbit_velocity_max;
		StringName radial_velocity_max;
		StringName linear_accel_max;
		StringName radial_accel_max;
		StringName tangent_accel_max;
		StringName damping_max;
		StringName scale_max;
		StringName scale_over_velocity_max;
		StringName hue_variation_max;
		StringName anim_speed_max;
		StringName anim_offset_max;
		StringName directional_velocity_max;

		StringName angle_texture;
		StringName angular_velocity_texture;
		StringName orbit_velocity_texture;
		StringName radial_velocity_texture;
		StringName linear_accel_texture;
		StringName radial_accel_texture;
		StringName tangent_accel_texture;
		StringName damping_texture;
		StringName scale_texture;
		StringName scale_over_velocity_texture;
		StringName hue_variation_texture;
		StringName anim_speed_texture;
		StringName anim_offset_texture;
		StringName velocity_limiter_texture;
		StringName directional_velocity_texture;

		StringName color;
		StringName color_ramp;
		StringName alpha_ramp;
		StringName emission_ramp;
		StringName color_initial_ramp;

		StringName velocity_limit_curve;
		StringName velocity_pivot;

		StringName emission_sphere_radius;
		StringName emission_box_extents;
		StringName emission_texture_point_count;
		StringName emission_texture_points;
		StringName emission_texture_normal;
		StringName emission_texture_color;
		StringName emission_ring_axis;
		StringName emission_ring_height;
		StringName emission_ring_radius;
		StringName emission_ring_inner_radius;
		StringName emission_ring_cone_angle;
		StringName emission_shape_offset;
		StringName emission_shape_scale;

		StringName turbulence_enabled;
		StringName turbulence_noise_strength;
		StringName turbulence_noise_scale;
		StringName turbulence_noise_speed;
		StringName turbulence_noise_speed_random;
		StringName turbulence_influence_over_life;
		StringName turbulence_influence_min;
		StringName turbulence_influence_max;
		StringName turbulence_initial_displacement_min;
		StringName turbulence_initial_displacement_max;

		StringName gravity;
		StringName inherit_emitter_velocity_ratio;

		StringName lifetime_randomness;

		StringName sub_emitter_frequency;
		StringName sub_emitter_amount_at_end;
		StringName sub_emitter_amount_at_collision;
		StringName sub_emitter_amount_at_start;
		StringName sub_emitter_keep_velocity;

		StringName collision_friction;
		StringName collision_bounce;
	};

	static ShaderNames *shader_names;

	SelfList<ParticleProcessMaterial> element;

	void _update_shader();
	_FORCE_INLINE_ void _queue_shader_change();

	Vector3 direction;
	float spread = 0.0f;
	float flatness = 0.0f;

	float params_min[PARAM_MAX] = {};
	float params_max[PARAM_MAX] = {};
	float params[PARAM_MAX] = {};

	Ref<Texture2D> tex_parameters[PARAM_MAX];
	Color color;
	Ref<Texture2D> color_ramp;
	Ref<Texture2D> alpha_curve;
	Ref<Texture2D> emission_curve;
	Ref<Texture2D> color_initial_ramp;
	Ref<Texture2D> velocity_limit_curve;

	bool directional_velocity_global = false;
	Vector3 velocity_pivot;

	bool particle_flags[PARTICLE_FLAG_MAX];

	EmissionShape emission_shape;
	float emission_sphere_radius = 0.0f;
	Vector3 emission_box_extents;
	Ref<Texture2D> emission_point_texture;
	Ref<Texture2D> emission_normal_texture;
	Ref<Texture2D> emission_color_texture;
	Vector3 emission_ring_axis;
	real_t emission_ring_height = 0.0f;
	real_t emission_ring_radius = 0.0f;
	real_t emission_ring_inner_radius = 0.0f;
	real_t emission_ring_cone_angle = 0.0f;
	int emission_point_count = 1;
	Vector3 emission_shape_offset;
	Vector3 emission_shape_scale;

	bool anim_loop = false;

	bool turbulence_enabled;
	Vector3 turbulence_noise_speed;
	Ref<Texture2D> turbulence_color_ramp;
	float turbulence_noise_strength = 0.0f;
	float turbulence_noise_scale = 0.0f;
	float turbulence_noise_speed_random = 0.0f;

	Vector3 gravity;

	double lifetime_randomness = 0.0;
	double inherit_emitter_velocity_ratio = 0.0;

	SubEmitterMode sub_emitter_mode;
	double sub_emitter_frequency = 0.0;
	int sub_emitter_amount_at_end = 0;
	int sub_emitter_amount_at_collision = 0;
	int sub_emitter_amount_at_start = 0;
	bool sub_emitter_keep_velocity = false;
	//do not save emission points here

	bool attractor_interaction_enabled = false;
	CollisionMode collision_mode;
	bool collision_scale = false;
	float collision_friction = 0.0f;
	float collision_bounce = 0.0f;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	static bool has_min_max_property(const String &p_name);

	void set_direction(Vector3 p_direction);
	Vector3 get_direction() const;

	void set_spread(float p_spread);
	float get_spread() const;

	void set_flatness(float p_flatness);
	float get_flatness() const;

	void set_velocity_pivot(const Vector3 &p_pivot);
	Vector3 get_velocity_pivot();

	void set_param(Parameter p_param, const Vector2 &p_value);
	Vector2 get_param(Parameter p_param) const;

	void set_param_min(Parameter p_param, float p_value);
	float get_param_min(Parameter p_param) const;

	void set_param_max(Parameter p_param, float p_value);
	float get_param_max(Parameter p_param) const;

	void set_param_texture(Parameter p_param, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_param_texture(Parameter p_param) const;

	void set_velocity_limit_curve(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_velocity_limit_curve() const;

	void set_alpha_curve(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_alpha_curve() const;
	void set_color(const Color &p_color);
	Color get_color() const;

	void set_color_ramp(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_color_ramp() const;

	void set_color_initial_ramp(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_color_initial_ramp() const;

	void set_emission_curve(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_emission_curve() const;

	void set_particle_flag(ParticleFlags p_particle_flag, bool p_enable);
	bool get_particle_flag(ParticleFlags p_particle_flag) const;

	void set_emission_shape(EmissionShape p_shape);
	void set_emission_sphere_radius(real_t p_radius);
	void set_emission_box_extents(Vector3 p_extents);
	void set_emission_point_texture(const Ref<Texture2D> &p_points);
	void set_emission_normal_texture(const Ref<Texture2D> &p_normals);
	void set_emission_color_texture(const Ref<Texture2D> &p_colors);
	void set_emission_ring_axis(Vector3 p_axis);
	void set_emission_ring_height(real_t p_height);
	void set_emission_ring_radius(real_t p_radius);
	void set_emission_ring_inner_radius(real_t p_radius);
	void set_emission_ring_cone_angle(real_t p_angle);
	void set_emission_point_count(int p_count);

	EmissionShape get_emission_shape() const;
	real_t get_emission_sphere_radius() const;
	Vector3 get_emission_box_extents() const;
	Ref<Texture2D> get_emission_point_texture() const;
	Ref<Texture2D> get_emission_normal_texture() const;
	Ref<Texture2D> get_emission_color_texture() const;
	Vector3 get_emission_ring_axis() const;
	real_t get_emission_ring_height() const;
	real_t get_emission_ring_radius() const;
	real_t get_emission_ring_inner_radius() const;
	real_t get_emission_ring_cone_angle() const;
	int get_emission_point_count() const;

	void set_turbulence_enabled(bool p_turbulence_enabled);
	void set_turbulence_noise_strength(float p_turbulence_noise_strength);
	void set_turbulence_noise_scale(float p_turbulence_noise_scale);
	void set_turbulence_noise_speed_random(float p_turbulence_noise_speed_random);
	void set_turbulence_noise_speed(const Vector3 &p_turbulence_noise_speed);

	bool get_turbulence_enabled() const;
	float get_turbulence_noise_strength() const;
	float get_turbulence_noise_scale() const;
	float get_turbulence_noise_speed_random() const;
	Vector3 get_turbulence_noise_speed() const;

	void set_gravity(const Vector3 &p_gravity);
	Vector3 get_gravity() const;

	void set_lifetime_randomness(double p_lifetime);
	double get_lifetime_randomness() const;

	void set_inherit_velocity_ratio(double p_ratio);
	double get_inherit_velocity_ratio();

	void set_attractor_interaction_enabled(bool p_enable);
	bool is_attractor_interaction_enabled() const;

	void set_collision_mode(CollisionMode p_collision_mode);
	CollisionMode get_collision_mode() const;

	void set_collision_use_scale(bool p_scale);
	bool is_collision_using_scale() const;

	void set_collision_friction(float p_friction);
	float get_collision_friction() const;

	void set_collision_bounce(float p_bounce);
	float get_collision_bounce() const;

	static void init_shaders();
	static void finish_shaders();
	static void flush_changes();

	void set_sub_emitter_mode(SubEmitterMode p_sub_emitter_mode);
	SubEmitterMode get_sub_emitter_mode() const;

	void set_sub_emitter_frequency(double p_frequency);
	double get_sub_emitter_frequency() const;

	void set_sub_emitter_amount_at_end(int p_amount);
	int get_sub_emitter_amount_at_end() const;

	void set_sub_emitter_amount_at_collision(int p_amount);
	int get_sub_emitter_amount_at_collision() const;

	void set_sub_emitter_amount_at_start(int p_amount);
	int get_sub_emitter_amount_at_start() const;

	void set_sub_emitter_keep_velocity(bool p_enable);
	bool get_sub_emitter_keep_velocity() const;

	void set_emission_shape_offset(const Vector3 &p_emission_shape_offset);
	Vector3 get_emission_shape_offset() const;

	void set_emission_shape_scale(const Vector3 &p_emission_shape_scale);
	Vector3 get_emission_shape_scale() const;

	virtual RID get_shader_rid() const override;

	virtual Shader::Mode get_shader_mode() const override;

	ParticleProcessMaterial();
	~ParticleProcessMaterial();
};

VARIANT_ENUM_CAST(ParticleProcessMaterial::Parameter)
VARIANT_ENUM_CAST(ParticleProcessMaterial::ParticleFlags)
VARIANT_ENUM_CAST(ParticleProcessMaterial::EmissionShape)
VARIANT_ENUM_CAST(ParticleProcessMaterial::SubEmitterMode)
VARIANT_ENUM_CAST(ParticleProcessMaterial::CollisionMode)
