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

#ifndef MATERIAL_STORAGE_DUMMY_H
#define MATERIAL_STORAGE_DUMMY_H

#include "servers/rendering/shader_compiler.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/storage/material_storage.h"
#include "servers/rendering/storage/utilities.h"

namespace RendererDummy {

class MaterialStorage : public RendererMaterialStorage {
private:
	static MaterialStorage *singleton;

	HashMap<StringName, RS::GlobalShaderParameterType> global_shader_variables;

	struct DummyShader {
		HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;
	};

	mutable RID_Owner<DummyShader> shader_owner;

	ShaderCompiler dummy_compiler;

	struct DummyMaterial {
		RID shader;
		RID next_pass;
	};

	mutable RID_Owner<DummyMaterial> material_owner;

public:
	static MaterialStorage *get_singleton() { return singleton; }

	MaterialStorage();
	~MaterialStorage();

	/* GLOBAL SHADER UNIFORM API */

	virtual void global_shader_parameter_add(const StringName &p_name, RS::GlobalShaderParameterType p_type, const Variant &p_value) override;
	virtual void global_shader_parameter_remove(const StringName &p_name) override;
	virtual Vector<StringName> global_shader_parameter_get_list() const override;

	virtual void global_shader_parameter_set(const StringName &p_name, const Variant &p_value) override {}
	virtual void global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) override {}
	virtual Variant global_shader_parameter_get(const StringName &p_name) const override { return Variant(); }
	virtual RS::GlobalShaderParameterType global_shader_parameter_get_type(const StringName &p_name) const override;

	virtual void global_shader_parameters_load_settings(bool p_load_textures = true) override;
	virtual void global_shader_parameters_clear() override {}

	virtual int32_t global_shader_parameters_instance_allocate(RID p_instance) override { return 0; }
	virtual void global_shader_parameters_instance_free(RID p_instance) override {}
	virtual void global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count = 0) override {}

	/* SHADER API */

	bool owns_shader(RID p_rid) { return shader_owner.owns(p_rid); }

	virtual RID shader_allocate() override;
	virtual void shader_initialize(RID p_rid) override;
	virtual void shader_free(RID p_rid) override;

	virtual void shader_set_code(RID p_shader, const String &p_code) override;
	virtual void shader_set_path_hint(RID p_shader, const String &p_code) override {}

	virtual String shader_get_code(RID p_shader) const override { return ""; }
	virtual void get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const override;

	virtual void shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) override {}
	virtual RID shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const override { return RID(); }
	virtual Variant shader_get_parameter_default(RID p_material, const StringName &p_param) const override { return Variant(); }

	virtual RS::ShaderNativeSourceCode shader_get_native_source_code(RID p_shader) const override { return RS::ShaderNativeSourceCode(); }

	/* MATERIAL API */

	bool owns_material(RID p_rid) { return material_owner.owns(p_rid); }

	virtual RID material_allocate() override;
	virtual void material_initialize(RID p_rid) override;
	virtual void material_free(RID p_rid) override;

	virtual void material_set_render_priority(RID p_material, int priority) override {}
	virtual void material_set_shader(RID p_shader_material, RID p_shader) override;

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) override {}
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const override { return Variant(); }

	virtual void material_set_next_pass(RID p_material, RID p_next_material) override;

	virtual bool material_is_animated(RID p_material) override { return false; }
	virtual bool material_casts_shadows(RID p_material) override { return false; }
	virtual RS::CullMode material_get_cull_mode(RID p_material) const override { return RS::CULL_MODE_DISABLED; }

	virtual void material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) override;
	virtual void material_update_dependency(RID p_material, DependencyTracker *p_instance) override {}
};

} // namespace RendererDummy

#endif // MATERIAL_STORAGE_DUMMY_H
