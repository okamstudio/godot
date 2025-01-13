/**************************************************************************/
/*  upnp_device_miniupnp.h                                                */
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

#ifndef UPNP_DEVICE_MINIUPNP_H
#define UPNP_DEVICE_MINIUPNP_H

#ifndef WEB_ENABLED

#include "upnp_device.h"

class UPNPDeviceMiniUPNP : public UPNPDevice {
	GDCLASS(UPNPDeviceMiniUPNP, UPNPDevice);

private:
	static UPNPDevice *_create(bool p_notify_postinitialize) { return static_cast<UPNPDevice *>(ClassDB::creator<UPNPDeviceMiniUPNP>(p_notify_postinitialize)); }

	String description_url;
	String service_type;
	String igd_control_url;
	String igd_service_type;
	String igd_our_addr;
	IGDStatus igd_status = IGD_STATUS_UNKNOWN_ERROR;

public:
	static void make_default();

	virtual void set_description_url(const String &url) override;
	virtual String get_description_url() const override;

	virtual void set_service_type(const String &type) override;
	virtual String get_service_type() const override;

	virtual void set_igd_control_url(const String &url) override;
	virtual String get_igd_control_url() const override;

	virtual void set_igd_service_type(const String &type) override;
	virtual String get_igd_service_type() const override;

	virtual void set_igd_our_addr(const String &addr) override;
	virtual String get_igd_our_addr() const override;

	virtual void set_igd_status(IGDStatus status) override;
	virtual IGDStatus get_igd_status() const override;

	virtual bool is_valid_gateway() const override;
	virtual String query_external_address() const override;
	virtual int add_port_mapping(int port, int port_internal = 0, String desc = "", String proto = "UDP", int duration = 0) const override;
	virtual int delete_port_mapping(int port, String proto = "UDP") const override;

	UPNPDeviceMiniUPNP() {}
	virtual ~UPNPDeviceMiniUPNP() {}
};

#endif // WEB_ENABLED

#endif // UPNP_DEVICE_MINIUPNP_H
