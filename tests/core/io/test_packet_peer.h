/**************************************************************************/
/*  test_packet_peer.h                                                    */
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
#include "tests/test_macros.h"

namespace TestPacketPeer {

TEST_CASE("[PacketPeer][PacketPeerStream] Encode buffer max size") {
	Ref<PacketPeerStream> pps;
	pps.instantiate();

	SUBCASE("Default value") {
		CHECK_EQ(pps->get_encode_buffer_max_size(), 8 * 1024 * 1024);
	}

	SUBCASE("Max encode buffer must be at least 1024 bytes") {
		ERR_PRINT_OFF;
		pps->set_encode_buffer_max_size(42);
		ERR_PRINT_ON;

		CHECK_EQ(pps->get_encode_buffer_max_size(), 8 * 1024 * 1024);
	}

	SUBCASE("Max encode buffer cannot exceed 256 MiB") {
		ERR_PRINT_OFF;
		pps->set_encode_buffer_max_size((256 * 1024 * 1024) + 42);
		ERR_PRINT_ON;

		CHECK_EQ(pps->get_encode_buffer_max_size(), 8 * 1024 * 1024);
	}

	SUBCASE("Should be next power of two") {
		pps->set_encode_buffer_max_size(2000);

		CHECK_EQ(pps->get_encode_buffer_max_size(), 2048);
	}
}

TEST_CASE("[PacketPeer][PacketPeerStream] Read a variant from peer") {
	String godot_rules = "Godot Rules!!!";

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->put_var(godot_rules);
	spb->seek(0);

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	Variant value;
	CHECK_EQ(pps->get_var(value), Error::OK);
	CHECK_EQ(String(value), godot_rules);
}

TEST_CASE("[PacketPeer][PacketPeerStream] Read a variant from peer fails") {
	Ref<PacketPeerStream> pps;
	pps.instantiate();

	Variant value;
	ERR_PRINT_OFF;
	CHECK_EQ(pps->get_var(value), Error::ERR_UNCONFIGURED);
	ERR_PRINT_ON;
}

TEST_CASE("[PacketPeer][PacketPeerStream] Put a variant to peer") {
	String godot_rules = "Godot Rules!!!";

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	CHECK_EQ(pps->put_var(godot_rules), Error::OK);

	spb->seek(0);
	CHECK_EQ(String(spb->get_var()), godot_rules);
}

TEST_CASE("[PacketPeer][PacketPeerStream] Put a variant to peer out of memory failure") {
	String more_than_1mb = String("*").repeat(1024 + 1);

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);
	pps->set_encode_buffer_max_size(1024);

	ERR_PRINT_OFF;
	CHECK_EQ(pps->put_var(more_than_1mb), Error::ERR_OUT_OF_MEMORY);
	ERR_PRINT_ON;
}

TEST_CASE("[PacketPeer][PacketPeerStream] Get packet buffer") {
	String godot_rules = "Godot Rules!!!";

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	// First 4 bytes are the length of the string.
	CharString cs = godot_rules.ascii();
	Vector<uint8_t> buffer = { (uint8_t)(cs.length() + 1), 0, 0, 0 };
	buffer.resize_zeroed(4 + cs.length() + 1);
	memcpy(buffer.ptrw() + 4, cs.get_data(), cs.length());
	spb->set_data_array(buffer);

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	buffer.clear();
	CHECK_EQ(pps->get_packet_buffer(buffer), Error::OK);

	CHECK_EQ(String(reinterpret_cast<const char *>(buffer.ptr())), godot_rules);
}

TEST_CASE("[PacketPeer][PacketPeerStream] Get packet buffer from an empty peer") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	Vector<uint8_t> buffer;
	ERR_PRINT_OFF;
	CHECK_EQ(pps->get_packet_buffer(buffer), Error::ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	CHECK_EQ(buffer.size(), 0);
}

TEST_CASE("[PacketPeer][PacketPeerStream] Put packet buffer") {
	String godot_rules = "Godot Rules!!!";

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	CHECK_EQ(pps->put_packet_buffer(godot_rules.to_ascii_buffer()), Error::OK);

	spb->seek(0);
	CHECK_EQ(spb->get_string(), godot_rules);
	// First 4 bytes are the length of the string.
	CharString cs = godot_rules.ascii();
	Vector<uint8_t> buffer = { (uint8_t)cs.length(), 0, 0, 0 };
	buffer.resize(4 + cs.length());
	memcpy(buffer.ptrw() + 4, cs.get_data(), cs.length());
	CHECK_EQ(spb->get_data_array(), buffer);
}

TEST_CASE("[PacketPeer][PacketPeerStream] Put packet buffer when is empty") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	Ref<PacketPeerStream> pps;
	pps.instantiate();
	pps->set_stream_peer(spb);

	Vector<uint8_t> buffer;
	CHECK_EQ(pps->put_packet_buffer(buffer), Error::OK);

	CHECK_EQ(spb->get_size(), 0);
}

} // namespace TestPacketPeer
