/**************************************************************************/
/*  packet_peer.cpp                                                       */
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

#include "packet_peer.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"

/* helpers / binders */

void PacketPeer::set_encode_buffer_max_size(int p_max_size) {
	ERR_FAIL_COND_MSG(p_max_size < 1024, "Max encode buffer must be at least 1024 bytes");
	ERR_FAIL_COND_MSG(p_max_size > 256 * 1024 * 1024, "Max encode buffer cannot exceed 256 MiB");
	encode_buffer_max_size = next_power_of_2(p_max_size);
	encode_buffer.clear();
}

int PacketPeer::get_encode_buffer_max_size() const {
	return encode_buffer_max_size;
}

Error PacketPeer::get_packet_buffer(Vector<uint8_t> &r_buffer) {
	const uint8_t *buffer;
	int buffer_size;
	Error err = get_packet(&buffer, buffer_size);
	if (err) {
		return err;
	}

	r_buffer.resize(buffer_size);
	if (buffer_size == 0) {
		return OK;
	}

	uint8_t *w = r_buffer.ptrw();
	for (int i = 0; i < buffer_size; i++) {
		w[i] = buffer[i];
	}

	return OK;
}

Error PacketPeer::put_packet_buffer(const Vector<uint8_t> &p_buffer) {
	int len = p_buffer.size();
	if (len == 0) {
		return OK;
	}

	const uint8_t *r = p_buffer.ptr();
	return put_packet(&r[0], len);
}

Error PacketPeer::get_var(Variant &r_variant, bool p_allow_objects) {
	const uint8_t *buffer;
	int buffer_size;
	Error err = get_packet(&buffer, buffer_size);
	if (err) {
		return err;
	}

	return decode_variant(r_variant, buffer, buffer_size, nullptr, p_allow_objects);
}

Error PacketPeer::put_var(const Variant &p_packet, bool p_full_objects) {
	int len;
	Error err = encode_variant(p_packet, nullptr, len, p_full_objects); // compute len first
	if (err) {
		return err;
	}

	if (len == 0) {
		return OK;
	}

	ERR_FAIL_COND_V_MSG(len > encode_buffer_max_size, ERR_OUT_OF_MEMORY, "Failed to encode variant, encode size is bigger then encode_buffer_max_size. Consider raising it via 'set_encode_buffer_max_size'.");

	if (encode_buffer.size() < len) [[unlikely]] {
		encode_buffer.resize(0); // Avoid realloc
		encode_buffer.resize(next_power_of_2(len));
	}

	uint8_t *w = encode_buffer.ptrw();
	err = encode_variant(p_packet, w, len, p_full_objects);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to encode Variant.");

	return put_packet(w, len);
}

Variant PacketPeer::_bnd_get_var(bool p_allow_objects) {
	Variant var;
	Error err = get_var(var, p_allow_objects);

	ERR_FAIL_COND_V(err != OK, Variant());
	return var;
}

Error PacketPeer::_put_packet(const Vector<uint8_t> &p_buffer) {
	return put_packet_buffer(p_buffer);
}

Vector<uint8_t> PacketPeer::_get_packet() {
	Vector<uint8_t> raw;
	last_get_error = get_packet_buffer(raw);
	return raw;
}

Error PacketPeer::_get_packet_error() const {
	return last_get_error;
}

void PacketPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_var", "allow_objects"), &PacketPeer::_bnd_get_var, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("put_var", "var", "full_objects"), &PacketPeer::put_var, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_packet"), &PacketPeer::_get_packet);
	ClassDB::bind_method(D_METHOD("put_packet", "buffer"), &PacketPeer::_put_packet);
	ClassDB::bind_method(D_METHOD("get_packet_error"), &PacketPeer::_get_packet_error);
	ClassDB::bind_method(D_METHOD("get_available_packet_count"), &PacketPeer::get_available_packet_count);

	ClassDB::bind_method(D_METHOD("get_encode_buffer_max_size"), &PacketPeer::get_encode_buffer_max_size);
	ClassDB::bind_method(D_METHOD("set_encode_buffer_max_size", "max_size"), &PacketPeer::set_encode_buffer_max_size);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "encode_buffer_max_size"), "set_encode_buffer_max_size", "get_encode_buffer_max_size");
}

/***************/

Error PacketPeerExtension::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	Error err;
	if (GDVIRTUAL_CALL(_get_packet, r_buffer, &r_buffer_size, err)) {
		return err;
	}
	WARN_PRINT_ONCE("PacketPeerExtension::_get_packet_native is unimplemented!");
	return FAILED;
}

Error PacketPeerExtension::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	Error err;
	if (GDVIRTUAL_CALL(_put_packet, p_buffer, p_buffer_size, err)) {
		return err;
	}
	WARN_PRINT_ONCE("PacketPeerExtension::_put_packet_native is unimplemented!");
	return FAILED;
}

void PacketPeerExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_packet, "r_buffer", "r_buffer_size");
	GDVIRTUAL_BIND(_put_packet, "p_buffer", "p_buffer_size");
	GDVIRTUAL_BIND(_get_available_packet_count);
	GDVIRTUAL_BIND(_get_max_packet_size);
}

/***************/

void PacketPeerStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream_peer", "peer"), &PacketPeerStream::set_stream_peer);
	ClassDB::bind_method(D_METHOD("get_stream_peer"), &PacketPeerStream::get_stream_peer);
	ClassDB::bind_method(D_METHOD("set_input_buffer_max_size", "max_size_bytes"), &PacketPeerStream::set_input_buffer_max_size);
	ClassDB::bind_method(D_METHOD("set_output_buffer_max_size", "max_size_bytes"), &PacketPeerStream::set_output_buffer_max_size);
	ClassDB::bind_method(D_METHOD("get_input_buffer_max_size"), &PacketPeerStream::get_input_buffer_max_size);
	ClassDB::bind_method(D_METHOD("get_output_buffer_max_size"), &PacketPeerStream::get_output_buffer_max_size);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "input_buffer_max_size"), "set_input_buffer_max_size", "get_input_buffer_max_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "output_buffer_max_size"), "set_output_buffer_max_size", "get_output_buffer_max_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream_peer", PROPERTY_HINT_RESOURCE_TYPE, "StreamPeer", PROPERTY_USAGE_NONE), "set_stream_peer", "get_stream_peer");
}

Error PacketPeerStream::_poll_buffer() const {
	ERR_FAIL_COND_V(peer.is_null(), ERR_UNCONFIGURED);

	int read = 0;
	ERR_FAIL_COND_V(input_buffer.size() < ring_buffer.space_left(), ERR_UNAVAILABLE);
	Error err = peer->get_partial_data(input_buffer.ptrw(), ring_buffer.space_left(), read);
	if (err) {
		return err;
	}
	if (read == 0) {
		return OK;
	}

	int w = ring_buffer.write(&input_buffer[0], read);
	ERR_FAIL_COND_V(w != read, ERR_BUG);

	return OK;
}

int PacketPeerStream::get_available_packet_count() const {
	_poll_buffer();

	uint32_t remaining = ring_buffer.data_left();

	int ofs = 0;
	int count = 0;

	while (remaining >= 4) {
		uint8_t lbuf[4];
		ring_buffer.copy(lbuf, ofs, 4);
		uint32_t len = decode_uint32(lbuf);
		remaining -= 4;
		ofs += 4;
		if (len > remaining) {
			break;
		}
		remaining -= len;
		ofs += len;
		count++;
	}

	return count;
}

Error PacketPeerStream::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	ERR_FAIL_COND_V(peer.is_null(), ERR_UNCONFIGURED);
	_poll_buffer();

	int remaining = ring_buffer.data_left();
	ERR_FAIL_COND_V(remaining < 4, ERR_UNAVAILABLE);
	uint8_t lbuf[4];
	ring_buffer.copy(lbuf, 0, 4);
	remaining -= 4;
	uint32_t len = decode_uint32(lbuf);
	ERR_FAIL_COND_V(remaining < (int)len, ERR_UNAVAILABLE);

	ERR_FAIL_COND_V(input_buffer.size() < (int)len, ERR_UNAVAILABLE);
	ring_buffer.read(lbuf, 4); //get rid of first 4 bytes
	ring_buffer.read(input_buffer.ptrw(), len); // read packet

	*r_buffer = &input_buffer[0];
	r_buffer_size = len;
	return OK;
}

Error PacketPeerStream::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(peer.is_null(), ERR_UNCONFIGURED);
	Error err = _poll_buffer(); //won't hurt to poll here too

	if (err) {
		return err;
	}

	if (p_buffer_size == 0) {
		return OK;
	}

	ERR_FAIL_COND_V(p_buffer_size < 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_buffer_size + 4 > output_buffer.size(), ERR_INVALID_PARAMETER);

	encode_uint32(p_buffer_size, output_buffer.ptrw());
	uint8_t *dst = &output_buffer.write[4];
	for (int i = 0; i < p_buffer_size; i++) {
		dst[i] = p_buffer[i];
	}

	return peer->put_data(&output_buffer[0], p_buffer_size + 4);
}

int PacketPeerStream::get_max_packet_size() const {
	return output_buffer.size();
}

void PacketPeerStream::set_stream_peer(const Ref<StreamPeer> &p_peer) {
	if (p_peer.ptr() != peer.ptr()) {
		ring_buffer.advance_read(ring_buffer.data_left()); // Reset the ring buffer.
	}

	peer = p_peer;
}

Ref<StreamPeer> PacketPeerStream::get_stream_peer() const {
	return peer;
}

void PacketPeerStream::set_input_buffer_max_size(int p_max_size) {
	ERR_FAIL_COND_MSG(p_max_size < 0, "Max size of input buffer size cannot be smaller than 0.");
	// WARNING: May lose packets.
	ERR_FAIL_COND_MSG(ring_buffer.data_left(), "Buffer in use, resizing would cause loss of data.");
	ring_buffer.resize(nearest_shift(next_power_of_2(p_max_size + 4)) - 1);
	input_buffer.resize(next_power_of_2(p_max_size + 4));
}

int PacketPeerStream::get_input_buffer_max_size() const {
	return input_buffer.size() - 4;
}

void PacketPeerStream::set_output_buffer_max_size(int p_max_size) {
	output_buffer.resize(next_power_of_2(p_max_size + 4));
}

int PacketPeerStream::get_output_buffer_max_size() const {
	return output_buffer.size() - 4;
}

PacketPeerStream::PacketPeerStream() {
	int64_t rbsize = GLOBAL_GET("network/limits/packet_peer_stream/max_buffer_po2");

	ring_buffer.resize(rbsize);
	input_buffer.resize(int64_t(1) << rbsize);
	output_buffer.resize(int64_t(1) << rbsize);
}
