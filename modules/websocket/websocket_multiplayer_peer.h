/**************************************************************************/
/*  websocket_multiplayer_peer.h                                          */
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

#ifndef WEBSOCKET_MULTIPLAYER_PEER_H
#define WEBSOCKET_MULTIPLAYER_PEER_H

#include "websocket_peer.h"

#include "core/error/error_list.h"
#include "core/io/tcp_server.h"
#include "core/templates/list.h"
#include "scene/main/multiplayer_peer.h"

class WebSocketMultiplayerPeer : public MultiplayerPeer {
	GDCLASS(WebSocketMultiplayerPeer, MultiplayerPeer);

private:
	Ref<WebSocketPeer> _create_peer();

protected:
	enum {
		SYS_NONE = 0,
		SYS_ADD = 1,
		SYS_DEL = 2,
		SYS_ID = 3,

		PROTO_SIZE = 9
	};

	struct Packet {
		int source = 0;
		uint8_t *data = nullptr;
		uint32_t size = 0;
	};

	struct PendingPeer {
		uint64_t time = 0;
		Ref<StreamPeerTCP> tcp;
		Ref<StreamPeer> connection;
		Ref<WebSocketPeer> ws;
	};

	uint64_t handshake_timeout = 3000;
	Ref<WebSocketPeer> peer_config;
	HashMap<int, PendingPeer> pending_peers;
	Ref<TCPServer> tcp_server;
	Ref<TLSOptions> tls_server_options;

	ConnectionStatus connection_status = CONNECTION_DISCONNECTED;

	List<Packet> incoming_packets;
	HashMap<int, Ref<WebSocketPeer>> peers_map;
	Packet current_packet;

	int target_peer = 0;
	int unique_id = 0;

	static void _bind_methods();

	void _poll_client();
	void _poll_server();
	void _clear();

public:
	/* MultiplayerPeer */
	virtual void set_target_peer(int p_target_peer) override;
	virtual int get_packet_peer() const override;
	virtual int get_packet_channel() const override { return 0; }
	virtual TransferMode get_packet_mode() const override { return TRANSFER_MODE_RELIABLE; }
	virtual int get_unique_id() const override;
	virtual bool is_server_relay_supported() const override { return true; }

	virtual int get_max_packet_size() const override;
	virtual bool is_server() const override;
	virtual void poll() override;
	virtual void close() override;
	virtual void disconnect_peer(int p_peer_id, bool p_force = false) override;

	virtual ConnectionStatus get_connection_status() const override;

	/* PacketPeer */
	virtual int get_available_packet_count() const override;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

	/* WebSocketPeer */
	virtual Ref<WebSocketPeer> get_peer(int p_peer_id) const;

	Error create_client(const String &p_url, Ref<TLSOptions> p_options);
	Error create_server(int p_port, IPAddress p_bind_ip, Ref<TLSOptions> p_options);

	void set_supported_protocols(const Vector<String> &p_protocols);
	Vector<String> get_supported_protocols() const;

	void set_handshake_headers(const Vector<String> &p_headers);
	Vector<String> get_handshake_headers() const;

	void set_outbound_buffer_size(int p_buffer_size);
	int get_outbound_buffer_size() const;

	void set_inbound_buffer_size(int p_buffer_size);
	int get_inbound_buffer_size() const;

	float get_handshake_timeout() const;
	void set_handshake_timeout(float p_timeout);

	IPAddress get_peer_address(int p_peer_id) const;
	int get_peer_port(int p_peer_id) const;

	void set_max_queued_packets(int p_max_queued_packets);
	int get_max_queued_packets() const;

	WebSocketMultiplayerPeer();
	~WebSocketMultiplayerPeer();
};

#endif // WEBSOCKET_MULTIPLAYER_PEER_H
