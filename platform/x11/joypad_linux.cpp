/**************************************************************************/
/*  joypad_linux.cpp                                                      */
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

#ifdef JOYDEV_ENABLED

#include "joypad_linux.h"

#include "core/os/os.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#ifdef UDEV_ENABLED
#include "libudev-so_wrap.h"
#endif

#define LONG_BITS (sizeof(long) * 8)
#define test_bit(nr, addr) (((1UL << ((nr) % LONG_BITS)) & ((addr)[(nr) / LONG_BITS])) != 0)
#define NBITS(x) ((((x)-1) / LONG_BITS) + 1)

#ifdef UDEV_ENABLED
static const char *ignore_str = "/dev/input/js";
#endif

JoypadLinux::Joypad::Joypad() {
	fd = -1;
	dpad = 0;
	devpath = "";
	for (int i = 0; i < MAX_ABS; i++) {
		abs_info[i] = nullptr;
	}
}

JoypadLinux::Joypad::~Joypad() {
	for (int i = 0; i < MAX_ABS; i++) {
		if (abs_info[i]) {
			memdelete(abs_info[i]);
		}
	}
}

void JoypadLinux::Joypad::reset() {
	dpad = 0;
	fd = -1;
	for (int i = 0; i < MAX_ABS; i++) {
		abs_map[i] = -1;
		curr_axis[i] = 0;
	}
	events.clear();
}

#ifdef UDEV_ENABLED
// This function is derived from SDL:
// https://github.com/libsdl-org/SDL/blob/main/src/core/linux/SDL_sandbox.c#L28-L45
static bool detect_sandbox() {
	if (access("/.flatpak-info", F_OK) == 0) {
		return true;
	}

	// For Snap, we check multiple variables because they might be set for
	// unrelated reasons. This is the same thing WebKitGTK does.
	if (OS::get_singleton()->has_environment("SNAP") && OS::get_singleton()->has_environment("SNAP_NAME") && OS::get_singleton()->has_environment("SNAP_REVISION")) {
		return true;
	}

	if (access("/run/host/container-manager", F_OK) == 0) {
		return true;
	}

	return false;
}
#endif // UDEV_ENABLED

JoypadLinux::JoypadLinux(InputDefault *in) {
#ifdef UDEV_ENABLED
	if (detect_sandbox()) {
		// Linux binaries in sandboxes / containers need special handling because
		// libudev doesn't work there. So we need to fallback to manual parsing
		// of /dev/input in such case.
		use_udev = false;
		print_verbose("JoypadLinux: udev enabled, but detected incompatible sandboxed mode. Falling back to /dev/input to detect joypads.");
	} else {
		use_udev = initialize_libudev() == 0;
		if (use_udev) {
			print_verbose("JoypadLinux: udev enabled and loaded successfully.");
		} else {
			print_verbose("JoypadLinux: udev enabled, but couldn't be loaded. Falling back to /dev/input to detect joypads.");
		}
	}
#else
	print_verbose("JoypadLinux: udev disabled, parsing /dev/input to detect joypads.");
#endif
	input = in;
	monitor_joypads_thread.start(monitor_joypads_thread_func, this);
	joypad_events_thread.start(joypad_events_thread_func, this);
}

JoypadLinux::~JoypadLinux() {
	monitor_joypads_exit.set();
	joypad_events_exit.set();
	monitor_joypads_thread.wait_to_finish();
	joypad_events_thread.wait_to_finish();
	close_joypads();
}

void JoypadLinux::monitor_joypads_thread_func(void *p_user) {
	if (p_user) {
		JoypadLinux *joy = (JoypadLinux *)p_user;
		joy->monitor_joypads_thread_run();
	}
}

void JoypadLinux::monitor_joypads_thread_run() {
#ifdef UDEV_ENABLED
	if (use_udev) {
		udev *_udev = udev_new();
		if (!_udev) {
			use_udev = false;
			ERR_PRINT("Failed getting an udev context, falling back to parsing /dev/input.");
			monitor_joypads();
		} else {
			enumerate_joypads(_udev);
			monitor_joypads(_udev);
			udev_unref(_udev);
		}
	} else {
		monitor_joypads();
	}
#else
	monitor_joypads();
#endif
}

#ifdef UDEV_ENABLED
void JoypadLinux::enumerate_joypads(udev *p_udev) {
	udev_enumerate *enumerate;
	udev_list_entry *devices, *dev_list_entry;
	udev_device *dev;

	enumerate = udev_enumerate_new(p_udev);
	udev_enumerate_add_match_subsystem(enumerate, "input");

	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(p_udev, path);
		const char *devnode = udev_device_get_devnode(dev);

		if (devnode) {
			String devnode_str = devnode;
			if (devnode_str.find(ignore_str) == -1) {
				open_joypad(devnode);
			}
		}
		udev_device_unref(dev);
	}
	udev_enumerate_unref(enumerate);
}

void JoypadLinux::monitor_joypads(udev *p_udev) {
	udev_device *dev = nullptr;
	udev_monitor *mon = udev_monitor_new_from_netlink(p_udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "input", nullptr);
	udev_monitor_enable_receiving(mon);
	int fd = udev_monitor_get_fd(mon);

	while (!monitor_joypads_exit.is_set()) {
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(fd + 1, &fds, nullptr, nullptr, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(fd, &fds)) {
			/* Make the call to receive the device.
			   select() ensured that this will not block. */
			dev = udev_monitor_receive_device(mon);

			if (dev && udev_device_get_devnode(dev) != nullptr) {
				String action = udev_device_get_action(dev);
				const char *devnode = udev_device_get_devnode(dev);
				if (devnode) {
					String devnode_str = devnode;
					if (devnode_str.find(ignore_str) == -1) {
						if (action == "add") {
							open_joypad(devnode);
						} else if (String(action) == "remove") {
							close_joypad(devnode);
						}
					}
				}
				udev_device_unref(dev);
			}
		}
		usleep(50000);
	}
	udev_monitor_unref(mon);
}
#endif

void JoypadLinux::monitor_joypads() {
	while (!monitor_joypads_exit.is_set()) {
		DIR *input_directory;
		input_directory = opendir("/dev/input");
		if (input_directory) {
			struct dirent *current;
			char fname[64];

			while ((current = readdir(input_directory)) != nullptr) {
				if (strncmp(current->d_name, "event", 5) != 0) {
					continue;
				}
				sprintf(fname, "/dev/input/%.*s", 16, current->d_name);
				if (attached_devices.find(fname) == -1) {
					open_joypad(fname);
				}
			}
		}
		closedir(input_directory);
		usleep(1000000); // 1s
	}
}

void JoypadLinux::close_joypads() {
	for (int i = 0; i < JOYPADS_MAX; i++) {
		MutexLock lock(joypads_mutex[i]);
		Joypad &joypad = joypads[i];
		close_joypad(joypad, i);
	}
}

void JoypadLinux::close_joypad(const char *p_devpath) {
	for (int i = 0; i < JOYPADS_MAX; i++) {
		MutexLock lock(joypads_mutex[i]);
		Joypad &joypad = joypads[i];
		if (joypads[i].devpath == p_devpath) {
			close_joypad(joypad, i);
		}
	}
}

void JoypadLinux::close_joypad(Joypad &p_joypad, int p_id) {
	if (p_joypad.fd != -1) {
		close(p_joypad.fd);
		p_joypad.fd = -1;
		attached_devices.erase(p_joypad.devpath);
		input->joy_connection_changed(p_id, false, "");
	};
	p_joypad.events.clear();
}

static String _hex_str(uint8_t p_byte) {
	static const char *dict = "0123456789abcdef";
	char ret[3];
	ret[2] = 0;

	ret[0] = dict[p_byte >> 4];
	ret[1] = dict[p_byte & 0xF];

	return ret;
}

void JoypadLinux::setup_joypad_properties(Joypad &p_joypad) {
	unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
	unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

	int num_buttons = 0;
	int num_axes = 0;

	if ((ioctl(p_joypad.fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
			(ioctl(p_joypad.fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)) {
		return;
	}
	for (int i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
		if (test_bit(i, keybit)) {
			p_joypad.key_map[i] = num_buttons++;
		}
	}
	for (int i = BTN_MISC; i < BTN_JOYSTICK; ++i) {
		if (test_bit(i, keybit)) {
			p_joypad.key_map[i] = num_buttons++;
		}
	}
	for (int i = 0; i < ABS_MISC; ++i) {
		/* Skip hats */
		if (i == ABS_HAT0X) {
			i = ABS_HAT3Y;
			continue;
		}
		if (test_bit(i, absbit)) {
			p_joypad.abs_map[i] = num_axes++;
			p_joypad.abs_info[i] = memnew(input_absinfo);
			if (ioctl(p_joypad.fd, EVIOCGABS(i), p_joypad.abs_info[i]) < 0) {
				memdelete(p_joypad.abs_info[i]);
				p_joypad.abs_info[i] = nullptr;
			}
		}
	}

	p_joypad.force_feedback = false;
	p_joypad.ff_effect_timestamp = 0;
	unsigned long ffbit[NBITS(FF_CNT)];
	if (ioctl(p_joypad.fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) != -1) {
		if (test_bit(FF_RUMBLE, ffbit)) {
			p_joypad.force_feedback = true;
		}
	}
}

void JoypadLinux::open_joypad(const char *p_path) {
	int joy_num = input->get_unused_joy_id();
	int fd = open(p_path, O_RDWR | O_NONBLOCK);
	if (fd != -1 && joy_num != -1) {
		unsigned long evbit[NBITS(EV_MAX)] = { 0 };
		unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
		unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

		// add to attached devices so we don't try to open it again
		attached_devices.push_back(String(p_path));

		if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
				(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
				(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)) {
			close(fd);
			return;
		}

		// Check if the device supports basic gamepad events and not non-joystick events
		bool has_key = test_bit(EV_KEY, evbit);
		bool has_abs_left = (test_bit(ABS_X, absbit) && test_bit(ABS_Y, absbit));
		bool has_abs_right = (test_bit(ABS_RX, absbit) && test_bit(ABS_RY, absbit));
		bool has_dpad = test_bit(ABS_HAT0X, absbit) || test_bit(BTN_DPAD_DOWN, keybit);
		bool has_direction_input = has_abs_left || has_abs_right || has_dpad;
		bool has_pointing_capability = test_bit(BTN_STYLUS, keybit) || test_bit(BTN_TOOL_PEN, keybit)
			|| test_bit(BTN_TOOL_FINGER, keybit) || test_bit(BTN_LEFT, keybit) || test_bit(BTN_TOUCH, keybit);
		bool is_gamepad = has_key && has_direction_input && !has_pointing_capability;
		if (!is_gamepad) {
			close(fd);
			return;
		}

		char uid[128];
		char namebuf[128];
		String name = "";
		input_id inpid;
		if (ioctl(fd, EVIOCGNAME(sizeof(namebuf)), namebuf) >= 0) {
			name = namebuf;
		}

		if (ioctl(fd, EVIOCGID, &inpid) < 0) {
			close(fd);
			return;
		}

		uint16_t vendor = BSWAP16(inpid.vendor);
		uint16_t product = BSWAP16(inpid.product);
		uint16_t version = BSWAP16(inpid.version);

		if (input->should_ignore_device(vendor, product)) {
			// This can be true in cases where Steam is passing information into the game to ignore
			// original gamepads when using virtual rebindings (See SteamInput).
			return;
		}

		MutexLock lock(joypads_mutex[joy_num]);
		Joypad &joypad = joypads[joy_num];
		joypad.reset();
		joypad.fd = fd;
		joypad.devpath = String(p_path);
		setup_joypad_properties(joypad);
		sprintf(uid, "%04x%04x", BSWAP16(inpid.bustype), 0);
		if (inpid.vendor && inpid.product && inpid.version) {
			sprintf(uid + String(uid).length(), "%04x%04x%04x%04x%04x%04x", vendor, 0, product, 0, version, 0);
			input->joy_connection_changed(joy_num, true, name, uid);
		} else {
			String uidname = uid;
			int uidlen = MIN(name.length(), 11);
			for (int i = 0; i < uidlen; i++) {
				uidname = uidname + _hex_str(name[i]);
			}
			uidname += "00";
			input->joy_connection_changed(joy_num, true, name, uidname);
		}
	}
}

void JoypadLinux::joypad_vibration_start(Joypad &p_joypad, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp) {
	if (!p_joypad.force_feedback || p_joypad.fd == -1 || p_weak_magnitude < 0.f || p_weak_magnitude > 1.f || p_strong_magnitude < 0.f || p_strong_magnitude > 1.f) {
		return;
	}
	if (p_joypad.ff_effect_id != -1) {
		joypad_vibration_stop(p_joypad, p_timestamp);
	}

	struct ff_effect effect;
	effect.type = FF_RUMBLE;
	effect.id = -1;
	effect.u.rumble.weak_magnitude = floor(p_weak_magnitude * (float)0xffff);
	effect.u.rumble.strong_magnitude = floor(p_strong_magnitude * (float)0xffff);
	effect.replay.length = floor(p_duration * 1000);
	effect.replay.delay = 0;

	if (ioctl(p_joypad.fd, EVIOCSFF, &effect) < 0) {
		return;
	}

	struct input_event play;
	play.type = EV_FF;
	play.code = effect.id;
	play.value = 1;
	if (write(p_joypad.fd, (const void *)&play, sizeof(play)) == -1) {
		print_verbose("Couldn't write to Joypad device.");
	}

	p_joypad.ff_effect_id = effect.id;
	p_joypad.ff_effect_timestamp = p_timestamp;
}

void JoypadLinux::joypad_vibration_stop(Joypad &p_joypad, uint64_t p_timestamp) {
	if (!p_joypad.force_feedback || p_joypad.fd == -1 || p_joypad.ff_effect_id == -1) {
		return;
	}

	if (ioctl(p_joypad.fd, EVIOCRMFF, p_joypad.ff_effect_id) < 0) {
		return;
	}

	p_joypad.ff_effect_id = -1;
	p_joypad.ff_effect_timestamp = p_timestamp;
}

float JoypadLinux::axis_correct(const input_absinfo *p_abs, int p_value) const {
	int min = p_abs->minimum;
	int max = p_abs->maximum;
	// Convert to a value between -1.0f and 1.0f.
	return 2.0f * (p_value - min) / (max - min) - 1.0f;
}

void JoypadLinux::joypad_events_thread_func(void *p_user) {
	if (p_user) {
		JoypadLinux *joy = (JoypadLinux *)p_user;
		joy->joypad_events_thread_run();
	}
}

void JoypadLinux::joypad_events_thread_run() {
	while (!joypad_events_exit.is_set()) {
		bool no_events = true;
		for (int i = 0; i < JOYPADS_MAX; i++) {
			MutexLock lock(joypads_mutex[i]);
			Joypad &joypad = joypads[i];
			if (joypad.fd == -1) {
				continue;
			}
			input_event event;
			while (read(joypad.fd, &event, sizeof(event)) > 0) {
				JoypadEvent joypad_event;
				joypad_event.type = event.type;
				joypad_event.code = event.code;
				joypad_event.value = event.value;
				joypad.events.push_back(joypad_event);
			}
			if (errno != EAGAIN) {
				close_joypad(joypad, i);
			}
		}
		if (no_events) {
			usleep(10000); // 10ms
		}
	}
}

void JoypadLinux::process_joypads() {
	for (int i = 0; i < JOYPADS_MAX; i++) {
		MutexLock lock(joypads_mutex[i]);
		Joypad &joypad = joypads[i];
		if (joypad.fd == -1) {
			continue;
		}
		for (uint32_t j = 0; j < joypad.events.size(); j++) {
			const JoypadEvent &joypad_event = joypad.events[j];
			// joypad_event may be tainted and out of MAX_KEY range, which will cause
			// joypad.key_map[joypad_event.code] to crash
			if (joypad_event.code >= MAX_KEY) {
				return;
			}

			switch (joypad_event.type) {
				case EV_KEY:
					input->joy_button(i, joypad.key_map[joypad_event.code], joypad_event.value);
					break;

				case EV_ABS:
					switch (joypad_event.code) {
						case ABS_HAT0X:
							if (joypad_event.value != 0) {
								if (joypad_event.value < 0) {
									joypad.dpad = (joypad.dpad | InputDefault::HAT_MASK_LEFT) & ~InputDefault::HAT_MASK_RIGHT;
								} else {
									joypad.dpad = (joypad.dpad | InputDefault::HAT_MASK_RIGHT) & ~InputDefault::HAT_MASK_LEFT;
								}
							} else {
								joypad.dpad &= ~(InputDefault::HAT_MASK_LEFT | InputDefault::HAT_MASK_RIGHT);
							}
							input->joy_hat(i, joypad.dpad);
							break;

						case ABS_HAT0Y:
							if (joypad_event.value != 0) {
								if (joypad_event.value < 0) {
									joypad.dpad = (joypad.dpad | InputDefault::HAT_MASK_UP) & ~InputDefault::HAT_MASK_DOWN;
								} else {
									joypad.dpad = (joypad.dpad | InputDefault::HAT_MASK_DOWN) & ~InputDefault::HAT_MASK_UP;
								}
							} else {
								joypad.dpad &= ~(InputDefault::HAT_MASK_UP | InputDefault::HAT_MASK_DOWN);
							}
							input->joy_hat(i, joypad.dpad);
							break;

						default:
							if (joypad_event.code >= MAX_ABS) {
								return;
							}
							if (joypad.abs_map[joypad_event.code] != -1 && joypad.abs_info[joypad_event.code]) {
								float value = axis_correct(joypad.abs_info[joypad_event.code], joypad_event.value);
								joypad.curr_axis[joypad.abs_map[joypad_event.code]] = value;
							}
							break;
					}
					break;
			}
		}
		joypad.events.clear();

		for (int j = 0; j < MAX_ABS; j++) {
			int index = joypad.abs_map[j];
			if (index != -1) {
				input->joy_axis(i, index, joypad.curr_axis[index]);
			}
		}

		if (joypad.force_feedback) {
			uint64_t timestamp = input->get_joy_vibration_timestamp(i);
			if (timestamp > joypad.ff_effect_timestamp) {
				Vector2 strength = input->get_joy_vibration_strength(i);
				float duration = input->get_joy_vibration_duration(i);
				if (strength.x == 0 && strength.y == 0) {
					joypad_vibration_stop(joypad, timestamp);
				} else {
					joypad_vibration_start(joypad, strength.x, strength.y, duration, timestamp);
				}
			}
		}
	}
}

#endif // JOYDEV_ENABLED
