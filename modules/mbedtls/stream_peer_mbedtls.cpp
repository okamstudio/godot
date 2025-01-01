/**************************************************************************/
/*  stream_peer_mbedtls.cpp                                               */
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

#include "stream_peer_mbedtls.h"

#include "core/io/stream_peer_tcp.h"

int StreamPeerMbedTLS::bio_send(void *ctx, const unsigned char *buf, size_t len) {
	if (buf == nullptr || len == 0) {
		return 0;
	}

	StreamPeerMbedTLS *sp = static_cast<StreamPeerMbedTLS *>(ctx);

	ERR_FAIL_NULL_V(sp, 0);

	int sent;
	Error err = sp->base->put_partial_data((const uint8_t *)buf, len, sent);
	if (err != OK) {
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	}
	if (sent == 0) {
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}
	return sent;
}

int StreamPeerMbedTLS::bio_recv(void *ctx, unsigned char *buf, size_t len) {
	if (buf == nullptr || len == 0) {
		return 0;
	}

	StreamPeerMbedTLS *sp = static_cast<StreamPeerMbedTLS *>(ctx);

	ERR_FAIL_NULL_V(sp, 0);

	int got;
	Error err = sp->base->get_partial_data((uint8_t *)buf, len, got);
	if (err != OK) {
		return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
	}
	if (got == 0) {
		return MBEDTLS_ERR_SSL_WANT_READ;
	}
	return got;
}

void StreamPeerMbedTLS::_cleanup() {
	tls_ctx->clear();
	base = Ref<StreamPeer>();
	status = STATUS_DISCONNECTED;
}

Error StreamPeerMbedTLS::_do_handshake() {
	int ret = mbedtls_ssl_handshake(tls_ctx->get_context());
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		// Handshake is still in progress, will retry via poll later.
		return OK;
	} else if (ret != 0) {
		// An error occurred.
		ERR_PRINT("TLS handshake error: " + itos(ret));
		TLSContextMbedTLS::print_mbedtls_error(ret);
		disconnect_from_stream();
		status = STATUS_ERROR;
		return FAILED;
	}

	status = STATUS_CONNECTED;
	return OK;
}

Error StreamPeerMbedTLS::connect_to_stream(Ref<StreamPeer> p_base, const String &p_common_name, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(p_base.is_null(), ERR_INVALID_PARAMETER);

	Error err = tls_ctx->init_client(MBEDTLS_SSL_TRANSPORT_STREAM, p_common_name, p_options.is_valid() ? p_options : TLSOptions::client());
	ERR_FAIL_COND_V(err != OK, err);

	base = p_base;
	mbedtls_ssl_set_bio(tls_ctx->get_context(), this, bio_send, bio_recv, nullptr);

	status = STATUS_HANDSHAKING;

	if (_do_handshake() != OK) {
		status = STATUS_ERROR_HOSTNAME_MISMATCH;
		return FAILED;
	}

	return OK;
}

Error StreamPeerMbedTLS::accept_stream(Ref<StreamPeer> p_base, Ref<TLSOptions> p_options) {
	ERR_FAIL_COND_V(p_base.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_options.is_null() || !p_options->is_server(), ERR_INVALID_PARAMETER);

	Error err = tls_ctx->init_server(MBEDTLS_SSL_TRANSPORT_STREAM, p_options);
	ERR_FAIL_COND_V(err != OK, err);

	base = p_base;

	mbedtls_ssl_set_bio(tls_ctx->get_context(), this, bio_send, bio_recv, nullptr);

	status = STATUS_HANDSHAKING;

	if (_do_handshake() != OK) {
		return FAILED;
	}

	status = STATUS_CONNECTED;
	return OK;
}

Error StreamPeerMbedTLS::put_data(const uint8_t *p_data, int p_bytes) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	Error err;
	int sent = 0;

	while (p_bytes > 0) {
		err = put_partial_data(p_data, p_bytes, sent);

		if (err != OK) {
			return err;
		}

		p_data += sent;
		p_bytes -= sent;
	}

	return OK;
}

Error StreamPeerMbedTLS::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	r_sent = 0;

	if (p_bytes == 0) {
		return OK;
	}

	do {
		int ret = mbedtls_ssl_write(tls_ctx->get_context(), &p_data[r_sent], p_bytes - r_sent);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			// Non blocking IO.
			break;
		} else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			// Clean close
			disconnect_from_stream();
			return ERR_FILE_EOF;
		} else if (ret <= 0) {
			TLSContextMbedTLS::print_mbedtls_error(ret);
			disconnect_from_stream();
			return ERR_CONNECTION_ERROR;
		}
		r_sent += ret;

	} while (r_sent < p_bytes);

	return OK;
}

Error StreamPeerMbedTLS::get_data(uint8_t *p_buffer, int p_bytes) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	Error err;

	int got = 0;
	while (p_bytes > 0) {
		err = get_partial_data(p_buffer, p_bytes, got);

		if (err != OK) {
			return err;
		}

		p_buffer += got;
		p_bytes -= got;
	}

	return OK;
}

Error StreamPeerMbedTLS::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_UNCONFIGURED);

	r_received = 0;

	do {
		int ret = mbedtls_ssl_read(tls_ctx->get_context(), &p_buffer[r_received], p_bytes - r_received);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			// Non blocking IO.
			break;
		} else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			// Clean close
			disconnect_from_stream();
			return ERR_FILE_EOF;
		} else if (ret <= 0) {
			TLSContextMbedTLS::print_mbedtls_error(ret);
			disconnect_from_stream();
			return ERR_CONNECTION_ERROR;
		}

		r_received += ret;

	} while (r_received < p_bytes);

	return OK;
}

void StreamPeerMbedTLS::poll() {
	ERR_FAIL_COND(status != STATUS_CONNECTED && status != STATUS_HANDSHAKING);
	ERR_FAIL_COND(base.is_null());

	if (status == STATUS_HANDSHAKING) {
		_do_handshake();
		return;
	}

	// We could pass nullptr as second parameter, but some behavior sanitizers don't seem to like that.
	// Passing a 1 byte buffer to workaround it.
	uint8_t byte;
	int ret = mbedtls_ssl_read(tls_ctx->get_context(), &byte, 0);

	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		// Nothing to read/write (non blocking IO)
	} else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		// Clean close (disconnect)
		disconnect_from_stream();
		return;
	} else if (ret < 0) {
		TLSContextMbedTLS::print_mbedtls_error(ret);
		disconnect_from_stream();
		return;
	}

	Ref<StreamPeerTCP> tcp = base;
	if (tcp.is_valid() && tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		disconnect_from_stream();
		return;
	}
}

int StreamPeerMbedTLS::get_available_bytes() const {
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, 0);

	return mbedtls_ssl_get_bytes_avail(&(tls_ctx->tls));
}

StreamPeerMbedTLS::StreamPeerMbedTLS() {
	tls_ctx.instantiate();
}

StreamPeerMbedTLS::~StreamPeerMbedTLS() {
	disconnect_from_stream();
}

void StreamPeerMbedTLS::disconnect_from_stream() {
	if (status != STATUS_CONNECTED && status != STATUS_HANDSHAKING) {
		return;
	}

	Ref<StreamPeerTCP> tcp = base;
	if (tcp.is_valid() && tcp->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		// We are still connected on the socket, try to send close notify.
		mbedtls_ssl_close_notify(tls_ctx->get_context());
	}

	_cleanup();
}

StreamPeerMbedTLS::Status StreamPeerMbedTLS::get_status() const {
	return status;
}

Ref<StreamPeer> StreamPeerMbedTLS::get_stream() const {
	return base;
}

StreamPeerTLS *StreamPeerMbedTLS::_create_func(bool p_notify_postinitialize) {
	return static_cast<StreamPeerTLS *>(ClassDB::creator<StreamPeerMbedTLS>(p_notify_postinitialize));
}

void StreamPeerMbedTLS::initialize_tls() {
	_create = _create_func;
}

void StreamPeerMbedTLS::finalize_tls() {
	_create = nullptr;
}
