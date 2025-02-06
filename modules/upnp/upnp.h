/**************************************************************************/
/*  upnp.h                                                                */
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

#include "upnp_device.h"

#include "core/object/ref_counted.h"

class UPNP : public RefCounted {
	GDCLASS(UPNP, RefCounted);

protected:
	static void _bind_methods();

	static UPNP *(*_create)(bool p_notify_postinitialize);

public:
	enum UPNPResult {
		UPNP_RESULT_SUCCESS,
		UPNP_RESULT_NOT_AUTHORIZED,
		UPNP_RESULT_PORT_MAPPING_NOT_FOUND,
		UPNP_RESULT_INCONSISTENT_PARAMETERS,
		UPNP_RESULT_NO_SUCH_ENTRY_IN_ARRAY,
		UPNP_RESULT_ACTION_FAILED,
		UPNP_RESULT_SRC_IP_WILDCARD_NOT_PERMITTED,
		UPNP_RESULT_EXT_PORT_WILDCARD_NOT_PERMITTED,
		UPNP_RESULT_INT_PORT_WILDCARD_NOT_PERMITTED,
		UPNP_RESULT_REMOTE_HOST_MUST_BE_WILDCARD,
		UPNP_RESULT_EXT_PORT_MUST_BE_WILDCARD,
		UPNP_RESULT_NO_PORT_MAPS_AVAILABLE,
		UPNP_RESULT_CONFLICT_WITH_OTHER_MECHANISM,
		UPNP_RESULT_CONFLICT_WITH_OTHER_MAPPING,
		UPNP_RESULT_SAME_PORT_VALUES_REQUIRED,
		UPNP_RESULT_ONLY_PERMANENT_LEASE_SUPPORTED,
		UPNP_RESULT_INVALID_GATEWAY,
		UPNP_RESULT_INVALID_PORT,
		UPNP_RESULT_INVALID_PROTOCOL,
		UPNP_RESULT_INVALID_DURATION,
		UPNP_RESULT_INVALID_ARGS,
		UPNP_RESULT_INVALID_RESPONSE,
		UPNP_RESULT_INVALID_PARAM,
		UPNP_RESULT_HTTP_ERROR,
		UPNP_RESULT_SOCKET_ERROR,
		UPNP_RESULT_MEM_ALLOC_ERROR,
		UPNP_RESULT_NO_GATEWAY,
		UPNP_RESULT_NO_DEVICES,
		UPNP_RESULT_UNKNOWN_ERROR,
	};

	static UPNP *create(bool p_notify_postinitialize = true) {
		if (!_create) {
			return nullptr;
		}
		return _create(p_notify_postinitialize);
	}

	virtual int get_device_count() const = 0;
	virtual Ref<UPNPDevice> get_device(int index) const = 0;
	virtual void add_device(Ref<UPNPDevice> device) = 0;
	virtual void set_device(int index, Ref<UPNPDevice> device) = 0;
	virtual void remove_device(int index) = 0;
	virtual void clear_devices() = 0;

	virtual Ref<UPNPDevice> get_gateway() const = 0;

	virtual int discover(int timeout = 2000, int ttl = 2, const String &device_filter = "InternetGatewayDevice") = 0;

	virtual String query_external_address() const = 0;

	virtual int add_port_mapping(int port, int port_internal = 0, String desc = "", String proto = "UDP", int duration = 0) const = 0;
	virtual int delete_port_mapping(int port, String proto = "UDP") const = 0;

	virtual void set_discover_multicast_if(const String &m_if) = 0;
	virtual String get_discover_multicast_if() const = 0;

	virtual void set_discover_local_port(int port) = 0;
	virtual int get_discover_local_port() const = 0;

	virtual void set_discover_ipv6(bool ipv6) = 0;
	virtual bool is_discover_ipv6() const = 0;

	UPNP() {}
	virtual ~UPNP() {}
};

VARIANT_ENUM_CAST(UPNP::UPNPResult)
