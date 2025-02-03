/**************************************************************************/
/*  rasterizer.h                                                          */
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

#ifndef RASTERIZER_H
#define RASTERIZER_H

#include "core/math/camera_matrix.h"
#include "core/math/transform_interpolator.h"
#include "servers/visual_server.h"

#include "core/self_list.h"

class RasterizerScene {
public:
	/* SHADOW ATLAS API */

	virtual RID shadow_atlas_create() = 0;
	virtual void shadow_atlas_set_size(RID p_atlas, int p_size, bool p_16_bits = true) = 0;
	virtual void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) = 0;
	virtual bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) = 0;

	virtual int get_directional_light_shadow_size(RID p_light_intance) = 0;
	virtual void set_directional_shadow_count(int p_count) = 0;

	/* ENVIRONMENT API */

	virtual RID environment_create() = 0;

	virtual void environment_set_background(RID p_env, VS::EnvironmentBG p_bg) = 0;
	virtual void environment_set_sky(RID p_env, RID p_sky) = 0;
	virtual void environment_set_sky_custom_fov(RID p_env, float p_scale) = 0;
	virtual void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) = 0;
	virtual void environment_set_bg_color(RID p_env, const Color &p_color) = 0;
	virtual void environment_set_bg_energy(RID p_env, float p_energy) = 0;
	virtual void environment_set_canvas_max_layer(RID p_env, int p_max_layer) = 0;
	virtual void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy = 1.0, float p_sky_contribution = 0.0) = 0;
	virtual void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) = 0;

	virtual void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) = 0;
	virtual void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) = 0;
	virtual void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) = 0;
	virtual void environment_set_glow_map(RID p_env, float p_glow_map_strength, RID p_glow_map) = 0;
	virtual void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) = 0;

	virtual void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) = 0;
	virtual void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) = 0;

	virtual void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) = 0;

	virtual void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) = 0;

	virtual void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) = 0;
	virtual void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) = 0;
	virtual void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) = 0;

	virtual bool is_environment(RID p_env) = 0;
	virtual VS::EnvironmentBG environment_get_background(RID p_env) = 0;
	virtual int environment_get_canvas_max_layer(RID p_env) = 0;

	struct InstanceBase : RID_Data {
		VS::InstanceType base_type;
		RID base;

		RID skeleton;
		RID material_override;
		RID material_overlay;

		// This is the main transform to be drawn with ..
		// This will either be the interpolated transform (when using fixed timestep interpolation)
		// or the ONLY transform (when not using FTI).
		Transform transform;

		// for interpolation we store the current transform (this physics tick)
		// and the transform in the previous tick
		Transform transform_curr;
		Transform transform_prev;

		int depth_layer;
		uint32_t layer_mask;

		//RID sampled_light;

		Vector<RID> materials;
		Vector<RID> light_instances;
		Vector<RID> reflection_probe_instances;
		Vector<RID> gi_probe_instances;

		PoolVector<float> blend_values;

		VS::ShadowCastingSetting cast_shadows;

		//fit in 32 bits
		bool mirror : 1;
		bool receive_shadows : 1;
		bool visible : 1;
		bool baked_light : 1; //this flag is only to know if it actually did use baked light
		bool redraw_if_visible : 1;

		bool on_interpolate_list : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		TransformInterpolator::Method interpolation_method : 3;

		// For fixed timestep interpolation.
		// Note 32 bits is plenty for checksum, no need for real_t
		float transform_checksum_curr;
		float transform_checksum_prev;

		float depth; //used for sorting

		SelfList<InstanceBase> dependency_item;

		InstanceBase *lightmap_capture;
		RID lightmap;
		Vector<Color> lightmap_capture_data; //in a array (12 values) to avoid wasting space if unused. Alpha is unused, but needed to send to shader
		int lightmap_slice;
		Rect2 lightmap_uv_rect;

		virtual void base_removed() = 0;
		virtual void base_changed(bool p_aabb, bool p_materials) = 0;
		InstanceBase() :
				dependency_item(this) {
			base_type = VS::INSTANCE_NONE;
			cast_shadows = VS::SHADOW_CASTING_SETTING_ON;
			receive_shadows = true;
			visible = true;
			depth_layer = 0;
			layer_mask = 1;
			baked_light = false;
			redraw_if_visible = false;
			lightmap_capture = nullptr;
			lightmap_slice = -1;
			lightmap_uv_rect = Rect2(0, 0, 1, 1);
			on_interpolate_list = false;
			on_interpolate_transform_list = false;
			interpolated = true;
			interpolation_method = TransformInterpolator::INTERP_LERP;
			transform_checksum_curr = 0.0;
			transform_checksum_prev = 0.0;
		}
	};

	virtual RID light_instance_create(RID p_light) = 0;
	virtual void light_instance_set_transform(RID p_light_instance, const Transform &p_transform) = 0;
	virtual void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) = 0;
	virtual void light_instance_mark_visible(RID p_light_instance) = 0;
	virtual bool light_instances_can_render_shadow_cube() const { return true; }

	virtual RID reflection_atlas_create() = 0;
	virtual void reflection_atlas_set_size(RID p_ref_atlas, int p_size) = 0;
	virtual void reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv) = 0;

	virtual RID reflection_probe_instance_create(RID p_probe) = 0;
	virtual void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) = 0;
	virtual void reflection_probe_release_atlas_index(RID p_instance) = 0;
	virtual bool reflection_probe_instance_needs_redraw(RID p_instance) = 0;
	virtual bool reflection_probe_instance_has_reflection(RID p_instance) = 0;
	virtual bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) = 0;
	virtual bool reflection_probe_instance_postprocess_step(RID p_instance) = 0;

	virtual RID gi_probe_instance_create() = 0;
	virtual void gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data) = 0;
	virtual void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) = 0;
	virtual void gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds) = 0;

	virtual void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) = 0;
	virtual void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) = 0;

	virtual void set_scene_pass(uint64_t p_pass) = 0;
	virtual void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) = 0;

	virtual bool free(RID p_rid) = 0;

	virtual ~RasterizerScene() {}
};

class RasterizerStorage {
public:
	/* TEXTURE API */

	virtual RID texture_create() = 0;
	virtual void texture_allocate(RID p_texture,
			int p_width,
			int p_height,
			int p_depth_3d,
			Image::Format p_format,
			VS::TextureType p_type,
			uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT) = 0;

	virtual void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_level = 0) = 0;

	virtual void texture_set_data_partial(RID p_texture,
			const Ref<Image> &p_image,
			int src_x, int src_y,
			int src_w, int src_h,
			int dst_x, int dst_y,
			int p_dst_mip,
			int p_level = 0) = 0;

	virtual Ref<Image> texture_get_data(RID p_texture, int p_level = 0) const = 0;
	virtual void texture_set_flags(RID p_texture, uint32_t p_flags) = 0;
	virtual uint32_t texture_get_flags(RID p_texture) const = 0;
	virtual Image::Format texture_get_format(RID p_texture) const = 0;
	virtual VS::TextureType texture_get_type(RID p_texture) const = 0;
	virtual uint32_t texture_get_texid(RID p_texture) const = 0;
	virtual uint32_t texture_get_width(RID p_texture) const = 0;
	virtual uint32_t texture_get_height(RID p_texture) const = 0;
	virtual uint32_t texture_get_depth(RID p_texture) const = 0;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d) = 0;
	virtual void texture_bind(RID p_texture, uint32_t p_texture_no) = 0;

	virtual void texture_set_path(RID p_texture, const String &p_path) = 0;
	virtual String texture_get_path(RID p_texture) const = 0;

	virtual void texture_set_shrink_all_x2_on_set_data(bool p_enable) = 0;

	virtual void texture_debug_usage(List<VS::TextureInfo> *r_info) = 0;

	virtual RID texture_create_radiance_cubemap(RID p_source, int p_resolution = -1) const = 0;

	virtual void texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) = 0;
	virtual void texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) = 0;
	virtual void texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata) = 0;

	virtual void textures_keep_original(bool p_enable) = 0;

	virtual void texture_set_proxy(RID p_proxy, RID p_base) = 0;
	virtual Size2 texture_size_with_proxy(RID p_texture) const = 0;
	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) = 0;

	/* SKY API */

	virtual RID sky_create() = 0;
	virtual void sky_set_texture(RID p_sky, RID p_cube_map, int p_radiance_size) = 0;

	/* SHADER API */

	virtual RID shader_create() = 0;

	virtual void shader_set_code(RID p_shader, const String &p_code) = 0;
	virtual String shader_get_code(RID p_shader) const = 0;
	virtual void shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const = 0;

	virtual void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) = 0;
	virtual RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const = 0;

	virtual void shader_add_custom_define(RID p_shader, const String &p_define) = 0;
	virtual void shader_get_custom_defines(RID p_shader, Vector<String> *p_defines) const = 0;
	virtual void shader_remove_custom_define(RID p_shader, const String &p_define) = 0;

	virtual void set_shader_async_hidden_forbidden(bool p_forbidden) = 0;
	virtual bool is_shader_async_hidden_forbidden() = 0;

	/* COMMON MATERIAL API */

	virtual RID material_create() = 0;

	virtual void material_set_render_priority(RID p_material, int priority) = 0;
	virtual void material_set_shader(RID p_shader_material, RID p_shader) = 0;
	virtual RID material_get_shader(RID p_shader_material) const = 0;

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) = 0;
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const = 0;
	virtual Variant material_get_param_default(RID p_material, const StringName &p_param) const = 0;

	virtual void material_set_line_width(RID p_material, float p_width) = 0;

	virtual void material_set_next_pass(RID p_material, RID p_next_material) = 0;

	virtual bool material_is_animated(RID p_material) = 0;
	virtual bool material_casts_shadows(RID p_material) = 0;
	virtual bool material_uses_tangents(RID p_material);
	virtual bool material_uses_ensure_correct_normals(RID p_material);

	virtual void material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) = 0;
	virtual void material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) = 0;

	/* MESH API */

	virtual RID mesh_create() = 0;

	virtual void mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>()) = 0;

	virtual void mesh_set_blend_shape_count(RID p_mesh, int p_amount) = 0;
	virtual int mesh_get_blend_shape_count(RID p_mesh) const = 0;

	virtual void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) = 0;
	virtual VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const = 0;

	virtual void mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values) = 0;
	virtual PoolVector<float> mesh_get_blend_shape_values(RID p_mesh) const = 0;

	virtual void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) = 0;

	virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) = 0;
	virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const = 0;

	virtual int mesh_surface_get_array_len(RID p_mesh, int p_surface) const = 0;
	virtual int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const = 0;

	virtual PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const = 0;
	virtual PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const = 0;

	virtual uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const = 0;
	virtual VS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const = 0;

	virtual AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const = 0;
	virtual Vector<PoolVector<uint8_t>> mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const = 0;
	virtual Vector<AABB> mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const = 0;

	virtual void mesh_remove_surface(RID p_mesh, int p_index) = 0;
	virtual int mesh_get_surface_count(RID p_mesh) const = 0;

	virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) = 0;
	virtual AABB mesh_get_custom_aabb(RID p_mesh) const = 0;

	virtual AABB mesh_get_aabb(RID p_mesh, RID p_skeleton) const = 0;

	virtual void mesh_clear(RID p_mesh) = 0;

	/* MULTIMESH API */
	struct MMInterpolator {
		VS::MultimeshTransformFormat _transform_format = VS::MULTIMESH_TRANSFORM_3D;
		VS::MultimeshColorFormat _color_format = VS::MULTIMESH_COLOR_NONE;
		VS::MultimeshCustomDataFormat _data_format = VS::MULTIMESH_CUSTOM_DATA_NONE;

		// in floats
		int _stride = 0;

		// Vertex format sizes in floats
		int _vf_size_xform = 0;
		int _vf_size_color = 0;
		int _vf_size_data = 0;

		// Set by allocate, can be used to prevent indexing out of range.
		int _num_instances = 0;

		// Quality determines whether to use lerp or slerp etc.
		int quality = 0;
		bool interpolated = false;
		bool on_interpolate_update_list = false;
		bool on_transform_update_list = false;

		PoolVector<float> _data_prev;
		PoolVector<float> _data_curr;
		PoolVector<float> _data_interpolated;
	};

	virtual RID multimesh_create();
	virtual void multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data = VS::MULTIMESH_CUSTOM_DATA_NONE);
	virtual int multimesh_get_instance_count(RID p_multimesh) const;
	virtual void multimesh_set_mesh(RID p_multimesh, RID p_mesh);
	virtual void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform);
	virtual void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform);
	virtual void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color);
	virtual void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color);
	virtual RID multimesh_get_mesh(RID p_multimesh) const;
	virtual Transform multimesh_instance_get_transform(RID p_multimesh, int p_index) const;
	virtual Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const;
	virtual Color multimesh_instance_get_color(RID p_multimesh, int p_index) const;
	virtual Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const;
	virtual void multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array);

	virtual void multimesh_set_as_bulk_array_interpolated(RID p_multimesh, const PoolVector<float> &p_array, const PoolVector<float> &p_array_prev);
	virtual void multimesh_set_physics_interpolated(RID p_multimesh, bool p_interpolated);
	virtual void multimesh_set_physics_interpolation_quality(RID p_multimesh, VS::MultimeshPhysicsInterpolationQuality p_quality);
	virtual void multimesh_instance_reset_physics_interpolation(RID p_multimesh, int p_index);

	virtual void multimesh_set_visible_instances(RID p_multimesh, int p_visible);
	virtual int multimesh_get_visible_instances(RID p_multimesh) const;
	virtual AABB multimesh_get_aabb(RID p_multimesh) const;
	virtual void multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) = 0;

	virtual RID _multimesh_create() = 0;
	virtual void _multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data = VS::MULTIMESH_CUSTOM_DATA_NONE) = 0;
	virtual int _multimesh_get_instance_count(RID p_multimesh) const = 0;
	virtual void _multimesh_set_mesh(RID p_multimesh, RID p_mesh) = 0;
	virtual void _multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) = 0;
	virtual void _multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) = 0;
	virtual void _multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) = 0;
	virtual void _multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) = 0;
	virtual RID _multimesh_get_mesh(RID p_multimesh) const = 0;
	virtual Transform _multimesh_instance_get_transform(RID p_multimesh, int p_index) const = 0;
	virtual Transform2D _multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const = 0;
	virtual Color _multimesh_instance_get_color(RID p_multimesh, int p_index) const = 0;
	virtual Color _multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const = 0;
	virtual void _multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array) = 0;
	virtual void _multimesh_set_visible_instances(RID p_multimesh, int p_visible) = 0;
	virtual int _multimesh_get_visible_instances(RID p_multimesh) const = 0;
	virtual AABB _multimesh_get_aabb(RID p_multimesh) const = 0;

	// Multimesh is responsible for allocating / destroying an MMInterpolator object.
	// This allows shared functionality for interpolation across backends.
	virtual MMInterpolator *_multimesh_get_interpolator(RID p_multimesh) const = 0;

private:
	void _multimesh_add_to_interpolation_lists(RID p_multimesh, MMInterpolator &r_mmi);

public:
	/* IMMEDIATE API */

	virtual RID immediate_create() = 0;
	virtual void immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID()) = 0;
	virtual void immediate_vertex(RID p_immediate, const Vector3 &p_vertex) = 0;
	virtual void immediate_normal(RID p_immediate, const Vector3 &p_normal) = 0;
	virtual void immediate_tangent(RID p_immediate, const Plane &p_tangent) = 0;
	virtual void immediate_color(RID p_immediate, const Color &p_color) = 0;
	virtual void immediate_uv(RID p_immediate, const Vector2 &tex_uv) = 0;
	virtual void immediate_uv2(RID p_immediate, const Vector2 &tex_uv) = 0;
	virtual void immediate_end(RID p_immediate) = 0;
	virtual void immediate_clear(RID p_immediate) = 0;
	virtual void immediate_set_material(RID p_immediate, RID p_material) = 0;
	virtual RID immediate_get_material(RID p_immediate) const = 0;
	virtual AABB immediate_get_aabb(RID p_immediate) const = 0;

	/* SKELETON API */

	virtual RID skeleton_create() = 0;
	virtual void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) = 0;
	virtual int skeleton_get_bone_count(RID p_skeleton) const = 0;
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) = 0;
	virtual Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const = 0;
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) = 0;
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const = 0;
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) = 0;
	virtual uint32_t skeleton_get_revision(RID p_skeleton) const = 0;
	virtual void skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) = 0;

	/* Light API */

	virtual RID light_create(VS::LightType p_type) = 0;

	RID directional_light_create() { return light_create(VS::LIGHT_DIRECTIONAL); }
	RID omni_light_create() { return light_create(VS::LIGHT_OMNI); }
	RID spot_light_create() { return light_create(VS::LIGHT_SPOT); }

	virtual void light_set_color(RID p_light, const Color &p_color) = 0;
	virtual void light_set_param(RID p_light, VS::LightParam p_param, float p_value) = 0;
	virtual void light_set_shadow(RID p_light, bool p_enabled) = 0;
	virtual void light_set_shadow_color(RID p_light, const Color &p_color) = 0;
	virtual void light_set_projector(RID p_light, RID p_texture) = 0;
	virtual void light_set_negative(RID p_light, bool p_enable) = 0;
	virtual void light_set_cull_mask(RID p_light, uint32_t p_mask) = 0;
	virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) = 0;
	virtual void light_set_use_gi(RID p_light, bool p_enable) = 0;
	virtual void light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode) = 0;

	virtual void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) = 0;
	virtual void light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) = 0;

	virtual void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) = 0;
	virtual void light_directional_set_blend_splits(RID p_light, bool p_enable) = 0;
	virtual bool light_directional_get_blend_splits(RID p_light) const = 0;
	virtual void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) = 0;
	virtual VS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const = 0;

	virtual VS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) = 0;
	virtual VS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) = 0;

	virtual bool light_has_shadow(RID p_light) const = 0;

	virtual VS::LightType light_get_type(RID p_light) const = 0;
	virtual AABB light_get_aabb(RID p_light) const = 0;
	virtual float light_get_param(RID p_light, VS::LightParam p_param) = 0;
	virtual Color light_get_color(RID p_light) = 0;
	virtual bool light_get_use_gi(RID p_light) = 0;
	virtual VS::LightBakeMode light_get_bake_mode(RID p_light) = 0;
	virtual uint64_t light_get_version(RID p_light) const = 0;

	/* PROBE API */

	virtual RID reflection_probe_create() = 0;

	virtual void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) = 0;
	virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution) = 0;
	virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity) = 0;
	virtual void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) = 0;
	virtual void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) = 0;
	virtual void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) = 0;
	virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance) = 0;
	virtual void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) = 0;
	virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) = 0;
	virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) = 0;

	virtual AABB reflection_probe_get_aabb(RID p_probe) const = 0;
	virtual VS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const = 0;
	virtual uint32_t reflection_probe_get_cull_mask(RID p_probe) const = 0;
	virtual Vector3 reflection_probe_get_extents(RID p_probe) const = 0;
	virtual Vector3 reflection_probe_get_origin_offset(RID p_probe) const = 0;
	virtual float reflection_probe_get_origin_max_distance(RID p_probe) const = 0;
	virtual bool reflection_probe_renders_shadows(RID p_probe) const = 0;

	virtual void instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) = 0;
	virtual void instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) = 0;

	virtual void instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) = 0;
	virtual void instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) = 0;

	/* GI PROBE API */

	virtual RID gi_probe_create() = 0;

	virtual void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) = 0;
	virtual AABB gi_probe_get_bounds(RID p_probe) const = 0;

	virtual void gi_probe_set_cell_size(RID p_probe, float p_range) = 0;
	virtual float gi_probe_get_cell_size(RID p_probe) const = 0;

	virtual void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) = 0;
	virtual Transform gi_probe_get_to_cell_xform(RID p_probe) const = 0;

	virtual void gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data) = 0;
	virtual PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const = 0;

	virtual void gi_probe_set_dynamic_range(RID p_probe, int p_range) = 0;
	virtual int gi_probe_get_dynamic_range(RID p_probe) const = 0;

	virtual void gi_probe_set_energy(RID p_probe, float p_range) = 0;
	virtual float gi_probe_get_energy(RID p_probe) const = 0;

	virtual void gi_probe_set_bias(RID p_probe, float p_range) = 0;
	virtual float gi_probe_get_bias(RID p_probe) const = 0;

	virtual void gi_probe_set_normal_bias(RID p_probe, float p_range) = 0;
	virtual float gi_probe_get_normal_bias(RID p_probe) const = 0;

	virtual void gi_probe_set_propagation(RID p_probe, float p_range) = 0;
	virtual float gi_probe_get_propagation(RID p_probe) const = 0;

	virtual void gi_probe_set_interior(RID p_probe, bool p_enable) = 0;
	virtual bool gi_probe_is_interior(RID p_probe) const = 0;

	virtual void gi_probe_set_compress(RID p_probe, bool p_enable) = 0;
	virtual bool gi_probe_is_compressed(RID p_probe) const = 0;

	virtual uint32_t gi_probe_get_version(RID p_probe) = 0;

	enum GIProbeCompression {
		GI_PROBE_UNCOMPRESSED,
		GI_PROBE_S3TC,
		GI_PROBE_ETC2
	};

	virtual RID gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) = 0;
	virtual void gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) = 0;

	/* LIGHTMAP CAPTURE */

	struct LightmapCaptureOctree {
		enum {
			CHILD_EMPTY = 0xFFFFFFFF
		};

		uint16_t light[6][3]; //anisotropic light
		float alpha;
		uint32_t children[8];
	};

	virtual RID lightmap_capture_create() = 0;
	virtual void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) = 0;
	virtual AABB lightmap_capture_get_bounds(RID p_capture) const = 0;
	virtual void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) = 0;
	virtual PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const = 0;
	virtual void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) = 0;
	virtual Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const = 0;
	virtual void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) = 0;
	virtual int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const = 0;
	virtual void lightmap_capture_set_energy(RID p_capture, float p_energy) = 0;
	virtual float lightmap_capture_get_energy(RID p_capture) const = 0;
	virtual void lightmap_capture_set_interior(RID p_capture, bool p_interior) = 0;
	virtual bool lightmap_capture_is_interior(RID p_capture) const = 0;
	virtual const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const = 0;

	/* PARTICLES */

	virtual RID particles_create() = 0;

	virtual void particles_set_emitting(RID p_particles, bool p_emitting) = 0;
	virtual bool particles_get_emitting(RID p_particles) = 0;

	virtual void particles_set_amount(RID p_particles, int p_amount) = 0;
	virtual void particles_set_lifetime(RID p_particles, float p_lifetime) = 0;
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) = 0;
	virtual void particles_set_pre_process_time(RID p_particles, float p_time) = 0;
	virtual void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) = 0;
	virtual void particles_set_randomness_ratio(RID p_particles, float p_ratio) = 0;
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) = 0;
	virtual void particles_set_speed_scale(RID p_particles, float p_scale) = 0;
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_process_material(RID p_particles, RID p_material) = 0;
	virtual void particles_set_fixed_fps(RID p_particles, int p_fps) = 0;
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) = 0;
	virtual void particles_restart(RID p_particles) = 0;

	virtual bool particles_is_inactive(RID p_particles) const = 0;

	virtual void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) = 0;

	virtual void particles_set_draw_passes(RID p_particles, int p_count) = 0;
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) = 0;

	virtual void particles_request_process(RID p_particles) = 0;
	virtual AABB particles_get_current_aabb(RID p_particles) = 0;
	virtual AABB particles_get_aabb(RID p_particles) const = 0;

	virtual void particles_set_emission_transform(RID p_particles, const Transform &p_transform) = 0;

	virtual int particles_get_draw_passes(RID p_particles) const = 0;
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const = 0;

	/* RENDER TARGET */

	enum RenderTargetFlags {
		RENDER_TARGET_VFLIP,
		RENDER_TARGET_TRANSPARENT,
		RENDER_TARGET_NO_3D_EFFECTS,
		RENDER_TARGET_NO_3D,
		RENDER_TARGET_NO_SAMPLING,
		RENDER_TARGET_HDR,
		RENDER_TARGET_KEEP_3D_LINEAR,
		RENDER_TARGET_DIRECT_TO_SCREEN,
		RENDER_TARGET_USE_32_BPC_DEPTH,
		RENDER_TARGET_FLAG_MAX
	};

	virtual RID render_target_create() = 0;
	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) = 0;
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height) = 0;
	virtual RID render_target_get_texture(RID p_render_target) const = 0;
	virtual uint32_t render_target_get_depth_texture_id(RID p_render_target) const = 0;
	virtual void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) = 0;
	virtual void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) = 0;
	virtual bool render_target_was_used(RID p_render_target) = 0;
	virtual void render_target_clear_used(RID p_render_target) = 0;
	virtual void render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) = 0;
	virtual void render_target_set_use_fxaa(RID p_render_target, bool p_fxaa) = 0;
	virtual void render_target_set_use_debanding(RID p_render_target, bool p_debanding) = 0;
	virtual void render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) = 0;

	/* CANVAS SHADOW */

	virtual RID canvas_light_shadow_buffer_create(int p_width) = 0;

	/* LIGHT SHADOW MAPPING */

	virtual RID canvas_light_occluder_create() = 0;
	virtual void canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines) = 0;

	/* INTERPOLATION */
	struct InterpolationData {
		void notify_free_multimesh(RID p_rid);
		LocalVector<RID> multimesh_interpolate_update_list;
		LocalVector<RID> multimesh_transform_update_lists[2];
		LocalVector<RID> *multimesh_transform_update_list_curr = &multimesh_transform_update_lists[0];
		LocalVector<RID> *multimesh_transform_update_list_prev = &multimesh_transform_update_lists[1];
	} _interpolation_data;

	void update_interpolation_tick(bool p_process = true);
	void update_interpolation_frame(bool p_process = true);

private:
	_FORCE_INLINE_ void _interpolate_RGBA8(const uint8_t *p_a, const uint8_t *p_b, uint8_t *r_dest, float p_f) const;

public:
	virtual VS::InstanceType get_base_type(RID p_rid) const = 0;
	virtual bool free(RID p_rid) = 0;

	virtual bool has_os_feature(const String &p_feature) const = 0;

	virtual void update_dirty_resources() = 0;

	virtual void set_debug_generate_wireframes(bool p_generate) = 0;

	virtual void render_info_begin_capture() = 0;
	virtual void render_info_end_capture() = 0;
	virtual int get_captured_render_info(VS::RenderInfo p_info) = 0;

	virtual uint64_t get_render_info(VS::RenderInfo p_info) = 0;
	virtual String get_video_adapter_name() const = 0;
	virtual String get_video_adapter_vendor() const = 0;

	static RasterizerStorage *base_singleton;
	RasterizerStorage();
	virtual ~RasterizerStorage() {}
};

class RasterizerCanvas {
public:
	enum CanvasRectFlags {

		CANVAS_RECT_REGION = 1,
		CANVAS_RECT_TILE = 2,
		CANVAS_RECT_FLIP_H = 4,
		CANVAS_RECT_FLIP_V = 8,
		CANVAS_RECT_TRANSPOSE = 16,
		CANVAS_RECT_CLIP_UV = 32
	};

	struct Light : public RID_Data {
		bool enabled : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		Color color;
		Transform2D xform_curr;
		Transform2D xform_prev;
		float height;
		float energy;
		float scale;
		int z_min;
		int z_max;
		int layer_min;
		int layer_max;
		int item_mask;
		int item_shadow_mask;
		VS::CanvasLightMode mode;
		RID texture;
		Vector2 texture_offset;
		RID canvas;
		RID shadow_buffer;
		int shadow_buffer_size;
		float shadow_gradient_length;
		VS::CanvasLightShadowFilter shadow_filter;
		Color shadow_color;
		float shadow_smooth;

		void *texture_cache; // implementation dependent
		Rect2 rect_cache;
		Transform2D xform_cache;
		float radius_cache; //used for shadow far plane
		CameraMatrix shadow_matrix_cache;

		Transform2D light_shader_xform;
		Vector2 light_shader_pos;

		Light *shadows_next_ptr;
		Light *filter_next_ptr;
		Light *next_ptr;
		Light *mask_next_ptr;

		RID light_internal;

		Light() {
			enabled = true;
			on_interpolate_transform_list = false;
			interpolated = true;
			color = Color(1, 1, 1);
			shadow_color = Color(0, 0, 0, 0);
			height = 0;
			z_min = -1024;
			z_max = 1024;
			layer_min = 0;
			layer_max = 0;
			item_mask = 1;
			scale = 1.0;
			energy = 1.0;
			item_shadow_mask = 1;
			mode = VS::CANVAS_LIGHT_MODE_ADD;
			texture_cache = nullptr;
			next_ptr = nullptr;
			mask_next_ptr = nullptr;
			filter_next_ptr = nullptr;
			shadow_buffer_size = 2048;
			shadow_gradient_length = 0;
			shadow_filter = VS::CANVAS_LIGHT_FILTER_NONE;
			shadow_smooth = 0.0;
		}
	};

	virtual RID light_internal_create() = 0;
	virtual void light_internal_update(RID p_rid, Light *p_light) = 0;
	virtual void light_internal_free(RID p_rid) = 0;

	struct Item : public RID_Data {
		struct Command {
			enum Type {

				TYPE_LINE,
				TYPE_POLYLINE,
				TYPE_RECT,
				TYPE_NINEPATCH,
				TYPE_PRIMITIVE,
				TYPE_POLYGON,
				TYPE_MESH,
				TYPE_MULTIMESH,
				TYPE_PARTICLES,
				TYPE_CIRCLE,
				TYPE_TRANSFORM,
				TYPE_CLIP_IGNORE,
				TYPE_MULTIRECT,
			};

			virtual bool contains_reference(const RID &p_rid) const { return false; }

			Type type;
			virtual ~Command() {}
		};

		struct CommandLine : public Command {
			Point2 from, to;
			Color color;
			float width;
			bool antialiased;
			CommandLine() { type = TYPE_LINE; }
		};
		struct CommandPolyLine : public Command {
			bool antialiased;
			bool multiline;
			Vector<Point2> triangles;
			Vector<Color> triangle_colors;
			Vector<Point2> lines;
			Vector<Color> line_colors;
			CommandPolyLine() {
				type = TYPE_POLYLINE;
				antialiased = false;
				multiline = false;
			}
		};

		struct CommandRect : public Command {
			Rect2 rect;
			RID texture;
			RID normal_map;
			Color modulate;
			Rect2 source;
			uint8_t flags;

			CommandRect() {
				flags = 0;
				type = TYPE_RECT;
			}
		};

		struct CommandMultiRect : public Command {
			RID texture;
			RID normal_map;
			Color modulate;
			Vector<Rect2> rects;
			Vector<Rect2> sources;
			uint8_t flags;

			CommandMultiRect() {
				flags = 0;
				type = TYPE_MULTIRECT;
			}
		};

		struct CommandNinePatch : public Command {
			Rect2 rect;
			Rect2 source;
			RID texture;
			RID normal_map;
			float margin[4];
			bool draw_center;
			Color color;
			VS::NinePatchAxisMode axis_x;
			VS::NinePatchAxisMode axis_y;
			CommandNinePatch() {
				draw_center = true;
				type = TYPE_NINEPATCH;
			}
		};

		struct CommandPrimitive : public Command {
			Vector<Point2> points;
			Vector<Point2> uvs;
			Vector<Color> colors;
			RID texture;
			RID normal_map;
			float width;

			CommandPrimitive() {
				type = TYPE_PRIMITIVE;
				width = 1;
			}
		};

		struct CommandPolygon : public Command {
			Vector<int> indices;
			Vector<Point2> points;
			Vector<Point2> uvs;
			Vector<Color> colors;
			Vector<int> bones;
			Vector<float> weights;
			RID texture;
			RID normal_map;
			int count;
			bool antialiased;
			bool antialiasing_use_indices;

			struct SkinningData {
				bool dirty = true;
				LocalVector<Rect2> active_bounds;
				LocalVector<uint16_t> active_bone_ids;
				Rect2 untransformed_bound;
			};
			mutable SkinningData *skinning_data = nullptr;

			CommandPolygon() {
				type = TYPE_POLYGON;
				count = 0;
			}
			virtual ~CommandPolygon() {
				if (skinning_data) {
					memdelete(skinning_data);
					skinning_data = nullptr;
				}
			}
		};

		struct CommandMesh : public Command {
			RID mesh;
			RID texture;
			RID normal_map;
			Transform2D transform;
			Color modulate;
			CommandMesh() { type = TYPE_MESH; }
		};

		struct CommandMultiMesh : public Command {
			RID multimesh;
			RID texture;
			RID normal_map;
			RID canvas_item;
			virtual bool contains_reference(const RID &p_rid) const { return multimesh == p_rid; }
			CommandMultiMesh() { type = TYPE_MULTIMESH; }
			virtual ~CommandMultiMesh() {
				// Remove any backlinks from multimesh to canvas item.
				if (multimesh.is_valid()) {
					RasterizerStorage::base_singleton->multimesh_attach_canvas_item(multimesh, canvas_item, false);
				}
			}
		};

		struct CommandParticles : public Command {
			RID particles;
			RID texture;
			RID normal_map;
			CommandParticles() { type = TYPE_PARTICLES; }
		};

		struct CommandCircle : public Command {
			Point2 pos;
			float radius;
			Color color;
			CommandCircle() { type = TYPE_CIRCLE; }
		};

		struct CommandTransform : public Command {
			Transform2D xform;
			CommandTransform() { type = TYPE_TRANSFORM; }
		};

		struct CommandClipIgnore : public Command {
			bool ignore;
			CommandClipIgnore() {
				type = TYPE_CLIP_IGNORE;
				ignore = false;
			}
		};

		struct ViewportRender {
			VisualServer *owner;
			void *udata;
			Rect2 rect;
		};

		// For interpolation we store the current local xform,
		// and the previous xform from the previous tick.
		Transform2D xform_curr;
		Transform2D xform_prev;

		bool clip : 1;
		bool visible : 1;
		bool behind : 1;
		bool update_when_visible : 1;
		bool distance_field : 1;
		bool light_masked : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		bool use_identity_xform : 1;
		mutable bool custom_rect : 1;
		mutable bool rect_dirty : 1;
		mutable bool bound_dirty : 1;

		Vector<Command *> commands;
		mutable Rect2 rect;
		RID material;
		RID skeleton;

		//VS::MaterialBlendMode blend_mode;
		int32_t light_mask;
		mutable uint32_t skeleton_revision;

		Item *next;

		struct SkinningData {
			Transform2D skeleton_relative_xform;
			Transform2D skeleton_relative_xform_inv;
		};
		SkinningData *skinning_data = nullptr;

		struct CopyBackBuffer {
			Rect2 rect;
			Rect2 screen_rect;
			bool full;
		};
		CopyBackBuffer *copy_back_buffer;

		Color final_modulate;
		Transform2D final_transform;
		Rect2 final_clip_rect;
		Item *final_clip_owner;
		Item *material_owner;
		ViewportRender *vp_render;

		Rect2 global_rect_cache;

	private:
		Rect2 calculate_polygon_bounds(const Item::CommandPolygon &p_polygon) const;
		void precalculate_polygon_bone_bounds(const Item::CommandPolygon &p_polygon) const;

	public:
		// the rect containing this item and all children,
		// in local space.
		Rect2 local_bound;

		// When using interpolation, the local bound for culling
		// should be a combined bound of the previous and current.
		// To keep this up to date, we need to keep track of the previous
		// bound separately rather than just the combined bound.
		Rect2 local_bound_prev;
		uint32_t local_bound_last_update_tick;

		const Rect2 &get_rect() const {
			if (custom_rect) {
				return rect;
			}
			if (!rect_dirty && !update_when_visible) {
				if (skeleton == RID()) {
					return rect;
				} else {
					// special case for skeletons
					uint32_t rev = RasterizerStorage::base_singleton->skeleton_get_revision(skeleton);
					if (rev == skeleton_revision) {
						// no change to the skeleton since we last calculated the bounding rect
						return rect;
					} else {
						// We need to recalculate.
						// Mark as done for next time.
						skeleton_revision = rev;
					}
				}
			}

			//must update rect
			int s = commands.size();
			if (s == 0) {
				rect = Rect2();
				rect_dirty = false;
				return rect;
			}

			Transform2D xf;
			bool found_xform = false;
			bool first = true;

			const Item::Command *const *cmd = &commands[0];

			for (int i = 0; i < s; i++) {
				const Item::Command *c = cmd[i];
				Rect2 r;

				switch (c->type) {
					case Item::Command::TYPE_LINE: {
						const Item::CommandLine *line = static_cast<const Item::CommandLine *>(c);
						r.position = line->from;
						r.expand_to(line->to);
					} break;
					case Item::Command::TYPE_POLYLINE: {
						const Item::CommandPolyLine *pline = static_cast<const Item::CommandPolyLine *>(c);
						if (pline->triangles.size()) {
							for (int j = 0; j < pline->triangles.size(); j++) {
								if (j == 0) {
									r.position = pline->triangles[j];
								} else {
									r.expand_to(pline->triangles[j]);
								}
							}
						} else {
							for (int j = 0; j < pline->lines.size(); j++) {
								if (j == 0) {
									r.position = pline->lines[j];
								} else {
									r.expand_to(pline->lines[j]);
								}
							}
						}

					} break;
					case Item::Command::TYPE_RECT: {
						const Item::CommandRect *crect = static_cast<const Item::CommandRect *>(c);
						r = crect->rect;

					} break;
					case Item::Command::TYPE_MULTIRECT: {
						const Item::CommandMultiRect *mrect = static_cast<const Item::CommandMultiRect *>(c);
						int num_rects = mrect->rects.size();
						if (num_rects) {
							r = mrect->rects[0];
							for (int n = 1; n < num_rects; n++) {
								r = mrect->rects[n].merge(r);
							}
						}
					} break;
					case Item::Command::TYPE_NINEPATCH: {
						const Item::CommandNinePatch *style = static_cast<const Item::CommandNinePatch *>(c);
						r = style->rect;
					} break;
					case Item::Command::TYPE_PRIMITIVE: {
						const Item::CommandPrimitive *primitive = static_cast<const Item::CommandPrimitive *>(c);
						r.position = primitive->points[0];
						for (int j = 1; j < primitive->points.size(); j++) {
							r.expand_to(primitive->points[j]);
						}
					} break;
					case Item::Command::TYPE_POLYGON: {
						const Item::CommandPolygon *polygon = static_cast<const Item::CommandPolygon *>(c);
						DEV_ASSERT(polygon);
						r = calculate_polygon_bounds(*polygon);
					} break;
					case Item::Command::TYPE_MESH: {
						const Item::CommandMesh *mesh = static_cast<const Item::CommandMesh *>(c);
						AABB aabb = RasterizerStorage::base_singleton->mesh_get_aabb(mesh->mesh, RID());

						r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);

					} break;
					case Item::Command::TYPE_MULTIMESH: {
						const Item::CommandMultiMesh *multimesh = static_cast<const Item::CommandMultiMesh *>(c);
						AABB aabb = RasterizerStorage::base_singleton->multimesh_get_aabb(multimesh->multimesh);

						r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);

					} break;
					case Item::Command::TYPE_PARTICLES: {
						const Item::CommandParticles *particles_cmd = static_cast<const Item::CommandParticles *>(c);
						if (particles_cmd->particles.is_valid()) {
							AABB aabb = RasterizerStorage::base_singleton->particles_get_aabb(particles_cmd->particles);
							r = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y);
						}

					} break;
					case Item::Command::TYPE_CIRCLE: {
						const Item::CommandCircle *circle = static_cast<const Item::CommandCircle *>(c);
						r.position = Point2(-circle->radius, -circle->radius) + circle->pos;
						r.size = Point2(circle->radius * 2.0, circle->radius * 2.0);
					} break;
					case Item::Command::TYPE_TRANSFORM: {
						const Item::CommandTransform *transform = static_cast<const Item::CommandTransform *>(c);
						xf = transform->xform;
						found_xform = true;
						continue;
					} break;

					case Item::Command::TYPE_CLIP_IGNORE: {
					} break;
				}

				if (found_xform) {
					r = xf.xform(r);
				}

				if (first) {
					rect = r;
					first = false;
				} else {
					rect = rect.merge(r);
				}
			}

			rect_dirty = false;
			return rect;
		}

		void remove_references(const RID &p_rid) {
			for (int i = commands.size() - 1; i >= 0; i--) {
				if (commands[i]->contains_reference(p_rid)) {
					memdelete(commands[i]);

					// This could possibly be unordered if occurring close
					// to canvas_item deletion, but is
					// unlikely to make much performance difference,
					// and is safer.
					commands.remove(i);
				}
			}
		}

		void clear() {
			for (int i = 0; i < commands.size(); i++) {
				memdelete(commands[i]);
			}
			commands.clear();
			clip = false;
			rect_dirty = true;
			final_clip_owner = nullptr;
			material_owner = nullptr;
			light_masked = false;

			if (skinning_data) {
				memdelete(skinning_data);
				skinning_data = nullptr;
			}
			on_interpolate_transform_list = false;
		}
		Item() {
			light_mask = 1;
			skeleton_revision = 0;
			vp_render = nullptr;
			next = nullptr;
			final_clip_owner = nullptr;
			clip = false;
			final_modulate = Color(1, 1, 1, 1);
			visible = true;
			rect_dirty = true;
			bound_dirty = true;
			custom_rect = false;
			behind = false;
			material_owner = nullptr;
			copy_back_buffer = nullptr;
			distance_field = false;
			light_masked = false;
			update_when_visible = false;
			on_interpolate_transform_list = false;
			interpolated = true;
			use_identity_xform = false;
			local_bound_last_update_tick = 0;
		}
		virtual ~Item() {
			clear();
			if (copy_back_buffer) {
				memdelete(copy_back_buffer);
			}
		}
	};

	virtual void canvas_begin() = 0;
	virtual void canvas_end() = 0;

	virtual void canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {}
	virtual void canvas_render_items_end() {}
	virtual void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) = 0;
	virtual void canvas_debug_viewport_shadows(Light *p_lights_with_shadow) = 0;

	struct LightOccluderInstance : public RID_Data {
		bool enabled : 1;
		bool on_interpolate_transform_list : 1;
		bool interpolated : 1;
		RID canvas;
		RID polygon;
		RID polygon_buffer;
		Rect2 aabb_cache;
		Transform2D xform_curr;
		Transform2D xform_prev;
		Transform2D xform_cache;
		int light_mask;
		VS::CanvasOccluderPolygonCullMode cull_cache;

		LightOccluderInstance *next;

		LightOccluderInstance() {
			enabled = true;
			next = nullptr;
			light_mask = 1;
			cull_cache = VS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
			on_interpolate_transform_list = false;
			interpolated = true;
		}
	};

	virtual void canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) = 0;

	virtual void reset_canvas() = 0;

	virtual void draw_window_margins(int *p_margins, RID *p_margin_textures) = 0;

	virtual ~RasterizerCanvas() {}
};

class Rasterizer {
protected:
	static Rasterizer *(*_create_func)();

public:
	static Rasterizer *create();

	virtual RasterizerStorage *get_storage() = 0;
	virtual RasterizerCanvas *get_canvas() = 0;
	virtual RasterizerScene *get_scene() = 0;

	virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) = 0;
	virtual void set_shader_time_scale(float p_scale) = 0;

	virtual void initialize() = 0;
	virtual void begin_frame(double frame_step) = 0;
	virtual void set_current_render_target(RID p_render_target) = 0;
	virtual void restore_render_target(bool p_3d) = 0;
	virtual void clear_render_target(const Color &p_color) = 0;
	virtual void blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen = 0) = 0;
	virtual void output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) = 0;
	virtual void end_frame(bool p_swap_buffers) = 0;
	virtual void finalize() = 0;

	virtual bool is_low_end() const = 0;

	virtual ~Rasterizer() {}
};

// Use float rather than real_t as cheaper and no need for 64 bit.
_FORCE_INLINE_ void RasterizerStorage::_interpolate_RGBA8(const uint8_t *p_a, const uint8_t *p_b, uint8_t *r_dest, float p_f) const {
	// Todo, jiggle these values and test for correctness.
	// Integer interpolation is finicky.. :)
	p_f *= 256.0f;
	int32_t mult = CLAMP(int32_t(p_f), 0, 255);

	for (int n = 0; n < 4; n++) {
		int32_t a = p_a[n];
		int32_t b = p_b[n];

		int32_t diff = b - a;

		diff *= mult;
		diff /= 255;

		int32_t res = a + diff;

		// may not be needed
		res = CLAMP(res, 0, 255);
		r_dest[n] = res;
	}
}

#endif // RASTERIZER_H
