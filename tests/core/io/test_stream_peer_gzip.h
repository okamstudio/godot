/**************************************************************************/
/*  test_stream_peer_gzip.h                                               */
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

#ifndef TEST_STREAM_PEER_GZIP_H
#define TEST_STREAM_PEER_GZIP_H

#include "core/io/stream_peer_gzip.h"
#include "tests/test_macros.h"

namespace TestStreamPeerGzip {

constexpr int DATA_SIZE = 2000;
constexpr int BUFFER_SIZE = 10000; // Should be meaningfully higher than data size

TEST_CASE("[TestStreamPeerGzip] Start compression") {
	Ref<StreamPeerGZIP> spg;
	spg.instantiate();

	SUBCASE("Normal scenario") {
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->get_available_bytes(), 0);
		spg->clear();
	}

	SUBCASE("Twice in a row") {
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), ERR_ALREADY_IN_USE);
		spg->clear();
	}

	SUBCASE("Invalid buffer size") {
		CHECK_EQ(spg->start_compression(false, -1), ERR_INVALID_PARAMETER);
		spg->clear();
	}
}

TEST_CASE("[TestStreamPeerGzip] Start decompression") {
	Ref<StreamPeerGZIP> spg;
	spg.instantiate();

	SUBCASE("Normal scenario") {
		CHECK_EQ(spg->start_decompression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->get_available_bytes(), 0);
		spg->clear();
	}

	SUBCASE("Twice in a row") {
		CHECK_EQ(spg->start_decompression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->start_decompression(false, BUFFER_SIZE), ERR_ALREADY_IN_USE);
		spg->clear();
	}

	SUBCASE("Invalid buffer size") {
		CHECK_EQ(spg->start_decompression(false, -1), ERR_INVALID_PARAMETER);
		spg->clear();
	}

	SUBCASE("Finish in decompression") {
		CHECK_EQ(spg->finish(), ERR_UNAVAILABLE);
		spg->clear();
	}
}

TEST_CASE("[TestStreamPeerGzip] Put/Get data") {
	Ref<StreamPeerGZIP> spg;
	spg.instantiate();

	// Prepare input data
	uint8_t input_data[DATA_SIZE];
	for (int i = 0; i < DATA_SIZE; i++) {
		input_data[i] = static_cast<uint8_t>(i);
	}

	SUBCASE("Unconfigured stream") {
		CHECK_EQ(spg->put_data(&input_data[0], DATA_SIZE), ERR_UNCONFIGURED);
		spg->clear();
	}

	SUBCASE("Invalid bytes number") {
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->put_data(&input_data[0], -1), ERR_INVALID_PARAMETER);
		uint8_t data_buffer[DATA_SIZE] = {};
		CHECK_EQ(spg->get_data(&data_buffer[0], -1), ERR_INVALID_PARAMETER);
		spg->clear();
	}

	SUBCASE("Successful put") {
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->put_data(&input_data[0], DATA_SIZE), OK);
		CHECK_EQ(spg->finish(), OK);
		CHECK_GT(spg->get_available_bytes(), 0);
		spg->clear();
	}

	SUBCASE("Successful put and get") {
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->put_data(&input_data[0], DATA_SIZE), OK);
		CHECK_EQ(spg->finish(), OK);
		const int available_bytes = spg->get_available_bytes();
		CHECK_GT(available_bytes, 0);

		uint8_t compressed_data[DATA_SIZE] = {};
		CHECK_EQ(spg->get_data(&compressed_data[0], available_bytes), OK);
		CHECK_EQ(spg->get_available_bytes(), 0);
		spg->clear();
	}

	SUBCASE("Successful put and get end-to-end") {
		// Compress data
		CHECK_EQ(spg->start_compression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->put_data(&input_data[0], DATA_SIZE), OK);
		CHECK_EQ(spg->finish(), OK);
		int available_bytes = spg->get_available_bytes();
		CHECK_GT(available_bytes, 0); // Available bytes are expected

		// Read compressed data
		uint8_t compressed_data[DATA_SIZE] = {};
		CHECK_EQ(spg->get_data(&compressed_data[0], available_bytes), OK);
		CHECK_EQ(spg->get_available_bytes(), 0); // All bytes are read
		spg->clear();

		// Decompress data
		CHECK_EQ(spg->start_decompression(false, BUFFER_SIZE), OK);
		CHECK_EQ(spg->put_data(&compressed_data[0], available_bytes), OK);
		available_bytes = spg->get_available_bytes();
		CHECK_GT(available_bytes, 0); // Available bytes are expected

		// Read decompressed data
		uint8_t decompressed_data[DATA_SIZE] = {};
		CHECK_EQ(spg->get_data(&decompressed_data[0], available_bytes), OK);
		CHECK_EQ(spg->get_available_bytes(), 0); // All bytes are read
		spg->clear();

		// Compare input array with decompressed one
		bool arraysAreEqual = true;
		for (size_t i = 0; i < DATA_SIZE; ++i) {
			if (input_data[i] != decompressed_data[i]) {
				arraysAreEqual = false;
				break;
			}
		}
		CHECK_EQ(true, arraysAreEqual);
	}
}

} // namespace TestStreamPeerGzip

#endif // TEST_STREAM_PEER_GZIP_H
