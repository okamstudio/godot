/**************************************************************************/
/*  test_crypto_mbedtls.h                                                 */
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

#include "core/crypto/crypto.h"
#include "core/crypto/hashing_context.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestCryptoMbedTLS {

void hmac_digest_test(HashingContext::HashType ht, String expected_hex);

TEST_CASE("[CryptoMbedTLS] HMAC digest") {
	// SHA-256
	hmac_digest_test(HashingContext::HashType::HASH_SHA256, "fe442023f8a7d36a810e1e7cd8a8e2816457f350a008fbf638296afa12085e59");

	// SHA-1
	hmac_digest_test(HashingContext::HashType::HASH_SHA1, "a0ac4cd68a2f4812c355983d94e8d025afe7dddf");
}

void hmac_context_digest_test(HashingContext::HashType ht, String expected_hex);

TEST_CASE("[HMACContext] HMAC digest") {
	// SHA-256
	hmac_context_digest_test(HashingContext::HashType::HASH_SHA256, "fe442023f8a7d36a810e1e7cd8a8e2816457f350a008fbf638296afa12085e59");

	// SHA-1
	hmac_context_digest_test(HashingContext::HashType::HASH_SHA1, "a0ac4cd68a2f4812c355983d94e8d025afe7dddf");
}

void crypto_key_public_only_test(const String &p_key_path, bool public_only);

TEST_CASE("[Crypto] CryptoKey is_public_only") {
	crypto_key_public_only_test(TestUtils::get_data_path("crypto/in.key"), false);
	crypto_key_public_only_test(TestUtils::get_data_path("crypto/in.pub"), true);
}

void crypto_key_save_test(const String &p_in_path, const String &p_out_path, bool public_only);

TEST_CASE("[Crypto] CryptoKey save") {
	const String in_priv_path = TestUtils::get_data_path("crypto/in.key");
	const String out_priv_path = TestUtils::get_data_path("crypto/out.key");
	crypto_key_save_test(in_priv_path, out_priv_path, false);

	const String in_pub_path = TestUtils::get_data_path("crypto/in.pub");
	const String out_pub_path = TestUtils::get_data_path("crypto/out.pub");
	crypto_key_save_test(in_pub_path, out_pub_path, true);
}

void crypto_key_save_public_only_test(const String &p_in_priv_path, const String &p_in_pub_path, const String &p_out_path);

TEST_CASE("[Crypto] CryptoKey save public_only") {
	const String in_priv_path = TestUtils::get_data_path("crypto/in.key");
	const String in_pub_path = TestUtils::get_data_path("crypto/in.pub");
	const String out_path = TestUtils::get_data_path("crypto/out_public_only.pub");
	crypto_key_save_public_only_test(in_priv_path, in_pub_path, out_path);
}

} // namespace TestCryptoMbedTLS
