/**************************************************************************/
/*  udp_server.h                                                          */
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

#include "core/io/net_socket.h"
#include "core/io/packet_peer_udp.h"

class UDPServer : public RefCounted {
	GDCLASS(UDPServer, RefCounted);

protected:
	enum {
		PACKET_BUFFER_SIZE = 65536
	};

	struct Peer {
		PacketPeerUDP *peer = nullptr;
		IPAddress ip;
		uint16_t port = 0;

		bool operator==(const Peer &p_other) const {
			return (ip == p_other.ip && port == p_other.port);
		}
	};
	uint8_t recv_buffer[PACKET_BUFFER_SIZE];

	List<Peer> peers;
	List<Peer> pending;
	int max_pending_connections = 16;

	Ref<NetSocket> _sock;
	static void _bind_methods();

public:
	void remove_peer(IPAddress p_ip, int p_port);
	Error listen(uint16_t p_port, const IPAddress &p_bind_address = IPAddress("*"));
	Error poll();
	int get_local_port() const;
	bool is_listening() const;
	bool is_connection_available() const;
	void set_max_pending_connections(int p_max);
	int get_max_pending_connections() const;
	Ref<PacketPeerUDP> take_connection();

	void stop();

	UDPServer();
	~UDPServer();
};
