/**************************************************************************/
/*  material_storage.h                                                    */
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

#ifndef MATERIAL_STORAGE_GLES3_H
#define MATERIAL_STORAGE_GLES3_H

#ifdef GLES3_ENABLED

#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/shader_compiler.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/storage/material_storage.h"
#include "servers/rendering/storage/utilities.h"

#include "drivers/gles3/shaders/canvas.glsl.gen.h"
#include "drivers/gles3/shaders/particles.glsl.gen.h"
#include "drivers/gles3/shaders/scene.glsl.gen.h"
#include "drivers/gles3/shaders/sky.glsl.gen.h"

namespace GLES3 {

/* Shader Structs */

struct ShaderData {
	String path;
	HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;
	HashMap<StringName, HashMap<int, RID>> default_texture_params;

	virtual void set_path_hint(const String &p_hint);
	virtual void set_default_texture_parameter(const StringName &p_name, RID p_texture, int p_index);
	virtual Variant get_default_parameter(const StringName &p_parameter) const;
	virtual void get_shader_uniform_list(List<PropertyInfo> *p_param_list) const;
	virtual void get_instance_param_list(List<RendererMaterialStorage::InstanceShaderParam> *p_param_list) const;
	virtual bool is_parameter_texture(const StringName &p_param) const;

	virtual void set_code(const String &p_Code) = 0;
	virtual bool is_animated() const = 0;
	virtual bool casts_shadows() const = 0;
	virtual RS::ShaderNativeSourceCode get_native_source_code() const { return RS::ShaderNativeSourceCode(); }

	virtual ~ShaderData() {}
};

typedef ShaderData *(*ShaderDataRequestFunction)();

struct Material;

struct Shader {
	ShaderData *data = nullptr;
	String code;
	String path_hint;
	RS::ShaderMode mode;
	HashMap<StringName, HashMap<int, RID>> default_texture_parameter;
	HashSet<Material *> owners;
};

/* Material structs */

struct MaterialData {
	void update_uniform_buffer(const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, const HashMap<StringName, Variant> &p_parameters, uint8_t *p_buffer, uint32_t p_buffer_size);
	void update_textures(const HashMap<StringName, Variant> &p_parameters, const HashMap<StringName, HashMap<int, RID>> &p_default_textures, const Vector<ShaderCompiler::GeneratedCode::Texture> &p_texture_uniforms, RID *p_textures, bool p_use_linear_color);

	virtual void set_render_priority(int p_priority) = 0;
	virtual void set_next_pass(RID p_pass) = 0;
	virtual void update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty) = 0;
	virtual void bind_uniforms() = 0;
	virtual ~MaterialData();

	// Used internally by all Materials
	void update_parameters_internal(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, const Vector<ShaderCompiler::GeneratedCode::Texture> &p_texture_uniforms, const HashMap<StringName, HashMap<int, RID>> &p_default_texture_params, uint32_t p_ubo_size, bool p_is_3d_shader_type);

protected:
	Vector<uint8_t> ubo_data;
	GLuint uniform_buffer = GLuint(0);
	Vector<RID> texture_cache;

private:
	friend class MaterialStorage;
	RID self;
	List<RID>::Element *global_buffer_E = nullptr;
	List<RID>::Element *global_texture_E = nullptr;
	uint64_t global_textures_pass = 0;
	HashMap<StringName, uint64_t> used_global_textures;
};

typedef MaterialData *(*MaterialDataRequestFunction)(ShaderData *);

struct Material {
	RID self;
	MaterialData *data = nullptr;
	Shader *shader = nullptr;
	//shortcut to shader data and type
	RS::ShaderMode shader_mode = RS::SHADER_MAX;
	uint32_t shader_id = 0;
	bool uniform_dirty = false;
	bool texture_dirty = false;
	HashMap<StringName, Variant> params;
	int32_t priority = 0;
	RID next_pass;
	SelfList<Material> update_element;

	Dependency dependency;

	Material() :
			update_element(this) {}
};

/* CanvasItem Materials */

struct CanvasShaderData : public ShaderData {
	enum BlendMode { // Used internally.
		BLEND_MODE_MIX,
		BLEND_MODE_ADD,
		BLEND_MODE_SUB,
		BLEND_MODE_MUL,
		BLEND_MODE_PMALPHA,
		BLEND_MODE_DISABLED,
		BLEND_MODE_LCD,
	};

	// All these members are (re)initialized in `set_code`.
	// Make sure to add the init to `set_code` whenever adding new members.

	bool valid;
	RID version;

	Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

	Vector<uint32_t> ubo_offsets;
	uint32_t ubo_size;

	String code;

	BlendMode blend_mode;

	bool uses_screen_texture;
	bool uses_screen_texture_mipmaps;
	bool uses_sdf;
	bool uses_time;
	bool uses_custom0;
	bool uses_custom1;

	uint64_t vertex_input_mask;

	virtual void set_code(const String &p_Code);
	virtual bool is_animated() const;
	virtual bool casts_shadows() const;
	virtual RS::ShaderNativeSourceCode get_native_source_code() const;

	CanvasShaderData();
	virtual ~CanvasShaderData();
};

ShaderData *_create_canvas_shader_func();

struct CanvasMaterialData : public MaterialData {
	CanvasShaderData *shader_data = nullptr;

	virtual void set_render_priority(int p_priority) {}
	virtual void set_next_pass(RID p_pass) {}
	virtual void update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
	virtual void bind_uniforms();
	virtual ~CanvasMaterialData();
};

MaterialData *_create_canvas_material_func(ShaderData *p_shader);

/* Sky Materials */

struct SkyShaderData : public ShaderData {
	// All these members are (re)initialized in `set_code`.
	// Make sure to add the init to `set_code` whenever adding new members.

	bool valid;
	RID version;

	Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

	Vector<uint32_t> ubo_offsets;
	uint32_t ubo_size;

	String code;

	bool uses_time;
	bool uses_position;
	bool uses_half_res;
	bool uses_quarter_res;
	bool uses_light;

	virtual void set_code(const String &p_Code);
	virtual bool is_animated() const;
	virtual bool casts_shadows() const;
	virtual RS::ShaderNativeSourceCode get_native_source_code() const;
	SkyShaderData();
	virtual ~SkyShaderData();
};

ShaderData *_create_sky_shader_func();

struct SkyMaterialData : public MaterialData {
	SkyShaderData *shader_data = nullptr;
	bool uniform_set_updated = false;

	virtual void set_render_priority(int p_priority) {}
	virtual void set_next_pass(RID p_pass) {}
	virtual void update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
	virtual void bind_uniforms();
	virtual ~SkyMaterialData();
};

MaterialData *_create_sky_material_func(ShaderData *p_shader);

/* Scene Materials */

struct SceneShaderData : public ShaderData {
	enum BlendMode { // Used internally.
		BLEND_MODE_MIX,
		BLEND_MODE_ADD,
		BLEND_MODE_SUB,
		BLEND_MODE_MUL,
		BLEND_MODE_PREMULT_ALPHA,
		BLEND_MODE_ALPHA_TO_COVERAGE
	};

	enum DepthDraw {
		DEPTH_DRAW_DISABLED,
		DEPTH_DRAW_OPAQUE,
		DEPTH_DRAW_ALWAYS
	};

	enum DepthTest {
		DEPTH_TEST_DISABLED,
		DEPTH_TEST_ENABLED
	};

	enum AlphaAntiAliasing {
		ALPHA_ANTIALIASING_OFF,
		ALPHA_ANTIALIASING_ALPHA_TO_COVERAGE,
		ALPHA_ANTIALIASING_ALPHA_TO_COVERAGE_AND_TO_ONE
	};

	// All these members are (re)initialized in `set_code`.
	// Make sure to add the init to `set_code` whenever adding new members.

	bool valid;
	RID version;

	Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

	Vector<uint32_t> ubo_offsets;
	uint32_t ubo_size;

	String code;

	BlendMode blend_mode;
	AlphaAntiAliasing alpha_antialiasing_mode;
	DepthDraw depth_draw;
	DepthTest depth_test;
	RS::CullMode cull_mode;

	bool uses_point_size;
	bool uses_alpha;
	bool uses_alpha_clip;
	bool uses_blend_alpha;
	bool uses_depth_prepass_alpha;
	bool uses_discard;
	bool uses_roughness;
	bool uses_normal;
	bool uses_particle_trails;
	bool wireframe;

	bool unshaded;
	bool uses_vertex;
	bool uses_position;
	bool uses_sss;
	bool uses_transmittance;
	bool uses_screen_texture;
	bool uses_screen_texture_mipmaps;
	bool uses_depth_texture;
	bool uses_normal_texture;
	bool uses_time;
	bool uses_vertex_time;
	bool uses_fragment_time;
	bool writes_modelview_or_projection;
	bool uses_world_coordinates;
	bool uses_tangent;
	bool uses_color;
	bool uses_uv;
	bool uses_uv2;
	bool uses_custom0;
	bool uses_custom1;
	bool uses_custom2;
	bool uses_custom3;
	bool uses_bones;
	bool uses_weights;

	uint64_t vertex_input_mask;

	virtual void set_code(const String &p_Code);
	virtual bool is_animated() const;
	virtual bool casts_shadows() const;
	virtual RS::ShaderNativeSourceCode get_native_source_code() const;

	SceneShaderData();
	virtual ~SceneShaderData();
};

ShaderData *_create_scene_shader_func();

struct SceneMaterialData : public MaterialData {
	SceneShaderData *shader_data = nullptr;
	uint64_t last_pass = 0;
	uint32_t index = 0;
	RID next_pass;
	uint8_t priority = 0;
	virtual void set_render_priority(int p_priority);
	virtual void set_next_pass(RID p_pass);
	virtual void update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
	virtual void bind_uniforms();
	virtual ~SceneMaterialData();
};

MaterialData *_create_scene_material_func(ShaderData *p_shader);

/* Particle Shader */

enum {
	PARTICLES_MAX_USERDATAS = 6
};

struct ParticlesShaderData : public ShaderData {
	// All these members are (re)initialized in `set_code`.
	// Make sure to add the init to `set_code` whenever adding new members.

	bool valid;
	RID version;

	Vector<ShaderCompiler::GeneratedCode::Texture> texture_uniforms;

	Vector<uint32_t> ubo_offsets;
	uint32_t ubo_size;

	String code;

	bool uses_collision;
	bool uses_time;

	bool userdatas_used[PARTICLES_MAX_USERDATAS] = {};
	uint32_t userdata_count;

	virtual void set_code(const String &p_Code);
	virtual bool is_animated() const;
	virtual bool casts_shadows() const;
	virtual RS::ShaderNativeSourceCode get_native_source_code() const;

	ParticlesShaderData() {}
	virtual ~ParticlesShaderData();
};

ShaderData *_create_particles_shader_func();

struct ParticleProcessMaterialData : public MaterialData {
	ParticlesShaderData *shader_data = nullptr;
	RID uniform_set;

	virtual void set_render_priority(int p_priority) {}
	virtual void set_next_pass(RID p_pass) {}
	virtual void update_parameters(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty);
	virtual void bind_uniforms();
	virtual ~ParticleProcessMaterialData();
};

MaterialData *_create_particles_material_func(ShaderData *p_shader);

/* Global shader uniform structs */
struct GlobalShaderUniforms {
	enum {
		BUFFER_DIRTY_REGION_SIZE = 1024
	};
	struct Variable {
		HashSet<RID> texture_materials; // materials using this

		RS::GlobalShaderParameterType type;
		Variant value;
		Variant override;
		int32_t buffer_index; //for vectors
		int32_t buffer_elements; //for vectors
	};

	HashMap<StringName, Variable> variables;

	struct Value {
		float x;
		float y;
		float z;
		float w;
	};

	struct ValueInt {
		int32_t x;
		int32_t y;
		int32_t z;
		int32_t w;
	};

	struct ValueUInt {
		uint32_t x;
		uint32_t y;
		uint32_t z;
		uint32_t w;
	};

	struct ValueUsage {
		uint32_t elements = 0;
	};

	List<RID> materials_using_buffer;
	List<RID> materials_using_texture;

	GLuint buffer = GLuint(0);
	Value *buffer_values = nullptr;
	ValueUsage *buffer_usage = nullptr;
	bool *buffer_dirty_regions = nullptr;
	uint32_t buffer_dirty_region_count = 0;

	uint32_t buffer_size;

	bool must_update_texture_materials = false;
	bool must_update_buffer_materials = false;

	HashMap<RID, int32_t> instance_buffer_pos;
};

class MaterialStorage : public RendererMaterialStorage {
private:
	friend struct MaterialData;
	static MaterialStorage *singleton;

	/* GLOBAL SHADER UNIFORM API */

	GlobalShaderUniforms global_shader_uniforms;

	int32_t _global_shader_uniform_allocate(uint32_t p_elements);
	void _global_shader_uniform_store_in_buffer(int32_t p_index, RS::GlobalShaderParameterType p_type, const Variant &p_value);
	void _global_shader_uniform_mark_buffer_dirty(int32_t p_index, int32_t p_elements);

	/* SHADER API */

	ShaderDataRequestFunction shader_data_request_func[RS::SHADER_MAX];
	mutable RID_Owner<Shader, true> shader_owner;

	/* MATERIAL API */
	MaterialDataRequestFunction material_data_request_func[RS::SHADER_MAX];
	mutable RID_Owner<Material, true> material_owner;

	SelfList<Material>::List material_update_list;

public:
	static MaterialStorage *get_singleton();

	MaterialStorage();
	virtual ~MaterialStorage();

	static _FORCE_INLINE_ void store_transform(const Transform3D &p_mtx, float *p_array) {
		p_array[0] = p_mtx.basis.rows[0][0];
		p_array[1] = p_mtx.basis.rows[1][0];
		p_array[2] = p_mtx.basis.rows[2][0];
		p_array[3] = 0;
		p_array[4] = p_mtx.basis.rows[0][1];
		p_array[5] = p_mtx.basis.rows[1][1];
		p_array[6] = p_mtx.basis.rows[2][1];
		p_array[7] = 0;
		p_array[8] = p_mtx.basis.rows[0][2];
		p_array[9] = p_mtx.basis.rows[1][2];
		p_array[10] = p_mtx.basis.rows[2][2];
		p_array[11] = 0;
		p_array[12] = p_mtx.origin.x;
		p_array[13] = p_mtx.origin.y;
		p_array[14] = p_mtx.origin.z;
		p_array[15] = 1;
	}

	static _FORCE_INLINE_ void store_transform_3x3(const Basis &p_mtx, float *p_array) {
		p_array[0] = p_mtx.rows[0][0];
		p_array[1] = p_mtx.rows[1][0];
		p_array[2] = p_mtx.rows[2][0];
		p_array[3] = 0;
		p_array[4] = p_mtx.rows[0][1];
		p_array[5] = p_mtx.rows[1][1];
		p_array[6] = p_mtx.rows[2][1];
		p_array[7] = 0;
		p_array[8] = p_mtx.rows[0][2];
		p_array[9] = p_mtx.rows[1][2];
		p_array[10] = p_mtx.rows[2][2];
		p_array[11] = 0;
	}

	static _FORCE_INLINE_ void store_camera(const Projection &p_mtx, float *p_array) {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				p_array[i * 4 + j] = p_mtx.columns[i][j];
			}
		}
	}

	struct Shaders {
		CanvasShaderGLES3 canvas_shader;
		SkyShaderGLES3 sky_shader;
		SceneShaderGLES3 scene_shader;
		ParticlesShaderGLES3 particles_process_shader;

		ShaderCompiler compiler_canvas;
		ShaderCompiler compiler_scene;
		ShaderCompiler compiler_particles;
		ShaderCompiler compiler_sky;
	} shaders;

	/* GLOBAL SHADER UNIFORM API */

	void _update_global_shader_uniforms();

	virtual void global_shader_parameter_add(const StringName &p_name, RS::GlobalShaderParameterType p_type, const Variant &p_value) override;
	virtual void global_shader_parameter_remove(const StringName &p_name) override;
	virtual Vector<StringName> global_shader_parameter_get_list() const override;

	virtual void global_shader_parameter_set(const StringName &p_name, const Variant &p_value) override;
	virtual void global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) override;
	virtual Variant global_shader_parameter_get(const StringName &p_name) const override;
	virtual RS::GlobalShaderParameterType global_shader_parameter_get_type(const StringName &p_name) const override;
	RS::GlobalShaderParameterType global_shader_parameter_get_type_internal(const StringName &p_name) const;

	virtual void global_shader_parameters_load_settings(bool p_load_textures = true) override;
	virtual void global_shader_parameters_clear() override;

	virtual int32_t global_shader_parameters_instance_allocate(RID p_instance) override;
	virtual void global_shader_parameters_instance_free(RID p_instance) override;
	virtual void global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count = 0) override;

	GLuint global_shader_parameters_get_uniform_buffer() const;

	/* SHADER API */

	Shader *get_shader(RID p_rid) { return shader_owner.get_or_null(p_rid); }
	bool owns_shader(RID p_rid) { return shader_owner.owns(p_rid); }

	void _shader_make_dirty(Shader *p_shader);

	virtual RID shader_allocate() override;
	virtual void shader_initialize(RID p_rid) override;
	virtual void shader_free(RID p_rid) override;

	virtual void shader_set_code(RID p_shader, const String &p_code) override;
	virtual void shader_set_path_hint(RID p_shader, const String &p_path) override;
	virtual String shader_get_code(RID p_shader) const override;
	virtual void get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const override;

	virtual void shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) override;
	virtual RID shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const override;
	virtual Variant shader_get_parameter_default(RID p_shader, const StringName &p_name) const override;

	virtual RS::ShaderNativeSourceCode shader_get_native_source_code(RID p_shader) const override;

	/* MATERIAL API */

	Material *get_material(RID p_rid) { return material_owner.get_or_null(p_rid); }
	bool owns_material(RID p_rid) { return material_owner.owns(p_rid); }

	void _material_queue_update(Material *material, bool p_uniform, bool p_texture);
	void _update_queued_materials();

	virtual RID material_allocate() override;
	virtual void material_initialize(RID p_rid) override;
	virtual void material_free(RID p_rid) override;

	virtual void material_set_shader(RID p_material, RID p_shader) override;

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) override;
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const override;

	virtual void material_set_next_pass(RID p_material, RID p_next_material) override;
	virtual void material_set_render_priority(RID p_material, int priority) override;

	virtual bool material_is_animated(RID p_material) override;
	virtual bool material_casts_shadows(RID p_material) override;
	virtual RS::CullMode material_get_cull_mode(RID p_material) const override;

	virtual void material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) override;

	virtual void material_update_dependency(RID p_material, DependencyTracker *p_instance) override;

	_FORCE_INLINE_ uint32_t material_get_shader_id(RID p_material) {
		Material *material = material_owner.get_or_null(p_material);
		return material->shader_id;
	}

	_FORCE_INLINE_ MaterialData *material_get_data(RID p_material, RS::ShaderMode p_shader_mode) {
		Material *material = material_owner.get_or_null(p_material);
		if (!material || material->shader_mode != p_shader_mode) {
			return nullptr;
		} else {
			return material->data;
		}
	}
};

} // namespace GLES3

#endif // GLES3_ENABLED

#endif // MATERIAL_STORAGE_GLES3_H
