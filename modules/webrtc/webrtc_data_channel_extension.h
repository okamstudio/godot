/**************************************************************************/
/*  webrtc_data_channel_extension.h                                       */
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

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/gdvirtual.gen.inc"
#include "core/variant/native_ptr.h"

class WebRTCDataChannelExtension : public WebRTCDataChannel {
	GDCLASS(WebRTCDataChannelExtension, WebRTCDataChannel);

protected:
	static void _bind_methods();

public:
	EXBIND0R(Error, poll);
	EXBIND0(close);

	EXBIND1(set_write_mode, WriteMode);
	EXBIND0RC(WriteMode, get_write_mode);

	EXBIND0RC(bool, was_string_packet);

	EXBIND0RC(ChannelState, get_ready_state);
	EXBIND0RC(String, get_label);
	EXBIND0RC(bool, is_ordered);
	EXBIND0RC(int, get_id);
	EXBIND0RC(int, get_max_packet_life_time);
	EXBIND0RC(int, get_max_retransmits);
	EXBIND0RC(String, get_protocol);
	EXBIND0RC(bool, is_negotiated);
	EXBIND0RC(int, get_buffered_amount);

	/** Inherited from PacketPeer: **/
	EXBIND0RC(int, get_available_packet_count);
	EXBIND0RC(int, get_max_packet_size);
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override; ///< buffer is GONE after next get_packet
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;

	/** GDExtension **/
	GDVIRTUAL2R(Error, _get_packet, GDExtensionConstPtr<const uint8_t *>, GDExtensionPtr<int>);
	GDVIRTUAL2R(Error, _put_packet, GDExtensionConstPtr<const uint8_t>, int);

	WebRTCDataChannelExtension() {}
};
