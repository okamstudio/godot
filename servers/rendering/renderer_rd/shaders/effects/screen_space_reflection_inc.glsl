layout(constant_id = 0) const bool sc_multiview = false;

layout(set = 4, binding = 0, std140) uniform SceneData {
	mat4x4 projection[2]; // With reverse-z and remap-z applied
	mat4x4 inv_projection[2]; // With reverse-z and remap-z applied
	vec4 eye_offset[2];
}
scene_data;

float z_ndc_from_view(vec2 ndc, float view_z) {
	return (ndc.x * (view_z * scene_data.inv_projection[params.view_index][0][3] - scene_data.inv_projection[params.view_index][0][2]) + ndc.y * (view_z * scene_data.inv_projection[params.view_index][1][3] - scene_data.inv_projection[params.view_index][1][2]) + (view_z * scene_data.inv_projection[params.view_index][3][3] - scene_data.inv_projection[params.view_index][3][2])) / (-view_z * scene_data.inv_projection[params.view_index][2][3] + scene_data.inv_projection[params.view_index][2][2]);
}
