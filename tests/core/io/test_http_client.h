/**************************************************************************/
/*  test_http_client.h                                                    */
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

#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"

namespace TestHTTPClient {

TEST_CASE("[HTTPClient] Instantiation") {
	Ref<HTTPClient> client = HTTPClient::create();
	CHECK_MESSAGE(client.is_valid(), "A HTTP Client created should not be a null pointer");
}

TEST_CASE("[HTTPClient] query_string_from_dict") {
	Ref<HTTPClient> client = HTTPClient::create();
	Dictionary empty_dict;
	String empty_query = client->query_string_from_dict(empty_dict);
	CHECK_MESSAGE(empty_query.is_empty(), "A empty dictionary should return a empty string");

	Dictionary dict1;
	dict1["key"] = "value";
	String single_key = client->query_string_from_dict(dict1);
	CHECK_MESSAGE(single_key == "key=value", "The query should return key=value for every string in the dictionary");

	// Check Dictionary with multiple values of different types.
	Dictionary dict2;
	dict2["key1"] = "value";
	dict2["key2"] = 123;
	Array values;
	values.push_back(1);
	values.push_back(2);
	values.push_back(3);
	dict2["key3"] = values;
	dict2["key4"] = Variant();
	String multiple_keys = client->query_string_from_dict(dict2);
	CHECK_MESSAGE(multiple_keys == "key1=value&key2=123&key3=1&key3=2&key3=3&key4",
			"The query should return key=value for every string in the dictionary. Pairs should be separated by &, arrays should be have a query for every element, and variants should have empty values");
}

TEST_CASE("[HTTPClient] verify_headers") {
	Ref<HTTPClient> client = HTTPClient::create();
	Vector<String> headers = { "Accept: text/html", "Content-Type: application/json", "Authorization: Bearer abc123" };

	Error err = client->verify_headers(headers);
	CHECK_MESSAGE(err == OK, "Expected OK for valid headers");

	ERR_PRINT_OFF;
	Vector<String> empty_header = { "" };
	err = client->verify_headers(empty_header);
	CHECK_MESSAGE(err == ERR_INVALID_PARAMETER, "Expected ERR_INVALID_PARAMETER for empty header");

	Vector<String> invalid_header = { "InvalidHeader", "Header: " };
	err = client->verify_headers(invalid_header);
	CHECK_MESSAGE(err == ERR_INVALID_PARAMETER, "Expected ERR_INVALID_PARAMETER for header with no colon");

	Vector<String> invalid_header_b = { ":", "Header: " };
	err = client->verify_headers(invalid_header_b);
	CHECK_MESSAGE(err == ERR_INVALID_PARAMETER, "Expected ERR_INVALID_PARAMETER for header with colon in first position");
	ERR_PRINT_ON;
}

#if defined(MODULE_MBEDTLS_ENABLED) || defined(WEB_ENABLED)
TEST_CASE("[HTTPClient] connect_to_host") {
	Ref<HTTPClient> client = HTTPClient::create();
	String host = "https://www.example.com";
	int port = 443;
	Ref<TLSOptions> tls_options;

	// Connect to host.
	Error err = client->connect_to_host(host, port, tls_options);
	CHECK_MESSAGE(err == OK, "Expected OK for successful connection");
}
#endif // MODULE_MBEDTLS_ENABLED || WEB_ENABLED

} // namespace TestHTTPClient
