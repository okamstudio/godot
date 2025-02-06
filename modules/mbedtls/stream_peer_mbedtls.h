/**************************************************************************/
/*  stream_peer_mbedtls.h                                                 */
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

#include "tls_context_mbedtls.h"

#include "core/io/stream_peer_tls.h"

class StreamPeerMbedTLS : public StreamPeerTLS {
private:
	Status status = STATUS_DISCONNECTED;
	String hostname;

	Ref<StreamPeer> base;

	static StreamPeerTLS *_create_func(bool p_notify_postinitialize);

	static int bio_recv(void *ctx, unsigned char *buf, size_t len);
	static int bio_send(void *ctx, const unsigned char *buf, size_t len);
	void _cleanup();

protected:
	Ref<TLSContextMbedTLS> tls_ctx;

	Error _do_handshake();

public:
	virtual void poll();
	virtual Error accept_stream(Ref<StreamPeer> p_base, Ref<TLSOptions> p_options);
	virtual Error connect_to_stream(Ref<StreamPeer> p_base, const String &p_common_name, Ref<TLSOptions> p_options);
	virtual Status get_status() const;
	virtual Ref<StreamPeer> get_stream() const;

	virtual void disconnect_from_stream();

	virtual Error put_data(const uint8_t *p_data, int p_bytes);
	virtual Error put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent);

	virtual Error get_data(uint8_t *p_buffer, int p_bytes);
	virtual Error get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received);

	virtual int get_available_bytes() const;

	static void initialize_tls();
	static void finalize_tls();

	StreamPeerMbedTLS();
	~StreamPeerMbedTLS();
};
