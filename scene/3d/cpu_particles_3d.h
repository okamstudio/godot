/**************************************************************************/
/*  cpu_particles_3d.h                                                    */
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

#ifndef CPU_PARTICLES_3D_H
#define CPU_PARTICLES_3D_H

#include "scene/3d/visual_instance_3d.h"

class RandomNumberGenerator;

class CPUParticles3D : public GeometryInstance3D {
private:
	GDCLASS(CPUParticles3D, GeometryInstance3D);

public:
	enum DrawOrder {
		DRAW_ORDER_INDEX,
		DRAW_ORDER_LIFETIME,
		DRAW_ORDER_VIEW_DEPTH,
		DRAW_ORDER_MAX
	};

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
		PARAM_MAX
	};

	enum ParticleFlags {
		PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY,
		PARTICLE_FLAG_ROTATE_Y,
		PARTICLE_FLAG_DISABLE_Z,
		PARTICLE_FLAG_MAX
	};

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

private:
	bool emitting = false;
	bool active = false;

	struct Particle {
		Transform3D transform;
		Color color;
		real_t custom[4] = {};
		Vector3 velocity;
		bool active = false;
		real_t angle_rand = 0.0;
		real_t scale_rand = 0.0;
		real_t hue_rot_rand = 0.0;
		real_t anim_offset_rand = 0.0;
		Color start_color_rand;
		double time = 0.0;
		double lifetime = 0.0;
		Color base_color;

		uint32_t seed = 0;
	};

	double time = 0.0;
	double frame_remainder = 0.0;
	int cycle = 0;
	bool redraw = false;

	RID multimesh;

	Vector<Particle> particles;
	Vector<float> particle_data;
	Vector<int> particle_order;

	struct SortLifetime {
		const Particle *particles = nullptr;

		bool operator()(int p_a, int p_b) const {
			return particles[p_a].time > particles[p_b].time;
		}
	};

	struct SortAxis {
		const Particle *particles = nullptr;
		Vector3 axis;
		bool operator()(int p_a, int p_b) const {
			return axis.dot(particles[p_a].transform.origin) < axis.dot(particles[p_b].transform.origin);
		}
	};

	//

	bool one_shot = false;

	double lifetime = 1.0;
	double pre_process_time = 0.0;
	double _requested_process_time = 0.0;
	real_t explosiveness_ratio = 0.0;
	real_t randomness_ratio = 0.0;
	double lifetime_randomness = 0.0;
	double speed_scale = 1.0;
	AABB visibility_aabb;
	bool local_coords = false;
	int fixed_fps = 0;
	bool fractional_delta = true;
	uint32_t seed = 0;
	bool use_fixed_seed = false;

	Transform3D inv_emission_transform;

	SafeFlag can_update;

	DrawOrder draw_order = DRAW_ORDER_INDEX;

	Ref<Mesh> mesh;

	////////

	Vector3 direction = Vector3(1, 0, 0);
	real_t spread = 45.0;
	real_t flatness = 0.0;

	real_t parameters_min[PARAM_MAX] = {};
	real_t parameters_max[PARAM_MAX] = {};

	Ref<Curve> curve_parameters[PARAM_MAX];
	Color color = Color(1, 1, 1, 1);
	Ref<Gradient> color_ramp;
	Ref<Gradient> color_initial_ramp;

	bool particle_flags[PARTICLE_FLAG_MAX] = {};

	EmissionShape emission_shape = EMISSION_SHAPE_POINT;
	real_t emission_sphere_radius = 1.0;
	Vector3 emission_box_extents = Vector3(1, 1, 1);
	Vector<Vector3> emission_points;
	Vector<Vector3> emission_normals;
	Vector<Color> emission_colors;
	int emission_point_count = 0;
	Vector3 emission_ring_axis;
	real_t emission_ring_height = 0.0;
	real_t emission_ring_radius = 0.0;
	real_t emission_ring_inner_radius = 0.0;
	real_t emission_ring_cone_angle = 0.0;

	Ref<Curve> scale_curve_x;
	Ref<Curve> scale_curve_y;
	Ref<Curve> scale_curve_z;
	bool split_scale = false;

	Vector3 gravity = Vector3(0, -9.8, 0);

	Ref<RandomNumberGenerator> rng;

	void _update_internal();
	void _particles_process(double p_delta);
	void _update_particle_data_buffer();

	Mutex update_mutex;

	void _update_render_thread();

	void _set_redraw(bool p_redraw);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

#ifndef DISABLE_DEPRECATED
	void _restart_bind_compat_92089();
	static void _bind_compatibility_methods();
#endif

public:
	AABB get_aabb() const override;

	void set_emitting(bool p_emitting);
	void set_amount(int p_amount);
	void set_lifetime(double p_lifetime);
	void set_one_shot(bool p_one_shot);
	void set_pre_process_time(double p_time);
	void set_explosiveness_ratio(real_t p_ratio);
	void set_randomness_ratio(real_t p_ratio);
	void set_visibility_aabb(const AABB &p_aabb);
	void set_lifetime_randomness(double p_random);
	void set_use_local_coordinates(bool p_enable);
	void set_speed_scale(double p_scale);

	bool is_emitting() const;
	int get_amount() const;
	double get_lifetime() const;
	bool get_one_shot() const;
	double get_pre_process_time() const;
	real_t get_explosiveness_ratio() const;
	real_t get_randomness_ratio() const;
	AABB get_visibility_aabb() const;
	double get_lifetime_randomness() const;
	bool get_use_local_coordinates() const;
	double get_speed_scale() const;

	void set_fixed_fps(int p_count);
	int get_fixed_fps() const;

	void set_fractional_delta(bool p_enable);
	bool get_fractional_delta() const;

	void set_draw_order(DrawOrder p_order);
	DrawOrder get_draw_order() const;

	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_use_fixed_seed(bool p_use_fixed_seed = false);
	bool get_use_fixed_seed() const;

	void set_seed(uint32_t p_seed);
	uint32_t get_seed() const;

	void request_particles_process(real_t p_requested_process_time);

	///////////////////

	void set_direction(Vector3 p_direction);
	Vector3 get_direction() const;

	void set_spread(real_t p_spread);
	real_t get_spread() const;

	void set_flatness(real_t p_flatness);
	real_t get_flatness() const;

	void set_param_min(Parameter p_param, real_t p_value);
	real_t get_param_min(Parameter p_param) const;

	void set_param_max(Parameter p_param, real_t p_value);
	real_t get_param_max(Parameter p_param) const;

	void set_param_curve(Parameter p_param, const Ref<Curve> &p_curve);
	Ref<Curve> get_param_curve(Parameter p_param) const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_color_ramp(const Ref<Gradient> &p_ramp);
	Ref<Gradient> get_color_ramp() const;

	void set_color_initial_ramp(const Ref<Gradient> &p_ramp);
	Ref<Gradient> get_color_initial_ramp() const;

	void set_particle_flag(ParticleFlags p_particle_flag, bool p_enable);
	bool get_particle_flag(ParticleFlags p_particle_flag) const;

	void set_emission_shape(EmissionShape p_shape);
	void set_emission_sphere_radius(real_t p_radius);
	void set_emission_box_extents(Vector3 p_extents);
	void set_emission_points(const Vector<Vector3> &p_points);
	void set_emission_normals(const Vector<Vector3> &p_normals);
	void set_emission_colors(const Vector<Color> &p_colors);
	void set_emission_ring_axis(Vector3 p_axis);
	void set_emission_ring_height(real_t p_height);
	void set_emission_ring_radius(real_t p_radius);
	void set_emission_ring_inner_radius(real_t p_radius);
	void set_emission_ring_cone_angle(real_t p_angle);
	void set_scale_curve_x(Ref<Curve> p_scale_curve);
	void set_scale_curve_y(Ref<Curve> p_scale_curve);
	void set_scale_curve_z(Ref<Curve> p_scale_curve);
	void set_split_scale(bool p_split_scale);

	EmissionShape get_emission_shape() const;
	real_t get_emission_sphere_radius() const;
	Vector3 get_emission_box_extents() const;
	Vector<Vector3> get_emission_points() const;
	Vector<Vector3> get_emission_normals() const;
	Vector<Color> get_emission_colors() const;
	Vector3 get_emission_ring_axis() const;
	real_t get_emission_ring_height() const;
	real_t get_emission_ring_radius() const;
	real_t get_emission_ring_inner_radius() const;
	real_t get_emission_ring_cone_angle() const;
	Ref<Curve> get_scale_curve_x() const;
	Ref<Curve> get_scale_curve_y() const;
	Ref<Curve> get_scale_curve_z() const;
	bool get_split_scale();

	void set_gravity(const Vector3 &p_gravity);
	Vector3 get_gravity() const;

	PackedStringArray get_configuration_warnings() const override;

	void restart(bool p_keep_seed = false);

	void convert_from_particles(Node *p_particles);

	AABB capture_aabb() const;

	CPUParticles3D();
	~CPUParticles3D();
};

VARIANT_ENUM_CAST(CPUParticles3D::DrawOrder)
VARIANT_ENUM_CAST(CPUParticles3D::Parameter)
VARIANT_ENUM_CAST(CPUParticles3D::ParticleFlags)
VARIANT_ENUM_CAST(CPUParticles3D::EmissionShape)

#endif // CPU_PARTICLES_3D_H
