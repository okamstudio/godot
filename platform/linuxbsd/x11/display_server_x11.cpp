/**************************************************************************/
/*  display_server_x11.cpp                                                */
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

#include "display_server_x11.h"

#ifdef X11_ENABLED

#include "x11/detect_prime_x11.h"
#include "x11/key_mapping_x11.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "drivers/png/png_driver_common.h"
#include "main/main.h"

#if defined(VULKAN_ENABLED)
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#endif

#if defined(GLES3_ENABLED)
#include "drivers/gles3/rasterizer_gles3.h"
#endif

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#undef CursorShape
#include <X11/XKBlib.h>

// ICCCM
#define WM_NormalState 1L // window normal state
#define WM_IconicState 3L // window minimized
// EWMH
#define _NET_WM_STATE_REMOVE 0L // remove/unset property
#define _NET_WM_STATE_ADD 1L // add/set property

// 2.2 is the first release with multitouch
#define XINPUT_CLIENT_VERSION_MAJOR 2
#define XINPUT_CLIENT_VERSION_MINOR 2

#define VALUATOR_ABSX 0
#define VALUATOR_ABSY 1
#define VALUATOR_PRESSURE 2
#define VALUATOR_TILTX 3
#define VALUATOR_TILTY 4

//#define DISPLAY_SERVER_X11_DEBUG_LOGS_ENABLED
#ifdef DISPLAY_SERVER_X11_DEBUG_LOGS_ENABLED
#define DEBUG_LOG_X11(...) printf(__VA_ARGS__)
#else
#define DEBUG_LOG_X11(...)
#endif

static const double abs_resolution_mult = 10000.0;
static const double abs_resolution_range_mult = 10.0;

// Hints for X11 fullscreen
struct Hints {
	unsigned long flags = 0;
	unsigned long functions = 0;
	unsigned long decorations = 0;
	long inputMode = 0;
	unsigned long status = 0;
};

static String get_atom_name(Display *p_disp, Atom p_atom) {
	char *name = XGetAtomName(p_disp, p_atom);
	ERR_FAIL_NULL_V_MSG(name, String(), "Atom is invalid.");
	String ret;
	ret.parse_utf8(name);
	XFree(name);
	return ret;
}

bool DisplayServerX11::has_feature(Feature p_feature) const {
	switch (p_feature) {
#ifndef DISABLE_DEPRECATED
		case FEATURE_GLOBAL_MENU: {
			return (native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU));
		} break;
#endif
		case FEATURE_SUBWINDOWS:
#ifdef TOUCH_ENABLED
		case FEATURE_TOUCHSCREEN:
#endif
		case FEATURE_MOUSE:
		case FEATURE_MOUSE_WARP:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_IME:
		case FEATURE_WINDOW_TRANSPARENCY:
		//case FEATURE_HIDPI:
		case FEATURE_ICON:
#ifdef DBUS_ENABLED
		case FEATURE_NATIVE_DIALOG_FILE:
#endif
		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_DIALOG_INPUT:
		//case FEATURE_NATIVE_ICON:
		case FEATURE_SWAP_BUFFERS:
#ifdef DBUS_ENABLED
		case FEATURE_KEEP_SCREEN_ON:
#endif
		case FEATURE_CLIPBOARD_PRIMARY:
		case FEATURE_TEXT_TO_SPEECH:
			return true;
		case FEATURE_SCREEN_CAPTURE:
			return !xwayland;
		default: {
		}
	}

	return false;
}

String DisplayServerX11::get_name() const {
	return "X11";
}

void DisplayServerX11::_update_real_mouse_position(const WindowData &wd) {
	Window root_return, child_return;
	int root_x, root_y, win_x, win_y;
	unsigned int mask_return;

	Bool xquerypointer_result = XQueryPointer(x11_display, wd.x11_window, &root_return, &child_return, &root_x, &root_y,
			&win_x, &win_y, &mask_return);

	if (xquerypointer_result) {
		if (win_x > 0 && win_y > 0 && win_x <= wd.size.width && win_y <= wd.size.height) {
			last_mouse_pos.x = win_x;
			last_mouse_pos.y = win_y;
			last_mouse_pos_valid = true;
			Input::get_singleton()->set_mouse_position(last_mouse_pos);
		}
	}
}

bool DisplayServerX11::_refresh_device_info() {
	int event_base, error_base;

	print_verbose("XInput: Refreshing devices.");

	if (!XQueryExtension(x11_display, "XInputExtension", &xi.opcode, &event_base, &error_base)) {
		print_verbose("XInput extension not available. Please upgrade your distribution.");
		return false;
	}

	int xi_major_query = XINPUT_CLIENT_VERSION_MAJOR;
	int xi_minor_query = XINPUT_CLIENT_VERSION_MINOR;

	if (XIQueryVersion(x11_display, &xi_major_query, &xi_minor_query) != Success) {
		print_verbose(vformat("XInput 2 not available (server supports %d.%d).", xi_major_query, xi_minor_query));
		xi.opcode = 0;
		return false;
	}

	if (xi_major_query < XINPUT_CLIENT_VERSION_MAJOR || (xi_major_query == XINPUT_CLIENT_VERSION_MAJOR && xi_minor_query < XINPUT_CLIENT_VERSION_MINOR)) {
		print_verbose(vformat("XInput %d.%d not available (server supports %d.%d). Touch input unavailable.",
				XINPUT_CLIENT_VERSION_MAJOR, XINPUT_CLIENT_VERSION_MINOR, xi_major_query, xi_minor_query));
	}

	xi.absolute_devices.clear();
	xi.touch_devices.clear();
	xi.pen_inverted_devices.clear();
	xi.last_relative_time = 0;

	int dev_count;
	XIDeviceInfo *info = XIQueryDevice(x11_display, XIAllDevices, &dev_count);

	for (int i = 0; i < dev_count; i++) {
		XIDeviceInfo *dev = &info[i];
		if (!dev->enabled) {
			continue;
		}
		if (!(dev->use == XISlavePointer || dev->use == XIFloatingSlave)) {
			continue;
		}

		bool direct_touch = false;
		bool absolute_mode = false;
		int resolution_x = 0;
		int resolution_y = 0;
		double abs_x_min = 0;
		double abs_x_max = 0;
		double abs_y_min = 0;
		double abs_y_max = 0;
		double pressure_min = 0;
		double pressure_max = 0;
		double tilt_x_min = 0;
		double tilt_x_max = 0;
		double tilt_y_min = 0;
		double tilt_y_max = 0;
		for (int j = 0; j < dev->num_classes; j++) {
#ifdef TOUCH_ENABLED
			if (dev->classes[j]->type == XITouchClass && ((XITouchClassInfo *)dev->classes[j])->mode == XIDirectTouch) {
				direct_touch = true;
			}
#endif
			if (dev->classes[j]->type == XIValuatorClass) {
				XIValuatorClassInfo *class_info = (XIValuatorClassInfo *)dev->classes[j];

				if (class_info->number == VALUATOR_ABSX && class_info->mode == XIModeAbsolute) {
					resolution_x = class_info->resolution;
					abs_x_min = class_info->min;
					abs_x_max = class_info->max;
					absolute_mode = true;
				} else if (class_info->number == VALUATOR_ABSY && class_info->mode == XIModeAbsolute) {
					resolution_y = class_info->resolution;
					abs_y_min = class_info->min;
					abs_y_max = class_info->max;
					absolute_mode = true;
				} else if (class_info->number == VALUATOR_PRESSURE && class_info->mode == XIModeAbsolute) {
					pressure_min = class_info->min;
					pressure_max = class_info->max;
				} else if (class_info->number == VALUATOR_TILTX && class_info->mode == XIModeAbsolute) {
					tilt_x_min = class_info->min;
					tilt_x_max = class_info->max;
				} else if (class_info->number == VALUATOR_TILTY && class_info->mode == XIModeAbsolute) {
					tilt_y_min = class_info->min;
					tilt_y_max = class_info->max;
				}
			}
		}
		if (direct_touch) {
			xi.touch_devices.push_back(dev->deviceid);
			print_verbose("XInput: Using touch device: " + String(dev->name));
		}
		if (absolute_mode) {
			// If no resolution was reported, use the min/max ranges.
			if (resolution_x <= 0) {
				resolution_x = (abs_x_max - abs_x_min) * abs_resolution_range_mult;
			}
			if (resolution_y <= 0) {
				resolution_y = (abs_y_max - abs_y_min) * abs_resolution_range_mult;
			}
			xi.absolute_devices[dev->deviceid] = Vector2(abs_resolution_mult / resolution_x, abs_resolution_mult / resolution_y);
			print_verbose("XInput: Absolute pointing device: " + String(dev->name));
		}

		xi.pressure = 0;
		xi.pen_pressure_range[dev->deviceid] = Vector2(pressure_min, pressure_max);
		xi.pen_tilt_x_range[dev->deviceid] = Vector2(tilt_x_min, tilt_x_max);
		xi.pen_tilt_y_range[dev->deviceid] = Vector2(tilt_y_min, tilt_y_max);
		xi.pen_inverted_devices[dev->deviceid] = String(dev->name).findn("eraser") > 0;
	}

	XIFreeDeviceInfo(info);
#ifdef TOUCH_ENABLED
	if (!xi.touch_devices.size()) {
		print_verbose("XInput: No touch devices found.");
	}
#endif

	return true;
}

void DisplayServerX11::_flush_mouse_motion() {
	// Block events polling while flushing motion events.
	MutexLock mutex_lock(events_mutex);

	for (uint32_t event_index = 0; event_index < polled_events.size(); ++event_index) {
		XEvent &event = polled_events[event_index];
		if (XGetEventData(x11_display, &event.xcookie) && event.xcookie.type == GenericEvent && event.xcookie.extension == xi.opcode) {
			XIDeviceEvent *event_data = (XIDeviceEvent *)event.xcookie.data;
			if (event_data->evtype == XI_RawMotion) {
				XFreeEventData(x11_display, &event.xcookie);
				polled_events.remove_at(event_index--);
				continue;
			}
			XFreeEventData(x11_display, &event.xcookie);
			break;
		}
	}

	xi.relative_motion.x = 0;
	xi.relative_motion.y = 0;
}

#ifdef SPEECHD_ENABLED

bool DisplayServerX11::tts_is_speaking() const {
	ERR_FAIL_NULL_V_MSG(tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->is_speaking();
}

bool DisplayServerX11::tts_is_paused() const {
	ERR_FAIL_NULL_V_MSG(tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->is_paused();
}

TypedArray<Dictionary> DisplayServerX11::tts_get_voices() const {
	ERR_FAIL_NULL_V_MSG(tts, TypedArray<Dictionary>(), "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->get_voices();
}

void DisplayServerX11::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->speak(p_text, p_voice, p_volume, p_pitch, p_rate, p_utterance_id, p_interrupt);
}

void DisplayServerX11::tts_pause() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->pause();
}

void DisplayServerX11::tts_resume() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->resume();
}

void DisplayServerX11::tts_stop() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->stop();
}

#endif

#ifdef DBUS_ENABLED

bool DisplayServerX11::is_dark_mode_supported() const {
	return portal_desktop->is_supported();
}

bool DisplayServerX11::is_dark_mode() const {
	switch (portal_desktop->get_appearance_color_scheme()) {
		case 1:
			// Prefers dark theme.
			return true;
		case 2:
			// Prefers light theme.
			return false;
		default:
			// Preference unknown.
			return false;
	}
}

void DisplayServerX11::set_system_theme_change_callback(const Callable &p_callable) {
	portal_desktop->set_system_theme_change_callback(p_callable);
}

Error DisplayServerX11::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) {
	WindowID window_id = last_focused_window;

	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}

	String xid = vformat("x11:%x", (uint64_t)windows[window_id].x11_window);
	return portal_desktop->file_dialog_show(last_focused_window, xid, p_title, p_current_directory, String(), p_filename, p_mode, p_filters, TypedArray<Dictionary>(), p_callback, false);
}

Error DisplayServerX11::file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) {
	WindowID window_id = last_focused_window;

	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}

	String xid = vformat("x11:%x", (uint64_t)windows[window_id].x11_window);
	return portal_desktop->file_dialog_show(last_focused_window, xid, p_title, p_current_directory, p_root, p_filename, p_mode, p_filters, p_options, p_callback, true);
}

#endif

void DisplayServerX11::mouse_set_mode(MouseMode p_mode) {
	_THREAD_SAFE_METHOD_

	if (p_mode == mouse_mode) {
		return;
	}

	if (mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		XUngrabPointer(x11_display, CurrentTime);
	}

	// The only modes that show a cursor are VISIBLE and CONFINED
	bool show_cursor = (p_mode == MOUSE_MODE_VISIBLE || p_mode == MOUSE_MODE_CONFINED);
	bool previously_shown = (mouse_mode == MOUSE_MODE_VISIBLE || mouse_mode == MOUSE_MODE_CONFINED);

	if (show_cursor && !previously_shown) {
		WindowID window_id = get_window_at_screen_position(mouse_get_position());
		if (window_id != INVALID_WINDOW_ID && window_mouseover_id != window_id) {
			if (window_mouseover_id != INVALID_WINDOW_ID) {
				_send_window_event(windows[window_mouseover_id], WINDOW_EVENT_MOUSE_EXIT);
			}
			window_mouseover_id = window_id;
			_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_ENTER);
		}
	}

	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (show_cursor) {
			XDefineCursor(x11_display, E.value.x11_window, cursors[current_cursor]); // show cursor
		} else {
			XDefineCursor(x11_display, E.value.x11_window, null_cursor); // hide cursor
		}
	}
	mouse_mode = p_mode;

	if (mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		//flush pending motion events
		_flush_mouse_motion();
		WindowID window_id = _get_focused_window_or_popup();
		if (!windows.has(window_id)) {
			window_id = MAIN_WINDOW_ID;
		}
		WindowData &window = windows[window_id];

		if (XGrabPointer(
					x11_display, window.x11_window, True,
					ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
					GrabModeAsync, GrabModeAsync, window.x11_window, None, CurrentTime) != GrabSuccess) {
			ERR_PRINT("NO GRAB");
		}

		if (mouse_mode == MOUSE_MODE_CAPTURED) {
			center.x = window.size.width / 2;
			center.y = window.size.height / 2;

			XWarpPointer(x11_display, None, window.x11_window,
					0, 0, 0, 0, (int)center.x, (int)center.y);

			Input::get_singleton()->set_mouse_position(center);
		}
	} else {
		do_mouse_warp = false;
	}

	XFlush(x11_display);
}

DisplayServerX11::MouseMode DisplayServerX11::mouse_get_mode() const {
	return mouse_mode;
}

void DisplayServerX11::warp_mouse(const Point2i &p_position) {
	_THREAD_SAFE_METHOD_

	if (mouse_mode == MOUSE_MODE_CAPTURED) {
		last_mouse_pos = p_position;
	} else {
		WindowID window_id = _get_focused_window_or_popup();
		if (!windows.has(window_id)) {
			window_id = MAIN_WINDOW_ID;
		}

		XWarpPointer(x11_display, None, windows[window_id].x11_window,
				0, 0, 0, 0, (int)p_position.x, (int)p_position.y);
	}
}

Point2i DisplayServerX11::mouse_get_position() const {
	int number_of_screens = XScreenCount(x11_display);
	for (int i = 0; i < number_of_screens; i++) {
		Window root, child;
		int root_x, root_y, win_x, win_y;
		unsigned int mask;
		if (XQueryPointer(x11_display, XRootWindow(x11_display, i), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
			XWindowAttributes root_attrs;
			XGetWindowAttributes(x11_display, root, &root_attrs);

			return Vector2i(root_attrs.x + root_x, root_attrs.y + root_y);
		}
	}
	return Vector2i();
}

BitField<MouseButtonMask> DisplayServerX11::mouse_get_button_state() const {
	int number_of_screens = XScreenCount(x11_display);
	for (int i = 0; i < number_of_screens; i++) {
		Window root, child;
		int root_x, root_y, win_x, win_y;
		unsigned int mask;
		if (XQueryPointer(x11_display, XRootWindow(x11_display, i), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
			BitField<MouseButtonMask> last_button_state = 0;

			if (mask & Button1Mask) {
				last_button_state.set_flag(MouseButtonMask::LEFT);
			}
			if (mask & Button2Mask) {
				last_button_state.set_flag(MouseButtonMask::MIDDLE);
			}
			if (mask & Button3Mask) {
				last_button_state.set_flag(MouseButtonMask::RIGHT);
			}
			if (mask & Button4Mask) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON1);
			}
			if (mask & Button5Mask) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON2);
			}

			return last_button_state;
		}
	}
	return 0;
}

void DisplayServerX11::clipboard_set(const String &p_text) {
	_THREAD_SAFE_METHOD_

	{
		// The clipboard content can be accessed while polling for events.
		MutexLock mutex_lock(events_mutex);
		internal_clipboard = p_text;
	}

	XSetSelectionOwner(x11_display, XA_PRIMARY, windows[MAIN_WINDOW_ID].x11_window, CurrentTime);
	XSetSelectionOwner(x11_display, XInternAtom(x11_display, "CLIPBOARD", 0), windows[MAIN_WINDOW_ID].x11_window, CurrentTime);
}

void DisplayServerX11::clipboard_set_primary(const String &p_text) {
	_THREAD_SAFE_METHOD_
	if (!p_text.is_empty()) {
		{
			// The clipboard content can be accessed while polling for events.
			MutexLock mutex_lock(events_mutex);
			internal_clipboard_primary = p_text;
		}

		XSetSelectionOwner(x11_display, XA_PRIMARY, windows[MAIN_WINDOW_ID].x11_window, CurrentTime);
		XSetSelectionOwner(x11_display, XInternAtom(x11_display, "PRIMARY", 0), windows[MAIN_WINDOW_ID].x11_window, CurrentTime);
	}
}

Bool DisplayServerX11::_predicate_clipboard_selection(Display *display, XEvent *event, XPointer arg) {
	if (event->type == SelectionNotify && event->xselection.requestor == *(Window *)arg) {
		return True;
	} else {
		return False;
	}
}

Bool DisplayServerX11::_predicate_clipboard_incr(Display *display, XEvent *event, XPointer arg) {
	if (event->type == PropertyNotify && event->xproperty.state == PropertyNewValue && event->xproperty.atom == *(Atom *)arg) {
		return True;
	} else {
		return False;
	}
}

String DisplayServerX11::_clipboard_get_impl(Atom p_source, Window x11_window, Atom target) const {
	String ret;

	Window selection_owner = XGetSelectionOwner(x11_display, p_source);
	if (selection_owner == x11_window) {
		static const char *target_type = "PRIMARY";
		if (p_source != None && get_atom_name(x11_display, p_source) == target_type) {
			return internal_clipboard_primary;
		} else {
			return internal_clipboard;
		}
	}

	if (selection_owner != None) {
		// Block events polling while processing selection events.
		MutexLock mutex_lock(events_mutex);

		Atom selection = XA_PRIMARY;
		XConvertSelection(x11_display, p_source, target, selection,
				x11_window, CurrentTime);

		XFlush(x11_display);

		// Blocking wait for predicate to be True and remove the event from the queue.
		XEvent event;
		XIfEvent(x11_display, &event, _predicate_clipboard_selection, (XPointer)&x11_window);

		// Do not get any data, see how much data is there.
		Atom type;
		int format, result;
		unsigned long len, bytes_left, dummy;
		unsigned char *data;
		XGetWindowProperty(x11_display, x11_window,
				selection, // Tricky..
				0, 0, // offset - len
				0, // Delete 0==FALSE
				AnyPropertyType, // flag
				&type, // return type
				&format, // return format
				&len, &bytes_left, // data length
				&data);

		if (data) {
			XFree(data);
		}

		if (type == XInternAtom(x11_display, "INCR", 0)) {
			// Data is going to be received incrementally.
			DEBUG_LOG_X11("INCR selection started.\n");

			LocalVector<uint8_t> incr_data;
			uint32_t data_size = 0;
			bool success = false;

			// Delete INCR property to notify the owner.
			XDeleteProperty(x11_display, x11_window, type);

			// Process events from the queue.
			bool done = false;
			while (!done) {
				if (!_wait_for_events()) {
					// Error or timeout, abort.
					break;
				}

				// Non-blocking wait for next event and remove it from the queue.
				XEvent ev;
				while (XCheckIfEvent(x11_display, &ev, _predicate_clipboard_incr, (XPointer)&selection)) {
					result = XGetWindowProperty(x11_display, x11_window,
							selection, // selection type
							0, LONG_MAX, // offset - len
							True, // delete property to notify the owner
							AnyPropertyType, // flag
							&type, // return type
							&format, // return format
							&len, &bytes_left, // data length
							&data);

					DEBUG_LOG_X11("PropertyNotify: len=%lu, format=%i\n", len, format);

					if (result == Success) {
						if (data && (len > 0)) {
							uint32_t prev_size = incr_data.size();
							if (prev_size == 0) {
								// First property contains initial data size.
								unsigned long initial_size = *(unsigned long *)data;
								incr_data.resize(initial_size);
							} else {
								// New chunk, resize to be safe and append data.
								incr_data.resize(MAX(data_size + len, prev_size));
								memcpy(incr_data.ptr() + data_size, data, len);
								data_size += len;
							}
						} else {
							// Last chunk, process finished.
							done = true;
							success = true;
						}
					} else {
						print_verbose("Failed to get selection data chunk.");
						done = true;
					}

					if (data) {
						XFree(data);
					}

					if (done) {
						break;
					}
				}
			}

			if (success && (data_size > 0)) {
				ret.parse_utf8((const char *)incr_data.ptr(), data_size);
			}
		} else if (bytes_left > 0) {
			// Data is ready and can be processed all at once.
			result = XGetWindowProperty(x11_display, x11_window,
					selection, 0, bytes_left, 0,
					AnyPropertyType, &type, &format,
					&len, &dummy, &data);

			if (result == Success) {
				ret.parse_utf8((const char *)data);
			} else {
				print_verbose("Failed to get selection data.");
			}

			if (data) {
				XFree(data);
			}
		}
	}

	return ret;
}

Atom DisplayServerX11::_clipboard_get_image_target(Atom p_source, Window x11_window) const {
	Atom target = XInternAtom(x11_display, "TARGETS", 0);
	Atom png = XInternAtom(x11_display, "image/png", 0);
	Atom *valid_targets = nullptr;
	unsigned long atom_count = 0;

	Window selection_owner = XGetSelectionOwner(x11_display, p_source);
	if (selection_owner != None && selection_owner != x11_window) {
		// Block events polling while processing selection events.
		MutexLock mutex_lock(events_mutex);

		Atom selection = XA_PRIMARY;
		XConvertSelection(x11_display, p_source, target, selection, x11_window, CurrentTime);

		XFlush(x11_display);

		// Blocking wait for predicate to be True and remove the event from the queue.
		XEvent event;
		XIfEvent(x11_display, &event, _predicate_clipboard_selection, (XPointer)&x11_window);
		// Do not get any data, see how much data is there.
		Atom type;
		int format, result;
		unsigned long len, bytes_left, dummy;
		XGetWindowProperty(x11_display, x11_window,
				selection, // Tricky..
				0, 0, // offset - len
				0, // Delete 0==FALSE
				XA_ATOM, // flag
				&type, // return type
				&format, // return format
				&len, &bytes_left, // data length
				(unsigned char **)&valid_targets);

		if (valid_targets) {
			XFree(valid_targets);
			valid_targets = nullptr;
		}

		if (type == XA_ATOM && bytes_left > 0) {
			// Data is ready and can be processed all at once.
			result = XGetWindowProperty(x11_display, x11_window,
					selection, 0, bytes_left / 4, 0,
					XA_ATOM, &type, &format,
					&len, &dummy, (unsigned char **)&valid_targets);
			if (result == Success) {
				atom_count = len;
			} else {
				print_verbose("Failed to get selection data.");
				return None;
			}
		} else {
			return None;
		}
	} else {
		return None;
	}
	for (unsigned long i = 0; i < atom_count; i++) {
		Atom atom = valid_targets[i];
		if (atom == png) {
			XFree(valid_targets);
			return png;
		}
	}

	XFree(valid_targets);
	return None;
}

String DisplayServerX11::_clipboard_get(Atom p_source, Window x11_window) const {
	String ret;
	Atom utf8_atom = XInternAtom(x11_display, "UTF8_STRING", True);
	if (utf8_atom != None) {
		ret = _clipboard_get_impl(p_source, x11_window, utf8_atom);
	}
	if (ret.is_empty()) {
		ret = _clipboard_get_impl(p_source, x11_window, XA_STRING);
	}
	return ret;
}

String DisplayServerX11::clipboard_get() const {
	_THREAD_SAFE_METHOD_

	String ret;
	ret = _clipboard_get(XInternAtom(x11_display, "CLIPBOARD", 0), windows[MAIN_WINDOW_ID].x11_window);

	if (ret.is_empty()) {
		ret = _clipboard_get(XA_PRIMARY, windows[MAIN_WINDOW_ID].x11_window);
	}

	return ret;
}

String DisplayServerX11::clipboard_get_primary() const {
	_THREAD_SAFE_METHOD_

	String ret;
	ret = _clipboard_get(XInternAtom(x11_display, "PRIMARY", 0), windows[MAIN_WINDOW_ID].x11_window);

	if (ret.is_empty()) {
		ret = _clipboard_get(XA_PRIMARY, windows[MAIN_WINDOW_ID].x11_window);
	}

	return ret;
}

Ref<Image> DisplayServerX11::clipboard_get_image() const {
	_THREAD_SAFE_METHOD_
	Atom clipboard = XInternAtom(x11_display, "CLIPBOARD", 0);
	Window x11_window = windows[MAIN_WINDOW_ID].x11_window;
	Ref<Image> ret;
	Atom target = _clipboard_get_image_target(clipboard, x11_window);
	if (target == None) {
		return ret;
	}

	Window selection_owner = XGetSelectionOwner(x11_display, clipboard);

	if (selection_owner != None && selection_owner != x11_window) {
		// Block events polling while processing selection events.
		MutexLock mutex_lock(events_mutex);

		// Identifier for the property the other window
		// will send the converted data to.
		Atom transfer_prop = XA_PRIMARY;
		XConvertSelection(x11_display,
				clipboard, // source selection
				target, // format to convert to
				transfer_prop, // output property
				x11_window, CurrentTime);

		XFlush(x11_display);

		// Blocking wait for predicate to be True and remove the event from the queue.
		XEvent event;
		XIfEvent(x11_display, &event, _predicate_clipboard_selection, (XPointer)&x11_window);

		// Do not get any data, see how much data is there.
		Atom type;
		int format, result;
		unsigned long len, bytes_left, dummy;
		unsigned char *data;
		XGetWindowProperty(x11_display, x11_window,
				transfer_prop, // Property data is transferred through
				0, 1, // offset, len (4 so we can get the size if INCR is used)
				0, // Delete 0==FALSE
				AnyPropertyType, // flag
				&type, // return type
				&format, // return format
				&len, &bytes_left, // data length
				&data);

		if (type == XInternAtom(x11_display, "INCR", 0)) {
			ERR_FAIL_COND_V_MSG(len != 1, ret, "Incremental transfer initial value was not length.");

			// Data is going to be received incrementally.
			DEBUG_LOG_X11("INCR selection started.\n");

			LocalVector<uint8_t> incr_data;
			uint32_t data_size = 0;
			bool success = false;

			// Initial response is the lower bound of the length of the transferred data.
			incr_data.resize(*(unsigned long *)data);
			XFree(data);
			data = nullptr;

			// Delete INCR property to notify the owner.
			XDeleteProperty(x11_display, x11_window, transfer_prop);

			// Process events from the queue.
			bool done = false;
			while (!done) {
				if (!_wait_for_events()) {
					// Error or timeout, abort.
					break;
				}
				// Non-blocking wait for next event and remove it from the queue.
				XEvent ev;
				while (XCheckIfEvent(x11_display, &ev, _predicate_clipboard_incr, (XPointer)&transfer_prop)) {
					result = XGetWindowProperty(x11_display, x11_window,
							transfer_prop, // output property
							0, LONG_MAX, // offset - len
							True, // delete property to notify the owner
							AnyPropertyType, // flag
							&type, // return type
							&format, // return format
							&len, &bytes_left, // data length
							&data);

					DEBUG_LOG_X11("PropertyNotify: len=%lu, format=%i\n", len, format);

					if (result == Success) {
						if (data && (len > 0)) {
							uint32_t prev_size = incr_data.size();
							// New chunk, resize to be safe and append data.
							incr_data.resize(MAX(data_size + len, prev_size));
							memcpy(incr_data.ptr() + data_size, data, len);
							data_size += len;
						} else if (!(format == 0 && len == 0)) {
							// For unclear reasons the first GetWindowProperty always returns a length and format of 0.
							// Otherwise, last chunk, process finished.
							done = true;
							success = true;
						}
					} else {
						print_verbose("Failed to get selection data chunk.");
						done = true;
					}

					if (data) {
						XFree(data);
						data = nullptr;
					}

					if (done) {
						break;
					}
				}
			}

			if (success && (data_size > 0)) {
				ret.instantiate();
				PNGDriverCommon::png_to_image(incr_data.ptr(), incr_data.size(), false, ret);
			}
		} else if (bytes_left > 0) {
			if (data) {
				XFree(data);
				data = nullptr;
			}
			// Data is ready and can be processed all at once.
			result = XGetWindowProperty(x11_display, x11_window,
					transfer_prop, 0, bytes_left + 4, 0,
					AnyPropertyType, &type, &format,
					&len, &dummy, &data);
			if (result == Success) {
				ret.instantiate();
				PNGDriverCommon::png_to_image((uint8_t *)data, bytes_left, false, ret);
			} else {
				print_verbose("Failed to get selection data.");
			}

			if (data) {
				XFree(data);
			}
		}
	}

	return ret;
}

bool DisplayServerX11::clipboard_has_image() const {
	Atom target = _clipboard_get_image_target(
			XInternAtom(x11_display, "CLIPBOARD", 0),
			windows[MAIN_WINDOW_ID].x11_window);
	return target != None;
}

Bool DisplayServerX11::_predicate_clipboard_save_targets(Display *display, XEvent *event, XPointer arg) {
	if (event->xany.window == *(Window *)arg) {
		return (event->type == SelectionRequest) ||
				(event->type == SelectionNotify);
	} else {
		return False;
	}
}

void DisplayServerX11::_clipboard_transfer_ownership(Atom p_source, Window x11_window) const {
	_THREAD_SAFE_METHOD_

	Window selection_owner = XGetSelectionOwner(x11_display, p_source);

	if (selection_owner != x11_window) {
		return;
	}

	// Block events polling while processing selection events.
	MutexLock mutex_lock(events_mutex);

	Atom clipboard_manager = XInternAtom(x11_display, "CLIPBOARD_MANAGER", False);
	Atom save_targets = XInternAtom(x11_display, "SAVE_TARGETS", False);
	XConvertSelection(x11_display, clipboard_manager, save_targets, None,
			x11_window, CurrentTime);

	// Process events from the queue.
	while (true) {
		if (!_wait_for_events()) {
			// Error or timeout, abort.
			break;
		}

		// Non-blocking wait for next event and remove it from the queue.
		XEvent ev;
		while (XCheckIfEvent(x11_display, &ev, _predicate_clipboard_save_targets, (XPointer)&x11_window)) {
			switch (ev.type) {
				case SelectionRequest:
					_handle_selection_request_event(&(ev.xselectionrequest));
					break;

				case SelectionNotify: {
					if (ev.xselection.target == save_targets) {
						// Once SelectionNotify is received, we're done whether it succeeded or not.
						return;
					}

					break;
				}
			}
		}
	}
}

int DisplayServerX11::get_screen_count() const {
	_THREAD_SAFE_METHOD_
	int count = 0;

	// Using Xinerama Extension
	int event_base, error_base;
	if (xinerama_ext_ok && XineramaQueryExtension(x11_display, &event_base, &error_base)) {
		XineramaScreenInfo *xsi = XineramaQueryScreens(x11_display, &count);
		XFree(xsi);
	}
	if (count == 0) {
		count = XScreenCount(x11_display);
	}

	return count;
}

int DisplayServerX11::get_primary_screen() const {
	int event_base, error_base;
	if (xinerama_ext_ok && XineramaQueryExtension(x11_display, &event_base, &error_base)) {
		return 0;
	} else {
		return XDefaultScreen(x11_display);
	}
}

int DisplayServerX11::get_keyboard_focus_screen() const {
	int count = get_screen_count();
	if (count < 2) {
		// Early exit with single monitor.
		return 0;
	}

	Window focus = 0;
	int revert_to = 0;

	XGetInputFocus(x11_display, &focus, &revert_to);
	if (focus) {
		Window focus_child = 0;
		int x = 0, y = 0;
		XTranslateCoordinates(x11_display, focus, DefaultRootWindow(x11_display), 0, 0, &x, &y, &focus_child);

		XWindowAttributes xwa;
		XGetWindowAttributes(x11_display, focus, &xwa);
		Rect2i window_rect = Rect2i(x, y, xwa.width, xwa.height);

		// Find which monitor has the largest overlap with the given window.
		int screen_index = 0;
		int max_area = 0;
		for (int i = 0; i < count; i++) {
			Rect2i screen_rect = _screen_get_rect(i);
			Rect2i intersection = screen_rect.intersection(window_rect);
			int area = intersection.get_area();
			if (area > max_area) {
				max_area = area;
				screen_index = i;
			}
		}
		return screen_index;
	}

	return get_primary_screen();
}

Rect2i DisplayServerX11::_screen_get_rect(int p_screen) const {
	Rect2i rect(0, 0, 0, 0);

	p_screen = _get_screen_index(p_screen);
	ERR_FAIL_COND_V(p_screen < 0, rect);

	// Using Xinerama Extension.
	bool found = false;
	int event_base, error_base;
	if (xinerama_ext_ok && XineramaQueryExtension(x11_display, &event_base, &error_base)) {
		int count;
		XineramaScreenInfo *xsi = XineramaQueryScreens(x11_display, &count);
		if (xsi) {
			if (count > 0) {
				// Check if screen is valid.
				if (p_screen < count) {
					rect.position.x = xsi[p_screen].x_org;
					rect.position.y = xsi[p_screen].y_org;
					rect.size.width = xsi[p_screen].width;
					rect.size.height = xsi[p_screen].height;
					found = true;
				} else {
					ERR_PRINT(vformat("Invalid screen index: %d (count: %d).", p_screen, count));
				}
			}
			XFree(xsi);
		}
	}

	if (!found) {
		int count = XScreenCount(x11_display);
		if (p_screen < count) {
			Window root = XRootWindow(x11_display, p_screen);
			XWindowAttributes xwa;
			XGetWindowAttributes(x11_display, root, &xwa);
			rect.position.x = xwa.x;
			rect.position.y = xwa.y;
			rect.size.width = xwa.width;
			rect.size.height = xwa.height;
		} else {
			ERR_PRINT(vformat("Invalid screen index: %d (count: %d).", p_screen, count));
		}
	}

	return rect;
}

Point2i DisplayServerX11::screen_get_position(int p_screen) const {
	_THREAD_SAFE_METHOD_

	return _screen_get_rect(p_screen).position;
}

Size2i DisplayServerX11::screen_get_size(int p_screen) const {
	_THREAD_SAFE_METHOD_

	return _screen_get_rect(p_screen).size;
}

// A Handler to avoid crashing on non-fatal X errors by default.
//
// The original X11 error formatter `_XPrintDefaultError` is defined here:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/e45ca7b41dcd3ace7681d6897505f85d374640f2/src/XlibInt.c#L1322
// It is not exposed through the API, accesses X11 internals,
// and is much more complex, so this is a less complete simplified error X11 printer.
int default_window_error_handler(Display *display, XErrorEvent *error) {
	static char message[1024];
	XGetErrorText(display, error->error_code, message, sizeof(message));

	ERR_PRINT(vformat("Unhandled XServer error: %s"
					  "\n   Major opcode of failed request: %d"
					  "\n   Serial number of failed request: %d"
					  "\n   Current serial number in output stream: %d",
			String::utf8(message), (uint64_t)error->request_code, (uint64_t)error->minor_code, (uint64_t)error->serial));
	return 0;
}

bool g_bad_window = false;
int bad_window_error_handler(Display *display, XErrorEvent *error) {
	if (error->error_code == BadWindow) {
		g_bad_window = true;
	} else {
		return default_window_error_handler(display, error);
	}
	return 0;
}

Rect2i DisplayServerX11::screen_get_usable_rect(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	int screen_count = get_screen_count();

	// Check if screen is valid.
	ERR_FAIL_INDEX_V(p_screen, screen_count, Rect2i(0, 0, 0, 0));

	bool is_multiscreen = screen_count > 1;

	// Use full monitor size as fallback.
	Rect2i rect = _screen_get_rect(p_screen);

	// There's generally only one screen reported by xlib even in multi-screen setup,
	// in this case it's just one virtual screen composed of all physical monitors.
	int x11_screen_count = ScreenCount(x11_display);
	Window x11_window = RootWindow(x11_display, p_screen < x11_screen_count ? p_screen : 0);

	Atom type;
	int format = 0;
	unsigned long remaining = 0;

	// Find active desktop for the root window.
	unsigned int desktop_index = 0;
	Atom desktop_prop = XInternAtom(x11_display, "_NET_CURRENT_DESKTOP", True);
	if (desktop_prop != None) {
		unsigned long desktop_len = 0;
		unsigned char *desktop_data = nullptr;
		if (XGetWindowProperty(x11_display, x11_window, desktop_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &desktop_len, &remaining, &desktop_data) == Success) {
			if ((format == 32) && (desktop_len > 0) && desktop_data) {
				desktop_index = (unsigned int)desktop_data[0];
			}
			if (desktop_data) {
				XFree(desktop_data);
			}
		}
	}

	bool use_simple_method = true;

	// First check for GTK work area, which is more accurate for multi-screen setup.
	if (is_multiscreen) {
		// Use already calculated work area when available.
		Atom gtk_workareas_prop = XInternAtom(x11_display, "_GTK_WORKAREAS", False);
		if (gtk_workareas_prop != None) {
			char gtk_workarea_prop_name[32];
			snprintf(gtk_workarea_prop_name, 32, "_GTK_WORKAREAS_D%d", desktop_index);
			Atom gtk_workarea_prop = XInternAtom(x11_display, gtk_workarea_prop_name, True);
			if (gtk_workarea_prop != None) {
				unsigned long workarea_len = 0;
				unsigned char *workarea_data = nullptr;
				if (XGetWindowProperty(x11_display, x11_window, gtk_workarea_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &workarea_len, &remaining, &workarea_data) == Success) {
					if ((format == 32) && (workarea_len % 4 == 0) && workarea_data) {
						long *rect_data = (long *)workarea_data;
						for (uint32_t data_offset = 0; data_offset < workarea_len; data_offset += 4) {
							Rect2i workarea_rect;
							workarea_rect.position.x = rect_data[data_offset];
							workarea_rect.position.y = rect_data[data_offset + 1];
							workarea_rect.size.x = rect_data[data_offset + 2];
							workarea_rect.size.y = rect_data[data_offset + 3];

							// Intersect with actual monitor size to find the correct area,
							// because areas are not in the same order as screens from Xinerama.
							if (rect.grow(-1).intersects(workarea_rect)) {
								rect = rect.intersection(workarea_rect);
								XFree(workarea_data);
								return rect;
							}
						}
					}
				}
				if (workarea_data) {
					XFree(workarea_data);
				}
			}
		}

		// Fallback to calculating work area by hand from struts.
		Atom client_list_prop = XInternAtom(x11_display, "_NET_CLIENT_LIST", True);
		if (client_list_prop != None) {
			unsigned long clients_len = 0;
			unsigned char *clients_data = nullptr;
			if (XGetWindowProperty(x11_display, x11_window, client_list_prop, 0, LONG_MAX, False, XA_WINDOW, &type, &format, &clients_len, &remaining, &clients_data) == Success) {
				if ((format == 32) && (clients_len > 0) && clients_data) {
					Window *windows_data = (Window *)clients_data;

					Rect2i desktop_rect;
					bool desktop_valid = false;

					// Get full desktop size.
					{
						Atom desktop_geometry_prop = XInternAtom(x11_display, "_NET_DESKTOP_GEOMETRY", True);
						if (desktop_geometry_prop != None) {
							unsigned long geom_len = 0;
							unsigned char *geom_data = nullptr;
							if (XGetWindowProperty(x11_display, x11_window, desktop_geometry_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &geom_len, &remaining, &geom_data) == Success) {
								if ((format == 32) && (geom_len >= 2) && geom_data) {
									desktop_valid = true;
									long *size_data = (long *)geom_data;
									desktop_rect.size.x = size_data[0];
									desktop_rect.size.y = size_data[1];
								}
							}
							if (geom_data) {
								XFree(geom_data);
							}
						}
					}

					// Get full desktop position.
					if (desktop_valid) {
						Atom desktop_viewport_prop = XInternAtom(x11_display, "_NET_DESKTOP_VIEWPORT", True);
						if (desktop_viewport_prop != None) {
							unsigned long viewport_len = 0;
							unsigned char *viewport_data = nullptr;
							if (XGetWindowProperty(x11_display, x11_window, desktop_viewport_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &viewport_len, &remaining, &viewport_data) == Success) {
								if ((format == 32) && (viewport_len >= 2) && viewport_data) {
									desktop_valid = true;
									long *pos_data = (long *)viewport_data;
									desktop_rect.position.x = pos_data[0];
									desktop_rect.position.y = pos_data[1];
								}
							}
							if (viewport_data) {
								XFree(viewport_data);
							}
						}
					}

					if (desktop_valid) {
						use_simple_method = false;

						// Handle bad window errors silently because there's no other way to check
						// that one of the windows has been destroyed in the meantime.
						int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&bad_window_error_handler);

						for (unsigned long win_index = 0; win_index < clients_len; ++win_index) {
							g_bad_window = false;

							// Remove strut size from desktop size to get a more accurate result.
							bool strut_found = false;
							unsigned long strut_len = 0;
							unsigned char *strut_data = nullptr;
							Atom strut_partial_prop = XInternAtom(x11_display, "_NET_WM_STRUT_PARTIAL", True);
							if (strut_partial_prop != None) {
								if (XGetWindowProperty(x11_display, windows_data[win_index], strut_partial_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &strut_len, &remaining, &strut_data) == Success) {
									strut_found = true;
								}
							}
							// Fallback to older strut property.
							if (!g_bad_window && !strut_found) {
								Atom strut_prop = XInternAtom(x11_display, "_NET_WM_STRUT", True);
								if (strut_prop != None) {
									if (XGetWindowProperty(x11_display, windows_data[win_index], strut_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &strut_len, &remaining, &strut_data) == Success) {
										strut_found = true;
									}
								}
							}
							if (!g_bad_window && strut_found && (format == 32) && (strut_len >= 4) && strut_data) {
								long *struts = (long *)strut_data;

								long left = struts[0];
								long right = struts[1];
								long top = struts[2];
								long bottom = struts[3];

								long left_start_y, left_end_y, right_start_y, right_end_y;
								long top_start_x, top_end_x, bottom_start_x, bottom_end_x;

								if (strut_len >= 12) {
									left_start_y = struts[4];
									left_end_y = struts[5];
									right_start_y = struts[6];
									right_end_y = struts[7];
									top_start_x = struts[8];
									top_end_x = struts[9];
									bottom_start_x = struts[10];
									bottom_end_x = struts[11];
								} else {
									left_start_y = 0;
									left_end_y = desktop_rect.size.y;
									right_start_y = 0;
									right_end_y = desktop_rect.size.y;
									top_start_x = 0;
									top_end_x = desktop_rect.size.x;
									bottom_start_x = 0;
									bottom_end_x = desktop_rect.size.x;
								}

								const Point2i &pos = desktop_rect.position;
								const Size2i &size = desktop_rect.size;

								Rect2i left_rect(pos.x, pos.y + left_start_y, left, left_end_y - left_start_y);
								if (left_rect.size.x > 0) {
									Rect2i intersection = rect.intersection(left_rect);
									if (intersection.has_area() && intersection.size.x < rect.size.x) {
										rect.position.x = left_rect.size.x;
										rect.size.x = rect.size.x - intersection.size.x;
									}
								}

								Rect2i right_rect(pos.x + size.x - right, pos.y + right_start_y, right, right_end_y - right_start_y);
								if (right_rect.size.x > 0) {
									Rect2i intersection = rect.intersection(right_rect);
									if (intersection.has_area() && right_rect.size.x < rect.size.x) {
										rect.size.x = intersection.position.x - rect.position.x;
									}
								}

								Rect2i top_rect(pos.x + top_start_x, pos.y, top_end_x - top_start_x, top);
								if (top_rect.size.y > 0) {
									Rect2i intersection = rect.intersection(top_rect);
									if (intersection.has_area() && intersection.size.y < rect.size.y) {
										rect.position.y = top_rect.size.y;
										rect.size.y = rect.size.y - intersection.size.y;
									}
								}

								Rect2i bottom_rect(pos.x + bottom_start_x, pos.y + size.y - bottom, bottom_end_x - bottom_start_x, bottom);
								if (bottom_rect.size.y > 0) {
									Rect2i intersection = rect.intersection(bottom_rect);
									if (intersection.has_area() && right_rect.size.y < rect.size.y) {
										rect.size.y = intersection.position.y - rect.position.y;
									}
								}
							}
							if (strut_data) {
								XFree(strut_data);
							}
						}

						// Restore default error handler.
						XSetErrorHandler(oldHandler);
					}
				}
			}
			if (clients_data) {
				XFree(clients_data);
			}
		}
	}

	// Single screen or fallback for multi screen.
	if (use_simple_method) {
		// Get desktop available size from the global work area.
		Atom workarea_prop = XInternAtom(x11_display, "_NET_WORKAREA", True);
		if (workarea_prop != None) {
			unsigned long workarea_len = 0;
			unsigned char *workarea_data = nullptr;
			if (XGetWindowProperty(x11_display, x11_window, workarea_prop, 0, LONG_MAX, False, XA_CARDINAL, &type, &format, &workarea_len, &remaining, &workarea_data) == Success) {
				if ((format == 32) && (workarea_len >= ((desktop_index + 1) * 4)) && workarea_data) {
					long *rect_data = (long *)workarea_data;
					int data_offset = desktop_index * 4;
					Rect2i workarea_rect;
					workarea_rect.position.x = rect_data[data_offset];
					workarea_rect.position.y = rect_data[data_offset + 1];
					workarea_rect.size.x = rect_data[data_offset + 2];
					workarea_rect.size.y = rect_data[data_offset + 3];

					// Intersect with actual monitor size to get a proper approximation in multi-screen setup.
					if (!is_multiscreen) {
						rect = workarea_rect;
					} else if (rect.intersects(workarea_rect)) {
						rect = rect.intersection(workarea_rect);
					}
				}
			}
			if (workarea_data) {
				XFree(workarea_data);
			}
		}
	}

	return rect;
}

int DisplayServerX11::screen_get_dpi(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	ERR_FAIL_INDEX_V(p_screen, get_screen_count(), 0);

	//Get physical monitor Dimensions through XRandR and calculate dpi
	Size2i sc = screen_get_size(p_screen);
	if (xrandr_ext_ok) {
		int count = 0;
		if (xrr_get_monitors) {
			xrr_monitor_info *monitors = xrr_get_monitors(x11_display, windows[MAIN_WINDOW_ID].x11_window, true, &count);
			if (p_screen < count) {
				double xdpi = sc.width / (double)monitors[p_screen].mwidth * 25.4;
				double ydpi = sc.height / (double)monitors[p_screen].mheight * 25.4;
				xrr_free_monitors(monitors);
				return (xdpi + ydpi) / 2;
			}
			xrr_free_monitors(monitors);
		} else if (p_screen == 0) {
			XRRScreenSize *sizes = XRRSizes(x11_display, 0, &count);
			if (sizes) {
				double xdpi = sc.width / (double)sizes[0].mwidth * 25.4;
				double ydpi = sc.height / (double)sizes[0].mheight * 25.4;
				return (xdpi + ydpi) / 2;
			}
		}
	}

	int width_mm = DisplayWidthMM(x11_display, p_screen);
	int height_mm = DisplayHeightMM(x11_display, p_screen);
	double xdpi = (width_mm ? sc.width / (double)width_mm * 25.4 : 0);
	double ydpi = (height_mm ? sc.height / (double)height_mm * 25.4 : 0);
	if (xdpi || ydpi) {
		return (xdpi + ydpi) / (xdpi && ydpi ? 2 : 1);
	}

	//could not get dpi
	return 96;
}

int get_image_errorhandler(Display *dpy, XErrorEvent *ev) {
	return 0;
}

Color DisplayServerX11::screen_get_pixel(const Point2i &p_position) const {
	Point2i pos = p_position;

	if (xwayland) {
		return Color();
	}

	int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(&get_image_errorhandler);

	Color color;
	int number_of_screens = XScreenCount(x11_display);
	for (int i = 0; i < number_of_screens; i++) {
		Window root = XRootWindow(x11_display, i);
		XWindowAttributes root_attrs;
		XGetWindowAttributes(x11_display, root, &root_attrs);
		if ((pos.x >= root_attrs.x) && (pos.x <= root_attrs.x + root_attrs.width) && (pos.y >= root_attrs.y) && (pos.y <= root_attrs.y + root_attrs.height)) {
			XImage *image = XGetImage(x11_display, root, pos.x, pos.y, 1, 1, AllPlanes, XYPixmap);
			if (image) {
				XColor c;
				c.pixel = XGetPixel(image, 0, 0);
				XDestroyImage(image);
				XQueryColor(x11_display, XDefaultColormap(x11_display, i), &c);
				color = Color(float(c.red) / 65535.0, float(c.green) / 65535.0, float(c.blue) / 65535.0, 1.0);
				break;
			}
		}
	}

	XSetErrorHandler(old_handler);

	return color;
}

Ref<Image> DisplayServerX11::screen_get_image(int p_screen) const {
	ERR_FAIL_INDEX_V(p_screen, get_screen_count(), Ref<Image>());

	switch (p_screen) {
		case SCREEN_PRIMARY: {
			p_screen = get_primary_screen();
		} break;
		case SCREEN_OF_MAIN_WINDOW: {
			p_screen = window_get_current_screen(MAIN_WINDOW_ID);
		} break;
		default:
			break;
	}

	ERR_FAIL_COND_V(p_screen < 0, Ref<Image>());

	if (xwayland) {
		return Ref<Image>();
	}

	int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(&get_image_errorhandler);

	XImage *image = nullptr;

	bool found = false;
	int event_base, error_base;
	if (xinerama_ext_ok && XineramaQueryExtension(x11_display, &event_base, &error_base)) {
		int xin_count;
		XineramaScreenInfo *xsi = XineramaQueryScreens(x11_display, &xin_count);
		if (xsi) {
			if (xin_count > 0) {
				if (p_screen < xin_count) {
					int x_count = XScreenCount(x11_display);
					for (int i = 0; i < x_count; i++) {
						Window root = XRootWindow(x11_display, i);
						XWindowAttributes root_attrs;
						XGetWindowAttributes(x11_display, root, &root_attrs);
						if ((xsi[p_screen].x_org >= root_attrs.x) && (xsi[p_screen].x_org <= root_attrs.x + root_attrs.width) && (xsi[p_screen].y_org >= root_attrs.y) && (xsi[p_screen].y_org <= root_attrs.y + root_attrs.height)) {
							found = true;
							image = XGetImage(x11_display, root, xsi[p_screen].x_org, xsi[p_screen].y_org, xsi[p_screen].width, xsi[p_screen].height, AllPlanes, ZPixmap);
							break;
						}
					}
				} else {
					ERR_PRINT(vformat("Invalid screen index: %d (count: %d).", p_screen, xin_count));
				}
			}
			XFree(xsi);
		}
	}
	if (!found) {
		int x_count = XScreenCount(x11_display);
		if (p_screen < x_count) {
			Window root = XRootWindow(x11_display, p_screen);

			XWindowAttributes root_attrs;
			XGetWindowAttributes(x11_display, root, &root_attrs);

			image = XGetImage(x11_display, root, root_attrs.x, root_attrs.y, root_attrs.width, root_attrs.height, AllPlanes, ZPixmap);
		} else {
			ERR_PRINT(vformat("Invalid screen index: %d (count: %d).", p_screen, x_count));
		}
	}

	XSetErrorHandler(old_handler);

	Ref<Image> img;
	if (image) {
		int width = image->width;
		int height = image->height;

		Vector<uint8_t> img_data;
		img_data.resize(height * width * 4);

		uint8_t *sr = (uint8_t *)image->data;
		uint8_t *wr = (uint8_t *)img_data.ptrw();

		if (image->bits_per_pixel == 24 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					wr[(y * width + x) * 4 + 0] = sr[(y * width + x) * 3 + 2];
					wr[(y * width + x) * 4 + 1] = sr[(y * width + x) * 3 + 1];
					wr[(y * width + x) * 4 + 2] = sr[(y * width + x) * 3 + 0];
					wr[(y * width + x) * 4 + 3] = 255;
				}
			}
		} else if (image->bits_per_pixel == 24 && image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					wr[(y * width + x) * 4 + 0] = sr[(y * width + x) * 3 + 2];
					wr[(y * width + x) * 4 + 1] = sr[(y * width + x) * 3 + 1];
					wr[(y * width + x) * 4 + 2] = sr[(y * width + x) * 3 + 0];
					wr[(y * width + x) * 4 + 3] = 255;
				}
			}
		} else if (image->bits_per_pixel == 32 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					wr[(y * width + x) * 4 + 0] = sr[(y * width + x) * 4 + 2];
					wr[(y * width + x) * 4 + 1] = sr[(y * width + x) * 4 + 1];
					wr[(y * width + x) * 4 + 2] = sr[(y * width + x) * 4 + 0];
					wr[(y * width + x) * 4 + 3] = 255;
				}
			}
		} else {
			String msg = vformat("XImage with RGB mask %x %x %x and depth %d is not supported.", (uint64_t)image->red_mask, (uint64_t)image->green_mask, (uint64_t)image->blue_mask, (int64_t)image->bits_per_pixel);
			XDestroyImage(image);
			ERR_FAIL_V_MSG(Ref<Image>(), msg);
		}
		img = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, img_data);
		XDestroyImage(image);
	}

	return img;
}

float DisplayServerX11::screen_get_refresh_rate(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	ERR_FAIL_INDEX_V(p_screen, get_screen_count(), SCREEN_REFRESH_RATE_FALLBACK);

	//Use xrandr to get screen refresh rate.
	if (xrandr_ext_ok) {
		XRRScreenResources *screen_info = XRRGetScreenResourcesCurrent(x11_display, windows[MAIN_WINDOW_ID].x11_window);
		if (screen_info) {
			RRMode current_mode = 0;
			xrr_monitor_info *monitors = nullptr;

			if (xrr_get_monitors) {
				int count = 0;
				monitors = xrr_get_monitors(x11_display, windows[MAIN_WINDOW_ID].x11_window, true, &count);
				ERR_FAIL_INDEX_V(p_screen, count, SCREEN_REFRESH_RATE_FALLBACK);
			} else {
				ERR_PRINT("An error occurred while trying to get the screen refresh rate.");
				return SCREEN_REFRESH_RATE_FALLBACK;
			}

			bool found_active_mode = false;
			for (int crtc = 0; crtc < screen_info->ncrtc; crtc++) { // Loop through outputs to find which one is currently outputting.
				XRRCrtcInfo *monitor_info = XRRGetCrtcInfo(x11_display, screen_info, screen_info->crtcs[crtc]);
				if (monitor_info->x != monitors[p_screen].x || monitor_info->y != monitors[p_screen].y) { // If X and Y aren't the same as the monitor we're looking for, this isn't the right monitor. Continue.
					continue;
				}

				if (monitor_info->mode != None) {
					current_mode = monitor_info->mode;
					found_active_mode = true;
					break;
				}
			}

			if (found_active_mode) {
				for (int mode = 0; mode < screen_info->nmode; mode++) {
					XRRModeInfo m_info = screen_info->modes[mode];
					if (m_info.id == current_mode) {
						// Snap to nearest 0.01 to stay consistent with other platforms.
						return Math::snapped((float)m_info.dotClock / ((float)m_info.hTotal * (float)m_info.vTotal), 0.01);
					}
				}
			}

			ERR_PRINT("An error occurred while trying to get the screen refresh rate."); // We should have returned the refresh rate by now. An error must have occurred.
			return SCREEN_REFRESH_RATE_FALLBACK;
		} else {
			ERR_PRINT("An error occurred while trying to get the screen refresh rate.");
			return SCREEN_REFRESH_RATE_FALLBACK;
		}
	}
	ERR_PRINT("An error occurred while trying to get the screen refresh rate.");
	return SCREEN_REFRESH_RATE_FALLBACK;
}

#ifdef DBUS_ENABLED
void DisplayServerX11::screen_set_keep_on(bool p_enable) {
	if (screen_is_kept_on() == p_enable) {
		return;
	}

	if (p_enable) {
		screensaver->inhibit();
	} else {
		screensaver->uninhibit();
	}

	keep_screen_on = p_enable;
}

bool DisplayServerX11::screen_is_kept_on() const {
	return keep_screen_on;
}
#endif

Vector<DisplayServer::WindowID> DisplayServerX11::get_window_list() const {
	_THREAD_SAFE_METHOD_

	Vector<int> ret;
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		ret.push_back(E.key);
	}
	return ret;
}

DisplayServer::WindowID DisplayServerX11::create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, WindowID p_transient_parent) {
	_THREAD_SAFE_METHOD_

	WindowID id = _create_window(p_mode, p_vsync_mode, p_flags, p_rect);
	for (int i = 0; i < WINDOW_FLAG_MAX; i++) {
		if (p_flags & (1 << i)) {
			window_set_flag(WindowFlags(i), true, id);
		}
	}
#ifdef RD_ENABLED
	if (rendering_device) {
		rendering_device->screen_create(id);
	}
#endif

	if (p_transient_parent != INVALID_WINDOW_ID) {
		window_set_transient(id, p_transient_parent);
	}

	return id;
}

void DisplayServerX11::show_window(WindowID p_id) {
	_THREAD_SAFE_METHOD_

	const WindowData &wd = windows[p_id];
	popup_open(p_id);

	DEBUG_LOG_X11("show_window: %lu (%u) \n", wd.x11_window, p_id);

	XMapWindow(x11_display, wd.x11_window);
	XSync(x11_display, False);
	_validate_mode_on_map(p_id);
}

void DisplayServerX11::delete_sub_window(WindowID p_id) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_id));
	ERR_FAIL_COND_MSG(p_id == MAIN_WINDOW_ID, "Main window can't be deleted");

	popup_close(p_id);

	WindowData &wd = windows[p_id];

	DEBUG_LOG_X11("delete_sub_window: %lu (%u) \n", wd.x11_window, p_id);

	if (window_mouseover_id == p_id) {
		window_mouseover_id = INVALID_WINDOW_ID;
		_send_window_event(windows[p_id], WINDOW_EVENT_MOUSE_EXIT);
	}

	while (wd.transient_children.size()) {
		window_set_transient(*wd.transient_children.begin(), INVALID_WINDOW_ID);
	}

	if (wd.transient_parent != INVALID_WINDOW_ID) {
		window_set_transient(p_id, INVALID_WINDOW_ID);
	}

#if defined(RD_ENABLED)
	if (rendering_device) {
		rendering_device->screen_free(p_id);
	}

	if (rendering_context) {
		rendering_context->window_destroy(p_id);
	}
#endif
#ifdef GLES3_ENABLED
	if (gl_manager) {
		gl_manager->window_destroy(p_id);
	}
	if (gl_manager_egl) {
		gl_manager_egl->window_destroy(p_id);
	}
#endif

	if (wd.xic) {
		XDestroyIC(wd.xic);
		wd.xic = nullptr;
	}
	XDestroyWindow(x11_display, wd.x11_xim_window);
#ifdef XKB_ENABLED
	if (xkb_loaded_v05p) {
		if (wd.xkb_state) {
			xkb_compose_state_unref(wd.xkb_state);
			wd.xkb_state = nullptr;
		}
	}
#endif

	XUnmapWindow(x11_display, wd.x11_window);
	XDestroyWindow(x11_display, wd.x11_window);

	window_set_rect_changed_callback(Callable(), p_id);
	window_set_window_event_callback(Callable(), p_id);
	window_set_input_event_callback(Callable(), p_id);
	window_set_input_text_callback(Callable(), p_id);
	window_set_drop_files_callback(Callable(), p_id);

	windows.erase(p_id);
}

int64_t DisplayServerX11::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
	ERR_FAIL_COND_V(!windows.has(p_window), 0);
	switch (p_handle_type) {
		case DISPLAY_HANDLE: {
			return (int64_t)x11_display;
		}
		case WINDOW_HANDLE: {
			return (int64_t)windows[p_window].x11_window;
		}
		case WINDOW_VIEW: {
			return 0; // Not supported.
		}
#ifdef GLES3_ENABLED
		case OPENGL_CONTEXT: {
			if (gl_manager) {
				return (int64_t)gl_manager->get_glx_context(p_window);
			}
			if (gl_manager_egl) {
				return (int64_t)gl_manager_egl->get_context(p_window);
			}
			return 0;
		}
		case EGL_DISPLAY: {
			if (gl_manager_egl) {
				return (int64_t)gl_manager_egl->get_display(p_window);
			}
			return 0;
		}
		case EGL_CONFIG: {
			if (gl_manager_egl) {
				return (int64_t)gl_manager_egl->get_config(p_window);
			}
			return 0;
		}
#endif
		default: {
			return 0;
		}
	}
}

void DisplayServerX11::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	wd.instance_id = p_instance;
}

ObjectID DisplayServerX11::window_get_attached_instance_id(WindowID p_window) const {
	ERR_FAIL_COND_V(!windows.has(p_window), ObjectID());
	const WindowData &wd = windows[p_window];
	return wd.instance_id;
}

DisplayServerX11::WindowID DisplayServerX11::get_window_at_screen_position(const Point2i &p_position) const {
	WindowID found_window = INVALID_WINDOW_ID;
	WindowID parent_window = INVALID_WINDOW_ID;
	unsigned int focus_order = 0;
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		const WindowData &wd = E.value;

		// Discard windows with no focus.
		if (wd.focus_order == 0) {
			continue;
		}

		// Find topmost window which contains the given position.
		WindowID window_id = E.key;
		Rect2i win_rect = Rect2i(window_get_position(window_id), window_get_size(window_id));
		if (win_rect.has_point(p_position)) {
			// For siblings, pick the window which was focused last.
			if ((parent_window != wd.transient_parent) || (wd.focus_order > focus_order)) {
				found_window = window_id;
				parent_window = wd.transient_parent;
				focus_order = wd.focus_order;
			}
		}
	}

	return found_window;
}

void DisplayServerX11::window_set_title(const String &p_title, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	XStoreName(x11_display, wd.x11_window, p_title.utf8().get_data());

	Atom _net_wm_name = XInternAtom(x11_display, "_NET_WM_NAME", false);
	Atom utf8_string = XInternAtom(x11_display, "UTF8_STRING", false);
	if (_net_wm_name != None && utf8_string != None) {
		XChangeProperty(x11_display, wd.x11_window, _net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)p_title.utf8().get_data(), p_title.utf8().length());
	}
}

void DisplayServerX11::window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].mpath = p_region;
	_update_window_mouse_passthrough(p_window);
}

void DisplayServerX11::_update_window_mouse_passthrough(WindowID p_window) {
	ERR_FAIL_COND(!windows.has(p_window));
	ERR_FAIL_COND(!xshaped_ext_ok);

	const Vector<Vector2> region_path = windows[p_window].mpath;

	int event_base, error_base;
	const Bool ext_okay = XShapeQueryExtension(x11_display, &event_base, &error_base);
	if (ext_okay) {
		if (windows[p_window].mpass) {
			Region region = XCreateRegion();
			XShapeCombineRegion(x11_display, windows[p_window].x11_window, ShapeInput, 0, 0, region, ShapeSet);
			XDestroyRegion(region);
		} else if (region_path.size() == 0) {
			XShapeCombineMask(x11_display, windows[p_window].x11_window, ShapeInput, 0, 0, None, ShapeSet);
		} else {
			XPoint *points = (XPoint *)memalloc(sizeof(XPoint) * region_path.size());
			for (int i = 0; i < region_path.size(); i++) {
				points[i].x = region_path[i].x;
				points[i].y = region_path[i].y;
			}
			Region region = XPolygonRegion(points, region_path.size(), EvenOddRule);
			memfree(points);
			XShapeCombineRegion(x11_display, windows[p_window].x11_window, ShapeInput, 0, 0, region, ShapeSet);
			XDestroyRegion(region);
		}
	}
}

void DisplayServerX11::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.rect_changed_callback = p_callable;
}

void DisplayServerX11::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.event_callback = p_callable;
}

void DisplayServerX11::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.input_event_callback = p_callable;
}

void DisplayServerX11::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.input_text_callback = p_callable;
}

void DisplayServerX11::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.drop_files_callback = p_callable;
}

int DisplayServerX11::window_get_current_screen(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	int count = get_screen_count();
	if (count < 2) {
		// Early exit with single monitor.
		return 0;
	}

	ERR_FAIL_COND_V(!windows.has(p_window), 0);
	const WindowData &wd = windows[p_window];

	const Rect2i window_rect(wd.position, wd.size);

	// Find which monitor has the largest overlap with the given window.
	int screen_index = 0;
	int max_area = 0;
	for (int i = 0; i < count; i++) {
		Rect2i screen_rect = _screen_get_rect(i);
		Rect2i intersection = screen_rect.intersection(window_rect);
		int area = intersection.get_area();
		if (area > max_area) {
			max_area = area;
			screen_index = i;
		}
	}

	return screen_index;
}

void DisplayServerX11::gl_window_make_current(DisplayServer::WindowID p_window_id) {
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->window_make_current(p_window_id);
	}
	if (gl_manager_egl) {
		gl_manager_egl->window_make_current(p_window_id);
	}
#endif
}

void DisplayServerX11::window_set_current_screen(int p_screen, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	p_screen = _get_screen_index(p_screen);
	ERR_FAIL_INDEX(p_screen, get_screen_count());

	if (window_get_current_screen(p_window) == p_screen) {
		return;
	}

	if (window_get_mode(p_window) == WINDOW_MODE_FULLSCREEN || window_get_mode(p_window) == WINDOW_MODE_MAXIMIZED) {
		Point2i position = screen_get_position(p_screen);
		Size2i size = screen_get_size(p_screen);

		XMoveResizeWindow(x11_display, wd.x11_window, position.x, position.y, size.x, size.y);
	} else {
		Rect2i srect = screen_get_usable_rect(p_screen);
		Point2i wpos = window_get_position(p_window) - screen_get_position(window_get_current_screen(p_window));
		Size2i wsize = window_get_size(p_window);
		wpos += srect.position;
		if (srect != Rect2i()) {
			wpos = wpos.clamp(srect.position, srect.position + srect.size - wsize / 3);
		}
		window_set_position(wpos, p_window);
	}
}

void DisplayServerX11::window_set_transient(WindowID p_window, WindowID p_parent) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(p_window == p_parent);

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd_window = windows[p_window];

	WindowID prev_parent = wd_window.transient_parent;
	ERR_FAIL_COND(prev_parent == p_parent);

	DEBUG_LOG_X11("window_set_transient: %lu (%u), prev_parent=%u, parent=%u\n", wd_window.x11_window, p_window, prev_parent, p_parent);

	ERR_FAIL_COND_MSG(wd_window.on_top, "Windows with the 'on top' can't become transient.");
	if (p_parent == INVALID_WINDOW_ID) {
		//remove transient

		ERR_FAIL_COND(prev_parent == INVALID_WINDOW_ID);
		ERR_FAIL_COND(!windows.has(prev_parent));

		WindowData &wd_parent = windows[prev_parent];

		wd_window.transient_parent = INVALID_WINDOW_ID;
		wd_parent.transient_children.erase(p_window);

		XSetTransientForHint(x11_display, wd_window.x11_window, None);

		XWindowAttributes xwa;
		XSync(x11_display, False);
		XGetWindowAttributes(x11_display, wd_parent.x11_window, &xwa);

		// Set focus to parent sub window to avoid losing all focus when closing a nested sub-menu.
		// RevertToPointerRoot is used to make sure we don't lose all focus in case
		// a subwindow and its parent are both destroyed.
		if (!wd_window.no_focus && !wd_window.is_popup && wd_window.focused) {
			if ((xwa.map_state == IsViewable) && !wd_parent.no_focus && !wd_window.is_popup && _window_focus_check()) {
				_set_input_focus(wd_parent.x11_window, RevertToPointerRoot);
			}
		}
	} else {
		ERR_FAIL_COND(!windows.has(p_parent));
		ERR_FAIL_COND_MSG(prev_parent != INVALID_WINDOW_ID, "Window already has a transient parent");
		WindowData &wd_parent = windows[p_parent];

		wd_window.transient_parent = p_parent;
		wd_parent.transient_children.insert(p_window);

		XSetTransientForHint(x11_display, wd_window.x11_window, wd_parent.x11_window);
	}
}

// Helper method. Assumes that the window id has already been checked and exists.
void DisplayServerX11::_update_size_hints(WindowID p_window) {
	WindowData &wd = windows[p_window];
	WindowMode window_mode = window_get_mode(p_window);
	XSizeHints *xsh = XAllocSizeHints();

	// Always set the position and size hints - they should be synchronized with the actual values after the window is mapped anyway
	xsh->flags |= PPosition | PSize;
	xsh->x = wd.position.x;
	xsh->y = wd.position.y;
	xsh->width = wd.size.width;
	xsh->height = wd.size.height;

	if (window_mode == WINDOW_MODE_FULLSCREEN || window_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		// Do not set any other hints to prevent the window manager from ignoring the fullscreen flags
	} else if (window_get_flag(WINDOW_FLAG_RESIZE_DISABLED, p_window)) {
		// If resizing is disabled, use the forced size
		xsh->flags |= PMinSize | PMaxSize;
		xsh->min_width = wd.size.x;
		xsh->max_width = wd.size.x;
		xsh->min_height = wd.size.y;
		xsh->max_height = wd.size.y;
	} else {
		// Otherwise, just respect min_size and max_size
		if (wd.min_size != Size2i()) {
			xsh->flags |= PMinSize;
			xsh->min_width = wd.min_size.x;
			xsh->min_height = wd.min_size.y;
		}
		if (wd.max_size != Size2i()) {
			xsh->flags |= PMaxSize;
			xsh->max_width = wd.max_size.x;
			xsh->max_height = wd.max_size.y;
		}
	}

	XSetWMNormalHints(x11_display, wd.x11_window, xsh);
	XFree(xsh);
}

Point2i DisplayServerX11::window_get_position(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Point2i());
	const WindowData &wd = windows[p_window];

	return wd.position;
}

Point2i DisplayServerX11::window_get_position_with_decorations(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	if (wd.fullscreen) {
		return wd.position;
	}

	XWindowAttributes xwa;
	XSync(x11_display, False);
	XGetWindowAttributes(x11_display, wd.x11_window, &xwa);
	int x = wd.position.x;
	int y = wd.position.y;
	Atom prop = XInternAtom(x11_display, "_NET_FRAME_EXTENTS", True);
	if (prop != None) {
		Atom type;
		int format;
		unsigned long len;
		unsigned long remaining;
		unsigned char *data = nullptr;
		if (XGetWindowProperty(x11_display, wd.x11_window, prop, 0, 4, False, AnyPropertyType, &type, &format, &len, &remaining, &data) == Success) {
			if (format == 32 && len == 4 && data) {
				long *extents = (long *)data;
				x -= extents[0]; // left
				y -= extents[2]; // top
			}
			XFree(data);
		}
	}
	return Size2i(x, y);
}

void DisplayServerX11::window_set_position(const Point2i &p_position, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	int x = 0;
	int y = 0;
	if (!window_get_flag(WINDOW_FLAG_BORDERLESS, p_window)) {
		//exclude window decorations
		XSync(x11_display, False);
		Atom prop = XInternAtom(x11_display, "_NET_FRAME_EXTENTS", True);
		if (prop != None) {
			Atom type;
			int format;
			unsigned long len;
			unsigned long remaining;
			unsigned char *data = nullptr;
			if (XGetWindowProperty(x11_display, wd.x11_window, prop, 0, 4, False, AnyPropertyType, &type, &format, &len, &remaining, &data) == Success) {
				if (format == 32 && len == 4 && data) {
					long *extents = (long *)data;
					x = extents[0];
					y = extents[2];
				}
				XFree(data);
			}
		}
	}
	XMoveWindow(x11_display, wd.x11_window, p_position.x - x, p_position.y - y);
	_update_real_mouse_position(wd);
}

void DisplayServerX11::window_set_max_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if ((p_size != Size2i()) && ((p_size.x < wd.min_size.x) || (p_size.y < wd.min_size.y))) {
		ERR_PRINT("Maximum window size can't be smaller than minimum window size!");
		return;
	}
	wd.max_size = p_size;

	_update_size_hints(p_window);
	XFlush(x11_display);
}

Size2i DisplayServerX11::window_get_max_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	return wd.max_size;
}

void DisplayServerX11::window_set_min_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if ((p_size != Size2i()) && (wd.max_size != Size2i()) && ((p_size.x > wd.max_size.x) || (p_size.y > wd.max_size.y))) {
		ERR_PRINT("Minimum window size can't be larger than maximum window size!");
		return;
	}
	wd.min_size = p_size;

	_update_size_hints(p_window);
	XFlush(x11_display);
}

Size2i DisplayServerX11::window_get_min_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	return wd.min_size;
}

void DisplayServerX11::window_set_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));

	Size2i size = p_size;
	size = size.maxi(1);

	WindowData &wd = windows[p_window];

	if (wd.size.width == size.width && wd.size.height == size.height) {
		return;
	}

	XWindowAttributes xwa;
	XSync(x11_display, False);
	XGetWindowAttributes(x11_display, wd.x11_window, &xwa);
	int old_w = xwa.width;
	int old_h = xwa.height;

	// Update our videomode width and height
	wd.size = size;

	// Update the size hints first to make sure the window size can be set
	_update_size_hints(p_window);

	// Resize the window
	XResizeWindow(x11_display, wd.x11_window, size.x, size.y);

	for (int timeout = 0; timeout < 50; ++timeout) {
		XSync(x11_display, False);
		XGetWindowAttributes(x11_display, wd.x11_window, &xwa);

		if (old_w != xwa.width || old_h != xwa.height) {
			break;
		}

		OS::get_singleton()->delay_usec(10'000);
	}

	// Keep rendering context window size in sync
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_size(p_window, xwa.width, xwa.height);
	}
#endif
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->window_resize(p_window, xwa.width, xwa.height);
	}
	if (gl_manager_egl) {
		gl_manager_egl->window_resize(p_window, xwa.width, xwa.height);
	}
#endif
}

Size2i DisplayServerX11::window_get_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];
	return wd.size;
}

Size2i DisplayServerX11::window_get_size_with_decorations(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	if (wd.fullscreen) {
		return wd.size;
	}

	XWindowAttributes xwa;
	XSync(x11_display, False);
	XGetWindowAttributes(x11_display, wd.x11_window, &xwa);
	int w = xwa.width;
	int h = xwa.height;
	Atom prop = XInternAtom(x11_display, "_NET_FRAME_EXTENTS", True);
	if (prop != None) {
		Atom type;
		int format;
		unsigned long len;
		unsigned long remaining;
		unsigned char *data = nullptr;
		if (XGetWindowProperty(x11_display, wd.x11_window, prop, 0, 4, False, AnyPropertyType, &type, &format, &len, &remaining, &data) == Success) {
			if (format == 32 && len == 4 && data) {
				long *extents = (long *)data;
				w += extents[0] + extents[1]; // left, right
				h += extents[2] + extents[3]; // top, bottom
			}
			XFree(data);
		}
	}
	return Size2i(w, h);
}

// Just a helper to reduce code duplication in `window_is_maximize_allowed`
// and `_set_wm_maximized`.
bool DisplayServerX11::_window_maximize_check(WindowID p_window, const char *p_atom_name) const {
	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];

	Atom property = XInternAtom(x11_display, p_atom_name, False);
	Atom type;
	int format;
	unsigned long len;
	unsigned long remaining;
	unsigned char *data = nullptr;
	bool retval = false;

	if (property == None) {
		return false;
	}

	int result = XGetWindowProperty(
			x11_display,
			wd.x11_window,
			property,
			0,
			1024,
			False,
			XA_ATOM,
			&type,
			&format,
			&len,
			&remaining,
			&data);

	if (result == Success && data) {
		Atom *atoms = (Atom *)data;
		Atom wm_act_max_horz;
		Atom wm_act_max_vert;
		if (strcmp(p_atom_name, "_NET_WM_STATE") == 0) {
			wm_act_max_horz = XInternAtom(x11_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
			wm_act_max_vert = XInternAtom(x11_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
		} else {
			wm_act_max_horz = XInternAtom(x11_display, "_NET_WM_ACTION_MAXIMIZE_HORZ", False);
			wm_act_max_vert = XInternAtom(x11_display, "_NET_WM_ACTION_MAXIMIZE_VERT", False);
		}
		bool found_wm_act_max_horz = false;
		bool found_wm_act_max_vert = false;

		for (uint64_t i = 0; i < len; i++) {
			if (atoms[i] == wm_act_max_horz) {
				found_wm_act_max_horz = true;
			}
			if (atoms[i] == wm_act_max_vert) {
				found_wm_act_max_vert = true;
			}

			if (found_wm_act_max_horz || found_wm_act_max_vert) {
				retval = true;
				break;
			}
		}

		XFree(data);
	}

	return retval;
}

bool DisplayServerX11::_window_minimize_check(WindowID p_window) const {
	const WindowData &wd = windows[p_window];

	// Using EWMH instead of ICCCM, might work better for Wayland users.
	Atom property = XInternAtom(x11_display, "_NET_WM_STATE", True);
	Atom hidden = XInternAtom(x11_display, "_NET_WM_STATE_HIDDEN", True);
	if (property == None || hidden == None) {
		return false;
	}

	Atom type;
	int format;
	unsigned long len;
	unsigned long remaining;
	Atom *atoms = nullptr;

	int result = XGetWindowProperty(
			x11_display,
			wd.x11_window,
			property,
			0,
			32,
			False,
			XA_ATOM,
			&type,
			&format,
			&len,
			&remaining,
			(unsigned char **)&atoms);

	if (result == Success && atoms) {
		for (unsigned int i = 0; i < len; i++) {
			if (atoms[i] == hidden) {
				XFree(atoms);
				return true;
			}
		}
		XFree(atoms);
	}

	return false;
}

bool DisplayServerX11::_window_fullscreen_check(WindowID p_window) const {
	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];

	// Using EWMH -- Extended Window Manager Hints
	Atom property = XInternAtom(x11_display, "_NET_WM_STATE", False);
	Atom type;
	int format;
	unsigned long len;
	unsigned long remaining;
	unsigned char *data = nullptr;
	bool retval = false;

	if (property == None) {
		return retval;
	}

	int result = XGetWindowProperty(
			x11_display,
			wd.x11_window,
			property,
			0,
			1024,
			False,
			XA_ATOM,
			&type,
			&format,
			&len,
			&remaining,
			&data);

	if (result == Success) {
		Atom *atoms = (Atom *)data;
		Atom wm_fullscreen = XInternAtom(x11_display, "_NET_WM_STATE_FULLSCREEN", False);
		for (uint64_t i = 0; i < len; i++) {
			if (atoms[i] == wm_fullscreen) {
				retval = true;
				break;
			}
		}
		XFree(data);
	}

	return retval;
}

void DisplayServerX11::_validate_mode_on_map(WindowID p_window) {
	// Check if we applied any window modes that didn't take effect while unmapped
	const WindowData &wd = windows[p_window];
	if (wd.fullscreen && !_window_fullscreen_check(p_window)) {
		_set_wm_fullscreen(p_window, true, wd.exclusive_fullscreen);
	} else if (wd.maximized && !_window_maximize_check(p_window, "_NET_WM_STATE")) {
		_set_wm_maximized(p_window, true);
	} else if (wd.minimized && !_window_minimize_check(p_window)) {
		_set_wm_minimized(p_window, true);
	}

	if (wd.on_top) {
		Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
		Atom wm_above = XInternAtom(x11_display, "_NET_WM_STATE_ABOVE", False);

		XClientMessageEvent xev;
		memset(&xev, 0, sizeof(xev));
		xev.type = ClientMessage;
		xev.window = wd.x11_window;
		xev.message_type = wm_state;
		xev.format = 32;
		xev.data.l[0] = _NET_WM_STATE_ADD;
		xev.data.l[1] = wm_above;
		xev.data.l[3] = 1;
		XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xev);
	}
}

bool DisplayServerX11::window_is_maximize_allowed(WindowID p_window) const {
	_THREAD_SAFE_METHOD_
	return _window_maximize_check(p_window, "_NET_WM_ALLOWED_ACTIONS");
}

void DisplayServerX11::_set_wm_maximized(WindowID p_window, bool p_enabled) {
	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	// Using EWMH -- Extended Window Manager Hints
	XEvent xev;
	Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
	Atom wm_max_horz = XInternAtom(x11_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	Atom wm_max_vert = XInternAtom(x11_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = p_enabled ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = wm_max_horz;
	xev.xclient.data.l[2] = wm_max_vert;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	if (p_enabled && window_is_maximize_allowed(p_window)) {
		// Wait for effective resizing (so the GLX context is too).
		// Give up after 0.5s, it's not going to happen on this WM.
		// https://github.com/godotengine/godot/issues/19978
		for (int attempt = 0; window_get_mode(p_window) != WINDOW_MODE_MAXIMIZED && attempt < 50; attempt++) {
			OS::get_singleton()->delay_usec(10'000);
		}
	}
	wd.maximized = p_enabled;
}

void DisplayServerX11::_set_wm_minimized(WindowID p_window, bool p_enabled) {
	WindowData &wd = windows[p_window];
	// Using ICCCM -- Inter-Client Communication Conventions Manual
	XEvent xev;
	Atom wm_change = XInternAtom(x11_display, "WM_CHANGE_STATE", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = wm_change;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = p_enabled ? WM_IconicState : WM_NormalState;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
	Atom wm_hidden = XInternAtom(x11_display, "_NET_WM_STATE_HIDDEN", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = p_enabled ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = wm_hidden;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	wd.minimized = p_enabled;
}

void DisplayServerX11::_set_wm_fullscreen(WindowID p_window, bool p_enabled, bool p_exclusive) {
	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (p_enabled && !window_get_flag(WINDOW_FLAG_BORDERLESS, p_window)) {
		// remove decorations if the window is not already borderless
		Hints hints;
		Atom property;
		hints.flags = 2;
		hints.decorations = 0;
		property = XInternAtom(x11_display, "_MOTIF_WM_HINTS", True);
		if (property != None) {
			XChangeProperty(x11_display, wd.x11_window, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
		}
	}

	if (p_enabled) {
		// Set the window as resizable to prevent window managers to ignore the fullscreen state flag.
		_update_size_hints(p_window);
	}

	// Using EWMH -- Extended Window Manager Hints
	XEvent xev;
	Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
	Atom wm_fullscreen = XInternAtom(x11_display, "_NET_WM_STATE_FULLSCREEN", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = p_enabled ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = wm_fullscreen;
	xev.xclient.data.l[2] = 0;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	// set bypass compositor hint
	Atom bypass_compositor = XInternAtom(x11_display, "_NET_WM_BYPASS_COMPOSITOR", False);
	unsigned long compositing_disable_on = 0; // Use default.
	if (p_enabled) {
		if (p_exclusive) {
			compositing_disable_on = 1; // Force composition OFF to reduce overhead.
		} else {
			compositing_disable_on = 2; // Force composition ON to allow popup windows.
		}
	}
	if (bypass_compositor != None) {
		XChangeProperty(x11_display, wd.x11_window, bypass_compositor, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&compositing_disable_on, 1);
	}

	XFlush(x11_display);

	if (!p_enabled) {
		// Reset the non-resizable flags if we un-set these before.
		_update_size_hints(p_window);

		// put back or remove decorations according to the last set borderless state
		Hints hints;
		Atom property;
		hints.flags = 2;
		hints.decorations = wd.borderless ? 0 : 1;
		property = XInternAtom(x11_display, "_MOTIF_WM_HINTS", True);
		if (property != None) {
			XChangeProperty(x11_display, wd.x11_window, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
		}
	}
}

void DisplayServerX11::window_set_mode(WindowMode p_mode, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	WindowMode old_mode = window_get_mode(p_window);
	if (old_mode == p_mode) {
		return; // do nothing
	}
	//remove all "extra" modes

	switch (old_mode) {
		case WINDOW_MODE_WINDOWED: {
			//do nothing
		} break;
		case WINDOW_MODE_MINIMIZED: {
			_set_wm_minimized(p_window, false);
		} break;
		case WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		case WINDOW_MODE_FULLSCREEN: {
			//Remove full-screen
			wd.fullscreen = false;
			wd.exclusive_fullscreen = false;

			_set_wm_fullscreen(p_window, false, false);

			//un-maximize required for always on top
			bool on_top = window_get_flag(WINDOW_FLAG_ALWAYS_ON_TOP, p_window);

			window_set_position(wd.last_position_before_fs, p_window);

			if (on_top) {
				_set_wm_maximized(p_window, false);
			}

		} break;
		case WINDOW_MODE_MAXIMIZED: {
			_set_wm_maximized(p_window, false);
		} break;
	}

	switch (p_mode) {
		case WINDOW_MODE_WINDOWED: {
			//do nothing
		} break;
		case WINDOW_MODE_MINIMIZED: {
			_set_wm_minimized(p_window, true);
		} break;
		case WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		case WINDOW_MODE_FULLSCREEN: {
			wd.last_position_before_fs = wd.position;

			if (window_get_flag(WINDOW_FLAG_ALWAYS_ON_TOP, p_window)) {
				_set_wm_maximized(p_window, true);
			}

			wd.fullscreen = true;
			if (p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
				wd.exclusive_fullscreen = true;
				_set_wm_fullscreen(p_window, true, true);
			} else {
				wd.exclusive_fullscreen = false;
				_set_wm_fullscreen(p_window, true, false);
			}
		} break;
		case WINDOW_MODE_MAXIMIZED: {
			_set_wm_maximized(p_window, true);
		} break;
	}
}

DisplayServer::WindowMode DisplayServerX11::window_get_mode(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), WINDOW_MODE_WINDOWED);
	const WindowData &wd = windows[p_window];

	if (wd.fullscreen) { //if fullscreen, it's not in another mode
		if (wd.exclusive_fullscreen) {
			return WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
		} else {
			return WINDOW_MODE_FULLSCREEN;
		}
	}

	// Test maximized.
	// Using EWMH -- Extended Window Manager Hints
	if (_window_maximize_check(p_window, "_NET_WM_STATE")) {
		return WINDOW_MODE_MAXIMIZED;
	}

	{
		if (_window_minimize_check(p_window)) {
			return WINDOW_MODE_MINIMIZED;
		}
	}

	// All other discarded, return windowed.

	return WINDOW_MODE_WINDOWED;
}

void DisplayServerX11::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	switch (p_flag) {
		case WINDOW_FLAG_RESIZE_DISABLED: {
			wd.resize_disabled = p_enabled;

			_update_size_hints(p_window);

			XFlush(x11_display);
		} break;
		case WINDOW_FLAG_BORDERLESS: {
			Hints hints;
			Atom property;
			hints.flags = 2;
			hints.decorations = p_enabled ? 0 : 1;
			property = XInternAtom(x11_display, "_MOTIF_WM_HINTS", True);
			if (property != None) {
				XChangeProperty(x11_display, wd.x11_window, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
			}

			// Preserve window size
			window_set_size(window_get_size(p_window), p_window);

			wd.borderless = p_enabled;
			_update_window_mouse_passthrough(p_window);
		} break;
		case WINDOW_FLAG_ALWAYS_ON_TOP: {
			ERR_FAIL_COND_MSG(wd.transient_parent != INVALID_WINDOW_ID, "Can't make a window transient if the 'on top' flag is active.");
			if (p_enabled && wd.fullscreen) {
				_set_wm_maximized(p_window, true);
			}

			Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
			Atom wm_above = XInternAtom(x11_display, "_NET_WM_STATE_ABOVE", False);

			XClientMessageEvent xev;
			memset(&xev, 0, sizeof(xev));
			xev.type = ClientMessage;
			xev.window = wd.x11_window;
			xev.message_type = wm_state;
			xev.format = 32;
			xev.data.l[0] = p_enabled ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
			xev.data.l[1] = wm_above;
			xev.data.l[3] = 1;
			XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xev);

			if (!p_enabled && !wd.fullscreen) {
				_set_wm_maximized(p_window, false);
			}
			wd.on_top = p_enabled;

		} break;
		case WINDOW_FLAG_TRANSPARENT: {
			wd.layered_window = p_enabled;
		} break;
		case WINDOW_FLAG_NO_FOCUS: {
			wd.no_focus = p_enabled;
		} break;
		case WINDOW_FLAG_MOUSE_PASSTHROUGH: {
			wd.mpass = p_enabled;
			_update_window_mouse_passthrough(p_window);
		} break;
		case WINDOW_FLAG_POPUP: {
			XWindowAttributes xwa;
			XSync(x11_display, False);
			XGetWindowAttributes(x11_display, wd.x11_window, &xwa);

			ERR_FAIL_COND_MSG(p_window == MAIN_WINDOW_ID, "Main window can't be popup.");
			ERR_FAIL_COND_MSG((xwa.map_state == IsViewable) && (wd.is_popup != p_enabled), "Popup flag can't changed while window is opened.");
			wd.is_popup = p_enabled;
		} break;
		default: {
		}
	}
}

bool DisplayServerX11::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];

	switch (p_flag) {
		case WINDOW_FLAG_RESIZE_DISABLED: {
			return wd.resize_disabled;
		} break;
		case WINDOW_FLAG_BORDERLESS: {
			bool borderless = wd.borderless;
			Atom prop = XInternAtom(x11_display, "_MOTIF_WM_HINTS", True);
			if (prop != None) {
				Atom type;
				int format;
				unsigned long len;
				unsigned long remaining;
				unsigned char *data = nullptr;
				if (XGetWindowProperty(x11_display, wd.x11_window, prop, 0, sizeof(Hints), False, AnyPropertyType, &type, &format, &len, &remaining, &data) == Success) {
					if (data && (format == 32) && (len >= 5)) {
						borderless = !(reinterpret_cast<Hints *>(data)->decorations);
					}
					if (data) {
						XFree(data);
					}
				}
			}
			return borderless;
		} break;
		case WINDOW_FLAG_ALWAYS_ON_TOP: {
			return wd.on_top;
		} break;
		case WINDOW_FLAG_TRANSPARENT: {
			return wd.layered_window;
		} break;
		case WINDOW_FLAG_NO_FOCUS: {
			return wd.no_focus;
		} break;
		case WINDOW_FLAG_MOUSE_PASSTHROUGH: {
			return wd.mpass;
		} break;
		case WINDOW_FLAG_POPUP: {
			return wd.is_popup;
		} break;
		default: {
		}
	}

	return false;
}

void DisplayServerX11::window_request_attention(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	const WindowData &wd = windows[p_window];
	// Using EWMH -- Extended Window Manager Hints
	//
	// Sets the _NET_WM_STATE_DEMANDS_ATTENTION atom for WM_STATE
	// Will be unset by the window manager after user react on the request for attention

	XEvent xev;
	Atom wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
	Atom wm_attention = XInternAtom(x11_display, "_NET_WM_STATE_DEMANDS_ATTENTION", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = wm_attention;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	XFlush(x11_display);
}

void DisplayServerX11::window_move_to_foreground(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	const WindowData &wd = windows[p_window];

	XEvent xev;
	Atom net_active_window = XInternAtom(x11_display, "_NET_ACTIVE_WINDOW", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = wd.x11_window;
	xev.xclient.message_type = net_active_window;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = CurrentTime;

	XSendEvent(x11_display, DefaultRootWindow(x11_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	XFlush(x11_display);
}

DisplayServerX11::WindowID DisplayServerX11::get_focused_window() const {
	return last_focused_window;
}

bool DisplayServerX11::window_is_focused(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);

	const WindowData &wd = windows[p_window];

	return wd.focused;
}

bool DisplayServerX11::window_can_draw(WindowID p_window) const {
	//this seems to be all that is provided by X11
	return window_get_mode(p_window) != WINDOW_MODE_MINIMIZED;
}

bool DisplayServerX11::can_any_window_draw() const {
	_THREAD_SAFE_METHOD_

	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (window_get_mode(E.key) != WINDOW_MODE_MINIMIZED) {
			return true;
		}
	}

	return false;
}

void DisplayServerX11::window_set_ime_active(const bool p_active, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (!wd.xic) {
		return;
	}
	if (!wd.focused) {
		wd.ime_active = false;
		im_text = String();
		im_selection = Vector2i();
		return;
	}

	// Block events polling while changing input focus
	// because it triggers some event polling internally.
	if (p_active) {
		MutexLock mutex_lock(events_mutex);

		wd.ime_active = true;

		XMapWindow(x11_display, wd.x11_xim_window);

		XWindowAttributes xwa;
		XSync(x11_display, False);
		XGetWindowAttributes(x11_display, wd.x11_xim_window, &xwa);
		if (xwa.map_state == IsViewable && _window_focus_check()) {
			_set_input_focus(wd.x11_xim_window, RevertToParent);
		}
		XSetICFocus(wd.xic);
	} else {
		MutexLock mutex_lock(events_mutex);
		XUnsetICFocus(wd.xic);
		XUnmapWindow(x11_display, wd.x11_xim_window);
		wd.ime_active = false;

		im_text = String();
		im_selection = Vector2i();
	}
}

void DisplayServerX11::window_set_ime_position(const Point2i &p_pos, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (!wd.xic) {
		return;
	}
	if (!wd.focused) {
		return;
	}

	if (wd.ime_active) {
		XWindowAttributes xwa;
		XSync(x11_display, False);
		XGetWindowAttributes(x11_display, wd.x11_xim_window, &xwa);
		if (xwa.map_state == IsViewable) {
			XMoveWindow(x11_display, wd.x11_xim_window, p_pos.x, p_pos.y);
		}
	}
}

Point2i DisplayServerX11::ime_get_selection() const {
	return im_selection;
}

String DisplayServerX11::ime_get_text() const {
	return im_text;
}

void DisplayServerX11::cursor_set_shape(CursorShape p_shape) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (p_shape == current_cursor) {
		return;
	}

	if (mouse_mode == MOUSE_MODE_VISIBLE || mouse_mode == MOUSE_MODE_CONFINED) {
		if (cursors[p_shape] != None) {
			for (const KeyValue<WindowID, WindowData> &E : windows) {
				XDefineCursor(x11_display, E.value.x11_window, cursors[p_shape]);
			}
		} else if (cursors[CURSOR_ARROW] != None) {
			for (const KeyValue<WindowID, WindowData> &E : windows) {
				XDefineCursor(x11_display, E.value.x11_window, cursors[CURSOR_ARROW]);
			}
		}
	}

	current_cursor = p_shape;
}

DisplayServerX11::CursorShape DisplayServerX11::cursor_get_shape() const {
	return current_cursor;
}

void DisplayServerX11::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (p_cursor.is_valid()) {
		HashMap<CursorShape, Vector<Variant>>::Iterator cursor_c = cursors_cache.find(p_shape);

		if (cursor_c) {
			if (cursor_c->value[0] == p_cursor && cursor_c->value[1] == p_hotspot) {
				cursor_set_shape(p_shape);
				return;
			}

			cursors_cache.erase(p_shape);
		}

		Ref<Image> image = _get_cursor_image_from_resource(p_cursor, p_hotspot);
		ERR_FAIL_COND(image.is_null());
		Vector2i texture_size = image->get_size();

		// Create the cursor structure
		XcursorImage *cursor_image = XcursorImageCreate(texture_size.width, texture_size.height);
		XcursorUInt image_size = texture_size.width * texture_size.height;
		XcursorDim size = sizeof(XcursorPixel) * image_size;

		cursor_image->version = 1;
		cursor_image->size = size;
		cursor_image->xhot = p_hotspot.x;
		cursor_image->yhot = p_hotspot.y;

		// allocate memory to contain the whole file
		cursor_image->pixels = (XcursorPixel *)memalloc(size);

		for (XcursorPixel index = 0; index < image_size; index++) {
			int row_index = floor(index / texture_size.width);
			int column_index = index % int(texture_size.width);

			*(cursor_image->pixels + index) = image->get_pixel(column_index, row_index).to_argb32();
		}

		ERR_FAIL_NULL(cursor_image->pixels);

		// Save it for a further usage
		cursors[p_shape] = XcursorImageLoadCursor(x11_display, cursor_image);

		Vector<Variant> params;
		params.push_back(p_cursor);
		params.push_back(p_hotspot);
		cursors_cache.insert(p_shape, params);

		if (p_shape == current_cursor) {
			if (mouse_mode == MOUSE_MODE_VISIBLE || mouse_mode == MOUSE_MODE_CONFINED) {
				for (const KeyValue<WindowID, WindowData> &E : windows) {
					XDefineCursor(x11_display, E.value.x11_window, cursors[p_shape]);
				}
			}
		}

		memfree(cursor_image->pixels);
		XcursorImageDestroy(cursor_image);
	} else {
		// Reset to default system cursor
		if (cursor_img[p_shape]) {
			cursors[p_shape] = XcursorImageLoadCursor(x11_display, cursor_img[p_shape]);
		}

		cursors_cache.erase(p_shape);

		CursorShape c = current_cursor;
		current_cursor = CURSOR_MAX;
		cursor_set_shape(c);
	}
}

int DisplayServerX11::keyboard_get_layout_count() const {
	int _group_count = 0;
	XkbDescRec *kbd = XkbAllocKeyboard();
	if (kbd) {
		kbd->dpy = x11_display;
		XkbGetControls(x11_display, XkbAllControlsMask, kbd);
		XkbGetNames(x11_display, XkbSymbolsNameMask, kbd);

		const Atom *groups = kbd->names->groups;
		if (kbd->ctrls != nullptr) {
			_group_count = kbd->ctrls->num_groups;
		} else {
			while (_group_count < XkbNumKbdGroups && groups[_group_count] != None) {
				_group_count++;
			}
		}
		XkbFreeKeyboard(kbd, 0, true);
	}
	return _group_count;
}

int DisplayServerX11::keyboard_get_current_layout() const {
	XkbStateRec state;
	XkbGetState(x11_display, XkbUseCoreKbd, &state);
	return state.group;
}

void DisplayServerX11::keyboard_set_current_layout(int p_index) {
	ERR_FAIL_INDEX(p_index, keyboard_get_layout_count());
	XkbLockGroup(x11_display, XkbUseCoreKbd, p_index);
}

String DisplayServerX11::keyboard_get_layout_language(int p_index) const {
	String ret;
	XkbDescRec *kbd = XkbAllocKeyboard();
	if (kbd) {
		kbd->dpy = x11_display;
		XkbGetControls(x11_display, XkbAllControlsMask, kbd);
		XkbGetNames(x11_display, XkbSymbolsNameMask, kbd);
		XkbGetNames(x11_display, XkbGroupNamesMask, kbd);

		int _group_count = 0;
		const Atom *groups = kbd->names->groups;
		if (kbd->ctrls != nullptr) {
			_group_count = kbd->ctrls->num_groups;
		} else {
			while (_group_count < XkbNumKbdGroups && groups[_group_count] != None) {
				_group_count++;
			}
		}

		Atom names = kbd->names->symbols;
		if (names != None) {
			Vector<String> info = get_atom_name(x11_display, names).split("+");
			if (p_index >= 0 && p_index < _group_count) {
				if (p_index + 1 < info.size()) {
					ret = info[p_index + 1]; // Skip "pc" at the start and "inet"/"group" at the end of symbols.
				} else {
					ret = "en"; // No symbol for layout fallback to "en".
				}
			} else {
				ERR_PRINT("Index " + itos(p_index) + "is out of bounds (" + itos(_group_count) + ").");
			}
		}
		XkbFreeKeyboard(kbd, 0, true);
	}
	return ret.substr(0, 2);
}

String DisplayServerX11::keyboard_get_layout_name(int p_index) const {
	String ret;
	XkbDescRec *kbd = XkbAllocKeyboard();
	if (kbd) {
		kbd->dpy = x11_display;
		XkbGetControls(x11_display, XkbAllControlsMask, kbd);
		XkbGetNames(x11_display, XkbSymbolsNameMask, kbd);
		XkbGetNames(x11_display, XkbGroupNamesMask, kbd);

		int _group_count = 0;
		const Atom *groups = kbd->names->groups;
		if (kbd->ctrls != nullptr) {
			_group_count = kbd->ctrls->num_groups;
		} else {
			while (_group_count < XkbNumKbdGroups && groups[_group_count] != None) {
				_group_count++;
			}
		}

		if (p_index >= 0 && p_index < _group_count) {
			ret = get_atom_name(x11_display, groups[p_index]);
		} else {
			ERR_PRINT("Index " + itos(p_index) + "is out of bounds (" + itos(_group_count) + ").");
		}
		XkbFreeKeyboard(kbd, 0, true);
	}
	return ret;
}

Key DisplayServerX11::keyboard_get_keycode_from_physical(Key p_keycode) const {
	Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	Key keycode_no_mod = p_keycode & KeyModifierMask::CODE_MASK;
	unsigned int xkeycode = KeyMappingX11::get_xlibcode(keycode_no_mod);
	KeySym xkeysym = XkbKeycodeToKeysym(x11_display, xkeycode, keyboard_get_current_layout(), 0);
	if (is_ascii_lower_case(xkeysym)) {
		xkeysym -= ('a' - 'A');
	}

	Key key = KeyMappingX11::get_keycode(xkeysym);
	// If not found, fallback to QWERTY.
	// This should match the behavior of the event pump
	if (key == Key::NONE) {
		return p_keycode;
	}
	return (Key)(key | modifiers);
}

Key DisplayServerX11::keyboard_get_label_from_physical(Key p_keycode) const {
	Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	Key keycode_no_mod = p_keycode & KeyModifierMask::CODE_MASK;
	unsigned int xkeycode = KeyMappingX11::get_xlibcode(keycode_no_mod);
	KeySym xkeysym = XkbKeycodeToKeysym(x11_display, xkeycode, keyboard_get_current_layout(), 0);
	if (is_ascii_lower_case(xkeysym)) {
		xkeysym -= ('a' - 'A');
	}

	Key key = KeyMappingX11::get_keycode(xkeysym);
#ifdef XKB_ENABLED
	if (xkb_loaded_v08p) {
		String keysym = String::chr(xkb_keysym_to_utf32(xkb_keysym_to_upper(xkeysym)));
		key = fix_key_label(keysym[0], KeyMappingX11::get_keycode(xkeysym));
	}
#endif

	// If not found, fallback to QWERTY.
	// This should match the behavior of the event pump
	if (key == Key::NONE) {
		return p_keycode;
	}
	return (Key)(key | modifiers);
}
DisplayServerX11::Property DisplayServerX11::_read_property(Display *p_display, Window p_window, Atom p_property) {
	Atom actual_type = None;
	int actual_format = 0;
	unsigned long nitems = 0;
	unsigned long bytes_after = 0;
	unsigned char *ret = nullptr;

	// Keep trying to read the property until there are no bytes unread.
	if (p_property != None) {
		int read_bytes = 1024;
		do {
			if (ret != nullptr) {
				XFree(ret);
			}

			XGetWindowProperty(p_display, p_window, p_property, 0, read_bytes, False, AnyPropertyType,
					&actual_type, &actual_format, &nitems, &bytes_after,
					&ret);

			read_bytes *= 2;

		} while (bytes_after != 0);
	}

	Property p = { ret, actual_format, (int)nitems, actual_type };

	return p;
}

static Atom pick_target_from_list(Display *p_display, const Atom *p_list, int p_count) {
	static const char *target_type = "text/uri-list";

	for (int i = 0; i < p_count; i++) {
		Atom atom = p_list[i];

		if (atom != None && get_atom_name(p_display, atom) == target_type) {
			return atom;
		}
	}
	return None;
}

static Atom pick_target_from_atoms(Display *p_disp, Atom p_t1, Atom p_t2, Atom p_t3) {
	static const char *target_type = "text/uri-list";
	if (p_t1 != None && get_atom_name(p_disp, p_t1) == target_type) {
		return p_t1;
	}

	if (p_t2 != None && get_atom_name(p_disp, p_t2) == target_type) {
		return p_t2;
	}

	if (p_t3 != None && get_atom_name(p_disp, p_t3) == target_type) {
		return p_t3;
	}

	return None;
}

void DisplayServerX11::_get_key_modifier_state(unsigned int p_x11_state, Ref<InputEventWithModifiers> state) {
	state->set_shift_pressed((p_x11_state & ShiftMask));
	state->set_ctrl_pressed((p_x11_state & ControlMask));
	state->set_alt_pressed((p_x11_state & Mod1Mask /*|| p_x11_state&Mod5Mask*/)); //altgr should not count as alt
	state->set_meta_pressed((p_x11_state & Mod4Mask));
}

void DisplayServerX11::_handle_key_event(WindowID p_window, XKeyEvent *p_event, LocalVector<XEvent> &p_events, uint32_t &p_event_index, bool p_echo) {
	WindowData &wd = windows[p_window];
	// X11 functions don't know what const is
	XKeyEvent *xkeyevent = p_event;

	if (wd.ime_in_progress) {
		return;
	}
	if (wd.ime_suppress_next_keyup) {
		wd.ime_suppress_next_keyup = false;
		if (xkeyevent->type != KeyPress) {
			return;
		}
	}

	// This code was pretty difficult to write.
	// The docs stink and every toolkit seems to
	// do it in a different way.

	/* Phase 1, obtain a proper keysym */

	// This was also very difficult to figure out.
	// You'd expect you could just use Keysym provided by
	// XKeycodeToKeysym to obtain internationalized
	// input.. WRONG!!
	// you must use XLookupString (???) which not only wastes
	// cycles generating an unnecessary string, but also
	// still works in half the cases. (won't handle deadkeys)
	// For more complex input methods (deadkeys and more advanced)
	// you have to use XmbLookupString (??).
	// So then you have to choose which of both results
	// you want to keep.
	// This is a real bizarreness and cpu waster.

	KeySym keysym_keycode = 0; // keysym used to find a keycode
	KeySym keysym_unicode = 0; // keysym used to find unicode

	// XLookupString returns keysyms usable as nice keycodes.
	char str[256] = {};
	XKeyEvent xkeyevent_no_mod = *xkeyevent;
	xkeyevent_no_mod.state &= ~ShiftMask;
	xkeyevent_no_mod.state &= ~ControlMask;
	XLookupString(xkeyevent, str, 255, &keysym_unicode, nullptr);
	XLookupString(&xkeyevent_no_mod, nullptr, 0, &keysym_keycode, nullptr);

	String keysym;
#ifdef XKB_ENABLED
	if (xkb_loaded_v08p) {
		KeySym keysym_unicode_nm = 0; // keysym used to find unicode
		XLookupString(&xkeyevent_no_mod, nullptr, 0, &keysym_unicode_nm, nullptr);
		keysym = String::chr(xkb_keysym_to_utf32(xkb_keysym_to_upper(keysym_unicode_nm)));
	}
#endif

	// Meanwhile, XLookupString returns keysyms useful for unicode.

	if (!xmbstring) {
		// keep a temporary buffer for the string
		xmbstring = (char *)memalloc(sizeof(char) * 8);
		xmblen = 8;
	}

	if (xkeyevent->type == KeyPress && wd.xic) {
		Status status;
#ifdef X_HAVE_UTF8_STRING
		int utf8len = 8;
		char *utf8string = (char *)memalloc(sizeof(char) * utf8len);
		int utf8bytes = Xutf8LookupString(wd.xic, xkeyevent, utf8string,
				utf8len - 1, &keysym_unicode, &status);
		if (status == XBufferOverflow) {
			utf8len = utf8bytes + 1;
			utf8string = (char *)memrealloc(utf8string, utf8len);
			utf8bytes = Xutf8LookupString(wd.xic, xkeyevent, utf8string,
					utf8len - 1, &keysym_unicode, &status);
		}
		utf8string[utf8bytes] = '\0';

		if (status == XLookupChars) {
			bool keypress = xkeyevent->type == KeyPress;
			Key keycode = KeyMappingX11::get_keycode(keysym_keycode);
			Key physical_keycode = KeyMappingX11::get_scancode(xkeyevent->keycode);

			if (keycode >= Key::A + 32 && keycode <= Key::Z + 32) {
				keycode -= 'a' - 'A';
			}

			String tmp;
			tmp.parse_utf8(utf8string, utf8bytes);
			for (int i = 0; i < tmp.length(); i++) {
				Ref<InputEventKey> k;
				k.instantiate();
				if (physical_keycode == Key::NONE && keycode == Key::NONE && tmp[i] == 0) {
					continue;
				}

				if (keycode == Key::NONE) {
					keycode = (Key)physical_keycode;
				}

				_get_key_modifier_state(xkeyevent->state, k);

				k->set_window_id(p_window);
				k->set_pressed(keypress);

				k->set_keycode(keycode);
				k->set_physical_keycode(physical_keycode);
				if (!keysym.is_empty()) {
					k->set_key_label(fix_key_label(keysym[0], keycode));
				} else {
					k->set_key_label(keycode);
				}
				if (keypress) {
					k->set_unicode(fix_unicode(tmp[i]));
				}

				k->set_echo(false);

				if (k->get_keycode() == Key::BACKTAB) {
					//make it consistent across platforms.
					k->set_keycode(Key::TAB);
					k->set_physical_keycode(Key::TAB);
					k->set_shift_pressed(true);
				}

				Input::get_singleton()->parse_input_event(k);
			}
			memfree(utf8string);
			return;
		}
		memfree(utf8string);
#else
		do {
			int mnbytes = XmbLookupString(xic, xkeyevent, xmbstring, xmblen - 1, &keysym_unicode, &status);
			xmbstring[mnbytes] = '\0';

			if (status == XBufferOverflow) {
				xmblen = mnbytes + 1;
				xmbstring = (char *)memrealloc(xmbstring, xmblen);
			}
		} while (status == XBufferOverflow);
#endif
#ifdef XKB_ENABLED
	} else if (xkeyevent->type == KeyPress && wd.xkb_state && xkb_loaded_v05p) {
		xkb_compose_feed_result res = xkb_compose_state_feed(wd.xkb_state, keysym_unicode);
		if (res == XKB_COMPOSE_FEED_ACCEPTED) {
			if (xkb_compose_state_get_status(wd.xkb_state) == XKB_COMPOSE_COMPOSED) {
				bool keypress = xkeyevent->type == KeyPress;
				Key keycode = KeyMappingX11::get_keycode(keysym_keycode);
				Key physical_keycode = KeyMappingX11::get_scancode(xkeyevent->keycode);
				KeyLocation key_location = KeyMappingX11::get_location(xkeyevent->keycode);

				if (keycode >= Key::A + 32 && keycode <= Key::Z + 32) {
					keycode -= 'a' - 'A';
				}

				char str_xkb[256] = {};
				int str_xkb_size = xkb_compose_state_get_utf8(wd.xkb_state, str_xkb, 255);

				String tmp;
				tmp.parse_utf8(str_xkb, str_xkb_size);
				for (int i = 0; i < tmp.length(); i++) {
					Ref<InputEventKey> k;
					k.instantiate();
					if (physical_keycode == Key::NONE && keycode == Key::NONE && tmp[i] == 0) {
						continue;
					}

					if (keycode == Key::NONE) {
						keycode = (Key)physical_keycode;
					}

					_get_key_modifier_state(xkeyevent->state, k);

					k->set_window_id(p_window);
					k->set_pressed(keypress);

					k->set_keycode(keycode);
					k->set_physical_keycode(physical_keycode);
					if (!keysym.is_empty()) {
						k->set_key_label(fix_key_label(keysym[0], keycode));
					} else {
						k->set_key_label(keycode);
					}
					if (keypress) {
						k->set_unicode(fix_unicode(tmp[i]));
					}

					k->set_location(key_location);

					k->set_echo(false);

					if (k->get_keycode() == Key::BACKTAB) {
						//make it consistent across platforms.
						k->set_keycode(Key::TAB);
						k->set_physical_keycode(Key::TAB);
						k->set_shift_pressed(true);
					}

					Input::get_singleton()->parse_input_event(k);
				}
				return;
			}
		}
#endif
	}

	/* Phase 2, obtain a Godot keycode from the keysym */

	// KeyMappingX11 just translated the X11 keysym to a PIGUI
	// keysym, so it works in all platforms the same.

	Key keycode = KeyMappingX11::get_keycode(keysym_keycode);
	Key physical_keycode = KeyMappingX11::get_scancode(xkeyevent->keycode);

	KeyLocation key_location = KeyMappingX11::get_location(xkeyevent->keycode);

	/* Phase 3, obtain a unicode character from the keysym */

	// KeyMappingX11 also translates keysym to unicode.
	// It does a binary search on a table to translate
	// most properly.
	char32_t unicode = keysym_unicode > 0 ? KeyMappingX11::get_unicode_from_keysym(keysym_unicode) : 0;

	/* Phase 4, determine if event must be filtered */

	// This seems to be a side-effect of using XIM.
	// XFilterEvent looks like a core X11 function,
	// but it's actually just used to see if we must
	// ignore a deadkey, or events XIM determines
	// must not reach the actual gui.
	// Guess it was a design problem of the extension

	bool keypress = xkeyevent->type == KeyPress;

	if (physical_keycode == Key::NONE && keycode == Key::NONE && unicode == 0) {
		return;
	}

	if (keycode == Key::NONE) {
		keycode = (Key)physical_keycode;
	}

	/* Phase 5, determine modifier mask */

	// No problems here, except I had no way to
	// know Mod1 was ALT and Mod4 was META (applekey/winkey)
	// just tried Mods until i found them.

	//print_verbose("mod1: "+itos(xkeyevent->state&Mod1Mask)+" mod 5: "+itos(xkeyevent->state&Mod5Mask));

	Ref<InputEventKey> k;
	k.instantiate();
	k->set_window_id(p_window);

	_get_key_modifier_state(xkeyevent->state, k);

	/* Phase 6, determine echo character */

	// Echo characters in X11 are a keyrelease and a keypress
	// one after the other with the (almot) same timestamp.
	// To detect them, i compare to the next event in list and
	// check that their difference in time is below a threshold.

	if (xkeyevent->type != KeyPress) {
		p_echo = false;

		// make sure there are events pending,
		// so this call won't block.
		if (p_event_index + 1 < p_events.size()) {
			XEvent &peek_event = p_events[p_event_index + 1];

			// I'm using a threshold of 5 msecs,
			// since sometimes there seems to be a little
			// jitter. I'm still not convinced that all this approach
			// is correct, but the xorg developers are
			// not very helpful today.

#define ABSDIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))
			::Time threshold = ABSDIFF(peek_event.xkey.time, xkeyevent->time);
#undef ABSDIFF
			if (peek_event.type == KeyPress && threshold < 5) {
				KeySym rk;
				XLookupString((XKeyEvent *)&peek_event, str, 256, &rk, nullptr);
				if (rk == keysym_keycode) {
					// Consume to next event.
					++p_event_index;
					_handle_key_event(p_window, (XKeyEvent *)&peek_event, p_events, p_event_index, true);
					return; //ignore current, echo next
				}
			}

			// use the time from peek_event so it always works
		}

		// save the time to check for echo when keypress happens
	}

	/* Phase 7, send event to Window */

	k->set_pressed(keypress);

	if (keycode >= Key::A + 32 && keycode <= Key::Z + 32) {
		keycode -= int('a' - 'A');
	}

	k->set_keycode(keycode);
	k->set_physical_keycode((Key)physical_keycode);
	if (!keysym.is_empty()) {
		k->set_key_label(fix_key_label(keysym[0], keycode));
	} else {
		k->set_key_label(keycode);
	}
	if (keypress) {
		k->set_unicode(fix_unicode(unicode));
	}

	k->set_location(key_location);

	k->set_echo(p_echo);

	if (k->get_keycode() == Key::BACKTAB) {
		//make it consistent across platforms.
		k->set_keycode(Key::TAB);
		k->set_physical_keycode(Key::TAB);
		k->set_shift_pressed(true);
	}

	//don't set mod state if modifier keys are released by themselves
	//else event.is_action() will not work correctly here
	if (!k->is_pressed()) {
		if (k->get_keycode() == Key::SHIFT) {
			k->set_shift_pressed(false);
		} else if (k->get_keycode() == Key::CTRL) {
			k->set_ctrl_pressed(false);
		} else if (k->get_keycode() == Key::ALT) {
			k->set_alt_pressed(false);
		} else if (k->get_keycode() == Key::META) {
			k->set_meta_pressed(false);
		}
	}

	bool last_is_pressed = Input::get_singleton()->is_key_pressed(k->get_keycode());
	if (k->is_pressed()) {
		if (last_is_pressed) {
			k->set_echo(true);
		}
	}

	Input::get_singleton()->parse_input_event(k);
}

Atom DisplayServerX11::_process_selection_request_target(Atom p_target, Window p_requestor, Atom p_property, Atom p_selection) const {
	if (p_target == XInternAtom(x11_display, "TARGETS", 0)) {
		// Request to list all supported targets.
		Atom data[9];
		data[0] = XInternAtom(x11_display, "TARGETS", 0);
		data[1] = XInternAtom(x11_display, "SAVE_TARGETS", 0);
		data[2] = XInternAtom(x11_display, "MULTIPLE", 0);
		data[3] = XInternAtom(x11_display, "UTF8_STRING", 0);
		data[4] = XInternAtom(x11_display, "COMPOUND_TEXT", 0);
		data[5] = XInternAtom(x11_display, "TEXT", 0);
		data[6] = XA_STRING;
		data[7] = XInternAtom(x11_display, "text/plain;charset=utf-8", 0);
		data[8] = XInternAtom(x11_display, "text/plain", 0);

		XChangeProperty(x11_display,
				p_requestor,
				p_property,
				XA_ATOM,
				32,
				PropModeReplace,
				(unsigned char *)&data,
				sizeof(data) / sizeof(data[0]));
		return p_property;
	} else if (p_target == XInternAtom(x11_display, "SAVE_TARGETS", 0)) {
		// Request to check if SAVE_TARGETS is supported, nothing special to do.
		XChangeProperty(x11_display,
				p_requestor,
				p_property,
				XInternAtom(x11_display, "NULL", False),
				32,
				PropModeReplace,
				nullptr,
				0);
		return p_property;
	} else if (p_target == XInternAtom(x11_display, "UTF8_STRING", 0) ||
			p_target == XInternAtom(x11_display, "COMPOUND_TEXT", 0) ||
			p_target == XInternAtom(x11_display, "TEXT", 0) ||
			p_target == XA_STRING ||
			p_target == XInternAtom(x11_display, "text/plain;charset=utf-8", 0) ||
			p_target == XInternAtom(x11_display, "text/plain", 0)) {
		// Directly using internal clipboard because we know our window
		// is the owner during a selection request.
		CharString clip;
		static const char *target_type = "PRIMARY";
		if (p_selection != None && get_atom_name(x11_display, p_selection) == target_type) {
			clip = internal_clipboard_primary.utf8();
		} else {
			clip = internal_clipboard.utf8();
		}
		XChangeProperty(x11_display,
				p_requestor,
				p_property,
				p_target,
				8,
				PropModeReplace,
				(unsigned char *)clip.get_data(),
				clip.length());
		return p_property;
	} else {
		char *target_name = XGetAtomName(x11_display, p_target);
		print_verbose(vformat("Target '%s' not supported.", target_name));
		if (target_name) {
			XFree(target_name);
		}
		return None;
	}
}

void DisplayServerX11::_handle_selection_request_event(XSelectionRequestEvent *p_event) const {
	XEvent respond;
	if (p_event->target == XInternAtom(x11_display, "MULTIPLE", 0)) {
		// Request for multiple target conversions at once.
		Atom atom_pair = XInternAtom(x11_display, "ATOM_PAIR", False);
		respond.xselection.property = None;

		Atom type;
		int format;
		unsigned long len;
		unsigned long remaining;
		unsigned char *data = nullptr;
		if (XGetWindowProperty(x11_display, p_event->requestor, p_event->property, 0, LONG_MAX, False, atom_pair, &type, &format, &len, &remaining, &data) == Success) {
			if ((len >= 2) && data) {
				Atom *targets = (Atom *)data;
				for (uint64_t i = 0; i < len; i += 2) {
					Atom target = targets[i];
					Atom &property = targets[i + 1];
					property = _process_selection_request_target(target, p_event->requestor, property, p_event->selection);
				}

				XChangeProperty(x11_display,
						p_event->requestor,
						p_event->property,
						atom_pair,
						32,
						PropModeReplace,
						(unsigned char *)targets,
						len);

				respond.xselection.property = p_event->property;
			}
			XFree(data);
		}
	} else {
		// Request for target conversion.
		respond.xselection.property = _process_selection_request_target(p_event->target, p_event->requestor, p_event->property, p_event->selection);
	}

	respond.xselection.type = SelectionNotify;
	respond.xselection.display = p_event->display;
	respond.xselection.requestor = p_event->requestor;
	respond.xselection.selection = p_event->selection;
	respond.xselection.target = p_event->target;
	respond.xselection.time = p_event->time;

	XSendEvent(x11_display, p_event->requestor, True, NoEventMask, &respond);
	XFlush(x11_display);
}

int DisplayServerX11::_xim_preedit_start_callback(::XIM xim, ::XPointer client_data,
		::XPointer call_data) {
	DisplayServerX11 *ds = reinterpret_cast<DisplayServerX11 *>(client_data);
	WindowID window_id = ds->_get_focused_window_or_popup();
	WindowData &wd = ds->windows[window_id];
	if (wd.ime_active) {
		wd.ime_in_progress = true;
	}

	return -1; // Allow preedit strings of any length (no limit).
}

void DisplayServerX11::_xim_preedit_done_callback(::XIM xim, ::XPointer client_data,
		::XPointer call_data) {
	DisplayServerX11 *ds = reinterpret_cast<DisplayServerX11 *>(client_data);
	WindowID window_id = ds->_get_focused_window_or_popup();
	WindowData &wd = ds->windows[window_id];
	if (wd.ime_active) {
		wd.ime_in_progress = false;
		wd.ime_suppress_next_keyup = true;
	}
}

void DisplayServerX11::_xim_preedit_draw_callback(::XIM xim, ::XPointer client_data,
		::XIMPreeditDrawCallbackStruct *call_data) {
	DisplayServerX11 *ds = reinterpret_cast<DisplayServerX11 *>(client_data);
	WindowID window_id = ds->_get_focused_window_or_popup();
	WindowData &wd = ds->windows[window_id];

	XIMText *xim_text = call_data->text;
	if (wd.ime_active) {
		if (xim_text != nullptr) {
			String changed_text;
			if (xim_text->encoding_is_wchar) {
				changed_text = String(xim_text->string.wide_char);
			} else {
				changed_text.parse_utf8(xim_text->string.multi_byte);
			}

			if (call_data->chg_length < 0) {
				ds->im_text = ds->im_text.substr(0, call_data->chg_first) + changed_text;
			} else {
				ds->im_text = ds->im_text.substr(0, call_data->chg_first) + changed_text + ds->im_text.substr(call_data->chg_length);
			}

			// Find the start and end of the selection.
			int start = 0, count = 0;
			for (int i = 0; i < xim_text->length; i++) {
				if (xim_text->feedback[i] & XIMReverse) {
					if (count == 0) {
						start = i;
						count = 1;
					} else {
						count++;
					}
				}
			}
			if (count > 0) {
				ds->im_selection = Point2i(start + call_data->chg_first, count);
			} else {
				ds->im_selection = Point2i(call_data->caret, 0);
			}
		} else {
			ds->im_text = String();
			ds->im_selection = Point2i();
		}

		callable_mp((Object *)OS_Unix::get_singleton()->get_main_loop(), &Object::notification).call_deferred(MainLoop::NOTIFICATION_OS_IME_UPDATE, false);
	}
}

void DisplayServerX11::_xim_preedit_caret_callback(::XIM xim, ::XPointer client_data,
		::XIMPreeditCaretCallbackStruct *call_data) {
}

void DisplayServerX11::_xim_destroy_callback(::XIM im, ::XPointer client_data,
		::XPointer call_data) {
	WARN_PRINT("Input method stopped");
	DisplayServerX11 *ds = reinterpret_cast<DisplayServerX11 *>(client_data);
	ds->xim = nullptr;

	for (KeyValue<WindowID, WindowData> &E : ds->windows) {
		E.value.xic = nullptr;
	}
}

void DisplayServerX11::_window_changed(XEvent *event) {
	WindowID window_id = MAIN_WINDOW_ID;

	// Assign the event to the relevant window
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (event->xany.window == E.value.x11_window) {
			window_id = E.key;
			break;
		}
	}

	Rect2i new_rect;

	WindowData &wd = windows[window_id];
	if (wd.x11_window != event->xany.window) { // Check if the correct window, in case it was not main window or anything else
		return;
	}

	// Query display server about a possible new window state.
	wd.fullscreen = _window_fullscreen_check(window_id);
	wd.maximized = _window_maximize_check(window_id, "_NET_WM_STATE") && !wd.fullscreen;
	wd.minimized = _window_minimize_check(window_id) && !wd.fullscreen && !wd.maximized;

	// Readjusting the window position if the window is being reparented by the window manager for decoration
	Window root, parent, *children;
	unsigned int nchildren;
	if (XQueryTree(x11_display, wd.x11_window, &root, &parent, &children, &nchildren) && wd.parent != parent) {
		wd.parent = parent;
		window_set_position(wd.position, window_id);
	}
	XFree(children);

	{
		//the position in xconfigure is not useful here, obtain it manually
		int x = 0, y = 0;
		Window child;
		XTranslateCoordinates(x11_display, wd.x11_window, DefaultRootWindow(x11_display), 0, 0, &x, &y, &child);
		new_rect.position.x = x;
		new_rect.position.y = y;

		new_rect.size.width = event->xconfigure.width;
		new_rect.size.height = event->xconfigure.height;
	}

	if (new_rect == Rect2i(wd.position, wd.size)) {
		return;
	}

	wd.position = new_rect.position;
	wd.size = new_rect.size;

#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_size(window_id, wd.size.width, wd.size.height);
	}
#endif
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->window_resize(window_id, wd.size.width, wd.size.height);
	}
	if (gl_manager_egl) {
		gl_manager_egl->window_resize(window_id, wd.size.width, wd.size.height);
	}
#endif

	if (wd.rect_changed_callback.is_valid()) {
		wd.rect_changed_callback.call(new_rect);
	}
}

DisplayServer::WindowID DisplayServerX11::_get_focused_window_or_popup() const {
	const List<WindowID>::Element *E = popup_list.back();
	if (E) {
		return E->get();
	}

	return last_focused_window;
}

void DisplayServerX11::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	static_cast<DisplayServerX11 *>(get_singleton())->_dispatch_input_event(p_event);
}

void DisplayServerX11::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	{
		List<WindowID>::Element *E = popup_list.back();
		if (E && Object::cast_to<InputEventKey>(*p_event)) {
			// Redirect keyboard input to active popup.
			if (windows.has(E->get())) {
				Callable callable = windows[E->get()].input_event_callback;
				if (callable.is_valid()) {
					callable.call(p_event);
				}
			}
			return;
		}
	}

	Ref<InputEventFromWindow> event_from_window = p_event;
	if (event_from_window.is_valid() && event_from_window->get_window_id() != INVALID_WINDOW_ID) {
		// Send to a single window.
		if (windows.has(event_from_window->get_window_id())) {
			Callable callable = windows[event_from_window->get_window_id()].input_event_callback;
			if (callable.is_valid()) {
				callable.call(p_event);
			}
		}
	} else {
		// Send to all windows.
		for (KeyValue<WindowID, WindowData> &E : windows) {
			Callable callable = E.value.input_event_callback;
			if (callable.is_valid()) {
				callable.call(p_event);
			}
		}
	}
}

void DisplayServerX11::_send_window_event(const WindowData &wd, WindowEvent p_event) {
	if (wd.event_callback.is_valid()) {
		Variant event = int(p_event);
		wd.event_callback.call(event);
	}
}

void DisplayServerX11::_set_input_focus(Window p_window, int p_revert_to) {
	Window focused_window;
	int focus_ret_state;
	XGetInputFocus(x11_display, &focused_window, &focus_ret_state);

	// Only attempt to change focus if the window isn't already focused, in order to
	// prevent issues with Godot stealing input focus with alternative window managers.
	if (p_window != focused_window) {
		XSetInputFocus(x11_display, p_window, p_revert_to, CurrentTime);
	}
}

void DisplayServerX11::_poll_events_thread(void *ud) {
	DisplayServerX11 *display_server = static_cast<DisplayServerX11 *>(ud);
	display_server->_poll_events();
}

Bool DisplayServerX11::_predicate_all_events(Display *display, XEvent *event, XPointer arg) {
	// Just accept all events.
	return True;
}

bool DisplayServerX11::_wait_for_events() const {
	int x11_fd = ConnectionNumber(x11_display);
	fd_set in_fds;

	XFlush(x11_display);

	FD_ZERO(&in_fds);
	FD_SET(x11_fd, &in_fds);

	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 1;

	// Wait for next event or timeout.
	int num_ready_fds = select(x11_fd + 1, &in_fds, nullptr, nullptr, &tv);

	if (num_ready_fds > 0) {
		// Event received.
		return true;
	} else {
		// Error or timeout.
		if (num_ready_fds < 0) {
			ERR_PRINT("_wait_for_events: select error: " + itos(errno));
		}
		return false;
	}
}

void DisplayServerX11::_poll_events() {
	while (!events_thread_done.is_set()) {
		_wait_for_events();

		// Process events from the queue.
		{
			MutexLock mutex_lock(events_mutex);

			_check_pending_events(polled_events);
		}
	}
}

void DisplayServerX11::_check_pending_events(LocalVector<XEvent> &r_events) {
	// Flush to make sure to gather all pending events.
	XFlush(x11_display);

	// Non-blocking wait for next event and remove it from the queue.
	XEvent ev = {};
	while (XCheckIfEvent(x11_display, &ev, _predicate_all_events, nullptr)) {
		// Check if the input manager wants to process the event.
		if (XFilterEvent(&ev, None)) {
			// Event has been filtered by the Input Manager,
			// it has to be ignored and a new one will be received.
			continue;
		}

		// Handle selection request events directly in the event thread, because
		// communication through the x server takes several events sent back and forth
		// and we don't want to block other programs while processing only one each frame.
		if (ev.type == SelectionRequest) {
			_handle_selection_request_event(&(ev.xselectionrequest));
			continue;
		}

		r_events.push_back(ev);
	}
}

DisplayServer::WindowID DisplayServerX11::window_get_active_popup() const {
	const List<WindowID>::Element *E = popup_list.back();
	if (E) {
		return E->get();
	} else {
		return INVALID_WINDOW_ID;
	}
}

void DisplayServerX11::window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.parent_safe_rect = p_rect;
}

Rect2i DisplayServerX11::window_get_popup_safe_rect(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Rect2i());
	const WindowData &wd = windows[p_window];
	return wd.parent_safe_rect;
}

void DisplayServerX11::popup_open(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	bool has_popup_ancestor = false;
	WindowID transient_root = p_window;
	while (true) {
		WindowID parent = windows[transient_root].transient_parent;
		if (parent == INVALID_WINDOW_ID) {
			break;
		} else {
			transient_root = parent;
			if (windows[parent].is_popup) {
				has_popup_ancestor = true;
				break;
			}
		}
	}

	// Detect tooltips and other similar popups that shouldn't block input to their parent.
	bool ignores_input = window_get_flag(WINDOW_FLAG_NO_FOCUS, p_window) && window_get_flag(WINDOW_FLAG_MOUSE_PASSTHROUGH, p_window);

	WindowData &wd = windows[p_window];
	if (wd.is_popup || (has_popup_ancestor && !ignores_input)) {
		// Find current popup parent, or root popup if new window is not transient.
		List<WindowID>::Element *C = nullptr;
		List<WindowID>::Element *E = popup_list.back();
		while (E) {
			if (wd.transient_parent != E->get() || wd.transient_parent == INVALID_WINDOW_ID) {
				C = E;
				E = E->prev();
			} else {
				break;
			}
		}
		if (C) {
			_send_window_event(windows[C->get()], DisplayServerX11::WINDOW_EVENT_CLOSE_REQUEST);
		}

		time_since_popup = OS::get_singleton()->get_ticks_msec();
		popup_list.push_back(p_window);
	}
}

void DisplayServerX11::popup_close(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	List<WindowID>::Element *E = popup_list.find(p_window);
	while (E) {
		List<WindowID>::Element *F = E->next();
		WindowID win_id = E->get();
		popup_list.erase(E);

		if (win_id != p_window) {
			// Only request close on related windows, not this window.  We are already processing it.
			_send_window_event(windows[win_id], DisplayServerX11::WINDOW_EVENT_CLOSE_REQUEST);
		}
		E = F;
	}
}

bool DisplayServerX11::mouse_process_popups() {
	_THREAD_SAFE_METHOD_

	if (popup_list.is_empty()) {
		return false;
	}

	uint64_t delta = OS::get_singleton()->get_ticks_msec() - time_since_popup;
	if (delta < 250) {
		return false;
	}

	int number_of_screens = XScreenCount(x11_display);
	bool closed = false;
	for (int i = 0; i < number_of_screens; i++) {
		Window root, child;
		int root_x, root_y, win_x, win_y;
		unsigned int mask;
		if (XQueryPointer(x11_display, XRootWindow(x11_display, i), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
			XWindowAttributes root_attrs;
			XGetWindowAttributes(x11_display, root, &root_attrs);
			Vector2i pos = Vector2i(root_attrs.x + root_x, root_attrs.y + root_y);
			if (mask != last_mouse_monitor_mask) {
				if (((mask & Button1Mask) || (mask & Button2Mask) || (mask & Button3Mask) || (mask & Button4Mask) || (mask & Button5Mask))) {
					List<WindowID>::Element *C = nullptr;
					List<WindowID>::Element *E = popup_list.back();
					// Find top popup to close.
					while (E) {
						// Popup window area.
						Rect2i win_rect = Rect2i(window_get_position_with_decorations(E->get()), window_get_size_with_decorations(E->get()));
						// Area of the parent window, which responsible for opening sub-menu.
						Rect2i safe_rect = window_get_popup_safe_rect(E->get());
						if (win_rect.has_point(pos)) {
							break;
						} else if (safe_rect != Rect2i() && safe_rect.has_point(pos)) {
							break;
						} else {
							C = E;
							E = E->prev();
						}
					}
					if (C) {
						_send_window_event(windows[C->get()], DisplayServerX11::WINDOW_EVENT_CLOSE_REQUEST);
						closed = true;
					}
				}
			}
			last_mouse_monitor_mask = mask;
		}
	}
	return closed;
}

bool DisplayServerX11::_window_focus_check() {
	Window focused_window;
	int focus_ret_state;
	XGetInputFocus(x11_display, &focused_window, &focus_ret_state);

	bool has_focus = false;
	for (const KeyValue<int, DisplayServerX11::WindowData> &wid : windows) {
		if (wid.value.x11_window == focused_window || (wid.value.xic && wid.value.ime_active && wid.value.x11_xim_window == focused_window)) {
			has_focus = true;
			break;
		}
	}

	return has_focus;
}

void DisplayServerX11::process_events() {
	ERR_FAIL_COND(!Thread::is_main_thread());

	_THREAD_SAFE_LOCK_

#ifdef DISPLAY_SERVER_X11_DEBUG_LOGS_ENABLED
	static int frame = 0;
	++frame;
#endif

	bool ignore_events = mouse_process_popups();

	if (app_focused) {
		//verify that one of the windows has focus, else send focus out notification
		bool focus_found = false;
		for (const KeyValue<WindowID, WindowData> &E : windows) {
			if (E.value.focused) {
				focus_found = true;
				break;
			}
		}

		if (!focus_found) {
			uint64_t delta = OS::get_singleton()->get_ticks_msec() - time_since_no_focus;

			if (delta > 250) {
				//X11 can go between windows and have no focus for a while, when creating them or something else. Use this as safety to avoid unnecessary focus in/outs.
				if (OS::get_singleton()->get_main_loop()) {
					DEBUG_LOG_X11("All focus lost, triggering NOTIFICATION_APPLICATION_FOCUS_OUT\n");
					OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
				}
				app_focused = false;
			}
		} else {
			time_since_no_focus = OS::get_singleton()->get_ticks_msec();
		}
	}

	do_mouse_warp = false;

	// Is the current mouse mode one where it needs to be grabbed.
	bool mouse_mode_grab = mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN;

	xi.pressure = 0;
	xi.tilt = Vector2();
	xi.pressure_supported = false;

	LocalVector<XEvent> events;
	{
		// Block events polling while flushing events.
		MutexLock mutex_lock(events_mutex);
		events = polled_events;
		polled_events.clear();

		// Check for more pending events to avoid an extra frame delay.
		_check_pending_events(events);
	}

	for (uint32_t event_index = 0; event_index < events.size(); ++event_index) {
		XEvent &event = events[event_index];

		bool ime_window_event = false;
		WindowID window_id = MAIN_WINDOW_ID;

		// Assign the event to the relevant window
		for (const KeyValue<WindowID, WindowData> &E : windows) {
			if (event.xany.window == E.value.x11_window) {
				window_id = E.key;
				break;
			}
			if (event.xany.window == E.value.x11_xim_window) {
				window_id = E.key;
				ime_window_event = true;
				break;
			}
		}

		if (XGetEventData(x11_display, &event.xcookie)) {
			if (event.xcookie.type == GenericEvent && event.xcookie.extension == xi.opcode) {
				XIDeviceEvent *event_data = (XIDeviceEvent *)event.xcookie.data;
				switch (event_data->evtype) {
					case XI_HierarchyChanged:
					case XI_DeviceChanged: {
						_refresh_device_info();
					} break;
					case XI_RawMotion: {
						if (ime_window_event || ignore_events) {
							break;
						}
						XIRawEvent *raw_event = (XIRawEvent *)event_data;
						int device_id = raw_event->sourceid;

						// Determine the axis used (called valuators in XInput for some forsaken reason)
						//  Mask is a bitmask indicating which axes are involved.
						//  We are interested in the values of axes 0 and 1.
						if (raw_event->valuators.mask_len <= 0) {
							break;
						}

						const double *values = raw_event->raw_values;

						double rel_x = 0.0;
						double rel_y = 0.0;

						if (XIMaskIsSet(raw_event->valuators.mask, VALUATOR_ABSX)) {
							rel_x = *values;
							values++;
						}

						if (XIMaskIsSet(raw_event->valuators.mask, VALUATOR_ABSY)) {
							rel_y = *values;
							values++;
						}

						if (XIMaskIsSet(raw_event->valuators.mask, VALUATOR_PRESSURE)) {
							HashMap<int, Vector2>::Iterator pen_pressure = xi.pen_pressure_range.find(device_id);
							if (pen_pressure) {
								Vector2 pen_pressure_range = pen_pressure->value;
								if (pen_pressure_range != Vector2()) {
									xi.pressure_supported = true;
									xi.pressure = (*values - pen_pressure_range[0]) /
											(pen_pressure_range[1] - pen_pressure_range[0]);
								}
							}

							values++;
						}

						if (XIMaskIsSet(raw_event->valuators.mask, VALUATOR_TILTX)) {
							HashMap<int, Vector2>::Iterator pen_tilt_x = xi.pen_tilt_x_range.find(device_id);
							if (pen_tilt_x) {
								Vector2 pen_tilt_x_range = pen_tilt_x->value;
								if (pen_tilt_x_range[0] != 0 && *values < 0) {
									xi.tilt.x = *values / -pen_tilt_x_range[0];
								} else if (pen_tilt_x_range[1] != 0) {
									xi.tilt.x = *values / pen_tilt_x_range[1];
								}
							}

							values++;
						}

						if (XIMaskIsSet(raw_event->valuators.mask, VALUATOR_TILTY)) {
							HashMap<int, Vector2>::Iterator pen_tilt_y = xi.pen_tilt_y_range.find(device_id);
							if (pen_tilt_y) {
								Vector2 pen_tilt_y_range = pen_tilt_y->value;
								if (pen_tilt_y_range[0] != 0 && *values < 0) {
									xi.tilt.y = *values / -pen_tilt_y_range[0];
								} else if (pen_tilt_y_range[1] != 0) {
									xi.tilt.y = *values / pen_tilt_y_range[1];
								}
							}

							values++;
						}

						HashMap<int, bool>::Iterator pen_inverted = xi.pen_inverted_devices.find(device_id);
						if (pen_inverted) {
							xi.pen_inverted = pen_inverted->value;
						}

						// https://bugs.freedesktop.org/show_bug.cgi?id=71609
						// http://lists.libsdl.org/pipermail/commits-libsdl.org/2015-June/000282.html
						if (raw_event->time == xi.last_relative_time && rel_x == xi.relative_motion.x && rel_y == xi.relative_motion.y) {
							break; // Flush duplicate to avoid overly fast motion
						}

						xi.old_raw_pos.x = xi.raw_pos.x;
						xi.old_raw_pos.y = xi.raw_pos.y;
						xi.raw_pos.x = rel_x;
						xi.raw_pos.y = rel_y;

						HashMap<int, Vector2>::Iterator abs_info = xi.absolute_devices.find(device_id);

						if (abs_info) {
							// Absolute mode device
							Vector2 mult = abs_info->value;

							xi.relative_motion.x += (xi.raw_pos.x - xi.old_raw_pos.x) * mult.x;
							xi.relative_motion.y += (xi.raw_pos.y - xi.old_raw_pos.y) * mult.y;
						} else {
							// Relative mode device
							xi.relative_motion.x = xi.raw_pos.x;
							xi.relative_motion.y = xi.raw_pos.y;
						}

						xi.last_relative_time = raw_event->time;
					} break;
#ifdef TOUCH_ENABLED
					case XI_TouchBegin:
					case XI_TouchEnd: {
						if (ime_window_event || ignore_events) {
							break;
						}
						bool is_begin = event_data->evtype == XI_TouchBegin;

						int index = event_data->detail;
						Vector2 pos = Vector2(event_data->event_x, event_data->event_y);

						Ref<InputEventScreenTouch> st;
						st.instantiate();
						st->set_window_id(window_id);
						st->set_index(index);
						st->set_position(pos);
						st->set_pressed(is_begin);

						if (is_begin) {
							if (xi.state.has(index)) { // Defensive
								break;
							}
							xi.state[index] = pos;
							if (xi.state.size() == 1) {
								// X11 may send a motion event when a touch gesture begins, that would result
								// in a spurious mouse motion event being sent to Godot; remember it to be able to filter it out
								xi.mouse_pos_to_filter = pos;
							}
							Input::get_singleton()->parse_input_event(st);
						} else {
							if (!xi.state.has(index)) { // Defensive
								break;
							}
							xi.state.erase(index);
							Input::get_singleton()->parse_input_event(st);
						}
					} break;

					case XI_TouchUpdate: {
						if (ime_window_event || ignore_events) {
							break;
						}

						int index = event_data->detail;
						Vector2 pos = Vector2(event_data->event_x, event_data->event_y);

						HashMap<int, Vector2>::Iterator curr_pos_elem = xi.state.find(index);
						if (!curr_pos_elem) { // Defensive
							break;
						}

						if (curr_pos_elem->value != pos) {
							Ref<InputEventScreenDrag> sd;
							sd.instantiate();
							sd->set_window_id(window_id);
							sd->set_index(index);
							sd->set_position(pos);
							sd->set_relative(pos - curr_pos_elem->value);
							sd->set_relative_screen_position(sd->get_relative());
							Input::get_singleton()->parse_input_event(sd);

							curr_pos_elem->value = pos;
						}
					} break;
#endif
				}
			}
		}
		XFreeEventData(x11_display, &event.xcookie);

		switch (event.type) {
			case MapNotify: {
				DEBUG_LOG_X11("[%u] MapNotify window=%lu (%u) \n", frame, event.xmap.window, window_id);
				if (ime_window_event) {
					break;
				}

				const WindowData &wd = windows[window_id];

				XWindowAttributes xwa;
				XSync(x11_display, False);
				XGetWindowAttributes(x11_display, wd.x11_window, &xwa);

				// Set focus when menu window is started.
				// RevertToPointerRoot is used to make sure we don't lose all focus in case
				// a subwindow and its parent are both destroyed.
				if ((xwa.map_state == IsViewable) && !wd.no_focus && !wd.is_popup && _window_focus_check()) {
					_set_input_focus(wd.x11_window, RevertToPointerRoot);
				}

				// Have we failed to set fullscreen while the window was unmapped?
				_validate_mode_on_map(window_id);
			} break;

			case Expose: {
				DEBUG_LOG_X11("[%u] Expose window=%lu (%u), count='%u' \n", frame, event.xexpose.window, window_id, event.xexpose.count);
				if (ime_window_event) {
					break;
				}

				windows[window_id].fullscreen = _window_fullscreen_check(window_id);

				Main::force_redraw();
			} break;

			case NoExpose: {
				DEBUG_LOG_X11("[%u] NoExpose drawable=%lu (%u) \n", frame, event.xnoexpose.drawable, window_id);
				if (ime_window_event) {
					break;
				}

				windows[window_id].minimized = true;
			} break;

			case VisibilityNotify: {
				DEBUG_LOG_X11("[%u] VisibilityNotify window=%lu (%u), state=%u \n", frame, event.xvisibility.window, window_id, event.xvisibility.state);
				if (ime_window_event) {
					break;
				}

				windows[window_id].minimized = _window_minimize_check(window_id);
			} break;

			case LeaveNotify: {
				DEBUG_LOG_X11("[%u] LeaveNotify window=%lu (%u), mode='%u' \n", frame, event.xcrossing.window, window_id, event.xcrossing.mode);
				if (ime_window_event) {
					break;
				}

				if (!mouse_mode_grab && window_mouseover_id == window_id) {
					window_mouseover_id = INVALID_WINDOW_ID;
					_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_EXIT);
				}

			} break;

			case EnterNotify: {
				DEBUG_LOG_X11("[%u] EnterNotify window=%lu (%u), mode='%u' \n", frame, event.xcrossing.window, window_id, event.xcrossing.mode);
				if (ime_window_event) {
					break;
				}

				if (!mouse_mode_grab && window_mouseover_id != window_id) {
					if (window_mouseover_id != INVALID_WINDOW_ID) {
						_send_window_event(windows[window_mouseover_id], WINDOW_EVENT_MOUSE_EXIT);
					}
					window_mouseover_id = window_id;
					_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_ENTER);
				}
			} break;

			case FocusIn: {
				DEBUG_LOG_X11("[%u] FocusIn window=%lu (%u), mode='%u' \n", frame, event.xfocus.window, window_id, event.xfocus.mode);
				if (ime_window_event || (event.xfocus.detail == NotifyInferior)) {
					break;
				}

				WindowData &wd = windows[window_id];
				last_focused_window = window_id;
				wd.focused = true;

				// Keep track of focus order for overlapping windows.
				static unsigned int focus_order = 0;
				wd.focus_order = ++focus_order;

				_send_window_event(wd, WINDOW_EVENT_FOCUS_IN);

				if (mouse_mode_grab) {
					// Show and update the cursor if confined and the window regained focus.

					for (const KeyValue<WindowID, WindowData> &E : windows) {
						if (mouse_mode == MOUSE_MODE_CONFINED) {
							XUndefineCursor(x11_display, E.value.x11_window);
						} else if (mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) { // Or re-hide it.
							XDefineCursor(x11_display, E.value.x11_window, null_cursor);
						}

						XGrabPointer(
								x11_display, E.value.x11_window, True,
								ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
								GrabModeAsync, GrabModeAsync, E.value.x11_window, None, CurrentTime);
					}
				}
#ifdef TOUCH_ENABLED
				// Grab touch devices to avoid OS gesture interference
				/*for (int i = 0; i < xi.touch_devices.size(); ++i) {
					XIGrabDevice(x11_display, xi.touch_devices[i], x11_window, CurrentTime, None, XIGrabModeAsync, XIGrabModeAsync, False, &xi.touch_event_mask);
				}*/
#endif

				if (!app_focused) {
					if (OS::get_singleton()->get_main_loop()) {
						OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
					}
					app_focused = true;
				}
			} break;

			case FocusOut: {
				DEBUG_LOG_X11("[%u] FocusOut window=%lu (%u), mode='%u' \n", frame, event.xfocus.window, window_id, event.xfocus.mode);
				WindowData &wd = windows[window_id];
				if (ime_window_event || (event.xfocus.detail == NotifyInferior)) {
					break;
				}
				if (wd.ime_active) {
					MutexLock mutex_lock(events_mutex);
					XUnsetICFocus(wd.xic);
					XUnmapWindow(x11_display, wd.x11_xim_window);
					wd.ime_active = false;
					im_text = String();
					im_selection = Vector2i();
					OS_Unix::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
				}
				wd.focused = false;

				Input::get_singleton()->release_pressed_events();
				_send_window_event(wd, WINDOW_EVENT_FOCUS_OUT);

				if (mouse_mode_grab) {
					for (const KeyValue<WindowID, WindowData> &E : windows) {
						//dear X11, I try, I really try, but you never work, you do whatever you want.
						if (mouse_mode == MOUSE_MODE_CAPTURED) {
							// Show the cursor if we're in captured mode so it doesn't look weird.
							XUndefineCursor(x11_display, E.value.x11_window);
						}
					}
					XUngrabPointer(x11_display, CurrentTime);
				}
#ifdef TOUCH_ENABLED
				// Ungrab touch devices so input works as usual while we are unfocused
				/*for (int i = 0; i < xi.touch_devices.size(); ++i) {
					XIUngrabDevice(x11_display, xi.touch_devices[i], CurrentTime);
				}*/

				// Release every pointer to avoid sticky points
				for (const KeyValue<int, Vector2> &E : xi.state) {
					Ref<InputEventScreenTouch> st;
					st.instantiate();
					st->set_index(E.key);
					st->set_window_id(window_id);
					st->set_position(E.value);
					Input::get_singleton()->parse_input_event(st);
				}
				xi.state.clear();
#endif
			} break;

			case ConfigureNotify: {
				DEBUG_LOG_X11("[%u] ConfigureNotify window=%lu (%u), event=%lu, above=%lu, override_redirect=%u \n", frame, event.xconfigure.window, window_id, event.xconfigure.event, event.xconfigure.above, event.xconfigure.override_redirect);
				if (event.xconfigure.window == windows[window_id].x11_xim_window) {
					break;
				}

				_window_changed(&event);
			} break;

			case ButtonPress:
			case ButtonRelease: {
				if (ime_window_event || ignore_events) {
					break;
				}
				/* exit in case of a mouse button press */
				last_timestamp = event.xbutton.time;
				if (mouse_mode == MOUSE_MODE_CAPTURED) {
					event.xbutton.x = last_mouse_pos.x;
					event.xbutton.y = last_mouse_pos.y;
				}

				Ref<InputEventMouseButton> mb;
				mb.instantiate();

				mb->set_window_id(window_id);
				_get_key_modifier_state(event.xbutton.state, mb);
				mb->set_button_index((MouseButton)event.xbutton.button);
				if (mb->get_button_index() == MouseButton::RIGHT) {
					mb->set_button_index(MouseButton::MIDDLE);
				} else if (mb->get_button_index() == MouseButton::MIDDLE) {
					mb->set_button_index(MouseButton::RIGHT);
				}
				mb->set_position(Vector2(event.xbutton.x, event.xbutton.y));
				mb->set_global_position(mb->get_position());

				mb->set_pressed((event.type == ButtonPress));

				if (mb->is_pressed() && mb->get_button_index() >= MouseButton::WHEEL_UP && mb->get_button_index() <= MouseButton::WHEEL_RIGHT) {
					MouseButtonMask mask = mouse_button_to_mask(mb->get_button_index());
					BitField<MouseButtonMask> scroll_mask = mouse_get_button_state();
					scroll_mask.set_flag(mask);
					mb->set_button_mask(scroll_mask);
				} else {
					mb->set_button_mask(mouse_get_button_state());
				}

				const WindowData &wd = windows[window_id];

				if (event.type == ButtonPress) {
					DEBUG_LOG_X11("[%u] ButtonPress window=%lu (%u), button_index=%u \n", frame, event.xbutton.window, window_id, mb->get_button_index());

					// Ensure window focus on click.
					// RevertToPointerRoot is used to make sure we don't lose all focus in case
					// a subwindow and its parent are both destroyed.
					if (!wd.no_focus && !wd.is_popup) {
						_set_input_focus(wd.x11_window, RevertToPointerRoot);
					}

					uint64_t diff = OS::get_singleton()->get_ticks_usec() / 1000 - last_click_ms;

					if (mb->get_button_index() == last_click_button_index) {
						if (diff < 400 && Vector2(last_click_pos).distance_to(Vector2(event.xbutton.x, event.xbutton.y)) < 5) {
							last_click_ms = 0;
							last_click_pos = Point2i(-100, -100);
							last_click_button_index = MouseButton::NONE;
							mb->set_double_click(true);
						}

					} else if (mb->get_button_index() < MouseButton::WHEEL_UP || mb->get_button_index() > MouseButton::WHEEL_RIGHT) {
						last_click_button_index = mb->get_button_index();
					}

					if (!mb->is_double_click()) {
						last_click_ms += diff;
						last_click_pos = Point2i(event.xbutton.x, event.xbutton.y);
					}
				} else {
					DEBUG_LOG_X11("[%u] ButtonRelease window=%lu (%u), button_index=%u \n", frame, event.xbutton.window, window_id, mb->get_button_index());

					WindowID window_id_other = INVALID_WINDOW_ID;
					Window wd_other_x11_window;
					if (wd.focused) {
						// Handle cases where an unfocused popup is open that needs to receive button-up events.
						WindowID popup_id = _get_focused_window_or_popup();
						if (popup_id != INVALID_WINDOW_ID && popup_id != window_id) {
							window_id_other = popup_id;
							wd_other_x11_window = windows[popup_id].x11_window;
						}
					} else {
						// Propagate the event to the focused window,
						// because it's received only on the topmost window.
						// Note: This is needed for drag & drop to work between windows,
						// because the engine expects events to keep being processed
						// on the same window dragging started.
						for (const KeyValue<WindowID, WindowData> &E : windows) {
							if (E.value.focused) {
								if (E.key != window_id) {
									window_id_other = E.key;
									wd_other_x11_window = E.value.x11_window;
								}
								break;
							}
						}
					}

					if (window_id_other != INVALID_WINDOW_ID) {
						int x, y;
						Window child;
						XTranslateCoordinates(x11_display, wd.x11_window, wd_other_x11_window, event.xbutton.x, event.xbutton.y, &x, &y, &child);

						mb->set_window_id(window_id_other);
						mb->set_position(Vector2(x, y));
						mb->set_global_position(mb->get_position());
					}
				}

				Input::get_singleton()->parse_input_event(mb);

			} break;
			case MotionNotify: {
				if (ime_window_event || ignore_events) {
					break;
				}
				// The X11 API requires filtering one-by-one through the motion
				// notify events, in order to figure out which event is the one
				// generated by warping the mouse pointer.
				WindowID focused_window_id = _get_focused_window_or_popup();
				if (!windows.has(focused_window_id)) {
					focused_window_id = MAIN_WINDOW_ID;
				}

				while (true) {
					if (mouse_mode == MOUSE_MODE_CAPTURED && event.xmotion.x == windows[focused_window_id].size.width / 2 && event.xmotion.y == windows[focused_window_id].size.height / 2) {
						//this is likely the warp event since it was warped here
						center = Vector2(event.xmotion.x, event.xmotion.y);
						break;
					}

					if (event_index + 1 < events.size()) {
						const XEvent &next_event = events[event_index + 1];
						if (next_event.type == MotionNotify) {
							++event_index;
							event = next_event;
						} else {
							break;
						}
					} else {
						break;
					}
				}

				last_timestamp = event.xmotion.time;

				// Motion is also simple.
				// A little hack is in order
				// to be able to send relative motion events.
				Point2i pos(event.xmotion.x, event.xmotion.y);

				// Avoidance of spurious mouse motion (see handling of touch)
				bool filter = false;
				// Adding some tolerance to match better Point2i to Vector2
				if (xi.state.size() && Vector2(pos).distance_squared_to(xi.mouse_pos_to_filter) < 2) {
					filter = true;
				}
				// Invalidate to avoid filtering a possible legitimate similar event coming later
				xi.mouse_pos_to_filter = Vector2(1e10, 1e10);
				if (filter) {
					break;
				}

				const WindowData &wd = windows[window_id];
				bool focused = wd.focused;

				if (mouse_mode == MOUSE_MODE_CAPTURED) {
					if (xi.relative_motion.x == 0 && xi.relative_motion.y == 0) {
						break;
					}

					Point2i new_center = pos;
					pos = last_mouse_pos + xi.relative_motion;
					center = new_center;
					do_mouse_warp = focused; // warp the cursor if we're focused in
				}

				if (!last_mouse_pos_valid) {
					last_mouse_pos = pos;
					last_mouse_pos_valid = true;
				}

				// Hackish but relative mouse motion is already handled in the RawMotion event.
				//  RawMotion does not provide the absolute mouse position (whereas MotionNotify does).
				//  Therefore, RawMotion cannot be the authority on absolute mouse position.
				//  RawMotion provides more precision than MotionNotify, which doesn't sense subpixel motion.
				//  Therefore, MotionNotify cannot be the authority on relative mouse motion.
				//  This means we need to take a combined approach...
				Point2i rel;

				// Only use raw input if in capture mode. Otherwise use the classic behavior.
				if (mouse_mode == MOUSE_MODE_CAPTURED) {
					rel = xi.relative_motion;
				} else {
					rel = pos - last_mouse_pos;
				}

				// Reset to prevent lingering motion
				xi.relative_motion.x = 0;
				xi.relative_motion.y = 0;
				if (mouse_mode == MOUSE_MODE_CAPTURED) {
					pos = Point2i(windows[focused_window_id].size.width / 2, windows[focused_window_id].size.height / 2);
				}

				BitField<MouseButtonMask> last_button_state = 0;
				if (event.xmotion.state & Button1Mask) {
					last_button_state.set_flag(MouseButtonMask::LEFT);
				}
				if (event.xmotion.state & Button2Mask) {
					last_button_state.set_flag(MouseButtonMask::MIDDLE);
				}
				if (event.xmotion.state & Button3Mask) {
					last_button_state.set_flag(MouseButtonMask::RIGHT);
				}
				if (event.xmotion.state & Button4Mask) {
					last_button_state.set_flag(MouseButtonMask::MB_XBUTTON1);
				}
				if (event.xmotion.state & Button5Mask) {
					last_button_state.set_flag(MouseButtonMask::MB_XBUTTON2);
				}

				Ref<InputEventMouseMotion> mm;
				mm.instantiate();

				mm->set_window_id(window_id);
				if (xi.pressure_supported) {
					mm->set_pressure(xi.pressure);
				} else {
					mm->set_pressure(bool(last_button_state.has_flag(MouseButtonMask::LEFT)) ? 1.0f : 0.0f);
				}
				mm->set_tilt(xi.tilt);
				mm->set_pen_inverted(xi.pen_inverted);

				_get_key_modifier_state(event.xmotion.state, mm);
				mm->set_button_mask(last_button_state);
				mm->set_position(pos);
				mm->set_global_position(pos);
				mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
				mm->set_screen_velocity(mm->get_velocity());

				mm->set_relative(rel);
				mm->set_relative_screen_position(rel);

				last_mouse_pos = pos;

				// printf("rel: %d,%d\n", rel.x, rel.y );
				// Don't propagate the motion event unless we have focus
				// this is so that the relative motion doesn't get messed up
				// after we regain focus.
				if (focused) {
					Input::get_singleton()->parse_input_event(mm);
				} else {
					// Propagate the event to the focused window,
					// because it's received only on the topmost window.
					// Note: This is needed for drag & drop to work between windows,
					// because the engine expects events to keep being processed
					// on the same window dragging started.
					for (const KeyValue<WindowID, WindowData> &E : windows) {
						const WindowData &wd_other = E.value;
						if (wd_other.focused) {
							int x, y;
							Window child;
							XTranslateCoordinates(x11_display, wd.x11_window, wd_other.x11_window, event.xmotion.x, event.xmotion.y, &x, &y, &child);

							Point2i pos_focused(x, y);

							mm->set_window_id(E.key);
							mm->set_position(pos_focused);
							mm->set_global_position(pos_focused);
							mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
							Input::get_singleton()->parse_input_event(mm);

							break;
						}
					}
				}

			} break;
			case KeyPress:
			case KeyRelease: {
				if (ignore_events) {
					break;
				}
#ifdef DISPLAY_SERVER_X11_DEBUG_LOGS_ENABLED
				if (event.type == KeyPress) {
					DEBUG_LOG_X11("[%u] KeyPress window=%lu (%u), keycode=%u, time=%lu \n", frame, event.xkey.window, window_id, event.xkey.keycode, event.xkey.time);
				} else {
					DEBUG_LOG_X11("[%u] KeyRelease window=%lu (%u), keycode=%u, time=%lu \n", frame, event.xkey.window, window_id, event.xkey.keycode, event.xkey.time);
				}
#endif
				last_timestamp = event.xkey.time;

				// key event is a little complex, so
				// it will be handled in its own function.
				_handle_key_event(window_id, &event.xkey, events, event_index);
			} break;

			case SelectionNotify:
				if (ime_window_event) {
					break;
				}
				if (event.xselection.target == requested) {
					Property p = _read_property(x11_display, windows[window_id].x11_window, XInternAtom(x11_display, "PRIMARY", 0));

					Vector<String> files = String((char *)p.data).split("\r\n", false);
					XFree(p.data);
					for (int i = 0; i < files.size(); i++) {
						files.write[i] = files[i].replace("file://", "").uri_decode();
					}

					if (windows[window_id].drop_files_callback.is_valid()) {
						Variant v_files = files;
						const Variant *v_args[1] = { &v_files };
						Variant ret;
						Callable::CallError ce;
						windows[window_id].drop_files_callback.callp((const Variant **)&v_args, 1, ret, ce);
						if (ce.error != Callable::CallError::CALL_OK) {
							ERR_PRINT(vformat("Failed to execute drop files callback: %s.", Variant::get_callable_error_text(windows[window_id].drop_files_callback, v_args, 1, ce)));
						}
					}

					//Reply that all is well.
					XClientMessageEvent m;
					memset(&m, 0, sizeof(m));
					m.type = ClientMessage;
					m.display = x11_display;
					m.window = xdnd_source_window;
					m.message_type = xdnd_finished;
					m.format = 32;
					m.data.l[0] = windows[window_id].x11_window;
					m.data.l[1] = 1;
					m.data.l[2] = xdnd_action_copy; //We only ever copy.

					XSendEvent(x11_display, xdnd_source_window, False, NoEventMask, (XEvent *)&m);
				}
				break;

			case ClientMessage:
				if (ime_window_event) {
					break;
				}
				if ((unsigned int)event.xclient.data.l[0] == (unsigned int)wm_delete) {
					_send_window_event(windows[window_id], WINDOW_EVENT_CLOSE_REQUEST);
				}

				else if ((unsigned int)event.xclient.message_type == (unsigned int)xdnd_enter) {
					//File(s) have been dragged over the window, check for supported target (text/uri-list)
					xdnd_version = (event.xclient.data.l[1] >> 24);
					Window source = event.xclient.data.l[0];
					bool more_than_3 = event.xclient.data.l[1] & 1;
					if (more_than_3) {
						Property p = _read_property(x11_display, source, XInternAtom(x11_display, "XdndTypeList", False));
						requested = pick_target_from_list(x11_display, (Atom *)p.data, p.nitems);
						XFree(p.data);
					} else {
						requested = pick_target_from_atoms(x11_display, event.xclient.data.l[2], event.xclient.data.l[3], event.xclient.data.l[4]);
					}
				} else if ((unsigned int)event.xclient.message_type == (unsigned int)xdnd_position) {
					//xdnd position event, reply with an XDND status message
					//just depending on type of data for now
					XClientMessageEvent m;
					memset(&m, 0, sizeof(m));
					m.type = ClientMessage;
					m.display = event.xclient.display;
					m.window = event.xclient.data.l[0];
					m.message_type = xdnd_status;
					m.format = 32;
					m.data.l[0] = windows[window_id].x11_window;
					m.data.l[1] = (requested != None);
					m.data.l[2] = 0; //empty rectangle
					m.data.l[3] = 0;
					m.data.l[4] = xdnd_action_copy;

					XSendEvent(x11_display, event.xclient.data.l[0], False, NoEventMask, (XEvent *)&m);
					XFlush(x11_display);
				} else if ((unsigned int)event.xclient.message_type == (unsigned int)xdnd_drop) {
					if (requested != None) {
						xdnd_source_window = event.xclient.data.l[0];
						if (xdnd_version >= 1) {
							XConvertSelection(x11_display, xdnd_selection, requested, XInternAtom(x11_display, "PRIMARY", 0), windows[window_id].x11_window, event.xclient.data.l[2]);
						} else {
							XConvertSelection(x11_display, xdnd_selection, requested, XInternAtom(x11_display, "PRIMARY", 0), windows[window_id].x11_window, CurrentTime);
						}
					} else {
						//Reply that we're not interested.
						XClientMessageEvent m;
						memset(&m, 0, sizeof(m));
						m.type = ClientMessage;
						m.display = event.xclient.display;
						m.window = event.xclient.data.l[0];
						m.message_type = xdnd_finished;
						m.format = 32;
						m.data.l[0] = windows[window_id].x11_window;
						m.data.l[1] = 0;
						m.data.l[2] = None; //Failed.
						XSendEvent(x11_display, event.xclient.data.l[0], False, NoEventMask, (XEvent *)&m);
					}
				}
				break;
			default:
				break;
		}
	}

	XFlush(x11_display);

	if (do_mouse_warp) {
		XWarpPointer(x11_display, None, windows[MAIN_WINDOW_ID].x11_window,
				0, 0, 0, 0, (int)windows[MAIN_WINDOW_ID].size.width / 2, (int)windows[MAIN_WINDOW_ID].size.height / 2);

		/*
		Window root, child;
		int root_x, root_y;
		int win_x, win_y;
		unsigned int mask;
		XQueryPointer( x11_display, x11_window, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask );

		printf("Root: %d,%d\n", root_x, root_y);
		printf("Win: %d,%d\n", win_x, win_y);
		*/
	}

#ifdef DBUS_ENABLED
	if (portal_desktop) {
		portal_desktop->process_file_dialog_callbacks();
	}
#endif

	_THREAD_SAFE_UNLOCK_

	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerX11::release_rendering_thread() {
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->release_current();
	}
	if (gl_manager_egl) {
		gl_manager_egl->release_current();
	}
#endif
}

void DisplayServerX11::swap_buffers() {
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->swap_buffers();
	}
	if (gl_manager_egl) {
		gl_manager_egl->swap_buffers();
	}
#endif
}

void DisplayServerX11::_update_context(WindowData &wd) {
	XClassHint *classHint = XAllocClassHint();

	if (classHint) {
		CharString name_str;
		switch (context) {
			case CONTEXT_EDITOR:
				name_str = "Godot_Editor";
				break;
			case CONTEXT_PROJECTMAN:
				name_str = "Godot_ProjectList";
				break;
			case CONTEXT_ENGINE:
				name_str = "Godot_Engine";
				break;
		}

		CharString class_str;
		if (context == CONTEXT_ENGINE) {
			String config_name = GLOBAL_GET("application/config/name");
			if (config_name.length() == 0) {
				class_str = "Godot_Engine";
			} else {
				class_str = config_name.utf8();
			}
		} else {
			class_str = "Godot";
		}

		classHint->res_class = class_str.ptrw();
		classHint->res_name = name_str.ptrw();

		XSetClassHint(x11_display, wd.x11_window, classHint);
		XFree(classHint);
	}
}

void DisplayServerX11::set_context(Context p_context) {
	_THREAD_SAFE_METHOD_

	context = p_context;

	for (KeyValue<WindowID, WindowData> &E : windows) {
		_update_context(E.value);
	}
}

bool DisplayServerX11::is_window_transparency_available() const {
	CharString net_wm_cm_name = vformat("_NET_WM_CM_S%d", XDefaultScreen(x11_display)).ascii();
	Atom net_wm_cm = XInternAtom(x11_display, net_wm_cm_name.get_data(), False);
	if (net_wm_cm == None) {
		return false;
	}
	if (XGetSelectionOwner(x11_display, net_wm_cm) == None) {
		return false;
	}
#if defined(RD_ENABLED)
	if (rendering_device && !rendering_device->is_composite_alpha_supported()) {
		return false;
	}
#endif
	return OS::get_singleton()->is_layered_allowed();
}

void DisplayServerX11::set_native_icon(const String &p_filename) {
	WARN_PRINT("Native icon not supported by this display server.");
}

bool g_set_icon_error = false;
int set_icon_errorhandler(Display *dpy, XErrorEvent *ev) {
	g_set_icon_error = true;
	return 0;
}

void DisplayServerX11::set_icon(const Ref<Image> &p_icon) {
	_THREAD_SAFE_METHOD_

	WindowData &wd = windows[MAIN_WINDOW_ID];

	int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&set_icon_errorhandler);

	Atom net_wm_icon = XInternAtom(x11_display, "_NET_WM_ICON", False);

	if (p_icon.is_valid()) {
		ERR_FAIL_COND(p_icon->get_width() <= 0 || p_icon->get_height() <= 0);

		Ref<Image> img = p_icon->duplicate();
		img->convert(Image::FORMAT_RGBA8);

		while (true) {
			int w = img->get_width();
			int h = img->get_height();

			if (g_set_icon_error) {
				g_set_icon_error = false;

				WARN_PRINT(vformat("Icon too large (%dx%d), attempting to downscale icon.", w, h));

				int new_width, new_height;
				if (w > h) {
					new_width = w / 2;
					new_height = h * new_width / w;
				} else {
					new_height = h / 2;
					new_width = w * new_height / h;
				}

				w = new_width;
				h = new_height;

				if (!w || !h) {
					WARN_PRINT("Unable to set icon.");
					break;
				}

				img->resize(w, h, Image::INTERPOLATE_CUBIC);
			}

			// We're using long to have wordsize (32Bit build -> 32 Bits, 64 Bit build -> 64 Bits
			Vector<long> pd;

			pd.resize(2 + w * h);

			pd.write[0] = w;
			pd.write[1] = h;

			const uint8_t *r = img->get_data().ptr();

			long *wr = &pd.write[2];
			uint8_t const *pr = r;

			for (int i = 0; i < w * h; i++) {
				long v = 0;
				//    A             R             G            B
				v |= pr[3] << 24 | pr[0] << 16 | pr[1] << 8 | pr[2];
				*wr++ = v;
				pr += 4;
			}

			if (net_wm_icon != None) {
				XChangeProperty(x11_display, wd.x11_window, net_wm_icon, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)pd.ptr(), pd.size());
			}

			if (!g_set_icon_error) {
				break;
			}
		}
	} else {
		XDeleteProperty(x11_display, wd.x11_window, net_wm_icon);
	}

	XFlush(x11_display);
	XSetErrorHandler(oldHandler);
}

void DisplayServerX11::window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window) {
	_THREAD_SAFE_METHOD_
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(p_window, p_vsync_mode);
	}
#endif

#if defined(GLES3_ENABLED)
	if (gl_manager) {
		gl_manager->set_use_vsync(p_vsync_mode != DisplayServer::VSYNC_DISABLED);
	}
	if (gl_manager_egl) {
		gl_manager_egl->set_use_vsync(p_vsync_mode != DisplayServer::VSYNC_DISABLED);
	}
#endif
}

DisplayServer::VSyncMode DisplayServerX11::window_get_vsync_mode(WindowID p_window) const {
	_THREAD_SAFE_METHOD_
#if defined(RD_ENABLED)
	if (rendering_context) {
		return rendering_context->window_get_vsync_mode(p_window);
	}
#endif
#if defined(GLES3_ENABLED)
	if (gl_manager) {
		return gl_manager->is_using_vsync() ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED;
	}
	if (gl_manager_egl) {
		return gl_manager_egl->is_using_vsync() ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED;
	}
#endif
	return DisplayServer::VSYNC_ENABLED;
}

Vector<String> DisplayServerX11::get_rendering_drivers_func() {
	Vector<String> drivers;

#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
	drivers.push_back("opengl3_es");
#endif

	return drivers;
}

DisplayServer *DisplayServerX11::create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerX11(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, r_error));
	return ds;
}

DisplayServerX11::WindowID DisplayServerX11::_create_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect) {
	//Create window

	XVisualInfo visualInfo;
	bool vi_selected = false;

#ifdef GLES3_ENABLED
	if (gl_manager) {
		Error err;
		visualInfo = gl_manager->get_vi(x11_display, err);
		ERR_FAIL_COND_V_MSG(err != OK, INVALID_WINDOW_ID, "Can't acquire visual info from display.");
		vi_selected = true;
	}
	if (gl_manager_egl) {
		XVisualInfo visual_info_template;
		int visual_id = gl_manager_egl->display_get_native_visual_id(x11_display);
		ERR_FAIL_COND_V_MSG(visual_id < 0, INVALID_WINDOW_ID, "Unable to get a visual id.");

		visual_info_template.visualid = (VisualID)visual_id;

		int number_of_visuals = 0;
		XVisualInfo *vi_list = XGetVisualInfo(x11_display, VisualIDMask, &visual_info_template, &number_of_visuals);
		ERR_FAIL_COND_V(number_of_visuals <= 0, INVALID_WINDOW_ID);

		visualInfo = vi_list[0];

		XFree(vi_list);
	}
#endif

	if (!vi_selected) {
		long visualMask = VisualScreenMask;
		int numberOfVisuals;
		XVisualInfo vInfoTemplate = {};
		vInfoTemplate.screen = DefaultScreen(x11_display);
		XVisualInfo *vi_list = XGetVisualInfo(x11_display, visualMask, &vInfoTemplate, &numberOfVisuals);
		ERR_FAIL_NULL_V(vi_list, INVALID_WINDOW_ID);

		visualInfo = vi_list[0];
		if (OS::get_singleton()->is_layered_allowed()) {
			for (int i = 0; i < numberOfVisuals; i++) {
				XRenderPictFormat *pict_format = XRenderFindVisualFormat(x11_display, vi_list[i].visual);
				if (!pict_format) {
					continue;
				}
				visualInfo = vi_list[i];
				if (pict_format->direct.alphaMask > 0) {
					break;
				}
			}
		}
		XFree(vi_list);
	}

	Colormap colormap = XCreateColormap(x11_display, RootWindow(x11_display, visualInfo.screen), visualInfo.visual, AllocNone);

	XSetWindowAttributes windowAttributes = {};
	windowAttributes.colormap = colormap;
	windowAttributes.background_pixel = 0xFFFFFFFF;
	windowAttributes.border_pixel = 0;
	windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

	unsigned long valuemask = CWBorderPixel | CWColormap | CWEventMask;

	if (OS::get_singleton()->is_layered_allowed()) {
		windowAttributes.background_pixmap = None;
		windowAttributes.background_pixel = 0;
		windowAttributes.border_pixmap = None;
		valuemask |= CWBackPixel;
	}

	WindowID id = window_id_counter++;
	WindowData &wd = windows[id];

	if (p_flags & WINDOW_FLAG_NO_FOCUS_BIT) {
		wd.no_focus = true;
	}

	if (p_flags & WINDOW_FLAG_POPUP_BIT) {
		wd.is_popup = true;
	}

	// Setup for menu subwindows:
	// - override_redirect forces the WM not to interfere with the window, to avoid delays due to
	//   handling decorations and placement.
	//   On the other hand, focus changes need to be handled manually when this is set.
	// - save_under is a hint for the WM to keep the content of windows behind to avoid repaint.
	if (wd.no_focus) {
		windowAttributes.override_redirect = True;
		windowAttributes.save_under = True;
		valuemask |= CWOverrideRedirect | CWSaveUnder;
	}

	int rq_screen = get_screen_from_rect(p_rect);
	if (rq_screen < 0) {
		rq_screen = get_primary_screen(); // Requested window rect is outside any screen bounds.
	}

	Rect2i win_rect = p_rect;
	if (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		Rect2i screen_rect = Rect2i(screen_get_position(rq_screen), screen_get_size(rq_screen));

		win_rect = screen_rect;
	} else {
		Rect2i srect = screen_get_usable_rect(rq_screen);
		Point2i wpos = p_rect.position;
		wpos = wpos.clamp(srect.position, srect.position + srect.size - p_rect.size / 3);

		win_rect.position = wpos;
	}

	// Position and size hints are set from these values before they are updated to the actual
	// window size, so we need to initialize them here.
	wd.position = win_rect.position;
	wd.size = win_rect.size;

	{
		wd.x11_window = XCreateWindow(x11_display, RootWindow(x11_display, visualInfo.screen), win_rect.position.x, win_rect.position.y, win_rect.size.width > 0 ? win_rect.size.width : 1, win_rect.size.height > 0 ? win_rect.size.height : 1, 0, visualInfo.depth, InputOutput, visualInfo.visual, valuemask, &windowAttributes);

		wd.parent = RootWindow(x11_display, visualInfo.screen);
		XSetWindowAttributes window_attributes_ime = {};
		window_attributes_ime.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

		wd.x11_xim_window = XCreateWindow(x11_display, wd.x11_window, 0, 0, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, CWEventMask, &window_attributes_ime);
#ifdef XKB_ENABLED
		if (dead_tbl && xkb_loaded_v05p) {
			wd.xkb_state = xkb_compose_state_new(dead_tbl, XKB_COMPOSE_STATE_NO_FLAGS);
		}
#endif
		// Enable receiving notification when the window is initialized (MapNotify)
		// so the focus can be set at the right time.
		if (!wd.no_focus && !wd.is_popup) {
			XSelectInput(x11_display, wd.x11_window, StructureNotifyMask);
		}

		//associate PID
		// make PID known to X11
		{
			const long pid = OS::get_singleton()->get_process_id();
			Atom net_wm_pid = XInternAtom(x11_display, "_NET_WM_PID", False);
			if (net_wm_pid != None) {
				XChangeProperty(x11_display, wd.x11_window, net_wm_pid, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
			}
		}

		long im_event_mask = 0;

		{
			XIEventMask all_event_mask;
			XSetWindowAttributes new_attr;

			new_attr.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
					ButtonReleaseMask | EnterWindowMask |
					LeaveWindowMask | PointerMotionMask |
					Button1MotionMask |
					Button2MotionMask | Button3MotionMask |
					Button4MotionMask | Button5MotionMask |
					ButtonMotionMask | KeymapStateMask |
					ExposureMask | VisibilityChangeMask |
					StructureNotifyMask |
					SubstructureNotifyMask | SubstructureRedirectMask |
					FocusChangeMask | PropertyChangeMask |
					ColormapChangeMask | OwnerGrabButtonMask |
					im_event_mask;

			XChangeWindowAttributes(x11_display, wd.x11_window, CWEventMask, &new_attr);

			static unsigned char all_mask_data[XIMaskLen(XI_LASTEVENT)] = {};

			all_event_mask.deviceid = XIAllDevices;
			all_event_mask.mask_len = sizeof(all_mask_data);
			all_event_mask.mask = all_mask_data;

			XISetMask(all_event_mask.mask, XI_HierarchyChanged);

#ifdef TOUCH_ENABLED
			if (xi.touch_devices.size()) {
				XISetMask(all_event_mask.mask, XI_TouchBegin);
				XISetMask(all_event_mask.mask, XI_TouchUpdate);
				XISetMask(all_event_mask.mask, XI_TouchEnd);
				XISetMask(all_event_mask.mask, XI_TouchOwnership);
			}
#endif

			XISelectEvents(x11_display, wd.x11_window, &all_event_mask, 1);
		}

		/* set the titlebar name */
		XStoreName(x11_display, wd.x11_window, "Godot");
		XSetWMProtocols(x11_display, wd.x11_window, &wm_delete, 1);
		if (xdnd_aware != None) {
			XChangeProperty(x11_display, wd.x11_window, xdnd_aware, XA_ATOM, 32, PropModeReplace, (unsigned char *)&xdnd_version, 1);
		}

		if (xim && xim_style) {
			// Block events polling while changing input focus
			// because it triggers some event polling internally.
			MutexLock mutex_lock(events_mutex);

			// Force on-the-spot for the over-the-spot style.
			if ((xim_style & XIMPreeditPosition) != 0) {
				xim_style &= ~XIMPreeditPosition;
				xim_style |= XIMPreeditCallbacks;
			}
			if ((xim_style & XIMPreeditCallbacks) != 0) {
				::XIMCallback preedit_start_callback;
				preedit_start_callback.client_data = (::XPointer)(this);
				preedit_start_callback.callback = (::XIMProc)(void *)(_xim_preedit_start_callback);

				::XIMCallback preedit_done_callback;
				preedit_done_callback.client_data = (::XPointer)(this);
				preedit_done_callback.callback = (::XIMProc)(_xim_preedit_done_callback);

				::XIMCallback preedit_draw_callback;
				preedit_draw_callback.client_data = (::XPointer)(this);
				preedit_draw_callback.callback = (::XIMProc)(_xim_preedit_draw_callback);

				::XIMCallback preedit_caret_callback;
				preedit_caret_callback.client_data = (::XPointer)(this);
				preedit_caret_callback.callback = (::XIMProc)(_xim_preedit_caret_callback);

				::XVaNestedList preedit_attributes = XVaCreateNestedList(0,
						XNPreeditStartCallback, &preedit_start_callback,
						XNPreeditDoneCallback, &preedit_done_callback,
						XNPreeditDrawCallback, &preedit_draw_callback,
						XNPreeditCaretCallback, &preedit_caret_callback,
						(char *)nullptr);

				wd.xic = XCreateIC(xim,
						XNInputStyle, xim_style,
						XNClientWindow, wd.x11_xim_window,
						XNFocusWindow, wd.x11_xim_window,
						XNPreeditAttributes, preedit_attributes,
						(char *)nullptr);
				XFree(preedit_attributes);
			} else {
				wd.xic = XCreateIC(xim,
						XNInputStyle, xim_style,
						XNClientWindow, wd.x11_xim_window,
						XNFocusWindow, wd.x11_xim_window,
						(char *)nullptr);
			}

			if (XGetICValues(wd.xic, XNFilterEvents, &im_event_mask, nullptr) != nullptr) {
				WARN_PRINT("XGetICValues couldn't obtain XNFilterEvents value");
				XDestroyIC(wd.xic);
				wd.xic = nullptr;
			}
			if (wd.xic) {
				XUnsetICFocus(wd.xic);
			} else {
				WARN_PRINT("XCreateIC couldn't create wd.xic");
			}
		} else {
			wd.xic = nullptr;
			WARN_PRINT("XCreateIC couldn't create wd.xic");
		}

		_update_context(wd);

		if (p_flags & WINDOW_FLAG_BORDERLESS_BIT) {
			Hints hints;
			Atom property;
			hints.flags = 2;
			hints.decorations = 0;
			property = XInternAtom(x11_display, "_MOTIF_WM_HINTS", True);
			if (property != None) {
				XChangeProperty(x11_display, wd.x11_window, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
			}
		}

		if (wd.is_popup || wd.no_focus) {
			// Set Utility type to disable fade animations.
			Atom type_atom = XInternAtom(x11_display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
			Atom wt_atom = XInternAtom(x11_display, "_NET_WM_WINDOW_TYPE", False);
			if (wt_atom != None && type_atom != None) {
				XChangeProperty(x11_display, wd.x11_window, wt_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&type_atom, 1);
			}
		} else {
			Atom type_atom = XInternAtom(x11_display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
			Atom wt_atom = XInternAtom(x11_display, "_NET_WM_WINDOW_TYPE", False);

			if (wt_atom != None && type_atom != None) {
				XChangeProperty(x11_display, wd.x11_window, wt_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&type_atom, 1);
			}
		}

		_update_size_hints(id);

#if defined(RD_ENABLED)
		if (rendering_context) {
			union {
#ifdef VULKAN_ENABLED
				RenderingContextDriverVulkanX11::WindowPlatformData vulkan;
#endif
			} wpd;
#ifdef VULKAN_ENABLED
			if (rendering_driver == "vulkan") {
				wpd.vulkan.window = wd.x11_window;
				wpd.vulkan.display = x11_display;
			}
#endif
			Error err = rendering_context->window_create(id, &wpd);
			ERR_FAIL_COND_V_MSG(err != OK, INVALID_WINDOW_ID, vformat("Can't create a %s window", rendering_driver));

			rendering_context->window_set_size(id, win_rect.size.width, win_rect.size.height);
			rendering_context->window_set_vsync_mode(id, p_vsync_mode);
		}
#endif
#ifdef GLES3_ENABLED
		if (gl_manager) {
			Error err = gl_manager->window_create(id, wd.x11_window, x11_display, win_rect.size.width, win_rect.size.height);
			ERR_FAIL_COND_V_MSG(err != OK, INVALID_WINDOW_ID, "Can't create an OpenGL window");
		}
		if (gl_manager_egl) {
			Error err = gl_manager_egl->window_create(id, x11_display, &wd.x11_window, win_rect.size.width, win_rect.size.height);
			ERR_FAIL_COND_V_MSG(err != OK, INVALID_WINDOW_ID, "Failed to create an OpenGLES window.");
		}
		window_set_vsync_mode(p_vsync_mode, id);
#endif

		//set_class_hint(x11_display, wd.x11_window);
		XFlush(x11_display);

		XSync(x11_display, False);
		//XSetErrorHandler(oldHandler);
	}

	window_set_mode(p_mode, id);

	//sync size
	{
		XWindowAttributes xwa;

		XSync(x11_display, False);
		XGetWindowAttributes(x11_display, wd.x11_window, &xwa);

		wd.position.x = xwa.x;
		wd.position.y = xwa.y;
		wd.size.width = xwa.width;
		wd.size.height = xwa.height;
	}

	//set cursor
	if (cursors[current_cursor] != None) {
		XDefineCursor(x11_display, wd.x11_window, cursors[current_cursor]);
	}

	return id;
}

static bool _is_xim_style_supported(const ::XIMStyle &p_style) {
	const ::XIMStyle supported_preedit = XIMPreeditCallbacks | XIMPreeditPosition | XIMPreeditNothing | XIMPreeditNone;
	const ::XIMStyle supported_status = XIMStatusNothing | XIMStatusNone;

	// Check preedit style is supported
	if ((p_style & supported_preedit) == 0) {
		return false;
	}

	// Check status style is supported
	if ((p_style & supported_status) == 0) {
		return false;
	}

	return true;
}

static ::XIMStyle _get_best_xim_style(const ::XIMStyle &p_style_a, const ::XIMStyle &p_style_b) {
	if (p_style_a == 0) {
		return p_style_b;
	}
	if (p_style_b == 0) {
		return p_style_a;
	}

	const ::XIMStyle preedit = XIMPreeditArea | XIMPreeditCallbacks | XIMPreeditPosition | XIMPreeditNothing | XIMPreeditNone;
	const ::XIMStyle status = XIMStatusArea | XIMStatusCallbacks | XIMStatusNothing | XIMStatusNone;

	::XIMStyle a = p_style_a & preedit;
	::XIMStyle b = p_style_b & preedit;
	if (a != b) {
		// Compare preedit styles.
		if ((a | b) & XIMPreeditCallbacks) {
			return a == XIMPreeditCallbacks ? p_style_a : p_style_b;
		} else if ((a | b) & XIMPreeditPosition) {
			return a == XIMPreeditPosition ? p_style_a : p_style_b;
		} else if ((a | b) & XIMPreeditArea) {
			return a == XIMPreeditArea ? p_style_a : p_style_b;
		} else if ((a | b) & XIMPreeditNothing) {
			return a == XIMPreeditNothing ? p_style_a : p_style_b;
		}
	} else {
		// Preedit styles are the same, compare status styles.
		a = p_style_a & status;
		b = p_style_b & status;

		if ((a | b) & XIMStatusCallbacks) {
			return a == XIMStatusCallbacks ? p_style_a : p_style_b;
		} else if ((a | b) & XIMStatusArea) {
			return a == XIMStatusArea ? p_style_a : p_style_b;
		} else if ((a | b) & XIMStatusNothing) {
			return a == XIMStatusNothing ? p_style_a : p_style_b;
		}
	}
	return p_style_a;
}

DisplayServerX11::DisplayServerX11(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, Error &r_error) {
	KeyMappingX11::initialize();

	xwayland = OS::get_singleton()->get_environment("XDG_SESSION_TYPE").to_lower() == "wayland";

	native_menu = memnew(NativeMenu);
	context = p_context;

#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	if (initialize_xlib(dylibloader_verbose) != 0) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Can't load Xlib dynamically.");
	}

	if (initialize_xcursor(dylibloader_verbose) != 0) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Can't load XCursor dynamically.");
	}
#ifdef XKB_ENABLED
	bool xkb_loaded = (initialize_xkbcommon(dylibloader_verbose) == 0);
	xkb_loaded_v05p = xkb_loaded;
	if (!xkb_context_new || !xkb_compose_table_new_from_locale || !xkb_compose_table_unref || !xkb_context_unref || !xkb_compose_state_feed || !xkb_compose_state_unref || !xkb_compose_state_new || !xkb_compose_state_get_status || !xkb_compose_state_get_utf8) {
		xkb_loaded_v05p = false;
		print_verbose("Detected XKBcommon library version older than 0.5, dead key composition and Unicode key labels disabled.");
	}
	xkb_loaded_v08p = xkb_loaded;
	if (!xkb_keysym_to_utf32 || !xkb_keysym_to_upper) {
		xkb_loaded_v08p = false;
		print_verbose("Detected XKBcommon library version older than 0.8, Unicode key labels disabled.");
	}
#endif
	if (initialize_xext(dylibloader_verbose) != 0) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Can't load Xext dynamically.");
	}

	if (initialize_xinerama(dylibloader_verbose) != 0) {
		xinerama_ext_ok = false;
	}

	if (initialize_xrandr(dylibloader_verbose) != 0) {
		xrandr_ext_ok = false;
	}

	if (initialize_xrender(dylibloader_verbose) != 0) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Can't load Xrender dynamically.");
	}

	if (initialize_xinput2(dylibloader_verbose) != 0) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Can't load Xinput2 dynamically.");
	}
#else
#ifdef XKB_ENABLED
	bool xkb_loaded = true;
	xkb_loaded_v05p = true;
	xkb_loaded_v08p = true;
#endif
#endif

#ifdef XKB_ENABLED
	if (xkb_loaded) {
		xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		if (xkb_ctx) {
			const char *locale = getenv("LC_ALL");
			if (!locale || !*locale) {
				locale = getenv("LC_CTYPE");
			}
			if (!locale || !*locale) {
				locale = getenv("LANG");
			}
			if (!locale || !*locale) {
				locale = "C";
			}
			dead_tbl = xkb_compose_table_new_from_locale(xkb_ctx, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
		}
	}
#endif

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	r_error = OK;

#ifdef SOWRAP_ENABLED
	{
		if (!XcursorImageCreate || !XcursorImageLoadCursor || !XcursorImageDestroy || !XcursorGetDefaultSize || !XcursorGetTheme || !XcursorLibraryLoadImage) {
			// There's no API to check version, check if functions are available instead.
			ERR_PRINT("Unsupported Xcursor library version.");
			r_error = ERR_UNAVAILABLE;
			return;
		}
	}
#endif

	for (int i = 0; i < CURSOR_MAX; i++) {
		cursors[i] = None;
		cursor_img[i] = nullptr;
	}

	XInitThreads(); //always use threads

	/** XLIB INITIALIZATION **/
	x11_display = XOpenDisplay(nullptr);

	if (!x11_display) {
		ERR_PRINT("X11 Display is not available");
		r_error = ERR_UNAVAILABLE;
		return;
	}

	if (xshaped_ext_ok) {
		int version_major = 0;
		int version_minor = 0;
		int rc = XShapeQueryVersion(x11_display, &version_major, &version_minor);
		print_verbose(vformat("Xshape %d.%d detected.", version_major, version_minor));
		if (rc != 1 || version_major < 1) {
			xshaped_ext_ok = false;
			print_verbose("Unsupported Xshape library version.");
		}
	}

	if (xinerama_ext_ok) {
		int version_major = 0;
		int version_minor = 0;
		int rc = XineramaQueryVersion(x11_display, &version_major, &version_minor);
		print_verbose(vformat("Xinerama %d.%d detected.", version_major, version_minor));
		if (rc != 1 || version_major < 1) {
			xinerama_ext_ok = false;
			print_verbose("Unsupported Xinerama library version.");
		}
	}

	if (xrandr_ext_ok) {
		int version_major = 0;
		int version_minor = 0;
		int rc = XRRQueryVersion(x11_display, &version_major, &version_minor);
		print_verbose(vformat("Xrandr %d.%d detected.", version_major, version_minor));
		if (rc != 1 || (version_major == 1 && version_minor < 3) || (version_major < 1)) {
			xrandr_ext_ok = false;
			print_verbose("Unsupported Xrandr library version.");
		}
	}

	{
		int version_major = 0;
		int version_minor = 0;
		int rc = XRenderQueryVersion(x11_display, &version_major, &version_minor);
		print_verbose(vformat("Xrender %d.%d detected.", version_major, version_minor));
		if (rc != 1 || (version_major == 0 && version_minor < 11)) {
			ERR_PRINT("Unsupported Xrender library version.");
			r_error = ERR_UNAVAILABLE;
			XCloseDisplay(x11_display);
			return;
		}
	}

	{
		int version_major = 2; // Report 2.2 as supported by engine, but should work with 2.1 or 2.0 library as well.
		int version_minor = 2;
		int rc = XIQueryVersion(x11_display, &version_major, &version_minor);
		print_verbose(vformat("Xinput %d.%d detected.", version_major, version_minor));
		if (rc != Success || (version_major < 2)) {
			ERR_PRINT("Unsupported Xinput2 library version.");
			r_error = ERR_UNAVAILABLE;
			XCloseDisplay(x11_display);
			return;
		}
	}

	char *modifiers = nullptr;
	Bool xkb_dar = False;
	XAutoRepeatOn(x11_display);
	xkb_dar = XkbSetDetectableAutoRepeat(x11_display, True, nullptr);

	// Try to support IME if detectable auto-repeat is supported
	if (xkb_dar == True) {
#ifdef X_HAVE_UTF8_STRING
		// Xutf8LookupString will be used later instead of XmbLookupString before
		// the multibyte sequences can be converted to unicode string.
		modifiers = XSetLocaleModifiers("");
#endif
	}

	if (modifiers == nullptr) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			WARN_PRINT("IME is disabled");
		}
		XSetLocaleModifiers("@im=none");
		WARN_PRINT("Error setting locale modifiers");
	}

	const char *err;
	int xrandr_major = 0;
	int xrandr_minor = 0;
	int event_base, error_base;
	xrandr_ext_ok = XRRQueryExtension(x11_display, &event_base, &error_base);
	xrandr_handle = dlopen("libXrandr.so.2", RTLD_LAZY);
	if (!xrandr_handle) {
		err = dlerror();
		// For some arcane reason, NetBSD now ships libXrandr.so.3 while the rest of the world has libXrandr.so.2...
		// In case this happens for other X11 platforms in the future, let's give it a try too before failing.
		xrandr_handle = dlopen("libXrandr.so.3", RTLD_LAZY);
		if (!xrandr_handle) {
			fprintf(stderr, "could not load libXrandr.so.2, Error: %s\n", err);
		}
	}

	if (xrandr_handle) {
		XRRQueryVersion(x11_display, &xrandr_major, &xrandr_minor);
		if (((xrandr_major << 8) | xrandr_minor) >= 0x0105) {
			xrr_get_monitors = (xrr_get_monitors_t)dlsym(xrandr_handle, "XRRGetMonitors");
			if (!xrr_get_monitors) {
				err = dlerror();
				fprintf(stderr, "could not find symbol XRRGetMonitors\nError: %s\n", err);
			} else {
				xrr_free_monitors = (xrr_free_monitors_t)dlsym(xrandr_handle, "XRRFreeMonitors");
				if (!xrr_free_monitors) {
					err = dlerror();
					fprintf(stderr, "could not find XRRFreeMonitors\nError: %s\n", err);
					xrr_get_monitors = nullptr;
				}
			}
		}
	}

	if (!_refresh_device_info()) {
		OS::get_singleton()->alert("Your system does not support XInput 2.\n"
								   "Please upgrade your distribution.",
				"Unable to initialize XInput");
		r_error = ERR_UNAVAILABLE;
		return;
	}

	xim = XOpenIM(x11_display, nullptr, nullptr, nullptr);

	if (xim == nullptr) {
		WARN_PRINT("XOpenIM failed");
		xim_style = 0L;
	} else {
		::XIMCallback im_destroy_callback;
		im_destroy_callback.client_data = (::XPointer)(this);
		im_destroy_callback.callback = (::XIMProc)(_xim_destroy_callback);
		if (XSetIMValues(xim, XNDestroyCallback, &im_destroy_callback,
					nullptr) != nullptr) {
			WARN_PRINT("Error setting XIM destroy callback");
		}

		::XIMStyles *xim_styles = nullptr;
		xim_style = 0L;
		char *imvalret = XGetIMValues(xim, XNQueryInputStyle, &xim_styles, nullptr);
		if (imvalret != nullptr || xim_styles == nullptr) {
			fprintf(stderr, "Input method doesn't support any styles\n");
		}

		if (xim_styles) {
			xim_style = 0L;
			for (int i = 0; i < xim_styles->count_styles; i++) {
				const ::XIMStyle &style = xim_styles->supported_styles[i];

				if (!_is_xim_style_supported(style)) {
					continue;
				}

				xim_style = _get_best_xim_style(xim_style, style);
			}

			XFree(xim_styles);
		}
		XFree(imvalret);
	}

	/* Atom internment */
	wm_delete = XInternAtom(x11_display, "WM_DELETE_WINDOW", true);
	// Set Xdnd (drag & drop) support.
	xdnd_aware = XInternAtom(x11_display, "XdndAware", False);
	xdnd_enter = XInternAtom(x11_display, "XdndEnter", False);
	xdnd_position = XInternAtom(x11_display, "XdndPosition", False);
	xdnd_status = XInternAtom(x11_display, "XdndStatus", False);
	xdnd_action_copy = XInternAtom(x11_display, "XdndActionCopy", False);
	xdnd_drop = XInternAtom(x11_display, "XdndDrop", False);
	xdnd_finished = XInternAtom(x11_display, "XdndFinished", False);
	xdnd_selection = XInternAtom(x11_display, "XdndSelection", False);

#ifdef SPEECHD_ENABLED
	// Init TTS
	bool tts_enabled = GLOBAL_GET("audio/general/text_to_speech");
	if (tts_enabled) {
		tts = memnew(TTS_Linux);
	}
#endif

	//!!!!!!!!!!!!!!!!!!!!!!!!!!
	//TODO - do Vulkan and OpenGL support checks, driver selection and fallback
	rendering_driver = p_rendering_driver;

	bool driver_found = false;
	String executable_name = OS::get_singleton()->get_executable_path().get_file();

	// Initialize context and rendering device.

#if defined(RD_ENABLED)
#if defined(VULKAN_ENABLED)
	if (rendering_driver == "vulkan") {
		rendering_context = memnew(RenderingContextDriverVulkanX11);
	}
#endif // VULKAN_ENABLED

	if (rendering_context) {
		if (rendering_context->initialize() != OK) {
			memdelete(rendering_context);
			rendering_context = nullptr;
			bool fallback_to_opengl3 = GLOBAL_GET("rendering/rendering_device/fallback_to_opengl3");
			if (fallback_to_opengl3 && rendering_driver != "opengl3") {
				WARN_PRINT("Your video card drivers seem not to support the required Vulkan version, switching to OpenGL 3.");
				rendering_driver = "opengl3";
				OS::get_singleton()->set_current_rendering_method("gl_compatibility");
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
			} else {
				r_error = ERR_CANT_CREATE;

				if (p_rendering_driver == "vulkan") {
					OS::get_singleton()->alert(
							vformat("Your video card drivers seem not to support the required Vulkan version.\n\n"
									"If possible, consider updating your video card drivers or using the OpenGL 3 driver.\n\n"
									"You can enable the OpenGL 3 driver by starting the engine from the\n"
									"command line with the command:\n\n    \"%s\" --rendering-driver opengl3\n\n"
									"If you recently updated your video card drivers, try rebooting.",
									executable_name),
							"Unable to initialize Vulkan video driver");
				}

				ERR_FAIL_MSG(vformat("Could not initialize %s", rendering_driver));
			}
		}
		driver_found = true;
	}
#endif // RD_ENABLED

#if defined(GLES3_ENABLED)
	if (rendering_driver == "opengl3" || rendering_driver == "opengl3_es") {
		if (getenv("DRI_PRIME") == nullptr) {
			int use_prime = -1;

			if (getenv("PRIMUS_DISPLAY") ||
					getenv("PRIMUS_libGLd") ||
					getenv("PRIMUS_libGLa") ||
					getenv("PRIMUS_libGL") ||
					getenv("PRIMUS_LOAD_GLOBAL") ||
					getenv("BUMBLEBEE_SOCKET")) {
				print_verbose("Optirun/primusrun detected. Skipping GPU detection");
				use_prime = 0;
			}

			// Some tools use fake libGL libraries and have them override the real one using
			// LD_LIBRARY_PATH, so we skip them. *But* Steam also sets LD_LIBRARY_PATH for its
			// runtime and includes system `/lib` and `/lib64`... so ignore Steam.
			if (use_prime == -1 && getenv("LD_LIBRARY_PATH") && !getenv("STEAM_RUNTIME_LIBRARY_PATH")) {
				String ld_library_path(getenv("LD_LIBRARY_PATH"));
				Vector<String> libraries = ld_library_path.split(":");

				for (int i = 0; i < libraries.size(); ++i) {
					if (FileAccess::exists(libraries[i] + "/libGL.so.1") ||
							FileAccess::exists(libraries[i] + "/libGL.so")) {
						print_verbose("Custom libGL override detected. Skipping GPU detection");
						use_prime = 0;
					}
				}
			}

			if (use_prime == -1) {
				print_verbose("Detecting GPUs, set DRI_PRIME in the environment to override GPU detection logic.");
				use_prime = detect_prime();
			}

			if (use_prime) {
				print_line("Found discrete GPU, setting DRI_PRIME=1 to use it.");
				print_line("Note: Set DRI_PRIME=0 in the environment to disable Godot from using the discrete GPU.");
				setenv("DRI_PRIME", "1", 1);
			}
		}
	}
	if (rendering_driver == "opengl3") {
		gl_manager = memnew(GLManager_X11(p_resolution, GLManager_X11::GLES_3_0_COMPATIBLE));
		if (gl_manager->initialize(x11_display) != OK || gl_manager->open_display(x11_display) != OK) {
			memdelete(gl_manager);
			gl_manager = nullptr;
			bool fallback = GLOBAL_GET("rendering/gl_compatibility/fallback_to_gles");
			if (fallback) {
				WARN_PRINT("Your video card drivers seem not to support the required OpenGL version, switching to OpenGLES.");
				rendering_driver = "opengl3_es";
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
			} else {
				r_error = ERR_UNAVAILABLE;

				OS::get_singleton()->alert(
						vformat("Your video card drivers seem not to support the required OpenGL 3.3 version.\n\n"
								"If possible, consider updating your video card drivers or using the Vulkan driver.\n\n"
								"You can enable the Vulkan driver by starting the engine from the\n"
								"command line with the command:\n\n    \"%s\" --rendering-driver vulkan\n\n"
								"If you recently updated your video card drivers, try rebooting.",
								executable_name),
						"Unable to initialize OpenGL video driver");

				ERR_FAIL_MSG("Could not initialize OpenGL.");
			}
		} else {
			driver_found = true;
			RasterizerGLES3::make_current(true);
		}
	}

	if (rendering_driver == "opengl3_es") {
		gl_manager_egl = memnew(GLManagerEGL_X11);
		if (gl_manager_egl->initialize() != OK || gl_manager_egl->open_display(x11_display) != OK) {
			memdelete(gl_manager_egl);
			gl_manager_egl = nullptr;
			r_error = ERR_UNAVAILABLE;

			OS::get_singleton()->alert(
					"Your video card drivers seem not to support the required OpenGL ES 3.0 version.\n\n"
					"If possible, consider updating your video card drivers.\n\n"
					"If you recently updated your video card drivers, try rebooting.",
					"Unable to initialize OpenGL ES video driver");

			ERR_FAIL_MSG("Could not initialize OpenGL ES.");
		}
		driver_found = true;
		RasterizerGLES3::make_current(false);
	}

#endif // GLES3_ENABLED

	if (!driver_found) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Video driver not found.");
	}

	Point2i window_position;
	if (p_position != nullptr) {
		window_position = *p_position;
	} else {
		if (p_screen == SCREEN_OF_MAIN_WINDOW) {
			p_screen = SCREEN_PRIMARY;
		}
		Rect2i scr_rect = screen_get_usable_rect(p_screen);
		window_position = scr_rect.position + (scr_rect.size - p_resolution) / 2;
	}

	WindowID main_window = _create_window(p_mode, p_vsync_mode, p_flags, Rect2i(window_position, p_resolution));
	if (main_window == INVALID_WINDOW_ID) {
		r_error = ERR_CANT_CREATE;
		return;
	}
	for (int i = 0; i < WINDOW_FLAG_MAX; i++) {
		if (p_flags & (1 << i)) {
			window_set_flag(WindowFlags(i), true, main_window);
		}
	}
	show_window(main_window);

#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_device = memnew(RenderingDevice);
		if (rendering_device->initialize(rendering_context, MAIN_WINDOW_ID) != OK) {
			memdelete(rendering_device);
			rendering_device = nullptr;
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
		rendering_device->screen_create(MAIN_WINDOW_ID);

		RendererCompositorRD::make_current();
	}
#endif // RD_ENABLED

	{
		//set all event master mask
		XIEventMask all_master_event_mask;
		static unsigned char all_master_mask_data[XIMaskLen(XI_LASTEVENT)] = {};
		all_master_event_mask.deviceid = XIAllMasterDevices;
		all_master_event_mask.mask_len = sizeof(all_master_mask_data);
		all_master_event_mask.mask = all_master_mask_data;
		XISetMask(all_master_event_mask.mask, XI_DeviceChanged);
		XISetMask(all_master_event_mask.mask, XI_RawMotion);
		XISelectEvents(x11_display, DefaultRootWindow(x11_display), &all_master_event_mask, 1);
	}

	cursor_size = XcursorGetDefaultSize(x11_display);
	cursor_theme = XcursorGetTheme(x11_display);

	if (!cursor_theme) {
		print_verbose("XcursorGetTheme could not get cursor theme");
		cursor_theme = "default";
	}

	for (int i = 0; i < CURSOR_MAX; i++) {
		static const char *cursor_file[] = {
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

		cursor_img[i] = XcursorLibraryLoadImage(cursor_file[i], cursor_theme, cursor_size);
		if (!cursor_img[i]) {
			const char *fallback = nullptr;

			switch (i) {
				case CURSOR_POINTING_HAND:
					fallback = "pointer";
					break;
				case CURSOR_CROSS:
					fallback = "crosshair";
					break;
				case CURSOR_WAIT:
					fallback = "wait";
					break;
				case CURSOR_BUSY:
					fallback = "progress";
					break;
				case CURSOR_DRAG:
					fallback = "grabbing";
					break;
				case CURSOR_CAN_DROP:
					fallback = "hand1";
					break;
				case CURSOR_FORBIDDEN:
					fallback = "forbidden";
					break;
				case CURSOR_VSIZE:
					fallback = "ns-resize";
					break;
				case CURSOR_HSIZE:
					fallback = "ew-resize";
					break;
				case CURSOR_BDIAGSIZE:
					fallback = "fd_double_arrow";
					break;
				case CURSOR_FDIAGSIZE:
					fallback = "bd_double_arrow";
					break;
				case CURSOR_MOVE:
					cursor_img[i] = cursor_img[CURSOR_DRAG];
					break;
				case CURSOR_VSPLIT:
					fallback = "sb_v_double_arrow";
					break;
				case CURSOR_HSPLIT:
					fallback = "sb_h_double_arrow";
					break;
				case CURSOR_HELP:
					fallback = "help";
					break;
			}
			if (fallback != nullptr) {
				cursor_img[i] = XcursorLibraryLoadImage(fallback, cursor_theme, cursor_size);
			}
		}
		if (cursor_img[i]) {
			cursors[i] = XcursorImageLoadCursor(x11_display, cursor_img[i]);
		} else {
			print_verbose("Failed loading custom cursor: " + String(cursor_file[i]));
		}
	}

	{
		// Creating an empty/transparent cursor

		// Create 1x1 bitmap
		Pixmap cursormask = XCreatePixmap(x11_display,
				RootWindow(x11_display, DefaultScreen(x11_display)), 1, 1, 1);

		// Fill with zero
		XGCValues xgc;
		xgc.function = GXclear;
		GC gc = XCreateGC(x11_display, cursormask, GCFunction, &xgc);
		XFillRectangle(x11_display, cursormask, gc, 0, 0, 1, 1);

		// Color value doesn't matter. Mask zero means no foreground or background will be drawn
		XColor col = {};

		Cursor cursor = XCreatePixmapCursor(x11_display,
				cursormask, // source (using cursor mask as placeholder, since it'll all be ignored)
				cursormask, // mask
				&col, &col, 0, 0);

		XFreePixmap(x11_display, cursormask);
		XFreeGC(x11_display, gc);

		if (cursor == None) {
			ERR_PRINT("FAILED CREATING CURSOR");
		}

		null_cursor = cursor;
	}
	cursor_set_shape(CURSOR_BUSY);

	// Search the X11 event queue for ConfigureNotify events and process all
	// that are currently queued early, so we can get the final window size
	// for correctly drawing of the bootsplash.
	XEvent config_event;
	while (XCheckTypedEvent(x11_display, ConfigureNotify, &config_event)) {
		_window_changed(&config_event);
	}
	events_thread.start(_poll_events_thread, this);

	_update_real_mouse_position(windows[MAIN_WINDOW_ID]);

#ifdef DBUS_ENABLED
	screensaver = memnew(FreeDesktopScreenSaver);
	screen_set_keep_on(GLOBAL_GET("display/window/energy_saving/keep_screen_on"));

	portal_desktop = memnew(FreeDesktopPortalDesktop);
#endif // DBUS_ENABLED

	XSetErrorHandler(&default_window_error_handler);

	r_error = OK;
}

DisplayServerX11::~DisplayServerX11() {
	// Send owned clipboard data to clipboard manager before exit.
	Window x11_main_window = windows[MAIN_WINDOW_ID].x11_window;
	_clipboard_transfer_ownership(XA_PRIMARY, x11_main_window);
	_clipboard_transfer_ownership(XInternAtom(x11_display, "CLIPBOARD", 0), x11_main_window);

	events_thread_done.set();
	events_thread.wait_to_finish();

	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

	//destroy all windows
	for (KeyValue<WindowID, WindowData> &E : windows) {
#if defined(RD_ENABLED)
		if (rendering_device) {
			rendering_device->screen_free(E.key);
		}

		if (rendering_context) {
			rendering_context->window_destroy(E.key);
		}
#endif
#ifdef GLES3_ENABLED
		if (gl_manager) {
			gl_manager->window_destroy(E.key);
		}
		if (gl_manager_egl) {
			gl_manager_egl->window_destroy(E.key);
		}
#endif

		WindowData &wd = E.value;
		if (wd.xic) {
			XDestroyIC(wd.xic);
			wd.xic = nullptr;
		}
		XDestroyWindow(x11_display, wd.x11_xim_window);
#ifdef XKB_ENABLED
		if (xkb_loaded_v05p) {
			if (wd.xkb_state) {
				xkb_compose_state_unref(wd.xkb_state);
				wd.xkb_state = nullptr;
			}
		}
#endif
		XUnmapWindow(x11_display, wd.x11_window);
		XDestroyWindow(x11_display, wd.x11_window);
	}

#ifdef XKB_ENABLED
	if (xkb_loaded_v05p) {
		if (dead_tbl) {
			xkb_compose_table_unref(dead_tbl);
		}
		if (xkb_ctx) {
			xkb_context_unref(xkb_ctx);
		}
	}
#endif

	//destroy drivers
#if defined(RD_ENABLED)
	if (rendering_device) {
		memdelete(rendering_device);
		rendering_device = nullptr;
	}

	if (rendering_context) {
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif

#ifdef GLES3_ENABLED
	if (gl_manager) {
		memdelete(gl_manager);
		gl_manager = nullptr;
	}
	if (gl_manager_egl) {
		memdelete(gl_manager_egl);
		gl_manager_egl = nullptr;
	}
#endif

	if (xrandr_handle) {
		dlclose(xrandr_handle);
	}

	for (int i = 0; i < CURSOR_MAX; i++) {
		if (cursors[i] != None) {
			XFreeCursor(x11_display, cursors[i]);
		}
		if (cursor_img[i] != nullptr) {
			XcursorImageDestroy(cursor_img[i]);
		}
	}

	if (xim) {
		XCloseIM(xim);
	}

	XCloseDisplay(x11_display);
	if (xmbstring) {
		memfree(xmbstring);
	}

#ifdef SPEECHD_ENABLED
	if (tts) {
		memdelete(tts);
	}
#endif

#ifdef DBUS_ENABLED
	memdelete(screensaver);
	memdelete(portal_desktop);
#endif
}

void DisplayServerX11::register_x11_driver() {
	register_create_function("x11", create_func, get_rendering_drivers_func);
}

#endif // X11 enabled
