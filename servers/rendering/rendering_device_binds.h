/**************************************************************************/
/*  rendering_device_binds.h                                              */
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

#ifndef RENDERING_DEVICE_BINDS_H
#define RENDERING_DEVICE_BINDS_H

#include "servers/rendering/rendering_device.h"

#define RD_SETGET(m_type, m_member)            \
	void set_##m_member(m_type p_##m_member) { \
		base.m_member = p_##m_member;          \
	}                                          \
	m_type get_##m_member() const {            \
		return base.m_member;                  \
	}

#define RD_BIND(m_variant_type, m_class, m_member)                                                          \
	ClassDB::bind_method(D_METHOD("set_" _MKSTR(m_member), "p_" _MKSTR(member)), &m_class::set_##m_member); \
	ClassDB::bind_method(D_METHOD("get_" _MKSTR(m_member)), &m_class::get_##m_member);                      \
	ADD_PROPERTY(PropertyInfo(m_variant_type, #m_member), "set_" _MKSTR(m_member), "get_" _MKSTR(m_member))

#define RD_SETGET_SUB(m_type, m_sub, m_member)           \
	void set_##m_sub##_##m_member(m_type p_##m_member) { \
		base.m_sub.m_member = p_##m_member;              \
	}                                                    \
	m_type get_##m_sub##_##m_member() const {            \
		return base.m_sub.m_member;                      \
	}

#define RD_BIND_SUB(m_variant_type, m_class, m_sub, m_member)                                                                           \
	ClassDB::bind_method(D_METHOD("set_" _MKSTR(m_sub) "_" _MKSTR(m_member), "p_" _MKSTR(member)), &m_class::set_##m_sub##_##m_member); \
	ClassDB::bind_method(D_METHOD("get_" _MKSTR(m_sub) "_" _MKSTR(m_member)), &m_class::get_##m_sub##_##m_member);                      \
	ADD_PROPERTY(PropertyInfo(m_variant_type, _MKSTR(m_sub) "_" _MKSTR(m_member)), "set_" _MKSTR(m_sub) "_" _MKSTR(m_member), "get_" _MKSTR(m_sub) "_" _MKSTR(m_member))

class RDTextureFormat : public RefCounted {
	GDCLASS(RDTextureFormat, RefCounted)

	friend class RenderingDevice;
	friend class RenderSceneBuffersRD;

	RD::TextureFormat base;

public:
	RD_SETGET(RD::DataFormat, format)
	RD_SETGET(uint32_t, width)
	RD_SETGET(uint32_t, height)
	RD_SETGET(uint32_t, depth)
	RD_SETGET(uint32_t, array_layers)
	RD_SETGET(uint32_t, mipmaps)
	RD_SETGET(RD::TextureType, texture_type)
	RD_SETGET(RD::TextureSamples, samples)
	RD_SETGET(BitField<RenderingDevice::TextureUsageBits>, usage_bits)
	RD_SETGET(bool, is_resolve_buffer)
	RD_SETGET(bool, is_discardable)

	void add_shareable_format(RD::DataFormat p_format) { base.shareable_formats.push_back(p_format); }
	void remove_shareable_format(RD::DataFormat p_format) { base.shareable_formats.erase(p_format); }

protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDTextureFormat, format);
		RD_BIND(Variant::INT, RDTextureFormat, width);
		RD_BIND(Variant::INT, RDTextureFormat, height);
		RD_BIND(Variant::INT, RDTextureFormat, depth);
		RD_BIND(Variant::INT, RDTextureFormat, array_layers);
		RD_BIND(Variant::INT, RDTextureFormat, mipmaps);
		RD_BIND(Variant::INT, RDTextureFormat, texture_type);
		RD_BIND(Variant::INT, RDTextureFormat, samples);
		RD_BIND(Variant::INT, RDTextureFormat, usage_bits);
		RD_BIND(Variant::BOOL, RDTextureFormat, is_resolve_buffer);
		RD_BIND(Variant::BOOL, RDTextureFormat, is_discardable);

		ClassDB::bind_method(D_METHOD("add_shareable_format", "format"), &RDTextureFormat::add_shareable_format);
		ClassDB::bind_method(D_METHOD("remove_shareable_format", "format"), &RDTextureFormat::remove_shareable_format);
	}
};

class RDTextureView : public RefCounted {
	GDCLASS(RDTextureView, RefCounted)

	friend class RenderingDevice;
	friend class RenderSceneBuffersRD;

	RD::TextureView base;

public:
	RD_SETGET(RD::DataFormat, format_override)
	RD_SETGET(RD::TextureSwizzle, swizzle_r)
	RD_SETGET(RD::TextureSwizzle, swizzle_g)
	RD_SETGET(RD::TextureSwizzle, swizzle_b)
	RD_SETGET(RD::TextureSwizzle, swizzle_a)
protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDTextureView, format_override);
		RD_BIND(Variant::INT, RDTextureView, swizzle_r);
		RD_BIND(Variant::INT, RDTextureView, swizzle_g);
		RD_BIND(Variant::INT, RDTextureView, swizzle_b);
		RD_BIND(Variant::INT, RDTextureView, swizzle_a);
	}
};

class RDAttachmentFormat : public RefCounted {
	GDCLASS(RDAttachmentFormat, RefCounted)
	friend class RenderingDevice;

	RD::AttachmentFormat base;

public:
	RD_SETGET(RD::DataFormat, format)
	RD_SETGET(RD::TextureSamples, samples)
	RD_SETGET(uint32_t, usage_flags)
protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDAttachmentFormat, format);
		RD_BIND(Variant::INT, RDAttachmentFormat, samples);
		RD_BIND(Variant::INT, RDAttachmentFormat, usage_flags);
	}
};

class RDFramebufferPass : public RefCounted {
	GDCLASS(RDFramebufferPass, RefCounted)
	friend class RenderingDevice;
	friend class FramebufferCacheRD;

	RD::FramebufferPass base;

public:
	RD_SETGET(PackedInt32Array, color_attachments)
	RD_SETGET(PackedInt32Array, input_attachments)
	RD_SETGET(PackedInt32Array, resolve_attachments)
	RD_SETGET(PackedInt32Array, preserve_attachments)
	RD_SETGET(int32_t, depth_attachment)
protected:
	enum {
		ATTACHMENT_UNUSED = -1
	};

	static void _bind_methods() {
		RD_BIND(Variant::PACKED_INT32_ARRAY, RDFramebufferPass, color_attachments);
		RD_BIND(Variant::PACKED_INT32_ARRAY, RDFramebufferPass, input_attachments);
		RD_BIND(Variant::PACKED_INT32_ARRAY, RDFramebufferPass, resolve_attachments);
		RD_BIND(Variant::PACKED_INT32_ARRAY, RDFramebufferPass, preserve_attachments);
		RD_BIND(Variant::INT, RDFramebufferPass, depth_attachment);

		BIND_CONSTANT(ATTACHMENT_UNUSED);
	}
};

class RDSamplerState : public RefCounted {
	GDCLASS(RDSamplerState, RefCounted)
	friend class RenderingDevice;

	RD::SamplerState base;

public:
	RD_SETGET(RD::SamplerFilter, mag_filter)
	RD_SETGET(RD::SamplerFilter, min_filter)
	RD_SETGET(RD::SamplerFilter, mip_filter)
	RD_SETGET(RD::SamplerRepeatMode, repeat_u)
	RD_SETGET(RD::SamplerRepeatMode, repeat_v)
	RD_SETGET(RD::SamplerRepeatMode, repeat_w)
	RD_SETGET(float, lod_bias)
	RD_SETGET(bool, use_anisotropy)
	RD_SETGET(float, anisotropy_max)
	RD_SETGET(bool, enable_compare)
	RD_SETGET(RD::CompareOperator, compare_op)
	RD_SETGET(float, min_lod)
	RD_SETGET(float, max_lod)
	RD_SETGET(RD::SamplerBorderColor, border_color)
	RD_SETGET(bool, unnormalized_uvw)

protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDSamplerState, mag_filter);
		RD_BIND(Variant::INT, RDSamplerState, min_filter);
		RD_BIND(Variant::INT, RDSamplerState, mip_filter);
		RD_BIND(Variant::INT, RDSamplerState, repeat_u);
		RD_BIND(Variant::INT, RDSamplerState, repeat_v);
		RD_BIND(Variant::INT, RDSamplerState, repeat_w);
		RD_BIND(Variant::FLOAT, RDSamplerState, lod_bias);
		RD_BIND(Variant::BOOL, RDSamplerState, use_anisotropy);
		RD_BIND(Variant::FLOAT, RDSamplerState, anisotropy_max);
		RD_BIND(Variant::BOOL, RDSamplerState, enable_compare);
		RD_BIND(Variant::INT, RDSamplerState, compare_op);
		RD_BIND(Variant::FLOAT, RDSamplerState, min_lod);
		RD_BIND(Variant::FLOAT, RDSamplerState, max_lod);
		RD_BIND(Variant::INT, RDSamplerState, border_color);
		RD_BIND(Variant::BOOL, RDSamplerState, unnormalized_uvw);
	}
};

class RDVertexAttribute : public RefCounted {
	GDCLASS(RDVertexAttribute, RefCounted)
	friend class RenderingDevice;
	RD::VertexAttribute base;

public:
	RD_SETGET(uint32_t, location)
	RD_SETGET(uint32_t, offset)
	RD_SETGET(RD::DataFormat, format)
	RD_SETGET(uint32_t, stride)
	RD_SETGET(RD::VertexFrequency, frequency)

protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDVertexAttribute, location);
		RD_BIND(Variant::INT, RDVertexAttribute, offset);
		RD_BIND(Variant::INT, RDVertexAttribute, format);
		RD_BIND(Variant::INT, RDVertexAttribute, stride);
		RD_BIND(Variant::INT, RDVertexAttribute, frequency);
	}
};
class RDShaderSource : public RefCounted {
	GDCLASS(RDShaderSource, RefCounted)
	String source[RD::SHADER_STAGE_MAX];
	RD::ShaderLanguage language = RD::SHADER_LANGUAGE_GLSL;

public:
	void set_stage_source(RD::ShaderStage p_stage, const String &p_source) {
		ERR_FAIL_INDEX(p_stage, RD::SHADER_STAGE_MAX);
		source[p_stage] = p_source;
	}

	String get_stage_source(RD::ShaderStage p_stage) const {
		ERR_FAIL_INDEX_V(p_stage, RD::SHADER_STAGE_MAX, String());
		return source[p_stage];
	}

	void set_language(RD::ShaderLanguage p_language) {
		language = p_language;
	}

	RD::ShaderLanguage get_language() const {
		return language;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_stage_source", "stage", "source"), &RDShaderSource::set_stage_source);
		ClassDB::bind_method(D_METHOD("get_stage_source", "stage"), &RDShaderSource::get_stage_source);

		ClassDB::bind_method(D_METHOD("set_language", "language"), &RDShaderSource::set_language);
		ClassDB::bind_method(D_METHOD("get_language"), &RDShaderSource::get_language);

		ADD_GROUP("Source", "source_");
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "source_vertex"), "set_stage_source", "get_stage_source", RD::SHADER_STAGE_VERTEX);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "source_fragment"), "set_stage_source", "get_stage_source", RD::SHADER_STAGE_FRAGMENT);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "source_tesselation_control"), "set_stage_source", "get_stage_source", RD::SHADER_STAGE_TESSELATION_CONTROL);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "source_tesselation_evaluation"), "set_stage_source", "get_stage_source", RD::SHADER_STAGE_TESSELATION_EVALUATION);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "source_compute"), "set_stage_source", "get_stage_source", RD::SHADER_STAGE_COMPUTE);
		ADD_GROUP("Syntax", "source_");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "language", PROPERTY_HINT_RANGE, "GLSL,HLSL"), "set_language", "get_language");
	}
};

class RDShaderSPIRV : public Resource {
	GDCLASS(RDShaderSPIRV, Resource)

	Vector<uint8_t> bytecode[RD::SHADER_STAGE_MAX];
	String compile_error[RD::SHADER_STAGE_MAX];

public:
	void set_stage_bytecode(RD::ShaderStage p_stage, const Vector<uint8_t> &p_bytecode) {
		ERR_FAIL_INDEX(p_stage, RD::SHADER_STAGE_MAX);
		bytecode[p_stage] = p_bytecode;
	}

	Vector<uint8_t> get_stage_bytecode(RD::ShaderStage p_stage) const {
		ERR_FAIL_INDEX_V(p_stage, RD::SHADER_STAGE_MAX, Vector<uint8_t>());
		return bytecode[p_stage];
	}

	Vector<RD::ShaderStageSPIRVData> get_stages() const {
		Vector<RD::ShaderStageSPIRVData> stages;
		for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
			if (bytecode[i].size()) {
				RD::ShaderStageSPIRVData stage;
				stage.shader_stage = RD::ShaderStage(i);
				stage.spirv = bytecode[i];
				stages.push_back(stage);
			}
		}
		return stages;
	}

	void set_stage_compile_error(RD::ShaderStage p_stage, const String &p_compile_error) {
		ERR_FAIL_INDEX(p_stage, RD::SHADER_STAGE_MAX);
		compile_error[p_stage] = p_compile_error;
	}

	String get_stage_compile_error(RD::ShaderStage p_stage) const {
		ERR_FAIL_INDEX_V(p_stage, RD::SHADER_STAGE_MAX, String());
		return compile_error[p_stage];
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_stage_bytecode", "stage", "bytecode"), &RDShaderSPIRV::set_stage_bytecode);
		ClassDB::bind_method(D_METHOD("get_stage_bytecode", "stage"), &RDShaderSPIRV::get_stage_bytecode);

		ClassDB::bind_method(D_METHOD("set_stage_compile_error", "stage", "compile_error"), &RDShaderSPIRV::set_stage_compile_error);
		ClassDB::bind_method(D_METHOD("get_stage_compile_error", "stage"), &RDShaderSPIRV::get_stage_compile_error);

		ADD_GROUP("Bytecode", "bytecode_");
		ADD_PROPERTYI(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode_vertex"), "set_stage_bytecode", "get_stage_bytecode", RD::SHADER_STAGE_VERTEX);
		ADD_PROPERTYI(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode_fragment"), "set_stage_bytecode", "get_stage_bytecode", RD::SHADER_STAGE_FRAGMENT);
		ADD_PROPERTYI(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode_tesselation_control"), "set_stage_bytecode", "get_stage_bytecode", RD::SHADER_STAGE_TESSELATION_CONTROL);
		ADD_PROPERTYI(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode_tesselation_evaluation"), "set_stage_bytecode", "get_stage_bytecode", RD::SHADER_STAGE_TESSELATION_EVALUATION);
		ADD_PROPERTYI(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode_compute"), "set_stage_bytecode", "get_stage_bytecode", RD::SHADER_STAGE_COMPUTE);
		ADD_GROUP("Compile Error", "compile_error_");
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "compile_error_vertex"), "set_stage_compile_error", "get_stage_compile_error", RD::SHADER_STAGE_VERTEX);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "compile_error_fragment"), "set_stage_compile_error", "get_stage_compile_error", RD::SHADER_STAGE_FRAGMENT);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "compile_error_tesselation_control"), "set_stage_compile_error", "get_stage_compile_error", RD::SHADER_STAGE_TESSELATION_CONTROL);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "compile_error_tesselation_evaluation"), "set_stage_compile_error", "get_stage_compile_error", RD::SHADER_STAGE_TESSELATION_EVALUATION);
		ADD_PROPERTYI(PropertyInfo(Variant::STRING, "compile_error_compute"), "set_stage_compile_error", "get_stage_compile_error", RD::SHADER_STAGE_COMPUTE);
	}
};

class RDShaderFile : public Resource {
	GDCLASS(RDShaderFile, Resource)

	HashMap<StringName, Ref<RDShaderSPIRV>> versions;
	String base_error;

public:
	void set_bytecode(const Ref<RDShaderSPIRV> &p_bytecode, const StringName &p_version = StringName()) {
		ERR_FAIL_COND(p_bytecode.is_null());
		versions[p_version] = p_bytecode;
		emit_changed();
	}

	Ref<RDShaderSPIRV> get_spirv(const StringName &p_version = StringName()) const {
		ERR_FAIL_COND_V(!versions.has(p_version), Ref<RDShaderSPIRV>());
		return versions[p_version];
	}

	Vector<RD::ShaderStageSPIRVData> get_spirv_stages(const StringName &p_version = StringName()) const {
		ERR_FAIL_COND_V(!versions.has(p_version), Vector<RD::ShaderStageSPIRVData>());
		return versions[p_version]->get_stages();
	}

	TypedArray<StringName> get_version_list() const {
		Vector<StringName> vnames;
		for (const KeyValue<StringName, Ref<RDShaderSPIRV>> &E : versions) {
			vnames.push_back(E.key);
		}
		vnames.sort_custom<StringName::AlphCompare>();
		TypedArray<StringName> ret;
		ret.resize(vnames.size());
		for (int i = 0; i < vnames.size(); i++) {
			ret[i] = vnames[i];
		}
		return ret;
	}

	void set_base_error(const String &p_error) {
		base_error = p_error;
		emit_changed();
	}

	String get_base_error() const {
		return base_error;
	}

	void print_errors(const String &p_file) {
		if (!base_error.is_empty()) {
			ERR_PRINT("Error parsing shader '" + p_file + "':\n\n" + base_error);
		} else {
			for (KeyValue<StringName, Ref<RDShaderSPIRV>> &E : versions) {
				for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
					String error = E.value->get_stage_compile_error(RD::ShaderStage(i));
					if (!error.is_empty()) {
						static const char *stage_str[RD::SHADER_STAGE_MAX] = {
							"vertex",
							"fragment",
							"tesselation_control",
							"tesselation_evaluation",
							"compute"
						};

						print_error("Error parsing shader '" + p_file + "', version '" + String(E.key) + "', stage '" + stage_str[i] + "':\n\n" + error);
					}
				}
			}
		}
	}

	typedef String (*OpenIncludeFunction)(const String &, void *userdata);
	Error parse_versions_from_text(const String &p_text, const String p_defines = String(), OpenIncludeFunction p_include_func = nullptr, void *p_include_func_userdata = nullptr);

protected:
	Dictionary _get_versions() const {
		TypedArray<StringName> vnames = get_version_list();
		Dictionary ret;
		for (int i = 0; i < vnames.size(); i++) {
			ret[vnames[i]] = versions[vnames[i]];
		}
		return ret;
	}
	void _set_versions(const Dictionary &p_versions) {
		versions.clear();
		List<Variant> keys;
		p_versions.get_key_list(&keys);
		for (const Variant &E : keys) {
			StringName vname = E;
			Ref<RDShaderSPIRV> bc = p_versions[E];
			ERR_CONTINUE(bc.is_null());
			versions[vname] = bc;
		}

		emit_changed();
	}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_bytecode", "bytecode", "version"), &RDShaderFile::set_bytecode, DEFVAL(StringName()));
		ClassDB::bind_method(D_METHOD("get_spirv", "version"), &RDShaderFile::get_spirv, DEFVAL(StringName()));
		ClassDB::bind_method(D_METHOD("get_version_list"), &RDShaderFile::get_version_list);

		ClassDB::bind_method(D_METHOD("set_base_error", "error"), &RDShaderFile::set_base_error);
		ClassDB::bind_method(D_METHOD("get_base_error"), &RDShaderFile::get_base_error);

		ClassDB::bind_method(D_METHOD("_set_versions", "versions"), &RDShaderFile::_set_versions);
		ClassDB::bind_method(D_METHOD("_get_versions"), &RDShaderFile::_get_versions);

		ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_versions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_versions", "_get_versions");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_error"), "set_base_error", "get_base_error");
	}
};

class RDUniform : public RefCounted {
	GDCLASS(RDUniform, RefCounted)
	friend class RenderingDevice;
	friend class UniformSetCacheRD;
	RD::Uniform base;

public:
	RD_SETGET(RD::UniformType, uniform_type)
	RD_SETGET(int32_t, binding)

	void add_id(const RID &p_id) { base.append_id(p_id); }
	void clear_ids() { base.clear_ids(); }
	TypedArray<RID> get_ids() const {
		TypedArray<RID> ids;
		for (uint32_t i = 0; i < base.get_id_count(); i++) {
			ids.push_back(base.get_id(i));
		}
		return ids;
	}

protected:
	void _set_ids(const TypedArray<RID> &p_ids) {
		base.clear_ids();
		for (int i = 0; i < p_ids.size(); i++) {
			RID id = p_ids[i];
			ERR_FAIL_COND(id.is_null());
			base.append_id(id);
		}
	}
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDUniform, uniform_type);
		RD_BIND(Variant::INT, RDUniform, binding);
		ClassDB::bind_method(D_METHOD("add_id", "id"), &RDUniform::add_id);
		ClassDB::bind_method(D_METHOD("clear_ids"), &RDUniform::clear_ids);
		ClassDB::bind_method(D_METHOD("_set_ids", "ids"), &RDUniform::_set_ids);
		ClassDB::bind_method(D_METHOD("get_ids"), &RDUniform::get_ids);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "_ids", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "_set_ids", "get_ids");
	}
};

class RDPipelineSpecializationConstant : public RefCounted {
	GDCLASS(RDPipelineSpecializationConstant, RefCounted)
	friend class RenderingDevice;

	Variant value = false;
	uint32_t constant_id = 0;

public:
	void set_value(const Variant &p_value) {
		ERR_FAIL_COND(p_value.get_type() != Variant::BOOL && p_value.get_type() != Variant::INT && p_value.get_type() != Variant::FLOAT);
		value = p_value;
	}
	Variant get_value() const { return value; }

	void set_constant_id(uint32_t p_id) {
		constant_id = p_id;
	}
	uint32_t get_constant_id() const {
		return constant_id;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_value", "value"), &RDPipelineSpecializationConstant::set_value);
		ClassDB::bind_method(D_METHOD("get_value"), &RDPipelineSpecializationConstant::get_value);

		ClassDB::bind_method(D_METHOD("set_constant_id", "constant_id"), &RDPipelineSpecializationConstant::set_constant_id);
		ClassDB::bind_method(D_METHOD("get_constant_id"), &RDPipelineSpecializationConstant::get_constant_id);

		ADD_PROPERTY(PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT), "set_value", "get_value");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "constant_id", PROPERTY_HINT_RANGE, "0,65535,0"), "set_constant_id", "get_constant_id");
	}
};

class RDPipelineRasterizationState : public RefCounted {
	GDCLASS(RDPipelineRasterizationState, RefCounted)
	friend class RenderingDevice;

	RD::PipelineRasterizationState base;

public:
	RD_SETGET(bool, enable_depth_clamp)
	RD_SETGET(bool, discard_primitives)
	RD_SETGET(bool, wireframe)
	RD_SETGET(RD::PolygonCullMode, cull_mode)
	RD_SETGET(RD::PolygonFrontFace, front_face)
	RD_SETGET(bool, depth_bias_enabled)
	RD_SETGET(float, depth_bias_constant_factor)
	RD_SETGET(float, depth_bias_clamp)
	RD_SETGET(float, depth_bias_slope_factor)
	RD_SETGET(float, line_width)
	RD_SETGET(uint32_t, patch_control_points)

protected:
	static void _bind_methods() {
		RD_BIND(Variant::BOOL, RDPipelineRasterizationState, enable_depth_clamp);
		RD_BIND(Variant::BOOL, RDPipelineRasterizationState, discard_primitives);
		RD_BIND(Variant::BOOL, RDPipelineRasterizationState, wireframe);
		RD_BIND(Variant::INT, RDPipelineRasterizationState, cull_mode);
		RD_BIND(Variant::INT, RDPipelineRasterizationState, front_face);
		RD_BIND(Variant::BOOL, RDPipelineRasterizationState, depth_bias_enabled);
		RD_BIND(Variant::FLOAT, RDPipelineRasterizationState, depth_bias_constant_factor);
		RD_BIND(Variant::FLOAT, RDPipelineRasterizationState, depth_bias_clamp);
		RD_BIND(Variant::FLOAT, RDPipelineRasterizationState, depth_bias_slope_factor);
		RD_BIND(Variant::FLOAT, RDPipelineRasterizationState, line_width);
		RD_BIND(Variant::INT, RDPipelineRasterizationState, patch_control_points);
	}
};

class RDPipelineMultisampleState : public RefCounted {
	GDCLASS(RDPipelineMultisampleState, RefCounted)
	friend class RenderingDevice;

	RD::PipelineMultisampleState base;
	TypedArray<int64_t> sample_masks;

public:
	RD_SETGET(RD::TextureSamples, sample_count)
	RD_SETGET(bool, enable_sample_shading)
	RD_SETGET(float, min_sample_shading)
	RD_SETGET(bool, enable_alpha_to_coverage)
	RD_SETGET(bool, enable_alpha_to_one)

	void set_sample_masks(const TypedArray<int64_t> &p_masks) { sample_masks = p_masks; }
	TypedArray<int64_t> get_sample_masks() const { return sample_masks; }

protected:
	static void _bind_methods() {
		RD_BIND(Variant::INT, RDPipelineMultisampleState, sample_count);
		RD_BIND(Variant::BOOL, RDPipelineMultisampleState, enable_sample_shading);
		RD_BIND(Variant::FLOAT, RDPipelineMultisampleState, min_sample_shading);
		RD_BIND(Variant::BOOL, RDPipelineMultisampleState, enable_alpha_to_coverage);
		RD_BIND(Variant::BOOL, RDPipelineMultisampleState, enable_alpha_to_one);

		ClassDB::bind_method(D_METHOD("set_sample_masks", "masks"), &RDPipelineMultisampleState::set_sample_masks);
		ClassDB::bind_method(D_METHOD("get_sample_masks"), &RDPipelineMultisampleState::get_sample_masks);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "sample_masks", PROPERTY_HINT_ARRAY_TYPE, "int"), "set_sample_masks", "get_sample_masks");
	}
};

class RDPipelineDepthStencilState : public RefCounted {
	GDCLASS(RDPipelineDepthStencilState, RefCounted)
	friend class RenderingDevice;

	RD::PipelineDepthStencilState base;

public:
	RD_SETGET(bool, enable_depth_test)
	RD_SETGET(bool, enable_depth_write)
	RD_SETGET(RD::CompareOperator, depth_compare_operator)
	RD_SETGET(bool, enable_depth_range)
	RD_SETGET(float, depth_range_min)
	RD_SETGET(float, depth_range_max)
	RD_SETGET(bool, enable_stencil)

	RD_SETGET_SUB(RD::StencilOperation, front_op, fail)
	RD_SETGET_SUB(RD::StencilOperation, front_op, pass)
	RD_SETGET_SUB(RD::StencilOperation, front_op, depth_fail)
	RD_SETGET_SUB(RD::CompareOperator, front_op, compare)
	RD_SETGET_SUB(uint32_t, front_op, compare_mask)
	RD_SETGET_SUB(uint32_t, front_op, write_mask)
	RD_SETGET_SUB(uint32_t, front_op, reference)

	RD_SETGET_SUB(RD::StencilOperation, back_op, fail)
	RD_SETGET_SUB(RD::StencilOperation, back_op, pass)
	RD_SETGET_SUB(RD::StencilOperation, back_op, depth_fail)
	RD_SETGET_SUB(RD::CompareOperator, back_op, compare)
	RD_SETGET_SUB(uint32_t, back_op, compare_mask)
	RD_SETGET_SUB(uint32_t, back_op, write_mask)
	RD_SETGET_SUB(uint32_t, back_op, reference)

protected:
	static void _bind_methods() {
		RD_BIND(Variant::BOOL, RDPipelineDepthStencilState, enable_depth_test);
		RD_BIND(Variant::BOOL, RDPipelineDepthStencilState, enable_depth_write);
		RD_BIND(Variant::INT, RDPipelineDepthStencilState, depth_compare_operator);
		RD_BIND(Variant::BOOL, RDPipelineDepthStencilState, enable_depth_range);
		RD_BIND(Variant::FLOAT, RDPipelineDepthStencilState, depth_range_min);
		RD_BIND(Variant::FLOAT, RDPipelineDepthStencilState, depth_range_max);
		RD_BIND(Variant::BOOL, RDPipelineDepthStencilState, enable_stencil);

		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, fail);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, pass);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, depth_fail);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, compare);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, compare_mask);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, write_mask);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, front_op, reference);

		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, fail);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, pass);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, depth_fail);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, compare);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, compare_mask);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, write_mask);
		RD_BIND_SUB(Variant::INT, RDPipelineDepthStencilState, back_op, reference);
	}
};

class RDPipelineColorBlendStateAttachment : public RefCounted {
	GDCLASS(RDPipelineColorBlendStateAttachment, RefCounted)
	friend class RenderingDevice;
	RD::PipelineColorBlendState::Attachment base;

public:
	RD_SETGET(bool, enable_blend)
	RD_SETGET(RD::BlendFactor, src_color_blend_factor)
	RD_SETGET(RD::BlendFactor, dst_color_blend_factor)
	RD_SETGET(RD::BlendOperation, color_blend_op)
	RD_SETGET(RD::BlendFactor, src_alpha_blend_factor)
	RD_SETGET(RD::BlendFactor, dst_alpha_blend_factor)
	RD_SETGET(RD::BlendOperation, alpha_blend_op)
	RD_SETGET(bool, write_r)
	RD_SETGET(bool, write_g)
	RD_SETGET(bool, write_b)
	RD_SETGET(bool, write_a)

	void set_as_mix() {
		base = RD::PipelineColorBlendState::Attachment();
		base.enable_blend = true;
		base.src_color_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
		base.dst_color_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		base.src_alpha_blend_factor = RD::BLEND_FACTOR_SRC_ALPHA;
		base.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_as_mix"), &RDPipelineColorBlendStateAttachment::set_as_mix);

		RD_BIND(Variant::BOOL, RDPipelineColorBlendStateAttachment, enable_blend);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, src_color_blend_factor);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, dst_color_blend_factor);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, color_blend_op);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, src_alpha_blend_factor);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, dst_alpha_blend_factor);
		RD_BIND(Variant::INT, RDPipelineColorBlendStateAttachment, alpha_blend_op);
		RD_BIND(Variant::BOOL, RDPipelineColorBlendStateAttachment, write_r);
		RD_BIND(Variant::BOOL, RDPipelineColorBlendStateAttachment, write_g);
		RD_BIND(Variant::BOOL, RDPipelineColorBlendStateAttachment, write_b);
		RD_BIND(Variant::BOOL, RDPipelineColorBlendStateAttachment, write_a);
	}
};

class RDPipelineColorBlendState : public RefCounted {
	GDCLASS(RDPipelineColorBlendState, RefCounted)
	friend class RenderingDevice;
	RD::PipelineColorBlendState base;

	TypedArray<RDPipelineColorBlendStateAttachment> attachments;

public:
	RD_SETGET(bool, enable_logic_op)
	RD_SETGET(RD::LogicOperation, logic_op)
	RD_SETGET(Color, blend_constant)

	void set_attachments(const TypedArray<RDPipelineColorBlendStateAttachment> &p_attachments) {
		attachments = p_attachments;
	}

	TypedArray<RDPipelineColorBlendStateAttachment> get_attachments() const {
		return attachments;
	}

protected:
	static void _bind_methods() {
		RD_BIND(Variant::BOOL, RDPipelineColorBlendState, enable_logic_op);
		RD_BIND(Variant::INT, RDPipelineColorBlendState, logic_op);
		RD_BIND(Variant::COLOR, RDPipelineColorBlendState, blend_constant);

		ClassDB::bind_method(D_METHOD("set_attachments", "attachments"), &RDPipelineColorBlendState::set_attachments);
		ClassDB::bind_method(D_METHOD("get_attachments"), &RDPipelineColorBlendState::get_attachments);
		ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "attachments", PROPERTY_HINT_ARRAY_TYPE, "RDPipelineColorBlendStateAttachment"), "set_attachments", "get_attachments");
	}
};

#endif // RENDERING_DEVICE_BINDS_H
