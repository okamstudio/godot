/**************************************************************************/
/*  midi_driver.h                                                         */
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

#include "core/typedefs.h"
#include "core/variant/variant.h"

/**
 * Multi-Platform abstraction for accessing to MIDI.
 */

class MIDIDriver {
	static MIDIDriver *singleton;
	static uint8_t last_received_message;

protected:
	// Categories of message for parser logic.
	enum class MessageCategory {
		Data,
		Voice,
		SysExBegin,
		SystemCommon, // excluding System Exclusive Begin/End
		SysExEnd,
		RealTime,
	};

	// Convert midi data to InputEventMIDI and send it to Input.
	// p_data_len is the length of the buffer passed at p_data, this must be
	// at least equal to the data required by the passed message type, but
	// may be larger. Only the required data will be read.
	static void send_event(int p_device_index, uint8_t p_status,
			const uint8_t *p_data = nullptr, size_t p_data_len = 0);

	class Parser {
	public:
		Parser() = default;
		Parser(int p_device_index) :
				device_index{ p_device_index } {}
		virtual ~Parser() = default;

		// Push a byte of MIDI stream. Any completed messages will be
		// forwarded to MIDIDriver::send_event.
		void parse_fragment(uint8_t p_fragment);

		static MessageCategory category(uint8_t p_midi_fragment);

		// If the byte is a Voice Message status byte return the contained
		// channel number, otherwise zero.
		static uint8_t channel(uint8_t p_status_byte);

		// If the byte is a status byte for a message with a fixed number of
		// additional data bytes, return the number expected, otherwise zero.
		static size_t expected_data(uint8_t p_status_byte);
		static size_t expected_data(MIDIMessage p_msg_type);

		// If the fragment is a status byte return the message type
		// represented, otherwise MIDIMessage::NONE.
		static MIDIMessage status_to_msg_enum(uint8_t p_status_byte);

	private:
		int device_index = 0;

		static constexpr size_t DATA_BUFFER_SIZE = 2;

		uint8_t status_byte = 0;
		uint8_t data_buffer[DATA_BUFFER_SIZE] = { 0 };
		size_t received_data_len = 0;
		bool skipping_sys_ex = false;
	};

	PackedStringArray connected_input_names;

public:
	static MIDIDriver *get_singleton();

	MIDIDriver();
	virtual ~MIDIDriver() = default;

	virtual Error open() = 0;
	virtual void close() = 0;

	PackedStringArray get_connected_inputs() const;
};
