/**************************************************************************/
/*  audio_driver_web.h                                                    */
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

#ifndef AUDIO_DRIVER_WEB_H
#define AUDIO_DRIVER_WEB_H

#include "godot_audio.h"
#include "godot_js.h"

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "servers/audio_server.h"

class AudioDriverWeb : public AudioDriver {
private:
	struct AudioContext {
		bool inited = false;
		float output_latency = 0.0;
		int state = -1;
		int channel_count = 0;
		int mix_rate = 0;
	};
	static AudioContext audio_context;

	float *output_rb = nullptr;
	float *input_rb = nullptr;

	int buffer_length = 0;
	int mix_rate = 0;
	int channel_count = 0;

	WASM_EXPORT static void _state_change_callback(int p_state);
	WASM_EXPORT static void _latency_update_callback(float p_latency);
	WASM_EXPORT static void _sample_playback_finished_callback(const char *p_playback_object_id);

	static AudioDriverWeb *singleton;

protected:
	void _audio_driver_process(int p_from = 0, int p_samples = 0);
	void _audio_driver_capture(int p_from = 0, int p_samples = 0);
	float *get_output_rb() const { return output_rb; }
	float *get_input_rb() const { return input_rb; }

	virtual Error create(int &p_buffer_samples, int p_channels) = 0;
	virtual void start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) = 0;
	virtual void finish_driver() {}

public:
	static bool is_available();

	virtual Error init() final;
	virtual void start() final;
	virtual void finish() final;

	virtual int get_mix_rate() const override;
	virtual SpeakerMode get_speaker_mode() const override;
	virtual float get_latency() override;

	virtual Error input_start() override;
	virtual Error input_stop() override;

	static void resume();

	// Samples.
	virtual bool is_stream_registered_as_sample(const Ref<AudioStream> &p_stream) const override;
	virtual void register_sample(const Ref<AudioSample> &p_sample) override;
	virtual void unregister_sample(const Ref<AudioSample> &p_sample) override;
	virtual void start_sample_playback(const Ref<AudioSamplePlayback> &p_playback) override;
	virtual void stop_sample_playback(const Ref<AudioSamplePlayback> &p_playback) override;
	virtual void set_sample_playback_pause(const Ref<AudioSamplePlayback> &p_playback, bool p_paused) override;
	virtual bool is_sample_playback_active(const Ref<AudioSamplePlayback> &p_playback) override;
	virtual double get_sample_playback_position(const Ref<AudioSamplePlayback> &p_playback) override;
	virtual void update_sample_playback_pitch_scale(const Ref<AudioSamplePlayback> &p_playback, float p_pitch_scale = 0.0f) override;
	virtual void set_sample_playback_bus_volumes_linear(const Ref<AudioSamplePlayback> &p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes) override;

	virtual void set_sample_bus_count(int p_count) override;
	virtual void remove_sample_bus(int p_index) override;
	virtual void add_sample_bus(int p_at_pos = -1) override;
	virtual void move_sample_bus(int p_bus, int p_to_pos) override;
	virtual void set_sample_bus_send(int p_bus, const StringName &p_send) override;
	virtual void set_sample_bus_volume_db(int p_bus, float p_volume_db) override;
	virtual void set_sample_bus_solo(int p_bus, bool p_enable) override;
	virtual void set_sample_bus_mute(int p_bus, bool p_enable) override;

	AudioDriverWeb() {}
};

#ifdef THREADS_ENABLED
class AudioDriverWorklet : public AudioDriverWeb {
private:
	enum {
		STATE_LOCK,
		STATE_PROCESS,
		STATE_SAMPLES_IN,
		STATE_SAMPLES_OUT,
		STATE_MAX,
	};
	Mutex mutex;
	Thread thread;
	bool quit = false;
	int32_t state[STATE_MAX] = { 0 };

	static void _audio_thread_func(void *p_data);

protected:
	virtual Error create(int &p_buffer_size, int p_output_channels) override;
	virtual void start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) override;
	virtual void finish_driver() override;

public:
	virtual const char *get_name() const override {
		return "AudioWorklet";
	}

	virtual void lock() override;
	virtual void unlock() override;
};

#else

class AudioDriverWorklet : public AudioDriverWeb {
private:
	static void _process_callback(int p_pos, int p_samples);
	static void _capture_callback(int p_pos, int p_samples);

	static AudioDriverWorklet *singleton;

protected:
	virtual Error create(int &p_buffer_size, int p_output_channels) override;
	virtual void start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) override;

public:
	virtual const char *get_name() const override {
		return "AudioWorklet";
	}

	virtual void lock() override {}
	virtual void unlock() override {}

	static AudioDriverWorklet *get_singleton() { return singleton; }

	AudioDriverWorklet() { singleton = this; }
};

#endif // THREADS_ENABLED

class AudioDriverScriptProcessor : public AudioDriverWeb {
private:
	static void _process_callback();

	static AudioDriverScriptProcessor *singleton;

protected:
	virtual Error create(int &p_buffer_size, int p_output_channels) override;
	virtual void start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) override;

public:
	virtual const char *get_name() const override { return "ScriptProcessor"; }

	virtual void lock() override {}
	virtual void unlock() override {}

	static AudioDriverScriptProcessor *get_singleton() { return singleton; }

	AudioDriverScriptProcessor() { singleton = this; }
};

#endif // AUDIO_DRIVER_WEB_H
