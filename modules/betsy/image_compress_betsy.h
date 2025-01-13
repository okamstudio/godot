/**************************************************************************/
/*  image_compress_betsy.h                                                */
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

#ifndef IMAGE_COMPRESS_BETSY_H
#define IMAGE_COMPRESS_BETSY_H

#include "core/io/image.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "core/templates/command_queue_mt.h"

#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server_default.h"

#if defined(VULKAN_ENABLED)
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif
#if defined(METAL_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#endif

enum BetsyFormat {
	BETSY_FORMAT_BC1,
	BETSY_FORMAT_BC1_DITHER,
	BETSY_FORMAT_BC3,
	BETSY_FORMAT_BC4_SIGNED,
	BETSY_FORMAT_BC4_UNSIGNED,
	BETSY_FORMAT_BC5_SIGNED,
	BETSY_FORMAT_BC5_UNSIGNED,
	BETSY_FORMAT_BC6_SIGNED,
	BETSY_FORMAT_BC6_UNSIGNED,
	BETSY_FORMAT_MAX,
};

enum BetsyShaderType {
	BETSY_SHADER_BC1_STANDARD,
	BETSY_SHADER_BC1_DITHER,
	BETSY_SHADER_BC4_SIGNED,
	BETSY_SHADER_BC4_UNSIGNED,
	BETSY_SHADER_BC6_SIGNED,
	BETSY_SHADER_BC6_UNSIGNED,
	BETSY_SHADER_ALPHA_STITCH,
	BETSY_SHADER_MAX,
};

struct BC6PushConstant {
	float sizeX;
	float sizeY;
	uint32_t padding[2] = { 0 };
};

struct BC1PushConstant {
	uint32_t num_refines;
	uint32_t padding[3] = { 0 };
};

struct BC4PushConstant {
	uint32_t channel_idx;
	uint32_t padding[3] = { 0 };
};

void free_device();

Error _betsy_compress_bptc(Image *r_img, Image::UsedChannels p_channels);
Error _betsy_compress_s3tc(Image *r_img, Image::UsedChannels p_channels);

class BetsyCompressor : public Object {
	mutable CommandQueueMT command_queue;
	bool exit = false;
	WorkerThreadPool::TaskID task_id = WorkerThreadPool::INVALID_TASK_ID;

	struct BetsyShader {
		RID compiled;
		RID pipeline;
	};

	// Resources shared by all compression formats.
	RenderingDevice *compress_rd = nullptr;
	RenderingContextDriver *compress_rcd = nullptr;
	BetsyShader cached_shaders[BETSY_SHADER_MAX];
	RID src_sampler;

	// Format-specific resources.
	RID dxt1_encoding_table_buffer;

	void _init();
	void _assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id);
	void _thread_loop();
	void _thread_exit();

	Error _get_shader(BetsyFormat p_format, const String &p_version, BetsyShader &r_shader);
	Error _compress(BetsyFormat p_format, Image *r_img);

public:
	void init();
	void finish();

	Error compress(BetsyFormat p_format, Image *r_img) {
		Error err;
		command_queue.push_and_ret(this, &BetsyCompressor::_compress, &err, p_format, r_img);
		return err;
	}
};

#endif // IMAGE_COMPRESS_BETSY_H
