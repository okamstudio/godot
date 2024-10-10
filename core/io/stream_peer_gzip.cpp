/**************************************************************************/
/*  stream_peer_gzip.cpp                                                  */
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

#include "core/io/stream_peer_gzip.h"

#include "core/io/zip_io.h"
#include <zlib.h>

void StreamPeerGZIP::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_compression", "use_deflate", "buffer_size"), &StreamPeerGZIP::start_compression, DEFVAL(false), DEFVAL(65535));
	ClassDB::bind_method(D_METHOD("start_decompression", "use_deflate", "buffer_size"), &StreamPeerGZIP::start_decompression, DEFVAL(false), DEFVAL(65535));
	ClassDB::bind_method(D_METHOD("finish"), &StreamPeerGZIP::finish);
	ClassDB::bind_method(D_METHOD("clear"), &StreamPeerGZIP::clear);
}

StreamPeerGZIP::StreamPeerGZIP() {
}

StreamPeerGZIP::~StreamPeerGZIP() {
	_close();
}

void StreamPeerGZIP::_close() {
	if (ctx) {
		z_stream *strm = (z_stream *)ctx;
		if (compressing) {
			deflateEnd(strm);
		} else {
			inflateEnd(strm);
		}
		memfree(strm);
		ctx = nullptr;
	}
}

void StreamPeerGZIP::clear() {
	_close();
	rb.clear();
	buffer.clear();
}

Error StreamPeerGZIP::start_compression(bool p_is_deflate, int buffer_size) {
	return _start(true, p_is_deflate, buffer_size);
}

Error StreamPeerGZIP::start_decompression(bool p_is_deflate, int buffer_size) {
	return _start(false, p_is_deflate, buffer_size);
}

Error StreamPeerGZIP::_start(bool p_compress, bool p_is_deflate, int buffer_size) {
	ERR_FAIL_COND_V(ctx != nullptr, ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V_MSG(buffer_size <= 0, ERR_INVALID_PARAMETER, "Invalid buffer size. It should be a positive integer.");
	clear();
	compressing = p_compress;
	rb.resize(nearest_shift(buffer_size - 1));
	buffer.resize(1024);

	// Create ctx.
	ctx = memalloc(sizeof(z_stream));
	z_stream &strm = *(z_stream *)ctx;
	strm.next_in = Z_NULL;
	strm.avail_in = 0;
	strm.zalloc = zipio_alloc;
	strm.zfree = zipio_free;
	strm.opaque = Z_NULL;
	int window_bits = p_is_deflate ? 15 : (15 + 16);
	int err = Z_OK;
	int level = Z_DEFAULT_COMPRESSION;
	if (compressing) {
		err = deflateInit2(&strm, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
	} else {
		err = inflateInit2(&strm, window_bits);
	}
	ERR_FAIL_COND_V(err != Z_OK, FAILED);
	return OK;
}

Error StreamPeerGZIP::_process(uint8_t *p_dst, int p_dst_size, const uint8_t *p_src, int p_src_size, int &r_consumed, int &r_out, bool p_close) {
	ERR_FAIL_NULL_V(ctx, ERR_UNCONFIGURED);
	z_stream &strm = *(z_stream *)ctx;
	strm.avail_in = p_src_size;
	strm.avail_out = p_dst_size;
	strm.next_in = (Bytef *)p_src;
	strm.next_out = (Bytef *)p_dst;
	int flush = p_close ? Z_FINISH : Z_NO_FLUSH;
	if (compressing) {
		int err = deflate(&strm, flush);
		ERR_FAIL_COND_V(err != (p_close ? Z_STREAM_END : Z_OK), FAILED);
	} else {
		int err = inflate(&strm, flush);
		ERR_FAIL_COND_V(err != Z_OK && err != Z_STREAM_END, FAILED);
	}
	r_out = p_dst_size - strm.avail_out;
	r_consumed = p_src_size - strm.avail_in;
	return OK;
}

Error StreamPeerGZIP::put_data(const uint8_t *p_data, int p_bytes) {
	int wrote = 0;
	Error err = put_partial_data(p_data, p_bytes, wrote);
	if (err != OK) {
		return err;
	}
	ERR_FAIL_COND_V(p_bytes != wrote, ERR_OUT_OF_MEMORY);
	return OK;
}

Error StreamPeerGZIP::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	ERR_FAIL_NULL_V(ctx, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);

	// Ensure we have enough space in temporary buffer.
	if (buffer.size() < p_bytes) {
		buffer.resize(p_bytes);
	}

	r_sent = 0;
	while (r_sent < p_bytes && rb.space_left() > 1024) { // Keep the ring buffer size meaningful.
		int sent = 0;
		int to_write = 0;
		// Compress or decompress
		Error err = _process(buffer.ptrw(), MIN(buffer.size(), rb.space_left()), p_data + r_sent, p_bytes - r_sent, sent, to_write);
		if (err != OK) {
			return err;
		}
		// When decompressing, we might need to do another round.
		r_sent += sent;

		// We can't write more than this buffer is full.
		if (sent == 0 && to_write == 0) {
			return OK;
		}
		if (to_write) {
			// Copy to ring buffer.
			int wrote = rb.write(buffer.ptr(), to_write);
			ERR_FAIL_COND_V(wrote != to_write, ERR_BUG);
		}
	}
	return OK;
}

Error StreamPeerGZIP::get_data(uint8_t *p_buffer, int p_bytes) {
	int received = 0;
	Error err = get_partial_data(p_buffer, p_bytes, received);
	if (err != OK) {
		return err;
	}
	ERR_FAIL_COND_V(p_bytes != received, ERR_UNAVAILABLE);
	return OK;
}

Error StreamPeerGZIP::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);

	r_received = MIN(p_bytes, rb.data_left());
	if (r_received == 0) {
		return OK;
	}
	int received = rb.read(p_buffer, r_received);
	ERR_FAIL_COND_V(received != r_received, ERR_BUG);
	return OK;
}

int StreamPeerGZIP::get_available_bytes() const {
	return rb.data_left();
}

Error StreamPeerGZIP::finish() {
	ERR_FAIL_COND_V(!ctx || !compressing, ERR_UNAVAILABLE);
	// Ensure we have enough space in temporary buffer.
	if (buffer.size() < get_available_bytes()) {
		buffer.resize(get_available_bytes()); // get_available_bytes() is what we can store in RingBuffer.
	}
	int consumed = 0;
	int to_write = 0;
	Error err = _process(buffer.ptrw(), buffer.size(), nullptr, 0, consumed, to_write, true); // compress
	if (err != OK) {
		return err;
	}
	int wrote = rb.write(buffer.ptr(), to_write);
	ERR_FAIL_COND_V(wrote != to_write, ERR_OUT_OF_MEMORY);
	return OK;
}
