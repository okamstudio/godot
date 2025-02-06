/**************************************************************************/
/*  test_stream_peer_extension.h                                          */
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

#ifndef TEST_STREAM_PEER_EXTENSION_H
#define TEST_STREAM_PEER_EXTENSION_H

#include "core/io/stream_peer.h"
#include "tests/test_macros.h"

namespace TestStreamPeerExtension {

TEST_CASE("[StreamPeerExtension] Initialization") {
	Ref<StreamPeerExtension> spb;
	spb.instantiate();
	CHECK_EQ(spb->get_available_bytes(), 0);
}

TEST_CASE("[StreamPeerExtension] Put data without overridden method return FAILED") {
	Ref<StreamPeerExtension> spb;
	spb.instantiate();

	uint8_t data = 42;
	Error error = spb->put_data(&data, 1);

	CHECK_EQ(error, FAILED);
}

TEST_CASE("[StreamPeerExtension] Put partial data without overridden method return FAILED") {
	Ref<StreamPeerExtension> spb;
	spb.instantiate();

	uint8_t data = 42;
	int sent_bytes = 0;
	Error error = spb->put_partial_data(&data, 1, sent_bytes);

	CHECK_EQ(error, FAILED);
	CHECK_EQ(sent_bytes, 0);
}

TEST_CASE("[StreamPeerExtension] Get data without overridden method return FAILED") {
	Ref<StreamPeerExtension> spb;
	spb.instantiate();

	uint8_t data_out = 0;
	Error error = spb->get_data(&data_out, 1);

	CHECK_EQ(error, FAILED);
}

TEST_CASE("[StreamPeerExtension] Get partial data without overridden method return FAILED") {
	Ref<StreamPeerExtension> spb;
	spb.instantiate();

	uint8_t data = 42;
	int received_bytes = 0;
	Error error = spb->get_partial_data(&data, 1, received_bytes);

	CHECK_EQ(error, FAILED);
	CHECK_EQ(received_bytes, 0);
}

} // namespace TestStreamPeerExtension

#endif // TEST_STREAM_PEER_EXTENSION_H
