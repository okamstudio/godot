/*
** The OpenGL Extension Wrangler Library
** Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
** Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
** Copyright (C) 2002, Lev Povalahev
** All rights reserved.
** 
** Redistribution and use in source and binary forms, with or without 
** modification, are permitted provided that the following conditions are met:
** 
** * Redistributions of source code must retain the above copyright notice, 
**   this list of conditions and the following disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice, 
**   this list of conditions and the following disclaimer in the documentation 
**   and/or other materials provided with the distribution.
** * The name of the author may be used to endorse or promote products 
**   derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
** THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Mesa 3-D graphics library
 * Version:  7.0
 *
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
** Copyright (c) 2007 The Khronos Group Inc.
** 
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
** 
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
** 
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#ifndef __glew_h__
#define __glew_h__
#define __GLEW_H__

#define GLEW_STATIC

#if defined(__gl_h_) || defined(__GL_H__) || defined(__X_GL_H)
#error gl.h included before glew.h
#endif
#if defined(__glext_h_) || defined(__GLEXT_H_)
#error glext.h included before glew.h
#endif
#if defined(__gl_ATI_h_)
#error glATI.h included before glew.h
#endif

#define __gl_h_
#define __GL_H__
#define __X_GL_H
#define __glext_h_
#define __GLEXT_H_
#define __gl_ATI_h_

#if defined(_WIN32)

/*
 * GLEW does not include <windows.h> to avoid name space pollution.
 * GL needs GLAPI and GLAPIENTRY, GLU needs APIENTRY, CALLBACK, and wchar_t
 * defined properly.
 */
/* <windef.h> */
#ifndef APIENTRY
#define GLEW_APIENTRY_DEFINED
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define APIENTRY __stdcall
#  elif (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED) || defined(__BORLANDC__)
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif
#endif
#ifndef GLAPI
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define GLAPI extern
#  endif
#endif
/* <winnt.h> */
#ifndef CALLBACK
#define GLEW_CALLBACK_DEFINED
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define CALLBACK __attribute__ ((__stdcall__))
#  elif (defined(_M_MRX000) || defined(_M_IX86) || defined(_M_ALPHA) || defined(_M_PPC)) && !defined(MIDL_PASS)
#    define CALLBACK __stdcall
#  else
#    define CALLBACK
#  endif
#endif
/* <wingdi.h> and <winnt.h> */
#ifndef WINGDIAPI
#define GLEW_WINGDIAPI_DEFINED
#define WINGDIAPI __declspec(dllimport)
#endif
/* <ctype.h> */
#if (defined(_MSC_VER) || defined(__BORLANDC__)) && !defined(_WCHAR_T_DEFINED)
typedef unsigned short wchar_t;
#  define _WCHAR_T_DEFINED
#endif
/* <stddef.h> */
#if !defined(_W64)
#  if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && defined(_MSC_VER) && _MSC_VER >= 1300
#    define _W64 __w64
#  else
#    define _W64
#  endif
#endif
#if !defined(_PTRDIFF_T_DEFINED) && !defined(_PTRDIFF_T_)
#  ifdef _WIN64
//typedef __int64 ptrdiff_t;
typedef long int ptrdiff_t;
#  else
typedef _W64 int ptrdiff_t;
#  endif
#  define _PTRDIFF_T_DEFINED
#  define _PTRDIFF_T_
#endif

#ifndef GLAPI
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define GLAPI extern
#  else
#    define GLAPI WINGDIAPI
#  endif
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

/*
 * GLEW_STATIC needs to be set when using the static version.
 * GLEW_BUILD is set when building the DLL version.
 */
#ifdef GLEW_STATIC
#  define GLEWAPI extern
#else
#  ifdef GLEW_BUILD
#    define GLEWAPI extern __declspec(dllexport)
#  else
#    define GLEWAPI extern __declspec(dllimport)
#  endif
#endif

#else /* _UNIX */

/*
 * Needed for ptrdiff_t in turn needed by VBO.  This is defined by ISO
 * C.  On my system, this amounts to _3 lines_ of included code, all of
 * them pretty much harmless.  If you know of a way of detecting 32 vs
 * 64 _targets_ at compile time you are free to replace this with
 * something that's portable.  For now, _this_ is the portable solution.
 * (mem, 2004-01-04)
 */

#include <stddef.h>

/* SGI MIPSPro doesn't like stdint.h in C++ mode */

#if defined(__sgi) && !defined(__GNUC__)
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define GLEW_APIENTRY_DEFINED
#define APIENTRY
#define GLEWAPI extern

/* <glu.h> */
#ifndef GLAPI
#define GLAPI extern
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#endif /* _WIN32 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------- GL_VERSION_1_1 ---------------------------- */

#ifndef GL_VERSION_1_1
#define GL_VERSION_1_1 1

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
#if defined(_MSC_VER)
#  if _MSC_VER < 1400
typedef __int64 GLint64EXT;
typedef unsigned __int64 GLuint64EXT;
#  else
typedef signed long long GLint64EXT;
typedef unsigned long long GLuint64EXT;
#  endif
#else
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#include <inttypes.h>
#  endif
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
#endif
typedef GLint64EXT  GLint64;
typedef GLuint64EXT GLuint64;
typedef struct __GLsync *GLsync;

typedef char GLchar;
typedef void (APIENTRY *GLDEBUGPROCAMD)(GLuint id,
                                        GLenum category,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar* message,
                                        GLvoid* userParam);

/* For ARB_debug_output */

typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar* message,
                                        GLvoid* userParam);

/* For GL_ARB_cl_event */

typedef struct _cl_context *cl_context;
typedef struct _cl_event *cl_event;

#define GL_ACCUM 0x0100
#define GL_LOAD 0x0101
#define GL_RETURN 0x0102
#define GL_MULT 0x0103
#define GL_ADD 0x0104
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_CURRENT_BIT 0x00000001
#define GL_POINT_BIT 0x00000002
#define GL_LINE_BIT 0x00000004
#define GL_POLYGON_BIT 0x00000008
#define GL_POLYGON_STIPPLE_BIT 0x00000010
#define GL_PIXEL_MODE_BIT 0x00000020
#define GL_LIGHTING_BIT 0x00000040
#define GL_FOG_BIT 0x00000080
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_ACCUM_BUFFER_BIT 0x00000200
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_VIEWPORT_BIT 0x00000800
#define GL_TRANSFORM_BIT 0x00001000
#define GL_ENABLE_BIT 0x00002000
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_HINT_BIT 0x00008000
#define GL_EVAL_BIT 0x00010000
#define GL_LIST_BIT 0x00020000
#define GL_TEXTURE_BIT 0x00040000
#define GL_SCISSOR_BIT 0x00080000
#define GL_ALL_ATTRIB_BITS 0x000fffff
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007
#define GL_QUAD_STRIP 0x0008
#define GL_POLYGON 0x0009
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_2_BYTES 0x1407
#define GL_3_BYTES 0x1408
#define GL_4_BYTES 0x1409
#define GL_DOUBLE 0x140A
#define GL_NONE 0
#define GL_FRONT_LEFT 0x0400
#define GL_FRONT_RIGHT 0x0401
#define GL_BACK_LEFT 0x0402
#define GL_BACK_RIGHT 0x0403
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_LEFT 0x0406
#define GL_RIGHT 0x0407
#define GL_FRONT_AND_BACK 0x0408
#define GL_AUX0 0x0409
#define GL_AUX1 0x040A
#define GL_AUX2 0x040B
#define GL_AUX3 0x040C
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_OUT_OF_MEMORY 0x0505
#define GL_2D 0x0600
#define GL_3D 0x0601
#define GL_3D_COLOR 0x0602
#define GL_3D_COLOR_TEXTURE 0x0603
#define GL_4D_COLOR_TEXTURE 0x0604
#define GL_PASS_THROUGH_TOKEN 0x0700
#define GL_POINT_TOKEN 0x0701
#define GL_LINE_TOKEN 0x0702
#define GL_POLYGON_TOKEN 0x0703
#define GL_BITMAP_TOKEN 0x0704
#define GL_DRAW_PIXEL_TOKEN 0x0705
#define GL_COPY_PIXEL_TOKEN 0x0706
#define GL_LINE_RESET_TOKEN 0x0707
#define GL_EXP 0x0800
#define GL_EXP2 0x0801
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_COEFF 0x0A00
#define GL_ORDER 0x0A01
#define GL_DOMAIN 0x0A02
#define GL_CURRENT_COLOR 0x0B00
#define GL_CURRENT_INDEX 0x0B01
#define GL_CURRENT_NORMAL 0x0B02
#define GL_CURRENT_TEXTURE_COORDS 0x0B03
#define GL_CURRENT_RASTER_COLOR 0x0B04
#define GL_CURRENT_RASTER_INDEX 0x0B05
#define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06
#define GL_CURRENT_RASTER_POSITION 0x0B07
#define GL_CURRENT_RASTER_POSITION_VALID 0x0B08
#define GL_CURRENT_RASTER_DISTANCE 0x0B09
#define GL_POINT_SMOOTH 0x0B10
#define GL_POINT_SIZE 0x0B11
#define GL_POINT_SIZE_RANGE 0x0B12
#define GL_POINT_SIZE_GRANULARITY 0x0B13
#define GL_LINE_SMOOTH 0x0B20
#define GL_LINE_WIDTH 0x0B21
#define GL_LINE_WIDTH_RANGE 0x0B22
#define GL_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_LINE_STIPPLE 0x0B24
#define GL_LINE_STIPPLE_PATTERN 0x0B25
#define GL_LINE_STIPPLE_REPEAT 0x0B26
#define GL_LIST_MODE 0x0B30
#define GL_MAX_LIST_NESTING 0x0B31
#define GL_LIST_BASE 0x0B32
#define GL_LIST_INDEX 0x0B33
#define GL_POLYGON_MODE 0x0B40
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_POLYGON_STIPPLE 0x0B42
#define GL_EDGE_FLAG 0x0B43
#define GL_CULL_FACE 0x0B44
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_LIGHTING 0x0B50
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_SHADE_MODEL 0x0B54
#define GL_COLOR_MATERIAL_FACE 0x0B55
#define GL_COLOR_MATERIAL_PARAMETER 0x0B56
#define GL_COLOR_MATERIAL 0x0B57
#define GL_FOG 0x0B60
#define GL_FOG_INDEX 0x0B61
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_MODE 0x0B65
#define GL_FOG_COLOR 0x0B66
#define GL_DEPTH_RANGE 0x0B70
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_ACCUM_CLEAR_VALUE 0x0B80
#define GL_STENCIL_TEST 0x0B90
#define GL_STENCIL_CLEAR_VALUE 0x0B91
#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_MATRIX_MODE 0x0BA0
#define GL_NORMALIZE 0x0BA1
#define GL_VIEWPORT 0x0BA2
#define GL_MODELVIEW_STACK_DEPTH 0x0BA3
#define GL_PROJECTION_STACK_DEPTH 0x0BA4
#define GL_TEXTURE_STACK_DEPTH 0x0BA5
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_TEXTURE_MATRIX 0x0BA8
#define GL_ATTRIB_STACK_DEPTH 0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH 0x0BB1
#define GL_ALPHA_TEST 0x0BC0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_DITHER 0x0BD0
#define GL_BLEND_DST 0x0BE0
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND 0x0BE2
#define GL_LOGIC_OP_MODE 0x0BF0
#define GL_INDEX_LOGIC_OP 0x0BF1
#define GL_COLOR_LOGIC_OP 0x0BF2
#define GL_AUX_BUFFERS 0x0C00
#define GL_DRAW_BUFFER 0x0C01
#define GL_READ_BUFFER 0x0C02
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11
#define GL_INDEX_CLEAR_VALUE 0x0C20
#define GL_INDEX_WRITEMASK 0x0C21
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_INDEX_MODE 0x0C30
#define GL_RGBA_MODE 0x0C31
#define GL_DOUBLEBUFFER 0x0C32
#define GL_STEREO 0x0C33
#define GL_RENDER_MODE 0x0C40
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_POINT_SMOOTH_HINT 0x0C51
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_POLYGON_SMOOTH_HINT 0x0C53
#define GL_FOG_HINT 0x0C54
#define GL_TEXTURE_GEN_S 0x0C60
#define GL_TEXTURE_GEN_T 0x0C61
#define GL_TEXTURE_GEN_R 0x0C62
#define GL_TEXTURE_GEN_Q 0x0C63
#define GL_PIXEL_MAP_I_TO_I 0x0C70
#define GL_PIXEL_MAP_S_TO_S 0x0C71
#define GL_PIXEL_MAP_I_TO_R 0x0C72
#define GL_PIXEL_MAP_I_TO_G 0x0C73
#define GL_PIXEL_MAP_I_TO_B 0x0C74
#define GL_PIXEL_MAP_I_TO_A 0x0C75
#define GL_PIXEL_MAP_R_TO_R 0x0C76
#define GL_PIXEL_MAP_G_TO_G 0x0C77
#define GL_PIXEL_MAP_B_TO_B 0x0C78
#define GL_PIXEL_MAP_A_TO_A 0x0C79
#define GL_PIXEL_MAP_I_TO_I_SIZE 0x0CB0
#define GL_PIXEL_MAP_S_TO_S_SIZE 0x0CB1
#define GL_PIXEL_MAP_I_TO_R_SIZE 0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE 0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE 0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE 0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE 0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE 0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE 0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE 0x0CB9
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#define GL_UNPACK_LSB_FIRST 0x0CF1
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_SWAP_BYTES 0x0D00
#define GL_PACK_LSB_FIRST 0x0D01
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_SKIP_ROWS 0x0D03
#define GL_PACK_SKIP_PIXELS 0x0D04
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAP_COLOR 0x0D10
#define GL_MAP_STENCIL 0x0D11
#define GL_INDEX_SHIFT 0x0D12
#define GL_INDEX_OFFSET 0x0D13
#define GL_RED_SCALE 0x0D14
#define GL_RED_BIAS 0x0D15
#define GL_ZOOM_X 0x0D16
#define GL_ZOOM_Y 0x0D17
#define GL_GREEN_SCALE 0x0D18
#define GL_GREEN_BIAS 0x0D19
#define GL_BLUE_SCALE 0x0D1A
#define GL_BLUE_BIAS 0x0D1B
#define GL_ALPHA_SCALE 0x0D1C
#define GL_ALPHA_BIAS 0x0D1D
#define GL_DEPTH_SCALE 0x0D1E
#define GL_DEPTH_BIAS 0x0D1F
#define GL_MAX_EVAL_ORDER 0x0D30
#define GL_MAX_LIGHTS 0x0D31
#define GL_MAX_CLIP_PLANES 0x0D32
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_PIXEL_MAP_TABLE 0x0D34
#define GL_MAX_ATTRIB_STACK_DEPTH 0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH 0x0D36
#define GL_MAX_NAME_STACK_DEPTH 0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH 0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH 0x0D39
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH 0x0D3B
#define GL_SUBPIXEL_BITS 0x0D50
#define GL_INDEX_BITS 0x0D51
#define GL_RED_BITS 0x0D52
#define GL_GREEN_BITS 0x0D53
#define GL_BLUE_BITS 0x0D54
#define GL_ALPHA_BITS 0x0D55
#define GL_DEPTH_BITS 0x0D56
#define GL_STENCIL_BITS 0x0D57
#define GL_ACCUM_RED_BITS 0x0D58
#define GL_ACCUM_GREEN_BITS 0x0D59
#define GL_ACCUM_BLUE_BITS 0x0D5A
#define GL_ACCUM_ALPHA_BITS 0x0D5B
#define GL_NAME_STACK_DEPTH 0x0D70
#define GL_AUTO_NORMAL 0x0D80
#define GL_MAP1_COLOR_4 0x0D90
#define GL_MAP1_INDEX 0x0D91
#define GL_MAP1_NORMAL 0x0D92
#define GL_MAP1_TEXTURE_COORD_1 0x0D93
#define GL_MAP1_TEXTURE_COORD_2 0x0D94
#define GL_MAP1_TEXTURE_COORD_3 0x0D95
#define GL_MAP1_TEXTURE_COORD_4 0x0D96
#define GL_MAP1_VERTEX_3 0x0D97
#define GL_MAP1_VERTEX_4 0x0D98
#define GL_MAP2_COLOR_4 0x0DB0
#define GL_MAP2_INDEX 0x0DB1
#define GL_MAP2_NORMAL 0x0DB2
#define GL_MAP2_TEXTURE_COORD_1 0x0DB3
#define GL_MAP2_TEXTURE_COORD_2 0x0DB4
#define GL_MAP2_TEXTURE_COORD_3 0x0DB5
#define GL_MAP2_TEXTURE_COORD_4 0x0DB6
#define GL_MAP2_VERTEX_3 0x0DB7
#define GL_MAP2_VERTEX_4 0x0DB8
#define GL_MAP1_GRID_DOMAIN 0x0DD0
#define GL_MAP1_GRID_SEGMENTS 0x0DD1
#define GL_MAP2_GRID_DOMAIN 0x0DD2
#define GL_MAP2_GRID_SEGMENTS 0x0DD3
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_FEEDBACK_BUFFER_POINTER 0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE 0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE 0x0DF2
#define GL_SELECTION_BUFFER_POINTER 0x0DF3
#define GL_SELECTION_BUFFER_SIZE 0x0DF4
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_BORDER 0x1005
#define GL_DONT_CARE 0x1100
#define GL_FASTEST 0x1101
#define GL_NICEST 0x1102
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_POSITION 0x1203
#define GL_SPOT_DIRECTION 0x1204
#define GL_SPOT_EXPONENT 0x1205
#define GL_SPOT_CUTOFF 0x1206
#define GL_CONSTANT_ATTENUATION 0x1207
#define GL_LINEAR_ATTENUATION 0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_COMPILE 0x1300
#define GL_COMPILE_AND_EXECUTE 0x1301
#define GL_CLEAR 0x1500
#define GL_AND 0x1501
#define GL_AND_REVERSE 0x1502
#define GL_COPY 0x1503
#define GL_AND_INVERTED 0x1504
#define GL_NOOP 0x1505
#define GL_XOR 0x1506
#define GL_OR 0x1507
#define GL_NOR 0x1508
#define GL_EQUIV 0x1509
#define GL_INVERT 0x150A
#define GL_OR_REVERSE 0x150B
#define GL_COPY_INVERTED 0x150C
#define GL_OR_INVERTED 0x150D
#define GL_NAND 0x150E
#define GL_SET 0x150F
#define GL_EMISSION 0x1600
#define GL_SHININESS 0x1601
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_COLOR_INDEXES 0x1603
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_STENCIL 0x1802
#define GL_COLOR_INDEX 0x1900
#define GL_STENCIL_INDEX 0x1901
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_BITMAP 0x1A00
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_RENDER 0x1C00
#define GL_FEEDBACK 0x1C01
#define GL_SELECT 0x1C02
#define GL_FLAT 0x1D00
#define GL_SMOOTH 0x1D01
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_S 0x2000
#define GL_T 0x2001
#define GL_R 0x2002
#define GL_Q 0x2003
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_TEXTURE_ENV 0x2300
#define GL_EYE_LINEAR 0x2400
#define GL_OBJECT_LINEAR 0x2401
#define GL_SPHERE_MAP 0x2402
#define GL_TEXTURE_GEN_MODE 0x2500
#define GL_OBJECT_PLANE 0x2501
#define GL_EYE_PLANE 0x2502
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP 0x2900
#define GL_REPEAT 0x2901
#define GL_CLIENT_PIXEL_STORE_BIT 0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT 0x00000002
#define GL_CLIENT_ALL_ATTRIB_BITS 0xffffffff
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_POLYGON_OFFSET_POINT 0x2A01
#define GL_POLYGON_OFFSET_LINE 0x2A02
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_ALPHA4 0x803B
#define GL_ALPHA8 0x803C
#define GL_ALPHA12 0x803D
#define GL_ALPHA16 0x803E
#define GL_LUMINANCE4 0x803F
#define GL_LUMINANCE8 0x8040
#define GL_LUMINANCE12 0x8041
#define GL_LUMINANCE16 0x8042
#define GL_LUMINANCE4_ALPHA4 0x8043
#define GL_LUMINANCE6_ALPHA2 0x8044
#define GL_LUMINANCE8_ALPHA8 0x8045
#define GL_LUMINANCE12_ALPHA4 0x8046
#define GL_LUMINANCE12_ALPHA12 0x8047
#define GL_LUMINANCE16_ALPHA16 0x8048
#define GL_INTENSITY 0x8049
#define GL_INTENSITY4 0x804A
#define GL_INTENSITY8 0x804B
#define GL_INTENSITY12 0x804C
#define GL_INTENSITY16 0x804D
#define GL_R3_G3_B2 0x2A10
#define GL_RGB4 0x804F
#define GL_RGB5 0x8050
#define GL_RGB8 0x8051
#define GL_RGB10 0x8052
#define GL_RGB12 0x8053
#define GL_RGB16 0x8054
#define GL_RGBA2 0x8055
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_RGBA8 0x8058
#define GL_RGB10_A2 0x8059
#define GL_RGBA12 0x805A
#define GL_RGBA16 0x805B
#define GL_TEXTURE_RED_SIZE 0x805C
#define GL_TEXTURE_GREEN_SIZE 0x805D
#define GL_TEXTURE_BLUE_SIZE 0x805E
#define GL_TEXTURE_ALPHA_SIZE 0x805F
#define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#define GL_TEXTURE_INTENSITY_SIZE 0x8061
#define GL_PROXY_TEXTURE_1D 0x8063
#define GL_PROXY_TEXTURE_2D 0x8064
#define GL_TEXTURE_PRIORITY 0x8066
#define GL_TEXTURE_RESIDENT 0x8067
#define GL_TEXTURE_BINDING_1D 0x8068
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_COLOR_ARRAY 0x8076
#define GL_INDEX_ARRAY 0x8077
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_EDGE_FLAG_ARRAY 0x8079
#define GL_VERTEX_ARRAY_SIZE 0x807A
#define GL_VERTEX_ARRAY_TYPE 0x807B
#define GL_VERTEX_ARRAY_STRIDE 0x807C
#define GL_NORMAL_ARRAY_TYPE 0x807E
#define GL_NORMAL_ARRAY_STRIDE 0x807F
#define GL_COLOR_ARRAY_SIZE 0x8081
#define GL_COLOR_ARRAY_TYPE 0x8082
#define GL_COLOR_ARRAY_STRIDE 0x8083
#define GL_INDEX_ARRAY_TYPE 0x8085
#define GL_INDEX_ARRAY_STRIDE 0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE 0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE 0x808C
#define GL_VERTEX_ARRAY_POINTER 0x808E
#define GL_NORMAL_ARRAY_POINTER 0x808F
#define GL_COLOR_ARRAY_POINTER 0x8090
#define GL_INDEX_ARRAY_POINTER 0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER 0x8093
#define GL_V2F 0x2A20
#define GL_V3F 0x2A21
#define GL_C4UB_V2F 0x2A22
#define GL_C4UB_V3F 0x2A23
#define GL_C3F_V3F 0x2A24
#define GL_N3F_V3F 0x2A25
#define GL_C4F_N3F_V3F 0x2A26
#define GL_T2F_V3F 0x2A27
#define GL_T4F_V4F 0x2A28
#define GL_T2F_C4UB_V3F 0x2A29
#define GL_T2F_C3F_V3F 0x2A2A
#define GL_T2F_N3F_V3F 0x2A2B
#define GL_T2F_C4F_N3F_V3F 0x2A2C
#define GL_T4F_C4F_N3F_V4F 0x2A2D
#define GL_LOGIC_OP GL_INDEX_LOGIC_OP
#define GL_TEXTURE_COMPONENTS GL_TEXTURE_INTERNAL_FORMAT
#define GL_COLOR_INDEX1_EXT 0x80E2
#define GL_COLOR_INDEX2_EXT 0x80E3
#define GL_COLOR_INDEX4_EXT 0x80E4
#define GL_COLOR_INDEX8_EXT 0x80E5
#define GL_COLOR_INDEX12_EXT 0x80E6
#define GL_COLOR_INDEX16_EXT 0x80E7

GLAPI void GLAPIENTRY glAccum (GLenum op, GLfloat value);
GLAPI void GLAPIENTRY glAlphaFunc (GLenum func, GLclampf ref);
GLAPI GLboolean GLAPIENTRY glAreTexturesResident (GLsizei n, const GLuint *textures, GLboolean *residences);
GLAPI void GLAPIENTRY glArrayElement (GLint i);
GLAPI void GLAPIENTRY glBegin (GLenum mode);
GLAPI void GLAPIENTRY glBindTexture (GLenum target, GLuint texture);
GLAPI void GLAPIENTRY glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
GLAPI void GLAPIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor);
GLAPI void GLAPIENTRY glCallList (GLuint list);
GLAPI void GLAPIENTRY glCallLists (GLsizei n, GLenum type, const GLvoid *lists);
GLAPI void GLAPIENTRY glClear (GLbitfield mask);
GLAPI void GLAPIENTRY glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void GLAPIENTRY glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GLAPI void GLAPIENTRY glClearDepth (GLclampd depth);
GLAPI void GLAPIENTRY glClearIndex (GLfloat c);
GLAPI void GLAPIENTRY glClearStencil (GLint s);
GLAPI void GLAPIENTRY glClipPlane (GLenum plane, const GLdouble *equation);
GLAPI void GLAPIENTRY glColor3b (GLbyte red, GLbyte green, GLbyte blue);
GLAPI void GLAPIENTRY glColor3bv (const GLbyte *v);
GLAPI void GLAPIENTRY glColor3d (GLdouble red, GLdouble green, GLdouble blue);
GLAPI void GLAPIENTRY glColor3dv (const GLdouble *v);
GLAPI void GLAPIENTRY glColor3f (GLfloat red, GLfloat green, GLfloat blue);
GLAPI void GLAPIENTRY glColor3fv (const GLfloat *v);
GLAPI void GLAPIENTRY glColor3i (GLint red, GLint green, GLint blue);
GLAPI void GLAPIENTRY glColor3iv (const GLint *v);
GLAPI void GLAPIENTRY glColor3s (GLshort red, GLshort green, GLshort blue);
GLAPI void GLAPIENTRY glColor3sv (const GLshort *v);
GLAPI void GLAPIENTRY glColor3ub (GLubyte red, GLubyte green, GLubyte blue);
GLAPI void GLAPIENTRY glColor3ubv (const GLubyte *v);
GLAPI void GLAPIENTRY glColor3ui (GLuint red, GLuint green, GLuint blue);
GLAPI void GLAPIENTRY glColor3uiv (const GLuint *v);
GLAPI void GLAPIENTRY glColor3us (GLushort red, GLushort green, GLushort blue);
GLAPI void GLAPIENTRY glColor3usv (const GLushort *v);
GLAPI void GLAPIENTRY glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
GLAPI void GLAPIENTRY glColor4bv (const GLbyte *v);
GLAPI void GLAPIENTRY glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
GLAPI void GLAPIENTRY glColor4dv (const GLdouble *v);
GLAPI void GLAPIENTRY glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void GLAPIENTRY glColor4fv (const GLfloat *v);
GLAPI void GLAPIENTRY glColor4i (GLint red, GLint green, GLint blue, GLint alpha);
GLAPI void GLAPIENTRY glColor4iv (const GLint *v);
GLAPI void GLAPIENTRY glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha);
GLAPI void GLAPIENTRY glColor4sv (const GLshort *v);
GLAPI void GLAPIENTRY glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
GLAPI void GLAPIENTRY glColor4ubv (const GLubyte *v);
GLAPI void GLAPIENTRY glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha);
GLAPI void GLAPIENTRY glColor4uiv (const GLuint *v);
GLAPI void GLAPIENTRY glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha);
GLAPI void GLAPIENTRY glColor4usv (const GLushort *v);
GLAPI void GLAPIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GLAPI void GLAPIENTRY glColorMaterial (GLenum face, GLenum mode);
GLAPI void GLAPIENTRY glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
GLAPI void GLAPIENTRY glCopyTexImage1D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
GLAPI void GLAPIENTRY glCopyTexImage2D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
GLAPI void GLAPIENTRY glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
GLAPI void GLAPIENTRY glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glCullFace (GLenum mode);
GLAPI void GLAPIENTRY glDeleteLists (GLuint list, GLsizei range);
GLAPI void GLAPIENTRY glDeleteTextures (GLsizei n, const GLuint *textures);
GLAPI void GLAPIENTRY glDepthFunc (GLenum func);
GLAPI void GLAPIENTRY glDepthMask (GLboolean flag);
GLAPI void GLAPIENTRY glDepthRange (GLclampd zNear, GLclampd zFar);
GLAPI void GLAPIENTRY glDisable (GLenum cap);
GLAPI void GLAPIENTRY glDisableClientState (GLenum array);
GLAPI void GLAPIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count);
GLAPI void GLAPIENTRY glDrawBuffer (GLenum mode);
GLAPI void GLAPIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
GLAPI void GLAPIENTRY glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glEdgeFlag (GLboolean flag);
GLAPI void GLAPIENTRY glEdgeFlagPointer (GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glEdgeFlagv (const GLboolean *flag);
GLAPI void GLAPIENTRY glEnable (GLenum cap);
GLAPI void GLAPIENTRY glEnableClientState (GLenum array);
GLAPI void GLAPIENTRY glEnd (void);
GLAPI void GLAPIENTRY glEndList (void);
GLAPI void GLAPIENTRY glEvalCoord1d (GLdouble u);
GLAPI void GLAPIENTRY glEvalCoord1dv (const GLdouble *u);
GLAPI void GLAPIENTRY glEvalCoord1f (GLfloat u);
GLAPI void GLAPIENTRY glEvalCoord1fv (const GLfloat *u);
GLAPI void GLAPIENTRY glEvalCoord2d (GLdouble u, GLdouble v);
GLAPI void GLAPIENTRY glEvalCoord2dv (const GLdouble *u);
GLAPI void GLAPIENTRY glEvalCoord2f (GLfloat u, GLfloat v);
GLAPI void GLAPIENTRY glEvalCoord2fv (const GLfloat *u);
GLAPI void GLAPIENTRY glEvalMesh1 (GLenum mode, GLint i1, GLint i2);
GLAPI void GLAPIENTRY glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
GLAPI void GLAPIENTRY glEvalPoint1 (GLint i);
GLAPI void GLAPIENTRY glEvalPoint2 (GLint i, GLint j);
GLAPI void GLAPIENTRY glFeedbackBuffer (GLsizei size, GLenum type, GLfloat *buffer);
GLAPI void GLAPIENTRY glFinish (void);
GLAPI void GLAPIENTRY glFlush (void);
GLAPI void GLAPIENTRY glFogf (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glFogfv (GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glFogi (GLenum pname, GLint param);
GLAPI void GLAPIENTRY glFogiv (GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glFrontFace (GLenum mode);
GLAPI void GLAPIENTRY glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLAPI GLuint GLAPIENTRY glGenLists (GLsizei range);
GLAPI void GLAPIENTRY glGenTextures (GLsizei n, GLuint *textures);
GLAPI void GLAPIENTRY glGetBooleanv (GLenum pname, GLboolean *params);
GLAPI void GLAPIENTRY glGetClipPlane (GLenum plane, GLdouble *equation);
GLAPI void GLAPIENTRY glGetDoublev (GLenum pname, GLdouble *params);
GLAPI GLenum GLAPIENTRY glGetError (void);
GLAPI void GLAPIENTRY glGetFloatv (GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetIntegerv (GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetLightfv (GLenum light, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetLightiv (GLenum light, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetMapdv (GLenum target, GLenum query, GLdouble *v);
GLAPI void GLAPIENTRY glGetMapfv (GLenum target, GLenum query, GLfloat *v);
GLAPI void GLAPIENTRY glGetMapiv (GLenum target, GLenum query, GLint *v);
GLAPI void GLAPIENTRY glGetMaterialfv (GLenum face, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetMaterialiv (GLenum face, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetPixelMapfv (GLenum map, GLfloat *values);
GLAPI void GLAPIENTRY glGetPixelMapuiv (GLenum map, GLuint *values);
GLAPI void GLAPIENTRY glGetPixelMapusv (GLenum map, GLushort *values);
GLAPI void GLAPIENTRY glGetPointerv (GLenum pname, GLvoid* *params);
GLAPI void GLAPIENTRY glGetPolygonStipple (GLubyte *mask);
GLAPI const GLubyte * GLAPIENTRY glGetString (GLenum name);
GLAPI void GLAPIENTRY glGetTexEnvfv (GLenum target, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetTexEnviv (GLenum target, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetTexGendv (GLenum coord, GLenum pname, GLdouble *params);
GLAPI void GLAPIENTRY glGetTexGenfv (GLenum coord, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetTexGeniv (GLenum coord, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
GLAPI void GLAPIENTRY glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params);
GLAPI void GLAPIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint *params);
GLAPI void GLAPIENTRY glHint (GLenum target, GLenum mode);
GLAPI void GLAPIENTRY glIndexMask (GLuint mask);
GLAPI void GLAPIENTRY glIndexPointer (GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glIndexd (GLdouble c);
GLAPI void GLAPIENTRY glIndexdv (const GLdouble *c);
GLAPI void GLAPIENTRY glIndexf (GLfloat c);
GLAPI void GLAPIENTRY glIndexfv (const GLfloat *c);
GLAPI void GLAPIENTRY glIndexi (GLint c);
GLAPI void GLAPIENTRY glIndexiv (const GLint *c);
GLAPI void GLAPIENTRY glIndexs (GLshort c);
GLAPI void GLAPIENTRY glIndexsv (const GLshort *c);
GLAPI void GLAPIENTRY glIndexub (GLubyte c);
GLAPI void GLAPIENTRY glIndexubv (const GLubyte *c);
GLAPI void GLAPIENTRY glInitNames (void);
GLAPI void GLAPIENTRY glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid *pointer);
GLAPI GLboolean GLAPIENTRY glIsEnabled (GLenum cap);
GLAPI GLboolean GLAPIENTRY glIsList (GLuint list);
GLAPI GLboolean GLAPIENTRY glIsTexture (GLuint texture);
GLAPI void GLAPIENTRY glLightModelf (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glLightModelfv (GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glLightModeli (GLenum pname, GLint param);
GLAPI void GLAPIENTRY glLightModeliv (GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glLightf (GLenum light, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glLightfv (GLenum light, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glLighti (GLenum light, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glLightiv (GLenum light, GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glLineStipple (GLint factor, GLushort pattern);
GLAPI void GLAPIENTRY glLineWidth (GLfloat width);
GLAPI void GLAPIENTRY glListBase (GLuint base);
GLAPI void GLAPIENTRY glLoadIdentity (void);
GLAPI void GLAPIENTRY glLoadMatrixd (const GLdouble *m);
GLAPI void GLAPIENTRY glLoadMatrixf (const GLfloat *m);
GLAPI void GLAPIENTRY glLoadName (GLuint name);
GLAPI void GLAPIENTRY glLogicOp (GLenum opcode);
GLAPI void GLAPIENTRY glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
GLAPI void GLAPIENTRY glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
GLAPI void GLAPIENTRY glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
GLAPI void GLAPIENTRY glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
GLAPI void GLAPIENTRY glMapGrid1d (GLint un, GLdouble u1, GLdouble u2);
GLAPI void GLAPIENTRY glMapGrid1f (GLint un, GLfloat u1, GLfloat u2);
GLAPI void GLAPIENTRY glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
GLAPI void GLAPIENTRY glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
GLAPI void GLAPIENTRY glMaterialf (GLenum face, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glMaterialfv (GLenum face, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glMateriali (GLenum face, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glMaterialiv (GLenum face, GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glMatrixMode (GLenum mode);
GLAPI void GLAPIENTRY glMultMatrixd (const GLdouble *m);
GLAPI void GLAPIENTRY glMultMatrixf (const GLfloat *m);
GLAPI void GLAPIENTRY glNewList (GLuint list, GLenum mode);
GLAPI void GLAPIENTRY glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz);
GLAPI void GLAPIENTRY glNormal3bv (const GLbyte *v);
GLAPI void GLAPIENTRY glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz);
GLAPI void GLAPIENTRY glNormal3dv (const GLdouble *v);
GLAPI void GLAPIENTRY glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz);
GLAPI void GLAPIENTRY glNormal3fv (const GLfloat *v);
GLAPI void GLAPIENTRY glNormal3i (GLint nx, GLint ny, GLint nz);
GLAPI void GLAPIENTRY glNormal3iv (const GLint *v);
GLAPI void GLAPIENTRY glNormal3s (GLshort nx, GLshort ny, GLshort nz);
GLAPI void GLAPIENTRY glNormal3sv (const GLshort *v);
GLAPI void GLAPIENTRY glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLAPI void GLAPIENTRY glPassThrough (GLfloat token);
GLAPI void GLAPIENTRY glPixelMapfv (GLenum map, GLsizei mapsize, const GLfloat *values);
GLAPI void GLAPIENTRY glPixelMapuiv (GLenum map, GLsizei mapsize, const GLuint *values);
GLAPI void GLAPIENTRY glPixelMapusv (GLenum map, GLsizei mapsize, const GLushort *values);
GLAPI void GLAPIENTRY glPixelStoref (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glPixelStorei (GLenum pname, GLint param);
GLAPI void GLAPIENTRY glPixelTransferf (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glPixelTransferi (GLenum pname, GLint param);
GLAPI void GLAPIENTRY glPixelZoom (GLfloat xfactor, GLfloat yfactor);
GLAPI void GLAPIENTRY glPointSize (GLfloat size);
GLAPI void GLAPIENTRY glPolygonMode (GLenum face, GLenum mode);
GLAPI void GLAPIENTRY glPolygonOffset (GLfloat factor, GLfloat units);
GLAPI void GLAPIENTRY glPolygonStipple (const GLubyte *mask);
GLAPI void GLAPIENTRY glPopAttrib (void);
GLAPI void GLAPIENTRY glPopClientAttrib (void);
GLAPI void GLAPIENTRY glPopMatrix (void);
GLAPI void GLAPIENTRY glPopName (void);
GLAPI void GLAPIENTRY glPrioritizeTextures (GLsizei n, const GLuint *textures, const GLclampf *priorities);
GLAPI void GLAPIENTRY glPushAttrib (GLbitfield mask);
GLAPI void GLAPIENTRY glPushClientAttrib (GLbitfield mask);
GLAPI void GLAPIENTRY glPushMatrix (void);
GLAPI void GLAPIENTRY glPushName (GLuint name);
GLAPI void GLAPIENTRY glRasterPos2d (GLdouble x, GLdouble y);
GLAPI void GLAPIENTRY glRasterPos2dv (const GLdouble *v);
GLAPI void GLAPIENTRY glRasterPos2f (GLfloat x, GLfloat y);
GLAPI void GLAPIENTRY glRasterPos2fv (const GLfloat *v);
GLAPI void GLAPIENTRY glRasterPos2i (GLint x, GLint y);
GLAPI void GLAPIENTRY glRasterPos2iv (const GLint *v);
GLAPI void GLAPIENTRY glRasterPos2s (GLshort x, GLshort y);
GLAPI void GLAPIENTRY glRasterPos2sv (const GLshort *v);
GLAPI void GLAPIENTRY glRasterPos3d (GLdouble x, GLdouble y, GLdouble z);
GLAPI void GLAPIENTRY glRasterPos3dv (const GLdouble *v);
GLAPI void GLAPIENTRY glRasterPos3f (GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glRasterPos3fv (const GLfloat *v);
GLAPI void GLAPIENTRY glRasterPos3i (GLint x, GLint y, GLint z);
GLAPI void GLAPIENTRY glRasterPos3iv (const GLint *v);
GLAPI void GLAPIENTRY glRasterPos3s (GLshort x, GLshort y, GLshort z);
GLAPI void GLAPIENTRY glRasterPos3sv (const GLshort *v);
GLAPI void GLAPIENTRY glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
GLAPI void GLAPIENTRY glRasterPos4dv (const GLdouble *v);
GLAPI void GLAPIENTRY glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GLAPI void GLAPIENTRY glRasterPos4fv (const GLfloat *v);
GLAPI void GLAPIENTRY glRasterPos4i (GLint x, GLint y, GLint z, GLint w);
GLAPI void GLAPIENTRY glRasterPos4iv (const GLint *v);
GLAPI void GLAPIENTRY glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w);
GLAPI void GLAPIENTRY glRasterPos4sv (const GLshort *v);
GLAPI void GLAPIENTRY glReadBuffer (GLenum mode);
GLAPI void GLAPIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
GLAPI void GLAPIENTRY glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
GLAPI void GLAPIENTRY glRectdv (const GLdouble *v1, const GLdouble *v2);
GLAPI void GLAPIENTRY glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
GLAPI void GLAPIENTRY glRectfv (const GLfloat *v1, const GLfloat *v2);
GLAPI void GLAPIENTRY glRecti (GLint x1, GLint y1, GLint x2, GLint y2);
GLAPI void GLAPIENTRY glRectiv (const GLint *v1, const GLint *v2);
GLAPI void GLAPIENTRY glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2);
GLAPI void GLAPIENTRY glRectsv (const GLshort *v1, const GLshort *v2);
GLAPI GLint GLAPIENTRY glRenderMode (GLenum mode);
GLAPI void GLAPIENTRY glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
GLAPI void GLAPIENTRY glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glScaled (GLdouble x, GLdouble y, GLdouble z);
GLAPI void GLAPIENTRY glScalef (GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glSelectBuffer (GLsizei size, GLuint *buffer);
GLAPI void GLAPIENTRY glShadeModel (GLenum mode);
GLAPI void GLAPIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask);
GLAPI void GLAPIENTRY glStencilMask (GLuint mask);
GLAPI void GLAPIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass);
GLAPI void GLAPIENTRY glTexCoord1d (GLdouble s);
GLAPI void GLAPIENTRY glTexCoord1dv (const GLdouble *v);
GLAPI void GLAPIENTRY glTexCoord1f (GLfloat s);
GLAPI void GLAPIENTRY glTexCoord1fv (const GLfloat *v);
GLAPI void GLAPIENTRY glTexCoord1i (GLint s);
GLAPI void GLAPIENTRY glTexCoord1iv (const GLint *v);
GLAPI void GLAPIENTRY glTexCoord1s (GLshort s);
GLAPI void GLAPIENTRY glTexCoord1sv (const GLshort *v);
GLAPI void GLAPIENTRY glTexCoord2d (GLdouble s, GLdouble t);
GLAPI void GLAPIENTRY glTexCoord2dv (const GLdouble *v);
GLAPI void GLAPIENTRY glTexCoord2f (GLfloat s, GLfloat t);
GLAPI void GLAPIENTRY glTexCoord2fv (const GLfloat *v);
GLAPI void GLAPIENTRY glTexCoord2i (GLint s, GLint t);
GLAPI void GLAPIENTRY glTexCoord2iv (const GLint *v);
GLAPI void GLAPIENTRY glTexCoord2s (GLshort s, GLshort t);
GLAPI void GLAPIENTRY glTexCoord2sv (const GLshort *v);
GLAPI void GLAPIENTRY glTexCoord3d (GLdouble s, GLdouble t, GLdouble r);
GLAPI void GLAPIENTRY glTexCoord3dv (const GLdouble *v);
GLAPI void GLAPIENTRY glTexCoord3f (GLfloat s, GLfloat t, GLfloat r);
GLAPI void GLAPIENTRY glTexCoord3fv (const GLfloat *v);
GLAPI void GLAPIENTRY glTexCoord3i (GLint s, GLint t, GLint r);
GLAPI void GLAPIENTRY glTexCoord3iv (const GLint *v);
GLAPI void GLAPIENTRY glTexCoord3s (GLshort s, GLshort t, GLshort r);
GLAPI void GLAPIENTRY glTexCoord3sv (const GLshort *v);
GLAPI void GLAPIENTRY glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q);
GLAPI void GLAPIENTRY glTexCoord4dv (const GLdouble *v);
GLAPI void GLAPIENTRY glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
GLAPI void GLAPIENTRY glTexCoord4fv (const GLfloat *v);
GLAPI void GLAPIENTRY glTexCoord4i (GLint s, GLint t, GLint r, GLint q);
GLAPI void GLAPIENTRY glTexCoord4iv (const GLint *v);
GLAPI void GLAPIENTRY glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q);
GLAPI void GLAPIENTRY glTexCoord4sv (const GLshort *v);
GLAPI void GLAPIENTRY glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glTexEnvf (GLenum target, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glTexEnvi (GLenum target, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glTexEnviv (GLenum target, GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glTexGend (GLenum coord, GLenum pname, GLdouble param);
GLAPI void GLAPIENTRY glTexGendv (GLenum coord, GLenum pname, const GLdouble *params);
GLAPI void GLAPIENTRY glTexGenf (GLenum coord, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glTexGenfv (GLenum coord, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glTexGeni (GLenum coord, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glTexGeniv (GLenum coord, GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glTexParameterf (GLenum target, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glTexParameteri (GLenum target, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glTexParameteriv (GLenum target, GLenum pname, const GLint *params);
GLAPI void GLAPIENTRY glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glTranslated (GLdouble x, GLdouble y, GLdouble z);
GLAPI void GLAPIENTRY glTranslatef (GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glVertex2d (GLdouble x, GLdouble y);
GLAPI void GLAPIENTRY glVertex2dv (const GLdouble *v);
GLAPI void GLAPIENTRY glVertex2f (GLfloat x, GLfloat y);
GLAPI void GLAPIENTRY glVertex2fv (const GLfloat *v);
GLAPI void GLAPIENTRY glVertex2i (GLint x, GLint y);
GLAPI void GLAPIENTRY glVertex2iv (const GLint *v);
GLAPI void GLAPIENTRY glVertex2s (GLshort x, GLshort y);
GLAPI void GLAPIENTRY glVertex2sv (const GLshort *v);
GLAPI void GLAPIENTRY glVertex3d (GLdouble x, GLdouble y, GLdouble z);
GLAPI void GLAPIENTRY glVertex3dv (const GLdouble *v);
GLAPI void GLAPIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glVertex3fv (const GLfloat *v);
GLAPI void GLAPIENTRY glVertex3i (GLint x, GLint y, GLint z);
GLAPI void GLAPIENTRY glVertex3iv (const GLint *v);
GLAPI void GLAPIENTRY glVertex3s (GLshort x, GLshort y, GLshort z);
GLAPI void GLAPIENTRY glVertex3sv (const GLshort *v);
GLAPI void GLAPIENTRY glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
GLAPI void GLAPIENTRY glVertex4dv (const GLdouble *v);
GLAPI void GLAPIENTRY glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GLAPI void GLAPIENTRY glVertex4fv (const GLfloat *v);
GLAPI void GLAPIENTRY glVertex4i (GLint x, GLint y, GLint z, GLint w);
GLAPI void GLAPIENTRY glVertex4iv (const GLint *v);
GLAPI void GLAPIENTRY glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w);
GLAPI void GLAPIENTRY glVertex4sv (const GLshort *v);
GLAPI void GLAPIENTRY glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height);

#define GLEW_VERSION_1_1 GLEW_GET_VAR(__GLEW_VERSION_1_1)

#endif /* GL_VERSION_1_1 */

/* ---------------------------------- GLU ---------------------------------- */

// -- GODOT start --
// We don't use GLU, so remove a dependency.
#if 0
/* this is where we can safely include GLU */
#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
#endif
// -- GODOT end --

/* ----------------------------- GL_VERSION_1_2 ---------------------------- */

#ifndef GL_VERSION_1_2
#define GL_VERSION_1_2 1

#define GL_SMOOTH_POINT_SIZE_RANGE 0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY 0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE 0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_UNSIGNED_BYTE_3_3_2 0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#define GL_UNSIGNED_INT_10_10_10_2 0x8036
#define GL_RESCALE_NORMAL 0x803A
#define GL_TEXTURE_BINDING_3D 0x806A
#define GL_PACK_SKIP_IMAGES 0x806B
#define GL_PACK_IMAGE_HEIGHT 0x806C
#define GL_UNPACK_SKIP_IMAGES 0x806D
#define GL_UNPACK_IMAGE_HEIGHT 0x806E
#define GL_TEXTURE_3D 0x806F
#define GL_PROXY_TEXTURE_3D 0x8070
#define GL_TEXTURE_DEPTH 0x8071
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_MAX_3D_TEXTURE_SIZE 0x8073
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_MAX_ELEMENTS_VERTICES 0x80E8
#define GL_MAX_ELEMENTS_INDICES 0x80E9
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
#define GL_SINGLE_COLOR 0x81F9
#define GL_SEPARATE_SPECULAR_COLOR 0x81FA
#define GL_UNSIGNED_BYTE_2_3_3_REV 0x8362
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_ALIASED_POINT_SIZE_RANGE 0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE 0x846E

typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (GLAPIENTRY * PFNGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

#define glCopyTexSubImage3D GLEW_GET_FUN(__glewCopyTexSubImage3D)
#define glDrawRangeElements GLEW_GET_FUN(__glewDrawRangeElements)
#define glTexImage3D GLEW_GET_FUN(__glewTexImage3D)
#define glTexSubImage3D GLEW_GET_FUN(__glewTexSubImage3D)

#define GLEW_VERSION_1_2 GLEW_GET_VAR(__GLEW_VERSION_1_2)

#endif /* GL_VERSION_1_2 */

/* ----------------------------- GL_VERSION_1_3 ---------------------------- */

#ifndef GL_VERSION_1_3
#define GL_VERSION_1_3 1

#define GL_MULTISAMPLE 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#define GL_SAMPLE_COVERAGE 0x80A0
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT 0x80AB
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE
#define GL_TEXTURE15 0x84CF
#define GL_TEXTURE16 0x84D0
#define GL_TEXTURE17 0x84D1
#define GL_TEXTURE18 0x84D2
#define GL_TEXTURE19 0x84D3
#define GL_TEXTURE20 0x84D4
#define GL_TEXTURE21 0x84D5
#define GL_TEXTURE22 0x84D6
#define GL_TEXTURE23 0x84D7
#define GL_TEXTURE24 0x84D8
#define GL_TEXTURE25 0x84D9
#define GL_TEXTURE26 0x84DA
#define GL_TEXTURE27 0x84DB
#define GL_TEXTURE28 0x84DC
#define GL_TEXTURE29 0x84DD
#define GL_TEXTURE30 0x84DE
#define GL_TEXTURE31 0x84DF
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE 0x84E1
#define GL_MAX_TEXTURE_UNITS 0x84E2
#define GL_TRANSPOSE_MODELVIEW_MATRIX 0x84E3
#define GL_TRANSPOSE_PROJECTION_MATRIX 0x84E4
#define GL_TRANSPOSE_TEXTURE_MATRIX 0x84E5
#define GL_TRANSPOSE_COLOR_MATRIX 0x84E6
#define GL_SUBTRACT 0x84E7
#define GL_COMPRESSED_ALPHA 0x84E9
#define GL_COMPRESSED_LUMINANCE 0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA 0x84EB
#define GL_COMPRESSED_INTENSITY 0x84EC
#define GL_COMPRESSED_RGB 0x84ED
#define GL_COMPRESSED_RGBA 0x84EE
#define GL_TEXTURE_COMPRESSION_HINT 0x84EF
#define GL_NORMAL_MAP 0x8511
#define GL_REFLECTION_MAP 0x8512
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP 0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_SOURCE2_ALPHA 0x858A
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859A
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE 0x86A0
#define GL_TEXTURE_COMPRESSED 0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS 0x86A3
#define GL_DOT3_RGB 0x86AE
#define GL_DOT3_RGBA 0x86AF
#define GL_MULTISAMPLE_BIT 0x20000000

typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLCLIENTACTIVETEXTUREPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLGETCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint lod, GLvoid *img);
typedef void (GLAPIENTRY * PFNGLLOADTRANSPOSEMATRIXDPROC) (const GLdouble m[16]);
typedef void (GLAPIENTRY * PFNGLLOADTRANSPOSEMATRIXFPROC) (const GLfloat m[16]);
typedef void (GLAPIENTRY * PFNGLMULTTRANSPOSEMATRIXDPROC) (const GLdouble m[16]);
typedef void (GLAPIENTRY * PFNGLMULTTRANSPOSEMATRIXFPROC) (const GLfloat m[16]);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1DPROC) (GLenum target, GLdouble s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1DVPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1FPROC) (GLenum target, GLfloat s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1FVPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1IPROC) (GLenum target, GLint s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1IVPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1SPROC) (GLenum target, GLshort s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1SVPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2DPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2DVPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2FPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2FVPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2IPROC) (GLenum target, GLint s, GLint t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2IVPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2SPROC) (GLenum target, GLshort s, GLshort t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2SVPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3DVPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3FVPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3IPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3IVPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3SPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3SVPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4DVPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4FVPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4IPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4IVPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4SPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4SVPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEPROC) (GLclampf value, GLboolean invert);

#define glActiveTexture GLEW_GET_FUN(__glewActiveTexture)
#define glClientActiveTexture GLEW_GET_FUN(__glewClientActiveTexture)
#define glCompressedTexImage1D GLEW_GET_FUN(__glewCompressedTexImage1D)
#define glCompressedTexImage2D GLEW_GET_FUN(__glewCompressedTexImage2D)
#define glCompressedTexImage3D GLEW_GET_FUN(__glewCompressedTexImage3D)
#define glCompressedTexSubImage1D GLEW_GET_FUN(__glewCompressedTexSubImage1D)
#define glCompressedTexSubImage2D GLEW_GET_FUN(__glewCompressedTexSubImage2D)
#define glCompressedTexSubImage3D GLEW_GET_FUN(__glewCompressedTexSubImage3D)
#define glGetCompressedTexImage GLEW_GET_FUN(__glewGetCompressedTexImage)
#define glLoadTransposeMatrixd GLEW_GET_FUN(__glewLoadTransposeMatrixd)
#define glLoadTransposeMatrixf GLEW_GET_FUN(__glewLoadTransposeMatrixf)
#define glMultTransposeMatrixd GLEW_GET_FUN(__glewMultTransposeMatrixd)
#define glMultTransposeMatrixf GLEW_GET_FUN(__glewMultTransposeMatrixf)
#define glMultiTexCoord1d GLEW_GET_FUN(__glewMultiTexCoord1d)
#define glMultiTexCoord1dv GLEW_GET_FUN(__glewMultiTexCoord1dv)
#define glMultiTexCoord1f GLEW_GET_FUN(__glewMultiTexCoord1f)
#define glMultiTexCoord1fv GLEW_GET_FUN(__glewMultiTexCoord1fv)
#define glMultiTexCoord1i GLEW_GET_FUN(__glewMultiTexCoord1i)
#define glMultiTexCoord1iv GLEW_GET_FUN(__glewMultiTexCoord1iv)
#define glMultiTexCoord1s GLEW_GET_FUN(__glewMultiTexCoord1s)
#define glMultiTexCoord1sv GLEW_GET_FUN(__glewMultiTexCoord1sv)
#define glMultiTexCoord2d GLEW_GET_FUN(__glewMultiTexCoord2d)
#define glMultiTexCoord2dv GLEW_GET_FUN(__glewMultiTexCoord2dv)
#define glMultiTexCoord2f GLEW_GET_FUN(__glewMultiTexCoord2f)
#define glMultiTexCoord2fv GLEW_GET_FUN(__glewMultiTexCoord2fv)
#define glMultiTexCoord2i GLEW_GET_FUN(__glewMultiTexCoord2i)
#define glMultiTexCoord2iv GLEW_GET_FUN(__glewMultiTexCoord2iv)
#define glMultiTexCoord2s GLEW_GET_FUN(__glewMultiTexCoord2s)
#define glMultiTexCoord2sv GLEW_GET_FUN(__glewMultiTexCoord2sv)
#define glMultiTexCoord3d GLEW_GET_FUN(__glewMultiTexCoord3d)
#define glMultiTexCoord3dv GLEW_GET_FUN(__glewMultiTexCoord3dv)
#define glMultiTexCoord3f GLEW_GET_FUN(__glewMultiTexCoord3f)
#define glMultiTexCoord3fv GLEW_GET_FUN(__glewMultiTexCoord3fv)
#define glMultiTexCoord3i GLEW_GET_FUN(__glewMultiTexCoord3i)
#define glMultiTexCoord3iv GLEW_GET_FUN(__glewMultiTexCoord3iv)
#define glMultiTexCoord3s GLEW_GET_FUN(__glewMultiTexCoord3s)
#define glMultiTexCoord3sv GLEW_GET_FUN(__glewMultiTexCoord3sv)
#define glMultiTexCoord4d GLEW_GET_FUN(__glewMultiTexCoord4d)
#define glMultiTexCoord4dv GLEW_GET_FUN(__glewMultiTexCoord4dv)
#define glMultiTexCoord4f GLEW_GET_FUN(__glewMultiTexCoord4f)
#define glMultiTexCoord4fv GLEW_GET_FUN(__glewMultiTexCoord4fv)
#define glMultiTexCoord4i GLEW_GET_FUN(__glewMultiTexCoord4i)
#define glMultiTexCoord4iv GLEW_GET_FUN(__glewMultiTexCoord4iv)
#define glMultiTexCoord4s GLEW_GET_FUN(__glewMultiTexCoord4s)
#define glMultiTexCoord4sv GLEW_GET_FUN(__glewMultiTexCoord4sv)
#define glSampleCoverage GLEW_GET_FUN(__glewSampleCoverage)

#define GLEW_VERSION_1_3 GLEW_GET_VAR(__GLEW_VERSION_1_3)

#endif /* GL_VERSION_1_3 */

/* ----------------------------- GL_VERSION_1_4 ---------------------------- */

#ifndef GL_VERSION_1_4
#define GL_VERSION_1_4 1

#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_POINT_SIZE_MIN 0x8126
#define GL_POINT_SIZE_MAX 0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE 0x8128
#define GL_POINT_DISTANCE_ATTENUATION 0x8129
#define GL_GENERATE_MIPMAP 0x8191
#define GL_GENERATE_MIPMAP_HINT 0x8192
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_MIRRORED_REPEAT 0x8370
#define GL_FOG_COORDINATE_SOURCE 0x8450
#define GL_FOG_COORDINATE 0x8451
#define GL_FRAGMENT_DEPTH 0x8452
#define GL_CURRENT_FOG_COORDINATE 0x8453
#define GL_FOG_COORDINATE_ARRAY_TYPE 0x8454
#define GL_FOG_COORDINATE_ARRAY_STRIDE 0x8455
#define GL_FOG_COORDINATE_ARRAY_POINTER 0x8456
#define GL_FOG_COORDINATE_ARRAY 0x8457
#define GL_COLOR_SUM 0x8458
#define GL_CURRENT_SECONDARY_COLOR 0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE 0x845A
#define GL_SECONDARY_COLOR_ARRAY_TYPE 0x845B
#define GL_SECONDARY_COLOR_ARRAY_STRIDE 0x845C
#define GL_SECONDARY_COLOR_ARRAY_POINTER 0x845D
#define GL_SECONDARY_COLOR_ARRAY 0x845E
#define GL_MAX_TEXTURE_LOD_BIAS 0x84FD
#define GL_TEXTURE_FILTER_CONTROL 0x8500
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_TEXTURE_DEPTH_SIZE 0x884A
#define GL_DEPTH_TEXTURE_MODE 0x884B
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_R_TO_TEXTURE 0x884E

typedef void (GLAPIENTRY * PFNGLBLENDCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONPROC) (GLenum mode);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (GLAPIENTRY * PFNGLFOGCOORDPOINTERPROC) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (GLAPIENTRY * PFNGLFOGCOORDDPROC) (GLdouble coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDDVPROC) (const GLdouble *coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDFPROC) (GLfloat coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDFVPROC) (const GLfloat *coord);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWARRAYSPROC) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTSPROC) (GLenum mode, GLsizei *count, GLenum type, const GLvoid **indices, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFVPROC) (GLenum pname, const GLfloat *params);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERIPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERIVPROC) (GLenum pname, const GLint *params);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3BPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3BVPROC) (const GLbyte *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3DPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3DVPROC) (const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3FPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3FVPROC) (const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3IPROC) (GLint red, GLint green, GLint blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3IVPROC) (const GLint *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3SPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3SVPROC) (const GLshort *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UBPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UBVPROC) (const GLubyte *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UIPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UIVPROC) (const GLuint *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3USPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3USVPROC) (const GLushort *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORPOINTERPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DPROC) (GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DVPROC) (const GLdouble *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FPROC) (GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FVPROC) (const GLfloat *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IPROC) (GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IVPROC) (const GLint *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SPROC) (GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SVPROC) (const GLshort *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DVPROC) (const GLdouble *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FVPROC) (const GLfloat *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IPROC) (GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IVPROC) (const GLint *p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SVPROC) (const GLshort *p);

#define glBlendColor GLEW_GET_FUN(__glewBlendColor)
#define glBlendEquation GLEW_GET_FUN(__glewBlendEquation)
#define glBlendFuncSeparate GLEW_GET_FUN(__glewBlendFuncSeparate)
#define glFogCoordPointer GLEW_GET_FUN(__glewFogCoordPointer)
#define glFogCoordd GLEW_GET_FUN(__glewFogCoordd)
#define glFogCoorddv GLEW_GET_FUN(__glewFogCoorddv)
#define glFogCoordf GLEW_GET_FUN(__glewFogCoordf)
#define glFogCoordfv GLEW_GET_FUN(__glewFogCoordfv)
#define glMultiDrawArrays GLEW_GET_FUN(__glewMultiDrawArrays)
#define glMultiDrawElements GLEW_GET_FUN(__glewMultiDrawElements)
#define glPointParameterf GLEW_GET_FUN(__glewPointParameterf)
#define glPointParameterfv GLEW_GET_FUN(__glewPointParameterfv)
#define glPointParameteri GLEW_GET_FUN(__glewPointParameteri)
#define glPointParameteriv GLEW_GET_FUN(__glewPointParameteriv)
#define glSecondaryColor3b GLEW_GET_FUN(__glewSecondaryColor3b)
#define glSecondaryColor3bv GLEW_GET_FUN(__glewSecondaryColor3bv)
#define glSecondaryColor3d GLEW_GET_FUN(__glewSecondaryColor3d)
#define glSecondaryColor3dv GLEW_GET_FUN(__glewSecondaryColor3dv)
#define glSecondaryColor3f GLEW_GET_FUN(__glewSecondaryColor3f)
#define glSecondaryColor3fv GLEW_GET_FUN(__glewSecondaryColor3fv)
#define glSecondaryColor3i GLEW_GET_FUN(__glewSecondaryColor3i)
#define glSecondaryColor3iv GLEW_GET_FUN(__glewSecondaryColor3iv)
#define glSecondaryColor3s GLEW_GET_FUN(__glewSecondaryColor3s)
#define glSecondaryColor3sv GLEW_GET_FUN(__glewSecondaryColor3sv)
#define glSecondaryColor3ub GLEW_GET_FUN(__glewSecondaryColor3ub)
#define glSecondaryColor3ubv GLEW_GET_FUN(__glewSecondaryColor3ubv)
#define glSecondaryColor3ui GLEW_GET_FUN(__glewSecondaryColor3ui)
#define glSecondaryColor3uiv GLEW_GET_FUN(__glewSecondaryColor3uiv)
#define glSecondaryColor3us GLEW_GET_FUN(__glewSecondaryColor3us)
#define glSecondaryColor3usv GLEW_GET_FUN(__glewSecondaryColor3usv)
#define glSecondaryColorPointer GLEW_GET_FUN(__glewSecondaryColorPointer)
#define glWindowPos2d GLEW_GET_FUN(__glewWindowPos2d)
#define glWindowPos2dv GLEW_GET_FUN(__glewWindowPos2dv)
#define glWindowPos2f GLEW_GET_FUN(__glewWindowPos2f)
#define glWindowPos2fv GLEW_GET_FUN(__glewWindowPos2fv)
#define glWindowPos2i GLEW_GET_FUN(__glewWindowPos2i)
#define glWindowPos2iv GLEW_GET_FUN(__glewWindowPos2iv)
#define glWindowPos2s GLEW_GET_FUN(__glewWindowPos2s)
#define glWindowPos2sv GLEW_GET_FUN(__glewWindowPos2sv)
#define glWindowPos3d GLEW_GET_FUN(__glewWindowPos3d)
#define glWindowPos3dv GLEW_GET_FUN(__glewWindowPos3dv)
#define glWindowPos3f GLEW_GET_FUN(__glewWindowPos3f)
#define glWindowPos3fv GLEW_GET_FUN(__glewWindowPos3fv)
#define glWindowPos3i GLEW_GET_FUN(__glewWindowPos3i)
#define glWindowPos3iv GLEW_GET_FUN(__glewWindowPos3iv)
#define glWindowPos3s GLEW_GET_FUN(__glewWindowPos3s)
#define glWindowPos3sv GLEW_GET_FUN(__glewWindowPos3sv)

#define GLEW_VERSION_1_4 GLEW_GET_VAR(__GLEW_VERSION_1_4)

#endif /* GL_VERSION_1_4 */

/* ----------------------------- GL_VERSION_1_5 ---------------------------- */

#ifndef GL_VERSION_1_5
#define GL_VERSION_1_5 1

#define GL_FOG_COORD_SRC GL_FOG_COORDINATE_SOURCE
#define GL_FOG_COORD GL_FOG_COORDINATE
#define GL_FOG_COORD_ARRAY GL_FOG_COORDINATE_ARRAY
#define GL_SRC0_RGB GL_SOURCE0_RGB
#define GL_FOG_COORD_ARRAY_POINTER GL_FOG_COORDINATE_ARRAY_POINTER
#define GL_FOG_COORD_ARRAY_TYPE GL_FOG_COORDINATE_ARRAY_TYPE
#define GL_SRC1_ALPHA GL_SOURCE1_ALPHA
#define GL_CURRENT_FOG_COORD GL_CURRENT_FOG_COORDINATE
#define GL_FOG_COORD_ARRAY_STRIDE GL_FOG_COORDINATE_ARRAY_STRIDE
#define GL_SRC0_ALPHA GL_SOURCE0_ALPHA
#define GL_SRC1_RGB GL_SOURCE1_RGB
#define GL_FOG_COORD_ARRAY_BUFFER_BINDING GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#define GL_SRC2_ALPHA GL_SOURCE2_ALPHA
#define GL_SRC2_RGB GL_SOURCE2_RGB
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_QUERY_COUNTER_BITS 0x8864
#define GL_CURRENT_QUERY 0x8865
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_SAMPLES_PASSED 0x8914

typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

typedef void (GLAPIENTRY * PFNGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
typedef void (GLAPIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
typedef void (GLAPIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* buffers);
typedef void (GLAPIENTRY * PFNGLDELETEQUERIESPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLENDQUERYPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n, GLuint* buffers);
typedef void (GLAPIENTRY * PFNGLGENQUERIESPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETBUFFERPOINTERVPROC) (GLenum target, GLenum pname, GLvoid** params);
typedef void (GLAPIENTRY * PFNGLGETBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, GLvoid* data);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTIVPROC) (GLuint id, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISBUFFERPROC) (GLuint buffer);
typedef GLboolean (GLAPIENTRY * PFNGLISQUERYPROC) (GLuint id);
typedef GLvoid* (GLAPIENTRY * PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean (GLAPIENTRY * PFNGLUNMAPBUFFERPROC) (GLenum target);

#define glBeginQuery GLEW_GET_FUN(__glewBeginQuery)
#define glBindBuffer GLEW_GET_FUN(__glewBindBuffer)
#define glBufferData GLEW_GET_FUN(__glewBufferData)
#define glBufferSubData GLEW_GET_FUN(__glewBufferSubData)
#define glDeleteBuffers GLEW_GET_FUN(__glewDeleteBuffers)
#define glDeleteQueries GLEW_GET_FUN(__glewDeleteQueries)
#define glEndQuery GLEW_GET_FUN(__glewEndQuery)
#define glGenBuffers GLEW_GET_FUN(__glewGenBuffers)
#define glGenQueries GLEW_GET_FUN(__glewGenQueries)
#define glGetBufferParameteriv GLEW_GET_FUN(__glewGetBufferParameteriv)
#define glGetBufferPointerv GLEW_GET_FUN(__glewGetBufferPointerv)
#define glGetBufferSubData GLEW_GET_FUN(__glewGetBufferSubData)
#define glGetQueryObjectiv GLEW_GET_FUN(__glewGetQueryObjectiv)
#define glGetQueryObjectuiv GLEW_GET_FUN(__glewGetQueryObjectuiv)
#define glGetQueryiv GLEW_GET_FUN(__glewGetQueryiv)
#define glIsBuffer GLEW_GET_FUN(__glewIsBuffer)
#define glIsQuery GLEW_GET_FUN(__glewIsQuery)
#define glMapBuffer GLEW_GET_FUN(__glewMapBuffer)
#define glUnmapBuffer GLEW_GET_FUN(__glewUnmapBuffer)

#define GLEW_VERSION_1_5 GLEW_GET_VAR(__GLEW_VERSION_1_5)

#endif /* GL_VERSION_1_5 */

/* ----------------------------- GL_VERSION_2_0 ---------------------------- */

#ifndef GL_VERSION_2_0
#define GL_VERSION_2_0 1

#define GL_BLEND_EQUATION_RGB GL_BLEND_EQUATION
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE 0x8643
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_STENCIL_BACK_FUNC 0x8800
#define GL_STENCIL_BACK_FAIL 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS 0x8803
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_DRAW_BUFFER0 0x8825
#define GL_DRAW_BUFFER1 0x8826
#define GL_DRAW_BUFFER2 0x8827
#define GL_DRAW_BUFFER3 0x8828
#define GL_DRAW_BUFFER4 0x8829
#define GL_DRAW_BUFFER5 0x882A
#define GL_DRAW_BUFFER6 0x882B
#define GL_DRAW_BUFFER7 0x882C
#define GL_DRAW_BUFFER8 0x882D
#define GL_DRAW_BUFFER9 0x882E
#define GL_DRAW_BUFFER10 0x882F
#define GL_DRAW_BUFFER11 0x8830
#define GL_DRAW_BUFFER12 0x8831
#define GL_DRAW_BUFFER13 0x8832
#define GL_DRAW_BUFFER14 0x8833
#define GL_DRAW_BUFFER15 0x8834
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_POINT_SPRITE 0x8861
#define GL_COORD_REPLACE 0x8862
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_MAX_TEXTURE_COORDS 0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_MAX_VARYING_FLOATS 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_SHADER_TYPE 0x8B4F
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_1D 0x8B5D
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_3D 0x8B5F
#define GL_SAMPLER_CUBE 0x8B60
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_2D_SHADOW 0x8B62
#define GL_DELETE_STATUS 0x8B80
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#define GL_LOWER_LEFT 0x8CA1
#define GL_UPPER_LEFT 0x8CA2
#define GL_STENCIL_BACK_REF 0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK 0x8CA4
#define GL_STENCIL_BACK_WRITEMASK 0x8CA5

typedef void (GLAPIENTRY * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar* name);
typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum, GLenum);
typedef void (GLAPIENTRY * PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (GLAPIENTRY * PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROC) (GLenum type);
typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void (GLAPIENTRY * PFNGLDELETESHADERPROC) (GLuint shader);
typedef void (GLAPIENTRY * PFNGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint);
typedef void (GLAPIENTRY * PFNGLDRAWBUFFERSPROC) (GLsizei n, const GLenum* bufs);
typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint);
typedef void (GLAPIENTRY * PFNGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void (GLAPIENTRY * PFNGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
typedef GLint (GLAPIENTRY * PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar* name);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint* param);
typedef void (GLAPIENTRY * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (GLAPIENTRY * PFNGLGETSHADERSOURCEPROC) (GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source);
typedef void (GLAPIENTRY * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint* param);
typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar* name);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBPOINTERVPROC) (GLuint, GLenum, GLvoid*);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBDVPROC) (GLuint, GLenum, GLdouble*);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVPROC) (GLuint, GLenum, GLfloat*);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVPROC) (GLuint, GLenum, GLint*);
typedef GLboolean (GLAPIENTRY * PFNGLISPROGRAMPROC) (GLuint program);
typedef GLboolean (GLAPIENTRY * PFNGLISSHADERPROC) (GLuint shader);
typedef void (GLAPIENTRY * PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (GLAPIENTRY * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths);
typedef void (GLAPIENTRY * PFNGLSTENCILFUNCSEPARATEPROC) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
typedef void (GLAPIENTRY * PFNGLSTENCILMASKSEPARATEPROC) (GLenum, GLuint);
typedef void (GLAPIENTRY * PFNGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (GLAPIENTRY * PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (GLAPIENTRY * PFNGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (GLAPIENTRY * PFNGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (GLAPIENTRY * PFNGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2IPROC) (GLint location, GLint v0, GLint v1);
typedef void (GLAPIENTRY * PFNGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GLAPIENTRY * PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3IPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (GLAPIENTRY * PFNGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GLAPIENTRY * PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4IPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (GLAPIENTRY * PFNGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMPROC) (GLuint program);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DPROC) (GLuint index, GLdouble x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FPROC) (GLuint index, GLfloat x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SPROC) (GLuint index, GLshort x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SPROC) (GLuint index, GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NBVPROC) (GLuint index, const GLbyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NIVPROC) (GLuint index, const GLint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NSVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUBVPROC) (GLuint index, const GLubyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUIVPROC) (GLuint index, const GLuint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUSVPROC) (GLuint index, const GLushort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4BVPROC) (GLuint index, const GLbyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4IVPROC) (GLuint index, const GLint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UBVPROC) (GLuint index, const GLubyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UIVPROC) (GLuint index, const GLuint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4USVPROC) (GLuint index, const GLushort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);

#define glAttachShader GLEW_GET_FUN(__glewAttachShader)
#define glBindAttribLocation GLEW_GET_FUN(__glewBindAttribLocation)
#define glBlendEquationSeparate GLEW_GET_FUN(__glewBlendEquationSeparate)
#define glCompileShader GLEW_GET_FUN(__glewCompileShader)
#define glCreateProgram GLEW_GET_FUN(__glewCreateProgram)
#define glCreateShader GLEW_GET_FUN(__glewCreateShader)
#define glDeleteProgram GLEW_GET_FUN(__glewDeleteProgram)
#define glDeleteShader GLEW_GET_FUN(__glewDeleteShader)
#define glDetachShader GLEW_GET_FUN(__glewDetachShader)
#define glDisableVertexAttribArray GLEW_GET_FUN(__glewDisableVertexAttribArray)
#define glDrawBuffers GLEW_GET_FUN(__glewDrawBuffers)
#define glEnableVertexAttribArray GLEW_GET_FUN(__glewEnableVertexAttribArray)
#define glGetActiveAttrib GLEW_GET_FUN(__glewGetActiveAttrib)
#define glGetActiveUniform GLEW_GET_FUN(__glewGetActiveUniform)
#define glGetAttachedShaders GLEW_GET_FUN(__glewGetAttachedShaders)
#define glGetAttribLocation GLEW_GET_FUN(__glewGetAttribLocation)
#define glGetProgramInfoLog GLEW_GET_FUN(__glewGetProgramInfoLog)
#define glGetProgramiv GLEW_GET_FUN(__glewGetProgramiv)
#define glGetShaderInfoLog GLEW_GET_FUN(__glewGetShaderInfoLog)
#define glGetShaderSource GLEW_GET_FUN(__glewGetShaderSource)
#define glGetShaderiv GLEW_GET_FUN(__glewGetShaderiv)
#define glGetUniformLocation GLEW_GET_FUN(__glewGetUniformLocation)
#define glGetUniformfv GLEW_GET_FUN(__glewGetUniformfv)
#define glGetUniformiv GLEW_GET_FUN(__glewGetUniformiv)
#define glGetVertexAttribPointerv GLEW_GET_FUN(__glewGetVertexAttribPointerv)
#define glGetVertexAttribdv GLEW_GET_FUN(__glewGetVertexAttribdv)
#define glGetVertexAttribfv GLEW_GET_FUN(__glewGetVertexAttribfv)
#define glGetVertexAttribiv GLEW_GET_FUN(__glewGetVertexAttribiv)
#define glIsProgram GLEW_GET_FUN(__glewIsProgram)
#define glIsShader GLEW_GET_FUN(__glewIsShader)
#define glLinkProgram GLEW_GET_FUN(__glewLinkProgram)
#define glShaderSource GLEW_GET_FUN(__glewShaderSource)
#define glStencilFuncSeparate GLEW_GET_FUN(__glewStencilFuncSeparate)
#define glStencilMaskSeparate GLEW_GET_FUN(__glewStencilMaskSeparate)
#define glStencilOpSeparate GLEW_GET_FUN(__glewStencilOpSeparate)
#define glUniform1f GLEW_GET_FUN(__glewUniform1f)
#define glUniform1fv GLEW_GET_FUN(__glewUniform1fv)
#define glUniform1i GLEW_GET_FUN(__glewUniform1i)
#define glUniform1iv GLEW_GET_FUN(__glewUniform1iv)
#define glUniform2f GLEW_GET_FUN(__glewUniform2f)
#define glUniform2fv GLEW_GET_FUN(__glewUniform2fv)
#define glUniform2i GLEW_GET_FUN(__glewUniform2i)
#define glUniform2iv GLEW_GET_FUN(__glewUniform2iv)
#define glUniform3f GLEW_GET_FUN(__glewUniform3f)
#define glUniform3fv GLEW_GET_FUN(__glewUniform3fv)
#define glUniform3i GLEW_GET_FUN(__glewUniform3i)
#define glUniform3iv GLEW_GET_FUN(__glewUniform3iv)
#define glUniform4f GLEW_GET_FUN(__glewUniform4f)
#define glUniform4fv GLEW_GET_FUN(__glewUniform4fv)
#define glUniform4i GLEW_GET_FUN(__glewUniform4i)
#define glUniform4iv GLEW_GET_FUN(__glewUniform4iv)
#define glUniformMatrix2fv GLEW_GET_FUN(__glewUniformMatrix2fv)
#define glUniformMatrix3fv GLEW_GET_FUN(__glewUniformMatrix3fv)
#define glUniformMatrix4fv GLEW_GET_FUN(__glewUniformMatrix4fv)
#define glUseProgram GLEW_GET_FUN(__glewUseProgram)
#define glValidateProgram GLEW_GET_FUN(__glewValidateProgram)
#define glVertexAttrib1d GLEW_GET_FUN(__glewVertexAttrib1d)
#define glVertexAttrib1dv GLEW_GET_FUN(__glewVertexAttrib1dv)
#define glVertexAttrib1f GLEW_GET_FUN(__glewVertexAttrib1f)
#define glVertexAttrib1fv GLEW_GET_FUN(__glewVertexAttrib1fv)
#define glVertexAttrib1s GLEW_GET_FUN(__glewVertexAttrib1s)
#define glVertexAttrib1sv GLEW_GET_FUN(__glewVertexAttrib1sv)
#define glVertexAttrib2d GLEW_GET_FUN(__glewVertexAttrib2d)
#define glVertexAttrib2dv GLEW_GET_FUN(__glewVertexAttrib2dv)
#define glVertexAttrib2f GLEW_GET_FUN(__glewVertexAttrib2f)
#define glVertexAttrib2fv GLEW_GET_FUN(__glewVertexAttrib2fv)
#define glVertexAttrib2s GLEW_GET_FUN(__glewVertexAttrib2s)
#define glVertexAttrib2sv GLEW_GET_FUN(__glewVertexAttrib2sv)
#define glVertexAttrib3d GLEW_GET_FUN(__glewVertexAttrib3d)
#define glVertexAttrib3dv GLEW_GET_FUN(__glewVertexAttrib3dv)
#define glVertexAttrib3f GLEW_GET_FUN(__glewVertexAttrib3f)
#define glVertexAttrib3fv GLEW_GET_FUN(__glewVertexAttrib3fv)
#define glVertexAttrib3s GLEW_GET_FUN(__glewVertexAttrib3s)
#define glVertexAttrib3sv GLEW_GET_FUN(__glewVertexAttrib3sv)
#define glVertexAttrib4Nbv GLEW_GET_FUN(__glewVertexAttrib4Nbv)
#define glVertexAttrib4Niv GLEW_GET_FUN(__glewVertexAttrib4Niv)
#define glVertexAttrib4Nsv GLEW_GET_FUN(__glewVertexAttrib4Nsv)
#define glVertexAttrib4Nub GLEW_GET_FUN(__glewVertexAttrib4Nub)
#define glVertexAttrib4Nubv GLEW_GET_FUN(__glewVertexAttrib4Nubv)
#define glVertexAttrib4Nuiv GLEW_GET_FUN(__glewVertexAttrib4Nuiv)
#define glVertexAttrib4Nusv GLEW_GET_FUN(__glewVertexAttrib4Nusv)
#define glVertexAttrib4bv GLEW_GET_FUN(__glewVertexAttrib4bv)
#define glVertexAttrib4d GLEW_GET_FUN(__glewVertexAttrib4d)
#define glVertexAttrib4dv GLEW_GET_FUN(__glewVertexAttrib4dv)
#define glVertexAttrib4f GLEW_GET_FUN(__glewVertexAttrib4f)
#define glVertexAttrib4fv GLEW_GET_FUN(__glewVertexAttrib4fv)
#define glVertexAttrib4iv GLEW_GET_FUN(__glewVertexAttrib4iv)
#define glVertexAttrib4s GLEW_GET_FUN(__glewVertexAttrib4s)
#define glVertexAttrib4sv GLEW_GET_FUN(__glewVertexAttrib4sv)
#define glVertexAttrib4ubv GLEW_GET_FUN(__glewVertexAttrib4ubv)
#define glVertexAttrib4uiv GLEW_GET_FUN(__glewVertexAttrib4uiv)
#define glVertexAttrib4usv GLEW_GET_FUN(__glewVertexAttrib4usv)
#define glVertexAttribPointer GLEW_GET_FUN(__glewVertexAttribPointer)

#define GLEW_VERSION_2_0 GLEW_GET_VAR(__GLEW_VERSION_2_0)

#endif /* GL_VERSION_2_0 */

/* ----------------------------- GL_VERSION_2_1 ---------------------------- */

#ifndef GL_VERSION_2_1
#define GL_VERSION_2_1 1

#define GL_CURRENT_RASTER_SECONDARY_COLOR 0x845F
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_FLOAT_MAT2x3 0x8B65
#define GL_FLOAT_MAT2x4 0x8B66
#define GL_FLOAT_MAT3x2 0x8B67
#define GL_FLOAT_MAT3x4 0x8B68
#define GL_FLOAT_MAT4x2 0x8B69
#define GL_FLOAT_MAT4x3 0x8B6A
#define GL_SRGB 0x8C40
#define GL_SRGB8 0x8C41
#define GL_SRGB_ALPHA 0x8C42
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_SLUMINANCE_ALPHA 0x8C44
#define GL_SLUMINANCE8_ALPHA8 0x8C45
#define GL_SLUMINANCE 0x8C46
#define GL_SLUMINANCE8 0x8C47
#define GL_COMPRESSED_SRGB 0x8C48
#define GL_COMPRESSED_SRGB_ALPHA 0x8C49
#define GL_COMPRESSED_SLUMINANCE 0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA 0x8C4B

typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4X2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4X3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

#define glUniformMatrix2x3fv GLEW_GET_FUN(__glewUniformMatrix2x3fv)
#define glUniformMatrix2x4fv GLEW_GET_FUN(__glewUniformMatrix2x4fv)
#define glUniformMatrix3x2fv GLEW_GET_FUN(__glewUniformMatrix3x2fv)
#define glUniformMatrix3x4fv GLEW_GET_FUN(__glewUniformMatrix3x4fv)
#define glUniformMatrix4x2fv GLEW_GET_FUN(__glewUniformMatrix4x2fv)
#define glUniformMatrix4x3fv GLEW_GET_FUN(__glewUniformMatrix4x3fv)

#define GLEW_VERSION_2_1 GLEW_GET_VAR(__GLEW_VERSION_2_1)

#endif /* GL_VERSION_2_1 */

/* ----------------------------- GL_VERSION_3_0 ---------------------------- */

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1

#define GL_MAX_CLIP_DISTANCES GL_MAX_CLIP_PLANES
#define GL_CLIP_DISTANCE5 GL_CLIP_PLANE5
#define GL_CLIP_DISTANCE1 GL_CLIP_PLANE1
#define GL_CLIP_DISTANCE3 GL_CLIP_PLANE3
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_R_TO_TEXTURE_ARB
#define GL_CLIP_DISTANCE0 GL_CLIP_PLANE0
#define GL_CLIP_DISTANCE4 GL_CLIP_PLANE4
#define GL_CLIP_DISTANCE2 GL_CLIP_PLANE2
#define GL_MAX_VARYING_COMPONENTS GL_MAX_VARYING_FLOATS
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x0001
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_CONTEXT_FLAGS 0x821E
#define GL_DEPTH_BUFFER 0x8223
#define GL_STENCIL_BUFFER 0x8224
#define GL_COMPRESSED_RED 0x8225
#define GL_COMPRESSED_RG 0x8226
#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER 0x88FD
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
#define GL_MIN_PROGRAM_TEXEL_OFFSET 0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET 0x8905
#define GL_CLAMP_VERTEX_COLOR 0x891A
#define GL_CLAMP_FRAGMENT_COLOR 0x891B
#define GL_CLAMP_READ_COLOR 0x891C
#define GL_FIXED_ONLY 0x891D
#define GL_TEXTURE_RED_TYPE 0x8C10
#define GL_TEXTURE_GREEN_TYPE 0x8C11
#define GL_TEXTURE_BLUE_TYPE 0x8C12
#define GL_TEXTURE_ALPHA_TYPE 0x8C13
#define GL_TEXTURE_LUMINANCE_TYPE 0x8C14
#define GL_TEXTURE_INTENSITY_TYPE 0x8C15
#define GL_TEXTURE_DEPTH_TYPE 0x8C16
#define GL_UNSIGNED_NORMALIZED 0x8C17
#define GL_TEXTURE_1D_ARRAY 0x8C18
#define GL_PROXY_TEXTURE_1D_ARRAY 0x8C19
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_PROXY_TEXTURE_2D_ARRAY 0x8C1B
#define GL_TEXTURE_BINDING_1D_ARRAY 0x8C1C
#define GL_TEXTURE_BINDING_2D_ARRAY 0x8C1D
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV 0x8C3B
#define GL_RGB9_E5 0x8C3D
#define GL_UNSIGNED_INT_5_9_9_9_REV 0x8C3E
#define GL_TEXTURE_SHARED_SIZE 0x8C3F
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH 0x8C76
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE 0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS 0x8C80
#define GL_TRANSFORM_FEEDBACK_VARYINGS 0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START 0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE 0x8C85
#define GL_PRIMITIVES_GENERATED 0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN 0x8C88
#define GL_RASTERIZER_DISCARD 0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#define GL_INTERLEAVED_ATTRIBS 0x8C8C
#define GL_SEPARATE_ATTRIBS 0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING 0x8C8F
#define GL_RGBA32UI 0x8D70
#define GL_RGB32UI 0x8D71
#define GL_RGBA16UI 0x8D76
#define GL_RGB16UI 0x8D77
#define GL_RGBA8UI 0x8D7C
#define GL_RGB8UI 0x8D7D
#define GL_RGBA32I 0x8D82
#define GL_RGB32I 0x8D83
#define GL_RGBA16I 0x8D88
#define GL_RGB16I 0x8D89
#define GL_RGBA8I 0x8D8E
#define GL_RGB8I 0x8D8F
#define GL_RED_INTEGER 0x8D94
#define GL_GREEN_INTEGER 0x8D95
#define GL_BLUE_INTEGER 0x8D96
#define GL_ALPHA_INTEGER 0x8D97
#define GL_RGB_INTEGER 0x8D98
#define GL_RGBA_INTEGER 0x8D99
#define GL_BGR_INTEGER 0x8D9A
#define GL_BGRA_INTEGER 0x8D9B
#define GL_SAMPLER_1D_ARRAY 0x8DC0
#define GL_SAMPLER_2D_ARRAY 0x8DC1
#define GL_SAMPLER_1D_ARRAY_SHADOW 0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW 0x8DC4
#define GL_SAMPLER_CUBE_SHADOW 0x8DC5
#define GL_UNSIGNED_INT_VEC2 0x8DC6
#define GL_UNSIGNED_INT_VEC3 0x8DC7
#define GL_UNSIGNED_INT_VEC4 0x8DC8
#define GL_INT_SAMPLER_1D 0x8DC9
#define GL_INT_SAMPLER_2D 0x8DCA
#define GL_INT_SAMPLER_3D 0x8DCB
#define GL_INT_SAMPLER_CUBE 0x8DCC
#define GL_INT_SAMPLER_1D_ARRAY 0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY 0x8DCF
#define GL_UNSIGNED_INT_SAMPLER_1D 0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D 0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D 0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE 0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY 0x8DD7
#define GL_QUERY_WAIT 0x8E13
#define GL_QUERY_NO_WAIT 0x8E14
#define GL_QUERY_BY_REGION_WAIT 0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT 0x8E16

typedef void (GLAPIENTRY * PFNGLBEGINCONDITIONALRENDERPROC) (GLuint, GLenum);
typedef void (GLAPIENTRY * PFNGLBEGINTRANSFORMFEEDBACKPROC) (GLenum);
typedef void (GLAPIENTRY * PFNGLBINDFRAGDATALOCATIONPROC) (GLuint, GLuint, const GLchar*);
typedef void (GLAPIENTRY * PFNGLCLAMPCOLORPROC) (GLenum, GLenum);
typedef void (GLAPIENTRY * PFNGLCLEARBUFFERFIPROC) (GLenum, GLint, GLfloat, GLint);
typedef void (GLAPIENTRY * PFNGLCLEARBUFFERFVPROC) (GLenum, GLint, const GLfloat*);
typedef void (GLAPIENTRY * PFNGLCLEARBUFFERIVPROC) (GLenum, GLint, const GLint*);
typedef void (GLAPIENTRY * PFNGLCLEARBUFFERUIVPROC) (GLenum, GLint, const GLuint*);
typedef void (GLAPIENTRY * PFNGLCOLORMASKIPROC) (GLuint, GLboolean, GLboolean, GLboolean, GLboolean);
typedef void (GLAPIENTRY * PFNGLDISABLEIPROC) (GLenum, GLuint);
typedef void (GLAPIENTRY * PFNGLENABLEIPROC) (GLenum, GLuint);
typedef void (GLAPIENTRY * PFNGLENDCONDITIONALRENDERPROC) (void);
typedef void (GLAPIENTRY * PFNGLENDTRANSFORMFEEDBACKPROC) (void);
typedef void (GLAPIENTRY * PFNGLGETBOOLEANI_VPROC) (GLenum, GLuint, GLboolean*);
typedef GLint (GLAPIENTRY * PFNGLGETFRAGDATALOCATIONPROC) (GLuint, const GLchar*);
typedef const GLubyte* (GLAPIENTRY * PFNGLGETSTRINGIPROC) (GLenum, GLuint);
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIIVPROC) (GLenum, GLenum, GLint*);
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIUIVPROC) (GLenum, GLenum, GLuint*);
typedef void (GLAPIENTRY * PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) (GLuint, GLuint, GLint*);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMUIVPROC) (GLuint, GLint, GLuint*);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIIVPROC) (GLuint, GLenum, GLint*);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIUIVPROC) (GLuint, GLenum, GLuint*);
typedef GLboolean (GLAPIENTRY * PFNGLISENABLEDIPROC) (GLenum, GLuint);
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIIVPROC) (GLenum, GLenum, const GLint*);
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIUIVPROC) (GLenum, GLenum, const GLuint*);
typedef void (GLAPIENTRY * PFNGLTRANSFORMFEEDBACKVARYINGSPROC) (GLuint, GLsizei, const GLchar **, GLenum);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UIPROC) (GLint, GLuint);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UIVPROC) (GLint, GLsizei, const GLuint*);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UIPROC) (GLint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UIVPROC) (GLint, GLsizei, const GLuint*);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UIPROC) (GLint, GLuint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UIVPROC) (GLint, GLsizei, const GLuint*);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UIPROC) (GLint, GLuint, GLuint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UIVPROC) (GLint, GLsizei, const GLuint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1IPROC) (GLuint, GLint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1IVPROC) (GLuint, const GLint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1UIPROC) (GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1UIVPROC) (GLuint, const GLuint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2IPROC) (GLuint, GLint, GLint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2IVPROC) (GLuint, const GLint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2UIPROC) (GLuint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2UIVPROC) (GLuint, const GLuint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3IPROC) (GLuint, GLint, GLint, GLint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3IVPROC) (GLuint, const GLint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3UIPROC) (GLuint, GLuint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3UIVPROC) (GLuint, const GLuint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4BVPROC) (GLuint, const GLbyte*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4IPROC) (GLuint, GLint, GLint, GLint, GLint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4IVPROC) (GLuint, const GLint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4SVPROC) (GLuint, const GLshort*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UBVPROC) (GLuint, const GLubyte*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UIPROC) (GLuint, GLuint, GLuint, GLuint, GLuint);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UIVPROC) (GLuint, const GLuint*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4USVPROC) (GLuint, const GLushort*);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBIPOINTERPROC) (GLuint, GLint, GLenum, GLsizei, const GLvoid*);

#define glBeginConditionalRender GLEW_GET_FUN(__glewBeginConditionalRender)
#define glBeginTransformFeedback GLEW_GET_FUN(__glewBeginTransformFeedback)
#define glBindFragDataLocation GLEW_GET_FUN(__glewBindFragDataLocation)
#define glClampColor GLEW_GET_FUN(__glewClampColor)
#define glClearBufferfi GLEW_GET_FUN(__glewClearBufferfi)
#define glClearBufferfv GLEW_GET_FUN(__glewClearBufferfv)
#define glClearBufferiv GLEW_GET_FUN(__glewClearBufferiv)
#define glClearBufferuiv GLEW_GET_FUN(__glewClearBufferuiv)
#define glColorMaski GLEW_GET_FUN(__glewColorMaski)
#define glDisablei GLEW_GET_FUN(__glewDisablei)
#define glEnablei GLEW_GET_FUN(__glewEnablei)
#define glEndConditionalRender GLEW_GET_FUN(__glewEndConditionalRender)
#define glEndTransformFeedback GLEW_GET_FUN(__glewEndTransformFeedback)
#define glGetBooleani_v GLEW_GET_FUN(__glewGetBooleani_v)
#define glGetFragDataLocation GLEW_GET_FUN(__glewGetFragDataLocation)
#define glGetStringi GLEW_GET_FUN(__glewGetStringi)
#define glGetTexParameterIiv GLEW_GET_FUN(__glewGetTexParameterIiv)
#define glGetTexParameterIuiv GLEW_GET_FUN(__glewGetTexParameterIuiv)
#define glGetTransformFeedbackVarying GLEW_GET_FUN(__glewGetTransformFeedbackVarying)
#define glGetUniformuiv GLEW_GET_FUN(__glewGetUniformuiv)
#define glGetVertexAttribIiv GLEW_GET_FUN(__glewGetVertexAttribIiv)
#define glGetVertexAttribIuiv GLEW_GET_FUN(__glewGetVertexAttribIuiv)
#define glIsEnabledi GLEW_GET_FUN(__glewIsEnabledi)
#define glTexParameterIiv GLEW_GET_FUN(__glewTexParameterIiv)
#define glTexParameterIuiv GLEW_GET_FUN(__glewTexParameterIuiv)
#define glTransformFeedbackVaryings GLEW_GET_FUN(__glewTransformFeedbackVaryings)
#define glUniform1ui GLEW_GET_FUN(__glewUniform1ui)
#define glUniform1uiv GLEW_GET_FUN(__glewUniform1uiv)
#define glUniform2ui GLEW_GET_FUN(__glewUniform2ui)
#define glUniform2uiv GLEW_GET_FUN(__glewUniform2uiv)
#define glUniform3ui GLEW_GET_FUN(__glewUniform3ui)
#define glUniform3uiv GLEW_GET_FUN(__glewUniform3uiv)
#define glUniform4ui GLEW_GET_FUN(__glewUniform4ui)
#define glUniform4uiv GLEW_GET_FUN(__glewUniform4uiv)
#define glVertexAttribI1i GLEW_GET_FUN(__glewVertexAttribI1i)
#define glVertexAttribI1iv GLEW_GET_FUN(__glewVertexAttribI1iv)
#define glVertexAttribI1ui GLEW_GET_FUN(__glewVertexAttribI1ui)
#define glVertexAttribI1uiv GLEW_GET_FUN(__glewVertexAttribI1uiv)
#define glVertexAttribI2i GLEW_GET_FUN(__glewVertexAttribI2i)
#define glVertexAttribI2iv GLEW_GET_FUN(__glewVertexAttribI2iv)
#define glVertexAttribI2ui GLEW_GET_FUN(__glewVertexAttribI2ui)
#define glVertexAttribI2uiv GLEW_GET_FUN(__glewVertexAttribI2uiv)
#define glVertexAttribI3i GLEW_GET_FUN(__glewVertexAttribI3i)
#define glVertexAttribI3iv GLEW_GET_FUN(__glewVertexAttribI3iv)
#define glVertexAttribI3ui GLEW_GET_FUN(__glewVertexAttribI3ui)
#define glVertexAttribI3uiv GLEW_GET_FUN(__glewVertexAttribI3uiv)
#define glVertexAttribI4bv GLEW_GET_FUN(__glewVertexAttribI4bv)
#define glVertexAttribI4i GLEW_GET_FUN(__glewVertexAttribI4i)
#define glVertexAttribI4iv GLEW_GET_FUN(__glewVertexAttribI4iv)
#define glVertexAttribI4sv GLEW_GET_FUN(__glewVertexAttribI4sv)
#define glVertexAttribI4ubv GLEW_GET_FUN(__glewVertexAttribI4ubv)
#define glVertexAttribI4ui GLEW_GET_FUN(__glewVertexAttribI4ui)
#define glVertexAttribI4uiv GLEW_GET_FUN(__glewVertexAttribI4uiv)
#define glVertexAttribI4usv GLEW_GET_FUN(__glewVertexAttribI4usv)
#define glVertexAttribIPointer GLEW_GET_FUN(__glewVertexAttribIPointer)

#define GLEW_VERSION_3_0 GLEW_GET_VAR(__GLEW_VERSION_3_0)

#endif /* GL_VERSION_3_0 */

/* ----------------------------- GL_VERSION_3_1 ---------------------------- */

#ifndef GL_VERSION_3_1
#define GL_VERSION_3_1 1

#define GL_TEXTURE_RECTANGLE 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE 0x84F8
#define GL_SAMPLER_2D_RECT 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW 0x8B64
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT 0x8C2E
#define GL_SAMPLER_BUFFER 0x8DC2
#define GL_INT_SAMPLER_2D_RECT 0x8DCD
#define GL_INT_SAMPLER_BUFFER 0x8DD0
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT 0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_BUFFER 0x8DD8
#define GL_RED_SNORM 0x8F90
#define GL_RG_SNORM 0x8F91
#define GL_RGB_SNORM 0x8F92
#define GL_RGBA_SNORM 0x8F93
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_R16_SNORM 0x8F98
#define GL_RG16_SNORM 0x8F99
#define GL_RGB16_SNORM 0x8F9A
#define GL_RGBA16_SNORM 0x8F9B
#define GL_SIGNED_NORMALIZED 0x8F9C
#define GL_PRIMITIVE_RESTART 0x8F9D
#define GL_PRIMITIVE_RESTART_INDEX 0x8F9E
#define GL_BUFFER_ACCESS_FLAGS 0x911F
#define GL_BUFFER_MAP_LENGTH 0x9120
#define GL_BUFFER_MAP_OFFSET 0x9121

typedef void (GLAPIENTRY * PFNGLDRAWARRAYSINSTANCEDPROC) (GLenum, GLint, GLsizei, GLsizei);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINSTANCEDPROC) (GLenum, GLsizei, GLenum, const GLvoid*, GLsizei);
typedef void (GLAPIENTRY * PFNGLPRIMITIVERESTARTINDEXPROC) (GLuint);
typedef void (GLAPIENTRY * PFNGLTEXBUFFERPROC) (GLenum, GLenum, GLuint);

#define glDrawArraysInstanced GLEW_GET_FUN(__glewDrawArraysInstanced)
#define glDrawElementsInstanced GLEW_GET_FUN(__glewDrawElementsInstanced)
#define glPrimitiveRestartIndex GLEW_GET_FUN(__glewPrimitiveRestartIndex)
#define glTexBuffer GLEW_GET_FUN(__glewTexBuffer)

#define GLEW_VERSION_3_1 GLEW_GET_VAR(__GLEW_VERSION_3_1)

#endif /* GL_VERSION_3_1 */

/* ----------------------------- GL_VERSION_3_2 ---------------------------- */

#ifndef GL_VERSION_3_2
#define GL_VERSION_3_2 1

#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_LINES_ADJACENCY 0x000A
#define GL_LINE_STRIP_ADJACENCY 0x000B
#define GL_TRIANGLES_ADJACENCY 0x000C
#define GL_TRIANGLE_STRIP_ADJACENCY 0x000D
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_GEOMETRY_VERTICES_OUT 0x8916
#define GL_GEOMETRY_INPUT_TYPE 0x8917
#define GL_GEOMETRY_OUTPUT_TYPE 0x8918
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS 0x8C29
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED 0x8DA7
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS 0x8DA8
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS 0x8DDF
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES 0x8DE0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS 0x8DE1
#define GL_MAX_VERTEX_OUTPUT_COMPONENTS 0x9122
#define GL_MAX_GEOMETRY_INPUT_COMPONENTS 0x9123
#define GL_MAX_GEOMETRY_OUTPUT_COMPONENTS 0x9124
#define GL_MAX_FRAGMENT_INPUT_COMPONENTS 0x9125
#define GL_CONTEXT_PROFILE_MASK 0x9126

typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTUREPROC) (GLenum, GLenum, GLuint, GLint);
typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERI64VPROC) (GLenum, GLenum, GLint64 *);
typedef void (GLAPIENTRY * PFNGLGETINTEGER64I_VPROC) (GLenum, GLuint, GLint64 *);

#define glFramebufferTexture GLEW_GET_FUN(__glewFramebufferTexture)
#define glGetBufferParameteri64v GLEW_GET_FUN(__glewGetBufferParameteri64v)
#define glGetInteger64i_v GLEW_GET_FUN(__glewGetInteger64i_v)

#define GLEW_VERSION_3_2 GLEW_GET_VAR(__GLEW_VERSION_3_2)

#endif /* GL_VERSION_3_2 */

/* ----------------------------- GL_VERSION_3_3 ---------------------------- */

#ifndef GL_VERSION_3_3
#define GL_VERSION_3_3 1

#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR 0x88FE
#define GL_ANY_SAMPLES_PASSED 0x8C2F
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#define GL_RGB10_A2UI 0x906F

typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);

#define glVertexAttribDivisor GLEW_GET_FUN(__glewVertexAttribDivisor)

#define GLEW_VERSION_3_3 GLEW_GET_VAR(__GLEW_VERSION_3_3)

#endif /* GL_VERSION_3_3 */

/* ----------------------------- GL_VERSION_4_0 ---------------------------- */

#ifndef GL_VERSION_4_0
#define GL_VERSION_4_0 1

#define GL_GEOMETRY_SHADER_INVOCATIONS 0x887F
#define GL_SAMPLE_SHADING 0x8C36
#define GL_MIN_SAMPLE_SHADING_VALUE 0x8C37
#define GL_MAX_GEOMETRY_SHADER_INVOCATIONS 0x8E5A
#define GL_MIN_FRAGMENT_INTERPOLATION_OFFSET 0x8E5B
#define GL_MAX_FRAGMENT_INTERPOLATION_OFFSET 0x8E5C
#define GL_FRAGMENT_INTERPOLATION_OFFSET_BITS 0x8E5D
#define GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET 0x8E5E
#define GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET 0x8E5F
#define GL_MAX_PROGRAM_TEXTURE_GATHER_COMPONENTS 0x8F9F
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#define GL_TEXTURE_BINDING_CUBE_MAP_ARRAY 0x900A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARRAY 0x900B
#define GL_SAMPLER_CUBE_MAP_ARRAY 0x900C
#define GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW 0x900D
#define GL_INT_SAMPLER_CUBE_MAP_ARRAY 0x900E
#define GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY 0x900F

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEIPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONIPROC) (GLuint buf, GLenum mode);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCIPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (GLAPIENTRY * PFNGLMINSAMPLESHADINGPROC) (GLclampf value);

#define glBlendEquationSeparatei GLEW_GET_FUN(__glewBlendEquationSeparatei)
#define glBlendEquationi GLEW_GET_FUN(__glewBlendEquationi)
#define glBlendFuncSeparatei GLEW_GET_FUN(__glewBlendFuncSeparatei)
#define glBlendFunci GLEW_GET_FUN(__glewBlendFunci)
#define glMinSampleShading GLEW_GET_FUN(__glewMinSampleShading)

#define GLEW_VERSION_4_0 GLEW_GET_VAR(__GLEW_VERSION_4_0)

#endif /* GL_VERSION_4_0 */

/* ----------------------------- GL_VERSION_4_1 ---------------------------- */

#ifndef GL_VERSION_4_1
#define GL_VERSION_4_1 1

#define GLEW_VERSION_4_1 GLEW_GET_VAR(__GLEW_VERSION_4_1)

#endif /* GL_VERSION_4_1 */

/* -------------------------- GL_3DFX_multisample -------------------------- */

#ifndef GL_3DFX_multisample
#define GL_3DFX_multisample 1

#define GL_MULTISAMPLE_3DFX 0x86B2
#define GL_SAMPLE_BUFFERS_3DFX 0x86B3
#define GL_SAMPLES_3DFX 0x86B4
#define GL_MULTISAMPLE_BIT_3DFX 0x20000000

#define GLEW_3DFX_multisample GLEW_GET_VAR(__GLEW_3DFX_multisample)

#endif /* GL_3DFX_multisample */

/* ---------------------------- GL_3DFX_tbuffer ---------------------------- */

#ifndef GL_3DFX_tbuffer
#define GL_3DFX_tbuffer 1

typedef void (GLAPIENTRY * PFNGLTBUFFERMASK3DFXPROC) (GLuint mask);

#define glTbufferMask3DFX GLEW_GET_FUN(__glewTbufferMask3DFX)

#define GLEW_3DFX_tbuffer GLEW_GET_VAR(__GLEW_3DFX_tbuffer)

#endif /* GL_3DFX_tbuffer */

/* -------------------- GL_3DFX_texture_compression_FXT1 ------------------- */

#ifndef GL_3DFX_texture_compression_FXT1
#define GL_3DFX_texture_compression_FXT1 1

#define GL_COMPRESSED_RGB_FXT1_3DFX 0x86B0
#define GL_COMPRESSED_RGBA_FXT1_3DFX 0x86B1

#define GLEW_3DFX_texture_compression_FXT1 GLEW_GET_VAR(__GLEW_3DFX_texture_compression_FXT1)

#endif /* GL_3DFX_texture_compression_FXT1 */

/* ----------------------- GL_AMD_conservative_depth ----------------------- */

#ifndef GL_AMD_conservative_depth
#define GL_AMD_conservative_depth 1

#define GLEW_AMD_conservative_depth GLEW_GET_VAR(__GLEW_AMD_conservative_depth)

#endif /* GL_AMD_conservative_depth */

/* -------------------------- GL_AMD_debug_output -------------------------- */

#ifndef GL_AMD_debug_output
#define GL_AMD_debug_output 1

#define GL_MAX_DEBUG_MESSAGE_LENGTH_AMD 0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_AMD 0x9144
#define GL_DEBUG_LOGGED_MESSAGES_AMD 0x9145
#define GL_DEBUG_SEVERITY_HIGH_AMD 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_AMD 0x9147
#define GL_DEBUG_SEVERITY_LOW_AMD 0x9148
#define GL_DEBUG_CATEGORY_API_ERROR_AMD 0x9149
#define GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD 0x914A
#define GL_DEBUG_CATEGORY_DEPRECATION_AMD 0x914B
#define GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD 0x914C
#define GL_DEBUG_CATEGORY_PERFORMANCE_AMD 0x914D
#define GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD 0x914E
#define GL_DEBUG_CATEGORY_APPLICATION_AMD 0x914F
#define GL_DEBUG_CATEGORY_OTHER_AMD 0x9150

typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECALLBACKAMDPROC) (GLDEBUGPROCAMD callback, void* userParam);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGEENABLEAMDPROC) (GLenum category, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGEINSERTAMDPROC) (GLenum category, GLenum severity, GLuint id, GLsizei length, const char* buf);
typedef GLuint (GLAPIENTRY * PFNGLGETDEBUGMESSAGELOGAMDPROC) (GLuint count, GLsizei bufsize, GLenum* categories, GLuint* severities, GLuint* ids, GLsizei* lengths, char* message);

#define glDebugMessageCallbackAMD GLEW_GET_FUN(__glewDebugMessageCallbackAMD)
#define glDebugMessageEnableAMD GLEW_GET_FUN(__glewDebugMessageEnableAMD)
#define glDebugMessageInsertAMD GLEW_GET_FUN(__glewDebugMessageInsertAMD)
#define glGetDebugMessageLogAMD GLEW_GET_FUN(__glewGetDebugMessageLogAMD)

#define GLEW_AMD_debug_output GLEW_GET_VAR(__GLEW_AMD_debug_output)

#endif /* GL_AMD_debug_output */

/* ----------------------- GL_AMD_draw_buffers_blend ----------------------- */

#ifndef GL_AMD_draw_buffers_blend
#define GL_AMD_draw_buffers_blend 1

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONINDEXEDAMDPROC) (GLuint buf, GLenum mode);
typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEINDEXEDAMDPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCINDEXEDAMDPROC) (GLuint buf, GLenum src, GLenum dst);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEINDEXEDAMDPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);

#define glBlendEquationIndexedAMD GLEW_GET_FUN(__glewBlendEquationIndexedAMD)
#define glBlendEquationSeparateIndexedAMD GLEW_GET_FUN(__glewBlendEquationSeparateIndexedAMD)
#define glBlendFuncIndexedAMD GLEW_GET_FUN(__glewBlendFuncIndexedAMD)
#define glBlendFuncSeparateIndexedAMD GLEW_GET_FUN(__glewBlendFuncSeparateIndexedAMD)

#define GLEW_AMD_draw_buffers_blend GLEW_GET_VAR(__GLEW_AMD_draw_buffers_blend)

#endif /* GL_AMD_draw_buffers_blend */

/* ------------------------- GL_AMD_name_gen_delete ------------------------ */

#ifndef GL_AMD_name_gen_delete
#define GL_AMD_name_gen_delete 1

#define GL_DATA_BUFFER_AMD 0x9151
#define GL_PERFORMANCE_MONITOR_AMD 0x9152
#define GL_QUERY_OBJECT_AMD 0x9153
#define GL_VERTEX_ARRAY_OBJECT_AMD 0x9154
#define GL_SAMPLER_OBJECT_AMD 0x9155

typedef void (GLAPIENTRY * PFNGLDELETENAMESAMDPROC) (GLenum identifier, GLuint num, const GLuint* names);
typedef void (GLAPIENTRY * PFNGLGENNAMESAMDPROC) (GLenum identifier, GLuint num, GLuint* names);
typedef GLboolean (GLAPIENTRY * PFNGLISNAMEAMDPROC) (GLenum identifier, GLuint name);

#define glDeleteNamesAMD GLEW_GET_FUN(__glewDeleteNamesAMD)
#define glGenNamesAMD GLEW_GET_FUN(__glewGenNamesAMD)
#define glIsNameAMD GLEW_GET_FUN(__glewIsNameAMD)

#define GLEW_AMD_name_gen_delete GLEW_GET_VAR(__GLEW_AMD_name_gen_delete)

#endif /* GL_AMD_name_gen_delete */

/* ----------------------- GL_AMD_performance_monitor ---------------------- */

#ifndef GL_AMD_performance_monitor
#define GL_AMD_performance_monitor 1

#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_COUNTER_TYPE_AMD 0x8BC0
#define GL_COUNTER_RANGE_AMD 0x8BC1
#define GL_UNSIGNED_INT64_AMD 0x8BC2
#define GL_PERCENTAGE_AMD 0x8BC3
#define GL_PERFMON_RESULT_AVAILABLE_AMD 0x8BC4
#define GL_PERFMON_RESULT_SIZE_AMD 0x8BC5
#define GL_PERFMON_RESULT_AMD 0x8BC6

typedef void (GLAPIENTRY * PFNGLBEGINPERFMONITORAMDPROC) (GLuint monitor);
typedef void (GLAPIENTRY * PFNGLDELETEPERFMONITORSAMDPROC) (GLsizei n, GLuint* monitors);
typedef void (GLAPIENTRY * PFNGLENDPERFMONITORAMDPROC) (GLuint monitor);
typedef void (GLAPIENTRY * PFNGLGENPERFMONITORSAMDPROC) (GLsizei n, GLuint* monitors);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERDATAAMDPROC) (GLuint monitor, GLenum pname, GLsizei dataSize, GLuint* data, GLint *bytesWritten);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERINFOAMDPROC) (GLuint group, GLuint counter, GLenum pname, void* data);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC) (GLuint group, GLuint counter, GLsizei bufSize, GLsizei* length, char *counterString);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERSAMDPROC) (GLuint group, GLint* numCounters, GLint *maxActiveCounters, GLsizei countersSize, GLuint *counters);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORGROUPSTRINGAMDPROC) (GLuint group, GLsizei bufSize, GLsizei* length, char *groupString);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORGROUPSAMDPROC) (GLint* numGroups, GLsizei groupsSize, GLuint *groups);
typedef void (GLAPIENTRY * PFNGLSELECTPERFMONITORCOUNTERSAMDPROC) (GLuint monitor, GLboolean enable, GLuint group, GLint numCounters, GLuint* counterList);

#define glBeginPerfMonitorAMD GLEW_GET_FUN(__glewBeginPerfMonitorAMD)
#define glDeletePerfMonitorsAMD GLEW_GET_FUN(__glewDeletePerfMonitorsAMD)
#define glEndPerfMonitorAMD GLEW_GET_FUN(__glewEndPerfMonitorAMD)
#define glGenPerfMonitorsAMD GLEW_GET_FUN(__glewGenPerfMonitorsAMD)
#define glGetPerfMonitorCounterDataAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterDataAMD)
#define glGetPerfMonitorCounterInfoAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterInfoAMD)
#define glGetPerfMonitorCounterStringAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterStringAMD)
#define glGetPerfMonitorCountersAMD GLEW_GET_FUN(__glewGetPerfMonitorCountersAMD)
#define glGetPerfMonitorGroupStringAMD GLEW_GET_FUN(__glewGetPerfMonitorGroupStringAMD)
#define glGetPerfMonitorGroupsAMD GLEW_GET_FUN(__glewGetPerfMonitorGroupsAMD)
#define glSelectPerfMonitorCountersAMD GLEW_GET_FUN(__glewSelectPerfMonitorCountersAMD)

#define GLEW_AMD_performance_monitor GLEW_GET_VAR(__GLEW_AMD_performance_monitor)

#endif /* GL_AMD_performance_monitor */

/* ------------------ GL_AMD_seamless_cubemap_per_texture ------------------ */

#ifndef GL_AMD_seamless_cubemap_per_texture
#define GL_AMD_seamless_cubemap_per_texture 1

#define GL_TEXTURE_CUBE_MAP_SEAMLESS_ARB 0x884F

#define GLEW_AMD_seamless_cubemap_per_texture GLEW_GET_VAR(__GLEW_AMD_seamless_cubemap_per_texture)

#endif /* GL_AMD_seamless_cubemap_per_texture */

/* ---------------------- GL_AMD_shader_stencil_export --------------------- */

#ifndef GL_AMD_shader_stencil_export
#define GL_AMD_shader_stencil_export 1

#define GLEW_AMD_shader_stencil_export GLEW_GET_VAR(__GLEW_AMD_shader_stencil_export)

#endif /* GL_AMD_shader_stencil_export */

/* ------------------------ GL_AMD_texture_texture4 ------------------------ */

#ifndef GL_AMD_texture_texture4
#define GL_AMD_texture_texture4 1

#define GLEW_AMD_texture_texture4 GLEW_GET_VAR(__GLEW_AMD_texture_texture4)

#endif /* GL_AMD_texture_texture4 */

/* --------------- GL_AMD_transform_feedback3_lines_triangles -------------- */

#ifndef GL_AMD_transform_feedback3_lines_triangles
#define GL_AMD_transform_feedback3_lines_triangles 1

#define GLEW_AMD_transform_feedback3_lines_triangles GLEW_GET_VAR(__GLEW_AMD_transform_feedback3_lines_triangles)

#endif /* GL_AMD_transform_feedback3_lines_triangles */

/* -------------------- GL_AMD_vertex_shader_tessellator ------------------- */

#ifndef GL_AMD_vertex_shader_tessellator
#define GL_AMD_vertex_shader_tessellator 1

#define GL_SAMPLER_BUFFER_AMD 0x9001
#define GL_INT_SAMPLER_BUFFER_AMD 0x9002
#define GL_UNSIGNED_INT_SAMPLER_BUFFER_AMD 0x9003
#define GL_TESSELLATION_MODE_AMD 0x9004
#define GL_TESSELLATION_FACTOR_AMD 0x9005
#define GL_DISCRETE_AMD 0x9006
#define GL_CONTINUOUS_AMD 0x9007

typedef void (GLAPIENTRY * PFNGLTESSELLATIONFACTORAMDPROC) (GLfloat factor);
typedef void (GLAPIENTRY * PFNGLTESSELLATIONMODEAMDPROC) (GLenum mode);

#define glTessellationFactorAMD GLEW_GET_FUN(__glewTessellationFactorAMD)
#define glTessellationModeAMD GLEW_GET_FUN(__glewTessellationModeAMD)

#define GLEW_AMD_vertex_shader_tessellator GLEW_GET_VAR(__GLEW_AMD_vertex_shader_tessellator)

#endif /* GL_AMD_vertex_shader_tessellator */

/* ----------------------- GL_APPLE_aux_depth_stencil ---------------------- */

#ifndef GL_APPLE_aux_depth_stencil
#define GL_APPLE_aux_depth_stencil 1

#define GL_AUX_DEPTH_STENCIL_APPLE 0x8A14

#define GLEW_APPLE_aux_depth_stencil GLEW_GET_VAR(__GLEW_APPLE_aux_depth_stencil)

#endif /* GL_APPLE_aux_depth_stencil */

/* ------------------------ GL_APPLE_client_storage ------------------------ */

#ifndef GL_APPLE_client_storage
#define GL_APPLE_client_storage 1

#define GL_UNPACK_CLIENT_STORAGE_APPLE 0x85B2

#define GLEW_APPLE_client_storage GLEW_GET_VAR(__GLEW_APPLE_client_storage)

#endif /* GL_APPLE_client_storage */

/* ------------------------- GL_APPLE_element_array ------------------------ */

#ifndef GL_APPLE_element_array
#define GL_APPLE_element_array 1

#define GL_ELEMENT_ARRAY_APPLE 0x8A0C
#define GL_ELEMENT_ARRAY_TYPE_APPLE 0x8A0D
#define GL_ELEMENT_ARRAY_POINTER_APPLE 0x8A0E

typedef void (GLAPIENTRY * PFNGLDRAWELEMENTARRAYAPPLEPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (GLAPIENTRY * PFNGLDRAWRANGEELEMENTARRAYAPPLEPROC) (GLenum mode, GLuint start, GLuint end, GLint first, GLsizei count);
typedef void (GLAPIENTRY * PFNGLELEMENTPOINTERAPPLEPROC) (GLenum type, const void* pointer);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTARRAYAPPLEPROC) (GLenum mode, const GLint* first, const GLsizei *count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWRANGEELEMENTARRAYAPPLEPROC) (GLenum mode, GLuint start, GLuint end, const GLint* first, const GLsizei *count, GLsizei primcount);

#define glDrawElementArrayAPPLE GLEW_GET_FUN(__glewDrawElementArrayAPPLE)
#define glDrawRangeElementArrayAPPLE GLEW_GET_FUN(__glewDrawRangeElementArrayAPPLE)
#define glElementPointerAPPLE GLEW_GET_FUN(__glewElementPointerAPPLE)
#define glMultiDrawElementArrayAPPLE GLEW_GET_FUN(__glewMultiDrawElementArrayAPPLE)
#define glMultiDrawRangeElementArrayAPPLE GLEW_GET_FUN(__glewMultiDrawRangeElementArrayAPPLE)

#define GLEW_APPLE_element_array GLEW_GET_VAR(__GLEW_APPLE_element_array)

#endif /* GL_APPLE_element_array */

/* ----------------------------- GL_APPLE_fence ---------------------------- */

#ifndef GL_APPLE_fence
#define GL_APPLE_fence 1

#define GL_DRAW_PIXELS_APPLE 0x8A0A
#define GL_FENCE_APPLE 0x8A0B

typedef void (GLAPIENTRY * PFNGLDELETEFENCESAPPLEPROC) (GLsizei n, const GLuint* fences);
typedef void (GLAPIENTRY * PFNGLFINISHFENCEAPPLEPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLFINISHOBJECTAPPLEPROC) (GLenum object, GLint name);
typedef void (GLAPIENTRY * PFNGLGENFENCESAPPLEPROC) (GLsizei n, GLuint* fences);
typedef GLboolean (GLAPIENTRY * PFNGLISFENCEAPPLEPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLSETFENCEAPPLEPROC) (GLuint fence);
typedef GLboolean (GLAPIENTRY * PFNGLTESTFENCEAPPLEPROC) (GLuint fence);
typedef GLboolean (GLAPIENTRY * PFNGLTESTOBJECTAPPLEPROC) (GLenum object, GLuint name);

#define glDeleteFencesAPPLE GLEW_GET_FUN(__glewDeleteFencesAPPLE)
#define glFinishFenceAPPLE GLEW_GET_FUN(__glewFinishFenceAPPLE)
#define glFinishObjectAPPLE GLEW_GET_FUN(__glewFinishObjectAPPLE)
#define glGenFencesAPPLE GLEW_GET_FUN(__glewGenFencesAPPLE)
#define glIsFenceAPPLE GLEW_GET_FUN(__glewIsFenceAPPLE)
#define glSetFenceAPPLE GLEW_GET_FUN(__glewSetFenceAPPLE)
#define glTestFenceAPPLE GLEW_GET_FUN(__glewTestFenceAPPLE)
#define glTestObjectAPPLE GLEW_GET_FUN(__glewTestObjectAPPLE)

#define GLEW_APPLE_fence GLEW_GET_VAR(__GLEW_APPLE_fence)

#endif /* GL_APPLE_fence */

/* ------------------------- GL_APPLE_float_pixels ------------------------- */

#ifndef GL_APPLE_float_pixels
#define GL_APPLE_float_pixels 1

#define GL_HALF_APPLE 0x140B
#define GL_RGBA_FLOAT32_APPLE 0x8814
#define GL_RGB_FLOAT32_APPLE 0x8815
#define GL_ALPHA_FLOAT32_APPLE 0x8816
#define GL_INTENSITY_FLOAT32_APPLE 0x8817
#define GL_LUMINANCE_FLOAT32_APPLE 0x8818
#define GL_LUMINANCE_ALPHA_FLOAT32_APPLE 0x8819
#define GL_RGBA_FLOAT16_APPLE 0x881A
#define GL_RGB_FLOAT16_APPLE 0x881B
#define GL_ALPHA_FLOAT16_APPLE 0x881C
#define GL_INTENSITY_FLOAT16_APPLE 0x881D
#define GL_LUMINANCE_FLOAT16_APPLE 0x881E
#define GL_LUMINANCE_ALPHA_FLOAT16_APPLE 0x881F
#define GL_COLOR_FLOAT_APPLE 0x8A0F

#define GLEW_APPLE_float_pixels GLEW_GET_VAR(__GLEW_APPLE_float_pixels)

#endif /* GL_APPLE_float_pixels */

/* ---------------------- GL_APPLE_flush_buffer_range ---------------------- */

#ifndef GL_APPLE_flush_buffer_range
#define GL_APPLE_flush_buffer_range 1

#define GL_BUFFER_SERIALIZED_MODIFY_APPLE 0x8A12
#define GL_BUFFER_FLUSHING_UNMAP_APPLE 0x8A13

typedef void (GLAPIENTRY * PFNGLBUFFERPARAMETERIAPPLEPROC) (GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC) (GLenum target, GLintptr offset, GLsizeiptr size);

#define glBufferParameteriAPPLE GLEW_GET_FUN(__glewBufferParameteriAPPLE)
#define glFlushMappedBufferRangeAPPLE GLEW_GET_FUN(__glewFlushMappedBufferRangeAPPLE)

#define GLEW_APPLE_flush_buffer_range GLEW_GET_VAR(__GLEW_APPLE_flush_buffer_range)

#endif /* GL_APPLE_flush_buffer_range */

/* ----------------------- GL_APPLE_object_purgeable ----------------------- */

#ifndef GL_APPLE_object_purgeable
#define GL_APPLE_object_purgeable 1

#define GL_BUFFER_OBJECT_APPLE 0x85B3
#define GL_RELEASED_APPLE 0x8A19
#define GL_VOLATILE_APPLE 0x8A1A
#define GL_RETAINED_APPLE 0x8A1B
#define GL_UNDEFINED_APPLE 0x8A1C
#define GL_PURGEABLE_APPLE 0x8A1D

typedef void (GLAPIENTRY * PFNGLGETOBJECTPARAMETERIVAPPLEPROC) (GLenum objectType, GLuint name, GLenum pname, GLint* params);
typedef GLenum (GLAPIENTRY * PFNGLOBJECTPURGEABLEAPPLEPROC) (GLenum objectType, GLuint name, GLenum option);
typedef GLenum (GLAPIENTRY * PFNGLOBJECTUNPURGEABLEAPPLEPROC) (GLenum objectType, GLuint name, GLenum option);

#define glGetObjectParameterivAPPLE GLEW_GET_FUN(__glewGetObjectParameterivAPPLE)
#define glObjectPurgeableAPPLE GLEW_GET_FUN(__glewObjectPurgeableAPPLE)
#define glObjectUnpurgeableAPPLE GLEW_GET_FUN(__glewObjectUnpurgeableAPPLE)

#define GLEW_APPLE_object_purgeable GLEW_GET_VAR(__GLEW_APPLE_object_purgeable)

#endif /* GL_APPLE_object_purgeable */

/* ------------------------- GL_APPLE_pixel_buffer ------------------------- */

#ifndef GL_APPLE_pixel_buffer
#define GL_APPLE_pixel_buffer 1

#define GL_MIN_PBUFFER_VIEWPORT_DIMS_APPLE 0x8A10

#define GLEW_APPLE_pixel_buffer GLEW_GET_VAR(__GLEW_APPLE_pixel_buffer)

#endif /* GL_APPLE_pixel_buffer */

/* ---------------------------- GL_APPLE_rgb_422 --------------------------- */

#ifndef GL_APPLE_rgb_422
#define GL_APPLE_rgb_422 1

#define GL_UNSIGNED_SHORT_8_8_APPLE 0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_APPLE 0x85BB
#define GL_RGB_422_APPLE 0x8A1F

#define GLEW_APPLE_rgb_422 GLEW_GET_VAR(__GLEW_APPLE_rgb_422)

#endif /* GL_APPLE_rgb_422 */

/* --------------------------- GL_APPLE_row_bytes -------------------------- */

#ifndef GL_APPLE_row_bytes
#define GL_APPLE_row_bytes 1

#define GL_PACK_ROW_BYTES_APPLE 0x8A15
#define GL_UNPACK_ROW_BYTES_APPLE 0x8A16

#define GLEW_APPLE_row_bytes GLEW_GET_VAR(__GLEW_APPLE_row_bytes)

#endif /* GL_APPLE_row_bytes */

/* ------------------------ GL_APPLE_specular_vector ----------------------- */

#ifndef GL_APPLE_specular_vector
#define GL_APPLE_specular_vector 1

#define GL_LIGHT_MODEL_SPECULAR_VECTOR_APPLE 0x85B0

#define GLEW_APPLE_specular_vector GLEW_GET_VAR(__GLEW_APPLE_specular_vector)

#endif /* GL_APPLE_specular_vector */

/* ------------------------- GL_APPLE_texture_range ------------------------ */

#ifndef GL_APPLE_texture_range
#define GL_APPLE_texture_range 1

#define GL_TEXTURE_RANGE_LENGTH_APPLE 0x85B7
#define GL_TEXTURE_RANGE_POINTER_APPLE 0x85B8
#define GL_TEXTURE_STORAGE_HINT_APPLE 0x85BC
#define GL_STORAGE_PRIVATE_APPLE 0x85BD
#define GL_STORAGE_CACHED_APPLE 0x85BE
#define GL_STORAGE_SHARED_APPLE 0x85BF

typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERPOINTERVAPPLEPROC) (GLenum target, GLenum pname, GLvoid **params);
typedef void (GLAPIENTRY * PFNGLTEXTURERANGEAPPLEPROC) (GLenum target, GLsizei length, GLvoid *pointer);

#define glGetTexParameterPointervAPPLE GLEW_GET_FUN(__glewGetTexParameterPointervAPPLE)
#define glTextureRangeAPPLE GLEW_GET_FUN(__glewTextureRangeAPPLE)

#define GLEW_APPLE_texture_range GLEW_GET_VAR(__GLEW_APPLE_texture_range)

#endif /* GL_APPLE_texture_range */

/* ------------------------ GL_APPLE_transform_hint ------------------------ */

#ifndef GL_APPLE_transform_hint
#define GL_APPLE_transform_hint 1

#define GL_TRANSFORM_HINT_APPLE 0x85B1

#define GLEW_APPLE_transform_hint GLEW_GET_VAR(__GLEW_APPLE_transform_hint)

#endif /* GL_APPLE_transform_hint */

/* ---------------------- GL_APPLE_vertex_array_object --------------------- */

#ifndef GL_APPLE_vertex_array_object
#define GL_APPLE_vertex_array_object 1

#define GL_VERTEX_ARRAY_BINDING_APPLE 0x85B5

typedef void (GLAPIENTRY * PFNGLBINDVERTEXARRAYAPPLEPROC) (GLuint array);
typedef void (GLAPIENTRY * PFNGLDELETEVERTEXARRAYSAPPLEPROC) (GLsizei n, const GLuint* arrays);
typedef void (GLAPIENTRY * PFNGLGENVERTEXARRAYSAPPLEPROC) (GLsizei n, const GLuint* arrays);
typedef GLboolean (GLAPIENTRY * PFNGLISVERTEXARRAYAPPLEPROC) (GLuint array);

#define glBindVertexArrayAPPLE GLEW_GET_FUN(__glewBindVertexArrayAPPLE)
#define glDeleteVertexArraysAPPLE GLEW_GET_FUN(__glewDeleteVertexArraysAPPLE)
#define glGenVertexArraysAPPLE GLEW_GET_FUN(__glewGenVertexArraysAPPLE)
#define glIsVertexArrayAPPLE GLEW_GET_FUN(__glewIsVertexArrayAPPLE)

#define GLEW_APPLE_vertex_array_object GLEW_GET_VAR(__GLEW_APPLE_vertex_array_object)

#endif /* GL_APPLE_vertex_array_object */

/* ---------------------- GL_APPLE_vertex_array_range ---------------------- */

#ifndef GL_APPLE_vertex_array_range
#define GL_APPLE_vertex_array_range 1

#define GL_VERTEX_ARRAY_RANGE_APPLE 0x851D
#define GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE 0x851E
#define GL_VERTEX_ARRAY_STORAGE_HINT_APPLE 0x851F
#define GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_APPLE 0x8520
#define GL_VERTEX_ARRAY_RANGE_POINTER_APPLE 0x8521
#define GL_STORAGE_CLIENT_APPLE 0x85B4
#define GL_STORAGE_CACHED_APPLE 0x85BE
#define GL_STORAGE_SHARED_APPLE 0x85BF

typedef void (GLAPIENTRY * PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC) (GLsizei length, void* pointer);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYPARAMETERIAPPLEPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYRANGEAPPLEPROC) (GLsizei length, void* pointer);

#define glFlushVertexArrayRangeAPPLE GLEW_GET_FUN(__glewFlushVertexArrayRangeAPPLE)
#define glVertexArrayParameteriAPPLE GLEW_GET_FUN(__glewVertexArrayParameteriAPPLE)
#define glVertexArrayRangeAPPLE GLEW_GET_FUN(__glewVertexArrayRangeAPPLE)

#define GLEW_APPLE_vertex_array_range GLEW_GET_VAR(__GLEW_APPLE_vertex_array_range)

#endif /* GL_APPLE_vertex_array_range */

/* ------------------- GL_APPLE_vertex_program_evaluators ------------------ */

#ifndef GL_APPLE_vertex_program_evaluators
#define GL_APPLE_vertex_program_evaluators 1

#define GL_VERTEX_ATTRIB_MAP1_APPLE 0x8A00
#define GL_VERTEX_ATTRIB_MAP2_APPLE 0x8A01
#define GL_VERTEX_ATTRIB_MAP1_SIZE_APPLE 0x8A02
#define GL_VERTEX_ATTRIB_MAP1_COEFF_APPLE 0x8A03
#define GL_VERTEX_ATTRIB_MAP1_ORDER_APPLE 0x8A04
#define GL_VERTEX_ATTRIB_MAP1_DOMAIN_APPLE 0x8A05
#define GL_VERTEX_ATTRIB_MAP2_SIZE_APPLE 0x8A06
#define GL_VERTEX_ATTRIB_MAP2_COEFF_APPLE 0x8A07
#define GL_VERTEX_ATTRIB_MAP2_ORDER_APPLE 0x8A08
#define GL_VERTEX_ATTRIB_MAP2_DOMAIN_APPLE 0x8A09

typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBAPPLEPROC) (GLuint index, GLenum pname);
typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBAPPLEPROC) (GLuint index, GLenum pname);
typedef GLboolean (GLAPIENTRY * PFNGLISVERTEXATTRIBENABLEDAPPLEPROC) (GLuint index, GLenum pname);
typedef void (GLAPIENTRY * PFNGLMAPVERTEXATTRIB1DAPPLEPROC) (GLuint index, GLuint size, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble* points);
typedef void (GLAPIENTRY * PFNGLMAPVERTEXATTRIB1FAPPLEPROC) (GLuint index, GLuint size, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat* points);
typedef void (GLAPIENTRY * PFNGLMAPVERTEXATTRIB2DAPPLEPROC) (GLuint index, GLuint size, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble* points);
typedef void (GLAPIENTRY * PFNGLMAPVERTEXATTRIB2FAPPLEPROC) (GLuint index, GLuint size, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat* points);

#define glDisableVertexAttribAPPLE GLEW_GET_FUN(__glewDisableVertexAttribAPPLE)
#define glEnableVertexAttribAPPLE GLEW_GET_FUN(__glewEnableVertexAttribAPPLE)
#define glIsVertexAttribEnabledAPPLE GLEW_GET_FUN(__glewIsVertexAttribEnabledAPPLE)
#define glMapVertexAttrib1dAPPLE GLEW_GET_FUN(__glewMapVertexAttrib1dAPPLE)
#define glMapVertexAttrib1fAPPLE GLEW_GET_FUN(__glewMapVertexAttrib1fAPPLE)
#define glMapVertexAttrib2dAPPLE GLEW_GET_FUN(__glewMapVertexAttrib2dAPPLE)
#define glMapVertexAttrib2fAPPLE GLEW_GET_FUN(__glewMapVertexAttrib2fAPPLE)

#define GLEW_APPLE_vertex_program_evaluators GLEW_GET_VAR(__GLEW_APPLE_vertex_program_evaluators)

#endif /* GL_APPLE_vertex_program_evaluators */

/* --------------------------- GL_APPLE_ycbcr_422 -------------------------- */

#ifndef GL_APPLE_ycbcr_422
#define GL_APPLE_ycbcr_422 1

#define GL_YCBCR_422_APPLE 0x85B9
#define GL_UNSIGNED_SHORT_8_8_APPLE 0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_APPLE 0x85BB

#define GLEW_APPLE_ycbcr_422 GLEW_GET_VAR(__GLEW_APPLE_ycbcr_422)

#endif /* GL_APPLE_ycbcr_422 */

/* ------------------------ GL_ARB_ES2_compatibility ----------------------- */

#ifndef GL_ARB_ES2_compatibility
#define GL_ARB_ES2_compatibility 1

#define GL_FIXED 0x140C
#define GL_IMPLEMENTATION_COLOR_READ_TYPE 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#define GL_LOW_FLOAT 0x8DF0
#define GL_MEDIUM_FLOAT 0x8DF1
#define GL_HIGH_FLOAT 0x8DF2
#define GL_LOW_INT 0x8DF3
#define GL_MEDIUM_INT 0x8DF4
#define GL_HIGH_INT 0x8DF5
#define GL_SHADER_BINARY_FORMATS 0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS 0x8DF9
#define GL_SHADER_COMPILER 0x8DFA
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#define GL_MAX_VARYING_VECTORS 0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD

typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFPROC) (GLclampf d);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFPROC) (GLclampf n, GLclampf f);
typedef void (GLAPIENTRY * PFNGLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint* range, GLint *precision);
typedef void (GLAPIENTRY * PFNGLRELEASESHADERCOMPILERPROC) (void);
typedef void (GLAPIENTRY * PFNGLSHADERBINARYPROC) (GLsizei count, const GLuint* shaders, GLenum binaryformat, const GLvoid*binary, GLsizei length);

#define glClearDepthf GLEW_GET_FUN(__glewClearDepthf)
#define glDepthRangef GLEW_GET_FUN(__glewDepthRangef)
#define glGetShaderPrecisionFormat GLEW_GET_FUN(__glewGetShaderPrecisionFormat)
#define glReleaseShaderCompiler GLEW_GET_FUN(__glewReleaseShaderCompiler)
#define glShaderBinary GLEW_GET_FUN(__glewShaderBinary)

#define GLEW_ARB_ES2_compatibility GLEW_GET_VAR(__GLEW_ARB_ES2_compatibility)

#endif /* GL_ARB_ES2_compatibility */

/* ----------------------- GL_ARB_blend_func_extended ---------------------- */

#ifndef GL_ARB_blend_func_extended
#define GL_ARB_blend_func_extended 1

#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC

typedef void (GLAPIENTRY * PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) (GLuint program, GLuint colorNumber, GLuint index, const char * name);
typedef GLint (GLAPIENTRY * PFNGLGETFRAGDATAINDEXPROC) (GLuint program, const char * name);

#define glBindFragDataLocationIndexed GLEW_GET_FUN(__glewBindFragDataLocationIndexed)
#define glGetFragDataIndex GLEW_GET_FUN(__glewGetFragDataIndex)

#define GLEW_ARB_blend_func_extended GLEW_GET_VAR(__GLEW_ARB_blend_func_extended)

#endif /* GL_ARB_blend_func_extended */

/* ---------------------------- GL_ARB_cl_event ---------------------------- */

#ifndef GL_ARB_cl_event
#define GL_ARB_cl_event 1

#define GL_SYNC_CL_EVENT_ARB 0x8240
#define GL_SYNC_CL_EVENT_COMPLETE_ARB 0x8241

typedef GLsync (GLAPIENTRY * PFNGLCREATESYNCFROMCLEVENTARBPROC) (cl_context context, cl_event event, GLbitfield flags);

#define glCreateSyncFromCLeventARB GLEW_GET_FUN(__glewCreateSyncFromCLeventARB)

#define GLEW_ARB_cl_event GLEW_GET_VAR(__GLEW_ARB_cl_event)

#endif /* GL_ARB_cl_event */

/* ----------------------- GL_ARB_color_buffer_float ----------------------- */

#ifndef GL_ARB_color_buffer_float
#define GL_ARB_color_buffer_float 1

#define GL_RGBA_FLOAT_MODE_ARB 0x8820
#define GL_CLAMP_VERTEX_COLOR_ARB 0x891A
#define GL_CLAMP_FRAGMENT_COLOR_ARB 0x891B
#define GL_CLAMP_READ_COLOR_ARB 0x891C
#define GL_FIXED_ONLY_ARB 0x891D

typedef void (GLAPIENTRY * PFNGLCLAMPCOLORARBPROC) (GLenum target, GLenum clamp);

#define glClampColorARB GLEW_GET_FUN(__glewClampColorARB)

#define GLEW_ARB_color_buffer_float GLEW_GET_VAR(__GLEW_ARB_color_buffer_float)

#endif /* GL_ARB_color_buffer_float */

/* -------------------------- GL_ARB_compatibility ------------------------- */

#ifndef GL_ARB_compatibility
#define GL_ARB_compatibility 1

#define GLEW_ARB_compatibility GLEW_GET_VAR(__GLEW_ARB_compatibility)

#endif /* GL_ARB_compatibility */

/* --------------------------- GL_ARB_copy_buffer -------------------------- */

#ifndef GL_ARB_copy_buffer
#define GL_ARB_copy_buffer 1

#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37

typedef void (GLAPIENTRY * PFNGLCOPYBUFFERSUBDATAPROC) (GLenum readtarget, GLenum writetarget, GLintptr readoffset, GLintptr writeoffset, GLsizeiptr size);

#define glCopyBufferSubData GLEW_GET_FUN(__glewCopyBufferSubData)

#define GLEW_ARB_copy_buffer GLEW_GET_VAR(__GLEW_ARB_copy_buffer)

#endif /* GL_ARB_copy_buffer */

/* -------------------------- GL_ARB_debug_output -------------------------- */

#ifndef GL_ARB_debug_output
#define GL_ARB_debug_output 1

#define GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB 0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_ARB 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION_ARB 0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM_ARB 0x8245
#define GL_DEBUG_SOURCE_API_ARB 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_ARB 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_ARB 0x8249
#define GL_DEBUG_SOURCE_APPLICATION_ARB 0x824A
#define GL_DEBUG_SOURCE_OTHER_ARB 0x824B
#define GL_DEBUG_TYPE_ERROR_ARB 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB 0x824E
#define GL_DEBUG_TYPE_PORTABILITY_ARB 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_ARB 0x8250
#define GL_DEBUG_TYPE_OTHER_ARB 0x8251
#define GL_MAX_DEBUG_MESSAGE_LENGTH_ARB 0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES_ARB 0x9144
#define GL_DEBUG_LOGGED_MESSAGES_ARB 0x9145
#define GL_DEBUG_SEVERITY_HIGH_ARB 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_ARB 0x9147
#define GL_DEBUG_SEVERITY_LOW_ARB 0x9148

typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECALLBACKARBPROC) (GLDEBUGPROCARB callback, void* userParam);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECONTROLARBPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGEINSERTARBPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* buf);
typedef GLuint (GLAPIENTRY * PFNGLGETDEBUGMESSAGELOGARBPROC) (GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, char* messageLog);

#define glDebugMessageCallbackARB GLEW_GET_FUN(__glewDebugMessageCallbackARB)
#define glDebugMessageControlARB GLEW_GET_FUN(__glewDebugMessageControlARB)
#define glDebugMessageInsertARB GLEW_GET_FUN(__glewDebugMessageInsertARB)
#define glGetDebugMessageLogARB GLEW_GET_FUN(__glewGetDebugMessageLogARB)

#define GLEW_ARB_debug_output GLEW_GET_VAR(__GLEW_ARB_debug_output)

#endif /* GL_ARB_debug_output */

/* ----------------------- GL_ARB_depth_buffer_float ----------------------- */

#ifndef GL_ARB_depth_buffer_float
#define GL_ARB_depth_buffer_float 1

#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD

#define GLEW_ARB_depth_buffer_float GLEW_GET_VAR(__GLEW_ARB_depth_buffer_float)

#endif /* GL_ARB_depth_buffer_float */

/* --------------------------- GL_ARB_depth_clamp -------------------------- */

#ifndef GL_ARB_depth_clamp
#define GL_ARB_depth_clamp 1

#define GL_DEPTH_CLAMP 0x864F

#define GLEW_ARB_depth_clamp GLEW_GET_VAR(__GLEW_ARB_depth_clamp)

#endif /* GL_ARB_depth_clamp */

/* -------------------------- GL_ARB_depth_texture ------------------------- */

#ifndef GL_ARB_depth_texture
#define GL_ARB_depth_texture 1

#define GL_DEPTH_COMPONENT16_ARB 0x81A5
#define GL_DEPTH_COMPONENT24_ARB 0x81A6
#define GL_DEPTH_COMPONENT32_ARB 0x81A7
#define GL_TEXTURE_DEPTH_SIZE_ARB 0x884A
#define GL_DEPTH_TEXTURE_MODE_ARB 0x884B

#define GLEW_ARB_depth_texture GLEW_GET_VAR(__GLEW_ARB_depth_texture)

#endif /* GL_ARB_depth_texture */

/* -------------------------- GL_ARB_draw_buffers -------------------------- */

#ifndef GL_ARB_draw_buffers
#define GL_ARB_draw_buffers 1

#define GL_MAX_DRAW_BUFFERS_ARB 0x8824
#define GL_DRAW_BUFFER0_ARB 0x8825
#define GL_DRAW_BUFFER1_ARB 0x8826
#define GL_DRAW_BUFFER2_ARB 0x8827
#define GL_DRAW_BUFFER3_ARB 0x8828
#define GL_DRAW_BUFFER4_ARB 0x8829
#define GL_DRAW_BUFFER5_ARB 0x882A
#define GL_DRAW_BUFFER6_ARB 0x882B
#define GL_DRAW_BUFFER7_ARB 0x882C
#define GL_DRAW_BUFFER8_ARB 0x882D
#define GL_DRAW_BUFFER9_ARB 0x882E
#define GL_DRAW_BUFFER10_ARB 0x882F
#define GL_DRAW_BUFFER11_ARB 0x8830
#define GL_DRAW_BUFFER12_ARB 0x8831
#define GL_DRAW_BUFFER13_ARB 0x8832
#define GL_DRAW_BUFFER14_ARB 0x8833
#define GL_DRAW_BUFFER15_ARB 0x8834

typedef void (GLAPIENTRY * PFNGLDRAWBUFFERSARBPROC) (GLsizei n, const GLenum* bufs);

#define glDrawBuffersARB GLEW_GET_FUN(__glewDrawBuffersARB)

#define GLEW_ARB_draw_buffers GLEW_GET_VAR(__GLEW_ARB_draw_buffers)

#endif /* GL_ARB_draw_buffers */

/* ----------------------- GL_ARB_draw_buffers_blend ----------------------- */

#ifndef GL_ARB_draw_buffers_blend
#define GL_ARB_draw_buffers_blend 1

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEIARBPROC) (GLuint buf, GLenum modeRGB, GLenum modeAlpha);
typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONIARBPROC) (GLuint buf, GLenum mode);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEIARBPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (GLAPIENTRY * PFNGLBLENDFUNCIARBPROC) (GLuint buf, GLenum src, GLenum dst);

#define glBlendEquationSeparateiARB GLEW_GET_FUN(__glewBlendEquationSeparateiARB)
#define glBlendEquationiARB GLEW_GET_FUN(__glewBlendEquationiARB)
#define glBlendFuncSeparateiARB GLEW_GET_FUN(__glewBlendFuncSeparateiARB)
#define glBlendFunciARB GLEW_GET_FUN(__glewBlendFunciARB)

#define GLEW_ARB_draw_buffers_blend GLEW_GET_VAR(__GLEW_ARB_draw_buffers_blend)

#endif /* GL_ARB_draw_buffers_blend */

/* -------------------- GL_ARB_draw_elements_base_vertex ------------------- */

#ifndef GL_ARB_draw_elements_base_vertex
#define GL_ARB_draw_elements_base_vertex 1

typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, void* indices, GLint basevertex);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount, GLint basevertex);
typedef void (GLAPIENTRY * PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, void* indices, GLint basevertex);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC) (GLenum mode, GLsizei* count, GLenum type, GLvoid**indices, GLsizei primcount, GLint *basevertex);

#define glDrawElementsBaseVertex GLEW_GET_FUN(__glewDrawElementsBaseVertex)
#define glDrawElementsInstancedBaseVertex GLEW_GET_FUN(__glewDrawElementsInstancedBaseVertex)
#define glDrawRangeElementsBaseVertex GLEW_GET_FUN(__glewDrawRangeElementsBaseVertex)
#define glMultiDrawElementsBaseVertex GLEW_GET_FUN(__glewMultiDrawElementsBaseVertex)

#define GLEW_ARB_draw_elements_base_vertex GLEW_GET_VAR(__GLEW_ARB_draw_elements_base_vertex)

#endif /* GL_ARB_draw_elements_base_vertex */

/* -------------------------- GL_ARB_draw_indirect ------------------------- */

#ifndef GL_ARB_draw_indirect
#define GL_ARB_draw_indirect 1

#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#define GL_DRAW_INDIRECT_BUFFER_BINDING 0x8F43

typedef void (GLAPIENTRY * PFNGLDRAWARRAYSINDIRECTPROC) (GLenum mode, const void* indirect);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINDIRECTPROC) (GLenum mode, GLenum type, const void* indirect);

#define glDrawArraysIndirect GLEW_GET_FUN(__glewDrawArraysIndirect)
#define glDrawElementsIndirect GLEW_GET_FUN(__glewDrawElementsIndirect)

#define GLEW_ARB_draw_indirect GLEW_GET_VAR(__GLEW_ARB_draw_indirect)

#endif /* GL_ARB_draw_indirect */

/* ------------------------- GL_ARB_draw_instanced ------------------------- */

#ifndef GL_ARB_draw_instanced
#define GL_ARB_draw_instanced 1

typedef void (GLAPIENTRY * PFNGLDRAWARRAYSINSTANCEDARBPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINSTANCEDARBPROC) (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount);

#define glDrawArraysInstancedARB GLEW_GET_FUN(__glewDrawArraysInstancedARB)
#define glDrawElementsInstancedARB GLEW_GET_FUN(__glewDrawElementsInstancedARB)

#define GLEW_ARB_draw_instanced GLEW_GET_VAR(__GLEW_ARB_draw_instanced)

#endif /* GL_ARB_draw_instanced */

/* -------------------- GL_ARB_explicit_attrib_location -------------------- */

#ifndef GL_ARB_explicit_attrib_location
#define GL_ARB_explicit_attrib_location 1

#define GLEW_ARB_explicit_attrib_location GLEW_GET_VAR(__GLEW_ARB_explicit_attrib_location)

#endif /* GL_ARB_explicit_attrib_location */

/* ------------------- GL_ARB_fragment_coord_conventions ------------------- */

#ifndef GL_ARB_fragment_coord_conventions
#define GL_ARB_fragment_coord_conventions 1

#define GLEW_ARB_fragment_coord_conventions GLEW_GET_VAR(__GLEW_ARB_fragment_coord_conventions)

#endif /* GL_ARB_fragment_coord_conventions */

/* ------------------------ GL_ARB_fragment_program ------------------------ */

#ifndef GL_ARB_fragment_program
#define GL_ARB_fragment_program 1

#define GL_FRAGMENT_PROGRAM_ARB 0x8804
#define GL_PROGRAM_ALU_INSTRUCTIONS_ARB 0x8805
#define GL_PROGRAM_TEX_INSTRUCTIONS_ARB 0x8806
#define GL_PROGRAM_TEX_INDIRECTIONS_ARB 0x8807
#define GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x8808
#define GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x8809
#define GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x880A
#define GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB 0x880B
#define GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB 0x880C
#define GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB 0x880D
#define GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x880E
#define GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x880F
#define GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x8810
#define GL_MAX_TEXTURE_COORDS_ARB 0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB 0x8872

#define GLEW_ARB_fragment_program GLEW_GET_VAR(__GLEW_ARB_fragment_program)

#endif /* GL_ARB_fragment_program */

/* --------------------- GL_ARB_fragment_program_shadow -------------------- */

#ifndef GL_ARB_fragment_program_shadow
#define GL_ARB_fragment_program_shadow 1

#define GLEW_ARB_fragment_program_shadow GLEW_GET_VAR(__GLEW_ARB_fragment_program_shadow)

#endif /* GL_ARB_fragment_program_shadow */

/* ------------------------- GL_ARB_fragment_shader ------------------------ */

#ifndef GL_ARB_fragment_shader
#define GL_ARB_fragment_shader 1

#define GL_FRAGMENT_SHADER_ARB 0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB 0x8B49
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB 0x8B8B

#define GLEW_ARB_fragment_shader GLEW_GET_VAR(__GLEW_ARB_fragment_shader)

#endif /* GL_ARB_fragment_shader */

/* ----------------------- GL_ARB_framebuffer_object ----------------------- */

#ifndef GL_ARB_framebuffer_object
#define GL_ARB_framebuffer_object 1

#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING 0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE 0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE 0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE 0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE 0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE 0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE 0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
#define GL_FRAMEBUFFER_DEFAULT 0x8218
#define GL_FRAMEBUFFER_UNDEFINED 0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_INDEX 0x8222
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_TEXTURE_STENCIL_SIZE 0x88F1
#define GL_UNSIGNED_NORMALIZED 0x8C17
#define GL_SRGB 0x8C40
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_RENDERBUFFER_BINDING 0x8CA7
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_RENDERBUFFER_SAMPLES 0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_COLOR_ATTACHMENT3 0x8CE3
#define GL_COLOR_ATTACHMENT4 0x8CE4
#define GL_COLOR_ATTACHMENT5 0x8CE5
#define GL_COLOR_ATTACHMENT6 0x8CE6
#define GL_COLOR_ATTACHMENT7 0x8CE7
#define GL_COLOR_ATTACHMENT8 0x8CE8
#define GL_COLOR_ATTACHMENT9 0x8CE9
#define GL_COLOR_ATTACHMENT10 0x8CEA
#define GL_COLOR_ATTACHMENT11 0x8CEB
#define GL_COLOR_ATTACHMENT12 0x8CEC
#define GL_COLOR_ATTACHMENT13 0x8CED
#define GL_COLOR_ATTACHMENT14 0x8CEE
#define GL_COLOR_ATTACHMENT15 0x8CEF
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_RENDERBUFFER_WIDTH 0x8D42
#define GL_RENDERBUFFER_HEIGHT 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT 0x8D44
#define GL_STENCIL_INDEX1 0x8D46
#define GL_STENCIL_INDEX4 0x8D47
#define GL_STENCIL_INDEX8 0x8D48
#define GL_STENCIL_INDEX16 0x8D49
#define GL_RENDERBUFFER_RED_SIZE 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE 0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE 0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE 0x8D55
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#define GL_MAX_SAMPLES 0x8D57

typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (GLAPIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef GLenum (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE1DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE3DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target,GLenum attachment, GLuint texture,GLint level,GLint layer);
typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef GLboolean (GLAPIENTRY * PFNGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

#define glBindFramebuffer GLEW_GET_FUN(__glewBindFramebuffer)
#define glBindRenderbuffer GLEW_GET_FUN(__glewBindRenderbuffer)
#define glBlitFramebuffer GLEW_GET_FUN(__glewBlitFramebuffer)
#define glCheckFramebufferStatus GLEW_GET_FUN(__glewCheckFramebufferStatus)
#define glDeleteFramebuffers GLEW_GET_FUN(__glewDeleteFramebuffers)
#define glDeleteRenderbuffers GLEW_GET_FUN(__glewDeleteRenderbuffers)
#define glFramebufferRenderbuffer GLEW_GET_FUN(__glewFramebufferRenderbuffer)
#define glFramebufferTexture1D GLEW_GET_FUN(__glewFramebufferTexture1D)
#define glFramebufferTexture2D GLEW_GET_FUN(__glewFramebufferTexture2D)
#define glFramebufferTexture3D GLEW_GET_FUN(__glewFramebufferTexture3D)
#define glFramebufferTextureLayer GLEW_GET_FUN(__glewFramebufferTextureLayer)
#define glGenFramebuffers GLEW_GET_FUN(__glewGenFramebuffers)
#define glGenRenderbuffers GLEW_GET_FUN(__glewGenRenderbuffers)
#define glGenerateMipmap GLEW_GET_FUN(__glewGenerateMipmap)
#define glGetFramebufferAttachmentParameteriv GLEW_GET_FUN(__glewGetFramebufferAttachmentParameteriv)
#define glGetRenderbufferParameteriv GLEW_GET_FUN(__glewGetRenderbufferParameteriv)
#define glIsFramebuffer GLEW_GET_FUN(__glewIsFramebuffer)
#define glIsRenderbuffer GLEW_GET_FUN(__glewIsRenderbuffer)
#define glRenderbufferStorage GLEW_GET_FUN(__glewRenderbufferStorage)
#define glRenderbufferStorageMultisample GLEW_GET_FUN(__glewRenderbufferStorageMultisample)

#define GLEW_ARB_framebuffer_object GLEW_GET_VAR(__GLEW_ARB_framebuffer_object)

#endif /* GL_ARB_framebuffer_object */

/* ------------------------ GL_ARB_framebuffer_sRGB ------------------------ */

#ifndef GL_ARB_framebuffer_sRGB
#define GL_ARB_framebuffer_sRGB 1

#define GL_FRAMEBUFFER_SRGB 0x8DB9

#define GLEW_ARB_framebuffer_sRGB GLEW_GET_VAR(__GLEW_ARB_framebuffer_sRGB)

#endif /* GL_ARB_framebuffer_sRGB */

/* ------------------------ GL_ARB_geometry_shader4 ------------------------ */

#ifndef GL_ARB_geometry_shader4
#define GL_ARB_geometry_shader4 1

#define GL_LINES_ADJACENCY_ARB 0xA
#define GL_LINE_STRIP_ADJACENCY_ARB 0xB
#define GL_TRIANGLES_ADJACENCY_ARB 0xC
#define GL_TRIANGLE_STRIP_ADJACENCY_ARB 0xD
#define GL_PROGRAM_POINT_SIZE_ARB 0x8642
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_ARB 0x8C29
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER 0x8CD4
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED_ARB 0x8DA7
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_ARB 0x8DA8
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_ARB 0x8DA9
#define GL_GEOMETRY_SHADER_ARB 0x8DD9
#define GL_GEOMETRY_VERTICES_OUT_ARB 0x8DDA
#define GL_GEOMETRY_INPUT_TYPE_ARB 0x8DDB
#define GL_GEOMETRY_OUTPUT_TYPE_ARB 0x8DDC
#define GL_MAX_GEOMETRY_VARYING_COMPONENTS_ARB 0x8DDD
#define GL_MAX_VERTEX_VARYING_COMPONENTS_ARB 0x8DDE
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB 0x8DDF
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES_ARB 0x8DE0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB 0x8DE1

typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTUREARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTUREFACEARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURELAYERARBPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERIARBPROC) (GLuint program, GLenum pname, GLint value);

#define glFramebufferTextureARB GLEW_GET_FUN(__glewFramebufferTextureARB)
#define glFramebufferTextureFaceARB GLEW_GET_FUN(__glewFramebufferTextureFaceARB)
#define glFramebufferTextureLayerARB GLEW_GET_FUN(__glewFramebufferTextureLayerARB)
#define glProgramParameteriARB GLEW_GET_FUN(__glewProgramParameteriARB)

#define GLEW_ARB_geometry_shader4 GLEW_GET_VAR(__GLEW_ARB_geometry_shader4)

#endif /* GL_ARB_geometry_shader4 */

/* ----------------------- GL_ARB_get_program_binary ----------------------- */

#ifndef GL_ARB_get_program_binary
#define GL_ARB_get_program_binary 1

#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#define GL_PROGRAM_BINARY_LENGTH 0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#define GL_PROGRAM_BINARY_FORMATS 0x87FF

typedef void (GLAPIENTRY * PFNGLGETPROGRAMBINARYPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLenum *binaryFormat, GLvoid*binary);
typedef void (GLAPIENTRY * PFNGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);

#define glGetProgramBinary GLEW_GET_FUN(__glewGetProgramBinary)
#define glProgramBinary GLEW_GET_FUN(__glewProgramBinary)
#define glProgramParameteri GLEW_GET_FUN(__glewProgramParameteri)

#define GLEW_ARB_get_program_binary GLEW_GET_VAR(__GLEW_ARB_get_program_binary)

#endif /* GL_ARB_get_program_binary */

/* --------------------------- GL_ARB_gpu_shader5 -------------------------- */

#ifndef GL_ARB_gpu_shader5
#define GL_ARB_gpu_shader5 1

#define GL_GEOMETRY_SHADER_INVOCATIONS 0x887F
#define GL_MAX_GEOMETRY_SHADER_INVOCATIONS 0x8E5A
#define GL_MIN_FRAGMENT_INTERPOLATION_OFFSET 0x8E5B
#define GL_MAX_FRAGMENT_INTERPOLATION_OFFSET 0x8E5C
#define GL_FRAGMENT_INTERPOLATION_OFFSET_BITS 0x8E5D
#define GL_MAX_VERTEX_STREAMS 0x8E71

#define GLEW_ARB_gpu_shader5 GLEW_GET_VAR(__GLEW_ARB_gpu_shader5)

#endif /* GL_ARB_gpu_shader5 */

/* ------------------------- GL_ARB_gpu_shader_fp64 ------------------------ */

#ifndef GL_ARB_gpu_shader_fp64
#define GL_ARB_gpu_shader_fp64 1

#define GL_DOUBLE_MAT2 0x8F46
#define GL_DOUBLE_MAT3 0x8F47
#define GL_DOUBLE_MAT4 0x8F48
#define GL_DOUBLE_VEC2 0x8FFC
#define GL_DOUBLE_VEC3 0x8FFD
#define GL_DOUBLE_VEC4 0x8FFE

typedef void (GLAPIENTRY * PFNGLGETUNIFORMDVPROC) (GLuint program, GLint location, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1DEXTPROC) (GLuint program, GLint location, GLdouble x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4DEXTPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4DVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM1DPROC) (GLint location, GLdouble x);
typedef void (GLAPIENTRY * PFNGLUNIFORM1DVPROC) (GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2DPROC) (GLint location, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLUNIFORM2DVPROC) (GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLUNIFORM3DVPROC) (GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4DPROC) (GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLUNIFORM4DVPROC) (GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3X4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4X2DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4X3DVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);

#define glGetUniformdv GLEW_GET_FUN(__glewGetUniformdv)
#define glProgramUniform1dEXT GLEW_GET_FUN(__glewProgramUniform1dEXT)
#define glProgramUniform1dvEXT GLEW_GET_FUN(__glewProgramUniform1dvEXT)
#define glProgramUniform2dEXT GLEW_GET_FUN(__glewProgramUniform2dEXT)
#define glProgramUniform2dvEXT GLEW_GET_FUN(__glewProgramUniform2dvEXT)
#define glProgramUniform3dEXT GLEW_GET_FUN(__glewProgramUniform3dEXT)
#define glProgramUniform3dvEXT GLEW_GET_FUN(__glewProgramUniform3dvEXT)
#define glProgramUniform4dEXT GLEW_GET_FUN(__glewProgramUniform4dEXT)
#define glProgramUniform4dvEXT GLEW_GET_FUN(__glewProgramUniform4dvEXT)
#define glProgramUniformMatrix2dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2dvEXT)
#define glProgramUniformMatrix2x3dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2x3dvEXT)
#define glProgramUniformMatrix2x4dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2x4dvEXT)
#define glProgramUniformMatrix3dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3dvEXT)
#define glProgramUniformMatrix3x2dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3x2dvEXT)
#define glProgramUniformMatrix3x4dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3x4dvEXT)
#define glProgramUniformMatrix4dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4dvEXT)
#define glProgramUniformMatrix4x2dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4x2dvEXT)
#define glProgramUniformMatrix4x3dvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4x3dvEXT)
#define glUniform1d GLEW_GET_FUN(__glewUniform1d)
#define glUniform1dv GLEW_GET_FUN(__glewUniform1dv)
#define glUniform2d GLEW_GET_FUN(__glewUniform2d)
#define glUniform2dv GLEW_GET_FUN(__glewUniform2dv)
#define glUniform3d GLEW_GET_FUN(__glewUniform3d)
#define glUniform3dv GLEW_GET_FUN(__glewUniform3dv)
#define glUniform4d GLEW_GET_FUN(__glewUniform4d)
#define glUniform4dv GLEW_GET_FUN(__glewUniform4dv)
#define glUniformMatrix2dv GLEW_GET_FUN(__glewUniformMatrix2dv)
#define glUniformMatrix2x3dv GLEW_GET_FUN(__glewUniformMatrix2x3dv)
#define glUniformMatrix2x4dv GLEW_GET_FUN(__glewUniformMatrix2x4dv)
#define glUniformMatrix3dv GLEW_GET_FUN(__glewUniformMatrix3dv)
#define glUniformMatrix3x2dv GLEW_GET_FUN(__glewUniformMatrix3x2dv)
#define glUniformMatrix3x4dv GLEW_GET_FUN(__glewUniformMatrix3x4dv)
#define glUniformMatrix4dv GLEW_GET_FUN(__glewUniformMatrix4dv)
#define glUniformMatrix4x2dv GLEW_GET_FUN(__glewUniformMatrix4x2dv)
#define glUniformMatrix4x3dv GLEW_GET_FUN(__glewUniformMatrix4x3dv)

#define GLEW_ARB_gpu_shader_fp64 GLEW_GET_VAR(__GLEW_ARB_gpu_shader_fp64)

#endif /* GL_ARB_gpu_shader_fp64 */

/* ------------------------ GL_ARB_half_float_pixel ------------------------ */

#ifndef GL_ARB_half_float_pixel
#define GL_ARB_half_float_pixel 1

#define GL_HALF_FLOAT_ARB 0x140B

#define GLEW_ARB_half_float_pixel GLEW_GET_VAR(__GLEW_ARB_half_float_pixel)

#endif /* GL_ARB_half_float_pixel */

/* ------------------------ GL_ARB_half_float_vertex ----------------------- */

#ifndef GL_ARB_half_float_vertex
#define GL_ARB_half_float_vertex 1

#define GL_HALF_FLOAT 0x140B

#define GLEW_ARB_half_float_vertex GLEW_GET_VAR(__GLEW_ARB_half_float_vertex)

#endif /* GL_ARB_half_float_vertex */

/* ----------------------------- GL_ARB_imaging ---------------------------- */

#ifndef GL_ARB_imaging
#define GL_ARB_imaging 1

#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#define GL_BLEND_COLOR 0x8005
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_BLEND_EQUATION 0x8009
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_CONVOLUTION_1D 0x8010
#define GL_CONVOLUTION_2D 0x8011
#define GL_SEPARABLE_2D 0x8012
#define GL_CONVOLUTION_BORDER_MODE 0x8013
#define GL_CONVOLUTION_FILTER_SCALE 0x8014
#define GL_CONVOLUTION_FILTER_BIAS 0x8015
#define GL_REDUCE 0x8016
#define GL_CONVOLUTION_FORMAT 0x8017
#define GL_CONVOLUTION_WIDTH 0x8018
#define GL_CONVOLUTION_HEIGHT 0x8019
#define GL_MAX_CONVOLUTION_WIDTH 0x801A
#define GL_MAX_CONVOLUTION_HEIGHT 0x801B
#define GL_POST_CONVOLUTION_RED_SCALE 0x801C
#define GL_POST_CONVOLUTION_GREEN_SCALE 0x801D
#define GL_POST_CONVOLUTION_BLUE_SCALE 0x801E
#define GL_POST_CONVOLUTION_ALPHA_SCALE 0x801F
#define GL_POST_CONVOLUTION_RED_BIAS 0x8020
#define GL_POST_CONVOLUTION_GREEN_BIAS 0x8021
#define GL_POST_CONVOLUTION_BLUE_BIAS 0x8022
#define GL_POST_CONVOLUTION_ALPHA_BIAS 0x8023
#define GL_HISTOGRAM 0x8024
#define GL_PROXY_HISTOGRAM 0x8025
#define GL_HISTOGRAM_WIDTH 0x8026
#define GL_HISTOGRAM_FORMAT 0x8027
#define GL_HISTOGRAM_RED_SIZE 0x8028
#define GL_HISTOGRAM_GREEN_SIZE 0x8029
#define GL_HISTOGRAM_BLUE_SIZE 0x802A
#define GL_HISTOGRAM_ALPHA_SIZE 0x802B
#define GL_HISTOGRAM_LUMINANCE_SIZE 0x802C
#define GL_HISTOGRAM_SINK 0x802D
#define GL_MINMAX 0x802E
#define GL_MINMAX_FORMAT 0x802F
#define GL_MINMAX_SINK 0x8030
#define GL_TABLE_TOO_LARGE 0x8031
#define GL_COLOR_MATRIX 0x80B1
#define GL_COLOR_MATRIX_STACK_DEPTH 0x80B2
#define GL_MAX_COLOR_MATRIX_STACK_DEPTH 0x80B3
#define GL_POST_COLOR_MATRIX_RED_SCALE 0x80B4
#define GL_POST_COLOR_MATRIX_GREEN_SCALE 0x80B5
#define GL_POST_COLOR_MATRIX_BLUE_SCALE 0x80B6
#define GL_POST_COLOR_MATRIX_ALPHA_SCALE 0x80B7
#define GL_POST_COLOR_MATRIX_RED_BIAS 0x80B8
#define GL_POST_COLOR_MATRIX_GREEN_BIAS 0x80B9
#define GL_POST_COLOR_MATRIX_BLUE_BIAS 0x80BA
#define GL_POST_COLOR_MATRIX_ALPHA_BIAS 0x80BB
#define GL_COLOR_TABLE 0x80D0
#define GL_POST_CONVOLUTION_COLOR_TABLE 0x80D1
#define GL_POST_COLOR_MATRIX_COLOR_TABLE 0x80D2
#define GL_PROXY_COLOR_TABLE 0x80D3
#define GL_PROXY_POST_CONVOLUTION_COLOR_TABLE 0x80D4
#define GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE 0x80D5
#define GL_COLOR_TABLE_SCALE 0x80D6
#define GL_COLOR_TABLE_BIAS 0x80D7
#define GL_COLOR_TABLE_FORMAT 0x80D8
#define GL_COLOR_TABLE_WIDTH 0x80D9
#define GL_COLOR_TABLE_RED_SIZE 0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE 0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE 0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE 0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE 0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE 0x80DF
#define GL_IGNORE_BORDER 0x8150
#define GL_CONSTANT_BORDER 0x8151
#define GL_WRAP_BORDER 0x8152
#define GL_REPLICATE_BORDER 0x8153
#define GL_CONVOLUTION_BORDER_COLOR 0x8154

typedef void (GLAPIENTRY * PFNGLCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
typedef void (GLAPIENTRY * PFNGLCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (GLAPIENTRY * PFNGLCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat params);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERIPROC) (GLenum target, GLenum pname, GLint params);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GLAPIENTRY * PFNGLCOPYCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPROC) (GLenum target, GLenum format, GLenum type, GLvoid *table);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *image);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETMINMAXPROC) (GLenum target, GLboolean reset, GLenum format, GLenum types, GLvoid *values);
typedef void (GLAPIENTRY * PFNGLGETMINMAXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLGETMINMAXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETSEPARABLEFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
typedef void (GLAPIENTRY * PFNGLHISTOGRAMPROC) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY * PFNGLMINMAXPROC) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY * PFNGLRESETHISTOGRAMPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLRESETMINMAXPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLSEPARABLEFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column);

#define glColorSubTable GLEW_GET_FUN(__glewColorSubTable)
#define glColorTable GLEW_GET_FUN(__glewColorTable)
#define glColorTableParameterfv GLEW_GET_FUN(__glewColorTableParameterfv)
#define glColorTableParameteriv GLEW_GET_FUN(__glewColorTableParameteriv)
#define glConvolutionFilter1D GLEW_GET_FUN(__glewConvolutionFilter1D)
#define glConvolutionFilter2D GLEW_GET_FUN(__glewConvolutionFilter2D)
#define glConvolutionParameterf GLEW_GET_FUN(__glewConvolutionParameterf)
#define glConvolutionParameterfv GLEW_GET_FUN(__glewConvolutionParameterfv)
#define glConvolutionParameteri GLEW_GET_FUN(__glewConvolutionParameteri)
#define glConvolutionParameteriv GLEW_GET_FUN(__glewConvolutionParameteriv)
#define glCopyColorSubTable GLEW_GET_FUN(__glewCopyColorSubTable)
#define glCopyColorTable GLEW_GET_FUN(__glewCopyColorTable)
#define glCopyConvolutionFilter1D GLEW_GET_FUN(__glewCopyConvolutionFilter1D)
#define glCopyConvolutionFilter2D GLEW_GET_FUN(__glewCopyConvolutionFilter2D)
#define glGetColorTable GLEW_GET_FUN(__glewGetColorTable)
#define glGetColorTableParameterfv GLEW_GET_FUN(__glewGetColorTableParameterfv)
#define glGetColorTableParameteriv GLEW_GET_FUN(__glewGetColorTableParameteriv)
#define glGetConvolutionFilter GLEW_GET_FUN(__glewGetConvolutionFilter)
#define glGetConvolutionParameterfv GLEW_GET_FUN(__glewGetConvolutionParameterfv)
#define glGetConvolutionParameteriv GLEW_GET_FUN(__glewGetConvolutionParameteriv)
#define glGetHistogram GLEW_GET_FUN(__glewGetHistogram)
#define glGetHistogramParameterfv GLEW_GET_FUN(__glewGetHistogramParameterfv)
#define glGetHistogramParameteriv GLEW_GET_FUN(__glewGetHistogramParameteriv)
#define glGetMinmax GLEW_GET_FUN(__glewGetMinmax)
#define glGetMinmaxParameterfv GLEW_GET_FUN(__glewGetMinmaxParameterfv)
#define glGetMinmaxParameteriv GLEW_GET_FUN(__glewGetMinmaxParameteriv)
#define glGetSeparableFilter GLEW_GET_FUN(__glewGetSeparableFilter)
#define glHistogram GLEW_GET_FUN(__glewHistogram)
#define glMinmax GLEW_GET_FUN(__glewMinmax)
#define glResetHistogram GLEW_GET_FUN(__glewResetHistogram)
#define glResetMinmax GLEW_GET_FUN(__glewResetMinmax)
#define glSeparableFilter2D GLEW_GET_FUN(__glewSeparableFilter2D)

#define GLEW_ARB_imaging GLEW_GET_VAR(__GLEW_ARB_imaging)

#endif /* GL_ARB_imaging */

/* ------------------------ GL_ARB_instanced_arrays ------------------------ */

#ifndef GL_ARB_instanced_arrays
#define GL_ARB_instanced_arrays 1

#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB 0x88FE

typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBDIVISORARBPROC) (GLuint index, GLuint divisor);

#define glVertexAttribDivisorARB GLEW_GET_FUN(__glewVertexAttribDivisorARB)

#define GLEW_ARB_instanced_arrays GLEW_GET_VAR(__GLEW_ARB_instanced_arrays)

#endif /* GL_ARB_instanced_arrays */

/* ------------------------ GL_ARB_map_buffer_range ------------------------ */

#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range 1

#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020

typedef void (GLAPIENTRY * PFNGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

#define glFlushMappedBufferRange GLEW_GET_FUN(__glewFlushMappedBufferRange)
#define glMapBufferRange GLEW_GET_FUN(__glewMapBufferRange)

#define GLEW_ARB_map_buffer_range GLEW_GET_VAR(__GLEW_ARB_map_buffer_range)

#endif /* GL_ARB_map_buffer_range */

/* ------------------------- GL_ARB_matrix_palette ------------------------- */

#ifndef GL_ARB_matrix_palette
#define GL_ARB_matrix_palette 1

#define GL_MATRIX_PALETTE_ARB 0x8840
#define GL_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB 0x8841
#define GL_MAX_PALETTE_MATRICES_ARB 0x8842
#define GL_CURRENT_PALETTE_MATRIX_ARB 0x8843
#define GL_MATRIX_INDEX_ARRAY_ARB 0x8844
#define GL_CURRENT_MATRIX_INDEX_ARB 0x8845
#define GL_MATRIX_INDEX_ARRAY_SIZE_ARB 0x8846
#define GL_MATRIX_INDEX_ARRAY_TYPE_ARB 0x8847
#define GL_MATRIX_INDEX_ARRAY_STRIDE_ARB 0x8848
#define GL_MATRIX_INDEX_ARRAY_POINTER_ARB 0x8849

typedef void (GLAPIENTRY * PFNGLCURRENTPALETTEMATRIXARBPROC) (GLint index);
typedef void (GLAPIENTRY * PFNGLMATRIXINDEXPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (GLAPIENTRY * PFNGLMATRIXINDEXUBVARBPROC) (GLint size, GLubyte *indices);
typedef void (GLAPIENTRY * PFNGLMATRIXINDEXUIVARBPROC) (GLint size, GLuint *indices);
typedef void (GLAPIENTRY * PFNGLMATRIXINDEXUSVARBPROC) (GLint size, GLushort *indices);

#define glCurrentPaletteMatrixARB GLEW_GET_FUN(__glewCurrentPaletteMatrixARB)
#define glMatrixIndexPointerARB GLEW_GET_FUN(__glewMatrixIndexPointerARB)
#define glMatrixIndexubvARB GLEW_GET_FUN(__glewMatrixIndexubvARB)
#define glMatrixIndexuivARB GLEW_GET_FUN(__glewMatrixIndexuivARB)
#define glMatrixIndexusvARB GLEW_GET_FUN(__glewMatrixIndexusvARB)

#define GLEW_ARB_matrix_palette GLEW_GET_VAR(__GLEW_ARB_matrix_palette)

#endif /* GL_ARB_matrix_palette */

/* --------------------------- GL_ARB_multisample -------------------------- */

#ifndef GL_ARB_multisample
#define GL_ARB_multisample 1

#define GL_MULTISAMPLE_ARB 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE_ARB 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_ARB 0x809F
#define GL_SAMPLE_COVERAGE_ARB 0x80A0
#define GL_SAMPLE_BUFFERS_ARB 0x80A8
#define GL_SAMPLES_ARB 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE_ARB 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT_ARB 0x80AB
#define GL_MULTISAMPLE_BIT_ARB 0x20000000

typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEARBPROC) (GLclampf value, GLboolean invert);

#define glSampleCoverageARB GLEW_GET_FUN(__glewSampleCoverageARB)

#define GLEW_ARB_multisample GLEW_GET_VAR(__GLEW_ARB_multisample)

#endif /* GL_ARB_multisample */

/* -------------------------- GL_ARB_multitexture -------------------------- */

#ifndef GL_ARB_multitexture
#define GL_ARB_multitexture 1

#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#define GL_TEXTURE2_ARB 0x84C2
#define GL_TEXTURE3_ARB 0x84C3
#define GL_TEXTURE4_ARB 0x84C4
#define GL_TEXTURE5_ARB 0x84C5
#define GL_TEXTURE6_ARB 0x84C6
#define GL_TEXTURE7_ARB 0x84C7
#define GL_TEXTURE8_ARB 0x84C8
#define GL_TEXTURE9_ARB 0x84C9
#define GL_TEXTURE10_ARB 0x84CA
#define GL_TEXTURE11_ARB 0x84CB
#define GL_TEXTURE12_ARB 0x84CC
#define GL_TEXTURE13_ARB 0x84CD
#define GL_TEXTURE14_ARB 0x84CE
#define GL_TEXTURE15_ARB 0x84CF
#define GL_TEXTURE16_ARB 0x84D0
#define GL_TEXTURE17_ARB 0x84D1
#define GL_TEXTURE18_ARB 0x84D2
#define GL_TEXTURE19_ARB 0x84D3
#define GL_TEXTURE20_ARB 0x84D4
#define GL_TEXTURE21_ARB 0x84D5
#define GL_TEXTURE22_ARB 0x84D6
#define GL_TEXTURE23_ARB 0x84D7
#define GL_TEXTURE24_ARB 0x84D8
#define GL_TEXTURE25_ARB 0x84D9
#define GL_TEXTURE26_ARB 0x84DA
#define GL_TEXTURE27_ARB 0x84DB
#define GL_TEXTURE28_ARB 0x84DC
#define GL_TEXTURE29_ARB 0x84DD
#define GL_TEXTURE30_ARB 0x84DE
#define GL_TEXTURE31_ARB 0x84DF
#define GL_ACTIVE_TEXTURE_ARB 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB 0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB 0x84E2

typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLCLIENTACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1DARBPROC) (GLenum target, GLdouble s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1FARBPROC) (GLenum target, GLfloat s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1IARBPROC) (GLenum target, GLint s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1IVARBPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1SARBPROC) (GLenum target, GLshort s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1SVARBPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2DARBPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2FARBPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2IARBPROC) (GLenum target, GLint s, GLint t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2IVARBPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2SARBPROC) (GLenum target, GLshort s, GLshort t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2SVARBPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3IARBPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3IVARBPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3SVARBPROC) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4IARBPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4IVARBPROC) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4SVARBPROC) (GLenum target, const GLshort *v);

#define glActiveTextureARB GLEW_GET_FUN(__glewActiveTextureARB)
#define glClientActiveTextureARB GLEW_GET_FUN(__glewClientActiveTextureARB)
#define glMultiTexCoord1dARB GLEW_GET_FUN(__glewMultiTexCoord1dARB)
#define glMultiTexCoord1dvARB GLEW_GET_FUN(__glewMultiTexCoord1dvARB)
#define glMultiTexCoord1fARB GLEW_GET_FUN(__glewMultiTexCoord1fARB)
#define glMultiTexCoord1fvARB GLEW_GET_FUN(__glewMultiTexCoord1fvARB)
#define glMultiTexCoord1iARB GLEW_GET_FUN(__glewMultiTexCoord1iARB)
#define glMultiTexCoord1ivARB GLEW_GET_FUN(__glewMultiTexCoord1ivARB)
#define glMultiTexCoord1sARB GLEW_GET_FUN(__glewMultiTexCoord1sARB)
#define glMultiTexCoord1svARB GLEW_GET_FUN(__glewMultiTexCoord1svARB)
#define glMultiTexCoord2dARB GLEW_GET_FUN(__glewMultiTexCoord2dARB)
#define glMultiTexCoord2dvARB GLEW_GET_FUN(__glewMultiTexCoord2dvARB)
#define glMultiTexCoord2fARB GLEW_GET_FUN(__glewMultiTexCoord2fARB)
#define glMultiTexCoord2fvARB GLEW_GET_FUN(__glewMultiTexCoord2fvARB)
#define glMultiTexCoord2iARB GLEW_GET_FUN(__glewMultiTexCoord2iARB)
#define glMultiTexCoord2ivARB GLEW_GET_FUN(__glewMultiTexCoord2ivARB)
#define glMultiTexCoord2sARB GLEW_GET_FUN(__glewMultiTexCoord2sARB)
#define glMultiTexCoord2svARB GLEW_GET_FUN(__glewMultiTexCoord2svARB)
#define glMultiTexCoord3dARB GLEW_GET_FUN(__glewMultiTexCoord3dARB)
#define glMultiTexCoord3dvARB GLEW_GET_FUN(__glewMultiTexCoord3dvARB)
#define glMultiTexCoord3fARB GLEW_GET_FUN(__glewMultiTexCoord3fARB)
#define glMultiTexCoord3fvARB GLEW_GET_FUN(__glewMultiTexCoord3fvARB)
#define glMultiTexCoord3iARB GLEW_GET_FUN(__glewMultiTexCoord3iARB)
#define glMultiTexCoord3ivARB GLEW_GET_FUN(__glewMultiTexCoord3ivARB)
#define glMultiTexCoord3sARB GLEW_GET_FUN(__glewMultiTexCoord3sARB)
#define glMultiTexCoord3svARB GLEW_GET_FUN(__glewMultiTexCoord3svARB)
#define glMultiTexCoord4dARB GLEW_GET_FUN(__glewMultiTexCoord4dARB)
#define glMultiTexCoord4dvARB GLEW_GET_FUN(__glewMultiTexCoord4dvARB)
#define glMultiTexCoord4fARB GLEW_GET_FUN(__glewMultiTexCoord4fARB)
#define glMultiTexCoord4fvARB GLEW_GET_FUN(__glewMultiTexCoord4fvARB)
#define glMultiTexCoord4iARB GLEW_GET_FUN(__glewMultiTexCoord4iARB)
#define glMultiTexCoord4ivARB GLEW_GET_FUN(__glewMultiTexCoord4ivARB)
#define glMultiTexCoord4sARB GLEW_GET_FUN(__glewMultiTexCoord4sARB)
#define glMultiTexCoord4svARB GLEW_GET_FUN(__glewMultiTexCoord4svARB)

#define GLEW_ARB_multitexture GLEW_GET_VAR(__GLEW_ARB_multitexture)

#endif /* GL_ARB_multitexture */

/* ------------------------- GL_ARB_occlusion_query ------------------------ */

#ifndef GL_ARB_occlusion_query
#define GL_ARB_occlusion_query 1

#define GL_QUERY_COUNTER_BITS_ARB 0x8864
#define GL_CURRENT_QUERY_ARB 0x8865
#define GL_QUERY_RESULT_ARB 0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB 0x8867
#define GL_SAMPLES_PASSED_ARB 0x8914

typedef void (GLAPIENTRY * PFNGLBEGINQUERYARBPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETEQUERIESARBPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLENDQUERYARBPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGENQUERIESARBPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTIVARBPROC) (GLuint id, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTUIVARBPROC) (GLuint id, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYIVARBPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISQUERYARBPROC) (GLuint id);

#define glBeginQueryARB GLEW_GET_FUN(__glewBeginQueryARB)
#define glDeleteQueriesARB GLEW_GET_FUN(__glewDeleteQueriesARB)
#define glEndQueryARB GLEW_GET_FUN(__glewEndQueryARB)
#define glGenQueriesARB GLEW_GET_FUN(__glewGenQueriesARB)
#define glGetQueryObjectivARB GLEW_GET_FUN(__glewGetQueryObjectivARB)
#define glGetQueryObjectuivARB GLEW_GET_FUN(__glewGetQueryObjectuivARB)
#define glGetQueryivARB GLEW_GET_FUN(__glewGetQueryivARB)
#define glIsQueryARB GLEW_GET_FUN(__glewIsQueryARB)

#define GLEW_ARB_occlusion_query GLEW_GET_VAR(__GLEW_ARB_occlusion_query)

#endif /* GL_ARB_occlusion_query */

/* ------------------------ GL_ARB_occlusion_query2 ------------------------ */

#ifndef GL_ARB_occlusion_query2
#define GL_ARB_occlusion_query2 1

#define GL_ANY_SAMPLES_PASSED 0x8C2F

#define GLEW_ARB_occlusion_query2 GLEW_GET_VAR(__GLEW_ARB_occlusion_query2)

#endif /* GL_ARB_occlusion_query2 */

/* ----------------------- GL_ARB_pixel_buffer_object ---------------------- */

#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1

#define GL_PIXEL_PACK_BUFFER_ARB 0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF

#define GLEW_ARB_pixel_buffer_object GLEW_GET_VAR(__GLEW_ARB_pixel_buffer_object)

#endif /* GL_ARB_pixel_buffer_object */

/* ------------------------ GL_ARB_point_parameters ------------------------ */

#ifndef GL_ARB_point_parameters
#define GL_ARB_point_parameters 1

#define GL_POINT_SIZE_MIN_ARB 0x8126
#define GL_POINT_SIZE_MAX_ARB 0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_ARB 0x8128
#define GL_POINT_DISTANCE_ATTENUATION_ARB 0x8129

typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFARBPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFVARBPROC) (GLenum pname, const GLfloat* params);

#define glPointParameterfARB GLEW_GET_FUN(__glewPointParameterfARB)
#define glPointParameterfvARB GLEW_GET_FUN(__glewPointParameterfvARB)

#define GLEW_ARB_point_parameters GLEW_GET_VAR(__GLEW_ARB_point_parameters)

#endif /* GL_ARB_point_parameters */

/* -------------------------- GL_ARB_point_sprite -------------------------- */

#ifndef GL_ARB_point_sprite
#define GL_ARB_point_sprite 1

#define GL_POINT_SPRITE_ARB 0x8861
#define GL_COORD_REPLACE_ARB 0x8862

#define GLEW_ARB_point_sprite GLEW_GET_VAR(__GLEW_ARB_point_sprite)

#endif /* GL_ARB_point_sprite */

/* ------------------------ GL_ARB_provoking_vertex ------------------------ */

#ifndef GL_ARB_provoking_vertex
#define GL_ARB_provoking_vertex 1

#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION 0x8E4C
#define GL_FIRST_VERTEX_CONVENTION 0x8E4D
#define GL_LAST_VERTEX_CONVENTION 0x8E4E
#define GL_PROVOKING_VERTEX 0x8E4F

typedef void (GLAPIENTRY * PFNGLPROVOKINGVERTEXPROC) (GLenum mode);

#define glProvokingVertex GLEW_GET_FUN(__glewProvokingVertex)

#define GLEW_ARB_provoking_vertex GLEW_GET_VAR(__GLEW_ARB_provoking_vertex)

#endif /* GL_ARB_provoking_vertex */

/* --------------------------- GL_ARB_robustness --------------------------- */

#ifndef GL_ARB_robustness
#define GL_ARB_robustness 1

#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB 0x00000004
#define GL_LOSE_CONTEXT_ON_RESET_ARB 0x8252
#define GL_GUILTY_CONTEXT_RESET_ARB 0x8253
#define GL_INNOCENT_CONTEXT_RESET_ARB 0x8254
#define GL_UNKNOWN_CONTEXT_RESET_ARB 0x8255
#define GL_RESET_NOTIFICATION_STRATEGY_ARB 0x8256
#define GL_NO_RESET_NOTIFICATION_ARB 0x8261

typedef void (GLAPIENTRY * PFNGLGETNCOLORTABLEARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei bufSize, void* table);
typedef void (GLAPIENTRY * PFNGLGETNCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint lod, GLsizei bufSize, void* img);
typedef void (GLAPIENTRY * PFNGLGETNCONVOLUTIONFILTERARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei bufSize, void* image);
typedef void (GLAPIENTRY * PFNGLGETNHISTOGRAMARBPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void* values);
typedef void (GLAPIENTRY * PFNGLGETNMAPDVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLdouble* v);
typedef void (GLAPIENTRY * PFNGLGETNMAPFVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLfloat* v);
typedef void (GLAPIENTRY * PFNGLGETNMAPIVARBPROC) (GLenum target, GLenum query, GLsizei bufSize, GLint* v);
typedef void (GLAPIENTRY * PFNGLGETNMINMAXARBPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void* values);
typedef void (GLAPIENTRY * PFNGLGETNPIXELMAPFVARBPROC) (GLenum map, GLsizei bufSize, GLfloat* values);
typedef void (GLAPIENTRY * PFNGLGETNPIXELMAPUIVARBPROC) (GLenum map, GLsizei bufSize, GLuint* values);
typedef void (GLAPIENTRY * PFNGLGETNPIXELMAPUSVARBPROC) (GLenum map, GLsizei bufSize, GLushort* values);
typedef void (GLAPIENTRY * PFNGLGETNPOLYGONSTIPPLEARBPROC) (GLsizei bufSize, GLubyte* pattern);
typedef void (GLAPIENTRY * PFNGLGETNSEPARABLEFILTERARBPROC) (GLenum target, GLenum format, GLenum type, GLsizei rowBufSize, void* row, GLsizei columnBufSize, GLvoid*column, GLvoid*span);
typedef void (GLAPIENTRY * PFNGLGETNTEXIMAGEARBPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* img);
typedef void (GLAPIENTRY * PFNGLGETNUNIFORMDVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETNUNIFORMFVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETNUNIFORMIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNUNIFORMUIVARBPROC) (GLuint program, GLint location, GLsizei bufSize, GLuint* params);
typedef void (GLAPIENTRY * PFNGLREADNPIXELSARBPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* data);

#define glGetnColorTableARB GLEW_GET_FUN(__glewGetnColorTableARB)
#define glGetnCompressedTexImageARB GLEW_GET_FUN(__glewGetnCompressedTexImageARB)
#define glGetnConvolutionFilterARB GLEW_GET_FUN(__glewGetnConvolutionFilterARB)
#define glGetnHistogramARB GLEW_GET_FUN(__glewGetnHistogramARB)
#define glGetnMapdvARB GLEW_GET_FUN(__glewGetnMapdvARB)
#define glGetnMapfvARB GLEW_GET_FUN(__glewGetnMapfvARB)
#define glGetnMapivARB GLEW_GET_FUN(__glewGetnMapivARB)
#define glGetnMinmaxARB GLEW_GET_FUN(__glewGetnMinmaxARB)
#define glGetnPixelMapfvARB GLEW_GET_FUN(__glewGetnPixelMapfvARB)
#define glGetnPixelMapuivARB GLEW_GET_FUN(__glewGetnPixelMapuivARB)
#define glGetnPixelMapusvARB GLEW_GET_FUN(__glewGetnPixelMapusvARB)
#define glGetnPolygonStippleARB GLEW_GET_FUN(__glewGetnPolygonStippleARB)
#define glGetnSeparableFilterARB GLEW_GET_FUN(__glewGetnSeparableFilterARB)
#define glGetnTexImageARB GLEW_GET_FUN(__glewGetnTexImageARB)
#define glGetnUniformdvARB GLEW_GET_FUN(__glewGetnUniformdvARB)
#define glGetnUniformfvARB GLEW_GET_FUN(__glewGetnUniformfvARB)
#define glGetnUniformivARB GLEW_GET_FUN(__glewGetnUniformivARB)
#define glGetnUniformuivARB GLEW_GET_FUN(__glewGetnUniformuivARB)
#define glReadnPixelsARB GLEW_GET_FUN(__glewReadnPixelsARB)

#define GLEW_ARB_robustness GLEW_GET_VAR(__GLEW_ARB_robustness)

#endif /* GL_ARB_robustness */

/* ------------------------- GL_ARB_sample_shading ------------------------- */

#ifndef GL_ARB_sample_shading
#define GL_ARB_sample_shading 1

#define GL_SAMPLE_SHADING_ARB 0x8C36
#define GL_MIN_SAMPLE_SHADING_VALUE_ARB 0x8C37

typedef void (GLAPIENTRY * PFNGLMINSAMPLESHADINGARBPROC) (GLclampf value);

#define glMinSampleShadingARB GLEW_GET_FUN(__glewMinSampleShadingARB)

#define GLEW_ARB_sample_shading GLEW_GET_VAR(__GLEW_ARB_sample_shading)

#endif /* GL_ARB_sample_shading */

/* ------------------------- GL_ARB_sampler_objects ------------------------ */

#ifndef GL_ARB_sampler_objects
#define GL_ARB_sampler_objects 1

#define GL_SAMPLER_BINDING 0x8919

typedef void (GLAPIENTRY * PFNGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
typedef void (GLAPIENTRY * PFNGLDELETESAMPLERSPROC) (GLsizei count, const GLuint * samplers);
typedef void (GLAPIENTRY * PFNGLGENSAMPLERSPROC) (GLsizei count, GLuint* samplers);
typedef void (GLAPIENTRY * PFNGLGETSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISSAMPLERPROC) (GLuint sampler);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERIIVPROC) (GLuint sampler, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERIUIVPROC) (GLuint sampler, GLenum pname, const GLuint* params);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERFPROC) (GLuint sampler, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERFVPROC) (GLuint sampler, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERIPROC) (GLuint sampler, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLSAMPLERPARAMETERIVPROC) (GLuint sampler, GLenum pname, const GLint* params);

#define glBindSampler GLEW_GET_FUN(__glewBindSampler)
#define glDeleteSamplers GLEW_GET_FUN(__glewDeleteSamplers)
#define glGenSamplers GLEW_GET_FUN(__glewGenSamplers)
#define glGetSamplerParameterIiv GLEW_GET_FUN(__glewGetSamplerParameterIiv)
#define glGetSamplerParameterIuiv GLEW_GET_FUN(__glewGetSamplerParameterIuiv)
#define glGetSamplerParameterfv GLEW_GET_FUN(__glewGetSamplerParameterfv)
#define glGetSamplerParameteriv GLEW_GET_FUN(__glewGetSamplerParameteriv)
#define glIsSampler GLEW_GET_FUN(__glewIsSampler)
#define glSamplerParameterIiv GLEW_GET_FUN(__glewSamplerParameterIiv)
#define glSamplerParameterIuiv GLEW_GET_FUN(__glewSamplerParameterIuiv)
#define glSamplerParameterf GLEW_GET_FUN(__glewSamplerParameterf)
#define glSamplerParameterfv GLEW_GET_FUN(__glewSamplerParameterfv)
#define glSamplerParameteri GLEW_GET_FUN(__glewSamplerParameteri)
#define glSamplerParameteriv GLEW_GET_FUN(__glewSamplerParameteriv)

#define GLEW_ARB_sampler_objects GLEW_GET_VAR(__GLEW_ARB_sampler_objects)

#endif /* GL_ARB_sampler_objects */

/* ------------------------ GL_ARB_seamless_cube_map ----------------------- */

#ifndef GL_ARB_seamless_cube_map
#define GL_ARB_seamless_cube_map 1

#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

#define GLEW_ARB_seamless_cube_map GLEW_GET_VAR(__GLEW_ARB_seamless_cube_map)

#endif /* GL_ARB_seamless_cube_map */

/* --------------------- GL_ARB_separate_shader_objects -------------------- */

#ifndef GL_ARB_separate_shader_objects
#define GL_ARB_separate_shader_objects 1

#define GL_VERTEX_SHADER_BIT 0x00000001
#define GL_FRAGMENT_SHADER_BIT 0x00000002
#define GL_GEOMETRY_SHADER_BIT 0x00000004
#define GL_TESS_CONTROL_SHADER_BIT 0x00000008
#define GL_TESS_EVALUATION_SHADER_BIT 0x00000010
#define GL_PROGRAM_SEPARABLE 0x8258
#define GL_ACTIVE_PROGRAM 0x8259
#define GL_PROGRAM_PIPELINE_BINDING 0x825A
#define GL_ALL_SHADER_BITS 0xFFFFFFFF

typedef void (GLAPIENTRY * PFNGLACTIVESHADERPROGRAMPROC) (GLuint pipeline, GLuint program);
typedef void (GLAPIENTRY * PFNGLBINDPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROGRAMVPROC) (GLenum type, GLsizei count, const char ** strings);
typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPIPELINESPROC) (GLsizei n, const GLuint* pipelines);
typedef void (GLAPIENTRY * PFNGLGENPROGRAMPIPELINESPROC) (GLsizei n, GLuint* pipelines);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPIPELINEINFOLOGPROC) (GLuint pipeline, GLsizei bufSize, GLsizei* length, char *infoLog);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPIPELINEIVPROC) (GLuint pipeline, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISPROGRAMPIPELINEPROC) (GLuint pipeline);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1DPROC) (GLuint program, GLint location, GLdouble x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FPROC) (GLuint program, GLint location, GLfloat x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IPROC) (GLuint program, GLint location, GLint x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IVPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UIPROC) (GLuint program, GLint location, GLuint x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2DPROC) (GLuint program, GLint location, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FPROC) (GLuint program, GLint location, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IPROC) (GLuint program, GLint location, GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IVPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UIPROC) (GLuint program, GLint location, GLuint x, GLuint y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3DPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IVPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UIPROC) (GLuint program, GLint location, GLuint x, GLuint y, GLuint z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4DPROC) (GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4DVPROC) (GLuint program, GLint location, GLsizei count, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FVPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IVPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UIPROC) (GLuint program, GLint location, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UIVPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUSEPROGRAMSTAGESPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMPIPELINEPROC) (GLuint pipeline);

#define glActiveShaderProgram GLEW_GET_FUN(__glewActiveShaderProgram)
#define glBindProgramPipeline GLEW_GET_FUN(__glewBindProgramPipeline)
#define glCreateShaderProgramv GLEW_GET_FUN(__glewCreateShaderProgramv)
#define glDeleteProgramPipelines GLEW_GET_FUN(__glewDeleteProgramPipelines)
#define glGenProgramPipelines GLEW_GET_FUN(__glewGenProgramPipelines)
#define glGetProgramPipelineInfoLog GLEW_GET_FUN(__glewGetProgramPipelineInfoLog)
#define glGetProgramPipelineiv GLEW_GET_FUN(__glewGetProgramPipelineiv)
#define glIsProgramPipeline GLEW_GET_FUN(__glewIsProgramPipeline)
#define glProgramUniform1d GLEW_GET_FUN(__glewProgramUniform1d)
#define glProgramUniform1dv GLEW_GET_FUN(__glewProgramUniform1dv)
#define glProgramUniform1f GLEW_GET_FUN(__glewProgramUniform1f)
#define glProgramUniform1fv GLEW_GET_FUN(__glewProgramUniform1fv)
#define glProgramUniform1i GLEW_GET_FUN(__glewProgramUniform1i)
#define glProgramUniform1iv GLEW_GET_FUN(__glewProgramUniform1iv)
#define glProgramUniform1ui GLEW_GET_FUN(__glewProgramUniform1ui)
#define glProgramUniform1uiv GLEW_GET_FUN(__glewProgramUniform1uiv)
#define glProgramUniform2d GLEW_GET_FUN(__glewProgramUniform2d)
#define glProgramUniform2dv GLEW_GET_FUN(__glewProgramUniform2dv)
#define glProgramUniform2f GLEW_GET_FUN(__glewProgramUniform2f)
#define glProgramUniform2fv GLEW_GET_FUN(__glewProgramUniform2fv)
#define glProgramUniform2i GLEW_GET_FUN(__glewProgramUniform2i)
#define glProgramUniform2iv GLEW_GET_FUN(__glewProgramUniform2iv)
#define glProgramUniform2ui GLEW_GET_FUN(__glewProgramUniform2ui)
#define glProgramUniform2uiv GLEW_GET_FUN(__glewProgramUniform2uiv)
#define glProgramUniform3d GLEW_GET_FUN(__glewProgramUniform3d)
#define glProgramUniform3dv GLEW_GET_FUN(__glewProgramUniform3dv)
#define glProgramUniform3f GLEW_GET_FUN(__glewProgramUniform3f)
#define glProgramUniform3fv GLEW_GET_FUN(__glewProgramUniform3fv)
#define glProgramUniform3i GLEW_GET_FUN(__glewProgramUniform3i)
#define glProgramUniform3iv GLEW_GET_FUN(__glewProgramUniform3iv)
#define glProgramUniform3ui GLEW_GET_FUN(__glewProgramUniform3ui)
#define glProgramUniform3uiv GLEW_GET_FUN(__glewProgramUniform3uiv)
#define glProgramUniform4d GLEW_GET_FUN(__glewProgramUniform4d)
#define glProgramUniform4dv GLEW_GET_FUN(__glewProgramUniform4dv)
#define glProgramUniform4f GLEW_GET_FUN(__glewProgramUniform4f)
#define glProgramUniform4fv GLEW_GET_FUN(__glewProgramUniform4fv)
#define glProgramUniform4i GLEW_GET_FUN(__glewProgramUniform4i)
#define glProgramUniform4iv GLEW_GET_FUN(__glewProgramUniform4iv)
#define glProgramUniform4ui GLEW_GET_FUN(__glewProgramUniform4ui)
#define glProgramUniform4uiv GLEW_GET_FUN(__glewProgramUniform4uiv)
#define glProgramUniformMatrix2dv GLEW_GET_FUN(__glewProgramUniformMatrix2dv)
#define glProgramUniformMatrix2fv GLEW_GET_FUN(__glewProgramUniformMatrix2fv)
#define glProgramUniformMatrix2x3dv GLEW_GET_FUN(__glewProgramUniformMatrix2x3dv)
#define glProgramUniformMatrix2x3fv GLEW_GET_FUN(__glewProgramUniformMatrix2x3fv)
#define glProgramUniformMatrix2x4dv GLEW_GET_FUN(__glewProgramUniformMatrix2x4dv)
#define glProgramUniformMatrix2x4fv GLEW_GET_FUN(__glewProgramUniformMatrix2x4fv)
#define glProgramUniformMatrix3dv GLEW_GET_FUN(__glewProgramUniformMatrix3dv)
#define glProgramUniformMatrix3fv GLEW_GET_FUN(__glewProgramUniformMatrix3fv)
#define glProgramUniformMatrix3x2dv GLEW_GET_FUN(__glewProgramUniformMatrix3x2dv)
#define glProgramUniformMatrix3x2fv GLEW_GET_FUN(__glewProgramUniformMatrix3x2fv)
#define glProgramUniformMatrix3x4dv GLEW_GET_FUN(__glewProgramUniformMatrix3x4dv)
#define glProgramUniformMatrix3x4fv GLEW_GET_FUN(__glewProgramUniformMatrix3x4fv)
#define glProgramUniformMatrix4dv GLEW_GET_FUN(__glewProgramUniformMatrix4dv)
#define glProgramUniformMatrix4fv GLEW_GET_FUN(__glewProgramUniformMatrix4fv)
#define glProgramUniformMatrix4x2dv GLEW_GET_FUN(__glewProgramUniformMatrix4x2dv)
#define glProgramUniformMatrix4x2fv GLEW_GET_FUN(__glewProgramUniformMatrix4x2fv)
#define glProgramUniformMatrix4x3dv GLEW_GET_FUN(__glewProgramUniformMatrix4x3dv)
#define glProgramUniformMatrix4x3fv GLEW_GET_FUN(__glewProgramUniformMatrix4x3fv)
#define glUseProgramStages GLEW_GET_FUN(__glewUseProgramStages)
#define glValidateProgramPipeline GLEW_GET_FUN(__glewValidateProgramPipeline)

#define GLEW_ARB_separate_shader_objects GLEW_GET_VAR(__GLEW_ARB_separate_shader_objects)

#endif /* GL_ARB_separate_shader_objects */

/* ----------------------- GL_ARB_shader_bit_encoding ---------------------- */

#ifndef GL_ARB_shader_bit_encoding
#define GL_ARB_shader_bit_encoding 1

#define GLEW_ARB_shader_bit_encoding GLEW_GET_VAR(__GLEW_ARB_shader_bit_encoding)

#endif /* GL_ARB_shader_bit_encoding */

/* ------------------------- GL_ARB_shader_objects ------------------------- */

#ifndef GL_ARB_shader_objects
#define GL_ARB_shader_objects 1

#define GL_PROGRAM_OBJECT_ARB 0x8B40
#define GL_SHADER_OBJECT_ARB 0x8B48
#define GL_OBJECT_TYPE_ARB 0x8B4E
#define GL_OBJECT_SUBTYPE_ARB 0x8B4F
#define GL_FLOAT_VEC2_ARB 0x8B50
#define GL_FLOAT_VEC3_ARB 0x8B51
#define GL_FLOAT_VEC4_ARB 0x8B52
#define GL_INT_VEC2_ARB 0x8B53
#define GL_INT_VEC3_ARB 0x8B54
#define GL_INT_VEC4_ARB 0x8B55
#define GL_BOOL_ARB 0x8B56
#define GL_BOOL_VEC2_ARB 0x8B57
#define GL_BOOL_VEC3_ARB 0x8B58
#define GL_BOOL_VEC4_ARB 0x8B59
#define GL_FLOAT_MAT2_ARB 0x8B5A
#define GL_FLOAT_MAT3_ARB 0x8B5B
#define GL_FLOAT_MAT4_ARB 0x8B5C
#define GL_SAMPLER_1D_ARB 0x8B5D
#define GL_SAMPLER_2D_ARB 0x8B5E
#define GL_SAMPLER_3D_ARB 0x8B5F
#define GL_SAMPLER_CUBE_ARB 0x8B60
#define GL_SAMPLER_1D_SHADOW_ARB 0x8B61
#define GL_SAMPLER_2D_SHADOW_ARB 0x8B62
#define GL_SAMPLER_2D_RECT_ARB 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB 0x8B64
#define GL_OBJECT_DELETE_STATUS_ARB 0x8B80
#define GL_OBJECT_COMPILE_STATUS_ARB 0x8B81
#define GL_OBJECT_LINK_STATUS_ARB 0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB 0x8B83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB 0x8B84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB 0x8B85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB 0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB 0x8B87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB 0x8B88

typedef char GLcharARB;
typedef unsigned int GLhandleARB;

typedef void (GLAPIENTRY * PFNGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
typedef void (GLAPIENTRY * PFNGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
typedef GLhandleARB (GLAPIENTRY * PFNGLCREATEPROGRAMOBJECTARBPROC) (void);
typedef GLhandleARB (GLAPIENTRY * PFNGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
typedef void (GLAPIENTRY * PFNGLDELETEOBJECTARBPROC) (GLhandleARB obj);
typedef void (GLAPIENTRY * PFNGLDETACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB attachedObj);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei* length, GLint *size, GLenum *type, GLcharARB *name);
typedef void (GLAPIENTRY * PFNGLGETATTACHEDOBJECTSARBPROC) (GLhandleARB containerObj, GLsizei maxCount, GLsizei* count, GLhandleARB *obj);
typedef GLhandleARB (GLAPIENTRY * PFNGLGETHANDLEARBPROC) (GLenum pname);
typedef void (GLAPIENTRY * PFNGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB *infoLog);
typedef void (GLAPIENTRY * PFNGLGETOBJECTPARAMETERFVARBPROC) (GLhandleARB obj, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETSHADERSOURCEARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB *source);
typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB* name);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMFVARBPROC) (GLhandleARB programObj, GLint location, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMIVARBPROC) (GLhandleARB programObj, GLint location, GLint* params);
typedef void (GLAPIENTRY * PFNGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (GLAPIENTRY * PFNGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB ** string, const GLint *length);
typedef void (GLAPIENTRY * PFNGLUNIFORM1FARBPROC) (GLint location, GLfloat v0);
typedef void (GLAPIENTRY * PFNGLUNIFORM1FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM1IARBPROC) (GLint location, GLint v0);
typedef void (GLAPIENTRY * PFNGLUNIFORM1IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2FARBPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (GLAPIENTRY * PFNGLUNIFORM2FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2IARBPROC) (GLint location, GLint v0, GLint v1);
typedef void (GLAPIENTRY * PFNGLUNIFORM2IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GLAPIENTRY * PFNGLUNIFORM3FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (GLAPIENTRY * PFNGLUNIFORM3IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GLAPIENTRY * PFNGLUNIFORM4FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (GLAPIENTRY * PFNGLUNIFORM4IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX2FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX3FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMARBPROC) (GLhandleARB programObj);

#define glAttachObjectARB GLEW_GET_FUN(__glewAttachObjectARB)
#define glCompileShaderARB GLEW_GET_FUN(__glewCompileShaderARB)
#define glCreateProgramObjectARB GLEW_GET_FUN(__glewCreateProgramObjectARB)
#define glCreateShaderObjectARB GLEW_GET_FUN(__glewCreateShaderObjectARB)
#define glDeleteObjectARB GLEW_GET_FUN(__glewDeleteObjectARB)
#define glDetachObjectARB GLEW_GET_FUN(__glewDetachObjectARB)
#define glGetActiveUniformARB GLEW_GET_FUN(__glewGetActiveUniformARB)
#define glGetAttachedObjectsARB GLEW_GET_FUN(__glewGetAttachedObjectsARB)
#define glGetHandleARB GLEW_GET_FUN(__glewGetHandleARB)
#define glGetInfoLogARB GLEW_GET_FUN(__glewGetInfoLogARB)
#define glGetObjectParameterfvARB GLEW_GET_FUN(__glewGetObjectParameterfvARB)
#define glGetObjectParameterivARB GLEW_GET_FUN(__glewGetObjectParameterivARB)
#define glGetShaderSourceARB GLEW_GET_FUN(__glewGetShaderSourceARB)
#define glGetUniformLocationARB GLEW_GET_FUN(__glewGetUniformLocationARB)
#define glGetUniformfvARB GLEW_GET_FUN(__glewGetUniformfvARB)
#define glGetUniformivARB GLEW_GET_FUN(__glewGetUniformivARB)
#define glLinkProgramARB GLEW_GET_FUN(__glewLinkProgramARB)
#define glShaderSourceARB GLEW_GET_FUN(__glewShaderSourceARB)
#define glUniform1fARB GLEW_GET_FUN(__glewUniform1fARB)
#define glUniform1fvARB GLEW_GET_FUN(__glewUniform1fvARB)
#define glUniform1iARB GLEW_GET_FUN(__glewUniform1iARB)
#define glUniform1ivARB GLEW_GET_FUN(__glewUniform1ivARB)
#define glUniform2fARB GLEW_GET_FUN(__glewUniform2fARB)
#define glUniform2fvARB GLEW_GET_FUN(__glewUniform2fvARB)
#define glUniform2iARB GLEW_GET_FUN(__glewUniform2iARB)
#define glUniform2ivARB GLEW_GET_FUN(__glewUniform2ivARB)
#define glUniform3fARB GLEW_GET_FUN(__glewUniform3fARB)
#define glUniform3fvARB GLEW_GET_FUN(__glewUniform3fvARB)
#define glUniform3iARB GLEW_GET_FUN(__glewUniform3iARB)
#define glUniform3ivARB GLEW_GET_FUN(__glewUniform3ivARB)
#define glUniform4fARB GLEW_GET_FUN(__glewUniform4fARB)
#define glUniform4fvARB GLEW_GET_FUN(__glewUniform4fvARB)
#define glUniform4iARB GLEW_GET_FUN(__glewUniform4iARB)
#define glUniform4ivARB GLEW_GET_FUN(__glewUniform4ivARB)
#define glUniformMatrix2fvARB GLEW_GET_FUN(__glewUniformMatrix2fvARB)
#define glUniformMatrix3fvARB GLEW_GET_FUN(__glewUniformMatrix3fvARB)
#define glUniformMatrix4fvARB GLEW_GET_FUN(__glewUniformMatrix4fvARB)
#define glUseProgramObjectARB GLEW_GET_FUN(__glewUseProgramObjectARB)
#define glValidateProgramARB GLEW_GET_FUN(__glewValidateProgramARB)

#define GLEW_ARB_shader_objects GLEW_GET_VAR(__GLEW_ARB_shader_objects)

#endif /* GL_ARB_shader_objects */

/* ------------------------ GL_ARB_shader_precision ------------------------ */

#ifndef GL_ARB_shader_precision
#define GL_ARB_shader_precision 1

#define GLEW_ARB_shader_precision GLEW_GET_VAR(__GLEW_ARB_shader_precision)

#endif /* GL_ARB_shader_precision */

/* ---------------------- GL_ARB_shader_stencil_export --------------------- */

#ifndef GL_ARB_shader_stencil_export
#define GL_ARB_shader_stencil_export 1

#define GLEW_ARB_shader_stencil_export GLEW_GET_VAR(__GLEW_ARB_shader_stencil_export)

#endif /* GL_ARB_shader_stencil_export */

/* ------------------------ GL_ARB_shader_subroutine ----------------------- */

#ifndef GL_ARB_shader_subroutine
#define GL_ARB_shader_subroutine 1

#define GL_ACTIVE_SUBROUTINES 0x8DE5
#define GL_ACTIVE_SUBROUTINE_UNIFORMS 0x8DE6
#define GL_MAX_SUBROUTINES 0x8DE7
#define GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS 0x8DE8
#define GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS 0x8E47
#define GL_ACTIVE_SUBROUTINE_MAX_LENGTH 0x8E48
#define GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH 0x8E49
#define GL_NUM_COMPATIBLE_SUBROUTINES 0x8E4A
#define GL_COMPATIBLE_SUBROUTINES 0x8E4B

typedef void (GLAPIENTRY * PFNGLGETACTIVESUBROUTINENAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei* length, char *name);
typedef void (GLAPIENTRY * PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC) (GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei* length, char *name);
typedef void (GLAPIENTRY * PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC) (GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint* values);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMSTAGEIVPROC) (GLuint program, GLenum shadertype, GLenum pname, GLint* values);
typedef GLuint (GLAPIENTRY * PFNGLGETSUBROUTINEINDEXPROC) (GLuint program, GLenum shadertype, const char* name);
typedef GLint (GLAPIENTRY * PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC) (GLuint program, GLenum shadertype, const char* name);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMSUBROUTINEUIVPROC) (GLenum shadertype, GLint location, GLuint* params);
typedef void (GLAPIENTRY * PFNGLUNIFORMSUBROUTINESUIVPROC) (GLenum shadertype, GLsizei count, const GLuint* indices);

#define glGetActiveSubroutineName GLEW_GET_FUN(__glewGetActiveSubroutineName)
#define glGetActiveSubroutineUniformName GLEW_GET_FUN(__glewGetActiveSubroutineUniformName)
#define glGetActiveSubroutineUniformiv GLEW_GET_FUN(__glewGetActiveSubroutineUniformiv)
#define glGetProgramStageiv GLEW_GET_FUN(__glewGetProgramStageiv)
#define glGetSubroutineIndex GLEW_GET_FUN(__glewGetSubroutineIndex)
#define glGetSubroutineUniformLocation GLEW_GET_FUN(__glewGetSubroutineUniformLocation)
#define glGetUniformSubroutineuiv GLEW_GET_FUN(__glewGetUniformSubroutineuiv)
#define glUniformSubroutinesuiv GLEW_GET_FUN(__glewUniformSubroutinesuiv)

#define GLEW_ARB_shader_subroutine GLEW_GET_VAR(__GLEW_ARB_shader_subroutine)

#endif /* GL_ARB_shader_subroutine */

/* ----------------------- GL_ARB_shader_texture_lod ----------------------- */

#ifndef GL_ARB_shader_texture_lod
#define GL_ARB_shader_texture_lod 1

#define GLEW_ARB_shader_texture_lod GLEW_GET_VAR(__GLEW_ARB_shader_texture_lod)

#endif /* GL_ARB_shader_texture_lod */

/* ---------------------- GL_ARB_shading_language_100 ---------------------- */

#ifndef GL_ARB_shading_language_100
#define GL_ARB_shading_language_100 1

#define GL_SHADING_LANGUAGE_VERSION_ARB 0x8B8C

#define GLEW_ARB_shading_language_100 GLEW_GET_VAR(__GLEW_ARB_shading_language_100)

#endif /* GL_ARB_shading_language_100 */

/* -------------------- GL_ARB_shading_language_include -------------------- */

#ifndef GL_ARB_shading_language_include
#define GL_ARB_shading_language_include 1

#define GL_SHADER_INCLUDE_ARB 0x8DAE
#define GL_NAMED_STRING_LENGTH_ARB 0x8DE9
#define GL_NAMED_STRING_TYPE_ARB 0x8DEA

typedef void (GLAPIENTRY * PFNGLCOMPILESHADERINCLUDEARBPROC) (GLuint shader, GLsizei count, const char ** path, const GLint *length);
typedef void (GLAPIENTRY * PFNGLDELETENAMEDSTRINGARBPROC) (GLint namelen, const char* name);
typedef void (GLAPIENTRY * PFNGLGETNAMEDSTRINGARBPROC) (GLint namelen, const char* name, GLsizei bufSize, GLint *stringlen, char *string);
typedef void (GLAPIENTRY * PFNGLGETNAMEDSTRINGIVARBPROC) (GLint namelen, const char* name, GLenum pname, GLint *params);
typedef GLboolean (GLAPIENTRY * PFNGLISNAMEDSTRINGARBPROC) (GLint namelen, const char* name);
typedef void (GLAPIENTRY * PFNGLNAMEDSTRINGARBPROC) (GLenum type, GLint namelen, const char* name, GLint stringlen, const char *string);

#define glCompileShaderIncludeARB GLEW_GET_FUN(__glewCompileShaderIncludeARB)
#define glDeleteNamedStringARB GLEW_GET_FUN(__glewDeleteNamedStringARB)
#define glGetNamedStringARB GLEW_GET_FUN(__glewGetNamedStringARB)
#define glGetNamedStringivARB GLEW_GET_FUN(__glewGetNamedStringivARB)
#define glIsNamedStringARB GLEW_GET_FUN(__glewIsNamedStringARB)
#define glNamedStringARB GLEW_GET_FUN(__glewNamedStringARB)

#define GLEW_ARB_shading_language_include GLEW_GET_VAR(__GLEW_ARB_shading_language_include)

#endif /* GL_ARB_shading_language_include */

/* ----------------------------- GL_ARB_shadow ----------------------------- */

#ifndef GL_ARB_shadow
#define GL_ARB_shadow 1

#define GL_TEXTURE_COMPARE_MODE_ARB 0x884C
#define GL_TEXTURE_COMPARE_FUNC_ARB 0x884D
#define GL_COMPARE_R_TO_TEXTURE_ARB 0x884E

#define GLEW_ARB_shadow GLEW_GET_VAR(__GLEW_ARB_shadow)

#endif /* GL_ARB_shadow */

/* ------------------------- GL_ARB_shadow_ambient ------------------------- */

#ifndef GL_ARB_shadow_ambient
#define GL_ARB_shadow_ambient 1

#define GL_TEXTURE_COMPARE_FAIL_VALUE_ARB 0x80BF

#define GLEW_ARB_shadow_ambient GLEW_GET_VAR(__GLEW_ARB_shadow_ambient)

#endif /* GL_ARB_shadow_ambient */

/* ------------------------------ GL_ARB_sync ------------------------------ */

#ifndef GL_ARB_sync
#define GL_ARB_sync 1

#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#define GL_MAX_SERVER_WAIT_TIMEOUT 0x9111
#define GL_OBJECT_TYPE 0x9112
#define GL_SYNC_CONDITION 0x9113
#define GL_SYNC_STATUS 0x9114
#define GL_SYNC_FLAGS 0x9115
#define GL_SYNC_FENCE 0x9116
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_UNSIGNALED 0x9118
#define GL_SIGNALED 0x9119
#define GL_ALREADY_SIGNALED 0x911A
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_CONDITION_SATISFIED 0x911C
#define GL_WAIT_FAILED 0x911D
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFF

typedef GLenum (GLAPIENTRY * PFNGLCLIENTWAITSYNCPROC) (GLsync GLsync,GLbitfield flags,GLuint64 timeout);
typedef void (GLAPIENTRY * PFNGLDELETESYNCPROC) (GLsync GLsync);
typedef GLsync (GLAPIENTRY * PFNGLFENCESYNCPROC) (GLenum condition,GLbitfield flags);
typedef void (GLAPIENTRY * PFNGLGETINTEGER64VPROC) (GLenum pname, GLint64* params);
typedef void (GLAPIENTRY * PFNGLGETSYNCIVPROC) (GLsync GLsync,GLenum pname,GLsizei bufSize,GLsizei* length, GLint *values);
typedef GLboolean (GLAPIENTRY * PFNGLISSYNCPROC) (GLsync GLsync);
typedef void (GLAPIENTRY * PFNGLWAITSYNCPROC) (GLsync GLsync,GLbitfield flags,GLuint64 timeout);

#define glClientWaitSync GLEW_GET_FUN(__glewClientWaitSync)
#define glDeleteSync GLEW_GET_FUN(__glewDeleteSync)
#define glFenceSync GLEW_GET_FUN(__glewFenceSync)
#define glGetInteger64v GLEW_GET_FUN(__glewGetInteger64v)
#define glGetSynciv GLEW_GET_FUN(__glewGetSynciv)
#define glIsSync GLEW_GET_FUN(__glewIsSync)
#define glWaitSync GLEW_GET_FUN(__glewWaitSync)

#define GLEW_ARB_sync GLEW_GET_VAR(__GLEW_ARB_sync)

#endif /* GL_ARB_sync */

/* ----------------------- GL_ARB_tessellation_shader ---------------------- */

#ifndef GL_ARB_tessellation_shader
#define GL_ARB_tessellation_shader 1

#define GL_PATCHES 0xE
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER 0x84F0
#define GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER 0x84F1
#define GL_MAX_TESS_CONTROL_INPUT_COMPONENTS 0x886C
#define GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS 0x886D
#define GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS 0x8E1E
#define GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS 0x8E1F
#define GL_PATCH_VERTICES 0x8E72
#define GL_PATCH_DEFAULT_INNER_LEVEL 0x8E73
#define GL_PATCH_DEFAULT_OUTER_LEVEL 0x8E74
#define GL_TESS_CONTROL_OUTPUT_VERTICES 0x8E75
#define GL_TESS_GEN_MODE 0x8E76
#define GL_TESS_GEN_SPACING 0x8E77
#define GL_TESS_GEN_VERTEX_ORDER 0x8E78
#define GL_TESS_GEN_POINT_MODE 0x8E79
#define GL_ISOLINES 0x8E7A
#define GL_FRACTIONAL_ODD 0x8E7B
#define GL_FRACTIONAL_EVEN 0x8E7C
#define GL_MAX_PATCH_VERTICES 0x8E7D
#define GL_MAX_TESS_GEN_LEVEL 0x8E7E
#define GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS 0x8E7F
#define GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS 0x8E80
#define GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS 0x8E81
#define GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS 0x8E82
#define GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS 0x8E83
#define GL_MAX_TESS_PATCH_COMPONENTS 0x8E84
#define GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS 0x8E85
#define GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS 0x8E86
#define GL_TESS_EVALUATION_SHADER 0x8E87
#define GL_TESS_CONTROL_SHADER 0x8E88
#define GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS 0x8E89
#define GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS 0x8E8A

typedef void (GLAPIENTRY * PFNGLPATCHPARAMETERFVPROC) (GLenum pname, const GLfloat* values);
typedef void (GLAPIENTRY * PFNGLPATCHPARAMETERIPROC) (GLenum pname, GLint value);

#define glPatchParameterfv GLEW_GET_FUN(__glewPatchParameterfv)
#define glPatchParameteri GLEW_GET_FUN(__glewPatchParameteri)

#define GLEW_ARB_tessellation_shader GLEW_GET_VAR(__GLEW_ARB_tessellation_shader)

#endif /* GL_ARB_tessellation_shader */

/* ---------------------- GL_ARB_texture_border_clamp ---------------------- */

#ifndef GL_ARB_texture_border_clamp
#define GL_ARB_texture_border_clamp 1

#define GL_CLAMP_TO_BORDER_ARB 0x812D

#define GLEW_ARB_texture_border_clamp GLEW_GET_VAR(__GLEW_ARB_texture_border_clamp)

#endif /* GL_ARB_texture_border_clamp */

/* ---------------------- GL_ARB_texture_buffer_object --------------------- */

#ifndef GL_ARB_texture_buffer_object
#define GL_ARB_texture_buffer_object 1

#define GL_TEXTURE_BUFFER_ARB 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE_ARB 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER_ARB 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING_ARB 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT_ARB 0x8C2E

typedef void (GLAPIENTRY * PFNGLTEXBUFFERARBPROC) (GLenum target, GLenum internalformat, GLuint buffer);

#define glTexBufferARB GLEW_GET_FUN(__glewTexBufferARB)

#define GLEW_ARB_texture_buffer_object GLEW_GET_VAR(__GLEW_ARB_texture_buffer_object)

#endif /* GL_ARB_texture_buffer_object */

/* ------------------- GL_ARB_texture_buffer_object_rgb32 ------------------ */

#ifndef GL_ARB_texture_buffer_object_rgb32
#define GL_ARB_texture_buffer_object_rgb32 1

#define GLEW_ARB_texture_buffer_object_rgb32 GLEW_GET_VAR(__GLEW_ARB_texture_buffer_object_rgb32)

#endif /* GL_ARB_texture_buffer_object_rgb32 */

/* ----------------------- GL_ARB_texture_compression ---------------------- */

#ifndef GL_ARB_texture_compression
#define GL_ARB_texture_compression 1

#define GL_COMPRESSED_ALPHA_ARB 0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB 0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB 0x84EB
#define GL_COMPRESSED_INTENSITY_ARB 0x84EC
#define GL_COMPRESSED_RGB_ARB 0x84ED
#define GL_COMPRESSED_RGBA_ARB 0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB 0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB 0x86A0
#define GL_TEXTURE_COMPRESSED_ARB 0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A3

typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE1DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE3DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLGETCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint lod, void* img);

#define glCompressedTexImage1DARB GLEW_GET_FUN(__glewCompressedTexImage1DARB)
#define glCompressedTexImage2DARB GLEW_GET_FUN(__glewCompressedTexImage2DARB)
#define glCompressedTexImage3DARB GLEW_GET_FUN(__glewCompressedTexImage3DARB)
#define glCompressedTexSubImage1DARB GLEW_GET_FUN(__glewCompressedTexSubImage1DARB)
#define glCompressedTexSubImage2DARB GLEW_GET_FUN(__glewCompressedTexSubImage2DARB)
#define glCompressedTexSubImage3DARB GLEW_GET_FUN(__glewCompressedTexSubImage3DARB)
#define glGetCompressedTexImageARB GLEW_GET_FUN(__glewGetCompressedTexImageARB)

#define GLEW_ARB_texture_compression GLEW_GET_VAR(__GLEW_ARB_texture_compression)

#endif /* GL_ARB_texture_compression */

/* -------------------- GL_ARB_texture_compression_bptc -------------------- */

#ifndef GL_ARB_texture_compression_bptc
#define GL_ARB_texture_compression_bptc 1

#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB 0x8E8C
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB 0x8E8D
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB 0x8E8E
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB 0x8E8F

#define GLEW_ARB_texture_compression_bptc GLEW_GET_VAR(__GLEW_ARB_texture_compression_bptc)

#endif /* GL_ARB_texture_compression_bptc */

/* -------------------- GL_ARB_texture_compression_rgtc -------------------- */

#ifndef GL_ARB_texture_compression_rgtc
#define GL_ARB_texture_compression_rgtc 1

#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE

#define GLEW_ARB_texture_compression_rgtc GLEW_GET_VAR(__GLEW_ARB_texture_compression_rgtc)

#endif /* GL_ARB_texture_compression_rgtc */

/* ------------------------ GL_ARB_texture_cube_map ------------------------ */

#ifndef GL_ARB_texture_cube_map
#define GL_ARB_texture_cube_map 1

#define GL_NORMAL_MAP_ARB 0x8511
#define GL_REFLECTION_MAP_ARB 0x8512
#define GL_TEXTURE_CUBE_MAP_ARB 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_ARB 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB 0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARB 0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB 0x851C

#define GLEW_ARB_texture_cube_map GLEW_GET_VAR(__GLEW_ARB_texture_cube_map)

#endif /* GL_ARB_texture_cube_map */

/* --------------------- GL_ARB_texture_cube_map_array --------------------- */

#ifndef GL_ARB_texture_cube_map_array
#define GL_ARB_texture_cube_map_array 1

#define GL_TEXTURE_CUBE_MAP_ARRAY_ARB 0x9009
#define GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_ARB 0x900A
#define GL_PROXY_TEXTURE_CUBE_MAP_ARRAY_ARB 0x900B
#define GL_SAMPLER_CUBE_MAP_ARRAY_ARB 0x900C
#define GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_ARB 0x900D
#define GL_INT_SAMPLER_CUBE_MAP_ARRAY_ARB 0x900E
#define GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_ARB 0x900F

#define GLEW_ARB_texture_cube_map_array GLEW_GET_VAR(__GLEW_ARB_texture_cube_map_array)

#endif /* GL_ARB_texture_cube_map_array */

/* ------------------------- GL_ARB_texture_env_add ------------------------ */

#ifndef GL_ARB_texture_env_add
#define GL_ARB_texture_env_add 1

#define GLEW_ARB_texture_env_add GLEW_GET_VAR(__GLEW_ARB_texture_env_add)

#endif /* GL_ARB_texture_env_add */

/* ----------------------- GL_ARB_texture_env_combine ---------------------- */

#ifndef GL_ARB_texture_env_combine
#define GL_ARB_texture_env_combine 1

#define GL_SUBTRACT_ARB 0x84E7
#define GL_COMBINE_ARB 0x8570
#define GL_COMBINE_RGB_ARB 0x8571
#define GL_COMBINE_ALPHA_ARB 0x8572
#define GL_RGB_SCALE_ARB 0x8573
#define GL_ADD_SIGNED_ARB 0x8574
#define GL_INTERPOLATE_ARB 0x8575
#define GL_CONSTANT_ARB 0x8576
#define GL_PRIMARY_COLOR_ARB 0x8577
#define GL_PREVIOUS_ARB 0x8578
#define GL_SOURCE0_RGB_ARB 0x8580
#define GL_SOURCE1_RGB_ARB 0x8581
#define GL_SOURCE2_RGB_ARB 0x8582
#define GL_SOURCE0_ALPHA_ARB 0x8588
#define GL_SOURCE1_ALPHA_ARB 0x8589
#define GL_SOURCE2_ALPHA_ARB 0x858A
#define GL_OPERAND0_RGB_ARB 0x8590
#define GL_OPERAND1_RGB_ARB 0x8591
#define GL_OPERAND2_RGB_ARB 0x8592
#define GL_OPERAND0_ALPHA_ARB 0x8598
#define GL_OPERAND1_ALPHA_ARB 0x8599
#define GL_OPERAND2_ALPHA_ARB 0x859A

#define GLEW_ARB_texture_env_combine GLEW_GET_VAR(__GLEW_ARB_texture_env_combine)

#endif /* GL_ARB_texture_env_combine */

/* ---------------------- GL_ARB_texture_env_crossbar ---------------------- */

#ifndef GL_ARB_texture_env_crossbar
#define GL_ARB_texture_env_crossbar 1

#define GLEW_ARB_texture_env_crossbar GLEW_GET_VAR(__GLEW_ARB_texture_env_crossbar)

#endif /* GL_ARB_texture_env_crossbar */

/* ------------------------ GL_ARB_texture_env_dot3 ------------------------ */

#ifndef GL_ARB_texture_env_dot3
#define GL_ARB_texture_env_dot3 1

#define GL_DOT3_RGB_ARB 0x86AE
#define GL_DOT3_RGBA_ARB 0x86AF

#define GLEW_ARB_texture_env_dot3 GLEW_GET_VAR(__GLEW_ARB_texture_env_dot3)

#endif /* GL_ARB_texture_env_dot3 */

/* -------------------------- GL_ARB_texture_float ------------------------- */

#ifndef GL_ARB_texture_float
#define GL_ARB_texture_float 1

#define GL_RGBA32F_ARB 0x8814
#define GL_RGB32F_ARB 0x8815
#define GL_ALPHA32F_ARB 0x8816
#define GL_INTENSITY32F_ARB 0x8817
#define GL_LUMINANCE32F_ARB 0x8818
#define GL_LUMINANCE_ALPHA32F_ARB 0x8819
#define GL_RGBA16F_ARB 0x881A
#define GL_RGB16F_ARB 0x881B
#define GL_ALPHA16F_ARB 0x881C
#define GL_INTENSITY16F_ARB 0x881D
#define GL_LUMINANCE16F_ARB 0x881E
#define GL_LUMINANCE_ALPHA16F_ARB 0x881F
#define GL_TEXTURE_RED_TYPE_ARB 0x8C10
#define GL_TEXTURE_GREEN_TYPE_ARB 0x8C11
#define GL_TEXTURE_BLUE_TYPE_ARB 0x8C12
#define GL_TEXTURE_ALPHA_TYPE_ARB 0x8C13
#define GL_TEXTURE_LUMINANCE_TYPE_ARB 0x8C14
#define GL_TEXTURE_INTENSITY_TYPE_ARB 0x8C15
#define GL_TEXTURE_DEPTH_TYPE_ARB 0x8C16
#define GL_UNSIGNED_NORMALIZED_ARB 0x8C17

#define GLEW_ARB_texture_float GLEW_GET_VAR(__GLEW_ARB_texture_float)

#endif /* GL_ARB_texture_float */

/* ------------------------- GL_ARB_texture_gather ------------------------- */

#ifndef GL_ARB_texture_gather
#define GL_ARB_texture_gather 1

#define GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET_ARB 0x8E5E
#define GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET_ARB 0x8E5F
#define GL_MAX_PROGRAM_TEXTURE_GATHER_COMPONENTS_ARB 0x8F9F

#define GLEW_ARB_texture_gather GLEW_GET_VAR(__GLEW_ARB_texture_gather)

#endif /* GL_ARB_texture_gather */

/* --------------------- GL_ARB_texture_mirrored_repeat -------------------- */

#ifndef GL_ARB_texture_mirrored_repeat
#define GL_ARB_texture_mirrored_repeat 1

#define GL_MIRRORED_REPEAT_ARB 0x8370

#define GLEW_ARB_texture_mirrored_repeat GLEW_GET_VAR(__GLEW_ARB_texture_mirrored_repeat)

#endif /* GL_ARB_texture_mirrored_repeat */

/* ----------------------- GL_ARB_texture_multisample ---------------------- */

#ifndef GL_ARB_texture_multisample
#define GL_ARB_texture_multisample 1

#define GL_SAMPLE_POSITION 0x8E50
#define GL_SAMPLE_MASK 0x8E51
#define GL_SAMPLE_MASK_VALUE 0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS 0x8E59
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE 0x9101
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9103
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE 0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY 0x9105
#define GL_TEXTURE_SAMPLES 0x9106
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS 0x9107
#define GL_SAMPLER_2D_MULTISAMPLE 0x9108
#define GL_INT_SAMPLER_2D_MULTISAMPLE 0x9109
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE 0x910A
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910B
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910C
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910D
#define GL_MAX_COLOR_TEXTURE_SAMPLES 0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES 0x910F
#define GL_MAX_INTEGER_SAMPLES 0x9110

typedef void (GLAPIENTRY * PFNGLGETMULTISAMPLEFVPROC) (GLenum pname, GLuint index, GLfloat* val);
typedef void (GLAPIENTRY * PFNGLSAMPLEMASKIPROC) (GLuint index, GLbitfield mask);
typedef void (GLAPIENTRY * PFNGLTEXIMAGE2DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (GLAPIENTRY * PFNGLTEXIMAGE3DMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);

#define glGetMultisamplefv GLEW_GET_FUN(__glewGetMultisamplefv)
#define glSampleMaski GLEW_GET_FUN(__glewSampleMaski)
#define glTexImage2DMultisample GLEW_GET_FUN(__glewTexImage2DMultisample)
#define glTexImage3DMultisample GLEW_GET_FUN(__glewTexImage3DMultisample)

#define GLEW_ARB_texture_multisample GLEW_GET_VAR(__GLEW_ARB_texture_multisample)

#endif /* GL_ARB_texture_multisample */

/* -------------------- GL_ARB_texture_non_power_of_two -------------------- */

#ifndef GL_ARB_texture_non_power_of_two
#define GL_ARB_texture_non_power_of_two 1

#define GLEW_ARB_texture_non_power_of_two GLEW_GET_VAR(__GLEW_ARB_texture_non_power_of_two)

#endif /* GL_ARB_texture_non_power_of_two */

/* ------------------------ GL_ARB_texture_query_lod ----------------------- */

#ifndef GL_ARB_texture_query_lod
#define GL_ARB_texture_query_lod 1

#define GLEW_ARB_texture_query_lod GLEW_GET_VAR(__GLEW_ARB_texture_query_lod)

#endif /* GL_ARB_texture_query_lod */

/* ------------------------ GL_ARB_texture_rectangle ----------------------- */

#ifndef GL_ARB_texture_rectangle
#define GL_ARB_texture_rectangle 1

#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_ARB 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_ARB 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB 0x84F8
#define GL_SAMPLER_2D_RECT_ARB 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB 0x8B64

#define GLEW_ARB_texture_rectangle GLEW_GET_VAR(__GLEW_ARB_texture_rectangle)

#endif /* GL_ARB_texture_rectangle */

/* --------------------------- GL_ARB_texture_rg --------------------------- */

#ifndef GL_ARB_texture_rg
#define GL_ARB_texture_rg 1

#define GL_RED 0x1903
#define GL_COMPRESSED_RED 0x8225
#define GL_COMPRESSED_RG 0x8226
#define GL_RG 0x8227
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_R16 0x822A
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C

#define GLEW_ARB_texture_rg GLEW_GET_VAR(__GLEW_ARB_texture_rg)

#endif /* GL_ARB_texture_rg */

/* ----------------------- GL_ARB_texture_rgb10_a2ui ----------------------- */

#ifndef GL_ARB_texture_rgb10_a2ui
#define GL_ARB_texture_rgb10_a2ui 1

#define GL_RGB10_A2UI 0x906F

#define GLEW_ARB_texture_rgb10_a2ui GLEW_GET_VAR(__GLEW_ARB_texture_rgb10_a2ui)

#endif /* GL_ARB_texture_rgb10_a2ui */

/* ------------------------- GL_ARB_texture_swizzle ------------------------ */

#ifndef GL_ARB_texture_swizzle
#define GL_ARB_texture_swizzle 1

#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46

#define GLEW_ARB_texture_swizzle GLEW_GET_VAR(__GLEW_ARB_texture_swizzle)

#endif /* GL_ARB_texture_swizzle */

/* --------------------------- GL_ARB_timer_query -------------------------- */

#ifndef GL_ARB_timer_query
#define GL_ARB_timer_query 1

#define GL_TIME_ELAPSED 0x88BF
#define GL_TIMESTAMP 0x8E28

typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTI64VPROC) (GLuint id, GLenum pname, GLint64* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTUI64VPROC) (GLuint id, GLenum pname, GLuint64* params);
typedef void (GLAPIENTRY * PFNGLQUERYCOUNTERPROC) (GLuint id, GLenum target);

#define glGetQueryObjecti64v GLEW_GET_FUN(__glewGetQueryObjecti64v)
#define glGetQueryObjectui64v GLEW_GET_FUN(__glewGetQueryObjectui64v)
#define glQueryCounter GLEW_GET_FUN(__glewQueryCounter)

#define GLEW_ARB_timer_query GLEW_GET_VAR(__GLEW_ARB_timer_query)

#endif /* GL_ARB_timer_query */

/* ----------------------- GL_ARB_transform_feedback2 ---------------------- */

#ifndef GL_ARB_transform_feedback2
#define GL_ARB_transform_feedback2 1

#define GL_TRANSFORM_FEEDBACK 0x8E22
#define GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED 0x8E23
#define GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE 0x8E24
#define GL_TRANSFORM_FEEDBACK_BINDING 0x8E25

typedef void (GLAPIENTRY * PFNGLBINDTRANSFORMFEEDBACKPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETETRANSFORMFEEDBACKSPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLDRAWTRANSFORMFEEDBACKPROC) (GLenum mode, GLuint id);
typedef void (GLAPIENTRY * PFNGLGENTRANSFORMFEEDBACKSPROC) (GLsizei n, GLuint* ids);
typedef GLboolean (GLAPIENTRY * PFNGLISTRANSFORMFEEDBACKPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLPAUSETRANSFORMFEEDBACKPROC) (void);
typedef void (GLAPIENTRY * PFNGLRESUMETRANSFORMFEEDBACKPROC) (void);

#define glBindTransformFeedback GLEW_GET_FUN(__glewBindTransformFeedback)
#define glDeleteTransformFeedbacks GLEW_GET_FUN(__glewDeleteTransformFeedbacks)
#define glDrawTransformFeedback GLEW_GET_FUN(__glewDrawTransformFeedback)
#define glGenTransformFeedbacks GLEW_GET_FUN(__glewGenTransformFeedbacks)
#define glIsTransformFeedback GLEW_GET_FUN(__glewIsTransformFeedback)
#define glPauseTransformFeedback GLEW_GET_FUN(__glewPauseTransformFeedback)
#define glResumeTransformFeedback GLEW_GET_FUN(__glewResumeTransformFeedback)

#define GLEW_ARB_transform_feedback2 GLEW_GET_VAR(__GLEW_ARB_transform_feedback2)

#endif /* GL_ARB_transform_feedback2 */

/* ----------------------- GL_ARB_transform_feedback3 ---------------------- */

#ifndef GL_ARB_transform_feedback3
#define GL_ARB_transform_feedback3 1

#define GL_MAX_TRANSFORM_FEEDBACK_BUFFERS 0x8E70
#define GL_MAX_VERTEX_STREAMS 0x8E71

typedef void (GLAPIENTRY * PFNGLBEGINQUERYINDEXEDPROC) (GLenum target, GLuint index, GLuint id);
typedef void (GLAPIENTRY * PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC) (GLenum mode, GLuint id, GLuint stream);
typedef void (GLAPIENTRY * PFNGLENDQUERYINDEXEDPROC) (GLenum target, GLuint index);
typedef void (GLAPIENTRY * PFNGLGETQUERYINDEXEDIVPROC) (GLenum target, GLuint index, GLenum pname, GLint* params);

#define glBeginQueryIndexed GLEW_GET_FUN(__glewBeginQueryIndexed)
#define glDrawTransformFeedbackStream GLEW_GET_FUN(__glewDrawTransformFeedbackStream)
#define glEndQueryIndexed GLEW_GET_FUN(__glewEndQueryIndexed)
#define glGetQueryIndexediv GLEW_GET_FUN(__glewGetQueryIndexediv)

#define GLEW_ARB_transform_feedback3 GLEW_GET_VAR(__GLEW_ARB_transform_feedback3)

#endif /* GL_ARB_transform_feedback3 */

/* ------------------------ GL_ARB_transpose_matrix ------------------------ */

#ifndef GL_ARB_transpose_matrix
#define GL_ARB_transpose_matrix 1

#define GL_TRANSPOSE_MODELVIEW_MATRIX_ARB 0x84E3
#define GL_TRANSPOSE_PROJECTION_MATRIX_ARB 0x84E4
#define GL_TRANSPOSE_TEXTURE_MATRIX_ARB 0x84E5
#define GL_TRANSPOSE_COLOR_MATRIX_ARB 0x84E6

typedef void (GLAPIENTRY * PFNGLLOADTRANSPOSEMATRIXDARBPROC) (GLdouble m[16]);
typedef void (GLAPIENTRY * PFNGLLOADTRANSPOSEMATRIXFARBPROC) (GLfloat m[16]);
typedef void (GLAPIENTRY * PFNGLMULTTRANSPOSEMATRIXDARBPROC) (GLdouble m[16]);
typedef void (GLAPIENTRY * PFNGLMULTTRANSPOSEMATRIXFARBPROC) (GLfloat m[16]);

#define glLoadTransposeMatrixdARB GLEW_GET_FUN(__glewLoadTransposeMatrixdARB)
#define glLoadTransposeMatrixfARB GLEW_GET_FUN(__glewLoadTransposeMatrixfARB)
#define glMultTransposeMatrixdARB GLEW_GET_FUN(__glewMultTransposeMatrixdARB)
#define glMultTransposeMatrixfARB GLEW_GET_FUN(__glewMultTransposeMatrixfARB)

#define GLEW_ARB_transpose_matrix GLEW_GET_VAR(__GLEW_ARB_transpose_matrix)

#endif /* GL_ARB_transpose_matrix */

/* ---------------------- GL_ARB_uniform_buffer_object --------------------- */

#ifndef GL_ARB_uniform_buffer_object
#define GL_ARB_uniform_buffer_object 1

#define GL_UNIFORM_BUFFER 0x8A11
#define GL_UNIFORM_BUFFER_BINDING 0x8A28
#define GL_UNIFORM_BUFFER_START 0x8A29
#define GL_UNIFORM_BUFFER_SIZE 0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS 0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS 0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS 0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS 0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS 0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE 0x8A30
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS 0x8A31
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS 0x8A32
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS 0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH 0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS 0x8A36
#define GL_UNIFORM_TYPE 0x8A37
#define GL_UNIFORM_SIZE 0x8A38
#define GL_UNIFORM_NAME_LENGTH 0x8A39
#define GL_UNIFORM_BLOCK_INDEX 0x8A3A
#define GL_UNIFORM_OFFSET 0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE 0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE 0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR 0x8A3E
#define GL_UNIFORM_BLOCK_BINDING 0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE 0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH 0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS 0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES 0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER 0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER 0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER 0x8A46
#define GL_INVALID_INDEX 0xFFFFFFFF

typedef void (GLAPIENTRY * PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) (GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, char* uniformBlockName);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMNAMEPROC) (GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, char* uniformName);
typedef void (GLAPIENTRY * PFNGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETINTEGERI_VPROC) (GLenum target, GLuint index, GLint* data);
typedef GLuint (GLAPIENTRY * PFNGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const char* uniformBlockName);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMINDICESPROC) (GLuint program, GLsizei uniformCount, const char** uniformNames, GLuint* uniformIndices);
typedef void (GLAPIENTRY * PFNGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

#define glBindBufferBase GLEW_GET_FUN(__glewBindBufferBase)
#define glBindBufferRange GLEW_GET_FUN(__glewBindBufferRange)
#define glGetActiveUniformBlockName GLEW_GET_FUN(__glewGetActiveUniformBlockName)
#define glGetActiveUniformBlockiv GLEW_GET_FUN(__glewGetActiveUniformBlockiv)
#define glGetActiveUniformName GLEW_GET_FUN(__glewGetActiveUniformName)
#define glGetActiveUniformsiv GLEW_GET_FUN(__glewGetActiveUniformsiv)
#define glGetIntegeri_v GLEW_GET_FUN(__glewGetIntegeri_v)
#define glGetUniformBlockIndex GLEW_GET_FUN(__glewGetUniformBlockIndex)
#define glGetUniformIndices GLEW_GET_FUN(__glewGetUniformIndices)
#define glUniformBlockBinding GLEW_GET_FUN(__glewUniformBlockBinding)

#define GLEW_ARB_uniform_buffer_object GLEW_GET_VAR(__GLEW_ARB_uniform_buffer_object)

#endif /* GL_ARB_uniform_buffer_object */

/* ------------------------ GL_ARB_vertex_array_bgra ----------------------- */

#ifndef GL_ARB_vertex_array_bgra
#define GL_ARB_vertex_array_bgra 1

#define GL_BGRA 0x80E1

#define GLEW_ARB_vertex_array_bgra GLEW_GET_VAR(__GLEW_ARB_vertex_array_bgra)

#endif /* GL_ARB_vertex_array_bgra */

/* ----------------------- GL_ARB_vertex_array_object ---------------------- */

#ifndef GL_ARB_vertex_array_object
#define GL_ARB_vertex_array_object 1

#define GL_VERTEX_ARRAY_BINDING 0x85B5

typedef void (GLAPIENTRY * PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (GLAPIENTRY * PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint* arrays);
typedef void (GLAPIENTRY * PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint* arrays);
typedef GLboolean (GLAPIENTRY * PFNGLISVERTEXARRAYPROC) (GLuint array);

#define glBindVertexArray GLEW_GET_FUN(__glewBindVertexArray)
#define glDeleteVertexArrays GLEW_GET_FUN(__glewDeleteVertexArrays)
#define glGenVertexArrays GLEW_GET_FUN(__glewGenVertexArrays)
#define glIsVertexArray GLEW_GET_FUN(__glewIsVertexArray)

#define GLEW_ARB_vertex_array_object GLEW_GET_VAR(__GLEW_ARB_vertex_array_object)

#endif /* GL_ARB_vertex_array_object */

/* ----------------------- GL_ARB_vertex_attrib_64bit ---------------------- */

#ifndef GL_ARB_vertex_attrib_64bit
#define GL_ARB_vertex_attrib_64bit 1

#define GL_DOUBLE_MAT2 0x8F46
#define GL_DOUBLE_MAT3 0x8F47
#define GL_DOUBLE_MAT4 0x8F48
#define GL_DOUBLE_VEC2 0x8FFC
#define GL_DOUBLE_VEC3 0x8FFD
#define GL_DOUBLE_VEC4 0x8FFE

typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBLDVPROC) (GLuint index, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1DPROC) (GLuint index, GLdouble x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2DPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4DPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4DVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBLPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);

#define glGetVertexAttribLdv GLEW_GET_FUN(__glewGetVertexAttribLdv)
#define glVertexAttribL1d GLEW_GET_FUN(__glewVertexAttribL1d)
#define glVertexAttribL1dv GLEW_GET_FUN(__glewVertexAttribL1dv)
#define glVertexAttribL2d GLEW_GET_FUN(__glewVertexAttribL2d)
#define glVertexAttribL2dv GLEW_GET_FUN(__glewVertexAttribL2dv)
#define glVertexAttribL3d GLEW_GET_FUN(__glewVertexAttribL3d)
#define glVertexAttribL3dv GLEW_GET_FUN(__glewVertexAttribL3dv)
#define glVertexAttribL4d GLEW_GET_FUN(__glewVertexAttribL4d)
#define glVertexAttribL4dv GLEW_GET_FUN(__glewVertexAttribL4dv)
#define glVertexAttribLPointer GLEW_GET_FUN(__glewVertexAttribLPointer)

#define GLEW_ARB_vertex_attrib_64bit GLEW_GET_VAR(__GLEW_ARB_vertex_attrib_64bit)

#endif /* GL_ARB_vertex_attrib_64bit */

/* -------------------------- GL_ARB_vertex_blend -------------------------- */

#ifndef GL_ARB_vertex_blend
#define GL_ARB_vertex_blend 1

#define GL_MODELVIEW0_ARB 0x1700
#define GL_MODELVIEW1_ARB 0x850A
#define GL_MAX_VERTEX_UNITS_ARB 0x86A4
#define GL_ACTIVE_VERTEX_UNITS_ARB 0x86A5
#define GL_WEIGHT_SUM_UNITY_ARB 0x86A6
#define GL_VERTEX_BLEND_ARB 0x86A7
#define GL_CURRENT_WEIGHT_ARB 0x86A8
#define GL_WEIGHT_ARRAY_TYPE_ARB 0x86A9
#define GL_WEIGHT_ARRAY_STRIDE_ARB 0x86AA
#define GL_WEIGHT_ARRAY_SIZE_ARB 0x86AB
#define GL_WEIGHT_ARRAY_POINTER_ARB 0x86AC
#define GL_WEIGHT_ARRAY_ARB 0x86AD
#define GL_MODELVIEW2_ARB 0x8722
#define GL_MODELVIEW3_ARB 0x8723
#define GL_MODELVIEW4_ARB 0x8724
#define GL_MODELVIEW5_ARB 0x8725
#define GL_MODELVIEW6_ARB 0x8726
#define GL_MODELVIEW7_ARB 0x8727
#define GL_MODELVIEW8_ARB 0x8728
#define GL_MODELVIEW9_ARB 0x8729
#define GL_MODELVIEW10_ARB 0x872A
#define GL_MODELVIEW11_ARB 0x872B
#define GL_MODELVIEW12_ARB 0x872C
#define GL_MODELVIEW13_ARB 0x872D
#define GL_MODELVIEW14_ARB 0x872E
#define GL_MODELVIEW15_ARB 0x872F
#define GL_MODELVIEW16_ARB 0x8730
#define GL_MODELVIEW17_ARB 0x8731
#define GL_MODELVIEW18_ARB 0x8732
#define GL_MODELVIEW19_ARB 0x8733
#define GL_MODELVIEW20_ARB 0x8734
#define GL_MODELVIEW21_ARB 0x8735
#define GL_MODELVIEW22_ARB 0x8736
#define GL_MODELVIEW23_ARB 0x8737
#define GL_MODELVIEW24_ARB 0x8738
#define GL_MODELVIEW25_ARB 0x8739
#define GL_MODELVIEW26_ARB 0x873A
#define GL_MODELVIEW27_ARB 0x873B
#define GL_MODELVIEW28_ARB 0x873C
#define GL_MODELVIEW29_ARB 0x873D
#define GL_MODELVIEW30_ARB 0x873E
#define GL_MODELVIEW31_ARB 0x873F

typedef void (GLAPIENTRY * PFNGLVERTEXBLENDARBPROC) (GLint count);
typedef void (GLAPIENTRY * PFNGLWEIGHTPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (GLAPIENTRY * PFNGLWEIGHTBVARBPROC) (GLint size, GLbyte *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTDVARBPROC) (GLint size, GLdouble *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTFVARBPROC) (GLint size, GLfloat *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTIVARBPROC) (GLint size, GLint *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTSVARBPROC) (GLint size, GLshort *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTUBVARBPROC) (GLint size, GLubyte *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTUIVARBPROC) (GLint size, GLuint *weights);
typedef void (GLAPIENTRY * PFNGLWEIGHTUSVARBPROC) (GLint size, GLushort *weights);

#define glVertexBlendARB GLEW_GET_FUN(__glewVertexBlendARB)
#define glWeightPointerARB GLEW_GET_FUN(__glewWeightPointerARB)
#define glWeightbvARB GLEW_GET_FUN(__glewWeightbvARB)
#define glWeightdvARB GLEW_GET_FUN(__glewWeightdvARB)
#define glWeightfvARB GLEW_GET_FUN(__glewWeightfvARB)
#define glWeightivARB GLEW_GET_FUN(__glewWeightivARB)
#define glWeightsvARB GLEW_GET_FUN(__glewWeightsvARB)
#define glWeightubvARB GLEW_GET_FUN(__glewWeightubvARB)
#define glWeightuivARB GLEW_GET_FUN(__glewWeightuivARB)
#define glWeightusvARB GLEW_GET_FUN(__glewWeightusvARB)

#define GLEW_ARB_vertex_blend GLEW_GET_VAR(__GLEW_ARB_vertex_blend)

#endif /* GL_ARB_vertex_blend */

/* ---------------------- GL_ARB_vertex_buffer_object ---------------------- */

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1

#define GL_BUFFER_SIZE_ARB 0x8764
#define GL_BUFFER_USAGE_ARB 0x8765
#define GL_ARRAY_BUFFER_ARB 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB 0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GL_READ_ONLY_ARB 0x88B8
#define GL_WRITE_ONLY_ARB 0x88B9
#define GL_READ_WRITE_ARB 0x88BA
#define GL_BUFFER_ACCESS_ARB 0x88BB
#define GL_BUFFER_MAPPED_ARB 0x88BC
#define GL_BUFFER_MAP_POINTER_ARB 0x88BD
#define GL_STREAM_DRAW_ARB 0x88E0
#define GL_STREAM_READ_ARB 0x88E1
#define GL_STREAM_COPY_ARB 0x88E2
#define GL_STATIC_DRAW_ARB 0x88E4
#define GL_STATIC_READ_ARB 0x88E5
#define GL_STATIC_COPY_ARB 0x88E6
#define GL_DYNAMIC_DRAW_ARB 0x88E8
#define GL_DYNAMIC_READ_ARB 0x88E9
#define GL_DYNAMIC_COPY_ARB 0x88EA

typedef ptrdiff_t GLsizeiptrARB;
typedef ptrdiff_t GLintptrARB;

typedef void (GLAPIENTRY * PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const GLvoid* data, GLenum usage);
typedef void (GLAPIENTRY * PFNGLBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid* data);
typedef void (GLAPIENTRY * PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint* buffers);
typedef void (GLAPIENTRY * PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint* buffers);
typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERIVARBPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETBUFFERPOINTERVARBPROC) (GLenum target, GLenum pname, GLvoid** params);
typedef void (GLAPIENTRY * PFNGLGETBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid* data);
typedef GLboolean (GLAPIENTRY * PFNGLISBUFFERARBPROC) (GLuint buffer);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (GLAPIENTRY * PFNGLUNMAPBUFFERARBPROC) (GLenum target);

#define glBindBufferARB GLEW_GET_FUN(__glewBindBufferARB)
#define glBufferDataARB GLEW_GET_FUN(__glewBufferDataARB)
#define glBufferSubDataARB GLEW_GET_FUN(__glewBufferSubDataARB)
#define glDeleteBuffersARB GLEW_GET_FUN(__glewDeleteBuffersARB)
#define glGenBuffersARB GLEW_GET_FUN(__glewGenBuffersARB)
#define glGetBufferParameterivARB GLEW_GET_FUN(__glewGetBufferParameterivARB)
#define glGetBufferPointervARB GLEW_GET_FUN(__glewGetBufferPointervARB)
#define glGetBufferSubDataARB GLEW_GET_FUN(__glewGetBufferSubDataARB)
#define glIsBufferARB GLEW_GET_FUN(__glewIsBufferARB)
#define glMapBufferARB GLEW_GET_FUN(__glewMapBufferARB)
#define glUnmapBufferARB GLEW_GET_FUN(__glewUnmapBufferARB)

#define GLEW_ARB_vertex_buffer_object GLEW_GET_VAR(__GLEW_ARB_vertex_buffer_object)

#endif /* GL_ARB_vertex_buffer_object */

/* ------------------------- GL_ARB_vertex_program ------------------------- */

#ifndef GL_ARB_vertex_program
#define GL_ARB_vertex_program 1

#define GL_COLOR_SUM_ARB 0x8458
#define GL_VERTEX_PROGRAM_ARB 0x8620
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB 0x8625
#define GL_CURRENT_VERTEX_ATTRIB_ARB 0x8626
#define GL_PROGRAM_LENGTH_ARB 0x8627
#define GL_PROGRAM_STRING_ARB 0x8628
#define GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB 0x862E
#define GL_MAX_PROGRAM_MATRICES_ARB 0x862F
#define GL_CURRENT_MATRIX_STACK_DEPTH_ARB 0x8640
#define GL_CURRENT_MATRIX_ARB 0x8641
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB 0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB 0x8643
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB 0x8645
#define GL_PROGRAM_ERROR_POSITION_ARB 0x864B
#define GL_PROGRAM_BINDING_ARB 0x8677
#define GL_MAX_VERTEX_ATTRIBS_ARB 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB 0x886A
#define GL_PROGRAM_ERROR_STRING_ARB 0x8874
#define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#define GL_PROGRAM_FORMAT_ARB 0x8876
#define GL_PROGRAM_INSTRUCTIONS_ARB 0x88A0
#define GL_MAX_PROGRAM_INSTRUCTIONS_ARB 0x88A1
#define GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A2
#define GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A3
#define GL_PROGRAM_TEMPORARIES_ARB 0x88A4
#define GL_MAX_PROGRAM_TEMPORARIES_ARB 0x88A5
#define GL_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A6
#define GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A7
#define GL_PROGRAM_PARAMETERS_ARB 0x88A8
#define GL_MAX_PROGRAM_PARAMETERS_ARB 0x88A9
#define GL_PROGRAM_NATIVE_PARAMETERS_ARB 0x88AA
#define GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB 0x88AB
#define GL_PROGRAM_ATTRIBS_ARB 0x88AC
#define GL_MAX_PROGRAM_ATTRIBS_ARB 0x88AD
#define GL_PROGRAM_NATIVE_ATTRIBS_ARB 0x88AE
#define GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB 0x88AF
#define GL_PROGRAM_ADDRESS_REGISTERS_ARB 0x88B0
#define GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB 0x88B1
#define GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B2
#define GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B3
#define GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB 0x88B4
#define GL_MAX_PROGRAM_ENV_PARAMETERS_ARB 0x88B5
#define GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB 0x88B6
#define GL_TRANSPOSE_CURRENT_MATRIX_ARB 0x88B7
#define GL_MATRIX0_ARB 0x88C0
#define GL_MATRIX1_ARB 0x88C1
#define GL_MATRIX2_ARB 0x88C2
#define GL_MATRIX3_ARB 0x88C3
#define GL_MATRIX4_ARB 0x88C4
#define GL_MATRIX5_ARB 0x88C5
#define GL_MATRIX6_ARB 0x88C6
#define GL_MATRIX7_ARB 0x88C7
#define GL_MATRIX8_ARB 0x88C8
#define GL_MATRIX9_ARB 0x88C9
#define GL_MATRIX10_ARB 0x88CA
#define GL_MATRIX11_ARB 0x88CB
#define GL_MATRIX12_ARB 0x88CC
#define GL_MATRIX13_ARB 0x88CD
#define GL_MATRIX14_ARB 0x88CE
#define GL_MATRIX15_ARB 0x88CF
#define GL_MATRIX16_ARB 0x88D0
#define GL_MATRIX17_ARB 0x88D1
#define GL_MATRIX18_ARB 0x88D2
#define GL_MATRIX19_ARB 0x88D3
#define GL_MATRIX20_ARB 0x88D4
#define GL_MATRIX21_ARB 0x88D5
#define GL_MATRIX22_ARB 0x88D6
#define GL_MATRIX23_ARB 0x88D7
#define GL_MATRIX24_ARB 0x88D8
#define GL_MATRIX25_ARB 0x88D9
#define GL_MATRIX26_ARB 0x88DA
#define GL_MATRIX27_ARB 0x88DB
#define GL_MATRIX28_ARB 0x88DC
#define GL_MATRIX29_ARB 0x88DD
#define GL_MATRIX30_ARB 0x88DE
#define GL_MATRIX31_ARB 0x88DF

typedef void (GLAPIENTRY * PFNGLBINDPROGRAMARBPROC) (GLenum target, GLuint program);
typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMSARBPROC) (GLsizei n, const GLuint* programs);
typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (GLAPIENTRY * PFNGLGENPROGRAMSARBPROC) (GLsizei n, GLuint* programs);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMENVPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMENVPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMSTRINGARBPROC) (GLenum target, GLenum pname, void* string);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVARBPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBPOINTERVARBPROC) (GLuint index, GLenum pname, GLvoid** pointer);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBDVARBPROC) (GLuint index, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVARBPROC) (GLuint index, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVARBPROC) (GLuint index, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISPROGRAMARBPROC) (GLuint program);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) (GLenum target, GLuint index, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) (GLenum target, GLuint index, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMSTRINGARBPROC) (GLenum target, GLenum format, GLsizei len, const void* string);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DARBPROC) (GLuint index, GLdouble x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DVARBPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FARBPROC) (GLuint index, GLfloat x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FVARBPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SARBPROC) (GLuint index, GLshort x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SVARBPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DARBPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DVARBPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FARBPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FVARBPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SARBPROC) (GLuint index, GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SVARBPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DVARBPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FVARBPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SVARBPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NBVARBPROC) (GLuint index, const GLbyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NIVARBPROC) (GLuint index, const GLint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NSVARBPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUBARBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUBVARBPROC) (GLuint index, const GLubyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUIVARBPROC) (GLuint index, const GLuint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4NUSVARBPROC) (GLuint index, const GLushort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4BVARBPROC) (GLuint index, const GLbyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DVARBPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FVARBPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4IVARBPROC) (GLuint index, const GLint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SVARBPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UBVARBPROC) (GLuint index, const GLubyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UIVARBPROC) (GLuint index, const GLuint* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4USVARBPROC) (GLuint index, const GLushort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);

#define glBindProgramARB GLEW_GET_FUN(__glewBindProgramARB)
#define glDeleteProgramsARB GLEW_GET_FUN(__glewDeleteProgramsARB)
#define glDisableVertexAttribArrayARB GLEW_GET_FUN(__glewDisableVertexAttribArrayARB)
#define glEnableVertexAttribArrayARB GLEW_GET_FUN(__glewEnableVertexAttribArrayARB)
#define glGenProgramsARB GLEW_GET_FUN(__glewGenProgramsARB)
#define glGetProgramEnvParameterdvARB GLEW_GET_FUN(__glewGetProgramEnvParameterdvARB)
#define glGetProgramEnvParameterfvARB GLEW_GET_FUN(__glewGetProgramEnvParameterfvARB)
#define glGetProgramLocalParameterdvARB GLEW_GET_FUN(__glewGetProgramLocalParameterdvARB)
#define glGetProgramLocalParameterfvARB GLEW_GET_FUN(__glewGetProgramLocalParameterfvARB)
#define glGetProgramStringARB GLEW_GET_FUN(__glewGetProgramStringARB)
#define glGetProgramivARB GLEW_GET_FUN(__glewGetProgramivARB)
#define glGetVertexAttribPointervARB GLEW_GET_FUN(__glewGetVertexAttribPointervARB)
#define glGetVertexAttribdvARB GLEW_GET_FUN(__glewGetVertexAttribdvARB)
#define glGetVertexAttribfvARB GLEW_GET_FUN(__glewGetVertexAttribfvARB)
#define glGetVertexAttribivARB GLEW_GET_FUN(__glewGetVertexAttribivARB)
#define glIsProgramARB GLEW_GET_FUN(__glewIsProgramARB)
#define glProgramEnvParameter4dARB GLEW_GET_FUN(__glewProgramEnvParameter4dARB)
#define glProgramEnvParameter4dvARB GLEW_GET_FUN(__glewProgramEnvParameter4dvARB)
#define glProgramEnvParameter4fARB GLEW_GET_FUN(__glewProgramEnvParameter4fARB)
#define glProgramEnvParameter4fvARB GLEW_GET_FUN(__glewProgramEnvParameter4fvARB)
#define glProgramLocalParameter4dARB GLEW_GET_FUN(__glewProgramLocalParameter4dARB)
#define glProgramLocalParameter4dvARB GLEW_GET_FUN(__glewProgramLocalParameter4dvARB)
#define glProgramLocalParameter4fARB GLEW_GET_FUN(__glewProgramLocalParameter4fARB)
#define glProgramLocalParameter4fvARB GLEW_GET_FUN(__glewProgramLocalParameter4fvARB)
#define glProgramStringARB GLEW_GET_FUN(__glewProgramStringARB)
#define glVertexAttrib1dARB GLEW_GET_FUN(__glewVertexAttrib1dARB)
#define glVertexAttrib1dvARB GLEW_GET_FUN(__glewVertexAttrib1dvARB)
#define glVertexAttrib1fARB GLEW_GET_FUN(__glewVertexAttrib1fARB)
#define glVertexAttrib1fvARB GLEW_GET_FUN(__glewVertexAttrib1fvARB)
#define glVertexAttrib1sARB GLEW_GET_FUN(__glewVertexAttrib1sARB)
#define glVertexAttrib1svARB GLEW_GET_FUN(__glewVertexAttrib1svARB)
#define glVertexAttrib2dARB GLEW_GET_FUN(__glewVertexAttrib2dARB)
#define glVertexAttrib2dvARB GLEW_GET_FUN(__glewVertexAttrib2dvARB)
#define glVertexAttrib2fARB GLEW_GET_FUN(__glewVertexAttrib2fARB)
#define glVertexAttrib2fvARB GLEW_GET_FUN(__glewVertexAttrib2fvARB)
#define glVertexAttrib2sARB GLEW_GET_FUN(__glewVertexAttrib2sARB)
#define glVertexAttrib2svARB GLEW_GET_FUN(__glewVertexAttrib2svARB)
#define glVertexAttrib3dARB GLEW_GET_FUN(__glewVertexAttrib3dARB)
#define glVertexAttrib3dvARB GLEW_GET_FUN(__glewVertexAttrib3dvARB)
#define glVertexAttrib3fARB GLEW_GET_FUN(__glewVertexAttrib3fARB)
#define glVertexAttrib3fvARB GLEW_GET_FUN(__glewVertexAttrib3fvARB)
#define glVertexAttrib3sARB GLEW_GET_FUN(__glewVertexAttrib3sARB)
#define glVertexAttrib3svARB GLEW_GET_FUN(__glewVertexAttrib3svARB)
#define glVertexAttrib4NbvARB GLEW_GET_FUN(__glewVertexAttrib4NbvARB)
#define glVertexAttrib4NivARB GLEW_GET_FUN(__glewVertexAttrib4NivARB)
#define glVertexAttrib4NsvARB GLEW_GET_FUN(__glewVertexAttrib4NsvARB)
#define glVertexAttrib4NubARB GLEW_GET_FUN(__glewVertexAttrib4NubARB)
#define glVertexAttrib4NubvARB GLEW_GET_FUN(__glewVertexAttrib4NubvARB)
#define glVertexAttrib4NuivARB GLEW_GET_FUN(__glewVertexAttrib4NuivARB)
#define glVertexAttrib4NusvARB GLEW_GET_FUN(__glewVertexAttrib4NusvARB)
#define glVertexAttrib4bvARB GLEW_GET_FUN(__glewVertexAttrib4bvARB)
#define glVertexAttrib4dARB GLEW_GET_FUN(__glewVertexAttrib4dARB)
#define glVertexAttrib4dvARB GLEW_GET_FUN(__glewVertexAttrib4dvARB)
#define glVertexAttrib4fARB GLEW_GET_FUN(__glewVertexAttrib4fARB)
#define glVertexAttrib4fvARB GLEW_GET_FUN(__glewVertexAttrib4fvARB)
#define glVertexAttrib4ivARB GLEW_GET_FUN(__glewVertexAttrib4ivARB)
#define glVertexAttrib4sARB GLEW_GET_FUN(__glewVertexAttrib4sARB)
#define glVertexAttrib4svARB GLEW_GET_FUN(__glewVertexAttrib4svARB)
#define glVertexAttrib4ubvARB GLEW_GET_FUN(__glewVertexAttrib4ubvARB)
#define glVertexAttrib4uivARB GLEW_GET_FUN(__glewVertexAttrib4uivARB)
#define glVertexAttrib4usvARB GLEW_GET_FUN(__glewVertexAttrib4usvARB)
#define glVertexAttribPointerARB GLEW_GET_FUN(__glewVertexAttribPointerARB)

#define GLEW_ARB_vertex_program GLEW_GET_VAR(__GLEW_ARB_vertex_program)

#endif /* GL_ARB_vertex_program */

/* -------------------------- GL_ARB_vertex_shader ------------------------- */

#ifndef GL_ARB_vertex_shader
#define GL_ARB_vertex_shader 1

#define GL_VERTEX_SHADER_ARB 0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB 0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB 0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A

typedef void (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONARBPROC) (GLhandleARB programObj, GLuint index, const GLcharARB* name);
typedef void (GLAPIENTRY * PFNGLGETACTIVEATTRIBARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei* length, GLint *size, GLenum *type, GLcharARB *name);
typedef GLint (GLAPIENTRY * PFNGLGETATTRIBLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB* name);

#define glBindAttribLocationARB GLEW_GET_FUN(__glewBindAttribLocationARB)
#define glGetActiveAttribARB GLEW_GET_FUN(__glewGetActiveAttribARB)
#define glGetAttribLocationARB GLEW_GET_FUN(__glewGetAttribLocationARB)

#define GLEW_ARB_vertex_shader GLEW_GET_VAR(__GLEW_ARB_vertex_shader)

#endif /* GL_ARB_vertex_shader */

/* ------------------- GL_ARB_vertex_type_2_10_10_10_rev ------------------- */

#ifndef GL_ARB_vertex_type_2_10_10_10_rev
#define GL_ARB_vertex_type_2_10_10_10_rev 1

#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_INT_2_10_10_10_REV 0x8D9F

typedef void (GLAPIENTRY * PFNGLCOLORP3UIPROC) (GLenum type, GLuint color);
typedef void (GLAPIENTRY * PFNGLCOLORP3UIVPROC) (GLenum type, const GLuint* color);
typedef void (GLAPIENTRY * PFNGLCOLORP4UIPROC) (GLenum type, GLuint color);
typedef void (GLAPIENTRY * PFNGLCOLORP4UIVPROC) (GLenum type, const GLuint* color);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP1UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP1UIVPROC) (GLenum texture, GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP2UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP2UIVPROC) (GLenum texture, GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP3UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP3UIVPROC) (GLenum texture, GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP4UIPROC) (GLenum texture, GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDP4UIVPROC) (GLenum texture, GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLNORMALP3UIPROC) (GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLNORMALP3UIVPROC) (GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORP3UIPROC) (GLenum type, GLuint color);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORP3UIVPROC) (GLenum type, const GLuint* color);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP1UIPROC) (GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP1UIVPROC) (GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP2UIPROC) (GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP2UIVPROC) (GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP3UIPROC) (GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP3UIVPROC) (GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP4UIPROC) (GLenum type, GLuint coords);
typedef void (GLAPIENTRY * PFNGLTEXCOORDP4UIVPROC) (GLenum type, const GLuint* coords);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP1UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP1UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP2UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP2UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP3UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP3UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP4UIPROC) (GLuint index, GLenum type, GLboolean normalized, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBP4UIVPROC) (GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXP2UIPROC) (GLenum type, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXP2UIVPROC) (GLenum type, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXP3UIPROC) (GLenum type, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXP3UIVPROC) (GLenum type, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLVERTEXP4UIPROC) (GLenum type, GLuint value);
typedef void (GLAPIENTRY * PFNGLVERTEXP4UIVPROC) (GLenum type, const GLuint* value);

#define glColorP3ui GLEW_GET_FUN(__glewColorP3ui)
#define glColorP3uiv GLEW_GET_FUN(__glewColorP3uiv)
#define glColorP4ui GLEW_GET_FUN(__glewColorP4ui)
#define glColorP4uiv GLEW_GET_FUN(__glewColorP4uiv)
#define glMultiTexCoordP1ui GLEW_GET_FUN(__glewMultiTexCoordP1ui)
#define glMultiTexCoordP1uiv GLEW_GET_FUN(__glewMultiTexCoordP1uiv)
#define glMultiTexCoordP2ui GLEW_GET_FUN(__glewMultiTexCoordP2ui)
#define glMultiTexCoordP2uiv GLEW_GET_FUN(__glewMultiTexCoordP2uiv)
#define glMultiTexCoordP3ui GLEW_GET_FUN(__glewMultiTexCoordP3ui)
#define glMultiTexCoordP3uiv GLEW_GET_FUN(__glewMultiTexCoordP3uiv)
#define glMultiTexCoordP4ui GLEW_GET_FUN(__glewMultiTexCoordP4ui)
#define glMultiTexCoordP4uiv GLEW_GET_FUN(__glewMultiTexCoordP4uiv)
#define glNormalP3ui GLEW_GET_FUN(__glewNormalP3ui)
#define glNormalP3uiv GLEW_GET_FUN(__glewNormalP3uiv)
#define glSecondaryColorP3ui GLEW_GET_FUN(__glewSecondaryColorP3ui)
#define glSecondaryColorP3uiv GLEW_GET_FUN(__glewSecondaryColorP3uiv)
#define glTexCoordP1ui GLEW_GET_FUN(__glewTexCoordP1ui)
#define glTexCoordP1uiv GLEW_GET_FUN(__glewTexCoordP1uiv)
#define glTexCoordP2ui GLEW_GET_FUN(__glewTexCoordP2ui)
#define glTexCoordP2uiv GLEW_GET_FUN(__glewTexCoordP2uiv)
#define glTexCoordP3ui GLEW_GET_FUN(__glewTexCoordP3ui)
#define glTexCoordP3uiv GLEW_GET_FUN(__glewTexCoordP3uiv)
#define glTexCoordP4ui GLEW_GET_FUN(__glewTexCoordP4ui)
#define glTexCoordP4uiv GLEW_GET_FUN(__glewTexCoordP4uiv)
#define glVertexAttribP1ui GLEW_GET_FUN(__glewVertexAttribP1ui)
#define glVertexAttribP1uiv GLEW_GET_FUN(__glewVertexAttribP1uiv)
#define glVertexAttribP2ui GLEW_GET_FUN(__glewVertexAttribP2ui)
#define glVertexAttribP2uiv GLEW_GET_FUN(__glewVertexAttribP2uiv)
#define glVertexAttribP3ui GLEW_GET_FUN(__glewVertexAttribP3ui)
#define glVertexAttribP3uiv GLEW_GET_FUN(__glewVertexAttribP3uiv)
#define glVertexAttribP4ui GLEW_GET_FUN(__glewVertexAttribP4ui)
#define glVertexAttribP4uiv GLEW_GET_FUN(__glewVertexAttribP4uiv)
#define glVertexP2ui GLEW_GET_FUN(__glewVertexP2ui)
#define glVertexP2uiv GLEW_GET_FUN(__glewVertexP2uiv)
#define glVertexP3ui GLEW_GET_FUN(__glewVertexP3ui)
#define glVertexP3uiv GLEW_GET_FUN(__glewVertexP3uiv)
#define glVertexP4ui GLEW_GET_FUN(__glewVertexP4ui)
#define glVertexP4uiv GLEW_GET_FUN(__glewVertexP4uiv)

#define GLEW_ARB_vertex_type_2_10_10_10_rev GLEW_GET_VAR(__GLEW_ARB_vertex_type_2_10_10_10_rev)

#endif /* GL_ARB_vertex_type_2_10_10_10_rev */

/* ------------------------- GL_ARB_viewport_array ------------------------- */

#ifndef GL_ARB_viewport_array
#define GL_ARB_viewport_array 1

#define GL_DEPTH_RANGE 0x0B70
#define GL_VIEWPORT 0x0BA2
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11
#define GL_MAX_VIEWPORTS 0x825B
#define GL_VIEWPORT_SUBPIXEL_BITS 0x825C
#define GL_VIEWPORT_BOUNDS_RANGE 0x825D
#define GL_LAYER_PROVOKING_VERTEX 0x825E
#define GL_VIEWPORT_INDEX_PROVOKING_VERTEX 0x825F
#define GL_UNDEFINED_VERTEX 0x8260
#define GL_FIRST_VERTEX_CONVENTION 0x8E4D
#define GL_LAST_VERTEX_CONVENTION 0x8E4E
#define GL_PROVOKING_VERTEX 0x8E4F

typedef void (GLAPIENTRY * PFNGLDEPTHRANGEARRAYVPROC) (GLuint first, GLsizei count, const GLclampd * v);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEINDEXEDPROC) (GLuint index, GLclampd n, GLclampd f);
typedef void (GLAPIENTRY * PFNGLGETDOUBLEI_VPROC) (GLenum target, GLuint index, GLdouble* data);
typedef void (GLAPIENTRY * PFNGLGETFLOATI_VPROC) (GLenum target, GLuint index, GLfloat* data);
typedef void (GLAPIENTRY * PFNGLSCISSORARRAYVPROC) (GLuint first, GLsizei count, const GLint * v);
typedef void (GLAPIENTRY * PFNGLSCISSORINDEXEDPROC) (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLSCISSORINDEXEDVPROC) (GLuint index, const GLint * v);
typedef void (GLAPIENTRY * PFNGLVIEWPORTARRAYVPROC) (GLuint first, GLsizei count, const GLfloat * v);
typedef void (GLAPIENTRY * PFNGLVIEWPORTINDEXEDFPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
typedef void (GLAPIENTRY * PFNGLVIEWPORTINDEXEDFVPROC) (GLuint index, const GLfloat * v);

#define glDepthRangeArrayv GLEW_GET_FUN(__glewDepthRangeArrayv)
#define glDepthRangeIndexed GLEW_GET_FUN(__glewDepthRangeIndexed)
#define glGetDoublei_v GLEW_GET_FUN(__glewGetDoublei_v)
#define glGetFloati_v GLEW_GET_FUN(__glewGetFloati_v)
#define glScissorArrayv GLEW_GET_FUN(__glewScissorArrayv)
#define glScissorIndexed GLEW_GET_FUN(__glewScissorIndexed)
#define glScissorIndexedv GLEW_GET_FUN(__glewScissorIndexedv)
#define glViewportArrayv GLEW_GET_FUN(__glewViewportArrayv)
#define glViewportIndexedf GLEW_GET_FUN(__glewViewportIndexedf)
#define glViewportIndexedfv GLEW_GET_FUN(__glewViewportIndexedfv)

#define GLEW_ARB_viewport_array GLEW_GET_VAR(__GLEW_ARB_viewport_array)

#endif /* GL_ARB_viewport_array */

/* --------------------------- GL_ARB_window_pos --------------------------- */

#ifndef GL_ARB_window_pos
#define GL_ARB_window_pos 1

typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DARBPROC) (GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DVARBPROC) (const GLdouble* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FARBPROC) (GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FVARBPROC) (const GLfloat* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IARBPROC) (GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IVARBPROC) (const GLint* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SARBPROC) (GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SVARBPROC) (const GLshort* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DARBPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DVARBPROC) (const GLdouble* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FARBPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FVARBPROC) (const GLfloat* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IARBPROC) (GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IVARBPROC) (const GLint* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SARBPROC) (GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SVARBPROC) (const GLshort* p);

#define glWindowPos2dARB GLEW_GET_FUN(__glewWindowPos2dARB)
#define glWindowPos2dvARB GLEW_GET_FUN(__glewWindowPos2dvARB)
#define glWindowPos2fARB GLEW_GET_FUN(__glewWindowPos2fARB)
#define glWindowPos2fvARB GLEW_GET_FUN(__glewWindowPos2fvARB)
#define glWindowPos2iARB GLEW_GET_FUN(__glewWindowPos2iARB)
#define glWindowPos2ivARB GLEW_GET_FUN(__glewWindowPos2ivARB)
#define glWindowPos2sARB GLEW_GET_FUN(__glewWindowPos2sARB)
#define glWindowPos2svARB GLEW_GET_FUN(__glewWindowPos2svARB)
#define glWindowPos3dARB GLEW_GET_FUN(__glewWindowPos3dARB)
#define glWindowPos3dvARB GLEW_GET_FUN(__glewWindowPos3dvARB)
#define glWindowPos3fARB GLEW_GET_FUN(__glewWindowPos3fARB)
#define glWindowPos3fvARB GLEW_GET_FUN(__glewWindowPos3fvARB)
#define glWindowPos3iARB GLEW_GET_FUN(__glewWindowPos3iARB)
#define glWindowPos3ivARB GLEW_GET_FUN(__glewWindowPos3ivARB)
#define glWindowPos3sARB GLEW_GET_FUN(__glewWindowPos3sARB)
#define glWindowPos3svARB GLEW_GET_FUN(__glewWindowPos3svARB)

#define GLEW_ARB_window_pos GLEW_GET_VAR(__GLEW_ARB_window_pos)

#endif /* GL_ARB_window_pos */

/* ------------------------- GL_ATIX_point_sprites ------------------------- */

#ifndef GL_ATIX_point_sprites
#define GL_ATIX_point_sprites 1

#define GL_TEXTURE_POINT_MODE_ATIX 0x60B0
#define GL_TEXTURE_POINT_ONE_COORD_ATIX 0x60B1
#define GL_TEXTURE_POINT_SPRITE_ATIX 0x60B2
#define GL_POINT_SPRITE_CULL_MODE_ATIX 0x60B3
#define GL_POINT_SPRITE_CULL_CENTER_ATIX 0x60B4
#define GL_POINT_SPRITE_CULL_CLIP_ATIX 0x60B5

#define GLEW_ATIX_point_sprites GLEW_GET_VAR(__GLEW_ATIX_point_sprites)

#endif /* GL_ATIX_point_sprites */

/* ---------------------- GL_ATIX_texture_env_combine3 --------------------- */

#ifndef GL_ATIX_texture_env_combine3
#define GL_ATIX_texture_env_combine3 1

#define GL_MODULATE_ADD_ATIX 0x8744
#define GL_MODULATE_SIGNED_ADD_ATIX 0x8745
#define GL_MODULATE_SUBTRACT_ATIX 0x8746

#define GLEW_ATIX_texture_env_combine3 GLEW_GET_VAR(__GLEW_ATIX_texture_env_combine3)

#endif /* GL_ATIX_texture_env_combine3 */

/* ----------------------- GL_ATIX_texture_env_route ----------------------- */

#ifndef GL_ATIX_texture_env_route
#define GL_ATIX_texture_env_route 1

#define GL_SECONDARY_COLOR_ATIX 0x8747
#define GL_TEXTURE_OUTPUT_RGB_ATIX 0x8748
#define GL_TEXTURE_OUTPUT_ALPHA_ATIX 0x8749

#define GLEW_ATIX_texture_env_route GLEW_GET_VAR(__GLEW_ATIX_texture_env_route)

#endif /* GL_ATIX_texture_env_route */

/* ---------------- GL_ATIX_vertex_shader_output_point_size ---------------- */

#ifndef GL_ATIX_vertex_shader_output_point_size
#define GL_ATIX_vertex_shader_output_point_size 1

#define GL_OUTPUT_POINT_SIZE_ATIX 0x610E

#define GLEW_ATIX_vertex_shader_output_point_size GLEW_GET_VAR(__GLEW_ATIX_vertex_shader_output_point_size)

#endif /* GL_ATIX_vertex_shader_output_point_size */

/* -------------------------- GL_ATI_draw_buffers -------------------------- */

#ifndef GL_ATI_draw_buffers
#define GL_ATI_draw_buffers 1

#define GL_MAX_DRAW_BUFFERS_ATI 0x8824
#define GL_DRAW_BUFFER0_ATI 0x8825
#define GL_DRAW_BUFFER1_ATI 0x8826
#define GL_DRAW_BUFFER2_ATI 0x8827
#define GL_DRAW_BUFFER3_ATI 0x8828
#define GL_DRAW_BUFFER4_ATI 0x8829
#define GL_DRAW_BUFFER5_ATI 0x882A
#define GL_DRAW_BUFFER6_ATI 0x882B
#define GL_DRAW_BUFFER7_ATI 0x882C
#define GL_DRAW_BUFFER8_ATI 0x882D
#define GL_DRAW_BUFFER9_ATI 0x882E
#define GL_DRAW_BUFFER10_ATI 0x882F
#define GL_DRAW_BUFFER11_ATI 0x8830
#define GL_DRAW_BUFFER12_ATI 0x8831
#define GL_DRAW_BUFFER13_ATI 0x8832
#define GL_DRAW_BUFFER14_ATI 0x8833
#define GL_DRAW_BUFFER15_ATI 0x8834

typedef void (GLAPIENTRY * PFNGLDRAWBUFFERSATIPROC) (GLsizei n, const GLenum* bufs);

#define glDrawBuffersATI GLEW_GET_FUN(__glewDrawBuffersATI)

#define GLEW_ATI_draw_buffers GLEW_GET_VAR(__GLEW_ATI_draw_buffers)

#endif /* GL_ATI_draw_buffers */

/* -------------------------- GL_ATI_element_array ------------------------- */

#ifndef GL_ATI_element_array
#define GL_ATI_element_array 1

#define GL_ELEMENT_ARRAY_ATI 0x8768
#define GL_ELEMENT_ARRAY_TYPE_ATI 0x8769
#define GL_ELEMENT_ARRAY_POINTER_ATI 0x876A

typedef void (GLAPIENTRY * PFNGLDRAWELEMENTARRAYATIPROC) (GLenum mode, GLsizei count);
typedef void (GLAPIENTRY * PFNGLDRAWRANGEELEMENTARRAYATIPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count);
typedef void (GLAPIENTRY * PFNGLELEMENTPOINTERATIPROC) (GLenum type, const void* pointer);

#define glDrawElementArrayATI GLEW_GET_FUN(__glewDrawElementArrayATI)
#define glDrawRangeElementArrayATI GLEW_GET_FUN(__glewDrawRangeElementArrayATI)
#define glElementPointerATI GLEW_GET_FUN(__glewElementPointerATI)

#define GLEW_ATI_element_array GLEW_GET_VAR(__GLEW_ATI_element_array)

#endif /* GL_ATI_element_array */

/* ------------------------- GL_ATI_envmap_bumpmap ------------------------- */

#ifndef GL_ATI_envmap_bumpmap
#define GL_ATI_envmap_bumpmap 1

#define GL_BUMP_ROT_MATRIX_ATI 0x8775
#define GL_BUMP_ROT_MATRIX_SIZE_ATI 0x8776
#define GL_BUMP_NUM_TEX_UNITS_ATI 0x8777
#define GL_BUMP_TEX_UNITS_ATI 0x8778
#define GL_DUDV_ATI 0x8779
#define GL_DU8DV8_ATI 0x877A
#define GL_BUMP_ENVMAP_ATI 0x877B
#define GL_BUMP_TARGET_ATI 0x877C

typedef void (GLAPIENTRY * PFNGLGETTEXBUMPPARAMETERFVATIPROC) (GLenum pname, GLfloat *param);
typedef void (GLAPIENTRY * PFNGLGETTEXBUMPPARAMETERIVATIPROC) (GLenum pname, GLint *param);
typedef void (GLAPIENTRY * PFNGLTEXBUMPPARAMETERFVATIPROC) (GLenum pname, GLfloat *param);
typedef void (GLAPIENTRY * PFNGLTEXBUMPPARAMETERIVATIPROC) (GLenum pname, GLint *param);

#define glGetTexBumpParameterfvATI GLEW_GET_FUN(__glewGetTexBumpParameterfvATI)
#define glGetTexBumpParameterivATI GLEW_GET_FUN(__glewGetTexBumpParameterivATI)
#define glTexBumpParameterfvATI GLEW_GET_FUN(__glewTexBumpParameterfvATI)
#define glTexBumpParameterivATI GLEW_GET_FUN(__glewTexBumpParameterivATI)

#define GLEW_ATI_envmap_bumpmap GLEW_GET_VAR(__GLEW_ATI_envmap_bumpmap)

#endif /* GL_ATI_envmap_bumpmap */

/* ------------------------- GL_ATI_fragment_shader ------------------------ */

#ifndef GL_ATI_fragment_shader
#define GL_ATI_fragment_shader 1

#define GL_RED_BIT_ATI 0x00000001
#define GL_2X_BIT_ATI 0x00000001
#define GL_4X_BIT_ATI 0x00000002
#define GL_GREEN_BIT_ATI 0x00000002
#define GL_COMP_BIT_ATI 0x00000002
#define GL_BLUE_BIT_ATI 0x00000004
#define GL_8X_BIT_ATI 0x00000004
#define GL_NEGATE_BIT_ATI 0x00000004
#define GL_BIAS_BIT_ATI 0x00000008
#define GL_HALF_BIT_ATI 0x00000008
#define GL_QUARTER_BIT_ATI 0x00000010
#define GL_EIGHTH_BIT_ATI 0x00000020
#define GL_SATURATE_BIT_ATI 0x00000040
#define GL_FRAGMENT_SHADER_ATI 0x8920
#define GL_REG_0_ATI 0x8921
#define GL_REG_1_ATI 0x8922
#define GL_REG_2_ATI 0x8923
#define GL_REG_3_ATI 0x8924
#define GL_REG_4_ATI 0x8925
#define GL_REG_5_ATI 0x8926
#define GL_CON_0_ATI 0x8941
#define GL_CON_1_ATI 0x8942
#define GL_CON_2_ATI 0x8943
#define GL_CON_3_ATI 0x8944
#define GL_CON_4_ATI 0x8945
#define GL_CON_5_ATI 0x8946
#define GL_CON_6_ATI 0x8947
#define GL_CON_7_ATI 0x8948
#define GL_MOV_ATI 0x8961
#define GL_ADD_ATI 0x8963
#define GL_MUL_ATI 0x8964
#define GL_SUB_ATI 0x8965
#define GL_DOT3_ATI 0x8966
#define GL_DOT4_ATI 0x8967
#define GL_MAD_ATI 0x8968
#define GL_LERP_ATI 0x8969
#define GL_CND_ATI 0x896A
#define GL_CND0_ATI 0x896B
#define GL_DOT2_ADD_ATI 0x896C
#define GL_SECONDARY_INTERPOLATOR_ATI 0x896D
#define GL_NUM_FRAGMENT_REGISTERS_ATI 0x896E
#define GL_NUM_FRAGMENT_CONSTANTS_ATI 0x896F
#define GL_NUM_PASSES_ATI 0x8970
#define GL_NUM_INSTRUCTIONS_PER_PASS_ATI 0x8971
#define GL_NUM_INSTRUCTIONS_TOTAL_ATI 0x8972
#define GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI 0x8973
#define GL_NUM_LOOPBACK_COMPONENTS_ATI 0x8974
#define GL_COLOR_ALPHA_PAIRING_ATI 0x8975
#define GL_SWIZZLE_STR_ATI 0x8976
#define GL_SWIZZLE_STQ_ATI 0x8977
#define GL_SWIZZLE_STR_DR_ATI 0x8978
#define GL_SWIZZLE_STQ_DQ_ATI 0x8979
#define GL_SWIZZLE_STRQ_ATI 0x897A
#define GL_SWIZZLE_STRQ_DQ_ATI 0x897B

typedef void (GLAPIENTRY * PFNGLALPHAFRAGMENTOP1ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (GLAPIENTRY * PFNGLALPHAFRAGMENTOP2ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef void (GLAPIENTRY * PFNGLALPHAFRAGMENTOP3ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef void (GLAPIENTRY * PFNGLBEGINFRAGMENTSHADERATIPROC) (void);
typedef void (GLAPIENTRY * PFNGLBINDFRAGMENTSHADERATIPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLCOLORFRAGMENTOP1ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (GLAPIENTRY * PFNGLCOLORFRAGMENTOP2ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef void (GLAPIENTRY * PFNGLCOLORFRAGMENTOP3ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef void (GLAPIENTRY * PFNGLDELETEFRAGMENTSHADERATIPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLENDFRAGMENTSHADERATIPROC) (void);
typedef GLuint (GLAPIENTRY * PFNGLGENFRAGMENTSHADERSATIPROC) (GLuint range);
typedef void (GLAPIENTRY * PFNGLPASSTEXCOORDATIPROC) (GLuint dst, GLuint coord, GLenum swizzle);
typedef void (GLAPIENTRY * PFNGLSAMPLEMAPATIPROC) (GLuint dst, GLuint interp, GLenum swizzle);
typedef void (GLAPIENTRY * PFNGLSETFRAGMENTSHADERCONSTANTATIPROC) (GLuint dst, const GLfloat* value);

#define glAlphaFragmentOp1ATI GLEW_GET_FUN(__glewAlphaFragmentOp1ATI)
#define glAlphaFragmentOp2ATI GLEW_GET_FUN(__glewAlphaFragmentOp2ATI)
#define glAlphaFragmentOp3ATI GLEW_GET_FUN(__glewAlphaFragmentOp3ATI)
#define glBeginFragmentShaderATI GLEW_GET_FUN(__glewBeginFragmentShaderATI)
#define glBindFragmentShaderATI GLEW_GET_FUN(__glewBindFragmentShaderATI)
#define glColorFragmentOp1ATI GLEW_GET_FUN(__glewColorFragmentOp1ATI)
#define glColorFragmentOp2ATI GLEW_GET_FUN(__glewColorFragmentOp2ATI)
#define glColorFragmentOp3ATI GLEW_GET_FUN(__glewColorFragmentOp3ATI)
#define glDeleteFragmentShaderATI GLEW_GET_FUN(__glewDeleteFragmentShaderATI)
#define glEndFragmentShaderATI GLEW_GET_FUN(__glewEndFragmentShaderATI)
#define glGenFragmentShadersATI GLEW_GET_FUN(__glewGenFragmentShadersATI)
#define glPassTexCoordATI GLEW_GET_FUN(__glewPassTexCoordATI)
#define glSampleMapATI GLEW_GET_FUN(__glewSampleMapATI)
#define glSetFragmentShaderConstantATI GLEW_GET_FUN(__glewSetFragmentShaderConstantATI)

#define GLEW_ATI_fragment_shader GLEW_GET_VAR(__GLEW_ATI_fragment_shader)

#endif /* GL_ATI_fragment_shader */

/* ------------------------ GL_ATI_map_object_buffer ----------------------- */

#ifndef GL_ATI_map_object_buffer
#define GL_ATI_map_object_buffer 1

typedef void* (GLAPIENTRY * PFNGLMAPOBJECTBUFFERATIPROC) (GLuint buffer);
typedef void (GLAPIENTRY * PFNGLUNMAPOBJECTBUFFERATIPROC) (GLuint buffer);

#define glMapObjectBufferATI GLEW_GET_FUN(__glewMapObjectBufferATI)
#define glUnmapObjectBufferATI GLEW_GET_FUN(__glewUnmapObjectBufferATI)

#define GLEW_ATI_map_object_buffer GLEW_GET_VAR(__GLEW_ATI_map_object_buffer)

#endif /* GL_ATI_map_object_buffer */

/* ----------------------------- GL_ATI_meminfo ---------------------------- */

#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo 1

#define GL_VBO_FREE_MEMORY_ATI 0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI 0x87FD

#define GLEW_ATI_meminfo GLEW_GET_VAR(__GLEW_ATI_meminfo)

#endif /* GL_ATI_meminfo */

/* -------------------------- GL_ATI_pn_triangles -------------------------- */

#ifndef GL_ATI_pn_triangles
#define GL_ATI_pn_triangles 1

#define GL_PN_TRIANGLES_ATI 0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI 0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI 0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI 0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI 0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI 0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI 0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI 0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI 0x87F8

typedef void (GLAPIENTRY * PFNGLPNTRIANGLESFATIPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLPNTRIANGLESIATIPROC) (GLenum pname, GLint param);

#define glPNTrianglesfATI GLEW_GET_FUN(__glPNTrianglewesfATI)
#define glPNTrianglesiATI GLEW_GET_FUN(__glPNTrianglewesiATI)

#define GLEW_ATI_pn_triangles GLEW_GET_VAR(__GLEW_ATI_pn_triangles)

#endif /* GL_ATI_pn_triangles */

/* ------------------------ GL_ATI_separate_stencil ------------------------ */

#ifndef GL_ATI_separate_stencil
#define GL_ATI_separate_stencil 1

#define GL_STENCIL_BACK_FUNC_ATI 0x8800
#define GL_STENCIL_BACK_FAIL_ATI 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL_ATI 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS_ATI 0x8803

typedef void (GLAPIENTRY * PFNGLSTENCILFUNCSEPARATEATIPROC) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
typedef void (GLAPIENTRY * PFNGLSTENCILOPSEPARATEATIPROC) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);

#define glStencilFuncSeparateATI GLEW_GET_FUN(__glewStencilFuncSeparateATI)
#define glStencilOpSeparateATI GLEW_GET_FUN(__glewStencilOpSeparateATI)

#define GLEW_ATI_separate_stencil GLEW_GET_VAR(__GLEW_ATI_separate_stencil)

#endif /* GL_ATI_separate_stencil */

/* ----------------------- GL_ATI_shader_texture_lod ----------------------- */

#ifndef GL_ATI_shader_texture_lod
#define GL_ATI_shader_texture_lod 1

#define GLEW_ATI_shader_texture_lod GLEW_GET_VAR(__GLEW_ATI_shader_texture_lod)

#endif /* GL_ATI_shader_texture_lod */

/* ---------------------- GL_ATI_text_fragment_shader ---------------------- */

#ifndef GL_ATI_text_fragment_shader
#define GL_ATI_text_fragment_shader 1

#define GL_TEXT_FRAGMENT_SHADER_ATI 0x8200

#define GLEW_ATI_text_fragment_shader GLEW_GET_VAR(__GLEW_ATI_text_fragment_shader)

#endif /* GL_ATI_text_fragment_shader */

/* --------------------- GL_ATI_texture_compression_3dc -------------------- */

#ifndef GL_ATI_texture_compression_3dc
#define GL_ATI_texture_compression_3dc 1

#define GL_COMPRESSED_LUMINANCE_ALPHA_3DC_ATI 0x8837

#define GLEW_ATI_texture_compression_3dc GLEW_GET_VAR(__GLEW_ATI_texture_compression_3dc)

#endif /* GL_ATI_texture_compression_3dc */

/* ---------------------- GL_ATI_texture_env_combine3 ---------------------- */

#ifndef GL_ATI_texture_env_combine3
#define GL_ATI_texture_env_combine3 1

#define GL_MODULATE_ADD_ATI 0x8744
#define GL_MODULATE_SIGNED_ADD_ATI 0x8745
#define GL_MODULATE_SUBTRACT_ATI 0x8746

#define GLEW_ATI_texture_env_combine3 GLEW_GET_VAR(__GLEW_ATI_texture_env_combine3)

#endif /* GL_ATI_texture_env_combine3 */

/* -------------------------- GL_ATI_texture_float ------------------------- */

#ifndef GL_ATI_texture_float
#define GL_ATI_texture_float 1

#define GL_RGBA_FLOAT32_ATI 0x8814
#define GL_RGB_FLOAT32_ATI 0x8815
#define GL_ALPHA_FLOAT32_ATI 0x8816
#define GL_INTENSITY_FLOAT32_ATI 0x8817
#define GL_LUMINANCE_FLOAT32_ATI 0x8818
#define GL_LUMINANCE_ALPHA_FLOAT32_ATI 0x8819
#define GL_RGBA_FLOAT16_ATI 0x881A
#define GL_RGB_FLOAT16_ATI 0x881B
#define GL_ALPHA_FLOAT16_ATI 0x881C
#define GL_INTENSITY_FLOAT16_ATI 0x881D
#define GL_LUMINANCE_FLOAT16_ATI 0x881E
#define GL_LUMINANCE_ALPHA_FLOAT16_ATI 0x881F

#define GLEW_ATI_texture_float GLEW_GET_VAR(__GLEW_ATI_texture_float)

#endif /* GL_ATI_texture_float */

/* ----------------------- GL_ATI_texture_mirror_once ---------------------- */

#ifndef GL_ATI_texture_mirror_once
#define GL_ATI_texture_mirror_once 1

#define GL_MIRROR_CLAMP_ATI 0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_ATI 0x8743

#define GLEW_ATI_texture_mirror_once GLEW_GET_VAR(__GLEW_ATI_texture_mirror_once)

#endif /* GL_ATI_texture_mirror_once */

/* ----------------------- GL_ATI_vertex_array_object ---------------------- */

#ifndef GL_ATI_vertex_array_object
#define GL_ATI_vertex_array_object 1

#define GL_STATIC_ATI 0x8760
#define GL_DYNAMIC_ATI 0x8761
#define GL_PRESERVE_ATI 0x8762
#define GL_DISCARD_ATI 0x8763
#define GL_OBJECT_BUFFER_SIZE_ATI 0x8764
#define GL_OBJECT_BUFFER_USAGE_ATI 0x8765
#define GL_ARRAY_OBJECT_BUFFER_ATI 0x8766
#define GL_ARRAY_OBJECT_OFFSET_ATI 0x8767

typedef void (GLAPIENTRY * PFNGLARRAYOBJECTATIPROC) (GLenum array, GLint size, GLenum type, GLsizei stride, GLuint buffer, GLuint offset);
typedef void (GLAPIENTRY * PFNGLFREEOBJECTBUFFERATIPROC) (GLuint buffer);
typedef void (GLAPIENTRY * PFNGLGETARRAYOBJECTFVATIPROC) (GLenum array, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETARRAYOBJECTIVATIPROC) (GLenum array, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETOBJECTBUFFERFVATIPROC) (GLuint buffer, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETOBJECTBUFFERIVATIPROC) (GLuint buffer, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVARIANTARRAYOBJECTFVATIPROC) (GLuint id, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETVARIANTARRAYOBJECTIVATIPROC) (GLuint id, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISOBJECTBUFFERATIPROC) (GLuint buffer);
typedef GLuint (GLAPIENTRY * PFNGLNEWOBJECTBUFFERATIPROC) (GLsizei size, const void* pointer, GLenum usage);
typedef void (GLAPIENTRY * PFNGLUPDATEOBJECTBUFFERATIPROC) (GLuint buffer, GLuint offset, GLsizei size, const void* pointer, GLenum preserve);
typedef void (GLAPIENTRY * PFNGLVARIANTARRAYOBJECTATIPROC) (GLuint id, GLenum type, GLsizei stride, GLuint buffer, GLuint offset);

#define glArrayObjectATI GLEW_GET_FUN(__glewArrayObjectATI)
#define glFreeObjectBufferATI GLEW_GET_FUN(__glewFreeObjectBufferATI)
#define glGetArrayObjectfvATI GLEW_GET_FUN(__glewGetArrayObjectfvATI)
#define glGetArrayObjectivATI GLEW_GET_FUN(__glewGetArrayObjectivATI)
#define glGetObjectBufferfvATI GLEW_GET_FUN(__glewGetObjectBufferfvATI)
#define glGetObjectBufferivATI GLEW_GET_FUN(__glewGetObjectBufferivATI)
#define glGetVariantArrayObjectfvATI GLEW_GET_FUN(__glewGetVariantArrayObjectfvATI)
#define glGetVariantArrayObjectivATI GLEW_GET_FUN(__glewGetVariantArrayObjectivATI)
#define glIsObjectBufferATI GLEW_GET_FUN(__glewIsObjectBufferATI)
#define glNewObjectBufferATI GLEW_GET_FUN(__glewNewObjectBufferATI)
#define glUpdateObjectBufferATI GLEW_GET_FUN(__glewUpdateObjectBufferATI)
#define glVariantArrayObjectATI GLEW_GET_FUN(__glewVariantArrayObjectATI)

#define GLEW_ATI_vertex_array_object GLEW_GET_VAR(__GLEW_ATI_vertex_array_object)

#endif /* GL_ATI_vertex_array_object */

/* ------------------- GL_ATI_vertex_attrib_array_object ------------------- */

#ifndef GL_ATI_vertex_attrib_array_object
#define GL_ATI_vertex_attrib_array_object 1

typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBARRAYOBJECTFVATIPROC) (GLuint index, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBARRAYOBJECTIVATIPROC) (GLuint index, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBARRAYOBJECTATIPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLuint buffer, GLuint offset);

#define glGetVertexAttribArrayObjectfvATI GLEW_GET_FUN(__glewGetVertexAttribArrayObjectfvATI)
#define glGetVertexAttribArrayObjectivATI GLEW_GET_FUN(__glewGetVertexAttribArrayObjectivATI)
#define glVertexAttribArrayObjectATI GLEW_GET_FUN(__glewVertexAttribArrayObjectATI)

#define GLEW_ATI_vertex_attrib_array_object GLEW_GET_VAR(__GLEW_ATI_vertex_attrib_array_object)

#endif /* GL_ATI_vertex_attrib_array_object */

/* ------------------------- GL_ATI_vertex_streams ------------------------- */

#ifndef GL_ATI_vertex_streams
#define GL_ATI_vertex_streams 1

#define GL_MAX_VERTEX_STREAMS_ATI 0x876B
#define GL_VERTEX_SOURCE_ATI 0x876C
#define GL_VERTEX_STREAM0_ATI 0x876D
#define GL_VERTEX_STREAM1_ATI 0x876E
#define GL_VERTEX_STREAM2_ATI 0x876F
#define GL_VERTEX_STREAM3_ATI 0x8770
#define GL_VERTEX_STREAM4_ATI 0x8771
#define GL_VERTEX_STREAM5_ATI 0x8772
#define GL_VERTEX_STREAM6_ATI 0x8773
#define GL_VERTEX_STREAM7_ATI 0x8774

typedef void (GLAPIENTRY * PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC) (GLenum stream);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3BATIPROC) (GLenum stream, GLbyte x, GLbyte y, GLbyte z);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3BVATIPROC) (GLenum stream, const GLbyte *v);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3DATIPROC) (GLenum stream, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3DVATIPROC) (GLenum stream, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3FATIPROC) (GLenum stream, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3FVATIPROC) (GLenum stream, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3IATIPROC) (GLenum stream, GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3IVATIPROC) (GLenum stream, const GLint *v);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3SATIPROC) (GLenum stream, GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLNORMALSTREAM3SVATIPROC) (GLenum stream, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLVERTEXBLENDENVFATIPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLVERTEXBLENDENVIATIPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2DATIPROC) (GLenum stream, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2DVATIPROC) (GLenum stream, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2FATIPROC) (GLenum stream, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2FVATIPROC) (GLenum stream, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2IATIPROC) (GLenum stream, GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2IVATIPROC) (GLenum stream, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2SATIPROC) (GLenum stream, GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM2SVATIPROC) (GLenum stream, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3DATIPROC) (GLenum stream, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3DVATIPROC) (GLenum stream, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3FATIPROC) (GLenum stream, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3FVATIPROC) (GLenum stream, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3IATIPROC) (GLenum stream, GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3IVATIPROC) (GLenum stream, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3SATIPROC) (GLenum stream, GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM3SVATIPROC) (GLenum stream, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4DATIPROC) (GLenum stream, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4DVATIPROC) (GLenum stream, const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4FATIPROC) (GLenum stream, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4FVATIPROC) (GLenum stream, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4IATIPROC) (GLenum stream, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4IVATIPROC) (GLenum stream, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4SATIPROC) (GLenum stream, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (GLAPIENTRY * PFNGLVERTEXSTREAM4SVATIPROC) (GLenum stream, const GLshort *v);

#define glClientActiveVertexStreamATI GLEW_GET_FUN(__glewClientActiveVertexStreamATI)
#define glNormalStream3bATI GLEW_GET_FUN(__glewNormalStream3bATI)
#define glNormalStream3bvATI GLEW_GET_FUN(__glewNormalStream3bvATI)
#define glNormalStream3dATI GLEW_GET_FUN(__glewNormalStream3dATI)
#define glNormalStream3dvATI GLEW_GET_FUN(__glewNormalStream3dvATI)
#define glNormalStream3fATI GLEW_GET_FUN(__glewNormalStream3fATI)
#define glNormalStream3fvATI GLEW_GET_FUN(__glewNormalStream3fvATI)
#define glNormalStream3iATI GLEW_GET_FUN(__glewNormalStream3iATI)
#define glNormalStream3ivATI GLEW_GET_FUN(__glewNormalStream3ivATI)
#define glNormalStream3sATI GLEW_GET_FUN(__glewNormalStream3sATI)
#define glNormalStream3svATI GLEW_GET_FUN(__glewNormalStream3svATI)
#define glVertexBlendEnvfATI GLEW_GET_FUN(__glewVertexBlendEnvfATI)
#define glVertexBlendEnviATI GLEW_GET_FUN(__glewVertexBlendEnviATI)
#define glVertexStream2dATI GLEW_GET_FUN(__glewVertexStream2dATI)
#define glVertexStream2dvATI GLEW_GET_FUN(__glewVertexStream2dvATI)
#define glVertexStream2fATI GLEW_GET_FUN(__glewVertexStream2fATI)
#define glVertexStream2fvATI GLEW_GET_FUN(__glewVertexStream2fvATI)
#define glVertexStream2iATI GLEW_GET_FUN(__glewVertexStream2iATI)
#define glVertexStream2ivATI GLEW_GET_FUN(__glewVertexStream2ivATI)
#define glVertexStream2sATI GLEW_GET_FUN(__glewVertexStream2sATI)
#define glVertexStream2svATI GLEW_GET_FUN(__glewVertexStream2svATI)
#define glVertexStream3dATI GLEW_GET_FUN(__glewVertexStream3dATI)
#define glVertexStream3dvATI GLEW_GET_FUN(__glewVertexStream3dvATI)
#define glVertexStream3fATI GLEW_GET_FUN(__glewVertexStream3fATI)
#define glVertexStream3fvATI GLEW_GET_FUN(__glewVertexStream3fvATI)
#define glVertexStream3iATI GLEW_GET_FUN(__glewVertexStream3iATI)
#define glVertexStream3ivATI GLEW_GET_FUN(__glewVertexStream3ivATI)
#define glVertexStream3sATI GLEW_GET_FUN(__glewVertexStream3sATI)
#define glVertexStream3svATI GLEW_GET_FUN(__glewVertexStream3svATI)
#define glVertexStream4dATI GLEW_GET_FUN(__glewVertexStream4dATI)
#define glVertexStream4dvATI GLEW_GET_FUN(__glewVertexStream4dvATI)
#define glVertexStream4fATI GLEW_GET_FUN(__glewVertexStream4fATI)
#define glVertexStream4fvATI GLEW_GET_FUN(__glewVertexStream4fvATI)
#define glVertexStream4iATI GLEW_GET_FUN(__glewVertexStream4iATI)
#define glVertexStream4ivATI GLEW_GET_FUN(__glewVertexStream4ivATI)
#define glVertexStream4sATI GLEW_GET_FUN(__glewVertexStream4sATI)
#define glVertexStream4svATI GLEW_GET_FUN(__glewVertexStream4svATI)

#define GLEW_ATI_vertex_streams GLEW_GET_VAR(__GLEW_ATI_vertex_streams)

#endif /* GL_ATI_vertex_streams */

/* --------------------------- GL_EXT_422_pixels --------------------------- */

#ifndef GL_EXT_422_pixels
#define GL_EXT_422_pixels 1

#define GL_422_EXT 0x80CC
#define GL_422_REV_EXT 0x80CD
#define GL_422_AVERAGE_EXT 0x80CE
#define GL_422_REV_AVERAGE_EXT 0x80CF

#define GLEW_EXT_422_pixels GLEW_GET_VAR(__GLEW_EXT_422_pixels)

#endif /* GL_EXT_422_pixels */

/* ---------------------------- GL_EXT_Cg_shader --------------------------- */

#ifndef GL_EXT_Cg_shader
#define GL_EXT_Cg_shader 1

#define GL_CG_VERTEX_SHADER_EXT 0x890E
#define GL_CG_FRAGMENT_SHADER_EXT 0x890F

#define GLEW_EXT_Cg_shader GLEW_GET_VAR(__GLEW_EXT_Cg_shader)

#endif /* GL_EXT_Cg_shader */

/* ------------------------------ GL_EXT_abgr ------------------------------ */

#ifndef GL_EXT_abgr
#define GL_EXT_abgr 1

#define GL_ABGR_EXT 0x8000

#define GLEW_EXT_abgr GLEW_GET_VAR(__GLEW_EXT_abgr)

#endif /* GL_EXT_abgr */

/* ------------------------------ GL_EXT_bgra ------------------------------ */

#ifndef GL_EXT_bgra
#define GL_EXT_bgra 1

#define GL_BGR_EXT 0x80E0
#define GL_BGRA_EXT 0x80E1

#define GLEW_EXT_bgra GLEW_GET_VAR(__GLEW_EXT_bgra)

#endif /* GL_EXT_bgra */

/* ------------------------ GL_EXT_bindable_uniform ------------------------ */

#ifndef GL_EXT_bindable_uniform
#define GL_EXT_bindable_uniform 1

#define GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT 0x8DE2
#define GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT 0x8DE3
#define GL_MAX_GEOMETRY_BINDABLE_UNIFORMS_EXT 0x8DE4
#define GL_MAX_BINDABLE_UNIFORM_SIZE_EXT 0x8DED
#define GL_UNIFORM_BUFFER_EXT 0x8DEE
#define GL_UNIFORM_BUFFER_BINDING_EXT 0x8DEF

typedef GLint (GLAPIENTRY * PFNGLGETUNIFORMBUFFERSIZEEXTPROC) (GLuint program, GLint location);
typedef GLintptr (GLAPIENTRY * PFNGLGETUNIFORMOFFSETEXTPROC) (GLuint program, GLint location);
typedef void (GLAPIENTRY * PFNGLUNIFORMBUFFEREXTPROC) (GLuint program, GLint location, GLuint buffer);

#define glGetUniformBufferSizeEXT GLEW_GET_FUN(__glewGetUniformBufferSizeEXT)
#define glGetUniformOffsetEXT GLEW_GET_FUN(__glewGetUniformOffsetEXT)
#define glUniformBufferEXT GLEW_GET_FUN(__glewUniformBufferEXT)

#define GLEW_EXT_bindable_uniform GLEW_GET_VAR(__GLEW_EXT_bindable_uniform)

#endif /* GL_EXT_bindable_uniform */

/* --------------------------- GL_EXT_blend_color -------------------------- */

#ifndef GL_EXT_blend_color
#define GL_EXT_blend_color 1

#define GL_CONSTANT_COLOR_EXT 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR_EXT 0x8002
#define GL_CONSTANT_ALPHA_EXT 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA_EXT 0x8004
#define GL_BLEND_COLOR_EXT 0x8005

typedef void (GLAPIENTRY * PFNGLBLENDCOLOREXTPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

#define glBlendColorEXT GLEW_GET_FUN(__glewBlendColorEXT)

#define GLEW_EXT_blend_color GLEW_GET_VAR(__GLEW_EXT_blend_color)

#endif /* GL_EXT_blend_color */

/* --------------------- GL_EXT_blend_equation_separate -------------------- */

#ifndef GL_EXT_blend_equation_separate
#define GL_EXT_blend_equation_separate 1

#define GL_BLEND_EQUATION_RGB_EXT 0x8009
#define GL_BLEND_EQUATION_ALPHA_EXT 0x883D

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEEXTPROC) (GLenum modeRGB, GLenum modeAlpha);

#define glBlendEquationSeparateEXT GLEW_GET_FUN(__glewBlendEquationSeparateEXT)

#define GLEW_EXT_blend_equation_separate GLEW_GET_VAR(__GLEW_EXT_blend_equation_separate)

#endif /* GL_EXT_blend_equation_separate */

/* ----------------------- GL_EXT_blend_func_separate ---------------------- */

#ifndef GL_EXT_blend_func_separate
#define GL_EXT_blend_func_separate 1

#define GL_BLEND_DST_RGB_EXT 0x80C8
#define GL_BLEND_SRC_RGB_EXT 0x80C9
#define GL_BLEND_DST_ALPHA_EXT 0x80CA
#define GL_BLEND_SRC_ALPHA_EXT 0x80CB

typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEEXTPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

#define glBlendFuncSeparateEXT GLEW_GET_FUN(__glewBlendFuncSeparateEXT)

#define GLEW_EXT_blend_func_separate GLEW_GET_VAR(__GLEW_EXT_blend_func_separate)

#endif /* GL_EXT_blend_func_separate */

/* ------------------------- GL_EXT_blend_logic_op ------------------------- */

#ifndef GL_EXT_blend_logic_op
#define GL_EXT_blend_logic_op 1

#define GLEW_EXT_blend_logic_op GLEW_GET_VAR(__GLEW_EXT_blend_logic_op)

#endif /* GL_EXT_blend_logic_op */

/* -------------------------- GL_EXT_blend_minmax -------------------------- */

#ifndef GL_EXT_blend_minmax
#define GL_EXT_blend_minmax 1

#define GL_FUNC_ADD_EXT 0x8006
#define GL_MIN_EXT 0x8007
#define GL_MAX_EXT 0x8008
#define GL_BLEND_EQUATION_EXT 0x8009

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONEXTPROC) (GLenum mode);

#define glBlendEquationEXT GLEW_GET_FUN(__glewBlendEquationEXT)

#define GLEW_EXT_blend_minmax GLEW_GET_VAR(__GLEW_EXT_blend_minmax)

#endif /* GL_EXT_blend_minmax */

/* ------------------------- GL_EXT_blend_subtract ------------------------- */

#ifndef GL_EXT_blend_subtract
#define GL_EXT_blend_subtract 1

#define GL_FUNC_SUBTRACT_EXT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT_EXT 0x800B

#define GLEW_EXT_blend_subtract GLEW_GET_VAR(__GLEW_EXT_blend_subtract)

#endif /* GL_EXT_blend_subtract */

/* ------------------------ GL_EXT_clip_volume_hint ------------------------ */

#ifndef GL_EXT_clip_volume_hint
#define GL_EXT_clip_volume_hint 1

#define GL_CLIP_VOLUME_CLIPPING_HINT_EXT 0x80F0

#define GLEW_EXT_clip_volume_hint GLEW_GET_VAR(__GLEW_EXT_clip_volume_hint)

#endif /* GL_EXT_clip_volume_hint */

/* ------------------------------ GL_EXT_cmyka ----------------------------- */

#ifndef GL_EXT_cmyka
#define GL_EXT_cmyka 1

#define GL_CMYK_EXT 0x800C
#define GL_CMYKA_EXT 0x800D
#define GL_PACK_CMYK_HINT_EXT 0x800E
#define GL_UNPACK_CMYK_HINT_EXT 0x800F

#define GLEW_EXT_cmyka GLEW_GET_VAR(__GLEW_EXT_cmyka)

#endif /* GL_EXT_cmyka */

/* ------------------------- GL_EXT_color_subtable ------------------------- */

#ifndef GL_EXT_color_subtable
#define GL_EXT_color_subtable 1

typedef void (GLAPIENTRY * PFNGLCOLORSUBTABLEEXTPROC) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const void* data);
typedef void (GLAPIENTRY * PFNGLCOPYCOLORSUBTABLEEXTPROC) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);

#define glColorSubTableEXT GLEW_GET_FUN(__glewColorSubTableEXT)
#define glCopyColorSubTableEXT GLEW_GET_FUN(__glewCopyColorSubTableEXT)

#define GLEW_EXT_color_subtable GLEW_GET_VAR(__GLEW_EXT_color_subtable)

#endif /* GL_EXT_color_subtable */

/* ---------------------- GL_EXT_compiled_vertex_array --------------------- */

#ifndef GL_EXT_compiled_vertex_array
#define GL_EXT_compiled_vertex_array 1

#define GL_ARRAY_ELEMENT_LOCK_FIRST_EXT 0x81A8
#define GL_ARRAY_ELEMENT_LOCK_COUNT_EXT 0x81A9

typedef void (GLAPIENTRY * PFNGLLOCKARRAYSEXTPROC) (GLint first, GLsizei count);
typedef void (GLAPIENTRY * PFNGLUNLOCKARRAYSEXTPROC) (void);

#define glLockArraysEXT GLEW_GET_FUN(__glewLockArraysEXT)
#define glUnlockArraysEXT GLEW_GET_FUN(__glewUnlockArraysEXT)

#define GLEW_EXT_compiled_vertex_array GLEW_GET_VAR(__GLEW_EXT_compiled_vertex_array)

#endif /* GL_EXT_compiled_vertex_array */

/* --------------------------- GL_EXT_convolution -------------------------- */

#ifndef GL_EXT_convolution
#define GL_EXT_convolution 1

#define GL_CONVOLUTION_1D_EXT 0x8010
#define GL_CONVOLUTION_2D_EXT 0x8011
#define GL_SEPARABLE_2D_EXT 0x8012
#define GL_CONVOLUTION_BORDER_MODE_EXT 0x8013
#define GL_CONVOLUTION_FILTER_SCALE_EXT 0x8014
#define GL_CONVOLUTION_FILTER_BIAS_EXT 0x8015
#define GL_REDUCE_EXT 0x8016
#define GL_CONVOLUTION_FORMAT_EXT 0x8017
#define GL_CONVOLUTION_WIDTH_EXT 0x8018
#define GL_CONVOLUTION_HEIGHT_EXT 0x8019
#define GL_MAX_CONVOLUTION_WIDTH_EXT 0x801A
#define GL_MAX_CONVOLUTION_HEIGHT_EXT 0x801B
#define GL_POST_CONVOLUTION_RED_SCALE_EXT 0x801C
#define GL_POST_CONVOLUTION_GREEN_SCALE_EXT 0x801D
#define GL_POST_CONVOLUTION_BLUE_SCALE_EXT 0x801E
#define GL_POST_CONVOLUTION_ALPHA_SCALE_EXT 0x801F
#define GL_POST_CONVOLUTION_RED_BIAS_EXT 0x8020
#define GL_POST_CONVOLUTION_GREEN_BIAS_EXT 0x8021
#define GL_POST_CONVOLUTION_BLUE_BIAS_EXT 0x8022
#define GL_POST_CONVOLUTION_ALPHA_BIAS_EXT 0x8023

typedef void (GLAPIENTRY * PFNGLCONVOLUTIONFILTER1DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const void* image);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* image);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERFEXTPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERFVEXTPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERIEXTPROC) (GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLCONVOLUTIONPARAMETERIVEXTPROC) (GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONFILTEREXTPROC) (GLenum target, GLenum format, GLenum type, void* image);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETSEPARABLEFILTEREXTPROC) (GLenum target, GLenum format, GLenum type, void* row, void* column, void* span);
typedef void (GLAPIENTRY * PFNGLSEPARABLEFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* row, const void* column);

#define glConvolutionFilter1DEXT GLEW_GET_FUN(__glewConvolutionFilter1DEXT)
#define glConvolutionFilter2DEXT GLEW_GET_FUN(__glewConvolutionFilter2DEXT)
#define glConvolutionParameterfEXT GLEW_GET_FUN(__glewConvolutionParameterfEXT)
#define glConvolutionParameterfvEXT GLEW_GET_FUN(__glewConvolutionParameterfvEXT)
#define glConvolutionParameteriEXT GLEW_GET_FUN(__glewConvolutionParameteriEXT)
#define glConvolutionParameterivEXT GLEW_GET_FUN(__glewConvolutionParameterivEXT)
#define glCopyConvolutionFilter1DEXT GLEW_GET_FUN(__glewCopyConvolutionFilter1DEXT)
#define glCopyConvolutionFilter2DEXT GLEW_GET_FUN(__glewCopyConvolutionFilter2DEXT)
#define glGetConvolutionFilterEXT GLEW_GET_FUN(__glewGetConvolutionFilterEXT)
#define glGetConvolutionParameterfvEXT GLEW_GET_FUN(__glewGetConvolutionParameterfvEXT)
#define glGetConvolutionParameterivEXT GLEW_GET_FUN(__glewGetConvolutionParameterivEXT)
#define glGetSeparableFilterEXT GLEW_GET_FUN(__glewGetSeparableFilterEXT)
#define glSeparableFilter2DEXT GLEW_GET_FUN(__glewSeparableFilter2DEXT)

#define GLEW_EXT_convolution GLEW_GET_VAR(__GLEW_EXT_convolution)

#endif /* GL_EXT_convolution */

/* ------------------------ GL_EXT_coordinate_frame ------------------------ */

#ifndef GL_EXT_coordinate_frame
#define GL_EXT_coordinate_frame 1

#define GL_TANGENT_ARRAY_EXT 0x8439
#define GL_BINORMAL_ARRAY_EXT 0x843A
#define GL_CURRENT_TANGENT_EXT 0x843B
#define GL_CURRENT_BINORMAL_EXT 0x843C
#define GL_TANGENT_ARRAY_TYPE_EXT 0x843E
#define GL_TANGENT_ARRAY_STRIDE_EXT 0x843F
#define GL_BINORMAL_ARRAY_TYPE_EXT 0x8440
#define GL_BINORMAL_ARRAY_STRIDE_EXT 0x8441
#define GL_TANGENT_ARRAY_POINTER_EXT 0x8442
#define GL_BINORMAL_ARRAY_POINTER_EXT 0x8443
#define GL_MAP1_TANGENT_EXT 0x8444
#define GL_MAP2_TANGENT_EXT 0x8445
#define GL_MAP1_BINORMAL_EXT 0x8446
#define GL_MAP2_BINORMAL_EXT 0x8447

typedef void (GLAPIENTRY * PFNGLBINORMALPOINTEREXTPROC) (GLenum type, GLsizei stride, void* pointer);
typedef void (GLAPIENTRY * PFNGLTANGENTPOINTEREXTPROC) (GLenum type, GLsizei stride, void* pointer);

#define glBinormalPointerEXT GLEW_GET_FUN(__glewBinormalPointerEXT)
#define glTangentPointerEXT GLEW_GET_FUN(__glewTangentPointerEXT)

#define GLEW_EXT_coordinate_frame GLEW_GET_VAR(__GLEW_EXT_coordinate_frame)

#endif /* GL_EXT_coordinate_frame */

/* -------------------------- GL_EXT_copy_texture -------------------------- */

#ifndef GL_EXT_copy_texture
#define GL_EXT_copy_texture 1

typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE1DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYTEXIMAGE2DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE1DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE2DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE3DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

#define glCopyTexImage1DEXT GLEW_GET_FUN(__glewCopyTexImage1DEXT)
#define glCopyTexImage2DEXT GLEW_GET_FUN(__glewCopyTexImage2DEXT)
#define glCopyTexSubImage1DEXT GLEW_GET_FUN(__glewCopyTexSubImage1DEXT)
#define glCopyTexSubImage2DEXT GLEW_GET_FUN(__glewCopyTexSubImage2DEXT)
#define glCopyTexSubImage3DEXT GLEW_GET_FUN(__glewCopyTexSubImage3DEXT)

#define GLEW_EXT_copy_texture GLEW_GET_VAR(__GLEW_EXT_copy_texture)

#endif /* GL_EXT_copy_texture */

/* --------------------------- GL_EXT_cull_vertex -------------------------- */

#ifndef GL_EXT_cull_vertex
#define GL_EXT_cull_vertex 1

#define GL_CULL_VERTEX_EXT 0x81AA
#define GL_CULL_VERTEX_EYE_POSITION_EXT 0x81AB
#define GL_CULL_VERTEX_OBJECT_POSITION_EXT 0x81AC

typedef void (GLAPIENTRY * PFNGLCULLPARAMETERDVEXTPROC) (GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLCULLPARAMETERFVEXTPROC) (GLenum pname, GLfloat* params);

#define glCullParameterdvEXT GLEW_GET_FUN(__glewCullParameterdvEXT)
#define glCullParameterfvEXT GLEW_GET_FUN(__glewCullParameterfvEXT)

#define GLEW_EXT_cull_vertex GLEW_GET_VAR(__GLEW_EXT_cull_vertex)

#endif /* GL_EXT_cull_vertex */

/* ------------------------ GL_EXT_depth_bounds_test ----------------------- */

#ifndef GL_EXT_depth_bounds_test
#define GL_EXT_depth_bounds_test 1

#define GL_DEPTH_BOUNDS_TEST_EXT 0x8890
#define GL_DEPTH_BOUNDS_EXT 0x8891

typedef void (GLAPIENTRY * PFNGLDEPTHBOUNDSEXTPROC) (GLclampd zmin, GLclampd zmax);

#define glDepthBoundsEXT GLEW_GET_FUN(__glewDepthBoundsEXT)

#define GLEW_EXT_depth_bounds_test GLEW_GET_VAR(__GLEW_EXT_depth_bounds_test)

#endif /* GL_EXT_depth_bounds_test */

/* ----------------------- GL_EXT_direct_state_access ---------------------- */

#ifndef GL_EXT_direct_state_access
#define GL_EXT_direct_state_access 1

#define GL_PROGRAM_MATRIX_EXT 0x8E2D
#define GL_TRANSPOSE_PROGRAM_MATRIX_EXT 0x8E2E
#define GL_PROGRAM_MATRIX_STACK_DEPTH_EXT 0x8E2F

typedef void (GLAPIENTRY * PFNGLBINDMULTITEXTUREEXTPROC) (GLenum texunit, GLenum target, GLuint texture);
typedef GLenum (GLAPIENTRY * PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC) (GLuint framebuffer, GLenum target);
typedef void (GLAPIENTRY * PFNGLCLIENTATTRIBDEFAULTEXTPROC) (GLbitfield mask);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOPYMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLCOPYTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (GLAPIENTRY * PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC) (GLenum array, GLuint index);
typedef void (GLAPIENTRY * PFNGLDISABLECLIENTSTATEIEXTPROC) (GLenum array, GLuint index);
typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC) (GLuint vaobj, GLuint index);
typedef void (GLAPIENTRY * PFNGLDISABLEVERTEXARRAYEXTPROC) (GLuint vaobj, GLenum array);
typedef void (GLAPIENTRY * PFNGLENABLECLIENTSTATEINDEXEDEXTPROC) (GLenum array, GLuint index);
typedef void (GLAPIENTRY * PFNGLENABLECLIENTSTATEIEXTPROC) (GLenum array, GLuint index);
typedef void (GLAPIENTRY * PFNGLENABLEVERTEXARRAYATTRIBEXTPROC) (GLuint vaobj, GLuint index);
typedef void (GLAPIENTRY * PFNGLENABLEVERTEXARRAYEXTPROC) (GLuint vaobj, GLenum array);
typedef void (GLAPIENTRY * PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC) (GLuint framebuffer, GLenum mode);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC) (GLuint framebuffer, GLsizei n, const GLenum* bufs);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERREADBUFFEREXTPROC) (GLuint framebuffer, GLenum mode);
typedef void (GLAPIENTRY * PFNGLGENERATEMULTITEXMIPMAPEXTPROC) (GLenum texunit, GLenum target);
typedef void (GLAPIENTRY * PFNGLGENERATETEXTUREMIPMAPEXTPROC) (GLuint texture, GLenum target);
typedef void (GLAPIENTRY * PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC) (GLenum texunit, GLenum target, GLint level, void* img);
typedef void (GLAPIENTRY * PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC) (GLuint texture, GLenum target, GLint level, void* img);
typedef void (GLAPIENTRY * PFNGLGETDOUBLEINDEXEDVEXTPROC) (GLenum target, GLuint index, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETDOUBLEI_VEXTPROC) (GLenum pname, GLuint index, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETFLOATINDEXEDVEXTPROC) (GLenum target, GLuint index, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETFLOATI_VEXTPROC) (GLenum pname, GLuint index, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC) (GLuint framebuffer, GLenum pname, GLint* param);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXENVFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXENVIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXGENDVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXGENFVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXGENIVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXIMAGEEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLint level, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXPARAMETERIIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXPARAMETERIUIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMULTITEXPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC) (GLuint buffer, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDBUFFERPOINTERVEXTPROC) (GLuint buffer, GLenum pname, void** params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDBUFFERSUBDATAEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, void* data);
typedef void (GLAPIENTRY * PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC) (GLuint program, GLenum target, GLuint index, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC) (GLuint program, GLenum target, GLuint index, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMSTRINGEXTPROC) (GLuint program, GLenum target, GLenum pname, void* string);
typedef void (GLAPIENTRY * PFNGLGETNAMEDPROGRAMIVEXTPROC) (GLuint program, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC) (GLuint renderbuffer, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETPOINTERINDEXEDVEXTPROC) (GLenum target, GLuint index, GLvoid** params);
typedef void (GLAPIENTRY * PFNGLGETPOINTERI_VEXTPROC) (GLenum pname, GLuint index, GLvoid** params);
typedef void (GLAPIENTRY * PFNGLGETTEXTUREIMAGEEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
typedef void (GLAPIENTRY * PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLint level, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETTEXTUREPARAMETERIIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETTEXTUREPARAMETERIUIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETTEXTUREPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETTEXTUREPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC) (GLuint vaobj, GLuint index, GLenum pname, GLint* param);
typedef void (GLAPIENTRY * PFNGLGETVERTEXARRAYINTEGERVEXTPROC) (GLuint vaobj, GLenum pname, GLint* param);
typedef void (GLAPIENTRY * PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC) (GLuint vaobj, GLuint index, GLenum pname, GLvoid** param);
typedef void (GLAPIENTRY * PFNGLGETVERTEXARRAYPOINTERVEXTPROC) (GLuint vaobj, GLenum pname, GLvoid** param);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPNAMEDBUFFEREXTPROC) (GLuint buffer, GLenum access);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPNAMEDBUFFERRANGEEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (GLAPIENTRY * PFNGLMATRIXFRUSTUMEXTPROC) (GLenum matrixMode, GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
typedef void (GLAPIENTRY * PFNGLMATRIXLOADIDENTITYEXTPROC) (GLenum matrixMode);
typedef void (GLAPIENTRY * PFNGLMATRIXLOADTRANSPOSEDEXTPROC) (GLenum matrixMode, const GLdouble* m);
typedef void (GLAPIENTRY * PFNGLMATRIXLOADTRANSPOSEFEXTPROC) (GLenum matrixMode, const GLfloat* m);
typedef void (GLAPIENTRY * PFNGLMATRIXLOADDEXTPROC) (GLenum matrixMode, const GLdouble* m);
typedef void (GLAPIENTRY * PFNGLMATRIXLOADFEXTPROC) (GLenum matrixMode, const GLfloat* m);
typedef void (GLAPIENTRY * PFNGLMATRIXMULTTRANSPOSEDEXTPROC) (GLenum matrixMode, const GLdouble* m);
typedef void (GLAPIENTRY * PFNGLMATRIXMULTTRANSPOSEFEXTPROC) (GLenum matrixMode, const GLfloat* m);
typedef void (GLAPIENTRY * PFNGLMATRIXMULTDEXTPROC) (GLenum matrixMode, const GLdouble* m);
typedef void (GLAPIENTRY * PFNGLMATRIXMULTFEXTPROC) (GLenum matrixMode, const GLfloat* m);
typedef void (GLAPIENTRY * PFNGLMATRIXORTHOEXTPROC) (GLenum matrixMode, GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
typedef void (GLAPIENTRY * PFNGLMATRIXPOPEXTPROC) (GLenum matrixMode);
typedef void (GLAPIENTRY * PFNGLMATRIXPUSHEXTPROC) (GLenum matrixMode);
typedef void (GLAPIENTRY * PFNGLMATRIXROTATEDEXTPROC) (GLenum matrixMode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLMATRIXROTATEFEXTPROC) (GLenum matrixMode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLMATRIXSCALEDEXTPROC) (GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLMATRIXSCALEFEXTPROC) (GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLMATRIXTRANSLATEDEXTPROC) (GLenum matrixMode, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLMATRIXTRANSLATEFEXTPROC) (GLenum matrixMode, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLMULTITEXBUFFEREXTPROC) (GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORDPOINTEREXTPROC) (GLenum texunit, GLint size, GLenum type, GLsizei stride, const void* pointer);
typedef void (GLAPIENTRY * PFNGLMULTITEXENVFEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLMULTITEXENVFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXENVIEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLMULTITEXENVIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENDEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLdouble param);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENDVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENFEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENFVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENIEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLMULTITEXGENIVEXTPROC) (GLenum texunit, GLenum coord, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLMULTITEXIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLMULTITEXIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERIIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERIUIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLuint* params);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERFEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERFVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLfloat* param);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERIEXTPROC) (GLenum texunit, GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLMULTITEXPARAMETERIVEXTPROC) (GLenum texunit, GLenum target, GLenum pname, const GLint* param);
typedef void (GLAPIENTRY * PFNGLMULTITEXRENDERBUFFEREXTPROC) (GLenum texunit, GLenum target, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLMULTITEXSUBIMAGE1DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLMULTITEXSUBIMAGE2DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLMULTITEXSUBIMAGE3DEXTPROC) (GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLNAMEDBUFFERDATAEXTPROC) (GLuint buffer, GLsizeiptr size, const void* data, GLenum usage);
typedef void (GLAPIENTRY * PFNGLNAMEDBUFFERSUBDATAEXTPROC) (GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data);
typedef void (GLAPIENTRY * PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC) (GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC) (GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC) (GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (GLAPIENTRY * PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC) (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC) (GLuint program, GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC) (GLuint program, GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC) (GLuint program, GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLint* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC) (GLuint program, GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC) (GLuint program, GLenum target, GLuint index, const GLuint* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLint* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC) (GLuint program, GLenum target, GLuint index, GLsizei count, const GLuint* params);
typedef void (GLAPIENTRY * PFNGLNAMEDPROGRAMSTRINGEXTPROC) (GLuint program, GLenum target, GLenum format, GLsizei len, const void* string);
typedef void (GLAPIENTRY * PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC) (GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC) (GLuint renderbuffer, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FEXTPROC) (GLuint program, GLint location, GLfloat v0);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IEXTPROC) (GLuint program, GLint location, GLint v0);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UIEXTPROC) (GLuint program, GLint location, GLuint v0);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FEXTPROC) (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IEXTPROC) (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UIEXTPROC) (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UIVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLuint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC) (GLbitfield mask);
typedef void (GLAPIENTRY * PFNGLTEXTUREBUFFEREXTPROC) (GLuint texture, GLenum target, GLenum internalformat, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLTEXTUREIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXTUREIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXTUREIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERIIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERIUIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLuint* params);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERFEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERFVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLfloat* param);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERIEXTPROC) (GLuint texture, GLenum target, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLTEXTUREPARAMETERIVEXTPROC) (GLuint texture, GLenum target, GLenum pname, const GLint* param);
typedef void (GLAPIENTRY * PFNGLTEXTURERENDERBUFFEREXTPROC) (GLuint texture, GLenum target, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLTEXTURESUBIMAGE1DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXTURESUBIMAGE2DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXTURESUBIMAGE3DEXTPROC) (GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
typedef GLboolean (GLAPIENTRY * PFNGLUNMAPNAMEDBUFFEREXTPROC) (GLuint buffer);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYCOLOROFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYINDEXOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum texunit, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYNORMALOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLint size, GLenum type, GLsizei stride, GLintptr offset);

#define glBindMultiTextureEXT GLEW_GET_FUN(__glewBindMultiTextureEXT)
#define glCheckNamedFramebufferStatusEXT GLEW_GET_FUN(__glewCheckNamedFramebufferStatusEXT)
#define glClientAttribDefaultEXT GLEW_GET_FUN(__glewClientAttribDefaultEXT)
#define glCompressedMultiTexImage1DEXT GLEW_GET_FUN(__glewCompressedMultiTexImage1DEXT)
#define glCompressedMultiTexImage2DEXT GLEW_GET_FUN(__glewCompressedMultiTexImage2DEXT)
#define glCompressedMultiTexImage3DEXT GLEW_GET_FUN(__glewCompressedMultiTexImage3DEXT)
#define glCompressedMultiTexSubImage1DEXT GLEW_GET_FUN(__glewCompressedMultiTexSubImage1DEXT)
#define glCompressedMultiTexSubImage2DEXT GLEW_GET_FUN(__glewCompressedMultiTexSubImage2DEXT)
#define glCompressedMultiTexSubImage3DEXT GLEW_GET_FUN(__glewCompressedMultiTexSubImage3DEXT)
#define glCompressedTextureImage1DEXT GLEW_GET_FUN(__glewCompressedTextureImage1DEXT)
#define glCompressedTextureImage2DEXT GLEW_GET_FUN(__glewCompressedTextureImage2DEXT)
#define glCompressedTextureImage3DEXT GLEW_GET_FUN(__glewCompressedTextureImage3DEXT)
#define glCompressedTextureSubImage1DEXT GLEW_GET_FUN(__glewCompressedTextureSubImage1DEXT)
#define glCompressedTextureSubImage2DEXT GLEW_GET_FUN(__glewCompressedTextureSubImage2DEXT)
#define glCompressedTextureSubImage3DEXT GLEW_GET_FUN(__glewCompressedTextureSubImage3DEXT)
#define glCopyMultiTexImage1DEXT GLEW_GET_FUN(__glewCopyMultiTexImage1DEXT)
#define glCopyMultiTexImage2DEXT GLEW_GET_FUN(__glewCopyMultiTexImage2DEXT)
#define glCopyMultiTexSubImage1DEXT GLEW_GET_FUN(__glewCopyMultiTexSubImage1DEXT)
#define glCopyMultiTexSubImage2DEXT GLEW_GET_FUN(__glewCopyMultiTexSubImage2DEXT)
#define glCopyMultiTexSubImage3DEXT GLEW_GET_FUN(__glewCopyMultiTexSubImage3DEXT)
#define glCopyTextureImage1DEXT GLEW_GET_FUN(__glewCopyTextureImage1DEXT)
#define glCopyTextureImage2DEXT GLEW_GET_FUN(__glewCopyTextureImage2DEXT)
#define glCopyTextureSubImage1DEXT GLEW_GET_FUN(__glewCopyTextureSubImage1DEXT)
#define glCopyTextureSubImage2DEXT GLEW_GET_FUN(__glewCopyTextureSubImage2DEXT)
#define glCopyTextureSubImage3DEXT GLEW_GET_FUN(__glewCopyTextureSubImage3DEXT)
#define glDisableClientStateIndexedEXT GLEW_GET_FUN(__glewDisableClientStateIndexedEXT)
#define glDisableClientStateiEXT GLEW_GET_FUN(__glewDisableClientStateiEXT)
#define glDisableVertexArrayAttribEXT GLEW_GET_FUN(__glewDisableVertexArrayAttribEXT)
#define glDisableVertexArrayEXT GLEW_GET_FUN(__glewDisableVertexArrayEXT)
#define glEnableClientStateIndexedEXT GLEW_GET_FUN(__glewEnableClientStateIndexedEXT)
#define glEnableClientStateiEXT GLEW_GET_FUN(__glewEnableClientStateiEXT)
#define glEnableVertexArrayAttribEXT GLEW_GET_FUN(__glewEnableVertexArrayAttribEXT)
#define glEnableVertexArrayEXT GLEW_GET_FUN(__glewEnableVertexArrayEXT)
#define glFlushMappedNamedBufferRangeEXT GLEW_GET_FUN(__glewFlushMappedNamedBufferRangeEXT)
#define glFramebufferDrawBufferEXT GLEW_GET_FUN(__glewFramebufferDrawBufferEXT)
#define glFramebufferDrawBuffersEXT GLEW_GET_FUN(__glewFramebufferDrawBuffersEXT)
#define glFramebufferReadBufferEXT GLEW_GET_FUN(__glewFramebufferReadBufferEXT)
#define glGenerateMultiTexMipmapEXT GLEW_GET_FUN(__glewGenerateMultiTexMipmapEXT)
#define glGenerateTextureMipmapEXT GLEW_GET_FUN(__glewGenerateTextureMipmapEXT)
#define glGetCompressedMultiTexImageEXT GLEW_GET_FUN(__glewGetCompressedMultiTexImageEXT)
#define glGetCompressedTextureImageEXT GLEW_GET_FUN(__glewGetCompressedTextureImageEXT)
#define glGetDoubleIndexedvEXT GLEW_GET_FUN(__glewGetDoubleIndexedvEXT)
#define glGetDoublei_vEXT GLEW_GET_FUN(__glewGetDoublei_vEXT)
#define glGetFloatIndexedvEXT GLEW_GET_FUN(__glewGetFloatIndexedvEXT)
#define glGetFloati_vEXT GLEW_GET_FUN(__glewGetFloati_vEXT)
#define glGetFramebufferParameterivEXT GLEW_GET_FUN(__glewGetFramebufferParameterivEXT)
#define glGetMultiTexEnvfvEXT GLEW_GET_FUN(__glewGetMultiTexEnvfvEXT)
#define glGetMultiTexEnvivEXT GLEW_GET_FUN(__glewGetMultiTexEnvivEXT)
#define glGetMultiTexGendvEXT GLEW_GET_FUN(__glewGetMultiTexGendvEXT)
#define glGetMultiTexGenfvEXT GLEW_GET_FUN(__glewGetMultiTexGenfvEXT)
#define glGetMultiTexGenivEXT GLEW_GET_FUN(__glewGetMultiTexGenivEXT)
#define glGetMultiTexImageEXT GLEW_GET_FUN(__glewGetMultiTexImageEXT)
#define glGetMultiTexLevelParameterfvEXT GLEW_GET_FUN(__glewGetMultiTexLevelParameterfvEXT)
#define glGetMultiTexLevelParameterivEXT GLEW_GET_FUN(__glewGetMultiTexLevelParameterivEXT)
#define glGetMultiTexParameterIivEXT GLEW_GET_FUN(__glewGetMultiTexParameterIivEXT)
#define glGetMultiTexParameterIuivEXT GLEW_GET_FUN(__glewGetMultiTexParameterIuivEXT)
#define glGetMultiTexParameterfvEXT GLEW_GET_FUN(__glewGetMultiTexParameterfvEXT)
#define glGetMultiTexParameterivEXT GLEW_GET_FUN(__glewGetMultiTexParameterivEXT)
#define glGetNamedBufferParameterivEXT GLEW_GET_FUN(__glewGetNamedBufferParameterivEXT)
#define glGetNamedBufferPointervEXT GLEW_GET_FUN(__glewGetNamedBufferPointervEXT)
#define glGetNamedBufferSubDataEXT GLEW_GET_FUN(__glewGetNamedBufferSubDataEXT)
#define glGetNamedFramebufferAttachmentParameterivEXT GLEW_GET_FUN(__glewGetNamedFramebufferAttachmentParameterivEXT)
#define glGetNamedProgramLocalParameterIivEXT GLEW_GET_FUN(__glewGetNamedProgramLocalParameterIivEXT)
#define glGetNamedProgramLocalParameterIuivEXT GLEW_GET_FUN(__glewGetNamedProgramLocalParameterIuivEXT)
#define glGetNamedProgramLocalParameterdvEXT GLEW_GET_FUN(__glewGetNamedProgramLocalParameterdvEXT)
#define glGetNamedProgramLocalParameterfvEXT GLEW_GET_FUN(__glewGetNamedProgramLocalParameterfvEXT)
#define glGetNamedProgramStringEXT GLEW_GET_FUN(__glewGetNamedProgramStringEXT)
#define glGetNamedProgramivEXT GLEW_GET_FUN(__glewGetNamedProgramivEXT)
#define glGetNamedRenderbufferParameterivEXT GLEW_GET_FUN(__glewGetNamedRenderbufferParameterivEXT)
#define glGetPointerIndexedvEXT GLEW_GET_FUN(__glewGetPointerIndexedvEXT)
#define glGetPointeri_vEXT GLEW_GET_FUN(__glewGetPointeri_vEXT)
#define glGetTextureImageEXT GLEW_GET_FUN(__glewGetTextureImageEXT)
#define glGetTextureLevelParameterfvEXT GLEW_GET_FUN(__glewGetTextureLevelParameterfvEXT)
#define glGetTextureLevelParameterivEXT GLEW_GET_FUN(__glewGetTextureLevelParameterivEXT)
#define glGetTextureParameterIivEXT GLEW_GET_FUN(__glewGetTextureParameterIivEXT)
#define glGetTextureParameterIuivEXT GLEW_GET_FUN(__glewGetTextureParameterIuivEXT)
#define glGetTextureParameterfvEXT GLEW_GET_FUN(__glewGetTextureParameterfvEXT)
#define glGetTextureParameterivEXT GLEW_GET_FUN(__glewGetTextureParameterivEXT)
#define glGetVertexArrayIntegeri_vEXT GLEW_GET_FUN(__glewGetVertexArrayIntegeri_vEXT)
#define glGetVertexArrayIntegervEXT GLEW_GET_FUN(__glewGetVertexArrayIntegervEXT)
#define glGetVertexArrayPointeri_vEXT GLEW_GET_FUN(__glewGetVertexArrayPointeri_vEXT)
#define glGetVertexArrayPointervEXT GLEW_GET_FUN(__glewGetVertexArrayPointervEXT)
#define glMapNamedBufferEXT GLEW_GET_FUN(__glewMapNamedBufferEXT)
#define glMapNamedBufferRangeEXT GLEW_GET_FUN(__glewMapNamedBufferRangeEXT)
#define glMatrixFrustumEXT GLEW_GET_FUN(__glewMatrixFrustumEXT)
#define glMatrixLoadIdentityEXT GLEW_GET_FUN(__glewMatrixLoadIdentityEXT)
#define glMatrixLoadTransposedEXT GLEW_GET_FUN(__glewMatrixLoadTransposedEXT)
#define glMatrixLoadTransposefEXT GLEW_GET_FUN(__glewMatrixLoadTransposefEXT)
#define glMatrixLoaddEXT GLEW_GET_FUN(__glewMatrixLoaddEXT)
#define glMatrixLoadfEXT GLEW_GET_FUN(__glewMatrixLoadfEXT)
#define glMatrixMultTransposedEXT GLEW_GET_FUN(__glewMatrixMultTransposedEXT)
#define glMatrixMultTransposefEXT GLEW_GET_FUN(__glewMatrixMultTransposefEXT)
#define glMatrixMultdEXT GLEW_GET_FUN(__glewMatrixMultdEXT)
#define glMatrixMultfEXT GLEW_GET_FUN(__glewMatrixMultfEXT)
#define glMatrixOrthoEXT GLEW_GET_FUN(__glewMatrixOrthoEXT)
#define glMatrixPopEXT GLEW_GET_FUN(__glewMatrixPopEXT)
#define glMatrixPushEXT GLEW_GET_FUN(__glewMatrixPushEXT)
#define glMatrixRotatedEXT GLEW_GET_FUN(__glewMatrixRotatedEXT)
#define glMatrixRotatefEXT GLEW_GET_FUN(__glewMatrixRotatefEXT)
#define glMatrixScaledEXT GLEW_GET_FUN(__glewMatrixScaledEXT)
#define glMatrixScalefEXT GLEW_GET_FUN(__glewMatrixScalefEXT)
#define glMatrixTranslatedEXT GLEW_GET_FUN(__glewMatrixTranslatedEXT)
#define glMatrixTranslatefEXT GLEW_GET_FUN(__glewMatrixTranslatefEXT)
#define glMultiTexBufferEXT GLEW_GET_FUN(__glewMultiTexBufferEXT)
#define glMultiTexCoordPointerEXT GLEW_GET_FUN(__glewMultiTexCoordPointerEXT)
#define glMultiTexEnvfEXT GLEW_GET_FUN(__glewMultiTexEnvfEXT)
#define glMultiTexEnvfvEXT GLEW_GET_FUN(__glewMultiTexEnvfvEXT)
#define glMultiTexEnviEXT GLEW_GET_FUN(__glewMultiTexEnviEXT)
#define glMultiTexEnvivEXT GLEW_GET_FUN(__glewMultiTexEnvivEXT)
#define glMultiTexGendEXT GLEW_GET_FUN(__glewMultiTexGendEXT)
#define glMultiTexGendvEXT GLEW_GET_FUN(__glewMultiTexGendvEXT)
#define glMultiTexGenfEXT GLEW_GET_FUN(__glewMultiTexGenfEXT)
#define glMultiTexGenfvEXT GLEW_GET_FUN(__glewMultiTexGenfvEXT)
#define glMultiTexGeniEXT GLEW_GET_FUN(__glewMultiTexGeniEXT)
#define glMultiTexGenivEXT GLEW_GET_FUN(__glewMultiTexGenivEXT)
#define glMultiTexImage1DEXT GLEW_GET_FUN(__glewMultiTexImage1DEXT)
#define glMultiTexImage2DEXT GLEW_GET_FUN(__glewMultiTexImage2DEXT)
#define glMultiTexImage3DEXT GLEW_GET_FUN(__glewMultiTexImage3DEXT)
#define glMultiTexParameterIivEXT GLEW_GET_FUN(__glewMultiTexParameterIivEXT)
#define glMultiTexParameterIuivEXT GLEW_GET_FUN(__glewMultiTexParameterIuivEXT)
#define glMultiTexParameterfEXT GLEW_GET_FUN(__glewMultiTexParameterfEXT)
#define glMultiTexParameterfvEXT GLEW_GET_FUN(__glewMultiTexParameterfvEXT)
#define glMultiTexParameteriEXT GLEW_GET_FUN(__glewMultiTexParameteriEXT)
#define glMultiTexParameterivEXT GLEW_GET_FUN(__glewMultiTexParameterivEXT)
#define glMultiTexRenderbufferEXT GLEW_GET_FUN(__glewMultiTexRenderbufferEXT)
#define glMultiTexSubImage1DEXT GLEW_GET_FUN(__glewMultiTexSubImage1DEXT)
#define glMultiTexSubImage2DEXT GLEW_GET_FUN(__glewMultiTexSubImage2DEXT)
#define glMultiTexSubImage3DEXT GLEW_GET_FUN(__glewMultiTexSubImage3DEXT)
#define glNamedBufferDataEXT GLEW_GET_FUN(__glewNamedBufferDataEXT)
#define glNamedBufferSubDataEXT GLEW_GET_FUN(__glewNamedBufferSubDataEXT)
#define glNamedCopyBufferSubDataEXT GLEW_GET_FUN(__glewNamedCopyBufferSubDataEXT)
#define glNamedFramebufferRenderbufferEXT GLEW_GET_FUN(__glewNamedFramebufferRenderbufferEXT)
#define glNamedFramebufferTexture1DEXT GLEW_GET_FUN(__glewNamedFramebufferTexture1DEXT)
#define glNamedFramebufferTexture2DEXT GLEW_GET_FUN(__glewNamedFramebufferTexture2DEXT)
#define glNamedFramebufferTexture3DEXT GLEW_GET_FUN(__glewNamedFramebufferTexture3DEXT)
#define glNamedFramebufferTextureEXT GLEW_GET_FUN(__glewNamedFramebufferTextureEXT)
#define glNamedFramebufferTextureFaceEXT GLEW_GET_FUN(__glewNamedFramebufferTextureFaceEXT)
#define glNamedFramebufferTextureLayerEXT GLEW_GET_FUN(__glewNamedFramebufferTextureLayerEXT)
#define glNamedProgramLocalParameter4dEXT GLEW_GET_FUN(__glewNamedProgramLocalParameter4dEXT)
#define glNamedProgramLocalParameter4dvEXT GLEW_GET_FUN(__glewNamedProgramLocalParameter4dvEXT)
#define glNamedProgramLocalParameter4fEXT GLEW_GET_FUN(__glewNamedProgramLocalParameter4fEXT)
#define glNamedProgramLocalParameter4fvEXT GLEW_GET_FUN(__glewNamedProgramLocalParameter4fvEXT)
#define glNamedProgramLocalParameterI4iEXT GLEW_GET_FUN(__glewNamedProgramLocalParameterI4iEXT)
#define glNamedProgramLocalParameterI4ivEXT GLEW_GET_FUN(__glewNamedProgramLocalParameterI4ivEXT)
#define glNamedProgramLocalParameterI4uiEXT GLEW_GET_FUN(__glewNamedProgramLocalParameterI4uiEXT)
#define glNamedProgramLocalParameterI4uivEXT GLEW_GET_FUN(__glewNamedProgramLocalParameterI4uivEXT)
#define glNamedProgramLocalParameters4fvEXT GLEW_GET_FUN(__glewNamedProgramLocalParameters4fvEXT)
#define glNamedProgramLocalParametersI4ivEXT GLEW_GET_FUN(__glewNamedProgramLocalParametersI4ivEXT)
#define glNamedProgramLocalParametersI4uivEXT GLEW_GET_FUN(__glewNamedProgramLocalParametersI4uivEXT)
#define glNamedProgramStringEXT GLEW_GET_FUN(__glewNamedProgramStringEXT)
#define glNamedRenderbufferStorageEXT GLEW_GET_FUN(__glewNamedRenderbufferStorageEXT)
#define glNamedRenderbufferStorageMultisampleCoverageEXT GLEW_GET_FUN(__glewNamedRenderbufferStorageMultisampleCoverageEXT)
#define glNamedRenderbufferStorageMultisampleEXT GLEW_GET_FUN(__glewNamedRenderbufferStorageMultisampleEXT)
#define glProgramUniform1fEXT GLEW_GET_FUN(__glewProgramUniform1fEXT)
#define glProgramUniform1fvEXT GLEW_GET_FUN(__glewProgramUniform1fvEXT)
#define glProgramUniform1iEXT GLEW_GET_FUN(__glewProgramUniform1iEXT)
#define glProgramUniform1ivEXT GLEW_GET_FUN(__glewProgramUniform1ivEXT)
#define glProgramUniform1uiEXT GLEW_GET_FUN(__glewProgramUniform1uiEXT)
#define glProgramUniform1uivEXT GLEW_GET_FUN(__glewProgramUniform1uivEXT)
#define glProgramUniform2fEXT GLEW_GET_FUN(__glewProgramUniform2fEXT)
#define glProgramUniform2fvEXT GLEW_GET_FUN(__glewProgramUniform2fvEXT)
#define glProgramUniform2iEXT GLEW_GET_FUN(__glewProgramUniform2iEXT)
#define glProgramUniform2ivEXT GLEW_GET_FUN(__glewProgramUniform2ivEXT)
#define glProgramUniform2uiEXT GLEW_GET_FUN(__glewProgramUniform2uiEXT)
#define glProgramUniform2uivEXT GLEW_GET_FUN(__glewProgramUniform2uivEXT)
#define glProgramUniform3fEXT GLEW_GET_FUN(__glewProgramUniform3fEXT)
#define glProgramUniform3fvEXT GLEW_GET_FUN(__glewProgramUniform3fvEXT)
#define glProgramUniform3iEXT GLEW_GET_FUN(__glewProgramUniform3iEXT)
#define glProgramUniform3ivEXT GLEW_GET_FUN(__glewProgramUniform3ivEXT)
#define glProgramUniform3uiEXT GLEW_GET_FUN(__glewProgramUniform3uiEXT)
#define glProgramUniform3uivEXT GLEW_GET_FUN(__glewProgramUniform3uivEXT)
#define glProgramUniform4fEXT GLEW_GET_FUN(__glewProgramUniform4fEXT)
#define glProgramUniform4fvEXT GLEW_GET_FUN(__glewProgramUniform4fvEXT)
#define glProgramUniform4iEXT GLEW_GET_FUN(__glewProgramUniform4iEXT)
#define glProgramUniform4ivEXT GLEW_GET_FUN(__glewProgramUniform4ivEXT)
#define glProgramUniform4uiEXT GLEW_GET_FUN(__glewProgramUniform4uiEXT)
#define glProgramUniform4uivEXT GLEW_GET_FUN(__glewProgramUniform4uivEXT)
#define glProgramUniformMatrix2fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2fvEXT)
#define glProgramUniformMatrix2x3fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2x3fvEXT)
#define glProgramUniformMatrix2x4fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2x4fvEXT)
#define glProgramUniformMatrix3fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3fvEXT)
#define glProgramUniformMatrix3x2fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3x2fvEXT)
#define glProgramUniformMatrix3x4fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3x4fvEXT)
#define glProgramUniformMatrix4fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4fvEXT)
#define glProgramUniformMatrix4x2fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4x2fvEXT)
#define glProgramUniformMatrix4x3fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4x3fvEXT)
#define glPushClientAttribDefaultEXT GLEW_GET_FUN(__glewPushClientAttribDefaultEXT)
#define glTextureBufferEXT GLEW_GET_FUN(__glewTextureBufferEXT)
#define glTextureImage1DEXT GLEW_GET_FUN(__glewTextureImage1DEXT)
#define glTextureImage2DEXT GLEW_GET_FUN(__glewTextureImage2DEXT)
#define glTextureImage3DEXT GLEW_GET_FUN(__glewTextureImage3DEXT)
#define glTextureParameterIivEXT GLEW_GET_FUN(__glewTextureParameterIivEXT)
#define glTextureParameterIuivEXT GLEW_GET_FUN(__glewTextureParameterIuivEXT)
#define glTextureParameterfEXT GLEW_GET_FUN(__glewTextureParameterfEXT)
#define glTextureParameterfvEXT GLEW_GET_FUN(__glewTextureParameterfvEXT)
#define glTextureParameteriEXT GLEW_GET_FUN(__glewTextureParameteriEXT)
#define glTextureParameterivEXT GLEW_GET_FUN(__glewTextureParameterivEXT)
#define glTextureRenderbufferEXT GLEW_GET_FUN(__glewTextureRenderbufferEXT)
#define glTextureSubImage1DEXT GLEW_GET_FUN(__glewTextureSubImage1DEXT)
#define glTextureSubImage2DEXT GLEW_GET_FUN(__glewTextureSubImage2DEXT)
#define glTextureSubImage3DEXT GLEW_GET_FUN(__glewTextureSubImage3DEXT)
#define glUnmapNamedBufferEXT GLEW_GET_FUN(__glewUnmapNamedBufferEXT)
#define glVertexArrayColorOffsetEXT GLEW_GET_FUN(__glewVertexArrayColorOffsetEXT)
#define glVertexArrayEdgeFlagOffsetEXT GLEW_GET_FUN(__glewVertexArrayEdgeFlagOffsetEXT)
#define glVertexArrayFogCoordOffsetEXT GLEW_GET_FUN(__glewVertexArrayFogCoordOffsetEXT)
#define glVertexArrayIndexOffsetEXT GLEW_GET_FUN(__glewVertexArrayIndexOffsetEXT)
#define glVertexArrayMultiTexCoordOffsetEXT GLEW_GET_FUN(__glewVertexArrayMultiTexCoordOffsetEXT)
#define glVertexArrayNormalOffsetEXT GLEW_GET_FUN(__glewVertexArrayNormalOffsetEXT)
#define glVertexArraySecondaryColorOffsetEXT GLEW_GET_FUN(__glewVertexArraySecondaryColorOffsetEXT)
#define glVertexArrayTexCoordOffsetEXT GLEW_GET_FUN(__glewVertexArrayTexCoordOffsetEXT)
#define glVertexArrayVertexAttribIOffsetEXT GLEW_GET_FUN(__glewVertexArrayVertexAttribIOffsetEXT)
#define glVertexArrayVertexAttribOffsetEXT GLEW_GET_FUN(__glewVertexArrayVertexAttribOffsetEXT)
#define glVertexArrayVertexOffsetEXT GLEW_GET_FUN(__glewVertexArrayVertexOffsetEXT)

#define GLEW_EXT_direct_state_access GLEW_GET_VAR(__GLEW_EXT_direct_state_access)

#endif /* GL_EXT_direct_state_access */

/* -------------------------- GL_EXT_draw_buffers2 ------------------------- */

#ifndef GL_EXT_draw_buffers2
#define GL_EXT_draw_buffers2 1

typedef void (GLAPIENTRY * PFNGLCOLORMASKINDEXEDEXTPROC) (GLuint buf, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (GLAPIENTRY * PFNGLDISABLEINDEXEDEXTPROC) (GLenum target, GLuint index);
typedef void (GLAPIENTRY * PFNGLENABLEINDEXEDEXTPROC) (GLenum target, GLuint index);
typedef void (GLAPIENTRY * PFNGLGETBOOLEANINDEXEDVEXTPROC) (GLenum value, GLuint index, GLboolean* data);
typedef void (GLAPIENTRY * PFNGLGETINTEGERINDEXEDVEXTPROC) (GLenum value, GLuint index, GLint* data);
typedef GLboolean (GLAPIENTRY * PFNGLISENABLEDINDEXEDEXTPROC) (GLenum target, GLuint index);

#define glColorMaskIndexedEXT GLEW_GET_FUN(__glewColorMaskIndexedEXT)
#define glDisableIndexedEXT GLEW_GET_FUN(__glewDisableIndexedEXT)
#define glEnableIndexedEXT GLEW_GET_FUN(__glewEnableIndexedEXT)
#define glGetBooleanIndexedvEXT GLEW_GET_FUN(__glewGetBooleanIndexedvEXT)
#define glGetIntegerIndexedvEXT GLEW_GET_FUN(__glewGetIntegerIndexedvEXT)
#define glIsEnabledIndexedEXT GLEW_GET_FUN(__glewIsEnabledIndexedEXT)

#define GLEW_EXT_draw_buffers2 GLEW_GET_VAR(__GLEW_EXT_draw_buffers2)

#endif /* GL_EXT_draw_buffers2 */

/* ------------------------- GL_EXT_draw_instanced ------------------------- */

#ifndef GL_EXT_draw_instanced
#define GL_EXT_draw_instanced 1

typedef void (GLAPIENTRY * PFNGLDRAWARRAYSINSTANCEDEXTPROC) (GLenum mode, GLint start, GLsizei count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINSTANCEDEXTPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);

#define glDrawArraysInstancedEXT GLEW_GET_FUN(__glewDrawArraysInstancedEXT)
#define glDrawElementsInstancedEXT GLEW_GET_FUN(__glewDrawElementsInstancedEXT)

#define GLEW_EXT_draw_instanced GLEW_GET_VAR(__GLEW_EXT_draw_instanced)

#endif /* GL_EXT_draw_instanced */

/* ----------------------- GL_EXT_draw_range_elements ---------------------- */

#ifndef GL_EXT_draw_range_elements
#define GL_EXT_draw_range_elements 1

#define GL_MAX_ELEMENTS_VERTICES_EXT 0x80E8
#define GL_MAX_ELEMENTS_INDICES_EXT 0x80E9

typedef void (GLAPIENTRY * PFNGLDRAWRANGEELEMENTSEXTPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

#define glDrawRangeElementsEXT GLEW_GET_FUN(__glewDrawRangeElementsEXT)

#define GLEW_EXT_draw_range_elements GLEW_GET_VAR(__GLEW_EXT_draw_range_elements)

#endif /* GL_EXT_draw_range_elements */

/* ---------------------------- GL_EXT_fog_coord --------------------------- */

#ifndef GL_EXT_fog_coord
#define GL_EXT_fog_coord 1

#define GL_FOG_COORDINATE_SOURCE_EXT 0x8450
#define GL_FOG_COORDINATE_EXT 0x8451
#define GL_FRAGMENT_DEPTH_EXT 0x8452
#define GL_CURRENT_FOG_COORDINATE_EXT 0x8453
#define GL_FOG_COORDINATE_ARRAY_TYPE_EXT 0x8454
#define GL_FOG_COORDINATE_ARRAY_STRIDE_EXT 0x8455
#define GL_FOG_COORDINATE_ARRAY_POINTER_EXT 0x8456
#define GL_FOG_COORDINATE_ARRAY_EXT 0x8457

typedef void (GLAPIENTRY * PFNGLFOGCOORDPOINTEREXTPROC) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (GLAPIENTRY * PFNGLFOGCOORDDEXTPROC) (GLdouble coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDDVEXTPROC) (const GLdouble *coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDFEXTPROC) (GLfloat coord);
typedef void (GLAPIENTRY * PFNGLFOGCOORDFVEXTPROC) (const GLfloat *coord);

#define glFogCoordPointerEXT GLEW_GET_FUN(__glewFogCoordPointerEXT)
#define glFogCoorddEXT GLEW_GET_FUN(__glewFogCoorddEXT)
#define glFogCoorddvEXT GLEW_GET_FUN(__glewFogCoorddvEXT)
#define glFogCoordfEXT GLEW_GET_FUN(__glewFogCoordfEXT)
#define glFogCoordfvEXT GLEW_GET_FUN(__glewFogCoordfvEXT)

#define GLEW_EXT_fog_coord GLEW_GET_VAR(__GLEW_EXT_fog_coord)

#endif /* GL_EXT_fog_coord */

/* ------------------------ GL_EXT_fragment_lighting ----------------------- */

#ifndef GL_EXT_fragment_lighting
#define GL_EXT_fragment_lighting 1

#define GL_FRAGMENT_LIGHTING_EXT 0x8400
#define GL_FRAGMENT_COLOR_MATERIAL_EXT 0x8401
#define GL_FRAGMENT_COLOR_MATERIAL_FACE_EXT 0x8402
#define GL_FRAGMENT_COLOR_MATERIAL_PARAMETER_EXT 0x8403
#define GL_MAX_FRAGMENT_LIGHTS_EXT 0x8404
#define GL_MAX_ACTIVE_LIGHTS_EXT 0x8405
#define GL_CURRENT_RASTER_NORMAL_EXT 0x8406
#define GL_LIGHT_ENV_MODE_EXT 0x8407
#define GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_EXT 0x8408
#define GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_EXT 0x8409
#define GL_FRAGMENT_LIGHT_MODEL_AMBIENT_EXT 0x840A
#define GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_EXT 0x840B
#define GL_FRAGMENT_LIGHT0_EXT 0x840C
#define GL_FRAGMENT_LIGHT7_EXT 0x8413

typedef void (GLAPIENTRY * PFNGLFRAGMENTCOLORMATERIALEXTPROC) (GLenum face, GLenum mode);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELFEXTPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELFVEXTPROC) (GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELIEXTPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELIVEXTPROC) (GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTFEXTPROC) (GLenum light, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTFVEXTPROC) (GLenum light, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTIEXTPROC) (GLenum light, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTIVEXTPROC) (GLenum light, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALFEXTPROC) (GLenum face, GLenum pname, const GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALFVEXTPROC) (GLenum face, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALIEXTPROC) (GLenum face, GLenum pname, const GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALIVEXTPROC) (GLenum face, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTLIGHTFVEXTPROC) (GLenum light, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTLIGHTIVEXTPROC) (GLenum light, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTMATERIALFVEXTPROC) (GLenum face, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTMATERIALIVEXTPROC) (GLenum face, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLLIGHTENVIEXTPROC) (GLenum pname, GLint param);

#define glFragmentColorMaterialEXT GLEW_GET_FUN(__glewFragmentColorMaterialEXT)
#define glFragmentLightModelfEXT GLEW_GET_FUN(__glewFragmentLightModelfEXT)
#define glFragmentLightModelfvEXT GLEW_GET_FUN(__glewFragmentLightModelfvEXT)
#define glFragmentLightModeliEXT GLEW_GET_FUN(__glewFragmentLightModeliEXT)
#define glFragmentLightModelivEXT GLEW_GET_FUN(__glewFragmentLightModelivEXT)
#define glFragmentLightfEXT GLEW_GET_FUN(__glewFragmentLightfEXT)
#define glFragmentLightfvEXT GLEW_GET_FUN(__glewFragmentLightfvEXT)
#define glFragmentLightiEXT GLEW_GET_FUN(__glewFragmentLightiEXT)
#define glFragmentLightivEXT GLEW_GET_FUN(__glewFragmentLightivEXT)
#define glFragmentMaterialfEXT GLEW_GET_FUN(__glewFragmentMaterialfEXT)
#define glFragmentMaterialfvEXT GLEW_GET_FUN(__glewFragmentMaterialfvEXT)
#define glFragmentMaterialiEXT GLEW_GET_FUN(__glewFragmentMaterialiEXT)
#define glFragmentMaterialivEXT GLEW_GET_FUN(__glewFragmentMaterialivEXT)
#define glGetFragmentLightfvEXT GLEW_GET_FUN(__glewGetFragmentLightfvEXT)
#define glGetFragmentLightivEXT GLEW_GET_FUN(__glewGetFragmentLightivEXT)
#define glGetFragmentMaterialfvEXT GLEW_GET_FUN(__glewGetFragmentMaterialfvEXT)
#define glGetFragmentMaterialivEXT GLEW_GET_FUN(__glewGetFragmentMaterialivEXT)
#define glLightEnviEXT GLEW_GET_FUN(__glewLightEnviEXT)

#define GLEW_EXT_fragment_lighting GLEW_GET_VAR(__GLEW_EXT_fragment_lighting)

#endif /* GL_EXT_fragment_lighting */

/* ------------------------ GL_EXT_framebuffer_blit ------------------------ */

#ifndef GL_EXT_framebuffer_blit
#define GL_EXT_framebuffer_blit 1

#define GL_DRAW_FRAMEBUFFER_BINDING_EXT 0x8CA6
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING_EXT 0x8CAA

typedef void (GLAPIENTRY * PFNGLBLITFRAMEBUFFEREXTPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

#define glBlitFramebufferEXT GLEW_GET_FUN(__glewBlitFramebufferEXT)

#define GLEW_EXT_framebuffer_blit GLEW_GET_VAR(__GLEW_EXT_framebuffer_blit)

#endif /* GL_EXT_framebuffer_blit */

/* --------------------- GL_EXT_framebuffer_multisample -------------------- */

#ifndef GL_EXT_framebuffer_multisample
#define GL_EXT_framebuffer_multisample 1

#define GL_RENDERBUFFER_SAMPLES_EXT 0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT 0x8D56
#define GL_MAX_SAMPLES_EXT 0x8D57

typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

#define glRenderbufferStorageMultisampleEXT GLEW_GET_FUN(__glewRenderbufferStorageMultisampleEXT)

#define GLEW_EXT_framebuffer_multisample GLEW_GET_VAR(__GLEW_EXT_framebuffer_multisample)

#endif /* GL_EXT_framebuffer_multisample */

/* ----------------------- GL_EXT_framebuffer_object ----------------------- */

#ifndef GL_EXT_framebuffer_object
#define GL_EXT_framebuffer_object 1

#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
#define GL_MAX_RENDERBUFFER_SIZE_EXT 0x84E8
#define GL_FRAMEBUFFER_BINDING_EXT 0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT 0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_EXT 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT 0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT 0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT 0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS_EXT 0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT 0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT 0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT 0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT 0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT 0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT 0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT 0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT 0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT 0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT 0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT 0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT 0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT 0x8CED
#define GL_COLOR_ATTACHMENT14_EXT 0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT 0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT 0x8D00
#define GL_STENCIL_ATTACHMENT_EXT 0x8D20
#define GL_FRAMEBUFFER_EXT 0x8D40
#define GL_RENDERBUFFER_EXT 0x8D41
#define GL_RENDERBUFFER_WIDTH_EXT 0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT 0x8D44
#define GL_STENCIL_INDEX1_EXT 0x8D46
#define GL_STENCIL_INDEX4_EXT 0x8D47
#define GL_STENCIL_INDEX8_EXT 0x8D48
#define GL_STENCIL_INDEX16_EXT 0x8D49
#define GL_RENDERBUFFER_RED_SIZE_EXT 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_EXT 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_EXT 0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_EXT 0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_EXT 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_EXT 0x8D55

typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFEREXTPROC) (GLenum target, GLuint framebuffer);
typedef void (GLAPIENTRY * PFNGLBINDRENDERBUFFEREXTPROC) (GLenum target, GLuint renderbuffer);
typedef GLenum (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERSEXTPROC) (GLsizei n, const GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLDELETERENDERBUFFERSEXTPROC) (GLsizei n, const GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERSEXTPROC) (GLsizei n, GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLGENRENDERBUFFERSEXTPROC) (GLsizei n, GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLGENERATEMIPMAPEXTPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) (GLenum target, GLenum attachment, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISFRAMEBUFFEREXTPROC) (GLuint framebuffer);
typedef GLboolean (GLAPIENTRY * PFNGLISRENDERBUFFEREXTPROC) (GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

#define glBindFramebufferEXT GLEW_GET_FUN(__glewBindFramebufferEXT)
#define glBindRenderbufferEXT GLEW_GET_FUN(__glewBindRenderbufferEXT)
#define glCheckFramebufferStatusEXT GLEW_GET_FUN(__glewCheckFramebufferStatusEXT)
#define glDeleteFramebuffersEXT GLEW_GET_FUN(__glewDeleteFramebuffersEXT)
#define glDeleteRenderbuffersEXT GLEW_GET_FUN(__glewDeleteRenderbuffersEXT)
#define glFramebufferRenderbufferEXT GLEW_GET_FUN(__glewFramebufferRenderbufferEXT)
#define glFramebufferTexture1DEXT GLEW_GET_FUN(__glewFramebufferTexture1DEXT)
#define glFramebufferTexture2DEXT GLEW_GET_FUN(__glewFramebufferTexture2DEXT)
#define glFramebufferTexture3DEXT GLEW_GET_FUN(__glewFramebufferTexture3DEXT)
#define glGenFramebuffersEXT GLEW_GET_FUN(__glewGenFramebuffersEXT)
#define glGenRenderbuffersEXT GLEW_GET_FUN(__glewGenRenderbuffersEXT)
#define glGenerateMipmapEXT GLEW_GET_FUN(__glewGenerateMipmapEXT)
#define glGetFramebufferAttachmentParameterivEXT GLEW_GET_FUN(__glewGetFramebufferAttachmentParameterivEXT)
#define glGetRenderbufferParameterivEXT GLEW_GET_FUN(__glewGetRenderbufferParameterivEXT)
#define glIsFramebufferEXT GLEW_GET_FUN(__glewIsFramebufferEXT)
#define glIsRenderbufferEXT GLEW_GET_FUN(__glewIsRenderbufferEXT)
#define glRenderbufferStorageEXT GLEW_GET_FUN(__glewRenderbufferStorageEXT)

#define GLEW_EXT_framebuffer_object GLEW_GET_VAR(__GLEW_EXT_framebuffer_object)

#endif /* GL_EXT_framebuffer_object */

/* ------------------------ GL_EXT_framebuffer_sRGB ------------------------ */

#ifndef GL_EXT_framebuffer_sRGB
#define GL_EXT_framebuffer_sRGB 1

#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#define GL_FRAMEBUFFER_SRGB_CAPABLE_EXT 0x8DBA

#define GLEW_EXT_framebuffer_sRGB GLEW_GET_VAR(__GLEW_EXT_framebuffer_sRGB)

#endif /* GL_EXT_framebuffer_sRGB */

/* ------------------------ GL_EXT_geometry_shader4 ------------------------ */

#ifndef GL_EXT_geometry_shader4
#define GL_EXT_geometry_shader4 1

#define GL_LINES_ADJACENCY_EXT 0xA
#define GL_LINE_STRIP_ADJACENCY_EXT 0xB
#define GL_TRIANGLES_ADJACENCY_EXT 0xC
#define GL_TRIANGLE_STRIP_ADJACENCY_EXT 0xD
#define GL_PROGRAM_POINT_SIZE_EXT 0x8642
#define GL_MAX_VARYING_COMPONENTS_EXT 0x8B4B
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT 0x8C29
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_EXT 0x8CD4
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT 0x8DA7
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT 0x8DA8
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT 0x8DA9
#define GL_GEOMETRY_SHADER_EXT 0x8DD9
#define GL_GEOMETRY_VERTICES_OUT_EXT 0x8DDA
#define GL_GEOMETRY_INPUT_TYPE_EXT 0x8DDB
#define GL_GEOMETRY_OUTPUT_TYPE_EXT 0x8DDC
#define GL_MAX_GEOMETRY_VARYING_COMPONENTS_EXT 0x8DDD
#define GL_MAX_VERTEX_VARYING_COMPONENTS_EXT 0x8DDE
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT 0x8DDF
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT 0x8DE0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT 0x8DE1

typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTUREEXTPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERIEXTPROC) (GLuint program, GLenum pname, GLint value);

#define glFramebufferTextureEXT GLEW_GET_FUN(__glewFramebufferTextureEXT)
#define glFramebufferTextureFaceEXT GLEW_GET_FUN(__glewFramebufferTextureFaceEXT)
#define glFramebufferTextureLayerEXT GLEW_GET_FUN(__glewFramebufferTextureLayerEXT)
#define glProgramParameteriEXT GLEW_GET_FUN(__glewProgramParameteriEXT)

#define GLEW_EXT_geometry_shader4 GLEW_GET_VAR(__GLEW_EXT_geometry_shader4)

#endif /* GL_EXT_geometry_shader4 */

/* --------------------- GL_EXT_gpu_program_parameters --------------------- */

#ifndef GL_EXT_gpu_program_parameters
#define GL_EXT_gpu_program_parameters 1

typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERS4FVEXTPROC) (GLenum target, GLuint index, GLsizei count, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC) (GLenum target, GLuint index, GLsizei count, const GLfloat* params);

#define glProgramEnvParameters4fvEXT GLEW_GET_FUN(__glewProgramEnvParameters4fvEXT)
#define glProgramLocalParameters4fvEXT GLEW_GET_FUN(__glewProgramLocalParameters4fvEXT)

#define GLEW_EXT_gpu_program_parameters GLEW_GET_VAR(__GLEW_EXT_gpu_program_parameters)

#endif /* GL_EXT_gpu_program_parameters */

/* --------------------------- GL_EXT_gpu_shader4 -------------------------- */

#ifndef GL_EXT_gpu_shader4
#define GL_EXT_gpu_shader4 1

#define GL_VERTEX_ATTRIB_ARRAY_INTEGER_EXT 0x88FD
#define GL_SAMPLER_1D_ARRAY_EXT 0x8DC0
#define GL_SAMPLER_2D_ARRAY_EXT 0x8DC1
#define GL_SAMPLER_BUFFER_EXT 0x8DC2
#define GL_SAMPLER_1D_ARRAY_SHADOW_EXT 0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW_EXT 0x8DC4
#define GL_SAMPLER_CUBE_SHADOW_EXT 0x8DC5
#define GL_UNSIGNED_INT_VEC2_EXT 0x8DC6
#define GL_UNSIGNED_INT_VEC3_EXT 0x8DC7
#define GL_UNSIGNED_INT_VEC4_EXT 0x8DC8
#define GL_INT_SAMPLER_1D_EXT 0x8DC9
#define GL_INT_SAMPLER_2D_EXT 0x8DCA
#define GL_INT_SAMPLER_3D_EXT 0x8DCB
#define GL_INT_SAMPLER_CUBE_EXT 0x8DCC
#define GL_INT_SAMPLER_2D_RECT_EXT 0x8DCD
#define GL_INT_SAMPLER_1D_ARRAY_EXT 0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY_EXT 0x8DCF
#define GL_INT_SAMPLER_BUFFER_EXT 0x8DD0
#define GL_UNSIGNED_INT_SAMPLER_1D_EXT 0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D_EXT 0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D_EXT 0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE_EXT 0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT_EXT 0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY_EXT 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY_EXT 0x8DD7
#define GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT 0x8DD8

typedef void (GLAPIENTRY * PFNGLBINDFRAGDATALOCATIONEXTPROC) (GLuint program, GLuint color, const GLchar *name);
typedef GLint (GLAPIENTRY * PFNGLGETFRAGDATALOCATIONEXTPROC) (GLuint program, const GLchar *name);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMUIVEXTPROC) (GLuint program, GLint location, GLuint *params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIIVEXTPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIUIVEXTPROC) (GLuint index, GLenum pname, GLuint *params);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UIEXTPROC) (GLint location, GLuint v0);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UIVEXTPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UIEXTPROC) (GLint location, GLuint v0, GLuint v1);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UIVEXTPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UIEXTPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UIVEXTPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UIEXTPROC) (GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UIVEXTPROC) (GLint location, GLsizei count, const GLuint *value);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1IEXTPROC) (GLuint index, GLint x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1IVEXTPROC) (GLuint index, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1UIEXTPROC) (GLuint index, GLuint x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI1UIVEXTPROC) (GLuint index, const GLuint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2IEXTPROC) (GLuint index, GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2IVEXTPROC) (GLuint index, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2UIEXTPROC) (GLuint index, GLuint x, GLuint y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI2UIVEXTPROC) (GLuint index, const GLuint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3IEXTPROC) (GLuint index, GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3IVEXTPROC) (GLuint index, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3UIEXTPROC) (GLuint index, GLuint x, GLuint y, GLuint z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI3UIVEXTPROC) (GLuint index, const GLuint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4BVEXTPROC) (GLuint index, const GLbyte *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4IEXTPROC) (GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4IVEXTPROC) (GLuint index, const GLint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4SVEXTPROC) (GLuint index, const GLshort *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UBVEXTPROC) (GLuint index, const GLubyte *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UIEXTPROC) (GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4UIVEXTPROC) (GLuint index, const GLuint *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBI4USVEXTPROC) (GLuint index, const GLushort *v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBIPOINTEREXTPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

#define glBindFragDataLocationEXT GLEW_GET_FUN(__glewBindFragDataLocationEXT)
#define glGetFragDataLocationEXT GLEW_GET_FUN(__glewGetFragDataLocationEXT)
#define glGetUniformuivEXT GLEW_GET_FUN(__glewGetUniformuivEXT)
#define glGetVertexAttribIivEXT GLEW_GET_FUN(__glewGetVertexAttribIivEXT)
#define glGetVertexAttribIuivEXT GLEW_GET_FUN(__glewGetVertexAttribIuivEXT)
#define glUniform1uiEXT GLEW_GET_FUN(__glewUniform1uiEXT)
#define glUniform1uivEXT GLEW_GET_FUN(__glewUniform1uivEXT)
#define glUniform2uiEXT GLEW_GET_FUN(__glewUniform2uiEXT)
#define glUniform2uivEXT GLEW_GET_FUN(__glewUniform2uivEXT)
#define glUniform3uiEXT GLEW_GET_FUN(__glewUniform3uiEXT)
#define glUniform3uivEXT GLEW_GET_FUN(__glewUniform3uivEXT)
#define glUniform4uiEXT GLEW_GET_FUN(__glewUniform4uiEXT)
#define glUniform4uivEXT GLEW_GET_FUN(__glewUniform4uivEXT)
#define glVertexAttribI1iEXT GLEW_GET_FUN(__glewVertexAttribI1iEXT)
#define glVertexAttribI1ivEXT GLEW_GET_FUN(__glewVertexAttribI1ivEXT)
#define glVertexAttribI1uiEXT GLEW_GET_FUN(__glewVertexAttribI1uiEXT)
#define glVertexAttribI1uivEXT GLEW_GET_FUN(__glewVertexAttribI1uivEXT)
#define glVertexAttribI2iEXT GLEW_GET_FUN(__glewVertexAttribI2iEXT)
#define glVertexAttribI2ivEXT GLEW_GET_FUN(__glewVertexAttribI2ivEXT)
#define glVertexAttribI2uiEXT GLEW_GET_FUN(__glewVertexAttribI2uiEXT)
#define glVertexAttribI2uivEXT GLEW_GET_FUN(__glewVertexAttribI2uivEXT)
#define glVertexAttribI3iEXT GLEW_GET_FUN(__glewVertexAttribI3iEXT)
#define glVertexAttribI3ivEXT GLEW_GET_FUN(__glewVertexAttribI3ivEXT)
#define glVertexAttribI3uiEXT GLEW_GET_FUN(__glewVertexAttribI3uiEXT)
#define glVertexAttribI3uivEXT GLEW_GET_FUN(__glewVertexAttribI3uivEXT)
#define glVertexAttribI4bvEXT GLEW_GET_FUN(__glewVertexAttribI4bvEXT)
#define glVertexAttribI4iEXT GLEW_GET_FUN(__glewVertexAttribI4iEXT)
#define glVertexAttribI4ivEXT GLEW_GET_FUN(__glewVertexAttribI4ivEXT)
#define glVertexAttribI4svEXT GLEW_GET_FUN(__glewVertexAttribI4svEXT)
#define glVertexAttribI4ubvEXT GLEW_GET_FUN(__glewVertexAttribI4ubvEXT)
#define glVertexAttribI4uiEXT GLEW_GET_FUN(__glewVertexAttribI4uiEXT)
#define glVertexAttribI4uivEXT GLEW_GET_FUN(__glewVertexAttribI4uivEXT)
#define glVertexAttribI4usvEXT GLEW_GET_FUN(__glewVertexAttribI4usvEXT)
#define glVertexAttribIPointerEXT GLEW_GET_FUN(__glewVertexAttribIPointerEXT)

#define GLEW_EXT_gpu_shader4 GLEW_GET_VAR(__GLEW_EXT_gpu_shader4)

#endif /* GL_EXT_gpu_shader4 */

/* ---------------------------- GL_EXT_histogram --------------------------- */

#ifndef GL_EXT_histogram
#define GL_EXT_histogram 1

#define GL_HISTOGRAM_EXT 0x8024
#define GL_PROXY_HISTOGRAM_EXT 0x8025
#define GL_HISTOGRAM_WIDTH_EXT 0x8026
#define GL_HISTOGRAM_FORMAT_EXT 0x8027
#define GL_HISTOGRAM_RED_SIZE_EXT 0x8028
#define GL_HISTOGRAM_GREEN_SIZE_EXT 0x8029
#define GL_HISTOGRAM_BLUE_SIZE_EXT 0x802A
#define GL_HISTOGRAM_ALPHA_SIZE_EXT 0x802B
#define GL_HISTOGRAM_LUMINANCE_SIZE_EXT 0x802C
#define GL_HISTOGRAM_SINK_EXT 0x802D
#define GL_MINMAX_EXT 0x802E
#define GL_MINMAX_FORMAT_EXT 0x802F
#define GL_MINMAX_SINK_EXT 0x8030

typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMEXTPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, void* values);
typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETHISTOGRAMPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMINMAXEXTPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, void* values);
typedef void (GLAPIENTRY * PFNGLGETMINMAXPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMINMAXPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLHISTOGRAMEXTPROC) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY * PFNGLMINMAXEXTPROC) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY * PFNGLRESETHISTOGRAMEXTPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLRESETMINMAXEXTPROC) (GLenum target);

#define glGetHistogramEXT GLEW_GET_FUN(__glewGetHistogramEXT)
#define glGetHistogramParameterfvEXT GLEW_GET_FUN(__glewGetHistogramParameterfvEXT)
#define glGetHistogramParameterivEXT GLEW_GET_FUN(__glewGetHistogramParameterivEXT)
#define glGetMinmaxEXT GLEW_GET_FUN(__glewGetMinmaxEXT)
#define glGetMinmaxParameterfvEXT GLEW_GET_FUN(__glewGetMinmaxParameterfvEXT)
#define glGetMinmaxParameterivEXT GLEW_GET_FUN(__glewGetMinmaxParameterivEXT)
#define glHistogramEXT GLEW_GET_FUN(__glewHistogramEXT)
#define glMinmaxEXT GLEW_GET_FUN(__glewMinmaxEXT)
#define glResetHistogramEXT GLEW_GET_FUN(__glewResetHistogramEXT)
#define glResetMinmaxEXT GLEW_GET_FUN(__glewResetMinmaxEXT)

#define GLEW_EXT_histogram GLEW_GET_VAR(__GLEW_EXT_histogram)

#endif /* GL_EXT_histogram */

/* ----------------------- GL_EXT_index_array_formats ---------------------- */

#ifndef GL_EXT_index_array_formats
#define GL_EXT_index_array_formats 1

#define GLEW_EXT_index_array_formats GLEW_GET_VAR(__GLEW_EXT_index_array_formats)

#endif /* GL_EXT_index_array_formats */

/* --------------------------- GL_EXT_index_func --------------------------- */

#ifndef GL_EXT_index_func
#define GL_EXT_index_func 1

typedef void (GLAPIENTRY * PFNGLINDEXFUNCEXTPROC) (GLenum func, GLfloat ref);

#define glIndexFuncEXT GLEW_GET_FUN(__glewIndexFuncEXT)

#define GLEW_EXT_index_func GLEW_GET_VAR(__GLEW_EXT_index_func)

#endif /* GL_EXT_index_func */

/* ------------------------- GL_EXT_index_material ------------------------- */

#ifndef GL_EXT_index_material
#define GL_EXT_index_material 1

typedef void (GLAPIENTRY * PFNGLINDEXMATERIALEXTPROC) (GLenum face, GLenum mode);

#define glIndexMaterialEXT GLEW_GET_FUN(__glewIndexMaterialEXT)

#define GLEW_EXT_index_material GLEW_GET_VAR(__GLEW_EXT_index_material)

#endif /* GL_EXT_index_material */

/* -------------------------- GL_EXT_index_texture ------------------------- */

#ifndef GL_EXT_index_texture
#define GL_EXT_index_texture 1

#define GLEW_EXT_index_texture GLEW_GET_VAR(__GLEW_EXT_index_texture)

#endif /* GL_EXT_index_texture */

/* -------------------------- GL_EXT_light_texture ------------------------- */

#ifndef GL_EXT_light_texture
#define GL_EXT_light_texture 1

#define GL_FRAGMENT_MATERIAL_EXT 0x8349
#define GL_FRAGMENT_NORMAL_EXT 0x834A
#define GL_FRAGMENT_COLOR_EXT 0x834C
#define GL_ATTENUATION_EXT 0x834D
#define GL_SHADOW_ATTENUATION_EXT 0x834E
#define GL_TEXTURE_APPLICATION_MODE_EXT 0x834F
#define GL_TEXTURE_LIGHT_EXT 0x8350
#define GL_TEXTURE_MATERIAL_FACE_EXT 0x8351
#define GL_TEXTURE_MATERIAL_PARAMETER_EXT 0x8352
#define GL_FRAGMENT_DEPTH_EXT 0x8452

typedef void (GLAPIENTRY * PFNGLAPPLYTEXTUREEXTPROC) (GLenum mode);
typedef void (GLAPIENTRY * PFNGLTEXTURELIGHTEXTPROC) (GLenum pname);
typedef void (GLAPIENTRY * PFNGLTEXTUREMATERIALEXTPROC) (GLenum face, GLenum mode);

#define glApplyTextureEXT GLEW_GET_FUN(__glewApplyTextureEXT)
#define glTextureLightEXT GLEW_GET_FUN(__glewTextureLightEXT)
#define glTextureMaterialEXT GLEW_GET_FUN(__glewTextureMaterialEXT)

#define GLEW_EXT_light_texture GLEW_GET_VAR(__GLEW_EXT_light_texture)

#endif /* GL_EXT_light_texture */

/* ------------------------- GL_EXT_misc_attribute ------------------------- */

#ifndef GL_EXT_misc_attribute
#define GL_EXT_misc_attribute 1

#define GLEW_EXT_misc_attribute GLEW_GET_VAR(__GLEW_EXT_misc_attribute)

#endif /* GL_EXT_misc_attribute */

/* ------------------------ GL_EXT_multi_draw_arrays ----------------------- */

#ifndef GL_EXT_multi_draw_arrays
#define GL_EXT_multi_draw_arrays 1

typedef void (GLAPIENTRY * PFNGLMULTIDRAWARRAYSEXTPROC) (GLenum mode, const GLint* first, const GLsizei *count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTSEXTPROC) (GLenum mode, GLsizei* count, GLenum type, const GLvoid **indices, GLsizei primcount);

#define glMultiDrawArraysEXT GLEW_GET_FUN(__glewMultiDrawArraysEXT)
#define glMultiDrawElementsEXT GLEW_GET_FUN(__glewMultiDrawElementsEXT)

#define GLEW_EXT_multi_draw_arrays GLEW_GET_VAR(__GLEW_EXT_multi_draw_arrays)

#endif /* GL_EXT_multi_draw_arrays */

/* --------------------------- GL_EXT_multisample -------------------------- */

#ifndef GL_EXT_multisample
#define GL_EXT_multisample 1

#define GL_MULTISAMPLE_EXT 0x809D
#define GL_SAMPLE_ALPHA_TO_MASK_EXT 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_EXT 0x809F
#define GL_SAMPLE_MASK_EXT 0x80A0
#define GL_1PASS_EXT 0x80A1
#define GL_2PASS_0_EXT 0x80A2
#define GL_2PASS_1_EXT 0x80A3
#define GL_4PASS_0_EXT 0x80A4
#define GL_4PASS_1_EXT 0x80A5
#define GL_4PASS_2_EXT 0x80A6
#define GL_4PASS_3_EXT 0x80A7
#define GL_SAMPLE_BUFFERS_EXT 0x80A8
#define GL_SAMPLES_EXT 0x80A9
#define GL_SAMPLE_MASK_VALUE_EXT 0x80AA
#define GL_SAMPLE_MASK_INVERT_EXT 0x80AB
#define GL_SAMPLE_PATTERN_EXT 0x80AC
#define GL_MULTISAMPLE_BIT_EXT 0x20000000

typedef void (GLAPIENTRY * PFNGLSAMPLEMASKEXTPROC) (GLclampf value, GLboolean invert);
typedef void (GLAPIENTRY * PFNGLSAMPLEPATTERNEXTPROC) (GLenum pattern);

#define glSampleMaskEXT GLEW_GET_FUN(__glewSampleMaskEXT)
#define glSamplePatternEXT GLEW_GET_FUN(__glewSamplePatternEXT)

#define GLEW_EXT_multisample GLEW_GET_VAR(__GLEW_EXT_multisample)

#endif /* GL_EXT_multisample */

/* ---------------------- GL_EXT_packed_depth_stencil ---------------------- */

#ifndef GL_EXT_packed_depth_stencil
#define GL_EXT_packed_depth_stencil 1

#define GL_DEPTH_STENCIL_EXT 0x84F9
#define GL_UNSIGNED_INT_24_8_EXT 0x84FA
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT 0x88F1

#define GLEW_EXT_packed_depth_stencil GLEW_GET_VAR(__GLEW_EXT_packed_depth_stencil)

#endif /* GL_EXT_packed_depth_stencil */

/* -------------------------- GL_EXT_packed_float -------------------------- */

#ifndef GL_EXT_packed_float
#define GL_EXT_packed_float 1

#define GL_R11F_G11F_B10F_EXT 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_EXT 0x8C3B
#define GL_RGBA_SIGNED_COMPONENTS_EXT 0x8C3C

#define GLEW_EXT_packed_float GLEW_GET_VAR(__GLEW_EXT_packed_float)

#endif /* GL_EXT_packed_float */

/* -------------------------- GL_EXT_packed_pixels ------------------------- */

#ifndef GL_EXT_packed_pixels
#define GL_EXT_packed_pixels 1

#define GL_UNSIGNED_BYTE_3_3_2_EXT 0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4_EXT 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1_EXT 0x8034
#define GL_UNSIGNED_INT_8_8_8_8_EXT 0x8035
#define GL_UNSIGNED_INT_10_10_10_2_EXT 0x8036

#define GLEW_EXT_packed_pixels GLEW_GET_VAR(__GLEW_EXT_packed_pixels)

#endif /* GL_EXT_packed_pixels */

/* ------------------------ GL_EXT_paletted_texture ------------------------ */

#ifndef GL_EXT_paletted_texture
#define GL_EXT_paletted_texture 1

#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_PROXY_TEXTURE_1D 0x8063
#define GL_PROXY_TEXTURE_2D 0x8064
#define GL_TEXTURE_3D_EXT 0x806F
#define GL_PROXY_TEXTURE_3D_EXT 0x8070
#define GL_COLOR_TABLE_FORMAT_EXT 0x80D8
#define GL_COLOR_TABLE_WIDTH_EXT 0x80D9
#define GL_COLOR_TABLE_RED_SIZE_EXT 0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE_EXT 0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE_EXT 0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE_EXT 0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE_EXT 0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE_EXT 0x80DF
#define GL_COLOR_INDEX1_EXT 0x80E2
#define GL_COLOR_INDEX2_EXT 0x80E3
#define GL_COLOR_INDEX4_EXT 0x80E4
#define GL_COLOR_INDEX8_EXT 0x80E5
#define GL_COLOR_INDEX12_EXT 0x80E6
#define GL_COLOR_INDEX16_EXT 0x80E7
#define GL_TEXTURE_INDEX_SIZE_EXT 0x80ED
#define GL_TEXTURE_CUBE_MAP_ARB 0x8513
#define GL_PROXY_TEXTURE_CUBE_MAP_ARB 0x851B

typedef void (GLAPIENTRY * PFNGLCOLORTABLEEXTPROC) (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const void* data);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEEXTPROC) (GLenum target, GLenum format, GLenum type, void* data);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint* params);

#define glColorTableEXT GLEW_GET_FUN(__glewColorTableEXT)
#define glGetColorTableEXT GLEW_GET_FUN(__glewGetColorTableEXT)
#define glGetColorTableParameterfvEXT GLEW_GET_FUN(__glewGetColorTableParameterfvEXT)
#define glGetColorTableParameterivEXT GLEW_GET_FUN(__glewGetColorTableParameterivEXT)

#define GLEW_EXT_paletted_texture GLEW_GET_VAR(__GLEW_EXT_paletted_texture)

#endif /* GL_EXT_paletted_texture */

/* ----------------------- GL_EXT_pixel_buffer_object ---------------------- */

#ifndef GL_EXT_pixel_buffer_object
#define GL_EXT_pixel_buffer_object 1

#define GL_PIXEL_PACK_BUFFER_EXT 0x88EB
#define GL_PIXEL_UNPACK_BUFFER_EXT 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_EXT 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_EXT 0x88EF

#define GLEW_EXT_pixel_buffer_object GLEW_GET_VAR(__GLEW_EXT_pixel_buffer_object)

#endif /* GL_EXT_pixel_buffer_object */

/* ------------------------- GL_EXT_pixel_transform ------------------------ */

#ifndef GL_EXT_pixel_transform
#define GL_EXT_pixel_transform 1

#define GL_PIXEL_TRANSFORM_2D_EXT 0x8330
#define GL_PIXEL_MAG_FILTER_EXT 0x8331
#define GL_PIXEL_MIN_FILTER_EXT 0x8332
#define GL_PIXEL_CUBIC_WEIGHT_EXT 0x8333
#define GL_CUBIC_EXT 0x8334
#define GL_AVERAGE_EXT 0x8335
#define GL_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT 0x8336
#define GL_MAX_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT 0x8337
#define GL_PIXEL_TRANSFORM_2D_MATRIX_EXT 0x8338

typedef void (GLAPIENTRY * PFNGLGETPIXELTRANSFORMPARAMETERFVEXTPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETPIXELTRANSFORMPARAMETERIVEXTPROC) (GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFORMPARAMETERFEXTPROC) (GLenum target, GLenum pname, const GLfloat param);
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFORMPARAMETERIEXTPROC) (GLenum target, GLenum pname, const GLint param);
typedef void (GLAPIENTRY * PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC) (GLenum target, GLenum pname, const GLint* params);

#define glGetPixelTransformParameterfvEXT GLEW_GET_FUN(__glewGetPixelTransformParameterfvEXT)
#define glGetPixelTransformParameterivEXT GLEW_GET_FUN(__glewGetPixelTransformParameterivEXT)
#define glPixelTransformParameterfEXT GLEW_GET_FUN(__glewPixelTransformParameterfEXT)
#define glPixelTransformParameterfvEXT GLEW_GET_FUN(__glewPixelTransformParameterfvEXT)
#define glPixelTransformParameteriEXT GLEW_GET_FUN(__glewPixelTransformParameteriEXT)
#define glPixelTransformParameterivEXT GLEW_GET_FUN(__glewPixelTransformParameterivEXT)

#define GLEW_EXT_pixel_transform GLEW_GET_VAR(__GLEW_EXT_pixel_transform)

#endif /* GL_EXT_pixel_transform */

/* ------------------- GL_EXT_pixel_transform_color_table ------------------ */

#ifndef GL_EXT_pixel_transform_color_table
#define GL_EXT_pixel_transform_color_table 1

#define GLEW_EXT_pixel_transform_color_table GLEW_GET_VAR(__GLEW_EXT_pixel_transform_color_table)

#endif /* GL_EXT_pixel_transform_color_table */

/* ------------------------ GL_EXT_point_parameters ------------------------ */

#ifndef GL_EXT_point_parameters
#define GL_EXT_point_parameters 1

#define GL_POINT_SIZE_MIN_EXT 0x8126
#define GL_POINT_SIZE_MAX_EXT 0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT 0x8128
#define GL_DISTANCE_ATTENUATION_EXT 0x8129

typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFEXTPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERFVEXTPROC) (GLenum pname, const GLfloat* params);

#define glPointParameterfEXT GLEW_GET_FUN(__glewPointParameterfEXT)
#define glPointParameterfvEXT GLEW_GET_FUN(__glewPointParameterfvEXT)

#define GLEW_EXT_point_parameters GLEW_GET_VAR(__GLEW_EXT_point_parameters)

#endif /* GL_EXT_point_parameters */

/* ------------------------- GL_EXT_polygon_offset ------------------------- */

#ifndef GL_EXT_polygon_offset
#define GL_EXT_polygon_offset 1

#define GL_POLYGON_OFFSET_EXT 0x8037
#define GL_POLYGON_OFFSET_FACTOR_EXT 0x8038
#define GL_POLYGON_OFFSET_BIAS_EXT 0x8039

typedef void (GLAPIENTRY * PFNGLPOLYGONOFFSETEXTPROC) (GLfloat factor, GLfloat bias);

#define glPolygonOffsetEXT GLEW_GET_FUN(__glewPolygonOffsetEXT)

#define GLEW_EXT_polygon_offset GLEW_GET_VAR(__GLEW_EXT_polygon_offset)

#endif /* GL_EXT_polygon_offset */

/* ------------------------ GL_EXT_provoking_vertex ------------------------ */

#ifndef GL_EXT_provoking_vertex
#define GL_EXT_provoking_vertex 1

#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION_EXT 0x8E4C
#define GL_FIRST_VERTEX_CONVENTION_EXT 0x8E4D
#define GL_LAST_VERTEX_CONVENTION_EXT 0x8E4E
#define GL_PROVOKING_VERTEX_EXT 0x8E4F

typedef void (GLAPIENTRY * PFNGLPROVOKINGVERTEXEXTPROC) (GLenum mode);

#define glProvokingVertexEXT GLEW_GET_FUN(__glewProvokingVertexEXT)

#define GLEW_EXT_provoking_vertex GLEW_GET_VAR(__GLEW_EXT_provoking_vertex)

#endif /* GL_EXT_provoking_vertex */

/* ------------------------- GL_EXT_rescale_normal ------------------------- */

#ifndef GL_EXT_rescale_normal
#define GL_EXT_rescale_normal 1

#define GL_RESCALE_NORMAL_EXT 0x803A

#define GLEW_EXT_rescale_normal GLEW_GET_VAR(__GLEW_EXT_rescale_normal)

#endif /* GL_EXT_rescale_normal */

/* -------------------------- GL_EXT_scene_marker -------------------------- */

#ifndef GL_EXT_scene_marker
#define GL_EXT_scene_marker 1

typedef void (GLAPIENTRY * PFNGLBEGINSCENEEXTPROC) (void);
typedef void (GLAPIENTRY * PFNGLENDSCENEEXTPROC) (void);

#define glBeginSceneEXT GLEW_GET_FUN(__glewBeginSceneEXT)
#define glEndSceneEXT GLEW_GET_FUN(__glewEndSceneEXT)

#define GLEW_EXT_scene_marker GLEW_GET_VAR(__GLEW_EXT_scene_marker)

#endif /* GL_EXT_scene_marker */

/* ------------------------- GL_EXT_secondary_color ------------------------ */

#ifndef GL_EXT_secondary_color
#define GL_EXT_secondary_color 1

#define GL_COLOR_SUM_EXT 0x8458
#define GL_CURRENT_SECONDARY_COLOR_EXT 0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE_EXT 0x845A
#define GL_SECONDARY_COLOR_ARRAY_TYPE_EXT 0x845B
#define GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT 0x845C
#define GL_SECONDARY_COLOR_ARRAY_POINTER_EXT 0x845D
#define GL_SECONDARY_COLOR_ARRAY_EXT 0x845E

typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3BEXTPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3BVEXTPROC) (const GLbyte *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3DEXTPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3DVEXTPROC) (const GLdouble *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3FEXTPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3FVEXTPROC) (const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3IEXTPROC) (GLint red, GLint green, GLint blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3IVEXTPROC) (const GLint *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3SEXTPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3SVEXTPROC) (const GLshort *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UBEXTPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UBVEXTPROC) (const GLubyte *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UIEXTPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3UIVEXTPROC) (const GLuint *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3USEXTPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3USVEXTPROC) (const GLushort *v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);

#define glSecondaryColor3bEXT GLEW_GET_FUN(__glewSecondaryColor3bEXT)
#define glSecondaryColor3bvEXT GLEW_GET_FUN(__glewSecondaryColor3bvEXT)
#define glSecondaryColor3dEXT GLEW_GET_FUN(__glewSecondaryColor3dEXT)
#define glSecondaryColor3dvEXT GLEW_GET_FUN(__glewSecondaryColor3dvEXT)
#define glSecondaryColor3fEXT GLEW_GET_FUN(__glewSecondaryColor3fEXT)
#define glSecondaryColor3fvEXT GLEW_GET_FUN(__glewSecondaryColor3fvEXT)
#define glSecondaryColor3iEXT GLEW_GET_FUN(__glewSecondaryColor3iEXT)
#define glSecondaryColor3ivEXT GLEW_GET_FUN(__glewSecondaryColor3ivEXT)
#define glSecondaryColor3sEXT GLEW_GET_FUN(__glewSecondaryColor3sEXT)
#define glSecondaryColor3svEXT GLEW_GET_FUN(__glewSecondaryColor3svEXT)
#define glSecondaryColor3ubEXT GLEW_GET_FUN(__glewSecondaryColor3ubEXT)
#define glSecondaryColor3ubvEXT GLEW_GET_FUN(__glewSecondaryColor3ubvEXT)
#define glSecondaryColor3uiEXT GLEW_GET_FUN(__glewSecondaryColor3uiEXT)
#define glSecondaryColor3uivEXT GLEW_GET_FUN(__glewSecondaryColor3uivEXT)
#define glSecondaryColor3usEXT GLEW_GET_FUN(__glewSecondaryColor3usEXT)
#define glSecondaryColor3usvEXT GLEW_GET_FUN(__glewSecondaryColor3usvEXT)
#define glSecondaryColorPointerEXT GLEW_GET_FUN(__glewSecondaryColorPointerEXT)

#define GLEW_EXT_secondary_color GLEW_GET_VAR(__GLEW_EXT_secondary_color)

#endif /* GL_EXT_secondary_color */

/* --------------------- GL_EXT_separate_shader_objects -------------------- */

#ifndef GL_EXT_separate_shader_objects
#define GL_EXT_separate_shader_objects 1

#define GL_ACTIVE_PROGRAM_EXT 0x8B8D

typedef void (GLAPIENTRY * PFNGLACTIVEPROGRAMEXTPROC) (GLuint program);
typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROGRAMEXTPROC) (GLenum type, const char* string);
typedef void (GLAPIENTRY * PFNGLUSESHADERPROGRAMEXTPROC) (GLenum type, GLuint program);

#define glActiveProgramEXT GLEW_GET_FUN(__glewActiveProgramEXT)
#define glCreateShaderProgramEXT GLEW_GET_FUN(__glewCreateShaderProgramEXT)
#define glUseShaderProgramEXT GLEW_GET_FUN(__glewUseShaderProgramEXT)

#define GLEW_EXT_separate_shader_objects GLEW_GET_VAR(__GLEW_EXT_separate_shader_objects)

#endif /* GL_EXT_separate_shader_objects */

/* --------------------- GL_EXT_separate_specular_color -------------------- */

#ifndef GL_EXT_separate_specular_color
#define GL_EXT_separate_specular_color 1

#define GL_LIGHT_MODEL_COLOR_CONTROL_EXT 0x81F8
#define GL_SINGLE_COLOR_EXT 0x81F9
#define GL_SEPARATE_SPECULAR_COLOR_EXT 0x81FA

#define GLEW_EXT_separate_specular_color GLEW_GET_VAR(__GLEW_EXT_separate_specular_color)

#endif /* GL_EXT_separate_specular_color */

/* --------------------- GL_EXT_shader_image_load_store -------------------- */

#ifndef GL_EXT_shader_image_load_store
#define GL_EXT_shader_image_load_store 1

#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT_EXT 0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT_EXT 0x00000002
#define GL_UNIFORM_BARRIER_BIT_EXT 0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT_EXT 0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT 0x00000020
#define GL_COMMAND_BARRIER_BIT_EXT 0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT_EXT 0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT_EXT 0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT_EXT 0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT_EXT 0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT_EXT 0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT_EXT 0x00001000
#define GL_MAX_IMAGE_UNITS_EXT 0x8F38
#define GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS_EXT 0x8F39
#define GL_IMAGE_BINDING_NAME_EXT 0x8F3A
#define GL_IMAGE_BINDING_LEVEL_EXT 0x8F3B
#define GL_IMAGE_BINDING_LAYERED_EXT 0x8F3C
#define GL_IMAGE_BINDING_LAYER_EXT 0x8F3D
#define GL_IMAGE_BINDING_ACCESS_EXT 0x8F3E
#define GL_IMAGE_1D_EXT 0x904C
#define GL_IMAGE_2D_EXT 0x904D
#define GL_IMAGE_3D_EXT 0x904E
#define GL_IMAGE_2D_RECT_EXT 0x904F
#define GL_IMAGE_CUBE_EXT 0x9050
#define GL_IMAGE_BUFFER_EXT 0x9051
#define GL_IMAGE_1D_ARRAY_EXT 0x9052
#define GL_IMAGE_2D_ARRAY_EXT 0x9053
#define GL_IMAGE_CUBE_MAP_ARRAY_EXT 0x9054
#define GL_IMAGE_2D_MULTISAMPLE_EXT 0x9055
#define GL_IMAGE_2D_MULTISAMPLE_ARRAY_EXT 0x9056
#define GL_INT_IMAGE_1D_EXT 0x9057
#define GL_INT_IMAGE_2D_EXT 0x9058
#define GL_INT_IMAGE_3D_EXT 0x9059
#define GL_INT_IMAGE_2D_RECT_EXT 0x905A
#define GL_INT_IMAGE_CUBE_EXT 0x905B
#define GL_INT_IMAGE_BUFFER_EXT 0x905C
#define GL_INT_IMAGE_1D_ARRAY_EXT 0x905D
#define GL_INT_IMAGE_2D_ARRAY_EXT 0x905E
#define GL_INT_IMAGE_CUBE_MAP_ARRAY_EXT 0x905F
#define GL_INT_IMAGE_2D_MULTISAMPLE_EXT 0x9060
#define GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT 0x9061
#define GL_UNSIGNED_INT_IMAGE_1D_EXT 0x9062
#define GL_UNSIGNED_INT_IMAGE_2D_EXT 0x9063
#define GL_UNSIGNED_INT_IMAGE_3D_EXT 0x9064
#define GL_UNSIGNED_INT_IMAGE_2D_RECT_EXT 0x9065
#define GL_UNSIGNED_INT_IMAGE_CUBE_EXT 0x9066
#define GL_UNSIGNED_INT_IMAGE_BUFFER_EXT 0x9067
#define GL_UNSIGNED_INT_IMAGE_1D_ARRAY_EXT 0x9068
#define GL_UNSIGNED_INT_IMAGE_2D_ARRAY_EXT 0x9069
#define GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_EXT 0x906A
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_EXT 0x906B
#define GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY_EXT 0x906C
#define GL_MAX_IMAGE_SAMPLES_EXT 0x906D
#define GL_IMAGE_BINDING_FORMAT_EXT 0x906E
#define GL_ALL_BARRIER_BITS_EXT 0xFFFFFFFF

typedef void (GLAPIENTRY * PFNGLBINDIMAGETEXTUREEXTPROC) (GLuint index, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLint format);
typedef void (GLAPIENTRY * PFNGLMEMORYBARRIEREXTPROC) (GLbitfield barriers);

#define glBindImageTextureEXT GLEW_GET_FUN(__glewBindImageTextureEXT)
#define glMemoryBarrierEXT GLEW_GET_FUN(__glewMemoryBarrierEXT)

#define GLEW_EXT_shader_image_load_store GLEW_GET_VAR(__GLEW_EXT_shader_image_load_store)

#endif /* GL_EXT_shader_image_load_store */

/* -------------------------- GL_EXT_shadow_funcs -------------------------- */

#ifndef GL_EXT_shadow_funcs
#define GL_EXT_shadow_funcs 1

#define GLEW_EXT_shadow_funcs GLEW_GET_VAR(__GLEW_EXT_shadow_funcs)

#endif /* GL_EXT_shadow_funcs */

/* --------------------- GL_EXT_shared_texture_palette --------------------- */

#ifndef GL_EXT_shared_texture_palette
#define GL_EXT_shared_texture_palette 1

#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

#define GLEW_EXT_shared_texture_palette GLEW_GET_VAR(__GLEW_EXT_shared_texture_palette)

#endif /* GL_EXT_shared_texture_palette */

/* ------------------------ GL_EXT_stencil_clear_tag ----------------------- */

#ifndef GL_EXT_stencil_clear_tag
#define GL_EXT_stencil_clear_tag 1

#define GL_STENCIL_TAG_BITS_EXT 0x88F2
#define GL_STENCIL_CLEAR_TAG_VALUE_EXT 0x88F3

#define GLEW_EXT_stencil_clear_tag GLEW_GET_VAR(__GLEW_EXT_stencil_clear_tag)

#endif /* GL_EXT_stencil_clear_tag */

/* ------------------------ GL_EXT_stencil_two_side ------------------------ */

#ifndef GL_EXT_stencil_two_side
#define GL_EXT_stencil_two_side 1

#define GL_STENCIL_TEST_TWO_SIDE_EXT 0x8910
#define GL_ACTIVE_STENCIL_FACE_EXT 0x8911

typedef void (GLAPIENTRY * PFNGLACTIVESTENCILFACEEXTPROC) (GLenum face);

#define glActiveStencilFaceEXT GLEW_GET_FUN(__glewActiveStencilFaceEXT)

#define GLEW_EXT_stencil_two_side GLEW_GET_VAR(__GLEW_EXT_stencil_two_side)

#endif /* GL_EXT_stencil_two_side */

/* -------------------------- GL_EXT_stencil_wrap -------------------------- */

#ifndef GL_EXT_stencil_wrap
#define GL_EXT_stencil_wrap 1

#define GL_INCR_WRAP_EXT 0x8507
#define GL_DECR_WRAP_EXT 0x8508

#define GLEW_EXT_stencil_wrap GLEW_GET_VAR(__GLEW_EXT_stencil_wrap)

#endif /* GL_EXT_stencil_wrap */

/* --------------------------- GL_EXT_subtexture --------------------------- */

#ifndef GL_EXT_subtexture
#define GL_EXT_subtexture 1

typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE1DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE2DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE3DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);

#define glTexSubImage1DEXT GLEW_GET_FUN(__glewTexSubImage1DEXT)
#define glTexSubImage2DEXT GLEW_GET_FUN(__glewTexSubImage2DEXT)
#define glTexSubImage3DEXT GLEW_GET_FUN(__glewTexSubImage3DEXT)

#define GLEW_EXT_subtexture GLEW_GET_VAR(__GLEW_EXT_subtexture)

#endif /* GL_EXT_subtexture */

/* ----------------------------- GL_EXT_texture ---------------------------- */

#ifndef GL_EXT_texture
#define GL_EXT_texture 1

#define GL_ALPHA4_EXT 0x803B
#define GL_ALPHA8_EXT 0x803C
#define GL_ALPHA12_EXT 0x803D
#define GL_ALPHA16_EXT 0x803E
#define GL_LUMINANCE4_EXT 0x803F
#define GL_LUMINANCE8_EXT 0x8040
#define GL_LUMINANCE12_EXT 0x8041
#define GL_LUMINANCE16_EXT 0x8042
#define GL_LUMINANCE4_ALPHA4_EXT 0x8043
#define GL_LUMINANCE6_ALPHA2_EXT 0x8044
#define GL_LUMINANCE8_ALPHA8_EXT 0x8045
#define GL_LUMINANCE12_ALPHA4_EXT 0x8046
#define GL_LUMINANCE12_ALPHA12_EXT 0x8047
#define GL_LUMINANCE16_ALPHA16_EXT 0x8048
#define GL_INTENSITY_EXT 0x8049
#define GL_INTENSITY4_EXT 0x804A
#define GL_INTENSITY8_EXT 0x804B
#define GL_INTENSITY12_EXT 0x804C
#define GL_INTENSITY16_EXT 0x804D
#define GL_RGB2_EXT 0x804E
#define GL_RGB4_EXT 0x804F
#define GL_RGB5_EXT 0x8050
#define GL_RGB8_EXT 0x8051
#define GL_RGB10_EXT 0x8052
#define GL_RGB12_EXT 0x8053
#define GL_RGB16_EXT 0x8054
#define GL_RGBA2_EXT 0x8055
#define GL_RGBA4_EXT 0x8056
#define GL_RGB5_A1_EXT 0x8057
#define GL_RGBA8_EXT 0x8058
#define GL_RGB10_A2_EXT 0x8059
#define GL_RGBA12_EXT 0x805A
#define GL_RGBA16_EXT 0x805B
#define GL_TEXTURE_RED_SIZE_EXT 0x805C
#define GL_TEXTURE_GREEN_SIZE_EXT 0x805D
#define GL_TEXTURE_BLUE_SIZE_EXT 0x805E
#define GL_TEXTURE_ALPHA_SIZE_EXT 0x805F
#define GL_TEXTURE_LUMINANCE_SIZE_EXT 0x8060
#define GL_TEXTURE_INTENSITY_SIZE_EXT 0x8061
#define GL_REPLACE_EXT 0x8062
#define GL_PROXY_TEXTURE_1D_EXT 0x8063
#define GL_PROXY_TEXTURE_2D_EXT 0x8064

#define GLEW_EXT_texture GLEW_GET_VAR(__GLEW_EXT_texture)

#endif /* GL_EXT_texture */

/* ---------------------------- GL_EXT_texture3D --------------------------- */

#ifndef GL_EXT_texture3D
#define GL_EXT_texture3D 1

#define GL_PACK_SKIP_IMAGES_EXT 0x806B
#define GL_PACK_IMAGE_HEIGHT_EXT 0x806C
#define GL_UNPACK_SKIP_IMAGES_EXT 0x806D
#define GL_UNPACK_IMAGE_HEIGHT_EXT 0x806E
#define GL_TEXTURE_3D_EXT 0x806F
#define GL_PROXY_TEXTURE_3D_EXT 0x8070
#define GL_TEXTURE_DEPTH_EXT 0x8071
#define GL_TEXTURE_WRAP_R_EXT 0x8072
#define GL_MAX_3D_TEXTURE_SIZE_EXT 0x8073

typedef void (GLAPIENTRY * PFNGLTEXIMAGE3DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);

#define glTexImage3DEXT GLEW_GET_FUN(__glewTexImage3DEXT)

#define GLEW_EXT_texture3D GLEW_GET_VAR(__GLEW_EXT_texture3D)

#endif /* GL_EXT_texture3D */

/* -------------------------- GL_EXT_texture_array ------------------------- */

#ifndef GL_EXT_texture_array
#define GL_EXT_texture_array 1

#define GL_COMPARE_REF_DEPTH_TO_TEXTURE_EXT 0x884E
#define GL_MAX_ARRAY_TEXTURE_LAYERS_EXT 0x88FF
#define GL_TEXTURE_1D_ARRAY_EXT 0x8C18
#define GL_PROXY_TEXTURE_1D_ARRAY_EXT 0x8C19
#define GL_TEXTURE_2D_ARRAY_EXT 0x8C1A
#define GL_PROXY_TEXTURE_2D_ARRAY_EXT 0x8C1B
#define GL_TEXTURE_BINDING_1D_ARRAY_EXT 0x8C1C
#define GL_TEXTURE_BINDING_2D_ARRAY_EXT 0x8C1D

#define GLEW_EXT_texture_array GLEW_GET_VAR(__GLEW_EXT_texture_array)

#endif /* GL_EXT_texture_array */

/* ---------------------- GL_EXT_texture_buffer_object --------------------- */

#ifndef GL_EXT_texture_buffer_object
#define GL_EXT_texture_buffer_object 1

#define GL_TEXTURE_BUFFER_EXT 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE_EXT 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER_EXT 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT_EXT 0x8C2E

typedef void (GLAPIENTRY * PFNGLTEXBUFFEREXTPROC) (GLenum target, GLenum internalformat, GLuint buffer);

#define glTexBufferEXT GLEW_GET_FUN(__glewTexBufferEXT)

#define GLEW_EXT_texture_buffer_object GLEW_GET_VAR(__GLEW_EXT_texture_buffer_object)

#endif /* GL_EXT_texture_buffer_object */

/* -------------------- GL_EXT_texture_compression_dxt1 -------------------- */

#ifndef GL_EXT_texture_compression_dxt1
#define GL_EXT_texture_compression_dxt1 1

#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1

#define GLEW_EXT_texture_compression_dxt1 GLEW_GET_VAR(__GLEW_EXT_texture_compression_dxt1)

#endif /* GL_EXT_texture_compression_dxt1 */

/* -------------------- GL_EXT_texture_compression_latc -------------------- */

#ifndef GL_EXT_texture_compression_latc
#define GL_EXT_texture_compression_latc 1

#define GL_COMPRESSED_LUMINANCE_LATC1_EXT 0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT 0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT 0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT 0x8C73

#define GLEW_EXT_texture_compression_latc GLEW_GET_VAR(__GLEW_EXT_texture_compression_latc)

#endif /* GL_EXT_texture_compression_latc */

/* -------------------- GL_EXT_texture_compression_rgtc -------------------- */

#ifndef GL_EXT_texture_compression_rgtc
#define GL_EXT_texture_compression_rgtc 1

#define GL_COMPRESSED_RED_RGTC1_EXT 0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1_EXT 0x8DBC
#define GL_COMPRESSED_RED_GREEN_RGTC2_EXT 0x8DBD
#define GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT 0x8DBE

#define GLEW_EXT_texture_compression_rgtc GLEW_GET_VAR(__GLEW_EXT_texture_compression_rgtc)

#endif /* GL_EXT_texture_compression_rgtc */

/* -------------------- GL_EXT_texture_compression_s3tc -------------------- */

#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1

#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3

#define GLEW_EXT_texture_compression_s3tc GLEW_GET_VAR(__GLEW_EXT_texture_compression_s3tc)

#endif /* GL_EXT_texture_compression_s3tc */

/* ------------------------ GL_EXT_texture_cube_map ------------------------ */

#ifndef GL_EXT_texture_cube_map
#define GL_EXT_texture_cube_map 1

#define GL_NORMAL_MAP_EXT 0x8511
#define GL_REFLECTION_MAP_EXT 0x8512
#define GL_TEXTURE_CUBE_MAP_EXT 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_EXT 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_EXT 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_EXT 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_EXT 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_EXT 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EXT 0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP_EXT 0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_EXT 0x851C

#define GLEW_EXT_texture_cube_map GLEW_GET_VAR(__GLEW_EXT_texture_cube_map)

#endif /* GL_EXT_texture_cube_map */

/* ----------------------- GL_EXT_texture_edge_clamp ----------------------- */

#ifndef GL_EXT_texture_edge_clamp
#define GL_EXT_texture_edge_clamp 1

#define GL_CLAMP_TO_EDGE_EXT 0x812F

#define GLEW_EXT_texture_edge_clamp GLEW_GET_VAR(__GLEW_EXT_texture_edge_clamp)

#endif /* GL_EXT_texture_edge_clamp */

/* --------------------------- GL_EXT_texture_env -------------------------- */

#ifndef GL_EXT_texture_env
#define GL_EXT_texture_env 1

#define GL_TEXTURE_ENV0_EXT 0
#define GL_ENV_BLEND_EXT 0
#define GL_TEXTURE_ENV_SHIFT_EXT 0
#define GL_ENV_REPLACE_EXT 0
#define GL_ENV_ADD_EXT 0
#define GL_ENV_SUBTRACT_EXT 0
#define GL_TEXTURE_ENV_MODE_ALPHA_EXT 0
#define GL_ENV_REVERSE_SUBTRACT_EXT 0
#define GL_ENV_REVERSE_BLEND_EXT 0
#define GL_ENV_COPY_EXT 0
#define GL_ENV_MODULATE_EXT 0

#define GLEW_EXT_texture_env GLEW_GET_VAR(__GLEW_EXT_texture_env)

#endif /* GL_EXT_texture_env */

/* ------------------------- GL_EXT_texture_env_add ------------------------ */

#ifndef GL_EXT_texture_env_add
#define GL_EXT_texture_env_add 1

#define GLEW_EXT_texture_env_add GLEW_GET_VAR(__GLEW_EXT_texture_env_add)

#endif /* GL_EXT_texture_env_add */

/* ----------------------- GL_EXT_texture_env_combine ---------------------- */

#ifndef GL_EXT_texture_env_combine
#define GL_EXT_texture_env_combine 1

#define GL_COMBINE_EXT 0x8570
#define GL_COMBINE_RGB_EXT 0x8571
#define GL_COMBINE_ALPHA_EXT 0x8572
#define GL_RGB_SCALE_EXT 0x8573
#define GL_ADD_SIGNED_EXT 0x8574
#define GL_INTERPOLATE_EXT 0x8575
#define GL_CONSTANT_EXT 0x8576
#define GL_PRIMARY_COLOR_EXT 0x8577
#define GL_PREVIOUS_EXT 0x8578
#define GL_SOURCE0_RGB_EXT 0x8580
#define GL_SOURCE1_RGB_EXT 0x8581
#define GL_SOURCE2_RGB_EXT 0x8582
#define GL_SOURCE0_ALPHA_EXT 0x8588
#define GL_SOURCE1_ALPHA_EXT 0x8589
#define GL_SOURCE2_ALPHA_EXT 0x858A
#define GL_OPERAND0_RGB_EXT 0x8590
#define GL_OPERAND1_RGB_EXT 0x8591
#define GL_OPERAND2_RGB_EXT 0x8592
#define GL_OPERAND0_ALPHA_EXT 0x8598
#define GL_OPERAND1_ALPHA_EXT 0x8599
#define GL_OPERAND2_ALPHA_EXT 0x859A

#define GLEW_EXT_texture_env_combine GLEW_GET_VAR(__GLEW_EXT_texture_env_combine)

#endif /* GL_EXT_texture_env_combine */

/* ------------------------ GL_EXT_texture_env_dot3 ------------------------ */

#ifndef GL_EXT_texture_env_dot3
#define GL_EXT_texture_env_dot3 1

#define GL_DOT3_RGB_EXT 0x8740
#define GL_DOT3_RGBA_EXT 0x8741

#define GLEW_EXT_texture_env_dot3 GLEW_GET_VAR(__GLEW_EXT_texture_env_dot3)

#endif /* GL_EXT_texture_env_dot3 */

/* ------------------- GL_EXT_texture_filter_anisotropic ------------------- */

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1

#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define GLEW_EXT_texture_filter_anisotropic GLEW_GET_VAR(__GLEW_EXT_texture_filter_anisotropic)

#endif /* GL_EXT_texture_filter_anisotropic */

/* ------------------------- GL_EXT_texture_integer ------------------------ */

#ifndef GL_EXT_texture_integer
#define GL_EXT_texture_integer 1

#define GL_RGBA32UI_EXT 0x8D70
#define GL_RGB32UI_EXT 0x8D71
#define GL_ALPHA32UI_EXT 0x8D72
#define GL_INTENSITY32UI_EXT 0x8D73
#define GL_LUMINANCE32UI_EXT 0x8D74
#define GL_LUMINANCE_ALPHA32UI_EXT 0x8D75
#define GL_RGBA16UI_EXT 0x8D76
#define GL_RGB16UI_EXT 0x8D77
#define GL_ALPHA16UI_EXT 0x8D78
#define GL_INTENSITY16UI_EXT 0x8D79
#define GL_LUMINANCE16UI_EXT 0x8D7A
#define GL_LUMINANCE_ALPHA16UI_EXT 0x8D7B
#define GL_RGBA8UI_EXT 0x8D7C
#define GL_RGB8UI_EXT 0x8D7D
#define GL_ALPHA8UI_EXT 0x8D7E
#define GL_INTENSITY8UI_EXT 0x8D7F
#define GL_LUMINANCE8UI_EXT 0x8D80
#define GL_LUMINANCE_ALPHA8UI_EXT 0x8D81
#define GL_RGBA32I_EXT 0x8D82
#define GL_RGB32I_EXT 0x8D83
#define GL_ALPHA32I_EXT 0x8D84
#define GL_INTENSITY32I_EXT 0x8D85
#define GL_LUMINANCE32I_EXT 0x8D86
#define GL_LUMINANCE_ALPHA32I_EXT 0x8D87
#define GL_RGBA16I_EXT 0x8D88
#define GL_RGB16I_EXT 0x8D89
#define GL_ALPHA16I_EXT 0x8D8A
#define GL_INTENSITY16I_EXT 0x8D8B
#define GL_LUMINANCE16I_EXT 0x8D8C
#define GL_LUMINANCE_ALPHA16I_EXT 0x8D8D
#define GL_RGBA8I_EXT 0x8D8E
#define GL_RGB8I_EXT 0x8D8F
#define GL_ALPHA8I_EXT 0x8D90
#define GL_INTENSITY8I_EXT 0x8D91
#define GL_LUMINANCE8I_EXT 0x8D92
#define GL_LUMINANCE_ALPHA8I_EXT 0x8D93
#define GL_RED_INTEGER_EXT 0x8D94
#define GL_GREEN_INTEGER_EXT 0x8D95
#define GL_BLUE_INTEGER_EXT 0x8D96
#define GL_ALPHA_INTEGER_EXT 0x8D97
#define GL_RGB_INTEGER_EXT 0x8D98
#define GL_RGBA_INTEGER_EXT 0x8D99
#define GL_BGR_INTEGER_EXT 0x8D9A
#define GL_BGRA_INTEGER_EXT 0x8D9B
#define GL_LUMINANCE_INTEGER_EXT 0x8D9C
#define GL_LUMINANCE_ALPHA_INTEGER_EXT 0x8D9D
#define GL_RGBA_INTEGER_MODE_EXT 0x8D9E

typedef void (GLAPIENTRY * PFNGLCLEARCOLORIIEXTPROC) (GLint red, GLint green, GLint blue, GLint alpha);
typedef void (GLAPIENTRY * PFNGLCLEARCOLORIUIEXTPROC) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY * PFNGLGETTEXPARAMETERIUIVEXTPROC) (GLenum target, GLenum pname, GLuint *params);
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIIVEXTPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERIUIVEXTPROC) (GLenum target, GLenum pname, const GLuint *params);

#define glClearColorIiEXT GLEW_GET_FUN(__glewClearColorIiEXT)
#define glClearColorIuiEXT GLEW_GET_FUN(__glewClearColorIuiEXT)
#define glGetTexParameterIivEXT GLEW_GET_FUN(__glewGetTexParameterIivEXT)
#define glGetTexParameterIuivEXT GLEW_GET_FUN(__glewGetTexParameterIuivEXT)
#define glTexParameterIivEXT GLEW_GET_FUN(__glewTexParameterIivEXT)
#define glTexParameterIuivEXT GLEW_GET_FUN(__glewTexParameterIuivEXT)

#define GLEW_EXT_texture_integer GLEW_GET_VAR(__GLEW_EXT_texture_integer)

#endif /* GL_EXT_texture_integer */

/* ------------------------ GL_EXT_texture_lod_bias ------------------------ */

#ifndef GL_EXT_texture_lod_bias
#define GL_EXT_texture_lod_bias 1

#define GL_MAX_TEXTURE_LOD_BIAS_EXT 0x84FD
#define GL_TEXTURE_FILTER_CONTROL_EXT 0x8500
#define GL_TEXTURE_LOD_BIAS_EXT 0x8501

#define GLEW_EXT_texture_lod_bias GLEW_GET_VAR(__GLEW_EXT_texture_lod_bias)

#endif /* GL_EXT_texture_lod_bias */

/* ---------------------- GL_EXT_texture_mirror_clamp ---------------------- */

#ifndef GL_EXT_texture_mirror_clamp
#define GL_EXT_texture_mirror_clamp 1

#define GL_MIRROR_CLAMP_EXT 0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_EXT 0x8743
#define GL_MIRROR_CLAMP_TO_BORDER_EXT 0x8912

#define GLEW_EXT_texture_mirror_clamp GLEW_GET_VAR(__GLEW_EXT_texture_mirror_clamp)

#endif /* GL_EXT_texture_mirror_clamp */

/* ------------------------- GL_EXT_texture_object ------------------------- */

#ifndef GL_EXT_texture_object
#define GL_EXT_texture_object 1

#define GL_TEXTURE_PRIORITY_EXT 0x8066
#define GL_TEXTURE_RESIDENT_EXT 0x8067
#define GL_TEXTURE_1D_BINDING_EXT 0x8068
#define GL_TEXTURE_2D_BINDING_EXT 0x8069
#define GL_TEXTURE_3D_BINDING_EXT 0x806A

typedef GLboolean (GLAPIENTRY * PFNGLARETEXTURESRESIDENTEXTPROC) (GLsizei n, const GLuint* textures, GLboolean* residences);
typedef void (GLAPIENTRY * PFNGLBINDTEXTUREEXTPROC) (GLenum target, GLuint texture);
typedef void (GLAPIENTRY * PFNGLDELETETEXTURESEXTPROC) (GLsizei n, const GLuint* textures);
typedef void (GLAPIENTRY * PFNGLGENTEXTURESEXTPROC) (GLsizei n, GLuint* textures);
typedef GLboolean (GLAPIENTRY * PFNGLISTEXTUREEXTPROC) (GLuint texture);
typedef void (GLAPIENTRY * PFNGLPRIORITIZETEXTURESEXTPROC) (GLsizei n, const GLuint* textures, const GLclampf* priorities);

#define glAreTexturesResidentEXT GLEW_GET_FUN(__glewAreTexturesResidentEXT)
#define glBindTextureEXT GLEW_GET_FUN(__glewBindTextureEXT)
#define glDeleteTexturesEXT GLEW_GET_FUN(__glewDeleteTexturesEXT)
#define glGenTexturesEXT GLEW_GET_FUN(__glewGenTexturesEXT)
#define glIsTextureEXT GLEW_GET_FUN(__glewIsTextureEXT)
#define glPrioritizeTexturesEXT GLEW_GET_FUN(__glewPrioritizeTexturesEXT)

#define GLEW_EXT_texture_object GLEW_GET_VAR(__GLEW_EXT_texture_object)

#endif /* GL_EXT_texture_object */

/* --------------------- GL_EXT_texture_perturb_normal --------------------- */

#ifndef GL_EXT_texture_perturb_normal
#define GL_EXT_texture_perturb_normal 1

#define GL_PERTURB_EXT 0x85AE
#define GL_TEXTURE_NORMAL_EXT 0x85AF

typedef void (GLAPIENTRY * PFNGLTEXTURENORMALEXTPROC) (GLenum mode);

#define glTextureNormalEXT GLEW_GET_FUN(__glewTextureNormalEXT)

#define GLEW_EXT_texture_perturb_normal GLEW_GET_VAR(__GLEW_EXT_texture_perturb_normal)

#endif /* GL_EXT_texture_perturb_normal */

/* ------------------------ GL_EXT_texture_rectangle ----------------------- */

#ifndef GL_EXT_texture_rectangle
#define GL_EXT_texture_rectangle 1

#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_EXT 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_EXT 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT 0x84F8

#define GLEW_EXT_texture_rectangle GLEW_GET_VAR(__GLEW_EXT_texture_rectangle)

#endif /* GL_EXT_texture_rectangle */

/* -------------------------- GL_EXT_texture_sRGB -------------------------- */

#ifndef GL_EXT_texture_sRGB
#define GL_EXT_texture_sRGB 1

#define GL_SRGB_EXT 0x8C40
#define GL_SRGB8_EXT 0x8C41
#define GL_SRGB_ALPHA_EXT 0x8C42
#define GL_SRGB8_ALPHA8_EXT 0x8C43
#define GL_SLUMINANCE_ALPHA_EXT 0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT 0x8C45
#define GL_SLUMINANCE_EXT 0x8C46
#define GL_SLUMINANCE8_EXT 0x8C47
#define GL_COMPRESSED_SRGB_EXT 0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT 0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT 0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT 0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F

#define GLEW_EXT_texture_sRGB GLEW_GET_VAR(__GLEW_EXT_texture_sRGB)

#endif /* GL_EXT_texture_sRGB */

/* --------------------- GL_EXT_texture_shared_exponent -------------------- */

#ifndef GL_EXT_texture_shared_exponent
#define GL_EXT_texture_shared_exponent 1

#define GL_RGB9_E5_EXT 0x8C3D
#define GL_UNSIGNED_INT_5_9_9_9_REV_EXT 0x8C3E
#define GL_TEXTURE_SHARED_SIZE_EXT 0x8C3F

#define GLEW_EXT_texture_shared_exponent GLEW_GET_VAR(__GLEW_EXT_texture_shared_exponent)

#endif /* GL_EXT_texture_shared_exponent */

/* -------------------------- GL_EXT_texture_snorm ------------------------- */

#ifndef GL_EXT_texture_snorm
#define GL_EXT_texture_snorm 1

#define GL_RED_SNORM 0x8F90
#define GL_RG_SNORM 0x8F91
#define GL_RGB_SNORM 0x8F92
#define GL_RGBA_SNORM 0x8F93
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_R16_SNORM 0x8F98
#define GL_RG16_SNORM 0x8F99
#define GL_RGB16_SNORM 0x8F9A
#define GL_RGBA16_SNORM 0x8F9B
#define GL_SIGNED_NORMALIZED 0x8F9C
#define GL_ALPHA_SNORM 0x9010
#define GL_LUMINANCE_SNORM 0x9011
#define GL_LUMINANCE_ALPHA_SNORM 0x9012
#define GL_INTENSITY_SNORM 0x9013
#define GL_ALPHA8_SNORM 0x9014
#define GL_LUMINANCE8_SNORM 0x9015
#define GL_LUMINANCE8_ALPHA8_SNORM 0x9016
#define GL_INTENSITY8_SNORM 0x9017
#define GL_ALPHA16_SNORM 0x9018
#define GL_LUMINANCE16_SNORM 0x9019
#define GL_LUMINANCE16_ALPHA16_SNORM 0x901A
#define GL_INTENSITY16_SNORM 0x901B

#define GLEW_EXT_texture_snorm GLEW_GET_VAR(__GLEW_EXT_texture_snorm)

#endif /* GL_EXT_texture_snorm */

/* ------------------------- GL_EXT_texture_swizzle ------------------------ */

#ifndef GL_EXT_texture_swizzle
#define GL_EXT_texture_swizzle 1

#define GL_TEXTURE_SWIZZLE_R_EXT 0x8E42
#define GL_TEXTURE_SWIZZLE_G_EXT 0x8E43
#define GL_TEXTURE_SWIZZLE_B_EXT 0x8E44
#define GL_TEXTURE_SWIZZLE_A_EXT 0x8E45
#define GL_TEXTURE_SWIZZLE_RGBA_EXT 0x8E46

#define GLEW_EXT_texture_swizzle GLEW_GET_VAR(__GLEW_EXT_texture_swizzle)

#endif /* GL_EXT_texture_swizzle */

/* --------------------------- GL_EXT_timer_query -------------------------- */

#ifndef GL_EXT_timer_query
#define GL_EXT_timer_query 1

#define GL_TIME_ELAPSED_EXT 0x88BF

typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTI64VEXTPROC) (GLuint id, GLenum pname, GLint64EXT *params);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64EXT *params);

#define glGetQueryObjecti64vEXT GLEW_GET_FUN(__glewGetQueryObjecti64vEXT)
#define glGetQueryObjectui64vEXT GLEW_GET_FUN(__glewGetQueryObjectui64vEXT)

#define GLEW_EXT_timer_query GLEW_GET_VAR(__GLEW_EXT_timer_query)

#endif /* GL_EXT_timer_query */

/* ----------------------- GL_EXT_transform_feedback ----------------------- */

#ifndef GL_EXT_transform_feedback
#define GL_EXT_transform_feedback 1

#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH_EXT 0x8C76
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE_EXT 0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_EXT 0x8C80
#define GL_TRANSFORM_FEEDBACK_VARYINGS_EXT 0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START_EXT 0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_EXT 0x8C85
#define GL_PRIMITIVES_GENERATED_EXT 0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_EXT 0x8C88
#define GL_RASTERIZER_DISCARD_EXT 0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_EXT 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_EXT 0x8C8B
#define GL_INTERLEAVED_ATTRIBS_EXT 0x8C8C
#define GL_SEPARATE_ATTRIBS_EXT 0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER_EXT 0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_EXT 0x8C8F

typedef void (GLAPIENTRY * PFNGLBEGINTRANSFORMFEEDBACKEXTPROC) (GLenum primitiveMode);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERBASEEXTPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLBINDBUFFEROFFSETEXTPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERRANGEEXTPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GLAPIENTRY * PFNGLENDTRANSFORMFEEDBACKEXTPROC) (void);
typedef void (GLAPIENTRY * PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei *size, GLenum *type, char *name);
typedef void (GLAPIENTRY * PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC) (GLuint program, GLsizei count, const char ** varyings, GLenum bufferMode);

#define glBeginTransformFeedbackEXT GLEW_GET_FUN(__glewBeginTransformFeedbackEXT)
#define glBindBufferBaseEXT GLEW_GET_FUN(__glewBindBufferBaseEXT)
#define glBindBufferOffsetEXT GLEW_GET_FUN(__glewBindBufferOffsetEXT)
#define glBindBufferRangeEXT GLEW_GET_FUN(__glewBindBufferRangeEXT)
#define glEndTransformFeedbackEXT GLEW_GET_FUN(__glewEndTransformFeedbackEXT)
#define glGetTransformFeedbackVaryingEXT GLEW_GET_FUN(__glewGetTransformFeedbackVaryingEXT)
#define glTransformFeedbackVaryingsEXT GLEW_GET_FUN(__glewTransformFeedbackVaryingsEXT)

#define GLEW_EXT_transform_feedback GLEW_GET_VAR(__GLEW_EXT_transform_feedback)

#endif /* GL_EXT_transform_feedback */

/* -------------------------- GL_EXT_vertex_array -------------------------- */

#ifndef GL_EXT_vertex_array
#define GL_EXT_vertex_array 1

#define GL_DOUBLE_EXT 0x140A
#define GL_VERTEX_ARRAY_EXT 0x8074
#define GL_NORMAL_ARRAY_EXT 0x8075
#define GL_COLOR_ARRAY_EXT 0x8076
#define GL_INDEX_ARRAY_EXT 0x8077
#define GL_TEXTURE_COORD_ARRAY_EXT 0x8078
#define GL_EDGE_FLAG_ARRAY_EXT 0x8079
#define GL_VERTEX_ARRAY_SIZE_EXT 0x807A
#define GL_VERTEX_ARRAY_TYPE_EXT 0x807B
#define GL_VERTEX_ARRAY_STRIDE_EXT 0x807C
#define GL_VERTEX_ARRAY_COUNT_EXT 0x807D
#define GL_NORMAL_ARRAY_TYPE_EXT 0x807E
#define GL_NORMAL_ARRAY_STRIDE_EXT 0x807F
#define GL_NORMAL_ARRAY_COUNT_EXT 0x8080
#define GL_COLOR_ARRAY_SIZE_EXT 0x8081
#define GL_COLOR_ARRAY_TYPE_EXT 0x8082
#define GL_COLOR_ARRAY_STRIDE_EXT 0x8083
#define GL_COLOR_ARRAY_COUNT_EXT 0x8084
#define GL_INDEX_ARRAY_TYPE_EXT 0x8085
#define GL_INDEX_ARRAY_STRIDE_EXT 0x8086
#define GL_INDEX_ARRAY_COUNT_EXT 0x8087
#define GL_TEXTURE_COORD_ARRAY_SIZE_EXT 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE_EXT 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE_EXT 0x808A
#define GL_TEXTURE_COORD_ARRAY_COUNT_EXT 0x808B
#define GL_EDGE_FLAG_ARRAY_STRIDE_EXT 0x808C
#define GL_EDGE_FLAG_ARRAY_COUNT_EXT 0x808D
#define GL_VERTEX_ARRAY_POINTER_EXT 0x808E
#define GL_NORMAL_ARRAY_POINTER_EXT 0x808F
#define GL_COLOR_ARRAY_POINTER_EXT 0x8090
#define GL_INDEX_ARRAY_POINTER_EXT 0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER_EXT 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER_EXT 0x8093

typedef void (GLAPIENTRY * PFNGLARRAYELEMENTEXTPROC) (GLint i);
typedef void (GLAPIENTRY * PFNGLCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const void* pointer);
typedef void (GLAPIENTRY * PFNGLDRAWARRAYSEXTPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (GLAPIENTRY * PFNGLEDGEFLAGPOINTEREXTPROC) (GLsizei stride, GLsizei count, const GLboolean* pointer);
typedef void (GLAPIENTRY * PFNGLGETPOINTERVEXTPROC) (GLenum pname, void** params);
typedef void (GLAPIENTRY * PFNGLINDEXPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, const void* pointer);
typedef void (GLAPIENTRY * PFNGLNORMALPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, const void* pointer);
typedef void (GLAPIENTRY * PFNGLTEXCOORDPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const void* pointer);
typedef void (GLAPIENTRY * PFNGLVERTEXPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const void* pointer);

#define glArrayElementEXT GLEW_GET_FUN(__glewArrayElementEXT)
#define glColorPointerEXT GLEW_GET_FUN(__glewColorPointerEXT)
#define glDrawArraysEXT GLEW_GET_FUN(__glewDrawArraysEXT)
#define glEdgeFlagPointerEXT GLEW_GET_FUN(__glewEdgeFlagPointerEXT)
#define glGetPointervEXT GLEW_GET_FUN(__glewGetPointervEXT)
#define glIndexPointerEXT GLEW_GET_FUN(__glewIndexPointerEXT)
#define glNormalPointerEXT GLEW_GET_FUN(__glewNormalPointerEXT)
#define glTexCoordPointerEXT GLEW_GET_FUN(__glewTexCoordPointerEXT)
#define glVertexPointerEXT GLEW_GET_FUN(__glewVertexPointerEXT)

#define GLEW_EXT_vertex_array GLEW_GET_VAR(__GLEW_EXT_vertex_array)

#endif /* GL_EXT_vertex_array */

/* ------------------------ GL_EXT_vertex_array_bgra ----------------------- */

#ifndef GL_EXT_vertex_array_bgra
#define GL_EXT_vertex_array_bgra 1

#define GL_BGRA 0x80E1

#define GLEW_EXT_vertex_array_bgra GLEW_GET_VAR(__GLEW_EXT_vertex_array_bgra)

#endif /* GL_EXT_vertex_array_bgra */

/* ----------------------- GL_EXT_vertex_attrib_64bit ---------------------- */

#ifndef GL_EXT_vertex_attrib_64bit
#define GL_EXT_vertex_attrib_64bit 1

#define GL_DOUBLE_MAT2_EXT 0x8F46
#define GL_DOUBLE_MAT3_EXT 0x8F47
#define GL_DOUBLE_MAT4_EXT 0x8F48
#define GL_DOUBLE_VEC2_EXT 0x8FFC
#define GL_DOUBLE_VEC3_EXT 0x8FFD
#define GL_DOUBLE_VEC4_EXT 0x8FFE

typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBLDVEXTPROC) (GLuint index, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1DEXTPROC) (GLuint index, GLdouble x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1DVEXTPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2DEXTPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2DVEXTPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3DEXTPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3DVEXTPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4DEXTPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4DVEXTPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBLPOINTEREXTPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);

#define glGetVertexAttribLdvEXT GLEW_GET_FUN(__glewGetVertexAttribLdvEXT)
#define glVertexArrayVertexAttribLOffsetEXT GLEW_GET_FUN(__glewVertexArrayVertexAttribLOffsetEXT)
#define glVertexAttribL1dEXT GLEW_GET_FUN(__glewVertexAttribL1dEXT)
#define glVertexAttribL1dvEXT GLEW_GET_FUN(__glewVertexAttribL1dvEXT)
#define glVertexAttribL2dEXT GLEW_GET_FUN(__glewVertexAttribL2dEXT)
#define glVertexAttribL2dvEXT GLEW_GET_FUN(__glewVertexAttribL2dvEXT)
#define glVertexAttribL3dEXT GLEW_GET_FUN(__glewVertexAttribL3dEXT)
#define glVertexAttribL3dvEXT GLEW_GET_FUN(__glewVertexAttribL3dvEXT)
#define glVertexAttribL4dEXT GLEW_GET_FUN(__glewVertexAttribL4dEXT)
#define glVertexAttribL4dvEXT GLEW_GET_FUN(__glewVertexAttribL4dvEXT)
#define glVertexAttribLPointerEXT GLEW_GET_FUN(__glewVertexAttribLPointerEXT)

#define GLEW_EXT_vertex_attrib_64bit GLEW_GET_VAR(__GLEW_EXT_vertex_attrib_64bit)

#endif /* GL_EXT_vertex_attrib_64bit */

/* -------------------------- GL_EXT_vertex_shader ------------------------- */

#ifndef GL_EXT_vertex_shader
#define GL_EXT_vertex_shader 1

#define GL_VERTEX_SHADER_EXT 0x8780
#define GL_VERTEX_SHADER_BINDING_EXT 0x8781
#define GL_OP_INDEX_EXT 0x8782
#define GL_OP_NEGATE_EXT 0x8783
#define GL_OP_DOT3_EXT 0x8784
#define GL_OP_DOT4_EXT 0x8785
#define GL_OP_MUL_EXT 0x8786
#define GL_OP_ADD_EXT 0x8787
#define GL_OP_MADD_EXT 0x8788
#define GL_OP_FRAC_EXT 0x8789
#define GL_OP_MAX_EXT 0x878A
#define GL_OP_MIN_EXT 0x878B
#define GL_OP_SET_GE_EXT 0x878C
#define GL_OP_SET_LT_EXT 0x878D
#define GL_OP_CLAMP_EXT 0x878E
#define GL_OP_FLOOR_EXT 0x878F
#define GL_OP_ROUND_EXT 0x8790
#define GL_OP_EXP_BASE_2_EXT 0x8791
#define GL_OP_LOG_BASE_2_EXT 0x8792
#define GL_OP_POWER_EXT 0x8793
#define GL_OP_RECIP_EXT 0x8794
#define GL_OP_RECIP_SQRT_EXT 0x8795
#define GL_OP_SUB_EXT 0x8796
#define GL_OP_CROSS_PRODUCT_EXT 0x8797
#define GL_OP_MULTIPLY_MATRIX_EXT 0x8798
#define GL_OP_MOV_EXT 0x8799
#define GL_OUTPUT_VERTEX_EXT 0x879A
#define GL_OUTPUT_COLOR0_EXT 0x879B
#define GL_OUTPUT_COLOR1_EXT 0x879C
#define GL_OUTPUT_TEXTURE_COORD0_EXT 0x879D
#define GL_OUTPUT_TEXTURE_COORD1_EXT 0x879E
#define GL_OUTPUT_TEXTURE_COORD2_EXT 0x879F
#define GL_OUTPUT_TEXTURE_COORD3_EXT 0x87A0
#define GL_OUTPUT_TEXTURE_COORD4_EXT 0x87A1
#define GL_OUTPUT_TEXTURE_COORD5_EXT 0x87A2
#define GL_OUTPUT_TEXTURE_COORD6_EXT 0x87A3
#define GL_OUTPUT_TEXTURE_COORD7_EXT 0x87A4
#define GL_OUTPUT_TEXTURE_COORD8_EXT 0x87A5
#define GL_OUTPUT_TEXTURE_COORD9_EXT 0x87A6
#define GL_OUTPUT_TEXTURE_COORD10_EXT 0x87A7
#define GL_OUTPUT_TEXTURE_COORD11_EXT 0x87A8
#define GL_OUTPUT_TEXTURE_COORD12_EXT 0x87A9
#define GL_OUTPUT_TEXTURE_COORD13_EXT 0x87AA
#define GL_OUTPUT_TEXTURE_COORD14_EXT 0x87AB
#define GL_OUTPUT_TEXTURE_COORD15_EXT 0x87AC
#define GL_OUTPUT_TEXTURE_COORD16_EXT 0x87AD
#define GL_OUTPUT_TEXTURE_COORD17_EXT 0x87AE
#define GL_OUTPUT_TEXTURE_COORD18_EXT 0x87AF
#define GL_OUTPUT_TEXTURE_COORD19_EXT 0x87B0
#define GL_OUTPUT_TEXTURE_COORD20_EXT 0x87B1
#define GL_OUTPUT_TEXTURE_COORD21_EXT 0x87B2
#define GL_OUTPUT_TEXTURE_COORD22_EXT 0x87B3
#define GL_OUTPUT_TEXTURE_COORD23_EXT 0x87B4
#define GL_OUTPUT_TEXTURE_COORD24_EXT 0x87B5
#define GL_OUTPUT_TEXTURE_COORD25_EXT 0x87B6
#define GL_OUTPUT_TEXTURE_COORD26_EXT 0x87B7
#define GL_OUTPUT_TEXTURE_COORD27_EXT 0x87B8
#define GL_OUTPUT_TEXTURE_COORD28_EXT 0x87B9
#define GL_OUTPUT_TEXTURE_COORD29_EXT 0x87BA
#define GL_OUTPUT_TEXTURE_COORD30_EXT 0x87BB
#define GL_OUTPUT_TEXTURE_COORD31_EXT 0x87BC
#define GL_OUTPUT_FOG_EXT 0x87BD
#define GL_SCALAR_EXT 0x87BE
#define GL_VECTOR_EXT 0x87BF
#define GL_MATRIX_EXT 0x87C0
#define GL_VARIANT_EXT 0x87C1
#define GL_INVARIANT_EXT 0x87C2
#define GL_LOCAL_CONSTANT_EXT 0x87C3
#define GL_LOCAL_EXT 0x87C4
#define GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT 0x87C5
#define GL_MAX_VERTEX_SHADER_VARIANTS_EXT 0x87C6
#define GL_MAX_VERTEX_SHADER_INVARIANTS_EXT 0x87C7
#define GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT 0x87C8
#define GL_MAX_VERTEX_SHADER_LOCALS_EXT 0x87C9
#define GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT 0x87CA
#define GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT 0x87CB
#define GL_MAX_OPTIMIZED_VERTEX_SHADER_INVARIANTS_EXT 0x87CC
#define GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT 0x87CD
#define GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCALS_EXT 0x87CE
#define GL_VERTEX_SHADER_INSTRUCTIONS_EXT 0x87CF
#define GL_VERTEX_SHADER_VARIANTS_EXT 0x87D0
#define GL_VERTEX_SHADER_INVARIANTS_EXT 0x87D1
#define GL_VERTEX_SHADER_LOCAL_CONSTANTS_EXT 0x87D2
#define GL_VERTEX_SHADER_LOCALS_EXT 0x87D3
#define GL_VERTEX_SHADER_OPTIMIZED_EXT 0x87D4
#define GL_X_EXT 0x87D5
#define GL_Y_EXT 0x87D6
#define GL_Z_EXT 0x87D7
#define GL_W_EXT 0x87D8
#define GL_NEGATIVE_X_EXT 0x87D9
#define GL_NEGATIVE_Y_EXT 0x87DA
#define GL_NEGATIVE_Z_EXT 0x87DB
#define GL_NEGATIVE_W_EXT 0x87DC
#define GL_ZERO_EXT 0x87DD
#define GL_ONE_EXT 0x87DE
#define GL_NEGATIVE_ONE_EXT 0x87DF
#define GL_NORMALIZED_RANGE_EXT 0x87E0
#define GL_FULL_RANGE_EXT 0x87E1
#define GL_CURRENT_VERTEX_EXT 0x87E2
#define GL_MVP_MATRIX_EXT 0x87E3
#define GL_VARIANT_VALUE_EXT 0x87E4
#define GL_VARIANT_DATATYPE_EXT 0x87E5
#define GL_VARIANT_ARRAY_STRIDE_EXT 0x87E6
#define GL_VARIANT_ARRAY_TYPE_EXT 0x87E7
#define GL_VARIANT_ARRAY_EXT 0x87E8
#define GL_VARIANT_ARRAY_POINTER_EXT 0x87E9
#define GL_INVARIANT_VALUE_EXT 0x87EA
#define GL_INVARIANT_DATATYPE_EXT 0x87EB
#define GL_LOCAL_CONSTANT_VALUE_EXT 0x87EC
#define GL_LOCAL_CONSTANT_DATATYPE_EXT 0x87ED

typedef void (GLAPIENTRY * PFNGLBEGINVERTEXSHADEREXTPROC) (void);
typedef GLuint (GLAPIENTRY * PFNGLBINDLIGHTPARAMETEREXTPROC) (GLenum light, GLenum value);
typedef GLuint (GLAPIENTRY * PFNGLBINDMATERIALPARAMETEREXTPROC) (GLenum face, GLenum value);
typedef GLuint (GLAPIENTRY * PFNGLBINDPARAMETEREXTPROC) (GLenum value);
typedef GLuint (GLAPIENTRY * PFNGLBINDTEXGENPARAMETEREXTPROC) (GLenum unit, GLenum coord, GLenum value);
typedef GLuint (GLAPIENTRY * PFNGLBINDTEXTUREUNITPARAMETEREXTPROC) (GLenum unit, GLenum value);
typedef void (GLAPIENTRY * PFNGLBINDVERTEXSHADEREXTPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETEVERTEXSHADEREXTPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLENABLEVARIANTCLIENTSTATEEXTPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLENDVERTEXSHADEREXTPROC) (void);
typedef void (GLAPIENTRY * PFNGLEXTRACTCOMPONENTEXTPROC) (GLuint res, GLuint src, GLuint num);
typedef GLuint (GLAPIENTRY * PFNGLGENSYMBOLSEXTPROC) (GLenum dataType, GLenum storageType, GLenum range, GLuint components);
typedef GLuint (GLAPIENTRY * PFNGLGENVERTEXSHADERSEXTPROC) (GLuint range);
typedef void (GLAPIENTRY * PFNGLGETINVARIANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (GLAPIENTRY * PFNGLGETINVARIANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);
typedef void (GLAPIENTRY * PFNGLGETINVARIANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (GLAPIENTRY * PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (GLAPIENTRY * PFNGLGETLOCALCONSTANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);
typedef void (GLAPIENTRY * PFNGLGETLOCALCONSTANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (GLAPIENTRY * PFNGLGETVARIANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (GLAPIENTRY * PFNGLGETVARIANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);
typedef void (GLAPIENTRY * PFNGLGETVARIANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (GLAPIENTRY * PFNGLGETVARIANTPOINTERVEXTPROC) (GLuint id, GLenum value, GLvoid **data);
typedef void (GLAPIENTRY * PFNGLINSERTCOMPONENTEXTPROC) (GLuint res, GLuint src, GLuint num);
typedef GLboolean (GLAPIENTRY * PFNGLISVARIANTENABLEDEXTPROC) (GLuint id, GLenum cap);
typedef void (GLAPIENTRY * PFNGLSETINVARIANTEXTPROC) (GLuint id, GLenum type, GLvoid *addr);
typedef void (GLAPIENTRY * PFNGLSETLOCALCONSTANTEXTPROC) (GLuint id, GLenum type, GLvoid *addr);
typedef void (GLAPIENTRY * PFNGLSHADEROP1EXTPROC) (GLenum op, GLuint res, GLuint arg1);
typedef void (GLAPIENTRY * PFNGLSHADEROP2EXTPROC) (GLenum op, GLuint res, GLuint arg1, GLuint arg2);
typedef void (GLAPIENTRY * PFNGLSHADEROP3EXTPROC) (GLenum op, GLuint res, GLuint arg1, GLuint arg2, GLuint arg3);
typedef void (GLAPIENTRY * PFNGLSWIZZLEEXTPROC) (GLuint res, GLuint in, GLenum outX, GLenum outY, GLenum outZ, GLenum outW);
typedef void (GLAPIENTRY * PFNGLVARIANTPOINTEREXTPROC) (GLuint id, GLenum type, GLuint stride, GLvoid *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTBVEXTPROC) (GLuint id, GLbyte *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTDVEXTPROC) (GLuint id, GLdouble *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTFVEXTPROC) (GLuint id, GLfloat *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTIVEXTPROC) (GLuint id, GLint *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTSVEXTPROC) (GLuint id, GLshort *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTUBVEXTPROC) (GLuint id, GLubyte *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTUIVEXTPROC) (GLuint id, GLuint *addr);
typedef void (GLAPIENTRY * PFNGLVARIANTUSVEXTPROC) (GLuint id, GLushort *addr);
typedef void (GLAPIENTRY * PFNGLWRITEMASKEXTPROC) (GLuint res, GLuint in, GLenum outX, GLenum outY, GLenum outZ, GLenum outW);

#define glBeginVertexShaderEXT GLEW_GET_FUN(__glewBeginVertexShaderEXT)
#define glBindLightParameterEXT GLEW_GET_FUN(__glewBindLightParameterEXT)
#define glBindMaterialParameterEXT GLEW_GET_FUN(__glewBindMaterialParameterEXT)
#define glBindParameterEXT GLEW_GET_FUN(__glewBindParameterEXT)
#define glBindTexGenParameterEXT GLEW_GET_FUN(__glewBindTexGenParameterEXT)
#define glBindTextureUnitParameterEXT GLEW_GET_FUN(__glewBindTextureUnitParameterEXT)
#define glBindVertexShaderEXT GLEW_GET_FUN(__glewBindVertexShaderEXT)
#define glDeleteVertexShaderEXT GLEW_GET_FUN(__glewDeleteVertexShaderEXT)
#define glDisableVariantClientStateEXT GLEW_GET_FUN(__glewDisableVariantClientStateEXT)
#define glEnableVariantClientStateEXT GLEW_GET_FUN(__glewEnableVariantClientStateEXT)
#define glEndVertexShaderEXT GLEW_GET_FUN(__glewEndVertexShaderEXT)
#define glExtractComponentEXT GLEW_GET_FUN(__glewExtractComponentEXT)
#define glGenSymbolsEXT GLEW_GET_FUN(__glewGenSymbolsEXT)
#define glGenVertexShadersEXT GLEW_GET_FUN(__glewGenVertexShadersEXT)
#define glGetInvariantBooleanvEXT GLEW_GET_FUN(__glewGetInvariantBooleanvEXT)
#define glGetInvariantFloatvEXT GLEW_GET_FUN(__glewGetInvariantFloatvEXT)
#define glGetInvariantIntegervEXT GLEW_GET_FUN(__glewGetInvariantIntegervEXT)
#define glGetLocalConstantBooleanvEXT GLEW_GET_FUN(__glewGetLocalConstantBooleanvEXT)
#define glGetLocalConstantFloatvEXT GLEW_GET_FUN(__glewGetLocalConstantFloatvEXT)
#define glGetLocalConstantIntegervEXT GLEW_GET_FUN(__glewGetLocalConstantIntegervEXT)
#define glGetVariantBooleanvEXT GLEW_GET_FUN(__glewGetVariantBooleanvEXT)
#define glGetVariantFloatvEXT GLEW_GET_FUN(__glewGetVariantFloatvEXT)
#define glGetVariantIntegervEXT GLEW_GET_FUN(__glewGetVariantIntegervEXT)
#define glGetVariantPointervEXT GLEW_GET_FUN(__glewGetVariantPointervEXT)
#define glInsertComponentEXT GLEW_GET_FUN(__glewInsertComponentEXT)
#define glIsVariantEnabledEXT GLEW_GET_FUN(__glewIsVariantEnabledEXT)
#define glSetInvariantEXT GLEW_GET_FUN(__glewSetInvariantEXT)
#define glSetLocalConstantEXT GLEW_GET_FUN(__glewSetLocalConstantEXT)
#define glShaderOp1EXT GLEW_GET_FUN(__glewShaderOp1EXT)
#define glShaderOp2EXT GLEW_GET_FUN(__glewShaderOp2EXT)
#define glShaderOp3EXT GLEW_GET_FUN(__glewShaderOp3EXT)
#define glSwizzleEXT GLEW_GET_FUN(__glewSwizzleEXT)
#define glVariantPointerEXT GLEW_GET_FUN(__glewVariantPointerEXT)
#define glVariantbvEXT GLEW_GET_FUN(__glewVariantbvEXT)
#define glVariantdvEXT GLEW_GET_FUN(__glewVariantdvEXT)
#define glVariantfvEXT GLEW_GET_FUN(__glewVariantfvEXT)
#define glVariantivEXT GLEW_GET_FUN(__glewVariantivEXT)
#define glVariantsvEXT GLEW_GET_FUN(__glewVariantsvEXT)
#define glVariantubvEXT GLEW_GET_FUN(__glewVariantubvEXT)
#define glVariantuivEXT GLEW_GET_FUN(__glewVariantuivEXT)
#define glVariantusvEXT GLEW_GET_FUN(__glewVariantusvEXT)
#define glWriteMaskEXT GLEW_GET_FUN(__glewWriteMaskEXT)

#define GLEW_EXT_vertex_shader GLEW_GET_VAR(__GLEW_EXT_vertex_shader)

#endif /* GL_EXT_vertex_shader */

/* ------------------------ GL_EXT_vertex_weighting ------------------------ */

#ifndef GL_EXT_vertex_weighting
#define GL_EXT_vertex_weighting 1

#define GL_MODELVIEW0_STACK_DEPTH_EXT 0x0BA3
#define GL_MODELVIEW0_MATRIX_EXT 0x0BA6
#define GL_MODELVIEW0_EXT 0x1700
#define GL_MODELVIEW1_STACK_DEPTH_EXT 0x8502
#define GL_MODELVIEW1_MATRIX_EXT 0x8506
#define GL_VERTEX_WEIGHTING_EXT 0x8509
#define GL_MODELVIEW1_EXT 0x850A
#define GL_CURRENT_VERTEX_WEIGHT_EXT 0x850B
#define GL_VERTEX_WEIGHT_ARRAY_EXT 0x850C
#define GL_VERTEX_WEIGHT_ARRAY_SIZE_EXT 0x850D
#define GL_VERTEX_WEIGHT_ARRAY_TYPE_EXT 0x850E
#define GL_VERTEX_WEIGHT_ARRAY_STRIDE_EXT 0x850F
#define GL_VERTEX_WEIGHT_ARRAY_POINTER_EXT 0x8510

typedef void (GLAPIENTRY * PFNGLVERTEXWEIGHTPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, void* pointer);
typedef void (GLAPIENTRY * PFNGLVERTEXWEIGHTFEXTPROC) (GLfloat weight);
typedef void (GLAPIENTRY * PFNGLVERTEXWEIGHTFVEXTPROC) (GLfloat* weight);

#define glVertexWeightPointerEXT GLEW_GET_FUN(__glewVertexWeightPointerEXT)
#define glVertexWeightfEXT GLEW_GET_FUN(__glewVertexWeightfEXT)
#define glVertexWeightfvEXT GLEW_GET_FUN(__glewVertexWeightfvEXT)

#define GLEW_EXT_vertex_weighting GLEW_GET_VAR(__GLEW_EXT_vertex_weighting)

#endif /* GL_EXT_vertex_weighting */

/* ---------------------- GL_GREMEDY_frame_terminator ---------------------- */

#ifndef GL_GREMEDY_frame_terminator
#define GL_GREMEDY_frame_terminator 1

typedef void (GLAPIENTRY * PFNGLFRAMETERMINATORGREMEDYPROC) (void);

#define glFrameTerminatorGREMEDY GLEW_GET_FUN(__glewFrameTerminatorGREMEDY)

#define GLEW_GREMEDY_frame_terminator GLEW_GET_VAR(__GLEW_GREMEDY_frame_terminator)

#endif /* GL_GREMEDY_frame_terminator */

/* ------------------------ GL_GREMEDY_string_marker ----------------------- */

#ifndef GL_GREMEDY_string_marker
#define GL_GREMEDY_string_marker 1

typedef void (GLAPIENTRY * PFNGLSTRINGMARKERGREMEDYPROC) (GLsizei len, const void* string);

#define glStringMarkerGREMEDY GLEW_GET_FUN(__glewStringMarkerGREMEDY)

#define GLEW_GREMEDY_string_marker GLEW_GET_VAR(__GLEW_GREMEDY_string_marker)

#endif /* GL_GREMEDY_string_marker */

/* --------------------- GL_HP_convolution_border_modes -------------------- */

#ifndef GL_HP_convolution_border_modes
#define GL_HP_convolution_border_modes 1

#define GLEW_HP_convolution_border_modes GLEW_GET_VAR(__GLEW_HP_convolution_border_modes)

#endif /* GL_HP_convolution_border_modes */

/* ------------------------- GL_HP_image_transform ------------------------- */

#ifndef GL_HP_image_transform
#define GL_HP_image_transform 1

typedef void (GLAPIENTRY * PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLIMAGETRANSFORMPARAMETERFHPPROC) (GLenum target, GLenum pname, const GLfloat param);
typedef void (GLAPIENTRY * PFNGLIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLIMAGETRANSFORMPARAMETERIHPPROC) (GLenum target, GLenum pname, const GLint param);
typedef void (GLAPIENTRY * PFNGLIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, const GLint* params);

#define glGetImageTransformParameterfvHP GLEW_GET_FUN(__glewGetImageTransformParameterfvHP)
#define glGetImageTransformParameterivHP GLEW_GET_FUN(__glewGetImageTransformParameterivHP)
#define glImageTransformParameterfHP GLEW_GET_FUN(__glewImageTransformParameterfHP)
#define glImageTransformParameterfvHP GLEW_GET_FUN(__glewImageTransformParameterfvHP)
#define glImageTransformParameteriHP GLEW_GET_FUN(__glewImageTransformParameteriHP)
#define glImageTransformParameterivHP GLEW_GET_FUN(__glewImageTransformParameterivHP)

#define GLEW_HP_image_transform GLEW_GET_VAR(__GLEW_HP_image_transform)

#endif /* GL_HP_image_transform */

/* -------------------------- GL_HP_occlusion_test ------------------------- */

#ifndef GL_HP_occlusion_test
#define GL_HP_occlusion_test 1

#define GL_OCCLUSION_TEST_HP 0x8165
#define GL_OCCLUSION_TEST_RESULT_HP 0x8166

#define GLEW_HP_occlusion_test GLEW_GET_VAR(__GLEW_HP_occlusion_test)

#endif /* GL_HP_occlusion_test */

/* ------------------------- GL_HP_texture_lighting ------------------------ */

#ifndef GL_HP_texture_lighting
#define GL_HP_texture_lighting 1

#define GLEW_HP_texture_lighting GLEW_GET_VAR(__GLEW_HP_texture_lighting)

#endif /* GL_HP_texture_lighting */

/* --------------------------- GL_IBM_cull_vertex -------------------------- */

#ifndef GL_IBM_cull_vertex
#define GL_IBM_cull_vertex 1

#define GL_CULL_VERTEX_IBM 103050

#define GLEW_IBM_cull_vertex GLEW_GET_VAR(__GLEW_IBM_cull_vertex)

#endif /* GL_IBM_cull_vertex */

/* ---------------------- GL_IBM_multimode_draw_arrays --------------------- */

#ifndef GL_IBM_multimode_draw_arrays
#define GL_IBM_multimode_draw_arrays 1

typedef void (GLAPIENTRY * PFNGLMULTIMODEDRAWARRAYSIBMPROC) (const GLenum* mode, const GLint *first, const GLsizei *count, GLsizei primcount, GLint modestride);
typedef void (GLAPIENTRY * PFNGLMULTIMODEDRAWELEMENTSIBMPROC) (const GLenum* mode, const GLsizei *count, GLenum type, const GLvoid * const *indices, GLsizei primcount, GLint modestride);

#define glMultiModeDrawArraysIBM GLEW_GET_FUN(__glewMultiModeDrawArraysIBM)
#define glMultiModeDrawElementsIBM GLEW_GET_FUN(__glewMultiModeDrawElementsIBM)

#define GLEW_IBM_multimode_draw_arrays GLEW_GET_VAR(__GLEW_IBM_multimode_draw_arrays)

#endif /* GL_IBM_multimode_draw_arrays */

/* ------------------------- GL_IBM_rasterpos_clip ------------------------- */

#ifndef GL_IBM_rasterpos_clip
#define GL_IBM_rasterpos_clip 1

#define GL_RASTER_POSITION_UNCLIPPED_IBM 103010

#define GLEW_IBM_rasterpos_clip GLEW_GET_VAR(__GLEW_IBM_rasterpos_clip)

#endif /* GL_IBM_rasterpos_clip */

/* --------------------------- GL_IBM_static_data -------------------------- */

#ifndef GL_IBM_static_data
#define GL_IBM_static_data 1

#define GL_ALL_STATIC_DATA_IBM 103060
#define GL_STATIC_VERTEX_ARRAY_IBM 103061

#define GLEW_IBM_static_data GLEW_GET_VAR(__GLEW_IBM_static_data)

#endif /* GL_IBM_static_data */

/* --------------------- GL_IBM_texture_mirrored_repeat -------------------- */

#ifndef GL_IBM_texture_mirrored_repeat
#define GL_IBM_texture_mirrored_repeat 1

#define GL_MIRRORED_REPEAT_IBM 0x8370

#define GLEW_IBM_texture_mirrored_repeat GLEW_GET_VAR(__GLEW_IBM_texture_mirrored_repeat)

#endif /* GL_IBM_texture_mirrored_repeat */

/* ----------------------- GL_IBM_vertex_array_lists ----------------------- */

#ifndef GL_IBM_vertex_array_lists
#define GL_IBM_vertex_array_lists 1

#define GL_VERTEX_ARRAY_LIST_IBM 103070
#define GL_NORMAL_ARRAY_LIST_IBM 103071
#define GL_COLOR_ARRAY_LIST_IBM 103072
#define GL_INDEX_ARRAY_LIST_IBM 103073
#define GL_TEXTURE_COORD_ARRAY_LIST_IBM 103074
#define GL_EDGE_FLAG_ARRAY_LIST_IBM 103075
#define GL_FOG_COORDINATE_ARRAY_LIST_IBM 103076
#define GL_SECONDARY_COLOR_ARRAY_LIST_IBM 103077
#define GL_VERTEX_ARRAY_LIST_STRIDE_IBM 103080
#define GL_NORMAL_ARRAY_LIST_STRIDE_IBM 103081
#define GL_COLOR_ARRAY_LIST_STRIDE_IBM 103082
#define GL_INDEX_ARRAY_LIST_STRIDE_IBM 103083
#define GL_TEXTURE_COORD_ARRAY_LIST_STRIDE_IBM 103084
#define GL_EDGE_FLAG_ARRAY_LIST_STRIDE_IBM 103085
#define GL_FOG_COORDINATE_ARRAY_LIST_STRIDE_IBM 103086
#define GL_SECONDARY_COLOR_ARRAY_LIST_STRIDE_IBM 103087

typedef void (GLAPIENTRY * PFNGLCOLORPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLEDGEFLAGPOINTERLISTIBMPROC) (GLint stride, const GLboolean ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLFOGCOORDPOINTERLISTIBMPROC) (GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLINDEXPOINTERLISTIBMPROC) (GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLNORMALPOINTERLISTIBMPROC) (GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLTEXCOORDPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);
typedef void (GLAPIENTRY * PFNGLVERTEXPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, const GLvoid ** pointer, GLint ptrstride);

#define glColorPointerListIBM GLEW_GET_FUN(__glewColorPointerListIBM)
#define glEdgeFlagPointerListIBM GLEW_GET_FUN(__glewEdgeFlagPointerListIBM)
#define glFogCoordPointerListIBM GLEW_GET_FUN(__glewFogCoordPointerListIBM)
#define glIndexPointerListIBM GLEW_GET_FUN(__glewIndexPointerListIBM)
#define glNormalPointerListIBM GLEW_GET_FUN(__glewNormalPointerListIBM)
#define glSecondaryColorPointerListIBM GLEW_GET_FUN(__glewSecondaryColorPointerListIBM)
#define glTexCoordPointerListIBM GLEW_GET_FUN(__glewTexCoordPointerListIBM)
#define glVertexPointerListIBM GLEW_GET_FUN(__glewVertexPointerListIBM)

#define GLEW_IBM_vertex_array_lists GLEW_GET_VAR(__GLEW_IBM_vertex_array_lists)

#endif /* GL_IBM_vertex_array_lists */

/* -------------------------- GL_INGR_color_clamp -------------------------- */

#ifndef GL_INGR_color_clamp
#define GL_INGR_color_clamp 1

#define GL_RED_MIN_CLAMP_INGR 0x8560
#define GL_GREEN_MIN_CLAMP_INGR 0x8561
#define GL_BLUE_MIN_CLAMP_INGR 0x8562
#define GL_ALPHA_MIN_CLAMP_INGR 0x8563
#define GL_RED_MAX_CLAMP_INGR 0x8564
#define GL_GREEN_MAX_CLAMP_INGR 0x8565
#define GL_BLUE_MAX_CLAMP_INGR 0x8566
#define GL_ALPHA_MAX_CLAMP_INGR 0x8567

#define GLEW_INGR_color_clamp GLEW_GET_VAR(__GLEW_INGR_color_clamp)

#endif /* GL_INGR_color_clamp */

/* ------------------------- GL_INGR_interlace_read ------------------------ */

#ifndef GL_INGR_interlace_read
#define GL_INGR_interlace_read 1

#define GL_INTERLACE_READ_INGR 0x8568

#define GLEW_INGR_interlace_read GLEW_GET_VAR(__GLEW_INGR_interlace_read)

#endif /* GL_INGR_interlace_read */

/* ------------------------ GL_INTEL_parallel_arrays ----------------------- */

#ifndef GL_INTEL_parallel_arrays
#define GL_INTEL_parallel_arrays 1

#define GL_PARALLEL_ARRAYS_INTEL 0x83F4
#define GL_VERTEX_ARRAY_PARALLEL_POINTERS_INTEL 0x83F5
#define GL_NORMAL_ARRAY_PARALLEL_POINTERS_INTEL 0x83F6
#define GL_COLOR_ARRAY_PARALLEL_POINTERS_INTEL 0x83F7
#define GL_TEXTURE_COORD_ARRAY_PARALLEL_POINTERS_INTEL 0x83F8

typedef void (GLAPIENTRY * PFNGLCOLORPOINTERVINTELPROC) (GLint size, GLenum type, const void** pointer);
typedef void (GLAPIENTRY * PFNGLNORMALPOINTERVINTELPROC) (GLenum type, const void** pointer);
typedef void (GLAPIENTRY * PFNGLTEXCOORDPOINTERVINTELPROC) (GLint size, GLenum type, const void** pointer);
typedef void (GLAPIENTRY * PFNGLVERTEXPOINTERVINTELPROC) (GLint size, GLenum type, const void** pointer);

#define glColorPointervINTEL GLEW_GET_FUN(__glewColorPointervINTEL)
#define glNormalPointervINTEL GLEW_GET_FUN(__glewNormalPointervINTEL)
#define glTexCoordPointervINTEL GLEW_GET_FUN(__glewTexCoordPointervINTEL)
#define glVertexPointervINTEL GLEW_GET_FUN(__glewVertexPointervINTEL)

#define GLEW_INTEL_parallel_arrays GLEW_GET_VAR(__GLEW_INTEL_parallel_arrays)

#endif /* GL_INTEL_parallel_arrays */

/* ------------------------ GL_INTEL_texture_scissor ----------------------- */

#ifndef GL_INTEL_texture_scissor
#define GL_INTEL_texture_scissor 1

typedef void (GLAPIENTRY * PFNGLTEXSCISSORFUNCINTELPROC) (GLenum target, GLenum lfunc, GLenum hfunc);
typedef void (GLAPIENTRY * PFNGLTEXSCISSORINTELPROC) (GLenum target, GLclampf tlow, GLclampf thigh);

#define glTexScissorFuncINTEL GLEW_GET_FUN(__glewTexScissorFuncINTEL)
#define glTexScissorINTEL GLEW_GET_FUN(__glewTexScissorINTEL)

#define GLEW_INTEL_texture_scissor GLEW_GET_VAR(__GLEW_INTEL_texture_scissor)

#endif /* GL_INTEL_texture_scissor */

/* -------------------------- GL_KTX_buffer_region ------------------------- */

#ifndef GL_KTX_buffer_region
#define GL_KTX_buffer_region 1

#define GL_KTX_FRONT_REGION 0x0
#define GL_KTX_BACK_REGION 0x1
#define GL_KTX_Z_REGION 0x2
#define GL_KTX_STENCIL_REGION 0x3

typedef GLuint (GLAPIENTRY * PFNGLBUFFERREGIONENABLEDEXTPROC) (void);
typedef void (GLAPIENTRY * PFNGLDELETEBUFFERREGIONEXTPROC) (GLenum region);
typedef void (GLAPIENTRY * PFNGLDRAWBUFFERREGIONEXTPROC) (GLuint region, GLint x, GLint y, GLsizei width, GLsizei height, GLint xDest, GLint yDest);
typedef GLuint (GLAPIENTRY * PFNGLNEWBUFFERREGIONEXTPROC) (GLenum region);
typedef void (GLAPIENTRY * PFNGLREADBUFFERREGIONEXTPROC) (GLuint region, GLint x, GLint y, GLsizei width, GLsizei height);

#define glBufferRegionEnabledEXT GLEW_GET_FUN(__glewBufferRegionEnabledEXT)
#define glDeleteBufferRegionEXT GLEW_GET_FUN(__glewDeleteBufferRegionEXT)
#define glDrawBufferRegionEXT GLEW_GET_FUN(__glewDrawBufferRegionEXT)
#define glNewBufferRegionEXT GLEW_GET_FUN(__glewNewBufferRegionEXT)
#define glReadBufferRegionEXT GLEW_GET_FUN(__glewReadBufferRegionEXT)

#define GLEW_KTX_buffer_region GLEW_GET_VAR(__GLEW_KTX_buffer_region)

#endif /* GL_KTX_buffer_region */

/* ------------------------- GL_MESAX_texture_stack ------------------------ */

#ifndef GL_MESAX_texture_stack
#define GL_MESAX_texture_stack 1

#define GL_TEXTURE_1D_STACK_MESAX 0x8759
#define GL_TEXTURE_2D_STACK_MESAX 0x875A
#define GL_PROXY_TEXTURE_1D_STACK_MESAX 0x875B
#define GL_PROXY_TEXTURE_2D_STACK_MESAX 0x875C
#define GL_TEXTURE_1D_STACK_BINDING_MESAX 0x875D
#define GL_TEXTURE_2D_STACK_BINDING_MESAX 0x875E

#define GLEW_MESAX_texture_stack GLEW_GET_VAR(__GLEW_MESAX_texture_stack)

#endif /* GL_MESAX_texture_stack */

/* -------------------------- GL_MESA_pack_invert -------------------------- */

#ifndef GL_MESA_pack_invert
#define GL_MESA_pack_invert 1

#define GL_PACK_INVERT_MESA 0x8758

#define GLEW_MESA_pack_invert GLEW_GET_VAR(__GLEW_MESA_pack_invert)

#endif /* GL_MESA_pack_invert */

/* ------------------------- GL_MESA_resize_buffers ------------------------ */

#ifndef GL_MESA_resize_buffers
#define GL_MESA_resize_buffers 1

typedef void (GLAPIENTRY * PFNGLRESIZEBUFFERSMESAPROC) (void);

#define glResizeBuffersMESA GLEW_GET_FUN(__glewResizeBuffersMESA)

#define GLEW_MESA_resize_buffers GLEW_GET_VAR(__GLEW_MESA_resize_buffers)

#endif /* GL_MESA_resize_buffers */

/* --------------------------- GL_MESA_window_pos -------------------------- */

#ifndef GL_MESA_window_pos
#define GL_MESA_window_pos 1

typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DMESAPROC) (GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2DVMESAPROC) (const GLdouble* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FMESAPROC) (GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2FVMESAPROC) (const GLfloat* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IMESAPROC) (GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2IVMESAPROC) (const GLint* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SMESAPROC) (GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS2SVMESAPROC) (const GLshort* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DMESAPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3DVMESAPROC) (const GLdouble* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FMESAPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3FVMESAPROC) (const GLfloat* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IMESAPROC) (GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3IVMESAPROC) (const GLint* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SMESAPROC) (GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS3SVMESAPROC) (const GLshort* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4DMESAPROC) (GLdouble x, GLdouble y, GLdouble z, GLdouble);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4DVMESAPROC) (const GLdouble* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4FMESAPROC) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4FVMESAPROC) (const GLfloat* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4IMESAPROC) (GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4IVMESAPROC) (const GLint* p);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4SMESAPROC) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (GLAPIENTRY * PFNGLWINDOWPOS4SVMESAPROC) (const GLshort* p);

#define glWindowPos2dMESA GLEW_GET_FUN(__glewWindowPos2dMESA)
#define glWindowPos2dvMESA GLEW_GET_FUN(__glewWindowPos2dvMESA)
#define glWindowPos2fMESA GLEW_GET_FUN(__glewWindowPos2fMESA)
#define glWindowPos2fvMESA GLEW_GET_FUN(__glewWindowPos2fvMESA)
#define glWindowPos2iMESA GLEW_GET_FUN(__glewWindowPos2iMESA)
#define glWindowPos2ivMESA GLEW_GET_FUN(__glewWindowPos2ivMESA)
#define glWindowPos2sMESA GLEW_GET_FUN(__glewWindowPos2sMESA)
#define glWindowPos2svMESA GLEW_GET_FUN(__glewWindowPos2svMESA)
#define glWindowPos3dMESA GLEW_GET_FUN(__glewWindowPos3dMESA)
#define glWindowPos3dvMESA GLEW_GET_FUN(__glewWindowPos3dvMESA)
#define glWindowPos3fMESA GLEW_GET_FUN(__glewWindowPos3fMESA)
#define glWindowPos3fvMESA GLEW_GET_FUN(__glewWindowPos3fvMESA)
#define glWindowPos3iMESA GLEW_GET_FUN(__glewWindowPos3iMESA)
#define glWindowPos3ivMESA GLEW_GET_FUN(__glewWindowPos3ivMESA)
#define glWindowPos3sMESA GLEW_GET_FUN(__glewWindowPos3sMESA)
#define glWindowPos3svMESA GLEW_GET_FUN(__glewWindowPos3svMESA)
#define glWindowPos4dMESA GLEW_GET_FUN(__glewWindowPos4dMESA)
#define glWindowPos4dvMESA GLEW_GET_FUN(__glewWindowPos4dvMESA)
#define glWindowPos4fMESA GLEW_GET_FUN(__glewWindowPos4fMESA)
#define glWindowPos4fvMESA GLEW_GET_FUN(__glewWindowPos4fvMESA)
#define glWindowPos4iMESA GLEW_GET_FUN(__glewWindowPos4iMESA)
#define glWindowPos4ivMESA GLEW_GET_FUN(__glewWindowPos4ivMESA)
#define glWindowPos4sMESA GLEW_GET_FUN(__glewWindowPos4sMESA)
#define glWindowPos4svMESA GLEW_GET_FUN(__glewWindowPos4svMESA)

#define GLEW_MESA_window_pos GLEW_GET_VAR(__GLEW_MESA_window_pos)

#endif /* GL_MESA_window_pos */

/* ------------------------- GL_MESA_ycbcr_texture ------------------------- */

#ifndef GL_MESA_ycbcr_texture
#define GL_MESA_ycbcr_texture 1

#define GL_UNSIGNED_SHORT_8_8_MESA 0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_MESA 0x85BB
#define GL_YCBCR_MESA 0x8757

#define GLEW_MESA_ycbcr_texture GLEW_GET_VAR(__GLEW_MESA_ycbcr_texture)

#endif /* GL_MESA_ycbcr_texture */

/* --------------------------- GL_NV_blend_square -------------------------- */

#ifndef GL_NV_blend_square
#define GL_NV_blend_square 1

#define GLEW_NV_blend_square GLEW_GET_VAR(__GLEW_NV_blend_square)

#endif /* GL_NV_blend_square */

/* ------------------------ GL_NV_conditional_render ----------------------- */

#ifndef GL_NV_conditional_render
#define GL_NV_conditional_render 1

#define GL_QUERY_WAIT_NV 0x8E13
#define GL_QUERY_NO_WAIT_NV 0x8E14
#define GL_QUERY_BY_REGION_WAIT_NV 0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT_NV 0x8E16

typedef void (GLAPIENTRY * PFNGLBEGINCONDITIONALRENDERNVPROC) (GLuint id, GLenum mode);
typedef void (GLAPIENTRY * PFNGLENDCONDITIONALRENDERNVPROC) (void);

#define glBeginConditionalRenderNV GLEW_GET_FUN(__glewBeginConditionalRenderNV)
#define glEndConditionalRenderNV GLEW_GET_FUN(__glewEndConditionalRenderNV)

#define GLEW_NV_conditional_render GLEW_GET_VAR(__GLEW_NV_conditional_render)

#endif /* GL_NV_conditional_render */

/* ----------------------- GL_NV_copy_depth_to_color ----------------------- */

#ifndef GL_NV_copy_depth_to_color
#define GL_NV_copy_depth_to_color 1

#define GL_DEPTH_STENCIL_TO_RGBA_NV 0x886E
#define GL_DEPTH_STENCIL_TO_BGRA_NV 0x886F

#define GLEW_NV_copy_depth_to_color GLEW_GET_VAR(__GLEW_NV_copy_depth_to_color)

#endif /* GL_NV_copy_depth_to_color */

/* ---------------------------- GL_NV_copy_image --------------------------- */

#ifndef GL_NV_copy_image
#define GL_NV_copy_image 1

typedef void (GLAPIENTRY * PFNGLCOPYIMAGESUBDATANVPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);

#define glCopyImageSubDataNV GLEW_GET_FUN(__glewCopyImageSubDataNV)

#define GLEW_NV_copy_image GLEW_GET_VAR(__GLEW_NV_copy_image)

#endif /* GL_NV_copy_image */

/* ------------------------ GL_NV_depth_buffer_float ----------------------- */

#ifndef GL_NV_depth_buffer_float
#define GL_NV_depth_buffer_float 1

#define GL_DEPTH_COMPONENT32F_NV 0x8DAB
#define GL_DEPTH32F_STENCIL8_NV 0x8DAC
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV 0x8DAD
#define GL_DEPTH_BUFFER_FLOAT_MODE_NV 0x8DAF

typedef void (GLAPIENTRY * PFNGLCLEARDEPTHDNVPROC) (GLdouble depth);
typedef void (GLAPIENTRY * PFNGLDEPTHBOUNDSDNVPROC) (GLdouble zmin, GLdouble zmax);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEDNVPROC) (GLdouble zNear, GLdouble zFar);

#define glClearDepthdNV GLEW_GET_FUN(__glewClearDepthdNV)
#define glDepthBoundsdNV GLEW_GET_FUN(__glewDepthBoundsdNV)
#define glDepthRangedNV GLEW_GET_FUN(__glewDepthRangedNV)

#define GLEW_NV_depth_buffer_float GLEW_GET_VAR(__GLEW_NV_depth_buffer_float)

#endif /* GL_NV_depth_buffer_float */

/* --------------------------- GL_NV_depth_clamp --------------------------- */

#ifndef GL_NV_depth_clamp
#define GL_NV_depth_clamp 1

#define GL_DEPTH_CLAMP_NV 0x864F

#define GLEW_NV_depth_clamp GLEW_GET_VAR(__GLEW_NV_depth_clamp)

#endif /* GL_NV_depth_clamp */

/* ---------------------- GL_NV_depth_range_unclamped ---------------------- */

#ifndef GL_NV_depth_range_unclamped
#define GL_NV_depth_range_unclamped 1

#define GL_SAMPLE_COUNT_BITS_NV 0x8864
#define GL_CURRENT_SAMPLE_COUNT_QUERY_NV 0x8865
#define GL_QUERY_RESULT_NV 0x8866
#define GL_QUERY_RESULT_AVAILABLE_NV 0x8867
#define GL_SAMPLE_COUNT_NV 0x8914

#define GLEW_NV_depth_range_unclamped GLEW_GET_VAR(__GLEW_NV_depth_range_unclamped)

#endif /* GL_NV_depth_range_unclamped */

/* ---------------------------- GL_NV_evaluators --------------------------- */

#ifndef GL_NV_evaluators
#define GL_NV_evaluators 1

#define GL_EVAL_2D_NV 0x86C0
#define GL_EVAL_TRIANGULAR_2D_NV 0x86C1
#define GL_MAP_TESSELLATION_NV 0x86C2
#define GL_MAP_ATTRIB_U_ORDER_NV 0x86C3
#define GL_MAP_ATTRIB_V_ORDER_NV 0x86C4
#define GL_EVAL_FRACTIONAL_TESSELLATION_NV 0x86C5
#define GL_EVAL_VERTEX_ATTRIB0_NV 0x86C6
#define GL_EVAL_VERTEX_ATTRIB1_NV 0x86C7
#define GL_EVAL_VERTEX_ATTRIB2_NV 0x86C8
#define GL_EVAL_VERTEX_ATTRIB3_NV 0x86C9
#define GL_EVAL_VERTEX_ATTRIB4_NV 0x86CA
#define GL_EVAL_VERTEX_ATTRIB5_NV 0x86CB
#define GL_EVAL_VERTEX_ATTRIB6_NV 0x86CC
#define GL_EVAL_VERTEX_ATTRIB7_NV 0x86CD
#define GL_EVAL_VERTEX_ATTRIB8_NV 0x86CE
#define GL_EVAL_VERTEX_ATTRIB9_NV 0x86CF
#define GL_EVAL_VERTEX_ATTRIB10_NV 0x86D0
#define GL_EVAL_VERTEX_ATTRIB11_NV 0x86D1
#define GL_EVAL_VERTEX_ATTRIB12_NV 0x86D2
#define GL_EVAL_VERTEX_ATTRIB13_NV 0x86D3
#define GL_EVAL_VERTEX_ATTRIB14_NV 0x86D4
#define GL_EVAL_VERTEX_ATTRIB15_NV 0x86D5
#define GL_MAX_MAP_TESSELLATION_NV 0x86D6
#define GL_MAX_RATIONAL_EVAL_ORDER_NV 0x86D7

typedef void (GLAPIENTRY * PFNGLEVALMAPSNVPROC) (GLenum target, GLenum mode);
typedef void (GLAPIENTRY * PFNGLGETMAPATTRIBPARAMETERFVNVPROC) (GLenum target, GLuint index, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMAPATTRIBPARAMETERIVNVPROC) (GLenum target, GLuint index, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETMAPCONTROLPOINTSNVPROC) (GLenum target, GLuint index, GLenum type, GLsizei ustride, GLsizei vstride, GLboolean packed, void* points);
typedef void (GLAPIENTRY * PFNGLGETMAPPARAMETERFVNVPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETMAPPARAMETERIVNVPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLMAPCONTROLPOINTSNVPROC) (GLenum target, GLuint index, GLenum type, GLsizei ustride, GLsizei vstride, GLint uorder, GLint vorder, GLboolean packed, const void* points);
typedef void (GLAPIENTRY * PFNGLMAPPARAMETERFVNVPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLMAPPARAMETERIVNVPROC) (GLenum target, GLenum pname, const GLint* params);

#define glEvalMapsNV GLEW_GET_FUN(__glewEvalMapsNV)
#define glGetMapAttribParameterfvNV GLEW_GET_FUN(__glewGetMapAttribParameterfvNV)
#define glGetMapAttribParameterivNV GLEW_GET_FUN(__glewGetMapAttribParameterivNV)
#define glGetMapControlPointsNV GLEW_GET_FUN(__glewGetMapControlPointsNV)
#define glGetMapParameterfvNV GLEW_GET_FUN(__glewGetMapParameterfvNV)
#define glGetMapParameterivNV GLEW_GET_FUN(__glewGetMapParameterivNV)
#define glMapControlPointsNV GLEW_GET_FUN(__glewMapControlPointsNV)
#define glMapParameterfvNV GLEW_GET_FUN(__glewMapParameterfvNV)
#define glMapParameterivNV GLEW_GET_FUN(__glewMapParameterivNV)

#define GLEW_NV_evaluators GLEW_GET_VAR(__GLEW_NV_evaluators)

#endif /* GL_NV_evaluators */

/* ----------------------- GL_NV_explicit_multisample ---------------------- */

#ifndef GL_NV_explicit_multisample
#define GL_NV_explicit_multisample 1

#define GL_SAMPLE_POSITION_NV 0x8E50
#define GL_SAMPLE_MASK_NV 0x8E51
#define GL_SAMPLE_MASK_VALUE_NV 0x8E52
#define GL_TEXTURE_BINDING_RENDERBUFFER_NV 0x8E53
#define GL_TEXTURE_RENDERBUFFER_DATA_STORE_BINDING_NV 0x8E54
#define GL_TEXTURE_RENDERBUFFER_NV 0x8E55
#define GL_SAMPLER_RENDERBUFFER_NV 0x8E56
#define GL_INT_SAMPLER_RENDERBUFFER_NV 0x8E57
#define GL_UNSIGNED_INT_SAMPLER_RENDERBUFFER_NV 0x8E58
#define GL_MAX_SAMPLE_MASK_WORDS_NV 0x8E59

typedef void (GLAPIENTRY * PFNGLGETMULTISAMPLEFVNVPROC) (GLenum pname, GLuint index, GLfloat* val);
typedef void (GLAPIENTRY * PFNGLSAMPLEMASKINDEXEDNVPROC) (GLuint index, GLbitfield mask);
typedef void (GLAPIENTRY * PFNGLTEXRENDERBUFFERNVPROC) (GLenum target, GLuint renderbuffer);

#define glGetMultisamplefvNV GLEW_GET_FUN(__glewGetMultisamplefvNV)
#define glSampleMaskIndexedNV GLEW_GET_FUN(__glewSampleMaskIndexedNV)
#define glTexRenderbufferNV GLEW_GET_FUN(__glewTexRenderbufferNV)

#define GLEW_NV_explicit_multisample GLEW_GET_VAR(__GLEW_NV_explicit_multisample)

#endif /* GL_NV_explicit_multisample */

/* ------------------------------ GL_NV_fence ------------------------------ */

#ifndef GL_NV_fence
#define GL_NV_fence 1

#define GL_ALL_COMPLETED_NV 0x84F2
#define GL_FENCE_STATUS_NV 0x84F3
#define GL_FENCE_CONDITION_NV 0x84F4

typedef void (GLAPIENTRY * PFNGLDELETEFENCESNVPROC) (GLsizei n, const GLuint* fences);
typedef void (GLAPIENTRY * PFNGLFINISHFENCENVPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLGENFENCESNVPROC) (GLsizei n, GLuint* fences);
typedef void (GLAPIENTRY * PFNGLGETFENCEIVNVPROC) (GLuint fence, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISFENCENVPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLSETFENCENVPROC) (GLuint fence, GLenum condition);
typedef GLboolean (GLAPIENTRY * PFNGLTESTFENCENVPROC) (GLuint fence);

#define glDeleteFencesNV GLEW_GET_FUN(__glewDeleteFencesNV)
#define glFinishFenceNV GLEW_GET_FUN(__glewFinishFenceNV)
#define glGenFencesNV GLEW_GET_FUN(__glewGenFencesNV)
#define glGetFenceivNV GLEW_GET_FUN(__glewGetFenceivNV)
#define glIsFenceNV GLEW_GET_FUN(__glewIsFenceNV)
#define glSetFenceNV GLEW_GET_FUN(__glewSetFenceNV)
#define glTestFenceNV GLEW_GET_FUN(__glewTestFenceNV)

#define GLEW_NV_fence GLEW_GET_VAR(__GLEW_NV_fence)

#endif /* GL_NV_fence */

/* --------------------------- GL_NV_float_buffer -------------------------- */

#ifndef GL_NV_float_buffer
#define GL_NV_float_buffer 1

#define GL_FLOAT_R_NV 0x8880
#define GL_FLOAT_RG_NV 0x8881
#define GL_FLOAT_RGB_NV 0x8882
#define GL_FLOAT_RGBA_NV 0x8883
#define GL_FLOAT_R16_NV 0x8884
#define GL_FLOAT_R32_NV 0x8885
#define GL_FLOAT_RG16_NV 0x8886
#define GL_FLOAT_RG32_NV 0x8887
#define GL_FLOAT_RGB16_NV 0x8888
#define GL_FLOAT_RGB32_NV 0x8889
#define GL_FLOAT_RGBA16_NV 0x888A
#define GL_FLOAT_RGBA32_NV 0x888B
#define GL_TEXTURE_FLOAT_COMPONENTS_NV 0x888C
#define GL_FLOAT_CLEAR_COLOR_VALUE_NV 0x888D
#define GL_FLOAT_RGBA_MODE_NV 0x888E

#define GLEW_NV_float_buffer GLEW_GET_VAR(__GLEW_NV_float_buffer)

#endif /* GL_NV_float_buffer */

/* --------------------------- GL_NV_fog_distance -------------------------- */

#ifndef GL_NV_fog_distance
#define GL_NV_fog_distance 1

#define GL_FOG_DISTANCE_MODE_NV 0x855A
#define GL_EYE_RADIAL_NV 0x855B
#define GL_EYE_PLANE_ABSOLUTE_NV 0x855C

#define GLEW_NV_fog_distance GLEW_GET_VAR(__GLEW_NV_fog_distance)

#endif /* GL_NV_fog_distance */

/* ------------------------- GL_NV_fragment_program ------------------------ */

#ifndef GL_NV_fragment_program
#define GL_NV_fragment_program 1

#define GL_MAX_FRAGMENT_PROGRAM_LOCAL_PARAMETERS_NV 0x8868
#define GL_FRAGMENT_PROGRAM_NV 0x8870
#define GL_MAX_TEXTURE_COORDS_NV 0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_NV 0x8872
#define GL_FRAGMENT_PROGRAM_BINDING_NV 0x8873
#define GL_PROGRAM_ERROR_STRING_NV 0x8874

typedef void (GLAPIENTRY * PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC) (GLuint id, GLsizei len, const GLubyte* name, GLdouble *params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC) (GLuint id, GLsizei len, const GLubyte* name, GLfloat *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMNAMEDPARAMETER4DNVPROC) (GLuint id, GLsizei len, const GLubyte* name, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC) (GLuint id, GLsizei len, const GLubyte* name, const GLdouble v[]);
typedef void (GLAPIENTRY * PFNGLPROGRAMNAMEDPARAMETER4FNVPROC) (GLuint id, GLsizei len, const GLubyte* name, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC) (GLuint id, GLsizei len, const GLubyte* name, const GLfloat v[]);

#define glGetProgramNamedParameterdvNV GLEW_GET_FUN(__glewGetProgramNamedParameterdvNV)
#define glGetProgramNamedParameterfvNV GLEW_GET_FUN(__glewGetProgramNamedParameterfvNV)
#define glProgramNamedParameter4dNV GLEW_GET_FUN(__glewProgramNamedParameter4dNV)
#define glProgramNamedParameter4dvNV GLEW_GET_FUN(__glewProgramNamedParameter4dvNV)
#define glProgramNamedParameter4fNV GLEW_GET_FUN(__glewProgramNamedParameter4fNV)
#define glProgramNamedParameter4fvNV GLEW_GET_FUN(__glewProgramNamedParameter4fvNV)

#define GLEW_NV_fragment_program GLEW_GET_VAR(__GLEW_NV_fragment_program)

#endif /* GL_NV_fragment_program */

/* ------------------------ GL_NV_fragment_program2 ------------------------ */

#ifndef GL_NV_fragment_program2
#define GL_NV_fragment_program2 1

#define GL_MAX_PROGRAM_EXEC_INSTRUCTIONS_NV 0x88F4
#define GL_MAX_PROGRAM_CALL_DEPTH_NV 0x88F5
#define GL_MAX_PROGRAM_IF_DEPTH_NV 0x88F6
#define GL_MAX_PROGRAM_LOOP_DEPTH_NV 0x88F7
#define GL_MAX_PROGRAM_LOOP_COUNT_NV 0x88F8

#define GLEW_NV_fragment_program2 GLEW_GET_VAR(__GLEW_NV_fragment_program2)

#endif /* GL_NV_fragment_program2 */

/* ------------------------ GL_NV_fragment_program4 ------------------------ */

#ifndef GL_NV_fragment_program4
#define GL_NV_fragment_program4 1

#define GLEW_NV_fragment_program4 GLEW_GET_VAR(__GLEW_NV_fragment_program4)

#endif /* GL_NV_fragment_program4 */

/* --------------------- GL_NV_fragment_program_option --------------------- */

#ifndef GL_NV_fragment_program_option
#define GL_NV_fragment_program_option 1

#define GLEW_NV_fragment_program_option GLEW_GET_VAR(__GLEW_NV_fragment_program_option)

#endif /* GL_NV_fragment_program_option */

/* ----------------- GL_NV_framebuffer_multisample_coverage ---------------- */

#ifndef GL_NV_framebuffer_multisample_coverage
#define GL_NV_framebuffer_multisample_coverage 1

#define GL_RENDERBUFFER_COVERAGE_SAMPLES_NV 0x8CAB
#define GL_RENDERBUFFER_COLOR_SAMPLES_NV 0x8E10
#define GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV 0x8E11
#define GL_MULTISAMPLE_COVERAGE_MODES_NV 0x8E12

typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC) (GLenum target, GLsizei coverageSamples, GLsizei colorSamples, GLenum internalformat, GLsizei width, GLsizei height);

#define glRenderbufferStorageMultisampleCoverageNV GLEW_GET_FUN(__glewRenderbufferStorageMultisampleCoverageNV)

#define GLEW_NV_framebuffer_multisample_coverage GLEW_GET_VAR(__GLEW_NV_framebuffer_multisample_coverage)

#endif /* GL_NV_framebuffer_multisample_coverage */

/* ------------------------ GL_NV_geometry_program4 ------------------------ */

#ifndef GL_NV_geometry_program4
#define GL_NV_geometry_program4 1

#define GL_GEOMETRY_PROGRAM_NV 0x8C26
#define GL_MAX_PROGRAM_OUTPUT_VERTICES_NV 0x8C27
#define GL_MAX_PROGRAM_TOTAL_OUTPUT_COMPONENTS_NV 0x8C28

typedef void (GLAPIENTRY * PFNGLPROGRAMVERTEXLIMITNVPROC) (GLenum target, GLint limit);

#define glProgramVertexLimitNV GLEW_GET_FUN(__glewProgramVertexLimitNV)

#define GLEW_NV_geometry_program4 GLEW_GET_VAR(__GLEW_NV_geometry_program4)

#endif /* GL_NV_geometry_program4 */

/* ------------------------- GL_NV_geometry_shader4 ------------------------ */

#ifndef GL_NV_geometry_shader4
#define GL_NV_geometry_shader4 1

#define GLEW_NV_geometry_shader4 GLEW_GET_VAR(__GLEW_NV_geometry_shader4)

#endif /* GL_NV_geometry_shader4 */

/* --------------------------- GL_NV_gpu_program4 -------------------------- */

#ifndef GL_NV_gpu_program4
#define GL_NV_gpu_program4 1

#define GL_MIN_PROGRAM_TEXEL_OFFSET_NV 0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET_NV 0x8905
#define GL_PROGRAM_ATTRIB_COMPONENTS_NV 0x8906
#define GL_PROGRAM_RESULT_COMPONENTS_NV 0x8907
#define GL_MAX_PROGRAM_ATTRIB_COMPONENTS_NV 0x8908
#define GL_MAX_PROGRAM_RESULT_COMPONENTS_NV 0x8909
#define GL_MAX_PROGRAM_GENERIC_ATTRIBS_NV 0x8DA5
#define GL_MAX_PROGRAM_GENERIC_RESULTS_NV 0x8DA6

typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERI4INVPROC) (GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERI4IVNVPROC) (GLenum target, GLuint index, const GLint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERI4UINVPROC) (GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERI4UIVNVPROC) (GLenum target, GLuint index, const GLuint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERSI4IVNVPROC) (GLenum target, GLuint index, GLsizei count, const GLint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMENVPARAMETERSI4UIVNVPROC) (GLenum target, GLuint index, GLsizei count, const GLuint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERI4INVPROC) (GLenum target, GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERI4IVNVPROC) (GLenum target, GLuint index, const GLint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERI4UINVPROC) (GLenum target, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERI4UIVNVPROC) (GLenum target, GLuint index, const GLuint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERSI4IVNVPROC) (GLenum target, GLuint index, GLsizei count, const GLint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMLOCALPARAMETERSI4UIVNVPROC) (GLenum target, GLuint index, GLsizei count, const GLuint *params);

#define glProgramEnvParameterI4iNV GLEW_GET_FUN(__glewProgramEnvParameterI4iNV)
#define glProgramEnvParameterI4ivNV GLEW_GET_FUN(__glewProgramEnvParameterI4ivNV)
#define glProgramEnvParameterI4uiNV GLEW_GET_FUN(__glewProgramEnvParameterI4uiNV)
#define glProgramEnvParameterI4uivNV GLEW_GET_FUN(__glewProgramEnvParameterI4uivNV)
#define glProgramEnvParametersI4ivNV GLEW_GET_FUN(__glewProgramEnvParametersI4ivNV)
#define glProgramEnvParametersI4uivNV GLEW_GET_FUN(__glewProgramEnvParametersI4uivNV)
#define glProgramLocalParameterI4iNV GLEW_GET_FUN(__glewProgramLocalParameterI4iNV)
#define glProgramLocalParameterI4ivNV GLEW_GET_FUN(__glewProgramLocalParameterI4ivNV)
#define glProgramLocalParameterI4uiNV GLEW_GET_FUN(__glewProgramLocalParameterI4uiNV)
#define glProgramLocalParameterI4uivNV GLEW_GET_FUN(__glewProgramLocalParameterI4uivNV)
#define glProgramLocalParametersI4ivNV GLEW_GET_FUN(__glewProgramLocalParametersI4ivNV)
#define glProgramLocalParametersI4uivNV GLEW_GET_FUN(__glewProgramLocalParametersI4uivNV)

#define GLEW_NV_gpu_program4 GLEW_GET_VAR(__GLEW_NV_gpu_program4)

#endif /* GL_NV_gpu_program4 */

/* --------------------------- GL_NV_gpu_program5 -------------------------- */

#ifndef GL_NV_gpu_program5
#define GL_NV_gpu_program5 1

#define GL_MAX_GEOMETRY_PROGRAM_INVOCATIONS_NV 0x8E5A
#define GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_NV 0x8E5B
#define GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_NV 0x8E5C
#define GL_FRAGMENT_PROGRAM_INTERPOLATION_OFFSET_BITS_NV 0x8E5D
#define GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET_NV 0x8E5E
#define GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET_NV 0x8E5F

#define GLEW_NV_gpu_program5 GLEW_GET_VAR(__GLEW_NV_gpu_program5)

#endif /* GL_NV_gpu_program5 */

/* ------------------------- GL_NV_gpu_program_fp64 ------------------------ */

#ifndef GL_NV_gpu_program_fp64
#define GL_NV_gpu_program_fp64 1

#define GLEW_NV_gpu_program_fp64 GLEW_GET_VAR(__GLEW_NV_gpu_program_fp64)

#endif /* GL_NV_gpu_program_fp64 */

/* --------------------------- GL_NV_gpu_shader5 --------------------------- */

#ifndef GL_NV_gpu_shader5
#define GL_NV_gpu_shader5 1

#define GL_INT64_NV 0x140E
#define GL_UNSIGNED_INT64_NV 0x140F
#define GL_INT8_NV 0x8FE0
#define GL_INT8_VEC2_NV 0x8FE1
#define GL_INT8_VEC3_NV 0x8FE2
#define GL_INT8_VEC4_NV 0x8FE3
#define GL_INT16_NV 0x8FE4
#define GL_INT16_VEC2_NV 0x8FE5
#define GL_INT16_VEC3_NV 0x8FE6
#define GL_INT16_VEC4_NV 0x8FE7
#define GL_INT64_VEC2_NV 0x8FE9
#define GL_INT64_VEC3_NV 0x8FEA
#define GL_INT64_VEC4_NV 0x8FEB
#define GL_UNSIGNED_INT8_NV 0x8FEC
#define GL_UNSIGNED_INT8_VEC2_NV 0x8FED
#define GL_UNSIGNED_INT8_VEC3_NV 0x8FEE
#define GL_UNSIGNED_INT8_VEC4_NV 0x8FEF
#define GL_UNSIGNED_INT16_NV 0x8FF0
#define GL_UNSIGNED_INT16_VEC2_NV 0x8FF1
#define GL_UNSIGNED_INT16_VEC3_NV 0x8FF2
#define GL_UNSIGNED_INT16_VEC4_NV 0x8FF3
#define GL_UNSIGNED_INT64_VEC2_NV 0x8FF5
#define GL_UNSIGNED_INT64_VEC3_NV 0x8FF6
#define GL_UNSIGNED_INT64_VEC4_NV 0x8FF7
#define GL_FLOAT16_NV 0x8FF8
#define GL_FLOAT16_VEC2_NV 0x8FF9
#define GL_FLOAT16_VEC3_NV 0x8FFA
#define GL_FLOAT16_VEC4_NV 0x8FFB

typedef void (GLAPIENTRY * PFNGLGETUNIFORMI64VNVPROC) (GLuint program, GLint location, GLint64EXT* params);
typedef void (GLAPIENTRY * PFNGLGETUNIFORMUI64VNVPROC) (GLuint program, GLint location, GLuint64EXT* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1I64NVPROC) (GLuint program, GLint location, GLint64EXT x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4I64NVPROC) (GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4I64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UI64NVPROC) (GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4UI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM1I64NVPROC) (GLint location, GLint64EXT x);
typedef void (GLAPIENTRY * PFNGLUNIFORM1I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UI64NVPROC) (GLint location, GLuint64EXT x);
typedef void (GLAPIENTRY * PFNGLUNIFORM1UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y);
typedef void (GLAPIENTRY * PFNGLUNIFORM2I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y);
typedef void (GLAPIENTRY * PFNGLUNIFORM2UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (GLAPIENTRY * PFNGLUNIFORM3I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (GLAPIENTRY * PFNGLUNIFORM3UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4I64NVPROC) (GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (GLAPIENTRY * PFNGLUNIFORM4I64VNVPROC) (GLint location, GLsizei count, const GLint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UI64NVPROC) (GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (GLAPIENTRY * PFNGLUNIFORM4UI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT* value);

#define glGetUniformi64vNV GLEW_GET_FUN(__glewGetUniformi64vNV)
#define glGetUniformui64vNV GLEW_GET_FUN(__glewGetUniformui64vNV)
#define glProgramUniform1i64NV GLEW_GET_FUN(__glewProgramUniform1i64NV)
#define glProgramUniform1i64vNV GLEW_GET_FUN(__glewProgramUniform1i64vNV)
#define glProgramUniform1ui64NV GLEW_GET_FUN(__glewProgramUniform1ui64NV)
#define glProgramUniform1ui64vNV GLEW_GET_FUN(__glewProgramUniform1ui64vNV)
#define glProgramUniform2i64NV GLEW_GET_FUN(__glewProgramUniform2i64NV)
#define glProgramUniform2i64vNV GLEW_GET_FUN(__glewProgramUniform2i64vNV)
#define glProgramUniform2ui64NV GLEW_GET_FUN(__glewProgramUniform2ui64NV)
#define glProgramUniform2ui64vNV GLEW_GET_FUN(__glewProgramUniform2ui64vNV)
#define glProgramUniform3i64NV GLEW_GET_FUN(__glewProgramUniform3i64NV)
#define glProgramUniform3i64vNV GLEW_GET_FUN(__glewProgramUniform3i64vNV)
#define glProgramUniform3ui64NV GLEW_GET_FUN(__glewProgramUniform3ui64NV)
#define glProgramUniform3ui64vNV GLEW_GET_FUN(__glewProgramUniform3ui64vNV)
#define glProgramUniform4i64NV GLEW_GET_FUN(__glewProgramUniform4i64NV)
#define glProgramUniform4i64vNV GLEW_GET_FUN(__glewProgramUniform4i64vNV)
#define glProgramUniform4ui64NV GLEW_GET_FUN(__glewProgramUniform4ui64NV)
#define glProgramUniform4ui64vNV GLEW_GET_FUN(__glewProgramUniform4ui64vNV)
#define glUniform1i64NV GLEW_GET_FUN(__glewUniform1i64NV)
#define glUniform1i64vNV GLEW_GET_FUN(__glewUniform1i64vNV)
#define glUniform1ui64NV GLEW_GET_FUN(__glewUniform1ui64NV)
#define glUniform1ui64vNV GLEW_GET_FUN(__glewUniform1ui64vNV)
#define glUniform2i64NV GLEW_GET_FUN(__glewUniform2i64NV)
#define glUniform2i64vNV GLEW_GET_FUN(__glewUniform2i64vNV)
#define glUniform2ui64NV GLEW_GET_FUN(__glewUniform2ui64NV)
#define glUniform2ui64vNV GLEW_GET_FUN(__glewUniform2ui64vNV)
#define glUniform3i64NV GLEW_GET_FUN(__glewUniform3i64NV)
#define glUniform3i64vNV GLEW_GET_FUN(__glewUniform3i64vNV)
#define glUniform3ui64NV GLEW_GET_FUN(__glewUniform3ui64NV)
#define glUniform3ui64vNV GLEW_GET_FUN(__glewUniform3ui64vNV)
#define glUniform4i64NV GLEW_GET_FUN(__glewUniform4i64NV)
#define glUniform4i64vNV GLEW_GET_FUN(__glewUniform4i64vNV)
#define glUniform4ui64NV GLEW_GET_FUN(__glewUniform4ui64NV)
#define glUniform4ui64vNV GLEW_GET_FUN(__glewUniform4ui64vNV)

#define GLEW_NV_gpu_shader5 GLEW_GET_VAR(__GLEW_NV_gpu_shader5)

#endif /* GL_NV_gpu_shader5 */

/* ---------------------------- GL_NV_half_float --------------------------- */

#ifndef GL_NV_half_float
#define GL_NV_half_float 1

#define GL_HALF_FLOAT_NV 0x140B

typedef unsigned short GLhalf;

typedef void (GLAPIENTRY * PFNGLCOLOR3HNVPROC) (GLhalf red, GLhalf green, GLhalf blue);
typedef void (GLAPIENTRY * PFNGLCOLOR3HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLCOLOR4HNVPROC) (GLhalf red, GLhalf green, GLhalf blue, GLhalf alpha);
typedef void (GLAPIENTRY * PFNGLCOLOR4HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLFOGCOORDHNVPROC) (GLhalf fog);
typedef void (GLAPIENTRY * PFNGLFOGCOORDHVNVPROC) (const GLhalf* fog);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1HNVPROC) (GLenum target, GLhalf s);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD1HVNVPROC) (GLenum target, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2HNVPROC) (GLenum target, GLhalf s, GLhalf t);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD2HVNVPROC) (GLenum target, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3HNVPROC) (GLenum target, GLhalf s, GLhalf t, GLhalf r);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD3HVNVPROC) (GLenum target, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4HNVPROC) (GLenum target, GLhalf s, GLhalf t, GLhalf r, GLhalf q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4HVNVPROC) (GLenum target, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLNORMAL3HNVPROC) (GLhalf nx, GLhalf ny, GLhalf nz);
typedef void (GLAPIENTRY * PFNGLNORMAL3HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3HNVPROC) (GLhalf red, GLhalf green, GLhalf blue);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLOR3HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD1HNVPROC) (GLhalf s);
typedef void (GLAPIENTRY * PFNGLTEXCOORD1HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2HNVPROC) (GLhalf s, GLhalf t);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD3HNVPROC) (GLhalf s, GLhalf t, GLhalf r);
typedef void (GLAPIENTRY * PFNGLTEXCOORD3HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4HNVPROC) (GLhalf s, GLhalf t, GLhalf r, GLhalf q);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEX2HNVPROC) (GLhalf x, GLhalf y);
typedef void (GLAPIENTRY * PFNGLVERTEX2HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEX3HNVPROC) (GLhalf x, GLhalf y, GLhalf z);
typedef void (GLAPIENTRY * PFNGLVERTEX3HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEX4HNVPROC) (GLhalf x, GLhalf y, GLhalf z, GLhalf w);
typedef void (GLAPIENTRY * PFNGLVERTEX4HVNVPROC) (const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1HNVPROC) (GLuint index, GLhalf x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1HVNVPROC) (GLuint index, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2HNVPROC) (GLuint index, GLhalf x, GLhalf y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2HVNVPROC) (GLuint index, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3HNVPROC) (GLuint index, GLhalf x, GLhalf y, GLhalf z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3HVNVPROC) (GLuint index, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4HNVPROC) (GLuint index, GLhalf x, GLhalf y, GLhalf z, GLhalf w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4HVNVPROC) (GLuint index, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS1HVNVPROC) (GLuint index, GLsizei n, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS2HVNVPROC) (GLuint index, GLsizei n, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS3HVNVPROC) (GLuint index, GLsizei n, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS4HVNVPROC) (GLuint index, GLsizei n, const GLhalf* v);
typedef void (GLAPIENTRY * PFNGLVERTEXWEIGHTHNVPROC) (GLhalf weight);
typedef void (GLAPIENTRY * PFNGLVERTEXWEIGHTHVNVPROC) (const GLhalf* weight);

#define glColor3hNV GLEW_GET_FUN(__glewColor3hNV)
#define glColor3hvNV GLEW_GET_FUN(__glewColor3hvNV)
#define glColor4hNV GLEW_GET_FUN(__glewColor4hNV)
#define glColor4hvNV GLEW_GET_FUN(__glewColor4hvNV)
#define glFogCoordhNV GLEW_GET_FUN(__glewFogCoordhNV)
#define glFogCoordhvNV GLEW_GET_FUN(__glewFogCoordhvNV)
#define glMultiTexCoord1hNV GLEW_GET_FUN(__glewMultiTexCoord1hNV)
#define glMultiTexCoord1hvNV GLEW_GET_FUN(__glewMultiTexCoord1hvNV)
#define glMultiTexCoord2hNV GLEW_GET_FUN(__glewMultiTexCoord2hNV)
#define glMultiTexCoord2hvNV GLEW_GET_FUN(__glewMultiTexCoord2hvNV)
#define glMultiTexCoord3hNV GLEW_GET_FUN(__glewMultiTexCoord3hNV)
#define glMultiTexCoord3hvNV GLEW_GET_FUN(__glewMultiTexCoord3hvNV)
#define glMultiTexCoord4hNV GLEW_GET_FUN(__glewMultiTexCoord4hNV)
#define glMultiTexCoord4hvNV GLEW_GET_FUN(__glewMultiTexCoord4hvNV)
#define glNormal3hNV GLEW_GET_FUN(__glewNormal3hNV)
#define glNormal3hvNV GLEW_GET_FUN(__glewNormal3hvNV)
#define glSecondaryColor3hNV GLEW_GET_FUN(__glewSecondaryColor3hNV)
#define glSecondaryColor3hvNV GLEW_GET_FUN(__glewSecondaryColor3hvNV)
#define glTexCoord1hNV GLEW_GET_FUN(__glewTexCoord1hNV)
#define glTexCoord1hvNV GLEW_GET_FUN(__glewTexCoord1hvNV)
#define glTexCoord2hNV GLEW_GET_FUN(__glewTexCoord2hNV)
#define glTexCoord2hvNV GLEW_GET_FUN(__glewTexCoord2hvNV)
#define glTexCoord3hNV GLEW_GET_FUN(__glewTexCoord3hNV)
#define glTexCoord3hvNV GLEW_GET_FUN(__glewTexCoord3hvNV)
#define glTexCoord4hNV GLEW_GET_FUN(__glewTexCoord4hNV)
#define glTexCoord4hvNV GLEW_GET_FUN(__glewTexCoord4hvNV)
#define glVertex2hNV GLEW_GET_FUN(__glewVertex2hNV)
#define glVertex2hvNV GLEW_GET_FUN(__glewVertex2hvNV)
#define glVertex3hNV GLEW_GET_FUN(__glewVertex3hNV)
#define glVertex3hvNV GLEW_GET_FUN(__glewVertex3hvNV)
#define glVertex4hNV GLEW_GET_FUN(__glewVertex4hNV)
#define glVertex4hvNV GLEW_GET_FUN(__glewVertex4hvNV)
#define glVertexAttrib1hNV GLEW_GET_FUN(__glewVertexAttrib1hNV)
#define glVertexAttrib1hvNV GLEW_GET_FUN(__glewVertexAttrib1hvNV)
#define glVertexAttrib2hNV GLEW_GET_FUN(__glewVertexAttrib2hNV)
#define glVertexAttrib2hvNV GLEW_GET_FUN(__glewVertexAttrib2hvNV)
#define glVertexAttrib3hNV GLEW_GET_FUN(__glewVertexAttrib3hNV)
#define glVertexAttrib3hvNV GLEW_GET_FUN(__glewVertexAttrib3hvNV)
#define glVertexAttrib4hNV GLEW_GET_FUN(__glewVertexAttrib4hNV)
#define glVertexAttrib4hvNV GLEW_GET_FUN(__glewVertexAttrib4hvNV)
#define glVertexAttribs1hvNV GLEW_GET_FUN(__glewVertexAttribs1hvNV)
#define glVertexAttribs2hvNV GLEW_GET_FUN(__glewVertexAttribs2hvNV)
#define glVertexAttribs3hvNV GLEW_GET_FUN(__glewVertexAttribs3hvNV)
#define glVertexAttribs4hvNV GLEW_GET_FUN(__glewVertexAttribs4hvNV)
#define glVertexWeighthNV GLEW_GET_FUN(__glewVertexWeighthNV)
#define glVertexWeighthvNV GLEW_GET_FUN(__glewVertexWeighthvNV)

#define GLEW_NV_half_float GLEW_GET_VAR(__GLEW_NV_half_float)

#endif /* GL_NV_half_float */

/* ------------------------ GL_NV_light_max_exponent ----------------------- */

#ifndef GL_NV_light_max_exponent
#define GL_NV_light_max_exponent 1

#define GL_MAX_SHININESS_NV 0x8504
#define GL_MAX_SPOT_EXPONENT_NV 0x8505

#define GLEW_NV_light_max_exponent GLEW_GET_VAR(__GLEW_NV_light_max_exponent)

#endif /* GL_NV_light_max_exponent */

/* ----------------------- GL_NV_multisample_coverage ---------------------- */

#ifndef GL_NV_multisample_coverage
#define GL_NV_multisample_coverage 1

#define GL_COVERAGE_SAMPLES_NV 0x80A9
#define GL_COLOR_SAMPLES_NV 0x8E20

#define GLEW_NV_multisample_coverage GLEW_GET_VAR(__GLEW_NV_multisample_coverage)

#endif /* GL_NV_multisample_coverage */

/* --------------------- GL_NV_multisample_filter_hint --------------------- */

#ifndef GL_NV_multisample_filter_hint
#define GL_NV_multisample_filter_hint 1

#define GL_MULTISAMPLE_FILTER_HINT_NV 0x8534

#define GLEW_NV_multisample_filter_hint GLEW_GET_VAR(__GLEW_NV_multisample_filter_hint)

#endif /* GL_NV_multisample_filter_hint */

/* ------------------------- GL_NV_occlusion_query ------------------------- */

#ifndef GL_NV_occlusion_query
#define GL_NV_occlusion_query 1

#define GL_PIXEL_COUNTER_BITS_NV 0x8864
#define GL_CURRENT_OCCLUSION_QUERY_ID_NV 0x8865
#define GL_PIXEL_COUNT_NV 0x8866
#define GL_PIXEL_COUNT_AVAILABLE_NV 0x8867

typedef void (GLAPIENTRY * PFNGLBEGINOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETEOCCLUSIONQUERIESNVPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLENDOCCLUSIONQUERYNVPROC) (void);
typedef void (GLAPIENTRY * PFNGLGENOCCLUSIONQUERIESNVPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLGETOCCLUSIONQUERYIVNVPROC) (GLuint id, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETOCCLUSIONQUERYUIVNVPROC) (GLuint id, GLenum pname, GLuint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISOCCLUSIONQUERYNVPROC) (GLuint id);

#define glBeginOcclusionQueryNV GLEW_GET_FUN(__glewBeginOcclusionQueryNV)
#define glDeleteOcclusionQueriesNV GLEW_GET_FUN(__glewDeleteOcclusionQueriesNV)
#define glEndOcclusionQueryNV GLEW_GET_FUN(__glewEndOcclusionQueryNV)
#define glGenOcclusionQueriesNV GLEW_GET_FUN(__glewGenOcclusionQueriesNV)
#define glGetOcclusionQueryivNV GLEW_GET_FUN(__glewGetOcclusionQueryivNV)
#define glGetOcclusionQueryuivNV GLEW_GET_FUN(__glewGetOcclusionQueryuivNV)
#define glIsOcclusionQueryNV GLEW_GET_FUN(__glewIsOcclusionQueryNV)

#define GLEW_NV_occlusion_query GLEW_GET_VAR(__GLEW_NV_occlusion_query)

#endif /* GL_NV_occlusion_query */

/* ----------------------- GL_NV_packed_depth_stencil ---------------------- */

#ifndef GL_NV_packed_depth_stencil
#define GL_NV_packed_depth_stencil 1

#define GL_DEPTH_STENCIL_NV 0x84F9
#define GL_UNSIGNED_INT_24_8_NV 0x84FA

#define GLEW_NV_packed_depth_stencil GLEW_GET_VAR(__GLEW_NV_packed_depth_stencil)

#endif /* GL_NV_packed_depth_stencil */

/* --------------------- GL_NV_parameter_buffer_object --------------------- */

#ifndef GL_NV_parameter_buffer_object
#define GL_NV_parameter_buffer_object 1

#define GL_MAX_PROGRAM_PARAMETER_BUFFER_BINDINGS_NV 0x8DA0
#define GL_MAX_PROGRAM_PARAMETER_BUFFER_SIZE_NV 0x8DA1
#define GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV 0x8DA2
#define GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV 0x8DA3
#define GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV 0x8DA4

typedef void (GLAPIENTRY * PFNGLPROGRAMBUFFERPARAMETERSIIVNVPROC) (GLenum target, GLuint buffer, GLuint index, GLsizei count, const GLint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMBUFFERPARAMETERSIUIVNVPROC) (GLenum target, GLuint buffer, GLuint index, GLsizei count, const GLuint *params);
typedef void (GLAPIENTRY * PFNGLPROGRAMBUFFERPARAMETERSFVNVPROC) (GLenum target, GLuint buffer, GLuint index, GLsizei count, const GLfloat *params);

#define glProgramBufferParametersIivNV GLEW_GET_FUN(__glewProgramBufferParametersIivNV)
#define glProgramBufferParametersIuivNV GLEW_GET_FUN(__glewProgramBufferParametersIuivNV)
#define glProgramBufferParametersfvNV GLEW_GET_FUN(__glewProgramBufferParametersfvNV)

#define GLEW_NV_parameter_buffer_object GLEW_GET_VAR(__GLEW_NV_parameter_buffer_object)

#endif /* GL_NV_parameter_buffer_object */

/* --------------------- GL_NV_parameter_buffer_object2 -------------------- */

#ifndef GL_NV_parameter_buffer_object2
#define GL_NV_parameter_buffer_object2 1

#define GLEW_NV_parameter_buffer_object2 GLEW_GET_VAR(__GLEW_NV_parameter_buffer_object2)

#endif /* GL_NV_parameter_buffer_object2 */

/* ------------------------- GL_NV_pixel_data_range ------------------------ */

#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1

#define GL_WRITE_PIXEL_DATA_RANGE_NV 0x8878
#define GL_READ_PIXEL_DATA_RANGE_NV 0x8879
#define GL_WRITE_PIXEL_DATA_RANGE_LENGTH_NV 0x887A
#define GL_READ_PIXEL_DATA_RANGE_LENGTH_NV 0x887B
#define GL_WRITE_PIXEL_DATA_RANGE_POINTER_NV 0x887C
#define GL_READ_PIXEL_DATA_RANGE_POINTER_NV 0x887D

typedef void (GLAPIENTRY * PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, void* pointer);

#define glFlushPixelDataRangeNV GLEW_GET_FUN(__glewFlushPixelDataRangeNV)
#define glPixelDataRangeNV GLEW_GET_FUN(__glewPixelDataRangeNV)

#define GLEW_NV_pixel_data_range GLEW_GET_VAR(__GLEW_NV_pixel_data_range)

#endif /* GL_NV_pixel_data_range */

/* --------------------------- GL_NV_point_sprite -------------------------- */

#ifndef GL_NV_point_sprite
#define GL_NV_point_sprite 1

#define GL_POINT_SPRITE_NV 0x8861
#define GL_COORD_REPLACE_NV 0x8862
#define GL_POINT_SPRITE_R_MODE_NV 0x8863

typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLPOINTPARAMETERIVNVPROC) (GLenum pname, const GLint* params);

#define glPointParameteriNV GLEW_GET_FUN(__glewPointParameteriNV)
#define glPointParameterivNV GLEW_GET_FUN(__glewPointParameterivNV)

#define GLEW_NV_point_sprite GLEW_GET_VAR(__GLEW_NV_point_sprite)

#endif /* GL_NV_point_sprite */

/* -------------------------- GL_NV_present_video -------------------------- */

#ifndef GL_NV_present_video
#define GL_NV_present_video 1

#define GL_FRAME_NV 0x8E26
#define GL_FIELDS_NV 0x8E27
#define GL_CURRENT_TIME_NV 0x8E28
#define GL_NUM_FILL_STREAMS_NV 0x8E29
#define GL_PRESENT_TIME_NV 0x8E2A
#define GL_PRESENT_DURATION_NV 0x8E2B

typedef void (GLAPIENTRY * PFNGLGETVIDEOI64VNVPROC) (GLuint video_slot, GLenum pname, GLint64EXT* params);
typedef void (GLAPIENTRY * PFNGLGETVIDEOIVNVPROC) (GLuint video_slot, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVIDEOUI64VNVPROC) (GLuint video_slot, GLenum pname, GLuint64EXT* params);
typedef void (GLAPIENTRY * PFNGLGETVIDEOUIVNVPROC) (GLuint video_slot, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLPRESENTFRAMEDUALFILLNVPROC) (GLuint video_slot, GLuint64EXT minPresentTime, GLuint beginPresentTimeId, GLuint presentDurationId, GLenum type, GLenum target0, GLuint fill0, GLenum target1, GLuint fill1, GLenum target2, GLuint fill2, GLenum target3, GLuint fill3);
typedef void (GLAPIENTRY * PFNGLPRESENTFRAMEKEYEDNVPROC) (GLuint video_slot, GLuint64EXT minPresentTime, GLuint beginPresentTimeId, GLuint presentDurationId, GLenum type, GLenum target0, GLuint fill0, GLuint key0, GLenum target1, GLuint fill1, GLuint key1);

#define glGetVideoi64vNV GLEW_GET_FUN(__glewGetVideoi64vNV)
#define glGetVideoivNV GLEW_GET_FUN(__glewGetVideoivNV)
#define glGetVideoui64vNV GLEW_GET_FUN(__glewGetVideoui64vNV)
#define glGetVideouivNV GLEW_GET_FUN(__glewGetVideouivNV)
#define glPresentFrameDualFillNV GLEW_GET_FUN(__glewPresentFrameDualFillNV)
#define glPresentFrameKeyedNV GLEW_GET_FUN(__glewPresentFrameKeyedNV)

#define GLEW_NV_present_video GLEW_GET_VAR(__GLEW_NV_present_video)

#endif /* GL_NV_present_video */

/* ------------------------ GL_NV_primitive_restart ------------------------ */

#ifndef GL_NV_primitive_restart
#define GL_NV_primitive_restart 1

#define GL_PRIMITIVE_RESTART_NV 0x8558
#define GL_PRIMITIVE_RESTART_INDEX_NV 0x8559

typedef void (GLAPIENTRY * PFNGLPRIMITIVERESTARTINDEXNVPROC) (GLuint index);
typedef void (GLAPIENTRY * PFNGLPRIMITIVERESTARTNVPROC) (void);

#define glPrimitiveRestartIndexNV GLEW_GET_FUN(__glewPrimitiveRestartIndexNV)
#define glPrimitiveRestartNV GLEW_GET_FUN(__glewPrimitiveRestartNV)

#define GLEW_NV_primitive_restart GLEW_GET_VAR(__GLEW_NV_primitive_restart)

#endif /* GL_NV_primitive_restart */

/* ------------------------ GL_NV_register_combiners ----------------------- */

#ifndef GL_NV_register_combiners
#define GL_NV_register_combiners 1

#define GL_REGISTER_COMBINERS_NV 0x8522
#define GL_VARIABLE_A_NV 0x8523
#define GL_VARIABLE_B_NV 0x8524
#define GL_VARIABLE_C_NV 0x8525
#define GL_VARIABLE_D_NV 0x8526
#define GL_VARIABLE_E_NV 0x8527
#define GL_VARIABLE_F_NV 0x8528
#define GL_VARIABLE_G_NV 0x8529
#define GL_CONSTANT_COLOR0_NV 0x852A
#define GL_CONSTANT_COLOR1_NV 0x852B
#define GL_PRIMARY_COLOR_NV 0x852C
#define GL_SECONDARY_COLOR_NV 0x852D
#define GL_SPARE0_NV 0x852E
#define GL_SPARE1_NV 0x852F
#define GL_DISCARD_NV 0x8530
#define GL_E_TIMES_F_NV 0x8531
#define GL_SPARE0_PLUS_SECONDARY_COLOR_NV 0x8532
#define GL_UNSIGNED_IDENTITY_NV 0x8536
#define GL_UNSIGNED_INVERT_NV 0x8537
#define GL_EXPAND_NORMAL_NV 0x8538
#define GL_EXPAND_NEGATE_NV 0x8539
#define GL_HALF_BIAS_NORMAL_NV 0x853A
#define GL_HALF_BIAS_NEGATE_NV 0x853B
#define GL_SIGNED_IDENTITY_NV 0x853C
#define GL_SIGNED_NEGATE_NV 0x853D
#define GL_SCALE_BY_TWO_NV 0x853E
#define GL_SCALE_BY_FOUR_NV 0x853F
#define GL_SCALE_BY_ONE_HALF_NV 0x8540
#define GL_BIAS_BY_NEGATIVE_ONE_HALF_NV 0x8541
#define GL_COMBINER_INPUT_NV 0x8542
#define GL_COMBINER_MAPPING_NV 0x8543
#define GL_COMBINER_COMPONENT_USAGE_NV 0x8544
#define GL_COMBINER_AB_DOT_PRODUCT_NV 0x8545
#define GL_COMBINER_CD_DOT_PRODUCT_NV 0x8546
#define GL_COMBINER_MUX_SUM_NV 0x8547
#define GL_COMBINER_SCALE_NV 0x8548
#define GL_COMBINER_BIAS_NV 0x8549
#define GL_COMBINER_AB_OUTPUT_NV 0x854A
#define GL_COMBINER_CD_OUTPUT_NV 0x854B
#define GL_COMBINER_SUM_OUTPUT_NV 0x854C
#define GL_MAX_GENERAL_COMBINERS_NV 0x854D
#define GL_NUM_GENERAL_COMBINERS_NV 0x854E
#define GL_COLOR_SUM_CLAMP_NV 0x854F
#define GL_COMBINER0_NV 0x8550
#define GL_COMBINER1_NV 0x8551
#define GL_COMBINER2_NV 0x8552
#define GL_COMBINER3_NV 0x8553
#define GL_COMBINER4_NV 0x8554
#define GL_COMBINER5_NV 0x8555
#define GL_COMBINER6_NV 0x8556
#define GL_COMBINER7_NV 0x8557

typedef void (GLAPIENTRY * PFNGLCOMBINERINPUTNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (GLAPIENTRY * PFNGLCOMBINEROUTPUTNVPROC) (GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum);
typedef void (GLAPIENTRY * PFNGLCOMBINERPARAMETERFNVPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLCOMBINERPARAMETERFVNVPROC) (GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLCOMBINERPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLCOMBINERPARAMETERIVNVPROC) (GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLFINALCOMBINERINPUTNVPROC) (GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (GLAPIENTRY * PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC) (GLenum variable, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC) (GLenum variable, GLenum pname, GLint* params);

#define glCombinerInputNV GLEW_GET_FUN(__glewCombinerInputNV)
#define glCombinerOutputNV GLEW_GET_FUN(__glewCombinerOutputNV)
#define glCombinerParameterfNV GLEW_GET_FUN(__glewCombinerParameterfNV)
#define glCombinerParameterfvNV GLEW_GET_FUN(__glewCombinerParameterfvNV)
#define glCombinerParameteriNV GLEW_GET_FUN(__glewCombinerParameteriNV)
#define glCombinerParameterivNV GLEW_GET_FUN(__glewCombinerParameterivNV)
#define glFinalCombinerInputNV GLEW_GET_FUN(__glewFinalCombinerInputNV)
#define glGetCombinerInputParameterfvNV GLEW_GET_FUN(__glewGetCombinerInputParameterfvNV)
#define glGetCombinerInputParameterivNV GLEW_GET_FUN(__glewGetCombinerInputParameterivNV)
#define glGetCombinerOutputParameterfvNV GLEW_GET_FUN(__glewGetCombinerOutputParameterfvNV)
#define glGetCombinerOutputParameterivNV GLEW_GET_FUN(__glewGetCombinerOutputParameterivNV)
#define glGetFinalCombinerInputParameterfvNV GLEW_GET_FUN(__glewGetFinalCombinerInputParameterfvNV)
#define glGetFinalCombinerInputParameterivNV GLEW_GET_FUN(__glewGetFinalCombinerInputParameterivNV)

#define GLEW_NV_register_combiners GLEW_GET_VAR(__GLEW_NV_register_combiners)

#endif /* GL_NV_register_combiners */

/* ----------------------- GL_NV_register_combiners2 ----------------------- */

#ifndef GL_NV_register_combiners2
#define GL_NV_register_combiners2 1

#define GL_PER_STAGE_CONSTANTS_NV 0x8535

typedef void (GLAPIENTRY * PFNGLCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, GLfloat* params);

#define glCombinerStageParameterfvNV GLEW_GET_FUN(__glewCombinerStageParameterfvNV)
#define glGetCombinerStageParameterfvNV GLEW_GET_FUN(__glewGetCombinerStageParameterfvNV)

#define GLEW_NV_register_combiners2 GLEW_GET_VAR(__GLEW_NV_register_combiners2)

#endif /* GL_NV_register_combiners2 */

/* ------------------------ GL_NV_shader_buffer_load ----------------------- */

#ifndef GL_NV_shader_buffer_load
#define GL_NV_shader_buffer_load 1

#define GL_BUFFER_GPU_ADDRESS_NV 0x8F1D
#define GL_GPU_ADDRESS_NV 0x8F34
#define GL_MAX_SHADER_BUFFER_ADDRESS_NV 0x8F35

typedef void (GLAPIENTRY * PFNGLGETBUFFERPARAMETERUI64VNVPROC) (GLenum target, GLenum pname, GLuint64EXT* params);
typedef void (GLAPIENTRY * PFNGLGETINTEGERUI64VNVPROC) (GLenum value, GLuint64EXT* result);
typedef void (GLAPIENTRY * PFNGLGETNAMEDBUFFERPARAMETERUI64VNVPROC) (GLuint buffer, GLenum pname, GLuint64EXT* params);
typedef GLboolean (GLAPIENTRY * PFNGLISBUFFERRESIDENTNVPROC) (GLenum target);
typedef GLboolean (GLAPIENTRY * PFNGLISNAMEDBUFFERRESIDENTNVPROC) (GLuint buffer);
typedef void (GLAPIENTRY * PFNGLMAKEBUFFERNONRESIDENTNVPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLMAKEBUFFERRESIDENTNVPROC) (GLenum target, GLenum access);
typedef void (GLAPIENTRY * PFNGLMAKENAMEDBUFFERNONRESIDENTNVPROC) (GLuint buffer);
typedef void (GLAPIENTRY * PFNGLMAKENAMEDBUFFERRESIDENTNVPROC) (GLuint buffer, GLenum access);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMUI64NVPROC) (GLuint program, GLint location, GLuint64EXT value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMUI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64EXT* value);
typedef void (GLAPIENTRY * PFNGLUNIFORMUI64NVPROC) (GLint location, GLuint64EXT value);
typedef void (GLAPIENTRY * PFNGLUNIFORMUI64VNVPROC) (GLint location, GLsizei count, const GLuint64EXT* value);

#define glGetBufferParameterui64vNV GLEW_GET_FUN(__glewGetBufferParameterui64vNV)
#define glGetIntegerui64vNV GLEW_GET_FUN(__glewGetIntegerui64vNV)
#define glGetNamedBufferParameterui64vNV GLEW_GET_FUN(__glewGetNamedBufferParameterui64vNV)
#define glIsBufferResidentNV GLEW_GET_FUN(__glewIsBufferResidentNV)
#define glIsNamedBufferResidentNV GLEW_GET_FUN(__glewIsNamedBufferResidentNV)
#define glMakeBufferNonResidentNV GLEW_GET_FUN(__glewMakeBufferNonResidentNV)
#define glMakeBufferResidentNV GLEW_GET_FUN(__glewMakeBufferResidentNV)
#define glMakeNamedBufferNonResidentNV GLEW_GET_FUN(__glewMakeNamedBufferNonResidentNV)
#define glMakeNamedBufferResidentNV GLEW_GET_FUN(__glewMakeNamedBufferResidentNV)
#define glProgramUniformui64NV GLEW_GET_FUN(__glewProgramUniformui64NV)
#define glProgramUniformui64vNV GLEW_GET_FUN(__glewProgramUniformui64vNV)
#define glUniformui64NV GLEW_GET_FUN(__glewUniformui64NV)
#define glUniformui64vNV GLEW_GET_FUN(__glewUniformui64vNV)

#define GLEW_NV_shader_buffer_load GLEW_GET_VAR(__GLEW_NV_shader_buffer_load)

#endif /* GL_NV_shader_buffer_load */

/* ---------------------- GL_NV_tessellation_program5 ---------------------- */

#ifndef GL_NV_tessellation_program5
#define GL_NV_tessellation_program5 1

#define GL_MAX_PROGRAM_PATCH_ATTRIBS_NV 0x86D8
#define GL_TESS_CONTROL_PROGRAM_NV 0x891E
#define GL_TESS_EVALUATION_PROGRAM_NV 0x891F
#define GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_NV 0x8C74
#define GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_NV 0x8C75

#define GLEW_NV_tessellation_program5 GLEW_GET_VAR(__GLEW_NV_tessellation_program5)

#endif /* GL_NV_tessellation_program5 */

/* -------------------------- GL_NV_texgen_emboss -------------------------- */

#ifndef GL_NV_texgen_emboss
#define GL_NV_texgen_emboss 1

#define GL_EMBOSS_LIGHT_NV 0x855D
#define GL_EMBOSS_CONSTANT_NV 0x855E
#define GL_EMBOSS_MAP_NV 0x855F

#define GLEW_NV_texgen_emboss GLEW_GET_VAR(__GLEW_NV_texgen_emboss)

#endif /* GL_NV_texgen_emboss */

/* ------------------------ GL_NV_texgen_reflection ------------------------ */

#ifndef GL_NV_texgen_reflection
#define GL_NV_texgen_reflection 1

#define GL_NORMAL_MAP_NV 0x8511
#define GL_REFLECTION_MAP_NV 0x8512

#define GLEW_NV_texgen_reflection GLEW_GET_VAR(__GLEW_NV_texgen_reflection)

#endif /* GL_NV_texgen_reflection */

/* ------------------------- GL_NV_texture_barrier ------------------------- */

#ifndef GL_NV_texture_barrier
#define GL_NV_texture_barrier 1

typedef void (GLAPIENTRY * PFNGLTEXTUREBARRIERNVPROC) (void);

#define glTextureBarrierNV GLEW_GET_FUN(__glewTextureBarrierNV)

#define GLEW_NV_texture_barrier GLEW_GET_VAR(__GLEW_NV_texture_barrier)

#endif /* GL_NV_texture_barrier */

/* --------------------- GL_NV_texture_compression_vtc --------------------- */

#ifndef GL_NV_texture_compression_vtc
#define GL_NV_texture_compression_vtc 1

#define GLEW_NV_texture_compression_vtc GLEW_GET_VAR(__GLEW_NV_texture_compression_vtc)

#endif /* GL_NV_texture_compression_vtc */

/* ----------------------- GL_NV_texture_env_combine4 ---------------------- */

#ifndef GL_NV_texture_env_combine4
#define GL_NV_texture_env_combine4 1

#define GL_COMBINE4_NV 0x8503
#define GL_SOURCE3_RGB_NV 0x8583
#define GL_SOURCE3_ALPHA_NV 0x858B
#define GL_OPERAND3_RGB_NV 0x8593
#define GL_OPERAND3_ALPHA_NV 0x859B

#define GLEW_NV_texture_env_combine4 GLEW_GET_VAR(__GLEW_NV_texture_env_combine4)

#endif /* GL_NV_texture_env_combine4 */

/* ---------------------- GL_NV_texture_expand_normal ---------------------- */

#ifndef GL_NV_texture_expand_normal
#define GL_NV_texture_expand_normal 1

#define GL_TEXTURE_UNSIGNED_REMAP_MODE_NV 0x888F

#define GLEW_NV_texture_expand_normal GLEW_GET_VAR(__GLEW_NV_texture_expand_normal)

#endif /* GL_NV_texture_expand_normal */

/* ------------------------ GL_NV_texture_rectangle ------------------------ */

#ifndef GL_NV_texture_rectangle
#define GL_NV_texture_rectangle 1

#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_NV 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_NV 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_NV 0x84F8

#define GLEW_NV_texture_rectangle GLEW_GET_VAR(__GLEW_NV_texture_rectangle)

#endif /* GL_NV_texture_rectangle */

/* -------------------------- GL_NV_texture_shader ------------------------- */

#ifndef GL_NV_texture_shader
#define GL_NV_texture_shader 1

#define GL_OFFSET_TEXTURE_RECTANGLE_NV 0x864C
#define GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV 0x864D
#define GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV 0x864E
#define GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV 0x86D9
#define GL_UNSIGNED_INT_S8_S8_8_8_NV 0x86DA
#define GL_UNSIGNED_INT_8_8_S8_S8_REV_NV 0x86DB
#define GL_DSDT_MAG_INTENSITY_NV 0x86DC
#define GL_SHADER_CONSISTENT_NV 0x86DD
#define GL_TEXTURE_SHADER_NV 0x86DE
#define GL_SHADER_OPERATION_NV 0x86DF
#define GL_CULL_MODES_NV 0x86E0
#define GL_OFFSET_TEXTURE_2D_MATRIX_NV 0x86E1
#define GL_OFFSET_TEXTURE_MATRIX_NV 0x86E1
#define GL_OFFSET_TEXTURE_2D_SCALE_NV 0x86E2
#define GL_OFFSET_TEXTURE_SCALE_NV 0x86E2
#define GL_OFFSET_TEXTURE_BIAS_NV 0x86E3
#define GL_OFFSET_TEXTURE_2D_BIAS_NV 0x86E3
#define GL_PREVIOUS_TEXTURE_INPUT_NV 0x86E4
#define GL_CONST_EYE_NV 0x86E5
#define GL_PASS_THROUGH_NV 0x86E6
#define GL_CULL_FRAGMENT_NV 0x86E7
#define GL_OFFSET_TEXTURE_2D_NV 0x86E8
#define GL_DEPENDENT_AR_TEXTURE_2D_NV 0x86E9
#define GL_DEPENDENT_GB_TEXTURE_2D_NV 0x86EA
#define GL_DOT_PRODUCT_NV 0x86EC
#define GL_DOT_PRODUCT_DEPTH_REPLACE_NV 0x86ED
#define GL_DOT_PRODUCT_TEXTURE_2D_NV 0x86EE
#define GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV 0x86F0
#define GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV 0x86F1
#define GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV 0x86F2
#define GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV 0x86F3
#define GL_HILO_NV 0x86F4
#define GL_DSDT_NV 0x86F5
#define GL_DSDT_MAG_NV 0x86F6
#define GL_DSDT_MAG_VIB_NV 0x86F7
#define GL_HILO16_NV 0x86F8
#define GL_SIGNED_HILO_NV 0x86F9
#define GL_SIGNED_HILO16_NV 0x86FA
#define GL_SIGNED_RGBA_NV 0x86FB
#define GL_SIGNED_RGBA8_NV 0x86FC
#define GL_SIGNED_RGB_NV 0x86FE
#define GL_SIGNED_RGB8_NV 0x86FF
#define GL_SIGNED_LUMINANCE_NV 0x8701
#define GL_SIGNED_LUMINANCE8_NV 0x8702
#define GL_SIGNED_LUMINANCE_ALPHA_NV 0x8703
#define GL_SIGNED_LUMINANCE8_ALPHA8_NV 0x8704
#define GL_SIGNED_ALPHA_NV 0x8705
#define GL_SIGNED_ALPHA8_NV 0x8706
#define GL_SIGNED_INTENSITY_NV 0x8707
#define GL_SIGNED_INTENSITY8_NV 0x8708
#define GL_DSDT8_NV 0x8709
#define GL_DSDT8_MAG8_NV 0x870A
#define GL_DSDT8_MAG8_INTENSITY8_NV 0x870B
#define GL_SIGNED_RGB_UNSIGNED_ALPHA_NV 0x870C
#define GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV 0x870D
#define GL_HI_SCALE_NV 0x870E
#define GL_LO_SCALE_NV 0x870F
#define GL_DS_SCALE_NV 0x8710
#define GL_DT_SCALE_NV 0x8711
#define GL_MAGNITUDE_SCALE_NV 0x8712
#define GL_VIBRANCE_SCALE_NV 0x8713
#define GL_HI_BIAS_NV 0x8714
#define GL_LO_BIAS_NV 0x8715
#define GL_DS_BIAS_NV 0x8716
#define GL_DT_BIAS_NV 0x8717
#define GL_MAGNITUDE_BIAS_NV 0x8718
#define GL_VIBRANCE_BIAS_NV 0x8719
#define GL_TEXTURE_BORDER_VALUES_NV 0x871A
#define GL_TEXTURE_HI_SIZE_NV 0x871B
#define GL_TEXTURE_LO_SIZE_NV 0x871C
#define GL_TEXTURE_DS_SIZE_NV 0x871D
#define GL_TEXTURE_DT_SIZE_NV 0x871E
#define GL_TEXTURE_MAG_SIZE_NV 0x871F

#define GLEW_NV_texture_shader GLEW_GET_VAR(__GLEW_NV_texture_shader)

#endif /* GL_NV_texture_shader */

/* ------------------------- GL_NV_texture_shader2 ------------------------- */

#ifndef GL_NV_texture_shader2
#define GL_NV_texture_shader2 1

#define GL_UNSIGNED_INT_S8_S8_8_8_NV 0x86DA
#define GL_UNSIGNED_INT_8_8_S8_S8_REV_NV 0x86DB
#define GL_DSDT_MAG_INTENSITY_NV 0x86DC
#define GL_DOT_PRODUCT_TEXTURE_3D_NV 0x86EF
#define GL_HILO_NV 0x86F4
#define GL_DSDT_NV 0x86F5
#define GL_DSDT_MAG_NV 0x86F6
#define GL_DSDT_MAG_VIB_NV 0x86F7
#define GL_HILO16_NV 0x86F8
#define GL_SIGNED_HILO_NV 0x86F9
#define GL_SIGNED_HILO16_NV 0x86FA
#define GL_SIGNED_RGBA_NV 0x86FB
#define GL_SIGNED_RGBA8_NV 0x86FC
#define GL_SIGNED_RGB_NV 0x86FE
#define GL_SIGNED_RGB8_NV 0x86FF
#define GL_SIGNED_LUMINANCE_NV 0x8701
#define GL_SIGNED_LUMINANCE8_NV 0x8702
#define GL_SIGNED_LUMINANCE_ALPHA_NV 0x8703
#define GL_SIGNED_LUMINANCE8_ALPHA8_NV 0x8704
#define GL_SIGNED_ALPHA_NV 0x8705
#define GL_SIGNED_ALPHA8_NV 0x8706
#define GL_SIGNED_INTENSITY_NV 0x8707
#define GL_SIGNED_INTENSITY8_NV 0x8708
#define GL_DSDT8_NV 0x8709
#define GL_DSDT8_MAG8_NV 0x870A
#define GL_DSDT8_MAG8_INTENSITY8_NV 0x870B
#define GL_SIGNED_RGB_UNSIGNED_ALPHA_NV 0x870C
#define GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV 0x870D

#define GLEW_NV_texture_shader2 GLEW_GET_VAR(__GLEW_NV_texture_shader2)

#endif /* GL_NV_texture_shader2 */

/* ------------------------- GL_NV_texture_shader3 ------------------------- */

#ifndef GL_NV_texture_shader3
#define GL_NV_texture_shader3 1

#define GL_OFFSET_PROJECTIVE_TEXTURE_2D_NV 0x8850
#define GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_NV 0x8851
#define GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_NV 0x8852
#define GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_NV 0x8853
#define GL_OFFSET_HILO_TEXTURE_2D_NV 0x8854
#define GL_OFFSET_HILO_TEXTURE_RECTANGLE_NV 0x8855
#define GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_NV 0x8856
#define GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_NV 0x8857
#define GL_DEPENDENT_HILO_TEXTURE_2D_NV 0x8858
#define GL_DEPENDENT_RGB_TEXTURE_3D_NV 0x8859
#define GL_DEPENDENT_RGB_TEXTURE_CUBE_MAP_NV 0x885A
#define GL_DOT_PRODUCT_PASS_THROUGH_NV 0x885B
#define GL_DOT_PRODUCT_TEXTURE_1D_NV 0x885C
#define GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_NV 0x885D
#define GL_HILO8_NV 0x885E
#define GL_SIGNED_HILO8_NV 0x885F
#define GL_FORCE_BLUE_TO_ONE_NV 0x8860

#define GLEW_NV_texture_shader3 GLEW_GET_VAR(__GLEW_NV_texture_shader3)

#endif /* GL_NV_texture_shader3 */

/* ------------------------ GL_NV_transform_feedback ----------------------- */

#ifndef GL_NV_transform_feedback
#define GL_NV_transform_feedback 1

#define GL_BACK_PRIMARY_COLOR_NV 0x8C77
#define GL_BACK_SECONDARY_COLOR_NV 0x8C78
#define GL_TEXTURE_COORD_NV 0x8C79
#define GL_CLIP_DISTANCE_NV 0x8C7A
#define GL_VERTEX_ID_NV 0x8C7B
#define GL_PRIMITIVE_ID_NV 0x8C7C
#define GL_GENERIC_ATTRIB_NV 0x8C7D
#define GL_TRANSFORM_FEEDBACK_ATTRIBS_NV 0x8C7E
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE_NV 0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_NV 0x8C80
#define GL_ACTIVE_VARYINGS_NV 0x8C81
#define GL_ACTIVE_VARYING_MAX_LENGTH_NV 0x8C82
#define GL_TRANSFORM_FEEDBACK_VARYINGS_NV 0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START_NV 0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_NV 0x8C85
#define GL_TRANSFORM_FEEDBACK_RECORD_NV 0x8C86
#define GL_PRIMITIVES_GENERATED_NV 0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_NV 0x8C88
#define GL_RASTERIZER_DISCARD_NV 0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_NV 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_NV 0x8C8B
#define GL_INTERLEAVED_ATTRIBS_NV 0x8C8C
#define GL_SEPARATE_ATTRIBS_NV 0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER_NV 0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV 0x8C8F

typedef void (GLAPIENTRY * PFNGLACTIVEVARYINGNVPROC) (GLuint program, const GLchar *name);
typedef void (GLAPIENTRY * PFNGLBEGINTRANSFORMFEEDBACKNVPROC) (GLenum primitiveMode);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERBASENVPROC) (GLenum target, GLuint index, GLuint buffer);
typedef void (GLAPIENTRY * PFNGLBINDBUFFEROFFSETNVPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset);
typedef void (GLAPIENTRY * PFNGLBINDBUFFERRANGENVPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (GLAPIENTRY * PFNGLENDTRANSFORMFEEDBACKNVPROC) (void);
typedef void (GLAPIENTRY * PFNGLGETACTIVEVARYINGNVPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
typedef void (GLAPIENTRY * PFNGLGETTRANSFORMFEEDBACKVARYINGNVPROC) (GLuint program, GLuint index, GLint *location);
typedef GLint (GLAPIENTRY * PFNGLGETVARYINGLOCATIONNVPROC) (GLuint program, const GLchar *name);
typedef void (GLAPIENTRY * PFNGLTRANSFORMFEEDBACKATTRIBSNVPROC) (GLuint count, const GLint *attribs, GLenum bufferMode);
typedef void (GLAPIENTRY * PFNGLTRANSFORMFEEDBACKVARYINGSNVPROC) (GLuint program, GLsizei count, const GLint *locations, GLenum bufferMode);

#define glActiveVaryingNV GLEW_GET_FUN(__glewActiveVaryingNV)
#define glBeginTransformFeedbackNV GLEW_GET_FUN(__glewBeginTransformFeedbackNV)
#define glBindBufferBaseNV GLEW_GET_FUN(__glewBindBufferBaseNV)
#define glBindBufferOffsetNV GLEW_GET_FUN(__glewBindBufferOffsetNV)
#define glBindBufferRangeNV GLEW_GET_FUN(__glewBindBufferRangeNV)
#define glEndTransformFeedbackNV GLEW_GET_FUN(__glewEndTransformFeedbackNV)
#define glGetActiveVaryingNV GLEW_GET_FUN(__glewGetActiveVaryingNV)
#define glGetTransformFeedbackVaryingNV GLEW_GET_FUN(__glewGetTransformFeedbackVaryingNV)
#define glGetVaryingLocationNV GLEW_GET_FUN(__glewGetVaryingLocationNV)
#define glTransformFeedbackAttribsNV GLEW_GET_FUN(__glewTransformFeedbackAttribsNV)
#define glTransformFeedbackVaryingsNV GLEW_GET_FUN(__glewTransformFeedbackVaryingsNV)

#define GLEW_NV_transform_feedback GLEW_GET_VAR(__GLEW_NV_transform_feedback)

#endif /* GL_NV_transform_feedback */

/* ----------------------- GL_NV_transform_feedback2 ----------------------- */

#ifndef GL_NV_transform_feedback2
#define GL_NV_transform_feedback2 1

#define GL_TRANSFORM_FEEDBACK_NV 0x8E22
#define GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED_NV 0x8E23
#define GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE_NV 0x8E24
#define GL_TRANSFORM_FEEDBACK_BINDING_NV 0x8E25

typedef void (GLAPIENTRY * PFNGLBINDTRANSFORMFEEDBACKNVPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETETRANSFORMFEEDBACKSNVPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLDRAWTRANSFORMFEEDBACKNVPROC) (GLenum mode, GLuint id);
typedef void (GLAPIENTRY * PFNGLGENTRANSFORMFEEDBACKSNVPROC) (GLsizei n, GLuint* ids);
typedef GLboolean (GLAPIENTRY * PFNGLISTRANSFORMFEEDBACKNVPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLPAUSETRANSFORMFEEDBACKNVPROC) (void);
typedef void (GLAPIENTRY * PFNGLRESUMETRANSFORMFEEDBACKNVPROC) (void);

#define glBindTransformFeedbackNV GLEW_GET_FUN(__glewBindTransformFeedbackNV)
#define glDeleteTransformFeedbacksNV GLEW_GET_FUN(__glewDeleteTransformFeedbacksNV)
#define glDrawTransformFeedbackNV GLEW_GET_FUN(__glewDrawTransformFeedbackNV)
#define glGenTransformFeedbacksNV GLEW_GET_FUN(__glewGenTransformFeedbacksNV)
#define glIsTransformFeedbackNV GLEW_GET_FUN(__glewIsTransformFeedbackNV)
#define glPauseTransformFeedbackNV GLEW_GET_FUN(__glewPauseTransformFeedbackNV)
#define glResumeTransformFeedbackNV GLEW_GET_FUN(__glewResumeTransformFeedbackNV)

#define GLEW_NV_transform_feedback2 GLEW_GET_VAR(__GLEW_NV_transform_feedback2)

#endif /* GL_NV_transform_feedback2 */

/* -------------------------- GL_NV_vdpau_interop -------------------------- */

#ifndef GL_NV_vdpau_interop
#define GL_NV_vdpau_interop 1

#define GL_SURFACE_STATE_NV 0x86EB
#define GL_SURFACE_REGISTERED_NV 0x86FD
#define GL_SURFACE_MAPPED_NV 0x8700
#define GL_WRITE_DISCARD_NV 0x88BE

typedef GLintptr GLvdpauSurfaceNV;

typedef void (GLAPIENTRY * PFNGLVDPAUFININVPROC) (void);
typedef void (GLAPIENTRY * PFNGLVDPAUGETSURFACEIVNVPROC) (GLvdpauSurfaceNV surface, GLenum pname, GLsizei bufSize, GLsizei* length, GLint *values);
typedef void (GLAPIENTRY * PFNGLVDPAUINITNVPROC) (const void* vdpDevice, const GLvoid*getProcAddress);
typedef void (GLAPIENTRY * PFNGLVDPAUISSURFACENVPROC) (GLvdpauSurfaceNV surface);
typedef void (GLAPIENTRY * PFNGLVDPAUMAPSURFACESNVPROC) (GLsizei numSurfaces, const GLvdpauSurfaceNV* surfaces);
typedef GLvdpauSurfaceNV (GLAPIENTRY * PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC) (const void* vdpSurface, GLenum target, GLsizei numTextureNames, const GLuint *textureNames);
typedef GLvdpauSurfaceNV (GLAPIENTRY * PFNGLVDPAUREGISTERVIDEOSURFACENVPROC) (const void* vdpSurface, GLenum target, GLsizei numTextureNames, const GLuint *textureNames);
typedef void (GLAPIENTRY * PFNGLVDPAUSURFACEACCESSNVPROC) (GLvdpauSurfaceNV surface, GLenum access);
typedef void (GLAPIENTRY * PFNGLVDPAUUNMAPSURFACESNVPROC) (GLsizei numSurface, const GLvdpauSurfaceNV* surfaces);
typedef void (GLAPIENTRY * PFNGLVDPAUUNREGISTERSURFACENVPROC) (GLvdpauSurfaceNV surface);

#define glVDPAUFiniNV GLEW_GET_FUN(__glewVDPAUFiniNV)
#define glVDPAUGetSurfaceivNV GLEW_GET_FUN(__glewVDPAUGetSurfaceivNV)
#define glVDPAUInitNV GLEW_GET_FUN(__glewVDPAUInitNV)
#define glVDPAUIsSurfaceNV GLEW_GET_FUN(__glewVDPAUIsSurfaceNV)
#define glVDPAUMapSurfacesNV GLEW_GET_FUN(__glewVDPAUMapSurfacesNV)
#define glVDPAURegisterOutputSurfaceNV GLEW_GET_FUN(__glewVDPAURegisterOutputSurfaceNV)
#define glVDPAURegisterVideoSurfaceNV GLEW_GET_FUN(__glewVDPAURegisterVideoSurfaceNV)
#define glVDPAUSurfaceAccessNV GLEW_GET_FUN(__glewVDPAUSurfaceAccessNV)
#define glVDPAUUnmapSurfacesNV GLEW_GET_FUN(__glewVDPAUUnmapSurfacesNV)
#define glVDPAUUnregisterSurfaceNV GLEW_GET_FUN(__glewVDPAUUnregisterSurfaceNV)

#define GLEW_NV_vdpau_interop GLEW_GET_VAR(__GLEW_NV_vdpau_interop)

#endif /* GL_NV_vdpau_interop */

/* ------------------------ GL_NV_vertex_array_range ----------------------- */

#ifndef GL_NV_vertex_array_range
#define GL_NV_vertex_array_range 1

#define GL_VERTEX_ARRAY_RANGE_NV 0x851D
#define GL_VERTEX_ARRAY_RANGE_LENGTH_NV 0x851E
#define GL_VERTEX_ARRAY_RANGE_VALID_NV 0x851F
#define GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV 0x8520
#define GL_VERTEX_ARRAY_RANGE_POINTER_NV 0x8521

typedef void (GLAPIENTRY * PFNGLFLUSHVERTEXARRAYRANGENVPROC) (void);
typedef void (GLAPIENTRY * PFNGLVERTEXARRAYRANGENVPROC) (GLsizei length, void* pointer);

#define glFlushVertexArrayRangeNV GLEW_GET_FUN(__glewFlushVertexArrayRangeNV)
#define glVertexArrayRangeNV GLEW_GET_FUN(__glewVertexArrayRangeNV)

#define GLEW_NV_vertex_array_range GLEW_GET_VAR(__GLEW_NV_vertex_array_range)

#endif /* GL_NV_vertex_array_range */

/* ----------------------- GL_NV_vertex_array_range2 ----------------------- */

#ifndef GL_NV_vertex_array_range2
#define GL_NV_vertex_array_range2 1

#define GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV 0x8533

#define GLEW_NV_vertex_array_range2 GLEW_GET_VAR(__GLEW_NV_vertex_array_range2)

#endif /* GL_NV_vertex_array_range2 */

/* ------------------- GL_NV_vertex_attrib_integer_64bit ------------------- */

#ifndef GL_NV_vertex_attrib_integer_64bit
#define GL_NV_vertex_attrib_integer_64bit 1

#define GL_INT64_NV 0x140E
#define GL_UNSIGNED_INT64_NV 0x140F

typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBLI64VNVPROC) (GLuint index, GLenum pname, GLint64EXT* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBLUI64VNVPROC) (GLuint index, GLenum pname, GLuint64EXT* params);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1I64NVPROC) (GLuint index, GLint64EXT x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1I64VNVPROC) (GLuint index, const GLint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1UI64NVPROC) (GLuint index, GLuint64EXT x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL1UI64VNVPROC) (GLuint index, const GLuint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2I64VNVPROC) (GLuint index, const GLint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL2UI64VNVPROC) (GLuint index, const GLuint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y, GLint64EXT z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3I64VNVPROC) (GLuint index, const GLint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL3UI64VNVPROC) (GLuint index, const GLuint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4I64NVPROC) (GLuint index, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4I64VNVPROC) (GLuint index, const GLint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4UI64NVPROC) (GLuint index, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBL4UI64VNVPROC) (GLuint index, const GLuint64EXT* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBLFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLsizei stride);

#define glGetVertexAttribLi64vNV GLEW_GET_FUN(__glewGetVertexAttribLi64vNV)
#define glGetVertexAttribLui64vNV GLEW_GET_FUN(__glewGetVertexAttribLui64vNV)
#define glVertexAttribL1i64NV GLEW_GET_FUN(__glewVertexAttribL1i64NV)
#define glVertexAttribL1i64vNV GLEW_GET_FUN(__glewVertexAttribL1i64vNV)
#define glVertexAttribL1ui64NV GLEW_GET_FUN(__glewVertexAttribL1ui64NV)
#define glVertexAttribL1ui64vNV GLEW_GET_FUN(__glewVertexAttribL1ui64vNV)
#define glVertexAttribL2i64NV GLEW_GET_FUN(__glewVertexAttribL2i64NV)
#define glVertexAttribL2i64vNV GLEW_GET_FUN(__glewVertexAttribL2i64vNV)
#define glVertexAttribL2ui64NV GLEW_GET_FUN(__glewVertexAttribL2ui64NV)
#define glVertexAttribL2ui64vNV GLEW_GET_FUN(__glewVertexAttribL2ui64vNV)
#define glVertexAttribL3i64NV GLEW_GET_FUN(__glewVertexAttribL3i64NV)
#define glVertexAttribL3i64vNV GLEW_GET_FUN(__glewVertexAttribL3i64vNV)
#define glVertexAttribL3ui64NV GLEW_GET_FUN(__glewVertexAttribL3ui64NV)
#define glVertexAttribL3ui64vNV GLEW_GET_FUN(__glewVertexAttribL3ui64vNV)
#define glVertexAttribL4i64NV GLEW_GET_FUN(__glewVertexAttribL4i64NV)
#define glVertexAttribL4i64vNV GLEW_GET_FUN(__glewVertexAttribL4i64vNV)
#define glVertexAttribL4ui64NV GLEW_GET_FUN(__glewVertexAttribL4ui64NV)
#define glVertexAttribL4ui64vNV GLEW_GET_FUN(__glewVertexAttribL4ui64vNV)
#define glVertexAttribLFormatNV GLEW_GET_FUN(__glewVertexAttribLFormatNV)

#define GLEW_NV_vertex_attrib_integer_64bit GLEW_GET_VAR(__GLEW_NV_vertex_attrib_integer_64bit)

#endif /* GL_NV_vertex_attrib_integer_64bit */

/* ------------------- GL_NV_vertex_buffer_unified_memory ------------------ */

#ifndef GL_NV_vertex_buffer_unified_memory
#define GL_NV_vertex_buffer_unified_memory 1

#define GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV 0x8F1E
#define GL_ELEMENT_ARRAY_UNIFIED_NV 0x8F1F
#define GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV 0x8F20
#define GL_VERTEX_ARRAY_ADDRESS_NV 0x8F21
#define GL_NORMAL_ARRAY_ADDRESS_NV 0x8F22
#define GL_COLOR_ARRAY_ADDRESS_NV 0x8F23
#define GL_INDEX_ARRAY_ADDRESS_NV 0x8F24
#define GL_TEXTURE_COORD_ARRAY_ADDRESS_NV 0x8F25
#define GL_EDGE_FLAG_ARRAY_ADDRESS_NV 0x8F26
#define GL_SECONDARY_COLOR_ARRAY_ADDRESS_NV 0x8F27
#define GL_FOG_COORD_ARRAY_ADDRESS_NV 0x8F28
#define GL_ELEMENT_ARRAY_ADDRESS_NV 0x8F29
#define GL_VERTEX_ATTRIB_ARRAY_LENGTH_NV 0x8F2A
#define GL_VERTEX_ARRAY_LENGTH_NV 0x8F2B
#define GL_NORMAL_ARRAY_LENGTH_NV 0x8F2C
#define GL_COLOR_ARRAY_LENGTH_NV 0x8F2D
#define GL_INDEX_ARRAY_LENGTH_NV 0x8F2E
#define GL_TEXTURE_COORD_ARRAY_LENGTH_NV 0x8F2F
#define GL_EDGE_FLAG_ARRAY_LENGTH_NV 0x8F30
#define GL_SECONDARY_COLOR_ARRAY_LENGTH_NV 0x8F31
#define GL_FOG_COORD_ARRAY_LENGTH_NV 0x8F32
#define GL_ELEMENT_ARRAY_LENGTH_NV 0x8F33
#define GL_DRAW_INDIRECT_UNIFIED_NV 0x8F40
#define GL_DRAW_INDIRECT_ADDRESS_NV 0x8F41
#define GL_DRAW_INDIRECT_LENGTH_NV 0x8F42

typedef void (GLAPIENTRY * PFNGLBUFFERADDRESSRANGENVPROC) (GLenum pname, GLuint index, GLuint64EXT address, GLsizeiptr length);
typedef void (GLAPIENTRY * PFNGLCOLORFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLEDGEFLAGFORMATNVPROC) (GLsizei stride);
typedef void (GLAPIENTRY * PFNGLFOGCOORDFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLGETINTEGERUI64I_VNVPROC) (GLenum value, GLuint index, GLuint64EXT result[]);
typedef void (GLAPIENTRY * PFNGLINDEXFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLNORMALFORMATNVPROC) (GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLSECONDARYCOLORFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLTEXCOORDFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBIFORMATNVPROC) (GLuint index, GLint size, GLenum type, GLsizei stride);
typedef void (GLAPIENTRY * PFNGLVERTEXFORMATNVPROC) (GLint size, GLenum type, GLsizei stride);

#define glBufferAddressRangeNV GLEW_GET_FUN(__glewBufferAddressRangeNV)
#define glColorFormatNV GLEW_GET_FUN(__glewColorFormatNV)
#define glEdgeFlagFormatNV GLEW_GET_FUN(__glewEdgeFlagFormatNV)
#define glFogCoordFormatNV GLEW_GET_FUN(__glewFogCoordFormatNV)
#define glGetIntegerui64i_vNV GLEW_GET_FUN(__glewGetIntegerui64i_vNV)
#define glIndexFormatNV GLEW_GET_FUN(__glewIndexFormatNV)
#define glNormalFormatNV GLEW_GET_FUN(__glewNormalFormatNV)
#define glSecondaryColorFormatNV GLEW_GET_FUN(__glewSecondaryColorFormatNV)
#define glTexCoordFormatNV GLEW_GET_FUN(__glewTexCoordFormatNV)
#define glVertexAttribFormatNV GLEW_GET_FUN(__glewVertexAttribFormatNV)
#define glVertexAttribIFormatNV GLEW_GET_FUN(__glewVertexAttribIFormatNV)
#define glVertexFormatNV GLEW_GET_FUN(__glewVertexFormatNV)

#define GLEW_NV_vertex_buffer_unified_memory GLEW_GET_VAR(__GLEW_NV_vertex_buffer_unified_memory)

#endif /* GL_NV_vertex_buffer_unified_memory */

/* -------------------------- GL_NV_vertex_program ------------------------- */

#ifndef GL_NV_vertex_program
#define GL_NV_vertex_program 1

#define GL_VERTEX_PROGRAM_NV 0x8620
#define GL_VERTEX_STATE_PROGRAM_NV 0x8621
#define GL_ATTRIB_ARRAY_SIZE_NV 0x8623
#define GL_ATTRIB_ARRAY_STRIDE_NV 0x8624
#define GL_ATTRIB_ARRAY_TYPE_NV 0x8625
#define GL_CURRENT_ATTRIB_NV 0x8626
#define GL_PROGRAM_LENGTH_NV 0x8627
#define GL_PROGRAM_STRING_NV 0x8628
#define GL_MODELVIEW_PROJECTION_NV 0x8629
#define GL_IDENTITY_NV 0x862A
#define GL_INVERSE_NV 0x862B
#define GL_TRANSPOSE_NV 0x862C
#define GL_INVERSE_TRANSPOSE_NV 0x862D
#define GL_MAX_TRACK_MATRIX_STACK_DEPTH_NV 0x862E
#define GL_MAX_TRACK_MATRICES_NV 0x862F
#define GL_MATRIX0_NV 0x8630
#define GL_MATRIX1_NV 0x8631
#define GL_MATRIX2_NV 0x8632
#define GL_MATRIX3_NV 0x8633
#define GL_MATRIX4_NV 0x8634
#define GL_MATRIX5_NV 0x8635
#define GL_MATRIX6_NV 0x8636
#define GL_MATRIX7_NV 0x8637
#define GL_CURRENT_MATRIX_STACK_DEPTH_NV 0x8640
#define GL_CURRENT_MATRIX_NV 0x8641
#define GL_VERTEX_PROGRAM_POINT_SIZE_NV 0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_NV 0x8643
#define GL_PROGRAM_PARAMETER_NV 0x8644
#define GL_ATTRIB_ARRAY_POINTER_NV 0x8645
#define GL_PROGRAM_TARGET_NV 0x8646
#define GL_PROGRAM_RESIDENT_NV 0x8647
#define GL_TRACK_MATRIX_NV 0x8648
#define GL_TRACK_MATRIX_TRANSFORM_NV 0x8649
#define GL_VERTEX_PROGRAM_BINDING_NV 0x864A
#define GL_PROGRAM_ERROR_POSITION_NV 0x864B
#define GL_VERTEX_ATTRIB_ARRAY0_NV 0x8650
#define GL_VERTEX_ATTRIB_ARRAY1_NV 0x8651
#define GL_VERTEX_ATTRIB_ARRAY2_NV 0x8652
#define GL_VERTEX_ATTRIB_ARRAY3_NV 0x8653
#define GL_VERTEX_ATTRIB_ARRAY4_NV 0x8654
#define GL_VERTEX_ATTRIB_ARRAY5_NV 0x8655
#define GL_VERTEX_ATTRIB_ARRAY6_NV 0x8656
#define GL_VERTEX_ATTRIB_ARRAY7_NV 0x8657
#define GL_VERTEX_ATTRIB_ARRAY8_NV 0x8658
#define GL_VERTEX_ATTRIB_ARRAY9_NV 0x8659
#define GL_VERTEX_ATTRIB_ARRAY10_NV 0x865A
#define GL_VERTEX_ATTRIB_ARRAY11_NV 0x865B
#define GL_VERTEX_ATTRIB_ARRAY12_NV 0x865C
#define GL_VERTEX_ATTRIB_ARRAY13_NV 0x865D
#define GL_VERTEX_ATTRIB_ARRAY14_NV 0x865E
#define GL_VERTEX_ATTRIB_ARRAY15_NV 0x865F
#define GL_MAP1_VERTEX_ATTRIB0_4_NV 0x8660
#define GL_MAP1_VERTEX_ATTRIB1_4_NV 0x8661
#define GL_MAP1_VERTEX_ATTRIB2_4_NV 0x8662
#define GL_MAP1_VERTEX_ATTRIB3_4_NV 0x8663
#define GL_MAP1_VERTEX_ATTRIB4_4_NV 0x8664
#define GL_MAP1_VERTEX_ATTRIB5_4_NV 0x8665
#define GL_MAP1_VERTEX_ATTRIB6_4_NV 0x8666
#define GL_MAP1_VERTEX_ATTRIB7_4_NV 0x8667
#define GL_MAP1_VERTEX_ATTRIB8_4_NV 0x8668
#define GL_MAP1_VERTEX_ATTRIB9_4_NV 0x8669
#define GL_MAP1_VERTEX_ATTRIB10_4_NV 0x866A
#define GL_MAP1_VERTEX_ATTRIB11_4_NV 0x866B
#define GL_MAP1_VERTEX_ATTRIB12_4_NV 0x866C
#define GL_MAP1_VERTEX_ATTRIB13_4_NV 0x866D
#define GL_MAP1_VERTEX_ATTRIB14_4_NV 0x866E
#define GL_MAP1_VERTEX_ATTRIB15_4_NV 0x866F
#define GL_MAP2_VERTEX_ATTRIB0_4_NV 0x8670
#define GL_MAP2_VERTEX_ATTRIB1_4_NV 0x8671
#define GL_MAP2_VERTEX_ATTRIB2_4_NV 0x8672
#define GL_MAP2_VERTEX_ATTRIB3_4_NV 0x8673
#define GL_MAP2_VERTEX_ATTRIB4_4_NV 0x8674
#define GL_MAP2_VERTEX_ATTRIB5_4_NV 0x8675
#define GL_MAP2_VERTEX_ATTRIB6_4_NV 0x8676
#define GL_MAP2_VERTEX_ATTRIB7_4_NV 0x8677
#define GL_MAP2_VERTEX_ATTRIB8_4_NV 0x8678
#define GL_MAP2_VERTEX_ATTRIB9_4_NV 0x8679
#define GL_MAP2_VERTEX_ATTRIB10_4_NV 0x867A
#define GL_MAP2_VERTEX_ATTRIB11_4_NV 0x867B
#define GL_MAP2_VERTEX_ATTRIB12_4_NV 0x867C
#define GL_MAP2_VERTEX_ATTRIB13_4_NV 0x867D
#define GL_MAP2_VERTEX_ATTRIB14_4_NV 0x867E
#define GL_MAP2_VERTEX_ATTRIB15_4_NV 0x867F

typedef GLboolean (GLAPIENTRY * PFNGLAREPROGRAMSRESIDENTNVPROC) (GLsizei n, const GLuint* ids, GLboolean *residences);
typedef void (GLAPIENTRY * PFNGLBINDPROGRAMNVPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMSNVPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLEXECUTEPROGRAMNVPROC) (GLenum target, GLuint id, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGENPROGRAMSNVPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPARAMETERDVNVPROC) (GLenum target, GLuint index, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPARAMETERFVNVPROC) (GLenum target, GLuint index, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMSTRINGNVPROC) (GLuint id, GLenum pname, GLubyte* program);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMIVNVPROC) (GLuint id, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETTRACKMATRIXIVNVPROC) (GLenum target, GLuint address, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBPOINTERVNVPROC) (GLuint index, GLenum pname, GLvoid** pointer);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBDVNVPROC) (GLuint index, GLenum pname, GLdouble* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVNVPROC) (GLuint index, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVNVPROC) (GLuint index, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISPROGRAMNVPROC) (GLuint id);
typedef void (GLAPIENTRY * PFNGLLOADPROGRAMNVPROC) (GLenum target, GLuint id, GLsizei len, const GLubyte* program);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETER4DNVPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETER4DVNVPROC) (GLenum target, GLuint index, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETER4FNVPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETER4FVNVPROC) (GLenum target, GLuint index, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERS4DVNVPROC) (GLenum target, GLuint index, GLuint num, const GLdouble* params);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERS4FVNVPROC) (GLenum target, GLuint index, GLuint num, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLREQUESTRESIDENTPROGRAMSNVPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLTRACKMATRIXNVPROC) (GLenum target, GLuint address, GLenum matrix, GLenum transform);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DNVPROC) (GLuint index, GLdouble x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1DVNVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FNVPROC) (GLuint index, GLfloat x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1FVNVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SNVPROC) (GLuint index, GLshort x);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB1SVNVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DNVPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2DVNVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FNVPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2FVNVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SNVPROC) (GLuint index, GLshort x, GLshort y);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB2SVNVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DNVPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3DVNVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FNVPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3FVNVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SNVPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB3SVNVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DNVPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4DVNVPROC) (GLuint index, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FNVPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4FVNVPROC) (GLuint index, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SNVPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4SVNVPROC) (GLuint index, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UBNVPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIB4UBVNVPROC) (GLuint index, const GLubyte* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERNVPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS1DVNVPROC) (GLuint index, GLsizei n, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS1FVNVPROC) (GLuint index, GLsizei n, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS1SVNVPROC) (GLuint index, GLsizei n, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS2DVNVPROC) (GLuint index, GLsizei n, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS2FVNVPROC) (GLuint index, GLsizei n, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS2SVNVPROC) (GLuint index, GLsizei n, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS3DVNVPROC) (GLuint index, GLsizei n, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS3FVNVPROC) (GLuint index, GLsizei n, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS3SVNVPROC) (GLuint index, GLsizei n, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS4DVNVPROC) (GLuint index, GLsizei n, const GLdouble* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS4FVNVPROC) (GLuint index, GLsizei n, const GLfloat* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS4SVNVPROC) (GLuint index, GLsizei n, const GLshort* v);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBS4UBVNVPROC) (GLuint index, GLsizei n, const GLubyte* v);

#define glAreProgramsResidentNV GLEW_GET_FUN(__glewAreProgramsResidentNV)
#define glBindProgramNV GLEW_GET_FUN(__glewBindProgramNV)
#define glDeleteProgramsNV GLEW_GET_FUN(__glewDeleteProgramsNV)
#define glExecuteProgramNV GLEW_GET_FUN(__glewExecuteProgramNV)
#define glGenProgramsNV GLEW_GET_FUN(__glewGenProgramsNV)
#define glGetProgramParameterdvNV GLEW_GET_FUN(__glewGetProgramParameterdvNV)
#define glGetProgramParameterfvNV GLEW_GET_FUN(__glewGetProgramParameterfvNV)
#define glGetProgramStringNV GLEW_GET_FUN(__glewGetProgramStringNV)
#define glGetProgramivNV GLEW_GET_FUN(__glewGetProgramivNV)
#define glGetTrackMatrixivNV GLEW_GET_FUN(__glewGetTrackMatrixivNV)
#define glGetVertexAttribPointervNV GLEW_GET_FUN(__glewGetVertexAttribPointervNV)
#define glGetVertexAttribdvNV GLEW_GET_FUN(__glewGetVertexAttribdvNV)
#define glGetVertexAttribfvNV GLEW_GET_FUN(__glewGetVertexAttribfvNV)
#define glGetVertexAttribivNV GLEW_GET_FUN(__glewGetVertexAttribivNV)
#define glIsProgramNV GLEW_GET_FUN(__glewIsProgramNV)
#define glLoadProgramNV GLEW_GET_FUN(__glewLoadProgramNV)
#define glProgramParameter4dNV GLEW_GET_FUN(__glewProgramParameter4dNV)
#define glProgramParameter4dvNV GLEW_GET_FUN(__glewProgramParameter4dvNV)
#define glProgramParameter4fNV GLEW_GET_FUN(__glewProgramParameter4fNV)
#define glProgramParameter4fvNV GLEW_GET_FUN(__glewProgramParameter4fvNV)
#define glProgramParameters4dvNV GLEW_GET_FUN(__glewProgramParameters4dvNV)
#define glProgramParameters4fvNV GLEW_GET_FUN(__glewProgramParameters4fvNV)
#define glRequestResidentProgramsNV GLEW_GET_FUN(__glewRequestResidentProgramsNV)
#define glTrackMatrixNV GLEW_GET_FUN(__glewTrackMatrixNV)
#define glVertexAttrib1dNV GLEW_GET_FUN(__glewVertexAttrib1dNV)
#define glVertexAttrib1dvNV GLEW_GET_FUN(__glewVertexAttrib1dvNV)
#define glVertexAttrib1fNV GLEW_GET_FUN(__glewVertexAttrib1fNV)
#define glVertexAttrib1fvNV GLEW_GET_FUN(__glewVertexAttrib1fvNV)
#define glVertexAttrib1sNV GLEW_GET_FUN(__glewVertexAttrib1sNV)
#define glVertexAttrib1svNV GLEW_GET_FUN(__glewVertexAttrib1svNV)
#define glVertexAttrib2dNV GLEW_GET_FUN(__glewVertexAttrib2dNV)
#define glVertexAttrib2dvNV GLEW_GET_FUN(__glewVertexAttrib2dvNV)
#define glVertexAttrib2fNV GLEW_GET_FUN(__glewVertexAttrib2fNV)
#define glVertexAttrib2fvNV GLEW_GET_FUN(__glewVertexAttrib2fvNV)
#define glVertexAttrib2sNV GLEW_GET_FUN(__glewVertexAttrib2sNV)
#define glVertexAttrib2svNV GLEW_GET_FUN(__glewVertexAttrib2svNV)
#define glVertexAttrib3dNV GLEW_GET_FUN(__glewVertexAttrib3dNV)
#define glVertexAttrib3dvNV GLEW_GET_FUN(__glewVertexAttrib3dvNV)
#define glVertexAttrib3fNV GLEW_GET_FUN(__glewVertexAttrib3fNV)
#define glVertexAttrib3fvNV GLEW_GET_FUN(__glewVertexAttrib3fvNV)
#define glVertexAttrib3sNV GLEW_GET_FUN(__glewVertexAttrib3sNV)
#define glVertexAttrib3svNV GLEW_GET_FUN(__glewVertexAttrib3svNV)
#define glVertexAttrib4dNV GLEW_GET_FUN(__glewVertexAttrib4dNV)
#define glVertexAttrib4dvNV GLEW_GET_FUN(__glewVertexAttrib4dvNV)
#define glVertexAttrib4fNV GLEW_GET_FUN(__glewVertexAttrib4fNV)
#define glVertexAttrib4fvNV GLEW_GET_FUN(__glewVertexAttrib4fvNV)
#define glVertexAttrib4sNV GLEW_GET_FUN(__glewVertexAttrib4sNV)
#define glVertexAttrib4svNV GLEW_GET_FUN(__glewVertexAttrib4svNV)
#define glVertexAttrib4ubNV GLEW_GET_FUN(__glewVertexAttrib4ubNV)
#define glVertexAttrib4ubvNV GLEW_GET_FUN(__glewVertexAttrib4ubvNV)
#define glVertexAttribPointerNV GLEW_GET_FUN(__glewVertexAttribPointerNV)
#define glVertexAttribs1dvNV GLEW_GET_FUN(__glewVertexAttribs1dvNV)
#define glVertexAttribs1fvNV GLEW_GET_FUN(__glewVertexAttribs1fvNV)
#define glVertexAttribs1svNV GLEW_GET_FUN(__glewVertexAttribs1svNV)
#define glVertexAttribs2dvNV GLEW_GET_FUN(__glewVertexAttribs2dvNV)
#define glVertexAttribs2fvNV GLEW_GET_FUN(__glewVertexAttribs2fvNV)
#define glVertexAttribs2svNV GLEW_GET_FUN(__glewVertexAttribs2svNV)
#define glVertexAttribs3dvNV GLEW_GET_FUN(__glewVertexAttribs3dvNV)
#define glVertexAttribs3fvNV GLEW_GET_FUN(__glewVertexAttribs3fvNV)
#define glVertexAttribs3svNV GLEW_GET_FUN(__glewVertexAttribs3svNV)
#define glVertexAttribs4dvNV GLEW_GET_FUN(__glewVertexAttribs4dvNV)
#define glVertexAttribs4fvNV GLEW_GET_FUN(__glewVertexAttribs4fvNV)
#define glVertexAttribs4svNV GLEW_GET_FUN(__glewVertexAttribs4svNV)
#define glVertexAttribs4ubvNV GLEW_GET_FUN(__glewVertexAttribs4ubvNV)

#define GLEW_NV_vertex_program GLEW_GET_VAR(__GLEW_NV_vertex_program)

#endif /* GL_NV_vertex_program */

/* ------------------------ GL_NV_vertex_program1_1 ------------------------ */

#ifndef GL_NV_vertex_program1_1
#define GL_NV_vertex_program1_1 1

#define GLEW_NV_vertex_program1_1 GLEW_GET_VAR(__GLEW_NV_vertex_program1_1)

#endif /* GL_NV_vertex_program1_1 */

/* ------------------------- GL_NV_vertex_program2 ------------------------- */

#ifndef GL_NV_vertex_program2
#define GL_NV_vertex_program2 1

#define GLEW_NV_vertex_program2 GLEW_GET_VAR(__GLEW_NV_vertex_program2)

#endif /* GL_NV_vertex_program2 */

/* ---------------------- GL_NV_vertex_program2_option --------------------- */

#ifndef GL_NV_vertex_program2_option
#define GL_NV_vertex_program2_option 1

#define GL_MAX_PROGRAM_EXEC_INSTRUCTIONS_NV 0x88F4
#define GL_MAX_PROGRAM_CALL_DEPTH_NV 0x88F5

#define GLEW_NV_vertex_program2_option GLEW_GET_VAR(__GLEW_NV_vertex_program2_option)

#endif /* GL_NV_vertex_program2_option */

/* ------------------------- GL_NV_vertex_program3 ------------------------- */

#ifndef GL_NV_vertex_program3
#define GL_NV_vertex_program3 1

#define MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C

#define GLEW_NV_vertex_program3 GLEW_GET_VAR(__GLEW_NV_vertex_program3)

#endif /* GL_NV_vertex_program3 */

/* ------------------------- GL_NV_vertex_program4 ------------------------- */

#ifndef GL_NV_vertex_program4
#define GL_NV_vertex_program4 1

#define GL_VERTEX_ATTRIB_ARRAY_INTEGER_NV 0x88FD

#define GLEW_NV_vertex_program4 GLEW_GET_VAR(__GLEW_NV_vertex_program4)

#endif /* GL_NV_vertex_program4 */

/* ------------------------ GL_OES_byte_coordinates ------------------------ */

#ifndef GL_OES_byte_coordinates
#define GL_OES_byte_coordinates 1

#define GL_BYTE 0x1400

#define GLEW_OES_byte_coordinates GLEW_GET_VAR(__GLEW_OES_byte_coordinates)

#endif /* GL_OES_byte_coordinates */

/* ------------------- GL_OES_compressed_paletted_texture ------------------ */

#ifndef GL_OES_compressed_paletted_texture
#define GL_OES_compressed_paletted_texture 1

#define GL_PALETTE4_RGB8_OES 0x8B90
#define GL_PALETTE4_RGBA8_OES 0x8B91
#define GL_PALETTE4_R5_G6_B5_OES 0x8B92
#define GL_PALETTE4_RGBA4_OES 0x8B93
#define GL_PALETTE4_RGB5_A1_OES 0x8B94
#define GL_PALETTE8_RGB8_OES 0x8B95
#define GL_PALETTE8_RGBA8_OES 0x8B96
#define GL_PALETTE8_R5_G6_B5_OES 0x8B97
#define GL_PALETTE8_RGBA4_OES 0x8B98
#define GL_PALETTE8_RGB5_A1_OES 0x8B99

#define GLEW_OES_compressed_paletted_texture GLEW_GET_VAR(__GLEW_OES_compressed_paletted_texture)

#endif /* GL_OES_compressed_paletted_texture */

/* --------------------------- GL_OES_read_format -------------------------- */

#ifndef GL_OES_read_format
#define GL_OES_read_format 1

#define GL_IMPLEMENTATION_COLOR_READ_TYPE_OES 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES 0x8B9B

#define GLEW_OES_read_format GLEW_GET_VAR(__GLEW_OES_read_format)

#endif /* GL_OES_read_format */

/* ------------------------ GL_OES_single_precision ------------------------ */

#ifndef GL_OES_single_precision
#define GL_OES_single_precision 1

typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFOESPROC) (GLclampd depth);
typedef void (GLAPIENTRY * PFNGLCLIPPLANEFOESPROC) (GLenum plane, const GLfloat* equation);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFOESPROC) (GLclampf n, GLclampf f);
typedef void (GLAPIENTRY * PFNGLFRUSTUMFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
typedef void (GLAPIENTRY * PFNGLGETCLIPPLANEFOESPROC) (GLenum plane, GLfloat* equation);
typedef void (GLAPIENTRY * PFNGLORTHOFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);

#define glClearDepthfOES GLEW_GET_FUN(__glewClearDepthfOES)
#define glClipPlanefOES GLEW_GET_FUN(__glewClipPlanefOES)
#define glDepthRangefOES GLEW_GET_FUN(__glewDepthRangefOES)
#define glFrustumfOES GLEW_GET_FUN(__glewFrustumfOES)
#define glGetClipPlanefOES GLEW_GET_FUN(__glewGetClipPlanefOES)
#define glOrthofOES GLEW_GET_FUN(__glewOrthofOES)

#define GLEW_OES_single_precision GLEW_GET_VAR(__GLEW_OES_single_precision)

#endif /* GL_OES_single_precision */

/* ---------------------------- GL_OML_interlace --------------------------- */

#ifndef GL_OML_interlace
#define GL_OML_interlace 1

#define GL_INTERLACE_OML 0x8980
#define GL_INTERLACE_READ_OML 0x8981

#define GLEW_OML_interlace GLEW_GET_VAR(__GLEW_OML_interlace)

#endif /* GL_OML_interlace */

/* ---------------------------- GL_OML_resample ---------------------------- */

#ifndef GL_OML_resample
#define GL_OML_resample 1

#define GL_PACK_RESAMPLE_OML 0x8984
#define GL_UNPACK_RESAMPLE_OML 0x8985
#define GL_RESAMPLE_REPLICATE_OML 0x8986
#define GL_RESAMPLE_ZERO_FILL_OML 0x8987
#define GL_RESAMPLE_AVERAGE_OML 0x8988
#define GL_RESAMPLE_DECIMATE_OML 0x8989

#define GLEW_OML_resample GLEW_GET_VAR(__GLEW_OML_resample)

#endif /* GL_OML_resample */

/* ---------------------------- GL_OML_subsample --------------------------- */

#ifndef GL_OML_subsample
#define GL_OML_subsample 1

#define GL_FORMAT_SUBSAMPLE_24_24_OML 0x8982
#define GL_FORMAT_SUBSAMPLE_244_244_OML 0x8983

#define GLEW_OML_subsample GLEW_GET_VAR(__GLEW_OML_subsample)

#endif /* GL_OML_subsample */

/* --------------------------- GL_PGI_misc_hints --------------------------- */

#ifndef GL_PGI_misc_hints
#define GL_PGI_misc_hints 1

#define GL_PREFER_DOUBLEBUFFER_HINT_PGI 107000
#define GL_CONSERVE_MEMORY_HINT_PGI 107005
#define GL_RECLAIM_MEMORY_HINT_PGI 107006
#define GL_NATIVE_GRAPHICS_HANDLE_PGI 107010
#define GL_NATIVE_GRAPHICS_BEGIN_HINT_PGI 107011
#define GL_NATIVE_GRAPHICS_END_HINT_PGI 107012
#define GL_ALWAYS_FAST_HINT_PGI 107020
#define GL_ALWAYS_SOFT_HINT_PGI 107021
#define GL_ALLOW_DRAW_OBJ_HINT_PGI 107022
#define GL_ALLOW_DRAW_WIN_HINT_PGI 107023
#define GL_ALLOW_DRAW_FRG_HINT_PGI 107024
#define GL_ALLOW_DRAW_MEM_HINT_PGI 107025
#define GL_STRICT_DEPTHFUNC_HINT_PGI 107030
#define GL_STRICT_LIGHTING_HINT_PGI 107031
#define GL_STRICT_SCISSOR_HINT_PGI 107032
#define GL_FULL_STIPPLE_HINT_PGI 107033
#define GL_CLIP_NEAR_HINT_PGI 107040
#define GL_CLIP_FAR_HINT_PGI 107041
#define GL_WIDE_LINE_HINT_PGI 107042
#define GL_BACK_NORMALS_HINT_PGI 107043

#define GLEW_PGI_misc_hints GLEW_GET_VAR(__GLEW_PGI_misc_hints)

#endif /* GL_PGI_misc_hints */

/* -------------------------- GL_PGI_vertex_hints -------------------------- */

#ifndef GL_PGI_vertex_hints
#define GL_PGI_vertex_hints 1

#define GL_VERTEX23_BIT_PGI 0x00000004
#define GL_VERTEX4_BIT_PGI 0x00000008
#define GL_COLOR3_BIT_PGI 0x00010000
#define GL_COLOR4_BIT_PGI 0x00020000
#define GL_EDGEFLAG_BIT_PGI 0x00040000
#define GL_INDEX_BIT_PGI 0x00080000
#define GL_MAT_AMBIENT_BIT_PGI 0x00100000
#define GL_VERTEX_DATA_HINT_PGI 107050
#define GL_VERTEX_CONSISTENT_HINT_PGI 107051
#define GL_MATERIAL_SIDE_HINT_PGI 107052
#define GL_MAX_VERTEX_HINT_PGI 107053
#define GL_MAT_AMBIENT_AND_DIFFUSE_BIT_PGI 0x00200000
#define GL_MAT_DIFFUSE_BIT_PGI 0x00400000
#define GL_MAT_EMISSION_BIT_PGI 0x00800000
#define GL_MAT_COLOR_INDEXES_BIT_PGI 0x01000000
#define GL_MAT_SHININESS_BIT_PGI 0x02000000
#define GL_MAT_SPECULAR_BIT_PGI 0x04000000
#define GL_NORMAL_BIT_PGI 0x08000000
#define GL_TEXCOORD1_BIT_PGI 0x10000000
#define GL_TEXCOORD2_BIT_PGI 0x20000000
#define GL_TEXCOORD3_BIT_PGI 0x40000000
#define GL_TEXCOORD4_BIT_PGI 0x80000000

#define GLEW_PGI_vertex_hints GLEW_GET_VAR(__GLEW_PGI_vertex_hints)

#endif /* GL_PGI_vertex_hints */

/* ----------------------- GL_REND_screen_coordinates ---------------------- */

#ifndef GL_REND_screen_coordinates
#define GL_REND_screen_coordinates 1

#define GL_SCREEN_COORDINATES_REND 0x8490
#define GL_INVERTED_SCREEN_W_REND 0x8491

#define GLEW_REND_screen_coordinates GLEW_GET_VAR(__GLEW_REND_screen_coordinates)

#endif /* GL_REND_screen_coordinates */

/* ------------------------------- GL_S3_s3tc ------------------------------ */

#ifndef GL_S3_s3tc
#define GL_S3_s3tc 1

#define GL_RGB_S3TC 0x83A0
#define GL_RGB4_S3TC 0x83A1
#define GL_RGBA_S3TC 0x83A2
#define GL_RGBA4_S3TC 0x83A3
#define GL_RGBA_DXT5_S3TC 0x83A4
#define GL_RGBA4_DXT5_S3TC 0x83A5

#define GLEW_S3_s3tc GLEW_GET_VAR(__GLEW_S3_s3tc)

#endif /* GL_S3_s3tc */

/* -------------------------- GL_SGIS_color_range -------------------------- */

#ifndef GL_SGIS_color_range
#define GL_SGIS_color_range 1

#define GL_EXTENDED_RANGE_SGIS 0x85A5
#define GL_MIN_RED_SGIS 0x85A6
#define GL_MAX_RED_SGIS 0x85A7
#define GL_MIN_GREEN_SGIS 0x85A8
#define GL_MAX_GREEN_SGIS 0x85A9
#define GL_MIN_BLUE_SGIS 0x85AA
#define GL_MAX_BLUE_SGIS 0x85AB
#define GL_MIN_ALPHA_SGIS 0x85AC
#define GL_MAX_ALPHA_SGIS 0x85AD

#define GLEW_SGIS_color_range GLEW_GET_VAR(__GLEW_SGIS_color_range)

#endif /* GL_SGIS_color_range */

/* ------------------------- GL_SGIS_detail_texture ------------------------ */

#ifndef GL_SGIS_detail_texture
#define GL_SGIS_detail_texture 1

typedef void (GLAPIENTRY * PFNGLDETAILTEXFUNCSGISPROC) (GLenum target, GLsizei n, const GLfloat* points);
typedef void (GLAPIENTRY * PFNGLGETDETAILTEXFUNCSGISPROC) (GLenum target, GLfloat* points);

#define glDetailTexFuncSGIS GLEW_GET_FUN(__glewDetailTexFuncSGIS)
#define glGetDetailTexFuncSGIS GLEW_GET_FUN(__glewGetDetailTexFuncSGIS)

#define GLEW_SGIS_detail_texture GLEW_GET_VAR(__GLEW_SGIS_detail_texture)

#endif /* GL_SGIS_detail_texture */

/* -------------------------- GL_SGIS_fog_function ------------------------- */

#ifndef GL_SGIS_fog_function
#define GL_SGIS_fog_function 1

typedef void (GLAPIENTRY * PFNGLFOGFUNCSGISPROC) (GLsizei n, const GLfloat* points);
typedef void (GLAPIENTRY * PFNGLGETFOGFUNCSGISPROC) (GLfloat* points);

#define glFogFuncSGIS GLEW_GET_FUN(__glewFogFuncSGIS)
#define glGetFogFuncSGIS GLEW_GET_FUN(__glewGetFogFuncSGIS)

#define GLEW_SGIS_fog_function GLEW_GET_VAR(__GLEW_SGIS_fog_function)

#endif /* GL_SGIS_fog_function */

/* ------------------------ GL_SGIS_generate_mipmap ------------------------ */

#ifndef GL_SGIS_generate_mipmap
#define GL_SGIS_generate_mipmap 1

#define GL_GENERATE_MIPMAP_SGIS 0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS 0x8192

#define GLEW_SGIS_generate_mipmap GLEW_GET_VAR(__GLEW_SGIS_generate_mipmap)

#endif /* GL_SGIS_generate_mipmap */

/* -------------------------- GL_SGIS_multisample -------------------------- */

#ifndef GL_SGIS_multisample
#define GL_SGIS_multisample 1

#define GL_MULTISAMPLE_SGIS 0x809D
#define GL_SAMPLE_ALPHA_TO_MASK_SGIS 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_SGIS 0x809F
#define GL_SAMPLE_MASK_SGIS 0x80A0
#define GL_1PASS_SGIS 0x80A1
#define GL_2PASS_0_SGIS 0x80A2
#define GL_2PASS_1_SGIS 0x80A3
#define GL_4PASS_0_SGIS 0x80A4
#define GL_4PASS_1_SGIS 0x80A5
#define GL_4PASS_2_SGIS 0x80A6
#define GL_4PASS_3_SGIS 0x80A7
#define GL_SAMPLE_BUFFERS_SGIS 0x80A8
#define GL_SAMPLES_SGIS 0x80A9
#define GL_SAMPLE_MASK_VALUE_SGIS 0x80AA
#define GL_SAMPLE_MASK_INVERT_SGIS 0x80AB
#define GL_SAMPLE_PATTERN_SGIS 0x80AC
#define GL_MULTISAMPLE_BIT_EXT 0x20000000

typedef void (GLAPIENTRY * PFNGLSAMPLEMASKSGISPROC) (GLclampf value, GLboolean invert);
typedef void (GLAPIENTRY * PFNGLSAMPLEPATTERNSGISPROC) (GLenum pattern);

#define glSampleMaskSGIS GLEW_GET_FUN(__glewSampleMaskSGIS)
#define glSamplePatternSGIS GLEW_GET_FUN(__glewSamplePatternSGIS)

#define GLEW_SGIS_multisample GLEW_GET_VAR(__GLEW_SGIS_multisample)

#endif /* GL_SGIS_multisample */

/* ------------------------- GL_SGIS_pixel_texture ------------------------- */

#ifndef GL_SGIS_pixel_texture
#define GL_SGIS_pixel_texture 1

#define GLEW_SGIS_pixel_texture GLEW_GET_VAR(__GLEW_SGIS_pixel_texture)

#endif /* GL_SGIS_pixel_texture */

/* ----------------------- GL_SGIS_point_line_texgen ----------------------- */

#ifndef GL_SGIS_point_line_texgen
#define GL_SGIS_point_line_texgen 1

#define GL_EYE_DISTANCE_TO_POINT_SGIS 0x81F0
#define GL_OBJECT_DISTANCE_TO_POINT_SGIS 0x81F1
#define GL_EYE_DISTANCE_TO_LINE_SGIS 0x81F2
#define GL_OBJECT_DISTANCE_TO_LINE_SGIS 0x81F3
#define GL_EYE_POINT_SGIS 0x81F4
#define GL_OBJECT_POINT_SGIS 0x81F5
#define GL_EYE_LINE_SGIS 0x81F6
#define GL_OBJECT_LINE_SGIS 0x81F7

#define GLEW_SGIS_point_line_texgen GLEW_GET_VAR(__GLEW_SGIS_point_line_texgen)

#endif /* GL_SGIS_point_line_texgen */

/* ------------------------ GL_SGIS_sharpen_texture ------------------------ */

#ifndef GL_SGIS_sharpen_texture
#define GL_SGIS_sharpen_texture 1

typedef void (GLAPIENTRY * PFNGLGETSHARPENTEXFUNCSGISPROC) (GLenum target, GLfloat* points);
typedef void (GLAPIENTRY * PFNGLSHARPENTEXFUNCSGISPROC) (GLenum target, GLsizei n, const GLfloat* points);

#define glGetSharpenTexFuncSGIS GLEW_GET_FUN(__glewGetSharpenTexFuncSGIS)
#define glSharpenTexFuncSGIS GLEW_GET_FUN(__glewSharpenTexFuncSGIS)

#define GLEW_SGIS_sharpen_texture GLEW_GET_VAR(__GLEW_SGIS_sharpen_texture)

#endif /* GL_SGIS_sharpen_texture */

/* --------------------------- GL_SGIS_texture4D --------------------------- */

#ifndef GL_SGIS_texture4D
#define GL_SGIS_texture4D 1

typedef void (GLAPIENTRY * PFNGLTEXIMAGE4DSGISPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLsizei extent, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE4DSGISPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint woffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei extent, GLenum format, GLenum type, const void* pixels);

#define glTexImage4DSGIS GLEW_GET_FUN(__glewTexImage4DSGIS)
#define glTexSubImage4DSGIS GLEW_GET_FUN(__glewTexSubImage4DSGIS)

#define GLEW_SGIS_texture4D GLEW_GET_VAR(__GLEW_SGIS_texture4D)

#endif /* GL_SGIS_texture4D */

/* ---------------------- GL_SGIS_texture_border_clamp --------------------- */

#ifndef GL_SGIS_texture_border_clamp
#define GL_SGIS_texture_border_clamp 1

#define GL_CLAMP_TO_BORDER_SGIS 0x812D

#define GLEW_SGIS_texture_border_clamp GLEW_GET_VAR(__GLEW_SGIS_texture_border_clamp)

#endif /* GL_SGIS_texture_border_clamp */

/* ----------------------- GL_SGIS_texture_edge_clamp ---------------------- */

#ifndef GL_SGIS_texture_edge_clamp
#define GL_SGIS_texture_edge_clamp 1

#define GL_CLAMP_TO_EDGE_SGIS 0x812F

#define GLEW_SGIS_texture_edge_clamp GLEW_GET_VAR(__GLEW_SGIS_texture_edge_clamp)

#endif /* GL_SGIS_texture_edge_clamp */

/* ------------------------ GL_SGIS_texture_filter4 ------------------------ */

#ifndef GL_SGIS_texture_filter4
#define GL_SGIS_texture_filter4 1

typedef void (GLAPIENTRY * PFNGLGETTEXFILTERFUNCSGISPROC) (GLenum target, GLenum filter, GLfloat* weights);
typedef void (GLAPIENTRY * PFNGLTEXFILTERFUNCSGISPROC) (GLenum target, GLenum filter, GLsizei n, const GLfloat* weights);

#define glGetTexFilterFuncSGIS GLEW_GET_FUN(__glewGetTexFilterFuncSGIS)
#define glTexFilterFuncSGIS GLEW_GET_FUN(__glewTexFilterFuncSGIS)

#define GLEW_SGIS_texture_filter4 GLEW_GET_VAR(__GLEW_SGIS_texture_filter4)

#endif /* GL_SGIS_texture_filter4 */

/* -------------------------- GL_SGIS_texture_lod -------------------------- */

#ifndef GL_SGIS_texture_lod
#define GL_SGIS_texture_lod 1

#define GL_TEXTURE_MIN_LOD_SGIS 0x813A
#define GL_TEXTURE_MAX_LOD_SGIS 0x813B
#define GL_TEXTURE_BASE_LEVEL_SGIS 0x813C
#define GL_TEXTURE_MAX_LEVEL_SGIS 0x813D

#define GLEW_SGIS_texture_lod GLEW_GET_VAR(__GLEW_SGIS_texture_lod)

#endif /* GL_SGIS_texture_lod */

/* ------------------------- GL_SGIS_texture_select ------------------------ */

#ifndef GL_SGIS_texture_select
#define GL_SGIS_texture_select 1

#define GLEW_SGIS_texture_select GLEW_GET_VAR(__GLEW_SGIS_texture_select)

#endif /* GL_SGIS_texture_select */

/* ----------------------------- GL_SGIX_async ----------------------------- */

#ifndef GL_SGIX_async
#define GL_SGIX_async 1

#define GL_ASYNC_MARKER_SGIX 0x8329

typedef void (GLAPIENTRY * PFNGLASYNCMARKERSGIXPROC) (GLuint marker);
typedef void (GLAPIENTRY * PFNGLDELETEASYNCMARKERSSGIXPROC) (GLuint marker, GLsizei range);
typedef GLint (GLAPIENTRY * PFNGLFINISHASYNCSGIXPROC) (GLuint* markerp);
typedef GLuint (GLAPIENTRY * PFNGLGENASYNCMARKERSSGIXPROC) (GLsizei range);
typedef GLboolean (GLAPIENTRY * PFNGLISASYNCMARKERSGIXPROC) (GLuint marker);
typedef GLint (GLAPIENTRY * PFNGLPOLLASYNCSGIXPROC) (GLuint* markerp);

#define glAsyncMarkerSGIX GLEW_GET_FUN(__glewAsyncMarkerSGIX)
#define glDeleteAsyncMarkersSGIX GLEW_GET_FUN(__glewDeleteAsyncMarkersSGIX)
#define glFinishAsyncSGIX GLEW_GET_FUN(__glewFinishAsyncSGIX)
#define glGenAsyncMarkersSGIX GLEW_GET_FUN(__glewGenAsyncMarkersSGIX)
#define glIsAsyncMarkerSGIX GLEW_GET_FUN(__glewIsAsyncMarkerSGIX)
#define glPollAsyncSGIX GLEW_GET_FUN(__glewPollAsyncSGIX)

#define GLEW_SGIX_async GLEW_GET_VAR(__GLEW_SGIX_async)

#endif /* GL_SGIX_async */

/* ------------------------ GL_SGIX_async_histogram ------------------------ */

#ifndef GL_SGIX_async_histogram
#define GL_SGIX_async_histogram 1

#define GL_ASYNC_HISTOGRAM_SGIX 0x832C
#define GL_MAX_ASYNC_HISTOGRAM_SGIX 0x832D

#define GLEW_SGIX_async_histogram GLEW_GET_VAR(__GLEW_SGIX_async_histogram)

#endif /* GL_SGIX_async_histogram */

/* -------------------------- GL_SGIX_async_pixel -------------------------- */

#ifndef GL_SGIX_async_pixel
#define GL_SGIX_async_pixel 1

#define GL_ASYNC_TEX_IMAGE_SGIX 0x835C
#define GL_ASYNC_DRAW_PIXELS_SGIX 0x835D
#define GL_ASYNC_READ_PIXELS_SGIX 0x835E
#define GL_MAX_ASYNC_TEX_IMAGE_SGIX 0x835F
#define GL_MAX_ASYNC_DRAW_PIXELS_SGIX 0x8360
#define GL_MAX_ASYNC_READ_PIXELS_SGIX 0x8361

#define GLEW_SGIX_async_pixel GLEW_GET_VAR(__GLEW_SGIX_async_pixel)

#endif /* GL_SGIX_async_pixel */

/* ----------------------- GL_SGIX_blend_alpha_minmax ---------------------- */

#ifndef GL_SGIX_blend_alpha_minmax
#define GL_SGIX_blend_alpha_minmax 1

#define GL_ALPHA_MIN_SGIX 0x8320
#define GL_ALPHA_MAX_SGIX 0x8321

#define GLEW_SGIX_blend_alpha_minmax GLEW_GET_VAR(__GLEW_SGIX_blend_alpha_minmax)

#endif /* GL_SGIX_blend_alpha_minmax */

/* ---------------------------- GL_SGIX_clipmap ---------------------------- */

#ifndef GL_SGIX_clipmap
#define GL_SGIX_clipmap 1

#define GLEW_SGIX_clipmap GLEW_GET_VAR(__GLEW_SGIX_clipmap)

#endif /* GL_SGIX_clipmap */

/* ---------------------- GL_SGIX_convolution_accuracy --------------------- */

#ifndef GL_SGIX_convolution_accuracy
#define GL_SGIX_convolution_accuracy 1

#define GL_CONVOLUTION_HINT_SGIX 0x8316

#define GLEW_SGIX_convolution_accuracy GLEW_GET_VAR(__GLEW_SGIX_convolution_accuracy)

#endif /* GL_SGIX_convolution_accuracy */

/* ------------------------- GL_SGIX_depth_texture ------------------------- */

#ifndef GL_SGIX_depth_texture
#define GL_SGIX_depth_texture 1

#define GL_DEPTH_COMPONENT16_SGIX 0x81A5
#define GL_DEPTH_COMPONENT24_SGIX 0x81A6
#define GL_DEPTH_COMPONENT32_SGIX 0x81A7

#define GLEW_SGIX_depth_texture GLEW_GET_VAR(__GLEW_SGIX_depth_texture)

#endif /* GL_SGIX_depth_texture */

/* -------------------------- GL_SGIX_flush_raster ------------------------- */

#ifndef GL_SGIX_flush_raster
#define GL_SGIX_flush_raster 1

typedef void (GLAPIENTRY * PFNGLFLUSHRASTERSGIXPROC) (void);

#define glFlushRasterSGIX GLEW_GET_FUN(__glewFlushRasterSGIX)

#define GLEW_SGIX_flush_raster GLEW_GET_VAR(__GLEW_SGIX_flush_raster)

#endif /* GL_SGIX_flush_raster */

/* --------------------------- GL_SGIX_fog_offset -------------------------- */

#ifndef GL_SGIX_fog_offset
#define GL_SGIX_fog_offset 1

#define GL_FOG_OFFSET_SGIX 0x8198
#define GL_FOG_OFFSET_VALUE_SGIX 0x8199

#define GLEW_SGIX_fog_offset GLEW_GET_VAR(__GLEW_SGIX_fog_offset)

#endif /* GL_SGIX_fog_offset */

/* -------------------------- GL_SGIX_fog_texture -------------------------- */

#ifndef GL_SGIX_fog_texture
#define GL_SGIX_fog_texture 1

#define GL_TEXTURE_FOG_SGIX 0
#define GL_FOG_PATCHY_FACTOR_SGIX 0
#define GL_FRAGMENT_FOG_SGIX 0

typedef void (GLAPIENTRY * PFNGLTEXTUREFOGSGIXPROC) (GLenum pname);

#define glTextureFogSGIX GLEW_GET_FUN(__glewTextureFogSGIX)

#define GLEW_SGIX_fog_texture GLEW_GET_VAR(__GLEW_SGIX_fog_texture)

#endif /* GL_SGIX_fog_texture */

/* ------------------- GL_SGIX_fragment_specular_lighting ------------------ */

#ifndef GL_SGIX_fragment_specular_lighting
#define GL_SGIX_fragment_specular_lighting 1

typedef void (GLAPIENTRY * PFNGLFRAGMENTCOLORMATERIALSGIXPROC) (GLenum face, GLenum mode);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELFSGIXPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELFVSGIXPROC) (GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELISGIXPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTMODELIVSGIXPROC) (GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTFSGIXPROC) (GLenum light, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTFVSGIXPROC) (GLenum light, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTISGIXPROC) (GLenum light, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTLIGHTIVSGIXPROC) (GLenum light, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALFSGIXPROC) (GLenum face, GLenum pname, const GLfloat param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALFVSGIXPROC) (GLenum face, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALISGIXPROC) (GLenum face, GLenum pname, const GLint param);
typedef void (GLAPIENTRY * PFNGLFRAGMENTMATERIALIVSGIXPROC) (GLenum face, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTLIGHTFVSGIXPROC) (GLenum light, GLenum value, GLfloat* data);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTLIGHTIVSGIXPROC) (GLenum light, GLenum value, GLint* data);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTMATERIALFVSGIXPROC) (GLenum face, GLenum pname, const GLfloat* data);
typedef void (GLAPIENTRY * PFNGLGETFRAGMENTMATERIALIVSGIXPROC) (GLenum face, GLenum pname, const GLint* data);

#define glFragmentColorMaterialSGIX GLEW_GET_FUN(__glewFragmentColorMaterialSGIX)
#define glFragmentLightModelfSGIX GLEW_GET_FUN(__glewFragmentLightModelfSGIX)
#define glFragmentLightModelfvSGIX GLEW_GET_FUN(__glewFragmentLightModelfvSGIX)
#define glFragmentLightModeliSGIX GLEW_GET_FUN(__glewFragmentLightModeliSGIX)
#define glFragmentLightModelivSGIX GLEW_GET_FUN(__glewFragmentLightModelivSGIX)
#define glFragmentLightfSGIX GLEW_GET_FUN(__glewFragmentLightfSGIX)
#define glFragmentLightfvSGIX GLEW_GET_FUN(__glewFragmentLightfvSGIX)
#define glFragmentLightiSGIX GLEW_GET_FUN(__glewFragmentLightiSGIX)
#define glFragmentLightivSGIX GLEW_GET_FUN(__glewFragmentLightivSGIX)
#define glFragmentMaterialfSGIX GLEW_GET_FUN(__glewFragmentMaterialfSGIX)
#define glFragmentMaterialfvSGIX GLEW_GET_FUN(__glewFragmentMaterialfvSGIX)
#define glFragmentMaterialiSGIX GLEW_GET_FUN(__glewFragmentMaterialiSGIX)
#define glFragmentMaterialivSGIX GLEW_GET_FUN(__glewFragmentMaterialivSGIX)
#define glGetFragmentLightfvSGIX GLEW_GET_FUN(__glewGetFragmentLightfvSGIX)
#define glGetFragmentLightivSGIX GLEW_GET_FUN(__glewGetFragmentLightivSGIX)
#define glGetFragmentMaterialfvSGIX GLEW_GET_FUN(__glewGetFragmentMaterialfvSGIX)
#define glGetFragmentMaterialivSGIX GLEW_GET_FUN(__glewGetFragmentMaterialivSGIX)

#define GLEW_SGIX_fragment_specular_lighting GLEW_GET_VAR(__GLEW_SGIX_fragment_specular_lighting)

#endif /* GL_SGIX_fragment_specular_lighting */

/* --------------------------- GL_SGIX_framezoom --------------------------- */

#ifndef GL_SGIX_framezoom
#define GL_SGIX_framezoom 1

typedef void (GLAPIENTRY * PFNGLFRAMEZOOMSGIXPROC) (GLint factor);

#define glFrameZoomSGIX GLEW_GET_FUN(__glewFrameZoomSGIX)

#define GLEW_SGIX_framezoom GLEW_GET_VAR(__GLEW_SGIX_framezoom)

#endif /* GL_SGIX_framezoom */

/* --------------------------- GL_SGIX_interlace --------------------------- */

#ifndef GL_SGIX_interlace
#define GL_SGIX_interlace 1

#define GL_INTERLACE_SGIX 0x8094

#define GLEW_SGIX_interlace GLEW_GET_VAR(__GLEW_SGIX_interlace)

#endif /* GL_SGIX_interlace */

/* ------------------------- GL_SGIX_ir_instrument1 ------------------------ */

#ifndef GL_SGIX_ir_instrument1
#define GL_SGIX_ir_instrument1 1

#define GLEW_SGIX_ir_instrument1 GLEW_GET_VAR(__GLEW_SGIX_ir_instrument1)

#endif /* GL_SGIX_ir_instrument1 */

/* ------------------------- GL_SGIX_list_priority ------------------------- */

#ifndef GL_SGIX_list_priority
#define GL_SGIX_list_priority 1

#define GLEW_SGIX_list_priority GLEW_GET_VAR(__GLEW_SGIX_list_priority)

#endif /* GL_SGIX_list_priority */

/* ------------------------- GL_SGIX_pixel_texture ------------------------- */

#ifndef GL_SGIX_pixel_texture
#define GL_SGIX_pixel_texture 1

typedef void (GLAPIENTRY * PFNGLPIXELTEXGENSGIXPROC) (GLenum mode);

#define glPixelTexGenSGIX GLEW_GET_FUN(__glewPixelTexGenSGIX)

#define GLEW_SGIX_pixel_texture GLEW_GET_VAR(__GLEW_SGIX_pixel_texture)

#endif /* GL_SGIX_pixel_texture */

/* ----------------------- GL_SGIX_pixel_texture_bits ---------------------- */

#ifndef GL_SGIX_pixel_texture_bits
#define GL_SGIX_pixel_texture_bits 1

#define GLEW_SGIX_pixel_texture_bits GLEW_GET_VAR(__GLEW_SGIX_pixel_texture_bits)

#endif /* GL_SGIX_pixel_texture_bits */

/* ------------------------ GL_SGIX_reference_plane ------------------------ */

#ifndef GL_SGIX_reference_plane
#define GL_SGIX_reference_plane 1

typedef void (GLAPIENTRY * PFNGLREFERENCEPLANESGIXPROC) (const GLdouble* equation);

#define glReferencePlaneSGIX GLEW_GET_FUN(__glewReferencePlaneSGIX)

#define GLEW_SGIX_reference_plane GLEW_GET_VAR(__GLEW_SGIX_reference_plane)

#endif /* GL_SGIX_reference_plane */

/* ---------------------------- GL_SGIX_resample --------------------------- */

#ifndef GL_SGIX_resample
#define GL_SGIX_resample 1

#define GL_PACK_RESAMPLE_SGIX 0x842E
#define GL_UNPACK_RESAMPLE_SGIX 0x842F
#define GL_RESAMPLE_DECIMATE_SGIX 0x8430
#define GL_RESAMPLE_REPLICATE_SGIX 0x8433
#define GL_RESAMPLE_ZERO_FILL_SGIX 0x8434

#define GLEW_SGIX_resample GLEW_GET_VAR(__GLEW_SGIX_resample)

#endif /* GL_SGIX_resample */

/* ----------------------------- GL_SGIX_shadow ---------------------------- */

#ifndef GL_SGIX_shadow
#define GL_SGIX_shadow 1

#define GL_TEXTURE_COMPARE_SGIX 0x819A
#define GL_TEXTURE_COMPARE_OPERATOR_SGIX 0x819B
#define GL_TEXTURE_LEQUAL_R_SGIX 0x819C
#define GL_TEXTURE_GEQUAL_R_SGIX 0x819D

#define GLEW_SGIX_shadow GLEW_GET_VAR(__GLEW_SGIX_shadow)

#endif /* GL_SGIX_shadow */

/* ------------------------- GL_SGIX_shadow_ambient ------------------------ */

#ifndef GL_SGIX_shadow_ambient
#define GL_SGIX_shadow_ambient 1

#define GL_SHADOW_AMBIENT_SGIX 0x80BF

#define GLEW_SGIX_shadow_ambient GLEW_GET_VAR(__GLEW_SGIX_shadow_ambient)

#endif /* GL_SGIX_shadow_ambient */

/* ----------------------------- GL_SGIX_sprite ---------------------------- */

#ifndef GL_SGIX_sprite
#define GL_SGIX_sprite 1

typedef void (GLAPIENTRY * PFNGLSPRITEPARAMETERFSGIXPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLSPRITEPARAMETERFVSGIXPROC) (GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLSPRITEPARAMETERISGIXPROC) (GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLSPRITEPARAMETERIVSGIXPROC) (GLenum pname, GLint* params);

#define glSpriteParameterfSGIX GLEW_GET_FUN(__glewSpriteParameterfSGIX)
#define glSpriteParameterfvSGIX GLEW_GET_FUN(__glewSpriteParameterfvSGIX)
#define glSpriteParameteriSGIX GLEW_GET_FUN(__glewSpriteParameteriSGIX)
#define glSpriteParameterivSGIX GLEW_GET_FUN(__glewSpriteParameterivSGIX)

#define GLEW_SGIX_sprite GLEW_GET_VAR(__GLEW_SGIX_sprite)

#endif /* GL_SGIX_sprite */

/* ----------------------- GL_SGIX_tag_sample_buffer ----------------------- */

#ifndef GL_SGIX_tag_sample_buffer
#define GL_SGIX_tag_sample_buffer 1

typedef void (GLAPIENTRY * PFNGLTAGSAMPLEBUFFERSGIXPROC) (void);

#define glTagSampleBufferSGIX GLEW_GET_FUN(__glewTagSampleBufferSGIX)

#define GLEW_SGIX_tag_sample_buffer GLEW_GET_VAR(__GLEW_SGIX_tag_sample_buffer)

#endif /* GL_SGIX_tag_sample_buffer */

/* ------------------------ GL_SGIX_texture_add_env ------------------------ */

#ifndef GL_SGIX_texture_add_env
#define GL_SGIX_texture_add_env 1

#define GLEW_SGIX_texture_add_env GLEW_GET_VAR(__GLEW_SGIX_texture_add_env)

#endif /* GL_SGIX_texture_add_env */

/* -------------------- GL_SGIX_texture_coordinate_clamp ------------------- */

#ifndef GL_SGIX_texture_coordinate_clamp
#define GL_SGIX_texture_coordinate_clamp 1

#define GL_TEXTURE_MAX_CLAMP_S_SGIX 0x8369
#define GL_TEXTURE_MAX_CLAMP_T_SGIX 0x836A
#define GL_TEXTURE_MAX_CLAMP_R_SGIX 0x836B

#define GLEW_SGIX_texture_coordinate_clamp GLEW_GET_VAR(__GLEW_SGIX_texture_coordinate_clamp)

#endif /* GL_SGIX_texture_coordinate_clamp */

/* ------------------------ GL_SGIX_texture_lod_bias ----------------------- */

#ifndef GL_SGIX_texture_lod_bias
#define GL_SGIX_texture_lod_bias 1

#define GLEW_SGIX_texture_lod_bias GLEW_GET_VAR(__GLEW_SGIX_texture_lod_bias)

#endif /* GL_SGIX_texture_lod_bias */

/* ---------------------- GL_SGIX_texture_multi_buffer --------------------- */

#ifndef GL_SGIX_texture_multi_buffer
#define GL_SGIX_texture_multi_buffer 1

#define GL_TEXTURE_MULTI_BUFFER_HINT_SGIX 0x812E

#define GLEW_SGIX_texture_multi_buffer GLEW_GET_VAR(__GLEW_SGIX_texture_multi_buffer)

#endif /* GL_SGIX_texture_multi_buffer */

/* ------------------------- GL_SGIX_texture_range ------------------------- */

#ifndef GL_SGIX_texture_range
#define GL_SGIX_texture_range 1

#define GL_RGB_SIGNED_SGIX 0x85E0
#define GL_RGBA_SIGNED_SGIX 0x85E1
#define GL_ALPHA_SIGNED_SGIX 0x85E2
#define GL_LUMINANCE_SIGNED_SGIX 0x85E3
#define GL_INTENSITY_SIGNED_SGIX 0x85E4
#define GL_LUMINANCE_ALPHA_SIGNED_SGIX 0x85E5
#define GL_RGB16_SIGNED_SGIX 0x85E6
#define GL_RGBA16_SIGNED_SGIX 0x85E7
#define GL_ALPHA16_SIGNED_SGIX 0x85E8
#define GL_LUMINANCE16_SIGNED_SGIX 0x85E9
#define GL_INTENSITY16_SIGNED_SGIX 0x85EA
#define GL_LUMINANCE16_ALPHA16_SIGNED_SGIX 0x85EB
#define GL_RGB_EXTENDED_RANGE_SGIX 0x85EC
#define GL_RGBA_EXTENDED_RANGE_SGIX 0x85ED
#define GL_ALPHA_EXTENDED_RANGE_SGIX 0x85EE
#define GL_LUMINANCE_EXTENDED_RANGE_SGIX 0x85EF
#define GL_INTENSITY_EXTENDED_RANGE_SGIX 0x85F0
#define GL_LUMINANCE_ALPHA_EXTENDED_RANGE_SGIX 0x85F1
#define GL_RGB16_EXTENDED_RANGE_SGIX 0x85F2
#define GL_RGBA16_EXTENDED_RANGE_SGIX 0x85F3
#define GL_ALPHA16_EXTENDED_RANGE_SGIX 0x85F4
#define GL_LUMINANCE16_EXTENDED_RANGE_SGIX 0x85F5
#define GL_INTENSITY16_EXTENDED_RANGE_SGIX 0x85F6
#define GL_LUMINANCE16_ALPHA16_EXTENDED_RANGE_SGIX 0x85F7
#define GL_MIN_LUMINANCE_SGIS 0x85F8
#define GL_MAX_LUMINANCE_SGIS 0x85F9
#define GL_MIN_INTENSITY_SGIS 0x85FA
#define GL_MAX_INTENSITY_SGIS 0x85FB

#define GLEW_SGIX_texture_range GLEW_GET_VAR(__GLEW_SGIX_texture_range)

#endif /* GL_SGIX_texture_range */

/* ----------------------- GL_SGIX_texture_scale_bias ---------------------- */

#ifndef GL_SGIX_texture_scale_bias
#define GL_SGIX_texture_scale_bias 1

#define GL_POST_TEXTURE_FILTER_BIAS_SGIX 0x8179
#define GL_POST_TEXTURE_FILTER_SCALE_SGIX 0x817A
#define GL_POST_TEXTURE_FILTER_BIAS_RANGE_SGIX 0x817B
#define GL_POST_TEXTURE_FILTER_SCALE_RANGE_SGIX 0x817C

#define GLEW_SGIX_texture_scale_bias GLEW_GET_VAR(__GLEW_SGIX_texture_scale_bias)

#endif /* GL_SGIX_texture_scale_bias */

/* ------------------------- GL_SGIX_vertex_preclip ------------------------ */

#ifndef GL_SGIX_vertex_preclip
#define GL_SGIX_vertex_preclip 1

#define GL_VERTEX_PRECLIP_SGIX 0x83EE
#define GL_VERTEX_PRECLIP_HINT_SGIX 0x83EF

#define GLEW_SGIX_vertex_preclip GLEW_GET_VAR(__GLEW_SGIX_vertex_preclip)

#endif /* GL_SGIX_vertex_preclip */

/* ---------------------- GL_SGIX_vertex_preclip_hint ---------------------- */

#ifndef GL_SGIX_vertex_preclip_hint
#define GL_SGIX_vertex_preclip_hint 1

#define GL_VERTEX_PRECLIP_SGIX 0x83EE
#define GL_VERTEX_PRECLIP_HINT_SGIX 0x83EF

#define GLEW_SGIX_vertex_preclip_hint GLEW_GET_VAR(__GLEW_SGIX_vertex_preclip_hint)

#endif /* GL_SGIX_vertex_preclip_hint */

/* ----------------------------- GL_SGIX_ycrcb ----------------------------- */

#ifndef GL_SGIX_ycrcb
#define GL_SGIX_ycrcb 1

#define GLEW_SGIX_ycrcb GLEW_GET_VAR(__GLEW_SGIX_ycrcb)

#endif /* GL_SGIX_ycrcb */

/* -------------------------- GL_SGI_color_matrix -------------------------- */

#ifndef GL_SGI_color_matrix
#define GL_SGI_color_matrix 1

#define GL_COLOR_MATRIX_SGI 0x80B1
#define GL_COLOR_MATRIX_STACK_DEPTH_SGI 0x80B2
#define GL_MAX_COLOR_MATRIX_STACK_DEPTH_SGI 0x80B3
#define GL_POST_COLOR_MATRIX_RED_SCALE_SGI 0x80B4
#define GL_POST_COLOR_MATRIX_GREEN_SCALE_SGI 0x80B5
#define GL_POST_COLOR_MATRIX_BLUE_SCALE_SGI 0x80B6
#define GL_POST_COLOR_MATRIX_ALPHA_SCALE_SGI 0x80B7
#define GL_POST_COLOR_MATRIX_RED_BIAS_SGI 0x80B8
#define GL_POST_COLOR_MATRIX_GREEN_BIAS_SGI 0x80B9
#define GL_POST_COLOR_MATRIX_BLUE_BIAS_SGI 0x80BA
#define GL_POST_COLOR_MATRIX_ALPHA_BIAS_SGI 0x80BB

#define GLEW_SGI_color_matrix GLEW_GET_VAR(__GLEW_SGI_color_matrix)

#endif /* GL_SGI_color_matrix */

/* --------------------------- GL_SGI_color_table -------------------------- */

#ifndef GL_SGI_color_table
#define GL_SGI_color_table 1

#define GL_COLOR_TABLE_SGI 0x80D0
#define GL_POST_CONVOLUTION_COLOR_TABLE_SGI 0x80D1
#define GL_POST_COLOR_MATRIX_COLOR_TABLE_SGI 0x80D2
#define GL_PROXY_COLOR_TABLE_SGI 0x80D3
#define GL_PROXY_POST_CONVOLUTION_COLOR_TABLE_SGI 0x80D4
#define GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE_SGI 0x80D5
#define GL_COLOR_TABLE_SCALE_SGI 0x80D6
#define GL_COLOR_TABLE_BIAS_SGI 0x80D7
#define GL_COLOR_TABLE_FORMAT_SGI 0x80D8
#define GL_COLOR_TABLE_WIDTH_SGI 0x80D9
#define GL_COLOR_TABLE_RED_SIZE_SGI 0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE_SGI 0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE_SGI 0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE_SGI 0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE_SGI 0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE_SGI 0x80DF

typedef void (GLAPIENTRY * PFNGLCOLORTABLEPARAMETERFVSGIPROC) (GLenum target, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLCOLORTABLEPARAMETERIVSGIPROC) (GLenum target, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLCOLORTABLESGIPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const void* table);
typedef void (GLAPIENTRY * PFNGLCOPYCOLORTABLESGIPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERFVSGIPROC) (GLenum target, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLEPARAMETERIVSGIPROC) (GLenum target, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETCOLORTABLESGIPROC) (GLenum target, GLenum format, GLenum type, void* table);

#define glColorTableParameterfvSGI GLEW_GET_FUN(__glewColorTableParameterfvSGI)
#define glColorTableParameterivSGI GLEW_GET_FUN(__glewColorTableParameterivSGI)
#define glColorTableSGI GLEW_GET_FUN(__glewColorTableSGI)
#define glCopyColorTableSGI GLEW_GET_FUN(__glewCopyColorTableSGI)
#define glGetColorTableParameterfvSGI GLEW_GET_FUN(__glewGetColorTableParameterfvSGI)
#define glGetColorTableParameterivSGI GLEW_GET_FUN(__glewGetColorTableParameterivSGI)
#define glGetColorTableSGI GLEW_GET_FUN(__glewGetColorTableSGI)

#define GLEW_SGI_color_table GLEW_GET_VAR(__GLEW_SGI_color_table)

#endif /* GL_SGI_color_table */

/* ----------------------- GL_SGI_texture_color_table ---------------------- */

#ifndef GL_SGI_texture_color_table
#define GL_SGI_texture_color_table 1

#define GL_TEXTURE_COLOR_TABLE_SGI 0x80BC
#define GL_PROXY_TEXTURE_COLOR_TABLE_SGI 0x80BD

#define GLEW_SGI_texture_color_table GLEW_GET_VAR(__GLEW_SGI_texture_color_table)

#endif /* GL_SGI_texture_color_table */

/* ------------------------- GL_SUNX_constant_data ------------------------- */

#ifndef GL_SUNX_constant_data
#define GL_SUNX_constant_data 1

#define GL_UNPACK_CONSTANT_DATA_SUNX 0x81D5
#define GL_TEXTURE_CONSTANT_DATA_SUNX 0x81D6

typedef void (GLAPIENTRY * PFNGLFINISHTEXTURESUNXPROC) (void);

#define glFinishTextureSUNX GLEW_GET_FUN(__glewFinishTextureSUNX)

#define GLEW_SUNX_constant_data GLEW_GET_VAR(__GLEW_SUNX_constant_data)

#endif /* GL_SUNX_constant_data */

/* -------------------- GL_SUN_convolution_border_modes -------------------- */

#ifndef GL_SUN_convolution_border_modes
#define GL_SUN_convolution_border_modes 1

#define GL_WRAP_BORDER_SUN 0x81D4

#define GLEW_SUN_convolution_border_modes GLEW_GET_VAR(__GLEW_SUN_convolution_border_modes)

#endif /* GL_SUN_convolution_border_modes */

/* -------------------------- GL_SUN_global_alpha -------------------------- */

#ifndef GL_SUN_global_alpha
#define GL_SUN_global_alpha 1

#define GL_GLOBAL_ALPHA_SUN 0x81D9
#define GL_GLOBAL_ALPHA_FACTOR_SUN 0x81DA

typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORBSUNPROC) (GLbyte factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORDSUNPROC) (GLdouble factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORFSUNPROC) (GLfloat factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORISUNPROC) (GLint factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORSSUNPROC) (GLshort factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORUBSUNPROC) (GLubyte factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORUISUNPROC) (GLuint factor);
typedef void (GLAPIENTRY * PFNGLGLOBALALPHAFACTORUSSUNPROC) (GLushort factor);

#define glGlobalAlphaFactorbSUN GLEW_GET_FUN(__glewGlobalAlphaFactorbSUN)
#define glGlobalAlphaFactordSUN GLEW_GET_FUN(__glewGlobalAlphaFactordSUN)
#define glGlobalAlphaFactorfSUN GLEW_GET_FUN(__glewGlobalAlphaFactorfSUN)
#define glGlobalAlphaFactoriSUN GLEW_GET_FUN(__glewGlobalAlphaFactoriSUN)
#define glGlobalAlphaFactorsSUN GLEW_GET_FUN(__glewGlobalAlphaFactorsSUN)
#define glGlobalAlphaFactorubSUN GLEW_GET_FUN(__glewGlobalAlphaFactorubSUN)
#define glGlobalAlphaFactoruiSUN GLEW_GET_FUN(__glewGlobalAlphaFactoruiSUN)
#define glGlobalAlphaFactorusSUN GLEW_GET_FUN(__glewGlobalAlphaFactorusSUN)

#define GLEW_SUN_global_alpha GLEW_GET_VAR(__GLEW_SUN_global_alpha)

#endif /* GL_SUN_global_alpha */

/* --------------------------- GL_SUN_mesh_array --------------------------- */

#ifndef GL_SUN_mesh_array
#define GL_SUN_mesh_array 1

#define GL_QUAD_MESH_SUN 0x8614
#define GL_TRIANGLE_MESH_SUN 0x8615

#define GLEW_SUN_mesh_array GLEW_GET_VAR(__GLEW_SUN_mesh_array)

#endif /* GL_SUN_mesh_array */

/* ------------------------ GL_SUN_read_video_pixels ----------------------- */

#ifndef GL_SUN_read_video_pixels
#define GL_SUN_read_video_pixels 1

typedef void (GLAPIENTRY * PFNGLREADVIDEOPIXELSSUNPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels);

#define glReadVideoPixelsSUN GLEW_GET_FUN(__glewReadVideoPixelsSUN)

#define GLEW_SUN_read_video_pixels GLEW_GET_VAR(__GLEW_SUN_read_video_pixels)

#endif /* GL_SUN_read_video_pixels */

/* --------------------------- GL_SUN_slice_accum -------------------------- */

#ifndef GL_SUN_slice_accum
#define GL_SUN_slice_accum 1

#define GL_SLICE_ACCUM_SUN 0x85CC

#define GLEW_SUN_slice_accum GLEW_GET_VAR(__GLEW_SUN_slice_accum)

#endif /* GL_SUN_slice_accum */

/* -------------------------- GL_SUN_triangle_list ------------------------- */

#ifndef GL_SUN_triangle_list
#define GL_SUN_triangle_list 1

#define GL_RESTART_SUN 0x01
#define GL_REPLACE_MIDDLE_SUN 0x02
#define GL_REPLACE_OLDEST_SUN 0x03
#define GL_TRIANGLE_LIST_SUN 0x81D7
#define GL_REPLACEMENT_CODE_SUN 0x81D8
#define GL_REPLACEMENT_CODE_ARRAY_SUN 0x85C0
#define GL_REPLACEMENT_CODE_ARRAY_TYPE_SUN 0x85C1
#define GL_REPLACEMENT_CODE_ARRAY_STRIDE_SUN 0x85C2
#define GL_REPLACEMENT_CODE_ARRAY_POINTER_SUN 0x85C3
#define GL_R1UI_V3F_SUN 0x85C4
#define GL_R1UI_C4UB_V3F_SUN 0x85C5
#define GL_R1UI_C3F_V3F_SUN 0x85C6
#define GL_R1UI_N3F_V3F_SUN 0x85C7
#define GL_R1UI_C4F_N3F_V3F_SUN 0x85C8
#define GL_R1UI_T2F_V3F_SUN 0x85C9
#define GL_R1UI_T2F_N3F_V3F_SUN 0x85CA
#define GL_R1UI_T2F_C4F_N3F_V3F_SUN 0x85CB

typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEPOINTERSUNPROC) (GLenum type, GLsizei stride, const void* pointer);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUBSUNPROC) (GLubyte code);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUBVSUNPROC) (const GLubyte* code);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUISUNPROC) (GLuint code);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUIVSUNPROC) (const GLuint* code);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUSSUNPROC) (GLushort code);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUSVSUNPROC) (const GLushort* code);

#define glReplacementCodePointerSUN GLEW_GET_FUN(__glewReplacementCodePointerSUN)
#define glReplacementCodeubSUN GLEW_GET_FUN(__glewReplacementCodeubSUN)
#define glReplacementCodeubvSUN GLEW_GET_FUN(__glewReplacementCodeubvSUN)
#define glReplacementCodeuiSUN GLEW_GET_FUN(__glewReplacementCodeuiSUN)
#define glReplacementCodeuivSUN GLEW_GET_FUN(__glewReplacementCodeuivSUN)
#define glReplacementCodeusSUN GLEW_GET_FUN(__glewReplacementCodeusSUN)
#define glReplacementCodeusvSUN GLEW_GET_FUN(__glewReplacementCodeusvSUN)

#define GLEW_SUN_triangle_list GLEW_GET_VAR(__GLEW_SUN_triangle_list)

#endif /* GL_SUN_triangle_list */

/* ----------------------------- GL_SUN_vertex ----------------------------- */

#ifndef GL_SUN_vertex
#define GL_SUN_vertex 1

typedef void (GLAPIENTRY * PFNGLCOLOR3FVERTEX3FSUNPROC) (GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLCOLOR3FVERTEX3FVSUNPROC) (const GLfloat* c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (const GLfloat* c, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLCOLOR4UBVERTEX2FSUNPROC) (GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLCOLOR4UBVERTEX2FVSUNPROC) (const GLubyte* c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLCOLOR4UBVERTEX3FSUNPROC) (GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLCOLOR4UBVERTEX3FVSUNPROC) (const GLubyte* c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLNORMAL3FVERTEX3FSUNPROC) (GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLNORMAL3FVERTEX3FVSUNPROC) (const GLfloat* n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC) (GLuint rc, GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *c, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC) (GLuint rc, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC) (const GLuint* rc, const GLubyte *c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *tc, const GLfloat *c, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *tc, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *tc, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC) (GLuint rc, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC) (const GLuint* rc, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC) (const GLfloat* tc, const GLfloat *c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (const GLfloat* tc, const GLfloat *c, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC) (const GLfloat* tc, const GLubyte *c, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC) (const GLfloat* tc, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLTEXCOORD2FVERTEX3FVSUNPROC) (const GLfloat* tc, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC) (GLfloat s, GLfloat t, GLfloat p, GLfloat q, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC) (const GLfloat* tc, const GLfloat *c, const GLfloat *n, const GLfloat *v);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FVERTEX4FSUNPROC) (GLfloat s, GLfloat t, GLfloat p, GLfloat q, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLTEXCOORD4FVERTEX4FVSUNPROC) (const GLfloat* tc, const GLfloat *v);

#define glColor3fVertex3fSUN GLEW_GET_FUN(__glewColor3fVertex3fSUN)
#define glColor3fVertex3fvSUN GLEW_GET_FUN(__glewColor3fVertex3fvSUN)
#define glColor4fNormal3fVertex3fSUN GLEW_GET_FUN(__glewColor4fNormal3fVertex3fSUN)
#define glColor4fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewColor4fNormal3fVertex3fvSUN)
#define glColor4ubVertex2fSUN GLEW_GET_FUN(__glewColor4ubVertex2fSUN)
#define glColor4ubVertex2fvSUN GLEW_GET_FUN(__glewColor4ubVertex2fvSUN)
#define glColor4ubVertex3fSUN GLEW_GET_FUN(__glewColor4ubVertex3fSUN)
#define glColor4ubVertex3fvSUN GLEW_GET_FUN(__glewColor4ubVertex3fvSUN)
#define glNormal3fVertex3fSUN GLEW_GET_FUN(__glewNormal3fVertex3fSUN)
#define glNormal3fVertex3fvSUN GLEW_GET_FUN(__glewNormal3fVertex3fvSUN)
#define glReplacementCodeuiColor3fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiColor3fVertex3fSUN)
#define glReplacementCodeuiColor3fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiColor3fVertex3fvSUN)
#define glReplacementCodeuiColor4fNormal3fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiColor4fNormal3fVertex3fSUN)
#define glReplacementCodeuiColor4fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiColor4fNormal3fVertex3fvSUN)
#define glReplacementCodeuiColor4ubVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiColor4ubVertex3fSUN)
#define glReplacementCodeuiColor4ubVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiColor4ubVertex3fvSUN)
#define glReplacementCodeuiNormal3fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiNormal3fVertex3fSUN)
#define glReplacementCodeuiNormal3fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiNormal3fVertex3fvSUN)
#define glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN)
#define glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN)
#define glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fNormal3fVertex3fSUN)
#define glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN)
#define glReplacementCodeuiTexCoord2fVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fVertex3fSUN)
#define glReplacementCodeuiTexCoord2fVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiTexCoord2fVertex3fvSUN)
#define glReplacementCodeuiVertex3fSUN GLEW_GET_FUN(__glewReplacementCodeuiVertex3fSUN)
#define glReplacementCodeuiVertex3fvSUN GLEW_GET_FUN(__glewReplacementCodeuiVertex3fvSUN)
#define glTexCoord2fColor3fVertex3fSUN GLEW_GET_FUN(__glewTexCoord2fColor3fVertex3fSUN)
#define glTexCoord2fColor3fVertex3fvSUN GLEW_GET_FUN(__glewTexCoord2fColor3fVertex3fvSUN)
#define glTexCoord2fColor4fNormal3fVertex3fSUN GLEW_GET_FUN(__glewTexCoord2fColor4fNormal3fVertex3fSUN)
#define glTexCoord2fColor4fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewTexCoord2fColor4fNormal3fVertex3fvSUN)
#define glTexCoord2fColor4ubVertex3fSUN GLEW_GET_FUN(__glewTexCoord2fColor4ubVertex3fSUN)
#define glTexCoord2fColor4ubVertex3fvSUN GLEW_GET_FUN(__glewTexCoord2fColor4ubVertex3fvSUN)
#define glTexCoord2fNormal3fVertex3fSUN GLEW_GET_FUN(__glewTexCoord2fNormal3fVertex3fSUN)
#define glTexCoord2fNormal3fVertex3fvSUN GLEW_GET_FUN(__glewTexCoord2fNormal3fVertex3fvSUN)
#define glTexCoord2fVertex3fSUN GLEW_GET_FUN(__glewTexCoord2fVertex3fSUN)
#define glTexCoord2fVertex3fvSUN GLEW_GET_FUN(__glewTexCoord2fVertex3fvSUN)
#define glTexCoord4fColor4fNormal3fVertex4fSUN GLEW_GET_FUN(__glewTexCoord4fColor4fNormal3fVertex4fSUN)
#define glTexCoord4fColor4fNormal3fVertex4fvSUN GLEW_GET_FUN(__glewTexCoord4fColor4fNormal3fVertex4fvSUN)
#define glTexCoord4fVertex4fSUN GLEW_GET_FUN(__glewTexCoord4fVertex4fSUN)
#define glTexCoord4fVertex4fvSUN GLEW_GET_FUN(__glewTexCoord4fVertex4fvSUN)

#define GLEW_SUN_vertex GLEW_GET_VAR(__GLEW_SUN_vertex)

#endif /* GL_SUN_vertex */

/* -------------------------- GL_WIN_phong_shading ------------------------- */

#ifndef GL_WIN_phong_shading
#define GL_WIN_phong_shading 1

#define GL_PHONG_WIN 0x80EA
#define GL_PHONG_HINT_WIN 0x80EB

#define GLEW_WIN_phong_shading GLEW_GET_VAR(__GLEW_WIN_phong_shading)

#endif /* GL_WIN_phong_shading */

/* -------------------------- GL_WIN_specular_fog -------------------------- */

#ifndef GL_WIN_specular_fog
#define GL_WIN_specular_fog 1

#define GL_FOG_SPECULAR_TEXTURE_WIN 0x80EC

#define GLEW_WIN_specular_fog GLEW_GET_VAR(__GLEW_WIN_specular_fog)

#endif /* GL_WIN_specular_fog */

/* ---------------------------- GL_WIN_swap_hint --------------------------- */

#ifndef GL_WIN_swap_hint
#define GL_WIN_swap_hint 1

typedef void (GLAPIENTRY * PFNGLADDSWAPHINTRECTWINPROC) (GLint x, GLint y, GLsizei width, GLsizei height);

#define glAddSwapHintRectWIN GLEW_GET_FUN(__glewAddSwapHintRectWIN)

#define GLEW_WIN_swap_hint GLEW_GET_VAR(__GLEW_WIN_swap_hint)

#endif /* GL_WIN_swap_hint */

/* ------------------------------------------------------------------------- */

#if defined(GLEW_MX) && defined(_WIN32)
#define GLEW_FUN_EXPORT
#else
#define GLEW_FUN_EXPORT GLEWAPI
#endif /* GLEW_MX */

#if defined(GLEW_MX)
#define GLEW_VAR_EXPORT
#else
#define GLEW_VAR_EXPORT GLEWAPI
#endif /* GLEW_MX */

#if defined(GLEW_MX) && defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */

GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE3DPROC __glewCopyTexSubImage3D;
GLEW_FUN_EXPORT PFNGLDRAWRANGEELEMENTSPROC __glewDrawRangeElements;
GLEW_FUN_EXPORT PFNGLTEXIMAGE3DPROC __glewTexImage3D;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE3DPROC __glewTexSubImage3D;

GLEW_FUN_EXPORT PFNGLACTIVETEXTUREPROC __glewActiveTexture;
GLEW_FUN_EXPORT PFNGLCLIENTACTIVETEXTUREPROC __glewClientActiveTexture;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE1DPROC __glewCompressedTexImage1D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE2DPROC __glewCompressedTexImage2D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE3DPROC __glewCompressedTexImage3D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC __glewCompressedTexSubImage1D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC __glewCompressedTexSubImage2D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC __glewCompressedTexSubImage3D;
GLEW_FUN_EXPORT PFNGLGETCOMPRESSEDTEXIMAGEPROC __glewGetCompressedTexImage;
GLEW_FUN_EXPORT PFNGLLOADTRANSPOSEMATRIXDPROC __glewLoadTransposeMatrixd;
GLEW_FUN_EXPORT PFNGLLOADTRANSPOSEMATRIXFPROC __glewLoadTransposeMatrixf;
GLEW_FUN_EXPORT PFNGLMULTTRANSPOSEMATRIXDPROC __glewMultTransposeMatrixd;
GLEW_FUN_EXPORT PFNGLMULTTRANSPOSEMATRIXFPROC __glewMultTransposeMatrixf;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1DPROC __glewMultiTexCoord1d;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1DVPROC __glewMultiTexCoord1dv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1FPROC __glewMultiTexCoord1f;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1FVPROC __glewMultiTexCoord1fv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1IPROC __glewMultiTexCoord1i;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1IVPROC __glewMultiTexCoord1iv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1SPROC __glewMultiTexCoord1s;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1SVPROC __glewMultiTexCoord1sv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2DPROC __glewMultiTexCoord2d;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2DVPROC __glewMultiTexCoord2dv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2FPROC __glewMultiTexCoord2f;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2FVPROC __glewMultiTexCoord2fv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2IPROC __glewMultiTexCoord2i;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2IVPROC __glewMultiTexCoord2iv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2SPROC __glewMultiTexCoord2s;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2SVPROC __glewMultiTexCoord2sv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3DPROC __glewMultiTexCoord3d;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3DVPROC __glewMultiTexCoord3dv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3FPROC __glewMultiTexCoord3f;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3FVPROC __glewMultiTexCoord3fv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3IPROC __glewMultiTexCoord3i;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3IVPROC __glewMultiTexCoord3iv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3SPROC __glewMultiTexCoord3s;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3SVPROC __glewMultiTexCoord3sv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4DPROC __glewMultiTexCoord4d;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4DVPROC __glewMultiTexCoord4dv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4FPROC __glewMultiTexCoord4f;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4FVPROC __glewMultiTexCoord4fv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4IPROC __glewMultiTexCoord4i;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4IVPROC __glewMultiTexCoord4iv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4SPROC __glewMultiTexCoord4s;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4SVPROC __glewMultiTexCoord4sv;
GLEW_FUN_EXPORT PFNGLSAMPLECOVERAGEPROC __glewSampleCoverage;

GLEW_FUN_EXPORT PFNGLBLENDCOLORPROC __glewBlendColor;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONPROC __glewBlendEquation;
GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEPROC __glewBlendFuncSeparate;
GLEW_FUN_EXPORT PFNGLFOGCOORDPOINTERPROC __glewFogCoordPointer;
GLEW_FUN_EXPORT PFNGLFOGCOORDDPROC __glewFogCoordd;
GLEW_FUN_EXPORT PFNGLFOGCOORDDVPROC __glewFogCoorddv;
GLEW_FUN_EXPORT PFNGLFOGCOORDFPROC __glewFogCoordf;
GLEW_FUN_EXPORT PFNGLFOGCOORDFVPROC __glewFogCoordfv;
GLEW_FUN_EXPORT PFNGLMULTIDRAWARRAYSPROC __glewMultiDrawArrays;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTSPROC __glewMultiDrawElements;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFPROC __glewPointParameterf;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFVPROC __glewPointParameterfv;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERIPROC __glewPointParameteri;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERIVPROC __glewPointParameteriv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3BPROC __glewSecondaryColor3b;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3BVPROC __glewSecondaryColor3bv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3DPROC __glewSecondaryColor3d;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3DVPROC __glewSecondaryColor3dv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3FPROC __glewSecondaryColor3f;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3FVPROC __glewSecondaryColor3fv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3IPROC __glewSecondaryColor3i;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3IVPROC __glewSecondaryColor3iv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3SPROC __glewSecondaryColor3s;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3SVPROC __glewSecondaryColor3sv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UBPROC __glewSecondaryColor3ub;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UBVPROC __glewSecondaryColor3ubv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UIPROC __glewSecondaryColor3ui;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UIVPROC __glewSecondaryColor3uiv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3USPROC __glewSecondaryColor3us;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3USVPROC __glewSecondaryColor3usv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORPOINTERPROC __glewSecondaryColorPointer;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2DPROC __glewWindowPos2d;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2DVPROC __glewWindowPos2dv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FPROC __glewWindowPos2f;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FVPROC __glewWindowPos2fv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IPROC __glewWindowPos2i;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IVPROC __glewWindowPos2iv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SPROC __glewWindowPos2s;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SVPROC __glewWindowPos2sv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DPROC __glewWindowPos3d;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DVPROC __glewWindowPos3dv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FPROC __glewWindowPos3f;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FVPROC __glewWindowPos3fv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IPROC __glewWindowPos3i;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IVPROC __glewWindowPos3iv;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SPROC __glewWindowPos3s;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SVPROC __glewWindowPos3sv;

GLEW_FUN_EXPORT PFNGLBEGINQUERYPROC __glewBeginQuery;
GLEW_FUN_EXPORT PFNGLBINDBUFFERPROC __glewBindBuffer;
GLEW_FUN_EXPORT PFNGLBUFFERDATAPROC __glewBufferData;
GLEW_FUN_EXPORT PFNGLBUFFERSUBDATAPROC __glewBufferSubData;
GLEW_FUN_EXPORT PFNGLDELETEBUFFERSPROC __glewDeleteBuffers;
GLEW_FUN_EXPORT PFNGLDELETEQUERIESPROC __glewDeleteQueries;
GLEW_FUN_EXPORT PFNGLENDQUERYPROC __glewEndQuery;
GLEW_FUN_EXPORT PFNGLGENBUFFERSPROC __glewGenBuffers;
GLEW_FUN_EXPORT PFNGLGENQUERIESPROC __glewGenQueries;
GLEW_FUN_EXPORT PFNGLGETBUFFERPARAMETERIVPROC __glewGetBufferParameteriv;
GLEW_FUN_EXPORT PFNGLGETBUFFERPOINTERVPROC __glewGetBufferPointerv;
GLEW_FUN_EXPORT PFNGLGETBUFFERSUBDATAPROC __glewGetBufferSubData;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTIVPROC __glewGetQueryObjectiv;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTUIVPROC __glewGetQueryObjectuiv;
GLEW_FUN_EXPORT PFNGLGETQUERYIVPROC __glewGetQueryiv;
GLEW_FUN_EXPORT PFNGLISBUFFERPROC __glewIsBuffer;
GLEW_FUN_EXPORT PFNGLISQUERYPROC __glewIsQuery;
GLEW_FUN_EXPORT PFNGLMAPBUFFERPROC __glewMapBuffer;
GLEW_FUN_EXPORT PFNGLUNMAPBUFFERPROC __glewUnmapBuffer;

GLEW_FUN_EXPORT PFNGLATTACHSHADERPROC __glewAttachShader;
GLEW_FUN_EXPORT PFNGLBINDATTRIBLOCATIONPROC __glewBindAttribLocation;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEPROC __glewBlendEquationSeparate;
GLEW_FUN_EXPORT PFNGLCOMPILESHADERPROC __glewCompileShader;
GLEW_FUN_EXPORT PFNGLCREATEPROGRAMPROC __glewCreateProgram;
GLEW_FUN_EXPORT PFNGLCREATESHADERPROC __glewCreateShader;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMPROC __glewDeleteProgram;
GLEW_FUN_EXPORT PFNGLDELETESHADERPROC __glewDeleteShader;
GLEW_FUN_EXPORT PFNGLDETACHSHADERPROC __glewDetachShader;
GLEW_FUN_EXPORT PFNGLDISABLEVERTEXATTRIBARRAYPROC __glewDisableVertexAttribArray;
GLEW_FUN_EXPORT PFNGLDRAWBUFFERSPROC __glewDrawBuffers;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXATTRIBARRAYPROC __glewEnableVertexAttribArray;
GLEW_FUN_EXPORT PFNGLGETACTIVEATTRIBPROC __glewGetActiveAttrib;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMPROC __glewGetActiveUniform;
GLEW_FUN_EXPORT PFNGLGETATTACHEDSHADERSPROC __glewGetAttachedShaders;
GLEW_FUN_EXPORT PFNGLGETATTRIBLOCATIONPROC __glewGetAttribLocation;
GLEW_FUN_EXPORT PFNGLGETPROGRAMINFOLOGPROC __glewGetProgramInfoLog;
GLEW_FUN_EXPORT PFNGLGETPROGRAMIVPROC __glewGetProgramiv;
GLEW_FUN_EXPORT PFNGLGETSHADERINFOLOGPROC __glewGetShaderInfoLog;
GLEW_FUN_EXPORT PFNGLGETSHADERSOURCEPROC __glewGetShaderSource;
GLEW_FUN_EXPORT PFNGLGETSHADERIVPROC __glewGetShaderiv;
GLEW_FUN_EXPORT PFNGLGETUNIFORMLOCATIONPROC __glewGetUniformLocation;
GLEW_FUN_EXPORT PFNGLGETUNIFORMFVPROC __glewGetUniformfv;
GLEW_FUN_EXPORT PFNGLGETUNIFORMIVPROC __glewGetUniformiv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBPOINTERVPROC __glewGetVertexAttribPointerv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBDVPROC __glewGetVertexAttribdv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBFVPROC __glewGetVertexAttribfv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIVPROC __glewGetVertexAttribiv;
GLEW_FUN_EXPORT PFNGLISPROGRAMPROC __glewIsProgram;
GLEW_FUN_EXPORT PFNGLISSHADERPROC __glewIsShader;
GLEW_FUN_EXPORT PFNGLLINKPROGRAMPROC __glewLinkProgram;
GLEW_FUN_EXPORT PFNGLSHADERSOURCEPROC __glewShaderSource;
GLEW_FUN_EXPORT PFNGLSTENCILFUNCSEPARATEPROC __glewStencilFuncSeparate;
GLEW_FUN_EXPORT PFNGLSTENCILMASKSEPARATEPROC __glewStencilMaskSeparate;
GLEW_FUN_EXPORT PFNGLSTENCILOPSEPARATEPROC __glewStencilOpSeparate;
GLEW_FUN_EXPORT PFNGLUNIFORM1FPROC __glewUniform1f;
GLEW_FUN_EXPORT PFNGLUNIFORM1FVPROC __glewUniform1fv;
GLEW_FUN_EXPORT PFNGLUNIFORM1IPROC __glewUniform1i;
GLEW_FUN_EXPORT PFNGLUNIFORM1IVPROC __glewUniform1iv;
GLEW_FUN_EXPORT PFNGLUNIFORM2FPROC __glewUniform2f;
GLEW_FUN_EXPORT PFNGLUNIFORM2FVPROC __glewUniform2fv;
GLEW_FUN_EXPORT PFNGLUNIFORM2IPROC __glewUniform2i;
GLEW_FUN_EXPORT PFNGLUNIFORM2IVPROC __glewUniform2iv;
GLEW_FUN_EXPORT PFNGLUNIFORM3FPROC __glewUniform3f;
GLEW_FUN_EXPORT PFNGLUNIFORM3FVPROC __glewUniform3fv;
GLEW_FUN_EXPORT PFNGLUNIFORM3IPROC __glewUniform3i;
GLEW_FUN_EXPORT PFNGLUNIFORM3IVPROC __glewUniform3iv;
GLEW_FUN_EXPORT PFNGLUNIFORM4FPROC __glewUniform4f;
GLEW_FUN_EXPORT PFNGLUNIFORM4FVPROC __glewUniform4fv;
GLEW_FUN_EXPORT PFNGLUNIFORM4IPROC __glewUniform4i;
GLEW_FUN_EXPORT PFNGLUNIFORM4IVPROC __glewUniform4iv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2FVPROC __glewUniformMatrix2fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3FVPROC __glewUniformMatrix3fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4FVPROC __glewUniformMatrix4fv;
GLEW_FUN_EXPORT PFNGLUSEPROGRAMPROC __glewUseProgram;
GLEW_FUN_EXPORT PFNGLVALIDATEPROGRAMPROC __glewValidateProgram;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DPROC __glewVertexAttrib1d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DVPROC __glewVertexAttrib1dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FPROC __glewVertexAttrib1f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FVPROC __glewVertexAttrib1fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SPROC __glewVertexAttrib1s;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SVPROC __glewVertexAttrib1sv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DPROC __glewVertexAttrib2d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DVPROC __glewVertexAttrib2dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FPROC __glewVertexAttrib2f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FVPROC __glewVertexAttrib2fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SPROC __glewVertexAttrib2s;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SVPROC __glewVertexAttrib2sv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DPROC __glewVertexAttrib3d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DVPROC __glewVertexAttrib3dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FPROC __glewVertexAttrib3f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FVPROC __glewVertexAttrib3fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SPROC __glewVertexAttrib3s;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SVPROC __glewVertexAttrib3sv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NBVPROC __glewVertexAttrib4Nbv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NIVPROC __glewVertexAttrib4Niv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NSVPROC __glewVertexAttrib4Nsv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUBPROC __glewVertexAttrib4Nub;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUBVPROC __glewVertexAttrib4Nubv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUIVPROC __glewVertexAttrib4Nuiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUSVPROC __glewVertexAttrib4Nusv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4BVPROC __glewVertexAttrib4bv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DPROC __glewVertexAttrib4d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DVPROC __glewVertexAttrib4dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FPROC __glewVertexAttrib4f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FVPROC __glewVertexAttrib4fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4IVPROC __glewVertexAttrib4iv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SPROC __glewVertexAttrib4s;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SVPROC __glewVertexAttrib4sv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UBVPROC __glewVertexAttrib4ubv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UIVPROC __glewVertexAttrib4uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4USVPROC __glewVertexAttrib4usv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBPOINTERPROC __glewVertexAttribPointer;

GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2X3FVPROC __glewUniformMatrix2x3fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2X4FVPROC __glewUniformMatrix2x4fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3X2FVPROC __glewUniformMatrix3x2fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3X4FVPROC __glewUniformMatrix3x4fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4X2FVPROC __glewUniformMatrix4x2fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4X3FVPROC __glewUniformMatrix4x3fv;

GLEW_FUN_EXPORT PFNGLBEGINCONDITIONALRENDERPROC __glewBeginConditionalRender;
GLEW_FUN_EXPORT PFNGLBEGINTRANSFORMFEEDBACKPROC __glewBeginTransformFeedback;
GLEW_FUN_EXPORT PFNGLBINDFRAGDATALOCATIONPROC __glewBindFragDataLocation;
GLEW_FUN_EXPORT PFNGLCLAMPCOLORPROC __glewClampColor;
GLEW_FUN_EXPORT PFNGLCLEARBUFFERFIPROC __glewClearBufferfi;
GLEW_FUN_EXPORT PFNGLCLEARBUFFERFVPROC __glewClearBufferfv;
GLEW_FUN_EXPORT PFNGLCLEARBUFFERIVPROC __glewClearBufferiv;
GLEW_FUN_EXPORT PFNGLCLEARBUFFERUIVPROC __glewClearBufferuiv;
GLEW_FUN_EXPORT PFNGLCOLORMASKIPROC __glewColorMaski;
GLEW_FUN_EXPORT PFNGLDISABLEIPROC __glewDisablei;
GLEW_FUN_EXPORT PFNGLENABLEIPROC __glewEnablei;
GLEW_FUN_EXPORT PFNGLENDCONDITIONALRENDERPROC __glewEndConditionalRender;
GLEW_FUN_EXPORT PFNGLENDTRANSFORMFEEDBACKPROC __glewEndTransformFeedback;
GLEW_FUN_EXPORT PFNGLGETBOOLEANI_VPROC __glewGetBooleani_v;
GLEW_FUN_EXPORT PFNGLGETFRAGDATALOCATIONPROC __glewGetFragDataLocation;
GLEW_FUN_EXPORT PFNGLGETSTRINGIPROC __glewGetStringi;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERIIVPROC __glewGetTexParameterIiv;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERIUIVPROC __glewGetTexParameterIuiv;
GLEW_FUN_EXPORT PFNGLGETTRANSFORMFEEDBACKVARYINGPROC __glewGetTransformFeedbackVarying;
GLEW_FUN_EXPORT PFNGLGETUNIFORMUIVPROC __glewGetUniformuiv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIIVPROC __glewGetVertexAttribIiv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIUIVPROC __glewGetVertexAttribIuiv;
GLEW_FUN_EXPORT PFNGLISENABLEDIPROC __glewIsEnabledi;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIIVPROC __glewTexParameterIiv;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIUIVPROC __glewTexParameterIuiv;
GLEW_FUN_EXPORT PFNGLTRANSFORMFEEDBACKVARYINGSPROC __glewTransformFeedbackVaryings;
GLEW_FUN_EXPORT PFNGLUNIFORM1UIPROC __glewUniform1ui;
GLEW_FUN_EXPORT PFNGLUNIFORM1UIVPROC __glewUniform1uiv;
GLEW_FUN_EXPORT PFNGLUNIFORM2UIPROC __glewUniform2ui;
GLEW_FUN_EXPORT PFNGLUNIFORM2UIVPROC __glewUniform2uiv;
GLEW_FUN_EXPORT PFNGLUNIFORM3UIPROC __glewUniform3ui;
GLEW_FUN_EXPORT PFNGLUNIFORM3UIVPROC __glewUniform3uiv;
GLEW_FUN_EXPORT PFNGLUNIFORM4UIPROC __glewUniform4ui;
GLEW_FUN_EXPORT PFNGLUNIFORM4UIVPROC __glewUniform4uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1IPROC __glewVertexAttribI1i;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1IVPROC __glewVertexAttribI1iv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1UIPROC __glewVertexAttribI1ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1UIVPROC __glewVertexAttribI1uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2IPROC __glewVertexAttribI2i;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2IVPROC __glewVertexAttribI2iv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2UIPROC __glewVertexAttribI2ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2UIVPROC __glewVertexAttribI2uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3IPROC __glewVertexAttribI3i;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3IVPROC __glewVertexAttribI3iv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3UIPROC __glewVertexAttribI3ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3UIVPROC __glewVertexAttribI3uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4BVPROC __glewVertexAttribI4bv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4IPROC __glewVertexAttribI4i;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4IVPROC __glewVertexAttribI4iv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4SVPROC __glewVertexAttribI4sv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UBVPROC __glewVertexAttribI4ubv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UIPROC __glewVertexAttribI4ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UIVPROC __glewVertexAttribI4uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4USVPROC __glewVertexAttribI4usv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBIPOINTERPROC __glewVertexAttribIPointer;

GLEW_FUN_EXPORT PFNGLDRAWARRAYSINSTANCEDPROC __glewDrawArraysInstanced;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINSTANCEDPROC __glewDrawElementsInstanced;
GLEW_FUN_EXPORT PFNGLPRIMITIVERESTARTINDEXPROC __glewPrimitiveRestartIndex;
GLEW_FUN_EXPORT PFNGLTEXBUFFERPROC __glewTexBuffer;

GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTUREPROC __glewFramebufferTexture;
GLEW_FUN_EXPORT PFNGLGETBUFFERPARAMETERI64VPROC __glewGetBufferParameteri64v;
GLEW_FUN_EXPORT PFNGLGETINTEGER64I_VPROC __glewGetInteger64i_v;

GLEW_FUN_EXPORT PFNGLVERTEXATTRIBDIVISORPROC __glewVertexAttribDivisor;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEIPROC __glewBlendEquationSeparatei;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONIPROC __glewBlendEquationi;
GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEIPROC __glewBlendFuncSeparatei;
GLEW_FUN_EXPORT PFNGLBLENDFUNCIPROC __glewBlendFunci;
GLEW_FUN_EXPORT PFNGLMINSAMPLESHADINGPROC __glewMinSampleShading;

GLEW_FUN_EXPORT PFNGLTBUFFERMASK3DFXPROC __glewTbufferMask3DFX;

GLEW_FUN_EXPORT PFNGLDEBUGMESSAGECALLBACKAMDPROC __glewDebugMessageCallbackAMD;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGEENABLEAMDPROC __glewDebugMessageEnableAMD;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGEINSERTAMDPROC __glewDebugMessageInsertAMD;
GLEW_FUN_EXPORT PFNGLGETDEBUGMESSAGELOGAMDPROC __glewGetDebugMessageLogAMD;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONINDEXEDAMDPROC __glewBlendEquationIndexedAMD;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEINDEXEDAMDPROC __glewBlendEquationSeparateIndexedAMD;
GLEW_FUN_EXPORT PFNGLBLENDFUNCINDEXEDAMDPROC __glewBlendFuncIndexedAMD;
GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEINDEXEDAMDPROC __glewBlendFuncSeparateIndexedAMD;

GLEW_FUN_EXPORT PFNGLDELETENAMESAMDPROC __glewDeleteNamesAMD;
GLEW_FUN_EXPORT PFNGLGENNAMESAMDPROC __glewGenNamesAMD;
GLEW_FUN_EXPORT PFNGLISNAMEAMDPROC __glewIsNameAMD;

GLEW_FUN_EXPORT PFNGLBEGINPERFMONITORAMDPROC __glewBeginPerfMonitorAMD;
GLEW_FUN_EXPORT PFNGLDELETEPERFMONITORSAMDPROC __glewDeletePerfMonitorsAMD;
GLEW_FUN_EXPORT PFNGLENDPERFMONITORAMDPROC __glewEndPerfMonitorAMD;
GLEW_FUN_EXPORT PFNGLGENPERFMONITORSAMDPROC __glewGenPerfMonitorsAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERDATAAMDPROC __glewGetPerfMonitorCounterDataAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERINFOAMDPROC __glewGetPerfMonitorCounterInfoAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC __glewGetPerfMonitorCounterStringAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERSAMDPROC __glewGetPerfMonitorCountersAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORGROUPSTRINGAMDPROC __glewGetPerfMonitorGroupStringAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORGROUPSAMDPROC __glewGetPerfMonitorGroupsAMD;
GLEW_FUN_EXPORT PFNGLSELECTPERFMONITORCOUNTERSAMDPROC __glewSelectPerfMonitorCountersAMD;

GLEW_FUN_EXPORT PFNGLTESSELLATIONFACTORAMDPROC __glewTessellationFactorAMD;
GLEW_FUN_EXPORT PFNGLTESSELLATIONMODEAMDPROC __glewTessellationModeAMD;

GLEW_FUN_EXPORT PFNGLDRAWELEMENTARRAYAPPLEPROC __glewDrawElementArrayAPPLE;
GLEW_FUN_EXPORT PFNGLDRAWRANGEELEMENTARRAYAPPLEPROC __glewDrawRangeElementArrayAPPLE;
GLEW_FUN_EXPORT PFNGLELEMENTPOINTERAPPLEPROC __glewElementPointerAPPLE;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTARRAYAPPLEPROC __glewMultiDrawElementArrayAPPLE;
GLEW_FUN_EXPORT PFNGLMULTIDRAWRANGEELEMENTARRAYAPPLEPROC __glewMultiDrawRangeElementArrayAPPLE;

GLEW_FUN_EXPORT PFNGLDELETEFENCESAPPLEPROC __glewDeleteFencesAPPLE;
GLEW_FUN_EXPORT PFNGLFINISHFENCEAPPLEPROC __glewFinishFenceAPPLE;
GLEW_FUN_EXPORT PFNGLFINISHOBJECTAPPLEPROC __glewFinishObjectAPPLE;
GLEW_FUN_EXPORT PFNGLGENFENCESAPPLEPROC __glewGenFencesAPPLE;
GLEW_FUN_EXPORT PFNGLISFENCEAPPLEPROC __glewIsFenceAPPLE;
GLEW_FUN_EXPORT PFNGLSETFENCEAPPLEPROC __glewSetFenceAPPLE;
GLEW_FUN_EXPORT PFNGLTESTFENCEAPPLEPROC __glewTestFenceAPPLE;
GLEW_FUN_EXPORT PFNGLTESTOBJECTAPPLEPROC __glewTestObjectAPPLE;

GLEW_FUN_EXPORT PFNGLBUFFERPARAMETERIAPPLEPROC __glewBufferParameteriAPPLE;
GLEW_FUN_EXPORT PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC __glewFlushMappedBufferRangeAPPLE;

GLEW_FUN_EXPORT PFNGLGETOBJECTPARAMETERIVAPPLEPROC __glewGetObjectParameterivAPPLE;
GLEW_FUN_EXPORT PFNGLOBJECTPURGEABLEAPPLEPROC __glewObjectPurgeableAPPLE;
GLEW_FUN_EXPORT PFNGLOBJECTUNPURGEABLEAPPLEPROC __glewObjectUnpurgeableAPPLE;

GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERPOINTERVAPPLEPROC __glewGetTexParameterPointervAPPLE;
GLEW_FUN_EXPORT PFNGLTEXTURERANGEAPPLEPROC __glewTextureRangeAPPLE;

GLEW_FUN_EXPORT PFNGLBINDVERTEXARRAYAPPLEPROC __glewBindVertexArrayAPPLE;
GLEW_FUN_EXPORT PFNGLDELETEVERTEXARRAYSAPPLEPROC __glewDeleteVertexArraysAPPLE;
GLEW_FUN_EXPORT PFNGLGENVERTEXARRAYSAPPLEPROC __glewGenVertexArraysAPPLE;
GLEW_FUN_EXPORT PFNGLISVERTEXARRAYAPPLEPROC __glewIsVertexArrayAPPLE;

GLEW_FUN_EXPORT PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC __glewFlushVertexArrayRangeAPPLE;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYPARAMETERIAPPLEPROC __glewVertexArrayParameteriAPPLE;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYRANGEAPPLEPROC __glewVertexArrayRangeAPPLE;

GLEW_FUN_EXPORT PFNGLDISABLEVERTEXATTRIBAPPLEPROC __glewDisableVertexAttribAPPLE;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXATTRIBAPPLEPROC __glewEnableVertexAttribAPPLE;
GLEW_FUN_EXPORT PFNGLISVERTEXATTRIBENABLEDAPPLEPROC __glewIsVertexAttribEnabledAPPLE;
GLEW_FUN_EXPORT PFNGLMAPVERTEXATTRIB1DAPPLEPROC __glewMapVertexAttrib1dAPPLE;
GLEW_FUN_EXPORT PFNGLMAPVERTEXATTRIB1FAPPLEPROC __glewMapVertexAttrib1fAPPLE;
GLEW_FUN_EXPORT PFNGLMAPVERTEXATTRIB2DAPPLEPROC __glewMapVertexAttrib2dAPPLE;
GLEW_FUN_EXPORT PFNGLMAPVERTEXATTRIB2FAPPLEPROC __glewMapVertexAttrib2fAPPLE;

GLEW_FUN_EXPORT PFNGLCLEARDEPTHFPROC __glewClearDepthf;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEFPROC __glewDepthRangef;
GLEW_FUN_EXPORT PFNGLGETSHADERPRECISIONFORMATPROC __glewGetShaderPrecisionFormat;
GLEW_FUN_EXPORT PFNGLRELEASESHADERCOMPILERPROC __glewReleaseShaderCompiler;
GLEW_FUN_EXPORT PFNGLSHADERBINARYPROC __glewShaderBinary;

GLEW_FUN_EXPORT PFNGLBINDFRAGDATALOCATIONINDEXEDPROC __glewBindFragDataLocationIndexed;
GLEW_FUN_EXPORT PFNGLGETFRAGDATAINDEXPROC __glewGetFragDataIndex;

GLEW_FUN_EXPORT PFNGLCREATESYNCFROMCLEVENTARBPROC __glewCreateSyncFromCLeventARB;

GLEW_FUN_EXPORT PFNGLCLAMPCOLORARBPROC __glewClampColorARB;

GLEW_FUN_EXPORT PFNGLCOPYBUFFERSUBDATAPROC __glewCopyBufferSubData;

GLEW_FUN_EXPORT PFNGLDEBUGMESSAGECALLBACKARBPROC __glewDebugMessageCallbackARB;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGECONTROLARBPROC __glewDebugMessageControlARB;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGEINSERTARBPROC __glewDebugMessageInsertARB;
GLEW_FUN_EXPORT PFNGLGETDEBUGMESSAGELOGARBPROC __glewGetDebugMessageLogARB;

GLEW_FUN_EXPORT PFNGLDRAWBUFFERSARBPROC __glewDrawBuffersARB;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEIARBPROC __glewBlendEquationSeparateiARB;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONIARBPROC __glewBlendEquationiARB;
GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEIARBPROC __glewBlendFuncSeparateiARB;
GLEW_FUN_EXPORT PFNGLBLENDFUNCIARBPROC __glewBlendFunciARB;

GLEW_FUN_EXPORT PFNGLDRAWELEMENTSBASEVERTEXPROC __glewDrawElementsBaseVertex;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC __glewDrawElementsInstancedBaseVertex;
GLEW_FUN_EXPORT PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC __glewDrawRangeElementsBaseVertex;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC __glewMultiDrawElementsBaseVertex;

GLEW_FUN_EXPORT PFNGLDRAWARRAYSINDIRECTPROC __glewDrawArraysIndirect;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINDIRECTPROC __glewDrawElementsIndirect;

GLEW_FUN_EXPORT PFNGLDRAWARRAYSINSTANCEDARBPROC __glewDrawArraysInstancedARB;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINSTANCEDARBPROC __glewDrawElementsInstancedARB;

GLEW_FUN_EXPORT PFNGLBINDFRAMEBUFFERPROC __glewBindFramebuffer;
GLEW_FUN_EXPORT PFNGLBINDRENDERBUFFERPROC __glewBindRenderbuffer;
GLEW_FUN_EXPORT PFNGLBLITFRAMEBUFFERPROC __glewBlitFramebuffer;
GLEW_FUN_EXPORT PFNGLCHECKFRAMEBUFFERSTATUSPROC __glewCheckFramebufferStatus;
GLEW_FUN_EXPORT PFNGLDELETEFRAMEBUFFERSPROC __glewDeleteFramebuffers;
GLEW_FUN_EXPORT PFNGLDELETERENDERBUFFERSPROC __glewDeleteRenderbuffers;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERRENDERBUFFERPROC __glewFramebufferRenderbuffer;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE1DPROC __glewFramebufferTexture1D;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DPROC __glewFramebufferTexture2D;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE3DPROC __glewFramebufferTexture3D;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURELAYERPROC __glewFramebufferTextureLayer;
GLEW_FUN_EXPORT PFNGLGENFRAMEBUFFERSPROC __glewGenFramebuffers;
GLEW_FUN_EXPORT PFNGLGENRENDERBUFFERSPROC __glewGenRenderbuffers;
GLEW_FUN_EXPORT PFNGLGENERATEMIPMAPPROC __glewGenerateMipmap;
GLEW_FUN_EXPORT PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __glewGetFramebufferAttachmentParameteriv;
GLEW_FUN_EXPORT PFNGLGETRENDERBUFFERPARAMETERIVPROC __glewGetRenderbufferParameteriv;
GLEW_FUN_EXPORT PFNGLISFRAMEBUFFERPROC __glewIsFramebuffer;
GLEW_FUN_EXPORT PFNGLISRENDERBUFFERPROC __glewIsRenderbuffer;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEPROC __glewRenderbufferStorage;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC __glewRenderbufferStorageMultisample;

GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTUREARBPROC __glewFramebufferTextureARB;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTUREFACEARBPROC __glewFramebufferTextureFaceARB;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURELAYERARBPROC __glewFramebufferTextureLayerARB;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERIARBPROC __glewProgramParameteriARB;

GLEW_FUN_EXPORT PFNGLGETPROGRAMBINARYPROC __glewGetProgramBinary;
GLEW_FUN_EXPORT PFNGLPROGRAMBINARYPROC __glewProgramBinary;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERIPROC __glewProgramParameteri;

GLEW_FUN_EXPORT PFNGLGETUNIFORMDVPROC __glewGetUniformdv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1DEXTPROC __glewProgramUniform1dEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1DVEXTPROC __glewProgramUniform1dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2DEXTPROC __glewProgramUniform2dEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2DVEXTPROC __glewProgramUniform2dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3DEXTPROC __glewProgramUniform3dEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3DVEXTPROC __glewProgramUniform3dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4DEXTPROC __glewProgramUniform4dEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4DVEXTPROC __glewProgramUniform4dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2DVEXTPROC __glewProgramUniformMatrix2dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X3DVEXTPROC __glewProgramUniformMatrix2x3dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X4DVEXTPROC __glewProgramUniformMatrix2x4dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3DVEXTPROC __glewProgramUniformMatrix3dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X2DVEXTPROC __glewProgramUniformMatrix3x2dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X4DVEXTPROC __glewProgramUniformMatrix3x4dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4DVEXTPROC __glewProgramUniformMatrix4dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X2DVEXTPROC __glewProgramUniformMatrix4x2dvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X3DVEXTPROC __glewProgramUniformMatrix4x3dvEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM1DPROC __glewUniform1d;
GLEW_FUN_EXPORT PFNGLUNIFORM1DVPROC __glewUniform1dv;
GLEW_FUN_EXPORT PFNGLUNIFORM2DPROC __glewUniform2d;
GLEW_FUN_EXPORT PFNGLUNIFORM2DVPROC __glewUniform2dv;
GLEW_FUN_EXPORT PFNGLUNIFORM3DPROC __glewUniform3d;
GLEW_FUN_EXPORT PFNGLUNIFORM3DVPROC __glewUniform3dv;
GLEW_FUN_EXPORT PFNGLUNIFORM4DPROC __glewUniform4d;
GLEW_FUN_EXPORT PFNGLUNIFORM4DVPROC __glewUniform4dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2DVPROC __glewUniformMatrix2dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2X3DVPROC __glewUniformMatrix2x3dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2X4DVPROC __glewUniformMatrix2x4dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3DVPROC __glewUniformMatrix3dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3X2DVPROC __glewUniformMatrix3x2dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3X4DVPROC __glewUniformMatrix3x4dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4DVPROC __glewUniformMatrix4dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4X2DVPROC __glewUniformMatrix4x2dv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4X3DVPROC __glewUniformMatrix4x3dv;

GLEW_FUN_EXPORT PFNGLCOLORSUBTABLEPROC __glewColorSubTable;
GLEW_FUN_EXPORT PFNGLCOLORTABLEPROC __glewColorTable;
GLEW_FUN_EXPORT PFNGLCOLORTABLEPARAMETERFVPROC __glewColorTableParameterfv;
GLEW_FUN_EXPORT PFNGLCOLORTABLEPARAMETERIVPROC __glewColorTableParameteriv;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONFILTER1DPROC __glewConvolutionFilter1D;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONFILTER2DPROC __glewConvolutionFilter2D;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERFPROC __glewConvolutionParameterf;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERFVPROC __glewConvolutionParameterfv;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERIPROC __glewConvolutionParameteri;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERIVPROC __glewConvolutionParameteriv;
GLEW_FUN_EXPORT PFNGLCOPYCOLORSUBTABLEPROC __glewCopyColorSubTable;
GLEW_FUN_EXPORT PFNGLCOPYCOLORTABLEPROC __glewCopyColorTable;
GLEW_FUN_EXPORT PFNGLCOPYCONVOLUTIONFILTER1DPROC __glewCopyConvolutionFilter1D;
GLEW_FUN_EXPORT PFNGLCOPYCONVOLUTIONFILTER2DPROC __glewCopyConvolutionFilter2D;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPROC __glewGetColorTable;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERFVPROC __glewGetColorTableParameterfv;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERIVPROC __glewGetColorTableParameteriv;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONFILTERPROC __glewGetConvolutionFilter;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONPARAMETERFVPROC __glewGetConvolutionParameterfv;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONPARAMETERIVPROC __glewGetConvolutionParameteriv;
GLEW_FUN_EXPORT PFNGLGETHISTOGRAMPROC __glewGetHistogram;
GLEW_FUN_EXPORT PFNGLGETHISTOGRAMPARAMETERFVPROC __glewGetHistogramParameterfv;
GLEW_FUN_EXPORT PFNGLGETHISTOGRAMPARAMETERIVPROC __glewGetHistogramParameteriv;
GLEW_FUN_EXPORT PFNGLGETMINMAXPROC __glewGetMinmax;
GLEW_FUN_EXPORT PFNGLGETMINMAXPARAMETERFVPROC __glewGetMinmaxParameterfv;
GLEW_FUN_EXPORT PFNGLGETMINMAXPARAMETERIVPROC __glewGetMinmaxParameteriv;
GLEW_FUN_EXPORT PFNGLGETSEPARABLEFILTERPROC __glewGetSeparableFilter;
GLEW_FUN_EXPORT PFNGLHISTOGRAMPROC __glewHistogram;
GLEW_FUN_EXPORT PFNGLMINMAXPROC __glewMinmax;
GLEW_FUN_EXPORT PFNGLRESETHISTOGRAMPROC __glewResetHistogram;
GLEW_FUN_EXPORT PFNGLRESETMINMAXPROC __glewResetMinmax;
GLEW_FUN_EXPORT PFNGLSEPARABLEFILTER2DPROC __glewSeparableFilter2D;

GLEW_FUN_EXPORT PFNGLVERTEXATTRIBDIVISORARBPROC __glewVertexAttribDivisorARB;

GLEW_FUN_EXPORT PFNGLFLUSHMAPPEDBUFFERRANGEPROC __glewFlushMappedBufferRange;
GLEW_FUN_EXPORT PFNGLMAPBUFFERRANGEPROC __glewMapBufferRange;

GLEW_FUN_EXPORT PFNGLCURRENTPALETTEMATRIXARBPROC __glewCurrentPaletteMatrixARB;
GLEW_FUN_EXPORT PFNGLMATRIXINDEXPOINTERARBPROC __glewMatrixIndexPointerARB;
GLEW_FUN_EXPORT PFNGLMATRIXINDEXUBVARBPROC __glewMatrixIndexubvARB;
GLEW_FUN_EXPORT PFNGLMATRIXINDEXUIVARBPROC __glewMatrixIndexuivARB;
GLEW_FUN_EXPORT PFNGLMATRIXINDEXUSVARBPROC __glewMatrixIndexusvARB;

GLEW_FUN_EXPORT PFNGLSAMPLECOVERAGEARBPROC __glewSampleCoverageARB;

GLEW_FUN_EXPORT PFNGLACTIVETEXTUREARBPROC __glewActiveTextureARB;
GLEW_FUN_EXPORT PFNGLCLIENTACTIVETEXTUREARBPROC __glewClientActiveTextureARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1DARBPROC __glewMultiTexCoord1dARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1DVARBPROC __glewMultiTexCoord1dvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1FARBPROC __glewMultiTexCoord1fARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1FVARBPROC __glewMultiTexCoord1fvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1IARBPROC __glewMultiTexCoord1iARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1IVARBPROC __glewMultiTexCoord1ivARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1SARBPROC __glewMultiTexCoord1sARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1SVARBPROC __glewMultiTexCoord1svARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2DARBPROC __glewMultiTexCoord2dARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2DVARBPROC __glewMultiTexCoord2dvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2FARBPROC __glewMultiTexCoord2fARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2FVARBPROC __glewMultiTexCoord2fvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2IARBPROC __glewMultiTexCoord2iARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2IVARBPROC __glewMultiTexCoord2ivARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2SARBPROC __glewMultiTexCoord2sARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2SVARBPROC __glewMultiTexCoord2svARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3DARBPROC __glewMultiTexCoord3dARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3DVARBPROC __glewMultiTexCoord3dvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3FARBPROC __glewMultiTexCoord3fARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3FVARBPROC __glewMultiTexCoord3fvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3IARBPROC __glewMultiTexCoord3iARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3IVARBPROC __glewMultiTexCoord3ivARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3SARBPROC __glewMultiTexCoord3sARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3SVARBPROC __glewMultiTexCoord3svARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4DARBPROC __glewMultiTexCoord4dARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4DVARBPROC __glewMultiTexCoord4dvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4FARBPROC __glewMultiTexCoord4fARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4FVARBPROC __glewMultiTexCoord4fvARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4IARBPROC __glewMultiTexCoord4iARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4IVARBPROC __glewMultiTexCoord4ivARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4SARBPROC __glewMultiTexCoord4sARB;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4SVARBPROC __glewMultiTexCoord4svARB;

GLEW_FUN_EXPORT PFNGLBEGINQUERYARBPROC __glewBeginQueryARB;
GLEW_FUN_EXPORT PFNGLDELETEQUERIESARBPROC __glewDeleteQueriesARB;
GLEW_FUN_EXPORT PFNGLENDQUERYARBPROC __glewEndQueryARB;
GLEW_FUN_EXPORT PFNGLGENQUERIESARBPROC __glewGenQueriesARB;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTIVARBPROC __glewGetQueryObjectivARB;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTUIVARBPROC __glewGetQueryObjectuivARB;
GLEW_FUN_EXPORT PFNGLGETQUERYIVARBPROC __glewGetQueryivARB;
GLEW_FUN_EXPORT PFNGLISQUERYARBPROC __glewIsQueryARB;

GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFARBPROC __glewPointParameterfARB;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFVARBPROC __glewPointParameterfvARB;

GLEW_FUN_EXPORT PFNGLPROVOKINGVERTEXPROC __glewProvokingVertex;

GLEW_FUN_EXPORT PFNGLGETNCOLORTABLEARBPROC __glewGetnColorTableARB;
GLEW_FUN_EXPORT PFNGLGETNCOMPRESSEDTEXIMAGEARBPROC __glewGetnCompressedTexImageARB;
GLEW_FUN_EXPORT PFNGLGETNCONVOLUTIONFILTERARBPROC __glewGetnConvolutionFilterARB;
GLEW_FUN_EXPORT PFNGLGETNHISTOGRAMARBPROC __glewGetnHistogramARB;
GLEW_FUN_EXPORT PFNGLGETNMAPDVARBPROC __glewGetnMapdvARB;
GLEW_FUN_EXPORT PFNGLGETNMAPFVARBPROC __glewGetnMapfvARB;
GLEW_FUN_EXPORT PFNGLGETNMAPIVARBPROC __glewGetnMapivARB;
GLEW_FUN_EXPORT PFNGLGETNMINMAXARBPROC __glewGetnMinmaxARB;
GLEW_FUN_EXPORT PFNGLGETNPIXELMAPFVARBPROC __glewGetnPixelMapfvARB;
GLEW_FUN_EXPORT PFNGLGETNPIXELMAPUIVARBPROC __glewGetnPixelMapuivARB;
GLEW_FUN_EXPORT PFNGLGETNPIXELMAPUSVARBPROC __glewGetnPixelMapusvARB;
GLEW_FUN_EXPORT PFNGLGETNPOLYGONSTIPPLEARBPROC __glewGetnPolygonStippleARB;
GLEW_FUN_EXPORT PFNGLGETNSEPARABLEFILTERARBPROC __glewGetnSeparableFilterARB;
GLEW_FUN_EXPORT PFNGLGETNTEXIMAGEARBPROC __glewGetnTexImageARB;
GLEW_FUN_EXPORT PFNGLGETNUNIFORMDVARBPROC __glewGetnUniformdvARB;
GLEW_FUN_EXPORT PFNGLGETNUNIFORMFVARBPROC __glewGetnUniformfvARB;
GLEW_FUN_EXPORT PFNGLGETNUNIFORMIVARBPROC __glewGetnUniformivARB;
GLEW_FUN_EXPORT PFNGLGETNUNIFORMUIVARBPROC __glewGetnUniformuivARB;
GLEW_FUN_EXPORT PFNGLREADNPIXELSARBPROC __glewReadnPixelsARB;

GLEW_FUN_EXPORT PFNGLMINSAMPLESHADINGARBPROC __glewMinSampleShadingARB;

GLEW_FUN_EXPORT PFNGLBINDSAMPLERPROC __glewBindSampler;
GLEW_FUN_EXPORT PFNGLDELETESAMPLERSPROC __glewDeleteSamplers;
GLEW_FUN_EXPORT PFNGLGENSAMPLERSPROC __glewGenSamplers;
GLEW_FUN_EXPORT PFNGLGETSAMPLERPARAMETERIIVPROC __glewGetSamplerParameterIiv;
GLEW_FUN_EXPORT PFNGLGETSAMPLERPARAMETERIUIVPROC __glewGetSamplerParameterIuiv;
GLEW_FUN_EXPORT PFNGLGETSAMPLERPARAMETERFVPROC __glewGetSamplerParameterfv;
GLEW_FUN_EXPORT PFNGLGETSAMPLERPARAMETERIVPROC __glewGetSamplerParameteriv;
GLEW_FUN_EXPORT PFNGLISSAMPLERPROC __glewIsSampler;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERIIVPROC __glewSamplerParameterIiv;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERIUIVPROC __glewSamplerParameterIuiv;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERFPROC __glewSamplerParameterf;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERFVPROC __glewSamplerParameterfv;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERIPROC __glewSamplerParameteri;
GLEW_FUN_EXPORT PFNGLSAMPLERPARAMETERIVPROC __glewSamplerParameteriv;

GLEW_FUN_EXPORT PFNGLACTIVESHADERPROGRAMPROC __glewActiveShaderProgram;
GLEW_FUN_EXPORT PFNGLBINDPROGRAMPIPELINEPROC __glewBindProgramPipeline;
GLEW_FUN_EXPORT PFNGLCREATESHADERPROGRAMVPROC __glewCreateShaderProgramv;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMPIPELINESPROC __glewDeleteProgramPipelines;
GLEW_FUN_EXPORT PFNGLGENPROGRAMPIPELINESPROC __glewGenProgramPipelines;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPIPELINEINFOLOGPROC __glewGetProgramPipelineInfoLog;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPIPELINEIVPROC __glewGetProgramPipelineiv;
GLEW_FUN_EXPORT PFNGLISPROGRAMPIPELINEPROC __glewIsProgramPipeline;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1DPROC __glewProgramUniform1d;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1DVPROC __glewProgramUniform1dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FPROC __glewProgramUniform1f;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FVPROC __glewProgramUniform1fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IPROC __glewProgramUniform1i;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IVPROC __glewProgramUniform1iv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UIPROC __glewProgramUniform1ui;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UIVPROC __glewProgramUniform1uiv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2DPROC __glewProgramUniform2d;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2DVPROC __glewProgramUniform2dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FPROC __glewProgramUniform2f;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FVPROC __glewProgramUniform2fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IPROC __glewProgramUniform2i;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IVPROC __glewProgramUniform2iv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UIPROC __glewProgramUniform2ui;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UIVPROC __glewProgramUniform2uiv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3DPROC __glewProgramUniform3d;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3DVPROC __glewProgramUniform3dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FPROC __glewProgramUniform3f;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FVPROC __glewProgramUniform3fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IPROC __glewProgramUniform3i;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IVPROC __glewProgramUniform3iv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UIPROC __glewProgramUniform3ui;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UIVPROC __glewProgramUniform3uiv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4DPROC __glewProgramUniform4d;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4DVPROC __glewProgramUniform4dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FPROC __glewProgramUniform4f;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FVPROC __glewProgramUniform4fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IPROC __glewProgramUniform4i;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IVPROC __glewProgramUniform4iv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UIPROC __glewProgramUniform4ui;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UIVPROC __glewProgramUniform4uiv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2DVPROC __glewProgramUniformMatrix2dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2FVPROC __glewProgramUniformMatrix2fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC __glewProgramUniformMatrix2x3dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC __glewProgramUniformMatrix2x3fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC __glewProgramUniformMatrix2x4dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC __glewProgramUniformMatrix2x4fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3DVPROC __glewProgramUniformMatrix3dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3FVPROC __glewProgramUniformMatrix3fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC __glewProgramUniformMatrix3x2dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC __glewProgramUniformMatrix3x2fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC __glewProgramUniformMatrix3x4dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC __glewProgramUniformMatrix3x4fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4DVPROC __glewProgramUniformMatrix4dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4FVPROC __glewProgramUniformMatrix4fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC __glewProgramUniformMatrix4x2dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC __glewProgramUniformMatrix4x2fv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC __glewProgramUniformMatrix4x3dv;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC __glewProgramUniformMatrix4x3fv;
GLEW_FUN_EXPORT PFNGLUSEPROGRAMSTAGESPROC __glewUseProgramStages;
GLEW_FUN_EXPORT PFNGLVALIDATEPROGRAMPIPELINEPROC __glewValidateProgramPipeline;

GLEW_FUN_EXPORT PFNGLATTACHOBJECTARBPROC __glewAttachObjectARB;
GLEW_FUN_EXPORT PFNGLCOMPILESHADERARBPROC __glewCompileShaderARB;
GLEW_FUN_EXPORT PFNGLCREATEPROGRAMOBJECTARBPROC __glewCreateProgramObjectARB;
GLEW_FUN_EXPORT PFNGLCREATESHADEROBJECTARBPROC __glewCreateShaderObjectARB;
GLEW_FUN_EXPORT PFNGLDELETEOBJECTARBPROC __glewDeleteObjectARB;
GLEW_FUN_EXPORT PFNGLDETACHOBJECTARBPROC __glewDetachObjectARB;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMARBPROC __glewGetActiveUniformARB;
GLEW_FUN_EXPORT PFNGLGETATTACHEDOBJECTSARBPROC __glewGetAttachedObjectsARB;
GLEW_FUN_EXPORT PFNGLGETHANDLEARBPROC __glewGetHandleARB;
GLEW_FUN_EXPORT PFNGLGETINFOLOGARBPROC __glewGetInfoLogARB;
GLEW_FUN_EXPORT PFNGLGETOBJECTPARAMETERFVARBPROC __glewGetObjectParameterfvARB;
GLEW_FUN_EXPORT PFNGLGETOBJECTPARAMETERIVARBPROC __glewGetObjectParameterivARB;
GLEW_FUN_EXPORT PFNGLGETSHADERSOURCEARBPROC __glewGetShaderSourceARB;
GLEW_FUN_EXPORT PFNGLGETUNIFORMLOCATIONARBPROC __glewGetUniformLocationARB;
GLEW_FUN_EXPORT PFNGLGETUNIFORMFVARBPROC __glewGetUniformfvARB;
GLEW_FUN_EXPORT PFNGLGETUNIFORMIVARBPROC __glewGetUniformivARB;
GLEW_FUN_EXPORT PFNGLLINKPROGRAMARBPROC __glewLinkProgramARB;
GLEW_FUN_EXPORT PFNGLSHADERSOURCEARBPROC __glewShaderSourceARB;
GLEW_FUN_EXPORT PFNGLUNIFORM1FARBPROC __glewUniform1fARB;
GLEW_FUN_EXPORT PFNGLUNIFORM1FVARBPROC __glewUniform1fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORM1IARBPROC __glewUniform1iARB;
GLEW_FUN_EXPORT PFNGLUNIFORM1IVARBPROC __glewUniform1ivARB;
GLEW_FUN_EXPORT PFNGLUNIFORM2FARBPROC __glewUniform2fARB;
GLEW_FUN_EXPORT PFNGLUNIFORM2FVARBPROC __glewUniform2fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORM2IARBPROC __glewUniform2iARB;
GLEW_FUN_EXPORT PFNGLUNIFORM2IVARBPROC __glewUniform2ivARB;
GLEW_FUN_EXPORT PFNGLUNIFORM3FARBPROC __glewUniform3fARB;
GLEW_FUN_EXPORT PFNGLUNIFORM3FVARBPROC __glewUniform3fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORM3IARBPROC __glewUniform3iARB;
GLEW_FUN_EXPORT PFNGLUNIFORM3IVARBPROC __glewUniform3ivARB;
GLEW_FUN_EXPORT PFNGLUNIFORM4FARBPROC __glewUniform4fARB;
GLEW_FUN_EXPORT PFNGLUNIFORM4FVARBPROC __glewUniform4fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORM4IARBPROC __glewUniform4iARB;
GLEW_FUN_EXPORT PFNGLUNIFORM4IVARBPROC __glewUniform4ivARB;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2FVARBPROC __glewUniformMatrix2fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3FVARBPROC __glewUniformMatrix3fvARB;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4FVARBPROC __glewUniformMatrix4fvARB;
GLEW_FUN_EXPORT PFNGLUSEPROGRAMOBJECTARBPROC __glewUseProgramObjectARB;
GLEW_FUN_EXPORT PFNGLVALIDATEPROGRAMARBPROC __glewValidateProgramARB;

GLEW_FUN_EXPORT PFNGLGETACTIVESUBROUTINENAMEPROC __glewGetActiveSubroutineName;
GLEW_FUN_EXPORT PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC __glewGetActiveSubroutineUniformName;
GLEW_FUN_EXPORT PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC __glewGetActiveSubroutineUniformiv;
GLEW_FUN_EXPORT PFNGLGETPROGRAMSTAGEIVPROC __glewGetProgramStageiv;
GLEW_FUN_EXPORT PFNGLGETSUBROUTINEINDEXPROC __glewGetSubroutineIndex;
GLEW_FUN_EXPORT PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC __glewGetSubroutineUniformLocation;
GLEW_FUN_EXPORT PFNGLGETUNIFORMSUBROUTINEUIVPROC __glewGetUniformSubroutineuiv;
GLEW_FUN_EXPORT PFNGLUNIFORMSUBROUTINESUIVPROC __glewUniformSubroutinesuiv;

GLEW_FUN_EXPORT PFNGLCOMPILESHADERINCLUDEARBPROC __glewCompileShaderIncludeARB;
GLEW_FUN_EXPORT PFNGLDELETENAMEDSTRINGARBPROC __glewDeleteNamedStringARB;
GLEW_FUN_EXPORT PFNGLGETNAMEDSTRINGARBPROC __glewGetNamedStringARB;
GLEW_FUN_EXPORT PFNGLGETNAMEDSTRINGIVARBPROC __glewGetNamedStringivARB;
GLEW_FUN_EXPORT PFNGLISNAMEDSTRINGARBPROC __glewIsNamedStringARB;
GLEW_FUN_EXPORT PFNGLNAMEDSTRINGARBPROC __glewNamedStringARB;

GLEW_FUN_EXPORT PFNGLCLIENTWAITSYNCPROC __glewClientWaitSync;
GLEW_FUN_EXPORT PFNGLDELETESYNCPROC __glewDeleteSync;
GLEW_FUN_EXPORT PFNGLFENCESYNCPROC __glewFenceSync;
GLEW_FUN_EXPORT PFNGLGETINTEGER64VPROC __glewGetInteger64v;
GLEW_FUN_EXPORT PFNGLGETSYNCIVPROC __glewGetSynciv;
GLEW_FUN_EXPORT PFNGLISSYNCPROC __glewIsSync;
GLEW_FUN_EXPORT PFNGLWAITSYNCPROC __glewWaitSync;

GLEW_FUN_EXPORT PFNGLPATCHPARAMETERFVPROC __glewPatchParameterfv;
GLEW_FUN_EXPORT PFNGLPATCHPARAMETERIPROC __glewPatchParameteri;

GLEW_FUN_EXPORT PFNGLTEXBUFFERARBPROC __glewTexBufferARB;

GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE1DARBPROC __glewCompressedTexImage1DARB;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE2DARBPROC __glewCompressedTexImage2DARB;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE3DARBPROC __glewCompressedTexImage3DARB;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC __glewCompressedTexSubImage1DARB;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC __glewCompressedTexSubImage2DARB;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC __glewCompressedTexSubImage3DARB;
GLEW_FUN_EXPORT PFNGLGETCOMPRESSEDTEXIMAGEARBPROC __glewGetCompressedTexImageARB;

GLEW_FUN_EXPORT PFNGLGETMULTISAMPLEFVPROC __glewGetMultisamplefv;
GLEW_FUN_EXPORT PFNGLSAMPLEMASKIPROC __glewSampleMaski;
GLEW_FUN_EXPORT PFNGLTEXIMAGE2DMULTISAMPLEPROC __glewTexImage2DMultisample;
GLEW_FUN_EXPORT PFNGLTEXIMAGE3DMULTISAMPLEPROC __glewTexImage3DMultisample;

GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTI64VPROC __glewGetQueryObjecti64v;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTUI64VPROC __glewGetQueryObjectui64v;
GLEW_FUN_EXPORT PFNGLQUERYCOUNTERPROC __glewQueryCounter;

GLEW_FUN_EXPORT PFNGLBINDTRANSFORMFEEDBACKPROC __glewBindTransformFeedback;
GLEW_FUN_EXPORT PFNGLDELETETRANSFORMFEEDBACKSPROC __glewDeleteTransformFeedbacks;
GLEW_FUN_EXPORT PFNGLDRAWTRANSFORMFEEDBACKPROC __glewDrawTransformFeedback;
GLEW_FUN_EXPORT PFNGLGENTRANSFORMFEEDBACKSPROC __glewGenTransformFeedbacks;
GLEW_FUN_EXPORT PFNGLISTRANSFORMFEEDBACKPROC __glewIsTransformFeedback;
GLEW_FUN_EXPORT PFNGLPAUSETRANSFORMFEEDBACKPROC __glewPauseTransformFeedback;
GLEW_FUN_EXPORT PFNGLRESUMETRANSFORMFEEDBACKPROC __glewResumeTransformFeedback;

GLEW_FUN_EXPORT PFNGLBEGINQUERYINDEXEDPROC __glewBeginQueryIndexed;
GLEW_FUN_EXPORT PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC __glewDrawTransformFeedbackStream;
GLEW_FUN_EXPORT PFNGLENDQUERYINDEXEDPROC __glewEndQueryIndexed;
GLEW_FUN_EXPORT PFNGLGETQUERYINDEXEDIVPROC __glewGetQueryIndexediv;

GLEW_FUN_EXPORT PFNGLLOADTRANSPOSEMATRIXDARBPROC __glewLoadTransposeMatrixdARB;
GLEW_FUN_EXPORT PFNGLLOADTRANSPOSEMATRIXFARBPROC __glewLoadTransposeMatrixfARB;
GLEW_FUN_EXPORT PFNGLMULTTRANSPOSEMATRIXDARBPROC __glewMultTransposeMatrixdARB;
GLEW_FUN_EXPORT PFNGLMULTTRANSPOSEMATRIXFARBPROC __glewMultTransposeMatrixfARB;

GLEW_FUN_EXPORT PFNGLBINDBUFFERBASEPROC __glewBindBufferBase;
GLEW_FUN_EXPORT PFNGLBINDBUFFERRANGEPROC __glewBindBufferRange;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC __glewGetActiveUniformBlockName;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMBLOCKIVPROC __glewGetActiveUniformBlockiv;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMNAMEPROC __glewGetActiveUniformName;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMSIVPROC __glewGetActiveUniformsiv;
GLEW_FUN_EXPORT PFNGLGETINTEGERI_VPROC __glewGetIntegeri_v;
GLEW_FUN_EXPORT PFNGLGETUNIFORMBLOCKINDEXPROC __glewGetUniformBlockIndex;
GLEW_FUN_EXPORT PFNGLGETUNIFORMINDICESPROC __glewGetUniformIndices;
GLEW_FUN_EXPORT PFNGLUNIFORMBLOCKBINDINGPROC __glewUniformBlockBinding;

GLEW_FUN_EXPORT PFNGLBINDVERTEXARRAYPROC __glewBindVertexArray;
GLEW_FUN_EXPORT PFNGLDELETEVERTEXARRAYSPROC __glewDeleteVertexArrays;
GLEW_FUN_EXPORT PFNGLGENVERTEXARRAYSPROC __glewGenVertexArrays;
GLEW_FUN_EXPORT PFNGLISVERTEXARRAYPROC __glewIsVertexArray;

GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBLDVPROC __glewGetVertexAttribLdv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1DPROC __glewVertexAttribL1d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1DVPROC __glewVertexAttribL1dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2DPROC __glewVertexAttribL2d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2DVPROC __glewVertexAttribL2dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3DPROC __glewVertexAttribL3d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3DVPROC __glewVertexAttribL3dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4DPROC __glewVertexAttribL4d;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4DVPROC __glewVertexAttribL4dv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBLPOINTERPROC __glewVertexAttribLPointer;

GLEW_FUN_EXPORT PFNGLVERTEXBLENDARBPROC __glewVertexBlendARB;
GLEW_FUN_EXPORT PFNGLWEIGHTPOINTERARBPROC __glewWeightPointerARB;
GLEW_FUN_EXPORT PFNGLWEIGHTBVARBPROC __glewWeightbvARB;
GLEW_FUN_EXPORT PFNGLWEIGHTDVARBPROC __glewWeightdvARB;
GLEW_FUN_EXPORT PFNGLWEIGHTFVARBPROC __glewWeightfvARB;
GLEW_FUN_EXPORT PFNGLWEIGHTIVARBPROC __glewWeightivARB;
GLEW_FUN_EXPORT PFNGLWEIGHTSVARBPROC __glewWeightsvARB;
GLEW_FUN_EXPORT PFNGLWEIGHTUBVARBPROC __glewWeightubvARB;
GLEW_FUN_EXPORT PFNGLWEIGHTUIVARBPROC __glewWeightuivARB;
GLEW_FUN_EXPORT PFNGLWEIGHTUSVARBPROC __glewWeightusvARB;

GLEW_FUN_EXPORT PFNGLBINDBUFFERARBPROC __glewBindBufferARB;
GLEW_FUN_EXPORT PFNGLBUFFERDATAARBPROC __glewBufferDataARB;
GLEW_FUN_EXPORT PFNGLBUFFERSUBDATAARBPROC __glewBufferSubDataARB;
GLEW_FUN_EXPORT PFNGLDELETEBUFFERSARBPROC __glewDeleteBuffersARB;
GLEW_FUN_EXPORT PFNGLGENBUFFERSARBPROC __glewGenBuffersARB;
GLEW_FUN_EXPORT PFNGLGETBUFFERPARAMETERIVARBPROC __glewGetBufferParameterivARB;
GLEW_FUN_EXPORT PFNGLGETBUFFERPOINTERVARBPROC __glewGetBufferPointervARB;
GLEW_FUN_EXPORT PFNGLGETBUFFERSUBDATAARBPROC __glewGetBufferSubDataARB;
GLEW_FUN_EXPORT PFNGLISBUFFERARBPROC __glewIsBufferARB;
GLEW_FUN_EXPORT PFNGLMAPBUFFERARBPROC __glewMapBufferARB;
GLEW_FUN_EXPORT PFNGLUNMAPBUFFERARBPROC __glewUnmapBufferARB;

GLEW_FUN_EXPORT PFNGLBINDPROGRAMARBPROC __glewBindProgramARB;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMSARBPROC __glewDeleteProgramsARB;
GLEW_FUN_EXPORT PFNGLDISABLEVERTEXATTRIBARRAYARBPROC __glewDisableVertexAttribArrayARB;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXATTRIBARRAYARBPROC __glewEnableVertexAttribArrayARB;
GLEW_FUN_EXPORT PFNGLGENPROGRAMSARBPROC __glewGenProgramsARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMENVPARAMETERDVARBPROC __glewGetProgramEnvParameterdvARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMENVPARAMETERFVARBPROC __glewGetProgramEnvParameterfvARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC __glewGetProgramLocalParameterdvARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC __glewGetProgramLocalParameterfvARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMSTRINGARBPROC __glewGetProgramStringARB;
GLEW_FUN_EXPORT PFNGLGETPROGRAMIVARBPROC __glewGetProgramivARB;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBPOINTERVARBPROC __glewGetVertexAttribPointervARB;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBDVARBPROC __glewGetVertexAttribdvARB;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBFVARBPROC __glewGetVertexAttribfvARB;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIVARBPROC __glewGetVertexAttribivARB;
GLEW_FUN_EXPORT PFNGLISPROGRAMARBPROC __glewIsProgramARB;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETER4DARBPROC __glewProgramEnvParameter4dARB;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETER4DVARBPROC __glewProgramEnvParameter4dvARB;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETER4FARBPROC __glewProgramEnvParameter4fARB;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETER4FVARBPROC __glewProgramEnvParameter4fvARB;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETER4DARBPROC __glewProgramLocalParameter4dARB;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETER4DVARBPROC __glewProgramLocalParameter4dvARB;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETER4FARBPROC __glewProgramLocalParameter4fARB;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETER4FVARBPROC __glewProgramLocalParameter4fvARB;
GLEW_FUN_EXPORT PFNGLPROGRAMSTRINGARBPROC __glewProgramStringARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DARBPROC __glewVertexAttrib1dARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DVARBPROC __glewVertexAttrib1dvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FARBPROC __glewVertexAttrib1fARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FVARBPROC __glewVertexAttrib1fvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SARBPROC __glewVertexAttrib1sARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SVARBPROC __glewVertexAttrib1svARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DARBPROC __glewVertexAttrib2dARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DVARBPROC __glewVertexAttrib2dvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FARBPROC __glewVertexAttrib2fARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FVARBPROC __glewVertexAttrib2fvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SARBPROC __glewVertexAttrib2sARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SVARBPROC __glewVertexAttrib2svARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DARBPROC __glewVertexAttrib3dARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DVARBPROC __glewVertexAttrib3dvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FARBPROC __glewVertexAttrib3fARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FVARBPROC __glewVertexAttrib3fvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SARBPROC __glewVertexAttrib3sARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SVARBPROC __glewVertexAttrib3svARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NBVARBPROC __glewVertexAttrib4NbvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NIVARBPROC __glewVertexAttrib4NivARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NSVARBPROC __glewVertexAttrib4NsvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUBARBPROC __glewVertexAttrib4NubARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUBVARBPROC __glewVertexAttrib4NubvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUIVARBPROC __glewVertexAttrib4NuivARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4NUSVARBPROC __glewVertexAttrib4NusvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4BVARBPROC __glewVertexAttrib4bvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DARBPROC __glewVertexAttrib4dARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DVARBPROC __glewVertexAttrib4dvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FARBPROC __glewVertexAttrib4fARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FVARBPROC __glewVertexAttrib4fvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4IVARBPROC __glewVertexAttrib4ivARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SARBPROC __glewVertexAttrib4sARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SVARBPROC __glewVertexAttrib4svARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UBVARBPROC __glewVertexAttrib4ubvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UIVARBPROC __glewVertexAttrib4uivARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4USVARBPROC __glewVertexAttrib4usvARB;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBPOINTERARBPROC __glewVertexAttribPointerARB;

GLEW_FUN_EXPORT PFNGLBINDATTRIBLOCATIONARBPROC __glewBindAttribLocationARB;
GLEW_FUN_EXPORT PFNGLGETACTIVEATTRIBARBPROC __glewGetActiveAttribARB;
GLEW_FUN_EXPORT PFNGLGETATTRIBLOCATIONARBPROC __glewGetAttribLocationARB;

GLEW_FUN_EXPORT PFNGLCOLORP3UIPROC __glewColorP3ui;
GLEW_FUN_EXPORT PFNGLCOLORP3UIVPROC __glewColorP3uiv;
GLEW_FUN_EXPORT PFNGLCOLORP4UIPROC __glewColorP4ui;
GLEW_FUN_EXPORT PFNGLCOLORP4UIVPROC __glewColorP4uiv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP1UIPROC __glewMultiTexCoordP1ui;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP1UIVPROC __glewMultiTexCoordP1uiv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP2UIPROC __glewMultiTexCoordP2ui;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP2UIVPROC __glewMultiTexCoordP2uiv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP3UIPROC __glewMultiTexCoordP3ui;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP3UIVPROC __glewMultiTexCoordP3uiv;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP4UIPROC __glewMultiTexCoordP4ui;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDP4UIVPROC __glewMultiTexCoordP4uiv;
GLEW_FUN_EXPORT PFNGLNORMALP3UIPROC __glewNormalP3ui;
GLEW_FUN_EXPORT PFNGLNORMALP3UIVPROC __glewNormalP3uiv;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORP3UIPROC __glewSecondaryColorP3ui;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORP3UIVPROC __glewSecondaryColorP3uiv;
GLEW_FUN_EXPORT PFNGLTEXCOORDP1UIPROC __glewTexCoordP1ui;
GLEW_FUN_EXPORT PFNGLTEXCOORDP1UIVPROC __glewTexCoordP1uiv;
GLEW_FUN_EXPORT PFNGLTEXCOORDP2UIPROC __glewTexCoordP2ui;
GLEW_FUN_EXPORT PFNGLTEXCOORDP2UIVPROC __glewTexCoordP2uiv;
GLEW_FUN_EXPORT PFNGLTEXCOORDP3UIPROC __glewTexCoordP3ui;
GLEW_FUN_EXPORT PFNGLTEXCOORDP3UIVPROC __glewTexCoordP3uiv;
GLEW_FUN_EXPORT PFNGLTEXCOORDP4UIPROC __glewTexCoordP4ui;
GLEW_FUN_EXPORT PFNGLTEXCOORDP4UIVPROC __glewTexCoordP4uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP1UIPROC __glewVertexAttribP1ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP1UIVPROC __glewVertexAttribP1uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP2UIPROC __glewVertexAttribP2ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP2UIVPROC __glewVertexAttribP2uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP3UIPROC __glewVertexAttribP3ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP3UIVPROC __glewVertexAttribP3uiv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP4UIPROC __glewVertexAttribP4ui;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBP4UIVPROC __glewVertexAttribP4uiv;
GLEW_FUN_EXPORT PFNGLVERTEXP2UIPROC __glewVertexP2ui;
GLEW_FUN_EXPORT PFNGLVERTEXP2UIVPROC __glewVertexP2uiv;
GLEW_FUN_EXPORT PFNGLVERTEXP3UIPROC __glewVertexP3ui;
GLEW_FUN_EXPORT PFNGLVERTEXP3UIVPROC __glewVertexP3uiv;
GLEW_FUN_EXPORT PFNGLVERTEXP4UIPROC __glewVertexP4ui;
GLEW_FUN_EXPORT PFNGLVERTEXP4UIVPROC __glewVertexP4uiv;

GLEW_FUN_EXPORT PFNGLDEPTHRANGEARRAYVPROC __glewDepthRangeArrayv;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEINDEXEDPROC __glewDepthRangeIndexed;
GLEW_FUN_EXPORT PFNGLGETDOUBLEI_VPROC __glewGetDoublei_v;
GLEW_FUN_EXPORT PFNGLGETFLOATI_VPROC __glewGetFloati_v;
GLEW_FUN_EXPORT PFNGLSCISSORARRAYVPROC __glewScissorArrayv;
GLEW_FUN_EXPORT PFNGLSCISSORINDEXEDPROC __glewScissorIndexed;
GLEW_FUN_EXPORT PFNGLSCISSORINDEXEDVPROC __glewScissorIndexedv;
GLEW_FUN_EXPORT PFNGLVIEWPORTARRAYVPROC __glewViewportArrayv;
GLEW_FUN_EXPORT PFNGLVIEWPORTINDEXEDFPROC __glewViewportIndexedf;
GLEW_FUN_EXPORT PFNGLVIEWPORTINDEXEDFVPROC __glewViewportIndexedfv;

GLEW_FUN_EXPORT PFNGLWINDOWPOS2DARBPROC __glewWindowPos2dARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2DVARBPROC __glewWindowPos2dvARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FARBPROC __glewWindowPos2fARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FVARBPROC __glewWindowPos2fvARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IARBPROC __glewWindowPos2iARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IVARBPROC __glewWindowPos2ivARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SARBPROC __glewWindowPos2sARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SVARBPROC __glewWindowPos2svARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DARBPROC __glewWindowPos3dARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DVARBPROC __glewWindowPos3dvARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FARBPROC __glewWindowPos3fARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FVARBPROC __glewWindowPos3fvARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IARBPROC __glewWindowPos3iARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IVARBPROC __glewWindowPos3ivARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SARBPROC __glewWindowPos3sARB;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SVARBPROC __glewWindowPos3svARB;

GLEW_FUN_EXPORT PFNGLDRAWBUFFERSATIPROC __glewDrawBuffersATI;

GLEW_FUN_EXPORT PFNGLDRAWELEMENTARRAYATIPROC __glewDrawElementArrayATI;
GLEW_FUN_EXPORT PFNGLDRAWRANGEELEMENTARRAYATIPROC __glewDrawRangeElementArrayATI;
GLEW_FUN_EXPORT PFNGLELEMENTPOINTERATIPROC __glewElementPointerATI;

GLEW_FUN_EXPORT PFNGLGETTEXBUMPPARAMETERFVATIPROC __glewGetTexBumpParameterfvATI;
GLEW_FUN_EXPORT PFNGLGETTEXBUMPPARAMETERIVATIPROC __glewGetTexBumpParameterivATI;
GLEW_FUN_EXPORT PFNGLTEXBUMPPARAMETERFVATIPROC __glewTexBumpParameterfvATI;
GLEW_FUN_EXPORT PFNGLTEXBUMPPARAMETERIVATIPROC __glewTexBumpParameterivATI;

GLEW_FUN_EXPORT PFNGLALPHAFRAGMENTOP1ATIPROC __glewAlphaFragmentOp1ATI;
GLEW_FUN_EXPORT PFNGLALPHAFRAGMENTOP2ATIPROC __glewAlphaFragmentOp2ATI;
GLEW_FUN_EXPORT PFNGLALPHAFRAGMENTOP3ATIPROC __glewAlphaFragmentOp3ATI;
GLEW_FUN_EXPORT PFNGLBEGINFRAGMENTSHADERATIPROC __glewBeginFragmentShaderATI;
GLEW_FUN_EXPORT PFNGLBINDFRAGMENTSHADERATIPROC __glewBindFragmentShaderATI;
GLEW_FUN_EXPORT PFNGLCOLORFRAGMENTOP1ATIPROC __glewColorFragmentOp1ATI;
GLEW_FUN_EXPORT PFNGLCOLORFRAGMENTOP2ATIPROC __glewColorFragmentOp2ATI;
GLEW_FUN_EXPORT PFNGLCOLORFRAGMENTOP3ATIPROC __glewColorFragmentOp3ATI;
GLEW_FUN_EXPORT PFNGLDELETEFRAGMENTSHADERATIPROC __glewDeleteFragmentShaderATI;
GLEW_FUN_EXPORT PFNGLENDFRAGMENTSHADERATIPROC __glewEndFragmentShaderATI;
GLEW_FUN_EXPORT PFNGLGENFRAGMENTSHADERSATIPROC __glewGenFragmentShadersATI;
GLEW_FUN_EXPORT PFNGLPASSTEXCOORDATIPROC __glewPassTexCoordATI;
GLEW_FUN_EXPORT PFNGLSAMPLEMAPATIPROC __glewSampleMapATI;
GLEW_FUN_EXPORT PFNGLSETFRAGMENTSHADERCONSTANTATIPROC __glewSetFragmentShaderConstantATI;

GLEW_FUN_EXPORT PFNGLMAPOBJECTBUFFERATIPROC __glewMapObjectBufferATI;
GLEW_FUN_EXPORT PFNGLUNMAPOBJECTBUFFERATIPROC __glewUnmapObjectBufferATI;

GLEW_FUN_EXPORT PFNGLPNTRIANGLESFATIPROC __glPNTrianglewesfATI;
GLEW_FUN_EXPORT PFNGLPNTRIANGLESIATIPROC __glPNTrianglewesiATI;

GLEW_FUN_EXPORT PFNGLSTENCILFUNCSEPARATEATIPROC __glewStencilFuncSeparateATI;
GLEW_FUN_EXPORT PFNGLSTENCILOPSEPARATEATIPROC __glewStencilOpSeparateATI;

GLEW_FUN_EXPORT PFNGLARRAYOBJECTATIPROC __glewArrayObjectATI;
GLEW_FUN_EXPORT PFNGLFREEOBJECTBUFFERATIPROC __glewFreeObjectBufferATI;
GLEW_FUN_EXPORT PFNGLGETARRAYOBJECTFVATIPROC __glewGetArrayObjectfvATI;
GLEW_FUN_EXPORT PFNGLGETARRAYOBJECTIVATIPROC __glewGetArrayObjectivATI;
GLEW_FUN_EXPORT PFNGLGETOBJECTBUFFERFVATIPROC __glewGetObjectBufferfvATI;
GLEW_FUN_EXPORT PFNGLGETOBJECTBUFFERIVATIPROC __glewGetObjectBufferivATI;
GLEW_FUN_EXPORT PFNGLGETVARIANTARRAYOBJECTFVATIPROC __glewGetVariantArrayObjectfvATI;
GLEW_FUN_EXPORT PFNGLGETVARIANTARRAYOBJECTIVATIPROC __glewGetVariantArrayObjectivATI;
GLEW_FUN_EXPORT PFNGLISOBJECTBUFFERATIPROC __glewIsObjectBufferATI;
GLEW_FUN_EXPORT PFNGLNEWOBJECTBUFFERATIPROC __glewNewObjectBufferATI;
GLEW_FUN_EXPORT PFNGLUPDATEOBJECTBUFFERATIPROC __glewUpdateObjectBufferATI;
GLEW_FUN_EXPORT PFNGLVARIANTARRAYOBJECTATIPROC __glewVariantArrayObjectATI;

GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBARRAYOBJECTFVATIPROC __glewGetVertexAttribArrayObjectfvATI;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBARRAYOBJECTIVATIPROC __glewGetVertexAttribArrayObjectivATI;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBARRAYOBJECTATIPROC __glewVertexAttribArrayObjectATI;

GLEW_FUN_EXPORT PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC __glewClientActiveVertexStreamATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3BATIPROC __glewNormalStream3bATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3BVATIPROC __glewNormalStream3bvATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3DATIPROC __glewNormalStream3dATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3DVATIPROC __glewNormalStream3dvATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3FATIPROC __glewNormalStream3fATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3FVATIPROC __glewNormalStream3fvATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3IATIPROC __glewNormalStream3iATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3IVATIPROC __glewNormalStream3ivATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3SATIPROC __glewNormalStream3sATI;
GLEW_FUN_EXPORT PFNGLNORMALSTREAM3SVATIPROC __glewNormalStream3svATI;
GLEW_FUN_EXPORT PFNGLVERTEXBLENDENVFATIPROC __glewVertexBlendEnvfATI;
GLEW_FUN_EXPORT PFNGLVERTEXBLENDENVIATIPROC __glewVertexBlendEnviATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2DATIPROC __glewVertexStream2dATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2DVATIPROC __glewVertexStream2dvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2FATIPROC __glewVertexStream2fATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2FVATIPROC __glewVertexStream2fvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2IATIPROC __glewVertexStream2iATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2IVATIPROC __glewVertexStream2ivATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2SATIPROC __glewVertexStream2sATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM2SVATIPROC __glewVertexStream2svATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3DATIPROC __glewVertexStream3dATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3DVATIPROC __glewVertexStream3dvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3FATIPROC __glewVertexStream3fATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3FVATIPROC __glewVertexStream3fvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3IATIPROC __glewVertexStream3iATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3IVATIPROC __glewVertexStream3ivATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3SATIPROC __glewVertexStream3sATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM3SVATIPROC __glewVertexStream3svATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4DATIPROC __glewVertexStream4dATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4DVATIPROC __glewVertexStream4dvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4FATIPROC __glewVertexStream4fATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4FVATIPROC __glewVertexStream4fvATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4IATIPROC __glewVertexStream4iATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4IVATIPROC __glewVertexStream4ivATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4SATIPROC __glewVertexStream4sATI;
GLEW_FUN_EXPORT PFNGLVERTEXSTREAM4SVATIPROC __glewVertexStream4svATI;

GLEW_FUN_EXPORT PFNGLGETUNIFORMBUFFERSIZEEXTPROC __glewGetUniformBufferSizeEXT;
GLEW_FUN_EXPORT PFNGLGETUNIFORMOFFSETEXTPROC __glewGetUniformOffsetEXT;
GLEW_FUN_EXPORT PFNGLUNIFORMBUFFEREXTPROC __glewUniformBufferEXT;

GLEW_FUN_EXPORT PFNGLBLENDCOLOREXTPROC __glewBlendColorEXT;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEEXTPROC __glewBlendEquationSeparateEXT;

GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEEXTPROC __glewBlendFuncSeparateEXT;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONEXTPROC __glewBlendEquationEXT;

GLEW_FUN_EXPORT PFNGLCOLORSUBTABLEEXTPROC __glewColorSubTableEXT;
GLEW_FUN_EXPORT PFNGLCOPYCOLORSUBTABLEEXTPROC __glewCopyColorSubTableEXT;

GLEW_FUN_EXPORT PFNGLLOCKARRAYSEXTPROC __glewLockArraysEXT;
GLEW_FUN_EXPORT PFNGLUNLOCKARRAYSEXTPROC __glewUnlockArraysEXT;

GLEW_FUN_EXPORT PFNGLCONVOLUTIONFILTER1DEXTPROC __glewConvolutionFilter1DEXT;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONFILTER2DEXTPROC __glewConvolutionFilter2DEXT;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERFEXTPROC __glewConvolutionParameterfEXT;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERFVEXTPROC __glewConvolutionParameterfvEXT;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERIEXTPROC __glewConvolutionParameteriEXT;
GLEW_FUN_EXPORT PFNGLCONVOLUTIONPARAMETERIVEXTPROC __glewConvolutionParameterivEXT;
GLEW_FUN_EXPORT PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC __glewCopyConvolutionFilter1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC __glewCopyConvolutionFilter2DEXT;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONFILTEREXTPROC __glewGetConvolutionFilterEXT;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC __glewGetConvolutionParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC __glewGetConvolutionParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETSEPARABLEFILTEREXTPROC __glewGetSeparableFilterEXT;
GLEW_FUN_EXPORT PFNGLSEPARABLEFILTER2DEXTPROC __glewSeparableFilter2DEXT;

GLEW_FUN_EXPORT PFNGLBINORMALPOINTEREXTPROC __glewBinormalPointerEXT;
GLEW_FUN_EXPORT PFNGLTANGENTPOINTEREXTPROC __glewTangentPointerEXT;

GLEW_FUN_EXPORT PFNGLCOPYTEXIMAGE1DEXTPROC __glewCopyTexImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXIMAGE2DEXTPROC __glewCopyTexImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE1DEXTPROC __glewCopyTexSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE2DEXTPROC __glewCopyTexSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE3DEXTPROC __glewCopyTexSubImage3DEXT;

GLEW_FUN_EXPORT PFNGLCULLPARAMETERDVEXTPROC __glewCullParameterdvEXT;
GLEW_FUN_EXPORT PFNGLCULLPARAMETERFVEXTPROC __glewCullParameterfvEXT;

GLEW_FUN_EXPORT PFNGLDEPTHBOUNDSEXTPROC __glewDepthBoundsEXT;

GLEW_FUN_EXPORT PFNGLBINDMULTITEXTUREEXTPROC __glewBindMultiTextureEXT;
GLEW_FUN_EXPORT PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC __glewCheckNamedFramebufferStatusEXT;
GLEW_FUN_EXPORT PFNGLCLIENTATTRIBDEFAULTEXTPROC __glewClientAttribDefaultEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXIMAGE1DEXTPROC __glewCompressedMultiTexImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXIMAGE2DEXTPROC __glewCompressedMultiTexImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXIMAGE3DEXTPROC __glewCompressedMultiTexImage3DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXSUBIMAGE1DEXTPROC __glewCompressedMultiTexSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXSUBIMAGE2DEXTPROC __glewCompressedMultiTexSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDMULTITEXSUBIMAGE3DEXTPROC __glewCompressedMultiTexSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC __glewCompressedTextureImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC __glewCompressedTextureImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC __glewCompressedTextureImage3DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC __glewCompressedTextureSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC __glewCompressedTextureSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC __glewCompressedTextureSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLCOPYMULTITEXIMAGE1DEXTPROC __glewCopyMultiTexImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYMULTITEXIMAGE2DEXTPROC __glewCopyMultiTexImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYMULTITEXSUBIMAGE1DEXTPROC __glewCopyMultiTexSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYMULTITEXSUBIMAGE2DEXTPROC __glewCopyMultiTexSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYMULTITEXSUBIMAGE3DEXTPROC __glewCopyMultiTexSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXTUREIMAGE1DEXTPROC __glewCopyTextureImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXTUREIMAGE2DEXTPROC __glewCopyTextureImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC __glewCopyTextureSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC __glewCopyTextureSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC __glewCopyTextureSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLDISABLECLIENTSTATEINDEXEDEXTPROC __glewDisableClientStateIndexedEXT;
GLEW_FUN_EXPORT PFNGLDISABLECLIENTSTATEIEXTPROC __glewDisableClientStateiEXT;
GLEW_FUN_EXPORT PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC __glewDisableVertexArrayAttribEXT;
GLEW_FUN_EXPORT PFNGLDISABLEVERTEXARRAYEXTPROC __glewDisableVertexArrayEXT;
GLEW_FUN_EXPORT PFNGLENABLECLIENTSTATEINDEXEDEXTPROC __glewEnableClientStateIndexedEXT;
GLEW_FUN_EXPORT PFNGLENABLECLIENTSTATEIEXTPROC __glewEnableClientStateiEXT;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXARRAYATTRIBEXTPROC __glewEnableVertexArrayAttribEXT;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXARRAYEXTPROC __glewEnableVertexArrayEXT;
GLEW_FUN_EXPORT PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEEXTPROC __glewFlushMappedNamedBufferRangeEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC __glewFramebufferDrawBufferEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC __glewFramebufferDrawBuffersEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERREADBUFFEREXTPROC __glewFramebufferReadBufferEXT;
GLEW_FUN_EXPORT PFNGLGENERATEMULTITEXMIPMAPEXTPROC __glewGenerateMultiTexMipmapEXT;
GLEW_FUN_EXPORT PFNGLGENERATETEXTUREMIPMAPEXTPROC __glewGenerateTextureMipmapEXT;
GLEW_FUN_EXPORT PFNGLGETCOMPRESSEDMULTITEXIMAGEEXTPROC __glewGetCompressedMultiTexImageEXT;
GLEW_FUN_EXPORT PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC __glewGetCompressedTextureImageEXT;
GLEW_FUN_EXPORT PFNGLGETDOUBLEINDEXEDVEXTPROC __glewGetDoubleIndexedvEXT;
GLEW_FUN_EXPORT PFNGLGETDOUBLEI_VEXTPROC __glewGetDoublei_vEXT;
GLEW_FUN_EXPORT PFNGLGETFLOATINDEXEDVEXTPROC __glewGetFloatIndexedvEXT;
GLEW_FUN_EXPORT PFNGLGETFLOATI_VEXTPROC __glewGetFloati_vEXT;
GLEW_FUN_EXPORT PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC __glewGetFramebufferParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXENVFVEXTPROC __glewGetMultiTexEnvfvEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXENVIVEXTPROC __glewGetMultiTexEnvivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXGENDVEXTPROC __glewGetMultiTexGendvEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXGENFVEXTPROC __glewGetMultiTexGenfvEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXGENIVEXTPROC __glewGetMultiTexGenivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXIMAGEEXTPROC __glewGetMultiTexImageEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXLEVELPARAMETERFVEXTPROC __glewGetMultiTexLevelParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXLEVELPARAMETERIVEXTPROC __glewGetMultiTexLevelParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXPARAMETERIIVEXTPROC __glewGetMultiTexParameterIivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXPARAMETERIUIVEXTPROC __glewGetMultiTexParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXPARAMETERFVEXTPROC __glewGetMultiTexParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETMULTITEXPARAMETERIVEXTPROC __glewGetMultiTexParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC __glewGetNamedBufferParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDBUFFERPOINTERVEXTPROC __glewGetNamedBufferPointervEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDBUFFERSUBDATAEXTPROC __glewGetNamedBufferSubDataEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC __glewGetNamedFramebufferAttachmentParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMLOCALPARAMETERIIVEXTPROC __glewGetNamedProgramLocalParameterIivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMLOCALPARAMETERIUIVEXTPROC __glewGetNamedProgramLocalParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMLOCALPARAMETERDVEXTPROC __glewGetNamedProgramLocalParameterdvEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMLOCALPARAMETERFVEXTPROC __glewGetNamedProgramLocalParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMSTRINGEXTPROC __glewGetNamedProgramStringEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDPROGRAMIVEXTPROC __glewGetNamedProgramivEXT;
GLEW_FUN_EXPORT PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC __glewGetNamedRenderbufferParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETPOINTERINDEXEDVEXTPROC __glewGetPointerIndexedvEXT;
GLEW_FUN_EXPORT PFNGLGETPOINTERI_VEXTPROC __glewGetPointeri_vEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTUREIMAGEEXTPROC __glewGetTextureImageEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC __glewGetTextureLevelParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC __glewGetTextureLevelParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTUREPARAMETERIIVEXTPROC __glewGetTextureParameterIivEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTUREPARAMETERIUIVEXTPROC __glewGetTextureParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTUREPARAMETERFVEXTPROC __glewGetTextureParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETTEXTUREPARAMETERIVEXTPROC __glewGetTextureParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXARRAYINTEGERI_VEXTPROC __glewGetVertexArrayIntegeri_vEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXARRAYINTEGERVEXTPROC __glewGetVertexArrayIntegervEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC __glewGetVertexArrayPointeri_vEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXARRAYPOINTERVEXTPROC __glewGetVertexArrayPointervEXT;
GLEW_FUN_EXPORT PFNGLMAPNAMEDBUFFEREXTPROC __glewMapNamedBufferEXT;
GLEW_FUN_EXPORT PFNGLMAPNAMEDBUFFERRANGEEXTPROC __glewMapNamedBufferRangeEXT;
GLEW_FUN_EXPORT PFNGLMATRIXFRUSTUMEXTPROC __glewMatrixFrustumEXT;
GLEW_FUN_EXPORT PFNGLMATRIXLOADIDENTITYEXTPROC __glewMatrixLoadIdentityEXT;
GLEW_FUN_EXPORT PFNGLMATRIXLOADTRANSPOSEDEXTPROC __glewMatrixLoadTransposedEXT;
GLEW_FUN_EXPORT PFNGLMATRIXLOADTRANSPOSEFEXTPROC __glewMatrixLoadTransposefEXT;
GLEW_FUN_EXPORT PFNGLMATRIXLOADDEXTPROC __glewMatrixLoaddEXT;
GLEW_FUN_EXPORT PFNGLMATRIXLOADFEXTPROC __glewMatrixLoadfEXT;
GLEW_FUN_EXPORT PFNGLMATRIXMULTTRANSPOSEDEXTPROC __glewMatrixMultTransposedEXT;
GLEW_FUN_EXPORT PFNGLMATRIXMULTTRANSPOSEFEXTPROC __glewMatrixMultTransposefEXT;
GLEW_FUN_EXPORT PFNGLMATRIXMULTDEXTPROC __glewMatrixMultdEXT;
GLEW_FUN_EXPORT PFNGLMATRIXMULTFEXTPROC __glewMatrixMultfEXT;
GLEW_FUN_EXPORT PFNGLMATRIXORTHOEXTPROC __glewMatrixOrthoEXT;
GLEW_FUN_EXPORT PFNGLMATRIXPOPEXTPROC __glewMatrixPopEXT;
GLEW_FUN_EXPORT PFNGLMATRIXPUSHEXTPROC __glewMatrixPushEXT;
GLEW_FUN_EXPORT PFNGLMATRIXROTATEDEXTPROC __glewMatrixRotatedEXT;
GLEW_FUN_EXPORT PFNGLMATRIXROTATEFEXTPROC __glewMatrixRotatefEXT;
GLEW_FUN_EXPORT PFNGLMATRIXSCALEDEXTPROC __glewMatrixScaledEXT;
GLEW_FUN_EXPORT PFNGLMATRIXSCALEFEXTPROC __glewMatrixScalefEXT;
GLEW_FUN_EXPORT PFNGLMATRIXTRANSLATEDEXTPROC __glewMatrixTranslatedEXT;
GLEW_FUN_EXPORT PFNGLMATRIXTRANSLATEFEXTPROC __glewMatrixTranslatefEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXBUFFEREXTPROC __glewMultiTexBufferEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORDPOINTEREXTPROC __glewMultiTexCoordPointerEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXENVFEXTPROC __glewMultiTexEnvfEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXENVFVEXTPROC __glewMultiTexEnvfvEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXENVIEXTPROC __glewMultiTexEnviEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXENVIVEXTPROC __glewMultiTexEnvivEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENDEXTPROC __glewMultiTexGendEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENDVEXTPROC __glewMultiTexGendvEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENFEXTPROC __glewMultiTexGenfEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENFVEXTPROC __glewMultiTexGenfvEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENIEXTPROC __glewMultiTexGeniEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXGENIVEXTPROC __glewMultiTexGenivEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXIMAGE1DEXTPROC __glewMultiTexImage1DEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXIMAGE2DEXTPROC __glewMultiTexImage2DEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXIMAGE3DEXTPROC __glewMultiTexImage3DEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERIIVEXTPROC __glewMultiTexParameterIivEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERIUIVEXTPROC __glewMultiTexParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERFEXTPROC __glewMultiTexParameterfEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERFVEXTPROC __glewMultiTexParameterfvEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERIEXTPROC __glewMultiTexParameteriEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXPARAMETERIVEXTPROC __glewMultiTexParameterivEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXRENDERBUFFEREXTPROC __glewMultiTexRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXSUBIMAGE1DEXTPROC __glewMultiTexSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXSUBIMAGE2DEXTPROC __glewMultiTexSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLMULTITEXSUBIMAGE3DEXTPROC __glewMultiTexSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLNAMEDBUFFERDATAEXTPROC __glewNamedBufferDataEXT;
GLEW_FUN_EXPORT PFNGLNAMEDBUFFERSUBDATAEXTPROC __glewNamedBufferSubDataEXT;
GLEW_FUN_EXPORT PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC __glewNamedCopyBufferSubDataEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC __glewNamedFramebufferRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC __glewNamedFramebufferTexture1DEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC __glewNamedFramebufferTexture2DEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC __glewNamedFramebufferTexture3DEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC __glewNamedFramebufferTextureEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC __glewNamedFramebufferTextureFaceEXT;
GLEW_FUN_EXPORT PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC __glewNamedFramebufferTextureLayerEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETER4DEXTPROC __glewNamedProgramLocalParameter4dEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETER4DVEXTPROC __glewNamedProgramLocalParameter4dvEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETER4FEXTPROC __glewNamedProgramLocalParameter4fEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETER4FVEXTPROC __glewNamedProgramLocalParameter4fvEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERI4IEXTPROC __glewNamedProgramLocalParameterI4iEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERI4IVEXTPROC __glewNamedProgramLocalParameterI4ivEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIEXTPROC __glewNamedProgramLocalParameterI4uiEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERI4UIVEXTPROC __glewNamedProgramLocalParameterI4uivEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERS4FVEXTPROC __glewNamedProgramLocalParameters4fvEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERSI4IVEXTPROC __glewNamedProgramLocalParametersI4ivEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMLOCALPARAMETERSI4UIVEXTPROC __glewNamedProgramLocalParametersI4uivEXT;
GLEW_FUN_EXPORT PFNGLNAMEDPROGRAMSTRINGEXTPROC __glewNamedProgramStringEXT;
GLEW_FUN_EXPORT PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC __glewNamedRenderbufferStorageEXT;
GLEW_FUN_EXPORT PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC __glewNamedRenderbufferStorageMultisampleCoverageEXT;
GLEW_FUN_EXPORT PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __glewNamedRenderbufferStorageMultisampleEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FEXTPROC __glewProgramUniform1fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FVEXTPROC __glewProgramUniform1fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IEXTPROC __glewProgramUniform1iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IVEXTPROC __glewProgramUniform1ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UIEXTPROC __glewProgramUniform1uiEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UIVEXTPROC __glewProgramUniform1uivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FEXTPROC __glewProgramUniform2fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FVEXTPROC __glewProgramUniform2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IEXTPROC __glewProgramUniform2iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IVEXTPROC __glewProgramUniform2ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UIEXTPROC __glewProgramUniform2uiEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UIVEXTPROC __glewProgramUniform2uivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FEXTPROC __glewProgramUniform3fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FVEXTPROC __glewProgramUniform3fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IEXTPROC __glewProgramUniform3iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IVEXTPROC __glewProgramUniform3ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UIEXTPROC __glewProgramUniform3uiEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UIVEXTPROC __glewProgramUniform3uivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FEXTPROC __glewProgramUniform4fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FVEXTPROC __glewProgramUniform4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IEXTPROC __glewProgramUniform4iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IVEXTPROC __glewProgramUniform4ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UIEXTPROC __glewProgramUniform4uiEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UIVEXTPROC __glewProgramUniform4uivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC __glewProgramUniformMatrix2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC __glewProgramUniformMatrix2x3fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC __glewProgramUniformMatrix2x4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC __glewProgramUniformMatrix3fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC __glewProgramUniformMatrix3x2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC __glewProgramUniformMatrix3x4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC __glewProgramUniformMatrix4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC __glewProgramUniformMatrix4x2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC __glewProgramUniformMatrix4x3fvEXT;
GLEW_FUN_EXPORT PFNGLPUSHCLIENTATTRIBDEFAULTEXTPROC __glewPushClientAttribDefaultEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREBUFFEREXTPROC __glewTextureBufferEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREIMAGE1DEXTPROC __glewTextureImage1DEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREIMAGE2DEXTPROC __glewTextureImage2DEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREIMAGE3DEXTPROC __glewTextureImage3DEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERIIVEXTPROC __glewTextureParameterIivEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERIUIVEXTPROC __glewTextureParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERFEXTPROC __glewTextureParameterfEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERFVEXTPROC __glewTextureParameterfvEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERIEXTPROC __glewTextureParameteriEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREPARAMETERIVEXTPROC __glewTextureParameterivEXT;
GLEW_FUN_EXPORT PFNGLTEXTURERENDERBUFFEREXTPROC __glewTextureRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESUBIMAGE1DEXTPROC __glewTextureSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESUBIMAGE2DEXTPROC __glewTextureSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESUBIMAGE3DEXTPROC __glewTextureSubImage3DEXT;
GLEW_FUN_EXPORT PFNGLUNMAPNAMEDBUFFEREXTPROC __glewUnmapNamedBufferEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYCOLOROFFSETEXTPROC __glewVertexArrayColorOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYEDGEFLAGOFFSETEXTPROC __glewVertexArrayEdgeFlagOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYFOGCOORDOFFSETEXTPROC __glewVertexArrayFogCoordOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYINDEXOFFSETEXTPROC __glewVertexArrayIndexOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYMULTITEXCOORDOFFSETEXTPROC __glewVertexArrayMultiTexCoordOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYNORMALOFFSETEXTPROC __glewVertexArrayNormalOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYSECONDARYCOLOROFFSETEXTPROC __glewVertexArraySecondaryColorOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYTEXCOORDOFFSETEXTPROC __glewVertexArrayTexCoordOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC __glewVertexArrayVertexAttribIOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC __glewVertexArrayVertexAttribOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYVERTEXOFFSETEXTPROC __glewVertexArrayVertexOffsetEXT;

GLEW_FUN_EXPORT PFNGLCOLORMASKINDEXEDEXTPROC __glewColorMaskIndexedEXT;
GLEW_FUN_EXPORT PFNGLDISABLEINDEXEDEXTPROC __glewDisableIndexedEXT;
GLEW_FUN_EXPORT PFNGLENABLEINDEXEDEXTPROC __glewEnableIndexedEXT;
GLEW_FUN_EXPORT PFNGLGETBOOLEANINDEXEDVEXTPROC __glewGetBooleanIndexedvEXT;
GLEW_FUN_EXPORT PFNGLGETINTEGERINDEXEDVEXTPROC __glewGetIntegerIndexedvEXT;
GLEW_FUN_EXPORT PFNGLISENABLEDINDEXEDEXTPROC __glewIsEnabledIndexedEXT;

GLEW_FUN_EXPORT PFNGLDRAWARRAYSINSTANCEDEXTPROC __glewDrawArraysInstancedEXT;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINSTANCEDEXTPROC __glewDrawElementsInstancedEXT;

GLEW_FUN_EXPORT PFNGLDRAWRANGEELEMENTSEXTPROC __glewDrawRangeElementsEXT;

GLEW_FUN_EXPORT PFNGLFOGCOORDPOINTEREXTPROC __glewFogCoordPointerEXT;
GLEW_FUN_EXPORT PFNGLFOGCOORDDEXTPROC __glewFogCoorddEXT;
GLEW_FUN_EXPORT PFNGLFOGCOORDDVEXTPROC __glewFogCoorddvEXT;
GLEW_FUN_EXPORT PFNGLFOGCOORDFEXTPROC __glewFogCoordfEXT;
GLEW_FUN_EXPORT PFNGLFOGCOORDFVEXTPROC __glewFogCoordfvEXT;

GLEW_FUN_EXPORT PFNGLFRAGMENTCOLORMATERIALEXTPROC __glewFragmentColorMaterialEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELFEXTPROC __glewFragmentLightModelfEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELFVEXTPROC __glewFragmentLightModelfvEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELIEXTPROC __glewFragmentLightModeliEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELIVEXTPROC __glewFragmentLightModelivEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTFEXTPROC __glewFragmentLightfEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTFVEXTPROC __glewFragmentLightfvEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTIEXTPROC __glewFragmentLightiEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTIVEXTPROC __glewFragmentLightivEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALFEXTPROC __glewFragmentMaterialfEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALFVEXTPROC __glewFragmentMaterialfvEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALIEXTPROC __glewFragmentMaterialiEXT;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALIVEXTPROC __glewFragmentMaterialivEXT;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTLIGHTFVEXTPROC __glewGetFragmentLightfvEXT;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTLIGHTIVEXTPROC __glewGetFragmentLightivEXT;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTMATERIALFVEXTPROC __glewGetFragmentMaterialfvEXT;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTMATERIALIVEXTPROC __glewGetFragmentMaterialivEXT;
GLEW_FUN_EXPORT PFNGLLIGHTENVIEXTPROC __glewLightEnviEXT;

GLEW_FUN_EXPORT PFNGLBLITFRAMEBUFFEREXTPROC __glewBlitFramebufferEXT;

GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __glewRenderbufferStorageMultisampleEXT;

GLEW_FUN_EXPORT PFNGLBINDFRAMEBUFFEREXTPROC __glewBindFramebufferEXT;
GLEW_FUN_EXPORT PFNGLBINDRENDERBUFFEREXTPROC __glewBindRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC __glewCheckFramebufferStatusEXT;
GLEW_FUN_EXPORT PFNGLDELETEFRAMEBUFFERSEXTPROC __glewDeleteFramebuffersEXT;
GLEW_FUN_EXPORT PFNGLDELETERENDERBUFFERSEXTPROC __glewDeleteRenderbuffersEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC __glewFramebufferRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE1DEXTPROC __glewFramebufferTexture1DEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DEXTPROC __glewFramebufferTexture2DEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE3DEXTPROC __glewFramebufferTexture3DEXT;
GLEW_FUN_EXPORT PFNGLGENFRAMEBUFFERSEXTPROC __glewGenFramebuffersEXT;
GLEW_FUN_EXPORT PFNGLGENRENDERBUFFERSEXTPROC __glewGenRenderbuffersEXT;
GLEW_FUN_EXPORT PFNGLGENERATEMIPMAPEXTPROC __glewGenerateMipmapEXT;
GLEW_FUN_EXPORT PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC __glewGetFramebufferAttachmentParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC __glewGetRenderbufferParameterivEXT;
GLEW_FUN_EXPORT PFNGLISFRAMEBUFFEREXTPROC __glewIsFramebufferEXT;
GLEW_FUN_EXPORT PFNGLISRENDERBUFFEREXTPROC __glewIsRenderbufferEXT;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEEXTPROC __glewRenderbufferStorageEXT;

GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTUREEXTPROC __glewFramebufferTextureEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTUREFACEEXTPROC __glewFramebufferTextureFaceEXT;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURELAYEREXTPROC __glewFramebufferTextureLayerEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERIEXTPROC __glewProgramParameteriEXT;

GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERS4FVEXTPROC __glewProgramEnvParameters4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC __glewProgramLocalParameters4fvEXT;

GLEW_FUN_EXPORT PFNGLBINDFRAGDATALOCATIONEXTPROC __glewBindFragDataLocationEXT;
GLEW_FUN_EXPORT PFNGLGETFRAGDATALOCATIONEXTPROC __glewGetFragDataLocationEXT;
GLEW_FUN_EXPORT PFNGLGETUNIFORMUIVEXTPROC __glewGetUniformuivEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIIVEXTPROC __glewGetVertexAttribIivEXT;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIUIVEXTPROC __glewGetVertexAttribIuivEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM1UIEXTPROC __glewUniform1uiEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM1UIVEXTPROC __glewUniform1uivEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM2UIEXTPROC __glewUniform2uiEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM2UIVEXTPROC __glewUniform2uivEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM3UIEXTPROC __glewUniform3uiEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM3UIVEXTPROC __glewUniform3uivEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM4UIEXTPROC __glewUniform4uiEXT;
GLEW_FUN_EXPORT PFNGLUNIFORM4UIVEXTPROC __glewUniform4uivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1IEXTPROC __glewVertexAttribI1iEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1IVEXTPROC __glewVertexAttribI1ivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1UIEXTPROC __glewVertexAttribI1uiEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI1UIVEXTPROC __glewVertexAttribI1uivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2IEXTPROC __glewVertexAttribI2iEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2IVEXTPROC __glewVertexAttribI2ivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2UIEXTPROC __glewVertexAttribI2uiEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI2UIVEXTPROC __glewVertexAttribI2uivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3IEXTPROC __glewVertexAttribI3iEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3IVEXTPROC __glewVertexAttribI3ivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3UIEXTPROC __glewVertexAttribI3uiEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI3UIVEXTPROC __glewVertexAttribI3uivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4BVEXTPROC __glewVertexAttribI4bvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4IEXTPROC __glewVertexAttribI4iEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4IVEXTPROC __glewVertexAttribI4ivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4SVEXTPROC __glewVertexAttribI4svEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UBVEXTPROC __glewVertexAttribI4ubvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UIEXTPROC __glewVertexAttribI4uiEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4UIVEXTPROC __glewVertexAttribI4uivEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBI4USVEXTPROC __glewVertexAttribI4usvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBIPOINTEREXTPROC __glewVertexAttribIPointerEXT;

GLEW_FUN_EXPORT PFNGLGETHISTOGRAMEXTPROC __glewGetHistogramEXT;
GLEW_FUN_EXPORT PFNGLGETHISTOGRAMPARAMETERFVEXTPROC __glewGetHistogramParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETHISTOGRAMPARAMETERIVEXTPROC __glewGetHistogramParameterivEXT;
GLEW_FUN_EXPORT PFNGLGETMINMAXEXTPROC __glewGetMinmaxEXT;
GLEW_FUN_EXPORT PFNGLGETMINMAXPARAMETERFVEXTPROC __glewGetMinmaxParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETMINMAXPARAMETERIVEXTPROC __glewGetMinmaxParameterivEXT;
GLEW_FUN_EXPORT PFNGLHISTOGRAMEXTPROC __glewHistogramEXT;
GLEW_FUN_EXPORT PFNGLMINMAXEXTPROC __glewMinmaxEXT;
GLEW_FUN_EXPORT PFNGLRESETHISTOGRAMEXTPROC __glewResetHistogramEXT;
GLEW_FUN_EXPORT PFNGLRESETMINMAXEXTPROC __glewResetMinmaxEXT;

GLEW_FUN_EXPORT PFNGLINDEXFUNCEXTPROC __glewIndexFuncEXT;

GLEW_FUN_EXPORT PFNGLINDEXMATERIALEXTPROC __glewIndexMaterialEXT;

GLEW_FUN_EXPORT PFNGLAPPLYTEXTUREEXTPROC __glewApplyTextureEXT;
GLEW_FUN_EXPORT PFNGLTEXTURELIGHTEXTPROC __glewTextureLightEXT;
GLEW_FUN_EXPORT PFNGLTEXTUREMATERIALEXTPROC __glewTextureMaterialEXT;

GLEW_FUN_EXPORT PFNGLMULTIDRAWARRAYSEXTPROC __glewMultiDrawArraysEXT;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTSEXTPROC __glewMultiDrawElementsEXT;

GLEW_FUN_EXPORT PFNGLSAMPLEMASKEXTPROC __glewSampleMaskEXT;
GLEW_FUN_EXPORT PFNGLSAMPLEPATTERNEXTPROC __glewSamplePatternEXT;

GLEW_FUN_EXPORT PFNGLCOLORTABLEEXTPROC __glewColorTableEXT;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEEXTPROC __glewGetColorTableEXT;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERFVEXTPROC __glewGetColorTableParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERIVEXTPROC __glewGetColorTableParameterivEXT;

GLEW_FUN_EXPORT PFNGLGETPIXELTRANSFORMPARAMETERFVEXTPROC __glewGetPixelTransformParameterfvEXT;
GLEW_FUN_EXPORT PFNGLGETPIXELTRANSFORMPARAMETERIVEXTPROC __glewGetPixelTransformParameterivEXT;
GLEW_FUN_EXPORT PFNGLPIXELTRANSFORMPARAMETERFEXTPROC __glewPixelTransformParameterfEXT;
GLEW_FUN_EXPORT PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC __glewPixelTransformParameterfvEXT;
GLEW_FUN_EXPORT PFNGLPIXELTRANSFORMPARAMETERIEXTPROC __glewPixelTransformParameteriEXT;
GLEW_FUN_EXPORT PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC __glewPixelTransformParameterivEXT;

GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFEXTPROC __glewPointParameterfEXT;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFVEXTPROC __glewPointParameterfvEXT;

GLEW_FUN_EXPORT PFNGLPOLYGONOFFSETEXTPROC __glewPolygonOffsetEXT;

GLEW_FUN_EXPORT PFNGLPROVOKINGVERTEXEXTPROC __glewProvokingVertexEXT;

GLEW_FUN_EXPORT PFNGLBEGINSCENEEXTPROC __glewBeginSceneEXT;
GLEW_FUN_EXPORT PFNGLENDSCENEEXTPROC __glewEndSceneEXT;

GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3BEXTPROC __glewSecondaryColor3bEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3BVEXTPROC __glewSecondaryColor3bvEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3DEXTPROC __glewSecondaryColor3dEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3DVEXTPROC __glewSecondaryColor3dvEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3FEXTPROC __glewSecondaryColor3fEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3FVEXTPROC __glewSecondaryColor3fvEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3IEXTPROC __glewSecondaryColor3iEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3IVEXTPROC __glewSecondaryColor3ivEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3SEXTPROC __glewSecondaryColor3sEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3SVEXTPROC __glewSecondaryColor3svEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UBEXTPROC __glewSecondaryColor3ubEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UBVEXTPROC __glewSecondaryColor3ubvEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UIEXTPROC __glewSecondaryColor3uiEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3UIVEXTPROC __glewSecondaryColor3uivEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3USEXTPROC __glewSecondaryColor3usEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3USVEXTPROC __glewSecondaryColor3usvEXT;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORPOINTEREXTPROC __glewSecondaryColorPointerEXT;

GLEW_FUN_EXPORT PFNGLACTIVEPROGRAMEXTPROC __glewActiveProgramEXT;
GLEW_FUN_EXPORT PFNGLCREATESHADERPROGRAMEXTPROC __glewCreateShaderProgramEXT;
GLEW_FUN_EXPORT PFNGLUSESHADERPROGRAMEXTPROC __glewUseShaderProgramEXT;

GLEW_FUN_EXPORT PFNGLBINDIMAGETEXTUREEXTPROC __glewBindImageTextureEXT;
GLEW_FUN_EXPORT PFNGLMEMORYBARRIEREXTPROC __glewMemoryBarrierEXT;

GLEW_FUN_EXPORT PFNGLACTIVESTENCILFACEEXTPROC __glewActiveStencilFaceEXT;

GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE1DEXTPROC __glewTexSubImage1DEXT;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE2DEXTPROC __glewTexSubImage2DEXT;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE3DEXTPROC __glewTexSubImage3DEXT;

GLEW_FUN_EXPORT PFNGLTEXIMAGE3DEXTPROC __glewTexImage3DEXT;

GLEW_FUN_EXPORT PFNGLTEXBUFFEREXTPROC __glewTexBufferEXT;

GLEW_FUN_EXPORT PFNGLCLEARCOLORIIEXTPROC __glewClearColorIiEXT;
GLEW_FUN_EXPORT PFNGLCLEARCOLORIUIEXTPROC __glewClearColorIuiEXT;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERIIVEXTPROC __glewGetTexParameterIivEXT;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERIUIVEXTPROC __glewGetTexParameterIuivEXT;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIIVEXTPROC __glewTexParameterIivEXT;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIUIVEXTPROC __glewTexParameterIuivEXT;

GLEW_FUN_EXPORT PFNGLARETEXTURESRESIDENTEXTPROC __glewAreTexturesResidentEXT;
GLEW_FUN_EXPORT PFNGLBINDTEXTUREEXTPROC __glewBindTextureEXT;
GLEW_FUN_EXPORT PFNGLDELETETEXTURESEXTPROC __glewDeleteTexturesEXT;
GLEW_FUN_EXPORT PFNGLGENTEXTURESEXTPROC __glewGenTexturesEXT;
GLEW_FUN_EXPORT PFNGLISTEXTUREEXTPROC __glewIsTextureEXT;
GLEW_FUN_EXPORT PFNGLPRIORITIZETEXTURESEXTPROC __glewPrioritizeTexturesEXT;

GLEW_FUN_EXPORT PFNGLTEXTURENORMALEXTPROC __glewTextureNormalEXT;

GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTI64VEXTPROC __glewGetQueryObjecti64vEXT;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTUI64VEXTPROC __glewGetQueryObjectui64vEXT;

GLEW_FUN_EXPORT PFNGLBEGINTRANSFORMFEEDBACKEXTPROC __glewBeginTransformFeedbackEXT;
GLEW_FUN_EXPORT PFNGLBINDBUFFERBASEEXTPROC __glewBindBufferBaseEXT;
GLEW_FUN_EXPORT PFNGLBINDBUFFEROFFSETEXTPROC __glewBindBufferOffsetEXT;
GLEW_FUN_EXPORT PFNGLBINDBUFFERRANGEEXTPROC __glewBindBufferRangeEXT;
GLEW_FUN_EXPORT PFNGLENDTRANSFORMFEEDBACKEXTPROC __glewEndTransformFeedbackEXT;
GLEW_FUN_EXPORT PFNGLGETTRANSFORMFEEDBACKVARYINGEXTPROC __glewGetTransformFeedbackVaryingEXT;
GLEW_FUN_EXPORT PFNGLTRANSFORMFEEDBACKVARYINGSEXTPROC __glewTransformFeedbackVaryingsEXT;

GLEW_FUN_EXPORT PFNGLARRAYELEMENTEXTPROC __glewArrayElementEXT;
GLEW_FUN_EXPORT PFNGLCOLORPOINTEREXTPROC __glewColorPointerEXT;
GLEW_FUN_EXPORT PFNGLDRAWARRAYSEXTPROC __glewDrawArraysEXT;
GLEW_FUN_EXPORT PFNGLEDGEFLAGPOINTEREXTPROC __glewEdgeFlagPointerEXT;
GLEW_FUN_EXPORT PFNGLGETPOINTERVEXTPROC __glewGetPointervEXT;
GLEW_FUN_EXPORT PFNGLINDEXPOINTEREXTPROC __glewIndexPointerEXT;
GLEW_FUN_EXPORT PFNGLNORMALPOINTEREXTPROC __glewNormalPointerEXT;
GLEW_FUN_EXPORT PFNGLTEXCOORDPOINTEREXTPROC __glewTexCoordPointerEXT;
GLEW_FUN_EXPORT PFNGLVERTEXPOINTEREXTPROC __glewVertexPointerEXT;

GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBLDVEXTPROC __glewGetVertexAttribLdvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYVERTEXATTRIBLOFFSETEXTPROC __glewVertexArrayVertexAttribLOffsetEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1DEXTPROC __glewVertexAttribL1dEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1DVEXTPROC __glewVertexAttribL1dvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2DEXTPROC __glewVertexAttribL2dEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2DVEXTPROC __glewVertexAttribL2dvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3DEXTPROC __glewVertexAttribL3dEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3DVEXTPROC __glewVertexAttribL3dvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4DEXTPROC __glewVertexAttribL4dEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4DVEXTPROC __glewVertexAttribL4dvEXT;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBLPOINTEREXTPROC __glewVertexAttribLPointerEXT;

GLEW_FUN_EXPORT PFNGLBEGINVERTEXSHADEREXTPROC __glewBeginVertexShaderEXT;
GLEW_FUN_EXPORT PFNGLBINDLIGHTPARAMETEREXTPROC __glewBindLightParameterEXT;
GLEW_FUN_EXPORT PFNGLBINDMATERIALPARAMETEREXTPROC __glewBindMaterialParameterEXT;
GLEW_FUN_EXPORT PFNGLBINDPARAMETEREXTPROC __glewBindParameterEXT;
GLEW_FUN_EXPORT PFNGLBINDTEXGENPARAMETEREXTPROC __glewBindTexGenParameterEXT;
GLEW_FUN_EXPORT PFNGLBINDTEXTUREUNITPARAMETEREXTPROC __glewBindTextureUnitParameterEXT;
GLEW_FUN_EXPORT PFNGLBINDVERTEXSHADEREXTPROC __glewBindVertexShaderEXT;
GLEW_FUN_EXPORT PFNGLDELETEVERTEXSHADEREXTPROC __glewDeleteVertexShaderEXT;
GLEW_FUN_EXPORT PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC __glewDisableVariantClientStateEXT;
GLEW_FUN_EXPORT PFNGLENABLEVARIANTCLIENTSTATEEXTPROC __glewEnableVariantClientStateEXT;
GLEW_FUN_EXPORT PFNGLENDVERTEXSHADEREXTPROC __glewEndVertexShaderEXT;
GLEW_FUN_EXPORT PFNGLEXTRACTCOMPONENTEXTPROC __glewExtractComponentEXT;
GLEW_FUN_EXPORT PFNGLGENSYMBOLSEXTPROC __glewGenSymbolsEXT;
GLEW_FUN_EXPORT PFNGLGENVERTEXSHADERSEXTPROC __glewGenVertexShadersEXT;
GLEW_FUN_EXPORT PFNGLGETINVARIANTBOOLEANVEXTPROC __glewGetInvariantBooleanvEXT;
GLEW_FUN_EXPORT PFNGLGETINVARIANTFLOATVEXTPROC __glewGetInvariantFloatvEXT;
GLEW_FUN_EXPORT PFNGLGETINVARIANTINTEGERVEXTPROC __glewGetInvariantIntegervEXT;
GLEW_FUN_EXPORT PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC __glewGetLocalConstantBooleanvEXT;
GLEW_FUN_EXPORT PFNGLGETLOCALCONSTANTFLOATVEXTPROC __glewGetLocalConstantFloatvEXT;
GLEW_FUN_EXPORT PFNGLGETLOCALCONSTANTINTEGERVEXTPROC __glewGetLocalConstantIntegervEXT;
GLEW_FUN_EXPORT PFNGLGETVARIANTBOOLEANVEXTPROC __glewGetVariantBooleanvEXT;
GLEW_FUN_EXPORT PFNGLGETVARIANTFLOATVEXTPROC __glewGetVariantFloatvEXT;
GLEW_FUN_EXPORT PFNGLGETVARIANTINTEGERVEXTPROC __glewGetVariantIntegervEXT;
GLEW_FUN_EXPORT PFNGLGETVARIANTPOINTERVEXTPROC __glewGetVariantPointervEXT;
GLEW_FUN_EXPORT PFNGLINSERTCOMPONENTEXTPROC __glewInsertComponentEXT;
GLEW_FUN_EXPORT PFNGLISVARIANTENABLEDEXTPROC __glewIsVariantEnabledEXT;
GLEW_FUN_EXPORT PFNGLSETINVARIANTEXTPROC __glewSetInvariantEXT;
GLEW_FUN_EXPORT PFNGLSETLOCALCONSTANTEXTPROC __glewSetLocalConstantEXT;
GLEW_FUN_EXPORT PFNGLSHADEROP1EXTPROC __glewShaderOp1EXT;
GLEW_FUN_EXPORT PFNGLSHADEROP2EXTPROC __glewShaderOp2EXT;
GLEW_FUN_EXPORT PFNGLSHADEROP3EXTPROC __glewShaderOp3EXT;
GLEW_FUN_EXPORT PFNGLSWIZZLEEXTPROC __glewSwizzleEXT;
GLEW_FUN_EXPORT PFNGLVARIANTPOINTEREXTPROC __glewVariantPointerEXT;
GLEW_FUN_EXPORT PFNGLVARIANTBVEXTPROC __glewVariantbvEXT;
GLEW_FUN_EXPORT PFNGLVARIANTDVEXTPROC __glewVariantdvEXT;
GLEW_FUN_EXPORT PFNGLVARIANTFVEXTPROC __glewVariantfvEXT;
GLEW_FUN_EXPORT PFNGLVARIANTIVEXTPROC __glewVariantivEXT;
GLEW_FUN_EXPORT PFNGLVARIANTSVEXTPROC __glewVariantsvEXT;
GLEW_FUN_EXPORT PFNGLVARIANTUBVEXTPROC __glewVariantubvEXT;
GLEW_FUN_EXPORT PFNGLVARIANTUIVEXTPROC __glewVariantuivEXT;
GLEW_FUN_EXPORT PFNGLVARIANTUSVEXTPROC __glewVariantusvEXT;
GLEW_FUN_EXPORT PFNGLWRITEMASKEXTPROC __glewWriteMaskEXT;

GLEW_FUN_EXPORT PFNGLVERTEXWEIGHTPOINTEREXTPROC __glewVertexWeightPointerEXT;
GLEW_FUN_EXPORT PFNGLVERTEXWEIGHTFEXTPROC __glewVertexWeightfEXT;
GLEW_FUN_EXPORT PFNGLVERTEXWEIGHTFVEXTPROC __glewVertexWeightfvEXT;

GLEW_FUN_EXPORT PFNGLFRAMETERMINATORGREMEDYPROC __glewFrameTerminatorGREMEDY;

GLEW_FUN_EXPORT PFNGLSTRINGMARKERGREMEDYPROC __glewStringMarkerGREMEDY;

GLEW_FUN_EXPORT PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC __glewGetImageTransformParameterfvHP;
GLEW_FUN_EXPORT PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC __glewGetImageTransformParameterivHP;
GLEW_FUN_EXPORT PFNGLIMAGETRANSFORMPARAMETERFHPPROC __glewImageTransformParameterfHP;
GLEW_FUN_EXPORT PFNGLIMAGETRANSFORMPARAMETERFVHPPROC __glewImageTransformParameterfvHP;
GLEW_FUN_EXPORT PFNGLIMAGETRANSFORMPARAMETERIHPPROC __glewImageTransformParameteriHP;
GLEW_FUN_EXPORT PFNGLIMAGETRANSFORMPARAMETERIVHPPROC __glewImageTransformParameterivHP;

GLEW_FUN_EXPORT PFNGLMULTIMODEDRAWARRAYSIBMPROC __glewMultiModeDrawArraysIBM;
GLEW_FUN_EXPORT PFNGLMULTIMODEDRAWELEMENTSIBMPROC __glewMultiModeDrawElementsIBM;

GLEW_FUN_EXPORT PFNGLCOLORPOINTERLISTIBMPROC __glewColorPointerListIBM;
GLEW_FUN_EXPORT PFNGLEDGEFLAGPOINTERLISTIBMPROC __glewEdgeFlagPointerListIBM;
GLEW_FUN_EXPORT PFNGLFOGCOORDPOINTERLISTIBMPROC __glewFogCoordPointerListIBM;
GLEW_FUN_EXPORT PFNGLINDEXPOINTERLISTIBMPROC __glewIndexPointerListIBM;
GLEW_FUN_EXPORT PFNGLNORMALPOINTERLISTIBMPROC __glewNormalPointerListIBM;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORPOINTERLISTIBMPROC __glewSecondaryColorPointerListIBM;
GLEW_FUN_EXPORT PFNGLTEXCOORDPOINTERLISTIBMPROC __glewTexCoordPointerListIBM;
GLEW_FUN_EXPORT PFNGLVERTEXPOINTERLISTIBMPROC __glewVertexPointerListIBM;

GLEW_FUN_EXPORT PFNGLCOLORPOINTERVINTELPROC __glewColorPointervINTEL;
GLEW_FUN_EXPORT PFNGLNORMALPOINTERVINTELPROC __glewNormalPointervINTEL;
GLEW_FUN_EXPORT PFNGLTEXCOORDPOINTERVINTELPROC __glewTexCoordPointervINTEL;
GLEW_FUN_EXPORT PFNGLVERTEXPOINTERVINTELPROC __glewVertexPointervINTEL;

GLEW_FUN_EXPORT PFNGLTEXSCISSORFUNCINTELPROC __glewTexScissorFuncINTEL;
GLEW_FUN_EXPORT PFNGLTEXSCISSORINTELPROC __glewTexScissorINTEL;

GLEW_FUN_EXPORT PFNGLBUFFERREGIONENABLEDEXTPROC __glewBufferRegionEnabledEXT;
GLEW_FUN_EXPORT PFNGLDELETEBUFFERREGIONEXTPROC __glewDeleteBufferRegionEXT;
GLEW_FUN_EXPORT PFNGLDRAWBUFFERREGIONEXTPROC __glewDrawBufferRegionEXT;
GLEW_FUN_EXPORT PFNGLNEWBUFFERREGIONEXTPROC __glewNewBufferRegionEXT;
GLEW_FUN_EXPORT PFNGLREADBUFFERREGIONEXTPROC __glewReadBufferRegionEXT;

GLEW_FUN_EXPORT PFNGLRESIZEBUFFERSMESAPROC __glewResizeBuffersMESA;

GLEW_FUN_EXPORT PFNGLWINDOWPOS2DMESAPROC __glewWindowPos2dMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2DVMESAPROC __glewWindowPos2dvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FMESAPROC __glewWindowPos2fMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2FVMESAPROC __glewWindowPos2fvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IMESAPROC __glewWindowPos2iMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2IVMESAPROC __glewWindowPos2ivMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SMESAPROC __glewWindowPos2sMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS2SVMESAPROC __glewWindowPos2svMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DMESAPROC __glewWindowPos3dMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3DVMESAPROC __glewWindowPos3dvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FMESAPROC __glewWindowPos3fMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3FVMESAPROC __glewWindowPos3fvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IMESAPROC __glewWindowPos3iMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3IVMESAPROC __glewWindowPos3ivMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SMESAPROC __glewWindowPos3sMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS3SVMESAPROC __glewWindowPos3svMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4DMESAPROC __glewWindowPos4dMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4DVMESAPROC __glewWindowPos4dvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4FMESAPROC __glewWindowPos4fMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4FVMESAPROC __glewWindowPos4fvMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4IMESAPROC __glewWindowPos4iMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4IVMESAPROC __glewWindowPos4ivMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4SMESAPROC __glewWindowPos4sMESA;
GLEW_FUN_EXPORT PFNGLWINDOWPOS4SVMESAPROC __glewWindowPos4svMESA;

GLEW_FUN_EXPORT PFNGLBEGINCONDITIONALRENDERNVPROC __glewBeginConditionalRenderNV;
GLEW_FUN_EXPORT PFNGLENDCONDITIONALRENDERNVPROC __glewEndConditionalRenderNV;

GLEW_FUN_EXPORT PFNGLCOPYIMAGESUBDATANVPROC __glewCopyImageSubDataNV;

GLEW_FUN_EXPORT PFNGLCLEARDEPTHDNVPROC __glewClearDepthdNV;
GLEW_FUN_EXPORT PFNGLDEPTHBOUNDSDNVPROC __glewDepthBoundsdNV;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEDNVPROC __glewDepthRangedNV;

GLEW_FUN_EXPORT PFNGLEVALMAPSNVPROC __glewEvalMapsNV;
GLEW_FUN_EXPORT PFNGLGETMAPATTRIBPARAMETERFVNVPROC __glewGetMapAttribParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETMAPATTRIBPARAMETERIVNVPROC __glewGetMapAttribParameterivNV;
GLEW_FUN_EXPORT PFNGLGETMAPCONTROLPOINTSNVPROC __glewGetMapControlPointsNV;
GLEW_FUN_EXPORT PFNGLGETMAPPARAMETERFVNVPROC __glewGetMapParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETMAPPARAMETERIVNVPROC __glewGetMapParameterivNV;
GLEW_FUN_EXPORT PFNGLMAPCONTROLPOINTSNVPROC __glewMapControlPointsNV;
GLEW_FUN_EXPORT PFNGLMAPPARAMETERFVNVPROC __glewMapParameterfvNV;
GLEW_FUN_EXPORT PFNGLMAPPARAMETERIVNVPROC __glewMapParameterivNV;

GLEW_FUN_EXPORT PFNGLGETMULTISAMPLEFVNVPROC __glewGetMultisamplefvNV;
GLEW_FUN_EXPORT PFNGLSAMPLEMASKINDEXEDNVPROC __glewSampleMaskIndexedNV;
GLEW_FUN_EXPORT PFNGLTEXRENDERBUFFERNVPROC __glewTexRenderbufferNV;

GLEW_FUN_EXPORT PFNGLDELETEFENCESNVPROC __glewDeleteFencesNV;
GLEW_FUN_EXPORT PFNGLFINISHFENCENVPROC __glewFinishFenceNV;
GLEW_FUN_EXPORT PFNGLGENFENCESNVPROC __glewGenFencesNV;
GLEW_FUN_EXPORT PFNGLGETFENCEIVNVPROC __glewGetFenceivNV;
GLEW_FUN_EXPORT PFNGLISFENCENVPROC __glewIsFenceNV;
GLEW_FUN_EXPORT PFNGLSETFENCENVPROC __glewSetFenceNV;
GLEW_FUN_EXPORT PFNGLTESTFENCENVPROC __glewTestFenceNV;

GLEW_FUN_EXPORT PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC __glewGetProgramNamedParameterdvNV;
GLEW_FUN_EXPORT PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC __glewGetProgramNamedParameterfvNV;
GLEW_FUN_EXPORT PFNGLPROGRAMNAMEDPARAMETER4DNVPROC __glewProgramNamedParameter4dNV;
GLEW_FUN_EXPORT PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC __glewProgramNamedParameter4dvNV;
GLEW_FUN_EXPORT PFNGLPROGRAMNAMEDPARAMETER4FNVPROC __glewProgramNamedParameter4fNV;
GLEW_FUN_EXPORT PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC __glewProgramNamedParameter4fvNV;

GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLECOVERAGENVPROC __glewRenderbufferStorageMultisampleCoverageNV;

GLEW_FUN_EXPORT PFNGLPROGRAMVERTEXLIMITNVPROC __glewProgramVertexLimitNV;

GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERI4INVPROC __glewProgramEnvParameterI4iNV;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERI4IVNVPROC __glewProgramEnvParameterI4ivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERI4UINVPROC __glewProgramEnvParameterI4uiNV;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERI4UIVNVPROC __glewProgramEnvParameterI4uivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERSI4IVNVPROC __glewProgramEnvParametersI4ivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMENVPARAMETERSI4UIVNVPROC __glewProgramEnvParametersI4uivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERI4INVPROC __glewProgramLocalParameterI4iNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERI4IVNVPROC __glewProgramLocalParameterI4ivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERI4UINVPROC __glewProgramLocalParameterI4uiNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERI4UIVNVPROC __glewProgramLocalParameterI4uivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERSI4IVNVPROC __glewProgramLocalParametersI4ivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMLOCALPARAMETERSI4UIVNVPROC __glewProgramLocalParametersI4uivNV;

GLEW_FUN_EXPORT PFNGLGETUNIFORMI64VNVPROC __glewGetUniformi64vNV;
GLEW_FUN_EXPORT PFNGLGETUNIFORMUI64VNVPROC __glewGetUniformui64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1I64NVPROC __glewProgramUniform1i64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1I64VNVPROC __glewProgramUniform1i64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UI64NVPROC __glewProgramUniform1ui64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1UI64VNVPROC __glewProgramUniform1ui64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2I64NVPROC __glewProgramUniform2i64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2I64VNVPROC __glewProgramUniform2i64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UI64NVPROC __glewProgramUniform2ui64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2UI64VNVPROC __glewProgramUniform2ui64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3I64NVPROC __glewProgramUniform3i64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3I64VNVPROC __glewProgramUniform3i64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UI64NVPROC __glewProgramUniform3ui64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3UI64VNVPROC __glewProgramUniform3ui64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4I64NVPROC __glewProgramUniform4i64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4I64VNVPROC __glewProgramUniform4i64vNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UI64NVPROC __glewProgramUniform4ui64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4UI64VNVPROC __glewProgramUniform4ui64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM1I64NVPROC __glewUniform1i64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM1I64VNVPROC __glewUniform1i64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM1UI64NVPROC __glewUniform1ui64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM1UI64VNVPROC __glewUniform1ui64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM2I64NVPROC __glewUniform2i64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM2I64VNVPROC __glewUniform2i64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM2UI64NVPROC __glewUniform2ui64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM2UI64VNVPROC __glewUniform2ui64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM3I64NVPROC __glewUniform3i64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM3I64VNVPROC __glewUniform3i64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM3UI64NVPROC __glewUniform3ui64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM3UI64VNVPROC __glewUniform3ui64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM4I64NVPROC __glewUniform4i64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM4I64VNVPROC __glewUniform4i64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORM4UI64NVPROC __glewUniform4ui64NV;
GLEW_FUN_EXPORT PFNGLUNIFORM4UI64VNVPROC __glewUniform4ui64vNV;

GLEW_FUN_EXPORT PFNGLCOLOR3HNVPROC __glewColor3hNV;
GLEW_FUN_EXPORT PFNGLCOLOR3HVNVPROC __glewColor3hvNV;
GLEW_FUN_EXPORT PFNGLCOLOR4HNVPROC __glewColor4hNV;
GLEW_FUN_EXPORT PFNGLCOLOR4HVNVPROC __glewColor4hvNV;
GLEW_FUN_EXPORT PFNGLFOGCOORDHNVPROC __glewFogCoordhNV;
GLEW_FUN_EXPORT PFNGLFOGCOORDHVNVPROC __glewFogCoordhvNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1HNVPROC __glewMultiTexCoord1hNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD1HVNVPROC __glewMultiTexCoord1hvNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2HNVPROC __glewMultiTexCoord2hNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD2HVNVPROC __glewMultiTexCoord2hvNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3HNVPROC __glewMultiTexCoord3hNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD3HVNVPROC __glewMultiTexCoord3hvNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4HNVPROC __glewMultiTexCoord4hNV;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4HVNVPROC __glewMultiTexCoord4hvNV;
GLEW_FUN_EXPORT PFNGLNORMAL3HNVPROC __glewNormal3hNV;
GLEW_FUN_EXPORT PFNGLNORMAL3HVNVPROC __glewNormal3hvNV;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3HNVPROC __glewSecondaryColor3hNV;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLOR3HVNVPROC __glewSecondaryColor3hvNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD1HNVPROC __glewTexCoord1hNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD1HVNVPROC __glewTexCoord1hvNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD2HNVPROC __glewTexCoord2hNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD2HVNVPROC __glewTexCoord2hvNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD3HNVPROC __glewTexCoord3hNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD3HVNVPROC __glewTexCoord3hvNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD4HNVPROC __glewTexCoord4hNV;
GLEW_FUN_EXPORT PFNGLTEXCOORD4HVNVPROC __glewTexCoord4hvNV;
GLEW_FUN_EXPORT PFNGLVERTEX2HNVPROC __glewVertex2hNV;
GLEW_FUN_EXPORT PFNGLVERTEX2HVNVPROC __glewVertex2hvNV;
GLEW_FUN_EXPORT PFNGLVERTEX3HNVPROC __glewVertex3hNV;
GLEW_FUN_EXPORT PFNGLVERTEX3HVNVPROC __glewVertex3hvNV;
GLEW_FUN_EXPORT PFNGLVERTEX4HNVPROC __glewVertex4hNV;
GLEW_FUN_EXPORT PFNGLVERTEX4HVNVPROC __glewVertex4hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1HNVPROC __glewVertexAttrib1hNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1HVNVPROC __glewVertexAttrib1hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2HNVPROC __glewVertexAttrib2hNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2HVNVPROC __glewVertexAttrib2hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3HNVPROC __glewVertexAttrib3hNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3HVNVPROC __glewVertexAttrib3hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4HNVPROC __glewVertexAttrib4hNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4HVNVPROC __glewVertexAttrib4hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS1HVNVPROC __glewVertexAttribs1hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS2HVNVPROC __glewVertexAttribs2hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS3HVNVPROC __glewVertexAttribs3hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS4HVNVPROC __glewVertexAttribs4hvNV;
GLEW_FUN_EXPORT PFNGLVERTEXWEIGHTHNVPROC __glewVertexWeighthNV;
GLEW_FUN_EXPORT PFNGLVERTEXWEIGHTHVNVPROC __glewVertexWeighthvNV;

GLEW_FUN_EXPORT PFNGLBEGINOCCLUSIONQUERYNVPROC __glewBeginOcclusionQueryNV;
GLEW_FUN_EXPORT PFNGLDELETEOCCLUSIONQUERIESNVPROC __glewDeleteOcclusionQueriesNV;
GLEW_FUN_EXPORT PFNGLENDOCCLUSIONQUERYNVPROC __glewEndOcclusionQueryNV;
GLEW_FUN_EXPORT PFNGLGENOCCLUSIONQUERIESNVPROC __glewGenOcclusionQueriesNV;
GLEW_FUN_EXPORT PFNGLGETOCCLUSIONQUERYIVNVPROC __glewGetOcclusionQueryivNV;
GLEW_FUN_EXPORT PFNGLGETOCCLUSIONQUERYUIVNVPROC __glewGetOcclusionQueryuivNV;
GLEW_FUN_EXPORT PFNGLISOCCLUSIONQUERYNVPROC __glewIsOcclusionQueryNV;

GLEW_FUN_EXPORT PFNGLPROGRAMBUFFERPARAMETERSIIVNVPROC __glewProgramBufferParametersIivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMBUFFERPARAMETERSIUIVNVPROC __glewProgramBufferParametersIuivNV;
GLEW_FUN_EXPORT PFNGLPROGRAMBUFFERPARAMETERSFVNVPROC __glewProgramBufferParametersfvNV;

GLEW_FUN_EXPORT PFNGLFLUSHPIXELDATARANGENVPROC __glewFlushPixelDataRangeNV;
GLEW_FUN_EXPORT PFNGLPIXELDATARANGENVPROC __glewPixelDataRangeNV;

GLEW_FUN_EXPORT PFNGLPOINTPARAMETERINVPROC __glewPointParameteriNV;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERIVNVPROC __glewPointParameterivNV;

GLEW_FUN_EXPORT PFNGLGETVIDEOI64VNVPROC __glewGetVideoi64vNV;
GLEW_FUN_EXPORT PFNGLGETVIDEOIVNVPROC __glewGetVideoivNV;
GLEW_FUN_EXPORT PFNGLGETVIDEOUI64VNVPROC __glewGetVideoui64vNV;
GLEW_FUN_EXPORT PFNGLGETVIDEOUIVNVPROC __glewGetVideouivNV;
GLEW_FUN_EXPORT PFNGLPRESENTFRAMEDUALFILLNVPROC __glewPresentFrameDualFillNV;
GLEW_FUN_EXPORT PFNGLPRESENTFRAMEKEYEDNVPROC __glewPresentFrameKeyedNV;

GLEW_FUN_EXPORT PFNGLPRIMITIVERESTARTINDEXNVPROC __glewPrimitiveRestartIndexNV;
GLEW_FUN_EXPORT PFNGLPRIMITIVERESTARTNVPROC __glewPrimitiveRestartNV;

GLEW_FUN_EXPORT PFNGLCOMBINERINPUTNVPROC __glewCombinerInputNV;
GLEW_FUN_EXPORT PFNGLCOMBINEROUTPUTNVPROC __glewCombinerOutputNV;
GLEW_FUN_EXPORT PFNGLCOMBINERPARAMETERFNVPROC __glewCombinerParameterfNV;
GLEW_FUN_EXPORT PFNGLCOMBINERPARAMETERFVNVPROC __glewCombinerParameterfvNV;
GLEW_FUN_EXPORT PFNGLCOMBINERPARAMETERINVPROC __glewCombinerParameteriNV;
GLEW_FUN_EXPORT PFNGLCOMBINERPARAMETERIVNVPROC __glewCombinerParameterivNV;
GLEW_FUN_EXPORT PFNGLFINALCOMBINERINPUTNVPROC __glewFinalCombinerInputNV;
GLEW_FUN_EXPORT PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC __glewGetCombinerInputParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC __glewGetCombinerInputParameterivNV;
GLEW_FUN_EXPORT PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC __glewGetCombinerOutputParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC __glewGetCombinerOutputParameterivNV;
GLEW_FUN_EXPORT PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC __glewGetFinalCombinerInputParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC __glewGetFinalCombinerInputParameterivNV;

GLEW_FUN_EXPORT PFNGLCOMBINERSTAGEPARAMETERFVNVPROC __glewCombinerStageParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC __glewGetCombinerStageParameterfvNV;

GLEW_FUN_EXPORT PFNGLGETBUFFERPARAMETERUI64VNVPROC __glewGetBufferParameterui64vNV;
GLEW_FUN_EXPORT PFNGLGETINTEGERUI64VNVPROC __glewGetIntegerui64vNV;
GLEW_FUN_EXPORT PFNGLGETNAMEDBUFFERPARAMETERUI64VNVPROC __glewGetNamedBufferParameterui64vNV;
GLEW_FUN_EXPORT PFNGLISBUFFERRESIDENTNVPROC __glewIsBufferResidentNV;
GLEW_FUN_EXPORT PFNGLISNAMEDBUFFERRESIDENTNVPROC __glewIsNamedBufferResidentNV;
GLEW_FUN_EXPORT PFNGLMAKEBUFFERNONRESIDENTNVPROC __glewMakeBufferNonResidentNV;
GLEW_FUN_EXPORT PFNGLMAKEBUFFERRESIDENTNVPROC __glewMakeBufferResidentNV;
GLEW_FUN_EXPORT PFNGLMAKENAMEDBUFFERNONRESIDENTNVPROC __glewMakeNamedBufferNonResidentNV;
GLEW_FUN_EXPORT PFNGLMAKENAMEDBUFFERRESIDENTNVPROC __glewMakeNamedBufferResidentNV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMUI64NVPROC __glewProgramUniformui64NV;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMUI64VNVPROC __glewProgramUniformui64vNV;
GLEW_FUN_EXPORT PFNGLUNIFORMUI64NVPROC __glewUniformui64NV;
GLEW_FUN_EXPORT PFNGLUNIFORMUI64VNVPROC __glewUniformui64vNV;

GLEW_FUN_EXPORT PFNGLTEXTUREBARRIERNVPROC __glewTextureBarrierNV;

GLEW_FUN_EXPORT PFNGLACTIVEVARYINGNVPROC __glewActiveVaryingNV;
GLEW_FUN_EXPORT PFNGLBEGINTRANSFORMFEEDBACKNVPROC __glewBeginTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLBINDBUFFERBASENVPROC __glewBindBufferBaseNV;
GLEW_FUN_EXPORT PFNGLBINDBUFFEROFFSETNVPROC __glewBindBufferOffsetNV;
GLEW_FUN_EXPORT PFNGLBINDBUFFERRANGENVPROC __glewBindBufferRangeNV;
GLEW_FUN_EXPORT PFNGLENDTRANSFORMFEEDBACKNVPROC __glewEndTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLGETACTIVEVARYINGNVPROC __glewGetActiveVaryingNV;
GLEW_FUN_EXPORT PFNGLGETTRANSFORMFEEDBACKVARYINGNVPROC __glewGetTransformFeedbackVaryingNV;
GLEW_FUN_EXPORT PFNGLGETVARYINGLOCATIONNVPROC __glewGetVaryingLocationNV;
GLEW_FUN_EXPORT PFNGLTRANSFORMFEEDBACKATTRIBSNVPROC __glewTransformFeedbackAttribsNV;
GLEW_FUN_EXPORT PFNGLTRANSFORMFEEDBACKVARYINGSNVPROC __glewTransformFeedbackVaryingsNV;

GLEW_FUN_EXPORT PFNGLBINDTRANSFORMFEEDBACKNVPROC __glewBindTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLDELETETRANSFORMFEEDBACKSNVPROC __glewDeleteTransformFeedbacksNV;
GLEW_FUN_EXPORT PFNGLDRAWTRANSFORMFEEDBACKNVPROC __glewDrawTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLGENTRANSFORMFEEDBACKSNVPROC __glewGenTransformFeedbacksNV;
GLEW_FUN_EXPORT PFNGLISTRANSFORMFEEDBACKNVPROC __glewIsTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLPAUSETRANSFORMFEEDBACKNVPROC __glewPauseTransformFeedbackNV;
GLEW_FUN_EXPORT PFNGLRESUMETRANSFORMFEEDBACKNVPROC __glewResumeTransformFeedbackNV;

GLEW_FUN_EXPORT PFNGLVDPAUFININVPROC __glewVDPAUFiniNV;
GLEW_FUN_EXPORT PFNGLVDPAUGETSURFACEIVNVPROC __glewVDPAUGetSurfaceivNV;
GLEW_FUN_EXPORT PFNGLVDPAUINITNVPROC __glewVDPAUInitNV;
GLEW_FUN_EXPORT PFNGLVDPAUISSURFACENVPROC __glewVDPAUIsSurfaceNV;
GLEW_FUN_EXPORT PFNGLVDPAUMAPSURFACESNVPROC __glewVDPAUMapSurfacesNV;
GLEW_FUN_EXPORT PFNGLVDPAUREGISTEROUTPUTSURFACENVPROC __glewVDPAURegisterOutputSurfaceNV;
GLEW_FUN_EXPORT PFNGLVDPAUREGISTERVIDEOSURFACENVPROC __glewVDPAURegisterVideoSurfaceNV;
GLEW_FUN_EXPORT PFNGLVDPAUSURFACEACCESSNVPROC __glewVDPAUSurfaceAccessNV;
GLEW_FUN_EXPORT PFNGLVDPAUUNMAPSURFACESNVPROC __glewVDPAUUnmapSurfacesNV;
GLEW_FUN_EXPORT PFNGLVDPAUUNREGISTERSURFACENVPROC __glewVDPAUUnregisterSurfaceNV;

GLEW_FUN_EXPORT PFNGLFLUSHVERTEXARRAYRANGENVPROC __glewFlushVertexArrayRangeNV;
GLEW_FUN_EXPORT PFNGLVERTEXARRAYRANGENVPROC __glewVertexArrayRangeNV;

GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBLI64VNVPROC __glewGetVertexAttribLi64vNV;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBLUI64VNVPROC __glewGetVertexAttribLui64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1I64NVPROC __glewVertexAttribL1i64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1I64VNVPROC __glewVertexAttribL1i64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1UI64NVPROC __glewVertexAttribL1ui64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL1UI64VNVPROC __glewVertexAttribL1ui64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2I64NVPROC __glewVertexAttribL2i64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2I64VNVPROC __glewVertexAttribL2i64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2UI64NVPROC __glewVertexAttribL2ui64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL2UI64VNVPROC __glewVertexAttribL2ui64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3I64NVPROC __glewVertexAttribL3i64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3I64VNVPROC __glewVertexAttribL3i64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3UI64NVPROC __glewVertexAttribL3ui64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL3UI64VNVPROC __glewVertexAttribL3ui64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4I64NVPROC __glewVertexAttribL4i64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4I64VNVPROC __glewVertexAttribL4i64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4UI64NVPROC __glewVertexAttribL4ui64NV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBL4UI64VNVPROC __glewVertexAttribL4ui64vNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBLFORMATNVPROC __glewVertexAttribLFormatNV;

GLEW_FUN_EXPORT PFNGLBUFFERADDRESSRANGENVPROC __glewBufferAddressRangeNV;
GLEW_FUN_EXPORT PFNGLCOLORFORMATNVPROC __glewColorFormatNV;
GLEW_FUN_EXPORT PFNGLEDGEFLAGFORMATNVPROC __glewEdgeFlagFormatNV;
GLEW_FUN_EXPORT PFNGLFOGCOORDFORMATNVPROC __glewFogCoordFormatNV;
GLEW_FUN_EXPORT PFNGLGETINTEGERUI64I_VNVPROC __glewGetIntegerui64i_vNV;
GLEW_FUN_EXPORT PFNGLINDEXFORMATNVPROC __glewIndexFormatNV;
GLEW_FUN_EXPORT PFNGLNORMALFORMATNVPROC __glewNormalFormatNV;
GLEW_FUN_EXPORT PFNGLSECONDARYCOLORFORMATNVPROC __glewSecondaryColorFormatNV;
GLEW_FUN_EXPORT PFNGLTEXCOORDFORMATNVPROC __glewTexCoordFormatNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBFORMATNVPROC __glewVertexAttribFormatNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBIFORMATNVPROC __glewVertexAttribIFormatNV;
GLEW_FUN_EXPORT PFNGLVERTEXFORMATNVPROC __glewVertexFormatNV;

GLEW_FUN_EXPORT PFNGLAREPROGRAMSRESIDENTNVPROC __glewAreProgramsResidentNV;
GLEW_FUN_EXPORT PFNGLBINDPROGRAMNVPROC __glewBindProgramNV;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMSNVPROC __glewDeleteProgramsNV;
GLEW_FUN_EXPORT PFNGLEXECUTEPROGRAMNVPROC __glewExecuteProgramNV;
GLEW_FUN_EXPORT PFNGLGENPROGRAMSNVPROC __glewGenProgramsNV;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPARAMETERDVNVPROC __glewGetProgramParameterdvNV;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPARAMETERFVNVPROC __glewGetProgramParameterfvNV;
GLEW_FUN_EXPORT PFNGLGETPROGRAMSTRINGNVPROC __glewGetProgramStringNV;
GLEW_FUN_EXPORT PFNGLGETPROGRAMIVNVPROC __glewGetProgramivNV;
GLEW_FUN_EXPORT PFNGLGETTRACKMATRIXIVNVPROC __glewGetTrackMatrixivNV;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBPOINTERVNVPROC __glewGetVertexAttribPointervNV;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBDVNVPROC __glewGetVertexAttribdvNV;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBFVNVPROC __glewGetVertexAttribfvNV;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIVNVPROC __glewGetVertexAttribivNV;
GLEW_FUN_EXPORT PFNGLISPROGRAMNVPROC __glewIsProgramNV;
GLEW_FUN_EXPORT PFNGLLOADPROGRAMNVPROC __glewLoadProgramNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETER4DNVPROC __glewProgramParameter4dNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETER4DVNVPROC __glewProgramParameter4dvNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETER4FNVPROC __glewProgramParameter4fNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETER4FVNVPROC __glewProgramParameter4fvNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERS4DVNVPROC __glewProgramParameters4dvNV;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERS4FVNVPROC __glewProgramParameters4fvNV;
GLEW_FUN_EXPORT PFNGLREQUESTRESIDENTPROGRAMSNVPROC __glewRequestResidentProgramsNV;
GLEW_FUN_EXPORT PFNGLTRACKMATRIXNVPROC __glewTrackMatrixNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DNVPROC __glewVertexAttrib1dNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1DVNVPROC __glewVertexAttrib1dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FNVPROC __glewVertexAttrib1fNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FVNVPROC __glewVertexAttrib1fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SNVPROC __glewVertexAttrib1sNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1SVNVPROC __glewVertexAttrib1svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DNVPROC __glewVertexAttrib2dNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2DVNVPROC __glewVertexAttrib2dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FNVPROC __glewVertexAttrib2fNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FVNVPROC __glewVertexAttrib2fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SNVPROC __glewVertexAttrib2sNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2SVNVPROC __glewVertexAttrib2svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DNVPROC __glewVertexAttrib3dNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3DVNVPROC __glewVertexAttrib3dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FNVPROC __glewVertexAttrib3fNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FVNVPROC __glewVertexAttrib3fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SNVPROC __glewVertexAttrib3sNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3SVNVPROC __glewVertexAttrib3svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DNVPROC __glewVertexAttrib4dNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4DVNVPROC __glewVertexAttrib4dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FNVPROC __glewVertexAttrib4fNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FVNVPROC __glewVertexAttrib4fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SNVPROC __glewVertexAttrib4sNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4SVNVPROC __glewVertexAttrib4svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UBNVPROC __glewVertexAttrib4ubNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4UBVNVPROC __glewVertexAttrib4ubvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBPOINTERNVPROC __glewVertexAttribPointerNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS1DVNVPROC __glewVertexAttribs1dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS1FVNVPROC __glewVertexAttribs1fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS1SVNVPROC __glewVertexAttribs1svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS2DVNVPROC __glewVertexAttribs2dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS2FVNVPROC __glewVertexAttribs2fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS2SVNVPROC __glewVertexAttribs2svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS3DVNVPROC __glewVertexAttribs3dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS3FVNVPROC __glewVertexAttribs3fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS3SVNVPROC __glewVertexAttribs3svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS4DVNVPROC __glewVertexAttribs4dvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS4FVNVPROC __glewVertexAttribs4fvNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS4SVNVPROC __glewVertexAttribs4svNV;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBS4UBVNVPROC __glewVertexAttribs4ubvNV;

GLEW_FUN_EXPORT PFNGLCLEARDEPTHFOESPROC __glewClearDepthfOES;
GLEW_FUN_EXPORT PFNGLCLIPPLANEFOESPROC __glewClipPlanefOES;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEFOESPROC __glewDepthRangefOES;
GLEW_FUN_EXPORT PFNGLFRUSTUMFOESPROC __glewFrustumfOES;
GLEW_FUN_EXPORT PFNGLGETCLIPPLANEFOESPROC __glewGetClipPlanefOES;
GLEW_FUN_EXPORT PFNGLORTHOFOESPROC __glewOrthofOES;

GLEW_FUN_EXPORT PFNGLDETAILTEXFUNCSGISPROC __glewDetailTexFuncSGIS;
GLEW_FUN_EXPORT PFNGLGETDETAILTEXFUNCSGISPROC __glewGetDetailTexFuncSGIS;

GLEW_FUN_EXPORT PFNGLFOGFUNCSGISPROC __glewFogFuncSGIS;
GLEW_FUN_EXPORT PFNGLGETFOGFUNCSGISPROC __glewGetFogFuncSGIS;

GLEW_FUN_EXPORT PFNGLSAMPLEMASKSGISPROC __glewSampleMaskSGIS;
GLEW_FUN_EXPORT PFNGLSAMPLEPATTERNSGISPROC __glewSamplePatternSGIS;

GLEW_FUN_EXPORT PFNGLGETSHARPENTEXFUNCSGISPROC __glewGetSharpenTexFuncSGIS;
GLEW_FUN_EXPORT PFNGLSHARPENTEXFUNCSGISPROC __glewSharpenTexFuncSGIS;

GLEW_FUN_EXPORT PFNGLTEXIMAGE4DSGISPROC __glewTexImage4DSGIS;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE4DSGISPROC __glewTexSubImage4DSGIS;

GLEW_FUN_EXPORT PFNGLGETTEXFILTERFUNCSGISPROC __glewGetTexFilterFuncSGIS;
GLEW_FUN_EXPORT PFNGLTEXFILTERFUNCSGISPROC __glewTexFilterFuncSGIS;

GLEW_FUN_EXPORT PFNGLASYNCMARKERSGIXPROC __glewAsyncMarkerSGIX;
GLEW_FUN_EXPORT PFNGLDELETEASYNCMARKERSSGIXPROC __glewDeleteAsyncMarkersSGIX;
GLEW_FUN_EXPORT PFNGLFINISHASYNCSGIXPROC __glewFinishAsyncSGIX;
GLEW_FUN_EXPORT PFNGLGENASYNCMARKERSSGIXPROC __glewGenAsyncMarkersSGIX;
GLEW_FUN_EXPORT PFNGLISASYNCMARKERSGIXPROC __glewIsAsyncMarkerSGIX;
GLEW_FUN_EXPORT PFNGLPOLLASYNCSGIXPROC __glewPollAsyncSGIX;

GLEW_FUN_EXPORT PFNGLFLUSHRASTERSGIXPROC __glewFlushRasterSGIX;

GLEW_FUN_EXPORT PFNGLTEXTUREFOGSGIXPROC __glewTextureFogSGIX;

GLEW_FUN_EXPORT PFNGLFRAGMENTCOLORMATERIALSGIXPROC __glewFragmentColorMaterialSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELFSGIXPROC __glewFragmentLightModelfSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELFVSGIXPROC __glewFragmentLightModelfvSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELISGIXPROC __glewFragmentLightModeliSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTMODELIVSGIXPROC __glewFragmentLightModelivSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTFSGIXPROC __glewFragmentLightfSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTFVSGIXPROC __glewFragmentLightfvSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTISGIXPROC __glewFragmentLightiSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTLIGHTIVSGIXPROC __glewFragmentLightivSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALFSGIXPROC __glewFragmentMaterialfSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALFVSGIXPROC __glewFragmentMaterialfvSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALISGIXPROC __glewFragmentMaterialiSGIX;
GLEW_FUN_EXPORT PFNGLFRAGMENTMATERIALIVSGIXPROC __glewFragmentMaterialivSGIX;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTLIGHTFVSGIXPROC __glewGetFragmentLightfvSGIX;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTLIGHTIVSGIXPROC __glewGetFragmentLightivSGIX;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTMATERIALFVSGIXPROC __glewGetFragmentMaterialfvSGIX;
GLEW_FUN_EXPORT PFNGLGETFRAGMENTMATERIALIVSGIXPROC __glewGetFragmentMaterialivSGIX;

GLEW_FUN_EXPORT PFNGLFRAMEZOOMSGIXPROC __glewFrameZoomSGIX;

GLEW_FUN_EXPORT PFNGLPIXELTEXGENSGIXPROC __glewPixelTexGenSGIX;

GLEW_FUN_EXPORT PFNGLREFERENCEPLANESGIXPROC __glewReferencePlaneSGIX;

GLEW_FUN_EXPORT PFNGLSPRITEPARAMETERFSGIXPROC __glewSpriteParameterfSGIX;
GLEW_FUN_EXPORT PFNGLSPRITEPARAMETERFVSGIXPROC __glewSpriteParameterfvSGIX;
GLEW_FUN_EXPORT PFNGLSPRITEPARAMETERISGIXPROC __glewSpriteParameteriSGIX;
GLEW_FUN_EXPORT PFNGLSPRITEPARAMETERIVSGIXPROC __glewSpriteParameterivSGIX;

GLEW_FUN_EXPORT PFNGLTAGSAMPLEBUFFERSGIXPROC __glewTagSampleBufferSGIX;

GLEW_FUN_EXPORT PFNGLCOLORTABLEPARAMETERFVSGIPROC __glewColorTableParameterfvSGI;
GLEW_FUN_EXPORT PFNGLCOLORTABLEPARAMETERIVSGIPROC __glewColorTableParameterivSGI;
GLEW_FUN_EXPORT PFNGLCOLORTABLESGIPROC __glewColorTableSGI;
GLEW_FUN_EXPORT PFNGLCOPYCOLORTABLESGIPROC __glewCopyColorTableSGI;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERFVSGIPROC __glewGetColorTableParameterfvSGI;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLEPARAMETERIVSGIPROC __glewGetColorTableParameterivSGI;
GLEW_FUN_EXPORT PFNGLGETCOLORTABLESGIPROC __glewGetColorTableSGI;

GLEW_FUN_EXPORT PFNGLFINISHTEXTURESUNXPROC __glewFinishTextureSUNX;

GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORBSUNPROC __glewGlobalAlphaFactorbSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORDSUNPROC __glewGlobalAlphaFactordSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORFSUNPROC __glewGlobalAlphaFactorfSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORISUNPROC __glewGlobalAlphaFactoriSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORSSUNPROC __glewGlobalAlphaFactorsSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORUBSUNPROC __glewGlobalAlphaFactorubSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORUISUNPROC __glewGlobalAlphaFactoruiSUN;
GLEW_FUN_EXPORT PFNGLGLOBALALPHAFACTORUSSUNPROC __glewGlobalAlphaFactorusSUN;

GLEW_FUN_EXPORT PFNGLREADVIDEOPIXELSSUNPROC __glewReadVideoPixelsSUN;

GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEPOINTERSUNPROC __glewReplacementCodePointerSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUBSUNPROC __glewReplacementCodeubSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUBVSUNPROC __glewReplacementCodeubvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUISUNPROC __glewReplacementCodeuiSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUIVSUNPROC __glewReplacementCodeuivSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUSSUNPROC __glewReplacementCodeusSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUSVSUNPROC __glewReplacementCodeusvSUN;

GLEW_FUN_EXPORT PFNGLCOLOR3FVERTEX3FSUNPROC __glewColor3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLCOLOR3FVERTEX3FVSUNPROC __glewColor3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC __glewColor4fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC __glewColor4fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4UBVERTEX2FSUNPROC __glewColor4ubVertex2fSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4UBVERTEX2FVSUNPROC __glewColor4ubVertex2fvSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4UBVERTEX3FSUNPROC __glewColor4ubVertex3fSUN;
GLEW_FUN_EXPORT PFNGLCOLOR4UBVERTEX3FVSUNPROC __glewColor4ubVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLNORMAL3FVERTEX3FSUNPROC __glewNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLNORMAL3FVERTEX3FVSUNPROC __glewNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC __glewReplacementCodeuiColor3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC __glewReplacementCodeuiColor3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC __glewReplacementCodeuiColor4fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC __glewReplacementCodeuiColor4fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC __glewReplacementCodeuiColor4ubVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC __glewReplacementCodeuiColor4ubVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC __glewReplacementCodeuiNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC __glewReplacementCodeuiNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC __glewReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC __glewReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC __glewReplacementCodeuiTexCoord2fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC __glewReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC __glewReplacementCodeuiTexCoord2fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC __glewReplacementCodeuiTexCoord2fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC __glewReplacementCodeuiVertex3fSUN;
GLEW_FUN_EXPORT PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC __glewReplacementCodeuiVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC __glewTexCoord2fColor3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC __glewTexCoord2fColor3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC __glewTexCoord2fColor4fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC __glewTexCoord2fColor4fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC __glewTexCoord2fColor4ubVertex3fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC __glewTexCoord2fColor4ubVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC __glewTexCoord2fNormal3fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC __glewTexCoord2fNormal3fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FVERTEX3FSUNPROC __glewTexCoord2fVertex3fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD2FVERTEX3FVSUNPROC __glewTexCoord2fVertex3fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC __glewTexCoord4fColor4fNormal3fVertex4fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC __glewTexCoord4fColor4fNormal3fVertex4fvSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD4FVERTEX4FSUNPROC __glewTexCoord4fVertex4fSUN;
GLEW_FUN_EXPORT PFNGLTEXCOORD4FVERTEX4FVSUNPROC __glewTexCoord4fVertex4fvSUN;

GLEW_FUN_EXPORT PFNGLADDSWAPHINTRECTWINPROC __glewAddSwapHintRectWIN;

#if defined(GLEW_MX) && !defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */

GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_1;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_2;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_3;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_4;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_1_5;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_2_0;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_2_1;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_3_0;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_3_1;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_3_2;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_3_3;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_4_0;
GLEW_VAR_EXPORT GLboolean __GLEW_VERSION_4_1;
GLEW_VAR_EXPORT GLboolean __GLEW_3DFX_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_3DFX_tbuffer;
GLEW_VAR_EXPORT GLboolean __GLEW_3DFX_texture_compression_FXT1;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_conservative_depth;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_debug_output;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_draw_buffers_blend;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_name_gen_delete;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_performance_monitor;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_seamless_cubemap_per_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_shader_stencil_export;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_texture_texture4;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_transform_feedback3_lines_triangles;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_vertex_shader_tessellator;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_aux_depth_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_client_storage;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_element_array;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_fence;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_float_pixels;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_flush_buffer_range;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_object_purgeable;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_pixel_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_rgb_422;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_row_bytes;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_specular_vector;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_texture_range;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_transform_hint;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_vertex_array_object;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_vertex_array_range;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_vertex_program_evaluators;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_ycbcr_422;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_ES2_compatibility;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_blend_func_extended;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_cl_event;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_color_buffer_float;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_compatibility;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_copy_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_debug_output;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_depth_buffer_float;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_depth_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_depth_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_draw_buffers;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_draw_buffers_blend;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_draw_elements_base_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_draw_indirect;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_draw_instanced;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_explicit_attrib_location;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_fragment_coord_conventions;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_fragment_program;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_fragment_program_shadow;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_fragment_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_framebuffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_framebuffer_sRGB;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_geometry_shader4;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_get_program_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_gpu_shader5;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_gpu_shader_fp64;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_half_float_pixel;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_half_float_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_imaging;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_instanced_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_map_buffer_range;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_matrix_palette;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_multitexture;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_occlusion_query;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_occlusion_query2;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_pixel_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_point_parameters;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_point_sprite;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_provoking_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_robustness;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_sample_shading;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_sampler_objects;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_seamless_cube_map;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_separate_shader_objects;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_bit_encoding;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_objects;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_precision;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_stencil_export;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_subroutine;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shader_texture_lod;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shading_language_100;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shading_language_include;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shadow;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_shadow_ambient;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_sync;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_tessellation_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_border_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_buffer_object_rgb32;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_compression;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_compression_bptc;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_compression_rgtc;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_cube_map;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_cube_map_array;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_env_add;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_env_combine;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_env_crossbar;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_env_dot3;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_float;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_gather;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_mirrored_repeat;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_non_power_of_two;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_query_lod;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_rectangle;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_rg;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_rgb10_a2ui;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_texture_swizzle;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_timer_query;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_transform_feedback2;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_transform_feedback3;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_transpose_matrix;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_uniform_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_array_bgra;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_array_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_attrib_64bit;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_blend;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_program;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_vertex_type_2_10_10_10_rev;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_viewport_array;
GLEW_VAR_EXPORT GLboolean __GLEW_ARB_window_pos;
GLEW_VAR_EXPORT GLboolean __GLEW_ATIX_point_sprites;
GLEW_VAR_EXPORT GLboolean __GLEW_ATIX_texture_env_combine3;
GLEW_VAR_EXPORT GLboolean __GLEW_ATIX_texture_env_route;
GLEW_VAR_EXPORT GLboolean __GLEW_ATIX_vertex_shader_output_point_size;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_draw_buffers;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_element_array;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_envmap_bumpmap;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_fragment_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_map_object_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_meminfo;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_pn_triangles;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_separate_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_shader_texture_lod;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_text_fragment_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_texture_compression_3dc;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_texture_env_combine3;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_texture_float;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_texture_mirror_once;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_vertex_array_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_vertex_attrib_array_object;
GLEW_VAR_EXPORT GLboolean __GLEW_ATI_vertex_streams;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_422_pixels;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_Cg_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_abgr;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_bgra;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_bindable_uniform;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_color;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_equation_separate;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_func_separate;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_logic_op;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_minmax;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_subtract;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_clip_volume_hint;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_cmyka;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_color_subtable;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_compiled_vertex_array;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_convolution;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_coordinate_frame;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_copy_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_cull_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_depth_bounds_test;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_direct_state_access;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_draw_buffers2;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_draw_instanced;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_draw_range_elements;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_fog_coord;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_fragment_lighting;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_framebuffer_blit;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_framebuffer_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_framebuffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_framebuffer_sRGB;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_geometry_shader4;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_gpu_program_parameters;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_gpu_shader4;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_histogram;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_index_array_formats;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_index_func;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_index_material;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_index_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_light_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_misc_attribute;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_multi_draw_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_packed_depth_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_packed_float;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_packed_pixels;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_paletted_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_pixel_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_pixel_transform;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_pixel_transform_color_table;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_point_parameters;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_polygon_offset;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_provoking_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_rescale_normal;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_scene_marker;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_secondary_color;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_separate_shader_objects;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_separate_specular_color;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shader_image_load_store;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shadow_funcs;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shared_texture_palette;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_stencil_clear_tag;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_stencil_two_side;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_stencil_wrap;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_subtexture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture3D;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_array;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_compression_dxt1;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_compression_latc;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_compression_rgtc;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_compression_s3tc;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_cube_map;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_edge_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_env;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_env_add;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_env_combine;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_env_dot3;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_filter_anisotropic;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_integer;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_lod_bias;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_mirror_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_object;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_perturb_normal;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_rectangle;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_sRGB;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_shared_exponent;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_snorm;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_swizzle;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_timer_query;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_transform_feedback;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_vertex_array;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_vertex_array_bgra;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_vertex_attrib_64bit;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_vertex_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_vertex_weighting;
GLEW_VAR_EXPORT GLboolean __GLEW_GREMEDY_frame_terminator;
GLEW_VAR_EXPORT GLboolean __GLEW_GREMEDY_string_marker;
GLEW_VAR_EXPORT GLboolean __GLEW_HP_convolution_border_modes;
GLEW_VAR_EXPORT GLboolean __GLEW_HP_image_transform;
GLEW_VAR_EXPORT GLboolean __GLEW_HP_occlusion_test;
GLEW_VAR_EXPORT GLboolean __GLEW_HP_texture_lighting;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_cull_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_multimode_draw_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_rasterpos_clip;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_static_data;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_texture_mirrored_repeat;
GLEW_VAR_EXPORT GLboolean __GLEW_IBM_vertex_array_lists;
GLEW_VAR_EXPORT GLboolean __GLEW_INGR_color_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_INGR_interlace_read;
GLEW_VAR_EXPORT GLboolean __GLEW_INTEL_parallel_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_INTEL_texture_scissor;
GLEW_VAR_EXPORT GLboolean __GLEW_KTX_buffer_region;
GLEW_VAR_EXPORT GLboolean __GLEW_MESAX_texture_stack;
GLEW_VAR_EXPORT GLboolean __GLEW_MESA_pack_invert;
GLEW_VAR_EXPORT GLboolean __GLEW_MESA_resize_buffers;
GLEW_VAR_EXPORT GLboolean __GLEW_MESA_window_pos;
GLEW_VAR_EXPORT GLboolean __GLEW_MESA_ycbcr_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_blend_square;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_conditional_render;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_copy_depth_to_color;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_copy_image;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_depth_buffer_float;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_depth_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_depth_range_unclamped;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_evaluators;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_explicit_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fence;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_float_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fog_distance;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fragment_program;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fragment_program2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fragment_program4;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fragment_program_option;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_framebuffer_multisample_coverage;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_geometry_program4;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_geometry_shader4;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_gpu_program4;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_gpu_program5;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_gpu_program_fp64;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_gpu_shader5;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_half_float;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_light_max_exponent;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_multisample_coverage;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_multisample_filter_hint;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_occlusion_query;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_packed_depth_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_parameter_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_parameter_buffer_object2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_pixel_data_range;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_point_sprite;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_present_video;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_primitive_restart;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_register_combiners;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_register_combiners2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_shader_buffer_load;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_tessellation_program5;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texgen_emboss;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texgen_reflection;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_barrier;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_compression_vtc;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_env_combine4;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_expand_normal;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_rectangle;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_shader;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_shader2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_shader3;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_transform_feedback;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_transform_feedback2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vdpau_interop;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_array_range;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_array_range2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_attrib_integer_64bit;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_buffer_unified_memory;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program1_1;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program2;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program2_option;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program3;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_vertex_program4;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_byte_coordinates;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_compressed_paletted_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_read_format;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_single_precision;
GLEW_VAR_EXPORT GLboolean __GLEW_OML_interlace;
GLEW_VAR_EXPORT GLboolean __GLEW_OML_resample;
GLEW_VAR_EXPORT GLboolean __GLEW_OML_subsample;
GLEW_VAR_EXPORT GLboolean __GLEW_PGI_misc_hints;
GLEW_VAR_EXPORT GLboolean __GLEW_PGI_vertex_hints;
GLEW_VAR_EXPORT GLboolean __GLEW_REND_screen_coordinates;
GLEW_VAR_EXPORT GLboolean __GLEW_S3_s3tc;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_color_range;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_detail_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_fog_function;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_generate_mipmap;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_pixel_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_point_line_texgen;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_sharpen_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture4D;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture_border_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture_edge_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture_filter4;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture_lod;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIS_texture_select;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_async;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_async_histogram;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_async_pixel;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_blend_alpha_minmax;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_clipmap;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_convolution_accuracy;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_depth_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_flush_raster;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_fog_offset;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_fog_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_fragment_specular_lighting;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_framezoom;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_interlace;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_ir_instrument1;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_list_priority;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_pixel_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_pixel_texture_bits;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_reference_plane;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_resample;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_shadow;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_shadow_ambient;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_sprite;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_tag_sample_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_add_env;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_coordinate_clamp;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_lod_bias;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_multi_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_range;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_texture_scale_bias;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_vertex_preclip;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_vertex_preclip_hint;
GLEW_VAR_EXPORT GLboolean __GLEW_SGIX_ycrcb;
GLEW_VAR_EXPORT GLboolean __GLEW_SGI_color_matrix;
GLEW_VAR_EXPORT GLboolean __GLEW_SGI_color_table;
GLEW_VAR_EXPORT GLboolean __GLEW_SGI_texture_color_table;
GLEW_VAR_EXPORT GLboolean __GLEW_SUNX_constant_data;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_convolution_border_modes;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_global_alpha;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_mesh_array;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_read_video_pixels;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_slice_accum;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_triangle_list;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_vertex;
GLEW_VAR_EXPORT GLboolean __GLEW_WIN_phong_shading;
GLEW_VAR_EXPORT GLboolean __GLEW_WIN_specular_fog;
GLEW_VAR_EXPORT GLboolean __GLEW_WIN_swap_hint;

#ifdef GLEW_MX
}; /* GLEWContextStruct */
#endif /* GLEW_MX */

/* ------------------------------------------------------------------------- */

/* error codes */
#define GLEW_OK 0
#define GLEW_NO_ERROR 0
#define GLEW_ERROR_NO_GL_VERSION 1  /* missing GL version */
#define GLEW_ERROR_GL_VERSION_10_ONLY 2  /* GL 1.1 and up are not supported */
#define GLEW_ERROR_GLX_VERSION_11_ONLY 3  /* GLX 1.2 and up are not supported */

/* string codes */
#define GLEW_VERSION 1
#define GLEW_VERSION_MAJOR 2
#define GLEW_VERSION_MINOR 3
#define GLEW_VERSION_MICRO 4

/* API */
#ifdef GLEW_MX

typedef struct GLEWContextStruct GLEWContext;
GLEWAPI GLenum glewContextInit (GLEWContext* ctx);
GLEWAPI GLboolean glewContextIsSupported (GLEWContext* ctx, const char* name);

#define glewInit() glewContextInit(glewGetContext())
#define glewIsSupported(x) glewContextIsSupported(glewGetContext(), x)
#define glewIsExtensionSupported(x) glewIsSupported(x)

#define GLEW_GET_VAR(x) (*(const GLboolean*)&(glewGetContext()->x))
#ifdef _WIN32
#  define GLEW_GET_FUN(x) glewGetContext()->x
#else
#  define GLEW_GET_FUN(x) x
#endif

#else /* GLEW_MX */

GLEWAPI GLenum glewInit ();
GLEWAPI GLboolean glewIsSupported (const char* name);
#define glewIsExtensionSupported(x) glewIsSupported(x)

#define GLEW_GET_VAR(x) (*(const GLboolean*)&x)
#define GLEW_GET_FUN(x) x

#endif /* GLEW_MX */

GLEWAPI GLboolean glewExperimental;
GLEWAPI GLboolean glewGetExtension (const char* name);
GLEWAPI const GLubyte* glewGetErrorString (GLenum error);
GLEWAPI const GLubyte* glewGetString (GLenum name);

#ifdef __cplusplus
}
#endif

#ifdef GLEW_APIENTRY_DEFINED
#undef GLEW_APIENTRY_DEFINED
#undef APIENTRY
#undef GLAPIENTRY
#define GLAPIENTRY
#endif

#ifdef GLEW_CALLBACK_DEFINED
#undef GLEW_CALLBACK_DEFINED
#undef CALLBACK
#endif

#ifdef GLEW_WINGDIAPI_DEFINED
#undef GLEW_WINGDIAPI_DEFINED
#undef WINGDIAPI
#endif

#undef GLAPI
/* #undef GLEWAPI */

#endif /* __glew_h__ */
