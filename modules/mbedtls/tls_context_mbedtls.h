/**************************************************************************/
/*  tls_context_mbedtls.h                                                 */
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

#include "crypto_mbedtls.h"

#include "core/object/ref_counted.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>

class TLSContextMbedTLS;

class CookieContextMbedTLS : public RefCounted {
	friend class TLSContextMbedTLS;

protected:
	bool inited = false;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_cookie_ctx cookie_ctx;

public:
	Error setup();
	void clear();

	CookieContextMbedTLS();
	~CookieContextMbedTLS();
};

class TLSContextMbedTLS : public RefCounted {
protected:
	bool inited = false;

public:
	static void print_mbedtls_error(int p_ret);

	Ref<X509CertificateMbedTLS> certs;
	Ref<CryptoKeyMbedTLS> pkey;
	Ref<CookieContextMbedTLS> cookies;

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context tls;
	mbedtls_ssl_config conf;

	Error _setup(int p_endpoint, int p_transport, int p_authmode);
	Error init_server(int p_transport, Ref<TLSOptions> p_options, Ref<CookieContextMbedTLS> p_cookies = Ref<CookieContextMbedTLS>());
	Error init_client(int p_transport, const String &p_hostname, Ref<TLSOptions> p_options);
	void clear();

	mbedtls_ssl_context *get_context();

	TLSContextMbedTLS();
	~TLSContextMbedTLS();
};
