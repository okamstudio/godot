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

/** Selects and returns one of the values, based on the platform OS. */
_FORCE_INLINE_ constexpr MTLFmtCaps select_platform_caps(MTLFmtCaps p_macOS_val, MTLFmtCaps p_iOS_val) {
#if (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_MACCATALYST
	return p_iOS_val;
#elif TARGET_OS_OSX
	return p_macOS_val;
#else
#error "unsupported platform"
#endif
}

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
	for (MTLFormatDesc &otherDesc : _mtlPixelFormatDescriptions) {
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
	CRASH_BAD_INDEX_MSG(p_format, RD::DATA_FORMAT_MAX, "Attempting to describe an invalid DataFormat");
	return _dataFormatDescriptions[p_format];
}

DataFormatDesc &PixelFormats::getDataFormatDesc(MTLPixelFormat p_format) {
	return getDataFormatDesc(getMTLPixelFormatDesc(p_format).dataFormat);
}

// Return a reference to the Metal format descriptor corresponding to the MTLPixelFormat.
MTLFormatDesc &PixelFormats::getMTLPixelFormatDesc(MTLPixelFormat p_format) {
	uint16_t fmtIdx = ((p_format < _mtlPixelFormatCoreCount)
					? _mtlFormatDescIndicesByMTLPixelFormatsCore[p_format]
					: _mtlFormatDescIndicesByMTLPixelFormatsExt[p_format]);
	return _mtlPixelFormatDescriptions[fmtIdx];
}

// Return a reference to the Metal format descriptor corresponding to the MTLVertexFormat.
MTLFormatDesc &PixelFormats::getMTLVertexFormatDesc(MTLVertexFormat p_format) {
	uint16_t fmtIdx = (p_format < _mtlVertexFormatCount) ? _mtlFormatDescIndicesByMTLVertexFormats[p_format] : 0;
	return _mtlVertexFormatDescriptions[fmtIdx];
}

PixelFormats::PixelFormats(id<MTLDevice> p_device) :
		device(p_device) {
	initMTLPixelFormatCapabilities();
	initMTLVertexFormatCapabilities();
	buildMTLFormatMaps();
	modifyMTLFormatCapabilities();

	initDataFormatCapabilities();
	buildDFFormatMaps();
}

#define addDfFormatDescFull(DATA_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE)                                                               \
	CRASH_BAD_INDEX_MSG(RD::DATA_FORMAT_##DATA_FMT, RD::DATA_FORMAT_MAX, "Attempting to describe too many DataFormats");                                                                                      \
	_dataFormatDescriptions[RD::DATA_FORMAT_##DATA_FMT] = { RD::DATA_FORMAT_##DATA_FMT, MTLPixelFormat##MTL_FMT, MTLPixelFormat##MTL_FMT_ALT, MTLVertexFormat##MTL_VTX_FMT, MTLVertexFormat##MTL_VTX_FMT_ALT, \
		CSPC, CSCB, { BLK_W, BLK_H }, BLK_BYTE_CNT, MTLFormatType::MVK_FMT_TYPE, "DATA_FORMAT_" #DATA_FMT, false }

#define addDataFormatDesc(DATA_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE) \
	addDfFormatDescFull(DATA_FMT, MTL_FMT, MTL_FMT_ALT, MTL_VTX_FMT, MTL_VTX_FMT_ALT, 0, 0, BLK_W, BLK_H, BLK_BYTE_CNT, MVK_FMT_TYPE)

#define addDfFormatDescChromaSubsampling(DATA_FMT, MTL_FMT, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT) \
	addDfFormatDescFull(DATA_FMT, MTL_FMT, Invalid, Invalid, Invalid, CSPC, CSCB, BLK_W, BLK_H, BLK_BYTE_CNT, ColorFloat)

void PixelFormats::initDataFormatCapabilities() {
	clear(_dataFormatDescriptions, RD::DATA_FORMAT_MAX);

	addDataFormatDesc(R4G4_UNORM_PACK8, Invalid, Invalid, Invalid, Invalid, 1, 1, 1, ColorFloat);
	addDataFormatDesc(R4G4B4A4_UNORM_PACK16, ABGR4Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(B4G4R4A4_UNORM_PACK16, Invalid, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);

	addDataFormatDesc(R5G6B5_UNORM_PACK16, B5G6R5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(B5G6R5_UNORM_PACK16, Invalid, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(R5G5B5A1_UNORM_PACK16, A1BGR5Unorm, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
	addDataFormatDesc(B5G5R5A1_UNORM_PACK16, Invalid, Invalid, Invalid, Invalid, 1, 1, 2, ColorFloat);
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
	addDataFormatDesc(B8G8R8A8_SNORM, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(B8G8R8A8_USCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(B8G8R8A8_SSCALED, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorFloat);
	addDataFormatDesc(B8G8R8A8_UINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorUInt8);
	addDataFormatDesc(B8G8R8A8_SINT, Invalid, Invalid, Invalid, Invalid, 1, 1, 4, ColorInt8);
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

#define addMTLPixelFormatDescFull(MTL_FMT, VIEW_CLASS, IOS_CAPS, MACOS_CAPS, MTL_FMT_LINEAR) \
	CRASH_BAD_INDEX_MSG(fmtIdx, _mtlPixelFormatCount, "Adding too many pixel formats");      \
	_mtlPixelFormatDescriptions[fmtIdx++] = { .mtlPixelFormat = MTLPixelFormat##MTL_FMT, RD::DATA_FORMAT_MAX, select_platform_caps(kMTLFmtCaps##MACOS_CAPS, kMTLFmtCaps##IOS_CAPS), MTLViewClass::VIEW_CLASS, MTLPixelFormat##MTL_FMT_LINEAR, "MTLPixelFormat" #MTL_FMT }

#define addMTLPixelFormatDesc(MTL_FMT, VIEW_CLASS, IOS_CAPS, MACOS_CAPS) \
	addMTLPixelFormatDescFull(MTL_FMT, VIEW_CLASS, IOS_CAPS, MACOS_CAPS, MTL_FMT)

#define addMTLPixelFormatDescSRGB(MTL_FMT, VIEW_CLASS, IOS_CAPS, MACOS_CAPS, MTL_FMT_LINEAR) \
	addMTLPixelFormatDescFull(MTL_FMT, VIEW_CLASS, IOS_CAPS, MACOS_CAPS, MTL_FMT_LINEAR)

void PixelFormats::initMTLPixelFormatCapabilities() {
	clear(_mtlPixelFormatDescriptions, _mtlPixelFormatCount);

	uint32_t fmtIdx = 0;

	// When adding to this list, be sure to ensure _mtlPixelFormatCount is large enough for the format count.

	// MTLPixelFormatInvalid must come first.
	addMTLPixelFormatDesc(Invalid, None, None, None);

	// Ordinary 8-bit pixel formats.
	addMTLPixelFormatDesc(A8Unorm, Color8, RF, RF);
	addMTLPixelFormatDesc(R8Unorm, Color8, All, All);
	addMTLPixelFormatDescSRGB(R8Unorm_sRGB, Color8, RFCMRB, None, R8Unorm);
	addMTLPixelFormatDesc(R8Snorm, Color8, RFWCMB, All);
	addMTLPixelFormatDesc(R8Uint, Color8, RWCM, RWCM);
	addMTLPixelFormatDesc(R8Sint, Color8, RWCM, RWCM);

	// Ordinary 16-bit pixel formats.
	addMTLPixelFormatDesc(R16Unorm, Color16, RFWCMB, All);
	addMTLPixelFormatDesc(R16Snorm, Color16, RFWCMB, All);
	addMTLPixelFormatDesc(R16Uint, Color16, RWCM, RWCM);
	addMTLPixelFormatDesc(R16Sint, Color16, RWCM, RWCM);
	addMTLPixelFormatDesc(R16Float, Color16, All, All);

	addMTLPixelFormatDesc(RG8Unorm, Color16, All, All);
	addMTLPixelFormatDescSRGB(RG8Unorm_sRGB, Color16, RFCMRB, None, RG8Unorm);
	addMTLPixelFormatDesc(RG8Snorm, Color16, RFWCMB, All);
	addMTLPixelFormatDesc(RG8Uint, Color16, RWCM, RWCM);
	addMTLPixelFormatDesc(RG8Sint, Color16, RWCM, RWCM);

	// Packed 16-bit pixel formats.
	addMTLPixelFormatDesc(B5G6R5Unorm, Color16, RFCMRB, None);
	addMTLPixelFormatDesc(A1BGR5Unorm, Color16, RFCMRB, None);
	addMTLPixelFormatDesc(ABGR4Unorm, Color16, RFCMRB, None);
	addMTLPixelFormatDesc(BGR5A1Unorm, Color16, RFCMRB, None);

	// Ordinary 32-bit pixel formats.
	addMTLPixelFormatDesc(R32Uint, Color32, RC, RWCM);
	addMTLPixelFormatDesc(R32Sint, Color32, RC, RWCM);
	addMTLPixelFormatDesc(R32Float, Color32, RCMB, All);

	addMTLPixelFormatDesc(RG16Unorm, Color32, RFWCMB, All);
	addMTLPixelFormatDesc(RG16Snorm, Color32, RFWCMB, All);
	addMTLPixelFormatDesc(RG16Uint, Color32, RWCM, RWCM);
	addMTLPixelFormatDesc(RG16Sint, Color32, RWCM, RWCM);
	addMTLPixelFormatDesc(RG16Float, Color32, All, All);

	addMTLPixelFormatDesc(RGBA8Unorm, Color32, All, All);
	addMTLPixelFormatDescSRGB(RGBA8Unorm_sRGB, Color32, RFCMRB, RFCMRB, RGBA8Unorm);
	addMTLPixelFormatDesc(RGBA8Snorm, Color32, RFWCMB, All);
	addMTLPixelFormatDesc(RGBA8Uint, Color32, RWCM, RWCM);
	addMTLPixelFormatDesc(RGBA8Sint, Color32, RWCM, RWCM);

	addMTLPixelFormatDesc(BGRA8Unorm, Color32, All, All);
	addMTLPixelFormatDescSRGB(BGRA8Unorm_sRGB, Color32, RFCMRB, RFCMRB, BGRA8Unorm);

	// Packed 32-bit pixel formats.
	addMTLPixelFormatDesc(RGB10A2Unorm, Color32, RFCMRB, All);
	addMTLPixelFormatDesc(RGB10A2Uint, Color32, RCM, RWCM);
	addMTLPixelFormatDesc(RG11B10Float, Color32, RFCMRB, All);
	addMTLPixelFormatDesc(RGB9E5Float, Color32, RFCMRB, RF);

	// Ordinary 64-bit pixel formats.
	addMTLPixelFormatDesc(RG32Uint, Color64, RC, RWCM);
	addMTLPixelFormatDesc(RG32Sint, Color64, RC, RWCM);
	addMTLPixelFormatDesc(RG32Float, Color64, RCB, All);

	addMTLPixelFormatDesc(RGBA16Unorm, Color64, RFWCMB, All);
	addMTLPixelFormatDesc(RGBA16Snorm, Color64, RFWCMB, All);
	addMTLPixelFormatDesc(RGBA16Uint, Color64, RWCM, RWCM);
	addMTLPixelFormatDesc(RGBA16Sint, Color64, RWCM, RWCM);
	addMTLPixelFormatDesc(RGBA16Float, Color64, All, All);

	// Ordinary 128-bit pixel formats.
	addMTLPixelFormatDesc(RGBA32Uint, Color128, RC, RWCM);
	addMTLPixelFormatDesc(RGBA32Sint, Color128, RC, RWCM);
	addMTLPixelFormatDesc(RGBA32Float, Color128, RC, All);

	// Compressed pixel formats.
	addMTLPixelFormatDesc(PVRTC_RGBA_2BPP, PVRTC_RGBA_2BPP, RF, None);
	addMTLPixelFormatDescSRGB(PVRTC_RGBA_2BPP_sRGB, PVRTC_RGBA_2BPP, RF, None, PVRTC_RGBA_2BPP);
	addMTLPixelFormatDesc(PVRTC_RGBA_4BPP, PVRTC_RGBA_4BPP, RF, None);
	addMTLPixelFormatDescSRGB(PVRTC_RGBA_4BPP_sRGB, PVRTC_RGBA_4BPP, RF, None, PVRTC_RGBA_4BPP);

	addMTLPixelFormatDesc(ETC2_RGB8, ETC2_RGB8, RF, None);
	addMTLPixelFormatDescSRGB(ETC2_RGB8_sRGB, ETC2_RGB8, RF, None, ETC2_RGB8);
	addMTLPixelFormatDesc(ETC2_RGB8A1, ETC2_RGB8A1, RF, None);
	addMTLPixelFormatDescSRGB(ETC2_RGB8A1_sRGB, ETC2_RGB8A1, RF, None, ETC2_RGB8A1);
	addMTLPixelFormatDesc(EAC_RGBA8, EAC_RGBA8, RF, None);
	addMTLPixelFormatDescSRGB(EAC_RGBA8_sRGB, EAC_RGBA8, RF, None, EAC_RGBA8);
	addMTLPixelFormatDesc(EAC_R11Unorm, EAC_R11, RF, None);
	addMTLPixelFormatDesc(EAC_R11Snorm, EAC_R11, RF, None);
	addMTLPixelFormatDesc(EAC_RG11Unorm, EAC_RG11, RF, None);
	addMTLPixelFormatDesc(EAC_RG11Snorm, EAC_RG11, RF, None);

	addMTLPixelFormatDesc(ASTC_4x4_LDR, ASTC_4x4, None, None);
	addMTLPixelFormatDescSRGB(ASTC_4x4_sRGB, ASTC_4x4, None, None, ASTC_4x4_LDR);
	addMTLPixelFormatDesc(ASTC_4x4_HDR, ASTC_4x4, None, None);
	addMTLPixelFormatDesc(ASTC_5x4_LDR, ASTC_5x4, None, None);
	addMTLPixelFormatDescSRGB(ASTC_5x4_sRGB, ASTC_5x4, None, None, ASTC_5x4_LDR);
	addMTLPixelFormatDesc(ASTC_5x4_HDR, ASTC_5x4, None, None);
	addMTLPixelFormatDesc(ASTC_5x5_LDR, ASTC_5x5, None, None);
	addMTLPixelFormatDescSRGB(ASTC_5x5_sRGB, ASTC_5x5, None, None, ASTC_5x5_LDR);
	addMTLPixelFormatDesc(ASTC_5x5_HDR, ASTC_5x5, None, None);
	addMTLPixelFormatDesc(ASTC_6x5_LDR, ASTC_6x5, None, None);
	addMTLPixelFormatDescSRGB(ASTC_6x5_sRGB, ASTC_6x5, None, None, ASTC_6x5_LDR);
	addMTLPixelFormatDesc(ASTC_6x5_HDR, ASTC_6x5, None, None);
	addMTLPixelFormatDesc(ASTC_6x6_LDR, ASTC_6x6, None, None);
	addMTLPixelFormatDescSRGB(ASTC_6x6_sRGB, ASTC_6x6, None, None, ASTC_6x6_LDR);
	addMTLPixelFormatDesc(ASTC_6x6_HDR, ASTC_6x6, None, None);
	addMTLPixelFormatDesc(ASTC_8x5_LDR, ASTC_8x5, None, None);
	addMTLPixelFormatDescSRGB(ASTC_8x5_sRGB, ASTC_8x5, None, None, ASTC_8x5_LDR);
	addMTLPixelFormatDesc(ASTC_8x5_HDR, ASTC_8x5, None, None);
	addMTLPixelFormatDesc(ASTC_8x6_LDR, ASTC_8x6, None, None);
	addMTLPixelFormatDescSRGB(ASTC_8x6_sRGB, ASTC_8x6, None, None, ASTC_8x6_LDR);
	addMTLPixelFormatDesc(ASTC_8x6_HDR, ASTC_8x6, None, None);
	addMTLPixelFormatDesc(ASTC_8x8_LDR, ASTC_8x8, None, None);
	addMTLPixelFormatDescSRGB(ASTC_8x8_sRGB, ASTC_8x8, None, None, ASTC_8x8_LDR);
	addMTLPixelFormatDesc(ASTC_8x8_HDR, ASTC_8x8, None, None);
	addMTLPixelFormatDesc(ASTC_10x5_LDR, ASTC_10x5, None, None);
	addMTLPixelFormatDescSRGB(ASTC_10x5_sRGB, ASTC_10x5, None, None, ASTC_10x5_LDR);
	addMTLPixelFormatDesc(ASTC_10x5_HDR, ASTC_10x5, None, None);
	addMTLPixelFormatDesc(ASTC_10x6_LDR, ASTC_10x6, None, None);
	addMTLPixelFormatDescSRGB(ASTC_10x6_sRGB, ASTC_10x6, None, None, ASTC_10x6_LDR);
	addMTLPixelFormatDesc(ASTC_10x6_HDR, ASTC_10x6, None, None);
	addMTLPixelFormatDesc(ASTC_10x8_LDR, ASTC_10x8, None, None);
	addMTLPixelFormatDescSRGB(ASTC_10x8_sRGB, ASTC_10x8, None, None, ASTC_10x8_LDR);
	addMTLPixelFormatDesc(ASTC_10x8_HDR, ASTC_10x8, None, None);
	addMTLPixelFormatDesc(ASTC_10x10_LDR, ASTC_10x10, None, None);
	addMTLPixelFormatDescSRGB(ASTC_10x10_sRGB, ASTC_10x10, None, None, ASTC_10x10_LDR);
	addMTLPixelFormatDesc(ASTC_10x10_HDR, ASTC_10x10, None, None);
	addMTLPixelFormatDesc(ASTC_12x10_LDR, ASTC_12x10, None, None);
	addMTLPixelFormatDescSRGB(ASTC_12x10_sRGB, ASTC_12x10, None, None, ASTC_12x10_LDR);
	addMTLPixelFormatDesc(ASTC_12x10_HDR, ASTC_12x10, None, None);
	addMTLPixelFormatDesc(ASTC_12x12_LDR, ASTC_12x12, None, None);
	addMTLPixelFormatDescSRGB(ASTC_12x12_sRGB, ASTC_12x12, None, None, ASTC_12x12_LDR);
	addMTLPixelFormatDesc(ASTC_12x12_HDR, ASTC_12x12, None, None);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

	addMTLPixelFormatDesc(BC1_RGBA, BC1_RGBA, RF, RF);
	addMTLPixelFormatDescSRGB(BC1_RGBA_sRGB, BC1_RGBA, RF, RF, BC1_RGBA);
	addMTLPixelFormatDesc(BC2_RGBA, BC2_RGBA, RF, RF);
	addMTLPixelFormatDescSRGB(BC2_RGBA_sRGB, BC2_RGBA, RF, RF, BC2_RGBA);
	addMTLPixelFormatDesc(BC3_RGBA, BC3_RGBA, RF, RF);
	addMTLPixelFormatDescSRGB(BC3_RGBA_sRGB, BC3_RGBA, RF, RF, BC3_RGBA);
	addMTLPixelFormatDesc(BC4_RUnorm, BC4_R, RF, RF);
	addMTLPixelFormatDesc(BC4_RSnorm, BC4_R, RF, RF);
	addMTLPixelFormatDesc(BC5_RGUnorm, BC5_RG, RF, RF);
	addMTLPixelFormatDesc(BC5_RGSnorm, BC5_RG, RF, RF);
	addMTLPixelFormatDesc(BC6H_RGBUfloat, BC6H_RGB, RF, RF);
	addMTLPixelFormatDesc(BC6H_RGBFloat, BC6H_RGB, RF, RF);
	addMTLPixelFormatDesc(BC7_RGBAUnorm, BC7_RGBA, RF, RF);
	addMTLPixelFormatDescSRGB(BC7_RGBAUnorm_sRGB, BC7_RGBA, RF, RF, BC7_RGBAUnorm);

#pragma clang diagnostic pop

	// YUV pixel formats.
	addMTLPixelFormatDesc(GBGR422, None, RF, RF);
	addMTLPixelFormatDesc(BGRG422, None, RF, RF);

	// Extended range and wide color pixel formats.
	addMTLPixelFormatDesc(BGRA10_XR, BGRA10_XR, None, None);
	addMTLPixelFormatDescSRGB(BGRA10_XR_sRGB, BGRA10_XR, None, None, BGRA10_XR);
	addMTLPixelFormatDesc(BGR10_XR, BGR10_XR, None, None);
	addMTLPixelFormatDescSRGB(BGR10_XR_sRGB, BGR10_XR, None, None, BGR10_XR);
	addMTLPixelFormatDesc(BGR10A2Unorm, Color32, None, None);

	// Depth and stencil pixel formats.
	addMTLPixelFormatDesc(Depth16Unorm, None, None, None);
	addMTLPixelFormatDesc(Depth32Float, None, DRM, DRFMR);
	addMTLPixelFormatDesc(Stencil8, None, DRM, DRMR);
	addMTLPixelFormatDesc(Depth24Unorm_Stencil8, Depth24_Stencil8, None, None);
	addMTLPixelFormatDesc(Depth32Float_Stencil8, Depth32_Stencil8, DRM, DRFMR);
	addMTLPixelFormatDesc(X24_Stencil8, Depth24_Stencil8, None, DRMR);
	addMTLPixelFormatDesc(X32_Stencil8, Depth32_Stencil8, DRM, DRMR);

	// When adding to this list, be sure to ensure _mtlPixelFormatCount is large enough for the format count.
}

#define addMTLVertexFormatDesc(MTL_VTX_FMT, IOS_CAPS, MACOS_CAPS)                                           \
	CRASH_BAD_INDEX_MSG(fmtIdx, _mtlVertexFormatCount, "Attempting to describe too many MTLVertexFormats"); \
	_mtlVertexFormatDescriptions[fmtIdx++] = { .mtlVertexFormat = MTLVertexFormat##MTL_VTX_FMT, RD::DATA_FORMAT_MAX, select_platform_caps(kMTLFmtCaps##MACOS_CAPS, kMTLFmtCaps##IOS_CAPS), MTLViewClass::None, MTLPixelFormatInvalid, "MTLVertexFormat" #MTL_VTX_FMT }

void PixelFormats::initMTLVertexFormatCapabilities() {
	clear(_mtlVertexFormatDescriptions, _mtlVertexFormatCount);

	uint32_t fmtIdx = 0;

	// When adding to this list, be sure to ensure _mtlVertexFormatCount is large enough for the format count.

	// MTLVertexFormatInvalid must come first.
	addMTLVertexFormatDesc(Invalid, None, None);

	addMTLVertexFormatDesc(UChar2Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Char2Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UChar2, Vertex, Vertex);
	addMTLVertexFormatDesc(Char2, Vertex, Vertex);

	addMTLVertexFormatDesc(UChar3Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Char3Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UChar3, Vertex, Vertex);
	addMTLVertexFormatDesc(Char3, Vertex, Vertex);

	addMTLVertexFormatDesc(UChar4Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Char4Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UChar4, Vertex, Vertex);
	addMTLVertexFormatDesc(Char4, Vertex, Vertex);

	addMTLVertexFormatDesc(UInt1010102Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Int1010102Normalized, Vertex, Vertex);

	addMTLVertexFormatDesc(UShort2Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Short2Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UShort2, Vertex, Vertex);
	addMTLVertexFormatDesc(Short2, Vertex, Vertex);
	addMTLVertexFormatDesc(Half2, Vertex, Vertex);

	addMTLVertexFormatDesc(UShort3Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Short3Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UShort3, Vertex, Vertex);
	addMTLVertexFormatDesc(Short3, Vertex, Vertex);
	addMTLVertexFormatDesc(Half3, Vertex, Vertex);

	addMTLVertexFormatDesc(UShort4Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(Short4Normalized, Vertex, Vertex);
	addMTLVertexFormatDesc(UShort4, Vertex, Vertex);
	addMTLVertexFormatDesc(Short4, Vertex, Vertex);
	addMTLVertexFormatDesc(Half4, Vertex, Vertex);

	addMTLVertexFormatDesc(UInt, Vertex, Vertex);
	addMTLVertexFormatDesc(Int, Vertex, Vertex);
	addMTLVertexFormatDesc(Float, Vertex, Vertex);

	addMTLVertexFormatDesc(UInt2, Vertex, Vertex);
	addMTLVertexFormatDesc(Int2, Vertex, Vertex);
	addMTLVertexFormatDesc(Float2, Vertex, Vertex);

	addMTLVertexFormatDesc(UInt3, Vertex, Vertex);
	addMTLVertexFormatDesc(Int3, Vertex, Vertex);
	addMTLVertexFormatDesc(Float3, Vertex, Vertex);

	addMTLVertexFormatDesc(UInt4, Vertex, Vertex);
	addMTLVertexFormatDesc(Int4, Vertex, Vertex);
	addMTLVertexFormatDesc(Float4, Vertex, Vertex);

	addMTLVertexFormatDesc(UCharNormalized, None, None);
	addMTLVertexFormatDesc(CharNormalized, None, None);
	addMTLVertexFormatDesc(UChar, None, None);
	addMTLVertexFormatDesc(Char, None, None);

	addMTLVertexFormatDesc(UShortNormalized, None, None);
	addMTLVertexFormatDesc(ShortNormalized, None, None);
	addMTLVertexFormatDesc(UShort, None, None);
	addMTLVertexFormatDesc(Short, None, None);
	addMTLVertexFormatDesc(Half, None, None);

	addMTLVertexFormatDesc(UChar4Normalized_BGRA, None, None);

	// When adding to this list, be sure to ensure _mtlVertexFormatCount is large enough for the format count.
}

void PixelFormats::buildMTLFormatMaps() {
	// Set all MTLPixelFormats and MTLVertexFormats to undefined/invalid.
	clear(_mtlFormatDescIndicesByMTLPixelFormatsCore, _mtlPixelFormatCoreCount);
	clear(_mtlFormatDescIndicesByMTLVertexFormats, _mtlVertexFormatCount);

	// Build lookup table for MTLPixelFormat specs.
	// For most Metal format values, which are small and consecutive, use a simple lookup array.
	// For outlier format values, which can be large, use a map.
	for (uint32_t fmtIdx = 0; fmtIdx < _mtlPixelFormatCount; fmtIdx++) {
		MTLPixelFormat fmt = _mtlPixelFormatDescriptions[fmtIdx].mtlPixelFormat;
		if (fmt) {
			if (fmt < _mtlPixelFormatCoreCount) {
				_mtlFormatDescIndicesByMTLPixelFormatsCore[fmt] = fmtIdx;
			} else {
				_mtlFormatDescIndicesByMTLPixelFormatsExt[fmt] = fmtIdx;
			}
		}
	}

	// Build lookup table for MTLVertexFormat specs.
	for (uint32_t fmtIdx = 0; fmtIdx < _mtlVertexFormatCount; fmtIdx++) {
		MTLVertexFormat fmt = _mtlVertexFormatDescriptions[fmtIdx].mtlVertexFormat;
		if (fmt) {
			_mtlFormatDescIndicesByMTLVertexFormats[fmt] = fmtIdx;
		}
	}
}

// If the device supports the feature set, add additional capabilities to a MTLPixelFormat.
void PixelFormats::addMTLPixelFormatCapabilities(id<MTLDevice> p_device,
		MTLFeatureSet p_feature_set,
		MTLPixelFormat p_format,
		MTLFmtCaps p_caps) {
	if ([p_device supportsFeatureSet:p_feature_set]) {
		flags::set(getMTLPixelFormatDesc(p_format).mtlFmtCaps, p_caps);
	}
}

// If the device supports the GPU family, add additional capabilities to a MTLPixelFormat.
void PixelFormats::addMTLPixelFormatCapabilities(id<MTLDevice> p_device,
		MTLGPUFamily p_family,
		MTLPixelFormat p_format,
		MTLFmtCaps p_caps) {
	if ([p_device supportsFamily:p_family]) {
		flags::set(getMTLPixelFormatDesc(p_format).mtlFmtCaps, p_caps);
	}
}

// Disable capability flags in the Metal pixel format.
void PixelFormats::disableMTLPixelFormatCapabilities(MTLPixelFormat p_format,
		MTLFmtCaps p_caps) {
	flags::clear(getMTLPixelFormatDesc(p_format).mtlFmtCaps, p_caps);
}

void PixelFormats::disableAllMTLPixelFormatCapabilities(MTLPixelFormat p_format) {
	getMTLPixelFormatDesc(p_format).mtlFmtCaps = kMTLFmtCapsNone;
}

// If the device supports the feature set, add additional capabilities to a MTLVertexFormat.
void PixelFormats::addMTLVertexFormatCapabilities(id<MTLDevice> p_device,
		MTLFeatureSet p_feature_set,
		MTLVertexFormat p_format,
		MTLFmtCaps p_caps) {
	if ([p_device supportsFeatureSet:p_feature_set]) {
		flags::set(getMTLVertexFormatDesc(p_format).mtlFmtCaps, p_caps);
	}
}

void PixelFormats::modifyMTLFormatCapabilities() {
	modifyMTLFormatCapabilities(device);
}

// If the supportsBCTextureCompression query is available, use it.
bool supports_bc_texture_compression(id<MTLDevice> p_device) {
#if (TARGET_OS_OSX || TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 160400)
	if (@available(macOS 11.0, iOS 16.4, *)) {
		return p_device.supportsBCTextureCompression;
	}
#endif
	return false;
}

#define addFeatSetMTLPixFmtCaps(FEAT_SET, MTL_FMT, CAPS) \
	addMTLPixelFormatCapabilities(p_device, MTLFeatureSet_##FEAT_SET, MTLPixelFormat##MTL_FMT, kMTLFmtCaps##CAPS)

#define addFeatSetMTLVtxFmtCaps(FEAT_SET, MTL_FMT, CAPS) \
	addMTLVertexFormatCapabilities(p_device, MTLFeatureSet_##FEAT_SET, MTLVertexFormat##MTL_FMT, kMTLFmtCaps##CAPS)

#define addGPUMTLPixFmtCaps(GPU_FAM, MTL_FMT, CAPS) \
	addMTLPixelFormatCapabilities(p_device, MTLGPUFamily##GPU_FAM, MTLPixelFormat##MTL_FMT, kMTLFmtCaps##CAPS)

#define disableAllMTLPixFmtCaps(MTL_FMT) \
	disableAllMTLPixelFormatCapabilities(MTLPixelFormat##MTL_FMT)

#define disableMTLPixFmtCaps(MTL_FMT, CAPS) \
	disableMTLPixelFormatCapabilities(MTLPixelFormat##MTL_FMT, kMTLFmtCaps##CAPS)

void PixelFormats::modifyMTLFormatCapabilities(id<MTLDevice> p_device) {
	if (!supports_bc_texture_compression(p_device)) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

		disableAllMTLPixFmtCaps(BC1_RGBA);
		disableAllMTLPixFmtCaps(BC1_RGBA_sRGB);
		disableAllMTLPixFmtCaps(BC2_RGBA);
		disableAllMTLPixFmtCaps(BC2_RGBA_sRGB);
		disableAllMTLPixFmtCaps(BC3_RGBA);
		disableAllMTLPixFmtCaps(BC3_RGBA_sRGB);
		disableAllMTLPixFmtCaps(BC4_RUnorm);
		disableAllMTLPixFmtCaps(BC4_RSnorm);
		disableAllMTLPixFmtCaps(BC5_RGUnorm);
		disableAllMTLPixFmtCaps(BC5_RGSnorm);
		disableAllMTLPixFmtCaps(BC6H_RGBUfloat);
		disableAllMTLPixFmtCaps(BC6H_RGBFloat);
		disableAllMTLPixFmtCaps(BC7_RGBAUnorm);
		disableAllMTLPixFmtCaps(BC7_RGBAUnorm_sRGB);

#pragma clang diagnostic pop
	}

	if (!p_device.supports32BitMSAA) {
		disableMTLPixFmtCaps(R32Uint, MSAA);
		disableMTLPixFmtCaps(R32Uint, Resolve);
		disableMTLPixFmtCaps(R32Sint, MSAA);
		disableMTLPixFmtCaps(R32Sint, Resolve);
		disableMTLPixFmtCaps(R32Float, MSAA);
		disableMTLPixFmtCaps(R32Float, Resolve);
		disableMTLPixFmtCaps(RG32Uint, MSAA);
		disableMTLPixFmtCaps(RG32Uint, Resolve);
		disableMTLPixFmtCaps(RG32Sint, MSAA);
		disableMTLPixFmtCaps(RG32Sint, Resolve);
		disableMTLPixFmtCaps(RG32Float, MSAA);
		disableMTLPixFmtCaps(RG32Float, Resolve);
		disableMTLPixFmtCaps(RGBA32Uint, MSAA);
		disableMTLPixFmtCaps(RGBA32Uint, Resolve);
		disableMTLPixFmtCaps(RGBA32Sint, MSAA);
		disableMTLPixFmtCaps(RGBA32Sint, Resolve);
		disableMTLPixFmtCaps(RGBA32Float, MSAA);
		disableMTLPixFmtCaps(RGBA32Float, Resolve);
	}

	if (!p_device.supports32BitFloatFiltering) {
		disableMTLPixFmtCaps(R32Float, Filter);
		disableMTLPixFmtCaps(RG32Float, Filter);
		disableMTLPixFmtCaps(RGBA32Float, Filter);
	}

#if TARGET_OS_OSX
	addGPUMTLPixFmtCaps(Apple1, R32Uint, Atomic);
	addGPUMTLPixFmtCaps(Apple1, R32Sint, Atomic);

	if (p_device.isDepth24Stencil8PixelFormatSupported) {
		addGPUMTLPixFmtCaps(Apple1, Depth24Unorm_Stencil8, DRFMR);
	}

	addFeatSetMTLPixFmtCaps(macOS_GPUFamily1_v2, Depth16Unorm, DRFMR);

	addFeatSetMTLPixFmtCaps(macOS_GPUFamily1_v3, BGR10A2Unorm, RFCMRB);

	addGPUMTLPixFmtCaps(Apple5, R8Unorm_sRGB, All);

	addGPUMTLPixFmtCaps(Apple5, RG8Unorm_sRGB, All);

	addGPUMTLPixFmtCaps(Apple5, B5G6R5Unorm, RFCMRB);
	addGPUMTLPixFmtCaps(Apple5, A1BGR5Unorm, RFCMRB);
	addGPUMTLPixFmtCaps(Apple5, ABGR4Unorm, RFCMRB);
	addGPUMTLPixFmtCaps(Apple5, BGR5A1Unorm, RFCMRB);

	addGPUMTLPixFmtCaps(Apple5, RGBA8Unorm_sRGB, All);
	addGPUMTLPixFmtCaps(Apple5, BGRA8Unorm_sRGB, All);

	// Blending is actually supported for this format, but format channels cannot be individually write-enabled during blending.
	// Disabling blending is the least-intrusive way to handle this in a Godot-friendly way.
	addGPUMTLPixFmtCaps(Apple5, RGB9E5Float, All);
	disableMTLPixFmtCaps(RGB9E5Float, Blend);

	addGPUMTLPixFmtCaps(Apple5, PVRTC_RGBA_2BPP, RF);
	addGPUMTLPixFmtCaps(Apple5, PVRTC_RGBA_2BPP_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple5, PVRTC_RGBA_4BPP, RF);
	addGPUMTLPixFmtCaps(Apple5, PVRTC_RGBA_4BPP_sRGB, RF);

	addGPUMTLPixFmtCaps(Apple5, ETC2_RGB8, RF);
	addGPUMTLPixFmtCaps(Apple5, ETC2_RGB8_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple5, ETC2_RGB8A1, RF);
	addGPUMTLPixFmtCaps(Apple5, ETC2_RGB8A1_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_RGBA8, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_RGBA8_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_R11Unorm, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_R11Snorm, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_RG11Unorm, RF);
	addGPUMTLPixFmtCaps(Apple5, EAC_RG11Snorm, RF);

	addGPUMTLPixFmtCaps(Apple5, ASTC_4x4_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_4x4_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_4x4_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_5x4_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_5x4_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_5x4_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_5x5_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_5x5_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_5x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_6x5_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_6x5_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_6x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_6x6_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_6x6_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_6x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x5_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x5_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x6_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x6_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x8_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_8x8_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x8_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x5_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x5_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x6_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x6_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x8_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x8_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x8_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x10_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_10x10_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x10_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_12x10_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_12x10_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_12x10_HDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_12x12_LDR, RF);
	addGPUMTLPixFmtCaps(Apple5, ASTC_12x12_sRGB, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_12x12_HDR, RF);

	addGPUMTLPixFmtCaps(Apple5, BGRA10_XR, All);
	addGPUMTLPixFmtCaps(Apple5, BGRA10_XR_sRGB, All);
	addGPUMTLPixFmtCaps(Apple5, BGR10_XR, All);
	addGPUMTLPixFmtCaps(Apple5, BGR10_XR_sRGB, All);

	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, UCharNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, CharNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, UChar, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, Char, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, UShortNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, ShortNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, UShort, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, Short, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, Half, Vertex);
	addFeatSetMTLVtxFmtCaps(macOS_GPUFamily1_v3, UChar4Normalized_BGRA, Vertex);
#endif

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v3, R8Unorm_sRGB, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, R8Unorm_sRGB, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, R8Snorm, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v3, RG8Unorm_sRGB, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RG8Unorm_sRGB, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, RG8Snorm, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, R32Uint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, R32Uint, Atomic);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, R32Sint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, R32Sint, Atomic);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, R32Float, RWCMB);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v3, RGBA8Unorm_sRGB, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RGBA8Unorm_sRGB, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, RGBA8Snorm, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v3, BGRA8Unorm_sRGB, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, BGRA8Unorm_sRGB, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RGB10A2Unorm, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RGB10A2Uint, RWCM);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RG11B10Float, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, RGB9E5Float, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RG32Uint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RG32Sint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RG32Float, RWCB);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RGBA32Uint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RGBA32Sint, RWC);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v2, RGBA32Float, RWC);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_4x4_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_4x4_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_5x4_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_5x4_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_5x5_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_5x5_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_6x5_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_6x5_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_6x6_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_6x6_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x5_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x5_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x6_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x6_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x8_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_8x8_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x5_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x5_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x6_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x6_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x8_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x8_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x10_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_10x10_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_12x10_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_12x10_sRGB, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_12x12_LDR, RF);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily2_v1, ASTC_12x12_sRGB, RF);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, Depth32Float, DRMR);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, Depth32Float_Stencil8, DRMR);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v1, Stencil8, DRMR);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v2, BGRA10_XR, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v2, BGRA10_XR_sRGB, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v2, BGR10_XR, All);
	addFeatSetMTLPixFmtCaps(iOS_GPUFamily3_v2, BGR10_XR_sRGB, All);

	addFeatSetMTLPixFmtCaps(iOS_GPUFamily1_v4, BGR10A2Unorm, All);

	addGPUMTLPixFmtCaps(Apple6, ASTC_4x4_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_5x4_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_5x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_6x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_6x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_8x8_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x5_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x6_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x8_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_10x10_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_12x10_HDR, RF);
	addGPUMTLPixFmtCaps(Apple6, ASTC_12x12_HDR, RF);

	addGPUMTLPixFmtCaps(Apple1, Depth16Unorm, DRFM);
	addGPUMTLPixFmtCaps(Apple3, Depth16Unorm, DRFMR);

	// Vertex formats.
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, UCharNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, CharNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, UChar, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, Char, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, UShortNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, ShortNormalized, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, UShort, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, Short, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, Half, Vertex);
	addFeatSetMTLVtxFmtCaps(iOS_GPUFamily1_v4, UChar4Normalized_BGRA, Vertex);

// Disable for iOS simulator last.
#if TARGET_OS_SIMULATOR
	if (![p_device supportsFamily:MTLGPUFamilyApple5]) {
		disableAllMTLPixFmtCaps(R8Unorm_sRGB);
		disableAllMTLPixFmtCaps(RG8Unorm_sRGB);
		disableAllMTLPixFmtCaps(B5G6R5Unorm);
		disableAllMTLPixFmtCaps(A1BGR5Unorm);
		disableAllMTLPixFmtCaps(ABGR4Unorm);
		disableAllMTLPixFmtCaps(BGR5A1Unorm);

		disableAllMTLPixFmtCaps(BGRA10_XR);
		disableAllMTLPixFmtCaps(BGRA10_XR_sRGB);
		disableAllMTLPixFmtCaps(BGR10_XR);
		disableAllMTLPixFmtCaps(BGR10_XR_sRGB);

		disableAllMTLPixFmtCaps(GBGR422);
		disableAllMTLPixFmtCaps(BGRG422);

		disableMTLPixFmtCaps(RGB9E5Float, ColorAtt);

		disableMTLPixFmtCaps(R8Unorm_sRGB, Write);
		disableMTLPixFmtCaps(RG8Unorm_sRGB, Write);
		disableMTLPixFmtCaps(RGBA8Unorm_sRGB, Write);
		disableMTLPixFmtCaps(BGRA8Unorm_sRGB, Write);
		disableMTLPixFmtCaps(PVRTC_RGBA_2BPP_sRGB, Write);
		disableMTLPixFmtCaps(PVRTC_RGBA_4BPP_sRGB, Write);
		disableMTLPixFmtCaps(ETC2_RGB8_sRGB, Write);
		disableMTLPixFmtCaps(ETC2_RGB8A1_sRGB, Write);
		disableMTLPixFmtCaps(EAC_RGBA8_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_4x4_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_5x4_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_5x5_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_6x5_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_6x6_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_8x5_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_8x6_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_8x8_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_10x5_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_10x6_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_10x8_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_10x10_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_12x10_sRGB, Write);
		disableMTLPixFmtCaps(ASTC_12x12_sRGB, Write);
	}
#endif
#endif
}

#undef addFeatSetMTLPixFmtCaps
#undef addGPUOSMTLPixFmtCaps
#undef disableMTLPixFmtCaps
#undef disableAllMTLPixFmtCaps
#undef addFeatSetMTLVtxFmtCaps

// Populates the DataFormat lookup maps and connects Godot and Metal pixel formats to one-another.
void PixelFormats::buildDFFormatMaps() {
	// Iterate through the DataFormat descriptions, populate the lookup maps and back pointers,
	// and validate the Metal formats for the platform and OS.
	for (uint32_t fmtIdx = 0; fmtIdx < RD::DATA_FORMAT_MAX; fmtIdx++) {
		DataFormatDesc &dfDesc = _dataFormatDescriptions[fmtIdx];
		DataFormat dfFmt = dfDesc.dataFormat;
		if (dfFmt != RD::DATA_FORMAT_MAX) {
			// Populate the back reference from the Metal formats to the Godot format.
			// Validate the corresponding Metal formats for the platform, and clear them
			// in the Godot format if not supported.
			if (dfDesc.mtlPixelFormat) {
				MTLFormatDesc &mtlDesc = getMTLPixelFormatDesc(dfDesc.mtlPixelFormat);
				if (mtlDesc.dataFormat == RD::DATA_FORMAT_MAX) {
					mtlDesc.dataFormat = dfFmt;
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
					mtlDesc.dataFormat = dfFmt;
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
}
