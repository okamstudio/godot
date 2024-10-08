/**************************************************************************/
/*  enet_packet_peer.cpp                                                  */
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

#include "enet_packet_peer.h"

void ENetPacketPeer::peer_disconnect(int p_data) {
	ERR_FAIL_NULL(peer);
	enet_peer_disconnect(peer, p_data);
}

void ENetPacketPeer::peer_disconnect_later(int p_data) {
	ERR_FAIL_NULL(peer);
	enet_peer_disconnect_later(peer, p_data);
}

void ENetPacketPeer::peer_disconnect_now(int p_data) {
	ERR_FAIL_NULL(peer);
	enet_peer_disconnect_now(peer, p_data);
	_on_disconnect();
}

void ENetPacketPeer::ping() {
	ERR_FAIL_NULL(peer);
	enet_peer_ping(peer);
}

void ENetPacketPeer::ping_interval(int p_interval) {
	ERR_FAIL_NULL(peer);
	enet_peer_ping_interval(peer, p_interval);
}

int ENetPacketPeer::send(uint8_t p_channel, ENetPacket *p_packet) {
	ERR_FAIL_NULL_V(peer, -1);
	ERR_FAIL_NULL_V(p_packet, -1);
	ERR_FAIL_COND_V_MSG(p_channel >= peer->channelCount, -1, vformat("Unable to send packet on channel %d, max channels: %d", p_channel, (int)peer->channelCount));
	return enet_peer_send(peer, p_channel, p_packet);
}

void ENetPacketPeer::reset() {
	ERR_FAIL_NULL_MSG(peer, "Peer not connected.");
	enet_peer_reset(peer);
	_on_disconnect();
}

void ENetPacketPeer::throttle_configure(int p_interval, int p_acceleration, int p_deceleration) {
	ERR_FAIL_NULL_MSG(peer, "Peer not connected.");
	enet_peer_throttle_configure(peer, p_interval, p_acceleration, p_deceleration);
}

void ENetPacketPeer::set_timeout(int p_timeout, int p_timeout_min, int p_timeout_max) {
	ERR_FAIL_NULL_MSG(peer, "Peer not connected.");
	ERR_FAIL_COND_MSG(p_timeout > p_timeout_min || p_timeout_min > p_timeout_max, "Timeout limit must be less than minimum timeout, which itself must be less than maximum timeout");
	enet_peer_timeout(peer, p_timeout, p_timeout_min, p_timeout_max);
}

int ENetPacketPeer::get_max_packet_size() const {
	return 1 << 24;
}

int ENetPacketPeer::get_available_packet_count() const {
	return packet_queue.size();
}

Error ENetPacketPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	ERR_FAIL_NULL_V(peer, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(packet_queue.is_empty(), ERR_UNAVAILABLE);
	if (last_packet) {
		enet_packet_destroy(last_packet);
		last_packet = nullptr;
	}
	last_packet = packet_queue.front()->get();
	packet_queue.pop_front();
	*r_buffer = (const uint8_t *)(last_packet->data);
	r_buffer_size = last_packet->dataLength;
	return OK;
}

Error ENetPacketPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_NULL_V(peer, ERR_UNCONFIGURED);
	ENetPacket *packet = enet_packet_create(p_buffer, p_buffer_size, ENET_PACKET_FLAG_RELIABLE);
	return send(0, packet) < 0 ? FAILED : OK;
}

IPAddress ENetPacketPeer::get_remote_address() const {
	ERR_FAIL_NULL_V(peer, IPAddress());
	IPAddress out;
#ifdef GODOT_ENET
	out.set_ipv6((uint8_t *)&(peer->address.host));
#else
	out.set_ipv4((uint8_t *)&(peer->address.host));
#endif
	return out;
}

int ENetPacketPeer::get_remote_port() const {
	ERR_FAIL_NULL_V(peer, 0);
	return peer->address.port;
}

bool ENetPacketPeer::is_active() const {
	return peer != nullptr;
}

double ENetPacketPeer::get_statistic(PeerStatistic p_stat) {
	ERR_FAIL_NULL_V(peer, 0);
	switch (p_stat) {
		case PEER_PACKET_LOSS:
			return peer->packetLoss;
		case PEER_PACKET_LOSS_VARIANCE:
			return peer->packetLossVariance;
		case PEER_PACKET_LOSS_EPOCH:
			return peer->packetLossEpoch;
		case PEER_ROUND_TRIP_TIME:
			return peer->roundTripTime;
		case PEER_ROUND_TRIP_TIME_VARIANCE:
			return peer->roundTripTimeVariance;
		case PEER_LAST_ROUND_TRIP_TIME:
			return peer->lastRoundTripTime;
		case PEER_LAST_ROUND_TRIP_TIME_VARIANCE:
			return peer->lastRoundTripTimeVariance;
		case PEER_PACKET_THROTTLE:
			return peer->packetThrottle;
		case PEER_PACKET_THROTTLE_LIMIT:
			return peer->packetThrottleLimit;
		case PEER_PACKET_THROTTLE_COUNTER:
			return peer->packetThrottleCounter;
		case PEER_PACKET_THROTTLE_EPOCH:
			return peer->packetThrottleEpoch;
		case PEER_PACKET_THROTTLE_ACCELERATION:
			return peer->packetThrottleAcceleration;
		case PEER_PACKET_THROTTLE_DECELERATION:
			return peer->packetThrottleDeceleration;
		case PEER_PACKET_THROTTLE_INTERVAL:
			return peer->packetThrottleInterval;
	}
	ERR_FAIL_V(0);
}

ENetPacketPeer::PeerState ENetPacketPeer::get_state() const {
	if (!is_active()) {
		return STATE_DISCONNECTED;
	}
	return (PeerState)peer->state;
}

int ENetPacketPeer::get_channels() const {
	ERR_FAIL_NULL_V_MSG(peer, 0, "The ENetConnection instance isn't currently active.");
	return peer->channelCount;
}

int ENetPacketPeer::get_packet_flags() const {
	ERR_FAIL_COND_V(packet_queue.is_empty(), 0);
	return packet_queue.front()->get()->flags;
}

void ENetPacketPeer::_on_disconnect() {
	if (peer) {
		peer->data = nullptr;
	}
	peer = nullptr;
}

void ENetPacketPeer::_queue_packet(ENetPacket *p_packet) {
	ERR_FAIL_NULL(peer);
	packet_queue.push_back(p_packet);
}

Error ENetPacketPeer::_send(int p_channel, PackedByteArray p_packet, int p_flags) {
	ERR_FAIL_NULL_V_MSG(peer, ERR_UNCONFIGURED, "Peer not connected.");
	ERR_FAIL_COND_V_MSG(p_channel < 0 || p_channel > (int)peer->channelCount, ERR_INVALID_PARAMETER, "Invalid channel");
	ERR_FAIL_COND_V_MSG(p_flags & ~FLAG_ALLOWED, ERR_INVALID_PARAMETER, "Invalid flags");
	ENetPacket *packet = enet_packet_create(p_packet.ptr(), p_packet.size(), p_flags);
	return send(p_channel, packet) == 0 ? OK : FAILED;
}

void ENetPacketPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("peer_disconnect", "data"), &ENetPacketPeer::peer_disconnect, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("peer_disconnect_later", "data"), &ENetPacketPeer::peer_disconnect_later, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("peer_disconnect_now", "data"), &ENetPacketPeer::peer_disconnect_now, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("ping"), &ENetPacketPeer::ping);
	ClassDB::bind_method(D_METHOD("ping_interval", "ping_interval"), &ENetPacketPeer::ping_interval);
	ClassDB::bind_method(D_METHOD("reset"), &ENetPacketPeer::reset);
	ClassDB::bind_method(D_METHOD("send", "channel", "packet", "flags"), &ENetPacketPeer::_send);
	ClassDB::bind_method(D_METHOD("throttle_configure", "interval", "acceleration", "deceleration"), &ENetPacketPeer::throttle_configure);
	ClassDB::bind_method(D_METHOD("set_timeout", "timeout", "timeout_min", "timeout_max"), &ENetPacketPeer::set_timeout);
	ClassDB::bind_method(D_METHOD("get_packet_flags"), &ENetPacketPeer::get_packet_flags);
	ClassDB::bind_method(D_METHOD("get_remote_address"), &ENetPacketPeer::get_remote_address);
	ClassDB::bind_method(D_METHOD("get_remote_port"), &ENetPacketPeer::get_remote_port);
	ClassDB::bind_method(D_METHOD("get_statistic", "statistic"), &ENetPacketPeer::get_statistic);
	ClassDB::bind_method(D_METHOD("get_state"), &ENetPacketPeer::get_state);
	ClassDB::bind_method(D_METHOD("get_channels"), &ENetPacketPeer::get_channels);
	ClassDB::bind_method(D_METHOD("is_active"), &ENetPacketPeer::is_active);

	BIND_ENUM_CONSTANT(STATE_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_ACKNOWLEDGING_CONNECT);
	BIND_ENUM_CONSTANT(STATE_CONNECTION_PENDING);
	BIND_ENUM_CONSTANT(STATE_CONNECTION_SUCCEEDED);
	BIND_ENUM_CONSTANT(STATE_CONNECTED);
	BIND_ENUM_CONSTANT(STATE_DISCONNECT_LATER);
	BIND_ENUM_CONSTANT(STATE_DISCONNECTING);
	BIND_ENUM_CONSTANT(STATE_ACKNOWLEDGING_DISCONNECT);
	BIND_ENUM_CONSTANT(STATE_ZOMBIE);

	BIND_ENUM_CONSTANT(PEER_PACKET_LOSS);
	BIND_ENUM_CONSTANT(PEER_PACKET_LOSS_VARIANCE);
	BIND_ENUM_CONSTANT(PEER_PACKET_LOSS_EPOCH);
	BIND_ENUM_CONSTANT(PEER_ROUND_TRIP_TIME);
	BIND_ENUM_CONSTANT(PEER_ROUND_TRIP_TIME_VARIANCE);
	BIND_ENUM_CONSTANT(PEER_LAST_ROUND_TRIP_TIME);
	BIND_ENUM_CONSTANT(PEER_LAST_ROUND_TRIP_TIME_VARIANCE);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_LIMIT);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_COUNTER);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_EPOCH);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_ACCELERATION);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_DECELERATION);
	BIND_ENUM_CONSTANT(PEER_PACKET_THROTTLE_INTERVAL);

	BIND_CONSTANT(PACKET_LOSS_SCALE);
	BIND_CONSTANT(PACKET_THROTTLE_SCALE);

	BIND_CONSTANT(FLAG_RELIABLE);
	BIND_CONSTANT(FLAG_UNSEQUENCED);
	BIND_CONSTANT(FLAG_UNRELIABLE_FRAGMENT);
}

ENetPacketPeer::ENetPacketPeer(ENetPeer *p_peer) {
	peer = p_peer;
	peer->data = this;
}

ENetPacketPeer::~ENetPacketPeer() {
	_on_disconnect();
	if (last_packet) {
		enet_packet_destroy(last_packet);
		last_packet = nullptr;
	}
	for (List<ENetPacket *>::Element *E = packet_queue.front(); E; E = E->next()) {
		enet_packet_destroy(E->get());
	}
	packet_queue.clear();
}
