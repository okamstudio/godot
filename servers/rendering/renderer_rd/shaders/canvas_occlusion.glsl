#[vertex]

#version 450

#VERSION_DEFINES

layout(location = 0) in highp vec3 vertex;

#ifdef POSITIONAL_SHADOW
layout(push_constant, std430) uniform Constants {
	mat2x4 modelview;
	vec4 rotation;
	vec2 direction;
	float z_far;
	uint pad;
	float z_near;
	uint cull_mode;
	float pad3;
	float pad4;
}
constants;

layout(set = 0, binding = 0, std430) restrict readonly buffer OccluderTransforms {
	mat2x4 transforms[];
}
occluder_transforms;

#else

layout(push_constant, std430) uniform Constants {
	mat4 projection;
	mat2x4 modelview;
	vec2 direction;
	float z_far;
	uint cull_mode;
}
constants;

#endif

#ifdef MODE_SHADOW
layout(location = 0) out highp float depth;
#endif

void main() {
#ifdef POSITIONAL_SHADOW
	float c = -(constants.z_far + constants.z_near) / (constants.z_far - constants.z_near);
	float d = -2.0 * constants.z_far * constants.z_near / (constants.z_far - constants.z_near);

	mat4 projection = mat4(vec4(1.0, 0.0, 0.0, 0.0),
			vec4(0.0, 1.0, 0.0, 0.0),
			vec4(0.0, 0.0, c, -1.0),
			vec4(0.0, 0.0, d, 0.0));

	// Precomputed:
	// Vector3 cam_target = Basis::from_euler(Vector3(0, 0, Math_TAU * ((i + 3) / 4.0))).xform(Vector3(0, 1, 0));
	// projection = projection * Projection(Transform3D().looking_at(cam_targets[i], Vector3(0, 0, -1)).affine_inverse());
	projection *= mat4(vec4(constants.rotation.x, 0.0, constants.rotation.y, 0.0), vec4(constants.rotation.z, 0.0, constants.rotation.w, 0.0), vec4(0.0, -1.0, 0.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
	mat4 modelview = mat4(occluder_transforms.transforms[constants.pad]) * mat4(constants.modelview);
#else
	mat4 projection = constants.projection;
	mat4 modelview = mat4(constants.modelview[0], constants.modelview[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
#endif

	highp vec4 vtx = vec4(vertex, 1.0) * modelview;

#ifdef MODE_SHADOW
	depth = dot(constants.direction, vtx.xy);
#endif

	gl_Position = projection * vtx;
}

#[fragment]

#version 450

#VERSION_DEFINES

#ifdef POSITIONAL_SHADOW
layout(push_constant, std430) uniform Constants {
	mat2x4 modelview;
	vec4 rotation;
	vec2 direction;
	float z_far;
	uint pad;
	float z_near;
	uint cull_mode;
	float pad3;
	float pad4;
}
constants;

#else

layout(push_constant, std430) uniform Constants {
	mat4 projection;
	mat2x4 modelview;
	vec2 direction;
	float z_far;
	uint cull_mode;
}
constants;

#endif

#ifdef MODE_SHADOW
layout(location = 0) in highp float depth;
layout(location = 0) out highp float distance_buf;
#else
layout(location = 0) out highp float sdf_buf;
#endif

#define POLYGON_CULL_DISABLED 0
#define POLYGON_CULL_FRONT 1
#define POLYGON_CULL_BACK 2

void main() {
#ifdef MODE_SHADOW
	bool front_facing = gl_FrontFacing;
	if (constants.cull_mode == POLYGON_CULL_BACK && !front_facing) {
		discard;
	} else if (constants.cull_mode == POLYGON_CULL_FRONT && front_facing) {
		discard;
	}
	distance_buf = depth / constants.z_far;
#else
	sdf_buf = 1.0;
#endif
}
