/**************************************************************************/
/*  webrtc_peer_connection.h                                              */
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

#include "webrtc_data_channel.h"

class WebRTCPeerConnection : public RefCounted {
	GDCLASS(WebRTCPeerConnection, RefCounted);

public:
	enum ConnectionState {
		STATE_NEW,
		STATE_CONNECTING,
		STATE_CONNECTED,
		STATE_DISCONNECTED,
		STATE_FAILED,
		STATE_CLOSED
	};

	enum GatheringState {
		GATHERING_STATE_NEW,
		GATHERING_STATE_GATHERING,
		GATHERING_STATE_COMPLETE,
	};

	enum SignalingState {
		SIGNALING_STATE_STABLE,
		SIGNALING_STATE_HAVE_LOCAL_OFFER,
		SIGNALING_STATE_HAVE_REMOTE_OFFER,
		SIGNALING_STATE_HAVE_LOCAL_PRANSWER,
		SIGNALING_STATE_HAVE_REMOTE_PRANSWER,
		SIGNALING_STATE_CLOSED,
	};

private:
	static StringName default_extension;

protected:
	static void _bind_methods();

public:
	static void set_default_extension(const StringName &p_name);

	virtual ConnectionState get_connection_state() const = 0;
	virtual GatheringState get_gathering_state() const = 0;
	virtual SignalingState get_signaling_state() const = 0;

	virtual Error initialize(Dictionary p_config = Dictionary()) = 0;
	virtual Ref<WebRTCDataChannel> create_data_channel(String p_label, Dictionary p_options = Dictionary()) = 0;
	virtual Error create_offer() = 0;
	virtual Error set_remote_description(String type, String sdp) = 0;
	virtual Error set_local_description(String type, String sdp) = 0;
	virtual Error add_ice_candidate(String sdpMidName, int sdpMlineIndexName, String sdpName) = 0;
	virtual Error poll() = 0;
	virtual void close() = 0;

	static WebRTCPeerConnection *create(bool p_notify_postinitialize = true);

	WebRTCPeerConnection();
	~WebRTCPeerConnection();
};

VARIANT_ENUM_CAST(WebRTCPeerConnection::ConnectionState);
VARIANT_ENUM_CAST(WebRTCPeerConnection::GatheringState);
VARIANT_ENUM_CAST(WebRTCPeerConnection::SignalingState);
