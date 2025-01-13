/**************************************************************************/
/*  wayland_thread.cpp                                                    */
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

#include "wayland_thread.h"

#ifdef WAYLAND_ENABLED

#ifdef __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#else
// Assume Linux.
#include <linux/input-event-codes.h>
#endif

// For the actual polling thread.
#include <poll.h>

// For shared memory buffer creation.
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Fix the wl_array_for_each macro to work with C++. This is based on the
// original from `wayland-util.h` in the Wayland client library.
#undef wl_array_for_each
#define wl_array_for_each(pos, array) \
	for (pos = (decltype(pos))(array)->data; (const char *)pos < ((const char *)(array)->data + (array)->size); (pos)++)

#define WAYLAND_THREAD_DEBUG_LOGS_ENABLED
#ifdef WAYLAND_THREAD_DEBUG_LOGS_ENABLED
#define DEBUG_LOG_WAYLAND_THREAD(...) print_verbose(__VA_ARGS__)
#else
#define DEBUG_LOG_WAYLAND_THREAD(...)
#endif

// Read the content pointed by fd into a Vector<uint8_t>.
Vector<uint8_t> WaylandThread::_read_fd(int fd) {
	// This is pretty much an arbitrary size.
	uint32_t chunk_size = 2048;

	LocalVector<uint8_t> data;
	data.resize(chunk_size);

	uint32_t bytes_read = 0;

	while (true) {
		ssize_t last_bytes_read = read(fd, data.ptr() + bytes_read, chunk_size);
		if (last_bytes_read < 0) {
			ERR_PRINT(vformat("Read error %d.", errno));

			data.clear();
			break;
		}

		if (last_bytes_read == 0) {
			// We're done, we've reached the EOF.
			DEBUG_LOG_WAYLAND_THREAD(vformat("Done reading %d bytes.", bytes_read));

			close(fd);

			data.resize(bytes_read);
			break;
		}

		DEBUG_LOG_WAYLAND_THREAD(vformat("Read chunk of %d bytes.", last_bytes_read));

		bytes_read += last_bytes_read;

		// Increase the buffer size by one chunk in preparation of the next read.
		data.resize(bytes_read + chunk_size);
	}

	return data;
}

// Based on the wayland book's shared memory boilerplate (PD/CC0).
// See: https://wayland-book.com/surfaces/shared-memory.html
int WaylandThread::_allocate_shm_file(size_t size) {
	int retries = 100;

	do {
		// Generate a random name.
		char name[] = "/wl_shm-godot-XXXXXX";
		for (long unsigned int i = sizeof(name) - 7; i < sizeof(name) - 1; i++) {
			name[i] = Math::random('A', 'Z');
		}

		// Try to open a shared memory object with that name.
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			// Success, unlink its name as we just need the file descriptor.
			shm_unlink(name);

			// Resize the file to the requested length.
			int ret;
			do {
				ret = ftruncate(fd, size);
			} while (ret < 0 && errno == EINTR);

			if (ret < 0) {
				close(fd);
				return -1;
			}

			return fd;
		}

		retries--;
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

// Return the content of a wl_data_offer.
Vector<uint8_t> WaylandThread::_wl_data_offer_read(struct wl_display *p_display, const char *p_mime, struct wl_data_offer *p_offer) {
	if (!p_offer) {
		return Vector<uint8_t>();
	}

	int fds[2];
	if (pipe(fds) == 0) {
		wl_data_offer_receive(p_offer, p_mime, fds[1]);

		// Let the compositor know about the pipe.
		// NOTE: It's important to just flush and not roundtrip here as we would risk
		// running some cleanup event, like for example `wl_data_device::leave`. We're
		// going to wait for the message anyways as the read will probably block if
		// the compositor doesn't read from the other end of the pipe.
		wl_display_flush(p_display);

		// Close the write end of the pipe, which we don't need and would otherwise
		// just stall our next `read`s.
		close(fds[1]);

		return _read_fd(fds[0]);
	}

	return Vector<uint8_t>();
}

// Read the content of a wp_primary_selection_offer.
Vector<uint8_t> WaylandThread::_wp_primary_selection_offer_read(struct wl_display *p_display, const char *p_mime, struct zwp_primary_selection_offer_v1 *p_offer) {
	if (!p_offer) {
		return Vector<uint8_t>();
	}

	int fds[2];
	if (pipe(fds) == 0) {
		zwp_primary_selection_offer_v1_receive(p_offer, p_mime, fds[1]);

		// NOTE: It's important to just flush and not roundtrip here as we would risk
		// running some cleanup event, like for example `wl_data_device::leave`. We're
		// going to wait for the message anyways as the read will probably block if
		// the compositor doesn't read from the other end of the pipe.
		wl_display_flush(p_display);

		// Close the write end of the pipe, which we don't need and would otherwise
		// just stall our next `read`s.
		close(fds[1]);

		return _read_fd(fds[0]);
	}

	return Vector<uint8_t>();
}

// Sets up an `InputEventKey` and returns whether it has any meaningful value.
bool WaylandThread::_seat_state_configure_key_event(SeatState &p_ss, Ref<InputEventKey> p_event, xkb_keycode_t p_keycode, bool p_pressed) {
	// TODO: Handle keys that release multiple symbols?
	Key keycode = KeyMappingXKB::get_keycode(xkb_state_key_get_one_sym(p_ss.xkb_state, p_keycode));
	Key physical_keycode = KeyMappingXKB::get_scancode(p_keycode);
	KeyLocation key_location = KeyMappingXKB::get_location(p_keycode);
	uint32_t unicode = xkb_state_key_get_utf32(p_ss.xkb_state, p_keycode);

	if (keycode == Key::NONE) {
		keycode = physical_keycode;
	}

	if (keycode >= Key::A + 32 && keycode <= Key::Z + 32) {
		keycode -= 'a' - 'A';
	}

	if (physical_keycode == Key::NONE && keycode == Key::NONE && unicode == 0) {
		return false;
	}

	p_event->set_window_id(DisplayServer::MAIN_WINDOW_ID);

	// Set all pressed modifiers.
	p_event->set_shift_pressed(p_ss.shift_pressed);
	p_event->set_ctrl_pressed(p_ss.ctrl_pressed);
	p_event->set_alt_pressed(p_ss.alt_pressed);
	p_event->set_meta_pressed(p_ss.meta_pressed);

	p_event->set_pressed(p_pressed);
	p_event->set_keycode(keycode);
	p_event->set_physical_keycode(physical_keycode);
	p_event->set_location(key_location);

	if (unicode != 0) {
		p_event->set_key_label(fix_key_label(unicode, keycode));
	} else {
		p_event->set_key_label(keycode);
	}

	if (p_pressed) {
		p_event->set_unicode(fix_unicode(unicode));
	}

	// Taken from DisplayServerX11.
	if (p_event->get_keycode() == Key::BACKTAB) {
		// Make it consistent across platforms.
		p_event->set_keycode(Key::TAB);
		p_event->set_physical_keycode(Key::TAB);
		p_event->set_shift_pressed(true);
	}

	return true;
}

void WaylandThread::_set_current_seat(struct wl_seat *p_seat) {
	if (p_seat == wl_seat_current) {
		return;
	}

	SeatState *old_state = wl_seat_get_seat_state(wl_seat_current);

	if (old_state) {
		seat_state_unlock_pointer(old_state);
	}

	SeatState *new_state = wl_seat_get_seat_state(p_seat);
	seat_state_unlock_pointer(new_state);

	wl_seat_current = p_seat;
	pointer_set_constraint(pointer_constraint);
}

// Returns whether it loaded the theme or not.
bool WaylandThread::_load_cursor_theme(int p_cursor_size) {
	if (wl_cursor_theme) {
		wl_cursor_theme_destroy(wl_cursor_theme);
		wl_cursor_theme = nullptr;
	}

	if (cursor_theme_name.is_empty()) {
		cursor_theme_name = "default";
	}

	print_verbose(vformat("Loading cursor theme \"%s\" size %d.", cursor_theme_name, p_cursor_size));

	wl_cursor_theme = wl_cursor_theme_load(cursor_theme_name.utf8().get_data(), p_cursor_size, registry.wl_shm);

	ERR_FAIL_NULL_V_MSG(wl_cursor_theme, false, "Can't load any cursor theme.");

	static const char *cursor_names[] = {
		"left_ptr",
		"xterm",
		"hand2",
		"cross",
		"watch",
		"left_ptr_watch",
		"fleur",
		"dnd-move",
		"crossed_circle",
		"v_double_arrow",
		"h_double_arrow",
		"size_bdiag",
		"size_fdiag",
		"move",
		"row_resize",
		"col_resize",
		"question_arrow"
	};

	static const char *cursor_names_fallback[] = {
		nullptr,
		nullptr,
		"pointer",
		"cross",
		"wait",
		"progress",
		"grabbing",
		"hand1",
		"forbidden",
		"ns-resize",
		"ew-resize",
		"fd_double_arrow",
		"bd_double_arrow",
		"fleur",
		"sb_v_double_arrow",
		"sb_h_double_arrow",
		"help"
	};

	for (int i = 0; i < DisplayServer::CURSOR_MAX; i++) {
		struct wl_cursor *cursor = wl_cursor_theme_get_cursor(wl_cursor_theme, cursor_names[i]);

		if (!cursor && cursor_names_fallback[i]) {
			cursor = wl_cursor_theme_get_cursor(wl_cursor_theme, cursor_names_fallback[i]);
		}

		if (cursor && cursor->image_count > 0) {
			wl_cursors[i] = cursor;
		} else {
			wl_cursors[i] = nullptr;
			print_verbose("Failed loading cursor: " + String(cursor_names[i]));
		}
	}

	return true;
}

void WaylandThread::_update_scale(int p_scale) {
	if (p_scale <= cursor_scale) {
		return;
	}

	print_verbose(vformat("Bumping cursor scale to %d", p_scale));

	// There's some display that's bigger than the cache, let's update it.
	cursor_scale = p_scale;

	if (wl_cursor_theme == nullptr) {
		// Ugh. Either we're still initializing (this must've been called from the
		// first roundtrips) or we had some error while doing so. We'll trust that it
		// will be updated for us if needed.
		return;
	}

	int cursor_size = unscaled_cursor_size * p_scale;

	if (_load_cursor_theme(cursor_size)) {
		for (struct wl_seat *wl_seat : registry.wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			seat_state_update_cursor(ss);
		}
	}
}

void WaylandThread::_wl_registry_on_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
	RegistryState *registry = (RegistryState *)data;
	ERR_FAIL_NULL(registry);

	if (strcmp(interface, wl_shm_interface.name) == 0) {
		registry->wl_shm = (struct wl_shm *)wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
		registry->wl_shm_name = name;
		return;
	}

	// NOTE: Deprecated.
	if (strcmp(interface, zxdg_exporter_v1_interface.name) == 0) {
		registry->xdg_exporter_v1 = (struct zxdg_exporter_v1 *)wl_registry_bind(wl_registry, name, &zxdg_exporter_v1_interface, 1);
		registry->xdg_exporter_v1_name = name;
		return;
	}

	if (strcmp(interface, zxdg_exporter_v2_interface.name) == 0) {
		registry->xdg_exporter_v2 = (struct zxdg_exporter_v2 *)wl_registry_bind(wl_registry, name, &zxdg_exporter_v2_interface, 1);
		registry->xdg_exporter_v2_name = name;
		return;
	}

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		registry->wl_compositor = (struct wl_compositor *)wl_registry_bind(wl_registry, name, &wl_compositor_interface, CLAMP((int)version, 1, 6));
		registry->wl_compositor_name = name;
		return;
	}

	if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
		registry->wl_data_device_manager = (struct wl_data_device_manager *)wl_registry_bind(wl_registry, name, &wl_data_device_manager_interface, CLAMP((int)version, 1, 3));
		registry->wl_data_device_manager_name = name;

		// This global creates some seat data. Let's do that for the ones already available.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wl_data_device == nullptr) {
				ss->wl_data_device = wl_data_device_manager_get_data_device(registry->wl_data_device_manager, wl_seat);
				wl_data_device_add_listener(ss->wl_data_device, &wl_data_device_listener, ss);
			}
		}
		return;
	}

	if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *wl_output = (struct wl_output *)wl_registry_bind(wl_registry, name, &wl_output_interface, CLAMP((int)version, 1, 4));
		wl_proxy_tag_godot((struct wl_proxy *)wl_output);

		registry->wl_outputs.push_back(wl_output);

		ScreenState *ss = memnew(ScreenState);
		ss->wl_output_name = name;
		ss->wayland_thread = registry->wayland_thread;

		wl_proxy_tag_godot((struct wl_proxy *)wl_output);
		wl_output_add_listener(wl_output, &wl_output_listener, ss);
		return;
	}

	if (strcmp(interface, wl_seat_interface.name) == 0) {
		struct wl_seat *wl_seat = (struct wl_seat *)wl_registry_bind(wl_registry, name, &wl_seat_interface, CLAMP((int)version, 1, 9));
		wl_proxy_tag_godot((struct wl_proxy *)wl_seat);

		SeatState *ss = memnew(SeatState);
		ss->wl_seat = wl_seat;
		ss->wl_seat_name = name;

		ss->registry = registry;
		ss->wayland_thread = registry->wayland_thread;

		// Some extra stuff depends on other globals. We'll initialize them if the
		// globals are already there, otherwise we'll have to do that once and if they
		// get announced.
		//
		// NOTE: Don't forget to also bind/destroy with the respective global.
		if (!ss->wl_data_device && registry->wl_data_device_manager) {
			// Clipboard & DnD.
			ss->wl_data_device = wl_data_device_manager_get_data_device(registry->wl_data_device_manager, wl_seat);
			wl_data_device_add_listener(ss->wl_data_device, &wl_data_device_listener, ss);
		}

		if (!ss->wp_primary_selection_device && registry->wp_primary_selection_device_manager) {
			// Primary selection.
			ss->wp_primary_selection_device = zwp_primary_selection_device_manager_v1_get_device(registry->wp_primary_selection_device_manager, wl_seat);
			zwp_primary_selection_device_v1_add_listener(ss->wp_primary_selection_device, &wp_primary_selection_device_listener, ss);
		}

		if (!ss->wp_tablet_seat && registry->wp_tablet_manager) {
			// Tablet.
			ss->wp_tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(registry->wp_tablet_manager, wl_seat);
			zwp_tablet_seat_v2_add_listener(ss->wp_tablet_seat, &wp_tablet_seat_listener, ss);
		}

		if (!ss->wp_text_input && registry->wp_text_input_manager) {
			// IME.
			ss->wp_text_input = zwp_text_input_manager_v3_get_text_input(registry->wp_text_input_manager, wl_seat);
			zwp_text_input_v3_add_listener(ss->wp_text_input, &wp_text_input_listener, ss);
		}

		registry->wl_seats.push_back(wl_seat);

		wl_seat_add_listener(wl_seat, &wl_seat_listener, ss);

		if (registry->wayland_thread->wl_seat_current == nullptr) {
			registry->wayland_thread->_set_current_seat(wl_seat);
		}

		return;
	}

	if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		registry->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, CLAMP((int)version, 1, 6));
		registry->xdg_wm_base_name = name;

		xdg_wm_base_add_listener(registry->xdg_wm_base, &xdg_wm_base_listener, nullptr);
		return;
	}

	if (strcmp(interface, wp_viewporter_interface.name) == 0) {
		registry->wp_viewporter = (struct wp_viewporter *)wl_registry_bind(wl_registry, name, &wp_viewporter_interface, 1);
		registry->wp_viewporter_name = name;
	}

	if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
		registry->wp_fractional_scale_manager = (struct wp_fractional_scale_manager_v1 *)wl_registry_bind(wl_registry, name, &wp_fractional_scale_manager_v1_interface, 1);
		registry->wp_fractional_scale_manager_name = name;

		// NOTE: We're not mapping the fractional scale object here because this is
		// supposed to be a "startup global". If for some reason this isn't true (who
		// knows), add a conditional branch for creating the add-on object.
	}

	if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		registry->xdg_decoration_manager = (struct zxdg_decoration_manager_v1 *)wl_registry_bind(wl_registry, name, &zxdg_decoration_manager_v1_interface, 1);
		registry->xdg_decoration_manager_name = name;
		return;
	}

	if (strcmp(interface, xdg_system_bell_v1_interface.name) == 0) {
		registry->xdg_system_bell = (struct xdg_system_bell_v1 *)wl_registry_bind(wl_registry, name, &xdg_system_bell_v1_interface, 1);
		registry->xdg_system_bell_name = name;
		return;
	}

	if (strcmp(interface, xdg_activation_v1_interface.name) == 0) {
		registry->xdg_activation = (struct xdg_activation_v1 *)wl_registry_bind(wl_registry, name, &xdg_activation_v1_interface, 1);
		registry->xdg_activation_name = name;
		return;
	}

	if (strcmp(interface, zwp_primary_selection_device_manager_v1_interface.name) == 0) {
		registry->wp_primary_selection_device_manager = (struct zwp_primary_selection_device_manager_v1 *)wl_registry_bind(wl_registry, name, &zwp_primary_selection_device_manager_v1_interface, 1);

		// This global creates some seat data. Let's do that for the ones already available.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (!ss->wp_primary_selection_device && registry->wp_primary_selection_device_manager) {
				ss->wp_primary_selection_device = zwp_primary_selection_device_manager_v1_get_device(registry->wp_primary_selection_device_manager, wl_seat);
				zwp_primary_selection_device_v1_add_listener(ss->wp_primary_selection_device, &wp_primary_selection_device_listener, ss);
			}
		}
	}

	if (strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0) {
		registry->wp_relative_pointer_manager = (struct zwp_relative_pointer_manager_v1 *)wl_registry_bind(wl_registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
		registry->wp_relative_pointer_manager_name = name;
		return;
	}

	if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
		registry->wp_pointer_constraints = (struct zwp_pointer_constraints_v1 *)wl_registry_bind(wl_registry, name, &zwp_pointer_constraints_v1_interface, 1);
		registry->wp_pointer_constraints_name = name;
		return;
	}

	if (strcmp(interface, zwp_pointer_gestures_v1_interface.name) == 0) {
		registry->wp_pointer_gestures = (struct zwp_pointer_gestures_v1 *)wl_registry_bind(wl_registry, name, &zwp_pointer_gestures_v1_interface, 1);
		registry->wp_pointer_gestures_name = name;
		return;
	}

	if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
		registry->wp_idle_inhibit_manager = (struct zwp_idle_inhibit_manager_v1 *)wl_registry_bind(wl_registry, name, &zwp_idle_inhibit_manager_v1_interface, 1);
		registry->wp_idle_inhibit_manager_name = name;
		return;
	}

	if (strcmp(interface, zwp_tablet_manager_v2_interface.name) == 0) {
		registry->wp_tablet_manager = (struct zwp_tablet_manager_v2 *)wl_registry_bind(wl_registry, name, &zwp_tablet_manager_v2_interface, 1);
		registry->wp_tablet_manager_name = name;

		// This global creates some seat data. Let's do that for the ones already available.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			ss->wp_tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(registry->wp_tablet_manager, wl_seat);
			zwp_tablet_seat_v2_add_listener(ss->wp_tablet_seat, &wp_tablet_seat_listener, ss);
		}

		return;
	}

	if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
		registry->wp_text_input_manager = (struct zwp_text_input_manager_v3 *)wl_registry_bind(wl_registry, name, &zwp_text_input_manager_v3_interface, 1);
		registry->wp_text_input_manager_name = name;

		// This global creates some seat data. Let's do that for the ones already available.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			ss->wp_text_input = zwp_text_input_manager_v3_get_text_input(registry->wp_text_input_manager, wl_seat);
			zwp_text_input_v3_add_listener(ss->wp_text_input, &wp_text_input_listener, ss);
		}

		return;
	}
}

void WaylandThread::_wl_registry_on_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
	RegistryState *registry = (RegistryState *)data;
	ERR_FAIL_NULL(registry);

	if (name == registry->wl_shm_name) {
		if (registry->wl_shm) {
			wl_shm_destroy(registry->wl_shm);
			registry->wl_shm = nullptr;
		}

		registry->wl_shm_name = 0;

		return;
	}

	// NOTE: Deprecated.
	if (name == registry->xdg_exporter_v1_name) {
		if (registry->xdg_exporter_v1) {
			zxdg_exporter_v1_destroy(registry->xdg_exporter_v1);
			registry->xdg_exporter_v1 = nullptr;
		}

		registry->xdg_exporter_v1_name = 0;

		return;
	}

	if (name == registry->xdg_exporter_v2_name) {
		if (registry->xdg_exporter_v2) {
			zxdg_exporter_v2_destroy(registry->xdg_exporter_v2);
			registry->xdg_exporter_v2 = nullptr;
		}

		registry->xdg_exporter_v2_name = 0;

		return;
	}

	if (name == registry->wl_compositor_name) {
		if (registry->wl_compositor) {
			wl_compositor_destroy(registry->wl_compositor);
			registry->wl_compositor = nullptr;
		}

		registry->wl_compositor_name = 0;

		return;
	}

	if (name == registry->wl_data_device_manager_name) {
		if (registry->wl_data_device_manager) {
			wl_data_device_manager_destroy(registry->wl_data_device_manager);
			registry->wl_data_device_manager = nullptr;
		}

		registry->wl_data_device_manager_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wl_data_device) {
				wl_data_device_destroy(ss->wl_data_device);
				ss->wl_data_device = nullptr;
			}

			ss->wl_data_device = nullptr;
		}

		return;
	}

	if (name == registry->xdg_wm_base_name) {
		if (registry->xdg_wm_base) {
			xdg_wm_base_destroy(registry->xdg_wm_base);
			registry->xdg_wm_base = nullptr;
		}

		registry->xdg_wm_base_name = 0;

		return;
	}

	if (name == registry->wp_viewporter_name) {
		WindowState *ws = &registry->wayland_thread->main_window;

		if (registry->wp_viewporter) {
			wp_viewporter_destroy(registry->wp_viewporter);
			registry->wp_viewporter = nullptr;
		}

		if (ws->wp_viewport) {
			wp_viewport_destroy(ws->wp_viewport);
			ws->wp_viewport = nullptr;
		}

		registry->wp_viewporter_name = 0;

		return;
	}

	if (name == registry->wp_fractional_scale_manager_name) {
		WindowState *ws = &registry->wayland_thread->main_window;

		if (registry->wp_fractional_scale_manager) {
			wp_fractional_scale_manager_v1_destroy(registry->wp_fractional_scale_manager);
			registry->wp_fractional_scale_manager = nullptr;
		}

		if (ws->wp_fractional_scale) {
			wp_fractional_scale_v1_destroy(ws->wp_fractional_scale);
			ws->wp_fractional_scale = nullptr;
		}

		registry->wp_fractional_scale_manager_name = 0;
	}

	if (name == registry->xdg_decoration_manager_name) {
		if (registry->xdg_decoration_manager) {
			zxdg_decoration_manager_v1_destroy(registry->xdg_decoration_manager);
			registry->xdg_decoration_manager = nullptr;
		}

		registry->xdg_decoration_manager_name = 0;

		return;
	}

	if (name == registry->xdg_system_bell_name) {
		if (registry->xdg_system_bell) {
			xdg_system_bell_v1_destroy(registry->xdg_system_bell);
			registry->xdg_system_bell = nullptr;
		}

		registry->xdg_system_bell_name = 0;

		return;
	}

	if (name == registry->xdg_activation_name) {
		if (registry->xdg_activation) {
			xdg_activation_v1_destroy(registry->xdg_activation);
			registry->xdg_activation = nullptr;
		}

		registry->xdg_activation_name = 0;

		return;
	}

	if (name == registry->wp_primary_selection_device_manager_name) {
		if (registry->wp_primary_selection_device_manager) {
			zwp_primary_selection_device_manager_v1_destroy(registry->wp_primary_selection_device_manager);
			registry->wp_primary_selection_device_manager = nullptr;
		}

		registry->wp_primary_selection_device_manager_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wp_primary_selection_device) {
				zwp_primary_selection_device_v1_destroy(ss->wp_primary_selection_device);
				ss->wp_primary_selection_device = nullptr;
			}

			if (ss->wp_primary_selection_source) {
				zwp_primary_selection_source_v1_destroy(ss->wp_primary_selection_source);
				ss->wp_primary_selection_source = nullptr;
			}

			if (ss->wp_primary_selection_offer) {
				memfree(wp_primary_selection_offer_get_offer_state(ss->wp_primary_selection_offer));
				zwp_primary_selection_offer_v1_destroy(ss->wp_primary_selection_offer);
				ss->wp_primary_selection_offer = nullptr;
			}
		}

		return;
	}

	if (name == registry->wp_relative_pointer_manager_name) {
		if (registry->wp_relative_pointer_manager) {
			zwp_relative_pointer_manager_v1_destroy(registry->wp_relative_pointer_manager);
			registry->wp_relative_pointer_manager = nullptr;
		}

		registry->wp_relative_pointer_manager_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wp_relative_pointer) {
				zwp_relative_pointer_v1_destroy(ss->wp_relative_pointer);
				ss->wp_relative_pointer = nullptr;
			}
		}

		return;
	}

	if (name == registry->wp_pointer_constraints_name) {
		if (registry->wp_pointer_constraints) {
			zwp_pointer_constraints_v1_destroy(registry->wp_pointer_constraints);
			registry->wp_pointer_constraints = nullptr;
		}

		registry->wp_pointer_constraints_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wp_relative_pointer) {
				zwp_relative_pointer_v1_destroy(ss->wp_relative_pointer);
				ss->wp_relative_pointer = nullptr;
			}

			if (ss->wp_locked_pointer) {
				zwp_locked_pointer_v1_destroy(ss->wp_locked_pointer);
				ss->wp_locked_pointer = nullptr;
			}

			if (ss->wp_confined_pointer) {
				zwp_confined_pointer_v1_destroy(ss->wp_confined_pointer);
				ss->wp_confined_pointer = nullptr;
			}
		}

		return;
	}

	if (name == registry->wp_pointer_gestures_name) {
		if (registry->wp_pointer_gestures) {
			zwp_pointer_gestures_v1_destroy(registry->wp_pointer_gestures);
		}

		registry->wp_pointer_gestures = nullptr;
		registry->wp_pointer_gestures_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wp_pointer_gesture_pinch) {
				zwp_pointer_gesture_pinch_v1_destroy(ss->wp_pointer_gesture_pinch);
				ss->wp_pointer_gesture_pinch = nullptr;
			}
		}

		return;
	}

	if (name == registry->wp_idle_inhibit_manager_name) {
		if (registry->wp_idle_inhibit_manager) {
			zwp_idle_inhibit_manager_v1_destroy(registry->wp_idle_inhibit_manager);
			registry->wp_idle_inhibit_manager = nullptr;
		}

		registry->wp_idle_inhibit_manager_name = 0;

		return;
	}

	if (name == registry->wp_tablet_manager_name) {
		if (registry->wp_tablet_manager) {
			zwp_tablet_manager_v2_destroy(registry->wp_tablet_manager);
			registry->wp_tablet_manager = nullptr;
		}

		registry->wp_tablet_manager_name = 0;

		// This global is used to create some seat data. Let's clean it.
		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			for (struct zwp_tablet_tool_v2 *tool : ss->tablet_tools) {
				TabletToolState *state = wp_tablet_tool_get_state(tool);
				if (state) {
					memdelete(state);
				}

				zwp_tablet_tool_v2_destroy(tool);
			}

			ss->tablet_tools.clear();
		}

		return;
	}

	if (name == registry->wp_text_input_manager_name) {
		if (registry->wp_text_input_manager) {
			zwp_text_input_manager_v3_destroy(registry->wp_text_input_manager);
			registry->wp_text_input_manager = nullptr;
		}

		registry->wp_text_input_manager_name = 0;

		for (struct wl_seat *wl_seat : registry->wl_seats) {
			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			zwp_text_input_v3_destroy(ss->wp_text_input);
			ss->wp_text_input = nullptr;
		}

		return;
	}

	{
		// Iterate through all of the seats to find if any got removed.
		List<struct wl_seat *>::Element *E = registry->wl_seats.front();
		while (E) {
			struct wl_seat *wl_seat = E->get();
			List<struct wl_seat *>::Element *N = E->next();

			SeatState *ss = wl_seat_get_seat_state(wl_seat);
			ERR_FAIL_NULL(ss);

			if (ss->wl_seat_name == name) {
				if (wl_seat) {
					wl_seat_destroy(wl_seat);
				}

				if (ss->wl_data_device) {
					wl_data_device_destroy(ss->wl_data_device);
				}

				if (ss->wp_tablet_seat) {
					zwp_tablet_seat_v2_destroy(ss->wp_tablet_seat);

					for (struct zwp_tablet_tool_v2 *tool : ss->tablet_tools) {
						TabletToolState *state = wp_tablet_tool_get_state(tool);
						if (state) {
							memdelete(state);
						}

						zwp_tablet_tool_v2_destroy(tool);
					}
				}

				memdelete(ss);

				registry->wl_seats.erase(E);
				return;
			}

			E = N;
		}
	}

	{
		// Iterate through all of the outputs to find if any got removed.
		// FIXME: This is a very bruteforce approach.
		List<struct wl_output *>::Element *it = registry->wl_outputs.front();
		while (it) {
			// Iterate through all of the screens to find if any got removed.
			struct wl_output *wl_output = it->get();
			ERR_FAIL_NULL(wl_output);

			ScreenState *ss = wl_output_get_screen_state(wl_output);

			if (ss->wl_output_name == name) {
				registry->wl_outputs.erase(it);

				memdelete(ss);
				wl_output_destroy(wl_output);

				return;
			}

			it = it->next();
		}
	}
}

void WaylandThread::_wl_surface_on_enter(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output) {
	if (!wl_output || !wl_proxy_is_godot((struct wl_proxy *)wl_output)) {
		// This won't have the right data bound to it. Not worth it and would probably
		// just break everything.
		return;
	}

	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	DEBUG_LOG_WAYLAND_THREAD(vformat("Window entered output %x.\n", (size_t)wl_output));

	ws->wl_outputs.insert(wl_output);

	// Workaround for buffer scaling as there's no guaranteed way of knowing the
	// preferred scale.
	// TODO: Skip this branch for newer `wl_surface`s once we add support for
	// `wl_surface::preferred_buffer_scale`
	if (ws->preferred_fractional_scale == 0) {
		window_state_update_size(ws, ws->rect.size.width, ws->rect.size.height);
	}
}

void WaylandThread::_frame_wl_callback_on_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data) {
	wl_callback_destroy(wl_callback);

	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);
	ERR_FAIL_NULL(ws->wayland_thread);
	ERR_FAIL_NULL(ws->wl_surface);

	ws->wayland_thread->set_frame();

	ws->frame_callback = wl_surface_frame(ws->wl_surface),
	wl_callback_add_listener(ws->frame_callback, &frame_wl_callback_listener, ws);

	if (ws->wl_surface && ws->buffer_scale_changed) {
		// NOTE: We're only now setting the buffer scale as the idea is to get this
		// data committed together with the new frame, all by the rendering driver.
		// This is important because we might otherwise set an invalid combination of
		// buffer size and scale (e.g. odd size and 2x scale). We're pretty much
		// guaranteed to get a proper buffer in the next render loop as the rescaling
		// method also informs the engine of a "window rect change", triggering
		// rendering if needed.
		wl_surface_set_buffer_scale(ws->wl_surface, window_state_get_preferred_buffer_scale(ws));
	}
}

void WaylandThread::_wl_surface_on_leave(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output) {
	if (!wl_output || !wl_proxy_is_godot((struct wl_proxy *)wl_output)) {
		// This won't have the right data bound to it. Not worth it and would probably
		// just break everything.
		return;
	}

	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	ws->wl_outputs.erase(wl_output);

	DEBUG_LOG_WAYLAND_THREAD(vformat("Window left output %x.\n", (size_t)wl_output));
}

// TODO: Add support to this event.
void WaylandThread::_wl_surface_on_preferred_buffer_scale(void *data, struct wl_surface *wl_surface, int32_t factor) {
}

// TODO: Add support to this event.
void WaylandThread::_wl_surface_on_preferred_buffer_transform(void *data, struct wl_surface *wl_surface, uint32_t transform) {
}

void WaylandThread::_wl_output_on_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
	ScreenState *ss = (ScreenState *)data;
	ERR_FAIL_NULL(ss);

	ss->pending_data.position.x = x;

	ss->pending_data.position.x = x;
	ss->pending_data.position.y = y;

	ss->pending_data.physical_size.width = physical_width;
	ss->pending_data.physical_size.height = physical_height;

	ss->pending_data.make.parse_utf8(make);
	ss->pending_data.model.parse_utf8(model);

	// `wl_output::done` is a version 2 addition. We'll directly update the data
	// for compatibility.
	if (wl_output_get_version(wl_output) == 1) {
		ss->data = ss->pending_data;
	}
}

void WaylandThread::_wl_output_on_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	ScreenState *ss = (ScreenState *)data;
	ERR_FAIL_NULL(ss);

	ss->pending_data.size.width = width;
	ss->pending_data.size.height = height;

	ss->pending_data.refresh_rate = refresh ? refresh / 1000.0f : -1;

	// `wl_output::done` is a version 2 addition. We'll directly update the data
	// for compatibility.
	if (wl_output_get_version(wl_output) == 1) {
		ss->data = ss->pending_data;
	}
}

// NOTE: The following `wl_output` events are only for version 2 onwards, so we
// can assume that they're "atomic" (i.e. rely on the `wl_output::done` event).

void WaylandThread::_wl_output_on_done(void *data, struct wl_output *wl_output) {
	ScreenState *ss = (ScreenState *)data;
	ERR_FAIL_NULL(ss);

	ss->data = ss->pending_data;

	ss->wayland_thread->_update_scale(ss->data.scale);

	DEBUG_LOG_WAYLAND_THREAD(vformat("Output %x done.", (size_t)wl_output));
}

void WaylandThread::_wl_output_on_scale(void *data, struct wl_output *wl_output, int32_t factor) {
	ScreenState *ss = (ScreenState *)data;
	ERR_FAIL_NULL(ss);

	ss->pending_data.scale = factor;

	DEBUG_LOG_WAYLAND_THREAD(vformat("Output %x scale %d", (size_t)wl_output, factor));
}

void WaylandThread::_wl_output_on_name(void *data, struct wl_output *wl_output, const char *name) {
}

void WaylandThread::_wl_output_on_description(void *data, struct wl_output *wl_output, const char *description) {
}

void WaylandThread::_xdg_wm_base_on_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

void WaylandThread::_xdg_surface_on_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);

	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	DEBUG_LOG_WAYLAND_THREAD(vformat("xdg surface on configure width %d height %d", ws->rect.size.width, ws->rect.size.height));
}

void WaylandThread::_xdg_toplevel_on_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	// Expect the window to be in a plain state. It will get properly set if the
	// compositor reports otherwise below.
	ws->mode = DisplayServer::WINDOW_MODE_WINDOWED;
	ws->suspended = false;

	uint32_t *state = nullptr;
	wl_array_for_each(state, states) {
		switch (*state) {
			case XDG_TOPLEVEL_STATE_MAXIMIZED: {
				ws->mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
			} break;

			case XDG_TOPLEVEL_STATE_FULLSCREEN: {
				ws->mode = DisplayServer::WINDOW_MODE_FULLSCREEN;
			} break;

			case XDG_TOPLEVEL_STATE_SUSPENDED: {
				ws->suspended = true;
			} break;

			default: {
				// We don't care about the other states (for now).
			} break;
		}
	}

	if (width != 0 && height != 0) {
		window_state_update_size(ws, width, height);
	}

	DEBUG_LOG_WAYLAND_THREAD(vformat("XDG toplevel on configure width %d height %d.", width, height));
}

void WaylandThread::_xdg_toplevel_on_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_CLOSE_REQUEST;
	ws->wayland_thread->push_message(msg);
}

void WaylandThread::_xdg_toplevel_on_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
}

void WaylandThread::_xdg_toplevel_on_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	ws->can_maximize = false;
	ws->can_fullscreen = false;
	ws->can_minimize = false;

	uint32_t *capability = nullptr;
	wl_array_for_each(capability, capabilities) {
		switch (*capability) {
			case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE: {
				ws->can_maximize = true;
			} break;
			case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN: {
				ws->can_fullscreen = true;
			} break;

			case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE: {
				ws->can_minimize = true;
			} break;

			default: {
			} break;
		}
	}
}

// NOTE: Deprecated.
void WaylandThread::_xdg_exported_v1_on_handle(void *data, zxdg_exported_v1 *exported, const char *handle) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	ws->exported_handle = vformat("wayland:%s", String::utf8(handle));
}

void WaylandThread::_xdg_exported_v2_on_handle(void *data, zxdg_exported_v2 *exported, const char *handle) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	ws->exported_handle = vformat("wayland:%s", String::utf8(handle));
}

void WaylandThread::_xdg_toplevel_decoration_on_configure(void *data, struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration, uint32_t mode) {
	if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
#ifdef LIBDECOR_ENABLED
		WARN_PRINT_ONCE("Native client side decorations are not yet supported without libdecor!");
#else
		WARN_PRINT_ONCE("Native client side decorations are not yet supported!");
#endif // LIBDECOR_ENABLED
	}
}

#ifdef LIBDECOR_ENABLED
void WaylandThread::libdecor_on_error(struct libdecor *context, enum libdecor_error error, const char *message) {
	ERR_PRINT(vformat("libdecor error %d: %s", error, message));
}

// NOTE: This is pretty much a reimplementation of _xdg_surface_on_configure
// and _xdg_toplevel_on_configure. Libdecor really likes wrapping everything,
// forcing us to do stuff like this.
void WaylandThread::libdecor_frame_on_configure(struct libdecor_frame *frame, struct libdecor_configuration *configuration, void *user_data) {
	WindowState *ws = (WindowState *)user_data;
	ERR_FAIL_NULL(ws);

	int width = 0;
	int height = 0;

	ws->pending_libdecor_configuration = configuration;

	if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
		// The configuration doesn't have a size. We'll use the one already set in the window.
		width = ws->rect.size.width;
		height = ws->rect.size.height;
	}

	ERR_FAIL_COND_MSG(width == 0 || height == 0, "Window has invalid size.");

	libdecor_window_state window_state = LIBDECOR_WINDOW_STATE_NONE;

	// Expect the window to be in a plain state. It will get properly set if the
	// compositor reports otherwise below.
	ws->mode = DisplayServer::WINDOW_MODE_WINDOWED;
	ws->suspended = false;

	if (libdecor_configuration_get_window_state(configuration, &window_state)) {
		if (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) {
			ws->mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
		}

		if (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) {
			ws->mode = DisplayServer::WINDOW_MODE_FULLSCREEN;
		}

		if (window_state & LIBDECOR_WINDOW_STATE_SUSPENDED) {
			ws->suspended = true;
		}
	}

	window_state_update_size(ws, width, height);

	DEBUG_LOG_WAYLAND_THREAD(vformat("libdecor frame on configure rect %s", ws->rect));
}

void WaylandThread::libdecor_frame_on_close(struct libdecor_frame *frame, void *user_data) {
	WindowState *ws = (WindowState *)user_data;
	ERR_FAIL_NULL(ws);

	Ref<WindowEventMessage> winevent_msg;
	winevent_msg.instantiate();
	winevent_msg->event = DisplayServer::WINDOW_EVENT_CLOSE_REQUEST;

	ws->wayland_thread->push_message(winevent_msg);

	DEBUG_LOG_WAYLAND_THREAD("libdecor frame on close");
}

void WaylandThread::libdecor_frame_on_commit(struct libdecor_frame *frame, void *user_data) {
	// We're skipping this as we don't really care about libdecor's commit for
	// atomicity reasons. See `_frame_wl_callback_on_done` for more info.

	DEBUG_LOG_WAYLAND_THREAD("libdecor frame on commit");
}

void WaylandThread::libdecor_frame_on_dismiss_popup(struct libdecor_frame *frame, const char *seat_name, void *user_data) {
}
#endif // LIBDECOR_ENABLED

void WaylandThread::_wl_seat_on_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	SeatState *ss = (SeatState *)data;

	ERR_FAIL_NULL(ss);

	// TODO: Handle touch.

	// Pointer handling.
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		if (!ss->wl_pointer) {
			ss->cursor_surface = wl_compositor_create_surface(ss->registry->wl_compositor);
			wl_surface_commit(ss->cursor_surface);

			ss->wl_pointer = wl_seat_get_pointer(wl_seat);
			wl_pointer_add_listener(ss->wl_pointer, &wl_pointer_listener, ss);

			if (ss->registry->wp_relative_pointer_manager) {
				ss->wp_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(ss->registry->wp_relative_pointer_manager, ss->wl_pointer);
				zwp_relative_pointer_v1_add_listener(ss->wp_relative_pointer, &wp_relative_pointer_listener, ss);
			}

			if (ss->registry->wp_pointer_gestures) {
				ss->wp_pointer_gesture_pinch = zwp_pointer_gestures_v1_get_pinch_gesture(ss->registry->wp_pointer_gestures, ss->wl_pointer);
				zwp_pointer_gesture_pinch_v1_add_listener(ss->wp_pointer_gesture_pinch, &wp_pointer_gesture_pinch_listener, ss);
			}

			// TODO: Constrain new pointers if the global mouse mode is constrained.
		}
	} else {
		if (ss->cursor_frame_callback) {
			// Just in case. I got bitten by weird race-like conditions already.
			wl_callback_set_user_data(ss->cursor_frame_callback, nullptr);

			wl_callback_destroy(ss->cursor_frame_callback);
			ss->cursor_frame_callback = nullptr;
		}

		if (ss->cursor_surface) {
			wl_surface_destroy(ss->cursor_surface);
			ss->cursor_surface = nullptr;
		}

		if (ss->wl_pointer) {
			wl_pointer_destroy(ss->wl_pointer);
			ss->wl_pointer = nullptr;
		}

		if (ss->wp_relative_pointer) {
			zwp_relative_pointer_v1_destroy(ss->wp_relative_pointer);
			ss->wp_relative_pointer = nullptr;
		}

		if (ss->wp_confined_pointer) {
			zwp_confined_pointer_v1_destroy(ss->wp_confined_pointer);
			ss->wp_confined_pointer = nullptr;
		}

		if (ss->wp_locked_pointer) {
			zwp_locked_pointer_v1_destroy(ss->wp_locked_pointer);
			ss->wp_locked_pointer = nullptr;
		}
	}

	// Keyboard handling.
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		if (!ss->wl_keyboard) {
			ss->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			ERR_FAIL_NULL(ss->xkb_context);

			ss->wl_keyboard = wl_seat_get_keyboard(wl_seat);
			wl_keyboard_add_listener(ss->wl_keyboard, &wl_keyboard_listener, ss);
		}
	} else {
		if (ss->xkb_context) {
			xkb_context_unref(ss->xkb_context);
			ss->xkb_context = nullptr;
		}

		if (ss->wl_keyboard) {
			wl_keyboard_destroy(ss->wl_keyboard);
			ss->wl_keyboard = nullptr;
		}
	}
}

void WaylandThread::_wl_seat_on_name(void *data, struct wl_seat *wl_seat, const char *name) {
}

void WaylandThread::_cursor_frame_callback_on_done(void *data, struct wl_callback *wl_callback, uint32_t time_ms) {
	wl_callback_destroy(wl_callback);

	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	ss->cursor_frame_callback = nullptr;

	ss->cursor_time_ms = time_ms;

	seat_state_update_cursor(ss);
}

void WaylandThread::_wl_pointer_on_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	if (!surface || !wl_proxy_is_godot((struct wl_proxy *)surface)) {
		return;
	}

	DEBUG_LOG_WAYLAND_THREAD("Pointing window.");

	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	ERR_FAIL_NULL(ss->cursor_surface);
	ss->pointer_enter_serial = serial;
	ss->pointed_surface = surface;
	ss->last_pointed_surface = surface;

	seat_state_update_cursor(ss);

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_MOUSE_ENTER;

	ss->wayland_thread->push_message(msg);
}

void WaylandThread::_wl_pointer_on_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {
	if (!surface || !wl_proxy_is_godot((struct wl_proxy *)surface)) {
		return;
	}

	DEBUG_LOG_WAYLAND_THREAD("Left window.");

	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	ss->pointed_surface = nullptr;

	ss->pointer_data_buffer.pressed_button_mask.clear();

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_MOUSE_EXIT;

	wayland_thread->push_message(msg);
}

void WaylandThread::_wl_pointer_on_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	WindowState *ws = wl_surface_get_window_state(ss->pointed_surface);
	ERR_FAIL_NULL(ws);

	PointerData &pd = ss->pointer_data_buffer;

	// TODO: Scale only when sending the Wayland message.
	pd.position.x = wl_fixed_to_double(surface_x);
	pd.position.y = wl_fixed_to_double(surface_y);

	pd.position *= window_state_get_scale_factor(ws);

	pd.motion_time = time;
}

void WaylandThread::_wl_pointer_on_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	PointerData &pd = ss->pointer_data_buffer;

	MouseButton button_pressed = MouseButton::NONE;

	switch (button) {
		case BTN_LEFT:
			button_pressed = MouseButton::LEFT;
			break;

		case BTN_MIDDLE:
			button_pressed = MouseButton::MIDDLE;
			break;

		case BTN_RIGHT:
			button_pressed = MouseButton::RIGHT;
			break;

		case BTN_EXTRA:
			button_pressed = MouseButton::MB_XBUTTON1;
			break;

		case BTN_SIDE:
			button_pressed = MouseButton::MB_XBUTTON2;
			break;

		default: {
		}
	}

	MouseButtonMask mask = mouse_button_to_mask(button_pressed);

	if (state & WL_POINTER_BUTTON_STATE_PRESSED) {
		pd.pressed_button_mask.set_flag(mask);
		pd.last_button_pressed = button_pressed;
		pd.double_click_begun = true;
	} else {
		pd.pressed_button_mask.clear_flag(mask);
	}

	pd.button_time = time;
	pd.button_serial = serial;
}

void WaylandThread::_wl_pointer_on_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	PointerData &pd = ss->pointer_data_buffer;

	switch (axis) {
		case WL_POINTER_AXIS_VERTICAL_SCROLL: {
			pd.scroll_vector.y = wl_fixed_to_double(value);
		} break;

		case WL_POINTER_AXIS_HORIZONTAL_SCROLL: {
			pd.scroll_vector.x = wl_fixed_to_double(value);
		} break;
	}

	pd.button_time = time;
}

void WaylandThread::_wl_pointer_on_frame(void *data, struct wl_pointer *wl_pointer) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	wayland_thread->_set_current_seat(ss->wl_seat);

	PointerData &old_pd = ss->pointer_data;
	PointerData &pd = ss->pointer_data_buffer;

	if (old_pd.motion_time != pd.motion_time || old_pd.relative_motion_time != pd.relative_motion_time) {
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();

		// Set all pressed modifiers.
		mm->set_shift_pressed(ss->shift_pressed);
		mm->set_ctrl_pressed(ss->ctrl_pressed);
		mm->set_alt_pressed(ss->alt_pressed);
		mm->set_meta_pressed(ss->meta_pressed);

		mm->set_window_id(DisplayServer::MAIN_WINDOW_ID);
		mm->set_button_mask(pd.pressed_button_mask);
		mm->set_position(pd.position);
		mm->set_global_position(pd.position);

		Vector2 pos_delta = pd.position - old_pd.position;

		if (old_pd.relative_motion_time != pd.relative_motion_time) {
			uint32_t time_delta = pd.relative_motion_time - old_pd.relative_motion_time;

			mm->set_relative(pd.relative_motion);
			mm->set_velocity((Vector2)pos_delta / time_delta);
		} else {
			// The spec includes the possibility of having motion events without an
			// associated relative motion event. If that's the case, fallback to a
			// simple delta of the position. The captured mouse won't report the
			// relative speed anymore though.
			uint32_t time_delta = pd.motion_time - old_pd.motion_time;

			mm->set_relative(pd.position - old_pd.position);
			mm->set_velocity((Vector2)pos_delta / time_delta);
		}
		mm->set_relative_screen_position(mm->get_relative());
		mm->set_screen_velocity(mm->get_velocity());

		Ref<InputEventMessage> msg;
		msg.instantiate();

		msg->event = mm;

		wayland_thread->push_message(msg);
	}

	if (pd.discrete_scroll_vector_120 - old_pd.discrete_scroll_vector_120 != Vector2i()) {
		// This is a discrete scroll (eg. from a scroll wheel), so we'll just emit
		// scroll wheel buttons.
		if (pd.scroll_vector.y != 0) {
			MouseButton button = pd.scroll_vector.y > 0 ? MouseButton::WHEEL_DOWN : MouseButton::WHEEL_UP;
			pd.pressed_button_mask.set_flag(mouse_button_to_mask(button));
		}

		if (pd.scroll_vector.x != 0) {
			MouseButton button = pd.scroll_vector.x > 0 ? MouseButton::WHEEL_RIGHT : MouseButton::WHEEL_LEFT;
			pd.pressed_button_mask.set_flag(mouse_button_to_mask(button));
		}
	} else {
		if (pd.scroll_vector - old_pd.scroll_vector != Vector2()) {
			// This is a continuous scroll, so we'll emit a pan gesture.
			Ref<InputEventPanGesture> pg;
			pg.instantiate();

			// Set all pressed modifiers.
			pg->set_shift_pressed(ss->shift_pressed);
			pg->set_ctrl_pressed(ss->ctrl_pressed);
			pg->set_alt_pressed(ss->alt_pressed);
			pg->set_meta_pressed(ss->meta_pressed);

			pg->set_position(pd.position);

			pg->set_window_id(DisplayServer::MAIN_WINDOW_ID);

			pg->set_delta(pd.scroll_vector);

			Ref<InputEventMessage> msg;
			msg.instantiate();

			msg->event = pg;

			wayland_thread->push_message(msg);
		}
	}

	if (old_pd.pressed_button_mask != pd.pressed_button_mask) {
		BitField<MouseButtonMask> pressed_mask_delta = old_pd.pressed_button_mask ^ pd.pressed_button_mask;

		const MouseButton buttons_to_test[] = {
			MouseButton::LEFT,
			MouseButton::MIDDLE,
			MouseButton::RIGHT,
			MouseButton::WHEEL_UP,
			MouseButton::WHEEL_DOWN,
			MouseButton::WHEEL_LEFT,
			MouseButton::WHEEL_RIGHT,
			MouseButton::MB_XBUTTON1,
			MouseButton::MB_XBUTTON2,
		};

		for (MouseButton test_button : buttons_to_test) {
			MouseButtonMask test_button_mask = mouse_button_to_mask(test_button);
			if (pressed_mask_delta.has_flag(test_button_mask)) {
				Ref<InputEventMouseButton> mb;
				mb.instantiate();

				// Set all pressed modifiers.
				mb->set_shift_pressed(ss->shift_pressed);
				mb->set_ctrl_pressed(ss->ctrl_pressed);
				mb->set_alt_pressed(ss->alt_pressed);
				mb->set_meta_pressed(ss->meta_pressed);

				mb->set_window_id(DisplayServer::MAIN_WINDOW_ID);
				mb->set_position(pd.position);
				mb->set_global_position(pd.position);

				if (test_button == MouseButton::WHEEL_UP || test_button == MouseButton::WHEEL_DOWN) {
					// If this is a discrete scroll, specify how many "clicks" it did for this
					// pointer frame.
					mb->set_factor(Math::abs(pd.discrete_scroll_vector_120.y / (float)120));
				}

				if (test_button == MouseButton::WHEEL_RIGHT || test_button == MouseButton::WHEEL_LEFT) {
					// If this is a discrete scroll, specify how many "clicks" it did for this
					// pointer frame.
					mb->set_factor(fabs(pd.discrete_scroll_vector_120.x / (float)120));
				}

				mb->set_button_mask(pd.pressed_button_mask);

				mb->set_button_index(test_button);
				mb->set_pressed(pd.pressed_button_mask.has_flag(test_button_mask));

				// We have to set the last position pressed here as we can't take for
				// granted what the individual events might have seen due to them not having
				// a guaranteed order.
				if (mb->is_pressed()) {
					pd.last_pressed_position = pd.position;
				}

				if (old_pd.double_click_begun && mb->is_pressed() && pd.last_button_pressed == old_pd.last_button_pressed && (pd.button_time - old_pd.button_time) < 400 && Vector2(old_pd.last_pressed_position).distance_to(Vector2(pd.last_pressed_position)) < 5) {
					pd.double_click_begun = false;
					mb->set_double_click(true);
				}

				Ref<InputEventMessage> msg;
				msg.instantiate();

				msg->event = mb;

				wayland_thread->push_message(msg);

				// Send an event resetting immediately the wheel key.
				// Wayland specification defines axis_stop events as optional and says to
				// treat all axis events as unterminated. As such, we have to manually do
				// it ourselves.
				if (test_button == MouseButton::WHEEL_UP || test_button == MouseButton::WHEEL_DOWN || test_button == MouseButton::WHEEL_LEFT || test_button == MouseButton::WHEEL_RIGHT) {
					// FIXME: This is ugly, I can't find a clean way to clone an InputEvent.
					// This works for now, despite being horrible.
					Ref<InputEventMouseButton> wh_up;
					wh_up.instantiate();

					wh_up->set_window_id(DisplayServer::MAIN_WINDOW_ID);
					wh_up->set_position(pd.position);
					wh_up->set_global_position(pd.position);

					// We have to unset the button to avoid it getting stuck.
					pd.pressed_button_mask.clear_flag(test_button_mask);
					wh_up->set_button_mask(pd.pressed_button_mask);

					wh_up->set_button_index(test_button);
					wh_up->set_pressed(false);

					Ref<InputEventMessage> msg_up;
					msg_up.instantiate();
					msg_up->event = wh_up;
					wayland_thread->push_message(msg_up);
				}
			}
		}
	}

	// Reset the scroll vectors as we already handled them.
	pd.scroll_vector = Vector2();
	pd.discrete_scroll_vector_120 = Vector2i();

	// Update the data all getters read. Wayland's specification requires us to do
	// this, since all pointer actions are sent in individual events.
	old_pd = pd;
}

void WaylandThread::_wl_pointer_on_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	ss->pointer_data_buffer.scroll_type = axis_source;
}

void WaylandThread::_wl_pointer_on_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
}

// NOTE: This event is deprecated since version 8 and superseded by
// `wl_pointer::axis_value120`. This thus converts the data to its
// fraction-of-120 format.
void WaylandThread::_wl_pointer_on_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	PointerData &pd = ss->pointer_data_buffer;

	// NOTE: We can allow ourselves to not accumulate this data (and thus just
	// assign it) as the spec guarantees only one event per axis type.

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
		pd.discrete_scroll_vector_120.y = discrete * 120;
	}

	if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		pd.discrete_scroll_vector_120.x = discrete * 120;
	}
}

// Supersedes `wl_pointer::axis_discrete` Since version 8.
void WaylandThread::_wl_pointer_on_axis_value120(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t value120) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	PointerData &pd = ss->pointer_data_buffer;

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
		pd.discrete_scroll_vector_120.y += value120;
	}

	if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		pd.discrete_scroll_vector_120.x += value120;
	}
}

// TODO: Add support to this event.
void WaylandThread::_wl_pointer_on_axis_relative_direction(void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction) {
}

void WaylandThread::_wl_keyboard_on_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
	ERR_FAIL_COND_MSG(format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, "Unsupported keymap format announced from the Wayland compositor.");

	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (ss->keymap_buffer) {
		// We have already a mapped buffer, so we unmap it. There's no need to reset
		// its pointer or size, as we're gonna set them below.
		munmap((void *)ss->keymap_buffer, ss->keymap_buffer_size);
		ss->keymap_buffer = nullptr;
	}

	ss->keymap_buffer = (const char *)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	ss->keymap_buffer_size = size;

	xkb_keymap_unref(ss->xkb_keymap);
	ss->xkb_keymap = xkb_keymap_new_from_string(ss->xkb_context, ss->keymap_buffer,
			XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

	xkb_state_unref(ss->xkb_state);
	ss->xkb_state = xkb_state_new(ss->xkb_keymap);
}

void WaylandThread::_wl_keyboard_on_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	wayland_thread->_set_current_seat(ss->wl_seat);

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_FOCUS_IN;
	wayland_thread->push_message(msg);
}

void WaylandThread::_wl_keyboard_on_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	ss->repeating_keycode = XKB_KEYCODE_INVALID;

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_FOCUS_OUT;
	wayland_thread->push_message(msg);
}

void WaylandThread::_wl_keyboard_on_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	// We have to add 8 to the scancode to get an XKB-compatible keycode.
	xkb_keycode_t xkb_keycode = key + 8;

	bool pressed = state & WL_KEYBOARD_KEY_STATE_PRESSED;

	if (pressed) {
		if (xkb_keymap_key_repeats(ss->xkb_keymap, xkb_keycode)) {
			ss->last_repeat_start_msec = OS::get_singleton()->get_ticks_msec();
			ss->repeating_keycode = xkb_keycode;
		}

		ss->last_key_pressed_serial = serial;
	} else if (ss->repeating_keycode == xkb_keycode) {
		ss->repeating_keycode = XKB_KEYCODE_INVALID;
	}

	Ref<InputEventKey> k;
	k.instantiate();

	if (!_seat_state_configure_key_event(*ss, k, xkb_keycode, pressed)) {
		return;
	}

	Ref<InputEventMessage> msg;
	msg.instantiate();
	msg->event = k;
	wayland_thread->push_message(msg);
}

void WaylandThread::_wl_keyboard_on_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	xkb_state_update_mask(ss->xkb_state, mods_depressed, mods_latched, mods_locked, ss->current_layout_index, ss->current_layout_index, group);

	ss->shift_pressed = xkb_state_mod_name_is_active(ss->xkb_state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_DEPRESSED);
	ss->ctrl_pressed = xkb_state_mod_name_is_active(ss->xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_DEPRESSED);
	ss->alt_pressed = xkb_state_mod_name_is_active(ss->xkb_state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_DEPRESSED);
	ss->meta_pressed = xkb_state_mod_name_is_active(ss->xkb_state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_DEPRESSED);

	ss->current_layout_index = group;
}

void WaylandThread::_wl_keyboard_on_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	ss->repeat_key_delay_msec = 1000 / rate;
	ss->repeat_start_delay_msec = delay;
}

// NOTE: Don't forget to `memfree` the offer's state.
void WaylandThread::_wl_data_device_on_data_offer(void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
	wl_proxy_tag_godot((struct wl_proxy *)id);
	wl_data_offer_add_listener(id, &wl_data_offer_listener, memnew(OfferState));
}

void WaylandThread::_wl_data_device_on_enter(void *data, struct wl_data_device *wl_data_device, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	ss->dnd_enter_serial = serial;
	ss->wl_data_offer_dnd = id;

	// Godot only supports DnD file copying for now.
	wl_data_offer_accept(id, serial, "text/uri-list");
	wl_data_offer_set_actions(id, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
}

void WaylandThread::_wl_data_device_on_leave(void *data, struct wl_data_device *wl_data_device) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (ss->wl_data_offer_dnd) {
		memdelete(wl_data_offer_get_offer_state(ss->wl_data_offer_dnd));
		wl_data_offer_destroy(ss->wl_data_offer_dnd);
		ss->wl_data_offer_dnd = nullptr;
	}
}

void WaylandThread::_wl_data_device_on_motion(void *data, struct wl_data_device *wl_data_device, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
}

void WaylandThread::_wl_data_device_on_drop(void *data, struct wl_data_device *wl_data_device) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	OfferState *os = wl_data_offer_get_offer_state(ss->wl_data_offer_dnd);
	ERR_FAIL_NULL(os);

	if (os) {
		Ref<DropFilesEventMessage> msg;
		msg.instantiate();

		Vector<uint8_t> list_data = _wl_data_offer_read(wayland_thread->wl_display, "text/uri-list", ss->wl_data_offer_dnd);

		msg->files = String::utf8((const char *)list_data.ptr(), list_data.size()).split("\r\n", false);
		for (int i = 0; i < msg->files.size(); i++) {
			msg->files.write[i] = msg->files[i].replace("file://", "").uri_decode();
		}

		wayland_thread->push_message(msg);

		wl_data_offer_finish(ss->wl_data_offer_dnd);
	}

	memdelete(wl_data_offer_get_offer_state(ss->wl_data_offer_dnd));
	wl_data_offer_destroy(ss->wl_data_offer_dnd);
	ss->wl_data_offer_dnd = nullptr;
}

void WaylandThread::_wl_data_device_on_selection(void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (ss->wl_data_offer_selection) {
		memdelete(wl_data_offer_get_offer_state(ss->wl_data_offer_selection));
		wl_data_offer_destroy(ss->wl_data_offer_selection);
	}

	ss->wl_data_offer_selection = id;
}

void WaylandThread::_wl_data_offer_on_offer(void *data, struct wl_data_offer *wl_data_offer, const char *mime_type) {
	OfferState *os = (OfferState *)data;
	ERR_FAIL_NULL(os);

	if (os) {
		os->mime_types.insert(String::utf8(mime_type));
	}
}

void WaylandThread::_wl_data_offer_on_source_actions(void *data, struct wl_data_offer *wl_data_offer, uint32_t source_actions) {
}

void WaylandThread::_wl_data_offer_on_action(void *data, struct wl_data_offer *wl_data_offer, uint32_t dnd_action) {
}

void WaylandThread::_wl_data_source_on_target(void *data, struct wl_data_source *wl_data_source, const char *mime_type) {
}

void WaylandThread::_wl_data_source_on_send(void *data, struct wl_data_source *wl_data_source, const char *mime_type, int32_t fd) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	Vector<uint8_t> *data_to_send = nullptr;

	if (wl_data_source == ss->wl_data_source_selection) {
		data_to_send = &ss->selection_data;
		DEBUG_LOG_WAYLAND_THREAD("Clipboard: requested selection.");
	}

	if (data_to_send) {
		ssize_t written_bytes = 0;

		bool valid_mime = false;

		if (strcmp(mime_type, "text/plain;charset=utf-8") == 0) {
			valid_mime = true;
		} else if (strcmp(mime_type, "text/plain") == 0) {
			valid_mime = true;
		}

		if (valid_mime) {
			written_bytes = write(fd, data_to_send->ptr(), data_to_send->size());
		}

		if (written_bytes > 0) {
			DEBUG_LOG_WAYLAND_THREAD(vformat("Clipboard: sent %d bytes.", written_bytes));
		} else if (written_bytes == 0) {
			DEBUG_LOG_WAYLAND_THREAD("Clipboard: no bytes sent.");
		} else {
			ERR_PRINT(vformat("Clipboard: write error %d.", errno));
		}
	}

	close(fd);
}

void WaylandThread::_wl_data_source_on_cancelled(void *data, struct wl_data_source *wl_data_source) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	wl_data_source_destroy(wl_data_source);

	if (wl_data_source == ss->wl_data_source_selection) {
		ss->wl_data_source_selection = nullptr;
		ss->selection_data.clear();

		DEBUG_LOG_WAYLAND_THREAD("Clipboard: selection set by another program.");
		return;
	}
}

void WaylandThread::_wl_data_source_on_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source) {
}

void WaylandThread::_wl_data_source_on_dnd_finished(void *data, struct wl_data_source *wl_data_source) {
}

void WaylandThread::_wl_data_source_on_action(void *data, struct wl_data_source *wl_data_source, uint32_t dnd_action) {
}

void WaylandThread::_wp_fractional_scale_on_preferred_scale(void *data, struct wp_fractional_scale_v1 *wp_fractional_scale_v1, uint32_t scale) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);

	ws->preferred_fractional_scale = (double)scale / 120;

	window_state_update_size(ws, ws->rect.size.width, ws->rect.size.height);
}

void WaylandThread::_wp_relative_pointer_on_relative_motion(void *data, struct zwp_relative_pointer_v1 *wp_relative_pointer, uint32_t uptime_hi, uint32_t uptime_lo, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (!ss->pointed_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	PointerData &pd = ss->pointer_data_buffer;

	WindowState *ws = wl_surface_get_window_state(ss->pointed_surface);
	ERR_FAIL_NULL(ws);

	pd.relative_motion.x = wl_fixed_to_double(dx);
	pd.relative_motion.y = wl_fixed_to_double(dy);

	pd.relative_motion *= window_state_get_scale_factor(ws);

	pd.relative_motion_time = uptime_lo;
}

void WaylandThread::_wp_pointer_gesture_pinch_on_begin(void *data, struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch_v1, uint32_t serial, uint32_t time, struct wl_surface *surface, uint32_t fingers) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (fingers == 2) {
		ss->old_pinch_scale = wl_fixed_from_int(1);
		ss->active_gesture = Gesture::MAGNIFY;
	}
}

void WaylandThread::_wp_pointer_gesture_pinch_on_update(void *data, struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch_v1, uint32_t time, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t scale, wl_fixed_t rotation) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	PointerData &pd = ss->pointer_data_buffer;

	if (ss->active_gesture == Gesture::MAGNIFY) {
		Ref<InputEventMagnifyGesture> mg;
		mg.instantiate();

		mg->set_window_id(DisplayServer::MAIN_WINDOW_ID);

		// Set all pressed modifiers.
		mg->set_shift_pressed(ss->shift_pressed);
		mg->set_ctrl_pressed(ss->ctrl_pressed);
		mg->set_alt_pressed(ss->alt_pressed);
		mg->set_meta_pressed(ss->meta_pressed);

		mg->set_position(pd.position);

		wl_fixed_t scale_delta = scale - ss->old_pinch_scale;
		mg->set_factor(1 + wl_fixed_to_double(scale_delta));

		Ref<InputEventMessage> magnify_msg;
		magnify_msg.instantiate();
		magnify_msg->event = mg;

		// Since Wayland allows only one gesture at a time and godot instead expects
		// both of them, we'll have to create two separate input events: one for
		// magnification and one for panning.

		Ref<InputEventPanGesture> pg;
		pg.instantiate();

		pg->set_window_id(DisplayServer::MAIN_WINDOW_ID);

		// Set all pressed modifiers.
		pg->set_shift_pressed(ss->shift_pressed);
		pg->set_ctrl_pressed(ss->ctrl_pressed);
		pg->set_alt_pressed(ss->alt_pressed);
		pg->set_meta_pressed(ss->meta_pressed);

		pg->set_position(pd.position);
		pg->set_delta(Vector2(wl_fixed_to_double(dx), wl_fixed_to_double(dy)));

		Ref<InputEventMessage> pan_msg;
		pan_msg.instantiate();
		pan_msg->event = pg;

		wayland_thread->push_message(magnify_msg);
		wayland_thread->push_message(pan_msg);

		ss->old_pinch_scale = scale;
	}
}

void WaylandThread::_wp_pointer_gesture_pinch_on_end(void *data, struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch_v1, uint32_t serial, uint32_t time, int32_t cancelled) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	ss->active_gesture = Gesture::NONE;
}

// NOTE: Don't forget to `memfree` the offer's state.
void WaylandThread::_wp_primary_selection_device_on_data_offer(void *data, struct zwp_primary_selection_device_v1 *wp_primary_selection_device_v1, struct zwp_primary_selection_offer_v1 *offer) {
	wl_proxy_tag_godot((struct wl_proxy *)offer);
	zwp_primary_selection_offer_v1_add_listener(offer, &wp_primary_selection_offer_listener, memnew(OfferState));
}

void WaylandThread::_wp_primary_selection_device_on_selection(void *data, struct zwp_primary_selection_device_v1 *wp_primary_selection_device_v1, struct zwp_primary_selection_offer_v1 *id) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (ss->wp_primary_selection_offer) {
		memfree(wp_primary_selection_offer_get_offer_state(ss->wp_primary_selection_offer));
		zwp_primary_selection_offer_v1_destroy(ss->wp_primary_selection_offer);
	}

	ss->wp_primary_selection_offer = id;
}

void WaylandThread::_wp_primary_selection_offer_on_offer(void *data, struct zwp_primary_selection_offer_v1 *wp_primary_selection_offer_v1, const char *mime_type) {
	OfferState *os = (OfferState *)data;
	ERR_FAIL_NULL(os);

	if (os) {
		os->mime_types.insert(String::utf8(mime_type));
	}
}

void WaylandThread::_wp_primary_selection_source_on_send(void *data, struct zwp_primary_selection_source_v1 *wp_primary_selection_source_v1, const char *mime_type, int32_t fd) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	Vector<uint8_t> *data_to_send = nullptr;

	if (wp_primary_selection_source_v1 == ss->wp_primary_selection_source) {
		data_to_send = &ss->primary_data;
		DEBUG_LOG_WAYLAND_THREAD("Clipboard: requested primary selection.");
	}

	if (data_to_send) {
		ssize_t written_bytes = 0;

		if (strcmp(mime_type, "text/plain") == 0) {
			written_bytes = write(fd, data_to_send->ptr(), data_to_send->size());
		}

		if (written_bytes > 0) {
			DEBUG_LOG_WAYLAND_THREAD(vformat("Clipboard: sent %d bytes.", written_bytes));
		} else if (written_bytes == 0) {
			DEBUG_LOG_WAYLAND_THREAD("Clipboard: no bytes sent.");
		} else {
			ERR_PRINT(vformat("Clipboard: write error %d.", errno));
		}
	}

	close(fd);
}

void WaylandThread::_wp_primary_selection_source_on_cancelled(void *data, struct zwp_primary_selection_source_v1 *wp_primary_selection_source_v1) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	if (wp_primary_selection_source_v1 == ss->wp_primary_selection_source) {
		zwp_primary_selection_source_v1_destroy(ss->wp_primary_selection_source);
		ss->wp_primary_selection_source = nullptr;

		ss->primary_data.clear();

		DEBUG_LOG_WAYLAND_THREAD("Clipboard: primary selection set by another program.");
		return;
	}
}

void WaylandThread::_wp_tablet_seat_on_tablet_added(void *data, struct zwp_tablet_seat_v2 *wp_tablet_seat_v2, struct zwp_tablet_v2 *id) {
}

void WaylandThread::_wp_tablet_seat_on_tool_added(void *data, struct zwp_tablet_seat_v2 *wp_tablet_seat_v2, struct zwp_tablet_tool_v2 *id) {
	SeatState *ss = (SeatState *)data;
	ERR_FAIL_NULL(ss);

	TabletToolState *state = memnew(TabletToolState);
	state->wl_seat = ss->wl_seat;

	wl_proxy_tag_godot((struct wl_proxy *)id);
	zwp_tablet_tool_v2_add_listener(id, &wp_tablet_tool_listener, state);

	ss->tablet_tools.push_back(id);
}

void WaylandThread::_wp_tablet_seat_on_pad_added(void *data, struct zwp_tablet_seat_v2 *wp_tablet_seat_v2, struct zwp_tablet_pad_v2 *id) {
}

void WaylandThread::_wp_tablet_tool_on_type(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t tool_type) {
	TabletToolState *state = wp_tablet_tool_get_state(wp_tablet_tool_v2);

	if (state && tool_type == ZWP_TABLET_TOOL_V2_TYPE_ERASER) {
		state->is_eraser = true;
	}
}

void WaylandThread::_wp_tablet_tool_on_hardware_serial(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t hardware_serial_hi, uint32_t hardware_serial_lo) {
}

void WaylandThread::_wp_tablet_tool_on_hardware_id_wacom(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t hardware_id_hi, uint32_t hardware_id_lo) {
}

void WaylandThread::_wp_tablet_tool_on_capability(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t capability) {
}

void WaylandThread::_wp_tablet_tool_on_done(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2) {
}

void WaylandThread::_wp_tablet_tool_on_removed(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	SeatState *ss = wl_seat_get_seat_state(ts->wl_seat);
	if (!ss) {
		return;
	}

	List<struct zwp_tablet_tool_v2 *>::Element *E = ss->tablet_tools.find(wp_tablet_tool_v2);

	if (E && E->get()) {
		struct zwp_tablet_tool_v2 *tool = E->get();
		TabletToolState *state = wp_tablet_tool_get_state(tool);
		if (state) {
			memdelete(state);
		}

		zwp_tablet_tool_v2_destroy(tool);
		ss->tablet_tools.erase(E);
	}
}

void WaylandThread::_wp_tablet_tool_on_proximity_in(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t serial, struct zwp_tablet_v2 *tablet, struct wl_surface *surface) {
	if (!surface || !wl_proxy_is_godot((struct wl_proxy *)surface)) {
		// We're probably on a decoration or something.
		return;
	}

	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	SeatState *ss = wl_seat_get_seat_state(ts->wl_seat);
	if (!ss) {
		return;
	}

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	ts->data_pending.proximity_serial = serial;
	ts->data_pending.proximal_surface = surface;
	ts->last_surface = surface;

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_MOUSE_ENTER;
	wayland_thread->push_message(msg);

	DEBUG_LOG_WAYLAND_THREAD("Tablet tool entered window.");
}

void WaylandThread::_wp_tablet_tool_on_proximity_out(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts || !ts->data_pending.proximal_surface) {
		// Not our stuff, we don't care.
		return;
	}

	SeatState *ss = wl_seat_get_seat_state(ts->wl_seat);
	if (!ss) {
		return;
	}

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	ts->data_pending.proximal_surface = nullptr;

	Ref<WindowEventMessage> msg;
	msg.instantiate();
	msg->event = DisplayServer::WINDOW_EVENT_MOUSE_EXIT;

	wayland_thread->push_message(msg);

	DEBUG_LOG_WAYLAND_THREAD("Tablet tool left window.");
}

void WaylandThread::_wp_tablet_tool_on_down(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t serial) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	TabletToolData &td = ts->data_pending;

	td.pressed_button_mask.set_flag(mouse_button_to_mask(MouseButton::LEFT));
	td.last_button_pressed = MouseButton::LEFT;
	td.double_click_begun = true;

	// The protocol doesn't cover this, but we can use this funky hack to make
	// double clicking work.
	td.button_time = OS::get_singleton()->get_ticks_msec();
}

void WaylandThread::_wp_tablet_tool_on_up(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	TabletToolData &td = ts->data_pending;

	td.pressed_button_mask.clear_flag(mouse_button_to_mask(MouseButton::LEFT));

	// The protocol doesn't cover this, but we can use this funky hack to make
	// double clicking work.
	td.button_time = OS::get_singleton()->get_ticks_msec();
}

void WaylandThread::_wp_tablet_tool_on_motion(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, wl_fixed_t x, wl_fixed_t y) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	if (!ts->data_pending.proximal_surface) {
		// We're probably on a decoration or some other third-party thing.
		return;
	}

	WindowState *ws = wl_surface_get_window_state(ts->data_pending.proximal_surface);
	ERR_FAIL_NULL(ws);

	TabletToolData &td = ts->data_pending;

	double scale_factor = window_state_get_scale_factor(ws);

	td.position.x = wl_fixed_to_double(x);
	td.position.y = wl_fixed_to_double(y);
	td.position *= scale_factor;

	td.motion_time = OS::get_singleton()->get_ticks_msec();
}

void WaylandThread::_wp_tablet_tool_on_pressure(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t pressure) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	ts->data_pending.pressure = pressure;
}

void WaylandThread::_wp_tablet_tool_on_distance(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t distance) {
	// Unsupported
}

void WaylandThread::_wp_tablet_tool_on_tilt(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, wl_fixed_t tilt_x, wl_fixed_t tilt_y) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	TabletToolData &td = ts->data_pending;

	td.tilt.x = wl_fixed_to_double(tilt_x);
	td.tilt.y = wl_fixed_to_double(tilt_y);
}

void WaylandThread::_wp_tablet_tool_on_rotation(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, wl_fixed_t degrees) {
	// Unsupported.
}

void WaylandThread::_wp_tablet_tool_on_slider(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, int32_t position) {
	// Unsupported.
}

void WaylandThread::_wp_tablet_tool_on_wheel(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, wl_fixed_t degrees, int32_t clicks) {
	// TODO
}

void WaylandThread::_wp_tablet_tool_on_button(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t serial, uint32_t button, uint32_t state) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	TabletToolData &td = ts->data_pending;

	MouseButton mouse_button = MouseButton::NONE;

	if (button == BTN_STYLUS) {
		mouse_button = MouseButton::LEFT;
	}

	if (button == BTN_STYLUS2) {
		mouse_button = MouseButton::RIGHT;
	}

	if (mouse_button != MouseButton::NONE) {
		MouseButtonMask mask = mouse_button_to_mask(mouse_button);

		if (state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED) {
			td.pressed_button_mask.set_flag(mask);
			td.last_button_pressed = mouse_button;
			td.double_click_begun = true;
		} else {
			td.pressed_button_mask.clear_flag(mask);
		}

		// The protocol doesn't cover this, but we can use this funky hack to make
		// double clicking work.
		td.button_time = OS::get_singleton()->get_ticks_msec();
	}
}

void WaylandThread::_wp_tablet_tool_on_frame(void *data, struct zwp_tablet_tool_v2 *wp_tablet_tool_v2, uint32_t time) {
	TabletToolState *ts = wp_tablet_tool_get_state(wp_tablet_tool_v2);
	if (!ts) {
		return;
	}

	SeatState *ss = wl_seat_get_seat_state(ts->wl_seat);
	if (!ss) {
		return;
	}

	WaylandThread *wayland_thread = ss->wayland_thread;
	ERR_FAIL_NULL(wayland_thread);

	TabletToolData &old_td = ts->data;
	TabletToolData &td = ts->data_pending;

	if (old_td.position != td.position || old_td.tilt != td.tilt || old_td.pressure != td.pressure) {
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();

		mm->set_window_id(DisplayServer::MAIN_WINDOW_ID);

		// Set all pressed modifiers.
		mm->set_shift_pressed(ss->shift_pressed);
		mm->set_ctrl_pressed(ss->ctrl_pressed);
		mm->set_alt_pressed(ss->alt_pressed);
		mm->set_meta_pressed(ss->meta_pressed);

		mm->set_button_mask(td.pressed_button_mask);

		mm->set_position(td.position);
		mm->set_global_position(td.position);

		// NOTE: The Godot API expects normalized values and we store them raw,
		// straight from the compositor, so we have to normalize them here.

		// According to the tablet proto spec, tilt is expressed in degrees relative
		// to the Z axis of the tablet, so it shouldn't go over 90 degrees either way,
		// I think. We'll clamp it just in case.
		td.tilt = td.tilt.clampf(-90, 90);

		mm->set_tilt(td.tilt / 90);

		// The tablet proto spec explicitly says that pressure is defined as a value
		// between 0 to 65535.
		mm->set_pressure(td.pressure / (float)65535);

		mm->set_pen_inverted(ts->is_eraser);

		mm->set_relative(td.position - old_td.position);
		mm->set_relative_screen_position(mm->get_relative());

		Vector2 pos_delta = td.position - old_td.position;
		uint32_t time_delta = td.motion_time - old_td.motion_time;
		mm->set_velocity((Vector2)pos_delta / time_delta);

		Ref<InputEventMessage> inputev_msg;
		inputev_msg.instantiate();

		inputev_msg->event = mm;

		wayland_thread->push_message(inputev_msg);
	}

	if (old_td.pressed_button_mask != td.pressed_button_mask) {
		BitField<MouseButtonMask> pressed_mask_delta = BitField<MouseButtonMask>((int64_t)old_td.pressed_button_mask ^ (int64_t)td.pressed_button_mask);

		for (MouseButton test_button : { MouseButton::LEFT, MouseButton::RIGHT }) {
			MouseButtonMask test_button_mask = mouse_button_to_mask(test_button);

			if (pressed_mask_delta.has_flag(test_button_mask)) {
				Ref<InputEventMouseButton> mb;
				mb.instantiate();

				// Set all pressed modifiers.
				mb->set_shift_pressed(ss->shift_pressed);
				mb->set_ctrl_pressed(ss->ctrl_pressed);
				mb->set_alt_pressed(ss->alt_pressed);
				mb->set_meta_pressed(ss->meta_pressed);

				mb->set_window_id(DisplayServer::MAIN_WINDOW_ID);
				mb->set_position(td.position);
				mb->set_global_position(td.position);

				mb->set_button_mask(td.pressed_button_mask);
				mb->set_button_index(test_button);
				mb->set_pressed(td.pressed_button_mask.has_flag(test_button_mask));

				// We have to set the last position pressed here as we can't take for
				// granted what the individual events might have seen due to them not having
				// a garaunteed order.
				if (mb->is_pressed()) {
					td.last_pressed_position = td.position;
				}

				if (old_td.double_click_begun && mb->is_pressed() && td.last_button_pressed == old_td.last_button_pressed && (td.button_time - old_td.button_time) < 400 && Vector2(td.last_pressed_position).distance_to(Vector2(old_td.last_pressed_position)) < 5) {
					td.double_click_begun = false;
					mb->set_double_click(true);
				}

				Ref<InputEventMessage> msg;
				msg.instantiate();

				msg->event = mb;

				wayland_thread->push_message(msg);
			}
		}
	}

	old_td = td;
}

void WaylandThread::_wp_text_input_on_enter(void *data, struct zwp_text_input_v3 *wp_text_input_v3, struct wl_surface *surface) {
	SeatState *ss = (SeatState *)data;
	if (!ss) {
		return;
	}

	ss->ime_enabled = true;
}

void WaylandThread::_wp_text_input_on_leave(void *data, struct zwp_text_input_v3 *wp_text_input_v3, struct wl_surface *surface) {
	SeatState *ss = (SeatState *)data;
	if (!ss) {
		return;
	}

	ss->ime_enabled = false;
	ss->ime_active = false;
	ss->ime_text = String();
	ss->ime_text_commit = String();
	ss->ime_cursor = Vector2i();

	Ref<IMEUpdateEventMessage> msg;
	msg.instantiate();
	msg->text = String();
	msg->selection = Vector2i();
	ss->wayland_thread->push_message(msg);
}

void WaylandThread::_wp_text_input_on_preedit_string(void *data, struct zwp_text_input_v3 *wp_text_input_v3, const char *text, int32_t cursor_begin, int32_t cursor_end) {
	SeatState *ss = (SeatState *)data;
	if (!ss) {
		return;
	}

	ss->ime_text = String::utf8(text);

	// Convert cursor positions from UTF-8 to UTF-32 offset.
	int32_t cursor_begin_utf32 = 0;
	int32_t cursor_end_utf32 = 0;
	for (int i = 0; i < ss->ime_text.length(); i++) {
		uint32_t c = ss->ime_text[i];
		if (c <= 0x7f) { // 7 bits.
			cursor_begin -= 1;
			cursor_end -= 1;
		} else if (c <= 0x7ff) { // 11 bits
			cursor_begin -= 2;
			cursor_end -= 2;
		} else if (c <= 0xffff) { // 16 bits
			cursor_begin -= 3;
			cursor_end -= 3;
		} else if (c <= 0x001fffff) { // 21 bits
			cursor_begin -= 4;
			cursor_end -= 4;
		} else if (c <= 0x03ffffff) { // 26 bits
			cursor_begin -= 5;
			cursor_end -= 5;
		} else if (c <= 0x7fffffff) { // 31 bits
			cursor_begin -= 6;
			cursor_end -= 6;
		} else {
			cursor_begin -= 1;
			cursor_end -= 1;
		}
		if (cursor_begin == 0) {
			cursor_begin_utf32 = i + 1;
		}
		if (cursor_end == 0) {
			cursor_end_utf32 = i + 1;
		}
		if (cursor_begin <= 0 && cursor_end <= 0) {
			break;
		}
	}
	ss->ime_cursor = Vector2i(cursor_begin_utf32, cursor_end_utf32 - cursor_begin_utf32);
}

void WaylandThread::_wp_text_input_on_commit_string(void *data, struct zwp_text_input_v3 *wp_text_input_v3, const char *text) {
	SeatState *ss = (SeatState *)data;
	if (!ss) {
		return;
	}

	ss->ime_text_commit = String::utf8(text);
}

void WaylandThread::_wp_text_input_on_delete_surrounding_text(void *data, struct zwp_text_input_v3 *wp_text_input_v3, uint32_t before_length, uint32_t after_length) {
	// Not implemented.
}

void WaylandThread::_wp_text_input_on_done(void *data, struct zwp_text_input_v3 *wp_text_input_v3, uint32_t serial) {
	SeatState *ss = (SeatState *)data;
	if (!ss) {
		return;
	}

	if (!ss->ime_text_commit.is_empty()) {
		Ref<IMECommitEventMessage> msg;
		msg.instantiate();
		msg->text = ss->ime_text_commit;
		ss->wayland_thread->push_message(msg);
	} else {
		Ref<IMEUpdateEventMessage> msg;
		msg.instantiate();
		msg->text = ss->ime_text;
		msg->selection = ss->ime_cursor;
		ss->wayland_thread->push_message(msg);
	}
	ss->ime_text = String();
	ss->ime_text_commit = String();
	ss->ime_cursor = Vector2i();
}

void WaylandThread::_xdg_activation_token_on_done(void *data, struct xdg_activation_token_v1 *xdg_activation_token, const char *token) {
	WindowState *ws = (WindowState *)data;
	ERR_FAIL_NULL(ws);
	ERR_FAIL_NULL(ws->wayland_thread);
	ERR_FAIL_NULL(ws->wl_surface);

	xdg_activation_v1_activate(ws->wayland_thread->registry.xdg_activation, token, ws->wl_surface);
	xdg_activation_token_v1_destroy(xdg_activation_token);

	DEBUG_LOG_WAYLAND_THREAD(vformat("Received activation token and requested window activation."));
}

// NOTE: This must be started after a valid wl_display is loaded.
void WaylandThread::_poll_events_thread(void *p_data) {
	ThreadData *data = (ThreadData *)p_data;
	ERR_FAIL_NULL(data);
	ERR_FAIL_NULL(data->wl_display);

	struct pollfd poll_fd;
	poll_fd.fd = wl_display_get_fd(data->wl_display);
	poll_fd.events = POLLIN | POLLHUP;

	while (true) {
		// Empty the event queue while it's full.
		while (wl_display_prepare_read(data->wl_display) != 0) {
			// We aren't using wl_display_dispatch(), instead "manually" handling events
			// through wl_display_dispatch_pending so that we can use a global mutex and
			// be sure that this and the main thread won't race over stuff, as long as
			// the main thread locks it too.
			//
			// Note that the main thread can still call wl_display_roundtrip as that
			// method directly handles all events, effectively bypassing this polling
			// loop and thus the mutex locking, avoiding a deadlock.
			MutexLock mutex_lock(data->mutex);

			if (wl_display_dispatch_pending(data->wl_display) == -1) {
				// Oh no. We'll check and handle any display error below.
				break;
			}
		}

		int werror = wl_display_get_error(data->wl_display);

		if (werror) {
			if (werror == EPROTO) {
				struct wl_interface *wl_interface = nullptr;
				uint32_t id = 0;

				int error_code = wl_display_get_protocol_error(data->wl_display, (const struct wl_interface **)&wl_interface, &id);
				CRASH_NOW_MSG(vformat("Wayland protocol error %d on interface %s@%d.", error_code, wl_interface ? wl_interface->name : "unknown", id));
			} else {
				CRASH_NOW_MSG(vformat("Wayland client error code %d.", werror));
			}
		}

		wl_display_flush(data->wl_display);

		// Wait for the event file descriptor to have new data.
		poll(&poll_fd, 1, -1);

		if (data->thread_done.is_set()) {
			wl_display_cancel_read(data->wl_display);
			break;
		}

		if (poll_fd.revents | POLLIN) {
			// Load the queues with fresh new data.
			wl_display_read_events(data->wl_display);
		} else {
			// Oh well... Stop signaling that we want to read.
			wl_display_cancel_read(data->wl_display);
		}

		// The docs advise to redispatch unconditionally and it looks like that if we
		// don't do this we can't catch protocol errors, which is bad.
		MutexLock mutex_lock(data->mutex);
		wl_display_dispatch_pending(data->wl_display);
	}
}

struct wl_display *WaylandThread::get_wl_display() const {
	return wl_display;
}

// NOTE: Stuff like libdecor can (and will) register foreign proxies which
// aren't formatted as we like. This method is needed to detect whether a proxy
// has our tag. Also, be careful! The proxy has to be manually tagged or it
// won't be recognized.
bool WaylandThread::wl_proxy_is_godot(struct wl_proxy *p_proxy) {
	ERR_FAIL_NULL_V(p_proxy, false);

	return wl_proxy_get_tag(p_proxy) == &proxy_tag;
}

void WaylandThread::wl_proxy_tag_godot(struct wl_proxy *p_proxy) {
	ERR_FAIL_NULL(p_proxy);

	wl_proxy_set_tag(p_proxy, &proxy_tag);
}

// Returns the wl_surface's `WindowState`, otherwise `nullptr`.
// NOTE: This will fail if the surface isn't tagged as ours.
WaylandThread::WindowState *WaylandThread::wl_surface_get_window_state(struct wl_surface *p_surface) {
	if (p_surface && wl_proxy_is_godot((wl_proxy *)p_surface)) {
		return (WindowState *)wl_surface_get_user_data(p_surface);
	}

	return nullptr;
}

// Returns the wl_outputs's `ScreenState`, otherwise `nullptr`.
// NOTE: This will fail if the output isn't tagged as ours.
WaylandThread::ScreenState *WaylandThread::wl_output_get_screen_state(struct wl_output *p_output) {
	if (p_output && wl_proxy_is_godot((wl_proxy *)p_output)) {
		return (ScreenState *)wl_output_get_user_data(p_output);
	}

	return nullptr;
}

// Returns the wl_seat's `SeatState`, otherwise `nullptr`.
// NOTE: This will fail if the output isn't tagged as ours.
WaylandThread::SeatState *WaylandThread::wl_seat_get_seat_state(struct wl_seat *p_seat) {
	if (p_seat && wl_proxy_is_godot((wl_proxy *)p_seat)) {
		return (SeatState *)wl_seat_get_user_data(p_seat);
	}

	return nullptr;
}

// Returns the wp_tablet_tool's `TabletToolState`, otherwise `nullptr`.
// NOTE: This will fail if the output isn't tagged as ours.
WaylandThread::TabletToolState *WaylandThread::wp_tablet_tool_get_state(struct zwp_tablet_tool_v2 *p_tool) {
	if (p_tool && wl_proxy_is_godot((wl_proxy *)p_tool)) {
		return (TabletToolState *)zwp_tablet_tool_v2_get_user_data(p_tool);
	}

	return nullptr;
}
// Returns the wl_data_offer's `OfferState`, otherwise `nullptr`.
// NOTE: This will fail if the output isn't tagged as ours.
WaylandThread::OfferState *WaylandThread::wl_data_offer_get_offer_state(struct wl_data_offer *p_offer) {
	if (p_offer && wl_proxy_is_godot((wl_proxy *)p_offer)) {
		return (OfferState *)wl_data_offer_get_user_data(p_offer);
	}

	return nullptr;
}

// Returns the wl_data_offer's `OfferState`, otherwise `nullptr`.
// NOTE: This will fail if the output isn't tagged as ours.
WaylandThread::OfferState *WaylandThread::wp_primary_selection_offer_get_offer_state(struct zwp_primary_selection_offer_v1 *p_offer) {
	if (p_offer && wl_proxy_is_godot((wl_proxy *)p_offer)) {
		return (OfferState *)zwp_primary_selection_offer_v1_get_user_data(p_offer);
	}

	return nullptr;
}

// This is implemented as a method because this is the simplest way of
// accounting for dynamic output scale changes.
int WaylandThread::window_state_get_preferred_buffer_scale(WindowState *p_ws) {
	ERR_FAIL_NULL_V(p_ws, 1);

	if (p_ws->preferred_fractional_scale > 0) {
		// We're scaling fractionally. Per spec, the buffer scale is always 1.
		return 1;
	}

	if (p_ws->wl_outputs.is_empty()) {
		DEBUG_LOG_WAYLAND_THREAD("Window has no output associated, returning buffer scale of 1.");
		return 1;
	}

	// TODO: Cache value?
	int max_size = 1;

	// ================================ IMPORTANT =================================
	// NOTE: Due to a Godot limitation, we can't really rescale the whole UI yet.
	// Because of this reason, all platforms have resorted to forcing the highest
	// scale possible of a system on any window, despite of what screen it's onto.
	// On this backend everything's already in place for dynamic window scale
	// handling, but in the meantime we'll just select the biggest _global_ output.
	// To restore dynamic scale selection, simply iterate over `p_ws->wl_outputs`
	// instead.
	for (struct wl_output *wl_output : p_ws->registry->wl_outputs) {
		ScreenState *ss = wl_output_get_screen_state(wl_output);

		if (ss && ss->pending_data.scale > max_size) {
			// NOTE: For some mystical reason, wl_output.done is emitted _after_ windows
			// get resized but the scale event gets sent _before_ that. I'm still leaning
			// towards the idea that rescaling when a window gets a resolution change is a
			// pretty good approach, but this means that we'll have to use the screen data
			// before it's "committed".
			// FIXME: Use the committed data. Somehow.
			max_size = ss->pending_data.scale;
		}
	}

	return max_size;
}

double WaylandThread::window_state_get_scale_factor(WindowState *p_ws) {
	ERR_FAIL_NULL_V(p_ws, 1);

	if (p_ws->fractional_scale > 0) {
		// The fractional scale amount takes priority.
		return p_ws->fractional_scale;
	}

	return p_ws->buffer_scale;
}

void WaylandThread::window_state_update_size(WindowState *p_ws, int p_width, int p_height) {
	ERR_FAIL_NULL(p_ws);

	int preferred_buffer_scale = window_state_get_preferred_buffer_scale(p_ws);
	bool using_fractional = p_ws->preferred_fractional_scale > 0;

	// If neither is true we no-op.
	bool scale_changed = false;
	bool size_changed = false;

	if (p_ws->rect.size.width != p_width || p_ws->rect.size.height != p_height) {
		p_ws->rect.size.width = p_width;
		p_ws->rect.size.height = p_height;

		size_changed = true;
	}

	if (using_fractional && p_ws->fractional_scale != p_ws->preferred_fractional_scale) {
		p_ws->fractional_scale = p_ws->preferred_fractional_scale;
		scale_changed = true;
	}

	if (p_ws->buffer_scale != preferred_buffer_scale) {
		// The buffer scale is always important, even if we use frac scaling.
		p_ws->buffer_scale = preferred_buffer_scale;
		p_ws->buffer_scale_changed = true;

		if (!using_fractional) {
			// We don't bother updating everything else if it's turned on though.
			scale_changed = true;
		}
	}

	if (p_ws->wl_surface && (size_changed || scale_changed)) {
		if (p_ws->wp_viewport) {
			wp_viewport_set_destination(p_ws->wp_viewport, p_width, p_height);
		}

		if (p_ws->xdg_surface) {
			xdg_surface_set_window_geometry(p_ws->xdg_surface, 0, 0, p_width, p_height);
		}
	}

#ifdef LIBDECOR_ENABLED
	if (p_ws->libdecor_frame) {
		struct libdecor_state *state = libdecor_state_new(p_width, p_height);
		libdecor_frame_commit(p_ws->libdecor_frame, state, p_ws->pending_libdecor_configuration);
		libdecor_state_free(state);
		p_ws->pending_libdecor_configuration = nullptr;
	}
#endif

	if (size_changed || scale_changed) {
		Size2i scaled_size = scale_vector2i(p_ws->rect.size, window_state_get_scale_factor(p_ws));

		if (using_fractional) {
			DEBUG_LOG_WAYLAND_THREAD(vformat("Resizing the window from %s to %s (fractional scale x%f).", p_ws->rect.size, scaled_size, p_ws->fractional_scale));
		} else {
			DEBUG_LOG_WAYLAND_THREAD(vformat("Resizing the window from %s to %s (buffer scale x%d).", p_ws->rect.size, scaled_size, p_ws->buffer_scale));
		}

		// FIXME: Actually resize the hint instead of centering it.
		p_ws->wayland_thread->pointer_set_hint(scaled_size / 2);

		Ref<WindowRectMessage> rect_msg;
		rect_msg.instantiate();
		rect_msg->rect = p_ws->rect;
		rect_msg->rect.size = scaled_size;
		p_ws->wayland_thread->push_message(rect_msg);
	}

	if (scale_changed) {
		Ref<WindowEventMessage> dpi_msg;
		dpi_msg.instantiate();
		dpi_msg->event = DisplayServer::WINDOW_EVENT_DPI_CHANGE;
		p_ws->wayland_thread->push_message(dpi_msg);
	}
}

// Scales a vector according to wp_fractional_scale's rules, where coordinates
// must be scaled with away from zero half-rounding.
Vector2i WaylandThread::scale_vector2i(const Vector2i &p_vector, double p_amount) {
	// This snippet is tiny, I know, but this is done a lot.
	int x = round(p_vector.x * p_amount);
	int y = round(p_vector.y * p_amount);

	return Vector2i(x, y);
}

void WaylandThread::seat_state_unlock_pointer(SeatState *p_ss) {
	ERR_FAIL_NULL(p_ss);

	if (p_ss->wl_pointer == nullptr) {
		return;
	}

	if (p_ss->wp_locked_pointer) {
		zwp_locked_pointer_v1_destroy(p_ss->wp_locked_pointer);
		p_ss->wp_locked_pointer = nullptr;
	}

	if (p_ss->wp_confined_pointer) {
		zwp_confined_pointer_v1_destroy(p_ss->wp_confined_pointer);
		p_ss->wp_confined_pointer = nullptr;
	}
}

void WaylandThread::seat_state_lock_pointer(SeatState *p_ss) {
	ERR_FAIL_NULL(p_ss);

	if (p_ss->wl_pointer == nullptr) {
		return;
	}

	if (registry.wp_pointer_constraints == nullptr) {
		return;
	}

	if (p_ss->wp_locked_pointer == nullptr) {
		struct wl_surface *locked_surface = p_ss->last_pointed_surface;

		if (locked_surface == nullptr) {
			locked_surface = window_get_wl_surface(DisplayServer::MAIN_WINDOW_ID);
		}

		ERR_FAIL_NULL(locked_surface);

		p_ss->wp_locked_pointer = zwp_pointer_constraints_v1_lock_pointer(registry.wp_pointer_constraints, locked_surface, p_ss->wl_pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	}
}

void WaylandThread::seat_state_set_hint(SeatState *p_ss, int p_x, int p_y) {
	if (p_ss->wp_locked_pointer == nullptr) {
		return;
	}

	zwp_locked_pointer_v1_set_cursor_position_hint(p_ss->wp_locked_pointer, wl_fixed_from_int(p_x), wl_fixed_from_int(p_y));
}

void WaylandThread::seat_state_confine_pointer(SeatState *p_ss) {
	ERR_FAIL_NULL(p_ss);

	if (p_ss->wl_pointer == nullptr) {
		return;
	}

	if (registry.wp_pointer_constraints == nullptr) {
		return;
	}

	if (p_ss->wp_confined_pointer == nullptr) {
		struct wl_surface *confined_surface = p_ss->last_pointed_surface;

		if (confined_surface == nullptr) {
			confined_surface = window_get_wl_surface(DisplayServer::MAIN_WINDOW_ID);
		}

		ERR_FAIL_NULL(confined_surface);

		p_ss->wp_confined_pointer = zwp_pointer_constraints_v1_confine_pointer(registry.wp_pointer_constraints, confined_surface, p_ss->wl_pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	}
}

void WaylandThread::seat_state_update_cursor(SeatState *p_ss) {
	ERR_FAIL_NULL(p_ss);

	WaylandThread *thread = p_ss->wayland_thread;
	ERR_FAIL_NULL(p_ss->wayland_thread);

	if (!p_ss->wl_pointer || !p_ss->cursor_surface) {
		return;
	}

	// NOTE: Those values are valid by default and will hide the cursor when
	// unchanged.
	struct wl_buffer *cursor_buffer = nullptr;
	uint32_t hotspot_x = 0;
	uint32_t hotspot_y = 0;
	int scale = 1;

	if (thread->cursor_visible) {
		DisplayServer::CursorShape shape = thread->cursor_shape;

		struct CustomCursor *custom_cursor = thread->custom_cursors.getptr(shape);

		if (custom_cursor) {
			cursor_buffer = custom_cursor->wl_buffer;
			hotspot_x = custom_cursor->hotspot.x;
			hotspot_y = custom_cursor->hotspot.y;

			// We can't really reasonably scale custom cursors, so we'll let the
			// compositor do it for us (badly).
			scale = 1;
		} else {
			struct wl_cursor *wl_cursor = thread->wl_cursors[shape];

			if (!wl_cursor) {
				return;
			}

			int frame_idx = 0;

			if (wl_cursor->image_count > 1) {
				// The cursor is animated.
				frame_idx = wl_cursor_frame(wl_cursor, p_ss->cursor_time_ms);

				if (!p_ss->cursor_frame_callback) {
					// Since it's animated, we'll re-update it the next frame.
					p_ss->cursor_frame_callback = wl_surface_frame(p_ss->cursor_surface);
					wl_callback_add_listener(p_ss->cursor_frame_callback, &cursor_frame_callback_listener, p_ss);
				}
			}

			struct wl_cursor_image *wl_cursor_image = wl_cursor->images[frame_idx];

			scale = thread->cursor_scale;

			cursor_buffer = wl_cursor_image_get_buffer(wl_cursor_image);

			// As the surface's buffer is scaled (thus the surface is smaller) and the
			// hotspot must be expressed in surface-local coordinates, we need to scale
			// it down accordingly.
			hotspot_x = wl_cursor_image->hotspot_x / scale;
			hotspot_y = wl_cursor_image->hotspot_y / scale;
		}
	}

	wl_pointer_set_cursor(p_ss->wl_pointer, p_ss->pointer_enter_serial, p_ss->cursor_surface, hotspot_x, hotspot_y);
	wl_surface_set_buffer_scale(p_ss->cursor_surface, scale);
	wl_surface_attach(p_ss->cursor_surface, cursor_buffer, 0, 0);
	wl_surface_damage_buffer(p_ss->cursor_surface, 0, 0, INT_MAX, INT_MAX);

	wl_surface_commit(p_ss->cursor_surface);
}

void WaylandThread::seat_state_echo_keys(SeatState *p_ss) {
	ERR_FAIL_NULL(p_ss);

	if (p_ss->wl_keyboard == nullptr) {
		return;
	}

	// TODO: Comment and document out properly this block of code.
	// In short, this implements key repeating.
	if (p_ss->repeat_key_delay_msec && p_ss->repeating_keycode != XKB_KEYCODE_INVALID) {
		uint64_t current_ticks = OS::get_singleton()->get_ticks_msec();
		uint64_t delayed_start_ticks = p_ss->last_repeat_start_msec + p_ss->repeat_start_delay_msec;

		if (p_ss->last_repeat_msec < delayed_start_ticks) {
			p_ss->last_repeat_msec = delayed_start_ticks;
		}

		if (current_ticks >= delayed_start_ticks) {
			uint64_t ticks_delta = current_ticks - p_ss->last_repeat_msec;

			int keys_amount = (ticks_delta / p_ss->repeat_key_delay_msec);

			for (int i = 0; i < keys_amount; i++) {
				Ref<InputEventKey> k;
				k.instantiate();

				if (!_seat_state_configure_key_event(*p_ss, k, p_ss->repeating_keycode, true)) {
					continue;
				}

				k->set_echo(true);

				Input::get_singleton()->parse_input_event(k);
			}

			p_ss->last_repeat_msec += ticks_delta - (ticks_delta % p_ss->repeat_key_delay_msec);
		}
	}
}

void WaylandThread::push_message(Ref<Message> message) {
	messages.push_back(message);
}

bool WaylandThread::has_message() {
	return messages.front() != nullptr;
}

Ref<WaylandThread::Message> WaylandThread::pop_message() {
	if (messages.front() != nullptr) {
		Ref<Message> msg = messages.front()->get();
		messages.pop_front();
		return msg;
	}

	// This method should only be called if `has_messages` returns true but if
	// that isn't the case we'll just return an invalid `Ref`. After all, due to
	// its `InputEvent`-like interface, we still have to dynamically cast and check
	// the `Ref`'s validity anyways.
	return Ref<Message>();
}

void WaylandThread::window_create(DisplayServer::WindowID p_window_id, int p_width, int p_height) {
	// TODO: Implement multi-window support.
	WindowState &ws = main_window;

	ws.registry = &registry;
	ws.wayland_thread = this;

	ws.rect.size.width = p_width;
	ws.rect.size.height = p_height;

	ws.wl_surface = wl_compositor_create_surface(registry.wl_compositor);
	wl_proxy_tag_godot((struct wl_proxy *)ws.wl_surface);
	wl_surface_add_listener(ws.wl_surface, &wl_surface_listener, &ws);

	if (registry.wp_viewporter) {
		ws.wp_viewport = wp_viewporter_get_viewport(registry.wp_viewporter, ws.wl_surface);

		if (registry.wp_fractional_scale_manager) {
			ws.wp_fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(registry.wp_fractional_scale_manager, ws.wl_surface);
			wp_fractional_scale_v1_add_listener(ws.wp_fractional_scale, &wp_fractional_scale_listener, &ws);
		}
	}

	bool decorated = false;

#ifdef LIBDECOR_ENABLED
	if (!decorated && libdecor_context) {
		ws.libdecor_frame = libdecor_decorate(libdecor_context, ws.wl_surface, (struct libdecor_frame_interface *)&libdecor_frame_interface, &ws);
		libdecor_frame_map(ws.libdecor_frame);

		decorated = true;
	}
#endif

	if (!decorated) {
		// libdecor has failed loading or is disabled, we shall handle xdg_toplevel
		// creation and decoration ourselves (and by decorating for now I just mean
		// asking for SSDs and hoping for the best).
		ws.xdg_surface = xdg_wm_base_get_xdg_surface(registry.xdg_wm_base, ws.wl_surface);
		xdg_surface_add_listener(ws.xdg_surface, &xdg_surface_listener, &ws);

		ws.xdg_toplevel = xdg_surface_get_toplevel(ws.xdg_surface);
		xdg_toplevel_add_listener(ws.xdg_toplevel, &xdg_toplevel_listener, &ws);

		if (registry.xdg_decoration_manager) {
			ws.xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(registry.xdg_decoration_manager, ws.xdg_toplevel);
			zxdg_toplevel_decoration_v1_add_listener(ws.xdg_toplevel_decoration, &xdg_toplevel_decoration_listener, &ws);

			decorated = true;
		}
	}

	ws.frame_callback = wl_surface_frame(ws.wl_surface);
	wl_callback_add_listener(ws.frame_callback, &frame_wl_callback_listener, &ws);

	if (registry.xdg_exporter_v2) {
		ws.xdg_exported_v2 = zxdg_exporter_v2_export_toplevel(registry.xdg_exporter_v2, ws.wl_surface);
		zxdg_exported_v2_add_listener(ws.xdg_exported_v2, &xdg_exported_v2_listener, &ws);
	} else if (registry.xdg_exporter_v1) {
		ws.xdg_exported_v1 = zxdg_exporter_v1_export(registry.xdg_exporter_v1, ws.wl_surface);
		zxdg_exported_v1_add_listener(ws.xdg_exported_v1, &xdg_exported_v1_listener, &ws);
	}

	wl_surface_commit(ws.wl_surface);

	// Wait for the surface to be configured before continuing.
	wl_display_roundtrip(wl_display);
}

struct wl_surface *WaylandThread::window_get_wl_surface(DisplayServer::WindowID p_window_id) const {
	// TODO: Use window IDs for multiwindow support.
	const WindowState &ws = main_window;

	return ws.wl_surface;
}

void WaylandThread::beep() const {
	if (registry.xdg_system_bell) {
		xdg_system_bell_v1_ring(registry.xdg_system_bell, nullptr);
	}
}

void WaylandThread::window_start_drag(DisplayServer::WindowID p_window_id) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ws.xdg_toplevel) {
		xdg_toplevel_move(ws.xdg_toplevel, ss->wl_seat, ss->pointer_data.button_serial);
	}

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_frame_move(ws.libdecor_frame, ss->wl_seat, ss->pointer_data.button_serial);
	}
#endif
}

void WaylandThread::window_start_resize(DisplayServer::WindowResizeEdge p_edge, DisplayServer::WindowID p_window) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ws.xdg_toplevel) {
		xdg_toplevel_resize_edge edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
		switch (p_edge) {
			case DisplayServer::WINDOW_EDGE_TOP_LEFT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_TOP: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
			} break;
			case DisplayServer::WINDOW_EDGE_TOP_RIGHT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
			} break;
			case DisplayServer::WINDOW_EDGE_LEFT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_RIGHT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_LEFT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT: {
				edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
			} break;
			default:
				break;
		}
		xdg_toplevel_resize(ws.xdg_toplevel, ss->wl_seat, ss->pointer_data.button_serial, edge);
	}

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_resize_edge edge = LIBDECOR_RESIZE_EDGE_NONE;
		switch (p_edge) {
			case DisplayServer::WINDOW_EDGE_TOP_LEFT: {
				edge = LIBDECOR_RESIZE_EDGE_TOP_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_TOP: {
				edge = LIBDECOR_RESIZE_EDGE_TOP;
			} break;
			case DisplayServer::WINDOW_EDGE_TOP_RIGHT: {
				edge = LIBDECOR_RESIZE_EDGE_TOP_RIGHT;
			} break;
			case DisplayServer::WINDOW_EDGE_LEFT: {
				edge = LIBDECOR_RESIZE_EDGE_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_RIGHT: {
				edge = LIBDECOR_RESIZE_EDGE_RIGHT;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_LEFT: {
				edge = LIBDECOR_RESIZE_EDGE_BOTTOM_LEFT;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM: {
				edge = LIBDECOR_RESIZE_EDGE_BOTTOM;
			} break;
			case DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT: {
				edge = LIBDECOR_RESIZE_EDGE_BOTTOM_RIGHT;
			} break;
			default:
				break;
		}
		libdecor_frame_resize(ws.libdecor_frame, ss->wl_seat, ss->pointer_data.button_serial, edge);
	}
#endif
}

void WaylandThread::window_set_max_size(DisplayServer::WindowID p_window_id, const Size2i &p_size) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	Vector2i logical_max_size = p_size / window_state_get_scale_factor(&ws);

	if (ws.wl_surface && ws.xdg_toplevel) {
		xdg_toplevel_set_max_size(ws.xdg_toplevel, logical_max_size.width, logical_max_size.height);
	}

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_frame_set_max_content_size(ws.libdecor_frame, logical_max_size.width, logical_max_size.height);
	}

	// FIXME: I'm not sure whether we have to commit the surface for this to apply.
#endif
}

void WaylandThread::window_set_min_size(DisplayServer::WindowID p_window_id, const Size2i &p_size) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	Size2i logical_min_size = p_size / window_state_get_scale_factor(&ws);

	if (ws.wl_surface && ws.xdg_toplevel) {
		xdg_toplevel_set_min_size(ws.xdg_toplevel, logical_min_size.width, logical_min_size.height);
	}

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_frame_set_min_content_size(ws.libdecor_frame, logical_min_size.width, logical_min_size.height);
	}

	// FIXME: I'm not sure whether we have to commit the surface for this to apply.
#endif
}

bool WaylandThread::window_can_set_mode(DisplayServer::WindowID p_window_id, DisplayServer::WindowMode p_window_mode) const {
	// TODO: Use window IDs for multiwindow support.
	const WindowState &ws = main_window;

	switch (p_window_mode) {
		case DisplayServer::WINDOW_MODE_WINDOWED: {
			// Looks like it's guaranteed.
			return true;
		};

		case DisplayServer::WINDOW_MODE_MINIMIZED: {
#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				return libdecor_frame_has_capability(ws.libdecor_frame, LIBDECOR_ACTION_MINIMIZE);
			}
#endif // LIBDECOR_ENABLED

			return ws.can_minimize;
		};

		case DisplayServer::WINDOW_MODE_MAXIMIZED: {
			// NOTE: libdecor doesn't seem to have a maximize capability query?
			// The fact that there's a fullscreen one makes me suspicious.
			return ws.can_maximize;
		};

		case DisplayServer::WINDOW_MODE_FULLSCREEN: {
#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				return libdecor_frame_has_capability(ws.libdecor_frame, LIBDECOR_ACTION_FULLSCREEN);
			}
#endif // LIBDECOR_ENABLED

			return ws.can_fullscreen;
		};

		case DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN: {
			// I'm not really sure but from what I can find Wayland doesn't really have
			// the concept of exclusive fullscreen.
			// TODO: Discuss whether to fallback to regular fullscreen or not.
			return false;
		};
	}

	return false;
}

void WaylandThread::window_try_set_mode(DisplayServer::WindowID p_window_id, DisplayServer::WindowMode p_window_mode) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	if (ws.mode == p_window_mode) {
		return;
	}

	// Don't waste time with hidden windows and whatnot. Behave like it worked.
#ifdef LIBDECOR_ENABLED
	if ((!ws.wl_surface || !ws.xdg_toplevel) && !ws.libdecor_frame) {
#else
	if (!ws.wl_surface || !ws.xdg_toplevel) {
#endif // LIBDECOR_ENABLED
		ws.mode = p_window_mode;
		return;
	}

	// Return back to a windowed state so that we can apply what the user asked.
	switch (ws.mode) {
		case DisplayServer::WINDOW_MODE_WINDOWED: {
			// Do nothing.
		} break;

		case DisplayServer::WINDOW_MODE_MINIMIZED: {
			// We can't do much according to the xdg_shell protocol. I have no idea
			// whether this implies that we should return or who knows what. For now
			// we'll do nothing.
			// TODO: Test this properly.
		} break;

		case DisplayServer::WINDOW_MODE_MAXIMIZED: {
			// Try to unmaximize. This isn't garaunteed to work actually, so we'll have
			// to check whether something changed.
			if (ws.xdg_toplevel) {
				xdg_toplevel_unset_maximized(ws.xdg_toplevel);
			}

#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				libdecor_frame_unset_maximized(ws.libdecor_frame);
			}
#endif // LIBDECOR_ENABLED
		} break;

		case DisplayServer::WINDOW_MODE_FULLSCREEN:
		case DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN: {
			// Same thing as above, unset fullscreen and check later if it worked.
			if (ws.xdg_toplevel) {
				xdg_toplevel_unset_fullscreen(ws.xdg_toplevel);
			}

#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				libdecor_frame_unset_fullscreen(ws.libdecor_frame);
			}
#endif // LIBDECOR_ENABLED
		} break;
	}

	// Wait for a configure event and hope that something changed.
	wl_display_roundtrip(wl_display);

	if (ws.mode != DisplayServer::WINDOW_MODE_WINDOWED) {
		// The compositor refused our "normalization" request. It'd be useless or
		// unpredictable to attempt setting a new state. We're done.
		return;
	}

	// Ask the compositor to set the state indicated by the new mode.
	switch (p_window_mode) {
		case DisplayServer::WINDOW_MODE_WINDOWED: {
			// Do nothing. We're already windowed.
		} break;

		case DisplayServer::WINDOW_MODE_MINIMIZED: {
			if (!window_can_set_mode(p_window_id, p_window_mode)) {
				// Minimization is special (read below). Better not mess with it if the
				// compositor explicitly announces that it doesn't support it.
				break;
			}

			if (ws.xdg_toplevel) {
				xdg_toplevel_set_minimized(ws.xdg_toplevel);
			}

#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				libdecor_frame_set_minimized(ws.libdecor_frame);
			}
#endif // LIBDECOR_ENABLED
	   // We have no way to actually detect this state, so we'll have to report it
	   // manually to the engine (hoping that it worked). In the worst case it'll
	   // get reset by the next configure event.
			ws.mode = DisplayServer::WINDOW_MODE_MINIMIZED;
		} break;

		case DisplayServer::WINDOW_MODE_MAXIMIZED: {
			if (ws.xdg_toplevel) {
				xdg_toplevel_set_maximized(ws.xdg_toplevel);
			}

#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				libdecor_frame_set_maximized(ws.libdecor_frame);
			}
#endif // LIBDECOR_ENABLED
		} break;

		case DisplayServer::WINDOW_MODE_FULLSCREEN:
		case DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN: {
			if (ws.xdg_toplevel) {
				xdg_toplevel_set_fullscreen(ws.xdg_toplevel, nullptr);
			}

#ifdef LIBDECOR_ENABLED
			if (ws.libdecor_frame) {
				libdecor_frame_set_fullscreen(ws.libdecor_frame, nullptr);
			}
#endif // LIBDECOR_ENABLED
		} break;

		default: {
		} break;
	}
}

void WaylandThread::window_set_borderless(DisplayServer::WindowID p_window_id, bool p_borderless) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	if (ws.xdg_toplevel_decoration) {
		if (p_borderless) {
			// We implement borderless windows by simply asking the compositor to let
			// us handle decorations (we don't).
			zxdg_toplevel_decoration_v1_set_mode(ws.xdg_toplevel_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
		} else {
			zxdg_toplevel_decoration_v1_set_mode(ws.xdg_toplevel_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
	}

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		bool visible_current = libdecor_frame_is_visible(ws.libdecor_frame);
		bool visible_target = !p_borderless;

		// NOTE: We have to do this otherwise we trip on a libdecor bug where it's
		// possible to destroy the frame more than once, by setting the visibility
		// to false multiple times and thus crashing.
		if (visible_current != visible_target) {
			print_verbose(vformat("Setting libdecor frame visibility to %d", visible_target));
			libdecor_frame_set_visibility(ws.libdecor_frame, visible_target);
		}
	}
#endif // LIBDECOR_ENABLED
}

void WaylandThread::window_set_title(DisplayServer::WindowID p_window_id, const String &p_title) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_frame_set_title(ws.libdecor_frame, p_title.utf8());
	}
#endif // LIBDECOR_ENABLE

	if (ws.xdg_toplevel) {
		xdg_toplevel_set_title(ws.xdg_toplevel, p_title.utf8());
	}
}

void WaylandThread::window_set_app_id(DisplayServer::WindowID p_window_id, const String &p_app_id) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

#ifdef LIBDECOR_ENABLED
	if (ws.libdecor_frame) {
		libdecor_frame_set_app_id(ws.libdecor_frame, p_app_id.utf8());
		return;
	}
#endif // LIBDECOR_ENABLED

	if (ws.xdg_toplevel) {
		xdg_toplevel_set_app_id(ws.xdg_toplevel, p_app_id.utf8());
		return;
	}
}

DisplayServer::WindowMode WaylandThread::window_get_mode(DisplayServer::WindowID p_window_id) const {
	// TODO: Use window IDs for multiwindow support.
	const WindowState &ws = main_window;

	return ws.mode;
}

void WaylandThread::window_request_attention(DisplayServer::WindowID p_window_id) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	if (registry.xdg_activation) {
		// Window attention requests are done through the XDG activation protocol.
		xdg_activation_token_v1 *xdg_activation_token = xdg_activation_v1_get_activation_token(registry.xdg_activation);
		xdg_activation_token_v1_add_listener(xdg_activation_token, &xdg_activation_token_listener, &ws);
		xdg_activation_token_v1_commit(xdg_activation_token);
	}
}

void WaylandThread::window_set_idle_inhibition(DisplayServer::WindowID p_window_id, bool p_enable) {
	// TODO: Use window IDs for multiwindow support.
	WindowState &ws = main_window;

	if (p_enable) {
		if (ws.registry->wp_idle_inhibit_manager && !ws.wp_idle_inhibitor) {
			ERR_FAIL_NULL(ws.wl_surface);
			ws.wp_idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(ws.registry->wp_idle_inhibit_manager, ws.wl_surface);
		}
	} else {
		if (ws.wp_idle_inhibitor) {
			zwp_idle_inhibitor_v1_destroy(ws.wp_idle_inhibitor);
			ws.wp_idle_inhibitor = nullptr;
		}
	}
}

bool WaylandThread::window_get_idle_inhibition(DisplayServer::WindowID p_window_id) const {
	// TODO: Use window IDs for multiwindow support.
	const WindowState &ws = main_window;

	return ws.wp_idle_inhibitor != nullptr;
}

WaylandThread::ScreenData WaylandThread::screen_get_data(int p_screen) const {
	ERR_FAIL_INDEX_V(p_screen, registry.wl_outputs.size(), ScreenData());

	return wl_output_get_screen_state(registry.wl_outputs.get(p_screen))->data;
}

int WaylandThread::get_screen_count() const {
	return registry.wl_outputs.size();
}

DisplayServer::WindowID WaylandThread::pointer_get_pointed_window_id() const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		WindowState *ws = wl_surface_get_window_state(ss->pointed_surface);

		if (ws) {
			return ws->id;
		}
	}

	return DisplayServer::INVALID_WINDOW_ID;
}

void WaylandThread::pointer_set_constraint(PointerConstraint p_constraint) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		seat_state_unlock_pointer(ss);

		if (p_constraint == PointerConstraint::LOCKED) {
			seat_state_lock_pointer(ss);
		} else if (p_constraint == PointerConstraint::CONFINED) {
			seat_state_confine_pointer(ss);
		}
	}

	pointer_constraint = p_constraint;
}

void WaylandThread::pointer_set_hint(const Point2i &p_hint) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);
	if (!ss) {
		return;
	}

	WindowState *ws = wl_surface_get_window_state(ss->pointed_surface);

	int hint_x = 0;
	int hint_y = 0;

	if (ws) {
		// NOTE: It looks like it's not really recommended to convert from
		// "godot-space" to "wayland-space" and in general I received mixed feelings
		// discussing about this. I'm not really sure about the maths behind this but,
		// oh well, we're setting a cursor hint. ¯\_(ツ)_/¯
		// See: https://oftc.irclog.whitequark.org/wayland/2023-08-23#1692756914-1692816818
		hint_x = round(p_hint.x / window_state_get_scale_factor(ws));
		hint_y = round(p_hint.y / window_state_get_scale_factor(ws));
	}

	if (ss) {
		seat_state_set_hint(ss, hint_x, hint_y);
	}
}

WaylandThread::PointerConstraint WaylandThread::pointer_get_constraint() const {
	return pointer_constraint;
}

BitField<MouseButtonMask> WaylandThread::pointer_get_button_mask() const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		return ss->pointer_data.pressed_button_mask;
	}

	return BitField<MouseButtonMask>();
}

Error WaylandThread::init() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif // DEBUG_ENABLED

	if (initialize_wayland_client(dylibloader_verbose) != 0) {
		WARN_PRINT("Can't load the Wayland client library.");
		return ERR_CANT_CREATE;
	}

	if (initialize_wayland_cursor(dylibloader_verbose) != 0) {
		WARN_PRINT("Can't load the Wayland cursor library.");
		return ERR_CANT_CREATE;
	}

	if (initialize_xkbcommon(dylibloader_verbose) != 0) {
		WARN_PRINT("Can't load the XKBcommon library.");
		return ERR_CANT_CREATE;
	}
#endif // SOWRAP_ENABLED

	KeyMappingXKB::initialize();

	wl_display = wl_display_connect(nullptr);
	ERR_FAIL_NULL_V_MSG(wl_display, ERR_CANT_CREATE, "Can't connect to a Wayland display.");

	thread_data.wl_display = wl_display;

	events_thread.start(_poll_events_thread, &thread_data);

	wl_registry = wl_display_get_registry(wl_display);

	ERR_FAIL_NULL_V_MSG(wl_registry, ERR_UNAVAILABLE, "Can't obtain the Wayland registry global.");

	registry.wayland_thread = this;

	wl_registry_add_listener(wl_registry, &wl_registry_listener, &registry);

	// Wait for registry to get notified from the compositor.
	wl_display_roundtrip(wl_display);

	ERR_FAIL_NULL_V_MSG(registry.wl_shm, ERR_UNAVAILABLE, "Can't obtain the Wayland shared memory global.");
	ERR_FAIL_NULL_V_MSG(registry.wl_compositor, ERR_UNAVAILABLE, "Can't obtain the Wayland compositor global.");
	ERR_FAIL_NULL_V_MSG(registry.xdg_wm_base, ERR_UNAVAILABLE, "Can't obtain the Wayland XDG shell global.");

	if (!registry.xdg_decoration_manager) {
#ifdef LIBDECOR_ENABLED
		WARN_PRINT("Can't obtain the XDG decoration manager. Libdecor will be used for drawing CSDs, if available.");
#else
		WARN_PRINT("Can't obtain the XDG decoration manager. Decorations won't show up.");
#endif // LIBDECOR_ENABLED
	}

	if (!registry.xdg_activation) {
		WARN_PRINT("Can't obtain the XDG activation global. Attention requesting won't work!");
	}

#ifndef DBUS_ENABLED
	if (!registry.wp_idle_inhibit_manager) {
		WARN_PRINT("Can't obtain the idle inhibition manager. The screen might turn off even after calling screen_set_keep_on()!");
	}
#endif // DBUS_ENABLED

	// Wait for seat capabilities.
	wl_display_roundtrip(wl_display);

#ifdef LIBDECOR_ENABLED
	bool libdecor_found = true;

#ifdef SOWRAP_ENABLED
	if (initialize_libdecor(dylibloader_verbose) != 0) {
		libdecor_found = false;
	}
#endif // SOWRAP_ENABLED

	if (libdecor_found) {
		libdecor_context = libdecor_new(wl_display, (struct libdecor_interface *)&libdecor_interface);
	} else {
		print_verbose("libdecor not found. Client-side decorations disabled.");
	}
#endif // LIBDECOR_ENABLED

	cursor_theme_name = OS::get_singleton()->get_environment("XCURSOR_THEME");

	unscaled_cursor_size = OS::get_singleton()->get_environment("XCURSOR_SIZE").to_int();
	if (unscaled_cursor_size <= 0) {
		print_verbose("Detected invalid cursor size preference, defaulting to 24.");
		unscaled_cursor_size = 24;
	}

	// NOTE: The scale is useful here as it might've been updated by _update_scale.
	bool cursor_theme_loaded = _load_cursor_theme(unscaled_cursor_size * cursor_scale);

	if (!cursor_theme_loaded) {
		return ERR_CANT_CREATE;
	}

	// Update the cursor.
	cursor_set_shape(DisplayServer::CURSOR_ARROW);

	initialized = true;
	return OK;
}

void WaylandThread::cursor_set_visible(bool p_visible) {
	cursor_visible = p_visible;

	for (struct wl_seat *wl_seat : registry.wl_seats) {
		SeatState *ss = wl_seat_get_seat_state(wl_seat);
		ERR_FAIL_NULL(ss);

		seat_state_update_cursor(ss);
	}
}

void WaylandThread::cursor_set_shape(DisplayServer::CursorShape p_cursor_shape) {
	cursor_shape = p_cursor_shape;

	for (struct wl_seat *wl_seat : registry.wl_seats) {
		SeatState *ss = wl_seat_get_seat_state(wl_seat);
		ERR_FAIL_NULL(ss);

		seat_state_update_cursor(ss);
	}
}

void WaylandThread::cursor_shape_set_custom_image(DisplayServer::CursorShape p_cursor_shape, Ref<Image> p_image, const Point2i &p_hotspot) {
	ERR_FAIL_COND(p_image.is_null());

	Size2i image_size = p_image->get_size();

	// NOTE: The stride is the width of the image in bytes.
	unsigned int image_stride = image_size.width * 4;
	unsigned int data_size = image_stride * image_size.height;

	// We need a shared memory object file descriptor in order to create a
	// wl_buffer through wl_shm.
	int fd = WaylandThread::_allocate_shm_file(data_size);
	ERR_FAIL_COND(fd == -1);

	CustomCursor &cursor = custom_cursors[p_cursor_shape];
	cursor.hotspot = p_hotspot;

	if (cursor.wl_buffer) {
		// Clean up the old Wayland buffer.
		wl_buffer_destroy(cursor.wl_buffer);
	}

	if (cursor.buffer_data) {
		// Clean up the old buffer data.
		munmap(cursor.buffer_data, cursor.buffer_data_size);
	}

	cursor.buffer_data = (uint32_t *)mmap(nullptr, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	cursor.buffer_data_size = data_size;

	// Create the Wayland buffer.
	struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(registry.wl_shm, fd, data_size);
	// TODO: Make sure that WL_SHM_FORMAT_ARGB8888 format is supported. It
	// technically isn't garaunteed to be supported, but I think that'd be a
	// pretty unlikely thing to stumble upon.
	cursor.wl_buffer = wl_shm_pool_create_buffer(wl_shm_pool, 0, image_size.width, image_size.height, image_stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(wl_shm_pool);

	// Fill the cursor buffer with the image data.
	for (unsigned int index = 0; index < (unsigned int)(image_size.width * image_size.height); index++) {
		int row_index = floor(index / image_size.width);
		int column_index = (index % int(image_size.width));

		cursor.buffer_data[index] = p_image->get_pixel(column_index, row_index).to_argb32();

		// Wayland buffers, unless specified, require associated alpha, so we'll just
		// associate the alpha in-place.
		uint8_t *pixel_data = (uint8_t *)&cursor.buffer_data[index];
		pixel_data[0] = pixel_data[0] * pixel_data[3] / 255;
		pixel_data[1] = pixel_data[1] * pixel_data[3] / 255;
		pixel_data[2] = pixel_data[2] * pixel_data[3] / 255;
	}
}

void WaylandThread::cursor_shape_clear_custom_image(DisplayServer::CursorShape p_cursor_shape) {
	if (custom_cursors.has(p_cursor_shape)) {
		CustomCursor cursor = custom_cursors[p_cursor_shape];
		custom_cursors.erase(p_cursor_shape);

		if (cursor.wl_buffer) {
			wl_buffer_destroy(cursor.wl_buffer);
		}

		if (cursor.buffer_data) {
			munmap(cursor.buffer_data, cursor.buffer_data_size);
		}
	}
}

void WaylandThread::window_set_ime_active(const bool p_active, DisplayServer::WindowID p_window_id) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ss->wp_text_input && ss->ime_enabled) {
		if (p_active) {
			ss->ime_active = true;
			zwp_text_input_v3_enable(ss->wp_text_input);
			zwp_text_input_v3_set_cursor_rectangle(ss->wp_text_input, ss->ime_rect.position.x, ss->ime_rect.position.y, ss->ime_rect.size.x, ss->ime_rect.size.y);
		} else {
			ss->ime_active = false;
			ss->ime_text = String();
			ss->ime_text_commit = String();
			ss->ime_cursor = Vector2i();
			zwp_text_input_v3_disable(ss->wp_text_input);
		}
		zwp_text_input_v3_commit(ss->wp_text_input);
	}
}

void WaylandThread::window_set_ime_position(const Point2i &p_pos, DisplayServer::WindowID p_window_id) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ss->wp_text_input && ss->ime_enabled) {
		ss->ime_rect = Rect2i(p_pos, Size2i(1, 10));
		zwp_text_input_v3_set_cursor_rectangle(ss->wp_text_input, ss->ime_rect.position.x, ss->ime_rect.position.y, ss->ime_rect.size.x, ss->ime_rect.size.y);
		zwp_text_input_v3_commit(ss->wp_text_input);
	}
}

int WaylandThread::keyboard_get_layout_count() const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ss->xkb_keymap) {
		return xkb_keymap_num_layouts(ss->xkb_keymap);
	}

	return 0;
}

int WaylandThread::keyboard_get_current_layout_index() const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		return ss->current_layout_index;
	}

	return 0;
}

void WaylandThread::keyboard_set_current_layout_index(int p_index) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		ss->current_layout_index = p_index;
	}
}

String WaylandThread::keyboard_get_layout_name(int p_index) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ss->xkb_keymap) {
		String ret;
		ret.parse_utf8(xkb_keymap_layout_get_name(ss->xkb_keymap, p_index));

		return ret;
	}

	return "";
}

Key WaylandThread::keyboard_get_key_from_physical(Key p_key) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss && ss->xkb_state) {
		xkb_keycode_t xkb_keycode = KeyMappingXKB::get_xkb_keycode(p_key);
		return KeyMappingXKB::get_keycode(xkb_state_key_get_one_sym(ss->xkb_state, xkb_keycode));
	}

	return Key::NONE;
}

void WaylandThread::keyboard_echo_keys() {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss) {
		seat_state_echo_keys(ss);
	}
}

void WaylandThread::selection_set_text(const String &p_text) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (registry.wl_data_device_manager == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't set selection, wl_data_device_manager global not available.");
	}

	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't set selection, current seat not set.");
		return;
	}

	if (ss->wl_data_device == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't set selection, seat doesn't have wl_data_device.");
	}

	ss->selection_data = p_text.to_utf8_buffer();

	if (ss->wl_data_source_selection == nullptr) {
		ss->wl_data_source_selection = wl_data_device_manager_create_data_source(registry.wl_data_device_manager);
		wl_data_source_add_listener(ss->wl_data_source_selection, &wl_data_source_listener, ss);
		wl_data_source_offer(ss->wl_data_source_selection, "text/plain;charset=utf-8");
		wl_data_source_offer(ss->wl_data_source_selection, "text/plain");

		// TODO: Implement a good way of getting the latest serial from the user.
		wl_data_device_set_selection(ss->wl_data_device, ss->wl_data_source_selection, MAX(ss->pointer_data.button_serial, ss->last_key_pressed_serial));
	}

	// Wait for the message to get to the server before continuing, otherwise the
	// clipboard update might come with a delay.
	wl_display_roundtrip(wl_display);
}

bool WaylandThread::selection_has_mime(const String &p_mime) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't get selection, current seat not set.");
		return false;
	}

	OfferState *os = wl_data_offer_get_offer_state(ss->wl_data_offer_selection);
	if (!os) {
		return false;
	}

	return os->mime_types.has(p_mime);
}

Vector<uint8_t> WaylandThread::selection_get_mime(const String &p_mime) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);
	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't get selection, current seat not set.");
		return Vector<uint8_t>();
	}

	if (ss->wl_data_source_selection) {
		// We have a source so the stuff we're pasting is ours. We'll have to pass the
		// data directly or we'd stall waiting for Godot (ourselves) to send us the
		// data :P

		OfferState *os = wl_data_offer_get_offer_state(ss->wl_data_offer_selection);
		ERR_FAIL_NULL_V(os, Vector<uint8_t>());

		if (os->mime_types.has(p_mime)) {
			// All righty, we're offering this type. Let's just return the data as is.
			return ss->selection_data;
		}

		// ... we don't offer that type. Oh well.
		return Vector<uint8_t>();
	}

	return _wl_data_offer_read(wl_display, p_mime.utf8(), ss->wl_data_offer_selection);
}

bool WaylandThread::primary_has_mime(const String &p_mime) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't get selection, current seat not set.");
		return false;
	}

	OfferState *os = wp_primary_selection_offer_get_offer_state(ss->wp_primary_selection_offer);
	if (!os) {
		return false;
	}

	return os->mime_types.has(p_mime);
}

Vector<uint8_t> WaylandThread::primary_get_mime(const String &p_mime) const {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);
	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't get primary, current seat not set.");
		return Vector<uint8_t>();
	}

	if (ss->wp_primary_selection_source) {
		// We have a source so the stuff we're pasting is ours. We'll have to pass the
		// data directly or we'd stall waiting for Godot (ourselves) to send us the
		// data :P

		OfferState *os = wp_primary_selection_offer_get_offer_state(ss->wp_primary_selection_offer);
		ERR_FAIL_NULL_V(os, Vector<uint8_t>());

		if (os->mime_types.has(p_mime)) {
			// All righty, we're offering this type. Let's just return the data as is.
			return ss->selection_data;
		}

		// ... we don't offer that type. Oh well.
		return Vector<uint8_t>();
	}

	return _wp_primary_selection_offer_read(wl_display, p_mime.utf8(), ss->wp_primary_selection_offer);
}

void WaylandThread::primary_set_text(const String &p_text) {
	SeatState *ss = wl_seat_get_seat_state(wl_seat_current);

	if (registry.wp_primary_selection_device_manager == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't set primary, protocol not available");
		return;
	}

	if (ss == nullptr) {
		DEBUG_LOG_WAYLAND_THREAD("Couldn't set primary, current seat not set.");
		return;
	}

	ss->primary_data = p_text.to_utf8_buffer();

	if (ss->wp_primary_selection_source == nullptr) {
		ss->wp_primary_selection_source = zwp_primary_selection_device_manager_v1_create_source(registry.wp_primary_selection_device_manager);
		zwp_primary_selection_source_v1_add_listener(ss->wp_primary_selection_source, &wp_primary_selection_source_listener, ss);
		zwp_primary_selection_source_v1_offer(ss->wp_primary_selection_source, "text/plain;charset=utf-8");
		zwp_primary_selection_source_v1_offer(ss->wp_primary_selection_source, "text/plain");
	}

	// TODO: Implement a good way of getting the latest serial from the user.
	zwp_primary_selection_device_v1_set_selection(ss->wp_primary_selection_device, ss->wp_primary_selection_source, MAX(ss->pointer_data.button_serial, ss->last_key_pressed_serial));

	// Wait for the message to get to the server before continuing, otherwise the
	// clipboard update might come with a delay.
	wl_display_roundtrip(wl_display);
}

void WaylandThread::commit_surfaces() {
	wl_surface_commit(main_window.wl_surface);
}

void WaylandThread::set_frame() {
	frame = true;
}

bool WaylandThread::get_reset_frame() {
	bool old_frame = frame;
	frame = false;

	return old_frame;
}

// Dispatches events until a frame event is received, a window is reported as
// suspended or the timeout expires.
bool WaylandThread::wait_frame_suspend_ms(int p_timeout) {
	if (main_window.suspended) {
		// The window is suspended! The compositor is telling us _explicitly_ that we
		// don't need to draw, without letting us guess through the frame event's
		// timing and stuff like that. Our job here is done.
		return false;
	}

	if (frame) {
		// We already have a frame! Probably it got there while the caller locked :D
		frame = false;
		return true;
	}

	struct pollfd poll_fd;
	poll_fd.fd = wl_display_get_fd(wl_display);
	poll_fd.events = POLLIN | POLLHUP;

	int begin_ms = OS::get_singleton()->get_ticks_msec();
	int remaining_ms = p_timeout;

	while (remaining_ms > 0) {
		// Empty the event queue while it's full.
		while (wl_display_prepare_read(wl_display) != 0) {
			if (wl_display_dispatch_pending(wl_display) == -1) {
				// Oh no. We'll check and handle any display error below.
				break;
			}

			if (main_window.suspended) {
				return false;
			}

			if (frame) {
				// We had a frame event in the queue :D
				frame = false;
				return true;
			}
		}

		int werror = wl_display_get_error(wl_display);

		if (werror) {
			if (werror == EPROTO) {
				struct wl_interface *wl_interface = nullptr;
				uint32_t id = 0;

				int error_code = wl_display_get_protocol_error(wl_display, (const struct wl_interface **)&wl_interface, &id);
				CRASH_NOW_MSG(vformat("Wayland protocol error %d on interface %s@%d.", error_code, wl_interface ? wl_interface->name : "unknown", id));
			} else {
				CRASH_NOW_MSG(vformat("Wayland client error code %d.", werror));
			}
		}

		wl_display_flush(wl_display);

		// Wait for the event file descriptor to have new data.
		poll(&poll_fd, 1, remaining_ms);

		if (poll_fd.revents | POLLIN) {
			// Load the queues with fresh new data.
			wl_display_read_events(wl_display);
		} else {
			// Oh well... Stop signaling that we want to read.
			wl_display_cancel_read(wl_display);

			// We've got no new events :(
			// We won't even bother with checking the frame flag.
			return false;
		}

		// Let's try dispatching now...
		wl_display_dispatch_pending(wl_display);

		if (main_window.suspended) {
			return false;
		}

		if (frame) {
			frame = false;
			return true;
		}

		remaining_ms -= OS::get_singleton()->get_ticks_msec() - begin_ms;
	}

	DEBUG_LOG_WAYLAND_THREAD("Frame timeout.");
	return false;
}

bool WaylandThread::is_suspended() const {
	return main_window.suspended;
}

void WaylandThread::destroy() {
	if (!initialized) {
		return;
	}

	if (wl_display && events_thread.is_started()) {
		thread_data.thread_done.set();

		// By sending a roundtrip message we're unblocking the polling thread so that
		// it can realize that it's done and also handle every event that's left.
		wl_display_roundtrip(wl_display);

		events_thread.wait_to_finish();
	}

	if (main_window.wp_fractional_scale) {
		wp_fractional_scale_v1_destroy(main_window.wp_fractional_scale);
	}

	if (main_window.wp_viewport) {
		wp_viewport_destroy(main_window.wp_viewport);
	}

	if (main_window.frame_callback) {
		wl_callback_destroy(main_window.frame_callback);
	}

#ifdef LIBDECOR_ENABLED
	if (main_window.libdecor_frame) {
		libdecor_frame_close(main_window.libdecor_frame);
	}
#endif // LIBDECOR_ENABLED

	if (main_window.xdg_toplevel) {
		xdg_toplevel_destroy(main_window.xdg_toplevel);
	}

	if (main_window.xdg_surface) {
		xdg_surface_destroy(main_window.xdg_surface);
	}

	if (main_window.wl_surface) {
		wl_surface_destroy(main_window.wl_surface);
	}

	for (struct wl_seat *wl_seat : registry.wl_seats) {
		SeatState *ss = wl_seat_get_seat_state(wl_seat);
		ERR_FAIL_NULL(ss);

		wl_seat_destroy(wl_seat);

		xkb_context_unref(ss->xkb_context);
		xkb_state_unref(ss->xkb_state);
		xkb_keymap_unref(ss->xkb_keymap);

		if (ss->wl_keyboard) {
			wl_keyboard_destroy(ss->wl_keyboard);
		}

		if (ss->keymap_buffer) {
			munmap((void *)ss->keymap_buffer, ss->keymap_buffer_size);
		}

		if (ss->wl_pointer) {
			wl_pointer_destroy(ss->wl_pointer);
		}

		if (ss->cursor_frame_callback) {
			// We don't need to set a null userdata for safety as the thread is done.
			wl_callback_destroy(ss->cursor_frame_callback);
		}

		if (ss->cursor_surface) {
			wl_surface_destroy(ss->cursor_surface);
		}

		if (ss->wl_data_device) {
			wl_data_device_destroy(ss->wl_data_device);
		}

		if (ss->wp_relative_pointer) {
			zwp_relative_pointer_v1_destroy(ss->wp_relative_pointer);
		}

		if (ss->wp_locked_pointer) {
			zwp_locked_pointer_v1_destroy(ss->wp_locked_pointer);
		}

		if (ss->wp_confined_pointer) {
			zwp_confined_pointer_v1_destroy(ss->wp_confined_pointer);
		}

		if (ss->wp_tablet_seat) {
			zwp_tablet_seat_v2_destroy(ss->wp_tablet_seat);
		}

		for (struct zwp_tablet_tool_v2 *tool : ss->tablet_tools) {
			TabletToolState *state = wp_tablet_tool_get_state(tool);
			if (state) {
				memdelete(state);
			}

			zwp_tablet_tool_v2_destroy(tool);
		}

		memdelete(ss);
	}

	for (struct wl_output *wl_output : registry.wl_outputs) {
		ERR_FAIL_NULL(wl_output);

		memdelete(wl_output_get_screen_state(wl_output));
		wl_output_destroy(wl_output);
	}

	if (wl_cursor_theme) {
		wl_cursor_theme_destroy(wl_cursor_theme);
	}

	if (registry.wp_idle_inhibit_manager) {
		zwp_idle_inhibit_manager_v1_destroy(registry.wp_idle_inhibit_manager);
	}

	if (registry.wp_pointer_constraints) {
		zwp_pointer_constraints_v1_destroy(registry.wp_pointer_constraints);
	}

	if (registry.wp_pointer_gestures) {
		zwp_pointer_gestures_v1_destroy(registry.wp_pointer_gestures);
	}

	if (registry.wp_relative_pointer_manager) {
		zwp_relative_pointer_manager_v1_destroy(registry.wp_relative_pointer_manager);
	}

	if (registry.xdg_activation) {
		xdg_activation_v1_destroy(registry.xdg_activation);
	}

	if (registry.xdg_system_bell) {
		xdg_system_bell_v1_destroy(registry.xdg_system_bell);
	}

	if (registry.xdg_decoration_manager) {
		zxdg_decoration_manager_v1_destroy(registry.xdg_decoration_manager);
	}

	if (registry.wp_fractional_scale_manager) {
		wp_fractional_scale_manager_v1_destroy(registry.wp_fractional_scale_manager);
	}

	if (registry.wp_viewporter) {
		wp_viewporter_destroy(registry.wp_viewporter);
	}

	if (registry.xdg_wm_base) {
		xdg_wm_base_destroy(registry.xdg_wm_base);
	}

	// NOTE: Deprecated.
	if (registry.xdg_exporter_v1) {
		zxdg_exporter_v1_destroy(registry.xdg_exporter_v1);
	}

	if (registry.xdg_exporter_v2) {
		zxdg_exporter_v2_destroy(registry.xdg_exporter_v2);
	}
	if (registry.wl_shm) {
		wl_shm_destroy(registry.wl_shm);
	}

	if (registry.wl_compositor) {
		wl_compositor_destroy(registry.wl_compositor);
	}

	if (wl_registry) {
		wl_registry_destroy(wl_registry);
	}

	if (wl_display) {
		wl_display_disconnect(wl_display);
	}
}

#endif // WAYLAND_ENABLED
