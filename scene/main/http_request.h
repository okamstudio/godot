/**************************************************************************/
/*  http_request.h                                                        */
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

#include "core/io/http_client.h"
#include "core/io/stream_peer_gzip.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "scene/main/node.h"

class Timer;

class HTTPRequest : public Node {
	GDCLASS(HTTPRequest, Node);

public:
	enum Result {
		RESULT_SUCCESS,
		RESULT_CHUNKED_BODY_SIZE_MISMATCH,
		RESULT_CANT_CONNECT,
		RESULT_CANT_RESOLVE,
		RESULT_CONNECTION_ERROR,
		RESULT_TLS_HANDSHAKE_ERROR,
		RESULT_NO_RESPONSE,
		RESULT_BODY_SIZE_LIMIT_EXCEEDED,
		RESULT_BODY_DECOMPRESS_FAILED,
		RESULT_REQUEST_FAILED,
		RESULT_DOWNLOAD_FILE_CANT_OPEN,
		RESULT_DOWNLOAD_FILE_WRITE_ERROR,
		RESULT_REDIRECT_LIMIT_REACHED,
		RESULT_TIMEOUT

	};

private:
	bool requesting = false;

	String request_string;
	String url;
	int port = 80;
	Vector<String> headers;
	bool use_tls = false;
	Ref<TLSOptions> tls_options;
	HTTPClient::Method method;
	Vector<uint8_t> request_data;

	bool request_sent = false;
	Ref<HTTPClient> client;
	PackedByteArray body;
	SafeFlag use_threads;
	bool accept_gzip = true;

	bool got_response = false;
	int response_code = 0;
	Vector<String> response_headers;

	String download_to_file;

	Ref<StreamPeerGZIP> decompressor;
	Ref<FileAccess> file;

	int body_len = -1;
	SafeNumeric<int> downloaded;
	SafeNumeric<int> final_body_size;
	int body_size_limit = -1;

	int redirections = 0;

	bool _update_connection();

	int max_redirects = 8;

	double timeout = 0;

	void _redirect_request(const String &p_new_url);

	bool _handle_response(bool *ret_value);

	Error _parse_url(const String &p_url);
	Error _request();

	bool has_header(const PackedStringArray &p_headers, const String &p_header_name);
	String get_header_value(const PackedStringArray &p_headers, const String &header_name);

	SafeFlag thread_done;
	SafeFlag thread_request_quit;

	Thread thread;

	void _defer_done(int p_status, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_data);
	void _request_done(int p_status, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_data);
	static void _thread_func(void *p_userdata);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	Error request(const String &p_url, const Vector<String> &p_custom_headers = Vector<String>(), HTTPClient::Method p_method = HTTPClient::METHOD_GET, const String &p_request_data = ""); //connects to a full url and perform request
	Error request_raw(const String &p_url, const Vector<String> &p_custom_headers = Vector<String>(), HTTPClient::Method p_method = HTTPClient::METHOD_GET, const Vector<uint8_t> &p_request_data_raw = Vector<uint8_t>()); //connects to a full url and perform request
	void cancel_request();
	HTTPClient::Status get_http_client_status() const;

	void set_use_threads(bool p_use);
	bool is_using_threads() const;

	void set_accept_gzip(bool p_gzip);
	bool is_accepting_gzip() const;

	void set_download_file(const String &p_file);
	String get_download_file() const;

	void set_download_chunk_size(int p_chunk_size);
	int get_download_chunk_size() const;

	void set_body_size_limit(int p_bytes);
	int get_body_size_limit() const;

	void set_max_redirects(int p_max);
	int get_max_redirects() const;

	Timer *timer = nullptr;

	void set_timeout(double p_timeout);
	double get_timeout();

	void _timeout();

	int get_downloaded_bytes() const;
	int get_body_size() const;

	void set_http_proxy(const String &p_host, int p_port);
	void set_https_proxy(const String &p_host, int p_port);

	void set_tls_options(const Ref<TLSOptions> &p_options);

	HTTPRequest();
};

VARIANT_ENUM_CAST(HTTPRequest::Result);
