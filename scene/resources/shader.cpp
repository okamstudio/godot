/**************************************************************************/
/*  shader.cpp                                                            */
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

#include "shader.h"
#include "shader.compat.inc"

#include "core/io/file_access.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_preprocessor.h"
#include "servers/rendering_server.h"
#include "texture.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_help.h"

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif
#endif

Shader::Mode Shader::get_mode() const {
	return mode;
}

void Shader::_check_shader_rid() const {
	MutexLock lock(shader_rid_mutex);
	if (shader_rid.is_null() && !preprocessed_code.is_empty()) {
		shader_rid = RenderingServer::get_singleton()->shader_create_from_code(preprocessed_code, get_path());
		preprocessed_code = String();
	}
}

void Shader::_dependency_changed() {
	// Preprocess and compile the code again because a dependency has changed. It also calls emit_changed() for us.
	_recompile();
}

void Shader::_recompile() {
	set_code(get_code());
}

void Shader::set_path(const String &p_path, bool p_take_over) {
	Resource::set_path(p_path, p_take_over);

	if (shader_rid.is_valid()) {
		RS::get_singleton()->shader_set_path_hint(shader_rid, p_path);
	}
}

void Shader::set_include_path(const String &p_path) {
	// Used only if the shader does not have a resource path set,
	// for example during loading stage or when created by code.
	include_path = p_path;
}

void Shader::set_code(const String &p_code) {
	for (const Ref<ShaderInclude> &E : include_dependencies) {
		E->disconnect_changed(callable_mp(this, &Shader::_dependency_changed));
	}

	code = p_code;
	preprocessed_code = p_code;

	{
		String path = get_path();
		if (path.is_empty()) {
			path = include_path;
		}
		// Preprocessor must run here and not in the server because:
		// 1) Need to keep track of include dependencies at resource level
		// 2) Server does not do interaction with Resource filetypes, this is a scene level feature.
		HashSet<Ref<ShaderInclude>> new_include_dependencies;
		ShaderPreprocessor preprocessor;
		Error result = preprocessor.preprocess(p_code, path, preprocessed_code, nullptr, nullptr, nullptr, &new_include_dependencies);
		if (result == OK) {
			// This ensures previous include resources are not freed and then re-loaded during parse (which would make compiling slower)
			include_dependencies = new_include_dependencies;
		}
	}

	// Try to get the shader type from the final, fully preprocessed shader code.
	String type = ShaderLanguage::get_shader_type(preprocessed_code);

	if (type == "canvas_item") {
		mode = MODE_CANVAS_ITEM;
	} else if (type == "particles") {
		mode = MODE_PARTICLES;
	} else if (type == "sky") {
		mode = MODE_SKY;
	} else if (type == "fog") {
		mode = MODE_FOG;
	} else {
		mode = MODE_SPATIAL;
	}

	for (const Ref<ShaderInclude> &E : include_dependencies) {
		E->connect_changed(callable_mp(this, &Shader::_dependency_changed));
	}

	if (shader_rid.is_valid()) {
		RenderingServer::get_singleton()->shader_set_code(shader_rid, preprocessed_code);
		preprocessed_code = String();
	}

	emit_changed();
}

String Shader::get_code() const {
	_update_shader();
	return code;
}

void Shader::get_shader_uniform_list(List<PropertyInfo> *p_params, bool p_get_groups) const {
	_update_shader();
	_check_shader_rid();

	List<PropertyInfo> local;
	RenderingServer::get_singleton()->get_shader_parameter_list(shader_rid, &local);

#ifdef TOOLS_ENABLED
	DocData::ClassDoc class_doc;
	class_doc.name = get_path();
	class_doc.is_script_doc = true;
#endif

	for (PropertyInfo &pi : local) {
		bool is_group = pi.usage == PROPERTY_USAGE_GROUP || pi.usage == PROPERTY_USAGE_SUBGROUP;
		if (!p_get_groups && is_group) {
			continue;
		}
		if (!is_group) {
			if (default_textures.has(pi.name)) { //do not show default textures
				continue;
			}
		}
		if (p_params) {
			//small little hack
			if (pi.type == Variant::RID) {
				pi.type = Variant::OBJECT;
			}
#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint()) {
				DocData::PropertyDoc prop_doc;
				prop_doc.name = "shader_parameter/" + pi.name;
#ifdef MODULE_REGEX_ENABLED
				const RegEx pattern("/\\*\\*\\s([^*]|[\\r\\n]|(\\*+([^*/]|[\\r\\n])))*\\*+/\\s*uniform\\s+\\w+\\s+" + pi.name + "(?=[\\s:;=])");
				Ref<RegExMatch> pattern_ref = pattern.search(code);
				if (pattern_ref.is_valid()) {
					RegExMatch *match = pattern_ref.ptr();
					const RegEx pattern_tip("\\/\\*\\*([\\s\\S]*?)\\*/");
					Ref<RegExMatch> pattern_tip_ref = pattern_tip.search(match->get_string(0));
					RegExMatch *match_tip = pattern_tip_ref.ptr();
					const RegEx pattern_stripped("\\n\\s*\\*\\s*");
					prop_doc.description = pattern_stripped.sub(match_tip->get_string(1), "\n", true);
				}
#endif
				class_doc.properties.push_back(prop_doc);
			}
#endif
			p_params->push_back(pi);
		}
	}
#ifdef TOOLS_ENABLED
	if (EditorHelp::get_doc_data() != nullptr && Engine::get_singleton()->is_editor_hint() && !class_doc.name.is_empty() && p_params) {
		EditorHelp::get_doc_data()->add_doc(class_doc);
	}
#endif
}

RID Shader::get_rid() const {
	_update_shader();
	_check_shader_rid();

	return shader_rid;
}

void Shader::set_default_texture_parameter(const StringName &p_name, const Ref<Texture> &p_texture, int p_index) {
	_check_shader_rid();

	if (p_texture.is_valid()) {
		if (!default_textures.has(p_name)) {
			default_textures[p_name] = HashMap<int, Ref<Texture>>();
		}
		default_textures[p_name][p_index] = p_texture;
		RS::get_singleton()->shader_set_default_texture_parameter(shader_rid, p_name, p_texture->get_rid(), p_index);
	} else {
		if (default_textures.has(p_name) && default_textures[p_name].has(p_index)) {
			default_textures[p_name].erase(p_index);

			if (default_textures[p_name].is_empty()) {
				default_textures.erase(p_name);
			}
		}
		RS::get_singleton()->shader_set_default_texture_parameter(shader_rid, p_name, RID(), p_index);
	}

	emit_changed();
}

Ref<Texture> Shader::get_default_texture_parameter(const StringName &p_name, int p_index) const {
	if (default_textures.has(p_name) && default_textures[p_name].has(p_index)) {
		return default_textures[p_name][p_index];
	}
	return Ref<Texture2D>();
}

void Shader::get_default_texture_parameter_list(List<StringName> *r_textures) const {
	for (const KeyValue<StringName, HashMap<int, Ref<Texture>>> &E : default_textures) {
		r_textures->push_back(E.key);
	}
}

bool Shader::is_text_shader() const {
	return true;
}

void Shader::_update_shader() const {
	// Base implementation does nothing.
}

Array Shader::_get_shader_uniform_list(bool p_get_groups) {
	List<PropertyInfo> uniform_list;
	get_shader_uniform_list(&uniform_list, p_get_groups);
	Array ret;
	for (const PropertyInfo &pi : uniform_list) {
		ret.push_back(pi.operator Dictionary());
	}
	return ret;
}

void Shader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_mode"), &Shader::get_mode);

	ClassDB::bind_method(D_METHOD("set_code", "code"), &Shader::set_code);
	ClassDB::bind_method(D_METHOD("get_code"), &Shader::get_code);

	ClassDB::bind_method(D_METHOD("set_default_texture_parameter", "name", "texture", "index"), &Shader::set_default_texture_parameter, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_default_texture_parameter", "name", "index"), &Shader::get_default_texture_parameter, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_shader_uniform_list", "get_groups"), &Shader::_get_shader_uniform_list, DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_code", "get_code");

	BIND_ENUM_CONSTANT(MODE_SPATIAL);
	BIND_ENUM_CONSTANT(MODE_CANVAS_ITEM);
	BIND_ENUM_CONSTANT(MODE_PARTICLES);
	BIND_ENUM_CONSTANT(MODE_SKY);
	BIND_ENUM_CONSTANT(MODE_FOG);
}

Shader::Shader() {
	// Shader RID will be empty until it is required.
}

Shader::~Shader() {
	if (shader_rid.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free(shader_rid);
	}
}

////////////

Ref<Resource> ResourceFormatLoaderShader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error error = OK;
	Vector<uint8_t> buffer = FileAccess::get_file_as_bytes(p_path, &error);
	ERR_FAIL_COND_V_MSG(error, nullptr, "Cannot load shader: " + p_path);

	String str;
	if (buffer.size() > 0) {
		error = str.parse_utf8((const char *)buffer.ptr(), buffer.size());
		ERR_FAIL_COND_V_MSG(error, nullptr, "Cannot parse shader: " + p_path);
	}

	Ref<Shader> shader;
	shader.instantiate();

	shader->set_include_path(p_path);
	shader->set_code(str);

	if (r_error) {
		*r_error = OK;
	}

	return shader;
}

void ResourceFormatLoaderShader::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gdshader");
}

bool ResourceFormatLoaderShader::handles_type(const String &p_type) const {
	return (p_type == "Shader");
}

String ResourceFormatLoaderShader::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "gdshader") {
		return "Shader";
	}
	return "";
}

Error ResourceFormatSaverShader::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<Shader> shader = p_resource;
	ERR_FAIL_COND_V(shader.is_null(), ERR_INVALID_PARAMETER);

	String source = shader->get_code();

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

	ERR_FAIL_COND_V_MSG(err, err, "Cannot save shader '" + p_path + "'.");

	file->store_string(source);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

void ResourceFormatSaverShader::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (const Shader *shader = Object::cast_to<Shader>(*p_resource)) {
		if (shader->is_text_shader()) {
			p_extensions->push_back("gdshader");
		}
	}
}

bool ResourceFormatSaverShader::recognize(const Ref<Resource> &p_resource) const {
	return p_resource->get_class_name() == "Shader"; //only shader, not inherited
}
