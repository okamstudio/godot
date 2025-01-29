/**************************************************************************/
/*  pixel_formats.mm                                                      */
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

/**************************************************************************/
/*                                                                        */
/* Portions of this code were derived from MoltenVK.                      */
/*                                                                        */
/* Copyright (c) 2015-2023 The Brenwill Workshop Ltd.                     */
/* (http://www.brenwill.com)                                              */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#import "pixel_formats.h"

#import "metal_utils.h"

#if TARGET_OS_IPHONE || TARGET_OS_TV
#if !(__IPHONE_OS_VERSION_MAX_ALLOWED >= 160400) // iOS/tvOS 16.4
#define MTLPixelFormatBC1_RGBA MTLPixelFormatInvalid
#define MTLPixelFormatBC1_RGBA_sRGB MTLPixelFormatInvalid
#define MTLPixelFormatBC2_RGBA MTLPixelFormatInvalid
#define MTLPixelFormatBC2_RGBA_sRGB MTLPixelFormatInvalid
#define MTLPixelFormatBC3_RGBA MTLPixelFormatInvalid
#define MTLPixelFormatBC3_RGBA_sRGB MTLPixelFormatInvalid
#define MTLPixelFormatBC4_RUnorm MTLPixelFormatInvalid
#define MTLPixelFormatBC4_RSnorm MTLPixelFormatInvalid
#define MTLPixelFormatBC5_RGUnorm MTLPixelFormatInvalid
#define MTLPixelFormatBC5_RGSnorm MTLPixelFormatInvalid
#define MTLPixelFormatBC6H_RGBUfloat MTLPixelFormatInvalid
#define MTLPixelFormatBC6H_RGBFloat MTLPixelFormatInvalid
#define MTLPixelFormatBC7_RGBAUnorm MTLPixelFormatInvalid
#define MTLPixelFormatBC7_RGBAUnorm_sRGB MTLPixelFormatInvalid
#endif

#define MTLPixelFormatDepth16Unorm_Stencil8 MTLPixelFormatDepth32Float_Stencil8
#define MTLPixelFormatDepth24Unorm_Stencil8 MTLPixelFormatInvalid
#define MTLPixelFormatX24_Stencil8 MTLPixelFormatInvalid
#endif

#if TARGET_OS_TV
#define MTLPixelFormatASTC_4x4_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_5x4_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_5x5_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_6x5_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_6x6_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_8x5_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_8x6_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_8x8_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_10x5_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_10x6_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_10x8_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_10x10_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_12x10_HDR MTLPixelFormatInvalid
#define MTLPixelFormatASTC_12x12_HDR MTLPixelFormatInvalid
#endif

#if !((__MAC_OS_X_VERSION_MAX_ALLOWED >= 140000) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 170000)) // Xcode 15
#define MTLVertexFormatFloatRG11B10 MTLVertexFormatInvalid
#define MTLVertexFormatFloatRGB9E5 MTLVertexFormatInvalid
#endif

template <typename T>
void clear(T *p_val, size_t p_count = 1) {
	memset(p_val, 0, sizeof(T) * p_count);
}

#pragma mark -
#pragma mark PixelFormats

bool PixelFormats::isSupported(DataFormat p_format) {
	return getDataFormatDesc(p_format).isSupported();
}

bool PixelFormats::isSupportedOrSubstitutable(DataFormat p_format) {
	return getDataFormatDesc(p_format).isSupportedOrSubstitutable();
}

bool PixelFormats::isPVRTCFormat(MTLPixelFormat p_format) {
	switch (p_format) {
		case MTLPixelFormatPVRTC_RGBA_2BPP:
		case MTLPixelFormatPVRTC_RGBA_2BPP_sRGB:
		case MTLPixelFormatPVRTC_RGBA_4BPP:
		case MTLPixelFormatPVRTC_RGBA_4BPP_sRGB:
		case MTLPixelFormatPVRTC_RGB_2BPP:
		case MTLPixelFormatPVRTC_RGB_2BPP_sRGB:
		case MTLPixelFormatPVRTC_RGB_4BPP:
		case MTLPixelFormatPVRTC_RGB_4BPP_sRGB:
			return true;
		default:
			return false;
	}
}

MTLFormatType PixelFormats::getFormatType(DataFormat p_format) {
	return getDataFormatDesc(p_format).formatType;
}

MTLFormatType PixelFormats::getFormatType(MTLPixelFormat p_format) {
	return getDataFormatDesc(p_format).formatType;
}

MTLPixelFormat PixelFormats::getMTLPixelFormat(DataFormat p_format) {
	DataFormatDesc &dfDesc = getDataFormatDesc(p_format);
	MTLPixelFormat mtlPixFmt = dfDesc.mtlPixelFormat;

	// If the MTLPixelFormat is not supported but DataFormat is valid,
	// attempt to substitute a different format.
	if (mtlPixFmt == MTLPixelFormatInvalid && p_format != RD::DATA_FORMAT_MAX && dfDesc.chromaSubsamplingPlaneCount <= 1) {
		mtlPixFmt = dfDesc.mtlPixelFormatSubstitute;
	}

	return mtlPixFmt;
}

RD::DataFormat PixelFormats::getDataFormat(MTLPixelFormat p_format) {
	return getMTLPixelFormatDesc(p_format).dataFormat;
}

uint32_t PixelFormats::getBytesPerBlock(DataFormat p_format) {
	return getDataFormatDesc(p_format).bytesPerBlock;
}

uint32_t PixelFormats::getBytesPerBlock(MTLPixelFormat p_format) {
	return getDataFormatDesc(p_format).bytesPerBlock;
}

uint8_t PixelFormats::getChromaSubsamplingPlaneCount(DataFormat p_format) {
	return getDataFormatDesc(p_format).chromaSubsamplingPlaneCount;
}

uint8_t PixelFormats::getChromaSubsamplingComponentBits(DataFormat p_format) {
	return getDataFormatDesc(p_format).chromaSubsamplingComponentBits;
}

float PixelFormats::getBytesPerTexel(DataFormat p_format) {
	return getDataFormatDesc(p_format).bytesPerTexel();
}

float PixelFormats::getBytesPerTexel(MTLPixelFormat p_format) {
	return getDataFormatDesc(p_format).bytesPerTexel();
}

size_t PixelFormats::getBytesPerRow(DataFormat p_format, uint32_t p_texels_per_row) {
	DataFormatDesc &dfDesc = getDataFormatDesc(p_format);
	return Math::division_round_up(p_texels_per_row, dfDesc.blockTexelSize.width) * dfDesc.bytesPerBlock;
}

size_t PixelFormats::getBytesPerRow(MTLPixelFormat p_format, uint32_t p_texels_per_row) {
	DataFormatDesc &dfDesc = getDataFormatDesc(p_format);
	return Math::division_round_up(p_texels_per_row, dfDesc.blockTexelSize.width) * dfDesc.bytesPerBlock;
}

size_t PixelFormats::getBytesPerLayer(DataFormat p_format, size_t p_bytes_per_row, uint32_t p_texel_rows_per_layer) {
	return Math::division_round_up(p_texel_rows_per_layer, getDataFormatDesc(p_format).blockTexelSize.height) * p_bytes_per_row;
}

size_t PixelFormats::getBytesPerLayer(MTLPixelFormat p_format, size_t p_bytes_per_row, uint32_t p_texel_rows_per_layer) {
	return Math::division_round_up(p_texel_rows_per_layer, getDataFormatDesc(p_format).blockTexelSize.height) * p_bytes_per_row;
}

bool PixelFormats::needsSwizzle(DataFormat p_format) {
	return getDataFormatDesc(p_format).needsSwizzle();
}

MTLFmtCaps PixelFormats::getCapabilities(DataFormat p_format, bool p_extended) {
	return getCapabilities(getDataFormatDesc(p_format).mtlPixelFormat, p_extended);
}

MTLFmtCaps PixelFormats::getCapabilities(MTLPixelFormat p_format, bool p_extended) {
	MTLFormatDesc &mtlDesc = getMTLPixelFormatDesc(p_format);
	MTLFmtCaps caps = mtlDesc.mtlFmtCaps;
	if (!p_extended || mtlDesc.mtlViewClass == MTLViewClass::None) {
		return caps;
	}
	// Now get caps of all formats in the view class.
	for (MTLFormatDesc &otherDesc : _mtl_vertex_format_descs) {
		if (otherDesc.mtlViewClass == mtlDesc.mtlViewClass) {
			caps |= otherDesc.mtlFmtCaps;
		}
	}
	return caps;
}

MTLVertexFormat PixelFormats::getMTLVertexFormat(DataFormat p_format) {
	DataFormatDesc &dfDesc = getDataFormatDesc(p_format);
	MTLVertexFormat format = dfDesc.mtlVertexFormat;

	if (format == MTLVertexFormatInvalid) {
		String errMsg;
		errMsg += "DataFormat ";
		errMsg += dfDesc.name;
		errMsg += " is not supported for vertex buffers on this device.";

		if (dfDesc.vertexIsSupportedOrSubstitutable()) {
			format = dfDesc.mtlVertexFormatSubstitute;

			DataFormatDesc &dfDescSubs = getDataFormatDesc(getMTLVertexFormatDesc(format).dataFormat);
			errMsg += " Using DataFormat ";
			errMsg += dfDescSubs.name;
			errMsg += " instead.";
		}
		WARN_PRINT(errMsg);
	}

	return format;
}

DataFormatDesc &PixelFormats::getDataFormatDesc(DataFormat p_format) {
	return _data_format_descs[p_format];
}

DataFormatDesc &PixelFormats::getDataFormatDesc(MTLPixelFormat p_format) {
	return getDataFormatDesc(getMTLPixelFormatDesc(p_format).dataFormat);
}

// Return a reference to the Metal format descriptor corresponding to the MTLPixelFormat.
MTLFormatDesc &PixelFormats::getMTLPixelFormatDesc(MTLPixelFormat p_format) {
	return _mtl_pixel_format_descs[p_format];
}

// Return a reference to the Metal format descriptor corresponding to the MTLVertexFormat.
MTLFormatDesc &PixelFormats::getMTLVertexFormatDesc(MTLVertexFormat p_format) {
	return _mtl_vertex_format_descs[p_format];
}

PixelFormats::PixelFormats(id<MTLDevice> p_device, const MetalFeatures &p_feat) :
		device(p_device) {
	initMTLPixelFormatCapabilities();
	initMTLVertexFormatCapabilities(p_feat);
	modifyMTLFormatCapabilities(p_feat);

	initDataFormatCapabilities();
	buildDFFormatMaps();
}

#define addDataFormatDescFull(DATA_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE, SWIZ_R, SWIZ_G, SWIZ_B, SWIZ_A) \
	dfFmt = RD::DATA_FORMAT_##DATA_FMT;                                                                                                                                           \
	_data_format_descs[dfFmt] = { dfFmt, MTLPixelFormat##MTL_FMT, MTLPixelFormat##MTL_FMT_ALT, MTLVertexFormat##MTL_VTX_FMT, MTLVertexFormat##MTL_VTX_FMT_ALT,                    \
		CSPC, CSCB, { BLK_W, BLK_H }, BLK_BYTE_CNT, MTLFormatType::MVK_FMT_TYPE,                                                                                                  \
		{ RD::TEXTURE_SWIZZLE_##SWIZ_R, RD::TEXTURE_SWIZZLE_##SWIZ_G, RD::TEXTURE_SWIZZLE_##SWIZ_B, RD::TEXTURE_SWIZZLE_##SWIZ_A },                                               \
		"DATA_FORMAT_" #DATA_FMT, false }

#define addDataFormatDesc(VK_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE) \
	addDataFormatDescFull(VK_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, 0, 0, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE, IDENTITY, IDENTITY, IDENTITY, IDENTITY)

#define addDataFormatDescSwizzled(VK_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE, SWIZ_R, SWIZ_G, SWIZ_B, SWIZ_A) \
	addDataFormatDescFull(VK_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, 0, 0, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE, SWIZ_R, SWIZ_G, SWIZ_B, SWIZ_A)

#define addDfFormatDescChromaSubsampling(DATA_FMT, MTL_FMT, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT) \
	addDataFormatDescFull(DATA_FMT, MTL_FMT, Invalid, Invalid, Invalid, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT, ColorFloat, IDENTITY, IDENTITY, IDENTITY, IDENTITY)

void PixelFormats::initDataFormatCapabilities() {
	_data_format_descs.reserve(RD::DATA_FORMAT_MAX + 1); // reserve enough space to avoid reallocs
	DataFormat dfFmt;

	addDataFormatDesc(R4G4_UNORM_PACK8, Invalid, Invalid, Invalid, Invalid, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R4G4_UNORM_PACK8, Invalid, Invalid, Invalid, Invalid, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R4G4B4A4_UNORM_PACK16, ABGR4Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDescSwizzled(B4G4R4A4_UNORM_PACK16, Invalid, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat, B, G, R, A);

	addDataFormatDesc(R5G6B5_UNORM_PACK16, B5G6R5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDescSwizzled(B5G6R5_UNORM_PACK16, B5G6R5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat, B, G, R, A);
	addDataFormatDesc(R5G5B5A1_UNORM_PACK16, A1BGR5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDescSwizzled(B5G5R5A1_UNORM_PACK16, A1BGR5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat, B, G, R, A);
	addDataFormatDesc(A1R5G5B5_UNORM_PACK16, BGR5A1Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);

	addDataFormatDesc(R8_UNORM, R8Unorm, Invalid, UCharNormalized, UChar2Normalized, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R8_SNORM, R8Snorm, Invalid, CharNormalized, Char2Normalized, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R8_USCALED, Invalid, Invalid, UChar, UChar2, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R8_SSCALED, Invalid, Invalid, Char, Char2, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R8_UINT, R8Uint, Invalid, UChar, UChar2, 1, 1, 1, ColorUInt8);
	addDataFormatDesc(R8_SINT, R8Sint, Invalid, Char, Char2, 1, 1, 1, ColorInt8);
	addDataFormatDesc(R8_SRGB, R8Unorm_sRGB, Invalid, UCharNormalized, UChar2Normalized, 1, 1, 1, ColorFloat);

	addDataFormatDesc(R8G8_UNORM, RG8Unorm, Invalid, UChar2Normalized, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R8G8_SNORM, RG8Snorm, Invalid, Char2Normalized, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R8G8_USCALED, Invalid, Invalid, UChar2, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R8G8_SSCALED, Invalid, Invalid, Char2, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R8G8_UINT, RG8Uint, Invalid, UChar2, Invalid, 1, 1, 2, ColorUInt8);
	addDataFormatDesc(R8G8_SINT, RG8Sint, Invalid, Char2, Invalid, 1, 1, 2, ColorInt8);
	addDataFormatDesc(R8G8_SRGB, RG8Unorm_sRGB, Invalid, UChar2Normalized, Invalid, 1, 1, 2, ColorFloat);

	addDataFormatDesc(R8G8B8_UNORM, Invalid, Invalid, UChar3Normalized, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(R8G8B8_SNORM, Invalid, Invalid, Char3Normalized, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(R8G8B8_USCALED, Invalid, Invalid, UChar3, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(R8G8B8_SSCALED, Invalid, Invalid, Char3, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(R8G8B8_UINT, Invalid, Invalid, UChar3, Invalid, 1, 1, 3, ColorUInt8);
	addDataFormatDesc(R8G8B8_SINT, Invalid, Invalid, Char3, Invalid, 1, 1, 3, ColorInt8);
	addDataFormatDesc(R8G8B8_SRGB, Invalid, Invalid, UChar3Normalized, Invalid, 1, 1, 3, ColorFloat);

	addDataFormatDesc(B8G8R8_UNORM, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(B8G8R8_SNORM, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(B8G8R8_USCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(B8G8R8_SSCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorFloat);
	addDataFormatDesc(B8G8R8_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorUInt8);
	addDataFormatDesc(B8G8R8_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorInt8);
	addDataFormatDesc(B8G8R8_SRGB, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, ColorFloat);

	addDataFormatDesc(R8G8B8A8_UNORM, RGBA8Unorm, Invalid, UChar4Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R8G8B8A8_SNORM, RGBA8Snorm, Invalid, Char4Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R8G8B8A8_USCALED, Invalid, Invalid, UChar4, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R8G8B8A8_SSCALED, Invalid, Invalid, Char4, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R8G8B8A8_UINT, RGBA8Uint, Invalid, UChar4, Invalid, 1, 1, 4, ColorUInt8);
	addDataFormatDesc(R8G8B8A8_SINT, RGBA8Sint, Invalid, Char4, Invalid, 1, 1, 4, ColorInt8);
	addDataFormatDesc(R8G8B8A8_SRGB, RGBA8Unorm_sRGB, Invalid, UChar4Normalized, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(B8G8R8A8_UNORM, BGRA8Unorm, Invalid, UChar4Normalized_BGRA, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDescSwizzled(B8G8R8A8_SNORM, RGBA8Snorm, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat, B, G, R, A);
	addDataFormatDesc(B8G8R8A8_USCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(B8G8R8A8_SSCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDescSwizzled(B8G8R8A8_UINT, RGBA8Uint, Invalid, Invalid, Invalid, 1, 1, 4, ColorUInt8, B, G, R, A);
	addDataFormatDescSwizzled(B8G8R8A8_SINT, RGBA8Sint, Invalid, Invalid, Invalid, 1, 1, 4, ColorInt8, B, G, R, A);
	addDataFormatDesc(B8G8R8A8_SRGB, BGRA8Unorm_sRGB, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(A8B8G8R8_UNORM_PACK32, RGBA8Unorm, Invalid, UChar4Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A8B8G8R8_SNORM_PACK32, RGBA8Snorm, Invalid, Char4Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A8B8G8R8_USCALED_PACK32, Invalid, Invalid, UChar4, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A8B8G8R8_SSCALED_PACK32, Invalid, Invalid, Char4, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A8B8G8R8_UINT_PACK32, RGBA8Uint, Invalid, UChar4, Invalid, 1, 1, 4, ColorUInt8);
	addDataFormatDesc(A8B8G8R8_SINT_PACK32, RGBA8Sint, Invalid, Char4, Invalid, 1, 1, 4, ColorInt8);
	addDataFormatDesc(A8B8G8R8_SRGB_PACK32, RGBA8Unorm_sRGB, Invalid, UChar4Normalized, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(A2R10G10B10_UNORM_PACK32, BGR10A2Unorm, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2R10G10B10_SNORM_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2R10G10B10_USCALED_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2R10G10B10_SSCALED_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2R10G10B10_UINT_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorUInt16);
	addDataFormatDesc(A2R10G10B10_SINT_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorInt16);

	addDataFormatDesc(A2B10G10R10_UNORM_PACK32, RGB10A2Unorm, Invalid, UInt1010102Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2B10G10R10_SNORM_PACK32, Invalid, Invalid, Int1010102Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2B10G10R10_USCALED_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2B10G10R10_SSCALED_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(A2B10G10R10_UINT_PACK32, RGB10A2Uint, Invalid, Invalid, Invalid, 1, 1, 4, ColorUInt16);
	addDataFormatDesc(A2B10G10R10_SINT_PACK32, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorInt16);

	addDataFormatDesc(R16_UNORM, R16Unorm, Invalid, UShortNormalized, UShort2Normalized, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R16_SNORM, R16Snorm, Invalid, ShortNormalized, Short2Normalized, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R16_USCALED, Invalid, Invalid, UShort, UShort2, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R16_SSCALED, Invalid, Invalid, Short, Short2, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R16_UINT, R16Uint, Invalid, UShort, UShort2, 1, 1, 2, ColorUInt16);
	addDataFormatDesc(R16_SINT, R16Sint, Invalid, Short, Short2, 1, 1, 2, ColorInt16);
	addDataFormatDesc(R16_SFLOAT, R16Float, Invalid, Half, Half2, 1, 1, 2, ColorFloat);

	addDataFormatDesc(R16G16_UNORM, RG16Unorm, Invalid, UShort2Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R16G16_SNORM, RG16Snorm, Invalid, Short2Normalized, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R16G16_USCALED, Invalid, Invalid, UShort2, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R16G16_SSCALED, Invalid, Invalid, Short2, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(R16G16_UINT, RG16Uint, Invalid, UShort2, Invalid, 1, 1, 4, ColorUInt16);
	addDataFormatDesc(R16G16_SINT, RG16Sint, Invalid, Short2, Invalid, 1, 1, 4, ColorInt16);
	addDataFormatDesc(R16G16_SFLOAT, RG16Float, Invalid, Half2, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(R16G16B16_UNORM, Invalid, Invalid, UShort3Normalized, Invalid, 1, 1, 6, ColorFloat);
	addDataFormatDesc(R16G16B16_SNORM, Invalid, Invalid, Short3Normalized, Invalid, 1, 1, 6, ColorFloat);
	addDataFormatDesc(R16G16B16_USCALED, Invalid, Invalid, UShort3, Invalid, 1, 1, 6, ColorFloat);
	addDataFormatDesc(R16G16B16_SSCALED, Invalid, Invalid, Short3, Invalid, 1, 1, 6, ColorFloat);
	addDataFormatDesc(R16G16B16_UINT, Invalid, Invalid, UShort3, Invalid, 1, 1, 6, ColorUInt16);
	addDataFormatDesc(R16G16B16_SINT, Invalid, Invalid, Short3, Invalid, 1, 1, 6, ColorInt16);
	addDataFormatDesc(R16G16B16_SFLOAT, Invalid, Invalid, Half3, Invalid, 1, 1, 6, ColorFloat);

	addDataFormatDesc(R16G16B16A16_UNORM, RGBA16Unorm, Invalid, UShort4Normalized, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R16G16B16A16_SNORM, RGBA16Snorm, Invalid, Short4Normalized, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R16G16B16A16_USCALED, Invalid, Invalid, UShort4, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R16G16B16A16_SSCALED, Invalid, Invalid, Short4, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R16G16B16A16_UINT, RGBA16Uint, Invalid, UShort4, Invalid, 1, 1, 8, ColorUInt16);
	addDataFormatDesc(R16G16B16A16_SINT, RGBA16Sint, Invalid, Short4, Invalid, 1, 1, 8, ColorInt16);
	addDataFormatDesc(R16G16B16A16_SFLOAT, RGBA16Float, Invalid, Half4, Invalid, 1, 1, 8, ColorFloat);

	addDataFormatDesc(R32_UINT, R32Uint, Invalid, UInt, Invalid, 1, 1, 4, ColorUInt32);
	addDataFormatDesc(R32_SINT, R32Sint, Invalid, Int, Invalid, 1, 1, 4, ColorInt32);
	addDataFormatDesc(R32_SFLOAT, R32Float, Invalid, Float, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(R32G32_UINT, RG32Uint, Invalid, UInt2, Invalid, 1, 1, 8, ColorUInt32);
	addDataFormatDesc(R32G32_SINT, RG32Sint, Invalid, Int2, Invalid, 1, 1, 8, ColorInt32);
	addDataFormatDesc(R32G32_SFLOAT, RG32Float, Invalid, Float2, Invalid, 1, 1, 8, ColorFloat);

	addDataFormatDesc(R32G32B32_UINT, Invalid, Invalid, UInt3, Invalid, 1, 1, 12, ColorUInt32);
	addDataFormatDesc(R32G32B32_SINT, Invalid, Invalid, Int3, Invalid, 1, 1, 12, ColorInt32);
	addDataFormatDesc(R32G32B32_SFLOAT, Invalid, Invalid, Float3, Invalid, 1, 1, 12, ColorFloat);

	addDataFormatDesc(R32G32B32A32_UINT, RGBA32Uint, Invalid, UInt4, Invalid, 1, 1, 16, ColorUInt32);
	addDataFormatDesc(R32G32B32A32_SINT, RGBA32Sint, Invalid, Int4, Invalid, 1, 1, 16, ColorInt32);
	addDataFormatDesc(R32G32B32A32_SFLOAT, RGBA32Float, Invalid, Float4, Invalid, 1, 1, 16, ColorFloat);

	addDataFormatDesc(R64_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R64_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 8, ColorFloat);
	addDataFormatDesc(R64_SFLOAT, Invalid, Invalid, Invalid, Invalid, 1, 1, 8, ColorFloat);

	addDataFormatDesc(R64G64_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 16, ColorFloat);
	addDataFormatDesc(R64G64_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 16, ColorFloat);
	addDataFormatDesc(R64G64_SFLOAT, Invalid, Invalid, Invalid, Invalid, 1, 1, 16, ColorFloat);

	addDataFormatDesc(R64G64B64_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 24, ColorFloat);
	addDataFormatDesc(R64G64B64_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 24, ColorFloat);
	addDataFormatDesc(R64G64B64_SFLOAT, Invalid, Invalid, Invalid, Invalid, 1, 1, 24, ColorFloat);

	addDataFormatDesc(R64G64B64A64_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 32, ColorFloat);
	addDataFormatDesc(R64G64B64A64_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 32, ColorFloat);
	addDataFormatDesc(R64G64B64A64_SFLOAT, Invalid, Invalid, Invalid, Invalid, 1, 1, 32, ColorFloat);

	addDataFormatDesc(B10G11R11_UFLOAT_PACK32, RG11B10Float, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(E5B9G9R9_UFLOAT_PACK32, RGB9E5Float, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);

	addDataFormatDesc(D32_SFLOAT, Depth32Float, Invalid, Invalid, Invalid, 1, 1, 4, DepthStencil);
	addDataFormatDesc(D32_SFLOAT_S8_UINT, Depth32Float_Stencil8, Invalid, Invalid, Invalid, 1, 1, 5, DepthStencil);

	addDataFormatDesc(S8_UINT, Stencil8, Invalid, Invalid, Invalid, 1, 1, 1, DepthStencil);

	addDataFormatDesc(D16_UNORM, Depth16Unorm, Depth32Float, Invalid, Invalid, 1, 1, 2, DepthStencil);
	addDataFormatDesc(D16_UNORM_S8_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 3, DepthStencil);
	addDataFormatDesc(D24_UNORM_S8_UINT, Depth24Unorm_Stencil8, Depth32Float_Stencil8, Invalid, Invalid, 1, 1, 4, DepthStencil);

	addDataFormatDesc(X8_D24_UNORM_PACK32, Invalid, Depth24Unorm_Stencil8, Invalid, Invalid, 1, 1, 4, DepthStencil);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	addDataFormatDesc(BC1_RGB_UNORM_BLOCK, BC1_RGBA, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(BC1_RGB_SRGB_BLOCK, BC1_RGBA_sRGB, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(BC1_RGBA_UNORM_BLOCK, BC1_RGBA, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(BC1_RGBA_SRGB_BLOCK, BC1_RGBA_sRGB, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);

	addDataFormatDesc(BC2_UNORM_BLOCK, BC2_RGBA, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(BC2_SRGB_BLOCK, BC2_RGBA_sRGB, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(BC3_UNORM_BLOCK, BC3_RGBA, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(BC3_SRGB_BLOCK, BC3_RGBA_sRGB, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(BC4_UNORM_BLOCK, BC4_RUnorm, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(BC4_SNORM_BLOCK, BC4_RSnorm, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);

	addDataFormatDesc(BC5_UNORM_BLOCK, BC5_RGUnorm, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(BC5_SNORM_BLOCK, BC5_RGSnorm, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(BC6H_UFLOAT_BLOCK, BC6H_RGBUfloat, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(BC6H_SFLOAT_BLOCK, BC6H_RGBFloat, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(BC7_UNORM_BLOCK, BC7_RGBAUnorm, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(BC7_SRGB_BLOCK, BC7_RGBAUnorm_sRGB, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

#pragma clang diagnostic pop

	addDataFormatDesc(ETC2_R8G8B8_UNORM_BLOCK, ETC2_RGB8, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(ETC2_R8G8B8_SRGB_BLOCK, ETC2_RGB8_sRGB, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(ETC2_R8G8B8A1_UNORM_BLOCK, ETC2_RGB8A1, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(ETC2_R8G8B8A1_SRGB_BLOCK, ETC2_RGB8A1_sRGB, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);

	addDataFormatDesc(ETC2_R8G8B8A8_UNORM_BLOCK, EAC_RGBA8, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(ETC2_R8G8B8A8_SRGB_BLOCK, EAC_RGBA8_sRGB, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(EAC_R11_UNORM_BLOCK, EAC_R11Unorm, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);
	addDataFormatDesc(EAC_R11_SNORM_BLOCK, EAC_R11Snorm, Invalid, Invalid, Invalid, 4, 4, 8, Compressed);

	addDataFormatDesc(EAC_R11G11_UNORM_BLOCK, EAC_RG11Unorm, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(EAC_R11G11_SNORM_BLOCK, EAC_RG11Snorm, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);

	addDataFormatDesc(ASTC_4x4_UNORM_BLOCK, ASTC_4x4_LDR, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(ASTC_4x4_SRGB_BLOCK, ASTC_4x4_sRGB, Invalid, Invalid, Invalid, 4, 4, 16, Compressed);
	addDataFormatDesc(ASTC_5x4_UNORM_BLOCK, ASTC_5x4_LDR, Invalid, Invalid, Invalid, 5, 4, 16, Compressed);
	addDataFormatDesc(ASTC_5x4_SRGB_BLOCK, ASTC_5x4_sRGB, Invalid, Invalid, Invalid, 5, 4, 16, Compressed);
	addDataFormatDesc(ASTC_5x5_UNORM_BLOCK, ASTC_5x5_LDR, Invalid, Invalid, Invalid, 5, 5, 16, Compressed);
	addDataFormatDesc(ASTC_5x5_SRGB_BLOCK, ASTC_5x5_sRGB, Invalid, Invalid, Invalid, 5, 5, 16, Compressed);
	addDataFormatDesc(ASTC_6x5_UNORM_BLOCK, ASTC_6x5_LDR, Invalid, Invalid, Invalid, 6, 5, 16, Compressed);
	addDataFormatDesc(ASTC_6x5_SRGB_BLOCK, ASTC_6x5_sRGB, Invalid, Invalid, Invalid, 6, 5, 16, Compressed);
	addDataFormatDesc(ASTC_6x6_UNORM_BLOCK, ASTC_6x6_LDR, Invalid, Invalid, Invalid, 6, 6, 16, Compressed);
	addDataFormatDesc(ASTC_6x6_SRGB_BLOCK, ASTC_6x6_sRGB, Invalid, Invalid, Invalid, 6, 6, 16, Compressed);
	addDataFormatDesc(ASTC_8x5_UNORM_BLOCK, ASTC_8x5_LDR, Invalid, Invalid, Invalid, 8, 5, 16, Compressed);
	addDataFormatDesc(ASTC_8x5_SRGB_BLOCK, ASTC_8x5_sRGB, Invalid, Invalid, Invalid, 8, 5, 16, Compressed);
	addDataFormatDesc(ASTC_8x6_UNORM_BLOCK, ASTC_8x6_LDR, Invalid, Invalid, Invalid, 8, 6, 16, Compressed);
	addDataFormatDesc(ASTC_8x6_SRGB_BLOCK, ASTC_8x6_sRGB, Invalid, Invalid, Invalid, 8, 6, 16, Compressed);
	addDataFormatDesc(ASTC_8x8_UNORM_BLOCK, ASTC_8x8_LDR, Invalid, Invalid, Invalid, 8, 8, 16, Compressed);
	addDataFormatDesc(ASTC_8x8_SRGB_BLOCK, ASTC_8x8_sRGB, Invalid, Invalid, Invalid, 8, 8, 16, Compressed);
	addDataFormatDesc(ASTC_10x5_UNORM_BLOCK, ASTC_10x5_LDR, Invalid, Invalid, Invalid, 10, 5, 16, Compressed);
	addDataFormatDesc(ASTC_10x5_SRGB_BLOCK, ASTC_10x5_sRGB, Invalid, Invalid, Invalid, 10, 5, 16, Compressed);
	addDataFormatDesc(ASTC_10x6_UNORM_BLOCK, ASTC_10x6_LDR, Invalid, Invalid, Invalid, 10, 6, 16, Compressed);
	addDataFormatDesc(ASTC_10x6_SRGB_BLOCK, ASTC_10x6_sRGB, Invalid, Invalid, Invalid, 10, 6, 16, Compressed);
	addDataFormatDesc(ASTC_10x8_UNORM_BLOCK, ASTC_10x8_LDR, Invalid, Invalid, Invalid, 10, 8, 16, Compressed);
	addDataFormatDesc(ASTC_10x8_SRGB_BLOCK, ASTC_10x8_sRGB, Invalid, Invalid, Invalid, 10, 8, 16, Compressed);
	addDataFormatDesc(ASTC_10x10_UNORM_BLOCK, ASTC_10x10_LDR, Invalid, Invalid, Invalid, 10, 10, 16, Compressed);
	addDataFormatDesc(ASTC_10x10_SRGB_BLOCK, ASTC_10x10_sRGB, Invalid, Invalid, Invalid, 10, 10, 16, Compressed);
	addDataFormatDesc(ASTC_12x10_UNORM_BLOCK, ASTC_12x10_LDR, Invalid, Invalid, Invalid, 12, 10, 16, Compressed);
	addDataFormatDesc(ASTC_12x10_SRGB_BLOCK, ASTC_12x10_sRGB, Invalid, Invalid, Invalid, 12, 10, 16, Compressed);
	addDataFormatDesc(ASTC_12x12_UNORM_BLOCK, ASTC_12x12_LDR, Invalid, Invalid, Invalid, 12, 12, 16, Compressed);
	addDataFormatDesc(ASTC_12x12_SRGB_BLOCK, ASTC_12x12_sRGB, Invalid, Invalid, Invalid, 12, 12, 16, Compressed);

	addDfFormatDescChromaSubsampling(G8B8G8R8_422_UNORM, GBGR422, 1, 8, 2, 1, 4);
	addDfFormatDescChromaSubsampling(B8G8R8G8_422_UNORM, BGRG422, 1, 8, 2, 1, 4);
	addDfFormatDescChromaSubsampling(G8_B8_R8_3PLANE_420_UNORM, Invalid, 3, 8, 2, 2, 6);
	addDfFormatDescChromaSubsampling(G8_B8R8_2PLANE_420_UNORM, Invalid, 2, 8, 2, 2, 6);
	addDfFormatDescChromaSubsampling(G8_B8_R8_3PLANE_422_UNORM, Invalid, 3, 8, 2, 1, 4);
	addDfFormatDescChromaSubsampling(G8_B8R8_2PLANE_422_UNORM, Invalid, 2, 8, 2, 1, 4);
	addDfFormatDescChromaSubsampling(G8_B8_R8_3PLANE_444_UNORM, Invalid, 3, 8, 1, 1, 3);
	addDfFormatDescChromaSubsampling(R10X6_UNORM_PACK16, R16Unorm, 0, 10, 1, 1, 2);
	addDfFormatDescChromaSubsampling(R10X6G10X6_UNORM_2PACK16, RG16Unorm, 0, 10, 1, 1, 4);
	addDfFormatDescChromaSubsampling(R10X6G10X6B10X6A10X6_UNORM_4PACK16, RGBA16Unorm, 0, 10, 1, 1, 8);
	addDfFormatDescChromaSubsampling(G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, Invalid, 1, 10, 2, 1, 8);
	addDfFormatDescChromaSubsampling(B10X6G10X6R10X6G10X6_422_UNORM_4PACK16, Invalid, 1, 10, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, Invalid, 3, 10, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, Invalid, 2, 10, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, Invalid, 3, 10, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, Invalid, 2, 10, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, Invalid, 3, 10, 1, 1, 6);
	addDfFormatDescChromaSubsampling(R12X4_UNORM_PACK16, R16Unorm, 0, 12, 1, 1, 2);
	addDfFormatDescChromaSubsampling(R12X4G12X4_UNORM_2PACK16, RG16Unorm, 0, 12, 1, 1, 4);
	addDfFormatDescChromaSubsampling(R12X4G12X4B12X4A12X4_UNORM_4PACK16, RGBA16Unorm, 0, 12, 1, 1, 8);
	addDfFormatDescChromaSubsampling(G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, Invalid, 1, 12, 2, 1, 8);
	addDfFormatDescChromaSubsampling(B12X4G12X4R12X4G12X4_422_UNORM_4PACK16, Invalid, 1, 12, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, Invalid, 3, 12, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, Invalid, 2, 12, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, Invalid, 3, 12, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, Invalid, 2, 12, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, Invalid, 3, 12, 1, 1, 6);
	addDfFormatDescChromaSubsampling(G16B16G16R16_422_UNORM, Invalid, 1, 16, 2, 1, 8);
	addDfFormatDescChromaSubsampling(B16G16R16G16_422_UNORM, Invalid, 1, 16, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G16_B16_R16_3PLANE_420_UNORM, Invalid, 3, 16, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G16_B16R16_2PLANE_420_UNORM, Invalid, 2, 16, 2, 2, 12);
	addDfFormatDescChromaSubsampling(G16_B16_R16_3PLANE_422_UNORM, Invalid, 3, 16, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G16_B16R16_2PLANE_422_UNORM, Invalid, 2, 16, 2, 1, 8);
	addDfFormatDescChromaSubsampling(G16_B16_R16_3PLANE_444_UNORM, Invalid, 3, 16, 1, 1, 6);
}

void PixelFormats::addMTLPixelFormatDescImpl(MTLPixelFormat p_pix_fmt, MTLPixelFormat p_pix_fmt_linear,
		MTLViewClass p_view_class, MTLFmtCaps p_fmt_caps, const char *p_name) {
	_mtl_pixel_format_descs[p_pix_fmt] = { .mtlPixelFormat = p_pix_fmt, DataFormat::DATA_FORMAT_MAX, p_fmt_caps, p_view_class, p_pix_fmt_linear, p_name };
}

#define addMTLPixelFormatDescFull(mtlFmt, mtlFmtLinear, viewClass, appleGPUCaps)                             \
	addMTLPixelFormatDescImpl(MTLPixelFormat##mtlFmt, MTLPixelFormat##mtlFmtLinear, MTLViewClass::viewClass, \
			appleGPUCaps, "MTLPixelFormat" #mtlFmt)

#define addMTLPixelFormatDesc(mtlFmt, viewClass, appleGPUCaps) \
	addMTLPixelFormatDescFull(mtlFmt, mtlFmt, viewClass, kMTLFmtCaps##appleGPUCaps)

#define addMTLPixelFormatDescSRGB(mtlFmt, viewClass, appleGPUCaps, mtlFmtLinear)               \
	/* Cannot write to sRGB textures in the simulator */                                       \
	if (TARGET_OS_SIMULATOR) {                                                                 \
		MTLFmtCaps appleFmtCaps = kMTLFmtCaps##appleGPUCaps;                                   \
		flags::clear(appleFmtCaps, kMTLFmtCapsWrite);                                          \
		addMTLPixelFormatDescFull(mtlFmt, mtlFmtLinear, viewClass, appleFmtCaps);              \
	} else {                                                                                   \
		addMTLPixelFormatDescFull(mtlFmt, mtlFmtLinear, viewClass, kMTLFmtCaps##appleGPUCaps); \
	}

void PixelFormats::initMTLPixelFormatCapabilities() {
	_mtl_pixel_format_descs.reserve(1024);

	// MTLPixelFormatInvalid must come first. Use addMTLPixelFormatDescImpl to avoid guard code.
	addMTLPixelFormatDescImpl(MTLPixelFormatInvalid, MTLPixelFormatInvalid, MTLViewClass::None, kMTLFmtCapsNone, "MTLPixelFormatInvalid");

	// Ordinary 8-bit pixel formats.
	addMTLPixelFormatDesc(A8Unorm, Color8, All);
	addMTLPixelFormatDesc(R8Unorm, Color8, All);
	addMTLPixelFormatDescSRGB(R8Unorm_sRGB, Color8, All, R8Unorm);
	addMTLPixelFormatDesc(R8Snorm, Color8, All);
	addMTLPixelFormatDesc(R8Uint, Color8, RWCM);
	addMTLPixelFormatDesc(R8Sint, Color8, RWCM);

	// Ordinary 16-bit pixel formats
	addMTLPixelFormatDesc(R16Unorm, Color16, RFWCMB);
	addMTLPixelFormatDesc(R16Snorm, Color16, RFWCMB);
	addMTLPixelFormatDesc(R16Uint, Color16, RWCM);
	addMTLPixelFormatDesc(R16Sint, Color16, RWCM);
	addMTLPixelFormatDesc(R16Float, Color16, All);

	addMTLPixelFormatDesc(RG8Unorm, Color16, All);
	addMTLPixelFormatDescSRGB(RG8Unorm_sRGB, Color16, All, RG8Unorm);
	addMTLPixelFormatDesc(RG8Snorm, Color16, All);
	addMTLPixelFormatDesc(RG8Uint, Color16, RWCM);
	addMTLPixelFormatDesc(RG8Sint, Color16, RWCM);

	// Packed 16-bit pixel formats
	addMTLPixelFormatDesc(B5G6R5Unorm, Color16, RFCMRB);
	addMTLPixelFormatDesc(A1BGR5Unorm, Color16, RFCMRB);
	addMTLPixelFormatDesc(ABGR4Unorm, Color16, RFCMRB);
	addMTLPixelFormatDesc(BGR5A1Unorm, Color16, RFCMRB);

	// Ordinary 32-bit pixel formats
	addMTLPixelFormatDesc(R32Uint, Color32, RWC);
	addMTLPixelFormatDesc(R32Sint, Color32, RWC);
	addMTLPixelFormatDesc(R32Float, Color32, All);

	addMTLPixelFormatDesc(RG16Unorm, Color32, RFWCMB);
	addMTLPixelFormatDesc(RG16Snorm, Color32, RFWCMB);
	addMTLPixelFormatDesc(RG16Uint, Color32, RWCM);
	addMTLPixelFormatDesc(RG16Sint, Color32, RWCM);
	addMTLPixelFormatDesc(RG16Float, Color32, All);

	addMTLPixelFormatDesc(RGBA8Unorm, Color32, All);
	addMTLPixelFormatDescSRGB(RGBA8Unorm_sRGB, Color32, All, RGBA8Unorm);
	addMTLPixelFormatDesc(RGBA8Snorm, Color32, All);
	addMTLPixelFormatDesc(RGBA8Uint, Color32, RWCM);
	addMTLPixelFormatDesc(RGBA8Sint, Color32, RWCM);

	addMTLPixelFormatDesc(BGRA8Unorm, Color32, All);
	addMTLPixelFormatDescSRGB(BGRA8Unorm_sRGB, Color32, All, BGRA8Unorm);

	// Packed 32-bit pixel formats
	addMTLPixelFormatDesc(RGB10A2Unorm, Color32, All);
	addMTLPixelFormatDesc(BGR10A2Unorm, Color32, All);
	addMTLPixelFormatDesc(RGB10A2Uint, Color32, RWCM);
	addMTLPixelFormatDesc(RG11B10Float, Color32, All);
	addMTLPixelFormatDesc(RGB9E5Float, Color32, All);

	// Ordinary 64-bit pixel formats
	addMTLPixelFormatDesc(RG32Uint, Color64, RWCM);
	addMTLPixelFormatDesc(RG32Sint, Color64, RWCM);
	addMTLPixelFormatDesc(RG32Float, Color64, All);

	addMTLPixelFormatDesc(RGBA16Unorm, Color64, RFWCMB);
	addMTLPixelFormatDesc(RGBA16Snorm, Color64, RFWCMB);
	addMTLPixelFormatDesc(RGBA16Uint, Color64, RWCM);
	addMTLPixelFormatDesc(RGBA16Sint, Color64, RWCM);
	addMTLPixelFormatDesc(RGBA16Float, Color64, All);

	// Ordinary 128-bit pixel formats
	addMTLPixelFormatDesc(RGBA32Uint, Color128, RWC);
	addMTLPixelFormatDesc(RGBA32Sint, Color128, RWC);
	addMTLPixelFormatDesc(RGBA32Float, Color128, All);

	// Compressed pixel formats
	addMTLPixelFormatDesc(PVRTC_RGBA_2BPP, PVRTC_RGBA_2BPP, RF);
	addMTLPixelFormatDescSRGB(PVRTC_RGBA_2BPP_sRGB, PVRTC_RGBA_2BPP, RF, PVRTC_RGBA_2BPP);
	addMTLPixelFormatDesc(PVRTC_RGBA_4BPP, PVRTC_RGBA_4BPP, RF);
	addMTLPixelFormatDescSRGB(PVRTC_RGBA_4BPP_sRGB, PVRTC_RGBA_4BPP, RF, PVRTC_RGBA_4BPP);

	addMTLPixelFormatDesc(ETC2_RGB8, ETC2_RGB8, RF);
	addMTLPixelFormatDescSRGB(ETC2_RGB8_sRGB, ETC2_RGB8, RF, ETC2_RGB8);
	addMTLPixelFormatDesc(ETC2_RGB8A1, ETC2_RGB8A1, RF);
	addMTLPixelFormatDescSRGB(ETC2_RGB8A1_sRGB, ETC2_RGB8A1, RF, ETC2_RGB8A1);
	addMTLPixelFormatDesc(EAC_RGBA8, EAC_RGBA8, RF);
	addMTLPixelFormatDescSRGB(EAC_RGBA8_sRGB, EAC_RGBA8, RF, EAC_RGBA8);
	addMTLPixelFormatDesc(EAC_R11Unorm, EAC_R11, RF);
	addMTLPixelFormatDesc(EAC_R11Snorm, EAC_R11, RF);
	addMTLPixelFormatDesc(EAC_RG11Unorm, EAC_RG11, RF);
	addMTLPixelFormatDesc(EAC_RG11Snorm, EAC_RG11, RF);

	addMTLPixelFormatDesc(ASTC_4x4_LDR, ASTC_4x4, RF);
	addMTLPixelFormatDescSRGB(ASTC_4x4_sRGB, ASTC_4x4, RF, ASTC_4x4_LDR);
	addMTLPixelFormatDesc(ASTC_4x4_HDR, ASTC_4x4, RF);
	addMTLPixelFormatDesc(ASTC_5x4_LDR, ASTC_5x4, RF);
	addMTLPixelFormatDescSRGB(ASTC_5x4_sRGB, ASTC_5x4, RF, ASTC_5x4_LDR);
	addMTLPixelFormatDesc(ASTC_5x4_HDR, ASTC_5x4, RF);
	addMTLPixelFormatDesc(ASTC_5x5_LDR, ASTC_5x5, RF);
	addMTLPixelFormatDescSRGB(ASTC_5x5_sRGB, ASTC_5x5, RF, ASTC_5x5_LDR);
	addMTLPixelFormatDesc(ASTC_5x5_HDR, ASTC_5x5, RF);
	addMTLPixelFormatDesc(ASTC_6x5_LDR, ASTC_6x5, RF);
	addMTLPixelFormatDescSRGB(ASTC_6x5_sRGB, ASTC_6x5, RF, ASTC_6x5_LDR);
	addMTLPixelFormatDesc(ASTC_6x5_HDR, ASTC_6x5, RF);
	addMTLPixelFormatDesc(ASTC_6x6_LDR, ASTC_6x6, RF);
	addMTLPixelFormatDescSRGB(ASTC_6x6_sRGB, ASTC_6x6, RF, ASTC_6x6_LDR);
	addMTLPixelFormatDesc(ASTC_6x6_HDR, ASTC_6x6, RF);
	addMTLPixelFormatDesc(ASTC_8x5_LDR, ASTC_8x5, RF);
	addMTLPixelFormatDescSRGB(ASTC_8x5_sRGB, ASTC_8x5, RF, ASTC_8x5_LDR);
	addMTLPixelFormatDesc(ASTC_8x5_HDR, ASTC_8x5, RF);
	addMTLPixelFormatDesc(ASTC_8x6_LDR, ASTC_8x6, RF);
	addMTLPixelFormatDescSRGB(ASTC_8x6_sRGB, ASTC_8x6, RF, ASTC_8x6_LDR);
	addMTLPixelFormatDesc(ASTC_8x6_HDR, ASTC_8x6, RF);
	addMTLPixelFormatDesc(ASTC_8x8_LDR, ASTC_8x8, RF);
	addMTLPixelFormatDescSRGB(ASTC_8x8_sRGB, ASTC_8x8, RF, ASTC_8x8_LDR);
	addMTLPixelFormatDesc(ASTC_8x8_HDR, ASTC_8x8, RF);
	addMTLPixelFormatDesc(ASTC_10x5_LDR, ASTC_10x5, RF);
	addMTLPixelFormatDescSRGB(ASTC_10x5_sRGB, ASTC_10x5, RF, ASTC_10x5_LDR);
	addMTLPixelFormatDesc(ASTC_10x5_HDR, ASTC_10x5, RF);
	addMTLPixelFormatDesc(ASTC_10x6_LDR, ASTC_10x6, RF);
	addMTLPixelFormatDescSRGB(ASTC_10x6_sRGB, ASTC_10x6, RF, ASTC_10x6_LDR);
	addMTLPixelFormatDesc(ASTC_10x6_HDR, ASTC_10x6, RF);
	addMTLPixelFormatDesc(ASTC_10x8_LDR, ASTC_10x8, RF);
	addMTLPixelFormatDescSRGB(ASTC_10x8_sRGB, ASTC_10x8, RF, ASTC_10x8_LDR);
	addMTLPixelFormatDesc(ASTC_10x8_HDR, ASTC_10x8, RF);
	addMTLPixelFormatDesc(ASTC_10x10_LDR, ASTC_10x10, RF);
	addMTLPixelFormatDescSRGB(ASTC_10x10_sRGB, ASTC_10x10, RF, ASTC_10x10_LDR);
	addMTLPixelFormatDesc(ASTC_10x10_HDR, ASTC_10x10, RF);
	addMTLPixelFormatDesc(ASTC_12x10_LDR, ASTC_12x10, RF);
	addMTLPixelFormatDescSRGB(ASTC_12x10_sRGB, ASTC_12x10, RF, ASTC_12x10_LDR);
	addMTLPixelFormatDesc(ASTC_12x10_HDR, ASTC_12x10, RF);
	addMTLPixelFormatDesc(ASTC_12x12_LDR, ASTC_12x12, RF);
	addMTLPixelFormatDescSRGB(ASTC_12x12_sRGB, ASTC_12x12, RF, ASTC_12x12_LDR);
	addMTLPixelFormatDesc(ASTC_12x12_HDR, ASTC_12x12, RF);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	addMTLPixelFormatDesc(BC1_RGBA, BC1_RGBA, RF);
	addMTLPixelFormatDescSRGB(BC1_RGBA_sRGB, BC1_RGBA, RF, BC1_RGBA);
	addMTLPixelFormatDesc(BC2_RGBA, BC2_RGBA, RF);
	addMTLPixelFormatDescSRGB(BC2_RGBA_sRGB, BC2_RGBA, RF, BC2_RGBA);
	addMTLPixelFormatDesc(BC3_RGBA, BC3_RGBA, RF);
	addMTLPixelFormatDescSRGB(BC3_RGBA_sRGB, BC3_RGBA, RF, BC3_RGBA);
	addMTLPixelFormatDesc(BC4_RUnorm, BC4_R, RF);
	addMTLPixelFormatDesc(BC4_RSnorm, BC4_R, RF);
	addMTLPixelFormatDesc(BC5_RGUnorm, BC5_RG, RF);
	addMTLPixelFormatDesc(BC5_RGSnorm, BC5_RG, RF);
	addMTLPixelFormatDesc(BC6H_RGBUfloat, BC6H_RGB, RF);
	addMTLPixelFormatDesc(BC6H_RGBFloat, BC6H_RGB, RF);
	addMTLPixelFormatDesc(BC7_RGBAUnorm, BC7_RGBA, RF);
	addMTLPixelFormatDescSRGB(BC7_RGBAUnorm_sRGB, BC7_RGBA, RF, BC7_RGBAUnorm);

#pragma clang diagnostic pop

	// YUV pixel formats
	addMTLPixelFormatDesc(GBGR422, None, RF);
	addMTLPixelFormatDesc(BGRG422, None, RF);

	// Extended range and wide color pixel formats
	addMTLPixelFormatDesc(BGRA10_XR, BGRA10_XR, All);
	addMTLPixelFormatDescSRGB(BGRA10_XR_sRGB, BGRA10_XR, All, BGRA10_XR);
	addMTLPixelFormatDesc(BGR10_XR, BGR10_XR, All);
	addMTLPixelFormatDescSRGB(BGR10_XR_sRGB, BGR10_XR, All, BGR10_XR);

	// Depth and stencil pixel formats
	addMTLPixelFormatDesc(Depth16Unorm, None, DRFMR);
	addMTLPixelFormatDesc(Depth32Float, None, DRMR);
	addMTLPixelFormatDesc(Stencil8, None, DRM);
	addMTLPixelFormatDesc(Depth24Unorm_Stencil8, Depth24_Stencil8, None);
	addMTLPixelFormatDesc(Depth32Float_Stencil8, Depth32_Stencil8, DRMR);
	addMTLPixelFormatDesc(X24_Stencil8, Depth24_Stencil8, None);
	addMTLPixelFormatDesc(X32_Stencil8, Depth32_Stencil8, DRM);
}

// If necessary, resize vector with empty elements.
void PixelFormats::addMTLVertexFormatDescImpl(MTLVertexFormat mtlVtxFmt, MTLFmtCaps vtxCap, const char *name) {
	if (mtlVtxFmt >= _mtl_vertex_format_descs.size()) {
		_mtl_vertex_format_descs.resize(mtlVtxFmt + 1);
	}
	_mtl_vertex_format_descs[mtlVtxFmt] = { .mtlVertexFormat = mtlVtxFmt, RD::DATA_FORMAT_MAX, vtxCap, MTLViewClass::None, MTLPixelFormatInvalid, name };
}

// Check mtlVtx exists on platform, to avoid overwriting the MTLVertexFormatInvalid entry.
#define addMTLVertexFormatDesc(mtlVtx)                                                                     \
	if (MTLVertexFormat##mtlVtx) {                                                                         \
		addMTLVertexFormatDescImpl(MTLVertexFormat##mtlVtx, kMTLFmtCapsVertex, "MTLVertexFormat" #mtlVtx); \
	}

void PixelFormats::initMTLVertexFormatCapabilities(const MetalFeatures &p_feat) {
	_mtl_vertex_format_descs.resize(MTLVertexFormatHalf + 3);
	// MTLVertexFormatInvalid must come first. Use addMTLVertexFormatDescImpl to avoid guard code.
	addMTLVertexFormatDescImpl(MTLVertexFormatInvalid, kMTLFmtCapsNone, "MTLVertexFormatInvalid");

	addMTLVertexFormatDesc(UChar2Normalized);
	addMTLVertexFormatDesc(Char2Normalized);
	addMTLVertexFormatDesc(UChar2);
	addMTLVertexFormatDesc(Char2);

	addMTLVertexFormatDesc(UChar3Normalized);
	addMTLVertexFormatDesc(Char3Normalized);
	addMTLVertexFormatDesc(UChar3);
	addMTLVertexFormatDesc(Char3);

	addMTLVertexFormatDesc(UChar4Normalized);
	addMTLVertexFormatDesc(Char4Normalized);
	addMTLVertexFormatDesc(UChar4);
	addMTLVertexFormatDesc(Char4);

	addMTLVertexFormatDesc(UInt1010102Normalized);
	addMTLVertexFormatDesc(Int1010102Normalized);

	addMTLVertexFormatDesc(UShort2Normalized);
	addMTLVertexFormatDesc(Short2Normalized);
	addMTLVertexFormatDesc(UShort2);
	addMTLVertexFormatDesc(Short2);
	addMTLVertexFormatDesc(Half2);

	addMTLVertexFormatDesc(UShort3Normalized);
	addMTLVertexFormatDesc(Short3Normalized);
	addMTLVertexFormatDesc(UShort3);
	addMTLVertexFormatDesc(Short3);
	addMTLVertexFormatDesc(Half3);

	addMTLVertexFormatDesc(UShort4Normalized);
	addMTLVertexFormatDesc(Short4Normalized);
	addMTLVertexFormatDesc(UShort4);
	addMTLVertexFormatDesc(Short4);
	addMTLVertexFormatDesc(Half4);

	addMTLVertexFormatDesc(UInt);
	addMTLVertexFormatDesc(Int);
	addMTLVertexFormatDesc(Float);

	addMTLVertexFormatDesc(UInt2);
	addMTLVertexFormatDesc(Int2);
	addMTLVertexFormatDesc(Float2);

	addMTLVertexFormatDesc(UInt3);
	addMTLVertexFormatDesc(Int3);
	addMTLVertexFormatDesc(Float3);

	addMTLVertexFormatDesc(UInt4);
	addMTLVertexFormatDesc(Int4);
	addMTLVertexFormatDesc(Float4);

	addMTLVertexFormatDesc(UCharNormalized);
	addMTLVertexFormatDesc(CharNormalized);
	addMTLVertexFormatDesc(UChar);
	addMTLVertexFormatDesc(Char);

	addMTLVertexFormatDesc(UShortNormalized);
	addMTLVertexFormatDesc(ShortNormalized);
	addMTLVertexFormatDesc(UShort);
	addMTLVertexFormatDesc(Short);
	addMTLVertexFormatDesc(Half);

	addMTLVertexFormatDesc(UChar4Normalized_BGRA);

	if (@available(macos 14.0, ios 17.0, tvos 17.0, *)) {
		if (p_feat.highestFamily >= MTLGPUFamilyApple5) {
			addMTLVertexFormatDesc(FloatRG11B10);
			addMTLVertexFormatDesc(FloatRGB9E5);
		}
	}
}

// Return a reference to the format capabilities, so the caller can manipulate them.
// Check mtlPixFmt exists on platform, to avoid overwriting the MTLPixelFormatInvalid entry.
// When returning the dummy, reset it on each access because it can be written to by caller.
MTLFmtCaps &PixelFormats::getMTLPixelFormatCapsIf(MTLPixelFormat mtlPixFmt, bool cond) {
	static MTLFmtCaps dummyFmtCaps;
	if (mtlPixFmt && cond) {
		return getMTLPixelFormatDesc(mtlPixFmt).mtlFmtCaps;
	} else {
		dummyFmtCaps = kMTLFmtCapsNone;
		return dummyFmtCaps;
	}
}

#define setMTLPixFmtCapsIf(cond, mtlFmt, caps) getMTLPixelFormatCapsIf(MTLPixelFormat##mtlFmt, cond) = kMTLFmtCaps##caps;
#define setMTLPixFmtCapsIfGPU(gpuFam, mtlFmt, caps) setMTLPixFmtCapsIf(gpuCaps.supports##gpuFam, mtlFmt, caps)

#define enableMTLPixFmtCapsIf(cond, mtlFmt, caps) flags::set(getMTLPixelFormatCapsIf(MTLPixelFormat##mtlFmt, cond), kMTLFmtCaps##caps);
#define enableMTLPixFmtCapsIfGPU(gpuFam, mtlFmt, caps) enableMTLPixFmtCapsIf(p_feat.highestFamily >= MTLGPUFamily##gpuFam, mtlFmt, caps)

#define disableMTLPixFmtCapsIf(cond, mtlFmt, caps) flags::clear(getMTLPixelFormatCapsIf(MTLPixelFormat##mtlFmt, cond), kMTLFmtCaps##caps);

// Modifies the format capability tables based on the capabilities of the specific MTLDevice.
void PixelFormats::modifyMTLFormatCapabilities(const MetalFeatures &p_feat) {
	bool noVulkanSupport = false; // Indicated supported in Metal but not Vulkan or SPIR-V.
	bool notMac = !p_feat.supportsMac;
	bool iosOnly1 = notMac && p_feat.highestFamily < MTLGPUFamilyApple2;
	bool iosOnly2 = notMac && p_feat.highestFamily < MTLGPUFamilyApple3;
	bool iosOnly6 = notMac && p_feat.highestFamily < MTLGPUFamilyApple7;
	bool iosOnly8 = notMac && p_feat.highestFamily < MTLGPUFamilyApple9;

	setMTLPixFmtCapsIf(iosOnly2, A8Unorm, RF);
	setMTLPixFmtCapsIf(iosOnly1, R8Unorm_sRGB, RFCMRB);
	setMTLPixFmtCapsIf(iosOnly1, R8Snorm, RFWCMB);

	setMTLPixFmtCapsIf(iosOnly1, RG8Unorm_sRGB, RFCMRB);
	setMTLPixFmtCapsIf(iosOnly1, RG8Snorm, RFWCMB);

	enableMTLPixFmtCapsIfGPU(Apple6, R32Uint, Atomic);
	enableMTLPixFmtCapsIfGPU(Apple6, R32Sint, Atomic);

	setMTLPixFmtCapsIf(iosOnly8, R32Float, RWCMB);

	setMTLPixFmtCapsIf(iosOnly1, RGBA8Unorm_sRGB, RFCMRB);
	setMTLPixFmtCapsIf(iosOnly1, RGBA8Snorm, RFWCMB);
	setMTLPixFmtCapsIf(iosOnly1, BGRA8Unorm_sRGB, RFCMRB);

	setMTLPixFmtCapsIf(iosOnly2, RGB10A2Unorm, RFCMRB);
	setMTLPixFmtCapsIf(iosOnly2, RGB10A2Uint, RCM);
	setMTLPixFmtCapsIf(iosOnly2, RG11B10Float, RFCMRB);
	setMTLPixFmtCapsIf(iosOnly2, RGB9E5Float, RFCMRB);

	// Blending is actually supported for RGB9E5Float, but format channels cannot
	// be individually write-enabled during blending on macOS. Disabling blending
	// on macOS is the least-intrusive way to handle this in a Vulkan-friendly way.
	disableMTLPixFmtCapsIf(p_feat.supportsMac, RGB9E5Float, Blend);

	// RGB9E5Float cannot be used as a render target on the simulator.
	disableMTLPixFmtCapsIf(TARGET_OS_SIMULATOR, RGB9E5Float, ColorAtt);

	setMTLPixFmtCapsIf(iosOnly6, RG32Uint, RWC);
	setMTLPixFmtCapsIf(iosOnly6, RG32Sint, RWC);

	// Metal supports reading both R&G into as one 64-bit atomic operation, but Vulkan and SPIR-V do not.
	// Including this here so we remember to update this if support is added to Vulkan in the future.
	bool atomic64 = noVulkanSupport && (p_feat.highestFamily >= MTLGPUFamilyApple9 || (p_feat.highestFamily >= MTLGPUFamilyApple8 && p_feat.supportsMac));
	enableMTLPixFmtCapsIf(atomic64, RG32Uint, Atomic);
	enableMTLPixFmtCapsIf(atomic64, RG32Sint, Atomic);

	setMTLPixFmtCapsIf(iosOnly8, RG32Float, RWCMB);
	setMTLPixFmtCapsIf(iosOnly6, RG32Float, RWCB);

	setMTLPixFmtCapsIf(iosOnly8, RGBA32Float, RWCM);
	setMTLPixFmtCapsIf(iosOnly6, RGBA32Float, RWC);

	bool msaa32 = p_feat.supports32BitMSAA;
	enableMTLPixFmtCapsIf(msaa32, R32Uint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, R32Sint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, R32Float, Resolve);
	enableMTLPixFmtCapsIf(msaa32, RG32Uint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, RG32Sint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, RG32Float, Resolve);
	enableMTLPixFmtCapsIf(msaa32, RGBA32Uint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, RGBA32Sint, MSAA);
	enableMTLPixFmtCapsIf(msaa32, RGBA32Float, Resolve);

	bool floatFB = p_feat.supports32BitFloatFiltering;
	enableMTLPixFmtCapsIf(floatFB, R32Float, Filter);
	enableMTLPixFmtCapsIf(floatFB, RG32Float, Filter);
	enableMTLPixFmtCapsIf(floatFB, RGBA32Float, Filter);
	enableMTLPixFmtCapsIf(floatFB, RGBA32Float, Blend); // Undocumented by confirmed through testing.

	bool noHDR_ASTC = p_feat.highestFamily < MTLGPUFamilyApple6;
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_4x4_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_5x4_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_5x5_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_6x5_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_6x6_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_8x5_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_8x6_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_8x8_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_10x5_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_10x6_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_10x8_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_10x10_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_12x10_HDR, None);
	setMTLPixFmtCapsIf(noHDR_ASTC, ASTC_12x12_HDR, None);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	bool noBC = !p_feat.supportsBCTextureCompression;
	setMTLPixFmtCapsIf(noBC, BC1_RGBA, None);
	setMTLPixFmtCapsIf(noBC, BC1_RGBA_sRGB, None);
	setMTLPixFmtCapsIf(noBC, BC2_RGBA, None);
	setMTLPixFmtCapsIf(noBC, BC2_RGBA_sRGB, None);
	setMTLPixFmtCapsIf(noBC, BC3_RGBA, None);
	setMTLPixFmtCapsIf(noBC, BC3_RGBA_sRGB, None);
	setMTLPixFmtCapsIf(noBC, BC4_RUnorm, None);
	setMTLPixFmtCapsIf(noBC, BC4_RSnorm, None);
	setMTLPixFmtCapsIf(noBC, BC5_RGUnorm, None);
	setMTLPixFmtCapsIf(noBC, BC5_RGSnorm, None);
	setMTLPixFmtCapsIf(noBC, BC6H_RGBUfloat, None);
	setMTLPixFmtCapsIf(noBC, BC6H_RGBFloat, None);
	setMTLPixFmtCapsIf(noBC, BC7_RGBAUnorm, None);
	setMTLPixFmtCapsIf(noBC, BC7_RGBAUnorm_sRGB, None);

#pragma clang diagnostic pop

	setMTLPixFmtCapsIf(iosOnly2, BGRA10_XR, None);
	setMTLPixFmtCapsIf(iosOnly2, BGRA10_XR_sRGB, None);
	setMTLPixFmtCapsIf(iosOnly2, BGR10_XR, None);
	setMTLPixFmtCapsIf(iosOnly2, BGR10_XR_sRGB, None);

	setMTLPixFmtCapsIf(iosOnly2, Depth16Unorm, DRFM);
	setMTLPixFmtCapsIf(iosOnly2, Depth32Float, DRM);

	setMTLPixFmtCapsIf(!p_feat.supportsDepth24Stencil8, Depth24Unorm_Stencil8, None);
	setMTLPixFmtCapsIf(iosOnly2, Depth32Float_Stencil8, DRM);
}

// Populates the DataFormat lookup maps and connects Godot and Metal pixel formats to one-another.
void PixelFormats::buildDFFormatMaps() {
	for (DataFormatDesc &dfDesc : _data_format_descs) {
		// Populate the back reference from the Metal formats to the Godot format.
		// Validate the corresponding Metal formats for the platform, and clear them
		// in the Godot format if not supported.
		if (dfDesc.mtlPixelFormat) {
			MTLFormatDesc &mtlDesc = getMTLPixelFormatDesc(dfDesc.mtlPixelFormat);
			if (mtlDesc.dataFormat == RD::DATA_FORMAT_MAX) {
				mtlDesc.dataFormat = dfDesc.dataFormat;
			}
			if (!mtlDesc.isSupported()) {
				dfDesc.mtlPixelFormat = MTLPixelFormatInvalid;
			}
		}
		if (dfDesc.mtlPixelFormatSubstitute) {
			MTLFormatDesc &mtlDesc = getMTLPixelFormatDesc(dfDesc.mtlPixelFormatSubstitute);
			if (!mtlDesc.isSupported()) {
				dfDesc.mtlPixelFormatSubstitute = MTLPixelFormatInvalid;
			}
		}
		if (dfDesc.mtlVertexFormat) {
			MTLFormatDesc &mtlDesc = getMTLVertexFormatDesc(dfDesc.mtlVertexFormat);
			if (mtlDesc.dataFormat == RD::DATA_FORMAT_MAX) {
				mtlDesc.dataFormat = dfDesc.dataFormat;
			}
			if (!mtlDesc.isSupported()) {
				dfDesc.mtlVertexFormat = MTLVertexFormatInvalid;
			}
		}
		if (dfDesc.mtlVertexFormatSubstitute) {
			MTLFormatDesc &mtlDesc = getMTLVertexFormatDesc(dfDesc.mtlVertexFormatSubstitute);
			if (!mtlDesc.isSupported()) {
				dfDesc.mtlVertexFormatSubstitute = MTLVertexFormatInvalid;
			}
		}
	}
}
