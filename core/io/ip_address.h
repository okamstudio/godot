/**************************************************************************/
/*  ip_address.h                                                          */
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

#include "core/string/ustring.h"

struct IPAddress {
private:
	union {
		uint8_t field8[16];
		uint16_t field16[8];
		uint32_t field32[4];
	};

	bool valid;
	bool wildcard;

protected:
	void _parse_ipv6(const String &p_string);
	void _parse_ipv4(const String &p_string, int p_start, uint8_t *p_ret);

public:
	//operator Variant() const;
	bool operator==(const IPAddress &p_ip) const {
		if (p_ip.valid != valid) {
			return false;
		}
		if (!valid) {
			return false;
		}
		for (int i = 0; i < 4; i++) {
			if (field32[i] != p_ip.field32[i]) {
				return false;
			}
		}
		return true;
	}

	bool operator!=(const IPAddress &p_ip) const {
		if (p_ip.valid != valid) {
			return true;
		}
		if (!valid) {
			return true;
		}
		for (int i = 0; i < 4; i++) {
			if (field32[i] != p_ip.field32[i]) {
				return true;
			}
		}
		return false;
	}

	void clear();
	bool is_wildcard() const { return wildcard; }
	bool is_valid() const { return valid; }
	bool is_ipv4() const;
	const uint8_t *get_ipv4() const;
	void set_ipv4(const uint8_t *p_ip);

	const uint8_t *get_ipv6() const;
	void set_ipv6(const uint8_t *p_buf);

	operator String() const;
	IPAddress(const String &p_string);
	IPAddress(uint32_t p_a, uint32_t p_b, uint32_t p_c, uint32_t p_d, bool is_v6 = false);
	IPAddress() { clear(); }
};
