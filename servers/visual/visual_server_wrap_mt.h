/**************************************************************************/
/*  visual_server_wrap_mt.h                                               */
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

#ifndef VISUAL_SERVER_WRAP_MT_H
#define VISUAL_SERVER_WRAP_MT_H

#include "core/command_queue_mt.h"
#include "core/os/thread.h"
#include "core/safe_refcount.h"
#include "servers/visual_server.h"

class VisualServerWrapMT : public VisualServer {
	// the real visual server
	mutable VisualServer *visual_server;

	mutable CommandQueueMT command_queue;

	static void _thread_callback(void *_instance);
	void thread_loop();

	Thread::ID server_thread;
	SafeFlag exit;
	Thread thread;
	SafeFlag draw_thread_up;
	bool create_thread;

	void thread_draw(bool p_swap_buffers, double frame_step);
	void thread_flush();

	void thread_exit();

	Mutex alloc_mutex;

	int pool_max_size;

	//#define DEBUG_SYNC

	static VisualServerWrapMT *singleton_mt;

#ifdef DEBUG_SYNC
#define SYNC_DEBUG print_line("sync on: " + String(__FUNCTION__));
#else
#define SYNC_DEBUG
#endif

public:
#define ServerName VisualServer
#define ServerNameWrapMT VisualServerWrapMT
#define server_name visual_server
#include "servers/server_wrap_mt_common.h"

	/* EVENT QUEUING */
	FUNCRID(texture)
	FUNC7(texture_allocate, RID, int, int, int, Image::Format, TextureType, uint32_t)
	FUNC3(texture_set_data, RID, const Ref<Image> &, int)
	FUNC10(texture_set_data_partial, RID, const Ref<Image> &, int, int, int, int, int, int, int, int)
	FUNC2RC(Ref<Image>, texture_get_data, RID, int)
	FUNC2(texture_set_flags, RID, uint32_t)
	FUNC1RC(uint32_t, texture_get_flags, RID)
	FUNC1RC(Image::Format, texture_get_format, RID)
	FUNC1RC(TextureType, texture_get_type, RID)
	FUNC1RC(uint32_t, texture_get_texid, RID)
	FUNC1RC(uint32_t, texture_get_width, RID)
	FUNC1RC(uint32_t, texture_get_height, RID)
	FUNC1RC(uint32_t, texture_get_depth, RID)
	FUNC4(texture_set_size_override, RID, int, int, int)
	FUNC2(texture_bind, RID, uint32_t)

	FUNC3(texture_set_detect_3d_callback, RID, TextureDetectCallback, void *)
	FUNC3(texture_set_detect_srgb_callback, RID, TextureDetectCallback, void *)
	FUNC3(texture_set_detect_normal_callback, RID, TextureDetectCallback, void *)

	FUNC2(texture_set_path, RID, const String &)
	FUNC1RC(String, texture_get_path, RID)
	FUNC1(texture_set_shrink_all_x2_on_set_data, bool)
	FUNC1S(texture_debug_usage, List<TextureInfo> *)

	FUNC1(textures_keep_original, bool)

	FUNC2(texture_set_proxy, RID, RID)

	FUNC2(texture_set_force_redraw_if_visible, RID, bool)

	/* SKY API */

	FUNCRID(sky)
	FUNC3(sky_set_texture, RID, RID, int)

	/* SHADER API */

	FUNCRID(shader)

	FUNC2(shader_set_code, RID, const String &)
	FUNC1RC(String, shader_get_code, RID)

	FUNC2SC(shader_get_param_list, RID, List<PropertyInfo> *)

	FUNC3(shader_set_default_texture_param, RID, const StringName &, RID)
	FUNC2RC(RID, shader_get_default_texture_param, RID, const StringName &)

	FUNC2(shader_add_custom_define, RID, const String &)
	FUNC2SC(shader_get_custom_defines, RID, Vector<String> *)
	FUNC2(shader_remove_custom_define, RID, const String &)

	FUNC1(set_shader_async_hidden_forbidden, bool)

	/* COMMON MATERIAL API */

	FUNCRID(material)

	FUNC2(material_set_shader, RID, RID)
	FUNC1RC(RID, material_get_shader, RID)

	FUNC3(material_set_param, RID, const StringName &, const Variant &)
	FUNC2RC(Variant, material_get_param, RID, const StringName &)
	FUNC2RC(Variant, material_get_param_default, RID, const StringName &)

	FUNC2(material_set_render_priority, RID, int)
	FUNC2(material_set_line_width, RID, float)
	FUNC2(material_set_next_pass, RID, RID)

	/* MESH API */

	FUNCRID(mesh)

	FUNC10(mesh_add_surface, RID, uint32_t, PrimitiveType, const PoolVector<uint8_t> &, int, const PoolVector<uint8_t> &, int, const AABB &, const Vector<PoolVector<uint8_t>> &, const Vector<AABB> &)

	FUNC2(mesh_set_blend_shape_count, RID, int)
	FUNC1RC(int, mesh_get_blend_shape_count, RID)

	FUNC2(mesh_set_blend_shape_mode, RID, BlendShapeMode)
	FUNC1RC(BlendShapeMode, mesh_get_blend_shape_mode, RID)

	FUNC4(mesh_surface_update_region, RID, int, int, const PoolVector<uint8_t> &)

	FUNC3(mesh_surface_set_material, RID, int, RID)
	FUNC2RC(RID, mesh_surface_get_material, RID, int)

	FUNC2RC(int, mesh_surface_get_array_len, RID, int)
	FUNC2RC(int, mesh_surface_get_array_index_len, RID, int)

	FUNC2RC(PoolVector<uint8_t>, mesh_surface_get_array, RID, int)
	FUNC2RC(PoolVector<uint8_t>, mesh_surface_get_index_array, RID, int)

	FUNC2RC(uint32_t, mesh_surface_get_format, RID, int)
	FUNC2RC(PrimitiveType, mesh_surface_get_primitive_type, RID, int)

	FUNC2RC(AABB, mesh_surface_get_aabb, RID, int)
	FUNC2RC(Vector<PoolVector<uint8_t>>, mesh_surface_get_blend_shapes, RID, int)
	FUNC2RC(Vector<AABB>, mesh_surface_get_skeleton_aabb, RID, int)

	FUNC2(mesh_remove_surface, RID, int)
	FUNC1RC(int, mesh_get_surface_count, RID)

	FUNC2(mesh_set_custom_aabb, RID, const AABB &)
	FUNC1RC(AABB, mesh_get_custom_aabb, RID)

	FUNC1(mesh_clear, RID)

	/* MULTIMESH API */

	FUNCRID(multimesh)

	FUNC5(multimesh_allocate, RID, int, MultimeshTransformFormat, MultimeshColorFormat, MultimeshCustomDataFormat)
	FUNC1RC(int, multimesh_get_instance_count, RID)

	FUNC2(multimesh_set_mesh, RID, RID)
	FUNC3(multimesh_instance_set_transform, RID, int, const Transform &)
	FUNC3(multimesh_instance_set_transform_2d, RID, int, const Transform2D &)
	FUNC3(multimesh_instance_set_color, RID, int, const Color &)
	FUNC3(multimesh_instance_set_custom_data, RID, int, const Color &)

	FUNC1RC(RID, multimesh_get_mesh, RID)
	FUNC1RC(AABB, multimesh_get_aabb, RID)

	FUNC2RC(Transform, multimesh_instance_get_transform, RID, int)
	FUNC2RC(Transform2D, multimesh_instance_get_transform_2d, RID, int)
	FUNC2RC(Color, multimesh_instance_get_color, RID, int)
	FUNC2RC(Color, multimesh_instance_get_custom_data, RID, int)

	FUNC2(multimesh_set_as_bulk_array, RID, const PoolVector<float> &)

	FUNC3(multimesh_set_as_bulk_array_interpolated, RID, const PoolVector<float> &, const PoolVector<float> &)
	FUNC2(multimesh_set_physics_interpolated, RID, bool)
	FUNC2(multimesh_set_physics_interpolation_quality, RID, MultimeshPhysicsInterpolationQuality)
	FUNC2(multimesh_instance_reset_physics_interpolation, RID, int)

	FUNC2(multimesh_set_visible_instances, RID, int)
	FUNC1RC(int, multimesh_get_visible_instances, RID)

	/* IMMEDIATE API */

	FUNCRID(immediate)
	FUNC3(immediate_begin, RID, PrimitiveType, RID)
	FUNC2(immediate_vertex, RID, const Vector3 &)
	FUNC2(immediate_normal, RID, const Vector3 &)
	FUNC2(immediate_tangent, RID, const Plane &)
	FUNC2(immediate_color, RID, const Color &)
	FUNC2(immediate_uv, RID, const Vector2 &)
	FUNC2(immediate_uv2, RID, const Vector2 &)
	FUNC1(immediate_end, RID)
	FUNC1(immediate_clear, RID)
	FUNC2(immediate_set_material, RID, RID)
	FUNC1RC(RID, immediate_get_material, RID)

	/* SKELETON API */

	FUNCRID(skeleton)
	FUNC3(skeleton_allocate, RID, int, bool)
	FUNC1RC(int, skeleton_get_bone_count, RID)
	FUNC3(skeleton_bone_set_transform, RID, int, const Transform &)
	FUNC2RC(Transform, skeleton_bone_get_transform, RID, int)
	FUNC3(skeleton_bone_set_transform_2d, RID, int, const Transform2D &)
	FUNC2RC(Transform2D, skeleton_bone_get_transform_2d, RID, int)
	FUNC2(skeleton_set_base_transform_2d, RID, const Transform2D &)

	/* Light API */

	FUNCRID(directional_light)
	FUNCRID(omni_light)
	FUNCRID(spot_light)

	FUNC2(light_set_color, RID, const Color &)
	FUNC3(light_set_param, RID, LightParam, float)
	FUNC2(light_set_shadow, RID, bool)
	FUNC2(light_set_shadow_color, RID, const Color &)
	FUNC2(light_set_projector, RID, RID)
	FUNC2(light_set_negative, RID, bool)
	FUNC2(light_set_cull_mask, RID, uint32_t)
	FUNC2(light_set_reverse_cull_face_mode, RID, bool)
	FUNC2(light_set_use_gi, RID, bool)
	FUNC2(light_set_bake_mode, RID, LightBakeMode)

	FUNC2(light_omni_set_shadow_mode, RID, LightOmniShadowMode)
	FUNC2(light_omni_set_shadow_detail, RID, LightOmniShadowDetail)

	FUNC2(light_directional_set_shadow_mode, RID, LightDirectionalShadowMode)
	FUNC2(light_directional_set_blend_splits, RID, bool)
	FUNC2(light_directional_set_shadow_depth_range_mode, RID, LightDirectionalShadowDepthRangeMode)

	/* PROBE API */

	FUNCRID(reflection_probe)

	FUNC2(reflection_probe_set_update_mode, RID, ReflectionProbeUpdateMode)
	FUNC2(reflection_probe_set_intensity, RID, float)
	FUNC2(reflection_probe_set_interior_ambient, RID, const Color &)
	FUNC2(reflection_probe_set_interior_ambient_energy, RID, float)
	FUNC2(reflection_probe_set_interior_ambient_probe_contribution, RID, float)
	FUNC2(reflection_probe_set_max_distance, RID, float)
	FUNC2(reflection_probe_set_extents, RID, const Vector3 &)
	FUNC2(reflection_probe_set_origin_offset, RID, const Vector3 &)
	FUNC2(reflection_probe_set_as_interior, RID, bool)
	FUNC2(reflection_probe_set_enable_box_projection, RID, bool)
	FUNC2(reflection_probe_set_enable_shadows, RID, bool)
	FUNC2(reflection_probe_set_cull_mask, RID, uint32_t)
	FUNC2(reflection_probe_set_resolution, RID, int)

	/* BAKED LIGHT API */

	FUNCRID(gi_probe)

	FUNC2(gi_probe_set_bounds, RID, const AABB &)
	FUNC1RC(AABB, gi_probe_get_bounds, RID)

	FUNC2(gi_probe_set_cell_size, RID, float)
	FUNC1RC(float, gi_probe_get_cell_size, RID)

	FUNC2(gi_probe_set_to_cell_xform, RID, const Transform &)
	FUNC1RC(Transform, gi_probe_get_to_cell_xform, RID)

	FUNC2(gi_probe_set_dynamic_range, RID, int)
	FUNC1RC(int, gi_probe_get_dynamic_range, RID)

	FUNC2(gi_probe_set_energy, RID, float)
	FUNC1RC(float, gi_probe_get_energy, RID)

	FUNC2(gi_probe_set_bias, RID, float)
	FUNC1RC(float, gi_probe_get_bias, RID)

	FUNC2(gi_probe_set_normal_bias, RID, float)
	FUNC1RC(float, gi_probe_get_normal_bias, RID)

	FUNC2(gi_probe_set_propagation, RID, float)
	FUNC1RC(float, gi_probe_get_propagation, RID)

	FUNC2(gi_probe_set_interior, RID, bool)
	FUNC1RC(bool, gi_probe_is_interior, RID)

	FUNC2(gi_probe_set_compress, RID, bool)
	FUNC1RC(bool, gi_probe_is_compressed, RID)

	FUNC2(gi_probe_set_dynamic_data, RID, const PoolVector<int> &)
	FUNC1RC(PoolVector<int>, gi_probe_get_dynamic_data, RID)

	/* LIGHTMAP CAPTURE */

	FUNCRID(lightmap_capture)

	FUNC2(lightmap_capture_set_bounds, RID, const AABB &)
	FUNC1RC(AABB, lightmap_capture_get_bounds, RID)

	FUNC2(lightmap_capture_set_octree, RID, const PoolVector<uint8_t> &)
	FUNC1RC(PoolVector<uint8_t>, lightmap_capture_get_octree, RID)
	FUNC2(lightmap_capture_set_octree_cell_transform, RID, const Transform &)
	FUNC1RC(Transform, lightmap_capture_get_octree_cell_transform, RID)
	FUNC2(lightmap_capture_set_octree_cell_subdiv, RID, int)
	FUNC1RC(int, lightmap_capture_get_octree_cell_subdiv, RID)
	FUNC2(lightmap_capture_set_energy, RID, float)
	FUNC1RC(float, lightmap_capture_get_energy, RID)
	FUNC2(lightmap_capture_set_interior, RID, bool)
	FUNC1RC(bool, lightmap_capture_is_interior, RID)

	/* PARTICLES */

	FUNCRID(particles)

	FUNC2(particles_set_emitting, RID, bool)
	FUNC1R(bool, particles_get_emitting, RID)
	FUNC2(particles_set_amount, RID, int)
	FUNC2(particles_set_lifetime, RID, float)
	FUNC2(particles_set_one_shot, RID, bool)
	FUNC2(particles_set_pre_process_time, RID, float)
	FUNC2(particles_set_explosiveness_ratio, RID, float)
	FUNC2(particles_set_randomness_ratio, RID, float)
	FUNC2(particles_set_custom_aabb, RID, const AABB &)
	FUNC2(particles_set_speed_scale, RID, float)
	FUNC2(particles_set_use_local_coordinates, RID, bool)
	FUNC2(particles_set_process_material, RID, RID)
	FUNC2(particles_set_fixed_fps, RID, int)
	FUNC2(particles_set_fractional_delta, RID, bool)
	FUNC1R(bool, particles_is_inactive, RID)
	FUNC1(particles_request_process, RID)
	FUNC1(particles_restart, RID)

	FUNC2(particles_set_draw_order, RID, VS::ParticlesDrawOrder)

	FUNC2(particles_set_draw_passes, RID, int)
	FUNC3(particles_set_draw_pass_mesh, RID, int, RID)
	FUNC2(particles_set_emission_transform, RID, const Transform &)

	FUNC1R(AABB, particles_get_current_aabb, RID)

	/* CAMERA API */

	FUNCRID(camera)
	FUNC4(camera_set_perspective, RID, float, float, float)
	FUNC4(camera_set_orthogonal, RID, float, float, float)
	FUNC5(camera_set_frustum, RID, float, Vector2, float, float)
	FUNC2(camera_set_transform, RID, const Transform &)
	FUNC2(camera_set_cull_mask, RID, uint32_t)
	FUNC2(camera_set_environment, RID, RID)
	FUNC2(camera_set_use_vertical_aspect, RID, bool)

	/* VIEWPORT TARGET API */

	FUNCRID(viewport)

	FUNC2(viewport_set_use_arvr, RID, bool)

	FUNC3(viewport_set_size, RID, int, int)

	FUNC2(viewport_set_active, RID, bool)
	FUNC2(viewport_set_parent_viewport, RID, RID)

	FUNC2(viewport_set_clear_mode, RID, ViewportClearMode)

	FUNC3(viewport_attach_to_screen, RID, const Rect2 &, int)
	FUNC2(viewport_set_render_direct_to_screen, RID, bool)
	FUNC1(viewport_detach, RID)

	FUNC2(viewport_set_update_mode, RID, ViewportUpdateMode)
	FUNC2(viewport_set_vflip, RID, bool)

	FUNC1RC(RID, viewport_get_texture, RID)

	FUNC2(viewport_set_hide_scenario, RID, bool)
	FUNC2(viewport_set_hide_canvas, RID, bool)
	FUNC2(viewport_set_disable_environment, RID, bool)
	FUNC2(viewport_set_disable_3d, RID, bool)
	FUNC2(viewport_set_keep_3d_linear, RID, bool)

	FUNC2(viewport_attach_camera, RID, RID)
	FUNC2(viewport_set_scenario, RID, RID)
	FUNC2(viewport_attach_canvas, RID, RID)

	FUNC2(viewport_remove_canvas, RID, RID)
	FUNC3(viewport_set_canvas_transform, RID, RID, const Transform2D &)
	FUNC2(viewport_set_transparent_background, RID, bool)

	FUNC2(viewport_set_global_canvas_transform, RID, const Transform2D &)
	FUNC4(viewport_set_canvas_stacking, RID, RID, int, int)
	FUNC3(viewport_set_shadow_atlas_size, RID, int, bool)
	FUNC3(viewport_set_shadow_atlas_quadrant_subdivision, RID, int, int)
	FUNC2(viewport_set_msaa, RID, ViewportMSAA)
	FUNC2(viewport_set_use_fxaa, RID, bool)
	FUNC2(viewport_set_use_debanding, RID, bool)
	FUNC2(viewport_set_sharpen_intensity, RID, float)
	FUNC2(viewport_set_hdr, RID, bool)
	FUNC2(viewport_set_use_32_bpc_depth, RID, bool)
	FUNC2(viewport_set_usage, RID, ViewportUsage)

	//this passes directly to avoid stalling, but it's pretty dangerous, so don't call after freeing a viewport
	virtual int viewport_get_render_info(RID p_viewport, ViewportRenderInfo p_info) {
		return visual_server->viewport_get_render_info(p_viewport, p_info);
	}

	FUNC2(viewport_set_debug_draw, RID, ViewportDebugDraw)

	/* ENVIRONMENT API */

	FUNCRID(environment)

	FUNC2(environment_set_background, RID, EnvironmentBG)
	FUNC2(environment_set_sky, RID, RID)
	FUNC2(environment_set_sky_custom_fov, RID, float)
	FUNC2(environment_set_sky_orientation, RID, const Basis &)
	FUNC2(environment_set_bg_color, RID, const Color &)
	FUNC2(environment_set_bg_energy, RID, float)
	FUNC2(environment_set_canvas_max_layer, RID, int)
	FUNC4(environment_set_ambient_light, RID, const Color &, float, float)
	FUNC2(environment_set_camera_feed_id, RID, int)
	FUNC7(environment_set_ssr, RID, bool, int, float, float, float, bool)
	FUNC13(environment_set_ssao, RID, bool, float, float, float, float, float, float, float, const Color &, EnvironmentSSAOQuality, EnvironmentSSAOBlur, float)

	FUNC6(environment_set_dof_blur_near, RID, bool, float, float, float, EnvironmentDOFBlurQuality)
	FUNC6(environment_set_dof_blur_far, RID, bool, float, float, float, EnvironmentDOFBlurQuality)
	FUNC12(environment_set_glow, RID, bool, int, float, float, float, EnvironmentGlowBlendMode, float, float, float, bool, bool)
	FUNC3(environment_set_glow_map, RID, float, RID)

	FUNC9(environment_set_tonemap, RID, EnvironmentToneMapper, float, float, bool, float, float, float, float)

	FUNC6(environment_set_adjustment, RID, bool, float, float, float, RID)

	FUNC5(environment_set_fog, RID, bool, const Color &, const Color &, float)
	FUNC7(environment_set_fog_depth, RID, bool, float, float, float, bool, float)
	FUNC5(environment_set_fog_height, RID, bool, float, float, float)

	/* SCENARIO API */

	FUNCRID(scenario)

	FUNC2(scenario_set_debug, RID, ScenarioDebugMode)
	FUNC2(scenario_set_environment, RID, RID)
	FUNC3(scenario_set_reflection_atlas_size, RID, int, int)
	FUNC2(scenario_set_fallback_environment, RID, RID)

	/* INSTANCING API */

	FUNCRID(instance)

	FUNC2(instance_set_base, RID, RID)
	FUNC2(instance_set_scenario, RID, RID)
	FUNC2(instance_set_layer_mask, RID, uint32_t)
	FUNC3(instance_set_pivot_data, RID, float, bool)
	FUNC2(instance_set_transform, RID, const Transform &)
	FUNC2(instance_set_interpolated, RID, bool)
	FUNC1(instance_reset_physics_interpolation, RID)
	FUNC2(instance_attach_object_instance_id, RID, ObjectID)
	FUNC3(instance_set_blend_shape_weight, RID, int, float)
	FUNC3(instance_set_surface_material, RID, int, RID)
	FUNC2(instance_set_visible, RID, bool)
	FUNC5(instance_set_use_lightmap, RID, RID, RID, int, const Rect2 &)

	FUNC2(instance_set_custom_aabb, RID, AABB)

	FUNC2(instance_attach_skeleton, RID, RID)
	FUNC2(instance_set_exterior, RID, bool)

	FUNC2(instance_set_extra_visibility_margin, RID, real_t)

	/* PORTALS API */

	FUNC2(instance_set_portal_mode, RID, InstancePortalMode)

	FUNCRID(ghost)
	FUNC4(ghost_set_scenario, RID, RID, ObjectID, const AABB &)
	FUNC2(ghost_update, RID, const AABB &)

	FUNCRID(portal)
	FUNC2(portal_set_scenario, RID, RID)
	FUNC3(portal_set_geometry, RID, const Vector<Vector3> &, real_t)
	FUNC4(portal_link, RID, RID, RID, bool)
	FUNC2(portal_set_active, RID, bool)

	/* ROOMGROUPS API */

	FUNCRID(roomgroup)
	FUNC2(roomgroup_prepare, RID, ObjectID)
	FUNC2(roomgroup_set_scenario, RID, RID)
	FUNC2(roomgroup_add_room, RID, RID)

	/* OCCLUDERS API */

	FUNCRID(occluder_instance)
	FUNC2(occluder_instance_set_scenario, RID, RID)
	FUNC2(occluder_instance_link_resource, RID, RID)
	FUNC2(occluder_instance_set_transform, RID, const Transform &)
	FUNC2(occluder_instance_set_active, RID, bool)

	FUNCRID(occluder_resource)
	FUNC2(occluder_resource_prepare, RID, OccluderType)
	FUNC2(occluder_resource_spheres_update, RID, const Vector<Plane> &)
	FUNC2(occluder_resource_mesh_update, RID, const Geometry::OccluderMeshData &)
	FUNC1(set_use_occlusion_culling, bool)
	FUNC1RC(Geometry::MeshData, occlusion_debug_get_current_polys, RID)

	/* ROOMS API */

	FUNCRID(room)
	FUNC2(room_set_scenario, RID, RID)
	FUNC4(room_add_instance, RID, RID, const AABB &, const Vector<Vector3> &)
	FUNC3(room_add_ghost, RID, ObjectID, const AABB &)
	FUNC5(room_set_bound, RID, ObjectID, const Vector<Plane> &, const AABB &, const Vector<Vector3> &)
	FUNC2(room_prepare, RID, int32_t)
	FUNC1(rooms_and_portals_clear, RID)
	FUNC2(rooms_unload, RID, String)
	FUNC8(rooms_finalize, RID, bool, bool, bool, bool, String, bool, bool)
	FUNC4(rooms_override_camera, RID, bool, const Vector3 &, const Vector<Plane> *)
	FUNC2(rooms_set_active, RID, bool)
	FUNC3(rooms_set_params, RID, int, real_t)
	FUNC3(rooms_set_debug_feature, RID, RoomsDebugFeature, bool)
	FUNC2(rooms_update_gameplay_monitor, RID, const Vector<Vector3> &)

	// don't use this in a game
	FUNC1RC(bool, rooms_is_loaded, RID)

	// Callbacks
	FUNC1(callbacks_register, VisualServerCallbacks *)

	// don't use these in a game!
	FUNC2RC(Vector<ObjectID>, instances_cull_aabb, const AABB &, RID)
	FUNC3RC(Vector<ObjectID>, instances_cull_ray, const Vector3 &, const Vector3 &, RID)
	FUNC2RC(Vector<ObjectID>, instances_cull_convex, const Vector<Plane> &, RID)

	FUNC3(instance_geometry_set_flag, RID, InstanceFlags, bool)
	FUNC2(instance_geometry_set_cast_shadows_setting, RID, ShadowCastingSetting)
	FUNC2(instance_geometry_set_material_override, RID, RID)
	FUNC2(instance_geometry_set_material_overlay, RID, RID)

	/* CANVAS (2D) */

	FUNCRID(canvas)
	FUNC3(canvas_set_item_mirroring, RID, RID, const Point2 &)
	FUNC2(canvas_set_modulate, RID, const Color &)
	FUNC3(canvas_set_parent, RID, RID, float)
	FUNC1(canvas_set_disable_scale, bool)

	FUNCRID(canvas_item)
	FUNC2(canvas_item_set_parent, RID, RID)
	FUNC2(canvas_item_set_name, RID, String)

	FUNC2(canvas_item_set_visible, RID, bool)
	FUNC2(canvas_item_set_light_mask, RID, int)

	FUNC2(canvas_item_set_update_when_visible, RID, bool)

	FUNC2(canvas_item_set_transform, RID, const Transform2D &)
	FUNC2(canvas_item_set_clip, RID, bool)
	FUNC2(canvas_item_set_distance_field_mode, RID, bool)
	FUNC3(canvas_item_set_custom_rect, RID, bool, const Rect2 &)
	FUNC2(canvas_item_set_modulate, RID, const Color &)
	FUNC2(canvas_item_set_self_modulate, RID, const Color &)

	FUNC2(canvas_item_set_draw_behind_parent, RID, bool)
	FUNC2(canvas_item_set_use_identity_transform, RID, bool)

	FUNC6(canvas_item_add_line, RID, const Point2 &, const Point2 &, const Color &, float, bool)
	FUNC5(canvas_item_add_polyline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	FUNC5(canvas_item_add_multiline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	FUNC3(canvas_item_add_rect, RID, const Rect2 &, const Color &)
	FUNC4(canvas_item_add_circle, RID, const Point2 &, float, const Color &)
	FUNC7(canvas_item_add_texture_rect, RID, const Rect2 &, RID, bool, const Color &, bool, RID)
	FUNC8(canvas_item_add_texture_rect_region, RID, const Rect2 &, RID, const Rect2 &, const Color &, bool, RID, bool)
	FUNC7(canvas_item_add_texture_multirect_region, RID, const Vector<Rect2> &, RID, const Vector<Rect2> &, const Color &, uint32_t, RID)
	FUNC11(canvas_item_add_nine_patch, RID, const Rect2 &, const Rect2 &, RID, const Vector2 &, const Vector2 &, NinePatchAxisMode, NinePatchAxisMode, bool, const Color &, RID)
	FUNC7(canvas_item_add_primitive, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID, float, RID)
	FUNC7(canvas_item_add_polygon, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID, RID, bool)
	FUNC12(canvas_item_add_triangle_array, RID, const Vector<int> &, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, const Vector<int> &, const Vector<float> &, RID, int, RID, bool, bool)
	FUNC6(canvas_item_add_mesh, RID, const RID &, const Transform2D &, const Color &, RID, RID)
	FUNC4(canvas_item_add_multimesh, RID, RID, RID, RID)
	FUNC4(canvas_item_add_particles, RID, RID, RID, RID)
	FUNC2(canvas_item_add_set_transform, RID, const Transform2D &)
	FUNC2(canvas_item_add_clip_ignore, RID, bool)
	FUNC2(canvas_item_set_sort_children_by_y, RID, bool)
	FUNC2(canvas_item_set_z_index, RID, int)
	FUNC2(canvas_item_set_z_as_relative_to_parent, RID, bool)
	FUNC3(canvas_item_set_copy_to_backbuffer, RID, bool, const Rect2 &)
	FUNC1(canvas_item_clear, RID)
	FUNC2(canvas_item_set_draw_index, RID, int)
	FUNC2(canvas_item_set_material, RID, RID)
	FUNC2(canvas_item_set_use_parent_material, RID, bool)

	FUNC2(canvas_item_attach_skeleton, RID, RID)
	FUNC2(canvas_item_set_skeleton_relative_xform, RID, Transform2D)
	FUNC1R(Rect2, _debug_canvas_item_get_rect, RID)
	FUNC1R(Rect2, _debug_canvas_item_get_local_bound, RID)

	FUNC2(canvas_item_set_interpolated, RID, bool)
	FUNC1(canvas_item_reset_physics_interpolation, RID)
	FUNC2(canvas_item_transform_physics_interpolation, RID, const Transform2D &)

	FUNC0R(RID, canvas_light_create)
	FUNC2(canvas_light_attach_to_canvas, RID, RID)
	FUNC2(canvas_light_set_enabled, RID, bool)
	FUNC2(canvas_light_set_scale, RID, float)
	FUNC2(canvas_light_set_transform, RID, const Transform2D &)
	FUNC2(canvas_light_set_texture, RID, RID)
	FUNC2(canvas_light_set_texture_offset, RID, const Vector2 &)
	FUNC2(canvas_light_set_color, RID, const Color &)
	FUNC2(canvas_light_set_height, RID, float)
	FUNC2(canvas_light_set_energy, RID, float)
	FUNC3(canvas_light_set_z_range, RID, int, int)
	FUNC3(canvas_light_set_layer_range, RID, int, int)
	FUNC2(canvas_light_set_item_cull_mask, RID, int)
	FUNC2(canvas_light_set_item_shadow_cull_mask, RID, int)

	FUNC2(canvas_light_set_mode, RID, CanvasLightMode)

	FUNC2(canvas_light_set_shadow_enabled, RID, bool)
	FUNC2(canvas_light_set_shadow_buffer_size, RID, int)
	FUNC2(canvas_light_set_shadow_gradient_length, RID, float)
	FUNC2(canvas_light_set_shadow_filter, RID, CanvasLightShadowFilter)
	FUNC2(canvas_light_set_shadow_color, RID, const Color &)
	FUNC2(canvas_light_set_shadow_smooth, RID, float)

	FUNC2(canvas_light_set_interpolated, RID, bool)
	FUNC1(canvas_light_reset_physics_interpolation, RID)
	FUNC2(canvas_light_transform_physics_interpolation, RID, const Transform2D &)

	FUNCRID(canvas_light_occluder)
	FUNC2(canvas_light_occluder_attach_to_canvas, RID, RID)
	FUNC2(canvas_light_occluder_set_enabled, RID, bool)
	FUNC2(canvas_light_occluder_set_polygon, RID, RID)
	FUNC2(canvas_light_occluder_set_transform, RID, const Transform2D &)
	FUNC2(canvas_light_occluder_set_light_mask, RID, int)

	FUNC2(canvas_light_occluder_set_interpolated, RID, bool)
	FUNC1(canvas_light_occluder_reset_physics_interpolation, RID)
	FUNC2(canvas_light_occluder_transform_physics_interpolation, RID, const Transform2D &)

	FUNCRID(canvas_occluder_polygon)
	FUNC3(canvas_occluder_polygon_set_shape, RID, const PoolVector<Vector2> &, bool)
	FUNC2(canvas_occluder_polygon_set_shape_as_lines, RID, const PoolVector<Vector2> &)

	FUNC2(canvas_occluder_polygon_set_cull_mode, RID, CanvasOccluderPolygonCullMode)

	/* BLACK BARS */

	FUNC4(black_bars_set_margins, int, int, int, int)
	FUNC4(black_bars_set_images, RID, RID, RID, RID)

	/* FREE */

	FUNC1(free, RID)

	/* EVENT QUEUING */

	FUNC3(request_frame_drawn_callback, Object *, const StringName &, const Variant &)

	virtual void init();
	virtual void finish();
	virtual void tick();
	virtual void pre_draw(bool p_will_draw);
	virtual void draw(bool p_swap_buffers, double frame_step);
	virtual void sync();
	FUNC1RC(bool, has_changed, ChangedPriority)
	virtual void set_physics_interpolation_enabled(bool p_enabled);

	/* RENDER INFO */

	//this passes directly to avoid stalling
	virtual uint64_t get_render_info(RenderInfo p_info) {
		return visual_server->get_render_info(p_info);
	}

	virtual String get_video_adapter_name() const {
		return visual_server->get_video_adapter_name();
	}

	virtual String get_video_adapter_vendor() const {
		return visual_server->get_video_adapter_vendor();
	}

	FUNC4(set_boot_image, const Ref<Image> &, const Color &, bool, bool)
	FUNC1(set_default_clear_color, const Color &)
	FUNC1(set_shader_time_scale, float)

	FUNC0R(RID, get_test_cube)

	FUNC1(set_debug_generate_wireframes, bool)

	virtual bool has_feature(Features p_feature) const { return visual_server->has_feature(p_feature); }
	virtual bool has_os_feature(const String &p_feature) const { return visual_server->has_os_feature(p_feature); }

	FUNC1(call_set_use_vsync, bool)

	static void set_use_vsync_callback(bool p_enable);

	virtual bool is_low_end() const {
		return visual_server->is_low_end();
	}

	VisualServerWrapMT(VisualServer *p_contained, bool p_create_thread);
	~VisualServerWrapMT();

#undef ServerName
#undef ServerNameWrapMT
#undef server_name
};

#ifdef DEBUG_SYNC
#undef DEBUG_SYNC
#endif
#undef SYNC_DEBUG

#endif // VISUAL_SERVER_WRAP_MT_H
