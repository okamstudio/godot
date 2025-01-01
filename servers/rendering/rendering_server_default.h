/**************************************************************************/
/*  rendering_server_default.h                                            */
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

#ifndef RENDERING_SERVER_DEFAULT_H
#define RENDERING_SERVER_DEFAULT_H

#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "core/templates/command_queue_mt.h"
#include "core/templates/hash_map.h"
#include "renderer_canvas_cull.h"
#include "renderer_viewport.h"
#include "rendering_server_globals.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering_server.h"
#include "servers/server_wrap_mt_common.h"

class RenderingServerDefault : public RenderingServer {
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

	static int changes;
	RID test_cube;

	List<Callable> frame_drawn_callbacks;

	static void _changes_changed() {}

	uint64_t frame_profile_frame = 0;
	Vector<FrameProfileArea> frame_profile;

	double frame_setup_time = 0;

	//for printing
	bool print_gpu_profile = false;
	HashMap<String, float> print_gpu_profile_task_time;
	uint64_t print_frame_profile_ticks_from = 0;
	uint32_t print_frame_profile_frame_count = 0;

	mutable CommandQueueMT command_queue;

	Thread::ID server_thread = Thread::MAIN_ID;
	WorkerThreadPool::TaskID server_task_id = WorkerThreadPool::INVALID_TASK_ID;
	bool exit = false;
	bool create_thread = false;

	void _assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id);
	void _thread_exit();
	void _thread_loop();

	void _draw(bool p_swap_buffers, double frame_step);
	void _run_post_draw_steps();
	void _init();
	void _finish();

	void _free(RID p_rid);

	void _call_on_render_thread(const Callable &p_callable);

public:
	//if editor is redrawing when it shouldn't, enable this and put a breakpoint in _changes_changed()
	//#define DEBUG_CHANGES

#ifdef DEBUG_CHANGES
	_FORCE_INLINE_ static void redraw_request() {
		changes++;
		_changes_changed();
	}

#else
	_FORCE_INLINE_ static void redraw_request() {
		changes++;
	}
#endif

#define WRITE_ACTION redraw_request();

#ifdef DEBUG_SYNC
#define SYNC_DEBUG print_line("sync on: " + String(__FUNCTION__));
#else
#define SYNC_DEBUG
#endif

#ifdef DEBUG_ENABLED
#define MAIN_THREAD_SYNC_WARN WARN_PRINT("Call to " + String(__FUNCTION__) + " causing RenderingServer synchronizations on every frame. This significantly affects performance.");
#endif

#include "servers/server_wrap_mt_common.h"

	/* TEXTURE API */

#define ServerName RendererTextureStorage
#define server_name RSG::texture_storage

#define FUNCRIDTEX0(m_type)                                                                              \
	virtual RID m_type##_create() override {                                                             \
		RID ret = RSG::texture_storage->texture_allocate();                                              \
		if (Thread::get_caller_id() == server_thread || RSG::rasterizer->can_create_resources_async()) { \
			RSG::texture_storage->m_type##_initialize(ret);                                              \
		} else {                                                                                         \
			command_queue.push(RSG::texture_storage, &RendererTextureStorage::m_type##_initialize, ret); \
		}                                                                                                \
		return ret;                                                                                      \
	}

#define FUNCRIDTEX1(m_type, m_type1)                                                                         \
	virtual RID m_type##_create(m_type1 p1) override {                                                       \
		RID ret = RSG::texture_storage->texture_allocate();                                                  \
		if (Thread::get_caller_id() == server_thread || RSG::rasterizer->can_create_resources_async()) {     \
			RSG::texture_storage->m_type##_initialize(ret, p1);                                              \
		} else {                                                                                             \
			command_queue.push(RSG::texture_storage, &RendererTextureStorage::m_type##_initialize, ret, p1); \
		}                                                                                                    \
		return ret;                                                                                          \
	}

#define FUNCRIDTEX2(m_type, m_type1, m_type2)                                                                    \
	virtual RID m_type##_create(m_type1 p1, m_type2 p2) override {                                               \
		RID ret = RSG::texture_storage->texture_allocate();                                                      \
		if (Thread::get_caller_id() == server_thread || RSG::rasterizer->can_create_resources_async()) {         \
			RSG::texture_storage->m_type##_initialize(ret, p1, p2);                                              \
		} else {                                                                                                 \
			command_queue.push(RSG::texture_storage, &RendererTextureStorage::m_type##_initialize, ret, p1, p2); \
		}                                                                                                        \
		return ret;                                                                                              \
	}

#define FUNCRIDTEX3(m_type, m_type1, m_type2, m_type3)                                                               \
	virtual RID m_type##_create(m_type1 p1, m_type2 p2, m_type3 p3) override {                                       \
		RID ret = RSG::texture_storage->texture_allocate();                                                          \
		if (Thread::get_caller_id() == server_thread || RSG::rasterizer->can_create_resources_async()) {             \
			RSG::texture_storage->m_type##_initialize(ret, p1, p2, p3);                                              \
		} else {                                                                                                     \
			command_queue.push(RSG::texture_storage, &RendererTextureStorage::m_type##_initialize, ret, p1, p2, p3); \
		}                                                                                                            \
		return ret;                                                                                                  \
	}

#define FUNCRIDTEX6(m_type, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6)                                                \
	virtual RID m_type##_create(m_type1 p1, m_type2 p2, m_type3 p3, m_type4 p4, m_type5 p5, m_type6 p6) override {               \
		RID ret = RSG::texture_storage->texture_allocate();                                                                      \
		if (Thread::get_caller_id() == server_thread || RSG::rasterizer->can_create_resources_async()) {                         \
			RSG::texture_storage->m_type##_initialize(ret, p1, p2, p3, p4, p5, p6);                                              \
		} else {                                                                                                                 \
			command_queue.push(RSG::texture_storage, &RendererTextureStorage::m_type##_initialize, ret, p1, p2, p3, p4, p5, p6); \
		}                                                                                                                        \
		return ret;                                                                                                              \
	}

	//these go pass-through, as they can be called from any thread
	FUNCRIDTEX1(texture_2d, const Ref<Image> &)
	FUNCRIDTEX2(texture_2d_layered, const Vector<Ref<Image>> &, TextureLayeredType)
	FUNCRIDTEX6(texture_3d, Image::Format, int, int, int, bool, const Vector<Ref<Image>> &)
	FUNCRIDTEX3(texture_external, int, int, uint64_t)
	FUNCRIDTEX1(texture_proxy, RID)

	// Called directly, not through the command queue.
	virtual RID texture_create_from_native_handle(TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, TextureLayeredType p_layered_type = TEXTURE_LAYERED_2D_ARRAY) override {
		return RSG::texture_storage->texture_create_from_native_handle(p_type, p_format, p_native_handle, p_width, p_height, p_depth, p_layers, p_layered_type);
	}

	//these go through command queue if they are in another thread
	FUNC3(texture_2d_update, RID, const Ref<Image> &, int)
	FUNC2(texture_3d_update, RID, const Vector<Ref<Image>> &)
	FUNC4(texture_external_update, RID, int, int, uint64_t)
	FUNC2(texture_proxy_update, RID, RID)

	//these also go pass-through
	FUNCRIDTEX0(texture_2d_placeholder)
	FUNCRIDTEX1(texture_2d_layered_placeholder, TextureLayeredType)
	FUNCRIDTEX0(texture_3d_placeholder)

	FUNC1RC(Ref<Image>, texture_2d_get, RID)
	FUNC2RC(Ref<Image>, texture_2d_layer_get, RID, int)
	FUNC1RC(Vector<Ref<Image>>, texture_3d_get, RID)

	FUNC2(texture_replace, RID, RID)

	FUNC3(texture_set_size_override, RID, int, int)
// FIXME: Disabled during Vulkan refactoring, should be ported.
#if 0
	FUNC2(texture_bind, RID, uint32_t)
#endif

	FUNC3(texture_set_detect_3d_callback, RID, TextureDetectCallback, void *)
	FUNC3(texture_set_detect_normal_callback, RID, TextureDetectCallback, void *)
	FUNC3(texture_set_detect_roughness_callback, RID, TextureDetectRoughnessCallback, void *)

	FUNC2(texture_set_path, RID, const String &)
	FUNC1RC(String, texture_get_path, RID)

	FUNC1RC(Image::Format, texture_get_format, RID)

	FUNC1(texture_debug_usage, List<TextureInfo> *)

	FUNC2(texture_set_force_redraw_if_visible, RID, bool)
	FUNCRIDTEX2(texture_rd, const RID &, const RS::TextureLayeredType)
	FUNC2RC(RID, texture_get_rd_texture, RID, bool)
	FUNC2RC(uint64_t, texture_get_native_handle, RID, bool)

	/* SHADER API */

#undef ServerName
#undef server_name

#define ServerName RendererMaterialStorage
#define server_name RSG::material_storage

	FUNCRIDSPLIT(shader)

	virtual RID shader_create_from_code(const String &p_code, const String &p_path_hint = String()) override {
		RID shader = RSG::material_storage->shader_allocate();
		bool using_server_thread = Thread::get_caller_id() == server_thread;
		if (using_server_thread || RSG::rasterizer->can_create_resources_async()) {
			if (using_server_thread) {
				command_queue.flush_if_pending();
			}

			RSG::material_storage->shader_initialize(shader);
			RSG::material_storage->shader_set_code(shader, p_code);
			RSG::material_storage->shader_set_path_hint(shader, p_path_hint);
		} else {
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::shader_initialize, shader);
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::shader_set_code, shader, p_code);
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::shader_set_path_hint, shader, p_path_hint);
		}

		return shader;
	}

	FUNC2(shader_set_code, RID, const String &)
	FUNC2(shader_set_path_hint, RID, const String &)
	FUNC1RC(String, shader_get_code, RID)

	FUNC2SC(get_shader_parameter_list, RID, List<PropertyInfo> *)

	FUNC4(shader_set_default_texture_parameter, RID, const StringName &, RID, int)
	FUNC3RC(RID, shader_get_default_texture_parameter, RID, const StringName &, int)
	FUNC2RC(Variant, shader_get_parameter_default, RID, const StringName &)

	FUNC1RC(ShaderNativeSourceCode, shader_get_native_source_code, RID)

	/* COMMON MATERIAL API */

	FUNCRIDSPLIT(material)

	virtual RID material_create_from_shader(RID p_next_pass, int p_render_priority, RID p_shader) override {
		RID material = RSG::material_storage->material_allocate();
		bool using_server_thread = Thread::get_caller_id() == server_thread;
		if (using_server_thread || RSG::rasterizer->can_create_resources_async()) {
			if (using_server_thread) {
				command_queue.flush_if_pending();
			}

			RSG::material_storage->material_initialize(material);
			RSG::material_storage->material_set_next_pass(material, p_next_pass);
			RSG::material_storage->material_set_render_priority(material, p_render_priority);
			RSG::material_storage->material_set_shader(material, p_shader);
		} else {
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::material_initialize, material);
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::material_set_next_pass, material, p_next_pass);
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::material_set_render_priority, material, p_render_priority);
			command_queue.push(RSG::material_storage, &RendererMaterialStorage::material_set_shader, material, p_shader);
		}

		return material;
	}

	FUNC2(material_set_shader, RID, RID)

	FUNC3(material_set_param, RID, const StringName &, const Variant &)
	FUNC2RC(Variant, material_get_param, RID, const StringName &)

	FUNC2(material_set_render_priority, RID, int)
	FUNC2(material_set_next_pass, RID, RID)

	/* MESH API */

//from now on, calls forwarded to this singleton
#undef ServerName
#undef server_name

#define ServerName RendererMeshStorage
#define server_name RSG::mesh_storage

	virtual RID mesh_create_from_surfaces(const Vector<SurfaceData> &p_surfaces, int p_blend_shape_count = 0) override {
		RID mesh = RSG::mesh_storage->mesh_allocate();

		bool using_server_thread = Thread::get_caller_id() == server_thread;
		if (using_server_thread || RSG::rasterizer->can_create_resources_async()) {
			if (using_server_thread) {
				command_queue.flush_if_pending();
			}
			RSG::mesh_storage->mesh_initialize(mesh);
			RSG::mesh_storage->mesh_set_blend_shape_count(mesh, p_blend_shape_count);
			for (int i = 0; i < p_surfaces.size(); i++) {
				RSG::mesh_storage->mesh_add_surface(mesh, p_surfaces[i]);
			}
			RSG::scene->mesh_generate_pipelines(mesh, using_server_thread);
		} else {
			command_queue.push(RSG::mesh_storage, &RendererMeshStorage::mesh_initialize, mesh);
			command_queue.push(RSG::mesh_storage, &RendererMeshStorage::mesh_set_blend_shape_count, mesh, p_blend_shape_count);
			for (int i = 0; i < p_surfaces.size(); i++) {
				command_queue.push(RSG::mesh_storage, &RendererMeshStorage::mesh_add_surface, mesh, p_surfaces[i]);
			}
			command_queue.push(RSG::scene, &RenderingMethod::mesh_generate_pipelines, mesh, true);
		}

		return mesh;
	}

	FUNC2(mesh_set_blend_shape_count, RID, int)

	FUNCRIDSPLIT(mesh)

	FUNC2(mesh_add_surface, RID, const SurfaceData &)

	FUNC1RC(int, mesh_get_blend_shape_count, RID)

	FUNC2(mesh_set_blend_shape_mode, RID, BlendShapeMode)
	FUNC1RC(BlendShapeMode, mesh_get_blend_shape_mode, RID)

	FUNC4(mesh_surface_update_vertex_region, RID, int, int, const Vector<uint8_t> &)
	FUNC4(mesh_surface_update_attribute_region, RID, int, int, const Vector<uint8_t> &)
	FUNC4(mesh_surface_update_skin_region, RID, int, int, const Vector<uint8_t> &)

	FUNC3(mesh_surface_set_material, RID, int, RID)
	FUNC2RC(RID, mesh_surface_get_material, RID, int)

	FUNC2RC(SurfaceData, mesh_get_surface, RID, int)

	FUNC1RC(int, mesh_get_surface_count, RID)

	FUNC2(mesh_set_custom_aabb, RID, const AABB &)
	FUNC1RC(AABB, mesh_get_custom_aabb, RID)

	FUNC2(mesh_set_path, RID, const String &)
	FUNC1RC(String, mesh_get_path, RID)

	FUNC2(mesh_set_shadow_mesh, RID, RID)

	FUNC2(mesh_surface_remove, RID, int)
	FUNC1(mesh_clear, RID)

	/* MULTIMESH API */

	FUNCRIDSPLIT(multimesh)

	FUNC5(multimesh_allocate_data, RID, int, MultimeshTransformFormat, bool, bool)
	FUNC1RC(int, multimesh_get_instance_count, RID)

	FUNC2(multimesh_set_mesh, RID, RID)
	FUNC3(multimesh_instance_set_transform, RID, int, const Transform3D &)
	FUNC3(multimesh_instance_set_transform_2d, RID, int, const Transform2D &)
	FUNC3(multimesh_instance_set_color, RID, int, const Color &)
	FUNC3(multimesh_instance_set_custom_data, RID, int, const Color &)

	FUNC2(multimesh_set_custom_aabb, RID, const AABB &)
	FUNC1RC(AABB, multimesh_get_custom_aabb, RID)

	FUNC1RC(RID, multimesh_get_mesh, RID)
	FUNC1RC(AABB, multimesh_get_aabb, RID)

	FUNC2RC(Transform3D, multimesh_instance_get_transform, RID, int)
	FUNC2RC(Transform2D, multimesh_instance_get_transform_2d, RID, int)
	FUNC2RC(Color, multimesh_instance_get_color, RID, int)
	FUNC2RC(Color, multimesh_instance_get_custom_data, RID, int)

	FUNC2(multimesh_set_buffer, RID, const Vector<float> &)
	FUNC1RC(RID, multimesh_get_buffer_rd_rid, RID)
	FUNC1RC(Vector<float>, multimesh_get_buffer, RID)

	FUNC3(multimesh_set_buffer_interpolated, RID, const Vector<float> &, const Vector<float> &)
	FUNC2(multimesh_set_physics_interpolated, RID, bool)
	FUNC2(multimesh_set_physics_interpolation_quality, RID, MultimeshPhysicsInterpolationQuality)
	FUNC2(multimesh_instance_reset_physics_interpolation, RID, int)

	FUNC2(multimesh_set_visible_instances, RID, int)
	FUNC1RC(int, multimesh_get_visible_instances, RID)

	/* SKELETON API */

	FUNCRIDSPLIT(skeleton)
	FUNC3(skeleton_allocate_data, RID, int, bool)
	FUNC1RC(int, skeleton_get_bone_count, RID)
	FUNC3(skeleton_bone_set_transform, RID, int, const Transform3D &)
	FUNC2RC(Transform3D, skeleton_bone_get_transform, RID, int)
	FUNC3(skeleton_bone_set_transform_2d, RID, int, const Transform2D &)
	FUNC2RC(Transform2D, skeleton_bone_get_transform_2d, RID, int)
	FUNC2(skeleton_set_base_transform_2d, RID, const Transform2D &)

	/* Light API */
#undef ServerName
#undef server_name

#define ServerName RendererLightStorage
#define server_name RSG::light_storage

	FUNCRIDSPLIT(directional_light)
	FUNCRIDSPLIT(omni_light)
	FUNCRIDSPLIT(spot_light)

	FUNC2(light_set_color, RID, const Color &)
	FUNC3(light_set_param, RID, LightParam, float)
	FUNC2(light_set_shadow, RID, bool)
	FUNC2(light_set_projector, RID, RID)
	FUNC2(light_set_negative, RID, bool)
	FUNC2(light_set_cull_mask, RID, uint32_t)
	FUNC5(light_set_distance_fade, RID, bool, float, float, float)
	FUNC2(light_set_reverse_cull_face_mode, RID, bool)
	FUNC2(light_set_shadow_caster_mask, RID, uint32_t)
	FUNC2(light_set_bake_mode, RID, LightBakeMode)
	FUNC2(light_set_max_sdfgi_cascade, RID, uint32_t)

	FUNC2(light_omni_set_shadow_mode, RID, LightOmniShadowMode)

	FUNC2(light_directional_set_shadow_mode, RID, LightDirectionalShadowMode)
	FUNC2(light_directional_set_blend_splits, RID, bool)
	FUNC2(light_directional_set_sky_mode, RID, LightDirectionalSkyMode)

	/* PROBE API */

	FUNCRIDSPLIT(reflection_probe)

	FUNC2(reflection_probe_set_update_mode, RID, ReflectionProbeUpdateMode)
	FUNC2(reflection_probe_set_intensity, RID, float)
	FUNC2(reflection_probe_set_blend_distance, RID, float)
	FUNC2(reflection_probe_set_ambient_color, RID, const Color &)
	FUNC2(reflection_probe_set_ambient_energy, RID, float)
	FUNC2(reflection_probe_set_ambient_mode, RID, ReflectionProbeAmbientMode)
	FUNC2(reflection_probe_set_max_distance, RID, float)
	FUNC2(reflection_probe_set_size, RID, const Vector3 &)
	FUNC2(reflection_probe_set_origin_offset, RID, const Vector3 &)
	FUNC2(reflection_probe_set_as_interior, RID, bool)
	FUNC2(reflection_probe_set_enable_box_projection, RID, bool)
	FUNC2(reflection_probe_set_enable_shadows, RID, bool)
	FUNC2(reflection_probe_set_cull_mask, RID, uint32_t)
	FUNC2(reflection_probe_set_reflection_mask, RID, uint32_t)
	FUNC2(reflection_probe_set_resolution, RID, int)
	FUNC2(reflection_probe_set_mesh_lod_threshold, RID, float)

	/* LIGHTMAP */

	FUNCRIDSPLIT(lightmap)

	FUNC3(lightmap_set_textures, RID, RID, bool)
	FUNC2(lightmap_set_probe_bounds, RID, const AABB &)
	FUNC2(lightmap_set_probe_interior, RID, bool)
	FUNC5(lightmap_set_probe_capture_data, RID, const PackedVector3Array &, const PackedColorArray &, const PackedInt32Array &, const PackedInt32Array &)
	FUNC2(lightmap_set_baked_exposure_normalization, RID, float)
	FUNC1RC(PackedVector3Array, lightmap_get_probe_capture_points, RID)
	FUNC1RC(PackedColorArray, lightmap_get_probe_capture_sh, RID)
	FUNC1RC(PackedInt32Array, lightmap_get_probe_capture_tetrahedra, RID)
	FUNC1RC(PackedInt32Array, lightmap_get_probe_capture_bsp_tree, RID)
	FUNC1(lightmap_set_probe_capture_update_speed, float)

	FUNC2(lightmap_set_shadowmask_textures, RID, RID)
	FUNC1R(ShadowmaskMode, lightmap_get_shadowmask_mode, RID)
	FUNC2(lightmap_set_shadowmask_mode, RID, ShadowmaskMode)

	/* Shadow Atlas */
	FUNC0R(RID, shadow_atlas_create)
	FUNC3(shadow_atlas_set_size, RID, int, bool)
	FUNC3(shadow_atlas_set_quadrant_subdivision, RID, int, int)

	FUNC2(directional_shadow_atlas_set_size, int, bool)

	/* DECAL API */

#undef ServerName
#undef server_name

#define ServerName RendererTextureStorage
#define server_name RSG::texture_storage

	FUNCRIDSPLIT(decal)

	FUNC2(decal_set_size, RID, const Vector3 &)
	FUNC3(decal_set_texture, RID, DecalTexture, RID)
	FUNC2(decal_set_emission_energy, RID, float)
	FUNC2(decal_set_albedo_mix, RID, float)
	FUNC2(decal_set_modulate, RID, const Color &)
	FUNC2(decal_set_cull_mask, RID, uint32_t)
	FUNC4(decal_set_distance_fade, RID, bool, float, float)
	FUNC3(decal_set_fade, RID, float, float)
	FUNC2(decal_set_normal_fade, RID, float)

	/* BAKED LIGHT API */

//from now on, calls forwarded to this singleton
#undef ServerName
#undef server_name

#define ServerName RendererGI
#define server_name RSG::gi

	FUNCRIDSPLIT(voxel_gi)

	FUNC8(voxel_gi_allocate_data, RID, const Transform3D &, const AABB &, const Vector3i &, const Vector<uint8_t> &, const Vector<uint8_t> &, const Vector<uint8_t> &, const Vector<int> &)

	FUNC1RC(AABB, voxel_gi_get_bounds, RID)
	FUNC1RC(Vector3i, voxel_gi_get_octree_size, RID)
	FUNC1RC(Vector<uint8_t>, voxel_gi_get_octree_cells, RID)
	FUNC1RC(Vector<uint8_t>, voxel_gi_get_data_cells, RID)
	FUNC1RC(Vector<uint8_t>, voxel_gi_get_distance_field, RID)
	FUNC1RC(Vector<int>, voxel_gi_get_level_counts, RID)
	FUNC1RC(Transform3D, voxel_gi_get_to_cell_xform, RID)

	FUNC2(voxel_gi_set_dynamic_range, RID, float)
	FUNC2(voxel_gi_set_propagation, RID, float)
	FUNC2(voxel_gi_set_energy, RID, float)
	FUNC2(voxel_gi_set_baked_exposure_normalization, RID, float)
	FUNC2(voxel_gi_set_bias, RID, float)
	FUNC2(voxel_gi_set_normal_bias, RID, float)
	FUNC2(voxel_gi_set_interior, RID, bool)
	FUNC2(voxel_gi_set_use_two_bounces, RID, bool)

	FUNC0(sdfgi_reset)

	/* PARTICLES */

#undef ServerName
#undef server_name

#define ServerName RendererParticlesStorage
#define server_name RSG::particles_storage

	FUNCRIDSPLIT(particles)

	FUNC2(particles_set_mode, RID, ParticlesMode)
	FUNC2(particles_set_emitting, RID, bool)
	FUNC1R(bool, particles_get_emitting, RID)
	FUNC2(particles_set_amount, RID, int)
	FUNC2(particles_set_amount_ratio, RID, float)
	FUNC2(particles_set_lifetime, RID, double)
	FUNC2(particles_set_one_shot, RID, bool)
	FUNC2(particles_set_pre_process_time, RID, double)
	FUNC2(particles_set_explosiveness_ratio, RID, float)
	FUNC2(particles_set_randomness_ratio, RID, float)
	FUNC2(particles_set_custom_aabb, RID, const AABB &)
	FUNC2(particles_set_speed_scale, RID, double)
	FUNC2(particles_set_use_local_coordinates, RID, bool)
	FUNC2(particles_set_process_material, RID, RID)
	FUNC2(particles_set_fixed_fps, RID, int)
	FUNC2(particles_set_interpolate, RID, bool)
	FUNC2(particles_set_fractional_delta, RID, bool)
	FUNC1R(bool, particles_is_inactive, RID)
	FUNC3(particles_set_trails, RID, bool, float)
	FUNC2(particles_set_trail_bind_poses, RID, const Vector<Transform3D> &)

	FUNC1(particles_request_process, RID)
	FUNC1(particles_restart, RID)
	FUNC6(particles_emit, RID, const Transform3D &, const Vector3 &, const Color &, const Color &, uint32_t)
	FUNC2(particles_set_subemitter, RID, RID)
	FUNC2(particles_set_collision_base_size, RID, float)

	FUNC2(particles_set_transform_align, RID, RS::ParticlesTransformAlign)

	FUNC2(particles_set_draw_order, RID, RS::ParticlesDrawOrder)

	FUNC2(particles_set_draw_passes, RID, int)
	FUNC3(particles_set_draw_pass_mesh, RID, int, RID)

	FUNC1R(AABB, particles_get_current_aabb, RID)
	FUNC2(particles_set_emission_transform, RID, const Transform3D &)
	FUNC2(particles_set_emitter_velocity, RID, const Vector3 &)
	FUNC2(particles_set_interp_to_end, RID, float)

	/* PARTICLES COLLISION */

	FUNCRIDSPLIT(particles_collision)

	FUNC2(particles_collision_set_collision_type, RID, ParticlesCollisionType)
	FUNC2(particles_collision_set_cull_mask, RID, uint32_t)
	FUNC2(particles_collision_set_sphere_radius, RID, real_t)
	FUNC2(particles_collision_set_box_extents, RID, const Vector3 &)
	FUNC2(particles_collision_set_attractor_strength, RID, real_t)
	FUNC2(particles_collision_set_attractor_directionality, RID, real_t)
	FUNC2(particles_collision_set_attractor_attenuation, RID, real_t)
	FUNC2(particles_collision_set_field_texture, RID, RID)
	FUNC1(particles_collision_height_field_update, RID)
	FUNC2(particles_collision_set_height_field_resolution, RID, ParticlesCollisionHeightfieldResolution)

	/* FOG VOLUME */

#undef ServerName
#undef server_name

#define ServerName RendererFog
#define server_name RSG::fog

	FUNCRIDSPLIT(fog_volume)

	FUNC2(fog_volume_set_shape, RID, FogVolumeShape)
	FUNC2(fog_volume_set_size, RID, const Vector3 &)
	FUNC2(fog_volume_set_material, RID, RID)

	/* VISIBILITY_NOTIFIER */

#undef ServerName
#undef server_name

#define ServerName RendererUtilities
#define server_name RSG::utilities

	FUNCRIDSPLIT(visibility_notifier)
	FUNC2(visibility_notifier_set_aabb, RID, const AABB &)
	FUNC3(visibility_notifier_set_callbacks, RID, const Callable &, const Callable &)

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RenderingMethod
#define server_name RSG::scene

	/* CAMERA API */

	FUNCRIDSPLIT(camera)
	FUNC4(camera_set_perspective, RID, float, float, float)
	FUNC4(camera_set_orthogonal, RID, float, float, float)
	FUNC5(camera_set_frustum, RID, float, Vector2, float, float)
	FUNC2(camera_set_transform, RID, const Transform3D &)
	FUNC2(camera_set_cull_mask, RID, uint32_t)
	FUNC2(camera_set_environment, RID, RID)
	FUNC2(camera_set_camera_attributes, RID, RID)
	FUNC2(camera_set_compositor, RID, RID)
	FUNC2(camera_set_use_vertical_aspect, RID, bool)

	/* OCCLUDER */
	FUNCRIDSPLIT(occluder)
	FUNC3(occluder_set_mesh, RID, const PackedVector3Array &, const PackedInt32Array &)

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RendererViewport
#define server_name RSG::viewport

	/* VIEWPORT TARGET API */

	FUNCRIDSPLIT(viewport)

	FUNC2(viewport_set_use_xr, RID, bool)
	FUNC3(viewport_set_size, RID, int, int)

	FUNC2(viewport_set_active, RID, bool)
	FUNC2(viewport_set_parent_viewport, RID, RID)

	FUNC2(viewport_set_clear_mode, RID, ViewportClearMode)

	FUNC3(viewport_attach_to_screen, RID, const Rect2 &, int)
	FUNC2(viewport_set_render_direct_to_screen, RID, bool)

	FUNC2(viewport_set_scaling_3d_mode, RID, ViewportScaling3DMode)
	FUNC2(viewport_set_scaling_3d_scale, RID, float)
	FUNC2(viewport_set_fsr_sharpness, RID, float)
	FUNC2(viewport_set_texture_mipmap_bias, RID, float)
	FUNC2(viewport_set_anisotropic_filtering_level, RID, ViewportAnisotropicFiltering)

	FUNC2(viewport_set_update_mode, RID, ViewportUpdateMode)
	FUNC1RC(ViewportUpdateMode, viewport_get_update_mode, RID)

	FUNC1RC(RID, viewport_get_render_target, RID)
	FUNC1RC(RID, viewport_get_texture, RID)

	FUNC2(viewport_set_disable_2d, RID, bool)
	FUNC2(viewport_set_environment_mode, RID, ViewportEnvironmentMode)
	FUNC2(viewport_set_disable_3d, RID, bool)

	FUNC2(viewport_set_canvas_cull_mask, RID, uint32_t)

	FUNC2(viewport_attach_camera, RID, RID)
	FUNC2(viewport_set_scenario, RID, RID)
	FUNC2(viewport_attach_canvas, RID, RID)

	FUNC2(viewport_remove_canvas, RID, RID)
	FUNC3(viewport_set_canvas_transform, RID, RID, const Transform2D &)
	FUNC2(viewport_set_transparent_background, RID, bool)
	FUNC2(viewport_set_use_hdr_2d, RID, bool)
	FUNC1RC(bool, viewport_is_using_hdr_2d, RID)
	FUNC2(viewport_set_snap_2d_transforms_to_pixel, RID, bool)
	FUNC2(viewport_set_snap_2d_vertices_to_pixel, RID, bool)

	FUNC2(viewport_set_default_canvas_item_texture_filter, RID, CanvasItemTextureFilter)
	FUNC2(viewport_set_default_canvas_item_texture_repeat, RID, CanvasItemTextureRepeat)

	FUNC2(viewport_set_global_canvas_transform, RID, const Transform2D &)
	FUNC4(viewport_set_canvas_stacking, RID, RID, int, int)
	FUNC3(viewport_set_positional_shadow_atlas_size, RID, int, bool)
	FUNC3(viewport_set_sdf_oversize_and_scale, RID, ViewportSDFOversize, ViewportSDFScale)
	FUNC3(viewport_set_positional_shadow_atlas_quadrant_subdivision, RID, int, int)
	FUNC2(viewport_set_msaa_2d, RID, ViewportMSAA)
	FUNC2(viewport_set_msaa_3d, RID, ViewportMSAA)
	FUNC2(viewport_set_screen_space_aa, RID, ViewportScreenSpaceAA)
	FUNC2(viewport_set_use_taa, RID, bool)
	FUNC2(viewport_set_use_debanding, RID, bool)
	FUNC2(viewport_set_force_motion_vectors, RID, bool)
	FUNC2(viewport_set_use_occlusion_culling, RID, bool)
	FUNC1(viewport_set_occlusion_rays_per_thread, int)
	FUNC1(viewport_set_occlusion_culling_build_quality, ViewportOcclusionCullingBuildQuality)
	FUNC2(viewport_set_mesh_lod_threshold, RID, float)

	FUNC3R(int, viewport_get_render_info, RID, ViewportRenderInfoType, ViewportRenderInfo)
	FUNC2(viewport_set_debug_draw, RID, ViewportDebugDraw)

	FUNC2(viewport_set_measure_render_time, RID, bool)
	FUNC1RC(double, viewport_get_measured_render_time_cpu, RID)
	FUNC1RC(double, viewport_get_measured_render_time_gpu, RID)
	FUNC1RC(RID, viewport_find_from_screen_attachment, DisplayServer::WindowID)

	FUNC2(call_set_vsync_mode, DisplayServer::VSyncMode, DisplayServer::WindowID)

	FUNC2(viewport_set_vrs_mode, RID, ViewportVRSMode)
	FUNC2(viewport_set_vrs_update_mode, RID, ViewportVRSUpdateMode)
	FUNC2(viewport_set_vrs_texture, RID, RID)

	/* COMPOSITOR EFFECT */

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RenderingMethod
#define server_name RSG::scene

	FUNCRIDSPLIT(compositor_effect)
	FUNC2(compositor_effect_set_enabled, RID, bool)
	FUNC3(compositor_effect_set_callback, RID, CompositorEffectCallbackType, const Callable &)
	FUNC3(compositor_effect_set_flag, RID, CompositorEffectFlags, bool)

	/* COMPOSITOR */

	FUNC2(compositor_set_compositor_effects, RID, const TypedArray<RID> &)

	FUNCRIDSPLIT(compositor)

	/* ENVIRONMENT API */

	FUNC1(voxel_gi_set_quality, VoxelGIQuality)

	/* SKY API */

	FUNCRIDSPLIT(sky)
	FUNC2(sky_set_radiance_size, RID, int)
	FUNC2(sky_set_mode, RID, SkyMode)
	FUNC2(sky_set_material, RID, RID)
	FUNC4R(Ref<Image>, sky_bake_panorama, RID, float, bool, const Size2i &)

	/* ENVIRONMENT */

	FUNCRIDSPLIT(environment)

	FUNC2(environment_set_background, RID, EnvironmentBG)
	FUNC2(environment_set_sky, RID, RID)
	FUNC2(environment_set_sky_custom_fov, RID, float)
	FUNC2(environment_set_sky_orientation, RID, const Basis &)
	FUNC2(environment_set_bg_color, RID, const Color &)
	FUNC3(environment_set_bg_energy, RID, float, float)
	FUNC2(environment_set_canvas_max_layer, RID, int)
	FUNC6(environment_set_ambient_light, RID, const Color &, EnvironmentAmbientSource, float, float, EnvironmentReflectionSource)

	FUNC2(environment_set_camera_feed_id, RID, int)

	FUNC6(environment_set_ssr, RID, bool, int, float, float, float)
	FUNC1(environment_set_ssr_roughness_quality, EnvironmentSSRRoughnessQuality)

	FUNC10(environment_set_ssao, RID, bool, float, float, float, float, float, float, float, float)
	FUNC6(environment_set_ssao_quality, EnvironmentSSAOQuality, bool, float, int, float, float)

	FUNC6(environment_set_ssil, RID, bool, float, float, float, float)
	FUNC6(environment_set_ssil_quality, EnvironmentSSILQuality, bool, float, int, float, float)

	FUNC13(environment_set_glow, RID, bool, Vector<float>, float, float, float, float, EnvironmentGlowBlendMode, float, float, float, float, RID)
	FUNC1(environment_glow_set_use_bicubic_upscale, bool)

	FUNC4(environment_set_tonemap, RID, EnvironmentToneMapper, float, float)

	FUNC7(environment_set_adjustment, RID, bool, float, float, float, bool, RID)

	FUNC11(environment_set_fog, RID, bool, const Color &, float, float, float, float, float, float, float, EnvironmentFogMode)

	FUNC4(environment_set_fog_depth, RID, float, float, float)
	FUNC14(environment_set_volumetric_fog, RID, bool, float, const Color &, const Color &, float, float, float, float, float, bool, float, float, float)

	FUNC2(environment_set_volumetric_fog_volume_size, int, int)
	FUNC1(environment_set_volumetric_fog_filter_active, bool)

	FUNC11(environment_set_sdfgi, RID, bool, int, float, EnvironmentSDFGIYScale, bool, float, bool, float, float, float)
	FUNC1(environment_set_sdfgi_ray_count, EnvironmentSDFGIRayCount)
	FUNC1(environment_set_sdfgi_frames_to_converge, EnvironmentSDFGIFramesToConverge)
	FUNC1(environment_set_sdfgi_frames_to_update_light, EnvironmentSDFGIFramesToUpdateLight)

	FUNC3R(Ref<Image>, environment_bake_panorama, RID, bool, const Size2i &)

	FUNC3(screen_space_roughness_limiter_set_active, bool, float, float)
	FUNC1(sub_surface_scattering_set_quality, SubSurfaceScatteringQuality)
	FUNC2(sub_surface_scattering_set_scale, float, float)

	FUNC1(positional_soft_shadow_filter_set_quality, ShadowQuality);
	FUNC1(directional_soft_shadow_filter_set_quality, ShadowQuality);
	FUNC1(decals_set_filter, RS::DecalFilter);
	FUNC1(light_projectors_set_filter, RS::LightProjectorFilter);
	FUNC1(lightmaps_set_bicubic_filter, bool);

	/* CAMERA ATTRIBUTES */

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RendererCameraAttributes
#define server_name RSG::camera_attributes

	FUNCRIDSPLIT(camera_attributes)

	FUNC2(camera_attributes_set_dof_blur_quality, DOFBlurQuality, bool)
	FUNC1(camera_attributes_set_dof_blur_bokeh_shape, DOFBokehShape)

	FUNC8(camera_attributes_set_dof_blur, RID, bool, float, float, bool, float, float, float)
	FUNC3(camera_attributes_set_exposure, RID, float, float)
	FUNC6(camera_attributes_set_auto_exposure, RID, bool, float, float, float, float)

	/* SCENARIO API */

#undef server_name
#undef ServerName

#define ServerName RenderingMethod
#define server_name RSG::scene

	FUNCRIDSPLIT(scenario)

	FUNC2(scenario_set_environment, RID, RID)
	FUNC2(scenario_set_camera_attributes, RID, RID)
	FUNC2(scenario_set_fallback_environment, RID, RID)
	FUNC2(scenario_set_compositor, RID, RID)

	/* INSTANCING API */
	FUNCRIDSPLIT(instance)

	FUNC2(instance_set_base, RID, RID)
	FUNC2(instance_set_scenario, RID, RID)
	FUNC2(instance_set_layer_mask, RID, uint32_t)
	FUNC3(instance_set_pivot_data, RID, float, bool)
	FUNC2(instance_set_transform, RID, const Transform3D &)
	FUNC2(instance_set_interpolated, RID, bool)
	FUNC1(instance_reset_physics_interpolation, RID)
	FUNC2(instance_attach_object_instance_id, RID, ObjectID)
	FUNC3(instance_set_blend_shape_weight, RID, int, float)
	FUNC3(instance_set_surface_override_material, RID, int, RID)
	FUNC2(instance_set_visible, RID, bool)

	FUNC2(instance_set_custom_aabb, RID, AABB)

	FUNC2(instance_attach_skeleton, RID, RID)

	FUNC2(instance_set_extra_visibility_margin, RID, real_t)
	FUNC2(instance_set_visibility_parent, RID, RID)

	FUNC2(instance_set_ignore_culling, RID, bool)

	// don't use these in a game!
	FUNC2RC(Vector<ObjectID>, instances_cull_aabb, const AABB &, RID)
	FUNC3RC(Vector<ObjectID>, instances_cull_ray, const Vector3 &, const Vector3 &, RID)
	FUNC2RC(Vector<ObjectID>, instances_cull_convex, const Vector<Plane> &, RID)

	FUNC3(instance_geometry_set_flag, RID, InstanceFlags, bool)
	FUNC2(instance_geometry_set_cast_shadows_setting, RID, ShadowCastingSetting)
	FUNC2(instance_geometry_set_material_override, RID, RID)
	FUNC2(instance_geometry_set_material_overlay, RID, RID)

	FUNC6(instance_geometry_set_visibility_range, RID, float, float, float, float, VisibilityRangeFadeMode)
	FUNC4(instance_geometry_set_lightmap, RID, RID, const Rect2 &, int)
	FUNC2(instance_geometry_set_lod_bias, RID, float)
	FUNC2(instance_geometry_set_transparency, RID, float)
	FUNC3(instance_geometry_set_shader_parameter, RID, const StringName &, const Variant &)
	FUNC2RC(Variant, instance_geometry_get_shader_parameter, RID, const StringName &)
	FUNC2RC(Variant, instance_geometry_get_shader_parameter_default_value, RID, const StringName &)
	FUNC2C(instance_geometry_get_shader_parameter_list, RID, List<PropertyInfo> *)

	FUNC3R(TypedArray<Image>, bake_render_uv2, RID, const TypedArray<RID> &, const Size2i &)

	FUNC1(gi_set_use_half_resolution, bool)

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RendererCanvasCull
#define server_name RSG::canvas

	/* CANVAS (2D) */

	FUNCRIDSPLIT(canvas)
	FUNC3(canvas_set_item_mirroring, RID, RID, const Point2 &)
	FUNC3(canvas_set_item_repeat, RID, const Point2 &, int)
	FUNC2(canvas_set_modulate, RID, const Color &)
	FUNC3(canvas_set_parent, RID, RID, float)
	FUNC1(canvas_set_disable_scale, bool)

	FUNCRIDSPLIT(canvas_texture)
	FUNC3(canvas_texture_set_channel, RID, CanvasTextureChannel, RID)
	FUNC3(canvas_texture_set_shading_parameters, RID, const Color &, float)

	FUNC2(canvas_texture_set_texture_filter, RID, CanvasItemTextureFilter)
	FUNC2(canvas_texture_set_texture_repeat, RID, CanvasItemTextureRepeat)

	FUNCRIDSPLIT(canvas_item)
	FUNC2(canvas_item_set_parent, RID, RID)

	FUNC2(canvas_item_set_default_texture_filter, RID, CanvasItemTextureFilter)
	FUNC2(canvas_item_set_default_texture_repeat, RID, CanvasItemTextureRepeat)

	FUNC2(canvas_item_set_visible, RID, bool)
	FUNC2(canvas_item_set_light_mask, RID, int)

	FUNC2(canvas_item_set_visibility_layer, RID, uint32_t)

	FUNC2(canvas_item_set_update_when_visible, RID, bool)

	FUNC2(canvas_item_set_transform, RID, const Transform2D &)
	FUNC2(canvas_item_set_clip, RID, bool)
	FUNC2(canvas_item_set_distance_field_mode, RID, bool)
	FUNC3(canvas_item_set_custom_rect, RID, bool, const Rect2 &)
	FUNC2(canvas_item_set_modulate, RID, const Color &)
	FUNC2(canvas_item_set_self_modulate, RID, const Color &)

	FUNC2(canvas_item_set_draw_behind_parent, RID, bool)

	FUNC6(canvas_item_add_line, RID, const Point2 &, const Point2 &, const Color &, float, bool)
	FUNC5(canvas_item_add_polyline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	FUNC5(canvas_item_add_multiline, RID, const Vector<Point2> &, const Vector<Color> &, float, bool)
	FUNC4(canvas_item_add_rect, RID, const Rect2 &, const Color &, bool)
	FUNC5(canvas_item_add_circle, RID, const Point2 &, float, const Color &, bool)
	FUNC6(canvas_item_add_texture_rect, RID, const Rect2 &, RID, bool, const Color &, bool)
	FUNC7(canvas_item_add_texture_rect_region, RID, const Rect2 &, RID, const Rect2 &, const Color &, bool, bool)
	FUNC8(canvas_item_add_msdf_texture_rect_region, RID, const Rect2 &, RID, const Rect2 &, const Color &, int, float, float)
	FUNC5(canvas_item_add_lcd_texture_rect_region, RID, const Rect2 &, RID, const Rect2 &, const Color &)
	FUNC10(canvas_item_add_nine_patch, RID, const Rect2 &, const Rect2 &, RID, const Vector2 &, const Vector2 &, NinePatchAxisMode, NinePatchAxisMode, bool, const Color &)
	FUNC5(canvas_item_add_primitive, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID)
	FUNC5(canvas_item_add_polygon, RID, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, RID)
	FUNC9(canvas_item_add_triangle_array, RID, const Vector<int> &, const Vector<Point2> &, const Vector<Color> &, const Vector<Point2> &, const Vector<int> &, const Vector<float> &, RID, int)
	FUNC5(canvas_item_add_mesh, RID, const RID &, const Transform2D &, const Color &, RID)
	FUNC3(canvas_item_add_multimesh, RID, RID, RID)
	FUNC3(canvas_item_add_particles, RID, RID, RID)
	FUNC2(canvas_item_add_set_transform, RID, const Transform2D &)
	FUNC2(canvas_item_add_clip_ignore, RID, bool)
	FUNC5(canvas_item_add_animation_slice, RID, double, double, double, double)

	FUNC2(canvas_item_set_sort_children_by_y, RID, bool)
	FUNC2(canvas_item_set_z_index, RID, int)
	FUNC2(canvas_item_set_z_as_relative_to_parent, RID, bool)
	FUNC3(canvas_item_set_copy_to_backbuffer, RID, bool, const Rect2 &)
	FUNC2(canvas_item_attach_skeleton, RID, RID)

	FUNC1(canvas_item_clear, RID)
	FUNC2(canvas_item_set_draw_index, RID, int)

	FUNC2(canvas_item_set_material, RID, RID)

	FUNC3(canvas_item_set_instance_shader_parameter, RID, const StringName &, const Variant &)
	FUNC2RC(Variant, canvas_item_get_instance_shader_parameter, RID, const StringName &)
	FUNC2RC(Variant, canvas_item_get_instance_shader_parameter_default_value, RID, const StringName &)
	FUNC2C(canvas_item_get_instance_shader_parameter_list, RID, List<PropertyInfo> *)

	FUNC2(canvas_item_set_use_parent_material, RID, bool)

	FUNC5(canvas_item_set_visibility_notifier, RID, bool, const Rect2 &, const Callable &, const Callable &)

	FUNC6(canvas_item_set_canvas_group_mode, RID, CanvasGroupMode, float, bool, float, bool)

	FUNC1(canvas_item_set_debug_redraw, bool)
	FUNC0RC(bool, canvas_item_get_debug_redraw)

	FUNC2(canvas_item_set_interpolated, RID, bool)
	FUNC1(canvas_item_reset_physics_interpolation, RID)
	FUNC2(canvas_item_transform_physics_interpolation, RID, const Transform2D &)

	FUNCRIDSPLIT(canvas_light)

	FUNC2(canvas_light_set_mode, RID, CanvasLightMode)

	FUNC2(canvas_light_attach_to_canvas, RID, RID)
	FUNC2(canvas_light_set_enabled, RID, bool)
	FUNC2(canvas_light_set_texture_scale, RID, float)
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
	FUNC2(canvas_light_set_directional_distance, RID, float)

	FUNC2(canvas_light_set_blend_mode, RID, CanvasLightBlendMode)

	FUNC2(canvas_light_set_shadow_enabled, RID, bool)
	FUNC2(canvas_light_set_shadow_filter, RID, CanvasLightShadowFilter)
	FUNC2(canvas_light_set_shadow_color, RID, const Color &)
	FUNC2(canvas_light_set_shadow_smooth, RID, float)

	FUNC2(canvas_light_set_interpolated, RID, bool)
	FUNC1(canvas_light_reset_physics_interpolation, RID)
	FUNC2(canvas_light_transform_physics_interpolation, RID, const Transform2D &)

	FUNCRIDSPLIT(canvas_light_occluder)
	FUNC2(canvas_light_occluder_attach_to_canvas, RID, RID)
	FUNC2(canvas_light_occluder_set_enabled, RID, bool)
	FUNC2(canvas_light_occluder_set_polygon, RID, RID)
	FUNC2(canvas_light_occluder_set_as_sdf_collision, RID, bool)
	FUNC2(canvas_light_occluder_set_transform, RID, const Transform2D &)
	FUNC2(canvas_light_occluder_set_light_mask, RID, int)

	FUNC2(canvas_light_occluder_set_interpolated, RID, bool)
	FUNC1(canvas_light_occluder_reset_physics_interpolation, RID)
	FUNC2(canvas_light_occluder_transform_physics_interpolation, RID, const Transform2D &)

	FUNCRIDSPLIT(canvas_occluder_polygon)
	FUNC3(canvas_occluder_polygon_set_shape, RID, const Vector<Vector2> &, bool)

	FUNC2(canvas_occluder_polygon_set_cull_mode, RID, CanvasOccluderPolygonCullMode)

	FUNC1(canvas_set_shadow_texture_size, int)

	FUNC1R(Rect2, _debug_canvas_item_get_rect, RID)

	/* GLOBAL SHADER UNIFORMS */

#undef server_name
#undef ServerName
//from now on, calls forwarded to this singleton
#define ServerName RendererMaterialStorage
#define server_name RSG::material_storage

	FUNC3(global_shader_parameter_add, const StringName &, GlobalShaderParameterType, const Variant &)
	FUNC1(global_shader_parameter_remove, const StringName &)
	FUNC0RC(Vector<StringName>, global_shader_parameter_get_list)
	FUNC2(global_shader_parameter_set, const StringName &, const Variant &)
	FUNC2(global_shader_parameter_set_override, const StringName &, const Variant &)
	FUNC1RC(GlobalShaderParameterType, global_shader_parameter_get_type, const StringName &)
	FUNC1RC(Variant, global_shader_parameter_get, const StringName &)

	FUNC1(global_shader_parameters_load_settings, bool)
	FUNC0(global_shader_parameters_clear)

	/* COMPOSITOR */

#undef server_name
#undef ServerName
#define ServerName RendererCompositor
#define server_name RSG::rasterizer

	FUNC4S(set_boot_image, const Ref<Image> &, const Color &, bool, bool)

	/* STATUS INFORMATION */

#undef server_name
#undef ServerName

	/* UTILITIES */

#define ServerName RendererUtilities
#define server_name RSG::utilities
	FUNC0RC(String, get_video_adapter_name)
	FUNC0RC(String, get_video_adapter_vendor)
	FUNC0RC(String, get_video_adapter_api_version)
#undef server_name
#undef ServerName
#undef WRITE_ACTION
#undef SYNC_DEBUG
#ifdef DEBUG_ENABLED
#undef MAIN_THREAD_SYNC_WARN
#endif

	virtual uint64_t get_rendering_info(RenderingInfo p_info) override;
	virtual RenderingDevice::DeviceType get_video_adapter_type() const override;

	virtual void set_frame_profiling_enabled(bool p_enable) override;
	virtual Vector<FrameProfileArea> get_frame_profile() override;
	virtual uint64_t get_frame_profile_frame() override;

	virtual RID get_test_cube() override;

	/* FREE */

	virtual void free(RID p_rid) override {
		if (Thread::get_caller_id() == server_thread) {
			command_queue.flush_if_pending();
			_free(p_rid);
		} else {
			command_queue.push(this, &RenderingServerDefault::_free, p_rid);
		}
	}

	/* INTERPOLATION */

	virtual void set_physics_interpolation_enabled(bool p_enabled) override;

	/* EVENT QUEUING */

	virtual void request_frame_drawn_callback(const Callable &p_callable) override;

	virtual void draw(bool p_present, double frame_step) override;
	virtual void sync() override;
	virtual bool has_changed() const override;
	virtual void init() override;
	virtual void finish() override;
	virtual void tick() override;
	virtual void pre_draw(bool p_will_draw) override;

	virtual bool is_on_render_thread() override {
		return Thread::get_caller_id() == server_thread;
	}

	virtual void call_on_render_thread(const Callable &p_callable) override {
		if (Thread::get_caller_id() == server_thread) {
			command_queue.flush_if_pending();
			p_callable.call();
		} else {
			command_queue.push(this, &RenderingServerDefault::_call_on_render_thread, p_callable);
		}
	}

	/* TESTING */

	virtual double get_frame_setup_time_cpu() const override;

	virtual Color get_default_clear_color() override;
	virtual void set_default_clear_color(const Color &p_color) override;

#ifndef DISABLE_DEPRECATED
	virtual bool has_feature(Features p_feature) const override;
#endif

	virtual bool has_os_feature(const String &p_feature) const override;
	virtual void set_debug_generate_wireframes(bool p_generate) override;

	virtual bool is_low_end() const override;

	virtual void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;

	virtual void set_print_gpu_profile(bool p_enable) override;

	virtual Size2i get_maximum_viewport_size() const override;

	RenderingServerDefault(bool p_create_thread = false);
	~RenderingServerDefault();
};

#endif // RENDERING_SERVER_DEFAULT_H
