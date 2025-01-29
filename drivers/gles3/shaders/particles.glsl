/* clang-format off */
#[modes]

mode_default =

#[specializations]

MODE_3D = false
USERDATA1_USED = false
USERDATA2_USED = false
USERDATA3_USED = false
USERDATA4_USED = false
USERDATA5_USED = false
USERDATA6_USED = false

#[vertex]

#define SDF_MAX_LENGTH 16384.0

layout(std140) uniform GlobalShaderUniformData { //ubo:1
	vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
};

// This needs to be outside clang-format so the ubo comment is in the right place
#ifdef MATERIAL_UNIFORMS_USED
layout(std140) uniform MaterialUniforms{ //ubo:2

#MATERIAL_UNIFORMS

};
#endif

/* clang-format on */

#define MAX_ATTRACTORS 32

#define ATTRACTOR_TYPE_SPHERE uint(0)
#define ATTRACTOR_TYPE_BOX uint(1)
#define ATTRACTOR_TYPE_VECTOR_FIELD uint(2)

struct Attractor {
	mat4 transform;
	vec4 extents; // Extents or radius. w-channel is padding.

	uint type;
	float strength;
	float attenuation;
	float directionality;
};

#define MAX_COLLIDERS 32

#define COLLIDER_TYPE_SPHERE uint(0)
#define COLLIDER_TYPE_BOX uint(1)
#define COLLIDER_TYPE_SDF uint(2)
#define COLLIDER_TYPE_HEIGHT_FIELD uint(3)
#define COLLIDER_TYPE_2D_SDF uint(4)

struct Collider {
	mat4 transform;
	vec4 extents; // Extents or radius. w-channel is padding.

	uint type;
	float scale;
	float pad0;
	float pad1;
};

layout(std140) uniform FrameData { //ubo:0
	bool emitting;
	uint cycle;
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

	uint random_seed;
	uint attractor_count;
	uint collider_count;
	uint frame;

	mat4 emission_transform;

	vec3 emitter_velocity;
	float interp_to_end;

	Attractor attractors[MAX_ATTRACTORS];
	Collider colliders[MAX_COLLIDERS];
};

#define PARTICLE_FLAG_ACTIVE uint(1)
#define PARTICLE_FLAG_STARTED uint(2)
#define PARTICLE_FLAG_TRAILED uint(4)
#define PARTICLE_FRAME_MASK uint(0xFFFF)
#define PARTICLE_FRAME_SHIFT uint(16)

// ParticleData
layout(location = 0) in highp vec4 color;
layout(location = 1) in highp vec4 velocity_flags;
layout(location = 2) in highp vec4 custom;
layout(location = 3) in highp vec4 xform_1;
layout(location = 4) in highp vec4 xform_2;
#ifdef MODE_3D
layout(location = 5) in highp vec4 xform_3;
#endif
#ifdef USERDATA1_USED
in highp vec4 userdata1;
#endif
#ifdef USERDATA2_USED
in highp vec4 userdata2;
#endif
#ifdef USERDATA3_USED
in highp vec4 userdata3;
#endif
#ifdef USERDATA4_USED
in highp vec4 userdata4;
#endif
#ifdef USERDATA5_USED
in highp vec4 userdata5;
#endif
#ifdef USERDATA6_USED
in highp vec4 userdata6;
#endif

out highp vec4 out_color; //tfb:
out highp vec4 out_velocity_flags; //tfb:
out highp vec4 out_custom; //tfb:
out highp vec4 out_xform_1; //tfb:
out highp vec4 out_xform_2; //tfb:
#ifdef MODE_3D
out highp vec4 out_xform_3; //tfb:MODE_3D
#endif
#ifdef USERDATA1_USED
out highp vec4 out_userdata1; //tfb:USERDATA1_USED
#endif
#ifdef USERDATA2_USED
out highp vec4 out_userdata2; //tfb:USERDATA2_USED
#endif
#ifdef USERDATA3_USED
out highp vec4 out_userdata3; //tfb:USERDATA3_USED
#endif
#ifdef USERDATA4_USED
out highp vec4 out_userdata4; //tfb:USERDATA4_USED
#endif
#ifdef USERDATA5_USED
out highp vec4 out_userdata5; //tfb:USERDATA5_USED
#endif
#ifdef USERDATA6_USED
out highp vec4 out_userdata6; //tfb:USERDATA6_USED
#endif

uniform sampler2D height_field_texture; //texunit:0

uniform float lifetime;
uniform bool clear;
uniform uint total_particles;
uniform bool use_fractional_delta;

uint hash(uint x) {
	x = ((x >> uint(16)) ^ x) * uint(0x45d9f3b);
	x = ((x >> uint(16)) ^ x) * uint(0x45d9f3b);
	x = (x >> uint(16)) ^ x;
	return x;
}

vec3 safe_normalize(vec3 direction) {
	const float EPSILON = 0.001;
	if (length(direction) < EPSILON) {
		return vec3(0.0);
	}
	return normalize(direction);
}

// Needed whenever 2D sdf texture is read from as it is packed in RGBA8.
float vec4_to_float(vec4 p_vec) {
	return dot(p_vec, vec4(1.0 / (255.0 * 255.0 * 255.0), 1.0 / (255.0 * 255.0), 1.0 / 255.0, 1.0)) * 2.0 - 1.0;
}

#GLOBALS

void main() {
	bool apply_forces = true;
	bool apply_velocity = true;
	float local_delta = delta;

	float mass = 1.0;

	bool restart = false;

	bool restart_position = false;
	bool restart_rotation_scale = false;
	bool restart_velocity = false;
	bool restart_color = false;
	bool restart_custom = false;

	mat4 xform = mat4(1.0);
	uint flags = 0u;

	if (clear) {
		out_color = vec4(1.0);
		out_custom = vec4(0.0);
		out_velocity_flags = vec4(0.0);
	} else {
		out_color = color;
		out_velocity_flags = velocity_flags;
		out_custom = custom;
		xform[0] = xform_1;
		xform[1] = xform_2;
#ifdef MODE_3D
		xform[2] = xform_3;
#endif
		xform = transpose(xform);
		flags = floatBitsToUint(velocity_flags.w);
#ifdef USERDATA1_USED
		out_userdata1 = userdata1;
#endif
#ifdef USERDATA2_USED
		out_userdata2 = userdata2;
#endif
#ifdef USERDATA3_USED
		out_userdata3 = userdata3;
#endif
#ifdef USERDATA4_USED
		out_userdata4 = userdata4;
#endif
#ifdef USERDATA5_USED
		out_userdata5 = userdata5;
#endif
#ifdef USERDATA6_USED
		out_userdata6 = userdata6;
#endif
	}

	//clear started flag if set
	flags &= ~PARTICLE_FLAG_STARTED;

	bool collided = false;
	vec3 collision_normal = vec3(0.0);
	float collision_depth = 0.0;

	vec3 attractor_force = vec3(0.0);

#if !defined(DISABLE_VELOCITY)

	if (bool(flags & PARTICLE_FLAG_ACTIVE)) {
		xform[3].xyz += out_velocity_flags.xyz * local_delta;
	}
#endif
	uint index = uint(gl_VertexID);
	if (emitting) {
		float restart_phase = float(index) / float(total_particles);

		if (randomness > 0.0) {
			uint seed = cycle;
			if (restart_phase >= system_phase) {
				seed -= uint(1);
			}
			seed *= uint(total_particles);
			seed += index;
			float random = float(hash(seed) % uint(65536)) / 65536.0;
			restart_phase += randomness * random * 1.0 / float(total_particles);
		}

		restart_phase *= (1.0 - explosiveness);

		if (system_phase > prev_system_phase) {
			// restart_phase >= prev_system_phase is used so particles emit in the first frame they are processed

			if (restart_phase >= prev_system_phase && restart_phase < system_phase) {
				restart = true;
				if (use_fractional_delta) {
					local_delta = (system_phase - restart_phase) * lifetime;
				}
			}

		} else if (delta > 0.0) {
			if (restart_phase >= prev_system_phase) {
				restart = true;
				if (use_fractional_delta) {
					local_delta = (1.0 - restart_phase + system_phase) * lifetime;
				}

			} else if (restart_phase < system_phase) {
				restart = true;
				if (use_fractional_delta) {
					local_delta = (system_phase - restart_phase) * lifetime;
				}
			}
		}

		if (restart) {
			flags = emitting ? (PARTICLE_FLAG_ACTIVE | PARTICLE_FLAG_STARTED | (cycle << PARTICLE_FRAME_SHIFT)) : 0u;
			restart_position = true;
			restart_rotation_scale = true;
			restart_velocity = true;
			restart_color = true;
			restart_custom = true;
		}
	}

	bool particle_active = bool(flags & PARTICLE_FLAG_ACTIVE);

	uint particle_number = (flags >> PARTICLE_FRAME_SHIFT) * uint(total_particles) + index;

	if (restart && particle_active) {
#CODE : START
	}

	if (particle_active) {
		for (uint i = 0u; i < attractor_count; i++) {
			vec3 dir;
			float amount;
			vec3 rel_vec = xform[3].xyz - attractors[i].transform[3].xyz;
			vec3 local_pos = rel_vec * mat3(attractors[i].transform);

			if (attractors[i].type == ATTRACTOR_TYPE_SPHERE) {
				dir = safe_normalize(rel_vec);
				float d = length(local_pos) / attractors[i].extents.x;
				if (d > 1.0) {
					continue;
				}
				amount = max(0.0, 1.0 - d);
			} else if (attractors[i].type == ATTRACTOR_TYPE_BOX) {
				dir = safe_normalize(rel_vec);

				vec3 abs_pos = abs(local_pos / attractors[i].extents.xyz);
				float d = max(abs_pos.x, max(abs_pos.y, abs_pos.z));
				if (d > 1.0) {
					continue;
				}
				amount = max(0.0, 1.0 - d);
			} else if (attractors[i].type == ATTRACTOR_TYPE_VECTOR_FIELD) {
			}
			mediump float attractor_attenuation = attractors[i].attenuation;
			amount = pow(amount, attractor_attenuation);
			dir = safe_normalize(mix(dir, attractors[i].transform[2].xyz, attractors[i].directionality));
			attractor_force -= amount * dir * attractors[i].strength;
		}

		float particle_size = particle_size;

#ifdef USE_COLLISION_SCALE

		particle_size *= dot(vec3(length(xform[0].xyz), length(xform[1].xyz), length(xform[2].xyz)), vec3(0.33333333333));

#endif

		if (collider_count == 1u && colliders[0].type == COLLIDER_TYPE_2D_SDF) {
			//2D collision

			vec2 pos = xform[3].xy;
			vec4 to_sdf_x = colliders[0].transform[0];
			vec4 to_sdf_y = colliders[0].transform[1];
			vec2 sdf_pos = vec2(dot(vec4(pos, 0, 1), to_sdf_x), dot(vec4(pos, 0, 1), to_sdf_y));

			vec4 sdf_to_screen = vec4(colliders[0].extents.xyz, colliders[0].scale);

			vec2 uv_pos = sdf_pos * sdf_to_screen.xy + sdf_to_screen.zw;

			if (all(greaterThan(uv_pos, vec2(0.0))) && all(lessThan(uv_pos, vec2(1.0)))) {
				vec2 pos2 = pos + vec2(0, particle_size);
				vec2 sdf_pos2 = vec2(dot(vec4(pos2, 0, 1), to_sdf_x), dot(vec4(pos2, 0, 1), to_sdf_y));
				float sdf_particle_size = distance(sdf_pos, sdf_pos2);

				float d = vec4_to_float(texture(height_field_texture, uv_pos)) * SDF_MAX_LENGTH;

				// Allowing for a small epsilon to allow particle just touching colliders to count as collided
				const float EPSILON = 0.001;
				d -= sdf_particle_size;
				if (d < EPSILON) {
					vec2 n = normalize(vec2(
							vec4_to_float(texture(height_field_texture, uv_pos + vec2(EPSILON, 0.0))) - vec4_to_float(texture(height_field_texture, uv_pos - vec2(EPSILON, 0.0))),
							vec4_to_float(texture(height_field_texture, uv_pos + vec2(0.0, EPSILON))) - vec4_to_float(texture(height_field_texture, uv_pos - vec2(0.0, EPSILON)))));

					collided = true;
					sdf_pos2 = sdf_pos + n * d;
					pos2 = vec2(dot(vec4(sdf_pos2, 0, 1), colliders[0].transform[2]), dot(vec4(sdf_pos2, 0, 1), colliders[0].transform[3]));

					n = pos - pos2;

					collision_normal = normalize(vec3(n, 0.0));
					collision_depth = length(n);
				}
			}

		} else {
			for (uint i = 0u; i < collider_count; i++) {
				vec3 normal;
				float depth;
				bool col = false;

				vec3 rel_vec = xform[3].xyz - colliders[i].transform[3].xyz;
				vec3 local_pos = rel_vec * mat3(colliders[i].transform);

				// Allowing for a small epsilon to allow particle just touching colliders to count as collided
				const float EPSILON = 0.001;
				if (colliders[i].type == COLLIDER_TYPE_SPHERE) {
					float d = length(rel_vec) - (particle_size + colliders[i].extents.x);

					if (d < EPSILON) {
						col = true;
						depth = -d;
						normal = normalize(rel_vec);
					}
				} else if (colliders[i].type == COLLIDER_TYPE_BOX) {
					vec3 abs_pos = abs(local_pos);
					vec3 sgn_pos = sign(local_pos);

					if (any(greaterThan(abs_pos, colliders[i].extents.xyz))) {
						//point outside box

						vec3 closest = min(abs_pos, colliders[i].extents.xyz);
						vec3 rel = abs_pos - closest;
						depth = length(rel) - particle_size;
						if (depth < EPSILON) {
							col = true;
							normal = mat3(colliders[i].transform) * (normalize(rel) * sgn_pos);
							depth = -depth;
						}
					} else {
						//point inside box
						vec3 axis_len = colliders[i].extents.xyz - abs_pos;
						// there has to be a faster way to do this?
						if (all(lessThan(axis_len.xx, axis_len.yz))) {
							normal = vec3(1, 0, 0);
						} else if (all(lessThan(axis_len.yy, axis_len.xz))) {
							normal = vec3(0, 1, 0);
						} else {
							normal = vec3(0, 0, 1);
						}

						col = true;
						depth = dot(normal * axis_len, vec3(1)) + particle_size;
						normal = mat3(colliders[i].transform) * (normal * sgn_pos);
					}
				} else if (colliders[i].type == COLLIDER_TYPE_SDF) {
				} else if (colliders[i].type == COLLIDER_TYPE_HEIGHT_FIELD) {
					vec3 local_pos_bottom = local_pos;
					local_pos_bottom.y -= particle_size;

					if (any(greaterThan(abs(local_pos_bottom), colliders[i].extents.xyz))) {
						continue;
					}
					const float DELTA = 1.0 / 8192.0;

					vec3 uvw_pos = vec3(local_pos_bottom / colliders[i].extents.xyz) * 0.5 + 0.5;

					float y = texture(height_field_texture, uvw_pos.xz).r;

					if (y + EPSILON > uvw_pos.y) {
						//inside heightfield

						vec3 pos1 = (vec3(uvw_pos.x, y, uvw_pos.z) * 2.0 - 1.0) * colliders[i].extents.xyz;
						vec3 pos2 = (vec3(uvw_pos.x + DELTA, texture(height_field_texture, uvw_pos.xz + vec2(DELTA, 0)).r, uvw_pos.z) * 2.0 - 1.0) * colliders[i].extents.xyz;
						vec3 pos3 = (vec3(uvw_pos.x, texture(height_field_texture, uvw_pos.xz + vec2(0, DELTA)).r, uvw_pos.z + DELTA) * 2.0 - 1.0) * colliders[i].extents.xyz;

						normal = normalize(cross(pos1 - pos2, pos1 - pos3));
						float local_y = (vec3(local_pos / colliders[i].extents.xyz) * 0.5 + 0.5).y;

						col = true;
						depth = dot(normal, pos1) - dot(normal, local_pos_bottom);
					}
				}

				if (col) {
					if (!collided) {
						collided = true;
						collision_normal = normal;
						collision_depth = depth;
					} else {
						vec3 c = collision_normal * collision_depth;
						c += normal * max(0.0, depth - dot(normal, c));
						collision_normal = normalize(c);
						collision_depth = length(c);
					}
				}
			}
		}
	}

	if (particle_active) {
#CODE : PROCESS
	}

	flags &= ~PARTICLE_FLAG_ACTIVE;
	if (particle_active) {
		flags |= PARTICLE_FLAG_ACTIVE;
	}

	xform = transpose(xform);
	out_xform_1 = xform[0];
	out_xform_2 = xform[1];
#ifdef MODE_3D
	out_xform_3 = xform[2];
#endif
	out_velocity_flags.w = uintBitsToFloat(flags);
}

/* clang-format off */
#[fragment]

void main() {
}
/* clang-format on */
