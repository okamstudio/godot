/**************************************************************************/
/*  shader_rd.h                                                           */
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

#pragma once

#include "core/os/mutex.h"
#include "core/string/string_builder.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering_server.h"

class ShaderRD {
public:
	struct VariantDefine {
		int group = 0;
		CharString text;
		bool default_enabled = true;
		VariantDefine() {}
		VariantDefine(int p_group, const String &p_text, bool p_default_enabled) {
			group = p_group;
			default_enabled = p_default_enabled;
			text = p_text.utf8();
		}
	};

private:
	//versions
	CharString general_defines;
	Vector<VariantDefine> variant_defines;
	Vector<bool> variants_enabled;
	Vector<uint32_t> variant_to_group;
	HashMap<int, LocalVector<int>> group_to_variant_map;
	Vector<bool> group_enabled;

	Vector<RD::PipelineImmutableSampler> immutable_samplers;

	struct Version {
		CharString uniforms;
		CharString vertex_globals;
		CharString compute_globals;
		CharString fragment_globals;
		HashMap<StringName, CharString> code_sections;
		Vector<CharString> custom_defines;
		Vector<WorkerThreadPool::GroupID> group_compilation_tasks;

		Vector<Vector<uint8_t>> variant_data;
		Vector<RID> variants;

		bool valid;
		bool dirty;
		bool initialize_needed;
	};

	Mutex variant_set_mutex;

	struct CompileData {
		Version *version;
		int group = 0;
	};

	void _compile_variant(uint32_t p_variant, CompileData p_data);

	void _initialize_version(Version *p_version);
	void _clear_version(Version *p_version);
	void _compile_version_start(Version *p_version, int p_group);
	void _compile_version_end(Version *p_version, int p_group);
	void _compile_ensure_finished(Version *p_version);
	void _allocate_placeholders(Version *p_version, int p_group);

	RID_Owner<Version> version_owner;

	struct StageTemplate {
		struct Chunk {
			enum Type {
				TYPE_VERSION_DEFINES,
				TYPE_MATERIAL_UNIFORMS,
				TYPE_VERTEX_GLOBALS,
				TYPE_FRAGMENT_GLOBALS,
				TYPE_COMPUTE_GLOBALS,
				TYPE_CODE,
				TYPE_TEXT
			};

			Type type;
			StringName code;
			CharString text;
		};
		LocalVector<Chunk> chunks;
	};

	bool is_compute = false;

	String name;

	CharString base_compute_defines;

	String base_sha256;
	LocalVector<String> group_sha256;

	static String shader_cache_dir;
	static bool shader_cache_cleanup_on_start;
	static bool shader_cache_save_compressed;
	static bool shader_cache_save_compressed_zstd;
	static bool shader_cache_save_debug;
	bool shader_cache_dir_valid = false;

	enum StageType {
		STAGE_TYPE_VERTEX,
		STAGE_TYPE_FRAGMENT,
		STAGE_TYPE_COMPUTE,
		STAGE_TYPE_MAX,
	};

	StageTemplate stage_templates[STAGE_TYPE_MAX];

	void _build_variant_code(StringBuilder &p_builder, uint32_t p_variant, const Version *p_version, const StageTemplate &p_template);

	void _add_stage(const char *p_code, StageType p_stage_type);

	String _version_get_sha1(Version *p_version) const;
	String _get_cache_file_path(Version *p_version, int p_group);
	bool _load_from_cache(Version *p_version, int p_group);
	void _save_to_cache(Version *p_version, int p_group);
	void _initialize_cache();

protected:
	ShaderRD();
	void setup(const char *p_vertex_code, const char *p_fragment_code, const char *p_compute_code, const char *p_name);

public:
	RID version_create();

	void version_set_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_vertex_globals, const String &p_fragment_globals, const Vector<String> &p_custom_defines);
	void version_set_compute_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_compute_globals, const Vector<String> &p_custom_defines);

	_FORCE_INLINE_ RID version_get_shader(RID p_version, int p_variant) {
		ERR_FAIL_INDEX_V(p_variant, variant_defines.size(), RID());
		ERR_FAIL_COND_V(!variants_enabled[p_variant], RID());

		Version *version = version_owner.get_or_null(p_version);
		ERR_FAIL_NULL_V(version, RID());

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

		uint32_t group = variant_to_group[p_variant];
		if (version->group_compilation_tasks[group] != 0) {
			_compile_version_end(version, group);
		}

		if (!version->valid) {
			return RID();
		}

		return version->variants[p_variant];
	}

	bool version_is_valid(RID p_version);

	bool version_free(RID p_version);

	// Enable/disable variants for things that you know won't be used at engine initialization time .
	void set_variant_enabled(int p_variant, bool p_enabled);
	bool is_variant_enabled(int p_variant) const;

	// Enable/disable groups for things that might be enabled at run time.
	void enable_group(int p_group);
	bool is_group_enabled(int p_group) const;

	static void set_shader_cache_dir(const String &p_dir);
	static void set_shader_cache_save_compressed(bool p_enable);
	static void set_shader_cache_save_compressed_zstd(bool p_enable);
	static void set_shader_cache_save_debug(bool p_enable);

	RS::ShaderNativeSourceCode version_get_native_source_code(RID p_version);

	void initialize(const Vector<String> &p_variant_defines, const String &p_general_defines = "", const Vector<RD::PipelineImmutableSampler> &r_immutable_samplers = Vector<RD::PipelineImmutableSampler>());
	void initialize(const Vector<VariantDefine> &p_variant_defines, const String &p_general_defines = "");

	virtual ~ShaderRD();
};
