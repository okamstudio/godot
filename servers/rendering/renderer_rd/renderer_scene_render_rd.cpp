/**************************************************************************/
/*  renderer_scene_render_rd.cpp                                          */
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

#include "renderer_scene_render_rd.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/environment/fog.h"
#include "servers/rendering/renderer_rd/shaders/decal_data_inc.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/light_data_inc.glsl.gen.h"
#include "servers/rendering/renderer_rd/shaders/scene_data_inc.glsl.gen.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/shader_include_db.h"
#include "servers/rendering/storage/camera_attributes_storage.h"

void get_vogel_disk(float *r_kernel, int p_sample_count) {
	const float golden_angle = 2.4;

	for (int i = 0; i < p_sample_count; i++) {
		float r = Math::sqrt(float(i) + 0.5) / Math::sqrt(float(p_sample_count));
		float theta = float(i) * golden_angle;

		r_kernel[i * 4] = Math::cos(theta) * r;
		r_kernel[i * 4 + 1] = Math::sin(theta) * r;
	}
}

RID RendererSceneRenderRD::sky_allocate() {
	return sky.allocate_sky_rid();
}
void RendererSceneRenderRD::sky_initialize(RID p_rid) {
	sky.initialize_sky_rid(p_rid);
}

void RendererSceneRenderRD::sky_set_radiance_size(RID p_sky, int p_radiance_size) {
	sky.sky_set_radiance_size(p_sky, p_radiance_size);
}

void RendererSceneRenderRD::sky_set_mode(RID p_sky, RS::SkyMode p_mode) {
	sky.sky_set_mode(p_sky, p_mode);
}

void RendererSceneRenderRD::sky_set_material(RID p_sky, RID p_material) {
	sky.sky_set_material(p_sky, p_material);
}

Ref<Image> RendererSceneRenderRD::sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) {
	return sky.sky_bake_panorama(p_sky, p_energy, p_bake_irradiance, p_size);
}

void RendererSceneRenderRD::environment_glow_set_use_bicubic_upscale(bool p_enable) {
	glow_bicubic_upscale = p_enable;
}

void RendererSceneRenderRD::environment_set_volumetric_fog_volume_size(int p_size, int p_depth) {
	volumetric_fog_size = p_size;
	volumetric_fog_depth = p_depth;
}

void RendererSceneRenderRD::environment_set_volumetric_fog_filter_active(bool p_enable) {
	volumetric_fog_filter_active = p_enable;
}

void RendererSceneRenderRD::environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) {
	gi.sdfgi_ray_count = p_ray_count;
}

void RendererSceneRenderRD::environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) {
	gi.sdfgi_frames_to_converge = p_frames;
}
void RendererSceneRenderRD::environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) {
	gi.sdfgi_frames_to_update_light = p_update;
}

Ref<Image> RendererSceneRenderRD::environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) {
	ERR_FAIL_COND_V(p_env.is_null(), Ref<Image>());

	RS::EnvironmentBG environment_background = environment_get_background(p_env);

	if (environment_background == RS::ENV_BG_CAMERA_FEED || environment_background == RS::ENV_BG_CANVAS || environment_background == RS::ENV_BG_KEEP) {
		return Ref<Image>(); //nothing to bake
	}

	RS::EnvironmentAmbientSource ambient_source = environment_get_ambient_source(p_env);

	bool use_ambient_light = false;
	bool use_cube_map = false;
	if (ambient_source == RS::ENV_AMBIENT_SOURCE_BG && (environment_background == RS::ENV_BG_CLEAR_COLOR || environment_background == RS::ENV_BG_COLOR)) {
		use_ambient_light = true;
	} else {
		use_cube_map = (ambient_source == RS::ENV_AMBIENT_SOURCE_BG && environment_background == RS::ENV_BG_SKY) || ambient_source == RS::ENV_AMBIENT_SOURCE_SKY;
		use_ambient_light = use_cube_map || ambient_source == RS::ENV_AMBIENT_SOURCE_COLOR;
	}
	use_cube_map = use_cube_map || (environment_background == RS::ENV_BG_SKY && environment_get_sky(p_env).is_valid());

	Color ambient_color;
	float ambient_color_sky_mix = 0.0;
	if (use_ambient_light) {
		ambient_color_sky_mix = environment_get_ambient_sky_contribution(p_env);
		const float ambient_energy = environment_get_ambient_light_energy(p_env);
		ambient_color = environment_get_ambient_light(p_env);
		ambient_color = ambient_color.srgb_to_linear();
		ambient_color.r *= ambient_energy;
		ambient_color.g *= ambient_energy;
		ambient_color.b *= ambient_energy;
	}

	if (use_cube_map) {
		Ref<Image> panorama = sky_bake_panorama(environment_get_sky(p_env), environment_get_bg_energy_multiplier(p_env), p_bake_irradiance, p_size);
		if (use_ambient_light && panorama.is_valid()) {
			for (int x = 0; x < p_size.width; x++) {
				for (int y = 0; y < p_size.height; y++) {
					panorama->set_pixel(x, y, ambient_color.lerp(panorama->get_pixel(x, y), ambient_color_sky_mix));
				}
			}
		}
		return panorama;
	} else {
		const float bg_energy_multiplier = environment_get_bg_energy_multiplier(p_env);
		Color panorama_color = ((environment_background == RS::ENV_BG_CLEAR_COLOR) ? RSG::texture_storage->get_default_clear_color() : environment_get_bg_color(p_env));
		panorama_color = panorama_color.srgb_to_linear();
		panorama_color.r *= bg_energy_multiplier;
		panorama_color.g *= bg_energy_multiplier;
		panorama_color.b *= bg_energy_multiplier;

		if (use_ambient_light) {
			panorama_color = ambient_color.lerp(panorama_color, ambient_color_sky_mix);
		}

		Ref<Image> panorama = Image::create_empty(p_size.width, p_size.height, false, Image::FORMAT_RGBAF);
		panorama->fill(panorama_color);
		return panorama;
	}
}

/* REFLECTION PROBE */

RID RendererSceneRenderRD::reflection_probe_create_framebuffer(RID p_color, RID p_depth) {
	Vector<RID> fb;
	fb.push_back(p_color);
	fb.push_back(p_depth);
	return RD::get_singleton()->framebuffer_create(fb);
}

/* FOG VOLUME INSTANCE */

RID RendererSceneRenderRD::fog_volume_instance_create(RID p_fog_volume) {
	return RendererRD::Fog::get_singleton()->fog_volume_instance_create(p_fog_volume);
}

void RendererSceneRenderRD::fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) {
	RendererRD::Fog::get_singleton()->fog_volume_instance_set_transform(p_fog_volume_instance, p_transform);
}

void RendererSceneRenderRD::fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) {
	RendererRD::Fog::get_singleton()->fog_volume_instance_set_active(p_fog_volume_instance, p_active);
}

RID RendererSceneRenderRD::fog_volume_instance_get_volume(RID p_fog_volume_instance) const {
	return RendererRD::Fog::get_singleton()->fog_volume_instance_get_volume(p_fog_volume_instance);
}

Vector3 RendererSceneRenderRD::fog_volume_instance_get_position(RID p_fog_volume_instance) const {
	return RendererRD::Fog::get_singleton()->fog_volume_instance_get_position(p_fog_volume_instance);
}

/* VOXEL GI */

RID RendererSceneRenderRD::voxel_gi_instance_create(RID p_base) {
	return gi.voxel_gi_instance_create(p_base);
}

void RendererSceneRenderRD::voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) {
	if (!is_dynamic_gi_supported()) {
		return;
	}

	gi.voxel_gi_instance_set_transform_to_data(p_probe, p_xform);
}

bool RendererSceneRenderRD::voxel_gi_needs_update(RID p_probe) const {
	if (!is_dynamic_gi_supported()) {
		return false;
	}

	return gi.voxel_gi_needs_update(p_probe);
}

void RendererSceneRenderRD::voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) {
	if (!is_dynamic_gi_supported()) {
		return;
	}

	gi.voxel_gi_update(p_probe, p_update_light_instances, p_light_instances, p_dynamic_objects);
}

void RendererSceneRenderRD::_debug_sdfgi_probes(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_framebuffer, const uint32_t p_view_count, const Projection *p_camera_with_transforms) {
	ERR_FAIL_COND(p_render_buffers.is_null());

	if (!p_render_buffers->has_custom_data(RB_SCOPE_SDFGI)) {
		return; //nothing to debug
	}

	Ref<RendererRD::GI::SDFGI> sdfgi = p_render_buffers->get_custom_data(RB_SCOPE_SDFGI);

	sdfgi->debug_probes(p_framebuffer, p_view_count, p_camera_with_transforms);
}

////////////////////////////////
Ref<RenderSceneBuffers> RendererSceneRenderRD::render_buffers_create() {
	Ref<RenderSceneBuffersRD> rb;
	rb.instantiate();

	rb->set_can_be_storage(_render_buffers_can_be_storage());
	rb->set_max_cluster_elements(max_cluster_elements);
	rb->set_base_data_format(_render_buffers_get_color_format());
	if (vrs) {
		rb->set_vrs(vrs);
	}

	setup_render_buffer_data(rb);

	return rb;
}

bool RendererSceneRenderRD::_compositor_effects_has_flag(const RenderDataRD *p_render_data, RS::CompositorEffectFlags p_flag, RS::CompositorEffectCallbackType p_callback_type) {
	RendererCompositorStorage *comp_storage = RendererCompositorStorage::get_singleton();

	if (p_render_data->compositor.is_null()) {
		return false;
	}

	if (p_render_data->reflection_probe.is_valid()) {
		return false;
	}

	ERR_FAIL_COND_V(!comp_storage->is_compositor(p_render_data->compositor), false);
	Vector<RID> re_rids = comp_storage->compositor_get_compositor_effects(p_render_data->compositor, p_callback_type, true);

	for (RID rid : re_rids) {
		if (comp_storage->compositor_effect_get_flag(rid, p_flag)) {
			return true;
		}
	}

	return false;
}

bool RendererSceneRenderRD::_has_compositor_effect(RS::CompositorEffectCallbackType p_callback_type, const RenderDataRD *p_render_data) {
	RendererCompositorStorage *comp_storage = RendererCompositorStorage::get_singleton();

	if (p_render_data->compositor.is_null()) {
		return false;
	}

	if (p_render_data->reflection_probe.is_valid()) {
		return false;
	}

	ERR_FAIL_COND_V(!comp_storage->is_compositor(p_render_data->compositor), false);

	Vector<RID> effects = comp_storage->compositor_get_compositor_effects(p_render_data->compositor, p_callback_type, true);

	return effects.size() > 0;
}

void RendererSceneRenderRD::_process_compositor_effects(RS::CompositorEffectCallbackType p_callback_type, const RenderDataRD *p_render_data) {
	RendererCompositorStorage *comp_storage = RendererCompositorStorage::get_singleton();

	if (p_render_data->compositor.is_null()) {
		return;
	}

	if (p_render_data->reflection_probe.is_valid()) {
		return;
	}

	ERR_FAIL_COND(!comp_storage->is_compositor(p_render_data->compositor));

	Vector<RID> re_rids = comp_storage->compositor_get_compositor_effects(p_render_data->compositor, p_callback_type, true);

	for (RID rid : re_rids) {
		Array arr;
		Callable callback = comp_storage->compositor_effect_get_callback(rid);

		arr.push_back(p_callback_type);
		arr.push_back(p_render_data);

		callback.callv(arr);
	}
}

void RendererSceneRenderRD::_render_buffers_copy_screen_texture(const RenderDataRD *p_render_data) {
	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	if (!rb->has_internal_texture()) {
		// We're likely rendering reflection probes where we can't use our backbuffers.
		return;
	}

	RD::get_singleton()->draw_command_begin_label("Copy screen texture");

	StringName texture_name;
	bool can_use_storage = _render_buffers_can_be_storage();
	Size2i size = rb->get_internal_size();

	// When upscaling, the blur texture needs to be at the target size for post-processing to work. We prefer to use a
	// dedicated backbuffer copy texture instead if the blur texture is not an option so shader effects work correctly.
	Size2i target_size = rb->get_target_size();
	bool internal_size_matches = (size.width == target_size.width) && (size.height == target_size.height);
	bool reuse_blur_texture = !rb->has_upscaled_texture() || internal_size_matches;
	if (reuse_blur_texture) {
		rb->allocate_blur_textures();
		texture_name = RB_TEX_BLUR_0;
	} else {
		uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
		usage_bits |= can_use_storage ? RD::TEXTURE_USAGE_STORAGE_BIT : RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
		rb->create_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_COLOR, rb->get_base_data_format(), usage_bits);
		texture_name = RB_TEX_BACK_COLOR;
	}

	for (uint32_t v = 0; v < rb->get_view_count(); v++) {
		RID texture = rb->get_internal_texture(v);
		int mipmaps = int(rb->get_texture_format(RB_SCOPE_BUFFERS, texture_name).mipmaps);
		RID dest = rb->get_texture_slice(RB_SCOPE_BUFFERS, texture_name, v, 0);

		if (can_use_storage) {
			copy_effects->copy_to_rect(texture, dest, Rect2i(0, 0, size.x, size.y));
		} else {
			RID fb = FramebufferCacheRD::get_singleton()->get_cache(dest);
			copy_effects->copy_to_fb_rect(texture, fb, Rect2i(0, 0, size.x, size.y));
		}

		for (int i = 1; i < mipmaps; i++) {
			RID source = dest;
			dest = rb->get_texture_slice(RB_SCOPE_BUFFERS, texture_name, v, i);
			Size2i msize = rb->get_texture_slice_size(RB_SCOPE_BUFFERS, texture_name, i);

			if (can_use_storage) {
				copy_effects->make_mipmap(source, dest, msize);
			} else {
				copy_effects->make_mipmap_raster(source, dest, msize);
			}
		}
	}

	RD::get_singleton()->draw_command_end_label();
}

void RendererSceneRenderRD::_render_buffers_copy_depth_texture(const RenderDataRD *p_render_data) {
	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	if (!rb->has_depth_texture()) {
		// We're likely rendering reflection probes where we can't use our backbuffers.
		return;
	}

	RD::get_singleton()->draw_command_begin_label("Copy depth texture");

	// note, this only creates our back depth texture if we haven't already created it.
	uint32_t usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
	usage_bits |= RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_STORAGE_BIT;
	usage_bits |= RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT; // set this as color attachment because we're copying data into it, it's not actually used as a depth buffer

	rb->create_texture(RB_SCOPE_BUFFERS, RB_TEX_BACK_DEPTH, RD::DATA_FORMAT_R32_SFLOAT, usage_bits, RD::TEXTURE_SAMPLES_1);

	bool can_use_storage = _render_buffers_can_be_storage();
	Size2i size = rb->get_internal_size();
	for (uint32_t v = 0; v < p_render_data->scene_data->view_count; v++) {
		RID depth_texture = rb->get_depth_texture(v);
		RID depth_back_texture = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BACK_DEPTH, v, 0);

		if (can_use_storage) {
			copy_effects->copy_to_rect(depth_texture, depth_back_texture, Rect2i(0, 0, size.x, size.y));
		} else {
			RID depth_back_fb = FramebufferCacheRD::get_singleton()->get_cache(depth_back_texture);
			copy_effects->copy_to_fb_rect(depth_texture, depth_back_fb, Rect2i(0, 0, size.x, size.y));
		}
	}

	RD::get_singleton()->draw_command_end_label();
}

void RendererSceneRenderRD::_render_buffers_post_process_and_tonemap(const RenderDataRD *p_render_data) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	ERR_FAIL_NULL(p_render_data);

	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	ERR_FAIL_COND_MSG(p_render_data->reflection_probe.is_valid(), "Post processes should not be applied on reflection probes.");

	// Glow, auto exposure and DoF (if enabled).

	Size2i target_size = rb->get_target_size();
	bool can_use_effects = target_size.x >= 8 && target_size.y >= 8; // FIXME I think this should check internal size, we do all our post processing at this size...
	can_use_effects &= _debug_draw_can_use_effects(debug_draw);
	bool can_use_storage = _render_buffers_can_be_storage();

	RS::ViewportScaling3DMode scale_mode = rb->get_scaling_3d_mode();
	bool use_upscaled_texture = rb->has_upscaled_texture() && (scale_mode == RS::VIEWPORT_SCALING_3D_MODE_FSR2 || scale_mode == RS::VIEWPORT_SCALING_3D_MODE_METALFX_TEMPORAL);
	SpatialUpscaler *spatial_upscaler = nullptr;
	if (can_use_effects) {
		if (scale_mode == RS::VIEWPORT_SCALING_3D_MODE_FSR) {
			spatial_upscaler = fsr;
		} else if (scale_mode == RS::VIEWPORT_SCALING_3D_MODE_METALFX_SPATIAL) {
#if METAL_ENABLED
			spatial_upscaler = mfx_spatial;
#endif
		}
	}

	RID render_target = rb->get_render_target();
	RID color_texture = use_upscaled_texture ? rb->get_upscaled_texture() : rb->get_internal_texture();
	Size2i color_size = use_upscaled_texture ? target_size : rb->get_internal_size();

	bool dest_is_msaa_2d = rb->get_view_count() == 1 && texture_storage->render_target_get_msaa(render_target) != RS::VIEWPORT_MSAA_DISABLED;

	if (can_use_effects && RSG::camera_attributes->camera_attributes_uses_dof(p_render_data->camera_attributes)) {
		RENDER_TIMESTAMP("Depth of Field");
		RD::get_singleton()->draw_command_begin_label("DOF");

		rb->allocate_blur_textures();

		RendererRD::BokehDOF::BokehBuffers buffers;

		// Textures we use
		buffers.base_texture_size = color_size;
		buffers.secondary_texture = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, 0, 0);
		buffers.half_texture[0] = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, 0, 0);
		buffers.half_texture[1] = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, 0, 1);

		if (can_use_storage) {
			for (uint32_t i = 0; i < rb->get_view_count(); i++) {
				buffers.base_texture = use_upscaled_texture ? rb->get_upscaled_texture(i) : rb->get_internal_texture(i);
				buffers.depth_texture = rb->get_depth_texture(i);

				// In stereo p_render_data->z_near and p_render_data->z_far can be offset for our combined frustum.
				float z_near = p_render_data->scene_data->view_projection[i].get_z_near();
				float z_far = p_render_data->scene_data->view_projection[i].get_z_far();
				bokeh_dof->bokeh_dof_compute(buffers, p_render_data->camera_attributes, z_near, z_far, p_render_data->scene_data->cam_orthogonal);
			};
		} else {
			// Set framebuffers.
			buffers.secondary_fb = rb->weight_buffers[1].fb;
			buffers.half_fb[0] = rb->weight_buffers[2].fb;
			buffers.half_fb[1] = rb->weight_buffers[3].fb;
			buffers.weight_texture[0] = rb->weight_buffers[0].weight;
			buffers.weight_texture[1] = rb->weight_buffers[1].weight;
			buffers.weight_texture[2] = rb->weight_buffers[2].weight;
			buffers.weight_texture[3] = rb->weight_buffers[3].weight;

			// Set weight buffers.
			buffers.base_weight_fb = rb->weight_buffers[0].fb;

			for (uint32_t i = 0; i < rb->get_view_count(); i++) {
				buffers.base_texture = use_upscaled_texture ? rb->get_upscaled_texture(i) : rb->get_internal_texture(i);
				buffers.depth_texture = rb->get_depth_texture(i);
				buffers.base_fb = FramebufferCacheRD::get_singleton()->get_cache(buffers.base_texture); // TODO move this into bokeh_dof_raster, we can do this internally

				// In stereo p_render_data->z_near and p_render_data->z_far can be offset for our combined frustum.
				float z_near = p_render_data->scene_data->view_projection[i].get_z_near();
				float z_far = p_render_data->scene_data->view_projection[i].get_z_far();
				bokeh_dof->bokeh_dof_raster(buffers, p_render_data->camera_attributes, z_near, z_far, p_render_data->scene_data->cam_orthogonal);
			}
		}
		RD::get_singleton()->draw_command_end_label();
	}

	float auto_exposure_scale = 1.0;

	if (can_use_effects && RSG::camera_attributes->camera_attributes_uses_auto_exposure(p_render_data->camera_attributes)) {
		RENDER_TIMESTAMP("Auto exposure");

		RD::get_singleton()->draw_command_begin_label("Auto exposure");

		Ref<RendererRD::Luminance::LuminanceBuffers> luminance_buffers = luminance->get_luminance_buffers(rb);

		uint64_t auto_exposure_version = RSG::camera_attributes->camera_attributes_get_auto_exposure_version(p_render_data->camera_attributes);
		bool set_immediate = auto_exposure_version != rb->get_auto_exposure_version();
		rb->set_auto_exposure_version(auto_exposure_version);

		double step = RSG::camera_attributes->camera_attributes_get_auto_exposure_adjust_speed(p_render_data->camera_attributes) * time_step;
		float auto_exposure_min_sensitivity = RSG::camera_attributes->camera_attributes_get_auto_exposure_min_sensitivity(p_render_data->camera_attributes);
		float auto_exposure_max_sensitivity = RSG::camera_attributes->camera_attributes_get_auto_exposure_max_sensitivity(p_render_data->camera_attributes);
		luminance->luminance_reduction(rb->get_internal_texture(), rb->get_internal_size(), luminance_buffers, auto_exposure_min_sensitivity, auto_exposure_max_sensitivity, step, set_immediate);

		// Swap final reduce with prev luminance.

		auto_exposure_scale = RSG::camera_attributes->camera_attributes_get_auto_exposure_scale(p_render_data->camera_attributes);

		RenderingServerDefault::redraw_request(); // Redraw all the time if auto exposure rendering is on.
		RD::get_singleton()->draw_command_end_label();
	}

	int max_glow_level = -1;

	if (can_use_effects && p_render_data->environment.is_valid() && environment_get_glow_enabled(p_render_data->environment)) {
		RENDER_TIMESTAMP("Glow");
		RD::get_singleton()->draw_command_begin_label("Gaussian Glow");

		rb->allocate_blur_textures();

		for (int i = 0; i < RS::MAX_GLOW_LEVELS; i++) {
			if (environment_get_glow_levels(p_render_data->environment)[i] > 0.0) {
				int mipmaps = int(rb->get_texture_format(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1).mipmaps);
				if (i >= mipmaps) {
					max_glow_level = mipmaps - 1;
				} else {
					max_glow_level = i;
				}
			}
		}

		float luminance_multiplier = _render_buffers_get_luminance_multiplier();
		for (uint32_t l = 0; l < rb->get_view_count(); l++) {
			for (int i = 0; i < (max_glow_level + 1); i++) {
				Size2i vp_size = rb->get_texture_slice_size(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, i);

				if (i == 0) {
					RID luminance_texture;
					if (RSG::camera_attributes->camera_attributes_uses_auto_exposure(p_render_data->camera_attributes)) {
						luminance_texture = luminance->get_current_luminance_buffer(rb); // this will return and empty RID if we don't have an auto exposure buffer
					}
					RID source = rb->get_internal_texture(l);
					RID dest = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, l, i);
					if (can_use_storage) {
						copy_effects->gaussian_glow(source, dest, vp_size, environment_get_glow_strength(p_render_data->environment), true, environment_get_glow_hdr_luminance_cap(p_render_data->environment), environment_get_exposure(p_render_data->environment), environment_get_glow_bloom(p_render_data->environment), environment_get_glow_hdr_bleed_threshold(p_render_data->environment), environment_get_glow_hdr_bleed_scale(p_render_data->environment), luminance_texture, auto_exposure_scale);
					} else {
						RID half = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_HALF_BLUR, 0, i); // we can reuse this for each view
						copy_effects->gaussian_glow_raster(source, half, dest, luminance_multiplier, vp_size, environment_get_glow_strength(p_render_data->environment), true, environment_get_glow_hdr_luminance_cap(p_render_data->environment), environment_get_exposure(p_render_data->environment), environment_get_glow_bloom(p_render_data->environment), environment_get_glow_hdr_bleed_threshold(p_render_data->environment), environment_get_glow_hdr_bleed_scale(p_render_data->environment), luminance_texture, auto_exposure_scale);
					}
				} else {
					RID source = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, l, i - 1);
					RID dest = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, l, i);

					if (can_use_storage) {
						copy_effects->gaussian_glow(source, dest, vp_size, environment_get_glow_strength(p_render_data->environment));
					} else {
						RID half = rb->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_HALF_BLUR, 0, i); // we can reuse this for each view
						copy_effects->gaussian_glow_raster(source, half, dest, luminance_multiplier, vp_size, environment_get_glow_strength(p_render_data->environment));
					}
				}
			}
		}

		RD::get_singleton()->draw_command_end_label();
	}

	{
		RENDER_TIMESTAMP("Tonemap");
		RD::get_singleton()->draw_command_begin_label("Tonemap");

		RendererRD::ToneMapper::TonemapSettings tonemap;

		tonemap.exposure_texture = luminance->get_current_luminance_buffer(rb);
		if (can_use_effects && RSG::camera_attributes->camera_attributes_uses_auto_exposure(p_render_data->camera_attributes) && tonemap.exposure_texture.is_valid()) {
			tonemap.use_auto_exposure = true;
			tonemap.auto_exposure_scale = auto_exposure_scale;
		} else {
			tonemap.exposure_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
		}

		if (can_use_effects && p_render_data->environment.is_valid() && environment_get_glow_enabled(p_render_data->environment)) {
			tonemap.use_glow = true;
			tonemap.glow_mode = RendererRD::ToneMapper::TonemapSettings::GlowMode(environment_get_glow_blend_mode(p_render_data->environment));
			tonemap.glow_intensity = environment_get_glow_blend_mode(p_render_data->environment) == RS::ENV_GLOW_BLEND_MODE_MIX ? environment_get_glow_mix(p_render_data->environment) : environment_get_glow_intensity(p_render_data->environment);
			for (int i = 0; i < RS::MAX_GLOW_LEVELS; i++) {
				tonemap.glow_levels[i] = environment_get_glow_levels(p_render_data->environment)[i];
			}

			Size2i msize = rb->get_texture_slice_size(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, 0);
			tonemap.glow_texture_size.x = msize.width;
			tonemap.glow_texture_size.y = msize.height;
			tonemap.glow_use_bicubic_upscale = glow_bicubic_upscale;
			tonemap.glow_texture = rb->get_texture(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1);
			if (environment_get_glow_map(p_render_data->environment).is_valid()) {
				tonemap.glow_map_strength = environment_get_glow_map_strength(p_render_data->environment);
				tonemap.glow_map = texture_storage->texture_get_rd_texture(environment_get_glow_map(p_render_data->environment));
			} else {
				tonemap.glow_map_strength = 0.0f;
				tonemap.glow_map = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
			}

		} else {
			tonemap.glow_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
			tonemap.glow_map = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
		}

		if (rb->get_screen_space_aa() == RS::VIEWPORT_SCREEN_SPACE_AA_FXAA) {
			tonemap.use_fxaa = true;
		}

		tonemap.use_debanding = rb->get_use_debanding();
		tonemap.texture_size = Vector2i(color_size.x, color_size.y);

		if (p_render_data->environment.is_valid()) {
			tonemap.tonemap_mode = environment_get_tone_mapper(p_render_data->environment);
			tonemap.white = environment_get_white(p_render_data->environment);
			tonemap.exposure = environment_get_exposure(p_render_data->environment);
		}

		tonemap.use_color_correction = false;
		tonemap.use_1d_color_correction = false;
		tonemap.color_correction_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);

		if (can_use_effects && p_render_data->environment.is_valid()) {
			tonemap.use_bcs = environment_get_adjustments_enabled(p_render_data->environment);
			tonemap.brightness = environment_get_adjustments_brightness(p_render_data->environment);
			tonemap.contrast = environment_get_adjustments_contrast(p_render_data->environment);
			tonemap.saturation = environment_get_adjustments_saturation(p_render_data->environment);
			if (environment_get_adjustments_enabled(p_render_data->environment) && environment_get_color_correction(p_render_data->environment).is_valid()) {
				tonemap.use_color_correction = true;
				tonemap.use_1d_color_correction = environment_get_use_1d_color_correction(p_render_data->environment);
				tonemap.color_correction_texture = texture_storage->texture_get_rd_texture(environment_get_color_correction(p_render_data->environment));
			}
		}

		tonemap.luminance_multiplier = _render_buffers_get_luminance_multiplier();
		tonemap.view_count = rb->get_view_count();

		tonemap.convert_to_srgb = !texture_storage->render_target_is_using_hdr(render_target);

		RID dest_fb;
		if (spatial_upscaler != nullptr) {
			// If we use a spatial upscaler to upscale we need to write our result into an intermediate buffer.
			// Note that this is cached so we only create the texture the first time.
			RID dest_texture = rb->create_texture(SNAME("Tonemapper"), SNAME("destination"), _render_buffers_get_color_format(), RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT, RD::TEXTURE_SAMPLES_1, Size2i(), 0, 1, true, true);
			dest_fb = FramebufferCacheRD::get_singleton()->get_cache(dest_texture);
		} else {
			// If we do a bilinear upscale we just render into our render target and our shader will upscale automatically.
			// Target size in this case is lying as we never get our real target size communicated.
			// Bit nasty but...

			if (dest_is_msaa_2d) {
				dest_fb = FramebufferCacheRD::get_singleton()->get_cache(texture_storage->render_target_get_rd_texture_msaa(render_target));
				texture_storage->render_target_set_msaa_needs_resolve(render_target, true); // Make sure this gets resolved.
			} else {
				dest_fb = texture_storage->render_target_get_rd_framebuffer(render_target);
			}
		}

		tone_mapper->tonemapper(color_texture, dest_fb, tonemap);

		RD::get_singleton()->draw_command_end_label();
	}

	if (rb.is_valid() && spatial_upscaler) {
		spatial_upscaler->ensure_context(rb);

		RD::get_singleton()->draw_command_begin_label(spatial_upscaler->get_label());

		for (uint32_t v = 0; v < rb->get_view_count(); v++) {
			RID source_texture = rb->get_texture_slice(SNAME("Tonemapper"), SNAME("destination"), v, 0);
			RID dest_texture = texture_storage->render_target_get_rd_texture_slice(render_target, v);

			spatial_upscaler->process(rb, source_texture, dest_texture);
		}

		if (dest_is_msaa_2d) {
			// We can't upscale directly into our MSAA buffer so we need to do a copy
			RID source_texture = texture_storage->render_target_get_rd_texture(render_target);
			RID dest_fb = FramebufferCacheRD::get_singleton()->get_cache(texture_storage->render_target_get_rd_texture_msaa(render_target));
			copy_effects->copy_to_fb_rect(source_texture, dest_fb, Rect2i(Point2i(), rb->get_target_size()));

			texture_storage->render_target_set_msaa_needs_resolve(render_target, true); // Make sure this gets resolved.
		}

		RD::get_singleton()->draw_command_end_label();
	}

	texture_storage->render_target_disable_clear_request(render_target);
}

void RendererSceneRenderRD::_post_process_subpass(RID p_source_texture, RID p_framebuffer, const RenderDataRD *p_render_data) {
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	RD::get_singleton()->draw_command_begin_label("Post Process Subpass");

	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	// FIXME: Our input it our internal_texture, shouldn't this be using internal_size ??
	// Seeing we don't support FSR in our mobile renderer right now target_size = internal_size...
	Size2i target_size = rb->get_target_size();
	bool can_use_effects = target_size.x >= 8 && target_size.y >= 8 && debug_draw == RS::VIEWPORT_DEBUG_DRAW_DISABLED;

	RD::DrawListID draw_list = RD::get_singleton()->draw_list_switch_to_next_pass();

	RendererRD::ToneMapper::TonemapSettings tonemap;

	if (p_render_data->environment.is_valid()) {
		tonemap.tonemap_mode = environment_get_tone_mapper(p_render_data->environment);
		tonemap.exposure = environment_get_exposure(p_render_data->environment);
		tonemap.white = environment_get_white(p_render_data->environment);
	}

	// We don't support glow or auto exposure here, if they are needed, don't use subpasses!
	// The problem is that we need to use the result so far and process them before we can
	// apply this to our results.
	if (can_use_effects && p_render_data->environment.is_valid() && environment_get_glow_enabled(p_render_data->environment)) {
		ERR_FAIL_MSG("Glow is not supported when using subpasses.");
	}

	if (can_use_effects && RSG::camera_attributes->camera_attributes_uses_auto_exposure(p_render_data->camera_attributes)) {
		ERR_FAIL_MSG("Auto Exposure is not supported when using subpasses.");
	}

	tonemap.use_glow = false;
	tonemap.glow_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
	tonemap.glow_map = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
	tonemap.use_auto_exposure = false;
	tonemap.exposure_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_WHITE);

	tonemap.use_color_correction = false;
	tonemap.use_1d_color_correction = false;
	tonemap.color_correction_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);

	if (can_use_effects && p_render_data->environment.is_valid()) {
		tonemap.use_bcs = environment_get_adjustments_enabled(p_render_data->environment);
		tonemap.brightness = environment_get_adjustments_brightness(p_render_data->environment);
		tonemap.contrast = environment_get_adjustments_contrast(p_render_data->environment);
		tonemap.saturation = environment_get_adjustments_saturation(p_render_data->environment);
		if (environment_get_adjustments_enabled(p_render_data->environment) && environment_get_color_correction(p_render_data->environment).is_valid()) {
			tonemap.use_color_correction = true;
			tonemap.use_1d_color_correction = environment_get_use_1d_color_correction(p_render_data->environment);
			tonemap.color_correction_texture = texture_storage->texture_get_rd_texture(environment_get_color_correction(p_render_data->environment));
		}
	}

	tonemap.use_debanding = rb->get_use_debanding();
	tonemap.texture_size = Vector2i(target_size.x, target_size.y);

	tonemap.luminance_multiplier = _render_buffers_get_luminance_multiplier();
	tonemap.view_count = rb->get_view_count();

	tonemap.convert_to_srgb = !texture_storage->render_target_is_using_hdr(rb->get_render_target());

	tone_mapper->tonemapper(draw_list, p_source_texture, RD::get_singleton()->framebuffer_get_format(p_framebuffer), tonemap);

	RD::get_singleton()->draw_command_end_label();
}

void RendererSceneRenderRD::_disable_clear_request(const RenderDataRD *p_render_data) {
	ERR_FAIL_COND(p_render_data->render_buffers.is_null());

	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();
	texture_storage->render_target_disable_clear_request(p_render_data->render_buffers->get_render_target());
}

bool RendererSceneRenderRD::_debug_draw_can_use_effects(RS::ViewportDebugDraw p_debug_draw) {
	bool can_use_effects = true;
	switch (p_debug_draw) {
		// No debug draw, use camera effects
		case RS::VIEWPORT_DEBUG_DRAW_DISABLED:
			can_use_effects = true;
			break;
		// Modes that completely override rendering to draw debug information should disable camera effects.
		case RS::VIEWPORT_DEBUG_DRAW_UNSHADED:
		case RS::VIEWPORT_DEBUG_DRAW_OVERDRAW:
		case RS::VIEWPORT_DEBUG_DRAW_WIREFRAME:
		case RS::VIEWPORT_DEBUG_DRAW_VOXEL_GI_ALBEDO:
		case RS::VIEWPORT_DEBUG_DRAW_CLUSTER_OMNI_LIGHTS:
		case RS::VIEWPORT_DEBUG_DRAW_CLUSTER_SPOT_LIGHTS:
		case RS::VIEWPORT_DEBUG_DRAW_CLUSTER_DECALS:
		case RS::VIEWPORT_DEBUG_DRAW_CLUSTER_REFLECTION_PROBES:
		case RS::VIEWPORT_DEBUG_DRAW_INTERNAL_BUFFER:
			can_use_effects = false;
			break;
		// Modes that draws information over part of the viewport needs camera effects because we see partially the normal draw mode.
		case RS::VIEWPORT_DEBUG_DRAW_SHADOW_ATLAS:
		case RS::VIEWPORT_DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS:
		case RS::VIEWPORT_DEBUG_DRAW_DECAL_ATLAS:
		case RS::VIEWPORT_DEBUG_DRAW_MOTION_VECTORS:
		// Modes that draws a buffer over viewport needs camera effects because if the buffer is not available it will be equivalent to normal draw mode.
		case RS::VIEWPORT_DEBUG_DRAW_NORMAL_BUFFER:
		case RS::VIEWPORT_DEBUG_DRAW_SSAO:
		case RS::VIEWPORT_DEBUG_DRAW_SSIL:
		case RS::VIEWPORT_DEBUG_DRAW_SDFGI:
		case RS::VIEWPORT_DEBUG_DRAW_GI_BUFFER:
		case RS::VIEWPORT_DEBUG_DRAW_OCCLUDERS:
			can_use_effects = true;
			break;
		// Other debug draw modes keep camera effects.
		case RS::VIEWPORT_DEBUG_DRAW_LIGHTING:
		case RS::VIEWPORT_DEBUG_DRAW_VOXEL_GI_LIGHTING:
		case RS::VIEWPORT_DEBUG_DRAW_VOXEL_GI_EMISSION:
		case RS::VIEWPORT_DEBUG_DRAW_SCENE_LUMINANCE:
		case RS::VIEWPORT_DEBUG_DRAW_PSSM_SPLITS:
		case RS::VIEWPORT_DEBUG_DRAW_SDFGI_PROBES:
		case RS::VIEWPORT_DEBUG_DRAW_DISABLE_LOD:
			can_use_effects = true;
			break;
		default:
			break;
	}

	return can_use_effects;
}

void RendererSceneRenderRD::_render_buffers_debug_draw(const RenderDataRD *p_render_data) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	Ref<RenderSceneBuffersRD> rb = p_render_data->render_buffers;
	ERR_FAIL_COND(rb.is_null());

	RID render_target = rb->get_render_target();

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_SHADOW_ATLAS) {
		if (p_render_data->shadow_atlas.is_valid()) {
			RID shadow_atlas_texture = RendererRD::LightStorage::get_singleton()->shadow_atlas_get_texture(p_render_data->shadow_atlas);

			if (shadow_atlas_texture.is_null()) {
				shadow_atlas_texture = texture_storage->texture_rd_get_default(RendererRD::TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
			}

			Size2 rtsize = texture_storage->render_target_get_size(render_target);
			copy_effects->copy_to_fb_rect(shadow_atlas_texture, texture_storage->render_target_get_rd_framebuffer(render_target), Rect2i(Vector2(), rtsize / 2), false, true);
		}
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS) {
		if (RendererRD::LightStorage::get_singleton()->directional_shadow_get_texture().is_valid()) {
			RID shadow_atlas_texture = RendererRD::LightStorage::get_singleton()->directional_shadow_get_texture();
			Size2i rtsize = texture_storage->render_target_get_size(render_target);
			RID dest_fb = texture_storage->render_target_get_rd_framebuffer(render_target);

			// Determine our display size, try and keep square by using the smallest edge.
			Size2i size = 2 * rtsize / 3;
			if (size.x < size.y) {
				size.y = size.x;
			} else if (size.y < size.x) {
				size.x = size.y;
			}

			copy_effects->copy_to_fb_rect(shadow_atlas_texture, dest_fb, Rect2i(Vector2(), size), false, true);

			// Visualize our view frustum to show coverage.
			for (int i = 0; i < p_render_data->render_shadow_count; i++) {
				RID light = p_render_data->render_shadows[i].light;
				RID base = light_storage->light_instance_get_base_light(light);

				if (light_storage->light_get_type(base) == RS::LIGHT_DIRECTIONAL) {
					debug_effects->draw_shadow_frustum(light, p_render_data->scene_data->cam_projection, p_render_data->scene_data->cam_transform, dest_fb, Rect2(Size2(), size));
				}
			}
		}
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_DECAL_ATLAS) {
		RID decal_atlas = RendererRD::TextureStorage::get_singleton()->decal_atlas_get_texture();

		if (decal_atlas.is_valid()) {
			Size2i rtsize = texture_storage->render_target_get_size(render_target);

			copy_effects->copy_to_fb_rect(decal_atlas, texture_storage->render_target_get_rd_framebuffer(render_target), Rect2i(Vector2(), rtsize / 2), false, false, true);
		}
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_SCENE_LUMINANCE) {
		RID luminance_texture = luminance->get_current_luminance_buffer(rb);
		if (luminance_texture.is_valid()) {
			Size2i rtsize = texture_storage->render_target_get_size(render_target);

			copy_effects->copy_to_fb_rect(luminance_texture, texture_storage->render_target_get_rd_framebuffer(render_target), Rect2(Vector2(), rtsize / 8), false, true);
		}
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_INTERNAL_BUFFER) {
		Size2 rtsize = texture_storage->render_target_get_size(render_target);
		copy_effects->copy_to_fb_rect(rb->get_internal_texture(), texture_storage->render_target_get_rd_framebuffer(render_target), Rect2(Vector2(), rtsize), false, false);
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_NORMAL_BUFFER && _render_buffers_get_normal_texture(rb).is_valid()) {
		Size2 rtsize = texture_storage->render_target_get_size(render_target);
		copy_effects->copy_to_fb_rect(_render_buffers_get_normal_texture(rb), texture_storage->render_target_get_rd_framebuffer(render_target), Rect2(Vector2(), rtsize), false, false, false, false, RID(), false, false, false, true);
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_OCCLUDERS) {
		if (p_render_data->occluder_debug_tex.is_valid()) {
			Size2i rtsize = texture_storage->render_target_get_size(render_target);
			copy_effects->copy_to_fb_rect(texture_storage->texture_get_rd_texture(p_render_data->occluder_debug_tex), texture_storage->render_target_get_rd_framebuffer(render_target), Rect2i(Vector2(), rtsize), true, false);
		}
	}

	if (debug_draw == RS::VIEWPORT_DEBUG_DRAW_MOTION_VECTORS && _render_buffers_get_velocity_texture(rb).is_valid()) {
		RID velocity = _render_buffers_get_velocity_texture(rb);
		RID depth = rb->get_depth_texture();
		RID dest_fb = texture_storage->render_target_get_rd_framebuffer(render_target);
		Size2i resolution = rb->get_internal_size();

		debug_effects->draw_motion_vectors(velocity, depth, dest_fb, p_render_data->scene_data->cam_projection, p_render_data->scene_data->cam_transform, p_render_data->scene_data->prev_cam_projection, p_render_data->scene_data->prev_cam_transform, resolution);
	}
}

RID RendererSceneRenderRD::render_buffers_get_default_voxel_gi_buffer() {
	return gi.default_voxel_gi_buffer;
}

float RendererSceneRenderRD::_render_buffers_get_luminance_multiplier() {
	return 1.0;
}

RD::DataFormat RendererSceneRenderRD::_render_buffers_get_color_format() {
	return RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
}

bool RendererSceneRenderRD::_render_buffers_can_be_storage() {
	return true;
}

void RendererSceneRenderRD::gi_set_use_half_resolution(bool p_enable) {
	gi.half_resolution = p_enable;
}

void RendererSceneRenderRD::positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) {
	ERR_FAIL_INDEX_MSG(p_quality, RS::SHADOW_QUALITY_MAX, "Shadow quality too high, please see RenderingServer's ShadowQuality enum");

	if (shadows_quality != p_quality) {
		shadows_quality = p_quality;

		switch (shadows_quality) {
			case RS::SHADOW_QUALITY_HARD: {
				penumbra_shadow_samples = 4;
				soft_shadow_samples = 0;
				shadows_quality_radius = 1.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_VERY_LOW: {
				penumbra_shadow_samples = 4;
				soft_shadow_samples = 1;
				shadows_quality_radius = 1.5;
			} break;
			case RS::SHADOW_QUALITY_SOFT_LOW: {
				penumbra_shadow_samples = 8;
				soft_shadow_samples = 4;
				shadows_quality_radius = 2.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_MEDIUM: {
				penumbra_shadow_samples = 12;
				soft_shadow_samples = 8;
				shadows_quality_radius = 2.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_HIGH: {
				penumbra_shadow_samples = 24;
				soft_shadow_samples = 16;
				shadows_quality_radius = 3.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_ULTRA: {
				penumbra_shadow_samples = 32;
				soft_shadow_samples = 32;
				shadows_quality_radius = 4.0;
			} break;
			case RS::SHADOW_QUALITY_MAX:
				break;
		}
		get_vogel_disk(penumbra_shadow_kernel, penumbra_shadow_samples);
		get_vogel_disk(soft_shadow_kernel, soft_shadow_samples);
	}

	_update_shader_quality_settings();
}

void RendererSceneRenderRD::directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) {
	ERR_FAIL_INDEX_MSG(p_quality, RS::SHADOW_QUALITY_MAX, "Shadow quality too high, please see RenderingServer's ShadowQuality enum");

	if (directional_shadow_quality != p_quality) {
		directional_shadow_quality = p_quality;

		switch (directional_shadow_quality) {
			case RS::SHADOW_QUALITY_HARD: {
				directional_penumbra_shadow_samples = 4;
				directional_soft_shadow_samples = 0;
				directional_shadow_quality_radius = 1.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_VERY_LOW: {
				directional_penumbra_shadow_samples = 4;
				directional_soft_shadow_samples = 1;
				directional_shadow_quality_radius = 1.5;
			} break;
			case RS::SHADOW_QUALITY_SOFT_LOW: {
				directional_penumbra_shadow_samples = 8;
				directional_soft_shadow_samples = 4;
				directional_shadow_quality_radius = 2.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_MEDIUM: {
				directional_penumbra_shadow_samples = 12;
				directional_soft_shadow_samples = 8;
				directional_shadow_quality_radius = 2.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_HIGH: {
				directional_penumbra_shadow_samples = 24;
				directional_soft_shadow_samples = 16;
				directional_shadow_quality_radius = 3.0;
			} break;
			case RS::SHADOW_QUALITY_SOFT_ULTRA: {
				directional_penumbra_shadow_samples = 32;
				directional_soft_shadow_samples = 32;
				directional_shadow_quality_radius = 4.0;
			} break;
			case RS::SHADOW_QUALITY_MAX:
				break;
		}
		get_vogel_disk(directional_penumbra_shadow_kernel, directional_penumbra_shadow_samples);
		get_vogel_disk(directional_soft_shadow_kernel, directional_soft_shadow_samples);
	}

	_update_shader_quality_settings();
}

void RendererSceneRenderRD::decals_set_filter(RenderingServer::DecalFilter p_filter) {
	if (decals_filter == p_filter) {
		return;
	}
	decals_filter = p_filter;
	_update_shader_quality_settings();
}
void RendererSceneRenderRD::light_projectors_set_filter(RenderingServer::LightProjectorFilter p_filter) {
	if (light_projectors_filter == p_filter) {
		return;
	}
	light_projectors_filter = p_filter;
	_update_shader_quality_settings();
}

void RendererSceneRenderRD::lightmaps_set_bicubic_filter(bool p_enable) {
	if (lightmap_filter_bicubic == p_enable) {
		return;
	}
	lightmap_filter_bicubic = p_enable;
	_update_shader_quality_settings();
}

int RendererSceneRenderRD::get_roughness_layers() const {
	return sky.roughness_layers;
}

bool RendererSceneRenderRD::is_using_radiance_cubemap_array() const {
	return sky.sky_use_cubemap_array;
}

void RendererSceneRenderRD::_update_vrs(Ref<RenderSceneBuffersRD> p_render_buffers) {
	if (p_render_buffers.is_null()) {
		return;
	}

	RID render_target = p_render_buffers->get_render_target();
	if (render_target.is_null()) {
		// must be rendering reflection probes
		return;
	}

	if (vrs) {
		RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

		RS::ViewportVRSMode vrs_mode = texture_storage->render_target_get_vrs_mode(render_target);
		if (vrs_mode != RS::VIEWPORT_VRS_DISABLED) {
			RID vrs_texture = p_render_buffers->get_texture(RB_SCOPE_VRS, RB_TEXTURE);

			// We use get_cache_multipass instead of get_cache_multiview because the default behavior is for
			// our vrs_texture to be used as the VRS attachment. In this particular case we're writing to it
			// so it needs to be set as our color attachment

			Vector<RID> textures;
			textures.push_back(vrs_texture);

			Vector<RD::FramebufferPass> passes;
			RD::FramebufferPass pass;
			pass.color_attachments.push_back(0);
			passes.push_back(pass);

			RID vrs_fb = FramebufferCacheRD::get_singleton()->get_cache_multipass(textures, passes, p_render_buffers->get_view_count());

			vrs->update_vrs_texture(vrs_fb, p_render_buffers->get_render_target());
		}
	}
}

bool RendererSceneRenderRD::_needs_post_prepass_render(RenderDataRD *p_render_data, bool p_use_gi) {
	if (p_render_data->render_buffers.is_valid()) {
		if (p_render_data->render_buffers->has_custom_data(RB_SCOPE_SDFGI)) {
			return true;
		}
	}
	return false;
}

void RendererSceneRenderRD::_post_prepass_render(RenderDataRD *p_render_data, bool p_use_gi) {
	if (p_render_data->render_buffers.is_valid() && p_use_gi) {
		if (!p_render_data->render_buffers->has_custom_data(RB_SCOPE_SDFGI)) {
			return;
		}

		Ref<RendererRD::GI::SDFGI> sdfgi = p_render_data->render_buffers->get_custom_data(RB_SCOPE_SDFGI);
		sdfgi->update_probes(p_render_data->environment, sky.sky_owner.get_or_null(environment_get_sky(p_render_data->environment)));
	}
}

void RendererSceneRenderRD::render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data, RenderingMethod::RenderInfo *r_render_info) {
	RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
	RendererRD::TextureStorage *texture_storage = RendererRD::TextureStorage::get_singleton();

	// getting this here now so we can direct call a bunch of things more easily
	ERR_FAIL_COND(p_render_buffers.is_null());
	Ref<RenderSceneBuffersRD> rb = p_render_buffers;
	ERR_FAIL_COND(rb.is_null());

	// setup scene data
	RenderSceneDataRD scene_data;
	{
		// Our first camera is used by default
		scene_data.cam_transform = p_camera_data->main_transform;
		scene_data.cam_projection = p_camera_data->main_projection;
		scene_data.cam_orthogonal = p_camera_data->is_orthogonal;
		scene_data.cam_frustum = p_camera_data->is_frustum;
		scene_data.camera_visible_layers = p_camera_data->visible_layers;
		scene_data.taa_jitter = p_camera_data->taa_jitter;
		scene_data.taa_frame_count = p_camera_data->taa_frame_count;
		scene_data.main_cam_transform = p_camera_data->main_transform;
		scene_data.flip_y = !p_reflection_probe.is_valid();

		scene_data.view_count = p_camera_data->view_count;
		for (uint32_t v = 0; v < p_camera_data->view_count; v++) {
			scene_data.view_eye_offset[v] = p_camera_data->view_offset[v].origin;
			scene_data.view_projection[v] = p_camera_data->view_projection[v];
		}

		scene_data.prev_cam_transform = p_prev_camera_data->main_transform;
		scene_data.prev_cam_projection = p_prev_camera_data->main_projection;
		scene_data.prev_taa_jitter = p_prev_camera_data->taa_jitter;

		for (uint32_t v = 0; v < p_camera_data->view_count; v++) {
			scene_data.prev_view_projection[v] = p_prev_camera_data->view_projection[v];
		}

		scene_data.z_near = p_camera_data->main_projection.get_z_near();
		scene_data.z_far = p_camera_data->main_projection.get_z_far();

		// this should be the same for all cameras..
		const float lod_distance_multiplier = p_camera_data->main_projection.get_lod_multiplier();

		// Also, take into account resolution scaling for the multiplier, since we have more leeway with quality
		// degradation visibility. Conversely, allow upwards scaling, too, for increased mesh detail at high res.
		const float scaling_3d_scale = GLOBAL_GET("rendering/scaling_3d/scale");
		scene_data.lod_distance_multiplier = lod_distance_multiplier * (1.0 / scaling_3d_scale);

		if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_DISABLE_LOD) {
			scene_data.screen_mesh_lod_threshold = 0.0;
		} else {
			scene_data.screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
		}

		if (p_shadow_atlas.is_valid()) {
			int shadow_atlas_size = light_storage->shadow_atlas_get_size(p_shadow_atlas);
			scene_data.shadow_atlas_pixel_size.x = 1.0 / shadow_atlas_size;
			scene_data.shadow_atlas_pixel_size.y = 1.0 / shadow_atlas_size;
		}
		{
			int directional_shadow_size = light_storage->directional_shadow_get_size();
			scene_data.directional_shadow_pixel_size.x = 1.0 / directional_shadow_size;
			scene_data.directional_shadow_pixel_size.y = 1.0 / directional_shadow_size;
		}

		scene_data.time = time;
		scene_data.time_step = time_step;
	}

	//assign render data
	RenderDataRD render_data;
	{
		render_data.render_buffers = rb;
		render_data.scene_data = &scene_data;

		render_data.instances = &p_instances;
		render_data.lights = &p_lights;
		render_data.reflection_probes = &p_reflection_probes;
		render_data.voxel_gi_instances = &p_voxel_gi_instances;
		render_data.decals = &p_decals;
		render_data.lightmaps = &p_lightmaps;
		render_data.fog_volumes = &p_fog_volumes;
		render_data.environment = p_environment;
		render_data.compositor = p_compositor;
		render_data.camera_attributes = p_camera_attributes;
		render_data.shadow_atlas = p_shadow_atlas;
		render_data.occluder_debug_tex = p_occluder_debug_tex;
		render_data.reflection_atlas = p_reflection_atlas;
		render_data.reflection_probe = p_reflection_probe;
		render_data.reflection_probe_pass = p_reflection_probe_pass;

		render_data.render_shadows = p_render_shadows;
		render_data.render_shadow_count = p_render_shadow_count;
		render_data.render_sdfgi_regions = p_render_sdfgi_regions;
		render_data.render_sdfgi_region_count = p_render_sdfgi_region_count;
		render_data.sdfgi_update_data = p_sdfgi_update_data;

		render_data.render_info = r_render_info;

		if (p_render_buffers.is_valid() && p_reflection_probe.is_null()) {
			render_data.transparent_bg = texture_storage->render_target_get_transparent(rb->get_render_target());
			render_data.render_region = texture_storage->render_target_get_render_region(rb->get_render_target());
		}
	}

	PagedArray<RID> empty;

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED || get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW) {
		render_data.lights = &empty;
		render_data.reflection_probes = &empty;
		render_data.voxel_gi_instances = &empty;
		render_data.lightmaps = &empty;
	}

	if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED ||
			get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_OVERDRAW ||
			get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_LIGHTING ||
			get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_PSSM_SPLITS) {
		render_data.decals = &empty;
	}

	Color clear_color;
	if (p_render_buffers.is_valid() && p_reflection_probe.is_null()) {
		clear_color = texture_storage->render_target_get_clear_request_color(rb->get_render_target());
	} else {
		clear_color = RSG::texture_storage->get_default_clear_color();
	}

	//calls _pre_opaque_render between depth pre-pass and opaque pass
	_render_scene(&render_data, clear_color);
}

void RendererSceneRenderRD::render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) {
	_render_material(p_cam_transform, p_cam_projection, p_cam_orthogonal, p_instances, p_framebuffer, p_region, 1.0);
}

void RendererSceneRenderRD::render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) {
	RendererRD::ParticlesStorage *particles_storage = RendererRD::ParticlesStorage::get_singleton();

	ERR_FAIL_COND(!particles_storage->particles_collision_is_heightfield(p_collider));
	Vector3 extents = particles_storage->particles_collision_get_extents(p_collider) * p_transform.basis.get_scale();
	Projection cm;
	cm.set_orthogonal(-extents.x, extents.x, -extents.z, extents.z, 0, extents.y * 2.0);

	Vector3 cam_pos = p_transform.origin;
	cam_pos.y += extents.y;

	Transform3D cam_xform;
	cam_xform.set_look_at(cam_pos, cam_pos - p_transform.basis.get_column(Vector3::AXIS_Y), -p_transform.basis.get_column(Vector3::AXIS_Z).normalized());

	RID fb = particles_storage->particles_collision_get_heightfield_framebuffer(p_collider);

	_render_particle_collider_heightfield(fb, cam_xform, cm, p_instances);
}

bool RendererSceneRenderRD::free(RID p_rid) {
	if (is_environment(p_rid)) {
		environment_free(p_rid);
	} else if (is_compositor(p_rid)) {
		compositor_free(p_rid);
	} else if (is_compositor_effect(p_rid)) {
		compositor_effect_free(p_rid);
	} else if (RSG::camera_attributes->owns_camera_attributes(p_rid)) {
		RSG::camera_attributes->camera_attributes_free(p_rid);
	} else if (gi.voxel_gi_instance_owns(p_rid)) {
		gi.voxel_gi_instance_free(p_rid);
	} else if (sky.sky_owner.owns(p_rid)) {
		sky.update_dirty_skys();
		sky.free_sky(p_rid);
	} else if (RendererRD::Fog::get_singleton()->owns_fog_volume_instance(p_rid)) {
		RendererRD::Fog::get_singleton()->fog_instance_free(p_rid);
	} else {
		return false;
	}

	return true;
}

void RendererSceneRenderRD::set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) {
	debug_draw = p_debug_draw;
}

void RendererSceneRenderRD::update() {
	sky.update_dirty_skys();
}

void RendererSceneRenderRD::set_time(double p_time, double p_step) {
	time = p_time;
	time_step = p_step;
}

void RendererSceneRenderRD::screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) {
	screen_space_roughness_limiter = p_enable;
	screen_space_roughness_limiter_amount = p_amount;
	screen_space_roughness_limiter_limit = p_limit;
}

bool RendererSceneRenderRD::screen_space_roughness_limiter_is_active() const {
	return screen_space_roughness_limiter;
}

float RendererSceneRenderRD::screen_space_roughness_limiter_get_amount() const {
	return screen_space_roughness_limiter_amount;
}

float RendererSceneRenderRD::screen_space_roughness_limiter_get_limit() const {
	return screen_space_roughness_limiter_limit;
}

TypedArray<Image> RendererSceneRenderRD::bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) {
	ERR_FAIL_COND_V_MSG(p_image_size.width <= 0, TypedArray<Image>(), "Image width must be greater than 0.");
	ERR_FAIL_COND_V_MSG(p_image_size.height <= 0, TypedArray<Image>(), "Image height must be greater than 0.");
	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	tf.width = p_image_size.width; // Always 64x64
	tf.height = p_image_size.height;
	tf.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

	RID albedo_alpha_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RID normal_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());
	RID orm_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());

	tf.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
	RID emission_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());

	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	RID depth_write_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());

	tf.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	tf.format = RD::get_singleton()->texture_is_format_supported_for_usage(RD::DATA_FORMAT_D32_SFLOAT, RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? RD::DATA_FORMAT_D32_SFLOAT : RD::DATA_FORMAT_X8_D24_UNORM_PACK32;
	RID depth_tex = RD::get_singleton()->texture_create(tf, RD::TextureView());

	Vector<RID> fb_tex;
	fb_tex.push_back(albedo_alpha_tex);
	fb_tex.push_back(normal_tex);
	fb_tex.push_back(orm_tex);
	fb_tex.push_back(emission_tex);
	fb_tex.push_back(depth_write_tex);
	fb_tex.push_back(depth_tex);

	RID fb = RD::get_singleton()->framebuffer_create(fb_tex);

	//RID sampled_light;

	RenderGeometryInstance *gi_inst = geometry_instance_create(p_base);
	ERR_FAIL_NULL_V(gi_inst, TypedArray<Image>());

	uint32_t sc = RSG::mesh_storage->mesh_get_surface_count(p_base);
	Vector<RID> materials;
	materials.resize(sc);

	for (uint32_t i = 0; i < sc; i++) {
		if (i < (uint32_t)p_material_overrides.size()) {
			materials.write[i] = p_material_overrides[i];
		}
	}

	gi_inst->set_surface_materials(materials);

	if (cull_argument.size() == 0) {
		cull_argument.push_back(nullptr);
	}
	cull_argument[0] = gi_inst;
	_render_uv2(cull_argument, fb, Rect2i(0, 0, p_image_size.width, p_image_size.height));

	geometry_instance_free(gi_inst);

	TypedArray<Image> ret;

	{
		PackedByteArray data = RD::get_singleton()->texture_get_data(albedo_alpha_tex, 0);
		Ref<Image> img = Image::create_from_data(p_image_size.width, p_image_size.height, false, Image::FORMAT_RGBA8, data);
		RD::get_singleton()->free(albedo_alpha_tex);
		ret.push_back(img);
	}

	{
		PackedByteArray data = RD::get_singleton()->texture_get_data(normal_tex, 0);
		Ref<Image> img = Image::create_from_data(p_image_size.width, p_image_size.height, false, Image::FORMAT_RGBA8, data);
		RD::get_singleton()->free(normal_tex);
		ret.push_back(img);
	}

	{
		PackedByteArray data = RD::get_singleton()->texture_get_data(orm_tex, 0);
		Ref<Image> img = Image::create_from_data(p_image_size.width, p_image_size.height, false, Image::FORMAT_RGBA8, data);
		RD::get_singleton()->free(orm_tex);
		ret.push_back(img);
	}

	{
		PackedByteArray data = RD::get_singleton()->texture_get_data(emission_tex, 0);
		Ref<Image> img = Image::create_from_data(p_image_size.width, p_image_size.height, false, Image::FORMAT_RGBAH, data);
		RD::get_singleton()->free(emission_tex);
		ret.push_back(img);
	}

	RD::get_singleton()->free(depth_write_tex);
	RD::get_singleton()->free(depth_tex);

	return ret;
}

void RendererSceneRenderRD::sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) {
	gi.sdfgi_debug_probe_pos = p_position;
	gi.sdfgi_debug_probe_dir = p_dir;
}

RendererSceneRenderRD *RendererSceneRenderRD::singleton = nullptr;

bool RendererSceneRenderRD::is_vrs_supported() const {
	return RD::get_singleton()->has_feature(RD::SUPPORTS_ATTACHMENT_VRS);
}

bool RendererSceneRenderRD::is_dynamic_gi_supported() const {
	// usable by default (unless low end = true)
	return true;
}

bool RendererSceneRenderRD::is_volumetric_supported() const {
	// usable by default (unless low end = true)
	return true;
}

uint32_t RendererSceneRenderRD::get_max_elements() const {
	return GLOBAL_GET("rendering/limits/cluster_builder/max_clustered_elements");
}

RendererSceneRenderRD::RendererSceneRenderRD() {
	singleton = this;
}

void RendererSceneRenderRD::init() {
	max_cluster_elements = get_max_elements();
	RendererRD::LightStorage::get_singleton()->set_max_cluster_elements(max_cluster_elements);

	/* Forward ID */
	forward_id_storage = create_forward_id_storage();

	/* Register the include files we make available by default to our users */
	{
		ShaderIncludeDB::register_built_in_include_file("godot/decal_data_inc.glsl", decal_data_inc_shader_glsl);
		ShaderIncludeDB::register_built_in_include_file("godot/light_data_inc.glsl", light_data_inc_shader_glsl);
		ShaderIncludeDB::register_built_in_include_file("godot/scene_data_inc.glsl", scene_data_inc_shader_glsl);
	}

	/* SKY SHADER */

	sky.init();

	/* GI */

	if (is_dynamic_gi_supported()) {
		gi.init(&sky);
	}

	{ //decals
		RendererRD::TextureStorage::get_singleton()->set_max_decals(max_cluster_elements);
	}

	{ //lights
	}

	if (is_volumetric_supported()) {
		RendererRD::Fog::get_singleton()->init_fog_shader(RendererRD::LightStorage::get_singleton()->get_max_directional_lights(), get_roughness_layers(), is_using_radiance_cubemap_array());
	}

	RSG::camera_attributes->camera_attributes_set_dof_blur_bokeh_shape(RS::DOFBokehShape(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_shape"))));
	RSG::camera_attributes->camera_attributes_set_dof_blur_quality(RS::DOFBlurQuality(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_quality"))), GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_use_jitter"));
	use_physical_light_units = GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units");

	screen_space_roughness_limiter = GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/enabled");
	screen_space_roughness_limiter_amount = GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/amount");
	screen_space_roughness_limiter_limit = GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/limit");
	glow_bicubic_upscale = int(GLOBAL_GET("rendering/environment/glow/upscale_mode")) > 0;

	directional_penumbra_shadow_kernel = memnew_arr(float, 128);
	directional_soft_shadow_kernel = memnew_arr(float, 128);
	penumbra_shadow_kernel = memnew_arr(float, 128);
	soft_shadow_kernel = memnew_arr(float, 128);
	positional_soft_shadow_filter_set_quality(RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality"))));
	directional_soft_shadow_filter_set_quality(RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality"))));

	environment_set_volumetric_fog_volume_size(GLOBAL_GET("rendering/environment/volumetric_fog/volume_size"), GLOBAL_GET("rendering/environment/volumetric_fog/volume_depth"));
	environment_set_volumetric_fog_filter_active(GLOBAL_GET("rendering/environment/volumetric_fog/use_filter"));

	decals_set_filter(RS::DecalFilter(int(GLOBAL_GET("rendering/textures/decals/filter"))));
	light_projectors_set_filter(RS::LightProjectorFilter(int(GLOBAL_GET("rendering/textures/light_projectors/filter"))));
	lightmaps_set_bicubic_filter(GLOBAL_GET("rendering/lightmapping/lightmap_gi/use_bicubic_filter"));

	cull_argument.set_page_pool(&cull_argument_pool);

	bool can_use_storage = _render_buffers_can_be_storage();
	bool can_use_vrs = is_vrs_supported();
	bokeh_dof = memnew(RendererRD::BokehDOF(!can_use_storage));
	copy_effects = memnew(RendererRD::CopyEffects(!can_use_storage));
	debug_effects = memnew(RendererRD::DebugEffects);
	luminance = memnew(RendererRD::Luminance(!can_use_storage));
	tone_mapper = memnew(RendererRD::ToneMapper);
	if (can_use_vrs) {
		vrs = memnew(RendererRD::VRS);
	}
	if (can_use_storage) {
		fsr = memnew(RendererRD::FSR);
	}
#ifdef METAL_ENABLED
	mfx_spatial = memnew(RendererRD::MFXSpatialEffect);
#endif
}

RendererSceneRenderRD::~RendererSceneRenderRD() {
	if (forward_id_storage) {
		memdelete(forward_id_storage);
	}

	if (bokeh_dof) {
		memdelete(bokeh_dof);
	}
	if (copy_effects) {
		memdelete(copy_effects);
	}
	if (debug_effects) {
		memdelete(debug_effects);
	}
	if (luminance) {
		memdelete(luminance);
	}
	if (tone_mapper) {
		memdelete(tone_mapper);
	}
	if (vrs) {
		memdelete(vrs);
	}
	if (fsr) {
		memdelete(fsr);
	}
#ifdef METAL_ENABLED
	if (mfx_spatial) {
		memdelete(mfx_spatial);
	}
#endif

	if (sky.sky_scene_state.uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(sky.sky_scene_state.uniform_set)) {
		RD::get_singleton()->free(sky.sky_scene_state.uniform_set);
	}

	if (is_dynamic_gi_supported()) {
		gi.free();
	}

	if (is_volumetric_supported()) {
		RendererRD::Fog::get_singleton()->free_fog_shader();
	}

	memdelete_arr(directional_penumbra_shadow_kernel);
	memdelete_arr(directional_soft_shadow_kernel);
	memdelete_arr(penumbra_shadow_kernel);
	memdelete_arr(soft_shadow_kernel);

	RSG::light_storage->directional_shadow_atlas_set_size(0);
	cull_argument.reset(); //avoid exit error
}
