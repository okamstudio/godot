/**************************************************************************/
/*  particles_storage.h                                                   */
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

#include "servers/rendering_server.h"

class RendererParticlesStorage {
public:
	virtual ~RendererParticlesStorage() {}

	/* PARTICLES */

	virtual RID particles_allocate() = 0;
	virtual void particles_initialize(RID p_rid) = 0;
	virtual void particles_free(RID p_rid) = 0;

	virtual void particles_set_mode(RID p_particles, RS::ParticlesMode p_mode) = 0;

	virtual void particles_set_emitting(RID p_particles, bool p_emitting) = 0;
	virtual bool particles_get_emitting(RID p_particles) = 0;

	virtual void particles_set_amount(RID p_particles, int p_amount) = 0;
	virtual void particles_set_amount_ratio(RID p_particles, float p_amount_ratio) = 0;
	virtual void particles_set_lifetime(RID p_particles, double p_lifetime) = 0;
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) = 0;
	virtual void particles_set_pre_process_time(RID p_particles, double p_time) = 0;
	virtual void particles_request_process_time(RID p_particles, real_t p_request_process_time) = 0;
	virtual void particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) = 0;
	virtual void particles_set_randomness_ratio(RID p_particles, real_t p_ratio) = 0;
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) = 0;
	virtual void particles_set_speed_scale(RID p_particles, double p_scale) = 0;
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_process_material(RID p_particles, RID p_material) = 0;
	virtual RID particles_get_process_material(RID p_particles) const = 0;
	virtual void particles_set_fixed_fps(RID p_particles, int p_fps) = 0;
	virtual void particles_set_interpolate(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_collision_base_size(RID p_particles, real_t p_size) = 0;

	virtual void particles_set_transform_align(RID p_particles, RS::ParticlesTransformAlign p_transform_align) = 0;

	virtual void particles_set_seed(RID p_particles, uint32_t p_seed) = 0;

	virtual void particles_set_trails(RID p_particles, bool p_enable, double p_length) = 0;
	virtual void particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) = 0;

	virtual void particles_restart(RID p_particles) = 0;
	virtual void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) = 0;
	virtual void particles_set_subemitter(RID p_particles, RID p_subemitter_particles) = 0;

	virtual bool particles_is_inactive(RID p_particles) const = 0;

	virtual void particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) = 0;

	virtual void particles_set_draw_passes(RID p_particles, int p_count) = 0;
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) = 0;

	virtual void particles_request_process(RID p_particles) = 0;
	virtual AABB particles_get_current_aabb(RID p_particles) = 0;
	virtual AABB particles_get_aabb(RID p_particles) const = 0;

	virtual void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) = 0;
	virtual void particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) = 0;
	virtual void particles_set_interp_to_end(RID p_particles, float p_interp_to_end) = 0;

	virtual int particles_get_draw_passes(RID p_particles) const = 0;
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const = 0;

	virtual void particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) = 0;

	virtual void particles_add_collision(RID p_particles, RID p_particles_collision_instance) = 0;
	virtual void particles_remove_collision(RID p_particles, RID p_particles_collision_instance) = 0;

	virtual void update_particles() = 0;

	/* PARTICLES COLLISION */

	virtual RID particles_collision_allocate() = 0;
	virtual void particles_collision_initialize(RID p_rid) = 0;
	virtual void particles_collision_free(RID p_rid) = 0;

	virtual void particles_collision_set_collision_type(RID p_particles_collision, RS::ParticlesCollisionType p_type) = 0;
	virtual void particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) = 0;
	virtual void particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) = 0; //for spheres
	virtual void particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) = 0; //for non-spheres
	virtual void particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) = 0;
	virtual void particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) = 0;
	virtual void particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) = 0;
	virtual void particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) = 0; //for SDF and vector field, heightfield is dynamic
	virtual void particles_collision_height_field_update(RID p_particles_collision) = 0; //for SDF and vector field
	virtual void particles_collision_set_height_field_resolution(RID p_particles_collision, RS::ParticlesCollisionHeightfieldResolution p_resolution) = 0; //for SDF and vector field
	virtual AABB particles_collision_get_aabb(RID p_particles_collision) const = 0;
	virtual bool particles_collision_is_heightfield(RID p_particles_collision) const = 0;
	virtual uint32_t particles_collision_get_height_field_mask(RID p_particles_collision) const = 0;
	virtual void particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) = 0;

	//used from 2D and 3D
	virtual RID particles_collision_instance_create(RID p_collision) = 0;
	virtual void particles_collision_instance_free(RID p_rid) = 0;
	virtual void particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) = 0;
	virtual void particles_collision_instance_set_active(RID p_collision_instance, bool p_active) = 0;
};
