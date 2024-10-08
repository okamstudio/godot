/**************************************************************************/
/*  cubemap_filter.cpp                                                    */
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

#include "cubemap_filter.h"

#include "../storage/texture_storage.h"
#include "core/config/project_settings.h"

using namespace GLES3;

CubemapFilter *CubemapFilter::singleton = nullptr;

CubemapFilter::CubemapFilter() {
	singleton = this;
	// Use a factor 4 larger for the compatibility renderer to make up for the fact
	// That we don't use an array texture. We will reduce samples on low roughness
	// to compensate.
	ggx_samples = 4 * uint32_t(GLOBAL_GET("rendering/reflections/sky_reflections/ggx_samples"));

	{
		String defines;
		defines += "\n#define MAX_SAMPLE_COUNT " + itos(ggx_samples) + "\n";
		cubemap_filter.shader.initialize(defines);
		cubemap_filter.shader_version = cubemap_filter.shader.version_create();
	}

	{ // Screen Triangle.
		glGenBuffers(1, &screen_triangle);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);

		const float qv[6] = {
			-1.0f,
			-1.0f,
			-1.0f,
			3.0f,
			3.0f,
			-1.0f,
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, qv, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		glGenVertexArrays(1, &screen_triangle_array);
		glBindVertexArray(screen_triangle_array);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);
		glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(RS::ARRAY_VERTEX);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}
}

CubemapFilter::~CubemapFilter() {
	glDeleteBuffers(1, &screen_triangle);
	glDeleteVertexArrays(1, &screen_triangle_array);

	cubemap_filter.shader.version_free(cubemap_filter.shader_version);
	singleton = nullptr;
}

// Helper functions for IBL filtering

Vector3 importance_sample_GGX(Vector2 xi, float roughness4) {
	// Compute distribution direction
	float phi = 2.0 * Math_PI * xi.x;
	float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (roughness4 - 1.0) * xi.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

	// Convert to spherical direction
	Vector3 half_vector;
	half_vector.x = sin_theta * cos(phi);
	half_vector.y = sin_theta * sin(phi);
	half_vector.z = cos_theta;

	return half_vector;
}

float distribution_GGX(float NdotH, float roughness4) {
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (roughness4 - 1.0) + 1.0);
	denom = Math_PI * denom * denom;

	return roughness4 / denom;
}

float radical_inverse_vdC(uint32_t bits) {
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
	bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
	bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);

	return float(bits) * 2.3283064365386963e-10;
}

Vector2 hammersley(uint32_t i, uint32_t N) {
	return Vector2(float(i) / float(N), radical_inverse_vdC(i));
}

void CubemapFilter::filter_radiance(GLuint p_source_cubemap, GLuint p_dest_cubemap, GLuint p_dest_framebuffer, int p_source_size, int p_mipmap_count, int p_layer) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, p_source_cubemap);
	glBindFramebuffer(GL_FRAMEBUFFER, p_dest_framebuffer);

	CubemapFilterShaderGLES3::ShaderVariant mode = CubemapFilterShaderGLES3::MODE_DEFAULT;

	if (p_layer == 0) {
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		// Copy over base layer without filtering.
		mode = CubemapFilterShaderGLES3::MODE_COPY;
	}

	int size = p_source_size >> p_layer;
	glViewport(0, 0, size, size);
	glBindVertexArray(screen_triangle_array);

	bool success = cubemap_filter.shader.version_bind_shader(cubemap_filter.shader_version, mode);
	if (!success) {
		return;
	}

	if (p_layer > 0) {
		const uint32_t sample_counts[5] = { 1, ggx_samples / 16, ggx_samples / 8, ggx_samples / 4, ggx_samples };
		uint32_t sample_count = sample_counts[MIN(4, p_layer)];

		float roughness = float(p_layer) / (p_mipmap_count - 1);
		roughness *= roughness; // Convert to non-perceptual roughness.
		float roughness4 = roughness * roughness;
		roughness4 *= roughness4;

		float solid_angle_texel = 4.0 * Math_PI / float(6 * size * size);

		LocalVector<float> sample_directions;
		sample_directions.resize(4 * sample_count);

		uint32_t index = 0;
		float weight = 0.0;
		for (uint32_t i = 0; i < sample_count; i++) {
			Vector2 xi = hammersley(i, sample_count);
			Vector3 dir = importance_sample_GGX(xi, roughness4);
			Vector3 light_vec = (2.0 * dir.z * dir - Vector3(0.0, 0.0, 1.0));

			if (light_vec.z <= 0.0) {
				continue;
			}

			sample_directions[index * 4] = light_vec.x;
			sample_directions[index * 4 + 1] = light_vec.y;
			sample_directions[index * 4 + 2] = light_vec.z;

			float D = distribution_GGX(dir.z, roughness4);
			float pdf = D * dir.z / (4.0 * dir.z) + 0.0001;

			float solid_angle_sample = 1.0 / (float(sample_count) * pdf + 0.0001);

			float mip_level = MAX(0.5 * log2(solid_angle_sample / solid_angle_texel) + float(MAX(1, p_layer - 3)), 1.0);

			sample_directions[index * 4 + 3] = mip_level;
			weight += light_vec.z;
			index++;
		}

		glUniform4fv(cubemap_filter.shader.version_get_uniform(CubemapFilterShaderGLES3::SAMPLE_DIRECTIONS_MIP, cubemap_filter.shader_version, mode), sample_count, sample_directions.ptr());
		cubemap_filter.shader.version_set_uniform(CubemapFilterShaderGLES3::WEIGHT, weight, cubemap_filter.shader_version, mode);
		cubemap_filter.shader.version_set_uniform(CubemapFilterShaderGLES3::SAMPLE_COUNT, index, cubemap_filter.shader_version, mode);
	}

	for (int i = 0; i < 6; i++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, p_dest_cubemap, p_layer);
#ifdef DEBUG_ENABLED
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			WARN_PRINT("Could not bind sky radiance face: " + itos(i) + ", status: " + GLES3::TextureStorage::get_singleton()->get_framebuffer_error(status));
		}
#endif
		cubemap_filter.shader.version_set_uniform(CubemapFilterShaderGLES3::FACE_ID, i, cubemap_filter.shader_version, mode);

		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
}

#endif // GLES3_ENABLED
