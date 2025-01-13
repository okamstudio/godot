/**************************************************************************/
/*  shader_compiler.cpp                                                   */
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

#include "shader_compiler.h"

#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/shader_types.h"

#define SL ShaderLanguage

static String _mktab(int p_level) {
	return String("\t").repeat(p_level);
}

static String _typestr(SL::DataType p_type) {
	String type = ShaderLanguage::get_datatype_name(p_type);
	if (!RS::get_singleton()->is_low_end() && ShaderLanguage::is_sampler_type(p_type)) {
		type = type.replace("sampler", "texture"); //we use textures instead of samplers in Vulkan GLSL
	}
	return type;
}

static int _get_datatype_alignment(SL::DataType p_type) {
	switch (p_type) {
		case SL::TYPE_VOID:
			return 0;
		case SL::TYPE_BOOL:
			return 4;
		case SL::TYPE_BVEC2:
			return 8;
		case SL::TYPE_BVEC3:
			return 16;
		case SL::TYPE_BVEC4:
			return 16;
		case SL::TYPE_INT:
			return 4;
		case SL::TYPE_IVEC2:
			return 8;
		case SL::TYPE_IVEC3:
			return 16;
		case SL::TYPE_IVEC4:
			return 16;
		case SL::TYPE_UINT:
			return 4;
		case SL::TYPE_UVEC2:
			return 8;
		case SL::TYPE_UVEC3:
			return 16;
		case SL::TYPE_UVEC4:
			return 16;
		case SL::TYPE_FLOAT:
			return 4;
		case SL::TYPE_VEC2:
			return 8;
		case SL::TYPE_VEC3:
			return 16;
		case SL::TYPE_VEC4:
			return 16;
		case SL::TYPE_MAT2:
			return 16;
		case SL::TYPE_MAT3:
			return 16;
		case SL::TYPE_MAT4:
			return 16;
		case SL::TYPE_SAMPLER2D:
			return 16;
		case SL::TYPE_ISAMPLER2D:
			return 16;
		case SL::TYPE_USAMPLER2D:
			return 16;
		case SL::TYPE_SAMPLER2DARRAY:
			return 16;
		case SL::TYPE_ISAMPLER2DARRAY:
			return 16;
		case SL::TYPE_USAMPLER2DARRAY:
			return 16;
		case SL::TYPE_SAMPLER3D:
			return 16;
		case SL::TYPE_ISAMPLER3D:
			return 16;
		case SL::TYPE_USAMPLER3D:
			return 16;
		case SL::TYPE_SAMPLERCUBE:
			return 16;
		case SL::TYPE_SAMPLERCUBEARRAY:
			return 16;
		case SL::TYPE_SAMPLEREXT:
			return 16;
		case SL::TYPE_STRUCT:
			return 0;
		case SL::TYPE_MAX: {
			ERR_FAIL_V(0);
		}
	}

	ERR_FAIL_V(0);
}

static String _interpstr(SL::DataInterpolation p_interp) {
	switch (p_interp) {
		case SL::INTERPOLATION_FLAT:
			return "flat ";
		case SL::INTERPOLATION_SMOOTH:
			return "";
		case SL::INTERPOLATION_DEFAULT:
			return "";
	}
	return "";
}

static String _prestr(SL::DataPrecision p_pres, bool p_force_highp = false) {
	switch (p_pres) {
		case SL::PRECISION_LOWP:
			return "lowp ";
		case SL::PRECISION_MEDIUMP:
			return "mediump ";
		case SL::PRECISION_HIGHP:
			return "highp ";
		case SL::PRECISION_DEFAULT:
			return p_force_highp ? "highp " : "";
	}
	return "";
}

static String _constr(bool p_is_const) {
	if (p_is_const) {
		return "const ";
	}
	return "";
}

static String _qualstr(SL::ArgumentQualifier p_qual) {
	switch (p_qual) {
		case SL::ARGUMENT_QUALIFIER_IN:
			return "";
		case SL::ARGUMENT_QUALIFIER_OUT:
			return "out ";
		case SL::ARGUMENT_QUALIFIER_INOUT:
			return "inout ";
	}
	return "";
}

static String _opstr(SL::Operator p_op) {
	return SL::get_operator_text(p_op);
}

static String _mkid(const String &p_id) {
	String id = "m_" + p_id.replace("__", "_dus_");
	return id.replace("__", "_dus_"); //doubleunderscore is reserved in glsl
}

static String f2sp0(float p_float) {
	String num = rtos(p_float);
	if (!num.contains_char('.') && !num.contains_char('e')) {
		num += ".0";
	}
	return num;
}

static String get_constant_text(SL::DataType p_type, const Vector<SL::Scalar> &p_values) {
	switch (p_type) {
		case SL::TYPE_BOOL:
			return p_values[0].boolean ? "true" : "false";
		case SL::TYPE_BVEC2:
		case SL::TYPE_BVEC3:
		case SL::TYPE_BVEC4: {
			String text = "bvec" + itos(p_type - SL::TYPE_BOOL + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += p_values[i].boolean ? "true" : "false";
			}
			text += ")";
			return text;
		}

		case SL::TYPE_INT:
			return itos(p_values[0].sint);
		case SL::TYPE_IVEC2:
		case SL::TYPE_IVEC3:
		case SL::TYPE_IVEC4: {
			String text = "ivec" + itos(p_type - SL::TYPE_INT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += itos(p_values[i].sint);
			}
			text += ")";
			return text;

		} break;
		case SL::TYPE_UINT:
			return itos(p_values[0].uint) + "u";
		case SL::TYPE_UVEC2:
		case SL::TYPE_UVEC3:
		case SL::TYPE_UVEC4: {
			String text = "uvec" + itos(p_type - SL::TYPE_UINT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += itos(p_values[i].uint) + "u";
			}
			text += ")";
			return text;
		} break;
		case SL::TYPE_FLOAT:
			return f2sp0(p_values[0].real);
		case SL::TYPE_VEC2:
		case SL::TYPE_VEC3:
		case SL::TYPE_VEC4: {
			String text = "vec" + itos(p_type - SL::TYPE_FLOAT + 1) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += f2sp0(p_values[i].real);
			}
			text += ")";
			return text;

		} break;
		case SL::TYPE_MAT2:
		case SL::TYPE_MAT3:
		case SL::TYPE_MAT4: {
			String text = "mat" + itos(p_type - SL::TYPE_MAT2 + 2) + "(";
			for (int i = 0; i < p_values.size(); i++) {
				if (i > 0) {
					text += ",";
				}

				text += f2sp0(p_values[i].real);
			}
			text += ")";
			return text;

		} break;
		default:
			ERR_FAIL_V(String());
	}
}

String ShaderCompiler::_get_sampler_name(ShaderLanguage::TextureFilter p_filter, ShaderLanguage::TextureRepeat p_repeat) {
	if (p_filter == ShaderLanguage::FILTER_DEFAULT) {
		ERR_FAIL_COND_V(actions.default_filter == ShaderLanguage::FILTER_DEFAULT, String());
		p_filter = actions.default_filter;
	}
	if (p_repeat == ShaderLanguage::REPEAT_DEFAULT) {
		ERR_FAIL_COND_V(actions.default_repeat == ShaderLanguage::REPEAT_DEFAULT, String());
		p_repeat = actions.default_repeat;
	}
	constexpr const char *name_mapping[] = {
		"SAMPLER_NEAREST_CLAMP",
		"SAMPLER_LINEAR_CLAMP",
		"SAMPLER_NEAREST_WITH_MIPMAPS_CLAMP",
		"SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP",
		"SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_CLAMP",
		"SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_CLAMP",
		"SAMPLER_NEAREST_REPEAT",
		"SAMPLER_LINEAR_REPEAT",
		"SAMPLER_NEAREST_WITH_MIPMAPS_REPEAT",
		"SAMPLER_LINEAR_WITH_MIPMAPS_REPEAT",
		"SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_REPEAT",
		"SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_REPEAT"
	};
	return String(name_mapping[p_filter + (p_repeat == ShaderLanguage::REPEAT_ENABLE ? ShaderLanguage::FILTER_DEFAULT : 0)]);
}

void ShaderCompiler::_dump_function_deps(const SL::ShaderNode *p_node, const StringName &p_for_func, const HashMap<StringName, String> &p_func_code, String &r_to_add, HashSet<StringName> &added) {
	int fidx = -1;

	for (int i = 0; i < p_node->vfunctions.size(); i++) {
		if (p_node->vfunctions[i].name == p_for_func) {
			fidx = i;
			break;
		}
	}

	ERR_FAIL_COND(fidx == -1);

	Vector<StringName> uses_functions;

	for (const StringName &E : p_node->vfunctions[fidx].uses_function) {
		uses_functions.push_back(E);
	}
	uses_functions.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

	for (int k = 0; k < uses_functions.size(); k++) {
		if (added.has(uses_functions[k])) {
			continue; //was added already
		}

		_dump_function_deps(p_node, uses_functions[k], p_func_code, r_to_add, added);

		SL::FunctionNode *fnode = nullptr;

		for (int i = 0; i < p_node->vfunctions.size(); i++) {
			if (p_node->vfunctions[i].name == uses_functions[k]) {
				fnode = p_node->vfunctions[i].function;
				break;
			}
		}

		ERR_FAIL_NULL(fnode);

		r_to_add += "\n";

		String header;
		if (fnode->return_type == SL::TYPE_STRUCT) {
			header = _mkid(fnode->return_struct_name);
		} else {
			header = _typestr(fnode->return_type);
		}

		if (fnode->return_array_size > 0) {
			header += "[";
			header += itos(fnode->return_array_size);
			header += "]";
		}

		header += " ";
		header += _mkid(fnode->rname);
		header += "(";

		for (int i = 0; i < fnode->arguments.size(); i++) {
			if (i > 0) {
				header += ", ";
			}
			header += _constr(fnode->arguments[i].is_const);
			if (fnode->arguments[i].type == SL::TYPE_STRUCT) {
				header += _qualstr(fnode->arguments[i].qualifier) + _mkid(fnode->arguments[i].struct_name) + " " + _mkid(fnode->arguments[i].name);
			} else {
				header += _qualstr(fnode->arguments[i].qualifier) + _prestr(fnode->arguments[i].precision) + _typestr(fnode->arguments[i].type) + " " + _mkid(fnode->arguments[i].name);
			}
			if (fnode->arguments[i].array_size > 0) {
				header += "[";
				header += itos(fnode->arguments[i].array_size);
				header += "]";
			}
		}

		header += ")\n";
		r_to_add += header;
		r_to_add += p_func_code[uses_functions[k]];

		added.insert(uses_functions[k]);
	}
}

static String _get_global_shader_uniform_from_type_and_index(const String &p_buffer, const String &p_index, ShaderLanguage::DataType p_type) {
	switch (p_type) {
		case ShaderLanguage::TYPE_BOOL: {
			return "bool(floatBitsToUint(" + p_buffer + "[" + p_index + "].x))";
		}
		case ShaderLanguage::TYPE_BVEC2: {
			return "bvec2(floatBitsToUint(" + p_buffer + "[" + p_index + "].xy))";
		}
		case ShaderLanguage::TYPE_BVEC3: {
			return "bvec3(floatBitsToUint(" + p_buffer + "[" + p_index + "].xyz))";
		}
		case ShaderLanguage::TYPE_BVEC4: {
			return "bvec4(floatBitsToUint(" + p_buffer + "[" + p_index + "].xyzw))";
		}
		case ShaderLanguage::TYPE_INT: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_IVEC2: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_IVEC3: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_IVEC4: {
			return "floatBitsToInt(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_UINT: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_UVEC2: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_UVEC3: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_UVEC4: {
			return "floatBitsToUint(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_FLOAT: {
			return "(" + p_buffer + "[" + p_index + "].x)";
		}
		case ShaderLanguage::TYPE_VEC2: {
			return "(" + p_buffer + "[" + p_index + "].xy)";
		}
		case ShaderLanguage::TYPE_VEC3: {
			return "(" + p_buffer + "[" + p_index + "].xyz)";
		}
		case ShaderLanguage::TYPE_VEC4: {
			return "(" + p_buffer + "[" + p_index + "].xyzw)";
		}
		case ShaderLanguage::TYPE_MAT2: {
			return "mat2(" + p_buffer + "[" + p_index + "].xy," + p_buffer + "[" + p_index + "+1u].xy)";
		}
		case ShaderLanguage::TYPE_MAT3: {
			return "mat3(" + p_buffer + "[" + p_index + "].xyz," + p_buffer + "[" + p_index + "+1u].xyz," + p_buffer + "[" + p_index + "+2u].xyz)";
		}
		case ShaderLanguage::TYPE_MAT4: {
			return "mat4(" + p_buffer + "[" + p_index + "].xyzw," + p_buffer + "[" + p_index + "+1u].xyzw," + p_buffer + "[" + p_index + "+2u].xyzw," + p_buffer + "[" + p_index + "+3u].xyzw)";
		}
		default: {
			ERR_FAIL_V("void");
		}
	}
}

String ShaderCompiler::_dump_node_code(const SL::Node *p_node, int p_level, GeneratedCode &r_gen_code, IdentifierActions &p_actions, const DefaultIdentifierActions &p_default_actions, bool p_assigning, bool p_use_scope) {
	String code;

	switch (p_node->type) {
		case SL::Node::NODE_TYPE_SHADER: {
			SL::ShaderNode *pnode = (SL::ShaderNode *)p_node;

			for (int i = 0; i < pnode->render_modes.size(); i++) {
				if (p_default_actions.render_mode_defines.has(pnode->render_modes[i]) && !used_rmode_defines.has(pnode->render_modes[i])) {
					r_gen_code.defines.push_back(p_default_actions.render_mode_defines[pnode->render_modes[i]]);
					used_rmode_defines.insert(pnode->render_modes[i]);
				}

				if (p_actions.render_mode_flags.has(pnode->render_modes[i])) {
					*p_actions.render_mode_flags[pnode->render_modes[i]] = true;
				}

				if (p_actions.render_mode_values.has(pnode->render_modes[i])) {
					Pair<int *, int> &p = p_actions.render_mode_values[pnode->render_modes[i]];
					*p.first = p.second;
				}
			}

			// structs

			for (int i = 0; i < pnode->vstructs.size(); i++) {
				SL::StructNode *st = pnode->vstructs[i].shader_struct;
				String struct_code;

				struct_code += "struct ";
				struct_code += _mkid(pnode->vstructs[i].name);
				struct_code += " ";
				struct_code += "{\n";
				for (SL::MemberNode *m : st->members) {
					if (m->datatype == SL::TYPE_STRUCT) {
						struct_code += _mkid(m->struct_name);
					} else {
						struct_code += _prestr(m->precision);
						struct_code += _typestr(m->datatype);
					}
					struct_code += " ";
					struct_code += _mkid(m->name);
					if (m->array_size > 0) {
						struct_code += "[";
						struct_code += itos(m->array_size);
						struct_code += "]";
					}
					struct_code += ";\n";
				}
				struct_code += "}";
				struct_code += ";\n";

				for (int j = 0; j < STAGE_MAX; j++) {
					r_gen_code.stage_globals[j] += struct_code;
				}
			}

			int max_texture_uniforms = 0;
			int max_uniforms = 0;

			for (const KeyValue<StringName, SL::ShaderNode::Uniform> &E : pnode->uniforms) {
				if (SL::is_sampler_type(E.value.type)) {
					if (E.value.hint == SL::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
							E.value.hint == SL::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
							E.value.hint == SL::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
						continue; // Don't create uniforms in the generated code for these.
					}
					max_texture_uniforms++;
				} else {
					if (E.value.scope == SL::ShaderNode::Uniform::SCOPE_INSTANCE) {
						continue; // Instances are indexed directly, don't need index uniforms.
					}

					max_uniforms++;
				}
			}

			r_gen_code.texture_uniforms.resize(max_texture_uniforms);

			Vector<int> uniform_sizes;
			Vector<int> uniform_alignments;
			Vector<StringName> uniform_defines;
			uniform_sizes.resize(max_uniforms);
			uniform_alignments.resize(max_uniforms);
			uniform_defines.resize(max_uniforms);
			bool uses_uniforms = false;

			Vector<StringName> uniform_names;

			for (const KeyValue<StringName, SL::ShaderNode::Uniform> &E : pnode->uniforms) {
				uniform_names.push_back(E.key);
			}

			uniform_names.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

			for (int k = 0; k < uniform_names.size(); k++) {
				const StringName &uniform_name = uniform_names[k];
				const SL::ShaderNode::Uniform &uniform = pnode->uniforms[uniform_name];

				String ucode;

				if (uniform.scope == SL::ShaderNode::Uniform::SCOPE_INSTANCE) {
					//insert, but don't generate any code.
					p_actions.uniforms->insert(uniform_name, uniform);
					continue; // Instances are indexed directly, don't need index uniforms.
				}

				if (uniform.hint == SL::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
						uniform.hint == SL::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
						uniform.hint == SL::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
					continue; // Don't create uniforms in the generated code for these.
				}

				if (SL::is_sampler_type(uniform.type)) {
					// Texture layouts are different for OpenGL GLSL and Vulkan GLSL
					if (!RS::get_singleton()->is_low_end()) {
						ucode = "layout(set = " + itos(actions.texture_layout_set) + ", binding = " + itos(actions.base_texture_binding_index + uniform.texture_binding) + ") ";
					}
					ucode += "uniform ";
				}

				bool is_buffer_global = !SL::is_sampler_type(uniform.type) && uniform.scope == SL::ShaderNode::Uniform::SCOPE_GLOBAL;

				if (is_buffer_global) {
					//this is an integer to index the global table
					ucode += _typestr(ShaderLanguage::TYPE_UINT);
				} else {
					ucode += _prestr(uniform.precision, ShaderLanguage::is_float_type(uniform.type));
					ucode += _typestr(uniform.type);
				}

				ucode += " " + _mkid(uniform_name);
				if (uniform.array_size > 0) {
					ucode += "[";
					ucode += itos(uniform.array_size);
					ucode += "]";
				}
				ucode += ";\n";
				if (SL::is_sampler_type(uniform.type)) {
					for (int j = 0; j < STAGE_MAX; j++) {
						r_gen_code.stage_globals[j] += ucode;
					}

					GeneratedCode::Texture texture;
					texture.name = uniform_name;
					texture.hint = uniform.hint;
					texture.type = uniform.type;
					texture.use_color = uniform.use_color;
					texture.filter = uniform.filter;
					texture.repeat = uniform.repeat;
					texture.global = uniform.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL;
					texture.array_size = uniform.array_size;
					if (texture.global) {
						r_gen_code.uses_global_textures = true;
					}

					r_gen_code.texture_uniforms.write[uniform.texture_order] = texture;
				} else {
					if (!uses_uniforms) {
						uses_uniforms = true;
					}
					uniform_defines.write[uniform.order] = ucode;
					if (is_buffer_global) {
						//globals are indices into the global table
						uniform_sizes.write[uniform.order] = ShaderLanguage::get_datatype_size(ShaderLanguage::TYPE_UINT);
						uniform_alignments.write[uniform.order] = _get_datatype_alignment(ShaderLanguage::TYPE_UINT);
					} else {
						// The following code enforces a 16-byte alignment of uniform arrays.
						if (uniform.array_size > 0) {
							int size = ShaderLanguage::get_datatype_size(uniform.type) * uniform.array_size;
							int m = (16 * uniform.array_size);
							if ((size % m) != 0) {
								size += m - (size % m);
							}
							uniform_sizes.write[uniform.order] = size;
							uniform_alignments.write[uniform.order] = 16;
						} else {
							uniform_sizes.write[uniform.order] = ShaderLanguage::get_datatype_size(uniform.type);
							uniform_alignments.write[uniform.order] = _get_datatype_alignment(uniform.type);
						}
					}
				}

				p_actions.uniforms->insert(uniform_name, uniform);
			}

			for (int i = 0; i < max_uniforms; i++) {
				r_gen_code.uniforms += uniform_defines[i];
			}

			// add up
			int offset = 0;
			for (int i = 0; i < uniform_sizes.size(); i++) {
				int align = offset % uniform_alignments[i];

				if (align != 0) {
					offset += uniform_alignments[i] - align;
				}

				r_gen_code.uniform_offsets.push_back(offset);

				offset += uniform_sizes[i];
			}

			r_gen_code.uniform_total_size = offset;

			if (r_gen_code.uniform_total_size % 16 != 0) { //UBO sizes must be multiples of 16
				r_gen_code.uniform_total_size += 16 - (r_gen_code.uniform_total_size % 16);
			}

			uint32_t index = p_default_actions.base_varying_index;

			List<Pair<StringName, SL::ShaderNode::Varying>> var_frag_to_light;

			Vector<StringName> varying_names;

			for (const KeyValue<StringName, SL::ShaderNode::Varying> &E : pnode->varyings) {
				varying_names.push_back(E.key);
			}

			varying_names.sort_custom<StringName::AlphCompare>(); //ensure order is deterministic so the same shader is always produced

			for (int k = 0; k < varying_names.size(); k++) {
				const StringName &varying_name = varying_names[k];
				const SL::ShaderNode::Varying &varying = pnode->varyings[varying_name];

				if (varying.stage == SL::ShaderNode::Varying::STAGE_FRAGMENT) {
					var_frag_to_light.push_back(Pair<StringName, SL::ShaderNode::Varying>(varying_name, varying));
					fragment_varyings.insert(varying_name);
					continue;
				}
				if (varying.type < SL::TYPE_INT) {
					continue; // Ignore boolean types to prevent crashing (if varying is just declared).
				}

				String vcode;
				String interp_mode = _interpstr(varying.interpolation);
				vcode += _prestr(varying.precision, ShaderLanguage::is_float_type(varying.type));
				vcode += _typestr(varying.type);
				vcode += " " + _mkid(varying_name);
				uint32_t inc = 1U;

				if (varying.array_size > 0) {
					inc = (uint32_t)varying.array_size;

					vcode += "[";
					vcode += itos(varying.array_size);
					vcode += "]";
				}

				switch (varying.type) {
					case SL::TYPE_MAT2:
						inc *= 2U;
						break;
					case SL::TYPE_MAT3:
						inc *= 3U;
						break;
					case SL::TYPE_MAT4:
						inc *= 4U;
						break;
					default:
						break;
				}

				vcode += ";\n";
				// GLSL ES 3.0 does not allow layout qualifiers for varyings
				if (!RS::get_singleton()->is_low_end()) {
					r_gen_code.stage_globals[STAGE_VERTEX] += "layout(location=" + itos(index) + ") ";
					r_gen_code.stage_globals[STAGE_FRAGMENT] += "layout(location=" + itos(index) + ") ";
				}
				r_gen_code.stage_globals[STAGE_VERTEX] += interp_mode + "out " + vcode;
				r_gen_code.stage_globals[STAGE_FRAGMENT] += interp_mode + "in " + vcode;

				index += inc;
			}

			if (var_frag_to_light.size() > 0) {
				String gcode = "\n\nstruct {\n";
				for (const Pair<StringName, SL::ShaderNode::Varying> &E : var_frag_to_light) {
					gcode += "\t" + _prestr(E.second.precision) + _typestr(E.second.type) + " " + _mkid(E.first);
					if (E.second.array_size > 0) {
						gcode += "[";
						gcode += itos(E.second.array_size);
						gcode += "]";
					}
					gcode += ";\n";
				}
				gcode += "} frag_to_light;\n";
				r_gen_code.stage_globals[STAGE_FRAGMENT] += gcode;
			}

			for (int i = 0; i < pnode->vconstants.size(); i++) {
				const SL::ShaderNode::Constant &cnode = pnode->vconstants[i];
				String gcode;
				gcode += _constr(true);
				gcode += _prestr(cnode.precision, ShaderLanguage::is_float_type(cnode.type));
				if (cnode.type == SL::TYPE_STRUCT) {
					gcode += _mkid(cnode.struct_name);
				} else {
					gcode += _typestr(cnode.type);
				}
				gcode += " " + _mkid(String(cnode.name));
				if (cnode.array_size > 0) {
					gcode += "[";
					gcode += itos(cnode.array_size);
					gcode += "]";
				}
				gcode += "=";
				gcode += _dump_node_code(cnode.initializer, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				gcode += ";\n";
				for (int j = 0; j < STAGE_MAX; j++) {
					r_gen_code.stage_globals[j] += gcode;
				}
			}

			HashMap<StringName, String> function_code;

			//code for functions
			for (int i = 0; i < pnode->vfunctions.size(); i++) {
				SL::FunctionNode *fnode = pnode->vfunctions[i].function;
				function = fnode;
				current_func_name = fnode->name;
				function_code[fnode->name] = _dump_node_code(fnode->body, p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				function = nullptr;
			}

			//place functions in actual code

			HashSet<StringName> added_funcs_per_stage[STAGE_MAX];

			for (int i = 0; i < pnode->vfunctions.size(); i++) {
				SL::FunctionNode *fnode = pnode->vfunctions[i].function;

				function = fnode;

				current_func_name = fnode->name;

				if (p_actions.entry_point_stages.has(fnode->name)) {
					Stage stage = p_actions.entry_point_stages[fnode->name];
					_dump_function_deps(pnode, fnode->name, function_code, r_gen_code.stage_globals[stage], added_funcs_per_stage[stage]);
					r_gen_code.code[fnode->name] = function_code[fnode->name];
				}

				function = nullptr;
			}

			//code+=dump_node_code(pnode->body,p_level);
		} break;
		case SL::Node::NODE_TYPE_STRUCT: {
		} break;
		case SL::Node::NODE_TYPE_FUNCTION: {
		} break;
		case SL::Node::NODE_TYPE_BLOCK: {
			SL::BlockNode *bnode = (SL::BlockNode *)p_node;

			//variables
			if (!bnode->single_statement) {
				code += _mktab(p_level - 1) + "{\n";
			}

			int i = 0;
			for (List<ShaderLanguage::Node *>::ConstIterator itr = bnode->statements.begin(); itr != bnode->statements.end(); ++itr, ++i) {
				String scode = _dump_node_code(*itr, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);

				if ((*itr)->type == SL::Node::NODE_TYPE_CONTROL_FLOW || bnode->single_statement) {
					code += scode; //use directly
					if (bnode->use_comma_between_statements && i + 1 < bnode->statements.size()) {
						code += ",";
					}
				} else {
					code += _mktab(p_level) + scode + ";\n";
				}
			}
			if (!bnode->single_statement) {
				code += _mktab(p_level - 1) + "}\n";
			}

		} break;
		case SL::Node::NODE_TYPE_VARIABLE_DECLARATION: {
			SL::VariableDeclarationNode *vdnode = (SL::VariableDeclarationNode *)p_node;

			String declaration;
			declaration += _constr(vdnode->is_const);
			if (vdnode->datatype == SL::TYPE_STRUCT) {
				declaration += _mkid(vdnode->struct_name);
			} else {
				declaration += _prestr(vdnode->precision) + _typestr(vdnode->datatype);
			}
			declaration += " ";
			for (int i = 0; i < vdnode->declarations.size(); i++) {
				bool is_array = vdnode->declarations[i].size > 0;
				if (i > 0) {
					declaration += ",";
				}
				declaration += _mkid(vdnode->declarations[i].name);
				if (is_array) {
					declaration += "[";
					if (vdnode->declarations[i].size_expression != nullptr) {
						declaration += _dump_node_code(vdnode->declarations[i].size_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					} else {
						declaration += itos(vdnode->declarations[i].size);
					}
					declaration += "]";
				}

				if (!is_array || vdnode->declarations[i].single_expression) {
					if (!vdnode->declarations[i].initializer.is_empty()) {
						declaration += "=";
						declaration += _dump_node_code(vdnode->declarations[i].initializer[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					}
				} else {
					int size = vdnode->declarations[i].initializer.size();
					if (size > 0) {
						declaration += "=";
						if (vdnode->datatype == SL::TYPE_STRUCT) {
							declaration += _mkid(vdnode->struct_name);
						} else {
							declaration += _typestr(vdnode->datatype);
						}
						declaration += "[";
						declaration += itos(size);
						declaration += "]";
						declaration += "(";
						for (int j = 0; j < size; j++) {
							if (j > 0) {
								declaration += ",";
							}
							declaration += _dump_node_code(vdnode->declarations[i].initializer[j], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
						}
						declaration += ")";
					}
				}
			}

			code += declaration;
		} break;
		case SL::Node::NODE_TYPE_VARIABLE: {
			SL::VariableNode *vnode = (SL::VariableNode *)p_node;
			bool use_fragment_varying = false;

			if (!vnode->is_local && !(p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX)) {
				if (p_assigning) {
					if (shader->varyings.has(vnode->name)) {
						use_fragment_varying = true;
					}
				} else {
					if (fragment_varyings.has(vnode->name)) {
						use_fragment_varying = true;
					}
				}
			}

			if (p_assigning && p_actions.write_flag_pointers.has(vnode->name)) {
				*p_actions.write_flag_pointers[vnode->name] = true;
			}

			if (p_default_actions.usage_defines.has(vnode->name) && !used_name_defines.has(vnode->name)) {
				String define = p_default_actions.usage_defines[vnode->name];
				if (define.begins_with("@")) {
					define = p_default_actions.usage_defines[define.substr(1, define.length())];
				}
				r_gen_code.defines.push_back(define);
				used_name_defines.insert(vnode->name);
			}

			if (p_actions.usage_flag_pointers.has(vnode->name) && !used_flag_pointers.has(vnode->name)) {
				*p_actions.usage_flag_pointers[vnode->name] = true;
				used_flag_pointers.insert(vnode->name);
			}

			if (p_default_actions.renames.has(vnode->name)) {
				code = p_default_actions.renames[vnode->name];
			} else {
				if (shader->uniforms.has(vnode->name)) {
					//its a uniform!
					const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[vnode->name];
					if (u.is_texture()) {
						StringName name;
						if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
							name = "color_buffer";
							if (u.filter >= ShaderLanguage::FILTER_NEAREST_MIPMAP) {
								r_gen_code.uses_screen_texture_mipmaps = true;
							}
							r_gen_code.uses_screen_texture = true;
						} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
							name = "normal_roughness_buffer";
							r_gen_code.uses_normal_roughness_texture = true;
						} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
							name = "depth_buffer";
							r_gen_code.uses_depth_texture = true;
						} else {
							name = _mkid(vnode->name); //texture, use as is
						}

						code = name;
					} else {
						//a scalar or vector
						if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL) {
							code = actions.base_uniform_string + _mkid(vnode->name); //texture, use as is
							//global variable, this means the code points to an index to the global table
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
							//instance variable, index it as such
							code = "(" + p_default_actions.instance_uniform_index_variable + "+" + itos(u.instance_index) + "u)";
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else {
							//regular uniform, index from UBO
							code = actions.base_uniform_string + _mkid(vnode->name);
						}
					}

				} else {
					if (use_fragment_varying) {
						code = "frag_to_light.";
					}
					code += _mkid(vnode->name); //its something else (local var most likely) use as is
				}
			}

			if (vnode->name == time_name) {
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX) {
					r_gen_code.uses_vertex_time = true;
				}
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_FRAGMENT) {
					r_gen_code.uses_fragment_time = true;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_ARRAY_CONSTRUCT: {
			SL::ArrayConstructNode *acnode = (SL::ArrayConstructNode *)p_node;
			int sz = acnode->initializer.size();
			if (acnode->datatype == SL::TYPE_STRUCT) {
				code += _mkid(acnode->struct_name);
			} else {
				code += _typestr(acnode->datatype);
			}
			code += "[";
			code += itos(acnode->initializer.size());
			code += "]";
			code += "(";
			for (int i = 0; i < sz; i++) {
				code += _dump_node_code(acnode->initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				if (i != sz - 1) {
					code += ", ";
				}
			}
			code += ")";
		} break;
		case SL::Node::NODE_TYPE_ARRAY: {
			SL::ArrayNode *anode = (SL::ArrayNode *)p_node;
			bool use_fragment_varying = false;

			if (!anode->is_local && !(p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX)) {
				if (anode->assign_expression != nullptr && shader->varyings.has(anode->name)) {
					use_fragment_varying = true;
				} else {
					if (p_assigning) {
						if (shader->varyings.has(anode->name)) {
							use_fragment_varying = true;
						}
					} else {
						if (fragment_varyings.has(anode->name)) {
							use_fragment_varying = true;
						}
					}
				}
			}

			if (p_assigning && p_actions.write_flag_pointers.has(anode->name)) {
				*p_actions.write_flag_pointers[anode->name] = true;
			}

			if (p_default_actions.usage_defines.has(anode->name) && !used_name_defines.has(anode->name)) {
				String define = p_default_actions.usage_defines[anode->name];
				if (define.begins_with("@")) {
					define = p_default_actions.usage_defines[define.substr(1, define.length())];
				}
				r_gen_code.defines.push_back(define);
				used_name_defines.insert(anode->name);
			}

			if (p_actions.usage_flag_pointers.has(anode->name) && !used_flag_pointers.has(anode->name)) {
				*p_actions.usage_flag_pointers[anode->name] = true;
				used_flag_pointers.insert(anode->name);
			}

			if (p_default_actions.renames.has(anode->name)) {
				code = p_default_actions.renames[anode->name];
			} else {
				if (shader->uniforms.has(anode->name)) {
					//its a uniform!
					const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[anode->name];
					if (u.is_texture()) {
						code = _mkid(anode->name); //texture, use as is
					} else {
						//a scalar or vector
						if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL) {
							code = actions.base_uniform_string + _mkid(anode->name); //texture, use as is
							//global variable, this means the code points to an index to the global table
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else if (u.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
							//instance variable, index it as such
							code = "(" + p_default_actions.instance_uniform_index_variable + "+" + itos(u.instance_index) + "u)";
							code = _get_global_shader_uniform_from_type_and_index(p_default_actions.global_buffer_array_variable, code, u.type);
						} else {
							//regular uniform, index from UBO
							code = actions.base_uniform_string + _mkid(anode->name);
						}
					}
				} else {
					if (use_fragment_varying) {
						code = "frag_to_light.";
					}
					code += _mkid(anode->name);
				}
			}

			if (anode->call_expression != nullptr) {
				code += ".";
				code += _dump_node_code(anode->call_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning, false);
			} else if (anode->index_expression != nullptr) {
				code += "[";
				code += _dump_node_code(anode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += "]";
			} else if (anode->assign_expression != nullptr) {
				code += "=";
				code += _dump_node_code(anode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
			}

			if (anode->name == time_name) {
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_VERTEX) {
					r_gen_code.uses_vertex_time = true;
				}
				if (p_actions.entry_point_stages.has(current_func_name) && p_actions.entry_point_stages[current_func_name] == STAGE_FRAGMENT) {
					r_gen_code.uses_fragment_time = true;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_CONSTANT: {
			SL::ConstantNode *cnode = (SL::ConstantNode *)p_node;

			if (cnode->array_size == 0) {
				return get_constant_text(cnode->datatype, cnode->values);
			} else {
				if (cnode->get_datatype() == SL::TYPE_STRUCT) {
					code += _mkid(cnode->struct_name);
				} else {
					code += _typestr(cnode->datatype);
				}
				code += "[";
				code += itos(cnode->array_size);
				code += "]";
				code += "(";
				for (int i = 0; i < cnode->array_size; i++) {
					if (i > 0) {
						code += ",";
					} else {
						code += "";
					}
					code += _dump_node_code(cnode->array_declarations[0].initializer[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				}
				code += ")";
			}

		} break;
		case SL::Node::NODE_TYPE_OPERATOR: {
			SL::OperatorNode *onode = (SL::OperatorNode *)p_node;

			switch (onode->op) {
				case SL::OP_ASSIGN:
				case SL::OP_ASSIGN_ADD:
				case SL::OP_ASSIGN_SUB:
				case SL::OP_ASSIGN_MUL:
				case SL::OP_ASSIGN_DIV:
				case SL::OP_ASSIGN_SHIFT_LEFT:
				case SL::OP_ASSIGN_SHIFT_RIGHT:
				case SL::OP_ASSIGN_MOD:
				case SL::OP_ASSIGN_BIT_AND:
				case SL::OP_ASSIGN_BIT_OR:
				case SL::OP_ASSIGN_BIT_XOR:
					code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, true) + _opstr(onode->op) + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					break;
				case SL::OP_BIT_INVERT:
				case SL::OP_NEGATE:
				case SL::OP_NOT:
				case SL::OP_DECREMENT:
				case SL::OP_INCREMENT: {
					const String node_code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);

					if (onode->op == SL::OP_NEGATE && node_code.begins_with("-")) { // To prevent writing unary minus twice.
						code = node_code;
					} else {
						code = _opstr(onode->op) + node_code;
					}

				} break;
				case SL::OP_POST_DECREMENT:
				case SL::OP_POST_INCREMENT:
					code = _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + _opstr(onode->op);
					break;
				case SL::OP_CALL:
				case SL::OP_STRUCT:
				case SL::OP_CONSTRUCT: {
					ERR_FAIL_COND_V(onode->arguments[0]->type != SL::Node::NODE_TYPE_VARIABLE, String());
					const SL::VariableNode *vnode = static_cast<const SL::VariableNode *>(onode->arguments[0]);
					const SL::FunctionNode *func = nullptr;
					const bool is_internal_func = internal_functions.has(vnode->name);

					if (!is_internal_func) {
						for (int i = 0; i < shader->vfunctions.size(); i++) {
							if (shader->vfunctions[i].name == vnode->name) {
								func = shader->vfunctions[i].function;
								break;
							}
						}
					}

					bool is_texture_func = false;
					bool is_screen_texture = false;
					bool texture_func_no_uv = false;
					bool texture_func_returns_data = false;

					if (onode->op == SL::OP_STRUCT) {
						code += _mkid(vnode->name);
					} else if (onode->op == SL::OP_CONSTRUCT) {
						code += String(vnode->name);
					} else {
						if (p_actions.usage_flag_pointers.has(vnode->name) && !used_flag_pointers.has(vnode->name)) {
							*p_actions.usage_flag_pointers[vnode->name] = true;
							used_flag_pointers.insert(vnode->name);
						}

						if (is_internal_func) {
							code += vnode->name;
							is_texture_func = texture_functions.has(vnode->name);
							texture_func_no_uv = (vnode->name == "textureSize" || vnode->name == "textureQueryLevels");
							texture_func_returns_data = texture_func_no_uv || vnode->name == "textureQueryLod";
						} else if (p_default_actions.renames.has(vnode->name)) {
							code += p_default_actions.renames[vnode->name];
						} else {
							code += _mkid(vnode->rname);
						}
					}

					code += "(";

					// if color backbuffer, depth backbuffer or normal roughness texture is used,
					// we will add logic to automatically switch between
					// sampler2D and sampler2D array and vec2 UV and vec3 UV.
					bool multiview_uv_needed = false;
					bool is_normal_roughness_texture = false;

					for (int i = 1; i < onode->arguments.size(); i++) {
						if (i > 1) {
							code += ", ";
						}

						bool is_out_qualifier = false;
						if (is_internal_func) {
							is_out_qualifier = SL::is_builtin_func_out_parameter(vnode->name, i - 1);
						} else if (func != nullptr) {
							const SL::ArgumentQualifier qualifier = func->arguments[i - 1].qualifier;
							is_out_qualifier = qualifier == SL::ARGUMENT_QUALIFIER_OUT || qualifier == SL::ARGUMENT_QUALIFIER_INOUT;
						}

						if (is_out_qualifier) {
							StringName name;
							bool found = false;
							{
								const SL::Node *node = onode->arguments[i];

								bool done = false;
								do {
									switch (node->type) {
										case SL::Node::NODE_TYPE_VARIABLE: {
											name = static_cast<const SL::VariableNode *>(node)->name;
											done = true;
											found = true;
										} break;
										case SL::Node::NODE_TYPE_MEMBER: {
											node = static_cast<const SL::MemberNode *>(node)->owner;
										} break;
										default: {
											done = true;
										} break;
									}
								} while (!done);
							}

							if (found && p_actions.write_flag_pointers.has(name)) {
								*p_actions.write_flag_pointers[name] = true;
							}
						}

						String node_code = _dump_node_code(onode->arguments[i], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
						if (is_texture_func && i == 1) {
							// If we're doing a texture lookup we need to check our texture argument
							StringName texture_uniform;
							bool correct_texture_uniform = false;

							switch (onode->arguments[i]->type) {
								case SL::Node::NODE_TYPE_VARIABLE: {
									const SL::VariableNode *varnode = static_cast<const SL::VariableNode *>(onode->arguments[i]);
									texture_uniform = varnode->name;
									correct_texture_uniform = true;
								} break;
								case SL::Node::NODE_TYPE_ARRAY: {
									const SL::ArrayNode *anode = static_cast<const SL::ArrayNode *>(onode->arguments[i]);
									texture_uniform = anode->name;
									correct_texture_uniform = true;
								} break;
								default:
									break;
							}

							if (correct_texture_uniform && !RS::get_singleton()->is_low_end()) {
								// Need to map from texture to sampler in order to sample when using Vulkan GLSL.
								String sampler_name;
								bool is_depth_texture = false;

								if (actions.custom_samplers.has(texture_uniform)) {
									sampler_name = actions.custom_samplers[texture_uniform];
								} else {
									if (shader->uniforms.has(texture_uniform)) {
										const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[texture_uniform];
										if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
											is_screen_texture = true;
										} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
											is_depth_texture = true;
										} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
											is_normal_roughness_texture = true;
										}
										sampler_name = _get_sampler_name(u.filter, u.repeat);
									} else {
										bool found = false;

										for (int j = 0; j < function->arguments.size(); j++) {
											if (function->arguments[j].name == texture_uniform) {
												if (function->arguments[j].tex_builtin_check) {
													ERR_CONTINUE(!actions.custom_samplers.has(function->arguments[j].tex_builtin));
													sampler_name = actions.custom_samplers[function->arguments[j].tex_builtin];
													found = true;
													break;
												}
												if (function->arguments[j].tex_argument_check) {
													if (function->arguments[j].tex_hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
														is_screen_texture = true;
													} else if (function->arguments[j].tex_hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
														is_depth_texture = true;
													} else if (function->arguments[j].tex_hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
														is_normal_roughness_texture = true;
													}
													sampler_name = _get_sampler_name(function->arguments[j].tex_argument_filter, function->arguments[j].tex_argument_repeat);
													found = true;
													break;
												}
											}
										}
										if (!found) {
											//function was most likely unused, so use anything (compiler will remove it anyway)
											sampler_name = _get_sampler_name(ShaderLanguage::FILTER_DEFAULT, ShaderLanguage::REPEAT_DEFAULT);
										}
									}
								}

								String data_type_name = "";
								if (actions.check_multiview_samplers && (is_screen_texture || is_depth_texture || is_normal_roughness_texture)) {
									data_type_name = "multiviewSampler";
									multiview_uv_needed = true;
								} else {
									data_type_name = ShaderLanguage::get_datatype_name(onode->arguments[i]->get_datatype());
								}

								code += data_type_name + "(" + node_code + ", " + sampler_name + ")";
							} else if (actions.check_multiview_samplers && correct_texture_uniform && RS::get_singleton()->is_low_end()) {
								// Texture function on low end hardware (i.e. OpenGL).
								// We just need to know if the texture supports multiview.

								if (shader->uniforms.has(texture_uniform)) {
									const ShaderLanguage::ShaderNode::Uniform &u = shader->uniforms[texture_uniform];
									if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE) {
										multiview_uv_needed = true;
									} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
										multiview_uv_needed = true;
									} else if (u.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE) {
										multiview_uv_needed = true;
									}
								}

								code += node_code;
							} else {
								code += node_code;
							}
						} else if (multiview_uv_needed && !texture_func_no_uv && i == 2) {
							// UV coordinate after using color, depth or normal roughness texture.
							node_code = "multiview_uv(" + node_code + ".xy)";

							code += node_code;
						} else {
							code += node_code;
						}
					}
					code += ")";
					if (is_screen_texture && !texture_func_returns_data && actions.apply_luminance_multiplier) {
						code = "(" + code + " * vec4(vec3(sc_luminance_multiplier()), 1.0))";
					}
					if (is_normal_roughness_texture && !texture_func_returns_data) {
						code = "normal_roughness_compatibility(" + code + ")";
					}
				} break;
				case SL::OP_INDEX: {
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "[";
					code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "]";

				} break;
				case SL::OP_SELECT_IF: {
					code += "(";
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += "?";
					code += _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += ":";
					code += _dump_node_code(onode->arguments[2], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					code += ")";

				} break;
				case SL::OP_EMPTY: {
					// Semicolon (or empty statement) - ignored.
				} break;

				default: {
					if (p_use_scope) {
						code += "(";
					}
					code += _dump_node_code(onode->arguments[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + " " + _opstr(onode->op) + " " + _dump_node_code(onode->arguments[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
					if (p_use_scope) {
						code += ")";
					}
					break;
				}
			}

		} break;
		case SL::Node::NODE_TYPE_CONTROL_FLOW: {
			SL::ControlFlowNode *cfnode = (SL::ControlFlowNode *)p_node;
			if (cfnode->flow_op == SL::FLOW_OP_IF) {
				code += _mktab(p_level) + "if (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				if (cfnode->blocks.size() == 2) {
					code += _mktab(p_level) + "else\n";
					code += _dump_node_code(cfnode->blocks[1], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				}
			} else if (cfnode->flow_op == SL::FLOW_OP_SWITCH) {
				code += _mktab(p_level) + "switch (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_CASE) {
				code += _mktab(p_level) + "case " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ":\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_DEFAULT) {
				code += _mktab(p_level) + "default:\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_DO) {
				code += _mktab(p_level) + "do";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ");";
			} else if (cfnode->flow_op == SL::FLOW_OP_WHILE) {
				code += _mktab(p_level) + "while (" + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ")\n";
				code += _dump_node_code(cfnode->blocks[0], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);
			} else if (cfnode->flow_op == SL::FLOW_OP_FOR) {
				String left = _dump_node_code(cfnode->blocks[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				String middle = _dump_node_code(cfnode->blocks[1], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				String right = _dump_node_code(cfnode->blocks[2], p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += _mktab(p_level) + "for (" + left + ";" + middle + ";" + right + ")\n";
				code += _dump_node_code(cfnode->blocks[3], p_level + 1, r_gen_code, p_actions, p_default_actions, p_assigning);

			} else if (cfnode->flow_op == SL::FLOW_OP_RETURN) {
				if (cfnode->expressions.size()) {
					code = "return " + _dump_node_code(cfnode->expressions[0], p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + ";";
				} else {
					code = "return;";
				}
			} else if (cfnode->flow_op == SL::FLOW_OP_DISCARD) {
				if (p_actions.usage_flag_pointers.has("DISCARD") && !used_flag_pointers.has("DISCARD")) {
					*p_actions.usage_flag_pointers["DISCARD"] = true;
					used_flag_pointers.insert("DISCARD");
				}

				code = "discard;";
			} else if (cfnode->flow_op == SL::FLOW_OP_CONTINUE) {
				code = "continue;";
			} else if (cfnode->flow_op == SL::FLOW_OP_BREAK) {
				code = "break;";
			}

		} break;
		case SL::Node::NODE_TYPE_MEMBER: {
			SL::MemberNode *mnode = (SL::MemberNode *)p_node;
			String name;
			if (mnode->basetype == SL::TYPE_STRUCT) {
				name = _mkid(mnode->name);
			} else {
				name = mnode->name;
			}
			code = _dump_node_code(mnode->owner, p_level, r_gen_code, p_actions, p_default_actions, p_assigning) + "." + name;
			if (mnode->index_expression != nullptr) {
				code += "[";
				code += _dump_node_code(mnode->index_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning);
				code += "]";
			} else if (mnode->assign_expression != nullptr) {
				code += "=";
				code += _dump_node_code(mnode->assign_expression, p_level, r_gen_code, p_actions, p_default_actions, true, false);
			} else if (mnode->call_expression != nullptr) {
				code += ".";
				code += _dump_node_code(mnode->call_expression, p_level, r_gen_code, p_actions, p_default_actions, p_assigning, false);
			}
		} break;
	}

	return code;
}

ShaderLanguage::DataType ShaderCompiler::_get_global_shader_uniform_type(const StringName &p_name) {
	RS::GlobalShaderParameterType gvt = RSG::material_storage->global_shader_parameter_get_type(p_name);
	return (ShaderLanguage::DataType)RS::global_shader_uniform_type_get_shader_datatype(gvt);
}

Error ShaderCompiler::compile(RS::ShaderMode p_mode, const String &p_code, IdentifierActions *p_actions, const String &p_path, GeneratedCode &r_gen_code) {
	SL::ShaderCompileInfo info;
	info.functions = ShaderTypes::get_singleton()->get_functions(p_mode);
	info.render_modes = ShaderTypes::get_singleton()->get_modes(p_mode);
	info.shader_types = ShaderTypes::get_singleton()->get_types();
	info.global_shader_uniform_type_func = _get_global_shader_uniform_type;

	Error err = parser.compile(p_code, info);

	if (err != OK) {
		Vector<ShaderLanguage::FilePosition> include_positions = parser.get_include_positions();

		String current;
		HashMap<String, Vector<String>> includes;
		includes[""] = Vector<String>();
		Vector<String> include_stack;
		Vector<String> shader_lines = p_code.split("\n");

		// Reconstruct the files.
		for (int i = 0; i < shader_lines.size(); i++) {
			String l = shader_lines[i];
			if (l.begins_with("@@>")) {
				String inc_path = l.replace_first("@@>", "");

				l = "#include \"" + inc_path + "\"";
				includes[current].append("#include \"" + inc_path + "\""); // Restore the include directive
				include_stack.push_back(current);
				current = inc_path;
				includes[inc_path] = Vector<String>();

			} else if (l.begins_with("@@<")) {
				if (include_stack.size()) {
					current = include_stack[include_stack.size() - 1];
					include_stack.resize(include_stack.size() - 1);
				}
			} else {
				includes[current].push_back(l);
			}
		}

		// Print the files.
		for (const KeyValue<String, Vector<String>> &E : includes) {
			if (E.key.is_empty()) {
				if (p_path == "") {
					print_line("--Main Shader--");
				} else {
					print_line("--" + p_path + "--");
				}
			} else {
				print_line("--" + E.key + "--");
			}
			int err_line = -1;
			for (int i = 0; i < include_positions.size(); i++) {
				if (include_positions[i].file == E.key) {
					err_line = include_positions[i].line;
				}
			}
			const Vector<String> &V = E.value;
			for (int i = 0; i < V.size(); i++) {
				if (i == err_line - 1) {
					// Mark the error line to be visible without having to look at
					// the trace at the end.
					print_line(vformat("E%4d-> %s", i + 1, V[i]));
				} else {
					print_line(vformat("%5d | %s", i + 1, V[i]));
				}
			}
		}

		String file;
		int line;
		if (include_positions.size() > 1) {
			file = include_positions[include_positions.size() - 1].file;
			line = include_positions[include_positions.size() - 1].line;
		} else {
			file = p_path;
			line = parser.get_error_line();
		}

		_err_print_error(nullptr, file.utf8().get_data(), line, parser.get_error_text().utf8().get_data(), false, ERR_HANDLER_SHADER);
		return err;
	}

	r_gen_code.defines.clear();
	r_gen_code.code.clear();
	for (int i = 0; i < STAGE_MAX; i++) {
		r_gen_code.stage_globals[i] = String();
	}
	r_gen_code.uses_fragment_time = false;
	r_gen_code.uses_vertex_time = false;
	r_gen_code.uses_global_textures = false;
	r_gen_code.uses_screen_texture_mipmaps = false;
	r_gen_code.uses_screen_texture = false;
	r_gen_code.uses_depth_texture = false;
	r_gen_code.uses_normal_roughness_texture = false;

	used_name_defines.clear();
	used_rmode_defines.clear();
	used_flag_pointers.clear();
	fragment_varyings.clear();

	shader = parser.get_shader();
	function = nullptr;
	_dump_node_code(shader, 1, r_gen_code, *p_actions, actions, false);

	return OK;
}

void ShaderCompiler::initialize(DefaultIdentifierActions p_actions) {
	actions = p_actions;

	time_name = "TIME";

	List<String> func_list;

	ShaderLanguage::get_builtin_funcs(&func_list);

	for (const String &E : func_list) {
		internal_functions.insert(E);
	}
	texture_functions.insert("texture");
	texture_functions.insert("textureProj");
	texture_functions.insert("textureLod");
	texture_functions.insert("textureProjLod");
	texture_functions.insert("textureGrad");
	texture_functions.insert("textureProjGrad");
	texture_functions.insert("textureGather");
	texture_functions.insert("textureSize");
	texture_functions.insert("textureQueryLod");
	texture_functions.insert("textureQueryLevels");
	texture_functions.insert("texelFetch");
}

ShaderCompiler::ShaderCompiler() {
}
