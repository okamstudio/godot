/**************************************************************************/
/*  enet_connection.h                                                     */
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

#include "enet_packet_peer.h"

#include "core/crypto/crypto.h"
#include "core/object/ref_counted.h"

#include <enet/enet.h>

template <typename T>
class TypedArray;

class ENetConnection : public RefCounted {
	GDCLASS(ENetConnection, RefCounted);

public:
	enum CompressionMode {
		COMPRESS_NONE = 0,
		COMPRESS_RANGE_CODER,
		COMPRESS_FASTLZ,
		COMPRESS_ZLIB,
		COMPRESS_ZSTD,
	};

	enum HostStatistic {
		HOST_TOTAL_SENT_DATA,
		HOST_TOTAL_SENT_PACKETS,
		HOST_TOTAL_RECEIVED_DATA,
		HOST_TOTAL_RECEIVED_PACKETS,
	};

	enum EventType {
		EVENT_ERROR = -1,
		EVENT_NONE = 0,
		EVENT_CONNECT,
		EVENT_DISCONNECT,
		EVENT_RECEIVE,
	};

	struct Event {
		Ref<ENetPacketPeer> peer;
		enet_uint8 channel_id = 0;
		enet_uint32 data = 0;
		ENetPacket *packet = nullptr;
	};

protected:
	static void _bind_methods();

private:
	ENetHost *host = nullptr;
	List<Ref<ENetPacketPeer>> peers;

	EventType _parse_event(const ENetEvent &p_event, Event &r_event);
	Error _create(ENetAddress *p_address, int p_max_peers, int p_max_channels, int p_in_bandwidth, int p_out_bandwidth);
	Array _service(int p_timeout = 0);
	void _broadcast(int p_channel, PackedByteArray p_packet, int p_flags);
	TypedArray<ENetPacketPeer> _get_peers();

	class Compressor {
	private:
		CompressionMode mode = COMPRESS_NONE;
		Vector<uint8_t> src_mem;
		Vector<uint8_t> dst_mem;
		ENetCompressor enet_compressor;

		Compressor(CompressionMode mode);

		static size_t enet_compress(void *context, const ENetBuffer *inBuffers, size_t inBufferCount, size_t inLimit, enet_uint8 *outData, size_t outLimit);
		static size_t enet_decompress(void *context, const enet_uint8 *inData, size_t inLimit, enet_uint8 *outData, size_t outLimit);
		static void enet_compressor_destroy(void *context) {
			memdelete((Compressor *)context);
		}

	public:
		static void setup(ENetHost *p_host, CompressionMode p_mode);
	};

public:
	void broadcast(enet_uint8 p_channel, ENetPacket *p_packet);
	void socket_send(const String &p_address, int p_port, const PackedByteArray &p_packet);
	Error create_host_bound(const IPAddress &p_bind_address = IPAddress("*"), int p_port = 0, int p_max_peers = 32, int p_max_channels = 0, int p_in_bandwidth = 0, int p_out_bandwidth = 0);
	Error create_host(int p_max_peers = 32, int p_max_channels = 0, int p_in_bandwidth = 0, int p_out_bandwidth = 0);
	void destroy();
	Ref<ENetPacketPeer> connect_to_host(const String &p_address, int p_port, int p_channels, int p_data = 0);
	EventType service(int p_timeout, Event &r_event);
	int check_events(EventType &r_type, Event &r_event);
	void flush();
	void bandwidth_limit(int p_in_bandwidth = 0, int p_out_bandwidth = 0);
	void channel_limit(int p_max_channels);
	void bandwidth_throttle();
	void compress(CompressionMode p_mode);
	double pop_statistic(HostStatistic p_stat);
	int get_max_channels() const;

	// Extras
	void get_peers(List<Ref<ENetPacketPeer>> &r_peers);
	int get_local_port() const;

	// Godot additions
	Error dtls_server_setup(const Ref<TLSOptions> &p_options);
	Error dtls_client_setup(const String &p_hostname, const Ref<TLSOptions> &p_options);
	void refuse_new_connections(bool p_refuse);

	ENetConnection() {}
	~ENetConnection();
};

VARIANT_ENUM_CAST(ENetConnection::CompressionMode);
VARIANT_ENUM_CAST(ENetConnection::EventType);
VARIANT_ENUM_CAST(ENetConnection::HostStatistic);
