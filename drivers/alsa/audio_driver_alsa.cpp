/**************************************************************************/
/*  audio_driver_alsa.cpp                                                 */
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

#include "audio_driver_alsa.h"

#ifdef ALSA_ENABLED

#include "core/config/project_settings.h"
#include "core/os/os.h"

#include <errno.h>

#if defined(PULSEAUDIO_ENABLED) && defined(SOWRAP_ENABLED)
extern "C" {
extern int initialize_pulse(int verbose);
}
#endif

Error AudioDriverALSA::init_output_device() {
	mix_rate = _get_configured_mix_rate();

	speaker_mode = SPEAKER_MODE_STEREO;
	channels = 2;

	// If there is a specified output device check that it is really present
	if (output_device_name != "Default") {
		PackedStringArray list = get_output_device_list();
		if (!list.has(output_device_name)) {
			output_device_name = "Default";
			new_output_device = "Default";
		}
	}

	int status;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;

#define CHECK_FAIL(m_cond)                                       \
	if (m_cond) {                                                \
		fprintf(stderr, "ALSA ERR: %s\n", snd_strerror(status)); \
		if (pcm_handle) {                                        \
			snd_pcm_close(pcm_handle);                           \
			pcm_handle = nullptr;                                \
		}                                                        \
		ERR_FAIL_COND_V(m_cond, ERR_CANT_OPEN);                  \
	}

	//todo, add
	//6 chans - "plug:surround51"
	//4 chans - "plug:surround40";

	if (output_device_name == "Default") {
		status = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	} else {
		String device = output_device_name;
		int pos = device.find_char(';');
		if (pos != -1) {
			device = device.substr(0, pos);
		}
		status = snd_pcm_open(&pcm_handle, device.utf8().get_data(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	}

	ERR_FAIL_COND_V(status < 0, ERR_CANT_OPEN);

	snd_pcm_hw_params_alloca(&hwparams);

	status = snd_pcm_hw_params_any(pcm_handle, hwparams);
	CHECK_FAIL(status < 0);

	status = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	CHECK_FAIL(status < 0);

	//not interested in anything else
	status = snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE);
	CHECK_FAIL(status < 0);

	//todo: support 4 and 6
	status = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2);
	CHECK_FAIL(status < 0);

	status = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &mix_rate, nullptr);
	CHECK_FAIL(status < 0);

	// In ALSA the period size seems to be the one that will determine the actual latency
	// Ref: https://www.alsa-project.org/main/index.php/FramesPeriods
	unsigned int periods = 2;
	int latency = Engine::get_singleton()->get_audio_output_latency();
	buffer_frames = closest_power_of_2(latency * mix_rate / 1000);
	buffer_size = buffer_frames * periods;
	period_size = buffer_frames;

	// set buffer size from project settings
	status = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hwparams, &buffer_size);
	CHECK_FAIL(status < 0);

	status = snd_pcm_hw_params_set_period_size_near(pcm_handle, hwparams, &period_size, nullptr);
	CHECK_FAIL(status < 0);

	print_verbose("Audio buffer frames: " + itos(period_size) + " calculated latency: " + itos(period_size * 1000 / mix_rate) + "ms");

	status = snd_pcm_hw_params_set_periods_near(pcm_handle, hwparams, &periods, nullptr);
	CHECK_FAIL(status < 0);

	status = snd_pcm_hw_params(pcm_handle, hwparams);
	CHECK_FAIL(status < 0);

	//snd_pcm_hw_params_free(&hwparams);

	snd_pcm_sw_params_alloca(&swparams);

	status = snd_pcm_sw_params_current(pcm_handle, swparams);
	CHECK_FAIL(status < 0);

	status = snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, period_size);
	CHECK_FAIL(status < 0);

	status = snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, 1);
	CHECK_FAIL(status < 0);

	status = snd_pcm_sw_params(pcm_handle, swparams);
	CHECK_FAIL(status < 0);

	samples_in.resize(period_size * channels);
	samples_out.resize(period_size * channels);

	return OK;
}

Error AudioDriverALSA::init() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
#ifdef PULSEAUDIO_ENABLED
	// On pulse enabled systems Alsa will silently use pulse.
	// It doesn't matter if this fails as that likely means there is no pulse
	initialize_pulse(dylibloader_verbose);
#endif

	if (initialize_asound(dylibloader_verbose)) {
		return ERR_CANT_OPEN;
	}
#endif
	bool ver_ok = false;
	String version = String::utf8(snd_asoundlib_version());
	Vector<String> ver_parts = version.split(".");
	if (ver_parts.size() >= 2) {
		ver_ok = ((ver_parts[0].to_int() == 1 && ver_parts[1].to_int() >= 1)) || (ver_parts[0].to_int() > 1); // 1.1.0
	}
	print_verbose(vformat("ALSA %s detected.", version));
	if (!ver_ok) {
		print_verbose("Unsupported ALSA library version!");
		return ERR_CANT_OPEN;
	}

	active.clear();
	exit_thread.clear();

	Error err = init_output_device();
	if (err == OK) {
		thread.start(AudioDriverALSA::thread_func, this);
	}

	return err;
}

void AudioDriverALSA::thread_func(void *p_udata) {
	AudioDriverALSA *ad = static_cast<AudioDriverALSA *>(p_udata);

	while (!ad->exit_thread.is_set()) {
		ad->lock();
		ad->start_counting_ticks();

		if (!ad->active.is_set()) {
			for (uint64_t i = 0; i < ad->period_size * ad->channels; i++) {
				ad->samples_out.write[i] = 0;
			}

		} else {
			ad->audio_server_process(ad->period_size, ad->samples_in.ptrw());

			for (uint64_t i = 0; i < ad->period_size * ad->channels; i++) {
				ad->samples_out.write[i] = ad->samples_in[i] >> 16;
			}
		}

		int todo = ad->period_size;
		int total = 0;

		while (todo && !ad->exit_thread.is_set()) {
			int16_t *src = (int16_t *)ad->samples_out.ptr();
			int wrote = snd_pcm_writei(ad->pcm_handle, (void *)(src + (total * ad->channels)), todo);

			if (wrote > 0) {
				total += wrote;
				todo -= wrote;
			} else if (wrote == -EAGAIN) {
				ad->stop_counting_ticks();
				ad->unlock();

				OS::get_singleton()->delay_usec(1000);

				ad->lock();
				ad->start_counting_ticks();
			} else {
				wrote = snd_pcm_recover(ad->pcm_handle, wrote, 0);
				if (wrote < 0) {
					ERR_PRINT("ALSA: Failed and can't recover: " + String(snd_strerror(wrote)));
					ad->active.clear();
					ad->exit_thread.set();
				}
			}
		}

		// User selected a new output device, finish the current one so we'll init the new device.
		if (ad->output_device_name != ad->new_output_device) {
			ad->output_device_name = ad->new_output_device;
			ad->finish_output_device();

			Error err = ad->init_output_device();
			if (err != OK) {
				ERR_PRINT("ALSA: init_output_device error");
				ad->output_device_name = "Default";
				ad->new_output_device = "Default";

				err = ad->init_output_device();
				if (err != OK) {
					ad->active.clear();
					ad->exit_thread.set();
				}
			}
		}

		ad->stop_counting_ticks();
		ad->unlock();
	}
}

void AudioDriverALSA::start() {
	active.set();
}

int AudioDriverALSA::get_mix_rate() const {
	return mix_rate;
}

AudioDriver::SpeakerMode AudioDriverALSA::get_speaker_mode() const {
	return speaker_mode;
}

PackedStringArray AudioDriverALSA::get_output_device_list() {
	PackedStringArray list;

	list.push_back("Default");

	void **hints;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
		return list;
	}

	for (void **n = hints; *n != nullptr; n++) {
		char *name = snd_device_name_get_hint(*n, "NAME");
		char *desc = snd_device_name_get_hint(*n, "DESC");

		if (name != nullptr && !strncmp(name, "plughw", 6)) {
			if (desc) {
				list.push_back(String::utf8(name) + ";" + String::utf8(desc));
			} else {
				list.push_back(String::utf8(name));
			}
		}

		if (desc != nullptr) {
			free(desc);
		}
		if (name != nullptr) {
			free(name);
		}
	}
	snd_device_name_free_hint(hints);

	return list;
}

String AudioDriverALSA::get_output_device() {
	return output_device_name;
}

void AudioDriverALSA::set_output_device(const String &p_name) {
	lock();
	new_output_device = p_name;
	unlock();
}

void AudioDriverALSA::lock() {
	mutex.lock();
}

void AudioDriverALSA::unlock() {
	mutex.unlock();
}

void AudioDriverALSA::finish_output_device() {
	if (pcm_handle) {
		snd_pcm_close(pcm_handle);
		pcm_handle = nullptr;
	}
}

void AudioDriverALSA::finish() {
	exit_thread.set();
	if (thread.is_started()) {
		thread.wait_to_finish();
	}

	finish_output_device();
}

#endif // ALSA_ENABLED
