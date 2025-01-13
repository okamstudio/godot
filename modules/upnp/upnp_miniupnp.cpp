/**************************************************************************/
/*  upnp_miniupnp.cpp                                                     */
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

#ifndef WEB_ENABLED

#include "upnp_miniupnp.h"

#include "upnp_device_miniupnp.h"

#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>

#include <stdlib.h>

void UPNPMiniUPNP::make_default() {
	UPNP::_create = UPNPMiniUPNP::_create;
}

bool UPNPMiniUPNP::is_common_device(const String &dev) const {
	return dev.is_empty() ||
			dev.contains("InternetGatewayDevice") ||
			dev.contains("WANIPConnection") ||
			dev.contains("WANPPPConnection") ||
			dev.contains("rootdevice");
}

int UPNPMiniUPNP::discover(int timeout, int ttl, const String &device_filter) {
	ERR_FAIL_COND_V_MSG(timeout < 0, UPNP_RESULT_INVALID_PARAM, "The response's wait time can't be negative.");
	ERR_FAIL_COND_V_MSG(ttl < 0 || ttl > 255, UPNP_RESULT_INVALID_PARAM, "The time-to-live must be set between 0 and 255 (inclusive).");

	devices.clear();

	int error = 0;
	struct UPNPDev *devlist;

	CharString cs = discover_multicast_if.utf8();
	const char *m_if = cs.length() ? cs.get_data() : nullptr;
	if (is_common_device(device_filter)) {
		devlist = upnpDiscover(timeout, m_if, nullptr, discover_local_port, discover_ipv6, ttl, &error);
	} else {
		devlist = upnpDiscoverAll(timeout, m_if, nullptr, discover_local_port, discover_ipv6, ttl, &error);
	}

	if (error != UPNPDISCOVER_SUCCESS) {
		switch (error) {
			case UPNPDISCOVER_SOCKET_ERROR:
				return UPNP_RESULT_SOCKET_ERROR;
			case UPNPDISCOVER_MEMORY_ERROR:
				return UPNP_RESULT_MEM_ALLOC_ERROR;
			default:
				return UPNP_RESULT_UNKNOWN_ERROR;
		}
	}

	if (!devlist) {
		return UPNP_RESULT_NO_DEVICES;
	}

	struct UPNPDev *dev = devlist;

	while (dev) {
		if (device_filter.is_empty() || strstr(dev->st, device_filter.utf8().get_data())) {
			add_device_to_list(dev, devlist);
		}

		dev = dev->pNext;
	}

	freeUPNPDevlist(devlist);

	return UPNP_RESULT_SUCCESS;
}

void UPNPMiniUPNP::add_device_to_list(UPNPDev *dev, UPNPDev *devlist) {
	Ref<UPNPDeviceMiniUPNP> new_device;
	new_device.instantiate();

	new_device->set_description_url(dev->descURL);
	new_device->set_service_type(dev->st);

	parse_igd(new_device, devlist);

	devices.push_back(new_device);
}

char *UPNPMiniUPNP::load_description(const String &url, int *size, int *status_code) const {
	return (char *)miniwget(url.utf8().get_data(), size, 0, status_code);
}

void UPNPMiniUPNP::parse_igd(Ref<UPNPDevice> dev, UPNPDev *devlist) {
	int size = 0;
	int status_code = -1;
	char *xml = load_description(dev->get_description_url(), &size, &status_code);

	if (status_code != 200) {
		dev->set_igd_status(UPNPDevice::IGD_STATUS_HTTP_ERROR);
		return;
	}

	if (!xml || size < 1) {
		dev->set_igd_status(UPNPDevice::IGD_STATUS_HTTP_EMPTY);
		return;
	}

	struct UPNPUrls urls = {};
	struct IGDdatas data;

	parserootdesc(xml, size, &data);
	free(xml);
	xml = nullptr;

	GetUPNPUrls(&urls, &data, dev->get_description_url().utf8().get_data(), 0);

	char addr[16];
#if MINIUPNPC_API_VERSION >= 18
	int i = UPNP_GetValidIGD(devlist, &urls, &data, (char *)&addr, 16, nullptr, 0);
#else
	int i = UPNP_GetValidIGD(devlist, &urls, &data, (char *)&addr, 16);
#endif

	if (i != 1) {
		FreeUPNPUrls(&urls);

		switch (i) {
			case 0:
				dev->set_igd_status(UPNPDevice::IGD_STATUS_NO_IGD);
				return;
			case 2:
				dev->set_igd_status(UPNPDevice::IGD_STATUS_DISCONNECTED);
				return;
			case 3:
				dev->set_igd_status(UPNPDevice::IGD_STATUS_UNKNOWN_DEVICE);
				return;
			default:
				dev->set_igd_status(UPNPDevice::IGD_STATUS_UNKNOWN_ERROR);
				return;
		}
	}

	if (urls.controlURL[0] == '\0') {
		FreeUPNPUrls(&urls);
		dev->set_igd_status(UPNPDevice::IGD_STATUS_INVALID_CONTROL);
		return;
	}

	dev->set_igd_control_url(urls.controlURL);
	dev->set_igd_service_type(data.first.servicetype);
	dev->set_igd_our_addr(addr);
	dev->set_igd_status(UPNPDevice::IGD_STATUS_OK);

	FreeUPNPUrls(&urls);
}

int UPNPMiniUPNP::upnp_result(int in) {
	switch (in) {
		case UPNPCOMMAND_SUCCESS:
			return UPNP_RESULT_SUCCESS;
		case UPNPCOMMAND_UNKNOWN_ERROR:
			return UPNP_RESULT_UNKNOWN_ERROR;
		case UPNPCOMMAND_INVALID_ARGS:
			return UPNP_RESULT_INVALID_ARGS;
		case UPNPCOMMAND_HTTP_ERROR:
			return UPNP_RESULT_HTTP_ERROR;
		case UPNPCOMMAND_INVALID_RESPONSE:
			return UPNP_RESULT_INVALID_RESPONSE;
		case UPNPCOMMAND_MEM_ALLOC_ERROR:
			return UPNP_RESULT_MEM_ALLOC_ERROR;

		case 402:
			return UPNP_RESULT_INVALID_ARGS;
		case 403:
			return UPNP_RESULT_NOT_AUTHORIZED;
		case 501:
			return UPNP_RESULT_ACTION_FAILED;
		case 606:
			return UPNP_RESULT_NOT_AUTHORIZED;
		case 714:
			return UPNP_RESULT_NO_SUCH_ENTRY_IN_ARRAY;
		case 715:
			return UPNP_RESULT_SRC_IP_WILDCARD_NOT_PERMITTED;
		case 716:
			return UPNP_RESULT_EXT_PORT_WILDCARD_NOT_PERMITTED;
		case 718:
			return UPNP_RESULT_CONFLICT_WITH_OTHER_MAPPING;
		case 724:
			return UPNP_RESULT_SAME_PORT_VALUES_REQUIRED;
		case 725:
			return UPNP_RESULT_ONLY_PERMANENT_LEASE_SUPPORTED;
		case 726:
			return UPNP_RESULT_REMOTE_HOST_MUST_BE_WILDCARD;
		case 727:
			return UPNP_RESULT_EXT_PORT_MUST_BE_WILDCARD;
		case 728:
			return UPNP_RESULT_NO_PORT_MAPS_AVAILABLE;
		case 729:
			return UPNP_RESULT_CONFLICT_WITH_OTHER_MECHANISM;
		case 732:
			return UPNP_RESULT_INT_PORT_WILDCARD_NOT_PERMITTED;
		case 733:
			return UPNP_RESULT_INCONSISTENT_PARAMETERS;
	}

	return UPNP_RESULT_UNKNOWN_ERROR;
}

int UPNPMiniUPNP::get_device_count() const {
	return devices.size();
}

Ref<UPNPDevice> UPNPMiniUPNP::get_device(int index) const {
	ERR_FAIL_INDEX_V(index, devices.size(), nullptr);

	return devices.get(index);
}

void UPNPMiniUPNP::add_device(Ref<UPNPDevice> device) {
	ERR_FAIL_COND(device.is_null());

	devices.push_back(device);
}

void UPNPMiniUPNP::set_device(int index, Ref<UPNPDevice> device) {
	ERR_FAIL_INDEX(index, devices.size());
	ERR_FAIL_COND(device.is_null());

	devices.set(index, device);
}

void UPNPMiniUPNP::remove_device(int index) {
	ERR_FAIL_INDEX(index, devices.size());

	devices.remove_at(index);
}

void UPNPMiniUPNP::clear_devices() {
	devices.clear();
}

Ref<UPNPDevice> UPNPMiniUPNP::get_gateway() const {
	ERR_FAIL_COND_V_MSG(devices.is_empty(), nullptr, "Couldn't find any UPNPDevices.");

	for (int i = 0; i < devices.size(); i++) {
		Ref<UPNPDevice> dev = get_device(i);

		if (dev.is_valid() && dev->is_valid_gateway()) {
			return dev;
		}
	}

	return nullptr;
}

void UPNPMiniUPNP::set_discover_multicast_if(const String &m_if) {
	discover_multicast_if = m_if;
}

String UPNPMiniUPNP::get_discover_multicast_if() const {
	return discover_multicast_if;
}

void UPNPMiniUPNP::set_discover_local_port(int port) {
	discover_local_port = port;
}

int UPNPMiniUPNP::get_discover_local_port() const {
	return discover_local_port;
}

void UPNPMiniUPNP::set_discover_ipv6(bool ipv6) {
	discover_ipv6 = ipv6;
}

bool UPNPMiniUPNP::is_discover_ipv6() const {
	return discover_ipv6;
}

String UPNPMiniUPNP::query_external_address() const {
	Ref<UPNPDevice> dev = get_gateway();

	if (dev.is_null()) {
		return "";
	}

	return dev->query_external_address();
}

int UPNPMiniUPNP::add_port_mapping(int port, int port_internal, String desc, String proto, int duration) const {
	Ref<UPNPDevice> dev = get_gateway();

	if (dev.is_null()) {
		return UPNP_RESULT_NO_GATEWAY;
	}

	return dev->add_port_mapping(port, port_internal, desc, proto, duration);
}

int UPNPMiniUPNP::delete_port_mapping(int port, String proto) const {
	Ref<UPNPDevice> dev = get_gateway();

	if (dev.is_null()) {
		return UPNP_RESULT_NO_GATEWAY;
	}

	return dev->delete_port_mapping(port, proto);
}

#endif // WEB_ENABLED
