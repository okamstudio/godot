/**************************************************************************/
/*  webrtc_peer_connection_js.h                                           */
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

#ifdef WEB_ENABLED

#include "webrtc_peer_connection.h"

extern "C" {
typedef void (*RTCOnIceConnectionStateChange)(void *p_obj, int p_state);
typedef void (*RTCOnIceGatheringStateChange)(void *p_obj, int p_state);
typedef void (*RTCOnSignalingStateChange)(void *p_obj, int p_state);
typedef void (*RTCOnIceCandidate)(void *p_obj, const char *p_mid, int p_mline_idx, const char *p_candidate);
typedef void (*RTCOnDataChannel)(void *p_obj, int p_id);
typedef void (*RTCOnSession)(void *p_obj, const char *p_type, const char *p_sdp);
typedef void (*RTCOnError)(void *p_obj);
extern int godot_js_rtc_pc_create(const char *p_config, void *p_obj, RTCOnIceConnectionStateChange p_on_connection_state_change, RTCOnIceGatheringStateChange p_on_gathering_state_change, RTCOnSignalingStateChange p_on_signaling_state_change, RTCOnIceCandidate p_on_candidate, RTCOnDataChannel p_on_datachannel);
extern void godot_js_rtc_pc_close(int p_id);
extern void godot_js_rtc_pc_destroy(int p_id);
extern void godot_js_rtc_pc_offer_create(int p_id, void *p_obj, RTCOnSession p_on_session, RTCOnError p_on_error);
extern void godot_js_rtc_pc_local_description_set(int p_id, const char *p_type, const char *p_sdp, void *p_obj, RTCOnError p_on_error);
extern void godot_js_rtc_pc_remote_description_set(int p_id, const char *p_type, const char *p_sdp, void *p_obj, RTCOnSession p_on_session, RTCOnError p_on_error);
extern void godot_js_rtc_pc_ice_candidate_add(int p_id, const char *p_mid_name, int p_mline_idx, const char *p_sdo);
extern int godot_js_rtc_pc_datachannel_create(int p_id, const char *p_label, const char *p_config);
}

class WebRTCPeerConnectionJS : public WebRTCPeerConnection {
	GDCLASS(WebRTCPeerConnectionJS, WebRTCPeerConnection);

private:
	int _js_id = 0;
	ConnectionState _conn_state = STATE_NEW;
	GatheringState _gathering_state = GATHERING_STATE_NEW;
	SignalingState _signaling_state = SIGNALING_STATE_STABLE;

	static void _on_connection_state_changed(void *p_obj, int p_state);
	static void _on_gathering_state_changed(void *p_obj, int p_state);
	static void _on_signaling_state_changed(void *p_obj, int p_state);
	static void _on_ice_candidate(void *p_obj, const char *p_mid_name, int p_mline_idx, const char *p_candidate);
	static void _on_data_channel(void *p_obj, int p_channel);
	static void _on_session_created(void *p_obj, const char *p_type, const char *p_session);
	static void _on_error(void *p_obj);

public:
	virtual ConnectionState get_connection_state() const override;
	virtual GatheringState get_gathering_state() const override;
	virtual SignalingState get_signaling_state() const override;

	virtual Error initialize(Dictionary configuration = Dictionary()) override;
	virtual Ref<WebRTCDataChannel> create_data_channel(String p_channel_name, Dictionary p_channel_config = Dictionary()) override;
	virtual Error create_offer() override;
	virtual Error set_remote_description(String type, String sdp) override;
	virtual Error set_local_description(String type, String sdp) override;
	virtual Error add_ice_candidate(String sdpMidName, int sdpMlineIndexName, String sdpName) override;
	virtual Error poll() override;
	virtual void close() override;

	WebRTCPeerConnectionJS();
	~WebRTCPeerConnectionJS();
};

#endif
