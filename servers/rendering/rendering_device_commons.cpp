/**************************************************************************/
/*  rendering_device_commons.cpp                                          */
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

#include "rendering_device_commons.h"

/*****************/
/**** GENERIC ****/
/*****************/

const char *const RenderingDeviceCommons::FORMAT_NAMES[DATA_FORMAT_MAX] = {
	"R4G4_Unorm_Pack8",
	"R4G4B4A4_Unorm_Pack16",
	"B4G4R4A4_Unorm_Pack16",
	"R5G6B5_Unorm_Pack16",
	"B5G6R5_Unorm_Pack16",
	"R5G5B5A1_Unorm_Pack16",
	"B5G5R5A1_Unorm_Pack16",
	"A1R5G5B5_Unorm_Pack16",
	"R8_Unorm",
	"R8_Snorm",
	"R8_Uscaled",
	"R8_Sscaled",
	"R8_Uint",
	"R8_Sint",
	"R8_Srgb",
	"R8G8_Unorm",
	"R8G8_Snorm",
	"R8G8_Uscaled",
	"R8G8_Sscaled",
	"R8G8_Uint",
	"R8G8_Sint",
	"R8G8_Srgb",
	"R8G8B8_Unorm",
	"R8G8B8_Snorm",
	"R8G8B8_Uscaled",
	"R8G8B8_Sscaled",
	"R8G8B8_Uint",
	"R8G8B8_Sint",
	"R8G8B8_Srgb",
	"B8G8R8_Unorm",
	"B8G8R8_Snorm",
	"B8G8R8_Uscaled",
	"B8G8R8_Sscaled",
	"B8G8R8_Uint",
	"B8G8R8_Sint",
	"B8G8R8_Srgb",
	"R8G8B8A8_Unorm",
	"R8G8B8A8_Snorm",
	"R8G8B8A8_Uscaled",
	"R8G8B8A8_Sscaled",
	"R8G8B8A8_Uint",
	"R8G8B8A8_Sint",
	"R8G8B8A8_Srgb",
	"B8G8R8A8_Unorm",
	"B8G8R8A8_Snorm",
	"B8G8R8A8_Uscaled",
	"B8G8R8A8_Sscaled",
	"B8G8R8A8_Uint",
	"B8G8R8A8_Sint",
	"B8G8R8A8_Srgb",
	"A8B8G8R8_Unorm_Pack32",
	"A8B8G8R8_Snorm_Pack32",
	"A8B8G8R8_Uscaled_Pack32",
	"A8B8G8R8_Sscaled_Pack32",
	"A8B8G8R8_Uint_Pack32",
	"A8B8G8R8_Sint_Pack32",
	"A8B8G8R8_Srgb_Pack32",
	"A2R10G10B10_Unorm_Pack32",
	"A2R10G10B10_Snorm_Pack32",
	"A2R10G10B10_Uscaled_Pack32",
	"A2R10G10B10_Sscaled_Pack32",
	"A2R10G10B10_Uint_Pack32",
	"A2R10G10B10_Sint_Pack32",
	"A2B10G10R10_Unorm_Pack32",
	"A2B10G10R10_Snorm_Pack32",
	"A2B10G10R10_Uscaled_Pack32",
	"A2B10G10R10_Sscaled_Pack32",
	"A2B10G10R10_Uint_Pack32",
	"A2B10G10R10_Sint_Pack32",
	"R16_Unorm",
	"R16_Snorm",
	"R16_Uscaled",
	"R16_Sscaled",
	"R16_Uint",
	"R16_Sint",
	"R16_Sfloat",
	"R16G16_Unorm",
	"R16G16_Snorm",
	"R16G16_Uscaled",
	"R16G16_Sscaled",
	"R16G16_Uint",
	"R16G16_Sint",
	"R16G16_Sfloat",
	"R16G16B16_Unorm",
	"R16G16B16_Snorm",
	"R16G16B16_Uscaled",
	"R16G16B16_Sscaled",
	"R16G16B16_Uint",
	"R16G16B16_Sint",
	"R16G16B16_Sfloat",
	"R16G16B16A16_Unorm",
	"R16G16B16A16_Snorm",
	"R16G16B16A16_Uscaled",
	"R16G16B16A16_Sscaled",
	"R16G16B16A16_Uint",
	"R16G16B16A16_Sint",
	"R16G16B16A16_Sfloat",
	"R32_Uint",
	"R32_Sint",
	"R32_Sfloat",
	"R32G32_Uint",
	"R32G32_Sint",
	"R32G32_Sfloat",
	"R32G32B32_Uint",
	"R32G32B32_Sint",
	"R32G32B32_Sfloat",
	"R32G32B32A32_Uint",
	"R32G32B32A32_Sint",
	"R32G32B32A32_Sfloat",
	"R64_Uint",
	"R64_Sint",
	"R64_Sfloat",
	"R64G64_Uint",
	"R64G64_Sint",
	"R64G64_Sfloat",
	"R64G64B64_Uint",
	"R64G64B64_Sint",
	"R64G64B64_Sfloat",
	"R64G64B64A64_Uint",
	"R64G64B64A64_Sint",
	"R64G64B64A64_Sfloat",
	"B10G11R11_Ufloat_Pack32",
	"E5B9G9R9_Ufloat_Pack32",
	"D16_Unorm",
	"X8_D24_Unorm_Pack32",
	"D32_Sfloat",
	"S8_Uint",
	"D16_Unorm_S8_Uint",
	"D24_Unorm_S8_Uint",
	"D32_Sfloat_S8_Uint",
	"Bc1_Rgb_Unorm_Block",
	"Bc1_Rgb_Srgb_Block",
	"Bc1_Rgba_Unorm_Block",
	"Bc1_Rgba_Srgb_Block",
	"Bc2_Unorm_Block",
	"Bc2_Srgb_Block",
	"Bc3_Unorm_Block",
	"Bc3_Srgb_Block",
	"Bc4_Unorm_Block",
	"Bc4_Snorm_Block",
	"Bc5_Unorm_Block",
	"Bc5_Snorm_Block",
	"Bc6H_Ufloat_Block",
	"Bc6H_Sfloat_Block",
	"Bc7_Unorm_Block",
	"Bc7_Srgb_Block",
	"Etc2_R8G8B8_Unorm_Block",
	"Etc2_R8G8B8_Srgb_Block",
	"Etc2_R8G8B8A1_Unorm_Block",
	"Etc2_R8G8B8A1_Srgb_Block",
	"Etc2_R8G8B8A8_Unorm_Block",
	"Etc2_R8G8B8A8_Srgb_Block",
	"Eac_R11_Unorm_Block",
	"Eac_R11_Snorm_Block",
	"Eac_R11G11_Unorm_Block",
	"Eac_R11G11_Snorm_Block",
	"Astc_4X4_Unorm_Block",
	"Astc_4X4_Srgb_Block",
	"Astc_5X4_Unorm_Block",
	"Astc_5X4_Srgb_Block",
	"Astc_5X5_Unorm_Block",
	"Astc_5X5_Srgb_Block",
	"Astc_6X5_Unorm_Block",
	"Astc_6X5_Srgb_Block",
	"Astc_6X6_Unorm_Block",
	"Astc_6X6_Srgb_Block",
	"Astc_8X5_Unorm_Block",
	"Astc_8X5_Srgb_Block",
	"Astc_8X6_Unorm_Block",
	"Astc_8X6_Srgb_Block",
	"Astc_8X8_Unorm_Block",
	"Astc_8X8_Srgb_Block",
	"Astc_10X5_Unorm_Block",
	"Astc_10X5_Srgb_Block",
	"Astc_10X6_Unorm_Block",
	"Astc_10X6_Srgb_Block",
	"Astc_10X8_Unorm_Block",
	"Astc_10X8_Srgb_Block",
	"Astc_10X10_Unorm_Block",
	"Astc_10X10_Srgb_Block",
	"Astc_12X10_Unorm_Block",
	"Astc_12X10_Srgb_Block",
	"Astc_12X12_Unorm_Block",
	"Astc_12X12_Srgb_Block",
	"G8B8G8R8_422_Unorm",
	"B8G8R8G8_422_Unorm",
	"G8_B8_R8_3Plane_420_Unorm",
	"G8_B8R8_2Plane_420_Unorm",
	"G8_B8_R8_3Plane_422_Unorm",
	"G8_B8R8_2Plane_422_Unorm",
	"G8_B8_R8_3Plane_444_Unorm",
	"R10X6_Unorm_Pack16",
	"R10X6G10X6_Unorm_2Pack16",
	"R10X6G10X6B10X6A10X6_Unorm_4Pack16",
	"G10X6B10X6G10X6R10X6_422_Unorm_4Pack16",
	"B10X6G10X6R10X6G10X6_422_Unorm_4Pack16",
	"G10X6_B10X6_R10X6_3Plane_420_Unorm_3Pack16",
	"G10X6_B10X6R10X6_2Plane_420_Unorm_3Pack16",
	"G10X6_B10X6_R10X6_3Plane_422_Unorm_3Pack16",
	"G10X6_B10X6R10X6_2Plane_422_Unorm_3Pack16",
	"G10X6_B10X6_R10X6_3Plane_444_Unorm_3Pack16",
	"R12X4_Unorm_Pack16",
	"R12X4G12X4_Unorm_2Pack16",
	"R12X4G12X4B12X4A12X4_Unorm_4Pack16",
	"G12X4B12X4G12X4R12X4_422_Unorm_4Pack16",
	"B12X4G12X4R12X4G12X4_422_Unorm_4Pack16",
	"G12X4_B12X4_R12X4_3Plane_420_Unorm_3Pack16",
	"G12X4_B12X4R12X4_2Plane_420_Unorm_3Pack16",
	"G12X4_B12X4_R12X4_3Plane_422_Unorm_3Pack16",
	"G12X4_B12X4R12X4_2Plane_422_Unorm_3Pack16",
	"G12X4_B12X4_R12X4_3Plane_444_Unorm_3Pack16",
	"G16B16G16R16_422_Unorm",
	"B16G16R16G16_422_Unorm",
	"G16_B16_R16_3Plane_420_Unorm",
	"G16_B16R16_2Plane_420_Unorm",
	"G16_B16_R16_3Plane_422_Unorm",
	"G16_B16R16_2Plane_422_Unorm",
	"G16_B16_R16_3Plane_444_Unorm",
};

/*****************/
/**** TEXTURE ****/
/*****************/

const uint32_t RenderingDeviceCommons::TEXTURE_SAMPLES_COUNT[TEXTURE_SAMPLES_MAX] = { 1, 2, 4, 8, 16, 32, 64 };

uint32_t RenderingDeviceCommons::get_image_format_pixel_size(DataFormat p_format) {
	switch (p_format) {
		case DATA_FORMAT_R4G4_UNORM_PACK8:
			return 1;
		case DATA_FORMAT_R4G4B4A4_UNORM_PACK16:
		case DATA_FORMAT_B4G4R4A4_UNORM_PACK16:
		case DATA_FORMAT_R5G6B5_UNORM_PACK16:
		case DATA_FORMAT_B5G6R5_UNORM_PACK16:
		case DATA_FORMAT_R5G5B5A1_UNORM_PACK16:
		case DATA_FORMAT_B5G5R5A1_UNORM_PACK16:
		case DATA_FORMAT_A1R5G5B5_UNORM_PACK16:
			return 2;
		case DATA_FORMAT_R8_UNORM:
		case DATA_FORMAT_R8_SNORM:
		case DATA_FORMAT_R8_USCALED:
		case DATA_FORMAT_R8_SSCALED:
		case DATA_FORMAT_R8_UINT:
		case DATA_FORMAT_R8_SINT:
		case DATA_FORMAT_R8_SRGB:
			return 1;
		case DATA_FORMAT_R8G8_UNORM:
		case DATA_FORMAT_R8G8_SNORM:
		case DATA_FORMAT_R8G8_USCALED:
		case DATA_FORMAT_R8G8_SSCALED:
		case DATA_FORMAT_R8G8_UINT:
		case DATA_FORMAT_R8G8_SINT:
		case DATA_FORMAT_R8G8_SRGB:
			return 2;
		case DATA_FORMAT_R8G8B8_UNORM:
		case DATA_FORMAT_R8G8B8_SNORM:
		case DATA_FORMAT_R8G8B8_USCALED:
		case DATA_FORMAT_R8G8B8_SSCALED:
		case DATA_FORMAT_R8G8B8_UINT:
		case DATA_FORMAT_R8G8B8_SINT:
		case DATA_FORMAT_R8G8B8_SRGB:
		case DATA_FORMAT_B8G8R8_UNORM:
		case DATA_FORMAT_B8G8R8_SNORM:
		case DATA_FORMAT_B8G8R8_USCALED:
		case DATA_FORMAT_B8G8R8_SSCALED:
		case DATA_FORMAT_B8G8R8_UINT:
		case DATA_FORMAT_B8G8R8_SINT:
		case DATA_FORMAT_B8G8R8_SRGB:
			return 3;
		case DATA_FORMAT_R8G8B8A8_UNORM:
		case DATA_FORMAT_R8G8B8A8_SNORM:
		case DATA_FORMAT_R8G8B8A8_USCALED:
		case DATA_FORMAT_R8G8B8A8_SSCALED:
		case DATA_FORMAT_R8G8B8A8_UINT:
		case DATA_FORMAT_R8G8B8A8_SINT:
		case DATA_FORMAT_R8G8B8A8_SRGB:
		case DATA_FORMAT_B8G8R8A8_UNORM:
		case DATA_FORMAT_B8G8R8A8_SNORM:
		case DATA_FORMAT_B8G8R8A8_USCALED:
		case DATA_FORMAT_B8G8R8A8_SSCALED:
		case DATA_FORMAT_B8G8R8A8_UINT:
		case DATA_FORMAT_B8G8R8A8_SINT:
		case DATA_FORMAT_B8G8R8A8_SRGB:
			return 4;
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
			return 4;
		case DATA_FORMAT_R16_UNORM:
		case DATA_FORMAT_R16_SNORM:
		case DATA_FORMAT_R16_USCALED:
		case DATA_FORMAT_R16_SSCALED:
		case DATA_FORMAT_R16_UINT:
		case DATA_FORMAT_R16_SINT:
		case DATA_FORMAT_R16_SFLOAT:
			return 2;
		case DATA_FORMAT_R16G16_UNORM:
		case DATA_FORMAT_R16G16_SNORM:
		case DATA_FORMAT_R16G16_USCALED:
		case DATA_FORMAT_R16G16_SSCALED:
		case DATA_FORMAT_R16G16_UINT:
		case DATA_FORMAT_R16G16_SINT:
		case DATA_FORMAT_R16G16_SFLOAT:
			return 4;
		case DATA_FORMAT_R16G16B16_UNORM:
		case DATA_FORMAT_R16G16B16_SNORM:
		case DATA_FORMAT_R16G16B16_USCALED:
		case DATA_FORMAT_R16G16B16_SSCALED:
		case DATA_FORMAT_R16G16B16_UINT:
		case DATA_FORMAT_R16G16B16_SINT:
		case DATA_FORMAT_R16G16B16_SFLOAT:
			return 6;
		case DATA_FORMAT_R16G16B16A16_UNORM:
		case DATA_FORMAT_R16G16B16A16_SNORM:
		case DATA_FORMAT_R16G16B16A16_USCALED:
		case DATA_FORMAT_R16G16B16A16_SSCALED:
		case DATA_FORMAT_R16G16B16A16_UINT:
		case DATA_FORMAT_R16G16B16A16_SINT:
		case DATA_FORMAT_R16G16B16A16_SFLOAT:
			return 8;
		case DATA_FORMAT_R32_UINT:
		case DATA_FORMAT_R32_SINT:
		case DATA_FORMAT_R32_SFLOAT:
			return 4;
		case DATA_FORMAT_R32G32_UINT:
		case DATA_FORMAT_R32G32_SINT:
		case DATA_FORMAT_R32G32_SFLOAT:
			return 8;
		case DATA_FORMAT_R32G32B32_UINT:
		case DATA_FORMAT_R32G32B32_SINT:
		case DATA_FORMAT_R32G32B32_SFLOAT:
			return 12;
		case DATA_FORMAT_R32G32B32A32_UINT:
		case DATA_FORMAT_R32G32B32A32_SINT:
		case DATA_FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case DATA_FORMAT_R64_UINT:
		case DATA_FORMAT_R64_SINT:
		case DATA_FORMAT_R64_SFLOAT:
			return 8;
		case DATA_FORMAT_R64G64_UINT:
		case DATA_FORMAT_R64G64_SINT:
		case DATA_FORMAT_R64G64_SFLOAT:
			return 16;
		case DATA_FORMAT_R64G64B64_UINT:
		case DATA_FORMAT_R64G64B64_SINT:
		case DATA_FORMAT_R64G64B64_SFLOAT:
			return 24;
		case DATA_FORMAT_R64G64B64A64_UINT:
		case DATA_FORMAT_R64G64B64A64_SINT:
		case DATA_FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		case DATA_FORMAT_B10G11R11_UFLOAT_PACK32:
		case DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32:
			return 4;
		case DATA_FORMAT_D16_UNORM:
			return 2;
		case DATA_FORMAT_X8_D24_UNORM_PACK32:
			return 4;
		case DATA_FORMAT_D32_SFLOAT:
			return 4;
		case DATA_FORMAT_S8_UINT:
			return 1;
		case DATA_FORMAT_D16_UNORM_S8_UINT:
			return 4;
		case DATA_FORMAT_D24_UNORM_S8_UINT:
			return 4;
		case DATA_FORMAT_D32_SFLOAT_S8_UINT:
			return 5; // ?
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
			return 1;
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
			return 1;
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
			return 1;
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
			return 1;
		case DATA_FORMAT_G8B8G8R8_422_UNORM:
		case DATA_FORMAT_B8G8R8G8_422_UNORM:
			return 4;
		case DATA_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case DATA_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case DATA_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case DATA_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case DATA_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
			return 4;
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
			return 2;
		case DATA_FORMAT_G16B16G16R16_422_UNORM:
		case DATA_FORMAT_B16G16R16G16_422_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case DATA_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case DATA_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case DATA_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
			return 8;
		default: {
			ERR_PRINT("Format not handled, bug");
		}
	}

	return 1;
}

// https://www.khronos.org/registry/DataFormat/specs/1.1/dataformat.1.1.pdf
void RenderingDeviceCommons::get_compressed_image_format_block_dimensions(DataFormat p_format, uint32_t &r_w, uint32_t &r_h) {
	switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: // Again, not sure about astc.
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK: {
			r_w = 4;
			r_h = 4;
		} break;
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK: // Unsupported
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK: {
			r_w = 4;
			r_h = 4;
		} break;
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK: {
			r_w = 8;
			r_h = 8;
		} break;
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK: // Unsupported
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
			r_w = 4;
			r_h = 4;
			return;
		default: {
			r_w = 1;
			r_h = 1;
		}
	}
}

uint32_t RenderingDeviceCommons::get_compressed_image_format_block_byte_size(DataFormat p_format) const {
	switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_BC2_UNORM_BLOCK:
		case DATA_FORMAT_BC2_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_BC3_UNORM_BLOCK:
		case DATA_FORMAT_BC3_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
			return 8;
		case DATA_FORMAT_BC5_UNORM_BLOCK:
		case DATA_FORMAT_BC5_SNORM_BLOCK:
			return 16;
		case DATA_FORMAT_BC6H_UFLOAT_BLOCK:
		case DATA_FORMAT_BC6H_SFLOAT_BLOCK:
			return 16;
		case DATA_FORMAT_BC7_UNORM_BLOCK:
		case DATA_FORMAT_BC7_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
			return 8;
		case DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
			return 16;
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
			return 8;
		case DATA_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11G11_SNORM_BLOCK:
			return 16;
		case DATA_FORMAT_ASTC_4x4_UNORM_BLOCK: // Again, not sure about astc.
		case DATA_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case DATA_FORMAT_ASTC_12x12_SRGB_BLOCK:
			return 16;
		default: {
		}
	}
	return 1;
}

uint32_t RenderingDeviceCommons::get_compressed_image_format_pixel_rshift(DataFormat p_format) {
	switch (p_format) {
		case DATA_FORMAT_BC1_RGB_UNORM_BLOCK: // These formats are half byte size, so rshift is 1.
		case DATA_FORMAT_BC1_RGB_SRGB_BLOCK:
		case DATA_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case DATA_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case DATA_FORMAT_BC4_UNORM_BLOCK:
		case DATA_FORMAT_BC4_SNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case DATA_FORMAT_EAC_R11_UNORM_BLOCK:
		case DATA_FORMAT_EAC_R11_SNORM_BLOCK:
			return 1;
		case DATA_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case DATA_FORMAT_ASTC_8x8_UNORM_BLOCK: {
			return 2;
		}
		default: {
		}
	}

	return 0;
}

uint32_t RenderingDeviceCommons::get_image_format_required_size(DataFormat p_format, uint32_t p_width, uint32_t p_height, uint32_t p_depth, uint32_t p_mipmaps, uint32_t *r_blockw, uint32_t *r_blockh, uint32_t *r_depth) {
	ERR_FAIL_COND_V(p_mipmaps == 0, 0);
	uint32_t w = p_width;
	uint32_t h = p_height;
	uint32_t d = p_depth;

	uint32_t size = 0;

	uint32_t pixel_size = get_image_format_pixel_size(p_format);
	uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(p_format);
	uint32_t blockw = 0;
	uint32_t blockh = 0;
	get_compressed_image_format_block_dimensions(p_format, blockw, blockh);

	for (uint32_t i = 0; i < p_mipmaps; i++) {
		uint32_t bw = STEPIFY(w, blockw);
		uint32_t bh = STEPIFY(h, blockh);

		uint32_t s = bw * bh;

		s *= pixel_size;
		s >>= pixel_rshift;
		size += s * d;
		if (r_blockw) {
			*r_blockw = bw;
		}
		if (r_blockh) {
			*r_blockh = bh;
		}
		if (r_depth) {
			*r_depth = d;
		}
		w = MAX(blockw, w >> 1);
		h = MAX(blockh, h >> 1);
		d = MAX(1u, d >> 1);
	}

	return size;
}

uint32_t RenderingDeviceCommons::get_image_required_mipmaps(uint32_t p_width, uint32_t p_height, uint32_t p_depth) {
	// Formats and block size don't really matter here since they can all go down to 1px (even if block is larger).
	uint32_t w = p_width;
	uint32_t h = p_height;
	uint32_t d = p_depth;

	uint32_t mipmaps = 1;

	while (true) {
		if (w == 1 && h == 1 && d == 1) {
			break;
		}

		w = MAX(1u, w >> 1);
		h = MAX(1u, h >> 1);
		d = MAX(1u, d >> 1);

		mipmaps++;
	}

	return mipmaps;
}

bool RenderingDeviceCommons::format_has_stencil(DataFormat p_format) {
	switch (p_format) {
		case DATA_FORMAT_S8_UINT:
		case DATA_FORMAT_D16_UNORM_S8_UINT:
		case DATA_FORMAT_D24_UNORM_S8_UINT:
		case DATA_FORMAT_D32_SFLOAT_S8_UINT: {
			return true;
		}
		default: {
		}
	}
	return false;
}

uint32_t RenderingDeviceCommons::format_get_plane_count(DataFormat p_format) {
	uint32_t planes = 1;
	switch (p_format) {
		case DATA_FORMAT_D16_UNORM_S8_UINT:
		case DATA_FORMAT_D24_UNORM_S8_UINT:
		case DATA_FORMAT_D32_SFLOAT_S8_UINT: {
			planes = 2;
			break;
		}
		default: {
		}
	}
	DEV_ASSERT(planes <= MAX_IMAGE_FORMAT_PLANES);
	return planes;
}

/*****************/
/**** SAMPLER ****/
/*****************/

const Color RenderingDeviceCommons::SAMPLER_BORDER_COLOR_VALUE[SAMPLER_BORDER_COLOR_MAX] = {
	Color(0, 0, 0, 0),
	Color(0, 0, 0, 0),
	Color(0, 0, 0, 1),
	Color(0, 0, 0, 1),
	Color(1, 1, 1, 1),
	Color(1, 1, 1, 1),
};

/**********************/
/**** VERTEX ARRAY ****/
/**********************/

uint32_t RenderingDeviceCommons::get_format_vertex_size(DataFormat p_format) {
	switch (p_format) {
		case DATA_FORMAT_R8_UNORM:
		case DATA_FORMAT_R8_SNORM:
		case DATA_FORMAT_R8_UINT:
		case DATA_FORMAT_R8_SINT:
		case DATA_FORMAT_R8G8_UNORM:
		case DATA_FORMAT_R8G8_SNORM:
		case DATA_FORMAT_R8G8_UINT:
		case DATA_FORMAT_R8G8_SINT:
		case DATA_FORMAT_R8G8B8_UNORM:
		case DATA_FORMAT_R8G8B8_SNORM:
		case DATA_FORMAT_R8G8B8_UINT:
		case DATA_FORMAT_R8G8B8_SINT:
		case DATA_FORMAT_B8G8R8_UNORM:
		case DATA_FORMAT_B8G8R8_SNORM:
		case DATA_FORMAT_B8G8R8_UINT:
		case DATA_FORMAT_B8G8R8_SINT:
		case DATA_FORMAT_R8G8B8A8_UNORM:
		case DATA_FORMAT_R8G8B8A8_SNORM:
		case DATA_FORMAT_R8G8B8A8_UINT:
		case DATA_FORMAT_R8G8B8A8_SINT:
		case DATA_FORMAT_B8G8R8A8_UNORM:
		case DATA_FORMAT_B8G8R8A8_SNORM:
		case DATA_FORMAT_B8G8R8A8_UINT:
		case DATA_FORMAT_B8G8R8A8_SINT:
		case DATA_FORMAT_A2B10G10R10_UNORM_PACK32:
			return 4;
		case DATA_FORMAT_R16_UNORM:
		case DATA_FORMAT_R16_SNORM:
		case DATA_FORMAT_R16_UINT:
		case DATA_FORMAT_R16_SINT:
		case DATA_FORMAT_R16_SFLOAT:
			return 4;
		case DATA_FORMAT_R16G16_UNORM:
		case DATA_FORMAT_R16G16_SNORM:
		case DATA_FORMAT_R16G16_UINT:
		case DATA_FORMAT_R16G16_SINT:
		case DATA_FORMAT_R16G16_SFLOAT:
			return 4;
		case DATA_FORMAT_R16G16B16_UNORM:
		case DATA_FORMAT_R16G16B16_SNORM:
		case DATA_FORMAT_R16G16B16_UINT:
		case DATA_FORMAT_R16G16B16_SINT:
		case DATA_FORMAT_R16G16B16_SFLOAT:
			return 8;
		case DATA_FORMAT_R16G16B16A16_UNORM:
		case DATA_FORMAT_R16G16B16A16_SNORM:
		case DATA_FORMAT_R16G16B16A16_UINT:
		case DATA_FORMAT_R16G16B16A16_SINT:
		case DATA_FORMAT_R16G16B16A16_SFLOAT:
			return 8;
		case DATA_FORMAT_R32_UINT:
		case DATA_FORMAT_R32_SINT:
		case DATA_FORMAT_R32_SFLOAT:
			return 4;
		case DATA_FORMAT_R32G32_UINT:
		case DATA_FORMAT_R32G32_SINT:
		case DATA_FORMAT_R32G32_SFLOAT:
			return 8;
		case DATA_FORMAT_R32G32B32_UINT:
		case DATA_FORMAT_R32G32B32_SINT:
		case DATA_FORMAT_R32G32B32_SFLOAT:
			return 12;
		case DATA_FORMAT_R32G32B32A32_UINT:
		case DATA_FORMAT_R32G32B32A32_SINT:
		case DATA_FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case DATA_FORMAT_R64_UINT:
		case DATA_FORMAT_R64_SINT:
		case DATA_FORMAT_R64_SFLOAT:
			return 8;
		case DATA_FORMAT_R64G64_UINT:
		case DATA_FORMAT_R64G64_SINT:
		case DATA_FORMAT_R64G64_SFLOAT:
			return 16;
		case DATA_FORMAT_R64G64B64_UINT:
		case DATA_FORMAT_R64G64B64_SINT:
		case DATA_FORMAT_R64G64B64_SFLOAT:
			return 24;
		case DATA_FORMAT_R64G64B64A64_UINT:
		case DATA_FORMAT_R64G64B64A64_SINT:
		case DATA_FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		default:
			return 0;
	}
}

/****************/
/**** SHADER ****/
/****************/

const char *RenderingDeviceCommons::SHADER_STAGE_NAMES[SHADER_STAGE_MAX] = {
	"Vertex",
	"Fragment",
	"TesselationControl",
	"TesselationEvaluation",
	"Compute",
};
