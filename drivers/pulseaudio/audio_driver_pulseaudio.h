/**************************************************************************/
/*  audio_driver_pulseaudio.h                                             */
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

#ifdef PULSEAUDIO_ENABLED

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio_server.h"

#ifdef SOWRAP_ENABLED
#include "pulse-so_wrap.h"
#else
#include <pulse/pulseaudio.h>
#endif

class AudioDriverPulseAudio : public AudioDriver {
	Thread thread;
	Mutex mutex;

	pa_mainloop *pa_ml = nullptr;
	pa_context *pa_ctx = nullptr;
	pa_stream *pa_str = nullptr;
	pa_stream *pa_rec_str = nullptr;
	pa_channel_map pa_map = {};
	pa_channel_map pa_rec_map = {};

	String output_device_name = "Default";
	String new_output_device = "Default";
	String default_output_device;

	String input_device_name;
	String new_input_device;
	String default_input_device;

	Vector<int32_t> samples_in;
	Vector<int16_t> samples_out;

	unsigned int mix_rate = 0;
	unsigned int buffer_frames = 0;
	unsigned int pa_buffer_size = 0;
	int channels = 0;
	int pa_ready = 0;
	int pa_status = 0;
	PackedStringArray pa_devices;
	PackedStringArray pa_rec_devices;

	SafeFlag active;
	SafeFlag exit_thread;

	float latency = 0;

	static void pa_state_cb(pa_context *c, void *userdata);
	static void pa_sink_info_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
	static void pa_source_info_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
	static void pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdata);
	static void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
	static void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);

	Error init_output_device();
	void finish_output_device();

	Error init_input_device();
	void finish_input_device();

	Error detect_channels(bool capture = false);

	static void thread_func(void *p_udata);

public:
	virtual const char *get_name() const override {
		return "PulseAudio";
	}

	virtual Error init() override;
	virtual void start() override;
	virtual int get_mix_rate() const override;
	virtual SpeakerMode get_speaker_mode() const override;
	virtual float get_latency() override;

	virtual void lock() override;
	virtual void unlock() override;
	virtual void finish() override;

	virtual PackedStringArray get_output_device_list() override;
	virtual String get_output_device() override;
	virtual void set_output_device(const String &p_name) override;

	virtual Error input_start() override;
	virtual Error input_stop() override;

	virtual PackedStringArray get_input_device_list() override;
	virtual String get_input_device() override;
	virtual void set_input_device(const String &p_name) override;

	AudioDriverPulseAudio();
	~AudioDriverPulseAudio() {}
};

#endif // PULSEAUDIO_ENABLED
