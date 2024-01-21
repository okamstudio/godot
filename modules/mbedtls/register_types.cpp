/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "crypto_mbedtls.h"
#include "dtls_server_mbedtls.h"
#include "packet_peer_mbed_dtls.h"
#include "stream_peer_mbedtls.h"

#if MBEDTLS_VERSION_MAJOR >= 3
#include <psa/crypto.h>
#endif

#ifdef TESTS_ENABLED
#include "tests/test_crypto_mbedtls.h"
#endif

static bool godot_mbedtls_initialized = false;

void initialize_mbedtls_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_EARLY_CORE) {
		return;
	}

#if MBEDTLS_VERSION_MAJOR >= 3
	int status = psa_crypto_init();
	ERR_FAIL_COND_MSG(status != PSA_SUCCESS, "Failed to initialize psa crypto. The mbedTLS modules will not work.");
#endif

#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->is_stdout_verbose()) {
		mbedtls_debug_set_threshold(1);
	}
#endif

	godot_mbedtls_initialized = true;

	CryptoMbedTLS::initialize_crypto();
	StreamPeerMbedTLS::initialize_tls();
	PacketPeerMbedDTLS::initialize_dtls();
	DTLSServerMbedTLS::initialize();
}

void uninitialize_mbedtls_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_CORE) {
		return;
	}

	if (!godot_mbedtls_initialized) {
		return;
	}

#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_psa_crypto_free();
#endif

	DTLSServerMbedTLS::finalize();
	PacketPeerMbedDTLS::finalize_dtls();
	StreamPeerMbedTLS::finalize_tls();
	CryptoMbedTLS::finalize_crypto();
}
