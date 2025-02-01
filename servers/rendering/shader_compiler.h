/**************************************************************************/
/*  shader_compiler.h                                                     */
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

#include "core/templates/pair.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering_server.h"

class ShaderCompiler {
public:
	enum Stage {
		STAGE_VERTEX,
		STAGE_FRAGMENT,
		STAGE_COMPUTE,
		STAGE_MAX
	};

	struct IdentifierActions {
		HashMap<StringName, Stage> entry_point_stages;

		HashMap<StringName, Pair<int *, int>> render_mode_values;
		HashMap<StringName, bool *> render_mode_flags;
		HashMap<StringName, bool *> usage_flag_pointers;
		HashMap<StringName, bool *> write_flag_pointers;

		HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> *uniforms = nullptr;
	};

	struct GeneratedCode {
		Vector<String> defines;
		struct Texture {
			StringName name;
			ShaderLanguage::DataType type = ShaderLanguage::DataType::TYPE_VOID;
			ShaderLanguage::ShaderNode::Uniform::Hint hint = ShaderLanguage::ShaderNode::Uniform::Hint::HINT_NONE;
			bool use_color = false;
			ShaderLanguage::TextureFilter filter = ShaderLanguage::TextureFilter::FILTER_DEFAULT;
			ShaderLanguage::TextureRepeat repeat = ShaderLanguage::TextureRepeat::REPEAT_DEFAULT;
			bool global = false;
			int array_size = 0;
		};

		Vector<Texture> texture_uniforms;

		Vector<uint32_t> uniform_offsets;
		uint32_t uniform_total_size = 0;
		String uniforms;
		String stage_globals[STAGE_MAX];

		HashMap<String, String> code;

		bool uses_global_textures = false;
		bool uses_fragment_time = false;
		bool uses_vertex_time = false;
		bool uses_screen_texture_mipmaps = false;
		bool uses_screen_texture = false;
		bool uses_depth_texture = false;
		bool uses_normal_roughness_texture = false;
	};

	struct DefaultIdentifierActions {
		HashMap<StringName, String> renames;
		HashMap<StringName, String> render_mode_defines;
		HashMap<StringName, String> usage_defines;
		HashMap<StringName, String> custom_samplers;
		ShaderLanguage::TextureFilter default_filter = ShaderLanguage::TextureFilter::FILTER_NEAREST;
		ShaderLanguage::TextureRepeat default_repeat = ShaderLanguage::TextureRepeat::REPEAT_DISABLE;
		int base_texture_binding_index = 0;
		int texture_layout_set = 0;
		String base_uniform_string;
		String global_buffer_array_variable;
		String instance_uniform_index_variable;
		uint32_t base_varying_index = 0;
		bool apply_luminance_multiplier = false;
		bool check_multiview_samplers = false;
	};

private:
	ShaderLanguage parser;

	String _get_sampler_name(ShaderLanguage::TextureFilter p_filter, ShaderLanguage::TextureRepeat p_repeat);

	void _dump_function_deps(const ShaderLanguage::ShaderNode *p_node, const StringName &p_for_func, const HashMap<StringName, String> &p_func_code, String &r_to_add, HashSet<StringName> &added);
	String _dump_node_code(const ShaderLanguage::Node *p_node, int p_level, GeneratedCode &r_gen_code, IdentifierActions &p_actions, const DefaultIdentifierActions &p_default_actions, bool p_assigning, bool p_scope = true);

	const ShaderLanguage::ShaderNode *shader = nullptr;
	const ShaderLanguage::FunctionNode *function = nullptr;
	StringName current_func_name;
	StringName time_name;
	HashSet<StringName> texture_functions;

	HashSet<StringName> used_name_defines;
	HashSet<StringName> used_flag_pointers;
	HashSet<StringName> used_rmode_defines;
	HashSet<StringName> internal_functions;
	HashSet<StringName> fragment_varyings;

	DefaultIdentifierActions actions;

	static ShaderLanguage::DataType _get_global_shader_uniform_type(const StringName &p_name);

public:
	Error compile(RS::ShaderMode p_mode, const String &p_code, IdentifierActions *p_actions, const String &p_path, GeneratedCode &r_gen_code);

	void initialize(DefaultIdentifierActions p_actions);
	ShaderCompiler();
};
