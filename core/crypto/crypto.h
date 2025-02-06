/**************************************************************************/
/*  crypto.h                                                              */
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
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/ref_counted.h"

class CryptoKey : public Resource {
	GDCLASS(CryptoKey, Resource);

protected:
	static void _bind_methods();
	static CryptoKey *(*_create)(bool p_notify_postinitialize);

public:
	static CryptoKey *create(bool p_notify_postinitialize = true);
	virtual Error load(const String &p_path, bool p_public_only = false) = 0;
	virtual Error save(const String &p_path, bool p_public_only = false) = 0;
	virtual String save_to_string(bool p_public_only = false) = 0;
	virtual Error load_from_string(const String &p_string_key, bool p_public_only = false) = 0;
	virtual bool is_public_only() const = 0;
};

class X509Certificate : public Resource {
	GDCLASS(X509Certificate, Resource);

protected:
	static void _bind_methods();
	static X509Certificate *(*_create)(bool p_notify_postinitialize);

public:
	static X509Certificate *create(bool p_notify_postinitialize = true);
	virtual Error load(const String &p_path) = 0;
	virtual Error load_from_memory(const uint8_t *p_buffer, int p_len) = 0;
	virtual Error save(const String &p_path) = 0;
	virtual String save_to_string() = 0;
	virtual Error load_from_string(const String &string) = 0;
};

class TLSOptions : public RefCounted {
	GDCLASS(TLSOptions, RefCounted);

private:
	enum Mode {
		MODE_CLIENT = 0,
		MODE_CLIENT_UNSAFE = 1,
		MODE_SERVER = 2,
	};

	Mode mode = MODE_CLIENT;
	String common_name;
	Ref<X509Certificate> trusted_ca_chain;
	Ref<X509Certificate> own_certificate;
	Ref<CryptoKey> private_key;

protected:
	static void _bind_methods();

public:
	static Ref<TLSOptions> client(Ref<X509Certificate> p_trusted_chain = Ref<X509Certificate>(), const String &p_common_name_override = String());
	static Ref<TLSOptions> client_unsafe(Ref<X509Certificate> p_trusted_chain);
	static Ref<TLSOptions> server(Ref<CryptoKey> p_own_key, Ref<X509Certificate> p_own_certificate);

	String get_common_name_override() const { return common_name; }
	Ref<X509Certificate> get_trusted_ca_chain() const { return trusted_ca_chain; }
	Ref<X509Certificate> get_own_certificate() const { return own_certificate; }
	Ref<CryptoKey> get_private_key() const { return private_key; }
	bool is_server() const { return mode == MODE_SERVER; }
	bool is_unsafe_client() const { return mode == MODE_CLIENT_UNSAFE; }
};

class HMACContext : public RefCounted {
	GDCLASS(HMACContext, RefCounted);

protected:
	static void _bind_methods();
	static HMACContext *(*_create)(bool p_notify_postinitialize);

public:
	static HMACContext *create(bool p_notify_postinitialize = true);

	virtual Error start(HashingContext::HashType p_hash_type, const PackedByteArray &p_key) = 0;
	virtual Error update(const PackedByteArray &p_data) = 0;
	virtual PackedByteArray finish() = 0;

	HMACContext() {}
	virtual ~HMACContext() {}
};

class Crypto : public RefCounted {
	GDCLASS(Crypto, RefCounted);

protected:
	static void _bind_methods();
	static Crypto *(*_create)(bool p_notify_postinitialize);
	static void (*_load_default_certificates)(const String &p_path);

public:
	static Crypto *create(bool p_notify_postinitialize = true);
	static void load_default_certificates(const String &p_path);

	virtual PackedByteArray generate_random_bytes(int p_bytes) = 0;
	virtual Ref<CryptoKey> generate_rsa(int p_bytes) = 0;
	virtual Ref<X509Certificate> generate_self_signed_certificate(Ref<CryptoKey> p_key, const String &p_issuer_name, const String &p_not_before, const String &p_not_after) = 0;

	virtual Vector<uint8_t> sign(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, Ref<CryptoKey> p_key) = 0;
	virtual bool verify(HashingContext::HashType p_hash_type, const Vector<uint8_t> &p_hash, const Vector<uint8_t> &p_signature, Ref<CryptoKey> p_key) = 0;
	virtual Vector<uint8_t> encrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_plaintext) = 0;
	virtual Vector<uint8_t> decrypt(Ref<CryptoKey> p_key, const Vector<uint8_t> &p_ciphertext) = 0;

	PackedByteArray hmac_digest(HashingContext::HashType p_hash_type, const PackedByteArray &p_key, const PackedByteArray &p_msg);

	// Compares two PackedByteArrays for equality without leaking timing information in order to prevent timing attacks.
	// @see: https://paragonie.com/blog/2015/11/preventing-timing-attacks-on-string-comparison-with-double-hmac-strategy
	bool constant_time_compare(const PackedByteArray &p_trusted, const PackedByteArray &p_received);

	Crypto() {}
};

class ResourceFormatLoaderCrypto : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

	// Treat certificates as text files, do not generate a `*.{crt,key,pub}.uid` file.
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override { return ResourceUID::INVALID_ID; }
	virtual bool has_custom_uid_support() const override { return true; }
};

class ResourceFormatSaverCrypto : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};
