/**************************************************************************/
/*  test_stream_peer.h                                                    */
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
#include "tests/test_macros.h"

namespace TestStreamPeer {

TEST_CASE("[StreamPeer] Initialization through StreamPeerBuffer") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	CHECK_EQ(spb->is_big_endian_enabled(), false);
}

TEST_CASE("[StreamPeer] Get and sets through StreamPeerBuffer") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	SUBCASE("A int8_t value") {
		int8_t value = 42;

		spb->clear();
		spb->put_8(value);
		spb->seek(0);

		CHECK_EQ(spb->get_8(), value);
	}

	SUBCASE("A uint8_t value") {
		uint8_t value = 42;

		spb->clear();
		spb->put_u8(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u8(), value);
	}

	SUBCASE("A int16_t value") {
		int16_t value = 42;

		spb->clear();
		spb->put_16(value);
		spb->seek(0);

		CHECK_EQ(spb->get_16(), value);
	}

	SUBCASE("A uint16_t value") {
		uint16_t value = 42;

		spb->clear();
		spb->put_u16(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u16(), value);
	}

	SUBCASE("A int32_t value") {
		int32_t value = 42;

		spb->clear();
		spb->put_32(value);
		spb->seek(0);

		CHECK_EQ(spb->get_32(), value);
	}

	SUBCASE("A uint32_t value") {
		uint32_t value = 42;

		spb->clear();
		spb->put_u32(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u32(), value);
	}

	SUBCASE("A int64_t value") {
		int64_t value = 42;

		spb->clear();
		spb->put_64(value);
		spb->seek(0);

		CHECK_EQ(spb->get_64(), value);
	}

	SUBCASE("A int64_t value") {
		uint64_t value = 42;

		spb->clear();
		spb->put_u64(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u64(), value);
	}

	SUBCASE("A half-precision float value") {
		float value = 3.1415927f;
		float expected = 3.14062f;

		spb->clear();
		spb->put_half(value);
		spb->seek(0);

		CHECK(spb->get_half() == doctest::Approx(expected));
	}

	SUBCASE("A float value") {
		float value = 42.0f;

		spb->clear();
		spb->put_float(value);
		spb->seek(0);

		CHECK_EQ(spb->get_float(), value);
	}

	SUBCASE("A double value") {
		double value = 42.0;

		spb->clear();
		spb->put_double(value);
		spb->seek(0);

		CHECK_EQ(spb->get_double(), value);
	}

	SUBCASE("A string value") {
		String value = "Hello, World!";

		spb->clear();
		spb->put_string(value);
		spb->seek(0);

		CHECK_EQ(spb->get_string(), value);
	}

	SUBCASE("A utf8 string value") {
		String value = String::utf8("Hello✩, World✩!");

		spb->clear();
		spb->put_utf8_string(value);
		spb->seek(0);

		CHECK_EQ(spb->get_utf8_string(), value);
	}

	SUBCASE("A variant value") {
		Array value;
		value.push_front(42);
		value.push_front("Hello, World!");

		spb->clear();
		spb->put_var(value);
		spb->seek(0);

		CHECK_EQ(spb->get_var(), value);
	}
}

TEST_CASE("[StreamPeer] Get and sets big endian through StreamPeerBuffer") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();
	spb->set_big_endian(true);

	SUBCASE("A int16_t value") {
		int16_t value = 42;

		spb->clear();
		spb->put_16(value);
		spb->seek(0);

		CHECK_EQ(spb->get_16(), value);
	}

	SUBCASE("A uint16_t value") {
		uint16_t value = 42;

		spb->clear();
		spb->put_u16(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u16(), value);
	}

	SUBCASE("A int32_t value") {
		int32_t value = 42;

		spb->clear();
		spb->put_32(value);
		spb->seek(0);

		CHECK_EQ(spb->get_32(), value);
	}

	SUBCASE("A uint32_t value") {
		uint32_t value = 42;

		spb->clear();
		spb->put_u32(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u32(), value);
	}

	SUBCASE("A int64_t value") {
		int64_t value = 42;

		spb->clear();
		spb->put_64(value);
		spb->seek(0);

		CHECK_EQ(spb->get_64(), value);
	}

	SUBCASE("A int64_t value") {
		uint64_t value = 42;

		spb->clear();
		spb->put_u64(value);
		spb->seek(0);

		CHECK_EQ(spb->get_u64(), value);
	}

	SUBCASE("A float value") {
		float value = 42.0f;

		spb->clear();
		spb->put_float(value);
		spb->seek(0);

		CHECK_EQ(spb->get_float(), value);
	}

	SUBCASE("A half-precision float value") {
		float value = 3.1415927f;
		float expected = 3.14062f;

		spb->clear();
		spb->put_half(value);
		spb->seek(0);

		CHECK(spb->get_half() == doctest::Approx(expected));
	}

	SUBCASE("A double value") {
		double value = 42.0;

		spb->clear();
		spb->put_double(value);
		spb->seek(0);

		CHECK_EQ(spb->get_double(), value);
	}
}

TEST_CASE("[StreamPeer] Get string when there is no string") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	ERR_PRINT_OFF;
	CHECK_EQ(spb->get_string(), "");
	ERR_PRINT_ON;
}

TEST_CASE("[StreamPeer] Get UTF8 string when there is no string") {
	Ref<StreamPeerBuffer> spb;
	spb.instantiate();

	ERR_PRINT_OFF;
	CHECK_EQ(spb->get_utf8_string(), "");
	ERR_PRINT_ON;
}

} // namespace TestStreamPeer
