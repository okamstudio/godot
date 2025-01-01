/**************************************************************************/
/*  metal_device_properties.h                                             */
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

#ifndef METAL_DEVICE_PROPERTIES_H
#define METAL_DEVICE_PROPERTIES_H

#import "servers/rendering/rendering_device.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

/** The buffer index to use for vertex content. */
const static uint32_t VERT_CONTENT_BUFFER_INDEX = 0;
const static uint32_t MAX_COLOR_ATTACHMENT_COUNT = 8;

typedef NS_OPTIONS(NSUInteger, SampleCount) {
	SampleCount1 = (1UL << 0),
	SampleCount2 = (1UL << 1),
	SampleCount4 = (1UL << 2),
	SampleCount8 = (1UL << 3),
	SampleCount16 = (1UL << 4),
	SampleCount32 = (1UL << 5),
	SampleCount64 = (1UL << 6),
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) MetalFeatures {
	uint32_t mslVersion = 0;
	MTLGPUFamily highestFamily = MTLGPUFamilyApple4;
	MTLLanguageVersion mslVersionEnum = MTLLanguageVersion1_2;
	SampleCount supportedSampleCounts = SampleCount1;
	long hostMemoryPageSize = 0;
	bool layeredRendering = false;
	bool multisampleLayeredRendering = false;
	bool quadPermute = false; /**< If true, quadgroup permutation functions (vote, ballot, shuffle) are supported in shaders. */
	bool simdPermute = false; /**< If true, SIMD-group permutation functions (vote, ballot, shuffle) are supported in shaders. */
	bool simdReduction = false; /**< If true, SIMD-group reduction functions (arithmetic) are supported in shaders. */
	bool tessellationShader = false; /**< If true, tessellation shaders are supported. */
	bool imageCubeArray = false; /**< If true, image cube arrays are supported. */
	MTLArgumentBuffersTier argument_buffers_tier = MTLArgumentBuffersTier1;
};

struct MetalLimits {
	uint64_t maxImageArrayLayers;
	uint64_t maxFramebufferHeight;
	uint64_t maxFramebufferWidth;
	uint64_t maxImageDimension1D;
	uint64_t maxImageDimension2D;
	uint64_t maxImageDimension3D;
	uint64_t maxImageDimensionCube;
	uint64_t maxViewportDimensionX;
	uint64_t maxViewportDimensionY;
	MTLSize maxThreadsPerThreadGroup;
	MTLSize maxComputeWorkGroupCount;
	uint64_t maxBoundDescriptorSets;
	uint64_t maxColorAttachments;
	uint64_t maxTexturesPerArgumentBuffer;
	uint64_t maxSamplersPerArgumentBuffer;
	uint64_t maxBuffersPerArgumentBuffer;
	uint64_t maxBufferLength;
	uint64_t minUniformBufferOffsetAlignment;
	uint64_t maxVertexDescriptorLayoutStride;
	uint16_t maxViewports;
	uint32_t maxPerStageBufferCount; /**< The total number of per-stage Metal buffers available for shader uniform content and attributes. */
	uint32_t maxPerStageTextureCount; /**< The total number of per-stage Metal textures available for shader uniform content. */
	uint32_t maxPerStageSamplerCount; /**< The total number of per-stage Metal samplers available for shader uniform content. */
	uint32_t maxVertexInputAttributes;
	uint32_t maxVertexInputBindings;
	uint32_t maxVertexInputBindingStride;
	uint32_t maxDrawIndexedIndexValue;

	uint32_t minSubgroupSize; /**< The minimum number of threads in a SIMD-group. */
	uint32_t maxSubgroupSize; /**< The maximum number of threads in a SIMD-group. */
	BitField<RDD::ShaderStage> subgroupSupportedShaderStages;
	BitField<RD::SubgroupOperations> subgroupSupportedOperations; /**< The subgroup operations supported by the device. */
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MetalDeviceProperties {
private:
	void init_features(id<MTLDevice> p_device);
	void init_limits(id<MTLDevice> p_device);

public:
	MetalFeatures features;
	MetalLimits limits;

	SampleCount find_nearest_supported_sample_count(RenderingDevice::TextureSamples p_samples) const;

	MetalDeviceProperties(id<MTLDevice> p_device);
	~MetalDeviceProperties();

private:
	static const SampleCount sample_count[RenderingDevice::TextureSamples::TEXTURE_SAMPLES_MAX];
};

#endif // METAL_DEVICE_PROPERTIES_H
