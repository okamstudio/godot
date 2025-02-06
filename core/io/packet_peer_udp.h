/**************************************************************************/
/*  packet_peer_udp.h                                                     */
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

#include "core/io/ip.h"
#include "core/io/net_socket.h"
#include "core/io/packet_peer.h"

class UDPServer;

class PacketPeerUDP : public PacketPeer {
	GDCLASS(PacketPeerUDP, PacketPeer);

protected:
	enum {
		PACKET_BUFFER_SIZE = 65536
	};

	RingBuffer<uint8_t> rb;
	uint8_t recv_buffer[PACKET_BUFFER_SIZE];
	uint8_t packet_buffer[PACKET_BUFFER_SIZE];
	IPAddress packet_ip;
	int packet_port = 0;
	int queue_count = 0;

	IPAddress peer_addr;
	int peer_port = 0;
	bool connected = false;
	bool blocking = true;
	bool broadcast = false;
	UDPServer *udp_server = nullptr;
	Ref<NetSocket> _sock;

	static void _bind_methods();

	String _get_packet_ip() const;

	Error _set_dest_address(const String &p_address, int p_port);
	Error _poll();

public:
	void set_blocking_mode(bool p_enable);

	Error bind(int p_port, const IPAddress &p_bind_address = IPAddress("*"), int p_recv_buffer_size = 65536);
	void close();
	Error wait();
	bool is_bound() const;

	Error connect_shared_socket(Ref<NetSocket> p_sock, IPAddress p_ip, uint16_t p_port, UDPServer *ref); // Used by UDPServer
	void disconnect_shared_socket(); // Used by UDPServer
	Error store_packet(IPAddress p_ip, uint32_t p_port, uint8_t *p_buf, int p_buf_size); // Used internally and by UDPServer
	Error connect_to_host(const IPAddress &p_host, int p_port);
	bool is_socket_connected() const;

	IPAddress get_packet_address() const;
	int get_packet_port() const;
	int get_local_port() const;
	void set_dest_address(const IPAddress &p_address, int p_port);

	Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
	int get_available_packet_count() const override;
	int get_max_packet_size() const override;
	void set_broadcast_enabled(bool p_enabled);
	Error join_multicast_group(IPAddress p_multi_address, const String &p_if_name);
	Error leave_multicast_group(IPAddress p_multi_address, const String &p_if_name);

	PacketPeerUDP();
	~PacketPeerUDP();
};
