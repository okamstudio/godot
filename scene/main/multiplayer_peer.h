/**************************************************************************/
/*  multiplayer_peer.h                                                    */
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

#include "core/io/packet_peer.h"

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/gdvirtual.gen.inc"
#include "core/variant/native_ptr.h"

class MultiplayerPeer : public PacketPeer {
	GDCLASS(MultiplayerPeer, PacketPeer);

public:
	enum TransferMode {
		TRANSFER_MODE_UNRELIABLE,
		TRANSFER_MODE_UNRELIABLE_ORDERED,
		TRANSFER_MODE_RELIABLE
	};

protected:
	static void _bind_methods();

private:
	int transfer_channel = 0;
	TransferMode transfer_mode = TRANSFER_MODE_RELIABLE;
	bool refuse_connections = false;

public:
	enum {
		TARGET_PEER_BROADCAST = 0,
		TARGET_PEER_SERVER = 1
	};

	enum ConnectionStatus {
		CONNECTION_DISCONNECTED,
		CONNECTION_CONNECTING,
		CONNECTION_CONNECTED,
	};

	virtual void set_transfer_channel(int p_channel);
	virtual int get_transfer_channel() const;
	virtual void set_transfer_mode(TransferMode p_mode);
	virtual TransferMode get_transfer_mode() const;
	virtual void set_refuse_new_connections(bool p_enable);
	virtual bool is_refusing_new_connections() const;
	virtual bool is_server_relay_supported() const;

	virtual void set_target_peer(int p_peer_id) = 0;

	virtual int get_packet_peer() const = 0;
	virtual TransferMode get_packet_mode() const = 0;
	virtual int get_packet_channel() const = 0;

	virtual void disconnect_peer(int p_peer, bool p_force = false) = 0;

	virtual bool is_server() const = 0;

	virtual void poll() = 0;
	virtual void close() = 0;

	virtual int get_unique_id() const = 0;

	virtual ConnectionStatus get_connection_status() const = 0;

	uint32_t generate_unique_id() const;

	MultiplayerPeer() {}
};

VARIANT_ENUM_CAST(MultiplayerPeer::ConnectionStatus);
VARIANT_ENUM_CAST(MultiplayerPeer::TransferMode);

class MultiplayerPeerExtension : public MultiplayerPeer {
	GDCLASS(MultiplayerPeerExtension, MultiplayerPeer);

protected:
	static void _bind_methods();

	PackedByteArray script_buffer;

public:
	/* PacketPeer extension */
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override; ///< buffer is GONE after next get_packet
	GDVIRTUAL2R(Error, _get_packet, GDExtensionConstPtr<const uint8_t *>, GDExtensionPtr<int>);
	GDVIRTUAL0R(PackedByteArray, _get_packet_script); // For GDScript.

	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	GDVIRTUAL2R(Error, _put_packet, GDExtensionConstPtr<const uint8_t>, int);
	GDVIRTUAL1R(Error, _put_packet_script, PackedByteArray); // For GDScript.

	EXBIND0RC(int, get_available_packet_count);
	EXBIND0RC(int, get_max_packet_size);

	/* MultiplayerPeer extension */
	virtual void set_refuse_new_connections(bool p_enable) override;
	GDVIRTUAL1(_set_refuse_new_connections, bool); // Optional.

	virtual bool is_refusing_new_connections() const override;
	GDVIRTUAL0RC(bool, _is_refusing_new_connections); // Optional.

	virtual bool is_server_relay_supported() const override;
	GDVIRTUAL0RC(bool, _is_server_relay_supported); // Optional.

	EXBIND1(set_transfer_channel, int);
	EXBIND0RC(int, get_transfer_channel);
	EXBIND1(set_transfer_mode, TransferMode);
	EXBIND0RC(TransferMode, get_transfer_mode);
	EXBIND1(set_target_peer, int);
	EXBIND0RC(int, get_packet_peer);
	EXBIND0RC(TransferMode, get_packet_mode);
	EXBIND0RC(int, get_packet_channel);
	EXBIND0RC(bool, is_server);
	EXBIND0(poll);
	EXBIND0(close);
	EXBIND2(disconnect_peer, int, bool);
	EXBIND0RC(int, get_unique_id);
	EXBIND0RC(ConnectionStatus, get_connection_status);
};
