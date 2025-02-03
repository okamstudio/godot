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

#include "core/io/resource_loader.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "scene/resources/audio_stream_sample.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio/effects/audio_effect_compressor.h"

#ifdef TOOLS_ENABLED
#include "main/main.h"
#define MARK_EDITED set_edited(true);
#else
#define MARK_EDITED
#endif

#ifdef DEV_ENABLED
// Compile with this define to get logging output (via print_verbose).
// #define GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
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

Array AudioDriver::get_device_list() {
	Array list;

	list.push_back("Default");

	return list;
}

String AudioDriver::get_device() {
	return "Default";
}

Array AudioDriver::capture_get_device_list() {
	Array list;

	list.push_back("Default");

	return list;
}

AudioDriver::AudioDriver() {
	_last_mix_time = 0;
	_last_mix_frames = 0;
	input_position = 0;
	input_size = 0;

#ifdef DEBUG_ENABLED
	prof_time = 0;
#endif
}

AudioDriverDummy AudioDriverManager::dummy_driver;
AudioDriver *AudioDriverManager::drivers[MAX_DRIVERS] = {
	&AudioDriverManager::dummy_driver,
};
int AudioDriverManager::driver_count = 1;
int AudioDriverManager::desired_driver_id = -2;
int AudioDriverManager::actual_driver_id = -2;

// Defaults are for the editor, outside the editor will be overridden.
uint32_t AudioDriverManager::_mute_state = AudioDriverManager::MuteFlags::MUTE_FLAG_DISABLED;
uint32_t AudioDriverManager::_mute_state_final = AudioDriverManager::MuteFlags::MUTE_FLAG_DISABLED;
uint32_t AudioDriverManager::_mute_state_mask = UINT32_MAX;

void AudioDriverManager::add_driver(AudioDriver *p_driver) {
	ERR_FAIL_COND(driver_count >= MAX_DRIVERS);
	drivers[driver_count - 1] = p_driver;

	// Last driver is always our dummy driver
	drivers[driver_count++] = &AudioDriverManager::dummy_driver;
}

int AudioDriverManager::get_driver_count() {
	return driver_count;
}

void AudioDriverManager::_log(String p_sz, int p_driver_id) {
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
	if ((p_driver_id >= 0) && (p_driver_id < driver_count)) {
		p_sz += get_driver(p_driver_id)->get_name();
	}
	print_verbose(p_sz);
#endif
}

void AudioDriverManager::_set_driver(int p_driver) {
	desired_driver_id = p_driver;

	if (!is_active()) {
		// last driver is always the dummy
		p_driver = driver_count - 1;
	}

	// noop
	if (actual_driver_id == p_driver) {
		return;
	}

	bool needs_start = false;

	// give the previous driver a chance to finish
	if ((actual_driver_id != -2) && (actual_driver_id < driver_count)) {
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
		_log("finishing audio driver ", actual_driver_id);
#endif
		AudioDriverManager::get_driver(actual_driver_id)->finish();
		actual_driver_id = -2;
		needs_start = true;
	}

	int failed_driver = -1;

	// Check if there is a selected driver
	if (p_driver >= 0 && p_driver < driver_count) {
		if (drivers[p_driver]->init() == OK) {
			drivers[p_driver]->set_singleton();
			actual_driver_id = p_driver;
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
			_log("init success audio driver ", actual_driver_id);
#endif
			if (needs_start) {
				drivers[p_driver]->start();
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
				_log("\tstarting driver ", p_driver);
#endif
			}
			return;
		} else {
			failed_driver = p_driver;
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
			_log("init failure audio driver ", p_driver);
#endif
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
			actual_driver_id = i;
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
			_log("init success (any) audio driver ", actual_driver_id);
#endif
			if (needs_start) {
				drivers[p_driver]->start();
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
				_log("\tstarting driver ", p_driver);
#endif
			}
			break;
		}
	}

	if (is_active() && (driver_count > 1) && (String(AudioDriver::get_singleton()->get_name()) == "Dummy")) {
		WARN_PRINT("All audio drivers failed, falling back to the dummy driver.");
	}
}

void AudioDriverManager::initialize(int p_driver) {
	bool mute_driver = GLOBAL_DEF("audio/muting/mute_driver", false);

	GLOBAL_DEF_RST("audio/enable_audio_input", false);
	GLOBAL_DEF_RST("audio/mix_rate", DEFAULT_MIX_RATE);
	GLOBAL_DEF_RST("audio/mix_rate.web", 0); // Safer default output_latency for web (use browser default).
	GLOBAL_DEF_RST("audio/output_latency", DEFAULT_OUTPUT_LATENCY);
	GLOBAL_DEF_RST("audio/output_latency.web", 50); // Safer default output_latency for web.

	// These Project settings are only actually set and used outside the editor.
	// The Editor uses EditorSettings defined in editor_node.cpp.
	bool mute_on_pause = GLOBAL_DEF("audio/muting/mute_on_pause", true);
	bool mute_on_silence = GLOBAL_DEF("audio/muting/mute_on_silence", false);
	bool mute_on_focus_loss = GLOBAL_DEF("audio/muting/mute_on_focus_loss", false);

	// Defaults for outside the editor
#ifdef TOOLS_ENABLED
	if (!(Engine::get_singleton()->is_editor_hint() || Main::is_project_manager())) {
#else
	{
#endif
		// Note that these can be set differently on different platforms if desired.
		// e.g. Android may want to ensure mute when app paused etc.
		_mute_state = mute_driver ? MuteFlags::MUTE_FLAG_DISABLED : 0;

		// Sensitive to all but focus and quiet mode
		_mute_state_mask = MuteFlags::MUTE_FLAG_DISABLED;
		if (mute_on_pause) {
			_mute_state_mask |= MuteFlags::MUTE_FLAG_PAUSED;
		}
		if (mute_on_silence) {
			_mute_state_mask |= MuteFlags::MUTE_FLAG_SILENCE;
		}
		if (mute_on_focus_loss) {
			_mute_state_mask |= MuteFlags::MUTE_FLAG_FOCUS_LOSS;
		}

		_update_mute_state();
	}

	_set_driver(p_driver);
}

AudioDriver *AudioDriverManager::get_driver(int p_driver) {
	ERR_FAIL_INDEX_V(p_driver, driver_count, nullptr);
	return drivers[p_driver];
}

void AudioDriverManager::_update_mute_state() {
	_mute_state_final = _mute_state & _mute_state_mask;
}

void AudioDriverManager::set_mute_sensitivity(MuteFlags p_flag, bool p_enabled) {
	if (p_enabled) {
		_mute_state_mask |= p_flag;
	} else {
		_mute_state_mask &= ~p_flag;
	}
	_update_mute_state();
	_set_driver(desired_driver_id);
}

void AudioDriverManager::set_mute_flag(MuteFlags p_flag, bool p_enabled) {
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
	_log("set_mute_flag " + itos(p_flag) + " " + String(Variant(p_enabled)) + ", flags was " + itos(_mute_state) + " (final flags " + itos(_mute_state_final) + ")");
#endif
	bool enabled = _mute_state & p_flag;

	if (enabled == p_enabled) {
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
		_log("\tno change");
#endif
		return;
	}
	if (p_enabled) {
		_mute_state |= p_flag;
	} else {
		_mute_state &= ~p_flag;
	}
	_update_mute_state();
#ifdef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
	_log("\tflags now " + itos(_mute_state) + " (final flags " + itos(_mute_state_final) + ")");
#endif
	_set_driver(desired_driver_id);
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

void AudioServer::_driver_process(int p_frames, int32_t *p_buffer) {
	int todo = p_frames;

#ifdef DEBUG_ENABLED
	uint64_t prof_ticks = OS::get_singleton()->get_ticks_usec();
#endif

	if (channel_count != get_channel_count()) {
		// Amount of channels changed due to a device change
		// reinitialize the buses channels and buffers
		init_channels_and_buffers();
	}

	ERR_FAIL_COND_MSG(buses.empty() && todo, "AudioServer bus count is less than 1.");

	bool processing_allowed = AudioDriverManager::is_audio_processing_allowed();

	while (todo) {
		if (to_mix == 0) {
			_mix_step();
		}

		int to_copy = MIN(to_mix, todo);

		if (processing_allowed) {
			Bus *master = buses[0];

			int from = buffer_size - to_mix;
			int from_buf = p_frames - todo;

			//master master, send to output
			int cs = master->channels.size();

			// take away 1 from the stride as we are manually incrementing one for stereo
			uintptr_t stride_minus_one = (cs * 2) - 1;

			for (int k = 0; k < cs; k++) {
				// destination start for data will be the same in all cases
				int32_t *dest = &p_buffer[(from_buf * (cs * 2)) + (k * 2)];

				if (master->channels[k].active) {
					const AudioFrame *buf = master->channels[k].buffer.ptr();

					for (int j = 0; j < to_copy; j++) {
						float l = CLAMP(buf[from + j].l, -1.0, 1.0);
						int32_t vl = l * ((1 << 20) - 1);
						int32_t vl2 = (vl < 0 ? -1 : 1) * (ABS(vl) << 11);
						*dest = vl2;
						dest++;

						float r = CLAMP(buf[from + j].r, -1.0, 1.0);
						int32_t vr = r * ((1 << 20) - 1);
						int32_t vr2 = (vr < 0 ? -1 : 1) * (ABS(vr) << 11);
						*dest = vr2;
						dest += stride_minus_one;
					}

				} else {
					// Bizarrely, profiling indicates that detecting the common case of cs == 1
					// and k == 0, and using memset is SLOWER than setting individually.
					// (Perhaps it gets optimized to a faster instruction than memset).
					for (int j = 0; j < to_copy; j++) {
						*dest = 0;
						dest++;
						*dest = 0;
						dest += stride_minus_one;
					}
				}
			}

		} // if audio processing allowed

		todo -= to_copy;
		to_mix -= to_copy;
	}

#ifdef DEBUG_ENABLED
	prof_time += OS::get_singleton()->get_ticks_usec() - prof_ticks;
#endif
}

void AudioServer::_mix_step() {
	bool solo_mode = false;
	bool processing_allowed = AudioDriverManager::is_audio_processing_allowed();

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

	//make callbacks for mixing the audio
	for (Set<CallbackItem>::Element *E = callbacks.front(); E; E = E->next()) {
		E->get().callback(E->get().userdata);
	}

	for (int i = buses.size() - 1; i >= 0; i--) {
		//go bus by bus
		Bus *bus = buses[i];

		if (processing_allowed) {
			for (int k = 0; k < bus->channels.size(); k++) {
				if (bus->channels[k].active && !bus->channels[k].used) {
					//buffer was not used, but it's still active, so it must be cleaned
					AudioFrame *buf = bus->channels.write[k].buffer.ptrw();

					for (uint32_t j = 0; j < buffer_size; j++) {
						buf[j] = AudioFrame(0, 0);
					}
				}
			}

			//process effects
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

					//swap buffers, so internal buffer always has the right data
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

		} // if processing allowed

		//process send

		Bus *send = nullptr;

		if (i > 0) {
			//everything has a send save for master bus
			if (!bus_map.has(bus->send)) {
				send = buses[0];
			} else {
				send = bus_map[bus->send];
				if (send->index_cache >= bus->index_cache) { //invalid, send to master
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

			float volume = Math::db2linear(bus->volume_db);

			if (solo_mode) {
				if (!bus->soloed) {
					volume = 0.0;
				}
			} else {
				if (bus->mute) {
					volume = 0.0;
				}
			}

			if (processing_allowed) {
				//apply volume and compute peak
				for (uint32_t j = 0; j < buffer_size; j++) {
					buf[j] *= volume;

					float l = ABS(buf[j].l);
					if (l > peak.l) {
						peak.l = l;
					}
					float r = ABS(buf[j].r);
					if (r > peak.r) {
						peak.r = r;
					}
				}
			}

			bus->channels.write[k].peak_volume = AudioFrame(Math::linear2db(peak.l + AUDIO_PEAK_OFFSET), Math::linear2db(peak.r + AUDIO_PEAK_OFFSET));

			if (!bus->channels[k].used) {
				//see if any audio is contained, because channel was not used

				if (MAX(peak.r, peak.l) > Math::db2linear(channel_disable_threshold_db)) {
					bus->channels.write[k].last_mix_with_audio = mix_frames;
				} else if (mix_frames - bus->channels[k].last_mix_with_audio > channel_disable_frames) {
					bus->channels.write[k].active = false;
					continue; //went inactive, don't mix.
				}
			}

			if (send && processing_allowed) {
				//if not master bus, send
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
	last_sound_played_ms = OS::get_singleton()->get_ticks_msec();

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
			buses[i]->send = "Master";
		}

		bus_map[attempt] = buses[i];
	}

	unlock();

	emit_signal("bus_layout_changed");
}

void AudioServer::remove_bus(int p_index) {
	ERR_FAIL_INDEX(p_index, buses.size());
	ERR_FAIL_COND(p_index == 0);

	MARK_EDITED

	lock();
	bus_map.erase(buses[p_index]->name);
	memdelete(buses[p_index]);
	buses.remove(p_index);
	unlock();

	emit_signal("bus_layout_changed");
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

	emit_signal("bus_layout_changed");
}

void AudioServer::move_bus(int p_bus, int p_to_pos) {
	ERR_FAIL_COND(p_bus < 1 || p_bus >= buses.size());
	ERR_FAIL_COND(p_to_pos != -1 && (p_to_pos < 1 || p_to_pos > buses.size()));

	MARK_EDITED

	if (p_bus == p_to_pos) {
		return;
	}

	Bus *bus = buses[p_bus];
	buses.remove(p_bus);

	if (p_to_pos == -1) {
		buses.push_back(bus);
	} else if (p_to_pos < p_bus) {
		buses.insert(p_to_pos, bus);
	} else {
		buses.insert(p_to_pos - 1, bus);
	}

	emit_signal("bus_layout_changed");
}

int AudioServer::get_bus_count() const {
	return buses.size();
}

void AudioServer::set_bus_name(int p_bus, const String &p_name) {
	ERR_FAIL_INDEX(p_bus, buses.size());
	if (p_bus == 0 && p_name != "Master") {
		return; //bus 0 is always master
	}

	MARK_EDITED

	lock();

	if (buses[p_bus]->name == p_name) {
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
	bus_map.erase(buses[p_bus]->name);
	buses[p_bus]->name = attempt;
	bus_map[attempt] = buses[p_bus];
	unlock();

	emit_signal("bus_layout_changed");
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
}
float AudioServer::get_bus_volume_db(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	return buses[p_bus]->volume_db;
}

int AudioServer::get_bus_channels(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	return buses[p_bus]->channels.size();
}

void AudioServer::set_bus_send(int p_bus, const StringName &p_send) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->send = p_send;
}

StringName AudioServer::get_bus_send(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), StringName());
	return buses[p_bus]->send;
}

void AudioServer::set_bus_solo(int p_bus, bool p_enable) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->solo = p_enable;
}

bool AudioServer::is_bus_solo(int p_bus) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

	return buses[p_bus]->solo;
}

void AudioServer::set_bus_mute(int p_bus, bool p_enable) {
	ERR_FAIL_INDEX(p_bus, buses.size());

	MARK_EDITED

	buses[p_bus]->mute = p_enable;
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
			Ref<AudioEffectInstance> fx = buses.write[p_bus]->effects.write[j].effect->instance();
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
	//fx.instance=p_effect->instance();
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

	buses[p_bus]->effects.remove(p_effect);
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

	return buses[p_bus]->channels[p_channel].peak_volume.l;
}
float AudioServer::get_bus_peak_volume_right_db(int p_bus, int p_channel) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), 0);

	return buses[p_bus]->channels[p_channel].peak_volume.r;
}

bool AudioServer::is_bus_channel_active(int p_bus, int p_channel) const {
	ERR_FAIL_INDEX_V(p_bus, buses.size(), false);
	ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), false);

	return buses[p_bus]->channels[p_channel].active;
}

void AudioServer::set_global_rate_scale(float p_scale) {
	ERR_FAIL_COND(p_scale <= 0);

	global_rate_scale = p_scale;
}
float AudioServer::get_global_rate_scale() const {
	return global_rate_scale;
}

void AudioServer::init_channels_and_buffers() {
	channel_count = get_channel_count();
	temp_buffer.resize(channel_count);

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
	channel_disable_threshold_db = GLOBAL_DEF_RST("audio/channel_disable_threshold_db", -60.0);
	channel_disable_frames = float(GLOBAL_DEF_RST("audio/channel_disable_time", 2.0)) * get_mix_rate();
	ProjectSettings::get_singleton()->set_custom_property_info("audio/channel_disable_time", PropertyInfo(Variant::REAL, "audio/channel_disable_time", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"));
	buffer_size = 1024; //hardcoded for now

	init_channels_and_buffers();

	mix_count = 0;
	set_bus_count(1);
	set_bus_name(0, "Master");

	if (AudioDriver::get_singleton()) {
		AudioDriver::get_singleton()->start();
	}

#ifdef TOOLS_ENABLED
	set_edited(false); //avoid editors from thinking this was edited
#endif

	GLOBAL_DEF_RST("audio/video_delay_compensation_ms", 0);
}

void AudioServer::update() {
#ifdef DEBUG_ENABLED
	if (ScriptDebugger::get_singleton() && ScriptDebugger::get_singleton()->is_profiling()) {
		// Driver time includes server time + effects times
		// Server time includes effects times
		uint64_t driver_time = AudioDriver::get_singleton()->get_profiling_time();
		uint64_t server_time = prof_time;

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

		ScriptDebugger::get_singleton()->add_profiling_frame_data("audio_thread", values);
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
	prof_time = 0;
#endif

	// give audio driver option to turn off to throttle CPU
	if (AudioDriverManager::get_mute_sensitivity(AudioDriverManager::MUTE_FLAG_SILENCE)) {
		uint32_t time_ms = OS::get_singleton()->get_ticks_msec();
		if (time_ms >= last_sound_played_ms) {
			uint32_t diff_ms = time_ms - last_sound_played_ms;
			if (diff_ms <= 10000) {
				if (AudioDriverManager::get_mute_flag(AudioDriverManager::MUTE_FLAG_SILENCE)) {
					AudioDriverManager::set_mute_flag(AudioDriverManager::MUTE_FLAG_SILENCE, false);
				}
			} else {
				if (!AudioDriverManager::get_mute_flag(AudioDriverManager::MUTE_FLAG_SILENCE)) {
					AudioDriverManager::set_mute_flag(AudioDriverManager::MUTE_FLAG_SILENCE, true);
				}
			}
		}
	}

	for (Set<CallbackItem>::Element *E = update_callbacks.front(); E; E = E->next()) {
		E->get().callback(E->get().userdata);
	}
}

void AudioServer::load_default_bus_layout() {
	String layout_path = ProjectSettings::get_singleton()->get("audio/default_bus_layout");

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

void *AudioServer::audio_data_alloc(uint32_t p_data_len, const uint8_t *p_from_data) {
	void *ad = memalloc(p_data_len);
	ERR_FAIL_COND_V(!ad, nullptr);
	if (p_from_data) {
		memcpy(ad, p_from_data, p_data_len);
	}

	audio_data_lock.lock();
	audio_data[ad] = p_data_len;
	audio_data_total_mem += p_data_len;
	audio_data_max_mem = MAX(audio_data_total_mem, audio_data_max_mem);
	audio_data_lock.unlock();

	return ad;
}

void AudioServer::audio_data_free(void *p_data) {
	audio_data_lock.lock();
	if (!audio_data.has(p_data)) {
		audio_data_lock.unlock();
		ERR_FAIL();
	}

	audio_data_total_mem -= audio_data[p_data];
	audio_data.erase(p_data);
	memfree(p_data);
	audio_data_lock.unlock();
}

size_t AudioServer::audio_data_get_total_memory_usage() const {
	return audio_data_total_mem;
}
size_t AudioServer::audio_data_get_max_memory_usage() const {
	return audio_data_max_mem;
}

void AudioServer::add_callback(AudioCallback p_callback, void *p_userdata) {
	lock();
	CallbackItem ci;
	ci.callback = p_callback;
	ci.userdata = p_userdata;
	callbacks.insert(ci);
	unlock();
}

void AudioServer::remove_callback(AudioCallback p_callback, void *p_userdata) {
	lock();
	CallbackItem ci;
	ci.callback = p_callback;
	ci.userdata = p_userdata;
	callbacks.erase(ci);
	unlock();
}

void AudioServer::add_update_callback(AudioCallback p_callback, void *p_userdata) {
	lock();
	CallbackItem ci;
	ci.callback = p_callback;
	ci.userdata = p_userdata;
	update_callbacks.insert(ci);
	unlock();
}

void AudioServer::remove_update_callback(AudioCallback p_callback, void *p_userdata) {
	lock();
	CallbackItem ci;
	ci.callback = p_callback;
	ci.userdata = p_userdata;
	update_callbacks.erase(ci);
	unlock();
}

void AudioServer::set_bus_layout(const Ref<AudioBusLayout> &p_bus_layout) {
	ERR_FAIL_COND(p_bus_layout.is_null() || p_bus_layout->buses.size() == 0);

	lock();
	for (int i = 0; i < buses.size(); i++) {
		memdelete(buses[i]);
	}
	buses.resize(p_bus_layout->buses.size());
	bus_map.clear();
	for (int i = 0; i < p_bus_layout->buses.size(); i++) {
		Bus *bus = memnew(Bus);
		if (i == 0) {
			bus->name = "Master";
		} else {
			bus->name = p_bus_layout->buses[i].name;
			bus->send = p_bus_layout->buses[i].send;
		}

		bus->solo = p_bus_layout->buses[i].solo;
		bus->mute = p_bus_layout->buses[i].mute;
		bus->bypass = p_bus_layout->buses[i].bypass;
		bus->volume_db = p_bus_layout->buses[i].volume_db;

		for (int j = 0; j < p_bus_layout->buses[i].effects.size(); j++) {
			Ref<AudioEffect> fx = p_bus_layout->buses[i].effects[j].effect;

			if (fx.is_valid()) {
				Bus::Effect bfx;
				bfx.effect = fx;
				bfx.enabled = p_bus_layout->buses[i].effects[j].enabled;
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
}

Ref<AudioBusLayout> AudioServer::generate_bus_layout() const {
	Ref<AudioBusLayout> state;
	state.instance();

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

Array AudioServer::get_device_list() {
	return AudioDriver::get_singleton()->get_device_list();
}

String AudioServer::get_device() {
	return AudioDriver::get_singleton()->get_device();
}

void AudioServer::set_enabled(bool p_enabled) {
	AudioDriverManager::set_mute_flag(AudioDriverManager::MUTE_FLAG_DISABLED, !p_enabled);
}

bool AudioServer::is_enabled() const {
	return !AudioDriverManager::get_mute_flag(AudioDriverManager::MUTE_FLAG_DISABLED);
}

void AudioServer::set_device(String device) {
	AudioDriver::get_singleton()->set_device(device);
}

Array AudioServer::capture_get_device_list() {
	return AudioDriver::get_singleton()->capture_get_device_list();
}

String AudioServer::capture_get_device() {
	return AudioDriver::get_singleton()->capture_get_device();
}

void AudioServer::capture_set_device(const String &p_name) {
	AudioDriver::get_singleton()->capture_set_device(p_name);
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

	ClassDB::bind_method(D_METHOD("set_global_rate_scale", "scale"), &AudioServer::set_global_rate_scale);
	ClassDB::bind_method(D_METHOD("get_global_rate_scale"), &AudioServer::get_global_rate_scale);

	ClassDB::bind_method(D_METHOD("lock"), &AudioServer::lock);
	ClassDB::bind_method(D_METHOD("unlock"), &AudioServer::unlock);

	ClassDB::bind_method(D_METHOD("get_speaker_mode"), &AudioServer::get_speaker_mode);
	ClassDB::bind_method(D_METHOD("get_mix_rate"), &AudioServer::get_mix_rate);
	ClassDB::bind_method(D_METHOD("get_device_list"), &AudioServer::get_device_list);
	ClassDB::bind_method(D_METHOD("get_device"), &AudioServer::get_device);
	ClassDB::bind_method(D_METHOD("set_device", "device"), &AudioServer::set_device);

	ClassDB::bind_method(D_METHOD("get_time_to_next_mix"), &AudioServer::get_time_to_next_mix);
	ClassDB::bind_method(D_METHOD("get_time_since_last_mix"), &AudioServer::get_time_since_last_mix);
	ClassDB::bind_method(D_METHOD("get_output_latency"), &AudioServer::get_output_latency);

	ClassDB::bind_method(D_METHOD("capture_get_device_list"), &AudioServer::capture_get_device_list);
	ClassDB::bind_method(D_METHOD("capture_get_device"), &AudioServer::capture_get_device);
	ClassDB::bind_method(D_METHOD("capture_set_device", "name"), &AudioServer::capture_set_device);

	ClassDB::bind_method(D_METHOD("set_bus_layout", "bus_layout"), &AudioServer::set_bus_layout);
	ClassDB::bind_method(D_METHOD("generate_bus_layout"), &AudioServer::generate_bus_layout);

	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &AudioServer::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &AudioServer::is_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "bus_count"), "set_bus_count", "get_bus_count");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "device"), "set_device", "get_device");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "capture_device"), "capture_set_device", "capture_get_device");
	// The default value may be set to an empty string by the platform-specific audio driver.
	// Override for class reference generation purposes.
	ADD_PROPERTY_DEFAULT("capture_device", "Default");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "global_rate_scale"), "set_global_rate_scale", "get_global_rate_scale");

	ADD_SIGNAL(MethodInfo("bus_layout_changed"));

	BIND_ENUM_CONSTANT(SPEAKER_MODE_STEREO);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_31);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_51);
	BIND_ENUM_CONSTANT(SPEAKER_SURROUND_71);
}

AudioServer::AudioServer() {
	singleton = this;
	audio_data_total_mem = 0;
	audio_data_max_mem = 0;
	mix_frames = 0;
	channel_count = 0;
	to_mix = 0;
#ifdef DEBUG_ENABLED
	prof_time = 0;
#endif
	mix_time = 0;
	mix_size = 0;
	global_rate_scale = 1;
	last_sound_played_ms = 0;
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
		p_list->push_back(PropertyInfo(Variant::STRING, "bus/" + itos(i) + "/name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/solo", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/mute", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/bypass_fx", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::REAL, "bus/" + itos(i) + "/volume_db", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::REAL, "bus/" + itos(i) + "/send", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));

		for (int j = 0; j < buses[i].effects.size(); j++) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "bus/" + itos(i) + "/effect/" + itos(j) + "/effect", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
			p_list->push_back(PropertyInfo(Variant::BOOL, "bus/" + itos(i) + "/effect/" + itos(j) + "/enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
		}
	}
}

AudioBusLayout::AudioBusLayout() {
	buses.resize(1);
	buses.write[0].name = "Master";
}

#undef GODOT_AUDIO_DRIVER_MANAGER_LOGGING_ENABLED
