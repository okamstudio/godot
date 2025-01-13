/**************************************************************************/
/*  upnp_device.h                                                         */
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

#ifndef UPNP_DEVICE_H
#define UPNP_DEVICE_H

#include "core/object/ref_counted.h"

class UPNPDevice : public RefCounted {
	GDCLASS(UPNPDevice, RefCounted);

protected:
	static void _bind_methods();

	static UPNPDevice *(*_create)(bool p_notify_postinitialize);

public:
	enum IGDStatus {
		IGD_STATUS_OK,
		IGD_STATUS_HTTP_ERROR,
		IGD_STATUS_HTTP_EMPTY,
		IGD_STATUS_NO_URLS,
		IGD_STATUS_NO_IGD,
		IGD_STATUS_DISCONNECTED,
		IGD_STATUS_UNKNOWN_DEVICE,
		IGD_STATUS_INVALID_CONTROL,
		IGD_STATUS_MALLOC_ERROR,
		IGD_STATUS_UNKNOWN_ERROR,
	};

	static UPNPDevice *create(bool p_notify_postinitialize = true) {
		if (!_create) {
			return nullptr;
		}
		return _create(p_notify_postinitialize);
	}

	virtual void set_description_url(const String &url) = 0;
	virtual String get_description_url() const = 0;

	virtual void set_service_type(const String &type) = 0;
	virtual String get_service_type() const = 0;

	virtual void set_igd_control_url(const String &url) = 0;
	virtual String get_igd_control_url() const = 0;

	virtual void set_igd_service_type(const String &type) = 0;
	virtual String get_igd_service_type() const = 0;

	virtual void set_igd_our_addr(const String &addr) = 0;
	virtual String get_igd_our_addr() const = 0;

	virtual void set_igd_status(IGDStatus status) = 0;
	virtual IGDStatus get_igd_status() const = 0;

	virtual bool is_valid_gateway() const = 0;
	virtual String query_external_address() const = 0;
	virtual int add_port_mapping(int port, int port_internal = 0, String desc = "", String proto = "UDP", int duration = 0) const = 0;
	virtual int delete_port_mapping(int port, String proto = "UDP") const = 0;

	UPNPDevice() {}
	virtual ~UPNPDevice() {}
};

VARIANT_ENUM_CAST(UPNPDevice::IGDStatus)

#endif // UPNP_DEVICE_H
