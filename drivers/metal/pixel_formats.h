/**************************************************************************/
/*  pixel_formats.h                                                       */
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

#ifndef PIXEL_FORMATS_H
#define PIXEL_FORMATS_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import "servers/rendering/rendering_device.h"

#import <Metal/Metal.h>

static const uint32_t _mtlPixelFormatCount = 256;
static const uint32_t _mtlPixelFormatCoreCount = MTLPixelFormatX32_Stencil8 + 2; // The actual last enum value is not available on iOS.
static const uint32_t _mtlVertexFormatCount = MTLVertexFormatHalf + 1;

#pragma mark -
#pragma mark Metal format capabilities

typedef enum : uint16_t {

	kMTLFmtCapsNone = 0,
	/*! The format can be used in a shader read operation. */
	kMTLFmtCapsRead = (1 << 0),
	/*! The format can be used in a shader filter operation during sampling. */
	kMTLFmtCapsFilter = (1 << 1),
	/*! The format can be used in a shader write operation. */
	kMTLFmtCapsWrite = (1 << 2),
	/*! The format can be used with atomic operations. */
	kMTLFmtCapsAtomic = (1 << 3),
	/*! The format can be used as a color attachment. */
	kMTLFmtCapsColorAtt = (1 << 4),
	/*! The format can be used as a depth-stencil attachment. */
	kMTLFmtCapsDSAtt = (1 << 5),
	/*! The format can be used with blend operations. */
	kMTLFmtCapsBlend = (1 << 6),
	/*! The format can be used as a destination for multisample antialias (MSAA) data. */
	kMTLFmtCapsMSAA = (1 << 7),
	/*! The format can be used as a resolve attachment. */
	kMTLFmtCapsResolve = (1 << 8),
	kMTLFmtCapsVertex = (1 << 9),

	kMTLFmtCapsRF = (kMTLFmtCapsRead | kMTLFmtCapsFilter),
	kMTLFmtCapsRC = (kMTLFmtCapsRead | kMTLFmtCapsColorAtt),
	kMTLFmtCapsRCB = (kMTLFmtCapsRC | kMTLFmtCapsBlend),
	kMTLFmtCapsRCM = (kMTLFmtCapsRC | kMTLFmtCapsMSAA),
	kMTLFmtCapsRCMB = (kMTLFmtCapsRCM | kMTLFmtCapsBlend),
	kMTLFmtCapsRWC = (kMTLFmtCapsRC | kMTLFmtCapsWrite),
	kMTLFmtCapsRWCB = (kMTLFmtCapsRWC | kMTLFmtCapsBlend),
	kMTLFmtCapsRWCM = (kMTLFmtCapsRWC | kMTLFmtCapsMSAA),
	kMTLFmtCapsRWCMB = (kMTLFmtCapsRWCM | kMTLFmtCapsBlend),
	kMTLFmtCapsRFCMRB = (kMTLFmtCapsRCMB | kMTLFmtCapsFilter | kMTLFmtCapsResolve),
	kMTLFmtCapsRFWCMB = (kMTLFmtCapsRWCMB | kMTLFmtCapsFilter),
	kMTLFmtCapsAll = (kMTLFmtCapsRFWCMB | kMTLFmtCapsResolve),

	kMTLFmtCapsDRM = (kMTLFmtCapsDSAtt | kMTLFmtCapsRead | kMTLFmtCapsMSAA),
	kMTLFmtCapsDRFM = (kMTLFmtCapsDRM | kMTLFmtCapsFilter),
	kMTLFmtCapsDRMR = (kMTLFmtCapsDRM | kMTLFmtCapsResolve),
	kMTLFmtCapsDRFMR = (kMTLFmtCapsDRFM | kMTLFmtCapsResolve),

	kMTLFmtCapsChromaSubsampling = kMTLFmtCapsRF,
	kMTLFmtCapsMultiPlanar = kMTLFmtCapsChromaSubsampling,
} MTLFmtCaps;

inline MTLFmtCaps operator|(MTLFmtCaps p_left, MTLFmtCaps p_right) {
	return static_cast<MTLFmtCaps>(static_cast<uint32_t>(p_left) | p_right);
}

inline MTLFmtCaps &operator|=(MTLFmtCaps &p_left, MTLFmtCaps p_right) {
	return (p_left = p_left | p_right);
}

#pragma mark -
#pragma mark Metal view classes

enum class MTLViewClass : uint8_t {
	None,
	Color8,
	Color16,
	Color32,
	Color64,
	Color128,
	PVRTC_RGB_2BPP,
	PVRTC_RGB_4BPP,
	PVRTC_RGBA_2BPP,
	PVRTC_RGBA_4BPP,
	EAC_R11,
	EAC_RG11,
	EAC_RGBA8,
	ETC2_RGB8,
	ETC2_RGB8A1,
	ASTC_4x4,
	ASTC_5x4,
	ASTC_5x5,
	ASTC_6x5,
	ASTC_6x6,
	ASTC_8x5,
	ASTC_8x6,
	ASTC_8x8,
	ASTC_10x5,
	ASTC_10x6,
	ASTC_10x8,
	ASTC_10x10,
	ASTC_12x10,
	ASTC_12x12,
	BC1_RGBA,
	BC2_RGBA,
	BC3_RGBA,
	BC4_R,
	BC5_RG,
	BC6H_RGB,
	BC7_RGBA,
	Depth24_Stencil8,
	Depth32_Stencil8,
	BGRA10_XR,
	BGR10_XR
};

#pragma mark -
#pragma mark Format descriptors

/** Enumerates the data type of a format. */
enum class MTLFormatType {
	None, /**< Format type is unknown. */
	ColorHalf, /**< A 16-bit floating point color. */
	ColorFloat, /**< A 32-bit floating point color. */
	ColorInt8, /**< A signed 8-bit integer color. */
	ColorUInt8, /**< An unsigned 8-bit integer color. */
	ColorInt16, /**< A signed 16-bit integer color. */
	ColorUInt16, /**< An unsigned 16-bit integer color. */
	ColorInt32, /**< A signed 32-bit integer color. */
	ColorUInt32, /**< An unsigned 32-bit integer color. */
	DepthStencil, /**< A depth and stencil value. */
	Compressed, /**< A block-compressed color. */
};

typedef struct Extent2D {
	uint32_t width;
	uint32_t height;
} Extent2D;

/** Describes the properties of a DataFormat, including the corresponding Metal pixel and vertex format. */
typedef struct DataFormatDesc {
	RD::DataFormat dataFormat;
	MTLPixelFormat mtlPixelFormat;
	MTLPixelFormat mtlPixelFormatSubstitute;
	MTLVertexFormat mtlVertexFormat;
	MTLVertexFormat mtlVertexFormatSubstitute;
	uint8_t chromaSubsamplingPlaneCount;
	uint8_t chromaSubsamplingComponentBits;
	Extent2D blockTexelSize;
	uint32_t bytesPerBlock;
	MTLFormatType formatType;
	const char *name;
	bool hasReportedSubstitution;

	inline double bytesPerTexel() const { return (double)bytesPerBlock / (double)(blockTexelSize.width * blockTexelSize.height); }

	inline bool isSupported() const { return (mtlPixelFormat != MTLPixelFormatInvalid || chromaSubsamplingPlaneCount > 1); }
	inline bool isSupportedOrSubstitutable() const { return isSupported() || (mtlPixelFormatSubstitute != MTLPixelFormatInvalid); }

	inline bool vertexIsSupported() const { return (mtlVertexFormat != MTLVertexFormatInvalid); }
	inline bool vertexIsSupportedOrSubstitutable() const { return vertexIsSupported() || (mtlVertexFormatSubstitute != MTLVertexFormatInvalid); }
} DataFormatDesc;

/** Describes the properties of a MTLPixelFormat or MTLVertexFormat. */
typedef struct MTLFormatDesc {
	union {
		MTLPixelFormat mtlPixelFormat;
		MTLVertexFormat mtlVertexFormat;
	};
	RD::DataFormat dataFormat;
	MTLFmtCaps mtlFmtCaps;
	MTLViewClass mtlViewClass;
	MTLPixelFormat mtlPixelFormatLinear;
	const char *name = nullptr;

	inline bool isSupported() const { return (mtlPixelFormat != MTLPixelFormatInvalid) && (mtlFmtCaps != kMTLFmtCapsNone); }
} MTLFormatDesc;

class API_AVAILABLE(macos(11.0), ios(14.0)) PixelFormats {
	using DataFormat = RD::DataFormat;

public:
	/** Returns whether the DataFormat is supported by the GPU bound to this instance. */
	bool isSupported(DataFormat p_format);

	/** Returns whether the DataFormat is supported by this implementation, or can be substituted by one that is. */
	bool isSupportedOrSubstitutable(DataFormat p_format);

	/** Returns whether the specified Metal MTLPixelFormat can be used as a depth format. */
	_FORCE_INLINE_ bool isDepthFormat(MTLPixelFormat p_format) {
		switch (p_format) {
			case MTLPixelFormatDepth32Float:
			case MTLPixelFormatDepth16Unorm:
			case MTLPixelFormatDepth32Float_Stencil8:
#if TARGET_OS_OSX
			case MTLPixelFormatDepth24Unorm_Stencil8:
#endif
				return true;
			default:
				return false;
		}
	}

	/** Returns whether the specified Metal MTLPixelFormat can be used as a stencil format. */
	_FORCE_INLINE_ bool isStencilFormat(MTLPixelFormat p_format) {
		switch (p_format) {
			case MTLPixelFormatStencil8:
#if TARGET_OS_OSX
			case MTLPixelFormatDepth24Unorm_Stencil8:
			case MTLPixelFormatX24_Stencil8:
#endif
			case MTLPixelFormatDepth32Float_Stencil8:
			case MTLPixelFormatX32_Stencil8:
				return true;
			default:
				return false;
		}
	}

	/** Returns whether the specified Metal MTLPixelFormat is a PVRTC format. */
	bool isPVRTCFormat(MTLPixelFormat p_format);

	/** Returns the format type corresponding to the specified Godot pixel format, */
	MTLFormatType getFormatType(DataFormat p_format);

	/** Returns the format type corresponding to the specified Metal MTLPixelFormat, */
	MTLFormatType getFormatType(MTLPixelFormat p_format);

	/**
	 * Returns the Metal MTLPixelFormat corresponding to the specified Godot pixel
	 * or returns MTLPixelFormatInvalid if no corresponding MTLPixelFormat exists.
	 */
	MTLPixelFormat getMTLPixelFormat(DataFormat p_format);

	/**
	 * Returns the DataFormat corresponding to the specified Metal MTLPixelFormat,
	 * or returns DATA_FORMAT_MAX if no corresponding DataFormat exists.
	 */
	DataFormat getDataFormat(MTLPixelFormat p_format);

	/**
	 * Returns the size, in bytes, of a texel block of the specified Godot pixel.
	 * For uncompressed formats, the returned value corresponds to the size in bytes of a single texel.
	 */
	uint32_t getBytesPerBlock(DataFormat p_format);

	/**
	 * Returns the size, in bytes, of a texel block of the specified Metal format.
	 * For uncompressed formats, the returned value corresponds to the size in bytes of a single texel.
	 */
	uint32_t getBytesPerBlock(MTLPixelFormat p_format);

	/** Returns the number of planes of the specified chroma-subsampling (YCbCr) DataFormat */
	uint8_t getChromaSubsamplingPlaneCount(DataFormat p_format);

	/** Returns the number of bits per channel of the specified chroma-subsampling (YCbCr) DataFormat */
	uint8_t getChromaSubsamplingComponentBits(DataFormat p_format);

	/**
	 * Returns the size, in bytes, of a texel of the specified Godot format.
	 * The returned value may be fractional for certain compressed formats.
	 */
	float getBytesPerTexel(DataFormat p_format);

	/**
	 * Returns the size, in bytes, of a texel of the specified Metal format.
	 * The returned value may be fractional for certain compressed formats.
	 */
	float getBytesPerTexel(MTLPixelFormat p_format);

	/**
	 * Returns the size, in bytes, of a row of texels of the specified Godot pixel format.
	 *
	 * For compressed formats, this takes into consideration the compression block size,
	 * and p_texels_per_row should specify the width in texels, not blocks. The result is rounded
	 * up if p_texels_per_row is not an integer multiple of the compression block width.
	 */
	size_t getBytesPerRow(DataFormat p_format, uint32_t p_texels_per_row);

	/**
	 * Returns the size, in bytes, of a row of texels of the specified Metal format.
	 *
	 * For compressed formats, this takes into consideration the compression block size,
	 * and texelsPerRow should specify the width in texels, not blocks. The result is rounded
	 * up if texelsPerRow is not an integer multiple of the compression block width.
	 */
	size_t getBytesPerRow(MTLPixelFormat p_format, uint32_t p_texels_per_row);

	/**
	 * Returns the size, in bytes, of a texture layer of the specified Godot pixel format.
	 *
	 * For compressed formats, this takes into consideration the compression block size,
	 * and p_texel_rows_per_layer should specify the height in texels, not blocks. The result is
	 * rounded up if p_texel_rows_per_layer is not an integer multiple of the compression block height.
	 */
	size_t getBytesPerLayer(DataFormat p_format, size_t p_bytes_per_row, uint32_t p_texel_rows_per_layer);

	/**
	 * Returns the size, in bytes, of a texture layer of the specified Metal format.
	 * For compressed formats, this takes into consideration the compression block size,
	 * and p_texel_rows_per_layer should specify the height in texels, not blocks. The result is
	 * rounded up if p_texel_rows_per_layer is not an integer multiple of the compression block height.
	 */
	size_t getBytesPerLayer(MTLPixelFormat p_format, size_t p_bytes_per_row, uint32_t p_texel_rows_per_layer);

	/** Returns the Metal format capabilities supported by the specified Godot format, without substitution. */
	MTLFmtCaps getCapabilities(DataFormat p_format, bool p_extended = false);

	/** Returns the Metal format capabilities supported by the specified Metal format. */
	MTLFmtCaps getCapabilities(MTLPixelFormat p_format, bool p_extended = false);

	/**
	 * Returns the Metal MTLVertexFormat corresponding to the specified
	 * DataFormat as used as a vertex attribute format.
	 */
	MTLVertexFormat getMTLVertexFormat(DataFormat p_format);

#pragma mark Construction

	explicit PixelFormats(id<MTLDevice> p_device);

protected:
	id<MTLDevice> device;

	DataFormatDesc &getDataFormatDesc(DataFormat p_format);
	DataFormatDesc &getDataFormatDesc(MTLPixelFormat p_format);
	MTLFormatDesc &getMTLPixelFormatDesc(MTLPixelFormat p_format);
	MTLFormatDesc &getMTLVertexFormatDesc(MTLVertexFormat p_format);
	void initDataFormatCapabilities();
	void initMTLPixelFormatCapabilities();
	void initMTLVertexFormatCapabilities();
	void buildMTLFormatMaps();
	void buildDFFormatMaps();
	void modifyMTLFormatCapabilities();
	void modifyMTLFormatCapabilities(id<MTLDevice> p_device);
	void addMTLPixelFormatCapabilities(id<MTLDevice> p_device,
			MTLFeatureSet p_feature_set,
			MTLPixelFormat p_format,
			MTLFmtCaps p_caps);
	void addMTLPixelFormatCapabilities(id<MTLDevice> p_device,
			MTLGPUFamily p_family,
			MTLPixelFormat p_format,
			MTLFmtCaps p_caps);
	void disableMTLPixelFormatCapabilities(MTLPixelFormat p_format,
			MTLFmtCaps p_caps);
	void disableAllMTLPixelFormatCapabilities(MTLPixelFormat p_format);
	void addMTLVertexFormatCapabilities(id<MTLDevice> p_device,
			MTLFeatureSet p_feature_set,
			MTLVertexFormat p_format,
			MTLFmtCaps p_caps);

	DataFormatDesc _dataFormatDescriptions[RD::DATA_FORMAT_MAX];
	MTLFormatDesc _mtlPixelFormatDescriptions[_mtlPixelFormatCount];
	MTLFormatDesc _mtlVertexFormatDescriptions[_mtlVertexFormatCount];

	// Most Metal formats have small values and are mapped by simple lookup array.
	// Outliers are mapped by a map.
	uint16_t _mtlFormatDescIndicesByMTLPixelFormatsCore[_mtlPixelFormatCoreCount];
	HashMap<uint32_t, uint32_t> _mtlFormatDescIndicesByMTLPixelFormatsExt;

	uint16_t _mtlFormatDescIndicesByMTLVertexFormats[_mtlVertexFormatCount];
};

#pragma clang diagnostic pop

#endif // PIXEL_FORMATS_H
