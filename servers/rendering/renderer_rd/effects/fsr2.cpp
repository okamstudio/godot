/**************************************************************************/
/*  fsr2.cpp                                                              */
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

#include "fsr2.h"

#include "../storage_rd/material_storage.h"
#include "../uniform_set_cache_rd.h"

using namespace RendererRD;

#ifndef _MSC_VER
#include <wchar.h>
#define wcscpy_s wcscpy
#endif

static RD::TextureType ffx_resource_type_to_rd_texture_type(FfxResourceType p_type) {
	switch (p_type) {
		case FFX_RESOURCE_TYPE_TEXTURE1D:
			return RD::TEXTURE_TYPE_1D;
		case FFX_RESOURCE_TYPE_TEXTURE2D:
			return RD::TEXTURE_TYPE_2D;
		case FFX_RESOURCE_TYPE_TEXTURE3D:
			return RD::TEXTURE_TYPE_3D;
		default:
			return RD::TEXTURE_TYPE_MAX;
	}
}

static FfxResourceType rd_texture_type_to_ffx_resource_type(RD::TextureType p_type) {
	switch (p_type) {
		case RD::TEXTURE_TYPE_1D:
			return FFX_RESOURCE_TYPE_TEXTURE1D;
		case RD::TEXTURE_TYPE_2D:
			return FFX_RESOURCE_TYPE_TEXTURE2D;
		case RD::TEXTURE_TYPE_3D:
			return FFX_RESOURCE_TYPE_TEXTURE3D;
		default:
			return FFX_RESOURCE_TYPE_BUFFER;
	}
}

static RD::DataFormat ffx_surface_format_to_rd_format(FfxSurfaceFormat p_format) {
	switch (p_format) {
		case FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
			return RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		case FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT:
			return RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT:
			return RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16B16A16_UNORM:
			return RD::DATA_FORMAT_R16G16B16A16_UNORM;
		case FFX_SURFACE_FORMAT_R32G32_FLOAT:
			return RD::DATA_FORMAT_R32G32_SFLOAT;
		case FFX_SURFACE_FORMAT_R32_UINT:
			return RD::DATA_FORMAT_R32_UINT;
		case FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
			return RD::DATA_FORMAT_R8G8B8A8_UNORM;
		case FFX_SURFACE_FORMAT_R8G8B8A8_UNORM:
			return RD::DATA_FORMAT_R8G8B8A8_UNORM;
		case FFX_SURFACE_FORMAT_R11G11B10_FLOAT:
			return RD::DATA_FORMAT_B10G11R11_UFLOAT_PACK32;
		case FFX_SURFACE_FORMAT_R16G16_FLOAT:
			return RD::DATA_FORMAT_R16G16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16_UINT:
			return RD::DATA_FORMAT_R16G16_UINT;
		case FFX_SURFACE_FORMAT_R16_FLOAT:
			return RD::DATA_FORMAT_R16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16_UINT:
			return RD::DATA_FORMAT_R16_UINT;
		case FFX_SURFACE_FORMAT_R16_UNORM:
			return RD::DATA_FORMAT_R16_UNORM;
		case FFX_SURFACE_FORMAT_R16_SNORM:
			return RD::DATA_FORMAT_R16_SNORM;
		case FFX_SURFACE_FORMAT_R8_UNORM:
			return RD::DATA_FORMAT_R8_UNORM;
		case FFX_SURFACE_FORMAT_R8_UINT:
			return RD::DATA_FORMAT_R8_UINT;
		case FFX_SURFACE_FORMAT_R8G8_UNORM:
			return RD::DATA_FORMAT_R8G8_UNORM;
		case FFX_SURFACE_FORMAT_R32_FLOAT:
			return RD::DATA_FORMAT_R32_SFLOAT;
		default:
			return RD::DATA_FORMAT_MAX;
	}
}

static FfxSurfaceFormat rd_format_to_ffx_surface_format(RD::DataFormat p_format) {
	switch (p_format) {
		case RD::DATA_FORMAT_R32G32B32A32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
		case RD::DATA_FORMAT_R16G16B16A16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		case RD::DATA_FORMAT_R16G16B16A16_UNORM:
			return FFX_SURFACE_FORMAT_R16G16B16A16_UNORM;
		case RD::DATA_FORMAT_R32G32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32G32_FLOAT;
		case RD::DATA_FORMAT_R32_UINT:
			return FFX_SURFACE_FORMAT_R32_UINT;
		case RD::DATA_FORMAT_R8G8B8A8_UNORM:
			return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
		case RD::DATA_FORMAT_B10G11R11_UFLOAT_PACK32:
			return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
		case RD::DATA_FORMAT_R16G16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16G16_FLOAT;
		case RD::DATA_FORMAT_R16G16_UINT:
			return FFX_SURFACE_FORMAT_R16G16_UINT;
		case RD::DATA_FORMAT_R16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16_FLOAT;
		case RD::DATA_FORMAT_R16_UINT:
			return FFX_SURFACE_FORMAT_R16_UINT;
		case RD::DATA_FORMAT_R16_UNORM:
			return FFX_SURFACE_FORMAT_R16_UNORM;
		case RD::DATA_FORMAT_R16_SNORM:
			return FFX_SURFACE_FORMAT_R16_SNORM;
		case RD::DATA_FORMAT_R8_UNORM:
			return FFX_SURFACE_FORMAT_R8_UNORM;
		case RD::DATA_FORMAT_R8_UINT:
			return FFX_SURFACE_FORMAT_R8_UINT;
		case RD::DATA_FORMAT_R8G8_UNORM:
			return FFX_SURFACE_FORMAT_R8G8_UNORM;
		case RD::DATA_FORMAT_R32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32_FLOAT;
		default:
			return FFX_SURFACE_FORMAT_UNKNOWN;
	}
}

static uint32_t ffx_usage_to_rd_usage_flags(uint32_t p_flags) {
	uint32_t ret = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;

	if (p_flags & FFX_RESOURCE_USAGE_RENDERTARGET) {
		ret |= RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (p_flags & FFX_RESOURCE_USAGE_UAV) {
		ret |= RD::TEXTURE_USAGE_STORAGE_BIT;
		ret |= RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		ret |= RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	}

	return ret;
}

static FfxErrorCode create_backend_context_rd(FfxFsr2Interface *p_backend_interface, FfxDevice p_device) {
	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);

	// Store pointer to the device common to all contexts.
	scratch.device = p_device;

	// Create a ring buffer of uniform buffers.
	// FIXME: This could be optimized to be a single memory block if it was possible for RD to create views into a particular memory range of a UBO.
	for (uint32_t i = 0; i < FSR2_UBO_RING_BUFFER_SIZE; i++) {
		scratch.ubo_ring_buffer[i] = RD::get_singleton()->uniform_buffer_create(FFX_MAX_CONST_SIZE * sizeof(uint32_t));
		ERR_FAIL_COND_V(scratch.ubo_ring_buffer[i].is_null(), FFX_ERROR_BACKEND_API_ERROR);
	}

	return FFX_OK;
}

static FfxErrorCode get_device_capabilities_rd(FfxFsr2Interface *p_backend_interface, FfxDeviceCapabilities *p_out_device_capabilities, FfxDevice p_device) {
	FSR2Effect::Device &effect_device = *reinterpret_cast<FSR2Effect::Device *>(p_device);

	*p_out_device_capabilities = effect_device.capabilities;

	return FFX_OK;
}

static FfxErrorCode destroy_backend_context_rd(FfxFsr2Interface *p_backend_interface) {
	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);

	for (uint32_t i = 0; i < FSR2_UBO_RING_BUFFER_SIZE; i++) {
		RD::get_singleton()->free(scratch.ubo_ring_buffer[i]);
	}

	return FFX_OK;
}

static FfxErrorCode create_resource_rd(FfxFsr2Interface *p_backend_interface, const FfxCreateResourceDescription *p_create_resource_description, FfxResourceInternal *p_out_resource) {
	// FSR2's base implementation won't issue a call to create a heap type that isn't just default on its own,
	// so we can safely ignore it as RD does not expose this concept.
	ERR_FAIL_COND_V(p_create_resource_description->heapType != FFX_HEAP_TYPE_DEFAULT, FFX_ERROR_INVALID_ARGUMENT);

	RenderingDevice *rd = RD::get_singleton();
	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	FfxResourceDescription res_desc = p_create_resource_description->resourceDescription;

	// FSR2's base implementation never requests buffer creation.
	ERR_FAIL_COND_V(res_desc.type != FFX_RESOURCE_TYPE_TEXTURE1D && res_desc.type != FFX_RESOURCE_TYPE_TEXTURE2D && res_desc.type != FFX_RESOURCE_TYPE_TEXTURE3D, FFX_ERROR_INVALID_ARGUMENT);

	if (res_desc.mipCount == 0) {
		// Mipmap count must be derived from the resource's dimensions.
		res_desc.mipCount = uint32_t(1 + floor(log2(MAX(MAX(res_desc.width, res_desc.height), res_desc.depth))));
	}

	Vector<PackedByteArray> initial_data;
	if (p_create_resource_description->initDataSize) {
		PackedByteArray byte_array;
		byte_array.resize(p_create_resource_description->initDataSize);
		memcpy(byte_array.ptrw(), p_create_resource_description->initData, p_create_resource_description->initDataSize);
		initial_data.push_back(byte_array);
	}

	RD::TextureFormat texture_format;
	texture_format.texture_type = ffx_resource_type_to_rd_texture_type(res_desc.type);
	texture_format.format = ffx_surface_format_to_rd_format(res_desc.format);
	texture_format.usage_bits = ffx_usage_to_rd_usage_flags(p_create_resource_description->usage);
	texture_format.width = res_desc.width;
	texture_format.height = res_desc.height;
	texture_format.depth = res_desc.depth;
	texture_format.mipmaps = res_desc.mipCount;
	texture_format.is_discardable = true;

	RID texture = rd->texture_create(texture_format, RD::TextureView(), initial_data);
	ERR_FAIL_COND_V(texture.is_null(), FFX_ERROR_BACKEND_API_ERROR);

	rd->set_resource_name(texture, String(p_create_resource_description->name));

	// Add the resource to the storage and use the internal index to reference it.
	p_out_resource->internalIndex = scratch.resources.add(texture, false, p_create_resource_description->id, res_desc);

	return FFX_OK;
}

static FfxErrorCode register_resource_rd(FfxFsr2Interface *p_backend_interface, const FfxResource *p_in_resource, FfxResourceInternal *p_out_resource) {
	if (p_in_resource->resource == nullptr) {
		// Null resource case.
		p_out_resource->internalIndex = -1;
		return FFX_OK;
	}

	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	const RID &rid = *reinterpret_cast<const RID *>(p_in_resource->resource);
	ERR_FAIL_COND_V(rid.is_null(), FFX_ERROR_INVALID_ARGUMENT);

	// Add the resource to the storage and use the internal index to reference it.
	p_out_resource->internalIndex = scratch.resources.add(rid, true, FSR2Context::RESOURCE_ID_DYNAMIC, p_in_resource->description);

	return FFX_OK;
}

static FfxErrorCode unregister_resources_rd(FfxFsr2Interface *p_backend_interface) {
	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	LocalVector<uint32_t> dynamic_list_copy = scratch.resources.dynamic_list;
	for (uint32_t i : dynamic_list_copy) {
		scratch.resources.remove(i);
	}

	return FFX_OK;
}

static FfxResourceDescription get_resource_description_rd(FfxFsr2Interface *p_backend_interface, FfxResourceInternal p_resource) {
	if (p_resource.internalIndex != -1) {
		FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
		return scratch.resources.descriptions[p_resource.internalIndex];
	} else {
		return {};
	}
}

static FfxErrorCode destroy_resource_rd(FfxFsr2Interface *p_backend_interface, FfxResourceInternal p_resource) {
	if (p_resource.internalIndex != -1) {
		FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
		if (scratch.resources.rids[p_resource.internalIndex].is_valid()) {
			RD::get_singleton()->free(scratch.resources.rids[p_resource.internalIndex]);
			scratch.resources.remove(p_resource.internalIndex);
		}
	}

	return FFX_OK;
}

static FfxErrorCode create_pipeline_rd(FfxFsr2Interface *p_backend_interface, FfxFsr2Pass p_pass, const FfxPipelineDescription *p_pipeline_description, FfxPipelineState *p_out_pipeline) {
	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	FSR2Effect::Device &device = *reinterpret_cast<FSR2Effect::Device *>(scratch.device);
	FSR2Effect::Pass &effect_pass = device.passes[p_pass];

	if (effect_pass.pipeline.pipeline_rid.is_null()) {
		// Create pipeline for the device if it hasn't been created yet.
		effect_pass.root_signature.shader_rid = effect_pass.shader->version_get_shader(effect_pass.shader_version, effect_pass.shader_variant);
		ERR_FAIL_COND_V(effect_pass.root_signature.shader_rid.is_null(), FFX_ERROR_BACKEND_API_ERROR);

		effect_pass.pipeline.pipeline_rid = RD::get_singleton()->compute_pipeline_create(effect_pass.root_signature.shader_rid);
		ERR_FAIL_COND_V(effect_pass.pipeline.pipeline_rid.is_null(), FFX_ERROR_BACKEND_API_ERROR);
	}

	// While this is not their intended use, we use the pipeline and root signature pointers to store the
	// RIDs to the pipeline and shader that RD needs for the compute pipeline.
	p_out_pipeline->pipeline = reinterpret_cast<FfxPipeline>(&effect_pass.pipeline);
	p_out_pipeline->rootSignature = reinterpret_cast<FfxRootSignature>(&effect_pass.root_signature);

	p_out_pipeline->srvCount = effect_pass.sampled_bindings.size();
	ERR_FAIL_COND_V(p_out_pipeline->srvCount > FFX_MAX_NUM_SRVS, FFX_ERROR_OUT_OF_RANGE);
	memcpy(p_out_pipeline->srvResourceBindings, effect_pass.sampled_bindings.ptr(), sizeof(FfxResourceBinding) * p_out_pipeline->srvCount);

	p_out_pipeline->uavCount = effect_pass.storage_bindings.size();
	ERR_FAIL_COND_V(p_out_pipeline->uavCount > FFX_MAX_NUM_UAVS, FFX_ERROR_OUT_OF_RANGE);
	memcpy(p_out_pipeline->uavResourceBindings, effect_pass.storage_bindings.ptr(), sizeof(FfxResourceBinding) * p_out_pipeline->uavCount);

	p_out_pipeline->constCount = effect_pass.uniform_bindings.size();
	ERR_FAIL_COND_V(p_out_pipeline->constCount > FFX_MAX_NUM_CONST_BUFFERS, FFX_ERROR_OUT_OF_RANGE);
	memcpy(p_out_pipeline->cbResourceBindings, effect_pass.uniform_bindings.ptr(), sizeof(FfxResourceBinding) * p_out_pipeline->constCount);

	bool low_resolution_mvs = (p_pipeline_description->contextFlags & FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) == 0;

	if (p_pass == FFX_FSR2_PASS_ACCUMULATE || p_pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN) {
		// Change the binding for motion vectors in this particular pass if low resolution MVs are used.
		if (low_resolution_mvs) {
			FfxResourceBinding &binding = p_out_pipeline->srvResourceBindings[2];
			wcscpy_s(binding.name, L"r_dilated_motion_vectors");
		}
	}

	return FFX_OK;
}

static FfxErrorCode destroy_pipeline_rd(FfxFsr2Interface *p_backend_interface, FfxPipelineState *p_pipeline) {
	// We don't want to destroy pipelines when the FSR2 API deems it necessary as it'll do so whenever the context is destroyed.

	return FFX_OK;
}

static FfxErrorCode schedule_gpu_job_rd(FfxFsr2Interface *p_backend_interface, const FfxGpuJobDescription *p_job) {
	ERR_FAIL_NULL_V(p_backend_interface, FFX_ERROR_INVALID_ARGUMENT);
	ERR_FAIL_NULL_V(p_job, FFX_ERROR_INVALID_ARGUMENT);

	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	scratch.gpu_jobs.push_back(*p_job);

	return FFX_OK;
}

static FfxErrorCode execute_gpu_job_clear_float_rd(FSR2Context::Scratch &p_scratch, const FfxClearFloatJobDescription &p_job) {
	RID resource = p_scratch.resources.rids[p_job.target.internalIndex];
	FfxResourceDescription &desc = p_scratch.resources.descriptions[p_job.target.internalIndex];

	ERR_FAIL_COND_V(desc.type == FFX_RESOURCE_TYPE_BUFFER, FFX_ERROR_INVALID_ARGUMENT);

	Color color(p_job.color[0], p_job.color[1], p_job.color[2], p_job.color[3]);
	RD::get_singleton()->texture_clear(resource, color, 0, desc.mipCount, 0, 1);

	return FFX_OK;
}

static FfxErrorCode execute_gpu_job_copy_rd(FSR2Context::Scratch &p_scratch, const FfxCopyJobDescription &p_job) {
	RID src = p_scratch.resources.rids[p_job.src.internalIndex];
	RID dst = p_scratch.resources.rids[p_job.dst.internalIndex];
	FfxResourceDescription &src_desc = p_scratch.resources.descriptions[p_job.src.internalIndex];
	FfxResourceDescription &dst_desc = p_scratch.resources.descriptions[p_job.dst.internalIndex];

	ERR_FAIL_COND_V(src_desc.type == FFX_RESOURCE_TYPE_BUFFER, FFX_ERROR_INVALID_ARGUMENT);
	ERR_FAIL_COND_V(dst_desc.type == FFX_RESOURCE_TYPE_BUFFER, FFX_ERROR_INVALID_ARGUMENT);

	for (uint32_t mip_level = 0; mip_level < src_desc.mipCount; mip_level++) {
		RD::get_singleton()->texture_copy(src, dst, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(src_desc.width, src_desc.height, src_desc.depth), mip_level, mip_level, 0, 0);
	}

	return FFX_OK;
}

static FfxErrorCode execute_gpu_job_compute_rd(FSR2Context::Scratch &p_scratch, const FfxComputeJobDescription &p_job) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL_V(uniform_set_cache, FFX_ERROR_BACKEND_API_ERROR);

	FSR2Effect::RootSignature &root_signature = *reinterpret_cast<FSR2Effect::RootSignature *>(p_job.pipeline.rootSignature);
	ERR_FAIL_COND_V(root_signature.shader_rid.is_null(), FFX_ERROR_INVALID_ARGUMENT);

	FSR2Effect::Pipeline &backend_pipeline = *reinterpret_cast<FSR2Effect::Pipeline *>(p_job.pipeline.pipeline);
	ERR_FAIL_COND_V(backend_pipeline.pipeline_rid.is_null(), FFX_ERROR_INVALID_ARGUMENT);

	thread_local LocalVector<RD::Uniform> compute_uniforms;
	compute_uniforms.clear();

	for (uint32_t i = 0; i < p_job.pipeline.srvCount; i++) {
		RID texture_rid = p_scratch.resources.rids[p_job.srvs[i].internalIndex];
		RD::Uniform texture_uniform(RD::UNIFORM_TYPE_TEXTURE, p_job.pipeline.srvResourceBindings[i].slotIndex, texture_rid);
		compute_uniforms.push_back(texture_uniform);
	}

	for (uint32_t i = 0; i < p_job.pipeline.uavCount; i++) {
		RID image_rid = p_scratch.resources.rids[p_job.uavs[i].internalIndex];
		RD::Uniform storage_uniform;
		storage_uniform.uniform_type = RD::UNIFORM_TYPE_IMAGE;
		storage_uniform.binding = p_job.pipeline.uavResourceBindings[i].slotIndex;

		if (p_job.uavMip[i] > 0) {
			LocalVector<RID> &mip_slice_rids = p_scratch.resources.mip_slice_rids[p_job.uavs[i].internalIndex];
			if (mip_slice_rids.is_empty()) {
				mip_slice_rids.resize(p_scratch.resources.descriptions[p_job.uavs[i].internalIndex].mipCount);
			}

			ERR_FAIL_COND_V(p_job.uavMip[i] >= mip_slice_rids.size(), FFX_ERROR_INVALID_ARGUMENT);

			if (mip_slice_rids[p_job.uavMip[i]].is_null()) {
				mip_slice_rids[p_job.uavMip[i]] = RD::get_singleton()->texture_create_shared_from_slice(RD::TextureView(), image_rid, 0, p_job.uavMip[i]);
			}

			ERR_FAIL_COND_V(mip_slice_rids[p_job.uavMip[i]].is_null(), FFX_ERROR_BACKEND_API_ERROR);

			storage_uniform.append_id(mip_slice_rids[p_job.uavMip[i]]);
		} else {
			storage_uniform.append_id(image_rid);
		}

		compute_uniforms.push_back(storage_uniform);
	}

	for (uint32_t i = 0; i < p_job.pipeline.constCount; i++) {
		RID buffer_rid = p_scratch.ubo_ring_buffer[p_scratch.ubo_ring_buffer_index];
		p_scratch.ubo_ring_buffer_index = (p_scratch.ubo_ring_buffer_index + 1) % FSR2_UBO_RING_BUFFER_SIZE;

		RD::get_singleton()->buffer_update(buffer_rid, 0, p_job.cbs[i].uint32Size * sizeof(uint32_t), p_job.cbs[i].data);

		RD::Uniform buffer_uniform(RD::UNIFORM_TYPE_UNIFORM_BUFFER, p_job.pipeline.cbResourceBindings[i].slotIndex, buffer_rid);
		compute_uniforms.push_back(buffer_uniform);
	}

	FSR2Effect::Device &device = *reinterpret_cast<FSR2Effect::Device *>(p_scratch.device);
	RD::Uniform u_point_clamp_sampler(RD::UniformType::UNIFORM_TYPE_SAMPLER, 0, device.point_clamp_sampler);
	RD::Uniform u_linear_clamp_sampler(RD::UniformType::UNIFORM_TYPE_SAMPLER, 1, device.linear_clamp_sampler);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, backend_pipeline.pipeline_rid);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(root_signature.shader_rid, 0, u_point_clamp_sampler, u_linear_clamp_sampler), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache_vec(root_signature.shader_rid, 1, compute_uniforms), 1);
	RD::get_singleton()->compute_list_dispatch(compute_list, p_job.dimensions[0], p_job.dimensions[1], p_job.dimensions[2]);
	RD::get_singleton()->compute_list_end();

	return FFX_OK;
}

static FfxErrorCode execute_gpu_jobs_rd(FfxFsr2Interface *p_backend_interface, FfxCommandList p_command_list) {
	ERR_FAIL_NULL_V(p_backend_interface, FFX_ERROR_INVALID_ARGUMENT);

	FSR2Context::Scratch &scratch = *reinterpret_cast<FSR2Context::Scratch *>(p_backend_interface->scratchBuffer);
	FfxErrorCode error_code = FFX_OK;
	for (const FfxGpuJobDescription &job : scratch.gpu_jobs) {
		switch (job.jobType) {
			case FFX_GPU_JOB_CLEAR_FLOAT: {
				error_code = execute_gpu_job_clear_float_rd(scratch, job.clearJobDescriptor);
			} break;
			case FFX_GPU_JOB_COPY: {
				error_code = execute_gpu_job_copy_rd(scratch, job.copyJobDescriptor);
			} break;
			case FFX_GPU_JOB_COMPUTE: {
				error_code = execute_gpu_job_compute_rd(scratch, job.computeJobDescriptor);
			} break;
			default: {
				error_code = FFX_ERROR_INVALID_ARGUMENT;
			} break;
		}

		if (error_code != FFX_OK) {
			scratch.gpu_jobs.clear();
			return error_code;
		}
	}

	scratch.gpu_jobs.clear();

	return FFX_OK;
}

static FfxResource get_resource_rd(RID *p_rid, const wchar_t *p_name) {
	FfxResource res = {};
	if (p_rid->is_null()) {
		return res;
	}

	wcscpy_s(res.name, p_name);

	RD::TextureFormat texture_format = RD::get_singleton()->texture_get_format(*p_rid);
	res.description.type = rd_texture_type_to_ffx_resource_type(texture_format.texture_type);
	res.description.format = rd_format_to_ffx_surface_format(texture_format.format);
	res.description.width = texture_format.width;
	res.description.height = texture_format.height;
	res.description.depth = texture_format.depth;
	res.description.mipCount = texture_format.mipmaps;
	res.description.flags = FFX_RESOURCE_FLAGS_NONE;
	res.resource = reinterpret_cast<void *>(p_rid);
	res.isDepth = texture_format.usage_bits & RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	return res;
}

FSR2Context::~FSR2Context() {
	ffxFsr2ContextDestroy(&fsr_context);
}

FSR2Effect::FSR2Effect() {
	FfxDeviceCapabilities &capabilities = device.capabilities;
	uint64_t default_subgroup_size = RD::get_singleton()->limit_get(RD::LIMIT_SUBGROUP_SIZE);
	capabilities.minimumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
	capabilities.waveLaneCountMin = RD::get_singleton()->limit_get(RD::LIMIT_SUBGROUP_MIN_SIZE);
	capabilities.waveLaneCountMax = RD::get_singleton()->limit_get(RD::LIMIT_SUBGROUP_MAX_SIZE);
	capabilities.fp16Supported = RD::get_singleton()->has_feature(RD::Features::SUPPORTS_FSR_HALF_FLOAT);
	capabilities.raytracingSupported = false;

	bool force_wave_64 = default_subgroup_size == 32 && capabilities.waveLaneCountMax == 64;
	bool use_lut = force_wave_64 || default_subgroup_size == 64;

	String general_defines_base =
			"\n#define FFX_GPU\n"
			"\n#define FFX_GLSL 1\n"
			"\n#define FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS 1\n"
			"\n#define FFX_FSR2_OPTION_HDR_COLOR_INPUT 1\n"
			"\n#define FFX_FSR2_OPTION_INVERTED_DEPTH 1\n"
			"\n#define FFX_FSR2_OPTION_GODOT_REACTIVE_MASK_CLAMP 1\n"
			"\n#define FFX_FSR2_OPTION_GODOT_DERIVE_INVALID_MOTION_VECTORS 1\n";

	if (use_lut) {
		general_defines_base += "\n#define FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE 1\n";
	}

	String general_defines = general_defines_base;
	if (capabilities.fp16Supported) {
		general_defines += "\n#define FFX_HALF 1\n";
	}

	Vector<String> modes;
	modes.push_back("");

	// Since Godot currently lacks a shader reflection mechanism to persist the name of the bindings in the shader cache and
	// there's also no mechanism to compile the shaders offline, the bindings are created manually by looking at the GLSL
	// files included in FSR2 and mapping the macro bindings (#define FSR2_BIND_*) to their respective implementation names.
	//
	// It is not guaranteed these will remain consistent at all between versions of FSR2, so it'll be necessary to keep these
	// bindings up to date whenever the library is updated. In such cases, it is very likely the validation layer will throw an
	// error if the bindings do not match.

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_DEPTH_CLIP];
		pass.shader = &shaders.depth_clip;
		pass.shader->initialize(modes, general_defines);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_reconstructed_previous_nearest_depth" },
			FfxResourceBinding{ 1, 0, L"r_dilated_motion_vectors" },
			FfxResourceBinding{ 2, 0, L"r_dilatedDepth" },
			FfxResourceBinding{ 3, 0, L"r_reactive_mask" },
			FfxResourceBinding{ 4, 0, L"r_transparency_and_composition_mask" },
			FfxResourceBinding{ 6, 0, L"r_previous_dilated_motion_vectors" },
			FfxResourceBinding{ 7, 0, L"r_input_motion_vectors" },
			FfxResourceBinding{ 8, 0, L"r_input_color_jittered" },
			FfxResourceBinding{ 9, 0, L"r_input_depth" },
			FfxResourceBinding{ 10, 0, L"r_input_exposure" }
		};

		pass.storage_bindings = {
			// FSR2_BIND_UAV_DEPTH_CLIP (11) does not point to anything.
			FfxResourceBinding{ 12, 0, L"rw_dilated_reactive_masks" },
			FfxResourceBinding{ 13, 0, L"rw_prepared_input_color" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 14, 0, L"cbFSR2" }
		};
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_RECONSTRUCT_PREVIOUS_DEPTH];
		pass.shader = &shaders.reconstruct_previous_depth;
		pass.shader->initialize(modes, general_defines);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_motion_vectors" },
			FfxResourceBinding{ 1, 0, L"r_input_depth" },
			FfxResourceBinding{ 2, 0, L"r_input_color_jittered" },
			FfxResourceBinding{ 3, 0, L"r_input_exposure" },
			FfxResourceBinding{ 4, 0, L"r_luma_history" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 5, 0, L"rw_reconstructed_previous_nearest_depth" },
			FfxResourceBinding{ 6, 0, L"rw_dilated_motion_vectors" },
			FfxResourceBinding{ 7, 0, L"rw_dilatedDepth" },
			FfxResourceBinding{ 8, 0, L"rw_prepared_input_color" },
			FfxResourceBinding{ 9, 0, L"rw_luma_history" },
			// FSR2_BIND_UAV_LUMA_INSTABILITY (10) does not point to anything.
			FfxResourceBinding{ 11, 0, L"rw_lock_input_luma" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 12, 0, L"cbFSR2" }
		};
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_LOCK];
		pass.shader = &shaders.lock;
		pass.shader->initialize(modes, general_defines);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_lock_input_luma" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 1, 0, L"rw_new_locks" },
			FfxResourceBinding{ 2, 0, L"rw_reconstructed_previous_nearest_depth" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 3, 0, L"cbFSR2" }
		};
	}

	{
		Vector<String> accumulate_modes;
		accumulate_modes.push_back("\n");
		accumulate_modes.push_back("\n#define FFX_FSR2_OPTION_APPLY_SHARPENING 1\n");

		String general_defines_accumulate;
		if (RD::get_singleton()->get_device_vendor_name() == "NVIDIA") {
			// Workaround: Disable FP16 path for the accumulate pass on NVIDIA due to reduced occupancy and high VRAM throughput.
			general_defines_accumulate = general_defines_base;
		} else {
			general_defines_accumulate = general_defines;
		}

		Pass &pass = device.passes[FFX_FSR2_PASS_ACCUMULATE];
		pass.shader = &shaders.accumulate;
		pass.shader->initialize(accumulate_modes, general_defines_accumulate);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_exposure" },
			FfxResourceBinding{ 1, 0, L"r_dilated_reactive_masks" },
			FfxResourceBinding{ 2, 0, L"r_input_motion_vectors" },
			FfxResourceBinding{ 3, 0, L"r_internal_upscaled_color" },
			FfxResourceBinding{ 4, 0, L"r_lock_status" },
			FfxResourceBinding{ 5, 0, L"r_input_depth" },
			FfxResourceBinding{ 6, 0, L"r_prepared_input_color" },
			// FSR2_BIND_SRV_LUMA_INSTABILITY(7) does not point to anything.
			FfxResourceBinding{ 8, 0, L"r_lanczos_lut" },
			FfxResourceBinding{ 9, 0, L"r_upsample_maximum_bias_lut" },
			FfxResourceBinding{ 10, 0, L"r_imgMips" },
			FfxResourceBinding{ 11, 0, L"r_auto_exposure" },
			FfxResourceBinding{ 12, 0, L"r_luma_history" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 13, 0, L"rw_internal_upscaled_color" },
			FfxResourceBinding{ 14, 0, L"rw_lock_status" },
			FfxResourceBinding{ 15, 0, L"rw_upscaled_output" },
			FfxResourceBinding{ 16, 0, L"rw_new_locks" },
			FfxResourceBinding{ 17, 0, L"rw_luma_history" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 18, 0, L"cbFSR2" }
		};

		// Sharpen pass is a clone of the accumulate pass.
		Pass &sharpen_pass = device.passes[FFX_FSR2_PASS_ACCUMULATE_SHARPEN];
		sharpen_pass = pass;
		sharpen_pass.shader_variant = 1;
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_RCAS];
		pass.shader = &shaders.rcas;
		pass.shader->initialize(modes, general_defines_base);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_exposure" },
			FfxResourceBinding{ 1, 0, L"r_rcas_input" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 2, 0, L"rw_upscaled_output" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 3, 0, L"cbFSR2" },
			FfxResourceBinding{ 4, 0, L"cbRCAS" }
		};
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_COMPUTE_LUMINANCE_PYRAMID];
		pass.shader = &shaders.compute_luminance_pyramid;
		pass.shader->initialize(modes, general_defines_base);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_color_jittered" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 1, 0, L"rw_spd_global_atomic" },
			FfxResourceBinding{ 2, 0, L"rw_img_mip_shading_change" },
			FfxResourceBinding{ 3, 0, L"rw_img_mip_5" },
			FfxResourceBinding{ 4, 0, L"rw_auto_exposure" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 5, 0, L"cbFSR2" },
			FfxResourceBinding{ 6, 0, L"cbSPD" }
		};
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_GENERATE_REACTIVE];
		pass.shader = &shaders.autogen_reactive;
		pass.shader->initialize(modes, general_defines);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_opaque_only" },
			FfxResourceBinding{ 1, 0, L"r_input_color_jittered" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 2, 0, L"rw_output_autoreactive" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 3, 0, L"cbGenerateReactive" },
			FfxResourceBinding{ 4, 0, L"cbFSR2" }
		};
	}

	{
		Pass &pass = device.passes[FFX_FSR2_PASS_TCR_AUTOGENERATE];
		pass.shader = &shaders.tcr_autogen;
		pass.shader->initialize(modes, general_defines);
		pass.shader_version = pass.shader->version_create();

		pass.sampled_bindings = {
			FfxResourceBinding{ 0, 0, L"r_input_opaque_only" },
			FfxResourceBinding{ 1, 0, L"r_input_color_jittered" },
			FfxResourceBinding{ 2, 0, L"r_input_motion_vectors" },
			FfxResourceBinding{ 3, 0, L"r_input_prev_color_pre_alpha" },
			FfxResourceBinding{ 4, 0, L"r_input_prev_color_post_alpha" },
			FfxResourceBinding{ 5, 0, L"r_reactive_mask" },
			FfxResourceBinding{ 6, 0, L"r_transparency_and_composition_mask" },
			FfxResourceBinding{ 13, 0, L"r_input_depth" }
		};

		pass.storage_bindings = {
			FfxResourceBinding{ 7, 0, L"rw_output_autoreactive" },
			FfxResourceBinding{ 8, 0, L"rw_output_autocomposition" },
			FfxResourceBinding{ 9, 0, L"rw_output_prev_color_pre_alpha" },
			FfxResourceBinding{ 10, 0, L"rw_output_prev_color_post_alpha" }
		};

		pass.uniform_bindings = {
			FfxResourceBinding{ 11, 0, L"cbFSR2" },
			FfxResourceBinding{ 12, 0, L"cbGenerateReactive" }
		};
	}

	RD::SamplerState state;
	state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
	state.min_filter = RD::SAMPLER_FILTER_NEAREST;
	state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	state.repeat_w = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	state.min_lod = -1000.0f;
	state.max_lod = 1000.0f;
	state.anisotropy_max = 1.0;
	device.point_clamp_sampler = RD::get_singleton()->sampler_create(state);
	ERR_FAIL_COND(device.point_clamp_sampler.is_null());

	state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
	state.min_filter = RD::SAMPLER_FILTER_LINEAR;
	device.linear_clamp_sampler = RD::get_singleton()->sampler_create(state);
	ERR_FAIL_COND(device.linear_clamp_sampler.is_null());
}

FSR2Effect::~FSR2Effect() {
	RD::get_singleton()->free(device.point_clamp_sampler);
	RD::get_singleton()->free(device.linear_clamp_sampler);

	for (uint32_t i = 0; i < FFX_FSR2_PASS_COUNT; i++) {
		device.passes[i].shader->version_free(device.passes[i].shader_version);
	}
}

FSR2Context *FSR2Effect::create_context(Size2i p_internal_size, Size2i p_target_size) {
	FSR2Context *context = memnew(RendererRD::FSR2Context);
	context->fsr_desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_DEPTH_INVERTED;
	context->fsr_desc.maxRenderSize.width = p_internal_size.x;
	context->fsr_desc.maxRenderSize.height = p_internal_size.y;
	context->fsr_desc.displaySize.width = p_target_size.x;
	context->fsr_desc.displaySize.height = p_target_size.y;
	context->fsr_desc.device = &device;

	FfxFsr2Interface &functions = context->fsr_desc.callbacks;
	functions.fpCreateBackendContext = create_backend_context_rd;
	functions.fpGetDeviceCapabilities = get_device_capabilities_rd;
	functions.fpDestroyBackendContext = destroy_backend_context_rd;
	functions.fpCreateResource = create_resource_rd;
	functions.fpRegisterResource = register_resource_rd;
	functions.fpUnregisterResources = unregister_resources_rd;
	functions.fpGetResourceDescription = get_resource_description_rd;
	functions.fpDestroyResource = destroy_resource_rd;
	functions.fpCreatePipeline = create_pipeline_rd;
	functions.fpDestroyPipeline = destroy_pipeline_rd;
	functions.fpScheduleGpuJob = schedule_gpu_job_rd;
	functions.fpExecuteGpuJobs = execute_gpu_jobs_rd;
	functions.scratchBuffer = &context->scratch;
	functions.scratchBufferSize = sizeof(context->scratch);

	FfxErrorCode result = ffxFsr2ContextCreate(&context->fsr_context, &context->fsr_desc);
	if (result == FFX_OK) {
		return context;
	} else {
		memdelete(context);
		return nullptr;
	}
}

void FSR2Effect::upscale(const Parameters &p_params) {
	// TODO: Transparency & Composition mask is not implemented.
	FfxFsr2DispatchDescription dispatch_desc = {};
	RID color = p_params.color;
	RID depth = p_params.depth;
	RID velocity = p_params.velocity;
	RID reactive = p_params.reactive;
	RID exposure = p_params.exposure;
	RID output = p_params.output;
	dispatch_desc.commandList = nullptr;
	dispatch_desc.color = get_resource_rd(&color, L"color");
	dispatch_desc.depth = get_resource_rd(&depth, L"depth");
	dispatch_desc.motionVectors = get_resource_rd(&velocity, L"velocity");
	dispatch_desc.reactive = get_resource_rd(&reactive, L"reactive");
	dispatch_desc.exposure = get_resource_rd(&exposure, L"exposure");
	dispatch_desc.transparencyAndComposition = {};
	dispatch_desc.output = get_resource_rd(&output, L"output");
	dispatch_desc.colorOpaqueOnly = {};
	dispatch_desc.jitterOffset.x = p_params.jitter.x;
	dispatch_desc.jitterOffset.y = p_params.jitter.y;
	dispatch_desc.motionVectorScale.x = float(p_params.internal_size.width);
	dispatch_desc.motionVectorScale.y = float(p_params.internal_size.height);
	dispatch_desc.reset = p_params.reset_accumulation;
	dispatch_desc.renderSize.width = p_params.internal_size.width;
	dispatch_desc.renderSize.height = p_params.internal_size.height;
	dispatch_desc.enableSharpening = (p_params.sharpness > 1e-6f);
	dispatch_desc.sharpness = p_params.sharpness;
	dispatch_desc.frameTimeDelta = p_params.delta_time;
	dispatch_desc.preExposure = 1.0f;
	dispatch_desc.cameraNear = p_params.z_near;
	dispatch_desc.cameraFar = p_params.z_far;
	dispatch_desc.cameraFovAngleVertical = p_params.fovy;
	dispatch_desc.viewSpaceToMetersFactor = 1.0f;
	dispatch_desc.enableAutoReactive = false;
	dispatch_desc.autoTcThreshold = 1.0f;
	dispatch_desc.autoTcScale = 1.0f;
	dispatch_desc.autoReactiveScale = 1.0f;
	dispatch_desc.autoReactiveMax = 1.0f;

	RendererRD::MaterialStorage::store_camera(p_params.reprojection, dispatch_desc.reprojectionMatrix);

	FfxErrorCode result = ffxFsr2ContextDispatch(&p_params.context->fsr_context, &dispatch_desc);
	ERR_FAIL_COND(result != FFX_OK);
}
