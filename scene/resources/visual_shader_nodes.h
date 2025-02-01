/**************************************************************************/
/*  visual_shader_nodes.h                                                 */
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

#include "scene/resources/curve_texture.h"
#include "scene/resources/visual_shader.h"

class Cubemap;
class Texture2DArray;

///////////////////////////////////////
/// Vector Base Node
///////////////////////////////////////

class VisualShaderNodeVectorBase : public VisualShaderNode {
	GDCLASS(VisualShaderNodeVectorBase, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_VECTOR_3D;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override = 0;

	virtual int get_input_port_count() const override = 0;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override = 0;

	virtual int get_output_port_count() const override = 0;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override = 0;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override = 0;

	virtual void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_VECTOR; }

	VisualShaderNodeVectorBase();
};

VARIANT_ENUM_CAST(VisualShaderNodeVectorBase::OpType)

///////////////////////////////////////
/// CONSTANTS
///////////////////////////////////////

class VisualShaderNodeConstant : public VisualShaderNode {
	GDCLASS(VisualShaderNodeConstant, VisualShaderNode);

public:
	virtual String get_caption() const override = 0;

	virtual int get_input_port_count() const override = 0;
	virtual PortType get_input_port_type(int p_port) const override = 0;
	virtual String get_input_port_name(int p_port) const override = 0;

	virtual int get_output_port_count() const override = 0;
	virtual PortType get_output_port_type(int p_port) const override = 0;
	virtual String get_output_port_name(int p_port) const override = 0;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override = 0;

	virtual Category get_category() const override { return CATEGORY_INPUT; }

	VisualShaderNodeConstant();
};

class VisualShaderNodeFloatConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeFloatConstant, VisualShaderNodeConstant);
	float constant = 0.0f;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(float p_constant);
	float get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeFloatConstant();
};

///////////////////////////////////////

class VisualShaderNodeIntConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeIntConstant, VisualShaderNodeConstant);
	int constant = 0;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(int p_constant);
	int get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeIntConstant();
};

///////////////////////////////////////

class VisualShaderNodeUIntConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeUIntConstant, VisualShaderNodeConstant);
	int constant = 0;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(int p_constant);
	int get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeUIntConstant();
};

///////////////////////////////////////

class VisualShaderNodeBooleanConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeBooleanConstant, VisualShaderNodeConstant);
	bool constant = false;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(bool p_constant);
	bool get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeBooleanConstant();
};

///////////////////////////////////////

class VisualShaderNodeColorConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeColorConstant, VisualShaderNodeConstant);
	Color constant = Color(1, 1, 1, 1);

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(const Color &p_constant);
	Color get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeColorConstant();
};

///////////////////////////////////////

class VisualShaderNodeVec2Constant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeVec2Constant, VisualShaderNodeConstant);
	Vector2 constant;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(const Vector2 &p_constant);
	Vector2 get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec2Constant();
};

///////////////////////////////////////

class VisualShaderNodeVec3Constant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeVec3Constant, VisualShaderNodeConstant);
	Vector3 constant;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(const Vector3 &p_constant);
	Vector3 get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec3Constant();
};

///////////////////////////////////////

class VisualShaderNodeVec4Constant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeVec4Constant, VisualShaderNodeConstant);
	Quaternion constant;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(const Quaternion &p_constant);
	Quaternion get_constant() const;

	void _set_constant_v4(const Vector4 &p_constant);
	Vector4 _get_constant_v4() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec4Constant();
};

///////////////////////////////////////

class VisualShaderNodeTransformConstant : public VisualShaderNodeConstant {
	GDCLASS(VisualShaderNodeTransformConstant, VisualShaderNodeConstant);
	Transform3D constant;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_constant(const Transform3D &p_constant);
	Transform3D get_constant() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeTransformConstant();
};

///////////////////////////////////////
/// TEXTURES
///////////////////////////////////////

class VisualShaderNodeTexture : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTexture, VisualShaderNode);
	Ref<Texture2D> texture;

public:
	enum Source {
		SOURCE_TEXTURE,
		SOURCE_SCREEN,
		SOURCE_2D_TEXTURE,
		SOURCE_2D_NORMAL,
		SOURCE_DEPTH,
		SOURCE_PORT,
		SOURCE_3D_NORMAL,
		SOURCE_ROUGHNESS,
		SOURCE_MAX,
	};

	enum TextureType {
		TYPE_DATA,
		TYPE_COLOR,
		TYPE_NORMAL_MAP,
		TYPE_MAX,
	};

private:
	Source source = SOURCE_TEXTURE;
	TextureType texture_type = TYPE_DATA;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_source(Source p_source);
	Source get_source() const;

	void set_texture(Ref<Texture2D> p_texture);
	Ref<Texture2D> get_texture() const;

	void set_texture_type(TextureType p_texture_type);
	TextureType get_texture_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeTexture();
};

VARIANT_ENUM_CAST(VisualShaderNodeTexture::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeTexture::Source)

///////////////////////////////////////

class VisualShaderNodeCurveTexture : public VisualShaderNodeResizableBase {
	GDCLASS(VisualShaderNodeCurveTexture, VisualShaderNodeResizableBase);
	Ref<CurveTexture> texture;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_texture(Ref<CurveTexture> p_texture);
	Ref<CurveTexture> get_texture() const;

	virtual Vector<StringName> get_editable_properties() const override;
	virtual bool is_use_prop_slots() const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeCurveTexture();
};

///////////////////////////////////////

class VisualShaderNodeCurveXYZTexture : public VisualShaderNodeResizableBase {
	GDCLASS(VisualShaderNodeCurveXYZTexture, VisualShaderNodeResizableBase);
	Ref<CurveXYZTexture> texture;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_texture(Ref<CurveXYZTexture> p_texture);
	Ref<CurveXYZTexture> get_texture() const;

	virtual Vector<StringName> get_editable_properties() const override;
	virtual bool is_use_prop_slots() const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeCurveXYZTexture();
};

///////////////////////////////////////

class VisualShaderNodeSample3D : public VisualShaderNode {
	GDCLASS(VisualShaderNodeSample3D, VisualShaderNode);

public:
	enum Source {
		SOURCE_TEXTURE,
		SOURCE_PORT,
		SOURCE_MAX,
	};

protected:
	Source source = SOURCE_TEXTURE;

	static void _bind_methods();

public:
	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_source(Source p_source);
	Source get_source() const;

	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeSample3D();
};

VARIANT_ENUM_CAST(VisualShaderNodeSample3D::Source)

class VisualShaderNodeTexture2DArray : public VisualShaderNodeSample3D {
	GDCLASS(VisualShaderNodeTexture2DArray, VisualShaderNodeSample3D);
	Ref<TextureLayered> texture_array;

protected:
#ifndef DISABLE_DEPRECATED
	void _set_texture_array_bind_compat_95126(Ref<Texture2DArray> p_texture_array);
	Ref<Texture2DArray> _get_texture_array_bind_compat_95126() const;
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual String get_input_port_name(int p_port) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	void set_texture_array(Ref<TextureLayered> p_texture_array);
	Ref<TextureLayered> get_texture_array() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeTexture2DArray();
};

class VisualShaderNodeTexture3D : public VisualShaderNodeSample3D {
	GDCLASS(VisualShaderNodeTexture3D, VisualShaderNodeSample3D);
	Ref<Texture3D> texture;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual String get_input_port_name(int p_port) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	void set_texture(Ref<Texture3D> p_texture);
	Ref<Texture3D> get_texture() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeTexture3D();
};

class VisualShaderNodeCubemap : public VisualShaderNode {
	GDCLASS(VisualShaderNodeCubemap, VisualShaderNode);
	Ref<TextureLayered> cube_map;

public:
	enum Source {
		SOURCE_TEXTURE,
		SOURCE_PORT,
		SOURCE_MAX,
	};

	enum TextureType {
		TYPE_DATA,
		TYPE_COLOR,
		TYPE_NORMAL_MAP,
		TYPE_MAX,
	};

private:
	Source source = SOURCE_TEXTURE;
	TextureType texture_type = TYPE_DATA;

protected:
#ifndef DISABLE_DEPRECATED
	void _set_cube_map_bind_compat_95126(Ref<Cubemap> p_cube_map);
	Ref<Cubemap> _get_cube_map_bind_compat_95126() const;
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_source(Source p_source);
	Source get_source() const;

	void set_cube_map(Ref<TextureLayered> p_cube_map);
	Ref<TextureLayered> get_cube_map() const;

	void set_texture_type(TextureType p_texture_type);
	TextureType get_texture_type() const;

	virtual Vector<StringName> get_editable_properties() const override;
	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeCubemap();
};

VARIANT_ENUM_CAST(VisualShaderNodeCubemap::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeCubemap::Source)

///////////////////////////////////////

class VisualShaderNodeLinearSceneDepth : public VisualShaderNode {
	GDCLASS(VisualShaderNodeLinearSceneDepth, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeLinearSceneDepth();
};

class VisualShaderNodeWorldPositionFromDepth : public VisualShaderNode {
	GDCLASS(VisualShaderNodeWorldPositionFromDepth, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeWorldPositionFromDepth();
};

class VisualShaderNodeScreenNormalWorldSpace : public VisualShaderNode {
	GDCLASS(VisualShaderNodeScreenNormalWorldSpace, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeScreenNormalWorldSpace();
};

///////////////////////////////////////
/// OPS
///////////////////////////////////////

class VisualShaderNodeFloatOp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeFloatOp, VisualShaderNode);

public:
	enum Operator {
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_POW,
		OP_MAX,
		OP_MIN,
		OP_ATAN2,
		OP_STEP,
		OP_ENUM_SIZE,
	};

protected:
	Operator op = OP_ADD;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeFloatOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeFloatOp::Operator)

class VisualShaderNodeIntOp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeIntOp, VisualShaderNode);

public:
	enum Operator {
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_MAX,
		OP_MIN,
		OP_BITWISE_AND,
		OP_BITWISE_OR,
		OP_BITWISE_XOR,
		OP_BITWISE_LEFT_SHIFT,
		OP_BITWISE_RIGHT_SHIFT,
		OP_ENUM_SIZE,
	};

protected:
	Operator op = OP_ADD;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeIntOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeIntOp::Operator)

class VisualShaderNodeUIntOp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeUIntOp, VisualShaderNode);

public:
	enum Operator {
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_MAX,
		OP_MIN,
		OP_BITWISE_AND,
		OP_BITWISE_OR,
		OP_BITWISE_XOR,
		OP_BITWISE_LEFT_SHIFT,
		OP_BITWISE_RIGHT_SHIFT,
		OP_ENUM_SIZE,
	};

protected:
	Operator op = OP_ADD;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeUIntOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeUIntOp::Operator)

class VisualShaderNodeVectorOp : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorOp, VisualShaderNodeVectorBase);

public:
	enum Operator {
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_POW,
		OP_MAX,
		OP_MIN,
		OP_CROSS,
		OP_ATAN2,
		OP_REFLECT,
		OP_STEP,
		OP_ENUM_SIZE,
	};

protected:
	Operator op = OP_ADD;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual void set_op_type(OpType p_op_type) override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;
	String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	VisualShaderNodeVectorOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeVectorOp::Operator)

///////////////////////////////////////

class VisualShaderNodeColorOp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeColorOp, VisualShaderNode);

public:
	enum Operator {
		OP_SCREEN,
		OP_DIFFERENCE,
		OP_DARKEN,
		OP_LIGHTEN,
		OP_OVERLAY,
		OP_DODGE,
		OP_BURN,
		OP_SOFT_LIGHT,
		OP_HARD_LIGHT,
		OP_MAX,
	};

protected:
	Operator op = OP_SCREEN;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_COLOR; }

	VisualShaderNodeColorOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeColorOp::Operator)

////////////////////////////////
/// TRANSFORM-TRANSFORM OPERATOR
////////////////////////////////

class VisualShaderNodeTransformOp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTransformOp, VisualShaderNode);

public:
	enum Operator {
		OP_AxB,
		OP_BxA,
		OP_AxB_COMP,
		OP_BxA_COMP,
		OP_ADD,
		OP_A_MINUS_B,
		OP_B_MINUS_A,
		OP_A_DIV_B,
		OP_B_DIV_A,
		OP_MAX,
	};

protected:
	Operator op = OP_AxB;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeTransformOp();
};

VARIANT_ENUM_CAST(VisualShaderNodeTransformOp::Operator)

///////////////////////////////////////
/// TRANSFORM-VECTOR MULTIPLICATION
///////////////////////////////////////

class VisualShaderNodeTransformVecMult : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTransformVecMult, VisualShaderNode);

public:
	enum Operator {
		OP_AxB,
		OP_BxA,
		OP_3x3_AxB,
		OP_3x3_BxA,
		OP_MAX,
	};

protected:
	Operator op = OP_AxB;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_operator(Operator p_op);
	Operator get_operator() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeTransformVecMult();
};

VARIANT_ENUM_CAST(VisualShaderNodeTransformVecMult::Operator)

///////////////////////////////////////
/// FLOAT FUNC
///////////////////////////////////////

class VisualShaderNodeFloatFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeFloatFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_SIN,
		FUNC_COS,
		FUNC_TAN,
		FUNC_ASIN,
		FUNC_ACOS,
		FUNC_ATAN,
		FUNC_SINH,
		FUNC_COSH,
		FUNC_TANH,
		FUNC_LOG,
		FUNC_EXP,
		FUNC_SQRT,
		FUNC_ABS,
		FUNC_SIGN,
		FUNC_FLOOR,
		FUNC_ROUND,
		FUNC_CEIL,
		FUNC_FRACT,
		FUNC_SATURATE,
		FUNC_NEGATE,
		FUNC_ACOSH,
		FUNC_ASINH,
		FUNC_ATANH,
		FUNC_DEGREES,
		FUNC_EXP2,
		FUNC_INVERSE_SQRT,
		FUNC_LOG2,
		FUNC_RADIANS,
		FUNC_RECIPROCAL,
		FUNC_ROUNDEVEN,
		FUNC_TRUNC,
		FUNC_ONEMINUS,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_SIGN;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeFloatFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeFloatFunc::Function)

///////////////////////////////////////
/// INT FUNC
///////////////////////////////////////

class VisualShaderNodeIntFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeIntFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_ABS,
		FUNC_NEGATE,
		FUNC_SIGN,
		FUNC_BITWISE_NOT,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_SIGN;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeIntFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeIntFunc::Function)

///////////////////////////////////////
/// UINT FUNC
///////////////////////////////////////

class VisualShaderNodeUIntFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeUIntFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_NEGATE,
		FUNC_BITWISE_NOT,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_NEGATE;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_SCALAR; }

	VisualShaderNodeUIntFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeUIntFunc::Function)

///////////////////////////////////////
/// VECTOR FUNC
///////////////////////////////////////

class VisualShaderNodeVectorFunc : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorFunc, VisualShaderNodeVectorBase);

	void _update_default_input_values();

public:
	enum Function {
		FUNC_NORMALIZE,
		FUNC_SATURATE,
		FUNC_NEGATE,
		FUNC_RECIPROCAL,
		FUNC_ABS,
		FUNC_ACOS,
		FUNC_ACOSH,
		FUNC_ASIN,
		FUNC_ASINH,
		FUNC_ATAN,
		FUNC_ATANH,
		FUNC_CEIL,
		FUNC_COS,
		FUNC_COSH,
		FUNC_DEGREES,
		FUNC_EXP,
		FUNC_EXP2,
		FUNC_FLOOR,
		FUNC_FRACT,
		FUNC_INVERSE_SQRT,
		FUNC_LOG,
		FUNC_LOG2,
		FUNC_RADIANS,
		FUNC_ROUND,
		FUNC_ROUNDEVEN,
		FUNC_SIGN,
		FUNC_SIN,
		FUNC_SINH,
		FUNC_SQRT,
		FUNC_TAN,
		FUNC_TANH,
		FUNC_TRUNC,
		FUNC_ONEMINUS,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_NORMALIZE;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual void set_op_type(OpType p_op_type) override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVectorFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeVectorFunc::Function)

///////////////////////////////////////
/// COLOR FUNC
///////////////////////////////////////

class VisualShaderNodeColorFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeColorFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_GRAYSCALE,
		FUNC_HSV2RGB,
		FUNC_RGB2HSV,
		FUNC_SEPIA,
		FUNC_LINEAR_TO_SRGB,
		FUNC_SRGB_TO_LINEAR,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_GRAYSCALE;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_COLOR; }

	VisualShaderNodeColorFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeColorFunc::Function)

///////////////////////////////////////
/// TRANSFORM FUNC
///////////////////////////////////////

class VisualShaderNodeTransformFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTransformFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_INVERSE,
		FUNC_TRANSPOSE,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_INVERSE;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeTransformFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeTransformFunc::Function)

///////////////////////////////////////
/// UV FUNC
///////////////////////////////////////

class VisualShaderNodeUVFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeUVFunc, VisualShaderNode);

public:
	enum Function {
		FUNC_PANNING,
		FUNC_SCALING,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_PANNING;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_show_prop_names() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeUVFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeUVFunc::Function)

///////////////////////////////////////
/// UV POLARCOORD
///////////////////////////////////////

class VisualShaderNodeUVPolarCoord : public VisualShaderNode {
	GDCLASS(VisualShaderNodeUVPolarCoord, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeUVPolarCoord();
};

///////////////////////////////////////
/// DOT
///////////////////////////////////////

class VisualShaderNodeDotProduct : public VisualShaderNode {
	GDCLASS(VisualShaderNodeDotProduct, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_VECTOR; }

	VisualShaderNodeDotProduct();
};

///////////////////////////////////////
/// LENGTH
///////////////////////////////////////

class VisualShaderNodeVectorLen : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorLen, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_op_type(OpType p_op_type) override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeVectorLen();
};

///////////////////////////////////////
/// DETERMINANT
///////////////////////////////////////

class VisualShaderNodeDeterminant : public VisualShaderNode {
	GDCLASS(VisualShaderNodeDeterminant, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_VECTOR; }

	VisualShaderNodeDeterminant();
};

///////////////////////////////////////
/// CLAMP
///////////////////////////////////////

class VisualShaderNodeClamp : public VisualShaderNode {
	GDCLASS(VisualShaderNodeClamp, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_FLOAT,
		OP_TYPE_INT,
		OP_TYPE_UINT,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_FLOAT;
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_FLOAT || op_type == OP_TYPE_INT || op_type == OP_TYPE_UINT) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeClamp();
};

VARIANT_ENUM_CAST(VisualShaderNodeClamp::OpType)

///////////////////////////////////////
/// DERIVATIVE FUNCTION
///////////////////////////////////////

class VisualShaderNodeDerivativeFunc : public VisualShaderNode {
	GDCLASS(VisualShaderNodeDerivativeFunc, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_MAX,
	};

	enum Function {
		FUNC_SUM,
		FUNC_X,
		FUNC_Y,
		FUNC_MAX,
	};

	enum Precision {
		PRECISION_NONE,
		PRECISION_COARSE,
		PRECISION_FINE,
		PRECISION_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;
	Function func = FUNC_SUM;
	Precision precision = PRECISION_NONE;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;
	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	void set_function(Function p_func);
	Function get_function() const;

	void set_precision(Precision p_precision);
	Precision get_precision() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeDerivativeFunc();
};

VARIANT_ENUM_CAST(VisualShaderNodeDerivativeFunc::OpType)
VARIANT_ENUM_CAST(VisualShaderNodeDerivativeFunc::Function)
VARIANT_ENUM_CAST(VisualShaderNodeDerivativeFunc::Precision)

///////////////////////////////////////
/// FACEFORWARD
///////////////////////////////////////

class VisualShaderNodeFaceForward : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeFaceForward, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_op_type(OpType p_op_type) override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeFaceForward();
};

///////////////////////////////////////
/// OUTER PRODUCT
///////////////////////////////////////

class VisualShaderNodeOuterProduct : public VisualShaderNode {
	GDCLASS(VisualShaderNodeOuterProduct, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeOuterProduct();
};

///////////////////////////////////////
/// STEP
///////////////////////////////////////

class VisualShaderNodeStep : public VisualShaderNode {
	GDCLASS(VisualShaderNodeStep, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_2D_SCALAR,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_3D_SCALAR,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_VECTOR_4D_SCALAR,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual int get_default_input_port(PortType p_type) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_SCALAR) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeStep();
};

VARIANT_ENUM_CAST(VisualShaderNodeStep::OpType)

///////////////////////////////////////
/// SMOOTHSTEP
///////////////////////////////////////

class VisualShaderNodeSmoothStep : public VisualShaderNode {
	GDCLASS(VisualShaderNodeSmoothStep, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_2D_SCALAR,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_3D_SCALAR,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_VECTOR_4D_SCALAR,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual int get_default_input_port(PortType p_type) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_SCALAR) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeSmoothStep();
};

VARIANT_ENUM_CAST(VisualShaderNodeSmoothStep::OpType)

///////////////////////////////////////
/// DISTANCE
///////////////////////////////////////

class VisualShaderNodeVectorDistance : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorDistance, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_op_type(OpType p_op_type) override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeVectorDistance();
};

///////////////////////////////////////
/// REFRACT
///////////////////////////////////////

class VisualShaderNodeVectorRefract : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorRefract, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;
	virtual void set_op_type(OpType p_op_type) override;

	VisualShaderNodeVectorRefract();
};

///////////////////////////////////////
/// MIX
///////////////////////////////////////

class VisualShaderNodeMix : public VisualShaderNode {
	GDCLASS(VisualShaderNodeMix, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_2D_SCALAR,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_3D_SCALAR,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_VECTOR_4D_SCALAR,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_SCALAR) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeMix();
};

VARIANT_ENUM_CAST(VisualShaderNodeMix::OpType)

///////////////////////////////////////
/// COMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorCompose : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorCompose, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_op_type(OpType p_op_type) override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeVectorCompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformCompose : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTransformCompose, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeTransformCompose();
};

///////////////////////////////////////
/// DECOMPOSE
///////////////////////////////////////

class VisualShaderNodeVectorDecompose : public VisualShaderNodeVectorBase {
	GDCLASS(VisualShaderNodeVectorDecompose, VisualShaderNodeVectorBase);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_op_type(OpType p_op_type) override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeVectorDecompose();
};

///////////////////////////////////////

class VisualShaderNodeTransformDecompose : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTransformDecompose, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TRANSFORM; }

	VisualShaderNodeTransformDecompose();
};

///////////////////////////////////////
/// PARAMETERS
///////////////////////////////////////

class VisualShaderNodeFloatParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeFloatParameter, VisualShaderNodeParameter);

public:
	enum Hint {
		HINT_NONE,
		HINT_RANGE,
		HINT_RANGE_STEP,
		HINT_MAX,
	};

private:
	Hint hint = HINT_NONE;
	float hint_range_min = 0.0f;
	float hint_range_max = 1.0f;
	float hint_range_step = 0.1f;
	bool default_value_enabled = false;
	float default_value = 0.0f;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_hint(Hint p_hint);
	Hint get_hint() const;

	void set_min(float p_value);
	float get_min() const;

	void set_max(float p_value);
	float get_max() const;

	void set_step(float p_value);
	float get_step() const;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(float p_value);
	float get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeFloatParameter();
};

VARIANT_ENUM_CAST(VisualShaderNodeFloatParameter::Hint)

class VisualShaderNodeIntParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeIntParameter, VisualShaderNodeParameter);

public:
	enum Hint {
		HINT_NONE,
		HINT_RANGE,
		HINT_RANGE_STEP,
		HINT_ENUM,
		HINT_MAX,
	};

private:
	Hint hint = HINT_NONE;
	int hint_range_min = 0;
	int hint_range_max = 100;
	int hint_range_step = 1;
	PackedStringArray hint_enum_names;
	bool default_value_enabled = false;
	int default_value = 0;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_hint(Hint p_hint);
	Hint get_hint() const;

	void set_min(int p_value);
	int get_min() const;

	void set_max(int p_value);
	int get_max() const;

	void set_step(int p_value);
	int get_step() const;

	void set_enum_names(const PackedStringArray &p_names);
	PackedStringArray get_enum_names() const;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(int p_value);
	int get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeIntParameter();
};

VARIANT_ENUM_CAST(VisualShaderNodeIntParameter::Hint)

///////////////////////////////////////

class VisualShaderNodeUIntParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeUIntParameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	int default_value = 0;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(int p_value);
	int get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeUIntParameter();
};

///////////////////////////////////////

class VisualShaderNodeBooleanParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeBooleanParameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	bool default_value = false;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(bool p_value);
	bool get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeBooleanParameter();
};

///////////////////////////////////////

class VisualShaderNodeColorParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeColorParameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	Color default_value = Color(1.0, 1.0, 1.0, 1.0);

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(const Color &p_value);
	Color get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeColorParameter();
};

///////////////////////////////////////

class VisualShaderNodeVec2Parameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeVec2Parameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	Vector2 default_value;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(const Vector2 &p_value);
	Vector2 get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec2Parameter();
};

///////////////////////////////////////

class VisualShaderNodeVec3Parameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeVec3Parameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	Vector3 default_value;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(const Vector3 &p_value);
	Vector3 get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec3Parameter();
};

///////////////////////////////////////

class VisualShaderNodeVec4Parameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeVec4Parameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	Vector4 default_value;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(const Vector4 &p_value);
	Vector4 get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeVec4Parameter();
};

///////////////////////////////////////

class VisualShaderNodeTransformParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeTransformParameter, VisualShaderNodeParameter);

private:
	bool default_value_enabled = false;
	Transform3D default_value = Transform3D(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0);

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;
	virtual bool is_use_prop_slots() const override;

	void set_default_value_enabled(bool p_enabled);
	bool is_default_value_enabled() const;

	void set_default_value(const Transform3D &p_value);
	Transform3D get_default_value() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	virtual Vector<StringName> get_editable_properties() const override;

	VisualShaderNodeTransformParameter();
};

///////////////////////////////////////

class VisualShaderNodeTextureParameter : public VisualShaderNodeParameter {
	GDCLASS(VisualShaderNodeTextureParameter, VisualShaderNodeParameter);

public:
	enum TextureType {
		TYPE_DATA,
		TYPE_COLOR,
		TYPE_NORMAL_MAP,
		TYPE_ANISOTROPY,
		TYPE_MAX,
	};

	enum ColorDefault {
		COLOR_DEFAULT_WHITE,
		COLOR_DEFAULT_BLACK,
		COLOR_DEFAULT_TRANSPARENT,
		COLOR_DEFAULT_MAX,
	};

	enum TextureFilter {
		FILTER_DEFAULT,
		FILTER_NEAREST,
		FILTER_LINEAR,
		FILTER_NEAREST_MIPMAP,
		FILTER_LINEAR_MIPMAP,
		FILTER_NEAREST_MIPMAP_ANISOTROPIC,
		FILTER_LINEAR_MIPMAP_ANISOTROPIC,
		FILTER_MAX,
	};

	enum TextureRepeat {
		REPEAT_DEFAULT,
		REPEAT_ENABLED,
		REPEAT_DISABLED,
		REPEAT_MAX,
	};

	enum TextureSource {
		SOURCE_NONE,
		SOURCE_SCREEN,
		SOURCE_DEPTH,
		SOURCE_NORMAL_ROUGHNESS,
		SOURCE_MAX,
	};

protected:
	TextureType texture_type = TYPE_DATA;
	ColorDefault color_default = COLOR_DEFAULT_WHITE;
	TextureFilter texture_filter = FILTER_DEFAULT;
	TextureRepeat texture_repeat = REPEAT_DEFAULT;
	TextureSource texture_source = SOURCE_NONE;

protected:
	static void _bind_methods();

public:
	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual HashMap<StringName, String> get_editable_properties_names() const override;
	virtual bool is_show_prop_names() const override;
	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	Vector<StringName> get_editable_properties() const override;

	void set_texture_type(TextureType p_type);
	TextureType get_texture_type() const;

	void set_color_default(ColorDefault p_default);
	ColorDefault get_color_default() const;

	void set_texture_filter(TextureFilter p_filter);
	TextureFilter get_texture_filter() const;

	void set_texture_repeat(TextureRepeat p_repeat);
	TextureRepeat get_texture_repeat() const;

	void set_texture_source(TextureSource p_source);
	TextureSource get_texture_source() const;

	bool is_qualifier_supported(Qualifier p_qual) const override;
	bool is_convertible_to_constant() const override;

	VisualShaderNodeTextureParameter();
};

VARIANT_ENUM_CAST(VisualShaderNodeTextureParameter::TextureType)
VARIANT_ENUM_CAST(VisualShaderNodeTextureParameter::ColorDefault)
VARIANT_ENUM_CAST(VisualShaderNodeTextureParameter::TextureFilter)
VARIANT_ENUM_CAST(VisualShaderNodeTextureParameter::TextureRepeat)
VARIANT_ENUM_CAST(VisualShaderNodeTextureParameter::TextureSource)

///////////////////////////////////////

class VisualShaderNodeTexture2DParameter : public VisualShaderNodeTextureParameter {
	GDCLASS(VisualShaderNodeTexture2DParameter, VisualShaderNodeTextureParameter);

public:
	virtual String get_caption() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeTexture2DParameter();
};

///////////////////////////////////////

class VisualShaderNodeTextureParameterTriplanar : public VisualShaderNodeTextureParameter {
	GDCLASS(VisualShaderNodeTextureParameterTriplanar, VisualShaderNodeTextureParameter);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeTextureParameterTriplanar();
};

///////////////////////////////////////

class VisualShaderNodeTexture2DArrayParameter : public VisualShaderNodeTextureParameter {
	GDCLASS(VisualShaderNodeTexture2DArrayParameter, VisualShaderNodeTextureParameter);

public:
	virtual String get_caption() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeTexture2DArrayParameter();
};

///////////////////////////////////////

class VisualShaderNodeTexture3DParameter : public VisualShaderNodeTextureParameter {
	GDCLASS(VisualShaderNodeTexture3DParameter, VisualShaderNodeTextureParameter);

public:
	virtual String get_caption() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeTexture3DParameter();
};

///////////////////////////////////////

class VisualShaderNodeCubemapParameter : public VisualShaderNodeTextureParameter {
	GDCLASS(VisualShaderNodeCubemapParameter, VisualShaderNodeTextureParameter);

public:
	virtual String get_caption() const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeCubemapParameter();
};

///////////////////////////////////////
/// IF
///////////////////////////////////////

class VisualShaderNodeIf : public VisualShaderNode {
	GDCLASS(VisualShaderNodeIf, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_CONDITIONAL; }

	VisualShaderNodeIf();
};

///////////////////////////////////////
/// SWITCH
///////////////////////////////////////

class VisualShaderNodeSwitch : public VisualShaderNode {
	GDCLASS(VisualShaderNodeSwitch, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_FLOAT,
		OP_TYPE_INT,
		OP_TYPE_UINT,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_BOOLEAN,
		OP_TYPE_TRANSFORM,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_FLOAT;

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_CONDITIONAL; }

	VisualShaderNodeSwitch();
};

VARIANT_ENUM_CAST(VisualShaderNodeSwitch::OpType)

///////////////////////////////////////
/// FRESNEL
///////////////////////////////////////

class VisualShaderNodeFresnel : public VisualShaderNode {
	GDCLASS(VisualShaderNodeFresnel, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;
	virtual bool is_generate_input_var(int p_port) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeFresnel();
};

///////////////////////////////////////
/// Is
///////////////////////////////////////

class VisualShaderNodeIs : public VisualShaderNode {
	GDCLASS(VisualShaderNodeIs, VisualShaderNode);

public:
	enum Function {
		FUNC_IS_INF,
		FUNC_IS_NAN,
		FUNC_MAX,
	};

protected:
	Function func = FUNC_IS_INF;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_function(Function p_func);
	Function get_function() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_CONDITIONAL; }

	VisualShaderNodeIs();
};

VARIANT_ENUM_CAST(VisualShaderNodeIs::Function)

///////////////////////////////////////
/// Compare
///////////////////////////////////////

class VisualShaderNodeCompare : public VisualShaderNode {
	GDCLASS(VisualShaderNodeCompare, VisualShaderNode);

public:
	enum ComparisonType {
		CTYPE_SCALAR,
		CTYPE_SCALAR_INT,
		CTYPE_SCALAR_UINT,
		CTYPE_VECTOR_2D,
		CTYPE_VECTOR_3D,
		CTYPE_VECTOR_4D,
		CTYPE_BOOLEAN,
		CTYPE_TRANSFORM,
		CTYPE_MAX,
	};

	enum Function {
		FUNC_EQUAL,
		FUNC_NOT_EQUAL,
		FUNC_GREATER_THAN,
		FUNC_GREATER_THAN_EQUAL,
		FUNC_LESS_THAN,
		FUNC_LESS_THAN_EQUAL,
		FUNC_MAX,
	};

	enum Condition {
		COND_ALL,
		COND_ANY,
		COND_MAX,
	};

protected:
	ComparisonType comparison_type = CTYPE_SCALAR;
	Function func = FUNC_EQUAL;
	Condition condition = COND_ALL;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_comparison_type(ComparisonType p_type);
	ComparisonType get_comparison_type() const;

	void set_function(Function p_func);
	Function get_function() const;

	void set_condition(Condition p_cond);
	Condition get_condition() const;

	virtual Vector<StringName> get_editable_properties() const override;
	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	virtual Category get_category() const override { return CATEGORY_CONDITIONAL; }

	VisualShaderNodeCompare();
};

VARIANT_ENUM_CAST(VisualShaderNodeCompare::ComparisonType)
VARIANT_ENUM_CAST(VisualShaderNodeCompare::Function)
VARIANT_ENUM_CAST(VisualShaderNodeCompare::Condition)

class VisualShaderNodeMultiplyAdd : public VisualShaderNode {
	GDCLASS(VisualShaderNodeMultiplyAdd, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_SCALAR) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeMultiplyAdd();
};

VARIANT_ENUM_CAST(VisualShaderNodeMultiplyAdd::OpType)

class VisualShaderNodeBillboard : public VisualShaderNode {
	GDCLASS(VisualShaderNodeBillboard, VisualShaderNode);

public:
	enum BillboardType {
		BILLBOARD_TYPE_DISABLED,
		BILLBOARD_TYPE_ENABLED,
		BILLBOARD_TYPE_FIXED_Y,
		BILLBOARD_TYPE_PARTICLES,
		BILLBOARD_TYPE_MAX,
	};

protected:
	BillboardType billboard_type = BILLBOARD_TYPE_ENABLED;
	bool keep_scale = false;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual bool is_show_prop_names() const override;

	void set_billboard_type(BillboardType p_billboard_type);
	BillboardType get_billboard_type() const;

	void set_keep_scale_enabled(bool p_enabled);
	bool is_keep_scale_enabled() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeBillboard();
};

VARIANT_ENUM_CAST(VisualShaderNodeBillboard::BillboardType)

///////////////////////////////////////
/// DistanceFade
///////////////////////////////////////

class VisualShaderNodeDistanceFade : public VisualShaderNode {
	GDCLASS(VisualShaderNodeDistanceFade, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeDistanceFade();
};

class VisualShaderNodeProximityFade : public VisualShaderNode {
	GDCLASS(VisualShaderNodeProximityFade, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeProximityFade();
};

class VisualShaderNodeRandomRange : public VisualShaderNode {
	GDCLASS(VisualShaderNodeRandomRange, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeRandomRange();
};

///////////////////////////////////////
/// Remap
///////////////////////////////////////

class VisualShaderNodeRemap : public VisualShaderNode {
	GDCLASS(VisualShaderNodeRemap, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_2D_SCALAR,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_3D_SCALAR,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_VECTOR_4D_SCALAR,
		OP_TYPE_MAX,
	};

protected:
	OpType op_type = OP_TYPE_SCALAR;
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void set_op_type(OpType p_op_type);
	OpType get_op_type() const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override {
		if (op_type == OP_TYPE_SCALAR) {
			return CATEGORY_SCALAR;
		} else {
			return CATEGORY_VECTOR;
		}
	}

	VisualShaderNodeRemap();
};

VARIANT_ENUM_CAST(VisualShaderNodeRemap::OpType)

class VisualShaderNodeRotationByAxis : public VisualShaderNode {
	GDCLASS(VisualShaderNodeRotationByAxis, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_UTILITY; }

	VisualShaderNodeRotationByAxis();
};

class VisualShaderNodeReroute : public VisualShaderNode {
	GDCLASS(VisualShaderNodeReroute, VisualShaderNode);

	PortType input_port_type = PORT_TYPE_SCALAR;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override { return false; }
	virtual bool is_output_port_expandable(int p_port) const override { return false; }

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_SPECIAL; }

	void _set_port_type(PortType p_type);
	PortType get_port_type() const { return input_port_type; }

	VisualShaderNodeReroute();
};
