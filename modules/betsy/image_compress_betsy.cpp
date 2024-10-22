/**************************************************************************/
/*  image_compress_betsy.cpp                                              */
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

#include "image_compress_betsy.h"

#include "core/config/project_settings.h"

#include "betsy_bc1.h"

#include "bc1.glsl.gen.h"
#include "bc4.glsl.gen.h"
#include "bc6h.glsl.gen.h"

static Mutex betsy_mutex;
static BetsyCompressor *betsy = nullptr;

void BetsyCompressor::_init() {
	// Create local RD.
	RenderingContextDriver *rcd = nullptr;
	RenderingDevice *rd = RenderingServer::get_singleton()->create_local_rendering_device();

	if (rd == nullptr) {
#if defined(RD_ENABLED)
#if defined(METAL_ENABLED)
		rcd = memnew(RenderingContextDriverMetal);
		rd = memnew(RenderingDevice);
#endif
#if defined(VULKAN_ENABLED)
		if (rcd == nullptr) {
			rcd = memnew(RenderingContextDriverVulkan);
			rd = memnew(RenderingDevice);
		}
#endif
#endif
		if (rcd != nullptr && rd != nullptr) {
			Error err = rcd->initialize();
			if (err == OK) {
				err = rd->initialize(rcd);
			}

			if (err != OK) {
				memdelete(rd);
				memdelete(rcd);
				rd = nullptr;
				rcd = nullptr;
			}
		}
	}

	ERR_FAIL_NULL_MSG(rd, "Unable to create a local RenderingDevice.");

	compress_rd = rd;
	compress_rcd = rcd;

	// Create the sampler state.
	RD::SamplerState src_sampler_state;
	{
		src_sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		src_sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		src_sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
		src_sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
		src_sampler_state.mip_filter = RD::SAMPLER_FILTER_NEAREST;
	}

	src_sampler = compress_rd->sampler_create(src_sampler_state);
}

void BetsyCompressor::init() {
	WorkerThreadPool::TaskID tid = WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &BetsyCompressor::_thread_loop), true);
	command_queue.set_pump_task_id(tid);
	command_queue.push(this, &BetsyCompressor::_assign_mt_ids, tid);
	command_queue.push_and_sync(this, &BetsyCompressor::_init);
	DEV_ASSERT(task_id == tid);
}

void BetsyCompressor::_assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id) {
	task_id = p_pump_task_id;
}

// Yield thread to WTP so other tasks can be done on it.
// Automatically regains control as soon a task is pushed to the command queue.
void BetsyCompressor::_thread_loop() {
	while (!exit) {
		WorkerThreadPool::get_singleton()->yield();
		command_queue.flush_all();
	}
}

void BetsyCompressor::_thread_exit() {
	exit = true;

	if (compress_rd != nullptr) {
		if (dxt1_encoding_table_buffer.is_valid()) {
			compress_rd->free(dxt1_encoding_table_buffer);
		}

		compress_rd->free(src_sampler);

		// Clear the shader cache, pipelines will be unreferenced automatically.
		for (KeyValue<String, BetsyShader> &E : cached_shaders) {
			if (E.value.compiled.is_valid()) {
				compress_rd->free(E.value.compiled);
			}
		}
		cached_shaders.clear();
	}
}

void BetsyCompressor::finish() {
	command_queue.push(this, &BetsyCompressor::_thread_exit);
	if (task_id != WorkerThreadPool::INVALID_TASK_ID) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(task_id);
		task_id = WorkerThreadPool::INVALID_TASK_ID;
	}

	if (compress_rd != nullptr) {
		// Free the RD (and RCD if necessary).
		memdelete(compress_rd);
		compress_rd = nullptr;
		if (compress_rcd != nullptr) {
			memdelete(compress_rcd);
			compress_rcd = nullptr;
		}
	}
}

// Helper functions.

static int get_next_multiple(int n, int m) {
	return n + (m - (n % m));
}

static String get_shader_name(BetsyFormat p_format) {
	switch (p_format) {
		case BETSY_FORMAT_BC1:
		case BETSY_FORMAT_BC1_DITHER:
			return "BC1";

		case BETSY_FORMAT_BC3:
			return "BC3";

		case BETSY_FORMAT_BC4_SIGNED:
		case BETSY_FORMAT_BC4_UNSIGNED:
			return "BC4";

		case BETSY_FORMAT_BC6_SIGNED:
		case BETSY_FORMAT_BC6_UNSIGNED:
			return "BC6";

		default:
			return "";
	}
}

Error BetsyCompressor::_compress(BetsyFormat p_format, Image *r_img) {
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();

	if (r_img->is_compressed()) {
		return ERR_INVALID_DATA;
	}

	Error err = OK;

	// Destination format.
	Image::Format dest_format = Image::FORMAT_MAX;
	RD::DataFormat dst_rd_format = RD::DATA_FORMAT_MAX;

	String version = "";

	switch (p_format) {
		case BETSY_FORMAT_BC1:
			version = "standard";
			dst_rd_format = RD::DATA_FORMAT_R32G32_UINT;
			dest_format = Image::FORMAT_DXT1;
			break;

		case BETSY_FORMAT_BC1_DITHER:
			version = "dithered";
			dst_rd_format = RD::DATA_FORMAT_R32G32_UINT;
			dest_format = Image::FORMAT_DXT1;
			break;

		case BETSY_FORMAT_BC4_UNSIGNED:
			version = "unsigned";
			dst_rd_format = RD::DATA_FORMAT_R32G32_UINT;
			dest_format = Image::FORMAT_RGTC_R;
			break;

		case BETSY_FORMAT_BC6_SIGNED:
			version = "signed";
			dst_rd_format = RD::DATA_FORMAT_R32G32B32A32_UINT;
			dest_format = Image::FORMAT_BPTC_RGBF;
			break;

		case BETSY_FORMAT_BC6_UNSIGNED:
			version = "unsigned";
			dst_rd_format = RD::DATA_FORMAT_R32G32B32A32_UINT;
			dest_format = Image::FORMAT_BPTC_RGBFU;
			break;

		default:
			err = ERR_INVALID_PARAMETER;
			break;
	}

	const String shader_name = get_shader_name(p_format) + "-" + version;
	BetsyShader shader;

	if (cached_shaders.has(shader_name)) {
		shader = cached_shaders[shader_name];

	} else {
		Ref<RDShaderFile> source;
		source.instantiate();

		switch (p_format) {
			case BETSY_FORMAT_BC1:
			case BETSY_FORMAT_BC1_DITHER:
				err = source->parse_versions_from_text(bc1_shader_glsl);
				break;

			case BETSY_FORMAT_BC4_SIGNED:
			case BETSY_FORMAT_BC4_UNSIGNED:
				err = source->parse_versions_from_text(bc4_shader_glsl);
				break;

			case BETSY_FORMAT_BC6_SIGNED:
			case BETSY_FORMAT_BC6_UNSIGNED:
				err = source->parse_versions_from_text(bc6h_shader_glsl);
				break;

			default:
				err = ERR_INVALID_PARAMETER;
				break;
		}

		if (err != OK) {
			source->print_errors("Betsy compress shader");
			return err;
		}

		// Compile the shader, return early if invalid.
		shader.compiled = compress_rd->shader_create_from_spirv(source->get_spirv_stages(version));
		if (shader.compiled.is_null()) {
			return ERR_CANT_CREATE;
		}

		// Compile the pipeline, return early if invalid.
		shader.pipeline = compress_rd->compute_pipeline_create(shader.compiled);
		if (shader.pipeline.is_null()) {
			return ERR_CANT_CREATE;
		}

		cached_shaders[shader_name] = shader;
	}

	if (shader.compiled.is_null() || shader.pipeline.is_null()) {
		return ERR_INVALID_DATA;
	}

	// src_texture format information.
	RD::TextureFormat src_texture_format;
	{
		src_texture_format.array_layers = 1;
		src_texture_format.depth = 1;
		src_texture_format.mipmaps = 1;
		src_texture_format.texture_type = RD::TEXTURE_TYPE_2D;
		src_texture_format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
	}

	switch (r_img->get_format()) {
		case Image::FORMAT_L8:
			r_img->convert(Image::FORMAT_RGBA8);
			src_texture_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			break;

		case Image::FORMAT_LA8:
			r_img->convert(Image::FORMAT_RGBA8);
			src_texture_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			break;

		case Image::FORMAT_R8:
			src_texture_format.format = RD::DATA_FORMAT_R8_UNORM;
			break;

		case Image::FORMAT_RG8:
			src_texture_format.format = RD::DATA_FORMAT_R8G8_UNORM;
			break;

		case Image::FORMAT_RGB8:
			r_img->convert(Image::FORMAT_RGBA8);
			src_texture_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			break;

		case Image::FORMAT_RGBA8:
			src_texture_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
			break;

		case Image::FORMAT_RH:
			src_texture_format.format = RD::DATA_FORMAT_R16_SFLOAT;
			break;

		case Image::FORMAT_RGH:
			src_texture_format.format = RD::DATA_FORMAT_R16G16_SFLOAT;
			break;

		case Image::FORMAT_RGBH:
			r_img->convert(Image::FORMAT_RGBAH);
			src_texture_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
			break;

		case Image::FORMAT_RGBAH:
			src_texture_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
			break;

		case Image::FORMAT_RF:
			src_texture_format.format = RD::DATA_FORMAT_R32_SFLOAT;
			break;

		case Image::FORMAT_RGF:
			src_texture_format.format = RD::DATA_FORMAT_R32G32_SFLOAT;
			break;

		case Image::FORMAT_RGBF:
			r_img->convert(Image::FORMAT_RGBAF);
			src_texture_format.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			break;

		case Image::FORMAT_RGBAF:
			src_texture_format.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
			break;

		case Image::FORMAT_RGBE9995:
			src_texture_format.format = RD::DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32;
			break;

		default: {
			return err;
		}
	}

	// For the destination format just copy the source format and change the usage bits.
	RD::TextureFormat dst_texture_format = src_texture_format;
	dst_texture_format.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	dst_texture_format.format = dst_rd_format;

	// Encoding table setup.
	if (dest_format == Image::FORMAT_DXT1 && dxt1_encoding_table_buffer.is_null()) {
		Vector<uint8_t> data;
		data.resize(1024 * 4);
		memcpy(data.ptrw(), dxt1_encoding_table, 1024 * 4);

		dxt1_encoding_table_buffer = compress_rd->storage_buffer_create(1024 * 4, data);
	}

	const int mip_count = r_img->get_mipmap_count() + 1;

	// Container for the compressed data.
	Vector<uint8_t> dst_data;
	dst_data.resize(Image::get_image_data_size(r_img->get_width(), r_img->get_height(), dest_format, r_img->has_mipmaps()));
	uint8_t *dst_data_ptr = dst_data.ptrw();

	Vector<Vector<uint8_t>> src_images;
	src_images.push_back(Vector<uint8_t>());
	Vector<uint8_t> *src_image_ptr = src_images.ptrw();

	// Compress each mipmap.
	for (int i = 0; i < mip_count; i++) {
		int64_t ofs, size;
		int width, height;
		r_img->get_mipmap_offset_size_and_dimensions(i, ofs, size, width, height);

		// Set the source texture width and size.
		src_texture_format.height = height;
		src_texture_format.width = width;

		// Set the destination texture width and size.
		dst_texture_format.height = (height + 3) >> 2;
		dst_texture_format.width = (width + 3) >> 2;

		// Create a buffer filled with the source mip layer data.
		src_image_ptr[0].resize(size);
		memcpy(src_image_ptr[0].ptrw(), r_img->ptr() + ofs, size);

		// Create the textures on the GPU.
		RID src_texture = compress_rd->texture_create(src_texture_format, RD::TextureView(), src_images);
		RID dst_texture = compress_rd->texture_create(dst_texture_format, RD::TextureView());

		Vector<RD::Uniform> uniforms;
		{
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
				u.binding = 0;
				u.append_id(src_sampler);
				u.append_id(src_texture);
				uniforms.push_back(u);
			}
			{
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
				u.binding = 1;
				u.append_id(dst_texture);
				uniforms.push_back(u);
			}

			if (dest_format == Image::FORMAT_DXT1) {
				RD::Uniform u;
				u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
				u.binding = 2;
				u.append_id(dxt1_encoding_table_buffer);
				uniforms.push_back(u);
			}
		}

		RID uniform_set = compress_rd->uniform_set_create(uniforms, shader.compiled, 0);
		RD::ComputeListID compute_list = compress_rd->compute_list_begin();

		compress_rd->compute_list_bind_compute_pipeline(compute_list, shader.pipeline);
		compress_rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);

		switch (dest_format) {
			case Image::FORMAT_BPTC_RGBFU:
			case Image::FORMAT_BPTC_RGBF: {
				BC6PushConstant push_constant;
				push_constant.sizeX = 1.0f / width;
				push_constant.sizeY = 1.0f / height;
				push_constant.padding[0] = 0;
				push_constant.padding[1] = 0;

				compress_rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(BC6PushConstant));
				compress_rd->compute_list_dispatch(compute_list, get_next_multiple(width, 32) / 32, get_next_multiple(height, 32) / 32, 1);
			} break;

			case Image::FORMAT_DXT1: {
				BC1PushConstant push_constant;
				push_constant.num_refines = 2;
				push_constant.padding[0] = 0;
				push_constant.padding[1] = 0;
				push_constant.padding[2] = 0;

				compress_rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(BC1PushConstant));
				compress_rd->compute_list_dispatch(compute_list, get_next_multiple(width, 32) / 32, get_next_multiple(height, 32) / 32, 1);
			} break;

			case Image::FORMAT_RGTC_R: {
				BC4PushConstant push_constant;
				push_constant.channel_idx = 0;
				push_constant.padding[0] = 0;
				push_constant.padding[1] = 0;
				push_constant.padding[2] = 0;

				compress_rd->compute_list_set_push_constant(compute_list, &push_constant, sizeof(BC4PushConstant));
				compress_rd->compute_list_dispatch(compute_list, 1, get_next_multiple(width, 16) / 16, get_next_multiple(height, 16) / 16);
			} break;

			default: {
			} break;
		}

		compress_rd->compute_list_end();

		compress_rd->submit();
		compress_rd->sync();

		// Copy data from the GPU to the buffer.
		const Vector<uint8_t> texture_data = compress_rd->texture_get_data(dst_texture, 0);
		int64_t dst_ofs = Image::get_image_mipmap_offset(r_img->get_width(), r_img->get_height(), dest_format, i);

		memcpy(dst_data_ptr + dst_ofs, texture_data.ptr(), texture_data.size());

		// Free the source and dest texture.
		compress_rd->free(dst_texture);
		compress_rd->free(src_texture);
	}

	src_images.clear();

	// Set the compressed data to the image.
	r_img->set_data(r_img->get_width(), r_img->get_height(), r_img->has_mipmaps(), dest_format, dst_data);

	print_verbose(vformat("Betsy: Encoding took %d ms.", OS::get_singleton()->get_ticks_msec() - start_time));

	return OK;
}

void ensure_betsy_exists() {
	betsy_mutex.lock();
	if (betsy == nullptr) {
		betsy = memnew(BetsyCompressor);
		betsy->init();
	}
	betsy_mutex.unlock();
}

Error _betsy_compress_bptc(Image *r_img, Image::UsedChannels p_channels) {
	ensure_betsy_exists();
	Image::Format format = r_img->get_format();
	Error result = ERR_UNAVAILABLE;

	if (format >= Image::FORMAT_RF && format <= Image::FORMAT_RGBE9995) {
		if (r_img->detect_signed()) {
			result = betsy->compress(BETSY_FORMAT_BC6_SIGNED, r_img);
		} else {
			result = betsy->compress(BETSY_FORMAT_BC6_UNSIGNED, r_img);
		}
	}

	if (!GLOBAL_GET("rendering/textures/vram_compression/cache_gpu_compressor")) {
		free_device();
	}

	return result;
}

Error _betsy_compress_s3tc(Image *r_img, Image::UsedChannels p_channels) {
	ensure_betsy_exists();
	Error result = ERR_UNAVAILABLE;

	switch (p_channels) {
		case Image::USED_CHANNELS_RGB:
		case Image::USED_CHANNELS_L:
			result = betsy->compress(BETSY_FORMAT_BC1, r_img);
			break;

		case Image::USED_CHANNELS_R:
			result = betsy->compress(BETSY_FORMAT_BC4_UNSIGNED, r_img);
			break;

		default:
			break;
	}

	if (!GLOBAL_GET("rendering/textures/vram_compression/cache_gpu_compressor")) {
		free_device();
	}

	return result;
}

void free_device() {
	if (betsy != nullptr) {
		betsy->finish();
		memdelete(betsy);
	}
}
