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
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "servers/rendering/renderer_rd/forward_clustered/scene_shader_forward_clustered.h"
#include "servers/rendering/renderer_rd/forward_mobile/scene_shader_forward_mobile.h"
#include "servers/rendering/storage/variant_converters.h"
#include "texture_storage.h"

using namespace RendererRD;

///////////////////////////////////////////////////////////////////////////
// UBI helper functions

static void _fill_std140_variant_ubo_value(ShaderLanguage::DataType type, int p_array_size, const Variant &value, uint8_t *data, bool p_linear_color) {
	switch (type) {
		case ShaderLanguage::TYPE_BOOL: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				PackedInt32Array ba = value;
				for (int i = 0; i < ba.size(); i++) {
					ba.set(i, ba[i] ? 1 : 0);
				}
				write_array_std140<int32_t>(ba, gui, p_array_size, 4);
			} else {
				bool v = value;
				gui[0] = v ? 1 : 0;
			}
		} break;
		case ShaderLanguage::TYPE_BVEC2: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				PackedInt32Array ba = convert_array_std140<Vector2i, int32_t>(value);
				for (int i = 0; i < ba.size(); i++) {
					ba.set(i, ba[i] ? 1 : 0);
				}
				write_array_std140<Vector2i>(ba, gui, p_array_size, 4);
			} else {
				uint32_t v = value;
				gui[0] = v & 1 ? 1 : 0;
				gui[1] = v & 2 ? 1 : 0;
			}
		} break;
		case ShaderLanguage::TYPE_BVEC3: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				PackedInt32Array ba = convert_array_std140<Vector3i, int32_t>(value);
				for (int i = 0; i < ba.size(); i++) {
					ba.set(i, ba[i] ? 1 : 0);
				}
				write_array_std140<Vector3i>(ba, gui, p_array_size, 4);
			} else {
				uint32_t v = value;
				gui[0] = (v & 1) ? 1 : 0;
				gui[1] = (v & 2) ? 1 : 0;
				gui[2] = (v & 4) ? 1 : 0;
			}
		} break;
		case ShaderLanguage::TYPE_BVEC4: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				PackedInt32Array ba = convert_array_std140<Vector4i, int32_t>(value);
				for (int i = 0; i < ba.size(); i++) {
					ba.set(i, ba[i] ? 1 : 0);
				}
				write_array_std140<Vector4i>(ba, gui, p_array_size, 4);
			} else {
				uint32_t v = value;
				gui[0] = (v & 1) ? 1 : 0;
				gui[1] = (v & 2) ? 1 : 0;
				gui[2] = (v & 4) ? 1 : 0;
				gui[3] = (v & 8) ? 1 : 0;
			}
		} break;
		case ShaderLanguage::TYPE_INT: {
			int32_t *gui = (int32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = value;
				write_array_std140<int32_t>(iv, gui, p_array_size, 4);
			} else {
				int v = value;
				gui[0] = v;
			}
		} break;
		case ShaderLanguage::TYPE_IVEC2: {
			int32_t *gui = (int32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector2i, int32_t>(value);
				write_array_std140<Vector2i>(iv, gui, p_array_size, 4);
			} else {
				Vector2i v = convert_to_vector<Vector2i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
			}
		} break;
		case ShaderLanguage::TYPE_IVEC3: {
			int32_t *gui = (int32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector3i, int32_t>(value);
				write_array_std140<Vector3i>(iv, gui, p_array_size, 4);
			} else {
				Vector3i v = convert_to_vector<Vector3i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
			}
		} break;
		case ShaderLanguage::TYPE_IVEC4: {
			int32_t *gui = (int32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector4i, int32_t>(value);
				write_array_std140<Vector4i>(iv, gui, p_array_size, 4);
			} else {
				Vector4i v = convert_to_vector<Vector4i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
				gui[3] = v.w;
			}
		} break;
		case ShaderLanguage::TYPE_UINT: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = value;
				write_array_std140<uint32_t>(iv, gui, p_array_size, 4);
			} else {
				int v = value;
				gui[0] = v;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC2: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector2i, int32_t>(value);
				write_array_std140<Vector2i>(iv, gui, p_array_size, 4);
			} else {
				Vector2i v = convert_to_vector<Vector2i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC3: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector3i, int32_t>(value);
				write_array_std140<Vector3i>(iv, gui, p_array_size, 4);
			} else {
				Vector3i v = convert_to_vector<Vector3i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC4: {
			uint32_t *gui = (uint32_t *)data;

			if (p_array_size > 0) {
				const PackedInt32Array &iv = convert_array_std140<Vector4i, int32_t>(value);
				write_array_std140<Vector4i>(iv, gui, p_array_size, 4);
			} else {
				Vector4i v = convert_to_vector<Vector4i>(value);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
				gui[3] = v.w;
			}
		} break;
		case ShaderLanguage::TYPE_FLOAT: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = value;
				write_array_std140<float>(a, gui, p_array_size, 4);
			} else {
				float v = value;
				gui[0] = v;
			}
		} break;
		case ShaderLanguage::TYPE_VEC2: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = convert_array_std140<Vector2, float>(value);
				write_array_std140<Vector2>(a, gui, p_array_size, 4);
			} else {
				Vector2 v = convert_to_vector<Vector2>(value);
				gui[0] = v.x;
				gui[1] = v.y;
			}
		} break;
		case ShaderLanguage::TYPE_VEC3: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = convert_array_std140<Vector3, float>(value, p_linear_color);
				write_array_std140<Vector3>(a, gui, p_array_size, 4);
			} else {
				Vector3 v = convert_to_vector<Vector3>(value, p_linear_color);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
			}
		} break;
		case ShaderLanguage::TYPE_VEC4: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = convert_array_std140<Vector4, float>(value, p_linear_color);
				write_array_std140<Vector4>(a, gui, p_array_size, 4);
			} else {
				Vector4 v = convert_to_vector<Vector4>(value, p_linear_color);
				gui[0] = v.x;
				gui[1] = v.y;
				gui[2] = v.z;
				gui[3] = v.w;
			}
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = value;
				int s = a.size();

				for (int i = 0, j = 0; i < p_array_size * 4; i += 4, j += 8) {
					if (i + 3 < s) {
						gui[j] = a[i];
						gui[j + 1] = a[i + 1];

						gui[j + 4] = a[i + 2];
						gui[j + 5] = a[i + 3];
					} else {
						gui[j] = 1;
						gui[j + 1] = 0;

						gui[j + 4] = 0;
						gui[j + 5] = 1;
					}
					gui[j + 2] = 0; // ignored
					gui[j + 3] = 0; // ignored
					gui[j + 6] = 0; // ignored
					gui[j + 7] = 0; // ignored
				}
			} else {
				Transform2D v = value;

				//in std140 members of mat2 are treated as vec4s
				gui[0] = v.columns[0][0];
				gui[1] = v.columns[0][1];
				gui[2] = 0; // ignored
				gui[3] = 0; // ignored

				gui[4] = v.columns[1][0];
				gui[5] = v.columns[1][1];
				gui[6] = 0; // ignored
				gui[7] = 0; // ignored
			}
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = convert_array_std140<Basis, float>(value);
				const Basis default_basis;
				const int s = a.size();

				for (int i = 0, j = 0; i < p_array_size * 9; i += 9, j += 12) {
					if (i + 8 < s) {
						gui[j] = a[i];
						gui[j + 1] = a[i + 1];
						gui[j + 2] = a[i + 2];
						gui[j + 3] = 0; // Ignored.

						gui[j + 4] = a[i + 3];
						gui[j + 5] = a[i + 4];
						gui[j + 6] = a[i + 5];
						gui[j + 7] = 0; // Ignored.

						gui[j + 8] = a[i + 6];
						gui[j + 9] = a[i + 7];
						gui[j + 10] = a[i + 8];
						gui[j + 11] = 0; // Ignored.
					} else {
						convert_item_std140(default_basis, gui + j);
					}
				}
			} else {
				convert_item_std140<Basis>(value, gui);
			}
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			float *gui = reinterpret_cast<float *>(data);

			if (p_array_size > 0) {
				const PackedFloat32Array &a = convert_array_std140<Projection, float>(value);
				write_array_std140<Projection>(a, gui, p_array_size, 16);
			} else {
				convert_item_std140<Projection>(value, gui);
			}
		} break;
		default: {
		}
	}
}

_FORCE_INLINE_ static void _fill_std140_ubo_value(ShaderLanguage::DataType type, const Vector<ShaderLanguage::Scalar> &value, uint8_t *data) {
	switch (type) {
		case ShaderLanguage::TYPE_BOOL: {
			uint32_t *gui = (uint32_t *)data;
			gui[0] = value[0].boolean ? 1 : 0;
		} break;
		case ShaderLanguage::TYPE_BVEC2: {
			uint32_t *gui = (uint32_t *)data;
			gui[0] = value[0].boolean ? 1 : 0;
			gui[1] = value[1].boolean ? 1 : 0;

		} break;
		case ShaderLanguage::TYPE_BVEC3: {
			uint32_t *gui = (uint32_t *)data;
			gui[0] = value[0].boolean ? 1 : 0;
			gui[1] = value[1].boolean ? 1 : 0;
			gui[2] = value[2].boolean ? 1 : 0;

		} break;
		case ShaderLanguage::TYPE_BVEC4: {
			uint32_t *gui = (uint32_t *)data;
			gui[0] = value[0].boolean ? 1 : 0;
			gui[1] = value[1].boolean ? 1 : 0;
			gui[2] = value[2].boolean ? 1 : 0;
			gui[3] = value[3].boolean ? 1 : 0;

		} break;
		case ShaderLanguage::TYPE_INT: {
			int32_t *gui = (int32_t *)data;
			gui[0] = value[0].sint;

		} break;
		case ShaderLanguage::TYPE_IVEC2: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_IVEC3: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_IVEC4: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].sint;
			}

		} break;
		case ShaderLanguage::TYPE_UINT: {
			uint32_t *gui = (uint32_t *)data;
			gui[0] = value[0].uint;

		} break;
		case ShaderLanguage::TYPE_UVEC2: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].uint;
			}
		} break;
		case ShaderLanguage::TYPE_UVEC3: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].uint;
			}

		} break;
		case ShaderLanguage::TYPE_UVEC4: {
			int32_t *gui = (int32_t *)data;

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].uint;
			}
		} break;
		case ShaderLanguage::TYPE_FLOAT: {
			float *gui = reinterpret_cast<float *>(data);
			gui[0] = value[0].real;

		} break;
		case ShaderLanguage::TYPE_VEC2: {
			float *gui = reinterpret_cast<float *>(data);

			for (int i = 0; i < 2; i++) {
				gui[i] = value[i].real;
			}

		} break;
		case ShaderLanguage::TYPE_VEC3: {
			float *gui = reinterpret_cast<float *>(data);

			for (int i = 0; i < 3; i++) {
				gui[i] = value[i].real;
			}

		} break;
		case ShaderLanguage::TYPE_VEC4: {
			float *gui = reinterpret_cast<float *>(data);

			for (int i = 0; i < 4; i++) {
				gui[i] = value[i].real;
			}
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			float *gui = reinterpret_cast<float *>(data);

			//in std140 members of mat2 are treated as vec4s
			gui[0] = value[0].real;
			gui[1] = value[1].real;
			gui[2] = 0;
			gui[3] = 0;
			gui[4] = value[2].real;
			gui[5] = value[3].real;
			gui[6] = 0;
			gui[7] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			float *gui = reinterpret_cast<float *>(data);

			gui[0] = value[0].real;
			gui[1] = value[1].real;
			gui[2] = value[2].real;
			gui[3] = 0;
			gui[4] = value[3].real;
			gui[5] = value[4].real;
			gui[6] = value[5].real;
			gui[7] = 0;
			gui[8] = value[6].real;
			gui[9] = value[7].real;
			gui[10] = value[8].real;
			gui[11] = 0;
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			float *gui = reinterpret_cast<float *>(data);

			for (int i = 0; i < 16; i++) {
				gui[i] = value[i].real;
			}
		} break;
		default: {
		}
	}
}

_FORCE_INLINE_ static void _fill_std140_ubo_empty(ShaderLanguage::DataType type, int p_array_size, uint8_t *data) {
	if (p_array_size <= 0) {
		p_array_size = 1;
	}

	switch (type) {
		case ShaderLanguage::TYPE_BOOL:
		case ShaderLanguage::TYPE_INT:
		case ShaderLanguage::TYPE_UINT:
		case ShaderLanguage::TYPE_FLOAT: {
			memset(data, 0, 4 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_BVEC2:
		case ShaderLanguage::TYPE_IVEC2:
		case ShaderLanguage::TYPE_UVEC2:
		case ShaderLanguage::TYPE_VEC2: {
			memset(data, 0, 8 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_BVEC3:
		case ShaderLanguage::TYPE_IVEC3:
		case ShaderLanguage::TYPE_UVEC3:
		case ShaderLanguage::TYPE_VEC3: {
			memset(data, 0, 12 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_BVEC4:
		case ShaderLanguage::TYPE_IVEC4:
		case ShaderLanguage::TYPE_UVEC4:
		case ShaderLanguage::TYPE_VEC4: {
			memset(data, 0, 16 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_MAT2: {
			memset(data, 0, 32 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_MAT3: {
			memset(data, 0, 48 * p_array_size);
		} break;
		case ShaderLanguage::TYPE_MAT4: {
			memset(data, 0, 64 * p_array_size);
		} break;

		default: {
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// MaterialStorage::ShaderData

void MaterialStorage::ShaderData::set_path_hint(const String &p_hint) {
	path = p_hint;
}

void MaterialStorage::ShaderData::set_default_texture_parameter(const StringName &p_name, RID p_texture, int p_index) {
	if (!p_texture.is_valid()) {
		if (default_texture_params.has(p_name) && default_texture_params[p_name].has(p_index)) {
			default_texture_params[p_name].erase(p_index);

			if (default_texture_params[p_name].is_empty()) {
				default_texture_params.erase(p_name);
			}
		}
	} else {
		if (!default_texture_params.has(p_name)) {
			default_texture_params[p_name] = HashMap<int, RID>();
		}
		default_texture_params[p_name][p_index] = p_texture;
	}
}

Variant MaterialStorage::ShaderData::get_default_parameter(const StringName &p_parameter) const {
	if (uniforms.has(p_parameter)) {
		ShaderLanguage::ShaderNode::Uniform uniform = uniforms[p_parameter];
		Vector<ShaderLanguage::Scalar> default_value = uniform.default_value;
		return ShaderLanguage::constant_value_to_variant(default_value, uniform.type, uniform.array_size, uniform.hint);
	}
	return Variant();
}

void MaterialStorage::ShaderData::get_shader_uniform_list(List<PropertyInfo> *p_param_list) const {
	SortArray<Pair<StringName, int>, ShaderLanguage::UniformOrderComparator> sorter;
	LocalVector<Pair<StringName, int>> filtered_uniforms;

	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : uniforms) {
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
		const ShaderLanguage::ShaderNode::Uniform &uniform = uniforms[uniform_name];

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

void MaterialStorage::ShaderData::get_instance_param_list(List<RendererMaterialStorage::InstanceShaderParam> *p_param_list) const {
	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : uniforms) {
		if (E.value.scope != ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
			continue;
		}

		RendererMaterialStorage::InstanceShaderParam p;
		p.info = ShaderLanguage::uniform_to_property_info(E.value);
		p.info.name = E.key; //supply name
		p.index = E.value.instance_index;
		p.default_value = ShaderLanguage::constant_value_to_variant(E.value.default_value, E.value.type, E.value.array_size, E.value.hint);
		p_param_list->push_back(p);
	}
}

bool MaterialStorage::ShaderData::is_parameter_texture(const StringName &p_param) const {
	if (!uniforms.has(p_param)) {
		return false;
	}

	return uniforms[p_param].is_texture();
}

RD::PipelineColorBlendState::Attachment MaterialStorage::ShaderData::blend_mode_to_blend_attachment(BlendMode p_mode) {
	RD::PipelineColorBlendState::Attachment attachment;

	switch (p_mode) {
		case BLEND_MODE_MIX: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_ADD;
			attachment.color_blend_op = RD::BLEND_OP_ADD;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		} break;
		case BLEND_MODE_ADD: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_ADD;
			attachment.color_blend_op = RD::BLEND_OP_ADD;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		} break;
		case BLEND_MODE_SUB: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
			attachment.color_blend_op = RD::BLEND_OP_REVERSE_SUBTRACT;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		} break;
		case BLEND_MODE_MUL: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_ADD;
			attachment.color_blend_op = RD::BLEND_OP_ADD;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_DST_COLOR;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ZERO;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_DST_ALPHA;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
		} break;
		case BLEND_MODE_ALPHA_TO_COVERAGE: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_ADD;
			attachment.color_blend_op = RD::BLEND_OP_ADD;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ZERO;
		} break;
		case BLEND_MODE_PREMULTIPLIED_ALPHA: {
			attachment.enable_blend = true;
			attachment.alpha_blend_op = RD::BLEND_OP_ADD;
			attachment.color_blend_op = RD::BLEND_OP_ADD;
			attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
			attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		} break;
		case BLEND_MODE_DISABLED:
		default: {
			// Use default attachment values.
		} break;
	}

	return attachment;
}

bool MaterialStorage::ShaderData::blend_mode_uses_blend_alpha(BlendMode p_mode) {
	switch (p_mode) {
		case BLEND_MODE_MIX:
			return false;
		case BLEND_MODE_ADD:
			return true;
		case BLEND_MODE_SUB:
			return true;
		case BLEND_MODE_MUL:
			return true;
		case BLEND_MODE_ALPHA_TO_COVERAGE:
			return false;
		case BLEND_MODE_PREMULTIPLIED_ALPHA:
			return true;
		case BLEND_MODE_DISABLED:
		default:
			return false;
	}
}

///////////////////////////////////////////////////////////////////////////
// MaterialStorage::MaterialData

void MaterialStorage::MaterialData::update_uniform_buffer(const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, const HashMap<StringName, Variant> &p_parameters, uint8_t *p_buffer, uint32_t p_buffer_size, bool p_use_linear_color) {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	bool uses_global_buffer = false;

	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : p_uniforms) {
		if (E.value.is_texture()) {
			continue; // texture, does not go here
		}

		if (E.value.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
			continue; //instance uniforms don't appear in the buffer
		}

		if (E.value.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
				E.value.hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
				E.value.hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
			continue;
		}

		if (E.value.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL) {
			//this is a global variable, get the index to it
			GlobalShaderUniforms::Variable *gv = material_storage->global_shader_uniforms.variables.getptr(E.key);
			uint32_t index = 0;
			if (gv) {
				index = gv->buffer_index;
			} else {
				WARN_PRINT("Shader uses global parameter '" + E.key + "', but it was removed at some point. Material will not display correctly.");
			}

			uint32_t offset = p_uniform_offsets[E.value.order];
			uint32_t *intptr = (uint32_t *)&p_buffer[offset];
			*intptr = index;
			uses_global_buffer = true;
			continue;
		}

		//regular uniform
		uint32_t offset = p_uniform_offsets[E.value.order];
#ifdef DEBUG_ENABLED
		uint32_t size = 0U;
		// The following code enforces a 16-byte alignment of uniform arrays.
		if (E.value.array_size > 0) {
			size = ShaderLanguage::get_datatype_size(E.value.type) * E.value.array_size;
			int m = (16 * E.value.array_size);
			if ((size % m) != 0U) {
				size += m - (size % m);
			}
		} else {
			size = ShaderLanguage::get_datatype_size(E.value.type);
		}
		ERR_CONTINUE(offset + size > p_buffer_size);
#endif
		uint8_t *data = &p_buffer[offset];
		HashMap<StringName, Variant>::ConstIterator V = p_parameters.find(E.key);

		if (V) {
			//user provided
			_fill_std140_variant_ubo_value(E.value.type, E.value.array_size, V->value, data, p_use_linear_color);

		} else if (E.value.default_value.size()) {
			//default value
			_fill_std140_ubo_value(E.value.type, E.value.default_value, data);
			//value=E.value.default_value;
		} else {
			//zero because it was not provided
			if ((E.value.type == ShaderLanguage::TYPE_VEC3 || E.value.type == ShaderLanguage::TYPE_VEC4) && E.value.hint == ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR) {
				//colors must be set as black, with alpha as 1.0
				_fill_std140_variant_ubo_value(E.value.type, E.value.array_size, Color(0, 0, 0, 1), data, p_use_linear_color);
			} else {
				//else just zero it out
				_fill_std140_ubo_empty(E.value.type, E.value.array_size, data);
			}
		}
	}

	if (uses_global_buffer != (global_buffer_E != nullptr)) {
		if (uses_global_buffer) {
			global_buffer_E = material_storage->global_shader_uniforms.materials_using_buffer.push_back(self);
		} else {
			material_storage->global_shader_uniforms.materials_using_buffer.erase(global_buffer_E);
			global_buffer_E = nullptr;
		}
	}
}

MaterialStorage::MaterialData::~MaterialData() {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

	if (global_buffer_E) {
		//unregister global buffers
		material_storage->global_shader_uniforms.materials_using_buffer.erase(global_buffer_E);
	}

	if (global_texture_E) {
		//unregister global textures

		for (const KeyValue<StringName, uint64_t> &E : used_global_textures) {
			GlobalShaderUniforms::Variable *v = material_storage->global_shader_uniforms.variables.getptr(E.key);
			if (v) {
				v->texture_materials.erase(self);
			}
		}
		//unregister material from those using global textures
		material_storage->global_shader_uniforms.materials_using_texture.erase(global_texture_E);
	}

	for (int i = 0; i < 2; i++) {
		if (uniform_buffer[i].is_valid()) {
			RD::get_singleton()->free(uniform_buffer[i]);
		}
	}
}

void MaterialStorage::MaterialData::update_textures(const HashMap<StringName, Variant> &p_parameters, const HashMap<StringName, HashMap<int, RID>> &p_default_textures, const Vector<ShaderCompiler::GeneratedCode::Texture> &p_texture_uniforms, RID *p_textures, bool p_use_linear_color, bool p_3d_material) {
	TextureStorage *texture_storage = TextureStorage::get_singleton();
	MaterialStorage *material_storage = MaterialStorage::get_singleton();

#ifdef TOOLS_ENABLED
	TextureStorage::Texture *roughness_detect_texture = nullptr;
	RS::TextureDetectRoughnessChannel roughness_channel = RS::TEXTURE_DETECT_ROUGHNESS_R;
	TextureStorage::Texture *normal_detect_texture = nullptr;
#endif

	bool uses_global_textures = false;
	global_textures_pass++;

	for (int i = 0, k = 0; i < p_texture_uniforms.size(); i++) {
		const StringName &uniform_name = p_texture_uniforms[i].name;
		int uniform_array_size = p_texture_uniforms[i].array_size;

		Vector<RID> textures;

		if (p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_SCREEN_TEXTURE ||
				p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL_ROUGHNESS_TEXTURE ||
				p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEPTH_TEXTURE) {
			continue;
		}

		if (p_texture_uniforms[i].global) {
			uses_global_textures = true;

			GlobalShaderUniforms::Variable *v = material_storage->global_shader_uniforms.variables.getptr(uniform_name);
			if (v) {
				if (v->buffer_index >= 0) {
					WARN_PRINT("Shader uses global parameter texture '" + String(uniform_name) + "', but it changed type and is no longer a texture!.");

				} else {
					HashMap<StringName, uint64_t>::Iterator E = used_global_textures.find(uniform_name);
					if (!E) {
						E = used_global_textures.insert(uniform_name, global_textures_pass);
						v->texture_materials.insert(self);
					} else {
						E->value = global_textures_pass;
					}

					textures.push_back(v->override.get_type() != Variant::NIL ? v->override : v->value);
				}

			} else {
				WARN_PRINT("Shader uses global parameter texture '" + String(uniform_name) + "', but it was removed at some point. Material will not display correctly.");
			}
		} else {
			HashMap<StringName, Variant>::ConstIterator V = p_parameters.find(uniform_name);
			if (V) {
				if (V->value.is_array()) {
					Array array = (Array)V->value;
					if (uniform_array_size > 0) {
						int size = MIN(uniform_array_size, array.size());
						for (int j = 0; j < size; j++) {
							textures.push_back(array[j]);
						}
					} else {
						if (array.size() > 0) {
							textures.push_back(array[0]);
						}
					}
				} else {
					textures.push_back(V->value);
				}
			}

			if (uniform_array_size > 0) {
				if (textures.size() < uniform_array_size) {
					HashMap<StringName, HashMap<int, RID>>::ConstIterator W = p_default_textures.find(uniform_name);
					for (int j = textures.size(); j < uniform_array_size; j++) {
						if (W && W->value.has(j)) {
							textures.push_back(W->value[j]);
						} else {
							textures.push_back(RID());
						}
					}
				}
			} else if (textures.is_empty()) {
				HashMap<StringName, HashMap<int, RID>>::ConstIterator W = p_default_textures.find(uniform_name);
				if (W && W->value.has(0)) {
					textures.push_back(W->value[0]);
				}
			}
		}

		RID rd_texture;

		if (textures.is_empty()) {
			//check default usage
			switch (p_texture_uniforms[i].type) {
				case ShaderLanguage::TYPE_ISAMPLER2D:
				case ShaderLanguage::TYPE_USAMPLER2D:
				case ShaderLanguage::TYPE_SAMPLER2D: {
					switch (p_texture_uniforms[i].hint) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_BLACK: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_BLACK);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_TRANSPARENT: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_TRANSPARENT);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ANISOTROPY: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_ANISO);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_NORMAL);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_NORMAL);
						} break;
						default: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
						} break;
					}
				} break;

				case ShaderLanguage::TYPE_SAMPLERCUBE: {
					switch (p_texture_uniforms[i].hint) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_BLACK: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_BLACK);
						} break;
						default: {
							rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_WHITE);
						} break;
					}
				} break;
				case ShaderLanguage::TYPE_SAMPLERCUBEARRAY: {
					rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_CUBEMAP_ARRAY_BLACK);
				} break;

				case ShaderLanguage::TYPE_ISAMPLER3D:
				case ShaderLanguage::TYPE_USAMPLER3D:
				case ShaderLanguage::TYPE_SAMPLER3D: {
					rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_3D_WHITE);
				} break;

				case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
				case ShaderLanguage::TYPE_USAMPLER2DARRAY:
				case ShaderLanguage::TYPE_SAMPLER2DARRAY: {
					rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_2D_ARRAY_WHITE);
				} break;

				default: {
				}
			}
#ifdef TOOLS_ENABLED
			if (roughness_detect_texture && normal_detect_texture && !normal_detect_texture->path.is_empty()) {
				roughness_detect_texture->detect_roughness_callback(roughness_detect_texture->detect_roughness_callback_ud, normal_detect_texture->path, roughness_channel);
			}
#endif
			if (uniform_array_size > 0) {
				for (int j = 0; j < uniform_array_size; j++) {
					p_textures[k++] = rd_texture;
				}
			} else {
				p_textures[k++] = rd_texture;
			}
		} else {
			bool srgb = p_use_linear_color && p_texture_uniforms[i].use_color;

			for (int j = 0; j < textures.size(); j++) {
				TextureStorage::Texture *tex = TextureStorage::get_singleton()->get_texture(textures[j]);

				if (tex) {
					rd_texture = (srgb && tex->rd_texture_srgb.is_valid()) ? tex->rd_texture_srgb : tex->rd_texture;
#ifdef TOOLS_ENABLED
					if (tex->detect_3d_callback && p_3d_material) {
						tex->detect_3d_callback(tex->detect_3d_callback_ud);
					}
					if (tex->detect_normal_callback && (p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL || p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL)) {
						if (p_texture_uniforms[i].hint == ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL) {
							normal_detect_texture = tex;
						}
						tex->detect_normal_callback(tex->detect_normal_callback_ud);
					}
					if (tex->detect_roughness_callback && (p_texture_uniforms[i].hint >= ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_R || p_texture_uniforms[i].hint <= ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_GRAY)) {
						//find the normal texture
						roughness_detect_texture = tex;
						roughness_channel = RS::TextureDetectRoughnessChannel(p_texture_uniforms[i].hint - ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_R);
					}
#endif // TOOLS_ENABLED
					if (tex->render_target) {
						tex->render_target->was_used = true;
						render_target_cache.push_back(tex->render_target);
					}
				}
				if (rd_texture.is_null()) {
					rd_texture = texture_storage->texture_rd_get_default(TextureStorage::DEFAULT_RD_TEXTURE_WHITE);
				}
#ifdef TOOLS_ENABLED
				if (roughness_detect_texture && normal_detect_texture && !normal_detect_texture->path.is_empty()) {
					roughness_detect_texture->detect_roughness_callback(roughness_detect_texture->detect_roughness_callback_ud, normal_detect_texture->path, roughness_channel);
				}
#endif
				p_textures[k++] = rd_texture;
			}
		}
	}
	{
		//for textures no longer used, unregister them
		List<StringName> to_delete;
		for (KeyValue<StringName, uint64_t> &E : used_global_textures) {
			if (E.value != global_textures_pass) {
				to_delete.push_back(E.key);

				GlobalShaderUniforms::Variable *v = material_storage->global_shader_uniforms.variables.getptr(E.key);
				if (v) {
					v->texture_materials.erase(self);
				}
			}
		}

		while (to_delete.front()) {
			used_global_textures.erase(to_delete.front()->get());
			to_delete.pop_front();
		}
		//handle registering/unregistering global textures
		if (uses_global_textures != (global_texture_E != nullptr)) {
			if (uses_global_textures) {
				global_texture_E = material_storage->global_shader_uniforms.materials_using_texture.push_back(self);
			} else {
				material_storage->global_shader_uniforms.materials_using_texture.erase(global_texture_E);
				global_texture_E = nullptr;
			}
		}
	}
}

void MaterialStorage::MaterialData::free_parameters_uniform_set(RID p_uniform_set) {
	if (p_uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(p_uniform_set)) {
		RD::get_singleton()->uniform_set_set_invalidation_callback(p_uniform_set, nullptr, nullptr);
		RD::get_singleton()->free(p_uniform_set);
	}
}

bool MaterialStorage::MaterialData::update_parameters_uniform_set(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, bool p_textures_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, const Vector<ShaderCompiler::GeneratedCode::Texture> &p_texture_uniforms, const HashMap<StringName, HashMap<int, RID>> &p_default_texture_params, uint32_t p_ubo_size, RID &uniform_set, RID p_shader, uint32_t p_shader_uniform_set, bool p_use_linear_color, bool p_3d_material) {
	if ((uint32_t)ubo_data[p_use_linear_color].size() != p_ubo_size) {
		p_uniform_dirty = true;
		if (uniform_buffer[p_use_linear_color].is_valid()) {
			RD::get_singleton()->free(uniform_buffer[p_use_linear_color]);
			uniform_buffer[p_use_linear_color] = RID();
		}

		ubo_data[p_use_linear_color].resize(p_ubo_size);
		if (ubo_data[p_use_linear_color].size()) {
			uniform_buffer[p_use_linear_color] = RD::get_singleton()->uniform_buffer_create(ubo_data[p_use_linear_color].size());
			memset(ubo_data[p_use_linear_color].ptrw(), 0, ubo_data[p_use_linear_color].size()); //clear
		}

		//clear previous uniform set
		if (uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(uniform_set)) {
			RD::get_singleton()->uniform_set_set_invalidation_callback(uniform_set, nullptr, nullptr);
			RD::get_singleton()->free(uniform_set);
			uniform_set = RID();
		}
	}

	//check whether buffer changed
	if (p_uniform_dirty && ubo_data[p_use_linear_color].size()) {
		update_uniform_buffer(p_uniforms, p_uniform_offsets, p_parameters, ubo_data[p_use_linear_color].ptrw(), ubo_data[p_use_linear_color].size(), p_use_linear_color);
		RD::get_singleton()->buffer_update(uniform_buffer[p_use_linear_color], 0, ubo_data[p_use_linear_color].size(), ubo_data[p_use_linear_color].ptrw());
	}

	uint32_t tex_uniform_count = 0U;
	for (int i = 0; i < p_texture_uniforms.size(); i++) {
		tex_uniform_count += uint32_t(p_texture_uniforms[i].array_size > 0 ? p_texture_uniforms[i].array_size : 1);
	}

	if ((uint32_t)texture_cache.size() != tex_uniform_count || p_textures_dirty) {
		texture_cache.resize(tex_uniform_count);
		render_target_cache.clear();
		p_textures_dirty = true;

		//clear previous uniform set
		if (uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(uniform_set)) {
			RD::get_singleton()->uniform_set_set_invalidation_callback(uniform_set, nullptr, nullptr);
			RD::get_singleton()->free(uniform_set);
			uniform_set = RID();
		}
	}

	if (p_textures_dirty && tex_uniform_count) {
		update_textures(p_parameters, p_default_texture_params, p_texture_uniforms, texture_cache.ptrw(), p_use_linear_color, p_3d_material);
	}

	if (p_ubo_size == 0 && (p_texture_uniforms.size() == 0)) {
		// This material does not require an uniform set, so don't create it.
		return false;
	}

	if (!p_textures_dirty && uniform_set.is_valid() && RD::get_singleton()->uniform_set_is_valid(uniform_set)) {
		//no reason to update uniform set, only UBO (or nothing) was needed to update
		return false;
	}

	Vector<RD::Uniform> uniforms;

	{
		if (p_ubo_size) {
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
			u.binding = 0;
			u.append_id(uniform_buffer[p_use_linear_color]);
			uniforms.push_back(u);
		}

		const RID *textures = texture_cache.ptrw();
		for (int i = 0, k = 0; i < p_texture_uniforms.size(); i++) {
			const int array_size = p_texture_uniforms[i].array_size;

			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_TEXTURE;
			u.binding = 1 + k;
			if (array_size > 0) {
				for (int j = 0; j < array_size; j++) {
					u.append_id(textures[k++]);
				}
			} else {
				u.append_id(textures[k++]);
			}
			uniforms.push_back(u);
		}
	}

	uniform_set = RD::get_singleton()->uniform_set_create(uniforms, p_shader, p_shader_uniform_set);

	RD::get_singleton()->uniform_set_set_invalidation_callback(uniform_set, MaterialStorage::_material_uniform_set_erased, &self);

	return true;
}

void MaterialStorage::MaterialData::set_as_used() {
	for (int i = 0; i < render_target_cache.size(); i++) {
		render_target_cache[i]->was_used = true;
	}
}

///////////////////////////////////////////////////////////////////////////
// MaterialStorage::Samplers

template void MaterialStorage::Samplers::append_uniforms(LocalVector<RD::Uniform> &p_uniforms, int p_first_index) const;

template void MaterialStorage::Samplers::append_uniforms(Vector<RD::Uniform> &p_uniforms, int p_first_index) const;

template <typename Collection>
void MaterialStorage::Samplers::append_uniforms(Collection &p_uniforms, int p_first_index) const {
	// Binding ids are aligned with samplers_inc.glsl.
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 0, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 1, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 2, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 3, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 4, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 5, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC][RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 6, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 7, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 8, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 9, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 10, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
	p_uniforms.push_back(RD::Uniform(RD::UNIFORM_TYPE_SAMPLER, p_first_index + 11, rids[RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC][RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED]));
}

bool MaterialStorage::Samplers::is_valid() const {
	return rids[1][1].is_valid();
}

bool MaterialStorage::Samplers::is_null() const {
	return rids[1][1].is_null();
}

///////////////////////////////////////////////////////////////////////////
// MaterialStorage

MaterialStorage *MaterialStorage::singleton = nullptr;

MaterialStorage *MaterialStorage::get_singleton() {
	return singleton;
}

MaterialStorage::MaterialStorage() {
	singleton = this;

	//default samplers
	default_samplers = samplers_rd_allocate();

	// buffers
	{ //create index array for copy shaders
		Vector<uint8_t> pv;
		pv.resize(6 * 2);
		{
			uint8_t *w = pv.ptrw();
			uint16_t *p16 = (uint16_t *)w;
			p16[0] = 0;
			p16[1] = 1;
			p16[2] = 2;
			p16[3] = 0;
			p16[4] = 2;
			p16[5] = 3;
		}
		quad_index_buffer = RD::get_singleton()->index_buffer_create(6, RenderingDevice::INDEX_BUFFER_FORMAT_UINT16, pv);
		quad_index_array = RD::get_singleton()->index_array_create(quad_index_buffer, 0, 6);
	}

	// Shaders
	for (int i = 0; i < SHADER_TYPE_MAX; i++) {
		shader_data_request_func[i] = nullptr;
	}

	static_assert(sizeof(GlobalShaderUniforms::Value) == 16);

	global_shader_uniforms.buffer_size = MAX(4096, (int)GLOBAL_GET("rendering/limits/global_shader_variables/buffer_size"));
	global_shader_uniforms.buffer_values = memnew_arr(GlobalShaderUniforms::Value, global_shader_uniforms.buffer_size);
	memset(global_shader_uniforms.buffer_values, 0, sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size);
	global_shader_uniforms.buffer_usage = memnew_arr(GlobalShaderUniforms::ValueUsage, global_shader_uniforms.buffer_size);
	global_shader_uniforms.buffer_dirty_regions = memnew_arr(bool, 1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE));
	memset(global_shader_uniforms.buffer_dirty_regions, 0, sizeof(bool) * (1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE)));
	global_shader_uniforms.buffer = RD::get_singleton()->storage_buffer_create(sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size);
}

MaterialStorage::~MaterialStorage() {
	memdelete_arr(global_shader_uniforms.buffer_values);
	memdelete_arr(global_shader_uniforms.buffer_usage);
	memdelete_arr(global_shader_uniforms.buffer_dirty_regions);
	RD::get_singleton()->free(global_shader_uniforms.buffer);

	// buffers

	RD::get_singleton()->free(quad_index_buffer); //array gets freed as dependency

	//def samplers
	samplers_rd_free(default_samplers);

	singleton = nullptr;
}

bool MaterialStorage::free(RID p_rid) {
	if (owns_shader(p_rid)) {
		shader_free(p_rid);
		return true;
	} else if (owns_material(p_rid)) {
		material_free(p_rid);
		return true;
	}

	return false;
}

/* GLOBAL SHADER UNIFORM API */

int32_t MaterialStorage::_global_shader_uniform_allocate(uint32_t p_elements) {
	int32_t idx = 0;
	while (idx + p_elements <= global_shader_uniforms.buffer_size) {
		if (global_shader_uniforms.buffer_usage[idx].elements == 0) {
			bool valid = true;
			for (uint32_t i = 1; i < p_elements; i++) {
				if (global_shader_uniforms.buffer_usage[idx + i].elements > 0) {
					valid = false;
					idx += i + global_shader_uniforms.buffer_usage[idx + i].elements;
					break;
				}
			}

			if (!valid) {
				continue; //if not valid, idx is in new position
			}

			return idx;
		} else {
			idx += global_shader_uniforms.buffer_usage[idx].elements;
		}
	}

	return -1;
}

void MaterialStorage::_global_shader_uniform_store_in_buffer(int32_t p_index, RS::GlobalShaderParameterType p_type, const Variant &p_value) {
	switch (p_type) {
		case RS::GLOBAL_VAR_TYPE_BOOL: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			bool b = p_value;
			bv.x = b ? 1.0 : 0.0;
			bv.y = 0.0;
			bv.z = 0.0;
			bv.w = 0.0;

		} break;
		case RS::GLOBAL_VAR_TYPE_BVEC2: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			uint32_t bvec = p_value;
			bv.x = (bvec & 1) ? 1.0 : 0.0;
			bv.y = (bvec & 2) ? 1.0 : 0.0;
			bv.z = 0.0;
			bv.w = 0.0;
		} break;
		case RS::GLOBAL_VAR_TYPE_BVEC3: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			uint32_t bvec = p_value;
			bv.x = (bvec & 1) ? 1.0 : 0.0;
			bv.y = (bvec & 2) ? 1.0 : 0.0;
			bv.z = (bvec & 4) ? 1.0 : 0.0;
			bv.w = 0.0;
		} break;
		case RS::GLOBAL_VAR_TYPE_BVEC4: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			uint32_t bvec = p_value;
			bv.x = (bvec & 1) ? 1.0 : 0.0;
			bv.y = (bvec & 2) ? 1.0 : 0.0;
			bv.z = (bvec & 4) ? 1.0 : 0.0;
			bv.w = (bvec & 8) ? 1.0 : 0.0;
		} break;
		case RS::GLOBAL_VAR_TYPE_INT: {
			GlobalShaderUniforms::ValueInt &bv = *(GlobalShaderUniforms::ValueInt *)&global_shader_uniforms.buffer_values[p_index];
			int32_t v = p_value;
			bv.x = v;
			bv.y = 0;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_IVEC2: {
			GlobalShaderUniforms::ValueInt &bv = *(GlobalShaderUniforms::ValueInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector2i v = convert_to_vector<Vector2i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_IVEC3: {
			GlobalShaderUniforms::ValueInt &bv = *(GlobalShaderUniforms::ValueInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector3i v = convert_to_vector<Vector3i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_IVEC4: {
			GlobalShaderUniforms::ValueInt &bv = *(GlobalShaderUniforms::ValueInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector4i v = convert_to_vector<Vector4i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = v.w;
		} break;
		case RS::GLOBAL_VAR_TYPE_RECT2I: {
			GlobalShaderUniforms::ValueInt &bv = *(GlobalShaderUniforms::ValueInt *)&global_shader_uniforms.buffer_values[p_index];
			Rect2i v = p_value;
			bv.x = v.position.x;
			bv.y = v.position.y;
			bv.z = v.size.x;
			bv.w = v.size.y;
		} break;
		case RS::GLOBAL_VAR_TYPE_UINT: {
			GlobalShaderUniforms::ValueUInt &bv = *(GlobalShaderUniforms::ValueUInt *)&global_shader_uniforms.buffer_values[p_index];
			uint32_t v = p_value;
			bv.x = v;
			bv.y = 0;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_UVEC2: {
			GlobalShaderUniforms::ValueUInt &bv = *(GlobalShaderUniforms::ValueUInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector2i v = convert_to_vector<Vector2i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_UVEC3: {
			GlobalShaderUniforms::ValueUInt &bv = *(GlobalShaderUniforms::ValueUInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector3i v = convert_to_vector<Vector3i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_UVEC4: {
			GlobalShaderUniforms::ValueUInt &bv = *(GlobalShaderUniforms::ValueUInt *)&global_shader_uniforms.buffer_values[p_index];
			Vector4i v = convert_to_vector<Vector4i>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = v.w;
		} break;
		case RS::GLOBAL_VAR_TYPE_FLOAT: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			float v = p_value;
			bv.x = v;
			bv.y = 0;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_VEC2: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			Vector2 v = convert_to_vector<Vector2>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = 0;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_VEC3: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			Vector3 v = convert_to_vector<Vector3>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = 0;
		} break;
		case RS::GLOBAL_VAR_TYPE_VEC4: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			Vector4 v = convert_to_vector<Vector4>(p_value);
			bv.x = v.x;
			bv.y = v.y;
			bv.z = v.z;
			bv.w = v.w;
		} break;
		case RS::GLOBAL_VAR_TYPE_COLOR: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			Color v = p_value;
			bv.x = v.r;
			bv.y = v.g;
			bv.z = v.b;
			bv.w = v.a;

			GlobalShaderUniforms::Value &bv_linear = global_shader_uniforms.buffer_values[p_index + 1];
			v = v.srgb_to_linear();
			bv_linear.x = v.r;
			bv_linear.y = v.g;
			bv_linear.z = v.b;
			bv_linear.w = v.a;

		} break;
		case RS::GLOBAL_VAR_TYPE_RECT2: {
			GlobalShaderUniforms::Value &bv = global_shader_uniforms.buffer_values[p_index];
			Rect2 v = p_value;
			bv.x = v.position.x;
			bv.y = v.position.y;
			bv.z = v.size.x;
			bv.w = v.size.y;
		} break;
		case RS::GLOBAL_VAR_TYPE_MAT2: {
			GlobalShaderUniforms::Value *bv = &global_shader_uniforms.buffer_values[p_index];
			Vector<float> m2 = p_value;
			if (m2.size() < 4) {
				m2.resize(4);
			}
			bv[0].x = m2[0];
			bv[0].y = m2[1];
			bv[0].z = 0;
			bv[0].w = 0;

			bv[1].x = m2[2];
			bv[1].y = m2[3];
			bv[1].z = 0;
			bv[1].w = 0;

		} break;
		case RS::GLOBAL_VAR_TYPE_MAT3: {
			GlobalShaderUniforms::Value *bv = &global_shader_uniforms.buffer_values[p_index];
			Basis v = p_value;
			convert_item_std140<Basis>(v, &bv->x);

		} break;
		case RS::GLOBAL_VAR_TYPE_MAT4: {
			GlobalShaderUniforms::Value *bv = &global_shader_uniforms.buffer_values[p_index];
			Projection m = p_value;
			convert_item_std140<Projection>(m, &bv->x);

		} break;
		case RS::GLOBAL_VAR_TYPE_TRANSFORM_2D: {
			GlobalShaderUniforms::Value *bv = &global_shader_uniforms.buffer_values[p_index];
			Transform2D v = p_value;
			convert_item_std140<Transform2D>(v, &bv->x);

		} break;
		case RS::GLOBAL_VAR_TYPE_TRANSFORM: {
			GlobalShaderUniforms::Value *bv = &global_shader_uniforms.buffer_values[p_index];
			Transform3D v = p_value;
			convert_item_std140<Transform3D>(v, &bv->x);

		} break;
		default: {
			ERR_FAIL();
		}
	}
}

void MaterialStorage::_global_shader_uniform_mark_buffer_dirty(int32_t p_index, int32_t p_elements) {
	int32_t prev_chunk = -1;

	for (int32_t i = 0; i < p_elements; i++) {
		int32_t chunk = (p_index + i) / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;
		if (chunk != prev_chunk) {
			if (!global_shader_uniforms.buffer_dirty_regions[chunk]) {
				global_shader_uniforms.buffer_dirty_regions[chunk] = true;
				global_shader_uniforms.buffer_dirty_region_count++;
			}
		}

		prev_chunk = chunk;
	}
}

void MaterialStorage::global_shader_parameter_add(const StringName &p_name, RS::GlobalShaderParameterType p_type, const Variant &p_value) {
	ERR_FAIL_COND(global_shader_uniforms.variables.has(p_name));
	GlobalShaderUniforms::Variable gv;
	gv.type = p_type;
	gv.value = p_value;
	gv.buffer_index = -1;

	if (p_type >= RS::GLOBAL_VAR_TYPE_SAMPLER2D) {
		//is texture
		global_shader_uniforms.must_update_texture_materials = true; //normally there are none
	} else {
		gv.buffer_elements = 1;
		if (p_type == RS::GLOBAL_VAR_TYPE_COLOR || p_type == RS::GLOBAL_VAR_TYPE_MAT2) {
			//color needs to elements to store srgb and linear
			gv.buffer_elements = 2;
		}
		if (p_type == RS::GLOBAL_VAR_TYPE_MAT3 || p_type == RS::GLOBAL_VAR_TYPE_TRANSFORM_2D) {
			//color needs to elements to store srgb and linear
			gv.buffer_elements = 3;
		}
		if (p_type == RS::GLOBAL_VAR_TYPE_MAT4 || p_type == RS::GLOBAL_VAR_TYPE_TRANSFORM) {
			//color needs to elements to store srgb and linear
			gv.buffer_elements = 4;
		}

		//is vector, allocate in buffer and update index
		gv.buffer_index = _global_shader_uniform_allocate(gv.buffer_elements);
		ERR_FAIL_COND_MSG(gv.buffer_index < 0, vformat("Failed allocating global variable '%s' out of buffer memory. Consider increasing it in the Project Settings.", String(p_name)));
		global_shader_uniforms.buffer_usage[gv.buffer_index].elements = gv.buffer_elements;
		_global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
		_global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);

		global_shader_uniforms.must_update_buffer_materials = true; //normally there are none
	}

	global_shader_uniforms.variables[p_name] = gv;
}

void MaterialStorage::global_shader_parameter_remove(const StringName &p_name) {
	if (!global_shader_uniforms.variables.has(p_name)) {
		return;
	}
	const GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];

	if (gv.buffer_index >= 0) {
		global_shader_uniforms.buffer_usage[gv.buffer_index].elements = 0;
		global_shader_uniforms.must_update_buffer_materials = true;
	} else {
		global_shader_uniforms.must_update_texture_materials = true;
	}

	global_shader_uniforms.variables.erase(p_name);
}

Vector<StringName> MaterialStorage::global_shader_parameter_get_list() const {
	if (!Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_V_MSG(Vector<StringName>(), "This function should never be used outside the editor, it can severely damage performance.");
	}

	Vector<StringName> names;
	for (const KeyValue<StringName, GlobalShaderUniforms::Variable> &E : global_shader_uniforms.variables) {
		names.push_back(E.key);
	}
	names.sort_custom<StringName::AlphCompare>();
	return names;
}

void MaterialStorage::global_shader_parameter_set(const StringName &p_name, const Variant &p_value) {
	ERR_FAIL_COND(!global_shader_uniforms.variables.has(p_name));
	GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];
	gv.value = p_value;
	if (gv.override.get_type() == Variant::NIL) {
		if (gv.buffer_index >= 0) {
			//buffer
			_global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
			_global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);
		} else {
			//texture
			MaterialStorage *material_storage = MaterialStorage::get_singleton();
			for (const RID &E : gv.texture_materials) {
				Material *material = material_storage->get_material(E);
				ERR_CONTINUE(!material);
				material_storage->_material_queue_update(material, false, true);
			}
		}
	}
}

void MaterialStorage::global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) {
	if (!global_shader_uniforms.variables.has(p_name)) {
		return; //variable may not exist
	}

	ERR_FAIL_COND(p_value.get_type() == Variant::OBJECT);

	GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];

	gv.override = p_value;

	if (gv.buffer_index >= 0) {
		//buffer
		if (gv.override.get_type() == Variant::NIL) {
			_global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
		} else {
			_global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.override);
		}

		_global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);
	} else {
		//texture
		MaterialStorage *material_storage = MaterialStorage::get_singleton();
		for (const RID &E : gv.texture_materials) {
			Material *material = material_storage->get_material(E);
			ERR_CONTINUE(!material);
			material_storage->_material_queue_update(material, false, true);
		}
	}
}

Variant MaterialStorage::global_shader_parameter_get(const StringName &p_name) const {
	if (!Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_V_MSG(Variant(), "This function should never be used outside the editor, it can severely damage performance.");
	}

	if (!global_shader_uniforms.variables.has(p_name)) {
		return Variant();
	}

	return global_shader_uniforms.variables[p_name].value;
}

RS::GlobalShaderParameterType MaterialStorage::global_shader_parameter_get_type_internal(const StringName &p_name) const {
	if (!global_shader_uniforms.variables.has(p_name)) {
		return RS::GLOBAL_VAR_TYPE_MAX;
	}

	return global_shader_uniforms.variables[p_name].type;
}

RS::GlobalShaderParameterType MaterialStorage::global_shader_parameter_get_type(const StringName &p_name) const {
	if (!Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_V_MSG(RS::GLOBAL_VAR_TYPE_MAX, "This function should never be used outside the editor, it can severely damage performance.");
	}

	return global_shader_parameter_get_type_internal(p_name);
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
				"samplerExternalOES",
			};

			RS::GlobalShaderParameterType gvtype = RS::GLOBAL_VAR_TYPE_MAX;

			for (int i = 0; i < RS::GLOBAL_VAR_TYPE_MAX; i++) {
				if (global_var_type_names[i] == type) {
					gvtype = RS::GlobalShaderParameterType(i);
					break;
				}
			}

			ERR_CONTINUE(gvtype == RS::GLOBAL_VAR_TYPE_MAX); //type invalid

			Variant value = d["value"];

			if (gvtype >= RS::GLOBAL_VAR_TYPE_SAMPLER2D) {
				String path = value;
				// Don't load the textures, but still add the parameter so shaders compile correctly while loading.
				if (!p_load_textures || path.is_empty()) {
					value = RID();
				} else {
					Ref<Resource> resource = ResourceLoader::load(path);
					value = resource;
				}
			}

			if (global_shader_uniforms.variables.has(name)) {
				//has it, update it
				global_shader_parameter_set(name, value);
			} else {
				global_shader_parameter_add(name, gvtype, value);
			}
		}
	}
}

void MaterialStorage::global_shader_parameters_clear() {
	global_shader_uniforms.variables.clear(); //not right but for now enough
}

RID MaterialStorage::global_shader_uniforms_get_storage_buffer() const {
	return global_shader_uniforms.buffer;
}

int32_t MaterialStorage::global_shader_parameters_instance_allocate(RID p_instance) {
	ERR_FAIL_COND_V(global_shader_uniforms.instance_buffer_pos.has(p_instance), -1);
	int32_t pos = _global_shader_uniform_allocate(ShaderLanguage::MAX_INSTANCE_UNIFORM_INDICES);
	global_shader_uniforms.instance_buffer_pos[p_instance] = pos; //save anyway
	ERR_FAIL_COND_V_MSG(pos < 0, -1, "Too many instances using shader instance variables. Increase buffer size in Project Settings.");
	global_shader_uniforms.buffer_usage[pos].elements = ShaderLanguage::MAX_INSTANCE_UNIFORM_INDICES;
	return pos;
}

void MaterialStorage::global_shader_parameters_instance_free(RID p_instance) {
	ERR_FAIL_COND(!global_shader_uniforms.instance_buffer_pos.has(p_instance));
	int32_t pos = global_shader_uniforms.instance_buffer_pos[p_instance];
	if (pos >= 0) {
		global_shader_uniforms.buffer_usage[pos].elements = 0;
	}
	global_shader_uniforms.instance_buffer_pos.erase(p_instance);
}

void MaterialStorage::global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count) {
	if (!global_shader_uniforms.instance_buffer_pos.has(p_instance)) {
		return; //just not allocated, ignore
	}
	int32_t pos = global_shader_uniforms.instance_buffer_pos[p_instance];

	if (pos < 0) {
		return; //again, not allocated, ignore
	}
	ERR_FAIL_INDEX(p_index, ShaderLanguage::MAX_INSTANCE_UNIFORM_INDICES);

	Variant::Type value_type = p_value.get_type();
	ERR_FAIL_COND_MSG(p_value.get_type() > Variant::COLOR, "Unsupported variant type for instance parameter: " + Variant::get_type_name(value_type)); //anything greater not supported

	const ShaderLanguage::DataType datatype_from_value[Variant::COLOR + 1] = {
		ShaderLanguage::TYPE_MAX, //nil
		ShaderLanguage::TYPE_BOOL, //bool
		ShaderLanguage::TYPE_INT, //int
		ShaderLanguage::TYPE_FLOAT, //float
		ShaderLanguage::TYPE_MAX, //string
		ShaderLanguage::TYPE_VEC2, //vec2
		ShaderLanguage::TYPE_IVEC2, //vec2i
		ShaderLanguage::TYPE_VEC4, //rect2
		ShaderLanguage::TYPE_IVEC4, //rect2i
		ShaderLanguage::TYPE_VEC3, // vec3
		ShaderLanguage::TYPE_IVEC3, //vec3i
		ShaderLanguage::TYPE_MAX, //xform2d not supported here
		ShaderLanguage::TYPE_VEC4, //vec4
		ShaderLanguage::TYPE_IVEC4, //vec4i
		ShaderLanguage::TYPE_VEC4, //plane
		ShaderLanguage::TYPE_VEC4, //quat
		ShaderLanguage::TYPE_MAX, //aabb not supported here
		ShaderLanguage::TYPE_MAX, //basis not supported here
		ShaderLanguage::TYPE_MAX, //xform not supported here
		ShaderLanguage::TYPE_MAX, //projection not supported here
		ShaderLanguage::TYPE_VEC4 //color
	};

	ShaderLanguage::DataType datatype = ShaderLanguage::TYPE_MAX;
	if (value_type == Variant::INT && p_flags_count > 0) {
		switch (p_flags_count) {
			case 1:
				datatype = ShaderLanguage::TYPE_BVEC2;
				break;
			case 2:
				datatype = ShaderLanguage::TYPE_BVEC3;
				break;
			case 3:
				datatype = ShaderLanguage::TYPE_BVEC4;
				break;
		}
	} else {
		datatype = datatype_from_value[value_type];
	}
	ERR_FAIL_COND_MSG(datatype == ShaderLanguage::TYPE_MAX, "Unsupported variant type for instance parameter: " + Variant::get_type_name(value_type)); //anything greater not supported

	pos += p_index;

	_fill_std140_variant_ubo_value(datatype, 0, p_value, (uint8_t *)&global_shader_uniforms.buffer_values[pos], true); //instances always use linear color in this renderer
	_global_shader_uniform_mark_buffer_dirty(pos, 1);
}

void MaterialStorage::_update_global_shader_uniforms() {
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	if (global_shader_uniforms.buffer_dirty_region_count > 0) {
		uint32_t total_regions = 1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE);
		if (total_regions / global_shader_uniforms.buffer_dirty_region_count <= 4) {
			// 25% of regions dirty, just update all buffer
			RD::get_singleton()->buffer_update(global_shader_uniforms.buffer, 0, sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size, global_shader_uniforms.buffer_values);
			memset(global_shader_uniforms.buffer_dirty_regions, 0, sizeof(bool) * total_regions);
		} else {
			uint32_t region_byte_size = sizeof(GlobalShaderUniforms::Value) * GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;

			for (uint32_t i = 0; i < total_regions; i++) {
				if (global_shader_uniforms.buffer_dirty_regions[i]) {
					RD::get_singleton()->buffer_update(global_shader_uniforms.buffer, i * region_byte_size, region_byte_size, &global_shader_uniforms.buffer_values[i * GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE]);

					global_shader_uniforms.buffer_dirty_regions[i] = false;
				}
			}
		}

		global_shader_uniforms.buffer_dirty_region_count = 0;
	}

	if (global_shader_uniforms.must_update_buffer_materials) {
		// only happens in the case of a buffer variable added or removed,
		// so not often.
		for (const RID &E : global_shader_uniforms.materials_using_buffer) {
			Material *material = material_storage->get_material(E);
			ERR_CONTINUE(!material); //wtf

			material_storage->_material_queue_update(material, true, false);
		}

		global_shader_uniforms.must_update_buffer_materials = false;
	}

	if (global_shader_uniforms.must_update_texture_materials) {
		// only happens in the case of a buffer variable added or removed,
		// so not often.
		for (const RID &E : global_shader_uniforms.materials_using_texture) {
			Material *material = material_storage->get_material(E);
			ERR_CONTINUE(!material); //wtf

			material_storage->_material_queue_update(material, false, true);
		}

		global_shader_uniforms.must_update_texture_materials = false;
	}
}

/* SHADER API */

RID MaterialStorage::shader_allocate() {
	return shader_owner.allocate_rid();
}

void MaterialStorage::shader_initialize(RID p_rid) {
	Shader shader;
	shader.data = nullptr;
	shader.type = SHADER_TYPE_MAX;

	shader_owner.initialize_rid(p_rid, shader);
}

void MaterialStorage::shader_free(RID p_rid) {
	Shader *shader = shader_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(shader);

	//make material unreference this
	while (shader->owners.size()) {
		material_set_shader((*shader->owners.begin())->self, RID());
	}

	//clear data if exists
	if (shader->data) {
		memdelete(shader->data);
	}
	shader_owner.free(p_rid);
}

void MaterialStorage::shader_set_code(RID p_shader, const String &p_code) {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);

	shader->code = p_code;
	String mode_string = ShaderLanguage::get_shader_type(p_code);

	ShaderType new_type;
	if (mode_string == "canvas_item") {
		new_type = SHADER_TYPE_2D;
	} else if (mode_string == "particles") {
		new_type = SHADER_TYPE_PARTICLES;
	} else if (mode_string == "spatial") {
		new_type = SHADER_TYPE_3D;
	} else if (mode_string == "sky") {
		new_type = SHADER_TYPE_SKY;
	} else if (mode_string == "fog") {
		new_type = SHADER_TYPE_FOG;
	} else {
		new_type = SHADER_TYPE_MAX;
	}

	if (new_type != shader->type) {
		if (shader->data) {
			memdelete(shader->data);
			shader->data = nullptr;
		}

		for (Material *E : shader->owners) {
			Material *material = E;
			material->shader_type = new_type;
			if (material->data) {
				memdelete(material->data);
				material->data = nullptr;
			}
		}

		shader->type = new_type;

		if (new_type < SHADER_TYPE_MAX && shader_data_request_func[new_type]) {
			shader->data = shader_data_request_func[new_type]();
		} else {
			shader->type = SHADER_TYPE_MAX; //invalid
		}

		for (Material *E : shader->owners) {
			Material *material = E;
			if (shader->data) {
				material->data = material_get_data_request_function(new_type)(shader->data);
				material->data->self = material->self;
				material->data->set_next_pass(material->next_pass);
				material->data->set_render_priority(material->priority);
			}
			material->shader_type = new_type;
		}

		if (shader->data) {
			for (const KeyValue<StringName, HashMap<int, RID>> &E : shader->default_texture_parameter) {
				for (const KeyValue<int, RID> &E2 : E.value) {
					shader->data->set_default_texture_parameter(E.key, E2.value, E2.key);
				}
			}
		}
	}

	if (shader->data) {
		shader->data->set_path_hint(shader->path_hint);
		shader->data->set_code(p_code);
	}

	for (Material *E : shader->owners) {
		Material *material = E;
		material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
		_material_queue_update(material, true, true);
	}
}

void MaterialStorage::shader_set_path_hint(RID p_shader, const String &p_path) {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);

	shader->path_hint = p_path;
	if (shader->data) {
		shader->data->set_path_hint(p_path);
	}
}

String MaterialStorage::shader_get_code(RID p_shader) const {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, String());
	return shader->code;
}

void MaterialStorage::get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);
	if (shader->data) {
		return shader->data->get_shader_uniform_list(p_param_list);
	}
}

void MaterialStorage::shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);

	if (p_texture.is_valid() && TextureStorage::get_singleton()->owns_texture(p_texture)) {
		if (!shader->default_texture_parameter.has(p_name)) {
			shader->default_texture_parameter[p_name] = HashMap<int, RID>();
		}
		shader->default_texture_parameter[p_name][p_index] = p_texture;
	} else {
		if (shader->default_texture_parameter.has(p_name) && shader->default_texture_parameter[p_name].has(p_index)) {
			shader->default_texture_parameter[p_name].erase(p_index);

			if (shader->default_texture_parameter[p_name].is_empty()) {
				shader->default_texture_parameter.erase(p_name);
			}
		}
	}
	if (shader->data) {
		shader->data->set_default_texture_parameter(p_name, p_texture, p_index);
	}
	for (Material *E : shader->owners) {
		Material *material = E;
		_material_queue_update(material, false, true);
	}
}

RID MaterialStorage::shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, RID());
	if (shader->default_texture_parameter.has(p_name) && shader->default_texture_parameter[p_name].has(p_index)) {
		return shader->default_texture_parameter[p_name][p_index];
	}

	return RID();
}

Variant MaterialStorage::shader_get_parameter_default(RID p_shader, const StringName &p_param) const {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, Variant());
	if (shader->data) {
		return shader->data->get_default_parameter(p_param);
	}
	return Variant();
}

void MaterialStorage::shader_set_data_request_function(ShaderType p_shader_type, ShaderDataRequestFunction p_function) {
	ERR_FAIL_INDEX(p_shader_type, SHADER_TYPE_MAX);
	shader_data_request_func[p_shader_type] = p_function;
}

RS::ShaderNativeSourceCode MaterialStorage::shader_get_native_source_code(RID p_shader) const {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, RS::ShaderNativeSourceCode());
	if (shader->data) {
		return shader->data->get_native_source_code();
	}
	return RS::ShaderNativeSourceCode();
}

/* MATERIAL API */

void MaterialStorage::_material_uniform_set_erased(void *p_material) {
	RID rid = *(RID *)p_material;
	Material *material = MaterialStorage::get_singleton()->get_material(rid);
	if (material) {
		if (material->data) {
			// Uniform set may be gone because a dependency was erased. This happens
			// if a texture is deleted, so re-create it.
			MaterialStorage::get_singleton()->_material_queue_update(material, false, true);
		}
		material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
	}
}

void MaterialStorage::_material_queue_update(Material *material, bool p_uniform, bool p_texture) {
	MutexLock lock(material_update_list_mutex);
	material->uniform_dirty = material->uniform_dirty || p_uniform;
	material->texture_dirty = material->texture_dirty || p_texture;

	if (material->update_element.in_list()) {
		return;
	}

	material_update_list.add(&material->update_element);
}

void MaterialStorage::_update_queued_materials() {
	MutexLock lock(material_update_list_mutex);
	while (material_update_list.first()) {
		Material *material = material_update_list.first()->self();
		bool uniforms_changed = false;

		if (material->data) {
			uniforms_changed = material->data->update_parameters(material->params, material->uniform_dirty, material->texture_dirty);
		}
		material->texture_dirty = false;
		material->uniform_dirty = false;

		material_update_list.remove(&material->update_element);

		if (uniforms_changed) {
			//some implementations such as 3D renderer cache the material uniform set, so update is required
			material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
		}
	}
}

RID MaterialStorage::material_allocate() {
	return material_owner.allocate_rid();
}

void MaterialStorage::material_initialize(RID p_rid) {
	material_owner.initialize_rid(p_rid);
	Material *material = material_owner.get_or_null(p_rid);
	material->self = p_rid;
}

void MaterialStorage::material_free(RID p_rid) {
	Material *material = material_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(material);

	// Need to clear texture arrays to prevent spin locking of their RID's.
	// This happens when the app is being closed.
	for (KeyValue<StringName, Variant> &E : material->params) {
		if (E.value.get_type() == Variant::ARRAY) {
			Array(E.value).clear();
		}
	}

	material_set_shader(p_rid, RID()); //clean up shader
	material->dependency.deleted_notify(p_rid);

	material_owner.free(p_rid);
}

void MaterialStorage::material_set_shader(RID p_material, RID p_shader) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);

	if (material->data) {
		memdelete(material->data);
		material->data = nullptr;
	}

	if (material->shader) {
		material->shader->owners.erase(material);
		material->shader = nullptr;
		material->shader_type = SHADER_TYPE_MAX;
	}

	if (p_shader.is_null()) {
		material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
		material->shader_id = 0;
		return;
	}

	Shader *shader = get_shader(p_shader);
	ERR_FAIL_NULL(shader);
	material->shader = shader;
	material->shader_type = shader->type;
	material->shader_id = p_shader.get_local_index();
	shader->owners.insert(material);

	if (shader->type == SHADER_TYPE_MAX) {
		return;
	}

	ERR_FAIL_NULL(shader->data);

	material->data = material_data_request_func[shader->type](shader->data);
	material->data->self = p_material;
	material->data->set_next_pass(material->next_pass);
	material->data->set_render_priority(material->priority);
	//updating happens later
	material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
	_material_queue_update(material, true, true);
}

MaterialStorage::ShaderData *MaterialStorage::material_get_shader_data(RID p_material) {
	const MaterialStorage::Material *material = MaterialStorage::get_singleton()->get_material(p_material);
	if (material && material->shader && material->shader->data) {
		return material->shader->data;
	}

	return nullptr;
}

void MaterialStorage::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);

	if (p_value.get_type() == Variant::NIL) {
		material->params.erase(p_param);
	} else {
		ERR_FAIL_COND(p_value.get_type() == Variant::OBJECT); //object not allowed
		material->params[p_param] = p_value;
	}

	if (material->shader && material->shader->data) { //shader is valid
		bool is_texture = material->shader->data->is_parameter_texture(p_param);
		_material_queue_update(material, !is_texture, is_texture);
	} else {
		_material_queue_update(material, true, true);
	}
}

Variant MaterialStorage::material_get_param(RID p_material, const StringName &p_param) const {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL_V(material, Variant());
	if (material->params.has(p_param)) {
		return material->params[p_param];
	} else {
		return Variant();
	}
}

void MaterialStorage::material_set_next_pass(RID p_material, RID p_next_material) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);

	if (material->next_pass == p_next_material) {
		return;
	}

	material->next_pass = p_next_material;
	if (material->data) {
		material->data->set_next_pass(p_next_material);
	}

	material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
}

void MaterialStorage::material_set_render_priority(RID p_material, int priority) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);
	material->priority = priority;
	if (material->data) {
		material->data->set_render_priority(priority);
	}
	material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
}

bool MaterialStorage::material_is_animated(RID p_material) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL_V(material, false);
	if (material->shader && material->shader->data) {
		if (material->shader->data->is_animated()) {
			return true;
		} else if (material->next_pass.is_valid()) {
			return material_is_animated(material->next_pass);
		}
	}
	return false; //by default nothing is animated
}

bool MaterialStorage::material_casts_shadows(RID p_material) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL_V(material, true);
	if (material->shader && material->shader->data) {
		if (material->shader->data->casts_shadows()) {
			return true;
		} else if (material->next_pass.is_valid()) {
			return material_casts_shadows(material->next_pass);
		}
	}
	return true; //by default everything casts shadows
}

RS::CullMode RendererRD::MaterialStorage::material_get_cull_mode(RID p_material) const {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL_V(material, RS::CULL_MODE_DISABLED);
	ERR_FAIL_NULL_V(material->shader, RS::CULL_MODE_DISABLED);
	if (material->shader->type == ShaderType::SHADER_TYPE_3D && material->shader->data) {
		RendererSceneRenderImplementation::SceneShaderForwardClustered::ShaderData *sd_clustered = dynamic_cast<RendererSceneRenderImplementation::SceneShaderForwardClustered::ShaderData *>(material->shader->data);
		if (sd_clustered) {
			return (RS::CullMode)sd_clustered->cull_mode;
		}

		RendererSceneRenderImplementation::SceneShaderForwardMobile::ShaderData *sd_mobile = dynamic_cast<RendererSceneRenderImplementation::SceneShaderForwardMobile::ShaderData *>(material->shader->data);
		if (sd_mobile) {
			return (RS::CullMode)sd_mobile->cull_mode;
		}
	}
	return RS::CULL_MODE_DISABLED;
}

void MaterialStorage::material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);
	if (material->shader && material->shader->data) {
		material->shader->data->get_instance_param_list(r_parameters);

		if (material->next_pass.is_valid()) {
			material_get_instance_shader_parameters(material->next_pass, r_parameters);
		}
	}
}

void MaterialStorage::material_update_dependency(RID p_material, DependencyTracker *p_instance) {
	Material *material = material_owner.get_or_null(p_material);
	ERR_FAIL_NULL(material);
	p_instance->update_dependency(&material->dependency);
	if (material->next_pass.is_valid()) {
		material_update_dependency(material->next_pass, p_instance);
	}
}

MaterialStorage::Samplers MaterialStorage::samplers_rd_allocate(float p_mipmap_bias, RS::ViewportAnisotropicFiltering anisotropic_filtering_level) const {
	Samplers samplers;
	samplers.mipmap_bias = p_mipmap_bias;
	samplers.anisotropic_filtering_level = (int)anisotropic_filtering_level;
	samplers.use_nearest_mipmap_filter = GLOBAL_GET("rendering/textures/default_filters/use_nearest_mipmap_filter");

	RD::SamplerFilter mip_filter = samplers.use_nearest_mipmap_filter ? RD::SAMPLER_FILTER_NEAREST : RD::SAMPLER_FILTER_LINEAR;
	float anisotropy_max = float(1 << samplers.anisotropic_filtering_level);

	for (int i = 1; i < RS::CANVAS_ITEM_TEXTURE_FILTER_MAX; i++) {
		for (int j = 1; j < RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX; j++) {
			RD::SamplerState sampler_state;
			switch (i) {
				case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.max_lod = 0;
				} break;
				case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.min_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.max_lod = 0;
				} break;
				case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.mip_filter = mip_filter;
					sampler_state.lod_bias = samplers.mipmap_bias;
				} break;
				case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.min_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.mip_filter = mip_filter;
					sampler_state.lod_bias = samplers.mipmap_bias;

				} break;
				case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
					sampler_state.mip_filter = mip_filter;
					sampler_state.lod_bias = samplers.mipmap_bias;
					sampler_state.use_anisotropy = true;
					sampler_state.anisotropy_max = anisotropy_max;
				} break;
				case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC: {
					sampler_state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.min_filter = RD::SAMPLER_FILTER_LINEAR;
					sampler_state.mip_filter = mip_filter;
					sampler_state.lod_bias = samplers.mipmap_bias;
					sampler_state.use_anisotropy = true;
					sampler_state.anisotropy_max = anisotropy_max;

				} break;
				default: {
				}
			}
			switch (j) {
				case RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED: {
					sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
					sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
					sampler_state.repeat_w = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;

				} break;
				case RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED: {
					sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_REPEAT;
					sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_REPEAT;
					sampler_state.repeat_w = RD::SAMPLER_REPEAT_MODE_REPEAT;
				} break;
				case RS::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR: {
					sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_MIRRORED_REPEAT;
					sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_MIRRORED_REPEAT;
					sampler_state.repeat_w = RD::SAMPLER_REPEAT_MODE_MIRRORED_REPEAT;
				} break;
				default: {
				}
			}

			samplers.rids[i][j] = RD::get_singleton()->sampler_create(sampler_state);
		}
	}

	return samplers;
}

void MaterialStorage::samplers_rd_free(Samplers &p_samplers) const {
	for (int i = 1; i < RS::CANVAS_ITEM_TEXTURE_FILTER_MAX; i++) {
		for (int j = 1; j < RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX; j++) {
			if (p_samplers.rids[i][j].is_valid()) {
				RD::get_singleton()->free(p_samplers.rids[i][j]);
				p_samplers.rids[i][j] = RID();
			}
		}
	}
}

void MaterialStorage::material_set_data_request_function(ShaderType p_shader_type, MaterialStorage::MaterialDataRequestFunction p_function) {
	ERR_FAIL_INDEX(p_shader_type, SHADER_TYPE_MAX);
	material_data_request_func[p_shader_type] = p_function;
}

MaterialStorage::MaterialDataRequestFunction MaterialStorage::material_get_data_request_function(ShaderType p_shader_type) {
	ERR_FAIL_INDEX_V(p_shader_type, SHADER_TYPE_MAX, nullptr);
	return material_data_request_func[p_shader_type];
}
