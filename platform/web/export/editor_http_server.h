/**************************************************************************/
/*  editor_http_server.h                                                  */
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

#include "core/io/image_loader.h"
#include "core/io/stream_peer_tls.h"
#include "core/io/tcp_server.h"
#include "core/io/zip_io.h"
#include "editor/editor_paths.h"

class EditorHTTPServer : public RefCounted {
private:
	Ref<TCPServer> server;
	HashMap<String, String> mimes;
	Ref<StreamPeerTCP> tcp;
	Ref<StreamPeerTLS> tls;
	Ref<StreamPeer> peer;
	Ref<CryptoKey> key;
	Ref<X509Certificate> cert;
	bool use_tls = false;
	uint64_t time = 0;
	uint8_t req_buf[4096];
	int req_pos = 0;

	SafeFlag server_quit;
	Mutex server_lock;
	Thread server_thread;

	void _clear_client();
	void _set_internal_certs(Ref<Crypto> p_crypto);
	void _send_response();
	void _poll();

	static void _server_thread_poll(void *data);

public:
	EditorHTTPServer();
	~EditorHTTPServer();

	void stop();
	Error listen(int p_port, IPAddress p_address, bool p_use_tls, String p_tls_key, String p_tls_cert);
	bool is_listening() const;
};
