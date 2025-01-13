/**************************************************************************/
/*  editor_http_server.cpp                                                */
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

#include "editor_http_server.h"

void EditorHTTPServer::_server_thread_poll(void *data) {
	EditorHTTPServer *web_server = static_cast<EditorHTTPServer *>(data);
	while (!web_server->server_quit.is_set()) {
		OS::get_singleton()->delay_usec(6900);
		{
			MutexLock lock(web_server->server_lock);
			web_server->_poll();
		}
	}
}

void EditorHTTPServer::_clear_client() {
	peer = Ref<StreamPeer>();
	tls = Ref<StreamPeerTLS>();
	tcp = Ref<StreamPeerTCP>();
	memset(req_buf, 0, sizeof(req_buf));
	time = 0;
	req_pos = 0;
}

void EditorHTTPServer::_set_internal_certs(Ref<Crypto> p_crypto) {
	const String cache_path = EditorPaths::get_singleton()->get_cache_dir();
	const String key_path = cache_path.path_join("html5_server.key");
	const String crt_path = cache_path.path_join("html5_server.crt");
	bool regen = !FileAccess::exists(key_path) || !FileAccess::exists(crt_path);
	if (!regen) {
		key = Ref<CryptoKey>(CryptoKey::create());
		cert = Ref<X509Certificate>(X509Certificate::create());
		if (key->load(key_path) != OK || cert->load(crt_path) != OK) {
			regen = true;
		}
	}
	if (regen) {
		key = p_crypto->generate_rsa(2048);
		key->save(key_path);
		cert = p_crypto->generate_self_signed_certificate(key, "CN=godot-debug.local,O=A Game Dev,C=XXA", "20140101000000", "20340101000000");
		cert->save(crt_path);
	}
}

void EditorHTTPServer::_send_response() {
	Vector<String> psa = String((char *)req_buf).split("\r\n");
	int len = psa.size();
	ERR_FAIL_COND_MSG(len < 4, "Not enough response headers, got: " + itos(len) + ", expected >= 4.");

	Vector<String> req = psa[0].split(" ", false);
	ERR_FAIL_COND_MSG(req.size() < 2, "Invalid protocol or status code.");

	// Wrong protocol
	ERR_FAIL_COND_MSG(req[0] != "GET" || req[2] != "HTTP/1.1", "Invalid method or HTTP version.");

	const int query_index = req[1].find_char('?');
	const String path = (query_index == -1) ? req[1] : req[1].substr(0, query_index);

	const String req_file = path.get_file();
	const String req_ext = path.get_extension();
	const String cache_path = EditorPaths::get_singleton()->get_temp_dir().path_join("web");
	const String filepath = cache_path.path_join(req_file);

	if (!mimes.has(req_ext) || !FileAccess::exists(filepath)) {
		String s = "HTTP/1.1 404 Not Found\r\n";
		s += "Connection: Close\r\n";
		s += "\r\n";
		CharString cs = s.utf8();
		peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
		return;
	}
	const String ctype = mimes[req_ext];

	Ref<FileAccess> f = FileAccess::open(filepath, FileAccess::READ);
	ERR_FAIL_COND(f.is_null());
	String s = "HTTP/1.1 200 OK\r\n";
	s += "Connection: Close\r\n";
	s += "Content-Type: " + ctype + "\r\n";
	s += "Access-Control-Allow-Origin: *\r\n";
	s += "Cross-Origin-Opener-Policy: same-origin\r\n";
	s += "Cross-Origin-Embedder-Policy: require-corp\r\n";
	s += "Cache-Control: no-store, max-age=0\r\n";
	s += "\r\n";
	CharString cs = s.utf8();
	Error err = peer->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
	if (err != OK) {
		ERR_FAIL();
	}

	while (true) {
		uint8_t bytes[4096];
		uint64_t read = f->get_buffer(bytes, 4096);
		if (read == 0) {
			break;
		}
		err = peer->put_data(bytes, read);
		if (err != OK) {
			ERR_FAIL();
		}
	}
}

void EditorHTTPServer::_poll() {
	if (!server->is_listening()) {
		return;
	}
	if (tcp.is_null()) {
		if (!server->is_connection_available()) {
			return;
		}
		tcp = server->take_connection();
		peer = tcp;
		time = OS::get_singleton()->get_ticks_usec();
	}
	if (OS::get_singleton()->get_ticks_usec() - time > 1000000) {
		_clear_client();
		return;
	}
	if (tcp->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}

	if (use_tls) {
		if (tls.is_null()) {
			tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
			peer = tls;
			if (tls->accept_stream(tcp, TLSOptions::server(key, cert)) != OK) {
				_clear_client();
				return;
			}
		}
		tls->poll();
		if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			// Still handshaking, keep waiting.
			return;
		}
		if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			_clear_client();
			return;
		}
	}

	while (true) {
		char *r = (char *)req_buf;
		int l = req_pos - 1;
		if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
			_send_response();
			_clear_client();
			return;
		}

		int read = 0;
		ERR_FAIL_COND(req_pos >= 4096);
		Error err = peer->get_partial_data(&req_buf[req_pos], 1, read);
		if (err != OK) {
			// Got an error
			_clear_client();
			return;
		} else if (read != 1) {
			// Busy, wait next poll
			return;
		}
		req_pos += read;
	}
}

void EditorHTTPServer::stop() {
	server_quit.set();
	if (server_thread.is_started()) {
		server_thread.wait_to_finish();
	}
	if (server.is_valid()) {
		server->stop();
	}
	_clear_client();
}

Error EditorHTTPServer::listen(int p_port, IPAddress p_address, bool p_use_tls, String p_tls_key, String p_tls_cert) {
	MutexLock lock(server_lock);
	if (server->is_listening()) {
		return ERR_ALREADY_IN_USE;
	}
	use_tls = p_use_tls;
	if (use_tls) {
		Ref<Crypto> crypto = Crypto::create();
		if (crypto.is_null()) {
			return ERR_UNAVAILABLE;
		}
		if (!p_tls_key.is_empty() && !p_tls_cert.is_empty()) {
			key = Ref<CryptoKey>(CryptoKey::create());
			Error err = key->load(p_tls_key);
			ERR_FAIL_COND_V(err != OK, err);
			cert = Ref<X509Certificate>(X509Certificate::create());
			err = cert->load(p_tls_cert);
			ERR_FAIL_COND_V(err != OK, err);
		} else {
			_set_internal_certs(crypto);
		}
	}
	Error err = server->listen(p_port, p_address);
	if (err == OK) {
		server_quit.clear();
		server_thread.start(_server_thread_poll, this);
	}
	return err;
}

bool EditorHTTPServer::is_listening() const {
	MutexLock lock(server_lock);
	return server->is_listening();
}

EditorHTTPServer::EditorHTTPServer() {
	mimes["html"] = "text/html";
	mimes["js"] = "application/javascript";
	mimes["json"] = "application/json";
	mimes["pck"] = "application/octet-stream";
	mimes["png"] = "image/png";
	mimes["svg"] = "image/svg";
	mimes["wasm"] = "application/wasm";
	server.instantiate();
	stop();
}

EditorHTTPServer::~EditorHTTPServer() {
	stop();
}
