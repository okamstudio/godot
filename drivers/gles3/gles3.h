/**************************************************************************/
/*  gles3.h                                                               */
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

#ifndef GLES3_H
#define GLES3_H

#ifdef GLES3_ENABLED

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "platform_gl.h"

class GLES3 : public Object {
	GDCLASS(GLES3, Object)
protected:
	static void _bind_methods();

public:
	enum GLEnum : unsigned int {
		GLES_PROTOTYPES = 1,
		ES_VERSION_2_0 = 1,
		DEPTH_BUFFER_BIT = 0x00000100,
		STENCIL_BUFFER_BIT = 0x00000400,
		COLOR_BUFFER_BIT = 0x00004000,
		FALSE = 0,
		TRUE = 1,
		POINTS = 0x0000,
		LINES = 0x0001,
		LINE_LOOP = 0x0002,
		LINE_STRIP = 0x0003,
		TRIANGLES = 0x0004,
		TRIANGLE_STRIP = 0x0005,
		TRIANGLE_FAN = 0x0006,
		ZERO = 0,
		ONE = 1,
		SRC_COLOR = 0x0300,
		ONE_MINUS_SRC_COLOR = 0x0301,
		SRC_ALPHA = 0x0302,
		ONE_MINUS_SRC_ALPHA = 0x0303,
		DST_ALPHA = 0x0304,
		ONE_MINUS_DST_ALPHA = 0x0305,
		DST_COLOR = 0x0306,
		ONE_MINUS_DST_COLOR = 0x0307,
		SRC_ALPHA_SATURATE = 0x0308,
		FUNC_ADD = 0x8006,
		BLEND_EQUATION = 0x8009,
		BLEND_EQUATION_RGB = 0x8009,
		BLEND_EQUATION_ALPHA = 0x883D,
		FUNC_SUBTRACT = 0x800A,
		FUNC_REVERSE_SUBTRACT = 0x800B,
		BLEND_DST_RGB = 0x80C8,
		BLEND_SRC_RGB = 0x80C9,
		BLEND_DST_ALPHA = 0x80CA,
		BLEND_SRC_ALPHA = 0x80CB,
		CONSTANT_COLOR = 0x8001,
		ONE_MINUS_CONSTANT_COLOR = 0x8002,
		CONSTANT_ALPHA = 0x8003,
		ONE_MINUS_CONSTANT_ALPHA = 0x8004,
		BLEND_COLOR = 0x8005,
		ARRAY_BUFFER = 0x8892,
		ELEMENT_ARRAY_BUFFER = 0x8893,
		ARRAY_BUFFER_BINDING = 0x8894,
		ELEMENT_ARRAY_BUFFER_BINDING = 0x8895,
		STREAM_DRAW = 0x88E0,
		STATIC_DRAW = 0x88E4,
		DYNAMIC_DRAW = 0x88E8,
		BUFFER_SIZE = 0x8764,
		BUFFER_USAGE = 0x8765,
		CURRENT_VERTEX_ATTRIB = 0x8626,
		FRONT = 0x0404,
		BACK = 0x0405,
		FRONT_AND_BACK = 0x0408,
		TEXTURE_2D = 0x0DE1,
		CULL_FACE = 0x0B44,
		BLEND = 0x0BE2,
		DITHER = 0x0BD0,
		STENCIL_TEST = 0x0B90,
		DEPTH_TEST = 0x0B71,
		SCISSOR_TEST = 0x0C11,
		POLYGON_OFFSET_FILL = 0x8037,
		SAMPLE_ALPHA_TO_COVERAGE = 0x809E,
		SAMPLE_COVERAGE = 0x80A0,
		NO_ERROR = 0,
		INVALID_ENUM = 0x0500,
		INVALID_VALUE = 0x0501,
		INVALID_OPERATION = 0x0502,
		OUT_OF_MEMORY = 0x0505,
		CW = 0x0900,
		CCW = 0x0901,
		LINE_WIDTH = 0x0B21,
		ALIASED_POINT_SIZE_RANGE = 0x846D,
		ALIASED_LINE_WIDTH_RANGE = 0x846E,
		CULL_FACE_MODE = 0x0B45,
		FRONT_FACE = 0x0B46,
		DEPTH_RANGE = 0x0B70,
		DEPTH_WRITEMASK = 0x0B72,
		DEPTH_CLEAR_VALUE = 0x0B73,
		DEPTH_FUNC = 0x0B74,
		STENCIL_CLEAR_VALUE = 0x0B91,
		STENCIL_FUNC = 0x0B92,
		STENCIL_FAIL = 0x0B94,
		STENCIL_PASS_DEPTH_FAIL = 0x0B95,
		STENCIL_PASS_DEPTH_PASS = 0x0B96,
		STENCIL_REF = 0x0B97,
		STENCIL_VALUE_MASK = 0x0B93,
		STENCIL_WRITEMASK = 0x0B98,
		STENCIL_BACK_FUNC = 0x8800,
		STENCIL_BACK_FAIL = 0x8801,
		STENCIL_BACK_PASS_DEPTH_FAIL = 0x8802,
		STENCIL_BACK_PASS_DEPTH_PASS = 0x8803,
		STENCIL_BACK_REF = 0x8CA3,
		STENCIL_BACK_VALUE_MASK = 0x8CA4,
		STENCIL_BACK_WRITEMASK = 0x8CA5,
		VIEWPORT = 0x0BA2,
		SCISSOR_BOX = 0x0C10,
		COLOR_CLEAR_VALUE = 0x0C22,
		COLOR_WRITEMASK = 0x0C23,
		UNPACK_ALIGNMENT = 0x0CF5,
		PACK_ALIGNMENT = 0x0D05,
		MAX_TEXTURE_SIZE = 0x0D33,
		MAX_VIEWPORT_DIMS = 0x0D3A,
		SUBPIXEL_BITS = 0x0D50,
		RED_BITS = 0x0D52,
		GREEN_BITS = 0x0D53,
		BLUE_BITS = 0x0D54,
		ALPHA_BITS = 0x0D55,
		DEPTH_BITS = 0x0D56,
		STENCIL_BITS = 0x0D57,
		POLYGON_OFFSET_UNITS = 0x2A00,
		POLYGON_OFFSET_FACTOR = 0x8038,
		TEXTURE_BINDING_2D = 0x8069,
		SAMPLE_BUFFERS = 0x80A8,
		SAMPLES = 0x80A9,
		SAMPLE_COVERAGE_VALUE = 0x80AA,
		SAMPLE_COVERAGE_INVERT = 0x80AB,
		NUM_COMPRESSED_TEXTURE_FORMATS = 0x86A2,
		COMPRESSED_TEXTURE_FORMATS = 0x86A3,
		DONT_CARE = 0x1100,
		FASTEST = 0x1101,
		NICEST = 0x1102,
		GENERATE_MIPMAP_HINT = 0x8192,
		BYTE = 0x1400,
		UNSIGNED_BYTE = 0x1401,
		SHORT = 0x1402,
		UNSIGNED_SHORT = 0x1403,
		INT = 0x1404,
		UNSIGNED_INT = 0x1405,
		FLOAT = 0x1406,
		FIXED = 0x140C,
		DEPTH_COMPONENT = 0x1902,
		ALPHA = 0x1906,
		RGB = 0x1907,
		RGBA = 0x1908,
		LUMINANCE = 0x1909,
		LUMINANCE_ALPHA = 0x190A,
		UNSIGNED_SHORT_4_4_4_4 = 0x8033,
		UNSIGNED_SHORT_5_5_5_1 = 0x8034,
		UNSIGNED_SHORT_5_6_5 = 0x8363,
		FRAGMENT_SHADER = 0x8B30,
		VERTEX_SHADER = 0x8B31,
		MAX_VERTEX_ATTRIBS = 0x8869,
		MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
		MAX_VARYING_VECTORS = 0x8DFC,
		MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D,
		MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C,
		MAX_TEXTURE_IMAGE_UNITS = 0x8872,
		MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD,
		SHADER_TYPE = 0x8B4F,
		DELETE_STATUS = 0x8B80,
		LINK_STATUS = 0x8B82,
		VALIDATE_STATUS = 0x8B83,
		ATTACHED_SHADERS = 0x8B85,
		ACTIVE_UNIFORMS = 0x8B86,
		ACTIVE_UNIFORM_MAX_LENGTH = 0x8B87,
		ACTIVE_ATTRIBUTES = 0x8B89,
		ACTIVE_ATTRIBUTE_MAX_LENGTH = 0x8B8A,
		SHADING_LANGUAGE_VERSION = 0x8B8C,
		CURRENT_PROGRAM = 0x8B8D,
		NEVER = 0x0200,
		LESS = 0x0201,
		EQUAL = 0x0202,
		LEQUAL = 0x0203,
		GREATER = 0x0204,
		NOTEQUAL = 0x0205,
		GEQUAL = 0x0206,
		ALWAYS = 0x0207,
		KEEP = 0x1E00,
		REPLACE = 0x1E01,
		INCR = 0x1E02,
		DECR = 0x1E03,
		INVERT = 0x150A,
		INCR_WRAP = 0x8507,
		DECR_WRAP = 0x8508,
		VENDOR = 0x1F00,
		RENDERER = 0x1F01,
		VERSION = 0x1F02,
		EXTENSIONS = 0x1F03,
		NEAREST = 0x2600,
		LINEAR = 0x2601,
		NEAREST_MIPMAP_NEAREST = 0x2700,
		LINEAR_MIPMAP_NEAREST = 0x2701,
		NEAREST_MIPMAP_LINEAR = 0x2702,
		LINEAR_MIPMAP_LINEAR = 0x2703,
		TEXTURE_MAG_FILTER = 0x2800,
		TEXTURE_MIN_FILTER = 0x2801,
		TEXTURE_WRAP_S = 0x2802,
		TEXTURE_WRAP_T = 0x2803,
		TEXTURE = 0x1702,
		TEXTURE_CUBE_MAP = 0x8513,
		TEXTURE_BINDING_CUBE_MAP = 0x8514,
		TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515,
		TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516,
		TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517,
		TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518,
		TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519,
		TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A,
		MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C,
		TEXTURE0 = 0x84C0,
		TEXTURE1 = 0x84C1,
		TEXTURE2 = 0x84C2,
		TEXTURE3 = 0x84C3,
		TEXTURE4 = 0x84C4,
		TEXTURE5 = 0x84C5,
		TEXTURE6 = 0x84C6,
		TEXTURE7 = 0x84C7,
		TEXTURE8 = 0x84C8,
		TEXTURE9 = 0x84C9,
		TEXTURE10 = 0x84CA,
		TEXTURE11 = 0x84CB,
		TEXTURE12 = 0x84CC,
		TEXTURE13 = 0x84CD,
		TEXTURE14 = 0x84CE,
		TEXTURE15 = 0x84CF,
		TEXTURE16 = 0x84D0,
		TEXTURE17 = 0x84D1,
		TEXTURE18 = 0x84D2,
		TEXTURE19 = 0x84D3,
		TEXTURE20 = 0x84D4,
		TEXTURE21 = 0x84D5,
		TEXTURE22 = 0x84D6,
		TEXTURE23 = 0x84D7,
		TEXTURE24 = 0x84D8,
		TEXTURE25 = 0x84D9,
		TEXTURE26 = 0x84DA,
		TEXTURE27 = 0x84DB,
		TEXTURE28 = 0x84DC,
		TEXTURE29 = 0x84DD,
		TEXTURE30 = 0x84DE,
		TEXTURE31 = 0x84DF,
		ACTIVE_TEXTURE = 0x84E0,
		REPEAT = 0x2901,
		CLAMP_TO_EDGE = 0x812F,
		MIRRORED_REPEAT = 0x8370,
		FLOAT_VEC2 = 0x8B50,
		FLOAT_VEC3 = 0x8B51,
		FLOAT_VEC4 = 0x8B52,
		INT_VEC2 = 0x8B53,
		INT_VEC3 = 0x8B54,
		INT_VEC4 = 0x8B55,
		BOOL = 0x8B56,
		BOOL_VEC2 = 0x8B57,
		BOOL_VEC3 = 0x8B58,
		BOOL_VEC4 = 0x8B59,
		FLOAT_MAT2 = 0x8B5A,
		FLOAT_MAT3 = 0x8B5B,
		FLOAT_MAT4 = 0x8B5C,
		SAMPLER_2D = 0x8B5E,
		SAMPLER_CUBE = 0x8B60,
		VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622,
		VERTEX_ATTRIB_ARRAY_SIZE = 0x8623,
		VERTEX_ATTRIB_ARRAY_STRIDE = 0x8624,
		VERTEX_ATTRIB_ARRAY_TYPE = 0x8625,
		VERTEX_ATTRIB_ARRAY_NORMALIZED = 0x886A,
		VERTEX_ATTRIB_ARRAY_POINTER = 0x8645,
		VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F,
		IMPLEMENTATION_COLOR_READ_TYPE = 0x8B9A,
		IMPLEMENTATION_COLOR_READ_FORMAT = 0x8B9B,
		COMPILE_STATUS = 0x8B81,
		INFO_LOG_LENGTH = 0x8B84,
		SHADER_SOURCE_LENGTH = 0x8B88,
		SHADER_COMPILER = 0x8DFA,
		SHADER_BINARY_FORMATS = 0x8DF8,
		NUM_SHADER_BINARY_FORMATS = 0x8DF9,
		LOW_FLOAT = 0x8DF0,
		MEDIUM_FLOAT = 0x8DF1,
		HIGH_FLOAT = 0x8DF2,
		LOW_INT = 0x8DF3,
		MEDIUM_INT = 0x8DF4,
		HIGH_INT = 0x8DF5,
		FRAMEBUFFER = 0x8D40,
		RENDERBUFFER = 0x8D41,
		RGBA4 = 0x8056,
		RGB5_A1 = 0x8057,
		RGB565 = 0x8D62,
		DEPTH_COMPONENT16 = 0x81A5,
		STENCIL_INDEX8 = 0x8D48,
		RENDERBUFFER_WIDTH = 0x8D42,
		RENDERBUFFER_HEIGHT = 0x8D43,
		RENDERBUFFER_INTERNAL_FORMAT = 0x8D44,
		RENDERBUFFER_RED_SIZE = 0x8D50,
		RENDERBUFFER_GREEN_SIZE = 0x8D51,
		RENDERBUFFER_BLUE_SIZE = 0x8D52,
		RENDERBUFFER_ALPHA_SIZE = 0x8D53,
		RENDERBUFFER_DEPTH_SIZE = 0x8D54,
		RENDERBUFFER_STENCIL_SIZE = 0x8D55,
		FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0,
		FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1,
		FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL = 0x8CD2,
		FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE = 0x8CD3,
		COLOR_ATTACHMENT0 = 0x8CE0,
		DEPTH_ATTACHMENT = 0x8D00,
		STENCIL_ATTACHMENT = 0x8D20,
		NONE = 0,
		FRAMEBUFFER_COMPLETE = 0x8CD5,
		FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6,
		FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7,
		FRAMEBUFFER_INCOMPLETE_DIMENSIONS = 0x8CD9,
		FRAMEBUFFER_UNSUPPORTED = 0x8CDD,
		FRAMEBUFFER_BINDING = 0x8CA6,
		RENDERBUFFER_BINDING = 0x8CA7,
		MAX_RENDERBUFFER_SIZE = 0x84E8,
		INVALID_FRAMEBUFFER_OPERATION = 0x0506,
		ES_VERSION_3_0 = 1,
		READ_BUFFER = 0x0C02,
		UNPACK_ROW_LENGTH = 0x0CF2,
		UNPACK_SKIP_ROWS = 0x0CF3,
		UNPACK_SKIP_PIXELS = 0x0CF4,
		PACK_ROW_LENGTH = 0x0D02,
		PACK_SKIP_ROWS = 0x0D03,
		PACK_SKIP_PIXELS = 0x0D04,
		COLOR = 0x1800,
		DEPTH = 0x1801,
		STENCIL = 0x1802,
		RED = 0x1903,
		RGB8 = 0x8051,
		RGBA8 = 0x8058,
		RGB10_A2 = 0x8059,
		TEXTURE_BINDING_3D = 0x806A,
		UNPACK_SKIP_IMAGES = 0x806D,
		UNPACK_IMAGE_HEIGHT = 0x806E,
		TEXTURE_3D = 0x806F,
		TEXTURE_WRAP_R = 0x8072,
		MAX_3D_TEXTURE_SIZE = 0x8073,
		UNSIGNED_INT_2_10_10_10_REV = 0x8368,
		MAX_ELEMENTS_VERTICES = 0x80E8,
		MAX_ELEMENTS_INDICES = 0x80E9,
		TEXTURE_MIN_LOD = 0x813A,
		TEXTURE_MAX_LOD = 0x813B,
		TEXTURE_BASE_LEVEL = 0x813C,
		TEXTURE_MAX_LEVEL = 0x813D,
		MIN = 0x8007,
		MAX = 0x8008,
		DEPTH_COMPONENT24 = 0x81A6,
		MAX_TEXTURE_LOD_BIAS = 0x84FD,
		TEXTURE_COMPARE_MODE = 0x884C,
		TEXTURE_COMPARE_FUNC = 0x884D,
		CURRENT_QUERY = 0x8865,
		QUERY_RESULT = 0x8866,
		QUERY_RESULT_AVAILABLE = 0x8867,
		BUFFER_MAPPED = 0x88BC,
		BUFFER_MAP_POINTER = 0x88BD,
		STREAM_READ = 0x88E1,
		STREAM_COPY = 0x88E2,
		STATIC_READ = 0x88E5,
		STATIC_COPY = 0x88E6,
		DYNAMIC_READ = 0x88E9,
		DYNAMIC_COPY = 0x88EA,
		MAX_DRAW_BUFFERS = 0x8824,
		DRAW_BUFFER0 = 0x8825,
		DRAW_BUFFER1 = 0x8826,
		DRAW_BUFFER2 = 0x8827,
		DRAW_BUFFER3 = 0x8828,
		DRAW_BUFFER4 = 0x8829,
		DRAW_BUFFER5 = 0x882A,
		DRAW_BUFFER6 = 0x882B,
		DRAW_BUFFER7 = 0x882C,
		DRAW_BUFFER8 = 0x882D,
		DRAW_BUFFER9 = 0x882E,
		DRAW_BUFFER10 = 0x882F,
		DRAW_BUFFER11 = 0x8830,
		DRAW_BUFFER12 = 0x8831,
		DRAW_BUFFER13 = 0x8832,
		DRAW_BUFFER14 = 0x8833,
		DRAW_BUFFER15 = 0x8834,
		MAX_FRAGMENT_UNIFORM_COMPONENTS = 0x8B49,
		MAX_VERTEX_UNIFORM_COMPONENTS = 0x8B4A,
		SAMPLER_3D = 0x8B5F,
		SAMPLER_2D_SHADOW = 0x8B62,
		FRAGMENT_SHADER_DERIVATIVE_HINT = 0x8B8B,
		PIXEL_PACK_BUFFER = 0x88EB,
		PIXEL_UNPACK_BUFFER = 0x88EC,
		PIXEL_PACK_BUFFER_BINDING = 0x88ED,
		PIXEL_UNPACK_BUFFER_BINDING = 0x88EF,
		FLOAT_MAT2x3 = 0x8B65,
		FLOAT_MAT2x4 = 0x8B66,
		FLOAT_MAT3x2 = 0x8B67,
		FLOAT_MAT3x4 = 0x8B68,
		FLOAT_MAT4x2 = 0x8B69,
		FLOAT_MAT4x3 = 0x8B6A,
		SRGB = 0x8C40,
		SRGB8 = 0x8C41,
		SRGB8_ALPHA8 = 0x8C43,
		COMPARE_REF_TO_TEXTURE = 0x884E,
		MAJOR_VERSION = 0x821B,
		MINOR_VERSION = 0x821C,
		NUM_EXTENSIONS = 0x821D,
		RGBA32F = 0x8814,
		RGB32F = 0x8815,
		RGBA16F = 0x881A,
		RGB16F = 0x881B,
		VERTEX_ATTRIB_ARRAY_INTEGER = 0x88FD,
		MAX_ARRAY_TEXTURE_LAYERS = 0x88FF,
		MIN_PROGRAM_TEXEL_OFFSET = 0x8904,
		MAX_PROGRAM_TEXEL_OFFSET = 0x8905,
		MAX_VARYING_COMPONENTS = 0x8B4B,
		TEXTURE_2D_ARRAY = 0x8C1A,
		TEXTURE_BINDING_2D_ARRAY = 0x8C1D,
		R11F_G11F_B10F = 0x8C3A,
		UNSIGNED_INT_10F_11F_11F_REV = 0x8C3B,
		RGB9_E5 = 0x8C3D,
		UNSIGNED_INT_5_9_9_9_REV = 0x8C3E,
		TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH = 0x8C76,
		TRANSFORM_FEEDBACK_BUFFER_MODE = 0x8C7F,
		MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS = 0x8C80,
		TRANSFORM_FEEDBACK_VARYINGS = 0x8C83,
		TRANSFORM_FEEDBACK_BUFFER_START = 0x8C84,
		TRANSFORM_FEEDBACK_BUFFER_SIZE = 0x8C85,
		TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88,
		RASTERIZER_DISCARD = 0x8C89,
		MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS = 0x8C8A,
		MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS = 0x8C8B,
		INTERLEAVED_ATTRIBS = 0x8C8C,
		SEPARATE_ATTRIBS = 0x8C8D,
		TRANSFORM_FEEDBACK_BUFFER = 0x8C8E,
		TRANSFORM_FEEDBACK_BUFFER_BINDING = 0x8C8F,
		RGBA32UI = 0x8D70,
		RGB32UI = 0x8D71,
		RGBA16UI = 0x8D76,
		RGB16UI = 0x8D77,
		RGBA8UI = 0x8D7C,
		RGB8UI = 0x8D7D,
		RGBA32I = 0x8D82,
		RGB32I = 0x8D83,
		RGBA16I = 0x8D88,
		RGB16I = 0x8D89,
		RGBA8I = 0x8D8E,
		RGB8I = 0x8D8F,
		RED_INTEGER = 0x8D94,
		RGB_INTEGER = 0x8D98,
		RGBA_INTEGER = 0x8D99,
		SAMPLER_2D_ARRAY = 0x8DC1,
		SAMPLER_2D_ARRAY_SHADOW = 0x8DC4,
		SAMPLER_CUBE_SHADOW = 0x8DC5,
		UNSIGNED_INT_VEC2 = 0x8DC6,
		UNSIGNED_INT_VEC3 = 0x8DC7,
		UNSIGNED_INT_VEC4 = 0x8DC8,
		INT_SAMPLER_2D = 0x8DCA,
		INT_SAMPLER_3D = 0x8DCB,
		INT_SAMPLER_CUBE = 0x8DCC,
		INT_SAMPLER_2D_ARRAY = 0x8DCF,
		UNSIGNED_INT_SAMPLER_2D = 0x8DD2,
		UNSIGNED_INT_SAMPLER_3D = 0x8DD3,
		UNSIGNED_INT_SAMPLER_CUBE = 0x8DD4,
		UNSIGNED_INT_SAMPLER_2D_ARRAY = 0x8DD7,
		BUFFER_ACCESS_FLAGS = 0x911F,
		BUFFER_MAP_LENGTH = 0x9120,
		BUFFER_MAP_OFFSET = 0x9121,
		DEPTH_COMPONENT32F = 0x8CAC,
		DEPTH32F_STENCIL8 = 0x8CAD,
		FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD,
		FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING = 0x8210,
		FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211,
		FRAMEBUFFER_ATTACHMENT_RED_SIZE = 0x8212,
		FRAMEBUFFER_ATTACHMENT_GREEN_SIZE = 0x8213,
		FRAMEBUFFER_ATTACHMENT_BLUE_SIZE = 0x8214,
		FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE = 0x8215,
		FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216,
		FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217,
		FRAMEBUFFER_DEFAULT = 0x8218,
		FRAMEBUFFER_UNDEFINED = 0x8219,
		DEPTH_STENCIL_ATTACHMENT = 0x821A,
		DEPTH_STENCIL = 0x84F9,
		UNSIGNED_INT_24_8 = 0x84FA,
		DEPTH24_STENCIL8 = 0x88F0,
		UNSIGNED_NORMALIZED = 0x8C17,
		DRAW_FRAMEBUFFER_BINDING = 0x8CA6,
		READ_FRAMEBUFFER = 0x8CA8,
		DRAW_FRAMEBUFFER = 0x8CA9,
		READ_FRAMEBUFFER_BINDING = 0x8CAA,
		RENDERBUFFER_SAMPLES = 0x8CAB,
		FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER = 0x8CD4,
		MAX_COLOR_ATTACHMENTS = 0x8CDF,
		COLOR_ATTACHMENT1 = 0x8CE1,
		COLOR_ATTACHMENT2 = 0x8CE2,
		COLOR_ATTACHMENT3 = 0x8CE3,
		COLOR_ATTACHMENT4 = 0x8CE4,
		COLOR_ATTACHMENT5 = 0x8CE5,
		COLOR_ATTACHMENT6 = 0x8CE6,
		COLOR_ATTACHMENT7 = 0x8CE7,
		COLOR_ATTACHMENT8 = 0x8CE8,
		COLOR_ATTACHMENT9 = 0x8CE9,
		COLOR_ATTACHMENT10 = 0x8CEA,
		COLOR_ATTACHMENT11 = 0x8CEB,
		COLOR_ATTACHMENT12 = 0x8CEC,
		COLOR_ATTACHMENT13 = 0x8CED,
		COLOR_ATTACHMENT14 = 0x8CEE,
		COLOR_ATTACHMENT15 = 0x8CEF,
		COLOR_ATTACHMENT16 = 0x8CF0,
		COLOR_ATTACHMENT17 = 0x8CF1,
		COLOR_ATTACHMENT18 = 0x8CF2,
		COLOR_ATTACHMENT19 = 0x8CF3,
		COLOR_ATTACHMENT20 = 0x8CF4,
		COLOR_ATTACHMENT21 = 0x8CF5,
		COLOR_ATTACHMENT22 = 0x8CF6,
		COLOR_ATTACHMENT23 = 0x8CF7,
		COLOR_ATTACHMENT24 = 0x8CF8,
		COLOR_ATTACHMENT25 = 0x8CF9,
		COLOR_ATTACHMENT26 = 0x8CFA,
		COLOR_ATTACHMENT27 = 0x8CFB,
		COLOR_ATTACHMENT28 = 0x8CFC,
		COLOR_ATTACHMENT29 = 0x8CFD,
		COLOR_ATTACHMENT30 = 0x8CFE,
		COLOR_ATTACHMENT31 = 0x8CFF,
		FRAMEBUFFER_INCOMPLETE_MULTISAMPLE = 0x8D56,
		MAX_SAMPLES = 0x8D57,
		HALF_FLOAT = 0x140B,
		MAP_READ_BIT = 0x0001,
		MAP_WRITE_BIT = 0x0002,
		MAP_INVALIDATE_RANGE_BIT = 0x0004,
		MAP_INVALIDATE_BUFFER_BIT = 0x0008,
		MAP_FLUSH_EXPLICIT_BIT = 0x0010,
		MAP_UNSYNCHRONIZED_BIT = 0x0020,
		RG = 0x8227,
		RG_INTEGER = 0x8228,
		R8 = 0x8229,
		RG8 = 0x822B,
		R16F = 0x822D,
		R32F = 0x822E,
		RG16F = 0x822F,
		RG32F = 0x8230,
		R8I = 0x8231,
		R8UI = 0x8232,
		R16I = 0x8233,
		R16UI = 0x8234,
		R32I = 0x8235,
		R32UI = 0x8236,
		RG8I = 0x8237,
		RG8UI = 0x8238,
		RG16I = 0x8239,
		RG16UI = 0x823A,
		RG32I = 0x823B,
		RG32UI = 0x823C,
		VERTEX_ARRAY_BINDING = 0x85B5,
		R8_SNORM = 0x8F94,
		RG8_SNORM = 0x8F95,
		RGB8_SNORM = 0x8F96,
		RGBA8_SNORM = 0x8F97,
		SIGNED_NORMALIZED = 0x8F9C,
		PRIMITIVE_RESTART_FIXED_INDEX = 0x8D69,
		COPY_READ_BUFFER = 0x8F36,
		COPY_WRITE_BUFFER = 0x8F37,
		COPY_READ_BUFFER_BINDING = 0x8F36,
		COPY_WRITE_BUFFER_BINDING = 0x8F37,
		UNIFORM_BUFFER = 0x8A11,
		UNIFORM_BUFFER_BINDING = 0x8A28,
		UNIFORM_BUFFER_START = 0x8A29,
		UNIFORM_BUFFER_SIZE = 0x8A2A,
		MAX_VERTEX_UNIFORM_BLOCKS = 0x8A2B,
		MAX_FRAGMENT_UNIFORM_BLOCKS = 0x8A2D,
		MAX_COMBINED_UNIFORM_BLOCKS = 0x8A2E,
		MAX_UNIFORM_BUFFER_BINDINGS = 0x8A2F,
		MAX_UNIFORM_BLOCK_SIZE = 0x8A30,
		MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS = 0x8A31,
		MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS = 0x8A33,
		UNIFORM_BUFFER_OFFSET_ALIGNMENT = 0x8A34,
		ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH = 0x8A35,
		ACTIVE_UNIFORM_BLOCKS = 0x8A36,
		UNIFORM_TYPE = 0x8A37,
		UNIFORM_SIZE = 0x8A38,
		UNIFORM_NAME_LENGTH = 0x8A39,
		UNIFORM_BLOCK_INDEX = 0x8A3A,
		UNIFORM_OFFSET = 0x8A3B,
		UNIFORM_ARRAY_STRIDE = 0x8A3C,
		UNIFORM_MATRIX_STRIDE = 0x8A3D,
		UNIFORM_IS_ROW_MAJOR = 0x8A3E,
		UNIFORM_BLOCK_BINDING = 0x8A3F,
		UNIFORM_BLOCK_DATA_SIZE = 0x8A40,
		UNIFORM_BLOCK_NAME_LENGTH = 0x8A41,
		UNIFORM_BLOCK_ACTIVE_UNIFORMS = 0x8A42,
		UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES = 0x8A43,
		UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER = 0x8A44,
		UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER = 0x8A46,
		INVALID_INDEX = 0xFFFFFFFFu,
		MAX_VERTEX_OUTPUT_COMPONENTS = 0x9122,
		MAX_FRAGMENT_INPUT_COMPONENTS = 0x9125,
		MAX_SERVER_WAIT_TIMEOUT = 0x9111,
		OBJECT_TYPE = 0x9112,
		SYNC_CONDITION = 0x9113,
		SYNC_STATUS = 0x9114,
		SYNC_FLAGS = 0x9115,
		SYNC_FENCE = 0x9116,
		SYNC_GPU_COMMANDS_COMPLETE = 0x9117,
		UNSIGNALED = 0x9118,
		SIGNALED = 0x9119,
		ALREADY_SIGNALED = 0x911A,
		TIMEOUT_EXPIRED = 0x911B,
		CONDITION_SATISFIED = 0x911C,
		WAIT_FAILED = 0x911D,
		SYNC_FLUSH_COMMANDS_BIT = 0x00000001,
		VERTEX_ATTRIB_ARRAY_DIVISOR = 0x88FE,
		ANY_SAMPLES_PASSED = 0x8C2F,
		ANY_SAMPLES_PASSED_CONSERVATIVE = 0x8D6A,
		SAMPLER_BINDING = 0x8919,
		RGB10_A2UI = 0x906F,
		TEXTURE_SWIZZLE_R = 0x8E42,
		TEXTURE_SWIZZLE_G = 0x8E43,
		TEXTURE_SWIZZLE_B = 0x8E44,
		TEXTURE_SWIZZLE_A = 0x8E45,
		GREEN = 0x1904,
		BLUE = 0x1905,
		INT_2_10_10_10_REV = 0x8D9F,
		TRANSFORM_FEEDBACK = 0x8E22,
		TRANSFORM_FEEDBACK_PAUSED = 0x8E23,
		TRANSFORM_FEEDBACK_ACTIVE = 0x8E24,
		TRANSFORM_FEEDBACK_BINDING = 0x8E25,
		PROGRAM_BINARY_RETRIEVABLE_HINT = 0x8257,
		PROGRAM_BINARY_LENGTH = 0x8741,
		NUM_PROGRAM_BINARY_FORMATS = 0x87FE,
		PROGRAM_BINARY_FORMATS = 0x87FF,
		COMPRESSED_R11_EAC = 0x9270,
		COMPRESSED_SIGNED_R11_EAC = 0x9271,
		COMPRESSED_RG11_EAC = 0x9272,
		COMPRESSED_SIGNED_RG11_EAC = 0x9273,
		COMPRESSED_RGB8_ETC2 = 0x9274,
		COMPRESSED_SRGB8_ETC2 = 0x9275,
		COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9276,
		COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277,
		COMPRESSED_RGBA8_ETC2_EAC = 0x9278,
		COMPRESSED_SRGB8_ALPHA8_ETC2_EAC = 0x9279,
		TEXTURE_IMMUTABLE_FORMAT = 0x912F,
		MAX_ELEMENT_INDEX = 0x8D6B,
		NUM_SAMPLE_COUNTS = 0x9380,
		TEXTURE_IMMUTABLE_LEVELS = 0x82DF,
	};

	static inline void ActiveTexture(GLEnum p_texture) {
		glActiveTexture(p_texture);
	}
	static inline void AttachShader(unsigned int p_program, unsigned int p_shader) {
		glAttachShader(p_program, p_shader);
	}
	static inline void BindAttribLocation(unsigned int p_program, unsigned int p_index, const char *p_name) {
		glBindAttribLocation(p_program, p_index, p_name);
	}
	static inline void BindBuffer(GLEnum p_target, unsigned int p_buffer) {
		glBindBuffer(p_target, p_buffer);
	}
	static inline void BindFramebuffer(GLEnum p_target, unsigned int p_framebuffer) {
		glBindFramebuffer(p_target, p_framebuffer);
	}
	static inline void BindRenderbuffer(GLEnum p_target, unsigned int p_renderbuffer) {
		glBindRenderbuffer(p_target, p_renderbuffer);
	}
	static inline void BindTexture(GLEnum p_target, unsigned int p_texture) {
		glBindTexture(p_target, p_texture);
	}
	static inline void BlendColor(float p_red, float p_green, float p_blue, float p_alpha) {
		glBlendColor(p_red, p_green, p_blue, p_alpha);
	}
	static inline void BlendEquation(GLEnum p_mode) {
		glBlendEquation(p_mode);
	}
	static inline void BlendEquationSeparate(GLEnum p_modeRGB, GLEnum p_modeAlpha) {
		glBlendEquationSeparate(p_modeRGB, p_modeAlpha);
	}
	static inline void BlendFunc(GLEnum p_sfactor, GLEnum p_dfactor) {
		glBlendFunc(p_sfactor, p_dfactor);
	}
	static inline void BlendFuncSeparate(GLEnum p_sfactorRGB, GLEnum p_dfactorRGB, GLEnum p_sfactorAlpha, GLEnum p_dfactorAlpha) {
		glBlendFuncSeparate(p_sfactorRGB, p_dfactorRGB, p_sfactorAlpha, p_dfactorAlpha);
	}
	static inline void BufferData(GLEnum p_target, size_t p_size, const void *p_data, GLEnum p_usage) {
		glBufferData(p_target, p_size, p_data, p_usage);
	}
	static inline void BufferSubData(GLEnum p_target, intptr_t p_offset, size_t p_size, const void *p_data) {
		glBufferSubData(p_target, p_offset, p_size, p_data);
	}
	static inline GLEnum CheckFramebufferStatus(GLEnum p_target) {
		return (GLEnum)glCheckFramebufferStatus(p_target);
	}
	static inline void Clear(uint32_t p_mask) {
		glClear(p_mask);
	}
	static inline void ClearColor(float p_red, float p_green, float p_blue, float p_alpha) {
		glClearColor(p_red, p_green, p_blue, p_alpha);
	}
	static inline void ClearDepthf(float p_d) {
		glClearDepthf(p_d);
	}
	static inline void ClearStencil(int p_s) {
		glClearStencil(p_s);
	}
	static inline void ColorMask(bool p_red, bool p_green, bool p_blue, bool p_alpha) {
		glColorMask(p_red, p_green, p_blue, p_alpha);
	}
	static inline void CompileShader(unsigned int p_shader) {
		glCompileShader(p_shader);
	}
	static inline void CompressedTexImage2D(GLEnum p_target, int p_level, GLEnum p_internalformat, int p_width, int p_height, int p_border, int p_imageSize, const void *p_data) {
		glCompressedTexImage2D(p_target, p_level, p_internalformat, p_width, p_height, p_border, p_imageSize, p_data);
	}
	static inline void CompressedTexSubImage2D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_width, int p_height, GLEnum p_format, int p_imageSize, const void *p_data) {
		glCompressedTexSubImage2D(p_target, p_level, p_xoffset, p_yoffset, p_width, p_height, p_format, p_imageSize, p_data);
	}
	static inline void CopyTexImage2D(GLEnum p_target, int p_level, GLEnum p_internalformat, int p_x, int p_y, int p_width, int p_height, int p_border) {
		glCopyTexImage2D(p_target, p_level, p_internalformat, p_x, p_y, p_width, p_height, p_border);
	}
	static inline void CopyTexSubImage2D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_x, int p_y, int p_width, int p_height) {
		glCopyTexSubImage2D(p_target, p_level, p_xoffset, p_yoffset, p_x, p_y, p_width, p_height);
	}
	static inline unsigned int CreateProgram() {
		return glCreateProgram();
	}
	static inline unsigned int CreateShader(GLEnum p_type) {
		return glCreateShader(p_type);
	}
	static inline void CullFace(GLEnum p_mode) {
		glCullFace(p_mode);
	}
	static inline void DeleteBuffers(int p_n, const unsigned int *p_buffers) {
		glDeleteBuffers(p_n, p_buffers);
	}
	static inline void DeleteFramebuffers(int p_n, const unsigned int *p_framebuffers) {
		glDeleteFramebuffers(p_n, p_framebuffers);
	}
	static inline void DeleteProgram(unsigned int p_program) {
		glDeleteProgram(p_program);
	}
	static inline void DeleteRenderbuffers(int p_n, const unsigned int *p_renderbuffers) {
		glDeleteRenderbuffers(p_n, p_renderbuffers);
	}
	static inline void DeleteShader(unsigned int p_shader) {
		glDeleteShader(p_shader);
	}
	static inline void DeleteTextures(int p_n, const unsigned int *p_textures) {
		glDeleteTextures(p_n, p_textures);
	}
	static inline void DepthFunc(GLEnum p_func) {
		glDepthFunc(p_func);
	}
	static inline void DepthMask(bool p_flag) {
		glDepthMask(p_flag);
	}
	static inline void DepthRangef(float p_n, float p_f) {
		glDepthRangef(p_n, p_f);
	}
	static inline void DetachShader(unsigned int p_program, unsigned int p_shader) {
		glDetachShader(p_program, p_shader);
	}
	static inline void Disable(GLEnum p_cap) {
		glDisable(p_cap);
	}
	static inline void DisableVertexAttribArray(unsigned int p_index) {
		glDisableVertexAttribArray(p_index);
	}
	static inline void DrawArrays(GLEnum p_mode, int p_first, int p_count) {
		glDrawArrays(p_mode, p_first, p_count);
	}
	static inline void DrawElements(GLEnum p_mode, int p_count, GLEnum p_type, const void *p_indices) {
		glDrawElements(p_mode, p_count, p_type, p_indices);
	}
	static inline void Enable(GLEnum p_cap) {
		glEnable(p_cap);
	}
	static inline void EnableVertexAttribArray(unsigned int p_index) {
		glEnableVertexAttribArray(p_index);
	}
	static inline void Finish() {
		glFinish();
	}
	static inline void Flush() {
		glFlush();
	}
	static inline void FramebufferRenderbuffer(GLEnum p_target, GLEnum p_attachment, GLEnum p_renderbuffertarget, unsigned int p_renderbuffer) {
		glFramebufferRenderbuffer(p_target, p_attachment, p_renderbuffertarget, p_renderbuffer);
	}
	static inline void FramebufferTexture2D(GLEnum p_target, GLEnum p_attachment, GLEnum p_textarget, unsigned int p_texture, int p_level) {
		glFramebufferTexture2D(p_target, p_attachment, p_textarget, p_texture, p_level);
	}
	static inline void FrontFace(GLEnum p_mode) {
		glFrontFace(p_mode);
	}
	static inline void GenBuffers(int p_n, unsigned int *p_buffers) {
		glGenBuffers(p_n, p_buffers);
	}
	static inline void GenerateMipmap(GLEnum p_target) {
		glGenerateMipmap(p_target);
	}
	static inline void GenFramebuffers(int p_n, unsigned int *p_framebuffers) {
		glGenFramebuffers(p_n, p_framebuffers);
	}
	static inline void GenRenderbuffers(int p_n, unsigned int *p_renderbuffers) {
		glGenRenderbuffers(p_n, p_renderbuffers);
	}
	static inline void GenTextures(int p_n, unsigned int *p_textures) {
		glGenTextures(p_n, p_textures);
	}
	static inline void GetActiveAttrib(unsigned int p_program, unsigned int p_index, int p_bufSize, int *p_length, int *p_size, GLEnum *p_type, char *p_name) {
		glGetActiveAttrib(p_program, p_index, p_bufSize, p_length, p_size, (GLenum *)p_type, p_name);
	}
	static inline void GetActiveUniform(unsigned int p_program, unsigned int p_index, int p_bufSize, int *p_length, int *p_size, GLEnum *p_type, char *p_name) {
		glGetActiveUniform(p_program, p_index, p_bufSize, p_length, p_size, (GLenum *)p_type, p_name);
	}
	static inline void GetAttachedShaders(unsigned int p_program, int p_maxCount, int *p_count, unsigned int *p_shaders) {
		glGetAttachedShaders(p_program, p_maxCount, p_count, p_shaders);
	}
	static inline int GetAttribLocation(unsigned int p_program, const char *p_name) {
		return glGetAttribLocation(p_program, p_name);
	}
	static inline void GetBooleanv(GLEnum p_pname, unsigned char *p_data) {
		glGetBooleanv(p_pname, p_data);
	}
	static inline void GetBufferParameteriv(GLEnum p_target, GLEnum p_pname, int *p_params) {
		glGetBufferParameteriv(p_target, p_pname, p_params);
	}
	static inline GLEnum GetError() {
		return (GLEnum)glGetError();
	}
	static inline void GetFloatv(GLEnum p_pname, float *p_data) {
		glGetFloatv(p_pname, p_data);
	}
	static inline void GetFramebufferAttachmentParameteriv(GLEnum p_target, GLEnum p_attachment, GLEnum p_pname, int *p_params) {
		glGetFramebufferAttachmentParameteriv(p_target, p_attachment, p_pname, p_params);
	}
	static inline void GetIntegerv(GLEnum p_pname, int *p_data) {
		glGetIntegerv(p_pname, p_data);
	}
	static inline void GetProgramiv(unsigned int p_program, GLEnum p_pname, int *p_params) {
		glGetProgramiv(p_program, p_pname, p_params);
	}
	static inline void GetProgramInfoLog(unsigned int p_program, int p_bufSize, int *p_length, char *p_infoLog) {
		glGetProgramInfoLog(p_program, p_bufSize, p_length, p_infoLog);
	}
	static inline void GetRenderbufferParameteriv(GLEnum p_target, GLEnum p_pname, int *p_params) {
		glGetRenderbufferParameteriv(p_target, p_pname, p_params);
	}
	static inline void GetShaderiv(unsigned int p_shader, GLEnum p_pname, int *p_params) {
		glGetShaderiv(p_shader, p_pname, p_params);
	}
	static inline void GetShaderInfoLog(unsigned int p_shader, int p_bufSize, int *p_length, char *p_infoLog) {
		glGetShaderInfoLog(p_shader, p_bufSize, p_length, p_infoLog);
	}
	static inline void GetShaderPrecisionFormat(GLEnum p_shadertype, GLEnum p_precisiontype, int *p_range, int *p_precision) {
		glGetShaderPrecisionFormat(p_shadertype, p_precisiontype, p_range, p_precision);
	}
	static inline void GetShaderSource(unsigned int p_shader, int p_bufSize, int *p_length, char *p_source) {
		glGetShaderSource(p_shader, p_bufSize, p_length, p_source);
	}
	static inline void GetTexParameterfv(GLEnum p_target, GLEnum p_pname, float *p_params) {
		glGetTexParameterfv(p_target, p_pname, p_params);
	}
	static inline void GetTexParameteriv(GLEnum p_target, GLEnum p_pname, int *p_params) {
		glGetTexParameteriv(p_target, p_pname, p_params);
	}
	static inline void GetUniformfv(unsigned int p_program, int p_location, float *p_params) {
		glGetUniformfv(p_program, p_location, p_params);
	}
	static inline void GetUniformiv(unsigned int p_program, int p_location, int *p_params) {
		glGetUniformiv(p_program, p_location, p_params);
	}
	static inline int GetUniformLocation(unsigned int p_program, const char *p_name) {
		return glGetUniformLocation(p_program, p_name);
	}
	static inline void GetVertexAttribfv(unsigned int p_index, GLEnum p_pname, float *p_params) {
		glGetVertexAttribfv(p_index, p_pname, p_params);
	}
	static inline void GetVertexAttribiv(unsigned int p_index, GLEnum p_pname, int *p_params) {
		glGetVertexAttribiv(p_index, p_pname, p_params);
	}
	static inline void GetVertexAttribPointerv(unsigned int p_index, GLEnum p_pname, void **p_pointer) {
		glGetVertexAttribPointerv(p_index, p_pname, p_pointer);
	}
	static inline void Hint(GLEnum p_target, GLEnum p_mode) {
		glHint(p_target, p_mode);
	}
	static inline bool IsBuffer(unsigned int p_buffer) {
		return glIsBuffer(p_buffer);
	}
	static inline bool IsEnabled(GLEnum p_cap) {
		return glIsEnabled(p_cap);
	}
	static inline bool IsFramebuffer(unsigned int p_framebuffer) {
		return glIsFramebuffer(p_framebuffer);
	}
	static inline bool IsProgram(unsigned int p_program) {
		return glIsProgram(p_program);
	}
	static inline bool IsRenderbuffer(unsigned int p_renderbuffer) {
		return glIsRenderbuffer(p_renderbuffer);
	}
	static inline bool IsShader(unsigned int p_shader) {
		return glIsShader(p_shader);
	}
	static inline bool IsTexture(unsigned int p_texture) {
		return glIsTexture(p_texture);
	}
	static inline void LineWidth(float p_width) {
		glLineWidth(p_width);
	}
	static inline void LinkProgram(unsigned int p_program) {
		glLinkProgram(p_program);
	}
	static inline void PixelStorei(GLEnum p_pname, int p_param) {
		glPixelStorei(p_pname, p_param);
	}
	static inline void PolygonOffset(float p_factor, float p_units) {
		glPolygonOffset(p_factor, p_units);
	}
	static inline void ReadPixels(int p_x, int p_y, int p_width, int p_height, GLEnum p_format, GLEnum p_type, void *p_pixels) {
		glReadPixels(p_x, p_y, p_width, p_height, p_format, p_type, p_pixels);
	}
	static inline void ReleaseShaderCompiler() {
		glReleaseShaderCompiler();
	}
	static inline void RenderbufferStorage(GLEnum p_target, GLEnum p_internalformat, int p_width, int p_height) {
		glRenderbufferStorage(p_target, p_internalformat, p_width, p_height);
	}
	static inline void SampleCoverage(float p_value, bool p_invert) {
		glSampleCoverage(p_value, p_invert);
	}
	static inline void Scissor(int p_x, int p_y, int p_width, int p_height) {
		glScissor(p_x, p_y, p_width, p_height);
	}
	static inline void ShaderBinary(int p_count, const unsigned int *p_shaders, GLEnum p_binaryFormat, const void *p_binary, int p_length) {
		glShaderBinary(p_count, p_shaders, p_binaryFormat, p_binary, p_length);
	}
	static inline void ShaderSource(unsigned int p_shader, int p_count, const char *const *p_string, const int *p_length) {
		glShaderSource(p_shader, p_count, p_string, p_length);
	}
	static inline void StencilFunc(GLEnum p_func, int p_ref, unsigned int p_mask) {
		glStencilFunc(p_func, p_ref, p_mask);
	}
	static inline void StencilFuncSeparate(GLEnum p_face, GLEnum p_func, int p_ref, unsigned int p_mask) {
		glStencilFuncSeparate(p_face, p_func, p_ref, p_mask);
	}
	static inline void StencilMask(unsigned int p_mask) {
		glStencilMask(p_mask);
	}
	static inline void StencilMaskSeparate(GLEnum p_face, unsigned int p_mask) {
		glStencilMaskSeparate(p_face, p_mask);
	}
	static inline void StencilOp(GLEnum p_fail, GLEnum p_zfail, GLEnum p_zpass) {
		glStencilOp(p_fail, p_zfail, p_zpass);
	}
	static inline void StencilOpSeparate(GLEnum p_face, GLEnum p_sfail, GLEnum p_dpfail, GLEnum p_dppass) {
		glStencilOpSeparate(p_face, p_sfail, p_dpfail, p_dppass);
	}
	static inline void TexImage2D(GLEnum p_target, int p_level, int p_internalformat, int p_width, int p_height, int p_border, GLEnum p_format, GLEnum p_type, const void *p_pixels) {
		glTexImage2D(p_target, p_level, p_internalformat, p_width, p_height, p_border, p_format, p_type, p_pixels);
	}
	static inline void TexParameterf(GLEnum p_target, GLEnum p_pname, float p_param) {
		glTexParameterf(p_target, p_pname, p_param);
	}
	static inline void TexParameterfv(GLEnum p_target, GLEnum p_pname, const float *p_params) {
		glTexParameterfv(p_target, p_pname, p_params);
	}
	static inline void TexParameteri(GLEnum p_target, GLEnum p_pname, int p_param) {
		glTexParameteri(p_target, p_pname, p_param);
	}
	static inline void TexParameteriv(GLEnum p_target, GLEnum p_pname, const int *p_params) {
		glTexParameteriv(p_target, p_pname, p_params);
	}
	static inline void TexSubImage2D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_width, int p_height, GLEnum p_format, GLEnum p_type, const void *p_pixels) {
		glTexSubImage2D(p_target, p_level, p_xoffset, p_yoffset, p_width, p_height, p_format, p_type, p_pixels);
	}
	static inline void Uniform1f(int p_location, float p_v0) {
		glUniform1f(p_location, p_v0);
	}
	static inline void Uniform1fv(int p_location, int p_count, const float *p_value) {
		glUniform1fv(p_location, p_count, p_value);
	}
	static inline void Uniform1i(int p_location, int p_v0) {
		glUniform1i(p_location, p_v0);
	}
	static inline void Uniform1iv(int p_location, int p_count, const int *p_value) {
		glUniform1iv(p_location, p_count, p_value);
	}
	static inline void Uniform2f(int p_location, float p_v0, float p_v1) {
		glUniform2f(p_location, p_v0, p_v1);
	}
	static inline void Uniform2fv(int p_location, int p_count, const float *p_value) {
		glUniform2fv(p_location, p_count, p_value);
	}
	static inline void Uniform2i(int p_location, int p_v0, int p_v1) {
		glUniform2i(p_location, p_v0, p_v1);
	}
	static inline void Uniform2iv(int p_location, int p_count, const int *p_value) {
		glUniform2iv(p_location, p_count, p_value);
	}
	static inline void Uniform3f(int p_location, float p_v0, float p_v1, float p_v2) {
		glUniform3f(p_location, p_v0, p_v1, p_v2);
	}
	static inline void Uniform3fv(int p_location, int p_count, const float *p_value) {
		glUniform3fv(p_location, p_count, p_value);
	}
	static inline void Uniform3i(int p_location, int p_v0, int p_v1, int p_v2) {
		glUniform3i(p_location, p_v0, p_v1, p_v2);
	}
	static inline void Uniform3iv(int p_location, int p_count, const int *p_value) {
		glUniform3iv(p_location, p_count, p_value);
	}
	static inline void Uniform4f(int p_location, float p_v0, float p_v1, float p_v2, float p_v3) {
		glUniform4f(p_location, p_v0, p_v1, p_v2, p_v3);
	}
	static inline void Uniform4fv(int p_location, int p_count, const float *p_value) {
		glUniform4fv(p_location, p_count, p_value);
	}
	static inline void Uniform4i(int p_location, int p_v0, int p_v1, int p_v2, int p_v3) {
		glUniform4i(p_location, p_v0, p_v1, p_v2, p_v3);
	}
	static inline void Uniform4iv(int p_location, int p_count, const int *p_value) {
		glUniform4iv(p_location, p_count, p_value);
	}
	static inline void UniformMatrix2fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix2fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix3fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix3fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix4fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix4fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UseProgram(unsigned int p_program) {
		glUseProgram(p_program);
	}
	static inline void ValidateProgram(unsigned int p_program) {
		glValidateProgram(p_program);
	}
	static inline void VertexAttrib1f(unsigned int p_index, float p_x) {
		glVertexAttrib1f(p_index, p_x);
	}
	static inline void VertexAttrib1fv(unsigned int p_index, const float *p_v) {
		glVertexAttrib1fv(p_index, p_v);
	}
	static inline void VertexAttrib2f(unsigned int p_index, float p_x, float p_y) {
		glVertexAttrib2f(p_index, p_x, p_y);
	}
	static inline void VertexAttrib2fv(unsigned int p_index, const float *p_v) {
		glVertexAttrib2fv(p_index, p_v);
	}
	static inline void VertexAttrib3f(unsigned int p_index, float p_x, float p_y, float p_z) {
		glVertexAttrib3f(p_index, p_x, p_y, p_z);
	}
	static inline void VertexAttrib3fv(unsigned int p_index, const float *p_v) {
		glVertexAttrib3fv(p_index, p_v);
	}
	static inline void VertexAttrib4f(unsigned int p_index, float p_x, float p_y, float p_z, float p_w) {
		glVertexAttrib4f(p_index, p_x, p_y, p_z, p_w);
	}
	static inline void VertexAttrib4fv(unsigned int p_index, const float *p_v) {
		glVertexAttrib4fv(p_index, p_v);
	}
	static inline void VertexAttribPointer(unsigned int p_index, int p_size, GLEnum p_type, bool p_normalized, int p_stride, const void *p_pointer) {
		glVertexAttribPointer(p_index, p_size, p_type, p_normalized, p_stride, p_pointer);
	}
	static inline void Viewport(int p_x, int p_y, int p_width, int p_height) {
		glViewport(p_x, p_y, p_width, p_height);
	}
	static inline void ReadBuffer(GLEnum p_src) {
		glReadBuffer(p_src);
	}
	static inline void DrawRangeElements(GLEnum p_mode, unsigned int p_start, unsigned int p_end, int p_count, GLEnum p_type, const void *p_indices) {
		glDrawRangeElements(p_mode, p_start, p_end, p_count, p_type, p_indices);
	}
	static inline void TexImage3D(GLEnum p_target, int p_level, int p_internalformat, int p_width, int p_height, int p_depth, int p_border, GLEnum p_format, GLEnum p_type, const void *p_pixels) {
		glTexImage3D(p_target, p_level, p_internalformat, p_width, p_height, p_depth, p_border, p_format, p_type, p_pixels);
	}
	static inline void TexSubImage3D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_zoffset, int p_width, int p_height, int p_depth, GLEnum p_format, GLEnum p_type, const void *p_pixels) {
		glTexSubImage3D(p_target, p_level, p_xoffset, p_yoffset, p_zoffset, p_width, p_height, p_depth, p_format, p_type, p_pixels);
	}
	static inline void CopyTexSubImage3D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_zoffset, int p_x, int p_y, int p_width, int p_height) {
		glCopyTexSubImage3D(p_target, p_level, p_xoffset, p_yoffset, p_zoffset, p_x, p_y, p_width, p_height);
	}
	static inline void CompressedTexImage3D(GLEnum p_target, int p_level, GLEnum p_internalformat, int p_width, int p_height, int p_depth, int p_border, int p_imageSize, const void *p_data) {
		glCompressedTexImage3D(p_target, p_level, p_internalformat, p_width, p_height, p_depth, p_border, p_imageSize, p_data);
	}
	static inline void CompressedTexSubImage3D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_zoffset, int p_width, int p_height, int p_depth, GLEnum p_format, int p_imageSize, const void *p_data) {
		glCompressedTexSubImage3D(p_target, p_level, p_xoffset, p_yoffset, p_zoffset, p_width, p_height, p_depth, p_format, p_imageSize, p_data);
	}
	static inline void GenQueries(int p_n, unsigned int *p_ids) {
		glGenQueries(p_n, p_ids);
	}
	static inline void DeleteQueries(int p_n, const unsigned int *p_ids) {
		glDeleteQueries(p_n, p_ids);
	}
	static inline bool IsQuery(unsigned int p_id) {
		return glIsQuery(p_id);
	}
	static inline void BeginQuery(GLEnum p_target, unsigned int p_id) {
		glBeginQuery(p_target, p_id);
	}
	static inline void EndQuery(GLEnum p_target) {
		glEndQuery(p_target);
	}
	static inline void GetQueryiv(GLEnum p_target, GLEnum p_pname, int *p_params) {
		glGetQueryiv(p_target, p_pname, p_params);
	}
	static inline void GetQueryObjectuiv(unsigned int p_id, GLEnum p_pname, unsigned int *p_params) {
		glGetQueryObjectuiv(p_id, p_pname, p_params);
	}
	static inline bool UnmapBuffer(GLEnum p_target) {
		return glUnmapBuffer(p_target);
	}
	static inline void GetBufferPointerv(GLEnum p_target, GLEnum p_pname, void **p_params) {
		glGetBufferPointerv(p_target, p_pname, p_params);
	}
	static inline void DrawBuffers(int p_n, const GLEnum *p_bufs) {
		glDrawBuffers(p_n, (GLenum *)p_bufs);
	}
	static inline void UniformMatrix2x3fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix2x3fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix3x2fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix3x2fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix2x4fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix2x4fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix4x2fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix4x2fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix3x4fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix3x4fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void UniformMatrix4x3fv(int p_location, int p_count, bool p_transpose, const float *p_value) {
		glUniformMatrix4x3fv(p_location, p_count, p_transpose, p_value);
	}
	static inline void BlitFramebuffer(int p_srcX0, int p_srcY0, int p_srcX1, int p_srcY1, int p_dstX0, int p_dstY0, int p_dstX1, int p_dstY1, uint32_t p_mask, GLEnum p_filter) {
		glBlitFramebuffer(p_srcX0, p_srcY0, p_srcX1, p_srcY1, p_dstX0, p_dstY0, p_dstX1, p_dstY1, p_mask, p_filter);
	}
	static inline void RenderbufferStorageMultisample(GLEnum p_target, int p_samples, GLEnum p_internalformat, int p_width, int p_height) {
		glRenderbufferStorageMultisample(p_target, p_samples, p_internalformat, p_width, p_height);
	}
	static inline void FramebufferTextureLayer(GLEnum p_target, GLEnum p_attachment, unsigned int p_texture, int p_level, int p_layer) {
		glFramebufferTextureLayer(p_target, p_attachment, p_texture, p_level, p_layer);
	}
	static inline void MapBufferRange(GLEnum p_target, intptr_t p_offset, size_t p_length, uint32_t p_access) {
		glMapBufferRange(p_target, p_offset, p_length, p_access);
	}
	static inline void FlushMappedBufferRange(GLEnum p_target, intptr_t p_offset, size_t p_length) {
		glFlushMappedBufferRange(p_target, p_offset, p_length);
	}
	static inline void BindVertexArray(unsigned int p_array) {
		glBindVertexArray(p_array);
	}
	static inline void DeleteVertexArrays(int p_n, const unsigned int *p_arrays) {
		glDeleteVertexArrays(p_n, p_arrays);
	}
	static inline void GenVertexArrays(int p_n, unsigned int *p_arrays) {
		glGenVertexArrays(p_n, p_arrays);
	}
	static inline bool IsVertexArray(unsigned int p_array) {
		return glIsVertexArray(p_array);
	}
	static inline void GetIntegeri_v(GLEnum p_target, unsigned int p_index, int *p_data) {
		glGetIntegeri_v(p_target, p_index, p_data);
	}
	static inline void BeginTransformFeedback(GLEnum p_primitiveMode) {
		glBeginTransformFeedback(p_primitiveMode);
	}
	static inline void EndTransformFeedback() {
		glEndTransformFeedback();
	}
	static inline void BindBufferRange(GLEnum p_target, unsigned int p_index, unsigned int p_buffer, intptr_t p_offset, size_t p_size) {
		glBindBufferRange(p_target, p_index, p_buffer, p_offset, p_size);
	}
	static inline void BindBufferBase(GLEnum p_target, unsigned int p_index, unsigned int p_buffer) {
		glBindBufferBase(p_target, p_index, p_buffer);
	}
	static inline void TransformFeedbackVaryings(unsigned int p_program, int p_count, const char *const *p_varyings, GLEnum p_bufferMode) {
		glTransformFeedbackVaryings(p_program, p_count, p_varyings, p_bufferMode);
	}
	static inline void GetTransformFeedbackVarying(unsigned int p_program, unsigned int p_index, int p_bufSize, int *p_length, int *p_size, GLEnum *p_type, char *p_name) {
		glGetTransformFeedbackVarying(p_program, p_index, p_bufSize, p_length, p_size, (GLenum *)p_type, p_name);
	}
	static inline void VertexAttribIPointer(unsigned int p_index, int p_size, GLEnum p_type, int p_stride, const void *p_pointer) {
		glVertexAttribIPointer(p_index, p_size, p_type, p_stride, p_pointer);
	}
	static inline void GetVertexAttribIiv(unsigned int p_index, GLEnum p_pname, int *p_params) {
		glGetVertexAttribIiv(p_index, p_pname, p_params);
	}
	static inline void GetVertexAttribIuiv(unsigned int p_index, GLEnum p_pname, unsigned int *p_params) {
		glGetVertexAttribIuiv(p_index, p_pname, p_params);
	}
	static inline void VertexAttribI4i(unsigned int p_index, int p_x, int p_y, int p_z, int p_w) {
		glVertexAttribI4i(p_index, p_x, p_y, p_z, p_w);
	}
	static inline void VertexAttribI4ui(unsigned int p_index, unsigned int p_x, unsigned int p_y, unsigned int p_z, unsigned int p_w) {
		glVertexAttribI4ui(p_index, p_x, p_y, p_z, p_w);
	}
	static inline void VertexAttribI4iv(unsigned int p_index, const int *p_v) {
		glVertexAttribI4iv(p_index, p_v);
	}
	static inline void VertexAttribI4uiv(unsigned int p_index, const unsigned int *p_v) {
		glVertexAttribI4uiv(p_index, p_v);
	}
	static inline void GetUniformuiv(unsigned int p_program, int p_location, unsigned int *p_params) {
		glGetUniformuiv(p_program, p_location, p_params);
	}
	static inline int GetFragDataLocation(unsigned int p_program, const char *p_name) {
		return glGetFragDataLocation(p_program, p_name);
	}
	static inline void Uniform1ui(int p_location, unsigned int p_v0) {
		glUniform1ui(p_location, p_v0);
	}
	static inline void Uniform2ui(int p_location, unsigned int p_v0, unsigned int p_v1) {
		glUniform2ui(p_location, p_v0, p_v1);
	}
	static inline void Uniform3ui(int p_location, unsigned int p_v0, unsigned int p_v1, unsigned int p_v2) {
		glUniform3ui(p_location, p_v0, p_v1, p_v2);
	}
	static inline void Uniform4ui(int p_location, unsigned int p_v0, unsigned int p_v1, unsigned int p_v2, unsigned int p_v3) {
		glUniform4ui(p_location, p_v0, p_v1, p_v2, p_v3);
	}
	static inline void Uniform1uiv(int p_location, int p_count, const unsigned int *p_value) {
		glUniform1uiv(p_location, p_count, p_value);
	}
	static inline void Uniform2uiv(int p_location, int p_count, const unsigned int *p_value) {
		glUniform2uiv(p_location, p_count, p_value);
	}
	static inline void Uniform3uiv(int p_location, int p_count, const unsigned int *p_value) {
		glUniform3uiv(p_location, p_count, p_value);
	}
	static inline void Uniform4uiv(int p_location, int p_count, const unsigned int *p_value) {
		glUniform4uiv(p_location, p_count, p_value);
	}
	static inline void ClearBufferiv(GLEnum p_buffer, int p_drawbuffer, const int *p_value) {
		glClearBufferiv(p_buffer, p_drawbuffer, p_value);
	}
	static inline void ClearBufferuiv(GLEnum p_buffer, int p_drawbuffer, const unsigned int *p_value) {
		glClearBufferuiv(p_buffer, p_drawbuffer, p_value);
	}
	static inline void ClearBufferfv(GLEnum p_buffer, int p_drawbuffer, const float *p_value) {
		glClearBufferfv(p_buffer, p_drawbuffer, p_value);
	}
	static inline void ClearBufferfi(GLEnum p_buffer, int p_drawbuffer, float p_depth, int p_stencil) {
		glClearBufferfi(p_buffer, p_drawbuffer, p_depth, p_stencil);
	}
	static inline void CopyBufferSubData(GLEnum p_readTarget, GLEnum p_writeTarget, intptr_t p_readOffset, intptr_t p_writeOffset, size_t p_size) {
		glCopyBufferSubData(p_readTarget, p_writeTarget, p_readOffset, p_writeOffset, p_size);
	}
	static inline void GetUniformIndices(unsigned int p_program, int p_uniformCount, const char *const *p_uniformNames, unsigned int *p_uniformIndices) {
		glGetUniformIndices(p_program, p_uniformCount, p_uniformNames, p_uniformIndices);
	}
	static inline void GetActiveUniformsiv(unsigned int p_program, int p_uniformCount, const unsigned int *p_uniformIndices, GLEnum p_pname, int *p_params) {
		glGetActiveUniformsiv(p_program, p_uniformCount, p_uniformIndices, p_pname, p_params);
	}
	static inline unsigned int GetUniformBlockIndex(unsigned int p_program, const char *p_uniformBlockName) {
		return glGetUniformBlockIndex(p_program, p_uniformBlockName);
	}
	static inline void GetActiveUniformBlockiv(unsigned int p_program, unsigned int p_uniformBlockIndex, GLEnum p_pname, int *p_params) {
		glGetActiveUniformBlockiv(p_program, p_uniformBlockIndex, p_pname, p_params);
	}
	static inline void GetActiveUniformBlockName(unsigned int p_program, unsigned int p_uniformBlockIndex, int p_bufSize, int *p_length, char *p_uniformBlockName) {
		glGetActiveUniformBlockName(p_program, p_uniformBlockIndex, p_bufSize, p_length, p_uniformBlockName);
	}
	static inline void UniformBlockBinding(unsigned int p_program, unsigned int p_uniformBlockIndex, unsigned int p_uniformBlockBinding) {
		glUniformBlockBinding(p_program, p_uniformBlockIndex, p_uniformBlockBinding);
	}
	static inline void DrawArraysInstanced(GLEnum p_mode, int p_first, int p_count, int p_instancecount) {
		glDrawArraysInstanced(p_mode, p_first, p_count, p_instancecount);
	}
	static inline void DrawElementsInstanced(GLEnum p_mode, int p_count, GLEnum p_type, const void *p_indices, int p_instancecount) {
		glDrawElementsInstanced(p_mode, p_count, p_type, p_indices, p_instancecount);
	}
	static inline GLsync FenceSync(GLEnum p_condition, uint32_t p_flags) {
		return glFenceSync(p_condition, p_flags);
	}
	static inline bool IsSync(GLsync p_sync) {
		return glIsSync(p_sync);
	}
	static inline void DeleteSync(GLsync p_sync) {
		glDeleteSync(p_sync);
	}
	static inline GLEnum ClientWaitSync(GLsync p_sync, uint32_t p_flags, uint64_t p_timeout) {
		return (GLEnum)glClientWaitSync(p_sync, p_flags, p_timeout);
	}
	static inline void WaitSync(GLsync p_sync, uint32_t p_flags, uint64_t p_timeout) {
		glWaitSync(p_sync, p_flags, p_timeout);
	}
	static inline void GetInteger64v(GLEnum p_pname, int64_t *p_data) {
		glGetInteger64v(p_pname, p_data);
	}
	static inline void GetSynciv(GLsync p_sync, GLEnum p_pname, int p_count, int *p_length, int *p_values) {
		glGetSynciv(p_sync, p_pname, p_count, p_length, p_values);
	}
	static inline void GetInteger64i_v(GLEnum p_target, unsigned int p_index, int64_t *p_data) {
		glGetInteger64i_v(p_target, p_index, p_data);
	}
	static inline void GetBufferParameteri64v(GLEnum p_target, GLEnum p_pname, int64_t *p_params) {
		glGetBufferParameteri64v(p_target, p_pname, p_params);
	}
	static inline void GenSamplers(int p_count, unsigned int *p_samplers) {
		glGenSamplers(p_count, p_samplers);
	}
	static inline void DeleteSamplers(int p_count, const unsigned int *p_samplers) {
		glDeleteSamplers(p_count, p_samplers);
	}
	static inline bool IsSampler(unsigned int p_sampler) {
		return glIsSampler(p_sampler);
	}
	static inline void BindSampler(unsigned int p_unit, unsigned int p_sampler) {
		glBindSampler(p_unit, p_sampler);
	}
	static inline void SamplerParameteri(unsigned int p_sampler, GLEnum p_pname, int p_param) {
		glSamplerParameteri(p_sampler, p_pname, p_param);
	}
	static inline void SamplerParameteriv(unsigned int p_sampler, GLEnum p_pname, const int *p_param) {
		glSamplerParameteriv(p_sampler, p_pname, p_param);
	}
	static inline void SamplerParameterf(unsigned int p_sampler, GLEnum p_pname, float p_param) {
		glSamplerParameterf(p_sampler, p_pname, p_param);
	}
	static inline void SamplerParameterfv(unsigned int p_sampler, GLEnum p_pname, const float *p_param) {
		glSamplerParameterfv(p_sampler, p_pname, p_param);
	}
	static inline void GetSamplerParameteriv(unsigned int p_sampler, GLEnum p_pname, int *p_params) {
		glGetSamplerParameteriv(p_sampler, p_pname, p_params);
	}
	static inline void GetSamplerParameterfv(unsigned int p_sampler, GLEnum p_pname, float *p_params) {
		glGetSamplerParameterfv(p_sampler, p_pname, p_params);
	}
	static inline void VertexAttribDivisor(unsigned int p_index, unsigned int p_divisor) {
		glVertexAttribDivisor(p_index, p_divisor);
	}
	static inline void BindTransformFeedback(GLEnum p_target, unsigned int p_id) {
		glBindTransformFeedback(p_target, p_id);
	}
	static inline void DeleteTransformFeedbacks(int p_n, const unsigned int *p_ids) {
		glDeleteTransformFeedbacks(p_n, p_ids);
	}
	static inline void GenTransformFeedbacks(int p_n, unsigned int *p_ids) {
		glGenTransformFeedbacks(p_n, p_ids);
	}
	static inline bool IsTransformFeedback(unsigned int p_id) {
		return glIsTransformFeedback(p_id);
	}
	static inline void PauseTransformFeedback() {
		glPauseTransformFeedback();
	}
	static inline void ResumeTransformFeedback() {
		glResumeTransformFeedback();
	}
	static inline void GetProgramBinary(unsigned int p_program, int p_bufSize, int *p_length, GLEnum *p_binaryFormat, void *p_binary) {
		glGetProgramBinary(p_program, p_bufSize, p_length, (GLenum *)p_binaryFormat, p_binary);
	}
	static inline void ProgramBinary(unsigned int p_program, GLEnum p_binaryFormat, const void *p_binary, int p_length) {
		glProgramBinary(p_program, p_binaryFormat, p_binary, p_length);
	}
	static inline void ProgramParameteri(unsigned int p_program, GLEnum p_pname, int p_value) {
		glProgramParameteri(p_program, p_pname, p_value);
	}
	static inline void InvalidateFramebuffer(GLEnum p_target, int p_numAttachments, const GLEnum *p_attachments) {
		glInvalidateFramebuffer(p_target, p_numAttachments, (const GLenum *)p_attachments);
	}
	static inline void InvalidateSubFramebuffer(GLEnum p_target, int p_numAttachments, const GLEnum *p_attachments, int p_x, int p_y, int p_width, int p_height) {
		glInvalidateSubFramebuffer(p_target, p_numAttachments, (const GLenum *)p_attachments, p_x, p_y, p_width, p_height);
	}
	static inline void TexStorage2D(GLEnum p_target, int p_levels, GLEnum p_internalformat, int p_width, int p_height) {
		glTexStorage2D(p_target, p_levels, p_internalformat, p_width, p_height);
	}
	static inline void TexStorage3D(GLEnum p_target, int p_levels, GLEnum p_internalformat, int p_width, int p_height, int p_depth) {
		glTexStorage3D(p_target, p_levels, p_internalformat, p_width, p_height, p_depth);
	}
	static inline void GetInternalformativ(GLEnum p_target, GLEnum p_internalformat, GLEnum p_pname, int p_count, int *p_params) {
		glGetInternalformativ(p_target, p_internalformat, p_pname, p_count, p_params);
	}

	GLES3();

private:
	static inline void _BindAttribLocation(unsigned int p_program, unsigned int p_index, const String &p_name) {
		glBindAttribLocation(p_program, p_index, p_name.utf8().get_data());
	}
	static inline void _BufferData(GLEnum p_target, size_t p_size, PackedByteArray p_data, GLEnum p_usage) {
		glBufferData(p_target, p_size, p_data.ptr(), p_usage);
	}
	static inline void _BufferSubData(GLEnum p_target, intptr_t p_offset, size_t p_size, PackedByteArray p_data) {
		glBufferSubData(p_target, p_offset, p_size, p_data.ptr());
	}
	static inline void _CompressedTexImage2D(GLEnum p_target, int p_level, GLEnum p_internalformat, int p_width, int p_height, int p_border, int p_imageSize, PackedByteArray p_data) {
		glCompressedTexImage2D(p_target, p_level, p_internalformat, p_width, p_height, p_border, p_imageSize, p_data.ptr());
	}
	static inline void _CompressedTexSubImage2D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_width, int p_height, GLEnum p_format, int p_imageSize, PackedByteArray p_data) {
		glCompressedTexSubImage2D(p_target, p_level, p_xoffset, p_yoffset, p_width, p_height, p_format, p_imageSize, p_data.ptr());
	}
	static inline void _DeleteBuffers(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteBuffers(p_buffers.size(), buff_stack);
	}
	static inline void _DeleteFramebuffers(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteFramebuffers(p_buffers.size(), buff_stack);
	}
	static inline void _DeleteRenderbuffers(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteRenderbuffers(p_buffers.size(), buff_stack);
	}
	static inline void _DeleteTextures(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteTextures(p_buffers.size(), buff_stack);
	}

	static inline void _DrawElements(GLEnum p_mode, int p_count, GLEnum p_type, int64_t p_indices) {
		glDrawElements(p_mode, p_count, p_type, ((uint8_t *)nullptr) + p_indices);
	}

	static inline PackedInt64Array _GenBuffers(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenBuffers(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = p_n;
		}
		return ret;
	}

	static inline PackedInt64Array _GenFramebuffers(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenFramebuffers(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline PackedInt64Array _GenRenderbuffers(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenRenderbuffers(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline PackedInt64Array _GenTextures(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenTextures(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline Dictionary _GetActiveAttrib(unsigned int p_program, unsigned int p_index) {
		GLenum type;
		GLchar name[256] = {}; // assuming attribute names won't be longer than 100 characters
		GLsizei nameLength;
		GLint size;

		glGetActiveAttrib(p_program, p_index, 256, &nameLength, &size, &type, name);

		Dictionary ret;
		ret["type"] = type;
		ret["name"] = name;
		ret["size"] = size;

		return ret;
	}

	static inline Dictionary _GetActiveUniform(unsigned int p_program, unsigned int p_index) {
		GLenum type;
		GLchar name[256] = {}; // assuming uniform names won't be longer than 100 characters
		GLsizei nameLength;
		GLint size;

		glGetActiveUniform(p_program, p_index, sizeof(name), &nameLength, &size, &type, name);

		Dictionary ret;
		ret["type"] = type;
		ret["name"] = name;
		ret["size"] = size;

		return ret;
	}

	static inline PackedInt64Array _GetAttachedShaders(unsigned int p_program) {
		GLuint shaders[256];
		GLsizei count;

		glGetAttachedShaders(p_program, 256, &count, shaders);

		PackedInt64Array ret;
		ret.resize(count);
		for (GLuint i = 0; i < count; i++) {
			ret.write[i] = shaders[i];
		}
		return ret;
	}

	static inline int _GetAttribLocation(unsigned int p_program, const String &p_name) {
		return glGetAttribLocation(p_program, p_name.utf8().get_data());
	}

	struct GetStruct {
		int glenum;
		int return_vals;
		int sub_enum;
	};

	static GetStruct get_structs[];

	static inline PackedInt64Array _GetBooleanv(GLEnum p_pname) {
		int ret_values = 1;
		int index = 0;
		while (get_structs[index].glenum != 0) {
			if (get_structs[index].glenum == p_pname) {
				if (get_structs[index].sub_enum == 0) {
					ret_values = get_structs[index].return_vals;
				} else {
					GLint ret_count;
					glGetIntegerv(GLenum(get_structs[index].sub_enum), &ret_count);
					ret_values = ret_count;
				}
				break;
			}
			index++;
		}

		unsigned char *data = (unsigned char *)alloca(ret_values * sizeof(unsigned char));
		glGetBooleanv(p_pname, data);
		PackedInt64Array ret;
		ret.resize(ret_values);
		for (int i = 0; i < ret_values; i++) {
			ret.write[i] = data[i];
		}
		return ret;
	}

	static inline PackedInt64Array _GetIntegerv(GLEnum p_pname) {
		int ret_values = 1;
		int index = 0;
		while (get_structs[index].glenum != 0) {
			if (get_structs[index].glenum == p_pname) {
				if (get_structs[index].sub_enum == 0) {
					ret_values = get_structs[index].return_vals;
				} else {
					GLint ret_count;
					glGetIntegerv(GLenum(get_structs[index].sub_enum), &ret_count);
					ret_values = ret_count;
				}
				break;
			}
			index++;
		}

		GLint *data = (GLint *)alloca(ret_values * sizeof(GLint));
		glGetIntegerv(p_pname, data);
		PackedInt64Array ret;
		ret.resize(ret_values);
		for (int i = 0; i < ret_values; i++) {
			ret.write[i] = data[i];
		}
		return ret;
	}

	static inline PackedFloat32Array _GetFloatv(GLEnum p_pname) {
		int ret_values = 1;
		int index = 0;
		while (get_structs[index].glenum != 0) {
			if (get_structs[index].glenum == p_pname) {
				if (get_structs[index].sub_enum == 0) {
					ret_values = get_structs[index].return_vals;
				} else {
					GLint ret_count;
					glGetIntegerv(GLenum(get_structs[index].sub_enum), &ret_count);
					ret_values = ret_count;
				}
				break;
			}
			index++;
		}

		float *data = (float *)alloca(ret_values * sizeof(float));
		glGetFloatv(p_pname, data);
		PackedFloat32Array ret;
		ret.resize(ret_values);
		for (int i = 0; i < ret_values; i++) {
			ret.write[i] = data[i];
		}
		return ret;
	}

	static inline int _GetBufferParameter(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		int param;
		glGetBufferParameteriv(p_target, p_pname, &param);
		return param;
	}

	static inline int _GetFramebufferAttachmentParameter(GLEnum p_target, GLEnum p_attachment, GLEnum p_pname) {
		// GLES3 is Always 1 param
		int param;
		glGetFramebufferAttachmentParameteriv(p_target, p_attachment, p_pname, &param);
		return param;
	}

	static inline int _GetProgram(unsigned int p_program, GLEnum p_pname) {
		int param;
		glGetProgramiv(p_program, p_pname, &param);
		return param;
	}

	static inline String _GetProgramInfoLog(unsigned int p_program) {
		char infolog[4096];
		int len = 0;
		glGetProgramInfoLog(p_program, 4096, &len, infolog);
		return String(infolog, len);
	}

	static inline int _GetRenderbufferParameter(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		int param;
		glGetRenderbufferParameteriv(p_target, p_pname, &param);
		return param;
	}

	static inline int _GetShader(unsigned int p_shader, GLEnum p_pname) {
		// GLES3 is Always 1 param
		int param;
		glGetShaderiv(p_shader, p_pname, &param);
		return param;
	}

	static inline String _GetShaderInfoLog(unsigned int p_program) {
		char infolog[4096];
		int len = 0;
		glGetShaderInfoLog(p_program, 4096, &len, infolog);
		return String(infolog, len);
	}

	static inline Vector3i _GetShaderPrecisionFormat(GLEnum p_shadertype, GLEnum p_precisiontype) {
		int range[2];
		int precision;
		glGetShaderPrecisionFormat(p_shadertype, p_precisiontype, range, &precision);
		return Vector3i(range[0], range[1], precision);
	}

	static inline String _GetShaderSource(unsigned int p_shader) {
		int source_len = 0;
		glGetShaderiv(p_shader, GL_SHADER_SOURCE_LENGTH, &source_len);
		LocalVector<char> chr;
		chr.resize(source_len);
		glGetShaderSource(p_shader, source_len, nullptr, chr.ptr());
		return String(chr.ptr(), chr.size());
	}

	static inline float _GetTexParameterf(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLfloat param;
		glGetTexParameterfv(p_target, p_pname, &param);
		return param;
	}
	static inline float _GetTexParameteri(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLint param;
		glGetTexParameteriv(p_target, p_pname, &param);
		return param;
	}

	static inline Vector4 _GetUniformf(unsigned int p_program, int p_location) {
		GLfloat params[4] = {};
		glGetUniformfv(p_program, p_location, params);
		return Vector4(params[0], params[1], params[2], params[3]);
	}

	static inline Vector4i _GetUniformi(unsigned int p_program, int p_location) {
		GLint params[4] = {};
		glGetUniformiv(p_program, p_location, params);
		return Vector4i(params[0], params[1], params[2], params[3]);
	}

	static inline int _GetUniformLocation(unsigned int p_program, const String &p_name) {
		return glGetUniformLocation(p_program, p_name.utf8().get_data());
	}

	static inline Vector4 _GetVertexAttribf(unsigned int p_index, GLEnum p_pname) {
		GLfloat params[4] = {};
		glGetVertexAttribfv(p_index, p_pname, params);
		return Vector4(params[0], params[1], params[2], params[3]);
	}
	static inline Vector4i _GetVertexAttribi(unsigned int p_index, GLEnum p_pname) {
		GLint params[4] = {};
		glGetVertexAttribiv(p_index, p_pname, params);
		return Vector4i(params[0], params[1], params[2], params[3]);
	}

	static inline int64_t _GetVertexAttribPointer(unsigned int p_index, GLEnum p_pname) {
		void *ptr = nullptr;
		glGetVertexAttribPointerv(p_index, p_pname, &ptr);
		return int64_t(ptr);
	}

	static inline PackedInt64Array _ShaderBinary(int p_count, GLEnum p_binaryFormat, PackedByteArray p_binary) {
		ERR_FAIL_COND_V(p_count <= 0, PackedInt64Array());
		GLuint *handles = (GLuint *)alloca(sizeof(GLuint) * p_count);
		glShaderBinary(p_count, handles, p_binaryFormat, p_binary.ptr(), p_binary.size());

		PackedInt64Array ret;
		ret.resize(p_count);
		for (int i = 0; i < p_count; i++) {
			ret.write[i] = handles[i];
		}
		return ret;
	}

	static inline void _ShaderSource(unsigned int p_shader, const String &p_source) {
		CharString cs = p_source.utf8();
		const char *csc = cs.ptr();
		GLint len = cs.length();
		glShaderSource(p_shader, 1, &csc, &len);
	}

	static inline void _TexImage2D(GLEnum p_target, int p_level, int p_internalformat, int p_width, int p_height, int p_border, GLEnum p_format, GLEnum p_type, PackedByteArray p_pixels) {
		glTexImage2D(p_target, p_level, p_internalformat, p_width, p_height, p_border, p_format, p_type, p_pixels.ptr());
	}

	static inline void _TexSubImage2D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_width, int p_height, GLEnum p_format, GLEnum p_type, PackedByteArray p_pixels) {
		glTexSubImage2D(p_target, p_level, p_xoffset, p_yoffset, p_width, p_height, p_format, p_type, p_pixels.ptr());
	}

	static inline void _Uniform1fv(int p_location, int p_count, PackedFloat32Array p_value) {
		glUniform1fv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform1iv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform1iv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform1uiv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform1uiv(p_location, p_count, (const GLuint *)p_value.ptr());
	}

	static inline void _Uniform2fv(int p_location, int p_count, PackedFloat32Array p_value) {
		glUniform2fv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform2iv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform2iv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform2uiv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform2uiv(p_location, p_count, (const GLuint *)p_value.ptr());
	}

	static inline void _Uniform3fv(int p_location, int p_count, PackedFloat32Array p_value) {
		glUniform3fv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform3iv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform3iv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform3uiv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform3uiv(p_location, p_count, (const GLuint *)p_value.ptr());
	}

	static inline void _Uniform4fv(int p_location, int p_count, PackedFloat32Array p_value) {
		glUniform4fv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform4iv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform4iv(p_location, p_count, p_value.ptr());
	}

	static inline void _Uniform4uiv(int p_location, int p_count, PackedInt32Array p_value) {
		glUniform4uiv(p_location, p_count, (const GLuint *)p_value.ptr());
	}

	static inline void _UniformMatrix2fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix2fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix3fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix3fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix4fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix4fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _VertexAttribPointer(unsigned int p_index, int p_size, GLEnum p_type, bool p_normalized, int p_stride, int64_t p_pointer) {
		glVertexAttribPointer(p_index, p_size, p_type, p_normalized, p_stride, (void *)p_pointer);
	}

	static inline void _DrawRangeElements(GLEnum p_mode, unsigned int p_start, unsigned int p_end, int p_count, GLEnum p_type, int64_t p_indices) {
		glDrawRangeElements(p_mode, p_start, p_end, p_count, p_type, (const void *)p_indices);
	}

	static inline void _TexImage3D(GLEnum p_target, int p_level, int p_internalformat, int p_width, int p_height, int p_depth, int p_border, GLEnum p_format, GLEnum p_type, PackedByteArray p_pixels) {
		glTexImage3D(p_target, p_level, p_internalformat, p_width, p_height, p_depth, p_border, p_format, p_type, p_pixels.ptr());
	}

	static inline void _TexSubImage3D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_zoffset, int p_width, int p_height, int p_depth, GLEnum p_format, GLEnum p_type, PackedByteArray p_pixels) {
		glTexSubImage3D(p_target, p_level, p_xoffset, p_yoffset, p_zoffset, p_width, p_height, p_depth, p_format, p_type, p_pixels.ptr());
	}

	static inline void _CompressedTexImage3D(GLEnum p_target, int p_level, GLEnum p_internalformat, int p_width, int p_height, int p_depth, int p_border, int p_imageSize, PackedByteArray p_data) {
		glCompressedTexImage3D(p_target, p_level, p_internalformat, p_width, p_height, p_depth, p_border, p_imageSize, p_data.ptr());
	}
	static inline void _CompressedTexSubImage3D(GLEnum p_target, int p_level, int p_xoffset, int p_yoffset, int p_zoffset, int p_width, int p_height, int p_depth, GLEnum p_format, int p_imageSize, PackedByteArray p_data) {
		glCompressedTexSubImage3D(p_target, p_level, p_xoffset, p_yoffset, p_zoffset, p_width, p_height, p_depth, p_format, p_imageSize, p_data.ptr());
	}

	static inline PackedInt64Array _GenQueries(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenQueries(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline void _DeleteQueries(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(GLuint) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteQueries(p_buffers.size(), buff_stack);
	}

	static inline int64_t _GetQueryi(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLint res;
		glGetQueryiv(p_target, p_pname, &res);
		return res;
	}
	static inline int64_t _GetQueryObjectui(unsigned int p_id, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLuint res;
		glGetQueryObjectuiv(p_id, p_pname, &res);
		return res;
	}

	static inline int64_t _GetBufferPointerv(GLEnum p_target, GLEnum p_pname) {
		void *buffer = nullptr;
		glGetBufferPointerv(p_target, p_pname, &buffer);
		return int64_t(buffer);
	}

	static inline void _DrawBuffers(PackedInt32Array p_buffers) {
		GLenum *buffers = (GLenum *)alloca(sizeof(GLenum) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buffers[i] = GLenum(p_buffers[i]);
		}
		glDrawBuffers(p_buffers.size(), buffers);
	}

	static inline void _UniformMatrix2x3fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix2x3fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix3x2fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix3x2fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix2x4fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix2x4fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix4x2fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix4x2fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix3x4fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix3x4fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline void _UniformMatrix4x3fv(int p_location, int p_count, bool p_transpose, PackedFloat32Array p_value) {
		glUniformMatrix4x3fv(p_location, p_count, p_transpose, p_value.ptr());
	}

	static inline PackedInt64Array _GenVertexArrays(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenVertexArrays(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline void _DeleteVertexArrays(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteVertexArrays(p_buffers.size(), buff_stack);
	}

	static inline void _VertexAttribIPointer(unsigned int p_index, int p_size, GLEnum p_type, int p_stride, int64_t p_pointer) {
		glVertexAttribIPointer(p_index, p_size, p_type, p_stride, (void *)p_pointer);
	}

	static inline Vector4i _GetVertexAttribIiv(unsigned int p_index, GLEnum p_pname) {
		GLint params[4] = {};
		glGetVertexAttribIiv(p_index, p_pname, params);
		return Vector4i(params[0], params[1], params[2], params[3]);
	}

	static inline Vector4i _GetVertexAttribIuiv(unsigned int p_index, GLEnum p_pname) {
		GLuint params[4] = {};
		glGetVertexAttribIuiv(p_index, p_pname, params);
		return Vector4i(params[0], params[1], params[2], params[3]);
	}

	static inline Vector4i _GetUniformui(unsigned int p_program, int p_location) {
		GLuint params[4] = {};
		glGetUniformuiv(p_program, p_location, params);
		return Vector4i(params[0], params[1], params[2], params[3]);
	}

	static inline int _GetFragDataLocation(unsigned int p_program, const String &p_name) {
		return glGetFragDataLocation(p_program, p_name.utf8().get_data());
	}

	static inline void _ClearBufferiv(GLEnum p_buffer, int p_drawbuffer, PackedInt32Array p_value) {
		glClearBufferiv(p_buffer, p_drawbuffer, p_value.ptr());
	}
	static inline void _ClearBufferuiv(GLEnum p_buffer, int p_drawbuffer, PackedInt32Array p_value) {
		glClearBufferuiv(p_buffer, p_drawbuffer, (const GLuint *)p_value.ptr());
	}
	static inline void _ClearBufferfv(GLEnum p_buffer, int p_drawbuffer, PackedFloat32Array p_value) {
		glClearBufferfv(p_buffer, p_drawbuffer, p_value.ptr());
	}

	static inline PackedInt64Array _GetUniformIndices(unsigned int p_program, PackedStringArray p_names) {
		Vector<CharString> cs;
		Vector<const char *> csptr;
		Vector<GLuint> indices;
		cs.resize(p_names.size());
		csptr.resize(p_names.size());
		indices.remove_at(p_names.size());

		for (int i = 0; i < p_names.size(); i++) {
			cs.write[i] = p_names[i].utf8();
			csptr.write[i] = cs[i].ptr();
		}

		glGetUniformIndices(p_program, p_names.size(), csptr.ptr(), indices.ptrw());

		PackedInt64Array ret;
		ret.resize(p_names.size());
		for (int i = 0; i < p_names.size(); i++) {
			ret.write[i] = indices[i];
		}
		return ret;
	}
	/*
	static inline void GetActiveUniformsiv( unsigned int p_program, int p_uniformCount, PackedInt64Array p_indices, GLEnum p_pname, int * p_params) {
		Vector<GLuint> indices;
		glGetActiveUniformsiv( p_program,  p_uniformCount,  p_uniformIndices,  p_pname,  p_params );
	}*/

	static inline unsigned int _GetUniformBlockIndex(unsigned int p_program, const String &p_uniformBlockName) {
		return glGetUniformBlockIndex(p_program, p_uniformBlockName.utf8().get_data());
	}
	/*
	static inline void GetActiveUniformBlockiv( unsigned int p_program, unsigned int p_uniformBlockIndex, GLEnum p_pname, int * p_params) {
		glGetActiveUniformBlockiv( p_program,  p_uniformBlockIndex,  p_pname,  p_params );
	}
	static inline void GetActiveUniformBlockName( unsigned int p_program, unsigned int p_uniformBlockIndex, int p_bufSize, int * p_length, char * p_uniformBlockName) {
		glGetActiveUniformBlockName( p_program,  p_uniformBlockIndex,  p_bufSize,  p_length,  p_uniformBlockName );
	}*/

	static inline void _DrawElementsInstanced(GLEnum p_mode, int p_count, GLEnum p_type, int64_t p_indices, int p_instancecount) {
		glDrawElementsInstanced(p_mode, p_count, p_type, (void *)p_indices, p_instancecount);
	}

	static inline PackedInt64Array _GenSamplers(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenSamplers(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline void _DeleteSamplers(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteSamplers(p_buffers.size(), buff_stack);
	}

	static inline float _GetSamplerParameterf(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLfloat param;
		glGetSamplerParameterfv(p_target, p_pname, &param);
		return param;
	}
	static inline float _GetSamplerParameteri(GLEnum p_target, GLEnum p_pname) {
		// GLES3 is Always 1 param
		GLint param;
		glGetSamplerParameteriv(p_target, p_pname, &param);
		return param;
	}

	static inline PackedInt64Array _GenTransformFeedbacks(int p_n) {
		ERR_FAIL_COND_V(p_n < 0, PackedInt64Array());
		GLuint *buffers = (GLuint *)alloca(sizeof(GLuint *) * p_n);
		glGenTransformFeedbacks(p_n, buffers);
		PackedInt64Array ret;
		ret.resize(p_n);
		for (int i = 0; i < p_n; i++) {
			ret.write[i] = buffers[i];
		}
		return ret;
	}

	static inline void _DeleteTransformFeedbacks(PackedInt64Array p_buffers) {
		GLuint *buff_stack = (GLuint *)alloca(sizeof(int) * p_buffers.size());
		for (int i = 0; i < p_buffers.size(); i++) {
			buff_stack[i] = p_buffers[i];
		}
		glDeleteTransformFeedbacks(p_buffers.size(), buff_stack);
	}

	static inline PackedByteArray _GetProgramBinary(unsigned int p_program, Array p_binaryFormat) {
		GLint len = 0;
		glGetProgramiv(p_program, GL_PROGRAM_BINARY_LENGTH, &len);
		PackedByteArray ret;
		ret.resize(len);
		GLenum format;
		glGetProgramBinary(p_program, len, nullptr, &format, ret.ptrw());
		p_binaryFormat.resize(1);
		p_binaryFormat[1] = format;
		return ret;
	}

	static inline void _ProgramBinary(unsigned int p_program, GLEnum p_binaryFormat, PackedByteArray p_binary) {
		glProgramBinary(p_program, p_binaryFormat, p_binary.ptr(), p_binary.size());
	}

	static inline void _InvalidateFramebuffer(GLEnum p_target, PackedInt32Array p_attachments) {
		GLenum *attachments = (GLenum *)alloca(sizeof(GLenum) * p_attachments.size());
		for (int i = 0; i < p_attachments.size(); i++) {
			attachments[i] = GLenum(p_attachments[i]);
		}

		glInvalidateFramebuffer(p_target, p_attachments.size(), (const GLenum *)attachments);
	}

	static inline void _InvalidateSubFramebuffer(GLEnum p_target, PackedInt32Array p_attachments, int p_x, int p_y, int p_width, int p_height) {
		GLenum *attachments = (GLenum *)alloca(sizeof(GLenum) * p_attachments.size());
		for (int i = 0; i < p_attachments.size(); i++) {
			attachments[i] = GLenum(p_attachments[i]);
		}

		glInvalidateSubFramebuffer(p_target, p_attachments.size(), (const GLenum *)attachments, p_x, p_y, p_width, p_height);
	}
};

VARIANT_ENUM_CAST(GLES3::GLEnum);

#endif

#endif // GLES3_H
