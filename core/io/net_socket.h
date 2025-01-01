/**************************************************************************/
/*  net_socket.h                                                          */
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

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include "core/io/ip.h"
#include "core/object/ref_counted.h"

class NetSocket : public RefCounted {
protected:
	static NetSocket *(*_create)();

public:
	static NetSocket *create();

	enum PollType : int32_t {
		POLL_TYPE_IN,
		POLL_TYPE_OUT,
		POLL_TYPE_IN_OUT
	};

	enum Type : int32_t {
		TYPE_NONE,
		TYPE_TCP,
		TYPE_UDP,
	};

	virtual Error open(Type p_type, IP::Type &ip_type) = 0;
	virtual void close() = 0;
	virtual Error bind(IPAddress p_addr, uint16_t p_port) = 0;
	virtual Error listen(int p_max_pending) = 0;
	virtual Error connect_to_host(IPAddress p_addr, uint16_t p_port) = 0;
	virtual Error poll(PollType p_type, int timeout) const = 0;
	virtual Error recv(uint8_t *p_buffer, int p_len, int &r_read) = 0;
	virtual Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port, bool p_peek = false) = 0;
	virtual Error send(const uint8_t *p_buffer, int p_len, int &r_sent) = 0;
	virtual Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) = 0;
	virtual Ref<NetSocket> accept(IPAddress &r_ip, uint16_t &r_port) = 0;

	virtual bool is_open() const = 0;
	virtual int get_available_bytes() const = 0;
	virtual Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) const = 0;

	virtual Error set_broadcasting_enabled(bool p_enabled) = 0; // Returns OK if the socket option has been set successfully.
	virtual void set_blocking_enabled(bool p_enabled) = 0;
	virtual void set_ipv6_only_enabled(bool p_enabled) = 0;
	virtual void set_tcp_no_delay_enabled(bool p_enabled) = 0;
	virtual void set_reuse_address_enabled(bool p_enabled) = 0;
	virtual Error join_multicast_group(const IPAddress &p_multi_address, const String &p_if_name) = 0;
	virtual Error leave_multicast_group(const IPAddress &p_multi_address, const String &p_if_name) = 0;

	virtual ~NetSocket() {}
};

#endif // NET_SOCKET_H
