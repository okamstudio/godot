/**************************************************************************/
/*  camera_linux.cpp                                                      */
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

#include "camera_linux.h"

#include "camera_feed_linux.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

void CameraLinux::camera_thread_func(void *p_camera_linux) {
	if (p_camera_linux) {
		CameraLinux *camera_linux = (CameraLinux *)p_camera_linux;
		camera_linux->_update_devices();
	}
}

void CameraLinux::_update_devices() {
	while (!exit_flag.is_set()) {
		{
			MutexLock lock(camera_mutex);

			for (int i = feeds.size() - 1; i >= 0; i--) {
				Ref<CameraFeedLinux> feed = (Ref<CameraFeedLinux>)feeds[i];
				String device_name = feed->get_device_name();
				if (!_is_active(device_name)) {
					remove_feed(feed);
				}
			}

			DIR *devices = opendir("/dev");

			if (devices) {
				struct dirent *device;

				while ((device = readdir(devices)) != nullptr) {
					if (strncmp(device->d_name, "video", 5) != 0) {
						continue;
					}
					String device_name = String("/dev/") + String(device->d_name);
					if (!_has_device(device_name)) {
						_add_device(device_name);
					}
				}
			}

			closedir(devices);
		}

		usleep(1000000);
	}
}

bool CameraLinux::_has_device(const String &p_device_name) {
	for (int i = 0; i < feeds.size(); i++) {
		Ref<CameraFeedLinux> feed = (Ref<CameraFeedLinux>)feeds[i];
		if (feed->get_device_name() == p_device_name) {
			return true;
		}
	}
	return false;
}

void CameraLinux::_add_device(const String &p_device_name) {
	int file_descriptor = _open_device(p_device_name);

	if (file_descriptor != -1) {
		if (_is_video_capture_device(file_descriptor)) {
			Ref<CameraFeedLinux> feed = memnew(CameraFeedLinux(p_device_name));
			add_feed(feed);
		}
	}

	close(file_descriptor);
}

int CameraLinux::_open_device(const String &p_device_name) {
	struct stat s;

	if (stat(p_device_name.ascii(), &s) == -1) {
		return -1;
	}

	if (!S_ISCHR(s.st_mode)) {
		return -1;
	}

	return open(p_device_name.ascii(), O_RDWR | O_NONBLOCK, 0);
}

// TODO any cheaper/cleaner way to check if file descriptor is invalid?
bool CameraLinux::_is_active(const String &p_device_name) {
	struct v4l2_capability capability;
	bool result = false;
	int file_descriptor = _open_device(p_device_name);
	if (file_descriptor != -1 && ioctl(file_descriptor, VIDIOC_QUERYCAP, &capability) != -1) {
		result = true;
	}
	close(file_descriptor);
	return result;
}

bool CameraLinux::_is_video_capture_device(int p_file_descriptor) {
	struct v4l2_capability capability;

	if (ioctl(p_file_descriptor, VIDIOC_QUERYCAP, &capability) == -1) {
		return false;
	}

	if (!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		return false;
	}

	if (!(capability.capabilities & V4L2_CAP_STREAMING)) {
		return false;
	}

	return _can_query_format(p_file_descriptor, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

bool CameraLinux::_can_query_format(int p_file_descriptor, int p_type) {
	struct v4l2_format format;
	memset(&format, 0, sizeof(format));
	format.type = p_type;

	return ioctl(p_file_descriptor, VIDIOC_G_FMT, &format) != -1;
}

CameraLinux::CameraLinux() {
	camera_thread.start(CameraLinux::camera_thread_func, this);
}

CameraLinux::~CameraLinux() {
	exit_flag.set();
	camera_thread.wait_to_finish();
}
