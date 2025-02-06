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
#ifdef SOWRAP_ENABLED
#include "libudev-so_wrap.h"
#else
#include <libudev.h>
#endif
#endif

#define LONG_BITS (sizeof(long) * 8)
#define test_bit(nr, addr) (((1UL << ((nr) % LONG_BITS)) & ((addr)[(nr) / LONG_BITS])) != 0)
#define NBITS(x) ((((x) - 1) / LONG_BITS) + 1)

#ifdef UDEV_ENABLED
static const char *ignore_str = "/dev/input/js";
#endif

// On Linux with Steam Input Xbox 360 devices have an index appended to their device name, this index is
// the Steam Input gamepad index
#define VALVE_GAMEPAD_NAME_PREFIX "Microsoft X-Box 360 pad "
// IDs used by Steam Input virtual controllers.
// See https://partner.steamgames.com/doc/features/steam_controller/steam_input_gamepad_emulation_bestpractices
#define VALVE_GAMEPAD_VID 0x28DE
#define VALVE_GAMEPAD_PID 0x11FF

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
	for (int i = 0; i < MAX_KEY; i++) {
		key_map[i] = -1;
	}
	for (int i = 0; i < MAX_ABS; i++) {
		abs_map[i] = -1;
		curr_axis[i] = 0;
	}
	events.clear();
}

JoypadLinux::JoypadLinux(Input *in) {
#ifdef UDEV_ENABLED
	if (OS::get_singleton()->is_sandboxed()) {
		// Linux binaries in sandboxes / containers need special handling because
		// libudev doesn't work there. So we need to fallback to manual parsing
		// of /dev/input in such case.
		use_udev = false;
		print_verbose("JoypadLinux: udev enabled, but detected incompatible sandboxed mode. Falling back to /dev/input to detect joypads.");
	}
#ifdef SOWRAP_ENABLED
	else {
#ifdef DEBUG_ENABLED
		int dylibloader_verbose = 1;
#else
		int dylibloader_verbose = 0;
#endif
		use_udev = initialize_libudev(dylibloader_verbose) == 0;
		if (use_udev) {
			if (!udev_new || !udev_unref || !udev_enumerate_new || !udev_enumerate_add_match_subsystem || !udev_enumerate_scan_devices || !udev_enumerate_get_list_entry || !udev_list_entry_get_next || !udev_list_entry_get_name || !udev_device_new_from_syspath || !udev_device_get_devnode || !udev_device_get_action || !udev_device_unref || !udev_enumerate_unref || !udev_monitor_new_from_netlink || !udev_monitor_filter_add_match_subsystem_devtype || !udev_monitor_enable_receiving || !udev_monitor_get_fd || !udev_monitor_receive_device || !udev_monitor_unref) {
				// There's no API to check version, check if functions are available instead.
				use_udev = false;
				print_verbose("JoypadLinux: Unsupported udev library version!");
			} else {
				print_verbose("JoypadLinux: udev enabled and loaded successfully.");
			}
		} else {
			print_verbose("JoypadLinux: udev enabled, but couldn't be loaded. Falling back to /dev/input to detect joypads.");
		}
	}
#endif // SOWRAP_ENABLED
#else
	print_verbose("JoypadLinux: udev disabled, parsing /dev/input to detect joypads.");
#endif // UDEV_ENABLED

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
		JoypadLinux *joy = static_cast<JoypadLinux *>(p_user);
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
			if (!devnode_str.contains(ignore_str)) {
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
					if (!devnode_str.contains(ignore_str)) {
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
		OS::get_singleton()->delay_usec(50'000);
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
				if (!attached_devices.has(fname)) {
					open_joypad(fname);
				}
			}
		}
		closedir(input_directory);
		OS::get_singleton()->delay_usec(1'000'000);
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
	}
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
	for (int i = 0; i < BTN_JOYSTICK; ++i) {
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

void JoypadLinux::_auto_remap(Joypad &p_joypad, const StringName &p_guid, const String &p_name, bool p_hat0x_exist, bool p_hat0y_exist) {
	if (p_joypad.key_map[BTN_GAMEPAD] == -1) {
		return;
	}

	// Generate key mapping for JoyButton.

	int joy_button_mappings[int(JoyButton::SDL_MAX)];
	for (int i = 0; i < int(JoyButton::SDL_MAX); i++) {
		joy_button_mappings[i] = -1;
	}

#define BUTTON_MAP_KEY(button, keycode) (joy_button_mappings[int(button)] = p_joypad.key_map[keycode])
#define KEY_EXIST(keycode) (p_joypad.key_map[keycode] != -1)
#define UNUSED_KEY_HIDE(keycode) (p_joypad.key_map[keycode] = -p_joypad.key_map[keycode] - 2) // Used for two events occur at one key press.

	BUTTON_MAP_KEY(JoyButton::A, BTN_A);
	BUTTON_MAP_KEY(JoyButton::B, BTN_B);
	BUTTON_MAP_KEY(JoyButton::X, BTN_X);
	BUTTON_MAP_KEY(JoyButton::Y, BTN_Y);

	if (KEY_EXIST(KEY_BACK)) {
		// Exceptions for certain Xbox devices. The BTN_SELECT event is supported, but KEY_BACK is actually emitted.
		BUTTON_MAP_KEY(JoyButton::BACK, KEY_BACK);
	} else if (KEY_EXIST(BTN_SELECT)) {
		BUTTON_MAP_KEY(JoyButton::BACK, BTN_SELECT);
	}

	if (KEY_EXIST(KEY_HOMEPAGE)) {
		// Exceptions for certain Xbox devices. The BTN_MODE event is supported, but KEY_HOMEPAGE is actually emitted.
		BUTTON_MAP_KEY(JoyButton::GUIDE, KEY_HOMEPAGE);
	} else if (KEY_EXIST(BTN_MODE)) {
		BUTTON_MAP_KEY(JoyButton::GUIDE, BTN_MODE);
	}

	BUTTON_MAP_KEY(JoyButton::START, BTN_START);
	BUTTON_MAP_KEY(JoyButton::LEFT_STICK, BTN_THUMBL);
	BUTTON_MAP_KEY(JoyButton::RIGHT_STICK, BTN_THUMBR);
	BUTTON_MAP_KEY(JoyButton::LEFT_SHOULDER, BTN_TL);
	BUTTON_MAP_KEY(JoyButton::RIGHT_SHOULDER, BTN_TR);

	if (!p_hat0y_exist) {
		BUTTON_MAP_KEY(JoyButton::DPAD_UP, BTN_DPAD_UP);
		BUTTON_MAP_KEY(JoyButton::DPAD_DOWN, BTN_DPAD_DOWN);
	} else {
		// D-pads may report both digital button events (BTN_DPAD_*) and analog button events
		// (ABS_HAT0X and ABS_HAT0Y) on some devices. But Godot has hardcoded analog buttons
		// events in process_joypads(), so it is necessary to hide possible digital buttons
		// events to prevent triggering twice for one press.
		UNUSED_KEY_HIDE(BTN_DPAD_UP);
		UNUSED_KEY_HIDE(BTN_DPAD_DOWN);

		// Some Xbox devices may report digital button events as BTN_TRIGGER_HAPPY3 ~ BTN_TRIGGER_HAPPY4,
		// see https://github.com/godotengine/godot/issues/66878#issuecomment-2231491673 for more.
		UNUSED_KEY_HIDE(BTN_TRIGGER_HAPPY3);
		UNUSED_KEY_HIDE(BTN_TRIGGER_HAPPY4);

		p_joypad.key_is_hidden = true;
	}
	if (!p_hat0x_exist) {
		BUTTON_MAP_KEY(JoyButton::DPAD_LEFT, BTN_DPAD_LEFT);
		BUTTON_MAP_KEY(JoyButton::DPAD_RIGHT, BTN_DPAD_RIGHT);
	} else {
		// D-pads may report both digital button events (BTN_DPAD_*) and analog button events
		// (ABS_HAT0X and ABS_HAT0Y) on some devices. But Godot has hardcoded analog buttons
		// events in process_joypads(), so it is necessary to hide possible digital buttons
		// events to prevent triggering twice for one press.
		UNUSED_KEY_HIDE(BTN_DPAD_LEFT);
		UNUSED_KEY_HIDE(BTN_DPAD_RIGHT);

		// Some Xbox devices may report digital button events as BTN_TRIGGER_HAPPY1 ~ BTN_TRIGGER_HAPPY2,
		// see https://github.com/godotengine/godot/issues/66878#issuecomment-2231491673 for more.
		UNUSED_KEY_HIDE(BTN_TRIGGER_HAPPY1);
		UNUSED_KEY_HIDE(BTN_TRIGGER_HAPPY2);

		p_joypad.key_is_hidden = true;
	}

	if (KEY_EXIST(KEY_RECORD)) {
		// For certain Xbox devices.
		BUTTON_MAP_KEY(JoyButton::MISC1, KEY_RECORD);
	} else {
		// For certain Nintendo Switch devices.
		BUTTON_MAP_KEY(JoyButton::MISC1, BTN_Z);
	}

#undef BUTTON_MAP_KEY
#undef KEY_EXIST

	// Generate key mapping for JoyAxis.

	int joy_axis_mappings[int(JoyAxis::SDL_MAX)];
	for (int i = 0; i < int(JoyAxis::SDL_MAX); i++) {
		joy_axis_mappings[i] = -1;
	}

#define AXIS_MAP_ABS(axis, abscode) (joy_axis_mappings[int(axis)] = p_joypad.abs_map[abscode])
#define AXIS_MAP_KEY(axis, keycode) (joy_axis_mappings[int(axis)] = p_joypad.key_map[keycode])
#define ABS_EXIST(abscode) (p_joypad.abs_map[abscode] != -1)

	AXIS_MAP_ABS(JoyAxis::LEFT_X, ABS_X);
	AXIS_MAP_ABS(JoyAxis::LEFT_Y, ABS_Y);

	// Trigger buttons can be available as digital (BTN_TL2/BTN_TR2) or analog buttons or both.
	bool trigger_is_digital = true;

	if (ABS_EXIST(ABS_RX)) { // For certain Xbox or certain Nintendo Switch devices.
		AXIS_MAP_ABS(JoyAxis::RIGHT_X, ABS_RX);
		AXIS_MAP_ABS(JoyAxis::RIGHT_Y, ABS_RY);

		if (ABS_EXIST(ABS_BRAKE)) {
			// For possible devices. Typically used for TRIGGER_LEFT if the ABS_BRAKE event exists.
			AXIS_MAP_ABS(JoyAxis::TRIGGER_LEFT, ABS_BRAKE);
			AXIS_MAP_ABS(JoyAxis::TRIGGER_RIGHT, ABS_GAS);
			trigger_is_digital = false;
		} else if (ABS_EXIST(ABS_Z)) {
			// For certain Xbox devices.
			AXIS_MAP_ABS(JoyAxis::TRIGGER_LEFT, ABS_Z);
			AXIS_MAP_ABS(JoyAxis::TRIGGER_RIGHT, ABS_RZ);
			trigger_is_digital = false;
		}
	} else { // ABS_RX does not exist. Try another solution. Mainly for devices with wireless connections.
		AXIS_MAP_ABS(JoyAxis::RIGHT_X, ABS_Z);
		AXIS_MAP_ABS(JoyAxis::RIGHT_Y, ABS_RZ);

		if (ABS_EXIST(ABS_BRAKE)) {
			AXIS_MAP_ABS(JoyAxis::TRIGGER_LEFT, ABS_BRAKE);
			AXIS_MAP_ABS(JoyAxis::TRIGGER_RIGHT, ABS_GAS);
			trigger_is_digital = false;
		}
	}

	if (trigger_is_digital) {
		// Only digital button events are reported. For certain Nintendo Switch devices.
		AXIS_MAP_KEY(JoyAxis::TRIGGER_LEFT, BTN_TL2);
		AXIS_MAP_KEY(JoyAxis::TRIGGER_RIGHT, BTN_TR2);
	} else {
		// Trigger buttons can be available as both digital and analog buttons. Prioritizes mapping
		// of analog button events to axes. Hide extra events to prevent multiple triggering and
		// interference. Shooting games may prefer digital button events (BTN_TL2/BTN_TR2).
		UNUSED_KEY_HIDE(BTN_TL2);
		UNUSED_KEY_HIDE(BTN_TR2);
		p_joypad.key_is_hidden = true;
	}

#undef UNUSED_KEY_HIDE
#undef AXIS_MAP_ABS
#undef AXIS_MAP_KEY
#undef ABS_EXIST

	input->unknown_gamepad_auto_map(p_guid, p_name, joy_button_mappings, joy_axis_mappings, trigger_is_digital);
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

		// Check if the device supports basic gamepad events
		bool has_abs_left = (test_bit(ABS_X, absbit) && test_bit(ABS_Y, absbit));
		bool has_abs_right = (test_bit(ABS_RX, absbit) && test_bit(ABS_RY, absbit));
		if (!(test_bit(EV_KEY, evbit) && test_bit(EV_ABS, evbit) && (has_abs_left || has_abs_right))) {
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

		for (const String &word : name.to_lower().split(" ")) {
			if (banned_words.has(word)) {
				return;
			}
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
			Dictionary joypad_info;
			joypad_info["vendor_id"] = inpid.vendor;
			joypad_info["product_id"] = inpid.product;
			joypad_info["raw_name"] = name;

			sprintf(uid + String(uid).length(), "%04x%04x%04x%04x%04x%04x", vendor, 0, product, 0, version, 0);

			if (inpid.vendor == VALVE_GAMEPAD_VID && inpid.product == VALVE_GAMEPAD_PID) {
				if (name.begins_with(VALVE_GAMEPAD_NAME_PREFIX)) {
					String idx_str = name.substr(strlen(VALVE_GAMEPAD_NAME_PREFIX));
					if (idx_str.is_valid_int()) {
						joypad_info["steam_input_index"] = idx_str.to_int();
					}
				}
			}

			if (input->is_unknown_gamepad_auto_mapped() && !input->is_mapping_known(uid)) {
				bool hat0x_exist = test_bit(ABS_HAT0X, absbit);
				bool hat0y_exist = test_bit(ABS_HAT0Y, absbit);
				_auto_remap(joypad, uid, name, hat0x_exist, hat0y_exist);
			}

			input->joy_connection_changed(joy_num, true, name, uid, joypad_info);
		} else {
			String uidname = uid;
			int uidlen = MIN(name.length(), 11);
			for (int i = 0; i < uidlen; i++) {
				uidname = uidname + _hex_str(name[i]);
			}
			uidname += "00";

			if (input->is_unknown_gamepad_auto_mapped() && !input->is_mapping_known(uidname)) {
				bool hat0x_exist = test_bit(ABS_HAT0X, absbit);
				bool hat0y_exist = test_bit(ABS_HAT0Y, absbit);
				_auto_remap(joypad, uidname, name, hat0x_exist, hat0y_exist);
			}

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
				no_events = false;
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
			OS::get_singleton()->delay_usec(10'000);
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

		// Restore hidden keys to make it easier for users to create their own mappings.
		if (input->is_joy_button_need_reshow(i)) {
			input->set_joy_button_need_reshow(i, false);

			if (joypad.key_is_hidden) {
				for (int j = 0; j < MAX_KEY; j++) {
					if (joypad.key_map[j] < -1) {
						joypad.key_map[j] = -joypad.key_map[j] - 2;
					}
				}

				joypad.key_is_hidden = false;
			}
		}

		for (uint32_t j = 0; j < joypad.events.size(); j++) {
			const JoypadEvent &joypad_event = joypad.events[j];
			// joypad_event may be tainted and out of MAX_KEY range, which will cause
			// joypad.key_map[joypad_event.code] to crash
			if (joypad_event.code >= MAX_KEY) {
				return;
			}

			switch (joypad_event.type) {
				case EV_KEY: {
					int button_idx = joypad.key_map[joypad_event.code];
					if (input->is_unknown_gamepad_auto_mapped() && button_idx < -1) {
						break; // Some buttons may need to be hidden.
					}
					input->joy_button(i, (JoyButton)button_idx, joypad_event.value);
				} break;

				case EV_ABS:
					switch (joypad_event.code) {
						case ABS_HAT0X:
							if (joypad_event.value != 0) {
								if (joypad_event.value < 0) {
									joypad.dpad.set_flag(HatMask::LEFT);
									joypad.dpad.clear_flag(HatMask::RIGHT);
								} else {
									joypad.dpad.set_flag(HatMask::RIGHT);
									joypad.dpad.clear_flag(HatMask::LEFT);
								}
							} else {
								joypad.dpad.clear_flag(HatMask::LEFT);
								joypad.dpad.clear_flag(HatMask::RIGHT);
							}
							input->joy_hat(i, joypad.dpad);
							break;

						case ABS_HAT0Y:
							if (joypad_event.value != 0) {
								if (joypad_event.value < 0) {
									joypad.dpad.set_flag(HatMask::UP);
									joypad.dpad.clear_flag(HatMask::DOWN);
								} else {
									joypad.dpad.set_flag(HatMask::DOWN);
									joypad.dpad.clear_flag(HatMask::UP);
								}
							} else {
								joypad.dpad.clear_flag(HatMask::UP);
								joypad.dpad.clear_flag(HatMask::DOWN);
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
				input->joy_axis(i, (JoyAxis)index, joypad.curr_axis[index]);
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
