/**************************************************************************/
/*  buffer_decoder.h                                                      */
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

#pragma once

#include "core/io/image.h"
#include "core/templates/vector.h"

class CameraFeed;

struct StreamingBuffer {
	void *start = nullptr;
	size_t length = 0;
};

class BufferDecoder {
protected:
	CameraFeed *camera_feed = nullptr;
	Ref<Image> image;
	int width = 0;
	int height = 0;

public:
	virtual void decode(StreamingBuffer p_buffer) = 0;

	BufferDecoder(CameraFeed *p_camera_feed);
	virtual ~BufferDecoder() {}
};

class AbstractYuyvBufferDecoder : public BufferDecoder {
protected:
	int *component_indexes = nullptr;

public:
	AbstractYuyvBufferDecoder(CameraFeed *p_camera_feed);
	~AbstractYuyvBufferDecoder();
};

class SeparateYuyvBufferDecoder : public AbstractYuyvBufferDecoder {
private:
	Vector<uint8_t> y_image_data;
	Vector<uint8_t> cbcr_image_data;
	Ref<Image> y_image;
	Ref<Image> cbcr_image;

public:
	SeparateYuyvBufferDecoder(CameraFeed *p_camera_feed);
	virtual void decode(StreamingBuffer p_buffer) override;
};

class YuyvToGrayscaleBufferDecoder : public AbstractYuyvBufferDecoder {
private:
	Vector<uint8_t> image_data;

public:
	YuyvToGrayscaleBufferDecoder(CameraFeed *p_camera_feed);
	virtual void decode(StreamingBuffer p_buffer) override;
};

class YuyvToRgbBufferDecoder : public AbstractYuyvBufferDecoder {
private:
	Vector<uint8_t> image_data;

public:
	YuyvToRgbBufferDecoder(CameraFeed *p_camera_feed);
	virtual void decode(StreamingBuffer p_buffer) override;
};

class CopyBufferDecoder : public BufferDecoder {
private:
	Vector<uint8_t> image_data;
	bool rgba = false;

public:
	CopyBufferDecoder(CameraFeed *p_camera_feed, bool p_rgba);
	virtual void decode(StreamingBuffer p_buffer) override;
};

class JpegBufferDecoder : public BufferDecoder {
private:
	Vector<uint8_t> image_data;

public:
	JpegBufferDecoder(CameraFeed *p_camera_feed);
	virtual void decode(StreamingBuffer p_buffer) override;
};
