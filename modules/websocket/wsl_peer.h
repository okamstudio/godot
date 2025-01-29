/**************************************************************************/
/*  wsl_peer.h                                                            */
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

#ifndef WSL_PEER_H
#define WSL_PEER_H

#ifndef WEB_ENABLED

#include "packet_buffer.h"
#include "websocket_peer.h"

#include "core/crypto/crypto_core.h"
#include "core/error/error_list.h"
#include "core/io/stream_peer_tcp.h"

#include <wslay/wslay.h>

#define WSL_MAX_HEADER_SIZE 4096

class WSLPeer : public WebSocketPeer {
private:
	static CryptoCore::RandomGenerator *_static_rng;
	static WebSocketPeer *_create(bool p_notify_postinitialize) { return static_cast<WebSocketPeer *>(ClassDB::creator<WSLPeer>(p_notify_postinitialize)); }

	// Callbacks.
	static ssize_t _wsl_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data);
	static void _wsl_recv_start_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_start_arg *arg, void *user_data);
	static void _wsl_frame_recv_chunk_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_frame_recv_chunk_arg *arg, void *user_data);

	static ssize_t _wsl_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data);
	static int _wsl_genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data);
	static void _wsl_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data);

	static wslay_event_callbacks _wsl_callbacks;

	// Helpers
	static String _compute_key_response(String p_key);
	static String _generate_key();

	// Client IP resolver.
	class Resolver {
		Array ip_candidates;
		IP::ResolverID resolver_id = IP::RESOLVER_INVALID_ID;
		int port = 0;

	public:
		bool has_more_candidates() {
			return ip_candidates.size() > 0 || resolver_id != IP::RESOLVER_INVALID_ID;
		}

		void try_next_candidate(Ref<StreamPeerTCP> &p_tcp);
		void start(const String &p_host, int p_port);
		void stop();
		Resolver() {}
	};

	struct PendingMessage {
		size_t payload_size = 0;
		uint8_t opcode = 0;

		void clear() {
			payload_size = 0;
			opcode = 0;
		}
	};

	Resolver resolver;

	// WebSocket connection state.
	WebSocketPeer::State ready_state = WebSocketPeer::STATE_CLOSED;
	bool is_server = false;
	Ref<StreamPeerTCP> tcp;
	Ref<StreamPeer> connection;
	wslay_event_context_ptr wsl_ctx = nullptr;

	String requested_url;
	String requested_host;
	bool pending_request = true;
	Ref<StreamPeerBuffer> handshake_buffer;
	String selected_protocol;
	String session_key;

	int close_code = -1;
	String close_reason;
	uint8_t was_string = 0;
	uint64_t last_heartbeat = 0;
	bool heartbeat_waiting = false;
	PendingMessage pending_message;

	// WebSocket configuration.
	bool use_tls = true;
	Ref<TLSOptions> tls_options;

	// Packet buffers.
	Vector<uint8_t> packet_buffer;
	// Our packet info is just a boolean (is_string), using uint8_t for it.
	PacketBuffer<uint8_t> in_buffer;

	Error _send(const uint8_t *p_buffer, int p_buffer_size, wslay_opcode p_opcode);

	Error _do_server_handshake();
	bool _parse_client_request();

	void _do_client_handshake();
	bool _verify_server_response();

	void _clear();

public:
	static void initialize();
	static void deinitialize();

	// PacketPeer
	virtual int get_available_packet_count() const override;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	virtual int get_max_packet_size() const override { return packet_buffer.size(); }

	// WebSocketPeer
	virtual Error send(const uint8_t *p_buffer, int p_buffer_size, WriteMode p_mode) override;
	virtual Error connect_to_url(const String &p_url, Ref<TLSOptions> p_options = Ref<TLSOptions>()) override;
	virtual Error accept_stream(Ref<StreamPeer> p_stream) override;
	virtual void close(int p_code = 1000, String p_reason = "") override;
	virtual void poll() override;

	virtual State get_ready_state() const override { return ready_state; }
	virtual int get_close_code() const override { return close_code; }
	virtual String get_close_reason() const override { return close_reason; }
	virtual int get_current_outbound_buffered_amount() const override;

	virtual IPAddress get_connected_host() const override;
	virtual uint16_t get_connected_port() const override;
	virtual String get_selected_protocol() const override;
	virtual String get_requested_url() const override;

	virtual bool was_string_packet() const override { return was_string; }
	virtual void set_no_delay(bool p_enabled) override;

	WSLPeer();
	~WSLPeer();
};

#endif // WEB_ENABLED

#endif // WSL_PEER_H
