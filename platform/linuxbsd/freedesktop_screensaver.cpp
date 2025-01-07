/**************************************************************************/
/*  freedesktop_screensaver.cpp                                           */
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

#include "freedesktop_screensaver.h"

#ifdef DBUS_ENABLED

#include "core/config/project_settings.h"

#ifdef SOWRAP_ENABLED
#include "dbus-so_wrap.h"
#else
#include <dbus/dbus.h>
#endif

#define BUS_OBJECT_NAME "org.freedesktop.ScreenSaver"
#define BUS_OBJECT_PATH "/org/freedesktop/ScreenSaver"
#define BUS_INTERFACE "org.freedesktop.ScreenSaver"

void FreeDesktopScreenSaver::inhibit() {
	if (unsupported) {
		return;
	}

	DBusError error;
	dbus_error_init(&error);

	DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		unsupported = true;
		return;
	}

	String app_name_string = GLOBAL_GET("application/config/name");
	CharString app_name_utf8 = app_name_string.utf8();
	const char *app_name = app_name_string.is_empty() ? "Godot Engine" : app_name_utf8.get_data();

	const char *reason = "Running Godot Engine project";

	DBusMessage *message = dbus_message_new_method_call(
			BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE,
			"Inhibit");
	dbus_message_append_args(
			message,
			DBUS_TYPE_STRING, &app_name,
			DBUS_TYPE_STRING, &reason,
			DBUS_TYPE_INVALID);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 50, &error);
	dbus_message_unref(message);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_connection_unref(bus);
		unsupported = false;
		return;
	}

	DBusMessageIter reply_iter;
	dbus_message_iter_init(reply, &reply_iter);
	dbus_message_iter_get_basic(&reply_iter, &cookie);
	PRINT_VERBOSE("FreeDesktopScreenSaver: Acquired screensaver inhibition cookie: " + uitos(cookie));

	dbus_message_unref(reply);
	dbus_connection_unref(bus);
}

void FreeDesktopScreenSaver::uninhibit() {
	if (unsupported) {
		return;
	}

	DBusError error;
	dbus_error_init(&error);

	DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		unsupported = true;
		return;
	}

	DBusMessage *message = dbus_message_new_method_call(
			BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE,
			"UnInhibit");
	dbus_message_append_args(
			message,
			DBUS_TYPE_UINT32, &cookie,
			DBUS_TYPE_INVALID);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 50, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_connection_unref(bus);
		unsupported = true;
		return;
	}

	PRINT_VERBOSE("FreeDesktopScreenSaver: Released screensaver inhibition cookie: " + uitos(cookie));

	dbus_message_unref(message);
	dbus_message_unref(reply);
	dbus_connection_unref(bus);
}

FreeDesktopScreenSaver::FreeDesktopScreenSaver() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	unsupported = (initialize_dbus(dylibloader_verbose) != 0);
#else
	unsupported = false;
#endif

	if (unsupported) {
		return;
	}

	bool ver_ok = false;
	int version_major = 0;
	int version_minor = 0;
	int version_rev = 0;
	dbus_get_version(&version_major, &version_minor, &version_rev);
	ver_ok = (version_major == 1 && version_minor >= 10) || (version_major > 1); // 1.10.0
	PRINT_VERBOSE(vformat("ScreenSaver: DBus %d.%d.%d detected.", version_major, version_minor, version_rev));
	if (!ver_ok) {
		PRINT_VERBOSE("ScreenSaver:: Unsupported DBus library version!");
		unsupported = true;
	}
}

#endif // DBUS_ENABLED
