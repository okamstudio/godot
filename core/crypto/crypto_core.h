/**************************************************************************/
/*  crypto_core.h                                                         */
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

#include "core/object/ref_counted.h"

class CryptoCore {
public:
	class RandomGenerator {
	private:
		void *entropy = nullptr;
		void *ctx = nullptr;

		static int _entropy_poll(void *p_data, unsigned char *r_buffer, size_t p_len, size_t *r_len);

	public:
		RandomGenerator();
		~RandomGenerator();

		Error init();
		Error get_random_bytes(uint8_t *r_buffer, size_t p_bytes);
	};

	class MD5Context {
	private:
		void *ctx = nullptr;

	public:
		MD5Context();
		~MD5Context();

		Error start();
		Error update(const uint8_t *p_src, size_t p_len);
		Error finish(unsigned char r_hash[16]);
	};

	class SHA1Context {
	private:
		void *ctx = nullptr;

	public:
		SHA1Context();
		~SHA1Context();

		Error start();
		Error update(const uint8_t *p_src, size_t p_len);
		Error finish(unsigned char r_hash[20]);
	};

	class SHA256Context {
	private:
		void *ctx = nullptr;

	public:
		SHA256Context();
		~SHA256Context();

		Error start();
		Error update(const uint8_t *p_src, size_t p_len);
		Error finish(unsigned char r_hash[32]);
	};

	class AESContext {
	private:
		void *ctx = nullptr;

	public:
		AESContext();
		~AESContext();

		Error set_encode_key(const uint8_t *p_key, size_t p_bits);
		Error set_decode_key(const uint8_t *p_key, size_t p_bits);
		Error encrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]);
		Error decrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]);
		Error encrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst);
		Error decrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst);
		Error encrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst);
		Error decrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst);
	};

	static String b64_encode_str(const uint8_t *p_src, int p_src_len);
	static Error b64_encode(uint8_t *r_dst, int p_dst_len, size_t *r_len, const uint8_t *p_src, int p_src_len);
	static Error b64_decode(uint8_t *r_dst, int p_dst_len, size_t *r_len, const uint8_t *p_src, int p_src_len);

	static Error md5(const uint8_t *p_src, int p_src_len, unsigned char r_hash[16]);
	static Error sha1(const uint8_t *p_src, int p_src_len, unsigned char r_hash[20]);
	static Error sha256(const uint8_t *p_src, int p_src_len, unsigned char r_hash[32]);
};
