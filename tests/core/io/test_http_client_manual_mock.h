/**************************************************************************/
/*  test_http_client_manual_mock.h                                        */
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

#ifndef TEST_HTTP_CLIENT_MANUAL_MOCK_H
#define TEST_HTTP_CLIENT_MANUAL_MOCK_H

#include "core/io/http_client.h"

#include "thirdparty/doctest/doctest.h"

class HTTPClientManualMock : public HTTPClient {
public:
	static HTTPClientManualMock *current_instance;

	static HTTPClient *_create_func(bool p_notify_postinitialize = true) {
		current_instance = static_cast<HTTPClientManualMock *>(ClassDB::creator<HTTPClientManualMock>(p_notify_postinitialize));
		return current_instance;
	}

	static HTTPClient *(*_old_create)(bool);
	static void make_current() {
		_old_create = HTTPClient::_create;
		HTTPClient::_create = _create_func;
	}
	static void reset_current() {
		if (_old_create) {
			HTTPClient::_create = _old_create;
		}
	}

	Vector<Status> get_status_return;

	int set_read_chunk_size_p_size_parameter = 0;
	int set_read_chunk_size_call_count = 0;

	String connect_to_host_p_host_parameter;
	int connect_to_host_p_port_parameter = 0;
	Ref<TLSOptions> connect_to_host_p_tls_options_parameter = nullptr;
	Error connect_to_host_return = Error::OK;
	int connect_to_host_call_count = 0;

	int close_call_count = 0;

	int get_response_code_return = 0;

	bool has_response_return = false;

	Method request_p_method_parameter = Method::METHOD_GET;
	String request_p_url_parameter;
	Vector<String> request_p_headers_parameter;
	uint8_t *request_p_body_parameter = nullptr;
	int request_p_body_size_parameter = 0;
	int request_call_count = 0;
	Error request_return = Error::OK;

	List<String> get_response_headers_r_response_parameter;
	Error get_response_headers_return = Error::OK;

	int64_t get_response_body_length_return;

	PackedByteArray read_response_body_chunk_return;
#ifdef THREADS_ENABLED
	Semaphore *read_response_body_chunk_semaphore = nullptr;
#endif // THREADS_ENABLED

	Error poll_return;

	Error request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_body_size) override {
		request_p_method_parameter = p_method;
		request_p_url_parameter = p_url;
		request_p_headers_parameter = p_headers;
		request_p_body_parameter = const_cast<uint8_t *>(p_body);
		request_p_body_size_parameter = p_body_size;
		request_call_count++;
		return request_return;
	}
	Error connect_to_host(const String &p_host, int p_port = -1, Ref<TLSOptions> p_tls_options = Ref<TLSOptions>()) override {
		connect_to_host_p_host_parameter = p_host;
		connect_to_host_p_port_parameter = p_port;
		connect_to_host_p_tls_options_parameter = p_tls_options;
		connect_to_host_call_count++;
		return connect_to_host_return;
	}

	void set_connection(const Ref<StreamPeer> &p_connection) override {}
	Ref<StreamPeer> get_connection() const override { return Ref<StreamPeer>(); }

	void close() override {
		close_call_count++;
	}

	Status get_status() const override {
		if (get_status_return.size() == 0) {
			FAIL("Call to HTTPClient::get_status not set. Please set a return value.");
			return Status();
		}

		Status status = get_status_return[get_status_return_current];
		if (get_status_return_current + 1 < get_status_return.size()) {
			get_status_return_current++;
		}
		return status;
	}

	bool has_response() const override { return has_response_return; }
	bool is_response_chunked() const override { return true; }
	int get_response_code() const override { return get_response_code_return; }
	Error get_response_headers(List<String> *r_response) override {
		*r_response = get_response_headers_r_response_parameter;
		(void)r_response;
		return get_response_headers_return;
	}
	int64_t get_response_body_length() const override {
		return get_response_body_length_return;
	}

	PackedByteArray read_response_body_chunk() override {
#ifdef THREADS_ENABLED
		if (read_response_body_chunk_semaphore != nullptr) {
			read_response_body_chunk_semaphore->post();
		}
#endif // THREADS_ENABLED
		return read_response_body_chunk_return;
	}

	void set_blocking_mode(bool p_enable) override {}
	bool is_blocking_mode_enabled() const override { return true; }

	void set_read_chunk_size(int p_size) override {
		set_read_chunk_size_p_size_parameter = p_size;
		set_read_chunk_size_call_count++;
	}
	int get_read_chunk_size() const override { return 0; }

	Error poll() override {
		return poll_return;
	}

	HTTPClientManualMock() {}

private:
	// This MUST be mutable because I need to update its value from a const method (the mock method)
	mutable Vector<Status>::Size get_status_return_current = 0;
};

#endif // TEST_HTTP_CLIENT_MANUAL_MOCK_H
