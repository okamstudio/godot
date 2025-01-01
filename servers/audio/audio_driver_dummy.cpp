/**************************************************************************/
/*  audio_driver_dummy.cpp                                                */
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

#include "audio_driver_dummy.h"

#include "core/os/os.h"

AudioDriverDummy *AudioDriverDummy::singleton = nullptr;

Error AudioDriverDummy::init() {
	active.clear();
	exit_thread.clear();
	samples_in = nullptr;

	if (mix_rate == -1) {
		mix_rate = _get_configured_mix_rate();
	}

	channels = get_channels();
	samples_in = memnew_arr(int32_t, size_t(buffer_frames) * channels);

	if (use_threads) {
		thread.start(AudioDriverDummy::thread_func, this);
	}

	return OK;
}

void AudioDriverDummy::thread_func(void *p_udata) {
	AudioDriverDummy *ad = static_cast<AudioDriverDummy *>(p_udata);

	uint64_t usdelay = (ad->buffer_frames / float(ad->mix_rate)) * 1000000;

	while (!ad->exit_thread.is_set()) {
		if (ad->active.is_set()) {
			ad->lock();
			ad->start_counting_ticks();

			ad->audio_server_process(ad->buffer_frames, ad->samples_in);

			ad->stop_counting_ticks();
			ad->unlock();
		}

		OS::get_singleton()->delay_usec(usdelay);
	}
}

void AudioDriverDummy::start() {
	active.set();
}

int AudioDriverDummy::get_mix_rate() const {
	return mix_rate;
}

AudioDriver::SpeakerMode AudioDriverDummy::get_speaker_mode() const {
	return speaker_mode;
}

void AudioDriverDummy::lock() {
	mutex.lock();
}

void AudioDriverDummy::unlock() {
	mutex.unlock();
}

void AudioDriverDummy::set_use_threads(bool p_use_threads) {
	use_threads = p_use_threads;
}

void AudioDriverDummy::set_speaker_mode(SpeakerMode p_mode) {
	speaker_mode = p_mode;
}

void AudioDriverDummy::set_mix_rate(int p_rate) {
	mix_rate = p_rate;
}

uint32_t AudioDriverDummy::get_channels() const {
	static const int channels_for_mode[4] = { 2, 4, 8, 16 };
	return channels_for_mode[speaker_mode];
}

void AudioDriverDummy::mix_audio(int p_frames, int32_t *p_buffer) {
	ERR_FAIL_COND(!active.is_set()); // If not active, should not mix.
	ERR_FAIL_COND(use_threads == true); // If using threads, this will not work well.

	uint32_t todo = p_frames;
	while (todo) {
		uint32_t to_mix = MIN(buffer_frames, todo);
		lock();
		audio_server_process(to_mix, samples_in);
		unlock();

		uint32_t total_samples = to_mix * channels;

		for (uint32_t i = 0; i < total_samples; i++) {
			p_buffer[i] = samples_in[i];
		}

		todo -= to_mix;
		p_buffer += total_samples;
	}
}

void AudioDriverDummy::finish() {
	if (use_threads) {
		exit_thread.set();
		if (thread.is_started()) {
			thread.wait_to_finish();
		}
	}

	if (samples_in) {
		memdelete_arr(samples_in);
	}
}

AudioDriverDummy::AudioDriverDummy() {
	singleton = this;
}
