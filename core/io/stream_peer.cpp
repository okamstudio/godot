/**************************************************************************/
/*  stream_peer.cpp                                                       */
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

#include "stream_peer.h"

#include "core/io/marshalls.h"

Error StreamPeer::_put_data(const Vector<uint8_t> &p_data) {
	int len = p_data.size();
	if (len == 0) {
		return OK;
	}
	const uint8_t *r = p_data.ptr();
	return put_data(&r[0], len);
}

Array StreamPeer::_put_partial_data(const Vector<uint8_t> &p_data) {
	Array ret;

	int len = p_data.size();
	if (len == 0) {
		ret.push_back(OK);
		ret.push_back(0);
		return ret;
	}

	const uint8_t *r = p_data.ptr();
	int sent;
	Error err = put_partial_data(&r[0], len, sent);

	if (err != OK) {
		sent = 0;
	}
	ret.push_back(err);
	ret.push_back(sent);
	return ret;
}

Array StreamPeer::_get_data(int p_bytes) {
	Array ret;

	Vector<uint8_t> data;
	data.resize(p_bytes);
	if (data.size() != p_bytes) {
		ret.push_back(ERR_OUT_OF_MEMORY);
		ret.push_back(Vector<uint8_t>());
		return ret;
	}

	uint8_t *w = data.ptrw();
	Error err = get_data(&w[0], p_bytes);

	ret.push_back(err);
	ret.push_back(data);
	return ret;
}

Array StreamPeer::_get_partial_data(int p_bytes) {
	Array ret;

	Vector<uint8_t> data;
	data.resize(p_bytes);
	if (data.size() != p_bytes) {
		ret.push_back(ERR_OUT_OF_MEMORY);
		ret.push_back(Vector<uint8_t>());
		return ret;
	}

	uint8_t *w = data.ptrw();
	int received;
	Error err = get_partial_data(&w[0], p_bytes, received);

	if (err != OK) {
		data.clear();
	} else if (received != data.size()) {
		data.resize(received);
	}

	ret.push_back(err);
	ret.push_back(data);
	return ret;
}

void StreamPeer::set_big_endian(bool p_big_endian) {
	big_endian = p_big_endian;
}

bool StreamPeer::is_big_endian_enabled() const {
	return big_endian;
}

void StreamPeer::put_u8(uint8_t p_val) {
	put_data((const uint8_t *)&p_val, 1);
}

void StreamPeer::put_8(int8_t p_val) {
	put_data((const uint8_t *)&p_val, 1);
}

void StreamPeer::put_u16(uint16_t p_val) {
	if (big_endian) {
		p_val = BSWAP16(p_val);
	}
	uint8_t buf[2];
	encode_uint16(p_val, buf);
	put_data(buf, 2);
}

void StreamPeer::put_16(int16_t p_val) {
	if (big_endian) {
		p_val = BSWAP16(p_val);
	}
	uint8_t buf[2];
	encode_uint16(p_val, buf);
	put_data(buf, 2);
}

void StreamPeer::put_u32(uint32_t p_val) {
	if (big_endian) {
		p_val = BSWAP32(p_val);
	}
	uint8_t buf[4];
	encode_uint32(p_val, buf);
	put_data(buf, 4);
}

void StreamPeer::put_32(int32_t p_val) {
	if (big_endian) {
		p_val = BSWAP32(p_val);
	}
	uint8_t buf[4];
	encode_uint32(p_val, buf);
	put_data(buf, 4);
}

void StreamPeer::put_u64(uint64_t p_val) {
	if (big_endian) {
		p_val = BSWAP64(p_val);
	}
	uint8_t buf[8];
	encode_uint64(p_val, buf);
	put_data(buf, 8);
}

void StreamPeer::put_64(int64_t p_val) {
	if (big_endian) {
		p_val = BSWAP64(p_val);
	}
	uint8_t buf[8];
	encode_uint64(p_val, buf);
	put_data(buf, 8);
}

void StreamPeer::put_half(float p_val) {
	uint8_t buf[2];

	encode_half(p_val, buf);
	uint16_t *p16 = (uint16_t *)buf;
	if (big_endian) {
		*p16 = BSWAP16(*p16);
	}

	put_data(buf, 2);
}

void StreamPeer::put_float(float p_val) {
	uint8_t buf[4];

	encode_float(p_val, buf);
	if (big_endian) {
		uint32_t *p32 = (uint32_t *)buf;
		*p32 = BSWAP32(*p32);
	}

	put_data(buf, 4);
}

void StreamPeer::put_double(double p_val) {
	uint8_t buf[8];
	encode_double(p_val, buf);
	if (big_endian) {
		uint64_t *p64 = (uint64_t *)buf;
		*p64 = BSWAP64(*p64);
	}
	put_data(buf, 8);
}

void StreamPeer::put_string(const String &p_string) {
	CharString cs = p_string.ascii();
	put_u32(cs.length());
	put_data((const uint8_t *)cs.get_data(), cs.length());
}

void StreamPeer::put_utf8_string(const String &p_string) {
	CharString cs = p_string.utf8();
	put_u32(cs.length());
	put_data((const uint8_t *)cs.get_data(), cs.length());
}

void StreamPeer::put_var(const Variant &p_variant, bool p_full_objects) {
	int len = 0;
	Vector<uint8_t> buf;
	encode_variant(p_variant, nullptr, len, p_full_objects);
	buf.resize(len);
	put_32(len);
	encode_variant(p_variant, buf.ptrw(), len, p_full_objects);
	put_data(buf.ptr(), buf.size());
}

uint8_t StreamPeer::get_u8() {
	uint8_t buf[1] = {};
	get_data(buf, 1);
	return buf[0];
}

int8_t StreamPeer::get_8() {
	uint8_t buf[1] = {};
	get_data(buf, 1);
	return buf[0];
}

uint16_t StreamPeer::get_u16() {
	uint8_t buf[2];
	get_data(buf, 2);
	uint16_t r = decode_uint16(buf);
	if (big_endian) {
		r = BSWAP16(r);
	}
	return r;
}

int16_t StreamPeer::get_16() {
	uint8_t buf[2];
	get_data(buf, 2);
	uint16_t r = decode_uint16(buf);
	if (big_endian) {
		r = BSWAP16(r);
	}
	return r;
}

uint32_t StreamPeer::get_u32() {
	uint8_t buf[4];
	get_data(buf, 4);
	uint32_t r = decode_uint32(buf);
	if (big_endian) {
		r = BSWAP32(r);
	}
	return r;
}

int32_t StreamPeer::get_32() {
	uint8_t buf[4];
	get_data(buf, 4);
	uint32_t r = decode_uint32(buf);
	if (big_endian) {
		r = BSWAP32(r);
	}
	return r;
}

uint64_t StreamPeer::get_u64() {
	uint8_t buf[8];
	get_data(buf, 8);
	uint64_t r = decode_uint64(buf);
	if (big_endian) {
		r = BSWAP64(r);
	}
	return r;
}

int64_t StreamPeer::get_64() {
	uint8_t buf[8];
	get_data(buf, 8);
	uint64_t r = decode_uint64(buf);
	if (big_endian) {
		r = BSWAP64(r);
	}
	return r;
}

float StreamPeer::get_half() {
	uint8_t buf[2];
	get_data(buf, 2);

	uint16_t *p16 = (uint16_t *)buf;
	if (big_endian) {
		*p16 = BSWAP16(*p16);
	}

	return decode_half(buf);
}

float StreamPeer::get_float() {
	uint8_t buf[4];
	get_data(buf, 4);

	if (big_endian) {
		uint32_t *p32 = (uint32_t *)buf;
		*p32 = BSWAP32(*p32);
	}

	return decode_float(buf);
}

double StreamPeer::get_double() {
	uint8_t buf[8];
	get_data(buf, 8);

	if (big_endian) {
		uint64_t *p64 = (uint64_t *)buf;
		*p64 = BSWAP64(*p64);
	}

	return decode_double(buf);
}

String StreamPeer::get_string(int p_bytes) {
	if (p_bytes < 0) {
		p_bytes = get_u32();
	}
	ERR_FAIL_COND_V(p_bytes < 0, String());

	Vector<char> buf;
	Error err = buf.resize(p_bytes + 1);
	ERR_FAIL_COND_V(err != OK, String());
	err = get_data((uint8_t *)&buf[0], p_bytes);
	ERR_FAIL_COND_V(err != OK, String());
	buf.write[p_bytes] = 0;
	return buf.ptr();
}

String StreamPeer::get_utf8_string(int p_bytes) {
	if (p_bytes < 0) {
		p_bytes = get_u32();
	}
	ERR_FAIL_COND_V(p_bytes < 0, String());

	Vector<uint8_t> buf;
	Error err = buf.resize(p_bytes);
	ERR_FAIL_COND_V(err != OK, String());
	err = get_data(buf.ptrw(), p_bytes);
	ERR_FAIL_COND_V(err != OK, String());

	String ret;
	ret.parse_utf8((const char *)buf.ptr(), buf.size());
	return ret;
}

Variant StreamPeer::get_var(bool p_allow_objects) {
	int len = get_32();
	Vector<uint8_t> var;
	Error err = var.resize(len);
	ERR_FAIL_COND_V(err != OK, Variant());
	err = get_data(var.ptrw(), len);
	ERR_FAIL_COND_V(err != OK, Variant());

	Variant ret;
	err = decode_variant(ret, var.ptr(), len, nullptr, p_allow_objects);
	ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to decode Variant.");

	return ret;
}

void StreamPeer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("put_data", "data"), &StreamPeer::_put_data);
	ClassDB::bind_method(D_METHOD("put_partial_data", "data"), &StreamPeer::_put_partial_data);

	ClassDB::bind_method(D_METHOD("get_data", "bytes"), &StreamPeer::_get_data);
	ClassDB::bind_method(D_METHOD("get_partial_data", "bytes"), &StreamPeer::_get_partial_data);

	ClassDB::bind_method(D_METHOD("get_available_bytes"), &StreamPeer::get_available_bytes);

	ClassDB::bind_method(D_METHOD("set_big_endian", "enable"), &StreamPeer::set_big_endian);
	ClassDB::bind_method(D_METHOD("is_big_endian_enabled"), &StreamPeer::is_big_endian_enabled);

	ClassDB::bind_method(D_METHOD("put_8", "value"), &StreamPeer::put_8);
	ClassDB::bind_method(D_METHOD("put_u8", "value"), &StreamPeer::put_u8);
	ClassDB::bind_method(D_METHOD("put_16", "value"), &StreamPeer::put_16);
	ClassDB::bind_method(D_METHOD("put_u16", "value"), &StreamPeer::put_u16);
	ClassDB::bind_method(D_METHOD("put_32", "value"), &StreamPeer::put_32);
	ClassDB::bind_method(D_METHOD("put_u32", "value"), &StreamPeer::put_u32);
	ClassDB::bind_method(D_METHOD("put_64", "value"), &StreamPeer::put_64);
	ClassDB::bind_method(D_METHOD("put_u64", "value"), &StreamPeer::put_u64);
	ClassDB::bind_method(D_METHOD("put_half", "value"), &StreamPeer::put_half);
	ClassDB::bind_method(D_METHOD("put_float", "value"), &StreamPeer::put_float);
	ClassDB::bind_method(D_METHOD("put_double", "value"), &StreamPeer::put_double);
	ClassDB::bind_method(D_METHOD("put_string", "value"), &StreamPeer::put_string);
	ClassDB::bind_method(D_METHOD("put_utf8_string", "value"), &StreamPeer::put_utf8_string);
	ClassDB::bind_method(D_METHOD("put_var", "value", "full_objects"), &StreamPeer::put_var, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("get_8"), &StreamPeer::get_8);
	ClassDB::bind_method(D_METHOD("get_u8"), &StreamPeer::get_u8);
	ClassDB::bind_method(D_METHOD("get_16"), &StreamPeer::get_16);
	ClassDB::bind_method(D_METHOD("get_u16"), &StreamPeer::get_u16);
	ClassDB::bind_method(D_METHOD("get_32"), &StreamPeer::get_32);
	ClassDB::bind_method(D_METHOD("get_u32"), &StreamPeer::get_u32);
	ClassDB::bind_method(D_METHOD("get_64"), &StreamPeer::get_64);
	ClassDB::bind_method(D_METHOD("get_u64"), &StreamPeer::get_u64);
	ClassDB::bind_method(D_METHOD("get_half"), &StreamPeer::get_half);
	ClassDB::bind_method(D_METHOD("get_float"), &StreamPeer::get_float);
	ClassDB::bind_method(D_METHOD("get_double"), &StreamPeer::get_double);
	ClassDB::bind_method(D_METHOD("get_string", "bytes"), &StreamPeer::get_string, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_utf8_string", "bytes"), &StreamPeer::get_utf8_string, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_var", "allow_objects"), &StreamPeer::get_var, DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "big_endian"), "set_big_endian", "is_big_endian_enabled");
}

////////////////////////////////

Error StreamPeerExtension::get_data(uint8_t *r_buffer, int p_bytes) {
	Error err;
	int received = 0;
	if (GDVIRTUAL_CALL(_get_data, r_buffer, p_bytes, &received, err)) {
		return err;
	}
	WARN_PRINT_ONCE("StreamPeerExtension::_get_data is unimplemented!");
	return FAILED;
}

Error StreamPeerExtension::get_partial_data(uint8_t *r_buffer, int p_bytes, int &r_received) {
	Error err;
	if (GDVIRTUAL_CALL(_get_partial_data, r_buffer, p_bytes, &r_received, err)) {
		return err;
	}
	WARN_PRINT_ONCE("StreamPeerExtension::_get_partial_data is unimplemented!");
	return FAILED;
}

Error StreamPeerExtension::put_data(const uint8_t *p_data, int p_bytes) {
	Error err;
	int sent = 0;
	if (GDVIRTUAL_CALL(_put_data, p_data, p_bytes, &sent, err)) {
		return err;
	}
	WARN_PRINT_ONCE("StreamPeerExtension::_put_data is unimplemented!");
	return FAILED;
}

Error StreamPeerExtension::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	Error err;
	if (GDVIRTUAL_CALL(_put_partial_data, p_data, p_bytes, &r_sent, err)) {
		return err;
	}
	WARN_PRINT_ONCE("StreamPeerExtension::_put_partial_data is unimplemented!");
	return FAILED;
}

void StreamPeerExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_data, "r_buffer", "r_bytes", "r_received");
	GDVIRTUAL_BIND(_get_partial_data, "r_buffer", "r_bytes", "r_received");
	GDVIRTUAL_BIND(_put_data, "p_data", "p_bytes", "r_sent");
	GDVIRTUAL_BIND(_put_partial_data, "p_data", "p_bytes", "r_sent");
	GDVIRTUAL_BIND(_get_available_bytes);
}

////////////////////////////////

void StreamPeerBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("seek", "position"), &StreamPeerBuffer::seek);
	ClassDB::bind_method(D_METHOD("get_size"), &StreamPeerBuffer::get_size);
	ClassDB::bind_method(D_METHOD("get_position"), &StreamPeerBuffer::get_position);
	ClassDB::bind_method(D_METHOD("resize", "size"), &StreamPeerBuffer::resize);
	ClassDB::bind_method(D_METHOD("set_data_array", "data"), &StreamPeerBuffer::set_data_array);
	ClassDB::bind_method(D_METHOD("get_data_array"), &StreamPeerBuffer::get_data_array);
	ClassDB::bind_method(D_METHOD("clear"), &StreamPeerBuffer::clear);
	ClassDB::bind_method(D_METHOD("duplicate"), &StreamPeerBuffer::duplicate);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data_array"), "set_data_array", "get_data_array");
}

Error StreamPeerBuffer::put_data(const uint8_t *p_data, int p_bytes) {
	if (p_bytes <= 0) {
		return OK;
	}

	if (pointer + p_bytes > data.size()) {
		data.resize(pointer + p_bytes);
	}

	uint8_t *w = data.ptrw();
	memcpy(&w[pointer], p_data, p_bytes);

	pointer += p_bytes;
	return OK;
}

Error StreamPeerBuffer::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	r_sent = p_bytes;
	return put_data(p_data, p_bytes);
}

Error StreamPeerBuffer::get_data(uint8_t *p_buffer, int p_bytes) {
	int recv;
	get_partial_data(p_buffer, p_bytes, recv);
	if (recv != p_bytes) {
		return ERR_INVALID_PARAMETER;
	}

	return OK;
}

Error StreamPeerBuffer::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	if (pointer + p_bytes > data.size()) {
		r_received = data.size() - pointer;
		if (r_received <= 0) {
			r_received = 0;
			return OK; //you got 0
		}
	} else {
		r_received = p_bytes;
	}

	const uint8_t *r = data.ptr();
	memcpy(p_buffer, r + pointer, r_received);

	pointer += r_received;
	// FIXME: return what? OK or ERR_*
	// return OK for now so we don't maybe return garbage
	return OK;
}

int StreamPeerBuffer::get_available_bytes() const {
	return data.size() - pointer;
}

void StreamPeerBuffer::seek(int p_pos) {
	ERR_FAIL_COND(p_pos < 0);
	ERR_FAIL_COND(p_pos > data.size());
	pointer = p_pos;
}

int StreamPeerBuffer::get_size() const {
	return data.size();
}

int StreamPeerBuffer::get_position() const {
	return pointer;
}

void StreamPeerBuffer::resize(int p_size) {
	data.resize(p_size);
}

void StreamPeerBuffer::set_data_array(const Vector<uint8_t> &p_data) {
	data = p_data;
	pointer = 0;
}

Vector<uint8_t> StreamPeerBuffer::get_data_array() const {
	return data;
}

void StreamPeerBuffer::clear() {
	data.clear();
	pointer = 0;
}

Ref<StreamPeerBuffer> StreamPeerBuffer::duplicate() const {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->data = data;
	return spb;
}
