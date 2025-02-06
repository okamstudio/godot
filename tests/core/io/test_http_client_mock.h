/**************************************************************************/
/*  test_http_client_mock.h                                               */
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

#ifndef TEST_HTTP_CLIENT_MOCK_H
#define TEST_HTTP_CLIENT_MOCK_H

#include "core/io/http_client.h"

#include "thirdparty/cpp_mock/cpp_mock.h"

class HTTPClientMock : public HTTPClient {
public:
	static HTTPClientMock *current_instance;

	static HTTPClient *_create_func(bool p_notify_postinitialize = true) {
		current_instance = static_cast<HTTPClientMock *>(ClassDB::creator<HTTPClientMock>(p_notify_postinitialize));
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

	MockMethod(Error, request, (Method, const String &, const Vector<String> &, const uint8_t *, int));
	MockMethod(Error, connect_to_host, (const String &, int, Ref<TLSOptions>));

	MockMethod(void, set_connection, (const Ref<StreamPeer> &));
	MockConstMethod(Ref<StreamPeer>, get_connection, ());

	MockMethod(void, close, ());

	MockConstMethod(Status, get_status, ());

	MockConstMethod(bool, has_response, ());
	MockConstMethod(bool, is_response_chunked, ());
	MockConstMethod(int, get_response_code, ());
	MockMethod(Error, get_response_headers, (List<String> *));
	MockConstMethod(int64_t, get_response_body_length, ());

	MockMethod(PackedByteArray, read_response_body_chunk, ());

	MockMethod(void, set_blocking_mode, (bool));
	MockConstMethod(bool, is_blocking_mode_enabled, ());

	MockMethod(void, set_read_chunk_size, (int));
	MockConstMethod(int, get_read_chunk_size, ());

	MockMethod(Error, poll, ());
};

#endif // TEST_HTTP_CLIENT_MOCK_H
