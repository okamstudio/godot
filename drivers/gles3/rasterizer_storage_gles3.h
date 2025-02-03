/**************************************************************************/
/*  rasterizer_storage_gles3.h                                            */
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

#ifndef RASTERIZER_STORAGE_GLES3_H
#define RASTERIZER_STORAGE_GLES3_H

#include "core/bitfield_dynamic.h"
#include "core/self_list.h"
#include "drivers/gles_common/rasterizer_asserts.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual/shader_language.h"
#include "shader_cache_gles3.h"
#include "shader_compiler_gles3.h"
#include "shader_gles3.h"

#include "shaders/blend_shape.glsl.gen.h"
#include "shaders/canvas.glsl.gen.h"
#include "shaders/copy.glsl.gen.h"
#include "shaders/cubemap_filter.glsl.gen.h"
#include "shaders/particles.glsl.gen.h"

template <class K>
class ThreadedCallableQueue;
class RasterizerCanvasGLES3;
class RasterizerSceneGLES3;

#define _TEXTURE_SRGB_DECODE_EXT 0x8A48
#define _DECODE_EXT 0x8A49
#define _SKIP_DECODE_EXT 0x8A4A

void glTexStorage2DCustom(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type);

#define WRAPPED_GL_ACTIVE_TEXTURE storage->gl_wrapper.gl_active_texture

class RasterizerStorageGLES3 : public RasterizerStorage {
public:
	GLuint aabb_index_id;

	RasterizerCanvasGLES3 *canvas;
	RasterizerSceneGLES3 *scene;
	static GLuint system_fbo; //on some devices, such as apple, screen is rendered to yet another fbo.

	enum RenderArchitecture {
		RENDER_ARCH_MOBILE,
		RENDER_ARCH_DESKTOP,
	};

	struct Config {
		bool shrink_textures_x2;
		bool use_fast_texture_filter;
		bool use_anisotropic_filter;
		bool use_lightmap_filter_bicubic;
		bool use_physical_light_attenuation;

		bool s3tc_supported;
		bool latc_supported;
		bool rgtc_supported;
		bool bptc_supported;
		bool etc_supported;
		bool etc2_supported;
		bool pvrtc_supported;

		bool srgb_decode_supported;

		bool support_npot_repeat_mipmap;
		bool texture_float_linear_supported;
		bool framebuffer_float_supported;
		bool framebuffer_half_float_supported;

		bool use_rgba_2d_shadows;

		float anisotropic_level;

		int max_texture_image_units;
		static const int32_t max_desired_texture_image_units = 64;
		int max_texture_size;
		int max_cubemap_texture_size;

		bool generate_wireframes;

		bool use_texture_array_environment;

		Set<String> extensions;

		bool keep_original_textures;

		bool use_depth_prepass;

		bool use_occlusion_queries;

		bool force_vertex_shading;

		// in some cases the legacy render didn't orphan. We will mark these
		// so the user can switch orphaning off for them.
		bool should_orphan;

		bool program_binary_supported;
		bool parallel_shader_compile_supported;
		bool async_compilation_enabled;
		bool shader_cache_enabled;
	} config;

	enum class OcclusionQueryPrevAction {
		NoAction,
		Rendered,
		Culled
	};

	struct OcclusionQueryData {
		OcclusionQueryPrevAction prev_action = OcclusionQueryPrevAction::NoAction;
		uint64_t prev_frame = 0;
		GLuint query;
		uint32_t surface;
	};
	Map<uint32_t, Map<uint32_t, OcclusionQueryData>> occlusion_queries_for_viewport;

	mutable struct Shaders {
		CopyShaderGLES3 copy;

		ShaderCompilerGLES3 compiler;
		ShaderCacheGLES3 *cache;
		ThreadedCallableQueue<GLuint> *cache_write_queue;
		ThreadedCallableQueue<GLuint> *compile_queue;

		CubemapFilterShaderGLES3 cubemap_filter;

		BlendShapeShaderGLES3 blend_shapes;

		ParticlesShaderGLES3 particles;

		ShaderCompilerGLES3::IdentifierActions actions_canvas;
		ShaderCompilerGLES3::IdentifierActions actions_scene;
		ShaderCompilerGLES3::IdentifierActions actions_particles;
	} shaders;

	struct Resources {
		GLuint white_tex;
		GLuint black_tex;
		GLuint transparent_tex;
		GLuint normal_tex;
		GLuint aniso_tex;
		GLuint depth_tex;

		GLuint white_tex_3d;
		GLuint white_tex_array;

		GLuint quadie;
		GLuint quadie_array;

		GLuint transform_feedback_buffers[2];
		GLuint transform_feedback_array;

	} resources;

	struct Info {
		uint64_t texture_mem;
		uint64_t vertex_mem;

		struct Render {
			uint32_t object_count;
			uint32_t draw_call_count;
			uint32_t material_switch_count;
			uint32_t surface_switch_count;
			uint32_t shader_rebind_count;
			uint32_t shader_compiles_started_count;
			uint32_t shader_compiles_in_progress_count;
			uint32_t vertices_count;
			uint32_t _2d_item_count;
			uint32_t _2d_draw_call_count;

			void reset() {
				object_count = 0;
				draw_call_count = 0;
				material_switch_count = 0;
				surface_switch_count = 0;
				shader_rebind_count = 0;
				shader_compiles_started_count = 0;
				shader_compiles_in_progress_count = 0;
				vertices_count = 0;
				_2d_item_count = 0;
				_2d_draw_call_count = 0;
			}
		} render, render_final, snap;

		Info() {
			texture_mem = 0;
			vertex_mem = 0;
			render.reset();
			render_final.reset();
		}

	} info;

	/////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////DATA///////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////

	struct Instantiable : public RID_Data {
		SelfList<RasterizerScene::InstanceBase>::List instance_list;

		_FORCE_INLINE_ void instance_change_notify(bool p_aabb, bool p_materials) {
			SelfList<RasterizerScene::InstanceBase> *instances = instance_list.first();
			while (instances) {
				instances->self()->base_changed(p_aabb, p_materials);
				instances = instances->next();
			}
		}

		_FORCE_INLINE_ void instance_remove_deps() {
			SelfList<RasterizerScene::InstanceBase> *instances = instance_list.first();
			while (instances) {
				SelfList<RasterizerScene::InstanceBase> *next = instances->next();
				instances->self()->base_removed();
				instances = next;
			}
		}

		Instantiable() {}
		virtual ~Instantiable() {
		}
	};

	struct GeometryOwner : public Instantiable {
		virtual ~GeometryOwner() {}
	};
	struct Geometry : Instantiable {
		enum Type {
			GEOMETRY_INVALID,
			GEOMETRY_SURFACE,
			GEOMETRY_IMMEDIATE,
			GEOMETRY_MULTISURFACE,
		};

		Type type;
		RID material;
		uint64_t last_pass;
		uint32_t index;

		virtual void material_changed_notify() {}

		Geometry() {
			last_pass = 0;
			index = 0;
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////API////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////

	/* TEXTURE API */

	struct RenderTarget;

	struct Texture : public RID_Data {
		Texture *proxy;
		Set<Texture *> proxy_owners;

		String path;
		uint32_t flags;
		int width, height, depth;
		int alloc_width, alloc_height, alloc_depth;
		Image::Format format;
		VS::TextureType type;

		GLenum target;
		GLenum gl_format_cache;
		GLenum gl_internal_format_cache;
		GLenum gl_type_cache;
		int data_size; //original data size, useful for retrieving back
		bool compressed;
		bool srgb;
		int total_data_size;
		bool ignore_mipmaps;

		int mipmaps;

		bool is_npot_repeat_mipmap;

		bool active;
		GLuint tex_id;

		bool using_srgb;
		bool redraw_if_visible;

		uint16_t stored_cube_sides;

		RenderTarget *render_target;

		Vector<Ref<Image>> images;

		VisualServer::TextureDetectCallback detect_3d;
		void *detect_3d_ud;

		VisualServer::TextureDetectCallback detect_srgb;
		void *detect_srgb_ud;

		VisualServer::TextureDetectCallback detect_normal;
		void *detect_normal_ud;

		Texture() :
				proxy(nullptr),
				flags(0),
				width(0),
				height(0),
				format(Image::FORMAT_L8),
				type(VS::TEXTURE_TYPE_2D),
				target(GL_TEXTURE_2D),
				data_size(0),
				compressed(false),
				srgb(false),
				total_data_size(0),
				ignore_mipmaps(false),
				mipmaps(0),
				active(false),
				tex_id(0),
				using_srgb(false),
				redraw_if_visible(false),
				stored_cube_sides(0),
				render_target(nullptr),
				detect_3d(nullptr),
				detect_3d_ud(nullptr),
				detect_srgb(nullptr),
				detect_srgb_ud(nullptr),
				detect_normal(nullptr),
				detect_normal_ud(nullptr) {
		}

		_ALWAYS_INLINE_ Texture *get_ptr() {
			if (proxy) {
				return proxy; //->get_ptr(); only one level of indirection, else not inlining possible.
			} else {
				return this;
			}
		}

		~Texture() {
			if (tex_id != 0) {
				glDeleteTextures(1, &tex_id);
			}

			for (Set<Texture *>::Element *E = proxy_owners.front(); E; E = E->next()) {
				E->get()->proxy = nullptr;
			}

			if (proxy) {
				proxy->proxy_owners.erase(this);
			}
		}
	};

	mutable RID_Owner<Texture> texture_owner;

	Ref<Image> _get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, uint32_t p_flags, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool &r_srgb, bool p_force_decompress) const;

	virtual RID texture_create();
	virtual void texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VS::TextureType p_type, uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT);
	virtual void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0);
	virtual void texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int p_dst_mip, int p_layer = 0);
	virtual Ref<Image> texture_get_data(RID p_texture, int p_layer = 0) const;
	virtual void texture_set_flags(RID p_texture, uint32_t p_flags);
	virtual uint32_t texture_get_flags(RID p_texture) const;
	virtual Image::Format texture_get_format(RID p_texture) const;
	virtual VS::TextureType texture_get_type(RID p_texture) const;
	virtual uint32_t texture_get_texid(RID p_texture) const;
	virtual uint32_t texture_get_width(RID p_texture) const;
	virtual uint32_t texture_get_height(RID p_texture) const;
	virtual uint32_t texture_get_depth(RID p_texture) const;
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth);
	virtual void texture_bind(RID p_texture, uint32_t p_texture_no);

	virtual void texture_set_path(RID p_texture, const String &p_path);
	virtual String texture_get_path(RID p_texture) const;

	virtual void texture_set_shrink_all_x2_on_set_data(bool p_enable);

	virtual void texture_debug_usage(List<VS::TextureInfo> *r_info);

	virtual RID texture_create_radiance_cubemap(RID p_source, int p_resolution = -1) const;

	virtual void textures_keep_original(bool p_enable);

	virtual void texture_set_detect_3d_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);
	virtual void texture_set_detect_srgb_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);
	virtual void texture_set_detect_normal_callback(RID p_texture, VisualServer::TextureDetectCallback p_callback, void *p_userdata);

	virtual void texture_set_proxy(RID p_texture, RID p_proxy);
	virtual Size2 texture_size_with_proxy(RID p_texture) const;

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable);

	/* SKY API */

	struct Sky : public RID_Data {
		RID panorama;
		GLuint radiance;
		GLuint irradiance;
		int radiance_size;
	};

	mutable RID_Owner<Sky> sky_owner;

	virtual RID sky_create();
	virtual void sky_set_texture(RID p_sky, RID p_panorama, int p_radiance_size);

	/* SHADER API */

	struct Material;

	struct Shader : public RID_Data {
		RID self;

		VS::ShaderMode mode;
		ShaderGLES3 *shader;
		String code;
		SelfList<Material>::List materials;

		Map<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;
		Vector<uint32_t> ubo_offsets;
		uint32_t ubo_size;

		uint32_t texture_count;

		uint32_t custom_code_id;
		uint32_t version;

		SelfList<Shader> dirty_list;

		Map<StringName, RID> default_textures;

		Vector<ShaderLanguage::DataType> texture_types;
		Vector<ShaderLanguage::ShaderNode::Uniform::Hint> texture_hints;

		bool valid;

		String path;

		struct CanvasItem {
			enum BlendMode {
				BLEND_MODE_MIX,
				BLEND_MODE_ADD,
				BLEND_MODE_SUB,
				BLEND_MODE_MUL,
				BLEND_MODE_PMALPHA,
				BLEND_MODE_DISABLED,
			};

			int blend_mode;

			enum LightMode {
				LIGHT_MODE_NORMAL,
				LIGHT_MODE_UNSHADED,
				LIGHT_MODE_LIGHT_ONLY
			};

			int light_mode;

			// these flags are specifically for batching
			// some of the logic is thus in rasterizer_storage.cpp
			// we could alternatively set bitflags for each 'uses' and test on the fly
			// defined in RasterizerStorageCommon::BatchFlags
			unsigned int batch_flags;

			bool uses_screen_texture;
			bool uses_screen_uv;
			bool uses_time;
			bool uses_modulate;
			bool uses_color;
			bool uses_vertex;

			// all these should disable item joining if used in a custom shader
			bool uses_world_matrix;
			bool uses_extra_matrix;
			bool uses_projection_matrix;
			bool uses_instance_custom;

		} canvas_item;

		struct Spatial {
			enum BlendMode {
				BLEND_MODE_MIX,
				BLEND_MODE_ADD,
				BLEND_MODE_SUB,
				BLEND_MODE_MUL,
			};

			int blend_mode;

			enum DepthDrawMode {
				DEPTH_DRAW_OPAQUE,
				DEPTH_DRAW_ALWAYS,
				DEPTH_DRAW_NEVER,
				DEPTH_DRAW_ALPHA_PREPASS,
			};

			int depth_draw_mode;

			enum CullMode {
				CULL_MODE_FRONT,
				CULL_MODE_BACK,
				CULL_MODE_DISABLED,
			};

			int cull_mode;

			bool uses_alpha;
			bool uses_alpha_scissor;
			bool unshaded;
			bool no_depth_test;
			bool uses_vertex;
			bool uses_discard;
			bool uses_sss;
			bool uses_screen_texture;
			bool uses_depth_texture;
			bool uses_time;
			bool uses_tangent;
			bool uses_ensure_correct_normals;
			bool writes_modelview_or_projection;
			bool uses_vertex_lighting;
			bool uses_world_coordinates;

		} spatial;

		struct Particles {
		} particles;

		bool uses_vertex_time;
		bool uses_fragment_time;

		Shader() :
				dirty_list(this) {
			shader = nullptr;
			ubo_size = 0;
			valid = false;
			custom_code_id = 0;
			version = 1;
		}
	};

	mutable SelfList<Shader>::List _shader_dirty_list;
	void _shader_make_dirty(Shader *p_shader);

	mutable RID_Owner<Shader> shader_owner;

	virtual RID shader_create();

	virtual void shader_set_code(RID p_shader, const String &p_code);
	virtual String shader_get_code(RID p_shader) const;
	virtual void shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const;

	virtual void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture);
	virtual RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const;

	virtual void shader_add_custom_define(RID p_shader, const String &p_define);
	virtual void shader_get_custom_defines(RID p_shader, Vector<String> *p_defines) const;
	virtual void shader_remove_custom_define(RID p_shader, const String &p_define);

	virtual void set_shader_async_hidden_forbidden(bool p_forbidden);
	virtual bool is_shader_async_hidden_forbidden();

	void _update_shader(Shader *p_shader) const;

	void update_dirty_shaders();

	/* COMMON MATERIAL API */

	struct Material : public RID_Data {
		Shader *shader;
		GLuint ubo_id;
		uint32_t ubo_size;
		Map<StringName, Variant> params;
		SelfList<Material> list;
		SelfList<Material> dirty_list;
		Vector<bool> texture_is_3d;
		Vector<RID> textures;
		float line_width;
		int render_priority;

		RID next_pass;

		uint32_t index;
		uint64_t last_pass;

		Map<Geometry *, int> geometry_owners;
		Map<RasterizerScene::InstanceBase *, int> instance_owners;

		bool can_cast_shadow_cache;
		bool is_animated_cache;

		Material() :
				shader(nullptr),
				ubo_id(0),
				ubo_size(0),
				list(this),
				dirty_list(this),
				line_width(1.0),
				render_priority(0),
				last_pass(0),
				can_cast_shadow_cache(false),
				is_animated_cache(false) {
		}
	};

	mutable SelfList<Material>::List _material_dirty_list;
	void _material_make_dirty(Material *p_material) const;
	void _material_add_geometry(RID p_material, Geometry *p_geometry);
	void _material_remove_geometry(RID p_material, Geometry *p_geometry);

	mutable RID_Owner<Material> material_owner;

	virtual RID material_create();

	virtual void material_set_shader(RID p_material, RID p_shader);
	virtual RID material_get_shader(RID p_material) const;

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value);
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const;
	virtual Variant material_get_param_default(RID p_material, const StringName &p_param) const;

	virtual void material_set_line_width(RID p_material, float p_width);
	virtual void material_set_next_pass(RID p_material, RID p_next_material);

	virtual bool material_is_animated(RID p_material);
	virtual bool material_casts_shadows(RID p_material);
	virtual bool material_uses_tangents(RID p_material);
	virtual bool material_uses_ensure_correct_normals(RID p_material);

	virtual void material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);
	virtual void material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);

	virtual void material_set_render_priority(RID p_material, int priority);

	void _update_material(Material *material);

	void update_dirty_materials();

	/* MESH API */

	struct Mesh;
	struct Surface : public Geometry {
		struct Attrib {
			bool enabled;
			bool integer;
			GLuint index;
			GLint size;
			GLenum type;
			GLboolean normalized;
			GLsizei stride;
			uint32_t offset;
		};

		Attrib attribs[VS::ARRAY_MAX];

		Mesh *mesh;
		uint32_t format;

		GLuint array_id;
		GLuint instancing_array_id;
		GLuint vertex_id;
		GLuint index_id;

		GLuint aabb_array_id;
		GLuint aabb_vertex_id;

		GLuint index_wireframe_id;
		GLuint array_wireframe_id;
		GLuint instancing_array_wireframe_id;
		int index_wireframe_len;

		Vector<AABB> skeleton_bone_aabb;
		Vector<bool> skeleton_bone_used;

		//bool packed;

		struct BlendShape {
			GLuint vertex_id;
			GLuint array_id;
		};

		Vector<BlendShape> blend_shapes;

		AABB aabb;

		int array_len;
		int index_array_len;
		int max_bone;

		int array_byte_size;
		int index_array_byte_size;

		VS::PrimitiveType primitive;

		bool active;

		virtual void material_changed_notify() {
			mesh->instance_change_notify(false, true);
			mesh->update_multimeshes();
		}

		void create_aabb_vbo(GLuint aabb_index_id);

		int total_data_size;

		Surface() :
				mesh(nullptr),
				format(0),
				array_id(0),
				vertex_id(0),
				index_id(0),
				index_wireframe_id(0),
				array_wireframe_id(0),
				instancing_array_wireframe_id(0),
				index_wireframe_len(0),
				array_len(0),
				index_array_len(0),
				array_byte_size(0),
				index_array_byte_size(0),
				primitive(VS::PRIMITIVE_POINTS),
				active(false),
				total_data_size(0) {
			type = GEOMETRY_SURFACE;
		}

		~Surface() {
		}
	};

	struct MultiMesh;

	struct Mesh : public GeometryOwner {
		bool active;
		Vector<Surface *> surfaces;
		int blend_shape_count;
		VS::BlendShapeMode blend_shape_mode;
		PoolRealArray blend_shape_values;
		AABB custom_aabb;
		mutable uint64_t last_pass;
		SelfList<MultiMesh>::List multimeshes;
		_FORCE_INLINE_ void update_multimeshes() {
			SelfList<MultiMesh> *mm = multimeshes.first();
			while (mm) {
				mm->self()->instance_change_notify(false, true);
				mm = mm->next();
			}
		}

		Mesh() :
				active(false),
				blend_shape_count(0),
				blend_shape_mode(VS::BLEND_SHAPE_MODE_NORMALIZED),
				last_pass(0) {
		}
	};

	mutable RID_Owner<Mesh> mesh_owner;

	virtual RID mesh_create();

	virtual void mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>());

	virtual void mesh_set_blend_shape_count(RID p_mesh, int p_amount);
	virtual int mesh_get_blend_shape_count(RID p_mesh) const;

	virtual void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode);
	virtual VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const;

	virtual void mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values);
	virtual PoolVector<float> mesh_get_blend_shape_values(RID p_mesh) const;

	virtual void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data);

	virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material);
	virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const;

	virtual int mesh_surface_get_array_len(RID p_mesh, int p_surface) const;
	virtual int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const;

	virtual PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const;
	virtual PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const;

	virtual uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const;
	virtual VS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const;

	virtual AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const;
	virtual Vector<PoolVector<uint8_t>> mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const;
	virtual Vector<AABB> mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const;

	virtual void mesh_remove_surface(RID p_mesh, int p_surface);
	virtual int mesh_get_surface_count(RID p_mesh) const;

	virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb);
	virtual AABB mesh_get_custom_aabb(RID p_mesh) const;

	virtual AABB mesh_get_aabb(RID p_mesh, RID p_skeleton) const;
	virtual void mesh_clear(RID p_mesh);

	void mesh_render_blend_shapes(Surface *s, const float *p_weights);

	/* MULTIMESH API */

	struct MultiMesh : public GeometryOwner {
		RID mesh;
		int size;
		VS::MultimeshTransformFormat transform_format;
		VS::MultimeshColorFormat color_format;
		VS::MultimeshCustomDataFormat custom_data_format;
		Vector<float> data;
		AABB aabb;
		SelfList<MultiMesh> update_list;
		SelfList<MultiMesh> mesh_list;
		GLuint buffer;
		int visible_instances;

		int xform_floats;
		int color_floats;
		int custom_data_floats;

		bool dirty_aabb;
		bool dirty_data;

		MMInterpolator interpolator;
		LocalVector<RID> linked_canvas_items;

		MultiMesh() :
				size(0),
				transform_format(VS::MULTIMESH_TRANSFORM_2D),
				color_format(VS::MULTIMESH_COLOR_NONE),
				custom_data_format(VS::MULTIMESH_CUSTOM_DATA_NONE),
				update_list(this),
				mesh_list(this),
				buffer(0),
				visible_instances(-1),
				xform_floats(0),
				color_floats(0),
				custom_data_floats(0),
				dirty_aabb(true),
				dirty_data(true) {
		}
	};

	mutable RID_Owner<MultiMesh> multimesh_owner;

	SelfList<MultiMesh>::List multimesh_update_list;

	void update_dirty_multimeshes();

	virtual RID _multimesh_create();

	virtual void _multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data_format = VS::MULTIMESH_CUSTOM_DATA_NONE);
	virtual int _multimesh_get_instance_count(RID p_multimesh) const;

	virtual void _multimesh_set_mesh(RID p_multimesh, RID p_mesh);
	virtual void _multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform);
	virtual void _multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform);
	virtual void _multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color);
	virtual void _multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data);

	virtual RID _multimesh_get_mesh(RID p_multimesh) const;

	virtual Transform _multimesh_instance_get_transform(RID p_multimesh, int p_index) const;
	virtual Transform2D _multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const;
	virtual Color _multimesh_instance_get_color(RID p_multimesh, int p_index) const;
	virtual Color _multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const;

	virtual void _multimesh_set_as_bulk_array(RID p_multimesh, const PoolVector<float> &p_array);

	virtual void _multimesh_set_visible_instances(RID p_multimesh, int p_visible);
	virtual int _multimesh_get_visible_instances(RID p_multimesh) const;

	virtual AABB _multimesh_get_aabb(RID p_multimesh) const;
	virtual MMInterpolator *_multimesh_get_interpolator(RID p_multimesh) const;
	virtual void multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach);

	/* IMMEDIATE API */

	struct Immediate : public Geometry {
		struct Chunk {
			RID texture;
			VS::PrimitiveType primitive;
			Vector<Vector3> vertices;
			Vector<Vector3> normals;
			Vector<Plane> tangents;
			Vector<Color> colors;
			Vector<Vector2> uvs;
			Vector<Vector2> uvs2;
		};

		List<Chunk> chunks;
		bool building;
		int mask;
		AABB aabb;

		Immediate() {
			type = GEOMETRY_IMMEDIATE;
			building = false;
		}
	};

	Vector3 chunk_vertex;
	Vector3 chunk_normal;
	Plane chunk_tangent;
	Color chunk_color;
	Vector2 chunk_uv;
	Vector2 chunk_uv2;

	mutable RID_Owner<Immediate> immediate_owner;

	virtual RID immediate_create();
	virtual void immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture = RID());
	virtual void immediate_vertex(RID p_immediate, const Vector3 &p_vertex);
	virtual void immediate_normal(RID p_immediate, const Vector3 &p_normal);
	virtual void immediate_tangent(RID p_immediate, const Plane &p_tangent);
	virtual void immediate_color(RID p_immediate, const Color &p_color);
	virtual void immediate_uv(RID p_immediate, const Vector2 &tex_uv);
	virtual void immediate_uv2(RID p_immediate, const Vector2 &tex_uv);
	virtual void immediate_end(RID p_immediate);
	virtual void immediate_clear(RID p_immediate);
	virtual void immediate_set_material(RID p_immediate, RID p_material);
	virtual RID immediate_get_material(RID p_immediate) const;
	virtual AABB immediate_get_aabb(RID p_immediate) const;

	/* SKELETON API */

	struct Skeleton : RID_Data {
		bool use_2d;
		int size;
		uint32_t revision;
		Vector<float> skel_texture;
		GLuint texture;
		SelfList<Skeleton> update_list;
		Set<RasterizerScene::InstanceBase *> instances; //instances using skeleton

		Transform2D base_transform_2d;
		LocalVector<RID> linked_canvas_items;

		Skeleton() :
				use_2d(false),
				size(0),
				revision(1),
				texture(0),
				update_list(this) {
		}
	};

	mutable RID_Owner<Skeleton> skeleton_owner;

	SelfList<Skeleton>::List skeleton_update_list;

	void update_dirty_skeletons();

	virtual RID skeleton_create();
	virtual void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false);
	virtual int skeleton_get_bone_count(RID p_skeleton) const;
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform);
	virtual Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const;
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform);
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const;
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform);
	virtual uint32_t skeleton_get_revision(RID p_skeleton) const;
	virtual void skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach);

	/* Light API */

	struct Light : Instantiable {
		VS::LightType type;
		float param[VS::LIGHT_PARAM_MAX];
		Color color;
		Color shadow_color;
		RID projector;
		bool shadow;
		bool negative;
		bool reverse_cull;
		VS::LightBakeMode bake_mode;
		uint32_t cull_mask;
		VS::LightOmniShadowMode omni_shadow_mode;
		VS::LightOmniShadowDetail omni_shadow_detail;
		VS::LightDirectionalShadowMode directional_shadow_mode;
		VS::LightDirectionalShadowDepthRangeMode directional_range_mode;
		bool directional_blend_splits;
		uint64_t version;
	};

	mutable RID_Owner<Light> light_owner;

	virtual RID light_create(VS::LightType p_type);

	virtual void light_set_color(RID p_light, const Color &p_color);
	virtual void light_set_param(RID p_light, VS::LightParam p_param, float p_value);
	virtual void light_set_shadow(RID p_light, bool p_enabled);
	virtual void light_set_shadow_color(RID p_light, const Color &p_color);
	virtual void light_set_projector(RID p_light, RID p_texture);
	virtual void light_set_negative(RID p_light, bool p_enable);
	virtual void light_set_cull_mask(RID p_light, uint32_t p_mask);
	virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled);
	virtual void light_set_use_gi(RID p_light, bool p_enabled);
	virtual void light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode);

	virtual void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode);
	virtual void light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail);

	virtual void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode);
	virtual void light_directional_set_blend_splits(RID p_light, bool p_enable);
	virtual bool light_directional_get_blend_splits(RID p_light) const;

	virtual VS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light);
	virtual VS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light);

	virtual void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode);
	virtual VS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const;

	virtual bool light_has_shadow(RID p_light) const;

	virtual VS::LightType light_get_type(RID p_light) const;
	virtual float light_get_param(RID p_light, VS::LightParam p_param);
	virtual Color light_get_color(RID p_light);
	virtual bool light_get_use_gi(RID p_light);
	virtual VS::LightBakeMode light_get_bake_mode(RID p_light);

	virtual AABB light_get_aabb(RID p_light) const;
	virtual uint64_t light_get_version(RID p_light) const;

	/* PROBE API */

	struct ReflectionProbe : Instantiable {
		VS::ReflectionProbeUpdateMode update_mode;
		float intensity;
		Color interior_ambient;
		float interior_ambient_energy;
		float interior_ambient_probe_contrib;
		float max_distance;
		Vector3 extents;
		Vector3 origin_offset;
		bool interior;
		bool box_projection;
		bool enable_shadows;
		uint32_t cull_mask;
	};

	mutable RID_Owner<ReflectionProbe> reflection_probe_owner;

	virtual RID reflection_probe_create();

	virtual void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode);
	virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity);
	virtual void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient);
	virtual void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy);
	virtual void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib);
	virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance);
	virtual void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents);
	virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset);
	virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable);
	virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable);
	virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable);
	virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers);
	virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution);

	virtual AABB reflection_probe_get_aabb(RID p_probe) const;
	virtual VS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const;
	virtual uint32_t reflection_probe_get_cull_mask(RID p_probe) const;

	virtual Vector3 reflection_probe_get_extents(RID p_probe) const;
	virtual Vector3 reflection_probe_get_origin_offset(RID p_probe) const;
	virtual float reflection_probe_get_origin_max_distance(RID p_probe) const;
	virtual bool reflection_probe_renders_shadows(RID p_probe) const;

	/* GI PROBE API */

	struct GIProbe : public Instantiable {
		AABB bounds;
		Transform to_cell;
		float cell_size;

		int dynamic_range;
		float energy;
		float bias;
		float normal_bias;
		float propagation;
		bool interior;
		bool compress;

		uint32_t version;

		PoolVector<int> dynamic_data;
	};

	mutable RID_Owner<GIProbe> gi_probe_owner;

	virtual RID gi_probe_create();

	virtual void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds);
	virtual AABB gi_probe_get_bounds(RID p_probe) const;

	virtual void gi_probe_set_cell_size(RID p_probe, float p_size);
	virtual float gi_probe_get_cell_size(RID p_probe) const;

	virtual void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform);
	virtual Transform gi_probe_get_to_cell_xform(RID p_probe) const;

	virtual void gi_probe_set_dynamic_data(RID p_probe, const PoolVector<int> &p_data);
	virtual PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const;

	virtual void gi_probe_set_dynamic_range(RID p_probe, int p_range);
	virtual int gi_probe_get_dynamic_range(RID p_probe) const;

	virtual void gi_probe_set_energy(RID p_probe, float p_range);
	virtual float gi_probe_get_energy(RID p_probe) const;

	virtual void gi_probe_set_bias(RID p_probe, float p_range);
	virtual float gi_probe_get_bias(RID p_probe) const;

	virtual void gi_probe_set_normal_bias(RID p_probe, float p_range);
	virtual float gi_probe_get_normal_bias(RID p_probe) const;

	virtual void gi_probe_set_propagation(RID p_probe, float p_range);
	virtual float gi_probe_get_propagation(RID p_probe) const;

	virtual void gi_probe_set_interior(RID p_probe, bool p_enable);
	virtual bool gi_probe_is_interior(RID p_probe) const;

	virtual void gi_probe_set_compress(RID p_probe, bool p_enable);
	virtual bool gi_probe_is_compressed(RID p_probe) const;

	virtual uint32_t gi_probe_get_version(RID p_probe);

	struct GIProbeData : public RID_Data {
		int width;
		int height;
		int depth;
		int levels;
		GLuint tex_id;
		GIProbeCompression compression;

		GIProbeData() {
		}
	};

	mutable RID_Owner<GIProbeData> gi_probe_data_owner;

	virtual RID gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression);
	virtual void gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data);

	/* LIGHTMAP CAPTURE */

	virtual RID lightmap_capture_create();
	virtual void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds);
	virtual AABB lightmap_capture_get_bounds(RID p_capture) const;
	virtual void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree);
	virtual PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const;
	virtual void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform);
	virtual Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const;
	virtual void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv);
	virtual int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const;

	virtual void lightmap_capture_set_energy(RID p_capture, float p_energy);
	virtual float lightmap_capture_get_energy(RID p_capture) const;
	virtual void lightmap_capture_set_interior(RID p_capture, bool p_interior);
	virtual bool lightmap_capture_is_interior(RID p_capture) const;

	virtual const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const;

	struct LightmapCapture : public Instantiable {
		PoolVector<LightmapCaptureOctree> octree;
		AABB bounds;
		Transform cell_xform;
		int cell_subdiv;
		float energy;
		bool interior;

		SelfList<LightmapCapture> update_list;

		LightmapCapture() :
				update_list(this) {
			energy = 1.0;
			cell_subdiv = 1;
			interior = false;
		}
	};

	SelfList<LightmapCapture>::List capture_update_list;

	void update_dirty_captures();

	mutable RID_Owner<LightmapCapture> lightmap_capture_data_owner;

	/* PARTICLES */

	struct Particles : public GeometryOwner {
		bool inactive;
		float inactive_time;
		bool emitting;
		bool one_shot;
		int amount;
		float lifetime;
		float pre_process_time;
		float explosiveness;
		float randomness;
		bool restart_request;
		AABB custom_aabb;
		bool use_local_coords;
		RID process_material;

		VS::ParticlesDrawOrder draw_order;

		Vector<RID> draw_passes;

		GLuint particle_buffers[2];
		GLuint particle_vaos[2];

		GLuint particle_buffer_histories[2];
		GLuint particle_vao_histories[2];
		bool particle_valid_histories[2];
		bool histories_enabled;

		SelfList<Particles> particle_element;

		float phase;
		float prev_phase;
		uint64_t prev_ticks;
		uint32_t random_seed;

		uint32_t cycle_number;

		float speed_scale;

		int fixed_fps;
		bool fractional_delta;
		float frame_remainder;

		bool clear;

		Transform emission_transform;

		Particles() :
				inactive(true),
				inactive_time(0.0),
				emitting(false),
				one_shot(false),
				amount(0),
				lifetime(1.0),
				pre_process_time(0.0),
				explosiveness(0.0),
				randomness(0.0),
				restart_request(false),
				custom_aabb(AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8))),
				use_local_coords(true),
				draw_order(VS::PARTICLES_DRAW_ORDER_INDEX),
				histories_enabled(false),
				particle_element(this),
				prev_ticks(0),
				random_seed(0),
				cycle_number(0),
				speed_scale(1.0),
				fixed_fps(0),
				fractional_delta(false),
				frame_remainder(0),
				clear(true) {
			particle_buffers[0] = 0;
			particle_buffers[1] = 0;
			glGenBuffers(2, particle_buffers);
			glGenVertexArrays(2, particle_vaos);
		}

		~Particles() {
			glDeleteBuffers(2, particle_buffers);
			glDeleteVertexArrays(2, particle_vaos);
			if (histories_enabled) {
				glDeleteBuffers(2, particle_buffer_histories);
				glDeleteVertexArrays(2, particle_vao_histories);
			}
		}
	};

	SelfList<Particles>::List particle_update_list;

	void update_particles();

	mutable RID_Owner<Particles> particles_owner;

	virtual RID particles_create();

	virtual void particles_set_emitting(RID p_particles, bool p_emitting);
	virtual bool particles_get_emitting(RID p_particles);
	virtual void particles_set_amount(RID p_particles, int p_amount);
	virtual void particles_set_lifetime(RID p_particles, float p_lifetime);
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot);
	virtual void particles_set_pre_process_time(RID p_particles, float p_time);
	virtual void particles_set_explosiveness_ratio(RID p_particles, float p_ratio);
	virtual void particles_set_randomness_ratio(RID p_particles, float p_ratio);
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb);
	virtual void particles_set_speed_scale(RID p_particles, float p_scale);
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable);
	virtual void particles_set_process_material(RID p_particles, RID p_material);
	virtual void particles_set_fixed_fps(RID p_particles, int p_fps);
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable);
	virtual void particles_restart(RID p_particles);

	virtual void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order);

	virtual void particles_set_draw_passes(RID p_particles, int p_passes);
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh);

	virtual void particles_request_process(RID p_particles);
	virtual AABB particles_get_current_aabb(RID p_particles);
	virtual AABB particles_get_aabb(RID p_particles) const;

	virtual void _particles_update_histories(Particles *particles);

	virtual void particles_set_emission_transform(RID p_particles, const Transform &p_transform);
	void _particles_process(Particles *p_particles, float p_delta);

	virtual int particles_get_draw_passes(RID p_particles) const;
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const;

	virtual bool particles_is_inactive(RID p_particles) const;

	/* INSTANCE */

	virtual void instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);
	virtual void instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);

	virtual void instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance);
	virtual void instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance);

	/* RENDER TARGET */

	struct RenderTarget : public RID_Data {
		GLuint fbo;
		GLuint color;
		GLuint depth;

		struct Buffers {
			bool active;
			bool effects_active;
			GLuint fbo;
			GLuint depth;
			GLuint specular;
			GLuint diffuse;
			GLuint normal_rough;
			GLuint sss;

			GLuint effect_fbo;
			GLuint effect;

		} buffers;

		struct Effects {
			struct MipMaps {
				struct Size {
					GLuint fbo = 0;
					int width = 0;
					int height = 0;
				};

				Vector<Size> sizes;
				GLuint color;
				int levels;

				MipMaps() :
						color(0),
						levels(0) {
				}
			};

			MipMaps mip_maps[2]; //first mipmap chain starts from full-screen
			//GLuint depth2; //depth for the second mipmap chain, in case of desiring upsampling

			struct SSAO {
				GLuint blur_fbo[2]; // blur fbo
				GLuint blur_red[2]; // 8 bits red buffer

				GLuint linear_depth;

				Vector<GLuint> depth_mipmap_fbos; //fbos for depth mipmapsla ver

				SSAO() :
						linear_depth(0) {
					blur_fbo[0] = 0;
					blur_fbo[1] = 0;
				}
			} ssao;

			Effects() {}

		} effects;

		struct Exposure {
			GLuint fbo;
			GLuint color;

			Exposure() :
					fbo(0) {}
		} exposure;

		// External FBO to render our final result to (mostly used for ARVR)
		struct External {
			GLuint fbo;
			GLuint color;
			GLuint depth;

			External() :
					fbo(0),
					color(0),
					depth(0) {
			}
		} external;

		uint64_t last_exposure_tick;

		int width, height;

		bool flags[RENDER_TARGET_FLAG_MAX];

		bool used_in_frame;
		VS::ViewportMSAA msaa;
		bool use_fxaa;
		bool use_debanding;
		float sharpen_intensity;

		RID texture;

		RenderTarget() :
				fbo(0),
				depth(0),
				last_exposure_tick(0),
				width(0),
				height(0),
				used_in_frame(false),
				msaa(VS::VIEWPORT_MSAA_DISABLED),
				use_fxaa(false),
				use_debanding(false),
				sharpen_intensity(0.0) {
			exposure.fbo = 0;
			buffers.fbo = 0;
			external.fbo = 0;
			for (int i = 0; i < RENDER_TARGET_FLAG_MAX; i++) {
				flags[i] = false;
			}
			flags[RENDER_TARGET_HDR] = true;
			buffers.active = false;
			buffers.effects_active = false;
		}
	};

	mutable RID_Owner<RenderTarget> render_target_owner;

	void _render_target_clear(RenderTarget *rt);
	void _render_target_allocate(RenderTarget *rt);

	virtual RID render_target_create();
	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y);
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height);
	virtual RID render_target_get_texture(RID p_render_target) const;
	virtual uint32_t render_target_get_depth_texture_id(RID p_render_target) const;
	virtual void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id);

	virtual void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value);
	virtual bool render_target_was_used(RID p_render_target);
	virtual void render_target_clear_used(RID p_render_target);
	virtual void render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa);
	virtual void render_target_set_use_fxaa(RID p_render_target, bool p_fxaa);
	virtual void render_target_set_use_debanding(RID p_render_target, bool p_debanding);
	virtual void render_target_set_sharpen_intensity(RID p_render_target, float p_intensity);

	/* CANVAS SHADOW */

	struct CanvasLightShadow : public RID_Data {
		int size;
		int height;
		GLuint fbo;
		GLuint depth;
		GLuint distance; //for older devices
	};

	RID_Owner<CanvasLightShadow> canvas_light_shadow_owner;

	virtual RID canvas_light_shadow_buffer_create(int p_width);

	/* LIGHT SHADOW MAPPING */

	struct CanvasOccluder : public RID_Data {
		GLuint array_id; // 0 means, unconfigured
		GLuint vertex_id; // 0 means, unconfigured
		GLuint index_id; // 0 means, unconfigured
		PoolVector<Vector2> lines;
		int len;
	};

	RID_Owner<CanvasOccluder> canvas_occluder_owner;

	virtual RID canvas_light_occluder_create();
	virtual void canvas_light_occluder_set_polylines(RID p_occluder, const PoolVector<Vector2> &p_lines);

	virtual VS::InstanceType get_base_type(RID p_rid) const;

	virtual bool free(RID p_rid);

	struct Frame {
		RenderTarget *current_rt;

		bool clear_request;
		Color clear_request_color;
		float time[4];
		float delta;
		uint64_t count;
	} frame;

	struct GLWrapper {
		mutable BitFieldDynamic texture_unit_table;
		mutable LocalVector<uint32_t> texture_units_bound;

		void gl_active_texture(GLenum p_texture) const {
			::glActiveTexture(p_texture);

			p_texture -= GL_TEXTURE0;

			// Check for below zero and above max in one check.
			ERR_FAIL_COND((unsigned int)p_texture >= texture_unit_table.get_num_bits());

			// Set if the first occurrence in the table.
			if (texture_unit_table.check_and_set(p_texture)) {
				texture_units_bound.push_back(p_texture);
			}
		}
		void initialize(int p_max_texture_image_units);
		void reset();
	} gl_wrapper;

	void initialize();
	void finalize();

	static int32_t safe_gl_get_integer(unsigned int p_gl_param_name, int32_t p_max_accepted = INT32_MAX);

	virtual bool has_os_feature(const String &p_feature) const;

	virtual void update_dirty_resources();

	virtual void set_debug_generate_wireframes(bool p_generate);

	virtual void render_info_begin_capture();
	virtual void render_info_end_capture();
	virtual int get_captured_render_info(VS::RenderInfo p_info);

	virtual uint64_t get_render_info(VS::RenderInfo p_info);
	virtual String get_video_adapter_name() const;
	virtual String get_video_adapter_vendor() const;

	// NOTE : THESE SIZES ARE IN BYTES. BUFFER SIZES MAY NOT BE SPECIFIED IN BYTES SO REMEMBER TO CONVERT THEM WHEN CALLING.
	void buffer_orphan_and_upload(unsigned int p_buffer_size_bytes, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, GLenum p_target = GL_ARRAY_BUFFER, GLenum p_usage = GL_DYNAMIC_DRAW, bool p_optional_orphan = false) const;
	bool safe_buffer_sub_data(unsigned int p_total_buffer_size_bytes, GLenum p_target, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, unsigned int &r_offset_after_bytes) const;

	RasterizerStorageGLES3();
	~RasterizerStorageGLES3();
};

inline bool RasterizerStorageGLES3::safe_buffer_sub_data(unsigned int p_total_buffer_size_bytes, GLenum p_target, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, unsigned int &r_offset_after_bytes) const {
	r_offset_after_bytes = p_offset_bytes + p_data_size_bytes;
#ifdef DEBUG_ENABLED
	// we are trying to write across the edge of the buffer
	if (r_offset_after_bytes > p_total_buffer_size_bytes) {
		return false;
	}
#endif
	glBufferSubData(p_target, p_offset_bytes, p_data_size_bytes, p_data);
	return true;
}

// standardize the orphan / upload in one place so it can be changed per platform as necessary, and avoid future
// bugs causing pipeline stalls
// NOTE : THESE SIZES ARE IN BYTES. BUFFER SIZES MAY NOT BE SPECIFIED IN BYTES SO REMEMBER TO CONVERT THEM WHEN CALLING.
inline void RasterizerStorageGLES3::buffer_orphan_and_upload(unsigned int p_buffer_size_bytes, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, GLenum p_target, GLenum p_usage, bool p_optional_orphan) const {
	// Orphan the buffer to avoid CPU/GPU sync points caused by glBufferSubData
	// Was previously #ifndef GLES_OVER_GL however this causes stalls on desktop mac also (and possibly other)
	if (!p_optional_orphan || (config.should_orphan)) {
		glBufferData(p_target, p_buffer_size_bytes, nullptr, p_usage);
#ifdef RASTERIZER_EXTRA_CHECKS
		// fill with garbage off the end of the array
		if (p_buffer_size_bytes) {
			unsigned int start = p_offset_bytes + p_data_size_bytes;
			unsigned int end = start + 1024;
			if (end < p_buffer_size) {
				uint8_t *garbage = (uint8_t *)alloca(1024);
				for (int n = 0; n < 1024; n++) {
					garbage[n] = Math::random(0, 255);
				}
				glBufferSubData(p_target, start, 1024, garbage);
			}
		}
#endif
	}
	ERR_FAIL_COND((p_offset_bytes + p_data_size_bytes) > p_buffer_size_bytes);
	glBufferSubData(p_target, p_offset_bytes, p_data_size_bytes, p_data);
}

#endif // RASTERIZER_STORAGE_GLES3_H
