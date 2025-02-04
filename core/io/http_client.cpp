/**************************************************************************/
/*  http_client.cpp                                                       */
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

#include "http_client.h"

const char *HTTPClient::_methods[METHOD_MAX] = {
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"OPTIONS",
	"TRACE",
	"CONNECT",
	"PATCH"
};

HTTPClient *HTTPClient::create(bool p_notify_postinitialize) {
	if (_create) {
		return _create(p_notify_postinitialize);
	}
	return nullptr;
}

void HTTPClient::set_http_proxy(const String &p_host, int p_port) {
	WARN_PRINT("HTTP proxy feature is not available");
}

void HTTPClient::set_https_proxy(const String &p_host, int p_port) {
	WARN_PRINT("HTTPS proxy feature is not available");
}

Error HTTPClient::_request_raw(Method p_method, const String &p_url, const Vector<String> &p_headers, const Vector<uint8_t> &p_body) {
	int size = p_body.size();
	return request(p_method, p_url, p_headers, size > 0 ? p_body.ptr() : nullptr, size);
}

Error HTTPClient::_request(Method p_method, const String &p_url, const Vector<String> &p_headers, const String &p_body) {
	CharString body_utf8 = p_body.utf8();
	int size = body_utf8.length();
	return request(p_method, p_url, p_headers, size > 0 ? (const uint8_t *)body_utf8.get_data() : nullptr, size);
}

String HTTPClient::query_string_from_dict(const Dictionary &p_dict) {
	String query = "";
	Array keys = p_dict.keys();
	for (int i = 0; i < keys.size(); ++i) {
		String encoded_key = String(keys[i]).uri_encode();
		const Variant &value = p_dict[keys[i]];
		switch (value.get_type()) {
			case Variant::ARRAY: {
				// Repeat the key with every values
				Array values = value;
				for (int j = 0; j < values.size(); ++j) {
					query += "&" + encoded_key + "=" + String(values[j]).uri_encode();
				}
				break;
			}
			case Variant::NIL: {
				// Add the key with no value
				query += "&" + encoded_key;
				break;
			}
			default: {
				// Add the key-value pair
				query += "&" + encoded_key + "=" + String(value).uri_encode();
			}
		}
	}
	return query.substr(1);
}

Error HTTPClient::verify_headers(const Vector<String> &p_headers) {
	for (int i = 0; i < p_headers.size(); i++) {
		String sanitized = p_headers[i].strip_edges();
		ERR_FAIL_COND_V_MSG(sanitized.is_empty(), ERR_INVALID_PARAMETER, vformat("Invalid HTTP header at index %d: empty.", i));
		ERR_FAIL_COND_V_MSG(sanitized.find_char(':') < 1, ERR_INVALID_PARAMETER,
				vformat("Invalid HTTP header at index %d: String must contain header-value pair, delimited by ':', but was: '%s'.", i, p_headers[i]));
	}

	return OK;
}

Dictionary HTTPClient::_get_response_headers_as_dictionary() {
	List<String> rh;
	get_response_headers(&rh);
	Dictionary ret;
	for (const String &s : rh) {
		int sp = s.find_char(':');
		if (sp == -1) {
			continue;
		}
		String key = s.substr(0, sp).strip_edges();
		String value = s.substr(sp + 1).strip_edges();
		ret[key] = value;
	}

	return ret;
}

PackedStringArray HTTPClient::_get_response_headers() {
	List<String> rh;
	get_response_headers(&rh);
	PackedStringArray ret;
	ret.resize(rh.size());
	int idx = 0;
	for (const String &E : rh) {
		ret.set(idx++, E);
	}

	return ret;
}

void HTTPClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_host", "host", "port", "tls_options"), &HTTPClient::connect_to_host, DEFVAL(-1), DEFVAL(Ref<TLSOptions>()));
	ClassDB::bind_method(D_METHOD("set_connection", "connection"), &HTTPClient::set_connection);
	ClassDB::bind_method(D_METHOD("get_connection"), &HTTPClient::get_connection);
	ClassDB::bind_method(D_METHOD("request_raw", "method", "url", "headers", "body"), &HTTPClient::_request_raw);
	ClassDB::bind_method(D_METHOD("request", "method", "url", "headers", "body"), &HTTPClient::_request, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("close"), &HTTPClient::close);

	ClassDB::bind_method(D_METHOD("has_response"), &HTTPClient::has_response);
	ClassDB::bind_method(D_METHOD("is_response_chunked"), &HTTPClient::is_response_chunked);
	ClassDB::bind_method(D_METHOD("get_response_code"), &HTTPClient::get_response_code);
	ClassDB::bind_method(D_METHOD("get_response_headers"), &HTTPClient::_get_response_headers);
	ClassDB::bind_method(D_METHOD("get_response_headers_as_dictionary"), &HTTPClient::_get_response_headers_as_dictionary);
	ClassDB::bind_method(D_METHOD("get_response_body_length"), &HTTPClient::get_response_body_length);
	ClassDB::bind_method(D_METHOD("read_response_body_chunk"), &HTTPClient::read_response_body_chunk);
	ClassDB::bind_method(D_METHOD("set_read_chunk_size", "bytes"), &HTTPClient::set_read_chunk_size);
	ClassDB::bind_method(D_METHOD("get_read_chunk_size"), &HTTPClient::get_read_chunk_size);

	ClassDB::bind_method(D_METHOD("set_blocking_mode", "enabled"), &HTTPClient::set_blocking_mode);
	ClassDB::bind_method(D_METHOD("is_blocking_mode_enabled"), &HTTPClient::is_blocking_mode_enabled);

	ClassDB::bind_method(D_METHOD("get_status"), &HTTPClient::get_status);
	ClassDB::bind_method(D_METHOD("poll"), &HTTPClient::poll);

	ClassDB::bind_method(D_METHOD("set_http_proxy", "host", "port"), &HTTPClient::set_http_proxy);
	ClassDB::bind_method(D_METHOD("set_https_proxy", "host", "port"), &HTTPClient::set_https_proxy);

	ClassDB::bind_method(D_METHOD("query_string_from_dict", "fields"), &HTTPClient::query_string_from_dict);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blocking_mode_enabled"), "set_blocking_mode", "is_blocking_mode_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "connection", PROPERTY_HINT_RESOURCE_TYPE, "StreamPeer", PROPERTY_USAGE_NONE), "set_connection", "get_connection");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "read_chunk_size", PROPERTY_HINT_RANGE, "256,16777216"), "set_read_chunk_size", "get_read_chunk_size");

	BIND_ENUM_CONSTANT(METHOD_GET);
	BIND_ENUM_CONSTANT(METHOD_HEAD);
	BIND_ENUM_CONSTANT(METHOD_POST);
	BIND_ENUM_CONSTANT(METHOD_PUT);
	BIND_ENUM_CONSTANT(METHOD_DELETE);
	BIND_ENUM_CONSTANT(METHOD_OPTIONS);
	BIND_ENUM_CONSTANT(METHOD_TRACE);
	BIND_ENUM_CONSTANT(METHOD_CONNECT);
	BIND_ENUM_CONSTANT(METHOD_PATCH);
	BIND_ENUM_CONSTANT(METHOD_MAX);

	BIND_ENUM_CONSTANT(STATUS_DISCONNECTED);
	BIND_ENUM_CONSTANT(STATUS_RESOLVING); // Resolving hostname (if hostname was passed in)
	BIND_ENUM_CONSTANT(STATUS_CANT_RESOLVE);
	BIND_ENUM_CONSTANT(STATUS_CONNECTING); // Connecting to IP
	BIND_ENUM_CONSTANT(STATUS_CANT_CONNECT);
	BIND_ENUM_CONSTANT(STATUS_CONNECTED); // Connected, now accepting requests
	BIND_ENUM_CONSTANT(STATUS_REQUESTING); // Request in progress
	BIND_ENUM_CONSTANT(STATUS_BODY); // Request resulted in body which must be read
	BIND_ENUM_CONSTANT(STATUS_CONNECTION_ERROR);
	BIND_ENUM_CONSTANT(STATUS_TLS_HANDSHAKE_ERROR);

	BIND_ENUM_CONSTANT(RESPONSE_CONTINUE);
	BIND_ENUM_CONSTANT(RESPONSE_SWITCHING_PROTOCOLS);
	BIND_ENUM_CONSTANT(RESPONSE_PROCESSING);

	// 2xx successful
	BIND_ENUM_CONSTANT(RESPONSE_OK);
	BIND_ENUM_CONSTANT(RESPONSE_CREATED);
	BIND_ENUM_CONSTANT(RESPONSE_ACCEPTED);
	BIND_ENUM_CONSTANT(RESPONSE_NON_AUTHORITATIVE_INFORMATION);
	BIND_ENUM_CONSTANT(RESPONSE_NO_CONTENT);
	BIND_ENUM_CONSTANT(RESPONSE_RESET_CONTENT);
	BIND_ENUM_CONSTANT(RESPONSE_PARTIAL_CONTENT);
	BIND_ENUM_CONSTANT(RESPONSE_MULTI_STATUS);
	BIND_ENUM_CONSTANT(RESPONSE_ALREADY_REPORTED);
	BIND_ENUM_CONSTANT(RESPONSE_IM_USED);

	// 3xx redirection
	BIND_ENUM_CONSTANT(RESPONSE_MULTIPLE_CHOICES);
	BIND_ENUM_CONSTANT(RESPONSE_MOVED_PERMANENTLY);
	BIND_ENUM_CONSTANT(RESPONSE_FOUND);
	BIND_ENUM_CONSTANT(RESPONSE_SEE_OTHER);
	BIND_ENUM_CONSTANT(RESPONSE_NOT_MODIFIED);
	BIND_ENUM_CONSTANT(RESPONSE_USE_PROXY);
	BIND_ENUM_CONSTANT(RESPONSE_SWITCH_PROXY);
	BIND_ENUM_CONSTANT(RESPONSE_TEMPORARY_REDIRECT);
	BIND_ENUM_CONSTANT(RESPONSE_PERMANENT_REDIRECT);

	// 4xx client error
	BIND_ENUM_CONSTANT(RESPONSE_BAD_REQUEST);
	BIND_ENUM_CONSTANT(RESPONSE_UNAUTHORIZED);
	BIND_ENUM_CONSTANT(RESPONSE_PAYMENT_REQUIRED);
	BIND_ENUM_CONSTANT(RESPONSE_FORBIDDEN);
	BIND_ENUM_CONSTANT(RESPONSE_NOT_FOUND);
	BIND_ENUM_CONSTANT(RESPONSE_METHOD_NOT_ALLOWED);
	BIND_ENUM_CONSTANT(RESPONSE_NOT_ACCEPTABLE);
	BIND_ENUM_CONSTANT(RESPONSE_PROXY_AUTHENTICATION_REQUIRED);
	BIND_ENUM_CONSTANT(RESPONSE_REQUEST_TIMEOUT);
	BIND_ENUM_CONSTANT(RESPONSE_CONFLICT);
	BIND_ENUM_CONSTANT(RESPONSE_GONE);
	BIND_ENUM_CONSTANT(RESPONSE_LENGTH_REQUIRED);
	BIND_ENUM_CONSTANT(RESPONSE_PRECONDITION_FAILED);
	BIND_ENUM_CONSTANT(RESPONSE_REQUEST_ENTITY_TOO_LARGE);
	BIND_ENUM_CONSTANT(RESPONSE_REQUEST_URI_TOO_LONG);
	BIND_ENUM_CONSTANT(RESPONSE_UNSUPPORTED_MEDIA_TYPE);
	BIND_ENUM_CONSTANT(RESPONSE_REQUESTED_RANGE_NOT_SATISFIABLE);
	BIND_ENUM_CONSTANT(RESPONSE_EXPECTATION_FAILED);
	BIND_ENUM_CONSTANT(RESPONSE_IM_A_TEAPOT);
	BIND_ENUM_CONSTANT(RESPONSE_MISDIRECTED_REQUEST);
	BIND_ENUM_CONSTANT(RESPONSE_UNPROCESSABLE_ENTITY);
	BIND_ENUM_CONSTANT(RESPONSE_LOCKED);
	BIND_ENUM_CONSTANT(RESPONSE_FAILED_DEPENDENCY);
	BIND_ENUM_CONSTANT(RESPONSE_UPGRADE_REQUIRED);
	BIND_ENUM_CONSTANT(RESPONSE_PRECONDITION_REQUIRED);
	BIND_ENUM_CONSTANT(RESPONSE_TOO_MANY_REQUESTS);
	BIND_ENUM_CONSTANT(RESPONSE_REQUEST_HEADER_FIELDS_TOO_LARGE);
	BIND_ENUM_CONSTANT(RESPONSE_UNAVAILABLE_FOR_LEGAL_REASONS);

	// 5xx server error
	BIND_ENUM_CONSTANT(RESPONSE_INTERNAL_SERVER_ERROR);
	BIND_ENUM_CONSTANT(RESPONSE_NOT_IMPLEMENTED);
	BIND_ENUM_CONSTANT(RESPONSE_BAD_GATEWAY);
	BIND_ENUM_CONSTANT(RESPONSE_SERVICE_UNAVAILABLE);
	BIND_ENUM_CONSTANT(RESPONSE_GATEWAY_TIMEOUT);
	BIND_ENUM_CONSTANT(RESPONSE_HTTP_VERSION_NOT_SUPPORTED);
	BIND_ENUM_CONSTANT(RESPONSE_VARIANT_ALSO_NEGOTIATES);
	BIND_ENUM_CONSTANT(RESPONSE_INSUFFICIENT_STORAGE);
	BIND_ENUM_CONSTANT(RESPONSE_LOOP_DETECTED);
	BIND_ENUM_CONSTANT(RESPONSE_NOT_EXTENDED);
	BIND_ENUM_CONSTANT(RESPONSE_NETWORK_AUTH_REQUIRED);
}
