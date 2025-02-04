/**************************************************************************/
/*  ip_address.cpp                                                        */
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

#include "ip_address.h"
/*
IPAddress::operator Variant() const {
	return operator String();
}*/

#include <string.h>

IPAddress::operator String() const {
	if (wildcard) {
		return "*";
	}

	if (!valid) {
		return "";
	}

	if (is_ipv4()) {
		// IPv4 address mapped to IPv6
		return itos(field8[12]) + "." + itos(field8[13]) + "." + itos(field8[14]) + "." + itos(field8[15]);
	}
	String ret;
	for (int i = 0; i < 8; i++) {
		if (i > 0) {
			ret = ret + ":";
		}
		uint16_t num = (field8[i * 2] << 8) + field8[i * 2 + 1];
		ret = ret + String::num_int64(num, 16);
	}

	return ret;
}

static void _parse_hex(const String &p_string, int p_start, uint8_t *p_dst) {
	uint16_t ret = 0;
	for (int i = p_start; i < p_start + 4; i++) {
		if (i >= p_string.length()) {
			break;
		}

		int n = 0;
		char32_t c = p_string[i];
		if (is_digit(c)) {
			n = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			n = 10 + (c - 'a');
		} else if (c >= 'A' && c <= 'F') {
			n = 10 + (c - 'A');
		} else if (c == ':') {
			break;
		} else {
			ERR_FAIL_MSG("Invalid character in IPv6 address: " + p_string + ".");
		}
		ret = ret << 4;
		ret += n;
	}

	p_dst[0] = ret >> 8;
	p_dst[1] = ret & 0xff;
}

void IPAddress::_parse_ipv6(const String &p_string) {
	static const int parts_total = 8;
	int parts[parts_total] = { 0 };
	int parts_count = 0;
	bool part_found = false;
	bool part_skip = false;
	bool part_ipv4 = false;
	int parts_idx = 0;

	for (int i = 0; i < p_string.length(); i++) {
		char32_t c = p_string[i];
		if (c == ':') {
			if (i == 0) {
				continue; // next must be a ":"
			}
			if (!part_found) {
				part_skip = true;
				parts[parts_idx++] = -1;
			}
			part_found = false;
		} else if (c == '.') {
			part_ipv4 = true;

		} else if (is_hex_digit(c)) {
			if (!part_found) {
				parts[parts_idx++] = i;
				part_found = true;
				++parts_count;
			}
		} else {
			ERR_FAIL_MSG("Invalid character in IPv6 address: " + p_string + ".");
		}
	}

	int parts_extra = 0;
	if (part_skip) {
		parts_extra = parts_total - parts_count;
	}

	int idx = 0;
	for (int i = 0; i < parts_idx; i++) {
		if (parts[i] == -1) {
			for (int j = 0; j < parts_extra; j++) {
				field16[idx++] = 0;
			}
			continue;
		}

		if (part_ipv4 && i == parts_idx - 1) {
			_parse_ipv4(p_string, parts[i], (uint8_t *)&field16[idx]); // should be the last one
		} else {
			_parse_hex(p_string, parts[i], (uint8_t *)&(field16[idx++]));
		}
	}
}

void IPAddress::_parse_ipv4(const String &p_string, int p_start, uint8_t *p_ret) {
	String ip;
	if (p_start != 0) {
		ip = p_string.substr(p_start);
	} else {
		ip = p_string;
	}

	int slices = ip.get_slice_count(".");
	ERR_FAIL_COND_MSG(slices != 4, "Invalid IP address string: " + ip + ".");
	for (int i = 0; i < 4; i++) {
		p_ret[i] = ip.get_slicec('.', i).to_int();
	}
}

void IPAddress::clear() {
	memset(&field8[0], 0, sizeof(field8));
	valid = false;
	wildcard = false;
}

bool IPAddress::is_ipv4() const {
	return (field32[0] == 0 && field32[1] == 0 && field16[4] == 0 && field16[5] == 0xffff);
}

const uint8_t *IPAddress::get_ipv4() const {
	ERR_FAIL_COND_V_MSG(!is_ipv4(), &(field8[12]), "IPv4 requested, but current IP is IPv6."); // Not the correct IPv4 (it's an IPv6), but we don't want to return a null pointer risking an engine crash.
	return &(field8[12]);
}

void IPAddress::set_ipv4(const uint8_t *p_ip) {
	clear();
	valid = true;
	field16[5] = 0xffff;
	field32[3] = *((const uint32_t *)p_ip);
}

const uint8_t *IPAddress::get_ipv6() const {
	return field8;
}

void IPAddress::set_ipv6(const uint8_t *p_buf) {
	clear();
	valid = true;
	for (int i = 0; i < 16; i++) {
		field8[i] = p_buf[i];
	}
}

IPAddress::IPAddress(const String &p_string) {
	clear();

	if (p_string == "*") {
		// Wildcard (not a valid IP)
		wildcard = true;

	} else if (p_string.contains_char(':')) {
		// IPv6
		_parse_ipv6(p_string);
		valid = true;

	} else if (p_string.get_slice_count(".") == 4) {
		// IPv4 (mapped to IPv6 internally)
		field16[5] = 0xffff;
		_parse_ipv4(p_string, 0, &field8[12]);
		valid = true;

	} else {
		ERR_PRINT("Invalid IP address.");
	}
}

_FORCE_INLINE_ static void _32_to_buf(uint8_t *p_dst, uint32_t p_n) {
	p_dst[0] = (p_n >> 24) & 0xff;
	p_dst[1] = (p_n >> 16) & 0xff;
	p_dst[2] = (p_n >> 8) & 0xff;
	p_dst[3] = (p_n >> 0) & 0xff;
}

IPAddress::IPAddress(uint32_t p_a, uint32_t p_b, uint32_t p_c, uint32_t p_d, bool is_v6) {
	clear();
	valid = true;
	if (!is_v6) {
		// Mapped to IPv6
		field16[5] = 0xffff;
		field8[12] = p_a;
		field8[13] = p_b;
		field8[14] = p_c;
		field8[15] = p_d;
	} else {
		_32_to_buf(&field8[0], p_a);
		_32_to_buf(&field8[4], p_b);
		_32_to_buf(&field8[8], p_c);
		_32_to_buf(&field8[12], p_d);
	}
}
