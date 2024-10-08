/**************************************************************************/
/*  buffer_decoder.cpp                                                    */
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

#include "buffer_decoder.h"

#include "servers/camera/camera_feed.h"

#include <linux/videodev2.h>

BufferDecoder::BufferDecoder(CameraFeed *p_camera_feed) {
	camera_feed = p_camera_feed;
	width = camera_feed->get_format().width;
	height = camera_feed->get_format().height;
	image.instantiate();
}

AbstractYuyvBufferDecoder::AbstractYuyvBufferDecoder(CameraFeed *p_camera_feed) :
		BufferDecoder(p_camera_feed) {
	switch (camera_feed->get_format().pixel_format) {
		case V4L2_PIX_FMT_YYUV:
			component_indexes = new int[4]{ 0, 1, 2, 3 };
			break;
		case V4L2_PIX_FMT_YVYU:
			component_indexes = new int[4]{ 0, 2, 3, 1 };
			break;
		case V4L2_PIX_FMT_UYVY:
			component_indexes = new int[4]{ 1, 3, 0, 2 };
			break;
		case V4L2_PIX_FMT_VYUY:
			component_indexes = new int[4]{ 1, 3, 2, 0 };
			break;
		default:
			component_indexes = new int[4]{ 0, 2, 1, 3 };
	}
}

AbstractYuyvBufferDecoder::~AbstractYuyvBufferDecoder() {
	delete[] component_indexes;
}

SeparateYuyvBufferDecoder::SeparateYuyvBufferDecoder(CameraFeed *p_camera_feed) :
		AbstractYuyvBufferDecoder(p_camera_feed) {
	y_image_data.resize(width * height);
	cbcr_image_data.resize(width * height);
	y_image.instantiate();
	cbcr_image.instantiate();
}

void SeparateYuyvBufferDecoder::decode(StreamingBuffer p_buffer) {
	uint8_t *y_dst = (uint8_t *)y_image_data.ptrw();
	uint8_t *uv_dst = (uint8_t *)cbcr_image_data.ptrw();
	uint8_t *src = (uint8_t *)p_buffer.start;
	uint8_t *y0_src = src + component_indexes[0];
	uint8_t *y1_src = src + component_indexes[1];
	uint8_t *u_src = src + component_indexes[2];
	uint8_t *v_src = src + component_indexes[3];

	for (int i = 0; i < width * height; i += 2) {
		*y_dst++ = *y0_src;
		*y_dst++ = *y1_src;
		*uv_dst++ = *u_src;
		*uv_dst++ = *v_src;

		y0_src += 4;
		y1_src += 4;
		u_src += 4;
		v_src += 4;
	}

	if (y_image.is_valid()) {
		y_image->set_data(width, height, false, Image::FORMAT_L8, y_image_data);
	} else {
		y_image.instantiate(width, height, false, Image::FORMAT_RGB8, y_image_data);
	}
	if (cbcr_image.is_valid()) {
		cbcr_image->set_data(width, height, false, Image::FORMAT_L8, cbcr_image_data);
	} else {
		cbcr_image.instantiate(width, height, false, Image::FORMAT_RGB8, cbcr_image_data);
	}

	camera_feed->set_ycbcr_images(y_image, cbcr_image);
}

YuyvToGrayscaleBufferDecoder::YuyvToGrayscaleBufferDecoder(CameraFeed *p_camera_feed) :
		AbstractYuyvBufferDecoder(p_camera_feed) {
	image_data.resize(width * height);
}

void YuyvToGrayscaleBufferDecoder::decode(StreamingBuffer p_buffer) {
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	uint8_t *src = (uint8_t *)p_buffer.start;
	uint8_t *y0_src = src + component_indexes[0];
	uint8_t *y1_src = src + component_indexes[1];

	for (int i = 0; i < width * height; i += 2) {
		*dst++ = *y0_src;
		*dst++ = *y1_src;

		y0_src += 4;
		y1_src += 4;
	}

	if (image.is_valid()) {
		image->set_data(width, height, false, Image::FORMAT_L8, image_data);
	} else {
		image.instantiate(width, height, false, Image::FORMAT_RGB8, image_data);
	}

	camera_feed->set_rgb_image(image);
}

YuyvToRgbBufferDecoder::YuyvToRgbBufferDecoder(CameraFeed *p_camera_feed) :
		AbstractYuyvBufferDecoder(p_camera_feed) {
	image_data.resize(width * height * 3);
}

void YuyvToRgbBufferDecoder::decode(StreamingBuffer p_buffer) {
	uint8_t *src = (uint8_t *)p_buffer.start;
	uint8_t *y0_src = src + component_indexes[0];
	uint8_t *y1_src = src + component_indexes[1];
	uint8_t *u_src = src + component_indexes[2];
	uint8_t *v_src = src + component_indexes[3];
	uint8_t *dst = (uint8_t *)image_data.ptrw();

	for (int i = 0; i < width * height; i += 2) {
		int u = *u_src;
		int v = *v_src;
		int u1 = (((u - 128) << 7) + (u - 128)) >> 6;
		int rg = (((u - 128) << 1) + (u - 128) + ((v - 128) << 2) + ((v - 128) << 1)) >> 3;
		int v1 = (((v - 128) << 1) + (v - 128)) >> 1;

		*dst++ = CLAMP(*y0_src + v1, 0, 255);
		*dst++ = CLAMP(*y0_src - rg, 0, 255);
		*dst++ = CLAMP(*y0_src + u1, 0, 255);

		*dst++ = CLAMP(*y1_src + v1, 0, 255);
		*dst++ = CLAMP(*y1_src - rg, 0, 255);
		*dst++ = CLAMP(*y1_src + u1, 0, 255);

		y0_src += 4;
		y1_src += 4;
		u_src += 4;
		v_src += 4;
	}

	if (image.is_valid()) {
		image->set_data(width, height, false, Image::FORMAT_RGB8, image_data);
	} else {
		image.instantiate(width, height, false, Image::FORMAT_RGB8, image_data);
	}

	camera_feed->set_rgb_image(image);
}

CopyBufferDecoder::CopyBufferDecoder(CameraFeed *p_camera_feed, bool p_rgba) :
		BufferDecoder(p_camera_feed) {
	rgba = p_rgba;
	image_data.resize(width * height * (rgba ? 4 : 2));
}

void CopyBufferDecoder::decode(StreamingBuffer p_buffer) {
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	memcpy(dst, p_buffer.start, p_buffer.length);

	if (image.is_valid()) {
		image->set_data(width, height, false, rgba ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8, image_data);
	} else {
		image.instantiate(width, height, false, rgba ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8, image_data);
	}

	camera_feed->set_rgb_image(image);
}

JpegBufferDecoder::JpegBufferDecoder(CameraFeed *p_camera_feed) :
		BufferDecoder(p_camera_feed) {
}

void JpegBufferDecoder::decode(StreamingBuffer p_buffer) {
	image_data.resize(p_buffer.length);
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	memcpy(dst, p_buffer.start, p_buffer.length);
	if (image->load_jpg_from_buffer(image_data) == OK) {
		camera_feed->set_rgb_image(image);
	}
}
