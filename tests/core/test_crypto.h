/**************************************************************************/
/*  test_crypto.h                                                         */
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

#ifndef TEST_CRYPTO_H
#define TEST_CRYPTO_H

#include "core/crypto/crypto.h"
#include "tests/test_macros.h"

namespace TestCrypto {

class _MockCrypto : public Crypto {
	virtual PackedByteArray generate_random_bytes(int p_bytes) { return PackedByteArray(); }
	virtual Ref<CryptoKey> generate_rsa(int p_bytes) { return nullptr; }
	virtual Ref<X509Certificate> generate_self_signed_certificate(Ref<CryptoKey> p_key, const String &p_issuer_name, const String &p_not_before, const String &p_not_after) { return nullptr; }

	virtual Vector<uint8_t> sign(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, Ref<CryptoKey> p_key) { return Vector<uint8_t>(); }
	virtual bool verify(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, const Vector<uint8_t> &p_signature, Ref<CryptoKey> p_key) { return false; }
	virtual Vector<uint8_t> encrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_plaintext) { return Vector<uint8_t>(); }
	virtual Vector<uint8_t> decrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_ciphertext) { return Vector<uint8_t>(); }
	virtual PackedByteArray hmac_digest(HashingContext::HashType p_hash_type, const PackedByteArray &p_key, const PackedByteArray &p_msg) { return PackedByteArray(); }
};

PackedByteArray raw_to_pba(const uint8_t *arr, size_t len) {
	PackedByteArray pba;
	pba.resize(len);
	for (size_t i = 0; i < len; i++) {
		pba.set(i, arr[i]);
	}
	return pba;
}

TEST_CASE("[Crypto] PackedByteArray constant time compare") {
	const uint8_t hm1[] = { 144, 140, 176, 38, 88, 113, 101, 45, 71, 105, 10, 91, 248, 16, 117, 244, 189, 30, 238, 29, 219, 134, 82, 130, 212, 114, 161, 166, 188, 169, 200, 106 };
	const uint8_t hm2[] = { 80, 30, 144, 228, 108, 38, 188, 125, 150, 64, 165, 127, 221, 118, 144, 232, 45, 100, 15, 248, 193, 244, 245, 34, 116, 147, 132, 200, 110, 27, 38, 75 };
	PackedByteArray p1 = raw_to_pba(hm1, std::size(hm1));
	PackedByteArray p2 = raw_to_pba(hm2, std::size(hm2));
	_MockCrypto crypto;
	bool equal = crypto.constant_time_compare(p1, p1);
	CHECK(equal);
	equal = crypto.constant_time_compare(p1, p2);
	CHECK(!equal);
}
} // namespace TestCrypto

#endif // TEST_CRYPTO_H
