/**************************************************************************/
/*  renderer_scene_render_rd.h                                            */
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

#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/effects/bokeh_dof.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/effects/debug_effects.h"
#include "servers/rendering/renderer_rd/effects/fsr.h"
#include "servers/rendering/renderer_rd/effects/luminance.h"
#ifdef METAL_ENABLED
#include "servers/rendering/renderer_rd/effects/metal_fx.h"
#endif
#include "servers/rendering/renderer_rd/effects/tone_mapper.h"
#include "servers/rendering/renderer_rd/effects/vrs.h"
#include "servers/rendering/renderer_rd/environment/gi.h"
#include "servers/rendering/renderer_rd/environment/sky.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_method.h"

class RendererSceneRenderRD : public RendererSceneRender {
	friend RendererRD::SkyRD;
	friend RendererRD::GI;

protected:
	RendererRD::ForwardIDStorage *forward_id_storage = nullptr;
	RendererRD::BokehDOF *bokeh_dof = nullptr;
	RendererRD::CopyEffects *copy_effects = nullptr;
	RendererRD::DebugEffects *debug_effects = nullptr;
	RendererRD::Luminance *luminance = nullptr;
	RendererRD::ToneMapper *tone_mapper = nullptr;
	RendererRD::FSR *fsr = nullptr;
	RendererRD::VRS *vrs = nullptr;
#ifdef METAL_ENABLED
	RendererRD::MFXSpatialEffect *mfx_spatial = nullptr;
#endif
	double time = 0.0;
	double time_step = 0.0;

	/* ENVIRONMENT */

	bool glow_bicubic_upscale = false;

	bool use_physical_light_units = false;

	////////////////////////////////

	virtual RendererRD::ForwardIDStorage *create_forward_id_storage() { return memnew(RendererRD::ForwardIDStorage); }

	void _update_vrs(Ref<RenderSceneBuffersRD> p_render_buffers);

	virtual void setup_render_buffer_data(Ref<RenderSceneBuffersRD> p_render_buffers) = 0;

	virtual void _render_scene(RenderDataRD *p_render_data, const Color &p_default_color) = 0;
	virtual void _render_buffers_debug_draw(const RenderDataRD *p_render_data);

	virtual void _render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region, float p_exposure_normalization) = 0;
	virtual void _render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) = 0;
	virtual void _render_sdfgi(Ref<RenderSceneBuffersRD> p_render_buffers, const Vector3i &p_from, const Vector3i &p_size, const AABB &p_bounds, const PagedArray<RenderGeometryInstance *> &p_instances, const RID &p_albedo_texture, const RID &p_emission_texture, const RID &p_emission_aniso_texture, const RID &p_geom_facing_texture, float p_exposure_normalization) = 0;
	virtual void _render_particle_collider_heightfield(RID p_fb, const Transform3D &p_cam_transform, const Projection &p_cam_projection, const PagedArray<RenderGeometryInstance *> &p_instances) = 0;

	void _debug_sdfgi_probes(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_framebuffer, uint32_t p_view_count, const Projection *p_camera_with_transforms);

	virtual RID _render_buffers_get_normal_texture(Ref<RenderSceneBuffersRD> p_render_buffers) = 0;
	virtual RID _render_buffers_get_velocity_texture(Ref<RenderSceneBuffersRD> p_render_buffers) = 0;

	bool _needs_post_prepass_render(RenderDataRD *p_render_data, bool p_use_gi);
	void _post_prepass_render(RenderDataRD *p_render_data, bool p_use_gi);

	bool _compositor_effects_has_flag(const RenderDataRD *p_render_data, RS::CompositorEffectFlags p_flag, RS::CompositorEffectCallbackType p_callback_type = RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_ANY);
	bool _has_compositor_effect(RS::CompositorEffectCallbackType p_callback_type, const RenderDataRD *p_render_data);
	void _process_compositor_effects(RS::CompositorEffectCallbackType p_callback_type, const RenderDataRD *p_render_data);
	void _render_buffers_copy_screen_texture(const RenderDataRD *p_render_data);
	void _render_buffers_copy_depth_texture(const RenderDataRD *p_render_data);
	void _render_buffers_post_process_and_tonemap(const RenderDataRD *p_render_data);
	void _post_process_subpass(RID p_source_texture, RID p_framebuffer, const RenderDataRD *p_render_data);
	void _disable_clear_request(const RenderDataRD *p_render_data);

	// needed for a single argument calls (material and uv2)
	PagedArrayPool<RenderGeometryInstance *> cull_argument_pool;
	PagedArray<RenderGeometryInstance *> cull_argument; //need this to exist

	RendererRD::SkyRD sky;
	RendererRD::GI gi;

	virtual void _update_shader_quality_settings() {}
	static bool _debug_draw_can_use_effects(RS::ViewportDebugDraw p_debug_draw);

private:
	RS::ViewportDebugDraw debug_draw = RS::VIEWPORT_DEBUG_DRAW_DISABLED;
	static RendererSceneRenderRD *singleton;

	/* Shadow atlas */
	RS::ShadowQuality shadows_quality = RS::SHADOW_QUALITY_MAX; //So it always updates when first set
	RS::ShadowQuality directional_shadow_quality = RS::SHADOW_QUALITY_MAX;
	float shadows_quality_radius = 1.0;
	float directional_shadow_quality_radius = 1.0;

	float *directional_penumbra_shadow_kernel = nullptr;
	float *directional_soft_shadow_kernel = nullptr;
	float *penumbra_shadow_kernel = nullptr;
	float *soft_shadow_kernel = nullptr;
	bool lightmap_filter_bicubic = false;
	int directional_penumbra_shadow_samples = 0;
	int directional_soft_shadow_samples = 0;
	int penumbra_shadow_samples = 0;
	int soft_shadow_samples = 0;
	RS::DecalFilter decals_filter = RS::DECAL_FILTER_LINEAR_MIPMAPS;
	RS::LightProjectorFilter light_projectors_filter = RS::LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS;

	/* RENDER BUFFERS */

	/* GI */
	bool screen_space_roughness_limiter = false;
	float screen_space_roughness_limiter_amount = 0.25;
	float screen_space_roughness_limiter_limit = 0.18;

	/* Light data */

	uint64_t scene_pass = 0;

	uint32_t max_cluster_elements = 512;

	/* Volumetric Fog */

	uint32_t volumetric_fog_size = 128;
	uint32_t volumetric_fog_depth = 128;
	bool volumetric_fog_filter_active = true;

public:
	static RendererSceneRenderRD *get_singleton() { return singleton; }

	/* LIGHTING */

	virtual void setup_added_reflection_probe(const Transform3D &p_transform, const Vector3 &p_half_size) {}
	virtual void setup_added_light(const RS::LightType p_type, const Transform3D &p_transform, float p_radius, float p_spot_aperture) {}
	virtual void setup_added_decal(const Transform3D &p_transform, const Vector3 &p_half_size) {}

	/* GI */

	RendererRD::GI *get_gi() { return &gi; }

	/* SKY */

	RendererRD::SkyRD *get_sky() { return &sky; }

	/* SKY API */

	virtual RID sky_allocate() override;
	virtual void sky_initialize(RID p_rid) override;

	virtual void sky_set_radiance_size(RID p_sky, int p_radiance_size) override;
	virtual void sky_set_mode(RID p_sky, RS::SkyMode p_mode) override;
	virtual void sky_set_material(RID p_sky, RID p_material) override;
	virtual Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) override;

	/* ENVIRONMENT API */

	virtual void environment_glow_set_use_bicubic_upscale(bool p_enable) override;

	virtual void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) override;
	virtual void environment_set_volumetric_fog_filter_active(bool p_enable) override;

	virtual void environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) override;
	virtual void environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) override;
	virtual void environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) override;

	virtual Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) override;

	_FORCE_INLINE_ bool is_using_physical_light_units() {
		return use_physical_light_units;
	}

	/* REFLECTION PROBE */

	virtual RID reflection_probe_create_framebuffer(RID p_color, RID p_depth);

	/* FOG VOLUMES */

	uint32_t get_volumetric_fog_size() const { return volumetric_fog_size; }
	uint32_t get_volumetric_fog_depth() const { return volumetric_fog_depth; }
	bool get_volumetric_fog_filter_active() const { return volumetric_fog_filter_active; }

	virtual RID fog_volume_instance_create(RID p_fog_volume) override;
	virtual void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) override;
	virtual void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) override;
	virtual RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const override;
	virtual Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const override;

	/* gi light probes */

	virtual RID voxel_gi_instance_create(RID p_base) override;
	virtual void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) override;
	virtual bool voxel_gi_needs_update(RID p_probe) const override;
	virtual void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) override;
	virtual void voxel_gi_set_quality(RS::VoxelGIQuality p_quality) override { gi.voxel_gi_quality = p_quality; }

	/* render buffers */

	virtual float _render_buffers_get_luminance_multiplier();
	virtual RD::DataFormat _render_buffers_get_color_format();
	virtual bool _render_buffers_can_be_storage();
	virtual Ref<RenderSceneBuffers> render_buffers_create() override;
	virtual void gi_set_use_half_resolution(bool p_enable) override;

	RID render_buffers_get_default_voxel_gi_buffer();

	virtual void base_uniforms_changed() = 0;

	virtual void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingMethod::RenderInfo *r_render_info = nullptr) override;

	virtual void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;

	virtual void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) override;

	virtual void set_scene_pass(uint64_t p_pass) override {
		scene_pass = p_pass;
	}
	_FORCE_INLINE_ uint64_t get_scene_pass() {
		return scene_pass;
	}

	virtual void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) override;
	virtual bool screen_space_roughness_limiter_is_active() const override;
	virtual float screen_space_roughness_limiter_get_amount() const;
	virtual float screen_space_roughness_limiter_get_limit() const;

	virtual void positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;
	virtual void directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;

	virtual void decals_set_filter(RS::DecalFilter p_filter) override;
	virtual void light_projectors_set_filter(RS::LightProjectorFilter p_filter) override;
	virtual void lightmaps_set_bicubic_filter(bool p_enable) override;

	_FORCE_INLINE_ RS::ShadowQuality shadows_quality_get() const {
		return shadows_quality;
	}
	_FORCE_INLINE_ RS::ShadowQuality directional_shadow_quality_get() const {
		return directional_shadow_quality;
	}
	_FORCE_INLINE_ float shadows_quality_radius_get() const {
		return shadows_quality_radius;
	}
	_FORCE_INLINE_ float directional_shadow_quality_radius_get() const {
		return directional_shadow_quality_radius;
	}

	_FORCE_INLINE_ float *directional_penumbra_shadow_kernel_get() {
		return directional_penumbra_shadow_kernel;
	}
	_FORCE_INLINE_ float *directional_soft_shadow_kernel_get() {
		return directional_soft_shadow_kernel;
	}
	_FORCE_INLINE_ float *penumbra_shadow_kernel_get() {
		return penumbra_shadow_kernel;
	}
	_FORCE_INLINE_ float *soft_shadow_kernel_get() {
		return soft_shadow_kernel;
	}

	_FORCE_INLINE_ int directional_penumbra_shadow_samples_get() const {
		return directional_penumbra_shadow_samples;
	}
	_FORCE_INLINE_ bool lightmap_filter_bicubic_get() const {
		return lightmap_filter_bicubic;
	}
	_FORCE_INLINE_ int directional_soft_shadow_samples_get() const {
		return directional_soft_shadow_samples;
	}
	_FORCE_INLINE_ int penumbra_shadow_samples_get() const {
		return penumbra_shadow_samples;
	}
	_FORCE_INLINE_ int soft_shadow_samples_get() const {
		return soft_shadow_samples;
	}

	_FORCE_INLINE_ RS::LightProjectorFilter light_projectors_get_filter() const {
		return light_projectors_filter;
	}
	_FORCE_INLINE_ RS::DecalFilter decals_get_filter() const {
		return decals_filter;
	}

	int get_roughness_layers() const;
	bool is_using_radiance_cubemap_array() const;

	virtual TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) override;

	virtual bool free(RID p_rid) override;

	virtual void update() override;

	virtual void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) override;
	_FORCE_INLINE_ RS::ViewportDebugDraw get_debug_draw_mode() const {
		return debug_draw;
	}

	virtual void set_time(double p_time, double p_step) override;

	virtual void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;

	virtual bool is_vrs_supported() const;
	virtual bool is_dynamic_gi_supported() const;
	virtual bool is_volumetric_supported() const;
	virtual uint32_t get_max_elements() const;

	void init();

	RendererSceneRenderRD();
	~RendererSceneRenderRD();
};
