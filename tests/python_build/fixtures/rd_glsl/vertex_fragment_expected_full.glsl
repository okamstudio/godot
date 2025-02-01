/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */
#pragma once

#include "servers/rendering/renderer_rd/shader_rd.h"

class VertexFragmentShaderRD : public ShaderRD {

public:

	VertexFragmentShaderRD() {

		static const char _vertex_code[] = {
R"<!>(
#version 450

#VERSION_DEFINES

#define M_PI 3.14159265359

layout(location = 0) out vec2 uv_interp;

void main() {
	uv_interp = vec2(0, 1);
}

)<!>"
		};
		static const char _fragment_code[] = {
R"<!>(
#version 450

#VERSION_DEFINES

layout(location = 0) in vec2 uv_interp;

void main() {
	uv_interp = vec2(1, 0);
}
)<!>"
		};
		setup(_vertex_code, _fragment_code, nullptr, "VertexFragmentShaderRD");
	}
};
