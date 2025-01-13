/**************************************************************************/
/*  audio_effect_record.cpp                                               */
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

#include "audio_effect_record.h"

void AudioEffectRecordInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	if (!is_recording) {
		for (int i = 0; i < p_frame_count; i++) {
			p_dst_frames[i] = p_src_frames[i];
		}
		return;
	}

	//Add incoming audio frames to the IO ring buffer
	const AudioFrame *src = p_src_frames;
	AudioFrame *rb_buf = ring_buffer.ptrw();
	for (int i = 0; i < p_frame_count; i++) {
		p_dst_frames[i] = p_src_frames[i];
		rb_buf[ring_buffer_pos & ring_buffer_mask] = src[i];
		ring_buffer_pos++;
	}
}

void AudioEffectRecordInstance::_update_buffer() {
	//Case: Frames are remaining in the buffer
	while (ring_buffer_read_pos < ring_buffer_pos) {
		//Read from the buffer into recording_data
		_io_store_buffer();
	}
}

void AudioEffectRecordInstance::_update(void *userdata) {
	AudioEffectRecordInstance *ins = static_cast<AudioEffectRecordInstance *>(userdata);
	ins->_update_buffer();
}

bool AudioEffectRecordInstance::process_silence() const {
	return true;
}

void AudioEffectRecordInstance::_io_thread_process() {
	while (is_recording) {
		_update_buffer();
		if (is_recording) {
			//Wait to avoid too much busy-wait
			OS::get_singleton()->delay_usec(500);
		}
	}
}

void AudioEffectRecordInstance::_io_store_buffer() {
	int to_read = ring_buffer_pos - ring_buffer_read_pos;

	AudioFrame *rb_buf = ring_buffer.ptrw();

	while (to_read) {
		AudioFrame buffered_frame = rb_buf[ring_buffer_read_pos & ring_buffer_mask];
		recording_data.push_back(buffered_frame.left);
		recording_data.push_back(buffered_frame.right);

		ring_buffer_read_pos++;
		to_read--;
	}
}

void AudioEffectRecordInstance::_thread_callback(void *_instance) {
	AudioEffectRecordInstance *aeri = reinterpret_cast<AudioEffectRecordInstance *>(_instance);

	aeri->_io_thread_process();
}

void AudioEffectRecordInstance::init() {
	//Reset recorder status
	ring_buffer_pos = 0;
	ring_buffer_read_pos = 0;

	//We start a new recording
	recording_data.clear(); //Clear data completely and reset length
	is_recording = true;

	io_thread.start(_thread_callback, this);
}

void AudioEffectRecordInstance::finish() {
	is_recording = false;
	if (io_thread.is_started()) {
		io_thread.wait_to_finish();
	}
}

Ref<AudioEffectInstance> AudioEffectRecord::instantiate() {
	Ref<AudioEffectRecordInstance> ins;
	ins.instantiate();
	ins->is_recording = false;

	// Reusing the buffer size calculations from audio_effect_delay.cpp.
	float ring_buffer_max_size = IO_BUFFER_SIZE_MS;
	ring_buffer_max_size /= 1000.0; //convert to seconds
	ring_buffer_max_size *= AudioServer::get_singleton()->get_mix_rate();

	int ringbuff_size = ring_buffer_max_size;

	int bits = 0;

	while (ringbuff_size > 0) {
		bits++;
		ringbuff_size /= 2;
	}

	ringbuff_size = 1 << bits;
	ins->ring_buffer_mask = ringbuff_size - 1;
	ins->ring_buffer_pos = 0;

	ins->ring_buffer.resize(ringbuff_size);

	ins->ring_buffer_read_pos = 0;

	ensure_thread_stopped();
	bool is_currently_recording = false;
	if (current_instance.is_valid()) {
		is_currently_recording = current_instance->is_recording;
	}
	if (is_currently_recording) {
		ins->init();
	}
	current_instance = ins;

	return ins;
}

void AudioEffectRecord::ensure_thread_stopped() {
	if (current_instance.is_valid()) {
		current_instance->finish();
	}
}

void AudioEffectRecord::set_recording_active(bool p_record) {
	if (p_record) {
		if (current_instance.is_null()) {
			WARN_PRINT("Recording should not be set as active before Godot has initialized.");
			return;
		}
		ensure_thread_stopped();
		current_instance->init();
	} else {
		if (current_instance.is_valid()) {
			current_instance->is_recording = false;
		}
	}
}

bool AudioEffectRecord::is_recording_active() const {
	if (current_instance.is_null()) {
		return false;
	} else {
		return current_instance->is_recording;
	}
}

void AudioEffectRecord::set_format(AudioStreamWAV::Format p_format) {
	format = p_format;
}

AudioStreamWAV::Format AudioEffectRecord::get_format() const {
	return format;
}

Ref<AudioStreamWAV> AudioEffectRecord::get_recording() const {
	AudioStreamWAV::Format dst_format = format;
	bool stereo = true; //forcing mono is not implemented

	Vector<uint8_t> dst_data;

	ERR_FAIL_COND_V(current_instance.is_null(), nullptr);
	ERR_FAIL_COND_V(current_instance->recording_data.is_empty(), nullptr);

	if (dst_format == AudioStreamWAV::FORMAT_8_BITS) {
		int data_size = current_instance->recording_data.size();
		dst_data.resize(data_size);
		uint8_t *w = dst_data.ptrw();

		for (int i = 0; i < data_size; i++) {
			int8_t v = CLAMP(current_instance->recording_data[i] * 128, -128, 127);
			w[i] = v;
		}
	} else if (dst_format == AudioStreamWAV::FORMAT_16_BITS) {
		int data_size = current_instance->recording_data.size();
		dst_data.resize(data_size * 2);
		uint8_t *w = dst_data.ptrw();

		for (int i = 0; i < data_size; i++) {
			int16_t v = CLAMP(current_instance->recording_data[i] * 32768, -32768, 32767);
			encode_uint16(v, &w[i * 2]);
		}
	} else if (dst_format == AudioStreamWAV::FORMAT_IMA_ADPCM) {
		//byte interleave
		Vector<float> left;
		Vector<float> right;

		int tframes = current_instance->recording_data.size() / 2;
		left.resize(tframes);
		right.resize(tframes);

		for (int i = 0; i < tframes; i++) {
			left.set(i, current_instance->recording_data[i * 2 + 0]);
			right.set(i, current_instance->recording_data[i * 2 + 1]);
		}

		Vector<uint8_t> bleft;
		Vector<uint8_t> bright;

		AudioStreamWAV::_compress_ima_adpcm(left, bleft);
		AudioStreamWAV::_compress_ima_adpcm(right, bright);

		int dl = bleft.size();
		dst_data.resize(dl * 2);

		uint8_t *w = dst_data.ptrw();
		const uint8_t *rl = bleft.ptr();
		const uint8_t *rr = bright.ptr();

		for (int i = 0; i < dl; i++) {
			w[i * 2 + 0] = rl[i];
			w[i * 2 + 1] = rr[i];
		}
	} else if (dst_format == AudioStreamWAV::FORMAT_QOA) {
		qoa_desc desc = {};
		desc.samples = current_instance->recording_data.size() / 2;
		desc.samplerate = AudioServer::get_singleton()->get_mix_rate();
		desc.channels = 2;
		AudioStreamWAV::_compress_qoa(current_instance->recording_data, dst_data, &desc);
	} else {
		ERR_PRINT("Format not implemented.");
	}

	Ref<AudioStreamWAV> sample;
	sample.instantiate();
	sample->set_data(dst_data);
	sample->set_format(dst_format);
	sample->set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
	sample->set_loop_mode(AudioStreamWAV::LOOP_DISABLED);
	sample->set_loop_begin(0);
	sample->set_loop_end(0);
	sample->set_stereo(stereo);

	return sample;
}

void AudioEffectRecord::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_recording_active", "record"), &AudioEffectRecord::set_recording_active);
	ClassDB::bind_method(D_METHOD("is_recording_active"), &AudioEffectRecord::is_recording_active);
	ClassDB::bind_method(D_METHOD("set_format", "format"), &AudioEffectRecord::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &AudioEffectRecord::get_format);
	ClassDB::bind_method(D_METHOD("get_recording"), &AudioEffectRecord::get_recording);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_ENUM, "8-Bit,16-Bit,IMA ADPCM,Quite OK Audio"), "set_format", "get_format");
}

AudioEffectRecord::AudioEffectRecord() {
	format = AudioStreamWAV::FORMAT_16_BITS;
}

AudioEffectRecord::~AudioEffectRecord() {
	ensure_thread_stopped();
}
