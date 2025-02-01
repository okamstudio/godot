/**************************************************************************/
/*  audio_driver_wasapi.h                                                 */
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

#ifdef WASAPI_ENABLED

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio_server.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class AudioDriverWASAPI : public AudioDriver {
	class AudioDeviceWASAPI {
	public:
		IAudioClient *audio_client = nullptr;
		IAudioRenderClient *render_client = nullptr; // Output
		IAudioCaptureClient *capture_client = nullptr; // Input
		SafeFlag active;

		WORD format_tag = 0;
		WORD bits_per_sample = 0;
		unsigned int channels = 0;
		unsigned int frame_size = 0;

		String device_name = "Default"; // Output OR Input
		String new_device = "Default"; // Output OR Input

		AudioDeviceWASAPI() {}
	};

	AudioDeviceWASAPI audio_input;
	AudioDeviceWASAPI audio_output;

	Mutex mutex;
	Thread thread;

	Vector<int32_t> samples_in;

	unsigned int channels = 0;
	int mix_rate = 0;
	int buffer_frames = 0;
	int target_latency_ms = 0;
	float real_latency = 0.0;
	bool using_audio_client_3 = false;

	SafeFlag exit_thread;

	static _FORCE_INLINE_ void write_sample(WORD format_tag, int bits_per_sample, BYTE *buffer, int i, int32_t sample);
	static _FORCE_INLINE_ int32_t read_sample(WORD format_tag, int bits_per_sample, BYTE *buffer, int i);
	static void thread_func(void *p_udata);

	Error init_output_device(bool p_reinit = false);
	Error init_input_device(bool p_reinit = false);

	Error finish_output_device();
	Error finish_input_device();

	Error audio_device_init(AudioDeviceWASAPI *p_device, bool p_input, bool p_reinit, bool p_no_audio_client_3 = false);
	Error audio_device_finish(AudioDeviceWASAPI *p_device);
	PackedStringArray audio_device_get_list(bool p_input);

public:
	virtual const char *get_name() const override {
		return "WASAPI";
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

	AudioDriverWASAPI();
};

#endif // WASAPI_ENABLED
