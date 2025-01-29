/**************************************************************************/
/*  cpu_particles_3d.cpp                                                  */
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

#include "cpu_particles_3d.h"
#include "cpu_particles_3d.compat.inc"

#include "core/math/random_number_generator.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/particle_process_material.h"

AABB CPUParticles3D::get_aabb() const {
	return AABB();
}

void CPUParticles3D::set_emitting(bool p_emitting) {
	if (emitting == p_emitting) {
		return;
	}

	emitting = p_emitting;
	if (emitting) {
		active = true;
		set_process_internal(true);

		// first update before rendering to avoid one frame delay after emitting starts
		if (time == 0) {
			_update_internal();
		}
	}
}

void CPUParticles3D::set_amount(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles must be greater than 0.");

	particles.resize(p_amount);
	{
		Particle *w = particles.ptrw();

		for (int i = 0; i < p_amount; i++) {
			w[i].active = false;
			w[i].custom[3] = 1.0; // Make sure w component isn't garbage data and doesn't break shaders with CUSTOM.y/Custom.w
		}
	}

	particle_data.resize((12 + 4 + 4) * p_amount);
	RS::get_singleton()->multimesh_set_visible_instances(multimesh, -1);
	RS::get_singleton()->multimesh_allocate_data(multimesh, p_amount, RS::MULTIMESH_TRANSFORM_3D, true, true);

	particle_order.resize(p_amount);
}

void CPUParticles3D::set_lifetime(double p_lifetime) {
	ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
	lifetime = p_lifetime;
}

void CPUParticles3D::set_one_shot(bool p_one_shot) {
	one_shot = p_one_shot;
}

void CPUParticles3D::set_pre_process_time(double p_time) {
	pre_process_time = p_time;
}

void CPUParticles3D::set_explosiveness_ratio(real_t p_ratio) {
	explosiveness_ratio = p_ratio;
}

void CPUParticles3D::set_randomness_ratio(real_t p_ratio) {
	randomness_ratio = p_ratio;
}

void CPUParticles3D::set_visibility_aabb(const AABB &p_aabb) {
	RS::get_singleton()->multimesh_set_custom_aabb(multimesh, p_aabb);
	visibility_aabb = p_aabb;
	update_gizmos();
}

void CPUParticles3D::set_lifetime_randomness(double p_random) {
	lifetime_randomness = p_random;
}

void CPUParticles3D::set_use_local_coordinates(bool p_enable) {
	local_coords = p_enable;
}

void CPUParticles3D::set_speed_scale(double p_scale) {
	speed_scale = p_scale;
}

bool CPUParticles3D::is_emitting() const {
	return emitting;
}

int CPUParticles3D::get_amount() const {
	return particles.size();
}

double CPUParticles3D::get_lifetime() const {
	return lifetime;
}

bool CPUParticles3D::get_one_shot() const {
	return one_shot;
}

double CPUParticles3D::get_pre_process_time() const {
	return pre_process_time;
}

real_t CPUParticles3D::get_explosiveness_ratio() const {
	return explosiveness_ratio;
}

real_t CPUParticles3D::get_randomness_ratio() const {
	return randomness_ratio;
}

AABB CPUParticles3D::get_visibility_aabb() const {
	return visibility_aabb;
}

double CPUParticles3D::get_lifetime_randomness() const {
	return lifetime_randomness;
}

bool CPUParticles3D::get_use_local_coordinates() const {
	return local_coords;
}

double CPUParticles3D::get_speed_scale() const {
	return speed_scale;
}

void CPUParticles3D::set_draw_order(DrawOrder p_order) {
	ERR_FAIL_INDEX(p_order, DRAW_ORDER_MAX);
	draw_order = p_order;
}

CPUParticles3D::DrawOrder CPUParticles3D::get_draw_order() const {
	return draw_order;
}

void CPUParticles3D::set_mesh(const Ref<Mesh> &p_mesh) {
	mesh = p_mesh;
	if (mesh.is_valid()) {
		RS::get_singleton()->multimesh_set_mesh(multimesh, mesh->get_rid());
	} else {
		RS::get_singleton()->multimesh_set_mesh(multimesh, RID());
	}

	update_configuration_warnings();
}

Ref<Mesh> CPUParticles3D::get_mesh() const {
	return mesh;
}

void CPUParticles3D::set_fixed_fps(int p_count) {
	fixed_fps = p_count;
}

int CPUParticles3D::get_fixed_fps() const {
	return fixed_fps;
}

void CPUParticles3D::set_fractional_delta(bool p_enable) {
	fractional_delta = p_enable;
}

bool CPUParticles3D::get_fractional_delta() const {
	return fractional_delta;
}

PackedStringArray CPUParticles3D::get_configuration_warnings() const {
	PackedStringArray warnings = GeometryInstance3D::get_configuration_warnings();

	bool mesh_found = false;
	bool anim_material_found = false;

	if (get_mesh().is_valid()) {
		mesh_found = true;
		for (int j = 0; j < get_mesh()->get_surface_count(); j++) {
			anim_material_found = Object::cast_to<ShaderMaterial>(get_mesh()->surface_get_material(j).ptr()) != nullptr;
			StandardMaterial3D *spat = Object::cast_to<StandardMaterial3D>(get_mesh()->surface_get_material(j).ptr());
			anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == StandardMaterial3D::BILLBOARD_PARTICLES);
		}
	}

	anim_material_found = anim_material_found || Object::cast_to<ShaderMaterial>(get_material_override().ptr()) != nullptr;
	StandardMaterial3D *spat = Object::cast_to<StandardMaterial3D>(get_material_override().ptr());
	anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == StandardMaterial3D::BILLBOARD_PARTICLES);

	if (!mesh_found) {
		warnings.push_back(RTR("Nothing is visible because no mesh has been assigned."));
	}

	if (!anim_material_found && (get_param_max(PARAM_ANIM_SPEED) != 0.0 || get_param_max(PARAM_ANIM_OFFSET) != 0.0 || get_param_curve(PARAM_ANIM_SPEED).is_valid() || get_param_curve(PARAM_ANIM_OFFSET).is_valid())) {
		warnings.push_back(RTR("CPUParticles3D animation requires the usage of a StandardMaterial3D whose Billboard Mode is set to \"Particle Billboard\"."));
	}

	return warnings;
}

void CPUParticles3D::restart(bool p_keep_seed) {
	time = 0;
	frame_remainder = 0;
	cycle = 0;
	emitting = false;

	{
		int pc = particles.size();
		Particle *w = particles.ptrw();

		for (int i = 0; i < pc; i++) {
			w[i].active = false;
		}
	}
	if (!p_keep_seed && !use_fixed_seed) {
		seed = Math::rand();
	}

	set_emitting(true);
}

void CPUParticles3D::set_direction(Vector3 p_direction) {
	direction = p_direction;
}

Vector3 CPUParticles3D::get_direction() const {
	return direction;
}

void CPUParticles3D::set_spread(real_t p_spread) {
	spread = p_spread;
}

real_t CPUParticles3D::get_spread() const {
	return spread;
}

void CPUParticles3D::set_flatness(real_t p_flatness) {
	flatness = p_flatness;
}

real_t CPUParticles3D::get_flatness() const {
	return flatness;
}

void CPUParticles3D::set_param_min(Parameter p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	parameters_min[p_param] = p_value;
	if (parameters_min[p_param] > parameters_max[p_param]) {
		set_param_max(p_param, p_value);
	}

	update_configuration_warnings();
}

real_t CPUParticles3D::get_param_min(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

	return parameters_min[p_param];
}

void CPUParticles3D::set_param_max(Parameter p_param, real_t p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	parameters_max[p_param] = p_value;
	if (parameters_min[p_param] > parameters_max[p_param]) {
		set_param_min(p_param, p_value);
	}

	update_configuration_warnings();
}

real_t CPUParticles3D::get_param_max(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

	return parameters_max[p_param];
}

static void _adjust_curve_range(const Ref<Curve> &p_curve, real_t p_min, real_t p_max) {
	Ref<Curve> curve = p_curve;
	if (curve.is_null()) {
		return;
	}

	curve->ensure_default_setup(p_min, p_max);
}

void CPUParticles3D::set_param_curve(Parameter p_param, const Ref<Curve> &p_curve) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	curve_parameters[p_param] = p_curve;

	switch (p_param) {
		case PARAM_INITIAL_LINEAR_VELOCITY: {
			//do none for this one
		} break;
		case PARAM_ANGULAR_VELOCITY: {
			_adjust_curve_range(p_curve, -360, 360);
		} break;
		case PARAM_ORBIT_VELOCITY: {
			_adjust_curve_range(p_curve, -500, 500);
		} break;
		case PARAM_LINEAR_ACCEL: {
			_adjust_curve_range(p_curve, -200, 200);
		} break;
		case PARAM_RADIAL_ACCEL: {
			_adjust_curve_range(p_curve, -200, 200);
		} break;
		case PARAM_TANGENTIAL_ACCEL: {
			_adjust_curve_range(p_curve, -200, 200);
		} break;
		case PARAM_DAMPING: {
			_adjust_curve_range(p_curve, 0, 100);
		} break;
		case PARAM_ANGLE: {
			_adjust_curve_range(p_curve, -360, 360);
		} break;
		case PARAM_SCALE: {
		} break;
		case PARAM_HUE_VARIATION: {
			_adjust_curve_range(p_curve, -1, 1);
		} break;
		case PARAM_ANIM_SPEED: {
			_adjust_curve_range(p_curve, 0, 200);
		} break;
		case PARAM_ANIM_OFFSET: {
		} break;
		default: {
		}
	}

	update_configuration_warnings();
}

Ref<Curve> CPUParticles3D::get_param_curve(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, Ref<Curve>());

	return curve_parameters[p_param];
}

void CPUParticles3D::set_color(const Color &p_color) {
	color = p_color;
}

Color CPUParticles3D::get_color() const {
	return color;
}

void CPUParticles3D::set_color_ramp(const Ref<Gradient> &p_ramp) {
	color_ramp = p_ramp;
}

Ref<Gradient> CPUParticles3D::get_color_ramp() const {
	return color_ramp;
}

void CPUParticles3D::set_color_initial_ramp(const Ref<Gradient> &p_ramp) {
	color_initial_ramp = p_ramp;
}

Ref<Gradient> CPUParticles3D::get_color_initial_ramp() const {
	return color_initial_ramp;
}

void CPUParticles3D::set_particle_flag(ParticleFlags p_particle_flag, bool p_enable) {
	ERR_FAIL_INDEX(p_particle_flag, PARTICLE_FLAG_MAX);
	particle_flags[p_particle_flag] = p_enable;
	if (p_particle_flag == PARTICLE_FLAG_DISABLE_Z) {
		notify_property_list_changed();
	}
}

bool CPUParticles3D::get_particle_flag(ParticleFlags p_particle_flag) const {
	ERR_FAIL_INDEX_V(p_particle_flag, PARTICLE_FLAG_MAX, false);
	return particle_flags[p_particle_flag];
}

void CPUParticles3D::set_emission_shape(EmissionShape p_shape) {
	ERR_FAIL_INDEX(p_shape, EMISSION_SHAPE_MAX);
	emission_shape = p_shape;
	update_gizmos();
}

void CPUParticles3D::set_emission_sphere_radius(real_t p_radius) {
	emission_sphere_radius = p_radius;
	update_gizmos();
}

void CPUParticles3D::set_emission_box_extents(Vector3 p_extents) {
	emission_box_extents = p_extents;
	update_gizmos();
}

void CPUParticles3D::set_emission_points(const Vector<Vector3> &p_points) {
	emission_points = p_points;
}

void CPUParticles3D::set_emission_normals(const Vector<Vector3> &p_normals) {
	emission_normals = p_normals;
}

void CPUParticles3D::set_emission_colors(const Vector<Color> &p_colors) {
	emission_colors = p_colors;
}

void CPUParticles3D::set_emission_ring_axis(Vector3 p_axis) {
	emission_ring_axis = p_axis;
	update_gizmos();
}

void CPUParticles3D::set_emission_ring_height(real_t p_height) {
	emission_ring_height = p_height;
	update_gizmos();
}

void CPUParticles3D::set_emission_ring_radius(real_t p_radius) {
	emission_ring_radius = p_radius;
	update_gizmos();
}

void CPUParticles3D::set_emission_ring_inner_radius(real_t p_radius) {
	emission_ring_inner_radius = p_radius;
	update_gizmos();
}

void CPUParticles3D::set_emission_ring_cone_angle(real_t p_angle) {
	emission_ring_cone_angle = p_angle;
	update_gizmos();
}

void CPUParticles3D::set_scale_curve_x(Ref<Curve> p_scale_curve) {
	scale_curve_x = p_scale_curve;
}

void CPUParticles3D::set_scale_curve_y(Ref<Curve> p_scale_curve) {
	scale_curve_y = p_scale_curve;
}

void CPUParticles3D::set_scale_curve_z(Ref<Curve> p_scale_curve) {
	scale_curve_z = p_scale_curve;
}

void CPUParticles3D::set_split_scale(bool p_split_scale) {
	split_scale = p_split_scale;
	notify_property_list_changed();
}

real_t CPUParticles3D::get_emission_sphere_radius() const {
	return emission_sphere_radius;
}

Vector3 CPUParticles3D::get_emission_box_extents() const {
	return emission_box_extents;
}

Vector<Vector3> CPUParticles3D::get_emission_points() const {
	return emission_points;
}

Vector<Vector3> CPUParticles3D::get_emission_normals() const {
	return emission_normals;
}

Vector<Color> CPUParticles3D::get_emission_colors() const {
	return emission_colors;
}

Vector3 CPUParticles3D::get_emission_ring_axis() const {
	return emission_ring_axis;
}

real_t CPUParticles3D::get_emission_ring_height() const {
	return emission_ring_height;
}

real_t CPUParticles3D::get_emission_ring_radius() const {
	return emission_ring_radius;
}

real_t CPUParticles3D::get_emission_ring_inner_radius() const {
	return emission_ring_inner_radius;
}

real_t CPUParticles3D::get_emission_ring_cone_angle() const {
	return emission_ring_cone_angle;
}

CPUParticles3D::EmissionShape CPUParticles3D::get_emission_shape() const {
	return emission_shape;
}

void CPUParticles3D::set_gravity(const Vector3 &p_gravity) {
	gravity = p_gravity;
}

Vector3 CPUParticles3D::get_gravity() const {
	return gravity;
}

Ref<Curve> CPUParticles3D::get_scale_curve_x() const {
	return scale_curve_x;
}

Ref<Curve> CPUParticles3D::get_scale_curve_y() const {
	return scale_curve_y;
}

Ref<Curve> CPUParticles3D::get_scale_curve_z() const {
	return scale_curve_z;
}

bool CPUParticles3D::get_split_scale() {
	return split_scale;
}

AABB CPUParticles3D::capture_aabb() const {
	RS::get_singleton()->multimesh_set_custom_aabb(multimesh, AABB());
	return RS::get_singleton()->multimesh_get_aabb(multimesh);
}

void CPUParticles3D::set_use_fixed_seed(bool p_use_fixed_seed) {
	if (p_use_fixed_seed == use_fixed_seed) {
		return;
	}
	use_fixed_seed = p_use_fixed_seed;
	notify_property_list_changed();
}

bool CPUParticles3D::get_use_fixed_seed() const {
	return use_fixed_seed;
}

void CPUParticles3D::set_seed(uint32_t p_seed) {
	seed = p_seed;
}

uint32_t CPUParticles3D::get_seed() const {
	return seed;
}

void CPUParticles3D::request_particles_process(real_t p_requested_process_time) {
	_requested_process_time = p_requested_process_time;
}

void CPUParticles3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "emitting") {
		p_property.hint = one_shot ? PROPERTY_HINT_ONESHOT : PROPERTY_HINT_NONE;
	}

	if (p_property.name == "emission_sphere_radius" && (emission_shape != EMISSION_SHAPE_SPHERE && emission_shape != EMISSION_SHAPE_SPHERE_SURFACE)) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "emission_box_extents" && emission_shape != EMISSION_SHAPE_BOX) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if ((p_property.name == "emission_point_texture" || p_property.name == "emission_color_texture" || p_property.name == "emission_points") && (emission_shape != EMISSION_SHAPE_POINTS && (emission_shape != EMISSION_SHAPE_DIRECTED_POINTS))) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "emission_normals" && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name.begins_with("emission_ring_") && emission_shape != EMISSION_SHAPE_RING) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name.begins_with("orbit_") && !particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name.begins_with("scale_curve_") && !split_scale) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "seed" && !use_fixed_seed) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

static uint32_t idhash(uint32_t x) {
	x = ((x >> uint32_t(16)) ^ x) * uint32_t(0x45d9f3b);
	x = ((x >> uint32_t(16)) ^ x) * uint32_t(0x45d9f3b);
	x = (x >> uint32_t(16)) ^ x;
	return x;
}

static real_t rand_from_seed(uint32_t &seed) {
	int k;
	int s = int(seed);
	if (s == 0) {
		s = 305420679;
	}
	k = s / 127773;
	s = 16807 * (s - k * 127773) - 2836 * k;
	if (s < 0) {
		s += 2147483647;
	}
	seed = uint32_t(s);
	return (seed % uint32_t(65536)) / 65535.0;
}

void CPUParticles3D::_update_internal() {
	if (particles.size() == 0 || !is_visible_in_tree()) {
		_set_redraw(false);
		return;
	}

	double delta = get_process_delta_time();
	if (!active && !emitting) {
		set_process_internal(false);
		_set_redraw(false);

		//reset variables
		time = 0;
		frame_remainder = 0;
		cycle = 0;
		return;
	}
	_set_redraw(true);

	bool processed = false;

	double frame_time;
	if (fixed_fps > 0) {
		frame_time = 1.0 / fixed_fps;
	} else {
		frame_time = 1.0 / 30.0;
	}
	double todo = _requested_process_time;
	_requested_process_time = 0.;
	if (time == 0 && pre_process_time > 0.0) {
		todo += pre_process_time;
	}
	real_t tmp_speed = speed_scale;
	speed_scale = 1.0;
	while (todo > 0) {
		_particles_process(frame_time);
		todo -= frame_time;
	}
	speed_scale = tmp_speed;
	todo = 0.0;

	if (fixed_fps > 0) {
		double decr = frame_time;

		double ldelta = delta;
		if (ldelta > 0.1) { //avoid recursive stalls if fps goes below 10
			ldelta = 0.1;
		} else if (ldelta <= 0.0) { //unlikely but..
			ldelta = 0.001;
		}
		todo = frame_remainder + ldelta;

		while (todo >= frame_time) {
			_particles_process(frame_time);
			processed = true;
			todo -= decr;
		}

		frame_remainder = todo;

	} else {
		_particles_process(delta);
		processed = true;
	}

	if (processed) {
		_update_particle_data_buffer();
	}
}

void CPUParticles3D::_particles_process(double p_delta) {
	p_delta *= speed_scale;

	int pcount = particles.size();
	Particle *w = particles.ptrw();

	Particle *parray = w;

	double prev_time = time;
	time += p_delta;
	if (time > lifetime) {
		time = Math::fmod(time, lifetime);
		cycle++;
		if (one_shot && cycle > 0) {
			set_emitting(false);
			notify_property_list_changed();
		}
	}

	Transform3D emission_xform;
	Basis velocity_xform;
	if (!local_coords) {
		emission_xform = get_global_transform();
		velocity_xform = emission_xform.basis;
	}

	double system_phase = time / lifetime;

	bool should_be_active = false;
	for (int i = 0; i < pcount; i++) {
		Particle &p = parray[i];

		if (!emitting && !p.active) {
			continue;
		}

		double local_delta = p_delta;

		// The phase is a ratio between 0 (birth) and 1 (end of life) for each particle.
		// While we use time in tests later on, for randomness we use the phase as done in the
		// original shader code, and we later multiply by lifetime to get the time.
		double restart_phase = double(i) / double(pcount);

		if (randomness_ratio > 0.0) {
			uint32_t _seed = cycle;
			if (restart_phase >= system_phase) {
				_seed -= uint32_t(1);
			}
			_seed *= uint32_t(pcount);
			_seed += uint32_t(i);
			double random = double(idhash(_seed) % uint32_t(65536)) / 65536.0;
			restart_phase += randomness_ratio * random * 1.0 / double(pcount);
		}

		restart_phase *= (1.0 - explosiveness_ratio);
		double restart_time = restart_phase * lifetime;
		bool restart = false;

		if (time > prev_time) {
			// restart_time >= prev_time is used so particles emit in the first frame they are processed

			if (restart_time >= prev_time && restart_time < time) {
				restart = true;
				if (fractional_delta) {
					local_delta = time - restart_time;
				}
			}

		} else if (local_delta > 0.0) {
			if (restart_time >= prev_time) {
				restart = true;
				if (fractional_delta) {
					local_delta = lifetime - restart_time + time;
				}

			} else if (restart_time < time) {
				restart = true;
				if (fractional_delta) {
					local_delta = time - restart_time;
				}
			}
		}

		if (p.time * (1.0 - explosiveness_ratio) > p.lifetime) {
			restart = true;
		}

		float tv = 0.0;

		if (restart) {
			if (!emitting) {
				p.active = false;
				continue;
			}
			p.active = true;

			/*real_t tex_linear_velocity = 0;
			if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY].is_valid()) {
				tex_linear_velocity = curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]->sample(0);
			}*/

			real_t tex_angle = 1.0;
			if (curve_parameters[PARAM_ANGLE].is_valid()) {
				tex_angle = curve_parameters[PARAM_ANGLE]->sample(tv);
			}

			real_t tex_anim_offset = 1.0;
			if (curve_parameters[PARAM_ANGLE].is_valid()) {
				tex_anim_offset = curve_parameters[PARAM_ANGLE]->sample(tv);
			}

			p.seed = seed + uint32_t(1) + i + cycle;
			rng->set_seed(p.seed);
			p.angle_rand = rng->randf();
			p.scale_rand = rng->randf();
			p.hue_rot_rand = rng->randf();
			p.anim_offset_rand = rng->randf();

			if (color_initial_ramp.is_valid()) {
				p.start_color_rand = color_initial_ramp->get_color_at_offset(rng->randf());
			} else {
				p.start_color_rand = Color(1, 1, 1, 1);
			}

			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				real_t angle1_rad = Math::atan2(direction.y, direction.x) + Math::deg_to_rad((rng->randf() * 2.0 - 1.0) * spread);
				Vector3 rot = Vector3(Math::cos(angle1_rad), Math::sin(angle1_rad), 0.0);
				p.velocity = rot * Math::lerp(parameters_min[PARAM_INITIAL_LINEAR_VELOCITY], parameters_max[PARAM_INITIAL_LINEAR_VELOCITY], rng->randf());
			} else {
				//initiate velocity spread in 3D
				real_t angle1_rad = Math::deg_to_rad((rng->randf() * (real_t)2.0 - (real_t)1.0) * spread);
				real_t angle2_rad = Math::deg_to_rad((rng->randf() * (real_t)2.0 - (real_t)1.0) * ((real_t)1.0 - flatness) * spread);

				Vector3 direction_xz = Vector3(Math::sin(angle1_rad), 0, Math::cos(angle1_rad));
				Vector3 direction_yz = Vector3(0, Math::sin(angle2_rad), Math::cos(angle2_rad));
				Vector3 spread_direction = Vector3(direction_xz.x * direction_yz.z, direction_yz.y, direction_xz.z * direction_yz.z);
				Vector3 direction_nrm = direction;
				if (direction_nrm.length_squared() > 0) {
					direction_nrm.normalize();
				} else {
					direction_nrm = Vector3(0, 0, 1);
				}
				// rotate spread to direction
				Vector3 binormal = Vector3(0.0, 1.0, 0.0).cross(direction_nrm);
				if (binormal.length_squared() < 0.00000001) {
					// direction is parallel to Y. Choose Z as the binormal.
					binormal = Vector3(0.0, 0.0, 1.0);
				}
				binormal.normalize();
				Vector3 normal = binormal.cross(direction_nrm);
				spread_direction = binormal * spread_direction.x + normal * spread_direction.y + direction_nrm * spread_direction.z;
				p.velocity = spread_direction * Math::lerp(parameters_min[PARAM_INITIAL_LINEAR_VELOCITY], parameters_max[PARAM_INITIAL_LINEAR_VELOCITY], rng->randf());
			}

			real_t base_angle = tex_angle * Math::lerp(parameters_min[PARAM_ANGLE], parameters_max[PARAM_ANGLE], p.angle_rand);
			p.custom[0] = Math::deg_to_rad(base_angle); //angle
			p.custom[1] = 0.0; //phase
			p.custom[2] = tex_anim_offset * Math::lerp(parameters_min[PARAM_ANIM_OFFSET], parameters_max[PARAM_ANIM_OFFSET], p.anim_offset_rand); //animation offset (0-1)
			p.custom[3] = (1.0 - rng->randf() * lifetime_randomness);
			p.transform = Transform3D();
			p.time = 0;
			p.lifetime = lifetime * p.custom[3];
			p.base_color = Color(1, 1, 1, 1);

			switch (emission_shape) {
				case EMISSION_SHAPE_POINT: {
					//do none
				} break;
				case EMISSION_SHAPE_SPHERE: {
					real_t s = 2.0 * rng->randf() - 1.0;
					real_t t = Math_TAU * rng->randf();
					real_t x = rng->randf();
					real_t radius = emission_sphere_radius * Math::sqrt(1.0 - s * s);
					p.transform.origin = Vector3(0, 0, 0).lerp(Vector3(radius * Math::cos(t), radius * Math::sin(t), emission_sphere_radius * s), x);
				} break;
				case EMISSION_SHAPE_SPHERE_SURFACE: {
					real_t s = 2.0 * rng->randf() - 1.0;
					real_t t = Math_TAU * rng->randf();
					real_t radius = emission_sphere_radius * Math::sqrt(1.0 - s * s);
					p.transform.origin = Vector3(radius * Math::cos(t), radius * Math::sin(t), emission_sphere_radius * s);
				} break;
				case EMISSION_SHAPE_BOX: {
					p.transform.origin = Vector3(rng->randf() * 2.0 - 1.0, rng->randf() * 2.0 - 1.0, rng->randf() * 2.0 - 1.0) * emission_box_extents;
				} break;
				case EMISSION_SHAPE_POINTS:
				case EMISSION_SHAPE_DIRECTED_POINTS: {
					int pc = emission_points.size();
					if (pc == 0) {
						break;
					}

					int random_idx = Math::rand() % pc;

					p.transform.origin = emission_points.get(random_idx);

					if (emission_shape == EMISSION_SHAPE_DIRECTED_POINTS && emission_normals.size() == pc) {
						if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
							Vector3 normal = emission_normals.get(random_idx);
							Vector2 normal_2d(normal.x, normal.y);
							Transform2D m2;
							m2.columns[0] = normal_2d;
							m2.columns[1] = normal_2d.orthogonal();
							Vector2 velocity_2d(p.velocity.x, p.velocity.y);
							velocity_2d = m2.basis_xform(velocity_2d);
							p.velocity.x = velocity_2d.x;
							p.velocity.y = velocity_2d.y;
						} else {
							Vector3 normal = emission_normals.get(random_idx);
							Vector3 v0 = Math::abs(normal.z) < 0.999 ? Vector3(0.0, 0.0, 1.0) : Vector3(0, 1.0, 0.0);
							Vector3 tangent = v0.cross(normal).normalized();
							Vector3 bitangent = tangent.cross(normal).normalized();
							Basis m3;
							m3.set_column(0, tangent);
							m3.set_column(1, bitangent);
							m3.set_column(2, normal);
							p.velocity = m3.xform(p.velocity);
						}
					}

					if (emission_colors.size() == pc) {
						p.base_color = emission_colors.get(random_idx);
					}
				} break;
				case EMISSION_SHAPE_RING: {
					real_t radius_clamped = MAX(0.001, emission_ring_radius);
					real_t top_radius = MAX(radius_clamped - Math::tan(Math::deg_to_rad(90.0 - emission_ring_cone_angle)) * emission_ring_height, 0.0);
					real_t y_pos = rng->randf();
					real_t skew = MAX(MIN(radius_clamped, top_radius) / MAX(radius_clamped, top_radius), 0.5);
					y_pos = radius_clamped < top_radius ? Math::pow(y_pos, skew) : 1.0 - Math::pow(y_pos, skew);
					real_t ring_random_angle = rng->randf() * Math_TAU;
					real_t ring_random_radius = Math::sqrt(rng->randf() * (radius_clamped * radius_clamped - emission_ring_inner_radius * emission_ring_inner_radius) + emission_ring_inner_radius * emission_ring_inner_radius);
					ring_random_radius = Math::lerp(ring_random_radius, ring_random_radius * (top_radius / radius_clamped), y_pos);
					Vector3 axis = emission_ring_axis == Vector3(0.0, 0.0, 0.0) ? Vector3(0.0, 0.0, 1.0) : emission_ring_axis.normalized();
					Vector3 ortho_axis;
					if (axis.abs() == Vector3(1.0, 0.0, 0.0)) {
						ortho_axis = Vector3(0.0, 1.0, 0.0).cross(axis);
					} else {
						ortho_axis = Vector3(1.0, 0.0, 0.0).cross(axis);
					}
					ortho_axis = ortho_axis.normalized();
					ortho_axis.rotate(axis, ring_random_angle);
					ortho_axis = ortho_axis.normalized();
					p.transform.origin = ortho_axis * ring_random_radius + (y_pos * emission_ring_height - emission_ring_height / 2.0) * axis;
				} break;
				case EMISSION_SHAPE_MAX: { // Max value for validity check.
					break;
				}
			}

			if (!local_coords) {
				p.velocity = velocity_xform.xform(p.velocity);
				p.transform = emission_xform * p.transform;
			}

			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				p.velocity.z = 0.0;
				p.transform.origin.z = 0.0;
			}

		} else if (!p.active) {
			continue;
		} else if (p.time > p.lifetime) {
			p.active = false;
			tv = 1.0;
		} else {
			uint32_t alt_seed = p.seed;

			p.time += local_delta;
			p.custom[1] = p.time / lifetime;
			tv = p.time / p.lifetime;

			real_t tex_linear_velocity = 1.0;
			if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY].is_valid()) {
				tex_linear_velocity = curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]->sample(tv);
			}

			real_t tex_orbit_velocity = 1.0;
			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				if (curve_parameters[PARAM_ORBIT_VELOCITY].is_valid()) {
					tex_orbit_velocity = curve_parameters[PARAM_ORBIT_VELOCITY]->sample(tv);
				}
			}

			real_t tex_angular_velocity = 1.0;
			if (curve_parameters[PARAM_ANGULAR_VELOCITY].is_valid()) {
				tex_angular_velocity = curve_parameters[PARAM_ANGULAR_VELOCITY]->sample(tv);
			}

			real_t tex_linear_accel = 1.0;
			if (curve_parameters[PARAM_LINEAR_ACCEL].is_valid()) {
				tex_linear_accel = curve_parameters[PARAM_LINEAR_ACCEL]->sample(tv);
			}

			real_t tex_tangential_accel = 1.0;
			if (curve_parameters[PARAM_TANGENTIAL_ACCEL].is_valid()) {
				tex_tangential_accel = curve_parameters[PARAM_TANGENTIAL_ACCEL]->sample(tv);
			}

			real_t tex_radial_accel = 1.0;
			if (curve_parameters[PARAM_RADIAL_ACCEL].is_valid()) {
				tex_radial_accel = curve_parameters[PARAM_RADIAL_ACCEL]->sample(tv);
			}

			real_t tex_damping = 1.0;
			if (curve_parameters[PARAM_DAMPING].is_valid()) {
				tex_damping = curve_parameters[PARAM_DAMPING]->sample(tv);
			}

			real_t tex_angle = 1.0;
			if (curve_parameters[PARAM_ANGLE].is_valid()) {
				tex_angle = curve_parameters[PARAM_ANGLE]->sample(tv);
			}
			real_t tex_anim_speed = 1.0;
			if (curve_parameters[PARAM_ANIM_SPEED].is_valid()) {
				tex_anim_speed = curve_parameters[PARAM_ANIM_SPEED]->sample(tv);
			}

			real_t tex_anim_offset = 1.0;
			if (curve_parameters[PARAM_ANIM_OFFSET].is_valid()) {
				tex_anim_offset = curve_parameters[PARAM_ANIM_OFFSET]->sample(tv);
			}

			Vector3 force = gravity;
			Vector3 position = p.transform.origin;
			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				position.z = 0.0;
			}
			//apply linear acceleration
			force += p.velocity.length() > 0.0 ? p.velocity.normalized() * tex_linear_accel * Math::lerp(parameters_min[PARAM_LINEAR_ACCEL], parameters_max[PARAM_LINEAR_ACCEL], rand_from_seed(alt_seed)) : Vector3();
			//apply radial acceleration
			Vector3 org = emission_xform.origin;
			Vector3 diff = position - org;
			force += diff.length() > 0.0 ? diff.normalized() * (tex_radial_accel)*Math::lerp(parameters_min[PARAM_RADIAL_ACCEL], parameters_max[PARAM_RADIAL_ACCEL], rand_from_seed(alt_seed)) : Vector3();
			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				Vector2 yx = Vector2(diff.y, diff.x);
				Vector2 yx2 = (yx * Vector2(-1.0, 1.0)).normalized();
				force += yx.length() > 0.0 ? Vector3(yx2.x, yx2.y, 0.0) * (tex_tangential_accel * Math::lerp(parameters_min[PARAM_TANGENTIAL_ACCEL], parameters_max[PARAM_TANGENTIAL_ACCEL], rand_from_seed(alt_seed))) : Vector3();

			} else {
				Vector3 crossDiff = diff.normalized().cross(gravity.normalized());
				force += crossDiff.length() > 0.0 ? crossDiff.normalized() * (tex_tangential_accel * Math::lerp(parameters_min[PARAM_TANGENTIAL_ACCEL], parameters_max[PARAM_TANGENTIAL_ACCEL], rand_from_seed(alt_seed))) : Vector3();
			}
			//apply attractor forces
			p.velocity += force * local_delta;
			//orbit velocity
			if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
				real_t orbit_amount = tex_orbit_velocity * Math::lerp(parameters_min[PARAM_ORBIT_VELOCITY], parameters_max[PARAM_ORBIT_VELOCITY], rand_from_seed(alt_seed));
				if (orbit_amount != 0.0) {
					real_t ang = orbit_amount * local_delta * Math_TAU;
					// Not sure why the ParticleProcessMaterial code uses a clockwise rotation matrix,
					// but we use -ang here to reproduce its behavior.
					Transform2D rot = Transform2D(-ang, Vector2());
					Vector2 rotv = rot.basis_xform(Vector2(diff.x, diff.y));
					p.transform.origin -= Vector3(diff.x, diff.y, 0);
					p.transform.origin += Vector3(rotv.x, rotv.y, 0);
				}
			}
			if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY].is_valid()) {
				p.velocity = p.velocity.normalized() * tex_linear_velocity;
			}

			if (parameters_max[PARAM_DAMPING] + tex_damping > 0.0) {
				real_t v = p.velocity.length();
				real_t damp = tex_damping * Math::lerp(parameters_min[PARAM_DAMPING], parameters_max[PARAM_DAMPING], rand_from_seed(alt_seed));
				v -= damp * local_delta;
				if (v < 0.0) {
					p.velocity = Vector3();
				} else {
					p.velocity = p.velocity.normalized() * v;
				}
			}
			real_t base_angle = (tex_angle)*Math::lerp(parameters_min[PARAM_ANGLE], parameters_max[PARAM_ANGLE], p.angle_rand);
			base_angle += p.custom[1] * lifetime * tex_angular_velocity * Math::lerp(parameters_min[PARAM_ANGULAR_VELOCITY], parameters_max[PARAM_ANGULAR_VELOCITY], rand_from_seed(alt_seed));
			p.custom[0] = Math::deg_to_rad(base_angle); //angle
			p.custom[2] = tex_anim_offset * Math::lerp(parameters_min[PARAM_ANIM_OFFSET], parameters_max[PARAM_ANIM_OFFSET], p.anim_offset_rand) + tv * tex_anim_speed * Math::lerp(parameters_min[PARAM_ANIM_SPEED], parameters_max[PARAM_ANIM_SPEED], rand_from_seed(alt_seed)); //angle
		}
		//apply color
		//apply hue rotation

		Vector3 tex_scale = Vector3(1.0, 1.0, 1.0);
		if (split_scale) {
			if (scale_curve_x.is_valid()) {
				tex_scale.x = scale_curve_x->sample(tv);
			} else {
				tex_scale.x = 1.0;
			}
			if (scale_curve_y.is_valid()) {
				tex_scale.y = scale_curve_y->sample(tv);
			} else {
				tex_scale.y = 1.0;
			}
			if (scale_curve_z.is_valid()) {
				tex_scale.z = scale_curve_z->sample(tv);
			} else {
				tex_scale.z = 1.0;
			}
		} else {
			if (curve_parameters[PARAM_SCALE].is_valid()) {
				float tmp_scale = curve_parameters[PARAM_SCALE]->sample(tv);
				tex_scale.x = tmp_scale;
				tex_scale.y = tmp_scale;
				tex_scale.z = tmp_scale;
			}
		}

		real_t tex_hue_variation = 0.0;
		if (curve_parameters[PARAM_HUE_VARIATION].is_valid()) {
			tex_hue_variation = curve_parameters[PARAM_HUE_VARIATION]->sample(tv);
		}

		real_t hue_rot_angle = (tex_hue_variation)*Math_TAU * Math::lerp(parameters_min[PARAM_HUE_VARIATION], parameters_max[PARAM_HUE_VARIATION], p.hue_rot_rand);
		real_t hue_rot_c = Math::cos(hue_rot_angle);
		real_t hue_rot_s = Math::sin(hue_rot_angle);

		Basis hue_rot_mat;
		{
			Basis mat1(0.299, 0.587, 0.114, 0.299, 0.587, 0.114, 0.299, 0.587, 0.114);
			Basis mat2(0.701, -0.587, -0.114, -0.299, 0.413, -0.114, -0.300, -0.588, 0.886);
			Basis mat3(0.168, 0.330, -0.497, -0.328, 0.035, 0.292, 1.250, -1.050, -0.203);

			for (int j = 0; j < 3; j++) {
				hue_rot_mat[j] = mat1[j] + mat2[j] * hue_rot_c + mat3[j] * hue_rot_s;
			}
		}

		if (color_ramp.is_valid()) {
			p.color = color_ramp->get_color_at_offset(tv) * color;
		} else {
			p.color = color;
		}

		Vector3 color_rgb = hue_rot_mat.xform_inv(Vector3(p.color.r, p.color.g, p.color.b));
		p.color.r = color_rgb.x;
		p.color.g = color_rgb.y;
		p.color.b = color_rgb.z;

		p.color *= p.base_color * p.start_color_rand;

		if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
			if (particle_flags[PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY]) {
				if (p.velocity.length() > 0.0) {
					p.transform.basis.set_column(1, p.velocity.normalized());
				} else {
					p.transform.basis.set_column(1, p.transform.basis.get_column(1));
				}
				p.transform.basis.set_column(0, p.transform.basis.get_column(1).cross(p.transform.basis.get_column(2)).normalized());
				p.transform.basis.set_column(2, Vector3(0, 0, 1));

			} else {
				p.transform.basis.set_column(0, Vector3(Math::cos(p.custom[0]), -Math::sin(p.custom[0]), 0.0));
				p.transform.basis.set_column(1, Vector3(Math::sin(p.custom[0]), Math::cos(p.custom[0]), 0.0));
				p.transform.basis.set_column(2, Vector3(0, 0, 1));
			}

		} else {
			//orient particle Y towards velocity
			if (particle_flags[PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY]) {
				if (p.velocity.length() > 0.0) {
					p.transform.basis.set_column(1, p.velocity.normalized());
				} else {
					p.transform.basis.set_column(1, p.transform.basis.get_column(1).normalized());
				}
				if (p.transform.basis.get_column(1) == p.transform.basis.get_column(0)) {
					p.transform.basis.set_column(0, p.transform.basis.get_column(1).cross(p.transform.basis.get_column(2)).normalized());
					p.transform.basis.set_column(2, p.transform.basis.get_column(0).cross(p.transform.basis.get_column(1)).normalized());
				} else {
					p.transform.basis.set_column(2, p.transform.basis.get_column(0).cross(p.transform.basis.get_column(1)).normalized());
					p.transform.basis.set_column(0, p.transform.basis.get_column(1).cross(p.transform.basis.get_column(2)).normalized());
				}
			} else {
				p.transform.basis.orthonormalize();
			}

			//turn particle by rotation in Y
			if (particle_flags[PARTICLE_FLAG_ROTATE_Y]) {
				Basis rot_y(Vector3(0, 1, 0), p.custom[0]);
				p.transform.basis = rot_y;
			}
		}

		p.transform.basis = p.transform.basis.orthonormalized();
		//scale by scale

		Vector3 base_scale = tex_scale * Math::lerp(parameters_min[PARAM_SCALE], parameters_max[PARAM_SCALE], p.scale_rand);
		if (base_scale.x < CMP_EPSILON) {
			base_scale.x = CMP_EPSILON;
		}
		if (base_scale.y < CMP_EPSILON) {
			base_scale.y = CMP_EPSILON;
		}
		if (base_scale.z < CMP_EPSILON) {
			base_scale.z = CMP_EPSILON;
		}

		p.transform.basis.scale(base_scale);

		if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
			p.velocity.z = 0.0;
			p.transform.origin.z = 0.0;
		}

		p.transform.origin += p.velocity * local_delta;

		should_be_active = true;
	}
	if (!Math::is_equal_approx(time, 0.0) && active && !should_be_active) {
		active = false;
		emit_signal(SceneStringName(finished));
	}
}

void CPUParticles3D::_update_particle_data_buffer() {
	MutexLock lock(update_mutex);

	int pc = particles.size();

	int *ow;
	int *order = nullptr;

	float *w = particle_data.ptrw();
	const Particle *r = particles.ptr();
	float *ptr = w;

	if (draw_order != DRAW_ORDER_INDEX) {
		ow = particle_order.ptrw();
		order = ow;

		for (int i = 0; i < pc; i++) {
			order[i] = i;
		}
		if (draw_order == DRAW_ORDER_LIFETIME) {
			SortArray<int, SortLifetime> sorter;
			sorter.compare.particles = r;
			sorter.sort(order, pc);
		} else if (draw_order == DRAW_ORDER_VIEW_DEPTH) {
			ERR_FAIL_NULL(get_viewport());
			Camera3D *c = get_viewport()->get_camera_3d();
			if (c) {
				Vector3 dir = c->get_global_transform().basis.get_column(2); //far away to close

				if (local_coords) {
					// will look different from Particles in editor as this is based on the camera in the scenetree
					// and not the editor camera
					dir = inv_emission_transform.xform(dir).normalized();
				} else {
					dir = dir.normalized();
				}

				SortArray<int, SortAxis> sorter;
				sorter.compare.particles = r;
				sorter.compare.axis = dir;
				sorter.sort(order, pc);
			}
		}
	}

	for (int i = 0; i < pc; i++) {
		int idx = order ? order[i] : i;

		Transform3D t = r[idx].transform;

		if (!local_coords) {
			t = inv_emission_transform * t;
		}

		if (r[idx].active) {
			ptr[0] = t.basis.rows[0][0];
			ptr[1] = t.basis.rows[0][1];
			ptr[2] = t.basis.rows[0][2];
			ptr[3] = t.origin.x;
			ptr[4] = t.basis.rows[1][0];
			ptr[5] = t.basis.rows[1][1];
			ptr[6] = t.basis.rows[1][2];
			ptr[7] = t.origin.y;
			ptr[8] = t.basis.rows[2][0];
			ptr[9] = t.basis.rows[2][1];
			ptr[10] = t.basis.rows[2][2];
			ptr[11] = t.origin.z;
		} else {
			memset(ptr, 0, sizeof(float) * 12);
		}

		Color c = r[idx].color;

		ptr[12] = c.r;
		ptr[13] = c.g;
		ptr[14] = c.b;
		ptr[15] = c.a;

		ptr[16] = r[idx].custom[0];
		ptr[17] = r[idx].custom[1];
		ptr[18] = r[idx].custom[2];
		ptr[19] = r[idx].custom[3];

		ptr += 20;
	}

	can_update.set();
}

void CPUParticles3D::_set_redraw(bool p_redraw) {
	if (redraw == p_redraw) {
		return;
	}
	redraw = p_redraw;

	{
		MutexLock lock(update_mutex);

		if (redraw) {
			RS::get_singleton()->connect("frame_pre_draw", callable_mp(this, &CPUParticles3D::_update_render_thread));
			RS::get_singleton()->instance_geometry_set_flag(get_instance(), RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE, true);
			RS::get_singleton()->multimesh_set_visible_instances(multimesh, -1);
		} else {
			if (RS::get_singleton()->is_connected("frame_pre_draw", callable_mp(this, &CPUParticles3D::_update_render_thread))) {
				RS::get_singleton()->disconnect("frame_pre_draw", callable_mp(this, &CPUParticles3D::_update_render_thread));
			}
			RS::get_singleton()->instance_geometry_set_flag(get_instance(), RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE, false);
			RS::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
		}
	}
}

void CPUParticles3D::_update_render_thread() {
	MutexLock lock(update_mutex);

	if (can_update.is_set()) {
		RS::get_singleton()->multimesh_set_buffer(multimesh, particle_data);
		can_update.clear(); //wait for next time
	}
}

void CPUParticles3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_process_internal(emitting);

			// first update before rendering to avoid one frame delay after emitting starts
			if (emitting && (time == 0)) {
				_update_internal();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_set_redraw(false);
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			// first update before rendering to avoid one frame delay after emitting starts
			if (emitting && (time == 0)) {
				_update_internal();
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_internal();
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			inv_emission_transform = get_global_transform().affine_inverse();

			if (!local_coords) {
				int pc = particles.size();

				float *w = particle_data.ptrw();
				const Particle *r = particles.ptr();
				float *ptr = w;

				for (int i = 0; i < pc; i++) {
					Transform3D t = inv_emission_transform * r[i].transform;

					if (r[i].active) {
						ptr[0] = t.basis.rows[0][0];
						ptr[1] = t.basis.rows[0][1];
						ptr[2] = t.basis.rows[0][2];
						ptr[3] = t.origin.x;
						ptr[4] = t.basis.rows[1][0];
						ptr[5] = t.basis.rows[1][1];
						ptr[6] = t.basis.rows[1][2];
						ptr[7] = t.origin.y;
						ptr[8] = t.basis.rows[2][0];
						ptr[9] = t.basis.rows[2][1];
						ptr[10] = t.basis.rows[2][2];
						ptr[11] = t.origin.z;
					} else {
						memset(ptr, 0, sizeof(float) * 12);
					}

					ptr += 20;
				}

				can_update.set();
			}
		} break;
	}
}

void CPUParticles3D::convert_from_particles(Node *p_particles) {
	GPUParticles3D *gpu_particles = Object::cast_to<GPUParticles3D>(p_particles);
	ERR_FAIL_NULL_MSG(gpu_particles, "Only GPUParticles3D nodes can be converted to CPUParticles3D.");

	set_emitting(gpu_particles->is_emitting());
	set_amount(gpu_particles->get_amount());
	set_lifetime(gpu_particles->get_lifetime());
	set_one_shot(gpu_particles->get_one_shot());
	set_pre_process_time(gpu_particles->get_pre_process_time());
	set_explosiveness_ratio(gpu_particles->get_explosiveness_ratio());
	set_randomness_ratio(gpu_particles->get_randomness_ratio());
	set_visibility_aabb(gpu_particles->get_visibility_aabb());
	set_use_local_coordinates(gpu_particles->get_use_local_coordinates());
	set_fixed_fps(gpu_particles->get_fixed_fps());
	set_fractional_delta(gpu_particles->get_fractional_delta());
	set_speed_scale(gpu_particles->get_speed_scale());
	set_draw_order(DrawOrder(gpu_particles->get_draw_order()));
	set_mesh(gpu_particles->get_draw_pass_mesh(0));

	Ref<ParticleProcessMaterial> material = gpu_particles->get_process_material();
	if (material.is_null()) {
		return;
	}

	set_direction(material->get_direction());
	set_spread(material->get_spread());
	set_flatness(material->get_flatness());

	set_color(material->get_color());

	Ref<GradientTexture1D> gt = material->get_color_ramp();
	if (gt.is_valid()) {
		set_color_ramp(gt->get_gradient());
	}

	Ref<GradientTexture1D> gti = material->get_color_initial_ramp();
	if (gti.is_valid()) {
		set_color_initial_ramp(gti->get_gradient());
	}

	set_particle_flag(PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY, material->get_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY));
	set_particle_flag(PARTICLE_FLAG_ROTATE_Y, material->get_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_ROTATE_Y));
	set_particle_flag(PARTICLE_FLAG_DISABLE_Z, material->get_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DISABLE_Z));

	set_emission_shape(EmissionShape(material->get_emission_shape()));
	set_emission_sphere_radius(material->get_emission_sphere_radius());
	set_emission_box_extents(material->get_emission_box_extents());
	set_emission_ring_height(material->get_emission_ring_height());
	set_emission_ring_radius(material->get_emission_ring_radius());
	set_emission_ring_inner_radius(material->get_emission_ring_inner_radius());
	set_emission_ring_cone_angle(material->get_emission_ring_cone_angle());

	Ref<CurveXYZTexture> scale3D = material->get_param_texture(ParticleProcessMaterial::PARAM_SCALE);
	if (scale3D.is_valid()) {
		split_scale = true;
		scale_curve_x = scale3D->get_curve_x();
		scale_curve_y = scale3D->get_curve_y();
		scale_curve_z = scale3D->get_curve_z();
	}

	set_gravity(material->get_gravity());
	set_lifetime_randomness(material->get_lifetime_randomness());

#define CONVERT_PARAM(m_param)                                                                  \
	set_param_min(m_param, material->get_param_min(ParticleProcessMaterial::m_param));          \
	{                                                                                           \
		Ref<CurveTexture> ctex = material->get_param_texture(ParticleProcessMaterial::m_param); \
		if (ctex.is_valid())                                                                    \
			set_param_curve(m_param, ctex->get_curve());                                        \
	}                                                                                           \
	set_param_max(m_param, material->get_param_max(ParticleProcessMaterial::m_param));

	CONVERT_PARAM(PARAM_INITIAL_LINEAR_VELOCITY);
	CONVERT_PARAM(PARAM_ANGULAR_VELOCITY);
	CONVERT_PARAM(PARAM_ORBIT_VELOCITY);
	CONVERT_PARAM(PARAM_LINEAR_ACCEL);
	CONVERT_PARAM(PARAM_RADIAL_ACCEL);
	CONVERT_PARAM(PARAM_TANGENTIAL_ACCEL);
	CONVERT_PARAM(PARAM_DAMPING);
	CONVERT_PARAM(PARAM_ANGLE);
	CONVERT_PARAM(PARAM_SCALE);
	CONVERT_PARAM(PARAM_HUE_VARIATION);
	CONVERT_PARAM(PARAM_ANIM_SPEED);
	CONVERT_PARAM(PARAM_ANIM_OFFSET);

#undef CONVERT_PARAM
}

void CPUParticles3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_emitting", "emitting"), &CPUParticles3D::set_emitting);
	ClassDB::bind_method(D_METHOD("set_amount", "amount"), &CPUParticles3D::set_amount);
	ClassDB::bind_method(D_METHOD("set_lifetime", "secs"), &CPUParticles3D::set_lifetime);
	ClassDB::bind_method(D_METHOD("set_one_shot", "enable"), &CPUParticles3D::set_one_shot);
	ClassDB::bind_method(D_METHOD("set_pre_process_time", "secs"), &CPUParticles3D::set_pre_process_time);
	ClassDB::bind_method(D_METHOD("set_explosiveness_ratio", "ratio"), &CPUParticles3D::set_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("set_randomness_ratio", "ratio"), &CPUParticles3D::set_randomness_ratio);
	ClassDB::bind_method(D_METHOD("set_visibility_aabb", "aabb"), &CPUParticles3D::set_visibility_aabb);
	ClassDB::bind_method(D_METHOD("set_lifetime_randomness", "random"), &CPUParticles3D::set_lifetime_randomness);
	ClassDB::bind_method(D_METHOD("set_use_local_coordinates", "enable"), &CPUParticles3D::set_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("set_fixed_fps", "fps"), &CPUParticles3D::set_fixed_fps);
	ClassDB::bind_method(D_METHOD("set_fractional_delta", "enable"), &CPUParticles3D::set_fractional_delta);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &CPUParticles3D::set_speed_scale);

	ClassDB::bind_method(D_METHOD("is_emitting"), &CPUParticles3D::is_emitting);
	ClassDB::bind_method(D_METHOD("get_amount"), &CPUParticles3D::get_amount);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &CPUParticles3D::get_lifetime);
	ClassDB::bind_method(D_METHOD("get_one_shot"), &CPUParticles3D::get_one_shot);
	ClassDB::bind_method(D_METHOD("get_pre_process_time"), &CPUParticles3D::get_pre_process_time);
	ClassDB::bind_method(D_METHOD("get_explosiveness_ratio"), &CPUParticles3D::get_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("get_randomness_ratio"), &CPUParticles3D::get_randomness_ratio);
	ClassDB::bind_method(D_METHOD("get_visibility_aabb"), &CPUParticles3D::get_visibility_aabb);
	ClassDB::bind_method(D_METHOD("get_lifetime_randomness"), &CPUParticles3D::get_lifetime_randomness);
	ClassDB::bind_method(D_METHOD("get_use_local_coordinates"), &CPUParticles3D::get_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("get_fixed_fps"), &CPUParticles3D::get_fixed_fps);
	ClassDB::bind_method(D_METHOD("get_fractional_delta"), &CPUParticles3D::get_fractional_delta);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &CPUParticles3D::get_speed_scale);

	ClassDB::bind_method(D_METHOD("set_draw_order", "order"), &CPUParticles3D::set_draw_order);

	ClassDB::bind_method(D_METHOD("get_draw_order"), &CPUParticles3D::get_draw_order);

	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &CPUParticles3D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &CPUParticles3D::get_mesh);

	ClassDB::bind_method(D_METHOD("set_use_fixed_seed", "use_fixed_seed"), &CPUParticles3D::set_use_fixed_seed);
	ClassDB::bind_method(D_METHOD("get_use_fixed_seed"), &CPUParticles3D::get_use_fixed_seed);

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &CPUParticles3D::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &CPUParticles3D::get_seed);

	ClassDB::bind_method(D_METHOD("restart", "keep_seed"), &CPUParticles3D::restart, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("request_particles_process", "process_time"), &CPUParticles3D::request_particles_process);
	ClassDB::bind_method(D_METHOD("capture_aabb"), &CPUParticles3D::capture_aabb);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting", PROPERTY_HINT_ONESHOT), "set_emitting", "is_emitting");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "amount", PROPERTY_HINT_RANGE, "1,1000000,1,exp"), "set_amount", "get_amount"); // FIXME: Evaluate support for `exp` in integer properties, or remove this.
	ADD_GROUP("Time", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.01,600.0,0.01,or_greater,exp,suffix:s"), "set_lifetime", "get_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "preprocess", PROPERTY_HINT_RANGE, "0.00,10.0,0.01,or_greater,exp,suffix:s"), "set_pre_process_time", "get_pre_process_time");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "explosiveness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_fixed_seed"), "set_use_fixed_seed", "get_use_fixed_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_RANGE, "0," + itos(UINT32_MAX) + ",1"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime_randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lifetime_randomness", "get_lifetime_randomness");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_fps", PROPERTY_HINT_RANGE, "0,1000,1,suffix:FPS"), "set_fixed_fps", "get_fixed_fps");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
	ADD_GROUP("Drawing", "");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "visibility_aabb", PROPERTY_HINT_NONE, "suffix:m"), "set_visibility_aabb", "get_visibility_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "draw_order", PROPERTY_HINT_ENUM, "Index,Lifetime,View Depth"), "set_draw_order", "get_draw_order");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");

	BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
	BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);
	BIND_ENUM_CONSTANT(DRAW_ORDER_VIEW_DEPTH);

	////////////////////////////////

	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &CPUParticles3D::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &CPUParticles3D::get_direction);

	ClassDB::bind_method(D_METHOD("set_spread", "degrees"), &CPUParticles3D::set_spread);
	ClassDB::bind_method(D_METHOD("get_spread"), &CPUParticles3D::get_spread);

	ClassDB::bind_method(D_METHOD("set_flatness", "amount"), &CPUParticles3D::set_flatness);
	ClassDB::bind_method(D_METHOD("get_flatness"), &CPUParticles3D::get_flatness);

	ClassDB::bind_method(D_METHOD("set_param_min", "param", "value"), &CPUParticles3D::set_param_min);
	ClassDB::bind_method(D_METHOD("get_param_min", "param"), &CPUParticles3D::get_param_min);

	ClassDB::bind_method(D_METHOD("set_param_max", "param", "value"), &CPUParticles3D::set_param_max);
	ClassDB::bind_method(D_METHOD("get_param_max", "param"), &CPUParticles3D::get_param_max);

	ClassDB::bind_method(D_METHOD("set_param_curve", "param", "curve"), &CPUParticles3D::set_param_curve);
	ClassDB::bind_method(D_METHOD("get_param_curve", "param"), &CPUParticles3D::get_param_curve);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &CPUParticles3D::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &CPUParticles3D::get_color);

	ClassDB::bind_method(D_METHOD("set_color_ramp", "ramp"), &CPUParticles3D::set_color_ramp);
	ClassDB::bind_method(D_METHOD("get_color_ramp"), &CPUParticles3D::get_color_ramp);

	ClassDB::bind_method(D_METHOD("set_color_initial_ramp", "ramp"), &CPUParticles3D::set_color_initial_ramp);
	ClassDB::bind_method(D_METHOD("get_color_initial_ramp"), &CPUParticles3D::get_color_initial_ramp);

	ClassDB::bind_method(D_METHOD("set_particle_flag", "particle_flag", "enable"), &CPUParticles3D::set_particle_flag);
	ClassDB::bind_method(D_METHOD("get_particle_flag", "particle_flag"), &CPUParticles3D::get_particle_flag);

	ClassDB::bind_method(D_METHOD("set_emission_shape", "shape"), &CPUParticles3D::set_emission_shape);
	ClassDB::bind_method(D_METHOD("get_emission_shape"), &CPUParticles3D::get_emission_shape);

	ClassDB::bind_method(D_METHOD("set_emission_sphere_radius", "radius"), &CPUParticles3D::set_emission_sphere_radius);
	ClassDB::bind_method(D_METHOD("get_emission_sphere_radius"), &CPUParticles3D::get_emission_sphere_radius);

	ClassDB::bind_method(D_METHOD("set_emission_box_extents", "extents"), &CPUParticles3D::set_emission_box_extents);
	ClassDB::bind_method(D_METHOD("get_emission_box_extents"), &CPUParticles3D::get_emission_box_extents);

	ClassDB::bind_method(D_METHOD("set_emission_points", "array"), &CPUParticles3D::set_emission_points);
	ClassDB::bind_method(D_METHOD("get_emission_points"), &CPUParticles3D::get_emission_points);

	ClassDB::bind_method(D_METHOD("set_emission_normals", "array"), &CPUParticles3D::set_emission_normals);
	ClassDB::bind_method(D_METHOD("get_emission_normals"), &CPUParticles3D::get_emission_normals);

	ClassDB::bind_method(D_METHOD("set_emission_colors", "array"), &CPUParticles3D::set_emission_colors);
	ClassDB::bind_method(D_METHOD("get_emission_colors"), &CPUParticles3D::get_emission_colors);

	ClassDB::bind_method(D_METHOD("set_emission_ring_axis", "axis"), &CPUParticles3D::set_emission_ring_axis);
	ClassDB::bind_method(D_METHOD("get_emission_ring_axis"), &CPUParticles3D::get_emission_ring_axis);

	ClassDB::bind_method(D_METHOD("set_emission_ring_height", "height"), &CPUParticles3D::set_emission_ring_height);
	ClassDB::bind_method(D_METHOD("get_emission_ring_height"), &CPUParticles3D::get_emission_ring_height);

	ClassDB::bind_method(D_METHOD("set_emission_ring_radius", "radius"), &CPUParticles3D::set_emission_ring_radius);
	ClassDB::bind_method(D_METHOD("get_emission_ring_radius"), &CPUParticles3D::get_emission_ring_radius);

	ClassDB::bind_method(D_METHOD("set_emission_ring_inner_radius", "inner_radius"), &CPUParticles3D::set_emission_ring_inner_radius);
	ClassDB::bind_method(D_METHOD("get_emission_ring_inner_radius"), &CPUParticles3D::get_emission_ring_inner_radius);

	ClassDB::bind_method(D_METHOD("set_emission_ring_cone_angle", "cone_angle"), &CPUParticles3D::set_emission_ring_cone_angle);
	ClassDB::bind_method(D_METHOD("get_emission_ring_cone_angle"), &CPUParticles3D::get_emission_ring_cone_angle);

	ClassDB::bind_method(D_METHOD("get_gravity"), &CPUParticles3D::get_gravity);
	ClassDB::bind_method(D_METHOD("set_gravity", "accel_vec"), &CPUParticles3D::set_gravity);

	ClassDB::bind_method(D_METHOD("get_split_scale"), &CPUParticles3D::get_split_scale);
	ClassDB::bind_method(D_METHOD("set_split_scale", "split_scale"), &CPUParticles3D::set_split_scale);

	ClassDB::bind_method(D_METHOD("get_scale_curve_x"), &CPUParticles3D::get_scale_curve_x);
	ClassDB::bind_method(D_METHOD("set_scale_curve_x", "scale_curve"), &CPUParticles3D::set_scale_curve_x);

	ClassDB::bind_method(D_METHOD("get_scale_curve_y"), &CPUParticles3D::get_scale_curve_y);
	ClassDB::bind_method(D_METHOD("set_scale_curve_y", "scale_curve"), &CPUParticles3D::set_scale_curve_y);

	ClassDB::bind_method(D_METHOD("get_scale_curve_z"), &CPUParticles3D::get_scale_curve_z);
	ClassDB::bind_method(D_METHOD("set_scale_curve_z", "scale_curve"), &CPUParticles3D::set_scale_curve_z);

	ClassDB::bind_method(D_METHOD("convert_from_particles", "particles"), &CPUParticles3D::convert_from_particles);

	ADD_SIGNAL(MethodInfo("finished"));

	ADD_GROUP("Emission Shape", "emission_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emission_shape", PROPERTY_HINT_ENUM, "Point,Sphere,Sphere Surface,Box,Points,Directed Points,Ring", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_emission_shape", "get_emission_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_sphere_radius", PROPERTY_HINT_RANGE, "0.01,128,0.01"), "set_emission_sphere_radius", "get_emission_sphere_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_box_extents"), "set_emission_box_extents", "get_emission_box_extents");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "emission_points"), "set_emission_points", "get_emission_points");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "emission_normals"), "set_emission_normals", "get_emission_normals");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_COLOR_ARRAY, "emission_colors"), "set_emission_colors", "get_emission_colors");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_ring_axis"), "set_emission_ring_axis", "get_emission_ring_axis");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_height", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_height", "get_emission_ring_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_radius", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_radius", "get_emission_ring_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_inner_radius", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_inner_radius", "get_emission_ring_inner_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_cone_angle", PROPERTY_HINT_RANGE, "0,90,0.01,degrees"), "set_emission_ring_cone_angle", "get_emission_ring_cone_angle");
	ADD_GROUP("Particle Flags", "particle_flag_");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_align_y"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_rotate_y"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_ROTATE_Y);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_disable_z"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_DISABLE_Z);
	ADD_GROUP("Direction", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread", PROPERTY_HINT_RANGE, "0,180,0.01"), "set_spread", "get_spread");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flatness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_flatness", "get_flatness");
	ADD_GROUP("Gravity", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");
	ADD_GROUP("Initial Velocity", "initial_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "initial_velocity_min", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_param_min", "get_param_min", PARAM_INITIAL_LINEAR_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "initial_velocity_max", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_param_max", "get_param_max", PARAM_INITIAL_LINEAR_VELOCITY);
	ADD_GROUP("Angular Velocity", "angular_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_velocity_min", PROPERTY_HINT_RANGE, "-720,720,0.01,or_less,or_greater"), "set_param_min", "get_param_min", PARAM_ANGULAR_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angular_velocity_max", PROPERTY_HINT_RANGE, "-720,720,0.01,or_less,or_greater"), "set_param_max", "get_param_max", PARAM_ANGULAR_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "angular_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANGULAR_VELOCITY);
	ADD_GROUP("Orbit Velocity", "orbit_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "orbit_velocity_min", PROPERTY_HINT_RANGE, "-1000,1000,0.01,or_less,or_greater"), "set_param_min", "get_param_min", PARAM_ORBIT_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "orbit_velocity_max", PROPERTY_HINT_RANGE, "-1000,1000,0.01,or_less,or_greater"), "set_param_max", "get_param_max", PARAM_ORBIT_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "orbit_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_ORBIT_VELOCITY);
	ADD_GROUP("Linear Accel", "linear_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_accel_min", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_min", "get_param_min", PARAM_LINEAR_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "linear_accel_max", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_max", "get_param_max", PARAM_LINEAR_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "linear_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_LINEAR_ACCEL);
	ADD_GROUP("Radial Accel", "radial_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "radial_accel_min", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_min", "get_param_min", PARAM_RADIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "radial_accel_max", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_max", "get_param_max", PARAM_RADIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "radial_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_RADIAL_ACCEL);
	ADD_GROUP("Tangential Accel", "tangential_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "tangential_accel_min", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_min", "get_param_min", PARAM_TANGENTIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "tangential_accel_max", PROPERTY_HINT_RANGE, "-100,100,0.01,or_less,or_greater"), "set_param_max", "get_param_max", PARAM_TANGENTIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "tangential_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_TANGENTIAL_ACCEL);
	ADD_GROUP("Damping", "");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "damping_min", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_param_min", "get_param_min", PARAM_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "damping_max", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), "set_param_max", "get_param_max", PARAM_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "damping_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_DAMPING);
	ADD_GROUP("Angle", "");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angle_min", PROPERTY_HINT_RANGE, "-720,720,0.1,or_less,or_greater,degrees"), "set_param_min", "get_param_min", PARAM_ANGLE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "angle_max", PROPERTY_HINT_RANGE, "-720,720,0.1,or_less,or_greater,degrees"), "set_param_max", "get_param_max", PARAM_ANGLE);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "angle_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANGLE);
	ADD_GROUP("Scale", "");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "scale_amount_min", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_param_min", "get_param_min", PARAM_SCALE);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "scale_amount_max", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_param_max", "get_param_max", PARAM_SCALE);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "scale_amount_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_SCALE);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "split_scale"), "set_split_scale", "get_split_scale");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scale_curve_x", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_scale_curve_x", "get_scale_curve_x");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scale_curve_y", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_scale_curve_y", "get_scale_curve_y");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scale_curve_z", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_scale_curve_z", "get_scale_curve_z");
	ADD_GROUP("Color", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_ramp", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_color_ramp", "get_color_ramp");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_initial_ramp", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_color_initial_ramp", "get_color_initial_ramp");

	ADD_GROUP("Hue Variation", "hue_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "hue_variation_min", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_param_min", "get_param_min", PARAM_HUE_VARIATION);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "hue_variation_max", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_param_max", "get_param_max", PARAM_HUE_VARIATION);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "hue_variation_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_HUE_VARIATION);
	ADD_GROUP("Animation", "anim_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "anim_speed_min", PROPERTY_HINT_RANGE, "0,128,0.01,or_greater,or_less"), "set_param_min", "get_param_min", PARAM_ANIM_SPEED);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "anim_speed_max", PROPERTY_HINT_RANGE, "0,128,0.01,or_greater,or_less"), "set_param_max", "get_param_max", PARAM_ANIM_SPEED);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "anim_speed_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANIM_SPEED);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "anim_offset_min", PROPERTY_HINT_RANGE, "0,1,0.0001"), "set_param_min", "get_param_min", PARAM_ANIM_OFFSET);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "anim_offset_max", PROPERTY_HINT_RANGE, "0,1,0.0001"), "set_param_max", "get_param_max", PARAM_ANIM_OFFSET);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "anim_offset_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANIM_OFFSET);

	BIND_ENUM_CONSTANT(PARAM_INITIAL_LINEAR_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ORBIT_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_RADIAL_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_TANGENTIAL_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGLE);
	BIND_ENUM_CONSTANT(PARAM_SCALE);
	BIND_ENUM_CONSTANT(PARAM_HUE_VARIATION);
	BIND_ENUM_CONSTANT(PARAM_ANIM_SPEED);
	BIND_ENUM_CONSTANT(PARAM_ANIM_OFFSET);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_ROTATE_Y);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_DISABLE_Z);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_MAX);

	BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE_SURFACE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINTS);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_DIRECTED_POINTS);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_RING);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_MAX);
}

CPUParticles3D::CPUParticles3D() {
	set_notify_transform(true);

	multimesh = RenderingServer::get_singleton()->multimesh_create();
	RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
	set_base(multimesh);

	set_emitting(true);
	set_amount(8);

	rng.instantiate();

	set_param_min(PARAM_INITIAL_LINEAR_VELOCITY, 0);
	set_param_min(PARAM_ANGULAR_VELOCITY, 0);
	set_param_min(PARAM_ORBIT_VELOCITY, 0);
	set_param_min(PARAM_LINEAR_ACCEL, 0);
	set_param_min(PARAM_RADIAL_ACCEL, 0);
	set_param_min(PARAM_TANGENTIAL_ACCEL, 0);
	set_param_min(PARAM_DAMPING, 0);
	set_param_min(PARAM_ANGLE, 0);
	set_param_min(PARAM_SCALE, 1);
	set_param_min(PARAM_HUE_VARIATION, 0);
	set_param_min(PARAM_ANIM_SPEED, 0);
	set_param_min(PARAM_ANIM_OFFSET, 0);
	set_param_max(PARAM_INITIAL_LINEAR_VELOCITY, 0);
	set_param_max(PARAM_ANGULAR_VELOCITY, 0);
	set_param_max(PARAM_ORBIT_VELOCITY, 0);
	set_param_max(PARAM_LINEAR_ACCEL, 0);
	set_param_max(PARAM_RADIAL_ACCEL, 0);
	set_param_max(PARAM_TANGENTIAL_ACCEL, 0);
	set_param_max(PARAM_DAMPING, 0);
	set_param_max(PARAM_ANGLE, 0);
	set_param_max(PARAM_SCALE, 1);
	set_param_max(PARAM_HUE_VARIATION, 0);
	set_param_max(PARAM_ANIM_SPEED, 0);
	set_param_max(PARAM_ANIM_OFFSET, 0);
	set_emission_shape(EMISSION_SHAPE_POINT);
	set_emission_sphere_radius(1);
	set_emission_box_extents(Vector3(1, 1, 1));
	set_emission_ring_axis(Vector3(0, 0, 1.0));
	set_emission_ring_height(1);
	set_emission_ring_radius(1);
	set_emission_ring_inner_radius(0);
	set_emission_ring_cone_angle(90);

	set_gravity(Vector3(0, -9.8, 0));

	for (int i = 0; i < PARTICLE_FLAG_MAX; i++) {
		particle_flags[i] = false;
	}

	set_color(Color(1, 1, 1, 1));
}

CPUParticles3D::~CPUParticles3D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(multimesh);
}
