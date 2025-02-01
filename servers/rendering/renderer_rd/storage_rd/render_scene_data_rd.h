/**************************************************************************/
/*  render_scene_data_rd.h                                                */
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

#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/storage/render_scene_data.h"

// This is a container for data related to rendering a single frame of a viewport where we load this data into a UBO
// that can be used by the main scene shader but also by various effects.

class RenderSceneDataRD : public RenderSceneData {
	GDCLASS(RenderSceneDataRD, RenderSceneData);

public:
	bool calculate_motion_vectors = false;

	Transform3D cam_transform;
	Projection cam_projection;
	Vector2 taa_jitter;
	float taa_frame_count = 0.0f;
	uint32_t camera_visible_layers;
	bool cam_orthogonal = false;
	bool cam_frustum = false;
	bool flip_y = false;

	// For billboards to cast correct shadows.
	Transform3D main_cam_transform;

	// For stereo rendering
	uint32_t view_count = 1;
	Vector3 view_eye_offset[RendererSceneRender::MAX_RENDER_VIEWS];
	Projection view_projection[RendererSceneRender::MAX_RENDER_VIEWS];

	Transform3D prev_cam_transform;
	Projection prev_cam_projection;
	Vector2 prev_taa_jitter;
	Projection prev_view_projection[RendererSceneRender::MAX_RENDER_VIEWS];

	float z_near = 0.0;
	float z_far = 0.0;

	float lod_distance_multiplier = 0.0;
	float screen_mesh_lod_threshold = 0.0;

	uint32_t directional_light_count = 0;
	float dual_paraboloid_side = 0.0;
	float opaque_prepass_threshold = 0.0;
	bool material_uv2_mode = false;
	float emissive_exposure_normalization = 0.0;

	Size2 shadow_atlas_pixel_size;
	Size2 directional_shadow_pixel_size;

	float time;
	float time_step;

	virtual Transform3D get_cam_transform() const override;
	virtual Projection get_cam_projection() const override;

	virtual uint32_t get_view_count() const override;
	virtual Vector3 get_view_eye_offset(uint32_t p_view) const override;
	virtual Projection get_view_projection(uint32_t p_view) const override;

	RID create_uniform_buffer();
	void update_ubo(RID p_uniform_buffer, RS::ViewportDebugDraw p_debug_mode, RID p_env, RID p_reflection_probe_instance, RID p_camera_attributes, bool p_pancake_shadows, const Size2i &p_screen_size, const Color &p_default_bg_color, float p_luminance_multiplier, bool p_opaque_render_buffers, bool p_apply_alpha_multiplier);
	virtual RID get_uniform_buffer() const override;

private:
	RID uniform_buffer; // loaded into this uniform buffer (supplied externally)

	// This struct is loaded into Set 1 - Binding 0, populated at start of rendering a frame, must match with shader code
	struct UBO {
		float projection_matrix[16];
		float inv_projection_matrix[16];
		float inv_view_matrix[16];
		float view_matrix[16];

		float projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
		float inv_projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
		float eye_offset[RendererSceneRender::MAX_RENDER_VIEWS][4];

		float main_cam_inv_view_matrix[16];

		float viewport_size[2];
		float screen_pixel_size[2];

		float directional_penumbra_shadow_kernel[128]; //32 vec4s
		float directional_soft_shadow_kernel[128];
		float penumbra_shadow_kernel[128];
		float soft_shadow_kernel[128];

		float radiance_inverse_xform[12];

		float ambient_light_color_energy[4];

		float ambient_color_sky_mix;
		uint32_t use_ambient_light;
		uint32_t use_ambient_cubemap;
		uint32_t use_reflection_cubemap;

		float shadow_atlas_pixel_size[2];
		float directional_shadow_pixel_size[2];

		uint32_t directional_light_count;
		float dual_paraboloid_side;
		float z_far;
		float z_near;

		uint32_t roughness_limiter_enabled;
		float roughness_limiter_amount;
		float roughness_limiter_limit;
		float opaque_prepass_threshold;

		// Fog
		uint32_t fog_enabled;
		uint32_t fog_mode;
		float fog_density;
		float fog_height;

		float fog_height_density;
		float fog_depth_curve;
		float fog_depth_begin;
		float taa_frame_count; // Used to add break up samples over multiple frames. Value is an integer from 0 to taa_phase_count -1.

		float fog_light_color[3];
		float fog_depth_end;

		float fog_sun_scatter;
		float fog_aerial_perspective;
		float time;
		float reflection_multiplier;

		float taa_jitter[2];
		uint32_t material_uv2_mode;
		float emissive_exposure_normalization; // Needed to normalize emissive when using physical units.

		float IBL_exposure_normalization; // Adjusts for baked exposure.
		uint32_t pancake_shadows;
		uint32_t camera_visible_layers;
		float pass_alpha_multiplier;
	};

	struct UBODATA {
		UBO ubo;
		UBO prev_ubo;
	};
};
