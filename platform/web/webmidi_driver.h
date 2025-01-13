/**************************************************************************/
/*  webmidi_driver.h                                                      */
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

#ifndef WEBMIDI_DRIVER_H
#define WEBMIDI_DRIVER_H

#include "core/os/midi_driver.h"

#include "godot_js.h"
#include "godot_midi.h"

class MIDIDriverWebMidi : public MIDIDriver {
private:
	static const int MAX_EVENT_BUFFER_LENGTH = 2;
	uint8_t _event_buffer[MAX_EVENT_BUFFER_LENGTH];

public:
	// Override return type to make writing static callbacks less tedious.
	static MIDIDriverWebMidi *get_singleton();

	virtual Error open() override;
	virtual void close() override final;

	MIDIDriverWebMidi() = default;
	virtual ~MIDIDriverWebMidi();

	WASM_EXPORT static void set_input_names_callback(int p_size, const char **p_input_names);
	static void _set_input_names_callback(const Vector<String> &p_input_names);

	WASM_EXPORT static void on_midi_message(int p_device_index, int p_status, const uint8_t *p_data, int p_data_len);
	static void _on_midi_message(int p_device_index, int p_status, const PackedByteArray &p_data, int p_data_len);
};

#endif // WEBMIDI_DRIVER_H
