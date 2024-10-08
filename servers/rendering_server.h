/**************************************************************************/
/*  rendering_server.h                                                    */
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

#ifndef RENDERING_SERVER_H
#define RENDERING_SERVER_H

#include "core/io/image.h"
#include "core/math/geometry_3d.h"
#include "core/math/transform_2d.h"
#include "core/object/class_db.h"
#include "core/templates/rid.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"
#include "servers/display_server.h"
#include "servers/rendering/rendering_device.h"

// Helper macros for code outside of the rendering server, but that is
// called by the rendering server.
#ifdef DEBUG_ENABLED
#define ERR_NOT_ON_RENDER_THREAD                                          \
	RenderingServer *rendering_server = RenderingServer::get_singleton(); \
	ERR_FAIL_NULL(rendering_server);                                      \
	ERR_FAIL_COND(!rendering_server->is_on_render_thread());
#define ERR_NOT_ON_RENDER_THREAD_V(m_ret)                                 \
	RenderingServer *rendering_server = RenderingServer::get_singleton(); \
	ERR_FAIL_NULL_V(rendering_server, m_ret);                             \
	ERR_FAIL_COND_V(!rendering_server->is_on_render_thread(), m_ret);
#else
#define ERR_NOT_ON_RENDER_THREAD
#define ERR_NOT_ON_RENDER_THREAD_V(m_ret)
#endif

template <typename T>
class TypedArray;

class RenderingServer : public Object {
	GDCLASS(RenderingServer, Object);

	static RenderingServer *singleton;

	int mm_policy = 0;
	bool render_loop_enabled = true;

	Array _get_array_from_surface(uint64_t p_format, Vector<uint8_t> p_vertex_data, Vector<uint8_t> p_attrib_data, Vector<uint8_t> p_skin_data, int p_vertex_len, Vector<uint8_t> p_index_data, int p_index_len, const AABB &p_aabb, const Vector4 &p_uv_scale) const;

	const Vector2 SMALL_VEC2 = Vector2(CMP_EPSILON, CMP_EPSILON);
	const Vector3 SMALL_VEC3 = Vector3(CMP_EPSILON, CMP_EPSILON, CMP_EPSILON);

	virtual TypedArray<StringName> _global_shader_parameter_get_list() const;

protected:
	RID _make_test_cube();
	void _free_internal_rids();
	RID test_texture;
	RID white_texture;
	RID test_material;

	Error _surface_set_data(Array p_arrays, uint64_t p_format, uint32_t *p_offsets, uint32_t p_vertex_stride, uint32_t p_normal_stride, uint32_t p_attrib_stride, uint32_t p_skin_stride, Vector<uint8_t> &r_vertex_array, Vector<uint8_t> &r_attrib_array, Vector<uint8_t> &r_skin_array, int p_vertex_array_len, Vector<uint8_t> &r_index_array, int p_index_array_len, AABB &r_aabb, Vector<AABB> &r_bone_aabb, Vector4 &r_uv_scale);

	static RenderingServer *(*create_func)();
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _environment_set_fog_bind_compat_84792(RID p_env, bool p_enable, const Color &p_light_color, float p_light_energy, float p_sun_scatter, float p_density, float p_height, float p_height_density, float p_aerial_perspective, float p_sky_affect);
	void _canvas_item_add_multiline_bind_compat_84523(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width = -1.0);
	void _canvas_item_add_rect_bind_compat_84523(RID p_item, const Rect2 &p_rect, const Color &p_color);
	void _canvas_item_add_circle_bind_compat_84523(RID p_item, const Point2 &p_pos, float p_radius, const Color &p_color);

	static void _bind_compatibility_methods();
#endif

public:
	static RenderingServer *get_singleton();
	static RenderingServer *create();

	enum {
		NO_INDEX_ARRAY = -1,
		ARRAY_WEIGHTS_SIZE = 4,
		CANVAS_ITEM_Z_MIN = -4096,
		CANVAS_ITEM_Z_MAX = 4096,
		MAX_GLOW_LEVELS = 7,
		MAX_CURSORS = 8,
		MAX_2D_DIRECTIONAL_LIGHTS = 8,
		MAX_MESH_SURFACES = 256
	};

	/* TEXTURE API */

	enum TextureType {
		TEXTURE_TYPE_2D,
		TEXTURE_TYPE_LAYERED,
		TEXTURE_TYPE_3D,
	};

	enum TextureLayeredType {
		TEXTURE_LAYERED_2D_ARRAY,
		TEXTURE_LAYERED_CUBEMAP,
		TEXTURE_LAYERED_CUBEMAP_ARRAY,
	};

	enum CubeMapLayer {
		CUBEMAP_LAYER_LEFT,
		CUBEMAP_LAYER_RIGHT,
		CUBEMAP_LAYER_BOTTOM,
		CUBEMAP_LAYER_TOP,
		CUBEMAP_LAYER_FRONT,
		CUBEMAP_LAYER_BACK
	};

	virtual RID texture_2d_create(const Ref<Image> &p_image) = 0;
	virtual RID texture_2d_layered_create(const Vector<Ref<Image>> &p_layers, TextureLayeredType p_layered_type) = 0;
	virtual RID texture_3d_create(Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) = 0; //all slices, then all the mipmaps, must be coherent
	virtual RID texture_external_create(int p_width, int p_height, uint64_t p_external_buffer = 0) = 0;
	virtual RID texture_proxy_create(RID p_base) = 0;

	virtual RID texture_create_from_native_handle(TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, TextureLayeredType p_layered_type = TEXTURE_LAYERED_2D_ARRAY) = 0;

	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) = 0;
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) = 0;
	virtual void texture_external_update(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer = 0) = 0;
	virtual void texture_proxy_update(RID p_texture, RID p_proxy_to) = 0;

	// These two APIs can be used together or in combination with the others.
	virtual RID texture_2d_placeholder_create() = 0;
	virtual RID texture_2d_layered_placeholder_create(TextureLayeredType p_layered_type) = 0;
	virtual RID texture_3d_placeholder_create() = 0;

	virtual Ref<Image> texture_2d_get(RID p_texture) const = 0;
	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const = 0;
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const = 0;

	virtual void texture_replace(RID p_texture, RID p_by_texture) = 0;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) = 0;

	virtual void texture_set_path(RID p_texture, const String &p_path) = 0;
	virtual String texture_get_path(RID p_texture) const = 0;

	virtual Image::Format texture_get_format(RID p_texture) const = 0;

	typedef void (*TextureDetectCallback)(void *);

	virtual void texture_set_detect_3d_callback(RID p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;
	virtual void texture_set_detect_normal_callback(RID p_texture, TextureDetectCallback p_callback, void *p_userdata) = 0;

	enum TextureDetectRoughnessChannel {
		TEXTURE_DETECT_ROUGHNESS_R,
		TEXTURE_DETECT_ROUGHNESS_G,
		TEXTURE_DETECT_ROUGHNESS_B,
		TEXTURE_DETECT_ROUGHNESS_A,
		TEXTURE_DETECT_ROUGHNESS_GRAY,
	};

	typedef void (*TextureDetectRoughnessCallback)(void *, const String &, TextureDetectRoughnessChannel);
	virtual void texture_set_detect_roughness_callback(RID p_texture, TextureDetectRoughnessCallback p_callback, void *p_userdata) = 0;

	struct TextureInfo {
		RID texture;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		Image::Format format;
		int64_t bytes;
		String path;
	};

	virtual void texture_debug_usage(List<TextureInfo> *r_info) = 0;
	Array _texture_debug_usage_bind();

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) = 0;

	virtual RID texture_rd_create(const RID &p_rd_texture, const RenderingServer::TextureLayeredType p_layer_type = RenderingServer::TEXTURE_LAYERED_2D_ARRAY) = 0;
	virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const = 0;
	virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const = 0;

	/* PIPELINES API */

	enum PipelineSource {
		PIPELINE_SOURCE_CANVAS,
		PIPELINE_SOURCE_MESH,
		PIPELINE_SOURCE_SURFACE,
		PIPELINE_SOURCE_DRAW,
		PIPELINE_SOURCE_SPECIALIZATION,
		PIPELINE_SOURCE_MAX
	};

	/* SHADER API */

	enum ShaderMode {
		SHADER_SPATIAL,
		SHADER_CANVAS_ITEM,
		SHADER_PARTICLES,
		SHADER_SKY,
		SHADER_FOG,
		SHADER_MAX
	};

	virtual RID shader_create() = 0;
	virtual RID shader_create_from_code(const String &p_code, const String &p_path_hint = String()) = 0;

	virtual void shader_set_code(RID p_shader, const String &p_code) = 0;
	virtual void shader_set_path_hint(RID p_shader, const String &p_path) = 0;
	virtual String shader_get_code(RID p_shader) const = 0;
	virtual void get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const = 0;
	virtual Variant shader_get_parameter_default(RID p_shader, const StringName &p_param) const = 0;

	virtual void shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index = 0) = 0;
	virtual RID shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index = 0) const = 0;

	struct ShaderNativeSourceCode {
		struct Version {
			struct Stage {
				String name;
				String code;
			};
			Vector<Stage> stages;
		};
		Vector<Version> versions;
	};

	virtual ShaderNativeSourceCode shader_get_native_source_code(RID p_shader) const = 0;

	/* COMMON MATERIAL API */

	enum {
		MATERIAL_RENDER_PRIORITY_MIN = -128,
		MATERIAL_RENDER_PRIORITY_MAX = 127,
	};

	virtual RID material_create() = 0;
	virtual RID material_create_from_shader(RID p_next_pass, int p_render_priority, RID p_shader) = 0;

	virtual void material_set_shader(RID p_shader_material, RID p_shader) = 0;

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) = 0;
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const = 0;

	virtual void material_set_render_priority(RID p_material, int priority) = 0;

	virtual void material_set_next_pass(RID p_material, RID p_next_material) = 0;

	/* MESH API */

	enum ArrayType {
		ARRAY_VERTEX = 0, // RG32F (2D), RGB32F, RGBA16 (compressed)
		ARRAY_NORMAL = 1, // RG16
		ARRAY_TANGENT = 2, // BA16 (with normal) or A16 (with vertex, when compressed)
		ARRAY_COLOR = 3, // RGBA8
		ARRAY_TEX_UV = 4, // RG32F or RG16
		ARRAY_TEX_UV2 = 5, // RG32F or RG16
		ARRAY_CUSTOM0 = 6, // Depends on ArrayCustomFormat.
		ARRAY_CUSTOM1 = 7,
		ARRAY_CUSTOM2 = 8,
		ARRAY_CUSTOM3 = 9,
		ARRAY_BONES = 10, // RGBA16UI (x2 if 8 weights)
		ARRAY_WEIGHTS = 11, // RGBA16UNORM (x2 if 8 weights)
		ARRAY_INDEX = 12, // 16 or 32 bits depending on length > 0xFFFF.
		ARRAY_MAX = 13
	};

	enum {
		ARRAY_CUSTOM_COUNT = ARRAY_BONES - ARRAY_CUSTOM0
	};

	enum ArrayCustomFormat {
		ARRAY_CUSTOM_RGBA8_UNORM,
		ARRAY_CUSTOM_RGBA8_SNORM,
		ARRAY_CUSTOM_RG_HALF,
		ARRAY_CUSTOM_RGBA_HALF,
		ARRAY_CUSTOM_R_FLOAT,
		ARRAY_CUSTOM_RG_FLOAT,
		ARRAY_CUSTOM_RGB_FLOAT,
		ARRAY_CUSTOM_RGBA_FLOAT,
		ARRAY_CUSTOM_MAX
	};

	enum ArrayFormat : uint64_t {
		/* ARRAY FORMAT FLAGS */
		ARRAY_FORMAT_VERTEX = 1 << ARRAY_VERTEX,
		ARRAY_FORMAT_NORMAL = 1 << ARRAY_NORMAL,
		ARRAY_FORMAT_TANGENT = 1 << ARRAY_TANGENT,
		ARRAY_FORMAT_COLOR = 1 << ARRAY_COLOR,
		ARRAY_FORMAT_TEX_UV = 1 << ARRAY_TEX_UV,
		ARRAY_FORMAT_TEX_UV2 = 1 << ARRAY_TEX_UV2,
		ARRAY_FORMAT_CUSTOM0 = 1 << ARRAY_CUSTOM0,
		ARRAY_FORMAT_CUSTOM1 = 1 << ARRAY_CUSTOM1,
		ARRAY_FORMAT_CUSTOM2 = 1 << ARRAY_CUSTOM2,
		ARRAY_FORMAT_CUSTOM3 = 1 << ARRAY_CUSTOM3,
		ARRAY_FORMAT_BONES = 1 << ARRAY_BONES,
		ARRAY_FORMAT_WEIGHTS = 1 << ARRAY_WEIGHTS,
		ARRAY_FORMAT_INDEX = 1 << ARRAY_INDEX,

		ARRAY_FORMAT_BLEND_SHAPE_MASK = ARRAY_FORMAT_VERTEX | ARRAY_FORMAT_NORMAL | ARRAY_FORMAT_TANGENT,

		ARRAY_FORMAT_CUSTOM_BASE = (ARRAY_INDEX + 1),
		ARRAY_FORMAT_CUSTOM_BITS = 3,
		ARRAY_FORMAT_CUSTOM_MASK = 0x7,
		ARRAY_FORMAT_CUSTOM0_SHIFT = (ARRAY_FORMAT_CUSTOM_BASE + 0),
		ARRAY_FORMAT_CUSTOM1_SHIFT = (ARRAY_FORMAT_CUSTOM_BASE + ARRAY_FORMAT_CUSTOM_BITS),
		ARRAY_FORMAT_CUSTOM2_SHIFT = (ARRAY_FORMAT_CUSTOM_BASE + ARRAY_FORMAT_CUSTOM_BITS * 2),
		ARRAY_FORMAT_CUSTOM3_SHIFT = (ARRAY_FORMAT_CUSTOM_BASE + ARRAY_FORMAT_CUSTOM_BITS * 3),

		ARRAY_COMPRESS_FLAGS_BASE = (ARRAY_INDEX + 1 + 12),

		ARRAY_FLAG_USE_2D_VERTICES = 1 << (ARRAY_COMPRESS_FLAGS_BASE + 0),
		ARRAY_FLAG_USE_DYNAMIC_UPDATE = 1 << (ARRAY_COMPRESS_FLAGS_BASE + 1),
		ARRAY_FLAG_USE_8_BONE_WEIGHTS = 1 << (ARRAY_COMPRESS_FLAGS_BASE + 2),

		ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY = 1 << (ARRAY_COMPRESS_FLAGS_BASE + 3),

		ARRAY_FLAG_COMPRESS_ATTRIBUTES = 1 << (ARRAY_COMPRESS_FLAGS_BASE + 4),
		// We leave enough room for up to 5 more compression flags.

		ARRAY_FLAG_FORMAT_VERSION_BASE = ARRAY_COMPRESS_FLAGS_BASE + 10,
		ARRAY_FLAG_FORMAT_VERSION_SHIFT = ARRAY_FLAG_FORMAT_VERSION_BASE,
		// When changes are made to the mesh format, add a new version and use it for the CURRENT_VERSION.
		ARRAY_FLAG_FORMAT_VERSION_1 = 0,
		ARRAY_FLAG_FORMAT_VERSION_2 = 1ULL << ARRAY_FLAG_FORMAT_VERSION_SHIFT,
		ARRAY_FLAG_FORMAT_CURRENT_VERSION = ARRAY_FLAG_FORMAT_VERSION_2,
		ARRAY_FLAG_FORMAT_VERSION_MASK = 0xFF, // 8 bits version
	};

	static_assert(sizeof(ArrayFormat) == 8, "ArrayFormat should be 64 bits long.");

	enum PrimitiveType {
		PRIMITIVE_POINTS,
		PRIMITIVE_LINES,
		PRIMITIVE_LINE_STRIP,
		PRIMITIVE_TRIANGLES,
		PRIMITIVE_TRIANGLE_STRIP,
		PRIMITIVE_MAX,
	};

	struct SurfaceData {
		PrimitiveType primitive = PRIMITIVE_MAX;

		uint64_t format = ARRAY_FLAG_FORMAT_CURRENT_VERSION;
		Vector<uint8_t> vertex_data; // Vertex, Normal, Tangent (change with skinning, blendshape).
		Vector<uint8_t> attribute_data; // Color, UV, UV2, Custom0-3.
		Vector<uint8_t> skin_data; // Bone index, Bone weight.
		uint32_t vertex_count = 0;
		Vector<uint8_t> index_data;
		uint32_t index_count = 0;

		AABB aabb;
		struct LOD {
			float edge_length = 0.0f;
			Vector<uint8_t> index_data;
		};
		Vector<LOD> lods;
		Vector<AABB> bone_aabbs;

		// Transforms used in runtime bone AABBs compute.
		// Since bone AABBs is saved in Mesh space, but bones is in Skeleton space.
		Transform3D mesh_to_skeleton_xform;

		Vector<uint8_t> blend_shape_data;

		Vector4 uv_scale;

		RID material;
	};

	virtual RID mesh_create_from_surfaces(const Vector<SurfaceData> &p_surfaces, int p_blend_shape_count = 0) = 0;
	virtual RID mesh_create() = 0;

	virtual void mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) = 0;

	virtual uint32_t mesh_surface_get_format_offset(BitField<ArrayFormat> p_format, int p_vertex_len, int p_array_index) const;
	virtual uint32_t mesh_surface_get_format_vertex_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const;
	virtual uint32_t mesh_surface_get_format_normal_tangent_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const;
	virtual uint32_t mesh_surface_get_format_attribute_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const;
	virtual uint32_t mesh_surface_get_format_skin_stride(BitField<ArrayFormat> p_format, int p_vertex_len) const;

	/// Returns stride
	virtual void mesh_surface_make_offsets_from_format(uint64_t p_format, int p_vertex_len, int p_index_len, uint32_t *r_offsets, uint32_t &r_vertex_element_size, uint32_t &r_normal_element_size, uint32_t &r_attrib_element_size, uint32_t &r_skin_element_size) const;
	virtual Error mesh_create_surface_data_from_arrays(SurfaceData *r_surface_data, PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes = Array(), const Dictionary &p_lods = Dictionary(), uint64_t p_compress_format = 0);
	Array mesh_create_arrays_from_surface_data(const SurfaceData &p_data) const;
	Array mesh_surface_get_arrays(RID p_mesh, int p_surface) const;
	TypedArray<Array> mesh_surface_get_blend_shape_arrays(RID p_mesh, int p_surface) const;
	Dictionary mesh_surface_get_lods(RID p_mesh, int p_surface) const;

	virtual void mesh_add_surface_from_arrays(RID p_mesh, PrimitiveType p_primitive, const Array &p_arrays, const Array &p_blend_shapes = Array(), const Dictionary &p_lods = Dictionary(), BitField<ArrayFormat> p_compress_format = 0);
	virtual void mesh_add_surface(RID p_mesh, const SurfaceData &p_surface) = 0;

	virtual int mesh_get_blend_shape_count(RID p_mesh) const = 0;

	enum BlendShapeMode {
		BLEND_SHAPE_MODE_NORMALIZED,
		BLEND_SHAPE_MODE_RELATIVE,
	};

	virtual void mesh_set_blend_shape_mode(RID p_mesh, BlendShapeMode p_mode) = 0;
	virtual BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const = 0;

	virtual void mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) = 0;
	virtual void mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) = 0;
	virtual void mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) = 0;

	virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) = 0;
	virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const = 0;

	virtual SurfaceData mesh_get_surface(RID p_mesh, int p_surface) const = 0;

	virtual int mesh_get_surface_count(RID p_mesh) const = 0;

	virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) = 0;
	virtual AABB mesh_get_custom_aabb(RID p_mesh) const = 0;

	virtual void mesh_set_path(RID p_mesh, const String &p_path) = 0;
	virtual String mesh_get_path(RID p_mesh) const = 0;

	virtual void mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) = 0;

	virtual void mesh_clear(RID p_mesh) = 0;

	/* MULTIMESH API */

	virtual RID multimesh_create() = 0;

	enum MultimeshTransformFormat {
		MULTIMESH_TRANSFORM_2D,
		MULTIMESH_TRANSFORM_3D,
	};

	enum MultimeshPhysicsInterpolationQuality {
		MULTIMESH_INTERP_QUALITY_FAST,
		MULTIMESH_INTERP_QUALITY_HIGH,
	};

	virtual void multimesh_allocate_data(RID p_multimesh, int p_instances, MultimeshTransformFormat p_transform_format, bool p_use_colors = false, bool p_use_custom_data = false) = 0;
	virtual int multimesh_get_instance_count(RID p_multimesh) const = 0;

	virtual void multimesh_set_mesh(RID p_multimesh, RID p_mesh) = 0;
	virtual void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) = 0;
	virtual void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) = 0;
	virtual void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) = 0;
	virtual void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) = 0;

	virtual RID multimesh_get_mesh(RID p_multimesh) const = 0;
	virtual AABB multimesh_get_aabb(RID p_multimesh) const = 0;

	virtual void multimesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) = 0;
	virtual AABB multimesh_get_custom_aabb(RID p_mesh) const = 0;

	virtual Transform3D multimesh_instance_get_transform(RID p_multimesh, int p_index) const = 0;
	virtual Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const = 0;
	virtual Color multimesh_instance_get_color(RID p_multimesh, int p_index) const = 0;
	virtual Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const = 0;

	virtual void multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) = 0;
	virtual Vector<float> multimesh_get_buffer(RID p_multimesh) const = 0;

	// Interpolation.
	virtual void multimesh_set_buffer_interpolated(RID p_multimesh, const Vector<float> &p_buffer_curr, const Vector<float> &p_buffer_prev) = 0;
	virtual void multimesh_set_physics_interpolated(RID p_multimesh, bool p_interpolated) = 0;
	virtual void multimesh_set_physics_interpolation_quality(RID p_multimesh, MultimeshPhysicsInterpolationQuality p_quality) = 0;
	virtual void multimesh_instance_reset_physics_interpolation(RID p_multimesh, int p_index) = 0;

	virtual void multimesh_set_visible_instances(RID p_multimesh, int p_visible) = 0;
	virtual int multimesh_get_visible_instances(RID p_multimesh) const = 0;

	/* SKELETON API */

	virtual RID skeleton_create() = 0;
	virtual void skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) = 0;
	virtual int skeleton_get_bone_count(RID p_skeleton) const = 0;
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) = 0;
	virtual Transform3D skeleton_bone_get_transform(RID p_skeleton, int p_bone) const = 0;
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) = 0;
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const = 0;
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) = 0;

	/* Light API */

	enum LightType {
		LIGHT_DIRECTIONAL,
		LIGHT_OMNI,
		LIGHT_SPOT
	};

	enum LightParam {
		LIGHT_PARAM_ENERGY,
		LIGHT_PARAM_INDIRECT_ENERGY,
		LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY,
		LIGHT_PARAM_SPECULAR,
		LIGHT_PARAM_RANGE,
		LIGHT_PARAM_SIZE,
		LIGHT_PARAM_ATTENUATION,
		LIGHT_PARAM_SPOT_ANGLE,
		LIGHT_PARAM_SPOT_ATTENUATION,
		LIGHT_PARAM_SHADOW_MAX_DISTANCE,
		LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET,
		LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET,
		LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET,
		LIGHT_PARAM_SHADOW_FADE_START,
		LIGHT_PARAM_SHADOW_NORMAL_BIAS,
		LIGHT_PARAM_SHADOW_BIAS,
		LIGHT_PARAM_SHADOW_PANCAKE_SIZE,
		LIGHT_PARAM_SHADOW_OPACITY,
		LIGHT_PARAM_SHADOW_BLUR,
		LIGHT_PARAM_TRANSMITTANCE_BIAS,
		LIGHT_PARAM_INTENSITY,
		LIGHT_PARAM_MAX
	};

	virtual RID directional_light_create() = 0;
	virtual RID omni_light_create() = 0;
	virtual RID spot_light_create() = 0;

	virtual void light_set_color(RID p_light, const Color &p_color) = 0;
	virtual void light_set_param(RID p_light, LightParam p_param, float p_value) = 0;
	virtual void light_set_shadow(RID p_light, bool p_enabled) = 0;
	virtual void light_set_projector(RID p_light, RID p_texture) = 0;
	virtual void light_set_negative(RID p_light, bool p_enable) = 0;
	virtual void light_set_cull_mask(RID p_light, uint32_t p_mask) = 0;
	virtual void light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) = 0;
	virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) = 0;

	enum LightBakeMode {
		LIGHT_BAKE_DISABLED,
		LIGHT_BAKE_STATIC,
		LIGHT_BAKE_DYNAMIC,
	};

	virtual void light_set_bake_mode(RID p_light, LightBakeMode p_bake_mode) = 0;
	virtual void light_set_max_sdfgi_cascade(RID p_light, uint32_t p_cascade) = 0;

	// Omni light
	enum LightOmniShadowMode {
		LIGHT_OMNI_SHADOW_DUAL_PARABOLOID,
		LIGHT_OMNI_SHADOW_CUBE,
	};

	virtual void light_omni_set_shadow_mode(RID p_light, LightOmniShadowMode p_mode) = 0;

	// Directional light
	enum LightDirectionalShadowMode {
		LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL,
		LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS,
		LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS,
	};

	enum LightDirectionalSkyMode {
		LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY,
		LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_ONLY,
		LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY,
	};

	virtual void light_directional_set_shadow_mode(RID p_light, LightDirectionalShadowMode p_mode) = 0;
	virtual void light_directional_set_blend_splits(RID p_light, bool p_enable) = 0;
	virtual void light_directional_set_sky_mode(RID p_light, LightDirectionalSkyMode p_mode) = 0;

	// Shadow atlas

	virtual RID shadow_atlas_create() = 0;
	virtual void shadow_atlas_set_size(RID p_atlas, int p_size, bool p_use_16_bits = true) = 0;
	virtual void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) = 0;

	virtual void directional_shadow_atlas_set_size(int p_size, bool p_16_bits = true) = 0;

	enum ShadowQuality {
		SHADOW_QUALITY_HARD,
		SHADOW_QUALITY_SOFT_VERY_LOW,
		SHADOW_QUALITY_SOFT_LOW,
		SHADOW_QUALITY_SOFT_MEDIUM,
		SHADOW_QUALITY_SOFT_HIGH,
		SHADOW_QUALITY_SOFT_ULTRA,
		SHADOW_QUALITY_MAX
	};

	virtual void positional_soft_shadow_filter_set_quality(ShadowQuality p_quality) = 0;
	virtual void directional_soft_shadow_filter_set_quality(ShadowQuality p_quality) = 0;

	enum LightProjectorFilter {
		LIGHT_PROJECTOR_FILTER_NEAREST,
		LIGHT_PROJECTOR_FILTER_LINEAR,
		LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS,
		LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS,
		LIGHT_PROJECTOR_FILTER_NEAREST_MIPMAPS_ANISOTROPIC,
		LIGHT_PROJECTOR_FILTER_LINEAR_MIPMAPS_ANISOTROPIC,
	};

	virtual void light_projectors_set_filter(LightProjectorFilter p_filter) = 0;

	/* PROBE API */

	virtual RID reflection_probe_create() = 0;

	enum ReflectionProbeUpdateMode {
		REFLECTION_PROBE_UPDATE_ONCE,
		REFLECTION_PROBE_UPDATE_ALWAYS,
	};

	virtual void reflection_probe_set_update_mode(RID p_probe, ReflectionProbeUpdateMode p_mode) = 0;
	virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity) = 0;

	enum ReflectionProbeAmbientMode {
		REFLECTION_PROBE_AMBIENT_DISABLED,
		REFLECTION_PROBE_AMBIENT_ENVIRONMENT,
		REFLECTION_PROBE_AMBIENT_COLOR,
	};

	virtual void reflection_probe_set_ambient_mode(RID p_probe, ReflectionProbeAmbientMode p_mode) = 0;
	virtual void reflection_probe_set_ambient_color(RID p_probe, const Color &p_color) = 0;
	virtual void reflection_probe_set_ambient_energy(RID p_probe, float p_energy) = 0;
	virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance) = 0;
	virtual void reflection_probe_set_size(RID p_probe, const Vector3 &p_size) = 0;
	virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) = 0;
	virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) = 0;
	virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) = 0;
	virtual void reflection_probe_set_reflection_mask(RID p_probe, uint32_t p_layers) = 0;
	virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution) = 0;
	virtual void reflection_probe_set_mesh_lod_threshold(RID p_probe, float p_pixels) = 0;

	/* DECAL API */

	enum DecalTexture {
		DECAL_TEXTURE_ALBEDO,
		DECAL_TEXTURE_NORMAL,
		DECAL_TEXTURE_ORM,
		DECAL_TEXTURE_EMISSION,
		DECAL_TEXTURE_MAX
	};

	virtual RID decal_create() = 0;
	virtual void decal_set_size(RID p_decal, const Vector3 &p_size) = 0;
	virtual void decal_set_texture(RID p_decal, DecalTexture p_type, RID p_texture) = 0;
	virtual void decal_set_emission_energy(RID p_decal, float p_energy) = 0;
	virtual void decal_set_albedo_mix(RID p_decal, float p_mix) = 0;
	virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) = 0;
	virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) = 0;
	virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) = 0;
	virtual void decal_set_fade(RID p_decal, float p_above, float p_below) = 0;
	virtual void decal_set_normal_fade(RID p_decal, float p_fade) = 0;

	enum DecalFilter {
		DECAL_FILTER_NEAREST,
		DECAL_FILTER_LINEAR,
		DECAL_FILTER_NEAREST_MIPMAPS,
		DECAL_FILTER_LINEAR_MIPMAPS,
		DECAL_FILTER_NEAREST_MIPMAPS_ANISOTROPIC,
		DECAL_FILTER_LINEAR_MIPMAPS_ANISOTROPIC,
	};

	virtual void decals_set_filter(DecalFilter p_quality) = 0;

	/* VOXEL GI API */

	virtual RID voxel_gi_create() = 0;

	virtual void voxel_gi_allocate_data(RID p_voxel_gi, const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3i &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts) = 0;

	virtual AABB voxel_gi_get_bounds(RID p_voxel_gi) const = 0;
	virtual Vector3i voxel_gi_get_octree_size(RID p_voxel_gi) const = 0;
	virtual Vector<uint8_t> voxel_gi_get_octree_cells(RID p_voxel_gi) const = 0;
	virtual Vector<uint8_t> voxel_gi_get_data_cells(RID p_voxel_gi) const = 0;
	virtual Vector<uint8_t> voxel_gi_get_distance_field(RID p_voxel_gi) const = 0;
	virtual Vector<int> voxel_gi_get_level_counts(RID p_voxel_gi) const = 0;
	virtual Transform3D voxel_gi_get_to_cell_xform(RID p_voxel_gi) const = 0;

	virtual void voxel_gi_set_dynamic_range(RID p_voxel_gi, float p_range) = 0;
	virtual void voxel_gi_set_propagation(RID p_voxel_gi, float p_range) = 0;
	virtual void voxel_gi_set_energy(RID p_voxel_gi, float p_energy) = 0;
	virtual void voxel_gi_set_baked_exposure_normalization(RID p_voxel_gi, float p_baked_exposure) = 0;
	virtual void voxel_gi_set_bias(RID p_voxel_gi, float p_bias) = 0;
	virtual void voxel_gi_set_normal_bias(RID p_voxel_gi, float p_range) = 0;
	virtual void voxel_gi_set_interior(RID p_voxel_gi, bool p_enable) = 0;
	virtual void voxel_gi_set_use_two_bounces(RID p_voxel_gi, bool p_enable) = 0;

	enum VoxelGIQuality {
		VOXEL_GI_QUALITY_LOW,
		VOXEL_GI_QUALITY_HIGH,
	};

	virtual void voxel_gi_set_quality(VoxelGIQuality) = 0;

	virtual void sdfgi_reset() = 0;

	/* LIGHTMAP */

	virtual RID lightmap_create() = 0;

	virtual void lightmap_set_textures(RID p_lightmap, RID p_light, bool p_uses_spherical_haromics) = 0;
	virtual void lightmap_set_probe_bounds(RID p_lightmap, const AABB &p_bounds) = 0;
	virtual void lightmap_set_probe_interior(RID p_lightmap, bool p_interior) = 0;
	virtual void lightmap_set_probe_capture_data(RID p_lightmap, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree) = 0;
	virtual void lightmap_set_baked_exposure_normalization(RID p_lightmap, float p_exposure) = 0;
	virtual PackedVector3Array lightmap_get_probe_capture_points(RID p_lightmap) const = 0;
	virtual PackedColorArray lightmap_get_probe_capture_sh(RID p_lightmap) const = 0;
	virtual PackedInt32Array lightmap_get_probe_capture_tetrahedra(RID p_lightmap) const = 0;
	virtual PackedInt32Array lightmap_get_probe_capture_bsp_tree(RID p_lightmap) const = 0;

	virtual void lightmap_set_probe_capture_update_speed(float p_speed) = 0;

	virtual void lightmaps_set_bicubic_filter(bool p_enable) = 0;

	/* PARTICLES API */

	virtual RID particles_create() = 0;

	enum ParticlesMode {
		PARTICLES_MODE_2D,
		PARTICLES_MODE_3D
	};
	virtual void particles_set_mode(RID p_particles, ParticlesMode p_mode) = 0;

	virtual void particles_set_emitting(RID p_particles, bool p_enable) = 0;
	virtual bool particles_get_emitting(RID p_particles) = 0;
	virtual void particles_set_amount(RID p_particles, int p_amount) = 0;
	virtual void particles_set_amount_ratio(RID p_particles, float p_amount_ratio) = 0;
	virtual void particles_set_lifetime(RID p_particles, double p_lifetime) = 0;
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) = 0;
	virtual void particles_set_pre_process_time(RID p_particles, double p_time) = 0;
	virtual void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) = 0;
	virtual void particles_set_randomness_ratio(RID p_particles, float p_ratio) = 0;
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) = 0;
	virtual void particles_set_speed_scale(RID p_particles, double p_scale) = 0;
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_process_material(RID p_particles, RID p_material) = 0;
	virtual void particles_set_fixed_fps(RID p_particles, int p_fps) = 0;
	virtual void particles_set_interpolate(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) = 0;
	virtual void particles_set_collision_base_size(RID p_particles, float p_size) = 0;

	enum ParticlesTransformAlign {
		PARTICLES_TRANSFORM_ALIGN_DISABLED,
		PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD,
		PARTICLES_TRANSFORM_ALIGN_Y_TO_VELOCITY,
		PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY,
	};

	virtual void particles_set_transform_align(RID p_particles, ParticlesTransformAlign p_transform_align) = 0;

	virtual void particles_set_trails(RID p_particles, bool p_enable, float p_length_sec) = 0;
	virtual void particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) = 0;

	virtual bool particles_is_inactive(RID p_particles) = 0;
	virtual void particles_request_process(RID p_particles) = 0;
	virtual void particles_restart(RID p_particles) = 0;

	virtual void particles_set_subemitter(RID p_particles, RID p_subemitter_particles) = 0;

	enum ParticlesEmitFlags {
		PARTICLES_EMIT_FLAG_POSITION = 1,
		PARTICLES_EMIT_FLAG_ROTATION_SCALE = 2,
		PARTICLES_EMIT_FLAG_VELOCITY = 4,
		PARTICLES_EMIT_FLAG_COLOR = 8,
		PARTICLES_EMIT_FLAG_CUSTOM = 16
	};

	virtual void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) = 0;

	enum ParticlesDrawOrder {
		PARTICLES_DRAW_ORDER_INDEX,
		PARTICLES_DRAW_ORDER_LIFETIME,
		PARTICLES_DRAW_ORDER_REVERSE_LIFETIME,
		PARTICLES_DRAW_ORDER_VIEW_DEPTH,
	};

	virtual void particles_set_draw_order(RID p_particles, ParticlesDrawOrder p_order) = 0;

	virtual void particles_set_draw_passes(RID p_particles, int p_count) = 0;
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) = 0;

	virtual AABB particles_get_current_aabb(RID p_particles) = 0;

	virtual void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) = 0; // This is only used for 2D, in 3D it's automatic.
	virtual void particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) = 0;
	virtual void particles_set_interp_to_end(RID p_particles, float p_interp) = 0;

	/* PARTICLES COLLISION API */

	virtual RID particles_collision_create() = 0;

	enum ParticlesCollisionType {
		PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT,
		PARTICLES_COLLISION_TYPE_BOX_ATTRACT,
		PARTICLES_COLLISION_TYPE_VECTOR_FIELD_ATTRACT,
		PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE,
		PARTICLES_COLLISION_TYPE_BOX_COLLIDE,
		PARTICLES_COLLISION_TYPE_SDF_COLLIDE,
		PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE,
	};

	virtual void particles_collision_set_collision_type(RID p_particles_collision, ParticlesCollisionType p_type) = 0;
	virtual void particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) = 0;
	virtual void particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) = 0; // For spheres.
	virtual void particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) = 0; // For non-spheres.
	virtual void particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) = 0;
	virtual void particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) = 0;
	virtual void particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) = 0;
	virtual void particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) = 0; // For SDF and vector field, heightfield is dynamic.

	virtual void particles_collision_height_field_update(RID p_particles_collision) = 0; // For SDF and vector field.

	enum ParticlesCollisionHeightfieldResolution { // Longest axis resolution.
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_256,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_512,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_1024,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_2048,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_4096,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_8192,
		PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX,
	};

	virtual void particles_collision_set_height_field_resolution(RID p_particles_collision, ParticlesCollisionHeightfieldResolution p_resolution) = 0; // For SDF and vector field.

	/* FOG VOLUME API */

	virtual RID fog_volume_create() = 0;

	enum FogVolumeShape {
		FOG_VOLUME_SHAPE_ELLIPSOID,
		FOG_VOLUME_SHAPE_CONE,
		FOG_VOLUME_SHAPE_CYLINDER,
		FOG_VOLUME_SHAPE_BOX,
		FOG_VOLUME_SHAPE_WORLD,
		FOG_VOLUME_SHAPE_MAX,
	};

	virtual void fog_volume_set_shape(RID p_fog_volume, FogVolumeShape p_shape) = 0;
	virtual void fog_volume_set_size(RID p_fog_volume, const Vector3 &p_size) = 0;
	virtual void fog_volume_set_material(RID p_fog_volume, RID p_material) = 0;

	/* VISIBILITY NOTIFIER API */

	virtual RID visibility_notifier_create() = 0;
	virtual void visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) = 0;
	virtual void visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) = 0;

	/* OCCLUDER API */

	virtual RID occluder_create() = 0;
	virtual void occluder_set_mesh(RID p_occluder, const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices) = 0;

	/* CAMERA API */

	virtual RID camera_create() = 0;
	virtual void camera_set_perspective(RID p_camera, float p_fovy_degrees, float p_z_near, float p_z_far) = 0;
	virtual void camera_set_orthogonal(RID p_camera, float p_size, float p_z_near, float p_z_far) = 0;
	virtual void camera_set_frustum(RID p_camera, float p_size, Vector2 p_offset, float p_z_near, float p_z_far) = 0;
	virtual void camera_set_transform(RID p_camera, const Transform3D &p_transform) = 0;
	virtual void camera_set_cull_mask(RID p_camera, uint32_t p_layers) = 0;
	virtual void camera_set_environment(RID p_camera, RID p_env) = 0;
	virtual void camera_set_camera_attributes(RID p_camera, RID p_camera_attributes) = 0;
	virtual void camera_set_compositor(RID p_camera, RID p_compositor) = 0;
	virtual void camera_set_use_vertical_aspect(RID p_camera, bool p_enable) = 0;

	/* VIEWPORT API */

	enum CanvasItemTextureFilter {
		CANVAS_ITEM_TEXTURE_FILTER_DEFAULT, // Uses canvas item setting for draw command, uses global setting for canvas item.
		CANVAS_ITEM_TEXTURE_FILTER_NEAREST,
		CANVAS_ITEM_TEXTURE_FILTER_LINEAR,
		CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS,
		CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS,
		CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC,
		CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC,
		CANVAS_ITEM_TEXTURE_FILTER_MAX
	};

	enum CanvasItemTextureRepeat {
		CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT, // Uses canvas item setting for draw command, uses global setting for canvas item.
		CANVAS_ITEM_TEXTURE_REPEAT_DISABLED,
		CANVAS_ITEM_TEXTURE_REPEAT_ENABLED,
		CANVAS_ITEM_TEXTURE_REPEAT_MIRROR,
		CANVAS_ITEM_TEXTURE_REPEAT_MAX,
	};

	virtual RID viewport_create() = 0;

	enum ViewportScaling3DMode {
		VIEWPORT_SCALING_3D_MODE_BILINEAR,
		VIEWPORT_SCALING_3D_MODE_FSR,
		VIEWPORT_SCALING_3D_MODE_FSR2,
		VIEWPORT_SCALING_3D_MODE_MAX,
		VIEWPORT_SCALING_3D_MODE_OFF = 255, // for internal use only
	};

	virtual void viewport_set_use_xr(RID p_viewport, bool p_use_xr) = 0;
	virtual void viewport_set_size(RID p_viewport, int p_width, int p_height) = 0;
	virtual void viewport_set_active(RID p_viewport, bool p_active) = 0;
	virtual void viewport_set_parent_viewport(RID p_viewport, RID p_parent_viewport) = 0;
	virtual void viewport_set_canvas_cull_mask(RID p_viewport, uint32_t p_canvas_cull_mask) = 0;

	virtual void viewport_attach_to_screen(RID p_viewport, const Rect2 &p_rect = Rect2(), DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID) = 0;
	virtual void viewport_set_render_direct_to_screen(RID p_viewport, bool p_enable) = 0;

	virtual void viewport_set_scaling_3d_mode(RID p_viewport, ViewportScaling3DMode p_scaling_3d_mode) = 0;
	virtual void viewport_set_scaling_3d_scale(RID p_viewport, float p_scaling_3d_scale) = 0;
	virtual void viewport_set_fsr_sharpness(RID p_viewport, float p_fsr_sharpness) = 0;
	virtual void viewport_set_texture_mipmap_bias(RID p_viewport, float p_texture_mipmap_bias) = 0;

	enum ViewportUpdateMode {
		VIEWPORT_UPDATE_DISABLED,
		VIEWPORT_UPDATE_ONCE, // Then goes to disabled, must be manually updated.
		VIEWPORT_UPDATE_WHEN_VISIBLE, // Default
		VIEWPORT_UPDATE_WHEN_PARENT_VISIBLE,
		VIEWPORT_UPDATE_ALWAYS
	};

	virtual void viewport_set_update_mode(RID p_viewport, ViewportUpdateMode p_mode) = 0;
	virtual ViewportUpdateMode viewport_get_update_mode(RID p_viewport) const = 0;

	enum ViewportClearMode {
		VIEWPORT_CLEAR_ALWAYS,
		VIEWPORT_CLEAR_NEVER,
		VIEWPORT_CLEAR_ONLY_NEXT_FRAME
	};

	virtual void viewport_set_clear_mode(RID p_viewport, ViewportClearMode p_clear_mode) = 0;

	virtual RID viewport_get_render_target(RID p_viewport) const = 0;
	virtual RID viewport_get_texture(RID p_viewport) const = 0;

	enum ViewportEnvironmentMode {
		VIEWPORT_ENVIRONMENT_DISABLED,
		VIEWPORT_ENVIRONMENT_ENABLED,
		VIEWPORT_ENVIRONMENT_INHERIT,
		VIEWPORT_ENVIRONMENT_MAX,
	};

	virtual void viewport_set_environment_mode(RID p_viewport, ViewportEnvironmentMode p_mode) = 0;
	virtual void viewport_set_disable_3d(RID p_viewport, bool p_disable) = 0;
	virtual void viewport_set_disable_2d(RID p_viewport, bool p_disable) = 0;

	virtual void viewport_attach_camera(RID p_viewport, RID p_camera) = 0;
	virtual void viewport_set_scenario(RID p_viewport, RID p_scenario) = 0;
	virtual void viewport_attach_canvas(RID p_viewport, RID p_canvas) = 0;
	virtual void viewport_remove_canvas(RID p_viewport, RID p_canvas) = 0;
	virtual void viewport_set_canvas_transform(RID p_viewport, RID p_canvas, const Transform2D &p_offset) = 0;
	virtual void viewport_set_transparent_background(RID p_viewport, bool p_enabled) = 0;
	virtual void viewport_set_use_hdr_2d(RID p_viewport, bool p_use_hdr) = 0;
	virtual bool viewport_is_using_hdr_2d(RID p_viewport) const = 0;
	virtual void viewport_set_snap_2d_transforms_to_pixel(RID p_viewport, bool p_enabled) = 0;
	virtual void viewport_set_snap_2d_vertices_to_pixel(RID p_viewport, bool p_enabled) = 0;

	virtual void viewport_set_default_canvas_item_texture_filter(RID p_viewport, CanvasItemTextureFilter p_filter) = 0;
	virtual void viewport_set_default_canvas_item_texture_repeat(RID p_viewport, CanvasItemTextureRepeat p_repeat) = 0;

	virtual void viewport_set_global_canvas_transform(RID p_viewport, const Transform2D &p_transform) = 0;
	virtual void viewport_set_canvas_stacking(RID p_viewport, RID p_canvas, int p_layer, int p_sublayer) = 0;

	enum ViewportSDFOversize {
		VIEWPORT_SDF_OVERSIZE_100_PERCENT,
		VIEWPORT_SDF_OVERSIZE_120_PERCENT,
		VIEWPORT_SDF_OVERSIZE_150_PERCENT,
		VIEWPORT_SDF_OVERSIZE_200_PERCENT,
		VIEWPORT_SDF_OVERSIZE_MAX
	};

	enum ViewportSDFScale {
		VIEWPORT_SDF_SCALE_100_PERCENT,
		VIEWPORT_SDF_SCALE_50_PERCENT,
		VIEWPORT_SDF_SCALE_25_PERCENT,
		VIEWPORT_SDF_SCALE_MAX
	};

	virtual void viewport_set_sdf_oversize_and_scale(RID p_viewport, ViewportSDFOversize p_oversize, ViewportSDFScale p_scale) = 0;

	virtual void viewport_set_positional_shadow_atlas_size(RID p_viewport, int p_size, bool p_16_bits = true) = 0;
	virtual void viewport_set_positional_shadow_atlas_quadrant_subdivision(RID p_viewport, int p_quadrant, int p_subdiv) = 0;

	enum ViewportMSAA {
		VIEWPORT_MSAA_DISABLED,
		VIEWPORT_MSAA_2X,
		VIEWPORT_MSAA_4X,
		VIEWPORT_MSAA_8X,
		VIEWPORT_MSAA_MAX,
	};

	virtual void viewport_set_msaa_3d(RID p_viewport, ViewportMSAA p_msaa) = 0;
	virtual void viewport_set_msaa_2d(RID p_viewport, ViewportMSAA p_msaa) = 0;

	enum ViewportScreenSpaceAA {
		VIEWPORT_SCREEN_SPACE_AA_DISABLED,
		VIEWPORT_SCREEN_SPACE_AA_FXAA,
		VIEWPORT_SCREEN_SPACE_AA_MAX,
	};

	virtual void viewport_set_screen_space_aa(RID p_viewport, ViewportScreenSpaceAA p_mode) = 0;

	virtual void viewport_set_use_taa(RID p_viewport, bool p_use_taa) = 0;

	virtual void viewport_set_use_debanding(RID p_viewport, bool p_use_debanding) = 0;

	virtual void viewport_set_mesh_lod_threshold(RID p_viewport, float p_pixels) = 0;

	virtual void viewport_set_use_occlusion_culling(RID p_viewport, bool p_use_occlusion_culling) = 0;
	virtual void viewport_set_occlusion_rays_per_thread(int p_rays_per_thread) = 0;

	enum ViewportOcclusionCullingBuildQuality {
		VIEWPORT_OCCLUSION_BUILD_QUALITY_LOW = 0,
		VIEWPORT_OCCLUSION_BUILD_QUALITY_MEDIUM = 1,
		VIEWPORT_OCCLUSION_BUILD_QUALITY_HIGH = 2,
	};

	virtual void viewport_set_occlusion_culling_build_quality(ViewportOcclusionCullingBuildQuality p_quality) = 0;

	enum ViewportRenderInfo {
		VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME,
		VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME,
		VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME,
		VIEWPORT_RENDER_INFO_MAX,
	};

	enum ViewportRenderInfoType {
		VIEWPORT_RENDER_INFO_TYPE_VISIBLE,
		VIEWPORT_RENDER_INFO_TYPE_SHADOW,
		VIEWPORT_RENDER_INFO_TYPE_CANVAS,
		VIEWPORT_RENDER_INFO_TYPE_MAX
	};

	virtual int viewport_get_render_info(RID p_viewport, ViewportRenderInfoType p_type, ViewportRenderInfo p_info) = 0;

	enum ViewportDebugDraw {
		VIEWPORT_DEBUG_DRAW_DISABLED,
		VIEWPORT_DEBUG_DRAW_UNSHADED,
		VIEWPORT_DEBUG_DRAW_LIGHTING,
		VIEWPORT_DEBUG_DRAW_OVERDRAW,
		VIEWPORT_DEBUG_DRAW_WIREFRAME,
		VIEWPORT_DEBUG_DRAW_NORMAL_BUFFER,
		VIEWPORT_DEBUG_DRAW_VOXEL_GI_ALBEDO,
		VIEWPORT_DEBUG_DRAW_VOXEL_GI_LIGHTING,
		VIEWPORT_DEBUG_DRAW_VOXEL_GI_EMISSION,
		VIEWPORT_DEBUG_DRAW_SHADOW_ATLAS,
		VIEWPORT_DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS,
		VIEWPORT_DEBUG_DRAW_SCENE_LUMINANCE,
		VIEWPORT_DEBUG_DRAW_SSAO,
		VIEWPORT_DEBUG_DRAW_SSIL,
		VIEWPORT_DEBUG_DRAW_PSSM_SPLITS,
		VIEWPORT_DEBUG_DRAW_DECAL_ATLAS,
		VIEWPORT_DEBUG_DRAW_SDFGI,
		VIEWPORT_DEBUG_DRAW_SDFGI_PROBES,
		VIEWPORT_DEBUG_DRAW_GI_BUFFER,
		VIEWPORT_DEBUG_DRAW_DISABLE_LOD,
		VIEWPORT_DEBUG_DRAW_CLUSTER_OMNI_LIGHTS,
		VIEWPORT_DEBUG_DRAW_CLUSTER_SPOT_LIGHTS,
		VIEWPORT_DEBUG_DRAW_CLUSTER_DECALS,
		VIEWPORT_DEBUG_DRAW_CLUSTER_REFLECTION_PROBES,
		VIEWPORT_DEBUG_DRAW_OCCLUDERS,
		VIEWPORT_DEBUG_DRAW_MOTION_VECTORS,
		VIEWPORT_DEBUG_DRAW_INTERNAL_BUFFER,
	};

	virtual void viewport_set_debug_draw(RID p_viewport, ViewportDebugDraw p_draw) = 0;

	virtual void viewport_set_measure_render_time(RID p_viewport, bool p_enable) = 0;
	virtual double viewport_get_measured_render_time_cpu(RID p_viewport) const = 0;
	virtual double viewport_get_measured_render_time_gpu(RID p_viewport) const = 0;

	virtual RID viewport_find_from_screen_attachment(DisplayServer::WindowID p_id = DisplayServer::MAIN_WINDOW_ID) const = 0;

	enum ViewportVRSMode {
		VIEWPORT_VRS_DISABLED,
		VIEWPORT_VRS_TEXTURE,
		VIEWPORT_VRS_XR,
		VIEWPORT_VRS_MAX,
	};

	enum ViewportVRSUpdateMode {
		VIEWPORT_VRS_UPDATE_DISABLED,
		VIEWPORT_VRS_UPDATE_ONCE,
		VIEWPORT_VRS_UPDATE_ALWAYS,
		VIEWPORT_VRS_UPDATE_MAX,
	};

	virtual void viewport_set_vrs_mode(RID p_viewport, ViewportVRSMode p_mode) = 0;
	virtual void viewport_set_vrs_update_mode(RID p_viewport, ViewportVRSUpdateMode p_mode) = 0;
	virtual void viewport_set_vrs_texture(RID p_viewport, RID p_texture) = 0;

	/* SKY API */

	enum SkyMode {
		SKY_MODE_AUTOMATIC,
		SKY_MODE_QUALITY,
		SKY_MODE_INCREMENTAL,
		SKY_MODE_REALTIME
	};

	virtual RID sky_create() = 0;
	virtual void sky_set_radiance_size(RID p_sky, int p_radiance_size) = 0;
	virtual void sky_set_mode(RID p_sky, SkyMode p_mode) = 0;
	virtual void sky_set_material(RID p_sky, RID p_material) = 0;
	virtual Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) = 0;

	/* COMPOSITOR EFFECTS API */

	enum CompositorEffectFlags {
		COMPOSITOR_EFFECT_FLAG_ACCESS_RESOLVED_COLOR = 1,
		COMPOSITOR_EFFECT_FLAG_ACCESS_RESOLVED_DEPTH = 2,
		COMPOSITOR_EFFECT_FLAG_NEEDS_MOTION_VECTORS = 4,
		COMPOSITOR_EFFECT_FLAG_NEEDS_ROUGHNESS = 8,
		COMPOSITOR_EFFECT_FLAG_NEEDS_SEPARATE_SPECULAR = 16,
	};

	enum CompositorEffectCallbackType {
		COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_OPAQUE,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_OPAQUE,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_SKY,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_TRANSPARENT,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_MAX,
		COMPOSITOR_EFFECT_CALLBACK_TYPE_ANY = -1,
	};

	virtual RID compositor_effect_create() = 0;
	virtual void compositor_effect_set_enabled(RID p_effect, bool p_enabled) = 0;
	virtual void compositor_effect_set_callback(RID p_effect, CompositorEffectCallbackType p_callback_type, const Callable &p_callback) = 0;
	virtual void compositor_effect_set_flag(RID p_effect, CompositorEffectFlags p_flag, bool p_set) = 0;

	/* COMPOSITOR API */

	virtual RID compositor_create() = 0;

	virtual void compositor_set_compositor_effects(RID p_compositor, const TypedArray<RID> &p_effects) = 0;

	/* ENVIRONMENT API */

	virtual RID environment_create() = 0;

	enum EnvironmentBG {
		ENV_BG_CLEAR_COLOR,
		ENV_BG_COLOR,
		ENV_BG_SKY,
		ENV_BG_CANVAS,
		ENV_BG_KEEP,
		ENV_BG_CAMERA_FEED,
		ENV_BG_MAX
	};

	enum EnvironmentAmbientSource {
		ENV_AMBIENT_SOURCE_BG,
		ENV_AMBIENT_SOURCE_DISABLED,
		ENV_AMBIENT_SOURCE_COLOR,
		ENV_AMBIENT_SOURCE_SKY,
	};

	enum EnvironmentReflectionSource {
		ENV_REFLECTION_SOURCE_BG,
		ENV_REFLECTION_SOURCE_DISABLED,
		ENV_REFLECTION_SOURCE_SKY,
	};

	virtual void environment_set_background(RID p_env, EnvironmentBG p_bg) = 0;
	virtual void environment_set_sky(RID p_env, RID p_sky) = 0;
	virtual void environment_set_sky_custom_fov(RID p_env, float p_scale) = 0;
	virtual void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) = 0;
	virtual void environment_set_bg_color(RID p_env, const Color &p_color) = 0;
	virtual void environment_set_bg_energy(RID p_env, float p_multiplier, float p_exposure_value) = 0;
	virtual void environment_set_canvas_max_layer(RID p_env, int p_max_layer) = 0;
	virtual void environment_set_ambient_light(RID p_env, const Color &p_color, EnvironmentAmbientSource p_ambient = ENV_AMBIENT_SOURCE_BG, float p_energy = 1.0, float p_sky_contribution = 0.0, EnvironmentReflectionSource p_reflection_source = ENV_REFLECTION_SOURCE_BG) = 0;

	enum EnvironmentGlowBlendMode {
		ENV_GLOW_BLEND_MODE_ADDITIVE,
		ENV_GLOW_BLEND_MODE_SCREEN,
		ENV_GLOW_BLEND_MODE_SOFTLIGHT,
		ENV_GLOW_BLEND_MODE_REPLACE,
		ENV_GLOW_BLEND_MODE_MIX,
	};

	virtual void environment_set_glow(RID p_env, bool p_enable, Vector<float> p_levels, float p_intensity, float p_strength, float p_mix, float p_bloom_threshold, EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, float p_glow_map_strength, RID p_glow_map) = 0;

	virtual void environment_glow_set_use_bicubic_upscale(bool p_enable) = 0;

	enum EnvironmentToneMapper {
		ENV_TONE_MAPPER_LINEAR,
		ENV_TONE_MAPPER_REINHARD,
		ENV_TONE_MAPPER_FILMIC,
		ENV_TONE_MAPPER_ACES
	};

	virtual void environment_set_tonemap(RID p_env, EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white) = 0;
	virtual void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, bool p_use_1d_color_correction, RID p_color_correction) = 0;

	virtual void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_in, float p_fade_out, float p_depth_tolerance) = 0;

	enum EnvironmentSSRRoughnessQuality {
		ENV_SSR_ROUGHNESS_QUALITY_DISABLED,
		ENV_SSR_ROUGHNESS_QUALITY_LOW,
		ENV_SSR_ROUGHNESS_QUALITY_MEDIUM,
		ENV_SSR_ROUGHNESS_QUALITY_HIGH,
	};

	virtual void environment_set_ssr_roughness_quality(EnvironmentSSRRoughnessQuality p_quality) = 0;

	virtual void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_power, float p_detail, float p_horizon, float p_sharpness, float p_light_affect, float p_ao_channel_affect) = 0;

	enum EnvironmentSSAOQuality {
		ENV_SSAO_QUALITY_VERY_LOW,
		ENV_SSAO_QUALITY_LOW,
		ENV_SSAO_QUALITY_MEDIUM,
		ENV_SSAO_QUALITY_HIGH,
		ENV_SSAO_QUALITY_ULTRA,
	};

	virtual void environment_set_ssao_quality(EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) = 0;

	virtual void environment_set_ssil(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_sharpness, float p_normal_rejection) = 0;

	enum EnvironmentSSILQuality {
		ENV_SSIL_QUALITY_VERY_LOW,
		ENV_SSIL_QUALITY_LOW,
		ENV_SSIL_QUALITY_MEDIUM,
		ENV_SSIL_QUALITY_HIGH,
		ENV_SSIL_QUALITY_ULTRA,
	};

	virtual void environment_set_ssil_quality(EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) = 0;

	enum EnvironmentSDFGIYScale {
		ENV_SDFGI_Y_SCALE_50_PERCENT,
		ENV_SDFGI_Y_SCALE_75_PERCENT,
		ENV_SDFGI_Y_SCALE_100_PERCENT,
	};

	virtual void environment_set_sdfgi(RID p_env, bool p_enable, int p_cascades, float p_min_cell_size, EnvironmentSDFGIYScale p_y_scale, bool p_use_occlusion, float p_bounce_feedback, bool p_read_sky, float p_energy, float p_normal_bias, float p_probe_bias) = 0;

	enum EnvironmentSDFGIRayCount {
		ENV_SDFGI_RAY_COUNT_4,
		ENV_SDFGI_RAY_COUNT_8,
		ENV_SDFGI_RAY_COUNT_16,
		ENV_SDFGI_RAY_COUNT_32,
		ENV_SDFGI_RAY_COUNT_64,
		ENV_SDFGI_RAY_COUNT_96,
		ENV_SDFGI_RAY_COUNT_128,
		ENV_SDFGI_RAY_COUNT_MAX,
	};

	virtual void environment_set_sdfgi_ray_count(EnvironmentSDFGIRayCount p_ray_count) = 0;

	enum EnvironmentSDFGIFramesToConverge {
		ENV_SDFGI_CONVERGE_IN_5_FRAMES,
		ENV_SDFGI_CONVERGE_IN_10_FRAMES,
		ENV_SDFGI_CONVERGE_IN_15_FRAMES,
		ENV_SDFGI_CONVERGE_IN_20_FRAMES,
		ENV_SDFGI_CONVERGE_IN_25_FRAMES,
		ENV_SDFGI_CONVERGE_IN_30_FRAMES,
		ENV_SDFGI_CONVERGE_MAX
	};

	virtual void environment_set_sdfgi_frames_to_converge(EnvironmentSDFGIFramesToConverge p_frames) = 0;

	enum EnvironmentSDFGIFramesToUpdateLight {
		ENV_SDFGI_UPDATE_LIGHT_IN_1_FRAME,
		ENV_SDFGI_UPDATE_LIGHT_IN_2_FRAMES,
		ENV_SDFGI_UPDATE_LIGHT_IN_4_FRAMES,
		ENV_SDFGI_UPDATE_LIGHT_IN_8_FRAMES,
		ENV_SDFGI_UPDATE_LIGHT_IN_16_FRAMES,
		ENV_SDFGI_UPDATE_LIGHT_MAX,
	};

	virtual void environment_set_sdfgi_frames_to_update_light(EnvironmentSDFGIFramesToUpdateLight p_update) = 0;

	enum EnvironmentFogMode {
		ENV_FOG_MODE_EXPONENTIAL,
		ENV_FOG_MODE_DEPTH,
	};

	virtual void environment_set_fog(RID p_env, bool p_enable, const Color &p_light_color, float p_light_energy, float p_sun_scatter, float p_density, float p_height, float p_height_density, float p_aerial_perspective, float p_sky_affect, EnvironmentFogMode p_mode = EnvironmentFogMode::ENV_FOG_MODE_EXPONENTIAL) = 0;
	virtual void environment_set_fog_depth(RID p_env, float p_curve, float p_begin, float p_end) = 0;

	virtual void environment_set_volumetric_fog(RID p_env, bool p_enable, float p_density, const Color &p_albedo, const Color &p_emission, float p_emission_energy, float p_anisotropy, float p_length, float p_detail_spread, float p_gi_inject, bool p_temporal_reprojection, float p_temporal_reprojection_amount, float p_ambient_inject, float p_sky_affect) = 0;
	virtual void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) = 0;
	virtual void environment_set_volumetric_fog_filter_active(bool p_enable) = 0;

	virtual Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) = 0;

	virtual void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) = 0;

	enum SubSurfaceScatteringQuality {
		SUB_SURFACE_SCATTERING_QUALITY_DISABLED,
		SUB_SURFACE_SCATTERING_QUALITY_LOW,
		SUB_SURFACE_SCATTERING_QUALITY_MEDIUM,
		SUB_SURFACE_SCATTERING_QUALITY_HIGH,
	};

	virtual void sub_surface_scattering_set_quality(SubSurfaceScatteringQuality p_quality) = 0;
	virtual void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) = 0;

	/* CAMERA EFFECTS */

	virtual RID camera_attributes_create() = 0;

	enum DOFBlurQuality {
		DOF_BLUR_QUALITY_VERY_LOW,
		DOF_BLUR_QUALITY_LOW,
		DOF_BLUR_QUALITY_MEDIUM,
		DOF_BLUR_QUALITY_HIGH,
	};

	virtual void camera_attributes_set_dof_blur_quality(DOFBlurQuality p_quality, bool p_use_jitter) = 0;

	enum DOFBokehShape {
		DOF_BOKEH_BOX,
		DOF_BOKEH_HEXAGON,
		DOF_BOKEH_CIRCLE
	};

	virtual void camera_attributes_set_dof_blur_bokeh_shape(DOFBokehShape p_shape) = 0;

	virtual void camera_attributes_set_dof_blur(RID p_camera_attributes, bool p_far_enable, float p_far_distance, float p_far_transition, bool p_near_enable, float p_near_distance, float p_near_transition, float p_amount) = 0;
	virtual void camera_attributes_set_exposure(RID p_camera_attributes, float p_multiplier, float p_exposure_normalization) = 0;
	virtual void camera_attributes_set_auto_exposure(RID p_camera_attributes, bool p_enable, float p_min_sensitivity, float p_max_sensitivity, float p_speed, float p_scale) = 0;

	/* SCENARIO API */

	virtual RID scenario_create() = 0;

	virtual void scenario_set_environment(RID p_scenario, RID p_environment) = 0;
	virtual void scenario_set_fallback_environment(RID p_scenario, RID p_environment) = 0;
	virtual void scenario_set_camera_attributes(RID p_scenario, RID p_camera_attributes) = 0;
	virtual void scenario_set_compositor(RID p_scenario, RID p_compositor) = 0;

	/* INSTANCING API */

	enum InstanceType {
		INSTANCE_NONE,
		INSTANCE_MESH,
		INSTANCE_MULTIMESH,
		INSTANCE_PARTICLES,
		INSTANCE_PARTICLES_COLLISION,
		INSTANCE_LIGHT,
		INSTANCE_REFLECTION_PROBE,
		INSTANCE_DECAL,
		INSTANCE_VOXEL_GI,
		INSTANCE_LIGHTMAP,
		INSTANCE_OCCLUDER,
		INSTANCE_VISIBLITY_NOTIFIER,
		INSTANCE_FOG_VOLUME,
		INSTANCE_MAX,

		INSTANCE_GEOMETRY_MASK = (1 << INSTANCE_MESH) | (1 << INSTANCE_MULTIMESH) | (1 << INSTANCE_PARTICLES)
	};

	virtual RID instance_create2(RID p_base, RID p_scenario);

	virtual RID instance_create() = 0;

	virtual void instance_set_base(RID p_instance, RID p_base) = 0;
	virtual void instance_set_scenario(RID p_instance, RID p_scenario) = 0;
	virtual void instance_set_layer_mask(RID p_instance, uint32_t p_mask) = 0;
	virtual void instance_set_pivot_data(RID p_instance, float p_sorting_offset, bool p_use_aabb_center) = 0;
	virtual void instance_set_transform(RID p_instance, const Transform3D &p_transform) = 0;
	virtual void instance_set_interpolated(RID p_instance, bool p_interpolated) = 0;
	virtual void instance_reset_physics_interpolation(RID p_instance) = 0;
	virtual void instance_attach_object_instance_id(RID p_instance, ObjectID p_id) = 0;
	virtual void instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight) = 0;
	virtual void instance_set_surface_override_material(RID p_instance, int p_surface, RID p_material) = 0;
	virtual void instance_set_visible(RID p_instance, bool p_visible) = 0;

	virtual void instance_set_custom_aabb(RID p_instance, AABB aabb) = 0;

	virtual void instance_attach_skeleton(RID p_instance, RID p_skeleton) = 0;

	virtual void instance_set_extra_visibility_margin(RID p_instance, real_t p_margin) = 0;
	virtual void instance_set_visibility_parent(RID p_instance, RID p_parent_instance) = 0;

	virtual void instance_set_ignore_culling(RID p_instance, bool p_enabled) = 0;

	// Don't use these in a game!
	virtual Vector<ObjectID> instances_cull_aabb(const AABB &p_aabb, RID p_scenario = RID()) const = 0;
	virtual Vector<ObjectID> instances_cull_ray(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const = 0;
	virtual Vector<ObjectID> instances_cull_convex(const Vector<Plane> &p_convex, RID p_scenario = RID()) const = 0;

	PackedInt64Array _instances_cull_aabb_bind(const AABB &p_aabb, RID p_scenario = RID()) const;
	PackedInt64Array _instances_cull_ray_bind(const Vector3 &p_from, const Vector3 &p_to, RID p_scenario = RID()) const;
	PackedInt64Array _instances_cull_convex_bind(const TypedArray<Plane> &p_convex, RID p_scenario = RID()) const;

	enum InstanceFlags {
		INSTANCE_FLAG_USE_BAKED_LIGHT,
		INSTANCE_FLAG_USE_DYNAMIC_GI,
		INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE,
		INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING,
		INSTANCE_FLAG_MAX
	};

	enum ShadowCastingSetting {
		SHADOW_CASTING_SETTING_OFF,
		SHADOW_CASTING_SETTING_ON,
		SHADOW_CASTING_SETTING_DOUBLE_SIDED,
		SHADOW_CASTING_SETTING_SHADOWS_ONLY,
	};

	enum VisibilityRangeFadeMode {
		VISIBILITY_RANGE_FADE_DISABLED,
		VISIBILITY_RANGE_FADE_SELF,
		VISIBILITY_RANGE_FADE_DEPENDENCIES,
	};

	virtual void instance_geometry_set_flag(RID p_instance, InstanceFlags p_flags, bool p_enabled) = 0;
	virtual void instance_geometry_set_cast_shadows_setting(RID p_instance, ShadowCastingSetting p_shadow_casting_setting) = 0;
	virtual void instance_geometry_set_material_override(RID p_instance, RID p_material) = 0;
	virtual void instance_geometry_set_material_overlay(RID p_instance, RID p_material) = 0;
	virtual void instance_geometry_set_visibility_range(RID p_instance, float p_min, float p_max, float p_min_margin, float p_max_margin, VisibilityRangeFadeMode p_fade_mode) = 0;
	virtual void instance_geometry_set_lightmap(RID p_instance, RID p_lightmap, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice) = 0;
	virtual void instance_geometry_set_lod_bias(RID p_instance, float p_lod_bias) = 0;
	virtual void instance_geometry_set_transparency(RID p_instance, float p_transparency) = 0;

	virtual void instance_geometry_set_shader_parameter(RID p_instance, const StringName &, const Variant &p_value) = 0;
	virtual Variant instance_geometry_get_shader_parameter(RID p_instance, const StringName &) const = 0;
	virtual Variant instance_geometry_get_shader_parameter_default_value(RID p_instance, const StringName &) const = 0;
	virtual void instance_geometry_get_shader_parameter_list(RID p_instance, List<PropertyInfo> *p_parameters) const = 0;

	/* Bake 3D objects */

	enum BakeChannels {
		BAKE_CHANNEL_ALBEDO_ALPHA,
		BAKE_CHANNEL_NORMAL,
		BAKE_CHANNEL_ORM,
		BAKE_CHANNEL_EMISSION
	};

	virtual TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) = 0;

	/* CANVAS (2D) */

	virtual RID canvas_create() = 0;
	virtual void canvas_set_item_mirroring(RID p_canvas, RID p_item, const Point2 &p_mirroring) = 0;
	virtual void canvas_set_item_repeat(RID p_item, const Point2 &p_repeat_size, int p_repeat_times) = 0;
	virtual void canvas_set_modulate(RID p_canvas, const Color &p_color) = 0;
	virtual void canvas_set_parent(RID p_canvas, RID p_parent, float p_scale) = 0;

	virtual void canvas_set_disable_scale(bool p_disable) = 0;

	/* CANVAS TEXTURE */
	virtual RID canvas_texture_create() = 0;

	enum CanvasTextureChannel {
		CANVAS_TEXTURE_CHANNEL_DIFFUSE,
		CANVAS_TEXTURE_CHANNEL_NORMAL,
		CANVAS_TEXTURE_CHANNEL_SPECULAR,
	};
	virtual void canvas_texture_set_channel(RID p_canvas_texture, CanvasTextureChannel p_channel, RID p_texture) = 0;
	virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) = 0;

	// Takes effect only for new draw commands.
	virtual void canvas_texture_set_texture_filter(RID p_canvas_texture, CanvasItemTextureFilter p_filter) = 0;
	virtual void canvas_texture_set_texture_repeat(RID p_canvas_texture, CanvasItemTextureRepeat p_repeat) = 0;

	/* CANVAS ITEM */

	virtual RID canvas_item_create() = 0;
	virtual void canvas_item_set_parent(RID p_item, RID p_parent) = 0;

	virtual void canvas_item_set_default_texture_filter(RID p_item, CanvasItemTextureFilter p_filter) = 0;
	virtual void canvas_item_set_default_texture_repeat(RID p_item, CanvasItemTextureRepeat p_repeat) = 0;

	virtual void canvas_item_set_visible(RID p_item, bool p_visible) = 0;
	virtual void canvas_item_set_light_mask(RID p_item, int p_mask) = 0;

	virtual void canvas_item_set_update_when_visible(RID p_item, bool p_update) = 0;

	virtual void canvas_item_set_transform(RID p_item, const Transform2D &p_transform) = 0;
	virtual void canvas_item_set_clip(RID p_item, bool p_clip) = 0;
	virtual void canvas_item_set_distance_field_mode(RID p_item, bool p_enable) = 0;
	virtual void canvas_item_set_custom_rect(RID p_item, bool p_custom_rect, const Rect2 &p_rect = Rect2()) = 0;
	virtual void canvas_item_set_modulate(RID p_item, const Color &p_color) = 0;
	virtual void canvas_item_set_self_modulate(RID p_item, const Color &p_color) = 0;
	virtual void canvas_item_set_visibility_layer(RID p_item, uint32_t p_visibility_layer) = 0;

	virtual void canvas_item_set_draw_behind_parent(RID p_item, bool p_enable) = 0;

	enum NinePatchAxisMode {
		NINE_PATCH_STRETCH,
		NINE_PATCH_TILE,
		NINE_PATCH_TILE_FIT,
	};

	virtual void canvas_item_add_line(RID p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = -1.0, bool p_antialiased = false) = 0;
	virtual void canvas_item_add_polyline(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width = -1.0, bool p_antialiased = false) = 0;
	virtual void canvas_item_add_multiline(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width = -1.0, bool p_antialiased = false) = 0;
	virtual void canvas_item_add_rect(RID p_item, const Rect2 &p_rect, const Color &p_color, bool p_antialiased = false) = 0;
	virtual void canvas_item_add_circle(RID p_item, const Point2 &p_pos, float p_radius, const Color &p_color, bool p_antialiased = false) = 0;
	virtual void canvas_item_add_texture_rect(RID p_item, const Rect2 &p_rect, RID p_texture, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false) = 0;
	virtual void canvas_item_add_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = false) = 0;
	virtual void canvas_item_add_msdf_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), int p_outline_size = 0, float p_px_range = 1.0, float p_scale = 1.0) = 0;
	virtual void canvas_item_add_lcd_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1)) = 0;
	virtual void canvas_item_add_nine_patch(RID p_item, const Rect2 &p_rect, const Rect2 &p_source, RID p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright, NinePatchAxisMode p_x_axis_mode = NINE_PATCH_STRETCH, NinePatchAxisMode p_y_axis_mode = NINE_PATCH_STRETCH, bool p_draw_center = true, const Color &p_modulate = Color(1, 1, 1)) = 0;
	virtual void canvas_item_add_primitive(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, RID p_texture) = 0;
	virtual void canvas_item_add_polygon(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), RID p_texture = RID()) = 0;
	virtual void canvas_item_add_triangle_array(RID p_item, const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>(), RID p_texture = RID(), int p_count = -1) = 0;
	virtual void canvas_item_add_mesh(RID p_item, const RID &p_mesh, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1), RID p_texture = RID()) = 0;
	virtual void canvas_item_add_multimesh(RID p_item, RID p_mesh, RID p_texture = RID()) = 0;
	virtual void canvas_item_add_particles(RID p_item, RID p_particles, RID p_texture) = 0;
	virtual void canvas_item_add_set_transform(RID p_item, const Transform2D &p_transform) = 0;
	virtual void canvas_item_add_clip_ignore(RID p_item, bool p_ignore) = 0;
	virtual void canvas_item_add_animation_slice(RID p_item, double p_animation_length, double p_slice_begin, double p_slice_end, double p_offset) = 0;

	virtual void canvas_item_set_sort_children_by_y(RID p_item, bool p_enable) = 0;
	virtual void canvas_item_set_z_index(RID p_item, int p_z) = 0;
	virtual void canvas_item_set_z_as_relative_to_parent(RID p_item, bool p_enable) = 0;
	virtual void canvas_item_set_copy_to_backbuffer(RID p_item, bool p_enable, const Rect2 &p_rect) = 0;

	virtual void canvas_item_attach_skeleton(RID p_item, RID p_skeleton) = 0;

	virtual void canvas_item_clear(RID p_item) = 0;
	virtual void canvas_item_set_draw_index(RID p_item, int p_index) = 0;

	virtual void canvas_item_set_material(RID p_item, RID p_material) = 0;

	virtual void canvas_item_set_use_parent_material(RID p_item, bool p_enable) = 0;

	virtual void canvas_item_set_visibility_notifier(RID p_item, bool p_enable, const Rect2 &p_area, const Callable &p_enter_callbable, const Callable &p_exit_callable) = 0;

	enum CanvasGroupMode {
		CANVAS_GROUP_MODE_DISABLED,
		CANVAS_GROUP_MODE_CLIP_ONLY,
		CANVAS_GROUP_MODE_CLIP_AND_DRAW,
		CANVAS_GROUP_MODE_TRANSPARENT,
	};

	virtual void canvas_item_set_canvas_group_mode(RID p_item, CanvasGroupMode p_mode, float p_clear_margin = 5.0, bool p_fit_empty = false, float p_fit_margin = 0.0, bool p_blur_mipmaps = false) = 0;

	virtual void canvas_item_set_debug_redraw(bool p_enabled) = 0;
	virtual bool canvas_item_get_debug_redraw() const = 0;

	virtual void canvas_item_set_interpolated(RID p_item, bool p_interpolated) = 0;
	virtual void canvas_item_reset_physics_interpolation(RID p_item) = 0;
	virtual void canvas_item_transform_physics_interpolation(RID p_item, const Transform2D &p_transform) = 0;

	/* CANVAS LIGHT */
	virtual RID canvas_light_create() = 0;

	enum CanvasLightMode {
		CANVAS_LIGHT_MODE_POINT,
		CANVAS_LIGHT_MODE_DIRECTIONAL,
	};

	virtual void canvas_light_set_mode(RID p_light, CanvasLightMode p_mode) = 0;

	virtual void canvas_light_attach_to_canvas(RID p_light, RID p_canvas) = 0;
	virtual void canvas_light_set_enabled(RID p_light, bool p_enabled) = 0;
	virtual void canvas_light_set_transform(RID p_light, const Transform2D &p_transform) = 0;
	virtual void canvas_light_set_color(RID p_light, const Color &p_color) = 0;
	virtual void canvas_light_set_height(RID p_light, float p_height) = 0;
	virtual void canvas_light_set_energy(RID p_light, float p_energy) = 0;
	virtual void canvas_light_set_z_range(RID p_light, int p_min_z, int p_max_z) = 0;
	virtual void canvas_light_set_layer_range(RID p_light, int p_min_layer, int p_max_layer) = 0;
	virtual void canvas_light_set_item_cull_mask(RID p_light, int p_mask) = 0;
	virtual void canvas_light_set_item_shadow_cull_mask(RID p_light, int p_mask) = 0;

	virtual void canvas_light_set_directional_distance(RID p_light, float p_distance) = 0;

	virtual void canvas_light_set_texture_scale(RID p_light, float p_scale) = 0;
	virtual void canvas_light_set_texture(RID p_light, RID p_texture) = 0;
	virtual void canvas_light_set_texture_offset(RID p_light, const Vector2 &p_offset) = 0;

	enum CanvasLightBlendMode {
		CANVAS_LIGHT_BLEND_MODE_ADD,
		CANVAS_LIGHT_BLEND_MODE_SUB,
		CANVAS_LIGHT_BLEND_MODE_MIX,
	};

	virtual void canvas_light_set_blend_mode(RID p_light, CanvasLightBlendMode p_mode) = 0;

	enum CanvasLightShadowFilter {
		CANVAS_LIGHT_FILTER_NONE,
		CANVAS_LIGHT_FILTER_PCF5,
		CANVAS_LIGHT_FILTER_PCF13,
		CANVAS_LIGHT_FILTER_MAX
	};

	virtual void canvas_light_set_shadow_enabled(RID p_light, bool p_enabled) = 0;
	virtual void canvas_light_set_shadow_filter(RID p_light, CanvasLightShadowFilter p_filter) = 0;
	virtual void canvas_light_set_shadow_color(RID p_light, const Color &p_color) = 0;
	virtual void canvas_light_set_shadow_smooth(RID p_light, float p_smooth) = 0;

	virtual void canvas_light_set_interpolated(RID p_light, bool p_interpolated) = 0;
	virtual void canvas_light_reset_physics_interpolation(RID p_light) = 0;
	virtual void canvas_light_transform_physics_interpolation(RID p_light, const Transform2D &p_transform) = 0;

	/* CANVAS LIGHT OCCLUDER */

	virtual RID canvas_light_occluder_create() = 0;
	virtual void canvas_light_occluder_attach_to_canvas(RID p_occluder, RID p_canvas) = 0;
	virtual void canvas_light_occluder_set_enabled(RID p_occluder, bool p_enabled) = 0;
	virtual void canvas_light_occluder_set_polygon(RID p_occluder, RID p_polygon) = 0;
	virtual void canvas_light_occluder_set_as_sdf_collision(RID p_occluder, bool p_enable) = 0;
	virtual void canvas_light_occluder_set_transform(RID p_occluder, const Transform2D &p_xform) = 0;
	virtual void canvas_light_occluder_set_light_mask(RID p_occluder, int p_mask) = 0;

	virtual void canvas_light_occluder_set_interpolated(RID p_occluder, bool p_interpolated) = 0;
	virtual void canvas_light_occluder_reset_physics_interpolation(RID p_occluder) = 0;
	virtual void canvas_light_occluder_transform_physics_interpolation(RID p_occluder, const Transform2D &p_transform) = 0;

	/* CANVAS LIGHT OCCLUDER POLYGON */

	virtual RID canvas_occluder_polygon_create() = 0;
	virtual void canvas_occluder_polygon_set_shape(RID p_occluder_polygon, const Vector<Vector2> &p_shape, bool p_closed) = 0;

	enum CanvasOccluderPolygonCullMode {
		CANVAS_OCCLUDER_POLYGON_CULL_DISABLED,
		CANVAS_OCCLUDER_POLYGON_CULL_CLOCKWISE,
		CANVAS_OCCLUDER_POLYGON_CULL_COUNTER_CLOCKWISE,
	};

	virtual void canvas_occluder_polygon_set_cull_mode(RID p_occluder_polygon, CanvasOccluderPolygonCullMode p_mode) = 0;

	virtual void canvas_set_shadow_texture_size(int p_size) = 0;

	Rect2 debug_canvas_item_get_rect(RID p_item);
	virtual Rect2 _debug_canvas_item_get_rect(RID p_item) = 0;

	/* GLOBAL SHADER UNIFORMS */

	enum GlobalShaderParameterType {
		GLOBAL_VAR_TYPE_BOOL,
		GLOBAL_VAR_TYPE_BVEC2,
		GLOBAL_VAR_TYPE_BVEC3,
		GLOBAL_VAR_TYPE_BVEC4,
		GLOBAL_VAR_TYPE_INT,
		GLOBAL_VAR_TYPE_IVEC2,
		GLOBAL_VAR_TYPE_IVEC3,
		GLOBAL_VAR_TYPE_IVEC4,
		GLOBAL_VAR_TYPE_RECT2I,
		GLOBAL_VAR_TYPE_UINT,
		GLOBAL_VAR_TYPE_UVEC2,
		GLOBAL_VAR_TYPE_UVEC3,
		GLOBAL_VAR_TYPE_UVEC4,
		GLOBAL_VAR_TYPE_FLOAT,
		GLOBAL_VAR_TYPE_VEC2,
		GLOBAL_VAR_TYPE_VEC3,
		GLOBAL_VAR_TYPE_VEC4,
		GLOBAL_VAR_TYPE_COLOR,
		GLOBAL_VAR_TYPE_RECT2,
		GLOBAL_VAR_TYPE_MAT2,
		GLOBAL_VAR_TYPE_MAT3,
		GLOBAL_VAR_TYPE_MAT4,
		GLOBAL_VAR_TYPE_TRANSFORM_2D,
		GLOBAL_VAR_TYPE_TRANSFORM,
		GLOBAL_VAR_TYPE_SAMPLER2D,
		GLOBAL_VAR_TYPE_SAMPLER2DARRAY,
		GLOBAL_VAR_TYPE_SAMPLER3D,
		GLOBAL_VAR_TYPE_SAMPLERCUBE,
		GLOBAL_VAR_TYPE_SAMPLEREXT,
		GLOBAL_VAR_TYPE_MAX
	};

	virtual void global_shader_parameter_add(const StringName &p_name, GlobalShaderParameterType p_type, const Variant &p_value) = 0;
	virtual void global_shader_parameter_remove(const StringName &p_name) = 0;
	virtual Vector<StringName> global_shader_parameter_get_list() const = 0;

	virtual void global_shader_parameter_set(const StringName &p_name, const Variant &p_value) = 0;
	virtual void global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) = 0;

	virtual Variant global_shader_parameter_get(const StringName &p_name) const = 0;
	virtual GlobalShaderParameterType global_shader_parameter_get_type(const StringName &p_name) const = 0;

	virtual void global_shader_parameters_load_settings(bool p_load_textures) = 0;
	virtual void global_shader_parameters_clear() = 0;

	static int global_shader_uniform_type_get_shader_datatype(GlobalShaderParameterType p_type);

	/* FREE */

	virtual void free(RID p_rid) = 0; // Free RIDs associated with the rendering server.

	/* INTERPOLATION */

	virtual void set_physics_interpolation_enabled(bool p_enabled) = 0;

	/* EVENT QUEUING */

	virtual void request_frame_drawn_callback(const Callable &p_callable) = 0;

	virtual void draw(bool p_swap_buffers = true, double frame_step = 0.0) = 0;
	virtual void sync() = 0;
	virtual bool has_changed() const = 0;
	virtual void init();
	virtual void finish() = 0;
	virtual void tick() = 0;
	virtual void pre_draw(bool p_will_draw) = 0;

	/* STATUS INFORMATION */

	enum RenderingInfo {
		RENDERING_INFO_TOTAL_OBJECTS_IN_FRAME,
		RENDERING_INFO_TOTAL_PRIMITIVES_IN_FRAME,
		RENDERING_INFO_TOTAL_DRAW_CALLS_IN_FRAME,
		RENDERING_INFO_TEXTURE_MEM_USED,
		RENDERING_INFO_BUFFER_MEM_USED,
		RENDERING_INFO_VIDEO_MEM_USED,
		RENDERING_INFO_PIPELINE_COMPILATIONS_CANVAS,
		RENDERING_INFO_PIPELINE_COMPILATIONS_MESH,
		RENDERING_INFO_PIPELINE_COMPILATIONS_SURFACE,
		RENDERING_INFO_PIPELINE_COMPILATIONS_DRAW,
		RENDERING_INFO_PIPELINE_COMPILATIONS_SPECIALIZATION,
		RENDERING_INFO_MAX
	};

	virtual uint64_t get_rendering_info(RenderingInfo p_info) = 0;
	virtual String get_video_adapter_name() const = 0;
	virtual String get_video_adapter_vendor() const = 0;
	virtual RenderingDevice::DeviceType get_video_adapter_type() const = 0;
	virtual String get_video_adapter_api_version() const = 0;

	struct FrameProfileArea {
		String name;
		double gpu_msec;
		double cpu_msec;
	};

	virtual void set_frame_profiling_enabled(bool p_enable) = 0;
	virtual Vector<FrameProfileArea> get_frame_profile() = 0;
	virtual uint64_t get_frame_profile_frame() = 0;

	virtual double get_frame_setup_time_cpu() const = 0;

	virtual void gi_set_use_half_resolution(bool p_enable) = 0;

	/* TESTING */

	virtual RID get_test_cube() = 0;

	virtual RID get_test_texture();
	virtual RID get_white_texture();

	virtual void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) = 0;

	virtual RID make_sphere_mesh(int p_lats, int p_lons, real_t p_radius);

	virtual void mesh_add_surface_from_mesh_data(RID p_mesh, const Geometry3D::MeshData &p_mesh_data);
	virtual void mesh_add_surface_from_planes(RID p_mesh, const Vector<Plane> &p_planes);

	virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) = 0;
	virtual Color get_default_clear_color() = 0;
	virtual void set_default_clear_color(const Color &p_color) = 0;

#ifndef DISABLE_DEPRECATED
	// Never actually used, should be removed when we can break compatibility.
	enum Features {
		FEATURE_SHADERS,
		FEATURE_MULTITHREADED,
	};
	virtual bool has_feature(Features p_feature) const = 0;
#endif
	virtual bool has_os_feature(const String &p_feature) const = 0;

	virtual void set_debug_generate_wireframes(bool p_generate) = 0;

	virtual void call_set_vsync_mode(DisplayServer::VSyncMode p_mode, DisplayServer::WindowID p_window) = 0;

	virtual bool is_low_end() const = 0;

	virtual void set_print_gpu_profile(bool p_enable) = 0;

	virtual Size2i get_maximum_viewport_size() const = 0;

	RenderingDevice *get_rendering_device() const;
	RenderingDevice *create_local_rendering_device() const;

	bool is_render_loop_enabled() const;
	void set_render_loop_enabled(bool p_enabled);

	virtual bool is_on_render_thread() = 0;
	virtual void call_on_render_thread(const Callable &p_callable) = 0;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	RenderingServer();
	virtual ~RenderingServer();

#ifdef TOOLS_ENABLED
	typedef void (*SurfaceUpgradeCallback)();
	void set_surface_upgrade_callback(SurfaceUpgradeCallback p_callback);
	void set_warn_on_surface_upgrade(bool p_warn);
#endif

#ifndef DISABLE_DEPRECATED
	void fix_surface_compatibility(SurfaceData &p_surface, const String &p_path = "");
#endif

private:
	// Binder helpers
	RID _texture_2d_layered_create(const TypedArray<Image> &p_layers, TextureLayeredType p_layered_type);
	RID _texture_3d_create(Image::Format p_format, int p_width, int p_height, int p_depth, bool p_mipmaps, const TypedArray<Image> &p_data);
	void _texture_3d_update(RID p_texture, const TypedArray<Image> &p_data);
	TypedArray<Image> _texture_3d_get(RID p_texture) const;
	TypedArray<Dictionary> _shader_get_shader_parameter_list(RID p_shader) const;
	RID _mesh_create_from_surfaces(const TypedArray<Dictionary> &p_surfaces, int p_blend_shape_count);
	void _mesh_add_surface(RID p_mesh, const Dictionary &p_surface);
	Dictionary _mesh_get_surface(RID p_mesh, int p_idx);
	TypedArray<Dictionary> _instance_geometry_get_shader_parameter_list(RID p_instance) const;
	TypedArray<Image> _bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size);
	void _particles_set_trail_bind_poses(RID p_particles, const TypedArray<Transform3D> &p_bind_poses);
#ifdef TOOLS_ENABLED
	SurfaceUpgradeCallback surface_upgrade_callback = nullptr;
	bool warn_on_surface_upgrade = true;
#endif
};

// Make variant understand the enums.
VARIANT_ENUM_CAST(RenderingServer::TextureType);
VARIANT_ENUM_CAST(RenderingServer::TextureLayeredType);
VARIANT_ENUM_CAST(RenderingServer::CubeMapLayer);
VARIANT_ENUM_CAST(RenderingServer::PipelineSource);
VARIANT_ENUM_CAST(RenderingServer::ShaderMode);
VARIANT_ENUM_CAST(RenderingServer::ArrayType);
VARIANT_BITFIELD_CAST(RenderingServer::ArrayFormat);
VARIANT_ENUM_CAST(RenderingServer::ArrayCustomFormat);
VARIANT_ENUM_CAST(RenderingServer::PrimitiveType);
VARIANT_ENUM_CAST(RenderingServer::BlendShapeMode);
VARIANT_ENUM_CAST(RenderingServer::MultimeshTransformFormat);
VARIANT_ENUM_CAST(RenderingServer::MultimeshPhysicsInterpolationQuality);
VARIANT_ENUM_CAST(RenderingServer::LightType);
VARIANT_ENUM_CAST(RenderingServer::LightParam);
VARIANT_ENUM_CAST(RenderingServer::LightBakeMode);
VARIANT_ENUM_CAST(RenderingServer::LightOmniShadowMode);
VARIANT_ENUM_CAST(RenderingServer::LightDirectionalShadowMode);
VARIANT_ENUM_CAST(RenderingServer::LightDirectionalSkyMode);
VARIANT_ENUM_CAST(RenderingServer::LightProjectorFilter);
VARIANT_ENUM_CAST(RenderingServer::ReflectionProbeUpdateMode);
VARIANT_ENUM_CAST(RenderingServer::ReflectionProbeAmbientMode);
VARIANT_ENUM_CAST(RenderingServer::VoxelGIQuality);
VARIANT_ENUM_CAST(RenderingServer::DecalTexture);
VARIANT_ENUM_CAST(RenderingServer::DecalFilter);
VARIANT_ENUM_CAST(RenderingServer::ParticlesMode);
VARIANT_ENUM_CAST(RenderingServer::ParticlesTransformAlign);
VARIANT_ENUM_CAST(RenderingServer::ParticlesDrawOrder);
VARIANT_ENUM_CAST(RenderingServer::ParticlesEmitFlags);
VARIANT_ENUM_CAST(RenderingServer::ParticlesCollisionType);
VARIANT_ENUM_CAST(RenderingServer::ParticlesCollisionHeightfieldResolution);
VARIANT_ENUM_CAST(RenderingServer::FogVolumeShape);
VARIANT_ENUM_CAST(RenderingServer::ViewportScaling3DMode);
VARIANT_ENUM_CAST(RenderingServer::ViewportUpdateMode);
VARIANT_ENUM_CAST(RenderingServer::ViewportClearMode);
VARIANT_ENUM_CAST(RenderingServer::ViewportEnvironmentMode);
VARIANT_ENUM_CAST(RenderingServer::ViewportMSAA);
VARIANT_ENUM_CAST(RenderingServer::ViewportScreenSpaceAA);
VARIANT_ENUM_CAST(RenderingServer::ViewportRenderInfo);
VARIANT_ENUM_CAST(RenderingServer::ViewportRenderInfoType);
VARIANT_ENUM_CAST(RenderingServer::ViewportDebugDraw);
VARIANT_ENUM_CAST(RenderingServer::ViewportOcclusionCullingBuildQuality);
VARIANT_ENUM_CAST(RenderingServer::ViewportSDFOversize);
VARIANT_ENUM_CAST(RenderingServer::ViewportSDFScale);
VARIANT_ENUM_CAST(RenderingServer::ViewportVRSMode);
VARIANT_ENUM_CAST(RenderingServer::ViewportVRSUpdateMode);
VARIANT_ENUM_CAST(RenderingServer::SkyMode);
VARIANT_ENUM_CAST(RenderingServer::CompositorEffectCallbackType);
VARIANT_ENUM_CAST(RenderingServer::CompositorEffectFlags);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentBG);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentAmbientSource);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentReflectionSource);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentGlowBlendMode);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentFogMode);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentToneMapper);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSSRRoughnessQuality);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSSAOQuality);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSSILQuality);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSDFGIFramesToConverge);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSDFGIRayCount);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSDFGIFramesToUpdateLight);
VARIANT_ENUM_CAST(RenderingServer::EnvironmentSDFGIYScale);
VARIANT_ENUM_CAST(RenderingServer::SubSurfaceScatteringQuality);
VARIANT_ENUM_CAST(RenderingServer::DOFBlurQuality);
VARIANT_ENUM_CAST(RenderingServer::DOFBokehShape);
VARIANT_ENUM_CAST(RenderingServer::ShadowQuality);
VARIANT_ENUM_CAST(RenderingServer::InstanceType);
VARIANT_ENUM_CAST(RenderingServer::InstanceFlags);
VARIANT_ENUM_CAST(RenderingServer::ShadowCastingSetting);
VARIANT_ENUM_CAST(RenderingServer::VisibilityRangeFadeMode);
VARIANT_ENUM_CAST(RenderingServer::NinePatchAxisMode);
VARIANT_ENUM_CAST(RenderingServer::CanvasItemTextureFilter);
VARIANT_ENUM_CAST(RenderingServer::CanvasItemTextureRepeat);
VARIANT_ENUM_CAST(RenderingServer::CanvasGroupMode);
VARIANT_ENUM_CAST(RenderingServer::CanvasLightMode);
VARIANT_ENUM_CAST(RenderingServer::CanvasLightBlendMode);
VARIANT_ENUM_CAST(RenderingServer::CanvasLightShadowFilter);
VARIANT_ENUM_CAST(RenderingServer::CanvasOccluderPolygonCullMode);
VARIANT_ENUM_CAST(RenderingServer::GlobalShaderParameterType);
VARIANT_ENUM_CAST(RenderingServer::RenderingInfo);
VARIANT_ENUM_CAST(RenderingServer::CanvasTextureChannel);
VARIANT_ENUM_CAST(RenderingServer::BakeChannels);

#ifndef DISABLE_DEPRECATED
VARIANT_ENUM_CAST(RenderingServer::Features);
#endif

// Alias to make it easier to use.
#define RS RenderingServer

#endif // RENDERING_SERVER_H
