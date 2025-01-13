/**************************************************************************/
/*  packet_peer_udp.cpp                                                   */
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

#include "packet_peer_udp.h"

#include "core/io/ip.h"
#include "core/io/udp_server.h"

void PacketPeerUDP::set_blocking_mode(bool p_enable) {
	blocking = p_enable;
}

void PacketPeerUDP::set_broadcast_enabled(bool p_enabled) {
	ERR_FAIL_COND(udp_server);
	broadcast = p_enabled;
	if (_sock.is_valid() && _sock->is_open()) {
		_sock->set_broadcasting_enabled(p_enabled);
	}
}

Error PacketPeerUDP::join_multicast_group(IPAddress p_multi_address, const String &p_if_name) {
	ERR_FAIL_COND_V(udp_server, ERR_LOCKED);
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!p_multi_address.is_valid(), ERR_INVALID_PARAMETER);

	if (!_sock->is_open()) {
		IP::Type ip_type = p_multi_address.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
		Error err = _sock->open(NetSocket::TYPE_UDP, ip_type);
		ERR_FAIL_COND_V(err != OK, err);
		_sock->set_blocking_enabled(false);
		_sock->set_broadcasting_enabled(broadcast);
	}
	return _sock->join_multicast_group(p_multi_address, p_if_name);
}

Error PacketPeerUDP::leave_multicast_group(IPAddress p_multi_address, const String &p_if_name) {
	ERR_FAIL_COND_V(udp_server, ERR_LOCKED);
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!_sock->is_open(), ERR_UNCONFIGURED);
	return _sock->leave_multicast_group(p_multi_address, p_if_name);
}

String PacketPeerUDP::_get_packet_ip() const {
	return get_packet_address();
}

Error PacketPeerUDP::_set_dest_address(const String &p_address, int p_port) {
	IPAddress ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_address);
		if (!ip.is_valid()) {
			return ERR_CANT_RESOLVE;
		}
	}

	set_dest_address(ip, p_port);
	return OK;
}

int PacketPeerUDP::get_available_packet_count() const {
	// TODO we should deprecate this, and expose poll instead!
	Error err = const_cast<PacketPeerUDP *>(this)->_poll();
	if (err != OK) {
		return -1;
	}

	return queue_count;
}

Error PacketPeerUDP::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	Error err = _poll();
	if (err != OK) {
		return err;
	}
	if (queue_count == 0) {
		return ERR_UNAVAILABLE;
	}

/* Bogus GCC warning here:
 * In member function 'int RingBuffer<T>::read(T*, int, bool) [with T = unsigned char]',
 *     inlined from 'virtual Error PacketPeerUDP::get_packet(const uint8_t**, int&)' at core/io/packet_peer_udp.cpp:112:9,
 *     inlined from 'virtual Error PacketPeerUDP::get_packet(const uint8_t**, int&)' at core/io/packet_peer_udp.cpp:99:7:
 * Error: ./core/ring_buffer.h:68:46: error: writing 1 byte into a region of size 0 [-Werror=stringop-overflow=]
 *   68 |                                 p_buf[dst++] = read[pos + i];
 *      |                                 ~~~~~~~~~~~~~^~~~~~~
 */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wstringop-overflow=0"
#endif

	uint32_t size = 0;
	uint8_t ipv6[16] = {};
	rb.read(ipv6, 16, true);
	packet_ip.set_ipv6(ipv6);
	rb.read((uint8_t *)&packet_port, 4, true);
	rb.read((uint8_t *)&size, 4, true);
	rb.read(packet_buffer, size, true);
	--queue_count;
	*r_buffer = packet_buffer;
	r_buffer_size = size;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

	return OK;
}

Error PacketPeerUDP::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!peer_addr.is_valid(), ERR_UNCONFIGURED);

	Error err;
	int sent = -1;

	if (!_sock->is_open()) {
		IP::Type ip_type = peer_addr.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
		err = _sock->open(NetSocket::TYPE_UDP, ip_type);
		ERR_FAIL_COND_V(err != OK, err);
		_sock->set_blocking_enabled(false);
		_sock->set_broadcasting_enabled(broadcast);
	}

	do {
		if (connected && !udp_server) {
			err = _sock->send(p_buffer, p_buffer_size, sent);
		} else {
			err = _sock->sendto(p_buffer, p_buffer_size, sent, peer_addr, peer_port);
		}
		if (err != OK) {
			if (err != ERR_BUSY) {
				return FAILED;
			} else if (!blocking) {
				return ERR_BUSY;
			}
			// Keep trying to send full packet
			continue;
		}
		return OK;

	} while (sent != p_buffer_size);

	return OK;
}

int PacketPeerUDP::get_max_packet_size() const {
	return 512; // uhm maybe not
}

Error PacketPeerUDP::bind(int p_port, const IPAddress &p_bind_address, int p_recv_buffer_size) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(_sock->is_open(), ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(!p_bind_address.is_valid() && !p_bind_address.is_wildcard(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_port < 0 || p_port > 65535, ERR_INVALID_PARAMETER, "The local port number must be between 0 and 65535 (inclusive).");

	Error err;
	IP::Type ip_type = IP::TYPE_ANY;

	if (p_bind_address.is_valid()) {
		ip_type = p_bind_address.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
	}

	err = _sock->open(NetSocket::TYPE_UDP, ip_type);

	if (err != OK) {
		return ERR_CANT_CREATE;
	}

	_sock->set_blocking_enabled(false);
	_sock->set_broadcasting_enabled(broadcast);
	err = _sock->bind(p_bind_address, p_port);

	if (err != OK) {
		_sock->close();
		return err;
	}
	rb.resize(nearest_shift(p_recv_buffer_size));
	return OK;
}

Error PacketPeerUDP::connect_shared_socket(Ref<NetSocket> p_sock, IPAddress p_ip, uint16_t p_port, UDPServer *p_server) {
	udp_server = p_server;
	connected = true;
	_sock = p_sock;
	peer_addr = p_ip;
	peer_port = p_port;
	packet_ip = peer_addr;
	packet_port = peer_port;
	return OK;
}

void PacketPeerUDP::disconnect_shared_socket() {
	udp_server = nullptr;
	_sock = Ref<NetSocket>(NetSocket::create());
	close();
}

Error PacketPeerUDP::connect_to_host(const IPAddress &p_host, int p_port) {
	ERR_FAIL_COND_V(udp_server, ERR_LOCKED);
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!p_host.is_valid(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_port < 1 || p_port > 65535, ERR_INVALID_PARAMETER, "The remote port number must be between 1 and 65535 (inclusive).");

	Error err;

	if (!_sock->is_open()) {
		IP::Type ip_type = p_host.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
		err = _sock->open(NetSocket::TYPE_UDP, ip_type);
		ERR_FAIL_COND_V(err != OK, ERR_CANT_OPEN);
		_sock->set_blocking_enabled(false);
	}

	err = _sock->connect_to_host(p_host, p_port);

	// I see no reason why we should get ERR_BUSY (wouldblock/eagain) here.
	// This is UDP, so connect is only used to tell the OS to which socket
	// it should deliver packets when multiple are bound on the same address/port.
	if (err != OK) {
		close();
		ERR_FAIL_V_MSG(FAILED, "Unable to connect");
	}

	connected = true;

	peer_addr = p_host;
	peer_port = p_port;

	// Flush any packet we might still have in queue.
	rb.clear();
	return OK;
}

bool PacketPeerUDP::is_socket_connected() const {
	return connected;
}

void PacketPeerUDP::close() {
	if (udp_server) {
		udp_server->remove_peer(peer_addr, peer_port);
		udp_server = nullptr;
		_sock = Ref<NetSocket>(NetSocket::create());
	} else if (_sock.is_valid()) {
		_sock->close();
	}
	rb.resize(16);
	queue_count = 0;
	connected = false;
}

Error PacketPeerUDP::wait() {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	return _sock->poll(NetSocket::POLL_TYPE_IN, -1);
}

Error PacketPeerUDP::_poll() {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);

	if (!_sock->is_open()) {
		return FAILED;
	}
	if (udp_server) {
		return OK; // Handled by UDPServer.
	}

	Error err;
	int read;
	IPAddress ip;
	uint16_t port;

	while (true) {
		if (connected) {
			err = _sock->recv(recv_buffer, sizeof(recv_buffer), read);
			ip = peer_addr;
			port = peer_port;
		} else {
			err = _sock->recvfrom(recv_buffer, sizeof(recv_buffer), read, ip, port);
		}

		if (err != OK) {
			if (err == ERR_BUSY) {
				break;
			}
			return FAILED;
		}

		err = store_packet(ip, port, recv_buffer, read);
#ifdef TOOLS_ENABLED
		if (err != OK) {
			WARN_PRINT("Buffer full, dropping packets!");
		}
#endif
	}

	return OK;
}

Error PacketPeerUDP::store_packet(IPAddress p_ip, uint32_t p_port, uint8_t *p_buf, int p_buf_size) {
	if (rb.space_left() < p_buf_size + 24) {
		return ERR_OUT_OF_MEMORY;
	}
	rb.write(p_ip.get_ipv6(), 16);
	rb.write((uint8_t *)&p_port, 4);
	rb.write((uint8_t *)&p_buf_size, 4);
	rb.write(p_buf, p_buf_size);
	++queue_count;
	return OK;
}

bool PacketPeerUDP::is_bound() const {
	return _sock.is_valid() && _sock->is_open();
}

IPAddress PacketPeerUDP::get_packet_address() const {
	return packet_ip;
}

int PacketPeerUDP::get_packet_port() const {
	return packet_port;
}

int PacketPeerUDP::get_local_port() const {
	uint16_t local_port;
	_sock->get_socket_address(nullptr, &local_port);
	return local_port;
}

void PacketPeerUDP::set_dest_address(const IPAddress &p_address, int p_port) {
	ERR_FAIL_COND_MSG(connected, "Destination address cannot be set for connected sockets");
	peer_addr = p_address;
	peer_port = p_port;
}

void PacketPeerUDP::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bind", "port", "bind_address", "recv_buf_size"), &PacketPeerUDP::bind, DEFVAL("*"), DEFVAL(65536));
	ClassDB::bind_method(D_METHOD("close"), &PacketPeerUDP::close);
	ClassDB::bind_method(D_METHOD("wait"), &PacketPeerUDP::wait);
	ClassDB::bind_method(D_METHOD("is_bound"), &PacketPeerUDP::is_bound);
	ClassDB::bind_method(D_METHOD("connect_to_host", "host", "port"), &PacketPeerUDP::connect_to_host);
	ClassDB::bind_method(D_METHOD("is_socket_connected"), &PacketPeerUDP::is_socket_connected);
	ClassDB::bind_method(D_METHOD("get_packet_ip"), &PacketPeerUDP::_get_packet_ip);
	ClassDB::bind_method(D_METHOD("get_packet_port"), &PacketPeerUDP::get_packet_port);
	ClassDB::bind_method(D_METHOD("get_local_port"), &PacketPeerUDP::get_local_port);
	ClassDB::bind_method(D_METHOD("set_dest_address", "host", "port"), &PacketPeerUDP::_set_dest_address);
	ClassDB::bind_method(D_METHOD("set_broadcast_enabled", "enabled"), &PacketPeerUDP::set_broadcast_enabled);
	ClassDB::bind_method(D_METHOD("join_multicast_group", "multicast_address", "interface_name"), &PacketPeerUDP::join_multicast_group);
	ClassDB::bind_method(D_METHOD("leave_multicast_group", "multicast_address", "interface_name"), &PacketPeerUDP::leave_multicast_group);
}

PacketPeerUDP::PacketPeerUDP() :
		_sock(Ref<NetSocket>(NetSocket::create())) {
	rb.resize(16);
}

PacketPeerUDP::~PacketPeerUDP() {
	close();
}
