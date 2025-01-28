/**************************************************************************/
/*  udp_server.cpp                                                        */
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

#include "udp_server.h"

void UDPServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("listen", "port", "bind_address"), &UDPServer::listen, DEFVAL("*"));
	ClassDB::bind_method(D_METHOD("poll"), &UDPServer::poll);
	ClassDB::bind_method(D_METHOD("is_connection_available"), &UDPServer::is_connection_available);
	ClassDB::bind_method(D_METHOD("get_local_port"), &UDPServer::get_local_port);
	ClassDB::bind_method(D_METHOD("is_listening"), &UDPServer::is_listening);
	ClassDB::bind_method(D_METHOD("take_connection"), &UDPServer::take_connection);
	ClassDB::bind_method(D_METHOD("stop"), &UDPServer::stop);
	ClassDB::bind_method(D_METHOD("set_max_pending_connections", "max_pending_connections"), &UDPServer::set_max_pending_connections);
	ClassDB::bind_method(D_METHOD("get_max_pending_connections"), &UDPServer::get_max_pending_connections);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_pending_connections", PROPERTY_HINT_RANGE, "0,256,1"), "set_max_pending_connections", "get_max_pending_connections");
}

Error UDPServer::poll() {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	if (!_sock->is_open()) {
		return ERR_UNCONFIGURED;
	}
	Error err;
	int read;
	IPAddress ip;
	uint16_t port;
	while (true) {
		err = _sock->recvfrom(recv_buffer, sizeof(recv_buffer), read, ip, port);
		if (err != OK) {
			if (err == ERR_BUSY) {
				break;
			}
			return FAILED;
		}
		Peer p;
		p.ip = ip;
		p.port = port;
		List<Peer>::Element *E = peers.find(p);
		if (!E) {
			E = pending.find(p);
		}
		if (E) {
			E->get().peer->store_packet(ip, port, recv_buffer, read);
		} else {
			if (pending.size() >= max_pending_connections) {
				// Drop connection.
				continue;
			}
			// It's a new peer, add it to the pending list.
			Peer peer;
			peer.ip = ip;
			peer.port = port;
			peer.peer = memnew(PacketPeerUDP);
			peer.peer->connect_shared_socket(_sock, ip, port, this);
			peer.peer->store_packet(ip, port, recv_buffer, read);
			pending.push_back(peer);
		}
	}
	return OK;
}

Error UDPServer::listen(uint16_t p_port, const IPAddress &p_bind_address) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(_sock->is_open(), ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(!p_bind_address.is_valid() && !p_bind_address.is_wildcard(), ERR_INVALID_PARAMETER);

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
	_sock->set_reuse_address_enabled(true);
	err = _sock->bind(p_bind_address, p_port);

	if (err != OK) {
		stop();
		return err;
	}
	return OK;
}

int UDPServer::get_local_port() const {
	uint16_t local_port;
	_sock->get_socket_address(nullptr, &local_port);
	return local_port;
}

bool UDPServer::is_listening() const {
	ERR_FAIL_COND_V(_sock.is_null(), false);

	return _sock->is_open();
}

bool UDPServer::is_connection_available() const {
	ERR_FAIL_COND_V(_sock.is_null(), false);

	if (!_sock->is_open()) {
		return false;
	}

	return pending.size() > 0;
}

void UDPServer::set_max_pending_connections(int p_max) {
	ERR_FAIL_COND_MSG(p_max < 0, "Max pending connections value must be a positive number (0 means refuse new connections).");
	max_pending_connections = p_max;
	while (p_max > pending.size()) {
		List<Peer>::Element *E = pending.back();
		if (!E) {
			break;
		}
		memdelete(E->get().peer);
		pending.erase(E);
	}
}

int UDPServer::get_max_pending_connections() const {
	return max_pending_connections;
}

Ref<PacketPeerUDP> UDPServer::take_connection() {
	Ref<PacketPeerUDP> conn;
	if (!is_connection_available()) {
		return conn;
	}

	Peer peer = pending.front()->get();
	pending.pop_front();
	peers.push_back(peer);
	return peer.peer;
}

void UDPServer::remove_peer(IPAddress p_ip, int p_port) {
	Peer peer;
	peer.ip = p_ip;
	peer.port = p_port;
	List<Peer>::Element *E = peers.find(peer);
	if (E) {
		peers.erase(E);
	}
}

void UDPServer::stop() {
	if (_sock.is_valid()) {
		_sock->close();
	}
	List<Peer>::Element *E = peers.front();
	while (E) {
		E->get().peer->disconnect_shared_socket();
		E = E->next();
	}
	E = pending.front();
	while (E) {
		E->get().peer->disconnect_shared_socket();
		memdelete(E->get().peer);
		E = E->next();
	}
	peers.clear();
	pending.clear();
}

UDPServer::UDPServer() :
		_sock(Ref<NetSocket>(NetSocket::create())) {
}

UDPServer::~UDPServer() {
	stop();
}
