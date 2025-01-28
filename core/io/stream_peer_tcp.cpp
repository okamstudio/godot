/**************************************************************************/
/*  stream_peer_tcp.cpp                                                   */
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

#include "stream_peer_tcp.h"

#include "core/config/project_settings.h"

Error StreamPeerTCP::poll() {
	if (status == STATUS_CONNECTED) {
		Error err;
		err = _sock->poll(NetSocket::POLL_TYPE_IN, 0);
		if (err == OK) {
			// FIN received
			if (_sock->get_available_bytes() == 0) {
				disconnect_from_host();
				return OK;
			}
		}
		// Also poll write
		err = _sock->poll(NetSocket::POLL_TYPE_IN_OUT, 0);
		if (err != OK && err != ERR_BUSY) {
			// Got an error
			disconnect_from_host();
			status = STATUS_ERROR;
			return err;
		}
		return OK;
	} else if (status != STATUS_CONNECTING) {
		return OK;
	}

	Error err = _sock->connect_to_host(peer_host, peer_port);

	if (err == OK) {
		status = STATUS_CONNECTED;
		return OK;
	} else if (err == ERR_BUSY) {
		// Check for connect timeout
		if (OS::get_singleton()->get_ticks_msec() > timeout) {
			disconnect_from_host();
			status = STATUS_ERROR;
			return ERR_CONNECTION_ERROR;
		}
		// Still trying to connect
		return OK;
	}

	disconnect_from_host();
	status = STATUS_ERROR;
	return ERR_CONNECTION_ERROR;
}

void StreamPeerTCP::accept_socket(Ref<NetSocket> p_sock, IPAddress p_host, uint16_t p_port) {
	_sock = p_sock;
	_sock->set_blocking_enabled(false);

	timeout = OS::get_singleton()->get_ticks_msec() + (((uint64_t)GLOBAL_GET("network/limits/tcp/connect_timeout_seconds")) * 1000);
	status = STATUS_CONNECTED;

	peer_host = p_host;
	peer_port = p_port;
}

Error StreamPeerTCP::bind(int p_port, const IPAddress &p_host) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(_sock->is_open(), ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V_MSG(p_port < 0 || p_port > 65535, ERR_INVALID_PARAMETER, "The local port number must be between 0 and 65535 (inclusive).");

	IP::Type ip_type = p_host.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
	if (p_host.is_wildcard()) {
		ip_type = IP::TYPE_ANY;
	}
	Error err = _sock->open(NetSocket::TYPE_TCP, ip_type);
	if (err != OK) {
		return err;
	}
	_sock->set_blocking_enabled(false);
	return _sock->bind(p_host, p_port);
}

Error StreamPeerTCP::connect_to_host(const IPAddress &p_host, int p_port) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(status != STATUS_NONE, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(!p_host.is_valid(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_port < 1 || p_port > 65535, ERR_INVALID_PARAMETER, "The remote port number must be between 1 and 65535 (inclusive).");

	if (!_sock->is_open()) {
		IP::Type ip_type = p_host.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
		Error err = _sock->open(NetSocket::TYPE_TCP, ip_type);
		if (err != OK) {
			return err;
		}
		_sock->set_blocking_enabled(false);
	}

	timeout = OS::get_singleton()->get_ticks_msec() + (((uint64_t)GLOBAL_GET("network/limits/tcp/connect_timeout_seconds")) * 1000);
	Error err = _sock->connect_to_host(p_host, p_port);

	if (err == OK) {
		status = STATUS_CONNECTED;
	} else if (err == ERR_BUSY) {
		status = STATUS_CONNECTING;
	} else {
		ERR_PRINT("Connection to remote host failed!");
		disconnect_from_host();
		return FAILED;
	}

	peer_host = p_host;
	peer_port = p_port;

	return OK;
}

Error StreamPeerTCP::write(const uint8_t *p_data, int p_bytes, int &r_sent, bool p_block) {
	ERR_FAIL_COND_V(_sock.is_null(), ERR_UNAVAILABLE);

	if (status != STATUS_CONNECTED) {
		return FAILED;
	}

	Error err;
	int data_to_send = p_bytes;
	const uint8_t *offset = p_data;
	int total_sent = 0;

	while (data_to_send) {
		int sent_amount = 0;
		err = _sock->send(offset, data_to_send, sent_amount);

		if (err != OK) {
			if (err != ERR_BUSY) {
				disconnect_from_host();
				return FAILED;
			}

			if (!p_block) {
				r_sent = total_sent;
				return OK;
			}

			// Block and wait for the socket to accept more data
			err = _sock->poll(NetSocket::POLL_TYPE_OUT, -1);
			if (err != OK) {
				disconnect_from_host();
				return FAILED;
			}
		} else {
			data_to_send -= sent_amount;
			offset += sent_amount;
			total_sent += sent_amount;
		}
	}

	r_sent = total_sent;

	return OK;
}

Error StreamPeerTCP::read(uint8_t *p_buffer, int p_bytes, int &r_received, bool p_block) {
	if (status != STATUS_CONNECTED) {
		return FAILED;
	}

	Error err;
	int to_read = p_bytes;
	int total_read = 0;
	r_received = 0;

	while (to_read) {
		int read = 0;
		err = _sock->recv(p_buffer + total_read, to_read, read);

		if (err != OK) {
			if (err != ERR_BUSY) {
				disconnect_from_host();
				return FAILED;
			}

			if (!p_block) {
				r_received = total_read;
				return OK;
			}

			err = _sock->poll(NetSocket::POLL_TYPE_IN, -1);

			if (err != OK) {
				disconnect_from_host();
				return FAILED;
			}

		} else if (read == 0) {
			disconnect_from_host();
			r_received = total_read;
			return ERR_FILE_EOF;

		} else {
			to_read -= read;
			total_read += read;

			if (!p_block) {
				r_received = total_read;
				return OK;
			}
		}
	}

	r_received = total_read;

	return OK;
}

void StreamPeerTCP::set_no_delay(bool p_enabled) {
	ERR_FAIL_COND(_sock.is_null() || !_sock->is_open());
	_sock->set_tcp_no_delay_enabled(p_enabled);
}

StreamPeerTCP::Status StreamPeerTCP::get_status() const {
	return status;
}

void StreamPeerTCP::disconnect_from_host() {
	if (_sock.is_valid() && _sock->is_open()) {
		_sock->close();
	}

	timeout = 0;
	status = STATUS_NONE;
	peer_host = IPAddress();
	peer_port = 0;
}

Error StreamPeerTCP::wait(NetSocket::PollType p_type, int p_timeout) {
	ERR_FAIL_COND_V(_sock.is_null() || !_sock->is_open(), ERR_UNAVAILABLE);
	return _sock->poll(p_type, p_timeout);
}

Error StreamPeerTCP::put_data(const uint8_t *p_data, int p_bytes) {
	int total;
	return write(p_data, p_bytes, total, true);
}

Error StreamPeerTCP::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	return write(p_data, p_bytes, r_sent, false);
}

Error StreamPeerTCP::get_data(uint8_t *p_buffer, int p_bytes) {
	int total;
	return read(p_buffer, p_bytes, total, true);
}

Error StreamPeerTCP::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	return read(p_buffer, p_bytes, r_received, false);
}

int StreamPeerTCP::get_available_bytes() const {
	ERR_FAIL_COND_V(_sock.is_null(), -1);
	return _sock->get_available_bytes();
}

IPAddress StreamPeerTCP::get_connected_host() const {
	return peer_host;
}

int StreamPeerTCP::get_connected_port() const {
	return peer_port;
}

int StreamPeerTCP::get_local_port() const {
	uint16_t local_port;
	_sock->get_socket_address(nullptr, &local_port);
	return local_port;
}

Error StreamPeerTCP::_connect(const String &p_address, int p_port) {
	IPAddress ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_address);
		if (!ip.is_valid()) {
			return ERR_CANT_RESOLVE;
		}
	}

	return connect_to_host(ip, p_port);
}

void StreamPeerTCP::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bind", "port", "host"), &StreamPeerTCP::bind, DEFVAL("*"));
	ClassDB::bind_method(D_METHOD("connect_to_host", "host", "port"), &StreamPeerTCP::_connect);
	ClassDB::bind_method(D_METHOD("poll"), &StreamPeerTCP::poll);
	ClassDB::bind_method(D_METHOD("get_status"), &StreamPeerTCP::get_status);
	ClassDB::bind_method(D_METHOD("get_connected_host"), &StreamPeerTCP::get_connected_host);
	ClassDB::bind_method(D_METHOD("get_connected_port"), &StreamPeerTCP::get_connected_port);
	ClassDB::bind_method(D_METHOD("get_local_port"), &StreamPeerTCP::get_local_port);
	ClassDB::bind_method(D_METHOD("disconnect_from_host"), &StreamPeerTCP::disconnect_from_host);
	ClassDB::bind_method(D_METHOD("set_no_delay", "enabled"), &StreamPeerTCP::set_no_delay);

	BIND_ENUM_CONSTANT(STATUS_NONE);
	BIND_ENUM_CONSTANT(STATUS_CONNECTING);
	BIND_ENUM_CONSTANT(STATUS_CONNECTED);
	BIND_ENUM_CONSTANT(STATUS_ERROR);
}

StreamPeerTCP::StreamPeerTCP() :
		_sock(Ref<NetSocket>(NetSocket::create())) {
}

StreamPeerTCP::~StreamPeerTCP() {
	disconnect_from_host();
}
