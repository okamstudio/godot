/**************************************************************************/
/*  shader_rd.cpp                                                         */
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

#include "shader_rd.h"

#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/worker_thread_pool.h"
#include "core/version.h"
#include "renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/shader_include_db.h"
#include "thirdparty/misc/smolv.h"

#define ENABLE_SHADER_CACHE 1

void ShaderRD::_add_stage(const char *p_code, StageType p_stage_type) {
	Vector<String> lines = String(p_code).split("\n");

	String text;

	int line_count = lines.size();
	for (int i = 0; i < line_count; i++) {
		const String &l = lines[i];
		bool push_chunk = false;

		StageTemplate::Chunk chunk;

		if (l.begins_with("#VERSION_DEFINES")) {
			chunk.type = StageTemplate::Chunk::TYPE_VERSION_DEFINES;
			push_chunk = true;
		} else if (l.begins_with("#GLOBALS")) {
			switch (p_stage_type) {
				case STAGE_TYPE_VERTEX:
					chunk.type = StageTemplate::Chunk::TYPE_VERTEX_GLOBALS;
					break;
				case STAGE_TYPE_FRAGMENT:
					chunk.type = StageTemplate::Chunk::TYPE_FRAGMENT_GLOBALS;
					break;
				case STAGE_TYPE_COMPUTE:
					chunk.type = StageTemplate::Chunk::TYPE_COMPUTE_GLOBALS;
					break;
				default: {
				}
			}

			push_chunk = true;
		} else if (l.begins_with("#MATERIAL_UNIFORMS")) {
			chunk.type = StageTemplate::Chunk::TYPE_MATERIAL_UNIFORMS;
			push_chunk = true;
		} else if (l.begins_with("#CODE")) {
			chunk.type = StageTemplate::Chunk::TYPE_CODE;
			push_chunk = true;
			chunk.code = l.replace_first("#CODE", String()).replace(":", "").strip_edges().to_upper();
		} else if (l.begins_with("#include ")) {
			String include_file = l.replace("#include ", "").strip_edges();
			if (include_file[0] == '"') {
				int end_pos = include_file.find_char('"', 1);
				if (end_pos >= 0) {
					include_file = include_file.substr(1, end_pos - 1);

					String include_code = ShaderIncludeDB::get_built_in_include_file(include_file);
					if (!include_code.is_empty()) {
						// Add these lines into our parse list so we parse them as well.
						Vector<String> include_lines = include_code.split("\n");

						for (int j = include_lines.size() - 1; j >= 0; j--) {
							lines.insert(i + 1, include_lines[j]);
						}

						line_count = lines.size();
					} else {
						// Add it in as is.
						text += l + "\n";
					}
				} else {
					// Add it in as is.
					text += l + "\n";
				}
			} else {
				// Add it in as is.
				text += l + "\n";
			}
		} else {
			text += l + "\n";
		}

		if (push_chunk) {
			if (!text.is_empty()) {
				StageTemplate::Chunk text_chunk;
				text_chunk.type = StageTemplate::Chunk::TYPE_TEXT;
				text_chunk.text = text.utf8();
				stage_templates[p_stage_type].chunks.push_back(text_chunk);
				text = String();
			}
			stage_templates[p_stage_type].chunks.push_back(chunk);
		}
	}

	if (!text.is_empty()) {
		StageTemplate::Chunk text_chunk;
		text_chunk.type = StageTemplate::Chunk::TYPE_TEXT;
		text_chunk.text = text.utf8();
		stage_templates[p_stage_type].chunks.push_back(text_chunk);
		text = String();
	}
}

void ShaderRD::setup(const char *p_vertex_code, const char *p_fragment_code, const char *p_compute_code, const char *p_name) {
	name = p_name;

	if (p_compute_code) {
		_add_stage(p_compute_code, STAGE_TYPE_COMPUTE);
		is_compute = true;
	} else {
		is_compute = false;
		if (p_vertex_code) {
			_add_stage(p_vertex_code, STAGE_TYPE_VERTEX);
		}
		if (p_fragment_code) {
			_add_stage(p_fragment_code, STAGE_TYPE_FRAGMENT);
		}
	}

	StringBuilder tohash;
	tohash.append("[GodotVersionNumber]");
	tohash.append(VERSION_NUMBER);
	tohash.append("[GodotVersionHash]");
	tohash.append(VERSION_HASH);
	tohash.append("[SpirvCacheKey]");
	tohash.append(RenderingDevice::get_singleton()->shader_get_spirv_cache_key());
	tohash.append("[BinaryCacheKey]");
	tohash.append(RenderingDevice::get_singleton()->shader_get_binary_cache_key());
	tohash.append("[Vertex]");
	tohash.append(p_vertex_code ? p_vertex_code : "");
	tohash.append("[Fragment]");
	tohash.append(p_fragment_code ? p_fragment_code : "");
	tohash.append("[Compute]");
	tohash.append(p_compute_code ? p_compute_code : "");

	base_sha256 = tohash.as_string().sha256_text();
}

RID ShaderRD::version_create() {
	//initialize() was never called
	ERR_FAIL_COND_V(group_to_variant_map.is_empty(), RID());

	Version version;
	version.dirty = true;
	version.valid = false;
	version.initialize_needed = true;
	version.variants.clear();
	version.variant_data.clear();
	return version_owner.make_rid(version);
}

void ShaderRD::_initialize_version(Version *p_version) {
	_clear_version(p_version);

	p_version->valid = false;
	p_version->dirty = false;

	p_version->variants.resize_zeroed(variant_defines.size());
	p_version->variant_data.resize(variant_defines.size());
	p_version->group_compilation_tasks.resize(group_enabled.size());
	p_version->group_compilation_tasks.fill(0);
}

void ShaderRD::_clear_version(Version *p_version) {
	_compile_ensure_finished(p_version);

	// Clear versions if they exist.
	if (!p_version->variants.is_empty()) {
		for (int i = 0; i < variant_defines.size(); i++) {
			if (p_version->variants[i].is_valid()) {
				RD::get_singleton()->free(p_version->variants[i]);
			}
		}

		p_version->variants.clear();
		p_version->variant_data.clear();
	}
}

void ShaderRD::_build_variant_code(StringBuilder &builder, uint32_t p_variant, const Version *p_version, const StageTemplate &p_template) {
	for (const StageTemplate::Chunk &chunk : p_template.chunks) {
		switch (chunk.type) {
			case StageTemplate::Chunk::TYPE_VERSION_DEFINES: {
				builder.append("\n"); //make sure defines begin at newline
				builder.append(general_defines.get_data());
				builder.append(variant_defines[p_variant].text.get_data());
				for (int j = 0; j < p_version->custom_defines.size(); j++) {
					builder.append(p_version->custom_defines[j].get_data());
				}
				builder.append("\n"); //make sure defines begin at newline
				if (p_version->uniforms.size()) {
					builder.append("#define MATERIAL_UNIFORMS_USED\n");
				}
				for (const KeyValue<StringName, CharString> &E : p_version->code_sections) {
					builder.append(String("#define ") + String(E.key) + "_CODE_USED\n");
				}
#if (defined(MACOS_ENABLED) || defined(IOS_ENABLED))
				if (RD::get_singleton()->get_device_capabilities().device_family == RDD::DEVICE_VULKAN) {
					builder.append("#define MOLTENVK_USED\n");
				}
				// Image atomics are supported on Metal 3.1 but no support in MoltenVK or SPIRV-Cross yet.
				builder.append("#define NO_IMAGE_ATOMICS\n");
#endif

				builder.append(String("#define RENDER_DRIVER_") + OS::get_singleton()->get_current_rendering_driver_name().to_upper() + "\n");
				builder.append("#define samplerExternalOES sampler2D\n");
				builder.append("#define textureExternalOES texture2D\n");
			} break;
			case StageTemplate::Chunk::TYPE_MATERIAL_UNIFORMS: {
				builder.append(p_version->uniforms.get_data()); //uniforms (same for vertex and fragment)
			} break;
			case StageTemplate::Chunk::TYPE_VERTEX_GLOBALS: {
				builder.append(p_version->vertex_globals.get_data()); // vertex globals
			} break;
			case StageTemplate::Chunk::TYPE_FRAGMENT_GLOBALS: {
				builder.append(p_version->fragment_globals.get_data()); // fragment globals
			} break;
			case StageTemplate::Chunk::TYPE_COMPUTE_GLOBALS: {
				builder.append(p_version->compute_globals.get_data()); // compute globals
			} break;
			case StageTemplate::Chunk::TYPE_CODE: {
				if (p_version->code_sections.has(chunk.code)) {
					builder.append(p_version->code_sections[chunk.code].get_data());
				}
			} break;
			case StageTemplate::Chunk::TYPE_TEXT: {
				builder.append(chunk.text.get_data());
			} break;
		}
	}
}

void ShaderRD::_compile_variant(uint32_t p_variant, CompileData p_data) {
	uint32_t variant = group_to_variant_map[p_data.group][p_variant];

	if (!variants_enabled[variant]) {
		return; // Variant is disabled, return.
	}

	Vector<RD::ShaderStageSPIRVData> stages;

	String error;
	String current_source;
	RD::ShaderStage current_stage = RD::SHADER_STAGE_VERTEX;
	bool build_ok = true;

	if (!is_compute) {
		//vertex stage

		StringBuilder builder;
		_build_variant_code(builder, variant, p_data.version, stage_templates[STAGE_TYPE_VERTEX]);

		current_source = builder.as_string();
		RD::ShaderStageSPIRVData stage;
		stage.spirv = RD::get_singleton()->shader_compile_spirv_from_source(RD::SHADER_STAGE_VERTEX, current_source, RD::SHADER_LANGUAGE_GLSL, &error);
		if (stage.spirv.size() == 0) {
			build_ok = false;
		} else {
			stage.shader_stage = RD::SHADER_STAGE_VERTEX;
			stages.push_back(stage);
		}
	}

	if (!is_compute && build_ok) {
		//fragment stage
		current_stage = RD::SHADER_STAGE_FRAGMENT;

		StringBuilder builder;
		_build_variant_code(builder, variant, p_data.version, stage_templates[STAGE_TYPE_FRAGMENT]);

		current_source = builder.as_string();
		RD::ShaderStageSPIRVData stage;
		stage.spirv = RD::get_singleton()->shader_compile_spirv_from_source(RD::SHADER_STAGE_FRAGMENT, current_source, RD::SHADER_LANGUAGE_GLSL, &error);
		if (stage.spirv.size() == 0) {
			build_ok = false;
		} else {
			stage.shader_stage = RD::SHADER_STAGE_FRAGMENT;
			stages.push_back(stage);
		}
	}

	if (is_compute) {
		//compute stage
		current_stage = RD::SHADER_STAGE_COMPUTE;

		StringBuilder builder;
		_build_variant_code(builder, variant, p_data.version, stage_templates[STAGE_TYPE_COMPUTE]);

		current_source = builder.as_string();

		RD::ShaderStageSPIRVData stage;
		stage.spirv = RD::get_singleton()->shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, current_source, RD::SHADER_LANGUAGE_GLSL, &error);
		if (stage.spirv.size() == 0) {
			build_ok = false;
		} else {
			stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
			stages.push_back(stage);
		}
	}

	if (!build_ok) {
		MutexLock lock(variant_set_mutex); //properly print the errors
		ERR_PRINT("Error compiling " + String(current_stage == RD::SHADER_STAGE_COMPUTE ? "Compute " : (current_stage == RD::SHADER_STAGE_VERTEX ? "Vertex" : "Fragment")) + " shader, variant #" + itos(variant) + " (" + variant_defines[variant].text.get_data() + ").");
		ERR_PRINT(error);

#ifdef DEBUG_ENABLED
		ERR_PRINT("code:\n" + current_source.get_with_code_lines());
#endif
		return;
	}

	Vector<uint8_t> shader_data = RD::get_singleton()->shader_compile_binary_from_spirv(stages, name + ":" + itos(variant));

	ERR_FAIL_COND(shader_data.is_empty());

	{
		MutexLock lock(variant_set_mutex);

		p_data.version->variants.write[variant] = RD::get_singleton()->shader_create_from_bytecode_with_samplers(shader_data, p_data.version->variants[variant], immutable_samplers);
		p_data.version->variant_data.write[variant] = shader_data;
	}
}

RS::ShaderNativeSourceCode ShaderRD::version_get_native_source_code(RID p_version) {
	Version *version = version_owner.get_or_null(p_version);
	RS::ShaderNativeSourceCode source_code;
	ERR_FAIL_NULL_V(version, source_code);

	source_code.versions.resize(variant_defines.size());

	for (int i = 0; i < source_code.versions.size(); i++) {
		if (!is_compute) {
			//vertex stage

			StringBuilder builder;
			_build_variant_code(builder, i, version, stage_templates[STAGE_TYPE_VERTEX]);

			RS::ShaderNativeSourceCode::Version::Stage stage;
			stage.name = "vertex";
			stage.code = builder.as_string();

			source_code.versions.write[i].stages.push_back(stage);
		}

		if (!is_compute) {
			//fragment stage

			StringBuilder builder;
			_build_variant_code(builder, i, version, stage_templates[STAGE_TYPE_FRAGMENT]);

			RS::ShaderNativeSourceCode::Version::Stage stage;
			stage.name = "fragment";
			stage.code = builder.as_string();

			source_code.versions.write[i].stages.push_back(stage);
		}

		if (is_compute) {
			//compute stage

			StringBuilder builder;
			_build_variant_code(builder, i, version, stage_templates[STAGE_TYPE_COMPUTE]);

			RS::ShaderNativeSourceCode::Version::Stage stage;
			stage.name = "compute";
			stage.code = builder.as_string();

			source_code.versions.write[i].stages.push_back(stage);
		}
	}

	return source_code;
}

String ShaderRD::_version_get_sha1(Version *p_version) const {
	StringBuilder hash_build;

	hash_build.append("[uniforms]");
	hash_build.append(p_version->uniforms.get_data());
	hash_build.append("[vertex_globals]");
	hash_build.append(p_version->vertex_globals.get_data());
	hash_build.append("[fragment_globals]");
	hash_build.append(p_version->fragment_globals.get_data());
	hash_build.append("[compute_globals]");
	hash_build.append(p_version->compute_globals.get_data());

	Vector<StringName> code_sections;
	for (const KeyValue<StringName, CharString> &E : p_version->code_sections) {
		code_sections.push_back(E.key);
	}
	code_sections.sort_custom<StringName::AlphCompare>();

	for (int i = 0; i < code_sections.size(); i++) {
		hash_build.append(String("[code:") + String(code_sections[i]) + "]");
		hash_build.append(p_version->code_sections[code_sections[i]].get_data());
	}
	for (int i = 0; i < p_version->custom_defines.size(); i++) {
		hash_build.append("[custom_defines:" + itos(i) + "]");
		hash_build.append(p_version->custom_defines[i].get_data());
	}

	return hash_build.as_string().sha1_text();
}

static const char *shader_file_header = "GDSC";
static const uint32_t cache_file_version = 3;

String ShaderRD::_get_cache_file_path(Version *p_version, int p_group) {
	const String &sha1 = _version_get_sha1(p_version);
	const String &api_safe_name = String(RD::get_singleton()->get_device_api_name()).validate_filename().to_lower();
	const String &path = shader_cache_dir.path_join(name).path_join(group_sha256[p_group]).path_join(sha1) + "." + api_safe_name + ".cache";
	return path;
}

bool ShaderRD::_load_from_cache(Version *p_version, int p_group) {
	const String &path = _get_cache_file_path(p_version, p_group);
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	char header[5] = { 0, 0, 0, 0, 0 };
	f->get_buffer((uint8_t *)header, 4);
	ERR_FAIL_COND_V(header != String(shader_file_header), false);

	uint32_t file_version = f->get_32();
	if (file_version != cache_file_version) {
		return false; // wrong version
	}

	uint32_t variant_count = f->get_32();

	ERR_FAIL_COND_V(variant_count != (uint32_t)group_to_variant_map[p_group].size(), false); //should not happen but check

	for (uint32_t i = 0; i < variant_count; i++) {
		int variant_id = group_to_variant_map[p_group][i];
		uint32_t variant_size = f->get_32();
		ERR_FAIL_COND_V(variant_size == 0 && variants_enabled[variant_id], false);
		if (!variants_enabled[variant_id]) {
			continue;
		}
		Vector<uint8_t> variant_bytes;
		variant_bytes.resize(variant_size);

		uint32_t br = f->get_buffer(variant_bytes.ptrw(), variant_size);

		ERR_FAIL_COND_V(br != variant_size, false);

		p_version->variant_data.write[variant_id] = variant_bytes;
	}

	for (uint32_t i = 0; i < variant_count; i++) {
		int variant_id = group_to_variant_map[p_group][i];
		if (!variants_enabled[variant_id]) {
			MutexLock lock(variant_set_mutex);
			p_version->variants.write[variant_id] = RID();
			continue;
		}
		{
			MutexLock lock(variant_set_mutex);
			RID shader = RD::get_singleton()->shader_create_from_bytecode_with_samplers(p_version->variant_data[variant_id], p_version->variants[variant_id], immutable_samplers);
			if (shader.is_null()) {
				for (uint32_t j = 0; j < i; j++) {
					int variant_free_id = group_to_variant_map[p_group][j];
					RD::get_singleton()->free(p_version->variants[variant_free_id]);
				}
				ERR_FAIL_COND_V(shader.is_null(), false);
			}

			p_version->variants.write[variant_id] = shader;
		}
	}

	p_version->valid = true;
	return true;
}

void ShaderRD::_save_to_cache(Version *p_version, int p_group) {
	ERR_FAIL_COND(!shader_cache_dir_valid);
	const String &path = _get_cache_file_path(p_version, p_group);
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	ERR_FAIL_COND(f.is_null());
	f->store_buffer((const uint8_t *)shader_file_header, 4);
	f->store_32(cache_file_version); // File version.
	uint32_t variant_count = group_to_variant_map[p_group].size();
	f->store_32(variant_count); // Variant count.
	for (uint32_t i = 0; i < variant_count; i++) {
		int variant_id = group_to_variant_map[p_group][i];
		f->store_32(p_version->variant_data[variant_id].size()); // Stage count.
		f->store_buffer(p_version->variant_data[variant_id].ptr(), p_version->variant_data[variant_id].size());
	}
}

void ShaderRD::_allocate_placeholders(Version *p_version, int p_group) {
	ERR_FAIL_COND(p_version->variants.is_empty());

	for (uint32_t i = 0; i < group_to_variant_map[p_group].size(); i++) {
		int variant_id = group_to_variant_map[p_group][i];
		RID shader = RD::get_singleton()->shader_create_placeholder();
		{
			MutexLock lock(variant_set_mutex);
			p_version->variants.write[variant_id] = shader;
		}
	}
}

// Try to compile all variants for a given group.
// Will skip variants that are disabled.
void ShaderRD::_compile_version_start(Version *p_version, int p_group) {
	if (!group_enabled[p_group]) {
		return;
	}

	p_version->dirty = false;

#if ENABLE_SHADER_CACHE
	if (shader_cache_dir_valid) {
		if (_load_from_cache(p_version, p_group)) {
			return;
		}
	}
#endif

	CompileData compile_data;
	compile_data.version = p_version;
	compile_data.group = p_group;

	WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &ShaderRD::_compile_variant, compile_data, group_to_variant_map[p_group].size(), -1, true, SNAME("ShaderCompilation"));
	p_version->group_compilation_tasks.write[p_group] = group_task;
}

void ShaderRD::_compile_version_end(Version *p_version, int p_group) {
	if (p_version->group_compilation_tasks.size() <= p_group || p_version->group_compilation_tasks[p_group] == 0) {
		return;
	}

	WorkerThreadPool::GroupID group_task = p_version->group_compilation_tasks[p_group];
	WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
	p_version->group_compilation_tasks.write[p_group] = 0;

	bool all_valid = true;

	for (uint32_t i = 0; i < group_to_variant_map[p_group].size(); i++) {
		int variant_id = group_to_variant_map[p_group][i];
		if (!variants_enabled[variant_id]) {
			continue; // Disabled.
		}
		if (p_version->variants[variant_id].is_null()) {
			all_valid = false;
			break;
		}
	}

	if (!all_valid) {
		// Clear versions if they exist.
		for (int i = 0; i < variant_defines.size(); i++) {
			if (!variants_enabled[i] || !group_enabled[variant_defines[i].group]) {
				continue; // Disabled.
			}
			if (!p_version->variants[i].is_null()) {
				RD::get_singleton()->free(p_version->variants[i]);
			}
		}

		p_version->variants.clear();
		p_version->variant_data.clear();
		return;
	}
#if ENABLE_SHADER_CACHE
	else if (shader_cache_dir_valid) {
		_save_to_cache(p_version, p_group);
	}
#endif

	p_version->valid = true;
}

void ShaderRD::_compile_ensure_finished(Version *p_version) {
	// Wait for compilation of existing groups if necessary.
	for (int i = 0; i < group_enabled.size(); i++) {
		_compile_version_end(p_version, i);
	}
}

void ShaderRD::version_set_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_vertex_globals, const String &p_fragment_globals, const Vector<String> &p_custom_defines) {
	ERR_FAIL_COND(is_compute);

	Version *version = version_owner.get_or_null(p_version);
	ERR_FAIL_NULL(version);

	_compile_ensure_finished(version);

	version->vertex_globals = p_vertex_globals.utf8();
	version->fragment_globals = p_fragment_globals.utf8();
	version->uniforms = p_uniforms.utf8();
	version->code_sections.clear();
	for (const KeyValue<String, String> &E : p_code) {
		version->code_sections[StringName(E.key.to_upper())] = E.value.utf8();
	}

	version->custom_defines.clear();
	for (int i = 0; i < p_custom_defines.size(); i++) {
		version->custom_defines.push_back(p_custom_defines[i].utf8());
	}

	version->dirty = true;
	if (version->initialize_needed) {
		_initialize_version(version);
		for (int i = 0; i < group_enabled.size(); i++) {
			if (!group_enabled[i]) {
				_allocate_placeholders(version, i);
				continue;
			}
			_compile_version_start(version, i);
		}
		version->initialize_needed = false;
	}
}

void ShaderRD::version_set_compute_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_compute_globals, const Vector<String> &p_custom_defines) {
	ERR_FAIL_COND(!is_compute);

	Version *version = version_owner.get_or_null(p_version);
	ERR_FAIL_NULL(version);

	_compile_ensure_finished(version);

	version->compute_globals = p_compute_globals.utf8();
	version->uniforms = p_uniforms.utf8();

	version->code_sections.clear();
	for (const KeyValue<String, String> &E : p_code) {
		version->code_sections[StringName(E.key.to_upper())] = E.value.utf8();
	}

	version->custom_defines.clear();
	for (int i = 0; i < p_custom_defines.size(); i++) {
		version->custom_defines.push_back(p_custom_defines[i].utf8());
	}

	version->dirty = true;
	if (version->initialize_needed) {
		_initialize_version(version);
		for (int i = 0; i < group_enabled.size(); i++) {
			if (!group_enabled[i]) {
				_allocate_placeholders(version, i);
				continue;
			}
			_compile_version_start(version, i);
		}
		version->initialize_needed = false;
	}
}

bool ShaderRD::version_is_valid(RID p_version) {
	Version *version = version_owner.get_or_null(p_version);
	ERR_FAIL_NULL_V(version, false);

	if (version->dirty) {
		_initialize_version(version);
		for (int i = 0; i < group_enabled.size(); i++) {
			if (!group_enabled[i]) {
				_allocate_placeholders(version, i);
				continue;
			}
			_compile_version_start(version, i);
		}
	}

	_compile_ensure_finished(version);

	return version->valid;
}

bool ShaderRD::version_free(RID p_version) {
	if (version_owner.owns(p_version)) {
		Version *version = version_owner.get_or_null(p_version);
		_clear_version(version);
		version_owner.free(p_version);
	} else {
		return false;
	}

	return true;
}

void ShaderRD::set_variant_enabled(int p_variant, bool p_enabled) {
	ERR_FAIL_COND(version_owner.get_rid_count() > 0); //versions exist
	ERR_FAIL_INDEX(p_variant, variants_enabled.size());
	variants_enabled.write[p_variant] = p_enabled;
}

bool ShaderRD::is_variant_enabled(int p_variant) const {
	ERR_FAIL_INDEX_V(p_variant, variants_enabled.size(), false);
	return variants_enabled[p_variant];
}

void ShaderRD::enable_group(int p_group) {
	ERR_FAIL_INDEX(p_group, group_enabled.size());

	if (group_enabled[p_group]) {
		// Group already enabled, do nothing.
		return;
	}

	group_enabled.write[p_group] = true;

	// Compile all versions again to include the new group.
	List<RID> all_versions;
	version_owner.get_owned_list(&all_versions);
	for (const RID &E : all_versions) {
		Version *version = version_owner.get_or_null(E);
		_compile_version_start(version, p_group);
	}
}

bool ShaderRD::is_group_enabled(int p_group) const {
	return group_enabled[p_group];
}

bool ShaderRD::shader_cache_cleanup_on_start = false;

ShaderRD::ShaderRD() {
	// Do not feel forced to use this, in most cases it makes little to no difference.
	bool use_32_threads = false;
	if (RD::get_singleton()->get_device_vendor_name() == "NVIDIA") {
		use_32_threads = true;
	}
	String base_compute_define_text;
	if (use_32_threads) {
		base_compute_define_text = "\n#define NATIVE_LOCAL_GROUP_SIZE 32\n#define NATIVE_LOCAL_SIZE_2D_X 8\n#define NATIVE_LOCAL_SIZE_2D_Y 4\n";
	} else {
		base_compute_define_text = "\n#define NATIVE_LOCAL_GROUP_SIZE 64\n#define NATIVE_LOCAL_SIZE_2D_X 8\n#define NATIVE_LOCAL_SIZE_2D_Y 8\n";
	}

	base_compute_defines = base_compute_define_text.ascii();
}

void ShaderRD::initialize(const Vector<String> &p_variant_defines, const String &p_general_defines, const Vector<RD::PipelineImmutableSampler> &r_immutable_samplers) {
	immutable_samplers = r_immutable_samplers;
	ERR_FAIL_COND(variant_defines.size());
	ERR_FAIL_COND(p_variant_defines.is_empty());

	general_defines = p_general_defines.utf8();

	// When initialized this way, there is just one group and its always enabled.
	group_to_variant_map.insert(0, LocalVector<int>{});
	group_enabled.push_back(true);

	for (int i = 0; i < p_variant_defines.size(); i++) {
		variant_defines.push_back(VariantDefine(0, p_variant_defines[i], true));
		variants_enabled.push_back(true);
		variant_to_group.push_back(0);
		group_to_variant_map[0].push_back(i);
	}

	if (!shader_cache_dir.is_empty()) {
		group_sha256.resize(1);
		_initialize_cache();
	}
}

void ShaderRD::_initialize_cache() {
	for (const KeyValue<int, LocalVector<int>> &E : group_to_variant_map) {
		StringBuilder hash_build;

		hash_build.append("[base_hash]");
		hash_build.append(base_sha256);
		hash_build.append("[general_defines]");
		hash_build.append(general_defines.get_data());
		hash_build.append("[group_id]");
		hash_build.append(itos(E.key));
		for (uint32_t i = 0; i < E.value.size(); i++) {
			hash_build.append("[variant_defines:" + itos(E.value[i]) + "]");
			hash_build.append(variant_defines[E.value[i]].text.get_data());
		}

		group_sha256[E.key] = hash_build.as_string().sha256_text();

		Ref<DirAccess> d = DirAccess::open(shader_cache_dir);
		ERR_FAIL_COND(d.is_null());
		if (d->change_dir(name) != OK) {
			Error err = d->make_dir(name);
			ERR_FAIL_COND(err != OK);
			d->change_dir(name);
		}

		// Erase other versions?
		if (shader_cache_cleanup_on_start) {
		}
		//
		if (d->change_dir(group_sha256[E.key]) != OK) {
			Error err = d->make_dir(group_sha256[E.key]);
			ERR_FAIL_COND(err != OK);
		}
		shader_cache_dir_valid = true;

		print_verbose("Shader '" + name + "' (group " + itos(E.key) + ") SHA256: " + group_sha256[E.key]);
	}
}

// Same as above, but allows specifying shader compilation groups.
void ShaderRD::initialize(const Vector<VariantDefine> &p_variant_defines, const String &p_general_defines) {
	ERR_FAIL_COND(variant_defines.size());
	ERR_FAIL_COND(p_variant_defines.is_empty());

	general_defines = p_general_defines.utf8();

	int max_group_id = 0;

	for (int i = 0; i < p_variant_defines.size(); i++) {
		// Fill variant array.
		variant_defines.push_back(p_variant_defines[i]);
		variants_enabled.push_back(true);
		variant_to_group.push_back(p_variant_defines[i].group);

		// Map variant array index to group id, so we can iterate over groups later.
		if (!group_to_variant_map.has(p_variant_defines[i].group)) {
			group_to_variant_map.insert(p_variant_defines[i].group, LocalVector<int>{});
		}
		group_to_variant_map[p_variant_defines[i].group].push_back(i);

		// Track max size.
		if (p_variant_defines[i].group > max_group_id) {
			max_group_id = p_variant_defines[i].group;
		}
	}

	// Set all to groups to false, then enable those that should be default.
	group_enabled.resize_zeroed(max_group_id + 1);
	bool *enabled_ptr = group_enabled.ptrw();
	for (int i = 0; i < p_variant_defines.size(); i++) {
		if (p_variant_defines[i].default_enabled) {
			enabled_ptr[p_variant_defines[i].group] = true;
		}
	}

	if (!shader_cache_dir.is_empty()) {
		group_sha256.resize(max_group_id + 1);
		_initialize_cache();
	}
}

void ShaderRD::set_shader_cache_dir(const String &p_dir) {
	shader_cache_dir = p_dir;
}

void ShaderRD::set_shader_cache_save_compressed(bool p_enable) {
	shader_cache_save_compressed = p_enable;
}

void ShaderRD::set_shader_cache_save_compressed_zstd(bool p_enable) {
	shader_cache_save_compressed_zstd = p_enable;
}

void ShaderRD::set_shader_cache_save_debug(bool p_enable) {
	shader_cache_save_debug = p_enable;
}

String ShaderRD::shader_cache_dir;
bool ShaderRD::shader_cache_save_compressed = true;
bool ShaderRD::shader_cache_save_compressed_zstd = true;
bool ShaderRD::shader_cache_save_debug = true;

ShaderRD::~ShaderRD() {
	List<RID> remaining;
	version_owner.get_owned_list(&remaining);
	if (remaining.size()) {
		ERR_PRINT(itos(remaining.size()) + " shaders of type " + name + " were never freed");
		while (remaining.size()) {
			version_free(remaining.front()->get());
			remaining.pop_front();
		}
	}
}
