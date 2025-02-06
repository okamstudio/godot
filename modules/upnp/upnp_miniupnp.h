/**************************************************************************/
/*  upnp_miniupnp.h                                                       */
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

#pragma once

#ifndef WEB_ENABLED

#include "upnp.h"

#include <miniupnpc/miniupnpc.h>

class UPNPMiniUPNP : public UPNP {
	GDCLASS(UPNPMiniUPNP, UPNP);

private:
	static UPNP *_create(bool p_notify_postinitialize) { return static_cast<UPNP *>(ClassDB::creator<UPNPMiniUPNP>(p_notify_postinitialize)); }

	String discover_multicast_if = "";
	int discover_local_port = 0;
	bool discover_ipv6 = false;

	Vector<Ref<UPNPDevice>> devices;

	bool is_common_device(const String &dev) const;
	void add_device_to_list(UPNPDev *dev, UPNPDev *devlist);
	void parse_igd(Ref<UPNPDevice> dev, UPNPDev *devlist);
	char *load_description(const String &url, int *size, int *status_code) const;

public:
	static void make_default();

	static int upnp_result(int in);

	virtual int get_device_count() const override;
	virtual Ref<UPNPDevice> get_device(int index) const override;
	virtual void add_device(Ref<UPNPDevice> device) override;
	virtual void set_device(int index, Ref<UPNPDevice> device) override;
	virtual void remove_device(int index) override;
	virtual void clear_devices() override;

	virtual Ref<UPNPDevice> get_gateway() const override;

	virtual int discover(int timeout = 2000, int ttl = 2, const String &device_filter = "InternetGatewayDevice") override;

	virtual String query_external_address() const override;

	virtual int add_port_mapping(int port, int port_internal = 0, String desc = "", String proto = "UDP", int duration = 0) const override;
	virtual int delete_port_mapping(int port, String proto = "UDP") const override;

	virtual void set_discover_multicast_if(const String &m_if) override;
	virtual String get_discover_multicast_if() const override;

	virtual void set_discover_local_port(int port) override;
	virtual int get_discover_local_port() const override;

	virtual void set_discover_ipv6(bool ipv6) override;
	virtual bool is_discover_ipv6() const override;

	UPNPMiniUPNP() {}
	virtual ~UPNPMiniUPNP() {}
};

#endif // WEB_ENABLED
