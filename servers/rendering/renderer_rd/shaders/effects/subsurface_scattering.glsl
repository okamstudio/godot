#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#ifdef USE_25_SAMPLES
const int kernel_size = 13;

const vec2 kernel[kernel_size] = vec2[](
		vec2(0.530605, 0.0),
		vec2(0.0211412, 0.0208333),
		vec2(0.0402784, 0.0833333),
		vec2(0.0493588, 0.1875),
		vec2(0.0410172, 0.333333),
		vec2(0.0263642, 0.520833),
		vec2(0.017924, 0.75),
		vec2(0.0128496, 1.02083),
		vec2(0.0094389, 1.33333),
		vec2(0.00700976, 1.6875),
		vec2(0.00500364, 2.08333),
		vec2(0.00333804, 2.52083),
		vec2(0.000973794, 3.0));

const vec4 skin_kernel[kernel_size] = vec4[](
		vec4(0.530605, 0.613514, 0.739601, 0),
		vec4(0.0211412, 0.0459286, 0.0378196, 0.0208333),
		vec4(0.0402784, 0.0657244, 0.04631, 0.0833333),
		vec4(0.0493588, 0.0367726, 0.0219485, 0.1875),
		vec4(0.0410172, 0.0199899, 0.0118481, 0.333333),
		vec4(0.0263642, 0.0119715, 0.00684598, 0.520833),
		vec4(0.017924, 0.00711691, 0.00347194, 0.75),
		vec4(0.0128496, 0.00356329, 0.00132016, 1.02083),
		vec4(0.0094389, 0.00139119, 0.000416598, 1.33333),
		vec4(0.00700976, 0.00049366, 0.000151938, 1.6875),
		vec4(0.00500364, 0.00020094, 5.28848e-005, 2.08333),
		vec4(0.00333804, 7.85443e-005, 1.2945e-005, 2.52083),
		vec4(0.000973794, 1.11862e-005, 9.43437e-007, 3));

#endif //USE_25_SAMPLES

#ifdef USE_17_SAMPLES
const int kernel_size = 9;
const vec2 kernel[kernel_size] = vec2[](
		vec2(0.536343, 0.0),
		vec2(0.0324462, 0.03125),
		vec2(0.0582416, 0.125),
		vec2(0.0571056, 0.28125),
		vec2(0.0347317, 0.5),
		vec2(0.0216301, 0.78125),
		vec2(0.0144609, 1.125),
		vec2(0.0100386, 1.53125),
		vec2(0.00317394, 2.0));

const vec4 skin_kernel[kernel_size] = vec4[](
		vec4(0.536343, 0.624624, 0.748867, 0),
		vec4(0.0324462, 0.0656718, 0.0532821, 0.03125),
		vec4(0.0582416, 0.0659959, 0.0411329, 0.125),
		vec4(0.0571056, 0.0287432, 0.0172844, 0.28125),
		vec4(0.0347317, 0.0151085, 0.00871983, 0.5),
		vec4(0.0216301, 0.00794618, 0.00376991, 0.78125),
		vec4(0.0144609, 0.00317269, 0.00106399, 1.125),
		vec4(0.0100386, 0.000914679, 0.000275702, 1.53125),
		vec4(0.00317394, 0.000134823, 3.77269e-005, 2));
#endif //USE_17_SAMPLES

#ifdef USE_11_SAMPLES
const int kernel_size = 6;
const vec2 kernel[kernel_size] = vec2[](
		vec2(0.560479, 0.0),
		vec2(0.0771802, 0.08),
		vec2(0.0821904, 0.32),
		vec2(0.03639, 0.72),
		vec2(0.0192831, 1.28),
		vec2(0.00471691, 2.0));

const vec4 skin_kernel[kernel_size] = vec4[](

		vec4(0.560479, 0.669086, 0.784728, 0),
		vec4(0.0771802, 0.113491, 0.0793803, 0.08),
		vec4(0.0821904, 0.0358608, 0.0209261, 0.32),
		vec4(0.03639, 0.0130999, 0.00643685, 0.72),
		vec4(0.0192831, 0.00282018, 0.00084214, 1.28),
		vec4(0.00471691, 0.000184771, 5.07565e-005, 2));

#endif //USE_11_SAMPLES

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	float camera_z_far;
	float camera_z_near;

	bool vertical;
	bool orthogonal;
	float unit_size;
	float scale;

	float depth_scale;
	uint pad[3];
}
params;

layout(set = 0, binding = 0) uniform sampler2D source_image;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D dest_image;
layout(set = 2, binding = 0) uniform sampler2D source_depth;

void do_filter(inout vec3 color_accum, inout vec3 divisor, vec2 uv, vec2 step, bool p_skin) {
	// Accumulate the other samples:
	for (int i = 1; i < kernel_size; i++) {
		// Fetch color and depth for current sample:
		vec2 offset = uv + kernel[i].y * step;
		vec4 color = texture(source_image, offset);

		if (abs(color.a) < 0.001) {
			break; //mix no more
		}

		vec3 w;
		if (p_skin) {
			//skin
			w = skin_kernel[i].rgb;
		} else {
			w = vec3(kernel[i].x);
		}

		color_accum += color.rgb * w;
		divisor += w;
	}
}

void main() {
	// Pixel being shaded
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThanEqual(ssC, params.screen_size))) { //too large, do nothing
		return;
	}

	vec2 uv = (vec2(ssC) + 0.5) / vec2(params.screen_size);

	// Fetch color of current pixel:
	vec4 base_color = texture(source_image, uv);
	float strength = abs(base_color.a);

	if (strength > 0.0) {
		vec2 dir = params.vertical ? vec2(0.0, 1.0) : vec2(1.0, 0.0);

		// Fetch linear depth of current pixel:
		float depth = texture(source_depth, uv).r * 2.0 - 1.0;
		float depth_scale;

		if (params.orthogonal) {
			depth = -(depth * (params.camera_z_far - params.camera_z_near) - (params.camera_z_far + params.camera_z_near)) / 2.0;
			depth_scale = params.unit_size; //remember depth is negative by default in OpenGL
		} else {
			depth = 2.0 * params.camera_z_near * params.camera_z_far / (params.camera_z_far + params.camera_z_near + depth * (params.camera_z_far - params.camera_z_near));
			depth_scale = params.unit_size / depth; //remember depth is negative by default in OpenGL
		}

		float scale = mix(params.scale, depth_scale, params.depth_scale);

		// Calculate the final step to fetch the surrounding pixels:
		vec2 step = scale * dir;
		step *= strength;
		step /= 3.0;
		// Accumulate the center sample:

		vec3 divisor;
		bool skin = bool(base_color.a < 0.0);

		if (skin) {
			//skin
			divisor = skin_kernel[0].rgb;
		} else {
			divisor = vec3(kernel[0].x);
		}

		vec3 color = base_color.rgb * divisor;

		do_filter(color, divisor, uv, step, skin);
		do_filter(color, divisor, uv, -step, skin);

		base_color.rgb = color / divisor;
	}

	imageStore(dest_image, ssC, base_color);
}
