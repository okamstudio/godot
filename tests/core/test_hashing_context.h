/**************************************************************************/
/*  test_hashing_context.h                                                */
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

#include "core/crypto/hashing_context.h"

#include "tests/test_macros.h"

namespace TestHashingContext {

TEST_CASE("[HashingContext] Default - MD5/SHA1/SHA256") {
	HashingContext ctx;

	static const uint8_t md5_expected[] = {
		0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e
	};
	static const uint8_t sha1_expected[] = {
		0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90,
		0xaf, 0xd8, 0x07, 0x09
	};
	static const uint8_t sha256_expected[] = {
		0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
		0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
	};

	CHECK(ctx.start(HashingContext::HASH_MD5) == OK);
	PackedByteArray result = ctx.finish();
	REQUIRE(result.size() == 16);
	CHECK(memcmp(result.ptr(), md5_expected, 16) == 0);

	CHECK(ctx.start(HashingContext::HASH_SHA1) == OK);
	result = ctx.finish();
	REQUIRE(result.size() == 20);
	CHECK(memcmp(result.ptr(), sha1_expected, 20) == 0);

	CHECK(ctx.start(HashingContext::HASH_SHA256) == OK);
	result = ctx.finish();
	REQUIRE(result.size() == 32);
	CHECK(memcmp(result.ptr(), sha256_expected, 32) == 0);
}

TEST_CASE("[HashingContext] Multiple updates - MD5/SHA1/SHA256") {
	HashingContext ctx;
	const String s = "xyz";

	const PackedByteArray s_byte_parts[] = {
		String("x").to_ascii_buffer(),
		String("y").to_ascii_buffer(),
		String("z").to_ascii_buffer()
	};

	static const uint8_t md5_expected[] = {
		0xd1, 0x6f, 0xb3, 0x6f, 0x09, 0x11, 0xf8, 0x78, 0x99, 0x8c, 0x13, 0x61, 0x91, 0xaf, 0x70, 0x5e
	};
	static const uint8_t sha1_expected[] = {
		0x66, 0xb2, 0x74, 0x17, 0xd3, 0x7e, 0x02, 0x4c, 0x46, 0x52, 0x6c, 0x2f, 0x6d, 0x35, 0x8a, 0x75,
		0x4f, 0xc5, 0x52, 0xf3
	};
	static const uint8_t sha256_expected[] = {
		0x36, 0x08, 0xbc, 0xa1, 0xe4, 0x4e, 0xa6, 0xc4, 0xd2, 0x68, 0xeb, 0x6d, 0xb0, 0x22, 0x60, 0x26,
		0x98, 0x92, 0xc0, 0xb4, 0x2b, 0x86, 0xbb, 0xf1, 0xe7, 0x7a, 0x6f, 0xa1, 0x6c, 0x3c, 0x92, 0x82
	};

	CHECK(ctx.start(HashingContext::HASH_MD5) == OK);
	CHECK(ctx.update(s_byte_parts[0]) == OK);
	CHECK(ctx.update(s_byte_parts[1]) == OK);
	CHECK(ctx.update(s_byte_parts[2]) == OK);
	PackedByteArray result = ctx.finish();
	REQUIRE(result.size() == 16);
	CHECK(memcmp(result.ptr(), md5_expected, 16) == 0);

	CHECK(ctx.start(HashingContext::HASH_SHA1) == OK);
	CHECK(ctx.update(s_byte_parts[0]) == OK);
	CHECK(ctx.update(s_byte_parts[1]) == OK);
	CHECK(ctx.update(s_byte_parts[2]) == OK);
	result = ctx.finish();
	REQUIRE(result.size() == 20);
	CHECK(memcmp(result.ptr(), sha1_expected, 20) == 0);

	CHECK(ctx.start(HashingContext::HASH_SHA256) == OK);
	CHECK(ctx.update(s_byte_parts[0]) == OK);
	CHECK(ctx.update(s_byte_parts[1]) == OK);
	CHECK(ctx.update(s_byte_parts[2]) == OK);
	result = ctx.finish();
	REQUIRE(result.size() == 32);
	CHECK(memcmp(result.ptr(), sha256_expected, 32) == 0);
}

TEST_CASE("[HashingContext] Invalid use of start") {
	HashingContext ctx;

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			ctx.start(static_cast<HashingContext::HashType>(-1)) == ERR_UNAVAILABLE,
			"Using invalid hash types should fail.");
	ERR_PRINT_ON;

	REQUIRE(ctx.start(HashingContext::HASH_MD5) == OK);

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			ctx.start(HashingContext::HASH_MD5) == ERR_ALREADY_IN_USE,
			"Calling 'start' twice before 'finish' should fail.");
	ERR_PRINT_ON;
}

TEST_CASE("[HashingContext] Invalid use of update") {
	HashingContext ctx;

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			ctx.update(PackedByteArray()) == ERR_UNCONFIGURED,
			"Calling 'update' before 'start' should fail.");
	ERR_PRINT_ON;

	REQUIRE(ctx.start(HashingContext::HASH_MD5) == OK);

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			ctx.update(PackedByteArray()) == FAILED,
			"Calling 'update' with an empty byte array should fail.");
	ERR_PRINT_ON;
}

TEST_CASE("[HashingContext] Invalid use of finish") {
	HashingContext ctx;

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			ctx.finish() == PackedByteArray(),
			"Calling 'finish' before 'start' should return an empty byte array.");
	ERR_PRINT_ON;
}
} // namespace TestHashingContext
