/**************************************************************************/
/*  websocket_peer.h                                                      */
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

#include "core/crypto/crypto.h"
#include "core/error/error_list.h"
#include "core/io/packet_peer.h"

class WebSocketPeer : public PacketPeer {
	GDCLASS(WebSocketPeer, PacketPeer);

public:
	enum State {
		STATE_CONNECTING,
		STATE_OPEN,
		STATE_CLOSING,
		STATE_CLOSED
	};

	enum WriteMode {
		WRITE_MODE_TEXT,
		WRITE_MODE_BINARY,
	};

	enum {
		DEFAULT_BUFFER_SIZE = 65535,
	};

private:
	virtual Error _send_bind(const PackedByteArray &p_data, WriteMode p_mode = WRITE_MODE_BINARY);

protected:
	static WebSocketPeer *(*_create)(bool p_notify_postinitialize);

	static void _bind_methods();

	Vector<String> supported_protocols;
	Vector<String> handshake_headers;

	Vector<String> _get_supported_protocols() const;
	Vector<String> _get_handshake_headers() const;

	int outbound_buffer_size = DEFAULT_BUFFER_SIZE;
	int inbound_buffer_size = DEFAULT_BUFFER_SIZE;
	int max_queued_packets = 4096;
	uint64_t heartbeat_interval_msec = 0;

public:
	static WebSocketPeer *create(bool p_notify_postinitialize = true) {
		if (!_create) {
			return nullptr;
		}
		return _create(p_notify_postinitialize);
	}

	virtual Error connect_to_url(const String &p_url, Ref<TLSOptions> p_options = Ref<TLSOptions>()) = 0;
	virtual Error accept_stream(Ref<StreamPeer> p_stream) = 0;

	virtual Error send(const uint8_t *p_buffer, int p_buffer_size, WriteMode p_mode) = 0;
	virtual void close(int p_code = 1000, String p_reason = "") = 0;

	virtual IPAddress get_connected_host() const = 0;
	virtual uint16_t get_connected_port() const = 0;
	virtual bool was_string_packet() const = 0;
	virtual void set_no_delay(bool p_enabled) = 0;
	virtual int get_current_outbound_buffered_amount() const = 0;
	virtual String get_selected_protocol() const = 0;
	virtual String get_requested_url() const = 0;

	virtual void poll() = 0;
	virtual State get_ready_state() const = 0;
	virtual int get_close_code() const = 0;
	virtual String get_close_reason() const = 0;

	Error send_text(const String &p_text);

	void set_supported_protocols(const Vector<String> &p_protocols);
	const Vector<String> get_supported_protocols() const;

	void set_handshake_headers(const Vector<String> &p_headers);
	const Vector<String> get_handshake_headers() const;

	void set_outbound_buffer_size(int p_buffer_size);
	int get_outbound_buffer_size() const;

	void set_inbound_buffer_size(int p_buffer_size);
	int get_inbound_buffer_size() const;

	void set_max_queued_packets(int p_max_queued_packets);
	int get_max_queued_packets() const;

	double get_heartbeat_interval() const;
	void set_heartbeat_interval(double p_interval);

	WebSocketPeer();
	~WebSocketPeer();
};

VARIANT_ENUM_CAST(WebSocketPeer::WriteMode);
VARIANT_ENUM_CAST(WebSocketPeer::State);
