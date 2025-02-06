/**************************************************************************/
/*  packet_buffer.h                                                       */
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

#include "core/templates/ring_buffer.h"

template <typename T>
class PacketBuffer {
private:
	typedef struct {
		uint32_t size;
		T info;
	} _Packet;

	Vector<_Packet> _packets;
	int _queued = 0;
	int _write_pos = 0;
	int _read_pos = 0;
	RingBuffer<uint8_t> _payload;

public:
	Error write_packet(const uint8_t *p_payload, uint32_t p_size, const T *p_info) {
		ERR_FAIL_COND_V_MSG(p_payload && (uint32_t)_payload.space_left() < p_size, ERR_OUT_OF_MEMORY, "Buffer payload full! Dropping data.");
		ERR_FAIL_COND_V_MSG(p_info && _queued >= _packets.size(), ERR_OUT_OF_MEMORY, "Too many packets in queue! Dropping data.");

		// If p_info is nullptr, only the payload is written
		if (p_info) {
			ERR_FAIL_COND_V(_write_pos > _packets.size(), ERR_OUT_OF_MEMORY);
			_Packet p;
			p.size = p_size;
			p.info = *p_info;
			_packets.write[_write_pos] = p;
			_queued += 1;
			_write_pos++;
			if (_write_pos >= _packets.size()) {
				_write_pos = 0;
			}
		}

		// If p_payload is nullptr, only the packet information is written.
		if (p_payload) {
			_payload.write((const uint8_t *)p_payload, p_size);
		}

		return OK;
	}

	Error read_packet(uint8_t *r_payload, int p_bytes, T *r_info, int &r_read) {
		ERR_FAIL_COND_V(_queued < 1, ERR_UNAVAILABLE);
		_Packet p = _packets[_read_pos];
		_read_pos += 1;
		if (_read_pos >= _packets.size()) {
			_read_pos = 0;
		}
		_queued -= 1;

		ERR_FAIL_COND_V(_payload.data_left() < (int)p.size, ERR_BUG);
		ERR_FAIL_COND_V(p_bytes < (int)p.size, ERR_OUT_OF_MEMORY);

		r_read = p.size;
		memcpy(r_info, &p.info, sizeof(T));
		_payload.read(r_payload, p.size);
		return OK;
	}

	void resize(int p_buf_shift, int p_max_packets) {
		_payload.resize(p_buf_shift);
		_packets.resize(p_max_packets);
		_read_pos = 0;
		_write_pos = 0;
		_queued = 0;
	}

	int packets_left() const {
		return _queued;
	}

	int payload_space_left() const {
		return _payload.space_left();
	}

	int packets_space_left() const {
		return _packets.size() - _queued;
	}

	void clear() {
		_payload.resize(0);
		_packets.resize(0);
		_read_pos = 0;
		_write_pos = 0;
		_queued = 0;
	}

	PacketBuffer() {
		clear();
	}

	~PacketBuffer() {
		clear();
	}
};
