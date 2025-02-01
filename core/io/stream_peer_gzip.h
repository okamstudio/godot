/**************************************************************************/
/*  stream_peer_gzip.h                                                    */
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

#include "core/core_bind.h"
#include "core/io/compression.h"
#include "core/templates/ring_buffer.h"

class StreamPeerGZIP : public StreamPeer {
	GDCLASS(StreamPeerGZIP, StreamPeer);

private:
	void *ctx = nullptr; // Will hold our z_stream instance.
	bool compressing = true;

	RingBuffer<uint8_t> rb;
	Vector<uint8_t> buffer;

	Error _process(uint8_t *p_dst, int p_dst_size, const uint8_t *p_src, int p_src_size, int &r_consumed, int &r_out, bool p_close = false);
	void _close();
	Error _start(bool p_compress, bool p_is_deflate, int buffer_size = 65535);

protected:
	static void _bind_methods();

public:
	Error start_compression(bool p_is_deflate, int buffer_size = 65535);
	Error start_decompression(bool p_is_deflate, int buffer_size = 65535);

	Error finish();
	void clear();

	virtual Error put_data(const uint8_t *p_data, int p_bytes) override;
	virtual Error put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) override;

	virtual Error get_data(uint8_t *p_buffer, int p_bytes) override;
	virtual Error get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) override;

	virtual int get_available_bytes() const override;

	StreamPeerGZIP();
	~StreamPeerGZIP();
};
