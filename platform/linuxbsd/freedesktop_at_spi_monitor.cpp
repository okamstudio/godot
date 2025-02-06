/**************************************************************************/
/*  freedesktop_at_spi_monitor.cpp                                        */
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

#include "freedesktop_at_spi_monitor.h"

#ifdef DBUS_ENABLED

#include "core/error/error_macros.h"
#include "core/os/os.h"
#include "core/string/ustring.h"

#ifdef SOWRAP_ENABLED
#include "dbus-so_wrap.h"
#else
#include <dbus/dbus.h>
#endif

#include <unistd.h>

#include "core/variant/variant.h"

#define BUS_OBJECT_NAME "org.a11y.Bus"
#define BUS_OBJECT_PATH "/org/a11y/bus"

#define BUS_INTERFACE_PROPERTIES "org.freedesktop.DBus.Properties"

void FreeDesktopAtSPIMonitor::monitor_thread_func(void *p_userdata) {
	FreeDesktopAtSPIMonitor *mon = (FreeDesktopAtSPIMonitor *)p_userdata;

	DBusError error;
	dbus_error_init(&error);

	DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		mon->supported.clear();
		return;
	}

	static const char *iface = "org.a11y.Status";
	static const char *member = "IsEnabled";

	while (!mon->exit_thread.is_set()) {
		DBusMessage *message = dbus_message_new_method_call(BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE_PROPERTIES, "Get");

		dbus_message_append_args(
				message,
				DBUS_TYPE_STRING, &iface,
				DBUS_TYPE_STRING, &member,
				DBUS_TYPE_INVALID);

		DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 50, &error);
		dbus_message_unref(message);

		if (!dbus_error_is_set(&error)) {
			DBusMessageIter iter, iter_variant, iter_struct;
			dbus_bool_t result;
			dbus_message_iter_init(reply, &iter);
			dbus_message_iter_recurse(&iter, &iter_variant);
			switch (dbus_message_iter_get_arg_type(&iter_variant)) {
				case DBUS_TYPE_STRUCT: {
					dbus_message_iter_recurse(&iter_variant, &iter_struct);
					if (dbus_message_iter_get_arg_type(&iter_struct) == DBUS_TYPE_BOOLEAN) {
						dbus_message_iter_get_basic(&iter_struct, &result);
						if (result) {
							mon->sr_enabled.set();
						} else {
							mon->sr_enabled.clear();
						}
					}
				} break;
				case DBUS_TYPE_BOOLEAN: {
					dbus_message_iter_get_basic(&iter_variant, &result);
					if (result) {
						mon->sr_enabled.set();
					} else {
						mon->sr_enabled.clear();
					}
				} break;
				default:
					break;
			}
			dbus_message_unref(reply);
		} else {
			dbus_error_free(&error);
		}

		usleep(50000);
	}

	dbus_connection_unref(bus);
}

FreeDesktopAtSPIMonitor::FreeDesktopAtSPIMonitor() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	if (initialize_dbus(dylibloader_verbose) != 0) {
		print_verbose("AT-SPI2: Failed to load DBus library!");
		supported.clear();
		return;
	}
#endif
	bool ver_ok = false;
	int version_major = 0;
	int version_minor = 0;
	int version_rev = 0;
	dbus_get_version(&version_major, &version_minor, &version_rev);
	ver_ok = (version_major == 1 && version_minor >= 10) || (version_major > 1); // 1.10.0
	print_verbose(vformat("AT-SPI2: DBus %d.%d.%d detected.", version_major, version_minor, version_rev));
	if (!ver_ok) {
		print_verbose("AT-SPI2: Unsupported DBus library version!");
		supported.clear();
		return;
	}

	supported.set();
	sr_enabled.clear();
	exit_thread.clear();

	thread.start(FreeDesktopAtSPIMonitor::monitor_thread_func, this);
}

FreeDesktopAtSPIMonitor::~FreeDesktopAtSPIMonitor() {
	exit_thread.set();
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
}

#endif // DBUS_ENABLED
