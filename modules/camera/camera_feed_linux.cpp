/**************************************************************************/
/*  camera_feed_linux.cpp                                                 */
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

#include "camera_feed_linux.h"

#include "servers/rendering_server.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

void CameraFeedLinux::update_buffer_thread_func(void *p_func) {
	if (p_func) {
		CameraFeedLinux *camera_feed_linux = (CameraFeedLinux *)p_func;
		camera_feed_linux->_update_buffer();
	}
}

void CameraFeedLinux::_update_buffer() {
	while (!exit_flag.is_set()) {
		_read_frame();
		usleep(10000);
	}
}

void CameraFeedLinux::_query_device(const String &p_device_name) {
	file_descriptor = open(p_device_name.ascii().get_data(), O_RDWR | O_NONBLOCK, 0);
	ERR_FAIL_COND_MSG(file_descriptor == -1, vformat("Cannot open file descriptor for %s. Error: %d.", p_device_name, errno));

	struct v4l2_capability capability;
	if (ioctl(file_descriptor, VIDIOC_QUERYCAP, &capability) == -1) {
		ERR_FAIL_MSG(vformat("Cannot query device. Error: %d.", errno));
	}
	name = String((char *)capability.card);

	for (int index = 0;; index++) {
		struct v4l2_fmtdesc fmtdesc;
		memset(&fmtdesc, 0, sizeof(fmtdesc));
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmtdesc.index = index;

		if (ioctl(file_descriptor, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
			break;
		}

		for (int res_index = 0;; res_index++) {
			struct v4l2_frmsizeenum frmsizeenum;
			memset(&frmsizeenum, 0, sizeof(frmsizeenum));
			frmsizeenum.pixel_format = fmtdesc.pixelformat;
			frmsizeenum.index = res_index;

			if (ioctl(file_descriptor, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1) {
				break;
			}

			for (int framerate_index = 0;; framerate_index++) {
				struct v4l2_frmivalenum frmivalenum;
				memset(&frmivalenum, 0, sizeof(frmivalenum));
				frmivalenum.pixel_format = fmtdesc.pixelformat;
				frmivalenum.width = frmsizeenum.discrete.width;
				frmivalenum.height = frmsizeenum.discrete.height;
				frmivalenum.index = framerate_index;

				if (ioctl(file_descriptor, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum) == -1) {
					if (framerate_index == 0) {
						_add_format(fmtdesc, frmsizeenum.discrete, -1, 1);
					}
					break;
				}

				_add_format(fmtdesc, frmsizeenum.discrete, frmivalenum.discrete.numerator, frmivalenum.discrete.denominator);
			}
		}
	}

	close(file_descriptor);
}

void CameraFeedLinux::_add_format(v4l2_fmtdesc p_description, v4l2_frmsize_discrete p_size, int p_frame_numerator, int p_frame_denominator) {
	FeedFormat feed_format;
	feed_format.width = p_size.width;
	feed_format.height = p_size.height;
	feed_format.format = String((char *)p_description.description);
	feed_format.frame_numerator = p_frame_numerator;
	feed_format.frame_denominator = p_frame_denominator;
	feed_format.pixel_format = p_description.pixelformat;
	print_verbose(vformat("%s %dx%d@%d/%dfps", (char *)p_description.description, p_size.width, p_size.height, p_frame_denominator, p_frame_numerator));
	formats.push_back(feed_format);
}

bool CameraFeedLinux::_request_buffers() {
	struct v4l2_requestbuffers requestbuffers;

	memset(&requestbuffers, 0, sizeof(requestbuffers));
	requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	requestbuffers.memory = V4L2_MEMORY_MMAP;
	requestbuffers.count = 4;

	if (ioctl(file_descriptor, VIDIOC_REQBUFS, &requestbuffers) == -1) {
		ERR_FAIL_V_MSG(false, vformat("ioctl(VIDIOC_REQBUFS) error: %d.", errno));
	}

	ERR_FAIL_COND_V_MSG(requestbuffers.count < 2, false, "Not enough buffers granted.");

	buffer_count = requestbuffers.count;
	buffers = new StreamingBuffer[buffer_count];

	for (unsigned int i = 0; i < buffer_count; i++) {
		struct v4l2_buffer buffer;

		memset(&buffer, 0, sizeof(buffer));
		buffer.type = requestbuffers.type;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		if (ioctl(file_descriptor, VIDIOC_QUERYBUF, &buffer) == -1) {
			delete[] buffers;
			ERR_FAIL_V_MSG(false, vformat("ioctl(VIDIOC_QUERYBUF) error: %d.", errno));
		}

		buffers[i].length = buffer.length;
		buffers[i].start = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, buffer.m.offset);

		if (buffers[i].start == MAP_FAILED) {
			for (unsigned int b = 0; b < i; b++) {
				_unmap_buffers(i);
			}
			delete[] buffers;
			ERR_FAIL_V_MSG(false, "Mapping buffers failed.");
		}
	}

	return true;
}

bool CameraFeedLinux::_start_capturing() {
	for (unsigned int i = 0; i < buffer_count; i++) {
		struct v4l2_buffer buffer;

		memset(&buffer, 0, sizeof(buffer));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		if (ioctl(file_descriptor, VIDIOC_QBUF, &buffer) == -1) {
			ERR_FAIL_V_MSG(false, vformat("ioctl(VIDIOC_QBUF) error: %d.", errno));
		}
	}

	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(file_descriptor, VIDIOC_STREAMON, &type) == -1) {
		ERR_FAIL_V_MSG(false, vformat("ioctl(VIDIOC_STREAMON) error: %d.", errno));
	}

	return true;
}

void CameraFeedLinux::_read_frame() {
	struct v4l2_buffer buffer;
	memset(&buffer, 0, sizeof(buffer));
	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;

	if (ioctl(file_descriptor, VIDIOC_DQBUF, &buffer) == -1) {
		if (errno != EAGAIN) {
			print_error(vformat("ioctl(VIDIOC_DQBUF) error: %d.", errno));
			exit_flag.set();
		}
		return;
	}

	buffer_decoder->decode(buffers[buffer.index]);

	if (ioctl(file_descriptor, VIDIOC_QBUF, &buffer) == -1) {
		print_error(vformat("ioctl(VIDIOC_QBUF) error: %d.", errno));
	}

	emit_signal(SNAME("frame_changed"));
}

void CameraFeedLinux::_stop_capturing() {
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(file_descriptor, VIDIOC_STREAMOFF, &type) == -1) {
		print_error(vformat("ioctl(VIDIOC_STREAMOFF) error: %d.", errno));
	}
}

void CameraFeedLinux::_unmap_buffers(unsigned int p_count) {
	for (unsigned int i = 0; i < p_count; i++) {
		munmap(buffers[i].start, buffers[i].length);
	}
}

void CameraFeedLinux::_start_thread() {
	exit_flag.clear();
	thread = memnew(Thread);
	thread->start(CameraFeedLinux::update_buffer_thread_func, this);
}

String CameraFeedLinux::get_device_name() const {
	return device_name;
}

bool CameraFeedLinux::activate_feed() {
	ERR_FAIL_COND_V_MSG(selected_format == -1, false, "CameraFeed format needs to be set before activating.");
	file_descriptor = open(device_name.ascii().get_data(), O_RDWR | O_NONBLOCK, 0);
	if (_request_buffers() && _start_capturing()) {
		buffer_decoder = _create_buffer_decoder();
		_start_thread();
		return true;
	}
	ERR_FAIL_V_MSG(false, "Could not activate feed.");
}

BufferDecoder *CameraFeedLinux::_create_buffer_decoder() {
	switch (formats[selected_format].pixel_format) {
		case V4L2_PIX_FMT_MJPEG:
		case V4L2_PIX_FMT_JPEG:
			return memnew(JpegBufferDecoder(this));
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YYUV:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY: {
			String output = parameters["output"];
			if (output == "separate") {
				return memnew(SeparateYuyvBufferDecoder(this));
			}
			if (output == "grayscale") {
				return memnew(YuyvToGrayscaleBufferDecoder(this));
			}
			if (output == "copy") {
				return memnew(CopyBufferDecoder(this, false));
			}
			return memnew(YuyvToRgbBufferDecoder(this));
		}
		default:
			return memnew(CopyBufferDecoder(this, true));
	}
}

void CameraFeedLinux::deactivate_feed() {
	exit_flag.set();
	thread->wait_to_finish();
	memdelete(thread);
	_stop_capturing();
	_unmap_buffers(buffer_count);
	delete[] buffers;
	memdelete(buffer_decoder);
	for (int i = 0; i < CameraServer::FEED_IMAGES; i++) {
		RID placeholder = RenderingServer::get_singleton()->texture_2d_placeholder_create();
		RenderingServer::get_singleton()->texture_replace(texture[i], placeholder);
	}
	base_width = 0;
	base_height = 0;
	close(file_descriptor);

	emit_signal(SNAME("format_changed"));
}

Array CameraFeedLinux::get_formats() const {
	Array result;
	for (const FeedFormat &format : formats) {
		Dictionary dictionary;
		dictionary["width"] = format.width;
		dictionary["height"] = format.height;
		dictionary["format"] = format.format;
		dictionary["frame_numerator"] = format.frame_numerator;
		dictionary["frame_denominator"] = format.frame_denominator;
		result.push_back(dictionary);
	}
	return result;
}

CameraFeed::FeedFormat CameraFeedLinux::get_format() const {
	FeedFormat feed_format = {};
	return selected_format == -1 ? feed_format : formats[selected_format];
}

bool CameraFeedLinux::set_format(int p_index, const Dictionary &p_parameters) {
	ERR_FAIL_COND_V_MSG(active, false, "Feed is active.");
	ERR_FAIL_INDEX_V_MSG(p_index, formats.size(), false, "Invalid format index.");

	FeedFormat feed_format = formats[p_index];

	file_descriptor = open(device_name.ascii().get_data(), O_RDWR | O_NONBLOCK, 0);

	struct v4l2_format format;
	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = feed_format.width;
	format.fmt.pix.height = feed_format.height;
	format.fmt.pix.pixelformat = feed_format.pixel_format;

	if (ioctl(file_descriptor, VIDIOC_S_FMT, &format) == -1) {
		close(file_descriptor);
		ERR_FAIL_V_MSG(false, vformat("Cannot set format, error: %d.", errno));
	}

	if (feed_format.frame_numerator > 0) {
		struct v4l2_streamparm param;
		memset(&param, 0, sizeof(param));

		param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		param.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param.parm.capture.timeperframe.numerator = feed_format.frame_numerator;
		param.parm.capture.timeperframe.denominator = feed_format.frame_denominator;

		if (ioctl(file_descriptor, VIDIOC_S_PARM, &param) == -1) {
			close(file_descriptor);
			ERR_FAIL_V_MSG(false, vformat("Cannot set framerate, error: %d.", errno));
		}
	}
	close(file_descriptor);

	parameters = p_parameters.duplicate();
	selected_format = p_index;
	emit_signal(SNAME("format_changed"));

	return true;
}

CameraFeedLinux::CameraFeedLinux(const String &p_device_name) :
		CameraFeed() {
	device_name = p_device_name;
	_query_device(device_name);
}

CameraFeedLinux::~CameraFeedLinux() {
	if (is_active()) {
		deactivate_feed();
	}
}
