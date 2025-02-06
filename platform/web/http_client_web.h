/**************************************************************************/
/*  http_client_web.h                                                     */
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum {
	GODOT_JS_FETCH_STATE_REQUESTING = 0,
	GODOT_JS_FETCH_STATE_BODY = 1,
	GODOT_JS_FETCH_STATE_DONE = 2,
	GODOT_JS_FETCH_STATE_ERROR = -1,
} godot_js_fetch_state_t;

extern int godot_js_fetch_create(const char *p_method, const char *p_url, const char **p_headers, int p_headers_len, const uint8_t *p_body, int p_body_len);
extern int godot_js_fetch_read_headers(int p_id, void (*parse_callback)(int p_size, const char **p_headers, void *p_ref), void *p_ref);
extern int godot_js_fetch_read_chunk(int p_id, uint8_t *p_buf, int p_buf_size);
extern void godot_js_fetch_free(int p_id);
extern godot_js_fetch_state_t godot_js_fetch_state_get(int p_id);
extern int godot_js_fetch_http_status_get(int p_id);
extern int godot_js_fetch_is_chunked(int p_id);

#ifdef __cplusplus
}
#endif

class HTTPClientWeb : public HTTPClient {
private:
	int js_id = 0;
	Status status = STATUS_DISCONNECTED;

	// 64 KiB by default (favors fast download speeds at the cost of memory usage).
	int read_limit = 65536;

	String host;
	int port = -1;
	bool use_tls = false;

	int polled_response_code = 0;
	Vector<String> response_headers;
	Vector<uint8_t> response_buffer;

#ifdef DEBUG_ENABLED
	uint64_t last_polling_frame = 0;
#endif

	static void _parse_headers(int p_len, const char **p_headers, void *p_ref);

public:
	static HTTPClient *_create_func(bool p_notify_postinitialize);

	Error request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_body_size) override;

	Error connect_to_host(const String &p_host, int p_port = -1, Ref<TLSOptions> p_tls_options = Ref<TLSOptions>()) override;
	void set_connection(const Ref<StreamPeer> &p_connection) override;
	Ref<StreamPeer> get_connection() const override;
	void close() override;
	Status get_status() const override;
	bool has_response() const override;
	bool is_response_chunked() const override;
	int get_response_code() const override;
	Error get_response_headers(List<String> *r_response) override;
	int64_t get_response_body_length() const override;
	PackedByteArray read_response_body_chunk() override;
	void set_blocking_mode(bool p_enable) override;
	bool is_blocking_mode_enabled() const override;
	void set_read_chunk_size(int p_size) override;
	int get_read_chunk_size() const override;
	Error poll() override;
	HTTPClientWeb();
	~HTTPClientWeb();
};
