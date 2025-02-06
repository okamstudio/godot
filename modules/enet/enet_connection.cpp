/**************************************************************************/
/*  enet_connection.cpp                                                   */
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

#include "enet_connection.h"

#include "enet_packet_peer.h"

#include "core/io/compression.h"
#include "core/io/ip.h"
#include "core/variant/typed_array.h"

void ENetConnection::broadcast(enet_uint8 p_channel, ENetPacket *p_packet) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_MSG(p_channel >= host->channelLimit, vformat("Unable to send packet on channel %d, max channels: %d", p_channel, (int)host->channelLimit));
	enet_host_broadcast(host, p_channel, p_packet);
}

Error ENetConnection::create_host_bound(const IPAddress &p_bind_address, int p_port, int p_max_peers, int p_max_channels, int p_in_bandwidth, int p_out_bandwidth) {
	ERR_FAIL_COND_V_MSG(!p_bind_address.is_valid() && !p_bind_address.is_wildcard(), ERR_INVALID_PARAMETER, "Invalid bind IP.");
	ERR_FAIL_COND_V_MSG(p_port < 0 || p_port > 65535, ERR_INVALID_PARAMETER, "The local port number must be between 0 and 65535 (inclusive).");

	ENetAddress address;
	memset(&address, 0, sizeof(address));
	address.port = p_port;
#ifdef GODOT_ENET
	if (p_bind_address.is_wildcard()) {
		address.wildcard = 1;
	} else {
		enet_address_set_ip(&address, p_bind_address.get_ipv6(), 16);
	}
#else
	if (p_bind_address.is_wildcard()) {
		address.host = 0;
	} else {
		ERR_FAIL_COND_V(!p_bind_address.is_ipv4(), ERR_INVALID_PARAMETER);
		address.host = *(uint32_t *)p_bind_address.get_ipv4();
	}
#endif
	return _create(&address, p_max_peers, p_max_channels, p_in_bandwidth, p_out_bandwidth);
}

Error ENetConnection::create_host(int p_max_peers, int p_max_channels, int p_in_bandwidth, int p_out_bandwidth) {
	return _create(nullptr, p_max_peers, p_max_channels, p_in_bandwidth, p_out_bandwidth);
}

void ENetConnection::destroy() {
	ERR_FAIL_NULL_MSG(host, "Host already destroyed.");
	for (const Ref<ENetPacketPeer> &peer : peers) {
		peer->_on_disconnect();
	}
	peers.clear();
	enet_host_destroy(host);
	host = nullptr;
}

Ref<ENetPacketPeer> ENetConnection::connect_to_host(const String &p_address, int p_port, int p_channels, int p_data) {
	Ref<ENetPacketPeer> out;
	ERR_FAIL_NULL_V_MSG(host, out, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_V_MSG(peers.size(), out, "The ENetConnection is already connected to a peer.");
	ERR_FAIL_COND_V_MSG(p_port < 1 || p_port > 65535, out, "The remote port number must be between 1 and 65535 (inclusive).");

	IPAddress ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
#ifdef GODOT_ENET
		ip = IP::get_singleton()->resolve_hostname(p_address);
#else
		ip = IP::get_singleton()->resolve_hostname(p_address, IP::TYPE_IPV4);
#endif
		ERR_FAIL_COND_V_MSG(!ip.is_valid(), out, "Couldn't resolve the server IP address or domain name.");
	}

	ENetAddress address;
#ifdef GODOT_ENET
	enet_address_set_ip(&address, ip.get_ipv6(), 16);
#else
	ERR_FAIL_COND_V_MSG(!ip.is_ipv4(), out, "Connecting to an IPv6 server isn't supported when using vanilla ENet. Recompile Godot with the bundled ENet library.");
	address.host = *(uint32_t *)ip.get_ipv4();
#endif
	address.port = p_port;

	// Initiate connection, allocating enough channels
	ENetPeer *peer = enet_host_connect(host, &address, p_channels > 0 ? p_channels : ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, p_data);

	if (peer == nullptr) {
		return nullptr;
	}
	out.instantiate(peer);
	peers.push_back(out);
	return out;
}

ENetConnection::EventType ENetConnection::_parse_event(const ENetEvent &p_event, Event &r_event) {
	switch (p_event.type) {
		case ENET_EVENT_TYPE_CONNECT: {
			if (p_event.peer->data == nullptr) {
				Ref<ENetPacketPeer> pp = memnew(ENetPacketPeer(p_event.peer));
				peers.push_back(pp);
			}
			r_event.peer = Ref<ENetPacketPeer>((ENetPacketPeer *)p_event.peer->data);
			r_event.data = p_event.data;
			return EVENT_CONNECT;
		} break;
		case ENET_EVENT_TYPE_DISCONNECT: {
			// A peer disconnected.
			if (p_event.peer->data != nullptr) {
				Ref<ENetPacketPeer> pp = Ref<ENetPacketPeer>((ENetPacketPeer *)p_event.peer->data);
				pp->_on_disconnect();
				peers.erase(pp);
				r_event.peer = pp;
				r_event.data = p_event.data;
				return EVENT_DISCONNECT;
			}
			return EVENT_ERROR;
		} break;
		case ENET_EVENT_TYPE_RECEIVE: {
			// Packet received.
			if (p_event.peer->data != nullptr) {
				Ref<ENetPacketPeer> pp = Ref<ENetPacketPeer>((ENetPacketPeer *)p_event.peer->data);
				r_event.peer = Ref<ENetPacketPeer>((ENetPacketPeer *)p_event.peer->data);
				r_event.channel_id = p_event.channelID;
				r_event.packet = p_event.packet;
				return EVENT_RECEIVE;
			}
			return EVENT_ERROR;
		} break;
		case ENET_EVENT_TYPE_NONE:
			return EVENT_NONE;
		default:
			return EVENT_NONE;
	}
}

ENetConnection::EventType ENetConnection::service(int p_timeout, Event &r_event) {
	ERR_FAIL_NULL_V_MSG(host, EVENT_ERROR, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_V(r_event.peer.is_valid(), EVENT_ERROR);

	// Drop peers that have already been disconnected.
	// NOTE: Forcibly disconnected peers (i.e. peers disconnected via
	// enet_peer_disconnect*) do not trigger DISCONNECTED events.
	List<Ref<ENetPacketPeer>>::Element *E = peers.front();
	while (E) {
		if (!E->get()->is_active()) {
			peers.erase(E->get());
		}
		E = E->next();
	}

	ENetEvent event;
	int ret = enet_host_service(host, &event, p_timeout);

	if (ret < 0) {
		return EVENT_ERROR;
	} else if (ret == 0) {
		return EVENT_NONE;
	}
	return _parse_event(event, r_event);
}

int ENetConnection::check_events(EventType &r_type, Event &r_event) {
	ERR_FAIL_NULL_V_MSG(host, -1, "The ENetConnection instance isn't currently active.");
	ENetEvent event;
	int ret = enet_host_check_events(host, &event);
	if (ret < 0) {
		r_type = EVENT_ERROR;
		return ret;
	}
	r_type = _parse_event(event, r_event);
	return ret;
}

void ENetConnection::flush() {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	enet_host_flush(host);
}

void ENetConnection::bandwidth_limit(int p_in_bandwidth, int p_out_bandwidth) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	enet_host_bandwidth_limit(host, p_in_bandwidth, p_out_bandwidth);
}

void ENetConnection::channel_limit(int p_max_channels) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	enet_host_channel_limit(host, p_max_channels);
}

void ENetConnection::bandwidth_throttle() {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	enet_host_bandwidth_throttle(host);
}

void ENetConnection::compress(CompressionMode p_mode) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	Compressor::setup(host, p_mode);
}

double ENetConnection::pop_statistic(HostStatistic p_stat) {
	ERR_FAIL_NULL_V_MSG(host, 0, "The ENetConnection instance isn't currently active.");
	uint32_t *ptr = nullptr;
	switch (p_stat) {
		case HOST_TOTAL_SENT_DATA:
			ptr = &(host->totalSentData);
			break;
		case HOST_TOTAL_SENT_PACKETS:
			ptr = &(host->totalSentPackets);
			break;
		case HOST_TOTAL_RECEIVED_DATA:
			ptr = &(host->totalReceivedData);
			break;
		case HOST_TOTAL_RECEIVED_PACKETS:
			ptr = &(host->totalReceivedPackets);
			break;
	}
	ERR_FAIL_NULL_V_MSG(ptr, 0, "Invalid statistic: " + itos(p_stat) + ".");
	uint32_t ret = *ptr;
	*ptr = 0;
	return ret;
}

int ENetConnection::get_max_channels() const {
	ERR_FAIL_NULL_V_MSG(host, 0, "The ENetConnection instance isn't currently active.");
	return host->channelLimit;
}

int ENetConnection::get_local_port() const {
	ERR_FAIL_NULL_V_MSG(host, 0, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_V_MSG(!(host->socket), 0, "The ENetConnection instance isn't currently bound.");
	ENetAddress address;
	ERR_FAIL_COND_V_MSG(enet_socket_get_address(host->socket, &address), 0, "Unable to get socket address");
	return address.port;
}

void ENetConnection::get_peers(List<Ref<ENetPacketPeer>> &r_peers) {
	for (const Ref<ENetPacketPeer> &I : peers) {
		r_peers.push_back(I);
	}
}

TypedArray<ENetPacketPeer> ENetConnection::_get_peers() {
	ERR_FAIL_NULL_V_MSG(host, Array(), "The ENetConnection instance isn't currently active.");
	TypedArray<ENetPacketPeer> out;
	for (const Ref<ENetPacketPeer> &I : peers) {
		out.push_back(I);
	}
	return out;
}

Error ENetConnection::dtls_server_setup(const Ref<TLSOptions> &p_options) {
#ifdef GODOT_ENET
	ERR_FAIL_NULL_V_MSG(host, ERR_UNCONFIGURED, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_V(p_options.is_null() || !p_options->is_server(), ERR_INVALID_PARAMETER);
	return enet_host_dtls_server_setup(host, p_options.ptr()) ? FAILED : OK;
#else
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "ENet DTLS support not available in this build.");
#endif
}

void ENetConnection::refuse_new_connections(bool p_refuse) {
#ifdef GODOT_ENET
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	enet_host_refuse_new_connections(host, p_refuse);
#else
	ERR_FAIL_MSG("ENet DTLS support not available in this build.");
#endif
}

Error ENetConnection::dtls_client_setup(const String &p_hostname, const Ref<TLSOptions> &p_options) {
#ifdef GODOT_ENET
	ERR_FAIL_NULL_V_MSG(host, ERR_UNCONFIGURED, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_V(p_options.is_null() || p_options->is_server(), ERR_INVALID_PARAMETER);
	return enet_host_dtls_client_setup(host, p_hostname.utf8().get_data(), p_options.ptr()) ? FAILED : OK;
#else
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "ENet DTLS support not available in this build.");
#endif
}

Error ENetConnection::_create(ENetAddress *p_address, int p_max_peers, int p_max_channels, int p_in_bandwidth, int p_out_bandwidth) {
	ERR_FAIL_COND_V_MSG(host != nullptr, ERR_ALREADY_IN_USE, "The ENetConnection instance is already active.");
	ERR_FAIL_COND_V_MSG(p_max_peers < 1 || p_max_peers > 4095, ERR_INVALID_PARAMETER, "The number of clients must be set between 1 and 4095 (inclusive).");
	ERR_FAIL_COND_V_MSG(p_max_channels < 0 || p_max_channels > ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, ERR_INVALID_PARAMETER, "Invalid channel count. Must be between 0 and 255 (0 means maximum, i.e. 255)");
	ERR_FAIL_COND_V_MSG(p_in_bandwidth < 0, ERR_INVALID_PARAMETER, "The incoming bandwidth limit must be greater than or equal to 0 (0 disables the limit).");
	ERR_FAIL_COND_V_MSG(p_out_bandwidth < 0, ERR_INVALID_PARAMETER, "The outgoing bandwidth limit must be greater than or equal to 0 (0 disables the limit).");

	host = enet_host_create(p_address /* the address to bind the server host to */,
			p_max_peers /* allow up to p_max_peers connections */,
			p_max_channels /* allow up to p_max_channel to be used */,
			p_in_bandwidth /* limit incoming bandwidth if > 0 */,
			p_out_bandwidth /* limit outgoing bandwidth if > 0 */);

	ERR_FAIL_NULL_V_MSG(host, ERR_CANT_CREATE, "Couldn't create an ENet host.");
	return OK;
}

Array ENetConnection::_service(int p_timeout) {
	Array out;
	Event event;
	Ref<ENetPacketPeer> peer;
	EventType ret = service(p_timeout, event);
	out.push_back(ret);
	out.push_back(event.peer);
	out.push_back(event.data);
	out.push_back(event.channel_id);
	if (event.packet && event.peer.is_valid()) {
		event.peer->_queue_packet(event.packet);
	}
	return out;
}

void ENetConnection::_broadcast(int p_channel, PackedByteArray p_packet, int p_flags) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_MSG(p_channel < 0 || p_channel > (int)host->channelLimit, "Invalid channel");
	ERR_FAIL_COND_MSG(p_flags & ~ENetPacketPeer::FLAG_ALLOWED, "Invalid flags");
	ENetPacket *pkt = enet_packet_create(p_packet.ptr(), p_packet.size(), p_flags);
	broadcast(p_channel, pkt);
}

void ENetConnection::socket_send(const String &p_address, int p_port, const PackedByteArray &p_packet) {
	ERR_FAIL_NULL_MSG(host, "The ENetConnection instance isn't currently active.");
	ERR_FAIL_COND_MSG(!(host->socket), "The ENetConnection instance isn't currently bound.");
	ERR_FAIL_COND_MSG(p_port < 1 || p_port > 65535, "The remote port number must be between 1 and 65535 (inclusive).");

	IPAddress ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
#ifdef GODOT_ENET
		ip = IP::get_singleton()->resolve_hostname(p_address);
#else
		ip = IP::get_singleton()->resolve_hostname(p_address, IP::TYPE_IPV4);
#endif
		ERR_FAIL_COND_MSG(!ip.is_valid(), "Couldn't resolve the server IP address or domain name.");
	}

	ENetAddress address;
#ifdef GODOT_ENET
	enet_address_set_ip(&address, ip.get_ipv6(), 16);
#else
	ERR_FAIL_COND_MSG(!ip.is_ipv4(), "Connecting to an IPv6 server isn't supported when using vanilla ENet. Recompile Godot with the bundled ENet library.");
	address.host = *(uint32_t *)ip.get_ipv4();
#endif
	address.port = p_port;

	ENetBuffer enet_buffers[1];
	enet_buffers[0].data = (void *)p_packet.ptr();
	enet_buffers[0].dataLength = p_packet.size();

	enet_socket_send(host->socket, &address, enet_buffers, 1);
}

void ENetConnection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_host_bound", "bind_address", "bind_port", "max_peers", "max_channels", "in_bandwidth", "out_bandwidth"), &ENetConnection::create_host_bound, DEFVAL(32), DEFVAL(0), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("create_host", "max_peers", "max_channels", "in_bandwidth", "out_bandwidth"), &ENetConnection::create_host, DEFVAL(32), DEFVAL(0), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("destroy"), &ENetConnection::destroy);
	ClassDB::bind_method(D_METHOD("connect_to_host", "address", "port", "channels", "data"), &ENetConnection::connect_to_host, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("service", "timeout"), &ENetConnection::_service, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("flush"), &ENetConnection::flush);
	ClassDB::bind_method(D_METHOD("bandwidth_limit", "in_bandwidth", "out_bandwidth"), &ENetConnection::bandwidth_limit, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("channel_limit", "limit"), &ENetConnection::channel_limit);
	ClassDB::bind_method(D_METHOD("broadcast", "channel", "packet", "flags"), &ENetConnection::_broadcast);
	ClassDB::bind_method(D_METHOD("compress", "mode"), &ENetConnection::compress);
	ClassDB::bind_method(D_METHOD("dtls_server_setup", "server_options"), &ENetConnection::dtls_server_setup);
	ClassDB::bind_method(D_METHOD("dtls_client_setup", "hostname", "client_options"), &ENetConnection::dtls_client_setup, DEFVAL(Ref<TLSOptions>()));
	ClassDB::bind_method(D_METHOD("refuse_new_connections", "refuse"), &ENetConnection::refuse_new_connections);
	ClassDB::bind_method(D_METHOD("pop_statistic", "statistic"), &ENetConnection::pop_statistic);
	ClassDB::bind_method(D_METHOD("get_max_channels"), &ENetConnection::get_max_channels);
	ClassDB::bind_method(D_METHOD("get_local_port"), &ENetConnection::get_local_port);
	ClassDB::bind_method(D_METHOD("get_peers"), &ENetConnection::_get_peers);
	ClassDB::bind_method(D_METHOD("socket_send", "destination_address", "destination_port", "packet"), &ENetConnection::socket_send);

	BIND_ENUM_CONSTANT(COMPRESS_NONE);
	BIND_ENUM_CONSTANT(COMPRESS_RANGE_CODER);
	BIND_ENUM_CONSTANT(COMPRESS_FASTLZ);
	BIND_ENUM_CONSTANT(COMPRESS_ZLIB);
	BIND_ENUM_CONSTANT(COMPRESS_ZSTD);

	BIND_ENUM_CONSTANT(EVENT_ERROR);
	BIND_ENUM_CONSTANT(EVENT_NONE);
	BIND_ENUM_CONSTANT(EVENT_CONNECT);
	BIND_ENUM_CONSTANT(EVENT_DISCONNECT);
	BIND_ENUM_CONSTANT(EVENT_RECEIVE);

	BIND_ENUM_CONSTANT(HOST_TOTAL_SENT_DATA);
	BIND_ENUM_CONSTANT(HOST_TOTAL_SENT_PACKETS);
	BIND_ENUM_CONSTANT(HOST_TOTAL_RECEIVED_DATA);
	BIND_ENUM_CONSTANT(HOST_TOTAL_RECEIVED_PACKETS);
}

ENetConnection::~ENetConnection() {
	if (host) {
		destroy();
	}
}

size_t ENetConnection::Compressor::enet_compress(void *context, const ENetBuffer *inBuffers, size_t inBufferCount, size_t inLimit, enet_uint8 *outData, size_t outLimit) {
	Compressor *compressor = (Compressor *)(context);

	if (size_t(compressor->src_mem.size()) < inLimit) {
		compressor->src_mem.resize(inLimit);
	}

	int total = inLimit;
	int ofs = 0;
	while (total) {
		for (size_t i = 0; i < inBufferCount; i++) {
			int to_copy = MIN(total, int(inBuffers[i].dataLength));
			memcpy(&compressor->src_mem.write[ofs], inBuffers[i].data, to_copy);
			ofs += to_copy;
			total -= to_copy;
		}
	}

	Compression::Mode mode;

	switch (compressor->mode) {
		case COMPRESS_FASTLZ: {
			mode = Compression::MODE_FASTLZ;
		} break;
		case COMPRESS_ZLIB: {
			mode = Compression::MODE_DEFLATE;
		} break;
		case COMPRESS_ZSTD: {
			mode = Compression::MODE_ZSTD;
		} break;
		default: {
			ERR_FAIL_V_MSG(0, vformat("Invalid ENet compression mode: %d", compressor->mode));
		}
	}

	int req_size = Compression::get_max_compressed_buffer_size(ofs, mode);
	if (compressor->dst_mem.size() < req_size) {
		compressor->dst_mem.resize(req_size);
	}
	int ret = Compression::compress(compressor->dst_mem.ptrw(), compressor->src_mem.ptr(), ofs, mode);

	if (ret < 0) {
		return 0;
	}

	if (ret > int(outLimit)) {
		return 0; // Do not bother
	}

	memcpy(outData, compressor->dst_mem.ptr(), ret);

	return ret;
}

size_t ENetConnection::Compressor::enet_decompress(void *context, const enet_uint8 *inData, size_t inLimit, enet_uint8 *outData, size_t outLimit) {
	Compressor *compressor = (Compressor *)(context);
	int ret = -1;
	switch (compressor->mode) {
		case COMPRESS_FASTLZ: {
			ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_FASTLZ);
		} break;
		case COMPRESS_ZLIB: {
			ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_DEFLATE);
		} break;
		case COMPRESS_ZSTD: {
			ret = Compression::decompress(outData, outLimit, inData, inLimit, Compression::MODE_ZSTD);
		} break;
		default: {
		}
	}
	if (ret < 0) {
		return 0;
	} else {
		return ret;
	}
}

void ENetConnection::Compressor::setup(ENetHost *p_host, CompressionMode p_mode) {
	ERR_FAIL_NULL(p_host);
	switch (p_mode) {
		case COMPRESS_NONE: {
			enet_host_compress(p_host, nullptr);
		} break;
		case COMPRESS_RANGE_CODER: {
			enet_host_compress_with_range_coder(p_host);
		} break;
		case COMPRESS_FASTLZ:
		case COMPRESS_ZLIB:
		case COMPRESS_ZSTD: {
			Compressor *compressor = memnew(Compressor(p_mode));
			enet_host_compress(p_host, &(compressor->enet_compressor));
		} break;
	}
}

ENetConnection::Compressor::Compressor(CompressionMode p_mode) {
	mode = p_mode;
	enet_compressor.context = this;
	enet_compressor.compress = enet_compress;
	enet_compressor.decompress = enet_decompress;
	enet_compressor.destroy = enet_compressor_destroy;
}
