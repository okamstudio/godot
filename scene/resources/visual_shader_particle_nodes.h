/**************************************************************************/
/*  visual_shader_particle_nodes.h                                        */
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

#include "scene/resources/visual_shader.h"

class ImageTexture;

// Emit nodes

class VisualShaderNodeParticleEmitter : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleEmitter, VisualShaderNode);

protected:
	bool mode_2d = false;
	static void _bind_methods();

public:
	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual void set_mode_2d(bool p_enabled);
	bool is_mode_2d() const;

	Vector<StringName> get_editable_properties() const override;
	virtual HashMap<StringName, String> get_editable_properties_names() const override;
	bool is_show_prop_names() const override;

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleEmitter();
};

class VisualShaderNodeParticleSphereEmitter : public VisualShaderNodeParticleEmitter {
	GDCLASS(VisualShaderNodeParticleSphereEmitter, VisualShaderNodeParticleEmitter);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeParticleSphereEmitter();
};

class VisualShaderNodeParticleBoxEmitter : public VisualShaderNodeParticleEmitter {
	GDCLASS(VisualShaderNodeParticleBoxEmitter, VisualShaderNodeParticleEmitter);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual void set_mode_2d(bool p_enabled) override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeParticleBoxEmitter();
};

class VisualShaderNodeParticleRingEmitter : public VisualShaderNodeParticleEmitter {
	GDCLASS(VisualShaderNodeParticleRingEmitter, VisualShaderNodeParticleEmitter);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeParticleRingEmitter();
};

class VisualShaderNodeParticleMeshEmitter : public VisualShaderNodeParticleEmitter {
	GDCLASS(VisualShaderNodeParticleMeshEmitter, VisualShaderNodeParticleEmitter);
	Ref<Mesh> mesh;
	bool use_all_surfaces = true;
	int surface_index = 0;

	Ref<ImageTexture> position_texture;
	Ref<ImageTexture> normal_texture;
	Ref<ImageTexture> color_texture;
	Ref<ImageTexture> uv_texture;
	Ref<ImageTexture> uv2_texture;

	String _generate_code(VisualShader::Type p_type, int p_id, const String *p_output_vars, int p_index, const String &p_texture_name, PortType p_port_type) const;

	void _update_texture(const Vector<Vector2> &p_array, Ref<ImageTexture> &r_texture);
	void _update_texture(const Vector<Vector3> &p_array, Ref<ImageTexture> &r_texture);
	void _update_texture(const Vector<Color> &p_array, Ref<ImageTexture> &r_texture);
	void _update_textures();

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_mesh(Ref<Mesh> p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_use_all_surfaces(bool p_enabled);
	bool is_use_all_surfaces() const;

	void set_surface_index(int p_surface_index);
	int get_surface_index() const;

	Vector<StringName> get_editable_properties() const override;
	HashMap<StringName, String> get_editable_properties_names() const override;
	Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeParticleMeshEmitter();
};

class VisualShaderNodeParticleMultiplyByAxisAngle : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleMultiplyByAxisAngle, VisualShaderNode);
	bool degrees_mode = true;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_show_prop_names() const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_degrees_mode(bool p_enabled);
	bool is_degrees_mode() const;
	Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleMultiplyByAxisAngle();
};

class VisualShaderNodeParticleConeVelocity : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleConeVelocity, VisualShaderNode);

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

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleConeVelocity();
};

class VisualShaderNodeParticleRandomness : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleRandomness, VisualShaderNode);

public:
	enum OpType {
		OP_TYPE_SCALAR,
		OP_TYPE_VECTOR_2D,
		OP_TYPE_VECTOR_3D,
		OP_TYPE_VECTOR_4D,
		OP_TYPE_MAX,
	};

private:
	OpType op_type = OP_TYPE_SCALAR;

protected:
	static void _bind_methods();

public:
	Vector<StringName> get_editable_properties() const override;
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_op_type(OpType p_type);
	OpType get_op_type() const;

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleRandomness();
};

VARIANT_ENUM_CAST(VisualShaderNodeParticleRandomness::OpType)

// Process nodes

class VisualShaderNodeParticleAccelerator : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleAccelerator, VisualShaderNode);

public:
	enum Mode {
		MODE_LINEAR,
		MODE_RADIAL,
		MODE_TANGENTIAL,
		MODE_MAX,
	};

private:
	Mode mode = MODE_LINEAR;

protected:
	static void _bind_methods();

public:
	Vector<StringName> get_editable_properties() const override;
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleAccelerator();
};

VARIANT_ENUM_CAST(VisualShaderNodeParticleAccelerator::Mode)

// Common nodes

class VisualShaderNodeParticleOutput : public VisualShaderNodeOutput {
	GDCLASS(VisualShaderNodeParticleOutput, VisualShaderNodeOutput);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual bool is_port_separator(int p_index) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	VisualShaderNodeParticleOutput();
};

class VisualShaderNodeParticleEmit : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParticleEmit, VisualShaderNode);

public:
	enum EmitFlags {
		EMIT_FLAG_POSITION = 1,
		EMIT_FLAG_ROT_SCALE = 2,
		EMIT_FLAG_VELOCITY = 4,
		EMIT_FLAG_COLOR = 8,
		EMIT_FLAG_CUSTOM = 16,
	};

protected:
	int flags = EMIT_FLAG_POSITION | EMIT_FLAG_ROT_SCALE | EMIT_FLAG_VELOCITY | EMIT_FLAG_COLOR | EMIT_FLAG_CUSTOM;
	static void _bind_methods();

public:
	Vector<StringName> get_editable_properties() const override;
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	void add_flag(EmitFlags p_flag);
	bool has_flag(EmitFlags p_flag) const;

	void set_flags(EmitFlags p_flags);
	EmitFlags get_flags() const;

	virtual bool is_show_prop_names() const override;
	virtual bool is_generate_input_var(int p_port) const override;
	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_PARTICLE; }

	VisualShaderNodeParticleEmit();
};

VARIANT_ENUM_CAST(VisualShaderNodeParticleEmit::EmitFlags)
