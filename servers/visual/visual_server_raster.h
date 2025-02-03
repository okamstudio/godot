/**************************************************************************/
/*  visual_server_raster.h                                                */
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

#ifndef VISUAL_SERVER_RASTER_H
#define VISUAL_SERVER_RASTER_H

#include "core/math/octree.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"
#include "visual_server_canvas.h"
#include "visual_server_globals.h"
#include "visual_server_scene.h"
#include "visual_server_viewport.h"

class VisualServerRaster : public VisualServer {
	enum {

		MAX_INSTANCE_CULL = 8192,
		MAX_INSTANCE_LIGHTS = 4,
		LIGHT_CACHE_DIRTY = -1,
		MAX_LIGHTS_CULLED = 256,
		MAX_ROOM_CULL = 32,
		MAX_EXTERIOR_PORTALS = 128,
		MAX_LIGHT_SAMPLERS = 256,
		INSTANCE_ROOMLESS_MASK = (1 << 20)

	};

	// low and high priority
	static int changes[2];
	RID test_cube;

	int black_margin[4];
	RID black_image[4];

	struct FrameDrawnCallbacks {
		ObjectID object;
		StringName method;
		Variant param;
	};

	List<FrameDrawnCallbacks> frame_drawn_callbacks;

	void _draw_margins();

	// This function is NOT dead code.
	// It is specifically for debugging redraws to help identify problems with
	// undesired constant editor updating.
	// The function will be called in DEV builds (and thus does not require a recompile),
	// allowing you to place a breakpoint either at the first line or the semicolon.
	// You can then look at the callstack to find the cause of the redraw.
	static void _changes_changed(int p_priority) {
		if (p_priority) {
			;
		}
	}

public:
	// if editor is redrawing when it shouldn't, use a DEV build and put a breakpoint in _changes_changed()
	_FORCE_INLINE_ static void redraw_request(bool p_high_priority = true) {
		int priority = p_high_priority ? 1 : 0;
		changes[priority] += 1;
#ifdef DEV_ENABLED
		_changes_changed(priority);
#endif
	}

#ifdef DEV_ENABLED
#define DISPLAY_CHANGED \
	changes[1] += 1;    \
	_changes_changed(1);
#else
#define DISPLAY_CHANGED \
	changes[1] += 1;
#endif

#define BIND0R(m_r, m_name) \
	m_r m_name() { return BINDBASE->m_name(); }
#define BIND1R(m_r, m_name, m_type1) \
	m_r m_name(m_type1 arg1) { return BINDBASE->m_name(arg1); }
#define BIND1RC(m_r, m_name, m_type1) \
	m_r m_name(m_type1 arg1) const { return BINDBASE->m_name(arg1); }
#define BIND2R(m_r, m_name, m_type1, m_type2) \
	m_r m_name(m_type1 arg1, m_type2 arg2) { return BINDBASE->m_name(arg1, arg2); }
#define BIND2RC(m_r, m_name, m_type1, m_type2) \
	m_r m_name(m_type1 arg1, m_type2 arg2) const { return BINDBASE->m_name(arg1, arg2); }
#define BIND3RC(m_r, m_name, m_type1, m_type2, m_type3) \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) const { return BINDBASE->m_name(arg1, arg2, arg3); }
#define BIND4RC(m_r, m_name, m_type1, m_type2, m_type3, m_type4) \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) const { return BINDBASE->m_name(arg1, arg2, arg3, arg4); }

#define BIND0N(m_name) \
	void m_name() { BINDBASE->m_name(); }
#define BIND1(m_name, m_type1) \
	void m_name(m_type1 arg1) { DISPLAY_CHANGED BINDBASE->m_name(arg1); }
#define BIND1N(m_name, m_type1) \
	void m_name(m_type1 arg1) { BINDBASE->m_name(arg1); }
#define BIND2(m_name, m_type1, m_type2) \
	void m_name(m_type1 arg1, m_type2 arg2) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2); }
#define BIND2C(m_name, m_type1, m_type2) \
	void m_name(m_type1 arg1, m_type2 arg2) const { BINDBASE->m_name(arg1, arg2); }
#define BIND3(m_name, m_type1, m_type2, m_type3) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3); }
#define BIND4(m_name, m_type1, m_type2, m_type3, m_type4) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4); }
#define BIND5(m_name, m_type1, m_type2, m_type3, m_type4, m_type5) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5); }
#define BIND6(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6); }
#define BIND7(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7); }
#define BIND8(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); }
#define BIND9(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); }
#define BIND10(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); }
#define BIND11(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); }
#define BIND12(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12); }
#define BIND13(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12, m_type13) \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12, m_type13 arg13) { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13); }

//from now on, calls forwarded to this singleton
#define BINDBASE VSG::storage

	/* TEXTURE API */

	BIND0R(RID, texture_create)
	BIND7(texture_allocate, RID, int, int, int, Image::Format, TextureType, uint32_t)
	BIND3(texture_set_data, RID, const Ref<Image> &, int)
	BIND4(texture_set_data_raw, RID, const PoolByteArray &, int, int);
	BIND10(texture_set_data_partial, RID, const Ref<Image> &, int, int, int, int, int, int, int, int)
	BIND2RC(Ref<Image>, texture_get_data, RID, int)
	BIND2(texture_set_flags, RID, uint32_t)
	BIND1RC(uint32_t, texture_get_flags, RID)
	BIND1RC(Image::Format, texture_get_format, RID)
	BIND1RC(TextureType, texture_get_type, RID)
	BIND1RC(uint32_t, texture_get_texid, RID)
	BIND1RC(uint32_t, texture_get_width, RID)
	BIND1RC(uint32_t, texture_get_height, RID)
	BIND1RC(uint32_t, texture_get_depth, RID)
	BIND4(texture_set_size_override, RID, int, int, int)
	BIND2(texture_bind, RID, uint32_t)

	BIND3(texture_set_detect_3d_callback, RID, TextureDetectCallback, void *)
	BIND3(texture_set_detect_srgb_callback, RID, TextureDetectCallback, void *)
	BIND3(texture_set_detect_normal_callback, RID, TextureDetectCallback, void *)

	BIND2(texture_set_path, RID, const String &)
	BIND1RC(String, texture_get_path, RID)
	BIND1(texture_set_shrink_all_x2_on_set_data, bool)
	BIND1(texture_debug_usage, List<TextureInfo> *)

	BIND1(textures_keep_original, bool)

	BIND2(texture_set_proxy, RID, RID)

	BIND2(texture_set_force_redraw_if_visible, RID, bool)

	/* SKY API */

	BIND0R(RID, sky_create)
	BIND3(sky_set_texture, RID, RID, int)

	/* SHADER API */

	BIND0R(RID, shader_create)

	BIND2(shader_set_code, RID, const String &)
	BIND1RC(String, shader_get_code, RID)

	BIND2C(shader_get_param_list, RID, List<PropertyInfo> *)

	BIND3(shader_set_default_texture_param, RID, const StringName &, RID)
	BIND2RC(RID, shader_get_default_texture_param, RID, const StringName &)

	BIND2(shader_add_custom_define, RID, const String &)
	BIND2C(shader_get_custom_defines, RID, Vector<String> *)
	BIND2(shader_remove_custom_define, RID, const String &)

	BIND1(set_shader_async_hidden_forbidden, bool)

	/* COMMON MATERIAL API */

	BIND0R(RID, material_create)

	BIND2(material_set_shader, RID, RID)
	BIND1RC(RID, material_get_shader, RID)

	BIND3(material_set_param, RID, const StringName &, const Variant &)
	BIND2RC(Variant, material_get_param, RID, const StringName &)
	BIND2RC(Variant, material_get_param_default, RID, const StringName &)

	BIND2(material_set_render_priority, RID, int)
	BIND2(material_set_line_width, RID, float)
	BIND2(material_set_next_pass, RID, RID)

	/* MESH API */

	BIND0R(RID, mesh_create)

	BIND10(mesh_add_surface, RID, uint32_t, PrimitiveType, const PoolVector<uint8_t> &, int, const PoolVector<uint8_t> &, int, const AABB &, const Vector<PoolVector<uint8_t>> &, const Vector<AABB> &)

	BIND2(mesh_set_blend_shape_count, RID, int)
	BIND1RC(int, mesh_get_blend_shape_count, RID)

	BIND2(mesh_set_blend_shape_mode, RID, BlendShapeMode)
	BIND1RC(BlendShapeMode, mesh_get_blend_shape_mode, RID)

	BIND4(mesh_surface_update_region, RID, int, int, const PoolVector<uint8_t> &)

	BIND3(mesh_surface_set_material, RID, int, RID)
	BIND2RC(RID, mesh_surface_get_material, RID, int)

	BIND2RC(int, mesh_surface_get_array_len, RID, int)
	BIND2RC(int, mesh_surface_get_array_index_len, RID, int)

	BIND2RC(PoolVector<uint8_t>, mesh_surface_get_array, RID, int)
	BIND2RC(PoolVector<uint8_t>, mesh_surface_get_index_array, RID, int)

	BIND2RC(uint32_t, mesh_surface_get_format, RID, int)
	BIND2RC(PrimitiveType, mesh_surface_get_primitive_type, RID, int)

	BIND2RC(AABB, mesh_surface_get_aabb, RID, int)
	BIND2RC(Vector<PoolVector<uint8_t>>, mesh_surface_get_blend_shapes, RID, int)
	BIND2RC(Vector<AABB>, mesh_surface_get_skeleton_aabb, RID, int)

	BIND2(mesh_remove_surface, RID, int)
	BIND1RC(int, mesh_get_surface_count, RID)

	BIND2(mesh_set_custom_aabb, RID, const AABB &)
	BIND1RC(AABB, mesh_get_custom_aabb, RID)

	BIND1(mesh_clear, RID)

	/* MULTIMESH API */

	BIND0R(RID, multimesh_create)

	BIND5(multimesh_allocate, RID, int, MultimeshTransformFormat, MultimeshColorFormat, MultimeshCustomDataFormat)
	BIND1RC(int, multimesh_get_instance_count, RID)

	BIND2(multimesh_set_mesh, RID, RID)
	BIND3(multimesh_instance_set_transform, RID, int, const Transform &)
	BIND3(multimesh_instance_set_transform_2d, RID, int, const Transform2D &)
	BIND3(multimesh_instance_set_color, RID, int, const Color &)
	BIND3(multimesh_instance_set_custom_data, RID, int, const Color &)

	BIND1RC(RID, multimesh_get_mesh, RID)
	BIND1RC(AABB, multimesh_get_aabb, RID)

	BIND2RC(Transform, multimesh_instance_get_transform, RID, int)
	BIND2RC(Transform2D, multimesh_instance_get_transform_2d, RID, int)
	BIND2RC(Color, multimesh_instance_get_color, RID, int)
	BIND2RC(Color, multimesh_instance_get_custom_data, RID, int)

	BIND2(multimesh_set_as_bulk_array, RID, const PoolVector<float> &)

	BIND3(multimesh_set_as_bulk_array_interpolated, RID, const PoolVector<float> &, const PoolVector<float> &)
	BIND2(multimesh_set_physics_interpolated, RID, bool)
	BIND2(multimesh_set_physics_interpolation_quality, RID, MultimeshPhysicsInterpolationQuality)
	BIND2(multimesh_instance_reset_physics_interpolation, RID, int)

	BIND2(multimesh_set_visible_instances, RID, int)
	BIND1RC(int, multimesh_get_visible_instances, RID)

	/* IMMEDIATE API */

	BIND0R(RID, immediate_create)
	BIND3(immediate_begin, RID, PrimitiveType, RID)
	BIND2(immediate_vertex, RID, const Vector3 &)
	BIND2(immediate_normal, RID, const Vector3 &)
	BIND2(immediate_tangent, RID, const Plane &)
	BIND2(immediate_color, RID, const Color &)
	BIND2(immediate_uv, RID, const Vector2 &)
	BIND2(immediate_uv2, RID, const Vector2 &)
	BIND1(immediate_end, RID)
	BIND1(immediate_clear, RID)
	BIND2(immediate_set_material, RID, RID)
	BIND1RC(RID, immediate_get_material, RID)

	/* SKELETON API */

	BIND0R(RID, skeleton_create)
	BIND3(skeleton_allocate, RID, int, bool)
	BIND1RC(int, skeleton_get_bone_count, RID)
	BIND3(skeleton_bone_set_transform, RID, int, const Transform &)
	BIND2RC(Transform, skeleton_bone_get_transform, RID, int)
	BIND3(skeleton_bone_set_transform_2d, RID, int, const Transform2D &)
	BIND2RC(Transform2D, skeleton_bone_get_transform_2d, RID, int)
	BIND2(skeleton_set_base_transform_2d, RID, const Transform2D &)

	/* Light API */

	BIND0R(RID, directional_light_create)
	BIND0R(RID, omni_light_create)
	BIND0R(RID, spot_light_create)

	BIND2(light_set_color, RID, const Color &)
	BIND3(light_set_param, RID, LightParam, float)
	BIND2(light_set_shadow, RID, bool)
	BIND2(light_set_shadow_color, RID, const Color &)
	BIND2(light_set_projector, RID, RID)
	BIND2(light_set_negative, RID, bool)
	BIND2(light_set_cull_mask, RID, uint32_t)
	BIND2(light_set_reverse_cull_face_mode, RID, bool)
	BIND2(light_set_use_gi, RID, bool)
	BIND2(light_set_bake_mode, RID, LightBakeMode)

	BIND2(light_omni_set_shadow_mode, RID, LightOmniShadowMode)
	BIND2(light_omni_set_shadow_detail, RID, LightOmniShadowDetail)

	BIND2(light_directional_set_shadow_mode, RID, LightDirectionalShadowMode)
	BIND2(light_directional_set_blend_splits, RID, bool)
	BIND2(light_directional_set_shadow_depth_range_mode, RID, LightDirectionalShadowDepthRangeMode)

	/* PROBE API */

	BIND0R(RID, reflection_probe_create)

	BIND2(reflection_probe_set_update_mode, RID, ReflectionProbeUpdateMode)
	BIND2(reflection_probe_set_intensity, RID, float)
	BIND2(reflection_probe_set_interior_ambient, RID, const Color &)
	BIND2(reflection_probe_set_interior_ambient_energy, RID, float)
	BIND2(reflection_probe_set_interior_ambient_probe_contribution, RID, float)
	BIND2(reflection_probe_set_max_distance, RID, float)
	BIND2(reflection_probe_set_extents, RID, const Vector3 &)
	BIND2(reflection_probe_set_origin_offset, RID, const Vector3 &)
	BIND2(reflection_probe_set_as_interior, RID, bool)
	BIND2(reflection_probe_set_enable_box_projection, RID, bool)
	BIND2(reflection_probe_set_enable_shadows, RID, bool)
	BIND2(reflection_probe_set_cull_mask, RID, uint32_t)
	BIND2(reflection_probe_set_resolution, RID, int)

	/* BAKED LIGHT API */

	BIND0R(RID, gi_probe_create)

	BIND2(gi_probe_set_bounds, RID, const AABB &)
	BIND1RC(AABB, gi_probe_get_bounds, RID)

	BIND2(gi_probe_set_cell_size, RID, float)
	BIND1RC(float, gi_probe_get_cell_size, RID)

	BIND2(gi_probe_set_to_cell_xform, RID, const Transform &)
	BIND1RC(Transform, gi_probe_get_to_cell_xform, RID)

	BIND2(gi_probe_set_dynamic_range, RID, int)
	BIND1RC(int, gi_probe_get_dynamic_range, RID)

	BIND2(gi_probe_set_energy, RID, float)
	BIND1RC(float, gi_probe_get_energy, RID)

	BIND2(gi_probe_set_bias, RID, float)
	BIND1RC(float, gi_probe_get_bias, RID)

	BIND2(gi_probe_set_normal_bias, RID, float)
	BIND1RC(float, gi_probe_get_normal_bias, RID)

	BIND2(gi_probe_set_propagation, RID, float)
	BIND1RC(float, gi_probe_get_propagation, RID)

	BIND2(gi_probe_set_interior, RID, bool)
	BIND1RC(bool, gi_probe_is_interior, RID)

	BIND2(gi_probe_set_compress, RID, bool)
	BIND1RC(bool, gi_probe_is_compressed, RID)

	BIND2(gi_probe_set_dynamic_data, RID, const PoolVector<int> &)
	BIND1RC(PoolVector<int>, gi_probe_get_dynamic_data, RID)

	/* LIGHTMAP CAPTURE */

	BIND0R(RID, lightmap_capture_create)

	BIND2(lightmap_capture_set_bounds, RID, const AABB &)
	BIND1RC(AABB, lightmap_capture_get_bounds, RID)

	BIND2(lightmap_capture_set_octree, RID, const PoolVector<uint8_t> &)
	BIND1RC(PoolVector<uint8_t>, lightmap_capture_get_octree, RID)

	BIND2(lightmap_capture_set_octree_cell_transform, RID, const Transform &)
	BIND1RC(Transform, lightmap_capture_get_octree_cell_transform, RID)
	BIND2(lightmap_capture_set_octree_cell_subdiv, RID, int)
	BIND1RC(int, lightmap_capture_get_octree_cell_subdiv, RID)

	BIND2(lightmap_capture_set_energy, RID, float)
	BIND1RC(float, lightmap_capture_get_energy, RID)

	BIND2(lightmap_capture_set_interior, RID, bool)
	BIND1RC(bool, lightmap_capture_is_interior, RID)

	/* PARTICLES */

	BIND0R(RID, particles_create)

	BIND2(particles_set_emitting, RID, bool)
	BIND1R(bool, particles_get_emitting, RID)
	BIND2(particles_set_amount, RID, int)
	BIND2(particles_set_lifetime, RID, float)
	BIND2(particles_set_one_shot, RID, bool)
	BIND2(particles_set_pre_process_time, RID, float)
	BIND2(particles_set_explosiveness_ratio, RID, float)
	BIND2(particles_set_randomness_ratio, RID, float)
	BIND2(particles_set_custom_aabb, RID, const AABB &)
	BIND2(particles_set_speed_scale, RID, float)
	BIND2(particles_set_use_local_coordinates, RID, bool)
	BIND2(particles_set_process_material, RID, RID)
	BIND2(particles_set_fixed_fps, RID, int)
	BIND2(particles_set_fractional_delta, RID, bool)
	BIND1R(bool, particles_is_inactive, RID)
	BIND1(particles_request_process, RID)
	BIND1(particles_restart, RID)

	BIND2(particles_set_draw_order, RID, VS::ParticlesDrawOrder)

	BIND2(particles_set_draw_passes, RID, int)
	BIND3(particles_set_draw_pass_mesh, RID, int, RID)

	BIND1R(AABB, particles_get_current_aabb, RID)
	BIND2(particles_set_emission_transform, RID, const Transform &)

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::scene

	/* EVENT QUEUING */

	/* CAMERA API */

	BIND0R(RID, camera_create)
	BIND4(camera_set_perspective, RID, float, float, float)
	BIND4(camera_set_orthogonal, RID, float, float, float)
	BIND5(camera_set_frustum, RID, float, Vector2, float, float)
	BIND2(camera_set_transform, RID, const Transform &)
	BIND2(camera_set_cull_mask, RID, uint32_t)
	BIND2(camera_set_environment, RID, RID)
	BIND2(camera_set_use_vertical_aspect, RID, bool)

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::viewport

	/* VIEWPORT TARGET API */

	BIND0R(RID, viewport_create)

	BIND2(viewport_set_use_arvr, RID, bool)
	BIND3(viewport_set_size, RID, int, int)

	BIND2(viewport_set_active, RID, bool)
	BIND2(viewport_set_parent_viewport, RID, RID)

	BIND2(viewport_set_clear_mode, RID, ViewportClearMode)

	BIND3(viewport_attach_to_screen, RID, const Rect2 &, int)
	BIND2(viewport_set_render_direct_to_screen, RID, bool)
	BIND1(viewport_detach, RID)

	BIND2(viewport_set_update_mode, RID, ViewportUpdateMode)
	BIND2(viewport_set_vflip, RID, bool)

	BIND1RC(RID, viewport_get_texture, RID)

	BIND2(viewport_set_hide_scenario, RID, bool)
	BIND2(viewport_set_hide_canvas, RID, bool)
	BIND2(viewport_set_disable_environment, RID, bool)
	BIND2(viewport_set_disable_3d, RID, bool)
	BIND2(viewport_set_keep_3d_linear, RID, bool)

	BIND2(viewport_attach_camera, RID, RID)
	BIND2(viewport_set_scenario, RID, RID)
	BIND2(viewport_attach_canvas, RID, RID)

	BIND2(viewport_remove_canvas, RID, RID)
	BIND3(viewport_set_canvas_transform, RID, RID, const Transform2D &)
	BIND2(viewport_set_transparent_background, RID, bool)

	BIND2(viewport_set_global_canvas_transform, RID, const Transform2D &)
	BIND4(viewport_set_canvas_stacking, RID, RID, int, int)
	BIND2(viewport_set_shadow_atlas_size, RID, int)
	BIND3(viewport_set_shadow_atlas_quadrant_subdivision, RID, int, int)
	BIND2(viewport_set_msaa, RID, ViewportMSAA)
	BIND2(viewport_set_use_fxaa, RID, bool)
	BIND2(viewport_set_use_debanding, RID, bool)
	BIND2(viewport_set_sharpen_intensity, RID, float)
	BIND2(viewport_set_hdr, RID, bool)
	BIND2(viewport_set_use_32_bpc_depth, RID, bool)
	BIND2(viewport_set_usage, RID, ViewportUsage)

	BIND2R(int, viewport_get_render_info, RID, ViewportRenderInfo)
	BIND2(viewport_set_debug_draw, RID, ViewportDebugDraw)

	/* ENVIRONMENT API */

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::scene_render

	BIND0R(RID, environment_create)

	BIND2(environment_set_background, RID, EnvironmentBG)
	BIND2(environment_set_sky, RID, RID)
	BIND2(environment_set_sky_custom_fov, RID, float)
	BIND2(environment_set_sky_orientation, RID, const Basis &)
	BIND2(environment_set_bg_color, RID, const Color &)
	BIND2(environment_set_bg_energy, RID, float)
	BIND2(environment_set_canvas_max_layer, RID, int)
	BIND4(environment_set_ambient_light, RID, const Color &, float, float)
	BIND2(environment_set_camera_feed_id, RID, int)
	BIND7(environment_set_ssr, RID, bool, int, float, float, float, bool)
	BIND13(environment_set_ssao, RID, bool, float, float, float, float, float, float, float, const Color &, EnvironmentSSAOQuality, EnvironmentSSAOBlur, float)

	BIND6(environment_set_dof_blur_near, RID, bool, float, float, float, EnvironmentDOFBlurQuality)
	BIND6(environment_set_dof_blur_far, RID, bool, float, float, float, EnvironmentDOFBlurQuality)
	BIND12(environment_set_glow, RID, bool, int, float, float, float, EnvironmentGlowBlendMode, float, float, float, bool, bool)
	BIND3(environment_set_glow_map, RID, float, RID)

	BIND9(environment_set_tonemap, RID, EnvironmentToneMapper, float, float, bool, float, float, float, float)

	BIND6(environment_set_adjustment, RID, bool, float, float, float, RID)

	BIND5(environment_set_fog, RID, bool, const Color &, const Color &, float)
	BIND7(environment_set_fog_depth, RID, bool, float, float, float, bool, float)
	BIND5(environment_set_fog_height, RID, bool, float, float, float)

#undef BINDBASE
#define BINDBASE VSG::scene

	/* SCENARIO API */

	BIND0R(RID, scenario_create)

	BIND2(scenario_set_debug, RID, ScenarioDebugMode)
	BIND2(scenario_set_environment, RID, RID)
	BIND3(scenario_set_reflection_atlas_size, RID, int, int)
	BIND2(scenario_set_fallback_environment, RID, RID)

	/* INSTANCING API */

	BIND0R(RID, instance_create)

	BIND2(instance_set_base, RID, RID)
	BIND2(instance_set_scenario, RID, RID)
	BIND2(instance_set_layer_mask, RID, uint32_t)
	BIND3(instance_set_pivot_data, RID, float, bool)
	BIND2(instance_set_transform, RID, const Transform &)
	BIND2(instance_set_interpolated, RID, bool)
	BIND1(instance_reset_physics_interpolation, RID)
	BIND2(instance_attach_object_instance_id, RID, ObjectID)
	BIND3(instance_set_blend_shape_weight, RID, int, float)
	BIND3(instance_set_surface_material, RID, int, RID)
	BIND2(instance_set_visible, RID, bool)
	BIND5(instance_set_use_lightmap, RID, RID, RID, int, const Rect2 &)

	BIND2(instance_set_custom_aabb, RID, AABB)

	BIND2(instance_attach_skeleton, RID, RID)
	BIND2(instance_set_exterior, RID, bool)

	BIND2(instance_set_extra_visibility_margin, RID, real_t)

	/* PORTALS */

	BIND2(instance_set_portal_mode, RID, InstancePortalMode)

	BIND0R(RID, ghost_create)
	BIND4(ghost_set_scenario, RID, RID, ObjectID, const AABB &)
	BIND2(ghost_update, RID, const AABB &)

	BIND0R(RID, portal_create)
	BIND2(portal_set_scenario, RID, RID)
	BIND3(portal_set_geometry, RID, const Vector<Vector3> &, real_t)
	BIND4(portal_link, RID, RID, RID, bool)
	BIND2(portal_set_active, RID, bool)

	/* ROOMGROUPS */

	BIND0R(RID, roomgroup_create)
	BIND2(roomgroup_prepare, RID, ObjectID)
	BIND2(roomgroup_set_scenario, RID, RID)
	BIND2(roomgroup_add_room, RID, RID)

	/* OCCLUDERS */

	BIND0R(RID, occluder_instance_create)
	BIND2(occluder_instance_set_scenario, RID, RID)
	BIND2(occluder_instance_link_resource, RID, RID)
	BIND2(occluder_instance_set_transform, RID, const Transform &)
	BIND2(occluder_instance_set_active, RID, bool)

	BIND0R(RID, occluder_resource_create)
	BIND2(occluder_resource_prepare, RID, OccluderType)
	BIND2(occluder_resource_spheres_update, RID, const Vector<Plane> &)
	BIND2(occluder_resource_mesh_update, RID, const Geometry::OccluderMeshData &)
	BIND1(set_use_occlusion_culling, bool)
	BIND1RC(Geometry::MeshData, occlusion_debug_get_current_polys, RID)

	/* ROOMS */

	BIND0R(RID, room_create)
	BIND2(room_set_scenario, RID, RID)
	BIND4(room_add_instance, RID, RID, const AABB &, const Vector<Vector3> &)
	BIND3(room_add_ghost, RID, ObjectID, const AABB &)
	BIND5(room_set_bound, RID, ObjectID, const Vector<Plane> &, const AABB &, const Vector<Vector3> &)
	BIND2(room_prepare, RID, int32_t)
	BIND1(rooms_and_portals_clear, RID)
	BIND2(rooms_unload, RID, String)
	BIND8(rooms_finalize, RID, bool, bool, bool, bool, String, bool, bool)
	BIND4(rooms_override_camera, RID, bool, const Vector3 &, const Vector<Plane> *)
	BIND2(rooms_set_active, RID, bool)
	BIND3(rooms_set_params, RID, int, real_t)
	BIND3(rooms_set_debug_feature, RID, RoomsDebugFeature, bool)
	BIND2(rooms_update_gameplay_monitor, RID, const Vector<Vector3> &)

	// don't use this in a game
	BIND1RC(bool, rooms_is_loaded, RID)

	// Callbacks
	BIND1(callbacks_register, VisualServerCallbacks *)

	// don't use these in a game!
	BIND2RC(Vector<ObjectID>, instances_cull_aabb, const AABB &, RID)
	BIND3RC(Vector<ObjectID>, instances_cull_ray, const Vector3 &, const Vector3 &, RID)
	BIND2RC(Vector<ObjectID>, instances_cull_convex, const Vector<Plane> &, RID)

	BIND3(instance_geometry_set_flag, RID, InstanceFlags, bool)
	BIND2(instance_geometry_set_cast_shadows_setting, RID, ShadowCastingSetting)
	BIND2(instance_geometry_set_material_override, RID, RID)
	BIND2(instance_geometry_set_material_overlay, RID, RID)

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::canvas

	/* CANVAS (2D) */

	BIND0R(RID, canvas_create)
	BIND3(canvas_set_item_mirroring, RID, RID, const Point2 &)
	BIND2(canvas_set_modulate, RID, const Color &)
	BIND3(canvas_set_parent, RID, RID, float)
	BIND1(canvas_set_disable_scale, bool)

	BIND0R(RID, canvas_item_create)
	BIND2(canvas_item_set_parent, RID, RID)
	BIND2(canvas_item_set_name, RID, String)

	BIND2(canvas_item_set_visible, RID, bool)
	BIND2(canvas_item_set_light_mask, RID, int)

	BIND2(canvas_item_set_update_when_visible, RID, bool)

	BIND2(canvas_item_set_transform, RID, const Transform2D &)
	BIND2(canvas_item_set_clip, RID, bool)
	BIND2(canvas_item_set_distance_field_mode, RID, bool)
	BIND3(canvas_item_set_custom_rect, RID, bool, const Rect2 &)
	BIND2(canvas_item_set_modulate, RID, const Color &)
	BIND2(canvas_item_set_self_modulate, RID, const Color &)

	BIND2(canvas_item_set_draw_behind_parent, RID, bool)
	BIND2(canvas_item_set_use_identity_transform, RID, bool)

	BIND6(canvas_item_add_line, RID, const Point2 &, const Point2 &, const Color &, float, bool)
	BIND5(canvas_item_add_polyline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	BIND5(canvas_item_add_multiline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	BIND3(canvas_item_add_rect, RID, const Rect2 &, const Color &)
	BIND4(canvas_item_add_circle, RID, const Point2 &, float, const Color &)
	BIND7(canvas_item_add_texture_rect, RID, const Rect2 &, RID, bool, const Color &, bool, RID)
	BIND8(canvas_item_add_texture_rect_region, RID, const Rect2 &, RID, const Rect2 &, const Color &, bool, RID, bool)
	BIND7(canvas_item_add_texture_multirect_region, RID, const Vector<Rect2> &, RID, const Vector<Rect2> &, const Color &, uint32_t, RID)
	BIND11(canvas_item_add_nine_patch, RID, const Rect2 &, const Rect2 &, RID, const Vector2 &, const Vector2 &, NinePatchAxisMode, NinePatchAxisMode, bool, const Color &, RID)
	BIND7(canvas_item_add_primitive, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID, float, RID)
	BIND7(canvas_item_add_polygon, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID, RID, bool)
	BIND12(canvas_item_add_triangle_array, RID, const Vector<int> &, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, const Vector<int> &, const Vector<float> &, RID, int, RID, bool, bool)
	BIND6(canvas_item_add_mesh, RID, const RID &, const Transform2D &, const Color &, RID, RID)
	BIND4(canvas_item_add_multimesh, RID, RID, RID, RID)
	BIND4(canvas_item_add_particles, RID, RID, RID, RID)
	BIND2(canvas_item_add_set_transform, RID, const Transform2D &)
	BIND2(canvas_item_add_clip_ignore, RID, bool)
	BIND2(canvas_item_set_sort_children_by_y, RID, bool)
	BIND2(canvas_item_set_z_index, RID, int)
	BIND2(canvas_item_set_z_as_relative_to_parent, RID, bool)
	BIND3(canvas_item_set_copy_to_backbuffer, RID, bool, const Rect2 &)
	BIND1(canvas_item_clear, RID)
	BIND2(canvas_item_set_draw_index, RID, int)
	BIND2(canvas_item_set_material, RID, RID)
	BIND2(canvas_item_set_use_parent_material, RID, bool)

	BIND2(canvas_item_attach_skeleton, RID, RID)
	BIND2(canvas_item_set_skeleton_relative_xform, RID, Transform2D)
	BIND1R(Rect2, _debug_canvas_item_get_rect, RID)
	BIND1R(Rect2, _debug_canvas_item_get_local_bound, RID)

	BIND2(canvas_item_set_interpolated, RID, bool)
	BIND1(canvas_item_reset_physics_interpolation, RID)
	BIND2(canvas_item_transform_physics_interpolation, RID, const Transform2D &)

	BIND0R(RID, canvas_light_create)
	BIND2(canvas_light_attach_to_canvas, RID, RID)
	BIND2(canvas_light_set_enabled, RID, bool)
	BIND2(canvas_light_set_scale, RID, float)
	BIND2(canvas_light_set_transform, RID, const Transform2D &)
	BIND2(canvas_light_set_texture, RID, RID)
	BIND2(canvas_light_set_texture_offset, RID, const Vector2 &)
	BIND2(canvas_light_set_color, RID, const Color &)
	BIND2(canvas_light_set_height, RID, float)
	BIND2(canvas_light_set_energy, RID, float)
	BIND3(canvas_light_set_z_range, RID, int, int)
	BIND3(canvas_light_set_layer_range, RID, int, int)
	BIND2(canvas_light_set_item_cull_mask, RID, int)
	BIND2(canvas_light_set_item_shadow_cull_mask, RID, int)

	BIND2(canvas_light_set_mode, RID, CanvasLightMode)

	BIND2(canvas_light_set_shadow_enabled, RID, bool)
	BIND2(canvas_light_set_shadow_buffer_size, RID, int)
	BIND2(canvas_light_set_shadow_gradient_length, RID, float)
	BIND2(canvas_light_set_shadow_filter, RID, CanvasLightShadowFilter)
	BIND2(canvas_light_set_shadow_color, RID, const Color &)
	BIND2(canvas_light_set_shadow_smooth, RID, float)

	BIND2(canvas_light_set_interpolated, RID, bool)
	BIND1(canvas_light_reset_physics_interpolation, RID)
	BIND2(canvas_light_transform_physics_interpolation, RID, const Transform2D &)

	BIND0R(RID, canvas_light_occluder_create)
	BIND2(canvas_light_occluder_attach_to_canvas, RID, RID)
	BIND2(canvas_light_occluder_set_enabled, RID, bool)
	BIND2(canvas_light_occluder_set_polygon, RID, RID)
	BIND2(canvas_light_occluder_set_transform, RID, const Transform2D &)
	BIND2(canvas_light_occluder_set_light_mask, RID, int)

	BIND2(canvas_light_occluder_set_interpolated, RID, bool)
	BIND1(canvas_light_occluder_reset_physics_interpolation, RID)
	BIND2(canvas_light_occluder_transform_physics_interpolation, RID, const Transform2D &)

	BIND0R(RID, canvas_occluder_polygon_create)
	BIND3(canvas_occluder_polygon_set_shape, RID, const PoolVector<Vector2> &, bool)
	BIND2(canvas_occluder_polygon_set_shape_as_lines, RID, const PoolVector<Vector2> &)

	BIND2(canvas_occluder_polygon_set_cull_mode, RID, CanvasOccluderPolygonCullMode)

	/* BLACK BARS */

	virtual void black_bars_set_margins(int p_left, int p_top, int p_right, int p_bottom);
	virtual void black_bars_set_images(RID p_left, RID p_top, RID p_right, RID p_bottom);

	/* FREE */

	virtual void free(RID p_rid); ///< free RIDs associated with the visual server

	/* EVENT QUEUING */

	virtual void request_frame_drawn_callback(Object *p_where, const StringName &p_method, const Variant &p_userdata);

	virtual void tick();
	virtual void pre_draw(bool p_will_draw);
	virtual void draw(bool p_swap_buffers, double frame_step);
	virtual void sync();
	virtual bool has_changed(ChangedPriority p_priority = CHANGED_PRIORITY_ANY) const;
	virtual void init();
	virtual void finish();

	virtual void set_physics_interpolation_enabled(bool p_enabled);

	/* STATUS INFORMATION */

	virtual uint64_t get_render_info(RenderInfo p_info);
	virtual String get_video_adapter_name() const;
	virtual String get_video_adapter_vendor() const;

	virtual RID get_test_cube();

	/* TESTING */

	virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true);
	virtual void set_default_clear_color(const Color &p_color);
	virtual void set_shader_time_scale(float p_scale);

	virtual bool has_feature(Features p_feature) const;

	virtual bool has_os_feature(const String &p_feature) const;
	virtual void set_debug_generate_wireframes(bool p_generate);

	virtual void call_set_use_vsync(bool p_enable);

	virtual bool is_low_end() const;

	VisualServerRaster();
	~VisualServerRaster();

#undef DISPLAY_CHANGED

#undef BIND0R
#undef BIND1RC
#undef BIND2RC
#undef BIND3RC
#undef BIND4RC

#undef BIND1
#undef BIND2
#undef BIND3
#undef BIND4
#undef BIND5
#undef BIND6
#undef BIND7
#undef BIND8
#undef BIND9
#undef BIND10
};

#endif // VISUAL_SERVER_RASTER_H
