/**************************************************************************/
/*  camera_feed_linux.h                                                   */
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

#include "buffer_decoder.h"

#include "core/os/thread.h"
#include "servers/camera/camera_feed.h"

#include <linux/videodev2.h>

struct StreamingBuffer;

class CameraFeedLinux : public CameraFeed {
private:
	SafeFlag exit_flag;
	Thread *thread = nullptr;
	String device_name;
	int file_descriptor = -1;
	StreamingBuffer *buffers = nullptr;
	unsigned int buffer_count = 0;
	BufferDecoder *buffer_decoder = nullptr;

	static void update_buffer_thread_func(void *p_func);

	void _update_buffer();
	void _query_device(const String &p_device_name);
	void _add_format(v4l2_fmtdesc description, v4l2_frmsize_discrete size, int frame_numerator, int frame_denominator);
	bool _request_buffers();
	bool _start_capturing();
	void _read_frame();
	void _stop_capturing();
	void _unmap_buffers(unsigned int p_count);
	BufferDecoder *_create_buffer_decoder();
	void _start_thread();

public:
	String get_device_name() const;
	bool activate_feed();
	void deactivate_feed();
	bool set_format(int p_index, const Dictionary &p_parameters);
	Array get_formats() const;
	FeedFormat get_format() const;

	CameraFeedLinux(const String &p_device_name);
	virtual ~CameraFeedLinux();
};
