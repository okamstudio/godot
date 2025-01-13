/**************************************************************************/
/*  audio_driver_web.cpp                                                  */
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

#include "audio_driver_web.h"

#include "godot_audio.h"

#include "core/config/project_settings.h"
#include "core/object/object.h"
#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"

#include <emscripten.h>

AudioDriverWeb::AudioContext AudioDriverWeb::audio_context;

bool AudioDriverWeb::is_available() {
	return godot_audio_is_available() != 0;
}

void AudioDriverWeb::_state_change_callback(int p_state) {
	AudioDriverWeb::audio_context.state = p_state;
}

void AudioDriverWeb::_latency_update_callback(float p_latency) {
	AudioDriverWeb::audio_context.output_latency = p_latency;
}

void AudioDriverWeb::_sample_playback_finished_callback(const char *p_playback_object_id) {
	const ObjectID playback_id = ObjectID(String::to_int(p_playback_object_id));

	Object *playback_object = ObjectDB::get_instance(playback_id);
	if (playback_object == nullptr) {
		return;
	}
	Ref<AudioSamplePlayback> playback = Object::cast_to<AudioSamplePlayback>(playback_object);
	if (playback.is_null()) {
		return;
	}

	AudioServer::get_singleton()->stop_sample_playback(playback);
}

void AudioDriverWeb::_audio_driver_process(int p_from, int p_samples) {
	int32_t *stream_buffer = reinterpret_cast<int32_t *>(output_rb);
	const int max_samples = memarr_len(output_rb);

	int write_pos = p_from;
	int to_write = p_samples;
	if (to_write == 0) {
		to_write = max_samples;
	}
	// High part
	if (write_pos + to_write > max_samples) {
		const int samples_high = max_samples - write_pos;
		audio_server_process(samples_high / channel_count, &stream_buffer[write_pos]);
		for (int i = write_pos; i < max_samples; i++) {
			output_rb[i] = float(stream_buffer[i] >> 16) / 32768.f;
		}
		to_write -= samples_high;
		write_pos = 0;
	}
	// Leftover
	audio_server_process(to_write / channel_count, &stream_buffer[write_pos]);
	for (int i = write_pos; i < write_pos + to_write; i++) {
		output_rb[i] = float(stream_buffer[i] >> 16) / 32768.f;
	}
}

void AudioDriverWeb::_audio_driver_capture(int p_from, int p_samples) {
	if (get_input_buffer().size() == 0) {
		return; // Input capture stopped.
	}
	const int max_samples = memarr_len(input_rb);

	int read_pos = p_from;
	int to_read = p_samples;
	if (to_read == 0) {
		to_read = max_samples;
	}
	// High part
	if (read_pos + to_read > max_samples) {
		const int samples_high = max_samples - read_pos;
		for (int i = read_pos; i < max_samples; i++) {
			input_buffer_write(int32_t(input_rb[i] * 32768.f) * (1U << 16));
		}
		to_read -= samples_high;
		read_pos = 0;
	}
	// Leftover
	for (int i = read_pos; i < read_pos + to_read; i++) {
		input_buffer_write(int32_t(input_rb[i] * 32768.f) * (1U << 16));
	}
}

Error AudioDriverWeb::init() {
	int latency = Engine::get_singleton()->get_audio_output_latency();
	if (!audio_context.inited) {
		audio_context.mix_rate = _get_configured_mix_rate();
		audio_context.channel_count = godot_audio_init(&audio_context.mix_rate, latency, &_state_change_callback, &_latency_update_callback);
		audio_context.inited = true;
	}
	mix_rate = audio_context.mix_rate;
	channel_count = audio_context.channel_count;
	buffer_length = closest_power_of_2((latency * mix_rate / 1000));
	Error err = create(buffer_length, channel_count);
	if (err != OK) {
		return err;
	}
	if (output_rb) {
		memdelete_arr(output_rb);
	}
	const size_t array_size = buffer_length * (size_t)channel_count;
	output_rb = memnew_arr(float, array_size);
	if (!output_rb) {
		return ERR_OUT_OF_MEMORY;
	}
	if (input_rb) {
		memdelete_arr(input_rb);
	}
	input_rb = memnew_arr(float, array_size);
	if (!input_rb) {
		return ERR_OUT_OF_MEMORY;
	}

	godot_audio_sample_set_finished_callback(&_sample_playback_finished_callback);

	return OK;
}

void AudioDriverWeb::start() {
	start(output_rb, memarr_len(output_rb), input_rb, memarr_len(input_rb));
}

void AudioDriverWeb::resume() {
	if (audio_context.state == 0) { // 'suspended'
		godot_audio_resume();
	}
}

float AudioDriverWeb::get_latency() {
	return audio_context.output_latency + (float(buffer_length) / mix_rate);
}

int AudioDriverWeb::get_mix_rate() const {
	return mix_rate;
}

AudioDriver::SpeakerMode AudioDriverWeb::get_speaker_mode() const {
	return get_speaker_mode_by_total_channels(channel_count);
}

void AudioDriverWeb::finish() {
	finish_driver();
	if (output_rb) {
		memdelete_arr(output_rb);
		output_rb = nullptr;
	}
	if (input_rb) {
		memdelete_arr(input_rb);
		input_rb = nullptr;
	}
}

Error AudioDriverWeb::input_start() {
	lock();
	input_buffer_init(buffer_length);
	unlock();
	if (godot_audio_input_start()) {
		return FAILED;
	}
	return OK;
}

Error AudioDriverWeb::input_stop() {
	godot_audio_input_stop();
	lock();
	input_buffer.clear();
	unlock();
	return OK;
}

bool AudioDriverWeb::is_stream_registered_as_sample(const Ref<AudioStream> &p_stream) const {
	ERR_FAIL_COND_V_MSG(p_stream.is_null(), false, "Parameter p_stream is null.");
	return godot_audio_sample_stream_is_registered(itos(p_stream->get_instance_id()).utf8().get_data()) != 0;
}

void AudioDriverWeb::register_sample(const Ref<AudioSample> &p_sample) {
	ERR_FAIL_COND_MSG(p_sample.is_null(), "Parameter p_sample is null.");
	ERR_FAIL_COND_MSG(p_sample->stream.is_null(), "Parameter p_sample->stream is null.");

	String loop_mode;
	switch (p_sample->loop_mode) {
		case AudioSample::LoopMode::LOOP_DISABLED: {
			loop_mode = "disabled";
		} break;

		case AudioSample::LoopMode::LOOP_FORWARD: {
			loop_mode = "forward";
		} break;

		case AudioSample::LoopMode::LOOP_PINGPONG: {
			loop_mode = "pingpong";
		} break;

		case AudioSample::LoopMode::LOOP_BACKWARD: {
			loop_mode = "backward";
		} break;
	}

	double length = p_sample->stream->get_length();

	Vector<AudioFrame> frames;
	int frames_total = mix_rate * length;
	{
		Ref<AudioStreamPlayback> stream_playback = p_sample->stream->instantiate_playback();
		frames.resize(frames_total);
		AudioFrame *frames_ptr = frames.ptrw();
		stream_playback->start();
		stream_playback->mix(frames_ptr, 1.0f, frames_total);
	}

	PackedFloat32Array data;
	data.resize(frames_total * 2);
	float *data_ptrw = data.ptrw();
	for (int i = 0; i < frames_total; i++) {
		data_ptrw[i] = frames[i].left;
		data_ptrw[i + frames_total] = frames[i].right;
	}

	godot_audio_sample_register_stream(
			itos(p_sample->stream->get_instance_id()).utf8().get_data(),
			data_ptrw,
			frames_total,
			loop_mode.utf8().get_data(),
			p_sample->loop_begin,
			p_sample->loop_end);
}

void AudioDriverWeb::unregister_sample(const Ref<AudioSample> &p_sample) {
	ERR_FAIL_COND_MSG(p_sample.is_null(), "Parameter p_sample is null.");
	ERR_FAIL_COND_MSG(p_sample->stream.is_null(), "Parameter p_sample->stream is null.");

	godot_audio_sample_unregister_stream(itos(p_sample->stream->get_instance_id()).utf8().get_data());
}

void AudioDriverWeb::start_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	ERR_FAIL_COND_MSG(p_playback->stream.is_null(), "Parameter p_playback->stream is null.");

	constexpr int real_max_channels = AudioServer::MAX_CHANNELS_PER_BUS * 2;
	PackedFloat32Array volume;
	volume.resize(real_max_channels);
	float *volume_ptrw = volume.ptrw();
	for (int i = 0; i < real_max_channels; i += 2) {
		if (p_playback->volume_vector.is_empty()) {
			volume_ptrw[i] = 0;
			volume_ptrw[i + 1] = 0;
		} else {
			const AudioFrame &frame = p_playback->volume_vector[i / 2];
			volume_ptrw[i] = frame.left;
			volume_ptrw[i + 1] = frame.right;
		}
	}
	godot_audio_sample_start(
			itos(p_playback->get_instance_id()).utf8().get_data(),
			itos(p_playback->stream->get_instance_id()).utf8().get_data(),
			AudioServer::get_singleton()->get_bus_index(p_playback->bus),
			p_playback->offset,
			p_playback->pitch_scale,
			volume_ptrw);
}

void AudioDriverWeb::stop_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	godot_audio_sample_stop(itos(p_playback->get_instance_id()).utf8().get_data());
}

void AudioDriverWeb::set_sample_playback_pause(const Ref<AudioSamplePlayback> &p_playback, bool p_paused) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	godot_audio_sample_set_pause(itos(p_playback->get_instance_id()).utf8().get_data(), p_paused);
}

bool AudioDriverWeb::is_sample_playback_active(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_V_MSG(p_playback.is_null(), false, "Parameter p_playback is null.");
	return godot_audio_sample_is_active(itos(p_playback->get_instance_id()).utf8().get_data()) != 0;
}

double AudioDriverWeb::get_sample_playback_position(const Ref<AudioSamplePlayback> &p_playback) {
	ERR_FAIL_COND_V_MSG(p_playback.is_null(), false, "Parameter p_playback is null.");
	return godot_audio_get_sample_playback_position(itos(p_playback->get_instance_id()).utf8().get_data());
}

void AudioDriverWeb::update_sample_playback_pitch_scale(const Ref<AudioSamplePlayback> &p_playback, float p_pitch_scale) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");
	godot_audio_sample_update_pitch_scale(
			itos(p_playback->get_instance_id()).utf8().get_data(),
			p_pitch_scale);
}

void AudioDriverWeb::set_sample_playback_bus_volumes_linear(const Ref<AudioSamplePlayback> &p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes) {
	ERR_FAIL_COND_MSG(p_playback.is_null(), "Parameter p_playback is null.");

	constexpr int real_max_channels = AudioServer::MAX_CHANNELS_PER_BUS * 2;

	PackedInt32Array buses;
	buses.resize(p_bus_volumes.size());
	int32_t *buses_ptrw = buses.ptrw();
	PackedFloat32Array values;
	values.resize(p_bus_volumes.size() * AudioServer::MAX_CHANNELS_PER_BUS * 2);
	float *values_ptrw = values.ptrw();
	int idx = 0;
	for (KeyValue<StringName, Vector<AudioFrame>> pair : p_bus_volumes) {
		int bus_index = AudioServer::get_singleton()->get_bus_index(pair.key);
		buses_ptrw[idx] = bus_index;
		ERR_FAIL_COND(pair.value.size() != AudioServer::MAX_CHANNELS_PER_BUS);
		for (int i = 0; i < real_max_channels; i += 2) {
			const AudioFrame &frame = pair.value[i / 2];
			values_ptrw[(idx * real_max_channels) + i] = frame.left;
			values_ptrw[(idx * real_max_channels) + i + 1] = frame.right;
		}
		idx++;
	}
	godot_audio_sample_set_volumes_linear(
			itos(p_playback->get_instance_id()).utf8().get_data(),
			buses_ptrw,
			buses.size(),
			values_ptrw,
			values.size());
}

void AudioDriverWeb::set_sample_bus_count(int p_count) {
	godot_audio_sample_bus_set_count(p_count);
}

void AudioDriverWeb::remove_sample_bus(int p_index) {
	godot_audio_sample_bus_remove(p_index);
}

void AudioDriverWeb::add_sample_bus(int p_at_pos) {
	godot_audio_sample_bus_add(p_at_pos);
}

void AudioDriverWeb::move_sample_bus(int p_bus, int p_to_pos) {
	godot_audio_sample_bus_move(p_bus, p_to_pos);
}

void AudioDriverWeb::set_sample_bus_send(int p_bus, const StringName &p_send) {
	godot_audio_sample_bus_set_send(p_bus, AudioServer::get_singleton()->get_bus_index(p_send));
}

void AudioDriverWeb::set_sample_bus_volume_db(int p_bus, float p_volume_db) {
	godot_audio_sample_bus_set_volume_db(p_bus, p_volume_db);
}

void AudioDriverWeb::set_sample_bus_solo(int p_bus, bool p_enable) {
	godot_audio_sample_bus_set_solo(p_bus, p_enable);
}

void AudioDriverWeb::set_sample_bus_mute(int p_bus, bool p_enable) {
	godot_audio_sample_bus_set_mute(p_bus, p_enable);
}

#ifdef THREADS_ENABLED

/// AudioWorkletNode implementation (threads)
void AudioDriverWorklet::_audio_thread_func(void *p_data) {
	AudioDriverWorklet *driver = static_cast<AudioDriverWorklet *>(p_data);
	const int out_samples = memarr_len(driver->get_output_rb());
	const int in_samples = memarr_len(driver->get_input_rb());
	int wpos = 0;
	int to_write = out_samples;
	int rpos = 0;
	int to_read = 0;
	int32_t step = 0;
	while (!driver->quit) {
		if (to_read) {
			driver->lock();
			driver->_audio_driver_capture(rpos, to_read);
			godot_audio_worklet_state_add(driver->state, STATE_SAMPLES_IN, -to_read);
			driver->unlock();
			rpos += to_read;
			if (rpos >= in_samples) {
				rpos -= in_samples;
			}
		}
		if (to_write) {
			driver->lock();
			driver->_audio_driver_process(wpos, to_write);
			godot_audio_worklet_state_add(driver->state, STATE_SAMPLES_OUT, to_write);
			driver->unlock();
			wpos += to_write;
			if (wpos >= out_samples) {
				wpos -= out_samples;
			}
		}
		step = godot_audio_worklet_state_wait(driver->state, STATE_PROCESS, step, 1);
		to_write = out_samples - godot_audio_worklet_state_get(driver->state, STATE_SAMPLES_OUT);
		to_read = godot_audio_worklet_state_get(driver->state, STATE_SAMPLES_IN);
	}
}

Error AudioDriverWorklet::create(int &p_buffer_size, int p_channels) {
	if (!godot_audio_has_worklet()) {
		return ERR_UNAVAILABLE;
	}
	return (Error)godot_audio_worklet_create(p_channels);
}

void AudioDriverWorklet::start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) {
	godot_audio_worklet_start(p_in_buf, p_in_buf_size, p_out_buf, p_out_buf_size, state);
	thread.start(_audio_thread_func, this);
}

void AudioDriverWorklet::lock() {
	mutex.lock();
}

void AudioDriverWorklet::unlock() {
	mutex.unlock();
}

void AudioDriverWorklet::finish_driver() {
	quit = true; // Ask thread to quit.
	thread.wait_to_finish();
}

#else // No threads.

/// AudioWorkletNode implementation (no threads)
AudioDriverWorklet *AudioDriverWorklet::singleton = nullptr;

Error AudioDriverWorklet::create(int &p_buffer_size, int p_channels) {
	if (!godot_audio_has_worklet()) {
		return ERR_UNAVAILABLE;
	}
	return (Error)godot_audio_worklet_create(p_channels);
}

void AudioDriverWorklet::start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) {
	_audio_driver_process();
	godot_audio_worklet_start_no_threads(p_out_buf, p_out_buf_size, &_process_callback, p_in_buf, p_in_buf_size, &_capture_callback);
}

void AudioDriverWorklet::_process_callback(int p_pos, int p_samples) {
	AudioDriverWorklet *driver = AudioDriverWorklet::get_singleton();
	driver->_audio_driver_process(p_pos, p_samples);
}

void AudioDriverWorklet::_capture_callback(int p_pos, int p_samples) {
	AudioDriverWorklet *driver = AudioDriverWorklet::get_singleton();
	driver->_audio_driver_capture(p_pos, p_samples);
}

#endif // THREADS_ENABLED

/// ScriptProcessorNode implementation
AudioDriverScriptProcessor *AudioDriverScriptProcessor::singleton = nullptr;

void AudioDriverScriptProcessor::_process_callback() {
	AudioDriverScriptProcessor::get_singleton()->_audio_driver_capture();
	AudioDriverScriptProcessor::get_singleton()->_audio_driver_process();
}

Error AudioDriverScriptProcessor::create(int &p_buffer_samples, int p_channels) {
	if (!godot_audio_has_script_processor()) {
		return ERR_UNAVAILABLE;
	}
	return (Error)godot_audio_script_create(&p_buffer_samples, p_channels);
}

void AudioDriverScriptProcessor::start(float *p_out_buf, int p_out_buf_size, float *p_in_buf, int p_in_buf_size) {
	godot_audio_script_start(p_in_buf, p_in_buf_size, p_out_buf, p_out_buf_size, &_process_callback);
}
