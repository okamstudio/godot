/**************************************************************************/
/*  visual_shader_sdf_nodes.h                                             */
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

class VisualShaderNodeSDFToScreenUV : public VisualShaderNode {
	GDCLASS(VisualShaderNodeSDFToScreenUV, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeSDFToScreenUV();
};

class VisualShaderNodeScreenUVToSDF : public VisualShaderNode {
	GDCLASS(VisualShaderNodeScreenUVToSDF, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const override;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeScreenUVToSDF();
};

class VisualShaderNodeTextureSDF : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTextureSDF, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeTextureSDF();
};

class VisualShaderNodeTextureSDFNormal : public VisualShaderNode {
	GDCLASS(VisualShaderNodeTextureSDFNormal, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeTextureSDFNormal();
};

class VisualShaderNodeSDFRaymarch : public VisualShaderNode {
	GDCLASS(VisualShaderNodeSDFRaymarch, VisualShaderNode);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_TEXTURES; }

	VisualShaderNodeSDFRaymarch();
};
