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

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/renderer_rd/effects/sort_effects.h"
#include "servers/rendering/renderer_rd/shaders/particles.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/particles_copy.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/shader_compiler.h"
#include "servers/rendering/storage/particles_storage.h"
#include "servers/rendering/storage/utilities.h"

namespace RendererRD {

class ParticlesStorage : public RendererParticlesStorage {
private:
	static ParticlesStorage *singleton;

	/* EFFECTS */
	SortEffects *sort_effects = nullptr;

	/* PARTICLES */

	enum {
		BASE_UNIFORM_SET,
		MATERIAL_UNIFORM_SET,
		COLLISION_TEXTURTES_UNIFORM_SET,
	};

	const int SAMPLERS_BINDING_FIRST_INDEX = 3;

	struct ParticleData {
		float xform[16];
		float velocity[3];
		uint32_t active;
		float color[4];
		float custom[4];
	};

	struct ParticlesFrameParams {
		enum {
			MAX_ATTRACTORS = 32,
			MAX_COLLIDERS = 32,
			MAX_3D_TEXTURES = 7
		};

		enum AttractorType {
			ATTRACTOR_TYPE_SPHERE,
			ATTRACTOR_TYPE_BOX,
			ATTRACTOR_TYPE_VECTOR_FIELD,
		};

		struct Attractor {
			float transform[16];
			float extents[3]; //exents or radius
			uint32_t type;

			uint32_t texture_index; //texture index for vector field
			float strength;
			float attenuation;
			float directionality;
		};

		enum CollisionType {
			COLLISION_TYPE_SPHERE,
			COLLISION_TYPE_BOX,
			COLLISION_TYPE_SDF,
			COLLISION_TYPE_HEIGHT_FIELD,
			COLLISION_TYPE_2D_SDF,

		};

		struct Collider {
			float transform[16];
			float extents[3]; //exents or radius
			uint32_t type;

			uint32_t texture_index; //texture index for vector field
			float scale;
			uint32_t pad[2];
		};

		uint32_t emitting;
		float system_phase;
		float prev_system_phase;
		uint32_t cycle;

		float explosiveness;
		float randomness;
		float time;
		float delta;

		uint32_t frame;
		float amount_ratio;
		uint32_t pad1;
		uint32_t pad2;

		uint32_t random_seed;
		uint32_t attractor_count;
		uint32_t collider_count;
		float particle_size;

		float emission_transform[16];

		float emitter_velocity[3];
		float interp_to_end;

		Attractor attractors[MAX_ATTRACTORS];
		Collider colliders[MAX_COLLIDERS];
	};

	struct ParticleEmissionBuffer {
		struct Data {
			float xform[16];
			float velocity[3];
			uint32_t flags;
			float color[4];
			float custom[4];
		};

		int32_t particle_count;
		int32_t particle_max;
		uint32_t pad1;
		uint32_t pad2;
		Data data[1]; //its 2020 and empty arrays are still non standard in C++
	};

	struct Particles {
		RS::ParticlesMode mode = RS::PARTICLES_MODE_3D;
		bool inactive = true;
		double inactive_time = 0.0;
		bool emitting = false;
		bool one_shot = false;
		int amount = 0;
		double lifetime = 1.0;
		double pre_process_time = 0.0;
		real_t request_process_time = 0.0;
		real_t explosiveness = 0.0;
		real_t randomness = 0.0;
		bool restart_request = false;
		AABB custom_aabb = AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8));
		bool use_local_coords = false;
		bool has_collision_cache = false;

		bool has_sdf_collision = false;
		Transform2D sdf_collision_transform;
		Rect2 sdf_collision_to_screen;
		RID sdf_collision_texture;

		RID process_material;
		uint32_t frame_counter = 0;
		RS::ParticlesTransformAlign transform_align = RS::PARTICLES_TRANSFORM_ALIGN_DISABLED;

		RS::ParticlesDrawOrder draw_order = RS::PARTICLES_DRAW_ORDER_INDEX;

		Vector<RID> draw_passes;
		Vector<Transform3D> trail_bind_poses;
		bool trail_bind_poses_dirty = false;
		RID trail_bind_pose_buffer;
		RID trail_bind_pose_uniform_set;

		RID particle_buffer;
		RID particle_instance_buffer;
		RID frame_params_buffer;

		uint32_t userdata_count = 0;

		RID particles_material_uniform_set;
		RID particles_copy_uniform_set;
		RID particles_transforms_buffer_uniform_set;
		RID collision_textures_uniform_set;

		RID collision_3d_textures[ParticlesFrameParams::MAX_3D_TEXTURES];
		uint32_t collision_3d_textures_used = 0;
		RID collision_heightmap_texture;

		RID particles_sort_buffer;
		RID particles_sort_uniform_set;

		bool dirty = false;
		SelfList<Particles> update_list;

		RID sub_emitter;

		double phase = 0.0;
		double prev_phase = 0.0;
		uint64_t prev_ticks = 0;
		uint32_t random_seed = 0;

		uint32_t cycle_number = 0;

		double speed_scale = 1.0;

		int fixed_fps = 30;
		bool interpolate = true;
		bool fractional_delta = false;
		double frame_remainder = 0;
		real_t collision_base_size = 0.01;

		uint32_t instance_motion_vectors_current_offset = 0;
		uint32_t instance_motion_vectors_previous_offset = 0;
		uint64_t instance_motion_vectors_last_change = -1;
		bool instance_motion_vectors_enabled = false;

		bool clear = true;

		bool force_sub_emit = false;

		Transform3D emission_transform;
		Vector3 emitter_velocity;
		float interp_to_end = 0.0;
		float amount_ratio = 1.0;

		Vector<uint8_t> emission_buffer_data;

		ParticleEmissionBuffer *emission_buffer = nullptr;
		RID emission_storage_buffer;

		RID unused_emission_storage_buffer;
		RID unused_trail_storage_buffer;

		HashSet<RID> collisions;

		Dependency dependency;

		double trail_lifetime = 0.3;
		bool trails_enabled = false;
		LocalVector<ParticlesFrameParams> frame_history;
		LocalVector<ParticlesFrameParams> trail_params;

		Particles() :
				update_list(this) {
			random_seed = Math::rand();
		}
	};

	void _particles_process(Particles *p_particles, double p_delta);
	void _particles_allocate_emission_buffer(Particles *particles);
	void _particles_ensure_unused_emission_buffer(Particles *particles);
	void _particles_ensure_unused_trail_buffer(Particles *particles);
	void _particles_free_data(Particles *particles);
	void _particles_update_buffers(Particles *particles);

	struct ParticlesShader {
		struct PushConstant {
			float lifetime;
			uint32_t clear;
			uint32_t total_particles;
			uint32_t trail_size;

			uint32_t use_fractional_delta;
			uint32_t sub_emitter_mode;
			uint32_t can_emit;
			uint32_t trail_pass;
		};

		ParticlesShaderRD shader;
		ShaderCompiler compiler;

		RID default_shader;
		RID default_material;
		RID default_shader_rd;

		RID base_uniform_set;

		struct CopyPushConstant {
			float sort_direction[3];
			uint32_t total_particles;

			uint32_t trail_size;
			uint32_t trail_total;
			float frame_delta;
			float frame_remainder;

			float align_up[3];
			uint32_t align_mode;

			uint32_t lifetime_split;
			uint32_t lifetime_reverse;
			uint32_t motion_vectors_current_offset;
			struct {
				uint32_t order_by_lifetime : 1;
				uint32_t copy_mode_2d : 1;
			};

			float inv_emission_transform[16];
		};

		enum {
			MAX_USERDATAS = 6
		};
		enum {
			COPY_MODE_FILL_INSTANCES,
			COPY_MODE_FILL_SORT_BUFFER,
			COPY_MODE_FILL_INSTANCES_WITH_SORT_BUFFER,
			COPY_MODE_MAX,
		};

		ParticlesCopyShaderRD copy_shader;
		RID copy_shader_version;
		RID copy_pipelines[COPY_MODE_MAX * (MAX_USERDATAS + 1)];

		LocalVector<float> pose_update_buffer;

	} particles_shader;

	SelfList<Particles>::List particle_update_list;

	mutable RID_Owner<Particles, true> particles_owner;

	/* Particle Shader */

	struct ParticlesShaderData : public MaterialStorage::ShaderData {
		bool valid = false;
		RID version;
		bool uses_collision = false;

		Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

		Vector<uint32_t> ubo_offsets;
		uint32_t ubo_size = 0;

		String code;

		RID pipeline;

		bool uses_time = false;

		bool userdatas_used[ParticlesShader::MAX_USERDATAS] = {};
		uint32_t userdata_count = 0;

		virtual void set_code(const String &p_Code);
		virtual bool is_animated() const;
		virtual bool casts_shadows() const;
		virtual RS::ShaderNativeSourceCode get_native_source_code() const;

		ParticlesShaderData() {}
		virtual ~ParticlesShaderData();
	};

	MaterialStorage::ShaderData *_create_particles_shader_func();
	static MaterialStorage::ShaderData *_create_particles_shader_funcs() {
		return ParticlesStorage::get_singleton()->_create_particles_shader_func();
	}

	struct ParticleProcessMaterialData : public MaterialStorage::MaterialData {
		ParticlesShaderData *shader_data = nullptr;
		RID uniform_set;

		virtual void set_render_priority(int p_priority) {}
		virtual void set_next_pass(RID p_pass) {}
		virtual bool update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
		virtual ~ParticleProcessMaterialData();
	};

	MaterialStorage::MaterialData *_create_particles_material_func(ParticlesShaderData *p_shader);
	static MaterialStorage::MaterialData *_create_particles_material_funcs(MaterialStorage::ShaderData *p_shader) {
		return ParticlesStorage::get_singleton()->_create_particles_material_func(static_cast<ParticlesShaderData *>(p_shader));
	}

	/* Particles Collision */

	struct ParticlesCollision {
		RS::ParticlesCollisionType type = RS::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT;
		uint32_t cull_mask = 0xFFFFFFFF;
		float radius = 1.0;
		Vector3 extents = Vector3(1, 1, 1);
		float attractor_strength = 1.0;
		float attractor_attenuation = 1.0;
		float attractor_directionality = 0.0;
		RID field_texture;
		RID heightfield_texture;
		RID heightfield_fb;
		Size2i heightfield_fb_size;
		uint32_t heightfield_mask = (1 << 20) - 1;

		RS::ParticlesCollisionHeightfieldResolution heightfield_resolution = RS::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024;

		Dependency dependency;
	};

	struct ParticlesCollisionInstance {
		RID collision;
		Transform3D transform;
		bool active = false;
	};

	mutable RID_Owner<ParticlesCollision, true> particles_collision_owner;

	mutable RID_Owner<ParticlesCollisionInstance> particles_collision_instance_owner;

public:
	static ParticlesStorage *get_singleton();

	ParticlesStorage();
	virtual ~ParticlesStorage();

	bool free(RID p_rid);

	/* PARTICLES */

	bool owns_particles(RID p_rid) { return particles_owner.owns(p_rid); }

	virtual RID particles_allocate() override;
	virtual void particles_initialize(RID p_rid) override;
	virtual void particles_free(RID p_rid) override;

	virtual void particles_set_mode(RID p_particles, RS::ParticlesMode p_mode) override;
	virtual void particles_set_emitting(RID p_particles, bool p_emitting) override;
	virtual void particles_set_amount(RID p_particles, int p_amount) override;
	virtual void particles_set_amount_ratio(RID p_particles, float p_amount_ratio) override;
	virtual void particles_set_lifetime(RID p_particles, double p_lifetime) override;
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) override;
	virtual void particles_set_pre_process_time(RID p_particles, double p_time) override;
	virtual void particles_request_process_time(RID p_particles, real_t p_request_process_time) override;
	virtual void particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) override;
	virtual void particles_set_randomness_ratio(RID p_particles, real_t p_ratio) override;
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override;
	virtual void particles_set_speed_scale(RID p_particles, double p_scale) override;
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override;
	virtual void particles_set_process_material(RID p_particles, RID p_material) override;
	virtual RID particles_get_process_material(RID p_particles) const override;

	virtual void particles_set_fixed_fps(RID p_particles, int p_fps) override;
	virtual void particles_set_interpolate(RID p_particles, bool p_enable) override;
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) override;
	virtual void particles_set_collision_base_size(RID p_particles, real_t p_size) override;
	virtual void particles_set_transform_align(RID p_particles, RS::ParticlesTransformAlign p_transform_align) override;
	virtual void particles_set_seed(RID p_particles, uint32_t p_seed) override;

	virtual void particles_set_trails(RID p_particles, bool p_enable, double p_length) override;
	virtual void particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) override;

	virtual void particles_restart(RID p_particles) override;
	virtual void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) override;

	virtual void particles_set_subemitter(RID p_particles, RID p_subemitter_particles) override;

	virtual void particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) override;

	virtual void particles_set_draw_passes(RID p_particles, int p_count) override;
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;

	virtual void particles_request_process(RID p_particles) override;
	virtual AABB particles_get_current_aabb(RID p_particles) override;
	virtual AABB particles_get_aabb(RID p_particles) const override;

	virtual void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) override;
	virtual void particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) override;
	virtual void particles_set_interp_to_end(RID p_particles, float p_interp_to_end) override;

	virtual bool particles_get_emitting(RID p_particles) override;
	virtual int particles_get_draw_passes(RID p_particles) const override;
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;

	virtual void particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) override;

	virtual bool particles_is_inactive(RID p_particles) const override;

	_FORCE_INLINE_ RS::ParticlesMode particles_get_mode(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, RS::PARTICLES_MODE_2D);
		return particles->mode;
	}

	_FORCE_INLINE_ uint32_t particles_get_frame_counter(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, false);
		return particles->frame_counter;
	}

	_FORCE_INLINE_ uint32_t particles_get_amount(RID p_particles, uint32_t &r_trail_divisor) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, 0);

		if (particles->trails_enabled && particles->trail_bind_poses.size() > 1) {
			r_trail_divisor = particles->trail_bind_poses.size();
		} else {
			r_trail_divisor = 1;
		}

		return particles->amount * r_trail_divisor;
	}

	_FORCE_INLINE_ bool particles_has_collision(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, false);

		return particles->has_collision_cache;
	}

	_FORCE_INLINE_ uint32_t particles_is_using_local_coords(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, false);

		return particles->use_local_coords;
	}

	_FORCE_INLINE_ RID particles_get_instance_buffer_uniform_set(RID p_particles, RID p_shader, uint32_t p_set) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, RID());
		if (particles->particles_transforms_buffer_uniform_set.is_null() || !RD::get_singleton()->uniform_set_is_valid(particles->particles_transforms_buffer_uniform_set)) {
			_particles_update_buffers(particles);
			Vector<RD::Uniform> uniforms;

			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 0;
				u.append_id(particles->particle_instance_buffer);
				uniforms.push_back(u);
			}

			particles->particles_transforms_buffer_uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_shader, p_set);
		}

		return particles->particles_transforms_buffer_uniform_set;
	}

	void particles_get_instance_buffer_motion_vectors_offsets(RID p_particles, uint32_t &r_current_offset, uint32_t &r_prev_offset);

	virtual void particles_add_collision(RID p_particles, RID p_particles_collision_instance) override;
	virtual void particles_remove_collision(RID p_particles, RID p_particles_collision_instance) override;
	void particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, RID p_texture);

	virtual void update_particles() override;

	void particles_update_dependency(RID p_particles, DependencyTracker *p_instance);
	Dependency *particles_get_dependency(RID p_particles) const;

	/* Particles Collision */

	bool owns_particles_collision(RID p_rid) { return particles_collision_owner.owns(p_rid); }

	virtual RID particles_collision_allocate() override;
	virtual void particles_collision_initialize(RID p_particles_collision) override;
	virtual void particles_collision_free(RID p_rid) override;

	virtual void particles_collision_set_collision_type(RID p_particles_collision, RS::ParticlesCollisionType p_type) override;
	virtual void particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) override;
	virtual void particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) override; //for spheres
	virtual void particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) override; //for non-spheres
	virtual void particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) override;
	virtual void particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) override;
	virtual void particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) override;
	virtual void particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) override; //for SDF and vector field, heightfield is dynamic
	virtual void particles_collision_height_field_update(RID p_particles_collision) override; //for SDF and vector field
	virtual void particles_collision_set_height_field_resolution(RID p_particles_collision, RS::ParticlesCollisionHeightfieldResolution p_resolution) override; //for SDF and vector field
	virtual AABB particles_collision_get_aabb(RID p_particles_collision) const override;
	Vector3 particles_collision_get_extents(RID p_particles_collision) const;
	virtual bool particles_collision_is_heightfield(RID p_particles_collision) const override;
	RID particles_collision_get_heightfield_framebuffer(RID p_particles_collision) const;
	virtual uint32_t particles_collision_get_height_field_mask(RID p_particles_collision) const override;
	virtual void particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) override;

	Dependency *particles_collision_get_dependency(RID p_particles) const;

	//used from 2D and 3D
	bool owns_particles_collision_instance(RID p_rid) { return particles_collision_instance_owner.owns(p_rid); }

	virtual RID particles_collision_instance_create(RID p_collision) override;
	virtual void particles_collision_instance_free(RID p_rid) override;
	virtual void particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) override;
	virtual void particles_collision_instance_set_active(RID p_collision_instance, bool p_active) override;
};

} // namespace RendererRD
