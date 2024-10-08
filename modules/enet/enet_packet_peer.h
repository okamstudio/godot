/**************************************************************************/
/*  enet_packet_peer.h                                                    */
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

#ifndef ENET_PACKET_PEER_H
#define ENET_PACKET_PEER_H

#include "core/io/packet_peer.h"

#include <enet/enet.h>

class ENetPacketPeer : public PacketPeer {
	GDCLASS(ENetPacketPeer, PacketPeer);

private:
	ENetPeer *peer = nullptr;
	List<ENetPacket *> packet_queue;
	ENetPacket *last_packet = nullptr;

	static void _bind_methods();
	Error _send(int p_channel, PackedByteArray p_packet, int p_flags);

protected:
	friend class ENetConnection;
	// Internally used by ENetConnection during service, destroy, etc.
	void _on_disconnect();
	void _queue_packet(ENetPacket *p_packet);

public:
	enum {
		PACKET_THROTTLE_SCALE = ENET_PEER_PACKET_THROTTLE_SCALE,
		PACKET_LOSS_SCALE = ENET_PEER_PACKET_LOSS_SCALE,
	};

	enum {
		FLAG_RELIABLE = ENET_PACKET_FLAG_RELIABLE,
		FLAG_UNSEQUENCED = ENET_PACKET_FLAG_UNSEQUENCED,
		FLAG_UNRELIABLE_FRAGMENT = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT,
		FLAG_ALLOWED = ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_UNSEQUENCED | ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT,
	};

	enum PeerState {
		STATE_DISCONNECTED = ENET_PEER_STATE_DISCONNECTED,
		STATE_CONNECTING = ENET_PEER_STATE_CONNECTING,
		STATE_ACKNOWLEDGING_CONNECT = ENET_PEER_STATE_ACKNOWLEDGING_CONNECT,
		STATE_CONNECTION_PENDING = ENET_PEER_STATE_CONNECTION_PENDING,
		STATE_CONNECTION_SUCCEEDED = ENET_PEER_STATE_CONNECTION_SUCCEEDED,
		STATE_CONNECTED = ENET_PEER_STATE_CONNECTED,
		STATE_DISCONNECT_LATER = ENET_PEER_STATE_DISCONNECT_LATER,
		STATE_DISCONNECTING = ENET_PEER_STATE_DISCONNECTING,
		STATE_ACKNOWLEDGING_DISCONNECT = ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT,
		STATE_ZOMBIE = ENET_PEER_STATE_ZOMBIE,
	};

	enum PeerStatistic {
		PEER_PACKET_LOSS,
		PEER_PACKET_LOSS_VARIANCE,
		PEER_PACKET_LOSS_EPOCH,
		PEER_ROUND_TRIP_TIME,
		PEER_ROUND_TRIP_TIME_VARIANCE,
		PEER_LAST_ROUND_TRIP_TIME,
		PEER_LAST_ROUND_TRIP_TIME_VARIANCE,
		PEER_PACKET_THROTTLE,
		PEER_PACKET_THROTTLE_LIMIT,
		PEER_PACKET_THROTTLE_COUNTER,
		PEER_PACKET_THROTTLE_EPOCH,
		PEER_PACKET_THROTTLE_ACCELERATION,
		PEER_PACKET_THROTTLE_DECELERATION,
		PEER_PACKET_THROTTLE_INTERVAL,
	};

	int get_max_packet_size() const override;
	int get_available_packet_count() const override;
	Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override; ///< buffer is GONE after next get_packet
	Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

	void peer_disconnect(int p_data = 0);
	void peer_disconnect_later(int p_data = 0);
	void peer_disconnect_now(int p_data = 0);

	void ping();
	void ping_interval(int p_interval);
	void reset();
	int send(uint8_t p_channel, ENetPacket *p_packet);
	void throttle_configure(int interval, int acceleration, int deceleration);
	void set_timeout(int p_timeout, int p_timeout_min, int p_timeout_max);
	double get_statistic(PeerStatistic p_stat);
	PeerState get_state() const;
	int get_channels() const;
	int get_packet_flags() const;

	// Extras
	IPAddress get_remote_address() const;
	int get_remote_port() const;

	// Used by ENetMultiplayer (TODO use meta? If only they where StringNames)
	bool is_active() const;

	ENetPacketPeer(ENetPeer *p_peer);
	~ENetPacketPeer();
};

VARIANT_ENUM_CAST(ENetPacketPeer::PeerState);
VARIANT_ENUM_CAST(ENetPacketPeer::PeerStatistic);

#endif // ENET_PACKET_PEER_H
