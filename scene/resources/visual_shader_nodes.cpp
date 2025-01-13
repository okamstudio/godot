/**************************************************************************/
/*  visual_shader_nodes.cpp                                               */
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

#include "visual_shader_nodes.h"
#include "visual_shader_nodes.compat.inc"

#include "scene/resources/image_texture.h"

////////////// Vector Base

VisualShaderNodeVectorBase::PortType VisualShaderNodeVectorBase::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

VisualShaderNodeVectorBase::PortType VisualShaderNodeVectorBase::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return p_port == 0 || get_output_port_count() > 1 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
			return p_port == 0 || get_output_port_count() > 1 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
			return p_port == 0 || get_output_port_count() > 1 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

void VisualShaderNodeVectorBase::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeVectorBase::OpType VisualShaderNodeVectorBase::get_op_type() const {
	return op_type;
}

void VisualShaderNodeVectorBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "type"), &VisualShaderNodeVectorBase::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeVectorBase::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Vector2,Vector3,Vector4"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

Vector<StringName> VisualShaderNodeVectorBase::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

VisualShaderNodeVectorBase::VisualShaderNodeVectorBase() {
}

////////////// Constants Base

VisualShaderNodeConstant::VisualShaderNodeConstant() {
}

////////////// Scalar(Float)

String VisualShaderNodeFloatConstant::get_caption() const {
	return "FloatConstant";
}

int VisualShaderNodeFloatConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeFloatConstant::PortType VisualShaderNodeFloatConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeFloatConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeFloatConstant::PortType VisualShaderNodeFloatConstant::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatConstant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeFloatConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + vformat("%.6f", constant) + ";\n";
}

void VisualShaderNodeFloatConstant::set_constant(float p_constant) {
	if (Math::is_equal_approx(constant, p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

float VisualShaderNodeFloatConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeFloatConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeFloatConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeFloatConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeFloatConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeFloatConstant::VisualShaderNodeFloatConstant() {
}

////////////// Scalar(Int)

String VisualShaderNodeIntConstant::get_caption() const {
	return "IntConstant";
}

int VisualShaderNodeIntConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeIntConstant::PortType VisualShaderNodeIntConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeIntConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIntConstant::PortType VisualShaderNodeIntConstant::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntConstant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeIntConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + itos(constant) + ";\n";
}

void VisualShaderNodeIntConstant::set_constant(int p_constant) {
	if (constant == p_constant) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

int VisualShaderNodeIntConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeIntConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeIntConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeIntConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeIntConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeIntConstant::VisualShaderNodeIntConstant() {
}

////////////// Scalar(UInt)

String VisualShaderNodeUIntConstant::get_caption() const {
	return "UIntConstant";
}

int VisualShaderNodeUIntConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeUIntConstant::PortType VisualShaderNodeUIntConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeUIntConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUIntConstant::PortType VisualShaderNodeUIntConstant::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntConstant::get_output_port_name(int p_port) const {
	return ""; // No output port means the editor will be used as port.
}

String VisualShaderNodeUIntConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + itos(constant) + "u;\n";
}

void VisualShaderNodeUIntConstant::set_constant(int p_constant) {
	if (constant == p_constant) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

int VisualShaderNodeUIntConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeUIntConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeUIntConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeUIntConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeUIntConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeUIntConstant::VisualShaderNodeUIntConstant() {
}

////////////// Boolean

String VisualShaderNodeBooleanConstant::get_caption() const {
	return "BooleanConstant";
}

int VisualShaderNodeBooleanConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeBooleanConstant::PortType VisualShaderNodeBooleanConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeBooleanConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeBooleanConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeBooleanConstant::PortType VisualShaderNodeBooleanConstant::get_output_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeBooleanConstant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeBooleanConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + (constant ? "true" : "false") + ";\n";
}

void VisualShaderNodeBooleanConstant::set_constant(bool p_constant) {
	if (constant == p_constant) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

bool VisualShaderNodeBooleanConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeBooleanConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeBooleanConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeBooleanConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeBooleanConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeBooleanConstant::VisualShaderNodeBooleanConstant() {
}

////////////// Color

String VisualShaderNodeColorConstant::get_caption() const {
	return "ColorConstant";
}

int VisualShaderNodeColorConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeColorConstant::PortType VisualShaderNodeColorConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_4D;
}

String VisualShaderNodeColorConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeColorConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeColorConstant::PortType VisualShaderNodeColorConstant::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeColorConstant::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeColorConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + vformat("vec4(%.6f, %.6f, %.6f, %.6f)", constant.r, constant.g, constant.b, constant.a) + ";\n";
}

void VisualShaderNodeColorConstant::set_constant(const Color &p_constant) {
	if (constant.is_equal_approx(p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

Color VisualShaderNodeColorConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeColorConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeColorConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeColorConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeColorConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeColorConstant::VisualShaderNodeColorConstant() {
}

////////////// Vector2

String VisualShaderNodeVec2Constant::get_caption() const {
	return "Vector2Constant";
}

int VisualShaderNodeVec2Constant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec2Constant::PortType VisualShaderNodeVec2Constant::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_2D;
}

String VisualShaderNodeVec2Constant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec2Constant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec2Constant::PortType VisualShaderNodeVec2Constant::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec2Constant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeVec2Constant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + vformat("vec2(%.6f, %.6f)", constant.x, constant.y) + ";\n";
}

void VisualShaderNodeVec2Constant::set_constant(const Vector2 &p_constant) {
	if (constant.is_equal_approx(p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

Vector2 VisualShaderNodeVec2Constant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeVec2Constant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeVec2Constant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeVec2Constant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeVec2Constant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeVec2Constant::VisualShaderNodeVec2Constant() {
}

////////////// Vector3

String VisualShaderNodeVec3Constant::get_caption() const {
	return "Vector3Constant";
}

int VisualShaderNodeVec3Constant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec3Constant::PortType VisualShaderNodeVec3Constant::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeVec3Constant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec3Constant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec3Constant::PortType VisualShaderNodeVec3Constant::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec3Constant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeVec3Constant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + vformat("vec3(%.6f, %.6f, %.6f)", constant.x, constant.y, constant.z) + ";\n";
}

void VisualShaderNodeVec3Constant::set_constant(const Vector3 &p_constant) {
	if (constant.is_equal_approx(p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

Vector3 VisualShaderNodeVec3Constant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeVec3Constant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeVec3Constant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeVec3Constant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeVec3Constant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeVec3Constant::VisualShaderNodeVec3Constant() {
}

////////////// Vector4

String VisualShaderNodeVec4Constant::get_caption() const {
	return "Vector4Constant";
}

int VisualShaderNodeVec4Constant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec4Constant::PortType VisualShaderNodeVec4Constant::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_4D;
}

String VisualShaderNodeVec4Constant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec4Constant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec4Constant::PortType VisualShaderNodeVec4Constant::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec4Constant::get_output_port_name(int p_port) const {
	return ""; // No output port means the editor will be used as port.
}

String VisualShaderNodeVec4Constant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + vformat("vec4(%.6f, %.6f, %.6f, %.6f)", constant.x, constant.y, constant.z, constant.w) + ";\n";
}

void VisualShaderNodeVec4Constant::set_constant(const Quaternion &p_constant) {
	if (constant.is_equal_approx(p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

Quaternion VisualShaderNodeVec4Constant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeVec4Constant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeVec4Constant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeVec4Constant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeVec4Constant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::QUATERNION, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeVec4Constant::VisualShaderNodeVec4Constant() {
}

////////////// Transform3D

String VisualShaderNodeTransformConstant::get_caption() const {
	return "TransformConstant";
}

int VisualShaderNodeTransformConstant::get_input_port_count() const {
	return 0;
}

VisualShaderNodeTransformConstant::PortType VisualShaderNodeTransformConstant::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeTransformConstant::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeTransformConstant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformConstant::PortType VisualShaderNodeTransformConstant::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformConstant::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeTransformConstant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	Transform3D t = constant;
	t.basis.transpose();

	String code = "	" + p_output_vars[0] + " = mat4(";
	code += vformat("vec4(%.6f, %.6f, %.6f, 0.0), ", t.basis[0].x, t.basis[0].y, t.basis[0].z);
	code += vformat("vec4(%.6f, %.6f, %.6f, 0.0), ", t.basis[1].x, t.basis[1].y, t.basis[1].z);
	code += vformat("vec4(%.6f, %.6f, %.6f, 0.0), ", t.basis[2].x, t.basis[2].y, t.basis[2].z);
	code += vformat("vec4(%.6f, %.6f, %.6f, 1.0));\n", t.origin.x, t.origin.y, t.origin.z);
	return code;
}

void VisualShaderNodeTransformConstant::set_constant(const Transform3D &p_constant) {
	if (constant.is_equal_approx(p_constant)) {
		return;
	}
	constant = p_constant;
	emit_changed();
}

Transform3D VisualShaderNodeTransformConstant::get_constant() const {
	return constant;
}

Vector<StringName> VisualShaderNodeTransformConstant::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("constant");
	return props;
}

void VisualShaderNodeTransformConstant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constant", "constant"), &VisualShaderNodeTransformConstant::set_constant);
	ClassDB::bind_method(D_METHOD("get_constant"), &VisualShaderNodeTransformConstant::get_constant);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "constant"), "set_constant", "get_constant");
}

VisualShaderNodeTransformConstant::VisualShaderNodeTransformConstant() {
}

////////////// Texture

String VisualShaderNodeTexture::get_caption() const {
	return "Texture2D";
}

int VisualShaderNodeTexture::get_input_port_count() const {
	return 3;
}

VisualShaderNodeTexture::PortType VisualShaderNodeTexture::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_2D;
		case 1:
			return PORT_TYPE_SCALAR;
		case 2:
			return PORT_TYPE_SAMPLER;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeTexture::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "uv";
		case 1:
			return "lod";
		case 2:
			return "sampler2D";
		default:
			return "";
	}
}

int VisualShaderNodeTexture::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTexture::PortType VisualShaderNodeTexture::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeTexture::get_output_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "color";
		default:
			return "";
	}
}

bool VisualShaderNodeTexture::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (p_port == 0) {
			return true;
		}
	}
	return false;
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeTexture::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "tex");
	dtp.params.push_back(texture);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

String VisualShaderNodeTexture::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code;

	switch (source) {
		case SOURCE_TEXTURE: {
			code += "uniform sampler2D " + make_unique_id(p_type, p_id, "tex");
			switch (texture_type) {
				case TYPE_DATA: {
				} break;
				case TYPE_COLOR: {
					code += " : source_color";
				} break;
				case TYPE_NORMAL_MAP: {
					code += " : hint_normal";
				} break;
				default: {
				} break;
			}
			code += ";\n";
		} break;
		case SOURCE_SCREEN: {
			if ((p_mode == Shader::MODE_SPATIAL || p_mode == Shader::MODE_CANVAS_ITEM) && p_type == VisualShader::TYPE_FRAGMENT) {
				code += "uniform sampler2D " + make_unique_id(p_type, p_id, "screen_tex") + " : hint_screen_texture;\n";
			}
		} break;
		case SOURCE_DEPTH:
		case SOURCE_3D_NORMAL:
		case SOURCE_ROUGHNESS: {
			if (p_mode == Shader::MODE_SPATIAL && p_type == VisualShader::TYPE_FRAGMENT) {
				String sampler_name = "";
				String hint = " : ";
				if (source == SOURCE_DEPTH) {
					sampler_name = "depth_tex";
					hint += "hint_depth_texture;\n";
				} else {
					sampler_name = source == SOURCE_ROUGHNESS ? "roughness_tex" : "normal_roughness_tex";
					hint += "hint_normal_roughness_texture;\n";
				}
				code += "uniform sampler2D " + make_unique_id(p_type, p_id, sampler_name) + hint;
			}
		} break;
		default: {
		} break;
	}

	return code;
}

String VisualShaderNodeTexture::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String default_uv;
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (source == SOURCE_SCREEN) {
			default_uv = "SCREEN_UV";
		} else {
			default_uv = "UV";
		}
	} else {
		default_uv = "vec2(0.0)";
	}

	String code;
	String uv = p_input_vars[0].is_empty() ? default_uv : p_input_vars[0];

	switch (source) {
		case SOURCE_PORT:
		case SOURCE_TEXTURE: {
			String id;
			if (source == SOURCE_PORT) {
				id = p_input_vars[2];
				if (id.is_empty()) {
					break;
				}
			} else { // SOURCE_TEXTURE
				id = make_unique_id(p_type, p_id, "tex");
			}
			if (p_input_vars[1].is_empty()) {
				code += "	" + p_output_vars[0] + " = texture(" + id + ", " + uv + ");\n";
			} else {
				code += "	" + p_output_vars[0] + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ");\n";
			}
			return code;
		} break;
		case SOURCE_SCREEN: {
			if ((p_mode == Shader::MODE_SPATIAL || p_mode == Shader::MODE_CANVAS_ITEM) && p_type == VisualShader::TYPE_FRAGMENT) {
				String id = make_unique_id(p_type, p_id, "screen_tex");
				if (p_input_vars[1].is_empty()) {
					code += "	" + p_output_vars[0] + " = texture(" + id + ", " + uv + ");\n";
				} else {
					code += "	" + p_output_vars[0] + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ");\n";
				}
				return code;
			}
		} break;
		case SOURCE_2D_NORMAL:
		case SOURCE_2D_TEXTURE: {
			if (p_mode == Shader::MODE_CANVAS_ITEM && p_type == VisualShader::TYPE_FRAGMENT) {
				String id = source == SOURCE_2D_TEXTURE ? "TEXTURE" : "NORMAL_TEXTURE";

				if (p_input_vars[1].is_empty()) {
					code += "	" + p_output_vars[0] + " = texture(" + id + ", " + uv + ");\n";
				} else {
					code += "	" + p_output_vars[0] + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ");\n";
				}
				return code;
			}
		} break;
		case SOURCE_3D_NORMAL:
		case SOURCE_ROUGHNESS:
		case SOURCE_DEPTH: {
			if (!p_for_preview && p_mode == Shader::MODE_SPATIAL && p_type == VisualShader::TYPE_FRAGMENT) {
				String var_name = "";
				String sampler_name = "";

				switch (source) {
					case SOURCE_DEPTH: {
						var_name = "_depth";
						sampler_name = "depth_tex";
					} break;
					case SOURCE_ROUGHNESS: {
						var_name = "_roughness";
						sampler_name = "roughness_tex";
					} break;
					case SOURCE_3D_NORMAL: {
						var_name = "_normal";
						sampler_name = "normal_roughness_tex";
					} break;
					default: {
					} break;
				}

				String id = make_unique_id(p_type, p_id, sampler_name);
				String type = source == SOURCE_3D_NORMAL ? "vec3" : "float";
				String components = source == SOURCE_3D_NORMAL ? "rgb" : "r";

				code += "	{\n";
				if (p_input_vars[1].is_empty()) {
					code += "		" + type + " " + var_name + " = texture(" + id + ", " + uv + ")." + components + ";\n";
				} else {
					code += "		" + type + " " + var_name + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ")." + components + ";\n";
				}
				if (source == SOURCE_3D_NORMAL) {
					code += "		" + p_output_vars[0] + " = vec4(" + var_name + ", 1.0);\n";
				} else {
					code += "		" + p_output_vars[0] + " = vec4(" + var_name + ", " + var_name + ", " + var_name + ", 1.0);\n";
				}
				code += "	}\n";

				return code;
			}
		} break;
		default: {
		} break;
	}

	code += "	" + p_output_vars[0] + " = vec4(0.0);\n";
	return code;
}

void VisualShaderNodeTexture::set_source(Source p_source) {
	ERR_FAIL_INDEX(int(p_source), int(SOURCE_MAX));
	if (source == p_source) {
		return;
	}
	switch (p_source) {
		case SOURCE_TEXTURE:
			simple_decl = true;
			break;
		case SOURCE_SCREEN:
			simple_decl = false;
			break;
		case SOURCE_2D_TEXTURE:
			simple_decl = false;
			break;
		case SOURCE_2D_NORMAL:
			simple_decl = false;
			break;
		case SOURCE_DEPTH:
			simple_decl = false;
			break;
		case SOURCE_PORT:
			simple_decl = false;
			break;
		case SOURCE_3D_NORMAL:
			simple_decl = false;
			break;
		case SOURCE_ROUGHNESS:
			simple_decl = false;
			break;
		default:
			break;
	}
	source = p_source;
	emit_changed();
}

VisualShaderNodeTexture::Source VisualShaderNodeTexture::get_source() const {
	return source;
}

void VisualShaderNodeTexture::set_texture(Ref<Texture2D> p_texture) {
	texture = p_texture;
	emit_changed();
}

Ref<Texture2D> VisualShaderNodeTexture::get_texture() const {
	return texture;
}

void VisualShaderNodeTexture::set_texture_type(TextureType p_texture_type) {
	ERR_FAIL_INDEX(int(p_texture_type), int(TYPE_MAX));
	if (texture_type == p_texture_type) {
		return;
	}
	texture_type = p_texture_type;
	emit_changed();
}

VisualShaderNodeTexture::TextureType VisualShaderNodeTexture::get_texture_type() const {
	return texture_type;
}

Vector<StringName> VisualShaderNodeTexture::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("source");
	if (source == SOURCE_TEXTURE) {
		props.push_back("texture");
		props.push_back("texture_type");
	}
	return props;
}

String VisualShaderNodeTexture::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	if (is_input_port_connected(2) && source != SOURCE_PORT) {
		return RTR("The sampler port is connected but not used. Consider changing the source to 'SamplerPort'.");
	}

	switch (source) {
		case SOURCE_TEXTURE:
		case SOURCE_PORT: {
			return String(); // All good.
		} break;
		case SOURCE_SCREEN: {
			if ((p_mode == Shader::MODE_SPATIAL || p_mode == Shader::MODE_CANVAS_ITEM) && p_type == VisualShader::TYPE_FRAGMENT) {
				return String(); // All good.
			}
		} break;
		case SOURCE_2D_NORMAL:
		case SOURCE_2D_TEXTURE: {
			if (p_mode == Shader::MODE_CANVAS_ITEM && p_type == VisualShader::TYPE_FRAGMENT) {
				return String(); // All good.
			}
		} break;
		case SOURCE_3D_NORMAL:
		case SOURCE_ROUGHNESS:
		case SOURCE_DEPTH: {
			if (p_mode == Shader::MODE_SPATIAL && p_type == VisualShader::TYPE_FRAGMENT) {
				if (get_output_port_for_preview() == 0) { // Not supported in preview(canvas_item) shader.
					return RTR("Invalid source for preview.");
				}
				return String(); // All good.
			}
		} break;
		default: {
		} break;
	}

	return RTR("Invalid source for shader.");
}

void VisualShaderNodeTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source", "value"), &VisualShaderNodeTexture::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &VisualShaderNodeTexture::get_source);

	ClassDB::bind_method(D_METHOD("set_texture", "value"), &VisualShaderNodeTexture::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &VisualShaderNodeTexture::get_texture);

	ClassDB::bind_method(D_METHOD("set_texture_type", "value"), &VisualShaderNodeTexture::set_texture_type);
	ClassDB::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeTexture::get_texture_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Texture,Screen,Texture2D,NormalMap2D,Depth,SamplerPort,Normal3D,Roughness"), "set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_type", PROPERTY_HINT_ENUM, "Data,Color,Normal Map"), "set_texture_type", "get_texture_type");

	BIND_ENUM_CONSTANT(SOURCE_TEXTURE);
	BIND_ENUM_CONSTANT(SOURCE_SCREEN);
	BIND_ENUM_CONSTANT(SOURCE_2D_TEXTURE);
	BIND_ENUM_CONSTANT(SOURCE_2D_NORMAL);
	BIND_ENUM_CONSTANT(SOURCE_DEPTH);
	BIND_ENUM_CONSTANT(SOURCE_PORT);
	BIND_ENUM_CONSTANT(SOURCE_3D_NORMAL);
	BIND_ENUM_CONSTANT(SOURCE_ROUGHNESS);
	BIND_ENUM_CONSTANT(SOURCE_MAX);

	BIND_ENUM_CONSTANT(TYPE_DATA);
	BIND_ENUM_CONSTANT(TYPE_COLOR);
	BIND_ENUM_CONSTANT(TYPE_NORMAL_MAP);
	BIND_ENUM_CONSTANT(TYPE_MAX);
}

VisualShaderNodeTexture::VisualShaderNodeTexture() {
}

////////////// CurveTexture

String VisualShaderNodeCurveTexture::get_caption() const {
	return "CurveTexture";
}

int VisualShaderNodeCurveTexture::get_input_port_count() const {
	return 1;
}

VisualShaderNodeCurveTexture::PortType VisualShaderNodeCurveTexture::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeCurveTexture::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeCurveTexture::get_output_port_count() const {
	return 1;
}

VisualShaderNodeCurveTexture::PortType VisualShaderNodeCurveTexture::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeCurveTexture::get_output_port_name(int p_port) const {
	return String();
}

void VisualShaderNodeCurveTexture::set_texture(Ref<CurveTexture> p_texture) {
	texture = p_texture;
	emit_changed();
}

Ref<CurveTexture> VisualShaderNodeCurveTexture::get_texture() const {
	return texture;
}

Vector<StringName> VisualShaderNodeCurveTexture::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("texture");
	return props;
}

String VisualShaderNodeCurveTexture::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "curve") + " : repeat_disable;\n";
}

String VisualShaderNodeCurveTexture::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	if (p_input_vars[0].is_empty()) {
		return "	" + p_output_vars[0] + " = 0.0;\n";
	}
	String id = make_unique_id(p_type, p_id, "curve");
	String code;
	code += "	" + p_output_vars[0] + " = texture(" + id + ", vec2(" + p_input_vars[0] + ")).r;\n";
	return code;
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeCurveTexture::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "curve");
	dtp.params.push_back(texture);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

void VisualShaderNodeCurveTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &VisualShaderNodeCurveTexture::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &VisualShaderNodeCurveTexture::get_texture);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_texture", "get_texture");
}

bool VisualShaderNodeCurveTexture::is_use_prop_slots() const {
	return true;
}

VisualShaderNodeCurveTexture::VisualShaderNodeCurveTexture() {
	set_input_port_default_value(0, 0.0);
	simple_decl = true;
	allow_v_resize = false;
}

////////////// CurveXYZTexture

String VisualShaderNodeCurveXYZTexture::get_caption() const {
	return "CurveXYZTexture";
}

int VisualShaderNodeCurveXYZTexture::get_input_port_count() const {
	return 1;
}

VisualShaderNodeCurveXYZTexture::PortType VisualShaderNodeCurveXYZTexture::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeCurveXYZTexture::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeCurveXYZTexture::get_output_port_count() const {
	return 1;
}

VisualShaderNodeCurveXYZTexture::PortType VisualShaderNodeCurveXYZTexture::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeCurveXYZTexture::get_output_port_name(int p_port) const {
	return String();
}

void VisualShaderNodeCurveXYZTexture::set_texture(Ref<CurveXYZTexture> p_texture) {
	texture = p_texture;
	emit_changed();
}

Ref<CurveXYZTexture> VisualShaderNodeCurveXYZTexture::get_texture() const {
	return texture;
}

Vector<StringName> VisualShaderNodeCurveXYZTexture::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("texture");
	return props;
}

String VisualShaderNodeCurveXYZTexture::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "curve3d") + ";\n";
}

String VisualShaderNodeCurveXYZTexture::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	if (p_input_vars[0].is_empty()) {
		return "	" + p_output_vars[0] + " = vec3(0.0);\n";
	}
	String id = make_unique_id(p_type, p_id, "curve3d");
	String code;
	code += "	" + p_output_vars[0] + " = texture(" + id + ", vec2(" + p_input_vars[0] + ")).rgb;\n";
	return code;
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeCurveXYZTexture::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "curve3d");
	dtp.params.push_back(texture);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

void VisualShaderNodeCurveXYZTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &VisualShaderNodeCurveXYZTexture::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &VisualShaderNodeCurveXYZTexture::get_texture);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "CurveXYZTexture"), "set_texture", "get_texture");
}

bool VisualShaderNodeCurveXYZTexture::is_use_prop_slots() const {
	return true;
}

VisualShaderNodeCurveXYZTexture::VisualShaderNodeCurveXYZTexture() {
	set_input_port_default_value(0, 0.0);
	simple_decl = true;
	allow_v_resize = false;
}

////////////// Sample3D

int VisualShaderNodeSample3D::get_input_port_count() const {
	return 3;
}

VisualShaderNodeSample3D::PortType VisualShaderNodeSample3D::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_SCALAR;
		case 2:
			return PORT_TYPE_SAMPLER;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeSample3D::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "uvw";
		case 1:
			return "lod";
		default:
			return "";
	}
}

int VisualShaderNodeSample3D::get_output_port_count() const {
	return 1;
}

VisualShaderNodeSample3D::PortType VisualShaderNodeSample3D::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeSample3D::get_output_port_name(int p_port) const {
	return "color";
}

bool VisualShaderNodeSample3D::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (p_port == 0) {
			return true;
		}
	}
	return false;
}

String VisualShaderNodeSample3D::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	String id;
	if (source == SOURCE_TEXTURE) {
		id = make_unique_id(p_type, p_id, "tex3d");
	} else { // SOURCE_PORT
		id = p_input_vars[2];
		if (id.is_empty()) {
			code += "	" + p_output_vars[0] + " = vec4(0.0);\n";
			return code;
		}
	}
	String default_uv;
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		default_uv = "vec3(UV, 0.0)";
	} else {
		default_uv = "vec3(0.0)";
	}

	String uv = p_input_vars[0].is_empty() ? default_uv : p_input_vars[0];
	if (p_input_vars[1].is_empty()) {
		code += "	" + p_output_vars[0] + " = texture(" + id + ", " + uv + ");\n";
	} else {
		code += "	" + p_output_vars[0] + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ");\n";
	}
	return code;
}

void VisualShaderNodeSample3D::set_source(Source p_source) {
	ERR_FAIL_INDEX(int(p_source), int(SOURCE_MAX));
	if (source == p_source) {
		return;
	}
	source = p_source;
	emit_changed();
}

VisualShaderNodeSample3D::Source VisualShaderNodeSample3D::get_source() const {
	return source;
}

void VisualShaderNodeSample3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source", "value"), &VisualShaderNodeSample3D::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &VisualShaderNodeSample3D::get_source);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Texture,SamplerPort"), "set_source", "get_source");

	BIND_ENUM_CONSTANT(SOURCE_TEXTURE);
	BIND_ENUM_CONSTANT(SOURCE_PORT);
	BIND_ENUM_CONSTANT(SOURCE_MAX);
}

String VisualShaderNodeSample3D::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	if (is_input_port_connected(2) && source != SOURCE_PORT) {
		return RTR("The sampler port is connected but not used. Consider changing the source to 'SamplerPort'.");
	}
	return String();
}

VisualShaderNodeSample3D::VisualShaderNodeSample3D() {
	simple_decl = false;
}

////////////// Texture2DArray

String VisualShaderNodeTexture2DArray::get_caption() const {
	return "Texture2DArray";
}

String VisualShaderNodeTexture2DArray::get_input_port_name(int p_port) const {
	if (p_port == 2) {
		return "sampler2DArray";
	}
	return VisualShaderNodeSample3D::get_input_port_name(p_port);
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeTexture2DArray::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "tex3d");
	dtp.params.push_back(texture_array);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

String VisualShaderNodeTexture2DArray::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	if (source == SOURCE_TEXTURE) {
		return "uniform sampler2DArray " + make_unique_id(p_type, p_id, "tex3d") + ";\n";
	}
	return String();
}

void VisualShaderNodeTexture2DArray::set_texture_array(Ref<TextureLayered> p_texture_array) {
	texture_array = p_texture_array;
	emit_changed();
}

Ref<TextureLayered> VisualShaderNodeTexture2DArray::get_texture_array() const {
	return texture_array;
}

Vector<StringName> VisualShaderNodeTexture2DArray::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("source");
	if (source == SOURCE_TEXTURE) {
		props.push_back("texture_array");
	}
	return props;
}

void VisualShaderNodeTexture2DArray::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture_array", "value"), &VisualShaderNodeTexture2DArray::set_texture_array);
	ClassDB::bind_method(D_METHOD("get_texture_array"), &VisualShaderNodeTexture2DArray::get_texture_array);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_array", PROPERTY_HINT_RESOURCE_TYPE, "Texture2DArray,CompressedTexture2DArray,PlaceholderTexture2DArray,Texture2DArrayRD"), "set_texture_array", "get_texture_array");
}

VisualShaderNodeTexture2DArray::VisualShaderNodeTexture2DArray() {
}

////////////// Texture3D

String VisualShaderNodeTexture3D::get_caption() const {
	return "Texture3D";
}

String VisualShaderNodeTexture3D::get_input_port_name(int p_port) const {
	if (p_port == 2) {
		return "sampler3D";
	}
	return VisualShaderNodeSample3D::get_input_port_name(p_port);
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeTexture3D::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "tex3d");
	dtp.params.push_back(texture);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

String VisualShaderNodeTexture3D::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	if (source == SOURCE_TEXTURE) {
		return "uniform sampler3D " + make_unique_id(p_type, p_id, "tex3d") + ";\n";
	}
	return String();
}

void VisualShaderNodeTexture3D::set_texture(Ref<Texture3D> p_texture) {
	texture = p_texture;
	emit_changed();
}

Ref<Texture3D> VisualShaderNodeTexture3D::get_texture() const {
	return texture;
}

Vector<StringName> VisualShaderNodeTexture3D::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("source");
	if (source == SOURCE_TEXTURE) {
		props.push_back("texture");
	}
	return props;
}

void VisualShaderNodeTexture3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture", "value"), &VisualShaderNodeTexture3D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &VisualShaderNodeTexture3D::get_texture);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture3D"), "set_texture", "get_texture");
}

VisualShaderNodeTexture3D::VisualShaderNodeTexture3D() {
}

////////////// Cubemap

String VisualShaderNodeCubemap::get_caption() const {
	return "Cubemap";
}

int VisualShaderNodeCubemap::get_input_port_count() const {
	return 3;
}

VisualShaderNodeCubemap::PortType VisualShaderNodeCubemap::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_SCALAR;
		case 2:
			return PORT_TYPE_SAMPLER;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeCubemap::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "uv";
		case 1:
			return "lod";
		case 2:
			return "samplerCube";
		default:
			return "";
	}
}

int VisualShaderNodeCubemap::get_output_port_count() const {
	return 1;
}

VisualShaderNodeCubemap::PortType VisualShaderNodeCubemap::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeCubemap::get_output_port_name(int p_port) const {
	return "color";
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNodeCubemap::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	VisualShader::DefaultTextureParam dtp;
	dtp.name = make_unique_id(p_type, p_id, "cube");
	dtp.params.push_back(cube_map);
	Vector<VisualShader::DefaultTextureParam> ret;
	ret.push_back(dtp);
	return ret;
}

String VisualShaderNodeCubemap::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	if (source == SOURCE_TEXTURE) {
		String u = "uniform samplerCube " + make_unique_id(p_type, p_id, "cube");
		switch (texture_type) {
			case TYPE_DATA:
				break;
			case TYPE_COLOR:
				u += " : source_color";
				break;
			case TYPE_NORMAL_MAP:
				u += " : hint_normal";
				break;
			default:
				break;
		}
		return u + ";\n";
	}
	return String();
}

String VisualShaderNodeCubemap::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	String id;

	if (source == SOURCE_TEXTURE) {
		id = make_unique_id(p_type, p_id, "cube");
	} else { // SOURCE_PORT
		id = p_input_vars[2];
		if (id.is_empty()) {
			code += "	" + p_output_vars[0] + " = vec4(0.0);\n";
			return code;
		}
	}

	String default_uv;
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		default_uv = "vec3(UV, 0.0)";
	} else {
		default_uv = "vec3(0.0)";
	}

	String uv = p_input_vars[0].is_empty() ? default_uv : p_input_vars[0];
	if (p_input_vars[1].is_empty()) {
		code += "	" + p_output_vars[0] + " = texture(" + id + ", " + uv + ");\n";
	} else {
		code += "	" + p_output_vars[0] + " = textureLod(" + id + ", " + uv + ", " + p_input_vars[1] + ");\n";
	}

	return code;
}

bool VisualShaderNodeCubemap::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (p_port == 0) {
			return true;
		}
	}
	return false;
}

void VisualShaderNodeCubemap::set_source(Source p_source) {
	ERR_FAIL_INDEX(int(p_source), int(SOURCE_MAX));
	if (source == p_source) {
		return;
	}
	source = p_source;
	emit_changed();
}

VisualShaderNodeCubemap::Source VisualShaderNodeCubemap::get_source() const {
	return source;
}

void VisualShaderNodeCubemap::set_cube_map(Ref<TextureLayered> p_cube_map) {
	cube_map = p_cube_map;
	emit_changed();
}

Ref<TextureLayered> VisualShaderNodeCubemap::get_cube_map() const {
	return cube_map;
}

void VisualShaderNodeCubemap::set_texture_type(TextureType p_texture_type) {
	ERR_FAIL_INDEX(int(p_texture_type), int(TYPE_MAX));
	if (texture_type == p_texture_type) {
		return;
	}
	texture_type = p_texture_type;
	emit_changed();
}

VisualShaderNodeCubemap::TextureType VisualShaderNodeCubemap::get_texture_type() const {
	return texture_type;
}

Vector<StringName> VisualShaderNodeCubemap::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("source");
	if (source == SOURCE_TEXTURE) {
		props.push_back("cube_map");
		props.push_back("texture_type");
	}
	return props;
}

String VisualShaderNodeCubemap::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	if (is_input_port_connected(2) && source != SOURCE_PORT) {
		return RTR("The sampler port is connected but not used. Consider changing the source to 'SamplerPort'.");
	}
	return String();
}

void VisualShaderNodeCubemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source", "value"), &VisualShaderNodeCubemap::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &VisualShaderNodeCubemap::get_source);

	ClassDB::bind_method(D_METHOD("set_cube_map", "value"), &VisualShaderNodeCubemap::set_cube_map);
	ClassDB::bind_method(D_METHOD("get_cube_map"), &VisualShaderNodeCubemap::get_cube_map);

	ClassDB::bind_method(D_METHOD("set_texture_type", "value"), &VisualShaderNodeCubemap::set_texture_type);
	ClassDB::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeCubemap::get_texture_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Texture,SamplerPort"), "set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "cube_map", PROPERTY_HINT_RESOURCE_TYPE, "Cubemap,CompressedCubemap,PlaceholderCubemap,TextureCubemapRD"), "set_cube_map", "get_cube_map");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_type", PROPERTY_HINT_ENUM, "Data,Color,Normal Map"), "set_texture_type", "get_texture_type");

	BIND_ENUM_CONSTANT(SOURCE_TEXTURE);
	BIND_ENUM_CONSTANT(SOURCE_PORT);
	BIND_ENUM_CONSTANT(SOURCE_MAX);

	BIND_ENUM_CONSTANT(TYPE_DATA);
	BIND_ENUM_CONSTANT(TYPE_COLOR);
	BIND_ENUM_CONSTANT(TYPE_NORMAL_MAP);
	BIND_ENUM_CONSTANT(TYPE_MAX);
}

VisualShaderNodeCubemap::VisualShaderNodeCubemap() {
	simple_decl = false;
}

////////////// Linear Depth

String VisualShaderNodeLinearSceneDepth::get_caption() const {
	return "LinearSceneDepth";
}

int VisualShaderNodeLinearSceneDepth::get_input_port_count() const {
	return 0;
}

VisualShaderNodeLinearSceneDepth::PortType VisualShaderNodeLinearSceneDepth::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeLinearSceneDepth::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeLinearSceneDepth::get_output_port_count() const {
	return 1;
}

VisualShaderNodeLinearSceneDepth::PortType VisualShaderNodeLinearSceneDepth::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeLinearSceneDepth::get_output_port_name(int p_port) const {
	return "linear depth";
}

bool VisualShaderNodeLinearSceneDepth::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeLinearSceneDepth::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "depth_tex") + " : hint_depth_texture;\n";
}

String VisualShaderNodeLinearSceneDepth::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	{\n";

	code += "		float __log_depth = textureLod(" + make_unique_id(p_type, p_id, "depth_tex") + ", SCREEN_UV, 0.0).x;\n";
	if (!RenderingServer::get_singleton()->is_low_end()) {
		code += "	vec4 __depth_view = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, __log_depth, 1.0);\n";
	} else {
		code += "	vec4 __depth_view = INV_PROJECTION_MATRIX * vec4(vec3(SCREEN_UV, __log_depth) * 2.0 - 1.0, 1.0);\n";
	}
	code += "		__depth_view.xyz /= __depth_view.w;\n";
	code += vformat("		%s = -__depth_view.z;\n", p_output_vars[0]);

	code += "	}\n";
	return code;
}

VisualShaderNodeLinearSceneDepth::VisualShaderNodeLinearSceneDepth() {
	simple_decl = false;
}

////////////// World Position from Depth

String VisualShaderNodeWorldPositionFromDepth::get_caption() const {
	return "WorldPositionFromDepth";
}

int VisualShaderNodeWorldPositionFromDepth::get_input_port_count() const {
	return 1;
}

VisualShaderNodeWorldPositionFromDepth::PortType VisualShaderNodeWorldPositionFromDepth::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_2D;
}

String VisualShaderNodeWorldPositionFromDepth::get_input_port_name(int p_port) const {
	return "screen uv";
}

bool VisualShaderNodeWorldPositionFromDepth::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_port == 0) {
		return true;
	}
	return false;
}

int VisualShaderNodeWorldPositionFromDepth::get_output_port_count() const {
	return 1;
}

VisualShaderNodeWorldPositionFromDepth::PortType VisualShaderNodeWorldPositionFromDepth::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeWorldPositionFromDepth::get_output_port_name(int p_port) const {
	return "world position";
}

bool VisualShaderNodeWorldPositionFromDepth::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeWorldPositionFromDepth::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "depth_tex") + " : hint_depth_texture, repeat_disable, filter_nearest;\n";
}

String VisualShaderNodeWorldPositionFromDepth::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	String uv = p_input_vars[0].is_empty() ? "SCREEN_UV" : p_input_vars[0];
	code += "	{\n";

	code += "		float __log_depth = textureLod(" + make_unique_id(p_type, p_id, "depth_tex") + ", " + uv + ", 0.0).x;\n";
	if (!RenderingServer::get_singleton()->is_low_end()) {
		code += "	vec4 __depth_view = INV_PROJECTION_MATRIX * vec4(" + uv + " * 2.0 - 1.0, __log_depth, 1.0);\n";
	} else {
		code += "	vec4 __depth_view = INV_PROJECTION_MATRIX * vec4(vec3(" + uv + ", __log_depth) * 2.0 - 1.0, 1.0);\n";
	}
	code += "		__depth_view.xyz /= __depth_view.w;\n";
	code += vformat("		%s = (INV_VIEW_MATRIX * __depth_view).xyz;\n", p_output_vars[0]);

	code += "	}\n";
	return code;
}

VisualShaderNodeWorldPositionFromDepth::VisualShaderNodeWorldPositionFromDepth() {
	simple_decl = false;
}

////////////// Unpack Normals in World Space

String VisualShaderNodeScreenNormalWorldSpace::get_caption() const {
	return "ScreenNormalWorldSpace";
}

int VisualShaderNodeScreenNormalWorldSpace::get_input_port_count() const {
	return 1;
}

VisualShaderNodeScreenNormalWorldSpace::PortType VisualShaderNodeScreenNormalWorldSpace::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_2D;
}

String VisualShaderNodeScreenNormalWorldSpace::get_input_port_name(int p_port) const {
	return "screen uv";
}

bool VisualShaderNodeScreenNormalWorldSpace::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_port == 0) {
		return true;
	}
	return false;
}

int VisualShaderNodeScreenNormalWorldSpace::get_output_port_count() const {
	return 1;
}

VisualShaderNodeScreenNormalWorldSpace::PortType VisualShaderNodeScreenNormalWorldSpace::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeScreenNormalWorldSpace::get_output_port_name(int p_port) const {
	return "screen normal";
}

bool VisualShaderNodeScreenNormalWorldSpace::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeScreenNormalWorldSpace::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "normal_rough_tex") + " : hint_normal_roughness_texture, repeat_disable, filter_nearest;\n";
}

String VisualShaderNodeScreenNormalWorldSpace::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	String uv = p_input_vars[0].is_empty() ? "SCREEN_UV" : p_input_vars[0];
	code += "	{\n";

	code += "		vec3 __normals = textureLod(" + make_unique_id(p_type, p_id, "normal_rough_tex") + ", " + uv + ", 0.0).xyz;\n";
	code += "		__normals = __normals * 2.0 - 1.0;\n";
	code += vformat("		%s = mat3(INV_VIEW_MATRIX) * __normals;\n", p_output_vars[0]);

	code += "	}\n";
	return code;
}

VisualShaderNodeScreenNormalWorldSpace::VisualShaderNodeScreenNormalWorldSpace() {
	simple_decl = false;
}

////////////// Float Op

String VisualShaderNodeFloatOp::get_caption() const {
	return "FloatOp";
}

int VisualShaderNodeFloatOp::get_input_port_count() const {
	return 2;
}

VisualShaderNodeFloatOp::PortType VisualShaderNodeFloatOp::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeFloatOp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeFloatOp::PortType VisualShaderNodeFloatOp::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatOp::get_output_port_name(int p_port) const {
	return "op"; //no output port means the editor will be used as port
}

String VisualShaderNodeFloatOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code = "	" + p_output_vars[0] + " = ";
	switch (op) {
		case OP_ADD:
			code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n";
			break;
		case OP_SUB:
			code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n";
			break;
		case OP_MUL:
			code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
			break;
		case OP_DIV:
			code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n";
			break;
		case OP_MOD:
			code += "mod(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_POW:
			code += "pow(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MAX:
			code += "max(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MIN:
			code += "min(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_ATAN2:
			code += "atan(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_STEP:
			code += "step(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		default:
			break;
	}
	return code;
}

void VisualShaderNodeFloatOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), int(OP_ENUM_SIZE));
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeFloatOp::Operator VisualShaderNodeFloatOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeFloatOp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeFloatOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeFloatOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeFloatOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "Add,Subtract,Multiply,Divide,Remainder,Power,Max,Min,ATan2,Step"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_ADD);
	BIND_ENUM_CONSTANT(OP_SUB);
	BIND_ENUM_CONSTANT(OP_MUL);
	BIND_ENUM_CONSTANT(OP_DIV);
	BIND_ENUM_CONSTANT(OP_MOD);
	BIND_ENUM_CONSTANT(OP_POW);
	BIND_ENUM_CONSTANT(OP_MAX);
	BIND_ENUM_CONSTANT(OP_MIN);
	BIND_ENUM_CONSTANT(OP_ATAN2);
	BIND_ENUM_CONSTANT(OP_STEP);
	BIND_ENUM_CONSTANT(OP_ENUM_SIZE);
}

VisualShaderNodeFloatOp::VisualShaderNodeFloatOp() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
}

////////////// Integer Op

String VisualShaderNodeIntOp::get_caption() const {
	return "IntOp";
}

int VisualShaderNodeIntOp::get_input_port_count() const {
	return 2;
}

VisualShaderNodeIntOp::PortType VisualShaderNodeIntOp::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeIntOp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIntOp::PortType VisualShaderNodeIntOp::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntOp::get_output_port_name(int p_port) const {
	return "op"; // No output port means the editor will be used as port.
}

String VisualShaderNodeIntOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code = "	" + p_output_vars[0] + " = ";
	switch (op) {
		case OP_ADD:
			code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n";
			break;
		case OP_SUB:
			code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n";
			break;
		case OP_MUL:
			code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
			break;
		case OP_DIV:
			code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n";
			break;
		case OP_MOD:
			code += p_input_vars[0] + " % " + p_input_vars[1] + ";\n";
			break;
		case OP_MAX:
			code += "max(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MIN:
			code += "min(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_BITWISE_AND:
			code += p_input_vars[0] + " & " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_OR:
			code += p_input_vars[0] + " | " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_XOR:
			code += p_input_vars[0] + " ^ " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_LEFT_SHIFT:
			code += p_input_vars[0] + " << " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_RIGHT_SHIFT:
			code += p_input_vars[0] + " >> " + p_input_vars[1] + ";\n";
			break;
		default:
			break;
	}

	return code;
}

void VisualShaderNodeIntOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), OP_ENUM_SIZE);
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeIntOp::Operator VisualShaderNodeIntOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeIntOp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeIntOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeIntOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeIntOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "Add,Subtract,Multiply,Divide,Remainder,Max,Min,Bitwise AND,Bitwise OR,Bitwise XOR,Bitwise Left Shift,Bitwise Right Shift"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_ADD);
	BIND_ENUM_CONSTANT(OP_SUB);
	BIND_ENUM_CONSTANT(OP_MUL);
	BIND_ENUM_CONSTANT(OP_DIV);
	BIND_ENUM_CONSTANT(OP_MOD);
	BIND_ENUM_CONSTANT(OP_MAX);
	BIND_ENUM_CONSTANT(OP_MIN);
	BIND_ENUM_CONSTANT(OP_BITWISE_AND);
	BIND_ENUM_CONSTANT(OP_BITWISE_OR);
	BIND_ENUM_CONSTANT(OP_BITWISE_XOR);
	BIND_ENUM_CONSTANT(OP_BITWISE_LEFT_SHIFT);
	BIND_ENUM_CONSTANT(OP_BITWISE_RIGHT_SHIFT);
	BIND_ENUM_CONSTANT(OP_ENUM_SIZE);
}

VisualShaderNodeIntOp::VisualShaderNodeIntOp() {
	set_input_port_default_value(0, 0);
	set_input_port_default_value(1, 0);
}

////////////// Unsigned Integer Op

String VisualShaderNodeUIntOp::get_caption() const {
	return "UIntOp";
}

int VisualShaderNodeUIntOp::get_input_port_count() const {
	return 2;
}

VisualShaderNodeUIntOp::PortType VisualShaderNodeUIntOp::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeUIntOp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUIntOp::PortType VisualShaderNodeUIntOp::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntOp::get_output_port_name(int p_port) const {
	return "op"; // No output port means the editor will be used as port.
}

String VisualShaderNodeUIntOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code = "	" + p_output_vars[0] + " = ";
	switch (op) {
		case OP_ADD:
			code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n";
			break;
		case OP_SUB:
			code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n";
			break;
		case OP_MUL:
			code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
			break;
		case OP_DIV:
			code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n";
			break;
		case OP_MOD:
			code += p_input_vars[0] + " % " + p_input_vars[1] + ";\n";
			break;
		case OP_MAX:
			code += "max(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MIN:
			code += "min(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_BITWISE_AND:
			code += p_input_vars[0] + " & " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_OR:
			code += p_input_vars[0] + " | " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_XOR:
			code += p_input_vars[0] + " ^ " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_LEFT_SHIFT:
			code += p_input_vars[0] + " << " + p_input_vars[1] + ";\n";
			break;
		case OP_BITWISE_RIGHT_SHIFT:
			code += p_input_vars[0] + " >> " + p_input_vars[1] + ";\n";
			break;
		default:
			break;
	}

	return code;
}

void VisualShaderNodeUIntOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), OP_ENUM_SIZE);
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeUIntOp::Operator VisualShaderNodeUIntOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeUIntOp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeUIntOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeUIntOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeUIntOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "Add,Subtract,Multiply,Divide,Remainder,Max,Min,Bitwise AND,Bitwise OR,Bitwise XOR,Bitwise Left Shift,Bitwise Right Shift"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_ADD);
	BIND_ENUM_CONSTANT(OP_SUB);
	BIND_ENUM_CONSTANT(OP_MUL);
	BIND_ENUM_CONSTANT(OP_DIV);
	BIND_ENUM_CONSTANT(OP_MOD);
	BIND_ENUM_CONSTANT(OP_MAX);
	BIND_ENUM_CONSTANT(OP_MIN);
	BIND_ENUM_CONSTANT(OP_BITWISE_AND);
	BIND_ENUM_CONSTANT(OP_BITWISE_OR);
	BIND_ENUM_CONSTANT(OP_BITWISE_XOR);
	BIND_ENUM_CONSTANT(OP_BITWISE_LEFT_SHIFT);
	BIND_ENUM_CONSTANT(OP_BITWISE_RIGHT_SHIFT);
	BIND_ENUM_CONSTANT(OP_ENUM_SIZE);
}

VisualShaderNodeUIntOp::VisualShaderNodeUIntOp() {
	set_input_port_default_value(0, 0);
	set_input_port_default_value(1, 0);
}

////////////// Vector Op

String VisualShaderNodeVectorOp::get_caption() const {
	return "VectorOp";
}

int VisualShaderNodeVectorOp::get_input_port_count() const {
	return 2;
}

String VisualShaderNodeVectorOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeVectorOp::get_output_port_count() const {
	return 1;
}

String VisualShaderNodeVectorOp::get_output_port_name(int p_port) const {
	return "op";
}

String VisualShaderNodeVectorOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code = "	" + p_output_vars[0] + " = ";
	switch (op) {
		case OP_ADD:
			code += p_input_vars[0] + " + " + p_input_vars[1] + ";\n";
			break;
		case OP_SUB:
			code += p_input_vars[0] + " - " + p_input_vars[1] + ";\n";
			break;
		case OP_MUL:
			code += p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
			break;
		case OP_DIV:
			code += p_input_vars[0] + " / " + p_input_vars[1] + ";\n";
			break;
		case OP_MOD:
			code += "mod(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_POW:
			code += "pow(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MAX:
			code += "max(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_MIN:
			code += "min(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_CROSS:
			if (op_type == OP_TYPE_VECTOR_2D) { // Not supported.
				code += "vec2(0.0);\n";
			} else if (op_type == OP_TYPE_VECTOR_4D) { // Not supported.
				code += "vec4(0.0);\n";
			} else {
				code += "cross(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			}
			break;
		case OP_ATAN2:
			code += "atan(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_REFLECT:
			code += "reflect(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		case OP_STEP:
			code += "step(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
			break;
		default:
			break;
	}

	return code;
}

void VisualShaderNodeVectorOp::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

void VisualShaderNodeVectorOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), int(OP_ENUM_SIZE));
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeVectorOp::Operator VisualShaderNodeVectorOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeVectorOp::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeVectorBase::get_editable_properties();
	props.push_back("operator");
	return props;
}

String VisualShaderNodeVectorOp::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	bool invalid_type = false;

	if (op_type == OP_TYPE_VECTOR_2D || op_type == OP_TYPE_VECTOR_4D) {
		if (op == OP_CROSS) {
			invalid_type = true;
		}
	}

	if (invalid_type) {
		return RTR("Invalid operator for that type.");
	}

	return String();
}

void VisualShaderNodeVectorOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeVectorOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeVectorOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "Add,Subtract,Multiply,Divide,Remainder,Power,Max,Min,Cross,ATan2,Reflect,Step"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_ADD);
	BIND_ENUM_CONSTANT(OP_SUB);
	BIND_ENUM_CONSTANT(OP_MUL);
	BIND_ENUM_CONSTANT(OP_DIV);
	BIND_ENUM_CONSTANT(OP_MOD);
	BIND_ENUM_CONSTANT(OP_POW);
	BIND_ENUM_CONSTANT(OP_MAX);
	BIND_ENUM_CONSTANT(OP_MIN);
	BIND_ENUM_CONSTANT(OP_CROSS);
	BIND_ENUM_CONSTANT(OP_ATAN2);
	BIND_ENUM_CONSTANT(OP_REFLECT);
	BIND_ENUM_CONSTANT(OP_STEP);
	BIND_ENUM_CONSTANT(OP_ENUM_SIZE);
}

VisualShaderNodeVectorOp::VisualShaderNodeVectorOp() {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2());
			set_input_port_default_value(1, Vector2());
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3());
			set_input_port_default_value(1, Vector3());
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion());
			set_input_port_default_value(1, Quaternion());
		} break;
		default:
			break;
	}
}

////////////// Color Op

String VisualShaderNodeColorOp::get_caption() const {
	return "ColorOp";
}

int VisualShaderNodeColorOp::get_input_port_count() const {
	return 2;
}

VisualShaderNodeColorOp::PortType VisualShaderNodeColorOp::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeColorOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeColorOp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeColorOp::PortType VisualShaderNodeColorOp::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeColorOp::get_output_port_name(int p_port) const {
	return "op"; //no output port means the editor will be used as port
}

String VisualShaderNodeColorOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	static const char *axisn[3] = { "x", "y", "z" };
	switch (op) {
		case OP_SCREEN: {
			code += "	" + p_output_vars[0] + " = vec3(1.0) - (vec3(1.0) - " + p_input_vars[0] + ") * (vec3(1.0) - " + p_input_vars[1] + ");\n";
		} break;
		case OP_DIFFERENCE: {
			code += "	" + p_output_vars[0] + " = abs(" + p_input_vars[0] + " - " + p_input_vars[1] + ");\n";
		} break;
		case OP_DARKEN: {
			code += "	" + p_output_vars[0] + " = min(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
		} break;
		case OP_LIGHTEN: {
			code += "	" + p_output_vars[0] + " = max(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";

		} break;
		case OP_OVERLAY: {
			for (int i = 0; i < 3; i++) {
				code += "	{\n";
				code += "		float base = " + p_input_vars[0] + "." + axisn[i] + ";\n";
				code += "		float blend = " + p_input_vars[1] + "." + axisn[i] + ";\n";
				code += "		if (base < 0.5) {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = 2.0 * base * blend;\n";
				code += "		} else {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = 1.0 - 2.0 * (1.0 - blend) * (1.0 - base);\n";
				code += "		}\n";
				code += "	}\n";
			}

		} break;
		case OP_DODGE: {
			code += "	" + p_output_vars[0] + " = (" + p_input_vars[0] + ") / (vec3(1.0) - " + p_input_vars[1] + ");\n";

		} break;
		case OP_BURN: {
			code += "	" + p_output_vars[0] + " = vec3(1.0) - (vec3(1.0) - " + p_input_vars[0] + ") / (" + p_input_vars[1] + ");\n";
		} break;
		case OP_SOFT_LIGHT: {
			for (int i = 0; i < 3; i++) {
				code += "	{\n";
				code += "		float base = " + p_input_vars[0] + "." + axisn[i] + ";\n";
				code += "		float blend = " + p_input_vars[1] + "." + axisn[i] + ";\n";
				code += "		if (base < 0.5) {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = (base * (blend + 0.5));\n";
				code += "		} else {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = (1.0 - (1.0 - base) * (1.0 - (blend - 0.5)));\n";
				code += "		}\n";
				code += "	}\n";
			}

		} break;
		case OP_HARD_LIGHT: {
			for (int i = 0; i < 3; i++) {
				code += "	{\n";
				code += "		float base = " + p_input_vars[0] + "." + axisn[i] + ";\n";
				code += "		float blend = " + p_input_vars[1] + "." + axisn[i] + ";\n";
				code += "		if (base < 0.5) {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = (base * (2.0 * blend));\n";
				code += "		} else {\n";
				code += "			" + p_output_vars[0] + "." + axisn[i] + " = (1.0 - (1.0 - base) * (1.0 - 2.0 * (blend - 0.5)));\n";
				code += "		}\n";
				code += "	}\n";
			}

		} break;
		default:
			break;
	}

	return code;
}

void VisualShaderNodeColorOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), int(OP_MAX));
	if (op == p_op) {
		return;
	}
	switch (p_op) {
		case OP_SCREEN:
			simple_decl = true;
			break;
		case OP_DIFFERENCE:
			simple_decl = true;
			break;
		case OP_DARKEN:
			simple_decl = true;
			break;
		case OP_LIGHTEN:
			simple_decl = true;
			break;
		case OP_OVERLAY:
			simple_decl = false;
			break;
		case OP_DODGE:
			simple_decl = true;
			break;
		case OP_BURN:
			simple_decl = true;
			break;
		case OP_SOFT_LIGHT:
			simple_decl = false;
			break;
		case OP_HARD_LIGHT:
			simple_decl = false;
			break;
		default:
			break;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeColorOp::Operator VisualShaderNodeColorOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeColorOp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeColorOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeColorOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeColorOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "Screen,Difference,Darken,Lighten,Overlay,Dodge,Burn,Soft Light,Hard Light"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_SCREEN);
	BIND_ENUM_CONSTANT(OP_DIFFERENCE);
	BIND_ENUM_CONSTANT(OP_DARKEN);
	BIND_ENUM_CONSTANT(OP_LIGHTEN);
	BIND_ENUM_CONSTANT(OP_OVERLAY);
	BIND_ENUM_CONSTANT(OP_DODGE);
	BIND_ENUM_CONSTANT(OP_BURN);
	BIND_ENUM_CONSTANT(OP_SOFT_LIGHT);
	BIND_ENUM_CONSTANT(OP_HARD_LIGHT);
	BIND_ENUM_CONSTANT(OP_MAX);
}

VisualShaderNodeColorOp::VisualShaderNodeColorOp() {
	set_input_port_default_value(0, Vector3());
	set_input_port_default_value(1, Vector3());
}

////////////// Transform Op

String VisualShaderNodeTransformOp::get_caption() const {
	return "TransformOp";
}

int VisualShaderNodeTransformOp::get_input_port_count() const {
	return 2;
}

VisualShaderNodeTransformOp::PortType VisualShaderNodeTransformOp::get_input_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformOp::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeTransformOp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformOp::PortType VisualShaderNodeTransformOp::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformOp::get_output_port_name(int p_port) const {
	return "mult"; //no output port means the editor will be used as port
}

String VisualShaderNodeTransformOp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	switch (op) {
		case OP_AxB:
			return "	" + p_output_vars[0] + " = " + p_input_vars[0] + " * " + p_input_vars[1] + ";\n";
		case OP_BxA:
			return "	" + p_output_vars[0] + " = " + p_input_vars[1] + " * " + p_input_vars[0] + ";\n";
		case OP_AxB_COMP:
			return "	" + p_output_vars[0] + " = matrixCompMult(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
		case OP_BxA_COMP:
			return "	" + p_output_vars[0] + " = matrixCompMult(" + p_input_vars[1] + ", " + p_input_vars[0] + ");\n";
		case OP_ADD:
			return "	" + p_output_vars[0] + " = " + p_input_vars[0] + " + " + p_input_vars[1] + ";\n";
		case OP_A_MINUS_B:
			return "	" + p_output_vars[0] + " = " + p_input_vars[0] + " - " + p_input_vars[1] + ";\n";
		case OP_B_MINUS_A:
			return "	" + p_output_vars[0] + " = " + p_input_vars[1] + " - " + p_input_vars[0] + ";\n";
		case OP_A_DIV_B:
			return "	" + p_output_vars[0] + " = " + p_input_vars[0] + " / " + p_input_vars[1] + ";\n";
		case OP_B_DIV_A:
			return "	" + p_output_vars[0] + " = " + p_input_vars[1] + " / " + p_input_vars[0] + ";\n";
		default:
			return "";
	}
}

void VisualShaderNodeTransformOp::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), int(OP_MAX));
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeTransformOp::Operator VisualShaderNodeTransformOp::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeTransformOp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeTransformOp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeTransformOp::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeTransformOp::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "A x B,B x A,A x B(per component),B x A(per component),A + B,A - B,B - A,A / B,B / A"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_AxB);
	BIND_ENUM_CONSTANT(OP_BxA);
	BIND_ENUM_CONSTANT(OP_AxB_COMP);
	BIND_ENUM_CONSTANT(OP_BxA_COMP);
	BIND_ENUM_CONSTANT(OP_ADD);
	BIND_ENUM_CONSTANT(OP_A_MINUS_B);
	BIND_ENUM_CONSTANT(OP_B_MINUS_A);
	BIND_ENUM_CONSTANT(OP_A_DIV_B);
	BIND_ENUM_CONSTANT(OP_B_DIV_A);
	BIND_ENUM_CONSTANT(OP_MAX);
}

VisualShaderNodeTransformOp::VisualShaderNodeTransformOp() {
	set_input_port_default_value(0, Transform3D());
	set_input_port_default_value(1, Transform3D());
}

////////////// TransformVec Mult

String VisualShaderNodeTransformVecMult::get_caption() const {
	return "TransformVectorMult";
}

int VisualShaderNodeTransformVecMult::get_input_port_count() const {
	return 2;
}

VisualShaderNodeTransformVecMult::PortType VisualShaderNodeTransformVecMult::get_input_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_TRANSFORM : PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeTransformVecMult::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeTransformVecMult::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformVecMult::PortType VisualShaderNodeTransformVecMult::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeTransformVecMult::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeTransformVecMult::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	if (op == OP_AxB) {
		return "	" + p_output_vars[0] + " = (" + p_input_vars[0] + " * vec4(" + p_input_vars[1] + ", 1.0)).xyz;\n";
	} else if (op == OP_BxA) {
		return "	" + p_output_vars[0] + " = (vec4(" + p_input_vars[1] + ", 1.0) * " + p_input_vars[0] + ").xyz;\n";
	} else if (op == OP_3x3_AxB) {
		return "	" + p_output_vars[0] + " = (" + p_input_vars[0] + " * vec4(" + p_input_vars[1] + ", 0.0)).xyz;\n";
	} else {
		return "	" + p_output_vars[0] + " = (vec4(" + p_input_vars[1] + ", 0.0) * " + p_input_vars[0] + ").xyz;\n";
	}
}

void VisualShaderNodeTransformVecMult::set_operator(Operator p_op) {
	ERR_FAIL_INDEX(int(p_op), int(OP_MAX));
	if (op == p_op) {
		return;
	}
	op = p_op;
	emit_changed();
}

VisualShaderNodeTransformVecMult::Operator VisualShaderNodeTransformVecMult::get_operator() const {
	return op;
}

Vector<StringName> VisualShaderNodeTransformVecMult::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("operator");
	return props;
}

void VisualShaderNodeTransformVecMult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operator", "op"), &VisualShaderNodeTransformVecMult::set_operator);
	ClassDB::bind_method(D_METHOD("get_operator"), &VisualShaderNodeTransformVecMult::get_operator);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operator", PROPERTY_HINT_ENUM, "A x B,B x A,A x B (3x3),B x A (3x3)"), "set_operator", "get_operator");

	BIND_ENUM_CONSTANT(OP_AxB);
	BIND_ENUM_CONSTANT(OP_BxA);
	BIND_ENUM_CONSTANT(OP_3x3_AxB);
	BIND_ENUM_CONSTANT(OP_3x3_BxA);
	BIND_ENUM_CONSTANT(OP_MAX);
}

VisualShaderNodeTransformVecMult::VisualShaderNodeTransformVecMult() {
	set_input_port_default_value(0, Transform3D());
	set_input_port_default_value(1, Vector3());
}

////////////// Float Func

String VisualShaderNodeFloatFunc::get_caption() const {
	return "FloatFunc";
}

int VisualShaderNodeFloatFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeFloatFunc::PortType VisualShaderNodeFloatFunc::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeFloatFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeFloatFunc::PortType VisualShaderNodeFloatFunc::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatFunc::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeFloatFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"sin($)",
		"cos($)",
		"tan($)",
		"asin($)",
		"acos($)",
		"atan($)",
		"sinh($)",
		"cosh($)",
		"tanh($)",
		"log($)",
		"exp($)",
		"sqrt($)",
		"abs($)",
		"sign($)",
		"floor($)",
		"round($)",
		"ceil($)",
		"fract($)",
		"min(max($, 0.0), 1.0)",
		"-($)",
		"acosh($)",
		"asinh($)",
		"atanh($)",
		"degrees($)",
		"exp2($)",
		"inversesqrt($)",
		"log2($)",
		"radians($)",
		"1.0 / ($)",
		"roundEven($)",
		"trunc($)",
		"1.0 - $"
	};
	return "	" + p_output_vars[0] + " = " + String(functions[func]).replace("$", p_input_vars[0]) + ";\n";
}

void VisualShaderNodeFloatFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeFloatFunc::Function VisualShaderNodeFloatFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeFloatFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeFloatFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeFloatFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeFloatFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Sin,Cos,Tan,ASin,ACos,ATan,SinH,CosH,TanH,Log,Exp,Sqrt,Abs,Sign,Floor,Round,Ceil,Fract,Saturate,Negate,ACosH,ASinH,ATanH,Degrees,Exp2,InverseSqrt,Log2,Radians,Reciprocal,RoundEven,Trunc,OneMinus"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_SIN);
	BIND_ENUM_CONSTANT(FUNC_COS);
	BIND_ENUM_CONSTANT(FUNC_TAN);
	BIND_ENUM_CONSTANT(FUNC_ASIN);
	BIND_ENUM_CONSTANT(FUNC_ACOS);
	BIND_ENUM_CONSTANT(FUNC_ATAN);
	BIND_ENUM_CONSTANT(FUNC_SINH);
	BIND_ENUM_CONSTANT(FUNC_COSH);
	BIND_ENUM_CONSTANT(FUNC_TANH);
	BIND_ENUM_CONSTANT(FUNC_LOG);
	BIND_ENUM_CONSTANT(FUNC_EXP);
	BIND_ENUM_CONSTANT(FUNC_SQRT);
	BIND_ENUM_CONSTANT(FUNC_ABS);
	BIND_ENUM_CONSTANT(FUNC_SIGN);
	BIND_ENUM_CONSTANT(FUNC_FLOOR);
	BIND_ENUM_CONSTANT(FUNC_ROUND);
	BIND_ENUM_CONSTANT(FUNC_CEIL);
	BIND_ENUM_CONSTANT(FUNC_FRACT);
	BIND_ENUM_CONSTANT(FUNC_SATURATE);
	BIND_ENUM_CONSTANT(FUNC_NEGATE);
	BIND_ENUM_CONSTANT(FUNC_ACOSH);
	BIND_ENUM_CONSTANT(FUNC_ASINH);
	BIND_ENUM_CONSTANT(FUNC_ATANH);
	BIND_ENUM_CONSTANT(FUNC_DEGREES);
	BIND_ENUM_CONSTANT(FUNC_EXP2);
	BIND_ENUM_CONSTANT(FUNC_INVERSE_SQRT);
	BIND_ENUM_CONSTANT(FUNC_LOG2);
	BIND_ENUM_CONSTANT(FUNC_RADIANS);
	BIND_ENUM_CONSTANT(FUNC_RECIPROCAL);
	BIND_ENUM_CONSTANT(FUNC_ROUNDEVEN);
	BIND_ENUM_CONSTANT(FUNC_TRUNC);
	BIND_ENUM_CONSTANT(FUNC_ONEMINUS);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeFloatFunc::VisualShaderNodeFloatFunc() {
	set_input_port_default_value(0, 0.0);
}

////////////// Int Func

String VisualShaderNodeIntFunc::get_caption() const {
	return "IntFunc";
}

int VisualShaderNodeIntFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeIntFunc::PortType VisualShaderNodeIntFunc::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeIntFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIntFunc::PortType VisualShaderNodeIntFunc::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntFunc::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeIntFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"abs($)",
		"-($)",
		"sign($)",
		"~($)"
	};

	return "	" + p_output_vars[0] + " = " + String(functions[func]).replace("$", p_input_vars[0]) + ";\n";
}

void VisualShaderNodeIntFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeIntFunc::Function VisualShaderNodeIntFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeIntFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeIntFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeIntFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeIntFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Abs,Negate,Sign,Bitwise NOT"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_ABS);
	BIND_ENUM_CONSTANT(FUNC_NEGATE);
	BIND_ENUM_CONSTANT(FUNC_SIGN);
	BIND_ENUM_CONSTANT(FUNC_BITWISE_NOT);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeIntFunc::VisualShaderNodeIntFunc() {
	set_input_port_default_value(0, 0);
}

////////////// Unsigned Int Func

String VisualShaderNodeUIntFunc::get_caption() const {
	return "UIntFunc";
}

int VisualShaderNodeUIntFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeUIntFunc::PortType VisualShaderNodeUIntFunc::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeUIntFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUIntFunc::PortType VisualShaderNodeUIntFunc::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntFunc::get_output_port_name(int p_port) const {
	return ""; // No output port means the editor will be used as port.
}

String VisualShaderNodeUIntFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"-($)",
		"~($)"
	};

	return "	" + p_output_vars[0] + " = " + String(functions[func]).replace("$", p_input_vars[0]) + ";\n";
}

void VisualShaderNodeUIntFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeUIntFunc::Function VisualShaderNodeUIntFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeUIntFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeUIntFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeUIntFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeUIntFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Negate,Bitwise NOT"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_NEGATE);
	BIND_ENUM_CONSTANT(FUNC_BITWISE_NOT);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeUIntFunc::VisualShaderNodeUIntFunc() {
	set_input_port_default_value(0, 0);
}

////////////// Vector Func

String VisualShaderNodeVectorFunc::get_caption() const {
	return "VectorFunc";
}

int VisualShaderNodeVectorFunc::get_input_port_count() const {
	return 1;
}

String VisualShaderNodeVectorFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeVectorFunc::get_output_port_count() const {
	return 1;
}

String VisualShaderNodeVectorFunc::get_output_port_name(int p_port) const {
	return "result";
}

String VisualShaderNodeVectorFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *funcs[FUNC_MAX] = {
		"normalize($)",
		"", // FUNC_SATURATE
		"-($)",
		"1.0 / ($)",
		"abs($)",
		"acos($)",
		"acosh($)",
		"asin($)",
		"asinh($)",
		"atan($)",
		"atanh($)",
		"ceil($)",
		"cos($)",
		"cosh($)",
		"degrees($)",
		"exp($)",
		"exp2($)",
		"floor($)",
		"fract($)",
		"inversesqrt($)",
		"log($)",
		"log2($)",
		"radians($)",
		"round($)",
		"roundEven($)",
		"sign($)",
		"sin($)",
		"sinh($)",
		"sqrt($)",
		"tan($)",
		"tanh($)",
		"trunc($)",
		"" // FUNC_ONEMINUS
	};

	if (func == FUNC_SATURATE) {
		String code;

		if (op_type == OP_TYPE_VECTOR_2D) {
			code = "max(min($, vec2(1.0)), vec2(0.0))";
		} else if (op_type == OP_TYPE_VECTOR_3D) {
			code = "max(min($, vec3(1.0)), vec3(0.0))";
		} else {
			code = "max(min($, vec4(1.0)), vec4(0.0))";
		}
		return "	" + p_output_vars[0] + " = " + code.replace("$", p_input_vars[0]) + ";\n";
	}

	if (func == FUNC_ONEMINUS) {
		String code;

		if (op_type == OP_TYPE_VECTOR_2D) {
			code = "vec2(1.0) - $";
		} else if (op_type == OP_TYPE_VECTOR_3D) {
			code = "vec3(1.0) - $";
		} else {
			code = "vec4(1.0) - $";
		}
		return "	" + p_output_vars[0] + " = " + code.replace("$", p_input_vars[0]) + ";\n";
	}

	return "	" + p_output_vars[0] + " = " + String(funcs[func]).replace("$", p_input_vars[0]) + ";\n";
}

void VisualShaderNodeVectorFunc::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

void VisualShaderNodeVectorFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeVectorFunc::Function VisualShaderNodeVectorFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeVectorFunc::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeVectorBase::get_editable_properties();
	props.push_back("function");
	return props;
}

void VisualShaderNodeVectorFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeVectorFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeVectorFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Normalize,Saturate,Negate,Reciprocal,Abs,ACos,ACosH,ASin,ASinH,ATan,ATanH,Ceil,Cos,CosH,Degrees,Exp,Exp2,Floor,Fract,InverseSqrt,Log,Log2,Radians,Round,RoundEven,Sign,Sin,SinH,Sqrt,Tan,TanH,Trunc,OneMinus"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_NORMALIZE);
	BIND_ENUM_CONSTANT(FUNC_SATURATE);
	BIND_ENUM_CONSTANT(FUNC_NEGATE);
	BIND_ENUM_CONSTANT(FUNC_RECIPROCAL);
	BIND_ENUM_CONSTANT(FUNC_ABS);
	BIND_ENUM_CONSTANT(FUNC_ACOS);
	BIND_ENUM_CONSTANT(FUNC_ACOSH);
	BIND_ENUM_CONSTANT(FUNC_ASIN);
	BIND_ENUM_CONSTANT(FUNC_ASINH);
	BIND_ENUM_CONSTANT(FUNC_ATAN);
	BIND_ENUM_CONSTANT(FUNC_ATANH);
	BIND_ENUM_CONSTANT(FUNC_CEIL);
	BIND_ENUM_CONSTANT(FUNC_COS);
	BIND_ENUM_CONSTANT(FUNC_COSH);
	BIND_ENUM_CONSTANT(FUNC_DEGREES);
	BIND_ENUM_CONSTANT(FUNC_EXP);
	BIND_ENUM_CONSTANT(FUNC_EXP2);
	BIND_ENUM_CONSTANT(FUNC_FLOOR);
	BIND_ENUM_CONSTANT(FUNC_FRACT);
	BIND_ENUM_CONSTANT(FUNC_INVERSE_SQRT);
	BIND_ENUM_CONSTANT(FUNC_LOG);
	BIND_ENUM_CONSTANT(FUNC_LOG2);
	BIND_ENUM_CONSTANT(FUNC_RADIANS);
	BIND_ENUM_CONSTANT(FUNC_ROUND);
	BIND_ENUM_CONSTANT(FUNC_ROUNDEVEN);
	BIND_ENUM_CONSTANT(FUNC_SIGN);
	BIND_ENUM_CONSTANT(FUNC_SIN);
	BIND_ENUM_CONSTANT(FUNC_SINH);
	BIND_ENUM_CONSTANT(FUNC_SQRT);
	BIND_ENUM_CONSTANT(FUNC_TAN);
	BIND_ENUM_CONSTANT(FUNC_TANH);
	BIND_ENUM_CONSTANT(FUNC_TRUNC);
	BIND_ENUM_CONSTANT(FUNC_ONEMINUS);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeVectorFunc::VisualShaderNodeVectorFunc() {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2());
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3());
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion());
		} break;
		default:
			break;
	}
}

////////////// ColorFunc

String VisualShaderNodeColorFunc::get_caption() const {
	return "ColorFunc";
}

int VisualShaderNodeColorFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeColorFunc::PortType VisualShaderNodeColorFunc::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeColorFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeColorFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeColorFunc::PortType VisualShaderNodeColorFunc::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeColorFunc::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeColorFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;

	switch (func) {
		case FUNC_GRAYSCALE:
			code += "	{\n";
			code += "		vec3 c = " + p_input_vars[0] + ";\n";
			code += "		float max1 = max(c.r, c.g);\n";
			code += "		float max2 = max(max1, c.b);\n";
			code += "		" + p_output_vars[0] + " = vec3(max2, max2, max2);\n";
			code += "	}\n";
			break;
		case FUNC_HSV2RGB:
			code += "	{\n";
			code += "		vec3 c = " + p_input_vars[0] + ";\n";
			code += "		vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n";
			code += "		vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n";
			code += "		" + p_output_vars[0] + " = c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n";
			code += "	}\n";
			break;
		case FUNC_RGB2HSV:
			code += "	{\n";
			code += "		vec3 c = " + p_input_vars[0] + ";\n";
			code += "		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n";
			code += "		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n";
			code += "		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n";
			code += "		float d = q.x - min(q.w, q.y);\n";
			code += "		float e = 1.0e-10;\n";
			code += "		" + p_output_vars[0] + " = vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n";
			code += "	}\n";
			break;
		case FUNC_SEPIA:
			code += "	{\n";
			code += "		vec3 c = " + p_input_vars[0] + ";\n";
			code += "		float r = (c.r * .393) + (c.g *.769) + (c.b * .189);\n";
			code += "		float g = (c.r * .349) + (c.g *.686) + (c.b * .168);\n";
			code += "		float b = (c.r * .272) + (c.g *.534) + (c.b * .131);\n";
			code += "		" + p_output_vars[0] + " = vec3(r, g, b);\n";
			code += "	}\n";
			break;
		case FUNC_LINEAR_TO_SRGB:
			code += "	{\n";
			if (RenderingServer::get_singleton()->is_low_end()) {
				code += "		vec3 c = " + p_input_vars[0] + ";\n";
				code += "		" + p_output_vars[0] + " = max(vec3(1.055) * pow(c, vec3(0.416666667)) - vec3(0.055), vec3(0.0));\n";
			} else {
				code += "		vec3 c = clamp(" + p_input_vars[0] + ", vec3(0.0), vec3(1.0));\n";
				code += "		const vec3 a = vec3(0.055f);\n";
				code += "		" + p_output_vars[0] + " = mix((vec3(1.0f) + a) * pow(c.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * c.rgb, lessThan(c.rgb, vec3(0.0031308f)));\n";
			}
			code += "	}\n";
			break;
		case FUNC_SRGB_TO_LINEAR:
			code += "	{\n";
			if (RenderingServer::get_singleton()->is_low_end()) {
				code += "		vec3 c = " + p_input_vars[0] + ";\n";
				code += "		" + p_output_vars[0] + " = c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878);\n";
			} else {
				code += "		vec3 c = " + p_input_vars[0] + ";\n";
				code += "		" + p_output_vars[0] + " = mix(pow((c.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), c.rgb * (1.0 / 12.92), lessThan(c.rgb, vec3(0.04045)));\n";
			}
			code += "	}\n";
			break;
		default:
			break;
	}

	return code;
}

void VisualShaderNodeColorFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeColorFunc::Function VisualShaderNodeColorFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeColorFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeColorFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeColorFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeColorFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Grayscale,HSV2RGB,RGB2HSV,Sepia,LinearToSRGB,SRGBToLinear"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_GRAYSCALE);
	BIND_ENUM_CONSTANT(FUNC_HSV2RGB);
	BIND_ENUM_CONSTANT(FUNC_RGB2HSV);
	BIND_ENUM_CONSTANT(FUNC_SEPIA);
	BIND_ENUM_CONSTANT(FUNC_LINEAR_TO_SRGB);
	BIND_ENUM_CONSTANT(FUNC_SRGB_TO_LINEAR);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeColorFunc::VisualShaderNodeColorFunc() {
	simple_decl = false;
	set_input_port_default_value(0, Vector3());
}

////////////// Transform Func

String VisualShaderNodeTransformFunc::get_caption() const {
	return "TransformFunc";
}

int VisualShaderNodeTransformFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeTransformFunc::PortType VisualShaderNodeTransformFunc::get_input_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformFunc::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeTransformFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformFunc::PortType VisualShaderNodeTransformFunc::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformFunc::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeTransformFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"inverse($)",
		"transpose($)"
	};

	String code;
	code += "	" + p_output_vars[0] + " = " + String(functions[func]).replace("$", p_input_vars[0]) + ";\n";
	return code;
}

void VisualShaderNodeTransformFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeTransformFunc::Function VisualShaderNodeTransformFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeTransformFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeTransformFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeTransformFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeTransformFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Inverse,Transpose"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_INVERSE);
	BIND_ENUM_CONSTANT(FUNC_TRANSPOSE);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeTransformFunc::VisualShaderNodeTransformFunc() {
	set_input_port_default_value(0, Transform3D());
}

////////////// UV Func

String VisualShaderNodeUVFunc::get_caption() const {
	return "UVFunc";
}

int VisualShaderNodeUVFunc::get_input_port_count() const {
	return 3;
}

VisualShaderNodeUVFunc::PortType VisualShaderNodeUVFunc::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_2D; // uv
		case 1:
			return PORT_TYPE_VECTOR_2D; // scale
		case 2:
			return PORT_TYPE_VECTOR_2D; // offset & pivot
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeUVFunc::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "uv";
		case 1:
			return "scale";
		case 2:
			switch (func) {
				case FUNC_PANNING:
					return "offset";
				case FUNC_SCALING:
					return "pivot";
				default:
					break;
			}
			break;
		default:
			break;
	}
	return "";
}

bool VisualShaderNodeUVFunc::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (p_port == 0) {
			return true;
		}
	}
	return false;
}

int VisualShaderNodeUVFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUVFunc::PortType VisualShaderNodeUVFunc::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeUVFunc::get_output_port_name(int p_port) const {
	return "uv";
}

bool VisualShaderNodeUVFunc::is_show_prop_names() const {
	return true;
}

String VisualShaderNodeUVFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;

	String uv;
	if (p_input_vars[0].is_empty()) {
		if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
			uv = "UV";
		} else {
			uv = "vec2(0.0)";
		}
	} else {
		uv = vformat("%s", p_input_vars[0]);
	}
	String scale = vformat("%s", p_input_vars[1]);
	String offset_pivot = vformat("%s", p_input_vars[2]);

	switch (func) {
		case FUNC_PANNING: {
			code += vformat("	%s = %s * %s + %s;\n", p_output_vars[0], offset_pivot, scale, uv);
		} break;
		case FUNC_SCALING: {
			code += vformat("	%s = (%s - %s) * %s + %s;\n", p_output_vars[0], uv, offset_pivot, scale, offset_pivot);
		} break;
		default:
			break;
	}
	return code;
}

void VisualShaderNodeUVFunc::set_function(VisualShaderNodeUVFunc::Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	if (p_func == FUNC_PANNING) {
		set_input_port_default_value(2, Vector2(), get_input_port_default_value(2)); // offset
	} else { // FUNC_SCALING
		set_input_port_default_value(2, Vector2(0.5, 0.5), get_input_port_default_value(2)); // pivot
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeUVFunc::Function VisualShaderNodeUVFunc::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeUVFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeUVFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeUVFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeUVFunc::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Panning,Scaling"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_PANNING);
	BIND_ENUM_CONSTANT(FUNC_SCALING);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeUVFunc::VisualShaderNodeUVFunc() {
	set_input_port_default_value(1, Vector2(1.0, 1.0)); // scale
	set_input_port_default_value(2, Vector2()); // offset
}

////////////// UV PolarCoord

String VisualShaderNodeUVPolarCoord::get_caption() const {
	return "UVPolarCoord";
}

int VisualShaderNodeUVPolarCoord::get_input_port_count() const {
	return 4;
}

VisualShaderNodeUVPolarCoord::PortType VisualShaderNodeUVPolarCoord::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_2D; // uv
		case 1:
			return PORT_TYPE_VECTOR_2D; // center
		case 2:
			return PORT_TYPE_SCALAR; // zoom
		case 3:
			return PORT_TYPE_SCALAR; // repeat
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeUVPolarCoord::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "uv";
		case 1:
			return "scale";
		case 2:
			return "zoom strength";
		case 3:
			return "repeat";
		default:
			break;
	}
	return "";
}

bool VisualShaderNodeUVPolarCoord::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
		if (p_port == 0) {
			return true;
		}
	}
	return false;
}

int VisualShaderNodeUVPolarCoord::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUVPolarCoord::PortType VisualShaderNodeUVPolarCoord::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeUVPolarCoord::get_output_port_name(int p_port) const {
	return "uv";
}

String VisualShaderNodeUVPolarCoord::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	{\n";

	String uv;
	if (p_input_vars[0].is_empty()) {
		if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
			uv = "UV";
		} else {
			uv = "vec2(0.0)";
		}
	} else {
		uv = vformat("%s", p_input_vars[0]);
	}
	String center = vformat("%s", p_input_vars[1]);
	String zoom = vformat("%s", p_input_vars[2]);
	String repeat = vformat("%s", p_input_vars[3]);

	if (p_mode == Shader::MODE_CANVAS_ITEM) {
		code += vformat("		vec2 __dir = %s - %s;\n", uv, center);
		code += "		float __radius = length(__dir) * 2.0;\n";
		code += "		float __angle = atan(__dir.y, __dir.x) * 1.0 / (PI * 2.0);\n";
		code += vformat("		%s = mod(vec2(__radius * %s, __angle * %s), 1.0);\n", p_output_vars[0], zoom, repeat);
	} else {
		code += vformat("		vec2 __dir = %s - %s;\n", uv, center);
		code += "		float __radius = length(__dir) * 2.0;\n";
		code += "		float __angle = atan(__dir.y, __dir.x) * 1.0 / (PI * 2.0);\n";
		code += vformat("		%s = vec2(__radius * %s, __angle * %s);\n", p_output_vars[0], zoom, repeat);
	}

	code += "	}\n";
	return code;
}

VisualShaderNodeUVPolarCoord::VisualShaderNodeUVPolarCoord() {
	set_input_port_default_value(1, Vector2(0.5, 0.5)); // center
	set_input_port_default_value(2, 1.0); // zoom
	set_input_port_default_value(3, 1.0); // repeat

	simple_decl = false;
}

////////////// Dot Product

String VisualShaderNodeDotProduct::get_caption() const {
	return "DotProduct";
}

int VisualShaderNodeDotProduct::get_input_port_count() const {
	return 2;
}

VisualShaderNodeDotProduct::PortType VisualShaderNodeDotProduct::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeDotProduct::get_input_port_name(int p_port) const {
	return p_port == 0 ? "a" : "b";
}

int VisualShaderNodeDotProduct::get_output_port_count() const {
	return 1;
}

VisualShaderNodeDotProduct::PortType VisualShaderNodeDotProduct::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDotProduct::get_output_port_name(int p_port) const {
	return "dot";
}

String VisualShaderNodeDotProduct::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = dot(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
}

VisualShaderNodeDotProduct::VisualShaderNodeDotProduct() {
	set_input_port_default_value(0, Vector3());
	set_input_port_default_value(1, Vector3());
}

////////////// Vector Len

String VisualShaderNodeVectorLen::get_caption() const {
	return "VectorLen";
}

int VisualShaderNodeVectorLen::get_input_port_count() const {
	return 1;
}

String VisualShaderNodeVectorLen::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeVectorLen::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVectorLen::PortType VisualShaderNodeVectorLen::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVectorLen::get_output_port_name(int p_port) const {
	return "length";
}

void VisualShaderNodeVectorLen::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

String VisualShaderNodeVectorLen::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = length(" + p_input_vars[0] + ");\n";
}

VisualShaderNodeVectorLen::VisualShaderNodeVectorLen() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
}

////////////// Determinant

String VisualShaderNodeDeterminant::get_caption() const {
	return "Determinant";
}

int VisualShaderNodeDeterminant::get_input_port_count() const {
	return 1;
}

VisualShaderNodeDeterminant::PortType VisualShaderNodeDeterminant::get_input_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeDeterminant::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeDeterminant::get_output_port_count() const {
	return 1;
}

VisualShaderNodeDeterminant::PortType VisualShaderNodeDeterminant::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDeterminant::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeDeterminant::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = determinant(" + p_input_vars[0] + ");\n";
}

VisualShaderNodeDeterminant::VisualShaderNodeDeterminant() {
	set_input_port_default_value(0, Transform3D());
}

////////////// Derivative Function

String VisualShaderNodeDerivativeFunc::get_caption() const {
	return "DerivativeFunc";
}

int VisualShaderNodeDerivativeFunc::get_input_port_count() const {
	return 1;
}

VisualShaderNodeDerivativeFunc::PortType VisualShaderNodeDerivativeFunc::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDerivativeFunc::get_input_port_name(int p_port) const {
	return "p";
}

int VisualShaderNodeDerivativeFunc::get_output_port_count() const {
	return 1;
}

VisualShaderNodeDerivativeFunc::PortType VisualShaderNodeDerivativeFunc::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDerivativeFunc::get_output_port_name(int p_port) const {
	return "result";
}

String VisualShaderNodeDerivativeFunc::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"fwidth$($)",
		"dFdx$($)",
		"dFdy$($)"
	};

	static const char *precisions[PRECISION_MAX] = {
		"",
		"Coarse",
		"Fine"
	};

	String code;
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		code += "	" + p_output_vars[0] + " = " + String(functions[func]).replace_first("$", "").replace_first("$", p_input_vars[0]) + ";\n";
		return code;
	}

	code += "	" + p_output_vars[0] + " = " + String(functions[func]).replace_first("$", String(precisions[precision])).replace_first("$", p_input_vars[0]) + ";\n";
	return code;
}

String VisualShaderNodeDerivativeFunc::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	if (precision != PRECISION_NONE && OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		String precision_str;
		switch (precision) {
			case PRECISION_COARSE: {
				precision_str = "Coarse";
			} break;
			case PRECISION_FINE: {
				precision_str = "Fine";
			} break;
			default: {
			} break;
		}

		return vformat(RTR("`%s` precision mode is not available for `gl_compatibility` profile.\nReverted to `None` precision."), precision_str);
	}

	return String();
}

void VisualShaderNodeDerivativeFunc::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX((int)p_op_type, int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeDerivativeFunc::OpType VisualShaderNodeDerivativeFunc::get_op_type() const {
	return op_type;
}

void VisualShaderNodeDerivativeFunc::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeDerivativeFunc::Function VisualShaderNodeDerivativeFunc::get_function() const {
	return func;
}

void VisualShaderNodeDerivativeFunc::set_precision(Precision p_precision) {
	ERR_FAIL_INDEX(int(p_precision), int(PRECISION_MAX));
	if (precision == p_precision) {
		return;
	}
	precision = p_precision;
	emit_changed();
}

VisualShaderNodeDerivativeFunc::Precision VisualShaderNodeDerivativeFunc::get_precision() const {
	return precision;
}

Vector<StringName> VisualShaderNodeDerivativeFunc::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	props.push_back("function");
	props.push_back("precision");
	return props;
}

void VisualShaderNodeDerivativeFunc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "type"), &VisualShaderNodeDerivativeFunc::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeDerivativeFunc::get_op_type);

	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeDerivativeFunc::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeDerivativeFunc::get_function);

	ClassDB::bind_method(D_METHOD("set_precision", "precision"), &VisualShaderNodeDerivativeFunc::set_precision);
	ClassDB::bind_method(D_METHOD("get_precision"), &VisualShaderNodeDerivativeFunc::get_precision);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector3,Vector4"), "set_op_type", "get_op_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Sum,X,Y"), "set_function", "get_function");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "precision", PROPERTY_HINT_ENUM, "None,Coarse,Fine"), "set_precision", "get_precision");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);

	BIND_ENUM_CONSTANT(FUNC_SUM);
	BIND_ENUM_CONSTANT(FUNC_X);
	BIND_ENUM_CONSTANT(FUNC_Y);
	BIND_ENUM_CONSTANT(FUNC_MAX);

	BIND_ENUM_CONSTANT(PRECISION_NONE);
	BIND_ENUM_CONSTANT(PRECISION_COARSE);
	BIND_ENUM_CONSTANT(PRECISION_FINE);
	BIND_ENUM_CONSTANT(PRECISION_MAX);
}

VisualShaderNodeDerivativeFunc::VisualShaderNodeDerivativeFunc() {
	set_input_port_default_value(0, 0.0);
}

////////////// Clamp

String VisualShaderNodeClamp::get_caption() const {
	return "Clamp";
}

int VisualShaderNodeClamp::get_input_port_count() const {
	return 3;
}

VisualShaderNodeClamp::PortType VisualShaderNodeClamp::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_INT:
			return PORT_TYPE_SCALAR_INT;
		case OP_TYPE_UINT:
			return PORT_TYPE_SCALAR_UINT;
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeClamp::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "";
	} else if (p_port == 1) {
		return "min";
	} else if (p_port == 2) {
		return "max";
	}
	return "";
}

int VisualShaderNodeClamp::get_output_port_count() const {
	return 1;
}

VisualShaderNodeClamp::PortType VisualShaderNodeClamp::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_INT:
			return PORT_TYPE_SCALAR_INT;
		case OP_TYPE_UINT:
			return PORT_TYPE_SCALAR_UINT;
		case OP_TYPE_VECTOR_2D:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeClamp::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeClamp::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = clamp(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

void VisualShaderNodeClamp::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX((int)p_op_type, int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_FLOAT:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2));
			break;
		case OP_TYPE_UINT:
		case OP_TYPE_INT:
			set_input_port_default_value(0, 0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0, get_input_port_default_value(1));
			set_input_port_default_value(2, 0, get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_2D:
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_3D:
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_4D:
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
			set_input_port_default_value(2, Quaternion(), get_input_port_default_value(2));
			break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeClamp::OpType VisualShaderNodeClamp::get_op_type() const {
	return op_type;
}

Vector<StringName> VisualShaderNodeClamp::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeClamp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "op_type"), &VisualShaderNodeClamp::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeClamp::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Float,Int,UInt,Vector2,Vector3,Vector4"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_FLOAT);
	BIND_ENUM_CONSTANT(OP_TYPE_INT);
	BIND_ENUM_CONSTANT(OP_TYPE_UINT);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeClamp::VisualShaderNodeClamp() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, 1.0);
}

////////////// FaceForward

String VisualShaderNodeFaceForward::get_caption() const {
	return "FaceForward";
}

int VisualShaderNodeFaceForward::get_input_port_count() const {
	return 3;
}

String VisualShaderNodeFaceForward::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "N";
		case 1:
			return "I";
		case 2:
			return "Nref";
		default:
			return "";
	}
}

int VisualShaderNodeFaceForward::get_output_port_count() const {
	return 1;
}

String VisualShaderNodeFaceForward::get_output_port_name(int p_port) const {
	return "";
}

void VisualShaderNodeFaceForward::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
			set_input_port_default_value(2, Quaternion(), get_input_port_default_value(2));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

String VisualShaderNodeFaceForward::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = faceforward(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

VisualShaderNodeFaceForward::VisualShaderNodeFaceForward() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0));
}

////////////// Outer Product

String VisualShaderNodeOuterProduct::get_caption() const {
	return "OuterProduct";
}

int VisualShaderNodeOuterProduct::get_input_port_count() const {
	return 2;
}

VisualShaderNodeOuterProduct::PortType VisualShaderNodeOuterProduct::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeOuterProduct::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "c";
		case 1:
			return "r";
		default:
			return "";
	}
}

int VisualShaderNodeOuterProduct::get_output_port_count() const {
	return 1;
}

VisualShaderNodeOuterProduct::PortType VisualShaderNodeOuterProduct::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeOuterProduct::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeOuterProduct::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = outerProduct(vec4(" + p_input_vars[0] + ", 0.0), vec4(" + p_input_vars[1] + ", 0.0));\n";
}

VisualShaderNodeOuterProduct::VisualShaderNodeOuterProduct() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
}

////////////// Step

String VisualShaderNodeStep::get_caption() const {
	return "Step";
}

int VisualShaderNodeStep::get_input_port_count() const {
	return 2;
}

VisualShaderNodeStep::PortType VisualShaderNodeStep::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_2D_SCALAR:
			if (p_port == 1) {
				return PORT_TYPE_VECTOR_2D;
			}
			break;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_3D_SCALAR:
			if (p_port == 1) {
				return PORT_TYPE_VECTOR_3D;
			}
			break;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		case OP_TYPE_VECTOR_4D_SCALAR:
			if (p_port == 1) {
				return PORT_TYPE_VECTOR_4D;
			}
			break;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeStep::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "edge";
		case 1:
			return "x";
	}
	return String();
}

int VisualShaderNodeStep::get_default_input_port(PortType p_type) const {
	return 1;
}

int VisualShaderNodeStep::get_output_port_count() const {
	return 1;
}

VisualShaderNodeStep::PortType VisualShaderNodeStep::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
		case OP_TYPE_VECTOR_2D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
		case OP_TYPE_VECTOR_3D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
		case OP_TYPE_VECTOR_4D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeStep::get_output_port_name(int p_port) const {
	return "";
}

void VisualShaderNodeStep::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_2D_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_3D_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_4D_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeStep::OpType VisualShaderNodeStep::get_op_type() const {
	return op_type;
}

String VisualShaderNodeStep::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = step(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
}

Vector<StringName> VisualShaderNodeStep::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeStep::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "op_type"), &VisualShaderNodeStep::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeStep::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector2Scalar,Vector3,Vector3Scalar,Vector4,Vector4Scalar"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeStep::VisualShaderNodeStep() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
}

////////////// SmoothStep

String VisualShaderNodeSmoothStep::get_caption() const {
	return "SmoothStep";
}

int VisualShaderNodeSmoothStep::get_input_port_count() const {
	return 3;
}

VisualShaderNodeSmoothStep::PortType VisualShaderNodeSmoothStep::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_2D_SCALAR:
			if (p_port == 2) {
				return PORT_TYPE_VECTOR_2D; // x
			}
			break;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_3D_SCALAR:
			if (p_port == 2) {
				return PORT_TYPE_VECTOR_3D; // x
			}
			break;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		case OP_TYPE_VECTOR_4D_SCALAR:
			if (p_port == 2) {
				return PORT_TYPE_VECTOR_4D; // x
			}
			break;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeSmoothStep::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "edge0";
		case 1:
			return "edge1";
		case 2:
			return "x";
	}
	return String();
}

int VisualShaderNodeSmoothStep::get_default_input_port(PortType p_type) const {
	return 2;
}

int VisualShaderNodeSmoothStep::get_output_port_count() const {
	return 1;
}

VisualShaderNodeSmoothStep::PortType VisualShaderNodeSmoothStep::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
		case OP_TYPE_VECTOR_2D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
		case OP_TYPE_VECTOR_3D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
		case OP_TYPE_VECTOR_4D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeSmoothStep::get_output_port_name(int p_port) const {
	return "";
}

void VisualShaderNodeSmoothStep::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_2D:
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_2D_SCALAR:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_3D:
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_3D_SCALAR:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_4D:
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Quaternion(), get_input_port_default_value(2)); // x
			break;
		case OP_TYPE_VECTOR_4D_SCALAR:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0)); // edge0
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1)); // edge1
			set_input_port_default_value(2, Quaternion(), get_input_port_default_value(2)); // x
			break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeSmoothStep::OpType VisualShaderNodeSmoothStep::get_op_type() const {
	return op_type;
}

String VisualShaderNodeSmoothStep::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = smoothstep(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

Vector<StringName> VisualShaderNodeSmoothStep::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeSmoothStep::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "op_type"), &VisualShaderNodeSmoothStep::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeSmoothStep::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector2Scalar,Vector3,Vector3Scalar,Vector4,Vector4Scalar"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeSmoothStep::VisualShaderNodeSmoothStep() {
	set_input_port_default_value(0, 0.0); // edge0
	set_input_port_default_value(1, 1.0); // edge1
	set_input_port_default_value(2, 0.5); // x
}

////////////// Distance

String VisualShaderNodeVectorDistance::get_caption() const {
	return "Distance";
}

int VisualShaderNodeVectorDistance::get_input_port_count() const {
	return 2;
}

String VisualShaderNodeVectorDistance::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "a";
		case 1:
			return "b";
	}
	return String();
}

int VisualShaderNodeVectorDistance::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVectorDistance::PortType VisualShaderNodeVectorDistance::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVectorDistance::get_output_port_name(int p_port) const {
	return "";
}

void VisualShaderNodeVectorDistance::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1)); // b
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1)); // b
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1)); // b
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

String VisualShaderNodeVectorDistance::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = distance(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
}

VisualShaderNodeVectorDistance::VisualShaderNodeVectorDistance() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0)); // a
	set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0)); // b
}

////////////// Refract Vector

String VisualShaderNodeVectorRefract::get_caption() const {
	return "Refract";
}

int VisualShaderNodeVectorRefract::get_input_port_count() const {
	return 3;
}

String VisualShaderNodeVectorRefract::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "I";
		case 1:
			return "N";
		case 2:
			return "eta";
	}
	return String();
}

int VisualShaderNodeVectorRefract::get_output_port_count() const {
	return 1;
}

String VisualShaderNodeVectorRefract::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeVectorRefract::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = refract(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

void VisualShaderNodeVectorRefract::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeVectorRefract::VisualShaderNodeVectorRefract() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(1, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(2, 0.0);
}

////////////// Mix

String VisualShaderNodeMix::get_caption() const {
	return "Mix";
}

int VisualShaderNodeMix::get_input_port_count() const {
	return 3;
}

VisualShaderNodeMix::PortType VisualShaderNodeMix::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_2D_SCALAR:
			if (p_port == 2) {
				break;
			}
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_3D_SCALAR:
			if (p_port == 2) {
				break;
			}
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		case OP_TYPE_VECTOR_4D_SCALAR:
			if (p_port == 2) {
				break;
			}
			return PORT_TYPE_VECTOR_4D;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeMix::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "a";
	} else if (p_port == 1) {
		return "b";
	} else {
		return "weight";
	}
}

int VisualShaderNodeMix::get_output_port_count() const {
	return 1;
}

VisualShaderNodeMix::PortType VisualShaderNodeMix::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
		case OP_TYPE_VECTOR_2D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
		case OP_TYPE_VECTOR_3D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
		case OP_TYPE_VECTOR_4D_SCALAR:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeMix::get_output_port_name(int p_port) const {
	return "mix";
}

void VisualShaderNodeMix::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0)); // a
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1)); // b
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_2D_SCALAR: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_3D_SCALAR: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, Quaternion(), get_input_port_default_value(2)); // weight
		} break;
		case OP_TYPE_VECTOR_4D_SCALAR: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0)); // a
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1)); // b
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2)); // weight
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeMix::OpType VisualShaderNodeMix::get_op_type() const {
	return op_type;
}

String VisualShaderNodeMix::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = mix(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

Vector<StringName> VisualShaderNodeMix::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeMix::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "op_type"), &VisualShaderNodeMix::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeMix::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector2Scalar,Vector3,Vector3Scalar,Vector4,Vector4Scalar"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeMix::VisualShaderNodeMix() {
	set_input_port_default_value(0, 0.0); // a
	set_input_port_default_value(1, 1.0); // b
	set_input_port_default_value(2, 0.5); // weight
}

////////////// Vector Compose

String VisualShaderNodeVectorCompose::get_caption() const {
	return "VectorCompose";
}

int VisualShaderNodeVectorCompose::get_input_port_count() const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return 2;
		case OP_TYPE_VECTOR_3D:
			return 3;
		case OP_TYPE_VECTOR_4D:
			return 4;
		default:
			break;
	}
	return 0;
}

VisualShaderNodeVectorCompose::PortType VisualShaderNodeVectorCompose::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVectorCompose::get_input_port_name(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
			}
		} break;
		case OP_TYPE_VECTOR_3D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
				case 2:
					return "z";
			}
		} break;
		case OP_TYPE_VECTOR_4D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
				case 2:
					return "z";
				case 3:
					return "w";
			}
		} break;
		default:
			break;
	}
	return String();
}

int VisualShaderNodeVectorCompose::get_output_port_count() const {
	return 1;
}

String VisualShaderNodeVectorCompose::get_output_port_name(int p_port) const {
	return "vec";
}

void VisualShaderNodeVectorCompose::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			float p1 = get_input_port_default_value(0);
			float p2 = get_input_port_default_value(1);

			set_input_port_default_value(0, p1);
			set_input_port_default_value(1, p2);
		} break;
		case OP_TYPE_VECTOR_3D: {
			float p1 = get_input_port_default_value(0);
			float p2 = get_input_port_default_value(1);
			float p3 = get_input_port_default_value(2);

			set_input_port_default_value(0, p1);
			set_input_port_default_value(1, p2);
			set_input_port_default_value(2, p3);
		} break;
		case OP_TYPE_VECTOR_4D: {
			float p1 = get_input_port_default_value(0);
			float p2 = get_input_port_default_value(1);
			float p3 = get_input_port_default_value(2);
			float p4 = get_input_port_default_value(3);

			set_input_port_default_value(0, p1);
			set_input_port_default_value(1, p2);
			set_input_port_default_value(2, p3);
			set_input_port_default_value(3, p4);
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

String VisualShaderNodeVectorCompose::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			code += "	" + p_output_vars[0] + " = vec2(" + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
		} break;
		case OP_TYPE_VECTOR_3D: {
			code += "	" + p_output_vars[0] + " = vec3(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
		} break;
		case OP_TYPE_VECTOR_4D: {
			code += "	" + p_output_vars[0] + " = vec4(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ", " + p_input_vars[3] + ");\n";
		} break;
		default:
			break;
	}
	return code;
}

VisualShaderNodeVectorCompose::VisualShaderNodeVectorCompose() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, 0.0);
}

////////////// Transform Compose

String VisualShaderNodeTransformCompose::get_caption() const {
	return "TransformCompose";
}

int VisualShaderNodeTransformCompose::get_input_port_count() const {
	return 4;
}

VisualShaderNodeTransformCompose::PortType VisualShaderNodeTransformCompose::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeTransformCompose::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "x";
	} else if (p_port == 1) {
		return "y";
	} else if (p_port == 2) {
		return "z";
	} else {
		return "origin";
	}
}

int VisualShaderNodeTransformCompose::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformCompose::PortType VisualShaderNodeTransformCompose::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformCompose::get_output_port_name(int p_port) const {
	return "xform";
}

String VisualShaderNodeTransformCompose::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = mat4(vec4(" + p_input_vars[0] + ", 0.0), vec4(" + p_input_vars[1] + ", 0.0), vec4(" + p_input_vars[2] + ", 0.0), vec4(" + p_input_vars[3] + ", 1.0));\n";
}

VisualShaderNodeTransformCompose::VisualShaderNodeTransformCompose() {
	set_input_port_default_value(0, Vector3());
	set_input_port_default_value(1, Vector3());
	set_input_port_default_value(2, Vector3());
	set_input_port_default_value(3, Vector3());
}

////////////// Vector Decompose
String VisualShaderNodeVectorDecompose::get_caption() const {
	return "VectorDecompose";
}

int VisualShaderNodeVectorDecompose::get_input_port_count() const {
	return 1;
}

String VisualShaderNodeVectorDecompose::get_input_port_name(int p_port) const {
	return "vec";
}

int VisualShaderNodeVectorDecompose::get_output_port_count() const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return 2;
		case OP_TYPE_VECTOR_3D:
			return 3;
		case OP_TYPE_VECTOR_4D:
			return 4;
		default:
			break;
	}
	return 0;
}

VisualShaderNodeVectorDecompose::PortType VisualShaderNodeVectorDecompose::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVectorDecompose::get_output_port_name(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
			}
		} break;
		case OP_TYPE_VECTOR_3D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
				case 2:
					return "z";
			}
		} break;
		case OP_TYPE_VECTOR_4D: {
			switch (p_port) {
				case 0:
					return "x";
				case 1:
					return "y";
				case 2:
					return "z";
				case 3:
					return "w";
			}
		} break;
		default:
			break;
	}
	return String();
}

void VisualShaderNodeVectorDecompose::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

String VisualShaderNodeVectorDecompose::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	switch (op_type) {
		case OP_TYPE_VECTOR_2D: {
			code += "	" + p_output_vars[0] + " = " + p_input_vars[0] + ".x;\n";
			code += "	" + p_output_vars[1] + " = " + p_input_vars[0] + ".y;\n";
		} break;
		case OP_TYPE_VECTOR_3D: {
			code += "	" + p_output_vars[0] + " = " + p_input_vars[0] + ".x;\n";
			code += "	" + p_output_vars[1] + " = " + p_input_vars[0] + ".y;\n";
			code += "	" + p_output_vars[2] + " = " + p_input_vars[0] + ".z;\n";
		} break;
		case OP_TYPE_VECTOR_4D: {
			code += "	" + p_output_vars[0] + " = " + p_input_vars[0] + ".x;\n";
			code += "	" + p_output_vars[1] + " = " + p_input_vars[0] + ".y;\n";
			code += "	" + p_output_vars[2] + " = " + p_input_vars[0] + ".z;\n";
			code += "	" + p_output_vars[3] + " = " + p_input_vars[0] + ".w;\n";
		} break;
		default:
			break;
	}
	return code;
}

VisualShaderNodeVectorDecompose::VisualShaderNodeVectorDecompose() {
	set_input_port_default_value(0, Vector3(0.0, 0.0, 0.0));
}

////////////// Transform Decompose

String VisualShaderNodeTransformDecompose::get_caption() const {
	return "TransformDecompose";
}

int VisualShaderNodeTransformDecompose::get_input_port_count() const {
	return 1;
}

VisualShaderNodeTransformDecompose::PortType VisualShaderNodeTransformDecompose::get_input_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformDecompose::get_input_port_name(int p_port) const {
	return "xform";
}

int VisualShaderNodeTransformDecompose::get_output_port_count() const {
	return 4;
}

VisualShaderNodeTransformDecompose::PortType VisualShaderNodeTransformDecompose::get_output_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeTransformDecompose::get_output_port_name(int p_port) const {
	if (p_port == 0) {
		return "x";
	} else if (p_port == 1) {
		return "y";
	} else if (p_port == 2) {
		return "z";
	} else {
		return "origin";
	}
}

String VisualShaderNodeTransformDecompose::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	" + p_output_vars[0] + " = " + p_input_vars[0] + "[0].xyz;\n";
	code += "	" + p_output_vars[1] + " = " + p_input_vars[0] + "[1].xyz;\n";
	code += "	" + p_output_vars[2] + " = " + p_input_vars[0] + "[2].xyz;\n";
	code += "	" + p_output_vars[3] + " = " + p_input_vars[0] + "[3].xyz;\n";
	return code;
}

VisualShaderNodeTransformDecompose::VisualShaderNodeTransformDecompose() {
	set_input_port_default_value(0, Transform3D());
}

////////////// Float Parameter

String VisualShaderNodeFloatParameter::get_caption() const {
	return "FloatParameter";
}

int VisualShaderNodeFloatParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeFloatParameter::PortType VisualShaderNodeFloatParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeFloatParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeFloatParameter::PortType VisualShaderNodeFloatParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFloatParameter::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeFloatParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = "";
	if (hint == HINT_RANGE) {
		code += _get_qual_str() + "uniform float " + get_parameter_name() + " : hint_range(" + rtos(hint_range_min) + ", " + rtos(hint_range_max) + ")";
	} else if (hint == HINT_RANGE_STEP) {
		code += _get_qual_str() + "uniform float " + get_parameter_name() + " : hint_range(" + rtos(hint_range_min) + ", " + rtos(hint_range_max) + ", " + rtos(hint_range_step) + ")";
	} else {
		code += _get_qual_str() + "uniform float " + get_parameter_name();
	}
	if (default_value_enabled) {
		code += " = " + rtos(default_value);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeFloatParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

bool VisualShaderNodeFloatParameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeFloatParameter::is_use_prop_slots() const {
	return true;
}

void VisualShaderNodeFloatParameter::set_hint(Hint p_hint) {
	ERR_FAIL_INDEX(int(p_hint), int(HINT_MAX));
	if (hint == p_hint) {
		return;
	}
	hint = p_hint;
	emit_changed();
}

VisualShaderNodeFloatParameter::Hint VisualShaderNodeFloatParameter::get_hint() const {
	return hint;
}

void VisualShaderNodeFloatParameter::set_min(float p_value) {
	if (Math::is_equal_approx(hint_range_min, p_value)) {
		return;
	}
	hint_range_min = p_value;
	emit_changed();
}

float VisualShaderNodeFloatParameter::get_min() const {
	return hint_range_min;
}

void VisualShaderNodeFloatParameter::set_max(float p_value) {
	if (Math::is_equal_approx(hint_range_max, p_value)) {
		return;
	}
	hint_range_max = p_value;
	emit_changed();
}

float VisualShaderNodeFloatParameter::get_max() const {
	return hint_range_max;
}

void VisualShaderNodeFloatParameter::set_step(float p_value) {
	if (Math::is_equal_approx(hint_range_step, p_value)) {
		return;
	}
	hint_range_step = p_value;
	emit_changed();
}

float VisualShaderNodeFloatParameter::get_step() const {
	return hint_range_step;
}

void VisualShaderNodeFloatParameter::set_default_value_enabled(bool p_enabled) {
	if (default_value_enabled == p_enabled) {
		return;
	}
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeFloatParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeFloatParameter::set_default_value(float p_value) {
	if (Math::is_equal_approx(default_value, p_value)) {
		return;
	}
	default_value = p_value;
	emit_changed();
}

float VisualShaderNodeFloatParameter::get_default_value() const {
	return default_value;
}

void VisualShaderNodeFloatParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hint", "hint"), &VisualShaderNodeFloatParameter::set_hint);
	ClassDB::bind_method(D_METHOD("get_hint"), &VisualShaderNodeFloatParameter::get_hint);

	ClassDB::bind_method(D_METHOD("set_min", "value"), &VisualShaderNodeFloatParameter::set_min);
	ClassDB::bind_method(D_METHOD("get_min"), &VisualShaderNodeFloatParameter::get_min);

	ClassDB::bind_method(D_METHOD("set_max", "value"), &VisualShaderNodeFloatParameter::set_max);
	ClassDB::bind_method(D_METHOD("get_max"), &VisualShaderNodeFloatParameter::get_max);

	ClassDB::bind_method(D_METHOD("set_step", "value"), &VisualShaderNodeFloatParameter::set_step);
	ClassDB::bind_method(D_METHOD("get_step"), &VisualShaderNodeFloatParameter::get_step);

	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeFloatParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeFloatParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeFloatParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeFloatParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "hint", PROPERTY_HINT_ENUM, "None,Range,Range+Step"), "set_hint", "get_hint");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step"), "set_step", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_value"), "set_default_value", "get_default_value");

	BIND_ENUM_CONSTANT(HINT_NONE);
	BIND_ENUM_CONSTANT(HINT_RANGE);
	BIND_ENUM_CONSTANT(HINT_RANGE_STEP);
	BIND_ENUM_CONSTANT(HINT_MAX);
}

bool VisualShaderNodeFloatParameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeFloatParameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeFloatParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("hint");
	if (hint == HINT_RANGE || hint == HINT_RANGE_STEP) {
		props.push_back("min");
		props.push_back("max");
	}
	if (hint == HINT_RANGE_STEP) {
		props.push_back("step");
	}
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeFloatParameter::VisualShaderNodeFloatParameter() {
}

////////////// Integer Parameter

String VisualShaderNodeIntParameter::get_caption() const {
	return "IntParameter";
}

int VisualShaderNodeIntParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeIntParameter::PortType VisualShaderNodeIntParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeIntParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIntParameter::PortType VisualShaderNodeIntParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_INT;
}

String VisualShaderNodeIntParameter::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

String VisualShaderNodeIntParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = "";
	if (hint == HINT_RANGE) {
		code += _get_qual_str() + "uniform int " + get_parameter_name() + " : hint_range(" + itos(hint_range_min) + ", " + itos(hint_range_max) + ")";
	} else if (hint == HINT_RANGE_STEP) {
		code += _get_qual_str() + "uniform int " + get_parameter_name() + " : hint_range(" + itos(hint_range_min) + ", " + itos(hint_range_max) + ", " + itos(hint_range_step) + ")";
	} else if (hint == HINT_ENUM) {
		code += _get_qual_str() + "uniform int " + get_parameter_name() + " : hint_enum(";

		bool first = true;
		for (const String &_name : hint_enum_names) {
			if (first) {
				first = false;
			} else {
				code += ", ";
			}
			code += "\"" + _name.c_escape() + "\"";
		}

		code += ")";
	} else {
		code += _get_qual_str() + "uniform int " + get_parameter_name();
	}
	if (default_value_enabled) {
		code += " = " + itos(default_value);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeIntParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

bool VisualShaderNodeIntParameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeIntParameter::is_use_prop_slots() const {
	return true;
}

void VisualShaderNodeIntParameter::set_hint(Hint p_hint) {
	ERR_FAIL_INDEX(int(p_hint), int(HINT_MAX));
	if (hint == p_hint) {
		return;
	}
	hint = p_hint;
	emit_changed();
}

VisualShaderNodeIntParameter::Hint VisualShaderNodeIntParameter::get_hint() const {
	return hint;
}

void VisualShaderNodeIntParameter::set_min(int p_value) {
	if (hint_range_min == p_value) {
		return;
	}
	hint_range_min = p_value;
	emit_changed();
}

int VisualShaderNodeIntParameter::get_min() const {
	return hint_range_min;
}

void VisualShaderNodeIntParameter::set_max(int p_value) {
	if (hint_range_max == p_value) {
		return;
	}
	hint_range_max = p_value;
	emit_changed();
}

int VisualShaderNodeIntParameter::get_max() const {
	return hint_range_max;
}

void VisualShaderNodeIntParameter::set_step(int p_value) {
	if (hint_range_step == p_value) {
		return;
	}
	hint_range_step = p_value;
	emit_changed();
}

int VisualShaderNodeIntParameter::get_step() const {
	return hint_range_step;
}

void VisualShaderNodeIntParameter::set_enum_names(const PackedStringArray &p_names) {
	if (hint_enum_names == p_names) {
		return;
	}
	hint_enum_names = p_names;
	emit_changed();
}

PackedStringArray VisualShaderNodeIntParameter::get_enum_names() const {
	return hint_enum_names;
}

void VisualShaderNodeIntParameter::set_default_value_enabled(bool p_default_value_enabled) {
	if (default_value_enabled == p_default_value_enabled) {
		return;
	}
	default_value_enabled = p_default_value_enabled;
	emit_changed();
}

bool VisualShaderNodeIntParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeIntParameter::set_default_value(int p_default_value) {
	if (default_value == p_default_value) {
		return;
	}
	default_value = p_default_value;
	emit_changed();
}

int VisualShaderNodeIntParameter::get_default_value() const {
	return default_value;
}

void VisualShaderNodeIntParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hint", "hint"), &VisualShaderNodeIntParameter::set_hint);
	ClassDB::bind_method(D_METHOD("get_hint"), &VisualShaderNodeIntParameter::get_hint);

	ClassDB::bind_method(D_METHOD("set_min", "value"), &VisualShaderNodeIntParameter::set_min);
	ClassDB::bind_method(D_METHOD("get_min"), &VisualShaderNodeIntParameter::get_min);

	ClassDB::bind_method(D_METHOD("set_max", "value"), &VisualShaderNodeIntParameter::set_max);
	ClassDB::bind_method(D_METHOD("get_max"), &VisualShaderNodeIntParameter::get_max);

	ClassDB::bind_method(D_METHOD("set_step", "value"), &VisualShaderNodeIntParameter::set_step);
	ClassDB::bind_method(D_METHOD("get_step"), &VisualShaderNodeIntParameter::get_step);

	ClassDB::bind_method(D_METHOD("set_enum_names", "names"), &VisualShaderNodeIntParameter::set_enum_names);
	ClassDB::bind_method(D_METHOD("get_enum_names"), &VisualShaderNodeIntParameter::get_enum_names);

	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeIntParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeIntParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeIntParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeIntParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "hint", PROPERTY_HINT_ENUM, "None,Range,Range + Step,Enum"), "set_hint", "get_hint");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "min"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "step"), "set_step", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "enum_names"), "set_enum_names", "get_enum_names");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_value"), "set_default_value", "get_default_value");

	BIND_ENUM_CONSTANT(HINT_NONE);
	BIND_ENUM_CONSTANT(HINT_RANGE);
	BIND_ENUM_CONSTANT(HINT_RANGE_STEP);
	BIND_ENUM_CONSTANT(HINT_ENUM);
	BIND_ENUM_CONSTANT(HINT_MAX);
}

bool VisualShaderNodeIntParameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeIntParameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeIntParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("hint");
	if (hint == HINT_RANGE || hint == HINT_RANGE_STEP) {
		props.push_back("min");
		props.push_back("max");
	}
	if (hint == HINT_RANGE_STEP) {
		props.push_back("step");
	}
	if (hint == HINT_ENUM) {
		props.push_back("enum_names");
	}
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeIntParameter::VisualShaderNodeIntParameter() {
}

////////////// Unsigned Integer Parameter

String VisualShaderNodeUIntParameter::get_caption() const {
	return "UIntParameter";
}

int VisualShaderNodeUIntParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeUIntParameter::PortType VisualShaderNodeUIntParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeUIntParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeUIntParameter::PortType VisualShaderNodeUIntParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR_UINT;
}

String VisualShaderNodeUIntParameter::get_output_port_name(int p_port) const {
	return ""; // No output port means the editor will be used as port.
}

String VisualShaderNodeUIntParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform uint " + get_parameter_name();
	if (default_value_enabled) {
		code += " = " + itos(default_value);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeUIntParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

bool VisualShaderNodeUIntParameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeUIntParameter::is_use_prop_slots() const {
	return true;
}

void VisualShaderNodeUIntParameter::set_default_value_enabled(bool p_default_value_enabled) {
	if (default_value_enabled == p_default_value_enabled) {
		return;
	}
	default_value_enabled = p_default_value_enabled;
	emit_changed();
}

bool VisualShaderNodeUIntParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeUIntParameter::set_default_value(int p_default_value) {
	if (default_value == p_default_value) {
		return;
	}
	default_value = p_default_value;
	emit_changed();
}

int VisualShaderNodeUIntParameter::get_default_value() const {
	return default_value;
}

void VisualShaderNodeUIntParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeUIntParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeUIntParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeUIntParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeUIntParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeUIntParameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // All qualifiers are supported.
}

bool VisualShaderNodeUIntParameter::is_convertible_to_constant() const {
	return true; // Conversion is allowed.
}

Vector<StringName> VisualShaderNodeUIntParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeUIntParameter::VisualShaderNodeUIntParameter() {
}

////////////// Boolean Parameter

String VisualShaderNodeBooleanParameter::get_caption() const {
	return "BooleanParameter";
}

int VisualShaderNodeBooleanParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeBooleanParameter::PortType VisualShaderNodeBooleanParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeBooleanParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeBooleanParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeBooleanParameter::PortType VisualShaderNodeBooleanParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeBooleanParameter::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

void VisualShaderNodeBooleanParameter::set_default_value_enabled(bool p_default_value_enabled) {
	if (default_value_enabled == p_default_value_enabled) {
		return;
	}
	default_value_enabled = p_default_value_enabled;
	emit_changed();
}

bool VisualShaderNodeBooleanParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeBooleanParameter::set_default_value(bool p_default_value) {
	if (default_value == p_default_value) {
		return;
	}
	default_value = p_default_value;
	emit_changed();
}

bool VisualShaderNodeBooleanParameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeBooleanParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform bool " + get_parameter_name();
	if (default_value_enabled) {
		if (default_value) {
			code += " = true";
		} else {
			code += " = false";
		}
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeBooleanParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

bool VisualShaderNodeBooleanParameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeBooleanParameter::is_use_prop_slots() const {
	return true;
}

void VisualShaderNodeBooleanParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeBooleanParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeBooleanParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeBooleanParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeBooleanParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeBooleanParameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeBooleanParameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeBooleanParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeBooleanParameter::VisualShaderNodeBooleanParameter() {
}

////////////// Color Parameter

String VisualShaderNodeColorParameter::get_caption() const {
	return "ColorParameter";
}

int VisualShaderNodeColorParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeColorParameter::PortType VisualShaderNodeColorParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeColorParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeColorParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeColorParameter::PortType VisualShaderNodeColorParameter::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeColorParameter::get_output_port_name(int p_port) const {
	return "color";
}

void VisualShaderNodeColorParameter::set_default_value_enabled(bool p_enabled) {
	if (default_value_enabled == p_enabled) {
		return;
	}
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeColorParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeColorParameter::set_default_value(const Color &p_value) {
	if (default_value.is_equal_approx(p_value)) {
		return;
	}
	default_value = p_value;
	emit_changed();
}

Color VisualShaderNodeColorParameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeColorParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform vec4 " + get_parameter_name() + " : source_color";
	if (default_value_enabled) {
		code += vformat(" = vec4(%.6f, %.6f, %.6f, %.6f)", default_value.r, default_value.g, default_value.b, default_value.a);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeColorParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

bool VisualShaderNodeColorParameter::is_show_prop_names() const {
	return true;
}

void VisualShaderNodeColorParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeColorParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeColorParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeColorParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeColorParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeColorParameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeColorParameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeColorParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeColorParameter::VisualShaderNodeColorParameter() {
}

////////////// Vector2 Parameter

String VisualShaderNodeVec2Parameter::get_caption() const {
	return "Vector2Parameter";
}

int VisualShaderNodeVec2Parameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec2Parameter::PortType VisualShaderNodeVec2Parameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_2D;
}

String VisualShaderNodeVec2Parameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec2Parameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec2Parameter::PortType VisualShaderNodeVec2Parameter::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec2Parameter::get_output_port_name(int p_port) const {
	return "vector";
}

void VisualShaderNodeVec2Parameter::set_default_value_enabled(bool p_enabled) {
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeVec2Parameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeVec2Parameter::set_default_value(const Vector2 &p_value) {
	default_value = p_value;
	emit_changed();
}

Vector2 VisualShaderNodeVec2Parameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeVec2Parameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform vec2 " + get_parameter_name();
	if (default_value_enabled) {
		code += vformat(" = vec2(%.6f, %.6f)", default_value.x, default_value.y);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeVec2Parameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

void VisualShaderNodeVec2Parameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeVec2Parameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeVec2Parameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeVec2Parameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeVec2Parameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeVec2Parameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeVec2Parameter::is_use_prop_slots() const {
	return true;
}

bool VisualShaderNodeVec2Parameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeVec2Parameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeVec2Parameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeVec2Parameter::VisualShaderNodeVec2Parameter() {
}

////////////// Vector3 Parameter

String VisualShaderNodeVec3Parameter::get_caption() const {
	return "Vector3Parameter";
}

int VisualShaderNodeVec3Parameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec3Parameter::PortType VisualShaderNodeVec3Parameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeVec3Parameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec3Parameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec3Parameter::PortType VisualShaderNodeVec3Parameter::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec3Parameter::get_output_port_name(int p_port) const {
	return "vector";
}

void VisualShaderNodeVec3Parameter::set_default_value_enabled(bool p_enabled) {
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeVec3Parameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeVec3Parameter::set_default_value(const Vector3 &p_value) {
	default_value = p_value;
	emit_changed();
}

Vector3 VisualShaderNodeVec3Parameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeVec3Parameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform vec3 " + get_parameter_name();
	if (default_value_enabled) {
		code += vformat(" = vec3(%.6f, %.6f, %.6f)", default_value.x, default_value.y, default_value.z);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeVec3Parameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

void VisualShaderNodeVec3Parameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeVec3Parameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeVec3Parameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeVec3Parameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeVec3Parameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeVec3Parameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeVec3Parameter::is_use_prop_slots() const {
	return true;
}

bool VisualShaderNodeVec3Parameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // all qualifiers are supported
}

bool VisualShaderNodeVec3Parameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeVec3Parameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeVec3Parameter::VisualShaderNodeVec3Parameter() {
}

////////////// Vector4 Parameter

String VisualShaderNodeVec4Parameter::get_caption() const {
	return "Vector4Parameter";
}

int VisualShaderNodeVec4Parameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVec4Parameter::PortType VisualShaderNodeVec4Parameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_4D;
}

String VisualShaderNodeVec4Parameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeVec4Parameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVec4Parameter::PortType VisualShaderNodeVec4Parameter::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeVec4Parameter::get_output_port_name(int p_port) const {
	return "vector";
}

void VisualShaderNodeVec4Parameter::set_default_value_enabled(bool p_enabled) {
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeVec4Parameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeVec4Parameter::set_default_value(const Vector4 &p_value) {
	default_value = p_value;
	emit_changed();
}

Vector4 VisualShaderNodeVec4Parameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeVec4Parameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform vec4 " + get_parameter_name();
	if (default_value_enabled) {
		code += vformat(" = vec4(%.6f, %.6f, %.6f, %.6f)", default_value.x, default_value.y, default_value.z, default_value.w);
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeVec4Parameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

void VisualShaderNodeVec4Parameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeVec4Parameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeVec4Parameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeVec4Parameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeVec4Parameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR4, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeVec4Parameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeVec4Parameter::is_use_prop_slots() const {
	return true;
}

bool VisualShaderNodeVec4Parameter::is_qualifier_supported(Qualifier p_qual) const {
	return true; // All qualifiers are supported.
}

bool VisualShaderNodeVec4Parameter::is_convertible_to_constant() const {
	return true; // Conversion is allowed.
}

Vector<StringName> VisualShaderNodeVec4Parameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeVec4Parameter::VisualShaderNodeVec4Parameter() {
}

////////////// Transform Parameter

String VisualShaderNodeTransformParameter::get_caption() const {
	return "TransformParameter";
}

int VisualShaderNodeTransformParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeTransformParameter::PortType VisualShaderNodeTransformParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeTransformParameter::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeTransformParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTransformParameter::PortType VisualShaderNodeTransformParameter::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeTransformParameter::get_output_port_name(int p_port) const {
	return ""; //no output port means the editor will be used as port
}

void VisualShaderNodeTransformParameter::set_default_value_enabled(bool p_enabled) {
	default_value_enabled = p_enabled;
	emit_changed();
}

bool VisualShaderNodeTransformParameter::is_default_value_enabled() const {
	return default_value_enabled;
}

void VisualShaderNodeTransformParameter::set_default_value(const Transform3D &p_value) {
	default_value = p_value;
	emit_changed();
}

Transform3D VisualShaderNodeTransformParameter::get_default_value() const {
	return default_value;
}

String VisualShaderNodeTransformParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform mat4 " + get_parameter_name();
	if (default_value_enabled) {
		Vector3 row0 = default_value.basis.rows[0];
		Vector3 row1 = default_value.basis.rows[1];
		Vector3 row2 = default_value.basis.rows[2];
		Vector3 origin = default_value.origin;
		code += " = mat4(" + vformat("vec4(%.6f, %.6f, %.6f, 0.0)", row0.x, row0.y, row0.z) + vformat(", vec4(%.6f, %.6f, %.6f, 0.0)", row1.x, row1.y, row1.z) + vformat(", vec4(%.6f, %.6f, %.6f, 0.0)", row2.x, row2.y, row2.z) + vformat(", vec4(%.6f, %.6f, %.6f, 1.0)", origin.x, origin.y, origin.z) + ")";
	}
	code += ";\n";
	return code;
}

String VisualShaderNodeTransformParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

void VisualShaderNodeTransformParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_value_enabled", "enabled"), &VisualShaderNodeTransformParameter::set_default_value_enabled);
	ClassDB::bind_method(D_METHOD("is_default_value_enabled"), &VisualShaderNodeTransformParameter::is_default_value_enabled);

	ClassDB::bind_method(D_METHOD("set_default_value", "value"), &VisualShaderNodeTransformParameter::set_default_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &VisualShaderNodeTransformParameter::get_default_value);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_value_enabled"), "set_default_value_enabled", "is_default_value_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "default_value"), "set_default_value", "get_default_value");
}

bool VisualShaderNodeTransformParameter::is_show_prop_names() const {
	return true;
}

bool VisualShaderNodeTransformParameter::is_use_prop_slots() const {
	return true;
}

bool VisualShaderNodeTransformParameter::is_qualifier_supported(Qualifier p_qual) const {
	if (p_qual == Qualifier::QUAL_INSTANCE) {
		return false;
	}
	return true;
}

bool VisualShaderNodeTransformParameter::is_convertible_to_constant() const {
	return true; // conversion is allowed
}

Vector<StringName> VisualShaderNodeTransformParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("default_value_enabled");
	if (default_value_enabled) {
		props.push_back("default_value");
	}
	return props;
}

VisualShaderNodeTransformParameter::VisualShaderNodeTransformParameter() {
}

//////////////

String get_sampler_hint(VisualShaderNodeTextureParameter::TextureType p_texture_type, VisualShaderNodeTextureParameter::ColorDefault p_color_default, VisualShaderNodeTextureParameter::TextureFilter p_texture_filter, VisualShaderNodeTextureParameter::TextureRepeat p_texture_repeat, VisualShaderNodeTextureParameter::TextureSource p_texture_source) {
	String code;
	bool has_colon = false;

	// type
	{
		String type_code;

		switch (p_texture_type) {
			case VisualShaderNodeTextureParameter::TYPE_DATA:
				if (p_color_default == VisualShaderNodeTextureParameter::COLOR_DEFAULT_BLACK) {
					type_code = "hint_default_black";
				} else if (p_color_default == VisualShaderNodeTextureParameter::COLOR_DEFAULT_TRANSPARENT) {
					type_code = "hint_default_transparent";
				}
				break;
			case VisualShaderNodeTextureParameter::TYPE_COLOR:
				type_code = "source_color";
				if (p_color_default == VisualShaderNodeTextureParameter::COLOR_DEFAULT_BLACK) {
					type_code += ", hint_default_black";
				} else if (p_color_default == VisualShaderNodeTextureParameter::COLOR_DEFAULT_TRANSPARENT) {
					type_code += ", hint_default_transparent";
				}
				break;
			case VisualShaderNodeTextureParameter::TYPE_NORMAL_MAP:
				type_code = "hint_normal";
				break;
			case VisualShaderNodeTextureParameter::TYPE_ANISOTROPY:
				type_code = "hint_anisotropy";
				break;
			default:
				break;
		}

		if (!type_code.is_empty()) {
			code += " : " + type_code;
			has_colon = true;
		}
	}

	// filter
	{
		String filter_code;

		switch (p_texture_filter) {
			case VisualShaderNodeTextureParameter::FILTER_NEAREST:
				filter_code = "filter_nearest";
				break;
			case VisualShaderNodeTextureParameter::FILTER_LINEAR:
				filter_code = "filter_linear";
				break;
			case VisualShaderNodeTextureParameter::FILTER_NEAREST_MIPMAP:
				filter_code = "filter_nearest_mipmap";
				break;
			case VisualShaderNodeTextureParameter::FILTER_LINEAR_MIPMAP:
				filter_code = "filter_linear_mipmap";
				break;
			case VisualShaderNodeTextureParameter::FILTER_NEAREST_MIPMAP_ANISOTROPIC:
				filter_code = "filter_nearest_mipmap_anisotropic";
				break;
			case VisualShaderNodeTextureParameter::FILTER_LINEAR_MIPMAP_ANISOTROPIC:
				filter_code = "filter_linear_mipmap_anisotropic";
				break;
			default:
				break;
		}

		if (!filter_code.is_empty()) {
			if (!has_colon) {
				code += " : ";
				has_colon = true;
			} else {
				code += ", ";
			}
			code += filter_code;
		}
	}

	// repeat
	{
		String repeat_code;

		switch (p_texture_repeat) {
			case VisualShaderNodeTextureParameter::REPEAT_ENABLED:
				repeat_code = "repeat_enable";
				break;
			case VisualShaderNodeTextureParameter::REPEAT_DISABLED:
				repeat_code = "repeat_disable";
				break;
			default:
				break;
		}

		if (!repeat_code.is_empty()) {
			if (!has_colon) {
				code += " : ";
				has_colon = true;
			} else {
				code += ", ";
			}
			code += repeat_code;
		}
	}

	// source
	{
		String source_code;

		switch (p_texture_source) {
			case VisualShaderNodeTextureParameter::SOURCE_SCREEN:
				source_code = "hint_screen_texture";
				break;
			case VisualShaderNodeTextureParameter::SOURCE_DEPTH:
				source_code = "hint_depth_texture";
				break;
			case VisualShaderNodeTextureParameter::SOURCE_NORMAL_ROUGHNESS:
				source_code = "hint_normal_roughness_texture";
				break;
			default:
				break;
		}

		if (!source_code.is_empty()) {
			if (!has_colon) {
				code += " : ";
			} else {
				code += ", ";
			}
			code += source_code;
		}
	}

	return code;
}

////////////// Texture Parameter

int VisualShaderNodeTextureParameter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeTextureParameter::PortType VisualShaderNodeTextureParameter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeTextureParameter::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeTextureParameter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeTextureParameter::PortType VisualShaderNodeTextureParameter::get_output_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_SAMPLER;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeTextureParameter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "";
}

void VisualShaderNodeTextureParameter::set_texture_type(TextureType p_texture_type) {
	ERR_FAIL_INDEX(int(p_texture_type), int(TYPE_MAX));
	if (texture_type == p_texture_type) {
		return;
	}
	texture_type = p_texture_type;
	emit_changed();
}

VisualShaderNodeTextureParameter::TextureType VisualShaderNodeTextureParameter::get_texture_type() const {
	return texture_type;
}

void VisualShaderNodeTextureParameter::set_color_default(ColorDefault p_color_default) {
	ERR_FAIL_INDEX(int(p_color_default), int(COLOR_DEFAULT_MAX));
	if (color_default == p_color_default) {
		return;
	}
	color_default = p_color_default;
	emit_changed();
}

VisualShaderNodeTextureParameter::ColorDefault VisualShaderNodeTextureParameter::get_color_default() const {
	return color_default;
}

void VisualShaderNodeTextureParameter::set_texture_filter(TextureFilter p_filter) {
	ERR_FAIL_INDEX(int(p_filter), int(FILTER_MAX));
	if (texture_filter == p_filter) {
		return;
	}
	texture_filter = p_filter;
	emit_changed();
}

VisualShaderNodeTextureParameter::TextureFilter VisualShaderNodeTextureParameter::get_texture_filter() const {
	return texture_filter;
}

void VisualShaderNodeTextureParameter::set_texture_repeat(TextureRepeat p_repeat) {
	ERR_FAIL_INDEX(int(p_repeat), int(REPEAT_MAX));
	if (texture_repeat == p_repeat) {
		return;
	}
	texture_repeat = p_repeat;
	emit_changed();
}

VisualShaderNodeTextureParameter::TextureRepeat VisualShaderNodeTextureParameter::get_texture_repeat() const {
	return texture_repeat;
}

void VisualShaderNodeTextureParameter::set_texture_source(TextureSource p_source) {
	ERR_FAIL_INDEX(int(p_source), int(SOURCE_MAX));
	if (texture_source == p_source) {
		return;
	}
	texture_source = p_source;
	emit_changed();
}

VisualShaderNodeTextureParameter::TextureSource VisualShaderNodeTextureParameter::get_texture_source() const {
	return texture_source;
}

Vector<StringName> VisualShaderNodeTextureParameter::get_editable_properties() const {
	Vector<StringName> props = VisualShaderNodeParameter::get_editable_properties();
	props.push_back("texture_type");
	if (texture_type == TYPE_DATA || texture_type == TYPE_COLOR) {
		props.push_back("color_default");
	}
	props.push_back("texture_filter");
	props.push_back("texture_repeat");
	props.push_back("texture_source");
	return props;
}

bool VisualShaderNodeTextureParameter::is_show_prop_names() const {
	return true;
}

String VisualShaderNodeTextureParameter::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	String warning = VisualShaderNodeParameter::get_warning(p_mode, p_type);

	if (texture_source != SOURCE_NONE) {
		String texture_source_str;

		switch (texture_source) {
			case SOURCE_SCREEN: {
				texture_source_str = "Screen";
			} break;
			case SOURCE_DEPTH: {
				texture_source_str = "Depth";
			} break;
			case SOURCE_NORMAL_ROUGHNESS: {
				texture_source_str = "NormalRoughness";
			} break;
			default:
				break;
		}

		if (texture_type == TYPE_NORMAL_MAP || texture_type == TYPE_ANISOTROPY) {
			String texture_type_str;

			switch (texture_type) {
				case TYPE_NORMAL_MAP: {
					texture_type_str = "Normal Map";
				} break;
				case TYPE_ANISOTROPY: {
					texture_type_str = "Anisotropic";
				} break;
				default:
					break;
			}
			if (!warning.is_empty()) {
				warning += "\n";
			}
			warning += vformat(RTR("'%s' type is incompatible with '%s' source."), texture_type_str, texture_source_str);
		} else if (color_default != COLOR_DEFAULT_WHITE) {
			String color_default_str;

			switch (color_default) {
				case COLOR_DEFAULT_BLACK: {
					color_default_str = "Black";
				} break;
				case COLOR_DEFAULT_TRANSPARENT: {
					color_default_str = "Transparent";
				} break;
				default:
					break;
			}
			if (!warning.is_empty()) {
				warning += "\n";
			}
			warning += vformat(RTR("'%s' default color is incompatible with '%s' source."), color_default_str, texture_source_str);
		}
	}

	return warning;
}

HashMap<StringName, String> VisualShaderNodeTextureParameter::get_editable_properties_names() const {
	HashMap<StringName, String> names;
	names.insert("texture_type", RTR("Type"));
	names.insert("color_default", RTR("Default Color"));
	names.insert("texture_filter", RTR("Filter"));
	names.insert("texture_repeat", RTR("Repeat"));
	names.insert("texture_source", RTR("Source"));
	return names;
}

void VisualShaderNodeTextureParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture_type", "type"), &VisualShaderNodeTextureParameter::set_texture_type);
	ClassDB::bind_method(D_METHOD("get_texture_type"), &VisualShaderNodeTextureParameter::get_texture_type);

	ClassDB::bind_method(D_METHOD("set_color_default", "color"), &VisualShaderNodeTextureParameter::set_color_default);
	ClassDB::bind_method(D_METHOD("get_color_default"), &VisualShaderNodeTextureParameter::get_color_default);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &VisualShaderNodeTextureParameter::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &VisualShaderNodeTextureParameter::get_texture_filter);

	ClassDB::bind_method(D_METHOD("set_texture_repeat", "repeat"), &VisualShaderNodeTextureParameter::set_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_texture_repeat"), &VisualShaderNodeTextureParameter::get_texture_repeat);

	ClassDB::bind_method(D_METHOD("set_texture_source", "source"), &VisualShaderNodeTextureParameter::set_texture_source);
	ClassDB::bind_method(D_METHOD("get_texture_source"), &VisualShaderNodeTextureParameter::get_texture_source);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_type", PROPERTY_HINT_ENUM, "Data,Color,Normal Map,Anisotropic"), "set_texture_type", "get_texture_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_default", PROPERTY_HINT_ENUM, "White,Black,Transparent"), "set_color_default", "get_color_default");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Default,Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_repeat", PROPERTY_HINT_ENUM, "Default,Enabled,Disabled"), "set_texture_repeat", "get_texture_repeat");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_source", PROPERTY_HINT_ENUM, "None,Screen,Depth,NormalRoughness"), "set_texture_source", "get_texture_source");

	BIND_ENUM_CONSTANT(TYPE_DATA);
	BIND_ENUM_CONSTANT(TYPE_COLOR);
	BIND_ENUM_CONSTANT(TYPE_NORMAL_MAP);
	BIND_ENUM_CONSTANT(TYPE_ANISOTROPY);
	BIND_ENUM_CONSTANT(TYPE_MAX);

	BIND_ENUM_CONSTANT(COLOR_DEFAULT_WHITE);
	BIND_ENUM_CONSTANT(COLOR_DEFAULT_BLACK);
	BIND_ENUM_CONSTANT(COLOR_DEFAULT_TRANSPARENT);
	BIND_ENUM_CONSTANT(COLOR_DEFAULT_MAX);

	BIND_ENUM_CONSTANT(FILTER_DEFAULT);
	BIND_ENUM_CONSTANT(FILTER_NEAREST);
	BIND_ENUM_CONSTANT(FILTER_LINEAR);
	BIND_ENUM_CONSTANT(FILTER_NEAREST_MIPMAP);
	BIND_ENUM_CONSTANT(FILTER_LINEAR_MIPMAP);
	BIND_ENUM_CONSTANT(FILTER_NEAREST_MIPMAP_ANISOTROPIC);
	BIND_ENUM_CONSTANT(FILTER_LINEAR_MIPMAP_ANISOTROPIC);
	BIND_ENUM_CONSTANT(FILTER_MAX);

	BIND_ENUM_CONSTANT(REPEAT_DEFAULT);
	BIND_ENUM_CONSTANT(REPEAT_ENABLED);
	BIND_ENUM_CONSTANT(REPEAT_DISABLED);
	BIND_ENUM_CONSTANT(REPEAT_MAX);

	BIND_ENUM_CONSTANT(SOURCE_NONE);
	BIND_ENUM_CONSTANT(SOURCE_SCREEN);
	BIND_ENUM_CONSTANT(SOURCE_DEPTH);
	BIND_ENUM_CONSTANT(SOURCE_NORMAL_ROUGHNESS);
	BIND_ENUM_CONSTANT(SOURCE_MAX);
}

bool VisualShaderNodeTextureParameter::is_qualifier_supported(Qualifier p_qual) const {
	switch (p_qual) {
		case Qualifier::QUAL_NONE:
			return true;
		case Qualifier::QUAL_GLOBAL:
			return true;
		case Qualifier::QUAL_INSTANCE:
			return false;
		default:
			break;
	}
	return false;
}

bool VisualShaderNodeTextureParameter::is_convertible_to_constant() const {
	return false; // conversion is not allowed
}

VisualShaderNodeTextureParameter::VisualShaderNodeTextureParameter() {
}

////////////// Texture2D Parameter

String VisualShaderNodeTexture2DParameter::get_caption() const {
	return "Texture2DParameter";
}

String VisualShaderNodeTexture2DParameter::get_output_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "sampler2D";
		default:
			return "";
	}
}

String VisualShaderNodeTexture2DParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform sampler2D " + get_parameter_name();
	code += get_sampler_hint(texture_type, color_default, texture_filter, texture_repeat, texture_source);
	code += ";\n";
	return code;
}

VisualShaderNodeTexture2DParameter::VisualShaderNodeTexture2DParameter() {
}

////////////// Texture Parameter (Triplanar)

String VisualShaderNodeTextureParameterTriplanar::get_caption() const {
	return "TextureParameterTriplanar";
}

int VisualShaderNodeTextureParameterTriplanar::get_input_port_count() const {
	return 2;
}

VisualShaderNodeTextureParameterTriplanar::PortType VisualShaderNodeTextureParameterTriplanar::get_input_port_type(int p_port) const {
	if (p_port == 0 || p_port == 1) {
		return PORT_TYPE_VECTOR_3D;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeTextureParameterTriplanar::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "weights";
	} else if (p_port == 1) {
		return "pos";
	}
	return "";
}

int VisualShaderNodeTextureParameterTriplanar::get_output_port_count() const {
	return 2;
}

VisualShaderNodeTextureParameterTriplanar::PortType VisualShaderNodeTextureParameterTriplanar::get_output_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_4D;
		case 1:
			return PORT_TYPE_SAMPLER;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeTextureParameterTriplanar::get_output_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "color";
		case 1:
			return "sampler2D";
		default:
			return "";
	}
}

String VisualShaderNodeTextureParameterTriplanar::generate_global_per_node(Shader::Mode p_mode, int p_id) const {
	String code;

	code += "// " + get_caption() + "\n";
	code += "	vec4 triplanar_texture(sampler2D p_sampler, vec3 p_weights, vec3 p_triplanar_pos) {\n";
	code += "		vec4 samp = vec4(0.0);\n";
	code += "		samp += texture(p_sampler, p_triplanar_pos.xy) * p_weights.z;\n";
	code += "		samp += texture(p_sampler, p_triplanar_pos.xz) * p_weights.y;\n";
	code += "		samp += texture(p_sampler, p_triplanar_pos.zy * vec2(-1.0, 1.0)) * p_weights.x;\n";
	code += "		return samp;\n";
	code += "	}\n";
	code += "\n";
	code += "	uniform vec3 triplanar_scale = vec3(1.0, 1.0, 1.0);\n";
	code += "	uniform vec3 triplanar_offset;\n";
	code += "	uniform float triplanar_sharpness = 0.5;\n";
	code += "\n";
	code += "	varying vec3 triplanar_power_normal;\n";
	code += "	varying vec3 triplanar_pos;\n";

	return code;
}

String VisualShaderNodeTextureParameterTriplanar::generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code;

	if (p_type == VisualShader::TYPE_VERTEX) {
		code += "// " + get_caption() + "\n";
		code += "	{\n";
		code += "		triplanar_power_normal = pow(abs(NORMAL), vec3(triplanar_sharpness));\n";
		code += "		triplanar_power_normal /= dot(triplanar_power_normal, vec3(1.0));\n";
		code += "		triplanar_pos = VERTEX * triplanar_scale + triplanar_offset;\n";
		code += "		triplanar_pos *= vec3(1.0, -1.0, 1.0);\n";
		code += "	}\n";
	}

	return code;
}

String VisualShaderNodeTextureParameterTriplanar::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform sampler2D " + get_parameter_name();
	code += get_sampler_hint(texture_type, color_default, texture_filter, texture_repeat, texture_source);
	code += ";\n";
	return code;
}

String VisualShaderNodeTextureParameterTriplanar::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String id = get_parameter_name();

	String code;
	if (p_input_vars[0].is_empty() && p_input_vars[1].is_empty()) {
		code += "	" + p_output_vars[0] + " = triplanar_texture(" + id + ", triplanar_power_normal, triplanar_pos);\n";
	} else if (!p_input_vars[0].is_empty() && p_input_vars[1].is_empty()) {
		code += "	" + p_output_vars[0] + " = triplanar_texture(" + id + ", " + p_input_vars[0] + ", triplanar_pos);\n";
	} else if (p_input_vars[0].is_empty() && !p_input_vars[1].is_empty()) {
		code += "	" + p_output_vars[0] + " = triplanar_texture(" + id + ", triplanar_power_normal, " + p_input_vars[1] + ");\n";
	} else {
		code += "	" + p_output_vars[0] + " = triplanar_texture(" + id + ", " + p_input_vars[0] + ", " + p_input_vars[1] + ");\n";
	}

	return code;
}

bool VisualShaderNodeTextureParameterTriplanar::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_port == 0) {
		return true;
	} else if (p_port == 1) {
		return true;
	}
	return false;
}

VisualShaderNodeTextureParameterTriplanar::VisualShaderNodeTextureParameterTriplanar() {
}

////////////// Texture2DArray Parameter

String VisualShaderNodeTexture2DArrayParameter::get_caption() const {
	return "Texture2DArrayParameter";
}

String VisualShaderNodeTexture2DArrayParameter::get_output_port_name(int p_port) const {
	return "sampler2DArray";
}

String VisualShaderNodeTexture2DArrayParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform sampler2DArray " + get_parameter_name();
	code += get_sampler_hint(texture_type, color_default, texture_filter, texture_repeat, texture_source);
	code += ";\n";
	return code;
}

VisualShaderNodeTexture2DArrayParameter::VisualShaderNodeTexture2DArrayParameter() {
}

////////////// Texture3D Parameter

String VisualShaderNodeTexture3DParameter::get_caption() const {
	return "Texture3DParameter";
}

String VisualShaderNodeTexture3DParameter::get_output_port_name(int p_port) const {
	return "sampler3D";
}

String VisualShaderNodeTexture3DParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform sampler3D " + get_parameter_name();
	code += get_sampler_hint(texture_type, color_default, texture_filter, texture_repeat, texture_source);
	code += ";\n";
	return code;
}

VisualShaderNodeTexture3DParameter::VisualShaderNodeTexture3DParameter() {
}

////////////// Cubemap Parameter

String VisualShaderNodeCubemapParameter::get_caption() const {
	return "CubemapParameter";
}

String VisualShaderNodeCubemapParameter::get_output_port_name(int p_port) const {
	return "samplerCube";
}

String VisualShaderNodeCubemapParameter::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String code = _get_qual_str() + "uniform samplerCube " + get_parameter_name();
	code += get_sampler_hint(texture_type, color_default, texture_filter, texture_repeat, texture_source);
	code += ";\n";
	return code;
}

VisualShaderNodeCubemapParameter::VisualShaderNodeCubemapParameter() {
}

////////////// If

String VisualShaderNodeIf::get_caption() const {
	return "If";
}

int VisualShaderNodeIf::get_input_port_count() const {
	return 6;
}

VisualShaderNodeIf::PortType VisualShaderNodeIf::get_input_port_type(int p_port) const {
	if (p_port == 0 || p_port == 1 || p_port == 2) {
		return PORT_TYPE_SCALAR;
	}
	return PORT_TYPE_VECTOR_3D;
}

String VisualShaderNodeIf::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "a";
		case 1:
			return "b";
		case 2:
			return "tolerance";
		case 3:
			return "a == b";
		case 4:
			return "a > b";
		case 5:
			return "a < b";
		default:
			return "";
	}
}

int VisualShaderNodeIf::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIf::PortType VisualShaderNodeIf::get_output_port_type(int p_port) const {
	return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
}

String VisualShaderNodeIf::get_output_port_name(int p_port) const {
	return "result";
}

String VisualShaderNodeIf::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	if(abs(" + p_input_vars[0] + " - " + p_input_vars[1] + ") < " + p_input_vars[2] + ")\n"; // abs(a - b) < tolerance eg. a == b
	code += "	{\n";
	code += "		" + p_output_vars[0] + " = " + p_input_vars[3] + ";\n";
	code += "	}\n";
	code += "	else if(" + p_input_vars[0] + " < " + p_input_vars[1] + ")\n"; // a < b
	code += "	{\n";
	code += "		" + p_output_vars[0] + " = " + p_input_vars[5] + ";\n";
	code += "	}\n";
	code += "	else\n"; // a > b (or a >= b if abs(a - b) < tolerance is false)
	code += "	{\n";
	code += "		" + p_output_vars[0] + " = " + p_input_vars[4] + ";\n";
	code += "	}\n";
	return code;
}

VisualShaderNodeIf::VisualShaderNodeIf() {
	simple_decl = false;
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, CMP_EPSILON);
	set_input_port_default_value(3, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(4, Vector3(0.0, 0.0, 0.0));
	set_input_port_default_value(5, Vector3(0.0, 0.0, 0.0));
}

////////////// Switch

String VisualShaderNodeSwitch::get_caption() const {
	return "Switch";
}

int VisualShaderNodeSwitch::get_input_port_count() const {
	return 3;
}

VisualShaderNodeSwitch::PortType VisualShaderNodeSwitch::get_input_port_type(int p_port) const {
	if (p_port == 0) {
		return PORT_TYPE_BOOLEAN;
	}
	if (p_port == 1 || p_port == 2) {
		switch (op_type) {
			case OP_TYPE_INT:
				return PORT_TYPE_SCALAR_INT;
			case OP_TYPE_UINT:
				return PORT_TYPE_SCALAR_UINT;
			case OP_TYPE_VECTOR_2D:
				return PORT_TYPE_VECTOR_2D;
			case OP_TYPE_VECTOR_3D:
				return PORT_TYPE_VECTOR_3D;
			case OP_TYPE_VECTOR_4D:
				return PORT_TYPE_VECTOR_4D;
			case OP_TYPE_BOOLEAN:
				return PORT_TYPE_BOOLEAN;
			case OP_TYPE_TRANSFORM:
				return PORT_TYPE_TRANSFORM;
			default:
				break;
		}
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeSwitch::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "value";
		case 1:
			return "true";
		case 2:
			return "false";
		default:
			return "";
	}
}

int VisualShaderNodeSwitch::get_output_port_count() const {
	return 1;
}

VisualShaderNodeSwitch::PortType VisualShaderNodeSwitch::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_INT:
			return PORT_TYPE_SCALAR_INT;
		case OP_TYPE_UINT:
			return PORT_TYPE_SCALAR_UINT;
		case OP_TYPE_VECTOR_2D:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		case OP_TYPE_BOOLEAN:
			return PORT_TYPE_BOOLEAN;
		case OP_TYPE_TRANSFORM:
			return PORT_TYPE_TRANSFORM;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeSwitch::get_output_port_name(int p_port) const {
	return "result";
}

void VisualShaderNodeSwitch::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_FLOAT:
			set_input_port_default_value(1, 1.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2));
			break;
		case OP_TYPE_UINT:
		case OP_TYPE_INT:
			set_input_port_default_value(1, 1, get_input_port_default_value(1));
			set_input_port_default_value(2, 0, get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_2D:
			set_input_port_default_value(1, Vector2(1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector2(0.0, 0.0), get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_3D:
			set_input_port_default_value(1, Vector3(1.0, 1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector3(0.0, 0.0, 0.0), get_input_port_default_value(2));
			break;
		case OP_TYPE_VECTOR_4D:
			set_input_port_default_value(1, Quaternion(1.0, 1.0, 1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Quaternion(0.0, 0.0, 0.0, 0.0), get_input_port_default_value(2));
			break;
		case OP_TYPE_BOOLEAN:
			set_input_port_default_value(1, true);
			set_input_port_default_value(2, false);
			break;
		case OP_TYPE_TRANSFORM:
			set_input_port_default_value(1, Transform3D());
			set_input_port_default_value(2, Transform3D());
			break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeSwitch::OpType VisualShaderNodeSwitch::get_op_type() const {
	return op_type;
}

Vector<StringName> VisualShaderNodeSwitch::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeSwitch::_bind_methods() { // static
	ClassDB::bind_method(D_METHOD("set_op_type", "type"), &VisualShaderNodeSwitch::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeSwitch::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Float,Int,UInt,Vector2,Vector3,Vector4,Boolean,Transform"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_FLOAT);
	BIND_ENUM_CONSTANT(OP_TYPE_INT);
	BIND_ENUM_CONSTANT(OP_TYPE_UINT);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_BOOLEAN);
	BIND_ENUM_CONSTANT(OP_TYPE_TRANSFORM);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

String VisualShaderNodeSwitch::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	bool use_mix = false;
	switch (op_type) {
		case OP_TYPE_FLOAT: {
			use_mix = true;
		} break;
		case OP_TYPE_VECTOR_2D: {
			use_mix = true;
		} break;
		case OP_TYPE_VECTOR_3D: {
			use_mix = true;
		} break;
		case OP_TYPE_VECTOR_4D: {
			use_mix = true;
		} break;
		default: {
		} break;
	}

	String code;
	if (use_mix) {
		code += "	" + p_output_vars[0] + " = mix(" + p_input_vars[2] + ", " + p_input_vars[1] + ", float(" + p_input_vars[0] + "));\n";
	} else {
		code += "	if (" + p_input_vars[0] + ") {\n";
		code += "		" + p_output_vars[0] + " = " + p_input_vars[1] + ";\n";
		code += "	} else {\n";
		code += "		" + p_output_vars[0] + " = " + p_input_vars[2] + ";\n";
		code += "	}\n";
	}
	return code;
}

VisualShaderNodeSwitch::VisualShaderNodeSwitch() {
	simple_decl = false;
	set_input_port_default_value(0, false);
	set_input_port_default_value(1, 1.0);
	set_input_port_default_value(2, 0.0);
}

////////////// Fresnel

String VisualShaderNodeFresnel::get_caption() const {
	return "Fresnel";
}

int VisualShaderNodeFresnel::get_input_port_count() const {
	return 4;
}

VisualShaderNodeFresnel::PortType VisualShaderNodeFresnel::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_VECTOR_3D;
		case 2:
			return PORT_TYPE_BOOLEAN;
		case 3:
			return PORT_TYPE_SCALAR;
		default:
			return PORT_TYPE_VECTOR_3D;
	}
}

String VisualShaderNodeFresnel::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "normal";
		case 1:
			return "view";
		case 2:
			return "invert";
		case 3:
			return "power";
		default:
			return "";
	}
}

int VisualShaderNodeFresnel::get_output_port_count() const {
	return 1;
}

VisualShaderNodeFresnel::PortType VisualShaderNodeFresnel::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeFresnel::get_output_port_name(int p_port) const {
	return "result";
}

bool VisualShaderNodeFresnel::is_generate_input_var(int p_port) const {
	if (p_port == 2) {
		return false;
	}
	return true;
}

String VisualShaderNodeFresnel::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String normal;
	String view;
	if (p_input_vars[0].is_empty()) {
		if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
			normal = "NORMAL";
		} else {
			normal = "vec3(0.0)";
		}
	} else {
		normal = p_input_vars[0];
	}
	if (p_input_vars[1].is_empty()) {
		if (p_mode == Shader::MODE_SPATIAL) {
			view = "VIEW";
		} else {
			view = "vec3(0.0)";
		}
	} else {
		view = p_input_vars[1];
	}

	if (is_input_port_connected(2)) {
		return "	" + p_output_vars[0] + " = " + p_input_vars[2] + " ? (pow(clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + ")) : (pow(1.0 - clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + "));\n";
	} else {
		if (get_input_port_default_value(2)) {
			return "	" + p_output_vars[0] + " = pow(clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + ");\n";
		} else {
			return "	" + p_output_vars[0] + " = pow(1.0 - clamp(dot(" + normal + ", " + view + "), 0.0, 1.0), " + p_input_vars[3] + ");\n";
		}
	}
}

bool VisualShaderNodeFresnel::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	if (p_port == 0) {
		if (p_mode == Shader::MODE_CANVAS_ITEM || p_mode == Shader::MODE_SPATIAL) {
			return true;
		}
	} else if (p_port == 1) {
		if (p_mode == Shader::MODE_SPATIAL) {
			return true;
		}
	}
	return false;
}

VisualShaderNodeFresnel::VisualShaderNodeFresnel() {
	set_input_port_default_value(2, false);
	set_input_port_default_value(3, 1.0);
}

////////////// Is

String VisualShaderNodeIs::get_caption() const {
	return "Is";
}

int VisualShaderNodeIs::get_input_port_count() const {
	return 1;
}

VisualShaderNodeIs::PortType VisualShaderNodeIs::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeIs::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeIs::get_output_port_count() const {
	return 1;
}

VisualShaderNodeIs::PortType VisualShaderNodeIs::get_output_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeIs::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeIs::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *functions[FUNC_MAX] = {
		"isinf($)",
		"isnan($)"
	};

	String code;
	code += "	" + p_output_vars[0] + " = " + String(functions[func]).replace("$", p_input_vars[0]) + ";\n";
	return code;
}

void VisualShaderNodeIs::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeIs::Function VisualShaderNodeIs::get_function() const {
	return func;
}

Vector<StringName> VisualShaderNodeIs::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("function");
	return props;
}

void VisualShaderNodeIs::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeIs::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeIs::get_function);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "Inf,NaN"), "set_function", "get_function");

	BIND_ENUM_CONSTANT(FUNC_IS_INF);
	BIND_ENUM_CONSTANT(FUNC_IS_NAN);
	BIND_ENUM_CONSTANT(FUNC_MAX);
}

VisualShaderNodeIs::VisualShaderNodeIs() {
	set_input_port_default_value(0, 0.0);
}

////////////// Compare

String VisualShaderNodeCompare::get_caption() const {
	return "Compare";
}

int VisualShaderNodeCompare::get_input_port_count() const {
	if (comparison_type == CTYPE_SCALAR && (func == FUNC_EQUAL || func == FUNC_NOT_EQUAL)) {
		return 3;
	}
	return 2;
}

VisualShaderNodeCompare::PortType VisualShaderNodeCompare::get_input_port_type(int p_port) const {
	switch (comparison_type) {
		case CTYPE_SCALAR:
			return PORT_TYPE_SCALAR;
		case CTYPE_SCALAR_INT:
			return PORT_TYPE_SCALAR_INT;
		case CTYPE_SCALAR_UINT:
			return PORT_TYPE_SCALAR_UINT;
		case CTYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case CTYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case CTYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		case CTYPE_BOOLEAN:
			return PORT_TYPE_BOOLEAN;
		case CTYPE_TRANSFORM:
			return PORT_TYPE_TRANSFORM;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeCompare::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "a";
	} else if (p_port == 1) {
		return "b";
	} else if (p_port == 2) {
		return "tolerance";
	}
	return "";
}

int VisualShaderNodeCompare::get_output_port_count() const {
	return 1;
}

VisualShaderNodeCompare::PortType VisualShaderNodeCompare::get_output_port_type(int p_port) const {
	return PORT_TYPE_BOOLEAN;
}

String VisualShaderNodeCompare::get_output_port_name(int p_port) const {
	if (p_port == 0) {
		return "result";
	}
	return "";
}

String VisualShaderNodeCompare::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	if (comparison_type == CTYPE_BOOLEAN || comparison_type == CTYPE_TRANSFORM) {
		if (func > FUNC_NOT_EQUAL) {
			return RTR("Invalid comparison function for that type.");
		}
	}
	return "";
}

String VisualShaderNodeCompare::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	static const char *operators[FUNC_MAX] = {
		"==",
		"!=",
		">",
		">=",
		"<",
		"<=",
	};

	static const char *functions[FUNC_MAX] = {
		"equal($)",
		"notEqual($)",
		"greaterThan($)",
		"greaterThanEqual($)",
		"lessThan($)",
		"lessThanEqual($)",
	};

	static const char *conditions[COND_MAX] = {
		"all($)",
		"any($)",
	};

	String code;
	switch (comparison_type) {
		case CTYPE_SCALAR: {
			if (func == FUNC_EQUAL) {
				code += "	" + p_output_vars[0] + " = (abs(" + p_input_vars[0] + " - " + p_input_vars[1] + ") < " + p_input_vars[2] + ");";
			} else if (func == FUNC_NOT_EQUAL) {
				code += "	" + p_output_vars[0] + " = !(abs(" + p_input_vars[0] + " - " + p_input_vars[1] + ") < " + p_input_vars[2] + ");";
			} else {
				code += "	" + p_output_vars[0] + " = " + (p_input_vars[0] + " $ " + p_input_vars[1]).replace("$", operators[func]) + ";\n";
			}
		} break;
		case CTYPE_SCALAR_UINT:
		case CTYPE_SCALAR_INT: {
			code += "	" + p_output_vars[0] + " = " + (p_input_vars[0] + " $ " + p_input_vars[1]).replace("$", operators[func]) + ";\n";
		} break;
		case CTYPE_VECTOR_2D: {
			code += "	{\n";
			code += "		bvec2 _bv = " + String(functions[func]).replace("$", p_input_vars[0] + ", " + p_input_vars[1]) + ";\n";
			code += "		" + p_output_vars[0] + " = " + String(conditions[condition]).replace("$", "_bv") + ";\n";
			code += "	}\n";
		} break;
		case CTYPE_VECTOR_3D: {
			code += "	{\n";
			code += "		bvec3 _bv = " + String(functions[func]).replace("$", p_input_vars[0] + ", " + p_input_vars[1]) + ";\n";
			code += "		" + p_output_vars[0] + " = " + String(conditions[condition]).replace("$", "_bv") + ";\n";
			code += "	}\n";
		} break;
		case CTYPE_VECTOR_4D: {
			code += "	{\n";
			code += "		bvec4 _bv = " + String(functions[func]).replace("$", p_input_vars[0] + ", " + p_input_vars[1]) + ";\n";
			code += "		" + p_output_vars[0] + " = " + String(conditions[condition]).replace("$", "_bv") + ";\n";
			code += "	}\n";
		} break;
		case CTYPE_BOOLEAN: {
			if (func > FUNC_NOT_EQUAL) {
				return "	" + p_output_vars[0] + " = false;\n";
			}
			code += "	" + p_output_vars[0] + " = " + (p_input_vars[0] + " $ " + p_input_vars[1]).replace("$", operators[func]) + ";\n";
		} break;
		case CTYPE_TRANSFORM: {
			if (func > FUNC_NOT_EQUAL) {
				return "	" + p_output_vars[0] + " = false;\n";
			}
			code += "	" + p_output_vars[0] + " = " + (p_input_vars[0] + " $ " + p_input_vars[1]).replace("$", operators[func]) + ";\n";
		} break;
		default:
			break;
	}
	return code;
}

void VisualShaderNodeCompare::set_comparison_type(ComparisonType p_comparison_type) {
	ERR_FAIL_INDEX(int(p_comparison_type), int(CTYPE_MAX));
	if (comparison_type == p_comparison_type) {
		return;
	}
	switch (p_comparison_type) {
		case CTYPE_SCALAR:
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			simple_decl = true;
			break;
		case CTYPE_SCALAR_UINT:
		case CTYPE_SCALAR_INT:
			set_input_port_default_value(0, 0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0, get_input_port_default_value(1));
			simple_decl = true;
			break;
		case CTYPE_VECTOR_2D:
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
			simple_decl = false;
			break;
		case CTYPE_VECTOR_3D:
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
			simple_decl = false;
			break;
		case CTYPE_VECTOR_4D:
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
			simple_decl = false;
			break;
		case CTYPE_BOOLEAN:
			set_input_port_default_value(0, false);
			set_input_port_default_value(1, false);
			simple_decl = true;
			break;
		case CTYPE_TRANSFORM:
			set_input_port_default_value(0, Transform3D());
			set_input_port_default_value(1, Transform3D());
			simple_decl = true;
			break;
		default:
			break;
	}
	comparison_type = p_comparison_type;
	emit_changed();
}

VisualShaderNodeCompare::ComparisonType VisualShaderNodeCompare::get_comparison_type() const {
	return comparison_type;
}

void VisualShaderNodeCompare::set_function(Function p_func) {
	ERR_FAIL_INDEX(int(p_func), int(FUNC_MAX));
	if (func == p_func) {
		return;
	}
	func = p_func;
	emit_changed();
}

VisualShaderNodeCompare::Function VisualShaderNodeCompare::get_function() const {
	return func;
}

void VisualShaderNodeCompare::set_condition(Condition p_condition) {
	ERR_FAIL_INDEX(int(p_condition), int(COND_MAX));
	if (condition == p_condition) {
		return;
	}
	condition = p_condition;
	emit_changed();
}

VisualShaderNodeCompare::Condition VisualShaderNodeCompare::get_condition() const {
	return condition;
}

Vector<StringName> VisualShaderNodeCompare::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("type");
	props.push_back("function");
	if (comparison_type == CTYPE_VECTOR_2D || comparison_type == CTYPE_VECTOR_3D || comparison_type == CTYPE_VECTOR_4D) {
		props.push_back("condition");
	}
	return props;
}

void VisualShaderNodeCompare::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_comparison_type", "type"), &VisualShaderNodeCompare::set_comparison_type);
	ClassDB::bind_method(D_METHOD("get_comparison_type"), &VisualShaderNodeCompare::get_comparison_type);

	ClassDB::bind_method(D_METHOD("set_function", "func"), &VisualShaderNodeCompare::set_function);
	ClassDB::bind_method(D_METHOD("get_function"), &VisualShaderNodeCompare::get_function);

	ClassDB::bind_method(D_METHOD("set_condition", "condition"), &VisualShaderNodeCompare::set_condition);
	ClassDB::bind_method(D_METHOD("get_condition"), &VisualShaderNodeCompare::get_condition);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, "Float,Int,UInt,Vector2,Vector3,Vector4,Boolean,Transform"), "set_comparison_type", "get_comparison_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "function", PROPERTY_HINT_ENUM, "a == b,a != b,a > b,a >= b,a < b,a <= b"), "set_function", "get_function");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "condition", PROPERTY_HINT_ENUM, "All,Any"), "set_condition", "get_condition");

	BIND_ENUM_CONSTANT(CTYPE_SCALAR);
	BIND_ENUM_CONSTANT(CTYPE_SCALAR_INT);
	BIND_ENUM_CONSTANT(CTYPE_SCALAR_UINT);
	BIND_ENUM_CONSTANT(CTYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(CTYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(CTYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(CTYPE_BOOLEAN);
	BIND_ENUM_CONSTANT(CTYPE_TRANSFORM);
	BIND_ENUM_CONSTANT(CTYPE_MAX);

	BIND_ENUM_CONSTANT(FUNC_EQUAL);
	BIND_ENUM_CONSTANT(FUNC_NOT_EQUAL);
	BIND_ENUM_CONSTANT(FUNC_GREATER_THAN);
	BIND_ENUM_CONSTANT(FUNC_GREATER_THAN_EQUAL);
	BIND_ENUM_CONSTANT(FUNC_LESS_THAN);
	BIND_ENUM_CONSTANT(FUNC_LESS_THAN_EQUAL);
	BIND_ENUM_CONSTANT(FUNC_MAX);

	BIND_ENUM_CONSTANT(COND_ALL);
	BIND_ENUM_CONSTANT(COND_ANY);
	BIND_ENUM_CONSTANT(COND_MAX);
}

VisualShaderNodeCompare::VisualShaderNodeCompare() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, CMP_EPSILON);
}

////////////// Fma

String VisualShaderNodeMultiplyAdd::get_caption() const {
	return "MultiplyAdd";
}

int VisualShaderNodeMultiplyAdd::get_input_port_count() const {
	return 3;
}

VisualShaderNodeMultiplyAdd::PortType VisualShaderNodeMultiplyAdd::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeMultiplyAdd::get_input_port_name(int p_port) const {
	if (p_port == 0) {
		return "a";
	} else if (p_port == 1) {
		return "b(*)";
	} else if (p_port == 2) {
		return "c(+)";
	}
	return "";
}

int VisualShaderNodeMultiplyAdd::get_output_port_count() const {
	return 1;
}

VisualShaderNodeMultiplyAdd::PortType VisualShaderNodeMultiplyAdd::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_3D:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case OP_TYPE_VECTOR_4D:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeMultiplyAdd::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeMultiplyAdd::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		return "	" + p_output_vars[0] + " = (" + p_input_vars[0] + " * " + p_input_vars[1] + ") + " + p_input_vars[2] + ";\n";
	}
	return "	" + p_output_vars[0] + " = fma(" + p_input_vars[0] + ", " + p_input_vars[1] + ", " + p_input_vars[2] + ");\n";
}

void VisualShaderNodeMultiplyAdd::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX((int)p_op_type, int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, 1.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 0.0, get_input_port_default_value(2));
		} break;
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector2(), get_input_port_default_value(2));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(1.0, 1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector3(), get_input_port_default_value(2));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Vector4(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector4(1.0, 1.0, 1.0, 1.0), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector4(), get_input_port_default_value(2));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeMultiplyAdd::OpType VisualShaderNodeMultiplyAdd::get_op_type() const {
	return op_type;
}

Vector<StringName> VisualShaderNodeMultiplyAdd::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeMultiplyAdd::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "type"), &VisualShaderNodeMultiplyAdd::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeMultiplyAdd::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector3,Vector4"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeMultiplyAdd::VisualShaderNodeMultiplyAdd() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 1.0);
	set_input_port_default_value(2, 0.0);
}

////////////// Billboard

String VisualShaderNodeBillboard::get_caption() const {
	return "GetBillboardMatrix";
}

int VisualShaderNodeBillboard::get_input_port_count() const {
	return 0;
}

VisualShaderNodeBillboard::PortType VisualShaderNodeBillboard::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeBillboard::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeBillboard::get_output_port_count() const {
	return 1;
}

VisualShaderNodeBillboard::PortType VisualShaderNodeBillboard::get_output_port_type(int p_port) const {
	return PORT_TYPE_TRANSFORM;
}

String VisualShaderNodeBillboard::get_output_port_name(int p_port) const {
	return "model_view_matrix";
}

String VisualShaderNodeBillboard::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;

	switch (billboard_type) {
		case BILLBOARD_TYPE_ENABLED:
			code += "	{\n";
			code += "		mat4 __mvm = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);\n";
			if (keep_scale) {
				code += "		__mvm = __mvm * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			}
			code += "		" + p_output_vars[0] + " = __mvm;\n";
			code += "	}\n";
			break;
		case BILLBOARD_TYPE_FIXED_Y:
			code += "	{\n";
			code += "		mat4 __mvm = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], MODEL_MATRIX[1], vec4(normalize(cross(INV_VIEW_MATRIX[0].xyz, MODEL_MATRIX[1].xyz)), 0.0), MODEL_MATRIX[3]);\n";
			if (keep_scale) {
				code += "		__mvm = __mvm * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			} else {
				code += "		__mvm = __mvm * mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0 / length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			}
			code += "		" + p_output_vars[0] + " = __mvm;\n";
			code += "	}\n";
			break;
		case BILLBOARD_TYPE_PARTICLES:
			code += "	{\n";
			code += "		mat4 __wm = mat4(normalize(INV_VIEW_MATRIX[0]), normalize(INV_VIEW_MATRIX[1]), normalize(INV_VIEW_MATRIX[2]), MODEL_MATRIX[3]);\n";
			code += "		__wm = __wm * mat4(vec4(cos(INSTANCE_CUSTOM.x), -sin(INSTANCE_CUSTOM.x), 0.0, 0.0), vec4(sin(INSTANCE_CUSTOM.x), cos(INSTANCE_CUSTOM.x), 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			if (keep_scale) {
				code += "		__wm = __wm * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			}
			code += "		" + p_output_vars[0] + " = VIEW_MATRIX * __wm;\n";
			code += "	}\n";
			break;
		default:
			code += "	" + p_output_vars[0] + " = mat4(1.0);\n";
			break;
	}

	return code;
}

bool VisualShaderNodeBillboard::is_show_prop_names() const {
	return true;
}

void VisualShaderNodeBillboard::set_billboard_type(BillboardType p_billboard_type) {
	ERR_FAIL_INDEX(int(p_billboard_type), int(BILLBOARD_TYPE_MAX));
	if (billboard_type == p_billboard_type) {
		return;
	}
	billboard_type = p_billboard_type;
	simple_decl = bool(billboard_type == BILLBOARD_TYPE_DISABLED);
	set_disabled(simple_decl);
	emit_changed();
}

VisualShaderNodeBillboard::BillboardType VisualShaderNodeBillboard::get_billboard_type() const {
	return billboard_type;
}

void VisualShaderNodeBillboard::set_keep_scale_enabled(bool p_enabled) {
	keep_scale = p_enabled;
	emit_changed();
}

bool VisualShaderNodeBillboard::is_keep_scale_enabled() const {
	return keep_scale;
}

Vector<StringName> VisualShaderNodeBillboard::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("billboard_type");
	if (billboard_type == BILLBOARD_TYPE_ENABLED || billboard_type == BILLBOARD_TYPE_FIXED_Y || billboard_type == BILLBOARD_TYPE_PARTICLES) {
		props.push_back("keep_scale");
	}
	return props;
}

void VisualShaderNodeBillboard::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_billboard_type", "billboard_type"), &VisualShaderNodeBillboard::set_billboard_type);
	ClassDB::bind_method(D_METHOD("get_billboard_type"), &VisualShaderNodeBillboard::get_billboard_type);

	ClassDB::bind_method(D_METHOD("set_keep_scale_enabled", "enabled"), &VisualShaderNodeBillboard::set_keep_scale_enabled);
	ClassDB::bind_method(D_METHOD("is_keep_scale_enabled"), &VisualShaderNodeBillboard::is_keep_scale_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "billboard_type", PROPERTY_HINT_ENUM, "Disabled,Enabled,Y-Billboard,Particles"), "set_billboard_type", "get_billboard_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_scale"), "set_keep_scale_enabled", "is_keep_scale_enabled");

	BIND_ENUM_CONSTANT(BILLBOARD_TYPE_DISABLED);
	BIND_ENUM_CONSTANT(BILLBOARD_TYPE_ENABLED);
	BIND_ENUM_CONSTANT(BILLBOARD_TYPE_FIXED_Y);
	BIND_ENUM_CONSTANT(BILLBOARD_TYPE_PARTICLES);
	BIND_ENUM_CONSTANT(BILLBOARD_TYPE_MAX);
}

VisualShaderNodeBillboard::VisualShaderNodeBillboard() {
	simple_decl = false;
}

////////////// DistanceFade

String VisualShaderNodeDistanceFade::get_caption() const {
	return "DistanceFade";
}

int VisualShaderNodeDistanceFade::get_input_port_count() const {
	return 2;
}

VisualShaderNodeDistanceFade::PortType VisualShaderNodeDistanceFade::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_SCALAR;
		case 1:
			return PORT_TYPE_SCALAR;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDistanceFade::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "min";
		case 1:
			return "max";
	}

	return "";
}

int VisualShaderNodeDistanceFade::get_output_port_count() const {
	return 1;
}

VisualShaderNodeDistanceFade::PortType VisualShaderNodeDistanceFade::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeDistanceFade::get_output_port_name(int p_port) const {
	return "amount";
}

bool VisualShaderNodeDistanceFade::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeDistanceFade::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += vformat("	%s = clamp(smoothstep(%s, %s, length(VERTEX)), 0.0, 1.0);\n", p_output_vars[0], p_input_vars[0], p_input_vars[1]);
	return code;
}

VisualShaderNodeDistanceFade::VisualShaderNodeDistanceFade() {
	set_input_port_default_value(0, 0.0);
	set_input_port_default_value(1, 10.0);
}

////////////// ProximityFade

String VisualShaderNodeProximityFade::get_caption() const {
	return "ProximityFade";
}

int VisualShaderNodeProximityFade::get_input_port_count() const {
	return 1;
}

VisualShaderNodeProximityFade::PortType VisualShaderNodeProximityFade::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeProximityFade::get_input_port_name(int p_port) const {
	return "distance";
}

int VisualShaderNodeProximityFade::get_output_port_count() const {
	return 1;
}

VisualShaderNodeProximityFade::PortType VisualShaderNodeProximityFade::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeProximityFade::get_output_port_name(int p_port) const {
	return "fade";
}

bool VisualShaderNodeProximityFade::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeProximityFade::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return "uniform sampler2D " + make_unique_id(p_type, p_id, "depth_tex") + " : hint_depth_texture;\n";
}

String VisualShaderNodeProximityFade::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	{\n";

	code += "		float __depth_tex = texture(" + make_unique_id(p_type, p_id, "depth_tex") + ", SCREEN_UV).r;\n";
	if (!RenderingServer::get_singleton()->is_low_end()) {
		code += "		vec4 __depth_world_pos = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, __depth_tex, 1.0);\n";
	} else {
		code += "		vec4 __depth_world_pos = INV_PROJECTION_MATRIX * vec4(vec3(SCREEN_UV, __depth_tex) * 2.0 - 1.0, 1.0);\n";
	}
	code += "		__depth_world_pos.xyz /= __depth_world_pos.w;\n";
	code += vformat("		%s = clamp(1.0 - smoothstep(__depth_world_pos.z + %s, __depth_world_pos.z, VERTEX.z), 0.0, 1.0);\n", p_output_vars[0], p_input_vars[0]);

	code += "	}\n";
	return code;
}

VisualShaderNodeProximityFade::VisualShaderNodeProximityFade() {
	set_input_port_default_value(0, 1.0);

	simple_decl = false;
}

////////////// Random Range

String VisualShaderNodeRandomRange::get_caption() const {
	return "RandomRange";
}

int VisualShaderNodeRandomRange::get_input_port_count() const {
	return 3;
}

VisualShaderNodeRandomRange::PortType VisualShaderNodeRandomRange::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_SCALAR;
		case 2:
			return PORT_TYPE_SCALAR;
		default:
			break;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeRandomRange::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "seed";
		case 1:
			return "min";
		case 2:
			return "max";
		default:
			break;
	}

	return "";
}

int VisualShaderNodeRandomRange::get_output_port_count() const {
	return 1;
}

VisualShaderNodeRandomRange::PortType VisualShaderNodeRandomRange::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeRandomRange::get_output_port_name(int p_port) const {
	return "value";
}

String VisualShaderNodeRandomRange::generate_global_per_node(Shader::Mode p_mode, int p_id) const {
	String code;

	code += "\n\n";
	code += "// 3D Noise with friendly permission by Inigo Quilez\n";
	code += "vec3 hash_noise_range( vec3 p ) {\n";
	code += "	p *= mat3(vec3(127.1, 311.7, -53.7), vec3(269.5, 183.3, 77.1), vec3(-301.7, 27.3, 215.3));\n";
	code += "	return 2.0 * fract(fract(p)*4375.55) -1.;\n";
	code += "}\n";
	code += "\n";

	return code;
}

String VisualShaderNodeRandomRange::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;

	code += vformat("	%s = mix(%s, %s, hash_noise_range(%s).x);\n", p_output_vars[0], p_input_vars[1], p_input_vars[2], p_input_vars[0]);

	return code;
}

VisualShaderNodeRandomRange::VisualShaderNodeRandomRange() {
	set_input_port_default_value(0, Vector3(1.0, 1.0, 1.0));
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, 1.0);
}

////////////// Remap

String VisualShaderNodeRemap::get_caption() const {
	return "Remap";
}

int VisualShaderNodeRemap::get_input_port_count() const {
	return 5;
}

VisualShaderNodeRemap::PortType VisualShaderNodeRemap::get_input_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_2D_SCALAR:
			if (p_port == 0) {
				return PORT_TYPE_VECTOR_2D;
			}
			break;
		case OP_TYPE_VECTOR_3D:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_3D_SCALAR:
			if (p_port == 0) {
				return PORT_TYPE_VECTOR_3D;
			}
			break;
		case OP_TYPE_VECTOR_4D:
			return PORT_TYPE_VECTOR_4D;
		case OP_TYPE_VECTOR_4D_SCALAR:
			if (p_port == 0) {
				return PORT_TYPE_VECTOR_4D;
			}
			break;
		default:
			break;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeRemap::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "value";
		case 1:
			return "input min";
		case 2:
			return "input max";
		case 3:
			return "output min";
		case 4:
			return "output max";
		default:
			break;
	}

	return "";
}

int VisualShaderNodeRemap::get_output_port_count() const {
	return 1;
}

VisualShaderNodeRemap::PortType VisualShaderNodeRemap::get_output_port_type(int p_port) const {
	switch (op_type) {
		case OP_TYPE_VECTOR_2D:
		case OP_TYPE_VECTOR_2D_SCALAR:
			return PORT_TYPE_VECTOR_2D;
		case OP_TYPE_VECTOR_3D:
		case OP_TYPE_VECTOR_3D_SCALAR:
			return PORT_TYPE_VECTOR_3D;
		case OP_TYPE_VECTOR_4D:
		case OP_TYPE_VECTOR_4D_SCALAR:
			return PORT_TYPE_VECTOR_4D;
		default:
			return PORT_TYPE_SCALAR;
	}
}

String VisualShaderNodeRemap::get_output_port_name(int p_port) const {
	return "value";
}

void VisualShaderNodeRemap::set_op_type(OpType p_op_type) {
	ERR_FAIL_INDEX(int(p_op_type), int(OP_TYPE_MAX));
	if (op_type == p_op_type) {
		return;
	}
	switch (p_op_type) {
		case OP_TYPE_SCALAR: {
			set_input_port_default_value(0, 0.0, get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 1.0, get_input_port_default_value(2));
			set_input_port_default_value(3, 0.0, get_input_port_default_value(3));
			set_input_port_default_value(4, 1.0, get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_2D: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector2(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector2(1.0, 1.0), get_input_port_default_value(2));
			set_input_port_default_value(3, Vector2(), get_input_port_default_value(3));
			set_input_port_default_value(4, Vector2(1.0, 1.0), get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_2D_SCALAR: {
			set_input_port_default_value(0, Vector2(), get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 1.0, get_input_port_default_value(2));
			set_input_port_default_value(3, 0.0, get_input_port_default_value(3));
			set_input_port_default_value(4, 1.0, get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_3D: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, Vector3(), get_input_port_default_value(1));
			set_input_port_default_value(2, Vector3(1.0, 1.0, 1.0), get_input_port_default_value(2));
			set_input_port_default_value(3, Vector3(), get_input_port_default_value(3));
			set_input_port_default_value(4, Vector3(1.0, 1.0, 1.0), get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_3D_SCALAR: {
			set_input_port_default_value(0, Vector3(), get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 1.0, get_input_port_default_value(2));
			set_input_port_default_value(3, 0.0, get_input_port_default_value(3));
			set_input_port_default_value(4, 1.0, get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_4D: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, Quaternion(), get_input_port_default_value(1));
			set_input_port_default_value(2, Quaternion(1.0, 1.0, 1.0, 1.0), get_input_port_default_value(2));
			set_input_port_default_value(3, Quaternion(), get_input_port_default_value(3));
			set_input_port_default_value(4, Quaternion(1.0, 1.0, 1.0, 1.0), get_input_port_default_value(4));
		} break;
		case OP_TYPE_VECTOR_4D_SCALAR: {
			set_input_port_default_value(0, Quaternion(), get_input_port_default_value(0));
			set_input_port_default_value(1, 0.0, get_input_port_default_value(1));
			set_input_port_default_value(2, 1.0, get_input_port_default_value(2));
			set_input_port_default_value(3, 0.0, get_input_port_default_value(3));
			set_input_port_default_value(4, 1.0, get_input_port_default_value(4));
		} break;
		default:
			break;
	}
	op_type = p_op_type;
	emit_changed();
}

VisualShaderNodeRemap::OpType VisualShaderNodeRemap::get_op_type() const {
	return op_type;
}

String VisualShaderNodeRemap::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	{\n";
	switch (op_type) {
		case OP_TYPE_SCALAR: {
			code += vformat("		float __input_range = %s - %s;\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		float __output_range = %s - %s;\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = %s + __output_range * ((%s - %s) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_2D: {
			code += vformat("		vec2 __input_range = %s - %s;\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec2 __output_range = %s - %s;\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = %s + __output_range * ((%s - %s) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_2D_SCALAR: {
			code += vformat("		vec2 __input_range = vec2(%s - %s);\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec2 __output_range = vec2(%s - %s);\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = vec2(%s) + __output_range * ((%s - vec2(%s)) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_3D: {
			code += vformat("		vec3 __input_range = %s - %s;\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec3 __output_range = %s - %s;\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = %s + __output_range * ((%s - %s) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_3D_SCALAR: {
			code += vformat("		vec3 __input_range = vec3(%s - %s);\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec3 __output_range = vec3(%s - %s);\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = vec3(%s) + __output_range * ((%s - vec3(%s)) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_4D: {
			code += vformat("		vec4 __input_range = %s - %s;\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec4 __output_range = %s - %s;\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = %s + __output_range * ((%s - %s) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		case OP_TYPE_VECTOR_4D_SCALAR: {
			code += vformat("		vec4 __input_range = vec4(%s - %s);\n", p_input_vars[2], p_input_vars[1]);
			code += vformat("		vec4 __output_range = vec4(%s - %s);\n", p_input_vars[4], p_input_vars[3]);
			code += vformat("		%s = vec4(%s) + __output_range * ((%s - vec4(%s)) / __input_range);\n", p_output_vars[0], p_input_vars[3], p_input_vars[0], p_input_vars[1]);
		} break;
		default:
			break;
	}
	code += "	}\n";
	return code;
}

Vector<StringName> VisualShaderNodeRemap::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("op_type");
	return props;
}

void VisualShaderNodeRemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_op_type", "op_type"), &VisualShaderNodeRemap::set_op_type);
	ClassDB::bind_method(D_METHOD("get_op_type"), &VisualShaderNodeRemap::get_op_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "op_type", PROPERTY_HINT_ENUM, "Scalar,Vector2,Vector2Scalar,Vector3,Vector3Scalar,Vector4,Vector4Scalar"), "set_op_type", "get_op_type");

	BIND_ENUM_CONSTANT(OP_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_2D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_3D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(OP_TYPE_VECTOR_4D_SCALAR);
	BIND_ENUM_CONSTANT(OP_TYPE_MAX);
}

VisualShaderNodeRemap::VisualShaderNodeRemap() {
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, 1.0);
	set_input_port_default_value(3, 0.0);
	set_input_port_default_value(4, 1.0);

	simple_decl = false;
}

////////////// RotationByAxis

String VisualShaderNodeRotationByAxis::get_caption() const {
	return "RotationByAxis";
}

int VisualShaderNodeRotationByAxis::get_input_port_count() const {
	return 3;
}

VisualShaderNodeRotationByAxis::PortType VisualShaderNodeRotationByAxis::get_input_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_SCALAR;
		case 2:
			return PORT_TYPE_VECTOR_3D;
		default:
			break;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeRotationByAxis::get_input_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "input";
		case 1:
			return "angle";
		case 2:
			return "axis";
		default:
			break;
	}

	return "";
}

int VisualShaderNodeRotationByAxis::get_output_port_count() const {
	return 2;
}

VisualShaderNodeRotationByAxis::PortType VisualShaderNodeRotationByAxis::get_output_port_type(int p_port) const {
	switch (p_port) {
		case 0:
			return PORT_TYPE_VECTOR_3D;
		case 1:
			return PORT_TYPE_TRANSFORM;
		default:
			break;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeRotationByAxis::get_output_port_name(int p_port) const {
	switch (p_port) {
		case 0:
			return "output";
		case 1:
			return "rotationMat";
		default:
			break;
	}

	return "";
}

bool VisualShaderNodeRotationByAxis::has_output_port_preview(int p_port) const {
	if (p_port == 0) {
		return true;
	}
	return false;
}

String VisualShaderNodeRotationByAxis::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	code += "	{\n";
	code += vformat("		float __angle = %s;\n", p_input_vars[1]);
	code += vformat("		vec3 __axis = normalize(%s);\n", p_input_vars[2]);
	code += vformat("		mat3 __rot_matrix = mat3(\n");
	code += vformat("			vec3( cos(__angle)+__axis.x*__axis.x*(1.0 - cos(__angle)), __axis.x*__axis.y*(1.0-cos(__angle))-__axis.z*sin(__angle), __axis.x*__axis.z*(1.0-cos(__angle))+__axis.y*sin(__angle) ),\n");
	code += vformat("			vec3( __axis.y*__axis.x*(1.0-cos(__angle))+__axis.z*sin(__angle), cos(__angle)+__axis.y*__axis.y*(1.0-cos(__angle)), __axis.y*__axis.z*(1.0-cos(__angle))-__axis.x*sin(__angle) ),\n");
	code += vformat("			vec3( __axis.z*__axis.x*(1.0-cos(__angle))-__axis.y*sin(__angle), __axis.z*__axis.y*(1.0-cos(__angle))+__axis.x*sin(__angle), cos(__angle)+__axis.z*__axis.z*(1.0-cos(__angle)) )\n");
	code += vformat("		);\n");
	if (is_output_port_connected(0)) {
		code += vformat("		%s = %s * __rot_matrix;\n", p_output_vars[0], p_input_vars[0]);
	}
	if (is_output_port_connected(1)) {
		code += vformat("		%s = mat4(__rot_matrix);\n", p_output_vars[1]);
	}
	code += "	}\n";
	return code;
}

VisualShaderNodeRotationByAxis::VisualShaderNodeRotationByAxis() {
	set_input_port_default_value(0, Vector3());
	set_input_port_default_value(1, 0.0);
	set_input_port_default_value(2, Vector3());

	simple_decl = false;
}

String VisualShaderNodeReroute::get_caption() const {
	return "Reroute";
}

int VisualShaderNodeReroute::get_input_port_count() const {
	return 1;
}

VisualShaderNodeReroute::PortType VisualShaderNodeReroute::get_input_port_type(int p_port) const {
	return input_port_type;
}

String VisualShaderNodeReroute::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeReroute::get_output_port_count() const {
	return 1;
}

VisualShaderNodeReroute::PortType VisualShaderNodeReroute::get_output_port_type(int p_port) const {
	return input_port_type;
}

String VisualShaderNodeReroute::get_output_port_name(int p_port) const {
	return String();
}

String VisualShaderNodeReroute::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	for (int i = 0; i < get_output_port_count(); i++) {
		if (input_port_type == PORT_TYPE_SAMPLER) {
			continue;
		}

		String input = p_input_vars[0];
		if (input.is_empty()) {
			code += vformat("	%s;\n", p_output_vars[i]);
			continue;
		}
		code += vformat("	%s = %s;\n", p_output_vars[i], input);
	}
	return code;
}

void VisualShaderNodeReroute::_set_port_type(PortType p_type) {
	input_port_type = p_type;
	switch (p_type) {
		case PORT_TYPE_SCALAR:
			set_input_port_default_value(0, 0.0);
			break;
		case PORT_TYPE_VECTOR_2D:
			set_input_port_default_value(0, Vector2());
			break;
		case PORT_TYPE_VECTOR_3D:
			set_input_port_default_value(0, Vector3());
			break;
		case PORT_TYPE_VECTOR_4D:
			set_input_port_default_value(0, Vector4());
			break;
		case PORT_TYPE_TRANSFORM:
			set_input_port_default_value(0, Transform3D());
			break;
		default:
			break;
	}
}

void VisualShaderNodeReroute::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_set_port_type", "port_type"), &VisualShaderNodeReroute::_set_port_type);
	ClassDB::bind_method(D_METHOD("get_port_type"), &VisualShaderNodeReroute::get_port_type);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "port_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_port_type", "get_port_type");
}

VisualShaderNodeReroute::VisualShaderNodeReroute() {
	set_input_port_default_value(0, 0.0);
}
