/* clang-format off */
[vertex]

#if defined(IS_UBERSHADER)
uniform highp int ubershader_flags;
#endif

layout(location = 0) in highp vec4 color;
/* clang-format on */
layout(location = 1) in highp vec4 velocity_active;
layout(location = 2) in highp vec4 custom;
layout(location = 3) in highp vec4 xform_1;
layout(location = 4) in highp vec4 xform_2;
layout(location = 5) in highp vec4 xform_3;

struct Attractor {
	vec3 pos;
	vec3 dir;
	float radius;
	float eat_radius;
	float strength;
	float attenuation;
};

#define MAX_ATTRACTORS 64

uniform bool emitting;
uniform float system_phase;
uniform float prev_system_phase;
uniform int total_particles;
uniform float explosiveness;
uniform float randomness;
uniform float time;
uniform float delta;

uniform int attractor_count;
uniform Attractor attractors[MAX_ATTRACTORS];
uniform bool clear;
uniform uint cycle;
uniform float lifetime;
uniform bool lifetime_infinite;
uniform mat4 emission_transform;
uniform uint random_seed;

out highp vec4 out_color; //tfb:
out highp vec4 out_velocity_active; //tfb:
out highp vec4 out_custom; //tfb:
out highp vec4 out_xform_1; //tfb:
out highp vec4 out_xform_2; //tfb:
out highp vec4 out_xform_3; //tfb:

#if defined(USE_MATERIAL)

/* clang-format off */
layout(std140) uniform UniformData { //ubo:0

MATERIAL_UNIFORMS

};
/* clang-format on */

#endif

/* clang-format off */

VERTEX_SHADER_GLOBALS

/* clang-format on */

uint hash(uint x) {
	x = ((x >> uint(16)) ^ x) * uint(0x45d9f3b);
	x = ((x >> uint(16)) ^ x) * uint(0x45d9f3b);
	x = (x >> uint(16)) ^ x;
	return x;
}

void main() {
	bool apply_forces = true;
	bool apply_velocity = true;
	float local_delta = delta;

	float mass = 1.0;

	float restart_phase = float(gl_VertexID) / float(total_particles);

	if (randomness > 0.0) {
		uint seed = cycle;
		if (restart_phase >= system_phase) {
			seed -= uint(1);
		}
		seed *= uint(total_particles);
		seed += uint(gl_VertexID);
		float random = float(hash(seed) % uint(65536)) / 65536.0;
		restart_phase += randomness * random * 1.0 / float(total_particles);
	}

	restart_phase *= (1.0 - explosiveness);
	bool restart = false;
	bool shader_active = velocity_active.a > 0.5;

	if (system_phase > prev_system_phase) {
		// restart_phase >= prev_system_phase is used so particles emit in the first frame they are processed

		if (restart_phase >= prev_system_phase && restart_phase < system_phase) {
			restart = true;
#ifdef USE_FRACTIONAL_DELTA //ubershader-runtime
			if (!lifetime_infinite) {
				local_delta = (system_phase - restart_phase) * lifetime;
			}
#endif //ubershader-runtime
		}

	} else if (delta > 0.0) {
		if (restart_phase >= prev_system_phase) {
			restart = true;
#ifdef USE_FRACTIONAL_DELTA //ubershader-runtime
			if (!lifetime_infinite) {
				local_delta = (1.0 - restart_phase + system_phase) * lifetime;
			}
#endif //ubershader-runtime
		} else if (restart_phase < system_phase) {
			restart = true;
#ifdef USE_FRACTIONAL_DELTA //ubershader-runtime
			if (!lifetime_infinite) {
				local_delta = (system_phase - restart_phase) * lifetime;
			}
#endif //ubershader-runtime
		}
	}

	uint current_cycle = cycle;

	if (system_phase < restart_phase) {
		current_cycle -= uint(1);
	}

	uint particle_number = current_cycle * uint(total_particles) + uint(gl_VertexID);
	int index = int(gl_VertexID);

	if (restart) {
		shader_active = emitting;
	}

	mat4 xform;

#if defined(ENABLE_KEEP_DATA)
	if (clear) {
#else
	if (clear || restart) {
#endif
		out_color = vec4(1.0);
		out_velocity_active = vec4(0.0);
		out_custom = vec4(0.0);
		if (!restart)
			shader_active = false;

		xform = mat4(
				vec4(1.0, 0.0, 0.0, 0.0),
				vec4(0.0, 1.0, 0.0, 0.0),
				vec4(0.0, 0.0, 1.0, 0.0),
				vec4(0.0, 0.0, 0.0, 1.0));
	} else {
		out_color = color;
		out_velocity_active = velocity_active;
		out_custom = custom;
		xform = transpose(mat4(xform_1, xform_2, xform_3, vec4(vec3(0.0), 1.0)));
	}

	if (shader_active) {
		//execute shader

		{
			/* clang-format off */

VERTEX_SHADER_CODE

			/* clang-format on */
		}

#if !defined(DISABLE_FORCE)

		if (false) {
			vec3 force = vec3(0.0);
			for (int i = 0; i < attractor_count; i++) {
				vec3 rel_vec = xform[3].xyz - attractors[i].pos;
				float dist = length(rel_vec);
				if (attractors[i].radius < dist)
					continue;
				if (attractors[i].eat_radius > 0.0 && attractors[i].eat_radius > dist) {
					out_velocity_active.a = 0.0;
				}

				rel_vec = normalize(rel_vec);

				float attenuation = pow(dist / attractors[i].radius, attractors[i].attenuation);

				if (attractors[i].dir == vec3(0.0)) {
					//towards center
					force += attractors[i].strength * rel_vec * attenuation * mass;
				} else {
					force += attractors[i].strength * attractors[i].dir * attenuation * mass;
				}
			}

			out_velocity_active.xyz += force * local_delta;
		}
#endif

#if !defined(DISABLE_VELOCITY)

		xform[3].xyz += out_velocity_active.xyz * local_delta;
#endif
	} else {
		xform = mat4(0.0);
	}

	xform = transpose(xform);

	out_velocity_active.a = float(shader_active);

	out_xform_1 = xform[0];
	out_xform_2 = xform[1];
	out_xform_3 = xform[2];
}

/* clang-format off */
[fragment]

#if defined(IS_UBERSHADER)
uniform highp int ubershader_flags;
#endif

// any code here is never executed, stuff is filled just so it works

#if defined(USE_MATERIAL)

layout(std140) uniform UniformData {

MATERIAL_UNIFORMS

};

#endif

FRAGMENT_SHADER_GLOBALS

void main() {

	{

LIGHT_SHADER_CODE

	}

	{

FRAGMENT_SHADER_CODE

	}
}
/* clang-format on */
