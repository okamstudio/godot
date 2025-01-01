/**************************************************************************/
/*  packet_peer_mbed_dtls.cpp                                             */
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

#include "packet_peer_mbed_dtls.h"

int PacketPeerMbedDTLS::bio_send(void *ctx, const unsigned char *buf, size_t len) {
	if (buf == nullptr || len == 0) {
		return 0;
	}

	PacketPeerMbedDTLS *sp = static_cast<PacketPeerMbedDTLS *>(ctx);

	ERR_FAIL_NULL_V(sp, 0);

	Error err = sp->base->put_packet((const uint8_t *)buf, len);
	if (err == ERR_BUSY) {
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	} else if (err != OK) {
		ERR_FAIL_V(MBEDTLS_ERR_SSL_INTERNAL_ERROR);
	}
	return len;
}

int PacketPeerMbedDTLS::bio_recv(void *ctx, unsigned char *buf, size_t len) {
	if (buf == nullptr || len == 0) {
		return 0;
	}

	PacketPeerMbedDTLS *sp = static_cast<PacketPeerMbedDTLS *>(ctx);

	ERR_FAIL_NULL_V(sp, 0);

	int pc = sp->base->get_available_packet_count();
	if (pc == 0) {
		return MBEDTLS_ERR_SSL_WANT_READ;
	} else if (pc < 0) {
		ERR_FAIL_V(MBEDTLS_ERR_SSL_INTERNAL_ERROR);
	}

	const uint8_t *buffer;
	int buffer_size = 0;
	Error err = sp->base->get_packet(&buffer, buffer_size);
	if (err != OK) {
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	}
	memcpy(buf, buffer, buffer_size);
	return buffer_size;
}

void PacketPeerMbedDTLS::_cleanup() {
	tls_ctx->clear();
	base = Ref<PacketPeer>();
	status = STATUS_DISCONNECTED;
}

int PacketPeerMbedDTLS::_set_cookie() {
	// Setup DTLS session cookie for this client
	uint8_t client_id[18];
	IPAddress addr = base->get_packet_address();
	uint16_t port = base->get_packet_port();
	memcpy(client_id, addr.get_ipv6(), 16);
	memcpy(&client_id[16], (uint8_t *)&port, 2);
	return mbedtls_ssl_set_client_transport_id(tls_ctx->get_context(), client_id, 18);
}

Error PacketPeerMbedDTLS::_do_handshake() {
	int ret = 0;
	while ((ret = mbedtls_ssl_handshake(tls_ctx->get_context())) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			if (ret != MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
				ERR_PRINT("TLS handshake error: " + itos(ret));
				TLSContextMbedTLS::print_mbedtls_error(ret);
			}
			_cleanup();
			status = STATUS_ERROR;
			return FAILED;
		}
		// Will retry via poll later
		return OK;
	}

	status = STATUS_CONNECTED;
	return OK;
}

Error PacketPeerMbedDTLS::connect_to_peer(Ref<PacketPeerUDP> p_base, const String &p_hostname, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(p_base.is_null() || !p_base->is_socket_connected(), ERR_INVALID_PARAMETER);

	Error err = tls_ctx->init_client(MBEDTLS_SSL_TRANSPORT_DATAGRAM, p_hostname, p_options.is_valid() ? p_options : TLSOptions::client());
	ERR_FAIL_COND_V(err != OK, err);

	base = p_base;

	mbedtls_ssl_set_bio(tls_ctx->get_context(), this, bio_send, bio_recv, nullptr);
	mbedtls_ssl_set_timer_cb(tls_ctx->get_context(), &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

	status = STATUS_HANDSHAKING;

	if (_do_handshake() != OK) {
		status = STATUS_ERROR_HOSTNAME_MISMATCH;
		return FAILED;
	}

	return OK;
}

Error PacketPeerMbedDTLS::accept_peer(Ref<PacketPeerUDP> p_base, Ref<TLSOptions> p_options, Ref<CookieContextMbedTLS> p_cookies) {
	ERR_FAIL_COND_V(p_base.is_null() || !p_base->is_socket_connected(), ERR_INVALID_PARAMETER);

	Error err = tls_ctx->init_server(MBEDTLS_SSL_TRANSPORT_DATAGRAM, p_options, p_cookies);
	ERR_FAIL_COND_V(err != OK, err);

	base = p_base;
	base->set_blocking_mode(false);

	mbedtls_ssl_session_reset(tls_ctx->get_context());

	int ret = _set_cookie();
	if (ret != 0) {
		_cleanup();
		ERR_FAIL_V_MSG(FAILED, "Error setting DTLS client cookie");
	}

	mbedtls_ssl_set_bio(tls_ctx->get_context(), this, bio_send, bio_recv, nullptr);
	mbedtls_ssl_set_timer_cb(tls_ctx->get_context(), &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

	status = STATUS_HANDSHAKING;

	if (_do_handshake() != OK) {
		status = STATUS_ERROR;
		return FAILED;
	}

	return OK;
}

Error PacketPeerMbedDTLS::put_packet(const uint8_t *p_buffer, int p_bytes) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	if (p_bytes == 0) {
		return OK;
	}

	int ret = mbedtls_ssl_write(tls_ctx->get_context(), p_buffer, p_bytes);
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		// Non blocking io.
	} else if (ret <= 0) {
		TLSContextMbedTLS::print_mbedtls_error(ret);
		_cleanup();
		return ERR_CONNECTION_ERROR;
	}

	return OK;
}

Error PacketPeerMbedDTLS::get_packet(const uint8_t **r_buffer, int &r_bytes) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	r_bytes = 0;

	int ret = mbedtls_ssl_read(tls_ctx->get_context(), packet_buffer, PACKET_BUFFER_SIZE);
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		ret = 0; // non blocking io
	} else if (ret <= 0) {
		if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			// Also send close notify back
			disconnect_from_peer();
		} else {
			_cleanup();
			status = STATUS_ERROR;
			TLSContextMbedTLS::print_mbedtls_error(ret);
		}
		return ERR_CONNECTION_ERROR;
	}
	*r_buffer = packet_buffer;
	r_bytes = ret;

	return OK;
}

void PacketPeerMbedDTLS::poll() {
	if (status == STATUS_HANDSHAKING) {
		_do_handshake();
		return;
	} else if (status != STATUS_CONNECTED) {
		return;
	}

	ERR_FAIL_COND(base.is_null());

	int ret = mbedtls_ssl_read(tls_ctx->get_context(), nullptr, 0);

	if (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
		if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			// Also send close notify back
			disconnect_from_peer();
		} else {
			_cleanup();
			status = STATUS_ERROR;
			TLSContextMbedTLS::print_mbedtls_error(ret);
		}
	}
}

int PacketPeerMbedDTLS::get_available_packet_count() const {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, 0);

	return mbedtls_ssl_get_bytes_avail(&(tls_ctx->tls)) > 0 ? 1 : 0;
}

int PacketPeerMbedDTLS::get_max_packet_size() const {
	return 488; // 512 (UDP in Godot) - 24 (DTLS header)
}

PacketPeerMbedDTLS::PacketPeerMbedDTLS() {
	tls_ctx.instantiate();
}

PacketPeerMbedDTLS::~PacketPeerMbedDTLS() {
	disconnect_from_peer();
}

void PacketPeerMbedDTLS::disconnect_from_peer() {
	if (status != STATUS_CONNECTED && status != STATUS_HANDSHAKING) {
		return;
	}

	if (status == STATUS_CONNECTED) {
		int ret = 0;
		// Send SSL close notification, blocking, but ignore other errors.
		do {
			ret = mbedtls_ssl_close_notify(tls_ctx->get_context());
		} while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);
	}

	_cleanup();
}

PacketPeerMbedDTLS::Status PacketPeerMbedDTLS::get_status() const {
	return status;
}

PacketPeerDTLS *PacketPeerMbedDTLS::_create_func(bool p_notify_postinitialize) {
	return static_cast<PacketPeerDTLS *>(ClassDB::creator<PacketPeerMbedDTLS>(p_notify_postinitialize));
}

void PacketPeerMbedDTLS::initialize_dtls() {
	_create = _create_func;
	available = true;
}

void PacketPeerMbedDTLS::finalize_dtls() {
	_create = nullptr;
	available = false;
}
