/**************************************************************************/
/*  test_stream_peer_buffer.h                                             */
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

#ifndef TEST_STREAM_PEER_BUFFER_H
#define TEST_STREAM_PEER_BUFFER_H

#include "core/io/stream_peer.h"
#include "tests/test_macros.h"

namespace TestStreamPeerBuffer {

TEST_CASE("[StreamPeerBuffer] Initialization") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	CHECK_EQ(spb->get_size(), 0);
	CHECK_EQ(spb->get_position(), 0);
	CHECK_EQ(spb->get_available_bytes(), 0);
}

TEST_CASE("[StreamPeerBuffer] Seek") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	uint8_t first = 5;
	uint8_t second = 7;
	uint8_t third = 11;

	spb->put_u8(first);
	spb->put_u8(second);
	spb->put_u8(third);

	spb->seek(0);
	CHECK_EQ(spb->get_u8(), first);
	CHECK_EQ(spb->get_u8(), second);
	CHECK_EQ(spb->get_u8(), third);

	spb->seek(1);
	CHECK_EQ(spb->get_position(), 1);
	CHECK_EQ(spb->get_u8(), second);

	spb->seek(1);
	ERR_PRINT_OFF;
	spb->seek(-1);
	ERR_PRINT_ON;
	CHECK_EQ(spb->get_position(), 1);
	ERR_PRINT_OFF;
	spb->seek(5);
	ERR_PRINT_ON;
	CHECK_EQ(spb->get_position(), 1);
}

TEST_CASE("[StreamPeerBuffer] Resize") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	CHECK_EQ(spb->get_size(), 0);
	CHECK_EQ(spb->get_position(), 0);
	CHECK_EQ(spb->get_available_bytes(), 0);

	spb->resize(42);
	CHECK_EQ(spb->get_size(), 42);
	CHECK_EQ(spb->get_position(), 0);
	CHECK_EQ(spb->get_available_bytes(), 42);

	spb->seek(21);
	CHECK_EQ(spb->get_size(), 42);
	CHECK_EQ(spb->get_position(), 21);
	CHECK_EQ(spb->get_available_bytes(), 21);
}

TEST_CASE("[StreamPeerBuffer] Get underlying data array") {
	uint8_t first = 5;
	uint8_t second = 7;
	uint8_t third = 11;

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->put_u8(first);
	spb->put_u8(second);
	spb->put_u8(third);

	Vector<uint8_t> data_array = spb->get_data_array();

	CHECK_EQ(data_array[0], first);
	CHECK_EQ(data_array[1], second);
	CHECK_EQ(data_array[2], third);
}

TEST_CASE("[StreamPeerBuffer] Set underlying data array") {
	uint8_t first = 5;
	uint8_t second = 7;
	uint8_t third = 11;

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->put_u8(1);
	spb->put_u8(2);
	spb->put_u8(3);

	Vector<uint8_t> new_data_array;
	new_data_array.push_back(first);
	new_data_array.push_back(second);
	new_data_array.push_back(third);

	spb->set_data_array(new_data_array);

	CHECK_EQ(spb->get_u8(), first);
	CHECK_EQ(spb->get_u8(), second);
	CHECK_EQ(spb->get_u8(), third);
}

TEST_CASE("[StreamPeerBuffer] Duplicate") {
	uint8_t first = 5;
	uint8_t second = 7;
	uint8_t third = 11;

	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->put_u8(first);
	spb->put_u8(second);
	spb->put_u8(third);

	Ref<StreamPeerBuffer> spb2 = spb->duplicate();

	CHECK_EQ(spb2->get_u8(), first);
	CHECK_EQ(spb2->get_u8(), second);
	CHECK_EQ(spb2->get_u8(), third);
}

TEST_CASE("[StreamPeerBuffer] Put data with size equal to zero does nothing") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	uint8_t data = 42;

	Error error = spb->put_data((const uint8_t *)&data, 0);

	CHECK_EQ(error, OK);
	CHECK_EQ(spb->get_size(), 0);
	CHECK_EQ(spb->get_position(), 0);
	CHECK_EQ(spb->get_available_bytes(), 0);
}

TEST_CASE("[StreamPeerBuffer] Get data with invalid size returns an error") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	uint8_t data = 42;
	spb->put_u8(data);
	spb->seek(0);

	uint8_t data_out = 0;
	Error error = spb->get_data(&data_out, 3);

	CHECK_EQ(error, ERR_INVALID_PARAMETER);
	CHECK_EQ(spb->get_size(), 1);
	CHECK_EQ(spb->get_position(), 1);
}

} // namespace TestStreamPeerBuffer

#endif // TEST_STREAM_PEER_BUFFER_H
