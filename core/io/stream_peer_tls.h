/**************************************************************************/
/*  stream_peer_tls.h                                                     */
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

#include "core/crypto/crypto.h"
#include "core/io/stream_peer.h"

class StreamPeerTLS : public StreamPeer {
	GDCLASS(StreamPeerTLS, StreamPeer);

protected:
	static StreamPeerTLS *(*_create)(bool p_notify_postinitialize);
	static void _bind_methods();

public:
	enum Status {
		STATUS_DISCONNECTED,
		STATUS_HANDSHAKING,
		STATUS_CONNECTED,
		STATUS_ERROR,
		STATUS_ERROR_HOSTNAME_MISMATCH
	};

	virtual void poll() = 0;
	virtual Error accept_stream(Ref<StreamPeer> p_base, Ref<TLSOptions> p_options) = 0;
	virtual Error connect_to_stream(Ref<StreamPeer> p_base, const String &p_common_name, Ref<TLSOptions> p_options) = 0;
	virtual Status get_status() const = 0;
	virtual Ref<StreamPeer> get_stream() const = 0;

	virtual void disconnect_from_stream() = 0;

	static StreamPeerTLS *create(bool p_notify_postinitialize = true);

	static bool is_available();

	StreamPeerTLS() {}
};

VARIANT_ENUM_CAST(StreamPeerTLS::Status);
