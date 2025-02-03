/**************************************************************************/
/*  shader_tracker_gles3.h                                                */
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

#ifndef SHADER_TRACKER_GLES3_H
#define SHADER_TRACKER_GLES3_H

#include "core/hash_map.h"
#include "core/list.h"
#include "servers/visual_server.h"
#include "shader_compiler_gles3.h"
#include <atomic>

class String;
class FileAccess;
class FileAccessCompressed;

class ShaderTrackerGLES3 {
	struct ShaderOptionsSerialized {
		String serialized;
		unsigned char hash;
	};

	Map<uint32_t, String> spatial_shader_code;
	Map<uint32_t, String> canvas_shader_code;
	Map<uint32_t, String> particle_shader_code;

	//Store all used action hashes and actions for a given shader code hash
	Map<uint32_t, Map<uint32_t, String>> used_spatial_shaders;
	Map<uint32_t, Map<uint32_t, String>> used_canvas_shaders;
	Map<uint32_t, Map<uint32_t, String>> used_particle_shaders;

	String output_folder;

public:
	ShaderTrackerGLES3();

	void add_shader(VS::ShaderMode p_mode, const String &p_shader_code, ShaderCompilerGLES3::IdentifierActions *p_actions, uint64_t conditional_key);

	~ShaderTrackerGLES3();

	//make me private
	static String _actions_to_strings(ShaderCompilerGLES3::IdentifierActions *p_actions, uint64_t conditional_key);

private:
	//void _add_spatial_shader(const String &p_shader_code, ShaderCompilerGLES3::IdentifierActions *p_actions, uint64_t conditional_key);

	static String _strings_to_csv_string(const Vector<String> &p_input);
};

class ShaderPreLoader {
	struct ShaderPermutation {
		uint32_t code;
		String actions;
		uint64_t conditional_key;
	};

private:
	static bool compiling;
	static Map<uint32_t, String> spatial_shader_code;
	static Vector<ShaderPermutation> spatial_shaders;
	static Map<uint32_t, String> canvas_shader_code;
	static Vector<ShaderPermutation> canvas_shaders;
	static Map<uint32_t, String> particle_shader_code;
	static Vector<ShaderPermutation> particle_shaders;

	static int spatial_count;
	static int canvas_count;
	static int particle_count;
	static int progress;

	static void deserialize_pair_map(Map<StringName, Pair<int *, int>> &p_render_mode_values, String p_data, List<int> &p_int_value_storage);
	static void deserialize_bool_map(Map<StringName, bool *> &p_map, String p_data, List<int> &p_int_value_storage);

	Error _load(VS::ShaderMode p_mode, const String &p_file_path);
	static void _compile_shader(VisualServer::ShaderMode p_mode, const ShaderPermutation &p_perm, const String &p_code);

public:
	Error load_spatial(const String &p_file_path);
	Error load_canvas(const String &p_file_path);
	Error load_particle(const String &p_file_path);
	void start();
	bool is_running() const;
	int get_stage() const;
	int get_stage_count() const;

	static void compile();
};

#endif // SHADER_TRACKER_GLES3_H
