/**************************************************************************/
/*  test_http_request_manual_mock.h                                       */
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

#ifndef TEST_HTTP_REQUEST_MANUAL_MOCK_H
#define TEST_HTTP_REQUEST_MANUAL_MOCK_H

#include "scene/main/http_request.h"
#include "tests/core/io/test_http_client_manual_mock.h"
#include "tests/test_macros.h"

namespace TestHTTPRequestManualMock {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

static inline PackedStringArray build_headers() {
	return PackedStringArray();
}
template <typename... Targs>
static inline PackedStringArray build_headers(Variant item, Targs... Fargs) {
	PackedStringArray psa = build_headers(Fargs...);
	psa.push_back(item);
	return psa;
}

TEST_CASE("[Network][HTTPRequest][ManualMock] Download chunk size is set when HTTP client is disconnected") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
	int expected_value = 42;

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_DISCONNECTED });

	http_request->set_download_chunk_size(expected_value);

	CHECK_EQ(http_client->set_read_chunk_size_p_size_parameter, expected_value);

	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][ManualMock] Download chunk size is not set when HTTP client is not disconnected") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
	int expected_value = 42;

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_CONNECTED });

	ERR_PRINT_OFF;
	http_request->set_download_chunk_size(expected_value);
	ERR_PRINT_ON;

	CHECK_EQ(http_client->set_read_chunk_size_call_count, 0);

	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] Request when disconnected") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_DISCONNECTED });
	SIGNAL_WATCH(http_request, "request_completed");

	String url = "http://foo.com";
	Error error = http_request->request(url);

	SceneTree::get_singleton()->process(0);

	CHECK(http_request->is_processing_internal() == false);
	CHECK(error == Error::OK);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_CANT_CONNECT, 0, PackedStringArray(), PackedByteArray())));

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] Port") {
	HTTPClientManualMock::make_current();

	SUBCASE("URLs are parse to get the port") {
		HTTPRequest *http_request = memnew(HTTPRequest);
		SceneTree::get_singleton()->get_root()->add_child(http_request);
		HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
		int port = 8080;
		String host = "foo.com";
		String url = "http://" + host + ":" + itos(port);

		Error error = http_request->request(url);

		CHECK_EQ(http_client->connect_to_host_p_host_parameter, host);
		CHECK_EQ(http_client->connect_to_host_p_port_parameter, port);
		CHECK_EQ(http_client->connect_to_host_p_tls_options_parameter, (Ref<TLSOptions>)(nullptr));
		CHECK_EQ(http_client->connect_to_host_call_count, 1);
		CHECK(http_request->is_processing_internal());
		CHECK(error == Error::OK);

		memdelete(http_request);
	}

	SUBCASE("HTTP URLs default port") {
		HTTPRequest *http_request = memnew(HTTPRequest);
		SceneTree::get_singleton()->get_root()->add_child(http_request);
		HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
		String host = "foo.com";
		String url = "http://" + host;

		Error error = http_request->request(url);

		CHECK_EQ(http_client->connect_to_host_p_host_parameter, host);
		CHECK_EQ(http_client->connect_to_host_p_port_parameter, 80);
		CHECK_EQ(http_client->connect_to_host_p_tls_options_parameter, (Ref<TLSOptions>)(nullptr));
		CHECK_EQ(http_client->connect_to_host_call_count, 1);
		CHECK(http_request->is_processing_internal());
		CHECK(error == Error::OK);

		memdelete(http_request);
	}

	SUBCASE("HTTPS URLs default port") {
		HTTPRequest *http_request = memnew(HTTPRequest);
		SceneTree::get_singleton()->get_root()->add_child(http_request);
		HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
		Ref<TLSOptions> tls_options = TLSOptions::client();
		String host = "foo.com";
		String url = "https://" + host;

		http_request->set_tls_options(tls_options);
		Error error = http_request->request(url);

		CHECK_EQ(http_client->connect_to_host_p_host_parameter, host);
		CHECK_EQ(http_client->connect_to_host_p_port_parameter, 443);
		CHECK_EQ(http_client->connect_to_host_p_tls_options_parameter, tls_options);
		CHECK_EQ(http_client->connect_to_host_call_count, 1);
		CHECK(http_request->is_processing_internal());
		CHECK(error == Error::OK);

		memdelete(http_request);
	}

	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] Requests") {
	HTTPClientManualMock::make_current();
	String url = "http://foo.com";

	SUBCASE("Can be cancelled") {
		HTTPRequest *http_request = memnew(HTTPRequest);
		HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
		SceneTree::get_singleton()->get_root()->add_child(http_request);

		Error error = http_request->request(url);
		CHECK(error == Error::OK);

		http_request->cancel_request();
		CHECK_FALSE(http_request->is_processing_internal());

		error = http_request->request(url);
		CHECK(error == Error::OK);
		CHECK_EQ(http_client->close_call_count, 1);

		memdelete(http_request);
	}

	SUBCASE("Are cancelled when HTTPRequest node is removed from SceneTree") {
		HTTPRequest *http_request = memnew(HTTPRequest);
		HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
		SceneTree::get_singleton()->get_root()->add_child(http_request);

		Error error = http_request->request(url);
		CHECK(error == Error::OK);

		ERR_PRINT_OFF;
		error = http_request->request(url);
		ERR_PRINT_ON;
		CHECK(error == Error::ERR_BUSY);

		// This will cancel the request.
		SceneTree::get_singleton()->get_root()->remove_child(http_request);
		CHECK_FALSE(http_request->is_processing_internal());

		// This is needed to create a new request.
		SceneTree::get_singleton()->get_root()->add_child(http_request);
		error = http_request->request(url);
		CHECK(error == Error::OK);
		CHECK_EQ(http_client->close_call_count, 1);

		memdelete(http_request);
	}

	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] Timeout") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_RESOLVING });
	http_client->poll_return = Error::OK;
	SIGNAL_WATCH(http_request, "request_completed");

	http_request->set_timeout(1);
	String url = "http://foo.com";
	Error error = http_request->request(url);

	// Call process with time greater than timeout.
	SceneTree::get_singleton()->process(2);

	CHECK_EQ(http_client->request_call_count, 0);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_TIMEOUT, 0, PackedStringArray(), PackedByteArray())));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] GET Request") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_RESOLVING, HTTPClient::STATUS_CONNECTING,
			// First STATUS_CONNECTED is to send the request, second STATUS_CONNECTED is to receive request.
			HTTPClient::STATUS_CONNECTED, HTTPClient::STATUS_CONNECTED });
	http_client->get_response_code_return = HTTPClient::ResponseCode::RESPONSE_OK;
	http_client->has_response_return = true;
	SIGNAL_WATCH(http_request, "request_completed");

	String url = "http://foo.com";
	Error error = http_request->request(url);

	// Call process for each status.
	for (int i = 0; i < 4; i++) {
		SceneTree::get_singleton()->process(0);
	}

	CHECK_EQ(http_client->request_p_method_parameter, HTTPClient::Method::METHOD_GET);
	CHECK_EQ(http_client->request_p_url_parameter, String("/"));
	CHECK_EQ(http_client->request_p_headers_parameter, build_headers("Accept-Encoding: gzip, deflate"));
	CHECK_EQ(http_client->request_p_body_parameter, (uint8_t *)nullptr);
	CHECK_EQ(http_client->request_p_body_size_parameter, 0);
	CHECK_EQ(http_client->request_call_count, 1);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_SUCCESS, HTTPClient::ResponseCode::RESPONSE_OK, PackedStringArray(), PackedByteArray())));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] GET Request with body and headers") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
	PackedByteArray body = String("Godot Rules!!!").to_utf8_buffer();

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_RESOLVING, HTTPClient::STATUS_CONNECTING, HTTPClient::STATUS_CONNECTED, HTTPClient::STATUS_BODY, HTTPClient::STATUS_BODY });
	http_client->get_response_code_return = HTTPClient::ResponseCode::RESPONSE_OK;
	http_client->has_response_return = true;
	List<String> headers;
	headers.push_front("Server: Mock");
	http_client->get_response_headers_r_response_parameter = headers;
	http_client->get_response_headers_return = Error::OK;
	http_client->get_response_body_length_return = body.size();
	http_client->read_response_body_chunk_return = body;
	SIGNAL_WATCH(http_request, "request_completed");

	String url = "http://foo.com";
	Error error = http_request->request(url);

	// Call process for each status.
	for (int i = 0; i < 4; i++) {
		SceneTree::get_singleton()->process(0);
	}

	CHECK_EQ(http_client->request_p_method_parameter, HTTPClient::Method::METHOD_GET);
	CHECK_EQ(http_client->request_p_url_parameter, String("/"));
	CHECK_EQ(http_client->request_p_headers_parameter, build_headers("Accept-Encoding: gzip, deflate"));
	CHECK_EQ(http_client->request_p_body_parameter, (uint8_t *)nullptr);
	CHECK_EQ(http_client->request_p_body_size_parameter, 0);
	CHECK_EQ(http_client->request_call_count, 1);
	SIGNAL_CHECK("request_completed",
			build_array(build_array(HTTPRequest::Result::RESULT_SUCCESS, HTTPClient::ResponseCode::RESPONSE_OK, build_headers("Server: Mock"), body)));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][ManualMock] POST Request with body and headers") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
	String body("Godot Rules!!!");

	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_RESOLVING, HTTPClient::STATUS_CONNECTING,
			// First STATUS_CONNECTED is to send the request, second STATUS_CONNECTED is to receive request.
			HTTPClient::STATUS_CONNECTED, HTTPClient::STATUS_CONNECTED });
	http_client->get_response_code_return = HTTPClient::ResponseCode::RESPONSE_CREATED;
	http_client->has_response_return = true;
	SIGNAL_WATCH(http_request, "request_completed");

	String url = "http://foo.com";
	Error error = http_request->request(url, build_headers("Accept: text/json"), HTTPClient::Method::METHOD_POST, body);

	// Call process for each status.
	for (int i = 0; i < 4; i++) {
		SceneTree::get_singleton()->process(0);
	}

	CHECK_EQ(http_client->request_p_method_parameter, HTTPClient::Method::METHOD_POST);
	CHECK_EQ(http_client->request_p_url_parameter, String("/"));
	CHECK_EQ(http_client->request_p_headers_parameter, build_headers("Accept-Encoding: gzip, deflate", "Accept: text/json"));
	CHECK_EQ(http_client->request_p_body_size_parameter, body.size() - 1);
	CHECK_EQ(http_client->request_call_count, 1);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_SUCCESS, HTTPClient::ResponseCode::RESPONSE_CREATED, PackedStringArray(), PackedByteArray())));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

#ifdef THREADS_ENABLED

TEST_CASE("[Network][HTTPRequest][SceneTree][Threads][ManualMock] GET Request with body") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;
	PackedByteArray body = String("Godot Rules!!!").to_utf8_buffer();
	Semaphore *semaphore = new Semaphore();

	// HTTPClient::STATUS_DISCONNECTED is needed by HTTPRequest::set_use_threads.
	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_DISCONNECTED, HTTPClient::STATUS_RESOLVING, HTTPClient::STATUS_CONNECTING, HTTPClient::STATUS_CONNECTED, HTTPClient::STATUS_BODY, HTTPClient::STATUS_BODY });
	http_client->get_response_code_return = HTTPClient::ResponseCode::RESPONSE_OK;
	http_client->has_response_return = true;
	http_client->get_response_headers_return = Error::OK;
	http_client->get_response_body_length_return = body.size();
	http_client->read_response_body_chunk_return = body;
	http_client->read_response_body_chunk_semaphore = semaphore;
	SIGNAL_WATCH(http_request, "request_completed");

	http_request->set_use_threads(true);
	String url = "http://foo.com";
	Error error = http_request->request(url);

	// Let the thread do its job.
	semaphore->wait();

	// This is needed to get defer calls processed.
	SceneTree::get_singleton()->process(0);

	CHECK_EQ(http_client->request_p_method_parameter, HTTPClient::Method::METHOD_GET);
	CHECK_EQ(http_client->request_p_url_parameter, String("/"));
	CHECK_EQ(http_client->request_p_headers_parameter, build_headers("Accept-Encoding: gzip, deflate"));
	CHECK_EQ(http_client->request_p_body_parameter, (uint8_t *)nullptr);
	CHECK_EQ(http_client->request_p_body_size_parameter, 0);
	CHECK_EQ(http_client->request_call_count, 1);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_SUCCESS, HTTPClient::ResponseCode::RESPONSE_OK, PackedStringArray(), body)));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	http_client->read_response_body_chunk_semaphore = nullptr;
	delete semaphore;
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

TEST_CASE("[Network][HTTPRequest][SceneTree][Threads][ManualMock] Timeout") {
	HTTPClientManualMock::make_current();
	HTTPRequest *http_request = memnew(HTTPRequest);
	SceneTree::get_singleton()->get_root()->add_child(http_request);
	HTTPClientManualMock *http_client = HTTPClientManualMock::current_instance;

	// HTTPClient::STATUS_DISCONNECTED is needed by HTTPRequest::set_use_threads.
	http_client->get_status_return = Vector<HTTPClient::Status>({ HTTPClient::STATUS_DISCONNECTED, HTTPClient::STATUS_RESOLVING });
	http_client->poll_return = Error::OK;
	SIGNAL_WATCH(http_request, "request_completed");

	http_request->set_use_threads(true);
	http_request->set_timeout(1);
	String url = "http://foo.com";
	Error error = http_request->request(url);

	// Call process with time greater than timeout.
	SceneTree::get_singleton()->process(2);

	CHECK_EQ(http_client->request_call_count, 0);
	SIGNAL_CHECK("request_completed", build_array(build_array(HTTPRequest::Result::RESULT_TIMEOUT, 0, PackedStringArray(), PackedByteArray())));
	CHECK_FALSE(http_request->is_processing_internal());
	CHECK(error == Error::OK);

	SIGNAL_UNWATCH(http_request, "request_completed");
	memdelete(http_request);
	HTTPClientManualMock::reset_current();
}

#endif // THREADS_ENABLED

} // namespace TestHTTPRequestManualMock

#endif // TEST_HTTP_REQUEST_MANUAL_MOCK_H
