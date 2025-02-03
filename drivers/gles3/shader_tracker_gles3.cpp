/**************************************************************************/
/*  shader_tracker_gles3.cpp                                              */
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

#include "shader_tracker_gles3.h"
#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/io/file_access_compressed.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/ustring.h"
#include "rasterizer_storage_gles3.h"

#include "servers/visual/visual_server_globals.h"

const String SPATIAL_LIST_FILE_NAME = "spatial.shaderlist";
const String CANVAS_LIST_FILE_NAME = "canvas.shaderlist";
const String PARTICLE_LIST_FILE_NAME = "particle.shaderlist";

const String SHADER_CODE_MARKER = "SHADER_CODE:";
const String LIST_CODE_SEPERATOR = "SHADER_LIST_START";

ShaderTrackerGLES3::ShaderTrackerGLES3() {
	spatial_shader_code = Map<uint32_t, String>();
	canvas_shader_code = Map<uint32_t, String>();
	particle_shader_code = Map<uint32_t, String>();

	used_spatial_shaders = Map<uint32_t, Map<uint32_t, String>>();
	used_canvas_shaders = Map<uint32_t, Map<uint32_t, String>>();
	used_particle_shaders = Map<uint32_t, Map<uint32_t, String>>();

	output_folder = OS::get_singleton()->get_executable_path().get_base_dir();
}

ShaderTrackerGLES3::~ShaderTrackerGLES3() {
	FileAccessCompressed *fa_spatial = memnew(FileAccessCompressed);
	fa_spatial->configure("GCPF"); //, (Compression::Mode)p_compress_mode);
	Error err = fa_spatial->_open(output_folder.plus_file(SPATIAL_LIST_FILE_NAME), FileAccess::WRITE);
	ERR_FAIL_COND_MSG(err, "ShaderTracker cannot open file '" + output_folder.plus_file(SPATIAL_LIST_FILE_NAME) + "'. Check user write permissions.");
	for (Map<uint32_t, String>::Element *e = spatial_shader_code.front(); e; e = e->next()) {
		fa_spatial->store_string(vformat("%s%d\n", SHADER_CODE_MARKER, e->key()));
		fa_spatial->store_string(e->value());
		fa_spatial->store_string("\n");
	}
	fa_spatial->store_line(LIST_CODE_SEPERATOR);
	for (Map<uint32_t, Map<uint32_t, String>>::Element *e = used_spatial_shaders.front(); e; e = e->next()) {
		for (Map<uint32_t, String>::Element *a = e->value().front(); a; a = a->next()) {
			fa_spatial->store_string(vformat("%d\n", e->key()));
			fa_spatial->store_string(a->value());
		}
	}
	memdelete(fa_spatial);

	FileAccessCompressed *fa_canvas = memnew(FileAccessCompressed);
	fa_canvas->configure("GCPF"); //, (Compression::Mode)p_compress_mode);
	err = fa_canvas->_open(output_folder.plus_file(CANVAS_LIST_FILE_NAME), FileAccess::WRITE);
	ERR_FAIL_COND_MSG(err, "ShaderTracker cannot open file '" + output_folder.plus_file(CANVAS_LIST_FILE_NAME) + "'. Check user write permissions.");
	for (Map<uint32_t, String>::Element *e = canvas_shader_code.front(); e; e = e->next()) {
		fa_canvas->store_string(vformat("%s%d\n", SHADER_CODE_MARKER, e->key()));
		fa_canvas->store_string(e->value());
		fa_canvas->store_string("\n");
	}
	fa_canvas->store_line(LIST_CODE_SEPERATOR);
	for (Map<uint32_t, Map<uint32_t, String>>::Element *e = used_canvas_shaders.front(); e; e = e->next()) {
		for (Map<uint32_t, String>::Element *a = e->value().front(); a; a = a->next()) {
			fa_canvas->store_string(vformat("%d\n", e->key()));
			fa_canvas->store_string(a->value());
		}
	}
	memdelete(fa_canvas);

	FileAccessCompressed *fa_particle = memnew(FileAccessCompressed);
	fa_particle->configure("GCPF"); //, (Compression::Mode)p_compress_mode);
	err = fa_particle->_open(output_folder.plus_file(PARTICLE_LIST_FILE_NAME), FileAccess::WRITE);
	ERR_FAIL_COND_MSG(err, "ShaderTracker cannot open file '" + output_folder.plus_file(PARTICLE_LIST_FILE_NAME) + "'. Check user write permissions.");
	for (Map<uint32_t, String>::Element *e = particle_shader_code.front(); e; e = e->next()) {
		fa_particle->store_string(vformat("%s%d\n", SHADER_CODE_MARKER, e->key()));
		fa_particle->store_string(e->value());
		fa_particle->store_string("\n");
	}
	fa_particle->store_line(LIST_CODE_SEPERATOR);
	for (Map<uint32_t, Map<uint32_t, String>>::Element *e = used_particle_shaders.front(); e; e = e->next()) {
		for (Map<uint32_t, String>::Element *a = e->value().front(); a; a = a->next()) {
			fa_particle->store_string(vformat("%d\n", e->key()));
			fa_particle->store_string(a->value());
		}
	}
	memdelete(fa_particle);
}

void ShaderTrackerGLES3::add_shader(VS::ShaderMode p_mode, const String &p_shader_code, ShaderCompilerGLES3::IdentifierActions *p_actions, uint64_t p_conditional_key) {
	Map<uint32_t, String> *shader_code_map;
	Map<uint32_t, Map<uint32_t, String>> *shader_action_map;

	if (p_mode == VS::ShaderMode::SHADER_SPATIAL) {
		shader_code_map = &spatial_shader_code;
		shader_action_map = &used_spatial_shaders;
		//_add_spatial_shader(p_shader_code, p_actions, p_conditional_key);
	} else if (p_mode == VS::ShaderMode::SHADER_CANVAS_ITEM) {
		shader_code_map = &canvas_shader_code;
		shader_action_map = &used_canvas_shaders;
	} else if (p_mode == VS::ShaderMode::SHADER_PARTICLES) {
		shader_code_map = &particle_shader_code;
		shader_action_map = &used_particle_shaders;
	} else {
		return;
	}

	uint32_t shader_hash = p_shader_code.hash();

	String serialized = _actions_to_strings(p_actions, p_conditional_key);
	uint32_t actions_hash = serialized.hash();

	if (!shader_action_map->has(shader_hash)) {
		shader_action_map->insert(shader_hash, Map<uint32_t, String>());
		shader_code_map->insert(shader_hash, p_shader_code);
	}
	Map<uint32_t, String> *actions_list = &(*shader_action_map)[shader_hash];

	if (actions_list->has(actions_hash)) {
		return;
	}

	actions_list->insert(actions_hash, serialized);
};

String ShaderTrackerGLES3::_strings_to_csv_string(const Vector<String> &p_input) {
	String line = "";
	int size = p_input.size();
	for (int i = 0; i < size; ++i) {
		String value = p_input[i];

		if (value.find("\n") != -1) {
			value = value.replace("\n", "");
		}
		if (value.find("\"") != -1 || value.find("\n") != -1) {
			value = "\"" + value.replace("\"", "\"\"") + "\"";
		}
		if (i < size - 1) {
			value += ";";
		}

		line += value;
	}
	return line;
}

String ShaderTrackerGLES3::_actions_to_strings(ShaderCompilerGLES3::IdentifierActions *p_actions, uint64_t p_conditional_key) {
	String result = "";

	Vector<String> render_mode_values;
	for (Map<StringName, Pair<int *, int>>::Element *e = p_actions->render_mode_values.front(); e; e = e->next()) {
		String tmp = vformat("%s=%d,%d", e->key(), *e->value().first, e->value().second);
		render_mode_values.push_back(tmp);
	}
	result += _strings_to_csv_string(render_mode_values) + "\n";

	Vector<String> render_mode_flags;
	for (Map<StringName, bool *>::Element *e = p_actions->render_mode_flags.front(); e; e = e->next()) {
		String tmp = vformat("%s=%d", e->key(), (int)*e->value());
		render_mode_flags.push_back(tmp);
	}
	result += _strings_to_csv_string(render_mode_flags) + "\n";

	Vector<String> usage_flag_pointers;
	for (Map<StringName, bool *>::Element *e = p_actions->usage_flag_pointers.front(); e; e = e->next()) {
		String tmp = vformat("%s=%d", e->key(), (int)*e->value());
		usage_flag_pointers.push_back(tmp);
	}
	result += _strings_to_csv_string(usage_flag_pointers) + "\n";

	Vector<String> write_flag_pointers;
	for (Map<StringName, bool *>::Element *e = p_actions->write_flag_pointers.front(); e; e = e->next()) {
		String tmp = vformat("%s=%d", e->key(), (int)*e->value());
		write_flag_pointers.push_back(tmp);
	}
	result += _strings_to_csv_string(write_flag_pointers) + "\n";
	result += vformat("%d\n", p_conditional_key);

	return result;
}

Error ShaderPreLoader::load_spatial(const String &p_file_path) {
	return _load(VS::ShaderMode::SHADER_SPATIAL, p_file_path);
}

Error ShaderPreLoader::load_canvas(const String &p_file_path) {
	return _load(VS::ShaderMode::SHADER_CANVAS_ITEM, p_file_path);
}

Error ShaderPreLoader::load_particle(const String &p_file_path) {
	return _load(VS::ShaderMode::SHADER_PARTICLES, p_file_path);
}

bool ShaderPreLoader::compiling = false;
int ShaderPreLoader::spatial_count = 0;
int ShaderPreLoader::canvas_count = 0;
int ShaderPreLoader::particle_count = 0;
int ShaderPreLoader::progress = 0;

Map<uint32_t, String> ShaderPreLoader::spatial_shader_code = Map<uint32_t, String>();
Vector<ShaderPreLoader::ShaderPermutation> ShaderPreLoader::spatial_shaders = Vector<ShaderPreLoader::ShaderPermutation>();
Map<uint32_t, String> ShaderPreLoader::canvas_shader_code = Map<uint32_t, String>();
Vector<ShaderPreLoader::ShaderPermutation> ShaderPreLoader::canvas_shaders = Vector<ShaderPreLoader::ShaderPermutation>();
Map<uint32_t, String> ShaderPreLoader::particle_shader_code = Map<uint32_t, String>();
Vector<ShaderPreLoader::ShaderPermutation> ShaderPreLoader::particle_shaders = Vector<ShaderPreLoader::ShaderPermutation>();

Error ShaderPreLoader::_load(VS::ShaderMode p_mode, const String &p_file_path) {
	if (compiling) {
		print_error("Can not load new shaderlists while already compiling. Please wait for the current compilation to finish!");
		return Error::ERR_BUSY;
	}

	Map<uint32_t, String> *code_map;
	Vector<ShaderPreLoader::ShaderPermutation> *permutations;
	int *count;
	if (p_mode == VS::ShaderMode::SHADER_SPATIAL) {
		code_map = &spatial_shader_code;
		permutations = &spatial_shaders;
		count = &spatial_count;
	} else if (p_mode == VS::ShaderMode::SHADER_CANVAS_ITEM) {
		code_map = &canvas_shader_code;
		permutations = &canvas_shaders;
		count = &canvas_count;
	} else if (p_mode == VS::ShaderMode::SHADER_PARTICLES) {
		code_map = &particle_shader_code;
		permutations = &particle_shaders;
		count = &particle_count;
	} else {
		return Error::ERR_INVALID_PARAMETER;
	}
	progress = 0;
	code_map->clear();
	permutations->clear();

	FileAccessCompressed *fa = memnew(FileAccessCompressed);
	fa->configure("GCPF");
	Error err = fa->_open(p_file_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(err, err, "ShaderPreLoader cannot open file '" + p_file_path + "'. Check user read permissions & the file location.");

	//Load shader code
	uint32_t current_shader_id = 0;
	String current_shader_code = "";
	for (String line = fa->get_line(); !fa->eof_reached(); line = fa->get_line()) {
		if (line.begins_with(SHADER_CODE_MARKER)) {
			// Start of a new shader
			if (current_shader_id) {
				//store prev shader
				current_shader_code = current_shader_code.substr(0, current_shader_code.length() - 1);
				//current_shader_code = "\n" + current_shader_code;
				code_map->insert(current_shader_id, current_shader_code);

				if (current_shader_id != current_shader_code.hash()) {
					print_error("Loaded code hash doesn't match stored hash!");
				}
			}
			current_shader_id = line.split(":")[1].to_int64();
			current_shader_code = "";
		} else if (line.begins_with(LIST_CODE_SEPERATOR)) {
			break;
		} else {
			current_shader_code += line + "\n";
		}
	}
	code_map->insert(current_shader_id, current_shader_code);

	//Load shader permutations
	*count = 0;
	while (!fa->eof_reached()) {
		String id = fa->get_line();
		String render_mode_values = fa->get_line();
		String render_mode_flags = fa->get_line();
		String usage_flag_pointers = fa->get_line();
		String write_flag_pointers = fa->get_line();
		uint64_t conditional_key = fa->get_line().to_int64();

		uint32_t id_number = id.to_int64();

		if (!id_number)
			break;

		if (!code_map->has(id_number)) {
			print_error(vformat("Shaderlist contains a shader id (\"%s\") that is not stored in shadercode!", id));
			break;
		}
		*count = *count + 1;

		String settings = render_mode_values + "\n" + render_mode_flags + "\n" + usage_flag_pointers + "\n" + write_flag_pointers;
		permutations->push_back(ShaderPermutation{
				id_number, settings, conditional_key });
	}

	memdelete(fa);

	return Error::OK;
}

void ShaderPreLoader::start() {
	if (!compiling) {
		progress = 0;
		compiling = true;
	}
}

bool ShaderPreLoader::is_running() const {
	return compiling;
}

void ShaderPreLoader::compile() {
	if (!compiling) {
		return;
	}

	if (progress < spatial_count) {
		ShaderPermutation perm = spatial_shaders.get(progress);
		String code = spatial_shader_code[perm.code];
		_compile_shader(VisualServer::ShaderMode::SHADER_SPATIAL, perm, code);
	} else if (progress < spatial_count + canvas_count) {
		ShaderPermutation perm = canvas_shaders.get(progress - spatial_count);
		String code = canvas_shader_code[perm.code];
		_compile_shader(VisualServer::ShaderMode::SHADER_CANVAS_ITEM, perm, code);
	} else if (progress < spatial_count + canvas_count + particle_count) {
		ShaderPermutation perm = particle_shaders.get(progress - spatial_count - canvas_count);
		String code = particle_shader_code[perm.code];
		_compile_shader(VisualServer::ShaderMode::SHADER_PARTICLES, perm, code);
	} else {
		compiling = false;
	}

	progress++;
}

void ShaderPreLoader::_compile_shader(VisualServer::ShaderMode p_shader_mode, const ShaderPermutation &p_perm, const String &p_code) {
	List<int> int_value_storage;
	Vector<String> lines = p_perm.actions.split("\n");
	Map<StringName, Pair<int *, int>> render_mode_values;
	Map<StringName, bool *> render_mode_flags;
	Map<StringName, bool *> usage_flag_pointers;
	Map<StringName, bool *> write_flag_pointers;
	Map<StringName, ShaderLanguage::ShaderNode::Uniform> uniforms;

	deserialize_pair_map(render_mode_values, lines[0], int_value_storage);
	deserialize_bool_map(render_mode_flags, lines[1], int_value_storage);
	deserialize_bool_map(usage_flag_pointers, lines[2], int_value_storage);
	deserialize_bool_map(write_flag_pointers, lines[3], int_value_storage);

	ShaderCompilerGLES3::IdentifierActions actions = {
		render_mode_values,
		render_mode_flags,
		usage_flag_pointers,
		write_flag_pointers,
		&uniforms
	};

	VisualServer *vs = VisualServer::get_singleton();
	ShaderCompilerGLES3 compiler;
	ShaderCompilerGLES3::GeneratedCode gen_code;

	RID shader_rid = vs->shader_create();

	//visual_server_globals.h
	RasterizerStorageGLES3::Shader *shader = ((RasterizerStorageGLES3 *)VSG::storage)->shader_owner.get(shader_rid);

	vs->shader_set_code(shader_rid, p_code);

	shader->shader->set_conditional_version(p_perm.conditional_key);

	Error err = compiler.compile(p_shader_mode, p_code, &actions, "", gen_code);
	ERR_FAIL_COND_MSG(err, "Failed to compile shader code!");

	shader->shader->set_custom_shader_code(shader->custom_code_id, gen_code.vertex, gen_code.vertex_global, gen_code.fragment, gen_code.light, gen_code.fragment_global, gen_code.uniforms, gen_code.texture_uniforms, gen_code.defines, ShaderGLES3::AsyncMode::ASYNC_MODE_HIDDEN);
	shader->shader->bind();
}

int ShaderPreLoader::get_stage() const {
	return progress;
}

int ShaderPreLoader::get_stage_count() const {
	return spatial_count + canvas_count + particle_count;
}

void ShaderPreLoader::deserialize_pair_map(Map<StringName, Pair<int *, int>> &p_render_mode_values, String p_string, List<int> &p_int_value_storage) {
	if (p_string.length() <= 1) {
		return;
	}

	Vector<String> splits = p_string.split(";");
	for (int i = 0; i < splits.size(); i++) {
		String split = splits[i];
		Vector<String> tmp = split.split("=");
		String key = tmp[0];
		Vector<String> values = tmp[1].split(",");

		p_int_value_storage.push_back(values[0].to_int());

		Pair<int *, int> a = Pair<int *, int>(&p_int_value_storage.back()->get(), values[1].to_int());
		p_render_mode_values.insert(key, a);
	}
}

void ShaderPreLoader::deserialize_bool_map(Map<StringName, bool *> &p_render_mode_values, String p_string, List<int> &p_int_value_storage) {
	if (p_string.length() <= 1) {
		return;
	}

	Vector<String> splits = p_string.split(";");
	for (int i = 0; i < splits.size(); i++) {
		String split = splits[i];
		Vector<String> tmp = split.split("=");
		String key = tmp[0];

		p_int_value_storage.push_back(tmp[1].to_int());

		bool *value = (bool *)&p_int_value_storage.back()->get();
		p_render_mode_values.insert(key, value);
	}
}
