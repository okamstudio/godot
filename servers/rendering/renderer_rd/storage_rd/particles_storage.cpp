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

#include "particles_storage.h"

#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_server_globals.h"
#include "texture_storage.h"

using namespace RendererRD;

ParticlesStorage *ParticlesStorage::singleton = nullptr;

ParticlesStorage *ParticlesStorage::get_singleton() {
	return singleton;
}

ParticlesStorage::ParticlesStorage() {
	singleton = this;

	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	/* Effects */

	sort_effects = memnew(SortEffects);

	/* Particles */

	{
		String defines = "#define SAMPLERS_BINDING_FIRST_INDEX " + itos(SAMPLERS_BINDING_FIRST_INDEX) + "\n";
		// Initialize particles
		Vector<String> particles_modes;
		particles_modes.push_back("");
		particles_shader.shader.initialize(particles_modes, defines);
	}
	MaterialStorage::get_singleton()->shader_set_data_request_function(MaterialStorage::SHADER_TYPE_PARTICLES, _create_particles_shader_funcs);
	MaterialStorage::get_singleton()->material_set_data_request_function(MaterialStorage::SHADER_TYPE_PARTICLES, _create_particles_material_funcs);

	{
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["COLOR"] = "PARTICLE.color";
		actions.renames["VELOCITY"] = "PARTICLE.velocity";
		//actions.renames["MASS"] = "mass"; ?
		actions.renames["ACTIVE"] = "particle_active";
		actions.renames["RESTART"] = "restart";
		actions.renames["CUSTOM"] = "PARTICLE.custom";
		actions.renames["AMOUNT_RATIO"] = "FRAME.amount_ratio";
		for (int i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
			String udname = "USERDATA" + itos(i + 1);
			actions.renames[udname] = "PARTICLE.userdata" + itos(i + 1);
			actions.usage_defines[udname] = "#define USERDATA" + itos(i + 1) + "_USED\n";
		}
		actions.renames["TRANSFORM"] = "PARTICLE.xform";
		actions.renames["TIME"] = "frame_history.data[0].time";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["LIFETIME"] = "params.lifetime";
		actions.renames["DELTA"] = "local_delta";
		actions.renames["NUMBER"] = "particle_number";
		actions.renames["INDEX"] = "index";
		//actions.renames["GRAVITY"] = "current_gravity";
		actions.renames["EMISSION_TRANSFORM"] = "FRAME.emission_transform";
		actions.renames["EMITTER_VELOCITY"] = "FRAME.emitter_velocity";
		actions.renames["INTERPOLATE_TO_END"] = "FRAME.interp_to_end";
		actions.renames["RANDOM_SEED"] = "FRAME.random_seed";
		actions.renames["FLAG_EMIT_POSITION"] = "EMISSION_FLAG_HAS_POSITION";
		actions.renames["FLAG_EMIT_ROT_SCALE"] = "EMISSION_FLAG_HAS_ROTATION_SCALE";
		actions.renames["FLAG_EMIT_VELOCITY"] = "EMISSION_FLAG_HAS_VELOCITY";
		actions.renames["FLAG_EMIT_COLOR"] = "EMISSION_FLAG_HAS_COLOR";
		actions.renames["FLAG_EMIT_CUSTOM"] = "EMISSION_FLAG_HAS_CUSTOM";
		actions.renames["RESTART_POSITION"] = "restart_position";
		actions.renames["RESTART_ROT_SCALE"] = "restart_rotation_scale";
		actions.renames["RESTART_VELOCITY"] = "restart_velocity";
		actions.renames["RESTART_COLOR"] = "restart_color";
		actions.renames["RESTART_CUSTOM"] = "restart_custom";
		actions.renames["emit_subparticle"] = "emit_subparticle";
		actions.renames["COLLIDED"] = "collided";
		actions.renames["COLLISION_NORMAL"] = "collision_normal";
		actions.renames["COLLISION_DEPTH"] = "collision_depth";
		actions.renames["ATTRACTOR_FORCE"] = "attractor_force";

		actions.render_mode_defines["disable_force"] = "#define DISABLE_FORCE\n";
		actions.render_mode_defines["disable_velocity"] = "#define DISABLE_VELOCITY\n";
		actions.render_mode_defines["keep_data"] = "#define ENABLE_KEEP_DATA\n";
		actions.render_mode_defines["collision_use_scale"] = "#define USE_COLLISION_SCALE\n";

		actions.base_texture_binding_index = 1;
		actions.texture_layout_set = 3;
		actions.base_uniform_string = "material.";
		actions.base_varying_index = 10;

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_ENABLE;
		actions.global_buffer_array_variable = "global_shader_uniforms.data";

		particles_shader.compiler.initialize(actions);
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

		ParticleProcessMaterialData *md = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(particles_shader.default_material, MaterialStorage::SHADER_TYPE_PARTICLES));
		particles_shader.default_shader_rd = particles_shader.shader.version_get_shader(md->shader_data->version, 0);

		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(material_storage->global_shader_uniforms_get_storage_buffer());
			uniforms.push_back(u);
		}

		material_storage->samplers_rd_get_default().append_uniforms(uniforms, SAMPLERS_BINDING_FIRST_INDEX);

		particles_shader.base_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, BASE_UNIFORM_SET);
	}

	{
		Vector<String> copy_modes;
		for (int i = 0; i <= ParticlesShader::MAX_USERDATAS; i++) {
			if (i == 0) {
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n");
				copy_modes.push_back("\n#define MODE_FILL_SORT_BUFFER\n#define USE_SORT_BUFFER\n");
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USE_SORT_BUFFER\n");
			} else {
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USERDATA_COUNT " + itos(i) + "\n");
				copy_modes.push_back("\n#define MODE_FILL_SORT_BUFFER\n#define USE_SORT_BUFFER\n#define USERDATA_COUNT " + itos(i) + "\n");
				copy_modes.push_back("\n#define MODE_FILL_INSTANCES\n#define USE_SORT_BUFFER\n#define USERDATA_COUNT " + itos(i) + "\n");
			}
		}

		particles_shader.copy_shader.initialize(copy_modes);

		particles_shader.copy_shader_version = particles_shader.copy_shader.version_create();

		for (int i = 0; i <= ParticlesShader::MAX_USERDATAS; i++) {
			for (int j = 0; j < ParticlesShader::COPY_MODE_MAX; j++) {
				particles_shader.copy_pipelines[i * ParticlesShader::COPY_MODE_MAX + j] = RD::get_singleton()->compute_pipeline_create(particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, i * ParticlesShader::COPY_MODE_MAX + j));
			}
		}
	}
}

ParticlesStorage::~ParticlesStorage() {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	particles_shader.copy_shader.version_free(particles_shader.copy_shader_version);

	material_storage->material_free(particles_shader.default_material);
	material_storage->shader_free(particles_shader.default_shader);

	if (sort_effects) {
		memdelete(sort_effects);
		sort_effects = nullptr;
	}

	singleton = nullptr;
}

bool ParticlesStorage::free(RID p_rid) {
	if (owns_particles(p_rid)) {
		particles_free(p_rid);
		return true;
	} else if (owns_particles_collision(p_rid)) {
		particles_collision_free(p_rid);
		return true;
	} else if (owns_particles_collision_instance(p_rid)) {
		particles_collision_instance_free(p_rid);
		return true;
	}

	return false;
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
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emitting = p_emitting;
}

bool ParticlesStorage::particles_get_emitting(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);

	return particles->emitting;
}

void ParticlesStorage::_particles_free_data(Particles *particles) {
	if (particles->particle_buffer.is_valid()) {
		RD::get_singleton()->free(particles->particle_buffer);
		particles->particle_buffer = RID();
		RD::get_singleton()->free(particles->particle_instance_buffer);
		particles->particle_instance_buffer = RID();
	}

	particles->userdata_count = 0;

	if (particles->frame_params_buffer.is_valid()) {
		RD::get_singleton()->free(particles->frame_params_buffer);
		particles->frame_params_buffer = RID();
	}
	particles->particles_transforms_buffer_uniform_set = RID();

	if (RD::get_singleton()->uniform_set_is_valid(particles->trail_bind_pose_uniform_set)) {
		RD::get_singleton()->free(particles->trail_bind_pose_uniform_set);
	}
	particles->trail_bind_pose_uniform_set = RID();

	if (particles->trail_bind_pose_buffer.is_valid()) {
		RD::get_singleton()->free(particles->trail_bind_pose_buffer);
		particles->trail_bind_pose_buffer = RID();
	}
	if (RD::get_singleton()->uniform_set_is_valid(particles->collision_textures_uniform_set)) {
		RD::get_singleton()->free(particles->collision_textures_uniform_set);
	}
	particles->collision_textures_uniform_set = RID();

	if (particles->particles_sort_buffer.is_valid()) {
		RD::get_singleton()->free(particles->particles_sort_buffer);
		particles->particles_sort_buffer = RID();
		particles->particles_sort_uniform_set = RID();
	}

	if (particles->emission_buffer != nullptr) {
		particles->emission_buffer = nullptr;
		particles->emission_buffer_data.clear();
		RD::get_singleton()->free(particles->emission_storage_buffer);
		particles->emission_storage_buffer = RID();
	}

	if (particles->unused_emission_storage_buffer.is_valid()) {
		RD::get_singleton()->free(particles->unused_emission_storage_buffer);
		particles->unused_emission_storage_buffer = RID();
	}

	if (particles->unused_trail_storage_buffer.is_valid()) {
		RD::get_singleton()->free(particles->unused_trail_storage_buffer);
		particles->unused_trail_storage_buffer = RID();
	}

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		//will need to be re-created
		RD::get_singleton()->free(particles->particles_material_uniform_set);
	}
	particles->particles_material_uniform_set = RID();
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
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND(p_length < 0.01);
	p_length = MIN(10.0, p_length);

	particles->trails_enabled = p_enable;
	particles->trail_lifetime = p_length;

	_particles_free_data(particles);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	if (particles->trail_bind_pose_buffer.is_valid() && particles->trail_bind_poses.size() != p_bind_poses.size()) {
		_particles_free_data(particles);

		particles->prev_ticks = 0;
		particles->phase = 0;
		particles->prev_phase = 0;
		particles->clear = true;
	}
	particles->trail_bind_poses = p_bind_poses;
	particles->trail_bind_poses_dirty = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
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
}

void ParticlesStorage::particles_restart(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->restart_request = true;
}

void ParticlesStorage::_particles_allocate_emission_buffer(Particles *particles) {
	ERR_FAIL_COND(particles->emission_buffer != nullptr);

	particles->emission_buffer_data.resize(sizeof(ParticleEmissionBuffer::Data) * particles->amount + sizeof(uint32_t) * 4);
	memset(particles->emission_buffer_data.ptrw(), 0, particles->emission_buffer_data.size());
	particles->emission_buffer = reinterpret_cast<ParticleEmissionBuffer *>(particles->emission_buffer_data.ptrw());
	particles->emission_buffer->particle_max = particles->amount;

	particles->emission_storage_buffer = RD::get_singleton()->storage_buffer_create(particles->emission_buffer_data.size(), particles->emission_buffer_data);

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		//will need to be re-created
		RD::get_singleton()->free(particles->particles_material_uniform_set);
		particles->particles_material_uniform_set = RID();
	}
}

void ParticlesStorage::_particles_ensure_unused_emission_buffer(Particles *particles) {
	if (particles->unused_emission_storage_buffer.is_null()) {
		// For rendering devices that do not support empty arrays (like C++),
		// we need to size the buffer with at least 1 element.
		particles->unused_emission_storage_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ParticleEmissionBuffer));
	}
}

void ParticlesStorage::_particles_ensure_unused_trail_buffer(Particles *particles) {
	if (particles->unused_trail_storage_buffer.is_null()) {
		particles->unused_trail_storage_buffer = RD::get_singleton()->storage_buffer_create(16 * sizeof(float)); // Size of mat4.
	}
}

void ParticlesStorage::particles_set_subemitter(RID p_particles, RID p_subemitter_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND(p_particles == p_subemitter_particles);

	particles->sub_emitter = p_subemitter_particles;

	if (RD::get_singleton()->uniform_set_is_valid(particles->particles_material_uniform_set)) {
		RD::get_singleton()->free(particles->particles_material_uniform_set);
		particles->particles_material_uniform_set = RID(); //clear and force to re create sub emitting
	}
}

void ParticlesStorage::particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_COND(particles->amount == 0);

	if (particles->emitting) {
		particles->clear = true;
		particles->emitting = false;
	}

	if (particles->emission_buffer == nullptr) {
		_particles_allocate_emission_buffer(particles);
	}

	particles->inactive = false;
	particles->inactive_time = 0;

	int32_t idx = particles->emission_buffer->particle_count;
	if (idx < particles->emission_buffer->particle_max) {
		RendererRD::MaterialStorage::store_transform(p_transform, particles->emission_buffer->data[idx].xform);

		particles->emission_buffer->data[idx].velocity[0] = p_velocity.x;
		particles->emission_buffer->data[idx].velocity[1] = p_velocity.y;
		particles->emission_buffer->data[idx].velocity[2] = p_velocity.z;

		particles->emission_buffer->data[idx].custom[0] = p_custom.r;
		particles->emission_buffer->data[idx].custom[1] = p_custom.g;
		particles->emission_buffer->data[idx].custom[2] = p_custom.b;
		particles->emission_buffer->data[idx].custom[3] = p_custom.a;

		particles->emission_buffer->data[idx].color[0] = p_color.r;
		particles->emission_buffer->data[idx].color[1] = p_color.g;
		particles->emission_buffer->data[idx].color[2] = p_color.b;
		particles->emission_buffer->data[idx].color[3] = p_color.a;

		particles->emission_buffer->data[idx].flags = p_emit_flags;
		particles->emission_buffer->particle_count++;
	}
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
	if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
		total_amount *= particles->trail_bind_poses.size();
	}

	uint32_t particle_data_size = sizeof(ParticleData) + sizeof(float) * 4 * particles->userdata_count;
	Vector<uint8_t> buffer = RD::get_singleton()->buffer_get_data(particles->particle_buffer);
	ERR_FAIL_COND_V(buffer.size() != (int)(total_amount * particle_data_size), AABB());

	Transform3D inv = particles->emission_transform.affine_inverse();

	AABB aabb;
	if (buffer.size()) {
		bool first = true;

		const uint8_t *data_ptr = (const uint8_t *)buffer.ptr();

		for (int i = 0; i < total_amount; i++) {
			const ParticleData &particle_data = *(const ParticleData *)&data_ptr[particle_data_size * i];
			if (particle_data.active) {
				Vector3 pos = Vector3(particle_data.xform[12], particle_data.xform[13], particle_data.xform[14]);
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

void ParticlesStorage::particles_update_dependency(RID p_particles, DependencyTracker *p_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	p_instance->update_dependency(&particles->dependency);
}

void ParticlesStorage::particles_get_instance_buffer_motion_vectors_offsets(RID p_particles, uint32_t &r_current_offset, uint32_t &r_prev_offset) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	r_current_offset = particles->instance_motion_vectors_current_offset;
	r_prev_offset = particles->instance_motion_vectors_previous_offset;
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

void ParticlesStorage::particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, RID p_texture) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->has_sdf_collision = p_enable;
	particles->sdf_collision_transform = p_xform;
	particles->sdf_collision_to_screen = p_to_screen;
	particles->sdf_collision_texture = p_texture;
}

void ParticlesStorage::_particles_process(Particles *p_particles, double p_delta) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	if (p_particles->particles_material_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(p_particles->particles_material_uniform_set)) {
		Vector<RD::Uniform> uniforms;

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(p_particles->frame_params_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(p_particles->particle_buffer);
			uniforms.push_back(u);
		}

		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			if (p_particles->emission_storage_buffer.is_valid()) {
				u.append_id(p_particles->emission_storage_buffer);
			} else {
				_particles_ensure_unused_emission_buffer(p_particles);
				u.append_id(p_particles->unused_emission_storage_buffer);
			}
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			Particles *sub_emitter = particles_owner.get_or_null(p_particles->sub_emitter);
			if (sub_emitter) {
				if (sub_emitter->emission_buffer == nullptr) { //no emission buffer, allocate emission buffer
					_particles_allocate_emission_buffer(sub_emitter);
				}
				u.append_id(sub_emitter->emission_storage_buffer);
			} else {
				_particles_ensure_unused_emission_buffer(p_particles);
				u.append_id(p_particles->unused_emission_storage_buffer);
			}
			uniforms.push_back(u);
		}

		p_particles->particles_material_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, 1);
	}

	double new_phase = Math::fmod((double)p_particles->phase + (p_delta / p_particles->lifetime) * p_particles->speed_scale, 1.0);

	//move back history (if there is any)
	for (uint32_t i = p_particles->frame_history.size() - 1; i > 0; i--) {
		p_particles->frame_history[i] = p_particles->frame_history[i - 1];
	}
	//update current frame
	ParticlesFrameParams &frame_params = p_particles->frame_history[0];

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

	frame_params.time = RendererCompositorRD::get_singleton()->get_total_time();
	frame_params.delta = p_delta * p_particles->speed_scale;
	frame_params.random_seed = p_particles->random_seed;
	frame_params.explosiveness = p_particles->explosiveness;
	frame_params.randomness = p_particles->randomness;

	if (p_particles->use_local_coords) {
		RendererRD::MaterialStorage::store_transform(Transform3D(), frame_params.emission_transform);
	} else {
		RendererRD::MaterialStorage::store_transform(p_particles->emission_transform, frame_params.emission_transform);
	}

	frame_params.cycle = p_particles->cycle_number;
	frame_params.frame = p_particles->frame_counter++;
	frame_params.amount_ratio = p_particles->amount_ratio;
	frame_params.pad1 = 0;
	frame_params.pad2 = 0;
	frame_params.emitter_velocity[0] = p_particles->emitter_velocity.x;
	frame_params.emitter_velocity[1] = p_particles->emitter_velocity.y;
	frame_params.emitter_velocity[2] = p_particles->emitter_velocity.z;
	frame_params.interp_to_end = p_particles->interp_to_end;

	{ //collision and attractors

		frame_params.collider_count = 0;
		frame_params.attractor_count = 0;
		frame_params.particle_size = p_particles->collision_base_size;

		RID collision_3d_textures[ParticlesFrameParams::MAX_3D_TEXTURES];
		RID collision_heightmap_texture;

		Transform3D to_particles;
		if (p_particles->use_local_coords) {
			to_particles = p_particles->emission_transform.affine_inverse();
		}

		if (p_particles->has_sdf_collision && RD::get_singleton()->texture_is_valid(p_particles->sdf_collision_texture)) {
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
			frame_params.colliders[0].texture_index = 0;
			frame_params.colliders[0].type = ParticlesFrameParams::COLLISION_TYPE_2D_SDF;

			collision_heightmap_texture = p_particles->sdf_collision_texture;

			//replace in all other history frames where used because parameters are no longer valid if screen moves
			for (ParticlesFrameParams &params : p_particles->frame_history) {
				if (params.collider_count > 0 && params.colliders[0].type == ParticlesFrameParams::COLLISION_TYPE_2D_SDF) {
					params.colliders[0] = frame_params.colliders[0];
				}
			}
		}

		uint32_t collision_3d_textures_used = 0;
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

				RendererRD::MaterialStorage::store_transform(to_collider, attr.transform);
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
						if (collision_3d_textures_used >= ParticlesFrameParams::MAX_3D_TEXTURES) {
							continue;
						}
						attr.type = ParticlesFrameParams::ATTRACTOR_TYPE_VECTOR_FIELD;
						Vector3 extents = pc->extents * scale;
						attr.extents[0] = extents.x;
						attr.extents[1] = extents.y;
						attr.extents[2] = extents.z;
						attr.texture_index = collision_3d_textures_used;

						collision_3d_textures[collision_3d_textures_used] = pc->field_texture;
						collision_3d_textures_used++;
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

				RendererRD::MaterialStorage::store_transform(to_collider, col.transform);
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
						if (collision_3d_textures_used >= ParticlesFrameParams::MAX_3D_TEXTURES) {
							continue;
						}
						col.type = ParticlesFrameParams::COLLISION_TYPE_SDF;
						Vector3 extents = pc->extents * scale;
						col.extents[0] = extents.x;
						col.extents[1] = extents.y;
						col.extents[2] = extents.z;
						col.texture_index = collision_3d_textures_used;
						col.scale = (scale.x + scale.y + scale.z) * 0.333333333333; //non uniform scale non supported

						collision_3d_textures[collision_3d_textures_used] = pc->field_texture;
						collision_3d_textures_used++;
					} break;
					case RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE: {
						if (collision_heightmap_texture != RID()) { //already taken
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

		bool different = false;
		if (collision_3d_textures_used == p_particles->collision_3d_textures_used) {
			for (int i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
				if (p_particles->collision_3d_textures[i] != collision_3d_textures[i]) {
					different = true;
					break;
				}
			}
		}

		if (collision_heightmap_texture != p_particles->collision_heightmap_texture) {
			different = true;
		}

		bool uniform_set_valid = RD::get_singleton()->uniform_set_is_valid(p_particles->collision_textures_uniform_set);

		if (different || !uniform_set_valid) {
			if (uniform_set_valid) {
				RD::get_singleton()->free(p_particles->collision_textures_uniform_set);
			}

			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 0;
				for (uint32_t i = 0; i < ParticlesFrameParams::MAX_3D_TEXTURES; i++) {
					RID rd_tex;
					if (i < collision_3d_textures_used) {
						if (TextureStorage::get_singleton()->texture_get_type(collision_3d_textures[i]) == TextureStorage::TYPE_3D) {
							rd_tex = TextureStorage::get_singleton()->texture_get_rd_texture(collision_3d_textures[i]);
						}
					}

					if (rd_tex == RID()) {
						rd_tex = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);
					}
					u.append_id(rd_tex);
				}
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
				u.binding = 1;
				if (collision_heightmap_texture.is_valid()) {
					u.append_id(collision_heightmap_texture);
				} else {
					u.append_id(texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_BLACK));
				}
				uniforms.push_back(u);
			}
			p_particles->collision_textures_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.default_shader_rd, 2);
			p_particles->collision_heightmap_texture = collision_heightmap_texture;
		}
	}

	ParticlesShader::PushConstant push_constant;

	int process_amount = p_particles->amount;

	if (p_particles->trails_enabled && p_particles->trail_bind_poses.size() > 1) {
		process_amount *= p_particles->trail_bind_poses.size();
	}
	push_constant.clear = p_particles->clear;
	push_constant.total_particles = p_particles->amount;
	push_constant.lifetime = p_particles->lifetime;
	push_constant.trail_size = p_particles->trail_params.size();
	push_constant.use_fractional_delta = p_particles->fractional_delta;
	push_constant.sub_emitter_mode = !p_particles->emitting && p_particles->emission_buffer && (p_particles->emission_buffer->particle_count > 0 || p_particles->force_sub_emit);
	push_constant.trail_pass = false;

	p_particles->force_sub_emit = false; //reset

	Particles *sub_emitter = particles_owner.get_or_null(p_particles->sub_emitter);

	if (sub_emitter && sub_emitter->emission_storage_buffer.is_valid()) {
		//	print_line("updating subemitter buffer");
		int32_t zero[4] = { 0, sub_emitter->amount, 0, 0 };
		RD::get_singleton()->buffer_update(sub_emitter->emission_storage_buffer, 0, sizeof(uint32_t) * 4, zero);
		push_constant.can_emit = true;

		if (sub_emitter->emitting) {
			sub_emitter->emitting = false;
			sub_emitter->clear = true; //will need to clear if it was emitting, sorry
		}
		//make sure the sub emitter processes particles too
		sub_emitter->inactive = false;
		sub_emitter->inactive_time = 0;

		sub_emitter->force_sub_emit = true;

	} else {
		push_constant.can_emit = false;
	}

	if (p_particles->emission_buffer && p_particles->emission_buffer->particle_count) {
		RD::get_singleton()->buffer_update(p_particles->emission_storage_buffer, 0, sizeof(uint32_t) * 4 + sizeof(ParticleEmissionBuffer::Data) * p_particles->emission_buffer->particle_count, p_particles->emission_buffer);
		p_particles->emission_buffer->particle_count = 0;
	}

	p_particles->clear = false;

	if (p_particles->trail_params.size() > 1) {
		//fill the trail params
		for (uint32_t i = 0; i < p_particles->trail_params.size(); i++) {
			uint32_t src_idx = i * p_particles->frame_history.size() / p_particles->trail_params.size();
			p_particles->trail_params[i] = p_particles->frame_history[src_idx];
		}
	} else {
		p_particles->trail_params[0] = p_particles->frame_history[0];
	}

	RD::get_singleton()->buffer_update(p_particles->frame_params_buffer, 0, sizeof(ParticlesFrameParams) * p_particles->trail_params.size(), p_particles->trail_params.ptr());

	ParticleProcessMaterialData *m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(p_particles->process_material, MaterialStorage::SHADER_TYPE_PARTICLES));
	if (!m) {
		m = static_cast<ParticleProcessMaterialData *>(material_storage->material_get_data(particles_shader.default_material, MaterialStorage::SHADER_TYPE_PARTICLES));
	}

	ERR_FAIL_NULL(m);

	p_particles->has_collision_cache = m->shader_data->uses_collision;

	//todo should maybe compute all particle systems together?
	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, m->shader_data->pipeline);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles_shader.base_uniform_set, BASE_UNIFORM_SET);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, p_particles->particles_material_uniform_set, MATERIAL_UNIFORM_SET);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, p_particles->collision_textures_uniform_set, COLLISION_TEXTURTES_UNIFORM_SET);

	if (m->uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(m->uniform_set)) {
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, m->uniform_set, 3);
		m->set_as_used();
	}

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ParticlesShader::PushConstant));

	if (p_particles->trails_enabled && p_particles->trail_bind_poses.size() > 1) {
		//trails requires two passes in order to catch particle starts
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_amount / p_particles->trail_bind_poses.size(), 1, 1);

		RD::get_singleton()->compute_list_add_barrier(compute_list);

		push_constant.trail_pass = true;
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ParticlesShader::PushConstant));
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_amount - p_particles->amount, 1, 1);
	} else {
		RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_amount, 1, 1);
	}

	RD::get_singleton()->compute_list_end();
}

void ParticlesStorage::particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (particles->draw_order != RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY) {
		return;
	}

	if (particles->particle_buffer.is_null() || particles->trail_bind_pose_uniform_set.is_null()) {
		return; //particles have not processed yet
	}

	bool do_sort = particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH;

	//copy to sort buffer
	if (do_sort && particles->particles_sort_buffer == RID()) {
		uint32_t size = particles->amount;
		if (size & 1) {
			size++; //make multiple of 16
		}
		size *= sizeof(float) * 2;
		particles->particles_sort_buffer = RD::get_singleton()->storage_buffer_create(size);

		{
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(particles->particles_sort_buffer);
				uniforms.push_back(u);
			}

			particles->particles_sort_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, ParticlesShader::COPY_MODE_FILL_SORT_BUFFER), 1);
		}
	}

	ParticlesShader::CopyPushConstant copy_push_constant;

	if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
		int fixed_fps = 60.0;
		if (particles->fixed_fps > 0) {
			fixed_fps = particles->fixed_fps;
		}

		copy_push_constant.trail_size = particles->trail_bind_poses.size();
		copy_push_constant.trail_total = particles->frame_history.size();
		copy_push_constant.frame_delta = 1.0 / fixed_fps;
	} else {
		copy_push_constant.trail_size = 1;
		copy_push_constant.trail_total = 1;
		copy_push_constant.frame_delta = 0.0;
	}

	copy_push_constant.order_by_lifetime = (particles->draw_order == RS::PARTICLES_DRAW_ORDER_LIFETIME || particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME);
	copy_push_constant.lifetime_split = (MIN(int(particles->amount * particles->phase), particles->amount - 1) + 1) % particles->amount;
	copy_push_constant.lifetime_reverse = particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME;
	copy_push_constant.motion_vectors_current_offset = particles->instance_motion_vectors_current_offset;

	copy_push_constant.frame_remainder = particles->interpolate ? particles->frame_remainder : 0.0;
	copy_push_constant.total_particles = particles->amount;
	copy_push_constant.copy_mode_2d = false;

	Vector3 axis = -p_axis; // cameras look to z negative

	if (particles->use_local_coords) {
		axis = particles->emission_transform.basis.xform_inv(axis).normalized();
	}

	copy_push_constant.sort_direction[0] = axis.x;
	copy_push_constant.sort_direction[1] = axis.y;
	copy_push_constant.sort_direction[2] = axis.z;

	copy_push_constant.align_up[0] = p_up_axis.x;
	copy_push_constant.align_up[1] = p_up_axis.y;
	copy_push_constant.align_up[2] = p_up_axis.z;

	copy_push_constant.align_mode = particles->transform_align;

	if (do_sort) {
		RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, particles_shader.copy_pipelines[ParticlesShader::COPY_MODE_FILL_SORT_BUFFER + particles->userdata_count * ParticlesShader::COPY_MODE_MAX]);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_sort_uniform_set, 1);
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);
		RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

		RD::get_singleton()->compute_list_dispatch_threads(compute_list, particles->amount, 1, 1);

		RD::get_singleton()->compute_list_end();
		sort_effects->sort_buffer(particles->particles_sort_uniform_set, particles->amount);
	}

	if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
		copy_push_constant.total_particles *= particles->trail_bind_poses.size();
	}

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	uint32_t copy_pipeline = do_sort ? ParticlesShader::COPY_MODE_FILL_INSTANCES_WITH_SORT_BUFFER : ParticlesShader::COPY_MODE_FILL_INSTANCES;
	copy_pipeline += particles->userdata_count * ParticlesShader::COPY_MODE_MAX;
	copy_push_constant.copy_mode_2d = particles->mode == RS::PARTICLES_MODE_2D ? 1 : 0;
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, particles_shader.copy_pipelines[copy_pipeline]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
	if (do_sort) {
		RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_sort_uniform_set, 1);
	}
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, copy_push_constant.total_particles, 1, 1);

	RD::get_singleton()->compute_list_end();
}

void ParticlesStorage::_particles_update_buffers(Particles *particles) {
	uint32_t userdata_count = 0;

	MaterialStorage::ShaderData *shader_data = MaterialStorage::get_singleton()->material_get_shader_data(particles->process_material);
	if (shader_data) {
		const ParticlesShaderData *particle_shader_data = static_cast<const ParticlesShaderData *>(shader_data);
		userdata_count = particle_shader_data->userdata_count;
	}

	bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0 || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	bool index_draw_order = particles->draw_order == RS::ParticlesDrawOrder::PARTICLES_DRAW_ORDER_INDEX;
	bool enable_motion_vectors = uses_motion_vectors && index_draw_order && !particles->instance_motion_vectors_enabled;
	bool only_instances_changed = false;

	if (userdata_count != particles->userdata_count) {
		// Mismatch userdata, re-create all buffers.
		_particles_free_data(particles);
	} else if (enable_motion_vectors) {
		// Only motion vectors are required, release the transforms buffer and uniform set.
		if (particles->particle_instance_buffer.is_valid()) {
			RD::get_singleton()->free(particles->particle_instance_buffer);
			particles->particle_instance_buffer = RID();
		}

		particles->particles_transforms_buffer_uniform_set = RID();
		only_instances_changed = true;
	} else if (!particles->particle_buffer.is_null()) {
		// No operation is required because a buffer already exists, return early.
		return;
	}

	if (particles->amount > 0) {
		int total_amount = particles->amount;
		if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
			total_amount *= particles->trail_bind_poses.size();
		}

		uint32_t xform_size = particles->mode == RS::PARTICLES_MODE_2D ? 2 : 3;
		if (particles->particle_buffer.is_null()) {
			particles->particle_buffer = RD::get_singleton()->storage_buffer_create((sizeof(ParticleData) + userdata_count * sizeof(float) * 4) * total_amount);
			particles->userdata_count = userdata_count;
		}

		PackedByteArray data;
		uint32_t particle_instance_buffer_size = total_amount * (xform_size + 1 + 1) * sizeof(float) * 4;
		if (uses_motion_vectors) {
			particle_instance_buffer_size *= 2;
			particles->instance_motion_vectors_enabled = true;
		}

		data.resize_zeroed(particle_instance_buffer_size);

		particles->particle_instance_buffer = RD::get_singleton()->storage_buffer_create(particle_instance_buffer_size, data);

		{
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 1;
				u.append_id(particles->particle_buffer);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 2;
				u.append_id(particles->particle_instance_buffer);
				uniforms.push_back(u);
			}

			particles->particles_copy_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, 0), 0);
		}

		particles->instance_motion_vectors_current_offset = 0;
		particles->instance_motion_vectors_previous_offset = 0;
		particles->instance_motion_vectors_last_change = -1;

		if (only_instances_changed) {
			// Notify the renderer the instances uniform must be retrieved again, as it's the only element that has been changed because motion vectors were enabled.
			particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES_INSTANCES);
		}
	}
}
void ParticlesStorage::update_particles() {
	if (!particle_update_list.first()) {
		return;
	}

	RENDER_TIMESTAMP("Update GPUParticles");
	uint32_t frame = RSG::rasterizer->get_frame_number();
	bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0 || (RendererCompositorStorage::get_singleton()->get_num_compositor_effects_with_motion_vectors() > 0);
	while (particle_update_list.first()) {
		//use transform feedback to process particles

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
			particles->inactive_time += particles->speed_scale * RendererCompositorRD::get_singleton()->get_frame_delta_time();
			if (particles->inactive_time > particles->lifetime * 1.2) {
				particles->inactive = true;
				continue;
			}
		}

		// TODO: Should use display refresh rate for all this.
		float screen_hz = 60;

		int fixed_fps = 0;
		if (particles->fixed_fps > 0) {
			fixed_fps = particles->fixed_fps;
		} else if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
			fixed_fps = screen_hz;
		}
		{
			//update trails
			int history_size = 1;
			int trail_steps = 1;
			if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
				history_size = MAX(1, int(particles->trail_lifetime * fixed_fps));
				trail_steps = particles->trail_bind_poses.size();
			}

			if (uint32_t(history_size) != particles->frame_history.size()) {
				particles->frame_history.resize(history_size);
				memset(particles->frame_history.ptr(), 0, sizeof(ParticlesFrameParams) * history_size);
				// Set the frame number so that we are able to distinguish an uninitialized
				// frame from the true frame number zero. See issue #88712 for details.
				for (int i = 0; i < history_size; i++) {
					particles->frame_history[i].frame = UINT32_MAX;
				}
			}

			if (uint32_t(trail_steps) != particles->trail_params.size() || particles->frame_params_buffer.is_null()) {
				particles->trail_params.resize(trail_steps);
				if (particles->frame_params_buffer.is_valid()) {
					RD::get_singleton()->free(particles->frame_params_buffer);
				}
				particles->frame_params_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ParticlesFrameParams) * trail_steps);
			}

			if (particles->trail_bind_poses.size() > 1 && particles->trail_bind_pose_buffer.is_null()) {
				particles->trail_bind_pose_buffer = RD::get_singleton()->storage_buffer_create(sizeof(float) * 16 * particles->trail_bind_poses.size());
				particles->trail_bind_poses_dirty = true;
			}

			if (particles->trail_bind_pose_uniform_set.is_null()) {
				Vector<RD::Uniform> uniforms;
				{
					RD::Uniform u;
					u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
					u.binding = 0;
					if (particles->trail_bind_pose_buffer.is_valid()) {
						u.append_id(particles->trail_bind_pose_buffer);
					} else {
						_particles_ensure_unused_trail_buffer(particles);
						u.append_id(particles->unused_trail_storage_buffer);
					}
					uniforms.push_back(u);
				}

				particles->trail_bind_pose_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, particles_shader.copy_shader.version_get_shader(particles_shader.copy_shader_version, 0), 2);
			}

			if (particles->trail_bind_pose_buffer.is_valid() && particles->trail_bind_poses_dirty) {
				if (particles_shader.pose_update_buffer.size() < uint32_t(particles->trail_bind_poses.size()) * 16) {
					particles_shader.pose_update_buffer.resize(particles->trail_bind_poses.size() * 16);
				}

				for (int i = 0; i < particles->trail_bind_poses.size(); i++) {
					RendererRD::MaterialStorage::store_transform(particles->trail_bind_poses[i], &particles_shader.pose_update_buffer[i * 16]);
				}

				RD::get_singleton()->buffer_update(particles->trail_bind_pose_buffer, 0, particles->trail_bind_poses.size() * 16 * sizeof(float), particles_shader.pose_update_buffer.ptr());
			}
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
			double delta = RendererCompositorRD::get_singleton()->get_frame_delta_time();
			if (delta > 0.1) { //avoid recursive stalls if fps goes below 10
				delta = 0.1;
			} else if (delta <= 0.0) { //unlikely but..
				delta = 0.001;
			}
			double todo = particles->frame_remainder + delta;

			while (todo >= frame_time || particles->clear) {
				_particles_process(particles, frame_time);
				todo -= decr;
			}

			particles->frame_remainder = todo;

		} else {
			if (zero_time_scale) {
				_particles_process(particles, 0.0);
			} else {
				_particles_process(particles, RendererCompositorRD::get_singleton()->get_frame_delta_time());
			}
		}

		// Ensure that memory is initialized (the code above should ensure that _particles_process is always called at least once upon clearing).
		DEV_ASSERT(!particles->clear);

		int total_amount = particles->amount;
		if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
			total_amount *= particles->trail_bind_poses.size();
		}

		// Swap offsets for motion vectors. Motion vectors can only be used when the draw order keeps the indices consistent across frames.
		bool index_draw_order = particles->draw_order == RS::ParticlesDrawOrder::PARTICLES_DRAW_ORDER_INDEX;
		particles->instance_motion_vectors_previous_offset = particles->instance_motion_vectors_current_offset;
		if (uses_motion_vectors && index_draw_order && particles->instance_motion_vectors_enabled && (frame - particles->instance_motion_vectors_last_change) == 1) {
			particles->instance_motion_vectors_current_offset = total_amount - particles->instance_motion_vectors_current_offset;
		}

		particles->instance_motion_vectors_last_change = frame;

		// Copy particles to instance buffer.
		if (particles->draw_order != RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD && particles->transform_align != RS::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY) {
			//does not need view dependent operation, do copy here
			ParticlesShader::CopyPushConstant copy_push_constant;

			// Affect 2D only.
			if (particles->use_local_coords) {
				// In local mode, particle positions are calculated locally (relative to the node position)
				// and they're also drawn locally.
				// It works as expected, so we just pass an identity transform.
				RendererRD::MaterialStorage::store_transform(Transform3D(), copy_push_constant.inv_emission_transform);
			} else {
				// In global mode, particle positions are calculated globally (relative to the canvas origin)
				// but they're drawn locally.
				// So, we need to pass the inverse of the emission transform to bring the
				// particles to local coordinates before drawing.
				Transform3D inv = particles->emission_transform.affine_inverse();
				RendererRD::MaterialStorage::store_transform(inv, copy_push_constant.inv_emission_transform);
			}

			copy_push_constant.total_particles = total_amount;
			copy_push_constant.frame_remainder = particles->interpolate ? particles->frame_remainder : 0.0;
			copy_push_constant.align_mode = particles->transform_align;
			copy_push_constant.align_up[0] = 0;
			copy_push_constant.align_up[1] = 0;
			copy_push_constant.align_up[2] = 0;

			if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
				copy_push_constant.trail_size = particles->trail_bind_poses.size();
				copy_push_constant.trail_total = particles->frame_history.size();
				copy_push_constant.frame_delta = 1.0 / fixed_fps;
			} else {
				copy_push_constant.trail_size = 1;
				copy_push_constant.trail_total = 1;
				copy_push_constant.frame_delta = 0.0;
			}

			copy_push_constant.order_by_lifetime = (particles->draw_order == RS::PARTICLES_DRAW_ORDER_LIFETIME || particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME);
			copy_push_constant.lifetime_split = (MIN(int(particles->amount * particles->phase), particles->amount - 1) + 1) % particles->amount;
			copy_push_constant.lifetime_reverse = particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME;
			copy_push_constant.motion_vectors_current_offset = particles->instance_motion_vectors_current_offset;

			RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
			copy_push_constant.copy_mode_2d = particles->mode == RS::PARTICLES_MODE_2D ? 1 : 0;
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, particles_shader.copy_pipelines[ParticlesShader::COPY_MODE_FILL_INSTANCES + particles->userdata_count * ParticlesShader::COPY_MODE_MAX]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->particles_copy_uniform_set, 0);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, particles->trail_bind_pose_uniform_set, 2);
			RD::get_singleton()->compute_list_set_push_constant(compute_list, &copy_push_constant, sizeof(ParticlesShader::CopyPushConstant));

			RD::get_singleton()->compute_list_dispatch_threads(compute_list, total_amount, 1, 1);

			RD::get_singleton()->compute_list_end();
		}

		particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
	}
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

/* Particles SHADER */

void ParticlesStorage::ParticlesShaderData::set_code(const String &p_code) {
	ParticlesStorage *particles_storage = ParticlesStorage::get_singleton();
	//compile

	code = p_code;
	valid = false;
	ubo_size = 0;
	uniforms.clear();
	uses_collision = false;

	if (code.is_empty()) {
		return; //just invalid, but no error
	}

	ShaderCompiler::GeneratedCode gen_code;
	ShaderCompiler::IdentifierActions actions;
	actions.entry_point_stages["start"] = ShaderCompiler::STAGE_COMPUTE;
	actions.entry_point_stages["process"] = ShaderCompiler::STAGE_COMPUTE;

	/*
	uses_time = false;

	actions.render_mode_flags["use_half_res_pass"] = &uses_half_res;
	actions.render_mode_flags["use_quarter_res_pass"] = &uses_quarter_res;

	actions.usage_flag_pointers["TIME"] = &uses_time;
*/

	actions.usage_flag_pointers["COLLIDED"] = &uses_collision;

	userdata_count = 0;
	for (uint32_t i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
		userdatas_used[i] = false;
		actions.usage_flag_pointers["USERDATA" + itos(i + 1)] = &userdatas_used[i];
	}

	actions.uniforms = &uniforms;

	Error err = particles_storage->particles_shader.compiler.compile(RS::SHADER_PARTICLES, code, &actions, path, gen_code);
	ERR_FAIL_COND_MSG(err != OK, "Shader compilation failed.");

	if (version.is_null()) {
		version = particles_storage->particles_shader.shader.version_create();
	}

	for (uint32_t i = 0; i < ParticlesShader::MAX_USERDATAS; i++) {
		if (userdatas_used[i]) {
			userdata_count++;
		}
	}

	particles_storage->particles_shader.shader.version_set_compute_code(version, gen_code.code, gen_code.uniforms, gen_code.stage_globals[ShaderCompiler::STAGE_COMPUTE], gen_code.defines);
	ERR_FAIL_COND(!particles_storage->particles_shader.shader.version_is_valid(version));

	ubo_size = gen_code.uniform_total_size;
	ubo_offsets = gen_code.uniform_offsets;
	texture_uniforms = gen_code.texture_uniforms;

	//update pipelines

	pipeline = RD::get_singleton()->compute_pipeline_create(particles_storage->particles_shader.shader.version_get_shader(version, 0));

	valid = true;
}

bool ParticlesStorage::ParticlesShaderData::is_animated() const {
	return false;
}

bool ParticlesStorage::ParticlesShaderData::casts_shadows() const {
	return false;
}

RS::ShaderNativeSourceCode ParticlesStorage::ParticlesShaderData::get_native_source_code() const {
	return ParticlesStorage::get_singleton()->particles_shader.shader.version_get_native_source_code(version);
}

ParticlesStorage::ParticlesShaderData::~ParticlesShaderData() {
	//pipeline variants will clear themselves if shader is gone
	if (version.is_valid()) {
		ParticlesStorage::get_singleton()->particles_shader.shader.version_free(version);
	}
}

MaterialStorage::ShaderData *ParticlesStorage::_create_particles_shader_func() {
	ParticlesShaderData *shader_data = memnew(ParticlesShaderData);
	return shader_data;
}

bool ParticlesStorage::ParticleProcessMaterialData::update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) {
	return update_parameters_uniform_set(p_parameters, p_uniform_dirty, p_textures_dirty, shader_data->uniforms, shader_data->ubo_offsets.ptr(), shader_data->texture_uniforms, shader_data->default_texture_params, shader_data->ubo_size, uniform_set, ParticlesStorage::get_singleton()->particles_shader.shader.version_get_shader(shader_data->version, 0), 3, true, false);
}

ParticlesStorage::ParticleProcessMaterialData::~ParticleProcessMaterialData() {
	free_parameters_uniform_set(uniform_set);
}

MaterialStorage::MaterialData *ParticlesStorage::_create_particles_material_func(ParticlesShaderData *p_shader) {
	ParticleProcessMaterialData *material_data = memnew(ParticleProcessMaterialData);
	material_data->shader_data = p_shader;
	//update will happen later anyway so do nothing.
	return material_data;
}
////////

/* PARTICLES COLLISION API */

RID ParticlesStorage::particles_collision_allocate() {
	return particles_collision_owner.allocate_rid();
}
void ParticlesStorage::particles_collision_initialize(RID p_rid) {
	particles_collision_owner.initialize_rid(p_rid, ParticlesCollision());
}

void ParticlesStorage::particles_collision_free(RID p_rid) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_rid);

	if (particles_collision->heightfield_texture.is_valid()) {
		RD::get_singleton()->free(particles_collision->heightfield_texture);
	}
	particles_collision->dependency.deleted_notify(p_rid);
	particles_collision_owner.free(p_rid);
}

RID ParticlesStorage::particles_collision_get_heightfield_framebuffer(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, RID());
	ERR_FAIL_COND_V(particles_collision->type != RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE, RID());

	if (particles_collision->heightfield_texture == RID()) {
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

		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_D32_SFLOAT;
		tf.width = size.x;
		tf.height = size.y;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		particles_collision->heightfield_texture = RD::get_singleton()->texture_create(tf, RD::TextureView());

		Vector<RID> fb_tex;
		fb_tex.push_back(particles_collision->heightfield_texture);
		particles_collision->heightfield_fb = RD::get_singleton()->framebuffer_create(fb_tex);
		particles_collision->heightfield_fb_size = size;
	}

	return particles_collision->heightfield_fb;
}

void ParticlesStorage::particles_collision_set_collision_type(RID p_particles_collision, RS::ParticlesCollisionType p_type) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	if (p_type == particles_collision->type) {
		return;
	}

	if (particles_collision->heightfield_texture.is_valid()) {
		RD::get_singleton()->free(particles_collision->heightfield_texture);
		particles_collision->heightfield_texture = RID();
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
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->field_texture = p_texture;
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

	if (particles_collision->heightfield_texture.is_valid()) {
		RD::get_singleton()->free(particles_collision->heightfield_texture);
		particles_collision->heightfield_texture = RID();
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
