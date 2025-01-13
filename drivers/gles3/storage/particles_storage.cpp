/**************************************************************************/
/*  particles_storage.cpp                                                 */
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

#ifdef GLES3_ENABLED

#include "particles_storage.h"

#include "config.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "texture_storage.h"
#include "utilities.h"

#include "servers/rendering/rendering_server_default.h"

using namespace GLES3;

ParticlesStorage *ParticlesStorage::singleton = nullptr;

ParticlesStorage *ParticlesStorage::get_singleton() {
	return singleton;
}

ParticlesStorage::ParticlesStorage() {
	singleton = this;
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();

	{
		String global_defines;
		global_defines += "#define MAX_GLOBAL_SHADER_UNIFORMS 256\n"; // TODO: this is arbitrary for now
		material_storage->shaders.particles_process_shader.initialize(global_defines, 1);
	}
	{
		// default material and shader for particles shader
		particles_shader.default_shader = material_storage->shader_allocate();
		material_storage->shader_initialize(particles_shader.default_shader);
		material_storage->shader_set_code(particles_shader.default_shader, R"(
// Default particles shader.

shader_type particles;

void process() {
	COLOR = vec4(1.0);
}
)");
		particles_shader.default_material = material_storage->material_allocate();
		material_storage->material_initialize(particles_shader.default_material);
		material_storage->material_set_shader(particles_shader.default_material, particles_shader.default_shader);
	}
	{
		particles_shader.copy_shader.initialize();
		particles_shader.copy_shader_version = particles_shader.copy_shader.version_create();
	}
}

ParticlesStorage::~ParticlesStorage() {
	singleton = nullptr;
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();

	material_storage->material_free(particles_shader.default_material);
	material_storage->shader_free(particles_shader.default_shader);
	particles_shader.copy_shader.version_free(particles_shader.copy_shader_version);
}

/* PARTICLES */

RID ParticlesStorage::particles_allocate() {
	return particles_owner.allocate_rid();
}

void ParticlesStorage::particles_initialize(RID p_rid) {
	particles_owner.initialize_rid(p_rid);
}

void ParticlesStorage::particles_free(RID p_rid) {
	Particles *particles = particles_owner.get_or_null(p_rid);

	particles->dependency.deleted_notify(p_rid);
	particles->update_list.remove_from_list();

	_particles_free_data(particles);
	particles_owner.free(p_rid);
}

void ParticlesStorage::particles_set_mode(RID p_particles, RS::ParticlesMode p_mode) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	if (particles->mode == p_mode) {
		return;
	}

	_particles_free_data(particles);

	particles->mode = p_mode;
}

void ParticlesStorage::particles_set_emitting(RID p_particles, bool p_emitting) {
	ERR_FAIL_COND_MSG(GLES3::Config::get_singleton()->disable_particles_workaround, "Due to driver bugs, GPUParticles are not supported on Adreno 3XX devices. Please use CPUParticles instead.");

	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emitting = p_emitting;
}

bool ParticlesStorage::particles_get_emitting(RID p_particles) {
	if (GLES3::Config::get_singleton()->disable_particles_workaround) {
		return false;
	}

	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);

	return particles->emitting;
}

void ParticlesStorage::_particles_free_data(Particles *particles) {
	particles->userdata_count = 0;
	particles->instance_buffer_size_cache = 0;
	particles->instance_buffer_stride_cache = 0;
	particles->num_attrib_arrays_cache = 0;
	particles->process_buffer_stride_cache = 0;

	if (particles->front_process_buffer != 0) {
		glDeleteVertexArrays(1, &particles->front_vertex_array);
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->front_process_buffer);
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->front_instance_buffer);
		particles->front_vertex_array = 0;
		particles->front_process_buffer = 0;
		particles->front_instance_buffer = 0;

		glDeleteVertexArrays(1, &particles->back_vertex_array);
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->back_process_buffer);
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->back_instance_buffer);
		particles->back_vertex_array = 0;
		particles->back_process_buffer = 0;
		particles->back_instance_buffer = 0;
	}

	if (particles->sort_buffer != 0) {
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->last_frame_buffer);
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->sort_buffer);
		particles->last_frame_buffer = 0;
		particles->sort_buffer = 0;
		particles->sort_buffer_filled = false;
		particles->last_frame_buffer_filled = false;
	}

	if (particles->frame_params_ubo != 0) {
		GLES3::Utilities::get_singleton()->buffer_free_data(particles->frame_params_ubo);
		particles->frame_params_ubo = 0;
	}
}

void ParticlesStorage::particles_set_amount(RID p_particles, int p_amount) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (particles->amount == p_amount) {
		return;
	}

	_particles_free_data(particles);

	particles->amount = p_amount;

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_amount_ratio(RID p_particles, float p_amount_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->amount_ratio = p_amount_ratio;
}

void ParticlesStorage::particles_set_lifetime(RID p_particles, double p_lifetime) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->lifetime = p_lifetime;
}

void ParticlesStorage::particles_set_one_shot(RID p_particles, bool p_one_shot) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->one_shot = p_one_shot;
}

void ParticlesStorage::particles_set_pre_process_time(RID p_particles, double p_time) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->pre_process_time = p_time;
}
void ParticlesStorage::particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->explosiveness = p_ratio;
}
void ParticlesStorage::particles_set_randomness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->randomness = p_ratio;
}

void ParticlesStorage::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->custom_aabb = p_aabb;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_set_speed_scale(RID p_particles, double p_scale) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->speed_scale = p_scale;
}
void ParticlesStorage::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->use_local_coords = p_enable;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_fixed_fps(RID p_particles, int p_fps) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->fixed_fps = p_fps;

	_particles_free_data(particles);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_interpolate(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->interpolate = p_enable;
}

void ParticlesStorage::particles_set_fractional_delta(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->fractional_delta = p_enable;
}

void ParticlesStorage::particles_set_trails(RID p_particles, bool p_enable, double p_length) {
	if (p_enable) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle trails.");
	}
}

void ParticlesStorage::particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) {
	if (p_bind_poses.size() != 0) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle trails.");
	}
}

void ParticlesStorage::particles_set_collision_base_size(RID p_particles, real_t p_size) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->collision_base_size = p_size;
}

void ParticlesStorage::particles_set_transform_align(RID p_particles, RS::ParticlesTransformAlign p_transform_align) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->transform_align = p_transform_align;
}

void ParticlesStorage::particles_set_process_material(RID p_particles, RID p_material) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->process_material = p_material;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES); //the instance buffer may have changed
}

RID ParticlesStorage::particles_get_process_material(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());

	return particles->process_material;
}

void ParticlesStorage::particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->draw_order = p_order;
}

void ParticlesStorage::particles_set_draw_passes(RID p_particles, int p_passes) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->draw_passes.resize(p_passes);
}

void ParticlesStorage::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_INDEX(p_pass, particles->draw_passes.size());
	particles->draw_passes.write[p_pass] = p_mesh;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_restart(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->restart_request = true;
}

void ParticlesStorage::particles_set_subemitter(RID p_particles, RID p_subemitter_particles) {
	if (p_subemitter_particles.is_valid()) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle sub-emitters.");
	}
}

void ParticlesStorage::particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	WARN_PRINT_ONCE_ED("The Compatibility renderer does not support manually emitting particles.");
}

void ParticlesStorage::particles_request_process(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (!particles->dirty) {
		particles->dirty = true;

		if (!particles->update_list.in_list()) {
			particle_update_list.add(&particles->update_list);
		}
	}
}

AABB ParticlesStorage::particles_get_current_aabb(RID p_particles) {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());

	int total_amount = particles->amount;

	// If available, read from the sort buffer which should be 2 frames out of date.
	// This will help alleviate GPU stalls.
	GLuint read_buffer = particles->sort_buffer_filled ? particles->sort_buffer : particles->back_instance_buffer;

	Vector<uint8_t> buffer = Utilities::buffer_get_data(GL_ARRAY_BUFFER, read_buffer, total_amount * sizeof(ParticleInstanceData3D));
	ERR_FAIL_COND_V(buffer.size() != (int)(total_amount * sizeof(ParticleInstanceData3D)), AABB());

	Transform3D inv = particles->emission_transform.affine_inverse();

	AABB aabb;
	if (buffer.size()) {
		bool first = true;

		const uint8_t *data_ptr = (const uint8_t *)buffer.ptr();
		uint32_t particle_data_size = sizeof(ParticleInstanceData3D);

		for (int i = 0; i < total_amount; i++) {
			const ParticleInstanceData3D &particle_data = *(const ParticleInstanceData3D *)&data_ptr[particle_data_size * i];
			// If scale is 0.0, we assume the particle is inactive.
			if (particle_data.xform[0] > 0.0) {
				Vector3 pos = Vector3(particle_data.xform[3], particle_data.xform[7], particle_data.xform[11]);
				if (!particles->use_local_coords) {
					pos = inv.xform(pos);
				}
				if (first) {
					aabb.position = pos;
					first = false;
				} else {
					aabb.expand_to(pos);
				}
			}
		}
	}

	float longest_axis_size = 0;
	for (int i = 0; i < particles->draw_passes.size(); i++) {
		if (particles->draw_passes[i].is_valid()) {
			AABB maabb = MeshStorage::get_singleton()->mesh_get_aabb(particles->draw_passes[i], RID());
			longest_axis_size = MAX(maabb.get_longest_axis_size(), longest_axis_size);
		}
	}

	aabb.grow_by(longest_axis_size);

	return aabb;
}

AABB ParticlesStorage::particles_get_aabb(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());

	return particles->custom_aabb;
}

void ParticlesStorage::particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emission_transform = p_transform;
}

void ParticlesStorage::particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emitter_velocity = p_velocity;
}

void ParticlesStorage::particles_set_interp_to_end(RID p_particles, float p_interp) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->interp_to_end = p_interp;
}

int ParticlesStorage::particles_get_draw_passes(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, 0);

	return particles->draw_passes.size();
}

RID ParticlesStorage::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());
	ERR_FAIL_INDEX_V(p_pass, particles->draw_passes.size(), RID());

	return particles->draw_passes[p_pass];
}

void ParticlesStorage::particles_add_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->collisions.insert(p_particles_collision_instance);
}

void ParticlesStorage::particles_remove_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->collisions.erase(p_particles_collision_instance);
}

void ParticlesStorage::particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, GLuint p_texture) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->has_sdf_collision = p_enable;
	particles->sdf_collision_transform = p_xform;
	particles->sdf_collision_to_screen = p_to_screen;
	particles->sdf_collision_texture = p_texture;
}

// Does one step of processing particles by reading from back_process_buffer and writing to front_process_buffer.
void ParticlesStorage::_particles_process(Particles *p_particles, double p_delta) {
	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();

	double new_phase = Math::fmod(p_particles->phase + (p_delta / p_particles->lifetime) * p_particles->speed_scale, 1.0);

	//update current frame
	ParticlesFrameParams frame_params;

	if (p_particles->clear) {
		p_particles->cycle_number = 0;
		p_particles->random_seed = Math::rand();
	} else if (new_phase < p_particles->phase) {
		if (p_particles->one_shot) {
			p_particles->emitting = false;
		}
		p_particles->cycle_number++;
	}

	frame_params.emitting = p_particles->emitting;
	frame_params.system_phase = new_phase;
	frame_params.prev_system_phase = p_particles->phase;

	p_particles->phase = new_phase;

	frame_params.time = RSG::rasterizer->get_total_time();
	frame_params.delta = p_delta * p_particles->speed_scale;
	frame_params.random_seed = p_particles->random_seed;
	frame_params.explosiveness = p_particles->explosiveness;
	frame_params.randomness = p_particles->randomness;

	if (p_particles->use_local_coords) {
		GLES3::MaterialStorage::store_transform(Transform3D(), frame_params.emission_transform);
	} else {
		GLES3::MaterialStorage::store_transform(p_particles->emission_transform, frame_params.emission_transform);
	}

	frame_params.cycle = p_particles->cycle_number;
	frame_params.frame = p_particles->frame_counter++;
	frame_params.amount_ratio = p_particles->amount_ratio;
	frame_params.pad1 = 0;
	frame_params.pad2 = 0;
	frame_params.interp_to_end = p_particles->interp_to_end;
	frame_params.emitter_velocity[0] = p_particles->emitter_velocity.x;
	frame_params.emitter_velocity[1] = p_particles->emitter_velocity.y;
	frame_params.emitter_velocity[2] = p_particles->emitter_velocity.z;

	{ //collision and attractors

		frame_params.collider_count = 0;
		frame_params.attractor_count = 0;
		frame_params.particle_size = p_particles->collision_base_size;

		GLuint collision_heightmap_texture = 0;

		Transform3D to_particles;
		if (p_particles->use_local_coords) {
			to_particles = p_particles->emission_transform.affine_inverse();
		}

		if (p_particles->has_sdf_collision && p_particles->sdf_collision_texture != 0) {
			//2D collision

			Transform2D xform = p_particles->sdf_collision_transform; //will use dotproduct manually so invert beforehand

			if (!p_particles->use_local_coords) {
				Transform2D emission;
				emission.columns[0] = Vector2(p_particles->emission_transform.basis.get_column(0).x, p_particles->emission_transform.basis.get_column(0).y);
				emission.columns[1] = Vector2(p_particles->emission_transform.basis.get_column(1).x, p_particles->emission_transform.basis.get_column(1).y);
				emission.set_origin(Vector2(p_particles->emission_transform.origin.x, p_particles->emission_transform.origin.y));
				xform = xform * emission.affine_inverse();
			}

			Transform2D revert = xform.affine_inverse();
			frame_params.collider_count = 1;
			frame_params.colliders[0].transform[0] = xform.columns[0][0];
			frame_params.colliders[0].transform[1] = xform.columns[0][1];
			frame_params.colliders[0].transform[2] = 0;
			frame_params.colliders[0].transform[3] = xform.columns[2][0];

			frame_params.colliders[0].transform[4] = xform.columns[1][0];
			frame_params.colliders[0].transform[5] = xform.columns[1][1];
			frame_params.colliders[0].transform[6] = 0;
			frame_params.colliders[0].transform[7] = xform.columns[2][1];

			frame_params.colliders[0].transform[8] = revert.columns[0][0];
			frame_params.colliders[0].transform[9] = revert.columns[0][1];
			frame_params.colliders[0].transform[10] = 0;
			frame_params.colliders[0].transform[11] = revert.columns[2][0];

			frame_params.colliders[0].transform[12] = revert.columns[1][0];
			frame_params.colliders[0].transform[13] = revert.columns[1][1];
			frame_params.colliders[0].transform[14] = 0;
			frame_params.colliders[0].transform[15] = revert.columns[2][1];

			frame_params.colliders[0].extents[0] = p_particles->sdf_collision_to_screen.size.x;
			frame_params.colliders[0].extents[1] = p_particles->sdf_collision_to_screen.size.y;
			frame_params.colliders[0].extents[2] = p_particles->sdf_collision_to_screen.position.x;
			frame_params.colliders[0].scale = p_particles->sdf_collision_to_screen.position.y;
			frame_params.colliders[0].type = ParticlesFrameParams::COLLISION_TYPE_2D_SDF;

			collision_heightmap_texture = p_particles->sdf_collision_texture;
		}

		for (const RID &E : p_particles->collisions) {
			ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(E);
			if (!pci || !pci->active) {
				continue;
			}
			ParticlesCollision *pc = particles_collision_owner.get_or_null(pci->collision);
			ERR_CONTINUE(!pc);

			Transform3D to_collider = pci->transform;
			if (p_particles->use_local_coords) {
				to_collider = to_particles * to_collider;
			}
			Vector3 scale = to_collider.basis.get_scale();
			to_collider.basis.orthonormalize();

			if (pc->type <= RS::PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT) {
				//attractor
				if (frame_params.attractor_count >= ParticlesFrameParams::MAX_ATTRACTORS) {
					continue;
				}

				ParticlesFrameParams::Attractor &attr = frame_params.attractors[frame_params.attractor_count];

				GLES3::MaterialStorage::store_transform(to_collider, attr.transform);
				attr.strength = pc->attractor_strength;
				attr.attenuation = pc->attractor_attenuation;
				attr.directionality = pc->attractor_directionality;

				switch (pc->type) {
					case RS::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT: {
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_SPHERE;
						float radius = pc->radius;
						radius *= (scale.x + scale.y + scale.z) / 3.0;
						attr.extents[0] = radius;
						attr.extents[1] = radius;
						attr.extents[2] = radius;
					} break;
					case RS::PARTICLES_COLLISION_TYPE_BOX_ATTRACT: {
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_BOX;
						Vector3 extents = pc->extents * scale;
						attr.extents[0] = extents.x;
						attr.extents[1] = extents.y;
						attr.extents[2] = extents.z;
					} break;
					case RS::PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT: {
						WARN_PRINT_ONCE_ED("Vector field particle attractors are not available in the Compatibility renderer.");
					} break;
					default: {
					}
				}

				frame_params.attractor_count++;
			} else {
				//collider
				if (frame_params.collider_count >= ParticlesFrameParams::MAX_COLLIDERS) {
					continue;
				}

				ParticlesFrameParams::Collider &col = frame_params.colliders[frame_params.collider_count];

				GLES3::MaterialStorage::store_transform(to_collider, col.transform);
				switch (pc->type) {
					case RS::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE: {
						col.type = ParticlesFrameParams::COLLISION_TYPE_SPHERE;
						float radius = pc->radius;
						radius *= (scale.x + scale.y + scale.z) / 3.0;
						col.extents[0] = radius;
						col.extents[1] = radius;
						col.extents[2] = radius;
					} break;
					case RS::PARTICLES_COLLISION_TYPE_BOX_COLLIDE: {
						col.type = ParticlesFrameParams::COLLISION_TYPE_BOX;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
					} break;
					case RS::PARTICLES_COLLISION_TYPE_SDF_COLLIDE: {
						WARN_PRINT_ONCE_ED("SDF Particle Colliders are not available in the Compatibility renderer.");
					} break;
					case RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE: {
						if (collision_heightmap_texture != 0) { //already taken
							continue;
						}

						col.type = ParticlesFrameParams::COLLISION_TYPE_HEIGHT_FIELD;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
						collision_heightmap_texture = pc->heightfield_texture;
					} break;
					default: {
					}
				}

				frame_params.collider_count++;
			}
		}

		// Bind heightmap or SDF texture.
		GLuint heightmap = collision_heightmap_texture;
		if (heightmap == 0) {
			GLES3::Texture *tex = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES3::DEFAULT_GL_TEXTURE_BLACK));
			heightmap = tex->tex_id;
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, heightmap);
	}

	if (p_particles->frame_params_ubo == 0) {
		glGenBuffers(1, &p_particles->frame_params_ubo);
		glBindBufferBase(GL_UNIFORM_BUFFER, PARTICLES_FRAME_UNIFORM_LOCATION, p_particles->frame_params_ubo);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_UNIFORM_BUFFER, p_particles->frame_params_ubo, sizeof(ParticlesFrameParams), &frame_params, GL_STREAM_DRAW, "Particle Frame UBO");
	} else {
		// Update per-frame UBO.
		glBindBufferBase(GL_UNIFORM_BUFFER, PARTICLES_FRAME_UNIFORM_LOCATION, p_particles->frame_params_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ParticlesFrameParams), &frame_params, GL_STREAM_DRAW);
	}

	// Get shader and set shader uniforms;
	ParticleProcessMaterialData *m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(p_particles->process_material, RS::SHADER_PARTICLES));
	if (!m) {
		m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(particles_shader.default_material, RS::SHADER_PARTICLES));
	}

	ERR_FAIL_NULL(m);

	ParticlesShaderGLES3::ShaderVariant variant = ParticlesShaderGLES3::MODE_DEFAULT;

	uint32_t specialization = 0;
	for (uint32_t i = 0; i < PARTICLES_MAX_USERDATAS; i++) {
		if (m->shader_data->userdatas_used[i]) {
			specialization |= ParticlesShaderGLES3::USERDATA1_USED << i;
		}
	}

	if (p_particles->mode == RS::ParticlesMode::PARTICLES_MODE_3D) {
		specialization |= ParticlesShaderGLES3::MODE_3D;
	}

	RID version = particles_shader.default_shader_version;
	if (m->shader_data->version.is_valid() && m->shader_data->valid) {
		// Bind material uniform buffer and textures.
		m->bind_uniforms();
		version = m->shader_data->version;
	}

	bool success = material_storage->shaders.particles_process_shader.version_bind_shader(version, variant, specialization);
	if (!success) {
		return;
	}

	material_storage->shaders.particles_process_shader.version_set_uniform(ParticlesShaderGLES3::LIFETIME, p_particles->lifetime, version, variant, specialization);
	material_storage->shaders.particles_process_shader.version_set_uniform(ParticlesShaderGLES3::CLEAR, p_particles->clear, version, variant, specialization);
	material_storage->shaders.particles_process_shader.version_set_uniform(ParticlesShaderGLES3::TOTAL_PARTICLES, uint32_t(p_particles->amount), version, variant, specialization);
	material_storage->shaders.particles_process_shader.version_set_uniform(ParticlesShaderGLES3::USE_FRACTIONAL_DELTA, p_particles->fractional_delta, version, variant, specialization);

	p_particles->clear = false;

	p_particles->has_collision_cache = m->shader_data->uses_collision;

	glBindVertexArray(p_particles->back_vertex_array);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, p_particles->front_process_buffer);

	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, p_particles->amount);
	glEndTransformFeedback();

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
	glBindVertexArray(0);

	SWAP(p_particles->front_process_buffer, p_particles->back_process_buffer);
	SWAP(p_particles->front_vertex_array, p_particles->back_vertex_array);
}

void ParticlesStorage::particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (particles->draw_order != RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY) {
		return;
	}

	if (particles->front_process_buffer == 0) {
		return; //particles have not processed yet
	}

	Vector3 axis = -p_axis; // cameras look to z negative

	if (particles->use_local_coords) {
		axis = particles->emission_transform.basis.xform_inv(axis).normalized();
	}

	// Sort will be done on CPU since we don't have compute shaders.
	// If the sort_buffer has valid data
	// Use a buffer that is 2 frames out of date to avoid stalls.
	if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->sort_buffer_filled) {
		glBindBuffer(GL_ARRAY_BUFFER, particles->sort_buffer);

		ParticleInstanceData3D *particle_array;
#ifndef __EMSCRIPTEN__
		particle_array = static_cast<ParticleInstanceData3D *>(glMapBufferRange(GL_ARRAY_BUFFER, 0, particles->amount * sizeof(ParticleInstanceData3D), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
		ERR_FAIL_NULL(particle_array);
#else
		LocalVector<ParticleInstanceData3D> particle_vector;
		particle_vector.resize(particles->amount);
		particle_array = particle_vector.ptr();
		godot_webgl2_glGetBufferSubData(GL_ARRAY_BUFFER, 0, particles->amount * sizeof(ParticleInstanceData3D), particle_array);
#endif
		SortArray<ParticleInstanceData3D, ParticlesViewSort> sorter;
		sorter.compare.z_dir = axis;
		sorter.sort(particle_array, particles->amount);

#ifndef __EMSCRIPTEN__
		glUnmapBuffer(GL_ARRAY_BUFFER);
#else
		glBufferSubData(GL_ARRAY_BUFFER, 0, particles->amount * sizeof(ParticleInstanceData3D), particle_vector.ptr());
#endif
	}

	glEnable(GL_RASTERIZER_DISCARD);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	_particles_update_instance_buffer(particles, axis, p_up_axis);
	glDisable(GL_RASTERIZER_DISCARD);
}

void ParticlesStorage::_particles_update_buffers(Particles *particles) {
	GLES3::MaterialStorage *material_storage = GLES3::MaterialStorage::get_singleton();
	uint32_t userdata_count = 0;

	if (particles->process_material.is_valid()) {
		GLES3::ParticleProcessMaterialData *material_data = static_cast<GLES3::ParticleProcessMaterialData *>(material_storage->material_get_data(particles->process_material, RS::SHADER_PARTICLES));
		if (material_data && material_data->shader_data->version.is_valid() && material_data->shader_data->valid) {
			userdata_count = material_data->shader_data->userdata_count;
		}
	}

	if (userdata_count != particles->userdata_count) {
		// Mismatch userdata, re-create buffers.
		_particles_free_data(particles);
	}

	if (particles->amount > 0 && particles->front_process_buffer == 0) {
		int total_amount = particles->amount;

		particles->userdata_count = userdata_count;

		uint32_t xform_size = particles->mode == RS::PARTICLES_MODE_2D ? 2 : 3;
		particles->instance_buffer_stride_cache = sizeof(float) * 4 * (xform_size + 1);
		particles->instance_buffer_size_cache = particles->instance_buffer_stride_cache * total_amount;
		particles->num_attrib_arrays_cache = 5 + userdata_count + (xform_size - 2);
		particles->process_buffer_stride_cache = sizeof(float) * 4 * particles->num_attrib_arrays_cache;

		PackedByteArray data;
		data.resize_zeroed(particles->process_buffer_stride_cache * total_amount);

		PackedByteArray instance_data;
		instance_data.resize_zeroed(particles->instance_buffer_size_cache);

		{
			glGenVertexArrays(1, &particles->front_vertex_array);
			glBindVertexArray(particles->front_vertex_array);
			glGenBuffers(1, &particles->front_process_buffer);
			glGenBuffers(1, &particles->front_instance_buffer);

			glBindBuffer(GL_ARRAY_BUFFER, particles->front_process_buffer);
			GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->front_process_buffer, particles->process_buffer_stride_cache * total_amount, data.ptr(), GL_DYNAMIC_COPY, "Particles front process buffer");

			for (uint32_t j = 0; j < particles->num_attrib_arrays_cache; j++) {
				glEnableVertexAttribArray(j);
				glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, particles->process_buffer_stride_cache, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * j));
			}
			glBindVertexArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, particles->front_instance_buffer);
			GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->front_instance_buffer, particles->instance_buffer_size_cache, instance_data.ptr(), GL_DYNAMIC_COPY, "Particles front instance buffer");
		}

		{
			glGenVertexArrays(1, &particles->back_vertex_array);
			glBindVertexArray(particles->back_vertex_array);
			glGenBuffers(1, &particles->back_process_buffer);
			glGenBuffers(1, &particles->back_instance_buffer);

			glBindBuffer(GL_ARRAY_BUFFER, particles->back_process_buffer);
			GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->back_process_buffer, particles->process_buffer_stride_cache * total_amount, data.ptr(), GL_DYNAMIC_COPY, "Particles back process buffer");

			for (uint32_t j = 0; j < particles->num_attrib_arrays_cache; j++) {
				glEnableVertexAttribArray(j);
				glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, particles->process_buffer_stride_cache, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * j));
			}
			glBindVertexArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, particles->back_instance_buffer);
			GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->back_instance_buffer, particles->instance_buffer_size_cache, instance_data.ptr(), GL_DYNAMIC_COPY, "Particles back instance buffer");
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void ParticlesStorage::_particles_allocate_history_buffers(Particles *particles) {
	if (particles->sort_buffer == 0) {
		glGenBuffers(1, &particles->last_frame_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, particles->last_frame_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->last_frame_buffer, particles->instance_buffer_size_cache, nullptr, GL_DYNAMIC_READ, "Particles last frame buffer");

		glGenBuffers(1, &particles->sort_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, particles->sort_buffer);
		GLES3::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, particles->sort_buffer, particles->instance_buffer_size_cache, nullptr, GL_DYNAMIC_READ, "Particles sort buffer");

		particles->sort_buffer_filled = false;
		particles->last_frame_buffer_filled = false;
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}
void ParticlesStorage::_particles_update_instance_buffer(Particles *particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	ParticlesCopyShaderGLES3::ShaderVariant variant = ParticlesCopyShaderGLES3::MODE_DEFAULT;

	uint64_t specialization = 0;
	if (particles->mode == RS::ParticlesMode::PARTICLES_MODE_3D) {
		specialization |= ParticlesCopyShaderGLES3::MODE_3D;
	}

	bool success = particles_shader.copy_shader.version_bind_shader(particles_shader.copy_shader_version, variant, specialization);
	if (!success) {
		return;
	}

	// Affect 2D only.
	if (particles->use_local_coords) {
		// In local mode, particle positions are calculated locally (relative to the node position)
		// and they're also drawn locally.
		// It works as expected, so we just pass an identity transform.
		particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::INV_EMISSION_TRANSFORM, Transform3D(), particles_shader.copy_shader_version, variant, specialization);
	} else {
		// In global mode, particle positions are calculated globally (relative to the canvas origin)
		// but they're drawn locally.
		// So, we need to pass the inverse of the emission transform to bring the
		// particles to local coordinates before drawing.
		Transform3D inv = particles->emission_transform.affine_inverse();
		particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::INV_EMISSION_TRANSFORM, inv, particles_shader.copy_shader_version, variant, specialization);
	}

	particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::FRAME_REMAINDER, particles->interpolate ? particles->frame_remainder : 0.0, particles_shader.copy_shader_version, variant, specialization);
	particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::ALIGN_MODE, uint32_t(particles->transform_align), particles_shader.copy_shader_version, variant, specialization);
	particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::ALIGN_UP, p_up_axis, particles_shader.copy_shader_version, variant, specialization);
	particles_shader.copy_shader.version_set_uniform(ParticlesCopyShaderGLES3::SORT_DIRECTION, p_axis, particles_shader.copy_shader_version, variant, specialization);

	glBindVertexArray(particles->back_vertex_array);
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, particles->front_instance_buffer, 0, particles->instance_buffer_size_cache);
	glBeginTransformFeedback(GL_POINTS);

	if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_LIFETIME) {
		uint32_t lifetime_split = (MIN(int(particles->amount * particles->phase), particles->amount - 1) + 1) % particles->amount;
		uint32_t stride = particles->process_buffer_stride_cache;

		glBindBuffer(GL_ARRAY_BUFFER, particles->back_process_buffer);

		// Offset VBO so you render starting at the newest particle.
		if (particles->amount - lifetime_split > 0) {
			glEnableVertexAttribArray(0); // Color.
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 0));
			glEnableVertexAttribArray(1); // .xyz: velocity. .z: flags.
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 1));
			glEnableVertexAttribArray(2); // Custom.
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 2));
			glEnableVertexAttribArray(3); // Xform1.
			glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 3));
			glEnableVertexAttribArray(4); // Xform2.
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 4));
			if (particles->mode == RS::PARTICLES_MODE_3D) {
				glEnableVertexAttribArray(5); // Xform3.
				glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(stride * lifetime_split + sizeof(float) * 4 * 5));
			}

			uint32_t to_draw = particles->amount - lifetime_split;
			glDrawArrays(GL_POINTS, 0, to_draw);
		}

		// Then render from index 0 up intil the newest particle.
		if (lifetime_split > 0) {
			glEndTransformFeedback();
			// Now output to the second portion of the instance buffer.
			glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, particles->front_instance_buffer, particles->instance_buffer_stride_cache * (particles->amount - lifetime_split), particles->instance_buffer_stride_cache * (lifetime_split));
			glBeginTransformFeedback(GL_POINTS);
			// Reset back to normal.
			for (uint32_t j = 0; j < particles->num_attrib_arrays_cache; j++) {
				glEnableVertexAttribArray(j);
				glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4 * j));
			}

			glDrawArrays(GL_POINTS, 0, lifetime_split);
		}
	} else {
		glDrawArrays(GL_POINTS, 0, particles->amount);
	}

	glEndTransformFeedback();
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0, 0, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticlesStorage::update_particles() {
	if (!particle_update_list.first()) {
		// Return early to avoid unnecessary state changes.
		return;
	}

	RENDER_TIMESTAMP("Update GPUParticles");
	glEnable(GL_RASTERIZER_DISCARD);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);

	GLuint global_buffer = GLES3::MaterialStorage::get_singleton()->global_shader_parameters_get_uniform_buffer();

	glBindBufferBase(GL_UNIFORM_BUFFER, PARTICLES_GLOBALS_UNIFORM_LOCATION, global_buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	while (particle_update_list.first()) {
		// Use transform feedback to process particles.

		Particles *particles = particle_update_list.first()->self();

		particles->update_list.remove_from_list();
		particles->dirty = false;

		_particles_update_buffers(particles);

		if (particles->restart_request) {
			particles->prev_ticks = 0;
			particles->phase = 0;
			particles->prev_phase = 0;
			particles->clear = true;
			particles->restart_request = false;
		}

		if (particles->inactive && !particles->emitting) {
			//go next
			continue;
		}

		if (particles->emitting) {
			if (particles->inactive) {
				//restart system from scratch
				particles->prev_ticks = 0;
				particles->phase = 0;
				particles->prev_phase = 0;
				particles->clear = true;
			}
			particles->inactive = false;
			particles->inactive_time = 0;
		} else {
			particles->inactive_time += particles->speed_scale * RSG::rasterizer->get_frame_delta_time();
			if (particles->inactive_time > particles->lifetime * 1.2) {
				particles->inactive = true;
				continue;
			}
		}

		// Copy the instance buffer that was last used into the last_frame buffer.
		// sort_buffer should now be 2 frames out of date.
		if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH || particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME) {
			_particles_allocate_history_buffers(particles);
			SWAP(particles->last_frame_buffer, particles->sort_buffer);

			glBindBuffer(GL_COPY_READ_BUFFER, particles->back_instance_buffer);
			glBindBuffer(GL_COPY_WRITE_BUFFER, particles->last_frame_buffer);
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, particles->instance_buffer_size_cache);

			// Last frame's last_frame turned into this frame's sort buffer.
			particles->sort_buffer_filled = particles->last_frame_buffer_filled;
			particles->sort_buffer_phase = particles->last_frame_phase;
			particles->last_frame_buffer_filled = true;
			particles->last_frame_phase = particles->phase;
			glBindBuffer(GL_COPY_READ_BUFFER, 0);
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
		}

		int fixed_fps = 0;
		if (particles->fixed_fps > 0) {
			fixed_fps = particles->fixed_fps;
		}

		bool zero_time_scale = Engine::get_singleton()->get_time_scale() <= 0.0;

		if (particles->clear && particles->pre_process_time > 0.0) {
			double frame_time;
			if (fixed_fps > 0) {
				frame_time = 1.0 / fixed_fps;
			} else {
				frame_time = 1.0 / 30.0;
			}

			double todo = particles->pre_process_time;

			while (todo >= 0) {
				_particles_process(particles, frame_time);
				todo -= frame_time;
			}
		}

		if (fixed_fps > 0) {
			double frame_time;
			double decr;
			if (zero_time_scale) {
				frame_time = 0.0;
				decr = 1.0 / fixed_fps;
			} else {
				frame_time = 1.0 / fixed_fps;
				decr = frame_time;
			}
			double delta = RSG::rasterizer->get_frame_delta_time();
			if (delta > 0.1) { //avoid recursive stalls if fps goes below 10
				delta = 0.1;
			} else if (delta <= 0.0) { //unlikely but..
				delta = 0.001;
			}
			double todo = particles->frame_remainder + delta;

			while (todo >= frame_time) {
				_particles_process(particles, frame_time);
				todo -= decr;
			}

			particles->frame_remainder = todo;

		} else {
			if (zero_time_scale) {
				_particles_process(particles, 0.0);
			} else {
				_particles_process(particles, RSG::rasterizer->get_frame_delta_time());
			}
		}

		// Copy particles to instance buffer and pack Color/Custom.
		// We don't have camera information here, so don't copy here if we need camera information for view depth or align mode.
		if (particles->draw_order != RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY) {
			_particles_update_instance_buffer(particles, Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0));

			if (particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME && particles->sort_buffer_filled) {
				if (particles->mode == RS::ParticlesMode::PARTICLES_MODE_2D) {
					_particles_reverse_lifetime_sort<ParticleInstanceData2D>(particles);
				} else {
					_particles_reverse_lifetime_sort<ParticleInstanceData3D>(particles);
				}
			}
		}

		SWAP(particles->front_instance_buffer, particles->back_instance_buffer);

		// At the end of update, the back_buffer contains the most up-to-date-information to read from.

		particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
	}

	glDisable(GL_RASTERIZER_DISCARD);
}

template <typename ParticleInstanceData>
void ParticlesStorage::_particles_reverse_lifetime_sort(Particles *particles) {
	glBindBuffer(GL_ARRAY_BUFFER, particles->sort_buffer);

	ParticleInstanceData *particle_array;
	uint32_t buffer_size = particles->amount * sizeof(ParticleInstanceData);
#ifndef __EMSCRIPTEN__
	particle_array = static_cast<ParticleInstanceData *>(glMapBufferRange(GL_ARRAY_BUFFER, 0, buffer_size, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));

	ERR_FAIL_NULL(particle_array);
#else
	LocalVector<ParticleInstanceData> particle_vector;
	particle_vector.resize(particles->amount);
	particle_array = particle_vector.ptr();
	godot_webgl2_glGetBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size, particle_array);
#endif

	uint32_t lifetime_split = (MIN(int(particles->amount * particles->sort_buffer_phase), particles->amount - 1) + 1) % particles->amount;
	for (uint32_t i = 0; i < lifetime_split / 2; i++) {
		SWAP(particle_array[i], particle_array[lifetime_split - i - 1]);
	}

	for (uint32_t i = 0; i < (particles->amount - lifetime_split) / 2; i++) {
		SWAP(particle_array[lifetime_split + i], particle_array[particles->amount - 1 - i]);
	}

#ifndef __EMSCRIPTEN__
	glUnmapBuffer(GL_ARRAY_BUFFER);
#else
	glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size, particle_vector.ptr());
#endif
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Dependency *ParticlesStorage::particles_get_dependency(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, nullptr);

	return &particles->dependency;
}

bool ParticlesStorage::particles_is_inactive(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);
	return !particles->emitting && particles->inactive;
}

/* PARTICLES COLLISION API */

RID ParticlesStorage::particles_collision_allocate() {
	return particles_collision_owner.allocate_rid();
}
void ParticlesStorage::particles_collision_initialize(RID p_rid) {
	particles_collision_owner.initialize_rid(p_rid, ParticlesCollision());
}

void ParticlesStorage::particles_collision_free(RID p_rid) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_rid);

	if (particles_collision->heightfield_texture != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(particles_collision->heightfield_texture);
		particles_collision->heightfield_texture = 0;
		glDeleteFramebuffers(1, &particles_collision->heightfield_fb);
		particles_collision->heightfield_fb = 0;
	}
	particles_collision->dependency.deleted_notify(p_rid);
	particles_collision_owner.free(p_rid);
}

GLuint ParticlesStorage::particles_collision_get_heightfield_framebuffer(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, 0);
	ERR_FAIL_COND_V(particles_collision->type != RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE, 0);

	if (particles_collision->heightfield_texture == 0) {
		//create
		const int resolutions[RS::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX] = { 256, 512, 1024, 2048, 4096, 8192 };
		Size2i size;
		if (particles_collision->extents.x > particles_collision->extents.z) {
			size.x = resolutions[particles_collision->heightfield_resolution];
			size.y = int32_t(particles_collision->extents.z / particles_collision->extents.x * size.x);
		} else {
			size.y = resolutions[particles_collision->heightfield_resolution];
			size.x = int32_t(particles_collision->extents.x / particles_collision->extents.z * size.y);
		}

		glGenTextures(1, &particles_collision->heightfield_texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, particles_collision->heightfield_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &particles_collision->heightfield_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, particles_collision->heightfield_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, particles_collision->heightfield_texture, 0);
#ifdef DEBUG_ENABLED
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			WARN_PRINT("Could create heightmap texture status: " + GLES3::TextureStorage::get_singleton()->get_framebuffer_error(status));
		}
#endif
		GLES3::Utilities::get_singleton()->texture_allocated_data(particles_collision->heightfield_texture, size.x * size.y * 4, "Particles collision heightfield texture");

		particles_collision->heightfield_fb_size = size;

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
	}

	return particles_collision->heightfield_fb;
}

void ParticlesStorage::particles_collision_set_collision_type(RID p_particles_collision, RS::ParticlesCollisionType p_type) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	if (p_type == particles_collision->type) {
		return;
	}

	if (particles_collision->heightfield_texture != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(particles_collision->heightfield_texture);
		particles_collision->heightfield_texture = 0;
		glDeleteFramebuffers(1, &particles_collision->heightfield_fb);
		particles_collision->heightfield_fb = 0;
	}

	particles_collision->type = p_type;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->cull_mask = p_cull_mask;
}

void ParticlesStorage::particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->radius = p_radius;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->extents = p_extents;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_strength = p_strength;
}

void ParticlesStorage::particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_directionality = p_directionality;
}

void ParticlesStorage::particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_attenuation = p_curve;
}

void ParticlesStorage::particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) {
	WARN_PRINT_ONCE_ED("The Compatibility renderer does not support SDF collisions in 3D particle shaders");
}

void ParticlesStorage::particles_collision_height_field_update(RID p_particles_collision) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_height_field_resolution(RID p_particles_collision, RS::ParticlesCollisionHeightfieldResolution p_resolution) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_INDEX(p_resolution, RS::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX);

	if (particles_collision->heightfield_resolution == p_resolution) {
		return;
	}

	particles_collision->heightfield_resolution = p_resolution;

	if (particles_collision->heightfield_texture != 0) {
		GLES3::Utilities::get_singleton()->texture_free_data(particles_collision->heightfield_texture);
		particles_collision->heightfield_texture = 0;
		glDeleteFramebuffers(1, &particles_collision->heightfield_fb);
		particles_collision->heightfield_fb = 0;
	}
}

AABB ParticlesStorage::particles_collision_get_aabb(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, AABB());

	switch (particles_collision->type) {
		case RS::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT:
		case RS::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE: {
			AABB aabb;
			aabb.position = -Vector3(1, 1, 1) * particles_collision->radius;
			aabb.size = Vector3(2, 2, 2) * particles_collision->radius;
			return aabb;
		}
		default: {
			AABB aabb;
			aabb.position = -particles_collision->extents;
			aabb.size = particles_collision->extents * 2;
			return aabb;
		}
	}
}

Vector3 ParticlesStorage::particles_collision_get_extents(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, Vector3());
	return particles_collision->extents;
}

bool ParticlesStorage::particles_collision_is_heightfield(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, false);
	return particles_collision->type == RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE;
}

Dependency *ParticlesStorage::particles_collision_get_dependency(RID p_particles_collision) const {
	ParticlesCollision *pc = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(pc, nullptr);

	return &pc->dependency;
}

/* Particles collision instance */

RID ParticlesStorage::particles_collision_instance_create(RID p_collision) {
	ParticlesCollisionInstance pci;
	pci.collision = p_collision;
	return particles_collision_instance_owner.make_rid(pci);
}

void ParticlesStorage::particles_collision_instance_free(RID p_rid) {
	particles_collision_instance_owner.free(p_rid);
}

void ParticlesStorage::particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	pci->transform = p_transform;
}

void ParticlesStorage::particles_collision_instance_set_active(RID p_collision_instance, bool p_active) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	pci->active = p_active;
}

#endif // GLES3_ENABLED
