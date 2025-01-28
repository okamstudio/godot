/**************************************************************************/
/*  crypto_mbedtls.h                                                      */
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

#ifndef CRYPTO_MBEDTLS_H
#define CRYPTO_MBEDTLS_H

#include "core/crypto/crypto.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>

class CryptoMbedTLS;
class TLSContextMbedTLS;
class CryptoKeyMbedTLS : public CryptoKey {
private:
	mbedtls_pk_context pkey;
	int locks = 0;
	bool public_only = true;

	int _parse_key(const uint8_t *p_buf, int p_size);

public:
	static CryptoKey *create(bool p_notify_postinitialize = true);
	static void make_default() { CryptoKey::_create = create; }
	static void finalize() { CryptoKey::_create = nullptr; }

	virtual Error load(const String &p_path, bool p_public_only);
	virtual Error save(const String &p_path, bool p_public_only);
	virtual String save_to_string(bool p_public_only);
	virtual Error load_from_string(const String &p_string_key, bool p_public_only);
	virtual bool is_public_only() const { return public_only; }

	CryptoKeyMbedTLS() {
		mbedtls_pk_init(&pkey);
		locks = 0;
	}
	~CryptoKeyMbedTLS() {
		mbedtls_pk_free(&pkey);
	}

	_FORCE_INLINE_ void lock() { locks++; }
	_FORCE_INLINE_ void unlock() { locks--; }

	friend class CryptoMbedTLS;
	friend class TLSContextMbedTLS;
};

class X509CertificateMbedTLS : public X509Certificate {
private:
	mbedtls_x509_crt cert;
	int locks;

public:
	static X509Certificate *create(bool p_notify_postinitialize = true);
	static void make_default() { X509Certificate::_create = create; }
	static void finalize() { X509Certificate::_create = nullptr; }

	virtual Error load(const String &p_path);
	virtual Error load_from_memory(const uint8_t *p_buffer, int p_len);
	virtual Error save(const String &p_path);
	virtual String save_to_string();
	virtual Error load_from_string(const String &p_string_key);

	X509CertificateMbedTLS() {
		mbedtls_x509_crt_init(&cert);
		locks = 0;
	}
	~X509CertificateMbedTLS() {
		mbedtls_x509_crt_free(&cert);
	}

	_FORCE_INLINE_ void lock() { locks++; }
	_FORCE_INLINE_ void unlock() { locks--; }

	friend class CryptoMbedTLS;
	friend class TLSContextMbedTLS;
};

class HMACContextMbedTLS : public HMACContext {
private:
	HashingContext::HashType hash_type;
	int hash_len = 0;
	void *ctx = nullptr;

public:
	static HMACContext *create(bool p_notify_postinitialize = true);
	static void make_default() { HMACContext::_create = create; }
	static void finalize() { HMACContext::_create = nullptr; }

	static bool is_md_type_allowed(mbedtls_md_type_t p_md_type);

	virtual Error start(HashingContext::HashType p_hash_type, const PackedByteArray &p_key);
	virtual Error update(const PackedByteArray &p_data);
	virtual PackedByteArray finish();

	HMACContextMbedTLS() {}
	~HMACContextMbedTLS();
};

class CryptoMbedTLS : public Crypto {
private:
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	static X509CertificateMbedTLS *default_certs;

public:
	static Crypto *create(bool p_notify_postinitialize = true);
	static void initialize_crypto();
	static void finalize_crypto();
	static X509CertificateMbedTLS *get_default_certificates();
	static void load_default_certificates(const String &p_path);
	static mbedtls_md_type_t md_type_from_hashtype(HashingContext::HashType p_hash_type, int &r_size);

	virtual PackedByteArray generate_random_bytes(int p_bytes);
	virtual Ref<CryptoKey> generate_rsa(int p_bytes);
	virtual Ref<X509Certificate> generate_self_signed_certificate(Ref<CryptoKey> p_key, const String &p_issuer_name, const String &p_not_before, const String &p_not_after);
	virtual Vector<uint8_t> sign(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, Ref<CryptoKey> p_key);
	virtual bool verify(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, const Vector<uint8_t> &p_signature, Ref<CryptoKey> p_key);
	virtual Vector<uint8_t> encrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_plaintext);
	virtual Vector<uint8_t> decrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_ciphertext);

	CryptoMbedTLS();
	~CryptoMbedTLS();
};

#endif // CRYPTO_MBEDTLS_H
