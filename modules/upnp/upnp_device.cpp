/**************************************************************************/
/*  upnp_device.cpp                                                       */
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

#include "upnp_device.h"

UPNPDevice *(*UPNPDevice::_create)(bool p_notify_postinitialize) = nullptr;

void UPNPDevice::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid_gateway"), &UPNPDevice::is_valid_gateway);
	ClassDB::bind_method(D_METHOD("query_external_address"), &UPNPDevice::query_external_address);
	ClassDB::bind_method(D_METHOD("add_port_mapping", "port", "port_internal", "desc", "proto", "duration"), &UPNPDevice::add_port_mapping, DEFVAL(0), DEFVAL(""), DEFVAL("UDP"), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("delete_port_mapping", "port", "proto"), &UPNPDevice::delete_port_mapping, DEFVAL("UDP"));

	ClassDB::bind_method(D_METHOD("set_description_url", "url"), &UPNPDevice::set_description_url);
	ClassDB::bind_method(D_METHOD("get_description_url"), &UPNPDevice::get_description_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description_url"), "set_description_url", "get_description_url");

	ClassDB::bind_method(D_METHOD("set_service_type", "type"), &UPNPDevice::set_service_type);
	ClassDB::bind_method(D_METHOD("get_service_type"), &UPNPDevice::get_service_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "service_type"), "set_service_type", "get_service_type");

	ClassDB::bind_method(D_METHOD("set_igd_control_url", "url"), &UPNPDevice::set_igd_control_url);
	ClassDB::bind_method(D_METHOD("get_igd_control_url"), &UPNPDevice::get_igd_control_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "igd_control_url"), "set_igd_control_url", "get_igd_control_url");

	ClassDB::bind_method(D_METHOD("set_igd_service_type", "type"), &UPNPDevice::set_igd_service_type);
	ClassDB::bind_method(D_METHOD("get_igd_service_type"), &UPNPDevice::get_igd_service_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "igd_service_type"), "set_igd_service_type", "get_igd_service_type");

	ClassDB::bind_method(D_METHOD("set_igd_our_addr", "addr"), &UPNPDevice::set_igd_our_addr);
	ClassDB::bind_method(D_METHOD("get_igd_our_addr"), &UPNPDevice::get_igd_our_addr);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "igd_our_addr"), "set_igd_our_addr", "get_igd_our_addr");

	ClassDB::bind_method(D_METHOD("set_igd_status", "status"), &UPNPDevice::set_igd_status);
	ClassDB::bind_method(D_METHOD("get_igd_status"), &UPNPDevice::get_igd_status);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "igd_status", PROPERTY_HINT_ENUM), "set_igd_status", "get_igd_status");

	BIND_ENUM_CONSTANT(IGD_STATUS_OK);
	BIND_ENUM_CONSTANT(IGD_STATUS_HTTP_ERROR);
	BIND_ENUM_CONSTANT(IGD_STATUS_HTTP_EMPTY);
	BIND_ENUM_CONSTANT(IGD_STATUS_NO_URLS);
	BIND_ENUM_CONSTANT(IGD_STATUS_NO_IGD);
	BIND_ENUM_CONSTANT(IGD_STATUS_DISCONNECTED);
	BIND_ENUM_CONSTANT(IGD_STATUS_UNKNOWN_DEVICE);
	BIND_ENUM_CONSTANT(IGD_STATUS_INVALID_CONTROL);
	BIND_ENUM_CONSTANT(IGD_STATUS_MALLOC_ERROR);
	BIND_ENUM_CONSTANT(IGD_STATUS_UNKNOWN_ERROR);
}
