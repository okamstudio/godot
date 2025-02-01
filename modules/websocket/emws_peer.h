/**************************************************************************/
/*  emws_peer.h                                                           */
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

#ifdef WEB_ENABLED

#include "packet_buffer.h"
#include "websocket_peer.h"

#include "core/error/error_list.h"
#include "core/io/packet_peer.h"
#include "core/templates/ring_buffer.h"

#include <emscripten.h>

extern "C" {
typedef void (*WSOnOpen)(void *p_ref, char *p_protocol);
typedef void (*WSOnMessage)(void *p_ref, const uint8_t *p_buf, int p_buf_len, int p_is_string);
typedef void (*WSOnClose)(void *p_ref, int p_code, const char *p_reason, int p_is_clean);
typedef void (*WSOnError)(void *p_ref);

extern int godot_js_websocket_create(void *p_ref, const char *p_url, const char *p_proto, WSOnOpen p_on_open, WSOnMessage p_on_message, WSOnError p_on_error, WSOnClose p_on_close);
extern int godot_js_websocket_send(int p_id, const uint8_t *p_buf, int p_buf_len, int p_raw);
extern int godot_js_websocket_buffered_amount(int p_id);
extern void godot_js_websocket_close(int p_id, int p_code, const char *p_reason);
extern void godot_js_websocket_destroy(int p_id);
}

class EMWSPeer : public WebSocketPeer {
private:
	int peer_sock = -1;

	State ready_state = STATE_CLOSED;
	Vector<uint8_t> packet_buffer;
	PacketBuffer<uint8_t> in_buffer;
	uint8_t was_string = 0;
	int close_code = -1;
	String close_reason;
	String selected_protocol;
	String requested_url;

	static WebSocketPeer *_create(bool p_notify_postinitialize) { return static_cast<WebSocketPeer *>(ClassDB::creator<EMWSPeer>(p_notify_postinitialize)); }
	static void _esws_on_connect(void *obj, char *proto);
	static void _esws_on_message(void *obj, const uint8_t *p_data, int p_data_size, int p_is_string);
	static void _esws_on_error(void *obj);
	static void _esws_on_close(void *obj, int code, const char *reason, int was_clean);

	void _clear();
	Error _send(const uint8_t *p_buffer, int p_buffer_size, bool p_binary);

public:
	static void initialize() { WebSocketPeer::_create = EMWSPeer::_create; }

	// PacketPeer
	virtual int get_available_packet_count() const override;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	virtual int get_max_packet_size() const override { return packet_buffer.size(); }

	// WebSocketPeer
	virtual Error send(const uint8_t *p_buffer, int p_buffer_size, WriteMode p_mode) override;
	virtual Error connect_to_url(const String &p_url, Ref<TLSOptions> p_tls_client_options) override;
	virtual Error accept_stream(Ref<StreamPeer> p_stream) override;
	virtual void close(int p_code = 1000, String p_reason = "") override;
	virtual void poll() override;

	virtual State get_ready_state() const override;
	virtual int get_close_code() const override;
	virtual String get_close_reason() const override;
	virtual int get_current_outbound_buffered_amount() const override;

	virtual IPAddress get_connected_host() const override;
	virtual uint16_t get_connected_port() const override;
	virtual String get_selected_protocol() const override;
	virtual String get_requested_url() const override;

	virtual bool was_string_packet() const override;
	virtual void set_no_delay(bool p_enabled) override;

	EMWSPeer();
	~EMWSPeer();
};

#endif // WEB_ENABLED
