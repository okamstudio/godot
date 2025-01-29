/**************************************************************************/
/*  material_storage.cpp                                                  */
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

#include "material_storage.h"

#include "core/config/project_settings.h"

using namespace RendererDummy;

MaterialStorage *MaterialStorage::singleton = nullptr;

MaterialStorage::MaterialStorage() {
	singleton = this;
	ShaderCompiler::DefaultIdentifierActions actions;
	dummy_compiler.initialize(actions);
}

MaterialStorage::~MaterialStorage() {
	singleton = nullptr;
	global_shader_variables.clear();
}

void MaterialStorage::global_shader_parameter_add(const StringName &p_name, RS::GlobalShaderParameterType p_type, const Variant &p_value) {
	ERR_FAIL_COND(global_shader_variables.has(p_name));

	global_shader_variables[p_name] = p_type;
}

void MaterialStorage::global_shader_parameter_remove(const StringName &p_name) {
	if (!global_shader_variables.has(p_name)) {
		return;
	}

	global_shader_variables.erase(p_name);
}

Vector<StringName> MaterialStorage::global_shader_parameter_get_list() const {
	Vector<StringName> names;
	for (const KeyValue<StringName, RS::GlobalShaderParameterType> &E : global_shader_variables) {
		names.push_back(E.key);
	}
	names.sort_custom<StringName::AlphCompare>();
	return names;
}

RS::GlobalShaderParameterType MaterialStorage::global_shader_parameter_get_type(const StringName &p_name) const {
	if (!global_shader_variables.has(p_name)) {
		print_line("don't have name, sorry");
		return RS::GLOBAL_VAR_TYPE_MAX;
	}

	return global_shader_variables[p_name];
}

void MaterialStorage::global_shader_parameters_load_settings(bool p_load_textures) {
	List<PropertyInfo> settings;
	ProjectSettings::get_singleton()->get_property_list(&settings);

	for (const PropertyInfo &E : settings) {
		if (E.name.begins_with("shader_globals/")) {
			StringName name = E.name.get_slice("/", 1);
			Dictionary d = GLOBAL_GET(E.name);

			ERR_CONTINUE(!d.has("type"));
			ERR_CONTINUE(!d.has("value"));

			String type = d["type"];

			static const char *global_var_type_names[RS::GLOBAL_VAR_TYPE_MAX] = {
				"bool",
				"bvec2",
				"bvec3",
				"bvec4",
				"int",
				"ivec2",
				"ivec3",
				"ivec4",
				"rect2i",
				"uint",
				"uvec2",
				"uvec3",
				"uvec4",
				"float",
				"vec2",
				"vec3",
				"vec4",
				"color",
				"rect2",
				"mat2",
				"mat3",
				"mat4",
				"transform_2d",
				"transform",
				"sampler2D",
				"sampler2DArray",
				"sampler3D",
				"samplerCube",
				"samplerExternalOES"
			};

			RS::GlobalShaderParameterType gvtype = RS::GLOBAL_VAR_TYPE_MAX;

			for (int i = 0; i < RS::GLOBAL_VAR_TYPE_MAX; i++) {
				if (global_var_type_names[i] == type) {
					gvtype = RS::GlobalShaderParameterType(i);
					break;
				}
			}

			ERR_CONTINUE(gvtype == RS::GLOBAL_VAR_TYPE_MAX); //type invalid

			if (!global_shader_variables.has(name)) {
				global_shader_parameter_add(name, gvtype, Variant());
			}
		}
	}
}

RID MaterialStorage::shader_allocate() {
	return shader_owner.allocate_rid();
}

void MaterialStorage::shader_initialize(RID p_rid) {
	shader_owner.initialize_rid(p_rid, DummyShader());
}

void MaterialStorage::shader_free(RID p_rid) {
	DummyShader *shader = shader_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(shader);

	shader_owner.free(p_rid);
}

void MaterialStorage::shader_set_code(RID p_shader, const String &p_code) {
	DummyShader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);
	if (p_code.is_empty()) {
		return;
	}

	String mode_string = ShaderLanguage::get_shader_type(p_code);

	RS::ShaderMode new_mode;
	if (mode_string == "canvas_item") {
		new_mode = RS::SHADER_CANVAS_ITEM;
	} else if (mode_string == "particles") {
		new_mode = RS::SHADER_PARTICLES;
	} else if (mode_string == "spatial") {
		new_mode = RS::SHADER_SPATIAL;
	} else if (mode_string == "sky") {
		new_mode = RS::SHADER_SKY;
	} else if (mode_string == "fog") {
		new_mode = RS::SHADER_FOG;
	} else {
		new_mode = RS::SHADER_MAX;
		ERR_FAIL_MSG("Shader type " + mode_string + " not supported in Dummy renderer.");
	}
	ShaderCompiler::IdentifierActions actions;
	actions.uniforms = &shader->uniforms;
	ShaderCompiler::GeneratedCode gen_code;

	Error err = MaterialStorage::get_singleton()->dummy_compiler.compile(new_mode, p_code, &actions, "", gen_code);
	ERR_FAIL_COND_MSG(err != OK, "Shader compilation failed.");
}

void MaterialStorage::get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
	DummyShader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);

	SortArray<Pair<StringName, int>, ShaderLanguage::UniformOrderComparator> sorter;
	LocalVector<Pair<StringName, int>> filtered_uniforms;

	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {
		if (E.value.scope != ShaderLanguage::ShaderNode::Uniform::SCOPE_LOCAL) {
			continue;
		}
		filtered_uniforms.push_back(Pair<StringName, int>(E.key, E.value.prop_order));
	}
	int uniform_count = filtered_uniforms.size();
	sorter.sort(filtered_uniforms.ptr(), uniform_count);

	String last_group;
	for (int i = 0; i < uniform_count; i++) {
		const StringName &uniform_name = filtered_uniforms[i].first;
		const ShaderLanguage::ShaderNode::Uniform &uniform = shader->uniforms[uniform_name];

		String group = uniform.group;
		if (!uniform.subgroup.is_empty()) {
			group += "::" + uniform.subgroup;
		}

		if (group != last_group) {
			PropertyInfo pi;
			pi.usage = PROPERTY_USAGE_GROUP;
			pi.name = group;
			p_param_list->push_back(pi);

			last_group = group;
		}

		PropertyInfo pi = ShaderLanguage::uniform_to_property_info(uniform);
		pi.name = uniform_name;
		p_param_list->push_back(pi);
	}
}

RID MaterialStorage::material_allocate() {
	return material_owner.allocate_rid();
}

void MaterialStorage::material_initialize(RID p_rid) {
	material_owner.initialize_rid(p_rid, DummyMaterial());
}

void MaterialStorage::material_free(RID p_rid) {
	DummyMaterial *material = material_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(material);

	material_owner.free(p_rid);
}

void MaterialStorage::material_set_shader(RID p_material, RID p_shader) {
	DummyMaterial *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);

	material->shader = p_shader;
}

void MaterialStorage::material_set_next_pass(RID p_material, RID p_next_material) {
	DummyMaterial *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);

	material->next_pass = p_next_material;
}

void MaterialStorage::material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) {
	DummyMaterial *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);
	DummyShader *shader = shader_owner.get_or_null(material->shader);

	if (shader) {
		for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {
			if (E.value.scope != ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
				continue;
			}

			RendererMaterialStorage::InstanceShaderParam p;
			p.info = ShaderLanguage::uniform_to_property_info(E.value);
			p.info.name = E.key; //supply name
			p.index = E.value.instance_index;
			p.default_value = ShaderLanguage::constant_value_to_variant(E.value.default_value, E.value.type, E.value.array_size, E.value.hint);
			r_parameters->push_back(p);
		}
	}
	if (material->next_pass.is_valid()) {
		material_get_instance_shader_parameters(material->next_pass, r_parameters);
	}
}
