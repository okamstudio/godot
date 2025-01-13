/**************************************************************************/
/*  audio_server.cpp                                                      */
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

#include "audio_server.h"

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"
#include "core/math/audio_frame.h"
#include "core/os/os.h"
#include "core/string/string_name.h"
#include "core/templates/pair.h"
#include "scene/scene_string_names.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio/effects/audio_effect_compressor.h"

#include <cstring>

#ifdef TOOLS_ENABLED
#define MARK_EDITED set_edited(true);
#else
#define MARK_EDITED
#endif

AudioDriver *AudioDriver::singleton = nullptr;
AudioDriver *AudioDriver::get_singleton() {
	return singleton;
}

void AudioDriver::set_singleton() {
	singleton = this;
}

void AudioDriver::audio_server_process(int p_frames, int32_t *p_buffer, bool p_update_mix_time) {
	if (p_update_mix_time) {
		update_mix_time(p_frames);
	}

	if (AudioServer::get_singleton()) {
		AudioServer::get_singleton()->_driver_process(p_frames, p_buffer);
	}
}

void AudioDriver::update_mix_time(int p_frames) {
	_last_mix_frames = p_frames;
	if (OS::get_singleton()) {
		_last_mix_time = OS::get_singleton()->get_ticks_usec();
	}
}

double AudioDriver::get_time_since_last_mix() {
	lock();
	uint64_t last_mix_time = _last_mix_time;
	unlock();
	return (OS::get_singleton()->get_ticks_usec() - last_mix_time) / 1000000.0;
}

double AudioDriver::get_time_to_next_mix() {
	lock();
	uint64_t last_mix_time = _last_mix_time;
	uint64_t last_mix_frames = _last_mix_frames;
	unlock();
	double total = (OS::get_singleton()->get_ticks_usec() - last_mix_time) / 1000000.0;
	double mix_buffer = last_mix_frames / (double)get_mix_rate();
	return mix_buffer - total;
}

void AudioDriver::input_buffer_init(int driver_buffer_frames) {
	const int input_buffer_channels = 2;
	input_buffer.resize(driver_buffer_frames * input_buffer_channels * 4);
	input_position = 0;
	input_size = 0;
}

void AudioDriver::input_buffer_write(int32_t sample) {
	if ((int)input_position < input_buffer.size()) {
		input_buffer.write[input_position++] = sample;
		if ((int)input_position >= input_buffer.size()) {
			input_position = 0;
		}
		if ((int)input_size < input_buffer.size()) {
			input_size++;
		}
	} else {
		WARN_PRINT("input_buffer_write: Invalid input_position=" + itos(input_position) + " input_buffer.size()=" + itos(input_buffer.size()));
	}
}

int AudioDriver::_get_configured_mix_rate() {
	StringName audio_driver_setting = "audio/driver/mix_rate";
	int mix_rate = GLOBAL_GET(audio_driver_setting);

#ifdef WEB_ENABLED
	// `0` is an acceptable value (resorts to the browser's default).
	return MAX(0, mix_rate);
#else // !WEB_ENABLED
	// In the case of invalid mix rate, let's default to a sensible value..
	if (mix_rate <= 0) {
		WARN_PRINT(vformat("Invalid mix rate of %d, consider reassigning setting \'%s\'. \nDefaulting mix rate to value %d.",
				mix_rate, audio_driver_setting, AudioDriverManager::DEFAULT_MIX_RATE));
		mix_rate = AudioDriverManager::DEFAULT_MIX_RATE;
	}
	return mix_rate;
#endif
}

AudioDriver::SpeakerMode AudioDriver::get_speaker_mode_by_total_channels(int p_channels) const {
	switch (p_channels) {
		case 4:
			return SPEAKER_SURROUND_31;
		case 6:
			return SPEAKER_SURROUND_51;
		case 8:
			return SPEAKER_SURROUND_71;
	}

	// Default to STEREO
	return SPEAKER_MODE_STEREO;
}

int AudioDriver::get_total_channels_by_speaker_mode(AudioDriver::SpeakerMode p_mode) const {
	switch (p_mode) {
		case SPEAKER_MODE_STEREO:
			return 2;
		case SPEAKER_SURROUND_31:
			return 4;
		case SPEAKER_SURROUND_51:
			return 6;
		case SPEAKER_SURROUND_71:
			return 8;
	}

	ERR_FAIL_V(2);
}

PackedStringArray AudioDriver::get_output_device_list() {
	PackedStringArray list;

	list.push_back("Default");

	return list;
}

String AudioDriver::get_output_device() {
	return "Default";
}

PackedStringArray AudioDriver::get_input_device_list() {
	PackedStringArray list;

	list.push_back("Default");

	return list;
}

void AudioDriver::start_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	if (p_playback.is_valid()) {
		if (p_playback->stream.is_valid()) {
			WARN_PRINT_ED(vformat(R"(Trying to play stream (%s) as a sample (%s), but the driver doesn't support sample playback.)", p_playback->get_instance_id(), p_playback->stream->get_instance_id()));
		} else {
			WARN_PRINT_ED(vformat(R"(Trying to play stream (%s) as a null sample, but the driver doesn't support sample playback.)", p_playback->get_instance_id()));
		}
	} else {
		WARN_PRINT_ED("Trying to play a null sample playback from a driver that don't support sample playback.");
	}
}

AudioDriverDummy AudioDriverManager::dummy_driver;
AudioDriver *AudioDriverManager::drivers[MAX_DRIVERS] = {
	&AudioDriverManager::dummy_driver,
};
int AudioDriverManager::driver_count = 1;

void AudioDriverManager::add_driver(AudioDriver *p_driver) {
	ERR_FAIL_COND(driver_count >= MAX_DRIVERS);
	drivers[driver_count - 1] = p_driver;

	// Last driver is always our dummy driver
	drivers[driver_count++] = &AudioDriverManager::dummy_driver;
}

int AudioDriverManager::get_driver_count() {
	return driver_count;
}

void AudioDriverManager::initialize(int p_driver) {
	GLOBAL_DEF_RST("audio/driver/enable_input", false);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "audio/driver/mix_rate", PROPERTY_HINT_RANGE, "11025,192000,1,or_greater,suffix:Hz"), DEFAULT_MIX_RATE);
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "audio/driver/mix_rate.web", PROPERTY_HINT_RANGE, "0,192000,1,or_greater,suffix:Hz"), 0); // Safer default output_latency for web (use browser default).

	int failed_driver = -1;

	// Check if there is a selected driver
	if (p_driver >= 0 && p_driver < driver_count) {
		if (drivers[p_driver]->init() == OK) {
			drivers[p_driver]->set_singleton();
			return;
		} else {
			failed_driver = p_driver;
		}
	}

	// No selected driver, try them all in order
	for (int i = 0; i < driver_count; i++) {
		// Don't re-init the driver if it failed above
		if (i == failed_driver) {
			continue;
		}

		if (drivers[i]->init() == OK) {
			drivers[i]->set_singleton();
			break;
		}
	}

	if (driver_count > 1 && String(AudioDriver::get_singleton()->get_name()) == "Dummy") {
		WARN_PRINT("All audio drivers failed, falling back to the dummy driver.");
	}
}

AudioDriver *AudioDriverManager::get_driver(int p_driver) {
	ERR_FAIL_INDEX_V(p_driver, driver_count, nullptr);
	return drivers[p_driver];
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

void AudioServer::_driver_process(int p_frames, int32_t *p_buffer) {
	mix_count++;
	int todo = p_frames;

#ifdef DEBUG_ENABLED
	uint64_t prof_ticks = OS::get_singleton()->get_ticks_usec();
#endif

	if (channel_count != get_channel_count()) {
		// Amount of channels changed due to a output_device change
		// reinitialize the buses channels and buffers
		init_channels_and_buffers();
	}

	ERR_FAIL_COND_MSG(buses.is_empty() && todo, "AudioServer bus count is less than 1.");
	while (todo) {
		if (to_mix == 0) {
			_mix_step();
		}

		int to_copy = MIN(to_mix, todo);

		Bus *master = buses[0];

		int from = buffer_size - to_mix;
		int from_buf = p_frames - todo;

		//master master, send to output
		int cs = master->channels.size();

		// Take away 1 from the stride, as we are manually incrementing by 1 for stereo.
		uintptr_t stride_minus_one = (cs * 2) - 1;

		for (int k = 0; k < cs; k++) {
			// The destination start for data will be the same in all cases.
			int32_t *dest = &p_buffer[from_buf * (cs * 2) + (k * 2)];

			if (master->channels[k].active) {
				const AudioFrame *buf = master->channels[k].buffer.ptr();

				for (int j = 0; j < to_copy; j++) {
					float l = CLAMP(buf[from + j].left, -1.0, 1.0);
					int32_t vl = l * ((1 << 20) - 1);
					int32_t vl2 = (vl < 0 ? -1 : 1) * (ABS(vl) << 11);
					*dest = vl2;
					dest++;

					float r = CLAMP(buf[from + j].right, -1.0, 1.0);
					int32_t vr = r * ((1 << 20) - 1);
					int32_t vr2 = (vr < 0 ? -1 : 1) * (ABS(vr) << 11);
					*dest = vr2;
					dest += stride_minus_one;
				}

			} else {
				// Bizarrely, profiling indicates that detecting the common case of cs == 1,
				// k == 0, and using memset is SLOWER than setting them individually.
				// Perhaps it gets optimized to a faster instruction than memset.
				for (int j = 0; j < to_copy; j++) {
					*dest = 0;
					dest++;
					*dest = 0;
					dest += stride_minus_one;
				}
			}
		}

		todo -= to_copy;
		to_mix -= to_copy;
	}

#ifdef DEBUG_ENABLED
	prof_time.add(OS::get_singleton()->get_ticks_usec() - prof_ticks);
#endif
}

void AudioServer::_mix_step() {
	bool solo_mode = false;

	for (int i = 0; i < buses.size(); i++) {
		Bus *bus = buses[i];
		bus->index_cache = i; //might be moved around by editor, so..
		for (int k = 0; k < bus->channels.size(); k++) {
			bus->channels.write[k].used = false;
		}

		if (bus->solo) {
			//solo chain
			solo_mode = true;
			bus->soloed = true;
			do {
				if (bus != buses[0]) {
					//everything has a send save for master bus
					if (!bus_map.has(bus->send)) {
						bus = buses[0]; //send to master
					} else {
						int prev_index_cache = bus->index_cache;
						bus = bus_map[bus->send];
						if (prev_index_cache >= bus->index_cache) { //invalid, send to master
							bus = buses[0];
						}
					}

					bus->soloed = true;
				} else {
					bus = nullptr;
				}

			} while (bus);
		} else {
			bus->soloed = false;
		}
	}
	// This is legacy code from 3.x that allows video players and other audio sources that do not implement AudioStreamPlayback to output audio.
	for (CallbackItem *ci : mix_callback_list) {
		ci->callback(ci->userdata);
	}

	// Main mixing loop for audio streams.
	// The basic idea here is to copy the samples returned by the AudioStreamPlayback's mix function into the audio buffers,
	//  while always maintaining a lookahead buffer of size LOOKAHEAD_BUFFER_SIZE to allow fade-outs for sudden stoppages.
	for (AudioStreamPlaybackListNode *playback : playback_list) {
		// Paused streams are no-ops. Don't even mix audio from the stream playback.
		if (playback->state.load() == AudioStreamPlaybackListNode::PAUSED) {
			continue;
		}

		if (playback->stream_playback->get_is_sample()) {
			continue;
		}

		// If `fading_out` is true, we're in the process of fading out the stream playback.
		// TODO: Currently this sets the volume of the stream to 0 which creates a linear interpolation between its previous volume and silence.
		//  A more punchy option for fading out could be to just use the lookahead buffer.
		bool fading_out = playback->state.load() == AudioStreamPlaybackListNode::FADE_OUT_TO_DELETION || playback->state.load() == AudioStreamPlaybackListNode::FADE_OUT_TO_PAUSE;

		AudioFrame *buf = mix_buffer.ptrw();

		// Copy the old contents of the lookahead buffer into the beginning of the mix buffer.
		for (int i = 0; i < LOOKAHEAD_BUFFER_SIZE; i++) {
			buf[i] = playback->lookahead[i];
		}

		// Mix the audio stream.
		unsigned int mixed_frames = playback->stream_playback->mix(&buf[LOOKAHEAD_BUFFER_SIZE], playback->pitch_scale.get(), buffer_size);

		if (tag_used_audio_streams && playback->stream_playback->is_playing()) {
			playback->stream_playback->tag_used_streams();
		}

		// Check to see if the stream has run out of samples.
		if (mixed_frames != buffer_size) {
			// We know we have at least the size of our lookahead buffer for fade-out purposes.

			float fadeout_base = 0.94;
			float fadeout_coefficient = 1;
			static_assert(LOOKAHEAD_BUFFER_SIZE == 64, "Update fadeout_base and comment here if you change LOOKAHEAD_BUFFER_SIZE.");
			// 0.94 ^ 64 = 0.01906. There might still be a pop but it'll be way better than if we didn't do this.
			for (unsigned int idx = mixed_frames; idx < buffer_size; idx++) {
				fadeout_coefficient *= fadeout_base;
				buf[idx] *= fadeout_coefficient;
			}
			AudioStreamPlaybackListNode::PlaybackState new_state;
			new_state = AudioStreamPlaybackListNode::AWAITING_DELETION;
			playback->state.store(new_state);
		} else {
			// Move the last little bit of what we just mixed into our lookahead buffer for the next call to _mix_step.
			for (int i = 0; i < LOOKAHEAD_BUFFER_SIZE; i++) {
				playback->lookahead[i] = buf[buffer_size + i];
			}
		}

		// Get the bus details for this playback. This contains information about which buses the playback is assigned to and the volume of the playback on each bus.
		AudioStreamPlaybackBusDetails *bus_details_ptr = playback->bus_details.load();
		ERR_FAIL_NULL(bus_details_ptr);
		// Make a copy of the bus details so we can modify it without worrying about other threads.
		AudioStreamPlaybackBusDetails bus_details = *bus_details_ptr;

		// Mix to any active buses.
		for (int idx = 0; idx < MAX_BUSES_PER_PLAYBACK; idx++) {
			if (!bus_details.bus_active[idx]) {
				continue;
			}
			// This is the AudioServer-internal index of the bus we're mixing to in this step of the loop. Not to be confused with `idx` which is an index into `AudioStreamPlaybackBusDetails` member var arrays.
			int bus_idx = thread_find_bus_index(bus_details.bus[idx]);

			// It's important to know whether or not this bus was active in the previous mix step of this stream. If it was, we need to perform volume interpolation to avoid pops.
			int prev_bus_idx = -1;
			for (int search_idx = 0; search_idx < MAX_BUSES_PER_PLAYBACK; search_idx++) {
				if (!playback->prev_bus_details->bus_active[search_idx]) {
					continue;
				}
				// If the StringNames of the buses match, we've found the previous bus index. This indicates that this playback mixed to `prev_bus_details->bus[prev_bus_index]` in the previous mix step, which gives us a way to look up the playback's previous volume.
				if (playback->prev_bus_details->bus[search_idx].hash() == bus_details.bus[idx].hash()) {
					prev_bus_idx = search_idx;
					break;
				}
			}

			// It's now time to mix to the bus. We do this by going through each channel of the bus and mixing to it.
			//  The channels correspond to output channels of the audio device, e.g. stereo or 5.1. To reduce needless nesting, this is done with a helper method named `_mix_step_for_channel`.
			for (int channel_idx = 0; channel_idx < channel_count; channel_idx++) {
				AudioFrame *channel_buf = thread_get_channel_mix_buffer(bus_idx, channel_idx);
				// TODO: This `fading_out` check could be replaced with with an exponential fadeout of the samples from the lookahead buffer for more punchy results.
				if (fading_out) {
					bus_details.volume[idx][channel_idx] = AudioFrame(0, 0);
				}
				AudioFrame channel_vol = bus_details.volume[idx][channel_idx];

				// If this bus was not active in the previous mix step, we want to start playback at the full volume to avoid crushing transients.
				AudioFrame prev_channel_vol = channel_vol;
				// If this bus was active in the previous mix step, we need to interpolate between the previous volume and the current volume to avoid pops. Set `prev_channel_volume` accordingly.
				if (prev_bus_idx != -1) {
					prev_channel_vol = playback->prev_bus_details->volume[prev_bus_idx][channel_idx];
				}
				_mix_step_for_channel(channel_buf, buf, prev_channel_vol, channel_vol, playback->attenuation_filter_cutoff_hz.get(), playback->highshelf_gain.get(), &playback->filter_process[channel_idx * 2], &playback->filter_process[channel_idx * 2 + 1]);
			}
		}

		// Now go through and fade-out any buses that were being played to previously that we missed by going through current data.
		for (int idx = 0; idx < MAX_BUSES_PER_PLAYBACK; idx++) {
			if (!playback->prev_bus_details->bus_active[idx]) {
				continue;
			}
			int bus_idx = thread_find_bus_index(playback->prev_bus_details->bus[idx]);

			int current_bus_idx = -1;
			for (int search_idx = 0; search_idx < MAX_BUSES_PER_PLAYBACK; search_idx++) {
				if (bus_details.bus[search_idx] == playback->prev_bus_details->bus[idx]) {
					current_bus_idx = search_idx;
				}
			}
			if (current_bus_idx != -1) {
				// If we found a corresponding bus in the current bus assignments, we've already mixed to this bus.
				continue;
			}

			for (int channel_idx = 0; channel_idx < channel_count; channel_idx++) {
				AudioFrame *channel_buf = thread_get_channel_mix_buffer(bus_idx, channel_idx);
				AudioFrame prev_channel_vol = playback->prev_bus_details->volume[idx][channel_idx];
				// Fade out to silence. This could be replaced with an exponential fadeout of the samples from the lookahead buffer for more punchy results.
				_mix_step_for_channel(channel_buf, buf, prev_channel_vol, AudioFrame(0, 0), playback->attenuation_filter_cutoff_hz.get(), playback->highshelf_gain.get(), &playback->filter_process[channel_idx * 2], &playback->filter_process[channel_idx * 2 + 1]);
			}
		}

		// Copy the bus details we mixed with to the previous bus details to maintain volume ramps.
		for (int i = 0; i < MAX_BUSES_PER_PLAYBACK; i++) {
			playback->prev_bus_details->bus_active[i] = bus_details.bus_active[i];
		}
		for (int i = 0; i < MAX_BUSES_PER_PLAYBACK; i++) {
			playback->prev_bus_details->bus[i] = bus_details.bus[i];
		}
		for (int i = 0; i < MAX_BUSES_PER_PLAYBACK; i++) {
			for (int j = 0; j < MAX_CHANNELS_PER_BUS; j++) {
				playback->prev_bus_details->volume[i][j] = bus_details.volume[i][j];
			}
		}

		switch (playback->state.load()) {
			case AudioStreamPlaybackListNode::AWAITING_DELETION:
			case AudioStreamPlaybackListNode::FADE_OUT_TO_DELETION:
				// Remove the playback from the list.
				_delete_stream_playback_list_node(playback);
				break;
			case AudioStreamPlaybackListNode::FADE_OUT_TO_PAUSE: {
				// Pause the stream.
				playback->state.store(AudioStreamPlaybackListNode::PAUSED);
			} break;
			case AudioStreamPlaybackListNode::PLAYING:
			case AudioStreamPlaybackListNode::PAUSED:
				// No-op!
				break;
		}
	}

	// Now that all of the buses have their audio sources mixed into them, we can process the effects and bus sends.
	for (int i = buses.size() - 1; i >= 0; i--) {
		Bus *bus = buses[i];

		for (int k = 0; k < bus->channels.size(); k++) {
			if (bus->channels[k].active && !bus->channels[k].used) {
				// Buffer was not used, but it's still active, so it must be cleaned.
				AudioFrame *buf = bus->channels.write[k].buffer.ptrw();

				for (uint32_t j = 0; j < buffer_size; j++) {
					buf[j] = AudioFrame(0, 0);
				}
			}
		}

		// Process effects.
		if (!bus->bypass) {
			for (int j = 0; j < bus->effects.size(); j++) {
				if (!bus->effects[j].enabled) {
					continue;
				}

#ifdef DEBUG_ENABLED
				uint64_t ticks = OS::get_singleton()->get_ticks_usec();
#endif

				for (int k = 0; k < bus->channels.size(); k++) {
					if (!(bus->channels[k].active || bus->channels[k].effect_instances[j]->process_silence())) {
						continue;
					}
					bus->channels.write[k].effect_instances.write[j]->process(bus->channels[k].buffer.ptr(), temp_buffer.write[k].ptrw(), buffer_size);
				}

				// Swap buffers, so internal buffer always has the right data.
				for (int k = 0; k < bus->channels.size(); k++) {
					if (!(buses[i]->channels[k].active || bus->channels[k].effect_instances[j]->process_silence())) {
						continue;
					}
					SWAP(bus->channels.write[k].buffer, temp_buffer.write[k]);
				}

#ifdef DEBUG_ENABLED
				bus->effects.write[j].prof_time += OS::get_singleton()->get_ticks_usec() - ticks;
#endif
			}
		}

		// Process send.

		Bus *send = nullptr;

		if (i > 0) {
			// Everything has a send except for the master bus.
			if (!bus_map.has(bus->send)) {
				send = buses[0];
			} else {
				send = bus_map[bus->send];
				if (send->index_cache >= bus->index_cache) { // Invalid, send to master.
					send = buses[0];
				}
			}
		}

		for (int k = 0; k < bus->channels.size(); k++) {
			if (!bus->channels[k].active) {
				bus->channels.write[k].peak_volume = AudioFrame(AUDIO_MIN_PEAK_DB, AUDIO_MIN_PEAK_DB);
				continue;
			}

			AudioFrame *buf = bus->channels.write[k].buffer.ptrw();

			AudioFrame peak = AudioFrame(0, 0);

			float volume = Math::db_to_linear(bus->volume_db);

			if (solo_mode) {
				if (!bus->soloed) {
					volume = 0.0;
				}
			} else {
				if (bus->mute) {
					volume = 0.0;
				}
			}

			// Apply volume and compute peak.
			for (uint32_t j = 0; j < buffer_size; j++) {
				buf[j] *= volume;

				float l = ABS(buf[j].left);
				if (l > peak.left) {
					peak.left = l;
				}
				float r = ABS(buf[j].right);
				if (r > peak.right) {
					peak.right = r;
				}
			}

			bus->channels.write[k].peak_volume = AudioFrame(Math::linear_to_db(peak.left + AUDIO_PEAK_OFFSET), Math::linear_to_db(peak.right + AUDIO_PEAK_OFFSET));

			if (!bus->channels[k].used) {
				// See if any audio is contained, because channel was not used.

				if (MAX(peak.right, peak.left) > Math::db_to_linear(channel_disable_threshold_db)) {
					bus->channels.write[k].last_mix_with_audio = mix_frames;
				} else if (mix_frames - bus->channels[k].last_mix_with_audio > channel_disable_frames) {
					bus->channels.write[k].active = false;
					continue; //went inactive, don't mix.
				}
			}

			if (send) {
				// If not master bus, send.
				AudioFrame *target_buf = thread_get_channel_mix_buffer(send->index_cache, k);

				for (uint32_t j = 0; j < buffer_size; j++) {
					target_buf[j] += buf[j];
				}
			}
		}
	}

	mix_frames += buffer_size;
	to_mix = buffer_size;
}

void AudioServer::_mix_step_for_channel(AudioFrame *p_out_buf, AudioFrame *p_source_buf, AudioFrame p_vol_start, AudioFrame p_vol_final, float p_attenuation_filter_cutoff_hz, float p_highshelf_gain, AudioFilterSW::Processor *p_processor_l, AudioFilterSW::Processor *p_processor_r) {
	// TODO: In the future it could be nice to replace all of these hardcoded effects with something a bit cleaner and more flexible, but for now this is what we do to support 3D audio players.
	if (p_highshelf_gain != 0) {
		AudioFilterSW filter;
		filter.set_mode(AudioFilterSW::HIGHSHELF);
		filter.set_sampling_rate(AudioServer::get_singleton()->get_mix_rate());
		filter.set_cutoff(p_attenuation_filter_cutoff_hz);
		filter.set_resonance(1);
		filter.set_stages(1);
		filter.set_gain(p_highshelf_gain);

		ERR_FAIL_NULL(p_processor_l);
		ERR_FAIL_NULL(p_processor_r);

		bool is_just_started = p_vol_start.left == 0 && p_vol_start.right == 0;
		p_processor_l->set_filter(&filter, /* clear_history= */ is_just_started);
		p_processor_l->update_coeffs(buffer_size);
		p_processor_r->set_filter(&filter, /* clear_history= */ is_just_started);
		p_processor_r->update_coeffs(buffer_size);

		for (unsigned int frame_idx = 0; frame_idx < buffer_size; frame_idx++) {
			// TODO: Make lerp speed buffer-size-invariant if buffer_size ever becomes a project setting to avoid very small buffer sizes causing pops due to too-fast lerps.
			float lerp_param = (float)frame_idx / buffer_size;
			AudioFrame vol = p_vol_final * lerp_param + (1 - lerp_param) * p_vol_start;
			AudioFrame mixed = vol * p_source_buf[frame_idx];
			p_processor_l->process_one_interp(mixed.left);
			p_processor_r->process_one_interp(mixed.right);
			p_out_buf[frame_idx] += mixed;
		}

	} else {
		for (unsigned int frame_idx = 0; frame_idx < buffer_size; frame_idx++) {
			// TODO: Make lerp speed buffer-size-invariant if buffer_size ever becomes a project setting to avoid very small buffer sizes causing pops due to too-fast lerps.
			float lerp_param = (float)frame_idx / buffer_size;
			p_out_buf[frame_idx] += (p_vol_final * lerp_param + (1 - lerp_param) * p_vol_start) * p_source_buf[frame_idx];
		}
	}
}

AudioServer::AudioStreamPlaybackListNode *AudioServer::_find_playback_list_node(Ref<AudioStreamPlayback> p_playback) {
	for (AudioStreamPlaybackListNode *playback_list_node : playback_list) {
		if (playback_list_node->stream_playback == p_playback) {
			return playback_list_node;
		}
	}
	return nullptr;
}

void AudioServer::_delete_stream_playback(Ref<AudioStreamPlayback> p_playback) {
	ERR_FAIL_COND(p_playback.is_null());
	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (playback_node) {
		_delete_stream_playback_list_node(playback_node);
	}
}

void AudioServer::_delete_stream_playback_list_node(AudioStreamPlaybackListNode *p_playback_node) {
	// Remove the playback from the list, registering a destructor to be run on the main thread.
	playback_list.erase(p_playback_node, [](AudioStreamPlaybackListNode *p) {
		delete p->prev_bus_details;
		delete p->bus_details.load();
		p->stream_playback.unref();
		delete p;
	});
}

bool AudioServer::thread_has_channel_mix_buffer(int p_bus, int p_buffer) const {
	if (p_bus < 0 || p_bus >= buses.size()) {
		return false;
	}
	if (p_buffer < 0 || p_buffer >= buses[p_bus]->channels.size()) {
		return false;
	}
	return true;
}

AudioFrame *AudioServer::thread_get_channel_mix_buffer(int p_bus, int p_buffer) {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), nullptr);
	ERR_FAIL_INDEX_V(p_buffer, buses[p_bus]->channels.size(), nullptr);

	AudioFrame *data = buses.write[p_bus]->channels.write[p_buffer].buffer.ptrw();

	if (!buses[p_bus]->channels[p_buffer].used) {
		buses.write[p_bus]->channels.write[p_buffer].used = true;
		buses.write[p_bus]->channels.write[p_buffer].active = true;
		buses.write[p_bus]->channels.write[p_buffer].last_mix_with_audio = mix_frames;
		for (uint32_t i = 0; i < buffer_size; i++) {
			data[i] = AudioFrame(0, 0);
		}
	}

	return data;
}

int AudioServer::thread_get_mix_buffer_size() const {
	return buffer_size;
}

int AudioServer::thread_find_bus_index(const StringName &p_name) {
	if (bus_map.has(p_name)) {
		return bus_map[p_name]->index_cache;
	} else {
		return 0;
	}
}

void AudioServer::set_bus_count(int p_count) {
	ERR_FAIL_COND(p_count < 1);
	ERR_FAIL_INDEX(p_count, 256);

	MARK_EDITED

	lock();
	int cb = buses.size();

	if (p_count < buses.size()) {
		for (int i = p_count; i < buses.size(); i++) {
			bus_map.erase(buses[i]->name);
			memdelete(buses[i]);
		}
	}

	buses.resize(p_count);

	for (int i = cb; i < buses.size(); i++) {
		String attempt = "New Bus";
		int attempts = 1;
		while (true) {
			bool name_free = true;
			for (int j = 0; j < i; j++) {
				if (buses[j]->name == attempt) {
					name_free = false;
					break;
				}
			}

			if (!name_free) {
				attempts++;
				attempt = "New Bus " + itos(attempts);
			} else {
				break;
			}
		}

		buses.write[i] = memnew(Bus);
		buses.write[i]->channels.resize(channel_count);
		for (int j = 0; j < channel_count; j++) {
			buses.write[i]->channels.write[j].buffer.resize(buffer_size);
		}
		buses[i]->name = attempt;
		buses[i]->solo = false;
		buses[i]->mute = false;
		buses[i]->bypass = false;
		buses[i]->volume_db = 0;
		if (i > 0) {
			buses[i]->send = SceneStringName(Master);
		}

		bus_map[attempt] = buses[i];
	}

	unlock();

	AudioDriver::get_singleton()->set_sample_bus_count(p_count);

	emit_signal(SNAME("bus_layout_changed"));
}

void AudioServer::remove_bus(int p_index) {
	ERR_FAIL_INDEX(p_index, buses.size());
	ERR_FAIL_COND(p_index == 0);

	MARK_EDITED

	lock();
	bus_map.erase(buses[p_index]->name);
	memdelete(buses[p_index]);
	buses.remove_at(p_index);
	unlock();

	AudioDriver::get_singleton()->remove_sample_bus(p_index);

	emit_signal(SNAME("bus_layout_changed"));
}

void AudioServer::add_bus(int p_at_pos) {
	MARK_EDITED

	if (p_at_pos >= buses.size()) {
		p_at_pos = -1;
	} else if (p_at_pos == 0) {
		if (buses.size() > 1) {
			p_at_pos = 1;
		} else {
			p_at_pos = -1;
		}
	}

	String attempt = "New Bus";
	int attempts = 1;
	while (true) {
		bool name_free = true;
		for (int j = 0; j < buses.size(); j++) {
			if (buses[j]->name == attempt) {
				name_free = false;
				break;
			}
		}

		if (!name_free) {
			attempts++;
			attempt = "New Bus " + itos(attempts);
		} else {
			break;
		}
	}

	Bus *bus = memnew(Bus);
	bus->channels.resize(channel_count);
	for (int j = 0; j < channel_count; j++) {
		bus->channels.write[j].buffer.resize(buffer_size);
	}
	bus->name = attempt;
	bus->solo = false;
	bus->mute = false;
	bus->bypass = false;
	bus->volume_db = 0;

	bus_map[attempt] = bus;

	if (p_at_pos == -1) {
		buses.push_back(bus);
	} else {
		buses.insert(p_at_pos, bus);
	}

	AudioDriver::get_singleton()->add_sample_bus(p_at_pos);

	emit_signal(SNAME("bus_layout_changed"));
}

void AudioServer::move_bus(int p_bus, int p_to_pos) {
	ERR_FAIL_COND(p_bus < 1 || p_bus >= buses.size());
	ERR_FAIL_COND(p_to_pos != -1 && (p_to_pos < 1 || p_to_pos > buses.size()));

	MARK_EDITED

	if (p_bus == p_to_pos) {
		return;
	}

	Bus *bus = buses[p_bus];
	buses.remove_at(p_bus);

	if (p_to_pos == -1) {
		buses.push_back(bus);
	} else if (p_to_pos < p_bus) {
		buses.insert(p_to_pos, bus);
	} else {
		buses.insert(p_to_pos - 1, bus);
	}

	AudioDriver::get_singleton()->move_sample_bus(p_bus, p_to_pos);

	emit_signal(SNAME("bus_layout_changed"));
}

int AudioServer::get_bus_count() const {
	return buses.size();
}

void AudioServer::set_bus_name(int p_bus, const String &p_name) {
	ERR_FAIL_INDEX(p_bus, buses.size());
	if (p_bus == 0 && p_name != "Master") {
		return; // Bus 0 is always "Master".
	}

	MARK_EDITED

	lock();

	StringName old_name = buses[p_bus]->name;

	if (old_name == p_name) {
		unlock();
		return;
	}

	String attempt = p_name;
	int attempts = 1;

	while (true) {
		bool name_free = true;
		for (int i = 0; i < buses.size(); i++) {
			if (buses[i]->name == attempt) {
				name_free = false;
				break;
			}
		}

		if (name_free) {
			break;
		}

		attempts++;
		attempt = p_name + " " + itos(attempts);
	}
	bus_map.erase(old_name);
	buses[p_bus]->name = attempt;
	bus_map[attempt] = buses[p_bus];
	unlock();

	emit_signal(SNAME("bus_renamed"), p_bus, old_name, attempt);
}

String AudioServer::get_bus_name(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), String());
	return buses[p_bus]->name;
}

int AudioServer::get_bus_index(const StringName &p_bus_name) const {
	for (int i = 0; i < buses.size(); ++i) {
		if (buses[i]->name == p_bus_name) {
			return i;
		}
	}
	return -1;
}

void AudioServer::set_bus_volume_db(int p_bus, float p_volume_db) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->volume_db = p_volume_db;

	AudioDriver::get_singleton()->set_sample_bus_volume_db(p_bus, p_volume_db);
}

float AudioServer::get_bus_volume_db(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	return buses[p_bus]->volume_db;
}

void AudioServer::set_bus_volume_linear(int p_bus, float p_volume_linear) {
	set_bus_volume_db(p_bus, Math::linear_to_db(p_volume_linear));
}

float AudioServer::get_bus_volume_linear(int p_bus) const {
	return Math::db_to_linear(get_bus_volume_db(p_bus));
}

int AudioServer::get_bus_channels(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	return buses[p_bus]->channels.size();
}

void AudioServer::set_bus_send(int p_bus, const StringName &p_send) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->send = p_send;

	AudioDriver::get_singleton()->set_sample_bus_send(p_bus, p_send);
}

StringName AudioServer::get_bus_send(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), StringName());
	return buses[p_bus]->send;
}

void AudioServer::set_bus_solo(int p_bus, bool p_enable) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->solo = p_enable;

	AudioDriver::get_singleton()->set_sample_bus_solo(p_bus, p_enable);
}

bool AudioServer::is_bus_solo(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

	return buses[p_bus]->solo;
}

void AudioServer::set_bus_mute(int p_bus, bool p_enable) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->mute = p_enable;

	AudioDriver::get_singleton()->set_sample_bus_mute(p_bus, p_enable);
}

bool AudioServer::is_bus_mute(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

	return buses[p_bus]->mute;
}

void AudioServer::set_bus_bypass_effects(int p_bus, bool p_enable) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->bypass = p_enable;
}

bool AudioServer::is_bus_bypassing_effects(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

	return buses[p_bus]->bypass;
}

void AudioServer::_update_bus_effects(int p_bus) {
	for (int i = 0; i < buses[p_bus]->channels.size(); i++) {
		buses.write[p_bus]->channels.write[i].effect_instances.resize(buses[p_bus]->effects.size());
		for (int j = 0; j < buses[p_bus]->effects.size(); j++) {
			Ref<AudioEffectInstance> fx = buses.write[p_bus]->effects.write[j].effect->instantiate();
			if (Object::cast_to<AudioEffectCompressorInstance>(*fx)) {
				Object::cast_to<AudioEffectCompressorInstance>(*fx)->set_current_channel(i);
			}
			buses.write[p_bus]->channels.write[i].effect_instances.write[j] = fx;
		}
	}
}

void AudioServer::add_bus_effect(int p_bus, const Ref<AudioEffect> &p_effect, int p_at_pos) {
	ERR_FAIL_COND(p_effect.is_null());
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	lock();

	Bus::Effect fx;
	fx.effect = p_effect;
	//fx.instance=p_effect->instantiate();
	fx.enabled = true;
#ifdef DEBUG_ENABLED
	fx.prof_time = 0;
#endif

	if (p_at_pos >= buses[p_bus]->effects.size() || p_at_pos < 0) {
		buses[p_bus]->effects.push_back(fx);
	} else {
		buses[p_bus]->effects.insert(p_at_pos, fx);
	}

	_update_bus_effects(p_bus);

	unlock();
}

void AudioServer::remove_bus_effect(int p_bus, int p_effect) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	lock();

	buses[p_bus]->effects.remove_at(p_effect);
	_update_bus_effects(p_bus);

	unlock();
}

int AudioServer::get_bus_effect_count(int p_bus) {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);

	return buses[p_bus]->effects.size();
}

Ref<AudioEffectInstance> AudioServer::get_bus_effect_instance(int p_bus, int p_effect, int p_channel) {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), Ref<AudioEffectInstance>());
	ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), Ref<AudioEffectInstance>());
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), Ref<AudioEffectInstance>());

	return buses[p_bus]->channels[p_channel].effect_instances[p_effect];
}

Ref<AudioEffect> AudioServer::get_bus_effect(int p_bus, int p_effect) {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), Ref<AudioEffect>());
	ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), Ref<AudioEffect>());

	return buses[p_bus]->effects[p_effect].effect;
}

void AudioServer::swap_bus_effects(int p_bus, int p_effect, int p_by_effect) {
	ERR_FAIL_INDEX(p_bus, buses.size());
	ERR_FAIL_INDEX(p_effect, buses[p_bus]->effects.size());
	ERR_FAIL_INDEX(p_by_effect, buses[p_bus]->effects.size());

	MARK_EDITED

	lock();
	SWAP(buses.write[p_bus]->effects.write[p_effect], buses.write[p_bus]->effects.write[p_by_effect]);
	_update_bus_effects(p_bus);
	unlock();
}

void AudioServer::set_bus_effect_enabled(int p_bus, int p_effect, bool p_enabled) {
	ERR_FAIL_INDEX(p_bus, buses.size());
	ERR_FAIL_INDEX(p_effect, buses[p_bus]->effects.size());

	MARK_EDITED

	buses.write[p_bus]->effects.write[p_effect].enabled = p_enabled;
}

bool AudioServer::is_bus_effect_enabled(int p_bus, int p_effect) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);
	ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), false);
	return buses[p_bus]->effects[p_effect].enabled;
}

float AudioServer::get_bus_peak_volume_left_db(int p_bus, int p_channel) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), 0);

	return buses[p_bus]->channels[p_channel].peak_volume.left;
}

float AudioServer::get_bus_peak_volume_right_db(int p_bus, int p_channel) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), 0);

	return buses[p_bus]->channels[p_channel].peak_volume.right;
}

bool AudioServer::is_bus_channel_active(int p_bus, int p_channel) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), false);

	return buses[p_bus]->channels[p_channel].active;
}

void AudioServer::set_playback_speed_scale(float p_scale) {
	ERR_FAIL_COND(p_scale <= 0);

	playback_speed_scale = p_scale;
}

float AudioServer::get_playback_speed_scale() const {
	return playback_speed_scale;
}

void AudioServer::start_playback_stream(Ref<AudioStreamPlayback> p_playback, const StringName &p_bus, Vector<AudioFrame> p_volume_db_vector, float p_start_time, float p_pitch_scale) {
	ERR_FAIL_COND(p_playback.is_null());

	HashMap<StringName, Vector<AudioFrame>> map;
	map[p_bus] = p_volume_db_vector;

	start_playback_stream(p_playback, map, p_start_time, p_pitch_scale);
}

void AudioServer::start_playback_stream(Ref<AudioStreamPlayback> p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes, float p_start_time, float p_pitch_scale, float p_highshelf_gain, float p_attenuation_cutoff_hz) {
	ERR_FAIL_COND(p_playback.is_null());

	AudioStreamPlaybackListNode *playback_node = new AudioStreamPlaybackListNode();
	playback_node->stream_playback = p_playback;
	playback_node->stream_playback->start(p_start_time);

	AudioStreamPlaybackBusDetails *new_bus_details = new AudioStreamPlaybackBusDetails();
	int idx = 0;
	for (KeyValue<StringName, Vector<AudioFrame>> pair : p_bus_volumes) {
		if (pair.value.size() < channel_count || pair.value.size() != MAX_CHANNELS_PER_BUS) {
			delete new_bus_details;
			ERR_FAIL();
		}

		new_bus_details->bus_active[idx] = true;
		new_bus_details->bus[idx] = pair.key;
		for (int channel_idx = 0; channel_idx < MAX_CHANNELS_PER_BUS; channel_idx++) {
			new_bus_details->volume[idx][channel_idx] = pair.value[channel_idx];
		}
		idx++;
	}
	playback_node->bus_details.store(new_bus_details);
	playback_node->prev_bus_details = new AudioStreamPlaybackBusDetails();

	playback_node->pitch_scale.set(p_pitch_scale);
	playback_node->highshelf_gain.set(p_highshelf_gain);
	playback_node->attenuation_filter_cutoff_hz.set(p_attenuation_cutoff_hz);

	memset(playback_node->prev_bus_details->volume, 0, sizeof(playback_node->prev_bus_details->volume));

	for (AudioFrame &frame : playback_node->lookahead) {
		frame = AudioFrame(0, 0);
	}

	playback_node->state.store(AudioStreamPlaybackListNode::PLAYING);

	playback_list.insert(playback_node);
}

void AudioServer::stop_playback_stream(Ref<AudioStreamPlayback> p_playback) {
	ERR_FAIL_COND(p_playback.is_null());

	// Handle sample playback.
	if (p_playback->get_is_sample()) {
		if (p_playback->get_sample_playback().is_valid()) {
			AudioServer::get_singleton()->stop_sample_playback(p_playback->get_sample_playback());
		} else {
			_delete_stream_playback(p_playback);
		}
		return;
	}

	if (!p_playback->is_playing()) {
		p_playback->stop();
	}

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}

	AudioStreamPlaybackListNode::PlaybackState new_state, old_state;
	do {
		old_state = playback_node->state.load();
		if (old_state == AudioStreamPlaybackListNode::AWAITING_DELETION) {
			break; // Don't fade out again.
		}
		new_state = AudioStreamPlaybackListNode::FADE_OUT_TO_DELETION;

	} while (!playback_node->state.compare_exchange_strong(old_state, new_state));
}

void AudioServer::set_playback_bus_exclusive(Ref<AudioStreamPlayback> p_playback, const StringName &p_bus, Vector<AudioFrame> p_volumes) {
	ERR_FAIL_COND(p_volumes.size() != MAX_CHANNELS_PER_BUS);

	HashMap<StringName, Vector<AudioFrame>> map;
	map[p_bus] = p_volumes;

	set_playback_bus_volumes_linear(p_playback, map);
}

void AudioServer::set_playback_bus_volumes_linear(Ref<AudioStreamPlayback> p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes) {
	ERR_FAIL_COND(p_bus_volumes.size() > MAX_BUSES_PER_PLAYBACK);

	// Samples.
	if (p_playback->get_is_sample() && p_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = p_playback->get_sample_playback();
		AudioDriver::get_singleton()->set_sample_playback_bus_volumes_linear(sample_playback, p_bus_volumes);
		return;
	}

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}
	AudioStreamPlaybackBusDetails *old_bus_details, *new_bus_details = new AudioStreamPlaybackBusDetails();

	int idx = 0;
	for (KeyValue<StringName, Vector<AudioFrame>> pair : p_bus_volumes) {
		if (idx >= MAX_BUSES_PER_PLAYBACK) {
			break;
		}
		ERR_FAIL_COND(pair.value.size() < channel_count);
		ERR_FAIL_COND(pair.value.size() != MAX_CHANNELS_PER_BUS);

		new_bus_details->bus_active[idx] = true;
		new_bus_details->bus[idx] = pair.key;
		for (int channel_idx = 0; channel_idx < MAX_CHANNELS_PER_BUS; channel_idx++) {
			new_bus_details->volume[idx][channel_idx] = pair.value[channel_idx];
		}
		idx++;
	}

	do {
		old_bus_details = playback_node->bus_details.load();
	} while (!playback_node->bus_details.compare_exchange_strong(old_bus_details, new_bus_details));

	bus_details_graveyard.insert(old_bus_details);
}

void AudioServer::set_playback_all_bus_volumes_linear(Ref<AudioStreamPlayback> p_playback, Vector<AudioFrame> p_volumes) {
	ERR_FAIL_COND(p_playback.is_null());
	ERR_FAIL_COND(p_volumes.size() != MAX_CHANNELS_PER_BUS);

	HashMap<StringName, Vector<AudioFrame>> map;

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}
	for (int bus_idx = 0; bus_idx < MAX_BUSES_PER_PLAYBACK; bus_idx++) {
		if (playback_node->bus_details.load()->bus_active[bus_idx]) {
			map[playback_node->bus_details.load()->bus[bus_idx]] = p_volumes;
		}
	}

	set_playback_bus_volumes_linear(p_playback, map);
}

void AudioServer::set_playback_pitch_scale(Ref<AudioStreamPlayback> p_playback, float p_pitch_scale) {
	ERR_FAIL_COND(p_playback.is_null());

	// Samples.
	if (p_playback->get_is_sample() && p_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = p_playback->get_sample_playback();
		AudioServer::get_singleton()->update_sample_playback_pitch_scale(sample_playback, p_pitch_scale);
		return;
	}

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}

	playback_node->pitch_scale.set(p_pitch_scale);
}

void AudioServer::set_playback_paused(Ref<AudioStreamPlayback> p_playback, bool p_paused) {
	ERR_FAIL_COND(p_playback.is_null());

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}

	AudioStreamPlaybackListNode::PlaybackState new_state, old_state;
	do {
		old_state = playback_node->state.load();
		new_state = p_paused ? AudioStreamPlaybackListNode::FADE_OUT_TO_PAUSE : AudioStreamPlaybackListNode::PLAYING;
		if (!p_paused && old_state == AudioStreamPlaybackListNode::PLAYING) {
			return; // No-op.
		}
		if (p_paused && (old_state == AudioStreamPlaybackListNode::PAUSED || old_state == AudioStreamPlaybackListNode::FADE_OUT_TO_PAUSE)) {
			return; // No-op.
		}

	} while (!playback_node->state.compare_exchange_strong(old_state, new_state));
}

void AudioServer::set_playback_highshelf_params(Ref<AudioStreamPlayback> p_playback, float p_gain, float p_attenuation_cutoff_hz) {
	ERR_FAIL_COND(p_playback.is_null());

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return;
	}

	playback_node->attenuation_filter_cutoff_hz.set(p_attenuation_cutoff_hz);
	playback_node->highshelf_gain.set(p_gain);
}

bool AudioServer::is_playback_active(Ref<AudioStreamPlayback> p_playback) {
	ERR_FAIL_COND_V(p_playback.is_null(), false);

	if (p_playback->get_is_sample()) {
		if (p_playback->get_sample_playback().is_valid()) {
			return sample_playback_list.has(p_playback->get_sample_playback());
		} else {
			return false;
		}
	}

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return false;
	}

	return playback_node->state.load() == AudioStreamPlaybackListNode::PLAYING;
}

float AudioServer::get_playback_position(Ref<AudioStreamPlayback> p_playback) {
	ERR_FAIL_COND_V(p_playback.is_null(), 0);

	// Samples.
	if (p_playback->get_is_sample() && p_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = p_playback->get_sample_playback();
		return AudioServer::get_singleton()->get_sample_playback_position(sample_playback);
	}

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return 0;
	}

	return playback_node->stream_playback->get_playback_position();
}

bool AudioServer::is_playback_paused(Ref<AudioStreamPlayback> p_playback) {
	ERR_FAIL_COND_V(p_playback.is_null(), false);

	AudioStreamPlaybackListNode *playback_node = _find_playback_list_node(p_playback);
	if (!playback_node) {
		return false;
	}

	return playback_node->state.load() == AudioStreamPlaybackListNode::PAUSED || playback_node->state.load() == AudioStreamPlaybackListNode::FADE_OUT_TO_PAUSE;
}

uint64_t AudioServer::get_mix_count() const {
	return mix_count;
}

uint64_t AudioServer::get_mixed_frames() const {
	return mix_frames;
}

String AudioServer::get_driver_name() const {
	return AudioDriver::get_singleton()->get_name();
}

void AudioServer::notify_listener_changed() {
	for (CallbackItem *ci : listener_changed_callback_list) {
		ci->callback(ci->userdata);
	}
}

void AudioServer::init_channels_and_buffers() {
	channel_count = get_channel_count();
	temp_buffer.resize(channel_count);
	mix_buffer.resize(buffer_size + LOOKAHEAD_BUFFER_SIZE);

	for (int i = 0; i < temp_buffer.size(); i++) {
		temp_buffer.write[i].resize(buffer_size);
	}

	for (int i = 0; i < buses.size(); i++) {
		buses[i]->channels.resize(channel_count);
		for (int j = 0; j < channel_count; j++) {
			buses.write[i]->channels.write[j].buffer.resize(buffer_size);
		}
		_update_bus_effects(i);
	}
}

void AudioServer::init() {
	channel_disable_threshold_db = GLOBAL_DEF_RST(PropertyInfo(Variant::FLOAT, "audio/buses/channel_disable_threshold_db", PROPERTY_HINT_RANGE, "-80,0,0.1,suffix:dB"), -60.0);
	channel_disable_frames = float(GLOBAL_DEF_RST(PropertyInfo(Variant::FLOAT, "audio/buses/channel_disable_time", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), 2.0)) * get_mix_rate();
	// TODO: Buffer size is hardcoded for now. This would be really nice to have as a project setting because currently it limits audio latency to an absolute minimum of 11ms with default mix rate, but there's some additional work required to make that happen. See TODOs in `_mix_step_for_channel`.
	// When this becomes a project setting, it should be specified in milliseconds rather than raw sample count, because 512 samples at 192khz is shorter than it is at 48khz, for example.
	buffer_size = 512;

	init_channels_and_buffers();

	mix_count = 0;
	set_bus_count(1);
	set_bus_name(0, "Master");

	if (AudioDriver::get_singleton()) {
		AudioDriver::get_singleton()->start();
		AudioDriver::get_singleton()->set_sample_bus_count(1);
	}

#ifdef TOOLS_ENABLED
	set_edited(false); //avoid editors from thinking this was edited
#endif

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "audio/video/video_delay_compensation_ms", PROPERTY_HINT_RANGE, "-1000,1000,1,suffix:ms"), 0);
}

void AudioServer::update() {
#ifdef DEBUG_ENABLED
	if (EngineDebugger::is_profiling(SNAME("servers"))) {
		// Driver time includes server time + effects times
		// Server time includes effects times
		uint64_t driver_time = AudioDriver::get_singleton()->get_profiling_time();
		uint64_t server_time = prof_time.get();

		// Subtract the server time from the driver time
		if (driver_time > server_time) {
			driver_time -= server_time;
		}

		Array values;

		for (int i = buses.size() - 1; i >= 0; i--) {
			Bus *bus = buses[i];
			if (bus->bypass) {
				continue;
			}

			for (int j = 0; j < bus->effects.size(); j++) {
				if (!bus->effects[j].enabled) {
					continue;
				}

				values.push_back(String(bus->name) + bus->effects[j].effect->get_name());
				values.push_back(USEC_TO_SEC(bus->effects[j].prof_time));

				// Subtract the effect time from the driver and server times
				if (driver_time > bus->effects[j].prof_time) {
					driver_time -= bus->effects[j].prof_time;
				}
				if (server_time > bus->effects[j].prof_time) {
					server_time -= bus->effects[j].prof_time;
				}
			}
		}

		values.push_back("audio_server");
		values.push_back(USEC_TO_SEC(server_time));
		values.push_back("audio_driver");
		values.push_back(USEC_TO_SEC(driver_time));

		values.push_front("audio_thread");
		EngineDebugger::profiler_add_frame_data("servers", values);
	}

	// Reset profiling times
	for (int i = buses.size() - 1; i >= 0; i--) {
		Bus *bus = buses[i];
		if (bus->bypass) {
			continue;
		}

		for (int j = 0; j < bus->effects.size(); j++) {
			if (!bus->effects[j].enabled) {
				continue;
			}

			bus->effects.write[j].prof_time = 0;
		}
	}

	AudioDriver::get_singleton()->reset_profiling_time();
	prof_time.set(0);
#endif

	for (CallbackItem *ci : update_callback_list) {
		ci->callback(ci->userdata);
	}
	mix_callback_list.maybe_cleanup();
	update_callback_list.maybe_cleanup();
	listener_changed_callback_list.maybe_cleanup();
	playback_list.maybe_cleanup();
	for (AudioStreamPlaybackBusDetails *bus_details : bus_details_graveyard_frame_old) {
		bus_details_graveyard_frame_old.erase(bus_details, [](AudioStreamPlaybackBusDetails *d) { delete d; });
	}
	for (AudioStreamPlaybackBusDetails *bus_details : bus_details_graveyard) {
		bus_details_graveyard_frame_old.insert(bus_details);
		bus_details_graveyard.erase(bus_details);
	}
	bus_details_graveyard.maybe_cleanup();
	bus_details_graveyard_frame_old.maybe_cleanup();
}

void AudioServer::load_default_bus_layout() {
	String layout_path = GLOBAL_GET("audio/buses/default_bus_layout");

	if (ResourceLoader::exists(layout_path)) {
		Ref<AudioBusLayout> default_layout = ResourceLoader::load(layout_path);
		if (default_layout.is_valid()) {
			set_bus_layout(default_layout);
		}
	}
}

void AudioServer::finish() {
	for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
		AudioDriverManager::get_driver(i)->finish();
	}

	for (int i = 0; i < buses.size(); i++) {
		memdelete(buses[i]);
	}

	buses.clear();
}

/* MISC config */

void AudioServer::lock() {
	AudioDriver::get_singleton()->lock();
}

void AudioServer::unlock() {
	AudioDriver::get_singleton()->unlock();
}

AudioServer::SpeakerMode AudioServer::get_speaker_mode() const {
	return (AudioServer::SpeakerMode)AudioDriver::get_singleton()->get_speaker_mode();
}

float AudioServer::get_mix_rate() const {
	return AudioDriver::get_singleton()->get_mix_rate();
}

float AudioServer::get_input_mix_rate() const {
	return AudioDriver::get_singleton()->get_input_mix_rate();
}

float AudioServer::read_output_peak_db() const {
	return 0;
}

AudioServer *AudioServer::get_singleton() {
	return singleton;
}

double AudioServer::get_output_latency() const {
	return AudioDriver::get_singleton()->get_latency();
}

double AudioServer::get_time_to_next_mix() const {
	return AudioDriver::get_singleton()->get_time_to_next_mix();
}

double AudioServer::get_time_since_last_mix() const {
	return AudioDriver::get_singleton()->get_time_since_last_mix();
}

AudioServer *AudioServer::singleton = nullptr;

void AudioServer::add_update_callback(AudioCallback p_callback, void *p_userdata) {
	CallbackItem *ci = new CallbackItem();
	ci->callback = p_callback;
	ci->userdata = p_userdata;
	update_callback_list.insert(ci);
}

void AudioServer::remove_update_callback(AudioCallback p_callback, void *p_userdata) {
	for (CallbackItem *ci : update_callback_list) {
		if (ci->callback == p_callback && ci->userdata == p_userdata) {
			update_callback_list.erase(ci, [](CallbackItem *c) { delete c; });
		}
	}
}

void AudioServer::add_mix_callback(AudioCallback p_callback, void *p_userdata) {
	CallbackItem *ci = new CallbackItem();
	ci->callback = p_callback;
	ci->userdata = p_userdata;
	mix_callback_list.insert(ci);
}

void AudioServer::remove_mix_callback(AudioCallback p_callback, void *p_userdata) {
	for (CallbackItem *ci : mix_callback_list) {
		if (ci->callback == p_callback && ci->userdata == p_userdata) {
			mix_callback_list.erase(ci, [](CallbackItem *c) { delete c; });
		}
	}
}

void AudioServer::add_listener_changed_callback(AudioCallback p_callback, void *p_userdata) {
	CallbackItem *ci = new CallbackItem();
	ci->callback = p_callback;
	ci->userdata = p_userdata;
	listener_changed_callback_list.insert(ci);
}

void AudioServer::remove_listener_changed_callback(AudioCallback p_callback, void *p_userdata) {
	for (CallbackItem *ci : listener_changed_callback_list) {
		if (ci->callback == p_callback && ci->userdata == p_userdata) {
			listener_changed_callback_list.erase(ci, [](CallbackItem *c) { delete c; });
		}
	}
}

void AudioServer::set_bus_layout(const Ref<AudioBusLayout> &p_bus_layout) {
	ERR_FAIL_COND(p_bus_layout.is_null() || p_bus_layout->buses.is_empty());

	lock();
	for (int i = 0; i < buses.size(); i++) {
		memdelete(buses[i]);
	}
	buses.resize(p_bus_layout->buses.size());
	bus_map.clear();

	AudioDriver::get_singleton()->set_sample_bus_count(buses.size());

	for (int i = 0; i < p_bus_layout->buses.size(); i++) {
		Bus *bus = memnew(Bus);
		if (i == 0) {
			bus->name = SceneStringName(Master);
		} else {
			bus->name = p_bus_layout->buses[i].name;
			bus->send = p_bus_layout->buses[i].send;
			AudioDriver::get_singleton()->set_sample_bus_send(i, bus->send);
		}

		bus->solo = p_bus_layout->buses[i].solo;
		bus->mute = p_bus_layout->buses[i].mute;
		bus->bypass = p_bus_layout->buses[i].bypass;
		bus->volume_db = p_bus_layout->buses[i].volume_db;

		AudioDriver::get_singleton()->set_sample_bus_solo(i, bus->solo);
		AudioDriver::get_singleton()->set_sample_bus_mute(i, bus->mute);
		AudioDriver::get_singleton()->set_sample_bus_volume_db(i, bus->volume_db);

		for (int j = 0; j < p_bus_layout->buses[i].effects.size(); j++) {
			Ref<AudioEffect> fx = p_bus_layout->buses[i].effects[j].effect;

			if (fx.is_valid()) {
				Bus::Effect bfx;
				bfx.effect = fx;
				bfx.enabled = p_bus_layout->buses[i].effects[j].enabled;
#ifdef DEBUG_ENABLED
				bfx.prof_time = 0;
#endif
				bus->effects.push_back(bfx);
			}
		}

		bus_map[bus->name] = bus;
		buses.write[i] = bus;

		buses[i]->channels.resize(channel_count);
		for (int j = 0; j < channel_count; j++) {
			buses.write[i]->channels.write[j].buffer.resize(buffer_size);
		}
		_update_bus_effects(i);
	}
#ifdef TOOLS_ENABLED
	set_edited(false);
#endif
	unlock();

	// Samples bus sync.
}

Ref<AudioBusLayout> AudioServer::generate_bus_layout() const {
	Ref<AudioBusLayout> state;
	state.instantiate();

	state->buses.resize(buses.size());

	for (int i = 0; i < buses.size(); i++) {
		state->buses.write[i].name = buses[i]->name;
		state->buses.write[i].send = buses[i]->send;
		state->buses.write[i].mute = buses[i]->mute;
		state->buses.write[i].solo = buses[i]->solo;
		state->buses.write[i].bypass = buses[i]->bypass;
		state->buses.write[i].volume_db = buses[i]->volume_db;
		for (int j = 0; j < buses[i]->effects.size(); j++) {
			AudioBusLayout::Bus::Effect fx;
			fx.effect = buses[i]->effects[j].effect;
			fx.enabled = buses[i]->effects[j].enabled;
			state->buses.write[i].effects.push_back(fx);
		}
	}

	return state;
}

PackedStringArray AudioServer::get_output_device_list() {
	return AudioDriver::get_singleton()->get_output_device_list();
}

String AudioServer::get_output_device() {
	return AudioDriver::get_singleton()->get_output_device();
}

void AudioServer::set_output_device(const String &p_name) {
	AudioDriver::get_singleton()->set_output_device(p_name);
}

PackedStringArray AudioServer::get_input_device_list() {
	return AudioDriver::get_singleton()->get_input_device_list();
}

String AudioServer::get_input_device() {
	return AudioDriver::get_singleton()->get_input_device();
}

void AudioServer::set_input_device(const String &p_name) {
	AudioDriver::get_singleton()->set_input_device(p_name);
}

void AudioServer::set_enable_tagging_used_audio_streams(bool p_enable) {
	tag_used_audio_streams = p_enable;
}

#ifdef TOOLS_ENABLED
void AudioServer::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if ((p_idx == 0 && pf == "get_bus_index") || (p_idx == 1 && pf == "set_bus_send")) {
		for (const AudioServer::Bus *E : buses) {
			r_options->push_back(String(E->name).quote());
		}
	}

	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

AudioServer::PlaybackType AudioServer::get_default_playback_type() const {
	int playback_type = GLOBAL_GET("audio/general/default_playback_type");
	ERR_FAIL_COND_V_MSG(
			playback_type < 0 || playback_type >= PlaybackType::PLAYBACK_TYPE_MAX,
			PlaybackType::PLAYBACK_TYPE_STREAM,
			vformat(R"(Project settings value (%s) for "audio/general/default_playback_type" is not supported)", playback_type));

	switch (playback_type) {
		case 1: {
			return PlaybackType::PLAYBACK_TYPE_SAMPLE;
		} break;

		case 0:
		default: {
			return PlaybackType::PLAYBACK_TYPE_STREAM;
		} break;
	}
}

bool AudioServer::is_stream_registered_as_sample(const Ref<AudioStream> &p_stream) {
	ERR_FAIL_COND_V_MSG(p_stream.is_null(), false, "Parameter p_stream is null.");
	return AudioDriver::get_singleton()->is_stream_registered_as_sample(p_stream);
}

void AudioServer::register_stream_as_sample(const Ref<AudioStream> &p_stream) {
	ERR_FAIL_COND_MSG(p_stream.is_null(), "Parameter p_stream is null.");
	ERR_FAIL_COND_MSG(!(p_stream->can_be_sampled()), "Parameter p_stream cannot be sampled.");
	Ref<AudioSample> sample = p_stream->generate_sample();
	register_sample(sample);
}

void AudioServer::unregister_stream_as_sample(const Ref<AudioStream> &p_stream) {
	ERR_FAIL_COND_MSG(p_stream.is_null(), "Parameter p_stream is null.");
	ERR_FAIL_COND_MSG(!(p_stream->can_be_sampled()), "Parameter p_stream cannot be sampled.");
	Ref<AudioSample> sample = p_stream->generate_sample();
	unregister_sample(sample);
}

void AudioServer::register_sample(const Ref<AudioSample> &p_sample) {
	ERR_FAIL_COND_MSG(p_sample.is_null(), "Parameter p_sample is null.");
	ERR_FAIL_COND_MSG(p_sample->stream.is_null(), "Parameter p_sample->stream is null.");
	ERR_FAIL_COND_MSG(!(p_sample->stream->can_be_sampled()), "Parameter p_stream cannot be sampled.");
	AudioDriver::get_singleton()->register_sample(p_sample);
}

void AudioServer::unregister_sample(const Ref<AudioSample> &p_sample) {
	ERR_FAIL_COND_MSG(p_sample.is_null(), "Parameter p_sample is null.");
	ERR_FAIL_COND_MSG(p_sample->stream.is_null(), "Parameter p_sample->stream is null.");
	AudioDriver::get_singleton()->unregister_sample(p_sample);
}

void AudioServer::start_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	AudioDriver::get_singleton()->start_sample_playback(p_playback);
	sample_playback_list.ordered_insert(p_playback);
}

void AudioServer::stop_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	if (sample_playback_list.has(p_playback)) {
		sample_playback_list.erase(p_playback);
		AudioDriver::get_singleton()->stop_sample_playback(p_playback);
		p_playback->stream_playback->set_sample_playback(nullptr);
		stop_playback_stream(p_playback->stream_playback);
	}
}

void AudioServer::set_sample_playback_pause(const Ref<AudioSamplePlayback> &p_playback, bool p_paused) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	AudioDriver::get_singleton()->set_sample_playback_pause(p_playback, p_paused);
}

bool AudioServer::is_sample_playback_active(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_V_MSG(p_playback.is_null(), false, "Parameter p_playback is null.");
	return AudioDriver::get_singleton()->is_sample_playback_active(p_playback);
}

double AudioServer::get_sample_playback_position(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_V_MSG(p_playback.is_null(), false, "Parameter p_playback is null.");
	return AudioDriver::get_singleton()->get_sample_playback_position(p_playback);
}

void AudioServer::update_sample_playback_pitch_scale(const Ref<AudioSamplePlayback> &p_playback, float p_pitch_scale) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	return AudioDriver::get_singleton()->update_sample_playback_pitch_scale(p_playback, p_pitch_scale);
}

void AudioServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bus_count", "amount"), &AudioServer::set_bus_count);
	ClassDB::bind_method(D_METHOD("get_bus_count"), &AudioServer::get_bus_count);

	ClassDB::bind_method(D_METHOD("remove_bus", "index"), &AudioServer::remove_bus);
	ClassDB::bind_method(D_METHOD("add_bus", "at_position"), &AudioServer::add_bus, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("move_bus", "index", "to_index"), &AudioServer::move_bus);

	ClassDB::bind_method(D_METHOD("set_bus_name", "bus_idx", "name"), &AudioServer::set_bus_name);
	ClassDB::bind_method(D_METHOD("get_bus_name", "bus_idx"), &AudioServer::get_bus_name);
	ClassDB::bind_method(D_METHOD("get_bus_index", "bus_name"), &AudioServer::get_bus_index);

	ClassDB::bind_method(D_METHOD("get_bus_channels", "bus_idx"), &AudioServer::get_bus_channels);

	ClassDB::bind_method(D_METHOD("set_bus_volume_db", "bus_idx", "volume_db"), &AudioServer::set_bus_volume_db);
	ClassDB::bind_method(D_METHOD("get_bus_volume_db", "bus_idx"), &AudioServer::get_bus_volume_db);

	ClassDB::bind_method(D_METHOD("set_bus_volume_linear", "bus_idx", "volume_linear"), &AudioServer::set_bus_volume_linear);
	ClassDB::bind_method(D_METHOD("get_bus_volume_linear", "bus_idx"), &AudioServer::get_bus_volume_linear);

	ClassDB::bind_method(D_METHOD("set_bus_send", "bus_idx", "send"), &AudioServer::set_bus_send);
	ClassDB::bind_method(D_METHOD("get_bus_send", "bus_idx"), &AudioServer::get_bus_send);

	ClassDB::bind_method(D_METHOD("set_bus_solo", "bus_idx", "enable"), &AudioServer::set_bus_solo);
	ClassDB::bind_method(D_METHOD("is_bus_solo", "bus_idx"), &AudioServer::is_bus_solo);

	ClassDB::bind_method(D_METHOD("set_bus_mute", "bus_idx", "enable"), &AudioServer::set_bus_mute);
	ClassDB::bind_method(D_METHOD("is_bus_mute", "bus_idx"), &AudioServer::is_bus_mute);

	ClassDB::bind_method(D_METHOD("set_bus_bypass_effects", "bus_idx", "enable"), &AudioServer::set_bus_bypass_effects);
	ClassDB::bind_method(D_METHOD("is_bus_bypassing_effects", "bus_idx"), &AudioServer::is_bus_bypassing_effects);

	ClassDB::bind_method(D_METHOD("add_bus_effect", "bus_idx", "effect", "at_position"), &AudioServer::add_bus_effect, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("remove_bus_effect", "bus_idx", "effect_idx"), &AudioServer::remove_bus_effect);

	ClassDB::bind_method(D_METHOD("get_bus_effect_count", "bus_idx"), &AudioServer::get_bus_effect_count);
	ClassDB::bind_method(D_METHOD("get_bus_effect", "bus_idx", "effect_idx"), &AudioServer::get_bus_effect);
	ClassDB::bind_method(D_METHOD("get_bus_effect_instance", "bus_idx", "effect_idx", "channel"), &AudioServer::get_bus_effect_instance, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("swap_bus_effects", "bus_idx", "effect_idx", "by_effect_idx"), &AudioServer::swap_bus_effects);

	ClassDB::bind_method(D_METHOD("set_bus_effect_enabled", "bus_idx", "effect_idx", "enabled"), &AudioServer::set_bus_effect_enabled);
	ClassDB::bind_method(D_METHOD("is_bus_effect_enabled", "bus_idx", "effect_idx"), &AudioServer::is_bus_effect_enabled);

	ClassDB::bind_method(D_METHOD("get_bus_peak_volume_left_db", "bus_idx", "channel"), &AudioServer::get_bus_peak_volume_left_db);
	ClassDB::bind_method(D_METHOD("get_bus_peak_volume_right_db", "bus_idx", "channel"), &AudioServer::get_bus_peak_volume_right_db);

	ClassDB::bind_method(D_METHOD("set_playback_speed_scale", "scale"), &AudioServer::set_playback_speed_scale);
	ClassDB::bind_method(D_METHOD("get_playback_speed_scale"), &AudioServer::get_playback_speed_scale);

	ClassDB::bind_method(D_METHOD("lock"), &AudioServer::lock);
	ClassDB::bind_method(D_METHOD("unlock"), &AudioServer::unlock);

	ClassDB::bind_method(D_METHOD("get_speaker_mode"), &AudioServer::get_speaker_mode);
	ClassDB::bind_method(D_METHOD("get_mix_rate"), &AudioServer::get_mix_rate);
	ClassDB::bind_method(D_METHOD("get_input_mix_rate"), &AudioServer::get_input_mix_rate);

	ClassDB::bind_method(D_METHOD("get_driver_name"), &AudioServer::get_driver_name);

	ClassDB::bind_method(D_METHOD("get_output_device_list"), &AudioServer::get_output_device_list);
	ClassDB::bind_method(D_METHOD("get_output_device"), &AudioServer::get_output_device);
	ClassDB::bind_method(D_METHOD("set_output_device", "name"), &AudioServer::set_output_device);

	ClassDB::bind_method(D_METHOD("get_time_to_next_mix"), &AudioServer::get_time_to_next_mix);
	ClassDB::bind_method(D_METHOD("get_time_since_last_mix"), &AudioServer::get_time_since_last_mix);
	ClassDB::bind_method(D_METHOD("get_output_latency"), &AudioServer::get_output_latency);

	ClassDB::bind_method(D_METHOD("get_input_device_list"), &AudioServer::get_input_device_list);
	ClassDB::bind_method(D_METHOD("get_input_device"), &AudioServer::get_input_device);
	ClassDB::bind_method(D_METHOD("set_input_device", "name"), &AudioServer::set_input_device);

	ClassDB::bind_method(D_METHOD("set_bus_layout", "bus_layout"), &AudioServer::set_bus_layout);
	ClassDB::bind_method(D_METHOD("generate_bus_layout"), &AudioServer::generate_bus_layout);

	ClassDB::bind_method(D_METHOD("set_enable_tagging_used_audio_streams", "enable"), &AudioServer::set_enable_tagging_used_audio_streams);

	ClassDB::bind_method(D_METHOD("is_stream_registered_as_sample", "stream"), &AudioServer::is_stream_registered_as_sample);
	ClassDB::bind_method(D_METHOD("register_stream_as_sample", "stream"), &AudioServer::register_stream_as_sample);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "bus_count"), "set_bus_count", "get_bus_count");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "output_device"), "set_output_device", "get_output_device");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "input_device"), "set_input_device", "get_input_device");
	// The default value may be set to an empty string by the platform-specific audio driver.
	// Override for class reference generation purposes.
	ADD_PROPERTY_DEFAULT("input_device", "Default");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback_speed_scale"), "set_playback_speed_scale", "get_playback_speed_scale");

	ADD_SIGNAL(MethodInfo("bus_layout_changed"));
	ADD_SIGNAL(MethodInfo("bus_renamed", PropertyInfo(Variant::INT, "bus_index"), PropertyInfo(Variant::STRING_NAME, "old_name"), PropertyInfo(Variant::STRING_NAME, "new_name")));

	BIND_ENUM_CONSTANT(SPEAKER_MODE_STEREO);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_31);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_51);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_71);

	BIND_ENUM_CONSTANT(PLAYBACK_TYPE_DEFAULT);
	BIND_ENUM_CONSTANT(PLAYBACK_TYPE_STREAM);
	BIND_ENUM_CONSTANT(PLAYBACK_TYPE_SAMPLE);
	BIND_ENUM_CONSTANT(PLAYBACK_TYPE_MAX);
}

AudioServer::AudioServer() {
	singleton = this;
}

AudioServer::~AudioServer() {
	singleton = nullptr;
}

/////////////////////////////////

bool AudioBusLayout::_set(const StringName &p_name, const Variant &p_value) {
	String s = p_name;
	if (s.begins_with("bus/")) {
		int index = s.get_slice("/", 1).to_int();
		if (buses.size() <= index) {
			buses.resize(index + 1);
		}

		Bus &bus = buses.write[index];

		String what = s.get_slice("/", 2);

		if (what == "name") {
			bus.name = p_value;
		} else if (what == "solo") {
			bus.solo = p_value;
		} else if (what == "mute") {
			bus.mute = p_value;
		} else if (what == "bypass_fx") {
			bus.bypass = p_value;
		} else if (what == "volume_db") {
			bus.volume_db = p_value;
		} else if (what == "send") {
			bus.send = p_value;
		} else if (what == "effect") {
			int which = s.get_slice("/", 3).to_int();
			if (bus.effects.size() <= which) {
				bus.effects.resize(which + 1);
			}

			Bus::Effect &fx = bus.effects.write[which];

			String fxwhat = s.get_slice("/", 4);
			if (fxwhat == "effect") {
				fx.effect = p_value;
			} else if (fxwhat == "enabled") {
				fx.enabled = p_value;
			} else {
				return false;
			}

			return true;
		} else {
			return false;
		}

		return true;
	}

	return false;
}

bool AudioBusLayout::_get(const StringName &p_name, Variant &r_ret) const {
	String s = p_name;
	if (s.begins_with("bus/")) {
		int index = s.get_slice("/", 1).to_int();
		if (index < 0 || index >= buses.size()) {
			return false;
		}

		const Bus &bus = buses[index];

		String what = s.get_slice("/", 2);

		if (what == "name") {
			r_ret = bus.name;
		} else if (what == "solo") {
			r_ret = bus.solo;
		} else if (what == "mute") {
			r_ret = bus.mute;
		} else if (what == "bypass_fx") {
			r_ret = bus.bypass;
		} else if (what == "volume_db") {
			r_ret = bus.volume_db;
		} else if (what == "send") {
			r_ret = bus.send;
		} else if (what == "effect") {
			int which = s.get_slice("/", 3).to_int();
			if (which < 0 || which >= bus.effects.size()) {
				return false;
			}

			const Bus::Effect &fx = bus.effects[which];

			String fxwhat = s.get_slice("/", 4);
			if (fxwhat == "effect") {
				r_ret = fx.effect;
			} else if (fxwhat == "enabled") {
				r_ret = fx.enabled;
			} else {
				return false;
			}

			return true;
		} else {
			return false;
		}

		return true;
	}

	return false;
}

void AudioBusLayout::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < buses.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "bus/" + itos(i) + "/name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/solo", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/mute", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/bypass_fx", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::FLOAT, "bus/" + itos(i) + "/volume_db", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::FLOAT, "bus/" + itos(i) + "/send", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));

		for (int j = 0; j < buses[i].effects.size(); j++) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "bus/" + itos(i) + "/effect/" + itos(j) + "/effect", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
			p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/effect/" + itos(j) + "/enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		}
	}
}

AudioBusLayout::AudioBusLayout() {
	buses.resize(1);
	buses.write[0].name = SceneStringName(Master);
}
