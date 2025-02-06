/**************************************************************************/
/*  webrtc_data_channel_js.cpp                                            */
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

#include "webrtc_data_channel_js.h"

#ifdef WEB_ENABLED

#include <emscripten.h>

extern "C" { // NOLINTBEGIN(modernize-use-using)
typedef void (*RTCChOnOpen)(void *p_obj);
typedef void (*RTCChOnMessage)(void *p_obj, const uint8_t *p_buffer, int p_size, int p_is_string);
typedef void (*RTCChOnClose)(void *p_obj);
typedef void (*RTCChOnError)(void *p_obj);

extern int godot_js_rtc_datachannel_ready_state_get(int p_id);
extern int godot_js_rtc_datachannel_send(int p_id, const uint8_t *p_buffer, int p_length, int p_raw);
extern int godot_js_rtc_datachannel_is_ordered(int p_id);
extern int godot_js_rtc_datachannel_id_get(int p_id);
extern int godot_js_rtc_datachannel_max_packet_lifetime_get(int p_id);
extern int godot_js_rtc_datachannel_max_retransmits_get(int p_id);
extern int godot_js_rtc_datachannel_is_negotiated(int p_id);
extern int godot_js_rtc_datachannel_get_buffered_amount(int p_id);
extern char *godot_js_rtc_datachannel_label_get(int p_id); // Must free the returned string.
extern char *godot_js_rtc_datachannel_protocol_get(int p_id); // Must free the returned string.
extern void godot_js_rtc_datachannel_destroy(int p_id);
extern void godot_js_rtc_datachannel_connect(int p_id, void *p_obj, RTCChOnOpen p_on_open, RTCChOnMessage p_on_message, RTCChOnError p_on_error, RTCChOnClose p_on_close);
extern void godot_js_rtc_datachannel_close(int p_id);
} // NOLINTEND(modernize-use-using)

void WebRTCDataChannelJS::_on_open(void *p_obj) {
	WebRTCDataChannelJS *peer = static_cast<WebRTCDataChannelJS *>(p_obj);
	peer->in_buffer.resize(peer->_in_buffer_shift);
}

void WebRTCDataChannelJS::_on_close(void *p_obj) {
	WebRTCDataChannelJS *peer = static_cast<WebRTCDataChannelJS *>(p_obj);
	peer->close();
}

void WebRTCDataChannelJS::_on_error(void *p_obj) {
	WebRTCDataChannelJS *peer = static_cast<WebRTCDataChannelJS *>(p_obj);
	peer->close();
}

void WebRTCDataChannelJS::_on_message(void *p_obj, const uint8_t *p_data, int p_size, int p_is_string) {
	WebRTCDataChannelJS *peer = static_cast<WebRTCDataChannelJS *>(p_obj);
	RingBuffer<uint8_t> &in_buffer = peer->in_buffer;

	ERR_FAIL_COND_MSG(in_buffer.space_left() < (int)(p_size + 5), "Buffer full! Dropping data.");

	uint8_t is_string = p_is_string ? 1 : 0;
	in_buffer.write((uint8_t *)&p_size, 4);
	in_buffer.write((uint8_t *)&is_string, 1);
	in_buffer.write(p_data, p_size);
	peer->queue_count++;
}

void WebRTCDataChannelJS::close() {
	in_buffer.resize(0);
	queue_count = 0;
	_was_string = false;
	godot_js_rtc_datachannel_close(_js_id);
}

Error WebRTCDataChannelJS::poll() {
	return OK;
}

WebRTCDataChannelJS::ChannelState WebRTCDataChannelJS::get_ready_state() const {
	return (ChannelState)godot_js_rtc_datachannel_ready_state_get(_js_id);
}

int WebRTCDataChannelJS::get_available_packet_count() const {
	return queue_count;
}

Error WebRTCDataChannelJS::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	ERR_FAIL_COND_V(get_ready_state() != STATE_OPEN, ERR_UNCONFIGURED);

	if (queue_count == 0) {
		return ERR_UNAVAILABLE;
	}

	uint32_t to_read = 0;
	uint32_t left = 0;
	uint8_t is_string = 0;
	r_buffer_size = 0;

	in_buffer.read((uint8_t *)&to_read, 4);
	--queue_count;
	left = in_buffer.data_left();

	if (left < to_read + 1) {
		in_buffer.advance_read(left);
		return FAILED;
	}

	in_buffer.read(&is_string, 1);
	_was_string = is_string == 1;
	in_buffer.read(packet_buffer, to_read);
	*r_buffer = packet_buffer;
	r_buffer_size = to_read;

	return OK;
}

Error WebRTCDataChannelJS::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(get_ready_state() != STATE_OPEN, ERR_UNCONFIGURED);

	int is_bin = _write_mode == WebRTCDataChannel::WRITE_MODE_BINARY ? 1 : 0;
	godot_js_rtc_datachannel_send(_js_id, p_buffer, p_buffer_size, is_bin);
	return OK;
}

int WebRTCDataChannelJS::get_max_packet_size() const {
	return 1200;
}

void WebRTCDataChannelJS::set_write_mode(WriteMode p_mode) {
	_write_mode = p_mode;
}

WebRTCDataChannel::WriteMode WebRTCDataChannelJS::get_write_mode() const {
	return _write_mode;
}

bool WebRTCDataChannelJS::was_string_packet() const {
	return _was_string;
}

String WebRTCDataChannelJS::get_label() const {
	return _label;
}

bool WebRTCDataChannelJS::is_ordered() const {
	return godot_js_rtc_datachannel_is_ordered(_js_id);
}

int WebRTCDataChannelJS::get_id() const {
	return godot_js_rtc_datachannel_id_get(_js_id);
}

int WebRTCDataChannelJS::get_max_packet_life_time() const {
	return godot_js_rtc_datachannel_max_packet_lifetime_get(_js_id);
}

int WebRTCDataChannelJS::get_max_retransmits() const {
	return godot_js_rtc_datachannel_max_retransmits_get(_js_id);
}

String WebRTCDataChannelJS::get_protocol() const {
	return _protocol;
}

bool WebRTCDataChannelJS::is_negotiated() const {
	return godot_js_rtc_datachannel_is_negotiated(_js_id);
}

int WebRTCDataChannelJS::get_buffered_amount() const {
	return godot_js_rtc_datachannel_get_buffered_amount(_js_id);
}

WebRTCDataChannelJS::WebRTCDataChannelJS() {
}

WebRTCDataChannelJS::WebRTCDataChannelJS(int js_id) {
	_js_id = js_id;

	godot_js_rtc_datachannel_connect(js_id, this, &_on_open, &_on_message, &_on_error, &_on_close);
	// Parse label
	char *label = godot_js_rtc_datachannel_label_get(js_id);
	if (label) {
		_label.parse_utf8(label);
		free(label);
	}
	char *protocol = godot_js_rtc_datachannel_protocol_get(js_id);
	if (protocol) {
		_protocol.parse_utf8(protocol);
		free(protocol);
	}
}

WebRTCDataChannelJS::~WebRTCDataChannelJS() {
	close();
	godot_js_rtc_datachannel_destroy(_js_id);
}
#endif
