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

#ifndef PARTICLES_STORAGE_GLES3_H
#define PARTICLES_STORAGE_GLES3_H

#ifdef GLES3_ENABLED

#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "drivers/gles3/shaders/particles_copy.glsl.gen.h"
#include "servers/rendering/storage/particles_storage.h"
#include "servers/rendering/storage/utilities.h"

#include "platform_gl.h"

namespace GLES3 {

enum ParticlesUniformLocation {
	PARTICLES_FRAME_UNIFORM_LOCATION,
	PARTICLES_GLOBALS_UNIFORM_LOCATION,
	PARTICLES_MATERIAL_UNIFORM_LOCATION,
};

class ParticlesStorage : public RendererParticlesStorage {
private:
	static ParticlesStorage *singleton;

	/* PARTICLES */

	struct ParticleInstanceData3D {
		float xform[12];
		float color[2]; // Color and custom are packed together into one vec4;
		float custom[2];
	};

	struct ParticleInstanceData2D {
		float xform[8];
		float color[2]; // Color and custom are packed together into one vec4;
		float custom[2];
	};

	struct ParticlesViewSort {
		Vector3 z_dir;
		bool operator()(const ParticleInstanceData3D &p_a, const ParticleInstanceData3D &p_b) const {
			return z_dir.dot(Vector3(p_a.xform[3], p_a.xform[7], p_a.xform[11])) < z_dir.dot(Vector3(p_b.xform[3], p_b.xform[7], p_b.xform[11]));
		}
	};

	struct ParticlesFrameParams {
		enum {
			MAX_ATTRACTORS = 32,
			MAX_COLLIDERS = 32,
			MAX_3D_TEXTURES = 0 // GLES3 renderer doesn't support using 3D textures for flow field or collisions.
		};

		enum AttractorType {
			ATTRACTOR_TYPE_SPHERE,
			ATTRACTOR_TYPE_BOX,
			ATTRACTOR_TYPE_VECTOR_FIELD,
		};

		struct Attractor {
			float transform[16];
			float extents[4]; // Extents or radius. w-channel is padding.

			uint32_t type;
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
			float extents[4]; // Extents or radius. w-channel is padding.

			uint32_t type;
			float scale;
			float pad0;
			float pad1;
		};

		uint32_t emitting;
		uint32_t cycle;
		float system_phase;
		float prev_system_phase;

		float explosiveness;
		float randomness;
		float time;
		float delta;

		float particle_size;
		float amount_ratio;
		float pad1;
		float pad2;

		uint32_t random_seed;
		uint32_t attractor_count;
		uint32_t collider_count;
		uint32_t frame;

		float emission_transform[16];
		float emitter_velocity[3];
		float interp_to_end;

		Attractor attractors[MAX_ATTRACTORS];
		Collider colliders[MAX_COLLIDERS];
	};

	static_assert(sizeof(ParticlesFrameParams) % 16 == 0, "ParticlesFrameParams size must be a multiple of 16 bytes");
	static_assert(sizeof(ParticlesFrameParams) < 16384, "ParticlesFrameParams must be 16384 bytes or smaller");

	struct Particles {
		RS::ParticlesMode mode = RS::PARTICLES_MODE_3D;
		bool inactive = true;
		double inactive_time = 0.0;
		bool emitting = false;
		bool one_shot = false;
		float amount_ratio = 1.0;
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
		GLuint sdf_collision_texture = 0;

		RID process_material;
		uint32_t frame_counter = 0;
		RS::ParticlesTransformAlign transform_align = RS::PARTICLES_TRANSFORM_ALIGN_DISABLED;

		RS::ParticlesDrawOrder draw_order = RS::PARTICLES_DRAW_ORDER_INDEX;

		Vector<RID> draw_passes;

		GLuint frame_params_ubo = 0;

		// We may process particles multiple times each frame (if they have a fixed FPS higher than the game FPS).
		// Unfortunately, this means we can't just use a round-robin system of 3 buffers.
		// To ensure the sort buffer is accurate, we copy the last frame instance buffer just before processing.

		// Transform Feedback buffer and VAO for rendering.
		// Each frame we render to this one.
		GLuint front_vertex_array = 0; // Binds process buffer. Used for processing.
		GLuint front_process_buffer = 0; // Transform + color + custom data + userdata + velocity + flags. Only needed for processing.
		GLuint front_instance_buffer = 0; // Transform + color + custom data. In packed format needed for rendering.

		// VAO for transform feedback, contains last frame's data.
		// Read from this one for particles process and then copy to last frame buffer.
		GLuint back_vertex_array = 0; // Binds process buffer. Used for processing.
		GLuint back_process_buffer = 0; // Transform + color + custom data + userdata + velocity + flags. Only needed for processing.
		GLuint back_instance_buffer = 0; // Transform + color + custom data. In packed format needed for rendering.

		uint32_t instance_buffer_size_cache = 0;
		uint32_t instance_buffer_stride_cache = 0;
		uint32_t num_attrib_arrays_cache = 0;
		uint32_t process_buffer_stride_cache = 0;

		// Only ever copied to, holds last frame's instance data, then swaps with sort_buffer.
		GLuint last_frame_buffer = 0;
		bool last_frame_buffer_filled = false;
		float last_frame_phase = 0.0;

		// The frame-before-last's instance buffer.
		// Use this to copy data back for sorting or computing AABB.
		GLuint sort_buffer = 0;
		bool sort_buffer_filled = false;
		float sort_buffer_phase = 0.0;

		uint32_t userdata_count = 0;

		bool dirty = false;
		SelfList<Particles> update_list;

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

		bool clear = true;

		Transform3D emission_transform;
		Vector3 emitter_velocity;
		float interp_to_end = 0.0;

		HashSet<RID> collisions;

		Dependency dependency;

		double trail_length = 1.0;
		bool trails_enabled = false;

		Particles() :
				update_list(this) {
			random_seed = Math::rand();
		}
	};

	void _particles_process(Particles *p_particles, double p_delta);
	void _particles_free_data(Particles *particles);
	void _particles_update_buffers(Particles *particles);
	void _particles_allocate_history_buffers(Particles *particles);
	void _particles_update_instance_buffer(Particles *particles, const Vector3 &p_axis, const Vector3 &p_up_axis);

	template <typename T>
	void _particles_reverse_lifetime_sort(Particles *particles);

	struct ParticlesShader {
		RID default_shader;
		RID default_material;
		RID default_shader_version;

		ParticlesCopyShaderGLES3 copy_shader;
		RID copy_shader_version;
	} particles_shader;

	SelfList<Particles>::List particle_update_list;

	mutable RID_Owner<Particles, true> particles_owner;

	/* Particles Collision */

	struct ParticlesCollision {
		RS::ParticlesCollisionType type = RS::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT;
		uint32_t cull_mask = 0xFFFFFFFF;
		float radius = 1.0;
		Vector3 extents = Vector3(1, 1, 1);
		float attractor_strength = 1.0;
		float attractor_attenuation = 1.0;
		float attractor_directionality = 0.0;
		GLuint field_texture = 0;
		GLuint heightfield_texture = 0;
		GLuint heightfield_fb = 0;
		Size2i heightfield_fb_size;

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
	virtual void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) override;
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
	virtual void particles_set_subemitter(RID p_particles, RID p_subemitter_particles) override;
	virtual void particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) override;
	virtual void particles_set_collision_base_size(RID p_particles, real_t p_size) override;

	virtual void particles_set_transform_align(RID p_particles, RS::ParticlesTransformAlign p_transform_align) override;
	virtual void particles_set_seed(RID p_particles, uint32_t p_seed) override;

	virtual void particles_set_trails(RID p_particles, bool p_enable, double p_length) override;
	virtual void particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) override;

	virtual void particles_restart(RID p_particles) override;

	virtual void particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) override;

	virtual void particles_set_draw_passes(RID p_particles, int p_count) override;
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;

	virtual void particles_request_process(RID p_particles) override;
	virtual AABB particles_get_current_aabb(RID p_particles) override;
	virtual AABB particles_get_aabb(RID p_particles) const override;

	virtual void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) override;
	virtual void particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) override;
	virtual void particles_set_interp_to_end(RID p_particles, float p_interp) override;

	virtual bool particles_get_emitting(RID p_particles) override;
	virtual int particles_get_draw_passes(RID p_particles) const override;
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;

	virtual void particles_add_collision(RID p_particles, RID p_instance) override;
	virtual void particles_remove_collision(RID p_particles, RID p_instance) override;

	void particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, GLuint p_texture);

	virtual void update_particles() override;
	virtual bool particles_is_inactive(RID p_particles) const override;

	_FORCE_INLINE_ RS::ParticlesMode particles_get_mode(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, RS::PARTICLES_MODE_2D);
		return particles->mode;
	}

	_FORCE_INLINE_ uint32_t particles_get_amount(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, 0);

		return particles->amount;
	}

	_FORCE_INLINE_ GLuint particles_get_gl_buffer(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);

		if ((particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH || particles->draw_order == RS::PARTICLES_DRAW_ORDER_REVERSE_LIFETIME) && particles->sort_buffer_filled) {
			return particles->sort_buffer;
		}
		return particles->back_instance_buffer;
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

	Dependency *particles_get_dependency(RID p_particles) const;

	/* PARTICLES COLLISION */
	bool owns_particles_collision(RID p_rid) { return particles_collision_owner.owns(p_rid); }

	virtual RID particles_collision_allocate() override;
	virtual void particles_collision_initialize(RID p_rid) override;
	virtual void particles_collision_free(RID p_rid) override;

	virtual void particles_collision_set_collision_type(RID p_particles_collision, RS::ParticlesCollisionType p_type) override;
	virtual void particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) override;
	virtual void particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) override;
	virtual void particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) override;
	virtual void particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) override;
	virtual void particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) override;
	virtual void particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) override;
	virtual void particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) override;
	virtual void particles_collision_height_field_update(RID p_particles_collision) override;
	virtual void particles_collision_set_height_field_resolution(RID p_particles_collision, RS::ParticlesCollisionHeightfieldResolution p_resolution) override;
	virtual AABB particles_collision_get_aabb(RID p_particles_collision) const override;
	Vector3 particles_collision_get_extents(RID p_particles_collision) const;
	virtual bool particles_collision_is_heightfield(RID p_particles_collision) const override;
	GLuint particles_collision_get_heightfield_framebuffer(RID p_particles_collision) const;

	_FORCE_INLINE_ Size2i particles_collision_get_heightfield_size(RID p_particles_collision) const {
		ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
		ERR_FAIL_NULL_V(particles_collision, Size2i());
		ERR_FAIL_COND_V(particles_collision->type != RS::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE, Size2i());

		return particles_collision->heightfield_fb_size;
	}

	Dependency *particles_collision_get_dependency(RID p_particles) const;

	/* PARTICLES COLLISION INSTANCE*/
	bool owns_particles_collision_instance(RID p_rid) { return particles_collision_instance_owner.owns(p_rid); }

	virtual RID particles_collision_instance_create(RID p_collision) override;
	virtual void particles_collision_instance_free(RID p_rid) override;
	virtual void particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) override;
	virtual void particles_collision_instance_set_active(RID p_collision_instance, bool p_active) override;
};

} // namespace GLES3

#endif // GLES3_ENABLED

#endif // PARTICLES_STORAGE_GLES3_H
