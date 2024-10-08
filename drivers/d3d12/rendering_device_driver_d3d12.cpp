/**************************************************************************/
/*  rendering_device_driver_d3d12.cpp                                     */
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

#include "rendering_device_driver_d3d12.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"
#include "servers/rendering/rendering_device.h"
#include "thirdparty/zlib/zlib.h"

#include "d3d12_godot_nir_bridge.h"
#include "dxil_hash.h"
#include "rendering_context_driver_d3d12.h"

// No point in fighting warnings in Mesa.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200) // "nonstandard extension used: zero-sized array in struct/union".
#pragma warning(disable : 4806) // "'&': unsafe operation: no value of type 'bool' promoted to type 'uint32_t' can equal the given constant".
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wstring-plus-int"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "nir_spirv.h"
#include "nir_to_dxil.h"
#include "spirv_to_dxil.h"
extern "C" {
#include "dxil_spirv_nir.h"
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if !defined(_MSC_VER)
#include <guiddef.h>

#include <dxguids.h>
#endif

// Mesa may define this.
#ifdef UNUSED
#undef UNUSED
#endif

#ifdef PIX_ENABLED
#if defined(__GNUC__)
#define _MSC_VER 1800
#endif
#define USE_PIX
#include "WinPixEventRuntime/pix3.h"
#if defined(__GNUC__)
#undef _MSC_VER
#endif
#endif

static const D3D12_RANGE VOID_RANGE = {};

static const uint32_t ROOT_CONSTANT_REGISTER = GODOT_NIR_DESCRIPTOR_SET_MULTIPLIER * (RDD::MAX_UNIFORM_SETS + 1);
static const uint32_t RUNTIME_DATA_REGISTER = GODOT_NIR_DESCRIPTOR_SET_MULTIPLIER * (RDD::MAX_UNIFORM_SETS + 2);

/*****************/
/**** GENERIC ****/
/*****************/

// NOTE: RD's packed format names are reversed in relation to DXGI's; e.g.:.
// - DATA_FORMAT_A8B8G8R8_UNORM_PACK32 -> DXGI_FORMAT_R8G8B8A8_UNORM (packed; note ABGR vs. RGBA).
// - DATA_FORMAT_B8G8R8A8_UNORM -> DXGI_FORMAT_B8G8R8A8_UNORM (not packed; note BGRA order matches).
// TODO: Add YUV formats properly, which would require better support for planes in the RD API.

const RenderingDeviceDriverD3D12::D3D12Format RenderingDeviceDriverD3D12::RD_TO_D3D12_FORMAT[RDD::DATA_FORMAT_MAX] = {
	/* DATA_FORMAT_R4G4_UNORM_PACK8 */ {},
	/* DATA_FORMAT_R4G4B4A4_UNORM_PACK16 */ { DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 2, 3, 0) },
	/* DATA_FORMAT_B4G4R4A4_UNORM_PACK16 */ { DXGI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(3, 2, 1, 0) },
	/* DATA_FORMAT_R5G6B5_UNORM_PACK16 */ { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G6R5_UNORM },
	/* DATA_FORMAT_B5G6R5_UNORM_PACK16 */ { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G6R5_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(2, 1, 0, 3) },
	/* DATA_FORMAT_R5G5B5A1_UNORM_PACK16 */ { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 2, 3, 0) },
	/* DATA_FORMAT_B5G5R5A1_UNORM_PACK16 */ { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(3, 2, 1, 0) },
	/* DATA_FORMAT_A1R5G5B5_UNORM_PACK16 */ { DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM },
	/* DATA_FORMAT_R8_UNORM */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UNORM },
	/* DATA_FORMAT_R8_SNORM */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_SNORM },
	/* DATA_FORMAT_R8_USCALED */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UINT },
	/* DATA_FORMAT_R8_SSCALED */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_SINT },
	/* DATA_FORMAT_R8_UINT */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_UINT },
	/* DATA_FORMAT_R8_SINT */ { DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8_SINT },
	/* DATA_FORMAT_R8_SRGB */ {},
	/* DATA_FORMAT_R8G8_UNORM */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UNORM },
	/* DATA_FORMAT_R8G8_SNORM */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_SNORM },
	/* DATA_FORMAT_R8G8_USCALED */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UINT },
	/* DATA_FORMAT_R8G8_SSCALED */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_SINT },
	/* DATA_FORMAT_R8G8_UINT */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_UINT },
	/* DATA_FORMAT_R8G8_SINT */ { DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8_SINT },
	/* DATA_FORMAT_R8G8_SRGB */ {},
	/* DATA_FORMAT_R8G8B8_UNORM */ {},
	/* DATA_FORMAT_R8G8B8_SNORM */ {},
	/* DATA_FORMAT_R8G8B8_USCALED */ {},
	/* DATA_FORMAT_R8G8B8_SSCALED */ {},
	/* DATA_FORMAT_R8G8B8_UINT */ {},
	/* DATA_FORMAT_R8G8B8_SINT */ {},
	/* DATA_FORMAT_R8G8B8_SRGB */ {},
	/* DATA_FORMAT_B8G8R8_UNORM */ {},
	/* DATA_FORMAT_B8G8R8_SNORM */ {},
	/* DATA_FORMAT_B8G8R8_USCALED */ {},
	/* DATA_FORMAT_B8G8R8_SSCALED */ {},
	/* DATA_FORMAT_B8G8R8_UINT */ {},
	/* DATA_FORMAT_B8G8R8_SINT */ {},
	/* DATA_FORMAT_B8G8R8_SRGB */ {},
	/* DATA_FORMAT_R8G8B8A8_UNORM */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM },
	/* DATA_FORMAT_R8G8B8A8_SNORM */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SNORM },
	/* DATA_FORMAT_R8G8B8A8_USCALED */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_R8G8B8A8_SSCALED */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_R8G8B8A8_UINT */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_R8G8B8A8_SINT */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_R8G8B8A8_SRGB */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
	/* DATA_FORMAT_B8G8R8A8_UNORM */ { DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM },
	/* DATA_FORMAT_B8G8R8A8_SNORM */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SNORM },
	/* DATA_FORMAT_B8G8R8A8_USCALED */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_B8G8R8A8_SSCALED */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_B8G8R8A8_UINT */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_B8G8R8A8_SINT */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_B8G8R8A8_SRGB */ { DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
	/* DATA_FORMAT_A8B8G8R8_UNORM_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM },
	/* DATA_FORMAT_A8B8G8R8_SNORM_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SNORM },
	/* DATA_FORMAT_A8B8G8R8_USCALED_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_A8B8G8R8_SSCALED_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_A8B8G8R8_UINT_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UINT },
	/* DATA_FORMAT_A8B8G8R8_SINT_PACK32 */ { DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_SINT },
	/* DATA_FORMAT_A8B8G8R8_SRGB_PACK32 */ { DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
	/* DATA_FORMAT_A2R10G10B10_UNORM_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(2, 1, 0, 3) },
	/* DATA_FORMAT_A2R10G10B10_SNORM_PACK32 */ {},
	/* DATA_FORMAT_A2R10G10B10_USCALED_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(2, 1, 0, 3) },
	/* DATA_FORMAT_A2R10G10B10_SSCALED_PACK32 */ {},
	/* DATA_FORMAT_A2R10G10B10_UINT_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(2, 1, 0, 3) },
	/* DATA_FORMAT_A2R10G10B10_SINT_PACK32 */ {},
	/* DATA_FORMAT_A2B10G10R10_UNORM_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UNORM },
	/* DATA_FORMAT_A2B10G10R10_SNORM_PACK32 */ {},
	/* DATA_FORMAT_A2B10G10R10_USCALED_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT },
	/* DATA_FORMAT_A2B10G10R10_SSCALED_PACK32 */ {},
	/* DATA_FORMAT_A2B10G10R10_UINT_PACK32 */ { DXGI_FORMAT_R10G10B10A2_TYPELESS, DXGI_FORMAT_R10G10B10A2_UINT },
	/* DATA_FORMAT_A2B10G10R10_SINT_PACK32 */ {},
	/* DATA_FORMAT_R16_UNORM */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM },
	/* DATA_FORMAT_R16_SNORM */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_SNORM },
	/* DATA_FORMAT_R16_USCALED */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UINT },
	/* DATA_FORMAT_R16_SSCALED */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_SINT },
	/* DATA_FORMAT_R16_UINT */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UINT },
	/* DATA_FORMAT_R16_SINT */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_SINT },
	/* DATA_FORMAT_R16_SFLOAT */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_FLOAT },
	/* DATA_FORMAT_R16G16_UNORM */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UNORM },
	/* DATA_FORMAT_R16G16_SNORM */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_SNORM },
	/* DATA_FORMAT_R16G16_USCALED */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UINT },
	/* DATA_FORMAT_R16G16_SSCALED */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_SINT },
	/* DATA_FORMAT_R16G16_UINT */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_UINT },
	/* DATA_FORMAT_R16G16_SINT */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_SINT },
	/* DATA_FORMAT_R16G16_SFLOAT */ { DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16_FLOAT },
	/* DATA_FORMAT_R16G16B16_UNORM */ {},
	/* DATA_FORMAT_R16G16B16_SNORM */ {},
	/* DATA_FORMAT_R16G16B16_USCALED */ {},
	/* DATA_FORMAT_R16G16B16_SSCALED */ {},
	/* DATA_FORMAT_R16G16B16_UINT */ {},
	/* DATA_FORMAT_R16G16B16_SINT */ {},
	/* DATA_FORMAT_R16G16B16_SFLOAT */ {},
	/* DATA_FORMAT_R16G16B16A16_UNORM */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_UNORM },
	/* DATA_FORMAT_R16G16B16A16_SNORM */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_SNORM },
	/* DATA_FORMAT_R16G16B16A16_USCALED */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_UINT },
	/* DATA_FORMAT_R16G16B16A16_SSCALED */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_SINT },
	/* DATA_FORMAT_R16G16B16A16_UINT */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_UINT },
	/* DATA_FORMAT_R16G16B16A16_SINT */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_SINT },
	/* DATA_FORMAT_R16G16B16A16_SFLOAT */ { DXGI_FORMAT_R16G16B16A16_TYPELESS, DXGI_FORMAT_R16G16B16A16_FLOAT },
	/* DATA_FORMAT_R32_UINT */ { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_UINT },
	/* DATA_FORMAT_R32_SINT */ { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_SINT },
	/* DATA_FORMAT_R32_SFLOAT */ { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT },
	/* DATA_FORMAT_R32G32_UINT */ { DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32_UINT },
	/* DATA_FORMAT_R32G32_SINT */ { DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32_SINT },
	/* DATA_FORMAT_R32G32_SFLOAT */ { DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32_FLOAT },
	/* DATA_FORMAT_R32G32B32_UINT */ { DXGI_FORMAT_R32G32B32_TYPELESS, DXGI_FORMAT_R32G32B32_UINT },
	/* DATA_FORMAT_R32G32B32_SINT */ { DXGI_FORMAT_R32G32B32_TYPELESS, DXGI_FORMAT_R32G32B32_SINT },
	/* DATA_FORMAT_R32G32B32_SFLOAT */ { DXGI_FORMAT_R32G32B32_TYPELESS, DXGI_FORMAT_R32G32B32_FLOAT },
	/* DATA_FORMAT_R32G32B32A32_UINT */ { DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_UINT },
	/* DATA_FORMAT_R32G32B32A32_SINT */ { DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_SINT },
	/* DATA_FORMAT_R32G32B32A32_SFLOAT */ { DXGI_FORMAT_R32G32B32A32_TYPELESS, DXGI_FORMAT_R32G32B32A32_FLOAT },
	/* DATA_FORMAT_R64_UINT */ {},
	/* DATA_FORMAT_R64_SINT */ {},
	/* DATA_FORMAT_R64_SFLOAT */ {},
	/* DATA_FORMAT_R64G64_UINT */ {},
	/* DATA_FORMAT_R64G64_SINT */ {},
	/* DATA_FORMAT_R64G64_SFLOAT */ {},
	/* DATA_FORMAT_R64G64B64_UINT */ {},
	/* DATA_FORMAT_R64G64B64_SINT */ {},
	/* DATA_FORMAT_R64G64B64_SFLOAT */ {},
	/* DATA_FORMAT_R64G64B64A64_UINT */ {},
	/* DATA_FORMAT_R64G64B64A64_SINT */ {},
	/* DATA_FORMAT_R64G64B64A64_SFLOAT */ {},
	/* DATA_FORMAT_B10G11R11_UFLOAT_PACK32 */ { DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT },
	/* DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32 */ { DXGI_FORMAT_R9G9B9E5_SHAREDEXP, DXGI_FORMAT_R9G9B9E5_SHAREDEXP },
	/* DATA_FORMAT_D16_UNORM */ { DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16_UNORM, 0, DXGI_FORMAT_D16_UNORM },
	/* DATA_FORMAT_X8_D24_UNORM_PACK32 */ { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_UNKNOWN, 0, DXGI_FORMAT_D24_UNORM_S8_UINT },
	/* DATA_FORMAT_D32_SFLOAT */ { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, DXGI_FORMAT_D32_FLOAT },
	/* DATA_FORMAT_S8_UINT */ {},
	/* DATA_FORMAT_D16_UNORM_S8_UINT */ {},
	/* DATA_FORMAT_D24_UNORM_S8_UINT */ { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_UNKNOWN, 0, DXGI_FORMAT_D24_UNORM_S8_UINT },
	/* DATA_FORMAT_D32_SFLOAT_S8_UINT */ { DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, DXGI_FORMAT_D32_FLOAT_S8X24_UINT },
	/* DATA_FORMAT_BC1_RGB_UNORM_BLOCK */ { DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1) },
	/* DATA_FORMAT_BC1_RGB_SRGB_BLOCK */ { DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM_SRGB, D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1) },
	/* DATA_FORMAT_BC1_RGBA_UNORM_BLOCK */ { DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM },
	/* DATA_FORMAT_BC1_RGBA_SRGB_BLOCK */ { DXGI_FORMAT_BC1_TYPELESS, DXGI_FORMAT_BC1_UNORM_SRGB },
	/* DATA_FORMAT_BC2_UNORM_BLOCK */ { DXGI_FORMAT_BC2_TYPELESS, DXGI_FORMAT_BC2_UNORM },
	/* DATA_FORMAT_BC2_SRGB_BLOCK */ { DXGI_FORMAT_BC2_TYPELESS, DXGI_FORMAT_BC2_UNORM_SRGB },
	/* DATA_FORMAT_BC3_UNORM_BLOCK */ { DXGI_FORMAT_BC3_TYPELESS, DXGI_FORMAT_BC3_UNORM },
	/* DATA_FORMAT_BC3_SRGB_BLOCK */ { DXGI_FORMAT_BC3_TYPELESS, DXGI_FORMAT_BC3_UNORM_SRGB },
	/* DATA_FORMAT_BC4_UNORM_BLOCK */ { DXGI_FORMAT_BC4_TYPELESS, DXGI_FORMAT_BC4_UNORM },
	/* DATA_FORMAT_BC4_SNORM_BLOCK */ { DXGI_FORMAT_BC4_TYPELESS, DXGI_FORMAT_BC4_SNORM },
	/* DATA_FORMAT_BC5_UNORM_BLOCK */ { DXGI_FORMAT_BC5_TYPELESS, DXGI_FORMAT_BC5_UNORM },
	/* DATA_FORMAT_BC5_SNORM_BLOCK */ { DXGI_FORMAT_BC5_TYPELESS, DXGI_FORMAT_BC5_SNORM },
	/* DATA_FORMAT_BC6H_UFLOAT_BLOCK */ { DXGI_FORMAT_BC6H_TYPELESS, DXGI_FORMAT_BC6H_UF16 },
	/* DATA_FORMAT_BC6H_SFLOAT_BLOCK */ { DXGI_FORMAT_BC6H_TYPELESS, DXGI_FORMAT_BC6H_SF16 },
	/* DATA_FORMAT_BC7_UNORM_BLOCK */ { DXGI_FORMAT_BC7_TYPELESS, DXGI_FORMAT_BC7_UNORM },
	/* DATA_FORMAT_BC7_SRGB_BLOCK */ { DXGI_FORMAT_BC7_TYPELESS, DXGI_FORMAT_BC7_UNORM_SRGB },
	/* DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK */ {},
	/* DATA_FORMAT_EAC_R11_UNORM_BLOCK */ {},
	/* DATA_FORMAT_EAC_R11_SNORM_BLOCK */ {},
	/* DATA_FORMAT_EAC_R11G11_UNORM_BLOCK */ {},
	/* DATA_FORMAT_EAC_R11G11_SNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_4x4_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_4x4_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_5x4_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_5x4_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_5x5_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_5x5_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_6x5_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_6x5_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_6x6_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_6x6_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x5_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x5_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x6_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x6_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x8_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_8x8_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x5_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x5_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x6_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x6_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x8_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x8_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x10_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_10x10_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_12x10_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_12x10_SRGB_BLOCK */ {},
	/* DATA_FORMAT_ASTC_12x12_UNORM_BLOCK */ {},
	/* DATA_FORMAT_ASTC_12x12_SRGB_BLOCK */ {},
	/* DATA_FORMAT_G8B8G8R8_422_UNORM */ {},
	/* DATA_FORMAT_B8G8R8G8_422_UNORM */ {},
	/* DATA_FORMAT_G8_B8_R8_3PLANE_420_UNORM */ {},
	/* DATA_FORMAT_G8_B8R8_2PLANE_420_UNORM */ {},
	/* DATA_FORMAT_G8_B8_R8_3PLANE_422_UNORM */ {},
	/* DATA_FORMAT_G8_B8R8_2PLANE_422_UNORM */ {},
	/* DATA_FORMAT_G8_B8_R8_3PLANE_444_UNORM */ {},
	/* DATA_FORMAT_R10X6_UNORM_PACK16 */ {},
	/* DATA_FORMAT_R10X6G10X6_UNORM_2PACK16 */ {},
	/* DATA_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_R12X4_UNORM_PACK16 */ {},
	/* DATA_FORMAT_R12X4G12X4_UNORM_2PACK16 */ {},
	/* DATA_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 */ {},
	/* DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 */ {},
	/* DATA_FORMAT_G16B16G16R16_422_UNORM */ {},
	/* DATA_FORMAT_B16G16R16G16_422_UNORM */ {},
	/* DATA_FORMAT_G16_B16_R16_3PLANE_420_UNORM */ {},
	/* DATA_FORMAT_G16_B16R16_2PLANE_420_UNORM */ {},
	/* DATA_FORMAT_G16_B16_R16_3PLANE_422_UNORM */ {},
	/* DATA_FORMAT_G16_B16R16_2PLANE_422_UNORM */ {},
	/* DATA_FORMAT_G16_B16_R16_3PLANE_444_UNORM */ {},
};

Error RenderingDeviceDriverD3D12::DescriptorsHeap::allocate(ID3D12Device *p_device, D3D12_DESCRIPTOR_HEAP_TYPE p_type, uint32_t p_descriptor_count, bool p_for_gpu) {
	ERR_FAIL_COND_V(heap, ERR_ALREADY_EXISTS);
	ERR_FAIL_COND_V(p_descriptor_count == 0, ERR_INVALID_PARAMETER);

	handle_size = p_device->GetDescriptorHandleIncrementSize(p_type);

	desc.Type = p_type;
	desc.NumDescriptors = p_descriptor_count;
	desc.Flags = p_for_gpu ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT res = p_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf()));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_CANT_CREATE, "CreateDescriptorHeap failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	return OK;
}

RenderingDeviceDriverD3D12::DescriptorsHeap::Walker RenderingDeviceDriverD3D12::DescriptorsHeap::make_walker() const {
	Walker walker;
	walker.handle_size = handle_size;
	walker.handle_count = desc.NumDescriptors;
	if (heap) {
#if defined(_MSC_VER) || !defined(_WIN32)
		walker.first_cpu_handle = heap->GetCPUDescriptorHandleForHeapStart();
		if ((desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)) {
			walker.first_gpu_handle = heap->GetGPUDescriptorHandleForHeapStart();
		}
#else
		heap->GetCPUDescriptorHandleForHeapStart(&walker.first_cpu_handle);
		if ((desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)) {
			heap->GetGPUDescriptorHandleForHeapStart(&walker.first_gpu_handle);
		}
#endif
	}
	return walker;
}

void RenderingDeviceDriverD3D12::DescriptorsHeap::Walker::advance(uint32_t p_count) {
	ERR_FAIL_COND_MSG(handle_index + p_count > handle_count, "Would advance past EOF.");
	handle_index += p_count;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderingDeviceDriverD3D12::DescriptorsHeap::Walker::get_curr_cpu_handle() {
	ERR_FAIL_COND_V_MSG(is_at_eof(), D3D12_CPU_DESCRIPTOR_HANDLE(), "Heap walker is at EOF.");
	return D3D12_CPU_DESCRIPTOR_HANDLE{ first_cpu_handle.ptr + handle_index * handle_size };
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderingDeviceDriverD3D12::DescriptorsHeap::Walker::get_curr_gpu_handle() {
	ERR_FAIL_COND_V_MSG(!first_gpu_handle.ptr, D3D12_GPU_DESCRIPTOR_HANDLE(), "Can't provide a GPU handle from a non-GPU descriptors heap.");
	ERR_FAIL_COND_V_MSG(is_at_eof(), D3D12_GPU_DESCRIPTOR_HANDLE(), "Heap walker is at EOF.");
	return D3D12_GPU_DESCRIPTOR_HANDLE{ first_gpu_handle.ptr + handle_index * handle_size };
}

static const D3D12_COMPARISON_FUNC RD_TO_D3D12_COMPARE_OP[RD::COMPARE_OP_MAX] = {
	D3D12_COMPARISON_FUNC_NEVER,
	D3D12_COMPARISON_FUNC_LESS,
	D3D12_COMPARISON_FUNC_EQUAL,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER,
	D3D12_COMPARISON_FUNC_NOT_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER_EQUAL,
	D3D12_COMPARISON_FUNC_ALWAYS,
};

uint32_t RenderingDeviceDriverD3D12::SubgroupCapabilities::supported_stages_flags_rd() const {
	// If there's a way to check exactly which are supported, I have yet to find it.
	return (
			RenderingDevice::ShaderStage::SHADER_STAGE_FRAGMENT_BIT |
			RenderingDevice::ShaderStage::SHADER_STAGE_COMPUTE_BIT);
}

uint32_t RenderingDeviceDriverD3D12::SubgroupCapabilities::supported_operations_flags_rd() const {
	if (!wave_ops_supported) {
		return 0;
	} else {
		return (
				RenderingDevice::SubgroupOperations::SUBGROUP_BASIC_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_BALLOT_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_VOTE_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_SHUFFLE_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_SHUFFLE_RELATIVE_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_QUAD_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_ARITHMETIC_BIT |
				RenderingDevice::SubgroupOperations::SUBGROUP_CLUSTERED_BIT);
	}
}

void RenderingDeviceDriverD3D12::_debug_message_func(D3D12_MESSAGE_CATEGORY p_category, D3D12_MESSAGE_SEVERITY p_severity, D3D12_MESSAGE_ID p_id, LPCSTR p_description, void *p_context) {
	String type_string;
	switch (p_category) {
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:
			type_string = "APPLICATION_DEFINED";
			break;
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:
			type_string = "MISCELLANEOUS";
			break;
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION:
			type_string = "INITIALIZATION";
			break;
		case D3D12_MESSAGE_CATEGORY_CLEANUP:
			type_string = "CLEANUP";
			break;
		case D3D12_MESSAGE_CATEGORY_COMPILATION:
			type_string = "COMPILATION";
			break;
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION:
			type_string = "STATE_CREATION";
			break;
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING:
			type_string = "STATE_SETTING";
			break;
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING:
			type_string = "STATE_GETTING";
			break;
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
			type_string = "RESOURCE_MANIPULATION";
			break;
		case D3D12_MESSAGE_CATEGORY_EXECUTION:
			type_string = "EXECUTION";
			break;
		case D3D12_MESSAGE_CATEGORY_SHADER:
			type_string = "SHADER";
			break;
	}

	String error_message(type_string +
			" - Message Id Number: " + String::num_int64(p_id) +
			"\n\t" + p_description);

	// Convert D3D12 severity to our own log macros.
	switch (p_severity) {
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
			print_verbose(error_message);
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			print_line(error_message);
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			WARN_PRINT(error_message);
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			ERR_PRINT(error_message);
			CRASH_COND_MSG(Engine::get_singleton()->is_abort_on_gpu_errors_enabled(),
					"Crashing, because abort on GPU errors is enabled.");
			break;
	}
}

/******************/
/**** RESOURCE ****/
/******************/

static const D3D12_RESOURCE_DIMENSION RD_TEXTURE_TYPE_TO_D3D12_RESOURCE_DIMENSION[RD::TEXTURE_TYPE_MAX] = {
	D3D12_RESOURCE_DIMENSION_TEXTURE1D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	D3D12_RESOURCE_DIMENSION_TEXTURE3D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	D3D12_RESOURCE_DIMENSION_TEXTURE1D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D,
	D3D12_RESOURCE_DIMENSION_TEXTURE2D,
};

void RenderingDeviceDriverD3D12::_resource_transition_batch(CommandBufferInfo *p_command_buffer, ResourceInfo *p_resource, uint32_t p_subresource, uint32_t p_num_planes, D3D12_RESOURCE_STATES p_new_state) {
	DEV_ASSERT(p_subresource != UINT32_MAX); // We don't support an "all-resources" command here.

	ResourceInfo::States *res_states = p_resource->states_ptr;
	D3D12_RESOURCE_STATES *curr_state = &res_states->subresource_states[p_subresource];

	// Transitions can be considered redundant if the current state has all the bits of the new state.
	// This check does not apply to the common state however, which must resort to checking if the state is the same (0).
	bool any_state_is_common = *curr_state == D3D12_RESOURCE_STATE_COMMON || p_new_state == D3D12_RESOURCE_STATE_COMMON;
	bool redundant_transition = any_state_is_common ? *curr_state == p_new_state : ((*curr_state) & p_new_state) == p_new_state;
	if (redundant_transition) {
		bool just_written = *curr_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		bool needs_uav_barrier = just_written && res_states->last_batch_with_uav_barrier != p_command_buffer->res_barriers_batch;
		if (needs_uav_barrier) {
			if (p_command_buffer->res_barriers.size() < p_command_buffer->res_barriers_count + 1) {
				p_command_buffer->res_barriers.resize(p_command_buffer->res_barriers_count + 1);
			}
			p_command_buffer->res_barriers[p_command_buffer->res_barriers_count] = CD3DX12_RESOURCE_BARRIER::UAV(p_resource->resource);
			p_command_buffer->res_barriers_count++;
			res_states->last_batch_with_uav_barrier = p_command_buffer->res_barriers_batch;
		}
	} else {
		uint64_t subres_mask_piece = ((uint64_t)1 << (p_subresource & 0b111111));
		uint8_t subres_qword = p_subresource >> 6;

		if (p_command_buffer->res_barriers_requests.has(res_states)) {
			BarrierRequest &br = p_command_buffer->res_barriers_requests.get(res_states);
			DEV_ASSERT(br.dx_resource == p_resource->resource);
			DEV_ASSERT(br.subres_mask_qwords == STEPIFY(res_states->subresource_states.size(), 64) / 64);
			DEV_ASSERT(br.planes == p_num_planes);

			// First, find if the subresource already has a barrier scheduled.
			uint8_t curr_group_idx = 0;
			bool same_transition_scheduled = false;
			for (curr_group_idx = 0; curr_group_idx < br.groups_count; curr_group_idx++) {
				if (unlikely(br.groups[curr_group_idx].states == BarrierRequest::DELETED_GROUP)) {
					continue;
				}
				if ((br.groups[curr_group_idx].subres_mask[subres_qword] & subres_mask_piece)) {
					uint32_t state_mask = br.groups[curr_group_idx].states;
					same_transition_scheduled = (state_mask & (uint32_t)p_new_state) == (uint32_t)p_new_state;
					break;
				}
			}
			if (!same_transition_scheduled) {
				bool subres_already_there = curr_group_idx != br.groups_count;
				D3D12_RESOURCE_STATES final_states = {};
				if (subres_already_there) {
					final_states = br.groups[curr_group_idx].states;
					final_states |= p_new_state;
					bool subres_alone = true;
					for (uint8_t i = 0; i < br.subres_mask_qwords; i++) {
						if (i == subres_qword) {
							if (br.groups[curr_group_idx].subres_mask[i] != subres_mask_piece) {
								subres_alone = false;
								break;
							}
						} else {
							if (br.groups[curr_group_idx].subres_mask[i] != 0) {
								subres_alone = false;
								break;
							}
						}
					}
					bool relocated = false;
					if (subres_alone) {
						// Subresource is there by itself.
						for (uint8_t i = 0; i < br.groups_count; i++) {
							if (unlikely(i == curr_group_idx)) {
								continue;
							}
							if (unlikely(br.groups[i].states == BarrierRequest::DELETED_GROUP)) {
								continue;
							}
							// There's another group with the final states; relocate to it.
							if (br.groups[i].states == final_states) {
								br.groups[curr_group_idx].subres_mask[subres_qword] &= ~subres_mask_piece;
								relocated = true;
								break;
							}
						}
						if (relocated) {
							// Let's delete the group where it used to be by itself.
							if (curr_group_idx == br.groups_count - 1) {
								br.groups_count--;
							} else {
								br.groups[curr_group_idx].states = BarrierRequest::DELETED_GROUP;
							}
						} else {
							// Its current group, where it's alone, can extend its states.
							br.groups[curr_group_idx].states = final_states;
						}
					} else {
						// Already there, but not by itself and the state mask is different, so it now belongs to a different group.
						br.groups[curr_group_idx].subres_mask[subres_qword] &= ~subres_mask_piece;
						subres_already_there = false;
					}
				} else {
					final_states = p_new_state;
				}
				if (!subres_already_there) {
					// See if it fits exactly the states of some of the groups to fit it there.
					for (uint8_t i = 0; i < br.groups_count; i++) {
						if (unlikely(i == curr_group_idx)) {
							continue;
						}
						if (unlikely(br.groups[i].states == BarrierRequest::DELETED_GROUP)) {
							continue;
						}
						if (br.groups[i].states == final_states) {
							br.groups[i].subres_mask[subres_qword] |= subres_mask_piece;
							subres_already_there = true;
							break;
						}
					}
					if (!subres_already_there) {
						// Add a new group to accommodate this subresource.
						uint8_t group_to_fill = 0;
						if (br.groups_count < BarrierRequest::MAX_GROUPS) {
							// There are still free groups.
							group_to_fill = br.groups_count;
							br.groups_count++;
						} else {
							// Let's try to take over a deleted one.
							for (; group_to_fill < br.groups_count; group_to_fill++) {
								if (unlikely(br.groups[group_to_fill].states == BarrierRequest::DELETED_GROUP)) {
									break;
								}
							}
							CRASH_COND(group_to_fill == br.groups_count);
						}

						br.groups[group_to_fill].states = final_states;
						for (uint8_t i = 0; i < br.subres_mask_qwords; i++) {
							if (unlikely(i == subres_qword)) {
								br.groups[group_to_fill].subres_mask[i] = subres_mask_piece;
							} else {
								br.groups[group_to_fill].subres_mask[i] = 0;
							}
						}
					}
				}
			}
		} else {
			BarrierRequest &br = p_command_buffer->res_barriers_requests[res_states];
			br.dx_resource = p_resource->resource;
			br.subres_mask_qwords = STEPIFY(p_resource->states_ptr->subresource_states.size(), 64) / 64;
			CRASH_COND(p_resource->states_ptr->subresource_states.size() > BarrierRequest::MAX_SUBRESOURCES);
			br.planes = p_num_planes;
			br.groups[0].states = p_new_state;
			for (uint8_t i = 0; i < br.subres_mask_qwords; i++) {
				if (unlikely(i == subres_qword)) {
					br.groups[0].subres_mask[i] = subres_mask_piece;
				} else {
					br.groups[0].subres_mask[i] = 0;
				}
			}
			br.groups_count = 1;
		}
	}
}

void RenderingDeviceDriverD3D12::_resource_transitions_flush(CommandBufferInfo *p_command_buffer) {
	for (const KeyValue<ResourceInfo::States *, BarrierRequest> &E : p_command_buffer->res_barriers_requests) {
		ResourceInfo::States *res_states = E.key;
		const BarrierRequest &br = E.value;

		uint32_t num_subresources = res_states->subresource_states.size();

		// When there's not a lot of subresources, the empirical finding is that it's better
		// to avoid attempting the single-barrier optimization.
		static const uint32_t SINGLE_BARRIER_ATTEMPT_MAX_NUM_SUBRESOURCES = 48;

		bool may_do_single_barrier = br.groups_count == 1 && num_subresources * br.planes >= SINGLE_BARRIER_ATTEMPT_MAX_NUM_SUBRESOURCES;
		if (may_do_single_barrier) {
			// A single group means we may be able to do a single all-subresources barrier.

			{
				// First requisite is that all subresources are involved.

				uint8_t subres_mask_full_qwords = num_subresources / 64;
				for (uint32_t i = 0; i < subres_mask_full_qwords; i++) {
					if (br.groups[0].subres_mask[i] != UINT64_MAX) {
						may_do_single_barrier = false;
						break;
					}
				}
				if (may_do_single_barrier) {
					if (num_subresources % 64) {
						DEV_ASSERT(br.subres_mask_qwords == subres_mask_full_qwords + 1);
						uint64_t mask_tail_qword = 0;
						for (uint8_t i = 0; i < num_subresources % 64; i++) {
							mask_tail_qword |= ((uint64_t)1 << i);
						}
						if ((br.groups[0].subres_mask[subres_mask_full_qwords] & mask_tail_qword) != mask_tail_qword) {
							may_do_single_barrier = false;
						}
					}
				}
			}

			if (may_do_single_barrier) {
				// Second requisite is that the source state is the same for all.

				for (uint32_t i = 1; i < num_subresources; i++) {
					if (res_states->subresource_states[i] != res_states->subresource_states[0]) {
						may_do_single_barrier = false;
						break;
					}
				}

				if (may_do_single_barrier) {
					// Hurray!, we can do a single barrier (plus maybe a UAV one, too).

					bool just_written = res_states->subresource_states[0] == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					bool needs_uav_barrier = just_written && res_states->last_batch_with_uav_barrier != p_command_buffer->res_barriers_batch;

					uint32_t needed_barriers = (needs_uav_barrier ? 1 : 0) + 1;
					if (p_command_buffer->res_barriers.size() < p_command_buffer->res_barriers_count + needed_barriers) {
						p_command_buffer->res_barriers.resize(p_command_buffer->res_barriers_count + needed_barriers);
					}

					if (needs_uav_barrier) {
						p_command_buffer->res_barriers[p_command_buffer->res_barriers_count] = CD3DX12_RESOURCE_BARRIER::UAV(br.dx_resource);
						p_command_buffer->res_barriers_count++;
						res_states->last_batch_with_uav_barrier = p_command_buffer->res_barriers_batch;
					}

					if (res_states->subresource_states[0] != br.groups[0].states) {
						p_command_buffer->res_barriers[p_command_buffer->res_barriers_count] = CD3DX12_RESOURCE_BARRIER::Transition(br.dx_resource, res_states->subresource_states[0], br.groups[0].states, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
						p_command_buffer->res_barriers_count++;
					}

					for (uint32_t i = 0; i < num_subresources; i++) {
						res_states->subresource_states[i] = br.groups[0].states;
					}
				}
			}
		}

		if (!may_do_single_barrier) {
			for (uint8_t i = 0; i < br.groups_count; i++) {
				const BarrierRequest::Group &g = E.value.groups[i];

				if (unlikely(g.states == BarrierRequest::DELETED_GROUP)) {
					continue;
				}

				uint32_t subresource = 0;
				do {
					uint64_t subres_mask_piece = ((uint64_t)1 << (subresource % 64));
					uint8_t subres_qword = subresource / 64;

					if (likely(g.subres_mask[subres_qword] == 0)) {
						subresource += 64;
						continue;
					}

					if (likely(!(g.subres_mask[subres_qword] & subres_mask_piece))) {
						subresource++;
						continue;
					}

					D3D12_RESOURCE_STATES *curr_state = &res_states->subresource_states[subresource];

					bool just_written = *curr_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					bool needs_uav_barrier = just_written && res_states->last_batch_with_uav_barrier != p_command_buffer->res_barriers_batch;

					uint32_t needed_barriers = (needs_uav_barrier ? 1 : 0) + br.planes;
					if (p_command_buffer->res_barriers.size() < p_command_buffer->res_barriers_count + needed_barriers) {
						p_command_buffer->res_barriers.resize(p_command_buffer->res_barriers_count + needed_barriers);
					}

					if (needs_uav_barrier) {
						p_command_buffer->res_barriers[p_command_buffer->res_barriers_count] = CD3DX12_RESOURCE_BARRIER::UAV(br.dx_resource);
						p_command_buffer->res_barriers_count++;
						res_states->last_batch_with_uav_barrier = p_command_buffer->res_barriers_batch;
					}

					if (*curr_state != g.states) {
						for (uint8_t k = 0; k < br.planes; k++) {
							p_command_buffer->res_barriers[p_command_buffer->res_barriers_count] = CD3DX12_RESOURCE_BARRIER::Transition(br.dx_resource, *curr_state, g.states, subresource + k * num_subresources);
							p_command_buffer->res_barriers_count++;
						}
					}

					*curr_state = g.states;

					subresource++;
				} while (subresource < num_subresources);
			}
		}
	}

	if (p_command_buffer->res_barriers_count) {
		p_command_buffer->cmd_list->ResourceBarrier(p_command_buffer->res_barriers_count, p_command_buffer->res_barriers.ptr());
		p_command_buffer->res_barriers_requests.clear();
	}

	p_command_buffer->res_barriers_count = 0;
	p_command_buffer->res_barriers_batch++;
}

/*****************/
/**** BUFFERS ****/
/*****************/

RDD::BufferID RenderingDeviceDriverD3D12::buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) {
	// D3D12 debug layers complain at CBV creation time if the size is not multiple of the value per the spec
	// but also if you give a rounded size at that point because it will extend beyond the
	// memory of the resource. Therefore, it seems the only way is to create it with a
	// rounded size.
	CD3DX12_RESOURCE_DESC1 resource_desc = CD3DX12_RESOURCE_DESC1::Buffer(STEPIFY(p_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	if (p_usage.has_flag(RDD::BUFFER_USAGE_STORAGE_BIT)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	} else {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
	switch (p_allocation_type) {
		case MEMORY_ALLOCATION_TYPE_CPU: {
			bool is_src = p_usage.has_flag(BUFFER_USAGE_TRANSFER_FROM_BIT);
			bool is_dst = p_usage.has_flag(BUFFER_USAGE_TRANSFER_TO_BIT);
			if (is_src && !is_dst) {
				// Looks like a staging buffer: CPU maps, writes sequentially, then GPU copies to VRAM.
				allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
				initial_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			}
			if (is_dst && !is_src) {
				// Looks like a readback buffer: GPU copies from VRAM, then CPU maps and reads.
				allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
				initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
			}
		} break;
		case MEMORY_ALLOCATION_TYPE_GPU: {
			// Use default parameters.
		} break;
	}

	ComPtr<ID3D12Resource> buffer;
	ComPtr<D3D12MA::Allocation> allocation;
	HRESULT res;
	if (barrier_capabilities.enhanced_barriers_supported) {
		res = allocator->CreateResource3(
				&allocation_desc,
				&resource_desc,
				D3D12_BARRIER_LAYOUT_UNDEFINED,
				nullptr,
				0,
				nullptr,
				allocation.GetAddressOf(),
				IID_PPV_ARGS(buffer.GetAddressOf()));
	} else {
		res = allocator->CreateResource(
				&allocation_desc,
				reinterpret_cast<const D3D12_RESOURCE_DESC *>(&resource_desc),
				initial_state,
				nullptr,
				allocation.GetAddressOf(),
				IID_PPV_ARGS(buffer.GetAddressOf()));
	}

	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), BufferID(), "Can't create buffer of size: " + itos(p_size) + ", error " + vformat("0x%08ux", (uint64_t)res) + ".");

	// Bookkeep.

	BufferInfo *buf_info = VersatileResource::allocate<BufferInfo>(resources_allocator);
	buf_info->resource = buffer.Get();
	buf_info->owner_info.resource = buffer;
	buf_info->owner_info.allocation = allocation;
	buf_info->owner_info.states.subresource_states.push_back(initial_state);
	buf_info->states_ptr = &buf_info->owner_info.states;
	buf_info->size = p_size;
	buf_info->flags.usable_as_uav = (resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	return BufferID(buf_info);
}

bool RenderingDeviceDriverD3D12::buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) {
	BufferInfo *buf_info = (BufferInfo *)p_buffer.id;
	buf_info->texel_format = p_format;
	return true;
}

void RenderingDeviceDriverD3D12::buffer_free(BufferID p_buffer) {
	BufferInfo *buf_info = (BufferInfo *)p_buffer.id;
	VersatileResource::free(resources_allocator, buf_info);
}

uint64_t RenderingDeviceDriverD3D12::buffer_get_allocation_size(BufferID p_buffer) {
	const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
	return buf_info->owner_info.allocation ? buf_info->owner_info.allocation->GetSize() : 0;
}

uint8_t *RenderingDeviceDriverD3D12::buffer_map(BufferID p_buffer) {
	const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
	void *data_ptr = nullptr;
	HRESULT res = buf_info->resource->Map(0, &VOID_RANGE, &data_ptr);
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), nullptr, "Map failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	return (uint8_t *)data_ptr;
}

void RenderingDeviceDriverD3D12::buffer_unmap(BufferID p_buffer) {
	const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
	buf_info->resource->Unmap(0, &VOID_RANGE);
}

/*****************/
/**** TEXTURE ****/
/*****************/

static const D3D12_SRV_DIMENSION RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_SRV[RD::TEXTURE_TYPE_MAX] = {
	D3D12_SRV_DIMENSION_TEXTURE1D,
	D3D12_SRV_DIMENSION_TEXTURE2D,
	D3D12_SRV_DIMENSION_TEXTURE3D,
	D3D12_SRV_DIMENSION_TEXTURECUBE,
	D3D12_SRV_DIMENSION_TEXTURE1DARRAY,
	D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
	D3D12_SRV_DIMENSION_TEXTURECUBEARRAY,
};

static const D3D12_SRV_DIMENSION RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_SRV_MS[RD::TEXTURE_TYPE_MAX] = {
	D3D12_SRV_DIMENSION_UNKNOWN,
	D3D12_SRV_DIMENSION_TEXTURE2DMS,
	D3D12_SRV_DIMENSION_UNKNOWN,
	D3D12_SRV_DIMENSION_UNKNOWN,
	D3D12_SRV_DIMENSION_UNKNOWN,
	D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY,
	D3D12_SRV_DIMENSION_UNKNOWN,
};

static const D3D12_UAV_DIMENSION RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_UAV[RD::TEXTURE_TYPE_MAX] = {
	D3D12_UAV_DIMENSION_TEXTURE1D,
	D3D12_UAV_DIMENSION_TEXTURE2D,
	D3D12_UAV_DIMENSION_TEXTURE3D,
	D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
	D3D12_UAV_DIMENSION_TEXTURE1DARRAY,
	D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
	D3D12_UAV_DIMENSION_TEXTURE2DARRAY,
};

uint32_t RenderingDeviceDriverD3D12::_find_max_common_supported_sample_count(VectorView<DXGI_FORMAT> p_formats) {
	uint32_t common = UINT32_MAX;

	MutexLock lock(format_sample_counts_mask_cache_mutex);
	for (uint32_t i = 0; i < p_formats.size(); i++) {
		if (format_sample_counts_mask_cache.has(p_formats[i])) {
			common &= format_sample_counts_mask_cache[p_formats[i]];
		} else {
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msql = {};
			msql.Format = p_formats[i];
			uint32_t mask = 0;
			for (int samples = 1 << (TEXTURE_SAMPLES_MAX - 1); samples >= 1; samples /= 2) {
				msql.SampleCount = (UINT)samples;
				HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msql, sizeof(msql));
				if (SUCCEEDED(res) && msql.NumQualityLevels) {
					int bit = get_shift_from_power_of_2(samples);
					ERR_FAIL_COND_V(bit == -1, 1);
					mask |= (uint32_t)(1 << bit);
				}
			}
			format_sample_counts_mask_cache.insert(p_formats[i], mask);
			common &= mask;
		}
	}
	if (common == UINT32_MAX) {
		return 1;
	} else {
		return ((uint32_t)1 << nearest_shift(common));
	}
}

UINT RenderingDeviceDriverD3D12::_compute_component_mapping(const RDD::TextureView &p_view) {
	UINT base_swizzle = RD_TO_D3D12_FORMAT[p_view.format].swizzle;

	D3D12_SHADER_COMPONENT_MAPPING component_swizzles[TEXTURE_SWIZZLE_MAX] = {
		D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0, // Unused.
		D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
		D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
		// These will be D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_*.
		D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(0, base_swizzle),
		D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(1, base_swizzle),
		D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(2, base_swizzle),
		D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(3, base_swizzle),
	};

	return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
			p_view.swizzle_r == TEXTURE_SWIZZLE_IDENTITY ? component_swizzles[TEXTURE_SWIZZLE_R] : component_swizzles[p_view.swizzle_r],
			p_view.swizzle_g == TEXTURE_SWIZZLE_IDENTITY ? component_swizzles[TEXTURE_SWIZZLE_G] : component_swizzles[p_view.swizzle_g],
			p_view.swizzle_b == TEXTURE_SWIZZLE_IDENTITY ? component_swizzles[TEXTURE_SWIZZLE_B] : component_swizzles[p_view.swizzle_b],
			p_view.swizzle_a == TEXTURE_SWIZZLE_IDENTITY ? component_swizzles[TEXTURE_SWIZZLE_A] : component_swizzles[p_view.swizzle_a]);
}

UINT RenderingDeviceDriverD3D12::_compute_plane_slice(DataFormat p_format, BitField<TextureAspectBits> p_aspect_bits) {
	TextureAspect aspect = TEXTURE_ASPECT_MAX;

	if (p_aspect_bits.has_flag(TEXTURE_ASPECT_COLOR_BIT)) {
		DEV_ASSERT(aspect == TEXTURE_ASPECT_MAX);
		aspect = TEXTURE_ASPECT_COLOR;
	}
	if (p_aspect_bits.has_flag(TEXTURE_ASPECT_DEPTH_BIT)) {
		DEV_ASSERT(aspect == TEXTURE_ASPECT_MAX);
		aspect = TEXTURE_ASPECT_DEPTH;
	} else if (p_aspect_bits.has_flag(TEXTURE_ASPECT_STENCIL_BIT)) {
		DEV_ASSERT(aspect == TEXTURE_ASPECT_MAX);
		aspect = TEXTURE_ASPECT_STENCIL;
	}

	DEV_ASSERT(aspect != TEXTURE_ASPECT_MAX);

	return _compute_plane_slice(p_format, aspect);
}

UINT RenderingDeviceDriverD3D12::_compute_plane_slice(DataFormat p_format, TextureAspect p_aspect) {
	switch (p_aspect) {
		case TEXTURE_ASPECT_COLOR:
			// The plane must be 0 for the color aspect (assuming the format is a regular color one, which must be the case).
			return 0;
		case TEXTURE_ASPECT_DEPTH:
			// The plane must be 0 for the color or depth aspect
			return 0;
		case TEXTURE_ASPECT_STENCIL:
			// The plane may be 0 for the stencil aspect (if the format is stencil-only), or 1 (if the format is depth-stencil; other cases are ill).
			return format_get_plane_count(p_format) == 2 ? 1 : 0;
		default:
			DEV_ASSERT(false);
			return 0;
	}
}

UINT RenderingDeviceDriverD3D12::_compute_subresource_from_layers(TextureInfo *p_texture, const TextureSubresourceLayers &p_layers, uint32_t p_layer_offset) {
	return D3D12CalcSubresource(p_layers.mipmap, p_layers.base_layer + p_layer_offset, _compute_plane_slice(p_texture->format, p_layers.aspect), p_texture->desc.MipLevels, p_texture->desc.ArraySize());
}

void RenderingDeviceDriverD3D12::_discard_texture_subresources(const TextureInfo *p_tex_info, const CommandBufferInfo *p_cmd_buf_info) {
	uint32_t planes = 1;
	if ((p_tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
		planes = format_get_plane_count(p_tex_info->format);
	}
	D3D12_DISCARD_REGION dr = {};
	dr.NumRects = p_cmd_buf_info->render_pass_state.region_is_all ? 0 : 1;
	dr.pRects = p_cmd_buf_info->render_pass_state.region_is_all ? nullptr : &p_cmd_buf_info->render_pass_state.region_rect;
	dr.FirstSubresource = UINT_MAX;
	dr.NumSubresources = 0;
	for (uint32_t u = 0; u < planes; u++) {
		for (uint32_t v = 0; v < p_tex_info->layers; v++) {
			for (uint32_t w = 0; w < p_tex_info->mipmaps; w++) {
				UINT subresource = D3D12CalcSubresource(
						p_tex_info->base_mip + w,
						p_tex_info->base_layer + v,
						u,
						p_tex_info->desc.MipLevels,
						p_tex_info->desc.ArraySize());
				if (dr.NumSubresources == 0) {
					dr.FirstSubresource = subresource;
					dr.NumSubresources = 1;
				} else if (dr.FirstSubresource + dr.NumSubresources == subresource) {
					dr.NumSubresources++;
				} else {
					p_cmd_buf_info->cmd_list->DiscardResource(p_tex_info->resource, &dr);
					dr.FirstSubresource = subresource;
					dr.NumSubresources = 1;
				}
			}
		}
	}
	if (dr.NumSubresources) {
		p_cmd_buf_info->cmd_list->DiscardResource(p_tex_info->resource, &dr);
	}
}

bool RenderingDeviceDriverD3D12::_unordered_access_supported_by_format(DataFormat p_format) {
	switch (p_format) {
		case DATA_FORMAT_R4G4_UNORM_PACK8:
		case DATA_FORMAT_R4G4B4A4_UNORM_PACK16:
		case DATA_FORMAT_B4G4R4A4_UNORM_PACK16:
		case DATA_FORMAT_R5G6B5_UNORM_PACK16:
		case DATA_FORMAT_B5G6R5_UNORM_PACK16:
		case DATA_FORMAT_R5G5B5A1_UNORM_PACK16:
		case DATA_FORMAT_B5G5R5A1_UNORM_PACK16:
		case DATA_FORMAT_A1R5G5B5_UNORM_PACK16:
		case DATA_FORMAT_A8B8G8R8_UNORM_PACK32:
		case DATA_FORMAT_A8B8G8R8_SNORM_PACK32:
		case DATA_FORMAT_A8B8G8R8_USCALED_PACK32:
		case DATA_FORMAT_A8B8G8R8_SSCALED_PACK32:
		case DATA_FORMAT_A8B8G8R8_UINT_PACK32:
		case DATA_FORMAT_A8B8G8R8_SINT_PACK32:
		case DATA_FORMAT_A8B8G8R8_SRGB_PACK32:
		case DATA_FORMAT_A2R10G10B10_UNORM_PACK32:
		case DATA_FORMAT_A2R10G10B10_SNORM_PACK32:
		case DATA_FORMAT_A2R10G10B10_USCALED_PACK32:
		case DATA_FORMAT_A2R10G10B10_SSCALED_PACK32:
		case DATA_FORMAT_A2R10G10B10_UINT_PACK32:
		case DATA_FORMAT_A2R10G10B10_SINT_PACK32:
		case DATA_FORMAT_A2B10G10R10_UNORM_PACK32:
		case DATA_FORMAT_A2B10G10R10_SNORM_PACK32:
		case DATA_FORMAT_A2B10G10R10_USCALED_PACK32:
		case DATA_FORMAT_A2B10G10R10_SSCALED_PACK32:
		case DATA_FORMAT_A2B10G10R10_UINT_PACK32:
		case DATA_FORMAT_A2B10G10R10_SINT_PACK32:
		case DATA_FORMAT_B10G11R11_UFLOAT_PACK32:
		case DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		case DATA_FORMAT_X8_D24_UNORM_PACK32:
		case DATA_FORMAT_R10X6_UNORM_PACK16:
		case DATA_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case DATA_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case DATA_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case DATA_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case DATA_FORMAT_R12X4_UNORM_PACK16:
		case DATA_FORMAT_R12X4G12X4_UNORM_2PACK16:
		case DATA_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case DATA_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case DATA_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
			return false;
		default:
			return true;
	}
}

RDD::TextureID RenderingDeviceDriverD3D12::texture_create(const TextureFormat &p_format, const TextureView &p_view) {
	// Using D3D12_RESOURCE_DESC1. Thanks to the layout, it's sliceable down to D3D12_RESOURCE_DESC if needed.
	CD3DX12_RESOURCE_DESC1 resource_desc = {};
	resource_desc.Dimension = RD_TEXTURE_TYPE_TO_D3D12_RESOURCE_DIMENSION[p_format.texture_type];
	resource_desc.Alignment = 0; // D3D12MA will override this to use a smaller alignment than the default if possible.

	resource_desc.Width = p_format.width;
	resource_desc.Height = p_format.height;
	resource_desc.DepthOrArraySize = p_format.depth * p_format.array_layers;
	resource_desc.MipLevels = p_format.mipmaps;

	// Format.
	bool cross_family_sharing = false;
	bool relaxed_casting_available = false;
	DXGI_FORMAT *relaxed_casting_formats = nullptr;
	uint32_t relaxed_casting_format_count = 0;
	{
		resource_desc.Format = RD_TO_D3D12_FORMAT[p_format.format].family;

		// If views of different families are wanted, special setup is needed for proper sharing among them.
		// If the driver reports relaxed casting is, leverage its new extended resource creation API (via D3D12MA).
		if (p_format.shareable_formats.size() && format_capabilities.relaxed_casting_supported) {
			relaxed_casting_available = true;
			relaxed_casting_formats = ALLOCA_ARRAY(DXGI_FORMAT, p_format.shareable_formats.size() + 1);
			relaxed_casting_formats[0] = RD_TO_D3D12_FORMAT[p_format.format].general_format;
			relaxed_casting_format_count++;
		}

		HashMap<DataFormat, D3D12_RESOURCE_FLAGS> aliases_forbidden_flags;
		for (int i = 0; i < p_format.shareable_formats.size(); i++) {
			DataFormat curr_format = p_format.shareable_formats[i];
			String format_text = "'" + String(FORMAT_NAMES[p_format.format]) + "'";

			ERR_FAIL_COND_V_MSG(RD_TO_D3D12_FORMAT[curr_format].family == DXGI_FORMAT_UNKNOWN, TextureID(), "Format " + format_text + " is not supported.");

			if (RD_TO_D3D12_FORMAT[curr_format].family != RD_TO_D3D12_FORMAT[p_format.format].family) {
				cross_family_sharing = true;
			}

			if (relaxed_casting_available) {
				relaxed_casting_formats[relaxed_casting_format_count] = RD_TO_D3D12_FORMAT[curr_format].general_format;
				relaxed_casting_format_count++;
			}
		}

		if (cross_family_sharing && !relaxed_casting_available) {
			// Per https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_texture_layout.
			if (p_format.texture_type == TEXTURE_TYPE_1D) {
				ERR_FAIL_V_MSG(TextureID(), "This texture's views require aliasing, but that's not supported for a 1D texture.");
			}
			if (p_format.samples != TEXTURE_SAMPLES_1) {
				ERR_FAIL_V_MSG(TextureID(), "This texture's views require aliasing, but that's not supported for a multi-sample texture.");
			}
			if ((p_format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
				ERR_FAIL_V_MSG(TextureID(), "This texture's views require aliasing, but that's not supported for a depth-stencil texture.");
			}
			if (RD_TO_D3D12_FORMAT[p_format.format].family == DXGI_FORMAT_R32G32B32_TYPELESS) {
				ERR_FAIL_V_MSG(TextureID(), "This texture's views require aliasing, but that's not supported for an R32G32B32 texture.");
			}
		}
	}

	// Usage.
	if ((p_format.usage_bits & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	} else {
		if ((p_format.usage_bits & TEXTURE_USAGE_CAN_COPY_TO_BIT) && _unordered_access_supported_by_format(p_format.format)) {
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // For clearing via UAV.
		}
	}
	if ((p_format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if ((p_format.usage_bits & TEXTURE_USAGE_STORAGE_BIT)) {
		resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if ((p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
		// For VRS images we can't use the typeless format.
		resource_desc.Format = DXGI_FORMAT_R8_UINT;
	}

	resource_desc.SampleDesc = {};
	DXGI_FORMAT format_to_test = (resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ? RD_TO_D3D12_FORMAT[p_format.format].dsv_format : RD_TO_D3D12_FORMAT[p_format.format].general_format;
	if (!(resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
		resource_desc.SampleDesc.Count = MIN(
				_find_max_common_supported_sample_count(format_to_test),
				TEXTURE_SAMPLES_COUNT[p_format.samples]);
	} else {
		// No MSAA in D3D12 if storage. May have become possible recently where supported, though.
		resource_desc.SampleDesc.Count = 1;
	}
	resource_desc.SampleDesc.Quality = resource_desc.SampleDesc.Count == 1 ? 0 : DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;

	// Create.

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_DEFAULT;
	if ((resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))) {
		allocation_desc.ExtraHeapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	} else {
		allocation_desc.ExtraHeapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	}
	if ((resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
		allocation_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	}

	D3D12_RESOURCE_STATES initial_state = {};
	ID3D12Resource *texture = nullptr;
	ComPtr<ID3D12Resource> main_texture;
	ComPtr<D3D12MA::Allocation> allocation;
	static const FLOAT black[4] = {};
	D3D12_CLEAR_VALUE clear_value = CD3DX12_CLEAR_VALUE(RD_TO_D3D12_FORMAT[p_format.format].general_format, black);
	D3D12_CLEAR_VALUE *clear_value_ptr = (resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? &clear_value : nullptr;
	{
		HRESULT res = E_FAIL;
		if (barrier_capabilities.enhanced_barriers_supported || (cross_family_sharing && relaxed_casting_available)) {
			// Create with undefined layout if enhanced barriers are supported. Leave as common otherwise for interop with legacy barriers.
			D3D12_BARRIER_LAYOUT initial_layout = barrier_capabilities.enhanced_barriers_supported ? D3D12_BARRIER_LAYOUT_UNDEFINED : D3D12_BARRIER_LAYOUT_COMMON;
			res = allocator->CreateResource3(
					&allocation_desc,
					&resource_desc,
					initial_layout,
					clear_value_ptr,
					relaxed_casting_format_count,
					relaxed_casting_formats,
					allocation.GetAddressOf(),
					IID_PPV_ARGS(main_texture.GetAddressOf()));
			initial_state = D3D12_RESOURCE_STATE_COMMON;
		} else {
			res = allocator->CreateResource(
					&allocation_desc,
					(D3D12_RESOURCE_DESC *)&resource_desc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					clear_value_ptr,
					allocation.GetAddressOf(),
					IID_PPV_ARGS(main_texture.GetAddressOf()));
			initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), TextureID(), "CreateResource failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
		texture = main_texture.Get();
	}

	// Describe views.

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	{
		srv_desc.Format = RD_TO_D3D12_FORMAT[p_view.format].general_format;
		srv_desc.ViewDimension = p_format.samples == TEXTURE_SAMPLES_1 ? RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_SRV[p_format.texture_type] : RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_SRV_MS[p_format.texture_type];
		srv_desc.Shader4ComponentMapping = _compute_component_mapping(p_view);

		switch (srv_desc.ViewDimension) {
			case D3D12_SRV_DIMENSION_TEXTURE1D: {
				srv_desc.Texture1D.MipLevels = p_format.mipmaps;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
				srv_desc.Texture1DArray.MipLevels = p_format.mipmaps;
				srv_desc.Texture1DArray.ArraySize = p_format.array_layers;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE2D: {
				srv_desc.Texture2D.MipLevels = p_format.mipmaps;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
				srv_desc.Texture2DArray.MipLevels = p_format.mipmaps;
				srv_desc.Texture2DArray.ArraySize = p_format.array_layers;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
				srv_desc.Texture2DMSArray.ArraySize = p_format.array_layers;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
				srv_desc.TextureCubeArray.MipLevels = p_format.mipmaps;
				srv_desc.TextureCubeArray.NumCubes = p_format.array_layers / 6;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURE3D: {
				srv_desc.Texture3D.MipLevels = p_format.mipmaps;
			} break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE: {
				srv_desc.TextureCube.MipLevels = p_format.mipmaps;
			} break;
			default: {
			}
		}
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC main_uav_desc = {};
	{
		main_uav_desc.Format = RD_TO_D3D12_FORMAT[p_format.format].general_format;
		main_uav_desc.ViewDimension = p_format.samples == TEXTURE_SAMPLES_1 ? RD_TEXTURE_TYPE_TO_D3D12_VIEW_DIMENSION_FOR_UAV[p_format.texture_type] : D3D12_UAV_DIMENSION_UNKNOWN;

		switch (main_uav_desc.ViewDimension) {
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
				main_uav_desc.Texture1DArray.ArraySize = p_format.array_layers;
			} break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
				// Either for an actual 2D texture array, cubemap or cubemap array.
				main_uav_desc.Texture2DArray.ArraySize = p_format.array_layers;
			} break;
			case D3D12_UAV_DIMENSION_TEXTURE3D: {
				main_uav_desc.Texture3D.WSize = p_format.depth;
			} break;
			default: {
			}
		}
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = main_uav_desc;
	uav_desc.Format = RD_TO_D3D12_FORMAT[p_view.format].general_format;

	// Bookkeep.

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);
	tex_info->resource = texture;
	tex_info->owner_info.resource = main_texture;
	tex_info->owner_info.allocation = allocation;
	tex_info->owner_info.states.subresource_states.resize(p_format.mipmaps * p_format.array_layers);
	for (uint32_t i = 0; i < tex_info->owner_info.states.subresource_states.size(); i++) {
		tex_info->owner_info.states.subresource_states[i] = initial_state;
	}
	tex_info->states_ptr = &tex_info->owner_info.states;
	tex_info->format = p_format.format;
	tex_info->desc = *(CD3DX12_RESOURCE_DESC *)&resource_desc;
	tex_info->base_layer = 0;
	tex_info->layers = resource_desc.ArraySize();
	tex_info->base_mip = 0;
	tex_info->mipmaps = resource_desc.MipLevels;
	tex_info->view_descs.srv = srv_desc;
	tex_info->view_descs.uav = uav_desc;

	if (!barrier_capabilities.enhanced_barriers_supported && (p_format.usage_bits & (TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT))) {
		// Fallback to clear resources when they're first used in a uniform set. Not necessary if enhanced barriers
		// are supported, as the discard flag will be used instead when transitioning from an undefined layout.
		textures_pending_clear.add(&tex_info->pending_clear);
	}

	return TextureID(tex_info);
}

RDD::TextureID RenderingDeviceDriverD3D12::texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil) {
	ERR_FAIL_V_MSG(TextureID(), "Unimplemented!");
}

RDD::TextureID RenderingDeviceDriverD3D12::texture_create_shared(TextureID p_original_texture, const TextureView &p_view) {
	return _texture_create_shared_from_slice(p_original_texture, p_view, (TextureSliceType)-1, 0, 0, 0, 0);
}

RDD::TextureID RenderingDeviceDriverD3D12::texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
	return _texture_create_shared_from_slice(p_original_texture, p_view, p_slice_type, p_layer, p_layers, p_mipmap, p_mipmaps);
}

RDD::TextureID RenderingDeviceDriverD3D12::_texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
	TextureInfo *owner_tex_info = (TextureInfo *)p_original_texture.id;
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(!owner_tex_info->owner_info.allocation, TextureID());
#endif

	ComPtr<ID3D12Resource> new_texture;
	ComPtr<D3D12MA::Allocation> new_allocation;
	ID3D12Resource *resource = owner_tex_info->resource;
	CD3DX12_RESOURCE_DESC new_tex_resource_desc = owner_tex_info->desc;

	// Describe views.

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = owner_tex_info->view_descs.srv;
	{
		srv_desc.Format = RD_TO_D3D12_FORMAT[p_view.format].general_format;
		srv_desc.Shader4ComponentMapping = _compute_component_mapping(p_view);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = owner_tex_info->view_descs.uav;
	{
		uav_desc.Format = RD_TO_D3D12_FORMAT[p_view.format].general_format;
	}

	if (p_slice_type != (TextureSliceType)-1) {
		// Complete description with slicing.

		switch (p_slice_type) {
			case TEXTURE_SLICE_2D: {
				if (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D && p_layer == 0) {
					srv_desc.Texture2D.MostDetailedMip = p_mipmap;
					srv_desc.Texture2D.MipLevels = p_mipmaps;

					DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D);
					uav_desc.Texture1D.MipSlice = p_mipmap;
				} else if (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS && p_layer == 0) {
					DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN);
				} else if ((srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY || (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D && p_layer)) || srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE || srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY) {
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srv_desc.Texture2DArray.MostDetailedMip = p_mipmap;
					srv_desc.Texture2DArray.MipLevels = p_mipmaps;
					srv_desc.Texture2DArray.FirstArraySlice = p_layer;
					srv_desc.Texture2DArray.ArraySize = 1;
					srv_desc.Texture2DArray.PlaneSlice = 0;
					srv_desc.Texture2DArray.ResourceMinLODClamp = 0.0f;

					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uav_desc.Texture2DArray.MipSlice = p_mipmap;
					uav_desc.Texture2DArray.FirstArraySlice = p_layer;
					uav_desc.Texture2DArray.ArraySize = 1;
					uav_desc.Texture2DArray.PlaneSlice = 0;
				} else if ((srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY || (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS && p_layer))) {
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srv_desc.Texture2DMSArray.FirstArraySlice = p_layer;
					srv_desc.Texture2DMSArray.ArraySize = 1;

					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;
				} else {
					DEV_ASSERT(false);
				}
			} break;
			case TEXTURE_SLICE_CUBEMAP: {
				if (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE || p_layer == 0) {
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					srv_desc.TextureCube.MostDetailedMip = p_mipmap;
					srv_desc.TextureCube.MipLevels = p_mipmaps;

					DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY);
					uav_desc.Texture2DArray.MipSlice = p_mipmap;
					uav_desc.Texture2DArray.FirstArraySlice = p_layer;
					uav_desc.Texture2DArray.ArraySize = 6;
					uav_desc.Texture2DArray.PlaneSlice = 0;
				} else if (srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY || p_layer != 0) {
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
					srv_desc.TextureCubeArray.MostDetailedMip = p_mipmap;
					srv_desc.TextureCubeArray.MipLevels = p_mipmaps;
					srv_desc.TextureCubeArray.First2DArrayFace = p_layer;
					srv_desc.TextureCubeArray.NumCubes = 1;
					srv_desc.TextureCubeArray.ResourceMinLODClamp = 0.0f;

					DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY);
					uav_desc.Texture2DArray.MipSlice = p_mipmap;
					uav_desc.Texture2DArray.FirstArraySlice = p_layer;
					uav_desc.Texture2DArray.ArraySize = 6;
					uav_desc.Texture2DArray.PlaneSlice = 0;
				} else {
					DEV_ASSERT(false);
				}
			} break;
			case TEXTURE_SLICE_3D: {
				DEV_ASSERT(srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D);
				srv_desc.Texture3D.MostDetailedMip = p_mipmap;
				srv_desc.Texture3D.MipLevels = p_mipmaps;

				DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D);
				uav_desc.Texture3D.MipSlice = p_mipmap;
				uav_desc.Texture3D.WSize = -1;
			} break;
			case TEXTURE_SLICE_2D_ARRAY: {
				DEV_ASSERT(srv_desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY);
				srv_desc.Texture2DArray.MostDetailedMip = p_mipmap;
				srv_desc.Texture2DArray.MipLevels = p_mipmaps;
				srv_desc.Texture2DArray.FirstArraySlice = p_layer;
				srv_desc.Texture2DArray.ArraySize = p_layers;

				DEV_ASSERT(uav_desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY);
				uav_desc.Texture2DArray.MipSlice = p_mipmap;
				uav_desc.Texture2DArray.FirstArraySlice = p_layer;
				uav_desc.Texture2DArray.ArraySize = p_layers;
			} break;
			default:
				break;
		}
	}

	// Bookkeep.

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);
	tex_info->resource = resource;
	tex_info->states_ptr = owner_tex_info->states_ptr;
	tex_info->format = p_view.format;
	tex_info->desc = new_tex_resource_desc;
	if (p_slice_type == (TextureSliceType)-1) {
		tex_info->base_layer = owner_tex_info->base_layer;
		tex_info->layers = owner_tex_info->layers;
		tex_info->base_mip = owner_tex_info->base_mip;
		tex_info->mipmaps = owner_tex_info->mipmaps;
	} else {
		tex_info->base_layer = p_layer;
		tex_info->layers = p_layers;
		tex_info->base_mip = p_mipmap;
		tex_info->mipmaps = p_mipmaps;
	}
	tex_info->view_descs.srv = srv_desc;
	tex_info->view_descs.uav = uav_desc;
	tex_info->main_texture = owner_tex_info;

	return TextureID(tex_info);
}

void RenderingDeviceDriverD3D12::texture_free(TextureID p_texture) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	VersatileResource::free(resources_allocator, tex_info);
}

uint64_t RenderingDeviceDriverD3D12::texture_get_allocation_size(TextureID p_texture) {
	const TextureInfo *tex_info = (const TextureInfo *)p_texture.id;
	return tex_info->owner_info.allocation ? tex_info->owner_info.allocation->GetSize() : 0;
}

void RenderingDeviceDriverD3D12::texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;

	UINT subresource = tex_info->desc.CalcSubresource(p_subresource.mipmap, p_subresource.layer, 0);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT64 subresource_total_size = 0;
	device->GetCopyableFootprints(
			&tex_info->desc,
			subresource,
			1,
			0,
			&footprint,
			nullptr,
			nullptr,
			&subresource_total_size);

	*r_layout = {};
	r_layout->offset = footprint.Offset;
	r_layout->size = subresource_total_size;
	r_layout->row_pitch = footprint.Footprint.RowPitch;
	r_layout->depth_pitch = subresource_total_size / tex_info->desc.Depth();
	r_layout->layer_pitch = subresource_total_size / tex_info->desc.ArraySize();
}

uint8_t *RenderingDeviceDriverD3D12::texture_map(TextureID p_texture, const TextureSubresource &p_subresource) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(tex_info->mapped_subresource != UINT_MAX, nullptr);
#endif

	UINT plane = _compute_plane_slice(tex_info->format, p_subresource.aspect);
	UINT subresource = tex_info->desc.CalcSubresource(p_subresource.mipmap, p_subresource.layer, plane);

	void *data_ptr = nullptr;
	HRESULT res = tex_info->resource->Map(subresource, &VOID_RANGE, &data_ptr);
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), nullptr, "Map failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	tex_info->mapped_subresource = subresource;
	return (uint8_t *)data_ptr;
}

void RenderingDeviceDriverD3D12::texture_unmap(TextureID p_texture) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(tex_info->mapped_subresource == UINT_MAX);
#endif
	tex_info->resource->Unmap(tex_info->mapped_subresource, &VOID_RANGE);
	tex_info->mapped_subresource = UINT_MAX;
}

BitField<RDD::TextureUsageBits> RenderingDeviceDriverD3D12::texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) {
	D3D12_FEATURE_DATA_FORMAT_SUPPORT srv_rtv_support = {};
	srv_rtv_support.Format = RD_TO_D3D12_FORMAT[p_format].general_format;
	if (srv_rtv_support.Format != DXGI_FORMAT_UNKNOWN) { // Some implementations (i.e., vkd3d-proton) error out instead of returning empty.
		HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &srv_rtv_support, sizeof(srv_rtv_support));
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	}

	D3D12_FEATURE_DATA_FORMAT_SUPPORT &uav_support = srv_rtv_support; // Fine for now.

	D3D12_FEATURE_DATA_FORMAT_SUPPORT dsv_support = {};
	dsv_support.Format = RD_TO_D3D12_FORMAT[p_format].dsv_format;
	if (dsv_support.Format != DXGI_FORMAT_UNKNOWN) { // See above.
		HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &dsv_support, sizeof(dsv_support));
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	}

	// Everything supported by default makes an all-or-nothing check easier for the caller.
	BitField<RDD::TextureUsageBits> supported = INT64_MAX;

	// Per https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1,
	// as long as the resource can be used as a texture, Sample() will work with point filter at least.
	// However, we've empirically found that checking for at least D3D12_FORMAT_SUPPORT1_SHADER_LOAD is needed.
	// That's almost good for integer formats. The problem is that theoretically there may be
	// float formats that support LOAD but not SAMPLE fully, so this check will not detect
	// such a flaw in the format. Linearly interpolated sampling would just not work on them.
	if (!(srv_rtv_support.Support1 & (D3D12_FORMAT_SUPPORT1_SHADER_LOAD | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) ||
			RD_TO_D3D12_FORMAT[p_format].general_format == DXGI_FORMAT_UNKNOWN) {
		supported.clear_flag(TEXTURE_USAGE_SAMPLING_BIT);
	}

	if (!(srv_rtv_support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) {
		supported.clear_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
	}
	if (!(dsv_support.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) {
		supported.clear_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}
	if (!(uav_support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)) { // Maybe check LOAD/STORE, too?
		supported.clear_flag(TEXTURE_USAGE_STORAGE_BIT);
	}
	if (!(uav_support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD)) { // Check a basic atomic at least.
		supported.clear_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
	}
	if (RD_TO_D3D12_FORMAT[p_format].general_format != DXGI_FORMAT_R8_UINT) {
		supported.clear_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT);
	}

	return supported;
}

bool RenderingDeviceDriverD3D12::texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) {
	r_raw_reinterpretation = false;

	if (format_capabilities.relaxed_casting_supported) {
		// Relaxed casting is supported, there should be no need to check for format family compatibility.
		return true;
	} else {
		TextureInfo *tex_info = (TextureInfo *)p_texture.id;
		if (tex_info->format == DATA_FORMAT_R16_UINT && p_format == DATA_FORMAT_R4G4B4A4_UNORM_PACK16) {
			// Specific cases that require buffer reinterpretation.
			r_raw_reinterpretation = true;
			return false;
		} else if (RD_TO_D3D12_FORMAT[tex_info->format].family != RD_TO_D3D12_FORMAT[p_format].family) {
			// Format family is different but copying resources directly is possible.
			return false;
		} else {
			// Format family is the same and the view can just cast the format.
			return true;
		}
	}
}

/*****************/
/**** SAMPLER ****/
/*****************/

static const D3D12_TEXTURE_ADDRESS_MODE RD_REPEAT_MODE_TO_D3D12_ADDRES_MODE[RDD::SAMPLER_REPEAT_MODE_MAX] = {
	D3D12_TEXTURE_ADDRESS_MODE_WRAP,
	D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
	D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	D3D12_TEXTURE_ADDRESS_MODE_BORDER,
	D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE,
};

static const FLOAT RD_TO_D3D12_SAMPLER_BORDER_COLOR[RDD::SAMPLER_BORDER_COLOR_MAX][4] = {
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 1 },
	{ 0, 0, 0, 1 },
	{ 1, 1, 1, 1 },
	{ 1, 1, 1, 1 },
};

RDD::SamplerID RenderingDeviceDriverD3D12::sampler_create(const SamplerState &p_state) {
	uint32_t slot = UINT32_MAX;

	if (samplers.is_empty()) {
		// Adding a seemigly busy slot 0 makes things easier elsewhere.
		samplers.push_back({});
		samplers.push_back({});
		slot = 1;
	} else {
		for (uint32_t i = 1; i < samplers.size(); i++) {
			if ((int)samplers[i].Filter == INT_MAX) {
				slot = i;
				break;
			}
		}
		if (slot == UINT32_MAX) {
			slot = samplers.size();
			samplers.push_back({});
		}
	}

	D3D12_SAMPLER_DESC &sampler_desc = samplers[slot];

	if (p_state.use_anisotropy) {
		sampler_desc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_STANDARD);
		sampler_desc.MaxAnisotropy = p_state.anisotropy_max;
	} else {
		static const D3D12_FILTER_TYPE RD_FILTER_TYPE_TO_D3D12[] = {
			D3D12_FILTER_TYPE_POINT, // SAMPLER_FILTER_NEAREST.
			D3D12_FILTER_TYPE_LINEAR, // SAMPLER_FILTER_LINEAR.
		};
		sampler_desc.Filter = D3D12_ENCODE_BASIC_FILTER(
				RD_FILTER_TYPE_TO_D3D12[p_state.min_filter],
				RD_FILTER_TYPE_TO_D3D12[p_state.mag_filter],
				RD_FILTER_TYPE_TO_D3D12[p_state.mip_filter],
				p_state.enable_compare ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD);
	}

	sampler_desc.AddressU = RD_REPEAT_MODE_TO_D3D12_ADDRES_MODE[p_state.repeat_u];
	sampler_desc.AddressV = RD_REPEAT_MODE_TO_D3D12_ADDRES_MODE[p_state.repeat_v];
	sampler_desc.AddressW = RD_REPEAT_MODE_TO_D3D12_ADDRES_MODE[p_state.repeat_w];

	for (int i = 0; i < 4; i++) {
		sampler_desc.BorderColor[i] = RD_TO_D3D12_SAMPLER_BORDER_COLOR[p_state.border_color][i];
	}

	sampler_desc.MinLOD = p_state.min_lod;
	sampler_desc.MaxLOD = p_state.max_lod;
	sampler_desc.MipLODBias = p_state.lod_bias;

	sampler_desc.ComparisonFunc = p_state.enable_compare ? RD_TO_D3D12_COMPARE_OP[p_state.compare_op] : D3D12_COMPARISON_FUNC_NEVER;

	// TODO: Emulate somehow?
	if (p_state.unnormalized_uvw) {
		WARN_PRINT("Creating a sampler with unnormalized UVW, which is not supported.");
	}

	return SamplerID(slot);
}

void RenderingDeviceDriverD3D12::sampler_free(SamplerID p_sampler) {
	samplers[p_sampler.id].Filter = (D3D12_FILTER)INT_MAX;
}

bool RenderingDeviceDriverD3D12::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) {
	D3D12_FEATURE_DATA_FORMAT_SUPPORT srv_rtv_support = {};
	srv_rtv_support.Format = RD_TO_D3D12_FORMAT[p_format].general_format;
	HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &srv_rtv_support, sizeof(srv_rtv_support));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	return (srv_rtv_support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
}

/**********************/
/**** VERTEX ARRAY ****/
/**********************/

RDD::VertexFormatID RenderingDeviceDriverD3D12::vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) {
	VertexFormatInfo *vf_info = VersatileResource::allocate<VertexFormatInfo>(resources_allocator);

	vf_info->input_elem_descs.resize(p_vertex_attribs.size());
	vf_info->vertex_buffer_strides.resize(p_vertex_attribs.size());
	for (uint32_t i = 0; i < p_vertex_attribs.size(); i++) {
		vf_info->input_elem_descs[i] = {};
		vf_info->input_elem_descs[i].SemanticName = "TEXCOORD";
		vf_info->input_elem_descs[i].SemanticIndex = p_vertex_attribs[i].location;
		vf_info->input_elem_descs[i].Format = RD_TO_D3D12_FORMAT[p_vertex_attribs[i].format].general_format;
		vf_info->input_elem_descs[i].InputSlot = i; // TODO: Can the same slot be used if data comes from the same buffer (regardless format)?
		vf_info->input_elem_descs[i].AlignedByteOffset = p_vertex_attribs[i].offset;
		if (p_vertex_attribs[i].frequency == VERTEX_FREQUENCY_INSTANCE) {
			vf_info->input_elem_descs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
			vf_info->input_elem_descs[i].InstanceDataStepRate = 1;
		} else {
			vf_info->input_elem_descs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			vf_info->input_elem_descs[i].InstanceDataStepRate = 0;
		}

		vf_info->vertex_buffer_strides[i] = p_vertex_attribs[i].stride;
	}

	return VertexFormatID(vf_info);
}

void RenderingDeviceDriverD3D12::vertex_format_free(VertexFormatID p_vertex_format) {
	VertexFormatInfo *vf_info = (VertexFormatInfo *)p_vertex_format.id;
	VersatileResource::free(resources_allocator, vf_info);
}

/******************/
/**** BARRIERS ****/
/******************/

static D3D12_BARRIER_ACCESS _rd_texture_layout_access_mask(RDD::TextureLayout p_texture_layout) {
	switch (p_texture_layout) {
		case RDD::TEXTURE_LAYOUT_STORAGE_OPTIMAL:
			return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		case RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_ACCESS_RENDER_TARGET;
		case RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		case RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		case RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		case RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL:
			return D3D12_BARRIER_ACCESS_COPY_SOURCE;
		case RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL:
			return D3D12_BARRIER_ACCESS_COPY_DEST;
		case RDD::TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL:
			return D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
		case RDD::TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL:
			return D3D12_BARRIER_ACCESS_RESOLVE_DEST;
		case RDD::TEXTURE_LAYOUT_VRS_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		default:
			return D3D12_BARRIER_ACCESS_NO_ACCESS;
	}
}

static void _rd_access_to_d3d12_and_mask(BitField<RDD::BarrierAccessBits> p_access, RDD::TextureLayout p_texture_layout, D3D12_BARRIER_ACCESS &r_access, D3D12_BARRIER_SYNC &r_sync_mask) {
	r_access = D3D12_BARRIER_ACCESS_COMMON;
	r_sync_mask = D3D12_BARRIER_SYNC_NONE;

	if (p_access.has_flag(RDD::BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		r_sync_mask |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_INDEX_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		r_sync_mask |= D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_DRAW;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
		r_sync_mask |= D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_UNIFORM_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
		r_sync_mask |= D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING |
				D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		r_sync_mask |= D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_RENDER_TARGET;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		r_sync_mask |= D3D12_BARRIER_SYNC_COPY;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		r_sync_mask |= D3D12_BARRIER_SYNC_COPY;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_RESOLVE_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
		r_sync_mask |= D3D12_BARRIER_SYNC_RESOLVE;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_RESOLVE_WRITE_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_RESOLVE_DEST;
		r_sync_mask |= D3D12_BARRIER_SYNC_RESOLVE;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		r_sync_mask |= D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_ALL_SHADING;
	}

	const D3D12_BARRIER_SYNC unordered_access_mask = D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING |
			D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING | D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;

	if (p_access.has_flag(RDD::BARRIER_ACCESS_STORAGE_CLEAR_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		r_sync_mask |= unordered_access_mask;
	}

	// These access bits only have compatibility with certain layouts unlike in Vulkan where they imply specific operations in the same layout.
	if (p_access.has_flag(RDD::BARRIER_ACCESS_SHADER_WRITE_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		r_sync_mask |= unordered_access_mask;
	} else if (p_access.has_flag(RDD::BARRIER_ACCESS_SHADER_READ_BIT)) {
		if (p_texture_layout == RDD::TEXTURE_LAYOUT_STORAGE_OPTIMAL) {
			// Unordered access must be enforced if the texture is using the storage layout.
			r_access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			r_sync_mask |= unordered_access_mask;
		} else {
			r_access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
			r_sync_mask |= D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING;
		}
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) || p_access.has_flag(RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		r_sync_mask |= D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_RENDER_TARGET;
	}

	if (p_access.has_flag(RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		r_sync_mask |= D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	} else if (p_access.has_flag(RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)) {
		r_access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		r_sync_mask |= D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	}
}

static void _rd_stages_to_d3d12(BitField<RDD::PipelineStageBits> p_stages, D3D12_BARRIER_SYNC &r_sync) {
	if (p_stages.has_flag(RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT)) {
		r_sync = D3D12_BARRIER_SYNC_ALL;
	} else {
		if (p_stages.has_flag(RDD::PIPELINE_STAGE_DRAW_INDIRECT_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_VERTEX_INPUT_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT) || p_stages.has_flag(RDD::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT) || p_stages.has_flag(RDD::PIPELINE_STAGE_GEOMETRY_SHADER_BIT)) {
			// There's no granularity for tessellation or geometry stages. The specification defines it as part of vertex shading.
			r_sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) || p_stages.has_flag(RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)) {
			// Covers both read and write operations for depth stencil.
			r_sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_COMPUTE_SHADER_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_COPY_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_COPY;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_RESOLVE_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_RESOLVE;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_CLEAR_STORAGE_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
		}

		if (p_stages.has_flag(RDD::PIPELINE_STAGE_ALL_GRAPHICS_BIT)) {
			r_sync |= D3D12_BARRIER_SYNC_DRAW;
		}
	}
}

static void _rd_stages_and_access_to_d3d12(BitField<RDD::PipelineStageBits> p_stages, RDD::TextureLayout p_texture_layout, BitField<RDD::BarrierAccessBits> p_access, D3D12_BARRIER_SYNC &r_sync, D3D12_BARRIER_ACCESS &r_access) {
	D3D12_BARRIER_SYNC sync_mask;
	r_sync = D3D12_BARRIER_SYNC_NONE;

	if (p_texture_layout == RDD::TEXTURE_LAYOUT_UNDEFINED) {
		// Undefined texture layouts are a special case where no access bits or synchronization scopes are allowed.
		r_access = D3D12_BARRIER_ACCESS_NO_ACCESS;
		return;
	}

	// Convert access bits to the D3D12 barrier access bits.
	_rd_access_to_d3d12_and_mask(p_access, p_texture_layout, r_access, sync_mask);

	if (p_texture_layout != RDD::TEXTURE_LAYOUT_MAX) {
		// Only allow the access bits compatible with the texture layout.
		r_access &= _rd_texture_layout_access_mask(p_texture_layout);
	}

	// Convert stage bits to the D3D12 synchronization scope bits.
	_rd_stages_to_d3d12(p_stages, r_sync);

	// Only enable synchronization stages compatible with the access bits that were used.
	r_sync &= sync_mask;

	if (r_sync == D3D12_BARRIER_SYNC_NONE) {
		if (p_access.is_empty()) {
			// No valid synchronization scope was defined and no access in particular is required.
			r_access = D3D12_BARRIER_ACCESS_NO_ACCESS;
		} else {
			// Access is required but the synchronization scope wasn't compatible. We fall back to the global synchronization scope and access.
			r_sync = D3D12_BARRIER_SYNC_ALL;
			r_access = D3D12_BARRIER_ACCESS_COMMON;
		}
	}
}

static D3D12_BARRIER_LAYOUT _rd_texture_layout_to_d3d12_barrier_layout(RDD::TextureLayout p_texture_layout) {
	switch (p_texture_layout) {
		case RDD::TEXTURE_LAYOUT_UNDEFINED:
			return D3D12_BARRIER_LAYOUT_UNDEFINED;
		case RDD::TEXTURE_LAYOUT_STORAGE_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		case RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		case RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		case RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		case RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		case RDD::TEXTURE_LAYOUT_COPY_SRC_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		case RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_COPY_DEST;
		case RDD::TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
		case RDD::TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
		case RDD::TEXTURE_LAYOUT_VRS_ATTACHMENT_OPTIMAL:
			return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		default:
			DEV_ASSERT(false && "Unknown texture layout.");
			return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
}

void RenderingDeviceDriverD3D12::command_pipeline_barrier(CommandBufferID p_cmd_buffer,
		BitField<PipelineStageBits> p_src_stages,
		BitField<PipelineStageBits> p_dst_stages,
		VectorView<RDD::MemoryBarrier> p_memory_barriers,
		VectorView<RDD::BufferBarrier> p_buffer_barriers,
		VectorView<RDD::TextureBarrier> p_texture_barriers) {
	if (!barrier_capabilities.enhanced_barriers_supported) {
		// Enhanced barriers are a requirement for this function.
		return;
	}

	if (p_memory_barriers.size() == 0 && p_buffer_barriers.size() == 0 && p_texture_barriers.size() == 0) {
		// At least one barrier must be present in the arguments.
		return;
	}

	// The command list must support the required interface.
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)(p_cmd_buffer.id);
	ID3D12GraphicsCommandList7 *cmd_list_7 = nullptr;
	HRESULT res = cmd_buf_info->cmd_list->QueryInterface(IID_PPV_ARGS(&cmd_list_7));
	ERR_FAIL_COND(FAILED(res));

	// Convert the RDD barriers to D3D12 enhanced barriers.
	thread_local LocalVector<D3D12_GLOBAL_BARRIER> global_barriers;
	thread_local LocalVector<D3D12_BUFFER_BARRIER> buffer_barriers;
	thread_local LocalVector<D3D12_TEXTURE_BARRIER> texture_barriers;
	global_barriers.clear();
	buffer_barriers.clear();
	texture_barriers.clear();

	D3D12_GLOBAL_BARRIER global_barrier = {};
	for (uint32_t i = 0; i < p_memory_barriers.size(); i++) {
		const MemoryBarrier &memory_barrier = p_memory_barriers[i];
		_rd_stages_and_access_to_d3d12(p_src_stages, RDD::TEXTURE_LAYOUT_MAX, memory_barrier.src_access, global_barrier.SyncBefore, global_barrier.AccessBefore);
		_rd_stages_and_access_to_d3d12(p_dst_stages, RDD::TEXTURE_LAYOUT_MAX, memory_barrier.dst_access, global_barrier.SyncAfter, global_barrier.AccessAfter);
		global_barriers.push_back(global_barrier);
	}

	D3D12_BUFFER_BARRIER buffer_barrier_d3d12 = {};
	buffer_barrier_d3d12.Offset = 0;
	buffer_barrier_d3d12.Size = UINT64_MAX; // The specification says this must be the size of the buffer barrier.
	for (uint32_t i = 0; i < p_buffer_barriers.size(); i++) {
		const BufferBarrier &buffer_barrier_rd = p_buffer_barriers[i];
		const BufferInfo *buffer_info = (const BufferInfo *)(buffer_barrier_rd.buffer.id);
		_rd_stages_and_access_to_d3d12(p_src_stages, RDD::TEXTURE_LAYOUT_MAX, buffer_barrier_rd.src_access, buffer_barrier_d3d12.SyncBefore, buffer_barrier_d3d12.AccessBefore);
		_rd_stages_and_access_to_d3d12(p_dst_stages, RDD::TEXTURE_LAYOUT_MAX, buffer_barrier_rd.dst_access, buffer_barrier_d3d12.SyncAfter, buffer_barrier_d3d12.AccessAfter);
		buffer_barrier_d3d12.pResource = buffer_info->resource;
		buffer_barriers.push_back(buffer_barrier_d3d12);
	}

	D3D12_TEXTURE_BARRIER texture_barrier_d3d12 = {};
	for (uint32_t i = 0; i < p_texture_barriers.size(); i++) {
		const TextureBarrier &texture_barrier_rd = p_texture_barriers[i];
		const TextureInfo *texture_info = (const TextureInfo *)(texture_barrier_rd.texture.id);
		if (texture_info->main_texture) {
			texture_info = texture_info->main_texture;
		}
		_rd_stages_and_access_to_d3d12(p_src_stages, texture_barrier_rd.prev_layout, texture_barrier_rd.src_access, texture_barrier_d3d12.SyncBefore, texture_barrier_d3d12.AccessBefore);
		_rd_stages_and_access_to_d3d12(p_dst_stages, texture_barrier_rd.next_layout, texture_barrier_rd.dst_access, texture_barrier_d3d12.SyncAfter, texture_barrier_d3d12.AccessAfter);
		texture_barrier_d3d12.LayoutBefore = _rd_texture_layout_to_d3d12_barrier_layout(texture_barrier_rd.prev_layout);
		texture_barrier_d3d12.LayoutAfter = _rd_texture_layout_to_d3d12_barrier_layout(texture_barrier_rd.next_layout);
		texture_barrier_d3d12.pResource = texture_info->resource;
		if (texture_barrier_rd.subresources.mipmap_count == texture_info->mipmaps && texture_barrier_rd.subresources.layer_count == texture_info->layers) {
			// So, all resources. Then, let's be explicit about it so D3D12 doesn't think
			// we are dealing with a subset of subresources.
			texture_barrier_d3d12.Subresources.IndexOrFirstMipLevel = 0xffffffff;
			texture_barrier_d3d12.Subresources.NumMipLevels = 0;
			// Because NumMipLevels == 0, all the other fields are ignored by D3D12.
		} else {
			texture_barrier_d3d12.Subresources.IndexOrFirstMipLevel = texture_barrier_rd.subresources.base_mipmap;
			texture_barrier_d3d12.Subresources.NumMipLevels = texture_barrier_rd.subresources.mipmap_count;
			texture_barrier_d3d12.Subresources.FirstArraySlice = texture_barrier_rd.subresources.base_layer;
			texture_barrier_d3d12.Subresources.NumArraySlices = texture_barrier_rd.subresources.layer_count;
			texture_barrier_d3d12.Subresources.FirstPlane = _compute_plane_slice(texture_info->format, texture_barrier_rd.subresources.aspect);
			texture_barrier_d3d12.Subresources.NumPlanes = format_get_plane_count(texture_info->format);
		}
		texture_barrier_d3d12.Flags = (texture_barrier_rd.prev_layout == RDD::TEXTURE_LAYOUT_UNDEFINED) ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE;
		texture_barriers.push_back(texture_barrier_d3d12);
	}

	// Define the barrier groups and execute.

	D3D12_BARRIER_GROUP barrier_groups[3] = {};
	uint32_t barrier_groups_count = 0;

	if (!global_barriers.is_empty()) {
		D3D12_BARRIER_GROUP &barrier_group = barrier_groups[barrier_groups_count++];
		barrier_group.Type = D3D12_BARRIER_TYPE_GLOBAL;
		barrier_group.NumBarriers = global_barriers.size();
		barrier_group.pGlobalBarriers = global_barriers.ptr();
	}

	if (!buffer_barriers.is_empty()) {
		D3D12_BARRIER_GROUP &barrier_group = barrier_groups[barrier_groups_count++];
		barrier_group.Type = D3D12_BARRIER_TYPE_BUFFER;
		barrier_group.NumBarriers = buffer_barriers.size();
		barrier_group.pBufferBarriers = buffer_barriers.ptr();
	}

	if (!texture_barriers.is_empty()) {
		D3D12_BARRIER_GROUP &barrier_group = barrier_groups[barrier_groups_count++];
		barrier_group.Type = D3D12_BARRIER_TYPE_TEXTURE;
		barrier_group.NumBarriers = texture_barriers.size();
		barrier_group.pTextureBarriers = texture_barriers.ptr();
	}

	cmd_list_7->Barrier(barrier_groups_count, barrier_groups);
}

/****************/
/**** FENCES ****/
/****************/

RDD::FenceID RenderingDeviceDriverD3D12::fence_create() {
	ComPtr<ID3D12Fence> d3d_fence;
	HRESULT res = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(d3d_fence.GetAddressOf()));
	ERR_FAIL_COND_V(!SUCCEEDED(res), FenceID());

	HANDLE event_handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	ERR_FAIL_NULL_V(event_handle, FenceID());

	FenceInfo *fence = memnew(FenceInfo);
	fence->d3d_fence = d3d_fence;
	fence->event_handle = event_handle;
	return FenceID(fence);
}

Error RenderingDeviceDriverD3D12::fence_wait(FenceID p_fence) {
	FenceInfo *fence = (FenceInfo *)(p_fence.id);
	DWORD res = WaitForSingleObjectEx(fence->event_handle, INFINITE, FALSE);
#ifdef PIX_ENABLED
	PIXNotifyWakeFromFenceSignal(fence->event_handle);
#endif

	return (res == WAIT_FAILED) ? FAILED : OK;
}

void RenderingDeviceDriverD3D12::fence_free(FenceID p_fence) {
	FenceInfo *fence = (FenceInfo *)(p_fence.id);
	CloseHandle(fence->event_handle);
	memdelete(fence);
}

/********************/
/**** SEMAPHORES ****/
/********************/

RDD::SemaphoreID RenderingDeviceDriverD3D12::semaphore_create() {
	ComPtr<ID3D12Fence> d3d_fence;
	HRESULT res = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(d3d_fence.GetAddressOf()));
	ERR_FAIL_COND_V(!SUCCEEDED(res), SemaphoreID());

	SemaphoreInfo *semaphore = memnew(SemaphoreInfo);
	semaphore->d3d_fence = d3d_fence;
	return SemaphoreID(semaphore);
}

void RenderingDeviceDriverD3D12::semaphore_free(SemaphoreID p_semaphore) {
	SemaphoreInfo *semaphore = (SemaphoreInfo *)(p_semaphore.id);
	memdelete(semaphore);
}

/******************/
/**** COMMANDS ****/
/******************/

// ----- QUEUE FAMILY -----

RDD::CommandQueueFamilyID RenderingDeviceDriverD3D12::command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface) {
	// Return the command list type encoded plus one so zero is an invalid value.
	// The only ones that support presenting to a surface are direct queues.
	if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_GRAPHICS_BIT) || (p_surface != 0)) {
		return CommandQueueFamilyID(D3D12_COMMAND_LIST_TYPE_DIRECT + 1);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_COMPUTE_BIT)) {
		return CommandQueueFamilyID(D3D12_COMMAND_LIST_TYPE_COMPUTE + 1);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_TRANSFER_BIT)) {
		return CommandQueueFamilyID(D3D12_COMMAND_LIST_TYPE_COPY + 1);
	} else {
		return CommandQueueFamilyID();
	}
}

// ----- QUEUE -----

RDD::CommandQueueID RenderingDeviceDriverD3D12::command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue) {
	ComPtr<ID3D12CommandQueue> d3d_queue;
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = (D3D12_COMMAND_LIST_TYPE)(p_cmd_queue_family.id - 1);
	HRESULT res = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(d3d_queue.GetAddressOf()));
	ERR_FAIL_COND_V(!SUCCEEDED(res), CommandQueueID());

	CommandQueueInfo *command_queue = memnew(CommandQueueInfo);
	command_queue->d3d_queue = d3d_queue;
	return CommandQueueID(command_queue);
}

Error RenderingDeviceDriverD3D12::command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) {
	CommandQueueInfo *command_queue = (CommandQueueInfo *)(p_cmd_queue.id);
	for (uint32_t i = 0; i < p_wait_semaphores.size(); i++) {
		const SemaphoreInfo *semaphore = (const SemaphoreInfo *)(p_wait_semaphores[i].id);
		command_queue->d3d_queue->Wait(semaphore->d3d_fence.Get(), semaphore->fence_value);
	}

	if (p_cmd_buffers.size() > 0) {
		thread_local LocalVector<ID3D12CommandList *> command_lists;
		command_lists.resize(p_cmd_buffers.size());
		for (uint32_t i = 0; i < p_cmd_buffers.size(); i++) {
			const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)(p_cmd_buffers[i].id);
			command_lists[i] = cmd_buf_info->cmd_list.Get();
		}

		command_queue->d3d_queue->ExecuteCommandLists(command_lists.size(), command_lists.ptr());

		for (uint32_t i = 0; i < p_cmd_semaphores.size(); i++) {
			SemaphoreInfo *semaphore = (SemaphoreInfo *)(p_cmd_semaphores[i].id);
			semaphore->fence_value++;
			command_queue->d3d_queue->Signal(semaphore->d3d_fence.Get(), semaphore->fence_value);
		}

		if (p_cmd_fence) {
			FenceInfo *fence = (FenceInfo *)(p_cmd_fence.id);
			fence->fence_value++;
			command_queue->d3d_queue->Signal(fence->d3d_fence.Get(), fence->fence_value);
			fence->d3d_fence->SetEventOnCompletion(fence->fence_value, fence->event_handle);
		}
	}

	HRESULT res;
	bool any_present_failed = false;
	for (uint32_t i = 0; i < p_swap_chains.size(); i++) {
		SwapChain *swap_chain = (SwapChain *)(p_swap_chains[i].id);
		res = swap_chain->d3d_swap_chain->Present(swap_chain->sync_interval, swap_chain->present_flags);
		if (!SUCCEEDED(res)) {
			print_verbose(vformat("D3D12: Presenting swapchain failed with error 0x%08ux.", (uint64_t)res));
			any_present_failed = true;
		}
	}

	return any_present_failed ? FAILED : OK;
}

void RenderingDeviceDriverD3D12::command_queue_free(CommandQueueID p_cmd_queue) {
	CommandQueueInfo *command_queue = (CommandQueueInfo *)(p_cmd_queue.id);
	memdelete(command_queue);
}

// ----- POOL -----

RDD::CommandPoolID RenderingDeviceDriverD3D12::command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) {
	CommandPoolInfo *command_pool = memnew(CommandPoolInfo);
	command_pool->queue_family = p_cmd_queue_family;
	command_pool->buffer_type = p_cmd_buffer_type;
	return CommandPoolID(command_pool);
}

void RenderingDeviceDriverD3D12::command_pool_free(CommandPoolID p_cmd_pool) {
	CommandPoolInfo *command_pool = (CommandPoolInfo *)(p_cmd_pool.id);
	memdelete(command_pool);
}

// ----- BUFFER -----

RDD::CommandBufferID RenderingDeviceDriverD3D12::command_buffer_create(CommandPoolID p_cmd_pool) {
	DEV_ASSERT(p_cmd_pool);

	const CommandPoolInfo *command_pool = (CommandPoolInfo *)(p_cmd_pool.id);
	D3D12_COMMAND_LIST_TYPE list_type;
	if (command_pool->buffer_type == COMMAND_BUFFER_TYPE_SECONDARY) {
		list_type = D3D12_COMMAND_LIST_TYPE_BUNDLE;
	} else {
		list_type = D3D12_COMMAND_LIST_TYPE(command_pool->queue_family.id - 1);
	}

	ID3D12CommandAllocator *cmd_allocator = nullptr;
	{
		HRESULT res = device->CreateCommandAllocator(list_type, IID_PPV_ARGS(&cmd_allocator));
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), CommandBufferID(), "CreateCommandAllocator failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	}

	ID3D12GraphicsCommandList *cmd_list = nullptr;
	{
		ComPtr<ID3D12Device4> device_4;
		device->QueryInterface(device_4.GetAddressOf());
		HRESULT res = E_FAIL;
		if (device_4) {
			res = device_4->CreateCommandList1(0, list_type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cmd_list));
		} else {
			res = device->CreateCommandList(0, list_type, cmd_allocator, nullptr, IID_PPV_ARGS(&cmd_list));
		}
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), CommandBufferID(), "CreateCommandList failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
		if (!device_4) {
			cmd_list->Close();
		}
	}

	// Bookkeep

	CommandBufferInfo *cmd_buf_info = VersatileResource::allocate<CommandBufferInfo>(resources_allocator);
	cmd_buf_info->cmd_allocator = cmd_allocator;
	cmd_buf_info->cmd_list = cmd_list;

	return CommandBufferID(cmd_buf_info);
}

bool RenderingDeviceDriverD3D12::command_buffer_begin(CommandBufferID p_cmd_buffer) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	HRESULT res = cmd_buf_info->cmd_allocator->Reset();
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "Reset failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	res = cmd_buf_info->cmd_list->Reset(cmd_buf_info->cmd_allocator.Get(), nullptr);
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "Reset failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	return true;
}

bool RenderingDeviceDriverD3D12::command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	HRESULT res = cmd_buf_info->cmd_allocator->Reset();
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "Reset failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	res = cmd_buf_info->cmd_list->Reset(cmd_buf_info->cmd_allocator.Get(), nullptr);
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), false, "Reset failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	return true;
}

void RenderingDeviceDriverD3D12::command_buffer_end(CommandBufferID p_cmd_buffer) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	HRESULT res = cmd_buf_info->cmd_list->Close();

	ERR_FAIL_COND_MSG(!SUCCEEDED(res), "Close failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	cmd_buf_info->graphics_pso = nullptr;
	cmd_buf_info->graphics_root_signature_crc = 0;
	cmd_buf_info->compute_pso = nullptr;
	cmd_buf_info->compute_root_signature_crc = 0;
	cmd_buf_info->descriptor_heaps_set = false;
}

void RenderingDeviceDriverD3D12::command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	for (uint32_t i = 0; i < p_secondary_cmd_buffers.size(); i++) {
		const CommandBufferInfo *secondary_cb_info = (const CommandBufferInfo *)p_secondary_cmd_buffers[i].id;
		cmd_buf_info->cmd_list->ExecuteBundle(secondary_cb_info->cmd_list.Get());
	}
}

/********************/
/**** SWAP CHAIN ****/
/********************/

void RenderingDeviceDriverD3D12::_swap_chain_release(SwapChain *p_swap_chain) {
	_swap_chain_release_buffers(p_swap_chain);

	p_swap_chain->d3d_swap_chain.Reset();
}

void RenderingDeviceDriverD3D12::_swap_chain_release_buffers(SwapChain *p_swap_chain) {
	for (ID3D12Resource *render_target : p_swap_chain->render_targets) {
		render_target->Release();
	}

	p_swap_chain->render_targets.clear();
	p_swap_chain->render_targets_info.clear();

	for (RDD::FramebufferID framebuffer : p_swap_chain->framebuffers) {
		framebuffer_free(framebuffer);
	}

	p_swap_chain->framebuffers.clear();
}

RDD::SwapChainID RenderingDeviceDriverD3D12::swap_chain_create(RenderingContextDriver::SurfaceID p_surface) {
	// Create the render pass that will be used to draw to the swap chain's framebuffers.
	RDD::Attachment attachment;
	attachment.format = DATA_FORMAT_R8G8B8A8_UNORM;
	attachment.samples = RDD::TEXTURE_SAMPLES_1;
	attachment.load_op = RDD::ATTACHMENT_LOAD_OP_CLEAR;
	attachment.store_op = RDD::ATTACHMENT_STORE_OP_STORE;

	RDD::Subpass subpass;
	RDD::AttachmentReference color_ref;
	color_ref.attachment = 0;
	color_ref.aspect.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
	subpass.color_references.push_back(color_ref);

	RenderPassID render_pass = render_pass_create(attachment, subpass, {}, 1);
	ERR_FAIL_COND_V(!render_pass, SwapChainID());

	// Create the empty swap chain until it is resized.
	SwapChain *swap_chain = memnew(SwapChain);
	swap_chain->surface = p_surface;
	swap_chain->data_format = attachment.format;
	swap_chain->render_pass = render_pass;
	return SwapChainID(swap_chain);
}

Error RenderingDeviceDriverD3D12::swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) {
	DEV_ASSERT(p_cmd_queue.id != 0);
	DEV_ASSERT(p_swap_chain.id != 0);

	CommandQueueInfo *command_queue = (CommandQueueInfo *)(p_cmd_queue.id);
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	RenderingContextDriverD3D12::Surface *surface = (RenderingContextDriverD3D12::Surface *)(swap_chain->surface);
	if (surface->width == 0 || surface->height == 0) {
		// Very likely the window is minimized, don't create a swap chain.
		return ERR_SKIP;
	}

	HRESULT res;
	const bool is_tearing_supported = context_driver->get_tearing_supported();
	UINT sync_interval = 0;
	UINT present_flags = 0;
	UINT creation_flags = 0;
	switch (surface->vsync_mode) {
		case DisplayServer::VSYNC_MAILBOX: {
			sync_interval = 1;
			present_flags = DXGI_PRESENT_RESTART;
		} break;
		case DisplayServer::VSYNC_ENABLED: {
			sync_interval = 1;
			present_flags = 0;
		} break;
		case DisplayServer::VSYNC_DISABLED: {
			sync_interval = 0;
			present_flags = is_tearing_supported ? DXGI_PRESENT_ALLOW_TEARING : 0;
			creation_flags = is_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		} break;
		case DisplayServer::VSYNC_ADAPTIVE: // Unsupported.
		default:
			sync_interval = 1;
			present_flags = 0;
			break;
	}

	print_verbose("Using swap chain flags: " + itos(creation_flags) + ", sync interval: " + itos(sync_interval) + ", present flags: " + itos(present_flags));

	if (swap_chain->d3d_swap_chain != nullptr && creation_flags != swap_chain->creation_flags) {
		// The swap chain must be recreated if the creation flags are different.
		_swap_chain_release(swap_chain);
	}

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	if (swap_chain->d3d_swap_chain != nullptr) {
		_swap_chain_release_buffers(swap_chain);
		res = swap_chain->d3d_swap_chain->ResizeBuffers(p_desired_framebuffer_count, 0, 0, DXGI_FORMAT_UNKNOWN, creation_flags);
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_UNAVAILABLE);
	} else {
		swap_chain_desc.BufferCount = p_desired_framebuffer_count;
		swap_chain_desc.Format = RD_TO_D3D12_FORMAT[swap_chain->data_format].general_format;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.SampleDesc.Count = 1;
		swap_chain_desc.Flags = creation_flags;
		swap_chain_desc.Scaling = DXGI_SCALING_NONE;
		if (OS::get_singleton()->is_layered_allowed()) {
			swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
			has_comp_alpha[(uint64_t)p_cmd_queue.id] = true;
		} else {
			swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			has_comp_alpha[(uint64_t)p_cmd_queue.id] = false;
		}

		ComPtr<IDXGISwapChain1> swap_chain_1;
		res = context_driver->dxgi_factory_get()->CreateSwapChainForHwnd(command_queue->d3d_queue.Get(), surface->hwnd, &swap_chain_desc, nullptr, nullptr, swap_chain_1.GetAddressOf());
		if (!SUCCEEDED(res) && swap_chain_desc.AlphaMode != DXGI_ALPHA_MODE_IGNORE) {
			swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			has_comp_alpha[(uint64_t)p_cmd_queue.id] = false;
			res = context_driver->dxgi_factory_get()->CreateSwapChainForHwnd(command_queue->d3d_queue.Get(), surface->hwnd, &swap_chain_desc, nullptr, nullptr, swap_chain_1.GetAddressOf());
		}
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);

		swap_chain_1.As(&swap_chain->d3d_swap_chain);
		ERR_FAIL_NULL_V(swap_chain->d3d_swap_chain, ERR_CANT_CREATE);

		res = context_driver->dxgi_factory_get()->MakeWindowAssociation(surface->hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
	}

	res = swap_chain->d3d_swap_chain->GetDesc1(&swap_chain_desc);
	ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
	ERR_FAIL_COND_V(swap_chain_desc.BufferCount == 0, ERR_CANT_CREATE);

	surface->width = swap_chain_desc.Width;
	surface->height = swap_chain_desc.Height;

	swap_chain->creation_flags = creation_flags;
	swap_chain->sync_interval = sync_interval;
	swap_chain->present_flags = present_flags;

	// Retrieve the render targets associated to the swap chain and recreate the framebuffers. The following code
	// relies on the address of the elements remaining static when new elements are inserted, so the container must
	// follow this restriction when reserving the right amount of elements beforehand.
	swap_chain->render_targets.reserve(swap_chain_desc.BufferCount);
	swap_chain->render_targets_info.reserve(swap_chain_desc.BufferCount);
	swap_chain->framebuffers.reserve(swap_chain_desc.BufferCount);

	for (uint32_t i = 0; i < swap_chain_desc.BufferCount; i++) {
		// Retrieve the resource corresponding to the swap chain's buffer.
		ID3D12Resource *render_target = nullptr;
		res = swap_chain->d3d_swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_target));
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
		swap_chain->render_targets.push_back(render_target);

		// Create texture information for the framebuffer to reference the resource. Since the states pointer must
		// reference an address of the element itself, we must insert it first and then modify it.
		swap_chain->render_targets_info.push_back(TextureInfo());
		TextureInfo &texture_info = swap_chain->render_targets_info[i];
		texture_info.owner_info.states.subresource_states.push_back(D3D12_RESOURCE_STATE_PRESENT);
		texture_info.states_ptr = &texture_info.owner_info.states;
		texture_info.format = swap_chain->data_format;
#if defined(_MSC_VER) || !defined(_WIN32)
		texture_info.desc = CD3DX12_RESOURCE_DESC(render_target->GetDesc());
#else
		render_target->GetDesc(&texture_info.desc);
#endif
		texture_info.layers = 1;
		texture_info.mipmaps = 1;
		texture_info.resource = render_target;
		texture_info.view_descs.srv.Format = texture_info.desc.Format;
		texture_info.view_descs.srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		// Create the framebuffer for this buffer.
		FramebufferID framebuffer = _framebuffer_create(swap_chain->render_pass, TextureID(&swap_chain->render_targets_info[i]), swap_chain_desc.Width, swap_chain_desc.Height, true);
		ERR_FAIL_COND_V(!framebuffer, ERR_CANT_CREATE);
		swap_chain->framebuffers.push_back(framebuffer);
	}

	// Once everything's been created correctly, indicate the surface no longer needs to be resized.
	context_driver->surface_set_needs_resize(swap_chain->surface, false);

	return OK;
}

RDD::FramebufferID RenderingDeviceDriverD3D12::swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) {
	DEV_ASSERT(p_swap_chain.id != 0);

	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	if (context_driver->surface_get_needs_resize(swap_chain->surface)) {
		r_resize_required = true;
		return FramebufferID();
	}

	const uint32_t buffer_index = swap_chain->d3d_swap_chain->GetCurrentBackBufferIndex();
	DEV_ASSERT(buffer_index < swap_chain->framebuffers.size());
	return swap_chain->framebuffers[buffer_index];
}

RDD::RenderPassID RenderingDeviceDriverD3D12::swap_chain_get_render_pass(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->render_pass;
}

RDD::DataFormat RenderingDeviceDriverD3D12::swap_chain_get_format(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->data_format;
}

void RenderingDeviceDriverD3D12::swap_chain_free(SwapChainID p_swap_chain) {
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	_swap_chain_release(swap_chain);
	render_pass_free(swap_chain->render_pass);
	memdelete(swap_chain);
}

/*********************/
/**** FRAMEBUFFER ****/
/*********************/

D3D12_RENDER_TARGET_VIEW_DESC RenderingDeviceDriverD3D12::_make_rtv_for_texture(const TextureInfo *p_texture_info, uint32_t p_mipmap_offset, uint32_t p_layer_offset, uint32_t p_layers, bool p_add_bases) {
	D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = p_texture_info->view_descs.srv.Format;

	switch (p_texture_info->view_descs.srv.ViewDimension) {
		case D3D12_SRV_DIMENSION_TEXTURE1D: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			rtv_desc.Texture1D.MipSlice = p_texture_info->base_mip + p_mipmap_offset;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			rtv_desc.Texture1DArray.MipSlice = (p_add_bases ? p_texture_info->base_mip : 0) + p_mipmap_offset;
			rtv_desc.Texture1DArray.FirstArraySlice = (p_add_bases ? p_texture_info->base_layer : 0) + p_layer_offset;
			rtv_desc.Texture1DArray.ArraySize = p_layers == UINT32_MAX ? p_texture_info->view_descs.srv.Texture1DArray.ArraySize : p_layers;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2D: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv_desc.Texture2D.MipSlice = (p_add_bases ? p_texture_info->base_mip : 0) + p_mipmap_offset;
			rtv_desc.Texture2D.PlaneSlice = p_texture_info->view_descs.srv.Texture2D.PlaneSlice;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv_desc.Texture2DArray.MipSlice = (p_add_bases ? p_texture_info->base_mip : 0) + p_mipmap_offset;
			rtv_desc.Texture2DArray.FirstArraySlice = (p_add_bases ? p_texture_info->base_layer : 0) + p_layer_offset;
			rtv_desc.Texture2DArray.ArraySize = p_layers == UINT32_MAX ? p_texture_info->view_descs.srv.Texture2DArray.ArraySize : p_layers;
			rtv_desc.Texture2DArray.PlaneSlice = p_texture_info->view_descs.srv.Texture2DArray.PlaneSlice;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			rtv_desc.Texture2DMSArray.FirstArraySlice = (p_add_bases ? p_texture_info->base_layer : 0) + p_layer_offset;
			rtv_desc.Texture2DMSArray.ArraySize = p_layers == UINT32_MAX ? p_texture_info->view_descs.srv.Texture2DMSArray.ArraySize : p_layers;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE3D: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			rtv_desc.Texture3D.MipSlice = p_texture_info->view_descs.srv.Texture3D.MostDetailedMip + p_mipmap_offset;
			rtv_desc.Texture3D.FirstWSlice = 0;
			rtv_desc.Texture3D.WSize = -1;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv_desc.Texture2DArray.MipSlice = (p_add_bases ? p_texture_info->base_mip : 0) + p_mipmap_offset;
			rtv_desc.Texture2DArray.FirstArraySlice = (p_add_bases ? p_texture_info->base_layer : 0) + p_layer_offset;
			rtv_desc.Texture2DArray.ArraySize = p_layers == UINT32_MAX ? p_texture_info->layers : p_layers;
			rtv_desc.Texture2DArray.PlaneSlice = 0;
		} break;
		default: {
			DEV_ASSERT(false);
		}
	}

	return rtv_desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC RenderingDeviceDriverD3D12::_make_ranged_uav_for_texture(const TextureInfo *p_texture_info, uint32_t p_mipmap_offset, uint32_t p_layer_offset, uint32_t p_layers, bool p_add_bases) {
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = p_texture_info->view_descs.uav;

	uint32_t mip = (p_add_bases ? p_texture_info->base_mip : 0) + p_mipmap_offset;
	switch (p_texture_info->view_descs.uav.ViewDimension) {
		case D3D12_UAV_DIMENSION_TEXTURE1D: {
			uav_desc.Texture1DArray.MipSlice = mip;
		} break;
		case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
			uav_desc.Texture1DArray.MipSlice = mip;
			uav_desc.Texture1DArray.FirstArraySlice = mip;
			uav_desc.Texture1DArray.ArraySize = p_layers;
		} break;
		case D3D12_UAV_DIMENSION_TEXTURE2D: {
			uav_desc.Texture2D.MipSlice = mip;
		} break;
		case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
			uav_desc.Texture2DArray.MipSlice = mip;
			uav_desc.Texture2DArray.FirstArraySlice = (p_add_bases ? p_texture_info->base_layer : 0) + p_layer_offset;
			uav_desc.Texture2DArray.ArraySize = p_layers;
		} break;
		case D3D12_UAV_DIMENSION_TEXTURE3D: {
			uav_desc.Texture3D.MipSlice = mip;
			uav_desc.Texture3D.WSize >>= p_mipmap_offset;
		} break;
		default:
			break;
	}

	return uav_desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC RenderingDeviceDriverD3D12::_make_dsv_for_texture(const TextureInfo *p_texture_info) {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
	dsv_desc.Format = RD_TO_D3D12_FORMAT[p_texture_info->format].dsv_format;
	dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

	switch (p_texture_info->view_descs.srv.ViewDimension) {
		case D3D12_SRV_DIMENSION_TEXTURE1D: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			dsv_desc.Texture1D.MipSlice = p_texture_info->base_mip;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			dsv_desc.Texture1DArray.MipSlice = p_texture_info->base_mip;
			dsv_desc.Texture1DArray.FirstArraySlice = p_texture_info->base_layer;
			dsv_desc.Texture1DArray.ArraySize = p_texture_info->view_descs.srv.Texture1DArray.ArraySize;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2D: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv_desc.Texture2D.MipSlice = p_texture_info->view_descs.srv.Texture2D.MostDetailedMip;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv_desc.Texture2DArray.MipSlice = p_texture_info->base_mip;
			dsv_desc.Texture2DArray.FirstArraySlice = p_texture_info->base_layer;
			dsv_desc.Texture2DArray.ArraySize = p_texture_info->view_descs.srv.Texture2DArray.ArraySize;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			dsv_desc.Texture2DMS.UnusedField_NothingToDefine = p_texture_info->view_descs.srv.Texture2DMS.UnusedField_NothingToDefine;
		} break;
		case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
			dsv_desc.Texture2DMSArray.FirstArraySlice = p_texture_info->base_layer;
			dsv_desc.Texture2DMSArray.ArraySize = p_texture_info->view_descs.srv.Texture2DMSArray.ArraySize;
		} break;
		default: {
			DEV_ASSERT(false);
		}
	}

	return dsv_desc;
}

RDD::FramebufferID RenderingDeviceDriverD3D12::_framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height, bool p_is_screen) {
	// Pre-bookkeep.
	FramebufferInfo *fb_info = VersatileResource::allocate<FramebufferInfo>(resources_allocator);
	fb_info->is_screen = p_is_screen;

	const RenderPassInfo *pass_info = (const RenderPassInfo *)p_render_pass.id;

	uint32_t num_color = 0;
	uint32_t num_depth_stencil = 0;
	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		const TextureInfo *tex_info = (const TextureInfo *)p_attachments[i].id;
		if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
			num_color++;
		} else if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
			num_depth_stencil++;
		}
	}

	uint32_t vrs_index = UINT32_MAX;
	for (const Subpass &E : pass_info->subpasses) {
		if (E.vrs_reference.attachment != AttachmentReference::UNUSED) {
			vrs_index = E.vrs_reference.attachment;
		}
	}

	if (num_color) {
		Error err = fb_info->rtv_heap.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, num_color, false);
		if (err) {
			VersatileResource::free(resources_allocator, fb_info);
			ERR_FAIL_V(FramebufferID());
		}
	}
	DescriptorsHeap::Walker rtv_heap_walker = fb_info->rtv_heap.make_walker();

	if (num_depth_stencil) {
		Error err = fb_info->dsv_heap.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, num_depth_stencil, false);
		if (err) {
			VersatileResource::free(resources_allocator, fb_info);
			ERR_FAIL_V(FramebufferID());
		}
	}
	DescriptorsHeap::Walker dsv_heap_walker = fb_info->dsv_heap.make_walker();

	fb_info->attachments_handle_inds.resize(p_attachments.size());
	fb_info->attachments.reserve(num_color + num_depth_stencil);

	uint32_t color_idx = 0;
	uint32_t depth_stencil_idx = 0;
	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		const TextureInfo *tex_info = (const TextureInfo *)p_attachments[i].id;

		if (fb_info->size.x == 0) {
			fb_info->size = Size2i(tex_info->desc.Width, tex_info->desc.Height);
		}

		if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = _make_rtv_for_texture(tex_info, 0, 0, UINT32_MAX);
			device->CreateRenderTargetView(tex_info->resource, &rtv_desc, rtv_heap_walker.get_curr_cpu_handle());
			rtv_heap_walker.advance();

			fb_info->attachments_handle_inds[i] = color_idx;
			fb_info->attachments.push_back(p_attachments[i]);
			color_idx++;
		} else if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = _make_dsv_for_texture(tex_info);
			device->CreateDepthStencilView(tex_info->resource, &dsv_desc, dsv_heap_walker.get_curr_cpu_handle());
			dsv_heap_walker.advance();

			fb_info->attachments_handle_inds[i] = depth_stencil_idx;
			fb_info->attachments.push_back(p_attachments[i]);
			depth_stencil_idx++;
		} else if (i == vrs_index) {
			fb_info->vrs_attachment = p_attachments[i];
		} else {
			DEV_ASSERT(false);
		}
	}

	DEV_ASSERT(fb_info->attachments.size() == color_idx + depth_stencil_idx);
	DEV_ASSERT((fb_info->vrs_attachment.id != 0) == (vrs_index != UINT32_MAX));

	DEV_ASSERT(rtv_heap_walker.is_at_eof());
	DEV_ASSERT(dsv_heap_walker.is_at_eof());

	return FramebufferID(fb_info);
}

RDD::FramebufferID RenderingDeviceDriverD3D12::framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) {
	return _framebuffer_create(p_render_pass, p_attachments, p_width, p_height, false);
}

void RenderingDeviceDriverD3D12::framebuffer_free(FramebufferID p_framebuffer) {
	FramebufferInfo *fb_info = (FramebufferInfo *)p_framebuffer.id;
	VersatileResource::free(resources_allocator, fb_info);
}

/****************/
/**** SHADER ****/
/****************/

static uint32_t SHADER_STAGES_BIT_OFFSET_INDICES[RenderingDevice::SHADER_STAGE_MAX] = {
	/* SHADER_STAGE_VERTEX */ 0,
	/* SHADER_STAGE_FRAGMENT */ 1,
	/* SHADER_STAGE_TESSELATION_CONTROL */ UINT32_MAX,
	/* SHADER_STAGE_TESSELATION_EVALUATION */ UINT32_MAX,
	/* SHADER_STAGE_COMPUTE */ 2,
};

uint32_t RenderingDeviceDriverD3D12::_shader_patch_dxil_specialization_constant(
		PipelineSpecializationConstantType p_type,
		const void *p_value,
		const uint64_t (&p_stages_bit_offsets)[D3D12_BITCODE_OFFSETS_NUM_STAGES],
		HashMap<ShaderStage, Vector<uint8_t>> &r_stages_bytecodes,
		bool p_is_first_patch) {
	uint32_t patch_val = 0;
	switch (p_type) {
		case PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT: {
			uint32_t int_value = *((const int *)p_value);
			ERR_FAIL_COND_V(int_value & (1 << 31), 0);
			patch_val = int_value;
		} break;
		case PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL: {
			bool bool_value = *((const bool *)p_value);
			patch_val = (uint32_t)bool_value;
		} break;
		case PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT: {
			uint32_t int_value = *((const int *)p_value);
			ERR_FAIL_COND_V(int_value & (1 << 31), 0);
			patch_val = (int_value >> 1);
		} break;
	}
	// For VBR encoding to encode the number of bits we expect (32), we need to set the MSB unconditionally.
	// However, signed VBR moves the MSB to the LSB, so setting the MSB to 1 wouldn't help. Therefore,
	// the bit we set to 1 is the one at index 30.
	patch_val |= (1 << 30);
	patch_val <<= 1; // What signed VBR does.

	auto tamper_bits = [](uint8_t *p_start, uint64_t p_bit_offset, uint64_t p_tb_value) -> uint64_t {
		uint64_t original = 0;
		uint32_t curr_input_byte = p_bit_offset / 8;
		uint8_t curr_input_bit = p_bit_offset % 8;
		auto get_curr_input_bit = [&]() -> bool {
			return ((p_start[curr_input_byte] >> curr_input_bit) & 1);
		};
		auto move_to_next_input_bit = [&]() {
			if (curr_input_bit == 7) {
				curr_input_bit = 0;
				curr_input_byte++;
			} else {
				curr_input_bit++;
			}
		};
		auto tamper_input_bit = [&](bool p_new_bit) {
			p_start[curr_input_byte] &= ~((uint8_t)1 << curr_input_bit);
			if (p_new_bit) {
				p_start[curr_input_byte] |= (uint8_t)1 << curr_input_bit;
			}
		};
		uint8_t value_bit_idx = 0;
		for (uint32_t i = 0; i < 5; i++) { // 32 bits take 5 full bytes in VBR.
			for (uint32_t j = 0; j < 7; j++) {
				bool input_bit = get_curr_input_bit();
				original |= (uint64_t)(input_bit ? 1 : 0) << value_bit_idx;
				tamper_input_bit((p_tb_value >> value_bit_idx) & 1);
				move_to_next_input_bit();
				value_bit_idx++;
			}
#ifdef DEV_ENABLED
			bool input_bit = get_curr_input_bit();
			DEV_ASSERT((i < 4 && input_bit) || (i == 4 && !input_bit));
#endif
			move_to_next_input_bit();
		}
		return original;
	};
	uint32_t stages_patched_mask = 0;
	for (int stage = 0; stage < SHADER_STAGE_MAX; stage++) {
		if (!r_stages_bytecodes.has((ShaderStage)stage)) {
			continue;
		}

		uint64_t offset = p_stages_bit_offsets[SHADER_STAGES_BIT_OFFSET_INDICES[stage]];
		if (offset == 0) {
			// This constant does not appear at this stage.
			continue;
		}

		Vector<uint8_t> &bytecode = r_stages_bytecodes[(ShaderStage)stage];
#ifdef DEV_ENABLED
		uint64_t orig_patch_val = tamper_bits(bytecode.ptrw(), offset, patch_val);
		// Checking against the value the NIR patch should have set.
		DEV_ASSERT(!p_is_first_patch || ((orig_patch_val >> 1) & GODOT_NIR_SC_SENTINEL_MAGIC_MASK) == GODOT_NIR_SC_SENTINEL_MAGIC);
		uint64_t readback_patch_val = tamper_bits(bytecode.ptrw(), offset, patch_val);
		DEV_ASSERT(readback_patch_val == patch_val);
#else
		tamper_bits(bytecode.ptrw(), offset, patch_val);
#endif

		stages_patched_mask |= (1 << stage);
	}
	return stages_patched_mask;
}

bool RenderingDeviceDriverD3D12::_shader_apply_specialization_constants(
		const ShaderInfo *p_shader_info,
		VectorView<PipelineSpecializationConstant> p_specialization_constants,
		HashMap<ShaderStage, Vector<uint8_t>> &r_final_stages_bytecode) {
	// If something needs to be patched, COW will do the trick.
	r_final_stages_bytecode = p_shader_info->stages_bytecode;
	uint32_t stages_re_sign_mask = 0;
	for (uint32_t i = 0; i < p_specialization_constants.size(); i++) {
		const PipelineSpecializationConstant &psc = p_specialization_constants[i];
		if (!(p_shader_info->spirv_specialization_constants_ids_mask & (1 << psc.constant_id))) {
			// This SC wasn't even in the original SPIR-V shader.
			continue;
		}
		for (const ShaderInfo::SpecializationConstant &sc : p_shader_info->specialization_constants) {
			if (psc.constant_id == sc.constant_id) {
				if (psc.int_value != sc.int_value) {
					stages_re_sign_mask |= _shader_patch_dxil_specialization_constant(psc.type, &psc.int_value, sc.stages_bit_offsets, r_final_stages_bytecode, false);
				}
				break;
			}
		}
	}
	// Re-sign patched stages.
	for (KeyValue<ShaderStage, Vector<uint8_t>> &E : r_final_stages_bytecode) {
		ShaderStage stage = E.key;
		if ((stages_re_sign_mask & (1 << stage))) {
			Vector<uint8_t> &bytecode = E.value;
			_shader_sign_dxil_bytecode(stage, bytecode);
		}
	}

	return true;
}

void RenderingDeviceDriverD3D12::_shader_sign_dxil_bytecode(ShaderStage p_stage, Vector<uint8_t> &r_dxil_blob) {
	uint8_t *w = r_dxil_blob.ptrw();
	compute_dxil_hash(w + 20, r_dxil_blob.size() - 20, w + 4);
}

String RenderingDeviceDriverD3D12::shader_get_binary_cache_key() {
	return "D3D12-SV" + uitos(ShaderBinary::VERSION) + "-" + itos(shader_capabilities.shader_model);
}

Vector<uint8_t> RenderingDeviceDriverD3D12::shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) {
	ShaderReflection shader_refl;
	if (_reflect_spirv(p_spirv, shader_refl) != OK) {
		return Vector<uint8_t>();
	}

	// Collect reflection data into binary data.
	ShaderBinary::Data binary_data;
	Vector<Vector<ShaderBinary::DataBinding>> sets_bindings;
	Vector<ShaderBinary::SpecializationConstant> specialization_constants;
	{
		binary_data.vertex_input_mask = shader_refl.vertex_input_mask;
		binary_data.fragment_output_mask = shader_refl.fragment_output_mask;
		binary_data.specialization_constants_count = shader_refl.specialization_constants.size();
		binary_data.is_compute = shader_refl.is_compute;
		binary_data.compute_local_size[0] = shader_refl.compute_local_size[0];
		binary_data.compute_local_size[1] = shader_refl.compute_local_size[1];
		binary_data.compute_local_size[2] = shader_refl.compute_local_size[2];
		binary_data.set_count = shader_refl.uniform_sets.size();
		binary_data.push_constant_size = shader_refl.push_constant_size;
		binary_data.nir_runtime_data_root_param_idx = UINT32_MAX;
		binary_data.stage_count = p_spirv.size();

		for (const Vector<ShaderUniform> &spirv_set : shader_refl.uniform_sets) {
			Vector<ShaderBinary::DataBinding> bindings;
			for (const ShaderUniform &spirv_uniform : spirv_set) {
				ShaderBinary::DataBinding binding;
				binding.type = (uint32_t)spirv_uniform.type;
				binding.binding = spirv_uniform.binding;
				binding.stages = (uint32_t)spirv_uniform.stages;
				binding.length = spirv_uniform.length;
				binding.writable = (uint32_t)spirv_uniform.writable;
				bindings.push_back(binding);
			}
			sets_bindings.push_back(bindings);
		}

		for (const ShaderSpecializationConstant &spirv_sc : shader_refl.specialization_constants) {
			ShaderBinary::SpecializationConstant spec_constant;
			spec_constant.type = (uint32_t)spirv_sc.type;
			spec_constant.constant_id = spirv_sc.constant_id;
			spec_constant.int_value = spirv_sc.int_value;
			spec_constant.stage_flags = spirv_sc.stages;
			specialization_constants.push_back(spec_constant);

			binary_data.spirv_specialization_constants_ids_mask |= (1 << spirv_sc.constant_id);
		}
	}

	// Translate SPIR-V shaders to DXIL, and collect shader info from the new representation.
	HashMap<ShaderStage, Vector<uint8_t>> dxil_blobs;
	BitField<ShaderStage> stages_processed;
	{
		HashMap<int, nir_shader *> stages_nir_shaders;

		auto free_nir_shaders = [&]() {
			for (KeyValue<int, nir_shader *> &E : stages_nir_shaders) {
				ralloc_free(E.value);
			}
			stages_nir_shaders.clear();
		};

		// This is based on spirv2dxil.c. May need updates when it changes.
		// Also, this has to stay around until after linking.
		nir_shader_compiler_options nir_options = *dxil_get_nir_compiler_options();
		nir_options.lower_base_vertex = false;

		dxil_spirv_runtime_conf dxil_runtime_conf = {};
		dxil_runtime_conf.runtime_data_cbv.base_shader_register = RUNTIME_DATA_REGISTER;
		dxil_runtime_conf.push_constant_cbv.base_shader_register = ROOT_CONSTANT_REGISTER;
		dxil_runtime_conf.zero_based_vertex_instance_id = true;
		dxil_runtime_conf.zero_based_compute_workgroup_id = true;
		dxil_runtime_conf.declared_read_only_images_as_srvs = true;
		// Making this explicit to let maintainers know that in practice this didn't improve performance,
		// probably because data generated by one shader and consumed by another one forces the resource
		// to transition from UAV to SRV, and back, instead of being an UAV all the time.
		// In case someone wants to try, care must be taken so in case of incompatible bindings across stages
		// happen as a result, all the stages are re-translated. That can happen if, for instance, a stage only
		// uses an allegedly writable resource only for reading but the next stage doesn't.
		dxil_runtime_conf.inferred_read_only_images_as_srvs = false;

		// - Translate SPIR-V to NIR.
		for (uint32_t i = 0; i < p_spirv.size(); i++) {
			ShaderStage stage = (ShaderStage)p_spirv[i].shader_stage;
			ShaderStage stage_flag = (ShaderStage)(1 << p_spirv[i].shader_stage);

			stages_processed.set_flag(stage_flag);

			{
				const char *entry_point = "main";

				static const gl_shader_stage SPIRV_TO_MESA_STAGES[SHADER_STAGE_MAX] = {
					/* SHADER_STAGE_VERTEX */ MESA_SHADER_VERTEX,
					/* SHADER_STAGE_FRAGMENT */ MESA_SHADER_FRAGMENT,
					/* SHADER_STAGE_TESSELATION_CONTROL */ MESA_SHADER_TESS_CTRL,
					/* SHADER_STAGE_TESSELATION_EVALUATION */ MESA_SHADER_TESS_EVAL,
					/* SHADER_STAGE_COMPUTE */ MESA_SHADER_COMPUTE,
				};

				nir_shader *shader = spirv_to_nir(
						(const uint32_t *)p_spirv[i].spirv.ptr(),
						p_spirv[i].spirv.size() / sizeof(uint32_t),
						nullptr,
						0,
						SPIRV_TO_MESA_STAGES[stage],
						entry_point,
						dxil_spirv_nir_get_spirv_options(), &nir_options);
				if (!shader) {
					free_nir_shaders();
					ERR_FAIL_V_MSG(Vector<uint8_t>(), "Shader translation (step 1) at stage " + String(SHADER_STAGE_NAMES[stage]) + " failed.");
				}

#ifdef DEV_ENABLED
				nir_validate_shader(shader, "Validate before feeding NIR to the DXIL compiler");
#endif

				if (stage == SHADER_STAGE_VERTEX) {
					dxil_runtime_conf.yz_flip.y_mask = 0xffff;
					dxil_runtime_conf.yz_flip.mode = DXIL_SPIRV_Y_FLIP_UNCONDITIONAL;
				} else {
					dxil_runtime_conf.yz_flip.y_mask = 0;
					dxil_runtime_conf.yz_flip.mode = DXIL_SPIRV_YZ_FLIP_NONE;
				}

				// This is based on spirv2dxil.c. May need updates when it changes.
				dxil_spirv_nir_prep(shader);
				bool requires_runtime_data = {};
				dxil_spirv_nir_passes(shader, &dxil_runtime_conf, &requires_runtime_data);

				stages_nir_shaders[stage] = shader;
			}
		}

		// - Link NIR shaders.
		for (int i = SHADER_STAGE_MAX - 1; i >= 0; i--) {
			if (!stages_nir_shaders.has(i)) {
				continue;
			}
			nir_shader *shader = stages_nir_shaders[i];
			nir_shader *prev_shader = nullptr;
			for (int j = i - 1; j >= 0; j--) {
				if (stages_nir_shaders.has(j)) {
					prev_shader = stages_nir_shaders[j];
					break;
				}
			}
			if (prev_shader) {
				bool requires_runtime_data = {};
				dxil_spirv_nir_link(shader, prev_shader, &dxil_runtime_conf, &requires_runtime_data);
			}
		}

		// - Translate NIR to DXIL.
		for (uint32_t i = 0; i < p_spirv.size(); i++) {
			ShaderStage stage = (ShaderStage)p_spirv[i].shader_stage;

			struct ShaderData {
				ShaderStage stage;
				ShaderBinary::Data &binary_data;
				Vector<Vector<ShaderBinary::DataBinding>> &sets_bindings;
				Vector<ShaderBinary::SpecializationConstant> &specialization_constants;
			} shader_data{ stage, binary_data, sets_bindings, specialization_constants };

			GodotNirCallbacks godot_nir_callbacks = {};
			godot_nir_callbacks.data = &shader_data;

			godot_nir_callbacks.report_resource = [](uint32_t p_register, uint32_t p_space, uint32_t p_dxil_type, void *p_data) {
				ShaderData &shader_data_in = *(ShaderData *)p_data;

				// Types based on Mesa's dxil_container.h.
				static const uint32_t DXIL_RES_SAMPLER = 1;
				static const ResourceClass DXIL_TYPE_TO_CLASS[] = {
					/* DXIL_RES_INVALID */ RES_CLASS_INVALID,
					/* DXIL_RES_SAMPLER */ RES_CLASS_INVALID, // Handling sampler as a flag.
					/* DXIL_RES_CBV */ RES_CLASS_CBV,
					/* DXIL_RES_SRV_TYPED */ RES_CLASS_SRV,
					/* DXIL_RES_SRV_RAW */ RES_CLASS_SRV,
					/* DXIL_RES_SRV_STRUCTURED */ RES_CLASS_SRV,
					/* DXIL_RES_UAV_TYPED */ RES_CLASS_UAV,
					/* DXIL_RES_UAV_RAW */ RES_CLASS_UAV,
					/* DXIL_RES_UAV_STRUCTURED */ RES_CLASS_UAV,
					/* DXIL_RES_UAV_STRUCTURED_WITH_COUNTER */ RES_CLASS_INVALID,
				};
				DEV_ASSERT(p_dxil_type < ARRAY_SIZE(DXIL_TYPE_TO_CLASS));
				ResourceClass res_class = DXIL_TYPE_TO_CLASS[p_dxil_type];

				if (p_register == ROOT_CONSTANT_REGISTER && p_space == 0) {
					DEV_ASSERT(res_class == RES_CLASS_CBV);
					shader_data_in.binary_data.dxil_push_constant_stages |= (1 << shader_data_in.stage);
				} else if (p_register == RUNTIME_DATA_REGISTER && p_space == 0) {
					DEV_ASSERT(res_class == RES_CLASS_CBV);
					shader_data_in.binary_data.nir_runtime_data_root_param_idx = 1; // Temporary, to be determined later.
				} else {
					DEV_ASSERT(p_space == 0);

					uint32_t set = p_register / GODOT_NIR_DESCRIPTOR_SET_MULTIPLIER;
					uint32_t binding = (p_register % GODOT_NIR_DESCRIPTOR_SET_MULTIPLIER) / GODOT_NIR_BINDING_MULTIPLIER;

					DEV_ASSERT(set < (uint32_t)shader_data_in.sets_bindings.size());
					[[maybe_unused]] bool found = false;
					for (int j = 0; j < shader_data_in.sets_bindings[set].size(); j++) {
						if (shader_data_in.sets_bindings[set][j].binding != binding) {
							continue;
						}

						ShaderBinary::DataBinding &binding_info = shader_data_in.sets_bindings.write[set].write[j];

						binding_info.dxil_stages |= (1 << shader_data_in.stage);

						if (res_class != RES_CLASS_INVALID) {
							DEV_ASSERT(binding_info.res_class == (uint32_t)RES_CLASS_INVALID || binding_info.res_class == (uint32_t)res_class);
							binding_info.res_class = res_class;
						} else if (p_dxil_type == DXIL_RES_SAMPLER) {
							binding_info.has_sampler = (uint32_t)true;
						} else {
							CRASH_NOW();
						}
						found = true;
						break;
					}
					DEV_ASSERT(found);
				}
			};

			godot_nir_callbacks.report_sc_bit_offset_fn = [](uint32_t p_sc_id, uint64_t p_bit_offset, void *p_data) {
				ShaderData &shader_data_in = *(ShaderData *)p_data;
				[[maybe_unused]] bool found = false;
				for (int j = 0; j < shader_data_in.specialization_constants.size(); j++) {
					if (shader_data_in.specialization_constants[j].constant_id != p_sc_id) {
						continue;
					}

					uint32_t offset_idx = SHADER_STAGES_BIT_OFFSET_INDICES[shader_data_in.stage];
					DEV_ASSERT(shader_data_in.specialization_constants.write[j].stages_bit_offsets[offset_idx] == 0);
					shader_data_in.specialization_constants.write[j].stages_bit_offsets[offset_idx] = p_bit_offset;
					found = true;
					break;
				}
				DEV_ASSERT(found);
			};

			godot_nir_callbacks.report_bitcode_bit_offset_fn = [](uint64_t p_bit_offset, void *p_data) {
				DEV_ASSERT(p_bit_offset % 8 == 0);
				ShaderData &shader_data_in = *(ShaderData *)p_data;
				uint32_t offset_idx = SHADER_STAGES_BIT_OFFSET_INDICES[shader_data_in.stage];
				for (int j = 0; j < shader_data_in.specialization_constants.size(); j++) {
					if (shader_data_in.specialization_constants.write[j].stages_bit_offsets[offset_idx] == 0) {
						// This SC has been optimized out from this stage.
						continue;
					}
					shader_data_in.specialization_constants.write[j].stages_bit_offsets[offset_idx] += p_bit_offset;
				}
			};

			auto shader_model_d3d_to_dxil = [](D3D_SHADER_MODEL p_d3d_shader_model) -> dxil_shader_model {
				static_assert(SHADER_MODEL_6_0 == 0x60000);
				static_assert(SHADER_MODEL_6_3 == 0x60003);
				static_assert(D3D_SHADER_MODEL_6_0 == 0x60);
				static_assert(D3D_SHADER_MODEL_6_3 == 0x63);
				return (dxil_shader_model)((p_d3d_shader_model >> 4) * 0x10000 + (p_d3d_shader_model & 0xf));
			};

			nir_to_dxil_options nir_to_dxil_options = {};
			nir_to_dxil_options.environment = DXIL_ENVIRONMENT_VULKAN;
			nir_to_dxil_options.shader_model_max = shader_model_d3d_to_dxil(shader_capabilities.shader_model);
			nir_to_dxil_options.validator_version_max = NO_DXIL_VALIDATION;
			nir_to_dxil_options.godot_nir_callbacks = &godot_nir_callbacks;

			dxil_logger logger = {};
			logger.log = [](void *p_priv, const char *p_msg) {
#ifdef DEBUG_ENABLED
				print_verbose(p_msg);
#endif
			};

			blob dxil_blob = {};
			bool ok = nir_to_dxil(stages_nir_shaders[stage], &nir_to_dxil_options, &logger, &dxil_blob);
			ralloc_free(stages_nir_shaders[stage]);
			stages_nir_shaders.erase(stage);
			if (!ok) {
				free_nir_shaders();
				ERR_FAIL_V_MSG(Vector<uint8_t>(), "Shader translation at stage " + String(SHADER_STAGE_NAMES[stage]) + " failed.");
			}

			Vector<uint8_t> blob_copy;
			blob_copy.resize(dxil_blob.size);
			memcpy(blob_copy.ptrw(), dxil_blob.data, dxil_blob.size);
			blob_finish(&dxil_blob);
			dxil_blobs.insert(stage, blob_copy);
		}
	}

#if 0
	if (dxil_blobs.has(SHADER_STAGE_FRAGMENT)) {
		Ref<FileAccess> f = FileAccess::open("res://1.dxil", FileAccess::WRITE);
		f->store_buffer(dxil_blobs[SHADER_STAGE_FRAGMENT].ptr(), dxil_blobs[SHADER_STAGE_FRAGMENT].size());
	}
#endif

	// Patch with default values of specialization constants.
	if (specialization_constants.size()) {
		for (const ShaderBinary::SpecializationConstant &sc : specialization_constants) {
			_shader_patch_dxil_specialization_constant((PipelineSpecializationConstantType)sc.type, &sc.int_value, sc.stages_bit_offsets, dxil_blobs, true);
		}
#if 0
		if (dxil_blobs.has(SHADER_STAGE_FRAGMENT)) {
			Ref<FileAccess> f = FileAccess::open("res://2.dxil", FileAccess::WRITE);
			f->store_buffer(dxil_blobs[SHADER_STAGE_FRAGMENT].ptr(), dxil_blobs[SHADER_STAGE_FRAGMENT].size());
		}
#endif
	}

	// Sign.
	for (KeyValue<ShaderStage, Vector<uint8_t>> &E : dxil_blobs) {
		ShaderStage stage = E.key;
		Vector<uint8_t> &dxil_blob = E.value;
		_shader_sign_dxil_bytecode(stage, dxil_blob);
	}

	// Build the root signature.
	ComPtr<ID3DBlob> root_sig_blob;
	{
		auto stages_to_d3d12_visibility = [](uint32_t p_stages_mask) -> D3D12_SHADER_VISIBILITY {
			switch (p_stages_mask) {
				case SHADER_STAGE_VERTEX_BIT: {
					return D3D12_SHADER_VISIBILITY_VERTEX;
				}
				case SHADER_STAGE_FRAGMENT_BIT: {
					return D3D12_SHADER_VISIBILITY_PIXEL;
				}
				default: {
					return D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		};

		LocalVector<D3D12_ROOT_PARAMETER1> root_params;

		// Root (push) constants.
		if (binary_data.dxil_push_constant_stages) {
			CD3DX12_ROOT_PARAMETER1 push_constant;
			push_constant.InitAsConstants(
					binary_data.push_constant_size / sizeof(uint32_t),
					ROOT_CONSTANT_REGISTER,
					0,
					stages_to_d3d12_visibility(binary_data.dxil_push_constant_stages));
			root_params.push_back(push_constant);
		}

		// NIR-DXIL runtime data.
		if (binary_data.nir_runtime_data_root_param_idx == 1) { // Set above to 1 when discovering runtime data is needed.
			DEV_ASSERT(!binary_data.is_compute); // Could be supported if needed, but it's pointless as of now.
			binary_data.nir_runtime_data_root_param_idx = root_params.size();
			CD3DX12_ROOT_PARAMETER1 nir_runtime_data;
			nir_runtime_data.InitAsConstants(
					sizeof(dxil_spirv_vertex_runtime_data) / sizeof(uint32_t),
					RUNTIME_DATA_REGISTER,
					0,
					D3D12_SHADER_VISIBILITY_VERTEX);
			root_params.push_back(nir_runtime_data);
		}

		// Descriptor tables (up to two per uniform set, for resources and/or samplers).

		// These have to stay around until serialization!
		struct TraceableDescriptorTable {
			uint32_t stages_mask = {};
			Vector<D3D12_DESCRIPTOR_RANGE1> ranges;
			Vector<ShaderBinary::DataBinding::RootSignatureLocation *> root_sig_locations;
		};
		Vector<TraceableDescriptorTable> resource_tables_maps;
		Vector<TraceableDescriptorTable> sampler_tables_maps;

		for (int set = 0; set < sets_bindings.size(); set++) {
			bool first_resource_in_set = true;
			bool first_sampler_in_set = true;
			sets_bindings.write[set].sort();
			for (int i = 0; i < sets_bindings[set].size(); i++) {
				const ShaderBinary::DataBinding &binding = sets_bindings[set][i];

				bool really_used = binding.dxil_stages != 0;
#ifdef DEV_ENABLED
				bool anybody_home = (ResourceClass)binding.res_class != RES_CLASS_INVALID || binding.has_sampler;
				DEV_ASSERT(anybody_home == really_used);
#endif
				if (!really_used) {
					continue; // Existed in SPIR-V; went away in DXIL.
				}

				auto insert_range = [](D3D12_DESCRIPTOR_RANGE_TYPE p_range_type,
											uint32_t p_num_descriptors,
											uint32_t p_dxil_register,
											uint32_t p_dxil_stages_mask,
											ShaderBinary::DataBinding::RootSignatureLocation(&p_root_sig_locations),
											Vector<TraceableDescriptorTable> &r_tables,
											bool &r_first_in_set) {
					if (r_first_in_set) {
						r_tables.resize(r_tables.size() + 1);
						r_first_in_set = false;
					}
					TraceableDescriptorTable &table = r_tables.write[r_tables.size() - 1];
					table.stages_mask |= p_dxil_stages_mask;

					CD3DX12_DESCRIPTOR_RANGE1 range;
					// Due to the aliasing hack for SRV-UAV of different families,
					// we can be causing an unintended change of data (sometimes the validation layers catch it).
					D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
					if (p_range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SRV || p_range_type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
						flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
					} else if (p_range_type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) {
						flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
					}
					range.Init(p_range_type, p_num_descriptors, p_dxil_register, 0, flags);

					table.ranges.push_back(range);
					table.root_sig_locations.push_back(&p_root_sig_locations);
				};

				uint32_t num_descriptors = 1;

				D3D12_DESCRIPTOR_RANGE_TYPE resource_range_type = {};
				switch ((ResourceClass)binding.res_class) {
					case RES_CLASS_INVALID: {
						num_descriptors = binding.length;
						DEV_ASSERT(binding.has_sampler);
					} break;
					case RES_CLASS_CBV: {
						resource_range_type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
						DEV_ASSERT(!binding.has_sampler);
					} break;
					case RES_CLASS_SRV: {
						resource_range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						num_descriptors = MAX(1u, binding.length); // An unbound R/O buffer is reflected as zero-size.
					} break;
					case RES_CLASS_UAV: {
						resource_range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						num_descriptors = MAX(1u, binding.length); // An unbound R/W buffer is reflected as zero-size.
						DEV_ASSERT(!binding.has_sampler);
					} break;
				}

				uint32_t dxil_register = set * GODOT_NIR_DESCRIPTOR_SET_MULTIPLIER + binding.binding * GODOT_NIR_BINDING_MULTIPLIER;

				if (binding.res_class != RES_CLASS_INVALID) {
					insert_range(
							resource_range_type,
							num_descriptors,
							dxil_register,
							sets_bindings[set][i].dxil_stages,
							sets_bindings.write[set].write[i].root_sig_locations[RS_LOC_TYPE_RESOURCE],
							resource_tables_maps,
							first_resource_in_set);
				}
				if (binding.has_sampler) {
					insert_range(
							D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
							num_descriptors,
							dxil_register,
							sets_bindings[set][i].dxil_stages,
							sets_bindings.write[set].write[i].root_sig_locations[RS_LOC_TYPE_SAMPLER],
							sampler_tables_maps,
							first_sampler_in_set);
				}
			}
		}

		auto make_descriptor_tables = [&root_params, &stages_to_d3d12_visibility](const Vector<TraceableDescriptorTable> &p_tables) {
			for (const TraceableDescriptorTable &table : p_tables) {
				D3D12_SHADER_VISIBILITY visibility = stages_to_d3d12_visibility(table.stages_mask);
				DEV_ASSERT(table.ranges.size() == table.root_sig_locations.size());
				for (int i = 0; i < table.ranges.size(); i++) {
					// By now we know very well which root signature location corresponds to the pointed uniform.
					table.root_sig_locations[i]->root_param_idx = root_params.size();
					table.root_sig_locations[i]->range_idx = i;
				}

				CD3DX12_ROOT_PARAMETER1 root_table;
				root_table.InitAsDescriptorTable(table.ranges.size(), table.ranges.ptr(), visibility);
				root_params.push_back(root_table);
			}
		};

		make_descriptor_tables(resource_tables_maps);
		make_descriptor_tables(sampler_tables_maps);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
		D3D12_ROOT_SIGNATURE_FLAGS root_sig_flags =
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
		if (!stages_processed.has_flag(SHADER_STAGE_VERTEX_BIT)) {
			root_sig_flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		}
		if (!stages_processed.has_flag(SHADER_STAGE_FRAGMENT_BIT)) {
			root_sig_flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		}
		if (binary_data.vertex_input_mask) {
			root_sig_flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}
		root_sig_desc.Init_1_1(root_params.size(), root_params.ptr(), 0, nullptr, root_sig_flags);

		ComPtr<ID3DBlob> error_blob;
		HRESULT res = D3DX12SerializeVersionedRootSignature(context_driver->lib_d3d12, &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, root_sig_blob.GetAddressOf(), error_blob.GetAddressOf());
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), Vector<uint8_t>(),
				"Serialization of root signature failed with error " + vformat("0x%08ux", (uint64_t)res) + " and the following message:\n" + String((char *)error_blob->GetBufferPointer(), error_blob->GetBufferSize()));

		binary_data.root_signature_crc = crc32(0, nullptr, 0);
		binary_data.root_signature_crc = crc32(binary_data.root_signature_crc, (const Bytef *)root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize());
	}

	Vector<Vector<uint8_t>> compressed_stages;
	Vector<uint32_t> zstd_size;

	uint32_t stages_binary_size = 0;

	for (uint32_t i = 0; i < p_spirv.size(); i++) {
		Vector<uint8_t> zstd;
		Vector<uint8_t> &dxil_blob = dxil_blobs[p_spirv[i].shader_stage];
		zstd.resize(Compression::get_max_compressed_buffer_size(dxil_blob.size(), Compression::MODE_ZSTD));
		int dst_size = Compression::compress(zstd.ptrw(), dxil_blob.ptr(), dxil_blob.size(), Compression::MODE_ZSTD);

		zstd_size.push_back(dst_size);
		zstd.resize(dst_size);
		compressed_stages.push_back(zstd);

		uint32_t s = compressed_stages[i].size();
		stages_binary_size += STEPIFY(s, 4);
	}

	CharString shader_name_utf = p_shader_name.utf8();

	binary_data.shader_name_len = shader_name_utf.length();

	uint32_t total_size = sizeof(uint32_t) * 3; // Header + version + main datasize;.
	total_size += sizeof(ShaderBinary::Data);

	total_size += STEPIFY(binary_data.shader_name_len, 4);

	for (int i = 0; i < sets_bindings.size(); i++) {
		total_size += sizeof(uint32_t);
		total_size += sets_bindings[i].size() * sizeof(ShaderBinary::DataBinding);
	}

	total_size += sizeof(ShaderBinary::SpecializationConstant) * specialization_constants.size();

	total_size += compressed_stages.size() * sizeof(uint32_t) * 3; // Sizes.
	total_size += stages_binary_size;

	binary_data.root_signature_len = root_sig_blob->GetBufferSize();
	total_size += binary_data.root_signature_len;

	Vector<uint8_t> ret;
	ret.resize(total_size);
	{
		uint32_t offset = 0;
		uint8_t *binptr = ret.ptrw();
		binptr[0] = 'G';
		binptr[1] = 'S';
		binptr[2] = 'B';
		binptr[3] = 'D'; // Godot shader binary data.
		offset += 4;
		encode_uint32(ShaderBinary::VERSION, binptr + offset);
		offset += sizeof(uint32_t);
		encode_uint32(sizeof(ShaderBinary::Data), binptr + offset);
		offset += sizeof(uint32_t);
		memcpy(binptr + offset, &binary_data, sizeof(ShaderBinary::Data));
		offset += sizeof(ShaderBinary::Data);

#define ADVANCE_OFFSET_WITH_ALIGNMENT(m_bytes)                         \
	{                                                                  \
		offset += m_bytes;                                             \
		uint32_t padding = STEPIFY(m_bytes, 4) - m_bytes;              \
		memset(binptr + offset, 0, padding); /* Avoid garbage data. */ \
		offset += padding;                                             \
	}

		if (binary_data.shader_name_len > 0) {
			memcpy(binptr + offset, shader_name_utf.ptr(), binary_data.shader_name_len);
			ADVANCE_OFFSET_WITH_ALIGNMENT(binary_data.shader_name_len);
		}

		for (int i = 0; i < sets_bindings.size(); i++) {
			int count = sets_bindings[i].size();
			encode_uint32(count, binptr + offset);
			offset += sizeof(uint32_t);
			if (count > 0) {
				memcpy(binptr + offset, sets_bindings[i].ptr(), sizeof(ShaderBinary::DataBinding) * count);
				offset += sizeof(ShaderBinary::DataBinding) * count;
			}
		}

		if (specialization_constants.size()) {
			memcpy(binptr + offset, specialization_constants.ptr(), sizeof(ShaderBinary::SpecializationConstant) * specialization_constants.size());
			offset += sizeof(ShaderBinary::SpecializationConstant) * specialization_constants.size();
		}

		for (int i = 0; i < compressed_stages.size(); i++) {
			encode_uint32(p_spirv[i].shader_stage, binptr + offset);
			offset += sizeof(uint32_t);
			encode_uint32(dxil_blobs[p_spirv[i].shader_stage].size(), binptr + offset);
			offset += sizeof(uint32_t);
			encode_uint32(zstd_size[i], binptr + offset);
			offset += sizeof(uint32_t);
			memcpy(binptr + offset, compressed_stages[i].ptr(), compressed_stages[i].size());
			ADVANCE_OFFSET_WITH_ALIGNMENT(compressed_stages[i].size());
		}

		memcpy(binptr + offset, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize());
		offset += root_sig_blob->GetBufferSize();

		ERR_FAIL_COND_V(offset != (uint32_t)ret.size(), Vector<uint8_t>());
	}

	return ret;
}

RDD::ShaderID RenderingDeviceDriverD3D12::shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name) {
	r_shader_desc = {}; // Driver-agnostic.
	ShaderInfo shader_info_in; // Driver-specific.

	const uint8_t *binptr = p_shader_binary.ptr();
	uint32_t binsize = p_shader_binary.size();

	uint32_t read_offset = 0;

	// Consistency check.
	ERR_FAIL_COND_V(binsize < sizeof(uint32_t) * 3 + sizeof(ShaderBinary::Data), ShaderID());
	ERR_FAIL_COND_V(binptr[0] != 'G' || binptr[1] != 'S' || binptr[2] != 'B' || binptr[3] != 'D', ShaderID());

	uint32_t bin_version = decode_uint32(binptr + 4);
	ERR_FAIL_COND_V(bin_version != ShaderBinary::VERSION, ShaderID());

	uint32_t bin_data_size = decode_uint32(binptr + 8);

	const ShaderBinary::Data &binary_data = *(reinterpret_cast<const ShaderBinary::Data *>(binptr + 12));

	r_shader_desc.push_constant_size = binary_data.push_constant_size;
	shader_info_in.dxil_push_constant_size = binary_data.dxil_push_constant_stages ? binary_data.push_constant_size : 0;
	shader_info_in.nir_runtime_data_root_param_idx = binary_data.nir_runtime_data_root_param_idx;

	r_shader_desc.vertex_input_mask = binary_data.vertex_input_mask;
	r_shader_desc.fragment_output_mask = binary_data.fragment_output_mask;

	r_shader_desc.is_compute = binary_data.is_compute;
	shader_info_in.is_compute = binary_data.is_compute;
	r_shader_desc.compute_local_size[0] = binary_data.compute_local_size[0];
	r_shader_desc.compute_local_size[1] = binary_data.compute_local_size[1];
	r_shader_desc.compute_local_size[2] = binary_data.compute_local_size[2];

	read_offset += sizeof(uint32_t) * 3 + bin_data_size;

	if (binary_data.shader_name_len) {
		r_name.parse_utf8((const char *)(binptr + read_offset), binary_data.shader_name_len);
		read_offset += STEPIFY(binary_data.shader_name_len, 4);
	}

	r_shader_desc.uniform_sets.resize(binary_data.set_count);
	shader_info_in.sets.resize(binary_data.set_count);

	for (uint32_t i = 0; i < binary_data.set_count; i++) {
		ERR_FAIL_COND_V(read_offset + sizeof(uint32_t) >= binsize, ShaderID());
		uint32_t set_count = decode_uint32(binptr + read_offset);
		read_offset += sizeof(uint32_t);
		const ShaderBinary::DataBinding *set_ptr = reinterpret_cast<const ShaderBinary::DataBinding *>(binptr + read_offset);
		uint32_t set_size = set_count * sizeof(ShaderBinary::DataBinding);
		ERR_FAIL_COND_V(read_offset + set_size >= binsize, ShaderID());

		shader_info_in.sets[i].bindings.reserve(set_count);

		for (uint32_t j = 0; j < set_count; j++) {
			ShaderUniform info;
			info.type = UniformType(set_ptr[j].type);
			info.writable = set_ptr[j].writable;
			info.length = set_ptr[j].length;
			info.binding = set_ptr[j].binding;

			ShaderInfo::UniformBindingInfo binding;
			binding.stages = set_ptr[j].dxil_stages;
			binding.res_class = (ResourceClass)set_ptr[j].res_class;
			binding.type = info.type;
			binding.length = info.length;
#ifdef DEV_ENABLED
			binding.writable = set_ptr[j].writable;
#endif
			static_assert(sizeof(ShaderInfo::UniformBindingInfo::root_sig_locations) == sizeof(ShaderBinary::DataBinding::root_sig_locations));
			memcpy((void *)&binding.root_sig_locations, (void *)&set_ptr[j].root_sig_locations, sizeof(ShaderInfo::UniformBindingInfo::root_sig_locations));

			if (binding.root_sig_locations.resource.root_param_idx != UINT32_MAX) {
				shader_info_in.sets[i].num_root_params.resources++;
			}
			if (binding.root_sig_locations.sampler.root_param_idx != UINT32_MAX) {
				shader_info_in.sets[i].num_root_params.samplers++;
			}

			r_shader_desc.uniform_sets.write[i].push_back(info);
			shader_info_in.sets[i].bindings.push_back(binding);
		}

		read_offset += set_size;
	}

	ERR_FAIL_COND_V(read_offset + binary_data.specialization_constants_count * sizeof(ShaderBinary::SpecializationConstant) >= binsize, ShaderID());

	r_shader_desc.specialization_constants.resize(binary_data.specialization_constants_count);
	shader_info_in.specialization_constants.resize(binary_data.specialization_constants_count);
	for (uint32_t i = 0; i < binary_data.specialization_constants_count; i++) {
		const ShaderBinary::SpecializationConstant &src_sc = *(reinterpret_cast<const ShaderBinary::SpecializationConstant *>(binptr + read_offset));
		ShaderSpecializationConstant sc;
		sc.type = PipelineSpecializationConstantType(src_sc.type);
		sc.constant_id = src_sc.constant_id;
		sc.int_value = src_sc.int_value;
		sc.stages = src_sc.stage_flags;
		r_shader_desc.specialization_constants.write[i] = sc;

		ShaderInfo::SpecializationConstant ssc;
		ssc.constant_id = src_sc.constant_id;
		ssc.int_value = src_sc.int_value;
		memcpy(ssc.stages_bit_offsets, src_sc.stages_bit_offsets, sizeof(ssc.stages_bit_offsets));
		shader_info_in.specialization_constants[i] = ssc;

		read_offset += sizeof(ShaderBinary::SpecializationConstant);
	}
	shader_info_in.spirv_specialization_constants_ids_mask = binary_data.spirv_specialization_constants_ids_mask;

	for (uint32_t i = 0; i < binary_data.stage_count; i++) {
		ERR_FAIL_COND_V(read_offset + sizeof(uint32_t) * 3 >= binsize, ShaderID());

		uint32_t stage = decode_uint32(binptr + read_offset);
		read_offset += sizeof(uint32_t);
		uint32_t dxil_size = decode_uint32(binptr + read_offset);
		read_offset += sizeof(uint32_t);
		uint32_t zstd_size = decode_uint32(binptr + read_offset);
		read_offset += sizeof(uint32_t);

		// Decompress.
		Vector<uint8_t> dxil;
		dxil.resize(dxil_size);
		int dec_dxil_size = Compression::decompress(dxil.ptrw(), dxil.size(), binptr + read_offset, zstd_size, Compression::MODE_ZSTD);
		ERR_FAIL_COND_V(dec_dxil_size != (int32_t)dxil_size, ShaderID());
		shader_info_in.stages_bytecode[ShaderStage(stage)] = dxil;

		zstd_size = STEPIFY(zstd_size, 4);
		read_offset += zstd_size;
		ERR_FAIL_COND_V(read_offset > binsize, ShaderID());

		r_shader_desc.stages.push_back(ShaderStage(stage));
	}

	const uint8_t *root_sig_data_ptr = binptr + read_offset;

	PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER d3d_D3D12CreateRootSignatureDeserializer = (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)(void *)GetProcAddress(context_driver->lib_d3d12, "D3D12CreateRootSignatureDeserializer");
	ERR_FAIL_NULL_V(d3d_D3D12CreateRootSignatureDeserializer, ShaderID());

	HRESULT res = d3d_D3D12CreateRootSignatureDeserializer(root_sig_data_ptr, binary_data.root_signature_len, IID_PPV_ARGS(shader_info_in.root_signature_deserializer.GetAddressOf()));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ShaderID(), "D3D12CreateRootSignatureDeserializer failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	read_offset += binary_data.root_signature_len;

	ERR_FAIL_COND_V(read_offset != binsize, ShaderID());

	ComPtr<ID3D12RootSignature> root_signature;
	res = device->CreateRootSignature(0, root_sig_data_ptr, binary_data.root_signature_len, IID_PPV_ARGS(shader_info_in.root_signature.GetAddressOf()));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ShaderID(), "CreateRootSignature failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	shader_info_in.root_signature_desc = shader_info_in.root_signature_deserializer->GetRootSignatureDesc();
	shader_info_in.root_signature_crc = binary_data.root_signature_crc;

	// Bookkeep.

	ShaderInfo *shader_info_ptr = VersatileResource::allocate<ShaderInfo>(resources_allocator);
	*shader_info_ptr = shader_info_in;
	return ShaderID(shader_info_ptr);
}

uint32_t RenderingDeviceDriverD3D12::shader_get_layout_hash(ShaderID p_shader) {
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;
	return shader_info_in->root_signature_crc;
}

void RenderingDeviceDriverD3D12::shader_free(ShaderID p_shader) {
	ShaderInfo *shader_info_in = (ShaderInfo *)p_shader.id;
	VersatileResource::free(resources_allocator, shader_info_in);
}

void RenderingDeviceDriverD3D12::shader_destroy_modules(ShaderID p_shader) {
	ShaderInfo *shader_info_in = (ShaderInfo *)p_shader.id;
	shader_info_in->stages_bytecode.clear();
}

/*********************/
/**** UNIFORM SET ****/
/*********************/

static void _add_descriptor_count_for_uniform(RenderingDevice::UniformType p_type, uint32_t p_binding_length, bool p_dobule_srv_uav_ambiguous, uint32_t &r_num_resources, uint32_t &r_num_samplers, bool &r_srv_uav_ambiguity) {
	r_srv_uav_ambiguity = false;

	// Some resource types can be SRV or UAV, depending on what NIR-DXIL decided for a specific shader variant.
	// The goal is to generate both SRV and UAV for the descriptor sets' heaps and copy only the relevant one
	// to the frame descriptor heap at binding time.
	// [[SRV_UAV_AMBIGUITY]]

	switch (p_type) {
		case RenderingDevice::UNIFORM_TYPE_SAMPLER: {
			r_num_samplers += p_binding_length;
		} break;
		case RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
		case RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
			r_num_resources += p_binding_length;
			r_num_samplers += p_binding_length;
		} break;
		case RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER: {
			r_num_resources += 1;
		} break;
		case RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER: {
			r_num_resources += p_dobule_srv_uav_ambiguous ? 2 : 1;
			r_srv_uav_ambiguity = true;
		} break;
		case RenderingDevice::UNIFORM_TYPE_IMAGE: {
			r_num_resources += p_binding_length * (p_dobule_srv_uav_ambiguous ? 2 : 1);
			r_srv_uav_ambiguity = true;
		} break;
		default: {
			r_num_resources += p_binding_length;
		}
	}
}

RDD::UniformSetID RenderingDeviceDriverD3D12::uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index) {
	// Pre-bookkeep.
	UniformSetInfo *uniform_set_info = VersatileResource::allocate<UniformSetInfo>(resources_allocator);

	// Do a first pass to count resources and samplers.
	uint32_t num_resource_descs = 0;
	uint32_t num_sampler_descs = 0;
	for (uint32_t i = 0; i < p_uniforms.size(); i++) {
		const BoundUniform &uniform = p_uniforms[i];

		// Since the uniform set may be created for a shader different than the one that will be actually bound,
		// which may have a different set of uniforms optimized out, the stages mask we can check now is not reliable.
		// Therefore, we can't make any assumptions here about descriptors that we may not need to create,
		// pixel or vertex-only shader resource states, etc.

		bool srv_uav_ambiguity = false;
		uint32_t binding_length = uniform.ids.size();
		if (uniform.type == UNIFORM_TYPE_SAMPLER_WITH_TEXTURE || uniform.type == UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER) {
			binding_length /= 2;
		}
		_add_descriptor_count_for_uniform(uniform.type, binding_length, true, num_resource_descs, num_sampler_descs, srv_uav_ambiguity);
	}
#ifdef DEV_ENABLED
	uniform_set_info->resources_desc_info.reserve(num_resource_descs);
#endif

	if (num_resource_descs) {
		Error err = uniform_set_info->desc_heaps.resources.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, num_resource_descs, false);
		if (err) {
			VersatileResource::free(resources_allocator, uniform_set_info);
			ERR_FAIL_V(UniformSetID());
		}
	}
	if (num_sampler_descs) {
		Error err = uniform_set_info->desc_heaps.samplers.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, num_sampler_descs, false);
		if (err) {
			VersatileResource::free(resources_allocator, uniform_set_info);
			ERR_FAIL_V(UniformSetID());
		}
	}
	struct {
		DescriptorsHeap::Walker resources;
		DescriptorsHeap::Walker samplers;
	} desc_heap_walkers;
	desc_heap_walkers.resources = uniform_set_info->desc_heaps.resources.make_walker();
	desc_heap_walkers.samplers = uniform_set_info->desc_heaps.samplers.make_walker();

	struct NeededState {
		bool is_buffer = false;
		uint64_t shader_uniform_idx_mask = 0;
		D3D12_RESOURCE_STATES states = {};
	};
	HashMap<ResourceInfo *, NeededState> resource_states;

	for (uint32_t i = 0; i < p_uniforms.size(); i++) {
		const BoundUniform &uniform = p_uniforms[i];

#ifdef DEV_ENABLED
		const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;
		const ShaderInfo::UniformBindingInfo &shader_uniform = shader_info_in->sets[p_set_index].bindings[i];
		bool is_compute = shader_info_in->stages_bytecode.has(SHADER_STAGE_COMPUTE);
		DEV_ASSERT(!(is_compute && (shader_uniform.stages & (SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT))));
		DEV_ASSERT(!(!is_compute && (shader_uniform.stages & SHADER_STAGE_COMPUTE_BIT)));
#endif

		switch (uniform.type) {
			case UNIFORM_TYPE_SAMPLER: {
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					const D3D12_SAMPLER_DESC &sampler_desc = samplers[uniform.ids[j].id];
					device->CreateSampler(&sampler_desc, desc_heap_walkers.samplers.get_curr_cpu_handle());
					desc_heap_walkers.samplers.advance();
				}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
				for (uint32_t j = 0; j < uniform.ids.size(); j += 2) {
					const D3D12_SAMPLER_DESC &sampler_desc = samplers[uniform.ids[j].id];
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j + 1].id;

					device->CreateSampler(&sampler_desc, desc_heap_walkers.samplers.get_curr_cpu_handle());
					desc_heap_walkers.samplers.advance();
					device->CreateShaderResourceView(texture_info->resource, &texture_info->view_descs.srv, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_info->view_descs.srv.ViewDimension });
#endif
					desc_heap_walkers.resources.advance();

					NeededState &ns = resource_states[texture_info];
					ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
					ns.states |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				}
			} break;
			case UNIFORM_TYPE_TEXTURE: {
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j].id;
					device->CreateShaderResourceView(texture_info->resource, &texture_info->view_descs.srv, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_info->view_descs.srv.ViewDimension });
#endif
					desc_heap_walkers.resources.advance();

					NeededState &ns = resource_states[texture_info];
					ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
					ns.states |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
				}
			} break;
			case UNIFORM_TYPE_IMAGE: {
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j].id;

					NeededState &ns = resource_states[texture_info];
					ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
					ns.states |= (D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}

				// SRVs first. [[SRV_UAV_AMBIGUITY]]
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j].id;

					device->CreateShaderResourceView(texture_info->resource, &texture_info->view_descs.srv, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_info->view_descs.srv.ViewDimension });
#endif
					desc_heap_walkers.resources.advance();
				}

				// UAVs then. [[SRV_UAV_AMBIGUITY]]
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j].id;

					device->CreateUnorderedAccessView(texture_info->resource, nullptr, &texture_info->view_descs.uav, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, {} });
#endif
					desc_heap_walkers.resources.advance();
				}
			} break;
			case UNIFORM_TYPE_TEXTURE_BUFFER:
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
				CRASH_NOW_MSG("Unimplemented!");
			} break;
			case UNIFORM_TYPE_IMAGE_BUFFER: {
				CRASH_NOW_MSG("Unimplemented!");
			} break;
			case UNIFORM_TYPE_UNIFORM_BUFFER: {
				BufferInfo *buf_info = (BufferInfo *)uniform.ids[0].id;

				D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
				cbv_desc.BufferLocation = buf_info->resource->GetGPUVirtualAddress();
				cbv_desc.SizeInBytes = STEPIFY(buf_info->size, 256);
				device->CreateConstantBufferView(&cbv_desc, desc_heap_walkers.resources.get_curr_cpu_handle());
				desc_heap_walkers.resources.advance();
#ifdef DEV_ENABLED
				uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_CBV, {} });
#endif

				NeededState &ns = resource_states[buf_info];
				ns.is_buffer = true;
				ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
				ns.states |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			} break;
			case UNIFORM_TYPE_STORAGE_BUFFER: {
				BufferInfo *buf_info = (BufferInfo *)uniform.ids[0].id;

				// SRV first. [[SRV_UAV_AMBIGUITY]]
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
					srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
					srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srv_desc.Buffer.FirstElement = 0;
					srv_desc.Buffer.NumElements = (buf_info->size + 3) / 4;
					srv_desc.Buffer.StructureByteStride = 0;
					srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
					device->CreateShaderResourceView(buf_info->resource, &srv_desc, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srv_desc.ViewDimension });
#endif
					desc_heap_walkers.resources.advance();
				}

				// UAV then. [[SRV_UAV_AMBIGUITY]]
				{
					if (buf_info->flags.usable_as_uav) {
						D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
						uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
						uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
						uav_desc.Buffer.FirstElement = 0;
						uav_desc.Buffer.NumElements = (buf_info->size + 3) / 4;
						uav_desc.Buffer.StructureByteStride = 0;
						uav_desc.Buffer.CounterOffsetInBytes = 0;
						uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
						device->CreateUnorderedAccessView(buf_info->resource, nullptr, &uav_desc, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
						uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, {} });
#endif
					} else {
						// If can't transition to UAV, leave this one empty since it won't be
						// used, and trying to create an UAV view would trigger a validation error.
					}

					desc_heap_walkers.resources.advance();
				}

				NeededState &ns = resource_states[buf_info];
				ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
				ns.is_buffer = true;
				ns.states |= (D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			} break;
			case UNIFORM_TYPE_INPUT_ATTACHMENT: {
				for (uint32_t j = 0; j < uniform.ids.size(); j++) {
					TextureInfo *texture_info = (TextureInfo *)uniform.ids[j].id;

					device->CreateShaderResourceView(texture_info->resource, &texture_info->view_descs.srv, desc_heap_walkers.resources.get_curr_cpu_handle());
#ifdef DEV_ENABLED
					uniform_set_info->resources_desc_info.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_info->view_descs.srv.ViewDimension });
#endif
					desc_heap_walkers.resources.advance();

					NeededState &ns = resource_states[texture_info];
					ns.shader_uniform_idx_mask |= ((uint64_t)1 << i);
					ns.states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}
			} break;
			default: {
				DEV_ASSERT(false);
			}
		}
	}

	DEV_ASSERT(desc_heap_walkers.resources.is_at_eof());
	DEV_ASSERT(desc_heap_walkers.samplers.is_at_eof());

	{
		uniform_set_info->resource_states.reserve(resource_states.size());
		for (const KeyValue<ResourceInfo *, NeededState> &E : resource_states) {
			UniformSetInfo::StateRequirement sr;
			sr.resource = E.key;
			sr.is_buffer = E.value.is_buffer;
			sr.states = E.value.states;
			sr.shader_uniform_idx_mask = E.value.shader_uniform_idx_mask;
			uniform_set_info->resource_states.push_back(sr);
		}
	}

	return UniformSetID(uniform_set_info);
}

void RenderingDeviceDriverD3D12::uniform_set_free(UniformSetID p_uniform_set) {
	UniformSetInfo *uniform_set_info = (UniformSetInfo *)p_uniform_set.id;
	VersatileResource::free(resources_allocator, uniform_set_info);
}

// ----- COMMANDS -----

void RenderingDeviceDriverD3D12::command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	if (barrier_capabilities.enhanced_barriers_supported) {
		return;
	}

	// Perform pending blackouts.
	{
		SelfList<TextureInfo> *E = textures_pending_clear.first();
		while (E) {
			TextureSubresourceRange subresources;
			subresources.layer_count = E->self()->layers;
			subresources.mipmap_count = E->self()->mipmaps;
			command_clear_color_texture(p_cmd_buffer, TextureID(E->self()), TEXTURE_LAYOUT_UNDEFINED, Color(), subresources);

			SelfList<TextureInfo> *next = E->next();
			E->remove_from_list();
			E = next;
		}
	}

	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	const UniformSetInfo *uniform_set_info = (const UniformSetInfo *)p_uniform_set.id;
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;
	const ShaderInfo::UniformSet &shader_set = shader_info_in->sets[p_set_index];

	for (const UniformSetInfo::StateRequirement &sr : uniform_set_info->resource_states) {
#ifdef DEV_ENABLED
		{
			uint32_t stages = 0;
			D3D12_RESOURCE_STATES wanted_state = {};
			bool writable = false;
			// Doing the full loop for debugging since the real one below may break early,
			// but we want an exhaustive check
			uint64_t inv_uniforms_mask = ~sr.shader_uniform_idx_mask; // Inverting the mask saves operations.
			for (uint8_t bit = 0; inv_uniforms_mask != UINT64_MAX; bit++) {
				uint64_t bit_mask = ((uint64_t)1 << bit);
				if (likely((inv_uniforms_mask & bit_mask))) {
					continue;
				}
				inv_uniforms_mask |= bit_mask;

				const ShaderInfo::UniformBindingInfo &binding = shader_set.bindings[bit];
				if (unlikely(!binding.stages)) {
					continue;
				}

				D3D12_RESOURCE_STATES required_states = sr.states;

				// Resolve a case of SRV/UAV ambiguity now. [[SRV_UAV_AMBIGUITY]]
				if ((required_states & D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) && (required_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) {
					if (binding.res_class == RES_CLASS_SRV) {
						required_states &= ~D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					} else {
						required_states = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					}
				}

				if (stages) { // Second occurrence at least?
					CRASH_COND_MSG(binding.writable != writable, "A resource is used in the same uniform set both as R/O and R/W. That's not supported and shouldn't happen.");
					CRASH_COND_MSG(required_states != wanted_state, "A resource is used in the same uniform set with different resource states. The code needs to be enhanced to support that.");
				} else {
					wanted_state = required_states;
					stages |= binding.stages;
					writable = binding.writable;
				}

				DEV_ASSERT((wanted_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) == (bool)(wanted_state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			}
		}
#endif

		// We may have assumed D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE for a resource,
		// because at uniform set creation time we couldn't know for sure which stages
		// it would be used in (due to the fact that a set can be created against a different,
		// albeit compatible, shader, which may make a different usage in the end).
		// However, now we know and can exclude up to one unneeded states.

		// TODO: If subresources involved already in the needed states, or scheduled for it,
		// maybe it's more optimal not to do anything here

		uint32_t stages = 0;
		D3D12_RESOURCE_STATES wanted_state = {};
		uint64_t inv_uniforms_mask = ~sr.shader_uniform_idx_mask; // Inverting the mask saves operations.
		for (uint8_t bit = 0; inv_uniforms_mask != UINT64_MAX; bit++) {
			uint64_t bit_mask = ((uint64_t)1 << bit);
			if (likely((inv_uniforms_mask & bit_mask))) {
				continue;
			}
			inv_uniforms_mask |= bit_mask;

			const ShaderInfo::UniformBindingInfo &binding = shader_set.bindings[bit];
			if (unlikely(!binding.stages)) {
				continue;
			}

			if (!stages) {
				D3D12_RESOURCE_STATES required_states = sr.states;

				// Resolve a case of SRV/UAV ambiguity now. [[SRV_UAV_AMBIGUITY]]
				if ((required_states & D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) && (required_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) {
					if (binding.res_class == RES_CLASS_SRV) {
						required_states &= ~D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					} else {
						required_states = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					}
				}

				wanted_state = required_states;

				if (!(wanted_state & D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)) {
					// By now, we already know the resource is used, and with no PS/NON_PS disjuntive; no need to check further.
					break;
				}
			}

			stages |= binding.stages;

			if (stages == (SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT) || stages == SHADER_STAGE_COMPUTE_BIT) {
				// By now, we already know the resource is used, and as both PS/NON_PS; no need to check further.
				break;
			}
		}

		if (likely(wanted_state)) {
			if ((wanted_state & D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)) {
				if (stages == SHADER_STAGE_VERTEX_BIT || stages == SHADER_STAGE_COMPUTE_BIT) {
					D3D12_RESOURCE_STATES unneeded_states = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
					wanted_state &= ~unneeded_states;
				} else if (stages == SHADER_STAGE_FRAGMENT_BIT) {
					D3D12_RESOURCE_STATES unneeded_states = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
					wanted_state &= ~unneeded_states;
				}
			}

			if (likely(wanted_state)) {
				if (sr.is_buffer) {
					_resource_transition_batch(cmd_buf_info, sr.resource, 0, 1, wanted_state);
				} else {
					TextureInfo *tex_info = (TextureInfo *)sr.resource;
					uint32_t planes = 1;
					if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
						planes = format_get_plane_count(tex_info->format);
					}
					for (uint32_t i = 0; i < tex_info->layers; i++) {
						for (uint32_t j = 0; j < tex_info->mipmaps; j++) {
							uint32_t subresource = D3D12CalcSubresource(tex_info->base_mip + j, tex_info->base_layer + i, 0, tex_info->desc.MipLevels, tex_info->desc.ArraySize());
							_resource_transition_batch(cmd_buf_info, tex_info, subresource, planes, wanted_state);
						}
					}
				}
			}
		}
	}

	if (p_set_index == shader_info_in->sets.size() - 1) {
		_resource_transitions_flush(cmd_buf_info);
	}
}

void RenderingDeviceDriverD3D12::_command_check_descriptor_sets(CommandBufferID p_cmd_buffer) {
	DEV_ASSERT(segment_begun && "Unable to use commands that rely on descriptors because a segment was never begun.");

	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	if (!cmd_buf_info->descriptor_heaps_set) {
		// Set descriptor heaps for the command buffer if they haven't been set yet.
		ID3D12DescriptorHeap *heaps[] = {
			frames[frame_idx].desc_heaps.resources.get_heap(),
			frames[frame_idx].desc_heaps.samplers.get_heap(),
		};

		cmd_buf_info->cmd_list->SetDescriptorHeaps(2, heaps);
		cmd_buf_info->descriptor_heaps_set = true;
	}
}

void RenderingDeviceDriverD3D12::_command_bind_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index, bool p_for_compute) {
	_command_check_descriptor_sets(p_cmd_buffer);

	UniformSetInfo *uniform_set_info = (UniformSetInfo *)p_uniform_set.id;
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;
	const ShaderInfo::UniformSet &shader_set = shader_info_in->sets[p_set_index];
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;

	using SetRootDescriptorTableFn = void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::*)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE);
	SetRootDescriptorTableFn set_root_desc_table_fn = p_for_compute ? &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable : &ID3D12GraphicsCommandList1::SetGraphicsRootDescriptorTable;

	// If this set's descriptors have already been set for the current execution and a compatible root signature, reuse!
	uint32_t root_sig_crc = p_for_compute ? cmd_buf_info->compute_root_signature_crc : cmd_buf_info->graphics_root_signature_crc;
	UniformSetInfo::RecentBind *last_bind = nullptr;
	for (int i = 0; i < (int)ARRAY_SIZE(uniform_set_info->recent_binds); i++) {
		if (uniform_set_info->recent_binds[i].segment_serial == frames[frame_idx].segment_serial) {
			if (uniform_set_info->recent_binds[i].root_signature_crc == root_sig_crc) {
				for (const RootDescriptorTable &table : uniform_set_info->recent_binds[i].root_tables.resources) {
					(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(table.root_param_idx, table.start_gpu_handle);
				}
				for (const RootDescriptorTable &table : uniform_set_info->recent_binds[i].root_tables.samplers) {
					(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(table.root_param_idx, table.start_gpu_handle);
				}
#ifdef DEV_ENABLED
				uniform_set_info->recent_binds[i].uses++;
				frames[frame_idx].uniform_set_reused++;
#endif
				return;
			} else {
				if (!last_bind || uniform_set_info->recent_binds[i].uses < last_bind->uses) {
					// Prefer this one since it's been used less or we still haven't a better option.
					last_bind = &uniform_set_info->recent_binds[i];
				}
			}
		} else {
			// Prefer this one since it's unused.
			last_bind = &uniform_set_info->recent_binds[i];
			last_bind->uses = 0;
		}
	}

	struct {
		DescriptorsHeap::Walker *resources = nullptr;
		DescriptorsHeap::Walker *samplers = nullptr;
	} frame_heap_walkers;
	frame_heap_walkers.resources = &frames[frame_idx].desc_heap_walkers.resources;
	frame_heap_walkers.samplers = &frames[frame_idx].desc_heap_walkers.samplers;

	struct {
		DescriptorsHeap::Walker resources;
		DescriptorsHeap::Walker samplers;
	} set_heap_walkers;
	set_heap_walkers.resources = uniform_set_info->desc_heaps.resources.make_walker();
	set_heap_walkers.samplers = uniform_set_info->desc_heaps.samplers.make_walker();

#ifdef DEV_ENABLED
	// Whether we have stages where the uniform is actually used should match
	// whether we have any root signature locations for it.
	for (uint32_t i = 0; i < shader_set.bindings.size(); i++) {
		bool has_rs_locations = false;
		if (shader_set.bindings[i].root_sig_locations.resource.root_param_idx != UINT32_MAX ||
				shader_set.bindings[i].root_sig_locations.sampler.root_param_idx != UINT32_MAX) {
			has_rs_locations = true;
			break;
		}

		bool has_stages = shader_set.bindings[i].stages;

		DEV_ASSERT(has_rs_locations == has_stages);
	}
#endif

	last_bind->root_tables.resources.reserve(shader_set.num_root_params.resources);
	last_bind->root_tables.resources.clear();
	last_bind->root_tables.samplers.reserve(shader_set.num_root_params.samplers);
	last_bind->root_tables.samplers.clear();
	last_bind->uses++;

	struct {
		RootDescriptorTable *resources = nullptr;
		RootDescriptorTable *samplers = nullptr;
	} tables;
	for (uint32_t i = 0; i < shader_set.bindings.size(); i++) {
		const ShaderInfo::UniformBindingInfo &binding = shader_set.bindings[i];

		uint32_t num_resource_descs = 0;
		uint32_t num_sampler_descs = 0;
		bool srv_uav_ambiguity = false;
		_add_descriptor_count_for_uniform(binding.type, binding.length, false, num_resource_descs, num_sampler_descs, srv_uav_ambiguity);

		bool resource_used = false;
		if (shader_set.bindings[i].stages) {
			{
				const ShaderInfo::UniformBindingInfo::RootSignatureLocation &rs_loc_resource = shader_set.bindings[i].root_sig_locations.resource;
				if (rs_loc_resource.root_param_idx != UINT32_MAX) { // Location used?
					DEV_ASSERT(num_resource_descs);
					DEV_ASSERT(!(srv_uav_ambiguity && (shader_set.bindings[i].res_class != RES_CLASS_SRV && shader_set.bindings[i].res_class != RES_CLASS_UAV))); // [[SRV_UAV_AMBIGUITY]]

					bool must_flush_table = tables.resources && rs_loc_resource.root_param_idx != tables.resources->root_param_idx;
					if (must_flush_table) {
						// Check the root signature data has been filled ordered.
						DEV_ASSERT(rs_loc_resource.root_param_idx > tables.resources->root_param_idx);

						(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(tables.resources->root_param_idx, tables.resources->start_gpu_handle);
						tables.resources = nullptr;
					}

					if (unlikely(frame_heap_walkers.resources->get_free_handles() < num_resource_descs)) {
						if (!frames[frame_idx].desc_heaps_exhausted_reported.resources) {
							frames[frame_idx].desc_heaps_exhausted_reported.resources = true;
							ERR_FAIL_MSG("Cannot bind uniform set because there's no enough room in current frame's RESOURCES descriptor heap.\n"
										 "Please increase the value of the rendering/rendering_device/d3d12/max_resource_descriptors_per_frame project setting.");
						} else {
							return;
						}
					}

					if (!tables.resources) {
						DEV_ASSERT(last_bind->root_tables.resources.size() < last_bind->root_tables.resources.get_capacity());
						last_bind->root_tables.resources.resize(last_bind->root_tables.resources.size() + 1);
						tables.resources = &last_bind->root_tables.resources[last_bind->root_tables.resources.size() - 1];
						tables.resources->root_param_idx = rs_loc_resource.root_param_idx;
						tables.resources->start_gpu_handle = frame_heap_walkers.resources->get_curr_gpu_handle();
					}

					// If there is ambiguity and it didn't clarify as SRVs, skip them, which come first. [[SRV_UAV_AMBIGUITY]]
					if (srv_uav_ambiguity && shader_set.bindings[i].res_class != RES_CLASS_SRV) {
						set_heap_walkers.resources.advance(num_resource_descs);
					}

					// TODO: Batch to avoid multiple calls where possible (in any case, flush before setting root descriptor tables, or even batch that as well).
					device->CopyDescriptorsSimple(
							num_resource_descs,
							frame_heap_walkers.resources->get_curr_cpu_handle(),
							set_heap_walkers.resources.get_curr_cpu_handle(),
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					frame_heap_walkers.resources->advance(num_resource_descs);

					// If there is ambiguity and it didn't clarify as UAVs, skip them, which come later. [[SRV_UAV_AMBIGUITY]]
					if (srv_uav_ambiguity && shader_set.bindings[i].res_class != RES_CLASS_UAV) {
						set_heap_walkers.resources.advance(num_resource_descs);
					}

					resource_used = true;
				}
			}

			{
				const ShaderInfo::UniformBindingInfo::RootSignatureLocation &rs_loc_sampler = shader_set.bindings[i].root_sig_locations.sampler;
				if (rs_loc_sampler.root_param_idx != UINT32_MAX) { // Location used?
					DEV_ASSERT(num_sampler_descs);
					DEV_ASSERT(!srv_uav_ambiguity); // [[SRV_UAV_AMBIGUITY]]

					bool must_flush_table = tables.samplers && rs_loc_sampler.root_param_idx != tables.samplers->root_param_idx;
					if (must_flush_table) {
						// Check the root signature data has been filled ordered.
						DEV_ASSERT(rs_loc_sampler.root_param_idx > tables.samplers->root_param_idx);

						(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(tables.samplers->root_param_idx, tables.samplers->start_gpu_handle);
						tables.samplers = nullptr;
					}

					if (unlikely(frame_heap_walkers.samplers->get_free_handles() < num_sampler_descs)) {
						if (!frames[frame_idx].desc_heaps_exhausted_reported.samplers) {
							frames[frame_idx].desc_heaps_exhausted_reported.samplers = true;
							ERR_FAIL_MSG("Cannot bind uniform set because there's no enough room in current frame's SAMPLERS descriptors heap.\n"
										 "Please increase the value of the rendering/rendering_device/d3d12/max_sampler_descriptors_per_frame project setting.");
						} else {
							return;
						}
					}

					if (!tables.samplers) {
						DEV_ASSERT(last_bind->root_tables.samplers.size() < last_bind->root_tables.samplers.get_capacity());
						last_bind->root_tables.samplers.resize(last_bind->root_tables.samplers.size() + 1);
						tables.samplers = &last_bind->root_tables.samplers[last_bind->root_tables.samplers.size() - 1];
						tables.samplers->root_param_idx = rs_loc_sampler.root_param_idx;
						tables.samplers->start_gpu_handle = frame_heap_walkers.samplers->get_curr_gpu_handle();
					}

					// TODO: Batch to avoid multiple calls where possible (in any case, flush before setting root descriptor tables, or even batch that as well).
					device->CopyDescriptorsSimple(
							num_sampler_descs,
							frame_heap_walkers.samplers->get_curr_cpu_handle(),
							set_heap_walkers.samplers.get_curr_cpu_handle(),
							D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
					frame_heap_walkers.samplers->advance(num_sampler_descs);
				}
			}
		}

		// Uniform set descriptor heaps are always full (descriptors are created for every uniform in them) despite
		// the shader variant a given set is created upon may not need all of them due to DXC optimizations.
		// Therefore, at this point we have to advance through the descriptor set descriptor's heap unconditionally.

		set_heap_walkers.resources.advance(num_resource_descs);
		if (srv_uav_ambiguity) {
			DEV_ASSERT(num_resource_descs);
			if (!resource_used) {
				set_heap_walkers.resources.advance(num_resource_descs); // Additional skip, since both SRVs and UAVs have to be bypassed.
			}
		}

		set_heap_walkers.samplers.advance(num_sampler_descs);
	}

	DEV_ASSERT(set_heap_walkers.resources.is_at_eof());
	DEV_ASSERT(set_heap_walkers.samplers.is_at_eof());

	{
		bool must_flush_table = tables.resources;
		if (must_flush_table) {
			(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(tables.resources->root_param_idx, tables.resources->start_gpu_handle);
		}
	}
	{
		bool must_flush_table = tables.samplers;
		if (must_flush_table) {
			(cmd_buf_info->cmd_list.Get()->*set_root_desc_table_fn)(tables.samplers->root_param_idx, tables.samplers->start_gpu_handle);
		}
	}

	last_bind->root_signature_crc = root_sig_crc;
	last_bind->segment_serial = frames[frame_idx].segment_serial;
}

/******************/
/**** TRANSFER ****/
/******************/

void RenderingDeviceDriverD3D12::command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) {
	_command_check_descriptor_sets(p_cmd_buffer);

	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	BufferInfo *buf_info = (BufferInfo *)p_buffer.id;

	if (frames[frame_idx].desc_heap_walkers.resources.is_at_eof()) {
		if (!frames[frame_idx].desc_heaps_exhausted_reported.resources) {
			frames[frame_idx].desc_heaps_exhausted_reported.resources = true;
			ERR_FAIL_MSG(
					"Cannot clear buffer because there's no enough room in current frame's RESOURCE descriptors heap.\n"
					"Please increase the value of the rendering/rendering_device/d3d12/max_resource_descriptors_per_frame project setting.");
		} else {
			return;
		}
	}
	if (frames[frame_idx].desc_heap_walkers.aux.is_at_eof()) {
		if (!frames[frame_idx].desc_heaps_exhausted_reported.aux) {
			frames[frame_idx].desc_heaps_exhausted_reported.aux = true;
			ERR_FAIL_MSG(
					"Cannot clear buffer because there's no enough room in current frame's AUX descriptors heap.\n"
					"Please increase the value of the rendering/rendering_device/d3d12/max_misc_descriptors_per_frame project setting.");
		} else {
			return;
		}
	}

	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, buf_info, 0, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		_resource_transitions_flush(cmd_buf_info);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = (buf_info->size + 3) / 4;
	uav_desc.Buffer.StructureByteStride = 0;
	uav_desc.Buffer.CounterOffsetInBytes = 0;
	uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	device->CreateUnorderedAccessView(
			buf_info->resource,
			nullptr,
			&uav_desc,
			frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle());

	device->CopyDescriptorsSimple(
			1,
			frames[frame_idx].desc_heap_walkers.resources.get_curr_cpu_handle(),
			frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	static const UINT values[4] = {};
	cmd_buf_info->cmd_list->ClearUnorderedAccessViewUint(
			frames[frame_idx].desc_heap_walkers.resources.get_curr_gpu_handle(),
			frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle(),
			buf_info->resource,
			values,
			0,
			nullptr);

	frames[frame_idx].desc_heap_walkers.resources.advance();
	frames[frame_idx].desc_heap_walkers.aux.advance();
}

void RenderingDeviceDriverD3D12::command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_buf_locfer, VectorView<BufferCopyRegion> p_regions) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	BufferInfo *src_buf_info = (BufferInfo *)p_src_buffer.id;
	BufferInfo *buf_loc_info = (BufferInfo *)p_buf_locfer.id;

	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, src_buf_info, 0, 1, D3D12_RESOURCE_STATE_COPY_SOURCE);
		_resource_transition_batch(cmd_buf_info, buf_loc_info, 0, 1, D3D12_RESOURCE_STATE_COPY_DEST);
		_resource_transitions_flush(cmd_buf_info);
	}

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		cmd_buf_info->cmd_list->CopyBufferRegion(buf_loc_info->resource, p_regions[i].dst_offset, src_buf_info->resource, p_regions[i].src_offset, p_regions[i].size);
	}
}

void RenderingDeviceDriverD3D12::command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	TextureInfo *src_tex_info = (TextureInfo *)p_src_texture.id;
	TextureInfo *dst_tex_info = (TextureInfo *)p_dst_texture.id;

	if (!barrier_capabilities.enhanced_barriers_supported) {
		// Batch all barrier transitions for the textures before performing the copies.
		for (uint32_t i = 0; i < p_regions.size(); i++) {
			uint32_t layer_count = MIN(p_regions[i].src_subresources.layer_count, p_regions[i].dst_subresources.layer_count);
			for (uint32_t j = 0; j < layer_count; j++) {
				UINT src_subresource = _compute_subresource_from_layers(src_tex_info, p_regions[i].src_subresources, j);
				UINT dst_subresource = _compute_subresource_from_layers(dst_tex_info, p_regions[i].dst_subresources, j);
				_resource_transition_batch(cmd_buf_info, src_tex_info, src_subresource, 1, D3D12_RESOURCE_STATE_COPY_SOURCE);
				_resource_transition_batch(cmd_buf_info, dst_tex_info, dst_subresource, 1, D3D12_RESOURCE_STATE_COPY_DEST);
			}
		}

		_resource_transitions_flush(cmd_buf_info);
	}

	CD3DX12_BOX src_box;
	for (uint32_t i = 0; i < p_regions.size(); i++) {
		uint32_t layer_count = MIN(p_regions[i].src_subresources.layer_count, p_regions[i].dst_subresources.layer_count);
		for (uint32_t j = 0; j < layer_count; j++) {
			UINT src_subresource = _compute_subresource_from_layers(src_tex_info, p_regions[i].src_subresources, j);
			UINT dst_subresource = _compute_subresource_from_layers(dst_tex_info, p_regions[i].dst_subresources, j);
			CD3DX12_TEXTURE_COPY_LOCATION src_location(src_tex_info->resource, src_subresource);
			CD3DX12_TEXTURE_COPY_LOCATION dst_location(dst_tex_info->resource, dst_subresource);
			src_box.left = p_regions[i].src_offset.x;
			src_box.top = p_regions[i].src_offset.y;
			src_box.front = p_regions[i].src_offset.z;
			src_box.right = p_regions[i].src_offset.x + p_regions[i].size.x;
			src_box.bottom = p_regions[i].src_offset.y + p_regions[i].size.y;
			src_box.back = p_regions[i].src_offset.z + p_regions[i].size.z;
			cmd_buf_info->cmd_list->CopyTextureRegion(&dst_location, p_regions[i].dst_offset.x, p_regions[i].dst_offset.y, p_regions[i].dst_offset.z, &src_location, &src_box);
		}
	}
}

void RenderingDeviceDriverD3D12::command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	TextureInfo *src_tex_info = (TextureInfo *)p_src_texture.id;
	TextureInfo *dst_tex_info = (TextureInfo *)p_dst_texture.id;

	UINT src_subresource = D3D12CalcSubresource(p_src_mipmap, p_src_layer, 0, src_tex_info->desc.MipLevels, src_tex_info->desc.ArraySize());
	UINT dst_subresource = D3D12CalcSubresource(p_dst_mipmap, p_dst_layer, 0, dst_tex_info->desc.MipLevels, dst_tex_info->desc.ArraySize());
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, src_tex_info, src_subresource, 1, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		_resource_transition_batch(cmd_buf_info, dst_tex_info, dst_subresource, 1, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ResolveSubresource(dst_tex_info->resource, dst_subresource, src_tex_info->resource, src_subresource, RD_TO_D3D12_FORMAT[src_tex_info->format].general_format);
}

void RenderingDeviceDriverD3D12::command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	if (tex_info->main_texture) {
		tex_info = tex_info->main_texture;
	}

	auto _transition_subresources = [&](D3D12_RESOURCE_STATES p_new_state) {
		for (uint32_t i = 0; i < p_subresources.layer_count; i++) {
			for (uint32_t j = 0; j < p_subresources.mipmap_count; j++) {
				UINT subresource = D3D12CalcSubresource(
						p_subresources.base_mipmap + j,
						p_subresources.base_layer + i,
						0,
						tex_info->desc.MipLevels,
						tex_info->desc.ArraySize());
				_resource_transition_batch(cmd_buf_info, tex_info, subresource, 1, p_new_state);
			}
		}
		_resource_transitions_flush(cmd_buf_info);
	};

	if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
		// Clear via RTV.

		if (frames[frame_idx].desc_heap_walkers.rtv.get_free_handles() < p_subresources.mipmap_count) {
			if (!frames[frame_idx].desc_heaps_exhausted_reported.rtv) {
				frames[frame_idx].desc_heaps_exhausted_reported.rtv = true;
				ERR_FAIL_MSG(
						"Cannot clear texture because there's no enough room in current frame's RENDER TARGET descriptors heap.\n"
						"Please increase the value of the rendering/rendering_device/d3d12/max_misc_descriptors_per_frame project setting.");
			} else {
				return;
			}
		}

		if (!barrier_capabilities.enhanced_barriers_supported) {
			_transition_subresources(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		for (uint32_t i = 0; i < p_subresources.mipmap_count; i++) {
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = _make_rtv_for_texture(tex_info, p_subresources.base_mipmap + i, p_subresources.base_layer, p_subresources.layer_count, false);
			rtv_desc.Format = tex_info->view_descs.uav.Format;
			device->CreateRenderTargetView(
					tex_info->resource,
					&rtv_desc,
					frames[frame_idx].desc_heap_walkers.rtv.get_curr_cpu_handle());

			cmd_buf_info->cmd_list->ClearRenderTargetView(
					frames[frame_idx].desc_heap_walkers.rtv.get_curr_cpu_handle(),
					p_color.components,
					0,
					nullptr);

			frames[frame_idx].desc_heap_walkers.rtv.advance();
		}
	} else if (tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
		// Clear via UAV.
		_command_check_descriptor_sets(p_cmd_buffer);

		if (frames[frame_idx].desc_heap_walkers.resources.get_free_handles() < p_subresources.mipmap_count) {
			if (!frames[frame_idx].desc_heaps_exhausted_reported.resources) {
				frames[frame_idx].desc_heaps_exhausted_reported.resources = true;
				ERR_FAIL_MSG(
						"Cannot clear texture because there's no enough room in current frame's RESOURCE descriptors heap.\n"
						"Please increase the value of the rendering/rendering_device/d3d12/max_resource_descriptors_per_frame project setting.");
			} else {
				return;
			}
		}
		if (frames[frame_idx].desc_heap_walkers.aux.get_free_handles() < p_subresources.mipmap_count) {
			if (!frames[frame_idx].desc_heaps_exhausted_reported.aux) {
				frames[frame_idx].desc_heaps_exhausted_reported.aux = true;
				ERR_FAIL_MSG(
						"Cannot clear texture because there's no enough room in current frame's AUX descriptors heap.\n"
						"Please increase the value of the rendering/rendering_device/d3d12/max_misc_descriptors_per_frame project setting.");
			} else {
				return;
			}
		}

		if (!barrier_capabilities.enhanced_barriers_supported) {
			_transition_subresources(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		for (uint32_t i = 0; i < p_subresources.mipmap_count; i++) {
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = _make_ranged_uav_for_texture(tex_info, p_subresources.base_mipmap + i, p_subresources.base_layer, p_subresources.layer_count, false);
			device->CreateUnorderedAccessView(
					tex_info->resource,
					nullptr,
					&uav_desc,
					frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle());
			device->CopyDescriptorsSimple(
					1,
					frames[frame_idx].desc_heap_walkers.resources.get_curr_cpu_handle(),
					frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle(),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			UINT values[4] = {
				(UINT)p_color.get_r8(),
				(UINT)p_color.get_g8(),
				(UINT)p_color.get_b8(),
				(UINT)p_color.get_a8(),
			};

			cmd_buf_info->cmd_list->ClearUnorderedAccessViewUint(
					frames[frame_idx].desc_heap_walkers.resources.get_curr_gpu_handle(),
					frames[frame_idx].desc_heap_walkers.aux.get_curr_cpu_handle(),
					tex_info->resource,
					values,
					0,
					nullptr);

			frames[frame_idx].desc_heap_walkers.resources.advance();
			frames[frame_idx].desc_heap_walkers.aux.advance();
		}
	} else {
		ERR_FAIL_MSG("Cannot clear texture because its format does not support UAV writes. You'll need to update its contents through another method.");
	}
}

void RenderingDeviceDriverD3D12::command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	BufferInfo *buf_info = (BufferInfo *)p_src_buffer.id;
	TextureInfo *tex_info = (TextureInfo *)p_dst_texture.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, buf_info, 0, 1, D3D12_RESOURCE_STATE_COPY_SOURCE);
	}

	uint32_t pixel_size = get_image_format_pixel_size(tex_info->format);
	uint32_t block_w = 0, block_h = 0;
	get_compressed_image_format_block_dimensions(tex_info->format, block_w, block_h);

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		uint32_t region_pitch = (p_regions[i].texture_region_size.x * pixel_size * block_w) >> get_compressed_image_format_pixel_rshift(tex_info->format);
		region_pitch = STEPIFY(region_pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
		src_footprint.Offset = p_regions[i].buffer_offset;
		src_footprint.Footprint = CD3DX12_SUBRESOURCE_FOOTPRINT(
				RD_TO_D3D12_FORMAT[tex_info->format].family,
				STEPIFY(p_regions[i].texture_region_size.x, block_w),
				STEPIFY(p_regions[i].texture_region_size.y, block_h),
				p_regions[i].texture_region_size.z,
				region_pitch);
		CD3DX12_TEXTURE_COPY_LOCATION copy_src(buf_info->resource, src_footprint);

		CD3DX12_BOX src_box(
				0, 0, 0,
				STEPIFY(p_regions[i].texture_region_size.x, block_w),
				STEPIFY(p_regions[i].texture_region_size.y, block_h),
				p_regions[i].texture_region_size.z);

		if (!barrier_capabilities.enhanced_barriers_supported) {
			for (uint32_t j = 0; j < p_regions[i].texture_subresources.layer_count; j++) {
				UINT dst_subresource = D3D12CalcSubresource(
						p_regions[i].texture_subresources.mipmap,
						p_regions[i].texture_subresources.base_layer + j,
						_compute_plane_slice(tex_info->format, p_regions[i].texture_subresources.aspect),
						tex_info->desc.MipLevels,
						tex_info->desc.ArraySize());
				CD3DX12_TEXTURE_COPY_LOCATION copy_dst(tex_info->resource, dst_subresource);

				_resource_transition_batch(cmd_buf_info, tex_info, dst_subresource, 1, D3D12_RESOURCE_STATE_COPY_DEST);
			}

			_resource_transitions_flush(cmd_buf_info);
		}

		for (uint32_t j = 0; j < p_regions[i].texture_subresources.layer_count; j++) {
			UINT dst_subresource = D3D12CalcSubresource(
					p_regions[i].texture_subresources.mipmap,
					p_regions[i].texture_subresources.base_layer + j,
					_compute_plane_slice(tex_info->format, p_regions[i].texture_subresources.aspect),
					tex_info->desc.MipLevels,
					tex_info->desc.ArraySize());
			CD3DX12_TEXTURE_COPY_LOCATION copy_dst(tex_info->resource, dst_subresource);

			cmd_buf_info->cmd_list->CopyTextureRegion(
					&copy_dst,
					p_regions[i].texture_offset.x,
					p_regions[i].texture_offset.y,
					p_regions[i].texture_offset.z,
					&copy_src,
					&src_box);
		}
	}
}

void RenderingDeviceDriverD3D12::command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_buf_locfer, VectorView<BufferTextureCopyRegion> p_regions) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	TextureInfo *tex_info = (TextureInfo *)p_src_texture.id;
	BufferInfo *buf_info = (BufferInfo *)p_buf_locfer.id;

	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, buf_info, 0, 1, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	uint32_t block_w = 0, block_h = 0;
	get_compressed_image_format_block_dimensions(tex_info->format, block_w, block_h);

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		if (!barrier_capabilities.enhanced_barriers_supported) {
			for (uint32_t j = 0; j < p_regions[i].texture_subresources.layer_count; j++) {
				UINT src_subresource = D3D12CalcSubresource(
						p_regions[i].texture_subresources.mipmap,
						p_regions[i].texture_subresources.base_layer + j,
						_compute_plane_slice(tex_info->format, p_regions[i].texture_subresources.aspect),
						tex_info->desc.MipLevels,
						tex_info->desc.ArraySize());

				_resource_transition_batch(cmd_buf_info, tex_info, src_subresource, 1, D3D12_RESOURCE_STATE_COPY_SOURCE);
			}

			_resource_transitions_flush(cmd_buf_info);
		}

		for (uint32_t j = 0; j < p_regions[i].texture_subresources.layer_count; j++) {
			UINT src_subresource = D3D12CalcSubresource(
					p_regions[i].texture_subresources.mipmap,
					p_regions[i].texture_subresources.base_layer + j,
					_compute_plane_slice(tex_info->format, p_regions[i].texture_subresources.aspect),
					tex_info->desc.MipLevels,
					tex_info->desc.ArraySize());

			CD3DX12_TEXTURE_COPY_LOCATION copy_src(tex_info->resource, src_subresource);

			uint32_t computed_d = MAX(1, tex_info->desc.DepthOrArraySize >> p_regions[i].texture_subresources.mipmap);
			uint32_t image_size = get_image_format_required_size(
					tex_info->format,
					MAX(1u, tex_info->desc.Width >> p_regions[i].texture_subresources.mipmap),
					MAX(1u, tex_info->desc.Height >> p_regions[i].texture_subresources.mipmap),
					computed_d,
					1);
			uint32_t row_pitch = image_size / (p_regions[i].texture_region_size.y * computed_d) * block_h;
			row_pitch = STEPIFY(row_pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT dst_footprint = {};
			dst_footprint.Offset = p_regions[i].buffer_offset;
			dst_footprint.Footprint.Width = STEPIFY(p_regions[i].texture_region_size.x, block_w);
			dst_footprint.Footprint.Height = STEPIFY(p_regions[i].texture_region_size.y, block_h);
			dst_footprint.Footprint.Depth = p_regions[i].texture_region_size.z;
			dst_footprint.Footprint.RowPitch = row_pitch;
			dst_footprint.Footprint.Format = RD_TO_D3D12_FORMAT[tex_info->format].family;

			CD3DX12_TEXTURE_COPY_LOCATION copy_dst(buf_info->resource, dst_footprint);

			cmd_buf_info->cmd_list->CopyTextureRegion(&copy_dst, 0, 0, 0, &copy_src, nullptr);
		}
	}
}

/******************/
/**** PIPELINE ****/
/******************/

void RenderingDeviceDriverD3D12::pipeline_free(PipelineID p_pipeline) {
	PipelineInfo *pipeline_info = (PipelineInfo *)(p_pipeline.id);
	pipeline_info->pso->Release();
	memdelete(pipeline_info);
}

// ----- BINDING -----

void RenderingDeviceDriverD3D12::command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, VectorView<uint32_t> p_data) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;
	if (!shader_info_in->dxil_push_constant_size) {
		return;
	}
	if (shader_info_in->is_compute) {
		cmd_buf_info->cmd_list->SetComputeRoot32BitConstants(0, p_data.size(), p_data.ptr(), p_dst_first_index);
	} else {
		cmd_buf_info->cmd_list->SetGraphicsRoot32BitConstants(0, p_data.size(), p_data.ptr(), p_dst_first_index);
	}
}

// ----- CACHE -----

bool RenderingDeviceDriverD3D12::pipeline_cache_create(const Vector<uint8_t> &p_data) {
	WARN_PRINT("PSO caching is not implemented yet in the Direct3D 12 driver.");
	return false;
}

void RenderingDeviceDriverD3D12::pipeline_cache_free() {
	ERR_FAIL_MSG("Not implemented.");
}

size_t RenderingDeviceDriverD3D12::pipeline_cache_query_size() {
	ERR_FAIL_V_MSG(0, "Not implemented.");
}

Vector<uint8_t> RenderingDeviceDriverD3D12::pipeline_cache_serialize() {
	ERR_FAIL_V_MSG(Vector<uint8_t>(), "Not implemented.");
}

/*******************/
/**** RENDERING ****/
/*******************/

// ----- SUBPASS -----

RDD::RenderPassID RenderingDeviceDriverD3D12::render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) {
	// Pre-bookkeep.
	RenderPassInfo *pass_info = VersatileResource::allocate<RenderPassInfo>(resources_allocator);

	pass_info->attachments.resize(p_attachments.size());
	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		pass_info->attachments[i] = p_attachments[i];
	}

	pass_info->subpasses.resize(p_subpasses.size());
	for (uint32_t i = 0; i < p_subpasses.size(); i++) {
		pass_info->subpasses[i] = p_subpasses[i];
	}

	pass_info->view_count = p_view_count;

	DXGI_FORMAT *formats = ALLOCA_ARRAY(DXGI_FORMAT, p_attachments.size());
	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		const D3D12Format &format = RD_TO_D3D12_FORMAT[p_attachments[i].format];
		if (format.dsv_format != DXGI_FORMAT_UNKNOWN) {
			formats[i] = format.dsv_format;
		} else {
			formats[i] = format.general_format;
		}
	}
	pass_info->max_supported_sample_count = _find_max_common_supported_sample_count(VectorView(formats, p_attachments.size()));

	return RenderPassID(pass_info);
}

void RenderingDeviceDriverD3D12::render_pass_free(RenderPassID p_render_pass) {
	RenderPassInfo *pass_info = (RenderPassInfo *)p_render_pass.id;
	VersatileResource::free(resources_allocator, pass_info);
}

// ----- COMMANDS -----

void RenderingDeviceDriverD3D12::command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_attachment_clears) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	const RenderPassInfo *pass_info = (const RenderPassInfo *)p_render_pass.id;
	const FramebufferInfo *fb_info = (const FramebufferInfo *)p_framebuffer.id;

	DEV_ASSERT(cmd_buf_info->render_pass_state.current_subpass == UINT32_MAX);

	auto _transition_subresources = [&](TextureInfo *p_texture_info, D3D12_RESOURCE_STATES p_states) {
		uint32_t planes = 1;
		if ((p_texture_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
			planes = format_get_plane_count(p_texture_info->format);
		}
		for (uint32_t i = 0; i < p_texture_info->layers; i++) {
			for (uint32_t j = 0; j < p_texture_info->mipmaps; j++) {
				uint32_t subresource = D3D12CalcSubresource(
						p_texture_info->base_mip + j,
						p_texture_info->base_layer + i,
						0,
						p_texture_info->desc.MipLevels,
						p_texture_info->desc.ArraySize());

				_resource_transition_batch(cmd_buf_info, p_texture_info, subresource, planes, p_states);
			}
		}
	};

	if (fb_info->is_screen || !barrier_capabilities.enhanced_barriers_supported) {
		// Screen framebuffers must perform this transition even if enhanced barriers are supported.
		for (uint32_t i = 0; i < fb_info->attachments.size(); i++) {
			TextureInfo *tex_info = (TextureInfo *)fb_info->attachments[i].id;
			if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
				_transition_subresources(tex_info, D3D12_RESOURCE_STATE_RENDER_TARGET);
			} else if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
				_transition_subresources(tex_info, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			} else {
				DEV_ASSERT(false);
			}
		}
		if (fb_info->vrs_attachment) {
			TextureInfo *tex_info = (TextureInfo *)fb_info->vrs_attachment.id;
			_transition_subresources(tex_info, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
		}

		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->render_pass_state.region_rect = CD3DX12_RECT(
			p_rect.position.x,
			p_rect.position.y,
			p_rect.position.x + p_rect.size.x,
			p_rect.position.y + p_rect.size.y);
	cmd_buf_info->render_pass_state.region_is_all = !(
			cmd_buf_info->render_pass_state.region_rect.left == 0 &&
			cmd_buf_info->render_pass_state.region_rect.top == 0 &&
			cmd_buf_info->render_pass_state.region_rect.right == fb_info->size.x &&
			cmd_buf_info->render_pass_state.region_rect.bottom == fb_info->size.y);

	for (uint32_t i = 0; i < pass_info->attachments.size(); i++) {
		if (pass_info->attachments[i].load_op == ATTACHMENT_LOAD_OP_DONT_CARE) {
			const TextureInfo *tex_info = (const TextureInfo *)fb_info->attachments[i].id;
			_discard_texture_subresources(tex_info, cmd_buf_info);
		}
	}

	if (fb_info->vrs_attachment && vrs_capabilities.ss_image_supported) {
		ComPtr<ID3D12GraphicsCommandList5> cmd_list_5;
		cmd_buf_info->cmd_list->QueryInterface(cmd_list_5.GetAddressOf());
		if (cmd_list_5) {
			static const D3D12_SHADING_RATE_COMBINER COMBINERS[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = {
				D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
				D3D12_SHADING_RATE_COMBINER_OVERRIDE,
			};
			cmd_list_5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, COMBINERS);
		}
	}

	cmd_buf_info->render_pass_state.current_subpass = UINT32_MAX;
	cmd_buf_info->render_pass_state.fb_info = fb_info;
	cmd_buf_info->render_pass_state.pass_info = pass_info;
	command_next_render_subpass(p_cmd_buffer, p_cmd_buffer_type);

	AttachmentClear *clears = ALLOCA_ARRAY(AttachmentClear, pass_info->attachments.size());
	Rect2i *clear_rects = ALLOCA_ARRAY(Rect2i, pass_info->attachments.size());
	uint32_t num_clears = 0;

	for (uint32_t i = 0; i < pass_info->attachments.size(); i++) {
		TextureInfo *tex_info = (TextureInfo *)fb_info->attachments[i].id;
		if (!tex_info) {
			continue;
		}

		AttachmentClear clear;
		if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
			if (pass_info->attachments[i].load_op == ATTACHMENT_LOAD_OP_CLEAR) {
				clear.aspect.set_flag(TEXTURE_ASPECT_COLOR_BIT);
				clear.color_attachment = i;
				tex_info->pending_clear.remove_from_list();
			}
		} else if ((tex_info->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
			if (pass_info->attachments[i].stencil_load_op == ATTACHMENT_LOAD_OP_CLEAR) {
				clear.aspect.set_flag(TEXTURE_ASPECT_DEPTH_BIT);
			}
		}
		if (!clear.aspect.is_empty()) {
			clear.value = p_attachment_clears[i];
			clears[num_clears] = clear;
			clear_rects[num_clears] = p_rect;
			num_clears++;
		}
	}

	if (num_clears) {
		command_render_clear_attachments(p_cmd_buffer, VectorView(clears, num_clears), VectorView(clear_rects, num_clears));
	}
}

void RenderingDeviceDriverD3D12::_end_render_pass(CommandBufferID p_cmd_buffer) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;

	DEV_ASSERT(cmd_buf_info->render_pass_state.current_subpass != UINT32_MAX);

	const FramebufferInfo *fb_info = cmd_buf_info->render_pass_state.fb_info;
	const RenderPassInfo *pass_info = cmd_buf_info->render_pass_state.pass_info;
	const Subpass &subpass = pass_info->subpasses[cmd_buf_info->render_pass_state.current_subpass];

	if (fb_info->is_screen) {
		// Screen framebuffers must transition back to present state when the render pass is finished.
		for (uint32_t i = 0; i < fb_info->attachments.size(); i++) {
			TextureInfo *src_tex_info = (TextureInfo *)(fb_info->attachments[i].id);
			uint32_t src_subresource = D3D12CalcSubresource(src_tex_info->base_mip, src_tex_info->base_layer, 0, src_tex_info->desc.MipLevels, src_tex_info->desc.ArraySize());
			_resource_transition_batch(cmd_buf_info, src_tex_info, src_subresource, 1, D3D12_RESOURCE_STATE_PRESENT);
		}
	}

	struct Resolve {
		ID3D12Resource *src_res = nullptr;
		uint32_t src_subres = 0;
		ID3D12Resource *dst_res = nullptr;
		uint32_t dst_subres = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};
	Resolve *resolves = ALLOCA_ARRAY(Resolve, subpass.resolve_references.size());
	uint32_t num_resolves = 0;

	for (uint32_t i = 0; i < subpass.resolve_references.size(); i++) {
		uint32_t color_index = subpass.color_references[i].attachment;
		uint32_t resolve_index = subpass.resolve_references[i].attachment;
		DEV_ASSERT((color_index == AttachmentReference::UNUSED) == (resolve_index == AttachmentReference::UNUSED));
		if (color_index == AttachmentReference::UNUSED || !fb_info->attachments[color_index]) {
			continue;
		}

		TextureInfo *src_tex_info = (TextureInfo *)fb_info->attachments[color_index].id;
		uint32_t src_subresource = D3D12CalcSubresource(src_tex_info->base_mip, src_tex_info->base_layer, 0, src_tex_info->desc.MipLevels, src_tex_info->desc.ArraySize());
		_resource_transition_batch(cmd_buf_info, src_tex_info, src_subresource, 1, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

		TextureInfo *dst_tex_info = (TextureInfo *)fb_info->attachments[resolve_index].id;
		uint32_t dst_subresource = D3D12CalcSubresource(dst_tex_info->base_mip, dst_tex_info->base_layer, 0, dst_tex_info->desc.MipLevels, dst_tex_info->desc.ArraySize());
		_resource_transition_batch(cmd_buf_info, dst_tex_info, dst_subresource, 1, D3D12_RESOURCE_STATE_RESOLVE_DEST);

		resolves[num_resolves].src_res = src_tex_info->resource;
		resolves[num_resolves].src_subres = src_subresource;
		resolves[num_resolves].dst_res = dst_tex_info->resource;
		resolves[num_resolves].dst_subres = dst_subresource;
		resolves[num_resolves].format = RD_TO_D3D12_FORMAT[src_tex_info->format].general_format;
		num_resolves++;
	}

	_resource_transitions_flush(cmd_buf_info);

	for (uint32_t i = 0; i < num_resolves; i++) {
		cmd_buf_info->cmd_list->ResolveSubresource(resolves[i].dst_res, resolves[i].dst_subres, resolves[i].src_res, resolves[i].src_subres, resolves[i].format);
	}
}

void RenderingDeviceDriverD3D12::command_end_render_pass(CommandBufferID p_cmd_buffer) {
	_end_render_pass(p_cmd_buffer);

	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	DEV_ASSERT(cmd_buf_info->render_pass_state.current_subpass != UINT32_MAX);

	const FramebufferInfo *fb_info = cmd_buf_info->render_pass_state.fb_info;
	const RenderPassInfo *pass_info = cmd_buf_info->render_pass_state.pass_info;

	if (vrs_capabilities.ss_image_supported) {
		ComPtr<ID3D12GraphicsCommandList5> cmd_list_5;
		cmd_buf_info->cmd_list->QueryInterface(cmd_list_5.GetAddressOf());
		if (cmd_list_5) {
			cmd_list_5->RSSetShadingRateImage(nullptr);
		}
	}

	for (uint32_t i = 0; i < pass_info->attachments.size(); i++) {
		if (pass_info->attachments[i].store_op == ATTACHMENT_STORE_OP_DONT_CARE) {
			const TextureInfo *tex_info = (const TextureInfo *)fb_info->attachments[i].id;
			_discard_texture_subresources(tex_info, cmd_buf_info);
		}
	}

	cmd_buf_info->render_pass_state.current_subpass = UINT32_MAX;
}

void RenderingDeviceDriverD3D12::command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;

	if (cmd_buf_info->render_pass_state.current_subpass == UINT32_MAX) {
		cmd_buf_info->render_pass_state.current_subpass = 0;
	} else {
		_end_render_pass(p_cmd_buffer);
		cmd_buf_info->render_pass_state.current_subpass++;
	}

	const FramebufferInfo *fb_info = cmd_buf_info->render_pass_state.fb_info;
	const RenderPassInfo *pass_info = cmd_buf_info->render_pass_state.pass_info;
	const Subpass &subpass = pass_info->subpasses[cmd_buf_info->render_pass_state.current_subpass];

	D3D12_CPU_DESCRIPTOR_HANDLE *rtv_handles = ALLOCA_ARRAY(D3D12_CPU_DESCRIPTOR_HANDLE, subpass.color_references.size());
	DescriptorsHeap::Walker rtv_heap_walker = fb_info->rtv_heap.make_walker();
	for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
		uint32_t attachment = subpass.color_references[i].attachment;
		if (attachment == AttachmentReference::UNUSED) {
			if (!frames[frame_idx].null_rtv_handle.ptr) {
				// No null descriptor-handle created for this frame yet.

				if (frames[frame_idx].desc_heap_walkers.rtv.is_at_eof()) {
					if (!frames[frame_idx].desc_heaps_exhausted_reported.rtv) {
						frames[frame_idx].desc_heaps_exhausted_reported.rtv = true;
						ERR_FAIL_MSG("Cannot begin subpass because there's no enough room in current frame's RENDER TARGET descriptors heap.\n"
									 "Please increase the value of the rendering/rendering_device/d3d12/max_misc_descriptors_per_frame project setting.");
					} else {
						return;
					}
				}

				D3D12_RENDER_TARGET_VIEW_DESC rtv_desc_null = {};
				rtv_desc_null.Format = DXGI_FORMAT_R8_UINT;
				rtv_desc_null.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				frames[frame_idx].null_rtv_handle = frames[frame_idx].desc_heap_walkers.rtv.get_curr_cpu_handle();
				device->CreateRenderTargetView(nullptr, &rtv_desc_null, frames[frame_idx].null_rtv_handle);
				frames[frame_idx].desc_heap_walkers.rtv.advance();
			}
			rtv_handles[i] = frames[frame_idx].null_rtv_handle;
		} else {
			uint32_t rt_index = fb_info->attachments_handle_inds[attachment];
			rtv_heap_walker.rewind();
			rtv_heap_walker.advance(rt_index);
			rtv_handles[i] = rtv_heap_walker.get_curr_cpu_handle();
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
	{
		DescriptorsHeap::Walker dsv_heap_walker = fb_info->dsv_heap.make_walker();
		if (subpass.depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
			uint32_t ds_index = fb_info->attachments_handle_inds[subpass.depth_stencil_reference.attachment];
			dsv_heap_walker.rewind();
			dsv_heap_walker.advance(ds_index);
			dsv_handle = dsv_heap_walker.get_curr_cpu_handle();
		}
	}

	cmd_buf_info->cmd_list->OMSetRenderTargets(subpass.color_references.size(), rtv_handles, false, dsv_handle.ptr ? &dsv_handle : nullptr);
}

void RenderingDeviceDriverD3D12::command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;

	D3D12_VIEWPORT *d3d12_viewports = ALLOCA_ARRAY(D3D12_VIEWPORT, p_viewports.size());
	for (uint32_t i = 0; i < p_viewports.size(); i++) {
		d3d12_viewports[i] = CD3DX12_VIEWPORT(
				p_viewports[i].position.x,
				p_viewports[i].position.y,
				p_viewports[i].size.x,
				p_viewports[i].size.y);
	}

	cmd_buf_info->cmd_list->RSSetViewports(p_viewports.size(), d3d12_viewports);
}

void RenderingDeviceDriverD3D12::command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;

	D3D12_RECT *d3d12_scissors = ALLOCA_ARRAY(D3D12_RECT, p_scissors.size());
	for (uint32_t i = 0; i < p_scissors.size(); i++) {
		d3d12_scissors[i] = CD3DX12_RECT(
				p_scissors[i].position.x,
				p_scissors[i].position.y,
				p_scissors[i].position.x + p_scissors[i].size.x,
				p_scissors[i].position.y + p_scissors[i].size.y);
	}

	cmd_buf_info->cmd_list->RSSetScissorRects(p_scissors.size(), d3d12_scissors);
}

void RenderingDeviceDriverD3D12::command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;

	DEV_ASSERT(cmd_buf_info->render_pass_state.current_subpass != UINT32_MAX);
	const FramebufferInfo *fb_info = cmd_buf_info->render_pass_state.fb_info;
	const RenderPassInfo *pass_info = cmd_buf_info->render_pass_state.pass_info;

	DescriptorsHeap::Walker rtv_heap_walker = fb_info->rtv_heap.make_walker();
	DescriptorsHeap::Walker dsv_heap_walker = fb_info->dsv_heap.make_walker();

	for (uint32_t i = 0; i < p_attachment_clears.size(); i++) {
		uint32_t attachment = UINT32_MAX;
		bool is_render_target = false;
		if (p_attachment_clears[i].aspect.has_flag(TEXTURE_ASPECT_COLOR_BIT)) {
			attachment = p_attachment_clears[i].color_attachment;
			is_render_target = true;
		} else {
			attachment = pass_info->subpasses[cmd_buf_info->render_pass_state.current_subpass].depth_stencil_reference.attachment;
		}

		for (uint32_t j = 0; j < p_rects.size(); j++) {
			D3D12_RECT rect = CD3DX12_RECT(
					p_rects[j].position.x,
					p_rects[j].position.y,
					p_rects[j].position.x + p_rects[j].size.x,
					p_rects[j].position.y + p_rects[j].size.y);
			const D3D12_RECT *rect_ptr = cmd_buf_info->render_pass_state.region_is_all ? nullptr : &rect;

			if (is_render_target) {
				uint32_t color_idx = fb_info->attachments_handle_inds[attachment];
				rtv_heap_walker.rewind();
				rtv_heap_walker.advance(color_idx);
				cmd_buf_info->cmd_list->ClearRenderTargetView(
						rtv_heap_walker.get_curr_cpu_handle(),
						p_attachment_clears[i].value.color.components,
						rect_ptr ? 1 : 0,
						rect_ptr);
			} else {
				uint32_t depth_stencil_idx = fb_info->attachments_handle_inds[attachment];
				dsv_heap_walker.rewind();
				dsv_heap_walker.advance(depth_stencil_idx);
				D3D12_CLEAR_FLAGS flags = {};
				if (p_attachment_clears[i].aspect.has_flag(TEXTURE_ASPECT_DEPTH_BIT)) {
					flags |= D3D12_CLEAR_FLAG_DEPTH;
				}
				if (p_attachment_clears[i].aspect.has_flag(TEXTURE_ASPECT_STENCIL_BIT)) {
					flags |= D3D12_CLEAR_FLAG_STENCIL;
				}
				cmd_buf_info->cmd_list->ClearDepthStencilView(
						dsv_heap_walker.get_curr_cpu_handle(),
						flags,
						p_attachment_clears[i].value.depth,
						p_attachment_clears[i].value.stencil,
						rect_ptr ? 1 : 0,
						rect_ptr);
			}
		}
	}
}

void RenderingDeviceDriverD3D12::command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	const PipelineInfo *pipeline_info = (const PipelineInfo *)p_pipeline.id;

	if (cmd_buf_info->graphics_pso == pipeline_info->pso) {
		return;
	}

	const ShaderInfo *shader_info_in = pipeline_info->shader_info;
	const RenderPipelineInfo &render_info = pipeline_info->render_info;

	cmd_buf_info->cmd_list->SetPipelineState(pipeline_info->pso);
	if (cmd_buf_info->graphics_root_signature_crc != shader_info_in->root_signature_crc) {
		cmd_buf_info->cmd_list->SetGraphicsRootSignature(shader_info_in->root_signature.Get());
		cmd_buf_info->graphics_root_signature_crc = shader_info_in->root_signature_crc;
	}

	cmd_buf_info->cmd_list->IASetPrimitiveTopology(render_info.dyn_params.primitive_topology);
	cmd_buf_info->cmd_list->OMSetBlendFactor(render_info.dyn_params.blend_constant.components);
	cmd_buf_info->cmd_list->OMSetStencilRef(render_info.dyn_params.stencil_reference);

	if (misc_features_support.depth_bounds_supported) {
		ComPtr<ID3D12GraphicsCommandList1> command_list_1;
		cmd_buf_info->cmd_list->QueryInterface(command_list_1.GetAddressOf());
		if (command_list_1) {
			command_list_1->OMSetDepthBounds(render_info.dyn_params.depth_bounds_min, render_info.dyn_params.depth_bounds_max);
		}
	}

	cmd_buf_info->render_pass_state.vf_info = render_info.vf_info;

	cmd_buf_info->graphics_pso = pipeline_info->pso;
	cmd_buf_info->compute_pso = nullptr;
}

void RenderingDeviceDriverD3D12::command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	_command_bind_uniform_set(p_cmd_buffer, p_uniform_set, p_shader, p_set_index, false);
}

void RenderingDeviceDriverD3D12::command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	cmd_buf_info->cmd_list->DrawInstanced(p_vertex_count, p_instance_count, p_base_vertex, p_first_instance);
}

void RenderingDeviceDriverD3D12::command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	cmd_buf_info->cmd_list->DrawIndexedInstanced(p_index_count, p_instance_count, p_first_index, p_vertex_offset, p_first_instance);
}

void RenderingDeviceDriverD3D12::command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	BufferInfo *indirect_buf_info = (BufferInfo *)p_indirect_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, indirect_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ExecuteIndirect(indirect_cmd_signatures.draw_indexed.Get(), p_draw_count, indirect_buf_info->resource, p_offset, nullptr, 0);
}

void RenderingDeviceDriverD3D12::command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	BufferInfo *indirect_buf_info = (BufferInfo *)p_indirect_buffer.id;
	BufferInfo *count_buf_info = (BufferInfo *)p_count_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, indirect_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transition_batch(cmd_buf_info, count_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ExecuteIndirect(indirect_cmd_signatures.draw_indexed.Get(), p_max_draw_count, indirect_buf_info->resource, p_offset, count_buf_info->resource, p_count_buffer_offset);
}

void RenderingDeviceDriverD3D12::command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	BufferInfo *indirect_buf_info = (BufferInfo *)p_indirect_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, indirect_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ExecuteIndirect(indirect_cmd_signatures.draw.Get(), p_draw_count, indirect_buf_info->resource, p_offset, nullptr, 0);
}

void RenderingDeviceDriverD3D12::command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	_bind_vertex_buffers(cmd_buf_info);
	BufferInfo *indirect_buf_info = (BufferInfo *)p_indirect_buffer.id;
	BufferInfo *count_buf_info = (BufferInfo *)p_count_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, indirect_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transition_batch(cmd_buf_info, count_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ExecuteIndirect(indirect_cmd_signatures.draw.Get(), p_max_draw_count, indirect_buf_info->resource, p_offset, count_buf_info->resource, p_count_buffer_offset);
}

void RenderingDeviceDriverD3D12::command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;

	DEV_ASSERT(cmd_buf_info->render_pass_state.current_subpass != UINT32_MAX);

	// Vertex buffer views are set deferredly, to be sure we already know the strides by then,
	// which is only true once the pipeline has been bound. Otherwise, we'd need that the pipeline
	// is always bound first, which would be not kind of us. [[DEFERRED_VERTEX_BUFFERS]]
	DEV_ASSERT(p_binding_count <= ARRAY_SIZE(cmd_buf_info->render_pass_state.vertex_buffer_views));
	for (uint32_t i = 0; i < p_binding_count; i++) {
		BufferInfo *buffer_info = (BufferInfo *)p_buffers[i].id;

		cmd_buf_info->render_pass_state.vertex_buffer_views[i] = {};
		cmd_buf_info->render_pass_state.vertex_buffer_views[i].BufferLocation = buffer_info->resource->GetGPUVirtualAddress() + p_offsets[i];
		cmd_buf_info->render_pass_state.vertex_buffer_views[i].SizeInBytes = buffer_info->size - p_offsets[i];
		if (!barrier_capabilities.enhanced_barriers_supported) {
			_resource_transition_batch(cmd_buf_info, buffer_info, 0, 1, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}
	}

	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->render_pass_state.vertex_buffer_count = p_binding_count;
}

void RenderingDeviceDriverD3D12::command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	BufferInfo *buffer_info = (BufferInfo *)p_buffer.id;

	D3D12_INDEX_BUFFER_VIEW d3d12_ib_view = {};
	d3d12_ib_view.BufferLocation = buffer_info->resource->GetGPUVirtualAddress() + p_offset;
	d3d12_ib_view.SizeInBytes = buffer_info->size - p_offset;
	d3d12_ib_view.Format = p_format == INDEX_BUFFER_FORMAT_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, buffer_info, 0, 1, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->IASetIndexBuffer(&d3d12_ib_view);
}

// [[DEFERRED_VERTEX_BUFFERS]]
void RenderingDeviceDriverD3D12::_bind_vertex_buffers(CommandBufferInfo *p_cmd_buf_info) {
	RenderPassState &render_pass_state = p_cmd_buf_info->render_pass_state;
	if (render_pass_state.vertex_buffer_count && render_pass_state.vf_info) {
		for (uint32_t i = 0; i < render_pass_state.vertex_buffer_count; i++) {
			render_pass_state.vertex_buffer_views[i].StrideInBytes = render_pass_state.vf_info->vertex_buffer_strides[i];
		}
		p_cmd_buf_info->cmd_list->IASetVertexBuffers(0, render_pass_state.vertex_buffer_count, render_pass_state.vertex_buffer_views);
		render_pass_state.vertex_buffer_count = 0;
	}
}

void RenderingDeviceDriverD3D12::command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	cmd_buf_info->cmd_list->OMSetBlendFactor(p_constants.components);
}

void RenderingDeviceDriverD3D12::command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) {
	if (!Math::is_equal_approx(p_width, 1.0f)) {
		ERR_FAIL_MSG("Setting line widths other than 1.0 is not supported by the Direct3D 12 rendering driver.");
	}
}

// ----- PIPELINE -----

static const D3D12_PRIMITIVE_TOPOLOGY_TYPE RD_PRIMITIVE_TO_D3D12_TOPOLOGY_TYPE[RDD::RENDER_PRIMITIVE_MAX] = {
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
};

static const D3D12_PRIMITIVE_TOPOLOGY RD_PRIMITIVE_TO_D3D12_TOPOLOGY[RDD::RENDER_PRIMITIVE_MAX] = {
	D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
	D3D_PRIMITIVE_TOPOLOGY_LINELIST,
	D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
	D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
	D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
	D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
};

static const D3D12_CULL_MODE RD_POLYGON_CULL_TO_D3D12_CULL_MODE[RDD::POLYGON_CULL_MAX] = {
	D3D12_CULL_MODE_NONE,
	D3D12_CULL_MODE_FRONT,
	D3D12_CULL_MODE_BACK,
};

static const D3D12_STENCIL_OP RD_TO_D3D12_STENCIL_OP[RDD::STENCIL_OP_MAX] = {
	D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_ZERO,
	D3D12_STENCIL_OP_REPLACE,
	D3D12_STENCIL_OP_INCR_SAT,
	D3D12_STENCIL_OP_DECR_SAT,
	D3D12_STENCIL_OP_INVERT,
	D3D12_STENCIL_OP_INCR,
	D3D12_STENCIL_OP_DECR,
};

static const D3D12_LOGIC_OP RD_TO_D3D12_LOGIC_OP[RDD::LOGIC_OP_MAX] = {
	D3D12_LOGIC_OP_CLEAR,
	D3D12_LOGIC_OP_AND,
	D3D12_LOGIC_OP_AND_REVERSE,
	D3D12_LOGIC_OP_COPY,
	D3D12_LOGIC_OP_AND_INVERTED,
	D3D12_LOGIC_OP_NOOP,
	D3D12_LOGIC_OP_XOR,
	D3D12_LOGIC_OP_OR,
	D3D12_LOGIC_OP_NOR,
	D3D12_LOGIC_OP_EQUIV,
	D3D12_LOGIC_OP_INVERT,
	D3D12_LOGIC_OP_OR_REVERSE,
	D3D12_LOGIC_OP_COPY_INVERTED,
	D3D12_LOGIC_OP_OR_INVERTED,
	D3D12_LOGIC_OP_NAND,
	D3D12_LOGIC_OP_SET,
};

static const D3D12_BLEND RD_TO_D3D12_BLEND_FACTOR[RDD::BLEND_FACTOR_MAX] = {
	D3D12_BLEND_ZERO,
	D3D12_BLEND_ONE,
	D3D12_BLEND_SRC_COLOR,
	D3D12_BLEND_INV_SRC_COLOR,
	D3D12_BLEND_DEST_COLOR,
	D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA,
	D3D12_BLEND_INV_SRC_ALPHA,
	D3D12_BLEND_DEST_ALPHA,
	D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
	D3D12_BLEND_SRC_ALPHA_SAT,
	D3D12_BLEND_SRC1_COLOR,
	D3D12_BLEND_INV_SRC1_COLOR,
	D3D12_BLEND_SRC1_ALPHA,
	D3D12_BLEND_INV_SRC1_ALPHA,
};

static const D3D12_BLEND_OP RD_TO_D3D12_BLEND_OP[RDD::BLEND_OP_MAX] = {
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

RDD::PipelineID RenderingDeviceDriverD3D12::render_pipeline_create(
		ShaderID p_shader,
		VertexFormatID p_vertex_format,
		RenderPrimitive p_render_primitive,
		PipelineRasterizationState p_rasterization_state,
		PipelineMultisampleState p_multisample_state,
		PipelineDepthStencilState p_depth_stencil_state,
		PipelineColorBlendState p_blend_state,
		VectorView<int32_t> p_color_attachments,
		BitField<PipelineDynamicStateFlags> p_dynamic_state,
		RenderPassID p_render_pass,
		uint32_t p_render_subpass,
		VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;

	CD3DX12_PIPELINE_STATE_STREAM pipeline_desc = {};

	const RenderPassInfo *pass_info = (const RenderPassInfo *)p_render_pass.id;
	RenderPipelineInfo render_info;

	// Attachments.
	LocalVector<uint32_t> color_attachments;
	{
		const Subpass &subpass = pass_info->subpasses[p_render_subpass];

		for (uint32_t i = 0; i < ARRAY_SIZE((&pipeline_desc.RTVFormats)->RTFormats); i++) {
			(&pipeline_desc.RTVFormats)->RTFormats[i] = DXGI_FORMAT_UNKNOWN;
		}

		for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
			const AttachmentReference &ref = subpass.color_references[i];
			if (ref.attachment != AttachmentReference::UNUSED) {
				const Attachment &attachment = pass_info->attachments[ref.attachment];
				DEV_ASSERT((&pipeline_desc.RTVFormats)->RTFormats[i] == DXGI_FORMAT_UNKNOWN);
				(&pipeline_desc.RTVFormats)->RTFormats[i] = RD_TO_D3D12_FORMAT[attachment.format].general_format;
			}
		}
		(&pipeline_desc.RTVFormats)->NumRenderTargets = p_color_attachments.size();

		if (subpass.depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
			const Attachment &attachment = pass_info->attachments[subpass.depth_stencil_reference.attachment];
			pipeline_desc.DSVFormat = RD_TO_D3D12_FORMAT[attachment.format].dsv_format;
		} else {
			pipeline_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}
	}

	// Vertex.
	if (p_vertex_format) {
		const VertexFormatInfo *vf_info = (const VertexFormatInfo *)p_vertex_format.id;
		(&pipeline_desc.InputLayout)->pInputElementDescs = vf_info->input_elem_descs.ptr();
		(&pipeline_desc.InputLayout)->NumElements = vf_info->input_elem_descs.size();
		render_info.vf_info = vf_info;
	}

	// Input assembly & tessellation.

	pipeline_desc.PrimitiveTopologyType = RD_PRIMITIVE_TO_D3D12_TOPOLOGY_TYPE[p_render_primitive];
	if (p_render_primitive == RENDER_PRIMITIVE_TESSELATION_PATCH) {
		// Is there any way to get the true point count limit?
		ERR_FAIL_COND_V(p_rasterization_state.patch_control_points < 1 || p_rasterization_state.patch_control_points > 32, PipelineID());
		render_info.dyn_params.primitive_topology = (D3D12_PRIMITIVE_TOPOLOGY)((int)D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + p_rasterization_state.patch_control_points);
	} else {
		render_info.dyn_params.primitive_topology = RD_PRIMITIVE_TO_D3D12_TOPOLOGY[p_render_primitive];
	}
	if (p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX) {
		// TODO: This is right for 16-bit indices; for 32-bit there's a different enum value to set, but we don't know at this point.
		pipeline_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
	} else {
		pipeline_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	}

	// Rasterization.
	(&pipeline_desc.RasterizerState)->DepthClipEnable = !p_rasterization_state.enable_depth_clamp;
	// In D3D12, discard can be supported with some extra effort (empty pixel shader + disable depth/stencil test); that said, unsupported by now.
	ERR_FAIL_COND_V(p_rasterization_state.discard_primitives, PipelineID());
	(&pipeline_desc.RasterizerState)->FillMode = p_rasterization_state.wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	(&pipeline_desc.RasterizerState)->CullMode = RD_POLYGON_CULL_TO_D3D12_CULL_MODE[p_rasterization_state.cull_mode];
	(&pipeline_desc.RasterizerState)->FrontCounterClockwise = p_rasterization_state.front_face == POLYGON_FRONT_FACE_COUNTER_CLOCKWISE;
	// In D3D12, there's still a point in setting up depth bias with no depth buffer, but just zeroing (disabling) it all in such case is closer to Vulkan.
	if (p_rasterization_state.depth_bias_enabled && pipeline_desc.DSVFormat != DXGI_FORMAT_UNKNOWN) {
		(&pipeline_desc.RasterizerState)->DepthBias = p_rasterization_state.depth_bias_constant_factor;
		(&pipeline_desc.RasterizerState)->DepthBiasClamp = p_rasterization_state.depth_bias_clamp;
		(&pipeline_desc.RasterizerState)->SlopeScaledDepthBias = p_rasterization_state.depth_bias_slope_factor;
		(&pipeline_desc.RasterizerState)->DepthBias = 0;
		(&pipeline_desc.RasterizerState)->DepthBiasClamp = 0.0f;
		(&pipeline_desc.RasterizerState)->SlopeScaledDepthBias = 0.0f;
	}
	(&pipeline_desc.RasterizerState)->ForcedSampleCount = 0;
	(&pipeline_desc.RasterizerState)->ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	(&pipeline_desc.RasterizerState)->MultisampleEnable = TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count] != 1;
	(&pipeline_desc.RasterizerState)->AntialiasedLineEnable = true;

	// In D3D12, there's no line width.
	ERR_FAIL_COND_V(!Math::is_equal_approx(p_rasterization_state.line_width, 1.0f), PipelineID());

	// Multisample.
	ERR_FAIL_COND_V(p_multisample_state.enable_sample_shading, PipelineID()); // How one enables this in D3D12?
	if ((&pipeline_desc.RTVFormats)->NumRenderTargets || pipeline_desc.DSVFormat != DXGI_FORMAT_UNKNOWN) {
		uint32_t sample_count = MIN(
				pass_info->max_supported_sample_count,
				TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count]);
		(&pipeline_desc.SampleDesc)->Count = sample_count;
	} else {
		(&pipeline_desc.SampleDesc)->Count = 1;
	}
	if ((&pipeline_desc.SampleDesc)->Count > 1) {
		(&pipeline_desc.SampleDesc)->Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
	} else {
		(&pipeline_desc.SampleDesc)->Quality = 0;
	}
	if (p_multisample_state.sample_mask.size()) {
		for (int i = 1; i < p_multisample_state.sample_mask.size(); i++) {
			// In D3D12 there's a single sample mask for every pixel.
			ERR_FAIL_COND_V(p_multisample_state.sample_mask[i] != p_multisample_state.sample_mask[0], PipelineID());
		}
		pipeline_desc.SampleMask = p_multisample_state.sample_mask[0];
	} else {
		pipeline_desc.SampleMask = 0xffffffff;
	}

	// Depth stencil.

	if (pipeline_desc.DSVFormat == DXGI_FORMAT_UNKNOWN) {
		(&pipeline_desc.DepthStencilState)->DepthEnable = false;
		(&pipeline_desc.DepthStencilState)->StencilEnable = false;
	} else {
		(&pipeline_desc.DepthStencilState)->DepthEnable = p_depth_stencil_state.enable_depth_test;
		(&pipeline_desc.DepthStencilState)->DepthWriteMask = p_depth_stencil_state.enable_depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		(&pipeline_desc.DepthStencilState)->DepthFunc = RD_TO_D3D12_COMPARE_OP[p_depth_stencil_state.depth_compare_operator];
		(&pipeline_desc.DepthStencilState)->DepthBoundsTestEnable = p_depth_stencil_state.enable_depth_range;
		(&pipeline_desc.DepthStencilState)->StencilEnable = p_depth_stencil_state.enable_stencil;

		// In D3D12 some elements can't be different across front and back.
		ERR_FAIL_COND_V(p_depth_stencil_state.front_op.compare_mask != p_depth_stencil_state.back_op.compare_mask, PipelineID());
		ERR_FAIL_COND_V(p_depth_stencil_state.front_op.write_mask != p_depth_stencil_state.back_op.write_mask, PipelineID());
		ERR_FAIL_COND_V(p_depth_stencil_state.front_op.reference != p_depth_stencil_state.back_op.reference, PipelineID());
		(&pipeline_desc.DepthStencilState)->StencilReadMask = p_depth_stencil_state.front_op.compare_mask;
		(&pipeline_desc.DepthStencilState)->StencilWriteMask = p_depth_stencil_state.front_op.write_mask;

		(&pipeline_desc.DepthStencilState)->FrontFace.StencilFailOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.front_op.fail];
		(&pipeline_desc.DepthStencilState)->FrontFace.StencilPassOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.front_op.pass];
		(&pipeline_desc.DepthStencilState)->FrontFace.StencilDepthFailOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.front_op.depth_fail];
		(&pipeline_desc.DepthStencilState)->FrontFace.StencilFunc = RD_TO_D3D12_COMPARE_OP[p_depth_stencil_state.front_op.compare];

		(&pipeline_desc.DepthStencilState)->BackFace.StencilFailOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.back_op.fail];
		(&pipeline_desc.DepthStencilState)->BackFace.StencilPassOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.back_op.pass];
		(&pipeline_desc.DepthStencilState)->BackFace.StencilDepthFailOp = RD_TO_D3D12_STENCIL_OP[p_depth_stencil_state.back_op.depth_fail];
		(&pipeline_desc.DepthStencilState)->BackFace.StencilFunc = RD_TO_D3D12_COMPARE_OP[p_depth_stencil_state.back_op.compare];

		if (misc_features_support.depth_bounds_supported) {
			render_info.dyn_params.depth_bounds_min = p_depth_stencil_state.enable_depth_range ? p_depth_stencil_state.depth_range_min : 0.0f;
			render_info.dyn_params.depth_bounds_max = p_depth_stencil_state.enable_depth_range ? p_depth_stencil_state.depth_range_max : 1.0f;
		} else {
			if (p_depth_stencil_state.enable_depth_range) {
				WARN_PRINT_ONCE("Depth bounds test is not supported by the GPU driver.");
			}
		}

		render_info.dyn_params.stencil_reference = p_depth_stencil_state.front_op.reference;
	}

	// Blend states.
	(&pipeline_desc.BlendState)->AlphaToCoverageEnable = p_multisample_state.enable_alpha_to_coverage;
	{
		bool all_attachments_same_blend = true;
		for (int i = 0; i < p_blend_state.attachments.size(); i++) {
			const PipelineColorBlendState::Attachment &bs = p_blend_state.attachments[i];
			D3D12_RENDER_TARGET_BLEND_DESC &bd = (&pipeline_desc.BlendState)->RenderTarget[i];

			bd.BlendEnable = bs.enable_blend;
			bd.LogicOpEnable = p_blend_state.enable_logic_op;
			bd.LogicOp = RD_TO_D3D12_LOGIC_OP[p_blend_state.logic_op];

			bd.SrcBlend = RD_TO_D3D12_BLEND_FACTOR[bs.src_color_blend_factor];
			bd.DestBlend = RD_TO_D3D12_BLEND_FACTOR[bs.dst_color_blend_factor];
			bd.BlendOp = RD_TO_D3D12_BLEND_OP[bs.color_blend_op];

			bd.SrcBlendAlpha = RD_TO_D3D12_BLEND_FACTOR[bs.src_alpha_blend_factor];
			bd.DestBlendAlpha = RD_TO_D3D12_BLEND_FACTOR[bs.dst_alpha_blend_factor];
			bd.BlendOpAlpha = RD_TO_D3D12_BLEND_OP[bs.alpha_blend_op];

			if (bs.write_r) {
				bd.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED;
			}
			if (bs.write_g) {
				bd.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
			}
			if (bs.write_b) {
				bd.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
			}
			if (bs.write_a) {
				bd.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
			}

			if (i > 0 && all_attachments_same_blend) {
				all_attachments_same_blend = &(&pipeline_desc.BlendState)->RenderTarget[i] == &(&pipeline_desc.BlendState)->RenderTarget[0];
			}
		}

		// Per D3D12 docs, if logic op used, independent blending is not supported.
		ERR_FAIL_COND_V(p_blend_state.enable_logic_op && !all_attachments_same_blend, PipelineID());

		(&pipeline_desc.BlendState)->IndependentBlendEnable = !all_attachments_same_blend;
	}

	render_info.dyn_params.blend_constant = p_blend_state.blend_constant;

	// Stages bytecodes + specialization constants.

	pipeline_desc.pRootSignature = shader_info_in->root_signature.Get();

	HashMap<ShaderStage, Vector<uint8_t>> final_stages_bytecode;
	bool ok = _shader_apply_specialization_constants(shader_info_in, p_specialization_constants, final_stages_bytecode);
	ERR_FAIL_COND_V(!ok, PipelineID());

	pipeline_desc.VS = D3D12_SHADER_BYTECODE{
		final_stages_bytecode[SHADER_STAGE_VERTEX].ptr(),
		(SIZE_T)final_stages_bytecode[SHADER_STAGE_VERTEX].size()
	};
	pipeline_desc.PS = D3D12_SHADER_BYTECODE{
		final_stages_bytecode[SHADER_STAGE_FRAGMENT].ptr(),
		(SIZE_T)final_stages_bytecode[SHADER_STAGE_FRAGMENT].size()
	};

	ComPtr<ID3D12Device2> device_2;
	device->QueryInterface(device_2.GetAddressOf());
	ID3D12PipelineState *pso = nullptr;
	HRESULT res = E_FAIL;
	if (device_2) {
		D3D12_PIPELINE_STATE_STREAM_DESC pssd = {};
		pssd.pPipelineStateSubobjectStream = &pipeline_desc;
		pssd.SizeInBytes = sizeof(pipeline_desc);
		res = device_2->CreatePipelineState(&pssd, IID_PPV_ARGS(&pso));
	} else {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = pipeline_desc.GraphicsDescV0();
		res = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	}
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), PipelineID(), "Create(Graphics)PipelineState failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	PipelineInfo *pipeline_info = memnew(PipelineInfo);
	pipeline_info->pso = pso;
	pipeline_info->shader_info = shader_info_in;
	pipeline_info->render_info = render_info;

	return PipelineID(pipeline_info);
}

/*****************/
/**** COMPUTE ****/
/*****************/

// ----- COMMANDS -----

void RenderingDeviceDriverD3D12::command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	const PipelineInfo *pipeline_info = (const PipelineInfo *)p_pipeline.id;

	if (cmd_buf_info->compute_pso == pipeline_info->pso) {
		return;
	}

	const ShaderInfo *shader_info_in = pipeline_info->shader_info;
	cmd_buf_info->cmd_list->SetPipelineState(pipeline_info->pso);
	if (cmd_buf_info->compute_root_signature_crc != shader_info_in->root_signature_crc) {
		cmd_buf_info->cmd_list->SetComputeRootSignature(shader_info_in->root_signature.Get());
		cmd_buf_info->compute_root_signature_crc = shader_info_in->root_signature_crc;
	}

	cmd_buf_info->compute_pso = pipeline_info->pso;
	cmd_buf_info->graphics_pso = nullptr;
}

void RenderingDeviceDriverD3D12::command_bind_compute_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	_command_bind_uniform_set(p_cmd_buffer, p_uniform_set, p_shader, p_set_index, true);
}

void RenderingDeviceDriverD3D12::command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->Dispatch(p_x_groups, p_y_groups, p_z_groups);
}

void RenderingDeviceDriverD3D12::command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) {
	CommandBufferInfo *cmd_buf_info = (CommandBufferInfo *)p_cmd_buffer.id;
	BufferInfo *indirect_buf_info = (BufferInfo *)p_indirect_buffer.id;
	if (!barrier_capabilities.enhanced_barriers_supported) {
		_resource_transition_batch(cmd_buf_info, indirect_buf_info, 0, 1, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		_resource_transitions_flush(cmd_buf_info);
	}

	cmd_buf_info->cmd_list->ExecuteIndirect(indirect_cmd_signatures.dispatch.Get(), 1, indirect_buf_info->resource, p_offset, nullptr, 0);
}

// ----- PIPELINE -----

RDD::PipelineID RenderingDeviceDriverD3D12::compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	const ShaderInfo *shader_info_in = (const ShaderInfo *)p_shader.id;

	CD3DX12_PIPELINE_STATE_STREAM pipeline_desc = {};

	// Stages bytecodes + specialization constants.

	pipeline_desc.pRootSignature = shader_info_in->root_signature.Get();

	HashMap<ShaderStage, Vector<uint8_t>> final_stages_bytecode;
	bool ok = _shader_apply_specialization_constants(shader_info_in, p_specialization_constants, final_stages_bytecode);
	ERR_FAIL_COND_V(!ok, PipelineID());

	pipeline_desc.CS = D3D12_SHADER_BYTECODE{
		final_stages_bytecode[SHADER_STAGE_COMPUTE].ptr(),
		(SIZE_T)final_stages_bytecode[SHADER_STAGE_COMPUTE].size()
	};

	ComPtr<ID3D12Device2> device_2;
	device->QueryInterface(device_2.GetAddressOf());
	ID3D12PipelineState *pso = nullptr;
	HRESULT res = E_FAIL;
	if (device_2) {
		D3D12_PIPELINE_STATE_STREAM_DESC pssd = {};
		pssd.pPipelineStateSubobjectStream = &pipeline_desc;
		pssd.SizeInBytes = sizeof(pipeline_desc);
		res = device_2->CreatePipelineState(&pssd, IID_PPV_ARGS(&pso));
	} else {
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = pipeline_desc.ComputeDescV0();
		res = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
	}
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), PipelineID(), "Create(Compute)PipelineState failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	PipelineInfo *pipeline_info = memnew(PipelineInfo);
	pipeline_info->pso = pso;
	pipeline_info->shader_info = shader_info_in;

	return PipelineID(pipeline_info);
}

/*****************/
/**** QUERIES ****/
/*****************/

// ----- TIMESTAMP -----

RDD::QueryPoolID RenderingDeviceDriverD3D12::timestamp_query_pool_create(uint32_t p_query_count) {
	ComPtr<ID3D12QueryHeap> query_heap;
	{
		D3D12_QUERY_HEAP_DESC qh_desc = {};
		qh_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		qh_desc.Count = p_query_count;
		qh_desc.NodeMask = 0;
		HRESULT res = device->CreateQueryHeap(&qh_desc, IID_PPV_ARGS(query_heap.GetAddressOf()));
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), QueryPoolID(), "CreateQueryHeap failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	}

	ComPtr<D3D12MA::Allocation> results_buffer_allocation;
	{
		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

		CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * p_query_count);

		ComPtr<ID3D12Resource> results_buffer;
		HRESULT res = allocator->CreateResource(
				&allocation_desc,
				&resource_desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				results_buffer_allocation.GetAddressOf(),
				IID_PPV_ARGS(results_buffer.GetAddressOf()));
		ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), QueryPoolID(), "D3D12MA::CreateResource failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	}

	// Bookkeep.

	TimestampQueryPoolInfo *tqp_info = VersatileResource::allocate<TimestampQueryPoolInfo>(resources_allocator);
	tqp_info->query_heap = query_heap;
	tqp_info->query_count = p_query_count;
	tqp_info->results_buffer_allocation = results_buffer_allocation;

	return RDD::QueryPoolID(tqp_info);
}

void RenderingDeviceDriverD3D12::timestamp_query_pool_free(QueryPoolID p_pool_id) {
	TimestampQueryPoolInfo *tqp_info = (TimestampQueryPoolInfo *)p_pool_id.id;
	VersatileResource::free(resources_allocator, tqp_info);
}

void RenderingDeviceDriverD3D12::timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) {
	TimestampQueryPoolInfo *tqp_info = (TimestampQueryPoolInfo *)p_pool_id.id;

	ID3D12Resource *results_buffer = tqp_info->results_buffer_allocation->GetResource();

	void *results_buffer_data = nullptr;
	results_buffer->Map(0, &VOID_RANGE, &results_buffer_data);
	memcpy(r_results, results_buffer_data, sizeof(uint64_t) * p_query_count);
	results_buffer->Unmap(0, &VOID_RANGE);
}

uint64_t RenderingDeviceDriverD3D12::timestamp_query_result_to_time(uint64_t p_result) {
	return p_result / (double)device_limits.timestamp_frequency * 1000000000.0;
}

void RenderingDeviceDriverD3D12::command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) {
}

void RenderingDeviceDriverD3D12::command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) {
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	TimestampQueryPoolInfo *tqp_info = (TimestampQueryPoolInfo *)p_pool_id.id;
	ID3D12Resource *results_buffer = tqp_info->results_buffer_allocation->GetResource();
	cmd_buf_info->cmd_list->EndQuery(tqp_info->query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, p_index);
	cmd_buf_info->cmd_list->ResolveQueryData(tqp_info->query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, p_index, 1, results_buffer, p_index * sizeof(uint64_t));
}

void RenderingDeviceDriverD3D12::command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) {
#ifdef PIX_ENABLED
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	PIXBeginEvent(cmd_buf_info->cmd_list.Get(), p_color.to_argb32(), p_label_name);
#endif
}

void RenderingDeviceDriverD3D12::command_end_label(CommandBufferID p_cmd_buffer) {
#ifdef PIX_ENABLED
	const CommandBufferInfo *cmd_buf_info = (const CommandBufferInfo *)p_cmd_buffer.id;
	PIXEndEvent(cmd_buf_info->cmd_list.Get());
#endif
}

void RenderingDeviceDriverD3D12::command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) {
	// TODO: Implement via DRED.
}

/********************/
/**** SUBMISSION ****/
/********************/

void RenderingDeviceDriverD3D12::begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) {
	frame_idx = p_frame_index;

	frames_drawn = p_frames_drawn;
	allocator->SetCurrentFrameIndex(p_frames_drawn);

	frames[frame_idx].desc_heap_walkers.resources.rewind();
	frames[frame_idx].desc_heap_walkers.samplers.rewind();
	frames[frame_idx].desc_heap_walkers.aux.rewind();
	frames[frame_idx].desc_heap_walkers.rtv.rewind();
	frames[frame_idx].desc_heaps_exhausted_reported = {};
	frames[frame_idx].null_rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE{};
	frames[frame_idx].segment_serial = segment_serial;

	segment_begun = true;
}

void RenderingDeviceDriverD3D12::end_segment() {
	segment_serial++;
	segment_begun = false;
}

/**************/
/**** MISC ****/
/**************/

void RenderingDeviceDriverD3D12::_set_object_name(ID3D12Object *p_object, String p_object_name) {
	ERR_FAIL_NULL(p_object);
	int name_len = p_object_name.size();
	WCHAR *name_w = (WCHAR *)alloca(sizeof(WCHAR) * (name_len + 1));
	MultiByteToWideChar(CP_UTF8, 0, p_object_name.utf8().get_data(), -1, name_w, name_len);
	p_object->SetName(name_w);
}

void RenderingDeviceDriverD3D12::set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) {
	switch (p_type) {
		case OBJECT_TYPE_TEXTURE: {
			const TextureInfo *tex_info = (const TextureInfo *)p_driver_id.id;
			if (tex_info->owner_info.allocation) {
				_set_object_name(tex_info->resource, p_name);
			}
		} break;
		case OBJECT_TYPE_SAMPLER: {
		} break;
		case OBJECT_TYPE_BUFFER: {
			const BufferInfo *buf_info = (const BufferInfo *)p_driver_id.id;
			_set_object_name(buf_info->resource, p_name);
		} break;
		case OBJECT_TYPE_SHADER: {
			const ShaderInfo *shader_info_in = (const ShaderInfo *)p_driver_id.id;
			_set_object_name(shader_info_in->root_signature.Get(), p_name);
		} break;
		case OBJECT_TYPE_UNIFORM_SET: {
			const UniformSetInfo *uniform_set_info = (const UniformSetInfo *)p_driver_id.id;
			if (uniform_set_info->desc_heaps.resources.get_heap()) {
				_set_object_name(uniform_set_info->desc_heaps.resources.get_heap(), p_name + " resources heap");
			}
			if (uniform_set_info->desc_heaps.samplers.get_heap()) {
				_set_object_name(uniform_set_info->desc_heaps.samplers.get_heap(), p_name + " samplers heap");
			}
		} break;
		case OBJECT_TYPE_PIPELINE: {
			const PipelineInfo *pipeline_info = (const PipelineInfo *)p_driver_id.id;
			_set_object_name(pipeline_info->pso, p_name);
		} break;
		default: {
			DEV_ASSERT(false);
		}
	}
}

uint64_t RenderingDeviceDriverD3D12::get_resource_native_handle(DriverResource p_type, ID p_driver_id) {
	switch (p_type) {
		case DRIVER_RESOURCE_LOGICAL_DEVICE: {
			return (uint64_t)device.Get();
		}
		case DRIVER_RESOURCE_PHYSICAL_DEVICE: {
			return (uint64_t)adapter.Get();
		}
		case DRIVER_RESOURCE_TOPMOST_OBJECT: {
			return 0;
		}
		case DRIVER_RESOURCE_COMMAND_QUEUE: {
			return (uint64_t)p_driver_id.id;
		}
		case DRIVER_RESOURCE_QUEUE_FAMILY: {
			return 0;
		}
		case DRIVER_RESOURCE_TEXTURE: {
			const TextureInfo *tex_info = (const TextureInfo *)p_driver_id.id;
			return (uint64_t)tex_info->main_texture;
		} break;
		case DRIVER_RESOURCE_TEXTURE_VIEW: {
			const TextureInfo *tex_info = (const TextureInfo *)p_driver_id.id;
			return (uint64_t)tex_info->resource;
		}
		case DRIVER_RESOURCE_TEXTURE_DATA_FORMAT: {
			const TextureInfo *tex_info = (const TextureInfo *)p_driver_id.id;
			return (uint64_t)tex_info->desc.Format;
		}
		case DRIVER_RESOURCE_SAMPLER:
		case DRIVER_RESOURCE_UNIFORM_SET:
			return 0;
		case DRIVER_RESOURCE_BUFFER: {
			const TextureInfo *tex_info = (const TextureInfo *)p_driver_id.id;
			return (uint64_t)tex_info->resource;
		} break;
		case DRIVER_RESOURCE_COMPUTE_PIPELINE:
		case DRIVER_RESOURCE_RENDER_PIPELINE: {
			return p_driver_id.id;
		}
		default: {
			return 0;
		}
	}
}

uint64_t RenderingDeviceDriverD3D12::get_total_memory_used() {
	D3D12MA::TotalStatistics stats;
	allocator->CalculateStatistics(&stats);
	return stats.Total.Stats.BlockBytes;
}

uint64_t RenderingDeviceDriverD3D12::limit_get(Limit p_limit) {
	uint64_t safe_unbounded = ((uint64_t)1 << 30);
	switch (p_limit) {
		case LIMIT_MAX_BOUND_UNIFORM_SETS:
			return safe_unbounded;
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return device_limits.max_srvs_per_shader_stage;
		case LIMIT_MAX_UNIFORM_BUFFER_SIZE:
			return 65536;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_X:
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_Y:
			return 16384; // Based on max. texture size. Maybe not correct.
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X:
			return D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y:
			return D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z:
			return D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X:
			return D3D12_CS_THREAD_GROUP_MAX_X;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y:
			return D3D12_CS_THREAD_GROUP_MAX_Y;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z:
			return D3D12_CS_THREAD_GROUP_MAX_Z;
		case LIMIT_SUBGROUP_SIZE:
		// Note in min/max. Shader model 6.6 supports it (see https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_WaveSize.html),
		// but at this time I don't know the implications on the transpilation to DXIL, etc.
		case LIMIT_SUBGROUP_MIN_SIZE:
		case LIMIT_SUBGROUP_MAX_SIZE:
			return subgroup_capabilities.size;
		case LIMIT_SUBGROUP_IN_SHADERS:
			return subgroup_capabilities.supported_stages_flags_rd();
		case LIMIT_SUBGROUP_OPERATIONS:
			return subgroup_capabilities.supported_operations_flags_rd();
		case LIMIT_VRS_TEXEL_WIDTH:
		case LIMIT_VRS_TEXEL_HEIGHT:
			return vrs_capabilities.ss_image_tile_size;
		case LIMIT_VRS_MAX_FRAGMENT_WIDTH:
		case LIMIT_VRS_MAX_FRAGMENT_HEIGHT:
			return vrs_capabilities.ss_max_fragment_size;
		default: {
#ifdef DEV_ENABLED
			WARN_PRINT("Returning maximum value for unknown limit " + itos(p_limit) + ".");
#endif
			return safe_unbounded;
		}
	}
}

uint64_t RenderingDeviceDriverD3D12::api_trait_get(ApiTrait p_trait) {
	switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return barrier_capabilities.enhanced_barriers_supported;
		case API_TRAIT_SHADER_CHANGE_INVALIDATION:
			return (uint64_t)SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH;
		case API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT:
			return D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		case API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP:
			return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
		case API_TRAIT_SECONDARY_VIEWPORT_SCISSOR:
			return false;
		case API_TRAIT_CLEARS_WITH_COPY_ENGINE:
			return false;
		default:
			return RenderingDeviceDriver::api_trait_get(p_trait);
	}
}

bool RenderingDeviceDriverD3D12::has_feature(Features p_feature) {
	switch (p_feature) {
		case SUPPORTS_MULTIVIEW:
			return multiview_capabilities.is_supported && multiview_capabilities.max_view_count > 1;
		case SUPPORTS_FSR_HALF_FLOAT:
			return shader_capabilities.native_16bit_ops && storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported;
		case SUPPORTS_ATTACHMENT_VRS:
			return vrs_capabilities.ss_image_supported;
		case SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS:
			return true;
		default:
			return false;
	}
}

const RDD::MultiviewCapabilities &RenderingDeviceDriverD3D12::get_multiview_capabilities() {
	return multiview_capabilities;
}

String RenderingDeviceDriverD3D12::get_api_name() const {
	return "D3D12";
}

String RenderingDeviceDriverD3D12::get_api_version() const {
	return vformat("%d_%d", feature_level / 10, feature_level % 10);
}

String RenderingDeviceDriverD3D12::get_pipeline_cache_uuid() const {
	return pipeline_cache_id;
}

const RDD::Capabilities &RenderingDeviceDriverD3D12::get_capabilities() const {
	return device_capabilities;
}

bool RenderingDeviceDriverD3D12::is_composite_alpha_supported(CommandQueueID p_queue) const {
	if (has_comp_alpha.has((uint64_t)p_queue.id)) {
		return has_comp_alpha[(uint64_t)p_queue.id];
	}
	return false;
}

/******************/

RenderingDeviceDriverD3D12::RenderingDeviceDriverD3D12(RenderingContextDriverD3D12 *p_context_driver) {
	DEV_ASSERT(p_context_driver != nullptr);

	this->context_driver = p_context_driver;
}

RenderingDeviceDriverD3D12::~RenderingDeviceDriverD3D12() {
	glsl_type_singleton_decref();
}

bool RenderingDeviceDriverD3D12::is_in_developer_mode() {
	HKEY hkey = nullptr;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hkey);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	DWORD value = 0;
	DWORD dword_size = sizeof(DWORD);
	result = RegQueryValueExW(hkey, L"AllowDevelopmentWithoutDevLicense", nullptr, nullptr, (PBYTE)&value, &dword_size);
	RegCloseKey(hkey);

	if (result != ERROR_SUCCESS) {
		return false;
	}

	return (value != 0);
}

Error RenderingDeviceDriverD3D12::_initialize_device() {
	HRESULT res;

	if (is_in_developer_mode()) {
		typedef HRESULT(WINAPI * PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES)(_In_ UINT, _In_count_(NumFeatures) const IID *, _In_opt_count_(NumFeatures) void *, _In_opt_count_(NumFeatures) UINT *);
		PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES d3d_D3D12EnableExperimentalFeatures = (PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES)(void *)GetProcAddress(context_driver->lib_d3d12, "D3D12EnableExperimentalFeatures");
		ERR_FAIL_NULL_V(d3d_D3D12EnableExperimentalFeatures, ERR_CANT_CREATE);

		UUID experimental_features[] = { D3D12ExperimentalShaderModels };
		d3d_D3D12EnableExperimentalFeatures(1, experimental_features, nullptr, nullptr);
	}

	ID3D12DeviceFactory *device_factory = context_driver->device_factory_get();
	if (device_factory != nullptr) {
		res = device_factory->CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
	} else {
		PFN_D3D12_CREATE_DEVICE d3d_D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)(void *)GetProcAddress(context_driver->lib_d3d12, "D3D12CreateDevice");
		ERR_FAIL_NULL_V(d3d_D3D12CreateDevice, ERR_CANT_CREATE);

		res = d3d_D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
	}
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_CANT_CREATE, "D3D12CreateDevice failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	if (context_driver->use_validation_layers()) {
		ComPtr<ID3D12InfoQueue> info_queue;
		res = device.As(&info_queue);
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);

#if CUSTOM_INFO_QUEUE_ENABLED
		ComPtr<ID3D12InfoQueue1> info_queue_1;
		device.As(&info_queue_1);
		if (info_queue_1) {
			// Custom printing supported (added in Windows 10 Release Preview build 20236). Even if the callback cookie is unused, it seems the
			// argument is not optional and the function will fail if it's not specified.
			DWORD callback_cookie;
			info_queue_1->SetMuteDebugOutput(TRUE);
			res = info_queue_1->RegisterMessageCallback(&_debug_message_func, D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, nullptr, &callback_cookie);
			ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
		} else
#endif
		{
			// Rely on D3D12's own debug printing.
			if (Engine::get_singleton()->is_abort_on_gpu_errors_enabled()) {
				res = info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
				ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
				res = info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
				res = info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
			}
		}

		D3D12_MESSAGE_SEVERITY severities_to_mute[] = {
			D3D12_MESSAGE_SEVERITY_INFO,
		};

		D3D12_MESSAGE_ID messages_to_mute[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
			// These happen due to how D3D12MA manages buffers; seems benign.
			D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_HAS_NO_RESOURCE,
			D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,
			// Seemingly a false positive.
			D3D12_MESSAGE_ID_DATA_STATIC_WHILE_SET_AT_EXECUTE_DESCRIPTOR_INVALID_DATA_CHANGE,
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumSeverities = ARRAY_SIZE(severities_to_mute);
		filter.DenyList.pSeverityList = severities_to_mute;
		filter.DenyList.NumIDs = ARRAY_SIZE(messages_to_mute);
		filter.DenyList.pIDList = messages_to_mute;

		res = info_queue->PushStorageFilter(&filter);
		ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);
	}

	return OK;
}

Error RenderingDeviceDriverD3D12::_check_capabilities() {
	// Check feature levels.
	const D3D_FEATURE_LEVEL FEATURE_LEVELS[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_2,
	};

	D3D12_FEATURE_DATA_FEATURE_LEVELS feat_levels = {};
	feat_levels.NumFeatureLevels = ARRAY_SIZE(FEATURE_LEVELS);
	feat_levels.pFeatureLevelsRequested = FEATURE_LEVELS;

	HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feat_levels, sizeof(feat_levels));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_UNAVAILABLE, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	// Example: D3D_FEATURE_LEVEL_12_1 = 0xc100.
	uint32_t feat_level_major = feat_levels.MaxSupportedFeatureLevel >> 12;
	uint32_t feat_level_minor = (feat_levels.MaxSupportedFeatureLevel >> 16) & 0xff;
	feature_level = feat_level_major * 10 + feat_level_minor;

	// Fill device capabilities.
	device_capabilities.device_family = DEVICE_DIRECTX;
	device_capabilities.version_major = feature_level / 10;
	device_capabilities.version_minor = feature_level % 10;

	// Assume not supported until proven otherwise.
	vrs_capabilities.draw_call_supported = false;
	vrs_capabilities.primitive_supported = false;
	vrs_capabilities.primitive_in_multiviewport = false;
	vrs_capabilities.ss_image_supported = false;
	vrs_capabilities.ss_image_tile_size = 1;
	vrs_capabilities.additional_rates_supported = false;
	multiview_capabilities.is_supported = false;
	multiview_capabilities.geometry_shader_is_supported = false;
	multiview_capabilities.tessellation_shader_is_supported = false;
	multiview_capabilities.max_view_count = 0;
	multiview_capabilities.max_instance_count = 0;
	multiview_capabilities.is_supported = false;
	subgroup_capabilities.size = 0;
	subgroup_capabilities.wave_ops_supported = false;
	shader_capabilities.shader_model = (D3D_SHADER_MODEL)0;
	shader_capabilities.native_16bit_ops = false;
	storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported = false;
	format_capabilities.relaxed_casting_supported = false;

	{
		static const D3D_SHADER_MODEL SMS_TO_CHECK[] = {
			D3D_SHADER_MODEL_6_6,
			D3D_SHADER_MODEL_6_5,
			D3D_SHADER_MODEL_6_4,
			D3D_SHADER_MODEL_6_3,
			D3D_SHADER_MODEL_6_2,
			D3D_SHADER_MODEL_6_1,
			D3D_SHADER_MODEL_6_0, // Determined by NIR (dxil_min_shader_model).
		};

		D3D12_FEATURE_DATA_SHADER_MODEL shader_model = {};
		for (uint32_t i = 0; i < ARRAY_SIZE(SMS_TO_CHECK); i++) {
			shader_model.HighestShaderModel = SMS_TO_CHECK[i];
			res = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shader_model, sizeof(shader_model));
			if (SUCCEEDED(res)) {
				shader_capabilities.shader_model = shader_model.HighestShaderModel;
				break;
			}
			if (res == E_INVALIDARG) {
				continue; // Must assume the device doesn't know about the SM just checked.
			}
			ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_CANT_CREATE, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
		}

#define D3D_SHADER_MODEL_TO_STRING(m_sm) vformat("%d.%d", (m_sm >> 4), (m_sm & 0xf))

		ERR_FAIL_COND_V_MSG(!shader_capabilities.shader_model, ERR_UNAVAILABLE,
				vformat("No support for any of the suitable shader models (%s-%s) has been found.", D3D_SHADER_MODEL_TO_STRING(SMS_TO_CHECK[ARRAY_SIZE(SMS_TO_CHECK) - 1]), D3D_SHADER_MODEL_TO_STRING(SMS_TO_CHECK[0])));

		print_verbose("- Shader:");
		print_verbose("  model: " + D3D_SHADER_MODEL_TO_STRING(shader_capabilities.shader_model));
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
	if (SUCCEEDED(res)) {
		storage_buffer_capabilities.storage_buffer_16_bit_access_is_supported = options.TypedUAVLoadAdditionalFormats;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1));
	if (SUCCEEDED(res)) {
		subgroup_capabilities.size = options1.WaveLaneCountMin;
		subgroup_capabilities.wave_ops_supported = options1.WaveOps;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2));
	if (SUCCEEDED(res)) {
		misc_features_support.depth_bounds_supported = options2.DepthBoundsTestSupported;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &options3, sizeof(options3));
	if (SUCCEEDED(res)) {
		// https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_view_instancing_tier
		// https://microsoft.github.io/DirectX-Specs/d3d/ViewInstancing.html#sv_viewid
		if (options3.ViewInstancingTier >= D3D12_VIEW_INSTANCING_TIER_1) {
			multiview_capabilities.is_supported = true;
			multiview_capabilities.geometry_shader_is_supported = options3.ViewInstancingTier >= D3D12_VIEW_INSTANCING_TIER_3;
			multiview_capabilities.tessellation_shader_is_supported = options3.ViewInstancingTier >= D3D12_VIEW_INSTANCING_TIER_3;
			multiview_capabilities.max_view_count = D3D12_MAX_VIEW_INSTANCE_COUNT;
			multiview_capabilities.max_instance_count = UINT32_MAX;
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4));
	if (SUCCEEDED(res)) {
		shader_capabilities.native_16bit_ops = options4.Native16BitShaderOpsSupported;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6));
	if (SUCCEEDED(res)) {
		if (options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1) {
			vrs_capabilities.draw_call_supported = true;
			if (options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2) {
				vrs_capabilities.primitive_supported = true;
				vrs_capabilities.primitive_in_multiviewport = options6.PerPrimitiveShadingRateSupportedWithViewportIndexing;
				vrs_capabilities.ss_image_supported = true;
				vrs_capabilities.ss_image_tile_size = options6.ShadingRateImageTileSize;
				vrs_capabilities.ss_max_fragment_size = 8; // TODO figure out if this is supplied and/or needed
				vrs_capabilities.additional_rates_supported = options6.AdditionalShadingRatesSupported;
			}
		}
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
	res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12));
	if (SUCCEEDED(res)) {
		format_capabilities.relaxed_casting_supported = options12.RelaxedFormatCastingSupported;
		barrier_capabilities.enhanced_barriers_supported = options12.EnhancedBarriersSupported;
	}

	if (vrs_capabilities.draw_call_supported || vrs_capabilities.primitive_supported || vrs_capabilities.ss_image_supported) {
		print_verbose("- D3D12 Variable Rate Shading supported:");
		if (vrs_capabilities.draw_call_supported) {
			print_verbose("  Draw call");
		}
		if (vrs_capabilities.primitive_supported) {
			print_verbose(String("  Per-primitive (multi-viewport: ") + (vrs_capabilities.primitive_in_multiviewport ? "yes" : "no") + ")");
		}
		if (vrs_capabilities.ss_image_supported) {
			print_verbose(String("  Screen-space image (tile size: ") + itos(vrs_capabilities.ss_image_tile_size) + ")");
		}
		if (vrs_capabilities.additional_rates_supported) {
			print_verbose(String("  Additional rates: ") + (vrs_capabilities.additional_rates_supported ? "yes" : "no"));
		}
	} else {
		print_verbose("- D3D12 Variable Rate Shading not supported");
	}

	if (multiview_capabilities.is_supported) {
		print_verbose("- D3D12 multiview supported:");
		print_verbose("  max view count: " + itos(multiview_capabilities.max_view_count));
		//print_verbose("  max instances: " + itos(multiview_capabilities.max_instance_count)); // Hardcoded; not very useful at the moment.
	} else {
		print_verbose("- D3D12 multiview not supported");
	}

	if (format_capabilities.relaxed_casting_supported) {
#if 0
		print_verbose("- Relaxed casting supported");
#else
		// Certain configurations (Windows 11 with an updated NVIDIA driver) crash when using relaxed casting.
		// Therefore, we disable it temporarily until we can assure that it's reliable.
		// There are fallbacks in place that work in every case, if less efficient.
		format_capabilities.relaxed_casting_supported = false;
		print_verbose("- Relaxed casting supported (but disabled for now)");
#endif
	} else {
		print_verbose("- Relaxed casting not supported");
	}

	print_verbose(String("- D3D12 16-bit ops supported: ") + (shader_capabilities.native_16bit_ops ? "yes" : "no"));

	if (misc_features_support.depth_bounds_supported) {
		print_verbose("- Depth bounds test supported");
	} else {
		print_verbose("- Depth bounds test not supported");
	}

	return OK;
}

Error RenderingDeviceDriverD3D12::_get_device_limits() {
	D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
	HRESULT res = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_UNAVAILABLE, "CheckFeatureSupport failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	// https://docs.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
	device_limits.max_srvs_per_shader_stage = options.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1 ? 128 : UINT64_MAX;
	device_limits.max_cbvs_per_shader_stage = options.ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2 ? 14 : UINT64_MAX;
	device_limits.max_samplers_across_all_stages = options.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1 ? 16 : 2048;
	if (options.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1) {
		device_limits.max_uavs_across_all_stages = feature_level <= 110 ? 8 : 64;
	} else if (options.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_2) {
		device_limits.max_uavs_across_all_stages = 64;
	} else {
		device_limits.max_uavs_across_all_stages = UINT64_MAX;
	}

	// Retrieving the timestamp frequency requires creating a command queue that will be discarded immediately.
	ComPtr<ID3D12CommandQueue> unused_command_queue;
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	res = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(unused_command_queue.GetAddressOf()));
	ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);

	res = unused_command_queue->GetTimestampFrequency(&device_limits.timestamp_frequency);
	if (!SUCCEEDED(res)) {
		print_verbose("D3D12: GetTimestampFrequency failed with error " + vformat("0x%08ux", (uint64_t)res) + ". Timestamps will be inaccurate.");
	}

	return OK;
}

Error RenderingDeviceDriverD3D12::_initialize_allocator() {
	D3D12MA::ALLOCATOR_DESC allocator_desc = {};
	allocator_desc.pDevice = device.Get();
	allocator_desc.pAdapter = adapter.Get();
	allocator_desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;

	HRESULT res = D3D12MA::CreateAllocator(&allocator_desc, &allocator);
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_CANT_CREATE, "D3D12MA::CreateAllocator failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");

	return OK;
}

static Error create_command_signature(ID3D12Device *device, D3D12_INDIRECT_ARGUMENT_TYPE p_type, uint32_t p_stride, ComPtr<ID3D12CommandSignature> *r_cmd_sig) {
	D3D12_INDIRECT_ARGUMENT_DESC iarg_desc = {};
	iarg_desc.Type = p_type;
	D3D12_COMMAND_SIGNATURE_DESC cs_desc = {};
	cs_desc.ByteStride = p_stride;
	cs_desc.NumArgumentDescs = 1;
	cs_desc.pArgumentDescs = &iarg_desc;
	cs_desc.NodeMask = 0;
	HRESULT res = device->CreateCommandSignature(&cs_desc, nullptr, IID_PPV_ARGS(r_cmd_sig->GetAddressOf()));
	ERR_FAIL_COND_V_MSG(!SUCCEEDED(res), ERR_CANT_CREATE, "CreateCommandSignature failed with error " + vformat("0x%08ux", (uint64_t)res) + ".");
	return OK;
};

Error RenderingDeviceDriverD3D12::_initialize_frames(uint32_t p_frame_count) {
	Error err;
	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	//CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	uint32_t resource_descriptors_per_frame = GLOBAL_GET("rendering/rendering_device/d3d12/max_resource_descriptors_per_frame");
	uint32_t sampler_descriptors_per_frame = GLOBAL_GET("rendering/rendering_device/d3d12/max_sampler_descriptors_per_frame");
	uint32_t misc_descriptors_per_frame = GLOBAL_GET("rendering/rendering_device/d3d12/max_misc_descriptors_per_frame");

	frames.resize(p_frame_count);
	for (uint32_t i = 0; i < frames.size(); i++) {
		err = frames[i].desc_heaps.resources.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, resource_descriptors_per_frame, true);
		ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Creating the frame's RESOURCE descriptors heap failed.");

		err = frames[i].desc_heaps.samplers.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sampler_descriptors_per_frame, true);
		ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Creating the frame's SAMPLER descriptors heap failed.");

		err = frames[i].desc_heaps.aux.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, misc_descriptors_per_frame, false);
		ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Creating the frame's AUX descriptors heap failed.");

		err = frames[i].desc_heaps.rtv.allocate(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, misc_descriptors_per_frame, false);
		ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Creating the frame's RENDER TARGET descriptors heap failed.");

		frames[i].desc_heap_walkers.resources = frames[i].desc_heaps.resources.make_walker();
		frames[i].desc_heap_walkers.samplers = frames[i].desc_heaps.samplers.make_walker();
		frames[i].desc_heap_walkers.aux = frames[i].desc_heaps.aux.make_walker();
		frames[i].desc_heap_walkers.rtv = frames[i].desc_heaps.rtv.make_walker();
	}

	return OK;
}

Error RenderingDeviceDriverD3D12::_initialize_command_signatures() {
	Error err = create_command_signature(device.Get(), D3D12_INDIRECT_ARGUMENT_TYPE_DRAW, sizeof(D3D12_DRAW_ARGUMENTS), &indirect_cmd_signatures.draw);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = create_command_signature(device.Get(), D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), &indirect_cmd_signatures.draw_indexed);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = create_command_signature(device.Get(), D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH, sizeof(D3D12_DISPATCH_ARGUMENTS), &indirect_cmd_signatures.dispatch);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	return OK;
}

Error RenderingDeviceDriverD3D12::initialize(uint32_t p_device_index, uint32_t p_frame_count) {
	context_device = context_driver->device_get(p_device_index);
	adapter = context_driver->create_adapter(p_device_index);
	ERR_FAIL_NULL_V(adapter, ERR_CANT_CREATE);

	HRESULT res = adapter->GetDesc(&adapter_desc);
	ERR_FAIL_COND_V(!SUCCEEDED(res), ERR_CANT_CREATE);

	// Set the pipeline cache ID based on the adapter information.
	pipeline_cache_id = String::hex_encode_buffer((uint8_t *)&adapter_desc.AdapterLuid, sizeof(LUID));
	pipeline_cache_id += "-driver-" + itos(adapter_desc.Revision);

	Error err = _initialize_device();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = _check_capabilities();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = _get_device_limits();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = _initialize_allocator();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = _initialize_frames(p_frame_count);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	err = _initialize_command_signatures();
	ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

	glsl_type_singleton_init_or_ref();

	return OK;
}
