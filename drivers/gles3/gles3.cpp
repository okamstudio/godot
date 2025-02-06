/**************************************************************************/
/*  gles3.cpp                                                             */
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

#include "gles3.h"

#ifdef GLES3_ENABLED

void GLES3::_bind_methods() {
	ClassDB::bind_static_method("GLES3", D_METHOD("ActiveTexture", "texture"), &GLES3::ActiveTexture);
	ClassDB::bind_static_method("GLES3", D_METHOD("AttachShader", "program", "shader"), &GLES3::AttachShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindAttribLocation", "program", "index", "name"), &GLES3::_BindAttribLocation);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindBuffer", "target", "buffer"), &GLES3::BindBuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindFramebuffer", "target", "framebuffer"), &GLES3::BindFramebuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindRenderbuffer", "target", "renderbuffer"), &GLES3::BindRenderbuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindTexture", "target", "texture"), &GLES3::BindTexture);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlendColor", "red", "green", "blue", "alpha"), &GLES3::BlendColor);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlendEquation", "mode"), &GLES3::BlendEquation);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlendEquationSeparate", "modeRGB", "modeAlpha"), &GLES3::BlendEquationSeparate);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlendFunc", "sfactor", "dfactor"), &GLES3::BlendFunc);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlendFuncSeparate", "sfactorRGB", "dfactorRGB", "sfactorAlpha", "dfactorAlpha"), &GLES3::BlendFuncSeparate);
	ClassDB::bind_static_method("GLES3", D_METHOD("BufferData", "target", "size", "data", "usage"), &GLES3::_BufferData);
	ClassDB::bind_static_method("GLES3", D_METHOD("BufferSubData", "target", "offset", "size", "data"), &GLES3::_BufferSubData);
	ClassDB::bind_static_method("GLES3", D_METHOD("CheckFramebufferStatus", "target"), &GLES3::CheckFramebufferStatus);
	ClassDB::bind_static_method("GLES3", D_METHOD("Clear", "mask"), &GLES3::Clear);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearColor", "red", "green", "blue", "alpha"), &GLES3::ClearColor);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearDepthf", "d"), &GLES3::ClearDepthf);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearStencil", "s"), &GLES3::ClearStencil);
	ClassDB::bind_static_method("GLES3", D_METHOD("ColorMask", "red", "green", "blue", "alpha"), &GLES3::ColorMask);
	ClassDB::bind_static_method("GLES3", D_METHOD("CompileShader", "shader"), &GLES3::CompileShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("CompressedTexImage2D", "target", "level", "internalformat", "width", "height", "border", "imageSize", "data"), &GLES3::_CompressedTexImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CompressedTexSubImage2D", "target", "level", "xoffset", "yoffset", "width", "height", "format", "imageSize", "data"), &GLES3::_CompressedTexSubImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CopyTexImage2D", "target", "level", "internalformat", "x", "y", "width", "height", "border"), &GLES3::CopyTexImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CopyTexSubImage2D", "target", "level", "xoffset", "yoffset", "x", "y", "width", "height"), &GLES3::CopyTexSubImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CreateProgram"), &GLES3::CreateProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("CreateShader", "type"), &GLES3::CreateShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("CullFace", "mode"), &GLES3::CullFace);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteBuffers", "buffers"), &GLES3::_DeleteBuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteFramebuffers", "framebuffers"), &GLES3::_DeleteFramebuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteProgram", "program"), &GLES3::DeleteProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteRenderbuffers", "renderbuffers"), &GLES3::_DeleteRenderbuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteShader", "shader"), &GLES3::DeleteShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteTextures", "textures"), &GLES3::_DeleteTextures);
	ClassDB::bind_static_method("GLES3", D_METHOD("DepthFunc", "func"), &GLES3::DepthFunc);
	ClassDB::bind_static_method("GLES3", D_METHOD("DepthMask", "flag"), &GLES3::DepthMask);
	ClassDB::bind_static_method("GLES3", D_METHOD("DepthRangef", "n", "f"), &GLES3::DepthRangef);
	ClassDB::bind_static_method("GLES3", D_METHOD("DetachShader", "program", "shader"), &GLES3::DetachShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("Disable", "cap"), &GLES3::Disable);
	ClassDB::bind_static_method("GLES3", D_METHOD("DisableVertexAttribArray", "index"), &GLES3::DisableVertexAttribArray);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawArrays", "mode", "first", "count"), &GLES3::DrawArrays);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawElements", "mode", "count", "type", "indices"), &GLES3::_DrawElements);
	ClassDB::bind_static_method("GLES3", D_METHOD("Enable", "cap"), &GLES3::Enable);
	ClassDB::bind_static_method("GLES3", D_METHOD("EnableVertexAttribArray", "index"), &GLES3::EnableVertexAttribArray);
	ClassDB::bind_static_method("GLES3", D_METHOD("Finish"), &GLES3::Finish);
	ClassDB::bind_static_method("GLES3", D_METHOD("Flush"), &GLES3::Flush);
	ClassDB::bind_static_method("GLES3", D_METHOD("FramebufferRenderbuffer", "target", "attachment", "renderbuffertarget", "renderbuffer"), &GLES3::FramebufferRenderbuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("FramebufferTexture2D", "target", "attachment", "textarget", "texture", "level"), &GLES3::FramebufferTexture2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("FrontFace", "mode"), &GLES3::FrontFace);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenBuffers", "n"), &GLES3::_GenBuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenerateMipmap", "target"), &GLES3::GenerateMipmap);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenFramebuffers", "n"), &GLES3::_GenFramebuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenRenderbuffers", "n"), &GLES3::_GenRenderbuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenTextures", "n"), &GLES3::_GenTextures);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetActiveAttrib", "program", "index"), &GLES3::_GetActiveAttrib);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetActiveUniform", "program", "index"), &GLES3::_GetActiveUniform);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetAttachedShaders", "program"), &GLES3::_GetAttachedShaders);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetAttribLocation", "program", "name"), &GLES3::_GetAttribLocation);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetBooleanv", "pname"), &GLES3::_GetBooleanv);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetBufferParameter", "target", "pname"), &GLES3::_GetBufferParameter);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetError"), &GLES3::GetError);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetFloatv", "pname"), &GLES3::_GetFloatv);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetFramebufferAttachmentParameteriv", "target", "attachment", "pname"), &GLES3::_GetFramebufferAttachmentParameter);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetIntegerv", "pname"), &GLES3::_GetIntegerv);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetProgramiv", "program", "pname"), &GLES3::_GetProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetProgramInfoLog", "program"), &GLES3::_GetProgramInfoLog);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetRenderbufferParameteriv", "target", "pname"), &GLES3::_GetRenderbufferParameter);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetShaderiv", "shader", "pname"), &GLES3::_GetShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetShaderInfoLog", "shader"), &GLES3::_GetShaderInfoLog);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetShaderPrecisionFormat", "shadertype", "precisiontype"), &GLES3::_GetShaderPrecisionFormat);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetShaderSource", "shader"), &GLES3::_GetShaderSource);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetTexParameterfv", "target", "pname"), &GLES3::_GetTexParameterf);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetTexParameteriv", "target", "pname"), &GLES3::_GetTexParameteri);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformfv", "program", "location"), &GLES3::_GetUniformf);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformiv", "program", "location"), &GLES3::_GetUniformi);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformLocation", "program", "name"), &GLES3::_GetUniformLocation);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetVertexAttribfv", "index", "pname"), &GLES3::_GetVertexAttribf);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetVertexAttribiv", "index", "pname"), &GLES3::_GetVertexAttribi);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetVertexAttribPointerv", "index", "pname"), &GLES3::_GetVertexAttribPointer);
	ClassDB::bind_static_method("GLES3", D_METHOD("Hint", "target", "mode"), &GLES3::Hint);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsBuffer", "buffer"), &GLES3::IsBuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsEnabled", "cap"), &GLES3::IsEnabled);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsFramebuffer", "framebuffer"), &GLES3::IsFramebuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsProgram", "program"), &GLES3::IsProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsRenderbuffer", "renderbuffer"), &GLES3::IsRenderbuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsShader", "shader"), &GLES3::IsShader);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsTexture", "texture"), &GLES3::IsTexture);
	ClassDB::bind_static_method("GLES3", D_METHOD("LineWidth", "width"), &GLES3::LineWidth);
	ClassDB::bind_static_method("GLES3", D_METHOD("LinkProgram", "program"), &GLES3::LinkProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("PixelStorei", "pname", "param"), &GLES3::PixelStorei);
	ClassDB::bind_static_method("GLES3", D_METHOD("PolygonOffset", "factor", "units"), &GLES3::PolygonOffset);
	//ClassDB::bind_static_method("GLES3", D_METHOD("ReadPixels", "x", "y", "width", "height", "format", "type", "pixels"), &GLES3::ReadPixels); - Dangerous and hard to bind
	ClassDB::bind_static_method("GLES3", D_METHOD("ReleaseShaderCompiler"), &GLES3::ReleaseShaderCompiler);
	ClassDB::bind_static_method("GLES3", D_METHOD("RenderbufferStorage", "target", "internalformat", "width", "height"), &GLES3::RenderbufferStorage);
	ClassDB::bind_static_method("GLES3", D_METHOD("SampleCoverage", "value", "invert"), &GLES3::SampleCoverage);
	ClassDB::bind_static_method("GLES3", D_METHOD("Scissor", "x", "y", "width", "height"), &GLES3::Scissor);
	ClassDB::bind_static_method("GLES3", D_METHOD("ShaderBinary", "count", "binaryFormat", "binary"), &GLES3::_ShaderBinary);
	ClassDB::bind_static_method("GLES3", D_METHOD("ShaderSource", "shader", "string"), &GLES3::_ShaderSource);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilFunc", "func", "ref", "mask"), &GLES3::StencilFunc);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilFuncSeparate", "face", "func", "ref", "mask"), &GLES3::StencilFuncSeparate);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilMask", "mask"), &GLES3::StencilMask);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilMaskSeparate", "face", "mask"), &GLES3::StencilMaskSeparate);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilOp", "fail", "zfail", "zpass"), &GLES3::StencilOp);
	ClassDB::bind_static_method("GLES3", D_METHOD("StencilOpSeparate", "face", "sfail", "dpfail", "dppass"), &GLES3::StencilOpSeparate);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexImage2D", "target", "level", "internalformat", "width", "height", "border", "format", "type", "pixels"), &GLES3::_TexImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexParameterf", "target", "pname", "param"), &GLES3::TexParameterf);
	//ClassDB::bind_static_method("GLES3", D_METHOD("TexParameterfv", "target", "pname", "params"), &GLES3::_TexParameterfv); - unused in GLES3 for more than one parameter.
	ClassDB::bind_static_method("GLES3", D_METHOD("TexParameteri", "target", "pname", "param"), &GLES3::TexParameteri);
	//ClassDB::bind_static_method("GLES3", D_METHOD("TexParameteriv", "target", "pname", "params"), &GLES3::_TexParameteriv); - unused in GLES3 for more than one parameter
	ClassDB::bind_static_method("GLES3", D_METHOD("TexSubImage2D", "target", "level", "xoffset", "yoffset", "width", "height", "format", "type", "pixels"), &GLES3::_TexSubImage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1f", "location", "v0"), &GLES3::Uniform1f);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1fv", "location", "count", "value"), &GLES3::_Uniform1fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1i", "location", "v0"), &GLES3::Uniform1i);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1iv", "location", "count", "value"), &GLES3::_Uniform1iv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2f", "location", "v0", "v1"), &GLES3::Uniform2f);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2fv", "location", "count", "value"), &GLES3::_Uniform2fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2i", "location", "v0", "v1"), &GLES3::Uniform2i);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2iv", "location", "count", "value"), &GLES3::_Uniform2iv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3f", "location", "v0", "v1", "v2"), &GLES3::Uniform3f);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3fv", "location", "count", "value"), &GLES3::_Uniform3fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3i", "location", "v0", "v1", "v2"), &GLES3::Uniform3i);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3iv", "location", "count", "value"), &GLES3::_Uniform3iv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4f", "location", "v0", "v1", "v2", "v3"), &GLES3::Uniform4f);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4fv", "location", "count", "value"), &GLES3::_Uniform4fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4i", "location", "v0", "v1", "v2", "v3"), &GLES3::Uniform4i);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4iv", "location", "count", "value"), &GLES3::_Uniform4iv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix2fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix2fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix3fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix3fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix4fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix4fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UseProgram", "program"), &GLES3::UseProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("ValidateProgram", "program"), &GLES3::ValidateProgram);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib1f", "index", "x"), &GLES3::VertexAttrib1f);
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib1fv", "index", "v"), &GLES3::VertexAttrib1fv); - Not much of a point in binding this, since the non vector functions work as well.
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib2f", "index", "x", "y"), &GLES3::VertexAttrib2f);
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib2fv", "index", "v"), &GLES3::VertexAttrib2fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib3f", "index", "x", "y", "z"), &GLES3::VertexAttrib3f);
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib3fv", "index", "v"), &GLES3::VertexAttrib3fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib4f", "index", "x", "y", "z", "w"), &GLES3::VertexAttrib4f);
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttrib4fv", "index", "v"), &GLES3::VertexAttrib4fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribPointer", "index", "size", "type", "normalized", "stride", "pointer"), &GLES3::_VertexAttribPointer);
	ClassDB::bind_static_method("GLES3", D_METHOD("Viewport", "x", "y", "width", "height"), &GLES3::Viewport);
	ClassDB::bind_static_method("GLES3", D_METHOD("ReadBuffer", "src"), &GLES3::ReadBuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawRangeElements", "mode", "start", "end", "count", "type", "indices"), &GLES3::_DrawRangeElements);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexImage3D", "target", "level", "internalformat", "width", "height", "depth", "border", "format", "type", "pixels"), &GLES3::_TexImage3D);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexSubImage3D", "target", "level", "xoffset", "yoffset", "zoffset", "width", "height", "depth", "format", "type", "pixels"), &GLES3::_TexSubImage3D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CopyTexSubImage3D", "target", "level", "xoffset", "yoffset", "zoffset", "x", "y", "width", "height"), &GLES3::CopyTexSubImage3D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CompressedTexImage3D", "target", "level", "internalformat", "width", "height", "depth", "border", "imageSize", "data"), &GLES3::_CompressedTexImage3D);
	ClassDB::bind_static_method("GLES3", D_METHOD("CompressedTexSubImage3D", "target", "level", "xoffset", "yoffset", "zoffset", "width", "height", "depth", "format", "imageSize", "data"), &GLES3::_CompressedTexSubImage3D);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenQueries", "n"), &GLES3::_GenQueries);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteQueries", "ids"), &GLES3::_DeleteQueries);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsQuery", "id"), &GLES3::IsQuery);
	ClassDB::bind_static_method("GLES3", D_METHOD("BeginQuery", "target", "id"), &GLES3::BeginQuery);
	ClassDB::bind_static_method("GLES3", D_METHOD("EndQuery", "target"), &GLES3::EndQuery);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetQueryiv", "target", "pname"), &GLES3::_GetQueryi);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetQueryObjectuiv", "id", "pname"), &GLES3::_GetQueryObjectui);
	ClassDB::bind_static_method("GLES3", D_METHOD("UnmapBuffer", "target"), &GLES3::UnmapBuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetBufferPointerv", "target", "pname"), &GLES3::_GetBufferPointerv);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawBuffers", "bufs"), &GLES3::_DrawBuffers);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix2x3fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix2x3fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix3x2fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix3x2fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix2x4fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix2x4fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix4x2fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix4x2fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix3x4fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix3x4fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformMatrix4x3fv", "location", "count", "transpose", "value"), &GLES3::_UniformMatrix4x3fv);
	ClassDB::bind_static_method("GLES3", D_METHOD("BlitFramebuffer", "srcX0", "srcY0", "srcX1", "srcY1", "dstX0", "dstY0", "dstX1", "dstY1", "mask", "filter"), &GLES3::BlitFramebuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("RenderbufferStorageMultisample", "target", "samples", "internalformat", "width", "height"), &GLES3::RenderbufferStorageMultisample);
	ClassDB::bind_static_method("GLES3", D_METHOD("FramebufferTextureLayer", "target", "attachment", "texture", "level", "layer"), &GLES3::FramebufferTextureLayer);
	ClassDB::bind_static_method("GLES3", D_METHOD("MapBufferRange", "target", "offset", "length", "access"), &GLES3::MapBufferRange);
	ClassDB::bind_static_method("GLES3", D_METHOD("FlushMappedBufferRange", "target", "offset", "length"), &GLES3::FlushMappedBufferRange);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindVertexArray", "array"), &GLES3::BindVertexArray);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteVertexArrays", "arrays"), &GLES3::_DeleteVertexArrays);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenVertexArrays", "n"), &GLES3::_GenVertexArrays);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsVertexArray", "array"), &GLES3::IsVertexArray);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetIntegeri_v", "target", "index", "data"), &GLES3::GetIntegeri_v); - redundant
	ClassDB::bind_static_method("GLES3", D_METHOD("BeginTransformFeedback", "primitiveMode"), &GLES3::BeginTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("EndTransformFeedback"), &GLES3::EndTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindBufferRange", "target", "index", "buffer", "offset", "size"), &GLES3::BindBufferRange);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindBufferBase", "target", "index", "buffer"), &GLES3::BindBufferBase);
	//ClassDB::bind_static_method("GLES3", D_METHOD("TransformFeedbackVaryings", "program", "count", "varyings", "bufferMode"), &GLES3::TransformFeedbackVaryings); - TODO, pretty complex
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetTransformFeedbackVarying", "program", "index", "bufSize", "length", "size", "type", "name"), &GLES3::GetTransformFeedbackVarying);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribIPointer", "index", "size", "type", "stride", "pointer"), &GLES3::_VertexAttribIPointer);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetVertexAttribIiv", "index", "pname"), &GLES3::_GetVertexAttribIiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetVertexAttribIuiv", "index", "pname"), &GLES3::_GetVertexAttribIuiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribI4i", "index", "x", "y", "z", "w"), &GLES3::VertexAttribI4i);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribI4ui", "index", "x", "y", "z", "w"), &GLES3::VertexAttribI4ui);
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribI4iv", "index", "v"), &GLES3::VertexAttribI4iv); - Redundant, unneeded for binder
	//ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribI4uiv", "index", "v"), &GLES3::VertexAttribI4uiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformuiv", "program", "location"), &GLES3::_GetUniformui);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetFragDataLocation", "program", "name"), &GLES3::_GetFragDataLocation);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1ui", "location", "v0"), &GLES3::Uniform1ui);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2ui", "location", "v0", "v1"), &GLES3::Uniform2ui);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3ui", "location", "v0", "v1", "v2"), &GLES3::Uniform3ui);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4ui", "location", "v0", "v1", "v2", "v3"), &GLES3::Uniform4ui);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform1uiv", "location", "count", "value"), &GLES3::_Uniform1uiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform2uiv", "location", "count", "value"), &GLES3::_Uniform2uiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform3uiv", "location", "count", "value"), &GLES3::_Uniform3uiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("Uniform4uiv", "location", "count", "value"), &GLES3::_Uniform4uiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearBufferiv", "buffer", "drawbuffer", "value"), &GLES3::_ClearBufferiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearBufferuiv", "buffer", "drawbuffer", "value"), &GLES3::_ClearBufferuiv);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearBufferfv", "buffer", "drawbuffer", "value"), &GLES3::_ClearBufferfv);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClearBufferfi", "buffer", "drawbuffer", "depth", "stencil"), &GLES3::ClearBufferfi);
	ClassDB::bind_static_method("GLES3", D_METHOD("CopyBufferSubData", "readTarget", "writeTarget", "readOffset", "writeOffset", "size"), &GLES3::CopyBufferSubData);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformIndices", "program", "uniformCount", "uniformNames", "uniformIndices"), &GLES3::GetUniformIndices); - TODO really complex
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetActiveUniformsiv", "program", "uniformCount", "uniformIndices", "pname", "params"), &GLES3::GetActiveUniformsiv); - Too complex
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetUniformBlockIndex", "program", "uniformBlockName"), &GLES3::GetUniformBlockIndex);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetActiveUniformBlockiv", "program", "uniformBlockIndex", "pname", "params"), &GLES3::GetActiveUniformBlockiv);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetActiveUniformBlockName", "program", "uniformBlockIndex", "bufSize", "length", "uniformBlockName"), &GLES3::GetActiveUniformBlockName);
	ClassDB::bind_static_method("GLES3", D_METHOD("UniformBlockBinding", "program", "uniformBlockIndex", "uniformBlockBinding"), &GLES3::UniformBlockBinding);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawArraysInstanced", "mode", "first", "count", "instancecount"), &GLES3::DrawArraysInstanced);
	ClassDB::bind_static_method("GLES3", D_METHOD("DrawElementsInstanced", "mode", "count", "type", "indices", "instancecount"), &GLES3::_DrawElementsInstanced);
	/*
	ClassDB::bind_static_method("GLES3", D_METHOD("FenceSync", "condition", "flags"), &GLES3::FenceSync);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsSync", "sync"), &GLES3::IsSync);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteSync", "sync"), &GLES3::DeleteSync);
	ClassDB::bind_static_method("GLES3", D_METHOD("ClientWaitSync", "sync", "flags", "timeout"), &GLES3::ClientWaitSync);
	ClassDB::bind_static_method("GLES3", D_METHOD("WaitSync", "sync", "flags", "timeout"), &GLES3::WaitSync);
*/
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetInteger64v", "pname", "data"), &GLES3::GetInteger64v);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetSynciv", "sync", "pname", "count", "length", "values"), &GLES3::GetSynciv);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetInteger64i_v", "target", "index", "data"), &GLES3::GetInteger64i_v);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetBufferParameteri64v", "target", "pname", "params"), &GLES3::GetBufferParameteri64v);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenSamplers", "count"), &GLES3::_GenSamplers);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteSamplers", "samplers"), &GLES3::_DeleteSamplers);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsSampler", "sampler"), &GLES3::IsSampler);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindSampler", "unit", "sampler"), &GLES3::BindSampler);
	ClassDB::bind_static_method("GLES3", D_METHOD("SamplerParameteri", "sampler", "pname", "param"), &GLES3::SamplerParameteri);
	//ClassDB::bind_static_method("GLES3", D_METHOD("SamplerParameteriv", "sampler", "pname", "param"), &GLES3::SamplerParameteriv); - Redundant
	ClassDB::bind_static_method("GLES3", D_METHOD("SamplerParameterf", "sampler", "pname", "param"), &GLES3::SamplerParameterf);
	//ClassDB::bind_static_method("GLES3", D_METHOD("SamplerParameterfv", "sampler", "pname", "param"), &GLES3::SamplerParameterfv); - Redundant
	ClassDB::bind_static_method("GLES3", D_METHOD("GetSamplerParameteri", "sampler", "pname"), &GLES3::_GetSamplerParameteri);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetSamplerParameterf", "sampler", "pname"), &GLES3::_GetSamplerParameterf);
	ClassDB::bind_static_method("GLES3", D_METHOD("VertexAttribDivisor", "index", "divisor"), &GLES3::VertexAttribDivisor);
	ClassDB::bind_static_method("GLES3", D_METHOD("BindTransformFeedback", "target", "id"), &GLES3::BindTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("DeleteTransformFeedbacks", "ids"), &GLES3::_DeleteTransformFeedbacks);
	ClassDB::bind_static_method("GLES3", D_METHOD("GenTransformFeedbacks", "n"), &GLES3::_GenTransformFeedbacks);
	ClassDB::bind_static_method("GLES3", D_METHOD("IsTransformFeedback", "id"), &GLES3::IsTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("PauseTransformFeedback"), &GLES3::PauseTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("ResumeTransformFeedback"), &GLES3::ResumeTransformFeedback);
	ClassDB::bind_static_method("GLES3", D_METHOD("GetProgramBinary", "program", "binaryFormat"), &GLES3::_GetProgramBinary);
	ClassDB::bind_static_method("GLES3", D_METHOD("ProgramBinary", "program", "binaryFormat", "binary"), &GLES3::_ProgramBinary);
	ClassDB::bind_static_method("GLES3", D_METHOD("ProgramParameteri", "program", "pname", "value"), &GLES3::ProgramParameteri);
	ClassDB::bind_static_method("GLES3", D_METHOD("InvalidateFramebuffer", "target", "attachments"), &GLES3::_InvalidateFramebuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("InvalidateSubFramebuffer", "target", "attachments", "x", "y", "width", "height"), &GLES3::_InvalidateSubFramebuffer);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexStorage2D", "target", "levels", "internalformat", "width", "height"), &GLES3::TexStorage2D);
	ClassDB::bind_static_method("GLES3", D_METHOD("TexStorage3D", "target", "levels", "internalformat", "width", "height", "depth"), &GLES3::TexStorage3D);
	//ClassDB::bind_static_method("GLES3", D_METHOD("GetInternalformativ", "target", "internalformat", "pname", "count", "params"), &GLES3::GetInternalformativ); - Complex to support

	BIND_ENUM_CONSTANT(GLES_PROTOTYPES)
	BIND_ENUM_CONSTANT(ES_VERSION_2_0)
	BIND_ENUM_CONSTANT(DEPTH_BUFFER_BIT)
	BIND_ENUM_CONSTANT(STENCIL_BUFFER_BIT)
	BIND_ENUM_CONSTANT(COLOR_BUFFER_BIT)
	BIND_ENUM_CONSTANT(FALSE)
	BIND_ENUM_CONSTANT(TRUE)
	BIND_ENUM_CONSTANT(POINTS)
	BIND_ENUM_CONSTANT(LINES)
	BIND_ENUM_CONSTANT(LINE_LOOP)
	BIND_ENUM_CONSTANT(LINE_STRIP)
	BIND_ENUM_CONSTANT(TRIANGLES)
	BIND_ENUM_CONSTANT(TRIANGLE_STRIP)
	BIND_ENUM_CONSTANT(TRIANGLE_FAN)
	BIND_ENUM_CONSTANT(ZERO)
	BIND_ENUM_CONSTANT(ONE)
	BIND_ENUM_CONSTANT(SRC_COLOR)
	BIND_ENUM_CONSTANT(ONE_MINUS_SRC_COLOR)
	BIND_ENUM_CONSTANT(SRC_ALPHA)
	BIND_ENUM_CONSTANT(ONE_MINUS_SRC_ALPHA)
	BIND_ENUM_CONSTANT(DST_ALPHA)
	BIND_ENUM_CONSTANT(ONE_MINUS_DST_ALPHA)
	BIND_ENUM_CONSTANT(DST_COLOR)
	BIND_ENUM_CONSTANT(ONE_MINUS_DST_COLOR)
	BIND_ENUM_CONSTANT(SRC_ALPHA_SATURATE)
	BIND_ENUM_CONSTANT(FUNC_ADD)
	BIND_ENUM_CONSTANT(BLEND_EQUATION)
	BIND_ENUM_CONSTANT(BLEND_EQUATION_RGB)
	BIND_ENUM_CONSTANT(BLEND_EQUATION_ALPHA)
	BIND_ENUM_CONSTANT(FUNC_SUBTRACT)
	BIND_ENUM_CONSTANT(FUNC_REVERSE_SUBTRACT)
	BIND_ENUM_CONSTANT(BLEND_DST_RGB)
	BIND_ENUM_CONSTANT(BLEND_SRC_RGB)
	BIND_ENUM_CONSTANT(BLEND_DST_ALPHA)
	BIND_ENUM_CONSTANT(BLEND_SRC_ALPHA)
	BIND_ENUM_CONSTANT(CONSTANT_COLOR)
	BIND_ENUM_CONSTANT(ONE_MINUS_CONSTANT_COLOR)
	BIND_ENUM_CONSTANT(CONSTANT_ALPHA)
	BIND_ENUM_CONSTANT(ONE_MINUS_CONSTANT_ALPHA)
	BIND_ENUM_CONSTANT(BLEND_COLOR)
	BIND_ENUM_CONSTANT(ARRAY_BUFFER)
	BIND_ENUM_CONSTANT(ELEMENT_ARRAY_BUFFER)
	BIND_ENUM_CONSTANT(ARRAY_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(ELEMENT_ARRAY_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(STREAM_DRAW)
	BIND_ENUM_CONSTANT(STATIC_DRAW)
	BIND_ENUM_CONSTANT(DYNAMIC_DRAW)
	BIND_ENUM_CONSTANT(BUFFER_SIZE)
	BIND_ENUM_CONSTANT(BUFFER_USAGE)
	BIND_ENUM_CONSTANT(CURRENT_VERTEX_ATTRIB)
	BIND_ENUM_CONSTANT(FRONT)
	BIND_ENUM_CONSTANT(BACK)
	BIND_ENUM_CONSTANT(FRONT_AND_BACK)
	BIND_ENUM_CONSTANT(TEXTURE_2D)
	BIND_ENUM_CONSTANT(CULL_FACE)
	BIND_ENUM_CONSTANT(BLEND)
	BIND_ENUM_CONSTANT(DITHER)
	BIND_ENUM_CONSTANT(STENCIL_TEST)
	BIND_ENUM_CONSTANT(DEPTH_TEST)
	BIND_ENUM_CONSTANT(SCISSOR_TEST)
	BIND_ENUM_CONSTANT(POLYGON_OFFSET_FILL)
	BIND_ENUM_CONSTANT(SAMPLE_ALPHA_TO_COVERAGE)
	BIND_ENUM_CONSTANT(SAMPLE_COVERAGE)
	BIND_ENUM_CONSTANT(NO_ERROR)
	BIND_ENUM_CONSTANT(INVALID_ENUM)
	BIND_ENUM_CONSTANT(INVALID_VALUE)
	BIND_ENUM_CONSTANT(INVALID_OPERATION)
	BIND_ENUM_CONSTANT(OUT_OF_MEMORY)
	BIND_ENUM_CONSTANT(CW)
	BIND_ENUM_CONSTANT(CCW)
	BIND_ENUM_CONSTANT(LINE_WIDTH)
	BIND_ENUM_CONSTANT(ALIASED_POINT_SIZE_RANGE)
	BIND_ENUM_CONSTANT(ALIASED_LINE_WIDTH_RANGE)
	BIND_ENUM_CONSTANT(CULL_FACE_MODE)
	BIND_ENUM_CONSTANT(FRONT_FACE)
	BIND_ENUM_CONSTANT(DEPTH_RANGE)
	BIND_ENUM_CONSTANT(DEPTH_WRITEMASK)
	BIND_ENUM_CONSTANT(DEPTH_CLEAR_VALUE)
	BIND_ENUM_CONSTANT(DEPTH_FUNC)
	BIND_ENUM_CONSTANT(STENCIL_CLEAR_VALUE)
	BIND_ENUM_CONSTANT(STENCIL_FUNC)
	BIND_ENUM_CONSTANT(STENCIL_FAIL)
	BIND_ENUM_CONSTANT(STENCIL_PASS_DEPTH_FAIL)
	BIND_ENUM_CONSTANT(STENCIL_PASS_DEPTH_PASS)
	BIND_ENUM_CONSTANT(STENCIL_REF)
	BIND_ENUM_CONSTANT(STENCIL_VALUE_MASK)
	BIND_ENUM_CONSTANT(STENCIL_WRITEMASK)
	BIND_ENUM_CONSTANT(STENCIL_BACK_FUNC)
	BIND_ENUM_CONSTANT(STENCIL_BACK_FAIL)
	BIND_ENUM_CONSTANT(STENCIL_BACK_PASS_DEPTH_FAIL)
	BIND_ENUM_CONSTANT(STENCIL_BACK_PASS_DEPTH_PASS)
	BIND_ENUM_CONSTANT(STENCIL_BACK_REF)
	BIND_ENUM_CONSTANT(STENCIL_BACK_VALUE_MASK)
	BIND_ENUM_CONSTANT(STENCIL_BACK_WRITEMASK)
	BIND_ENUM_CONSTANT(VIEWPORT)
	BIND_ENUM_CONSTANT(SCISSOR_BOX)
	BIND_ENUM_CONSTANT(COLOR_CLEAR_VALUE)
	BIND_ENUM_CONSTANT(COLOR_WRITEMASK)
	BIND_ENUM_CONSTANT(UNPACK_ALIGNMENT)
	BIND_ENUM_CONSTANT(PACK_ALIGNMENT)
	BIND_ENUM_CONSTANT(MAX_TEXTURE_SIZE)
	BIND_ENUM_CONSTANT(MAX_VIEWPORT_DIMS)
	BIND_ENUM_CONSTANT(SUBPIXEL_BITS)
	BIND_ENUM_CONSTANT(RED_BITS)
	BIND_ENUM_CONSTANT(GREEN_BITS)
	BIND_ENUM_CONSTANT(BLUE_BITS)
	BIND_ENUM_CONSTANT(ALPHA_BITS)
	BIND_ENUM_CONSTANT(DEPTH_BITS)
	BIND_ENUM_CONSTANT(STENCIL_BITS)
	BIND_ENUM_CONSTANT(POLYGON_OFFSET_UNITS)
	BIND_ENUM_CONSTANT(POLYGON_OFFSET_FACTOR)
	BIND_ENUM_CONSTANT(TEXTURE_BINDING_2D)
	BIND_ENUM_CONSTANT(SAMPLE_BUFFERS)
	BIND_ENUM_CONSTANT(SAMPLES)
	BIND_ENUM_CONSTANT(SAMPLE_COVERAGE_VALUE)
	BIND_ENUM_CONSTANT(SAMPLE_COVERAGE_INVERT)
	BIND_ENUM_CONSTANT(NUM_COMPRESSED_TEXTURE_FORMATS)
	BIND_ENUM_CONSTANT(COMPRESSED_TEXTURE_FORMATS)
	BIND_ENUM_CONSTANT(DONT_CARE)
	BIND_ENUM_CONSTANT(FASTEST)
	BIND_ENUM_CONSTANT(NICEST)
	BIND_ENUM_CONSTANT(GENERATE_MIPMAP_HINT)
	BIND_ENUM_CONSTANT(BYTE)
	BIND_ENUM_CONSTANT(UNSIGNED_BYTE)
	BIND_ENUM_CONSTANT(SHORT)
	BIND_ENUM_CONSTANT(UNSIGNED_SHORT)
	BIND_ENUM_CONSTANT(INT)
	BIND_ENUM_CONSTANT(UNSIGNED_INT)
	BIND_ENUM_CONSTANT(FLOAT)
	BIND_ENUM_CONSTANT(FIXED)
	BIND_ENUM_CONSTANT(DEPTH_COMPONENT)
	BIND_ENUM_CONSTANT(ALPHA)
	BIND_ENUM_CONSTANT(RGB)
	BIND_ENUM_CONSTANT(RGBA)
	BIND_ENUM_CONSTANT(LUMINANCE)
	BIND_ENUM_CONSTANT(LUMINANCE_ALPHA)
	BIND_ENUM_CONSTANT(UNSIGNED_SHORT_4_4_4_4)
	BIND_ENUM_CONSTANT(UNSIGNED_SHORT_5_5_5_1)
	BIND_ENUM_CONSTANT(UNSIGNED_SHORT_5_6_5)
	BIND_ENUM_CONSTANT(FRAGMENT_SHADER)
	BIND_ENUM_CONSTANT(VERTEX_SHADER)
	BIND_ENUM_CONSTANT(MAX_VERTEX_ATTRIBS)
	BIND_ENUM_CONSTANT(MAX_VERTEX_UNIFORM_VECTORS)
	BIND_ENUM_CONSTANT(MAX_VARYING_VECTORS)
	BIND_ENUM_CONSTANT(MAX_COMBINED_TEXTURE_IMAGE_UNITS)
	BIND_ENUM_CONSTANT(MAX_VERTEX_TEXTURE_IMAGE_UNITS)
	BIND_ENUM_CONSTANT(MAX_TEXTURE_IMAGE_UNITS)
	BIND_ENUM_CONSTANT(MAX_FRAGMENT_UNIFORM_VECTORS)
	BIND_ENUM_CONSTANT(SHADER_TYPE)
	BIND_ENUM_CONSTANT(DELETE_STATUS)
	BIND_ENUM_CONSTANT(LINK_STATUS)
	BIND_ENUM_CONSTANT(VALIDATE_STATUS)
	BIND_ENUM_CONSTANT(ATTACHED_SHADERS)
	BIND_ENUM_CONSTANT(ACTIVE_UNIFORMS)
	BIND_ENUM_CONSTANT(ACTIVE_UNIFORM_MAX_LENGTH)
	BIND_ENUM_CONSTANT(ACTIVE_ATTRIBUTES)
	BIND_ENUM_CONSTANT(ACTIVE_ATTRIBUTE_MAX_LENGTH)
	BIND_ENUM_CONSTANT(SHADING_LANGUAGE_VERSION)
	BIND_ENUM_CONSTANT(CURRENT_PROGRAM)
	BIND_ENUM_CONSTANT(NEVER)
	BIND_ENUM_CONSTANT(LESS)
	BIND_ENUM_CONSTANT(EQUAL)
	BIND_ENUM_CONSTANT(LEQUAL)
	BIND_ENUM_CONSTANT(GREATER)
	BIND_ENUM_CONSTANT(NOTEQUAL)
	BIND_ENUM_CONSTANT(GEQUAL)
	BIND_ENUM_CONSTANT(ALWAYS)
	BIND_ENUM_CONSTANT(KEEP)
	BIND_ENUM_CONSTANT(REPLACE)
	BIND_ENUM_CONSTANT(INCR)
	BIND_ENUM_CONSTANT(DECR)
	BIND_ENUM_CONSTANT(INVERT)
	BIND_ENUM_CONSTANT(INCR_WRAP)
	BIND_ENUM_CONSTANT(DECR_WRAP)
	BIND_ENUM_CONSTANT(VENDOR)
	BIND_ENUM_CONSTANT(RENDERER)
	BIND_ENUM_CONSTANT(VERSION)
	BIND_ENUM_CONSTANT(EXTENSIONS)
	BIND_ENUM_CONSTANT(NEAREST)
	BIND_ENUM_CONSTANT(LINEAR)
	BIND_ENUM_CONSTANT(NEAREST_MIPMAP_NEAREST)
	BIND_ENUM_CONSTANT(LINEAR_MIPMAP_NEAREST)
	BIND_ENUM_CONSTANT(NEAREST_MIPMAP_LINEAR)
	BIND_ENUM_CONSTANT(LINEAR_MIPMAP_LINEAR)
	BIND_ENUM_CONSTANT(TEXTURE_MAG_FILTER)
	BIND_ENUM_CONSTANT(TEXTURE_MIN_FILTER)
	BIND_ENUM_CONSTANT(TEXTURE_WRAP_S)
	BIND_ENUM_CONSTANT(TEXTURE_WRAP_T)
	BIND_ENUM_CONSTANT(TEXTURE)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP)
	BIND_ENUM_CONSTANT(TEXTURE_BINDING_CUBE_MAP)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_POSITIVE_X)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_NEGATIVE_X)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_POSITIVE_Y)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_NEGATIVE_Y)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_POSITIVE_Z)
	BIND_ENUM_CONSTANT(TEXTURE_CUBE_MAP_NEGATIVE_Z)
	BIND_ENUM_CONSTANT(MAX_CUBE_MAP_TEXTURE_SIZE)
	BIND_ENUM_CONSTANT(TEXTURE0)
	BIND_ENUM_CONSTANT(TEXTURE1)
	BIND_ENUM_CONSTANT(TEXTURE2)
	BIND_ENUM_CONSTANT(TEXTURE3)
	BIND_ENUM_CONSTANT(TEXTURE4)
	BIND_ENUM_CONSTANT(TEXTURE5)
	BIND_ENUM_CONSTANT(TEXTURE6)
	BIND_ENUM_CONSTANT(TEXTURE7)
	BIND_ENUM_CONSTANT(TEXTURE8)
	BIND_ENUM_CONSTANT(TEXTURE9)
	BIND_ENUM_CONSTANT(TEXTURE10)
	BIND_ENUM_CONSTANT(TEXTURE11)
	BIND_ENUM_CONSTANT(TEXTURE12)
	BIND_ENUM_CONSTANT(TEXTURE13)
	BIND_ENUM_CONSTANT(TEXTURE14)
	BIND_ENUM_CONSTANT(TEXTURE15)
	BIND_ENUM_CONSTANT(TEXTURE16)
	BIND_ENUM_CONSTANT(TEXTURE17)
	BIND_ENUM_CONSTANT(TEXTURE18)
	BIND_ENUM_CONSTANT(TEXTURE19)
	BIND_ENUM_CONSTANT(TEXTURE20)
	BIND_ENUM_CONSTANT(TEXTURE21)
	BIND_ENUM_CONSTANT(TEXTURE22)
	BIND_ENUM_CONSTANT(TEXTURE23)
	BIND_ENUM_CONSTANT(TEXTURE24)
	BIND_ENUM_CONSTANT(TEXTURE25)
	BIND_ENUM_CONSTANT(TEXTURE26)
	BIND_ENUM_CONSTANT(TEXTURE27)
	BIND_ENUM_CONSTANT(TEXTURE28)
	BIND_ENUM_CONSTANT(TEXTURE29)
	BIND_ENUM_CONSTANT(TEXTURE30)
	BIND_ENUM_CONSTANT(TEXTURE31)
	BIND_ENUM_CONSTANT(ACTIVE_TEXTURE)
	BIND_ENUM_CONSTANT(REPEAT)
	BIND_ENUM_CONSTANT(CLAMP_TO_EDGE)
	BIND_ENUM_CONSTANT(MIRRORED_REPEAT)
	BIND_ENUM_CONSTANT(FLOAT_VEC2)
	BIND_ENUM_CONSTANT(FLOAT_VEC3)
	BIND_ENUM_CONSTANT(FLOAT_VEC4)
	BIND_ENUM_CONSTANT(INT_VEC2)
	BIND_ENUM_CONSTANT(INT_VEC3)
	BIND_ENUM_CONSTANT(INT_VEC4)
	BIND_ENUM_CONSTANT(BOOL)
	BIND_ENUM_CONSTANT(BOOL_VEC2)
	BIND_ENUM_CONSTANT(BOOL_VEC3)
	BIND_ENUM_CONSTANT(BOOL_VEC4)
	BIND_ENUM_CONSTANT(FLOAT_MAT2)
	BIND_ENUM_CONSTANT(FLOAT_MAT3)
	BIND_ENUM_CONSTANT(FLOAT_MAT4)
	BIND_ENUM_CONSTANT(SAMPLER_2D)
	BIND_ENUM_CONSTANT(SAMPLER_CUBE)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_ENABLED)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_SIZE)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_STRIDE)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_TYPE)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_NORMALIZED)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_POINTER)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(IMPLEMENTATION_COLOR_READ_TYPE)
	BIND_ENUM_CONSTANT(IMPLEMENTATION_COLOR_READ_FORMAT)
	BIND_ENUM_CONSTANT(COMPILE_STATUS)
	BIND_ENUM_CONSTANT(INFO_LOG_LENGTH)
	BIND_ENUM_CONSTANT(SHADER_SOURCE_LENGTH)
	BIND_ENUM_CONSTANT(SHADER_COMPILER)
	BIND_ENUM_CONSTANT(SHADER_BINARY_FORMATS)
	BIND_ENUM_CONSTANT(NUM_SHADER_BINARY_FORMATS)
	BIND_ENUM_CONSTANT(LOW_FLOAT)
	BIND_ENUM_CONSTANT(MEDIUM_FLOAT)
	BIND_ENUM_CONSTANT(HIGH_FLOAT)
	BIND_ENUM_CONSTANT(LOW_INT)
	BIND_ENUM_CONSTANT(MEDIUM_INT)
	BIND_ENUM_CONSTANT(HIGH_INT)
	BIND_ENUM_CONSTANT(FRAMEBUFFER)
	BIND_ENUM_CONSTANT(RENDERBUFFER)
	BIND_ENUM_CONSTANT(RGBA4)
	BIND_ENUM_CONSTANT(RGB5_A1)
	BIND_ENUM_CONSTANT(RGB565)
	BIND_ENUM_CONSTANT(DEPTH_COMPONENT16)
	BIND_ENUM_CONSTANT(STENCIL_INDEX8)
	BIND_ENUM_CONSTANT(RENDERBUFFER_WIDTH)
	BIND_ENUM_CONSTANT(RENDERBUFFER_HEIGHT)
	BIND_ENUM_CONSTANT(RENDERBUFFER_INTERNAL_FORMAT)
	BIND_ENUM_CONSTANT(RENDERBUFFER_RED_SIZE)
	BIND_ENUM_CONSTANT(RENDERBUFFER_GREEN_SIZE)
	BIND_ENUM_CONSTANT(RENDERBUFFER_BLUE_SIZE)
	BIND_ENUM_CONSTANT(RENDERBUFFER_ALPHA_SIZE)
	BIND_ENUM_CONSTANT(RENDERBUFFER_DEPTH_SIZE)
	BIND_ENUM_CONSTANT(RENDERBUFFER_STENCIL_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT0)
	BIND_ENUM_CONSTANT(DEPTH_ATTACHMENT)
	BIND_ENUM_CONSTANT(STENCIL_ATTACHMENT)
	BIND_ENUM_CONSTANT(NONE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_COMPLETE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_INCOMPLETE_DIMENSIONS)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_UNSUPPORTED)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_BINDING)
	BIND_ENUM_CONSTANT(RENDERBUFFER_BINDING)
	BIND_ENUM_CONSTANT(MAX_RENDERBUFFER_SIZE)
	BIND_ENUM_CONSTANT(INVALID_FRAMEBUFFER_OPERATION)
	BIND_ENUM_CONSTANT(ES_VERSION_3_0)
	BIND_ENUM_CONSTANT(READ_BUFFER)
	BIND_ENUM_CONSTANT(UNPACK_ROW_LENGTH)
	BIND_ENUM_CONSTANT(UNPACK_SKIP_ROWS)
	BIND_ENUM_CONSTANT(UNPACK_SKIP_PIXELS)
	BIND_ENUM_CONSTANT(PACK_ROW_LENGTH)
	BIND_ENUM_CONSTANT(PACK_SKIP_ROWS)
	BIND_ENUM_CONSTANT(PACK_SKIP_PIXELS)
	BIND_ENUM_CONSTANT(COLOR)
	BIND_ENUM_CONSTANT(DEPTH)
	BIND_ENUM_CONSTANT(STENCIL)
	BIND_ENUM_CONSTANT(RED)
	BIND_ENUM_CONSTANT(RGB8)
	BIND_ENUM_CONSTANT(RGBA8)
	BIND_ENUM_CONSTANT(RGB10_A2)
	BIND_ENUM_CONSTANT(TEXTURE_BINDING_3D)
	BIND_ENUM_CONSTANT(UNPACK_SKIP_IMAGES)
	BIND_ENUM_CONSTANT(UNPACK_IMAGE_HEIGHT)
	BIND_ENUM_CONSTANT(TEXTURE_3D)
	BIND_ENUM_CONSTANT(TEXTURE_WRAP_R)
	BIND_ENUM_CONSTANT(MAX_3D_TEXTURE_SIZE)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_2_10_10_10_REV)
	BIND_ENUM_CONSTANT(MAX_ELEMENTS_VERTICES)
	BIND_ENUM_CONSTANT(MAX_ELEMENTS_INDICES)
	BIND_ENUM_CONSTANT(TEXTURE_MIN_LOD)
	BIND_ENUM_CONSTANT(TEXTURE_MAX_LOD)
	BIND_ENUM_CONSTANT(TEXTURE_BASE_LEVEL)
	BIND_ENUM_CONSTANT(TEXTURE_MAX_LEVEL)
	BIND_ENUM_CONSTANT(MIN)
	BIND_ENUM_CONSTANT(MAX)
	BIND_ENUM_CONSTANT(DEPTH_COMPONENT24)
	BIND_ENUM_CONSTANT(MAX_TEXTURE_LOD_BIAS)
	BIND_ENUM_CONSTANT(TEXTURE_COMPARE_MODE)
	BIND_ENUM_CONSTANT(TEXTURE_COMPARE_FUNC)
	BIND_ENUM_CONSTANT(CURRENT_QUERY)
	BIND_ENUM_CONSTANT(QUERY_RESULT)
	BIND_ENUM_CONSTANT(QUERY_RESULT_AVAILABLE)
	BIND_ENUM_CONSTANT(BUFFER_MAPPED)
	BIND_ENUM_CONSTANT(BUFFER_MAP_POINTER)
	BIND_ENUM_CONSTANT(STREAM_READ)
	BIND_ENUM_CONSTANT(STREAM_COPY)
	BIND_ENUM_CONSTANT(STATIC_READ)
	BIND_ENUM_CONSTANT(STATIC_COPY)
	BIND_ENUM_CONSTANT(DYNAMIC_READ)
	BIND_ENUM_CONSTANT(DYNAMIC_COPY)
	BIND_ENUM_CONSTANT(MAX_DRAW_BUFFERS)
	BIND_ENUM_CONSTANT(DRAW_BUFFER0)
	BIND_ENUM_CONSTANT(DRAW_BUFFER1)
	BIND_ENUM_CONSTANT(DRAW_BUFFER2)
	BIND_ENUM_CONSTANT(DRAW_BUFFER3)
	BIND_ENUM_CONSTANT(DRAW_BUFFER4)
	BIND_ENUM_CONSTANT(DRAW_BUFFER5)
	BIND_ENUM_CONSTANT(DRAW_BUFFER6)
	BIND_ENUM_CONSTANT(DRAW_BUFFER7)
	BIND_ENUM_CONSTANT(DRAW_BUFFER8)
	BIND_ENUM_CONSTANT(DRAW_BUFFER9)
	BIND_ENUM_CONSTANT(DRAW_BUFFER10)
	BIND_ENUM_CONSTANT(DRAW_BUFFER11)
	BIND_ENUM_CONSTANT(DRAW_BUFFER12)
	BIND_ENUM_CONSTANT(DRAW_BUFFER13)
	BIND_ENUM_CONSTANT(DRAW_BUFFER14)
	BIND_ENUM_CONSTANT(DRAW_BUFFER15)
	BIND_ENUM_CONSTANT(MAX_FRAGMENT_UNIFORM_COMPONENTS)
	BIND_ENUM_CONSTANT(MAX_VERTEX_UNIFORM_COMPONENTS)
	BIND_ENUM_CONSTANT(SAMPLER_3D)
	BIND_ENUM_CONSTANT(SAMPLER_2D_SHADOW)
	BIND_ENUM_CONSTANT(FRAGMENT_SHADER_DERIVATIVE_HINT)
	BIND_ENUM_CONSTANT(PIXEL_PACK_BUFFER)
	BIND_ENUM_CONSTANT(PIXEL_UNPACK_BUFFER)
	BIND_ENUM_CONSTANT(PIXEL_PACK_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(PIXEL_UNPACK_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(FLOAT_MAT2x3)
	BIND_ENUM_CONSTANT(FLOAT_MAT2x4)
	BIND_ENUM_CONSTANT(FLOAT_MAT3x2)
	BIND_ENUM_CONSTANT(FLOAT_MAT3x4)
	BIND_ENUM_CONSTANT(FLOAT_MAT4x2)
	BIND_ENUM_CONSTANT(FLOAT_MAT4x3)
	BIND_ENUM_CONSTANT(SRGB)
	BIND_ENUM_CONSTANT(SRGB8)
	BIND_ENUM_CONSTANT(SRGB8_ALPHA8)
	BIND_ENUM_CONSTANT(COMPARE_REF_TO_TEXTURE)
	BIND_ENUM_CONSTANT(MAJOR_VERSION)
	BIND_ENUM_CONSTANT(MINOR_VERSION)
	BIND_ENUM_CONSTANT(NUM_EXTENSIONS)
	BIND_ENUM_CONSTANT(RGBA32F)
	BIND_ENUM_CONSTANT(RGB32F)
	BIND_ENUM_CONSTANT(RGBA16F)
	BIND_ENUM_CONSTANT(RGB16F)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_INTEGER)
	BIND_ENUM_CONSTANT(MAX_ARRAY_TEXTURE_LAYERS)
	BIND_ENUM_CONSTANT(MIN_PROGRAM_TEXEL_OFFSET)
	BIND_ENUM_CONSTANT(MAX_PROGRAM_TEXEL_OFFSET)
	BIND_ENUM_CONSTANT(MAX_VARYING_COMPONENTS)
	BIND_ENUM_CONSTANT(TEXTURE_2D_ARRAY)
	BIND_ENUM_CONSTANT(TEXTURE_BINDING_2D_ARRAY)
	BIND_ENUM_CONSTANT(R11F_G11F_B10F)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_10F_11F_11F_REV)
	BIND_ENUM_CONSTANT(RGB9_E5)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_5_9_9_9_REV)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BUFFER_MODE)
	BIND_ENUM_CONSTANT(MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_VARYINGS)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BUFFER_START)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BUFFER_SIZE)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)
	BIND_ENUM_CONSTANT(RASTERIZER_DISCARD)
	BIND_ENUM_CONSTANT(MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS)
	BIND_ENUM_CONSTANT(MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS)
	BIND_ENUM_CONSTANT(INTERLEAVED_ATTRIBS)
	BIND_ENUM_CONSTANT(SEPARATE_ATTRIBS)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BUFFER)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(RGBA32UI)
	BIND_ENUM_CONSTANT(RGB32UI)
	BIND_ENUM_CONSTANT(RGBA16UI)
	BIND_ENUM_CONSTANT(RGB16UI)
	BIND_ENUM_CONSTANT(RGBA8UI)
	BIND_ENUM_CONSTANT(RGB8UI)
	BIND_ENUM_CONSTANT(RGBA32I)
	BIND_ENUM_CONSTANT(RGB32I)
	BIND_ENUM_CONSTANT(RGBA16I)
	BIND_ENUM_CONSTANT(RGB16I)
	BIND_ENUM_CONSTANT(RGBA8I)
	BIND_ENUM_CONSTANT(RGB8I)
	BIND_ENUM_CONSTANT(RED_INTEGER)
	BIND_ENUM_CONSTANT(RGB_INTEGER)
	BIND_ENUM_CONSTANT(RGBA_INTEGER)
	BIND_ENUM_CONSTANT(SAMPLER_2D_ARRAY)
	BIND_ENUM_CONSTANT(SAMPLER_2D_ARRAY_SHADOW)
	BIND_ENUM_CONSTANT(SAMPLER_CUBE_SHADOW)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_VEC2)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_VEC3)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_VEC4)
	BIND_ENUM_CONSTANT(INT_SAMPLER_2D)
	BIND_ENUM_CONSTANT(INT_SAMPLER_3D)
	BIND_ENUM_CONSTANT(INT_SAMPLER_CUBE)
	BIND_ENUM_CONSTANT(INT_SAMPLER_2D_ARRAY)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_SAMPLER_2D)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_SAMPLER_3D)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_SAMPLER_CUBE)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_SAMPLER_2D_ARRAY)
	BIND_ENUM_CONSTANT(BUFFER_ACCESS_FLAGS)
	BIND_ENUM_CONSTANT(BUFFER_MAP_LENGTH)
	BIND_ENUM_CONSTANT(BUFFER_MAP_OFFSET)
	BIND_ENUM_CONSTANT(DEPTH_COMPONENT32F)
	BIND_ENUM_CONSTANT(DEPTH32F_STENCIL8)
	BIND_ENUM_CONSTANT(FLOAT_32_UNSIGNED_INT_24_8_REV)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_RED_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_GREEN_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_BLUE_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_DEFAULT)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_UNDEFINED)
	BIND_ENUM_CONSTANT(DEPTH_STENCIL_ATTACHMENT)
	BIND_ENUM_CONSTANT(DEPTH_STENCIL)
	BIND_ENUM_CONSTANT(UNSIGNED_INT_24_8)
	BIND_ENUM_CONSTANT(DEPTH24_STENCIL8)
	BIND_ENUM_CONSTANT(UNSIGNED_NORMALIZED)
	BIND_ENUM_CONSTANT(DRAW_FRAMEBUFFER_BINDING)
	BIND_ENUM_CONSTANT(READ_FRAMEBUFFER)
	BIND_ENUM_CONSTANT(DRAW_FRAMEBUFFER)
	BIND_ENUM_CONSTANT(READ_FRAMEBUFFER_BINDING)
	BIND_ENUM_CONSTANT(RENDERBUFFER_SAMPLES)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER)
	BIND_ENUM_CONSTANT(MAX_COLOR_ATTACHMENTS)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT1)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT2)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT3)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT4)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT5)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT6)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT7)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT8)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT9)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT10)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT11)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT12)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT13)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT14)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT15)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT16)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT17)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT18)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT19)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT20)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT21)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT22)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT23)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT24)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT25)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT26)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT27)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT28)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT29)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT30)
	BIND_ENUM_CONSTANT(COLOR_ATTACHMENT31)
	BIND_ENUM_CONSTANT(FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
	BIND_ENUM_CONSTANT(MAX_SAMPLES)
	BIND_ENUM_CONSTANT(HALF_FLOAT)
	BIND_ENUM_CONSTANT(MAP_READ_BIT)
	BIND_ENUM_CONSTANT(MAP_WRITE_BIT)
	BIND_ENUM_CONSTANT(MAP_INVALIDATE_RANGE_BIT)
	BIND_ENUM_CONSTANT(MAP_INVALIDATE_BUFFER_BIT)
	BIND_ENUM_CONSTANT(MAP_FLUSH_EXPLICIT_BIT)
	BIND_ENUM_CONSTANT(MAP_UNSYNCHRONIZED_BIT)
	BIND_ENUM_CONSTANT(RG)
	BIND_ENUM_CONSTANT(RG_INTEGER)
	BIND_ENUM_CONSTANT(R8)
	BIND_ENUM_CONSTANT(RG8)
	BIND_ENUM_CONSTANT(R16F)
	BIND_ENUM_CONSTANT(R32F)
	BIND_ENUM_CONSTANT(RG16F)
	BIND_ENUM_CONSTANT(RG32F)
	BIND_ENUM_CONSTANT(R8I)
	BIND_ENUM_CONSTANT(R8UI)
	BIND_ENUM_CONSTANT(R16I)
	BIND_ENUM_CONSTANT(R16UI)
	BIND_ENUM_CONSTANT(R32I)
	BIND_ENUM_CONSTANT(R32UI)
	BIND_ENUM_CONSTANT(RG8I)
	BIND_ENUM_CONSTANT(RG8UI)
	BIND_ENUM_CONSTANT(RG16I)
	BIND_ENUM_CONSTANT(RG16UI)
	BIND_ENUM_CONSTANT(RG32I)
	BIND_ENUM_CONSTANT(RG32UI)
	BIND_ENUM_CONSTANT(VERTEX_ARRAY_BINDING)
	BIND_ENUM_CONSTANT(R8_SNORM)
	BIND_ENUM_CONSTANT(RG8_SNORM)
	BIND_ENUM_CONSTANT(RGB8_SNORM)
	BIND_ENUM_CONSTANT(RGBA8_SNORM)
	BIND_ENUM_CONSTANT(SIGNED_NORMALIZED)
	BIND_ENUM_CONSTANT(PRIMITIVE_RESTART_FIXED_INDEX)
	BIND_ENUM_CONSTANT(COPY_READ_BUFFER)
	BIND_ENUM_CONSTANT(COPY_WRITE_BUFFER)
	BIND_ENUM_CONSTANT(COPY_READ_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(COPY_WRITE_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(UNIFORM_BUFFER)
	BIND_ENUM_CONSTANT(UNIFORM_BUFFER_BINDING)
	BIND_ENUM_CONSTANT(UNIFORM_BUFFER_START)
	BIND_ENUM_CONSTANT(UNIFORM_BUFFER_SIZE)
	BIND_ENUM_CONSTANT(MAX_VERTEX_UNIFORM_BLOCKS)
	BIND_ENUM_CONSTANT(MAX_FRAGMENT_UNIFORM_BLOCKS)
	BIND_ENUM_CONSTANT(MAX_COMBINED_UNIFORM_BLOCKS)
	BIND_ENUM_CONSTANT(MAX_UNIFORM_BUFFER_BINDINGS)
	BIND_ENUM_CONSTANT(MAX_UNIFORM_BLOCK_SIZE)
	BIND_ENUM_CONSTANT(MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
	BIND_ENUM_CONSTANT(MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS)
	BIND_ENUM_CONSTANT(UNIFORM_BUFFER_OFFSET_ALIGNMENT)
	BIND_ENUM_CONSTANT(ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH)
	BIND_ENUM_CONSTANT(ACTIVE_UNIFORM_BLOCKS)
	BIND_ENUM_CONSTANT(UNIFORM_TYPE)
	BIND_ENUM_CONSTANT(UNIFORM_SIZE)
	BIND_ENUM_CONSTANT(UNIFORM_NAME_LENGTH)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_INDEX)
	BIND_ENUM_CONSTANT(UNIFORM_OFFSET)
	BIND_ENUM_CONSTANT(UNIFORM_ARRAY_STRIDE)
	BIND_ENUM_CONSTANT(UNIFORM_MATRIX_STRIDE)
	BIND_ENUM_CONSTANT(UNIFORM_IS_ROW_MAJOR)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_BINDING)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_DATA_SIZE)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_NAME_LENGTH)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_ACTIVE_UNIFORMS)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER)
	BIND_ENUM_CONSTANT(UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER)
	BIND_ENUM_CONSTANT(INVALID_INDEX)
	BIND_ENUM_CONSTANT(MAX_VERTEX_OUTPUT_COMPONENTS)
	BIND_ENUM_CONSTANT(MAX_FRAGMENT_INPUT_COMPONENTS)
	BIND_ENUM_CONSTANT(MAX_SERVER_WAIT_TIMEOUT)
	BIND_ENUM_CONSTANT(OBJECT_TYPE)
	BIND_ENUM_CONSTANT(SYNC_CONDITION)
	BIND_ENUM_CONSTANT(SYNC_STATUS)
	BIND_ENUM_CONSTANT(SYNC_FLAGS)
	BIND_ENUM_CONSTANT(SYNC_FENCE)
	BIND_ENUM_CONSTANT(SYNC_GPU_COMMANDS_COMPLETE)
	BIND_ENUM_CONSTANT(UNSIGNALED)
	BIND_ENUM_CONSTANT(SIGNALED)
	BIND_ENUM_CONSTANT(ALREADY_SIGNALED)
	BIND_ENUM_CONSTANT(TIMEOUT_EXPIRED)
	BIND_ENUM_CONSTANT(CONDITION_SATISFIED)
	BIND_ENUM_CONSTANT(WAIT_FAILED)
	BIND_ENUM_CONSTANT(SYNC_FLUSH_COMMANDS_BIT)
	BIND_ENUM_CONSTANT(VERTEX_ATTRIB_ARRAY_DIVISOR)
	BIND_ENUM_CONSTANT(ANY_SAMPLES_PASSED)
	BIND_ENUM_CONSTANT(ANY_SAMPLES_PASSED_CONSERVATIVE)
	BIND_ENUM_CONSTANT(SAMPLER_BINDING)
	BIND_ENUM_CONSTANT(RGB10_A2UI)
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_R)
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_G)
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_B)
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_A)
	BIND_ENUM_CONSTANT(GREEN)
	BIND_ENUM_CONSTANT(BLUE)
	BIND_ENUM_CONSTANT(INT_2_10_10_10_REV)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_PAUSED)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_ACTIVE)
	BIND_ENUM_CONSTANT(TRANSFORM_FEEDBACK_BINDING)
	BIND_ENUM_CONSTANT(PROGRAM_BINARY_RETRIEVABLE_HINT)
	BIND_ENUM_CONSTANT(PROGRAM_BINARY_LENGTH)
	BIND_ENUM_CONSTANT(NUM_PROGRAM_BINARY_FORMATS)
	BIND_ENUM_CONSTANT(PROGRAM_BINARY_FORMATS)
	BIND_ENUM_CONSTANT(COMPRESSED_R11_EAC)
	BIND_ENUM_CONSTANT(COMPRESSED_SIGNED_R11_EAC)
	BIND_ENUM_CONSTANT(COMPRESSED_RG11_EAC)
	BIND_ENUM_CONSTANT(COMPRESSED_SIGNED_RG11_EAC)
	BIND_ENUM_CONSTANT(COMPRESSED_RGB8_ETC2)
	BIND_ENUM_CONSTANT(COMPRESSED_SRGB8_ETC2)
	BIND_ENUM_CONSTANT(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
	BIND_ENUM_CONSTANT(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
	BIND_ENUM_CONSTANT(COMPRESSED_RGBA8_ETC2_EAC)
	BIND_ENUM_CONSTANT(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
	BIND_ENUM_CONSTANT(TEXTURE_IMMUTABLE_FORMAT)
	BIND_ENUM_CONSTANT(MAX_ELEMENT_INDEX)
	BIND_ENUM_CONSTANT(NUM_SAMPLE_COUNTS)
	BIND_ENUM_CONSTANT(TEXTURE_IMMUTABLE_LEVELS)
}

GLES3::GetStruct GLES3::get_structs[] = {
	{ GLES3::ALIASED_LINE_WIDTH_RANGE, 2, 0 },
	{ GLES3::ALIASED_POINT_SIZE_RANGE, 2, 0 },
	{ GLES3::BLEND_COLOR, 4, 0 },
	{ GLES3::COLOR_CLEAR_VALUE, 4, 0 },
	{ GLES3::COMPRESSED_TEXTURE_FORMATS, 0, GLES3::NUM_COMPRESSED_TEXTURE_FORMATS },
	{ GLES3::DEPTH_RANGE, 2, 0 },
	{ GLES3::MAX_VIEWPORT_DIMS, 2, 0 },
	{ GLES3::PROGRAM_BINARY_FORMATS, 0, GLES3::NUM_PROGRAM_BINARY_FORMATS },
	{ GLES3::MAX_VIEWPORT_DIMS, 4, 0 },
	{ GLES3::SHADER_BINARY_FORMATS, 0, GLES3::NUM_SHADER_BINARY_FORMATS },
	{ GLES3::MAX_VIEWPORT_DIMS, 4, 0 },
	{ 0, 0, 0 }
};

#endif