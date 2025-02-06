/**************************************************************************/
/*  packet_peer.h                                                         */
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

#include "core/io/stream_peer.h"
#include "core/object/class_db.h"
#include "core/templates/ring_buffer.h"

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/gdvirtual.gen.inc"
#include "core/variant/native_ptr.h"

class PacketPeer : public RefCounted {
	GDCLASS(PacketPeer, RefCounted);

	Variant _bnd_get_var(bool p_allow_objects = false);

	static void _bind_methods();

	Error _put_packet(const Vector<uint8_t> &p_buffer);
	Vector<uint8_t> _get_packet();
	Error _get_packet_error() const;

	mutable Error last_get_error = OK;

	int encode_buffer_max_size = 8 * 1024 * 1024;
	Vector<uint8_t> encode_buffer;

public:
	virtual int get_available_packet_count() const = 0;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) = 0; ///< buffer is GONE after next get_packet
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) = 0;

	virtual int get_max_packet_size() const = 0;

	/* helpers / binders */

	virtual Error get_packet_buffer(Vector<uint8_t> &r_buffer);
	virtual Error put_packet_buffer(const Vector<uint8_t> &p_buffer);

	virtual Error get_var(Variant &r_variant, bool p_allow_objects = false);
	virtual Error put_var(const Variant &p_packet, bool p_full_objects = false);

	void set_encode_buffer_max_size(int p_max_size);
	int get_encode_buffer_max_size() const;

	PacketPeer() {}
	~PacketPeer() {}
};

class PacketPeerExtension : public PacketPeer {
	GDCLASS(PacketPeerExtension, PacketPeer);

protected:
	static void _bind_methods();

public:
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override; ///< buffer is GONE after next get_packet
	GDVIRTUAL2R(Error, _get_packet, GDExtensionConstPtr<const uint8_t *>, GDExtensionPtr<int>);

	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	GDVIRTUAL2R(Error, _put_packet, GDExtensionConstPtr<const uint8_t>, int);

	EXBIND0RC(int, get_available_packet_count);
	EXBIND0RC(int, get_max_packet_size);
};

class PacketPeerStream : public PacketPeer {
	GDCLASS(PacketPeerStream, PacketPeer);

	//the way the buffers work sucks, will change later

	mutable Ref<StreamPeer> peer;
	mutable RingBuffer<uint8_t> ring_buffer;
	mutable Vector<uint8_t> input_buffer;
	mutable Vector<uint8_t> output_buffer;

	Error _poll_buffer() const;

protected:
	static void _bind_methods();

public:
	virtual int get_available_packet_count() const override;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

	virtual int get_max_packet_size() const override;

	void set_stream_peer(const Ref<StreamPeer> &p_peer);
	Ref<StreamPeer> get_stream_peer() const;
	void set_input_buffer_max_size(int p_max_size);
	int get_input_buffer_max_size() const;
	void set_output_buffer_max_size(int p_max_size);
	int get_output_buffer_max_size() const;
	PacketPeerStream();
};
