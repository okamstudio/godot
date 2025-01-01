/**************************************************************************/
/*  render_scene_data_rd.cpp                                              */
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

#include "render_scene_data_rd.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"

Transform3D RenderSceneDataRD::get_cam_transform() const {
	return cam_transform;
}

Projection RenderSceneDataRD::get_cam_projection() const {
	Projection correction;
	correction.set_depth_correction(flip_y);
	correction.add_jitter_offset(taa_jitter);

	return correction * cam_projection;
}

uint32_t RenderSceneDataRD::get_view_count() const {
	return view_count;
}

Vector3 RenderSceneDataRD::get_view_eye_offset(uint32_t p_view) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_view, view_count, Vector3());

	return view_eye_offset[p_view];
}

Projection RenderSceneDataRD::get_view_projection(uint32_t p_view) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_view, view_count, Projection());

	Projection correction;
	correction.set_depth_correction(flip_y);
	correction.add_jitter_offset(taa_jitter);

	return correction * view_projection[p_view];
}

RID RenderSceneDataRD::create_uniform_buffer() {
	return RD::get_singleton()->uniform_buffer_create(sizeof(UBODATA));
}

void RenderSceneDataRD::update_ubo(RID p_uniform_buffer, RS::ViewportDebugDraw p_debug_mode, RID p_env, RID p_reflection_probe_instance, RID p_camera_attributes, bool p_pancake_shadows, const Size2i &p_screen_size, const Color &p_default_bg_color, float p_luminance_multiplier, bool p_opaque_render_buffers, bool p_apply_alpha_multiplier) {
	RendererSceneRenderRD *render_scene_render = RendererSceneRenderRD::get_singleton();

	UBODATA ubo_data;
	memset(&ubo_data, 0, sizeof(UBODATA));

	// just for easy access..
	UBO &ubo = ubo_data.ubo;
	UBO &prev_ubo = ubo_data.prev_ubo;

	Projection correction;
	correction.set_depth_correction(flip_y);
	correction.add_jitter_offset(taa_jitter);
	Projection projection = correction * cam_projection;

	//store camera into ubo
	RendererRD::MaterialStorage::store_camera(projection, ubo.projection_matrix);
	RendererRD::MaterialStorage::store_camera(projection.inverse(), ubo.inv_projection_matrix);
	RendererRD::MaterialStorage::store_transform(cam_transform, ubo.inv_view_matrix);
	RendererRD::MaterialStorage::store_transform(cam_transform.affine_inverse(), ubo.view_matrix);

#ifdef REAL_T_IS_DOUBLE
	RendererRD::MaterialStorage::split_double(-cam_transform.origin.x, &ubo.inv_view_matrix[12], &ubo.inv_view_matrix[3]);
	RendererRD::MaterialStorage::split_double(-cam_transform.origin.y, &ubo.inv_view_matrix[13], &ubo.inv_view_matrix[7]);
	RendererRD::MaterialStorage::split_double(-cam_transform.origin.z, &ubo.inv_view_matrix[14], &ubo.inv_view_matrix[11]);
#endif

	for (uint32_t v = 0; v < view_count; v++) {
		projection = correction * view_projection[v];
		RendererRD::MaterialStorage::store_camera(projection, ubo.projection_matrix_view[v]);
		RendererRD::MaterialStorage::store_camera(projection.inverse(), ubo.inv_projection_matrix_view[v]);

		ubo.eye_offset[v][0] = view_eye_offset[v].x;
		ubo.eye_offset[v][1] = view_eye_offset[v].y;
		ubo.eye_offset[v][2] = view_eye_offset[v].z;
		ubo.eye_offset[v][3] = 0.0;
	}

	RendererRD::MaterialStorage::store_transform(main_cam_transform, ubo.main_cam_inv_view_matrix);

	ubo.taa_jitter[0] = taa_jitter.x;
	ubo.taa_jitter[1] = taa_jitter.y;
	ubo.taa_frame_count = taa_frame_count;

	ubo.z_far = z_far;
	ubo.z_near = z_near;

	ubo.pancake_shadows = p_pancake_shadows;

	RendererRD::MaterialStorage::store_soft_shadow_kernel(render_scene_render->directional_penumbra_shadow_kernel_get(), ubo.directional_penumbra_shadow_kernel);
	RendererRD::MaterialStorage::store_soft_shadow_kernel(render_scene_render->directional_soft_shadow_kernel_get(), ubo.directional_soft_shadow_kernel);
	RendererRD::MaterialStorage::store_soft_shadow_kernel(render_scene_render->penumbra_shadow_kernel_get(), ubo.penumbra_shadow_kernel);
	RendererRD::MaterialStorage::store_soft_shadow_kernel(render_scene_render->soft_shadow_kernel_get(), ubo.soft_shadow_kernel);
	ubo.camera_visible_layers = camera_visible_layers;
	ubo.pass_alpha_multiplier = p_opaque_render_buffers && p_apply_alpha_multiplier ? 0.0f : 1.0f;

	ubo.viewport_size[0] = p_screen_size.x;
	ubo.viewport_size[1] = p_screen_size.y;

	Size2 screen_pixel_size = Vector2(1.0, 1.0) / Size2(p_screen_size);
	ubo.screen_pixel_size[0] = screen_pixel_size.x;
	ubo.screen_pixel_size[1] = screen_pixel_size.y;

	ubo.shadow_atlas_pixel_size[0] = shadow_atlas_pixel_size.x;
	ubo.shadow_atlas_pixel_size[1] = shadow_atlas_pixel_size.y;

	ubo.directional_shadow_pixel_size[0] = directional_shadow_pixel_size.x;
	ubo.directional_shadow_pixel_size[1] = directional_shadow_pixel_size.y;

	ubo.time = time;

	ubo.directional_light_count = directional_light_count;
	ubo.dual_paraboloid_side = dual_paraboloid_side;
	ubo.opaque_prepass_threshold = opaque_prepass_threshold;
	ubo.material_uv2_mode = material_uv2_mode;

	ubo.fog_enabled = false;

	if (p_debug_mode == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
		ubo.use_ambient_light = true;
		ubo.ambient_light_color_energy[0] = 1;
		ubo.ambient_light_color_energy[1] = 1;
		ubo.ambient_light_color_energy[2] = 1;
		ubo.ambient_light_color_energy[3] = 1.0;
		ubo.use_ambient_cubemap = false;
		ubo.use_reflection_cubemap = false;
	} else if (p_env.is_valid()) {
		RS::EnvironmentBG env_bg = render_scene_render->environment_get_background(p_env);
		RS::EnvironmentAmbientSource ambient_src = render_scene_render->environment_get_ambient_source(p_env);

		float bg_energy_multiplier = render_scene_render->environment_get_bg_energy_multiplier(p_env);

		ubo.ambient_light_color_energy[3] = bg_energy_multiplier;

		ubo.ambient_color_sky_mix = render_scene_render->environment_get_ambient_sky_contribution(p_env);

		//ambient
		if (ambient_src == RS::ENV_AMBIENT_SOURCE_BG && (env_bg == RS::ENV_BG_CLEAR_COLOR || env_bg == RS::ENV_BG_COLOR)) {
			Color color = env_bg == RS::ENV_BG_CLEAR_COLOR ? p_default_bg_color : render_scene_render->environment_get_bg_color(p_env);
			color = color.srgb_to_linear();

			ubo.ambient_light_color_energy[0] = color.r * bg_energy_multiplier;
			ubo.ambient_light_color_energy[1] = color.g * bg_energy_multiplier;
			ubo.ambient_light_color_energy[2] = color.b * bg_energy_multiplier;
			ubo.use_ambient_light = true;
			ubo.use_ambient_cubemap = false;
		} else {
			float energy = render_scene_render->environment_get_ambient_light_energy(p_env);
			Color color = render_scene_render->environment_get_ambient_light(p_env);
			color = color.srgb_to_linear();
			ubo.ambient_light_color_energy[0] = color.r * energy;
			ubo.ambient_light_color_energy[1] = color.g * energy;
			ubo.ambient_light_color_energy[2] = color.b * energy;

			Basis sky_transform = render_scene_render->environment_get_sky_orientation(p_env);
			sky_transform = sky_transform.inverse() * cam_transform.basis;
			RendererRD::MaterialStorage::store_transform_3x3(sky_transform, ubo.radiance_inverse_xform);

			ubo.use_ambient_cubemap = (ambient_src == RS::ENV_AMBIENT_SOURCE_BG && env_bg == RS::ENV_BG_SKY) || ambient_src == RS::ENV_AMBIENT_SOURCE_SKY;
			ubo.use_ambient_light = ubo.use_ambient_cubemap || ambient_src == RS::ENV_AMBIENT_SOURCE_COLOR;
		}

		//specular
		RS::EnvironmentReflectionSource ref_src = render_scene_render->environment_get_reflection_source(p_env);
		if ((ref_src == RS::ENV_REFLECTION_SOURCE_BG && env_bg == RS::ENV_BG_SKY) || ref_src == RS::ENV_REFLECTION_SOURCE_SKY) {
			ubo.use_reflection_cubemap = true;
		} else {
			ubo.use_reflection_cubemap = false;
		}

		ubo.fog_enabled = render_scene_render->environment_get_fog_enabled(p_env);
		ubo.fog_mode = render_scene_render->environment_get_fog_mode(p_env);
		ubo.fog_density = render_scene_render->environment_get_fog_density(p_env);
		ubo.fog_height = render_scene_render->environment_get_fog_height(p_env);
		ubo.fog_height_density = render_scene_render->environment_get_fog_height_density(p_env);
		ubo.fog_aerial_perspective = render_scene_render->environment_get_fog_aerial_perspective(p_env);

		ubo.fog_depth_curve = render_scene_render->environment_get_fog_depth_curve(p_env);
		ubo.fog_depth_end = render_scene_render->environment_get_fog_depth_end(p_env) > 0.0 ? render_scene_render->environment_get_fog_depth_end(p_env) : ubo.z_far;
		ubo.fog_depth_begin = MIN(render_scene_render->environment_get_fog_depth_begin(p_env), ubo.fog_depth_end - 0.001);

		Color fog_color = render_scene_render->environment_get_fog_light_color(p_env).srgb_to_linear();
		float fog_energy = render_scene_render->environment_get_fog_light_energy(p_env);

		ubo.fog_light_color[0] = fog_color.r * fog_energy;
		ubo.fog_light_color[1] = fog_color.g * fog_energy;
		ubo.fog_light_color[2] = fog_color.b * fog_energy;

		ubo.fog_sun_scatter = render_scene_render->environment_get_fog_sun_scatter(p_env);
	} else {
		if (p_reflection_probe_instance.is_valid() && RendererRD::LightStorage::get_singleton()->reflection_probe_is_interior(p_reflection_probe_instance)) {
			ubo.use_ambient_light = false;
		} else {
			ubo.use_ambient_light = true;
			Color clear_color = p_default_bg_color;
			clear_color = clear_color.srgb_to_linear();
			ubo.ambient_light_color_energy[0] = clear_color.r;
			ubo.ambient_light_color_energy[1] = clear_color.g;
			ubo.ambient_light_color_energy[2] = clear_color.b;
			ubo.ambient_light_color_energy[3] = 1.0;
		}

		ubo.use_ambient_cubemap = false;
		ubo.use_reflection_cubemap = false;
	}

	if (p_camera_attributes.is_valid()) {
		ubo.emissive_exposure_normalization = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_camera_attributes);
		ubo.IBL_exposure_normalization = 1.0;
		if (p_env.is_valid()) {
			RID sky_rid = render_scene_render->environment_get_sky(p_env);
			if (sky_rid.is_valid()) {
				float current_exposure = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_camera_attributes) * render_scene_render->environment_get_bg_intensity(p_env) / p_luminance_multiplier;
				ubo.IBL_exposure_normalization = current_exposure / MAX(0.001, render_scene_render->get_sky()->sky_get_baked_exposure(sky_rid));
			}
		}
	} else if (emissive_exposure_normalization > 0.0) {
		// This branch is triggered when using render_material().
		// Emissive is set outside the function.
		ubo.emissive_exposure_normalization = emissive_exposure_normalization;
		// IBL isn't used don't set it.
	} else {
		ubo.emissive_exposure_normalization = 1.0;
		ubo.IBL_exposure_normalization = 1.0;
	}

	ubo.roughness_limiter_enabled = p_opaque_render_buffers && render_scene_render->screen_space_roughness_limiter_is_active();
	ubo.roughness_limiter_amount = render_scene_render->screen_space_roughness_limiter_get_amount();
	ubo.roughness_limiter_limit = render_scene_render->screen_space_roughness_limiter_get_limit();

	if (calculate_motion_vectors) {
		// Q : Should we make a complete copy or should we define a separate UBO with just the components we need?
		memcpy(&prev_ubo, &ubo, sizeof(UBO));

		Projection prev_correction;
		prev_correction.set_depth_correction(true);
		prev_correction.add_jitter_offset(prev_taa_jitter);
		Projection prev_projection = prev_correction * prev_cam_projection;

		//store camera into ubo
		RendererRD::MaterialStorage::store_camera(prev_projection, prev_ubo.projection_matrix);
		RendererRD::MaterialStorage::store_camera(prev_projection.inverse(), prev_ubo.inv_projection_matrix);
		RendererRD::MaterialStorage::store_transform(prev_cam_transform, prev_ubo.inv_view_matrix);
		RendererRD::MaterialStorage::store_transform(prev_cam_transform.affine_inverse(), prev_ubo.view_matrix);

#ifdef REAL_T_IS_DOUBLE
		RendererRD::MaterialStorage::split_double(-prev_cam_transform.origin.x, &prev_ubo.inv_view_matrix[12], &prev_ubo.inv_view_matrix[3]);
		RendererRD::MaterialStorage::split_double(-prev_cam_transform.origin.y, &prev_ubo.inv_view_matrix[13], &prev_ubo.inv_view_matrix[7]);
		RendererRD::MaterialStorage::split_double(-prev_cam_transform.origin.z, &prev_ubo.inv_view_matrix[14], &prev_ubo.inv_view_matrix[11]);
#endif

		for (uint32_t v = 0; v < view_count; v++) {
			prev_projection = prev_correction * view_projection[v];
			RendererRD::MaterialStorage::store_camera(prev_projection, prev_ubo.projection_matrix_view[v]);
			RendererRD::MaterialStorage::store_camera(prev_projection.inverse(), prev_ubo.inv_projection_matrix_view[v]);
		}
		prev_ubo.taa_jitter[0] = prev_taa_jitter.x;
		prev_ubo.taa_jitter[1] = prev_taa_jitter.y;
		prev_ubo.time -= time_step;
	}

	uniform_buffer = p_uniform_buffer;
	RD::get_singleton()->buffer_update(uniform_buffer, 0, sizeof(UBODATA), &ubo);
}

RID RenderSceneDataRD::get_uniform_buffer() const {
	return uniform_buffer;
}
