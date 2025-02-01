/**************************************************************************/
/*  test_visual_shader.h                                                  */
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

#include "tests/test_macros.h"

namespace TestVisualArray {

TEST_CASE("[SceneTree][VisualShader] Object creation and parameter") {
	Ref<VisualShader> vs = memnew(VisualShader);
	CHECK(vs.is_valid());

	CHECK(vs->get_mode() == Shader::MODE_SPATIAL);

	for (int i = 1; i < Shader::MODE_MAX; i++) {
		vs->set_mode((Shader::Mode)i);
		CHECK(vs->get_mode() == i);
	}
}

TEST_CASE("[SceneTree][VisualShader] Testing VisualShaderNodes") {
	SUBCASE("Testing Node Creation") {
		Ref<VisualShader> vs = memnew(VisualShader);
		CHECK(vs.is_valid());

		for (int i = 0; i < VisualShader::TYPE_MAX; i++) {
			Ref<VisualShaderNode> vsn = memnew(VisualShaderNodeInput);
			CHECK(vsn.is_valid());
			vs->add_node(VisualShader::Type(i), vsn, Vector2(1, 10), i + 2);
			CHECK(vs->get_node(VisualShader::Type(i), i + 2) == vsn);
		}

		ERR_PRINT_OFF;

		// Testing for Invalid entries.
		Ref<VisualShaderNode> vsn5 = memnew(VisualShaderNodeInput);
		Ref<VisualShaderNode> vsn6 = memnew(VisualShaderNodeInput);
		CHECK(vsn6.is_valid());
		CHECK(vsn5.is_valid());

		vs->add_node(VisualShader::TYPE_SKY, vsn5, Vector2(1, 10), 0);
		CHECK_FALSE(vs->get_node(VisualShader::TYPE_SKY, 0) == vsn6);
		vs->add_node(VisualShader::TYPE_MAX, vsn6, Vector2(1, 10), 7);
		CHECK_FALSE(vs->get_node(VisualShader::TYPE_SKY, 7) == vsn6);

		ERR_PRINT_ON;
	}

	SUBCASE("Testing VisualShaderNode position getter and setter") {
		Ref<VisualShader> vs = memnew(VisualShader);
		CHECK(vs.is_valid());

		Ref<VisualShaderNode> vsn1 = memnew(VisualShaderNodeInput);
		CHECK(vsn1.is_valid());
		vs->add_node(VisualShader::TYPE_COLLIDE, vsn1, Vector2(0, 0), 3);
		CHECK(vs->get_node_position(VisualShader::TYPE_COLLIDE, 3) == Vector2(0, 0));
		vs->set_node_position(VisualShader::TYPE_COLLIDE, 3, Vector2(1, 1));
		CHECK(vs->get_node_position(VisualShader::TYPE_COLLIDE, 3) == Vector2(1, 1));

		Ref<VisualShaderNode> vsn2 = memnew(VisualShaderNodeInput);
		CHECK(vsn2.is_valid());
		vs->add_node(VisualShader::TYPE_FOG, vsn2, Vector2(1, 2), 4);
		CHECK(vs->get_node_position(VisualShader::TYPE_FOG, 4) == Vector2(1, 2));
		vs->set_node_position(VisualShader::TYPE_FOG, 4, Vector2(2, 2));
		CHECK(vs->get_node_position(VisualShader::TYPE_FOG, 4) == Vector2(2, 2));
	}

	SUBCASE("Testing VisualShaderNode ID") {
		Ref<VisualShader> vs = memnew(VisualShader);
		CHECK(vs.is_valid());

		for (int i = 0; i < VisualShader::TYPE_MAX; i++) {
			Ref<VisualShaderNode> vsn = memnew(VisualShaderNodeInput);
			CHECK(vsn.is_valid());
			vs->add_node(VisualShader::Type(i), vsn, Vector2(1, 10), i + 2);
			CHECK(vs->get_valid_node_id(VisualShader::Type(i)) - 1 == i + 2);
		}
	}

	SUBCASE("Testing remove and replace VisualShaderNode") {
		Ref<VisualShader> vs = memnew(VisualShader);
		CHECK(vs.is_valid());

		ERR_PRINT_OFF;

		for (int i = 0; i < VisualShader::TYPE_MAX; i++) {
			Ref<VisualShaderNode> vsn = memnew(VisualShaderNodeInput);
			CHECK(vsn.is_valid());
			vs->add_node(VisualShader::Type(i), vsn, Vector2(1, 10), i + 2);
			CHECK(vs->get_node(VisualShader::Type(i), i + 2) == vsn);
			vs->remove_node(VisualShader::Type(i), i + 2);
			CHECK_FALSE(vs->get_node(VisualShader::Type(i), i + 2) == vsn);
		}

		ERR_PRINT_ON;
	}
}

TEST_CASE("[SceneTree][VisualShader] Testing Varyings") {
	Ref<VisualShader> vs = memnew(VisualShader);

	vs->add_varying("Test1", VisualShader::VARYING_MODE_FRAG_TO_LIGHT, VisualShader::VARYING_TYPE_TRANSFORM);
	CHECK(vs->has_varying("Test1") == true);

	vs->add_varying("Test2", VisualShader::VARYING_MODE_VERTEX_TO_FRAG_LIGHT, VisualShader::VARYING_TYPE_VECTOR_2D);
	CHECK(vs->has_varying("Test2"));

	CHECK_FALSE(vs->has_varying("Does_not_exits"));
	ERR_PRINT_OFF;
	vs->add_varying("Test3", VisualShader::VARYING_MODE_MAX, VisualShader::VARYING_TYPE_INT);
	CHECK_FALSE(vs->has_varying("Test3"));

	vs->add_varying("Test4", VisualShader::VARYING_MODE_FRAG_TO_LIGHT, VisualShader::VARYING_TYPE_MAX);
	CHECK_FALSE(vs->has_varying("Test4"));
	ERR_PRINT_ON;
}

} //namespace TestVisualArray
